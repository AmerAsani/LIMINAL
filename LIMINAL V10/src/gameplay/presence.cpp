#include "gameplay/presence.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/log.hpp"
#include "world/room.hpp"
#include "world/world.hpp"

namespace lim::game {

using anim::Entity;
using anim::Figure;

namespace {
constexpr float kPiF = 3.14159265f;
float wrapA(float a) { return std::remainder(a, 2.0f * kPiF); }
float hdist(vec3 a, vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }
vec3 flat(vec3 v) { return {v.x, v.y, 0.0f}; }
vec3 nrmF(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-5f ? v / l : fb;
}
vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }

// dunkel: kein Raumlicht (Leuchten aus, dunkler Raum oder Stromausfall) - die Taschenlampe zaehlt nicht
bool darkTile(const Presence::Context& c, const world::Tile* t) {
    if (!t) return false;
    if (t->light < 0.13f) return true;
    return c.view.blackoutSeed >= 0 && c.view.blackoutAmount > 0.5f && t->room && (int)(t->room->seed & 31u) == c.view.blackoutSeed;
}
// trifft der Lichtkegel der Taschenlampe den Punkt?
bool inBeam(const Viewer& v, world::World& w, vec3 p, float range) {
    if (!v.flashOn) return false;
    vec3 d = p - v.flashPos;
    const float len = length(d);
    return len < range && len > 0.05f && dot(d / len, v.flashDir) > 0.9f && clearSight(w, v.flashPos, p);
}
}  // namespace

struct Presence::Agent {
    Role role = Role::Smiler;
    bool human = false;
    Entity ent = Entity::Smiler;
    anim::CreatureAnimator cre;
    anim::Animator anim;
    FigureRig rig;
    int material = -1;
    vec3 pos, vel;
    float yaw = 0.0f;
    double age = 0.0, unseenT = 0.0, seenT = 0.0, stateT = 0.0, timer = 0.0, timer2 = 0.0, lostT = 0.0, stareT = 0.0, cool = 0.0;
    int state = 0, stage = 0;
    bool showcase = false, walkShow = false, sighted = false, frozen = false, noShadow = false, wasSeen = false, contact = false;
    bool stared = false;
    Sight sight;
    NavField field;
    vec3 goal;
    bool hasGoal = false;
    double speed = 1.0;
    float glow = 0.0f;
    // Eingaben der Animation
    bool look = false;
    vec3 lookAt;
    float tilt = 0.0f, lean = 0.0f, reach = 0.0f, jaw = 0.0f, alert = 0.0f, crouch = 0.0f, growl = 0.0f, fly = 0.0f, shiver = 0.0f;
    vec3 up{0, 0, 1}, fwd{};
    // Deathmoth: Ruheplatz an Wand oder Decke, Kreisbahn um das Licht
    vec3 rest, restN{0, 0, 1}, restF{1, 0, 0};
    float phase = 0.0f, radius = 0.6f, bob = 0.0f, spin = 1.0f;
    // Hound: was er gehoert hat
    vec3 heard;
    float stareLimit = 7.0f;

    void initBody(vec3 p, float y, unsigned seed) {
        pos = p, yaw = y;
        if (human) {
            anim.init(Figure::Player, p, y);
            rig.init(Figure::Player);
            rig.setPose(anim.pose());
        } else {
            cre.init(ent, p, y, seed);
            rig.initEntity(ent);
            rig.setCreaturePose(cre.pose());
        }
    }
    void snapBody(vec3 p, float y) {
        pos = p, yaw = y;
        if (human) {
            anim.snap(p, y);
            rig.setPose(anim.pose());
        } else {
            cre.snap(p, y);
            rig.setCreaturePose(cre.pose());
        }
    }
    float height() const { return human ? anim::figureBuild(Figure::Player).height : cre.height(); }
    // Fuesse fuer die Wahrnehmung (die Motte fliegt: Mitte des Koerpers)
    vec3 feet() const { return ent == Entity::Moth && !human ? pos - vec3{0, 0, 0.06f} : pos; }
};

const char* Presence::roleName(Role r) {
    switch (r) {
        case Role::Smiler: return "Smiler";
        case Role::Hound: return "Hound";
        case Role::Moth: return "Deathmoth";
        case Role::Gestalt: return "Gestalt";
        case Role::Nest: return "Nest";
    }
    return "?";
}

