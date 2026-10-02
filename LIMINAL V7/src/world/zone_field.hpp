// Zonenfeld (Port von V4, zones.py): liefert fuer jede Weltposition die Gewichte
// aller Zonen. Unabhaengige Rauschfelder werden per Softmax geschaerft - meist
// dominiert eine Zone klar, dazwischen liegen weiche Uebergangsbaender.
#pragma once

#include "core/core.hpp"
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

private:
    const LevelDef& level_;
    int count_;
    u64 seeds_[kMaxZones];
};

}  // namespace lim::world
