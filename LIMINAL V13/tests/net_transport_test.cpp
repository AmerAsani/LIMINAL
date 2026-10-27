// V13.3: Prueft den Transport des Koop-Modus (liminal_net: WebRTC-Datenkanaele ueber libdatachannel, Lobby,
// Beitrittscodes) ueber echte Verbindungen auf 127.0.0.1 (MULTIPLAYER_TESTS.md, Abschnitt 6).
//   - Codes, Host-Zertifikat, Lobby-Nachrichten (Gegentests, Fuzzing)
//   - Beitritt ueber die Lobby: kurzer und langer Code, beide Kanaele, Hoechstgroesse, Reihenfolge, Trennung
//   - Wege: direkt, nur Relay (eigene IP verborgen), "NAT": direkte Kandidaten verworfen -> Relay, gar kein Weg
//   - Direkt ohne Lobby (eingebaute Lobby im Host, langer Code)
//   - Lobby mit TLS: richtiges Zertifikat, falsches Zertifikat, unbekannte Zertifizierungsstelle
//   - Angriffe: falscher Code, falscher Pruefteil, Sperre nach Fehlversuchen, voll, Bann, Muell und Fluten gegen den
//     Host (Gast mit richtigem Code), Angriffe und Fuzzing gegen die Lobby
#include <winsock2.h>
#include <windows.h>

#include <rtc/rtc.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "netdll/hostcert.hpp"
#include "netdll/joincode.hpp"
#include "netdll/ln_api.h"
#include "netdll/lobby_proto.hpp"
#include "netdll/lobby_server.hpp"

using namespace lim::netc;
using u8 = std::uint8_t;
using Clock = std::chrono::steady_clock;

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}
static void note(const std::string& s) { std::printf("  %s\n", s.c_str()); }
static double secSince(Clock::time_point t) { return std::chrono::duration<double>(Clock::now() - t).count(); }
static const LnLimits kLim{3, 4, 120.0, 60.0, 8, 20000, 10000, 5, 60000};

