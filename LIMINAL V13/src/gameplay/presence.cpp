#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>

#include "core/log.hpp"
#include "world/level_def.hpp"
#include "world/room.hpp"
#include "world/sector.hpp"
#include "world/world.hpp"

namespace lim::game {

const char* Presence::roleName(Role r) {
    switch (r) {
        case Role::Smiler: return "Smiler";
        case Role::Hound: return "Hound";
        case Role::Moth: return "Deathmoth";
        case Role::Gestalt: return "Gestalt";
        case Role::Nest: return "Nest";
        case Role::Clump: return "Clump";
        case Role::Faceling: return "Faceling";
        case Role::SkinStealer: return "Skin-Stealer";
        case Role::Wretch: return "Wretch";
        case Role::Rat: return "Death Rat";
        case Role::Window: return "Fensterwesen";
        case Role::Duller: return "Duller";  // V13.3
        case Role::Arms: return "Arms";
        case Role::Wrangler: return "Wrangler";  // V13.3
        case Role::Growler: return "Growler";  // V13.3
        case Role::Nguithr: return "Nguithr'xurh";
        case Role::Ouroboros: return "Ouroboros-Arbeiter";  // V13.3
        case Role::Goblin: return "Plague Goblin";  // V13.3
    }
    return "?";
}

Presence::Presence() = default;
Presence::~Presence() = default;

void Presence::clear() {
    agents_.clear();
    hive_ = Hive{};
    pendingSmiler_ = -1.0;
    windowsPlaced_ = false;  // V13.2: neue Ebene, neue Fenster
    screamT_ = 0.0;
    broodPlaced_ = false, quake_ = trance_ = 0.0f;  // V13.3: Wrangler
    ouroPlaced_ = false, ouroSite_.reset();         // V13.3: Ouroboros-Baustelle der neuen Ebene
    goblinCtl_.reset(), nextGoblin_ = 50.0;          // V13.3: Plague Goblins - neue Ebene, neue Bande (auch nach einer Flucht)
}

void Presence::setGrace(double s) {
    nextThreat_ = std::max(nextThreat_, s);
    nextBlackout_ = std::max(nextBlackout_, s * 0.8);
    nextHive_ = std::max(nextHive_, s * 0.4);
    nextGestalt_ = std::max(nextGestalt_, s * 0.6);
    for (double& k : nextKin_) k = std::max(k, s * 0.5);  // V13.2
    nextDuller_ = std::max(nextDuller_, s * 0.6), nextArms_ = std::max(nextArms_, s * 0.3);  // V13.3
    nextWrangler_ = std::max(nextWrangler_, s * 0.8);     // V13.3
    nextGrowler_ = std::max(nextGrowler_, s * 0.5), nextNguithr_ = std::max(nextNguithr_, s * 0.3), corpseT_ = std::max(corpseT_, s * 0.5);  // V13.3
    nextWindowL2_ = std::max(nextWindowL2_, s * 0.5), nextMothL2_ = std::max(nextMothL2_, s * 0.3);  // V13.3 Teil 4
}

void Presence::remove(size_t i) { agents_.erase(agents_.begin() + (std::ptrdiff_t)i); }

bool Presence::has(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return true;
    return false;
}
int Presence::count(Role r) const {
    int n = 0;
    for (const auto& a : agents_) n += a->role == r && !a->showcase ? 1 : 0;
    return n;
}
vec3 Presence::posOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->pos;
    return {};
}
float Presence::distOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->sight.dist;
    return 1e9f;
}
bool Presence::seenRole(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->role == Role::Smiler ? a->contact : a->sight.level > 0.25f;
    return false;
}
bool Presence::seenRoleBy(Role r, int target) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase && a->target == target && (a->role == Role::Smiler ? a->contact : a->sight.level > 0.25f)) return true;
    return false;
}
float Presence::yawOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->yaw;
    return 0.0f;
}
int Presence::stateOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->state;
    return -1;
}
int Presence::stageOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->stage;
    return -1;
}
bool Presence::killerHead(vec3& out) const {
    for (const auto& a : agents_)
        if (a->state == 10 && !a->showcase) {
            out = a->cre.headPos();
            return true;
        }
    return false;
}
bool Presence::threatActive() const { return has(Role::Smiler) || has(Role::Hound) || wranglerActive(); }  // V13.3: Wrangler
bool Presence::hunting() const {
    for (const auto& a : agents_) {
        if (a->showcase) continue;
        if (a->role == Role::Clump && a->state >= 2 && a->state <= 4) return true;  // V11
        if (a->role == Role::Hound && (a->state == 2 || a->state == 3)) return true;
        if (a->role == Role::Smiler && a->state == 1 && a->sight.dist < 14.0f) return true;
        if ((a->role == Role::Duller && a->state >= 2 && a->state <= 4) || (a->role == Role::Arms && a->state >= 1 && a->state <= 4)) return true;  // V13.3
        if (a->role == Role::Wrangler && (a->state == 2 || a->state == 3)) return true;  // V13.3: es hat einen gespuert
        if (a->role == Role::Growler && a->state >= 2 && a->state <= 4) return true;  // V13.3: jagt, holt aus, greift an
    }
    return false;
}
float Presence::threatProximity() const {
    float p = 0.0f;
    for (const auto& a : agents_) {
        if (a->showcase) continue;
        const float d = a->sight.dist;
        if (a->role == Role::Smiler) p = std::max(p, std::clamp(1.0f - (d - 1.0f) / 18.0f, 0.0f, 1.0f) * (a->state >= 1 ? 1.0f : 0.5f));
        if (a->role == Role::Hound) p = std::max(p, std::clamp(1.0f - (d - 1.0f) / 22.0f, 0.0f, 1.0f) * (a->state >= 2 ? 1.0f : 0.35f));
        if (a->role == Role::Moth && a->state >= 2) p = std::max(p, std::clamp(1.0f - d / 10.0f, 0.0f, 1.0f) * 0.45f);
        if (a->role == Role::Clump) p = std::max(p, std::clamp(1.0f - (d - 1.0f) / 14.0f, 0.0f, 1.0f) * (a->state >= 2 ? 1.0f : 0.3f));
        if ((a->role == Role::Duller && a->state >= 2 && a->state <= 4) || (a->role == Role::Arms && a->state >= 1 && a->state <= 4))  // V13.3
            p = std::max(p, a->role == Role::Duller ? 0.85f : 0.75f);
        if (a->role == Role::Wrangler && a->mode != 3) p = std::max(p, std::clamp(1.0f - (d - 3.0f) / 26.0f, 0.0f, 1.0f) * std::min(1.0f, a->growl * 1.1f));  // V13.3
    }
    return std::max(p, static_ * 0.8f);  // V13.3: das Rauschen eines Growlers
}
int Presence::swarm(vec3& center) const {
    int n = 0;
    vec3 s{};
    for (const auto& a : agents_)
        if (a->role == Role::Moth && !a->showcase && a->fly > 0.5f) s += a->pos, ++n;
    if (n) center = s / (float)n;
    return n;
}

// --- Orte finden -------------------------------------------------------------------------------
// Versteckter Platz auf dem Weg lo..hi (Kosten) vom Spieler, ausserhalb seines Blicks
bool Presence::hiddenSpawn(const Context& c, int lo, int hi, bool behind, vec3& out, const std::function<bool(vec3)>& ok) {
    if (!fld().valid()) return false;
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(lo, hi, cand);
    if (ok)  // V13.2: nur Stellen, die dem Wesen erlaubt sind (Raumtypen)
        std::erase_if(cand, [&](const std::pair<i64, i64>& xy) {
            const world::Tile* t = c.world->tileIfLoaded(xy.first, xy.second);
            return !t || !ok(vec3{(float)xy.first + 0.5f, (float)xy.second + 0.5f, t->floor});
        });
    if (cand.empty()) return false;
    const vec3 fl = nrmF(flat(c.view.look), {1, 0, 0});
    for (int pass = 0; pass < 2; ++pass)
        for (int tries = 0; tries < 60; ++tries) {
            auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
            const world::Tile* t = c.world->tileIfLoaded(x, y);
            if (!t || t->clearance() < 2.1f || (t->flags & world::TF_STAIR)) continue;
            vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->floor};
            if (pass == 0 && behind && dot(nrmF(flat(p - c.view.eye), fl), fl) > 0.1f) continue;
            bool visible = false;
            for (float h : {1.75f, 1.0f, 0.4f}) {
                const vec3 tgt = p + vec3{0, 0, h};
                if (dot(nrmF(tgt - c.view.eye, fl), c.view.look) > 0.3f && clearSight(*c.world, c.view.eye, tgt)) visible = true;
            }
            if (visible) continue;
            out = p;
            return true;
        }
    return false;
}

// Ziel fuer einen Weg (Hound streift umher / zieht sich zurueck): Kachel auf dem Ring lo..hi um den
// Spieler; away: moeglichst vom Spieler weg (von a aus gesehen)
bool Presence::pickGoal(const Context& c, Agent& a, int lo, int hi, bool away) {
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(lo, hi, cand);
    const vec3 awayDir = nrmF(flat(a.pos - c.view.eye), {1, 0, 0});
    for (int tries = 0; tries < 50 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || !figureWalkable(t, 1.1f)) continue;
        const vec3 g{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        if (away && dot(nrmF(flat(g - c.view.eye), awayDir), awayDir) < 0.3f) continue;
        a.field.build(*c.world, x, y, 1.1f);
        if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) continue;
        a.goal = g;
        a.hasGoal = true;
        return true;
    }
    a.hasGoal = false;
    return false;
}

// --- Auftritte ---------------------------------------------------------------------------------
void Presence::spawnSmiler(const Context& c, std::vector<Event>& out, double dist, bool blackout) {
    if (!fld().valid()) return;
    std::vector<std::pair<i64, i64>> cand;
    // F1 (ENTITY_BEHAVIOR.md): nie naeher als 15 m Weg (Kosten etwa 8,5 je m)
    const int lo = dist > 0.0 ? (int)(std::max(dist, 15.0) * 8.5) : (blackout ? 130 : 130),
              hi = dist > 0.0 ? (int)(std::max(dist, 15.0) * 12.0) : (blackout ? 230 : 210);
    fld().ring(lo, hi, cand);
    const vec3 eye = c.view.eye;
    float bestScore = 1e9f;
    vec3 best;
    for (int tries = 0; tries < 220 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || t->clearance() < 2.1f || (t->flags & world::TF_STAIR) || !darkTile(c, t, (float)x + 0.5f, (float)y + 0.5f)) continue;
        // V11: im Stromausfall bevorzugt im Raum des Spielers - ist der zu klein fuer 15 m Weg (F1), in
        // einem anderen dunklen Bereich
        const float otherRoom = blackout && c.room && t->room != c.room ? 6.0f : 0.0f;
        const vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        const vec3 face = p + vec3{0.1f, 0, 1.72f};
        const vec3 dd = nrmF(face - eye, {1, 0, 0});
        if (dot(dd, c.view.look) > 0.55f) continue;  // nie im Blickfeld erscheinen
        const bool los = clearSight(*c.world, eye, face);
        float score = std::fabs(hdist(p, eye) - (dist > 0.0 ? (float)dist : 10.0f)) + (los ? -3.0f : 0.0f) + otherRoom + (float)rng_.uniform(0.0, 2.0);
        if (score < bestScore) bestScore = score, best = p;
    }
    if (bestScore > 1e8f) return;
    auto a = std::make_unique<Agent>();
    a->role = Role::Smiler;
    a->ent = Entity::Smiler;
    a->initBody(best, std::atan2(eye.y - best.y, eye.x - best.x), (unsigned)rng_.randint(1, 1 << 30));
    a->look = true;
    a->lookAt = eye;
    a->glow = 0.0f;
    a->stage = blackout ? 1 : 0;
    a->rig.setGlow(0.0f);
    log::info("Praesenz: Smiler im Dunkeln bei ({:.1f}, {:.1f}), {:.0f} m{}", best.x, best.y, hdist(best, eye), blackout ? " (Stromausfall)" : "");
    out.push_back({Event::Appear, Role::Smiler, best + vec3{0, 0, 1.7f}});
    agents_.push_back(std::move(a));
}

void Presence::spawnHound(const Context& c, std::vector<Event>& out, double dist) {
    vec3 p;
    const int lo = dist > 0.0 ? (int)(dist * 8.5) : 230, hi = dist > 0.0 ? (int)(dist * 12.5) : 360;
    if (!hiddenSpawn(c, lo, hi, true, p)) return;
    auto a = std::make_unique<Agent>();
    a->role = Role::Hound;
    a->ent = Entity::Hound;
    a->initBody(p, std::atan2(c.view.eye.y - p.y, c.view.eye.x - p.x) + (float)rng_.uniform(-1.0, 1.0), (unsigned)rng_.randint(1, 1 << 30));
    a->state = 0;
    a->stareLimit = (float)rng_.uniform(6.0, 9.0);
    a->timer = rng_.uniform(2.0, 5.0);
    a->glow = 1.0f;
    a->rig.setGlow(1.0f);
    // er streift in der Naehe herum - nicht gleich auf dich zu
    pickGoal(c, *a, 90, 200, false);
    log::info("Praesenz: Hound bei ({:.1f}, {:.1f}), {:.0f} m", p.x, p.y, hdist(p, c.view.eye));
    out.push_back({Event::Appear, Role::Hound, p});
    agents_.push_back(std::move(a));
}

