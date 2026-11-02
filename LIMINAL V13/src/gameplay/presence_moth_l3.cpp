// V13.3 Level 3: Deathmoth-Schwaerme mit Praetorianern (KREATUR_Deathmoth.md, Abschnitt Level 3; Wikidot Level 3 Rev. 18,
// Entity 4 Rev. 6). Level-JSON presence.moth.mode "swarm"; Rolle Moth, Varianten 3 (Maennchen), 4 (Weibchen) in voller
// Groesse (30-45 cm Spannweite) und 5 (Praetorianer, knapp 1 m, acht Beine; creature_moth_l3.cpp).
//
// Die Level-3-Seite: immer in grossen Schwaermen, auch die Maennchen greifen Menschen an; sie zielen auf Mund (bis in die
// Lunge), Nase, Ohren und Augen und ersticken ein Opfer in Sekunden; ueber 40 % sind Praetorianer, die alle Noclip-Stellen
// kennen und Umwege nehmen, um die Beute einzukesseln. Entity 4: helles Licht lockt sie; Praetorianer stechen mit
// Giftbeinen (Laehmung) und spucken aetzende Fluessigkeit; gestresste Weibchen rufen mit Lockstoff die Praetorianer.
//
// Im Spiel (ein Schwarm zur Zeit an einer brennenden Roehre, ausser Sicht; presence.moth: group = gewoehnliche Motten,
// chance = Anteil der Praetorianer, every = Abstand):
//   gewoehnliche   0 sitzen an der Decke um die Roehre (Fluegel zittern vor dem Auffliegen)
//                  2 jagen: zum Kopf des Spielers - mit Lampe zum Licht vor der Lampe (es lockt sie), ohne langsamer
//                  3 kuendigen an: schwirren vor dem Gesicht, das Brummen schwillt (1,2 s, Event Hiss)
//                  4 im Gesicht: Mund, Nase, Augen, Ohren (Plaetze vor der Kamera) - 0,9 Punkte je Motte und Sekunde
//                  5 abgeschuettelt: taumeln weg (2-3 s); 6 zurueck an die Roehre
//   Reizen: Lampe auf eine Motte (bis 10 m), naeher als 4,5 m an der Roehre, Rennen bis 9 m. Abwehr (Seite: Licht dimmen,
//   meiden, fliehen): Lampe aus - sie lassen los und verlieren ihn (nach 2,5 s im Dunkeln und 6 m Abstand kehren sie um);
//   Rennen schuettelt sie ab; im Dunkeln ohne Lampe greifen sie seltener an.
//   Praetorianer   0 wachen am Boden nahe der Roehre (im Schatten), faecheln ab und zu
//                  1 alarmiert: faechelt Lockstoff (1,5 s) - dann
//                  2 nahen; einer nimmt den Umweg (ungesehen fort, taucht hinter dem Spieler wieder auf - Noclip-Stelle)
//                  3 baeumt sich auf (Ankuendigung, 0,8 s: Klicken, Zischen) -> 4 sticht (9 Punkte, Laehmung 0,45)
//                  5 weicht ab (1,5 s), 6 spuckt aus 3-6 m (Ankuendigung 0,6 s, 5 Punkte)
//   Hoechstens zwei Praetorianer greifen zugleich an. Klaenge: Klangnummern KC_MOTH_*/KC_PR_* (kin_l3.hpp).
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"
#include "gameplay/kin_l3.hpp"
#include "props/prop_library.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
struct Swarm {
    vec3 tube;
    int group = -1;
    bool roused = false;
    double prov = 0.0, t = 0.0, darkT = 0.0, faceTick = 0.0, faceDmg = 0.0, announcedT = -100.0, lampOffT = 0.0;
    int faceCount = 0;
};
struct MothL3 {
    int slot = -1;
    double knockT = 0.0;
};
struct PraeL3 {
    double t = 0.0, fanT = 0.0, coolT = 0.0, spitT = 6.0, clickT = 0.0;
    bool flanker = false, hit = false, warned = false, detoured = false;
    vec3 post;
};
template <class T, class A>
T& extOf(A& a) {
    if (!a.ext) a.ext = std::make_shared<T>();
    return *static_cast<T*>(a.ext.get());
}
float cueM(int k) { return kKinCue + (float)k; }
// Plaetze im Gesicht (Kamerarahmen: vor, rechts, oben): Mund, Nase, Augen, Ohren, Wangen
constexpr float kSlot[8][3] = {{0.075f, 0.0f, -0.095f}, {0.095f, 0.0f, -0.035f}, {0.075f, 0.036f, 0.005f}, {0.075f, -0.036f, 0.005f},
                               {-0.005f, 0.092f, -0.02f}, {-0.005f, -0.092f, -0.02f}, {0.06f, 0.065f, -0.07f}, {0.06f, -0.065f, -0.07f}};
}  // namespace

