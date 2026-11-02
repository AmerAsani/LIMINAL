#include "world/generator.hpp"
#include "world/imported_textures.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

#include "core/math.hpp"
#include "core/rng.hpp"
#include <format>

namespace lim::world {

// ======================================================================================
// Sector
// ======================================================================================

void Sector::buildIndex() {
    index_.assign((size_t)S * (size_t)S, -1);
    for (size_t i = 0; i < rooms.size(); ++i) {
        const Room& r = *rooms[i];
        for (i64 y = r.y0; y < r.y1; ++y)
            for (i64 x = r.x0; x < r.x1; ++x) index_[(size_t)((y - y0) * S + (x - x0))] = (i32)i;
    }
    for (size_t i = 0; i < doors.size(); ++i) {
        const Door& d = *doors[i];
        if (!d.owned) continue;
        for (i64 y = d.y0; y < d.y1; ++y)
            for (i64 x = d.x0; x < d.x1; ++x)
                if (contains(x, y)) index_[(size_t)((y - y0) * S + (x - x0))] = -2 - (i32)i;
    }
}

Room* Sector::roomAt(i64 x, i64 y) const {
    if (!contains(x, y)) return nullptr;
    if (index_.empty()) {
        for (const auto& r : rooms)
            if (r->contains(x, y)) return r.get();
        return nullptr;
    }
    i32 v = index_[(size_t)((y - y0) * S + (x - x0))];
    return v >= 0 ? rooms[(size_t)v].get() : nullptr;
}

const Door* Sector::doorAt(i64 x, i64 y) const {
    if (!contains(x, y) || index_.empty()) return nullptr;
    i32 v = index_[(size_t)((y - y0) * S + (x - x0))];
    return v <= -2 ? doors[(size_t)(-2 - v)].get() : nullptr;
}

// ======================================================================================
// Generator
// ======================================================================================

struct Generator::Leaf {
    i64 x0, y0, x1, y1;
    bool corridor, big, endless;
    bool passage = false;  // V6: breiter Zwischenraum (Passage)
};

struct Generator::Ctx {
    Sector& sector;
    Rng rng;
    // Portale je Kante: (absolute Position, Breite)
    std::vector<std::pair<i64, i64>> left, right, top, bottom;
    double rarity;
    std::vector<Leaf> leaves;
    // BSP-Knoten: leaf >= 0 -> Blatt, sonst Teilung
    struct Node {
        int leaf = -1;
        int axis = 0;
        i64 s = 0, t = 0;
        int a = -1, b = -1;
    };
    std::vector<Node> nodes;
    // Teilungswaende: (axis, s, t, Paare (a, b, lo, hi))
    struct Pair {
        int a, b;
        i64 lo, hi;
    };
    struct Wall {
        int axis;
        i64 s, t;
        std::vector<Pair> pairs;
    };
    std::vector<Wall> walls;

    Ctx(Sector& s, u64 seed) : sector(s), rng(seed) {}
};

Generator::Generator(const LevelDef& level, i64 seed, MaterialBank& bank, double oddsNone, double oddsOne,
                     SupplyRules supplyRules)
    : level_(level),
      seed_(seed),
      bank_(bank),
      zones_(level, seed),
      oddsNone_(oddsNone),
      oddsOne_(oddsOne),
      supplyRules_(std::move(supplyRules)) {
    for (const auto& z : level.zones) {
        zoneFloor_.push_back(planePatternFromName(z.floorPattern));
        zoneCeil_.push_back(planePatternFromName(z.ceilPattern));
        zoneWall_.push_back(wallPatternFromName(z.wallPattern));
    }
    if (level_.mapped()) plan_ = std::make_unique<SpawnPlan>(*level_.map, level_.spawnRules, (u64)seed_);  // V11
}

// --- Portale ----------------------------------------------------------------------------
std::vector<std::pair<i64, i64>> Generator::edgePortals(int kind, i64 ex, i64 ey) const {
    const i64 S = level_.sectorSize;
    Rng rng(hash64(seed_, "portal", kind, ex, ey));
    i64 n = rng.randint(2, 4);
    std::vector<std::pair<i64, i64>> res;
    int tries = 0;
    while ((i64)res.size() < n && tries < 40) {
        ++tries;
        i64 w = rng.random() < 0.7 ? 1 : 2;
        i64 p = rng.randint(6, S - 9);
        bool ok = true;
        for (const auto& [q, qw] : res)
            if (std::abs(p - q) < 11) ok = false;
        if (ok) res.push_back({p, w});
    }
    std::sort(res.begin(), res.end());
    return res;
}

double Generator::rarity(double x, double y) const {
    return 1.0 + std::min(level_.rarityMaxExtra, std::hypot(x, y) / level_.rarityDistance);
}

// --- Sektor -------------------------------------------------------------------------------
std::unique_ptr<Sector> Generator::generateSector(i64 sx, i64 sy) const {
    if (level_.mapped()) return mapSector(sx, sy);
    const i64 S = level_.sectorSize;
    auto sec = std::make_unique<Sector>(sx, sy, (int)S);
    const i64 X0 = sec->x0, Y0 = sec->y0;
    Ctx c(*sec, hash64(seed_, "sector", sx, sy));
    for (auto [p, w] : edgePortals(0, sx, sy)) c.left.push_back({Y0 + p, w});
    for (auto [p, w] : edgePortals(0, sx + 1, sy)) c.right.push_back({Y0 + p, w});
    for (auto [p, w] : edgePortals(1, sx, sy)) c.top.push_back({X0 + p, w});
    for (auto [p, w] : edgePortals(1, sx, sy + 1)) c.bottom.push_back({X0 + p, w});
    c.rarity = rarity((double)X0 + (double)S / 2.0, (double)Y0 + (double)S / 2.0);

    int root = bsp(c, X0 + 1, Y0 + 1, X0 + S, Y0 + S, 0);
    makeRooms(c);
    collect(c, root);
    makeDoors(c);
    makePortals(c);
    resolveFloors(*sec);
    resolveHeights(*sec);
    for (auto& room : sec->rooms) style(*room);
    for (auto& door : sec->doors) doorStyle(*door);
    // V3: Versorgungsraeume mit Theke und (seltenen) Items
    for (auto& room : sec->rooms) {
        if (room->type->supply) {
            room->planSupply(level_, oddsNone_, oddsOne_, supplyRules_, supplyScale_);
            room->avgLight = std::min(1.4, room->avgLight + 0.2);  // Licht faellt in den Flur
        }
    }
    for (auto& room : sec->rooms) {
        if (!room->items.empty()) sec->itemRooms.push_back(room.get());
        if (room->hasMachine()) sec->supplyRooms.push_back(room.get());
    }
    sec->buildIndex();
    return sec;
}

// --- BSP ------------------------------------------------------------------------------------
bool Generator::wallOk(const Ctx& c, int axis, i64 s, i64 t, i64 x0, i64 y0, i64 x1, i64 y1) const {
    const Sector& sec = c.sector;
    const i64 S = sec.S;
    auto check = [&](const std::vector<std::pair<i64, i64>>& edge) {
        for (const auto& [p, w] : edge)
            if (!(s + t <= p - 1 || s >= p + w + 1)) return false;
        return true;
    };
    if (axis == 0) {  // senkrechte Wand -> betrifft Portale oben/unten
        if (y0 == sec.y0 + 1 && !check(c.top)) return false;
        if (y1 == sec.y0 + S && !check(c.bottom)) return false;
    } else {          // waagrechte Wand -> betrifft Portale links/rechts
        if (x0 == sec.x0 + 1 && !check(c.left)) return false;
        if (x1 == sec.x0 + S && !check(c.right)) return false;
    }
    return true;
}

bool Generator::pick(Ctx& c, int axis, i64 lo, i64 hi, i64 t, i64 x0, i64 y0, i64 x1, i64 y1, i64& out) const {
    for (int k = 0; k < 16; ++k) {
        i64 a = c.rng.randint(lo, hi);
        i64 b = c.rng.randint(lo, hi);
        i64 s = floorDiv(a + b, 2);
        if (wallOk(c, axis, s, t, x0, y0, x1, y1)) {
            out = s;
            return true;
        }
    }
    return false;
}

int Generator::leaf(Ctx& c, i64 x0, i64 y0, i64 x1, i64 y1, bool corridor, bool big, bool endless) const {
    c.leaves.push_back({x0, y0, x1, y1, corridor, big, endless});
    Ctx::Node n;
    n.leaf = (int)c.leaves.size() - 1;
    c.nodes.push_back(n);
    return (int)c.nodes.size() - 1;
}

int Generator::bsp(Ctx& c, i64 x0, i64 y0, i64 x1, i64 y1, int depth) const {
    Rng& rng = c.rng;
    const i64 w = x1 - x0, h = y1 - y0;
    ZoneWeights zw = zones_.weights((double)(x0 + x1) * 0.5, (double)(y0 + y1) * 0.5);
    const int nz = zones_.count();
    i64 minLeaf = std::max<i64>(2, (i64)pyRound(zones_.blend(zw, &ZoneDef::minLeaf)));
    double maxLeaf = zones_.blend(zw, &ZoneDef::maxLeaf);
    const size_t domIdx = rng.weighted(zw.data(), (size_t)nz);
    const ZoneDef& dom = level_.zones[domIdx];
    i64 t = rng.randint(dom.wallThickness.lo, dom.wallThickness.hi);

    const bool v6 = level_.layout >= kLayoutV6;
    // V8: in der Welt gelten Passagen und Fluchten der Ebene, die hier liegt
    const StructureDef& st = level_.structureFor((int)domIdx);
    // Sehr selten: ein Gang durch den gesamten Sektor ("endloser Gang")
    if (depth == 0 && rng.random() < 0.07 * c.rarity * (v6 ? st.endlessMul : 1.0)) {
        i64 cw = rng.randint(3, 4);
        int ax = (int)rng.randint(0, 1);
        int node = corridorSplit(c, x0, y0, x1, y1, depth, t, cw, ax, minLeaf, true);
        if (node >= 0) return node;
    }

    // V6: Hauptachsen werden zu breiten Passagen - offene Zwischenraeume mit Raeumen zu beiden Seiten
    if (v6 && depth <= 1 && !st.passageTypes.empty() && std::min(w, h) >= 40 &&
        rng.random() < st.passageChance * (depth == 0 ? 1.0 : 0.45)) {
        i64 pw = rng.randint(st.passageWidth.lo, st.passageWidth.hi);
        int ax = w >= h ? 0 : 1;
        if (rng.random() < 0.35) ax = 1 - ax;
        int node = corridorSplit(c, x0, y0, x1, y1, depth, t, pw, ax, minLeaf, false, true);
        if (node >= 0) return node;
    }

    if (depth > 0 && (double)w <= maxLeaf && (double)h <= maxLeaf && rng.random() < 0.55)
        return leaf(c, x0, y0, x1, y1);
    if (depth > 0 && std::min(w, h) >= 12 && (double)std::max(w, h) <= std::max(30.0, maxLeaf * 1.5) &&
        rng.random() < zones_.blend(zw, &ZoneDef::bigChance))
        return leaf(c, x0, y0, x1, y1, false, true);

    int axis;
    if ((double)w > (double)h * 1.25) axis = 0;
    else if ((double)h > (double)w * 1.25) axis = 1;
    else axis = (int)rng.randint(0, 1);
    i64 length = 0, span = 0;
    bool found = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        length = axis == 0 ? w : h;
        span = axis == 0 ? h : w;
        if (length >= 2 * minLeaf + t) {
            found = true;
            break;
        }
        axis = 1 - axis;
    }
    if (!found) return leaf(c, x0, y0, x1, y1);

