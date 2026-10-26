// V13.2: liminal_net.dll - Transport des Mehrspielers (MULTIPLAYER_ARCHITEKTUR.md, Abschnitte 4-6).
// ENet traegt die Pakete; darueber Handshake mit Beweis des Beitrittsschluessels und AES-256-GCM je Nachricht.
// Alles aus dem Netz gilt als feindlich: feste Laengen im Handshake, Hoechstgroesse, Zaehler (keine Wiederholung),
// Rate je Gast, Zeitlimits, Sperre nach falschen Beweisen. Fehler trennen die Gegenstelle, nie den Prozess.
#define LN_BUILD
#include "netdll/ln_api.h"

#include <enet/enet.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "netdll/secure.hpp"

namespace {

using lim::netsec::Aead;
using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

constexpr u32 kMagic = 0x4E4D494Cu;  // "LIMN"
enum Frame : u8 { F_HELLO = 1, F_CHALLENGE = 2, F_PROOF = 3, F_DATA = 4, F_REFUSE = 5 };
constexpr size_t kHelloLen = 1 + 4 + 2 + 16 + 1 + LN_NAME_MAX;  // 40
constexpr size_t kChallengeLen = 1 + 4 + 16 + 4;                // 25 (kleiner als HELLO: keine Verstaerkung)
constexpr size_t kProofLen = 1 + 4 + 4 + 16;                    // 25
constexpr size_t kDataHead = 1 + 8;
constexpr size_t kMaxFrame = kDataHead + LN_MAX_PAYLOAD + lim::netsec::kTag;
constexpr int kHalfOpenPerAddr = 3;  // gleichzeitige Handshakes je Adresse (3 Gaeste hinter einem Router)
constexpr int kEnetHeadroom = 32;    // ENet-Plaetze ueber den Spielplaetzen: Ueberzaehlige bekommen sofort eine Absage

double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
void put32(u8* p, u32 v) { for (int i = 0; i < 4; ++i) p[i] = (u8)(v >> (8 * i)); }
u32 get32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
void put64(u8* p, u64 v) { for (int i = 0; i < 8; ++i) p[i] = (u8)(v >> (8 * i)); }
u64 get64(const u8* p) {
    u64 v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
    return v;
}

// Name: 1..16 Zeichen aus Buchstaben, Ziffern, Leerzeichen, '-', '_', '.'
bool validName(const char* s, size_t n) {
    if (n == 0 || n > LN_NAME_MAX) return false;
    for (size_t i = 0; i < n; ++i) {
        const unsigned char ch = (unsigned char)s[i];
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_' || ch == '.';
        if (!ok) return false;
    }
    return s[0] != ' ' && s[n - 1] != ' ';
}

struct Bucket {
    double rate = 120, burst = 60, tokens = 60, last = -1;
    bool take(double now) {
        if (last < 0) last = now;
        tokens = std::min(burst, tokens + (now - last) * rate);
        last = now;
        if (tokens < 1.0) return false;
        tokens -= 1.0;
        return true;
    }
};

struct Peer {
    enum Phase { Free, WaitHello, WaitProof, Open, Closing } phase = Free;  // Closing: abgewiesen, Trennung laeuft
    bool halfOpen() const { return phase == WaitHello || phase == WaitProof || phase == Closing; }
    ENetPeer* ep = nullptr;
    u8 cn[16]{}, sn[16]{};
    u32 cookie = 0;
    char name[LN_NAME_MAX + 1]{};
    size_t nameLen = 0;
    Aead tx, rx;
    u64 txCtr[2]{}, rxCtr[2]{};
    bool rxAny[2]{};
    double opened = 0;
    Bucket bucket;
    int invalid = 0, rateDrops = 0;
    u32 addr = 0;
    bool announced = false;  // Gast: CONNECTED gemeldet (erste gueltige Nachricht des Hosts)
    LnStats st{};
};

struct Ban {
    int fails = 0;
    double until = 0, window = 0;
};

}  // namespace

struct LnContext {
    bool server = false;
    ENetHost* host = nullptr;
    u8 key[LN_KEY_BYTES]{};
    LnLimits lim{};
    std::vector<Peer> peers;  // Host: Plaetze; Gast: einer (der Host)
    std::deque<LnEvent> queue;
    std::map<u32, Ban> bans;
    std::string myName;
};