// Deathmoth-Nest an der Tuer eines dunklen Zimmers, Motten auf Wand und Decke dahinter
bool Presence::spawnHive(const Context& c, double dist) {
    if (!fld().valid() || hive_.active) return false;
    std::vector<std::pair<i64, i64>> cand;
    const bool exact = hiveDoorX_ >= 0;  // V13.2: genau diese Tuer (Skript)
    if (exact) cand.push_back({hiveDoorX_, hiveDoorY_}), hiveDoorX_ = hiveDoorY_ = -1;
    else fld().ring(dist > 0.0 ? (int)(dist * 8.0) : 110, dist > 0.0 ? (int)(dist * 13.0) : 330, cand);
    const vec3 eye = c.view.eye;
    // alle Tueren auf dem Ring, in zufaelliger Reihenfolge
    std::vector<std::pair<i64, i64>> doors;
    for (auto [x, y] : cand)
        if (const world::Tile* t = c.world->tileIfLoaded(x, y); t && (t->flags & world::TF_DOOR) && t->door) doors.push_back({x, y});
    for (size_t k = doors.size(); k > 1; --k) std::swap(doors[k - 1], doors[(size_t)rng_.randint(0, (i64)k - 1)]);
    int nKind = 0, nDark = 0, nNear = 0, nSpots = 0;
    double minLight = 9.0, minAvg = 9.0;
    for (auto [x, y] : doors) {
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        const world::Door& d = *t->door;
        if (d.kind != world::DoorKind::Door || d.width() != 1 || d.top - d.sill < 2.05 || d.top - d.sill > 2.45) {
            ++nKind;
            continue;
        }
        // die beiden Seiten: welche ist dunkel?
        for (int side = 0; side < 2; ++side) {
            i64 fx = x, fy = y;
            float yaw;
            vec3 root;
            if (d.axis == 0) {
                fx = side == 0 ? d.x0 - 1 : d.x1;
                yaw = side == 0 ? kPiF : 0.0f;
                root = {(float)(side == 0 ? d.x0 : d.x1), (float)y + 0.5f, 0.0f};
            } else {
                fy = side == 0 ? d.y0 - 1 : d.y1;
                yaw = side == 0 ? -0.5f * kPiF : 0.5f * kPiF;
                root = {(float)x + 0.5f, (float)(side == 0 ? d.y0 : d.y1), 0.0f};
            }
            const world::Tile* f = c.world->tileIfLoaded(fx, fy);
            if (f && f->open() && f->room) minLight = std::min(minLight, (double)f->light), minAvg = std::min(minAvg, f->room->avgLight);
            // dunkles Zimmer: der Raum ist dunkel (die Kachel an der Tuer bekommt noch Licht vom Flur ab)
            if (!f || !f->open() || !f->room || (f->light > 0.35f && !exact) || f->clearance() < 2.6f || f->room->avgLight > (exact ? 0.3 : 0.2)) {
                ++nDark;
                continue;
            }
            root.z = f->floor;
            if (hdist(root, eye) < 8.0f && !exact) {
                ++nNear;
                continue;
            }
            const vec3 front = root + dirYaw(yaw) * 0.2f + vec3{0, 0, 1.2f};
            if (!exact && dot(nrmF(front - eye, {1, 0, 0}), c.view.look) > 0.45f && clearSight(*c.world, eye, front)) continue;  // nicht vor deinen Augen
            // Ruheplaetze: Waende und Decke des dunklen Zimmers nahe der Tuer
            struct Spot {
                vec3 p, n, f;
            };
            std::vector<Spot> spots;
            const world::Room* room = f->room;
            for (i64 yy = fy - 5; yy <= fy + 5; ++yy)
                for (i64 xx = fx - 5; xx <= fx + 5; ++xx) {
                    const world::Tile* q = c.world->tileIfLoaded(xx, yy);
                    if (!q || !q->open() || q->room != room || q->light > 0.2f) continue;  // nur im Dunkeln
                    static const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
                    for (int k = 0; k < 4; ++k) {
                        const world::Tile* n = c.world->tileIfLoaded(xx + ddx[k], yy + ddy[k]);
                        if (!n || !n->solid()) continue;
                        const vec3 nn{-(float)ddx[k], -(float)ddy[k], 0.0f};
                        const vec3 along{-nn.y, nn.x, 0.0f};
                        const float h = (float)rng_.uniform(0.6, std::min(2.3, (double)q->clearance() - 0.3));
                        vec3 p{(float)xx + 0.5f + 0.5f * (float)ddx[k], (float)yy + 0.5f + 0.5f * (float)ddy[k], q->floor + h};
                        p += along * (float)rng_.uniform(-0.35, 0.35) + nn * 0.035f;
                        const float tilt = (float)rng_.uniform(-0.6, 0.6);
                        spots.push_back({p, nn, anim::rotateAround({0, 0, 1}, nn, tilt)});
                    }
                    if (rng_.random() < 0.4) {
                        const vec3 p{(float)xx + (float)rng_.uniform(0.2, 0.8), (float)yy + (float)rng_.uniform(0.2, 0.8), q->ceil - 0.035f};
                        spots.push_back({p, {0, 0, -1}, dirYaw((float)rng_.uniform(-kPiF, kPiF))});
                    }
                }
            if (spots.size() < 6) {
                ++nSpots;
                continue;
            }
            // das Nest
            auto nest = std::make_unique<Agent>();
            nest->role = Role::Nest;
            nest->ent = Entity::Nest;
            nest->noShadow = true;
            nest->initBody(root, yaw, 7u);
            agents_.push_back(std::move(nest));
            // fuenf bis acht Motten, nicht zu dicht beieinander
            const int want = (int)rng_.randint(5, 8);
            std::vector<vec3> used;
            for (int k = 0; k < 60 && (int)used.size() < want; ++k) {
                const Spot& s = spots[(size_t)rng_.randint(0, (i64)spots.size() - 1)];
                bool close = false;
                for (const vec3& u : used) close = close || length(u - s.p) < 0.4f;
                if (close) continue;
                used.push_back(s.p);
                auto m = std::make_unique<Agent>();
                m->role = Role::Moth;
                m->ent = Entity::Moth;
                m->noShadow = true;
                m->rest = s.p, m->restN = s.n, m->restF = s.f;
                m->up = s.n, m->fwd = s.f;
                m->phase = (float)rng_.uniform(0.0, 6.28);
                m->radius = (float)rng_.uniform(0.35, 0.9);
                m->bob = (float)rng_.uniform(-0.3, 0.3);
                m->spin = rng_.random() < 0.5 ? 1.0f : -1.0f;
                m->speed = rng_.uniform(2.0, 2.8);
                m->initBody(s.p, std::atan2(s.f.y, s.f.x), (unsigned)rng_.randint(1, 1 << 30));
                anim::CreatureAnimator::Input in;  // gleich fertig an Wand oder Decke sitzend
                in.pos = s.p, in.yaw = m->yaw, in.up = s.n, in.fwd = s.f;
                for (int it = 0; it < 30; ++it) m->cre.update(0.1, in);
                m->rig.setCreaturePose(m->cre.pose());
                agents_.push_back(std::move(m));
            }
            hive_ = Hive{};
            hive_.active = true;
            hive_.nest = root;
            hive_.room = room;
            hive_.home.build(*c.world, fx, fy, 0.8f);
            log::info("Praesenz: Deathmoth-Nest an einer Tuer bei ({:.1f}, {:.1f}), {} Motten", root.x, root.y, (int)used.size());
            return true;
        }
    }
    if (dist > 0.0 || forced_ >= 0 || exact)
        log::info("Praesenz: kein Nest - {} Tueren im Umkreis ({} falsche Art, {} Seiten nicht dunkel, {} zu nah, {} ohne Platz; dunkelste Seite {:.2f} / Raum {:.2f})",
                  doors.size(), nKind, nDark, nNear, nSpots, minLight, minAvg);
    return false;
}

void Presence::spawnGestalt(const Context& c, std::vector<Event>& out, double dist) {
    if (!fld().valid()) return;
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(dist > 0.0 ? (int)(dist * 9.0) : 150, dist > 0.0 ? (int)(dist * 12.0) : 290, cand);
    const vec3 eye = c.view.eye;
    float bestScore = 1e9f;
    vec3 best;
    for (int tries = 0; tries < 200 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || t->clearance() < 2.1f || (t->flags & world::TF_STAIR) || t->light < 0.25f || darkTile(c, t, (float)x + 0.5f, (float)y + 0.5f)) continue;
        const vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        const vec3 head = p + vec3{0, 0, 1.75f};
        const vec3 dd = nrmF(head - eye, {1, 0, 0});
        if (dot(dd, c.view.look) > 0.5f) continue;            // erscheint, waehrend man woanders hinsieht
        if (!clearSight(*c.world, eye, head)) continue;       // aber dort, wo man sie sehen kann
        float score = (float)rng_.uniform(0.0, 2.0);
        if (t->flags & world::TF_DOOR) score -= 2.0f;          // in einer Tuer, am Ende eines Gangs
        if (t->room && t->room->isCorridor) score -= 1.5f;
        if (score < bestScore) bestScore = score, best = p;
    }
    if (bestScore > 1e8f) return;
    auto a = std::make_unique<Agent>();
    a->role = Role::Gestalt;
    a->ent = Entity::Gestalt;
    a->noShadow = true;  // sie ist nicht wirklich da
    a->initBody(best, std::atan2(eye.y - best.y, eye.x - best.x), (unsigned)rng_.randint(1, 1 << 30));
    a->look = true;
    a->lookAt = eye;
    log::info("Praesenz: Gestalt am Ende eines Gangs bei ({:.1f}, {:.1f}), {:.0f} m", best.x, best.y, hdist(best, eye));
    (void)out;
    agents_.push_back(std::move(a));
}

void Presence::force(Role r, double dist) {
    // Aufruf aus Skripten: beim naechsten update() erscheinen, unabhaengig vom Bereich
    forced_ = (int)r;
    forcedDist_ = dist;
}

void Presence::showcase(int kind, vec3 pos, float yaw, float walkSpeed) {
    auto a = std::make_unique<Agent>();
    // V13.2: kind 0..kEntityCount-1 = Wesen (Rolle = Entity, gleiche Reihenfolge), 99 = Mensch (der Spieler)
    a->human = kind == 99;
    a->ent = (Entity)std::clamp(kind, 0, anim::kEntityCount - 1);
    a->role = (Role)std::clamp(kind, 0, kRoleCount - 1);
    a->showcase = true;
    a->walkShow = walkSpeed > 0.0f;
    a->speed = walkSpeed;
    a->glow = 1.0f;
    a->look = kind == 0;
    if (a->ent == Entity::Moth) pos.z += 1.3f;
    if (a->ent == Entity::Window) pos.z += 1.9f;  // V13.2: Mitte des Fensters (Level 188: 1,9 m ueber dem Boden)
    if (a->ent == Entity::Arms) pos.z += 2.8f, a->up = {0, 0, -1};  // V13.3: Oeffnung in der Decke (Gang in Level 1: 2,8 m)
    if (a->ent == Entity::Wrangler) {  // V13.3: quer vor dem Betrachter, ein Bogen aus dem Boden; der Kopf bricht 2,5 m zum Betrachter hin heraus
        a->fwd = {std::cos(yaw), std::sin(yaw), 0.0f}, a->bait = 2.5f / 32.0f, a->up = {0, 0, 1};
        yaw += 1.5708f;
        a->lean = 6.8f / 16.0f, a->crouch = 0.14f, a->tilt = 1.45f / 4.0f, a->hang = 1.0f, a->fly = 0.35f;
    }
    a->initBody(pos, yaw, (unsigned)rng_.randint(1, 1 << 30));
    a->rig.setGlow(1.0f);
    if (a->ent == Entity::Moth && a->walkShow) a->fly = 1.0f;
    agents_.push_back(std::move(a));
}

bool Presence::showcasePos(vec3& pos, float& yaw) const {
    for (auto it = agents_.rbegin(); it != agents_.rend(); ++it)
        if ((*it)->showcase) {
            pos = (*it)->pos, yaw = (*it)->yaw;
            return true;
        }
    return false;
}

bool Presence::showcaseSet(const std::string& key, float v) {
    Agent* a = nullptr;
    for (auto it = agents_.rbegin(); it != agents_.rend() && !a; ++it)
        if ((*it)->showcase) a = it->get();
    if (!a) return false;
    const vec3 side{std::cos(a->yaw + 1.1f), std::sin(a->yaw + 1.1f), 0.0f};
    if (key == "jaw") a->jaw = v;
    else if (key == "alert") a->alert = v;
    else if (key == "crouch") a->crouch = v;
    else if (key == "growl") a->growl = v;
    else if (key == "reach") a->reach = v;
    else if (key == "fly") a->fly = v;
    else if (key == "shiver") a->shiver = v;
    else if (key == "lean") a->lean = v;
    else if (key == "tilt") a->tilt = v;
    else if (key == "hang") a->hang = v;
    else if (key == "bait") a->bait = v;
    else if (key == "light") a->light = v;
    else if (key == "perceive") a->perceive = v, a->perceiveAt = a->pos + side * 3.0f + vec3{0, 0, 0.8f};
    else if (key == "look") a->look = v > 0.5f;
    else if (key == "speed") a->speed = v, a->walkShow = v > 0.0f;
    else if (key == "wave") a->wave = v;
    else if (key == "talk") a->talk = v;
    else if (key == "plea") a->plea = v;
    else if (key == "strike") a->strike = v, a->strikeLoop = v >= 1.0f;
    else if (key == "variant") a->variant = (int)v, a->initBody(a->pos, a->yaw, 7);
    else if (key == "task") a->state = (int)v, a->stateT = 0.0;  // V13.3 (Ouroboros-Arbeiter: Aufgabe)
    else return false;
    return true;
}

