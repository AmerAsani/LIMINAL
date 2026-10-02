#include "world/room.hpp"

#include <cmath>
#include <format>
#include <limits>
#include <set>

#include "core/math.hpp"
#include "core/rng.hpp"

namespace lim::world {

static constexpr i64 kBucket = 8;  // Rastergroesse fuer die Suche nach Lichtquellen

// ======================================================================================
// Door
// ======================================================================================

double Door::otherAvgLight(const Room* r) const {
    Room* o = (r == roomA) ? roomB : roomA;
    if (o) return o->avgLight;
    if (portal && remoteResolved) return remoteAvgLight;
    return 0.4;
}

void Door::inner(const Room* r, std::vector<TilePos>& tiles, int& dx, int& dy) const {
    tiles.clear();
    // Bei Portalen ist die entfernte Seite nullptr; "r ist roomA" gilt dann nur,
    // wenn r wirklich roomA ist - genau wie "room is self.room_a" in V4.
    bool isA = (r == roomA);
    if (axis == 0) {
        if (isA) {
            for (i64 y = y0; y < y1; ++y) tiles.push_back({x0 - 1, y});
            dx = -1, dy = 0;
        } else {
            for (i64 y = y0; y < y1; ++y) tiles.push_back({x1, y});
            dx = 1, dy = 0;
        }
    } else {
        if (isA) {
            for (i64 x = x0; x < x1; ++x) tiles.push_back({x, y0 - 1});
            dx = 0, dy = -1;
        } else {
            for (i64 x = x0; x < x1; ++x) tiles.push_back({x, y1});
            dx = 0, dy = 1;
        }
    }
}

Tile Door::cell() const {
    auto sideLight = [&](Room* r, char side) {
        if (r) return r->avgLight;
        if (portal && remoteSide == side && remoteResolved) return remoteAvgLight;
        return 0.4;
    };
    double la = sideLight(roomA, 'a');
    double lb = sideLight(roomB, 'b');
    Tile t;
    t.floor = (float)sill;
    t.ceil = (float)top;
    t.fmat = matSill;
    t.cmat = matFrame;
    t.wmat = matFrame;
    t.roomWall = matFrame;
    t.light = (float)((double)(i64)((la + lb) * 0.5 * 32.0 + 0.5) / 32.0);
    t.flags = TF_DOOR;
    t.door = this;
    return t;
}

const char* Door::describe() const {
    switch (kind) {
        case DoorKind::Door: return portal ? "Tür (Sektorgrenze)" : "Tür";
        case DoorKind::Wide: return portal ? "Doppeltür (Sektorgrenze)" : "Doppeltür";
        case DoorKind::Gate: return portal ? "Tor (Sektorgrenze)" : "Tor";
        case DoorKind::Opening: return portal ? "Offener Übergang (Sektorgrenze)" : "Offener Übergang";
    }
    return "Durchgang";
}

// ======================================================================================
// Room
// ======================================================================================

Room::Room(i64 sx_, i64 sy_, int index_, i64 x0_, i64 y0_, i64 x1_, i64 y1_, const RoomTypeDef* type_,
           const ZoneWeights& zw, int zone_, u64 seed_, bool corridor)
    : sx(sx_), sy(sy_), index(index_), x0(x0_), y0(y0_), x1(x1_), y1(y1_), type(type_), zoneW(zw), zone(zone_),
      seed(seed_), isCorridor(corridor), stairStep(type_->stairStep) {}

std::optional<TilePos> Room::machineTile() const {
    if (!machine_) return std::nullopt;
    return TilePos{x0 + machine_->first, y0 + machine_->second};
}

// --- Versorgungsraum (V3) --------------------------------------------------------------
void Room::planSupply(const LevelDef& level, double oddsNone, double oddsOne, const SupplyRules& rules) {
    items.clear();
    counter_.clear();
    machine_.reset();
    if (!type->has(F_COUNTER)) return;
    Rng rng(seed ^ 0xF00DULL);
    // Frei bleibt nur der Bereich direkt vor jeder Tuer.
    std::set<std::pair<i64, i64>> keep;
    std::vector<TilePos> tiles;
    for (Door* d : doors) {
        int dx, dy;
        d->inner(this, tiles, dx, dy);
        for (const auto& t : tiles)
            for (int k = 0; k < 2; ++k)
                for (int s = -1; s <= 1; ++s) {
                    if (d->axis == 0) keep.insert({t.x + dx * k - x0, t.y + s - y0});
                    else keep.insert({t.x + s - x0, t.y + dy * k - y0});
                }
    }
    const i64 W = w(), H = h();
    std::vector<std::pair<i64, i64>> walls[4];
    for (i64 lx = 0; lx < W; ++lx) walls[0].push_back({lx, 0});
    for (i64 lx = 0; lx < W; ++lx) walls[1].push_back({lx, H - 1});
    for (i64 ly = 0; ly < H; ++ly) walls[2].push_back({0, ly});
    for (i64 ly = 0; ly < H; ++ly) walls[3].push_back({W - 1, ly});
    // je Wand das laengste zusammenhaengende freie Stueck; Laengswaende bevorzugt
    bool haveBest = false;
    double bestScore = 0;
    std::vector<std::pair<i64, i64>> bestStrip;
    int bestWall = 0;
    for (int wi = 0; wi < 4; ++wi) {
        std::vector<std::pair<i64, i64>> run, longest;
        for (const auto& t : walls[wi]) {
            if (keep.count(t)) run.clear();
            else {
                run.push_back(t);
                if (run.size() > longest.size()) longest = run;
            }
        }
        bool longWall = (wi < 2) == (W >= H);
        double score = (double)longest.size() + (longWall ? 0.5 : 0.0) + rng.random() * 0.3;
        if (!haveBest || score > bestScore) {
            haveBest = true;
            bestScore = score;
            bestStrip = longest;
            bestWall = wi;
        }
    }
    if (bestStrip.empty()) {
        // kein freier Wandplatz (winziger Raum voller Tueren) -> normales Zimmer
        type = &level.type(level.supplyFallbackType);
        return;
    }
    // Richtung von der Wand in den Raum
    static const int kDir[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    machineDirX = kDir[bestWall][0];
    machineDirY = kDir[bestWall][1];
    auto end = rng.random() < 0.5 ? bestStrip.front() : bestStrip.back();
    machine_ = end;
    std::vector<std::pair<i64, i64>> strip;
    for (const auto& t : bestStrip)
        if (t != end) strip.push_back(t);
    counter_ = strip;
    bool alongX = bestWall < 2;
    double r = rng.random();
    int n = r < oddsNone ? 0 : (r < oddsOne ? 1 : 2);
    auto spots = strip;
    const int count = std::min<int>(n, (int)spots.size());  // wie range(min(n, len(spots))) - einmal ausgewertet
    for (int k = 0; k < count; ++k) {
        i64 i = rng.randint(0, (i64)spots.size() - 1);
        auto [lx, ly] = spots[(size_t)i];
        spots.erase(spots.begin() + i);
        double off = rng.uniform(-0.22, 0.22);
        double x, y;
        if (alongX) {
            x = x0 + lx + 0.5 + off;
            y = y0 + ly + 0.5;
        } else {
            x = x0 + lx + 0.5;
            y = y0 + ly + 0.5 + off;
        }
        // Item-Art: kumulative Auswahl aus der Level-Tabelle
        std::string kind = level.supplyItems.empty() ? std::string("energy") : level.supplyItems.back().item;
        double rr = rng.random(), acc = 0.0;
        for (const auto& si : level.supplyItems) {
            acc += si.weight;
            if (rr < acc) {
                kind = si.item;
                break;
            }
        }
        // V6-Seltenheit: eigener Hash je Platz (keine Zufallszahl aus rng -> V4-Reihenfolge bleibt)
        bool skip = false;
        for (const SupplyRule& rule : rules) {
            if (rule.item != kind || rule.keep >= 1.0) continue;
            if (hfloat(seed, 0xE6E7, (i64)k) < rule.keep) break;
            if (!rule.replace.empty() && hfloat(seed, 0xE6E8, (i64)k) < rule.replaceChance) kind = rule.replace;
            else skip = true;
            break;
        }
        if (skip) continue;
        ItemSpawn it;
        it.id = std::format("{},{},{},{}", sx, sy, index, k);
        it.kind = kind;
        it.x = x;
        it.y = y;
        it.z = floor + kCounterHeight;
        it.dirX = machineDirX;
        it.dirY = machineDirY;
        items.push_back(std::move(it));
    }
}

Room::FeatResult Room::counterAt(i64 lx, i64 ly) const {
    FeatResult r;
    if (machine_ && machine_->first == lx && machine_->second == ly) {
        r.kind = FeatResult::Block;
        r.h = 1.9;
        r.side = mVend;
        r.top = mVendTop;
        return r;
    }
    for (const auto& c : counter_)
        if (c.first == lx && c.second == ly) {
            r.kind = FeatResult::Block;
            r.h = kCounterHeight;
            r.side = mCounter;
            r.top = mCounterTop;
            return r;
        }
    return r;
}

// --- Vorbereitung ----------------------------------------------------------------------
void Room::prepare() {
    if (prepared_) return;
    const size_t area = (size_t)(w() * h());
    stairH_.assign(area, std::numeric_limits<double>::quiet_NaN());
    clear_.assign(area, 0);
    lamps_.assign(area, 0);
    wallLamps_.assign(area, 0);
    Rng rng(seed ^ 0x5EEDULL);
    for (Door* d : doors) {
        planStairs(*d);
        planClearance(*d);
    }
    planFeatures(rng);
    planLights(rng);
    if (hasOccluders_) {
        occ_.assign(area, 0);
        for (i64 y = y0; y < y1; ++y)
            for (i64 x = x0; x < x1; ++x) {
                size_t i = (size_t)((y - y0) * w() + (x - x0));
                if (clear_[i] || !std::isnan(stairH_[i])) continue;
                FeatResult f = featureAt(x - x0, y - y0);
                occ_[i] = (f.kind == FeatResult::Solid) || (f.kind == FeatResult::Block && f.h >= 1.8);
            }
    }
    prepared_ = true;
    // Treppen einheitlich beleuchten -> sie lesen sich als ein Bauteil
    if (!stairOrder_.empty()) {
        size_t n = std::min<size_t>(24, stairOrder_.size());
        double s = 0.0;
        for (size_t i = 0; i < n; ++i) s += lightAt(stairOrder_[i].x, stairOrder_[i].y);
        stairLight_ = s / (double)n;
    }
}

void Room::planStairs(const Door& d) {
    double diff = d.sill - floor;
    if (diff <= 1e-6) return;
    i64 n = std::max<i64>(1, (i64)std::ceil(diff / stairStep - 1e-6) - 1);
    double sub = diff / (double)(n + 1);
    std::vector<TilePos> tiles;
    int dx, dy;
    d.inner(this, tiles, dx, dy);
    std::vector<TilePos> ext = tiles;
    if (d.axis == 0) {
        ext.push_back({tiles.front().x, tiles.front().y - 1});
        ext.push_back({tiles.back().x, tiles.back().y + 1});
    } else {
        ext.push_back({tiles.front().x - 1, tiles.front().y});
        ext.push_back({tiles.back().x + 1, tiles.back().y});
    }
    for (i64 k = 0; k < n; ++k) {
        double hgt = d.sill - (double)(k + 1) * sub;
        for (const auto& t : ext) {
            i64 x = t.x + dx * k, y = t.y + dy * k;
            if (!contains(x, y)) continue;
            size_t i = (size_t)((y - y0) * w() + (x - x0));
            double old = stairH_[i];
            if (std::isnan(old)) {
                stairH_[i] = hgt;
                stairOrder_.push_back({x, y});
            } else if (hgt > old) {
                stairH_[i] = hgt;
            }
        }
    }
}

void Room::planClearance(const Door& d) {
    std::vector<TilePos> tiles;
    int dx, dy;
    d.inner(this, tiles, dx, dy);
    i64 depth = 3 + std::max<i64>(0, (i64)((d.sill - floor) / stairStep) + 1);
    for (const auto& t : tiles)
        for (i64 k = 0; k < depth; ++k)
            for (int s = -1; s <= 1; ++s) {
                if (d.axis == 0) setBit(clear_, t.x + dx * k, t.y + s);
                else setBit(clear_, t.x + s, t.y + dy * k);
            }
}

// --- Einbauten -------------------------------------------------------------------------
void Room::planFeatures(Rng& rng) {
    feats_.clear();
    ceilFns_.clear();
    const i64 W = w(), H = h();
    if (type->has(F_COUNTER)) feats_.push_back({FeatKind::Counter});
    if (type->has(F_PILLARS)) {
        i64 sp = rng.randint(3, 5);
        gridPillars(sp, 1, 2);
    }
    if (type->has(F_PILLARS_SPARSE)) {
        i64 sp = rng.randint(6, 8);
        gridPillars(sp, 1, 3);
    }
    if (type->has(F_THICK_PILLARS)) {
        i64 sp = rng.randint(6, 9);
        gridPillars(sp, 2, 3);
    }
    if (type->has(F_GIANT_PILLARS)) {
        i64 sp = rng.randint(11, 15);
        gridPillars(sp, 3, 4);
    }
    if (type->has(F_COLONNADE) && std::min(W, H) >= 4) {
        feats_.push_back({FeatKind::Colonnade});
        colStep_ = rng.randint(2, 4);
    }
    if (type->has(F_CUBICLES)) feats_.push_back({FeatKind::Cubicles});
    if (type->has(F_SHELVES)) {
        shelfH_ = rng.uniform(2.2, std::min(3.4, height - 0.6));
        shelfSeg_ = rng.randint(6, 10);
        feats_.push_back({FeatKind::Shelves});
    }
    if (type->has(F_CRATES)) feats_.push_back({FeatKind::Crates});
    if (type->has(F_MACHINES) && std::min(W, H) >= 5) feats_.push_back({FeatKind::Machines});
    if (type->has(F_PODIUM)) feats_.push_back({FeatKind::Podium});
    if (type->has(F_MONOLITH)) {
        monoH_ = std::min(height - 3.0, 16.0);
        feats_.push_back({FeatKind::Monolith});
    }
    if (type->has(F_BEAMS) && height >= 3.4) {
        beamDrop_ = rng.uniform(0.4, 0.9);
        beamStep_ = rng.randint(3, 5);
        ceilFns_.push_back(CeilKind::Beams);
    }
    if (type->has(F_VAULT)) ceilFns_.push_back(CeilKind::Vault);
    if (type->has(F_PIPES) && height >= 2.3) ceilFns_.push_back(CeilKind::Pipes);
}

void Room::gridPillars(i64 sp, i64 th, i64 margin) {
    const i64 W = w(), H = h();
    if (W < 2 * margin + th || H < 2 * margin + th) return;
    Feat f{FeatKind::Pillars};
    f.sp = sp;
    f.th = th;
    f.margin = margin;
    f.ox = margin + floorDiv(floorMod(W - 2 * margin - th, sp), 2);
    f.oy = margin + floorDiv(floorMod(H - 2 * margin - th, sp), 2);
    feats_.push_back(f);
}

void Room::acrossAlong(i64 lx, i64 ly, i64& a, i64& b, i64& wa, i64& la) const {
    if (axis() == 0) {
        a = ly, b = lx, wa = h(), la = w();
    } else {
        a = lx, b = ly, wa = w(), la = h();
    }
}

Room::FeatResult Room::featureAt(i64 lx, i64 ly) const {
    const i64 W = w(), H = h();
    const u64 s = seed;
    for (const Feat& f : feats_) {
        FeatResult r;
        switch (f.kind) {
            case FeatKind::Counter: r = counterAt(lx, ly); break;
            case FeatKind::Pillars:
                if (f.margin <= lx && lx < W - f.margin && f.margin <= ly && ly < H - f.margin) {
                    if (floorMod(lx - f.ox, f.sp) < f.th && floorMod(ly - f.oy, f.sp) < f.th && lx >= f.ox && ly >= f.oy)
                        r.kind = FeatResult::Solid;
                }
                break;
            case FeatKind::Colonnade: {
                i64 a, b, wa, la;
                acrossAlong(lx, ly, a, b, wa, la);
                if ((a == 1 || a == wa - 2) && 1 <= b && b < la - 1 && floorMod(b, colStep_) == 1) r.kind = FeatResult::Solid;
                break;
            }
            case FeatKind::Cubicles:
                if (2 <= lx && lx < W - 3 && 2 <= ly && ly < H - 3) {
                    i64 i = floorMod(lx - 2, 5), j = floorMod(ly - 2, 5);
                    if ((j == 0 && i <= 3) || (i == 0 && j <= 2)) r = {FeatResult::Block, 1.35, mPart, mPartTop};
                }
                break;
            case FeatKind::Shelves: {
                i64 a, b, wa, la;
                acrossAlong(lx, ly, a, b, wa, la);
                if (2 <= a && a < wa - 2 && floorMod(a - 2, 3) == 0 && 2 <= b && b < la - 2) {
                    if (floorMod(b - 2, shelfSeg_) != shelfSeg_ - 1) r = {FeatResult::Block, shelfH_, mShelf, mShelfTop};
                }
                break;
            }
            case FeatKind::Crates: {
                if (!(2 <= lx && lx < W - 2 && 2 <= ly && ly < H - 2)) break;
                i64 ci = floorDiv(lx - 2, 6), ri = floorMod(lx - 2, 6);
                i64 cj = floorDiv(ly - 2, 6), rj = floorMod(ly - 2, 6);
                if (hfloat(s, 11, ci, cj) > 0.45) break;
                i64 cw = 1 + (i64)(hfloat(s, 12, ci, cj) * 3);
                i64 ch = 1 + (i64)(hfloat(s, 13, ci, cj) * 3);
                i64 ox = 1 + (i64)(hfloat(s, 14, ci, cj) * (double)(4 - cw));
                i64 oy = 1 + (i64)(hfloat(s, 15, ci, cj) * (double)(4 - ch));
                if (ox <= ri && ri < ox + cw && oy <= rj && rj < oy + ch && lx < W - 2 && ly < H - 2) {
                    double hgt = 0.9 + hfloat(s, 16, ci, cj) * 1.4;
                    r = {FeatResult::Block, hgt, mCrate, mCrateTop};
                }
                break;
            }
            case FeatKind::Machines: {
                if (!(1 <= lx && lx < W - 1 && 1 <= ly && ly < H - 1)) break;
                i64 ci = floorDiv(lx - 1, 4), ri = floorMod(lx - 1, 4);
                i64 cj = floorDiv(ly - 1, 4), rj = floorMod(ly - 1, 4);
                if (hfloat(s, 21, ci, cj) > 0.55) break;
                if (1 <= ri && ri <= 2 && 1 <= rj && rj <= 1 + (i64)(hfloat(s, 22, ci, cj) * 2) && lx < W - 1 && ly < H - 1) {
                    double hgt = 0.8 + hfloat(s, 23, ci, cj) * 1.0;
                    r = {FeatResult::Block, hgt, mMachine, mMachineTop};
                }
                break;
            }
            case FeatKind::Podium: {
                i64 px0 = floorDiv(W, 4), px1 = W - floorDiv(W, 4);
                i64 py0 = floorDiv(H, 4), py1 = H - floorDiv(H, 4);
                if (px0 <= lx && lx < px1 && py0 <= ly && ly < py1) {
                    bool edge = lx == px0 || lx == px1 - 1 || ly == py0 || ly == py1 - 1;
                    r = {FeatResult::Block, edge ? 0.45 : 0.9, mStage, mStageTop};
                }
                break;
            }
            case FeatKind::Monolith: {
                i64 mw = std::max<i64>(2, floorDiv(W, 6)), mh = std::max<i64>(2, floorDiv(H, 6));
                i64 cx = floorDiv(W, 2), cy = floorDiv(H, 2);
                if (cx - floorDiv(mw, 2) <= lx && lx < cx - floorDiv(mw, 2) + mw && cy - floorDiv(mh, 2) <= ly &&
                    ly < cy - floorDiv(mh, 2) + mh)
                    r = {FeatResult::Block, monoH_, mMono, mMono};
                break;
            }
        }
        if (r.kind != FeatResult::None) return r;
    }
    return {};
}

double Room::ceilingAt(i64 lx, i64 ly, double fl) const {
    double ce = floor + height;
    for (CeilKind c : ceilFns_) {
        i64 a, b, wa, la;
        acrossAlong(lx, ly, a, b, wa, la);
        switch (c) {
            case CeilKind::Beams:
                if (floorMod(b, beamStep_) == 0 && ce - beamDrop_ - fl >= 2.4) ce = ce - beamDrop_;
                break;
            case CeilKind::Vault: {
                if (wa < 2) break;
                double t = std::fabs(((double)a + 0.5) - (double)wa * 0.5) / ((double)wa * 0.5);
                double side = std::max(2.1, height * 0.7);
                double v = floor + side + (height - side) * (1.0 - t * t);
                if (v - fl >= 2.0) ce = v;
                break;
            }
            case CeilKind::Pipes:
                if ((a == 0 || a == wa - 1) && ce - 0.3 - fl >= 1.95) ce = ce - 0.3;
                break;
        }
    }
    return ce;
}

// --- Licht -----------------------------------------------------------------------------
void Room::planLights(Rng& rng) {
    const i64 W = w(), H = h();
    const std::string& style = lightStyle;
    fixtures_.clear();
    double inten = lampInt;
    double dead = 0.06;

    auto grid = [&](i64 sx_, i64 sy_, double radius, i64 margin) {
        i64 ox = margin + floorDiv(floorMod(W - 2 * margin - 1, sx_), 2);
        i64 oy = margin + floorDiv(floorMod(H - 2 * margin - 1, sy_), 2);
        for (i64 ly = oy; ly < H - margin; ly += sy_)
            for (i64 lx = ox; lx < W - margin; lx += sx_) {
                if (hfloat(seed, 31, lx, ly) < dead) continue;
                setBit(lamps_, x0 + lx, y0 + ly);
                fixtures_.push_back({(double)(x0 + lx) + 0.5, (double)(y0 + ly) + 0.5, inten, 1.0 / (radius * radius),
                                     radius, Fixture::Ceiling, 0, 0});
            }
    };

    if (style == "panels") {
        i64 a = rng.randint(3, 4);
        i64 b = rng.randint(3, 4);
        grid(a, b, 3.6, 1);
    } else if (style == "panels_wide") {
        i64 a = rng.randint(5, 7);
        i64 b = rng.randint(5, 7);
        grid(a, b, 5.5, 2);
    } else if (style == "panels_long") {
        if (axis() == 0) grid(3, std::max<i64>(1, H), 3.4, 0);
        else grid(std::max<i64>(1, W), 3, 3.4, 0);
    } else if (style == "pools") {
        i64 s = rng.randint(7, 9);
        grid(s, s, 5.5, 2);
    } else if (style == "emergency") {
        grid(7, 7, 3.8, 1);
    } else if (style == "void") {
        dead = 0.3;
        grid(14, 14, 7.0, 3);
    } else if (style == "bright") {
        grid(2, 2, 3.0, 0);
    } else if (style == "dark") {
        i64 cx = floorDiv(W, 2), cy = floorDiv(H, 2);
        setBit(lamps_, x0 + cx, y0 + cy);
        fixtures_.push_back({(double)(x0 + cx) + 0.5, (double)(y0 + cy) + 0.5, 1.1, 1.0 / (3.8 * 3.8), 3.8,
                             Fixture::Ceiling, 0, 0});
    } else if (style == "wall") {
        // Wandleuchten entlang der Laengswaende
        i64 step = rng.randint(5, 7);
        if (axis() == 0) {
            for (i64 lx = 2; lx < W - 1; lx += step) {
                i64 rows[2] = {0, H - 1};
                int nrows = H > 2 ? 2 : 1;
                for (int k = 0; k < nrows; ++k) {
                    i64 ly = rows[k];
                    if (hfloat(seed, 32, lx, ly) < 0.12) continue;
                    setBit(wallLamps_, x0 + lx, y0 + ly);
                    fixtures_.push_back({(double)(x0 + lx) + 0.5, (double)(y0 + ly) + 0.5, inten, 1.0 / (4.5 * 4.5), 4.5,
                                         Fixture::Wall, 0, ly == 0 ? 1 : -1});
                }
            }
        } else {
            for (i64 ly = 2; ly < H - 1; ly += step) {
                i64 cols[2] = {0, W - 1};
                int ncols = W > 2 ? 2 : 1;
                for (int k = 0; k < ncols; ++k) {
                    i64 lx = cols[k];
                    if (hfloat(seed, 32, lx, ly) < 0.12) continue;
                    setBit(wallLamps_, x0 + lx, y0 + ly);
                    fixtures_.push_back({(double)(x0 + lx) + 0.5, (double)(y0 + ly) + 0.5, inten, 1.0 / (4.5 * 4.5), 4.5,
                                         Fixture::Wall, lx == 0 ? 1 : -1, 0});
                }
            }
        }
    }
    // Optional (nur neue Level, verbraucht keine Zufallszahlen): kleine Raeume, in die das Raster
    // keine Lampe setzt, bekommen eine in der Mitte statt voellig dunkel zu bleiben.
    if (lightFallback && fixtures_.empty()) {
        i64 cx = floorDiv(W, 2), cy = floorDiv(H, 2);
        setBit(lamps_, x0 + cx, y0 + cy);
        fixtures_.push_back({(double)(x0 + cx) + 0.5, (double)(y0 + cy) + 0.5, inten, 1.0 / (4.5 * 4.5), 4.5,
                             Fixture::Ceiling, 0, 0});
    }
    // Automaten leuchten in den Raum (und damit durch die Tuer in den Flur)
    if (machine_) {
        fixtures_.push_back({(double)(x0 + machine_->first) + 0.5, (double)(y0 + machine_->second) + 0.5, 0.8,
                             1.0 / (4.2 * 4.2), 4.2, Fixture::Machine, machineDirX, machineDirY});
    }

    // Suchraster
    bx0_ = floorDiv(x0, kBucket) - 1;
    by0_ = floorDiv(y0, kBucket) - 1;
    bw_ = floorDiv(x1 - 1, kBucket) + 2 - bx0_;
    bh_ = floorDiv(y1 - 1, kBucket) + 2 - by0_;
    buckets_.assign((size_t)(bw_ * bh_), {});
    for (int i = 0; i < (int)fixtures_.size(); ++i) {
        const Fixture& f = fixtures_[(size_t)i];
        i64 bx = floorDiv(ifloor(f.x), kBucket) - bx0_;
        i64 by = floorDiv(ifloor(f.y), kBucket) - by0_;
        buckets_[(size_t)(by * bw_ + bx)].push_back(i);
    }
    hasOccluders_ = type->has(F_PILLARS) || type->has(F_PILLARS_SPARSE) || type->has(F_THICK_PILLARS) ||
                    type->has(F_GIANT_PILLARS) || type->has(F_COLONNADE) || type->has(F_SHELVES) ||
                    type->has(F_MONOLITH);
}

bool Room::occludes(i64 x, i64 y) const {
    if (!contains(x, y)) return false;
    return occ_[(size_t)((y - y0) * w() + (x - x0))] != 0;
}

bool Room::shadowed(double fx, double fy, double cx, double cy) const {
    double dx = cx - fx, dy = cy - fy;
    i64 n = (i64)(std::hypot(dx, dy) * 2.0);
    i64 tx0 = ifloor(cx), ty0 = ifloor(cy);
    for (i64 i = 1; i < n; ++i) {
        double k = (double)i / (double)n;
        i64 tx = ifloor(fx + dx * k);
        i64 ty = ifloor(fy + dy * k);
        if ((tx != tx0 || ty != ty0) && occludes(tx, ty)) return true;
    }
    return false;
}

double Room::lightAt(i64 x, i64 y) const {
    double cx = (double)x + 0.5, cy = (double)y + 0.5;
    double lum = ambient;
    i64 bx = floorDiv(x, kBucket), by = floorDiv(y, kBucket);
    for (i64 kx = bx - 1; kx <= bx + 1; ++kx)
        for (i64 ky = by - 1; ky <= by + 1; ++ky) {
            i64 ix = kx - bx0_, iy = ky - by0_;
            if (ix < 0 || iy < 0 || ix >= bw_ || iy >= bh_) continue;
            for (int fi : buckets_[(size_t)(iy * bw_ + ix)]) {
                const Fixture& f = fixtures_[(size_t)fi];
                double d2 = (cx - f.x) * (cx - f.x) + (cy - f.y) * (cy - f.y);
                double t = 1.0 - d2 * f.invR2;
                if (t > 0.0) {
                    if (hasOccluders_ && d2 > 1.0 && shadowed(f.x, f.y, cx, cy)) lum += f.intensity * t * t * 0.2;
                    else lum += f.intensity * t * t;
                }
            }
        }
    // Licht, das durch Tueren aus Nachbarraeumen faellt
    for (const Door* door : doors) {
        auto [dx, dy] = door->center();
        double d = std::hypot(cx - dx, cy - dy);
        if (d < 5.5) {
            double ol = door->otherAvgLight(this);
            double b = ol * 0.6 * (1.0 - d / 5.5);
            if (b > lum) lum = b > lum * 1.2 ? lum * 0.4 + b * 0.6 : b;
        }
    }
    return std::min(1.6, lum);
}

// --- Kacheln -------------------------------------------------------------------------------
Tile Room::tile(i64 x, i64 y) const {
    const i64 lx = x - x0, ly = y - y0;
    const size_t idx = (size_t)(ly * w() + lx);
    double fl = floor;
    u16 fm = fmat, cm = cmat, wm = wmat;
    u8 flags = 0;
    double st = stairH_[idx];
    bool isStair = !std::isnan(st);
    if (isStair) {
        fl = st;
        fm = stairtop;
        wm = stairmat;
        flags |= TF_STAIR;
    } else if (machine_ && (counterAt(lx, ly).kind != FeatResult::None)) {
        FeatResult r = counterAt(lx, ly);
        fl = floor + r.h;
        wm = r.side;
        fm = r.top;
        flags |= TF_FEATURE;
        if (machine_->first == lx && machine_->second == ly) flags |= TF_MACHINE;
    } else if (!clear_[idx]) {
        FeatResult r = featureAt(lx, ly);
        if (r.kind == FeatResult::Solid) {
            Tile t;
            t.room = this;
            return t;  // massiv (Saeule)
        }
        if (r.kind == FeatResult::Block) {
            fl = floor + r.h;
            wm = r.side;
            fm = r.top;
            flags |= TF_FEATURE;
        }
    }
    double ce = ceilingAt(lx, ly, fl);
    if (lamps_[idx]) {
        cm = lampmat;
        flags |= TF_LAMP;
    }
    bool wallLamp = wallLamps_[idx] != 0;
    if (wallLamp && !isStair) {
        wm = walllampmat;
        flags |= TF_WALLLAMP;
    }
    double lum = isStair ? stairLight_ : lightAt(x, y);
    Tile t;
    t.floor = (float)fl;
    t.ceil = (float)ce;
    t.fmat = fm;
    t.cmat = cm;
    t.wmat = wm;
    t.roomWall = (wallLamp && !isStair) ? walllampmat : wmat;
    t.light = (float)((double)(i64)(lum * 32.0 + 0.5) / 32.0);
    t.flags = flags;
    t.room = this;
    return t;
}

}  // namespace lim::world