    // Korridorstreifen einfuegen (Flur mit Raeumen links und rechts)
    if (span >= 12 && length >= 2 * minLeaf + 2 * t + 2 &&
        rng.random() < zones_.blend(zw, &ZoneDef::corridorChance) * (depth < 5 ? 1.0 : 0.35)) {
        i64 cw = rng.randint(dom.corridorWidth.lo, dom.corridorWidth.hi);
        int node = corridorSplit(c, x0, y0, x1, y1, depth, t, cw, axis, minLeaf, false);
        if (node >= 0) return node;
    }

    i64 start = axis == 0 ? x0 : y0;
    i64 end = axis == 0 ? x1 : y1;
    i64 s;
    if (!pick(c, axis, start + minLeaf, end - minLeaf - t, t, x0, y0, x1, y1, s)) return leaf(c, x0, y0, x1, y1);
    int left, right;
    if (axis == 0) {
        left = bsp(c, x0, y0, s, y1, depth + 1);
        right = bsp(c, s + t, y0, x1, y1, depth + 1);
    } else {
        left = bsp(c, x0, y0, x1, s, depth + 1);
        right = bsp(c, x0, s + t, x1, y1, depth + 1);
    }
    Ctx::Node n;
    n.axis = axis;
    n.s = s;
    n.t = t;
    n.a = left;
    n.b = right;
    c.nodes.push_back(n);
    return (int)c.nodes.size() - 1;
}

int Generator::corridorSplit(Ctx& c, i64 x0, i64 y0, i64 x1, i64 y1, int depth, i64 t, i64 cw, int axis, i64 minLeaf,
                             bool endless, bool passage) const {
    Rng& rng = c.rng;
    i64 start = axis == 0 ? x0 : y0;
    i64 end = axis == 0 ? x1 : y1;
    i64 lo = start + minLeaf;
    i64 hi = end - minLeaf - 2 * t - cw;
    if (hi < lo) return -1;
    i64 s1 = 0, s2 = 0;
    bool ok = false;
    for (int k = 0; k < 16; ++k) {
        i64 a = rng.randint(lo, hi);
        i64 b = rng.randint(lo, hi);
        s1 = floorDiv(a + b, 2);
        s2 = s1 + t + cw;
        if (wallOk(c, axis, s1, t, x0, y0, x1, y1) && wallOk(c, axis, s2, t, x0, y0, x1, y1)) {
            ok = true;
            break;
        }
    }
    if (!ok) return -1;
    i64 span = axis == 0 ? (y1 - y0) : (x1 - x0);
    int left, corr, right;
    if (axis == 0) {
        left = bsp(c, x0, y0, s1, y1, depth + 1);
        corr = leaf(c, s1 + t, y0, s2, y1, true, false, endless && span >= 60);
        c.leaves.back().passage = passage;
        right = bsp(c, s2 + t, y0, x1, y1, depth + 1);
    } else {
        left = bsp(c, x0, y0, x1, s1, depth + 1);
        corr = leaf(c, x0, s1 + t, x1, s2, true, false, endless && span >= 60);
        c.leaves.back().passage = passage;
        right = bsp(c, x0, s2 + t, x1, y1, depth + 1);
    }
    Ctx::Node inner;
    inner.axis = axis;
    inner.s = s2;
    inner.t = t;
    inner.a = corr;
    inner.b = right;
    c.nodes.push_back(inner);
    int innerIdx = (int)c.nodes.size() - 1;
    Ctx::Node outer;
    outer.axis = axis;
    outer.s = s1;
    outer.t = t;
    outer.a = left;
    outer.b = innerIdx;
    c.nodes.push_back(outer);
    return (int)c.nodes.size() - 1;
}

// --- Raumtypen ---------------------------------------------------------------------------
static double affinity(const RoomTypeDef& t, const ZoneWeights& zw, int nz) {
    double s = 0.0;
    for (int i = 0; i < nz; ++i) s += t.affinity[(size_t)i] * zw[(size_t)i];
    return s + 1e-4;
}

