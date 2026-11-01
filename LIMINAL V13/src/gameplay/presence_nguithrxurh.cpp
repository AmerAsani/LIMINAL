// V13.3: Nguithr'xurh (Entity 16, Wikidot entity-16; KREATUR_Nguithrxurh.md) - Auftreten und Verhalten.
//
// Eine Kolonie ("Familie"): an der Decke eines Gangs haengen bis zu sechs Netzkugeln mit Betaeubungsstoff, darum sitzen zwei
// bis vier Walzenspinnen reglos. Sie jagen nicht hinterher - wer die Netze meidet, ist sicher (die Seite).
//
//   Netz   0 lauert     steht man direkt unter einer Kugel (0,7 s), schneidet eine Spinne sie ab: sie faellt, zerplatzt
//                       (erste Dosis - "man fuehlt sich seltsam")
//          1 wartet     2,8 s: ist man noch da (oder schon benommen in der Naehe), fallen alle uebrigen auf einmal
//          2 alle       die Spinnen seilen sich ab, sobald man benommen ist; nach einer Weile ohne Opfer wachsen neue Kugeln
//   Spinne 0 Decke      reglos (Hinterleib pumpt, Pedipalpen tippen), baut ab und zu (laeuft ein Stueck); im Licht erstarrt sie,
//                       laenger angeleuchtet zirpt sie und huscht ins Dunkel (3)
//          1 abseilen   am Faden hinab, Kopf voran
//          2 kommen     langsam zum Opfer (nur nahe dem Netz)
//          4 am Opfer   kaut; reagiert man nicht mehr (Betaeubung >= 0,8), frisst sie es lebendig (10: toedlich)
//          5/6 zurueck  zum Faden und hinauf - wenn man sich erholt oder sich vom Netz entfernt
// Betaeubung (je Spieler, Presence::stupor): langsamer, kein Sprinten, Bild und Klang gedaempft; klingt nach 6 s ab, schneller
// in Bewegung und mit Spruengen ("den Koerper schuetteln" - die Seite).
// Ebenen (presence.nguithrxurh): mode large (Level 1) / medium (Level 5) / small (Level 2, mit eingesponnenen Deathmoths);
// chance = hungrig (greift Menschen an; Level 2 nur selten).
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr int kS = anim::ng::kSacs;
struct WebAg {
    bool laid = false;
    vec3 anchor[kS]{};
    float hang[kS]{}, floorZ[kS]{}, fall[kS]{};
    bool ok[kS]{};
    int phase = 0, queue = 0;
    double underT = 0.0, waitT = 0.0, regrowT = 0.0, cutT = 0.0, chitT = 4.0;
    float tremble = 0.0f;
};
struct SpAg {
    vec3 home, from, to;
    float scale = 1.0f, moveP = 1.0f, moveDur = 1.0f;
    double moveT = 6.0, lightT = 0.0, skitT = 0.0, delay = 0.0;
    int index = 0;
};
// Vorlagen: Presence::Agent ist privat - so muss der Name hier nicht fallen (wie lightReact)
template <class A>
bool isWeb(const A& a) {
    return a.variant == 1 || a.variant == 2;
}
template <class A>
WebAg& wag(A& a) {
    if (!a.ext) a.ext = std::make_shared<WebAg>();
    return *static_cast<WebAg*>(a.ext.get());
}
template <class A>
SpAg& sag(A& a) {
    if (!a.ext) a.ext = std::make_shared<SpAg>();
    return *static_cast<SpAg*>(a.ext.get());
}
float fallTime(const WebAg& g, int k) {
    const float drop = g.anchor[k].z - g.hang[k] - g.floorZ[k] - anim::ng::kSac[k].r * 0.7f;
    return std::sqrt(std::max(0.0f, 2.0f * drop / 9.8f));
}
std::uint32_t hashU(std::uint32_t a) {
    a ^= a >> 16, a *= 0x7feb352du, a ^= a >> 15, a *= 0x846ca68bu, a ^= a >> 16;
    return a;
}
// Lage der Kugeln: aus der Mitte (Kachel) bestimmt - Host und Gast rechnen dasselbe (Koop ohne Protokollaenderung)
void layWeb(world::World& w, vec3 c, WebAg& g) {
    const std::uint32_t h = hashU((std::uint32_t)ifloor(c.x) * 73856093u ^ (std::uint32_t)ifloor(c.y) * 19349663u);
    const float rot = (float)(h & 1023u) / 1024.0f * 6.2831853f, sc = 0.85f + (float)((h >> 10) & 255u) / 255.0f * 0.3f;
    for (int k = 0; k < kS; ++k) {
        const anim::ng::Sac& S = anim::ng::kSac[k];
        const vec3 off{(S.x * std::cos(rot) - S.y * std::sin(rot)) * sc, (S.x * std::sin(rot) + S.y * std::cos(rot)) * sc, 0.0f};
        g.ok[k] = false;
        for (float shrink : {1.0f, 0.45f}) {
            const vec3 p = c + off * shrink;
            const world::Tile* t = w.tileIfLoaded(ifloor(p.x), ifloor(p.y));
            if (!t || !t->open() || std::fabs(t->ceil - c.z) > 0.3f || t->clearance() < 2.35f) continue;
            g.anchor[k] = {p.x, p.y, t->ceil};
            g.floorZ[k] = t->floor;
            // die Unterkante der Kugel knapp ueber Kopfhoehe (2,0 bis 2,3 m), je Kugel verschieden
            g.hang[k] = std::clamp(t->ceil - t->floor - 2.02f - S.r - 0.16f * (float)(k % 3) + (k == 0 ? 0.1f : 0.0f), 0.15f, 1.3f);
            g.ok[k] = true;
            break;
        }
        g.fall[k] = -1.0f;
    }
    g.laid = true;
}
template <class V>
auto webOfGroup(V& v, int group) -> decltype(v.front().get()) {
    for (auto& o : v)
        if (o->role == Presence::Role::Nguithr && isWeb(*o) && o->group == group && !o->showcase) return o.get();
    return nullptr;
}
}  // namespace

