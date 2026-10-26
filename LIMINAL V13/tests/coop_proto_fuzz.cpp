// V13.2: Fuzzing des Spielprotokolls v2 ohne Spiel und Netz - fuer Laeufe unter AddressSanitizer und
// UndefinedBehaviorSanitizer (MULTIPLAYER_TESTS.md):
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -Isrc tests/coop_proto_fuzz.cpp src/net/coop_proto.cpp src/net/protocol.cpp
//   coop_proto_fuzz [Runden]
// Zufallsbytes, Bitfehler, Kuerzungen und Anhaengsel gueltiger Nachrichten durch decode (beide Rollen, beide Zustaende);
// jede angenommene Nachricht muss kanonisch sein (encode ergibt dieselben Bytes).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "net/coop_proto.hpp"

using namespace lim;
namespace nc = net::coop;

int main(int argc, char** argv) {
    const long rounds = argc > 1 ? std::atol(argv[1]) : 1000000;
    std::vector<std::vector<u8>> seeds;
    auto add = [&](const nc::Msg& m) {
        u8 buf[nc::kMaxMsg];
        const size_t n = nc::encode(m, buf, sizeof(buf));
        if (!n) {
            std::printf("Vorlage %d nicht kodierbar\n", (int)m.type);
            std::exit(1);
        }
        seeds.emplace_back(buf, buf + n);
    };
    nc::Msg m;
    m.type = nc::Type::Join;
    add(m);
    m = {};
    m.type = nc::Type::Input;
    m.input.count = 4;
    for (int i = 0; i < 4; ++i) m.input.f[(size_t)i] = {(u32)(100 + i), 16, 10, -10, 100, -100, 3};
    add(m);
    m = {};
    m.type = nc::Type::Action;
    m.action = {1, 2, 7};
    add(m);
    m = {};
    m.type = nc::Type::Welcome;
    m.welcome.level = "level188", m.welcome.anchor = "t_a";
    add(m);
    m = {};
    m.type = nc::Type::Snapshot;
    m.snapshot.nPlayers = 3;
    for (int i = 0; i < 3; ++i) m.snapshot.players[(size_t)i].id = (u8)i;
    m.snapshot.nCreatures = nc::kMaxCreatures;
    for (int i = 0; i < nc::kMaxCreatures; ++i) m.snapshot.creatures[(size_t)i].netId = (u16)(i + 1);
    add(m);
    m = {};
    m.type = nc::Type::Event;
    m.event.kind = (u8)nc::EventKind::Dropped, m.event.b = 1, m.event.text = "battery";
    add(m);
    m = {};
    m.type = nc::Type::Event;
    m.event.kind = (u8)nc::EventKind::Toast, m.event.text = "Ünïcödé – ✓";
    add(m);
    m = {};
    m.type = nc::Type::Inventory;
    m.inventory.n = 3;
    m.inventory.e[0] = {0, "almond", 2}, m.inventory.e[1] = {9, "battery", 1}, m.inventory.e[2] = {35, "energy", 99};
    add(m);
    m = {};
    m.type = nc::Type::Roster;
    m.roster.n = 4;
    for (int i = 0; i < 4; ++i) m.roster.e[(size_t)i] = {(u8)i, 7, "Spieler"};
    add(m);
    std::mt19937_64 rng(20261026);
    long accepted = 0, total = 0;
    for (long it = 0; it < rounds; ++it) {
        std::vector<u8> d;
        if (it % 4 == 0) {
            d.resize(rng() % 1300);
            for (auto& x : d) x = (u8)rng();
            if (!d.empty() && (rng() & 1)) d[0] = (u8)(rng() % 24);
        } else {
            d = seeds[rng() % seeds.size()];
            const int k = 1 + (int)(rng() % 6);
            for (int j = 0; j < k; ++j) {
                switch (rng() % 5) {
                    case 0:
                        if (!d.empty()) d[rng() % d.size()] = (u8)rng();
                        break;
                    case 1:
                        if (!d.empty()) d.resize(rng() % d.size());
                        break;
                    case 2: d.push_back((u8)rng()); break;
                    case 3:
                        if (!d.empty()) d[rng() % d.size()] ^= (u8)(1u << (rng() % 8));
                        break;
                    default:
                        if (d.size() > 2) d.insert(d.begin() + (long)(rng() % d.size()), (u8)rng());
                }
            }
        }
        for (int role = 0; role < 2; ++role)
            for (int st = 0; st < 3; ++st) {
                ++total;
                nc::Msg out;
                if (nc::decode(role ? net::Role::Host : net::Role::Guest, (net::ConnState)st, d.data(), d.size(), out) != nc::Reject::None) continue;
                ++accepted;
                u8 buf[nc::kMaxMsg];
                const size_t n = nc::encode(out, buf, sizeof(buf));
                if (n != d.size() || std::memcmp(buf, d.data(), n) != 0) {
                    std::printf("FEHLER: angenommene Nachricht nicht kanonisch (Runde %ld, Typ %d)\n", it, (int)out.type);
                    return 1;
                }
            }
    }
    std::printf("OK: %ld Faelle, %ld angenommen (alle kanonisch)\n", total, accepted);
    return 0;
}
