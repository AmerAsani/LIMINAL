// V13.2/V13.3 Koop: Zugang zum Transport (liminal_net.dll) ueber Funktionszeiger (MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 7).
// Das Spiel laedt die DLL erst, wenn man hostet oder beitritt (LoadLibrary); LiminalGame.exe bindet keine
// Netzwerkbibliothek. Tests und Bots fuellen dieselbe Tabelle mit der statischen Transportschicht.
#pragma once

#include <string>

#include "netdll/ln_api.h"

namespace lim::coop {

struct NetApi {
    LnContext* (*host)(const LnConfig*, const LnLimits*) = nullptr;
    LnContext* (*join)(const LnConfig*, const char*, const char*, const char*) = nullptr;
    int (*poll)(LnContext*, LnEvent*) = nullptr;
    int (*send)(LnContext*, int, int, const uint8_t*, int) = nullptr;
    void (*drop)(LnContext*, int, int, int) = nullptr;
    int (*stats)(LnContext*, int, LnStats*) = nullptr;
    int (*code)(LnContext*, int, char*, int) = nullptr;
    uint16_t (*port)(LnContext*) = nullptr;
    void (*close)(LnContext*) = nullptr;
    int (*random)(uint8_t*, int) = nullptr;
    int (*version)() = nullptr;
    bool ok() const { return host && join && poll && send && drop && stats && code && port && close && random && version; }
};

// Verbindungsweg (aus koop.json, MULTIPLAYER_SETUP.md, und dem Menue)
struct NetSettings {
    std::string lobby;      // "wss://name:port"; leer: keine Lobby eingetragen
    std::string lobbyCert;  // festgelegtes Zertifikat der Lobby (PEM-Inhalt oder SHA-256 "AB:CD:..."); leer: Windows
    std::string ice;        // weitere STUN/TURN-Server ("stun:host:port turn:user:pass@host:port")
    std::string bind;       // nur Tests: lokale Adresse
    uint16_t directPort = 27500;
    bool useLobby = true;   // false: direkt (Host: eingebaute Lobby; Gast: Adresse + langer Code)
    bool relayOnly = false; // eigene IP verbergen (nur ueber das Relay der Lobby)
    // Zeiger in die Zeichenketten dieses Objekts (so lange gueltig, wie es lebt)
    LnConfig config() const {
        return LnConfig{useLobby ? lobby.c_str() : "", lobbyCert.c_str(), ice.c_str(), bind.c_str(), directPort, relayOnly ? 1 : 0};
    }
};

// liminal_net.dll aus dem Ordner des Spiels laden (nicht aus dem Suchpfad). false: fehlt, falsche Version (error)
bool loadNetLibrary(NetApi& api, std::string& error);

}  // namespace lim::coop
