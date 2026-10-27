// V13.3: Beitrittscode des Koop-Modus (MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 5).
//
//   kurz  (12 Zeichen):  RRRR-RRSS-SSSS          Raumnummer (6, 30 Bit) + Geheimteil (6, 30 Bit)
//   lang  (24 Zeichen):  ... + 12 Zeichen Pruefteil (60 Bit des SHA-256-Fingerabdrucks des Host-Zertifikats)
//
// Die Lobby sieht nur die Raumnummer. Das Geheimteil prueft der Host im verschluesselten Kanal. Den Pruefteil prueft
// der Gast gegen den Fingerabdruck, den DTLS danach erzwingt (Pinning). Zeichen: Crockford-Base32 (0-9, A-Z ohne
// I L O U); beim Lesen sind Gross/Klein, Bindestriche und Leerzeichen egal, O gilt als 0, I und L als 1.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace lim::netc {

constexpr int kRoomChars = 6;
constexpr int kSecretChars = 6;
constexpr int kPinChars = 12;

struct JoinCode {
    std::string room;    // 6 Zeichen
    std::string secret;  // 6 Zeichen
    std::string pin;     // leer oder 12 Zeichen
    bool hasPin() const { return !pin.empty(); }
    std::string text(bool withPin) const;  // in Vierergruppen mit Bindestrichen
};

// liest einen kurzen oder langen Code; false bei falscher Laenge oder fremden Zeichen
bool parseCode(const std::string& s, JoinCode& out);
// zufaellige Zeichen (Raumnummer, Geheimteil); false, wenn kein Zufall verfuegbar ist
bool randomChars(int n, std::string& out);
bool randomBytes(uint8_t* out, size_t n);
// Pruefteil aus einem SHA-256-Fingerabdruck "AB:CD:..." (wie in SDP); leer bei falschem Format
std::string pinFromFingerprint(const std::string& fp);
// Vergleich in fester Zeit (gleiche Laenge vorausgesetzt)
bool equalConst(const std::string& a, const std::string& b);
bool roomOk(const std::string& s);  // genau 6 Zeichen des Alphabets (kanonisch)

}  // namespace lim::netc
