// V13.3: Growler (Entity 130, Wikidot trimmed:entity-130; KREATUR_Growler.md) - Auftreten und Verhalten.
//
// Blind - er ortet mit Klicks (Echoortung): was sich bewegt, findet er auf 7 (jung) bis 13 m (ausgewachsen) mit freier
// Bahn; wer still steht, nur ganz nah. Licht ist ihm gleich. Rennen hoert er weiter. Seine Waffe ist ein telepathisches
// Rauschen (10 bis 80 m, mit der Groesse): Bild und Klang verzerren sich, nah zehrt es am Verstand, grosse schaden dem
// Kopf (Health). Wer genau hinsieht, dem wird es staerker ("nicht auf das Rauschen konzentrieren").
//
//   0 Wandern    ziellos, torkelnd; klickt alle 2,5-4 s
//   1 Lauschen   bleibt stehen, der Knoten dreht sich zum Geraeusch, schnelle Klicks
//   2 Jagd       torkelt schnell zur letzten Ortung; verliert er sie (6 s nichts), wandert er weiter (er ist dumm)
//   3 Ausholen   richtet sich auf, die Drahtrippen straeuben sich, das Rauschen schwillt (1,1 s - die Ankuendigung)
//   4 Stoss      stoesst vor, umschlingt, spiesst auf: ausgewachsen toedlich (danach oeffnet er die Brust, 10 = Zugriff),
//                jung ein Treffer (14 Health) und er laesst ab
//   6 Verwirrt   ein geworfener Gegenstand trifft ihn am Kopf (Level 1: schwere Gegenstaende verwirren ihn): 5-7 s Taumeln
//   7 Verschmelzen  zwei treffen sich: der groessere nimmt den kleineren auf und waechst (8: wird aufgenommen)
//   11 Steckt fest  (Level 2) zu gross fuer den Gang: zappelt, verhungert, bricht zusammen
//   12 Leiche    zusammengesunken, harmlos; versperrt halb den Gang (wer hindurch will, watet durch Draht)
// Ebenen (Level-JSON presence.growler): mode "adult" (Level 1, 5) einzeln; "young" (Level 2) klein, zu zweit oder dritt,
// leuchtend bunt (im Dunkeln zu sehen), verschmelzen in engen Gaengen; chance: Anteil der Leichen.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
struct GAg {
    float scale = 1.0f, scaleT = 1.0f;
    bool young = false;
    int merges = 0;
    double clickT = 1.0, staticT = 0.5, confT = 0.0, starveT = 0.0, lurchT = 0.0, strikeT = 0.0, whipT = 0.0;
    vec3 echo;
    double echoAge = 1e9;
    u16 partner = 0;
    float merge = 0.0f;
    bool missed = false;
};
// Vorlage: Presence::Agent ist privat (wie lightReact)
template <class A>
GAg& gag(A& a) {
    if (!a.ext) a.ext = std::make_shared<GAg>();
    return *static_cast<GAg*>(a.ext.get());
}
constexpr int kWander = 0, kListen = 1, kHunt = 2, kWindup = 3, kStrike = 4, kConfused = 6, kMerge = 7, kAbsorbed = 8, kStuck = 11, kCorpse = 12;
// Reichweite des Rauschens (m) nach der Groesse: jung 10 m, ausgewachsen ~35 m, gross bis 80 m (die Seite: 10-80 m)
float staticRange(float s) { return std::clamp(10.0f + 50.0f * (s - 0.5f), 10.0f, 80.0f); }
}  // namespace

float Presence::growlerScale(int i) const {
    for (const auto& a : agents_)
        if (a->role == Role::Growler && !a->showcase && i-- == 0) return a->ext ? static_cast<const GAg*>(a->ext.get())->scale : 1.0f;
    return 0.0f;
}

