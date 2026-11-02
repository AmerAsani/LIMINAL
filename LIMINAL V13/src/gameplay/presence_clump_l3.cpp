// V13.3 Level 3: das grosse, bewaffnete Buendel (KREATUR_Clump.md, Abschnitt "Level 3"; Wikidot Level 3 Rev. 18, Entity 5
// Rev. 38). Level-JSON presence.clump_l3 (every); Rolle Clump, Variante 1 (bewaffnet: Rohr, Knochenspiess, Knochensichel) und
// 2 (entwaffnet: es hat sich einen zweiten Knochen freigelegt). Das gewoehnliche Buendel (thinkClump) bleibt, wie es ist.
//
// Die Seiten: in Level 3 viel groesser (gestreckt bis 8 ft), Muskeln wesentlich staerker, die Arme geordnet und koordiniert,
// schneller, wendiger, kluger; jagt typisch an der Decke kletternd und an Haengendem schwingend; traegt oft mehrere Waffen
// zugleich und fuehrt sie muehelos; entwaffnet verstuemmelt es sich und nutzt blanken Knochen als Waffe. Entity 5: keine
// Augen (Level 1) - es jagt nach Gehoer; still bleiben; beim Geraeusch mehrerer rennender Menschen zieht es sich zurueck.
// Auslegung (im Briefing so gekennzeichnet):
//   0 lauert an der Decke (Arme gespreizt, die Waffenarme haengen; Knochen klappern, Atem durch die Zaehne)
//   1 lauscht (Finger tippen an der Decke)     2 jagt an der Decke (Hand ueber Hand, Staub rieselt) zum Geraeusch -
//   klueger als das kleine: es zielt dorthin, wohin der Laerm sich bewegt     3 laesst sich fallen (Aufschlag)
//   4 jagt am Boden (koordiniert, im Wechsel lauschen)     5 holt aus: Knochen schabt ueber Stein (Ankuendigung 1,1 s) -
//   leise weggegangen: es sucht weiter     6 Hieb zur Stelle des Laerms: Treffer (Schaden, nie ein Sofort-Tod) oder daneben;
//   ein Fehlschlag kann das Rohr brechen     7 entwaffnet: es reisst sich den Knochen frei (2,6 s; dabei hoert es nichts)
//   8 Rueckzug an die Decke und fort (20 s Stille; zwei rennende Spieler in der Naehe)     9 fort
// Klaenge ueber Klangnummern (gameplay/kin_l3.hpp, KC_CL_*), Synthese in audio/synth_clump_l3.cpp.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"
#include "gameplay/kin_l3.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kBig = 1.35f;     // Massstab des Modells (Buendelskelett) - Runde 1: 1,5 sprengte die Gaenge
constexpr float kCeilMin = 2.45f;  // so viel freie Hoehe braucht es an der Decke

struct BigClump {  // je Wesen (Agent::ext)
    double boneT = 4.0, breathT = 6.0, ceilT = 0.0, fieldT = 0.0, lostT = 0.0, phaseT = 0.0, hold = 0.0;
    int arm = 1, strikes = 0;
    bool pipe = true, struck = false, swapped = false;
    vec3 aim, lastHeard, heardVel;
    double heardT = -1.0;
    float ceil = 1.0f;
};
struct BigCtl {
    double next = 140.0, req = -1.0, quick = 1.0;
    int strikes = 0, hits = 0, misses = 0, tears = 0, drops = 0, fled = 0;
};
BigCtl& ctlOf(std::shared_ptr<void>& p) {
    if (!p) p = std::make_shared<BigCtl>();
    return *static_cast<BigCtl*>(p.get());
}
template <class A>
BigClump& bigOf(A& a) {
    if (!a.ext) a.ext = std::make_shared<BigClump>();
    return *static_cast<BigClump*>(a.ext.get());
}
float cue(int k) { return kKinCue + (float)k; }
}  // namespace