// --- Ereignisse einer DLL-Sitzung --------------------------------------------------------------------------
struct Msg {
    int peer, channel;
    std::vector<u8> data;
};
struct Ctx {
    LnContext* c = nullptr;
    std::string code;
    int connected = 0, disconnected = 0, lastReason = -1, lastPeer = -2, progress = 0, rooms = 0;
    std::vector<int> peers;
    std::vector<std::string> names;
    std::vector<Msg> msgs;
    Clock::time_point t0 = Clock::now(), tConnected{};
    void poll() {
        if (!c) return;
        LnEvent e;
        while (ln_poll(c, &e)) {
            switch (e.type) {
                case LN_EV_CONNECTED:
                    ++connected, peers.push_back(e.peer), names.push_back(e.name);
                    if (connected == 1) tConnected = Clock::now();
                    break;
                case LN_EV_DISCONNECTED: ++disconnected, lastReason = e.reason, lastPeer = e.peer; break;
                case LN_EV_MESSAGE: msgs.push_back({e.peer, e.channel, std::vector<u8>(e.data, e.data + e.len)}); break;
                case LN_EV_ROOM:
                    ++rooms;
                    code = std::string((const char*)e.data, (size_t)e.len);
                    break;
                case LN_EV_PROGRESS: progress = e.reason; break;
            }
        }
    }
    void close() {
        if (c) ln_close(c);
        c = nullptr;
    }
    std::string longCode() const {
        char b[LN_CODE_MAX];
        return c && ln_code(c, 1, b, sizeof(b)) ? std::string(b) : std::string();
    }
    double connectMs() const { return std::chrono::duration<double, std::milli>(tConnected - t0).count(); }
};
static bool pump(std::vector<Ctx*> cs, double seconds, const std::function<bool()>& cond) {
    const auto t0 = Clock::now();
    while (secSince(t0) < seconds) {
        for (Ctx* c : cs) c->poll();
        if (cond && cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    for (Ctx* c : cs) c->poll();
    return cond ? cond() : true;
}

// --- Lobby im Test ------------------------------------------------------------------------------------------
struct TestLobby {
    LobbyServer s;
    LobbyConfig cfg;
    std::string url;
    bool start(bool turn = true) {
        cfg.bind = "127.0.0.1";
        cfg.port = 0;
        cfg.turn = turn;
        cfg.turnPort = 0;
        cfg.publicHost = "127.0.0.1";
        std::string err;
        if (!s.start(cfg, err)) {
            std::printf("  Lobby startet nicht: %s\n", err.c_str());
            return false;
        }
        url = std::string(cfg.tlsCert.empty() ? "ws" : "wss") + "://127.0.0.1:" + std::to_string(s.port());
        return true;
    }
};
struct Cfg {
    std::string lobby, ca, ice, bind = "127.0.0.1";
    uint16_t port = 0;
    int relayOnly = 0;
    LnConfig get() const { return LnConfig{lobby.c_str(), ca.c_str(), ice.c_str(), bind.c_str(), port, relayOnly}; }
};
static Ctx host(const Cfg& cfg, LnLimits lim = kLim) {
    Ctx h;
    const LnConfig c = cfg.get();
    h.c = ln_host(&c, &lim);
    return h;
}
static Ctx join(const Cfg& cfg, const std::string& code, const std::string& name, const char* direct = nullptr) {
    Ctx g;
    const LnConfig c = cfg.get();
    g.c = ln_join(&c, direct, code.c_str(), name.c_str());
    return g;
}
static bool waitRoom(Ctx& h) { return pump({&h}, 10.0, [&] { return !h.code.empty() || h.disconnected; }) && !h.code.empty(); }

// --- Gast, der das Protokoll nach der Lobby selbst spricht (Angriffe auf den Host) -------------------------------
struct Rogue {
    std::shared_ptr<rtc::WebSocket> ws;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> r, u;
    std::mutex m;
    std::atomic<bool> rOpen{false}, uOpen{false}, closed{false};
    std::atomic<int> received{0};
    std::vector<std::vector<u8>> frames;
    std::string room, name;
    bool start(const std::string& url, const std::string& room_, const std::string& name_) {
        room = room_, name = name_;
        ws = std::make_shared<rtc::WebSocket>();
        ws->onOpen([this] {
            LMsg j;
            j.t = LType::Join;
            j.room = room, j.name = name;
            ws->send(build(j));
        });
        ws->onMessage([this](rtc::message_variant v) {
            if (!std::holds_alternative<std::string>(v)) return;
            LMsg mm;
            if (!parseFromLobby(std::get<std::string>(v), mm)) return;
            std::lock_guard<std::mutex> l(m);
            if (mm.t == LType::Joined && !pc) {
                rtc::Configuration rc;
                rc.bindAddress = "127.0.0.1";
                rc.maxMessageSize = 65536;
                pc = std::make_shared<rtc::PeerConnection>(rc);
                pc->onLocalDescription([this](rtc::Description d) {
                    LMsg s;
                    s.t = LType::Sig, s.kind = "answer", s.sdp = std::string(d);
                    ws->send(build(s));
                });
                pc->onLocalCandidate([this](rtc::Candidate c) {
                    LMsg s;
                    s.t = LType::Cand, s.cand = c.candidate(), s.mid = c.mid();
                    ws->send(build(s));
                });
                pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
                    auto& slot = dc->label() == "r" ? r : u;
                    auto& flag = dc->label() == "r" ? rOpen : uOpen;
                    slot = dc;
                    dc->onOpen([&flag] { flag = true; });
                    dc->onClosed([this] { closed = true; });
                    dc->onMessage([this](rtc::message_variant mv) {
                        if (auto* b = std::get_if<rtc::binary>(&mv)) {
                            std::lock_guard<std::mutex> l2(m);
                            frames.emplace_back((const u8*)b->data(), (const u8*)b->data() + b->size());
                        }
                        ++received;
                    });
                    if (dc->isOpen()) flag = true;
                });
            } else if (mm.t == LType::Sig && pc) {
                pc->setRemoteDescription(rtc::Description(mm.sdp, mm.kind));
            } else if (mm.t == LType::Cand && pc) {
                pc->addRemoteCandidate(rtc::Candidate(mm.cand, mm.mid));
            }
        });
        ws->open(url);
        return true;
    }
    bool open() const { return rOpen && uOpen; }
    void send(bool reliable, const std::vector<u8>& d) {
        try {
            (reliable ? r : u)->send(reinterpret_cast<const std::byte*>(d.data()), d.size());
        } catch (...) {
        }
    }
    void sendText(const std::string& s) {
        try {
            r->send(s);
        } catch (...) {
        }
    }
    static std::vector<u8> hello(const std::string& secret, const std::string& name, int ver = LN_VERSION) {
        std::vector<u8> h = {1, 'L', 'I', 'M', 'N', (u8)(ver & 0xFF), (u8)(ver >> 8)};
        h.insert(h.end(), secret.begin(), secret.end());
        h.push_back((u8)name.size());
        h.insert(h.end(), name.begin(), name.end());
        return h;
    }
    bool hasFrame(const std::vector<u8>& f) {
        std::lock_guard<std::mutex> l(m);
        for (auto& x : frames)
            if (x == f) return true;
        return false;
    }
    void stop() {
        try {
            if (pc) pc->close();
            if (ws) ws->close();
        } catch (...) {
        }
    }
};

// =============================================================================================
static void testCodes() {
    std::printf("Beitrittscodes und Zertifikat\n");
    JoinCode c;
    check(parseCode("ab12-cd34-ef56", c) && c.room == "AB12CD" && c.secret == "34EF56" && !c.hasPin(), "kurzer Code, Kleinbuchstaben");
    check(parseCode("AB12 CD34 EF56 GH78 JK9M NPQR", c) && c.hasPin() && c.pin == "GH78JK9MNPQR", "langer Code mit Leerzeichen");
    check(parseCode("oI1L-0000-0000", c) && c.room == "011100", "O = 0, I und L = 1");
    check(!parseCode("AB12-CD34-EF5", c), "zu kurz");
    check(!parseCode("AB12-CD34-EF56-G", c), "Laenge zwischen kurz und lang");
    check(!parseCode("AB12-CD34-EF5U", c), "U gehoert nicht zum Alphabet");
    check(!parseCode("AB12-CD34-EF5*", c), "fremdes Zeichen");
    check(!parseCode(std::string(100, 'A'), c), "ueberlang");
    check(JoinCode{"AB12CD", "34EF56", "GH78JK9MNPQR"}.text(true) == "AB12-CD34-EF56-GH78-JK9M-NPQR", "Text in Vierergruppen");
    HostCert hc;
    std::string err;
    check(makeHostCert(hc, err), "Host-Zertifikat erzeugt");
    check(fingerprintOfPem(hc.certPem) == hc.fingerprint && hc.fingerprint.size() == 95, "Fingerabdruck aus PEM gleich dem beim Erzeugen");
    const std::string pin = pinFromFingerprint(hc.fingerprint);
    check(pin.size() == 12, "Pruefteil: 12 Zeichen");
    check(pinFromFingerprint("00:11") == "" && pinFromFingerprint(std::string(95, 'Z')) == "", "Pruefteil: falsches Format -> leer");
    // bekannter Wert: Fingerabdruck 0x80 00 ... -> erstes Zeichen 16 ('G'), Rest 0
    std::string fp = "80";
    for (int i = 1; i < 32; ++i) fp += ":00";
    check(pinFromFingerprint(fp) == "G00000000000", "Pruefteil: bekannte Bits");
    HostCert hc2;
    makeHostCert(hc2, err);
    check(hc2.fingerprint != hc.fingerprint, "jedes Zertifikat hat einen eigenen Fingerabdruck");
    std::string r1, r2;
    check(randomChars(6, r1) && randomChars(6, r2) && r1 != r2 && roomOk(r1), "Zufallszeichen");
    check(equalConst("ABC", "ABC") && !equalConst("ABC", "ABD") && !equalConst("AB", "ABC"), "Vergleich in fester Zeit");
}

