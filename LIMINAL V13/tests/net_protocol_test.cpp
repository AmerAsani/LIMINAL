// V13: Prueft die Pruefschicht des Mehrspielerprotokolls (src/net) ohne Netzwerk.
//
//   net_protocol_test [fuzz-runden]
//
// MULTIPLAYER_BEDROHUNGSMODELL.md, Abschnitt 6, und MULTIPLAYER_TESTS.md:
//   - jede gueltige Nachricht geht unveraendert hin und zurueck (kanonische Kodierung)
//   - jede Regel hat einen Gegentest: Kennung, Typ, Rolle, Zustand, Laenge (jede Kuerzung, jedes Anhaengsel),
//     Version, Bereiche, Name, Tasten, Sitzungscode, Folgenummer, Rate
//   - Fuzzing: Zufallspakete und Bitfehler in gueltigen Paketen - nie ein Absturz, angenommen wird nur, was
//     kanonisch wieder genau dieselben Bytes ergibt
//   - Gegenstelle: Handshake, Zeitlimits, Trennung nach zu vielen Fehlern, Flut
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "net/protocol.hpp"

using namespace lim;
using namespace lim::net;

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}

static const std::array<u8, 16> kCode = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3};

static Message hello(const char* name) {
    Message m;
    m.type = MsgType::Hello;
    m.hello.code = kCode;
    m.hello.nameLen = (u8)std::strlen(name);
    for (size_t i = 0; i < std::strlen(name) && i < (size_t)kNameMax; ++i) m.hello.name[i] = name[i];
    return m;
}
static Message input(u32 seq, int mx = 50, int my = -20, int yaw = 100, int pitch = -40, u16 buttons = 0x05) {
    Message m;
    m.type = MsgType::Input;
    m.input = {seq, (i8)mx, (i8)my, (i16)yaw, (i16)pitch, buttons};
    return m;
}
static Message snapshot(int count) {
    Message m;
    m.type = MsgType::Snapshot;
    m.snapshot.tick = 777;
    m.snapshot.count = (u8)count;
    for (int i = 0; i < count; ++i) m.snapshot.players[(size_t)i] = {(u8)i, 1000 * i, -2000 * i, 1500, (i16)(300 * i), (u8)(i & 3)};
    return m;
}
static std::vector<u8> bytes(const Message& m) {
    std::vector<u8> b(kMaxPacket);
    b.resize(encode(m, b.data(), b.size()));
    return b;
}
static Reject dec(Role from, ConnState st, const std::vector<u8>& b, Message* out = nullptr) {
    Message m;
    Reject r = decode(from, st, b.data(), b.size(), m);
    if (out) *out = m;
    return r;
}

static void testRoundTrip() {
    std::printf("Hin und zurueck\n");
    struct Case {
        Message m;
        Role from;
        ConnState st;
        const char* name;
    };
    Message wel;
    wel.type = MsgType::Welcome;
    wel.welcome = {2, 0xDEADBEEF};
    Message bye;
    bye.type = MsgType::Bye;
    bye.bye.reason = 1;
    Message ping;
    ping.type = MsgType::Ping;
    ping.ping.nonce = 42;
    const Case cases[] = {{hello("Wanderer_7"), Role::Guest, ConnState::Handshake, "Hello"},
                          {wel, Role::Host, ConnState::Handshake, "Welcome"},
                          {input(9), Role::Guest, ConnState::Connected, "Input"},
                          {snapshot(0), Role::Host, ConnState::Connected, "Snapshot leer"},
                          {snapshot(4), Role::Host, ConnState::Connected, "Snapshot voll"},
                          {bye, Role::Guest, ConnState::Connected, "Bye"},
                          {ping, Role::Host, ConnState::Connected, "Ping"}};
    for (const Case& c : cases) {
        std::vector<u8> b = bytes(c.m);
        check(b.size() == wireSize(c.m) && !b.empty(), std::string(c.name) + ": Laenge");
        Message back;
        check(dec(c.from, c.st, b, &back) == Reject::None, std::string(c.name) + ": wird angenommen");
        check(bytes(back) == b, std::string(c.name) + ": dieselben Bytes zurueck");
    }
    check(bytes(input(1)).size() == 17 && bytes(hello("a")).size() == 40 && bytes(snapshot(2)).size() == 42, "feste Groessen 17/40/10+16n");
}