namespace {

int g_enetRefs = 0;
bool enetUp() {
    if (g_enetRefs++ == 0 && enet_initialize() != 0) {
        g_enetRefs = 0;
        return false;
    }
    return true;
}
void enetDown() {
    if (g_enetRefs > 0 && --g_enetRefs == 0) enet_deinitialize();
}

void sendRaw(Peer& p, int channel, const u8* data, size_t n) {
    if (!p.ep) return;
    ENetPacket* pk = enet_packet_create(data, n, channel == LN_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (pk && enet_peer_send(p.ep, (enet_uint8)channel, pk) != 0) enet_packet_destroy(pk);
}

void pushEvent(LnContext* c, int type, int peer, int channel, int reason, const u8* data, int len, const char* name) {
    LnEvent ev{};
    ev.type = type, ev.peer = peer, ev.channel = channel, ev.reason = reason, ev.len = len;
    if (data && len > 0) std::memcpy(ev.data, data, (size_t)len);
    if (name) std::strncpy(ev.name, name, LN_NAME_MAX);
    c->queue.push_back(ev);
}

// Schluessel je Richtung: HKDF(Beitrittsschluessel, Salz = Zufall Gast | Zufall Host)
bool deriveKeys(const u8 key[LN_KEY_BYTES], const u8 cn[16], const u8 sn[16], u8 c2s[32], u8 s2c[32]) {
    u8 salt[32], okm[64];
    std::memcpy(salt, cn, 16), std::memcpy(salt + 16, sn, 16);
    static const char info[] = "LIMN v2 keys";
    if (!lim::netsec::hkdfSha256(key, LN_KEY_BYTES, salt, 32, (const u8*)info, sizeof(info) - 1, okm, 64)) return false;
    std::memcpy(c2s, okm, 32), std::memcpy(s2c, okm + 32, 32);
    SecureZeroMemory(okm, sizeof(okm));
    return true;
}

bool proofMac(const u8 key[LN_KEY_BYTES], const u8 cn[16], const u8 sn[16], const char* name, size_t nameLen, u8 out[16]) {
    u8 msg[10 + 32 + LN_NAME_MAX];
    std::memcpy(msg, "LIMN-proof", 10);
    std::memcpy(msg + 10, cn, 16), std::memcpy(msg + 26, sn, 16);
    std::memcpy(msg + 42, name, nameLen);
    u8 full[32];
    if (!lim::netsec::hmacSha256(key, LN_KEY_BYTES, msg, 42 + nameLen, full)) return false;
    std::memcpy(out, full, 16);
    return true;
}

void nonceFor(u8 out[12], int dir, int channel, u64 ctr) {
    out[0] = (u8)dir, out[1] = (u8)channel, out[2] = 0, out[3] = 0;
    put64(out + 4, ctr);
}

// verschluesselt senden (dir 0: Gast -> Host, 1: Host -> Gast)
bool sendData(Peer& p, int dir, int channel, const u8* data, int len) {
    if (p.phase != Peer::Open || !p.tx.ready() || len < 0 || len > LN_MAX_PAYLOAD) return false;
    u8 buf[kMaxFrame];
    const u64 ctr = ++p.txCtr[channel];
    buf[0] = F_DATA;
    put64(buf + 1, ctr);
    u8 nonce[12];
    nonceFor(nonce, dir, channel, ctr);
    const u8 aad[2] = {F_DATA, (u8)channel};
    if (!p.tx.seal(nonce, aad, 2, data, (size_t)len, buf + kDataHead)) return false;
    sendRaw(p, channel, buf, kDataHead + (size_t)len + lim::netsec::kTag);
    ++p.st.sent;
    return true;
}

// entschluesseln und pruefen; false = ungueltig
bool openData(Peer& p, int dir, int channel, const u8* in, size_t n, u8* out, int& outLen) {
    if (n < kDataHead + lim::netsec::kTag || n > kMaxFrame || in[0] != F_DATA || channel < 0 || channel > 1) return false;
    const u64 ctr = get64(in + 1);
    // keine Wiederholung: je Kanal strikt steigend (zuverlaessig: geordnet; unzuverlaessig: ENet verwirft Aelteres)
    if (p.rxAny[channel] && ctr <= p.rxCtr[channel]) return false;
    if (ctr == 0) return false;
    u8 nonce[12];
    nonceFor(nonce, dir, channel, ctr);
    const u8 aad[2] = {F_DATA, (u8)channel};
    const size_t m = n - kDataHead - lim::netsec::kTag;
    if (!p.rx.open(nonce, aad, 2, in + kDataHead, m, out)) return false;
    p.rxAny[channel] = true;
    p.rxCtr[channel] = ctr;
    outLen = (int)m;
    return true;
}

void refuse(LnContext* c, Peer& p, int reason) {
    const u8 r[2] = {F_REFUSE, (u8)reason};
    sendRaw(p, LN_RELIABLE, r, 2);
    if (p.ep) enet_peer_disconnect_later(p.ep, (enet_uint32)reason);
    p.phase = Peer::Closing;  // alles Weitere wird ignoriert (kein zweiter Beweis auf derselben Verbindung)
    (void)c;
}

int slotOf(ENetPeer* ep) { return (int)(intptr_t)ep->data - 1; }

void freePeer(LnContext* c, int slot, int reason, bool notify) {
    Peer& p = c->peers[(size_t)slot];
    if (notify && p.phase == Peer::Open) pushEvent(c, LN_EV_DISCONNECTED, c->server ? slot : 0, 0, reason, nullptr, 0, nullptr);
    if (p.ep) p.ep->data = nullptr;
    p = Peer{};
}

// Fehlversuch einer Adresse (falscher Beweis, Muell oder Zeitlimit im Handshake); nach banFails gesperrt
void noteFail(LnContext* c, u32 addr, double now) {
    Ban& b = c->bans[addr];
    if (now - b.window > 60.0) b.fails = 0, b.window = now;
    if (++b.fails >= c->lim.banFails) b.until = now + c->lim.banMs / 1000.0;
}

void invalid(LnContext* c, int slot, double now) {
    Peer& p = c->peers[(size_t)slot];
    ++p.st.dropped;
    if (++p.invalid > c->lim.maxInvalid && p.ep) {
        if (p.halfOpen()) noteFail(c, p.addr, now);
        enet_peer_disconnect_now(p.ep, LN_R_PROTOCOL);
        freePeer(c, slot, LN_R_PROTOCOL, true);
    }
}

// --- Host --------------------------------------------------------------------------------------------------------
void serverReceive(LnContext* c, ENetPeer* ep, int channel, const u8* d, size_t n, double now) {
    const int slot = ep->data ? slotOf(ep) : -1;
    if (slot < 0) return;
    Peer& p = c->peers[(size_t)slot];
    if (p.phase == Peer::Closing) return;
    if (n > kMaxFrame || n == 0) return invalid(c, slot, now);
    switch (p.phase) {
        case Peer::WaitHello: {
            if (n != kHelloLen || d[0] != F_HELLO || get32(d + 1) != kMagic) return invalid(c, slot, now);
            const unsigned ver = (unsigned)d[5] | ((unsigned)d[6] << 8);
            if (ver != LN_VERSION) return refuse(c, p, LN_R_VERSION);
            std::memcpy(p.cn, d + 7, 16);
            const size_t nl = d[23];
            if (nl > LN_NAME_MAX || !validName((const char*)d + 24, nl)) return refuse(c, p, LN_R_PROTOCOL);
            std::memcpy(p.name, d + 24, nl);
            p.name[nl] = 0, p.nameLen = nl;
            if (!lim::netsec::randomBytes(p.sn, 16) || !lim::netsec::randomBytes((u8*)&p.cookie, 4)) return refuse(c, p, LN_R_PROTOCOL);
            u8 ch[kChallengeLen];
            ch[0] = F_CHALLENGE;
            put32(ch + 1, kMagic);
            std::memcpy(ch + 5, p.sn, 16);
            put32(ch + 21, p.cookie);
            sendRaw(p, LN_RELIABLE, ch, sizeof(ch));
            p.phase = Peer::WaitProof;
            return;
        }
        case Peer::WaitProof: {
            if (n != kProofLen || d[0] != F_PROOF || get32(d + 1) != kMagic || get32(d + 5) != p.cookie) return invalid(c, slot, now);
            u8 want[16];
            if (!proofMac(c->key, p.cn, p.sn, p.name, p.nameLen, want) || !lim::netsec::equalConstTime(want, d + 9, 16)) {
                noteFail(c, p.addr, now);
                return refuse(c, p, LN_R_BADKEY);
            }
            int open = 0;
            for (const Peer& q : c->peers) open += q.phase == Peer::Open ? 1 : 0;
            if (open >= c->lim.maxPeers) return refuse(c, p, LN_R_FULL);
            u8 c2s[32], s2c[32];
            if (!deriveKeys(c->key, p.cn, p.sn, c2s, s2c) || !p.rx.init(c2s) || !p.tx.init(s2c)) return refuse(c, p, LN_R_PROTOCOL);
            SecureZeroMemory(c2s, 32), SecureZeroMemory(s2c, 32);
            p.phase = Peer::Open;
            pushEvent(c, LN_EV_CONNECTED, slot, 0, 0, nullptr, 0, p.name);
            return;
        }
        case Peer::Open: {
            u8 out[LN_MAX_PAYLOAD];
            int len = 0;
            if (!openData(p, 0, channel, d, n, out, len)) return invalid(c, slot, now);
            if (!p.bucket.take(now)) {
                ++p.st.dropped;
                if (++p.rateDrops > 200) {  // dauerhaft ueber der Rate: trennen
                    enet_peer_disconnect_now(p.ep, LN_R_RATE);
                    freePeer(c, slot, LN_R_RATE, true);
                }
                return;
            }
            p.rateDrops = 0;
            ++p.st.received;
            pushEvent(c, LN_EV_MESSAGE, slot, channel, 0, out, len, nullptr);
            return;
        }
        default: return;
    }
}

void serverConnect(LnContext* c, ENetPeer* ep, double now) {
    const u32 addr = ep->address.host;
    auto b = c->bans.find(addr);
    if (b != c->bans.end() && b->second.until > now) {
        enet_peer_disconnect_now(ep, LN_R_BANNED);
        return;
    }
    int half = 0, halfHere = 0, freeSlot = -1;
    for (size_t i = 0; i < c->peers.size(); ++i) {
        const Peer& p = c->peers[i];
        if (p.halfOpen()) ++half, halfHere += p.addr == addr ? 1 : 0;
        if (p.phase == Peer::Free && freeSlot < 0) freeSlot = (int)i;
    }
    if (freeSlot < 0 || half >= c->lim.maxHalfOpen || halfHere >= kHalfOpenPerAddr) {
        enet_peer_disconnect_now(ep, LN_R_FULL);
        return;
    }
    Peer& p = c->peers[(size_t)freeSlot];
    p = Peer{};
    p.phase = Peer::WaitHello;
    p.ep = ep;
    p.addr = addr;
    p.opened = now;
    p.bucket.rate = c->lim.rate, p.bucket.burst = c->lim.burst, p.bucket.tokens = c->lim.burst;
    ep->data = (void*)(intptr_t)(freeSlot + 1);
    enet_peer_timeout(ep, 0, (enet_uint32)std::max(1000, c->lim.silenceMs / 2), (enet_uint32)c->lim.silenceMs);
}

// --- Gast ----------------------------------------------------------------------------------------------------------
void clientReceive(LnContext* c, int channel, const u8* d, size_t n) {
    Peer& p = c->peers[0];
    if (n == 0 || n > kMaxFrame) return;
    if (d[0] == F_REFUSE && n == 2) {
        const int reason = d[1];
        p.phase = Peer::Free;
        pushEvent(c, LN_EV_DISCONNECTED, 0, 0, reason, nullptr, 0, nullptr);
        if (p.ep) enet_peer_disconnect_now(p.ep, (enet_uint32)reason);
        p.ep = nullptr;
        return;
    }
    if (p.phase == Peer::WaitHello) {  // warten auf CHALLENGE
        if (n != kChallengeLen || d[0] != F_CHALLENGE || get32(d + 1) != kMagic) return;
        std::memcpy(p.sn, d + 5, 16);
        const u32 cookie = get32(d + 21);
        u8 pr[kProofLen];
        pr[0] = F_PROOF;
        put32(pr + 1, kMagic);
        put32(pr + 5, cookie);
        if (!proofMac(c->key, p.cn, p.sn, c->myName.c_str(), c->myName.size(), pr + 9)) return;
        u8 c2s[32], s2c[32];
        if (!deriveKeys(c->key, p.cn, p.sn, c2s, s2c) || !p.tx.init(c2s) || !p.rx.init(s2c)) return;
        SecureZeroMemory(c2s, 32), SecureZeroMemory(s2c, 32);
        sendRaw(p, LN_RELIABLE, pr, sizeof(pr));
        p.phase = Peer::Open;
        return;
    }
    if (p.phase == Peer::Open) {
        u8 out[LN_MAX_PAYLOAD];
        int len = 0;
        if (!openData(p, 1, channel, d, n, out, len)) {
            ++p.st.dropped;
            return;
        }
        if (!p.announced) {  // die erste gueltige Nachricht beweist: der Host kennt den Schluessel
            p.announced = true;
            pushEvent(c, LN_EV_CONNECTED, 0, 0, 0, nullptr, 0, nullptr);
        }
        ++p.st.received;
        pushEvent(c, LN_EV_MESSAGE, 0, channel, 0, out, len, nullptr);
    }
}

void pump(LnContext* c) {
    const double now = nowSec();
    ENetEvent ev;
    for (int guard = 0; guard < 512 && enet_host_service(c->host, &ev, 0) > 0; ++guard) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT:
                if (c->server) {
                    serverConnect(c, ev.peer, now);
                } else {  // Gast: HELLO senden
                    Peer& p = c->peers[0];
                    p.ep = ev.peer;
                    u8 h[kHelloLen] = {};
                    h[0] = F_HELLO;
                    put32(h + 1, kMagic);
                    h[5] = (u8)(LN_VERSION & 0xFF), h[6] = (u8)(LN_VERSION >> 8);
                    std::memcpy(h + 7, p.cn, 16);
                    h[23] = (u8)c->myName.size();
                    std::memcpy(h + 24, c->myName.data(), c->myName.size());
                    sendRaw(p, LN_RELIABLE, h, sizeof(h));
                    p.phase = Peer::WaitHello;
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (c->server) serverReceive(c, ev.peer, ev.channelID, ev.packet->data, ev.packet->dataLength, now);
                else clientReceive(c, ev.channelID, ev.packet->data, ev.packet->dataLength);
                enet_packet_destroy(ev.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT: {
                const int reason = ev.data ? (int)ev.data : LN_R_TIMEOUT;
                if (c->server) {
                    if (ev.peer->data) freePeer(c, slotOf(ev.peer), reason, true);
                } else {
                    Peer& p = c->peers[0];
                    const bool was = p.phase != Peer::Free;
                    const bool open = p.announced;
                    p = Peer{};
                    if (was) pushEvent(c, LN_EV_DISCONNECTED, 0, 0, open ? reason : (reason == LN_R_TIMEOUT ? LN_R_UNREACHABLE : reason), nullptr, 0, nullptr);
                }
                break;
            }
            default: break;
        }
    }
    // Zeitlimit im Handshake
    if (c->server)
        for (size_t i = 0; i < c->peers.size(); ++i) {
            Peer& p = c->peers[i];
            if (p.halfOpen() && now - p.opened > c->lim.handshakeMs / 1000.0) {
                if (p.phase != Peer::Closing) noteFail(c, p.addr, now);
                if (p.ep) enet_peer_disconnect_now(p.ep, LN_R_TIMEOUT);
                freePeer(c, (int)i, LN_R_TIMEOUT, false);
            }
        }
}

}  // namespace

extern "C" {

LN_API LnContext* ln_host(uint16_t port, const uint8_t key[LN_KEY_BYTES], const LnLimits* lim) {
    if (!key || !enetUp()) return nullptr;
    auto* c = new LnContext;
    c->server = true;
    std::memcpy(c->key, key, LN_KEY_BYTES);
    c->lim = lim ? *lim : LnLimits{3, 8, 120.0, 60.0, 8, 5000, 10000, 5, 60000};
    c->lim.maxPeers = std::clamp(c->lim.maxPeers, 1, 16);
    c->lim.maxHalfOpen = std::clamp(c->lim.maxHalfOpen, 1, 32);
    c->peers.resize((size_t)(c->lim.maxPeers + c->lim.maxHalfOpen));
    ENetAddress a{};
    a.host = ENET_HOST_ANY;
    a.port = port;
    c->host = enet_host_create(&a, c->peers.size() + kEnetHeadroom, 2, 0, 0);
    if (!c->host) {
        delete c;
        enetDown();
        return nullptr;
    }
    return c;
}

LN_API LnContext* ln_connect(const char* hostName, uint16_t port, const uint8_t key[LN_KEY_BYTES], const char* name) {
    if (!hostName || !key || !name || !validName(name, std::strlen(name)) || !enetUp()) return nullptr;
    auto* c = new LnContext;
    std::memcpy(c->key, key, LN_KEY_BYTES);
    c->myName = name;
    c->lim = LnLimits{1, 1, 1e9, 1e9, 1000000, 5000, 10000, 5, 60000};
    c->peers.resize(1);
    c->host = enet_host_create(nullptr, 1, 2, 0, 0);
    ENetAddress a{};
    if (!c->host || enet_address_set_host(&a, hostName) != 0) {
        if (c->host) enet_host_destroy(c->host);
        delete c;
        enetDown();
        return nullptr;
    }
    a.port = port;
    Peer& p = c->peers[0];
    lim::netsec::randomBytes(p.cn, 16);
    p.ep = enet_host_connect(c->host, &a, 2, 0);
    if (!p.ep) {
        enet_host_destroy(c->host);
        delete c;
        enetDown();
        return nullptr;
    }
    p.phase = Peer::WaitHello;
    enet_peer_timeout(p.ep, 0, 5000, 10000);
    return c;
}

LN_API int ln_poll(LnContext* c, LnEvent* ev) {
    if (!c || !ev) return 0;
    if (c->queue.empty()) pump(c);
    if (c->queue.empty()) return 0;
    *ev = c->queue.front();
    c->queue.pop_front();
    return 1;
}

LN_API int ln_send(LnContext* c, int peer, int channel, const uint8_t* data, int len) {
    if (!c || channel < 0 || channel > 1 || !data) return 0;
    if (c->server) {
        if (peer < 0 || peer >= (int)c->peers.size()) return 0;
        return sendData(c->peers[(size_t)peer], 1, channel, data, len) ? 1 : 0;
    }
    return sendData(c->peers[0], 0, channel, data, len) ? 1 : 0;
}

LN_API void ln_drop(LnContext* c, int peer, int reason, int ban) {
    if (!c || !c->server || peer < 0 || peer >= (int)c->peers.size()) return;
    Peer& p = c->peers[(size_t)peer];
    if (p.phase == Peer::Free) return;
    if (ban) c->bans[p.addr].until = 1e300;  // fuer die ganze Sitzung
    if (p.ep) enet_peer_disconnect(p.ep, (enet_uint32)reason);
    freePeer(c, peer, reason, false);
}

LN_API int ln_stats(LnContext* c, int peer, LnStats* out) {
    if (!c || !out) return 0;
    const size_t i = c->server ? (size_t)peer : 0;
    if (i >= c->peers.size() || !c->peers[i].ep) return 0;
    Peer& p = c->peers[i];
    *out = p.st;
    out->rttMs = p.ep->roundTripTime, out->rttVarMs = p.ep->roundTripTimeVariance;
    out->loss = (float)p.ep->packetLoss / (float)ENET_PEER_PACKET_LOSS_SCALE;
    return 1;
}

LN_API uint16_t ln_port(LnContext* c) {
    if (!c || !c->host) return 0;
    ENetAddress a{};
    return enet_socket_get_address(c->host->socket, &a) == 0 ? a.port : c->host->address.port;  // Port 0: vom System vergeben
}

LN_API void ln_close(LnContext* c) {
    if (!c) return;
    if (c->host) {
        for (Peer& p : c->peers)
            if (p.ep) enet_peer_disconnect_now(p.ep, LN_R_NORMAL);
        enet_host_flush(c->host);
        enet_host_destroy(c->host);
    }
    delete c;
    enetDown();
}

LN_API int ln_random(uint8_t* out, int n) { return out && n > 0 && lim::netsec::randomBytes(out, (size_t)n) ? 1 : 0; }
LN_API int ln_version(void) { return LN_VERSION; }

}  // extern "C"