Presence::Presence() = default;
Presence::~Presence() = default;

void Presence::clear() {
    agents_.clear();
    hive_ = Hive{};
    pendingSmiler_ = -1.0;
}

void Presence::setGrace(double s) {
    nextThreat_ = std::max(nextThreat_, s);
    nextBlackout_ = std::max(nextBlackout_, s * 0.8);
    nextHive_ = std::max(nextHive_, s * 0.4);
    nextGestalt_ = std::max(nextGestalt_, s * 0.6);
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
bool Presence::threatActive() const { return has(Role::Smiler) || has(Role::Hound); }
bool Presence::hunting() const {
    for (const auto& a : agents_) {
        if (a->showcase) continue;
        if (a->role == Role::Hound && (a->state == 2 || a->state == 3)) return true;
        if (a->role == Role::Smiler && a->state == 1 && a->sight.dist < 14.0f) return true;
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
    }
    return p;
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
bool Presence::hiddenSpawn(const Context& c, int lo, int hi, bool behind, vec3& out) {
    if (!playerField_.valid()) return false;
    std::vector<std::pair<i64, i64>> cand;
    playerField_.ring(lo, hi, cand);
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
    playerField_.ring(lo, hi, cand);
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
    if (!playerField_.valid()) return;
    std::vector<std::pair<i64, i64>> cand;
    const int lo = dist > 0.0 ? (int)(dist * 8.5) : (blackout ? 55 : 80), hi = dist > 0.0 ? (int)(dist * 12.0) : (blackout ? 160 : 190);
    playerField_.ring(lo, hi, cand);
    const vec3 eye = c.view.eye;
    float bestScore = 1e9f;
    vec3 best;
    for (int tries = 0; tries < 220 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || t->clearance() < 2.1f || (t->flags & world::TF_STAIR) || !darkTile(c, t)) continue;
        if (blackout && c.room && t->room != c.room) continue;
        const vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        const vec3 face = p + vec3{0.1f, 0, 1.72f};
        const vec3 dd = nrmF(face - eye, {1, 0, 0});
        if (dot(dd, c.view.look) > 0.55f) continue;  // nie im Blickfeld erscheinen
        const bool los = clearSight(*c.world, eye, face);
        float score = std::fabs(hdist(p, eye) - (dist > 0.0 ? (float)dist : 10.0f)) + (los ? -3.0f : 0.0f) + (float)rng_.uniform(0.0, 2.0);
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
    if (!playerField_.valid() || hive_.active) return false;
    std::vector<std::pair<i64, i64>> cand;
    playerField_.ring(dist > 0.0 ? (int)(dist * 8.0) : 110, dist > 0.0 ? (int)(dist * 13.0) : 330, cand);
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
            if (!f || !f->open() || !f->room || f->light > 0.35f || f->clearance() < 2.6f || f->room->avgLight > 0.2) {
                ++nDark;
                continue;
            }
            root.z = f->floor;
            if (hdist(root, eye) < 8.0f) {
                ++nNear;
                continue;
            }
            const vec3 front = root + dirYaw(yaw) * 0.2f + vec3{0, 0, 1.2f};
            if (dot(nrmF(front - eye, {1, 0, 0}), c.view.look) > 0.45f && clearSight(*c.world, eye, front)) continue;  // nicht vor deinen Augen
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
    if (dist > 0.0 || forced_ >= 0)
        log::info("Praesenz: kein Nest - {} Tueren im Umkreis ({} falsche Art, {} Seiten nicht dunkel, {} zu nah, {} ohne Platz; dunkelste Seite {:.2f} / Raum {:.2f})",
                  doors.size(), nKind, nDark, nNear, nSpots, minLight, minAvg);
    return false;
}

void Presence::spawnGestalt(const Context& c, std::vector<Event>& out, double dist) {
    if (!playerField_.valid()) return;
    std::vector<std::pair<i64, i64>> cand;
    playerField_.ring(dist > 0.0 ? (int)(dist * 9.0) : 150, dist > 0.0 ? (int)(dist * 12.0) : 290, cand);
    const vec3 eye = c.view.eye;
    float bestScore = 1e9f;
    vec3 best;
    for (int tries = 0; tries < 200 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || t->clearance() < 2.1f || (t->flags & world::TF_STAIR) || t->light < 0.25f || darkTile(c, t)) continue;
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
    a->role = kind == 1 ? Role::Hound : (kind == 2 ? Role::Moth : (kind == 3 ? Role::Gestalt : (kind == 4 ? Role::Nest : Role::Smiler)));
    a->human = kind >= 5;
    a->ent = (Entity)std::clamp(kind, 0, anim::kEntityCount - 1);
    a->showcase = true;
    a->walkShow = walkSpeed > 0.0f;
    a->speed = walkSpeed;
    a->glow = 1.0f;
    a->look = kind == 0;
    if (a->ent == Entity::Moth) pos.z += 1.3f;
    a->initBody(pos, yaw, (unsigned)rng_.randint(1, 1 << 30));
    a->rig.setGlow(1.0f);
    if (a->ent == Entity::Moth && a->walkShow) a->fly = 1.0f;
    agents_.push_back(std::move(a));
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
void Presence::thinkSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
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
    const world::Tile* here = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y));
    const bool dark = darkTile(c, here);
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
    const world::Tile* pt = c.world->tileIfLoaded(ifloor(c.player->x), ifloor(c.player->y));
    const bool playerInLight = pt && !darkTile(c, pt);
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
    // Panik: wer in seiner Naehe rennt, wird angegriffen (Wikidot)
    if (c.player->sprinting && d < 12.0f && a.stage < 2) {
        a.stage = 2;
        log::info("Praesenz: Smiler - Panik, er greift an");
    }
    // Blickkontakt: er ruehrt sich nicht (ausser im Angriff)
    if (a.contact && a.stage < 2) return;
    // Angriff
    if (d < 1.05f && std::fabs(a.pos.z - (float)c.player->z) < 1.4f) {
        if (c.mode >= 2) out.push_back({Event::Attack, Role::Smiler, face, 1.5f});
        else out.push_back({Event::Flicker, Role::Smiler, face});
        out.push_back({Event::Gone, Role::Smiler, face});
        a.state = 9;
        return;
    }
    // naeher - nur durch das Dunkel: am Rand des Lichts bleibt er stehen
    double tx, ty;
    if (!playerField_.next(a.pos.x, a.pos.y, tx, ty)) return;
    const world::Tile* nt = c.world->tileIfLoaded(ifloor(tx), ifloor(ty));
    const bool nextDark = darkTile(c, nt) || hdist(vec3{(float)tx, (float)ty, 0.0f}, eye) < 1.2f;
    if (!nextDark && a.stage < 2) return;
    const double speed = a.stage >= 2 ? 4.2 : 1.25;
    walkTo(a, playerField_, speed, dt, c);
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
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    const bool playerLit = c.view.flashOn || litPoint(c.view, *c.world, eye - vec3{0, 0, 0.6f});
    const float seeRange = c.view.flashOn ? 32.0f : (playerLit ? 22.0f : 7.0f);  // das Licht zieht ihn an (Fandom)
    const bool sees = los && d < seeRange && (facing > 0.15f || d < 5.0f);
    const int here = playerField_.at(ifloor(a.pos.x), ifloor(a.pos.y));
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
                    walkTo(a, playerField_, 0.45, dt, c);  // der Blick wirkt nicht mehr - er kommt wieder
                }
            } else {
                a.stareT = std::max(0.0, a.stareT - dt * 0.3);
                a.timer += dt;
                if (d > 4.0f) walkTo(a, playerField_, 0.8, dt, c);
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
                if (c.mode >= 2) out.push_back({Event::Attack, Role::Hound, head, 1.0f});
                a.cool = 8.0;
                a.jaw = 1.0f;
                setState(4);
                pickGoal(c, a, 230, 380, true);
                break;
            }
            if (!walkTo(a, playerField_, speed, dt, c) || a.stateT > 6.0) setState(2), a.cool = 3.0;
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
            tgt = steer(tgt, hive_.hasTarget && c.view.flashOn ? playerField_ : hive_.home);
            if (a.state == 3) {  // stoesst zu
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
                    if (!t || !(t->flags & world::TF_LAMP) || darkTile(c, t)) continue;
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
        return;
    }
    switch (a.role) {
        case Role::Smiler: thinkSmiler(a, dt, c, out); break;
        case Role::Hound: thinkHound(a, dt, c, out); break;
        case Role::Moth: thinkMoth(a, dt, c, out); break;
        case Role::Gestalt: thinkGestalt(a, dt, c, out); break;
        default: break;
    }
}