static void testRejects() {
    std::printf("Gegentests je Regel\n");
    const std::vector<u8> in = bytes(input(5));
    // jede Kuerzung und jedes Anhaengsel
    bool allShort = true, allLong = true;
    for (size_t k = 0; k < in.size(); ++k) {
        std::vector<u8> b(in.begin(), in.begin() + (long)k);
        if (dec(Role::Guest, ConnState::Connected, b) == Reject::None) allShort = false;
    }
    for (size_t extra = 1; extra < 64; ++extra) {
        std::vector<u8> b = in;
        b.resize(in.size() + extra, 0);
        if (dec(Role::Guest, ConnState::Connected, b) != Reject::BadLength) allLong = false;
    }
    check(allShort, "jede Kuerzung abgelehnt");
    check(allLong, "jedes Anhaengsel abgelehnt (Laenge)");
    std::vector<u8> big(kMaxPacket + 1, 0);
    check(dec(Role::Guest, ConnState::Connected, big) == Reject::TooLong, "ueber 1200 Byte abgelehnt");
    check(dec(Role::Guest, ConnState::Connected, {}) == Reject::TooShort, "leeres Paket abgelehnt");
    {
        std::vector<u8> b = in;
        b[0] ^= 0xFF;
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::BadMagic, "falsche Kennung");
        b = in;
        b[4] = 0;
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::UnknownType, "Typ 0 unbekannt");
        b[4] = 200;
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::UnknownType, "Typ 200 unbekannt");
    }
    // Rollen: der Gast sendet nie Zustaende, der Host nie Eingaben
    check(dec(Role::Guest, ConnState::Connected, bytes(snapshot(2))) == Reject::WrongRole, "Gast sendet Zustand -> falsche Rolle");
    check(dec(Role::Host, ConnState::Connected, bytes(input(1))) == Reject::WrongRole, "Host sendet Eingabe -> falsche Rolle");
    Message wel;
    wel.type = MsgType::Welcome;
    wel.welcome = {1, 5};
    check(dec(Role::Guest, ConnState::Handshake, bytes(wel)) == Reject::WrongRole, "Gast sendet Welcome -> falsche Rolle");
    // Zustaende
    check(dec(Role::Guest, ConnState::Handshake, in) == Reject::WrongState, "Eingabe vor dem Handshake");
    check(dec(Role::Guest, ConnState::Connected, bytes(hello("x"))) == Reject::WrongState, "zweites Hello");
    check(dec(Role::Guest, ConnState::Closed, in) == Reject::WrongState, "nach dem Schliessen");
    // Bereiche
    check(bytes(input(1, -128)).empty(), "Achse -128 laesst sich nicht kodieren");
    {
        std::vector<u8> b = in;
        b[9] = 0x80;  // moveX = -128
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::BadRange, "Achse -128 abgelehnt");
        b = in;
        b[11] = (u8)(251 & 0xFF), b[12] = 0;  // yaw = 251
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::BadRange, "Blickaenderung 251 abgelehnt");
        b = in;
        b[13] = 0x00, b[14] = 0x80;  // pitch = -32768
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::BadRange, "Neigung -32768 abgelehnt");
        b = in;
        b[15] = 0x40, b[16] = 0;  // Taste ausserhalb der Maske
        check(dec(Role::Guest, ConnState::Connected, b) == Reject::BadButtons, "unbekannte Taste abgelehnt");
    }
    // Snapshot: zu viele Spieler, doppelte Kennung, Koordinate, Laenge passt nicht zur Anzahl
    {
        std::vector<u8> b = bytes(snapshot(2));
        b[9] = 3;
        check(dec(Role::Host, ConnState::Connected, b) == Reject::BadLength, "Anzahl 3 bei 2 Eintraegen");
        b[9] = 5;
        check(dec(Role::Host, ConnState::Connected, b) == Reject::BadRange, "Anzahl 5 > 4");
        b = bytes(snapshot(2));
        b[10 + 16] = 0;  // zweiter Eintrag: Kennung 0 doppelt
        check(dec(Role::Host, ConnState::Connected, b) == Reject::BadRange, "doppelte Spielerkennung");
        Message s = snapshot(1);
        s.snapshot.players[0].xMm = kMaxCoordMm + 1;
        check(bytes(s).empty(), "Koordinate ausserhalb laesst sich nicht kodieren");
    }
    // Name: leer, zu lang, Steuerzeichen, Nicht-ASCII, Rest nicht null
    {
        Message h = hello("ok");
        h.hello.nameLen = 0;
        check(bytes(h).empty(), "leerer Name");
        h = hello("ok");
        h.hello.nameLen = 17;
        check(bytes(h).empty(), "Name 17");
        std::vector<u8> b = bytes(hello("Anna"));
        b[5 + 2 + 16 + 1 + 1] = '\n';
        check(dec(Role::Guest, ConnState::Handshake, b) == Reject::BadName, "Steuerzeichen im Namen");
        b = bytes(hello("Anna"));
        b[5 + 2 + 16 + 1 + 1] = 0xC3;
        check(dec(Role::Guest, ConnState::Handshake, b) == Reject::BadName, "Nicht-ASCII im Namen");
        b = bytes(hello("Anna"));
        b[5 + 2 + 16 + 1 + 10] = 'x';
        check(dec(Role::Guest, ConnState::Handshake, b) == Reject::BadName, "Daten hinter dem Namen");
        b = bytes(hello("Anna"));
        b[5] = 2;
        check(dec(Role::Guest, ConnState::Handshake, b) == Reject::BadVersion, "falsche Version");
    }
}