bool Presence::mothL3(const Context& c) const { return c.kin && c.kin[world::KIN_MOTH].on && c.kin[world::KIN_MOTH].mode == "swarm"; }

void Presence::swarmRouse(bool on) {
    if (auto* S = static_cast<Swarm*>(l3Swarm_.get())) S->prov = on ? 2.0 : 0.0;
}
bool Presence::swarmRoused() const {
    const auto* S = static_cast<const Swarm*>(l3Swarm_.get());
    return S && S->roused;
}

void Presence::prepMothL3(Agent& a, const Context& c) {
    if (a.variant == 5) {
        a.cre.setExtra({5.0f});
        // Vorfuehrung: die Motten-Vorfuehrung schwebt 1,3 m hoch - der Praetorianer steht am Boden
        if (a.showcase)
            if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)); t && a.pos.z > t->floor + 0.05f) a.snapBody({a.pos.x, a.pos.y, t->floor}, a.yaw);
    } else {
        a.cre.setExtra({});
    }
}

bool Presence::spawnMothL3(const Context& c, std::vector<Event>& out, bool forced) {
    if (!mothL3(c) || !fld().valid()) return false;
    const world::KinDef& K = c.kin[world::KIN_MOTH];
    const vec3 eye = c.view.eye;
    const world::Sector* sec = c.world->sectorIfReady(c.player->x, c.player->y);
    if (!sec) return false;
    static const props::Model* tubes[2] = {props::model("tube_white", 0), props::model("tube_white", 1)};
    const int lo = forced ? 40 : 120, hi = forced ? 220 : 360;
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
        if (!t || !t->open() || t->clearance() > 4.5f || t->clearance() < 2.2f || darkTile(c, t, pr.x, pr.y)) continue;
        const vec3 lamp{pr.x, pr.y, t->ceil - 0.12f};
        bool visible = false;
        for (const vec3& q : {lamp, lamp + vec3{0.6f, 0, 0}, lamp + vec3{-0.6f, 0, 0}, lamp + vec3{0, 0.6f, 0}, lamp + vec3{0, -0.6f, 0},
                              vec3{pr.x, pr.y, t->floor + 0.6f}})
            if (dot(nrmF(q - eye, {1, 0, 0}), c.view.look) > 0.2f && clearSight(*c.world, eye, q)) visible = true;
        if (visible && !forced) continue;
        const float score = (float)rng_.uniform(0.0, 3.0) + (visible ? 10.0f : 0.0f);
        if (score < best) best = score, site = lamp, st = t;
    }
    if (!st) return false;
    auto S = std::make_shared<Swarm>();
    S->tube = site;
    S->group = nextGroup_++;
    l3Swarm_ = S;
    const int n = (int)rng_.randint(std::max(3, K.groupMin), std::max(3, K.groupMax));
    const double share = std::clamp(K.chance, 0.0, 0.8);
    const int np = std::max(1, (int)std::lround(n * share / std::max(0.2, 1.0 - share)));
    std::vector<vec3> used;
    int made = 0, madeP = 0;
    // gewoehnliche: an der Decke um die Roehre (Maennchen und Weibchen - in Level 3 beide angriffslustig)
    for (int i = 0; i < n * 5 && made < n; ++i) {
        const vec3 p{site.x + (float)rng_.uniform(-1.1, 1.1), site.y + (float)rng_.uniform(-0.8, 0.8), st->ceil - 0.03f};
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        if (!t || !t->open() || std::fabs(t->ceil - st->ceil) > 0.05f) continue;
        bool close = false;
        for (const vec3& u : used) close = close || length(u - p) < 0.3f;
        if (close) continue;
        used.push_back(p);
        auto m = std::make_unique<Agent>();
        m->role = Role::Moth;
        m->ent = Entity::Moth;
        m->variant = rng_.uniform(0.0, 1.0) < 0.5 ? 3 : 4;
        m->group = S->group;
        m->noShadow = true;
        const float yaw = (float)rng_.uniform(-kPiF, kPiF);
        m->rest = p, m->restN = {0, 0, -1}, m->restF = dirYaw(yaw);
        m->up = m->restN, m->fwd = m->restF;
        m->home = site;
        m->phase = (float)rng_.uniform(0.0, 6.28);
        m->radius = (float)rng_.uniform(0.3, 0.7);
        m->bob = (float)rng_.uniform(-0.15, 0.15);
        m->spin = rng_.random() < 0.5 ? 1.0f : -1.0f;
        m->speed = rng_.uniform(2.6, 3.4);
        m->timer = rng_.uniform(4.0, 18.0);
        m->initBody(p, yaw, (unsigned)rng_.randint(1, 1 << 30));
        prepMothL3(*m, c);
        anim::CreatureAnimator::Input in;
        in.pos = p, in.yaw = yaw, in.up = m->restN, in.fwd = m->restF;
        for (int it = 0; it < 30; ++it) m->cre.update(0.1, in);
        m->rig.setCreaturePose(m->cre.pose());
        agents_.push_back(std::move(m));
        ++made;
    }
    // Praetorianer: am Boden um die Roehre, im Schatten der Waende
    for (int i = 0; i < np * 8 && madeP < np; ++i) {
        const vec3 p{site.x + (float)rng_.uniform(-3.5, 3.5), site.y + (float)rng_.uniform(-3.5, 3.5), 0.0f};
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        if (!figureWalkable(t, 1.1f) || std::fabs(t->floor - st->floor) > 0.3f) continue;
        const int cost = fld().at(ifloor(p.x), ifloor(p.y));
        if (cost < 0 || (!forced && cost < 90)) continue;
        bool close = false;
        for (const vec3& u : used) close = close || hdist(u, p) < 1.0f;
        if (close) continue;
        const vec3 q{p.x, p.y, t->floor};
        bool visible = false;
        for (float h : {0.3f, 0.8f})
            if (dot(nrmF(q + vec3{0, 0, h} - eye, {1, 0, 0}), c.view.look) > 0.2f && clearSight(*c.world, eye, q + vec3{0, 0, h})) visible = true;
        if (visible && !forced) continue;
        used.push_back(q);
        auto m = std::make_unique<Agent>();
        m->role = Role::Moth;
        m->ent = Entity::Moth;
        m->variant = 5;
        m->group = S->group;
        m->glow = 1.0f;
        m->initBody(q, std::atan2(eye.y - q.y, eye.x - q.x) + (float)rng_.uniform(-1.0, 1.0), (unsigned)rng_.randint(1, 1 << 30));
        prepMothL3(*m, c);
        PraeL3& P = extOf<PraeL3>(*m);
        P.post = q;
        P.fanT = rng_.uniform(3.0, 9.0);
        agents_.push_back(std::move(m));
        ++madeP;
    }
    if (!made && !madeP) return false;
    out.push_back({Event::Appear, Role::Moth, site});
    log::info("Praesenz: Deathmoth-Schwarm (Level 3): {} Motten, {} Praetorianer an einer Roehre bei ({:.1f}, {:.1f}), {:.0f} m", made, madeP, site.x,
              site.y, hdist(site, eye));
    return true;
}

