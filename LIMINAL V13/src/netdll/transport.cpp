// V13.3: liminal_net.dll - Transport des Koop-Modus (MULTIPLAYER_ARCHITEKTUR.md, Abschnitte 4-7).
//
// Verbindung, NAT-Durchdringung, Relay und Verschluesselung macht libdatachannel (WebRTC-Datenkanaele: ICE/STUN/TURN
// ueber libjuice, DTLS 1.2 ueber Mbed TLS, SCTP ueber usrsctp). Hier steht nur, was die Bibliothek nicht kennt:
// Lobby-Nachrichten, Grenzen (Plaetze, halbe Beitritte, Raten, Zeitlimits, Sperren), die Pruefung des Codes
// (Geheimteil im DTLS-Kanal, Pruefteil gegen den Fingerabdruck) und eine kleine Rahmung (Hallo, Ping, Nutzdaten).
//
// Alles aus dem Netz gilt als feindlich: feste Laengen, Hoechstgroesse, Raten je Gast, Zeitlimits; Fehler trennen die
// Gegenstelle, nie den Prozess. Jeder Aufruf der Bibliothek steht in try/catch (sie meldet Fehler als Ausnahme).
// Nachrichten werden im Spiel-Thread abgeholt (ln_poll); die Rueckrufe der Bibliothek legen nur Beschreibungen,
// Kandidaten und neue Kanaele in einen Eingang.
#define LN_BUILD
#include "netdll/ln_api.h"

#include <rtc/rtc.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "netdll/hostcert.hpp"
#include "netdll/joincode.hpp"
#include "netdll/lobby_client.hpp"
#include "netdll/lobby_proto.hpp"
#include "netdll/lobby_server.hpp"

namespace {

using namespace lim::netc;
using u8 = std::uint8_t;

constexpr u8 F_HELLO = 1, F_WELCOME = 2, F_REFUSE = 3, F_DATA = 0x10, F_PING = 0x11, F_PONG = 0x12;
constexpr char kMagic[4] = {'L', 'I', 'M', 'N'};
constexpr size_t kHelloFixed = 1 + 4 + 2 + kSecretChars + 1;  // + Name
constexpr size_t kPingLen = 9;
constexpr uint16_t kDefaultPort = 27500;
constexpr double kPingInterval = 0.5;
constexpr size_t kLossWindow = 40;              // 20 s
constexpr size_t kMaxBufferedUnreliable = 16 * 1024;  // unzuverlaessig: lieber verwerfen als veraltet zustellen
constexpr size_t kMaxBufferedReliable = 4 * 1024 * 1024;
constexpr int kMaxReadPerFrame = 256;           // je Kanal und Bild; der Rest wartet (Gegendruck)
constexpr int kMaxRemoteCandidates = 40;
constexpr double kGuestTimeout = 30.0;
constexpr double kLobbyTimeout = 10.0;

double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

struct Inbox {
    enum Kind { Desc = 1, Cand = 2, Channel = 3 };
    struct Item {
        int slot = 0;
        uint32_t gen = 0;
        int kind = 0;
        std::string a, b;
        std::shared_ptr<rtc::DataChannel> dc;
    };
    std::mutex m;
    std::deque<Item> q;
    void push(Item it) {
        std::lock_guard<std::mutex> l(m);
        if (q.size() < 2048) q.push_back(std::move(it));
    }
    bool pop(Item& out) {
        std::lock_guard<std::mutex> l(m);
        if (q.empty()) return false;
        out = std::move(q.front());
        q.pop_front();
        return true;
    }
};

struct Bucket {
    double rate = 120, burst = 60, tokens = 60, last = -1;
    void set(double r, double b) { rate = r, burst = b, tokens = b, last = -1; }
    bool take(double now) {
        if (last < 0) last = now;
        tokens = std::min(burst, tokens + (now - last) * rate);
        last = now;
        if (tokens < 1.0) return false;
        tokens -= 1.0;
        return true;
    }
};

struct PingWin {
    struct P {
        uint32_t seq;
        double t;
        bool answered;
    };
    uint32_t seq = 0;
    std::deque<P> sent;
    double rtt = 0, rttVar = 0;
    bool any = false;
    void answer(uint32_t s, double now) {
        for (P& p : sent)
            if (p.seq == s && !p.answered) {
                p.answered = true;
                const double r = (now - p.t) * 1000.0;
                if (!any) rtt = r, rttVar = r * 0.5, any = true;
                else rttVar = 0.75 * rttVar + 0.25 * std::abs(r - rtt), rtt = 0.875 * rtt + 0.125 * r;
            }
    }
    float loss(double now) const {
        int n = 0, lost = 0;
        for (const P& p : sent)
            if (now - p.t > 2.0) ++n, lost += p.answered ? 0 : 1;  // nach 2 s gilt ein Ping als verloren
        return n ? (float)lost / (float)n : 0.0f;
    }
};

struct Peer {
    enum Phase { Free, Signaling, Hello, Open, Closing };  // Closing: Absage gesendet, Verbindung lebt noch kurz
    Phase phase = Free;
    uint32_t gen = 0;
    int lobbyId = 0;
    std::string addr, name;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> r, u;
    std::string remoteFp;  // Fingerabdruck aus der Beschreibung der Gegenstelle
    double opened = 0, lastRx = 0, nextPing = 0, closeAt = 0;
    int remoteCands = 0;
    bool helloSent = false;
    PingWin ping;
    Bucket bucket, pingBucket;
    int invalid = 0, rateDrops = 0;
    LnStats st{};
};

struct Grave {
    double t;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> r, u;
};

std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

void initLibrary() {
    static std::once_flag once;
    std::call_once(once, [] {
        const char* env = std::getenv("LIMINAL_NET_LOG");
        if (env && *env == '1') rtc::InitLogger(rtc::LogLevel::Warning, [](rtc::LogLevel, std::string msg) { std::fprintf(stderr, "[rtc] %s\n", msg.c_str()); });
        else rtc::InitLogger(rtc::LogLevel::None);
        // SCTP fuer ein Echtzeitspiel: libdatachannel laesst die Wartezeit bis zur Wiederholung bis 10 s wachsen und gibt
        // nach 5 Versuchen auf. Auf verlustreichen Wegen (Relay, zwei gestoerte Strecken) stand der Verkehr dann bis zu
        // 10 s still (auch der unzuverlaessige Kanal wartet im Staufenster) und die Stille-Erkennung trennte nach 37 min im
        // Langlauf. Hoechstens 2 s Wartezeit, 12 Versuche; ob eine Verbindung tot ist, entscheidet die Stille (10 s).
        rtc::SctpSettings ss;
        ss.maxRetransmitTimeout = std::chrono::milliseconds(2000);
        ss.initialRetransmitTimeout = std::chrono::milliseconds(1000);
        ss.maxRetransmitAttempts = 12;
        rtc::SetSctpSettings(ss);
    });
}

}  // namespace

