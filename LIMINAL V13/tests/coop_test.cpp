// V13.2: Pruefungen des Koop-Modus mit echten Spielsitzungen ohne Fenster (MULTIPLAYER_TESTS.md).
//   Bots       Host und 1..3 Gaeste in einem Prozess, verbunden ueber echtes UDP (127.0.0.1) - wahlweise ueber einen
//              Netzstoerer (UDP-Proxy: Laufzeit, Schwankung, Verlust, Doppelte, Umordnung, Bandbreite)
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
#include <string>
#include <thread>
#include <vector>

#include "coop/coop.hpp"
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
    explicit UdpProxy(u16 hostPort) : hostPort_(hostPort) {
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
                    h.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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

struct Game {
    std::unique_ptr<coop::CoopHost> host;
    std::unique_ptr<GameSession> hs;
    std::vector<std::unique_ptr<Bot>> bots;
    std::unique_ptr<UdpProxy> proxy;
    game::PlayerControl hostCtl;
    std::mt19937 rng{7};
    double hostTurnT = 0;
    float hostTurn = 0;
    bool hostWalks = true;
    std::string level;

    bool start(const std::string& lvl, long long seed) {
        level = lvl;
        hs = gEnv->make(lvl, seed, "medium", "Host");
        hs->runSeed = seed;
        hs->placeAtSpawn();
        hs->preload();
        host = std::make_unique<coop::CoopHost>(gEnv->api, "Host", kBuild);
        std::string err;
        if (!host->start(0, 4, err)) {
            std::printf("  Host startet nicht: %s\n", err.c_str());
            return false;
        }
        host->attach(*hs, "", gEnv->diffIndex("medium"));
        return true;
    }
    u16 joinPort() const { return proxy ? proxy->port() : host->port(); }

    // ein Bild fuer alle (Echtzeit: ENet misst echte Zeit)
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
        coop::JoinKey k = host->key();
        if (!b->c->connect("127.0.0.1", joinPort(), k, name, err)) std::printf("  verbinden: %s\n", err.c_str());
        bots.push_back(std::move(b));
        return *bots.back();
    }
    // laufen lassen (Echtzeit); cond: frueher fertig
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
static void testJoinKey() {
    std::printf("Beitrittsschluessel\n");
    coop::JoinKey k;
    for (int i = 0; i < 16; ++i) k.b[(size_t)i] = (u8)(i * 37 + 5);
    const std::string t = k.text();
    coop::JoinKey back;
    check(t.size() == 31 && coop::JoinKey::parse(t, back) && back.b == k.b, "Base32 hin und zurueck (" + t + ")");
    std::string lower = t;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    check(coop::JoinKey::parse(lower, back) && back.b == k.b, "Kleinbuchstaben angenommen");
    check(!coop::JoinKey::parse(t.substr(0, 30), back), "zu kurz abgelehnt");
    check(!coop::JoinKey::parse(t + "A", back), "zu lang abgelehnt");
    std::string bad = t;
    bad[0] = '1';
    check(!coop::JoinKey::parse(bad, back), "fremdes Zeichen abgelehnt");
}

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

static void testBots(int nBots, double seconds, const Netem* em, const std::string& label) {
    std::printf("Bots: Host + %d %s\n", nBots, label.c_str());
    Game g;
    if (!g.start("level1", 4242)) {
        check(false, "Host startet");
        return;
    }
    if (em) {
        g.proxy = std::make_unique<UdpProxy>(g.host->port());
        g.proxy->set(*em);
    }
    for (int i = 0; i < nBots; ++i) g.addBot(std::format("Bot{}", i + 1));
    g.run(20.0, [&] { return g.allPlaying(); });
    check(g.allPlaying(), std::format("{}: alle beigetreten", label));
    for (auto& b : g.bots) check(b->c->selfId() >= 1 && b->c->selfId() <= 3, std::format("{}: {} hat Spielernummer {}", label, b->name, b->c->selfId()));
    // Wesen beim Host erzwingen, die Gaeste sehen ihre Puppen
    g.hs->presence().force(game::Presence::Role::Hound, 10.0);
    const auto t0 = std::chrono::steady_clock::now();
    size_t mem0 = memMB();
    g.run(seconds);
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
    check(snaps > (u64)(seconds * 10 * nBots * (em && em->loss > 0.05 ? 0.5 : 0.8)), std::format("{}: Momentaufnahmen angekommen ({})", label, snaps));
    note(std::format("{}: Abweichung der Vorhersage im Mittel {:.3f} m, ueber 10 cm in {} von {} Momentaufnahmen", label,
                     errN ? sumErr / (double)errN : 0.0, over10, errN));
    note(std::format("{}: Korrekturen {}, Spruenge {}, groesste Abweichung {:.2f} m, Puppen {}, Versuche aufzunehmen {}, Host abgelehnt {}/{} Absichten, "
                     "Eingaben verworfen {}, Speicher {} -> {} MB",
                     label, corr, jumps, maxErr, puppets, picks, g.host->counters.actionsRefused, g.host->counters.actions, g.host->counters.inputsRefused,
                     mem0, memMB()));
    if (g.proxy)
        note(std::format("{}: Netz {:.1f} kB/s zum Gast, {:.1f} kB/s zum Host je Gast, verworfen {}", label, g.proxy->bytesDown() / 1024.0 / secs / nBots,
                         g.proxy->bytesUp() / 1024.0 / secs / nBots, g.proxy->dropped()));
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
    if (h) {
        a.s->interact();
        b.s->netAction(1, h);  // Wettlauf: der zweite (weit weg) will dieselbe
    }
    g.run(2.0);
    check(pa->inventory.count("almond") == 1 && a.s->inventory().count("almond") == 1, "Gast hat die Flasche (Host und Gast)");
    game::Pawn* pb = g.host->pawnOf(b.c->selfId());
    check(pb && pb->inventory.count("almond") == 0 && b.s->inventory().count("almond") == 0, "der zweite bekommt sie nicht");
    check(g.hs->itemCount() == hostItems0 && a.s->itemCount() == gastItems0, "aus der Welt verschwunden (Host und Gast)");
    check(a.s->sharedHash() == g.hs->sharedHash() && b.s->sharedHash() == g.hs->sharedHash(), "geteilter Zustand nach dem Aufnehmen gleich");
    // benutzen: der Host wendet es an (Verstand steigt)
    pa->stats.sanity = 40.0;
    int aslot = -1;
    for (int i = 0; i < game::Inventory::kSize; ++i)
        if (a.s->inventory().slots[(size_t)i] && a.s->inventory().slots[(size_t)i]->kind == "almond") aslot = i;
    if (aslot >= 0) a.s->useSlot(aslot);
    g.run(1.5);
    check(pa->stats.sanity > 60.0 && a.s->stats().sanity > 60.0 && a.s->inventory().count("almond") == 0,
          std::format("benutzt: Verstand beim Host {:.0f}, beim Gast {:.0f}", pa->stats.sanity, a.s->stats().sanity));
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
    u64 sent = 0, sessions = 0;
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds) {
        // je Durchgang eine neue Verbindung (nach Trennung durch ungueltige Nachrichten)
        LnContext* e = A.connect("127.0.0.1", g.host->port(), g.host->key().b.data(), "Fuzzer");
        ++sessions;
        bool up = false, closed = false;
        for (int i = 0; i < 300 && !up; ++i) {
            g.frame(kDt);
            LnEvent ev;
            while (A.poll(e, &ev)) up = up || ev.type == LN_EV_CONNECTED;
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
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
            while (A.poll(e, &ev)) closed = closed || ev.type == LN_EV_DISCONNECTED;
            std::this_thread::sleep_for(std::chrono::milliseconds(12));
        }
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
    check(sent > 1000, "genug Nachrichten gesendet");
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
    check(g.allPlaying(), "ehrlicher Gast spielt");
    coop::NetApi& A = gEnv->api;
    auto openEvil = [&](const char* name) {
        LnContext* e = A.connect("127.0.0.1", g.host->port(), g.host->key().b.data(), name);
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
        // Verbindung gilt erst mit der ersten Nachricht des Hosts als offen: Ping als Beitritt-Ersatz geht nicht -
        // deshalb gleich eine Nachricht senden; der Host antwortet erst nach dem Beitritt
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
        // Der Transport meldet CONNECTED erst mit einer Nachricht des Hosts; der Host sendet erst nach Join.
        // Darum: erst Join (gueltig), dann Unfug
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
        check(g.host->counters.rejected >= rej0 + 8, std::format("Host verwirft ungueltige Nachrichten ({})", g.host->counters.rejected - rej0));
        check(closed, "nach mehr als 8 ungueltigen Nachrichten getrennt");
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
        check(p != nullptr, "Speedhack-Gast hat eine Figur");
        if (p) {
            const double x0 = p->player.x, y0 = p->player.y;
            const u64 ref0 = g.host->counters.inputsRefused;
            u32 seq = 1;
            const auto t0 = std::chrono::steady_clock::now();
            while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < 3.0) {
                nc::Msg in;
                in.type = nc::Type::Input;
                in.input.count = 1;
                in.input.f[0] = {seq++, 100, 127, 0, nc::toMrad(p->player.angle), 0, 1};
                sendRaw(e, enc(in), LN_UNRELIABLE);
                g.frame(kDt);
                LnEvent ev;
                while (A.poll(e, &ev)) {}
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            const double moved = std::hypot(p->player.x - x0, p->player.y - y0);
            check(g.host->counters.inputsRefused > ref0 + 50, std::format("zu schnelle Uhr: {} Eingaben verworfen", g.host->counters.inputsRefused - ref0));
            check(moved < 3.0 * 6.5, std::format("in 3 s hoechstens rennend weit ({:.1f} m)", moved));
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
        check(g.host->counters.actionsRefused > refA, std::format("erfundene/ferne Gegenstaende abgelehnt ({})", g.host->counters.actionsRefused - refA));
        check(g.host->counters.dropped > dropA, std::format("Flut von Absichten gedrosselt ({} verworfen)", g.host->counters.dropped - dropA));
        check(g.hs->itemCount() == itemsBefore, "kein Gegenstand verschwunden");
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
        check(closed, "andere Spieldaten: abgewiesen");
        A.close(e);
    }
    g.run(1.0);
    check(honest.c->state() == coop::CoopClient::State::Playing && honest.c->counters.desync == 0, "der ehrliche Gast spielt ungestoert weiter");
    g.stopAll();
}

static void testReconnectKick() {
    std::printf("Wiederverbinden, Entfernen, Sperren\n");
    Game g;
    if (!g.start("level0", 99)) return;
    Bot& b = g.addBot("Zurueck");
    g.run(15.0, [&] { return g.allPlaying(); });
    check(g.allPlaying(), "beigetreten");
    const int id = b.c->selfId();
    game::Pawn* p = g.host->pawnOf(id);
    if (p) p->inventory.add("battery");
    g.run(1.0);
    check(b.s && b.s->inventory().count("battery") == 1, "Inventar vom Host beim Gast");
    // Verbindung verlieren (ohne Abschied), wiederkommen
    coop::CoopClient* c = b.c.get();
    c->stop();
    g.run(1.0);
    b.s.reset();
    std::string err;
    check(c->connect("127.0.0.1", g.host->port(), g.host->key(), "Zurueck", err), "erneut verbinden");
    g.run(10.0, [&] { return g.allPlaying(); });
    check(g.allPlaying() && c->selfId() == id, std::format("gleiche Spielernummer nach dem Wiederverbinden ({} / {})", c->selfId(), id));
    g.run(1.0);
    check(b.s && b.s->inventory().count("battery") == 1, "Inventar nach dem Wiederverbinden zurueck");
    // entfernen
    check(g.host->kick(id, false), "Host entfernt den Gast");
    g.run(2.0, [&] { return c->state() == coop::CoopClient::State::Closed; });
    check(c->state() == coop::CoopClient::State::Closed, "Gast getrennt: " + c->closeReason());
    // sperren: zweiter Gast, gesperrt, versucht es erneut
    Bot& b2 = g.addBot("Gesperrt");
    g.run(10.0, [&] { return b2.c->state() == coop::CoopClient::State::Playing; });
    check(g.host->kick(b2.c->selfId(), true), "Host sperrt den Gast");
    g.run(2.0);
    b2.s.reset();
    check(b2.c->connect("127.0.0.1", g.host->port(), g.host->key(), "Gesperrt", err), "gesperrter Gast versucht es erneut");
    g.run(4.0, [&] { return b2.c->state() == coop::CoopClient::State::Closed; });
    check(b2.c->state() != coop::CoopClient::State::Playing, "gesperrt: kein Beitritt (" + b2.c->closeReason() + ")");
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
    g.run(21.0, [&] { return p->alive && !b.c->dead(); });
    check(p->alive && !b.c->dead() && !b.s->stats().dead, "nach 20 s am Sicherungspunkt zurueck");
    // alle tot: die Gruppe beginnt neu
    p->stats.health = 0.0;
    g.hs->stats().health = 0.0;
    g.hs->stats().dead = true;
    g.run(9.0, [&] { return p->alive && !g.hs->stats().dead; });
    check(p->alive && !g.hs->stats().dead, "alle tot: Neubeginn am Sicherungspunkt");
    g.stopAll();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: coop_test <data-Ordner> [--long Sekunden] [--quick]\n");
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
    if (!only.empty()) {
        if (only == "attacks") testAttacks();
        else if (only == "death") testDeath();
        else if (only == "journey") testJourney();
        else if (only == "reconnect") testReconnectKick();
        else if (only == "full") testFull();
        else if (only == "pickup") testPickupAndPuppets();
        else if (only == "save") testCoopSave();
        else if (only == "netfuzz") testNetFuzz(quick ? 30.0 : 300.0);
        else if (only == "fuzz") testFuzz();
        else if (only == "bots") testBots(3, quick ? 10 : 30, nullptr, "(direkt, vier Spieler)");
    } else if (longRun > 0) {
        Netem em{60, 20, 0.02, 0.005, 0.01, 512};
        testBots(3, longRun, &em, std::format("Langlauf {:.0f} s ueber den Netzstoerer", longRun));
    } else {
        testJoinKey();
        testFuzz();
        testBots(1, quick ? 10 : 25, nullptr, "(direkt)");
        testBots(3, quick ? 10 : 30, nullptr, "(direkt, vier Spieler)");
        Netem lan{20, 5, 0.01, 0.0, 0.0, 0};
        Netem wan{80, 30, 0.03, 0.01, 0.02, 256};
        Netem bad{250, 80, 0.10, 0.02, 0.05, 128};
        testBots(3, quick ? 10 : 30, &lan, "(LAN: 20 ms, 1 % Verlust)");
        testBots(3, quick ? 10 : 30, &wan, "(Internet: 80 ms +-30, 3 % Verlust, 1 % doppelt, 2 % umgeordnet, 256 kbit/s)");
        testBots(2, quick ? 10 : 30, &bad, "(schlecht: 250 ms +-80, 10 % Verlust, 5 % umgeordnet, 128 kbit/s)");
        testPickupAndPuppets();
        testCoopSave();
        testFull();
        testAttacks();
        testDeath();
        testJourney();
        testReconnectKick();  // zuletzt: sperrt 127.0.0.1
    }
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    WSACleanup();
    return gFails ? 1 : 0;
}
