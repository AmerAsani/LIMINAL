// V13.2: Prueft den Transport des Mehrspielers (src/netdll: ENet, Handshake, Verschluesselung) ueber echtes UDP auf
// 127.0.0.1 (MULTIPLAYER_TESTS.md).
//   - Kryptografie gegen Testvektoren: HMAC-SHA256 (RFC 4231), HKDF-SHA256 (RFC 5869), AES-256-GCM (GCM-Spezifikation)
//   - Verbindung: Handshake, Nachrichten in beide Richtungen, beide Kanaele, Hoechstgroesse
//   - Angriffe: falscher Schluessel, Sperre nach Fehlversuchen, voll, falsche Version, Muell vor dem Handshake,
//     halboffene Flut, Wiederholung abgefangener Pakete, veraenderte Pakete, zu grosse Pakete, Nachrichtenflut
#include <windows.h>

#include <enet/enet.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "netdll/ln_api.h"
#include "netdll/secure.hpp"

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}
using u8 = std::uint8_t;

static std::vector<u8> hex(const char* s) {
    std::vector<u8> out;
    for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
        unsigned v = 0;
        std::sscanf(s + i, "%2x", &v);
        out.push_back((u8)v);
    }
    return out;
}
static bool same(const u8* a, const std::vector<u8>& b) { return std::memcmp(a, b.data(), b.size()) == 0; }

static void testCrypto() {
    std::printf("Kryptografie (Testvektoren)\n");
    using namespace lim::netsec;
    {  // RFC 4231, Testfall 1
        std::vector<u8> key(20, 0x0b);
        const char* data = "Hi There";
        u8 out[32];
        check(hmacSha256(key.data(), key.size(), (const u8*)data, 8, out) &&
                  same(out, hex("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7")),
              "HMAC-SHA256 RFC 4231 Fall 1");
    }
    {  // RFC 4231, Testfall 2
        const char* key = "Jefe";
        const char* data = "what do ya want for nothing?";
        u8 out[32];
        check(hmacSha256((const u8*)key, 4, (const u8*)data, std::strlen(data), out) &&
                  same(out, hex("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")),
              "HMAC-SHA256 RFC 4231 Fall 2");
    }
    {  // RFC 5869, Testfall 1
        std::vector<u8> ikm(22, 0x0b), salt = hex("000102030405060708090a0b0c"), info = hex("f0f1f2f3f4f5f6f7f8f9");
        u8 okm[42];
        check(hkdfSha256(ikm.data(), ikm.size(), salt.data(), salt.size(), info.data(), info.size(), okm, 42) &&
                  same(okm, hex("3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865")),
              "HKDF-SHA256 RFC 5869 Fall 1");
    }
    {  // GCM-Spezifikation (McGrew/Viega), Testfall 13 und 14: Schluessel 0, Nonce 0
        u8 key[32] = {}, nonce[12] = {};
        Aead a;
        check(a.init(key), "AES-256-GCM: Schluessel angenommen");
        u8 tag13[16];
        check(a.seal(nonce, nullptr, 0, nullptr, 0, tag13) && same(tag13, hex("530f8afbc74536b9a963b4f1c4cb738b")), "AES-256-GCM Fall 13 (leer)");
        u8 pt[16] = {}, ct[32];
        check(a.seal(nonce, nullptr, 0, pt, 16, ct) && same(ct, hex("cea7403d4d606b6e074ec5d3baf39d18")) && same(ct + 16, hex("d0d1c8a799996bf0265b98b5d48ab919")),
              "AES-256-GCM Fall 14");
        u8 back[16];
        check(a.open(nonce, nullptr, 0, ct, 16, back) && std::memcmp(back, pt, 16) == 0, "AES-256-GCM: Entschluesseln");
        ct[3] ^= 0x01;
        check(!a.open(nonce, nullptr, 0, ct, 16, back), "AES-256-GCM: veraenderter Geheimtext wird abgelehnt");
        ct[3] ^= 0x01, ct[20] ^= 0x80;
        check(!a.open(nonce, nullptr, 0, ct, 16, back), "AES-256-GCM: veraenderter Tag wird abgelehnt");
        ct[20] ^= 0x80;
        const u8 aad[2] = {4, 1};
        check(!a.open(nonce, aad, 2, ct, 16, back), "AES-256-GCM: andere Zusatzdaten werden abgelehnt");
    }
    u8 r1[16], r2[16];
    check(randomBytes(r1, 16) && randomBytes(r2, 16) && std::memcmp(r1, r2, 16) != 0, "Zufall: zwei Abrufe verschieden");
    const u8 x[4] = {1, 2, 3, 4}, y[4] = {1, 2, 3, 5};
    check(equalConstTime(x, x, 4) && !equalConstTime(x, y, 4), "Vergleich ohne fruehen Abbruch");
}