static const RoomTypeDef& chooseType(const LevelDef& L, Rng& rng, i64 w, i64 h, bool corridor, bool big, bool endless,
                                     const ZoneWeights& zw, double rarity) {
    const int nz = L.zoneCount();
    i64 sh = std::min(w, h), lg = std::max(w, h);
    std::vector<const RoomTypeDef*> cands;
    std::vector<double> weights;
    if (corridor || (sh <= 2 && lg >= 6)) {
        if (endless) return L.type(L.endlessType);
        for (int k : L.corridorTypes)
            if (L.type(k).fits(sh, lg)) cands.push_back(&L.type(k));
        if (cands.empty()) cands.push_back(&L.type(L.defaultCorridorType));
        for (auto* t : cands) weights.push_back(affinity(*t, zw, nz));
        return *cands[rng.weighted(weights)];
    }
    double r = rng.random();
    double pRare = 0.03 * rarity;
    double pUnusual = 0.07 * rarity;
    std::vector<Category> order;
    if (r < pRare) order = {Category::Rare, Category::Unusual, Category::Large, Category::Normal};
    else if (r < pRare + pUnusual) order = {Category::Unusual, Category::Large, Category::Normal};
    else if (big || sh >= 18) order = {Category::Large, Category::Normal};
    else order = {Category::Normal};
    for (Category cat : order) {
        cands.clear();
        weights.clear();
        for (int k : L.byCategory[(size_t)cat])
            if (L.type(k).fits(sh, lg)) cands.push_back(&L.type(k));
        if (!cands.empty()) {
            for (auto* t : cands) weights.push_back(affinity(*t, zw, nz));
            return *cands[rng.weighted(weights)];
        }
    }
    return L.type(L.fallbackType);
}

// V8: Raumtyp in der zusammenhaengenden Welt - nur Typen der Ebenen, die hier wirklich liegen.
// Uebergangsraeume (Ebene -1) erscheinen fast nur an den Nahtstellen zweier Ebenen.
static const RoomTypeDef& chooseTypeWorld(const LevelDef& L, const ZoneField& zf, Rng& rng, i64 w, i64 h, bool corridor,
                                          bool big, bool endless, const ZoneWeights& zw, double rarity, int zone) {
    const int nz = L.zoneCount();
    const RegionDef& R = L.regions[(size_t)L.regionOfZone(zone)];
    const auto share = zf.regionShare(zw);
    double top = 0.0;
    for (double v : share) top = std::max(top, v);
    const double seamness = std::clamp((1.0 - top) * 2.5, 0.0, 1.0);
    auto weight = [&](const RoomTypeDef& t) -> double {
        double a = 0.0;
        for (int i = 0; i < nz; ++i) a += t.affinity[(size_t)i] * zw[(size_t)i];
        if (t.region < 0) {
            // nur dort, wo (mindestens zwei) der Ebenen zusammentreffen, die er verbindet
            int meet = 0;
            for (size_t k = 0; k < share.size(); ++k)
                if ((t.seamMask >> k & 1u) && share[k] >= 0.08) ++meet;
            if (meet < 2) return 0.0;
            return a * std::min(1.0, seamness * t.seam);
        }
        if (share[(size_t)t.region] < 0.15) return 0.0;
        return a + 1e-4;
    };
    const i64 sh = std::min(w, h), lg = std::max(w, h);
    std::vector<const RoomTypeDef*> cands;
    std::vector<double> weights;
    auto gather = [&](const std::vector<int>& list) {
        cands.clear();
        weights.clear();
        for (int k : list) {
            const RoomTypeDef& t = L.type(k);
            if (!t.fits(sh, lg)) continue;
            double wt = weight(t);
            if (wt <= 1e-6) continue;
            cands.push_back(&t);
            weights.push_back(wt);
        }
    };
    if (corridor || (sh <= 2 && lg >= 6)) {
        if (endless && R.endlessType >= 0) return L.type(R.endlessType);
        gather(L.corridorTypes);
        if (cands.empty()) return L.type(R.defaultCorridorType);
        return *cands[rng.weighted(weights)];
    }
    double r = rng.random();
    double pRare = 0.03 * rarity;
    double pUnusual = 0.07 * rarity;
    std::vector<Category> order;
    if (r < pRare) order = {Category::Rare, Category::Unusual, Category::Large, Category::Normal};
    else if (r < pRare + pUnusual) order = {Category::Unusual, Category::Large, Category::Normal};
    else if (big || sh >= 18) order = {Category::Large, Category::Normal};
    else order = {Category::Normal};
    for (Category cat : order) {
        gather(L.byCategory[(size_t)cat]);
        if (!cands.empty()) return *cands[rng.weighted(weights)];
    }
    return L.type(R.fallbackType);
}

void Generator::makeRooms(Ctx& c) const {
    Sector& sec = c.sector;
    const int nz = zones_.count();
    for (size_t i = 0; i < c.leaves.size(); ++i) {
        const Leaf& lf = c.leaves[i];
        double cx = (double)(lf.x0 + lf.x1) * 0.5;
        double cy = (double)(lf.y0 + lf.y1) * 0.5;
        ZoneWeights zw = zones_.weights(cx, cy);
        int zone = (int)c.rng.weighted(zw.data(), (size_t)nz);
        const auto& pt = level_.structureFor(zone).passageTypes;
        const RoomTypeDef* rtp;
        if (lf.passage && !pt.empty()) rtp = &level_.type(pt[(size_t)c.rng.randint(0, (i64)pt.size() - 1)]);
        else if (level_.world())
            rtp = &chooseTypeWorld(level_, zones_, c.rng, lf.x1 - lf.x0, lf.y1 - lf.y0, lf.corridor || lf.passage, lf.big,
                                   lf.endless, zw, c.rarity, zone);
        else
            rtp = &chooseType(level_, c.rng, lf.x1 - lf.x0, lf.y1 - lf.y0, lf.corridor, lf.big, lf.endless, zw, c.rarity);
        const RoomTypeDef& rt = *rtp;
        u64 seed = hash64(seed_, "room", sec.sx, sec.sy, (i64)i);
        sec.rooms.push_back(std::make_unique<Room>(sec.sx, sec.sy, (int)i, lf.x0, lf.y0, lf.x1, lf.y1, &rt, zw, zone,
                                                   seed, lf.corridor || rt.category == Category::Corridor));
        // V8: seltene Aufzuege (Hash, keine Zufallszahl; erst weit genug vom Start)
        if (level_.world() && level_.elevators.chance > 0.0) {
            Room& r = *sec.rooms.back();
            if (!r.isCorridor && !rt.supply && rt.region >= 0 && std::hypot(cx, cy) >= level_.elevators.start &&
                std::min(r.w(), r.h()) >= 6 && hfloat(seed, 0xE1E7) < level_.elevators.chance)
                r.elevatorWanted = true;
        }
    }
}

std::vector<int> Generator::collect(Ctx& c, int node) const {
    const Ctx::Node n = c.nodes[(size_t)node];
    if (n.leaf >= 0) return {n.leaf};
    std::vector<int> la = collect(c, n.a);
    std::vector<int> lb = collect(c, n.b);
    const auto& L = c.leaves;
    Ctx::Wall wall{n.axis, n.s, n.t, {}};
    if (n.axis == 0) {
        for (int a : la) {
            if (L[(size_t)a].x1 != n.s) continue;
            for (int b : lb) {
                if (L[(size_t)b].x0 != n.s + n.t) continue;
                i64 lo = std::max(L[(size_t)a].y0, L[(size_t)b].y0), hi = std::min(L[(size_t)a].y1, L[(size_t)b].y1);
                if (hi - lo >= 1) wall.pairs.push_back({a, b, lo, hi});
            }
        }
    } else {
        for (int a : la) {
            if (L[(size_t)a].y1 != n.s) continue;
            for (int b : lb) {
                if (L[(size_t)b].y0 != n.s + n.t) continue;
                i64 lo = std::max(L[(size_t)a].x0, L[(size_t)b].x0), hi = std::min(L[(size_t)a].x1, L[(size_t)b].x1);
                if (hi - lo >= 1) wall.pairs.push_back({a, b, lo, hi});
            }
        }
    }
    c.walls.push_back(std::move(wall));
    la.insert(la.end(), lb.begin(), lb.end());
    return la;
}

// --- Tueren ------------------------------------------------------------------------------
void Generator::doorSpec(Rng& rng, const Room& ra, const Room& rb, i64 overlap, DoorKind& kind, i64& wd) const {
    bool bigA = ra.type->category == Category::Large || ra.type->biggish;
    bool bigB = rb.type->category == Category::Large || rb.type->biggish;
    if (bigA && bigB && overlap >= 8 && rng.random() < 0.55) {
        kind = DoorKind::Opening;
        wd = std::min<i64>(overlap - 2, rng.randint(3, 7));
        return;
    }
    if ((bigA || bigB) && overlap >= 5 && rng.random() < 0.45) {
        kind = DoorKind::Gate;
        wd = std::min<i64>(overlap - 2, rng.randint(2, 3));
        return;
    }
    if (overlap >= 4 && level_.zones[(size_t)ra.zone].wideDoors && level_.zones[(size_t)rb.zone].wideDoors &&
        rng.random() < 0.18) {
        kind = DoorKind::Wide;
        wd = 2;
        return;
    }
    kind = DoorKind::Door;
    wd = 1;
}

