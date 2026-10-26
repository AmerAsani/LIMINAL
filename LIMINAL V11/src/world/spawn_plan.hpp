// V11: Belegung einer festen Karte fuer einen Lauf (WORLD_GENERATION.md, Abschnitt 4.2).
//
// Die Architektur ist fest; je Lauf (Seed) wird hier entschieden, welche Spawnpunkte der Karte
// belegt werden und womit. Regeln aus der Leveldatei ("spawn_rules"):
//   count [lo, hi]   so viele Punkte einer Art werden belegt
//   spacing m        Mindestabstand zwischen zwei belegten Punkten derselben Art
//   table            Gewichte der Gegenstaende (nur "item")
//   first_within m   das erste Exemplar liegt hoechstens so viele Meter Weg vom Start entfernt
//   chance           Dekoration: Wahrscheinlichkeit je Punkt (mal dessen Gewicht)
// Punkte derselben Gruppe schliessen sich aus. Die Wege werden ueber den Raumgraphen der Karte
// gemessen (Raummitte -> Tuer -> Raummitte). Alles deterministisch aus dem Seed: ein Spielstand
// braucht nur den Seed.
#pragma once

#include <string>
#include <vector>

#include "core/json.hpp"
#include "world/map_def.hpp"

namespace lim::world {

class SpawnPlan {
public:
    SpawnPlan() = default;
    SpawnPlan(const MapDef& map, const Json& rules, u64 seed);

    bool active(size_t spawn) const { return spawn < active_.size() && active_[spawn]; }
    const std::string& item(size_t spawn) const { return item_[spawn]; }
    int variant(size_t spawn) const { return spawn < variant_.size() ? variant_[spawn] : 0; }
    // Wegabstand eines Punktes vom Start (m, ueber den Raumgraphen)
    double pathFromStart(double x, double y) const;
    size_t size() const { return active_.size(); }
    int countActive(const std::string& kind) const;

private:
    const MapDef* map_ = nullptr;
    std::vector<bool> active_;
    std::vector<std::string> item_;
    std::vector<int> variant_;
    std::vector<double> roomDist_;  // Weg vom Start zur Raummitte
};

}  // namespace lim::world