Presence::BigClumpInfo Presence::bigClumpInfo() const {
    BigClumpInfo I;
    if (bigClump_) {
        const BigCtl& C = *static_cast<const BigCtl*>(bigClump_.get());
        I.strikes = C.strikes, I.hits = C.hits, I.misses = C.misses, I.tears = C.tears, I.drops = C.drops, I.fled = C.fled;
    }
    for (const auto& a : agents_)
        if (a->role == Role::Clump && a->variant > 0 && !a->showcase) {
            I.active = true, I.state = a->state, I.variant = a->variant, I.pos = a->pos, I.ceil = a->hang;
            break;
        }
    return I;
}

bool Presence::bigClumpSet(const std::string& key, double v) {
    BigCtl& C = ctlOf(bigClump_);
    if (key == "spawn") return C.req = std::max(0.0, v), true;
    if (key == "quick") return C.quick = std::max(0.05, v), true;
    if (key == "next") return C.next = v, true;
    for (auto& a : agents_) {
        if (a->role != Role::Clump || a->variant <= 0 || a->showcase) continue;
        BigClump& b = bigOf(*a);
        if (key == "state") a->state = (int)v, a->stateT = 0.0;
        else if (key == "ceil") b.ceil = (float)std::clamp(v, 0.0, 1.0), a->hang = b.ceil;
        else if (key == "disarm") a->state = 7, a->stateT = 0.0, b.pipe = false;
        else return false;
        return true;
    }
    return false;
}

// Groesse, Decke, Waffen an die Animation (auch Puppen und Vorfuehrungen): extra_ = {Variante, Decke, Hieb, Arm, Riss}
void Presence::prepClumpL3(Agent& a, const Context& c) {
    (void)c;
    a.cre.setScale(kBig);
    // Waffenarm: beim Gast aus dem Kanal wave (Arm / 8), sonst aus mode
    const float arm = a.puppet ? std::round(a.wave * 8.0f) : (float)(a.mode > 0 ? a.mode : 3);
    a.cre.setExtra({(float)a.variant, a.hang, a.strike, arm, a.plea});
    a.noShadow = false;
}

// --- Auftritt ------------------------------------------------------------------------------------------------------------------
bool Presence::spawnClumpL3(const Context& c, std::vector<Event>& out, double dist) {
    if (!fld().valid()) return false;
    const vec3 eye = c.view.eye;
    const int lo = dist > 0.0 ? (int)(dist * 8.5) : 180, hi = dist > 0.0 ? (int)(dist * 12.5) : 380;
    vec3 p;
    auto ok = [&](vec3 q) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
        if (!t || t->clearance() < kCeilMin || !figureWalkable(t, 1.1f)) return false;
        const vec3 top = q + vec3{0, 0, t->clearance() - 0.35f};
        return !(dot(nrmF(top - eye, {1, 0, 0}), c.view.look) > 0.3f && clearSight(*c.world, eye, top));
    };
    if (!hiddenSpawn(c, lo, hi, true, p, ok) && !hiddenSpawn(c, lo, hi, false, p, ok)) return false;
    auto a = std::make_unique<Agent>();
    a->role = Role::Clump;
    a->ent = Entity::Clump;
    a->variant = 1;
    a->mode = 1;
    a->hang = 1.0f;
    a->initBody(p, (float)rng_.uniform(-3.14, 3.14), (unsigned)rng_.randint(1, 1 << 30));
    a->state = 0;
    BigClump& b = bigOf(*a);
    b.ceil = 1.0f;
    prepClumpL3(*a, c);
    log::info("Praesenz: grosses Buendel an der Decke bei ({:.1f}, {:.1f}), {:.0f} m", p.x, p.y, hdist(p, eye));
    out.push_back({Event::Appear, Role::Clump, p + vec3{0, 0, 2.2f}});
    agents_.push_back(std::move(a));
    return true;
}

void Presence::regieClumpL3(double dt, const Context& c, std::vector<Event>& out) {
    BigCtl& C = ctlOf(bigClump_);
    if (!c.clumpBig || c.mode <= 0 || c.suspended) return;
    bool any = false;
    for (const auto& a : agents_) any = any || (a->role == Role::Clump && a->variant > 0 && !a->showcase);
    if (C.req >= 0.0) {
        const double d = C.req;
        C.req = -1.0;
        if (!any && !spawnClumpL3(c, out, d)) log::info("Praesenz: erzwungenes grosses Buendel nicht moeglich");
        return;
    }
    if (any) return;
    const bool busy = has(Role::Smiler) || wranglerActive() || (pack_ && packInfo().phase >= 1);
    C.next -= dt * (busy ? 0.2 : 1.0);
    if (C.next <= 0.0)
        C.next = spawnClumpL3(c, out, 0.0) ? rng_.uniform(c.clumpBig->every * 0.7, c.clumpBig->every * 1.3) * c.stalkerPause : 8.0;
}