void Presence::addDose(int target, float d) {
    if (target < 0 || target >= (int)stupor_.size()) return;
    stupor_[(size_t)target] = std::min(1.0f, stupor_[(size_t)target] + d);
    stuporHold_[(size_t)target] = 6.0;
}

float Presence::slowFor(int target, vec3 feet) const {
    const float s = stupor(target);
    float k = 1.0f - 0.75f * std::clamp((s - 0.15f) / 0.8f, 0.0f, 1.0f);
    // durch das Gewirr einer Growler-Leiche waten
    for (const auto& a : agents_)
        if (a->role == Role::Growler && a->state == 12 && hdist(a->pos, feet) < 0.95f * std::max(0.6f, a->cre.scale())) k = std::min(k, 0.45f);
    return k;
}

vec3 Presence::spiderOf(int i, int* state) const {
    for (const auto& a : agents_)
        if (a->role == Role::Nguithr && !isWeb(*a) && !a->showcase && i-- == 0) {
            if (state) *state = a->state;
            return a->pos;
        }
    return {};
}

bool Presence::placeAgent(Role r, int i, vec3 p) {
    for (auto& a : agents_)
        if (a->role == r && !a->showcase && i-- == 0) {
            a->snapBody(p, a->yaw);
            return true;
        }
    return false;
}

vec3 Presence::webOf(int i) const {
    for (const auto& a : agents_)
        if (a->role == Role::Nguithr && isWeb(*a) && !a->showcase && i-- == 0) return a->pos;
    return {};
}