static void testFuzz(int rounds) {
    std::printf("Fuzzing (%d Runden)\n", rounds);
    std::mt19937 rng(1234);
    const Role roles[] = {Role::Guest, Role::Host};
    const ConnState states[] = {ConnState::Handshake, ConnState::Connected, ConnState::Closed};
    int accepted = 0, nonCanonical = 0;
    std::vector<std::vector<u8>> seeds = {bytes(input(3)), bytes(hello("Max")), bytes(snapshot(3)), bytes(snapshot(0))};
    for (int k = 0; k < rounds; ++k) {
        std::vector<u8> b;
        if (k % 2 == 0) {  // Zufallsbytes, oft mit gueltiger Kennung, damit der Parser tief laeuft
            const size_t n = rng() % 160;
            b.resize(n);
            for (auto& x : b) x = (u8)rng();
            if (n >= 5 && rng() % 2) std::memcpy(b.data(), "LIMN", 4), b[4] = (u8)(1 + rng() % 6);
        } else {  // gueltiges Paket mit Bitfehlern, Kuerzung oder Anhaengsel
            b = seeds[rng() % seeds.size()];
            const int flips = 1 + (int)(rng() % 4);
            for (int f = 0; f < flips; ++f) b[rng() % b.size()] ^= (u8)(1u << (rng() % 8));
            if (rng() % 8 == 0) b.resize(rng() % (b.size() + 8), 0x41);
        }
        for (Role r : roles)
            for (ConnState st : states) {
                Message m;
                if (decode(r, st, b.data(), b.size(), m) == Reject::None) {
                    ++accepted;
                    if (bytes(m) != b) ++nonCanonical;
                }
            }
    }
    std::printf("  %d Pakete x 6 Rollen/Zustaende geprueft, %d angenommen, davon %d nicht kanonisch\n", rounds, accepted, nonCanonical);
    check(nonCanonical == 0, "angenommene Pakete sind kanonisch");
}