void Presence::regieMothL3(double dt, const Context& c, std::vector<Event>& out) {
    if (!mothL3(c)) return;
    const world::KinDef& K = c.kin[world::KIN_MOTH];
    bool any = false;
    for (const auto& a : agents_) any = any || (a->role == Role::Moth && a->variant >= 3 && !a->showcase);
    if (any) {
        // der Schwarm als Ganzes: Reizung, Rueckzug (einmal je Bild)
        if (auto* S = static_cast<Swarm*>(l3Swarm_.get())) {
            S->t += dt;
            S->lampOffT = c.view.flashOn ? 0.0 : S->lampOffT + dt;
            S->prov = std::max(0.0, S->prov - dt * 0.35);
        }
        return;
    }
    l3Swarm_.reset();
    nextMothL2_ -= dt;
    if (nextMothL2_ <= 0.0) nextMothL2_ = spawnMothL3(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 8.0;
}

// --- gewoehnliche Motten -------------------------------------------------------------------------------------------------
void Presence::thinkMothL3(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    auto* Sp = static_cast<Swarm*>(l3Swarm_.get());
    if (!Sp) {
        a.state = 9;
        return;
    }
    Swarm& S = *Sp;
    MothL3& M = extOf<MothL3>(a);
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    a.stateT += dt;
    a.cool -= dt;
    a.shiver = 0.0f, a.reach = 0.0f;
    const float dEye = length(a.pos - eye);
    a.perceive = dEye < 4.0f ? 1.0f : 0.0f, a.perceiveAt = eye;
    a.light = c.view.flashOn && dEye < 9.0f ? 1.0f : 0.0f;
    a.lightFrom = c.view.flashPos;
    // Basis der Kamera (Plaetze im Gesicht)
    const vec3 fw = nrmF(c.view.look, {1, 0, 0}), rt = nrmF(cross(fw, vec3{0, 0, 1}), {0, 1, 0}), up = cross(rt, fw);
    // --- Reizung des Schwarms (nur die erste Motte rechnet) -----------------------------------------------------------------
    bool lead = true;
    for (const auto& o : agents_) {
        if (o.get() == &a) break;
        if (o->role == Role::Moth && o->group == a.group && o->variant >= 3 && o->variant <= 4) lead = false;
    }
    if (lead) {
        double add = 0.0;
        for (const auto& o : agents_) {
            if (o->group != a.group || o->variant < 3 || o->variant > 4) continue;
            if (inBeam(c.view, *c.world, o->pos, 10.0f)) add = std::max(add, 1.6);
        }
        if (hdist(S.tube, feet) < 4.5f) add = std::max(add, 1.0);
        if (hdist(S.tube, feet) < 9.0f && (c.player->sprinting || c.noise >= 0.79)) add = std::max(add, 1.4);
        S.prov += add * dt;
        if (!S.roused && S.prov > 1.2) {
            S.roused = true, S.darkT = 0.0;
            out.push_back({Event::TakeOff, Role::Moth, S.tube});
            out.push_back({Event::Mimic, Role::Moth, S.tube, cueM(KC_MOTH_SWARM)});
            log::info("Praesenz: Deathmoth-Schwarm (Level 3) aufgescheucht");
            for (auto& o : agents_)
                if (o->group == a.group && o->variant >= 3 && o->variant <= 4 && o->state == 0) o->state = 2, o->stateT = 0.0, o->fly = 1.0f;
        }
        // Rueckzug: Lampe aus, weg vom Schwarm, oder weit weg
        if (S.roused) {
            vec3 ctr{};
            int nn = 0;
            for (const auto& o : agents_)
                if (o->group == a.group && o->variant >= 3 && o->variant <= 4) ctr += o->pos, ++nn;
            if (nn) ctr = ctr / (float)nn;
            int hunting = 0;
            for (const auto& o : agents_) hunting += (o->group == a.group && o->state >= 2 && o->state <= 4) ? 1 : 0;
            const bool losing = (!c.view.flashOn && (hdist(ctr, eye) > 6.0f || (S.lampOffT > 3.0 && hunting == 0))) || hdist(S.tube, feet) > 25.0f;
            S.darkT = losing ? S.darkT + dt : 0.0;
            if (S.darkT > 2.5) {
                S.roused = false, S.prov = 0.0;
                for (auto& o : agents_)
                    if (o->group == a.group && o->variant >= 3 && o->variant <= 4 && o->state >= 2 && o->state <= 5) o->state = 6, o->stateT = 0.0;
                log::info("Praesenz: Deathmoth-Schwarm verliert den Spieler (Licht aus, Abstand)");
            }
        }
        // Gesicht: Schaden je Sekunde nach der Zahl der Motten im Gesicht
        int on = 0;
        for (const auto& o : agents_) on += (o->group == a.group && o->state == 4) ? 1 : 0;
        S.faceCount = on;
        if (on > 0 && c.mode >= 2) {
            S.faceDmg += 0.9 * on * dt;
            S.faceTick -= dt;
            if (S.faceTick <= 0.0) {
                S.faceTick = 1.0;
                out.push_back({Event::Hit, Role::Moth, eye + fw * 0.08f, (float)S.faceDmg});
                out.push_back({Event::Mimic, Role::Moth, eye + fw * 0.08f, cueM(KC_MOTH_FACE)});
                S.faceDmg = 0.0;
            }
        } else {
            S.faceTick = 0.3, S.faceDmg = 0.0;
        }
    }
    auto openAt = [&](vec3 p) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        return t && t->open() && t->clearance() > 0.6f;
    };
    auto orbit = [&](vec3 tgt, float radius, float speedK, float jitter) {
        a.phase += (float)dt * (2.8f + 0.8f * radius) * a.spin;
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
            a.shiver = a.timer < 1.4 || S.prov > 0.4 ? 1.0f : 0.0f;
            if (a.timer <= 0.0) a.state = 1, a.stateT = 0.0, a.fly = 1.0f, a.timer2 = rng_.uniform(5.0, 14.0), a.vel = vec3{0, 0, -0.8f};
            return;
        }
        case 1: {  // um die Roehre (ruhig)
            a.fly = 1.0f;
            orbit(S.tube - vec3{0, 0, 0.15f}, a.radius, 0.8f, 5.0f);
            a.timer2 -= dt;
            if (a.timer2 <= 0.0) a.state = 6, a.stateT = 0.0;
            if (S.roused && S.lampOffT < 1.0) a.state = 2, a.stateT = 0.0;
            break;
        }
        case 2: {  // jagt: zum Licht vor der Lampe (lockt), sonst zum Kopf
            a.fly = 1.0f;
            if (S.lampOffT > 1.5 && dEye > 1.2f) {  // im Dunkeln verlieren sie ihn (Licht lockt sie - Wikidot Entity 4)
                a.state = 6, a.stateT = 0.0;
                break;
            }
            const vec3 tgt = c.view.flashOn ? c.view.flashPos + c.view.flashDir * 0.55f : eye + fw * 0.4f;
            orbit(tgt, 0.35f + 0.3f * a.radius, c.view.flashOn ? 1.15f : 0.8f, 7.0f);
            // vor dem Gesicht: ankuendigen (das Brummen schwillt) - einmal je 6 s und Spieler
            if (dEye < 1.3f && a.cool <= 0.0 && (c.view.flashOn || rng_.uniform(0.0, 1.0) < dt * 0.6)) {
                if (time_ - S.announcedT > 6.0) {
                    S.announcedT = time_;
                    out.push_back({Event::Hiss, Role::Moth, eye + fw * 0.4f, 1.0f});
                }
                a.state = 3, a.stateT = 0.0;
            }
            break;
        }
        case 3: {  // kuendigt an: schwirrt vor dem Gesicht
            a.fly = 1.0f;
            orbit(eye + fw * 0.45f, 0.18f + 0.2f * a.radius, 1.8f, 9.0f);
            if (a.stateT > 1.2) {
                // freier Platz im Gesicht
                bool taken[8]{};
                for (const auto& o : agents_)
                    if (o.get() != &a && o->group == a.group && o->state == 4 && o->ext) {
                        const int s = static_cast<const MothL3*>(o->ext.get())->slot;
                        if (s >= 0 && s < 8) taken[s] = true;
                    }
                int slot = -1;
                for (int k = 0; k < 8 && slot < 0; ++k)
                    if (!taken[(k + (int)(a.phase * 3.0f)) % 8]) slot = (k + (int)(a.phase * 3.0f)) % 8;
                if (slot >= 0 && dEye < 1.0f) {
                    M.slot = slot;
                    a.state = 4, a.stateT = 0.0;
                } else if (a.stateT > 3.0) {
                    a.state = 2, a.stateT = 0.0, a.cool = 1.0;
                }
            }
            break;
        }
        case 4: {  // im Gesicht: krallt sich fest, kriecht zu Mund, Nase, Augen, Ohren
            const float* s = kSlot[std::clamp(M.slot, 0, 7)];
            const float wob = 0.008f * (float)std::sin(time_ * 31.0 + a.phase);
            const vec3 want = eye + fw * (s[0] + wob) + rt * s[1] + up * s[2];
            a.vel = {};
            a.pos = a.pos + (want - a.pos) * (float)std::min(1.0, dt * 14.0);
            a.fly = 0.55f + 0.35f * (float)std::sin(time_ * 19.0 + a.phase);
            a.shiver = 1.0f;
            a.up = nrmF(a.pos - (eye - fw * 0.04f), fw), a.fwd = nrmF(eye - fw * 0.02f - up * 0.05f - a.pos, -fw);
            a.yaw = std::atan2(a.fwd.y, a.fwd.x);
            // loslassen: Lampe aus (nach 0,8 s), Rennen schuettelt sie ab, dunkel macht sie unsicher
            double rate = 0.12;
            if (S.lampOffT > 0.8) rate += 0.55;
            if (c.player->sprinting) rate += 0.45;
            const world::Tile* pt = c.world->tileIfLoaded(ifloor(feet.x), ifloor(feet.y));
            if (darkTile(c, pt, feet.x, feet.y) && !c.view.flashOn) rate += 0.2;
            if (rng_.uniform(0.0, 1.0) < rate * dt) {
                a.state = 5, a.stateT = 0.0, M.knockT = rng_.uniform(2.0, 3.2);
                a.vel = nrmF(a.pos - eye, rt) * 2.6f + vec3{0, 0, 0.6f};
                if (rng_.uniform(0.0, 1.0) < 0.4) out.push_back({Event::Mimic, Role::Moth, a.pos, cueM(KC_MOTH_SHAKE)});
            }
            return;
        }
        case 5: {  // abgeschuettelt: taumelt
            a.fly = 1.0f;
            a.vel += vec3{(float)rng_.uniform(-6.0, 6.0), (float)rng_.uniform(-6.0, 6.0), (float)rng_.uniform(-3.0, 2.0)} * (float)dt;
            a.vel = a.vel * (1.0f - 0.8f * (float)dt);
            if (a.stateT > M.knockT) a.state = S.roused && S.lampOffT < 1.0 ? 2 : 6, a.stateT = 0.0, a.cool = 1.5;
            break;
        }
        case 6: {  // zurueck an den Platz
            a.fly = 1.0f;
            const vec3 home = a.rest + a.restN * 0.1f;
            toward(home, 2.0f);
            if (length(home - a.pos) < 0.12f || (a.stateT > 20.0 && !a.sight.inView)) {
                a.state = 0, a.stateT = 0.0, a.pos = a.rest, a.timer = rng_.uniform(10.0, 30.0);
                return;
            }
            if (S.roused && S.lampOffT < 1.0) a.state = 2, a.stateT = 0.0;
            break;
        }
        default: break;
    }
    // Waende: abprallen; zwischen Boden und Decke bleiben
    vec3 np = a.pos + a.vel * (float)dt;
    if (!openAt(np)) {
        if (!openAt({np.x, a.pos.y, a.pos.z})) a.vel.x = -a.vel.x * 0.6f;
        if (!openAt({a.pos.x, np.y, a.pos.z})) a.vel.y = -a.vel.y * 0.6f;
        np = a.pos + a.vel * (float)dt;
        if (!openAt(np)) np = {a.pos.x, a.pos.y, np.z};
    }
    if (const world::Tile* t = c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)); t && t->open()) {
        const float lo = t->floor + 0.3f, hi = t->ceil - (a.state == 6 ? 0.03f : 0.1f);
        if (np.z > hi) np.z = std::max(hi, a.pos.z - (float)dt * 0.5f), a.vel.z = -std::fabs(a.vel.z) * 0.5f;
        if (np.z < lo) np.z = std::min(lo, a.pos.z + (float)dt * 0.5f), a.vel.z = std::fabs(a.vel.z) * 0.5f;
    }
    a.pos = np;
    if (length(flat(a.vel)) > 0.05f) a.yaw = std::atan2(a.vel.y, a.vel.x);
    a.up = {0, 0, 1};
    a.fwd = {};
    if (hdist(a.pos, eye) > 50.0f && a.unseenT > 4.0) a.state = 9;
}

