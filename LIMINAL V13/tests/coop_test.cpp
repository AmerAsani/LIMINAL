// V13.2/V13.3: Pruefungen des Koop-Modus mit echten Spielsitzungen ohne Fenster (MULTIPLAYER_TESTS.md).
//   Bots       Host und 1..3 Gaeste in einem Prozess, verbunden ueber die neue Verbindungsschicht (Lobby im Test,
//              WebRTC/DTLS, getrennte Loopback-Adressen: Host 127.0.0.3, Gaeste 127.0.0.2, Lobby/TURN 127.0.0.1)
//   Wege       direkt; direkt ueber den Netzstoerer (UDP-Proxy: Laufzeit, Schwankung, Verlust, Doppelte, Umordnung,
//              Bandbreite); "NAT": die Lobby verwirft alle direkten Kandidaten, nur das TURN-Relay bleibt (der Verkehr zum
//              Relay laeuft durch den Netzstoerer); direkt ohne Lobby (eingebaute Lobby des Hosts, langer Code)
//   Angriffe   ein boeswilliger Gast mit richtigem Schluessel: Muell, falsche Rolle und falscher Zustand, Uebergroesse,
//              zu schnelle Uhr (Speedhack), erfundene und ferne Gegenstaende, Fluten
//   Fuzzing    Zufalls- und veraenderte Nachrichten durch decode (beide Rollen), Hin- und Rueckweg gueltiger
//   Abgleich   Pruefsummen des geteilten Zustands (Desync), Abweichung der Vorhersage, Ort beim Host gegen Gast
//   Reise      die Gruppe wechselt durch alle zehn Ebenen
//   Langlauf   coop_test <data> --long <s>: vier Spieler ueber den Netzstoerer
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "coop/coop.hpp"
#include "netdll/lobby_server.hpp"
#include "core/jobs.hpp"
#include "core/log.hpp"
#include "core/rng.hpp"
#include "gameplay/components.hpp"
#include "gameplay/content.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/session.hpp"
#include "net/coop_proto.hpp"
#include "world/map_def.hpp"

namespace lim::coop {
void staticNetApi(NetApi& a);
}

using namespace lim;
using game::GameSession;
namespace nc = net::coop;

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}
static void note(const std::string& s) { std::printf("  %s\n", s.c_str()); }
// pruefen und das Ergebnis mit Messwert ausgeben (fuer MULTIPLAYER_TESTS.md)
static void checkN(bool ok, const std::string& what) {
    check(ok, what);
    if (ok) note(what);
}
static constexpr u32 kBuild = 0x13020001u;
static constexpr double kDt = 1.0 / 60.0;

// =============================================================================================
// Netzstoerer: UDP-Proxy zwischen Gaesten und Host
// =============================================================================================
struct Netem {
    double latencyMs = 0, jitterMs = 0, loss = 0, dup = 0, reorder = 0;
    double kbps = 0;  // 0: unbegrenzt (je Richtung)
};

class UdpProxy {
public:
    // lauscht auf 127.0.0.1 und leitet je Absender ueber einen eigenen Socket an target:targetPort weiter
    explicit UdpProxy(u16 hostPort, const char* target = "127.0.0.1") : hostPort_(hostPort) {
        inet_pton(AF_INET, target, &target_);
        listen_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = 0;
        bind(listen_, (sockaddr*)&a, sizeof(a));
        int len = sizeof(a);
        getsockname(listen_, (sockaddr*)&a, &len);
        port_ = ntohs(a.sin_port);
        u_long nb = 1;
        ioctlsocket(listen_, FIONBIO, &nb);
        th_ = std::thread([this] { run(); });
    }
    ~UdpProxy() {
        stop_ = true;
        th_.join();
        closesocket(listen_);
        for (auto& [k, u] : up_) closesocket(u.sock);
    }
    u16 port() const { return port_; }
    void set(const Netem& n) {
        std::lock_guard<std::mutex> l(m_);
        cfg_ = n;
    }
    u64 bytesDown() const { return bytesDown_; }
    u64 bytesUp() const { return bytesUp_; }
    u64 dropped() const { return dropped_; }

private:
    struct Up {
        SOCKET sock;
        sockaddr_in client;
    };
    struct Pkt {
        double at;
        bool down;
        u64 client;
        std::vector<char> data;
    };
    static double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
    void enqueue(bool down, u64 client, const char* d, int n) {
        Netem c;
        {
            std::lock_guard<std::mutex> l(m_);
            c = cfg_;
        }
        std::uniform_real_distribution<double> U(0.0, 1.0);
        if (U(rng_) < c.loss) {
            ++dropped_;
            return;
        }
        const int copies = U(rng_) < c.dup ? 2 : 1;
        for (int k = 0; k < copies; ++k) {
            double delay = c.latencyMs * 0.5 + (U(rng_) * 2.0 - 1.0) * c.jitterMs * 0.5;  // je Richtung die Haelfte
            if (U(rng_) < c.reorder) delay += 15.0 + U(rng_) * 40.0;
            double& busy = down ? busyDown_ : busyUp_;
            const double now = nowMs();
            if (c.kbps > 0) {  // Bandbreite: Pakete stehen in der Schlange, zu lange Schlange: verwerfen
                const double tx = n * 8.0 / c.kbps;  // ms
                busy = std::max(busy, now) + tx;
                if (busy - now > 400.0) {
                    ++dropped_;
                    busy -= tx;
                    return;
                }
                delay = std::max(delay, busy - now);
            }
            q_.push_back({now + std::max(0.0, delay), down, client, std::vector<char>(d, d + n)});
        }
    }
    void run() {
        std::vector<char> buf(2048);
        while (!stop_) {
            fd_set rs;
            FD_ZERO(&rs);
            FD_SET(listen_, &rs);
            for (auto& [k, u] : up_) FD_SET(u.sock, &rs);
            timeval tv{0, 1000};
            select(0, &rs, nullptr, nullptr, &tv);
            sockaddr_in from{};
            int fl = sizeof(from);
            for (;;) {  // Gaeste -> Host
                const int n = recvfrom(listen_, buf.data(), (int)buf.size(), 0, (sockaddr*)&from, &fl);
                if (n <= 0) break;
                const u64 key = ((u64)from.sin_addr.s_addr << 16) | from.sin_port;
                if (!up_.count(key)) {
                    Up u{socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP), from};
                    sockaddr_in a{};
                    a.sin_family = AF_INET;
                    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                    bind(u.sock, (sockaddr*)&a, sizeof(a));
                    sockaddr_in h{};
                    h.sin_family = AF_INET;
                    h.sin_addr = target_;
                    h.sin_port = htons(hostPort_);
                    connect(u.sock, (sockaddr*)&h, sizeof(h));
                    u_long nb = 1;
                    ioctlsocket(u.sock, FIONBIO, &nb);
                    up_[key] = u;
                }
                bytesUp_ += (u64)n;
                enqueue(false, key, buf.data(), n);
                fl = sizeof(from);
            }
            for (auto& [k, u] : up_)
                for (;;) {  // Host -> Gast
                    const int n = recv(u.sock, buf.data(), (int)buf.size(), 0);
                    if (n <= 0) break;
                    bytesDown_ += (u64)n;
                    enqueue(true, k, buf.data(), n);
                }
            const double now = nowMs();
            for (auto it = q_.begin(); it != q_.end();) {
                if (it->at > now) {
                    ++it;
                    continue;
                }
                auto u = up_.find(it->client);
                if (u != up_.end()) {
                    if (it->down) sendto(listen_, it->data.data(), (int)it->data.size(), 0, (sockaddr*)&u->second.client, sizeof(sockaddr_in));
                    else send(u->second.sock, it->data.data(), (int)it->data.size(), 0);
                }
                it = q_.erase(it);
            }
        }
    }
    u16 hostPort_, port_ = 0;
    in_addr target_{};
    SOCKET listen_;
    std::map<u64, Up> up_;
    std::deque<Pkt> q_;
    std::thread th_;
    std::atomic<bool> stop_{false};
    std::mutex m_;
    Netem cfg_;
    std::mt19937 rng_{12345};
    double busyDown_ = 0, busyUp_ = 0;
    std::atomic<u64> bytesDown_{0}, bytesUp_{0}, dropped_{0};
};

// =============================================================================================
// Spielwelt: Host und Bots
// =============================================================================================
struct Env {
    game::Content content;
    game::PrefabLibrary prefabs;
    JobSystem jobs{3};
    coop::NetApi api;
    int diffIndex(const std::string& key) const {
        for (size_t i = 0; i < content.difficulties().size(); ++i)
            if (content.difficulties()[i].key == key) return (int)i;
        return 0;
    }
    std::unique_ptr<GameSession> make(const std::string& level, long long seed, const std::string& diff, const std::string& name) {
        auto s = std::make_unique<GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, diff, name, jobs, 3);
        s->realisticBody = true;
        return s;
    }
    static long long seedFor(long long runSeed, const std::string& next) {
        return next == "level0" ? runSeed : (long long)(hash64((u64)runSeed, "level", next.c_str()) % 2147483646ull) + 1;
    }
};
static Env* gEnv = nullptr;

struct Bot {
    std::string name;
    std::unique_ptr<coop::CoopClient> c;
    std::unique_ptr<GameSession> s;
    std::mt19937 rng;
    game::PlayerControl ctl;
    double turnT = 0, lampT = 0, actT = 0;
    float turn = 0;
    bool idle = false;  // steht still (Abgleich)
    int picks = 0;
    explicit Bot(std::string n, u32 seed) : name(std::move(n)), rng(seed) {}
};

// Wege der Tests (MULTIPLAYER_TESTS.md, Abschnitt 7)
enum class Way {
    Lobby,        // ueber die Lobby, direkter Weg (Host-Kandidaten)
    LobbyNetem,   // ueber die Lobby, direkter Weg durch den Netzstoerer (Host-Kandidaten auf Proxys umgeschrieben)
    Relay,        // "NAT auf beiden Seiten": nur Relay-Kandidaten; der Verkehr zum TURN-Server durch den Netzstoerer
    Direct,       // ohne Lobby: eingebaute Lobby des Hosts, langer Code, Adresse
};
static const char* wayName(Way w) { return w == Way::Lobby ? "Lobby, direkt" : w == Way::LobbyNetem ? "Lobby, Netzstoerer" : w == Way::Relay ? "nur Relay" : "direkt ohne Lobby"; }