void Generator::addDoor(Sector& sec, int axis, i64 s, i64 t, Room* ra, Room* rb, i64 p, i64 wd, DoorKind kind) const {
    auto door = std::make_unique<Door>();
    if (axis == 0) {
        door->x0 = s, door->y0 = p, door->x1 = s + t, door->y1 = p + wd;
    } else {
        door->x0 = p, door->y0 = s, door->x1 = p + wd, door->y1 = s + t;
    }
    door->axis = axis;
    door->roomA = ra;
    door->roomB = rb;
    door->kind = kind;
    ra->doors.push_back(door.get());
    rb->doors.push_back(door.get());
    sec.doors.push_back(std::move(door));
}

static bool freeSpot(const std::vector<std::pair<i64, i64>>& used, i64 p, i64 wd) {
    for (const auto& [u0, u1] : used)
        if (!(p + wd + 1 <= u0 || p >= u1 + 1)) return false;
    return true;
}

void Generator::makeDoors(Ctx& c) const {
    Rng& rng = c.rng;
    Sector& sec = c.sector;
    auto& rooms = sec.rooms;
    // V6: Fluchten statt Tueren - Flure gehen offen ineinander ueber, grosse Raeume oeffnen sich breit
    // zum Flur. Nur im V6-Aufbau (verbraucht sonst keine Zufallszahlen).
    const bool v6 = level_.layout >= kLayoutV6;
    auto open = [&](const Room& ra, const Room& rb, i64 overlap, DoorKind& kind, i64& wd) {
        if (!v6) return;
        const StructureDef& st = level_.structureFor(ra.zone);
        bool bigA = ra.type->category == Category::Large || ra.type->biggish;
        bool bigB = rb.type->category == Category::Large || rb.type->biggish;
        if (ra.isCorridor && rb.isCorridor && overlap >= 2 && overlap <= 10 && rng.random() < st.openJoin) {
            kind = DoorKind::Opening;
            wd = overlap;  // ganze Breite: der Flur laeuft ohne Tuer weiter
        } else if (((ra.isCorridor && bigB) || (rb.isCorridor && bigA)) && overlap >= 6 && rng.random() < st.openLarge) {
            kind = DoorKind::Opening;
            wd = std::min<i64>(overlap - 2, rng.randint(3, 8));
        }
    };
    for (const auto& wall : c.walls) {
        const auto& pairs = wall.pairs;
        if (pairs.empty()) continue;
        const int axis = wall.axis;
        const i64 s = wall.s, t = wall.t;
        std::vector<std::pair<i64, i64>> used;
        // Pflichttuer (spannender Baum -> alles bleibt erreichbar)
        std::vector<size_t> good;
        for (size_t i = 0; i < pairs.size(); ++i)
            if (pairs[i].hi - pairs[i].lo >= 3) good.push_back(i);
        size_t tree;
        if (!good.empty()) {
            std::vector<double> weights;
            for (size_t i : good) {
                const auto& p = pairs[i];
                weights.push_back((rooms[(size_t)p.a]->isCorridor || rooms[(size_t)p.b]->isCorridor) ? 3.0 : 1.0);
            }
            tree = good[rng.weighted(weights)];
        } else {
            tree = 0;
            for (size_t i = 1; i < pairs.size(); ++i)
                if (pairs[i].hi - pairs[i].lo > pairs[tree].hi - pairs[tree].lo) tree = i;
        }
        i64 lo = pairs[tree].lo, hi = pairs[tree].hi;
        {
            Room* ra = rooms[(size_t)pairs[tree].a].get();
            Room* rb = rooms[(size_t)pairs[tree].b].get();
            DoorKind kind;
            i64 wd;
            doorSpec(rng, *ra, *rb, hi - lo, kind, wd);
            open(*ra, *rb, hi - lo, kind, wd);
            i64 margin = (hi - lo >= wd + 2) ? 1 : 0;
            if (hi - lo < wd + 2 * margin) {
                kind = DoorKind::Door;
                wd = 1;
            }
            i64 p = rng.randint(lo + margin, std::max(lo + margin, hi - margin - wd));
            used.push_back({p, p + wd});
            addDoor(sec, axis, s, t, ra, rb, p, wd, kind);
        }

        // Zusaetzliche Tueren -> Schleifen, Flure mit vielen Tueren
        double mid = (double)(lo + hi) * 0.5;
        double wx = axis == 0 ? (double)s : mid;
        double wy = axis == 0 ? mid : (double)s;
        double loop = zones_.blend(zones_.weights(wx, wy), &ZoneDef::loopChance);
        for (size_t pi = 0; pi < pairs.size(); ++pi) {
            const auto& pr = pairs[pi];
            Room* ra = rooms[(size_t)pr.a].get();
            Room* rb = rooms[(size_t)pr.b].get();
            i64 plo = pr.lo, phi = pr.hi;
            i64 overlap = phi - plo;
            if (overlap < 3) continue;
            if (ra->type->multiDoors || rb->type->multiDoors) {
                i64 p = plo + 1;
                while (p + 1 <= phi - 1) {
                    if (freeSpot(used, p, 1) && rng.random() < 0.85) {
                        used.push_back({p, p + 1});
                        addDoor(sec, axis, s, t, ra, rb, p, 1, DoorKind::Door);
                    }
                    p += 3;
                }
                continue;
            }
            if (pi == tree) continue;
            double prob = loop;
            if (ra->isCorridor || rb->isCorridor) prob = std::max(prob, 0.6);
            for (const RoomTypeDef* rt : {ra->type, rb->type})
                if (rt->extraDoors != 0.0) prob = std::max(prob, rt->extraDoors);
            if (rng.random() >= prob) continue;
            DoorKind kind;
            i64 wd;
            doorSpec(rng, *ra, *rb, overlap, kind, wd);
            open(*ra, *rb, overlap, kind, wd);
            if (v6 && kind == DoorKind::Opening && wd == overlap) {
                if (freeSpot(used, plo, wd)) {
                    used.push_back({plo, plo + wd});
                    addDoor(sec, axis, s, t, ra, rb, plo, wd, kind);
                }
                continue;
            }
            if (overlap < wd + 2) {
                kind = DoorKind::Door;
                wd = 1;
            }
            for (int k = 0; k < 6; ++k) {
                i64 p = rng.randint(plo + 1, std::max(plo + 1, phi - 1 - wd));
                if (freeSpot(used, p, wd)) {
                    used.push_back({p, p + wd});
                    addDoor(sec, axis, s, t, ra, rb, p, wd, kind);
                    break;
                }
            }
        }
    }
}