bool Presence::kinSet(Role r, const std::string& key, double v) {
    for (auto& a : agents_)
        if (a->role == r && !a->showcase) {
            if (key == "hungry") a->hungry = v > 0.5;
            else if (key == "pleads") a->pleads = v > 0.5;
            else if (key == "mode") a->mode = (int)v;
            else if (key == "provoke") a->provoke = v;
            else if (key == "state") a->state = (int)v, a->stateT = 0.0;  // V13.3 (Tests, Bilder)
            else if (key == "variant") a->variant = a->mode = (int)v, a->initBody(a->pos, a->yaw, 7), a->rig.setGlow(1.0f);  // V13.3 (Wrangler)
            else return false;
            return true;
        }
    return false;
}

int Presence::variantOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->variant;
    return -1;
}

vec3 Presence::headOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->cre.headPos();
    return {};
}

void Presence::clearShowcase() {
    std::erase_if(agents_, [](const auto& a) { return a->showcase; });
}

// --- Bewegung ----------------------------------------------------------------------------------
// Ein Stueck dem Feld folgen (koerperlich, mit Tempo); false: kein Weg
bool Presence::walkTo(Agent& a, const NavField& f, double speed, double dt, const Context& c) {
    double remaining = speed * dt;
    double tx = a.pos.x, ty = a.pos.y;
    bool moved = false;
    for (int it = 0; it < 4 && remaining > 1e-4; ++it) {
        if (!f.next(a.pos.x, a.pos.y, tx, ty)) break;
        double dx = tx - a.pos.x, dy = ty - a.pos.y, len = std::hypot(dx, dy);
        if (len < 1e-4) break;
        double st = std::min(remaining, len);
        a.pos.x += (float)(dx / len * st), a.pos.y += (float)(dy / len * st);
        remaining -= st;
        moved = true;
    }
    if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)); t && t->open())
        a.pos.z += (t->floor - a.pos.z) * (float)std::min(1.0, dt * 12.0);
    if (moved) {
        float want = (float)std::atan2(ty - a.pos.y, tx - a.pos.x);
        a.yaw += wrapA(want - a.yaw) * (float)std::min(1.0, dt * 5.0);
    }
    return moved;
}

// --- Verhalten ---------------------------------------------------------------------------------
// Smiler: im Dunkeln; Blickkontakt haelt ihn fest; ungesehen kommt er durch das Dunkel naeher
// V11: der toedliche Zugriff. Die Kreatur faehrt aus ihrer Richtung bis knapp vor die Linse (0,11 s),
// zittert, das Maul ganz offen; nach 2,6 s ist sie fort (der Schirm ist dann laengst schwarz)
void Presence::startKill(Agent& a, const Context& c) {
    const vec3 eye = c.view.eye;
    a.state = 10;
    a.stateT = 0.0;
    a.killDir = nrmF(flat(a.cre.headPos() - eye), nrmF(flat(c.view.look), {1, 0, 0}));
    const vec3 off = a.cre.headPos() - a.pos, f = dirYaw(a.yaw), l{-f.y, f.x, 0.0f};
    a.killHead = {dot(off, f), dot(off, l), off.z};
    log::info("Praesenz: {} - toedlicher Zugriff", roleName(a.role));
}

void Presence::killPose(Agent& a, double dt, const Context& c) {
    a.stateT += dt;
    const vec3 eye = c.view.eye;
    const float u = (float)std::min(1.0, a.stateT / 0.11);
    // Laecheln: das Gesicht fuellt das Bild; Hund: etwas weiter und tiefer, der Blick faellt ins offene Maul;
    // V11 Buendel: die Masse baeumt sich vor dir auf, das Maul nach vorn, der lange Arm greift zu
    const bool hound = a.role == Role::Hound, clump = a.role == Role::Clump;
    const float stop = clump ? 0.62f : (hound ? 0.5f : 0.36f);
    vec3 head = eye + a.killDir * (1.1f + (stop - 1.1f) * u * u) + vec3{0, 0, clump ? -0.3f : (hound ? -0.14f : -0.04f)};
    head += vec3{(float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0)} * 0.012f;
    const float yaw = std::atan2(eye.y - head.y, eye.x - head.x);
    const vec3 f = dirYaw(yaw), l{-f.y, f.x, 0.0f};
    a.snapBody(head - (f * a.killHead.x + l * a.killHead.y + vec3{0, 0, a.killHead.z}), yaw);
    a.look = true;
    a.lookAt = eye;
    a.jaw = 1.0f, a.glow = 1.0f, a.reach = 1.0f, a.crouch = 0.0f, a.growl = 1.0f, a.alert = 1.0f;
    if (a.stateT > 2.6) a.state = 9;
}

void Presence::thinkSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.state == 10) {  // V11: Zugriff
        killPose(a, dt, c);
        return;
    }
    const vec3 eye = c.view.eye;
    const vec3 face = a.cre.headPos();
    const vec3 df = face - eye;
    const float dist = length(df), d = hdist(a.pos, eye);
    const float ang = std::acos(std::clamp(dot(df / std::max(dist, 0.01f), c.view.look), -1.0f, 1.0f));
    const bool los = clearSight(*c.world, eye, face);
    a.contact = los && ang < 0.33f && dist < 30.0f && a.glow > 0.3f;
    a.look = true;
    a.lookAt = eye;
    a.jaw = std::clamp(1.25f - d / 9.0f, 0.12f, 1.0f);
    a.stateT += dt;
    // V13.2: Licht zieht ihn an (Entity-3-Seite) - das Gesicht wendet sich zur Lampe, die Augen kneifen, keine Gewoehnung;
    // Laerm in der Naehe: die Augen gehen weit auf, das Gesicht dreht sich dorthin
    lightReact(a, c, face, 22.0f, dt, 1e9);
    a.perceive = c.noise > 0.4 && d < 14.0f ? 1.0f : 0.0f;
    a.perceiveAt = eye;
    const world::Tile* here = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y));
    const bool dark = darkTile(c, here, a.pos.x, a.pos.y);
    if (a.contact && !a.stared) {
        a.stared = true;
        out.push_back({Event::Stare, Role::Smiler, face});
    }
    // Verschwinden: das Licht kehrt zurueck, er verliert das Interesse, oder er ist zu lange da
    if (a.state == 8) {
        a.glow -= (float)dt * 1.4f;
        if (a.glow <= 0.0f) {
            out.push_back({Event::Gone, Role::Smiler, face});
            a.state = 9;
        }
        return;
    }
    if (a.state == 0) {  // die Augen oeffnen sich
        a.glow = std::min(1.0f, a.glow + (float)dt * 0.55f);
        if (a.glow >= 1.0f) a.state = 1, a.stateT = 0.0;
    }
    // V13.3: ein staerkeres Licht (geworfener Leuchtstab) zieht ihn vom Spieler fort (presence_lure.cpp)
    if (baitSmiler(a, dt, c, out, d, dark)) return;
    const world::Tile* pt = c.world->tileIfLoaded(ifloor(c.player->x), ifloor(c.player->y));
    const bool playerInLight = pt && !darkTile(c, pt, (float)c.player->x, (float)c.player->y);
    // Interesse: zieht das Licht der Lampe ihn an, bleibt er; wer im Licht steht oder die Lampe aus hat
    // und Abstand haelt, wird uninteressant
    if ((playerInLight && d > 5.0f) || (!c.view.flashOn && d > 10.0f && !a.contact) || d > 24.0f) a.lostT += dt;
    else a.lostT = std::max(0.0, a.lostT - dt * 0.5);
    if (!dark || a.lostT > 16.0 || a.age > 170.0) {
        a.state = 8;
        if (!dark && here) out.push_back({Event::Flicker, Role::Smiler, a.pos + vec3{0, 0, 2.0f}});
        log::info("Praesenz: Smiler erlischt ({})", !dark ? "Licht" : (a.lostT > 16.0 ? "kein Interesse" : "Zeit"));
        return;
    }
    if (a.state < 1) return;
    // Panik: wer in seiner Naehe rennt, wird angegriffen (Wikidot). V11 (F2): erst oeffnet es sich weit,
    // der Ton steigt - 1,5 s Zeit, stehen zu bleiben und den Blick zu halten (F3)
    // V13.2 (Entity-3-Seite: Panik "oder lauter Laerm"): eine harte Landung oder der Aufprall eines geworfenen
    // Dings in seiner Naehe wirkt wie Rennen
    bool loud = c.noise >= 0.7 && d < 10.0f;
    if (c.pings)
        for (const auto& [where, vol] : *c.pings) loud = loud || (vol >= 0.5f && hdist(where, a.pos) < 8.0f);
    const bool panic = (c.player->sprinting && d < 10.0f) || loud;
    if (panic && a.stage < 2) {
        a.stage = 2;
        a.timer2 = 1.5;
        out.push_back({Event::Announce, Role::Smiler, face});
        log::info("Praesenz: Smiler - Panik, er oeffnet sich (Ankuendigung)");
    }
    if (a.stage == 2) {
        a.jaw = 1.0f;
        a.glow = 1.0f;
        a.timer2 -= dt;
        // V13.2: es bleibt mindestens 0,6 s weit offen (sichtbar), auch wenn man schon stillsteht
        const bool calm = !c.player->sprinting && c.player->groundSpeed < 0.6 && a.contact && a.timer2 < 0.9;
        if (calm) {
            a.stage = 1;
            out.push_back({Event::Calm, Role::Smiler, face});
            log::info("Praesenz: Smiler - beruhigt (Spieler steht und haelt den Blick)");
        } else if (a.timer2 <= 0.0) {
            const bool still = c.player->sprinting || c.player->groundSpeed > 2.6;
            a.stage = still ? 3 : 1;
            if (!still) out.push_back({Event::Calm, Role::Smiler, face});
            log::info("Praesenz: Smiler - {}", still ? "Zugriff" : "beruhigt");
        }
        if (a.stage == 2) return;  // waehrend der Ankuendigung ruehrt es sich nicht
    }
    // Blickkontakt: er ruehrt sich nicht (ausser im Angriff)
    if (a.contact && a.stage < 2) return;
    // Angriff
    if (d < 1.05f && std::fabs(a.pos.z - (float)c.player->z) < 1.4f) {
        if (c.mode >= 2) {  // V11: toedlich
            out.push_back({Event::Attack, Role::Smiler, face, 1.0f});
            startKill(a, c);
            return;
        }
        out.push_back({Event::Flicker, Role::Smiler, face});
        out.push_back({Event::Gone, Role::Smiler, face});
        a.state = 9;
        return;
    }
    // naeher - nur durch das Dunkel: am Rand des Lichts bleibt er stehen
    double tx, ty;
    if (!fld().next(a.pos.x, a.pos.y, tx, ty)) return;
    const world::Tile* nt = c.world->tileIfLoaded(ifloor(tx), ifloor(ty));
    const bool nextDark = darkTile(c, nt, (float)tx, (float)ty) || hdist(vec3{(float)tx, (float)ty, 0.0f}, eye) < 1.2f;
    if (!nextDark && a.stage < 3) return;
    const double speed = a.stage >= 3 ? 4.2 : 1.25;
    walkTo(a, fld(), speed, dt, c);
    // ein tiefes, kaum hoerbares Droehnen, wenn er ungesehen naeher kommt (eigene Ergaenzung)
    a.timer -= dt;
    if (d < 9.0f && a.timer <= 0.0) {
        out.push_back({Event::Rustle, Role::Smiler, face, std::clamp(1.0f - d / 9.0f, 0.2f, 1.0f)});
        a.timer = rng_.uniform(3.0, 5.0);
    }
}

