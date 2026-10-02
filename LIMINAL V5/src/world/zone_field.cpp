#include "world/zone_field.hpp"

#include <cmath>

#include "core/rng.hpp"

namespace lim::world {

ZoneField::ZoneField(const LevelDef& level, i64 seed) : level_(level), count_(level.zoneCount()) {
    for (int i = 0; i < count_; ++i) seeds_[i] = hash64(seed, "zone", i) & 0xFFFFFFFFULL;
}

ZoneWeights ZoneField::weights(double x, double y) const {
    const double s = 1.0 / level_.zoneScale;
    double vals[kMaxZones];
    for (int i = 0; i < count_; ++i) vals[i] = fbm(seeds_[i], x * s + i * 3.7, y * s - i * 1.3, 2);
    // Rund um den Startpunkt bevorzugte Zonen (V4: Bueroflure) - ein vertrauter Anfang.
    double r2 = (x * x + y * y) / (level_.originRadius * level_.originRadius);
    for (int i = 0; i < count_; ++i) {
        const ZoneDef& z = level_.zones[(size_t)i];
        if (z.originBias != 0.0) vals[i] += z.originBias * std::exp(-r2);
        if (z.weightOffset != 0.0) vals[i] += z.weightOffset;
    }
    double top = vals[0];
    for (int i = 1; i < count_; ++i) top = std::max(top, vals[i]);
    double ex[kMaxZones];
    double total = 0.0;
    for (int i = 0; i < count_; ++i) {
        ex[i] = std::exp(level_.zoneSharpness * (vals[i] - top));
        total += ex[i];
    }
    ZoneWeights w{};
    for (int i = 0; i < count_; ++i) w[(size_t)i] = ex[i] / total;
    return w;
}

double ZoneField::blend(const ZoneWeights& w, double ZoneDef::*attr) const {
    double total = 0.0;
    for (int i = 0; i < count_; ++i) total += level_.zones[(size_t)i].*attr * w[(size_t)i];
    return total;
}

Rgb ZoneField::mix(const ZoneWeights& w, Rgb ZoneDef::*attr) const {
    double r = 0.0, g = 0.0, b = 0.0;
    for (int i = 0; i < count_; ++i) {
        const Rgb& c = level_.zones[(size_t)i].*attr;
        r += c[0] * w[(size_t)i];
        g += c[1] * w[(size_t)i];
        b += c[2] * w[(size_t)i];
    }
    return {r, g, b};
}

int ZoneField::dominant(const ZoneWeights& w) const {
    int best = 0;
    for (int i = 1; i < count_; ++i)
        if (w[(size_t)i] > w[(size_t)best]) best = i;
    return best;
}

}  // namespace lim::world