// Groesse, Leuchten (Junge im Dunkeln), Rauschen beim eigenen Spieler; Netz der Spinnen: presence_nguithrxurh.cpp
void Presence::prepGN(Agent& a, const Context& c) {
    if (a.role == Role::Nguithr) {
        prepNguithr(a, c);
        return;
    }
    float s;
    if (a.puppet) s = 0.3f + 1.5f * a.fly;  // Koop: Groesse im Kanal fly
    else if (a.showcase) s = a.variant == 1 ? 0.5f : (a.variant == 2 ? 1.4f : 1.0f);
    else s = gag(a).scale;
    if (!a.puppet) a.fly = std::clamp((s - 0.3f) / 1.5f, 0.0f, 1.0f);
    a.cre.setScale(s);
    a.rig.setGlow(a.glow);
    // Rauschen beim Spieler dieses Rechners (durch Waende - es ist telepathisch); sieht man ihn an, wird es staerker
    if (a.state != kCorpse && a.state != kAbsorbed && !a.showcase) {
        const float d = length(a.pos + vec3{0, 0, 1.2f * s} - selfEye_);
        const float range = staticRange(s);
        float lvl = std::pow(std::clamp(1.0f - d / range, 0.0f, 1.0f), 1.4f);
        lvl *= (a.state >= kHunt && a.state <= kStrike) || a.state == 10 ? 1.0f : 0.55f;
        if (a.state == kConfused) lvl *= 0.4f + 0.6f * (float)(std::fmod(time_ * 7.3, 1.0) > 0.5);  // stottert
        if (a.sight.direct) lvl *= 1.3f;
        static_ = std::clamp(std::max(static_, lvl), 0.0f, 1.0f);
    }
    (void)c;
}

void Presence::preGN(double dt, const Context& c) {
    static_ = 0.0f;
    selfEye_ = c.view.eye;
    // Betaeubung (Nguithr'xurh) klingt ab - langsam, schneller in Bewegung ("den Koerper schuetteln")
    for (size_t i = 0; i < stupor_.size(); ++i) {
        if (stuporHold_[i] > 0.0) {
            stuporHold_[i] -= dt;
            continue;
        }
        const Player* p = (int)i == c.selfId ? c.player : nullptr;
        if (!p && c.others)
            for (const Other& o : *c.others)
                if (o.id == (int)i) p = o.player;
        const double rate = 0.028 + (p && p->groundSpeed > 0.5 ? 0.02 : 0.0);
        stupor_[i] = (float)std::max(0.0, (double)stupor_[i] - dt * rate - (p && p->jumped ? 0.05 : 0.0));
    }
}

bool Presence::spawnGrowler(const Context& c, std::vector<Event>& out, bool forced) {
    world::KinDef K;
    if (c.kin && c.kin[world::KIN_GROWLER].on) K = c.kin[world::KIN_GROWLER];
    else if (!forced) return false;
    const bool young = K.mode == "young";
    vec3 p;
    const auto allowed = [&](vec3 q) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
        return roomAllowed(c, q, world::KIN_GROWLER) && t && t->clearance() >= (young ? 1.6f : 2.3f);
    };
    if (!hiddenSpawn(c, forced ? 100 : 150, forced ? 220 : 320, false, p, allowed) && !hiddenSpawn(c, 100, 900, false, p, allowed)) return false;
    const int n = young ? (int)rng_.randint(std::max(2, K.groupMin), std::max(2, K.groupMax)) : 1;
    const int group = nextGroup_++;
    int made = 0;
    for (int i = 0; i < n; ++i) {
        vec3 q = p;
        if (i > 0) {  // Junge: die anderen ein paar Meter weiter (sie finden sich und verschmelzen)
            bool found = false;
            for (int t = 0; t < 16 && !found; ++t) {
                const vec3 c2 = p + vec3{(float)rng_.uniform(-5.0, 5.0), (float)rng_.uniform(-5.0, 5.0), 0.0f};
                const world::Tile* tl = c.world->tileIfLoaded(ifloor(c2.x), ifloor(c2.y));
                if (figureWalkable(tl, 1.5f) && std::fabs(tl->floor - p.z) < 0.4f && hdist(c2, p) > 2.5f) q = {c2.x, c2.y, tl->floor}, found = true;
            }
            if (!found) continue;
        }
        auto a = std::make_unique<Agent>();
        a->role = Role::Growler;
        a->ent = Entity::Growler;
        a->group = group;
        GAg& g = gag(*a);
        g.young = young;
        g.scale = g.scaleT = young ? (float)rng_.uniform(0.45, 0.58) : (float)rng_.uniform(0.92, 1.12);
        a->glow = young ? 1.0f : 0.0f;  // Junge: leuchtend bunt, im Dunkeln zu sehen (Level 2)
        a->initBody(q, (float)rng_.uniform(-kPiF, kPiF), (unsigned)rng_.randint(1, 1 << 30));
        a->cre.setScale(g.scale);
        a->rig.setGlow(a->glow);
        a->timer2 = rng_.uniform(0.5, 2.5);
        g.clickT = rng_.uniform(0.5, 3.0);
        out.push_back({Event::Appear, Role::Growler, q});
        agents_.push_back(std::move(a));
        ++made;
    }
    log::info("Praesenz: Growler x{} ({}) bei ({:.1f}, {:.1f}), {:.0f} m", made, young ? "jung" : "ausgewachsen", p.x, p.y, hdist(p, c.view.eye));
    return made > 0;
}

