// V6: prueft die Item-Balance der Versorgungsraeume (Energie-Riegel seltener).
//
//   supply_balance_test <data-Ordner>
//
// Erzeugt fuer alle Level und Schwierigkeitsgrade viele Sektoren mit mehreren Seeds,
// einmal mit der V5-Verteilung (ohne Seltenheitsregeln, Wirkungen wie V5) und einmal
// mit den V6-Regeln aus data/items.json. Geprueft wird:
//   - Energy Bars sind deutlich seltener (20 .. 45 % der V5-Menge), aber nicht weg
//   - die Sanity, die ein Versorgungsraum im Mittel liefert, bleibt etwa gleich (-12 .. +12 %)
//   - Weltaufbau, Item-IDs und Item-Positionen bleiben gueltig (V6 ist Teilmenge von V5)
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "core/jobs.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "gameplay/content.hpp"
#include "world/world.hpp"

using namespace lim;

static int gFails = 0;
static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}

struct Tally {
    int sectors = 0, supplyRooms = 0;
    std::map<std::string, int> items;
    double sanity = 0;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: supply_balance_test <data-Ordner>\n");
        return 2;
    }
    game::Content content;
    std::string err;
    if (!content.load(argv[1], err)) {
        std::printf("Inhalte nicht ladbar: %s\n", err.c_str());
        // Ursache genauer: lesbar? gueltiges JSON?
        std::string f = std::string(argv[1]) + "/items.json";
        auto text = paths::readFile(f);
        std::printf("  %s: %s, %zu Bytes\n", f.c_str(), text ? "lesbar" : "NICHT lesbar", text ? text->size() : 0);
        if (text) {
            std::string perr;
            auto j = Json::parse(*text, &perr);
            std::printf("  JSON: %s %s\n", j ? "ok" : "Fehler:", perr.c_str());
        }
        return 2;
    }
    // Wirkungen in V5 (data/items.json von V5)
    const std::map<std::string, double> v5Sanity = {{"energy", 20.0}, {"almond", 35.0}};
    auto v6Sanity = [&](const std::string& k) {
        const game::ItemDef* d = content.item(k);
        return d ? d->sanity : 0.0;
    };
    const auto rules = content.supplyRules();
    check(!rules.empty(), "keine Seltenheitsregel in items.json");

    JobSystem jobs(2);
    const int R = 3;  // 7 x 7 Sektoren je Seed
    const long long seeds[] = {1, 7, 1234, 4242, 99991, 2024, 31337, 987654321};
    // V6: beide Weltstrukturen pruefen (neuer Aufbau fuer neue Spiele, alter fuer alte Spielstaende)
    for (const auto& info : content.levels())
    for (const auto& lvl : {info.def, info.legacy}) {
        if (lvl == info.legacy && info.legacy->layout == info.def->layout) continue;
        struct { std::shared_ptr<const world::LevelDef> def; } lv{lvl};
        for (const auto& diff : content.difficulties()) {
            Tally v5, v6;
            for (long long seed : seeds) {
                world::World a(lv.def, seed, diff.itemsNone, diff.itemsOne, jobs, 4);
                world::World b(lv.def, seed, diff.itemsNone, diff.itemsOne, jobs, 4, rules);
                for (int sy = -R; sy <= R; ++sy)
                    for (int sx = -R; sx <= R; ++sx) {
                        const world::Sector& sa = a.sector(sx, sy);
                        const world::Sector& sb = b.sector(sx, sy);
                        ++v5.sectors, ++v6.sectors;
                        check(sa.rooms.size() == sb.rooms.size(), "Weltaufbau veraendert");
                        for (size_t i = 0; i < std::min(sa.rooms.size(), sb.rooms.size()); ++i) {
                            const world::Room& ra = *sa.rooms[i];
                            const world::Room& rb = *sb.rooms[i];
                            check(ra.x0 == rb.x0 && ra.y0 == rb.y0 && ra.x1 == rb.x1 && ra.y1 == rb.y1 &&
                                      ra.type->key == rb.type->key,
                                  "Raum veraendert");
                            if (ra.hasMachine()) ++v5.supplyRooms;
                            if (rb.hasMachine()) ++v6.supplyRooms;
                            std::map<std::string, const world::ItemSpawn*> ids;
                            for (const auto& it : ra.items) {
                                ++v5.items[it.kind];
                                v5.sanity += v5Sanity.at(it.kind);
                                ids[it.id] = &it;
                            }
                            for (const auto& it : rb.items) {
                                ++v6.items[it.kind];
                                v6.sanity += v6Sanity(it.kind);
                                auto f = ids.find(it.id);
                                check(f != ids.end() && std::fabs(f->second->x - it.x) < 1e-9 &&
                                          std::fabs(f->second->y - it.y) < 1e-9,
                                      "Item-ID/Position nicht stabil: " + it.id);
                            }
                        }
                    }
            }
            auto per = [](double v, int n) { return n ? v / n : 0.0; };
            int e5 = v5.items["energy"], e6 = v6.items["energy"];
            int a5 = v5.items["almond"], a6 = v6.items["almond"];
            double s5 = per(v5.sanity, v5.supplyRooms), s6 = per(v6.sanity, v6.supplyRooms);
            std::printf("%s V%d / %-6s  Versorgungsraeume %4d | Energy Bars V5 %4d -> V6 %4d (%3.0f %%) | "
                        "Mandelwasser %4d -> %4d | Sanity je Raum %5.1f -> %5.1f (%+.0f %%) | "
                        "Sektoren je Energy Bar %4.1f -> %4.1f\n",
                        lv.def->id.c_str(), lv.def->layout, diff.key.c_str(), v6.supplyRooms, e5, e6, e5 ? 100.0 * e6 / e5 : 0.0, a5,
                        a6, s5, s6, s5 > 0 ? (s6 / s5 - 1.0) * 100.0 : 0.0, per(v5.sectors, e5), per(v6.sectors, e6));
            std::string w = lv.def->id + " V" + std::to_string(lv.def->layout) + "/" + diff.key;
            check(v5.supplyRooms == v6.supplyRooms, w + ": Anzahl Versorgungsraeume veraendert");
            check(e6 > 0, w + ": Energy Bars komplett verschwunden");
            check(e5 > 0 && e6 >= 0.20 * e5 && e6 <= 0.45 * e5, w + ": Energy Bars nicht im Zielbereich 20..45 %");
            check(s5 > 0 && s6 >= 0.88 * s5 && s6 <= 1.12 * s5, w + ": Sanity je Versorgungsraum ausserhalb +-12 %");
        }
    }
    std::printf("%s (%d Fehler)\n", gFails ? "FEHLGESCHLAGEN" : "OK", gFails);
    return gFails ? 1 : 0;
}