struct LnContext {
    bool server = false;
    LnLimits lim{};
    std::string lobbyUrl, lobbyHost, lobbyPin, bindAddress;
    bool lobbyTls = false, direct = false, relayOnly = false;
    uint16_t port = 0;
    std::vector<IceEntry> extraIce;
    std::vector<rtc::IceServer> ice;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
    std::unique_ptr<LobbyClient> ws;
    double wsStarted = 0, wsRetryAt = 0;
    bool wsOpen = false, lobbyReported = false;
    std::unique_ptr<LobbyServer> embedded;
    std::deque<LnEvent> events;
    std::vector<Grave> graves;
    std::vector<Peer> peers;  // Host: Plaetze; Gast: einer (der Host)
    uint32_t genCounter = 0;
    // Host
    HostCert cert;
    std::string room, secret, pin;
    struct Fails {
        int n = 0;
        double window = 0, until = 0;
    };
    std::map<std::string, Fails> fails;
    std::set<std::string> banned;
    // Gast
    JoinCode code;
    std::string myName, directHost;
    uint16_t directPort = 0;
    double started = 0;
    int progress = 0;
    bool joined = false, connected = false, finished = false;
};

namespace {

void pushEvent(LnContext* c, int type, int peer, int channel, int reason, const u8* data, int len, const char* name) {
    LnEvent ev{};
    ev.type = type, ev.peer = peer, ev.channel = channel, ev.reason = reason, ev.len = len;
    if (data && len > 0) std::memcpy(ev.data, data, (size_t)len);
    if (name) std::strncpy(ev.name, name, LN_NAME_MAX);
    c->events.push_back(ev);
}

// --- Adressen und Server ---------------------------------------------------------------------------
// "ws://name:port/pfad" oder "wss://..." -> Rechner, Port, TLS
bool parseUrl(const std::string& url, std::string& host, uint16_t& port, bool& tls) {
    std::string rest;
    if (url.rfind("wss://", 0) == 0) tls = true, rest = url.substr(6);
    else if (url.rfind("ws://", 0) == 0) tls = false, rest = url.substr(5);
    else return false;
    rest = rest.substr(0, rest.find('/'));
    port = tls ? 443 : 80;
    if (!rest.empty() && rest[0] == '[') {
        const size_t e = rest.find(']');
        if (e == std::string::npos) return false;
        host = rest.substr(1, e - 1);
        rest = rest.substr(e + 1);
    } else {
        const size_t p = rest.rfind(':');
        host = rest.substr(0, p);
        rest = p == std::string::npos ? std::string() : rest.substr(p);
    }
    if (!rest.empty()) {
        if (rest[0] != ':' || rest.size() < 2 || rest.size() > 6) return false;
        const int v = std::atoi(rest.c_str() + 1);
        if (v < 1 || v > 65535) return false;
        port = (uint16_t)v;
    }
    return !host.empty() && hostOk(host);
}

// "adresse", "adresse:port", "[v6]:port" oder "v6"
bool parseDirect(const std::string& s, std::string& host, uint16_t& port) {
    port = kDefaultPort;
    if (s.empty() || s.size() > 260) return false;
    if (s[0] == '[') {
        const size_t e = s.find(']');
        if (e == std::string::npos) return false;
        host = s.substr(1, e - 1);
        const std::string rest = s.substr(e + 1);
        if (!rest.empty()) {
            if (rest[0] != ':') return false;
            const int v = std::atoi(rest.c_str() + 1);
            if (v < 1 || v > 65535) return false;
            port = (uint16_t)v;
        }
    } else if (std::count(s.begin(), s.end(), ':') == 1) {
        const size_t p = s.find(':');
        host = s.substr(0, p);
        const int v = std::atoi(s.c_str() + p + 1);
        if (v < 1 || v > 65535) return false;
        port = (uint16_t)v;
    } else {
        host = s;
    }
    return !host.empty() && hostOk(host);
}

// "stun:host:port turn:user:pass@host:port"
std::vector<IceEntry> parseIceList(const char* s) {
    std::vector<IceEntry> out;
    if (!s) return out;
    std::string all(s);
    size_t i = 0;
    while (i < all.size() && out.size() < (size_t)kMaxIce) {
        while (i < all.size() && all[i] == ' ') ++i;
        size_t e = all.find(' ', i);
        if (e == std::string::npos) e = all.size();
        std::string t = all.substr(i, e - i);
        i = e;
        if (t.empty()) continue;
        IceEntry ie;
        if (t.rfind("stun:", 0) == 0) t = t.substr(5);
        else if (t.rfind("turn:", 0) == 0) {
            ie.turn = true;
            t = t.substr(5);
            const size_t at = t.rfind('@');
            const size_t colon = t.find(':');
            if (at == std::string::npos || colon == std::string::npos || colon > at) continue;
            ie.user = t.substr(0, colon), ie.pass = t.substr(colon + 1, at - colon - 1);
            t = t.substr(at + 1);
            if (!credOk(ie.user) || !credOk(ie.pass)) continue;
        } else continue;
        uint16_t port = 3478;
        std::string host;
        if (!parseDirect(t, host, port)) continue;
        ie.host = host, ie.port = port;
        out.push_back(ie);
    }
    return out;
}

void setIce(LnContext* c, const std::vector<IceEntry>& fromLobby) {
    c->ice.clear();
    auto add = [&](const IceEntry& e) {
        const std::string host = e.host.empty() ? c->lobbyHost : e.host;
        if (host.empty() || !hostOk(host)) return;
        try {
            if (e.turn) c->ice.emplace_back(host, e.port, e.user, e.pass, rtc::IceServer::RelayType::TurnUdp);
            else c->ice.emplace_back(host, e.port);
        } catch (...) {
        }
    };
    for (const IceEntry& e : fromLobby) add(e);
    for (const IceEntry& e : c->extraIce) add(e);
}

// --- Lobby -----------------------------------------------------------------------------------------------
void lobbySend(LnContext* c, const LMsg& m) {
    if (!c->ws) return;
    const std::string s = build(m);
    if (s.empty()) return;
    c->ws->send(s);
}

bool loopbackHost(const std::string& h) { return h == "localhost" || h == "::1" || h.rfind("127.", 0) == 0; }

void lobbyConnect(LnContext* c) {
    c->wsOpen = false;
    c->wsStarted = nowSec();
    // WinHTTP: wss:// wird gegen die Stammzertifikate von Windows geprueft oder gegen das festgelegte Zertifikat
    c->ws = std::make_unique<LobbyClient>();
    c->ws->open(c->lobbyUrl, c->lobbyPin, !c->direct && !loopbackHost(c->lobbyHost));
}

void lobbyClose(LnContext* c) {
    if (!c->ws) return;
    c->ws->close();
    c->ws.reset();
    c->wsOpen = false;
}

int reasonOfErr(const std::string& e) {
    if (e == "noroom") return LN_R_NOROOM;
    if (e == "full") return LN_R_FULL;
    if (e == "busy") return LN_R_BUSY;
    if (e == "banned") return LN_R_BANNED;
    if (e == "rate") return LN_R_RATE;
    if (e == "version") return LN_R_VERSION;
    if (e == "closed") return LN_R_HOSTGONE;
    if (e == "badkey") return LN_R_BADKEY;
    if (e == "timeout") return LN_R_TIMEOUT;
    return LN_R_PROTOCOL;
}

// --- Gegenstellen ----------------------------------------------------------------------------------------
void closePeer(LnContext* c, int slot) {
    Peer& p = c->peers[(size_t)slot];
    if (p.phase == Peer::Free && !p.pc) return;
    try {
        if (p.pc) p.pc->resetCallbacks();
        if (p.r) p.r->close();
        if (p.u) p.u->close();
        if (p.pc) p.pc->close();
    } catch (...) {
    }
    c->graves.push_back({nowSec(), p.pc, p.r, p.u});
    const uint32_t gen = p.gen;
    p = Peer{};
    p.gen = gen + 1;
}

void sendFrame(Peer& p, bool reliable, const u8* d, size_t n) {
    auto& dc = reliable ? p.r : p.u;
    if (!dc) return;
    try {
        if (!dc->isOpen()) return;
        if (!reliable && dc->bufferedAmount() > kMaxBufferedUnreliable) {
            ++p.st.dropped;
            return;
        }
        dc->send(reinterpret_cast<const std::byte*>(d), n);
        p.st.bytesSent += n;
    } catch (...) {
    }
}

void refusePeer(Peer& p, int reason) {
    const u8 f[2] = {F_REFUSE, (u8)reason};
    sendFrame(p, true, f, 2);
}

// Absage senden und die Verbindung noch 1 s offen halten, damit sie ankommt (danach schliesst pumpHost)
void refuseLinger(Peer& p, int reason, double now) {
    refusePeer(p, reason);
    p.phase = Peer::Closing;
    p.closeAt = now + 1.0;
}

// Fehlversuch einer Adresse (falscher Code, Muell, Zeitlimit beim Beitritt); nach banFails gesperrt
void noteFail(LnContext* c, const std::string& addr, double now) {
    if (addr.empty()) return;
    auto& f = c->fails[addr];
    if (now - f.window > 600.0) f.n = 0, f.window = now;
    if (++f.n >= c->lim.banFails) f.until = now + c->lim.banMs / 1000.0;
}

void hostDrop(LnContext* c, int slot, int reason, bool notify, bool fail) {
    Peer& p = c->peers[(size_t)slot];
    if (fail) noteFail(c, p.addr, nowSec());
    if (notify && p.phase == Peer::Open) pushEvent(c, LN_EV_DISCONNECTED, slot, 0, reason, nullptr, 0, nullptr);
    if (p.phase == Peer::Signaling && p.lobbyId) {  // die Lobby soll den wartenden Gast entlassen
        LMsg m;
        m.t = LType::Refuse;
        m.id = p.lobbyId;
        m.err = reason == LN_R_FULL ? "full" : reason == LN_R_BANNED ? "banned" : reason == LN_R_BADKEY ? "badkey" : "timeout";
        lobbySend(c, m);
    }
    closePeer(c, slot);
}

void guestFail(LnContext* c, int reason) {
    if (c->finished) return;
    c->finished = true;
    pushEvent(c, LN_EV_DISCONNECTED, 0, 0, reason, nullptr, 0, nullptr);
    closePeer(c, 0);
    lobbyClose(c);
}

std::shared_ptr<rtc::PeerConnection> makePc(LnContext* c, int slot, uint32_t gen) {
    rtc::Configuration rc;
    rc.iceServers = c->ice;
    if (!c->bindAddress.empty()) rc.bindAddress = c->bindAddress;
    if (c->relayOnly) rc.iceTransportPolicy = rtc::TransportPolicy::Relay;
    rc.maxMessageSize = 4096;
    if (c->server) {
        rc.certificatePemFile = c->cert.certPem;
        rc.keyPemFile = c->cert.keyPem;
        if (c->direct) {  // ein UDP-Port fuer alle Gaeste (Portfreigabe moeglich)
            rc.enableIceUdpMux = true;
            rc.portRangeBegin = rc.portRangeEnd = c->port;
        }
    }
    auto pc = std::make_shared<rtc::PeerConnection>(rc);
    std::weak_ptr<Inbox> wi = c->inbox;
    pc->onLocalDescription([wi, slot, gen](rtc::Description d) {
        if (auto in = wi.lock()) in->push({slot, gen, Inbox::Desc, std::string(d), d.typeString(), nullptr});
    });
    pc->onLocalCandidate([wi, slot, gen](rtc::Candidate cd) {
        if (auto in = wi.lock()) in->push({slot, gen, Inbox::Cand, cd.candidate(), cd.mid(), nullptr});
    });
    if (!c->server)
        pc->onDataChannel([wi, slot, gen](std::shared_ptr<rtc::DataChannel> dc) {
            if (auto in = wi.lock()) in->push({slot, gen, Inbox::Channel, dc->label(), "", dc});
        });
    return pc;
}

bool channelsOpen(const Peer& p);

// --- Host: Gast aus der Lobby ------------------------------------------------------------------------------
void hostAdmit(LnContext* c, const LMsg& m, double now) {
    auto refuse = [&](const char* e) {
        LMsg r;
        r.t = LType::Refuse;
        r.id = m.id;
        r.err = e;
        lobbySend(c, r);
    };
    if (c->banned.count(m.addr)) return refuse("banned");
    if (auto f = c->fails.find(m.addr); f != c->fails.end() && f->second.until > now) return refuse("banned");
    int open = 0, half = 0, halfHere = 0, freeSlot = -1;
    for (size_t i = 0; i < c->peers.size(); ++i) {
        const Peer& p = c->peers[i];
        if (p.phase == Peer::Open) ++open;
        else if (p.phase != Peer::Free && p.phase != Peer::Closing) ++half, halfHere += p.addr == m.addr ? 1 : 0;
        else if (freeSlot < 0 && !p.pc) freeSlot = (int)i;
        if (p.phase != Peer::Free && p.lobbyId == m.id) return;  // doppelt
    }
    if (open >= c->lim.maxPeers) return refuse("full");
    if (freeSlot < 0 || half >= c->lim.maxHalfOpen || halfHere >= 3) return refuse("busy");  // 3: drei Freunde hinter einem Router
    Peer& p = c->peers[(size_t)freeSlot];
    const uint32_t gen = p.gen;
    p = Peer{};
    p.gen = gen;
    p.phase = Peer::Signaling;
    p.lobbyId = m.id;
    p.addr = m.addr;
    p.name = m.name;
    p.opened = now;
    p.bucket.set(c->lim.rate, c->lim.burst);
    p.pingBucket.set(10.0, 20.0);
    try {
        p.pc = makePc(c, freeSlot, gen);
        p.r = p.pc->createDataChannel("r");
        // unzuverlaessig: ungeordnet, Lebensdauer 150 ms (PR-SCTP). Was bis dahin nicht durch ist, verfaellt - bei
        // knapper Bandbreite stauen sich so keine veralteten Momentaufnahmen hinter der Staukontrolle
        rtc::DataChannelInit ui;
        ui.reliability.unordered = true;
        ui.reliability.maxPacketLifeTime = std::chrono::milliseconds(150);
        p.u = p.pc->createDataChannel("u", ui);
    } catch (...) {
        closePeer(c, freeSlot);
        return refuse("busy");
    }
}

void handleLobbyHost(LnContext* c, const LMsg& m, double now) {
    auto peerOf = [&](int lobbyId) -> int {
        for (size_t i = 0; i < c->peers.size(); ++i)
            if (c->peers[i].phase != Peer::Free && c->peers[i].lobbyId == lobbyId) return (int)i;
        return -1;
    };
    switch (m.t) {
        case LType::Room: {
            const bool first = c->room.empty();
            c->room = m.room;
            setIce(c, m.ice);
            const std::string text = JoinCode{c->room, c->secret, c->pin}.text(false);
            pushEvent(c, LN_EV_ROOM, -1, 0, first ? 0 : 1, (const u8*)text.data(), (int)text.size(), nullptr);
            break;
        }
        case LType::Guest: hostAdmit(c, m, now); break;
        case LType::Sig: {
            const int s = peerOf(m.from);
            if (s < 0 || m.kind != "answer") break;
            Peer& p = c->peers[(size_t)s];
            if (p.phase != Peer::Signaling || !p.remoteFp.empty()) break;
            try {
                rtc::Description d(m.sdp, "answer");
                auto fp = d.fingerprint();
                if (!fp || fp->algorithm != rtc::CertificateFingerprint::Algorithm::Sha256) throw std::runtime_error("fp");
                p.remoteFp = upper(fp->value);
                p.pc->setRemoteDescription(d);
            } catch (...) {
                hostDrop(c, s, LN_R_PROTOCOL, false, true);
            }
            break;
        }
        case LType::Cand: {
            const int s = peerOf(m.from);
            if (s < 0) break;
            Peer& p = c->peers[(size_t)s];
            if (p.phase != Peer::Signaling || ++p.remoteCands > kMaxRemoteCandidates) break;
            try {
                p.pc->addRemoteCandidate(rtc::Candidate(m.cand, m.mid));
            } catch (...) {
            }
            break;
        }
        case LType::Gone: {
            const int s = peerOf(m.id);
            if (s >= 0 && c->peers[(size_t)s].phase == Peer::Signaling && !channelsOpen(c->peers[(size_t)s])) closePeer(c, s);
            break;
        }
        case LType::Err:
            if (c->room.empty() && !c->lobbyReported) {  // Raum nicht eroeffnet
                c->lobbyReported = true;
                pushEvent(c, LN_EV_DISCONNECTED, -1, 0, m.err == "version" ? LN_R_VERSION : m.err == "busy" ? LN_R_FULL : LN_R_LOBBY, nullptr, 0, nullptr);
            }
            break;
        default: break;
    }
}

// --- Gast: Nachrichten der Lobby ---------------------------------------------------------------------------
void handleLobbyGuest(LnContext* c, const LMsg& m) {
    Peer& p = c->peers[0];
    switch (m.t) {
        case LType::Joined:
            if (c->joined) break;
            c->joined = true;
            setIce(c, m.ice);
            c->progress = LN_P_SIGNALING;
            pushEvent(c, LN_EV_PROGRESS, 0, 0, LN_P_SIGNALING, nullptr, 0, nullptr);
            try {
                p.pc = makePc(c, 0, p.gen);
            } catch (...) {
                guestFail(c, LN_R_ICE);
            }
            break;
        case LType::Sig: {
            if (!p.pc || m.kind != "offer" || !p.remoteFp.empty()) break;
            try {
                rtc::Description d(m.sdp, "offer");
                auto fp = d.fingerprint();
                if (!fp || fp->algorithm != rtc::CertificateFingerprint::Algorithm::Sha256) return guestFail(c, LN_R_CERT);
                p.remoteFp = upper(fp->value);
                // Pruefteil des Codes: der Host muss genau dieses Zertifikat haben (DTLS erzwingt es danach)
                if (c->code.hasPin() && !equalConst(pinFromFingerprint(p.remoteFp), c->code.pin)) return guestFail(c, LN_R_CERT);
                p.pc->setRemoteDescription(d);
                if (c->direct) {  // die eingegebene Adresse als Kandidat (Portfreigabe ueber das Internet)
                    rtc::Candidate cand("candidate:1 1 UDP 1694498815 " + c->directHost + " " + std::to_string(c->directPort) + " typ host", d.bundleMid());
                    if (cand.resolve(rtc::Candidate::ResolveMode::Lookup)) p.pc->addRemoteCandidate(cand);
                }
                c->progress = LN_P_CONNECTING;
                pushEvent(c, LN_EV_PROGRESS, 0, 0, LN_P_CONNECTING, nullptr, 0, nullptr);
            } catch (...) {
                guestFail(c, LN_R_PROTOCOL);
            }
            break;
        }
        case LType::Cand:
            if (!p.pc || ++p.remoteCands > kMaxRemoteCandidates) break;
            try {
                p.pc->addRemoteCandidate(rtc::Candidate(m.cand, m.mid));
            } catch (...) {
            }
            break;
        case LType::Err: {
            int r = reasonOfErr(m.err);
            // der Host hat aufgegeben, waehrend ICE noch suchte: kein Weg gefunden
            if (r == LN_R_TIMEOUT && c->progress >= LN_P_CONNECTING && c->progress < LN_P_HELLO) r = LN_R_ICE;
            guestFail(c, r);
            break;
        }
        default: break;
    }
}

void pumpLobby(LnContext* c, double now) {
    if (!c->ws) {
        if (c->server && now >= c->wsRetryAt) lobbyConnect(c);  // Host: Lobby neu versuchen (neuer Code)
        return;
    }
    const bool open = c->ws->isOpen(), closed = c->ws->isClosed();
    const bool certBad = closed && (c->ws->error() == LobbyClient::Error::Tls || c->ws->error() == LobbyClient::Error::Pin);
    if (open && !c->wsOpen) {
        c->wsOpen = true;
        LMsg m;
        if (c->server) {
            m.t = LType::Host;
            m.max = std::clamp(c->lim.maxPeers, 1, 7);
        } else {
            m.t = LType::Join;
            m.room = c->code.room;
            m.name = c->myName;
        }
        lobbySend(c, m);
    }
    for (int k = 0; k < 64 && c->ws; ++k) {
        const std::optional<std::string> msg = c->ws->receive();
        if (!msg) break;
        LMsg m;
        if (!parseFromLobby(*msg, m)) continue;  // ungueltig: verwerfen
        if (c->server) handleLobbyHost(c, m, now);
        else handleLobbyGuest(c, m);
    }
    if (!c->ws) return;
    if (closed || (!c->wsOpen && now - c->wsStarted > kLobbyTimeout)) {
        const bool wasOpen = c->wsOpen;
        lobbyClose(c);
        if (c->server) {
            if (!c->room.empty()) {  // Raum verloren: neue Beitritte erst wieder mit neuem Code
                c->room.clear();
                pushEvent(c, LN_EV_ROOM, -1, 0, 0, nullptr, 0, nullptr);
            } else if (!c->lobbyReported) {
                c->lobbyReported = true;
                pushEvent(c, LN_EV_DISCONNECTED, -1, 0, certBad ? LN_R_CERT : LN_R_LOBBY, nullptr, 0, nullptr);
            }
            c->wsRetryAt = now + 5.0;
            if (c->direct) c->wsRetryAt = now + 1.0;
        } else if (!c->connected && c->progress < LN_P_CONNECTING) {
            guestFail(c, certBad ? LN_R_CERT : LN_R_LOBBY);  // ohne Beschreibung des Hosts geht es nicht weiter
        }
        (void)wasOpen;
    }
}

// --- Rahmen ------------------------------------------------------------------------------------------------
void countInvalid(LnContext* c, int slot, double now) {
    Peer& p = c->peers[(size_t)slot];
    ++p.st.dropped;
    if (++p.invalid > c->lim.maxInvalid) {
        if (c->server) hostDrop(c, slot, LN_R_PROTOCOL, true, p.phase != Peer::Open);
        else guestFail(c, LN_R_PROTOCOL);
    }
    (void)now;
}

void sendPing(Peer& p, double now) {
    u8 f[kPingLen];
    f[0] = F_PING;
    const uint32_t s = ++p.ping.seq;
    std::memcpy(f + 1, &s, 4);
    const uint32_t t = (uint32_t)(now * 1000.0);
    std::memcpy(f + 5, &t, 4);
    p.ping.sent.push_back({s, now, false});
    while (p.ping.sent.size() > kLossWindow) p.ping.sent.pop_front();
    sendFrame(p, false, f, kPingLen);
}

// gemeinsam fuer Host und Gast im offenen Zustand: Nutzdaten, Ping, Pong. false: ungueltig
bool openFrame(LnContext* c, int slot, int channel, const u8* d, size_t n, double now) {
    Peer& p = c->peers[(size_t)slot];
    if (d[0] == F_DATA && n >= 1 && n <= 1 + LN_MAX_PAYLOAD) {
        if (c->server && !p.bucket.take(now)) {
            ++p.st.dropped;
            if (++p.rateDrops > 200) hostDrop(c, slot, LN_R_RATE, true, false);  // dauerhaft ueber der Rate
            return true;
        }
        p.rateDrops = 0;
        ++p.st.received;
        if (n > 1) pushEvent(c, LN_EV_MESSAGE, slot, channel, 0, d + 1, (int)(n - 1), nullptr);
        return true;
    }
    if (channel == LN_UNRELIABLE && n == kPingLen && d[0] == F_PING) {
        if (p.pingBucket.take(now)) {
            u8 f[kPingLen];
            std::memcpy(f, d, kPingLen);
            f[0] = F_PONG;
            sendFrame(p, false, f, kPingLen);
        }
        return true;
    }
    if (channel == LN_UNRELIABLE && n == kPingLen && d[0] == F_PONG) {
        uint32_t s;
        std::memcpy(&s, d + 1, 4);
        p.ping.answer(s, now);
        return true;
    }
    return false;
}

void hostFrame(LnContext* c, int slot, int channel, const u8* d, size_t n, double now) {
    Peer& p = c->peers[(size_t)slot];
    if (n == 0 || n > 1 + LN_MAX_PAYLOAD) return countInvalid(c, slot, now);
    p.lastRx = now;
    if (p.phase == Peer::Hello) {
        if (channel != LN_RELIABLE || d[0] != F_HELLO || n < kHelloFixed + 1 || n > kHelloFixed + LN_NAME_MAX || std::memcmp(d + 1, kMagic, 4) != 0)
            return countInvalid(c, slot, now);
        const unsigned ver = (unsigned)d[5] | ((unsigned)d[6] << 8);
        if (ver != LN_VERSION) return refuseLinger(p, LN_R_VERSION, now);
        const std::string secret((const char*)d + 7, kSecretChars);
        const size_t nl = d[7 + kSecretChars];
        if (nl != n - kHelloFixed) return countInvalid(c, slot, now);
        const std::string name((const char*)d + kHelloFixed, nl);
        if (!equalConst(secret, c->secret)) {
            noteFail(c, p.addr, now);
            return refuseLinger(p, LN_R_BADKEY, now);
        }
        if (!nameOk(name)) {
            noteFail(c, p.addr, now);
            return refuseLinger(p, LN_R_PROTOCOL, now);
        }
        int open = 0;
        for (const Peer& q : c->peers) open += q.phase == Peer::Open ? 1 : 0;
        if (open >= c->lim.maxPeers) return refuseLinger(p, LN_R_FULL, now);
        p.name = name;
        p.phase = Peer::Open;
        p.nextPing = now;
        const u8 w[3] = {F_WELCOME, (u8)(LN_VERSION & 0xFF), (u8)(LN_VERSION >> 8)};
        sendFrame(p, true, w, 3);
        pushEvent(c, LN_EV_CONNECTED, slot, 0, 0, nullptr, 0, p.name.c_str());
        return;
    }
    if (p.phase != Peer::Open || !openFrame(c, slot, channel, d, n, now)) countInvalid(c, slot, now);
}

void guestFrame(LnContext* c, int channel, const u8* d, size_t n, double now) {
    Peer& p = c->peers[0];
    if (n == 0 || n > 1 + LN_MAX_PAYLOAD) return countInvalid(c, 0, now);
    p.lastRx = now;
    if (!c->connected) {
        if (channel == LN_RELIABLE && n == 3 && d[0] == F_WELCOME && ((unsigned)d[1] | ((unsigned)d[2] << 8)) == LN_VERSION) {
            c->connected = true;
            p.phase = Peer::Open;
            p.nextPing = now;
            pushEvent(c, LN_EV_CONNECTED, 0, 0, 0, nullptr, 0, nullptr);
            LMsg done;  // die Lobby wird nicht mehr gebraucht
            done.t = LType::Done;
            lobbySend(c, done);
            lobbyClose(c);
            return;
        }
        if (channel == LN_RELIABLE && n == 2 && d[0] == F_REFUSE) return guestFail(c, d[1] >= 1 && d[1] <= LN_R_BUSY ? d[1] : LN_R_PROTOCOL);
        return countInvalid(c, 0, now);
    }
    if (channel == LN_RELIABLE && n == 2 && d[0] == F_REFUSE) return guestFail(c, d[1] >= 1 && d[1] <= LN_R_BUSY ? d[1] : LN_R_PROTOCOL);
    if (!openFrame(c, 0, channel, d, n, now)) countInvalid(c, 0, now);
}

// Nachrichten eines Kanals abholen (hoechstens kMaxReadPerFrame; der Rest bleibt in der Bibliothek)
void readChannel(LnContext* c, int slot, int channel, double now) {
    const uint32_t gen = c->peers[(size_t)slot].gen;
    for (int k = 0; k < kMaxReadPerFrame; ++k) {
        Peer& p = c->peers[(size_t)slot];
        auto& dc = channel == LN_RELIABLE ? p.r : p.u;
        if (!dc || p.phase == Peer::Free || p.gen != gen) return;
        std::optional<rtc::message_variant> m;
        try {
            m = dc->receive();
        } catch (...) {
        }
        if (!m) return;
        if (!std::holds_alternative<rtc::binary>(*m)) {
            countInvalid(c, slot, now);
            continue;
        }
        const rtc::binary& b = std::get<rtc::binary>(*m);
        p.st.bytesReceived += b.size();
        if (c->server) hostFrame(c, slot, channel, reinterpret_cast<const u8*>(b.data()), b.size(), now);
        else guestFrame(c, channel, reinterpret_cast<const u8*>(b.data()), b.size(), now);
        if (c->peers[(size_t)slot].gen != gen || c->finished) return;  // getrennt
    }
}

bool channelsOpen(const Peer& p) {
    try {
        return p.r && p.u && p.r->isOpen() && p.u->isOpen();
    } catch (...) {
        return false;
    }
}

// DTLS hat den Fingerabdruck geprueft; mit Mbed TLS darf eine Gegenstelle ohne Zertifikat aber nicht durchrutschen
// (MULTIPLAYER_RECHERCHE.md, Abschnitt 4): der gepruefte Wert muss vorliegen und gleich dem aus der Beschreibung sein
bool fingerprintVerified(Peer& p) {
    try {
        const std::string fp = upper(p.pc->remoteFingerprint().value);
        return !fp.empty() && !p.remoteFp.empty() && equalConst(fp, p.remoteFp);
    } catch (...) {
        return false;
    }
}

void pumpInbox(LnContext* c) {
    Inbox::Item it;
    for (int guard = 0; guard < 512 && c->inbox->pop(it); ++guard) {
        if (it.slot < 0 || it.slot >= (int)c->peers.size()) continue;
        Peer& p = c->peers[(size_t)it.slot];
        if (p.gen != it.gen || !p.pc) continue;  // veraltet
        LMsg m;
        if (it.kind == Inbox::Desc) {
            m.t = LType::Sig;
            m.kind = it.b == "offer" ? "offer" : "answer";
            m.sdp = it.a;
            if (!sdpOk(m.sdp)) continue;
        } else if (it.kind == Inbox::Cand) {
            m.t = LType::Cand;
            m.cand = it.a, m.mid = it.b;
            if (!candOk(m.cand) || !midOk(m.mid)) continue;
        } else if (it.kind == Inbox::Channel && !c->server) {
            if (it.a == "r" && !p.r) p.r = it.dc;
            else if (it.a == "u" && !p.u) p.u = it.dc;
            else
                try {
                    it.dc->close();
                } catch (...) {
                }
            continue;
        } else {
            continue;
        }
        if (c->server) m.to = p.lobbyId;
        lobbySend(c, m);
    }
}

void pumpHost(LnContext* c, double now) {
    for (size_t i = 0; i < c->peers.size(); ++i) {
        Peer& p = c->peers[i];
        if (p.phase == Peer::Free) continue;
        if (p.phase == Peer::Closing) {
            if (now >= p.closeAt) closePeer(c, (int)i);
            continue;
        }
        rtc::PeerConnection::State st = rtc::PeerConnection::State::Closed;
        try {
            st = p.pc->state();
        } catch (...) {
        }
        if (st == rtc::PeerConnection::State::Failed || st == rtc::PeerConnection::State::Closed) {
            hostDrop(c, (int)i, p.phase == Peer::Open ? LN_R_TIMEOUT : LN_R_ICE, true, false);
            continue;
        }
        if (p.phase == Peer::Signaling && channelsOpen(p)) {
            if (!fingerprintVerified(p)) {
                hostDrop(c, (int)i, LN_R_CERT, false, true);
                continue;
            }
            p.phase = Peer::Hello;
            p.lastRx = now;
        }
        if (p.phase != Peer::Open && now - p.opened > c->lim.handshakeMs / 1000.0) {
            hostDrop(c, (int)i, LN_R_TIMEOUT, false, p.phase == Peer::Hello);
            continue;
        }
        if (p.phase == Peer::Hello || p.phase == Peer::Open) {
            const uint32_t gen = p.gen;
            readChannel(c, (int)i, LN_RELIABLE, now);
            if (c->peers[i].gen == gen) readChannel(c, (int)i, LN_UNRELIABLE, now);
            if (c->peers[i].gen != gen) continue;
        }
        if (p.phase == Peer::Open) {
            if (now - p.lastRx > c->lim.silenceMs / 1000.0) {
                hostDrop(c, (int)i, LN_R_TIMEOUT, true, false);
                continue;
            }
            try {
                if (p.r->bufferedAmount() > kMaxBufferedReliable) {  // liest nicht mehr: trennen
                    hostDrop(c, (int)i, LN_R_TIMEOUT, true, false);
                    continue;
                }
            } catch (...) {
            }
            if (now >= p.nextPing) p.nextPing = now + kPingInterval, sendPing(p, now);
        }
    }
}

void pumpGuest(LnContext* c, double now) {
    Peer& p = c->peers[0];
    if (c->finished) return;
    if (p.pc) {
        rtc::PeerConnection::State st = rtc::PeerConnection::State::New;
        try {
            st = p.pc->state();
        } catch (...) {
        }
        if (p.helloSent) {  // zuerst lesen: eine Absage des Hosts kommt vor dem Schliessen
            readChannel(c, 0, LN_RELIABLE, now);
            if (!c->finished) readChannel(c, 0, LN_UNRELIABLE, now);
            if (c->finished) return;
        }
        if (st == rtc::PeerConnection::State::Failed || st == rtc::PeerConnection::State::Closed)
            return guestFail(c, c->connected || p.helloSent ? LN_R_TIMEOUT : LN_R_ICE);
        if (!p.helloSent && channelsOpen(p)) {
            if (!fingerprintVerified(p) || (c->code.hasPin() && !equalConst(pinFromFingerprint(p.remoteFp), c->code.pin))) return guestFail(c, LN_R_CERT);
            u8 h[kHelloFixed + LN_NAME_MAX];
            h[0] = F_HELLO;
            std::memcpy(h + 1, kMagic, 4);
            h[5] = (u8)(LN_VERSION & 0xFF), h[6] = (u8)(LN_VERSION >> 8);
            std::memcpy(h + 7, c->code.secret.data(), kSecretChars);
            h[7 + kSecretChars] = (u8)c->myName.size();
            std::memcpy(h + kHelloFixed, c->myName.data(), c->myName.size());
            sendFrame(p, true, h, kHelloFixed + c->myName.size());
            p.helloSent = true;
            p.phase = Peer::Hello;
            p.lastRx = now;
            c->progress = LN_P_HELLO;
            pushEvent(c, LN_EV_PROGRESS, 0, 0, LN_P_HELLO, nullptr, 0, nullptr);
        }
    }
    if (!c->connected) {
        if (now - c->started > kGuestTimeout)
            return guestFail(c, c->progress >= LN_P_HELLO ? LN_R_TIMEOUT : c->progress >= LN_P_SIGNALING ? LN_R_ICE : LN_R_LOBBY);
        return;
    }
    if (now - p.lastRx > c->lim.silenceMs / 1000.0) return guestFail(c, LN_R_TIMEOUT);
    if (now >= p.nextPing) p.nextPing = now + kPingInterval, sendPing(p, now);
}

void pump(LnContext* c) {
    const double now = nowSec();
    pumpLobby(c, now);
    pumpInbox(c);
    if (c->server) pumpHost(c, now);
    else pumpGuest(c, now);
    // Graeber: geschlossene Verbindungen erst nach einer Weile freigeben (Rueckrufe der Bibliothek laufen aus)
    std::erase_if(c->graves, [now](const Grave& g) { return now - g.t > 3.0; });
}

LnContext* makeContext(const LnConfig* cfg) {
    initLibrary();
    auto* c = new LnContext;
    if (cfg) {
        if (cfg->lobby && *cfg->lobby) c->lobbyUrl = cfg->lobby;
        if (cfg->lobbyCert && *cfg->lobbyCert)  // PEM oder Fingerabdruck "AB:CD:..."
            c->lobbyPin = std::strncmp(cfg->lobbyCert, "-----BEGIN", 10) == 0 ? fingerprintOfPem(cfg->lobbyCert) : upper(cfg->lobbyCert);
        if (cfg->bindAddress) c->bindAddress = cfg->bindAddress;
        c->extraIce = parseIceList(cfg->ice);
        c->port = cfg->port ? cfg->port : kDefaultPort;
        c->relayOnly = cfg->relayOnly != 0;
    } else {
        c->port = kDefaultPort;
    }
    c->direct = c->lobbyUrl.empty();
    return c;
}

}  // namespace