// --- Denken --------------------------------------------------------------------------------------------------------------------
void Presence::thinkClumpL3(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    BigCtl& C = ctlOf(bigClump_);
    BigClump& b = bigOf(a);
    const vec3 eye = c.view.eye;
    const vec3 ppos{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, ppos);
    const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y));
    const float clear = t ? t->clearance() : 0.0f;
    a.stateT += dt;
    a.alert = 0.0f, a.perceive = 0.0f, a.shiver = 0.0f, a.reach = 0.0f;
    a.look = false;
    // Hoeren (blind): weiter als das kleine Buendel; es merkt sich, wohin der Laerm wandert (klug - Auslegung)
    vec3 where;
    float loud = 0.0f;
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    if (c.noise > 0.01 && pathD < (float)c.noise * 27.0f) where = ppos, loud = (float)c.noise;
    if (c.pings)
        for (const auto& [q, s] : *c.pings)
            if (hdist(a.pos, q) * 1.2f < s * 27.0f && s > loud) where = q, loud = s;
    const bool hears = loud > 0.0f && a.state != 7;
    if (hears) {
        if (b.heardT >= 0.0 && b.heardT < 1.5 && hdist(where, b.lastHeard) < 4.0f)
            b.heardVel = b.heardVel + ((where - b.lastHeard) / (float)std::max(0.05, b.heardT) - b.heardVel) * 0.5f;
        b.lastHeard = where, b.heardT = 0.0;
    } else if (b.heardT >= 0.0) {
        b.heardT += dt;
    }
    const vec3 predicted = b.lastHeard + flat(b.heardVel) * (float)std::min(1.2, 0.6 + 0.02 * pathD);
    // mehrere rennende Menschen in der Naehe: es zieht sich zurueck (Entity 5)
    int runners = c.player->sprinting && d < 15.0f ? 1 : 0;
    if (c.others)
        for (const Other& o : *c.others)
            if (o.player && o.player->sprinting && hdist(a.pos, vec3{(float)o.player->x, (float)o.player->y, 0.0f}) < 15.0f) ++runners;
    auto setState = [&](int s) {
        a.state = s;
        a.stateT = 0.0;
    };
    auto chase = [&](vec3 to) {
        a.heard = to;
        b.fieldT = 0.5;
        a.field.build(*c.world, ifloor(to.x), ifloor(to.y), 0.8f);
    };
    const vec3 headAt = a.cre.headPos();
    // Bewegungsgeraeusch und Stimme: Knochen klappern, Atem durch die Zaehne (Geraeusch vor Sicht)
    b.boneT -= dt;
    if (b.boneT <= 0.0 && a.state != 7) {
        b.boneT = rng_.uniform(5.0, 10.0) * (a.state >= 4 ? 0.5 : 1.0);
        out.push_back({Event::Knock, Role::Clump, headAt, cue(KC_CL_BONES)});
    }
    b.breathT -= dt;
    if (b.breathT <= 0.0) {
        b.breathT = rng_.uniform(7.0, 12.0);
        if (d < 14.0f) out.push_back({Event::Drag, Role::Clump, headAt, cue(KC_CL_BREATH)});
    }
    // Decke: haelt es nur, wo die Decke hoch genug ist
    const bool canCeil = clear >= kCeilMin;
    auto toCeil = [&](float want) {
        b.ceil += (want - b.ceil) * (float)std::min(1.0, dt * (want > b.ceil ? 1.6 : 5.0));
        a.hang = b.ceil;
    };
    if (runners >= 2 && a.state < 8 && a.state != 7) {
        ++C.fled;
        log::info("Praesenz: grosses Buendel weicht vor mehreren Rennenden zurueck");
        setState(8);
    }
    switch (a.state) {
        case 0: {  // lauert an der Decke
            toCeil(canCeil ? 1.0f : 0.0f);
            if (hears) {
                setState(1);
                chase(where);
            }
            break;
        }
        case 1: {  // lauscht: die Finger tippen an der Decke
            toCeil(canCeil ? 1.0f : 0.0f);
            a.perceive = 1.0f;
            a.perceiveAt = b.lastHeard;
            if (a.stateT > 1.2 && hears) {
                setState(2);
                chase(predicted);
                out.push_back({Event::Knock, Role::Clump, a.pos + vec3{0, 0, clear - 0.1f}, cue(KC_CL_CEIL)});
                b.ceilT = 1.3;
            } else if (a.stateT > 8.0) {
                setState(0);
            }
            break;
        }
        case 2: {  // jagt an der Decke - zum vorausgesagten Ort des Laerms
            toCeil(canCeil ? 1.0f : 0.0f);
            if (hears && hdist(predicted, a.heard) > 1.5f) chase(predicted);
            b.ceilT -= dt;
            if (b.ceilT <= 0.0) {
                b.ceilT = rng_.uniform(1.1, 1.6);
                out.push_back({Event::Knock, Role::Clump, a.pos + vec3{0, 0, std::max(2.0f, clear - 0.1f)}, cue(KC_CL_CEIL)});
            }
            const bool moving = walkTo(a, a.field, 1.9 * C.quick, dt, c);
            const bool fresh = b.heardT >= 0.0 && b.heardT < 3.0;
            // ueber der Stelle (oder die Decke endet): fallen lassen
            if ((fresh && hdist(a.pos, b.lastHeard) < 2.2f) || !canCeil || (!moving && fresh)) {
                setState(3);
                ++C.drops;
            } else if (!moving || (b.heardT > 10.0)) {
                setState(1);
            }
            break;
        }
        case 3: {  // laesst sich fallen
            toCeil(0.0f);
            if (a.stateT > 0.45 && !b.struck) {
                b.struck = true;
                out.push_back({Event::Drop, Role::Clump, a.pos + vec3{0, 0, 0.4f}, cue(KC_CL_DROP)});
            }
            if (a.stateT > 0.9) b.struck = false, setState(4), chase(predicted), b.lostT = 0.0;
            break;
        }
        case 4: {  // jagt am Boden: koordiniert, schnell, im Wechsel lauschen
            toCeil(0.0f);
            const bool listen = std::fmod(a.stateT, 4.0) > 2.9;
            a.perceive = listen ? 1.0f : 0.0f;
            a.perceiveAt = a.heard;
            if (hears) {
                b.lostT = 0.0;
                if (hdist(predicted, a.heard) > 1.2f) chase(predicted);
            } else {
                b.lostT += dt;
            }
            const bool close = d < 2.8f && b.heardT >= 0.0 && b.heardT < 1.2 && hdist(b.lastHeard, ppos) < 2.5f;
            if (close && c.mode >= 2) {  // ausholen - Ankuendigung: Knochen schabt ueber Stein, die Waffenarme heben sich
                setState(5);
                b.aim = b.lastHeard;
                b.arm = b.pipe && a.variant == 1 ? (C.strikes % 2 ? 1 : 3) : (C.strikes % 2 ? 1 : 5);
                if (a.variant == 2) b.arm = C.strikes % 2 ? 3 : 5;
                a.mode = b.arm;
                a.wave = (float)b.arm * 0.125f;  // fuer die Puppe beim Gast
                out.push_back({Event::Swing, Role::Clump, headAt + vec3{0, 0, 0.3f}, cue(KC_CL_SCRAPE)});
                break;
            }
            if (!listen) walkTo(a, a.field, 2.1 * C.quick, dt, c);
            if (b.lostT > 20.0) setState(8);
            break;
        }
        case 5: {  // holt aus (1,1 s): zielt auf die Stelle des Laerms; leise weg - es sucht weiter
            a.alert = 1.0f;
            a.strike = (float)std::min(0.33, a.stateT / 1.1 * 0.33);
            if (hears && loud >= 0.2f) b.aim = where;
            if (hdist(b.aim, a.pos) > 0.2f) a.yaw += wrapA(std::atan2(b.aim.y - a.pos.y, b.aim.x - a.pos.x) - a.yaw) * (float)std::min(1.0, dt * 5.0);
            if (d > 2.7f && c.noise < 0.2) {
                setState(4);
                b.lostT = 0.0;
                break;
            }
            if (a.stateT > 1.1) {
                setState(6);
                b.struck = false;
                out.push_back({Event::Swing, Role::Clump, headAt, cue(KC_CL_SWING)});
            }
            break;
        }
        case 6: {  // der Hieb
            a.alert = 1.0f;
            a.strike = (float)std::min(1.0, 0.33 + a.stateT / 0.35 * 0.32 + (a.stateT > 0.35 ? (a.stateT - 0.35) / 0.6 * 0.35 : 0.0));
            if (a.stateT > 0.3 && !b.struck) {
                b.struck = true;
                ++C.strikes;
                ++b.strikes;
                if (hdist(ppos, b.aim) < 1.15f && d < 3.0f) {
                    ++C.hits;
                    const float dmg = b.arm == 3 && b.pipe && a.variant == 1 ? 24.0f : 30.0f;  // Rohr / Knochen
                    out.push_back({Event::Hit, Role::Clump, eye - vec3{0, 0, 0.4f}, dmg});
                    out.push_back({Event::Swing, Role::Clump, eye - vec3{0, 0, 0.4f}, cue(KC_CL_HIT)});
                } else {
                    ++C.misses;
                    const vec3 at = a.pos + dirYaw(a.yaw) * 2.2f + vec3{0, 0, 0.1f};
                    out.push_back({Event::Swing, Role::Clump, at, cue(KC_CL_MISS)});
                    // das Rohr bricht am Boden oder an der Wand (jeder zweite Fehlschlag mit dem Rohr)
                    if (b.arm == 3 && b.pipe && a.variant == 1 && rng_.uniform(0.0, 1.0) < 0.5) {
                        b.pipe = false;
                        out.push_back({Event::Swing, Role::Clump, at, cue(KC_CL_SNAP)});
                    }
                }
            }
            if (a.stateT > 0.95) {
                a.strike = 0.0f;
                if (!b.pipe && a.variant == 1) setState(7);  // entwaffnet
                else setState(4), b.lostT = 0.0;
            }
            break;
        }
        case 7: {  // entwaffnet: reisst sich den Knochen frei - beschaeftigt, hoert nichts (Fluchtfenster)
            a.plea = (float)std::min(1.0, a.stateT / 2.6);
            a.shiver = a.stateT > 0.8 && a.stateT < 1.9 ? 1.0f : 0.0f;
            if (a.stateT > 1.3 && !b.swapped) {
                b.swapped = true;
                ++C.tears;
                out.push_back({Event::Swing, Role::Clump, headAt, cue(KC_CL_TEAR)});
            }
            if (a.stateT > 1.6 && a.variant == 1) swapVariant(a, 2);
            if (a.stateT > 2.6) {
                a.plea = 0.0f;
                b.pipe = false;
                setState(4);
                b.lostT = 0.0;
            }
            break;
        }
        case 8: {  // Rueckzug: an die Decke und fort vom Spieler
            toCeil(canCeil ? 1.0f : 0.0f);
            a.timer2 -= dt;
            if ((!a.hasGoal || !walkTo(a, a.field, 1.7, dt, c)) && a.timer2 <= 0.0) {
                if (!pickGoal(c, a, 220, 380, true) && a.unseenT > 2.0) a.state = 9;
                a.timer2 = 1.0;
            }
            if ((d > 24.0f && a.unseenT > 2.0) || (a.stateT > 35.0 && a.unseenT > 2.0)) {
                out.push_back({Event::Gone, Role::Clump, a.pos});
                a.state = 9;
            }
            break;
        }
        default: break;
    }
    // Schritte: schwere Handschlaege am Boden, an der Decke das Greifen (Klangnummer statt des kleinen Klatschens)
    if (a.cre.stepped() && (a.state == 2 || a.state == 4 || a.state == 8) && rng_.uniform(0.0, 1.0) < 0.35)
        out.push_back({Event::Drag, Role::Clump, a.cre.stepPos(), cue(KC_CL_STEP)});
}

}  // namespace lim::game