// Hound: streift umher, hoert und sieht; pirscht, knurrt, laesst sich durch Blickkontakt eine Weile
// einschuechtern; stuermt los, wenn man ihm den Ruecken zukehrt oder rennt
void Presence::thinkHound(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.state == 10) {  // V11: Zugriff
        killPose(a, dt, c);
        return;
    }
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    const bool playerLit = c.view.flashOn || litPoint(c.view, *c.world, eye - vec3{0, 0, 0.6f});
    const float seeRange = c.view.flashOn ? 32.0f : (playerLit ? 22.0f : 7.0f);  // das Licht zieht ihn an (Fandom)
    const bool sees = los && d < seeRange && (facing > 0.15f || d < 5.0f);
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    const bool hears = (c.player->sprinting && pathD < 30.0f) || (pathD < 7.0f && c.player->gait > 0.3f);
    // Blickkontakt: man sieht ihn an - im Licht oder seine weissen Augen im Dunkeln
    a.contact = los && a.sight.inView && a.sight.angle < 0.3f && (a.sight.level > 0.2f || d < 12.0f);
    a.stateT += dt;
    a.cool -= dt;
    const vec3 ppos{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    if (sees) a.heard = ppos, a.lostT = 0.0;
    else a.lostT += dt;
    auto setState = [&](int s) {
        a.state = s;
        a.stateT = 0.0;
        if (s == 2) a.timer = 0.0, a.stareT = 0.0;  // Zeit ohne Blickkontakt beginnt neu
    };
    a.growl = 0.0f;
    a.reach = 0.0f;
    if (a.state < 3) a.jaw = 0.0f;
    // V13.2: geblendet zuckt er zurueck (Pfote vor die Augen), nach 1,5 s hat er sich gewoehnt
    lightReact(a, c, head, 18.0f, dt, 1.5);
    a.perceive = 0.0f;
    switch (a.state) {
        case 0: {  // streift umher
            a.crouch = 0.0f;
            a.alert = 0.3f;
            a.look = false;
            if (sees) {
                setState(2);
                out.push_back({Event::Growl, Role::Hound, head, 0.6f});
                a.timer2 = rng_.uniform(2.0, 3.5);
                break;
            }
            if (hears) {
                setState(1);
                a.heard = ppos;
                a.field.build(*c.world, ifloor(ppos.x), ifloor(ppos.y), 1.1f);
                break;
            }
            a.timer -= dt;
            if (a.timer <= 0.0) {  // stehen bleiben, lauschen
                a.timer = rng_.uniform(5.0, 11.0);
                a.timer2 = rng_.uniform(1.5, 3.5);
            }
            if (a.timer2 > 0.0) {
                a.timer2 -= dt;
                a.alert = 0.8f;
                // V13.2: er wittert - Kopf hoch, schnueffelt in die Richtung, aus der er zuletzt etwas hoerte
                a.perceive = 1.0f;
                a.perceiveAt = a.heard + vec3{0, 0, 0.8f};
                if (hdist(a.heard, a.pos) < 0.5f) a.perceiveAt = a.pos + dirYaw(a.yaw + 0.8f) * 3.0f + vec3{0, 0, 0.8f};
                break;
            }
            if (!a.hasGoal || !walkTo(a, a.field, 0.85, dt, c) || hdist(a.pos, a.goal) < 1.2f)
                if (!pickGoal(c, a, 60, 220, false)) a.timer2 = 1.5;  // kein Weg: erst einmal lauschen
            break;
        }
        case 1: {  // gehoert: geht hin
            a.alert = 1.0f;
            a.crouch = 0.3f;
            if (sees) {
                setState(2);
                out.push_back({Event::Growl, Role::Hound, head, 0.7f});
                a.timer2 = rng_.uniform(1.5, 3.0);
                break;
            }
            if (hears && hdist(ppos, a.heard) > 3.0f) {
                a.heard = ppos;
                a.field.build(*c.world, ifloor(ppos.x), ifloor(ppos.y), 1.1f);
            }
            if (!walkTo(a, a.field, 1.4, dt, c) || hdist(a.pos, a.heard) < 1.5f) {
                // V13.2: am Ort des Geraeuschs: wittern, bevor er weiterzieht
                a.perceive = 1.0f;
                a.perceiveAt = a.pos + dirYaw(a.yaw + (float)std::sin(a.stateT * 0.7) * 1.2f) * 3.0f + vec3{0, 0, 0.8f};
                if (a.stateT > 4.0) setState(0), pickGoal(c, a, 60, 220, false);
            }
            break;
        }
        case 2: {  // pirscht: geduckt, knurrend, auf Abstand
            a.crouch = 1.0f;
            a.alert = 1.0f;
            a.look = true;
            a.lookAt = eye;
            a.growl = d < 15.0f ? 1.0f : 0.4f;
            a.timer2 -= dt;
            if (a.timer2 <= 0.0 && d < 18.0f) {
                out.push_back({Event::Growl, Role::Hound, head, std::clamp(1.0f - d / 18.0f, 0.3f, 1.0f)});
                a.timer2 = rng_.uniform(2.4, 3.8);
            }
            if (a.contact) {
                a.stareT += dt;
                a.timer = 0.0;  // Zeit ohne Blickkontakt
                if (a.stareT < a.stareLimit) {
                    // eingeschuechtert: bleibt stehen, weicht ein Stueck zurueck
                    if (d < 5.0f) {
                        const vec3 back = a.pos - toP * (float)(0.45 * dt);
                        if (figureWalkable(c.world->tileIfLoaded(ifloor(back.x), ifloor(back.y)), 1.1f)) a.pos.x = back.x, a.pos.y = back.y;
                    }
                    a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 4.0);
                } else if (d > 3.2f) {
                    walkTo(a, fld(), 0.45, dt, c);  // der Blick wirkt nicht mehr - er kommt wieder
                }
            } else {
                a.stareT = std::max(0.0, a.stareT - dt * 0.3);
                a.timer += dt;
                if (d > 4.0f) walkTo(a, fld(), 0.8, dt, c);
                else a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 4.0);
                // den Ruecken zugekehrt: Bellen, dann der Angriff
                if (a.timer > 1.4 && d < 9.0f && a.cool <= 0.0) {
                    setState(3);
                    out.push_back({Event::Bark, Role::Hound, head});
                    break;
                }
            }
            if (c.player->sprinting && d < 14.0f && a.stateT > 0.8 && a.cool <= 0.0) {
                setState(3);
                out.push_back({Event::Bark, Role::Hound, head});
                break;
            }
            // langsam zurueckgezogen, Licht aus: er verliert die Spur
            if ((a.lostT > 10.0 && d > 10.0f) || d > 32.0f) {
                out.push_back({Event::Whine, Role::Hound, head});
                out.push_back({Event::Lost, Role::Hound, a.pos});
                setState(4);
                pickGoal(c, a, 260, 380, true);
            }
            break;
        }
        case 3: {  // stuermt los
            a.crouch = 0.4f;
            a.look = true;
            a.lookAt = eye - vec3{0, 0, 0.3f};
            a.jaw = d < 3.0f ? 1.0f - d / 3.0f : 0.2f;
            a.reach = d < 2.2f ? 1.0f - d / 2.2f : 0.0f;
            const double speed = c.stalkerSpeed * 1.25;
            if (d < 1.15f && std::fabs(a.pos.z - (float)c.player->z) < 1.0f) {
                if (c.mode >= 2) {  // V11: toedlich
                    out.push_back({Event::Attack, Role::Hound, head, 1.0f});
                    startKill(a, c);
                    break;
                }
                a.cool = 8.0;
                a.jaw = 1.0f;
                setState(4);
                pickGoal(c, a, 230, 380, true);
                break;
            }
            if (!walkTo(a, fld(), speed, dt, c) || a.stateT > 6.0) setState(2), a.cool = 3.0;
            if (d < 6.0f && a.stateT > 0.5) {
                a.timer2 -= dt;
                if (a.timer2 <= 0.0) out.push_back({Event::Breath, Role::Hound, head}), a.timer2 = 0.7;
            }
            break;
        }
        case 4: {  // zieht sich zurueck
            a.crouch = 0.2f;
            a.look = false;
            a.jaw = std::max(0.0f, a.jaw - (float)dt);
            a.timer2 -= dt;
            if ((!a.hasGoal || !walkTo(a, a.field, a.stateT < 3.0 ? 3.0 : 1.6, dt, c)) && a.timer2 <= 0.0) {
                if (!pickGoal(c, a, 260, 380, true) && a.unseenT > 1.5) a.state = 9;
                a.timer2 = 1.0;
            }
            if ((d > 26.0f && a.unseenT > 1.5) || (a.stateT > 40.0 && a.unseenT > 1.5)) {
                out.push_back({Event::Gone, Role::Hound, a.pos});
                a.state = 9;
            }
            break;
        }
        default: break;
    }
    if (a.age > 260.0 && a.state < 4) setState(4), pickGoal(c, a, 260, 380, true);
}

// Deathmoth: sitzt reglos; aufgescheucht kreist sie um das Licht, stoesst zu, kehrt zurueck
void Presence::thinkMoth(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    a.stateT += dt;
    a.cool -= dt;
    a.shiver = 0.0f;
    // V13.2: Bewegung - sitzend nimmt sie dich wahr (Fuehler vor), fliegend zieht die Lampe sie an, beim Zustossen Sturzflug
    a.reach = 0.0f;
    a.perceive = hdist(a.pos, eye) < 5.5f ? 1.0f : 0.0f;
    a.perceiveAt = eye;
    a.light = c.view.flashOn && hdist(a.pos, eye) < 8.0f ? 1.0f : 0.0f;
    a.lightFrom = c.view.flashPos;
    auto openAt = [&](vec3 p) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        return t && t->open() && t->clearance() > 0.6f;
    };
    // Weg: steht etwas zwischen Motte und Ziel, folgt sie dem Feld (zum Spieler bzw. zum Nest)
    auto steer = [&](vec3 tgt, const NavField& f) {
        if (clearSight(*c.world, a.pos, tgt)) return tgt;
        double tx, ty;
        if (!f.next(a.pos.x, a.pos.y, tx, ty)) return tgt;
        const world::Tile* t = c.world->tileIfLoaded(ifloor(tx), ifloor(ty));
        return vec3{(float)tx, (float)ty, t ? std::min(t->floor + 1.6f, t->ceil - 0.3f) : a.pos.z};
    };
    switch (a.state) {
        case 0:  // sitzt
            a.pos = a.rest, a.up = a.restN, a.fwd = a.restF, a.fly = 0.0f, a.vel = {};
            return;
        case 1:  // zittert mit den Fluegeln, dann fort
            a.shiver = 1.0f;
            a.fly = 0.0f;
            if (a.stateT > a.timer) {
                a.state = 2, a.stateT = 0.0;
                a.vel = a.restN * 1.4f + vec3{0, 0, 0.3f};
                a.fly = 1.0f;
            }
            return;
        case 4: {  // zurueck zum Ruheplatz, landen
            a.fly = 1.0f;
            const vec3 home = a.rest + a.restN * 0.15f;
            const vec3 to = home - a.pos;
            const float l = length(to);
            if (l < 0.18f) {
                a.state = 0, a.stateT = 0.0;
                a.pos = a.rest;
                return;
            }
            const vec3 w = steer(home, hive_.home);
            a.vel += (nrmF(w - a.pos, {0, 0, 1}) * (l < 0.6f ? 1.0f : 1.9f) - a.vel) * (float)std::min(1.0, dt * 3.0);
            if (a.stateT > 25.0 && !a.sight.inView) a.state = 0, a.stateT = 0.0, a.pos = a.rest;  // festgeflogen: ungesehen zurueck
            break;
        }
        default: {
            a.fly = 1.0f;
            vec3 tgt = hive_.hasTarget ? hive_.target : hive_.nest + vec3{0, 0, 1.6f};
            tgt = steer(tgt, hive_.hasTarget && c.view.flashOn ? fld() : hive_.home);
            if (a.state == 3) {  // stoesst zu
                a.reach = 1.0f;
                tgt = eye - vec3{0, 0, 0.25f};
                const vec3 to = tgt - a.pos;
                const float dl = length(to);
                if (dl < 0.38f) {
                    if (c.mode >= 2) out.push_back({Event::Sting, Role::Moth, a.pos});
                    a.cool = rng_.uniform(4.0, 6.5);
                    a.state = 2, a.stateT = 0.0;
                    a.vel = -nrmF(to, {1, 0, 0}) * 2.4f;
                    break;
                }
                // direkt auf dich zu - nahe am Ziel langsamer, damit sie nicht vorbeischiesst
                a.vel += (nrmF(to, {1, 0, 0}) * std::min(4.4f, 1.2f + 3.0f * dl) - a.vel) * (float)std::min(1.0, dt * 12.0);
                if (a.stateT > 2.2) a.state = 2, a.stateT = 0.0, a.cool = 2.0;
                break;
            }
            // kreist flatternd um das Licht (unruhig, mit Ruckern)
            a.phase += (float)dt * (2.0f + 0.6f * a.radius) * a.spin;
            const vec3 want = tgt + vec3{std::cos(a.phase) * a.radius, std::sin(a.phase) * a.radius, a.bob + 0.15f * std::sin(a.phase * 2.3f)};
            const vec3 jit{(float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-0.6, 0.6)};
            a.vel += ((want - a.pos) * 4.0f - a.vel * 1.6f + jit * 5.0f) * (float)dt;
            const float sp = length(a.vel);
            if (sp > (float)a.speed + 1.0f) a.vel = a.vel * (((float)a.speed + 1.0f) / sp);
            if (c.view.flashOn && hdist(a.pos, eye) < 7.0f && a.cool <= 0.0 && rng_.random() < dt * 0.2) a.state = 3, a.stateT = 0.0;
            break;
        }
    }
    // Waende: abprallen statt hindurch; zwischen Boden und Decke bleiben
    vec3 np = a.pos + a.vel * (float)dt;
    if (!openAt(np)) {
        if (!openAt({np.x, a.pos.y, a.pos.z})) a.vel.x = -a.vel.x * 0.6f;
        if (!openAt({a.pos.x, np.y, a.pos.z})) a.vel.y = -a.vel.y * 0.6f;
        np = a.pos + a.vel * (float)dt;
        if (!openAt(np)) np = {a.pos.x, a.pos.y, np.z};
    }
    if (const world::Tile* t = c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)); t && t->open()) {
        const float lo = t->floor + 0.25f, hi = t->ceil - (a.state == 4 ? 0.03f : 0.12f);
        if (np.z > hi) np.z = std::max(hi, a.pos.z - (float)dt * 0.5f), a.vel.z = -std::fabs(a.vel.z) * 0.5f;
        if (np.z < lo) np.z = std::min(lo, a.pos.z + (float)dt * 0.5f), a.vel.z = std::fabs(a.vel.z) * 0.5f;
    }
    a.pos = np;
    if (length(flat(a.vel)) > 0.05f) a.yaw = std::atan2(a.vel.y, a.vel.x);
    a.up = {0, 0, 1};
    a.fwd = {};
}