void Presence::prepNguithr(Agent& a, const Context& c) {
    if (isWeb(a)) {
        WebAg& g = wag(a);
        if (!g.laid) {
            if (a.showcase || a.puppet) {  // auf die Decke ueber der Stelle
                if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)); t && a.showcase) a.pos.z = t->ceil;
            }
            layWeb(*c.world, a.pos, g);
        }
        // Koop: Fallzeit je Kugel in den Kanaelen jaw alert crouch growl reach fly (0 haengt, sonst (t + 0,1) / 3,1)
        float* ch[kS] = {&a.jaw, &a.alert, &a.crouch, &a.growl, &a.reach, &a.fly};
        for (int k = 0; k < kS; ++k) {
            if (a.puppet) g.fall[k] = *ch[k] > 0.001f ? *ch[k] * 3.1f - 0.1f : -1.0f;
            else *ch[k] = g.fall[k] < 0.0f ? 0.0f : std::clamp((g.fall[k] + 0.1f) / 3.1f, 0.004f, 1.0f);
        }
        if (!a.puppet) a.shiver = g.tremble;
        std::vector<float> ex;
        ex.reserve(2 + kS * 6);
        ex.push_back(1.0f);
        ex.push_back((float)kS);
        for (int k = 0; k < kS; ++k) {
            ex.push_back(g.anchor[k].x), ex.push_back(g.anchor[k].y), ex.push_back(g.anchor[k].z);
            ex.push_back(g.ok[k] ? g.hang[k] : -1.0f);
            ex.push_back(g.floorZ[k]);
            ex.push_back(g.fall[k]);
        }
        a.cre.setExtra(ex);
        a.rig.setGlow(0.0f);
        return;
    }
    // Spinne: Groesse (Koop: Kanal bait), Lage, Augen werfen das Lampenlicht zurueck
    float s;
    if (a.puppet) s = 0.3f + 1.5f * a.bait;
    else if (a.showcase) s = 2.0f;
    else s = sag(a).scale;
    if (!a.puppet) a.bait = std::clamp((s - 0.3f) / 1.5f, 0.0f, 1.0f);
    if (a.showcase && a.variant == 3) {  // Vorfuehrung an der Decke
        if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y))) a.pos.z = t->ceil;
        a.up = {0, 0, -1};
        a.fwd = dirYaw(a.yaw);
    } else if (a.showcase) {
        a.up = {0, 0, 1};
        a.fwd = dirYaw(a.yaw);
    }
    a.cre.setScale(s);
    a.cre.setExtra({0.0f});
    a.rig.setGlow(0.25f + 2.2f * a.light);
}