static void testLimits() {
    std::printf("Rate, Folge, Gegenstelle\n");
    TokenBucket tb(10.0, 5.0);
    int got = 0;
    for (int i = 0; i < 20; ++i) got += tb.take(0.0);
    check(got == 5, "Eimer: 5 auf einmal");
    check(!tb.take(0.05) && tb.take(0.1), "Eimer fuellt sich mit 10 je Sekunde");
    check(!tb.take(0.05), "Uhr rueckwaerts gibt keine Marken");

    SeqWindow sw;
    check(sw.accept(100) && sw.accept(101) && !sw.accept(101) && !sw.accept(50), "Folge: doppelt und alt verworfen");
    check(!sw.accept(101 + (1u << 21)), "Folge: Sprung ueber 2^20 verworfen");
    SeqWindow wrap;
    check(wrap.accept(0xFFFFFFF0u) && wrap.accept(5), "Folge: Ueberlauf der Nummer");

    // Host-Seite: Gast mit falschem Code -> nach 8 Fehlern getrennt
    {
        Peer p(Role::Guest, 0.0, kCode);
        Message h = hello("Eve");
        h.hello.code[3] ^= 1;
        std::vector<u8> b = bytes(h);
        Message out;
        Peer::Verdict v = Peer::Verdict::Accept;
        int n = 0;
        while (v != Peer::Verdict::Disconnect && n < 20) v = p.onPacket(0.01 * ++n, b.data(), b.size(), out);
        check(v == Peer::Verdict::Disconnect && n == 8 && p.lastReject() == Reject::BadCode, "falscher Code: Trennung nach 8 Versuchen");
        check(p.onPacket(1.0, b.data(), b.size(), out) == Peer::Verdict::Disconnect, "getrennt bleibt getrennt");
    }
    // Handshake, Eingaben, vertauschte Folge
    {
        Peer p(Role::Guest, 0.0, kCode);
        Message out;
        std::vector<u8> in = bytes(input(7));
        check(p.onPacket(0.1, in.data(), in.size(), out) == Peer::Verdict::Drop && p.lastReject() == Reject::WrongState, "Eingabe vor Hello verworfen");
        std::vector<u8> h = bytes(hello("Max"));
        check(p.onPacket(0.2, h.data(), h.size(), out) == Peer::Verdict::Accept && p.state() == ConnState::Connected, "Hello mit Code verbindet");
        check(p.onPacket(0.3, in.data(), in.size(), out) == Peer::Verdict::Accept && out.input.seq == 7, "Eingabe angenommen");
        check(p.onPacket(0.31, in.data(), in.size(), out) == Peer::Verdict::Drop && p.lastReject() == Reject::OldSequence, "doppelte Eingabe verworfen");
        check(p.invalidCount() == 1, "vertauschte Pakete zaehlen nicht als Fehler");
        check(!p.timedOut(5.0) && p.timedOut(10.5), "Stille ueber 10 s -> Zeitlimit");
    }
    {
        Peer p(Role::Guest, 0.0, kCode);
        check(!p.timedOut(4.9) && p.timedOut(5.1), "Handshake ueber 5 s -> Zeitlimit");
    }
    // Flut: 90 je Sekunde erlaubt, 10 000 in einer Sekunde -> verwerfen, dann trennen
    {
        Peer p(Role::Guest, 0.0, kCode);
        Message out;
        std::vector<u8> h = bytes(hello("Max"));
        p.onPacket(0.0, h.data(), h.size(), out);
        int accepted = 0;
        Peer::Verdict v = Peer::Verdict::Accept;
        for (int i = 0; i < 10000 && v != Peer::Verdict::Disconnect; ++i) {
            std::vector<u8> in = bytes(input((u32)(i + 1)));
            v = p.onPacket(0.5 + i * 1e-4, in.data(), in.size(), out);
            accepted += v == Peer::Verdict::Accept;
        }
        check(v == Peer::Verdict::Disconnect, "Flut fuehrt zur Trennung");
        check(accepted <= 80, "Flut: hoechstens Eimer plus Nachfuellung angenommen (" + std::to_string(accepted) + ")");
    }
    // Gast-Seite: Host-Nachrichten
    {
        Peer p(Role::Host, 0.0, kCode);
        Message out;
        std::vector<u8> s = bytes(snapshot(2));
        check(p.onPacket(0.1, s.data(), s.size(), out) == Peer::Verdict::Drop, "Zustand vor Welcome verworfen");
        Message wel;
        wel.type = MsgType::Welcome;
        wel.welcome = {1, 9};
        std::vector<u8> w = bytes(wel);
        check(p.onPacket(0.2, w.data(), w.size(), out) == Peer::Verdict::Accept && p.state() == ConnState::Connected, "Welcome verbindet");
        check(p.onPacket(0.3, s.data(), s.size(), out) == Peer::Verdict::Accept && out.snapshot.count == 2, "Zustand angenommen");
        std::vector<u8> in = bytes(input(1));
        check(p.onPacket(0.4, in.data(), in.size(), out) == Peer::Verdict::Drop && p.lastReject() == Reject::WrongRole, "Host darf keine Eingabe senden");
    }
}

int main(int argc, char** argv) {
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 1000000;
    testRoundTrip();
    testRejects();
    testFuzz(rounds);
    testLimits();
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLER" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