// Gestalt: veraendert sich nie, solange man sie sieht; sieht man weg, ist sie fort
void Presence::thinkGestalt(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const float d = a.sight.dist;
    const bool seen = a.sight.level > 0.25f;
    a.look = true;
    a.lookAt = c.view.eye;
    if (seen && !a.stared) {
        a.stared = true;
        out.push_back({Event::Vision, Role::Gestalt, a.pos + vec3{0, 0, 1.6f}});
    }
    if (seen) return;
    // V13.2: ungesehen nimmt sie den Beobachter wahr - Kopf zu ihm, vorgebeugt (sichtbar erst beim naechsten Blick)
    a.perceive = d < 26.0f ? 1.0f : 0.0f;
    a.perceiveAt = c.view.eye;
    // kurz weggesehen: der Kopf ist anders geneigt; laenger: fort - manchmal steht sie spaeter woanders
    if (a.wasSeen && a.unseenT > 0.25 && a.unseenT < 0.25 + dt * 1.5) a.tilt = (float)rng_.uniform(-0.3, 0.3);
    if ((a.wasSeen && a.unseenT > 0.8) || d < 11.0f || a.age > 75.0) {
        a.state = 9;
        if (a.wasSeen && gestaltRepeat_ < 2 && rng_.random() < 0.55) {
            ++gestaltRepeat_;
            nextGestalt_ = rng_.uniform(18.0, 40.0);
        } else {
            gestaltRepeat_ = 0;
        }
    }
}

// Schwarm am Nest: aufscheuchen, Ziel (Licht), beruhigen, verschwinden
void Presence::updateHive(double dt, const Context& c, std::vector<Event>& out) {
    if (!hive_.active) return;
    hive_.age += dt;
    const vec3 eye = c.view.eye;
    int flying = 0, moths = 0, inView = 0;
    vec3 centroid{};
    for (auto& ap : agents_) {
        Agent& m = *ap;
        if (m.showcase || (m.role != Role::Moth && m.role != Role::Nest)) continue;
        if (m.sight.inView) ++inView;
        if (m.role != Role::Moth) continue;
        ++moths;
        centroid += m.pos;
        if (m.state >= 1) ++flying;
        // Licht trifft sie oder man kommt zu nah: der ganze Schwarm fliegt auf
        if (!hive_.alarmed && m.state == 0 && (inBeam(c.view, *c.world, m.pos, 14.0f) || length(m.pos - eye) < 3.2f)) {
            hive_.alarmed = true;
            hive_.darkT = 0.0;
            out.push_back({Event::TakeOff, Role::Moth, m.pos});
            log::info("Praesenz: der Mottenschwarm fliegt auf");
            for (auto& bp : agents_)
                if (bp->role == Role::Moth && !bp->showcase && bp->state == 0) bp->state = 1, bp->stateT = 0.0, bp->timer = rng_.uniform(0.25, 1.1);
        }
    }
    if (moths) centroid = centroid / (float)moths;
    if (hive_.alarmed) {
        // Ziel: die Lampe des Spielers - sonst die naechste brennende Deckenleuchte
        hive_.darkT = c.view.flashOn ? 0.0 : hive_.darkT + dt;
        if (c.view.flashOn) {
            hive_.target = c.view.flashPos + c.view.flashDir * 1.3f;
            hive_.hasTarget = true;
        } else if (hive_.darkT < 0.1 || std::fmod(hive_.age, 1.0) < dt) {
            hive_.hasTarget = false;
            float best = 1e9f;
            for (int dy = -7; dy <= 7; ++dy)
                for (int dx = -7; dx <= 7; ++dx) {
                    const i64 x = ifloor(centroid.x) + dx, y = ifloor(centroid.y) + dy;
                    const world::Tile* t = c.world->tileIfLoaded(x, y);
                    if (!t || !(t->flags & world::TF_LAMP) || darkTile(c, t, (float)x + 0.5f, (float)y + 0.5f)) continue;
                    const float dd = (float)(dx * dx + dy * dy);
                    if (dd < best) best = dd, hive_.target = {(float)x + 0.5f, (float)y + 0.5f, t->ceil - 0.45f}, hive_.hasTarget = true;
                }
        }
        // Lampe aus: eine Weile um die Leuchten, dann zurueck zum Nest
        if (hive_.darkT > 9.0)
            for (auto& ap : agents_)
                if (ap->role == Role::Moth && !ap->showcase && (ap->state == 2 || ap->state == 3)) ap->state = 4, ap->stateT = 0.0;
        if (flying == 0 && hive_.darkT > 9.0) hive_.alarmed = false;
    }
    // weit weg und nicht im Blick: der Schwarm ist fort
    if (hdist(hive_.nest, eye) > 55.0f && inView == 0) {
        std::erase_if(agents_, [](const auto& a) { return !a->showcase && (a->role == Role::Moth || a->role == Role::Nest); });
        hive_ = Hive{};
        nextHive_ = rng_.uniform(60.0, 120.0);
        log::info("Praesenz: das Mottennest ist fort");
    }
}

void Presence::think(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.showcase) {  // Vorfuehrung: das Gesicht sieht zur Kamera
        if (a.look) a.lookAt = c.view.eye;
        if (a.role == Role::Growler || a.role == Role::Nguithr) prepGN(a, c);  // V13.3
        if (a.role == Role::Ouroboros) prepOuroboros(a, c);                 // V13.3
        if (a.role == Role::Goblin) prepGoblin(a, c);                       // V13.3
        if (a.role == Role::Moth && a.variant > 0) prepMothL2(a);           // V13.3 Teil 4
        return;
    }
    switch (a.role) {
        case Role::Smiler: thinkSmiler(a, dt, c, out); break;
        case Role::Hound: thinkHound(a, dt, c, out); break;
        case Role::Moth:
            if (a.variant > 0) thinkMothL2(a, dt, c, out);  // V13.3 Teil 4: Level 2, ohne Nest
            else thinkMoth(a, dt, c, out);
            break;
        case Role::Gestalt: thinkGestalt(a, dt, c, out); break;
        case Role::Clump: thinkClump(a, dt, c, out); break;
        case Role::Faceling: thinkFaceling(a, dt, c, out); break;  // V13.2
        case Role::SkinStealer: thinkSkinStealer(a, dt, c, out); break;
        case Role::Wretch: thinkWretch(a, dt, c, out); break;
        case Role::Rat: thinkRat(a, dt, c, out); break;
        case Role::Window: thinkWindow(a, dt, c, out); break;
        case Role::Duller: thinkDuller(a, dt, c, out); break;  // V13.3
        case Role::Arms: thinkArms(a, dt, c, out); break;
        case Role::Wrangler: thinkWrangler(a, dt, c, out); break;  // V13.3
        case Role::Growler: thinkGrowler(a, dt, c, out); break;  // V13.3
        case Role::Nguithr: thinkNguithr(a, dt, c, out); break;
        case Role::Ouroboros: thinkOuroboros(a, dt, c, out); break;  // V13.3
        case Role::Goblin: thinkGoblin(a, dt, c, out); break;  // V13.3
        default: break;
    }
}

// --- Buendel (V11, ENTITY_BEHAVIOR 2.3) --------------------------------------------------------
// Hoert nur - blind fuer Licht und Blick. Reichweite etwa Lautstaerke x 22 m Weg (rennen 22 m, gehen 10 m,
// leise gehen 3,3 m - unter dem Schacht oder nah am Buendel); geworfene Dinge am Ort des Aufpralls
bool Presence::clumpHears(const Agent& a, const Context& c, vec3& where, float& loud) const {
    loud = 0.0f;
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    if (c.noise > 0.01 && pathD < (float)c.noise * 22.0f) {
        where = {(float)c.player->x, (float)c.player->y, (float)c.player->z};
        loud = (float)c.noise;
    }
    if (c.pings)
        for (const auto& [p, s] : *c.pings)
            if (hdist(a.pos, p) * 1.25f < s * 22.0f && s > loud) where = p, loud = s;
    return loud > 0.0f;
}

// ein Deckenschacht in Hoerweite, aber nicht zu nah und nicht im Blick (F1): 14 - 60 m Weg
bool Presence::spawnClump(const Context& c, std::vector<Event>& out, bool forced) {
    if (!c.lairs || c.lairs->empty() || !fld().valid()) return false;
    const vec3 eye = c.view.eye;
    int best = -1;
    float bestScore = 1e9f;
    for (size_t i = 0; i < c.lairs->size(); ++i) {
        const vec3 L = (*c.lairs)[i];
        const int here = fld().at(ifloor(L.x), ifloor(L.y));
        if (here < 0) continue;
        const float pathD = (float)here / 10.0f;
        if (!forced && (pathD < 14.0f || pathD > 60.0f)) continue;
        const vec3 arm = L - vec3{0, 0, 0.8f};
        if (!forced && dot(nrmF(arm - eye, {1, 0, 0}), c.view.look) > 0.3f && clearSight(*c.world, eye, arm)) continue;
        const float score = (forced ? std::fabs(pathD - 8.0f) : std::fabs(pathD - 28.0f)) + (float)rng_.uniform(0.0, 8.0);
        if (score < bestScore) bestScore = score, best = (int)i;
    }
    if (best < 0) return false;
    const vec3 L = (*c.lairs)[(size_t)best];
    const world::Tile* t = c.world->tileIfLoaded(ifloor(L.x), ifloor(L.y));
    if (!t) return false;
    auto a = std::make_unique<Agent>();
    a->role = Role::Clump;
    a->ent = Entity::Clump;
    a->home = L;
    a->hang = 1.0f, a->bait = 1.0f;
    a->noShadow = true;
    a->initBody({L.x, L.y, t->floor}, (float)rng_.uniform(-3.14, 3.14), (unsigned)rng_.randint(1, 1 << 30));
    a->cre.setHidden(true);
    a->state = 0;
    log::info("Praesenz: Buendel im Deckenschacht bei ({:.1f}, {:.1f}), {:.0f} m", L.x, L.y, hdist(L, eye));
    out.push_back({Event::Appear, Role::Clump, L});
    agents_.push_back(std::move(a));
    return true;
}

