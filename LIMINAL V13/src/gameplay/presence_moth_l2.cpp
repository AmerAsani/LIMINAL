// V13.3 Teil 4: Deathmoths in Level 2 (KREATUR_Deathmoth.md, Abschnitt Level 2; Wikidot Entity 4 Rev. 6, Level 2 Rev. 93).
//
// Die Level-2-Seite: frueher zahlreich um die Leuchtstoffroehren (Licht lockt sie), im grossen Stromausfall weggezogen oder
// von den Smilern gefressen; heute kleiner als anderswo (Nahrungsmangel, enge Gaenge) und meist friedlich - man soll sie
// aber nicht aus der Naehe reizen. Entity 4: die Maennchen sind friedliche Aasfresser mit Ruessel, die Weibchen
// aggressiv mit Reisskiefern; gestresste Weibchen rufen mit Lockstoffen die anderen herbei. Fressfeinde in Level 2:
// die kleinen Nguithr'xurh an den Decken ueber den Maschinen (presence_nguithrxurh.cpp, Netz mit eingesponnenen Motten).
//
// Im Spiel: eine kleine Gruppe (presence.moth.group) an einer brennenden Roehre, kein Nest. Maennchen (Variante 1,
// ent_moth_male, Massstab 0,52: etwa 23 cm Spannweite), manchmal ein Weibchen (Variante 2, ent_moth, Massstab 0,66).
//
//   state 0  sitzt      an der Decke neben der Roehre; ab und zu fliegt eine auf
//         1  fliegt     friedlich (stage 0: um die Roehre, stoesst klickend ans Glas; 1: um die Lampe des Spielers -
//                       das Licht lockt sie; 2: zurueck an den Platz; 3: im Netz gefangen, zappelt; 4: treibt ins Netz;
//                       5: zieht ab - die Roehre ist dunkel)
//         2  gereizt    schwirrt um den Kopf (Ankuendigung: Brummen, Event::Hiss role Moth); ohne Weibchen beruhigen
//                       sie sich, sobald man zuruecktritt
//         3  beisst     nur das Weibchen: stoesst zu (Event::Hit role Moth, 3 Punkte), dann wieder Abstand
//   Reizen   der Gruppe: das Gesicht unter 0,9 m an den Motten, die Lampe aus unter 2,5 m auf sie, Rennen oder Springen
//            unter 1,5 m; klingt ab. Wer zuruecktritt (4 m) oder das Licht senkt, ist sie los.
// Klaenge (app.cpp, synth_l2.cpp): Klicken am Glas (Event::Knock role Moth), Brummen (Hiss), Biss (Hit), Auffliegen
// (TakeOff); das Flattern der fliegenden Motten ist die Schleife moth_flutter (Presence::swarm).
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"
#include "props/prop_library.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
struct MothGroup {
    vec3 tube;            // Leuchte, um die sie kreisen
    double prov = 0.0;    // Reizung der Gruppe
    double agT = 0.0;     // so lange schon gereizt
    double darkT = 0.0;   // so lange ist die Roehre schon dunkel
    double lampOffT = 0.0;
    bool agitated = false, female = false, hissed = false;
};
template <class A>
MothGroup& grp(A& a) {
    if (!a.ext) a.ext = std::make_shared<MothGroup>();
    return *static_cast<MothGroup*>(a.ext.get());
}
constexpr float kMale = 0.52f, kFemale = 0.66f;
}  // namespace

bool Presence::mothL2(const Context& c) const { return c.kin && c.kin[world::KIN_MOTH].on; }

bool Presence::agentInfo(Role r, int i, vec3& pos, int& state, int& variant) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase && i-- == 0) {
            pos = a->pos, state = a->state, variant = a->variant;
            return true;
        }
    return false;
}

void Presence::prepMothL2(Agent& a) { a.cre.setScale(a.variant == 2 ? kFemale : kMale); }