bool Presence::spawnNguithr(const Context& c, std::vector<Event>& out, bool forced) {
    world::KinDef K;
    K.mode = "large", K.groupMin = 3, K.groupMax = 4, K.chance = 1.0;  // erzwungen ohne Eintrag: wie Level 1
    if (c.kin && c.kin[world::KIN_NGUITHR].on) K = c.kin[world::KIN_NGUITHR];
    else if (!forced) return false;
    if (!fld().valid()) return false;
    const bool small = K.mode == "small";
    const float scale = K.mode == "small" ? 0.95f : (K.mode == "medium" ? 1.5f : 2.0f);  // Runde 1: 1,6 in Level 1 wirkte wie ein Insekt
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(forced ? 40 : 90, forced ? 200 : 260, cand);
    const vec3 eye = c.view.eye;
    float best = 1e9f;
    vec3 site;
    for (int tries = 0; tries < 260 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || !t->open() || (t->flags & world::TF_STAIR) || t->clearance() < 2.45f || t->clearance() > 3.9f) continue;
        if (!roomAllowed(c, {(float)x + 0.5f, (float)y + 0.5f, t->floor}, world::KIN_NGUITHR)) continue;
        const vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->ceil};
        const vec3 low = p - vec3{0, 0, 0.6f};
        if (dot(nrmF(low - eye, {1, 0, 0}), c.view.look) > 0.35f && clearSight(*c.world, eye, low)) continue;  // nicht vor den Augen
        int same = 0, machine = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const world::Tile* n = c.world->tileIfLoaded(x + dx, y + dy);
                if (n && n->open() && std::fabs(n->ceil - t->ceil) < 0.25f) ++same;
                if (n && (n->flags & world::TF_MACHINE)) ++machine;
            }
        if (same < 4) continue;
        float score = (float)rng_.uniform(0.0, 3.0);
        if (t->room && t->room->isCorridor) score -= 2.0f;  // in Gaengen, wo man hindurch muss (Level 1: nie unter Netzen hindurch)
        if (small && machine > 0) score -= 3.0f;            // Level 2: Decke ueber den Maschinen
        if (score < best) best = score, site = p;
    }
    if (best > 1e8f) return false;
    const int group = nextGroup_++;
    auto w = std::make_unique<Agent>();
    w->role = Role::Nguithr;
    w->ent = Entity::Nguithr;
    w->variant = small ? 2 : 1;
    w->group = group;
    w->hungry = rng_.uniform(0.0, 1.0) < K.chance;
    w->initBody(site, (float)rng_.uniform(-kPiF, kPiF), (unsigned)rng_.randint(1, 1 << 30));
    Agent& web = *w;
    agents_.push_back(std::move(w));
    prepNguithr(web, c);
    {
        anim::CreatureAnimator::Input in;
        in.pos = web.pos, in.yaw = web.yaw;
        web.cre.update(0.0, in);
        web.rig.setCreaturePose(web.cre.pose());
    }
    WebAg& g = wag(web);
    int okN = 0;
    for (bool b : g.ok) okN += b ? 1 : 0;
    // die Spinnen sitzen um die Kugeln an der Decke
    const int n = (int)rng_.randint(K.groupMin, K.groupMax);
    int made = 0;
    for (int i = 0; i < n; ++i) {
        vec3 q = site;
        bool found = false;
        for (int t = 0; t < 20 && !found; ++t) {
            const float ang = (float)rng_.uniform(-kPiF, kPiF), r = (float)rng_.uniform(0.25, 0.95);
            const vec3 p2 = site + vec3{std::cos(ang), std::sin(ang), 0.0f} * r;
            const world::Tile* tl = c.world->tileIfLoaded(ifloor(p2.x), ifloor(p2.y));
            if (tl && tl->open() && std::fabs(tl->ceil - site.z) < 0.05f) q = {p2.x, p2.y, tl->ceil}, found = true;
        }
        if (!found) continue;
        auto a = std::make_unique<Agent>();
        a->role = Role::Nguithr;
        a->ent = Entity::Nguithr;
        a->variant = 0;
        a->group = group;
        a->noShadow = true;
        SpAg& sp = sag(*a);
        sp.scale = scale * (float)rng_.uniform(0.85, 1.15);
        sp.home = q;
        sp.index = i;
        sp.moveT = rng_.uniform(4.0, 14.0);
        a->up = {0, 0, -1};
        a->yaw = (float)rng_.uniform(-kPiF, kPiF);
        a->fwd = dirYaw(a->yaw);
        a->initBody(q, a->yaw, (unsigned)rng_.randint(1, 1 << 30));
        prepNguithr(*a, c);
        anim::CreatureAnimator::Input in;  // gleich fertig an der Decke
        in.pos = q, in.yaw = a->yaw, in.up = a->up, in.fwd = a->fwd;
        for (int it = 0; it < 20; ++it) a->cre.update(0.1, in);
        a->rig.setCreaturePose(a->cre.pose());
        agents_.push_back(std::move(a));
        ++made;
    }
    out.push_back({Event::Appear, Role::Nguithr, site});
    log::info("Praesenz: Nguithr'xurh-Kolonie ({}, {} Kugeln, {} Spinnen, {}) an der Decke bei ({:.1f}, {:.1f}, {:.1f}), {:.0f} m", K.mode, okN, made,
              web.hungry ? "hungrig" : "satt", site.x, site.y, site.z, hdist(site, eye));
    return true;
}

void Presence::regieNguithr(double dt, const Context& c, std::vector<Event>& out) {
    if (!c.kin || !c.kin[world::KIN_NGUITHR].on) return;
    const world::KinDef& K = c.kin[world::KIN_NGUITHR];
    bool alive = false;
    for (const auto& a : agents_) alive = alive || (a->role == Role::Nguithr && isWeb(*a) && !a->showcase);
    if (alive) return;
    nextNguithr_ -= dt;
    if (nextNguithr_ <= 0.0) nextNguithr_ = spawnNguithr(c, out, false) ? rng_.uniform(K.every * 0.6, K.every * 1.2) : 6.0;
}