// Lauern -> Lauschen -> Jagen (zum Ort des letzten Geraeuschs) -> Suchen -> Ausholen -> Griff (toedlich, F4);
// Rueckkehr in den Schacht nach 20 s Stille
void Presence::thinkClump(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.state == 10) {
        a.hang = 0.0f, a.bait = 0.0f;
        killPose(a, dt, c);
        return;
    }
    const vec3 ppos{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, ppos);
    vec3 where;
    float loud = 0.0f;
    const bool hears = clumpHears(a, c, where, loud);
    // ganz nah hoert es jeden Schritt, auch leise; wer still steht, bleibt unbemerkt (blind)
    const bool close = d < 2.5f && c.noise > 0.1 && a.hang < 0.5f;
    a.stateT += dt;
    a.alert = 0.0f, a.reach = 0.0f, a.shiver = 0.0f, a.perceive = 0.0f;
    a.look = false;
    a.noShadow = a.hang > 0.5f;
    auto setState = [&](int s) {
        a.state = s;
        a.stateT = 0.0;
    };
    auto chase = [&](vec3 to) {
        a.heard = to;
        a.field.build(*c.world, ifloor(to.x), ifloor(to.y), 0.6f);
    };
    auto windup = [&]() {
        setState(4);
        a.heard = ppos;  // es zielt auf die Stelle, die es gehoert hat
        a.timer = rng_.uniform(1.0, 1.5);
        out.push_back({Event::Windup, Role::Clump, a.cre.headPos() + vec3{0, 0, 0.6f}, 1.0f});
    };
    auto drag = [&]() {
        a.timer2 -= dt;
        if (a.timer2 <= 0.0) {
            out.push_back({Event::Drag, Role::Clump, a.pos + vec3{0, 0, 0.3f}, 1.0f});
            a.timer2 = rng_.uniform(1.2, 1.8);
        }
    };
    switch (a.state) {
        case 0:  // Lauern: im Schacht, ein Arm haengt heraus, die Finger zucken
            a.hang = 1.0f, a.bait = 1.0f;
            if (hears) {
                setState(1);
                a.heard = where;
                a.lostT = 0.0, a.timer = 0.4, a.knocks = 0;
            }
            break;
        case 1:  // Lauschen: der Arm zieht sich zurueck, zwei, drei Schlaege an Rohr oder Wand
            a.hang = 1.0f, a.bait = 0.0f;
            a.timer -= dt;
            if (a.timer <= 0.0 && a.knocks < 3) {
                out.push_back({Event::Knock, Role::Clump, a.home - vec3{0, 0, 0.15f}, 1.0f});
                ++a.knocks;
                a.timer = rng_.uniform(1.0, 1.6);
            }
            a.lostT = hears ? 0.0 : a.lostT + dt;
            if (hears && a.stateT > 1.6) {  // weiter Laerm: es faellt heraus und jagt
                chase(where);
                setState(2);
                out.push_back({Event::Drop, Role::Clump, a.pos + vec3{0, 0, 0.3f}, 1.0f});
            } else if (a.lostT > 10.0) {
                setState(0);
            }
            break;
        case 2: {  // Jagen: Hand ueber Hand zum Ort des letzten Geraeuschs (nicht zum Spieler)
            a.hang = 0.0f, a.bait = 0.0f;
            if (close && c.mode >= 2) {
                windup();
                break;
            }
            if (hears && hdist(where, a.heard) > 1.5f) chase(where);
            if (a.stateT < 0.9) break;  // faellt noch
            const double surge = 0.55 + 0.9 * std::pow(std::max(0.0, std::sin(time_ * 4.1 + a.age)), 2.0);  // ruckartig
            const bool moving = walkTo(a, a.field, 1.35 * surge, dt, c);
            drag();
            if (!moving || hdist(a.pos, a.heard) < 1.2f) setState(3), a.lostT = 0.0;
            break;
        }
        case 3: {  // Suchen: tastet mit allen Gliedern, verharrt
            a.hang = 0.0f;
            // V13.2: im Wechsel tasten und lauschen (erstarrt, Kern hoch, Finger tippen) - zum letzten Geraeusch hin
            const bool listen = std::fmod(a.stateT, 4.5) > 2.6;
            a.shiver = listen ? 0.0f : 1.0f;
            a.perceive = listen ? 1.0f : 0.0f;
            a.perceiveAt = a.heard;
            if (close && c.mode >= 2) {
                windup();
                break;
            }
            if (hears) {
                a.lostT = 0.0;
                if (hdist(where, a.pos) > 2.0f) chase(where), setState(2);
            } else {
                a.lostT += dt;
            }
            if (a.lostT > 20.0) {  // nichts mehr: zurueck in den Schacht
                chase(a.home);
                setState(5);
                a.cool = 0.0;
            }
            break;
        }
        case 4:  // Ausholen: der laengste Arm holt aus (1,0 - 1,5 s Ankuendigung, F2) - zur Stelle des Laerms
            a.alert = 1.0f;
            if (hears && loud >= 0.2f) a.heard = where;  // lauter Laerm verraet, wo du bist; leise Schritte nicht
            if (hdist(a.heard, a.pos) > 0.2f) a.yaw = std::atan2(a.heard.y - a.pos.y, a.heard.x - a.pos.x);
            if (d > 2.5f && c.noise < 0.2) {  // leise weggegangen: es sucht weiter
                setState(3);
                a.lostT = 0.0;
            } else if (a.stateT > a.timer) {
                if (hdist(ppos, a.heard) < 1.0f && d < 3.2f) {
                    startKill(a, c);
                    out.push_back({Event::Attack, Role::Clump, a.cre.headPos(), 1.0f});
                } else {  // blind: der Griff geht ins Leere, die Hand klatscht auf den Boden
                    out.push_back({Event::Drop, Role::Clump, a.heard, 0.6f});
                    setState(3);
                    a.lostT = 0.0;
                }
            }
            break;
        case 5:  // Rueckkehr in den Schacht
            if (hears) {
                chase(where);
                setState(2);
                break;
            }
            if (hdist(a.pos, a.home) > 0.6f && walkTo(a, a.field, 1.0, dt, c)) {
                a.hang = 0.0f;
                drag();
            } else {
                a.hang = 1.0f;  // hinauf
                a.cool += dt;
                if (a.cool > 1.6) setState(0);
            }
            break;
        default: break;
    }
}


// --- Ablauf ------------------------------------------------------------------------------------
void Presence::regie(double dt, const Context& c, std::vector<Event>& out) {
    const Player& p = *c.player;
    const i64 pcx = ifloor(p.x), pcy = ifloor(p.y);
    // --- Regie ---------------------------------------------------------------------------------
    if (c.mode > 0 && !c.suspended) {
        const bool threat = has(Role::Smiler) || has(Role::Hound);
        const world::Tile* pt = c.world->tileIfLoaded(pcx, pcy);
        const bool playerDark = darkTile(c, pt, (float)p.x, (float)p.y);
        if (forced_ >= 0) {
            const Role r = (Role)forced_;
            const double fd = forcedDist_;
            forced_ = -1;
            if (r == Role::Smiler && c.region == 1 && c.canBlackout && c.room && pendingSmiler_ < 0.0) {
                // V11 (ENTITY_BEHAVIOR.md 2.1): im Tiefgeschoss nur im Stromausfall - erst die Welle, dann er
                out.push_back({Event::Blackout, Role::Smiler, c.view.eye, (float)rng_.uniform(c.blackoutLen0, c.blackoutLen1)});
                pendingSmiler_ = rng_.uniform(7.0, 10.0);
                smilerTries_ = 6;
            } else if (r == Role::Smiler) {
                const size_t before = agents_.size();
                spawnSmiler(c, out, fd, false);
                if (agents_.size() == before && c.canBlackout && c.room && pendingSmiler_ < 0.0) {  // kein Dunkel in der Naehe: das Licht faellt aus
                    out.push_back({Event::Blackout, Role::Smiler, c.view.eye, 30.0f});
                    pendingSmiler_ = 7.0;
                    smilerTries_ = 6;
                }
            }
            else if (r == Role::Hound) spawnHound(c, out, fd);
            else if (r == Role::Gestalt) spawnGestalt(c, out, fd);
            else if (r == Role::Clump) {
                if (!spawnClump(c, out, true)) log::info("Praesenz: erzwungenes Buendel nicht moeglich (kein Lager)");
            } else if (r == Role::Window && windowL2(c)) {  // V13.3 Teil 4: Level 2 - ein Fenster an einem Gangende
                if (!spawnWindowL2(c, out, true)) log::info("Praesenz: erzwungenes Fenster nicht moeglich (kein Gangende)");
            } else if (r == Role::Moth && mothL2(c)) {  // V13.3 Teil 4: Level 2 - Motten an einer Leuchte
                if (!spawnMothL2(c, out, true)) log::info("Praesenz: erzwungene Motten nicht moeglich (keine brennende Leuchte)");
            } else if (r == Role::Window) {  // V13.2: Fenster der Ebene (neu verteilt)
                windowsPlaced_ = false;
            } else if (r == Role::Duller) {  // V13.3: gleich an eine Wand neben dem Spieler (Skripte, Tests)
                if (!spawnDuller(c, out, true)) log::info("Praesenz: erzwungener Duller nicht moeglich (keine Wand mit Gang dahinter)");
            } else if (r == Role::Arms) {
                if (!spawnArms(c, out, true)) log::info("Praesenz: erzwungene Arms nicht moeglich (keine Decke in Reichweite)");
            } else if (r == Role::Wrangler) {  // V13.3: Maennchen (ausser die Ebene kennt nur Weibchen)
                const bool fem = c.kin && c.kin[world::KIN_WRANGLER].on && c.kin[world::KIN_WRANGLER].mode == "female";
                if (!spawnWrangler(c, out, fem ? 1 : 0)) log::info("Praesenz: erzwungener Wrangler nicht moeglich");
            } else if (r == Role::Growler || r == Role::Nguithr) {  // V13.3: eigene Auftritte (presence_growler/_nguithrxurh.cpp)
                if (!(r == Role::Growler ? spawnGrowler(c, out, true) : spawnNguithr(c, out, true))) log::info("Praesenz: erzwungenes {} nicht moeglich", roleName(r));
            } else if (r == Role::Ouroboros) {  // V13.3: die Baustelle im Sektor Ouroboros (gleich, ohne Sichtpruefung)
                if (!spawnOuroboros(c, out, true)) log::info("Praesenz: erzwungene Ouroboros-Baustelle nicht moeglich (kein Sektor Ouroboros geladen)");
            } else if (r == Role::Goblin) {  // V13.3: eine Bande Plague Goblins (Portal ausser Sicht)
                if (!spawnGoblin(c, out, true)) log::info("Praesenz: erzwungene Plague Goblins nicht moeglich (kein Platz fuer das Portal)");
            } else if (r >= Role::Faceling) {  // V13.2: wie die Ebene sie kennt; kennt sie es nicht, die Grundart
                static std::array<world::KinDef, world::KIN_COUNT> base;
                Context x = c;
                if (!c.kin || !c.kin[(size_t)r - (size_t)Role::Faceling].on) {
                    if (c.kin) std::copy(c.kin, c.kin + world::KIN_COUNT, base.begin());
                    base[(size_t)r - (size_t)Role::Faceling].on = true;
                    x.kin = base.data();
                }
                if (!spawnKin(r, x, out)) log::info("Praesenz: erzwungenes {} nicht moeglich", roleName(r));
            }
            else if (!hive_.active && !spawnHive(c, fd > 0.0 ? fd : 0.0) && fd <= 0.0)
                log::info("Praesenz: erzwungenes Nest nicht moeglich");
        }
        if (hiveDoorX_ >= 0 && !hive_.active) {  // V13.2 (Skript): Nest an dieser Tuer
            const int hx = hiveDoorX_, hy = hiveDoorY_;
            if (!spawnHive(c, 0.0)) log::warn("Praesenz: kein Nest an der Tuer ({}, {})", hx, hy);
        }
        if (forceEncounter_ && c.canBlackout && c.room && !threat && pendingSmiler_ < 0.0) {  // V11 (Skript)
            out.push_back({Event::Blackout, Role::Smiler, c.view.eye, (float)rng_.uniform(c.blackoutLen0, c.blackoutLen1)});
            pendingSmiler_ = rng_.uniform(7.0, 10.0);
            smilerTries_ = 6;
            forceEncounter_ = false;
            log::info("Praesenz: Begegnung erzwungen (Stromausfall)");
        }
        if (forceBlackout_ && c.canBlackout && c.room) {  // Skript: das Licht faellt aus (ohne Smiler)
            out.push_back({Event::Blackout, Role::Smiler, c.view.eye, 30.0f});
            forceBlackout_ = false;
        }
        // nach einem Stromausfall: im Dunkeln des Raums der Smiler. V11: erst wenn die Welle vorbei ist;
        // findet sich kein Platz (F1: ausser Sicht, mindestens 15 m), noch einige Male versuchen
        if (pendingSmiler_ >= 0.0) {
            pendingSmiler_ -= dt;
            if (pendingSmiler_ < 0.0 && c.view.blackoutSeed >= 0 && !threat) {
                const size_t before = agents_.size();
                spawnSmiler(c, out, 0.0, true);
                if (agents_.size() == before && smilerTries_-- > 0) pendingSmiler_ = 1.5;
            }
        }
        if (!blackoutTimed_ && c.region == 1) {
            blackoutTimed_ = true;
            nextBlackout_ = rng_.uniform(c.blackoutEvery0, c.blackoutEvery1) * 0.6;  // der erste kommt frueher
        }
        if (c.region == 0) {
            // Innenraeume: nur die Gestalt - seltener, solange der Verstand klar ist
            if (!has(Role::Gestalt) && c.allowGestalt) nextGestalt_ -= dt * (0.45 + 1.4 * std::clamp((75.0 - c.sanity) / 55.0, 0.0, 1.0));
            if (!has(Role::Gestalt) && nextGestalt_ <= 0.0) {
                spawnGestalt(c, out, 0.0);
                nextGestalt_ = has(Role::Gestalt) ? rng_.uniform(150.0, 280.0) : 6.0;
            }
        } else if (c.region == 1) {
            // Tiefgeschoss: Stromausfaelle, im Dunkeln der Smiler; der Hound
            if (!threat && pendingSmiler_ < 0.0 && c.allowBlackout) nextBlackout_ -= dt;
            if (nextBlackout_ <= 0.0 && !threat && c.canBlackout && c.room) {
                const float len = (float)rng_.uniform(c.blackoutLen0, c.blackoutLen1);
                out.push_back({Event::Blackout, Role::Smiler, c.view.eye, len});
                pendingSmiler_ = c.allowSmiler ? rng_.uniform(7.0, 10.0) : -1.0;  // V11: nach der Welle
                smilerTries_ = 6;
                nextBlackout_ = rng_.uniform(c.blackoutEvery0, c.blackoutEvery1) * c.stalkerPause;
                log::info("Praesenz: Stromausfall ({:.0f} s)", len);
            } else if (nextBlackout_ <= 0.0) {
                nextBlackout_ = 5.0;  // gleich noch einmal, sobald es passt
            }
            // Dunkelheit (und das Licht der Lampe darin) lockt; sonst seltener
            if (!threat && c.allowHound) nextThreat_ -= dt * (playerDark ? (c.view.flashOn ? 2.0 : 1.4) : 0.7);
            if (!threat && nextThreat_ <= 0.0) {
                const size_t before = agents_.size();
                // V11: das Laecheln kommt hier nur mit dem Stromausfall (oben); sonst der Hund
                spawnHound(c, out, 0.0);
                if (agents_.size() > before) {
                    ++threatsSeen_;
                    nextThreat_ = rng_.uniform(170.0, 290.0) * c.stalkerPause;
                } else {
                    nextThreat_ = 8.0;
                }
            }
        } else {
            // Hotel: Mottennester an dunklen Zimmern, selten ein Hound
            if (!hive_.active) nextHive_ -= dt;
            if (!hive_.active && nextHive_ <= 0.0) nextHive_ = spawnHive(c, 0.0) ? 1e9 : 6.0;
            if (!threat) nextThreat_ -= dt * 0.4;
            if (!threat && nextThreat_ <= 0.0) {
                const size_t before = agents_.size();
                spawnHound(c, out, 0.0);
                nextThreat_ = agents_.size() > before ? rng_.uniform(260.0, 420.0) * c.stalkerPause : 8.0;
            }
        }
    }
    // V13.2: neue Wesen der Ebene (Level-JSON presence.<wesen>): je Wesen eine Gruppe zur Zeit; die Fensterwesen einmal
    // je Ebene in dunklen Fenstern; die Ratten fallen ab und zu irgendwo ueber ein anderes Wesen her (Schrei-Ereignis)
    if (c.mode > 0 && !c.suspended && c.kin) {
        if (!windowsPlaced_) placeWindows(c);
        regieWrangler(dt, c, out);  // V13.3
        regieWindowL2(dt, c, out);  // V13.3 Teil 4: Level 2
        regieMothL2(dt, c, out);
        for (int k = 0; k < world::KIN_WINDOW; ++k) {
            const world::KinDef& K = c.kin[k];
            if (!K.on) continue;
            const Role r = (Role)((int)Role::Faceling + k);
            bool alive = false;
            for (const auto& a : agents_) alive = alive || (a->role == r && !a->showcase);
            if (alive) continue;
            nextKin_[k] -= dt;
            if (nextKin_[k] <= 0.0) nextKin_[k] = spawnKin(r, c, out) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 8.0;
        }
        const world::KinDef& R = c.kin[world::KIN_RAT];
        if (R.on && R.chance > 0.0 && screamT_ <= 0.0) {
            nextScream_ -= dt * R.chance;
            vec3 p;
            if (nextScream_ <= 0.0 && hiddenSpawn(c, 90, 240, false, p)) {
                nextScream_ = rng_.uniform(160.0, 320.0);
                screamT_ = rng_.uniform(3.0, 5.0);
                screamPos_ = p + vec3{0, 0, 0.3f};
                screamTick_ = 0.0;
                out.push_back({Event::Scream, Role::Rat, screamPos_, 1.0f});
                log::info("Praesenz: Schrei-Ereignis der Ratten bei ({:.1f}, {:.1f})", p.x, p.y);
            } else if (nextScream_ <= 0.0) {
                nextScream_ = 10.0;
                log::info("Praesenz: Schrei-Ereignis - kein Ort ausser Sicht, spaeter");
            }
        }
    }
    if (c.mode > 0 && !c.suspended && c.kin) {  // V13.3: Duller (Wandhinterhalt), Arms (Lueftungen im Dunkeln)
        regieDuller(dt, c, out);
        regieArms(dt, c, out);
    }
    if (c.mode > 0 && !c.suspended) regieGrowler(dt, c, out), regieNguithr(dt, c, out);  // V13.3
    if (c.mode > 0 && !c.suspended) regieOuroboros(dt, c, out);  // V13.3: Ouroboros-Baustelle
    if (c.mode > 0 && !c.suspended) regieGoblin(dt, c, out);     // V13.3: Plague Goblins
    if (screamT_ > 0.0) {  // das Licht flackert, bis es vorbei ist
        screamT_ -= dt;
        screamTick_ -= dt;
        if (screamTick_ <= 0.0) screamTick_ = rng_.uniform(0.3, 0.6), out.push_back({Event::Flicker, Role::Rat, screamPos_ + vec3{0, 0, 2.0f}});
    }
    // V11: Buendel - haust in den Deckenschaechten der Karte; hoechstens eins in Hoerweite. Ist es weit weg und
    // lauert, verschwindet es (es wartet anderswo)
    if (c.mode > 0 && !c.suspended && c.allowClump && c.lairs && !c.lairs->empty()) {
        if (!has(Role::Clump)) nextClump_ -= dt;
        if (nextClump_ <= 0.0) {
            if (rng_.uniform(0.0, 1.0) >= c.clumpRate) nextClump_ = rng_.uniform(90.0, 160.0);  // diesmal nicht (Level 1: selten)
            else nextClump_ = spawnClump(c, out, false) ? rng_.uniform(40.0, 90.0) : 6.0;
        }
        std::erase_if(agents_, [&](const auto& a) {
            if (a->role != Role::Clump || a->showcase || a->state != 0) return false;
            const int here = fld().at(ifloor(a->pos.x), ifloor(a->pos.y));
            return here < 0 || here > 900;
        });
    }
}