bool Presence::spawnMothL2(const Context& c, std::vector<Event>& out, bool forced) {
    if (!mothL2(c) || !fld().valid()) return false;
    const world::KinDef& K = c.kin[world::KIN_MOTH];
    const vec3 eye = c.view.eye;
    const world::Sector* sec = c.world->sectorIfReady(c.player->x, c.player->y);
    if (!sec) return false;
    // brennende Leuchtstoffroehren der Karte (tube_white, tube_orange; die toten Gruppen sind tube_dead)
    static const props::Model* tubes[4] = {props::model("tube_white", 0), props::model("tube_white", 1), props::model("tube_orange", 0),
                                           props::model("tube_orange", 1)};
    const int lo = forced ? 30 : 80, hi = forced ? 220 : 340;
    // Netze der Nguithr'xurh in der Naehe: dort sammeln sie sich auch (die Spinnen fressen Motten)
    std::vector<vec3> webs;
    for (const auto& a : agents_)
        if (a->role == Role::Nguithr && (a->variant == 1 || a->variant == 2) && !a->showcase) webs.push_back(a->pos);
    float best = 1e9f;
    vec3 site;
    const world::Tile* st = nullptr;
    for (const auto& pr : sec->props) {
        bool tube = false;
        for (const props::Model* m : tubes) tube = tube || (m && pr.model == m);
        if (!tube || hdist({pr.x, pr.y, 0.0f}, eye) > (float)hi * 0.1f + 2.0f) continue;
        const i64 x = ifloor(pr.x), y = ifloor(pr.y);
        const int cost = fld().at(x, y);
        if (cost < lo || cost > hi) continue;
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || !t->open() || t->clearance() > 4.2f) continue;
        if (darkTile(c, t, pr.x, pr.y)) continue;
        const vec3 lamp{pr.x, pr.y, t->ceil - 0.12f};
        bool visible = false;
        for (const vec3& q : {lamp, lamp + vec3{0.5f, 0, 0}, lamp + vec3{-0.5f, 0, 0}, lamp + vec3{0, 0.5f, 0}, lamp + vec3{0, -0.5f, 0}})
            if (dot(nrmF(q - eye, {1, 0, 0}), c.view.look) > 0.25f && clearSight(*c.world, eye, q)) visible = true;
        if (visible && !forced) continue;
        float score = (float)rng_.uniform(0.0, 3.0) + (visible ? 10.0f : 0.0f);
        for (const vec3& w : webs)
            if (hdist(w, lamp) < 5.0f) score -= 2.5f;
        if (score < best) best = score, site = lamp, st = t;
    }
    if (!st) return false;
    auto G = std::make_shared<MothGroup>();
    G->tube = site;
    G->female = rng_.uniform(0.0, 1.0) < K.chance;
    const int n = (int)rng_.randint(std::max(2, K.groupMin), std::max(2, K.groupMax));
    const int group = nextGroup_++;
    std::vector<vec3> used;
    int made = 0;
    for (int i = 0; i < n * 4 && made < n; ++i) {
        const vec3 p{site.x + (float)rng_.uniform(-0.65, 0.65), site.y + (float)rng_.uniform(-0.45, 0.45), st->ceil - 0.025f};
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        if (!t || !t->open() || std::fabs(t->ceil - st->ceil) > 0.05f) continue;
        bool close = false;
        for (const vec3& u : used) close = close || length(u - p) < 0.16f;
        if (close) continue;
        used.push_back(p);
        auto m = std::make_unique<Agent>();
        m->role = Role::Moth;
        m->ent = Entity::Moth;
        m->variant = (made == 0 && G->female) ? 2 : 1;
        m->group = group;
        m->ext = G;
        m->noShadow = true;
        const float yaw = (float)rng_.uniform(-kPiF, kPiF);
        m->rest = p, m->restN = {0, 0, -1}, m->restF = dirYaw(yaw);
        m->up = m->restN, m->fwd = m->restF;
        m->home = site;
        m->phase = (float)rng_.uniform(0.0, 6.28);
        m->radius = (float)rng_.uniform(0.22, 0.55);
        m->bob = (float)rng_.uniform(-0.12, 0.12);
        m->spin = rng_.random() < 0.5 ? 1.0f : -1.0f;
        m->speed = rng_.uniform(1.6, 2.3);
        m->timer = rng_.uniform(2.0, 14.0);
        m->initBody(p, yaw, (unsigned)rng_.randint(1, 1 << 30));
        prepMothL2(*m);
        anim::CreatureAnimator::Input in;  // gleich fertig an der Decke sitzend
        in.pos = p, in.yaw = yaw, in.up = m->restN, in.fwd = m->restF;
        for (int it = 0; it < 30; ++it) m->cre.update(0.1, in);
        m->rig.setCreaturePose(m->cre.pose());
        if (rng_.random() < 0.35) {  // einige fliegen schon um die Roehre
            m->state = 1, m->stage = 0;
            m->pos = site + vec3{std::cos(m->phase) * m->radius, std::sin(m->phase) * m->radius, -0.1f};
            m->fly = 1.0f;
            m->timer2 = rng_.uniform(6.0, 20.0);
        }
        agents_.push_back(std::move(m));
        ++made;
    }
    if (!made) return false;
    out.push_back({Event::Appear, Role::Moth, site});
    log::info("Praesenz: {} Deathmoths (Level 2{}) an einer Roehre bei ({:.1f}, {:.1f}), {:.0f} m", made, G->female ? ", mit Weibchen" : "", site.x,
              site.y, hdist(site, eye));
    return true;
}