void Presence::thinkNguithr(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    a.stateT += dt;
    a.cool -= dt;
    if (isWeb(a)) {
        // --- Netz: die Kolonie entscheidet --------------------------------------------------------------------------
        WebAg& g = wag(a);
        if (!g.laid) prepNguithr(a, c);
        const float st = stupor(a.target);
        int nearK = -1;
        float nearD = 9.0f;
        int hanging = 0;
        for (int k = 0; k < kS; ++k) {
            if (!g.ok[k] || g.fall[k] >= 0.0f) continue;
            ++hanging;
            const float dd = hdist(feet, g.anchor[k]);
            if (dd < nearD && std::fabs(feet.z - g.floorZ[k]) < 0.6f) nearD = dd, nearK = k;
        }
        const float dWeb = hdist(feet, a.pos);
        // fallende Kugeln: zerplatzen am Boden; wer darunter steht, bekommt den Stoff ab
        for (int k = 0; k < kS; ++k) {
            if (!g.ok[k] || g.fall[k] < 0.0f) continue;
            const float prev = g.fall[k];
            g.fall[k] = std::min(g.fall[k] + (float)dt, 3.0f);
            const float ti = fallTime(g, k);
            if (prev < ti && g.fall[k] >= ti) {
                const vec3 at{g.anchor[k].x, g.anchor[k].y, g.floorZ[k] + 0.05f};
                const bool on = hdist(feet, at) < 1.25f && std::fabs(feet.z - g.floorZ[k]) < 0.6f && c.mode >= 2;
                const float dose = on ? (g.phase <= 1 ? 0.33f : 0.24f) : 0.0f;
                if (dose > 0.0f) addDose(a.target, dose);
                out.push_back({Event::SacBurst, Role::Nguithr, at, dose});
            }
        }
        auto cut = [&](int k) {
            g.fall[k] = 0.0f;
            out.push_back({Event::Snip, Role::Nguithr, g.anchor[k] - vec3{0, 0, 0.05f}, 1.0f});
            g.tremble = 1.0f;
        };
        g.tremble = std::max(0.0f, g.tremble - (float)dt * 1.5f);
        switch (g.phase) {
            case 0: {  // lauert: wer genau unter einer Kugel steht
                const bool under = nearK >= 0 && nearD < 0.85f;
                g.underT = under && a.hungry && c.mode >= 2 ? g.underT + dt : std::max(0.0, g.underT - dt);
                if (g.underT > 0.7) {
                    cut(nearK);
                    g.phase = 1, g.waitT = 0.0, g.underT = 0.0;
                    log::info("Praesenz: Nguithr'xurh schneidet eine Kugel ab");
                }
                break;
            }
            case 1: {  // die erste ist gefallen: wer bleibt (oder benommen in der Naehe steht), bekommt alle
                g.waitT += dt;
                if (g.waitT > 2.8) {
                    if ((dWeb < 1.9f || (st > 0.2f && dWeb < 2.6f)) && c.mode >= 2) {
                        g.phase = 2, g.queue = hanging, g.cutT = 0.0;
                        log::info("Praesenz: Nguithr'xurh laesst alle Kugeln fallen ({})", hanging);
                    } else if (dWeb > 3.5f) {
                        g.phase = 0;  // fort - sie lauern weiter
                    }
                }
                break;
            }
            case 2: {  // alle fallen kurz nacheinander; danach seilen sich die Spinnen ab (thinkNguithr der Spinnen)
                g.cutT -= dt;
                if (g.queue > 0 && g.cutT <= 0.0) {
                    for (int k = 0; k < kS; ++k)
                        if (g.ok[k] && g.fall[k] < 0.0f) {
                            cut(k);
                            break;
                        }
                    --g.queue;
                    g.cutT = rng_.uniform(0.06, 0.2);
                }
                break;
            }
            default: break;
        }
        // neue Kugeln: alles gefallen und lange niemand in der Naehe (die Seite: sie bauen weiter), ungesehen
        if (hanging == 0 && g.phase != 1 && g.queue == 0) {
            g.regrowT = dWeb > 6.0f ? g.regrowT + dt : 0.0;
            if (g.regrowT > 45.0 && a.sight.level < 0.2f) {
                for (int k = 0; k < kS; ++k) g.fall[k] = -1.0f;
                g.phase = 0, g.regrowT = 0.0;
                log::info("Praesenz: Nguithr'xurh hat neue Kugeln gesponnen");
            }
        }
        // leises Klicken der Cheliceren in der Naehe (Geraeusch vor Sicht)
        g.chitT -= dt;
        if (g.chitT <= 0.0) {
            g.chitT = rng_.uniform(6.0, 13.0);
            if (hdist(eye, a.pos) < 11.0f) out.push_back({Event::Stridulate, Role::Nguithr, a.pos - vec3{0, 0, 0.1f}, 0.25f});
        }
        if ((hdist(eye, a.pos) > 55.0f && a.unseenT > 3.0) || (a.age > 900.0 && a.unseenT > 5.0 && g.phase == 0)) a.state = 9;
        prepNguithr(a, c);
        return;
    }
    // --- Spinne ---------------------------------------------------------------------------------------------------
    SpAg& sp = sag(a);
    auto* web = webOfGroup(agents_, a.group);
    if (!web || web->state == 9) {
        a.state = 9;
        return;
    }
    WebAg& wg = wag(*web);
    if (a.state == 10) {  // frisst: springt ins Gesicht
        a.hang = 0.0f, a.fly = 0.0f;
        killPose(a, dt, c);
        a.up = {0, 0, 1}, a.fwd = dirYaw(a.yaw);
        a.jaw = 1.0f, a.reach = 1.0f, a.perceive = 1.0f;
        prepNguithr(a, c);
        return;
    }
    const float s = sp.scale;
    const vec3 body = a.pos - a.up * (0.04f * s);
    lightReact(a, c, body, 10.0f, dt, 1e9);
    a.reach = 0.0f, a.jaw = 0.0f, a.perceive = 0.0f, a.alert = 0.0f, a.hang = 0.0f, a.crouch = 0.0f, a.fly = 0.0f;
    const float st = stupor(a.target);
    const float dFeet = hdist(a.pos, feet);
    const float dWeb = hdist(feet, web->pos);
    auto setState = [&](int x) { a.state = x, a.stateT = 0.0; };
    auto ceilAt = [&](vec3 p, float& z) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        if (!t || !t->open() || std::fabs(t->ceil - web->pos.z) > 0.12f) return false;
        z = t->ceil;
        return true;
    };
    auto startMove = [&](vec3 to, float speed) {
        sp.from = a.pos, sp.to = to, sp.moveP = 0.0f;
        sp.moveDur = std::max(0.05f, hdist(to, a.pos) / speed);
    };
    auto skitter = [&](float loud) {
        sp.skitT -= dt;
        if (sp.skitT <= 0.0) sp.skitT = 0.3, out.push_back({Event::Skitter, Role::Nguithr, a.pos, loud});
    };
    switch (a.state) {
        case 0:
        case 3: {  // an der Decke
            a.up = {0, 0, -1};
            if (a.state == 0 && sp.moveP < 1.0f && a.light > 0.5f) sp.moveP = 1.0f, sp.home = a.pos;  // auch mitten im Bauen: im Licht erstarren
            if (sp.moveP < 1.0f) {  // unterwegs (bauen oder huschen)
                sp.moveP = std::min(1.0f, sp.moveP + (float)dt / sp.moveDur);
                const vec3 p = sp.from + (sp.to - sp.from) * sp.moveP;
                a.pos = {p.x, p.y, a.pos.z};
                const vec3 dv = flat(sp.to - sp.from);
                if (length(dv) > 1e-3f) a.yaw = std::atan2(dv.y, dv.x);
                skitter(a.state == 3 ? 0.8f : 0.25f);
                wg.tremble = std::max(wg.tremble, 0.6f);
                if (sp.moveP >= 1.0f) setState(0), sp.home = a.pos;
            } else {
                // im Licht erstarrt sie; laenger angeleuchtet zirpt sie und huscht weg vom Licht
                sp.lightT = a.light > 0.5f ? sp.lightT + dt : 0.0;
                if (sp.lightT > 1.3 && a.cool <= 0.0) {
                    out.push_back({Event::Stridulate, Role::Nguithr, a.pos, 1.0f});
                    const vec3 away = nrmF(flat(a.pos - a.lightFrom), dirYaw(a.yaw));
                    for (int t = 0; t < 12; ++t) {
                        const vec3 q = a.pos + rotateYaw(away, (float)rng_.uniform(-0.8, 0.8)) * (float)rng_.uniform(1.0, 1.7);
                        float z;
                        if (hdist(q, web->pos) < 2.2f && ceilAt(q, z)) {
                            startMove({q.x, q.y, z}, 1.5f * std::sqrt(s));
                            setState(3);
                            break;
                        }
                    }
                    a.cool = 4.0, sp.lightT = 0.0;
                }
                // bauen: ab und zu ein Stueck weiter (langsam)
                sp.moveT -= dt * (a.light > 0.5f ? 0.0 : 1.0);
                if (sp.moveT <= 0.0) {
                    sp.moveT = rng_.uniform(6.0, 16.0);
                    const float ang = (float)rng_.uniform(-kPiF, kPiF), r = (float)rng_.uniform(0.2, 0.9);
                    const vec3 q = web->pos + vec3{std::cos(ang), std::sin(ang), 0.0f} * r;
                    float z;
                    if (ceilAt(q, z)) startMove({q.x, q.y, z}, 0.25f);
                }
                // die Kolonie hat alle Kugeln fallen lassen und das Opfer ist benommen: abseilen (nacheinander)
                if (wg.phase == 2 && wg.queue == 0 && st >= 0.45f && dWeb < 3.5f && c.mode >= 2) {
                    sp.delay += dt;
                    if (sp.delay > 0.6 * sp.index + 0.4) setState(1), sp.delay = 0.0;
                } else {
                    sp.delay = 0.0;
                }
            }
            a.fwd = dirYaw(a.yaw);
            break;
        }
        case 1: {  // abseilen: am Faden, Kopf voran
            a.hang = 1.0f;
            const vec3 to = nrmF(flat(feet - a.pos), dirYaw(a.yaw));
            a.up = to, a.fwd = {0, 0, -1};
            const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y));
            const float floorZ = t ? t->floor : a.pos.z - 2.5f;
            a.pos.z -= (float)dt * 0.75f;
            a.yaw += (float)dt * 0.8f;  // dreht sich langsam am Faden
            if (a.stateT < dt * 1.5) out.push_back({Event::Skitter, Role::Nguithr, a.pos, 0.5f});
            if (a.pos.z <= floorZ + 0.02f) a.pos.z = floorZ, setState(2), a.up = {0, 0, 1};
            break;
        }
        case 2: {  // am Boden: langsam zum Opfer (nur nahe dem Netz)
            a.up = {0, 0, 1};
            const vec3 to = nrmF(flat(feet - a.pos), dirYaw(a.yaw));
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 8.0);
            a.fwd = dirYaw(a.yaw);
            const double burst = std::fmod(a.stateT, 1.2) < 0.5 ? 1.0 : 0.25;  // huschen in Stoessen
            const float speed = (float)(0.75 * burst) * std::sqrt(s);
            const vec3 np = a.pos + to * (speed * (float)dt);
            if (const world::Tile* t = c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)); t && t->open() && std::fabs(t->floor - a.pos.z) < 0.3f)
                a.pos = {np.x, np.y, t->floor};
            if (burst > 0.5) skitter(0.6f);
            if (dFeet < 0.45f) setState(4);
            if (st < 0.3f || dWeb > 4.0f || hdist(a.pos, web->pos) > 4.5f) setState(5);
            break;
        }
        case 4: {  // am Opfer: kaut, tastet; reagiert es nicht mehr, frisst sie (toedlich)
            a.up = {0, 0, 1};
            a.jaw = 1.0f, a.perceive = 1.0f, a.perceiveAt = eye;
            a.reach = 0.35f + 0.35f * (float)std::sin(a.stateT * 5.0);
            if (st >= 0.8f && c.mode >= 2 && a.stateT > 0.8) {
                out.push_back({Event::Attack, Role::Nguithr, a.cre.headPos(), 1.0f});
                startKill(a, c);
                break;
            }
            if (dFeet > 0.9f) setState(2);
            if (st < 0.35f) setState(5);
            break;
        }
        case 5: {  // zurueck unter das Netz, dann hinauf
            a.up = {0, 0, 1};
            const vec3 target{sp.home.x, sp.home.y, a.pos.z};
            const vec3 to = nrmF(flat(target - a.pos), dirYaw(a.yaw));
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 8.0);
            a.fwd = dirYaw(a.yaw);
            const vec3 np = a.pos + to * (0.6f * std::sqrt(s) * (float)dt);
            if (const world::Tile* t = c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)); t && t->open() && std::fabs(t->floor - a.pos.z) < 0.3f)
                a.pos = {np.x, np.y, t->floor};
            skitter(0.4f);
            if (hdist(a.pos, target) < 0.15f) setState(6);
            break;
        }
        case 6: {  // hinauf
            a.hang = 1.0f;
            a.up = dirYaw(a.yaw), a.fwd = {0, 0, 1};
            a.pos.z += (float)dt * 0.6f;
            if (a.pos.z >= sp.home.z) a.pos = sp.home, setState(0), a.up = {0, 0, -1}, a.fwd = dirYaw(a.yaw);
            break;
        }
        default: break;
    }
    prepNguithr(a, c);
}

}  // namespace lim::game