// V13.3 (B-030): auf der Adresse pruefen, auf der der Dienst spaeter lauscht - der TURN-Server liegt auf 127.0.0.1;
// die Pruefung auf 127.0.0.3 fand den Port frei, obwohl ein zweiter coop_test (Langlauf) ihn dort schon hielt
static u16 freeUdpPort(u16 from, const char* addr = "127.0.0.3") {
    for (u16 p = from; p < from + 400; ++p) {
        SOCKET u = socket(AF_INET, SOCK_DGRAM, 0), t = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        inet_pton(AF_INET, addr, &a.sin_addr);
        a.sin_port = htons(p);
        const bool ok = bind(u, (sockaddr*)&a, sizeof(a)) == 0 && bind(t, (sockaddr*)&a, sizeof(a)) == 0;
        closesocket(u), closesocket(t);
        if (ok) return p;
    }
    return 0;
}

// "candidate:1 1 UDP prio ip port typ host ..." zerlegen und Adresse ersetzen
static std::vector<std::string> candTokens(const std::string& c) {
    std::istringstream is(c);
    std::vector<std::string> t;
    std::string x;
    while (is >> x) t.push_back(x);
    return t;
}

struct Game {
    std::unique_ptr<coop::CoopHost> host;
    std::unique_ptr<GameSession> hs;
    std::vector<std::unique_ptr<Bot>> bots;
    std::unique_ptr<lim::netc::LobbyServer> lobby;
    std::mutex pm;                                       // Proxys (der Haken der Lobby legt sie an)
    std::map<u16, std::unique_ptr<UdpProxy>> hostProxies;  // Host-Port -> Proxy
    std::unique_ptr<UdpProxy> turnProxy;
    Netem em;
    bool useEm = false;
    Way way = Way::Lobby;
    coop::NetSettings hostNet, botNet;
    std::string directAddr;
    std::atomic<int> dropped{0};
    game::PlayerControl hostCtl;
    std::mt19937 rng{7};
    double hostTurnT = 0;
    float hostTurn = 0;
    bool hostWalks = true;
    std::string level;

    UdpProxy* proxyFor(u16 hostPort) {
        std::lock_guard<std::mutex> l(pm);
        auto& p = hostProxies[hostPort];
        if (!p) {
            p = std::make_unique<UdpProxy>(hostPort, "127.0.0.3");
            if (useEm) p->set(em);
        }
        return p.get();
    }
    void setNetem(const Netem& n) {
        std::lock_guard<std::mutex> l(pm);
        em = n, useEm = true;
        for (auto& [k, p] : hostProxies) p->set(n);
        if (turnProxy) turnProxy->set(n);
    }
    u64 bytesDown() {
        std::lock_guard<std::mutex> l(pm);
        u64 b = turnProxy ? turnProxy->bytesDown() : 0;
        for (auto& [k, p] : hostProxies) b += p->bytesDown();
        return b;
    }
    u64 bytesUp() {
        std::lock_guard<std::mutex> l(pm);
        u64 b = turnProxy ? turnProxy->bytesUp() : 0;
        for (auto& [k, p] : hostProxies) b += p->bytesUp();
        return b;
    }
    u64 droppedPackets() {
        std::lock_guard<std::mutex> l(pm);
        u64 b = turnProxy ? turnProxy->dropped() : 0;
        for (auto& [k, p] : hostProxies) b += p->dropped();
        return b;
    }
    bool hasProxy() {
        std::lock_guard<std::mutex> l(pm);
        return turnProxy || !hostProxies.empty() || way == Way::LobbyNetem;
    }

