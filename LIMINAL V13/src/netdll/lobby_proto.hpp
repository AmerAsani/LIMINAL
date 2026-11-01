// V13.3: Nachrichten zwischen Spiel (liminal_net.dll) und Lobby-Server (MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 6).
//
// JSON-Objekte als WebSocket-Textnachrichten, hoechstens kMaxLobbyMsg Byte. Jede Seite prueft jedes Feld (Typ,
// Laenge, Zeichen, Bereich) und baut weitergereichte Nachrichten NEU aus geprueften Feldern auf - nie wird
// empfangenes JSON unveraendert weitergegeben. Die Lobby sieht Raumnummer, Namen und Verbindungsbeschreibungen
// (SDP, ICE-Kandidaten), nie das Geheimteil des Codes und keinen Spielverkehr.
//
//   Spiel -> Lobby   host  {v, max}                  Raum oeffnen (Host)
//                    join  {v, room, name}           beitreten (Gast)
//                    sig   {to?, kind, sdp}          Beschreibung (offer/answer); "to" nur vom Host
//                    cand  {to?, c, mid}             ICE-Kandidat
//                    refuse{id, e}                   Host weist einen Gast ab
//                    ban   {id, addr}                Host sperrt die Adresse eines Gasts fuer den Raum
//                    done  {}                        Gast ist verbunden und verlaesst die Lobby
//   Lobby -> Spiel   room  {room, ice, ttl}          Raum offen
//                    joined{id, ice}                 Gast angenommen, wartet auf die Beschreibung des Hosts
//                    guest {id, name, addr}          (an den Host) ein Gast will beitreten
//                    sig/cand {from?, ...}           weitergereicht
//                    gone  {id}                      (an den Host) Gast hat die Lobby verlassen
//                    err   {e}                       Fehler (Code, siehe errOk)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lim::netc {

constexpr int kLobbyVersion = 1;
constexpr size_t kMaxLobbyMsg = 8192;
constexpr size_t kMaxSdp = 6000;
constexpr size_t kMaxCand = 300;
constexpr int kMaxIce = 6;
constexpr int kMaxGuestId = 1000000;

struct IceEntry {
    bool turn = false;
    std::string host;  // leer: derselbe Rechner wie die Lobby
    uint16_t port = 3478;
    std::string user, pass;
};

enum class LType { None, Host, Join, Sig, Cand, Refuse, Ban, Done, Room, Joined, Guest, Gone, Err };

struct LMsg {
    LType t = LType::None;
    int v = kLobbyVersion;
    int max = 0;         // host: Gaeste 1..7
    std::string room;    // join, room
    std::string name;    // join, guest
    int id = 0;          // refuse, ban, joined, guest, gone
    int to = 0;          // sig/cand vom Host an die Lobby: Ziel (Gast)
    int from = 0;        // sig/cand von der Lobby an den Host: Absender (Gast)
    std::string kind;    // sig: "offer" | "answer"
    std::string sdp;
    std::string cand, mid;
    std::string err;     // err, refuse
    std::string addr;    // guest, ban: Kennung der Adresse (je Raum, keine IP)
    std::vector<IceEntry> ice;
    int ttl = 0;         // room: Gueltigkeit der Relay-Zugangsdaten (s)
};

// fromHost: Nachricht eines Hosts (darf "to" setzen) oder eines Gasts
bool parseFromClient(const std::string& text, bool fromHost, LMsg& out);
bool parseFromLobby(const std::string& text, LMsg& out);
std::string build(const LMsg& m);  // leer, wenn m ungueltig waere

// Pruefungen (auch fuer die DLL und Tests)
bool nameOk(const std::string& s);       // 1..16: Buchstaben, Ziffern, Leerzeichen, - _ . (nicht am Rand leer)
bool sdpOk(const std::string& s);        // druckbares ASCII mit CR/LF, hoechstens kMaxSdp
bool candOk(const std::string& s);       // "candidate:..." druckbar, hoechstens kMaxCand
bool midOk(const std::string& s);        // [A-Za-z0-9_-], 1..32
bool errOk(const std::string& s);        // bekannte Fehlercodes
bool hostOk(const std::string& s);       // Rechnername oder IP (auch IPv6 ohne Klammern), 0..253
bool credOk(const std::string& s);       // [A-Za-z0-9_-], 1..64
// Nachrichten nur bis zu dieser Schachtelungstiefe parsen (vor dem eigentlichen Parser)
bool depthOk(const std::string& text, int maxDepth);

}  // namespace lim::netc