void Generator::makePortals(Ctx& c) const {
    Sector& sec = c.sector;
    const i64 S = sec.S, X0 = sec.x0, Y0 = sec.y0, sx = sec.sx, sy = sec.sy;
    struct Spec {
        const std::vector<std::pair<i64, i64>>* list;
        int edge;  // 0 links, 1 oben, 2 rechts, 3 unten
    };
    const Spec specs[4] = {{&c.left, 0}, {&c.top, 1}, {&c.right, 2}, {&c.bottom, 3}};
    for (const Spec& sp : specs) {
        for (const auto& [p, w] : *sp.list) {
            i64 ix, iy;
            switch (sp.edge) {
                case 0: ix = X0 + 1, iy = p; break;
                case 1: ix = p, iy = Y0 + 1; break;
                case 2: ix = X0 + S - 1, iy = p; break;
                default: ix = p, iy = Y0 + S - 1; break;
            }
            Room* room = nullptr;
            for (auto& r : sec.rooms)
                if (r->contains(ix, iy)) {
                    room = r.get();
                    break;
                }
            if (!room) continue;
            auto door = std::make_unique<Door>();
            switch (sp.edge) {
                case 0: door->x0 = X0, door->y0 = p, door->x1 = X0 + 1, door->y1 = p + w, door->axis = 0; break;
                case 1: door->x0 = p, door->y0 = Y0, door->x1 = p + w, door->y1 = Y0 + 1, door->axis = 1; break;
                case 2: door->x0 = X0 + S, door->y0 = p, door->x1 = X0 + S + 1, door->y1 = p + w, door->axis = 0; break;
                default: door->x0 = p, door->y0 = Y0 + S, door->x1 = p + w, door->y1 = Y0 + S + 1, door->axis = 1; break;
            }
            bool remoteA = sp.edge < 2;
            if (remoteA) door->roomB = room;
            else door->roomA = room;
            door->kind = DoorKind::Door;
            door->portal = true;
            door->owned = remoteA;
            door->remoteSide = remoteA ? 'a' : 'b';
            switch (sp.edge) {
                case 0: door->remoteSx = sx - 1, door->remoteSy = sy; break;
                case 1: door->remoteSx = sx, door->remoteSy = sy - 1; break;
                case 2: door->remoteSx = sx + 1, door->remoteSy = sy; break;
                default: door->remoteSx = sx, door->remoteSy = sy + 1; break;
            }
            door->sill = 0.0;
            door->top = 2.2;
            room->fixedFloor = true;
            room->doors.push_back(door.get());
            sec.doors.push_back(std::move(door));
        }
    }
}

// --- Hoehen -----------------------------------------------------------------------------
void Generator::resolveFloors(Sector& sec) const {
    for (auto& rp : sec.rooms) {
        Room& room = *rp;
        Rng rng(room.seed ^ 0xF100ULL);
        double depth = 0.0;
        const RoomTypeDef& rt = *room.type;
        const ZoneDef& z = level_.zones[(size_t)room.zone];
        if (room.fixedFloor || rt.supply) {
            depth = 0.0;
        } else if (rt.sunken && rng.random() < (*rt.sunken)[0]) {
            depth = rng.uniform((*rt.sunken)[1], (*rt.sunken)[2]);
        } else if (z.sunken > 0.0 && rng.random() < z.sunken) {
            depth = z.sunkenDepth;
        }
        if (depth > 0.0) {
            double step = room.stairStep;
            for (Door* d : room.doors) depth = std::min(depth, (double)(room.depthFrom(*d) - 2) * step);
            depth = pyRound2(std::floor(depth / 0.6 + 1e-6) * 0.6);
        }
        room.floor = depth > 0.0 ? -depth : 0.0;
    }
    // Fixpunkt: Raeume anheben, in denen die Treppe zur Tuer nicht passt.
    bool changed = true;
    int guard = 0;
    while (changed && guard < 50) {
        changed = false;
        ++guard;
        for (auto& dp : sec.doors) {
            Door& d = *dp;
            Room* a = d.roomA;
            Room* b = d.roomB;
            if (!a || !b || d.portal) continue;
            if (std::fabs(a->floor - b->floor) < 1e-6) continue;
            Room* lo = a->floor < b->floor ? a : b;
            Room* hi = a->floor < b->floor ? b : a;
            double diff = hi->floor - lo->floor;
            i64 n = std::max<i64>(1, (i64)std::ceil(diff / lo->stairStep - 1e-6) - 1);
            if (n + 3 > lo->depthFrom(d) && !lo->fixedFloor) {
                lo->floor = hi->floor;
                changed = true;
            }
        }
    }
    for (auto& dp : sec.doors) {
        if (dp->portal) continue;
        dp->sill = std::max(dp->roomA->floor, dp->roomB->floor);
    }
}

void Generator::resolveHeights(Sector& sec) const {
    for (auto& rp : sec.rooms) {
        Room& room = *rp;
        Rng r(room.seed ^ 0x4E16ULL);
        const RoomTypeDef& rt = *room.type;
        const ZoneDef& z = level_.zones[(size_t)room.zone];
        double hgt;
        if (rt.height) {
            hgt = r.uniform(rt.height->lo, rt.height->hi);
        } else if (room.isCorridor) {
            hgt = r.uniform(z.hCorridor.lo, z.hCorridor.hi);
        } else if (rt.category == Category::Large) {
            double k = std::min(1.0, std::max(0.0, (double)(std::min(room.w(), room.h()) - 12) / 30.0));
            double lo = z.hBig.lo, hi = z.hBig.hi;
            hgt = lo + (hi - lo) * (0.5 * k + 0.5 * r.random());
        } else {
            hgt = r.uniform(z.hRoom.lo, z.hRoom.hi);
        }
        for (Door* d : room.doors) {
            double need = (d->sill - room.floor) + z.doorClearance;
            hgt = std::max(hgt, need);
        }
        if (room.fixedFloor) hgt = std::max(hgt, 2.6);
        room.height = pyRound(hgt * 20.0) / 20.0;
    }
    for (auto& dp : sec.doors) {
        Door& d = *dp;
        if (d.portal) continue;
        Room* a = d.roomA;
        Room* b = d.roomB;
        double dh = std::min(level_.zones[(size_t)a->zone].doorHeight, level_.zones[(size_t)b->zone].doorHeight);
        double cmin = std::min(a->ceil(), b->ceil());
        double top;
        switch (d.kind) {
            case DoorKind::Wide: top = d.sill + dh + 0.3; break;
            case DoorKind::Gate: top = d.sill + std::max(dh, std::min(4.5, (cmin - d.sill) * 0.6)); break;
            case DoorKind::Opening:
                // V6: Flur geht in Flur ueber - kein Sturz
                if (level_.layout >= kLayoutV6 && a->isCorridor && b->isCorridor) top = cmin;
                else top = cmin - hfloat(a->seed, b->seed, 7) * 1.5;
                break;
            default: top = d.sill + dh; break;
        }
        top = std::min(top, cmin - 0.05);
        d.top = pyRound(std::max(top, d.sill + 1.9) * 20.0) / 20.0;
    }
}