    bool start(const std::string& lvl, long long seed, Way w = Way::Lobby, const Netem* n = nullptr) {
        level = lvl;
        way = w;
        if (n) em = *n, useEm = true;
        hs = gEnv->make(lvl, seed, "medium", "Host");
        hs->runSeed = seed;
        hs->placeAtSpawn();
        hs->preload();
        std::string err;
        hostNet = {}, botNet = {};
        hostNet.bind = "127.0.0.3", botNet.bind = "127.0.0.2";
        if (w != Way::Direct) {
            lim::netc::LobbyConfig lc;
            lc.bind = "127.0.0.1";
            lc.port = 0;
            lc.turn = true;
            lc.publicHost = "127.0.0.1";
            lc.joinsPerMinute = 1000, lc.failsPerTenMinutes = 1000;  // Tests verbinden oft hintereinander
            if (w == Way::Relay) {  // TURN hinter dem Netzstoerer: die Lobby nennt den Port des Proxys
                lc.turnPort = freeUdpPort(41000, "127.0.0.1");
                turnProxy = std::make_unique<UdpProxy>(lc.turnPort, "127.0.0.1");
                if (useEm) turnProxy->set(em);
                lc.publicTurnPort = turnProxy->port();
            } else {
                lc.turnPort = 0;
            }
            if (w == Way::LobbyNetem)
                lc.candidateHook = [this](std::string& c, bool fromHost) {
                    auto t = candTokens(c);
                    if (t.size() < 8 || t[7] != "host" || !fromHost) return ++dropped, false;  // nur der Host, nur direkt
                    UdpProxy* p = proxyFor((u16)std::atoi(t[5].c_str()));
                    t[4] = "127.0.0.1", t[5] = std::to_string(p->port());
                    c.clear();
                    for (size_t i = 0; i < t.size(); ++i) c += (i ? " " : "") + t[i];
                    return true;
                };
            if (w == Way::Relay)
                lc.candidateHook = [this](std::string& c, bool) {
                    auto t = candTokens(c);
                    if (t.size() >= 8 && t[7] == "relay") return true;
                    ++dropped;
                    return false;
                };
            lobby = std::make_unique<lim::netc::LobbyServer>();
            if (!lobby->start(lc, err)) {
                std::printf("  Lobby startet nicht: %s\n", err.c_str());
                return false;
            }
            hostNet.lobby = botNet.lobby = "ws://127.0.0.1:" + std::to_string(lobby->port());
        } else {
            hostNet.useLobby = botNet.useLobby = false;
            hostNet.directPort = freeUdpPort(38211);
            directAddr = "127.0.0.3:" + std::to_string(hostNet.directPort);
        }
        host = std::make_unique<coop::CoopHost>(gEnv->api, "Host", kBuild);
        if (!host->start(hostNet, 4, err)) {
            std::printf("  Host startet nicht: %s\n", err.c_str());
            return false;
        }
        host->attach(*hs, "", gEnv->diffIndex("medium"));
        const auto t0 = std::chrono::steady_clock::now();
        while (host->code(true).empty() && std::chrono::steady_clock::now() - t0 < std::chrono::seconds(10)) {
            host->beforeUpdate(kDt);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (host->code(true).empty()) {
            std::printf("  kein Beitrittscode: %s\n", host->lobbyStatus().c_str());
            return false;
        }
        return true;
    }
    std::string joinCode() const { return way == Way::Direct ? host->code(true) : host->code(false); }

    // ein Bild fuer alle (Echtzeit: das Netz misst echte Zeit)
    void frame(double dt) {
        host->beforeUpdate(dt);
        if (hostWalks) botControl(hostCtl, hostTurnT, hostTurn, rng, dt, *hs, nullptr);
        else hostCtl = {};
        hs->update(dt, hostCtl, false);
        host->afterUpdate(dt);
        hs->events().clear();
        std::string lvl, anchor;
        for (auto& b : bots) {
            if (!b->c) continue;
            b->c->beforeUpdate(dt);
            if (!b->s) {
                welcome(*b);
                continue;
            }
            if (b->idle) b->ctl = {};
            else botControl(b->ctl, b->turnT, b->turn, b->rng, dt, *b->s, b.get());
            b->s->update(dt, b->ctl, false);
            b->c->afterUpdate(dt, b->ctl);
            b->s->events().clear();
        }
    }
    void welcome(Bot& b) {
        nc::Welcome w;
        if (!b.c->takeWelcome(w)) return;
        const auto& diffs = gEnv->content.difficulties();
        b.s = gEnv->make(w.level, w.seed, diffs[std::min<size_t>(w.difficulty, diffs.size() - 1)].key, b.name);
        b.s->setSupplyScale(w.supply / 100.0);
        b.s->runSeed = w.runSeed;
        b.s->placeAtSpawn();
        if (!w.anchor.empty()) b.s->placeAtAnchor(w.anchor);
        b.s->preload();
        b.c->attach(*b.s);
    }
    static void botControl(game::PlayerControl& c, double& turnT, float& turn, std::mt19937& rng, double dt, GameSession& s, Bot* b) {
        std::uniform_real_distribution<float> U(0.0f, 1.0f);
        turnT -= dt;
        if (turnT <= 0.0) {
            turnT = 0.8 + 2.5 * U(rng);
            turn = (U(rng) - 0.5f) * 2.4f;
            c.sprint = U(rng) < 0.25f;
            c.sneak = !c.sprint && U(rng) < 0.1f;
        }
        c.forward = 1.0f;
        c.side = U(rng) < 0.02f ? (U(rng) - 0.5f) * 2.0f : 0.0f;
        c.yawDelta = turn * (float)dt;
        c.pitchDelta = 0.0f;
        c.jump = U(rng) < 0.004f;
        c.jumpHeld = false;
        // gegen die Wand gelaufen: umdrehen
        if (s.player().groundSpeed < 0.3 && turnT < 0.5) turn = 3.0f;
        if (b) {
            b->lampT -= dt;
            if (b->lampT <= 0.0) b->lampT = 6.0 + 10.0 * U(rng), s.toggleFlashlight();
            b->actT -= dt;
            if (s.target() != ecs::kNull && b->actT <= 0.0) {
                s.interact();
                b->actT = 0.6;
                ++b->picks;
            } else if (b->actT <= -8.0) {
                b->actT = 0.0;
                if (U(rng) < 0.5f) s.useSlot(0);
                else s.dropSlot(0, false);
            }
        }
    }
    Bot& addBot(const std::string& name) {
        auto b = std::make_unique<Bot>(name, (u32)(bots.size() * 7919 + 13));
        b->c = std::make_unique<coop::CoopClient>(gEnv->api, kBuild);
        std::string err;
        if (!b->c->connect(botNet, directAddr, joinCode(), name, err)) std::printf("  verbinden: %s\n", err.c_str());
        bots.push_back(std::move(b));
        return *bots.back();
    }
    // laufen lassen (Echtzeit); cond: frueher fertig
    // so viele Bilder (Spielzeit), unabhaengig davon, wie schnell der Rechner sie schafft
    void runFrames(int frames, const std::function<bool()>& cond = nullptr) {
        auto next = std::chrono::steady_clock::now();
        for (int i = 0; i < frames; ++i) {
            frame(kDt);
            if (cond && cond()) break;
            next += std::chrono::microseconds(16667);
            std::this_thread::sleep_until(next);
            if (std::chrono::steady_clock::now() > next + std::chrono::milliseconds(200)) next = std::chrono::steady_clock::now();
        }
    }
    void run(double seconds, const std::function<bool()>& cond = nullptr) {
        const auto t0 = std::chrono::steady_clock::now();
        auto next = t0;
        for (;;) {
            if (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() >= seconds) break;
            frame(kDt);
            if (cond && cond()) break;
            next += std::chrono::microseconds(16667);
            std::this_thread::sleep_until(next);
            if (std::chrono::steady_clock::now() > next + std::chrono::milliseconds(200)) next = std::chrono::steady_clock::now();
        }
    }
    bool allPlaying() const {
        for (auto& b : bots)
            if (!b->c || b->c->state() != coop::CoopClient::State::Playing) return false;
        return true;
    }
    // Ort des Gasts beim Host gegen seine eigene Vorhersage (alle stehen still)
    double maxPosError() {
        double m = 0;
        for (auto& b : bots) {
            if (!b->s) continue;
            game::Pawn* p = host->pawnOf(b->c->selfId());
            if (!p || !p->alive) continue;  // Tote stehen, wo sie fielen
            const double d = std::hypot(p->player.x - b->s->player().x, p->player.y - b->s->player().y);
            m = std::max(m, d);
        }
        return m;
    }
    void stopAll() {
        for (auto& b : bots)
            if (b->c) b->c->stop();
        if (host) host->stop();
        if (lobby) lobby->stop();
    }
    // Weg des ersten Gasts (LnPath) und Ping
    int botPath() {
        for (auto& b : bots)
            if (b->c) return b->c->netStats().path;
        return 0;
    }
};

static size_t memMB() {
    PROCESS_MEMORY_COUNTERS pmc{};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    return pmc.WorkingSetSize / (1024 * 1024);
}

// =============================================================================================
// Pruefungen
// =============================================================================================
static void testFuzz() {
    std::printf("Fuzzing des Spielprotokolls\n");
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> B(0, 255);
    // gueltige Vorlagen beider Rollen
    std::vector<std::vector<u8>> seeds;
    auto add = [&](const nc::Msg& m) {
        u8 buf[nc::kMaxMsg];
        const size_t n = nc::encode(m, buf, sizeof(buf));
        check(n > 0, std::format("Vorlage Typ {} kodierbar", (int)m.type));
        seeds.emplace_back(buf, buf + n);
        nc::Msg back;
        const net::Role from = (u8)m.type < 16 && m.type != nc::Type::Bye ? net::Role::Guest : net::Role::Host;
        const net::ConnState st = m.type == nc::Type::Join || m.type == nc::Type::Welcome ? net::ConnState::Handshake : net::ConnState::Connected;
        u8 buf2[nc::kMaxMsg];
        check(nc::decode(from, st, buf, n, back) == nc::Reject::None && nc::encode(back, buf2, sizeof(buf2)) == n && std::memcmp(buf, buf2, n) == 0,
              std::format("Typ {}: hin und zurueck gleich", (int)m.type));
    };
    nc::Msg m;
    m.type = nc::Type::Join;
    m.join.build = kBuild;
    add(m);
    m = {};
    m.type = nc::Type::Input;
    m.input.count = 3;
    for (int i = 0; i < 3; ++i) m.input.f[(size_t)i] = {(u32)(10 + i), 16, 127, -5, 1234, -400, 5};
    add(m);
    m = {};
    m.type = nc::Type::Action;
    m.action = {7, 1, 0xDEADBEEF};
    add(m);
    m = {};
    m.type = nc::Type::Welcome;
    m.welcome.playerId = 2, m.welcome.level = "level5_3", m.welcome.anchor = "t_up", m.welcome.runSeed = -77, m.welcome.seed = 1234567;
    add(m);
    m = {};
    m.type = nc::Type::Snapshot;
    m.snapshot.nPlayers = 3;
    for (int i = 0; i < 3; ++i) m.snapshot.players[(size_t)i].id = (u8)i, m.snapshot.players[(size_t)i].x = 1000 * i;
    m.snapshot.nCreatures = nc::kMaxCreatures;
    for (int i = 0; i < nc::kMaxCreatures; ++i) m.snapshot.creatures[(size_t)i].netId = (u16)(i + 1), m.snapshot.creatures[(size_t)i].role = (u8)(i % 6);
    add(m);
    m = {};
    m.type = nc::Type::Event;
    m.event.kind = (u8)nc::EventKind::Toast;
    m.event.text = "Konserve aufgenommen – Ünïcode ✓";
    add(m);
    m = {};
    m.type = nc::Type::Inventory;
    m.inventory.n = 2;
    m.inventory.e[0] = {0, "konserve", 3};
    m.inventory.e[1] = {35, "batterie", 1};
    add(m);
    m = {};
    m.type = nc::Type::Roster;
    m.roster.n = 2;
    m.roster.e[0] = {0, 7, "Host"};
    m.roster.e[1] = {1, 5, "Wanderer"};
    add(m);
    // die groesste Momentaufnahme passt in 1200 Byte
    {
        nc::Msg big;
        big.type = nc::Type::Snapshot;
        big.snapshot.nPlayers = 3;
        for (int i = 0; i < 3; ++i) big.snapshot.players[(size_t)i].id = (u8)i;
        big.snapshot.nCreatures = nc::kMaxCreatures;
        for (int i = 0; i < nc::kMaxCreatures; ++i) big.snapshot.creatures[(size_t)i].netId = (u16)(i + 1);
        u8 buf[nc::kMaxMsg];
        const size_t n = nc::encode(big, buf, sizeof(buf));
        check(n > 0 && n <= 1200, std::format("groesste Momentaufnahme {} Byte", n));
    }
    // ungueltige Inhalte werden nicht kodiert
    {
        nc::Msg b;
        b.type = nc::Type::Event;
        b.event.kind = (u8)nc::EventKind::Toast;
        b.event.text = std::string("a\x01" "b");
        u8 buf[nc::kMaxMsg];
        check(nc::encode(b, buf, sizeof(buf)) == 0, "Steuerzeichen im Text: nicht kodiert");
        b.event.text = "\xE2\x80\xAE" "evil";
        check(nc::encode(b, buf, sizeof(buf)) == 0, "Richtungsumkehr (U+202E): nicht kodiert");
        b.event.text = "\xC0\xAF";
        check(nc::encode(b, buf, sizeof(buf)) == 0, "ueberlange UTF-8-Form: nicht kodiert");
        b = {};
        b.type = nc::Type::Inventory;
        b.inventory.n = 1;
        b.inventory.e[0] = {3, "../etc", 1};
        check(nc::encode(b, buf, sizeof(buf)) == 0, "Art mit Pfadzeichen: nicht kodiert");
    }
    // Rollen und Zustaende
    {
        u8 buf[nc::kMaxMsg];
        nc::Msg w;
        w.type = nc::Type::Snapshot;
        const size_t n = nc::encode(w, buf, sizeof(buf));
        nc::Msg out;
        check(nc::decode(net::Role::Guest, net::ConnState::Connected, buf, n, out) == nc::Reject::WrongRole, "Gast sendet Momentaufnahme: falsche Rolle");
        nc::Msg in;
        in.type = nc::Type::Input;
        in.input.count = 1;
        const size_t n2 = nc::encode(in, buf, sizeof(buf));
        check(nc::decode(net::Role::Guest, net::ConnState::Handshake, buf, n2, out) == nc::Reject::WrongState, "Eingabe vor dem Beitritt: falscher Zustand");
        nc::Msg j;
        j.type = nc::Type::Join;
        const size_t n3 = nc::encode(j, buf, sizeof(buf));
        check(nc::decode(net::Role::Guest, net::ConnState::Connected, buf, n3, out) == nc::Reject::WrongState, "zweiter Beitritt: falscher Zustand");
        std::vector<u8> huge(1201, 2);
        check(nc::decode(net::Role::Guest, net::ConnState::Connected, huge.data(), huge.size(), out) == nc::Reject::TooLong, "1201 Byte: zu lang");
    }
    // Zufall und Veraenderung: nie ein Absturz; was angenommen wird, ist kanonisch (wieder kodierbar, gleich)
    int accepted = 0, total = 0;
    for (int it = 0; it < 60000; ++it) {
        std::vector<u8> d;
        if (it % 3 == 0) {
            d.resize((size_t)(rng() % 300));
            for (auto& x : d) x = (u8)B(rng);
            if (!d.empty() && (rng() % 2)) d[0] = (u8)(rng() % 24);
        } else {
            d = seeds[rng() % seeds.size()];
            const int k = 1 + (int)(rng() % 4);
            for (int j = 0; j < k; ++j) {
                const int op = (int)(rng() % 4);
                if (op == 0 && !d.empty()) d[rng() % d.size()] = (u8)B(rng);
                else if (op == 1 && !d.empty()) d.resize(rng() % d.size());
                else if (op == 2) d.push_back((u8)B(rng));
                else if (!d.empty()) d[rng() % d.size()] ^= (u8)(1u << (rng() % 8));
            }
        }
        for (net::Role from : {net::Role::Guest, net::Role::Host})
            for (net::ConnState st : {net::ConnState::Handshake, net::ConnState::Connected}) {
                ++total;
                nc::Msg out;
                if (nc::decode(from, st, d.data(), d.size(), out) == nc::Reject::None) {
                    ++accepted;
                    u8 buf[nc::kMaxMsg];
                    const size_t n = nc::encode(out, buf, sizeof(buf));
                    if (n != d.size() || std::memcmp(buf, d.data(), n) != 0) {
                        check(false, std::format("angenommene Nachricht nicht kanonisch (Typ {})", (int)out.type));
                        it = 1 << 30;
                        break;
                    }
                }
            }
    }
    check(true, "kein Absturz");
    note(std::format("{} Zufallsfaelle, {} angenommen (alle kanonisch)", total, accepted));
}

static void testBots(int nBots, double seconds, Way way, const Netem* em, const std::string& label) {
    std::printf("Bots: Host + %d %s\n", nBots, label.c_str());
    Game g;
    if (!g.start("level1", 4242, way, em)) {
        check(false, "Host startet");
        return;
    }
    const auto tj = std::chrono::steady_clock::now();
    for (int i = 0; i < nBots; ++i) g.addBot(std::format("Bot{}", i + 1));
    g.run(30.0, [&] { return g.allPlaying(); });
    check(g.allPlaying(), std::format("{}: alle beigetreten ({:.1f} s)", label, std::chrono::duration<double>(std::chrono::steady_clock::now() - tj).count()));
    for (auto& b : g.bots)
        if (b->c->state() != coop::CoopClient::State::Playing) note(std::format("{}: {} - {}", label, b->name, b->c->closeReason()));
    if (way == Way::Relay || way == Way::LobbyNetem) note(std::format("{}: {} Kandidaten von der Lobby verworfen", label, g.dropped.load()));
    for (auto& b : g.bots) check(b->c->selfId() >= 1 && b->c->selfId() <= 3, std::format("{}: {} hat Spielernummer {}", label, b->name, b->c->selfId()));
    // Wesen beim Host erzwingen, die Gaeste sehen ihre Puppen
    g.hs->presence().force(game::Presence::Role::Hound, 10.0);
    const auto t0 = std::chrono::steady_clock::now();
    size_t mem0 = memMB();
    if (seconds > 600.0) {  // Langlauf: Speicher alle 5 Minuten (waechst er weiter oder bleibt er stehen?)
        for (double t = 0.0; t < seconds; t += 300.0) {
            g.run(std::min(300.0, seconds - t));
            note(std::format("{}: nach {:.0f} s Speicher {} MB, Sektoren beim Host {}", label, std::min(seconds, t + 300.0), memMB(),
                             g.hs->world().loadedSectors()));
        }
    } else {
        g.run(seconds);
    }
    // still stehen und abgleichen
    for (auto& b : g.bots) b->idle = true;
    g.hostWalks = false;
    g.run(1.5);
    const double posErr = g.maxPosError();
    int puppets = 0, picks = 0;
    u64 desync = 0, snaps = 0, corr = 0, jumps = 0, rejected = 0;
    double maxErr = 0, sumErr = 0;
    u64 errN = 0, over10 = 0;
    for (auto& b : g.bots) {
        if (!b->s) continue;
        puppets += b->s->presence().activeCount();
        picks += b->picks;
        desync += b->c->counters.desync;
        snaps += b->c->counters.snapshots;
        corr += b->c->counters.corrections;
        jumps += b->c->counters.snaps;
        rejected += b->c->counters.rejected;
        maxErr = std::max(maxErr, b->c->counters.maxError);
        sumErr += b->c->counters.sumError, errN += b->c->counters.errSamples, over10 += b->c->counters.over10cm;
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    check(g.allPlaying(), std::format("{}: nach {:.0f} s noch alle verbunden", label, secs));
    check(desync == 0, std::format("{}: geteilter Zustand gleich (Abweichungen {})", label, desync));
    check(rejected == 0, std::format("{}: keine ungueltige Nachricht vom Host ({})", label, rejected));
    check(posErr < (em ? 0.35 : 0.15), std::format("{}: Ort beim Host und beim Gast nach dem Anhalten {:.3f} m auseinander", label, posErr));
    {
        // gewaehlter Weg (nach dem Spielen: ICE kann anfangs ueber einen vorlaeufigen Kandidaten laufen)
        const int wantPath = way == Way::Relay ? LN_PATH_RELAY : way == Way::LobbyNetem ? LN_PATH_NAT : LN_PATH_DIRECT;
        bool allPath = true;
        for (auto& b : g.bots) allPath = allPath && b->c->netStats().path == wantPath;
        const LnStats st = g.bots.front()->c->netStats();
        check(allPath, std::format("{}: Weg aller Gaeste {} (1 direkt, 2 ueber NAT/Proxy, 3 Relay; erster: {})", label, wantPath, st.path));
        note(std::format("{}: Ping {} ms (+-{}), Verlust {:.0f} %, Weg {}", label, st.rttMs, st.rttVarMs, st.loss * 100.0f, st.path));
    }
    check(snaps > (u64)(seconds * 10 * nBots * (em && em->loss > 0.05 ? 0.5 : 0.8)), std::format("{}: Momentaufnahmen angekommen ({})", label, snaps));
    note(std::format("{}: Abweichung der Vorhersage im Mittel {:.3f} m, ueber 10 cm in {} von {} Momentaufnahmen", label,
                     errN ? sumErr / (double)errN : 0.0, over10, errN));
    note(std::format("{}: Korrekturen {}, Spruenge {}, groesste Abweichung {:.2f} m, Puppen {}, Versuche aufzunehmen {}, Host abgelehnt {}/{} Absichten, "
                     "Eingaben verworfen {}, Speicher {} -> {} MB",
                     label, corr, jumps, maxErr, puppets, picks, g.host->counters.actionsRefused, g.host->counters.actions, g.host->counters.inputsRefused,
                     mem0, memMB()));
    if (g.hasProxy())
        note(std::format("{}: Netz am Stoerer {:.1f} kB/s abwaerts, {:.1f} kB/s aufwaerts je Gast (mit DTLS/SCTP{}), verworfen {}", label,
                         g.bytesDown() / 1024.0 / secs / nBots, g.bytesUp() / 1024.0 / secs / nBots, way == Way::Relay ? ", TURN, beide Strecken" : "",
                         g.droppedPackets()));
    g.stopAll();
}

// Ablegen und Aufnehmen ueber das Netz; zwei greifen nach demselben Gegenstand; Wesen als Puppe beim Gast
static void testPickupAndPuppets() {
    std::printf("Ablegen, Aufnehmen, Wettlauf, Puppen\n");
    Game g;
    if (!g.start("level1", 4242)) return;
    Bot& a = g.addBot("Sammler");
    Bot& b = g.addBot("Zweiter");
    g.run(15.0, [&] { return g.allPlaying(); });
    check(g.allPlaying(), "beigetreten");
    a.idle = b.idle = true;
    g.hostWalks = false;
    g.run(1.0);
    game::Pawn* pa = g.host->pawnOf(a.c->selfId());
    if (!pa) return;
    // die Figur von "Sammler" legt eine Mandelwasser-Flasche vor sich ab (Blick nach unten)
    pa->inventory.add("almond");
    int slot = -1;
    for (int i = 0; i < game::Inventory::kSize; ++i)
        if (pa->inventory.slots[(size_t)i] && pa->inventory.slots[(size_t)i]->kind == "almond") slot = i;
    pa->player.pitch = -0.7;
    a.s->player().pitch = -0.7;
    const int hostItems0 = g.hs->itemCount(), gastItems0 = a.s->itemCount();
    check(g.hs->pawnDrop(*pa, slot, false), "Figur legt ab (Host)");
    g.run(2.0);
    check(g.hs->itemCount() == hostItems0 + 1 && a.s->itemCount() == gastItems0 + 1, std::format("abgelegt: beim Host {} -> {}, beim Gast {} -> {}", hostItems0,
                                                                                                    g.hs->itemCount(), gastItems0, a.s->itemCount()));
    check(a.s->sharedHash() == g.hs->sharedHash() && b.s->sharedHash() == g.hs->sharedHash(), "geteilter Zustand nach dem Ablegen gleich");
    // der Gast nimmt sie (Ziel ueber die eigene Sicht), der zweite greift im selben Bild nach derselben Kennzahl
    u32 h = a.s->targetHash();
    for (int i = 0; i < 120 && !h; ++i) {  // kurz umsehen, bis die Flasche im Blick ist
        a.s->player().angle += 0.05;
        g.frame(kDt);
        h = a.s->targetHash();
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    check(h != 0, "Gast hat die Flasche im Blick");
    game::Pawn* pb = g.host->pawnOf(b.c->selfId());
    // V13.3: beide stehen in Reichweite (der zweite etwa 1,6 m daneben) - wer zuerst ankommt, haengt vom Netz ab. Geprueft
    // wird, dass genau einer die Flasche bekommt und Host und Gaeste sich einig sind (ferne Griffe: Teil "attacks")
    if (pb) note(std::format("Abstand der beiden Figuren beim Host {:.2f} m", std::hypot(pb->player.x - pa->player.x, pb->player.y - pa->player.y)));
    if (h) {
        a.s->interact();
        b.s->netAction(1, h);  // Wettlauf: der zweite will im selben Bild dieselbe
    }
    g.run(2.0);
    const int hostA = pa->inventory.count("almond"), hostB = pb ? pb->inventory.count("almond") : -1;
    const int gastA = a.s->inventory().count("almond"), gastB = b.s->inventory().count("almond");
    check(pb && hostA + hostB == 1 && gastA == hostA && gastB == hostB,
          std::format("Wettlauf: genau einer hat die Flasche, Host und Gast einig (Sammler {}/{}, Zweiter {}/{})", hostA, gastA, hostB, gastB));
    check(g.hs->itemCount() == hostItems0 && a.s->itemCount() == gastItems0, "aus der Welt verschwunden (Host und Gast)");
    check(a.s->sharedHash() == g.hs->sharedHash() && b.s->sharedHash() == g.hs->sharedHash(), "geteilter Zustand nach dem Aufnehmen gleich");
    // benutzen: der Host wendet es an (Verstand steigt)
    Bot& w = hostB == 1 ? b : a;
    game::Pawn* pw = hostB == 1 ? pb : pa;
    pw->stats.sanity = 40.0;
    int aslot = -1;
    for (int i = 0; i < game::Inventory::kSize; ++i)
        if (w.s->inventory().slots[(size_t)i] && w.s->inventory().slots[(size_t)i]->kind == "almond") aslot = i;
    if (aslot >= 0) w.s->useSlot(aslot);
    g.run(1.5);
    check(pw->stats.sanity > 60.0 && w.s->stats().sanity > 60.0 && w.s->inventory().count("almond") == 0,
          std::format("benutzt ({}): Verstand beim Host {:.0f}, beim Gast {:.0f}", hostB == 1 ? "Zweiter" : "Sammler", pw->stats.sanity, w.s->stats().sanity));
    // ein Hound beim Gast: die Puppe folgt dem Original (100 ms zurueck)
    g.hs->presence().force(game::Presence::Role::Hound, 9.0);
    g.run(3.0, [&] { return a.s->presence().has(game::Presence::Role::Hound); });
    check(a.s->presence().has(game::Presence::Role::Hound), "Gast sieht den Hound als Puppe");
    double worst = 0;
    for (int i = 0; i < 120; ++i) {
        g.frame(kDt);
        if (g.hs->presence().has(game::Presence::Role::Hound) && a.s->presence().has(game::Presence::Role::Hound))
            worst = std::max(worst, (double)length(g.hs->presence().posOf(game::Presence::Role::Hound) - a.s->presence().posOf(game::Presence::Role::Hound)));
        std::this_thread::sleep_for(std::chrono::milliseconds(14));
    }
    check(worst < 1.6, std::format("Puppe hoechstens {:.2f} m hinter dem Hound", worst));
    g.stopAll();
}

// Koop-Stand: der Host speichert Werte und Inventar der Gaeste nach Namen; ein neuer Host mit diesem Stand gibt sie
// einem Gast mit demselben Namen zurueck
static void testCoopSave() {
    std::printf("Koop-Stand: Gaeste speichern und wiederherstellen\n");
    Json saved;
    {
        Game g;
        if (!g.start("level0", 5150)) return;
        Bot& b = g.addBot("Merker");
        g.run(15.0, [&] { return g.allPlaying(); });
        game::Pawn* p = g.host->pawnOf(b.c->selfId());
        check(p != nullptr, "Gast spielt");
        if (!p) return;
        p->inventory.add("battery");
        p->inventory.add("almond");
        p->stats.sanity = 42.0;
        g.run(0.5);
        saved = g.host->saveGuests();
        g.stopAll();
    }
    check(saved["Merker"].isObject(), "Stand enthaelt den Gast");
    Game g2;
    if (!g2.start("level0", 5150)) return;
    g2.host->loadGuests(saved);
    Bot& b2 = g2.addBot("Merker");
    g2.run(15.0, [&] { return g2.allPlaying(); });
    g2.run(1.0);
    game::Pawn* p2 = g2.host->pawnOf(b2.c->selfId());
    check(p2 && p2->inventory.count("battery") == 1 && p2->inventory.count("almond") == 1 && std::fabs(p2->stats.sanity - 42.0) < 1.0,
          "Werte und Inventar zurueck (Host)");
    check(b2.s && b2.s->inventory().count("battery") == 1 && b2.s->inventory().count("almond") == 1, "Inventar zurueck (Gast)");
    Bot& b3 = g2.addBot("Fremder");
    g2.run(10.0, [&] { return g2.allPlaying(); });
    game::Pawn* p3 = g2.host->pawnOf(b3.c->selfId());
    check(p3 && p3->inventory.count() == 0, "ein anderer Name bekommt nichts");
    g2.stopAll();
}

// Fuzzing ueber den echten Netzweg: ein Gast mit Schluessel sendet zufaellige, formal gueltige Eingaben und Absichten
// (zufaellige Achsen, Blicke, Tasten, Zeiten, Kennzahlen, Plaetze) so schnell, wie die Rate erlaubt; dazwischen
// einzelne ungueltige. Der Host darf weder abstuerzen noch den ehrlichen Gast stoeren.
static void testNetFuzz(double seconds) {
    std::printf("Fuzzing ueber den Netzweg (%.0f s)\n", seconds);
    Game g;
    if (!g.start("level1", 4242)) return;
    Bot& honest = g.addBot("Ehrlich");
    g.run(15.0, [&] { return g.allPlaying(); });
    coop::NetApi& A = gEnv->api;
    std::mt19937 rng(77);
    auto enc = [](const nc::Msg& m) {
        u8 buf[nc::kMaxMsg];
        const size_t n = nc::encode(m, buf, sizeof(buf));
        return std::vector<u8>(buf, buf + n);
    };
    u64 sent = 0, sessions = 0, failedJoins = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds) {
        // je Durchgang eine neue Verbindung (nach Trennung durch ungueltige Nachrichten)
        const LnConfig cfg = g.botNet.config();
        LnContext* e = A.join(&cfg, nullptr, g.joinCode().c_str(), "Fuzzer");
        ++sessions;
        bool up = false, closed = false;
        int why = -1;
        const u64 sent0 = sent;
        for (int i = 0; i < 600 && !up && why < 0; ++i) {
            g.frame(kDt);
            LnEvent ev;
            while (A.poll(e, &ev)) {
                up = up || ev.type == LN_EV_CONNECTED;
                if (ev.type == LN_EV_DISCONNECTED) why = ev.reason;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        if (!up) {
            ++failedJoins;
            if (failedJoins <= 3) note(std::format("Fuzzer-Verbindung {} nicht offen (Grund {})", sessions, why));
            A.close(e);
            g.run(1.0);
            continue;
        }
        nc::Msg j;
        j.type = nc::Type::Join;
        j.join.build = kBuild;
        auto jb = enc(j);
        A.send(e, 0, LN_RELIABLE, jb.data(), (int)jb.size());
        u32 seq = 1, aseq = 1;
        const auto t1 = std::chrono::steady_clock::now();
        while (!closed && std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count() < 30.0 &&
               std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds) {
            for (int k = 0; k < 2; ++k) {
                nc::Msg m;
                const int kind = (int)(rng() % 100);
                std::vector<u8> d;
                if (kind < 70) {
                    m.type = nc::Type::Input;
                    m.input.count = (u8)(1 + rng() % 4);
                    for (int i = 0; i < m.input.count; ++i)
                        m.input.f[(size_t)i] = {seq++, (u8)(1 + rng() % 100), (i8)((int)(rng() % 255) - 127), (i8)((int)(rng() % 255) - 127),
                                                (i16)((int)(rng() % 6285) - 3142), (i16)((int)(rng() % 2901) - 1450), (u8)(rng() % 16)};
                    if (rng() % 50 == 0) seq += (u32)(rng() % 5000);  // Spruenge
                    d = enc(m);
                } else if (kind < 95) {
                    m.type = nc::Type::Action;
                    m.action.seq = aseq++;
                    m.action.kind = (u8)(1 + rng() % 7);
                    m.action.arg = m.action.kind == 1 ? (u32)rng() : (u32)(rng() % (m.action.kind == 5 ? 2 : m.action.kind == 7 ? 4 : 36));
                    d = enc(m);
                } else if (kind < 98) {
                    m.type = nc::Type::Ping;
                    m.ping = {(u32)rng(), (u32)rng()};
                    d = enc(m);
                } else {
                    d.resize(1 + rng() % 200);  // ungueltig
                    for (auto& x : d) x = (u8)rng();
                }
                if (!d.empty()) A.send(e, 0, (rng() & 1) ? LN_RELIABLE : LN_UNRELIABLE, d.data(), (int)d.size()), ++sent;
            }
            g.frame(kDt);
            LnEvent ev;
            while (A.poll(e, &ev))
                if (ev.type == LN_EV_DISCONNECTED) closed = true, why = ev.reason;
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
        }
        if (sessions <= 3) note(std::format("Fuzzer-Verbindung {}: {} Nachrichten, Ende {}", sessions, sent - sent0, closed ? coop::reasonText(why) : "Zeit"));
        A.close(e);
        g.run(0.3);
    }
    g.run(1.0);
    check(honest.c->state() == coop::CoopClient::State::Playing && honest.c->counters.desync == 0 && honest.c->counters.rejected == 0,
          "der ehrliche Gast spielt ungestoert weiter");
    note(std::format("{} Nachrichten in {} Verbindungen; Host: {} abgelehnt, {} gedrosselt, {} Eingaben, davon {} verworfen (Zeitbudget), "
                     "{} Absichten, davon {} abgelehnt, {} Trennungen",
                     sent, sessions, g.host->counters.rejected, g.host->counters.dropped, g.host->counters.inputs, g.host->counters.inputsRefused,
                     g.host->counters.actions, g.host->counters.actionsRefused, g.host->counters.kicked));
    check(sent > (u64)(seconds * 30), std::format("genug Nachrichten gesendet ({}; {} Verbindungen nicht offen)", sent, failedJoins));
    g.stopAll();
}

static void testFull() {
    std::printf("Voll: vier Spieler, ein fuenfter\n");
    Game g;
    if (!g.start("level0", 77)) return;
    for (int i = 0; i < 3; ++i) g.addBot(std::format("B{}", i));
    g.run(15.0, [&] { return g.allPlaying(); });
    check(g.allPlaying(), "drei Gaeste beigetreten (vier Spieler)");
    Bot& extra = g.addBot("Zuviel");
    g.run(6.0, [&] { return extra.c->state() == coop::CoopClient::State::Closed; });
    check(extra.c->state() == coop::CoopClient::State::Closed, "fuenfter Spieler abgewiesen: " + extra.c->closeReason());
    g.bots.pop_back();
    g.run(1.0);
    check(g.allPlaying(), "die anderen bleiben verbunden");
    g.stopAll();
}

// boeswilliger Gast: kennt den Schluessel, spricht das Protokoll aber falsch
static void testAttacks() {
    std::printf("Angriffe eines Gasts mit Schluessel\n");
    Game g;
    if (!g.start("level1", 4242)) return;
    Bot& honest = g.addBot("Ehrlich");
    g.run(15.0, [&] { return g.allPlaying(); });
    checkN(g.allPlaying(), "ehrlicher Gast spielt");
    coop::NetApi& A = gEnv->api;
    auto openEvil = [&](const char* name) {
        const LnConfig cfg = g.botNet.config();
        LnContext* e = A.join(&cfg, nullptr, g.joinCode().c_str(), name);
        bool up = false;
        int why = -1;
        for (int i = 0; i < 300 && !up; ++i) {
            g.frame(kDt);
            LnEvent ev;
            while (A.poll(e, &ev)) {
                up = up || ev.type == LN_EV_CONNECTED;
                if (ev.type == LN_EV_DISCONNECTED) why = ev.reason;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!up) note(std::format("{}: Transport nicht verbunden (Grund {})", name, why));
        return e;
    };
    auto sendRaw = [&](LnContext* e, const std::vector<u8>& d, int ch = LN_RELIABLE) { A.send(e, 0, ch, d.data(), (int)d.size()); };
    auto enc = [](const nc::Msg& m) {
        u8 buf[nc::kMaxMsg];
        const size_t n = nc::encode(m, buf, sizeof(buf));
        return std::vector<u8>(buf, buf + n);
    };
    auto pumpEvil = [&](LnContext* e, double s, bool* closed) {
        const auto t0 = std::chrono::steady_clock::now();
        while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < s) {
            g.frame(kDt);
            LnEvent ev;
            while (A.poll(e, &ev))
                if (ev.type == LN_EV_DISCONNECTED && closed) *closed = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    };
    // 1) Muell und falsche Rollen: nach 8 ungueltigen getrennt
    {
        LnContext* e = openEvil("Muell");
        // erst Join (gueltig), dann Unfug
        nc::Msg j;
        j.type = nc::Type::Join;
        j.join.build = kBuild;
        sendRaw(e, enc(j));
        pumpEvil(e, 1.0, nullptr);
        const u64 rej0 = g.host->counters.rejected;
        nc::Msg w;
        w.type = nc::Type::Snapshot;
        sendRaw(e, enc(w));                           // falsche Rolle
        sendRaw(e, enc(j));                           // zweiter Beitritt
        sendRaw(e, std::vector<u8>{2, 9, 9, 9});      // Eingabe mit falscher Laenge
        sendRaw(e, std::vector<u8>{200, 1, 2});       // unbekannter Typ
        sendRaw(e, std::vector<u8>(1100, 3));         // Aktion mit Rest
        for (int i = 0; i < 6; ++i) sendRaw(e, std::vector<u8>{17});
        bool closed = false;
        pumpEvil(e, 2.0, &closed);
        checkN(g.host->counters.rejected >= rej0 + 8, std::format("Host verwirft ungueltige Nachrichten ({})", g.host->counters.rejected - rej0));
        checkN(closed, "nach mehr als 8 ungueltigen Nachrichten getrennt");
        A.close(e);
    }
    // 2) Speedhack: Eingaben mit 100 ms je Paket, 60 Pakete je Sekunde (6-fache Zeit)
    {
        LnContext* e = openEvil("Schnell");
        nc::Msg j;
        j.type = nc::Type::Join;
        j.join.build = kBuild;
        sendRaw(e, enc(j));
        pumpEvil(e, 1.5, nullptr);
        game::Pawn* p = nullptr;
        for (int id = 1; id <= 3 && !p; ++id)
            if (game::Pawn* q = g.host->pawnOf(id); q && q->name == "Schnell") p = q;
        if (!p) {
            std::string ids;
            for (int id = 1; id <= 3; ++id)
                if (game::Pawn* q = g.host->pawnOf(id)) ids += std::format("{}={} ", id, q->name);
            note("Figuren beim Host: " + ids);
        }
        checkN(p != nullptr, "Speedhack-Gast hat eine Figur");
        if (p) {
            const double x0 = p->player.x, y0 = p->player.y;
            const u64 ref0 = g.host->counters.inputsRefused, acc0 = g.host->counters.inputs;
            u64 sentIn = 0;
            u32 seq = 1;
            const auto t0 = std::chrono::steady_clock::now();
            while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 3.0) {
                nc::Msg in;
                in.type = nc::Type::Input;
                in.input.count = 1;
                in.input.f[0] = {seq++, 100, 127, 0, nc::toMrad(p->player.angle), 0, 1};
                sendRaw(e, enc(in), LN_UNRELIABLE), ++sentIn;
                g.frame(kDt);
                LnEvent ev;
                while (A.poll(e, &ev)) {}
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            const double moved = std::hypot(p->player.x - x0, p->player.y - y0);
            note(std::format("Speedhack: {} gesendet, {} angenommen, {} verworfen (Zeitbudget), Host gedrosselt gesamt {}", sentIn, g.host->counters.inputs - acc0, g.host->counters.inputsRefused - ref0, g.host->counters.dropped));
            checkN((g.host->counters.inputsRefused - ref0) * 2 > sentIn, std::format("zu schnelle Uhr (6-fache Zeit): {} von {} Eingaben verworfen", g.host->counters.inputsRefused - ref0, sentIn));
            checkN(moved < 3.0 * 6.5, std::format("in 3 s hoechstens rennend weit ({:.1f} m)", moved));
        }
        // 3) erfundene und ferne Gegenstaende, Flut von Absichten
        const u64 refA = g.host->counters.actionsRefused, dropA = g.host->counters.dropped;
        u32 farHash = 0;
        g.hs->registry().each<game::Pickup, game::Transform>([&](ecs::Entity, game::Pickup& pk, game::Transform& t) {
            if (p && std::hypot(t.pos.x - p->player.x, t.pos.y - p->player.y) > 8.0 && !farHash) farHash = game::worldItemHash(pk.worldId);
        });
        const int itemsBefore = g.hs->itemCount();
        for (u32 i = 0; i < 40; ++i) {
            nc::Msg a;
            a.type = nc::Type::Action;
            a.action = {i + 1, 1, i == 0 && farHash ? farHash : 0x1234567u + i};
            sendRaw(e, enc(a));
        }
        pumpEvil(e, 2.0, nullptr);
        checkN(g.host->counters.actionsRefused > refA, std::format("erfundene/ferne Gegenstaende abgelehnt ({})", g.host->counters.actionsRefused - refA));
        checkN(g.host->counters.dropped > dropA, std::format("Flut von Absichten gedrosselt ({} verworfen)", g.host->counters.dropped - dropA));
        checkN(g.hs->itemCount() == itemsBefore, "kein Gegenstand verschwunden");
        A.close(e);
    }
    // 4) falscher Spielstand (andere Daten): abgewiesen
    {
        LnContext* e = openEvil("Alt");
        nc::Msg j;
        j.type = nc::Type::Join;
        j.join.build = kBuild ^ 1;
        sendRaw(e, enc(j));
        bool closed = false;
        pumpEvil(e, 2.0, &closed);
        checkN(closed, "andere Spieldaten: abgewiesen");
        A.close(e);
    }
    g.run(1.0);
    checkN(honest.c->state() == coop::CoopClient::State::Playing && honest.c->counters.desync == 0, "der ehrliche Gast spielt ungestoert weiter");
    g.stopAll();
}

static void testReconnectKick() {
    std::printf("Wiederverbinden, Entfernen, Sperren\n");
    Game g;
    if (!g.start("level0", 99)) return;
    Bot& b = g.addBot("Zurueck");
    g.run(15.0, [&] { return g.allPlaying(); });
    checkN(g.allPlaying(), "beigetreten");
    const int id = b.c->selfId();
    game::Pawn* p = g.host->pawnOf(id);
    if (p) p->inventory.add("battery");
    g.run(1.0);
    checkN(b.s && b.s->inventory().count("battery") == 1, "Inventar vom Host beim Gast");
    // Verbindung verlieren (ohne Abschied), wiederkommen
    coop::CoopClient* c = b.c.get();
    c->stop();
    g.run(1.0);
    b.s.reset();
    std::string err;
    checkN(c->connect(g.botNet, g.directAddr, g.joinCode(), "Zurueck", err), "erneut verbinden (gleicher Code)");
    g.run(10.0, [&] { return g.allPlaying(); });
    checkN(g.allPlaying() && c->selfId() == id, std::format("gleiche Spielernummer nach dem Wiederverbinden ({} / {})", c->selfId(), id));
    g.run(1.0);
    checkN(b.s && b.s->inventory().count("battery") == 1, "Inventar nach dem Wiederverbinden zurueck");
    // entfernen
    checkN(g.host->kick(id, false), "Host entfernt den Gast");
    g.run(2.0, [&] { return c->state() == coop::CoopClient::State::Closed; });
    checkN(c->state() == coop::CoopClient::State::Closed, "Gast getrennt: " + c->closeReason());
    // sperren: zweiter Gast, gesperrt, versucht es erneut
    Bot& b2 = g.addBot("Gesperrt");
    g.run(10.0, [&] { return b2.c->state() == coop::CoopClient::State::Playing; });
    checkN(g.host->kick(b2.c->selfId(), true), "Host sperrt den Gast");
    g.run(2.0);
    b2.c->stop();  // V13.3 (B-028): wie die App (leaveCoop) - erst der Gast, dann die Sitzung freigeben (sonst raeumt stop() spaeter darin auf)
    b2.s.reset();
    checkN(b2.c->connect(g.botNet, g.directAddr, g.joinCode(), "Gesperrt", err), "gesperrter Gast versucht es erneut");
    g.run(8.0, [&] { return b2.c->state() == coop::CoopClient::State::Closed; });
    checkN(b2.c->state() != coop::CoopClient::State::Playing, "gesperrt: kein Beitritt (" + b2.c->closeReason() + ")");
    g.stopAll();
}

// die Gruppe wechselt durch alle zehn Ebenen
static void testJourney() {
    std::printf("Reise: Host + 2 Gaeste durch alle Ebenen\n");
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level5", "level5_3", "level188", "level903", "level37", "level36"};
    Game g;
    if (!g.start("level0", 31337)) return;
    g.addBot("Erste");
    g.addBot("Zweite");
    g.run(15.0, [&] { return g.allPlaying(); });
    check(g.allPlaying(), "Reise: beigetreten");
    for (auto& b : g.bots) b->idle = true;
    g.hostWalks = false;
    std::vector<std::string> visited{"level0"};
    std::vector<std::string> path{"level0"};
    for (int step = 0; step < 24; ++step) {
        const std::string cur = g.hs->levelId;
        const auto L = gEnv->content.level(cur, world::kLayoutV6);
        // naechster Uebergang: auf dem kuerzesten Weg (Breitensuche ueber die Uebergaenge) zur naechsten noch nicht
        // besuchten Ebene; Loecher und Tauchgaenge zaehlen nicht (dafuer muesste man fallen oder tauchen)
        auto usable = [](const world::MapTransition& t) { return !t.target.empty() && t.x1 > t.x0 && t.kind != "hole" && t.kind != "dive"; };
        // Breitensuche: erster Uebergang zu jeder erreichbaren Ebene
        auto reach = [&](const std::string& from, std::map<std::string, std::string>* first) {
            std::map<std::string, std::string> fs{{from, ""}};
            std::deque<std::string> q{from};
            std::vector<std::string> order;
            while (!q.empty()) {
                const std::string at = q.front();
                q.pop_front();
                const auto A = gEnv->content.level(at, world::kLayoutV6);
                if (!A || !A->map) continue;
                for (const auto& t : A->map->transitions) {
                    if (!usable(t) || fs.count(t.target)) continue;
                    fs[t.target] = at == from ? t.id : fs[at];
                    order.push_back(t.target);
                    q.push_back(t.target);
                }
            }
            if (first) *first = fs;
            return order;
        };
        std::map<std::string, std::string> firstStep;
        const auto order = reach(cur, &firstStep);
        auto unvisited = [&](const std::string& l) { return std::find(visited.begin(), visited.end(), l) == visited.end(); };
        // naechste unbesuchte Ebene, von der aus alle anderen unbesuchten noch erreichbar bleiben (Sackgassen zuletzt)
        std::string goal;
        for (const auto& cand : order) {
            if (!unvisited(cand) || !goal.empty()) continue;
            const auto from = reach(cand, nullptr);
            bool ok = true;
            for (const auto& o : order)
                if (o != cand && unvisited(o)) ok = ok && std::find(from.begin(), from.end(), o) != from.end();
            if (ok) goal = cand;
        }
        for (const auto& cand : order)
            if (goal.empty() && unvisited(cand)) goal = cand;
        const world::MapTransition* tr = nullptr;
        if (!goal.empty())
            for (const auto& t : L->map->transitions)
                if (t.id == firstStep[goal] && usable(t)) tr = &t;
        if (!tr) break;
        // alle in den Ausloesebereich; die noetige Sache (Sicherung) traegt der Host
        const double cx = (tr->x0 + tr->x1) * 0.5, cy = (tr->y0 + tr->y1) * 0.5;
        if (!tr->needs.empty()) g.hs->inventory().add(tr->needs);
        auto put = [&](game::Player& p) {
            p.x = cx, p.y = cy;
            p.z = g.hs->world().tile(ifloor(cx), ifloor(cy)).floor;
            p.vx = p.vy = p.vz = 0;
        };
        put(g.hs->player());
        g.hs->preload();
        for (auto& b : g.bots)
            if (game::Pawn* p = g.host->pawnOf(b->c->selfId())) put(p->player);
        std::string next, anchor;
        bool changed = false;
        for (int f = 0; f < 600 && !changed; ++f) {
            g.frame(kDt);
            changed = g.host->takeLevelChange(next, anchor);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        check(changed && next == tr->target, std::format("{} -> {}: die Gruppe geht ({})", cur, tr->target, next));
        if (!changed) break;
        // Host wechselt (wie App::changeLevel)
        const long long seed = Env::seedFor(g.hs->runSeedOr(), next);
        auto ns = gEnv->make(next, seed, g.hs->difficulty, "Host");
        ns->setSupplyScale(g.host->supplyScaleNext());
        ns->carryOver(*g.hs);
        if (!ns->placeAtAnchor(anchor)) ns->placeAtSpawn();
        ns->preload();
        g.hs = std::move(ns);
        g.host->attach(*g.hs, anchor, gEnv->diffIndex("medium"));
        // Gaeste folgen
        bool all = false;
        for (int f = 0; f < 900 && !all; ++f) {
            g.frame(kDt);
            for (auto& b : g.bots) {
                std::string lv, an;
                if (b->c->takeLevelChange(lv, an)) {
                    const long long s2 = Env::seedFor(b->s->runSeedOr(), lv);
                    auto bs = gEnv->make(lv, s2, b->s->difficulty, b->name);
                    bs->setSupplyScale(b->c->pendingSupply());
                    bs->carryOver(*b->s);
                    if (!bs->placeAtAnchor(an)) bs->placeAtSpawn();
                    bs->preload();
                    b->s = std::move(bs);
                    b->c->attach(*b->s);
                }
            }
            all = true;
            for (auto& b : g.bots) all = all && b->s && b->s->levelId == next && b->c->counters.snapshots > 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        g.run(1.5);
        bool sameSeed = true, together = true;
        for (auto& b : g.bots) {
            sameSeed = sameSeed && b->s && b->s->seed == g.hs->seed && std::fabs(b->s->supplyScale() - g.hs->supplyScale()) < 1e-6;
            together = together && b->s && std::hypot(b->s->player().x - g.hs->player().x, b->s->player().y - g.hs->player().y) < 4.0;
        }
        check(all && sameSeed, std::format("{}: beide Gaeste in derselben Ebene, gleicher Seed, Vorraete {:.0f} %", next, g.hs->supplyScale() * 100.0));
        check(together, std::format("{}: alle am Anker beisammen", next));
        check(g.allPlaying(), std::format("{}: alle verbunden", next));
        visited.push_back(next);
        path.push_back(next);
        if (std::set<std::string>(visited.begin(), visited.end()).size() == 10) break;
    }
    std::sort(visited.begin(), visited.end());
    const size_t distinct = (size_t)(std::unique(visited.begin(), visited.end()) - visited.begin());
    check(distinct == 10, std::format("Reise: {} von 10 Ebenen erreicht", distinct));
    std::string way;
    for (auto& v : path) way += v + " ";
    note("Weg: " + way);
    g.stopAll();
}

static void testDeath() {
    std::printf("Tod, Zuschauen, Wiedereinstieg\n");
    Game g;
    if (!g.start("level1", 4242)) return;
    Bot& b = g.addBot("Opfer");
    g.run(15.0, [&] { return g.allPlaying(); });
    b.idle = true;
    g.hostWalks = false;
    game::Pawn* p = g.host->pawnOf(b.c->selfId());
    check(p != nullptr, "Figur des Gasts beim Host");
    if (!p) return;
    p->stats.health = 0.0;
    g.run(2.0);
    check(!p->alive && b.c->dead() && b.s->stats().dead, "Gast ist tot (Host und Gast)");
    check(b.c->spectate() == 0, "der Tote sieht dem Host zu");
    g.runFrames(60 * 23, [&] { return p->alive && !b.c->dead(); });  // 20 s Spielzeit bis zum Wiedereinstieg
    check(p->alive && !b.c->dead() && !b.s->stats().dead, "nach 20 s am Sicherungspunkt zurueck");
    // alle tot: die Gruppe beginnt neu
    p->stats.health = 0.0;
    g.hs->stats().health = 0.0;
    g.hs->stats().dead = true;
    g.runFrames(60 * 10, [&] { return p->alive && !g.hs->stats().dead; });
    check(p->alive && !g.hs->stats().dead, "alle tot: Neubeginn am Sicherungspunkt");
    g.stopAll();
}

// Abbruch des Netzes: der Netzstoerer verwirft 13 s lang alles -> beide Seiten trennen nach dem Zeitlimit (10 s); der
// Gast kommt mit demselben Code zurueck und bekommt seinen Platz und sein Inventar
static void testOutage() {
    std::printf("Netzausfall, Zeitlimit, Wiederverbinden\n");
    Game g;
    Netem ok{20, 5, 0.0, 0.0, 0.0, 0};
    if (!g.start("level0", 99, Way::LobbyNetem, &ok)) return;
    Bot& b = g.addBot("Funkloch");
    g.run(20.0, [&] { return g.allPlaying(); });
    checkN(g.allPlaying(), "beigetreten (ueber den Netzstoerer)");
    const int id = b.c->selfId();
    if (game::Pawn* p = g.host->pawnOf(id)) p->inventory.add("battery");
    g.run(1.0);
    Netem dead = ok;
    dead.loss = 1.0;
    g.setNetem(dead);
    const auto t0 = std::chrono::steady_clock::now();
    g.run(16.0, [&] { return b.c->state() == coop::CoopClient::State::Closed; });
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    checkN(b.c->state() == coop::CoopClient::State::Closed, std::format("Gast merkt den Ausfall nach {:.1f} s: {}", secs, b.c->closeReason()));
    g.run(2.0);
    bool hostKnows = false;
    for (const auto& pi : g.host->players())
        if (pi.id == id) hostKnows = !pi.connected;
    checkN(hostKnows, "Host fuehrt den Gast als getrennt (Platz bleibt 120 s)");
    g.setNetem(ok);
    b.s.reset();
    std::string err;
    checkN(b.c->connect(g.botNet, g.directAddr, g.joinCode(), "Funkloch", err), "neu verbinden");
    g.run(20.0, [&] { return g.allPlaying(); });
    checkN(g.allPlaying() && b.c->selfId() == id, std::format("zurueck mit derselben Spielernummer ({} / {})", b.c->selfId(), id));
    g.run(1.0);
    checkN(b.s && b.s->inventory().count("battery") == 1, "Inventar nach dem Ausfall zurueck");
    g.stopAll();
}

// Host geht: (a) sauber beendet -> die Gaeste erfahren es sofort; (b) Host-Prozess "stirbt" (alles verworfen) -> Zeitlimit
static void testHostAbort() {
    std::printf("Host beendet, Host faellt aus\n");
    {
        Game g;
        if (!g.start("level0", 7)) return;
        Bot& b = g.addBot("Bleibt");
        g.run(15.0, [&] { return g.allPlaying(); });
        checkN(g.allPlaying(), "beigetreten");
        g.host->stop();
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 300 && b.c->state() != coop::CoopClient::State::Closed; ++i) {
            b.c->beforeUpdate(kDt);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        checkN(b.c->state() == coop::CoopClient::State::Closed, std::format("Host beendet: Gast getrennt nach {:.2f} s ({})", secs, b.c->closeReason()));
        g.host.reset();
        g.stopAll();
    }
    {
        Game g;
        Netem ok{20, 5, 0.0, 0.0, 0.0, 0};
        if (!g.start("level0", 7, Way::LobbyNetem, &ok)) return;
        Bot& b = g.addBot("Wartet");
        g.run(20.0, [&] { return g.allPlaying(); });
        checkN(g.allPlaying(), "beigetreten (Netzstoerer)");
        Netem dead = ok;
        dead.loss = 1.0;
        g.setNetem(dead);  // der Host antwortet nicht mehr (wie ein abgestuerzter Prozess)
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 1600 && b.c->state() != coop::CoopClient::State::Closed; ++i) {
            b.c->beforeUpdate(kDt);
            if (b.s) b.c->afterUpdate(kDt, {});
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        checkN(b.c->state() == coop::CoopClient::State::Closed && secs < 15.0,
              std::format("Host faellt aus: Gast getrennt nach {:.1f} s ({})", secs, b.c->closeReason()));
        g.stopAll();
    }
}

// kein Weg: die Lobby verwirft alle Kandidaten und es gibt kein Relay -> klare Meldung statt Haengen
static void testNoPath() {
    std::printf("Kein Verbindungsweg (UDP gesperrt)\n");
    Game g;
    if (!g.start("level0", 5)) return;
    // Lobby neu: ohne TURN, alle Kandidaten verworfen
    g.host->stop();
    g.lobby->stop();
    lim::netc::LobbyConfig lc;
    lc.bind = "127.0.0.1", lc.port = 0, lc.turn = false;
    lc.candidateHook = [](std::string&, bool) { return false; };
    g.lobby = std::make_unique<lim::netc::LobbyServer>();
    std::string err;
    g.lobby->start(lc, err);
    g.hostNet.lobby = g.botNet.lobby = "ws://127.0.0.1:" + std::to_string(g.lobby->port());
    g.host = std::make_unique<coop::CoopHost>(gEnv->api, "Host", kBuild);
    g.host->start(g.hostNet, 4, err);
    g.host->attach(*g.hs, "", gEnv->diffIndex("medium"));
    for (int i = 0; i < 600 && g.host->code(false).empty(); ++i) g.frame(kDt), std::this_thread::sleep_for(std::chrono::milliseconds(5));
    Bot& b = g.addBot("Abgeschnitten");
    const auto t0 = std::chrono::steady_clock::now();
    g.run(40.0, [&] { return b.c->state() == coop::CoopClient::State::Closed; });
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    checkN(b.c->state() == coop::CoopClient::State::Closed && b.c->closeReason().find("Verbindungsweg") != std::string::npos,
          std::format("klare Meldung nach {:.0f} s: {}", secs, b.c->closeReason()));
    g.stopAll();
}

// V13.3: geteilter Stand bei fernen Funden und spaetem Beitritt. Im Langlauf gefunden: ein Gast verpasste einen Fund,
// den der Host in einem beim Gast nicht geladenen Sektor aufnahm - danach stimmte die Pruefsumme nie wieder. Ein spaeter
// Gast sah zudem Funde, die beim Host schon fort waren.
static void testLateJoin() {
    std::printf("Ferne Funde, spaeter Beitritt\n");
    // a) Pruefschicht: ein Gast bekommt "aufgenommen" fuer einen Fund, den er (noch) nicht geladen hat. Der Fund darf
    //    beim Laden nicht erscheinen, und die Pruefsumme muss der des Hosts gleichen.
    {
        auto host = gEnv->make("level1", 4242, "medium", "Host");
        host->runSeed = 4242;
        host->placeAtSpawn();
        host->preload();
        u32 hash = 0;
        vec3 at{};
        double best = 1e9;
        const game::Player& hp = host->player();
        host->registry().each<game::Pickup, game::Transform>([&](ecs::Entity, game::Pickup& pk, game::Transform& tr) {
            const double d = std::hypot(tr.pos.x - hp.x, tr.pos.y - hp.y);
            if (d < best && pk.worldId.rfind("drop:", 0) != 0) best = d, hash = game::worldItemHash(pk.worldId), at = tr.pos;
        });
        game::Pawn probe(3, "Pruefer", gEnv->content.difficulty(host->difficulty), &gEnv->content);
        probe.player = game::Player(at.x + 0.4, at.y, at.z, 3.14159);
        probe.player.pitch = -0.6;
        std::string msg;
        int note = -1;
        const bool took = hash && host->pawnInteract(probe, hash, msg, note);
        checkN(took, std::format("Host: Pruef-Figur nimmt einen Fund auf ({:.1f} m vom Start)", best));
        auto guest = gEnv->make("level1", 4242, "medium", "Gast");
        guest->netRole = GameSession::NetRole::Guest;
        int before = 0;
        guest->registry().each<game::Pickup>([&](ecs::Entity, game::Pickup& pk) { before += game::worldItemHash(pk.worldId) == hash ? 1 : 0; });
        guest->netRemoveItem(hash);  // kommt vor dem Laden an
        guest->runSeed = 4242;
        guest->placeAtSpawn();
        guest->preload();
        int after = 0;
        guest->registry().each<game::Pickup>([&](ecs::Entity, game::Pickup& pk) { after += game::worldItemHash(pk.worldId) == hash ? 1 : 0; });
        checkN(before == 0 && after == 0, std::format("Gast: Fund vor dem Laden gemeldet, danach nicht erzeugt (vorher {}, nachher {})", before, after));
        checkN(guest->sharedHash() == host->sharedHash(), "Gast: Pruefsumme gleich der des Hosts");
        guest->netRemoveItem(hash);  // doppelt gemeldet (Stand nach dem Wiederverbinden mit derselben Sitzung)
        checkN(guest->sharedHash() == host->sharedHash(), "Gast: doppelte Meldung zaehlt nicht doppelt");
    }
    // b) ueber das Netz: der Host hat schon etwas aufgenommen und abgelegt, dann tritt ein zweiter Gast bei; er bekommt den
    //    Stand mit dem Willkommen (Ereignisse werden gehalten, bis seine Sitzung steht)
    Game g;
    if (!g.start("level1", 4242)) return;
    Bot& a = g.addBot("Frueh");
    g.run(20.0, [&] { return g.allPlaying(); });
    checkN(g.allPlaying(), "erster Gast spielt");
    a.idle = true;
    g.hostWalks = false;
    g.run(1.0);
    u32 hash = 0;
    vec3 at{};
    double best = 1e9;
    const game::Player& hp = g.hs->player();
    g.hs->registry().each<game::Pickup, game::Transform>([&](ecs::Entity, game::Pickup& pk, game::Transform& tr) {
        const double d = std::hypot(tr.pos.x - hp.x, tr.pos.y - hp.y);
        if (d < best && pk.worldId.rfind("drop:", 0) != 0) best = d, hash = game::worldItemHash(pk.worldId), at = tr.pos;
    });
    game::Pawn probe(3, "Pruefer", gEnv->content.difficulty(g.hs->difficulty), &gEnv->content);
    probe.player = game::Player(at.x + 0.4, at.y, at.z, 3.14159);
    probe.player.pitch = -0.6;
    std::string msg;
    int note = -1;
    checkN(hash && g.hs->pawnInteract(probe, hash, msg, note), "Host nimmt einen Fund auf");
    g.hs->inventory().add("almond");
    for (int i = 0; i < game::Inventory::kSize; ++i)
        if (const auto& sl = g.hs->inventory().slots[(size_t)i]; sl && sl->kind == "almond") {
            g.hs->dropSlot(i, false);
            break;
        }
    g.run(2.0);
    checkN(a.s->sharedHash() == g.hs->sharedHash(), "erster Gast: Stand gleich (aufgenommen und abgelegt)");
    Bot& b = g.addBot("Spaet");
    g.run(20.0, [&] { return g.allPlaying(); });
    g.run(2.0);
    int ghost = 0;
    if (b.s)
        b.s->registry().each<game::Pickup>([&](ecs::Entity, game::Pickup& pk) { ghost += game::worldItemHash(pk.worldId) == hash ? 1 : 0; });
    checkN(b.s && b.s->sharedHash() == g.hs->sharedHash() && ghost == 0,
           std::format("spaeter Gast: Stand gleich, der aufgenommene Fund fehlt auch bei ihm ({} sichtbar)", ghost));
    g.run(3.0);
    checkN(a.c->counters.desync == 0 && b.c->counters.desync == 0, "keine Abweichung bei den Pruefsummen des Hosts");
    g.stopAll();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: coop_test <data-Ordner> [--long Sekunden] [--quick] [--only teil,teil]\n"
                    "  Teile: fuzz bots netem relay direct pickup save full attacks death journey outage hostabort nopath late reconnect netfuzz\n");
        return 2;
    }
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    log::init("coop_test.log");  // Protokoll der Sitzungen und des Koop (im Arbeitsordner)
    static Env env;
    gEnv = &env;
    std::string err;
    if (!env.content.load(argv[1], err)) {
        std::printf("Inhalte nicht ladbar: %s\n", err.c_str());
        return 2;
    }
    env.prefabs.loadDirectory(std::string(argv[1]) + "/prefabs");
    coop::staticNetApi(env.api);
    double longRun = 0;
    bool quick = false;
    std::string only;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--long" && i + 1 < argc) longRun = std::atof(argv[++i]);
        if (std::string(argv[i]) == "--quick") quick = true;
        if (std::string(argv[i]) == "--only" && i + 1 < argc) only = argv[++i];
    }
    Netem lan{20, 5, 0.01, 0.0, 0.0, 0};
    Netem wan{80, 30, 0.03, 0.01, 0.02, 256};
    Netem bad{250, 80, 0.10, 0.02, 0.05, 128};
    Netem relayWan{80, 30, 0.03, 0.01, 0.02, 1024};  // Relay: beide Strecken zum TURN-Server laufen durch den Stoerer
    const double T = quick ? 10.0 : 30.0;
    auto want = [&](const char* n) { return only.empty() || ("," + only + ",").find(std::string(",") + n + ",") != std::string::npos; };
    if (longRun > 0) {
        Netem em{60, 20, 0.02, 0.005, 0.01, 1024};
        testBots(3, longRun, Way::Relay, &em, std::format("Langlauf {:.0f} s ueber das Relay und den Netzstoerer", longRun));
    } else {
        if (want("fuzz")) testFuzz();
        if (want("bots")) {
            testBots(1, quick ? 10 : 25, Way::Lobby, nullptr, "(Lobby, direkt)");
            testBots(3, T, Way::Lobby, nullptr, "(Lobby, direkt, vier Spieler)");
        }
        if (want("netem")) {
            testBots(3, T, Way::LobbyNetem, &lan, "(LAN: 20 ms, 1 % Verlust)");
            testBots(3, T, Way::LobbyNetem, &wan, "(Internet: 80 ms +-30, 3 % Verlust, 1 % doppelt, 2 % umgeordnet, 256 kbit/s)");
            testBots(2, T, Way::LobbyNetem, &bad, "(schlecht: 250 ms +-80, 10 % Verlust, 5 % umgeordnet, 128 kbit/s)");
        }
        if (want("relay")) testBots(3, T, Way::Relay, &relayWan, "(NAT beidseitig, nur Relay: je Strecke 80 ms +-30, 3 % Verlust, 1 % doppelt, 2 % umgeordnet)");
        if (want("direct")) testBots(2, quick ? 10 : 20, Way::Direct, nullptr, "(direkt ohne Lobby, langer Code)");
        if (want("pickup")) testPickupAndPuppets();
        if (want("save")) testCoopSave();
        if (want("full")) testFull();
        if (want("attacks")) testAttacks();
        if (want("death")) testDeath();
        if (want("journey")) testJourney();
        if (want("outage")) testOutage();
        if (want("hostabort")) testHostAbort();
        if (want("nopath")) testNoPath();
        if (want("late")) testLateJoin();
        if (want("reconnect")) testReconnectKick();
        if (want("netfuzz")) testNetFuzz(quick ? 30.0 : 300.0);
    }
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    WSACleanup();
    return gFails ? 1 : 0;
}