static void testLobbyProto() {
    std::printf("Lobby-Nachrichten (Pruefschicht)\n");
    LMsg m, o;
    m.t = LType::Join, m.room = "AB12CD", m.name = "Gast 1";
    std::string s = build(m);
    check(parseFromClient(s, false, o) && o.t == LType::Join && o.room == "AB12CD" && o.name == "Gast 1", "join hin und zurueck");
    check(!parseFromClient(s, true, o) || o.t == LType::Join, "join auch vom Host lesbar (Rolle prueft die Lobby)");
    LMsg sig;
    sig.t = LType::Sig, sig.kind = "offer", sig.sdp = "v=0\r\no=- 1 1 IN IP4 0.0.0.0\r\n", sig.to = 3;
    check(parseFromClient(build(sig), true, o) && o.to == 3 && o.sdp == sig.sdp, "sig vom Host mit Ziel");
    check(!parseFromClient(build(sig), false, o), "sig vom Gast mit Ziel abgelehnt");
    LMsg room;
    room.t = LType::Room, room.room = "XY34ZW", room.ttl = 3600;
    room.ice = {{false, "127.0.0.1", 3478, "", ""}, {true, "", 3478, "u_1", "p-2"}};
    check(parseFromLobby(build(room), o) && o.ice.size() == 2 && o.ice[1].turn && o.ice[1].user == "u_1" && o.ice[1].host.empty(), "room mit STUN/TURN");
    const char* bad[] = {
        "", "x", "[]", "{}", "{\"t\":5}", "{\"t\":\"nope\"}", "{\"t\":\"join\",\"v\":1,\"room\":\"AB12C\",\"name\":\"a\"}",
        "{\"t\":\"join\",\"v\":1,\"room\":\"AB12CU\",\"name\":\"a\"}", "{\"t\":\"join\",\"v\":1,\"room\":\"AB12CD\",\"name\":\"\"}",
        "{\"t\":\"join\",\"v\":1,\"room\":\"AB12CD\",\"name\":\"abcdefghijklmnopq\"}", "{\"t\":\"join\",\"v\":1,\"room\":\"AB12CD\",\"name\":\"a\\u0001\"}",
        "{\"t\":\"join\",\"v\":\"1\",\"room\":\"AB12CD\",\"name\":\"a\"}", "{\"t\":\"host\",\"v\":1,\"max\":99}",
        "{\"t\":\"cand\",\"c\":\"evil\",\"mid\":\"0\"}", "{\"t\":\"cand\",\"c\":\"candidate:1 1 UDP 1 1.2.3.4 5 typ host\",\"mid\":\"0 1\"}",
        "{\"t\":\"sig\",\"kind\":\"offer\",\"sdp\":\"v=0\\r\\n\\u0000\"}", "{\"t\":\"sig\",\"kind\":\"pranswer\",\"sdp\":\"v=0\\r\\no=x\"}",
        "{\"t\":\"refuse\",\"id\":1,\"e\":\"full\"}", "{\"t\":\"ban\",\"id\":1}", "{\"t\":\"room\",\"room\":\"AB12CD\",\"ttl\":1}",
        "[[[[[[[[[[1]]]]]]]]]]", "{\"t\":\"join\",\"a\":[[[[[[1]]]]]]}"};
    int rej = 0;
    for (const char* b : bad) rej += parseFromClient(b, false, o) ? 0 : 1;
    check(rej == (int)(sizeof(bad) / sizeof(bad[0])), "Gegentests vom Gast abgelehnt (" + std::to_string(rej) + "/" + std::to_string(sizeof(bad) / sizeof(bad[0])) + ")");
    check(!parseFromClient(std::string(kMaxLobbyMsg + 1, ' '), true, o), "ueber 8 KiB abgelehnt");
    check(!parseFromLobby("{\"t\":\"err\",\"e\":\"anything\"}", o) && parseFromLobby("{\"t\":\"err\",\"e\":\"noroom\"}", o), "Fehlercodes nur aus der Liste");
    check(!parseFromLobby("{\"t\":\"room\",\"room\":\"AB12CD\",\"ttl\":1,\"ice\":[{\"type\":\"turn\",\"host\":\"h\",\"port\":1,\"user\":\"a b\",\"pass\":\"x\"}]}", o),
          "TURN-Zugang mit Leerzeichen abgelehnt");
    // Fuzzing: veraenderte gueltige Nachrichten und Zufall; angenommene Nachrichten muessen sich kanonisch neu bauen lassen
    std::vector<std::string> seeds = {build(m), build(sig), build(room)};
    LMsg cand;
    cand.t = LType::Cand, cand.cand = "candidate:1 1 UDP 2122317823 127.0.0.1 50000 typ host", cand.mid = "0";
    seeds.push_back(build(cand));
    LMsg host;
    host.t = LType::Host, host.max = 3;
    seeds.push_back(build(host));
    std::mt19937 rng(4242);
    int accepted = 0, canonical = 0;
    const int N = 300000;
    for (int i = 0; i < N; ++i) {
        std::string x = seeds[rng() % seeds.size()];
        const int kind = (int)(rng() % 4);
        if (kind == 0) x = std::string(rng() % 300, '\0'), std::generate(x.begin(), x.end(), [&] { return (char)(rng() & 0xFF); });
        else {
            const int muts = 1 + (int)(rng() % 6);
            for (int k = 0; k < muts && !x.empty(); ++k) {
                const size_t p = rng() % x.size();
                switch (rng() % 4) {
                    case 0: x[p] = (char)(rng() & 0xFF); break;
                    case 1: x.erase(p, 1 + rng() % 5); break;
                    case 2: x.insert(p, 1, "{}[]\":,0a\\"[rng() % 10]); break;
                    default: x = x.substr(0, p); break;
                }
            }
        }
        LMsg a, b2;
        const bool okc = parseFromClient(x, (rng() & 1) != 0, a);
        const bool okl = parseFromLobby(x, b2);
        if (okc) {
            ++accepted;
            LMsg again;
            canonical += parseFromClient(build(a), true, again) || parseFromClient(build(a), false, again) ? 1 : 0;
        }
        if (okl) {
            ++accepted;
            LMsg again;
            canonical += parseFromLobby(build(b2), again) ? 1 : 0;
        }
    }
    check(accepted == canonical, "Fuzzing: jede angenommene Nachricht laesst sich neu bauen und wieder lesen");
    note(std::to_string(N) + " Faelle, " + std::to_string(accepted) + " angenommen, kein Absturz");
}