void Presence::regieGrowler(double dt, const Context& c, std::vector<Event>& out) {
    if (!c.kin || !c.kin[world::KIN_GROWLER].on) return;
    const world::KinDef& K = c.kin[world::KIN_GROWLER];
    int alive = 0, corpses = 0;
    for (const auto& a : agents_)
        if (a->role == Role::Growler && !a->showcase) (a->state == kCorpse ? corpses : alive) += 1;
    if (alive == 0) {
        nextGrowler_ -= dt;
        if (nextGrowler_ <= 0.0) nextGrowler_ = spawnGrowler(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 8.0;
    }
    // Level 2: Leichen sind haeufig (die Seite) - ab und zu liegt eine in einem Gang, ausser Sicht
    if (K.mode == "young" && K.chance > 0.0 && corpses < 2) {
        corpseT_ -= dt;
        vec3 p;
        const auto corridor = [&](vec3 q) {
            const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
            return t && t->room && t->room->isCorridor && t->clearance() >= 2.0f;
        };
        if (corpseT_ <= 0.0) {
            corpseT_ = rng_.uniform(60.0, 120.0);
            if (rng_.uniform(0.0, 1.0) < K.chance && hiddenSpawn(c, 120, 300, false, p, corridor)) {
                auto a = std::make_unique<Agent>();
                a->role = Role::Growler;
                a->ent = Entity::Growler;
                GAg& g = gag(*a);
                g.young = true;
                g.scale = g.scaleT = (float)rng_.uniform(0.9, 1.35);  // verschmolzen und steckengeblieben
                a->state = kCorpse;
                a->hang = 1.0f;
                a->initBody(p, (float)rng_.uniform(-kPiF, kPiF), (unsigned)rng_.randint(1, 1 << 30));
                a->cre.setScale(g.scale);
                anim::CreatureAnimator::Input in;  // gleich zusammengesunken
                in.pos = p, in.yaw = a->yaw, in.hang = 1.0f;
                a->cre.setScale(g.scale);
                for (int it = 0; it < 60; ++it) a->cre.update(0.1, in);
                a->rig.setCreaturePose(a->cre.pose());
                log::info("Praesenz: Growler-Leiche im Gang bei ({:.1f}, {:.1f})", p.x, p.y);
                agents_.push_back(std::move(a));
            }
        }
    }
}

void Presence::thinkGrowler(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    GAg& g = gag(a);
    if (a.state == 10) {  // Zugriff: spiesst auf, oeffnet die Brust (Kamera des Schrecks)
        killPose(a, dt, c);
        a.crouch = 0.0f, a.strike = 0.0f;
        g.scaleT = std::min(g.scaleT, 1.7f);
        prepGN(a, c);
        return;
    }
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, eye);
    const float s = g.scale;
    const vec3 knot = a.cre.headPos();
    a.stateT += dt;
    a.cool -= dt;
    g.echoAge += dt;
    // Groesse folgt langsam (Wachsen nach dem Aufnehmen, Verschmelzen)
    g.scale += (g.scaleT - g.scale) * (float)std::min(1.0, dt * 0.8);
    a.look = false;
    a.alert = 0.0f, a.perceive = 0.0f, a.reach = 0.0f, a.crouch = 0.0f, a.jaw = 0.0f, a.shiver = 0.0f, a.strike = 0.0f;
    a.growl = std::clamp((float)(0.25 + 0.75 * (a.state >= kHunt && a.state <= kStrike)), 0.0f, 1.0f);
    auto setState = [&](int st) { a.state = st, a.stateT = 0.0; };
    // Junge leuchten (im Dunkeln zu sehen); eine Leiche erlischt
    a.glow = g.young && a.state != kCorpse ? 1.0f : 0.0f;
    // --- Leiche --------------------------------------------------------------------------------------------------
    if (a.state == kCorpse) {
        a.hang = 1.0f, a.growl = 0.0f;
        if ((d > 60.0f && a.unseenT > 3.0) || (a.age > 900.0 && a.unseenT > 10.0)) a.state = 9;
        prepGN(a, c);
        return;
    }
    a.hang = 0.0f;
    // --- Wahrnehmung -------------------------------------------------------------------------------------------
    // Klicks (Echoortung): Bewegung findet er bis echoR mit freier Bahn; Stillstehende nur ganz nah. Licht zaehlt nicht.
    const float echoR = (g.young ? 7.0f : 13.0f) * std::sqrt(std::max(0.5f, s));
    const bool moves = c.player->gait > 0.3 || c.player->sprinting || c.player->groundSpeed > 0.5;
    const float still = 3.2f * std::sqrt(s);
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    bool hit = false;
    g.clickT -= dt;
    if (g.clickT <= 0.0 && a.state != kStuck && a.state != kMerge && a.state != kAbsorbed) {
        const bool busy = a.state == kHunt || a.state == kListen;
        g.clickT = busy ? rng_.uniform(0.6, 1.0) : rng_.uniform(2.5, 4.0);
        out.push_back({Event::Click, Role::Growler, knot, std::clamp(s / 1.4f, 0.2f, 1.0f)});
        const vec3 chest = feet + vec3{0, 0, 1.1f};
        if (d < echoR && (moves || d < still) && clearSight(*c.world, knot, chest)) hit = true;
    }
    // Hoeren: Rennen, Gehen, Landen (wie das Buendel: Laerm x 22 m Weg); geworfene Dinge
    bool heard = c.noise > 0.01 && pathD < (float)c.noise * (g.young ? 14.0f : 22.0f);
    if (heard) g.echo = feet, g.echoAge = std::min(g.echoAge, 0.5);
    if (hit) g.echo = feet, g.echoAge = 0.0;
    if (c.pings)
        for (const auto& [pp, loud] : *c.pings) {
            const float dh = hdist(pp, a.pos);
            // trifft ihn ein geworfener Gegenstand (am Koerper, am Kopf): verwirrt (Level-1-Seite)
            if (dh < 1.1f * s && std::fabs(pp.z - (a.pos.z + 1.0f * s)) < 1.4f * s && a.state != kConfused && a.state != kMerge) {
                setState(kConfused);
                g.confT = rng_.uniform(5.0, 7.0);
                out.push_back({Event::Collapse, Role::Growler, knot, 0.3f});
                log::info("Praesenz: Growler getroffen - verwirrt");
            } else if (dh < 14.0f && a.state <= kHunt) {  // sonst: er geht dem Aufprall nach
                g.echo = pp, g.echoAge = 0.0;
                if (a.state == kWander) setState(kListen);
            }
        }
    // Rauschen schadet aus der Naehe (grosse: Health, alle: Verstand) - je 0,5 s ein Takt
    g.staticT -= dt;
    if (g.staticT <= 0.0) {
        g.staticT = 0.5;
        const float near = std::clamp(1.0f - d / (6.0f * std::sqrt(s)), 0.0f, 1.0f);
        if (near > 0.0f && c.mode >= 2) out.push_back({Event::Static, Role::Growler, knot, s >= 0.9f ? 0.6f * s * near : 0.0f});
    }
    // --- Verschmelzen: zwei treffen sich (nicht jagend) --------------------------------------------------------
    if ((a.state == kWander || a.state == kListen) && a.cool <= 0.0)
        for (auto& o : agents_) {
            if (o.get() == &a || o->role != Role::Growler || o->showcase || o->puppet) continue;
            if (o->state != kWander && o->state != kListen) continue;
            if (hdist(o->pos, a.pos) > 1.4f * std::max(s, gag(*o).scale)) continue;
            // der groessere nimmt auf
            Agent& big = gag(*o).scale > s ? *o : a;
            Agent& small = &big == &a ? *o : a;
            gag(big).partner = small.netId, gag(small).partner = big.netId;
            big.state = kMerge, big.stateT = 0.0;
            small.state = kAbsorbed, small.stateT = 0.0;
            const float s1 = gag(big).scale, s2 = gag(small).scale;
            gag(big).scaleT = std::min(1.8f, std::cbrt(s1 * s1 * s1 + s2 * s2 * s2) * 1.08f);
            ++gag(big).merges;
            out.push_back({Event::Merge, Role::Growler, (big.pos + small.pos) * 0.5f + vec3{0, 0, 1.0f * s1}, std::clamp(gag(big).scaleT / 1.4f, 0.3f, 1.0f)});
            log::info("Praesenz: zwei Growler verschmelzen ({:.2f} + {:.2f} -> {:.2f})", s1, s2, gag(big).scaleT);
            break;
        }
    // Steckt fest (Level 2): zu gross fuer den Gang - er bekommt Kopf und Ruecken nicht mehr hindurch
    if (g.young && a.state != kStuck && a.state != kMerge && a.state != kAbsorbed && s > 1.1f)
        if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)); t && t->room && t->room->isCorridor && t->clearance() < 2.15f * s) {
            setState(kStuck);
            g.starveT = rng_.uniform(70.0, 100.0);
            log::info("Praesenz: Growler ({:.2f}) steckt im Gang fest", s);
        }
    // --- Zustaende ---------------------------------------------------------------------------------------------
    const double baseSpeed = g.young ? 1.9 : (s > 1.25f ? 3.4 : 3.0);
    const double surge = 0.72 + 0.56 * std::pow(std::max(0.0, std::sin(time_ * 2.6 + a.age)), 2.0);  // torkelnd
    switch (a.state) {
        case kWander: {
            if (hit || heard) {
                setState(kListen);
                break;
            }
            wander(a, dt, c, 0.65, world::KIN_GROWLER);
            break;
        }
        case kListen: {  // stehen, lauschen, klicken - dann los
            a.perceive = 1.0f;
            a.perceiveAt = g.echo + vec3{0, 0, 1.0f};
            const vec3 to = nrmF(flat(g.echo - a.pos), dirYaw(a.yaw));
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 2.0);
            if (a.stateT > 1.2 && g.echoAge < 1.5) {
                setState(kHunt);
                a.field.build(*c.world, ifloor(g.echo.x), ifloor(g.echo.y), 1.6f);
            } else if (a.stateT > 4.0) {
                setState(kWander);
            }
            break;
        }
        case kHunt: {
            a.alert = 1.0f;
            a.look = true;
            a.lookAt = g.echo + vec3{0, 0, 1.0f};
            // frische Ortung: dem Weg zum Spieler folgen; sonst zum Ort der letzten Ortung
            const bool fresh = g.echoAge < 1.2 && hdist(g.echo, feet) < 1.5f;
            bool moved;
            if (fresh) moved = walkTo(a, fld(), baseSpeed * surge, dt, c);
            else {
                if (std::fmod(a.stateT, 1.0) < dt) a.field.build(*c.world, ifloor(g.echo.x), ifloor(g.echo.y), 1.6f);
                moved = hdist(a.pos, g.echo) > 1.0f && walkTo(a, a.field, baseSpeed * surge * 0.8, dt, c);
            }
            // nah genug: ausholen (die Ankuendigung)
            if (d < 2.3f * std::sqrt(s) && fresh && std::fabs(a.pos.z - (float)c.player->z) < 1.2f && c.mode >= 2 && a.cool <= 0.0) {
                setState(kWindup);
                out.push_back({Event::Lurch, Role::Growler, knot + vec3{0, 0, 0.3f * s}, std::clamp(s, 0.3f, 1.0f)});
                break;
            }
            if (!moved && !fresh && g.echoAge > 2.0) setState(kListen);
            if (g.echoAge > 6.0) setState(kWander), a.hasGoal = false;  // verloren - er ist dumm
            break;
        }
        case kWindup: {  // aufrichten, Arme breit, Rippen straeuben sich (1,1 s)
            a.alert = 1.0f, a.crouch = 1.0f, a.growl = 1.0f;
            a.look = true, a.lookAt = eye;
            const vec3 to = nrmF(flat(eye - a.pos), dirYaw(a.yaw));
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 3.0);
            if (a.stateT > 1.1) {
                setState(kStrike);
                g.whipT = 0.0;
                g.missed = false;
            }
            break;
        }
        case kStrike: {  // Stoss: umschlingen, aufspiessen, Ranken peitschen
            a.alert = 1.0f, a.reach = 1.0f, a.growl = 1.0f;
            g.whipT += dt / 0.7;
            a.strike = (float)std::min(g.whipT, 0.999);
            a.look = true, a.lookAt = eye;
            if (a.stateT < 0.25) walkTo(a, fld(), 4.5 * std::sqrt(s), dt, c);  // der Satz nach vorn
            if (a.stateT >= 0.3 && a.stateT - dt < 0.3) {
                const bool inReach = d < 1.9f * std::sqrt(s) && std::fabs(a.pos.z - (float)c.player->z) < 1.2f;
                if (inReach && c.mode >= 2) {
                    if (g.young) {  // jung: ein Treffer, dann laesst er ab
                        out.push_back({Event::Hit, Role::Growler, knot, 14.0f});
                        a.cool = 4.0;
                    } else {
                        out.push_back({Event::Attack, Role::Growler, knot, 1.0f});
                        startKill(a, c);
                        g.scaleT = std::min(1.8f, g.scaleT + 0.08f);  // er waechst (die Seite: neue Pfeifenreiniger)
                        break;
                    }
                } else {
                    g.missed = true;
                }
            }
            if (a.stateT > 0.8) {
                setState(g.missed ? kHunt : kWander);
                a.cool = g.missed ? 1.5 : 4.0;
                a.strike = 0.0f;
                if (!g.missed) pickGoal(c, a, 200, 340, true);
            }
            break;
        }
        case kConfused: {  // taumelt, schlaegt um sich; danach zieht er sich zurueck
            a.shiver = 1.0f, a.growl = 0.6f;
            g.confT -= dt;
            if (std::fmod(a.stateT, 0.8) < dt) a.yaw += (float)rng_.uniform(-1.2, 1.2);
            if (g.confT <= 0.0) {
                setState(kWander);
                g.echoAge = 1e9;
                a.cool = 6.0;
                pickGoal(c, a, 240, 380, true);
            }
            break;
        }
        case kMerge: {  // nimmt den anderen auf (3 s): Ranken und Arme greifen hinueber, er zittert
            a.perceive = 1.0f, a.reach = 0.5f, a.growl = 1.0f, a.shiver = 0.35f;
            for (auto& o : agents_)
                if (o->netId == g.partner && o.get() != &a) a.perceiveAt = o->pos + vec3{0, 0, 1.0f * gag(*o).scale};
            if (a.stateT > 3.0) setState(kWander), a.cool = 8.0;
            break;
        }
        case kAbsorbed: {  // wird aufgenommen: schrumpft in den anderen hinein
            a.shiver = 1.0f, a.growl = 1.0f;
            Agent* big = nullptr;
            for (auto& o : agents_)
                if (o->netId == g.partner && o.get() != &a) big = o.get();
            if (big) a.pos = a.pos + (big->pos - a.pos) * (float)std::min(1.0, dt * 1.2);
            g.scaleT = g.scale = std::max(0.05f, g.scale - (float)dt * 0.35f);
            if (a.stateT > 2.8 || !big) a.state = 9;
            break;
        }
        case kStuck: {  // zappelt im Gang, verhungert
            a.shiver = 0.7f, a.alert = 0.5f;
            a.crouch = 0.3f + 0.3f * (float)std::sin(a.stateT * 1.3);
            g.starveT -= dt;
            if (g.starveT <= 0.0) {
                a.state = kCorpse, a.stateT = 0.0;
                out.push_back({Event::Collapse, Role::Growler, knot, 1.0f});
                log::info("Praesenz: der festgesteckte Growler ist verhungert");
            }
            break;
        }
        default: break;
    }
    // fort, wenn weit weg und ungesehen
    if (a.state == kWander && ((d > 55.0f && a.unseenT > 3.0) || (a.age > 480.0 && a.unseenT > 5.0))) a.state = 9;
    prepGN(a, c);
}

}  // namespace lim::game
