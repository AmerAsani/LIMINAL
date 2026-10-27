// V13.3: Lobby-Server des Koop-Modus (MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 6; Betrieb: MULTIPLAYER_SETUP.md).
//
// Vermittelt Beitrittscodes und reicht die Verbindungsbeschreibungen (SDP, ICE-Kandidaten) zwischen Host und Gast
// weiter (Signalisierung). Startet auf Wunsch den STUN/TURN-Server von libjuice (Relay, wenn kein direkter Weg geht).
// Keine eigene Kryptografie: TLS (wss) macht libdatachannel/Mbed TLS, die Echtheit der Gegenstellen prueft DTLS.
//
// Laeuft als eigenes Programm (liminal_lobby.exe), eingebaut im Host fuer die Direktverbindung (ein Raum, ohne Relay)
// und in den Tests. Alles aus dem Netz gilt als feindlich: Hoechstgroessen, Raten je Verbindung und je Adresse,
// Zeitlimits, Sperren; weitergereichte Nachrichten werden aus geprueften Feldern neu gebaut.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "netdll/lobby_proto.hpp"

namespace lim::netc {

struct LobbyConfig {
    std::string bind;              // leer: alle Schnittstellen
    uint16_t port = 27515;         // TCP (WebSocket); 0: vom System vergeben
    std::string tlsCert, tlsKey;   // PEM-Dateien oder PEM-Inhalt; leer: ohne TLS (ws://, nur LAN/Test)
    bool turn = true;              // STUN/TURN-Server (libjuice) starten
    uint16_t turnPort = 3478;      // UDP; 0: vom System vergeben
    std::string publicHost;        // so erreichen Spieler STUN/TURN; leer: derselbe Rechner wie die Lobby
    uint16_t publicTurnPort = 0;   // so erreichen Spieler den TURN-Port (Weiterleitung auf einen anderen Port); 0: turnPort
    std::string externalAddress;   // oeffentliche IP fuer die Relay-Adressen (wenn der Server hinter NAT steht)
    uint16_t relayPortBegin = 0, relayPortEnd = 0;  // UDP-Ports der Relay-Zuteilungen (0: beliebig)
    std::vector<IceEntry> extraIce;  // weitere Server (z. B. TURN eines Anbieters), werden durchgereicht
    int maxConnections = 512;      // WebSocket-Verbindungen insgesamt
    int maxPerIp = 12;             // gleichzeitige Verbindungen je Adresse
    int maxRooms = 256, maxRoomsPerIp = 4;
    int joinsPerMinute = 20;       // Beitrittsversuche je Adresse
    int failsPerTenMinutes = 10;   // unbekannte Codes je Adresse, danach 10 min gesperrt
    double roomHours = 12.0;       // Hoechstdauer eines Raums und der Relay-Zugangsdaten
    bool singleRoom = false;       // eingebaute Lobby (direkt): ein Raum, "host" nur ueber 127.0.0.1/::1
    bool verbose = false;          // Ereignisse auf stderr (ohne IP-Adressen)
    // nur Tests: ICE-Kandidaten (auch in SDP) umschreiben oder verwerfen (Rueckgabe false); fromHost: Richtung
    std::function<bool(std::string& candidate, bool fromHost)> candidateHook;
};

class LobbyServer {
public:
    LobbyServer();
    ~LobbyServer();
    LobbyServer(const LobbyServer&) = delete;
    LobbyServer& operator=(const LobbyServer&) = delete;

    bool start(const LobbyConfig& cfg, std::string& error);
    void stop();
    uint16_t port() const;
    uint16_t turnPort() const;  // 0: kein TURN

    struct Stats {
        int connections = 0, rooms = 0, guests = 0;
        uint64_t messages = 0, rejected = 0, refusedJoins = 0, rateLimited = 0, closedForAbuse = 0;
    };
    Stats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

}  // namespace lim::netc
