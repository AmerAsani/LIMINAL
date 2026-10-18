// Zonenfeld (Port von V4, zones.py): liefert fuer jede Weltposition die Gewichte
// aller Zonen. Unabhaengige Rauschfelder werden per Softmax geschaerft - meist
// dominiert eine Zone klar, dazwischen liegen weiche Uebergangsbaender.
#pragma once

#include "core/core.hpp"
#include <array>

#include "world/level_def.hpp"

namespace lim::world {

class ZoneField {
public:
    ZoneField(const LevelDef& level, i64 seed);

    ZoneWeights weights(double x, double y) const;
    // Gewichteter Mittelwert eines Zonenattributs (Summationsreihenfolge wie V4).
    double blend(const ZoneWeights& w, double ZoneDef::*attr) const;
    Rgb mix(const ZoneWeights& w, Rgb ZoneDef::*attr) const;
    int dominant(const ZoneWeights& w) const;
    int count() const { return count_; }
    // V8: Anteil jeder Ebene (Region) an einer Weltposition (Summe 1). Nur in der Welt sinnvoll.
    std::array<double, kMaxZones> regionWeights(double x, double y) const;
    // V8: Schichtkoordinate (0 = Start, 1 = erste Grenze, ...) nach Verformung
    double strataCoord(double x, double y) const;
    // V8: Anteile der Ebenen aus Zonengewichten (z. B. eines Raums)
    std::array<double, kMaxZones> regionShare(const ZoneWeights& w) const;

private:
    const LevelDef& level_;
    int count_;
    u64 seeds_[kMaxZones];
    u64 warpSeed_ = 0, islandSeed_ = 0;
};

}  // namespace lim::world