// --- Stil: Materialien, Licht, Nebel -------------------------------------------------------
void Generator::style(Room& room) const {
    MaterialBank& bank = bank_;
    Rng rng(room.seed ^ 0x57A1ULL);
    const ZoneWeights& zw = room.zoneW;
    const ZoneDef& z = level_.zones[(size_t)room.zone];
    const RoomTypeDef& rt = *room.type;
    const int nz = zones_.count();
    double var = rng.uniform(0.9, 1.06);
    // V8: in der Welt mischen sich die Farben vieler Zonen - gestufte Werte halten die Zahl der
    // Materialien im Rahmen (die Bank fasst 16368)
    const bool coarse = level_.world();
    if (coarse) var = 0.9 + std::round((var - 0.9) / 0.04) * 0.04;
    auto snap = [&](Rgb c) {
        if (coarse)
            for (double& v : c) v = std::round(v * 32.0) / 32.0;
        return c;
    };

    auto col = [&](Rgb ZoneDef::*attr) {
        Rgb c = zones_.mix(zw, attr);
        return snap(Rgb{c[0] * var, c[1] * var, c[2] * var});
    };
    Rgb wall = col(&ZoneDef::wall);
    Rgb floor = col(&ZoneDef::floor);
    Rgb ceil = col(&ZoneDef::ceil);
    Rgb trim = col(&ZoneDef::trim);
    Rgb lamp = snap(zones_.mix(zw, &ZoneDef::lamp));
    // Uebergangsbereiche: Boden aus der zweitstaerksten Zone
    std::vector<int> order(nz);
    for (int i = 0; i < nz; ++i) order[(size_t)i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return zw[(size_t)a] > zw[(size_t)b]; });
    PlanePattern floorPat = zoneFloor_[(size_t)room.zone];
    if (nz > 1 && zw[(size_t)order[1]] > 0.27) floorPat = zoneFloor_[(size_t)order[1]];
    WallPattern wallPat = zoneWall_[(size_t)room.zone];
    PlanePattern ceilPat = zoneCeil_[(size_t)room.zone];
    // Raumtyp-spezifischer Stil (V4: Lichtraum, Schachbrettraum)
    const StyleOverride& so = rt.style;
    if (so.wall) wall = *so.wall;
    if (so.floor) floor = *so.floor;
    if (so.ceil) ceil = *so.ceil;
    if (so.lamp) lamp = *so.lamp;
    if (so.wallPattern) wallPat = wallPatternFromName(*so.wallPattern);
    if (so.floorPattern) floorPat = planePatternFromName(*so.floorPattern);
    if (so.ceilPattern) ceilPat = planePatternFromName(*so.ceilPattern);
    // V12: Texturen aus Bilddateien (Zone, Raumtyp ueberschreibt); unbekannte Namen fallen auf das Muster zurueck
    auto texOf = [](const std::string& name) -> u8 {
        const int l = name.empty() ? -1 : importedLayer(name);
        return l < 0 ? (u8)255 : (u8)l;
    };
    u8 wallTex = texOf(so.wallTex ? *so.wallTex : z.wallTex);
    u8 floorTex = texOf(so.floorTex ? *so.floorTex : z.floorTex);
    u8 ceilTex = texOf(so.ceilTex ? *so.ceilTex : z.ceilTex);

    std::vector<Band> bands;
    u16 trimId = bank.get(trim, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Trim);
    room.baseboard = z.baseboard;
    if (z.baseboard) bands.push_back({0.0, 0.12, trimId});
    if (z.bandHi > z.bandLo) bands.push_back({z.bandLo, z.bandHi, trimId});  // V11: lackiertes Wandband
    if (z.dampHi > z.dampLo) {
        // V12: Feuchtigkeitsrand - die Wand zieht Wasser aus dem Teppich. Buendig in acht Stufen, die nach oben
        // ausklingen (unten dunkel, oben kaum noch), damit er wie ein Rand wirkt und nicht wie ein Anstrich
        const double wsd = wallPat == WallPattern::Wallpaper ? 0.86 : 0.72;
        const int steps = 8;
        for (int k = 0; k < steps; ++k) {
            const double t = (double)k / steps, f = z.dampFactor + (1.0 - z.dampFactor) * t * t;
            const double a = z.dampLo + (z.dampHi - z.dampLo) * t, e = z.dampLo + (z.dampHi - z.dampLo) * (t + 1.0 / steps);
            u16 d = bank.get({wall[0] * f, wall[1] * f, wall[2] * f}, PlanePattern::None, wallPat, false, {}, wsd, Surface::Default,
                             {255, 255, wallTex});
            bands.push_back({a, e, d, 1});
        }
    }
    if (z.ceilTrim > 0.0) {  // V12: Winkelprofil der Rasterdecke an der Wand
        u16 ct = bank.get(z.ceilTrimColor, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Trim);
        bands.push_back({-z.ceilTrim, 0.0, ct, 2});
    }
    u16 lampId = bank.get(lamp, PlanePattern::None, WallPattern::Plain, true);
    room.lampmat = lampId;
    room.lampColor = lamp;
    room.wallColor = wall;
    double sd = wallPat == WallPattern::Wallpaper ? 0.86 : 0.72;
    room.wmat = bank.get(wall, PlanePattern::None, wallPat, false, bands, sd, Surface::Default, {255, 255, wallTex});
    std::vector<Band> lampBands = bands;
    lampBands.push_back({1.75, 2.05, lampId});
    room.walllampmat = bank.get(wall, PlanePattern::None, wallPat, false, lampBands, sd, Surface::Default, {255, 255, wallTex});
    room.fmat = bank.get(floor, floorPat, WallPattern::Plain, false, {}, floorPat == PlanePattern::Checker ? 0.3 : 0.72,
                         Surface::Default, {floorTex, floorTex, floorTex});
    room.cmat = bank.get(ceil, ceilPat, WallPattern::Plain, false, {}, 0.72, Surface::Default, {ceilTex, ceilTex, ceilTex});
    Rgb stair{floor[0] * 0.95, floor[1] * 0.95, floor[2] * 0.95};
    // V12: Stufen tragen die Textur des Bodens (Holztreppen in Level 5.3 sind aus Holz, nicht aus glattem Grau)
    room.stairmat = bank.get({stair[0] * 0.8, stair[1] * 0.8, stair[2] * 0.8}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72,
                             Surface::Default, {floorTex, floorTex, floorTex});
    room.stairtop = bank.get(stair, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Default, {floorTex, floorTex, floorTex});
    room.mPart = bank.get({0.42, 0.45, 0.54}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Partition);
    room.mPartTop = bank.get({0.32, 0.34, 0.40}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Partition);
    room.mShelf = bank.get({0.26, 0.34, 0.56}, PlanePattern::None, WallPattern::Blocks, false, {}, 0.72, Surface::Shelf);
    room.mShelfTop = bank.get({0.30, 0.30, 0.34}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Metal);
    room.mCrate = bank.get({0.56, 0.40, 0.22}, PlanePattern::None, WallPattern::Panels, false, {}, 0.72, Surface::Crate);
    room.mCrateTop = bank.get({0.50, 0.36, 0.20}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Crate);
    room.mMachine = bank.get({0.34, 0.40, 0.34}, PlanePattern::None, WallPattern::Blocks, false, {}, 0.72, Surface::Machine);
    room.mMachineTop = bank.get({0.28, 0.30, 0.28}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Machine);
    room.mStage = bank.get({floor[0] * 0.8, floor[1] * 0.8, floor[2] * 0.8}, PlanePattern::None, WallPattern::Plain, false,
                           {}, 0.72, Surface::Stage);
    room.mStageTop = bank.get(floor, PlanePattern::BigTiles, WallPattern::Plain, false, {}, 0.72, Surface::Stage);
    room.mMono = bank.get({0.05, 0.05, 0.06}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Monolith);
    // V11: Gruben der festen Karten (Lochfeld in Level 0): Licht reicht nicht hinab - Waende und Grund fast schwarz.
    // Nur dort angelegt, damit die alten Welten dieselben Materialnummern behalten.
    if (room.mapped) room.mPit = bank.get({0.025, 0.023, 0.02}, PlanePattern::None, WallPattern::Concrete);
    // V3: Versorgungsraum - Theke (Holzdekor, helle Arbeitsplatte) und leuchtender Automat
    room.mCounter = bank.get({0.50, 0.37, 0.24}, PlanePattern::None, WallPattern::Panels, false, {}, 0.72, Surface::Counter);
    room.mCounterTop = bank.get({0.80, 0.79, 0.74}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::CounterTop);
    room.mVend = bank.get({0.78, 0.30, 0.24}, PlanePattern::None, WallPattern::Plain, true, {}, 0.72, Surface::Vending);
    room.mVendTop = bank.get({0.22, 0.20, 0.20}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Metal);
    room.mFixture = bank.get({0.74, 0.74, 0.72}, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Metal);

    // Licht
    std::string lstyle = !rt.light.empty() ? rt.light : z.lightStyle;
    if (!z.largeRoomLight.empty() && rt.category == Category::Large && lstyle == "panels") lstyle = z.largeRoomLight;
    room.lightStyle = lstyle;
    {  // V11: Bauform der Leuchten aus der Zone (verbraucht keine Zufallszahl)
        const std::string& fx = level_.zones[(size_t)room.zone].fixture;
        room.fixture = fx == "tube" ? 1 : (fx == "hanging" ? 2 : 0);
    }
    double amb = zones_.blend(zw, &ZoneDef::ambient) * rt.ambientMul * rng.uniform(0.8, 1.15);
    if (rng.random() < 0.08 && !rt.noDarkVariant) amb *= 0.35;  // vereinzelt deutlich dunklere Raeume
    const LightStyleDef& ls = level_.lightStyleFor(room.zone, lstyle);
    if (ls.ambient) amb = *ls.ambient;
    room.ambient = amb;
    room.lampInt = ls.intensity;
    room.lightFallback = ls.centerFallback;
    room.lampHum = ls.hum;
    room.avgLight = std::min(1.4, amb + ls.extra);

    // Nebel
    room.fog = zones_.mix(zw, &ZoneDef::fog);
    room.fogDensity = zones_.blend(zw, &ZoneDef::fogDensity) * rt.fogMul;
    if (so.fog) room.fog = *so.fog;
    else if (ls.darkFog) room.fog = level_.world() ? level_.regions[(size_t)level_.regionOfZone(room.zone)].darkFog : level_.darkFog;

    // Flackern: fast alle Lampen ruhig, selten ein leises Pulsieren,
    // sehr selten eine alternde Roehre mit weichen Helligkeitsschwankungen.
    double r = rng.random();
    room.flicker = r < 0.012 ? 2 : (r < 0.06 ? 1 : 0);
}

