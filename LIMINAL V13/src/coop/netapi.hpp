// V13.2 Koop: Zugang zum Transport (liminal_net.dll) ueber Funktionszeiger (MULTIPLAYER_ARCHITEKTUR.md, Abschnitt 6).
// Das Spiel laedt die DLL erst, wenn man hostet oder beitritt (LoadLibrary); LiminalGame.exe bindet keine
// Netzwerkbibliothek. Tests und Bots fuellen dieselbe Tabelle mit der statischen Transportschicht.
#pragma once

#include <string>

#include "netdll/ln_api.h"

namespace lim::coop {

struct NetApi {
    LnContext* (*host)(uint16_t, const uint8_t*, const LnLimits*) = nullptr;
    LnContext* (*connect)(const char*, uint16_t, const uint8_t*, const char*) = nullptr;
    int (*poll)(LnContext*, LnEvent*) = nullptr;
    int (*send)(LnContext*, int, int, const uint8_t*, int) = nullptr;
    void (*drop)(LnContext*, int, int, int) = nullptr;
    int (*stats)(LnContext*, int, LnStats*) = nullptr;
    uint16_t (*port)(LnContext*) = nullptr;
    void (*close)(LnContext*) = nullptr;
    int (*random)(uint8_t*, int) = nullptr;
    int (*version)() = nullptr;
    bool ok() const { return host && connect && poll && send && drop && stats && port && close && random && version; }
};

// liminal_net.dll aus dem Ordner des Spiels laden (nicht aus dem Suchpfad). false: fehlt, falsche Version (error)
bool loadNetLibrary(NetApi& api, std::string& error);

}  // namespace lim::coop