// =============================================================================================
static void testLobbyJoin() {
    std::printf("Beitritt ueber die Lobby\n");
    TestLobby L;
    if (!L.start()) return check(false, "Lobby");
    Cfg cfg;
    cfg.lobby = L.url;
    Ctx h = host(cfg);
    check(h.c != nullptr, "Host oeffnet");
    check(waitRoom(h), "Raum offen, Code: " + h.code);
    const std::string longCode = h.longCode();
    check(longCode.size() == 29 && longCode.rfind(h.code, 0) == 0, "langer Code beginnt mit dem kurzen");
    Ctx g = join(cfg, h.code, "Gast Eins");
    const bool ok = pump({&h, &g}, 15.0, [&] { return h.connected && g.connected; });
    check(ok, "Gast verbunden (kurzer Code)");
    if (!ok) {
        note("Gast: Grund " + std::to_string(g.lastReason) + ", Fortschritt " + std::to_string(g.progress));
        return;
    }
    note("verbunden nach " + std::to_string((int)g.connectMs()) + " ms");
    check(h.names.size() == 1 && h.names[0] == "Gast Eins", "Host kennt den Namen");
    const int hp = h.peers[0];
    // Nachrichten in beide Richtungen, beide Kanaele
    std::vector<u8> big(1200, 0x5A), tooBig(1201, 1);
    check(ln_send(g.c, 0, LN_RELIABLE, big.data(), 1200) == 1, "1200 Byte angenommen");
    check(ln_send(g.c, 0, LN_RELIABLE, tooBig.data(), 1201) == 0, "1201 Byte abgelehnt");
    for (int i = 0; i < 200; ++i) {
        u8 b[4] = {(u8)i, (u8)(i >> 8), 7, 7};
        ln_send(h.c, hp, LN_RELIABLE, b, 4);
    }
    const u8 un[3] = {9, 9, 9};
    for (int i = 0; i < 20; ++i) ln_send(h.c, hp, LN_UNRELIABLE, un, 3), ln_send(g.c, 0, LN_UNRELIABLE, un, 3);
    pump({&h, &g}, 5.0, [&] { return g.msgs.size() >= 210 && h.msgs.size() >= 11; });
    int inOrder = 0, unrel = 0;
    for (const Msg& m : g.msgs) {
        if (m.channel == LN_RELIABLE && m.data.size() == 4 && m.data[0] + 256 * m.data[1] == inOrder) ++inOrder;
        if (m.channel == LN_UNRELIABLE) ++unrel;
    }
    check(inOrder == 200, "200 zuverlaessige Nachrichten vollstaendig und geordnet (" + std::to_string(inOrder) + ")");
    check(unrel >= 15, "unzuverlaessiger Kanal kommt an (" + std::to_string(unrel) + "/20)");
    check(!h.msgs.empty() && h.msgs[0].data.size() == 1200 && h.msgs[0].peer == hp, "Host bekommt 1200 Byte vom richtigen Gast");
    pump({&h, &g}, 2.0, nullptr);  // Pings
    LnStats st{};
    check(ln_stats(g.c, 0, &st) == 1 && st.path == LN_PATH_DIRECT, "Weg: direkt (" + std::to_string(st.path) + ")");
    note("Ping " + std::to_string(st.rttMs) + " ms, Verlust " + std::to_string((int)(st.loss * 100)) + " %");
    // ein zweiter Gast mit dem langen Code
    Ctx g2 = join(cfg, longCode, "Zwei");
    check(pump({&h, &g2}, 15.0, [&] { return g2.connected || g2.disconnected; }) && g2.connected, "zweiter Gast mit langem Code (Pruefteil passt)");
    // Trennen: der Host merkt es
    g.close();
    check(pump({&h}, 15.0, [&] { return h.disconnected > 0; }) && h.lastPeer == hp, "Host meldet das Gehen des Gasts");
    g2.close();
    h.close();
    // Host weg: neuer Gast mit altem Code bekommt "Code unbekannt"
    Ctx g3 = join(cfg, longCode, "Drei");
    check(pump({&g3}, 10.0, [&] { return g3.disconnected > 0; }) && g3.lastReason == LN_R_NOROOM, "nach dem Host: Code unbekannt");
    g3.close();
    L.s.stop();
}