void Presence::animate(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.frozen) return;
    if (a.role == Role::Nest) {  // V13.2: Saecke atmen, der Schleier schwingt, das Gelege zuckt; Licht und Alarm machen es unruhig
        if (!a.puppet && !a.showcase) {
            a.light = inBeam(c.view, *c.world, a.pos + vec3{0, 0, 1.4f}, 12.0f) ? 1.0f : 0.0f;
            a.alert = hive_.alarmed ? 1.0f : 0.0f;
        }
        if (hdist(a.pos, c.view.eye) > 30.0f && a.rig.posed()) return;  // weit weg: Pose bleibt
        anim::CreatureAnimator::Input in;
        in.pos = a.pos, in.yaw = a.yaw, in.light = a.light, in.alert = a.alert;
        a.cre.update(dt, in);
        a.rig.setCreaturePose(a.cre.pose());
        return;
    }
    // Motten am Ruheplatz und weit weg: Pose bleibt (sie sitzen still)
    if (a.role == Role::Moth && a.state == 0 && a.stateT > 2.0 && !a.showcase) return;
    const bool audible = a.role == Role::Hound || a.role == Role::Smiler || a.role == Role::Clump || a.role == Role::Faceling ||
                         a.role == Role::SkinStealer || a.role == Role::Wretch || a.role == Role::Rat || a.role == Role::Growler ||
                         a.role == Role::Goblin;  // V13.2, V13.3
    if (a.human) {
        anim::Animator::Input in;
        in.pos = a.pos;
        in.yaw = a.yaw;
        in.look = a.look;
        in.lookAt = a.lookAt;
        a.anim.update(dt, in);
        a.rig.setPose(a.anim.pose());
        return;
    }
    anim::CreatureAnimator::Input in;
    in.pos = a.pos;
    in.yaw = a.yaw;
    in.look = a.look;
    in.lookAt = a.lookAt;
    in.tilt = a.tilt;
    in.lean = a.lean;
    in.reach = a.reach;
    in.reachAt = (a.puppet ? a.lookAt : c.view.eye) - vec3{0, 0, a.role == Role::Window ? 0.75f : 0.35f};  // V13.2: Fenster greift nach der Brust
    if (a.role == Role::Wrangler && !a.puppet && !a.showcase) in.reachAt = a.goal;  // V13.3: schnappt nach dem Spieler oder dem Geraeusch
    if (a.role == Role::Goblin) in.reachAt = a.heard;  // V13.3: Plague Goblins - Ware, Boden, Sprungziel (presence_goblin.cpp)
    in.jaw = a.jaw;
    in.alert = a.alert;
    in.crouch = a.crouch;
    in.growl = a.growl;
    in.up = a.up;
    in.fwd = a.fwd;
    in.fly = a.fly;
    in.shiver = a.shiver;
    in.hang = a.hang;
    in.bait = a.bait;
    in.light = a.light, in.perceive = a.perceive, in.perceiveAt = a.perceiveAt;
    in.wave = a.wave, in.talk = a.talk, in.plea = a.plea, in.strike = a.strike;
    in.lightFrom = a.showcase && a.light > 0.0f && length(a.lightFrom) < 1e-3f ? c.view.eye : a.lightFrom;
    if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y))) in.clearance = t->clearance();
    a.cre.update(dt, in);
    a.rig.setCreaturePose(a.cre.pose());
    if (audible && a.cre.stepped()) out.push_back({Event::Step, a.role, a.cre.stepPos(), a.cre.stepWeight() * (a.cre.running() ? 1.4f : 0.8f)});
}

void Presence::update(double dt, const Context& c, std::vector<Event>& out) {
    time_ += dt;
    if (!c.world || !c.player) return;
    preGN(dt, c);  // V13.3
    const Player& p = *c.player;
    if (c.puppet) {  // V13.2 Koop (Gast): nur nachfuehren und bewegen
        for (auto& ap : agents_) {
            Agent& a = *ap;
            a.age += dt;
            a.stateT += dt;
            if (a.puppet) puppetStep(a, c);
            if (a.role == Role::Growler || a.role == Role::Nguithr) prepGN(a, c);  // V13.3: Groesse, Netzkugeln
            if (a.role == Role::Ouroboros) prepOuroboros(a, c);                 // V13.3: Aufgabe, Mauer, Geruest
            if (a.role == Role::Goblin) prepGoblin(a, c);                       // V13.3: Zustand, Groesse, Portal
            if (a.role == Role::Moth && a.variant > 0) prepMothL2(a);           // V13.3 Teil 4: kleiner (Level 2)
            a.sight = perceive(c.view, *c.world, a.feet(), a.height());
            if (a.role == Role::Smiler) {
                const double ph = time_ * 1.35 + 0.4 * std::sin(time_ * 0.31);
                a.rig.setGlow(a.glow * (0.74f + 0.26f * (float)(0.5 + 0.5 * std::sin(ph))) * (1.0f + 0.5f * a.light));
            }
            animate(a, dt, c, out);
        }
        wranglerFeel(dt, c);  // V13.3
        return;
    }
    if (c.mode <= 0 || c.suspended) {
        bool any = false;
        for (auto& a : agents_) any = any || !a->showcase;
        if (any) {
            std::erase_if(agents_, [](const auto& a) { return !a->showcase && a->state != 10; });  // V11: der Toetende bleibt
            hive_ = Hive{};
            pendingSmiler_ = -1.0;
            if (c.suspended) setGrace(45.0);
        }
    }
    // Wegefeld vom Spieler
    fieldT_ -= dt;
    const i64 pcx = ifloor(p.x), pcy = ifloor(p.y);
    if (fieldT_ <= 0.0 || pcx != playerField_.cx() || pcy != playerField_.cy()) {
        playerField_.build(*c.world, pcx, pcy);
        fieldT_ = 0.4;
    }
    // V13.2 Koop: Wegefelder der anderen Spieler (wie das eigene: neu bei jeder Kachel, sonst alle 0,4 s)
    std::erase_if(otherFields_, [&](const OtherField& f) {
        if (!c.others) return true;
        for (const Other& o : *c.others)
            if (o.id == f.id) return false;
        return true;
    });
    if (c.others)
        for (const Other& o : *c.others) {
            OtherField* f = nullptr;
            for (OtherField& q : otherFields_) f = q.id == o.id ? &q : f;
            if (!f) f = &otherFields_.emplace_back(), f->id = o.id;
            f->t -= dt;
            const i64 ox = ifloor(o.player->x), oy = ifloor(o.player->y);
            if (f->t <= 0.0 || ox != f->field.cx() || oy != f->field.cy() || !f->field.valid()) {
                f->field.build(*c.world, ox, oy);
                f->t = 0.4;
            }
        }
    // Ziele: dieser Spieler (falls er lebt) und die anderen
    struct Tgt {
        int id;
        const Player* p;
        const Viewer* v;
        double noise;
        const NavField* f;
        const world::Room* room;
        bool canBlackout;
        const std::vector<std::string>* loot;  // V13.3 Plague Goblins
        bool hatchet;
        double health;
    };
    std::vector<Tgt> tgts;
    if (c.selfAlive) tgts.push_back({c.selfId, c.player, &c.view, c.noise, &playerField_, c.room, c.canBlackout, c.loot, c.hatchet, c.health});
    if (c.others)
        for (const Other& o : *c.others) tgts.push_back({o.id, o.player, &o.view, o.noise, &fieldOf(o.id), o.room, o.canBlackout, o.loot, o.hatchet, o.health});
    auto ctxFor = [&](const Tgt& t) {
        Context x = c;
        x.player = t.p, x.view = *t.v, x.noise = t.noise, x.room = t.room, x.canBlackout = t.canBlackout;
        x.loot = t.loot, x.hatchet = t.hatchet, x.health = t.health;  // V13.3
        x.others = nullptr;
        return x;
    };
    const bool multi = c.others && !c.others->empty();
    // --- Regie: reihum um einen der Spieler (alle 30 s ein anderer) ---------------------------
    if (!tgts.empty()) {
        focusT_ -= dt;
        if (focus_ < 0 || focus_ >= (int)tgts.size() || focusT_ <= 0.0) focus_ = multi ? (int)rng_.randint(0, (i64)tgts.size() - 1) : 0, focusT_ = 30.0;
        const Tgt& F = tgts[(size_t)focus_];
        curField_ = F.f;
        const size_t before = agents_.size();
        if (multi || !c.selfAlive) regie(dt, ctxFor(F), out);
        else regie(dt, c, out);
        // niemand erscheint im Blickfeld irgendeines Spielers (das Nest und seine Motten sitzen im Dunkeln hinter Tueren)
        for (size_t i = before; i < agents_.size();) {
            Agent& a = *agents_[i];
            bool visible = false;
            if (multi && a.role != Role::Nest && (a.role != Role::Moth || a.variant > 0) && a.role != Role::Arms)  // V13.3: Arms im Schacht; Motten in Level 2 an Leuchten
                for (const Tgt& t : tgts) visible = visible || perceive(*t.v, *c.world, a.feet(), a.height()).level > 0.2f;
            if (visible) {
                log::info("Praesenz: {} verworfen - ein Mitspieler saehe es erscheinen", roleName(a.role));
                remove(i);
            } else {
                a.target = F.id;
                ++i;
            }
        }
        curField_ = &playerField_;
    }
    for (auto& ap : agents_)
        if (!ap->netId && !ap->showcase) {
            ap->netId = nextNetId_++;
            if (!nextNetId_) nextNetId_ = 1;
        }
    // Nest: um den Spieler, der ihm am naechsten ist
    if (!tgts.empty()) {
        size_t best = 0;
        for (size_t i = 1; i < tgts.size(); ++i)
            if (hdist(tgts[i].v->eye, hive_.nest) < hdist(tgts[best].v->eye, hive_.nest)) best = i;
        curField_ = tgts[best].f;
        if (multi || !c.selfAlive) updateHive(dt, ctxFor(tgts[best]), out);
        else updateHive(dt, c, out);
        curField_ = &playerField_;
    }
    // --- Entities ------------------------------------------------------------------------------
    for (auto& ap : agents_) {
        Agent& a = *ap;
        a.age += dt;
        const float H = a.height();
        // Ziel: der naechste lebende Spieler (bleibt beim bisherigen, solange es nicht deutlich naehere gibt)
        const Tgt* T = nullptr;
        if (!tgts.empty()) {
            const Tgt* best = &tgts[0];
            const Tgt* cur = nullptr;
            for (const Tgt& t : tgts) {
                if (hdist(t.v->eye, a.pos) < hdist(best->v->eye, a.pos)) best = &t;
                if (t.id == a.target) cur = &t;
            }
            T = cur && hdist(cur->v->eye, a.pos) < hdist(best->v->eye, a.pos) * 1.25f + 0.5f ? cur : best;
        }
        if (!T) {  // niemand lebt: die Wesen halten inne
            animate(a, dt, c, out);
            continue;
        }
        a.target = T->id;
        // V13.2: allein - kein anderer Spieler naeher als 6 m am Ziel (der Skin-Stealer sucht Einzelne)
        a.alone = true;
        for (const Tgt& t : tgts)
            if (&t != T && hdist(t.v->eye, T->v->eye) < 6.0f) a.alone = false;
        const Context ct = multi || !c.selfAlive ? ctxFor(*T) : c;
        curField_ = T->f;
        a.sight = perceive(ct.view, *c.world, a.feet(), H);
        // gesehen: vom Ziel; die Gestalt erstarrt, sobald irgendein Spieler sie sieht
        bool seenAny = false;
        if (multi)
            for (const Tgt& t : tgts) seenAny = seenAny || (t.v == T->v ? a.sight.level : perceive(*t.v, *c.world, a.feet(), H).level) > 0.25f;
        const bool seen = a.role == Role::Smiler ? a.contact : a.sight.level > 0.25f;
        if (seen) {
            a.seenT += dt;
            a.unseenT = 0.0;
            const bool kinThreat = (a.role == Role::Faceling && a.mode >= 2) || a.role == Role::SkinStealer || (a.role == Role::Wretch && !a.pleads) ||
                                   a.role == Role::Duller || (a.role == Role::Wrangler && a.mode != 3);  // V13.3
            if (!a.sighted && !a.showcase && (a.role == Role::Smiler || a.role == Role::Hound || (a.role == Role::Clump && a.state >= 2) || kinThreat) &&
                (a.role == Role::Smiler || a.sight.level > 0.35f)) {
                a.sighted = true;
                ++encounters;
                out.push_back({Event::FirstSight, a.role, a.pos + vec3{0, 0, H * 0.9f}, 1.0f, T->id});
            }
            a.wasSeen = true;
        } else {
            a.unseenT += dt;
            a.seenT = 0.0;
        }
        const size_t e0 = out.size();
        think(a, dt, ct, out);
        for (size_t i = e0; i < out.size(); ++i) out[i].target = T->id;
        curField_ = &playerField_;
        // V11: das Leuchten pulsiert wie Atem (langsam, nicht ganz gleichmaessig)
        if (a.role == Role::Smiler) {
            const double ph = time_ * 1.35 + 0.4 * std::sin(time_ * 0.31);
            // V13.2: im Lichtkegel werfen die Augen das Licht zurueck (Entity-3-Seite: "reflect light") - heller
            a.rig.setGlow(a.glow * (0.74f + 0.26f * (float)(0.5 + 0.5 * std::sin(ph))) * (1.0f + 0.5f * a.light));
        }
        // die Gestalt veraendert sich nicht, solange man sie sieht (V13.2 Koop: irgendein Spieler)
        a.frozen = a.role == Role::Gestalt && (seen || seenAny) && !a.showcase;  // V13.2: die Vorfuehrung bewegt sich (Bildfolgen)
        if (a.showcase && a.walkShow) {  // Vorfuehrung: im Kreis (3 m Radius)
            const float speed = (float)a.speed, w = speed / 3.0f;
            a.yaw += (float)dt * w;
            a.pos += vec3{std::cos(a.yaw), std::sin(a.yaw), 0.0f} * (float)(dt * speed);
        }
        if (a.showcase && a.strikeLoop) a.strike = (float)std::fmod(a.age, 1.8) / 1.8f;  // V13.2: Schlag in Schleife
        animate(a, dt, ct, out);
    }
    // Abgaenge
    for (size_t i = 0; i < agents_.size();) {
        Agent& a = *agents_[i];
        if (a.state == 9 && !a.showcase) {
            if (a.role == Role::Smiler || a.role == Role::Hound)
                nextThreat_ = std::max(nextThreat_, rng_.uniform(150.0, 260.0) * c.stalkerPause * (0.6 + 0.4 * std::clamp(c.sanity / 100.0, 0.0, 1.0)));
            if (a.role == Role::Clump) nextClump_ = std::max(nextClump_, rng_.uniform(60.0, 120.0));
            remove(i);
        } else {
            ++i;
        }
    }
    wranglerFeel(dt, c);  // V13.3: Beben und Sog am eigenen Spieler
}

