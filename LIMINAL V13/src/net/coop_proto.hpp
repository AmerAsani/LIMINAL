// V13.2: Spielprotokoll des Koop-Modus (Version 2) ueber dem verschluesselten Transport (liminal_net.dll).
//
// MULTIPLAYER_ARCHITEKTUR.md, Abschnitte 3 und 8: Der Host ist autoritativ. Ein Gast sendet nur Eingaben (Achsen,
// Blick, Tasten), Absichten (aufnehmen, benutzen, ablegen, Lampe) und Lebenszeichen - nie Positionen, Werte oder
// Inventare. Jede Nachricht hat eine genaue Laenge, jedes Feld einen Bereich; Typen sind je Rolle und
// Verbindungszustand erlaubt (Erlaubnisliste). Nur kanonische Nachrichten existieren: encode schreibt nichts, was
// decode ablehnen wuerde. Was die Nachrichten im Spiel bedeuten, prueft der Host zusaetzlich an seinem Zustand
// (Reichweite, Inventar, Abklingzeiten; src/coop).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include "core/core.hpp"
#include "net/protocol.hpp"

namespace lim::net::coop {

constexpr u16 kProtoVersion = 3;  // V13.2: 3 - Gesten der neuen Wesen (kAnimN 18)
constexpr size_t kMaxMsg = 1200;    // = LN_MAX_PAYLOAD
constexpr int kMaxPlayers = 4;      // Host + 3 Gaeste
constexpr int kMaxCreatures = 24;   // je Momentaufnahme (die naechsten)
constexpr int kMaxInputs = 4;       // Eingaben je Paket (die letzten, gegen Verlust)
constexpr int kInvSlots = 36;       // = Inventory::kSize
constexpr int kKindMax = 24;        // Kennung eines Gegenstands ([a-z0-9_])
constexpr int kLevelMax = 24;       // Kennung einer Ebene oder eines Uebergangs ([a-z0-9_.-])
constexpr int kTextMax = 96;        // Meldungen (UTF-8, darstellbar)
constexpr int kNameMax = 16;
constexpr int kAnimN = 18;          // Eingaben der Animation eines Wesens (V13.2: + wave talk plea strike)
constexpr int kRoles = 40;          // V13.3: Obergrenze fuer Presence::Role (heute 11). Rollen sind Zahlen (1 Byte); neue Wesen
                                    // brauchen bis 40 keine Protokollaenderung. Unbekannte Rollen zeigt der Gast nicht an.
constexpr i32 kCoordMax = 100'000'000;  // mm (100 km)
constexpr u8 kButtonMask = 0x0F;        // Rennen, Springen, Sprung gehalten, Schleichen

enum class Type : u8 {
    // Gast -> Host
    Join = 1, Input = 2, Action = 3, Ping = 4,
    // beide
    Bye = 5,
    // Host -> Gast
    Welcome = 16, Snapshot = 17, Event = 18, Inventory = 19, Roster = 20, Pong = 21, Check = 22,
};

enum class Reject : u8 {
    None, Empty, TooLong, UnknownType, WrongRole, WrongState, BadLength, BadVersion, BadRange, BadText, BadKind, BadButtons,
    Duplicate,
};
const char* rejectName(Reject r);

struct Join {
    u16 version = kProtoVersion;
    u32 build = 0;                 // Pruefsumme der Spieldaten (gleiche Welt)
    std::array<u8, 16> token{};    // Sitzungstoken zum Wiederverbinden (0 = neu)
};

struct InputFrame {
    u32 seq = 0;
    u8 dtMs = 16;          // 1..100
    i8 forward = 0, side = 0;  // -127..127
    i16 yaw = 0;           // mrad, -3142..3142 (absolut)
    i16 pitch = 0;         // mrad, -1450..1450
    u8 buttons = 0;        // Bit 0 Rennen, 1 Springen, 2 Sprung gehalten, 3 Schleichen
};
struct Input {
    u8 count = 0;  // 1..kMaxInputs, aufsteigende Folgenummern
    std::array<InputFrame, kMaxInputs> f{};
};

enum class ActionKind : u8 { Interact = 1, UseSlot = 2, Drop = 3, DropAll = 4, Lamp = 5, Select = 6, Spectate = 7 };
struct Action {
    u32 seq = 0;
    u8 kind = 1;
    u32 arg = 0;  // Interact: Kennzahl des Gegenstands; Platz 0..35; Lampe 0/1; Zuschauen: Spieler 0..3
};

struct Ping {
    u32 nonce = 0, time = 0;
};
struct Bye {
    u8 reason = 0;  // 0 normal, 1 Zeitlimit, 2 Protokoll, 3 voll, 4 entfernt, 5 gesperrt, 6 Version, 7 Host beendet
};

struct Welcome {
    u8 playerId = 1;               // 1..3 (0 = Host)
    std::array<u8, 16> token{};    // fuer das Wiederverbinden
    i64 runSeed = 0, seed = 0;
    u8 difficulty = 0;             // Index (0..7)
    std::string level, anchor;     // je hoechstens kLevelMax
    u32 worldHash = 0;
    u32 tick = 0;
    u8 maxPlayers = 4;             // 2..4
    u8 flags = 0;                  // Bit 0: Wiederverbunden (Werte und Inventar zurueck)
    u16 supply = 100;              // Vorraete dieser Ebene in % (100..300; Koop: 1 + 0,6 x (n - 1))
};

struct PlayerNet {
    u8 id = 0;
    i32 x = 0, y = 0, z = 0;  // mm
    i16 yaw = 0, pitch = 0;   // mrad
    u8 speed = 0;             // cm/s / 4
    u8 flags = 0;             // Bit 0 Lampe an, 1 tot, 2 rennt, 3 schleicht, 4 am Boden, 5 schwimmt, 6 bereit
};
struct SelfNet {
    u32 ack = 0;              // letzte verarbeitete Eingabe
    i32 x = 0, y = 0, z = 0;
    i16 vx = 0, vy = 0, vz = 0;  // cm/s
    i16 yaw = 0, pitch = 0;
    u8 health = 100, sanity = 100, stamina = 100, battery = 100;  // 0..100
    u16 flags = 0;            // Bit 0 Lampe an, 1 tot, 2 erschoepft, 3 am Boden, 4 zuschauen, 5 Lampe leer
    u8 respawnIn = 0;         // s
    u8 spectate = 0;          // Spieler, dem man zusieht (0..3)
    u8 boost = 0;             // Tempo-Bonus in % (Energy Bar), 0..100
};
struct CreatureNet {
    u16 netId = 0;
    u8 role = 0;              // Presence::Role 0..kRoles-1
    u8 variant = 0;           // Modellvariante 0..3
    i32 x = 0, y = 0, z = 0;
    i16 yaw = 0;
    u8 state = 0;
    u8 target = 255;          // Spieler, auf den es sich richtet (255: keiner)
    u8 flags = 0;             // Bit 0 sieht hin (look), 1 im Licht, 2 erstarrt
    std::array<u8, kAnimN> anim{};  // jaw alert crouch growl reach fly shiver hang bait light perceive glow (0..255), tilt lean (-127..127),
                                    // wave talk plea strike (0..255)
    std::array<i8, 3> up{0, 0, 127}, fwd{127, 0, 0};  // Lage (Motte an Wand oder Decke), Einheitsvektoren x 127
};
struct Snapshot {
    u32 tick = 0;
    SelfNet self;
    u8 nPlayers = 0;  // andere Spieler (0..3)
    std::array<PlayerNet, kMaxPlayers - 1> players{};
    u8 nCreatures = 0;
    std::array<CreatureNet, kMaxCreatures> creatures{};
};

enum class EventKind : u8 {
    Picked = 1,     // a: Kennzahl des Gegenstands, b: Spieler
    Dropped = 2,    // a: Kennzahl (abgelegt), b: Anzahl; text: Art; Ort
    Blackout = 3,   // b: Dauer (1/10 s); Ort
    Died = 4,       // a: Spieler, b: Rolle des Wesens (-1 keins)
    Respawn = 5,    // a: Spieler; Ort, b: Blick (mrad)
    Level = 6,      // text: "ebene|uebergang"; a: niedrige 32 Bit des Seeds
    Toast = 7,      // text; b: Dauer (1/10 s)
    Ready = 8,      // a: bereit, b: lebend; Ort des Ausgangs; c in z: Sekunden bis zum Mitziehen (-1 keins)
    Note = 9,       // a: Notiz
    Sound = 10,     // text: Klang; Ort; b: Lautstaerke (1/100)
    Joined = 11,    // a: Spieler; text: Name
    Left = 12,      // a: Spieler; b: Grund
    Flicker = 13,   // Ort
    Consumed = 14,  // text: Meldung; Klang ueber Sound
    Figure = 15,    // a: Presence::Event::Type, b: Rolle; Ort (Klang und Schreck beim Gast)
};
struct Event {
    u32 seq = 0;
    u8 kind = 7;
    i32 a = 0, b = 0;
    i32 x = 0, y = 0, z = 0;
    std::string text;  // hoechstens kTextMax Byte
};

struct InvEntry {
    u8 slot = 0;      // 0..35
    std::string kind;
    u8 count = 0;     // 1..99
};
struct Inventory {
    u8 selected = 0;
    u8 n = 0;
    std::array<InvEntry, kInvSlots> e{};
};

struct RosterEntry {
    u8 id = 0;
    u8 flags = 0;  // Bit 0 lebt, 1 bereit, 2 verbunden
    std::string name;
};
struct Roster {
    u8 n = 0;
    std::array<RosterEntry, kMaxPlayers> e{};
};

struct Check {
    u32 tick = 0, hash = 0;  // Pruefsumme des geteilten Zustands (aufgenommen, abgelegt)
};

struct Msg {
    Type type = Type::Ping;
    Join join;
    Input input;
    Action action;
    Ping ping;
    Bye bye;
    Welcome welcome;
    Snapshot snapshot;
    Event event;
    Inventory inventory;
    Roster roster;
    Check check;
};

// Schreibt m kanonisch nach buf; 0, wenn m ungueltig ist oder buf zu klein
size_t encode(const Msg& m, u8* buf, size_t cap);
// Liest und prueft eine Nachricht des Absenders `from` im Zustand `st`. Bei Reject::None ist out gefuellt.
Reject decode(Role from, ConnState st, const u8* data, size_t n, Msg& out);

// Hilfen
bool kindOk(const std::string& s);   // [a-z0-9_], 1..kKindMax
bool levelOk(const std::string& s);  // [a-z0-9_.-], 0..kLevelMax
bool textOk(const std::string& s);   // UTF-8, darstellbar, hoechstens kTextMax
bool nameOk(const std::string& s);   // wie im Transport: 1..16, Buchstaben, Ziffern, Leer, _ -
inline i32 toMm(double m) { return (i32)std::lround(std::clamp(m * 1000.0, (double)-kCoordMax, (double)kCoordMax)); }
inline double fromMm(i32 v) { return v / 1000.0; }
i16 toMrad(double a);  // auf -pi..pi gefaltet
inline double fromMrad(i16 v) { return v / 1000.0; }

}  // namespace lim::net::coop