static void testWrongCodes() {
    std::printf("Falsche Codes, Sperre, voll, Bann\n");
    TestLobby L;
    if (!L.start()) return check(false, "Lobby");
    Cfg cfg;
    cfg.lobby = L.url;
    LnLimits lim = kLim;
    lim.maxPeers = 1;
    Ctx h = host(cfg, lim);
    if (!waitRoom(h)) return check(false, "Raum");
    JoinCode jc;
    parseCode(h.longCode(), jc);
    auto tryJoin = [&](const std::string& code, const std::string& name) {
        Ctx g = join(cfg, code, name);
        pump({&h, &g}, 15.0, [&] { return g.connected || g.disconnected; });
        const int r = g.connected ? 0 : g.lastReason;
        g.close();
        pump({&h}, 0.3, nullptr);
        return r;
    };
    check(tryJoin("AB12-CD34", "x") == LN_R_BADCODE, "Format falsch -> BADCODE");
    std::string other = jc.room == "000000" ? "000001" : "000000";
    check(tryJoin(other + jc.secret, "x") == LN_R_NOROOM, "unbekannter Raum -> NOROOM");
    JoinCode wrongPin = jc;
    wrongPin.pin[0] = wrongPin.pin[0] == '0' ? '1' : '0';
    check(tryJoin(wrongPin.text(true), "x") == LN_R_CERT, "falscher Pruefteil -> CERT (nichts gesendet)");
    JoinCode wrongSecret = jc;
    wrongSecret.secret[0] = wrongSecret.secret[0] == '0' ? '1' : '0';
    int badKey = 0;
    for (int i = 0; i < 4; ++i) badKey += tryJoin(wrongSecret.text(false), "x") == LN_R_BADKEY ? 1 : 0;
    check(badKey == 4, "falsches Geheimteil -> BADKEY (" + std::to_string(badKey) + "/4)");
    check(tryJoin(jc.text(false), "Ehrlich") == 0, "danach mit richtigem Code: verbunden (4 Fehlversuche < 5)");
    pump({&h}, 0.5, nullptr);
    Ctx keep = join(cfg, jc.text(false), "Bleibt");
    pump({&h, &keep}, 15.0, [&] { return keep.connected || keep.disconnected; });
    check(keep.connected, "Gast bleibt verbunden");
    check(tryJoin(jc.text(false), "Vierter") == LN_R_FULL, "voll -> FULL");
    // 5. Fehlversuch: die Adresse ist gesperrt, auch mit richtigem Code
    tryJoin(wrongSecret.text(false), "x");
    keep.close();
    pump({&h}, 3.0, [&] { return h.disconnected > 0; });
    check(tryJoin(jc.text(false), "Ehrlich") == LN_R_BANNED, "nach 5 Fehlversuchen gesperrt (auch mit richtigem Code)");
    h.close();
    // Bann durch den Host
    Ctx h2 = host(cfg);
    if (!waitRoom(h2)) return check(false, "Raum 2");
    Ctx g = join(cfg, h2.code, "Stoerer");
    pump({&h2, &g}, 15.0, [&] { return h2.connected && g.connected; });
    check(g.connected, "Gast verbunden");
    if (!h2.peers.empty()) ln_drop(h2.c, h2.peers[0], LN_R_BANNED, 1);
    check(pump({&g}, 10.0, [&] { return g.disconnected > 0; }) && g.lastReason == LN_R_BANNED, "Gast erfaehrt: gesperrt");
    g.close();
    Ctx again = join(cfg, h2.code, "Stoerer");
    check(pump({&h2, &again}, 10.0, [&] { return again.connected || again.disconnected; }) && again.lastReason == LN_R_BANNED,
          "erneuter Versuch: schon die Lobby weist ab");
    again.close();
    h2.close();
    L.s.stop();
}