// --- V13.2: neue Wesen (ENTITY_ABGLEICH.md; Verhalten je Wesen: KREATUR_<Name>.md) -----------------------------------
// Auftritte nach der Ebene (Level-JSON presence.<wesen>), ausser Sicht, in den erlaubten Raumtypen. Gemeinsam: sie
// gehen umher (wander), sehen (Blickfeld, Licht, Abstand) und hoeren (Rennen, Gehen). Was sie dann tun, folgt der
// jeweiligen Wikidot-Seite; jede Gefahr kuendigt sich an (Laut, Haltung) und hat einen Ausweg.
const NavField& Presence::fieldOf(int id) const {
    for (const OtherField& f : otherFields_)
        if (f.id == id) return f.field;
    return playerField_;
}

void Presence::netStates(std::vector<NetState>& out, vec3 near, size_t max) const {
    std::vector<const Agent*> list;
    for (const auto& a : agents_)
        if (!a->showcase && a->netId) list.push_back(a.get());
    std::sort(list.begin(), list.end(), [&](const Agent* a, const Agent* b) { return length(a->pos - near) < length(b->pos - near); });
    if (list.size() > max) list.resize(max);
    for (const Agent* a : list) {
        NetState n;
        n.netId = a->netId;
        n.role = (int)a->role;
        n.variant = a->variant;
        n.pos = a->pos;
        n.yaw = a->yaw;
        n.state = std::clamp(a->state, 0, 15);
        n.target = a->target;
        n.look = a->look;
        n.lit = a->light > 0.5f;
        n.frozen = a->frozen;
        n.jaw = a->jaw, n.alert = a->alert, n.crouch = a->crouch, n.growl = a->growl, n.reach = a->reach, n.fly = a->fly;
        n.shiver = a->shiver, n.hang = a->hang, n.bait = a->bait, n.light = a->light, n.perceive = a->perceive, n.glow = a->glow;
        n.tilt = a->tilt, n.lean = a->lean;
        n.wave = a->wave, n.talk = a->talk, n.plea = a->plea, n.strike = a->strike;
        n.up = a->up, n.fwd = length(a->fwd) > 0.1f ? a->fwd : dirYaw(a->yaw);
        out.push_back(n);
    }
}

void Presence::netApply(const std::vector<NetState>& in, double stamp) {
    // fort, was der Host nicht mehr meldet (ausser Vorfuehrungen)
    std::erase_if(agents_, [&](const auto& a) {
        if (!a->puppet) return false;
        for (const NetState& n : in)
            if (n.netId == a->netId) return false;
        return true;
    });
    for (const NetState& n : in) {
        Agent* a = nullptr;
        for (auto& q : agents_)
            if (q->puppet && q->netId == n.netId) a = q.get();
        if (n.role < 0 || n.role >= kRoleCount) continue;  // V13.3: Rolle unbekannt (Protokoll traegt bis kRoles): nicht zeigen
        const Role role = (Role)n.role;
        if (a && (a->role != role || a->variant != n.variant)) {  // Kennung neu vergeben: neu anlegen
            std::erase_if(agents_, [&](const auto& q) { return q.get() == a; });
            a = nullptr;
        }
        if (!a) {
            auto na = std::make_unique<Agent>();
            na->role = role;
            na->ent = (Entity)role;  // gleiche Reihenfolge
            na->puppet = true;
            na->netId = n.netId;
            na->variant = n.variant;
            na->noShadow = role == Role::Nest || role == Role::Moth || role == Role::Gestalt || role == Role::Clump || role == Role::Window || role == Role::Arms;
            na->up = n.up, na->fwd = n.fwd;
            na->initBody(n.pos, n.yaw, 0x9E37u + n.netId);
            if (role == Role::Moth) {  // gleich fertig an Wand oder Decke (wie beim Host)
                anim::CreatureAnimator::Input ci;
                ci.pos = n.pos, ci.yaw = n.yaw, ci.up = n.up, ci.fwd = n.fwd;
                for (int it = 0; it < 30; ++it) na->cre.update(0.1, ci);
                na->rig.setCreaturePose(na->cre.pose());
            }
            a = na.get();
            agents_.push_back(std::move(na));
        }
        // Probe merken (aelteste faellt heraus)
        if (a->nSamples == (int)a->samples.size()) {
            for (size_t i = 1; i < a->samples.size(); ++i) a->samples[i - 1] = a->samples[i];
            --a->nSamples;
        }
        a->samples[(size_t)a->nSamples++] = {stamp, n.pos, n.yaw};
        if (n.state != a->state) a->stateT = 0.0;
        a->state = n.state;
        a->target = n.target;
        a->look = n.look;
        a->netLit = n.lit;
        a->frozen = n.frozen;
        a->jaw = n.jaw, a->alert = n.alert, a->crouch = n.crouch, a->growl = n.growl, a->reach = n.reach, a->fly = n.fly;
        a->shiver = n.shiver, a->hang = n.hang, a->bait = n.bait, a->light = n.light, a->perceive = n.perceive, a->glow = n.glow;
        a->tilt = n.tilt, a->lean = n.lean;
        a->wave = n.wave, a->talk = n.talk, a->plea = n.plea, a->strike = n.strike;
        a->up = n.up, a->fwd = n.fwd;
        if (a->role == Role::Hound || a->role == Role::Wrangler) a->rig.setGlow(1.0f);  // V13.3: Augen des Wranglers
    }
}

// Puppe: 100 ms in der Vergangenheit zwischen zwei Proben des Hosts; Blick und Licht zum Spieler, auf den sie sich richtet
void Presence::puppetStep(Agent& a, const Context& c) {
    if (a.nSamples > 0) {
        const double t = time_ - 0.1;
        const Agent::Sample* s0 = &a.samples[0];
        const Agent::Sample* s1 = s0;
        for (int i = 0; i < a.nSamples; ++i) {
            if (a.samples[(size_t)i].t <= t) s0 = &a.samples[(size_t)i];
            if (a.samples[(size_t)i].t >= t) {
                s1 = &a.samples[(size_t)i];
                break;
            }
            s1 = &a.samples[(size_t)i];
        }
        const double span = s1->t - s0->t;
        const float k = span > 1e-4 ? (float)std::clamp((t - s0->t) / span, 0.0, 1.0) : 1.0f;
        a.pos = s0->p + (s1->p - s0->p) * k;
        a.yaw = s0->yaw + wrapA(s1->yaw - s0->yaw) * k;
    }
    // wohin es sieht: das Auge des Ziels (dieser Rechner oder ein Mitspieler)
    vec3 eye = c.view.eye, flash = c.view.flashPos;
    if (a.target != c.selfId && c.others)
        for (const Other& o : *c.others)
            if (o.id == a.target) eye = o.view.eye, flash = o.view.flashOn ? o.view.flashPos : o.view.eye;
    a.lookAt = eye;
    a.perceiveAt = eye;
    a.lightFrom = flash;
    // V13.3: ein Smiler am Leuchtkoeder (Kanal bait) sieht zum naechsten Stab (KREATUR_Smiler.md)
    if (a.role == Role::Smiler && a.bait > 0.5f && c.lures) {
        float bd = 5.0f;
        for (const Lure& L : *c.lures)
            if (hdist(L.pos, a.pos) < bd) bd = hdist(L.pos, a.pos), a.lookAt = a.perceiveAt = L.pos;
    }
}

void Presence::collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail) {
    for (auto& a : agents_) a->rig.collect(out, palette, eye, detail, !a->frozen, false, false, a->material);
}

void Presence::shadows(std::vector<ShadowCaster>& out) const {
    for (const auto& a : agents_) {
        if (a->noShadow) continue;
        ShadowCaster s;
        a->rig.shadow(s);
        out.push_back(std::move(s));
    }
}

}  // namespace lim::game