void Presence::regieMothL2(double dt, const Context& c, std::vector<Event>& out) {
    if (!mothL2(c)) return;
    const world::KinDef& K = c.kin[world::KIN_MOTH];
    std::vector<int> groups;
    for (const auto& a : agents_)
        if (a->role == Role::Moth && a->variant > 0 && !a->showcase && std::find(groups.begin(), groups.end(), a->group) == groups.end())
            groups.push_back(a->group);
    if ((int)groups.size() >= std::max(1, K.count)) return;
    nextMothL2_ -= dt;
    if (nextMothL2_ <= 0.0) nextMothL2_ = spawnMothL2(c, out, false) ? rng_.uniform(K.every * 0.6, K.every * 1.2) : 7.0;
}

void Presence::thinkMothL2(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    MothGroup& G = grp(a);
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    a.stateT += dt;
    a.cool -= dt;
    a.shiver = 0.0f;
    a.reach = 0.0f;
    const float dEye = length(a.pos - eye);
    a.perceive = dEye < 3.0f ? 1.0f : 0.0f;
    a.perceiveAt = eye;
    a.light = c.view.flashOn && dEye < 8.0f ? 1.0f : 0.0f;
    a.lightFrom = c.view.flashPos;
    const bool female = a.variant == 2;
    // --- die Gruppe: Roehre dunkel? gereizt? (einmal je Bild - das erste Mitglied der Gruppe rechnet) ---------------
    bool lead = true;
    for (const auto& o : agents_) {
        if (o.get() == &a) break;
        if (o->ext == a.ext) lead = false;
    }
    if (lead) {
        const world::Tile* tt = c.world->tileIfLoaded(ifloor(G.tube.x), ifloor(G.tube.y));
        G.darkT = darkTile(c, tt, G.tube.x, G.tube.y) ? G.darkT + dt : 0.0;
        G.lampOffT = c.view.flashOn ? 0.0 : G.lampOffT + dt;
        // Reizen: Gesicht dicht an den Motten, Licht aus der Naehe auf sie, Rennen/Springen darunter
        double add = 0.0;
        for (const auto& o : agents_) {
            if (o->ext != a.ext || o->state > 1) continue;
            const float de = length(o->pos - eye);
            if (de < 0.9f) add = std::max(add, 1.0);
            if (de < 2.5f && inBeam(c.view, *c.world, o->pos, 2.5f)) add = std::max(add, 0.8);
            if (hdist(o->pos, feet) < 1.5f && (c.player->sprinting || c.noise >= 0.79)) add = std::max(add, 2.0);
        }
        G.prov = std::max(0.0, G.prov + (add - 0.4) * dt);
        const float dG = hdist(G.tube, feet);
        if (!G.agitated && G.prov > 2.5) {
            G.agitated = true, G.agT = 0.0, G.hissed = false;
            out.push_back({Event::TakeOff, Role::Moth, G.tube});
            log::info("Praesenz: Deathmoths gereizt ({})", G.female ? "Weibchen dabei" : "nur Maennchen");
            for (auto& o : agents_)
                if (o->ext == a.ext && o->state <= 1 && o->stage != 3) o->state = 2, o->stateT = 0.0, o->fly = 1.0f;
        }
        if (G.agitated) {
            G.agT += dt;
            if (!G.hissed && G.agT > 0.25) {  // Ankuendigung: das Brummen schwillt an (in 1,4 s beisst das Weibchen)
                G.hissed = true;
                out.push_back({Event::Hiss, Role::Moth, eye + vec3{0, 0, 0.1f}, G.female ? 1.0f : 0.5f});
            }
            const bool still = hdist(G.tube, feet) < 4.0f && (G.prov > 0.8 || (c.view.flashOn && dG < 3.0f));
            if ((!still && G.agT > 2.0) || G.agT > (G.female ? 14.0 : 6.0)) {
                G.agitated = false, G.prov = 0.0;
                for (auto& o : agents_)
                    if (o->ext == a.ext && o->state >= 2 && o->state <= 3) o->state = 1, o->stage = 2, o->stateT = 0.0;
            }
        }
    }
    auto openAt = [&](vec3 p) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        return t && t->open() && t->clearance() > 0.6f;
    };
    auto orbit = [&](vec3 tgt, float radius, float speedK, float jitter) {
        a.phase += (float)dt * (2.6f + 0.8f * radius) * a.spin;
        const vec3 want = tgt + vec3{std::cos(a.phase) * radius, std::sin(a.phase) * radius, a.bob + 0.1f * std::sin(a.phase * 2.3f)};
        const vec3 jit{(float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-0.6, 0.6)};
        a.vel += ((want - a.pos) * 5.0f - a.vel * 1.8f + jit * jitter) * (float)dt;
        const float sp = length(a.vel), mx = (float)a.speed * speedK;
        if (sp > mx) a.vel = a.vel * (mx / sp);
    };
    auto toward = [&](vec3 tgt, float speed) {
        const vec3 to = tgt - a.pos;
        a.vel += (nrmF(to, {0, 0, 1}) * std::min(speed, 0.6f + 3.0f * length(to)) - a.vel) * (float)std::min(1.0, dt * 4.0);
    };
    switch (a.state) {
        case 0: {  // sitzt
            a.pos = a.rest, a.up = a.restN, a.fwd = a.restF, a.fly = 0.0f, a.vel = {};
            a.timer -= dt;
            const bool lured = c.view.flashOn && (inBeam(c.view, *c.world, a.pos, 9.0f) || length(c.view.flashPos - a.pos) < 4.0f);
            if (G.darkT > 1.5 || lured || a.timer <= 0.0) {
                a.state = 1, a.stage = lured ? 1 : (G.darkT > 1.5 ? 5 : 0), a.stateT = 0.0;
                a.vel = vec3{0, 0, -0.9f} + dirYaw((float)rng_.uniform(-3.14, 3.14)) * 0.4f;
                a.fly = 1.0f;
                a.timer2 = rng_.uniform(6.0, 22.0);
            }
            return;
        }
        case 1: {
            a.fly = 1.0f;
            if (a.stage == 3) {  // im Netz: zappelt, haengt fest
                a.vel = {};
                a.shiver = 1.0f;
                a.fly = 0.4f + 0.3f * (float)std::sin(a.stateT * 23.0);
                if (a.stateT > 6.0 && a.unseenT > 2.0) a.state = 9;  // eingesponnen (ungesehen)
                return;
            }
            if (a.stage == 4) {  // treibt ins Netz
                toward(a.goal, 1.4f);
                if (length(a.goal - a.pos) < 0.08f) {
                    a.stage = 3, a.stateT = 0.0, a.pos = a.goal;
                    out.push_back({Event::Stridulate, Role::Nguithr, a.goal + vec3{0, 0, 0.3f}, 0.4f});
                    return;
                }
                break;
            }
            if (G.darkT > 1.5 && a.stage != 1 && a.stage != 5) a.stage = 5, a.stateT = 0.0;  // die Roehre ist aus: fort
            if (c.view.flashOn && a.stage != 5 && (inBeam(c.view, *c.world, a.pos, 9.0f) || length(c.view.flashPos - a.pos) < 4.0f)) a.stage = 1;
            if (a.stage == 1) {  // die Lampe lockt: um das Licht vor der Lampe
                if (G.lampOffT > 3.0 || length(c.view.flashPos - a.pos) > 12.0f) {
                    a.stage = G.darkT > 1.5 ? 5 : 0, a.stateT = 0.0;
                } else {
                    orbit(c.view.flashPos + c.view.flashDir * 1.0f, a.radius + 0.15f, 1.3f, 6.0f);
                }
            } else if (a.stage == 0) {  // um die Roehre, ab und zu ans Glas
                orbit(G.tube, a.radius, 1.0f, 5.0f);
                if (length(a.pos - G.tube) < 0.2f && a.cool <= 0.0 && hdist(a.pos, eye) < 16.0f) {
                    a.cool = rng_.uniform(1.5, 4.0);
                    out.push_back({Event::Knock, Role::Moth, a.pos, 0.4f});
                }
                a.timer2 -= dt;
                // ein Netz nahebei: manchmal treibt ein Maennchen hinein (die Nguithr'xurh fressen Motten)
                if (!female && rng_.random() < dt / 45.0)
                    for (const auto& o : agents_)
                        if (o->role == Role::Nguithr && (o->variant == 1 || o->variant == 2) && hdist(o->pos, G.tube) < 4.0f) {
                            const float ang = (float)rng_.uniform(-kPiF, kPiF);
                            a.goal = o->pos + vec3{std::cos(ang) * 0.3f, std::sin(ang) * 0.3f, -0.42f};
                            a.stage = 4, a.stateT = 0.0;
                            break;
                        }
                if (a.timer2 <= 0.0) a.stage = 2, a.stateT = 0.0;
            } else if (a.stage == 2) {  // zurueck an den Platz
                const vec3 home = a.rest + a.restN * 0.1f;
                toward(home, 1.5f);
                if (length(home - a.pos) < 0.12f || (a.stateT > 20.0 && !a.sight.inView)) {
                    a.state = 0, a.stateT = 0.0, a.pos = a.rest, a.timer = rng_.uniform(8.0, 30.0);
                    return;
                }
            } else {  // stage 5: fort - weg vom Spieler, ungesehen ist sie verschwunden
                vec3 away = nrmF(flat(a.pos - eye), {1, 0, 0});
                toward(a.pos + away * 2.0f + vec3{0, 0, 0.2f}, 2.2f);
                if (a.unseenT > 1.0 || a.stateT > 25.0) a.state = 9;
            }
            break;
        }
        case 2: {  // gereizt: schwirrt um den Kopf
            a.fly = 1.0f;
            orbit(eye + vec3{0, 0, 0.05f}, 0.35f + 0.25f * a.radius, 1.8f, 9.0f);
            if (female && G.hissed && G.agT > 1.65 && a.cool <= 0.0 && hdist(G.tube, feet) < 3.0f) a.state = 3, a.stateT = 0.0;
            break;
        }
        case 3: {  // beisst (nur das Weibchen)
            a.fly = 1.0f;
            a.reach = 1.0f;
            const vec3 tgt = eye - vec3{0, 0, 0.2f};
            const vec3 to = tgt - a.pos;
            const float dl = length(to);
            if (dl < 0.32f) {
                if (c.mode >= 2) out.push_back({Event::Hit, Role::Moth, a.pos, 3.0f});
                a.cool = rng_.uniform(3.5, 5.5);
                a.state = 2, a.stateT = 0.0;
                a.vel = -nrmF(to, {1, 0, 0}) * 2.2f;
                break;
            }
            a.vel += (nrmF(to, {1, 0, 0}) * std::min(3.8f, 1.0f + 3.0f * dl) - a.vel) * (float)std::min(1.0, dt * 12.0);
            if (a.stateT > 2.0) a.state = 2, a.stateT = 0.0, a.cool = 2.0;
            break;
        }
        default: break;
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
        const float lo = t->floor + 0.3f, hi = t->ceil - ((a.state == 1 && a.stage == 2) ? 0.02f : 0.08f);
        if (np.z > hi) np.z = std::max(hi, a.pos.z - (float)dt * 0.5f), a.vel.z = -std::fabs(a.vel.z) * 0.5f;
        if (np.z < lo) np.z = std::min(lo, a.pos.z + (float)dt * 0.5f), a.vel.z = std::fabs(a.vel.z) * 0.5f;
    }
    a.pos = np;
    if (length(flat(a.vel)) > 0.05f) a.yaw = std::atan2(a.vel.y, a.vel.x);
    a.up = {0, 0, 1};
    a.fwd = {};
    // weit weg und ungesehen: die Gruppe ist fort
    if (hdist(a.pos, eye) > 45.0f && a.unseenT > 3.0) a.state = 9;
}

}  // namespace lim::game
