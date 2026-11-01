// V13.3: Fuzzing der eigenen Parser der Verbindungsschicht (Lobby-Nachrichten, Beitrittscode, Pruefteil) - ohne Netz,
// fuer Laeufe unter AddressSanitizer/UndefinedBehaviorSanitizer (MULTIPLAYER_TESTS.md, Abschnitt 7.4).
//   lobby_proto_fuzz [runden]
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "netdll/joincode.hpp"
#include "netdll/lobby_proto.hpp"

using namespace lim::netc;

int main(int argc, char** argv) {
    const long rounds = argc > 1 ? std::atol(argv[1]) : 200000;
    std::mt19937 rng(20261027);
    std::vector<std::string> seeds;
    {
        LMsg m;
        m.t = LType::Join, m.room = "AB12CD", m.name = "Gast";
        seeds.push_back(build(m));
        m = {};
        m.t = LType::Host, m.max = 3;
        seeds.push_back(build(m));
        m = {};
        m.t = LType::Sig, m.kind = "offer", m.sdp = "v=0\r\no=- 1 1 IN IP4 0.0.0.0\r\na=fingerprint:sha-256 AB:CD\r\n", m.to = 2;
        seeds.push_back(build(m));
        m = {};
        m.t = LType::Cand, m.cand = "candidate:1 1 UDP 2122317823 127.0.0.1 50000 typ host", m.mid = "0", m.from = 4;
        seeds.push_back(build(m));
        m = {};
        m.t = LType::Room, m.room = "XY34ZW", m.ttl = 60, m.ice = {{false, "h", 3478, "", ""}, {true, "", 3478, "u", "p"}};
        seeds.push_back(build(m));
        m = {};
        m.t = LType::Guest, m.id = 7, m.name = "N", m.addr = "tok123";
        seeds.push_back(build(m));
        m = {};
        m.t = LType::Ban, m.id = 7, m.addr = "tok123";
        seeds.push_back(build(m));
        seeds.push_back("{\"t\":\"err\",\"e\":\"noroom\"}");
        seeds.push_back("K7QF-M2XA-9FJT-W3HB-ZP4C-1234");
        std::string fp = "AB";
        for (int i = 1; i < 32; ++i) fp += ":CD";
        seeds.push_back(fp);
    }
    long accepted = 0, rebuilt = 0;
    for (long r = 0; r < rounds; ++r) {
        std::string x = seeds[rng() % seeds.size()];
        if (r % 4 == 0) {
            x.resize(rng() % 400);
            for (char& c : x) c = (char)(rng() & 0xFF);
        } else {
            for (int k = 0, n = 1 + (int)(rng() % 8); k < n; ++k) {
                if (x.empty()) x = "{";
                const size_t p = rng() % x.size();
                switch (rng() % 5) {
                    case 0: x[p] = (char)(rng() & 0xFF); break;
                    case 1: x.erase(p, 1 + rng() % 8); break;
                    case 2: x.insert(p, std::string(1 + rng() % 40, "{}[]\":,0a\\-u"[rng() % 12])); break;
                    case 3: x = x.substr(0, p); break;
                    default: x.insert(p, x.substr(0, rng() % (x.size() + 1))); break;
                }
            }
        }
        LMsg a, b;
        const bool ok1 = parseFromClient(x, (rng() & 1) != 0, a);
        const bool ok2 = parseFromLobby(x, b);
        if (ok1) {
            ++accepted;
            LMsg c;
            rebuilt += parseFromClient(build(a), true, c) || parseFromClient(build(a), false, c) ? 1 : 0;
        }
        if (ok2) {
            ++accepted;
            LMsg c;
            rebuilt += parseFromLobby(build(b), c) ? 1 : 0;
        }
        JoinCode jc;
        if (parseCode(x, jc)) (void)jc.text(true);
        (void)pinFromFingerprint(x);
        (void)depthOk(x, 4);
        (void)nameOk(x), (void)sdpOk(x), (void)candOk(x), (void)hostOk(x);
    }
    const bool ok = accepted == rebuilt;
    std::printf("%s: %ld Runden, %ld angenommen, %ld neu gebaut und wieder gelesen\n", ok ? "OK" : "FEHLER", rounds, accepted, rebuilt);
    return ok ? 0 : 1;
}
