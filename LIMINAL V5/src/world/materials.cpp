#include "world/materials.hpp"

#include <cmath>

#include "core/log.hpp"
#include "core/math.hpp"

namespace lim::world {

PlanePattern planePatternFromName(const std::string& s) {
    if (s == "none" || s == "plain") return PlanePattern::None;
    if (s == "carpet") return PlanePattern::Carpet;
    if (s == "slabs") return PlanePattern::Slabs;
    if (s == "bigtiles") return PlanePattern::BigTiles;
    if (s == "grate") return PlanePattern::Grate;
    if (s == "tiles") return PlanePattern::Tiles;
    if (s == "checker") return PlanePattern::Checker;
    if (s == "lines") return PlanePattern::Lines;
    log::warn("Unbekanntes Bodenmuster '{}'", s);
    return PlanePattern::None;
}

WallPattern wallPatternFromName(const std::string& s) {
    if (s == "plain") return WallPattern::Plain;
    if (s == "wallpaper") return WallPattern::Wallpaper;
    if (s == "blocks") return WallPattern::Blocks;
    if (s == "bricks") return WallPattern::Bricks;
    if (s == "panels") return WallPattern::Panels;
    if (s == "concrete") return WallPattern::Concrete;
    log::warn("Unbekanntes Wandmuster '{}'", s);
    return WallPattern::Plain;
}

// Quantisiert Farbkanaele wie V4 (_q), damit aehnliche Farben ein Material teilen.
static double quant(double v) { return pyRound(clamp(v, 0.0, 1.0) * 48.0) / 48.0; }

MaterialBank::MaterialBank() { items_.resize(kCapacity); }

u16 MaterialBank::get(std::array<double, 3> rgb, PlanePattern plane, WallPattern wall, bool emissive,
                      std::vector<Band> bands, double seamDark, Surface surface) {
    rgb = {quant(rgb[0]), quant(rgb[1]), quant(rgb[2])};
    Key key{rgb, (u8)plane, (u8)wall, emissive, bands, seamDark, (u8)surface};
    std::lock_guard lock(mutex_);
    auto it = index_.find(key);
    if (it != index_.end()) return it->second;
    std::size_t id = count_.load(std::memory_order_relaxed);
    LIM_CHECK(id < kCapacity, "Zu viele Materialien");
    auto m = std::make_unique<Material>();
    m->id = (u16)id;
    m->rgb = rgb;
    m->emissive = emissive;
    m->plane = plane;
    m->wall = wall;
    m->surface = surface;
    m->bands = std::move(bands);
    m->seamDark = seamDark;
    items_[id] = std::move(m);
    index_.emplace(std::move(key), (u16)id);
    count_.store(id + 1, std::memory_order_release);
    return (u16)id;
}

}  // namespace lim::world
