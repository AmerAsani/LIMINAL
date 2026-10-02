#include "gameplay/explore.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/base64.hpp"
#include "world/room.hpp"

namespace lim::game {

namespace {

u16 rgb565(const std::array<double, 3>& c) {
    auto q = [](double v, int bits) { return (u16)std::clamp((int)std::lround(v * ((1 << bits) - 1)), 0, (1 << bits) - 1); };
    return (u16)((q(c[0], 5) << 11) | (q(c[1], 6) << 5) | q(c[2], 5));
}

}  // namespace

MapCell ExploreMap::describe(const world::Tile& t, const world::MaterialBank& bank) {
    MapCell m;
    if (t.solid()) {
        m.kind = MapKind::Wall;
        return m;
    }
    if (t.flags & world::TF_DOOR) m.kind = MapKind::Door;
    else if (t.flags & world::TF_MACHINE) m.kind = MapKind::Machine;
    else if (t.flags & world::TF_STAIR) m.kind = MapKind::Stair;
    else if (t.flags & world::TF_FEATURE) {
        double base = t.room ? t.room->floor : (double)t.floor;
        m.kind = ((double)t.floor - base) > 1.3 ? MapKind::High : MapKind::Low;
    } else m.kind = MapKind::Floor;
    m.light = (u8)std::clamp((int)std::lround(t.light / 1.6f * 255.0f), 0, 255);
    m.color = rgb565(bank[t.fmat].rgb);
    return m;
}

MapCell ExploreMap::at(i64 x, i64 y) const {
    auto it = chunks_.find(world::chunkOf(x, y));
    if (it == chunks_.end()) return {};
    return it->second.cells[(size_t)((y & world::CS_MASK) * world::CS + (x & world::CS_MASK))];
}

void ExploreMap::mark(i64 x, i64 y, const world::Tile& t, const world::MaterialBank& bank) {
    MapCell& c = chunks_[world::chunkOf(x, y)].cells[(size_t)((y & world::CS_MASK) * world::CS + (x & world::CS_MASK))];
    if (c.kind != MapKind::Unknown && c.kind != MapKind::Pending) return;
    if (c.kind == MapKind::Unknown) ++known_;
    c = describe(t, bank);
    ++rev_;
}

void ExploreMap::reveal(world::World& w, double px, double py, double pz, double dt, bool force) {
    timer_ -= dt;
    double dx0 = px - lastX_, dy0 = py - lastY_;
    if (!force && dx0 * dx0 + dy0 * dy0 < 0.3 * 0.3 && timer_ > 0.0) return;
    timer_ = 0.25;
    lastX_ = px, lastY_ = py;
    const world::MaterialBank& bank = w.bank();
    const i64 cx = ifloor(px), cy = ifloor(py);
    // direkte Umgebung immer (auch die Wand, an der man entlangstreift)
    for (i64 dy = -1; dy <= 1; ++dy)
        for (i64 dx = -1; dx <= 1; ++dx)
            if (const world::Tile* t = w.tileIfLoaded(cx + dx, cy + dy)) mark(cx + dx, cy + dy, *t, bank);
    // Sichtstrahlen rundum (DDA: jede durchquerte Kachel genau einmal, kein Durchrutschen an Ecken)
    const int kRays = 128;
    for (int i = 0; i < kRays; ++i) {
        double a = (i + 0.5) * (6.283185307179586 / kRays);
        double rx = std::cos(a), ry = std::sin(a);
        i64 tx = cx, ty = cy;
        int sx = rx >= 0 ? 1 : -1, sy = ry >= 0 ? 1 : -1;
        double ddx = std::fabs(rx) > 1e-9 ? 1.0 / std::fabs(rx) : 1e30;
        double ddy = std::fabs(ry) > 1e-9 ? 1.0 / std::fabs(ry) : 1e30;
        double tmx = (sx > 0 ? (double)(cx + 1) - px : px - (double)cx) * ddx;
        double tmy = (sy > 0 ? (double)(cy + 1) - py : py - (double)cy) * ddy;
        while (true) {
            double t;
            if (tmx < tmy) {
                t = tmx;
                tmx += ddx;
                tx += sx;
            } else {
                t = tmy;
                tmy += ddy;
                ty += sy;
            }
            if (t > kRadius) break;
            const world::Tile* tile = w.tileIfLoaded(tx, ty);
            if (!tile) break;
            mark(tx, ty, *tile, bank);
            if (tile->solid()) break;
            // hohe Einbauten und deutlich hoehere Boeden versperren die Sicht
            if ((double)tile->floor - pz > 1.25) break;
            if ((double)tile->ceil - std::max((double)tile->floor, pz) < 0.6) break;
        }
    }
}

void ExploreMap::refreshChunk(const world::ChunkData& cd, const world::MaterialBank& bank) {
    auto it = chunks_.find(cd.key);
    if (it == chunks_.end()) return;
    bool any = false;
    for (int ly = 0; ly < world::CS; ++ly)
        for (int lx = 0; lx < world::CS; ++lx) {
            MapCell& c = it->second.cells[(size_t)(ly * world::CS + lx)];
            if (c.kind != MapKind::Pending) continue;
            c = describe(cd.tile(lx, ly), bank);
            any = true;
        }
    if (any) ++rev_;
}

Json ExploreMap::toJson() const {
    std::vector<world::ChunkKey> keys;
    keys.reserve(chunks_.size());
    for (const auto& [k, c] : chunks_) keys.push_back(k);
    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    Json arr = Json::array();
    for (const auto& k : keys) {
        const Chunk& c = chunks_.at(k);
        unsigned char bits[world::CS * world::CS / 8] = {};
        bool any = false;
        for (size_t i = 0; i < c.cells.size(); ++i)
            if (c.cells[i].known()) bits[i >> 3] |= (unsigned char)(1u << (i & 7)), any = true;
        if (!any) continue;
        Json e = Json::array();
        e.push((long long)k.x);
        e.push((long long)k.y);
        e.push(base64Encode(bits, sizeof(bits)));
        arr.push(std::move(e));
    }
    Json j = Json::object();
    j.set("v", 1);
    j.set("chunks", std::move(arr));
    return j;
}

void ExploreMap::fromJson(const Json& j) {
    clear();
    for (const Json& e : j["chunks"].items()) {
        if (!e.isArray() || e.size() < 3) continue;
        auto bits = base64Decode(e[2].asString(""));
        if (bits.size() != world::CS * world::CS / 8) continue;
        Chunk& c = chunks_[world::ChunkKey{e[0].asInt(), e[1].asInt()}];
        for (size_t i = 0; i < c.cells.size(); ++i)
            if (bits[i >> 3] & (1u << (i & 7))) {
                c.cells[i].kind = MapKind::Pending;
                ++known_;
            }
    }
    ++rev_;
}

void ExploreMap::clear() {
    chunks_.clear();
    known_ = 0;
    ++rev_;
    lastX_ = lastY_ = 1e18;
    timer_ = 0.0;
}

}  // namespace lim::game