extern "C" {

LN_API LnContext* ln_host(const LnConfig* cfg, const LnLimits* lim) {
    LnContext* c = makeContext(cfg);
    c->server = true;
    c->lim = lim ? *lim : LnLimits{3, 4, 120.0, 60.0, 8, 20000, 10000, 5, 600000};
    c->lim.maxPeers = std::clamp(c->lim.maxPeers, 1, 7);
    c->lim.maxHalfOpen = std::clamp(c->lim.maxHalfOpen, 1, 8);
    c->peers.resize((size_t)(c->lim.maxPeers + c->lim.maxHalfOpen));
    std::string err;
    if (!makeHostCert(c->cert, err) || !randomChars(kSecretChars, c->secret)) {
        delete c;
        return nullptr;
    }
    c->pin = pinFromFingerprint(c->cert.fingerprint);
    if (c->direct) {  // die Lobby laeuft im Host selbst: ein Raum, ohne Relay
        LobbyConfig lc;
        lc.bind = c->bindAddress;
        lc.port = c->port;
        lc.turn = false;
        lc.singleRoom = true;
        lc.maxPerIp = 8;
        lc.maxConnections = 32;  // im Spiel des Hosts: wenige Verbindungen genuegen
        lc.maxRooms = 1;
        c->embedded = std::make_unique<LobbyServer>();
        if (!c->embedded->start(lc, err)) {
            delete c;
            return nullptr;
        }
        c->port = c->embedded->port();
        c->lobbyUrl = "ws://" + std::string(c->bindAddress.empty() || c->bindAddress == "0.0.0.0" ? "127.0.0.1" : c->bindAddress) + ":" +
                      std::to_string(c->port);
    }
    uint16_t p = 0;
    if (!parseUrl(c->lobbyUrl, c->lobbyHost, p, c->lobbyTls)) {
        delete c;
        return nullptr;
    }
    lobbyConnect(c);
    return c;
}

LN_API LnContext* ln_join(const LnConfig* cfg, const char* direct, const char* code, const char* name) {
    if (!code || !name) return nullptr;
    LnContext* c = makeContext(cfg);
    c->lim = LnLimits{1, 1, 1e9, 1e9, 1000000, 20000, 10000, 5, 60000};
    c->peers.resize(1);
    c->started = nowSec();
    c->myName = name;
    c->progress = LN_P_LOBBY;
    if (!nameOk(c->myName)) {
        guestFail(c, LN_R_PROTOCOL);
        return c;
    }
    if (!parseCode(code, c->code)) {
        guestFail(c, LN_R_BADCODE);
        return c;
    }
    if (c->direct) {
        // ohne Lobby: die Adresse des Hosts; der Pruefteil ist Pflicht (die Signalisierung ist hier unverschluesselt)
        if (!direct || !parseDirect(direct, c->directHost, c->directPort) || !c->code.hasPin()) {
            guestFail(c, LN_R_BADCODE);
            return c;
        }
        const bool v6 = c->directHost.find(':') != std::string::npos;
        c->lobbyUrl = "ws://" + (v6 ? "[" + c->directHost + "]" : c->directHost) + ":" + std::to_string(c->directPort);
    }
    uint16_t p = 0;
    if (!parseUrl(c->lobbyUrl, c->lobbyHost, p, c->lobbyTls)) {
        guestFail(c, LN_R_LOBBY);
        return c;
    }
    pushEvent(c, LN_EV_PROGRESS, 0, 0, LN_P_LOBBY, nullptr, 0, nullptr);
    lobbyConnect(c);
    return c;
}

LN_API int ln_poll(LnContext* c, LnEvent* ev) {
    if (!c || !ev) return 0;
    if (c->events.empty()) pump(c);
    if (c->events.empty()) return 0;
    *ev = c->events.front();
    c->events.pop_front();
    return 1;
}

LN_API int ln_send(LnContext* c, int peer, int channel, const uint8_t* data, int len) {
    if (!c || channel < 0 || channel > 1 || !data || len < 0 || len > LN_MAX_PAYLOAD) return 0;
    if (!c->server) peer = 0;
    if (peer < 0 || peer >= (int)c->peers.size()) return 0;
    Peer& p = c->peers[(size_t)peer];
    if (p.phase != Peer::Open) return 0;
    auto& dc = channel == LN_RELIABLE ? p.r : p.u;
    try {
        if (!dc || !dc->isOpen()) return 0;
        if (channel == LN_UNRELIABLE && dc->bufferedAmount() > kMaxBufferedUnreliable) {
            ++p.st.dropped;
            return 0;
        }
        rtc::binary b((size_t)len + 1);
        b[0] = std::byte{F_DATA};
        if (len) std::memcpy(b.data() + 1, data, (size_t)len);
        p.st.bytesSent += b.size();
        dc->send(std::move(b));
    } catch (...) {
        return 0;
    }
    ++p.st.sent;
    return 1;
}

LN_API void ln_drop(LnContext* c, int peer, int reason, int ban) {
    if (!c || !c->server || peer < 0 || peer >= (int)c->peers.size()) return;
    Peer& p = c->peers[(size_t)peer];
    if (p.phase == Peer::Free) return;
    if (ban) {
        if (!p.addr.empty()) c->banned.insert(p.addr);
        LMsg m;  // auch in der Lobby sperren (Adresse des Gasts, fuer diesen Raum)
        m.t = LType::Ban;
        m.id = p.lobbyId;
        m.addr = p.addr;
        if (m.id && credOk(m.addr)) lobbySend(c, m);
    }
    if (p.phase == Peer::Open || p.phase == Peer::Hello) return refuseLinger(p, reason, nowSec());
    if (p.phase != Peer::Closing) hostDrop(c, peer, reason, false, false);
}

LN_API int ln_stats(LnContext* c, int peer, LnStats* out) {
    if (!c || !out) return 0;
    const size_t i = c->server ? (size_t)peer : 0;
    if (peer < 0 && c->server) return 0;
    if (i >= c->peers.size() || !c->peers[i].pc) return 0;
    Peer& p = c->peers[i];
    *out = p.st;
    const double now = nowSec();
    out->rttMs = (uint32_t)std::lround(p.ping.rtt);
    out->rttVarMs = (uint32_t)std::lround(p.ping.rttVar);
    out->loss = p.ping.loss(now);
    out->path = LN_PATH_UNKNOWN;
    try {
        // Laufzeit: die Messung von SCTP (ohne die Wartezeit bis zum naechsten Bild des Spiels, die im eigenen Ping
        // steckt); fehlt sie, der Ping
        if (auto r = p.pc->rtt(); r && r->count() > 0) out->rttMs = (uint32_t)r->count();
        if (std::getenv("LIMINAL_NET_LOG"))
            std::fprintf(stderr, "[ln] Laufzeit SCTP %d ms, Ping %.0f ms\n", (int)out->rttMs, p.ping.rtt);
        rtc::Candidate l, r;
        if (p.pc->getSelectedCandidatePair(&l, &r)) {
            if (std::getenv("LIMINAL_NET_LOG")) std::fprintf(stderr, "[ln] Weg: %s <-> %s\n", l.candidate().c_str(), r.candidate().c_str());
            using T = rtc::Candidate::Type;
            if (l.type() == T::Relayed || r.type() == T::Relayed) out->path = LN_PATH_RELAY;
            else if (l.type() == T::Host && r.type() == T::Host) out->path = LN_PATH_DIRECT;
            else out->path = LN_PATH_NAT;
        }
    } catch (...) {
    }
    return 1;
}

LN_API int ln_code(LnContext* c, int withPin, char* out, int cap) {
    if (!c || !c->server || c->room.empty() || !out || cap <= 0) return 0;
    const std::string t = JoinCode{c->room, c->secret, c->pin}.text(withPin != 0);
    if ((int)t.size() >= cap) return 0;
    std::memcpy(out, t.c_str(), t.size() + 1);
    return 1;
}

LN_API uint16_t ln_port(LnContext* c) { return c && c->embedded ? c->embedded->port() : 0; }

LN_API void ln_close(LnContext* c) {
    if (!c) return;
    for (size_t i = 0; i < c->peers.size(); ++i) closePeer(c, (int)i);
    lobbyClose(c);
    if (c->embedded) c->embedded->stop();
    c->graves.clear();
    delete c;
}

LN_API int ln_random(uint8_t* out, int n) { return out && n > 0 && randomBytes(out, (size_t)n) ? 1 : 0; }
LN_API int ln_version(void) { return LN_VERSION; }

}  // extern "C"