static void testPaths() {
    std::printf("Wege: Relay, NAT (nur Relay bleibt), kein Weg\n");
    {
        TestLobby L;
        if (!L.start()) return check(false, "Lobby");
        Cfg cfg;
        cfg.lobby = L.url;
        cfg.relayOnly = 1;
        Ctx h = host(cfg);
        waitRoom(h);
        Ctx g = join(cfg, h.code, "Verborgen");
        check(pump({&h, &g}, 20.0, [&] { return h.connected && g.connected; }), "nur Relay (beide verbergen die IP): verbunden");
        pump({&h, &g}, 1.0, nullptr);
        LnStats st{};
        ln_stats(g.c, 0, &st);
        check(st.path == LN_PATH_RELAY, "Weg: Relay (" + std::to_string(st.path) + ")");
        note("verbunden nach " + std::to_string((int)g.connectMs()) + " ms");
        g.close(), h.close();
        L.s.stop();
    }
    {  // NAT: die Lobby verwirft alle Kandidaten ausser Relay -> ICE muss ueber TURN gehen
        TestLobby L;
        std::atomic<int> dropped{0};
        L.cfg.candidateHook = [&](std::string& c, bool) {
            if (c.find(" typ relay") != std::string::npos) return true;
            ++dropped;
            return false;
        };
        if (!L.start()) return check(false, "Lobby");
        Cfg cfg;
        cfg.lobby = L.url;
        Ctx h = host(cfg);
        waitRoom(h);
        Ctx g = join(cfg, h.code, "HinterNAT");
        check(pump({&h, &g}, 20.0, [&] { return h.connected && g.connected; }), "direkte Wege gesperrt: ueber Relay verbunden");
        pump({&h, &g}, 1.0, nullptr);
        LnStats st{};
        ln_stats(g.c, 0, &st);
        check(st.path == LN_PATH_RELAY && dropped > 0, "Weg: Relay, " + std::to_string(dropped.load()) + " direkte Kandidaten verworfen");
        g.close(), h.close();
        L.s.stop();
    }
    {  // kein Weg: kein Relay und alle Kandidaten verworfen -> klare Meldung
        TestLobby L;
        L.cfg.candidateHook = [](std::string&, bool) { return false; };
        if (!L.start(false)) return check(false, "Lobby");
        Cfg cfg;
        cfg.lobby = L.url;
        Ctx h = host(cfg);
        waitRoom(h);
        Ctx g = join(cfg, h.code, "Abgeschnitten");
        const auto t0 = Clock::now();
        pump({&h, &g}, 40.0, [&] { return g.connected || g.disconnected; });
        check(!g.connected && g.lastReason == LN_R_ICE, "kein Weg: Grund ICE (" + std::to_string(g.lastReason) + ") nach " + std::to_string((int)secSince(t0)) + " s");
        check(h.connected == 0, "Host hat keinen halben Gast angenommen");
        g.close(), h.close();
        L.s.stop();
    }
}

static uint16_t freePort(uint16_t base) {
    for (uint16_t p = base; p < base + 200; ++p) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, 0), u = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons(p);
        const bool ok = bind(s, (sockaddr*)&a, sizeof(a)) == 0 && bind(u, (sockaddr*)&a, sizeof(a)) == 0;
        closesocket(s), closesocket(u);
        if (ok) return p;
    }
    return 0;
}

static void testDirect() {
    std::printf("Direkt ohne Lobby (eingebaute Lobby, langer Code)\n");
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
    Cfg cfg;
    cfg.port = freePort(37311);
    Ctx h = host(cfg);
    check(h.c && ln_port(h.c) == cfg.port, "Host lauscht auf Port " + std::to_string(cfg.port));
    check(waitRoom(h), "eingebaute Lobby gibt einen Code");
    const std::string addr = "127.0.0.1:" + std::to_string(cfg.port);
    Ctx bad = join(cfg, h.code, "Kurz", addr.c_str());
    check(pump({&bad}, 3.0, [&] { return bad.disconnected > 0; }) && bad.lastReason == LN_R_BADCODE, "direkt mit kurzem Code abgelehnt (Pruefteil Pflicht)");
    bad.close();
    Ctx g = join(cfg, h.longCode(), "Direkt", addr.c_str());
    check(pump({&h, &g}, 15.0, [&] { return h.connected && g.connected; }), "direkt verbunden");
    pump({&h, &g}, 1.0, nullptr);
    LnStats st{};
    ln_stats(g.c, 0, &st);
    check(st.path == LN_PATH_DIRECT, "Weg: direkt");
    u8 b[3] = {1, 2, 3};
    ln_send(g.c, 0, LN_RELIABLE, b, 3);
    check(pump({&h, &g}, 3.0, [&] { return !h.msgs.empty(); }), "Nachricht kommt an");
    // ein zweiter "Host" an der eingebauten Lobby wird abgewiesen
    auto ws = std::make_shared<rtc::WebSocket>();
    std::atomic<bool> denied{false};
    ws->onOpen([ws] {
        LMsg m;
        m.t = LType::Host, m.max = 3;
        ws->send(build(m));
    });
    ws->onMessage([&](rtc::message_variant v) {
        if (auto* s = std::get_if<std::string>(&v); s && s->find("denied") != std::string::npos) denied = true;
    });
    ws->open("ws://" + addr);
    pump({&h, &g}, 5.0, [&] { return denied.load(); });
    check(denied, "zweiter Raum an der eingebauten Lobby abgewiesen");
    ws->close();
    g.close(), h.close();
}

static void testTls() {
    std::printf("Lobby mit TLS (wss)\n");
    HostCert server, other;
    std::string err;
    makeHostCert(server, err, "CN=localhost");
    makeHostCert(other, err, "CN=localhost");
    TestLobby L;
    L.cfg.tlsCert = server.certPem, L.cfg.tlsKey = server.keyPem;
    if (!L.start()) return check(false, "Lobby mit TLS");
    const std::string url = "wss://localhost:" + std::to_string(L.s.port());
    auto tryHost = [&](const std::string& ca) {
        Cfg cfg;
        cfg.lobby = url, cfg.ca = ca;
        Ctx h = host(cfg);
        pump({&h}, 12.0, [&] { return !h.code.empty() || h.disconnected; });
        const bool ok = !h.code.empty();
        h.close();
        return ok;
    };
    check(tryHost(server.certPem), "festgelegtes Zertifikat der Lobby: Raum offen");
    check(!tryHost(other.certPem), "fremdes Zertifikat (gleicher Name): abgewiesen");
    check(!tryHost(""), "selbst signiert gegen die Windows-Stammzertifikate: abgewiesen");
    L.s.stop();
}