// --- V11: feste Karte ------------------------------------------------------------------------------
// Die Karte liegt vollstaendig in Sektor (0, 0); alle anderen Sektoren sind massiv. Raeume, Hoehen,
// Tueren und Leuchten kommen aus der Karte, Materialien und Einbauten aus Zone und Raumtyp (mit dem
// festen Seed des Raums) - die Architektur ist in jedem Lauf gleich.
void Generator::applyLightStyle(Room& room, const std::string& name) const {
    const LightStyleDef& ls = level_.lightStyleFor(room.zone, name);
    room.lightStyle = name;
    room.lampInt = ls.intensity;
    room.lightFallback = ls.centerFallback;
    room.lampHum = ls.hum;
    if (ls.ambient) room.ambient = *ls.ambient;
    room.avgLight = std::min(1.4, room.ambient + ls.extra);
    if (ls.darkFog) room.fog = level_.darkFog;
}

// Material der Bank fuer ein Teil der Ausstattung
static Surface surfaceOf(props::Look l) {
    using props::Look;
    switch (l) {
        case Look::Paint: return Surface::PropPaint;
        case Look::CarPaint: return Surface::PropCarPaint;
        case Look::Metal: return Surface::PropMetal;
        case Look::Galvanized: return Surface::PropGalvanized;
        case Look::Plastic: return Surface::PropPlastic;
        case Look::Rubber: return Surface::PropRubber;
        case Look::Glass: return Surface::PropGlass;
        case Look::Water: return Surface::PropWater;
        case Look::RoadPaint: return Surface::PropRoadPaint;
        case Look::Sign: return Surface::PropSign;
        case Look::Stencil: return Surface::PropStencil;
        case Look::Wood: return Surface::PropWood;
        case Look::Fabric: return Surface::PropFabric;
        case Look::Concrete: return Surface::PropConcrete;
        case Look::Cardboard: return Surface::PropCardboard;
        case Look::Glow: return Surface::PropGlow;
        case Look::Ceramic: return Surface::PropCeramic;
    }
    return Surface::PropPaint;
}

u16 propMaterial(MaterialBank& bank, const props::MatDesc& d) {
    const int tl = d.tex.empty() ? -1 : importedLayer(d.tex);
    const u8 l = tl < 0 ? (u8)255 : (u8)tl;
    return bank.get({d.r, d.g, d.b}, PlanePattern::None, WallPattern::Plain, d.glow > 0.0f, {}, 0.72, surfaceOf(d.look), {l, l, l},
                    {d.rough, d.metal});
}

void Generator::addProp(Sector& sec, const props::Model* m, float x, float y, float z, float yaw, float sx, float sy, float sz) const {
    if (!m) return;
    Sector::Prop p;
    p.model = m;
    p.x = x, p.y = y, p.z = z, p.yaw = yaw, p.sx = sx, p.sy = sy, p.sz = sz;
    for (const props::Part& part : m->parts) {
        const props::MatDesc& d = part.mat;
        // V12: Teile aus Blender koennen eine Textur aus data/textures waehlen (Ebene je Richtung gleich)
        const int tl = d.tex.empty() ? -1 : importedLayer(d.tex);
        const u8 l = tl < 0 ? (u8)255 : (u8)tl;
        p.mats.push_back(bank_.get({d.r, d.g, d.b}, PlanePattern::None, WallPattern::Plain, d.glow > 0.0f, {}, 0.72, surfaceOf(d.look),
                                   {l, l, l}, {d.rough, d.metal}));
        p.glow.push_back(d.glow);
    }
    if (const Room* r = sec.roomAt((i64)std::floor(x), (i64)std::floor(y))) {
        p.flickMode = (u8)r->flicker;
        p.flickSeed = (u8)r->lightCircuit();
    }
    sec.props.push_back(std::move(p));
}

