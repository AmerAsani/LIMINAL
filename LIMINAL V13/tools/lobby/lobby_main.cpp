// V13.3: liminal_lobby.exe - Lobby (Beitrittscodes, Signalisierung) und STUN/TURN-Relay fuer den Koop-Modus.
// Anleitung, Kosten und Datenschutz: MULTIPLAYER_SETUP.md. Ohne Konto, ohne fremde Dienste.
//
//   liminal_lobby.exe [--port 27515] [--bind ADRESSE] [--turn-port 3478] [--no-turn]
//                     [--public-host NAME] [--public-turn-port N] [--external-ip IP] [--relay-ports 50000-50199]
//                     [--tls-cert DATEI --tls-key DATEI] [--max-rooms 256] [--verbose]
//
// Gespeichert wird nichts auf der Platte. IP-Adressen haelt der Server nur im Speicher (Raten, Sperren) und
// schreibt sie nie ins Protokoll.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "netdll/lobby_server.hpp"

namespace {
std::atomic<bool> gStop{false};
void onSignal(int) { gStop = true; }

void usage() {
    std::printf(
        "liminal_lobby - Lobby und Relay fuer den Koop-Modus von LIMINAL\n\n"
        "  --port N            TCP-Port der Lobby (WebSocket), Standard 27515\n"
        "  --bind ADRESSE      nur an dieser Adresse lauschen (Standard: alle)\n"
        "  --turn-port N       UDP-Port fuer STUN/TURN, Standard 3478\n"
        "  --no-turn           ohne STUN/TURN (dann nur direkte Wege)\n"
        "  --public-host NAME  so erreichen Spieler STUN/TURN (Standard: wie die Lobby)\n"
        "  --public-turn-port N  so erreichen Spieler den TURN-Port (Weiterleitung), Standard wie --turn-port\n"
        "  --external-ip IP    oeffentliche IP fuer Relay-Adressen (Server hinter NAT)\n"
        "  --relay-ports A-B   UDP-Ports fuer Relay-Zuteilungen (fuer die Firewall)\n"
        "  --tls-cert DATEI    Zertifikat (PEM) fuer wss://\n"
        "  --tls-key DATEI     Schluessel (PEM) fuer wss://\n"
        "  --max-rooms N       hoechstens N Raeume (Standard 256)\n"
        "  --verbose           Ereignisse ausgeben (ohne IP-Adressen)\n");
}
}  // namespace

int main(int argc, char** argv) {
    lim::netc::LobbyConfig cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--port") cfg.port = (uint16_t)std::atoi(next().c_str());
        else if (a == "--bind") cfg.bind = next();
        else if (a == "--turn-port") cfg.turnPort = (uint16_t)std::atoi(next().c_str());
        else if (a == "--no-turn") cfg.turn = false;
        else if (a == "--public-host") cfg.publicHost = next();
        else if (a == "--public-turn-port") cfg.publicTurnPort = (uint16_t)std::atoi(next().c_str());
        else if (a == "--external-ip") cfg.externalAddress = next();
        else if (a == "--relay-ports") {
            const std::string r = next();
            const size_t d = r.find('-');
            if (d != std::string::npos) cfg.relayPortBegin = (uint16_t)std::atoi(r.c_str()), cfg.relayPortEnd = (uint16_t)std::atoi(r.c_str() + d + 1);
        } else if (a == "--tls-cert") cfg.tlsCert = next();
        else if (a == "--tls-key") cfg.tlsKey = next();
        else if (a == "--max-rooms") cfg.maxRooms = std::max(1, std::atoi(next().c_str()));
        else if (a == "--verbose") cfg.verbose = true;
        else {
            usage();
            return a == "--help" || a == "-h" ? 0 : 2;
        }
    }
    if (!cfg.publicHost.empty() && !lim::netc::hostOk(cfg.publicHost)) {
        std::fprintf(stderr, "--public-host: ungueltiger Name\n");
        return 2;
    }
    lim::netc::LobbyServer server;
    std::string err;
    if (!server.start(cfg, err)) {
        std::fprintf(stderr, "Start fehlgeschlagen: %s\n", err.c_str());
        return 1;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::printf("LIMINAL Lobby laeuft: %s://%s:%u", cfg.tlsCert.empty() ? "ws" : "wss", cfg.bind.empty() ? "<alle>" : cfg.bind.c_str(), server.port());
    if (server.turnPort()) std::printf(" | STUN/TURN UDP %u%s%s", server.turnPort(), cfg.publicHost.empty() ? "" : " als ", cfg.publicHost.c_str());
    std::printf("\nBeenden mit Strg+C.\n");
    std::fflush(stdout);
    auto last = std::chrono::steady_clock::now();
    while (!gStop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (std::chrono::steady_clock::now() - last > std::chrono::seconds(60)) {
            last = std::chrono::steady_clock::now();
            const auto s = server.stats();
            std::printf("Verbindungen %d, Raeume %d, wartende Gaeste %d, Nachrichten %llu, abgelehnt %llu, abgewiesen %llu, gedrosselt %llu\n",
                        s.connections, s.rooms, s.guests, (unsigned long long)s.messages, (unsigned long long)s.rejected,
                        (unsigned long long)s.refusedJoins, (unsigned long long)s.rateLimited);
            std::fflush(stdout);
        }
    }
    server.stop();
    std::printf("Lobby beendet.\n");
    return 0;
}