static void testHostAttacks() {
    std::printf("Angriffe auf den Host (Gast mit richtigem Raum)\n");
    TestLobby L;
    if (!L.start()) return check(false, "Lobby");
    Cfg cfg;
    cfg.lobby = L.url;
    LnLimits lim = kLim;
    lim.handshakeMs = 4000;
    Ctx h = host(cfg, lim);
    waitRoom(h);
    JoinCode jc;
    parseCode(h.code, jc);
    Ctx honest = join(cfg, h.code, "Ehrlich");
    pump({&h, &honest}, 15.0, [&] { return honest.connected; });
    check(honest.connected, "ehrlicher Gast verbunden");
    auto rogue = [&](const char* what, const std::function<void(Rogue&)>& act, const std::function<bool(Rogue&)>& expect, double wait = 6.0) {
        Rogue r;
        r.start(L.url, jc.room, "Boese");
        pump({&h, &honest}, 15.0, [&] { return r.open(); });
        if (!r.open()) return check(false, std::string(what) + ": Kanaele nicht offen");
        const int before = h.connected;
        act(r);
        pump({&h, &honest}, wait, [&] { return expect(r); });
        check(expect(r) && h.connected == before, what);
        r.stop();
    };
    rogue("Nutzdaten vor dem Hallo -> getrennt", [](Rogue& r) {
        for (int i = 0; i < 12; ++i) r.send(true, {0x10, 1, 2, 3});
    }, [](Rogue& r) { return r.closed.load(); });
    rogue("Text statt Binaer -> getrennt", [](Rogue& r) {
        for (int i = 0; i < 12; ++i) r.sendText("hallo");
    }, [](Rogue& r) { return r.closed.load(); });
    rogue("Uebergroesse (3000 Byte) -> getrennt", [](Rogue& r) {
        for (int i = 0; i < 12; ++i) r.send(true, std::vector<u8>(3000, 0x10));
    }, [](Rogue& r) { return r.closed.load(); });
    rogue("falsche Version im Hallo -> Absage VERSION", [&](Rogue& r) { r.send(true, Rogue::hello(jc.secret, "Boese", 2)); },
          [](Rogue& r) { return r.hasFrame({3, LN_R_VERSION}); });
    rogue("kein Hallo -> Zeitlimit", [](Rogue&) {}, [](Rogue& r) { return r.closed.load(); }, 8.0);
    // Flut nach gueltigem Hallo
    {
        Rogue r;
        r.start(L.url, jc.room, "Flut");
        pump({&h, &honest}, 15.0, [&] { return r.open(); });
        const int before = h.connected;
        r.send(true, Rogue::hello(jc.secret, "Flut"));
        pump({&h, &honest}, 5.0, [&] { return h.connected > before; });
        check(h.connected == before + 1, "Flut-Gast mit Hallo angenommen");
        const size_t msgsBefore = h.msgs.size();
        const auto t0 = Clock::now();
        for (int i = 0; i < 3000; ++i) r.send(false, {0x10, (u8)i});
        pump({&h, &honest}, 2.0, nullptr);
        const double secs = secSince(t0);
        size_t fromFlood = 0;
        for (size_t i = msgsBefore; i < h.msgs.size(); ++i) fromFlood += h.msgs[i].peer == h.peers.back() ? 1 : 0;
        check(fromFlood <= (size_t)(lim.burst + lim.rate * secs + 5), "Flut gedrosselt: " + std::to_string(fromFlood) + " von 3000 in " + std::to_string(secs).substr(0, 4) + " s");
        for (int k = 0; k < 3; ++k)
            for (int i = 0; i < 400; ++i) r.send(false, {0x10, (u8)i});
        pump({&h, &honest}, 4.0, [&] { return r.closed.load(); });
        check(h.lastReason == LN_R_RATE || r.closed, "anhaltende Flut -> getrennt (Grund " + std::to_string(h.lastReason) + ")");
        r.stop();
    }
    // der ehrliche Gast spielt weiter
    u8 b[2] = {4, 2};
    const size_t before = honest.msgs.size();
    ln_send(h.c, honest.peers.empty() ? 0 : h.peers[0], LN_RELIABLE, b, 2);
    check(pump({&h, &honest}, 3.0, [&] { return honest.msgs.size() > before; }) && honest.disconnected == 0, "ehrlicher Gast bleibt verbunden");
    honest.close(), h.close();
    L.s.stop();
}

