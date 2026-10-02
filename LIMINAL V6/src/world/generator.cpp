#include "world/generator.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

#include "core/math.hpp"
#include "core/rng.hpp"

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
            room->planSupply(level_, oddsNone_, oddsOne_, supplyRules_);
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
    const ZoneDef& dom = level_.zones[rng.weighted(zw.data(), (size_t)nz)];
    i64 t = rng.randint(dom.wallThickness.lo, dom.wallThickness.hi);

    const bool v6 = level_.layout >= kLayoutV6;
    const StructureDef& st = level_.structure;
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

void Generator::makeRooms(Ctx& c) const {
    Sector& sec = c.sector;
    const int nz = zones_.count();
    for (size_t i = 0; i < c.leaves.size(); ++i) {
        const Leaf& lf = c.leaves[i];
        double cx = (double)(lf.x0 + lf.x1) * 0.5;
        double cy = (double)(lf.y0 + lf.y1) * 0.5;
        ZoneWeights zw = zones_.weights(cx, cy);
        int zone = (int)c.rng.weighted(zw.data(), (size_t)nz);
        const auto& pt = level_.structure.passageTypes;
        const RoomTypeDef& rt = lf.passage ? level_.type(pt[(size_t)c.rng.randint(0, (i64)pt.size() - 1)])
                                           : chooseType(level_, c.rng, lf.x1 - lf.x0, lf.y1 - lf.y0, lf.corridor, lf.big,
                                                        lf.endless, zw, c.rarity);
        u64 seed = hash64(seed_, "room", sec.sx, sec.sy, (i64)i);
        sec.rooms.push_back(std::make_unique<Room>(sec.sx, sec.sy, (int)i, lf.x0, lf.y0, lf.x1, lf.y1, &rt, zw, zone,
                                                   seed, lf.corridor || rt.category == Category::Corridor));
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
        const StructureDef& st = level_.structure;
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

    auto col = [&](Rgb ZoneDef::*attr) {
        Rgb c = zones_.mix(zw, attr);
        return Rgb{c[0] * var, c[1] * var, c[2] * var};
    };
    Rgb wall = col(&ZoneDef::wall);
    Rgb floor = col(&ZoneDef::floor);
    Rgb ceil = col(&ZoneDef::ceil);
    Rgb trim = col(&ZoneDef::trim);
    Rgb lamp = zones_.mix(zw, &ZoneDef::lamp);
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

    std::vector<Band> bands;
    u16 trimId = bank.get(trim, PlanePattern::None, WallPattern::Plain, false, {}, 0.72, Surface::Trim);
    room.baseboard = z.baseboard;
    if (z.baseboard) bands.push_back({0.0, 0.12, trimId});
    u16 lampId = bank.get(lamp, PlanePattern::None, WallPattern::Plain, true);
    room.lampmat = lampId;
    room.lampColor = lamp;
    room.wallColor = wall;
    double sd = wallPat == WallPattern::Wallpaper ? 0.86 : 0.72;
    room.wmat = bank.get(wall, PlanePattern::None, wallPat, false, bands, sd);
    std::vector<Band> lampBands = bands;
    lampBands.push_back({1.75, 2.05, lampId});
    room.walllampmat = bank.get(wall, PlanePattern::None, wallPat, false, lampBands, sd);
    room.fmat = bank.get(floor, floorPat, WallPattern::Plain, false, {}, floorPat == PlanePattern::Checker ? 0.3 : 0.72);
    room.cmat = bank.get(ceil, ceilPat);
    Rgb stair{floor[0] * 0.95, floor[1] * 0.95, floor[2] * 0.95};
    room.stairmat = bank.get({stair[0] * 0.8, stair[1] * 0.8, stair[2] * 0.8}, PlanePattern::None, WallPattern::Plain);
    room.stairtop = bank.get(stair, PlanePattern::None);
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
    double amb = zones_.blend(zw, &ZoneDef::ambient) * rt.ambientMul * rng.uniform(0.8, 1.15);
    if (rng.random() < 0.08 && !rt.noDarkVariant) amb *= 0.35;  // vereinzelt deutlich dunklere Raeume
    const LightStyleDef& ls = level_.lightStyle(lstyle);
    if (ls.ambient) amb = *ls.ambient;
    room.ambient = amb;
    room.lampInt = ls.intensity;
    room.lightFallback = ls.centerFallback;
    room.avgLight = std::min(1.4, amb + ls.extra);

    // Nebel
    room.fog = zones_.mix(zw, &ZoneDef::fog);
    room.fogDensity = zones_.blend(zw, &ZoneDef::fogDensity) * rt.fogMul;
    if (so.fog) room.fog = *so.fog;
    else if (ls.darkFog) room.fog = level_.darkFog;

    // Flackern: fast alle Lampen ruhig, selten ein leises Pulsieren,
    // sehr selten eine alternde Roehre mit weichen Helligkeitsschwankungen.
    double r = rng.random();
    room.flicker = r < 0.012 ? 2 : (r < 0.06 ? 1 : 0);
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