std::unique_ptr<Sector> Generator::mapSector(i64 sx, i64 sy) const {
    const i64 S = level_.sectorSize;
    auto sec = std::make_unique<Sector>(sx, sy, (int)S);
    const MapDef& M = *level_.map;
    if (sx != 0 || sy != 0) {
        sec->buildIndex();
        return sec;
    }
    auto zoneIndex = [&](const std::string& key) {
        for (size_t z = 0; z < level_.zones.size(); ++z)
            if (level_.zones[z].key == key) return (int)z;
        return 0;
    };
    const u64 mapSalt = hash64(level_.id.c_str(), "map");
    for (size_t i = 0; i < M.rooms.size(); ++i) {
        const MapRoom& mr = M.rooms[i];
        int ti = level_.findType(mr.type);
        if (ti < 0) ti = level_.fallbackType >= 0 ? level_.fallbackType : 0;
        const RoomTypeDef& rt = level_.type(ti);
        const int zone = zoneIndex(mr.zone);
        ZoneWeights zw{};
        zw[(size_t)zone] = 1.0;
        auto room = std::make_unique<Room>(sx, sy, (int)i, mr.x0, mr.y0, mr.x1, mr.y1, &rt, zw, zone, mr.seed ^ mapSalt,
                                           rt.category == Category::Corridor);
        room->floor = mr.floor;
        room->fixedFloor = true;
        room->mapped = true;
        room->height = mr.height;
        room->circuit = mr.circuit;
        room->pillarSp = mr.pillarSp, room->pillarSpY = mr.pillarSpY, room->pillarTh = mr.pillarTh;
        room->pillarOx = mr.pillarOx, room->pillarOy = mr.pillarOy, room->gridBeamDrop = mr.beamDrop;
        style(*room);
        if (!mr.light.empty()) applyLightStyle(*room, mr.light);
        if (mr.ambient >= 0.0) {
            room->ambient = mr.ambient;
            room->avgLight = std::min(1.4, mr.ambient + level_.lightStyleFor(zone, room->lightStyle).extra);
        }
        if (mr.flicker >= 0) room->flicker = mr.flicker;
        for (const MapRoom::Block& b : mr.blocks) room->blocks.push_back({b.x0, b.y0, b.x1, b.y1, b.h, b.hidden});
        room->water = mr.water;  // V11
        if (!mr.lights.empty()) {
            room->lightsAuthored = true;
            for (const MapLight& l : mr.lights) {
                Fixture f{};
                f.x = l.x, f.y = l.y, f.intensity = l.intensity, f.radius = l.radius, f.invR2 = 1.0 / (l.radius * l.radius);
                f.kind = l.kind == 1 ? Fixture::Wall : l.kind == 3 ? Fixture::Glow : Fixture::Ceiling;
                f.dirX = l.dirX, f.dirY = l.dirY;
                if (l.z > -900.0) f.z = l.z;
                f.panel = l.panel;
                if (l.r >= 0.0) f.cr = (float)l.r, f.cg = (float)l.g, f.cb = (float)l.b;
                room->authoredLights.push_back(f);
            }
        }
        sec->rooms.push_back(std::move(room));
    }
    for (const MapDoor& md : M.doors) {
        Room* ra = sec->rooms[(size_t)md.a].get();
        Room* rb = sec->rooms[(size_t)md.b].get();
        const DoorKind kind = md.kind == MapDoorKind::Opening ? DoorKind::Opening
                              : md.kind == MapDoorKind::Gate  ? DoorKind::Gate
                              : md.kind == MapDoorKind::Wide  ? DoorKind::Wide
                                                              : DoorKind::Door;
        if (md.axis == 0) addDoor(*sec, 0, md.x0, md.x1 - md.x0, ra, rb, md.y0, md.y1 - md.y0, kind);
        else addDoor(*sec, 1, md.y0, md.y1 - md.y0, ra, rb, md.x0, md.x1 - md.x0, kind);
        Door& d = *sec->doors.back();
        d.sill = std::max(ra->floor, rb->floor);
        const double dh = std::min(level_.zones[(size_t)ra->zone].doorHeight, level_.zones[(size_t)rb->zone].doorHeight);
        const double cmin = std::min(ra->ceil(), rb->ceil());
        double top;
        if (md.top > 0.0) top = d.sill + md.top;
        else
            switch (kind) {
                case DoorKind::Wide: top = d.sill + dh + 0.3; break;
                case DoorKind::Gate: top = d.sill + std::max(dh, std::min(4.5, (cmin - d.sill) * 0.6)); break;
                case DoorKind::Opening: top = cmin; break;
                default: top = d.sill + dh; break;
            }
        top = std::min(top, cmin - 0.05);
        if (kind == DoorKind::Opening) top = cmin;
        d.top = std::max(top, d.sill + 1.9);
        doorStyle(d);
    }
    sec->buildIndex();
    // V11: Wasseroberflaechen (je Raum mit Wasser eine Flaeche auf dem Spiegel, von beiden Seiten sichtbar)
    // V13.3: klares Wasser - die Flaeche traegt Muster und Textur des Bodens darunter, getoent mit der Wasserfarbe
    // (shaders/gbuffer.hlsli: Wellen, Brechung, Spiegelung); so sieht man durch flaches Wasser die Fliesen
    auto clearWater = [&](const MapRoom& mr) {
        Sector::Prop& p = sec->props.back();
        const Room* r = sec->roomAt((mr.x0 + mr.x1) / 2, (mr.y0 + mr.y1) / 2);
        if (!r || p.mats.empty() || !p.model || p.model->parts.empty()) return;
        const Material& fm = bank_[r->fmat];
        const props::MatDesc& d = p.model->parts[0].mat;
        p.mats[0] = bank_.get({d.r, d.g, d.b}, fm.plane, WallPattern::Plain, false, {}, 0.72, Surface::PropWater,
                              {fm.layer[0], 255, 255});
    };
    for (const MapRoom& mr : M.rooms)
        if (mr.water > -900.0) {
            addProp(*sec, props::model("water_surface", mr.waterLook), (float)(mr.x0 + mr.x1) * 0.5f, (float)(mr.y0 + mr.y1) * 0.5f, (float)mr.water, 0.0f,
                    (float)(mr.x1 - mr.x0), (float)(mr.y1 - mr.y0), 1.0f);
            clearWater(mr);
        }
    // V12: auch in den Durchgaengen zwischen gefluteten Raeumen (sonst bleibt in jeder Oeffnung ein trockener Streifen)
    for (const MapDoor& md : M.doors) {
        const MapRoom& ra = M.rooms[(size_t)md.a];
        const MapRoom& rb = M.rooms[(size_t)md.b];
        if (ra.water < -900.0 || rb.water < -900.0) continue;
        const MapRoom& hi = ra.floor >= rb.floor ? ra : rb;
        addProp(*sec, props::model("water_surface", hi.waterLook), (float)(md.x0 + md.x1) * 0.5f, (float)(md.y0 + md.y1) * 0.5f,
                (float)std::max(ra.water, rb.water), 0.0f, (float)(md.x1 - md.x0), (float)(md.y1 - md.y0), 1.0f);
        clearWater(hi);
    }
    // Ausstattung: feste Objekte der Karte
    for (const MapObject& o : M.objects) {
        addProp(*sec, props::model(o.model, o.variant), (float)o.x, (float)o.y, (float)o.z, (float)o.yaw, (float)o.sx, (float)o.sy,
                (float)o.sz);
        if (o.tag == "pipes" || o.tag == "machine" || o.tag == "glow" || o.tag == "gate" || o.tag == "steam" || o.tag == "water" || o.tag == "fence" || o.tag == "window" || o.tag == "music" || o.tag == "waves")
            sec->spots.push_back({o.tag, (float)o.x, (float)o.y, (float)o.z, (float)o.yaw, o.model});
        // Notbeleuchtung (gruene Schilder) hat eine eigene Batterie: kein Stromkreis, brennt im Stromausfall
        if (o.tag == "emergency" && props::model(o.model, o.variant) && !sec->props.empty()) sec->props.back().flickSeed = 0, sec->props.back().flickMode = 0;
    }
    // Belegung dieses Laufs: Dekoration, Kisten, Gegenstaende
    if (plan_)
        for (size_t i = 0; i < M.spawns.size(); ++i) {
            if (!plan_->active(i)) continue;
            const MapSpawn& s = M.spawns[i];
            const int v = plan_->variant(i);
            if (s.kind == "deco") {
                const props::Model* m = props::model(s.set, v);
                if (m) addProp(*sec, m, (float)s.x, (float)s.y, (float)s.z, (float)s.yaw);
                if (m && (s.set == "car" || s.set == "puddle")) sec->spots.push_back({s.set, (float)s.x, (float)s.y, (float)s.z});
                // Wagen: unsichtbarer Block fuer Kollision, Schatten und Wege (Grundflaeche gedreht)
                if (m && s.set == "car" && s.room >= 0) {
                    const double c = std::fabs(std::cos(s.yaw)), sn = std::fabs(std::sin(s.yaw));
                    const double hx = 2.1 * c + 0.85 * sn, hy = 2.1 * sn + 0.85 * c;
                    sec->rooms[(size_t)s.room]->blocks.push_back({(i64)std::floor(s.x - hx + 0.5), (i64)std::floor(s.y - hy + 0.5),
                                                                  (i64)std::ceil(s.x + hx - 0.5), (i64)std::ceil(s.y + hy - 0.5), 1.42, true});
                }
            } else if (s.kind == "crate") {
                const bool pal = (v & 3) == 0;
                addProp(*sec, props::model(pal ? "pallet" : "crate", v >> 2), (float)s.x, (float)s.y, (float)s.z,
                        (float)(v & 255) * 0.0245f);
            } else if (s.kind == "fusebox" && s.room >= 0) {
                Sector::NoteSpot f{std::format("map:f{}", i), (float)s.x, (float)s.y, (float)sec->rooms[(size_t)s.room]->floor + 1.2f,
                                   (float)s.yaw, s.set.empty() ? std::string("fuse_station") : s.set};
                sec->fuseboxes.push_back(std::move(f));
            } else if (s.kind == "note" && s.room >= 0) {
                sec->notes.push_back({std::format("map:n{}", i), (float)s.x, (float)s.y, (float)sec->rooms[(size_t)s.room]->floor, (float)s.yaw});
            } else if (s.kind == "entity" && s.room >= 0) {
                // V11: Lager; das Buendel haust in einem Deckenschacht (das Gitter haengt offen herab)
                const Room& room = *sec->rooms[(size_t)s.room];
                const float ceil = (float)room.ceil();
                sec->entities.push_back({std::format("map:e{}", i), (float)s.x, (float)s.y, ceil, (float)s.yaw, s.role});
                if (s.role == "clump")
                    if (const props::Model* m = props::model("ceiling_vent", v & 1)) addProp(*sec, m, (float)s.x, (float)s.y, ceil, (float)(v & 3) * 1.5708f);
            } else if (s.kind == "item" && s.room >= 0) {
                Room& room = *sec->rooms[(size_t)s.room];
                ItemSpawn it;
                it.id = std::format("m{}", i);
                it.kind = plan_->item(i);
                it.x = s.x, it.y = s.y;
                // V11: liegt der Spawnpunkt hoeher als der Boden (Regalboden, Couchtisch), gilt seine Hoehe
                it.z = s.z > room.floor + 0.05 ? s.z : room.floor + (s.hasTag("table") ? 0.78 : 0.0);
                it.dirX = 1, it.dirY = 0;
                room.items.push_back(std::move(it));
            }
        }
    for (auto& room : sec->rooms)
        if (!room->items.empty()) sec->itemRooms.push_back(room.get());
    return sec;
}

void Generator::doorStyle(Door& door) const {
    Room* room = door.roomA ? door.roomA : door.roomB;
    Rgb trim = zones_.mix(room->zoneW, &ZoneDef::trim);
    u16 frame = bank_.get(trim, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Trim);
    u16 sill = bank_.get({trim[0] * 0.7, trim[1] * 0.7, trim[2] * 0.7}, PlanePattern::None, WallPattern::Plain, false, {},
                         0.72, Surface::Trim);
    if (door.kind == DoorKind::Opening) {
        frame = room->wmat;
        sill = room->fmat;
    }
    door.matSill = sill;
    door.matFrame = frame;
}

}  // namespace lim::world
