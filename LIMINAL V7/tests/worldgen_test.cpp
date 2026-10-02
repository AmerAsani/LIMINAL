// Vergleicht die C++-Weltgenerierung mit Referenzdaten aus V4 (Python).
//
//   worldgen_test <level.json> <v4_reference.json>
//
// Die Referenz entsteht mit tools/v4_reference/dump_ref.py aus dem Original-
// Code von V4. Gleiche Seeds muessen gleiche Welten ergeben, damit Seeds und
// V4-Spielstaende in V5 weiter funktionieren.
#include <cmath>
#include <cstdio>
#include <format>
#include <memory>
#include <string>

#include "core/jobs.hpp"
#include "core/json.hpp"
#include "core/log.hpp"
#include "core/rng.hpp"
#include "world/world.hpp"

using namespace lim;
using namespace lim::world;

static int gFails = 0, gChecks = 0;

static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        if (gFails < 60) std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}

static bool near(double a, double b, double tol = 2e-5) { return std::fabs(a - b) <= tol; }

static const char* kindName(DoorKind k) {
    switch (k) {
        case DoorKind::Door: return "door";
        case DoorKind::Wide: return "wide";
        case DoorKind::Gate: return "gate";
        case DoorKind::Opening: return "opening";
    }
    return "?";
}

