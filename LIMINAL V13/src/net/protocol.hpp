// V13: Pruefschicht eines moeglichen Mehrspielerprotokolls - ohne Netzwerk, ohne Sockets.
//
// MULTIPLAYER_BEDROHUNGSMODELL.md, Abschnitt 6, setzt die Regeln: autoritativer Server, der Gast sendet nur
// Eingaben (nie Zustaende), jede Nachricht wird nach Typ, Laenge, Bereich, Rolle, Verbindungszustand, Folge und
// Rate geprueft; Fehler trennen die Verbindung, nie das Spiel.
//
// Diese Bibliothek (lim_net) wird vom Spiel NICHT gebunden (MULTIPLAYER_ENTSCHEIDUNG.md). Sie ist die
// gepruefte Grundlage fuer einen spaeteren Modus und wird von tests/net_protocol_test.cpp geprueft.
#pragma once

#include <array>
#include <cstddef>

#include "core/core.hpp"

namespace lim::net {

constexpr u32 kMagic = 0x4E4D494Cu;  // "LIMN" (little endian)
constexpr u16 kVersion = 1;
constexpr size_t kMaxPacket = 1200;   // nie mehr lesen oder annehmen
constexpr int kMaxSnapshotPlayers = 4;
constexpr int kNameMax = 16;
constexpr i16 kMaxLookMilli = 250;   // hoechste Blickaenderung je Eingabepaket (0,25 rad)
constexpr u16 kButtonMask = 0x3F;    // Springen, Rennen, Schleichen, Benutzen, Lampe, Gegenstand
constexpr i32 kMaxCoordMm = 1'000'000'000;

enum class Role : u8 { Host, Guest };            // wer die Nachricht sendet
enum class ConnState : u8 { Handshake, Connected, Closed };
enum class MsgType : u8 { Hello = 1, Welcome = 2, Input = 3, Snapshot = 4, Bye = 5, Ping = 6 };

enum class Reject : u8 {
    None,
    TooShort,
    TooLong,
    BadMagic,
    UnknownType,
    WrongRole,   // Typ ist fuer diese Rolle nicht erlaubt (z. B. Gast sendet Zustand)
    WrongState,  // Typ ist in diesem Verbindungszustand nicht erlaubt
    BadLength,   // Laenge passt nicht genau zum Typ
    BadVersion,
    BadRange,    // Feld ausserhalb seines Bereichs
    BadName,
    BadButtons,
    BadCode,     // falscher Sitzungscode
    OldSequence,
    RateLimited,
};
const char* rejectName(Reject r);

struct Hello {
    u16 version = kVersion;
    std::array<u8, 16> code{};  // Sitzungscode (vom Host angezeigt)
    u8 nameLen = 0;
    std::array<char, kNameMax> name{};
};
struct Welcome {
    u8 playerId = 0;  // 1..kMaxSnapshotPlayers-1 (0 = Host)
    u32 cookie = 0;
};
struct Input {  // die einzige Nachricht, mit der ein Gast das Spiel beeinflusst
    u32 seq = 0;
    i8 moveX = 0, moveY = 0;  // -127..127 (Achse * 127)
    i16 yawMilli = 0, pitchMilli = 0;
    u16 buttons = 0;
};
struct PlayerState {
    u8 id = 0;
    i32 xMm = 0, yMm = 0, zMm = 0;
    i16 yawMilli = 0;  // -3142..3142
    u8 flags = 0;      // Bit 0 Lampe an, Bit 1 geduckt, Bit 2 tot
};
struct Snapshot {
    u32 tick = 0;
    u8 count = 0;
    std::array<PlayerState, kMaxSnapshotPlayers> players{};
};
struct Bye {
    u8 reason = 0;  // 0 normal, 1 Zeitlimit, 2 Protokollfehler, 3 voll
};
struct Ping {
    u32 nonce = 0;
};

struct Message {
    MsgType type = MsgType::Ping;
    Hello hello;
    Welcome welcome;
    Input input;
    Snapshot snapshot;
    Bye bye;
    Ping ping;
};

// Genaue Laenge je Typ (Snapshot: nach Anzahl)
size_t wireSize(const Message& m);
// Schreibt m kanonisch nach buf; 0, wenn m ungueltig ist oder buf zu klein
size_t encode(const Message& m, u8* buf, size_t cap);
// Liest und prueft ein Paket des Absenders `from` im Zustand `st`. Bei Reject::None ist out gefuellt.
Reject decode(Role from, ConnState st, const u8* data, size_t n, Message& out);

// Marken-Eimer: rate Marken je Sekunde, hoechstens burst
class TokenBucket {
public:
    TokenBucket(double rate, double burst) : rate_(rate), burst_(burst), tokens_(burst) {}
    bool take(double now);

private:
    double rate_, burst_, tokens_, last_ = -1.0;
};

// Folgenummern: nur strikt neuere (Seriennummern-Arithmetik, Sprung hoechstens 2^20)
class SeqWindow {
public:
    bool accept(u32 seq);

private:
    bool any_ = false;
    u32 last_ = 0;
};

// Eine Gegenstelle aus Sicht des Empfaengers. Der Host haelt je Gast einen Peer (remote = Guest).
class Peer {
public:
    enum class Verdict : u8 { Accept, Drop, Disconnect };
    struct Limits {
        double rate = 90.0, burst = 30.0;  // Pakete je Sekunde
        int maxInvalid = 8;                // ungueltige Pakete bis zur Trennung
        int maxDroppedInRow = 64;          // ueber die Rate verworfene Pakete in Folge bis zur Trennung
        double handshakeTimeout = 5.0, silenceTimeout = 10.0;
    };
    Peer(Role remote, double now, const std::array<u8, 16>& sessionCode, Limits lim);
    Peer(Role remote, double now, const std::array<u8, 16>& sessionCode) : Peer(remote, now, sessionCode, Limits{}) {}

    Verdict onPacket(double now, const u8* data, size_t n, Message& out);
    bool timedOut(double now) const;
    ConnState state() const { return state_; }
    Reject lastReject() const { return last_; }
    int invalidCount() const { return invalid_; }
    void markWelcomed() { state_ = ConnState::Connected; }  // Gast-Seite: nach Welcome

private:
    Verdict fail(Reject r);
    Role remote_;
    ConnState state_ = ConnState::Handshake;
    std::array<u8, 16> code_;
    Limits lim_;
    TokenBucket bucket_;
    SeqWindow seq_;
    double opened_, heard_;
    int invalid_ = 0, droppedInRow_ = 0;
    Reject last_ = Reject::None;
};

}  // namespace lim::net