// --- Praetorianer --------------------------------------------------------------------------------------------------------
void Presence::thinkPraetorian(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    auto* Sp = static_cast<Swarm*>(l3Swarm_.get());
    PraeL3& P = extOf<PraeL3>(a);
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, feet);
    a.stateT += dt;
    P.t += dt;
    P.coolT -= dt;
    P.spitT -= dt;
    a.fly = 0.0f, a.up = {0, 0, 1}, a.fwd = {};
    a.crouch = 0.0f, a.jaw = 0.0f, a.alert = 0.0f;
    a.look = true, a.lookAt = eye;
    a.perceive = 0.0f;
    const vec3 head = a.cre.headPos();
    lightReact(a, c, head, 9.0f, dt, 0.6);  // zuckt im Licht kurz zurueck - laesst sich aber nicht vertreiben
    if (a.cre.stepped() && d < 16.0f && rng_.uniform(0.0, 1.0) < 0.5) out.push_back({Event::Mimic, Role::Moth, a.cre.stepPos(), cueM(KC_PR_CLICK)});
    const bool los = clearSight(*c.world, head, eye);
    const bool roused = Sp && Sp->roused;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0, P.warned = false, P.hit = false; };
    auto face = [&](vec3 at, double rate) {
        const vec3 to = nrmF(flat(at - a.pos), dirYaw(a.yaw));
        a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * rate);
    };
    auto goTo = [&](vec3 goal, double speed) {
        if (!a.hasGoal || hdist(goal, a.goal) > 0.8f || a.timer2 <= 0.0) {
            a.field.build(*c.world, ifloor(goal.x), ifloor(goal.y), 1.1f);
            a.goal = goal, a.hasGoal = true, a.timer2 = 1.0;
        }
        a.timer2 -= dt;
        if (!walkTo(a, a.field, speed, dt, c)) {
            const vec3 to = nrmF(flat(goal - a.pos), dirYaw(a.yaw));
            const vec3 np = a.pos + to * (float)(speed * dt);
            if (figureWalkable(c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)), 1.1f)) a.pos = np;
            face(goal, 6.0);
        }
    };
    // wie viele greifen schon an (hoechstens zwei)
    int engaged = 0;
    for (const auto& o : agents_)
        if (o.get() != &a && o->role == Role::Moth && o->variant == 5 && o->state >= 2 && o->state <= 6) ++engaged;
    if ((d > 50.0f && a.unseenT > 5.0) || (!Sp && a.unseenT > 3.0)) {
        a.state = 9;
        return;
    }
    switch (a.state) {
        case 0: {  // wacht am Boden
            face(eye, 0.8);
            P.fanT -= dt;
            a.shiver = P.fanT < 1.4 ? 1.0f : 0.0f;
            if (P.fanT <= 0.0) P.fanT = rng_.uniform(6.0, 14.0);
            a.perceive = 1.0f, a.perceiveAt = eye;
            const bool sees = los && (d < 9.0f || (c.view.flashOn && d < 15.0f));
            if (roused || sees || (c.player->sprinting && d < 12.0f)) setState(1);
            break;
        }
        case 1: {  // alarmiert: faechelt Lockstoff (ruft die anderen), dann los
            face(eye, 4.0);
            a.shiver = 1.0f, a.alert = 1.0f;
            if (!P.warned) {
                P.warned = true;
                out.push_back({Event::Mimic, Role::Moth, head, cueM(KC_PR_FAN)});
                if (Sp && !Sp->roused) Sp->prov = std::max(Sp->prov, 1.3);  // Lockstoff: der Schwarm erwacht
            }
            if (a.stateT > 1.5) {
                if (engaged >= 2) {  // genug sind schon dran: wacht weiter (faechelt)
                    setState(0);
                    P.fanT = 2.0;
                } else {
                    P.flanker = engaged == 1 || rng_.uniform(0.0, 1.0) < 0.35;  // einer nimmt den Umweg
                    P.detoured = false;
                    setState(2);
                }
            }
            break;
        }
        case 2: {  // naht (Umweg: ungesehen fort, hinter dem Spieler wieder da)
            a.crouch = d > 5.0f ? 0.6f : 0.25f;
            a.alert = 1.0f;
            if (P.flanker && !P.detoured && a.unseenT > 1.0 && d > 6.0f) {
                vec3 q;
                if (hiddenSpawn(c, 50, 110, true, q, [&](vec3 p) { return figureWalkable(c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y)), 1.1f); })) {
                    a.snapBody(q, std::atan2(eye.y - q.y, eye.x - q.x));
                    a.hasGoal = false;
                    log::info("Praesenz: Praetorianer nimmt eine Noclip-Abkuerzung - hinter dem Spieler bei ({:.1f}, {:.1f})", q.x, q.y);
                }
                P.detoured = true;
            }
            goTo(feet, d > 4.0f ? 2.3 : 1.2);
            if (d < 2.4f && P.coolT <= 0.0 && los) setState(3);
            else if (d > 3.0f && d < 6.0f && P.spitT <= 0.0 && los) setState(6);
            if (!roused && d > 14.0f && a.stateT > 8.0) setState(7);  // der Schwarm hat aufgegeben: zurueck
            break;
        }
        case 3: {  // baeumt sich auf (Ankuendigung)
            face(eye, 6.0);
            a.strike = (float)std::min(0.5, a.stateT / 0.8 * 0.5);
            a.reach = 1.0f;
            if (!P.warned) {
                P.warned = true;
                out.push_back({Event::Mimic, Role::Moth, head, cueM(KC_PR_REAR)});
            }
            if (a.stateT > 0.8) setState(4);
            break;
        }
        case 4: {  // sticht
            face(eye, 6.0);
            a.reach = 1.0f;
            a.strike = (float)std::min(1.0, 0.5 + a.stateT / 0.32 * 0.5);
            if (!P.hit && a.strike >= 0.6f) {
                P.hit = true;
                const vec3 chest = eye - vec3{0, 0, 0.45f};
                const bool hit = (length(chest - (a.pos + vec3{0, 0, 0.7f})) < 2.1f) && c.mode >= 2;
                out.push_back({Event::Mimic, Role::Moth, head + nrmF(eye - head, {1, 0, 0}) * 0.5f, cueM(KC_PR_STAB)});
                if (hit) {
                    out.push_back({Event::Hit, Role::Moth, chest, 9.0f});
                    addDose(a.target, 0.45f);
                }
            }
            if (a.stateT > 0.5) setState(5), P.coolT = rng_.uniform(2.6, 4.2);
            break;
        }
        case 5: {  // weicht ab
            a.strike = 0.0f;
            const vec3 away = nrmF(flat(a.pos - feet), dirYaw(a.yaw + kPiF));
            const vec3 np = a.pos + away * (float)(1.4 * dt);
            if (figureWalkable(c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)), 1.1f)) a.pos = np;
            face(eye, 5.0);
            if (a.stateT > 1.5) setState(2);
            break;
        }
        case 6: {  // spuckt aetzende Fluessigkeit
            face(eye, 6.0);
            a.perceive = 1.0f, a.perceiveAt = eye;
            a.jaw = a.stateT > 0.6 ? 1.0f : 0.0f;
            a.crouch = a.stateT < 0.6 ? 0.4f : 0.0f;  // zieht den Kopf zurueck (Ankuendigung)
            if (!P.warned) P.warned = true, out.push_back({Event::Hiss, Role::Moth, head, 0.6f});
            if (!P.hit && a.stateT > 0.6) {
                P.hit = true;
                out.push_back({Event::Mimic, Role::Moth, head, cueM(KC_PR_SPIT)});
                const vec3 to = nrmF(eye - head, {1, 0, 0});
                const bool aimed = dot(to, nrmF(flat(dirYaw(a.yaw)), {1, 0, 0})) > 0.7f && clearSight(*c.world, head, eye) && length(eye - head) < 7.0f;
                if (aimed && c.mode >= 2 && rng_.uniform(0.0, 1.0) < 0.7) out.push_back({Event::Hit, Role::Moth, eye, 5.0f});
            }
            if (a.stateT > 1.0) setState(2), P.spitT = rng_.uniform(6.0, 10.0);
            break;
        }
        case 7: {  // zurueck auf den Posten
            goTo(P.post, 1.4);
            if (hdist(a.pos, P.post) < 0.6f || a.stateT > 15.0) setState(0);
            if (roused) setState(2);
            break;
        }
        default: break;
    }
}

}  // namespace lim::game