// --- Ablauf ------------------------------------------------------------------------------------
void Presence::update(double dt, const Context& c, std::vector<Event>& out) {
    time_ += dt;
    if (!c.world || !c.player) return;
    const Player& p = *c.player;
    if (c.mode <= 0 || c.suspended) {
        bool any = false;
        for (auto& a : agents_) any = any || !a->showcase;
        if (any) {
            std::erase_if(agents_, [](const auto& a) { return !a->showcase; });
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
    // --- Regie ---------------------------------------------------------------------------------
    if (c.mode > 0 && !c.suspended) {
        const bool threat = has(Role::Smiler) || has(Role::Hound);
        const world::Tile* pt = c.world->tileIfLoaded(pcx, pcy);
        const bool playerDark = darkTile(c, pt);
        if (forced_ >= 0) {
            const Role r = (Role)forced_;
            const double fd = forcedDist_;
            forced_ = -1;
            if (r == Role::Smiler) {
                const size_t before = agents_.size();
                spawnSmiler(c, out, fd, false);
                if (agents_.size() == before && c.canBlackout && c.room && pendingSmiler_ < 0.0) {  // kein Dunkel in der Naehe: das Licht faellt aus
                    out.push_back({Event::Blackout, Role::Smiler, c.view.eye, 30.0f});
                    pendingSmiler_ = 2.5;
                }
            }
            else if (r == Role::Hound) spawnHound(c, out, fd);
            else if (r == Role::Gestalt) spawnGestalt(c, out, fd);
            else if (!hive_.active && !spawnHive(c, fd > 0.0 ? fd : 0.0) && fd <= 0.0)
                log::info("Praesenz: erzwungenes Nest nicht moeglich");
        }
        if (forceBlackout_ && c.canBlackout && c.room) {  // Skript: das Licht faellt aus (ohne Smiler)
            out.push_back({Event::Blackout, Role::Smiler, c.view.eye, 30.0f});
            forceBlackout_ = false;
        }
        // nach einem Stromausfall: im Dunkeln des Raums der Smiler
        if (pendingSmiler_ >= 0.0) {
            pendingSmiler_ -= dt;
            if (pendingSmiler_ < 0.0 && c.view.blackoutSeed >= 0 && !threat) spawnSmiler(c, out, 0.0, true);
        }
        if (c.region == 0) {
            // Innenraeume: nur die Gestalt - seltener, solange der Verstand klar ist
            if (!has(Role::Gestalt)) nextGestalt_ -= dt * (0.45 + 1.4 * std::clamp((75.0 - c.sanity) / 55.0, 0.0, 1.0));
            if (!has(Role::Gestalt) && nextGestalt_ <= 0.0) {
                spawnGestalt(c, out, 0.0);
                nextGestalt_ = has(Role::Gestalt) ? rng_.uniform(150.0, 280.0) : 6.0;
            }
        } else if (c.region == 1) {
            // Tiefgeschoss: Stromausfaelle, im Dunkeln der Smiler; der Hound
            if (!threat && pendingSmiler_ < 0.0) nextBlackout_ -= dt;
            if (nextBlackout_ <= 0.0 && !threat && c.canBlackout && c.room) {
                const float len = (float)rng_.uniform(20.0, 32.0);
                out.push_back({Event::Blackout, Role::Smiler, c.view.eye, len});
                pendingSmiler_ = rng_.uniform(2.5, 4.0);
                nextBlackout_ = rng_.uniform(200.0, 340.0) * c.stalkerPause;
                log::info("Praesenz: Stromausfall ({:.0f} s)", len);
            } else if (nextBlackout_ <= 0.0) {
                nextBlackout_ = 5.0;  // gleich noch einmal, sobald es passt
            }
            // Dunkelheit (und das Licht der Lampe darin) lockt; sonst seltener
            if (!threat) nextThreat_ -= dt * (playerDark ? (c.view.flashOn ? 2.0 : 1.4) : 0.7);
            if (!threat && nextThreat_ <= 0.0) {
                const size_t before = agents_.size();
                if (playerDark || (threatsSeen_ % 2 == 1)) spawnSmiler(c, out, 0.0, false);
                if (agents_.size() == before) spawnHound(c, out, 0.0);
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
    updateHive(dt, c, out);
    // --- Entities ------------------------------------------------------------------------------
    for (auto& ap : agents_) {
        Agent& a = *ap;
        a.age += dt;
        const float H = a.height();
        a.sight = perceive(c.view, *c.world, a.feet(), H);
        const bool seen = a.role == Role::Smiler ? a.contact : a.sight.level > 0.25f;
        if (seen) {
            a.seenT += dt;
            a.unseenT = 0.0;
            if (!a.sighted && !a.showcase && (a.role == Role::Smiler || a.role == Role::Hound) && (a.role == Role::Smiler || a.sight.level > 0.35f)) {
                a.sighted = true;
                ++encounters;
                out.push_back({Event::FirstSight, a.role, a.pos + vec3{0, 0, H * 0.9f}});
            }
            a.wasSeen = true;
        } else {
            a.unseenT += dt;
            a.seenT = 0.0;
        }
        think(a, dt, c, out);
        if (a.role == Role::Smiler) a.rig.setGlow(a.glow);
        // die Gestalt veraendert sich nicht, solange man sie sieht
        a.frozen = a.role == Role::Gestalt && seen;
        if (a.showcase && a.walkShow) {  // Vorfuehrung: im Kreis (3 m Radius)
            const float speed = (float)a.speed, w = speed / 3.0f;
            a.yaw += (float)dt * w;
            a.pos += vec3{std::cos(a.yaw), std::sin(a.yaw), 0.0f} * (float)(dt * speed);
        }
        if (a.frozen || a.role == Role::Nest) {
            if (a.role == Role::Nest && !a.rig.posed()) a.rig.setCreaturePose(a.cre.pose());
            continue;
        }
        // Motten am Ruheplatz und weit weg: Pose bleibt (sie sitzen still)
        if (a.role == Role::Moth && a.state == 0 && a.stateT > 2.0 && !a.showcase) continue;
        const bool audible = a.role == Role::Hound || a.role == Role::Smiler;
        if (a.human) {
            anim::Animator::Input in;
            in.pos = a.pos;
            in.yaw = a.yaw;
            in.look = a.look;
            in.lookAt = a.lookAt;
            a.anim.update(dt, in);
            a.rig.setPose(a.anim.pose());
            continue;
        }
        anim::CreatureAnimator::Input in;
        in.pos = a.pos;
        in.yaw = a.yaw;
        in.look = a.look;
        in.lookAt = a.lookAt;
        in.tilt = a.tilt;
        in.lean = a.lean;
        in.reach = a.reach;
        in.reachAt = c.view.eye - vec3{0, 0, 0.35f};
        in.jaw = a.jaw;
        in.alert = a.alert;
        in.crouch = a.crouch;
        in.growl = a.growl;
        in.up = a.up;
        in.fwd = a.fwd;
        in.fly = a.fly;
        in.shiver = a.shiver;
        if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y))) in.clearance = t->clearance();
        a.cre.update(dt, in);
        a.rig.setCreaturePose(a.cre.pose());
        if (audible && a.cre.stepped()) out.push_back({Event::Step, a.role, a.cre.stepPos(), a.cre.stepWeight() * (a.cre.running() ? 1.4f : 0.8f)});
    }
    // Abgaenge
    for (size_t i = 0; i < agents_.size();) {
        Agent& a = *agents_[i];
        if (a.state == 9 && !a.showcase) {
            if (a.role == Role::Smiler || a.role == Role::Hound)
                nextThreat_ = std::max(nextThreat_, rng_.uniform(150.0, 260.0) * c.stalkerPause * (0.6 + 0.4 * std::clamp(c.sanity / 100.0, 0.0, 1.0)));
            remove(i);
        } else {
            ++i;
        }
    }
}

void Presence::collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail) {
    for (auto& a : agents_) a->rig.collect(out, palette, eye, detail, !(a->frozen || a->role == Role::Nest), false, false, a->material);
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
