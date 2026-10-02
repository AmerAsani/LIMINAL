#include "gameplay/finds.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/rng.hpp"
#include "world/room.hpp"
#include "world/tile.hpp"

namespace lim::game {

namespace {

// Abstand (Kacheln, Schachbrett) zur naechsten Tuer des Raums
i64 doorDistance(const world::Room& r, i64 x, i64 y) {
    i64 best = 1 << 20;
    for (const world::Door* d : r.doors) {
        i64 dx = x < d->x0 ? d->x0 - x : (x >= d->x1 ? x - d->x1 + 1 : 0);
        i64 dy = y < d->y0 ? d->y0 - y : (y >= d->y1 ? y - d->y1 + 1 : 0);
        best = std::min(best, std::max(dx, dy));
    }
    return best;
}

bool inDoor(const world::Room& r, i64 x, i64 y) {
    for (const world::Door* d : r.doors)
        if (d->contains(x, y)) return true;
    return false;
}

// Freie Bodenkachel auf Raumhoehe: keine Stufe, kein Einbau, nicht im Weg einer Tuer
bool freeFloor(const world::Room& r, i64 x, i64 y, i64 doorGap) {
    if (!r.contains(x, y)) return false;
    world::Tile t = r.tile(x, y);
    if (!t.open() || (t.flags & (world::TF_STAIR | world::TF_FEATURE | world::TF_MACHINE | world::TF_DOOR))) return false;
    if (std::fabs(t.floor - (float)r.floor) > 0.01f || t.ceil - t.floor < 2.0f) return false;
    return doorDistance(r, x, y) >= doorGap;
}

}  // namespace

void roomFinds(const world::Room& r, const world::LevelDef& L, std::vector<FindSpot>& out) {
    const i64 W = r.w(), H = r.h();
    if (W * H < 6 || r.type->supply) return;
    const auto& f = L.finds;
    const std::string base = std::format("{},{},{}", r.sx, r.sy, r.index);
    const bool dark = r.lightStyle == "dark" || r.lightStyle == "void";

    // Bodenfunde: Kacheln am Rand bevorzugt (dort legt man Dinge ab), Start je Raum verschieden
    auto floorSpot = [&](u64 salt, FindSpot& s) {
        std::vector<std::pair<i64, i64>> edge, inner;
        for (i64 y = r.y0; y < r.y1; ++y)
            for (i64 x = r.x0; x < r.x1; ++x) {
                bool atEdge = x == r.x0 || y == r.y0 || x == r.x1 - 1 || y == r.y1 - 1;
                (atEdge ? edge : inner).push_back({x, y});
            }
        for (auto* list : {&edge, &inner}) {
            if (list->empty()) continue;
            size_t start = (size_t)(hash64(r.seed, salt) % list->size());
            for (size_t k = 0; k < list->size(); ++k) {
                auto [x, y] = (*list)[(start + k) % list->size()];
                if (!freeFloor(r, x, y, 2)) continue;
                float jx = (float)(hfloat(r.seed, salt, 1) - 0.5) * 0.4f, jy = (float)(hfloat(r.seed, salt, 2) - 0.5) * 0.4f;
                s.pos = {(float)x + 0.5f + jx, (float)y + 0.5f + jy, (float)r.floor};
                s.yaw = (float)(hfloat(r.seed, salt, 3) * 6.283185307);
                s.tx = x, s.ty = y;
                s.hash = hash64(r.seed, salt, 4);
                return true;
            }
        }
        return false;
    };

    if (hfloat(r.seed, 0x7A01) < f.note) {
        FindSpot s;
        s.kind = FindSpot::Note;
        s.id = "note:" + base;
        if (floorSpot(0x7A02, s)) out.push_back(s);
    }
    if (hfloat(r.seed, 0x7B01) < (dark ? f.batteryDark : f.battery)) {
        FindSpot s;
        s.kind = FindSpot::Battery;
        s.id = "find:" + base;
        if (floorSpot(0x7B02, s)) out.push_back(s);
    }

    // Notausgang: selten, erst weit weg vom Start; in einer freien Wand (keine Ecke, keine Tuer,
    // keine Wandleuchte), Raum hoch genug fuer Tuer und Schild
    auto [cx, cy] = r.center();
    double dist = std::hypot(cx, cy);
    double p = std::clamp((dist - f.exitStart) / std::max(f.exitRange, 1.0), 0.0, 1.0) * f.exitMax;
    if (L.nextLevel.empty() || r.height < 2.55 || p <= 0.0 || hfloat(r.seed, 0x7C01) >= p) return;
    struct Cand {
        i64 x, y;
        int dx, dy;  // von der Wand in den Raum
    };
    std::vector<Cand> cands;
    for (i64 x = r.x0 + 1; x < r.x1 - 1; ++x) {
        cands.push_back({x, r.y0, 0, 1});
        cands.push_back({x, r.y1 - 1, 0, -1});
    }
    for (i64 y = r.y0 + 1; y < r.y1 - 1; ++y) {
        cands.push_back({r.x0, y, 1, 0});
        cands.push_back({r.x1 - 1, y, -1, 0});
    }
    if (cands.empty()) return;
    size_t start = (size_t)(hash64(r.seed, 0x7C02) % cands.size());
    for (size_t k = 0; k < cands.size(); ++k) {
        const Cand& c = cands[(start + k) % cands.size()];
        // Kachel und ihre Nachbarn entlang der Wand frei, dahinter massive Wand (keine Tuer)
        i64 ax = c.dy != 0 ? 1 : 0, ay = c.dx != 0 ? 1 : 0;  // Richtung entlang der Wand
        bool ok = true;
        for (int s = -1; s <= 1 && ok; ++s) {
            i64 x = c.x + ax * s, y = c.y + ay * s;
            if (!freeFloor(r, x, y, 2) || (r.tile(x, y).flags & world::TF_WALLLAMP)) ok = false;
            if (inDoor(r, x - c.dx, y - c.dy)) ok = false;
        }
        if (!ok) continue;
        FindSpot s;
        s.kind = FindSpot::Exit;
        s.id = "exit:" + base;
        s.pos = {(float)c.x + 0.5f - c.dx * 0.5f, (float)c.y + 0.5f - c.dy * 0.5f, (float)r.floor};
        s.yaw = std::atan2((float)c.dy, (float)c.dx);
        s.tx = c.x, s.ty = c.y;
        s.hash = hash64(r.seed, 0x7C03);
        out.push_back(s);
        return;
    }
}

}  // namespace lim::game