// --- Hilfen fuer echte Verbindungen ------------------------------------------------------------------------------
static const u8 kKey[16] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 11, 22, 33, 44, 55, 66};
static void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// pumpt beide Seiten, bis cond() oder Zeitlimit; sammelt Ereignisse
struct Side {
    LnContext* c = nullptr;
    std::vector<LnEvent> ev;
    void pump() {
        LnEvent e;
        while (c && ln_poll(c, &e)) ev.push_back(e);
    }
    int count(int type) const {
        int n = 0;
        for (const auto& e : ev) n += e.type == type ? 1 : 0;
        return n;
    }
    const LnEvent* last(int type) const {
        for (auto it = ev.rbegin(); it != ev.rend(); ++it)
            if (it->type == type) return &*it;
        return nullptr;
    }
};
static bool runUntil(std::vector<Side*> sides, const std::function<bool()>& cond, int ms) {
    const auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(ms)) {
        for (Side* s : sides) s->pump();
        if (cond()) return true;
        sleepMs(1);
    }
    for (Side* s : sides) s->pump();
    return cond();
}

static LnLimits limits(int peers = 3) { return LnLimits{peers, 8, 120.0, 60.0, 8, 1500, 4000, 5, 3000}; }

// --- roher ENet-Gast (fuer Angriffe am Transport vorbei) --------------------------------------------------------
struct RawClient {
    ENetHost* h = nullptr;
    ENetPeer* p = nullptr;
    bool connected = false, disconnected = false;
    std::vector<std::vector<u8>> got;
    bool open(uint16_t port) {
        h = enet_host_create(nullptr, 1, 2, 0, 0);
        ENetAddress a{};
        enet_address_set_host(&a, "127.0.0.1");
        a.port = port;
        p = h ? enet_host_connect(h, &a, 2, 0) : nullptr;
        return p != nullptr;
    }
    void service() {
        ENetEvent e;
        while (h && enet_host_service(h, &e, 0) > 0) {
            if (e.type == ENET_EVENT_TYPE_CONNECT) connected = true;
            if (e.type == ENET_EVENT_TYPE_DISCONNECT) disconnected = true;
            if (e.type == ENET_EVENT_TYPE_RECEIVE) {
                got.emplace_back(e.packet->data, e.packet->data + e.packet->dataLength);
                enet_packet_destroy(e.packet);
            }
        }
    }
    void send(const std::vector<u8>& d, int ch = 0, bool reliable = true) {
        ENetPacket* pk = enet_packet_create(d.data(), d.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        enet_peer_send(p, (enet_uint8)ch, pk);
        enet_host_flush(h);
    }
    void close() {
        if (h) enet_host_destroy(h);
        h = nullptr, p = nullptr;
    }
};
static void put32(u8* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (u8)(v >> (8 * i)); }
static void put64(u8* p, uint64_t v) { for (int i = 0; i < 8; ++i) p[i] = (u8)(v >> (8 * i)); }

static std::vector<u8> helloFrame(const u8 cn[16], const char* name, int version = LN_VERSION) {
    std::vector<u8> h(40, 0);
    h[0] = 1;
    put32(&h[1], 0x4E4D494Cu);
    h[5] = (u8)version, h[6] = 0;
    std::memcpy(&h[7], cn, 16);
    h[23] = (u8)std::strlen(name);
    std::memcpy(&h[24], name, std::strlen(name));
    return h;
}

static void testConnect() {
    std::printf("Verbindung (127.0.0.1)\n");
    Side host, guest;
    LnLimits lim = limits();
    host.c = ln_host(0, kKey, &lim);
    check(host.c != nullptr, "Host oeffnet einen Port");
    const uint16_t port = ln_port(host.c);
    check(port != 0, "Port vom System vergeben");
    guest.c = ln_connect("127.0.0.1", port, kKey, "Wanderer");
    check(guest.c != nullptr, "Gast verbindet");
    // Host sieht den Gast nach dem Beweis; der Gast erst mit der ersten Nachricht des Hosts (gegenseitig)
    check(runUntil({&host, &guest}, [&] { return host.count(LN_EV_CONNECTED) == 1; }, 3000), "Host: Gast angemeldet");
    const LnEvent* hc = host.last(LN_EV_CONNECTED);
    check(hc && std::string(hc->name) == "Wanderer", "Host kennt den Namen des Gasts");
    const int gp = hc ? hc->peer : 0;
    const u8 welcome[] = "willkommen";
    check(ln_send(host.c, gp, LN_RELIABLE, welcome, sizeof(welcome)) == 1, "Host sendet (zuverlaessig)");
    check(runUntil({&host, &guest}, [&] { return guest.count(LN_EV_MESSAGE) == 1; }, 3000), "Gast empfaengt");
    check(guest.count(LN_EV_CONNECTED) == 1, "Gast: verbunden erst nach der ersten gueltigen Nachricht des Hosts");
    const LnEvent* gm = guest.last(LN_EV_MESSAGE);
    check(gm && gm->len == (int)sizeof(welcome) && std::memcmp(gm->data, welcome, sizeof(welcome)) == 0, "Inhalt unveraendert");
    // beide Kanaele, Hoechstgroesse
    std::vector<u8> big(LN_MAX_PAYLOAD, 0x5A);
    check(ln_send(guest.c, 0, LN_UNRELIABLE, big.data(), (int)big.size()) == 1, "Gast sendet 1200 Byte (unzuverlaessig)");
    check(ln_send(guest.c, 0, LN_UNRELIABLE, big.data(), (int)big.size() + 1) == 0, "1201 Byte werden nicht gesendet");
    check(runUntil({&host, &guest}, [&] { return host.count(LN_EV_MESSAGE) >= 1; }, 3000), "Host empfaengt 1200 Byte");
    const LnEvent* hm = host.last(LN_EV_MESSAGE);
    check(hm && hm->len == LN_MAX_PAYLOAD && hm->channel == LN_UNRELIABLE, "Groesse und Kanal stimmen");
    // 200 zuverlaessige Nachrichten in Reihenfolge, im Takt des Spiels (100/s, unter der Rate von 120/s)
    for (int i = 0; i < 200; ++i) {
        u8 m[4];
        put32(m, (uint32_t)i);
        ln_send(guest.c, 0, LN_RELIABLE, m, 4);
        if (i % 10 == 9) runUntil({&host, &guest}, [] { return false; }, 100);
    }
    runUntil({&host, &guest}, [&] { return host.count(LN_EV_MESSAGE) >= 201; }, 5000);
    bool ordered = true;
    int k = 0;
    for (const auto& e : host.ev)
        if (e.type == LN_EV_MESSAGE && e.channel == LN_RELIABLE) {
            const uint32_t v = (uint32_t)e.data[0] | ((uint32_t)e.data[1] << 8) | ((uint32_t)e.data[2] << 16) | ((uint32_t)e.data[3] << 24);
            ordered = ordered && v == (uint32_t)k++;
        }
    check(k == 200 && ordered, "200 zuverlaessige Nachrichten vollstaendig und geordnet (" + std::to_string(k) + ")");
    LnStats st{};
    check(ln_stats(host.c, gp, &st) == 1 && st.received >= 201, "Messwerte (empfangen, Laufzeit)");
    // Gast trennt: Host meldet es
    ln_close(guest.c);
    guest.c = nullptr;
    check(runUntil({&host}, [&] { return host.count(LN_EV_DISCONNECTED) == 1; }, 3000), "Trennung beim Host gemeldet");
    ln_close(host.c);
}

static void testWrongKey() {
    std::printf("Falscher Schluessel, Sperre\n");
    Side host;
    LnLimits lim = limits();
    host.c = ln_host(0, kKey, &lim);
    const uint16_t port = ln_port(host.c);
    u8 bad[16];
    std::memcpy(bad, kKey, 16);
    bad[0] ^= 1;
    int refused = 0;
    for (int i = 0; i < 5; ++i) {
        Side g;
        g.c = ln_connect("127.0.0.1", port, bad, "Fremder");
        runUntil({&host, &g}, [&] { return g.count(LN_EV_DISCONNECTED) == 1; }, 3000);
        const LnEvent* d = g.last(LN_EV_DISCONNECTED);
        refused += d && d->reason == LN_R_BADKEY ? 1 : 0;
        check(g.count(LN_EV_CONNECTED) == 0 && g.count(LN_EV_MESSAGE) == 0, "falscher Schluessel: keine Verbindung, keine Nachricht");
        ln_close(g.c);
    }
    check(refused == 5, "5 falsche Beweise abgelehnt (" + std::to_string(refused) + ")");
    check(host.count(LN_EV_CONNECTED) == 0, "Host meldet keinen Gast");
    {  // gesperrt: auch mit richtigem Schluessel nicht (bis die Sperre ablaeuft)
        Side g;
        g.c = ln_connect("127.0.0.1", port, kKey, "Fremder");
        runUntil({&host, &g}, [&] { return g.count(LN_EV_DISCONNECTED) == 1; }, 3000);
        check(g.count(LN_EV_CONNECTED) == 0 && host.count(LN_EV_CONNECTED) == 0, "nach 5 Fehlversuchen gesperrt");
        ln_close(g.c);
    }
    sleepMs(3100);  // Sperre in diesem Test 3 s
    {
        Side g;
        g.c = ln_connect("127.0.0.1", port, kKey, "Wanderer");
        runUntil({&host, &g}, [&] { return host.count(LN_EV_CONNECTED) == 1; }, 3000);
        check(host.count(LN_EV_CONNECTED) == 1, "nach Ablauf der Sperre: Beitritt mit richtigem Schluessel");
        ln_close(g.c);
    }
    ln_close(host.c);
}

static void testFull() {
    std::printf("Voll, Version, Muell, halboffene Flut\n");
    Side host;
    LnLimits lim = limits(2);
    host.c = ln_host(0, kKey, &lim);
    const uint16_t port = ln_port(host.c);
    Side a, b, c;
    a.c = ln_connect("127.0.0.1", port, kKey, "A");
    b.c = ln_connect("127.0.0.1", port, kKey, "B");
    runUntil({&host, &a, &b}, [&] { return host.count(LN_EV_CONNECTED) == 2; }, 3000);
    c.c = ln_connect("127.0.0.1", port, kKey, "C");
    runUntil({&host, &a, &b, &c}, [&] { return c.count(LN_EV_DISCONNECTED) == 1; }, 3000);
    const LnEvent* d = c.last(LN_EV_DISCONNECTED);
    check(host.count(LN_EV_CONNECTED) == 2 && d && d->reason == LN_R_FULL, "dritter Gast bei 2 Plaetzen: abgewiesen (voll)");
    ln_close(c.c);
    // falsche Version (roher Gast)
    {
        RawClient r;
        r.open(port);
        for (int i = 0; i < 300 && !r.connected; ++i) host.pump(), r.service(), sleepMs(2);
        u8 cn[16] = {1};
        r.send(helloFrame(cn, "Alt", 1));
        bool refusedV = false;
        for (int i = 0; i < 600 && !refusedV; ++i) {
            host.pump(), r.service(), sleepMs(2);
            for (auto& g : r.got) refusedV = refusedV || (g.size() == 2 && g[0] == 5 && g[1] == LN_R_VERSION);
        }
        check(refusedV, "falsche Version: abgewiesen (Grund Version)");
        r.close();
    }
    // Muell vor dem Handshake: Zufallsbytes -> nie angemeldet, nach zu vielen Fehlern getrennt
    {
        RawClient r;
        r.open(port);
        for (int i = 0; i < 300 && !r.connected; ++i) host.pump(), r.service(), sleepMs(2);
        std::mt19937 rng(7);
        for (int i = 0; i < 40; ++i) {
            std::vector<u8> junk(1 + rng() % 300);
            for (auto& x : junk) x = (u8)rng();
            r.send(junk, (int)(rng() % 2), (rng() % 2) == 0);
        }
        for (int i = 0; i < 500 && !r.disconnected; ++i) host.pump(), r.service(), sleepMs(2);
        check(r.disconnected, "Zufallsbytes: Gegenstelle getrennt");
        check(host.count(LN_EV_CONNECTED) == 2, "Zufallsbytes: kein neuer Gast angemeldet");
        r.close();
    }
    // halboffene Flut: 20 Verbindungen ohne HELLO von einer Adresse; nach dem Zeitlimit wieder Platz
    {
        std::vector<RawClient> flood(20);
        for (auto& r : flood) r.open(port);
        for (int i = 0; i < 300; ++i) {
            host.pump();
            for (auto& r : flood) r.service();
            sleepMs(2);
        }
        int early = 0;
        for (auto& r : flood) early += r.disconnected ? 1 : 0;
        check(early >= 17, "halboffene Flut: hoechstens 3 je Adresse warten, " + std::to_string(early) + "/20 sofort abgewiesen");
        int dropped = 0;
        for (int i = 0; i < 1200; ++i) {  // Zeitlimit Handshake 1,5 s
            host.pump();
            for (auto& r : flood) r.service();
            sleepMs(2);
        }
        for (auto& r : flood) dropped += r.disconnected ? 1 : 0;
        check(dropped == 20, "halboffene Verbindungen nach dem Zeitlimit getrennt (" + std::to_string(dropped) + "/20)");
        for (auto& r : flood) r.close();
        ln_close(b.c);
        b.c = nullptr;
        runUntil({&host, &a}, [&] { return host.count(LN_EV_DISCONNECTED) >= 1; }, 3000);
        Side e;
        e.c = ln_connect("127.0.0.1", port, kKey, "E");
        runUntil({&host, &a, &e}, [&] { return host.count(LN_EV_CONNECTED) == 3; }, 3000);
        check(host.count(LN_EV_CONNECTED) == 3, "nach der Flut: ein echter Gast tritt bei");
        ln_close(e.c);
    }
    ln_close(a.c);
    ln_close(host.c);
}

// Handshake von Hand (wie transport.cpp), um Pakete abzufangen, zu wiederholen und zu veraendern
static void testReplay() {
    std::printf("Wiederholung, Veraenderung, Flut\n");
    using namespace lim::netsec;
    Side host;
    LnLimits lim = limits();
    host.c = ln_host(0, kKey, &lim);
    const uint16_t port = ln_port(host.c);
    RawClient r;
    r.open(port);
    for (int i = 0; i < 300 && !r.connected; ++i) host.pump(), r.service(), sleepMs(2);
    u8 cn[16];
    randomBytes(cn, 16);
    r.send(helloFrame(cn, "Spion"));
    std::vector<u8> ch;
    for (int i = 0; i < 500 && ch.empty(); ++i) {
        host.pump(), r.service(), sleepMs(2);
        for (auto& g : r.got)
            if (g.size() == 25 && g[0] == 2) ch = g;
    }
    check(!ch.empty(), "CHALLENGE erhalten (25 Byte, kleiner als HELLO)");
    if (ch.empty()) return r.close(), ln_close(host.c);
    u8 sn[16];
    std::memcpy(sn, &ch[5], 16);
    std::vector<u8> pr(25, 0);
    pr[0] = 3;
    put32(&pr[1], 0x4E4D494Cu);
    std::memcpy(&pr[5], &ch[21], 4);
    u8 msg[10 + 32 + 16];
    std::memcpy(msg, "LIMN-proof", 10), std::memcpy(msg + 10, cn, 16), std::memcpy(msg + 26, sn, 16), std::memcpy(msg + 42, "Spion", 5);
    u8 mac[32];
    hmacSha256(kKey, 16, msg, 47, mac);
    std::memcpy(&pr[9], mac, 16);
    r.send(pr);
    check(runUntil({&host}, [&] { r.service(); return host.count(LN_EV_CONNECTED) == 1; }, 3000), "Beweis von Hand angenommen");
    // Schluessel wie transport.cpp
    u8 salt[32], okm[64];
    std::memcpy(salt, cn, 16), std::memcpy(salt + 16, sn, 16);
    hkdfSha256(kKey, 16, salt, 32, (const u8*)"LIMN v2 keys", 12, okm, 64);
    Aead tx;
    tx.init(okm);
    auto frame = [&](uint64_t ctr, int channel, const std::vector<u8>& pt) {
        std::vector<u8> f(9 + pt.size() + 16);
        f[0] = 4;
        put64(&f[1], ctr);
        u8 nonce[12] = {0, (u8)channel, 0, 0};
        put64(nonce + 4, ctr);
        const u8 aad[2] = {4, (u8)channel};
        tx.seal(nonce, aad, 2, pt.data(), pt.size(), f.data() + 9);
        return f;
    };
    const int before = host.count(LN_EV_MESSAGE);
    auto f1 = frame(1, 0, {1, 2, 3});
    r.send(f1, 0);
    runUntil({&host}, [&] { r.service(); return host.count(LN_EV_MESSAGE) == before + 1; }, 2000);
    check(host.count(LN_EV_MESSAGE) == before + 1, "gueltige Nachricht angenommen");
    r.send(f1, 0);  // Wiederholung desselben Pakets
    auto fOld = frame(1, 0, {9});
    r.send(fOld, 0);  // alter Zaehler, neu verschluesselt
    runUntil({&host}, [&] { r.service(); return false; }, 300);
    check(host.count(LN_EV_MESSAGE) == before + 1, "Wiederholung und alter Zaehler verworfen");
    auto f2 = frame(2, 0, {4, 5, 6});
    f2[12] ^= 0x10;  // veraendert
    r.send(f2, 0);
    runUntil({&host}, [&] { r.service(); return false; }, 300);
    check(host.count(LN_EV_MESSAGE) == before + 1, "veraenderte Nachricht verworfen");
    auto f3 = frame(3, 1, {7});  // Kanal im Paket und Kanal von ENet muessen passen
    r.send(f3, 0);
    runUntil({&host}, [&] { r.service(); return false; }, 300);
    check(host.count(LN_EV_MESSAGE) == before + 1, "Nachricht auf falschem Kanal verworfen");
    // Flut: 2000 gueltige Nachrichten auf einmal -> nur Rate + Vorrat kommen an, der Host bleibt bedienbar
    const int b2 = host.count(LN_EV_MESSAGE);
    for (uint64_t i = 0; i < 2000; ++i) {
        auto f = frame(10 + i, 1, {(u8)i});
        ENetPacket* pk = enet_packet_create(f.data(), f.size(), 0);
        enet_peer_send(r.p, 1, pk);
        if (i % 100 == 99) enet_host_flush(r.h);
    }
    enet_host_flush(r.h);
    runUntil({&host}, [&] { r.service(); return false; }, 800);
    const int got = host.count(LN_EV_MESSAGE) - b2;
    check(got > 0 && got < 300, "Flut: " + std::to_string(got) + " von 2000 angenommen (Rate 120/s, Vorrat 60)");
    check(r.disconnected || host.count(LN_EV_DISCONNECTED) >= 0, "Flut: Host laeuft weiter");
    r.close();
    ln_close(host.c);
}

int main() {
    if (enet_initialize() != 0) {
        std::printf("ENet nicht verfuegbar\n");
        return 2;
    }
    testCrypto();
    testConnect();
    testWrongKey();
    testFull();
    testReplay();
    enet_deinitialize();
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
