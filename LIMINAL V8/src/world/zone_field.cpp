#include "world/zone_field.hpp"

#include <algorithm>
#include <cmath>

#include "core/rng.hpp"

namespace lim::world {

ZoneField::ZoneField(const LevelDef& level, i64 seed) : level_(level), count_(level.zoneCount()) {
    for (int i = 0; i < count_; ++i) seeds_[i] = hash64(seed, "zone", i) & 0xFFFFFFFFULL;
    if (level.world()) {
        warpSeed_ = hash64(seed, "strata-warp") & 0xFFFFFFFFULL;
        islandSeed_ = hash64(seed, "strata-island") & 0xFFFFFFFFULL;
    }
}

// V8: Schichten der Welt. Der Abstand zum Start wird grossraeumig verformt (Grenzen werden zu
// Buchten und Zungen) und von einem zweiten Feld zu Ausbuchtungen und Inseln verschoben; nahe dem
// Start bleibt alles ruhig, damit jeder Lauf in den Bueros beginnt.
double ZoneField::strataCoord(double x, double y) const {
    const StrataDef& S = level_.strata;
    const double d = std::hypot(x, y);
    const double calm = std::clamp((d - S.calm * 0.5) / std::max(1.0, S.calm), 0.0, 1.0);
    const double k = calm * calm * (3.0 - 2.0 * calm);
    // Domain-Warping: das ganze Feld wird verwirbelt (Zungen, Buchten, abgeschnuerte Inseln) ...
    const double wx = (fbm(warpSeed_, x / S.warpScale + 11.3, y / S.warpScale - 7.9, 3) * 2.0 - 1.0) * S.warp * k;
    const double wy = (fbm(warpSeed_ ^ 0x5BD1E995ULL, x / S.warpScale - 4.2, y / S.warpScale + 2.6, 3) * 2.0 - 1.0) * S.warp * k;
    // ... und ein feineres Feld schiebt einzelne Taschen vor oder zurueck
    const double n2 = fbm(islandSeed_, x / S.islandScale - 3.1, y / S.islandScale + 5.7, 2) * 2.0 - 1.0;
    const double r = std::max(0.0, std::hypot(x + wx, y + wy) + S.island * n2 * k);
    return r < S.first ? r / S.first : 1.0 + (r - S.first) / S.width;
}

std::array<double, kMaxZones> ZoneField::regionWeights(double x, double y) const {
    std::array<double, kMaxZones> m{};
    const StrataDef& S = level_.strata;
    if (!level_.world() || S.order.empty()) {
        m[0] = 1.0;
        return m;
    }
    const double u = strataCoord(x, y);
    const i64 k = (i64)std::floor(u);
    const double f = u - (double)k;
    const i64 n = (i64)S.order.size();
    auto reg = [&](i64 layer) { return S.order[(size_t)(((layer % n) + n) % n)]; };
    // Nahtstellen: auf beiden Seiten einer Grenze laufen die Ebenen weich ineinander
    double toNext = 0.0, toPrev = 0.0;
    if (f > 1.0 - S.seam) toNext = 0.5 * (f - (1.0 - S.seam)) / S.seam;
    if (k > 0 && f < S.seam) toPrev = 0.5 * (S.seam - f) / S.seam;
    auto smooth = [](double t) { return t * t * (3.0 - 2.0 * t); };
    toNext = 0.5 * smooth(toNext * 2.0);
    toPrev = 0.5 * smooth(toPrev * 2.0);
    m[(size_t)reg(k)] += 1.0 - toNext - toPrev;
    if (toNext > 0.0) m[(size_t)reg(k + 1)] += toNext;
    if (toPrev > 0.0) m[(size_t)reg(k - 1)] += toPrev;
    return m;
}

std::array<double, kMaxZones> ZoneField::regionShare(const ZoneWeights& w) const {
    std::array<double, kMaxZones> m{};
    for (int i = 0; i < count_; ++i) m[(size_t)level_.zones[(size_t)i].region] += w[(size_t)i];
    return m;
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
    // V8: die Schicht bestimmt, welche Ebene hier liegt - ihre Zonen setzen sich durch
    if (level_.world()) {
        auto m = regionWeights(x, y);
        for (int i = 0; i < count_; ++i) vals[i] += level_.strata.bias * m[(size_t)level_.zones[(size_t)i].region];
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