static bool rgbEq(const Json& j, const std::array<double, 3>& c) {
    return near(j[0].asNumber(), c[0]) && near(j[1].asNumber(), c[1]) && near(j[2].asNumber(), c[2]);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("Aufruf: worldgen_test <level.json> <v4_reference.json>\n");
        return 2;
    }
    std::string err;
    auto levelOpt = LevelDef::loadFile(argv[1], &err);
    if (!levelOpt) {
        std::printf("Level-Fehler: %s\n", err.c_str());
        return 2;
    }
    auto level = std::make_shared<const LevelDef>(std::move(*levelOpt));
    auto ref = loadJsonFile(argv[2]);
    if (!ref) {
        std::printf("Referenz nicht lesbar\n");
        return 2;
    }

    // --- Zufallsfunktionen ----------------------------------------------------------------
    const Json& r = (*ref)["rng"];
    check(std::to_string(hash64(1, 2, 3)) == r["hash64"][0].asString(), "hash64(1,2,3)");
    check(std::to_string(hash64(7, "portal", 0, -3, 5)) == r["hash64"][1].asString(), "hash64(portal)");
    check(std::to_string(hash64(-1)) == r["hash64"][2].asString(), "hash64(-1)");
    check(std::to_string(hash64("zone")) == r["hash64"][3].asString(), "hash64('zone')");
    check(near(hfloat(7, 11, -2, 3), r["hfloat"][0].asNumber(), 1e-6), "hfloat 1");
    check(near(hfloat(12345, 31, 4, 5), r["hfloat"][1].asNumber(), 1e-6), "hfloat 2");
    {
        Rng g(0xDEADBEEFULL);
        for (int i = 0; i < 4; ++i) check(std::to_string(g.next64()) == r["rng"][i].asString(), "Rng.next64");
        for (int i = 0; i < 4; ++i) check(g.randint(3, 17) == r["rng"][4 + i].asInt(), "Rng.randint");
        for (int i = 0; i < 4; ++i) check(near(g.random(), r["rng"][8 + i].asNumber(), 1e-6), "Rng.random");
    }
    check(near(fbm(99, 1.25, -3.5, 2), r["fbm"][0].asNumber(), 1e-6), "fbm 1");
    check(near(fbm(5, -100.3, 42.7, 3), r["fbm"][1].asNumber(), 1e-6), "fbm 2");
    {
        ZoneField zf(*level, 7);
        const double pts[3][2] = {{0, 0}, {300, -200}, {-1000, 450}};
        for (int p = 0; p < 3; ++p) {
            auto w = zf.weights(pts[p][0], pts[p][1]);
            for (int i = 0; i < 5; ++i) check(near(w[(size_t)i], r["zone_w"][p][i].asNumber(), 1e-6), "Zonengewichte");
        }
    }
    std::printf("Zufallsfunktionen: %d Pruefungen, %d Fehler\n", gChecks, gFails);

    // --- Welten ---------------------------------------------------------------------------
    JobSystem jobs(2);
    for (const Json& wj : (*ref)["worlds"].items()) {
        i64 seed = wj["seed"].asInt();
        World world(level, seed, 0.28, 0.80, jobs, 4);
        int f0 = gFails, c0 = gChecks;
        for (const Json& sj : wj["sectors"].items()) {
            i64 sx = sj["sx"].asInt(), sy = sj["sy"].asInt();
            Sector& sec = world.sector(sx, sy);
            std::string where = std::format("Seed {} Sektor ({},{})", seed, sx, sy);
            check(sec.rooms.size() == sj["rooms"].size(),
                  std::format("{}: {} Raeume statt {}", where, sec.rooms.size(), sj["rooms"].size()));
            size_t n = std::min(sec.rooms.size(), sj["rooms"].size());
            for (size_t i = 0; i < n; ++i) {
                const Room& rm = *sec.rooms[i];
                const Json& rj = sj["rooms"][i];
                std::string w = std::format("{} Raum {}", where, i);
                check(rm.x0 == rj["rect"][0].asInt() && rm.y0 == rj["rect"][1].asInt() && rm.x1 == rj["rect"][2].asInt() &&
                          rm.y1 == rj["rect"][3].asInt(),
                      w + " Rechteck");
                check(rm.type->key == rj["type"].asString(), w + " Typ " + rm.type->key + " statt " + rj["type"].asString());
                check(rm.zone == rj["zone"].asInt(), w + " Zone");
                check(near(rm.floor, rj["floor"].asNumber()), w + " Boden");
                check(near(rm.height, rj["height"].asNumber()), w + std::format(" Hoehe {} statt {}", rm.height, rj["height"].asNumber()));
                check(rm.lightStyle == rj["light"].asString(), w + " Lichtstil");
                check(near(rm.ambient, rj["ambient"].asNumber()), w + " Grundhelligkeit");
                check(near(rm.lampInt, rj["lamp_int"].asNumber()), w + " Lampenstaerke");
                check(near(rm.avgLight, rj["avg"].asNumber()), w + " mittlere Helligkeit");
                check(rgbEq(rj["fog"], rm.fog), w + " Nebelfarbe");
                check(near(rm.fogDensity, rj["fog_d"].asNumber()), w + " Nebeldichte");
                check(rm.flicker == rj["flicker"].asInt(), w + " Flackern");
                check(rgbEq(rj["wall"], world.bank()[rm.wmat].rgb), w + " Wandfarbe");
                check(rgbEq(rj["floor_rgb"], world.bank()[rm.fmat].rgb), w + " Bodenfarbe");
                check(rm.items.size() == rj["items"].size(), w + " Anzahl Items");
                for (size_t k = 0; k < std::min(rm.items.size(), rj["items"].size()); ++k) {
                    const auto& it = rm.items[k];
                    const Json& ij = rj["items"][k];
                    check(it.id == ij[0].asString() && it.kind == ij[1].asString() && near(it.x, ij[2].asNumber()) &&
                              near(it.y, ij[3].asNumber()) && near(it.z, ij[4].asNumber()),
                          w + " Item " + it.id);
                }
            }
            check(sec.doors.size() == sj["doors"].size(),
                  std::format("{}: {} Tueren statt {}", where, sec.doors.size(), sj["doors"].size()));
            size_t nd = std::min(sec.doors.size(), sj["doors"].size());
            for (size_t i = 0; i < nd; ++i) {
                const Door& d = *sec.doors[i];
                const Json& dj = sj["doors"][i];
                std::string w = std::format("{} Tuer {}", where, i);
                check(d.x0 == dj[0].asInt() && d.y0 == dj[1].asInt() && d.x1 == dj[2].asInt() && d.y1 == dj[3].asInt() &&
                          d.axis == dj[4].asInt(),
                      w + " Lage");
                check(std::string(kindName(d.kind)) == dj[5].asString(), w + " Art");
                check(near(d.sill, dj[6].asNumber()) && near(d.top, dj[7].asNumber()), w + " Schwelle/Sturz");
                check((int)d.portal == dj[8].asInt() && (int)d.owned == dj[9].asInt(), w + " Portal");
            }
        }
        for (const Json& cj : wj["chunks"].items()) {
            ChunkKey key{cj["cx"].asInt(), cj["cy"].asInt()};
            std::string where = std::format("Seed {} Chunk ({},{})", seed, key.x, key.y);
            int tileFails = 0;
            for (int i = 0; i < CS * CS; ++i) {
                const Tile& t = world.tile(key.x * CS + (i % CS), key.y * CS + (i / CS));
                const Json& tj = cj["cells"][(size_t)i];
                bool ok;
                if (tj.isNull()) ok = t.solid();
                else
                    ok = t.open() && near(t.floor, tj[0].asNumber()) && near(t.ceil, tj[1].asNumber()) &&
                         near(t.light, tj[2].asNumber()) && rgbEq(tj[3], world.bank()[t.fmat].rgb) &&
                         rgbEq(tj[4], world.bank()[t.wmat].rgb) && (int)world.bank()[t.cmat].emissive == tj[5].asInt();
                if (!ok && tileFails++ < 3) {
                    std::string got = t.solid() ? "massiv"
                                                : std::format("f={:.3f} c={:.3f} l={:.4f}", t.floor, t.ceil, t.light);
                    check(false, std::format("{} Kachel {} ({}): {} / Referenz {}", where, i, i % CS, got, tj.dump()));
                } else {
                    check(ok, where);
                }
            }
            const ChunkData* cd = world.chunk(key);
            for (int i = 0; i < (CS + 1) * (CS + 1) && cd; ++i)
                check(near(cd->corners[(size_t)i], cj["corners"][(size_t)i].asNumber(), 1e-4), where + " Ecke");
        }
        auto [x, y, z, a] = world.findSpawn();
        const Json& sp = wj["spawn"];
        check(near(x, sp[0].asNumber()) && near(y, sp[1].asNumber()) && near(z, sp[2].asNumber()) &&
                  near(a, sp[3].asNumber(), 1e-5),
              std::format("Seed {} Startpunkt ({},{},{})", seed, x, y, z));
        std::printf("Seed %lld: %d Pruefungen, %d Fehler\n", (long long)seed, gChecks - c0, gFails - f0);
    }
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