static void testLobbyAttacks() {
    std::printf("Angriffe auf die Lobby\n");
    TestLobby L;
    L.cfg.maxPerIp = 6;
    if (!L.start()) return check(false, "Lobby");
    auto raw = [&](const std::function<void(rtc::WebSocket&)>& act) {
        auto ws = std::make_shared<rtc::WebSocket>();
        std::atomic<bool> open{false};
        ws->onOpen([&] { open = true; });
        ws->open(L.url);
        for (int i = 0; i < 300 && !open; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (open) act(*ws);
        for (int i = 0; i < 300 && !ws->isClosed(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const bool closed = ws->isClosed();
        ws->close();
        return closed;
    };
    check(raw([](rtc::WebSocket& w) { w.send("unsinn"); }), "Unsinn -> getrennt");
    check(raw([](rtc::WebSocket& w) { w.send(rtc::binary(10, std::byte{1})); }), "Binaer -> getrennt");
    check(raw([](rtc::WebSocket& w) { w.send(std::string(20000, 'a')); }), "20 KB -> getrennt");
    check(raw([](rtc::WebSocket& w) { w.send(std::string(3000, '[') + std::string(3000, ']')); }), "tiefe Schachtelung -> getrennt");
    check(raw([](rtc::WebSocket& w) { w.send("{\"t\":\"ban\",\"id\":1,\"addr\":\"x\"}"); }), "Gast sendet Host-Befehl -> getrennt");
    check(raw([](rtc::WebSocket&) {}), "stumm -> nach 10 s getrennt");
    // viele Verbindungen von einer Adresse
    std::vector<std::shared_ptr<rtc::WebSocket>> many;
    std::atomic<int> opened{0};
    for (int i = 0; i < 12; ++i) {
        auto ws = std::make_shared<rtc::WebSocket>();
        ws->onOpen([&] { ++opened; });
        ws->open(L.url);
        many.push_back(ws);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    int alive = 0;
    for (auto& w : many) alive += w->isOpen() ? 1 : 0;
    check(alive <= L.cfg.maxPerIp, "hoechstens " + std::to_string(L.cfg.maxPerIp) + " Verbindungen je Adresse (offen: " + std::to_string(alive) + ")");
    for (auto& w : many) w->close();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Raten von Codes: nach 10 unbekannten Raeumen gesperrt
    int refused = 0, rate = 0;
    for (int i = 0; i < 14; ++i) {
        std::string err;
        raw([&](rtc::WebSocket& w) {
            w.onMessage([&](rtc::message_variant v) {
                if (auto* s = std::get_if<std::string>(&v)) err = *s;
            });
            LMsg j;
            j.t = LType::Join, j.room = "ZZZZZ" + std::string(1, "0123456789ABCD"[i]), j.name = "Rater";
            w.send(build(j));
        });
        refused += err.find("noroom") != std::string::npos ? 1 : 0;
        rate += err.find("rate") != std::string::npos ? 1 : 0;
    }
    check(refused == L.cfg.failsPerTenMinutes && rate == 14 - L.cfg.failsPerTenMinutes,
          "Codes raten: " + std::to_string(refused) + " x unbekannt, danach " + std::to_string(rate) + " x gesperrt");
    const auto st = L.s.stats();
    note("Lobby: " + std::to_string(st.messages) + " Nachrichten, " + std::to_string(st.rejected) + " abgelehnt, " + std::to_string(st.refusedJoins) +
         " Beitritte abgewiesen, " + std::to_string(st.rateLimited) + " gedrosselt");
    L.s.stop();
    // Fuzzing ueber das Netz gegen eine Lobby ohne Ratenbremse; danach muss ein echter Beitritt gehen
    TestLobby F;
    F.cfg.maxPerIp = 100000, F.cfg.joinsPerMinute = 100000, F.cfg.failsPerTenMinutes = 100000, F.cfg.maxConnections = 100000;
    if (!F.start()) return check(false, "Lobby (Fuzz)");
    std::mt19937 rng(77);
    const char* tmpl[] = {"{\"t\":\"host\",\"v\":1,\"max\":3}", "{\"t\":\"join\",\"v\":1,\"room\":\"AB12CD\",\"name\":\"x\"}",
                          "{\"t\":\"sig\",\"kind\":\"answer\",\"sdp\":\"v=0\\r\\n\"}", "{\"t\":\"cand\",\"c\":\"candidate:1 1 UDP 1 1.2.3.4 9 typ host\",\"mid\":\"0\"}",
                          "{\"t\":\"refuse\",\"id\":1,\"e\":\"full\"}", "{\"t\":\"done\"}"};
    int sent = 0;
    std::vector<std::shared_ptr<rtc::WebSocket>> conns;
    for (int c = 0; c < 60; ++c) {
        auto ws = std::make_shared<rtc::WebSocket>();
        ws->open(F.url);
        conns.push_back(ws);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    for (int i = 0; i < 3000; ++i) {
        auto& ws = conns[rng() % conns.size()];
        if (!ws->isOpen()) continue;
        std::string x = tmpl[rng() % 6];
        for (int k = 0, n = 1 + (int)(rng() % 4); k < n && !x.empty(); ++k) x[rng() % x.size()] = (char)(rng() % 95 + 32);
        try {
            ws->send(x);
            ++sent;
        } catch (...) {
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    for (auto& w : conns) w->close();
    Cfg cfg;
    cfg.lobby = F.url;
    Ctx h = host(cfg);
    waitRoom(h);
    Ctx g = join(cfg, h.code, "Nachher");
    check(pump({&h, &g}, 15.0, [&] { return h.connected && g.connected; }), "nach " + std::to_string(sent) + " veraenderten Nachrichten: Beitritt geht");
    g.close(), h.close();
    F.s.stop();
}

int main(int argc, char** argv) {
    std::string only;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--only" && i + 1 < argc) only = argv[++i];
    std::printf("liminal_net %d (libdatachannel %s)\n", ln_version(), RTC_VERSION);
    const auto t0 = Clock::now();
    if (only.empty() || only == "codes") testCodes();
    if (only.empty() || only == "proto") testLobbyProto();
    if (only.empty() || only == "join") testLobbyJoin();
    if (only.empty() || only == "wrong") testWrongCodes();
    if (only.empty() || only == "paths") testPaths();
    if (only.empty() || only == "direct") testDirect();
    if (only.empty() || only == "tls") testTls();
    if (only.empty() || only == "host") testHostAttacks();
    if (only.empty() || only == "lobby") testLobbyAttacks();
    std::printf("%s: %d Pruefungen, %d Fehler (%.0f s)\n", gFails ? "FEHLER" : "OK", gChecks, gFails, secSince(t0));
    return gFails ? 1 : 0;
}
