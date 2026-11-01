// V13.3: Plague Goblins (Entity 25, Wikidot entity-25 Rev. 28; Level 1 Rev. 83, Fussnote; KREATUR_PlagueGoblins.md) -
// Auftreten und Verhalten einer Bande ("swindle").
//
// Die Seite: scheue, gelegenheitsgetriebene Sammler in Banden von 9 bis 27; sie bestehlen vor allem Alleinreisende, Schwache und
// Alte; drei Kasten - Spaeher (dunkle Ledermasken mit Glasaugen; ziehen sich bei Gefahr zurueck und warnen die anderen, ein
// Hinterhalt kann folgen), Henchmen (glatte helle Masken; tragen und sammeln), Torwaechterinnen (Hut, Cape, verzierte Maske,
// Silberstock mit Sanduhr - oeffnet das Portal, durch das die Bande kommt und geht). Was sie aufheben, verschwindet im Umhang.
// Sie fliehen vor Hounds; wer Mandelwasser oder Essen anbietet, wird beschenkt und gewarnt; Schreien und Fuchteln verscheucht
// sie; wird einer verletzt oder getoetet, zieht sich die Bande zurueck und kommt nicht wieder. Level 1: mit Messer oder Axt
// zu verscheuchen.
//
// Ablauf (Bande, Swindle::phase):
//   0 Ankunft   ausser Sicht oeffnet die Torwaechterin das Portal (Klang vor Sicht: Sand, Glas, Kraechzen), die Bande springt
//               einzeln heraus
//   1 Spaehen   die Spaeher pirschen bis auf etwa 12 m heran und spaehen; Henchmen suchen am Portal nach Brauchbarem. Sieht man
//               einen Spaeher lange an oder leuchtet ihn an, schreit er und rennt zurueck - die Bande ist gewarnt (Hinterhalt)
//   2 Stehlen   ist das Ziel allein und hat etwas Brauchbares: zwei Diebe naehern sich von hinten - sie erstarren, solange man
//               sie ansieht. Hinter dem Ruecken springt einer auf den Rucksack (es zerrt und raschelt: Ankuendigung, 1,2 s) -
//               dreht man sich um, rennt oder schwingt das Beil, faellt er ab. Sonst nimmt er etwas und flieht zum Portal
//   3 Bedraengen ein Geschwaechter (Gesundheit < 45) oder wer einen Dieb in die Enge treibt: Masken hoch, Fauchen (1,4 s - F2),
//               dann springen bis zu drei Goblins nacheinander zu (je Sprung Kreischen und Ducken vorher; Biss 5); hoechstens
//               9 s, und nie bis zum Tod (bei Gesundheit < 22 lassen sie ab - Auslegung: sie wollen die Vorraete)
//   4 Abzug     alle zurueck durch das Portal; die Torwaechterin zuletzt, dann schliesst es
//   5 Handel    legt man Mandelwasser oder Essen hin, holt es einer; die Torwaechterin bietet ein Geschenk an (Batterie), die
//               Spaeher warnen vor Gefahr (Warnschrei, wenn ein feindliches Wesen naeher als 25 m ist); dann ziehen sie ab
// Einzelne: im Lichtkegel schirmen sie die Augen ab; wer auf sie zurennt oder sie lange anleuchtet, verscheucht sie kurz. Das
// Beil (goblinSwing): ein Treffer bis 1,8 m toetet einen Goblin - er verliert, was er trug; die Bande flieht und kommt in
// dieser Ebene nicht wieder. Ein Schwung ohne Treffer verscheucht, wer davor steht.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
using namespace anim::gb;
enum Phase : int { P_ARRIVE = 0, P_SCOUT = 1, P_STEAL = 2, P_MOB = 3, P_LEAVE = 4, P_FRIEND = 5 };
struct GobAg {
    float scale = 1.0f;
    double delay = 0.0, scareT = 0.0, coolT = 0.0, peekT = 0.0, chatT = 2.0, lightT = 0.0, stuckT = 0.0, fleeT = 0.0;
    std::vector<std::string> stash;  // im Umhang
    int job = 0;                     // 0 sammelt/spaeht, 1 Dieb, 2 umkreist, 3 Angreifer
    float startYaw = 0.0f;           // Wuehlen: Blickrichtung des Opfers zu Beginn
    bool bit = false, warned = false, fleeing = false;
    vec3 claim;                      // Ware, die er holen will
    bool hasClaim = false, claimDropped = false;
    std::string claimKind;
    vec3 lastPos;
    float glow = 0.35f;
};
struct Swindle {
    bool active = false;
    int group = -1, phase = P_ARRIVE, target = 0;
    double phaseT = 0.0, life = 0.0, mobT = 0.0, friendT = 0.0, closeT = 0.0, cornerT = 0.0, warnT = 0.0, chatT = 3.0;
    vec3 portal;
    float portalYaw = 0.0f;
    NavField home;
    bool homeOk = false;
    int steals = 0, fails = 0, gifts = 0, kills = 0;
    bool routed = false, friendly = false, alarm = false, hissed = false, giftDone = false, mimicDone = false, weakWilling = true;
    double quick = 1.0;
    int mobForce = 0;
    world::World* world = nullptr;
    std::vector<Presence::Event> pending;  // aus goblinSwing (ausserhalb von update)
    std::vector<vec3> claims;
};
Swindle& swindleOf(std::shared_ptr<void>& p) {
    if (!p) p = std::make_shared<Swindle>();
    return *static_cast<Swindle*>(p.get());
}
const Swindle* swindleOf(const std::shared_ptr<void>& p) { return p ? static_cast<const Swindle*>(p.get()) : nullptr; }
template <class A>
GobAg& gag(A& a) {
    if (!a.ext) a.ext = std::make_shared<GobAg>();
    return *static_cast<GobAg*>(a.ext.get());
}
float casteScale(int variant, u16 netId, unsigned seed) {
    const unsigned h = (unsigned)netId * 2654435761u + seed * 40503u + 17u;
    const float r = (float)((h >> 9) & 1023u) / 1023.0f;
    if (variant == 2) return 1.18f + 0.06f * r;  // die Seite: Weibchen sind groesser; Torwaechterinnen sind erwachsene Weibchen
    if (variant == 1) return 0.94f + 0.08f * r;
    return 0.86f + 0.2f * r;
}
// Fluchtziel lo..hi m von 'from' weg (begehbar fuer ein katzengrosses Wesen), Weg dorthin in a.field
template <class A>
bool fleeGoal(A& a, Rng& rng, world::World& w, vec3 from, float lo, float hi) {
    const vec3 away = nrmF(flat(a.pos - from), dirYaw(a.yaw));
    for (int t = 0; t < 14; ++t) {
        const vec3 g = a.pos + rotateYaw(away, (float)rng.uniform(-1.1, 1.1)) * (float)rng.uniform(lo, hi);
        const world::Tile* tl = w.tileIfLoaded(ifloor(g.x), ifloor(g.y));
        if (!figureWalkable(tl, 0.8f)) continue;
        a.field.build(w, ifloor(g.x), ifloor(g.y), 0.8f);
        if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) continue;
        a.goal = {g.x, g.y, tl->floor};
        a.hasGoal = true;
        return true;
    }
    a.hasGoal = false;
    return false;
}
// Weg zu einem Punkt (nur neu, wenn sich die Zielkachel aendert)
template <class A>
bool goalTo(A& a, world::World& w, vec3 p) {
    if (a.hasGoal && ifloor(a.goal.x) == ifloor(p.x) && ifloor(a.goal.y) == ifloor(p.y)) return true;
    const world::Tile* tl = w.tileIfLoaded(ifloor(p.x), ifloor(p.y));
    if (!figureWalkable(tl, 0.7f)) return false;
    a.field.build(w, ifloor(p.x), ifloor(p.y), 0.7f);
    if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) return a.hasGoal = false, false;
    a.goal = {p.x, p.y, tl->floor};
    a.hasGoal = true;
    return true;
}
bool hostile(Presence::Role r) {
    using R = Presence::Role;
    return r == R::Hound || r == R::Smiler || r == R::Clump || r == R::SkinStealer || r == R::Wretch || r == R::Duller ||
           r == R::Wrangler || r == R::Growler;
}
}  // namespace

// --- Animation: Zustand, Groesse, Leuchten (auch Puppen beim Gast und Vorfuehrungen) -----------------------------------
void Presence::prepGoblin(Agent& a, const Context& c) {
    (void)c;
    GobAg& g = gag(a);
    if (a.variant != 3 && (a.netId || a.puppet || a.showcase)) g.scale = casteScale(a.variant, a.netId, 0);
    if (a.variant == 3) g.scale = 1.0f;
    a.cre.setScale(g.scale);
    a.cre.setExtra({(float)a.variant, (float)a.state, (float)a.stateT});
    a.noShadow = a.variant == 3;
    a.rig.setGlow(a.glow);
    if (a.puppet || a.showcase)  // Greifziel: vor den Fuessen am Boden, beim Sprung das Ziel
        a.heard = a.strike > 0.0f ? a.lookAt - vec3{0, 0, 0.6f} : a.pos + dirYaw(a.yaw) * (0.22f * g.scale);
}

// --- Regie ---------------------------------------------------------------------------------------------------------------
void Presence::regieGoblin(double dt, const Context& c, std::vector<Event>& out) {
    if (!c.kin || !c.kin[world::KIN_GOBLIN].on) return;
    Swindle& S = swindleOf(goblinCtl_);
    if (S.active) return;
    if (S.routed) return;  // die Seite: "They will not return" - in dieser Ebene keine Bande mehr
    nextGoblin_ -= dt;
    if (nextGoblin_ > 0.0) return;
    const world::KinDef& K = c.kin[world::KIN_GOBLIN];
    nextGoblin_ = spawnGoblin(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) : 8.0;
}

bool Presence::spawnGoblin(const Context& c, std::vector<Event>& out, bool forced) {
    world::KinDef K;
    K.groupMin = 9, K.groupMax = 12, K.every = 240.0, K.chance = 0.6;  // erzwungen ohne Eintrag: wie Level 1
    if (c.kin && c.kin[world::KIN_GOBLIN].on) K = c.kin[world::KIN_GOBLIN];
    else if (!forced) return false;
    Swindle& S0 = swindleOf(goblinCtl_);
    if (S0.active) return false;
    // Portal: auf dem Weg 24-38 m vom Spieler (erzwungen: 15-26 m), ausser Sicht, mit Platz zum Herausspringen
    vec3 site;
    auto ok = [&](vec3 p) {
        if (!roomAllowed(c, p, world::KIN_GOBLIN)) return false;
        int open = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) open += figureWalkable(c.world->tileIfLoaded(ifloor(p.x) + dx, ifloor(p.y) + dy), 1.4f) ? 1 : 0;
        return open >= 7;
    };
    if (!hiddenSpawn(c, forced ? 150 : 240, forced ? 260 : 380, false, site, ok)) return false;
    Swindle S;
    S.active = true;
    S.group = nextGroup_++;
    S.portal = site;
    S.portalYaw = std::atan2(c.view.eye.y - site.y, c.view.eye.x - site.x);
    S.home.build(*c.world, ifloor(site.x), ifloor(site.y), 0.7f);
    S.homeOk = S.home.valid();
    S.weakWilling = rng_.uniform(0.0, 1.0) < K.chance;
    S.world = c.world;
    S.quick = S0.quick;
    swindleOf(goblinCtl_) = S;
    // das Portal
    auto pa = std::make_unique<Agent>();
    pa->role = Role::Goblin;
    pa->ent = Entity::Goblin;
    pa->variant = 3;
    pa->group = S.group;
    pa->state = 1;
    pa->noShadow = true;
    pa->glow = 1.0f;
    pa->initBody(site, S.portalYaw, (unsigned)rng_.randint(1, 1 << 30));
    agents_.push_back(std::move(pa));
    // die Bande: eine Torwaechterin, zwei Spaeher, der Rest Henchmen - alle noch im Portal
    const int n = (int)rng_.randint(K.groupMin, K.groupMax);
    for (int i = 0; i < n; ++i) {
        auto a = std::make_unique<Agent>();
        a->role = Role::Goblin;
        a->ent = Entity::Goblin;
        a->variant = i == 0 ? 2 : (i <= 2 ? 1 : 0);
        a->group = S.group;
        a->state = S_INSIDE;
        GobAg& g = gag(*a);
        g.delay = 0.95 + 0.32 * (double)i + (i == 0 ? 0.0 : 0.25);
        g.scale = casteScale(a->variant, 0, (unsigned)i);
        g.chatT = rng_.uniform(1.0, 5.0);
        a->glow = 0.0f;
        a->initBody(site, S.portalYaw + (float)rng_.uniform(-0.3, 0.3), (unsigned)rng_.randint(1, 1 << 30));
        agents_.push_back(std::move(a));
    }
    out.push_back({Event::Portal, Role::Goblin, site + vec3{0, 0, 0.7f}, 1.0f});
    out.push_back({Event::Croak, Role::Goblin, site + vec3{0, 0, 0.4f}, 0.5f});
    log::info("Praesenz: Plague Goblins - Portal bei ({:.1f}, {:.1f}), {} Goblins, {:.0f} m{}", site.x, site.y, n, hdist(site, c.view.eye),
              S.weakWilling ? "" : " (setzen Geschwaechten nicht zu)");
    return true;
}

// --- Beil ---------------------------------------------------------------------------------------------------------------
int Presence::goblinSwing(int target, vec3 eye, vec3 look) {
    Swindle& S = swindleOf(goblinCtl_);
    if (!S.active || !S.world) return 0;
    const vec3 fl = nrmF(flat(look), {1, 0, 0});
    Agent* best = nullptr;
    float bestD = 1e9f;
    int scared = 0;
    for (auto& ap : agents_) {
        Agent& a = *ap;
        if (a.role != Role::Goblin || a.group != S.group || a.variant == 3 || a.state == S_DEAD || a.state == S_INSIDE || a.state == S_ENTER) continue;
        GobAg& g = gag(a);
        if (a.state == S_RUMMAGE && a.target == target) {  // der Schwung schuettelt den Dieb vom Ruecken
            a.state = S_SCURRY, a.stateT = 0.0;
            a.pos = vec3{a.pos.x, a.pos.y, a.pos.z} - vec3{0, 0, 0.6f};
            if (const world::Tile* t = S.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)); t && t->open()) a.pos.z = t->floor;
            g.scareT = 3.0, g.fleeing = true;
            fleeGoal(a, rng_, *S.world, eye, 5.0f, 8.0f);
            S.pending.push_back({Event::Croak, Role::Goblin, a.pos + vec3{0, 0, 0.4f}, 1.0f, target});
            ++scared;
            continue;
        }
        const vec3 chest = a.pos + vec3{0, 0, 0.3f * g.scale};
        const vec3 to = flat(chest - eye);
        const float d = length(to);
        if (d < 1e-3f) continue;
        const float cosA = dot(to / d, fl);
        if (d <= 1.8f && cosA >= 0.72f && clearSight(*S.world, eye, chest) && d < bestD) best = &a, bestD = d;
        else if (d <= 6.0f && cosA >= 0.5f && clearSight(*S.world, eye, chest) && a.state != S_ATTACK) {
            g.scareT = rng_.uniform(2.5, 4.0), g.fleeing = true;
            a.state = S_SCURRY, a.stateT = 0.0;
            fleeGoal(a, rng_, *S.world, eye, 5.0f, 9.0f);
            if (scared++ < 2) S.pending.push_back({Event::Croak, Role::Goblin, chest, 1.0f, target});
        }
    }
    if (best) {
        Agent& a = *best;
        GobAg& g = gag(a);
        a.state = S_DEAD, a.stateT = 0.0;
        a.strike = 0.0f, a.growl = 0.0f;
        S.pending.push_back({Event::Perish, Role::Goblin, a.pos + vec3{0, 0, 0.3f * g.scale}, 1.0f, target});
        for (const std::string& k : g.stash) {
            Event e{Event::Spill, Role::Goblin, a.pos + vec3{0, 0, 0.25f}, 1.0f, target};
            e.item = k;
            S.pending.push_back(e);
        }
        g.stash.clear();
        ++S.kills;
        S.routed = true;
        if (S.phase != P_LEAVE) S.phase = P_LEAVE, S.phaseT = 0.0;
        log::info("Praesenz: Plague Goblin vom Beil getroffen - die Bande flieht und kommt nicht wieder");
        return 2;
    }
    return scared > 0 ? 1 : 0;
}

Presence::GoblinInfo Presence::goblinInfo() const {
    GoblinInfo inf;
    const Swindle* S = swindleOf(goblinCtl_);
    if (!S) return inf;
    inf.active = S->active, inf.friendly = S->friendly, inf.routed = S->routed;
    inf.phase = S->phase, inf.steals = S->steals, inf.fails = S->fails, inf.gifts = S->gifts;
    inf.portal = S->portal;
    for (const auto& ap : agents_) {
        const Agent& a = *ap;
        if (a.role != Role::Goblin || a.group != S->group || a.showcase) continue;
        if (a.variant == 3) {
            inf.portalOpen = a.state == 1 || a.state == 2;
            continue;
        }
        ++inf.bodies;
        inf.dead += a.state == S_DEAD ? 1 : 0;
        inf.pos.push_back(a.pos);
        inf.state.push_back(a.state);
        inf.variant.push_back(a.variant);
        inf.mask.push_back(a.growl > 0.5f || a.strike > 0.0f ? 1.0f : 0.0f);
    }
    return inf;
}

bool Presence::goblinSet(const std::string& key, double v) {
    Swindle& S = swindleOf(goblinCtl_);
    if (key == "quick") S.quick = std::max(0.1, v);
    else if (!S.active) return false;
    else if (key == "phase") S.phase = (int)v, S.phaseT = 0.0;
    else if (key == "mob") S.mobForce = (int)v;
    else if (key == "friendly") S.friendly = v > 0.5, S.phase = v > 0.5 ? P_FRIEND : S.phase, S.phaseT = 0.0;
    else if (key == "rout") S.routed = true, S.phase = P_LEAVE, S.phaseT = 0.0;
    else return false;
    return true;
}

// --- Verhalten ----------------------------------------------------------------------------------------------------------
void Presence::thinkGoblin(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    Swindle& S = swindleOf(goblinCtl_);
    GobAg& g = gag(a);
    a.stateT += dt;
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const bool mine = S.active && a.group == S.group;
    S.world = c.world;

    // ============================================ Portal: das Hirn der Bande ============================================
    if (a.variant == 3) {
        if (!mine) {
            a.state = 9;
            return;
        }
        for (Event& e : S.pending) out.push_back(e);
        S.pending.clear();
        const double qdt = dt * S.quick;
        S.life += qdt, S.phaseT += qdt;
        S.target = a.target;
        // Mitglieder
        std::vector<Agent*> mem;
        Agent* gate = nullptr;
        int waiting = 0, alive = 0;
        for (auto& ap : agents_) {
            Agent& m = *ap;
            if (m.role != Role::Goblin || m.group != S.group || m.variant == 3 || m.showcase) continue;
            if (m.state == S_DEAD) continue;
            mem.push_back(&m);
            ++alive;
            if (m.variant == 2) gate = &m;
            if (m.state == S_INSIDE || m.state == S_EMERGE) ++waiting;
        }
        // Oeffnen / offen / Schliessen
        if (a.state == 1) {
            a.fly = (float)std::min(1.0, a.stateT / 0.9);
            if (a.stateT >= 0.9) a.state = 2, a.stateT = 0.0;
        } else if (a.state == 2) {
            a.fly = 1.0f;
        } else if (a.state == 3) {
            a.fly = (float)std::max(0.0, 1.0 - a.stateT / 0.8);
            if (a.stateT >= 0.8) {
                a.state = 9;
                S.active = false;
                return;
            }
        }
        a.glow = 0.6f + 0.4f * a.fly;
        // Hounds: Furcht und Hass (die Seite) - ist einer nah, fliehen alle
        for (const auto& ap : agents_)
            if (ap->role == Role::Hound && !ap->showcase && (hdist(ap->pos, S.portal) < 30.0f || hdist(ap->pos, feet) < 25.0f) &&
                S.phase != P_LEAVE) {
                S.phase = P_LEAVE, S.phaseT = 0.0;
                out.push_back({Event::Croak, Role::Goblin, S.portal + vec3{0, 0, 0.5f}, 1.0f});
                log::info("Praesenz: Plague Goblins fliehen vor einem Hound");
            }
        const bool alone = a.alone;
        const bool hasLoot = c.loot && !c.loot->empty();
        switch (S.phase) {
            case P_ARRIVE:
                if (waiting == 0 && S.phaseT > 1.0) S.phase = P_SCOUT, S.phaseT = 0.0;
                break;
            case P_SCOUT: {
                bool peeked = false;
                for (Agent* m : mem)
                    if (m->variant == 1 && gag(*m).peekT > 6.0) peeked = true;
                if (S.alarm || peeked || hdist(feet, S.portal) < 12.0f || S.phaseT > 45.0) S.phase = P_STEAL, S.phaseT = 0.0;
                break;
            }
            case P_STEAL: {
                if (S.steals >= 2 || S.fails >= 2 || (S.phaseT > 55.0 && S.steals == 0) || (!hasLoot && S.phaseT > 35.0)) S.phase = P_LEAVE, S.phaseT = 0.0;
                // wer einen Dieb mit Beute in die Enge treibt (bis 1,6 m, laenger als 1,5 s), wird bedraengt
                bool corner = false;
                for (Agent* m : mem)
                    if (!gag(*m).stash.empty() && m->state != S_ENTER && hdist(m->pos, feet) < 1.6f) corner = true;
                S.cornerT = corner ? S.cornerT + qdt : std::max(0.0, S.cornerT - qdt);
                const bool weak = alone && c.health < 45.0 && S.weakWilling;
                if (c.mode >= 2 && (S.mobForce > 0 || weak || S.cornerT > 1.5) && S.phase == P_STEAL) {
                    S.phase = P_MOB, S.phaseT = 0.0, S.mobT = 0.0, S.hissed = false, S.mobForce = 0;
                    log::info("Praesenz: Plague Goblins bedraengen das Ziel ({})", weak ? "geschwaecht" : "in die Enge getrieben");
                }
                break;
            }
            case P_MOB:
                S.mobT += qdt;
                if (!S.hissed) {  // F2: Masken hoch, Fauchen - in 1,4 s geht es los
                    S.hissed = true;
                    const vec3 at = gate ? gate->pos : (mem.empty() ? S.portal : mem.front()->pos);
                    out.push_back({Event::Hiss, Role::Goblin, at + vec3{0, 0, 0.4f}, 1.0f});
                }
                if (S.mobT > 9.4 || c.health < 22.0 || S.routed) S.phase = P_LEAVE, S.phaseT = 0.0;
                break;
            case P_FRIEND:
                S.friendT += qdt;
                if (S.friendT > 50.0) S.phase = P_LEAVE, S.phaseT = 0.0;
                break;
            case P_LEAVE: {
                // schliessen, wenn alle drin sind (die Torwaechterin zuletzt) oder nach 25 s
                if ((alive == 0 || S.phaseT > 25.0) && a.state == 2) {
                    a.state = 3, a.stateT = 0.0;
                    out.push_back({Event::Portal, Role::Goblin, S.portal + vec3{0, 0, 0.7f}, 0.0f});
                    for (Agent* m : mem)  // wer zu spaet kommt, flieht anders (ungesehen fort)
                        if (m->state != S_ENTER) gag(*m).fleeing = true;
                }
                break;
            }
            default: break;
        }
        (void)alone;
        // leises Schnattern der Bande (Klang vor Sicht)
        S.chatT -= qdt;
        if (S.chatT <= 0.0 && !mem.empty() && S.phase != P_MOB) {
            Agent* m = mem[(size_t)rng_.randint(0, (i64)mem.size() - 1)];
            out.push_back({Event::Croak, Role::Goblin, m->pos + vec3{0, 0, 0.45f}, S.phase == P_LEAVE ? 0.5f : 0.0f});
            gag(*m).chatT = 0.6;
            S.chatT = rng_.uniform(2.5, 6.0);
        }
        return;
    }

    // ============================================ ein Goblin ==========================================================
    const float sc = g.scale;
    const vec3 head = a.cre.headPos();
    a.reach = 0.0f, a.jaw = 0.0f, a.growl = 0.0f, a.strike = 0.0f, a.fly = 0.0f, a.perceive = 0.0f, a.talk = 0.0f;
    a.plea = 0.0f, a.wave = 0.0f, a.lean = 0.0f, a.alert = 0.0f, a.crouch = 0.0f, a.shiver = 0.0f, a.bait = 0.0f, a.look = false;
    a.perceiveAt = eye;
    a.lookAt = eye;
    a.heard = a.pos + dirYaw(a.yaw) * (0.22f * g.scale);  // Greifziel: vor den Fuessen (picken)
    const double qdt = dt * (mine ? S.quick : 1.0);
    auto setState = [&](int s) {
        if (a.state != s) a.state = s, a.stateT = 0.0;
    };
    auto floorAt = [&](vec3 p) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        return t && t->open() ? t->floor : p.z;
    };
    // Leuchten der Augen: wirft Licht zurueck (die Seite: grosse, stark spiegelnde Augen, gelber Schimmer im Dunkeln)
    auto glowTo = [&](float t) { a.glow += (t - a.glow) * (float)std::min(1.0, dt * 6.0); };
    if (a.state == S_DEAD) {
        a.shiver = (float)std::max(0.0, 1.0 - a.stateT / 2.4);
        glowTo(0.0f);
        a.growl = 0.0f;
        if (a.stateT > 30.0 && a.unseenT > 3.0) a.state = 9;
        return;
    }
    if (a.state == S_INSIDE) {
        a.glow = 0.0f;
        if (!mine) {
            a.state = 9;
            return;
        }
        if (a.stateT * S.quick >= g.delay) {
            setState(S_EMERGE);
            out.push_back({Event::Croak, Role::Goblin, a.pos + vec3{0, 0, 0.4f}, a.variant == 2 ? 0.5f : 0.0f});
            if (a.variant == 2) a.wave = 1.0f;
        }
        return;
    }
    if (a.state == S_EMERGE) {  // springt heraus, landet geduckt vor dem Portal
        const float u = (float)std::min(1.0, a.stateT / 0.55);
        const vec3 f = dirYaw(mine ? S.portalYaw : a.yaw);
        const vec3 from = mine ? S.portal : a.pos;
        const vec3 p = from + f * (0.7f * u) + rotateYaw(f, kPiF * 0.5f) * (0.35f * std::sin((float)(g.delay * 7.0)) * u);
        a.pos = {p.x, p.y, floorAt(p)};
        a.crouch = 1.0f - u;
        glowTo(0.4f);
        if (a.variant == 2) a.wave = 1.0f - u;
        if (a.stateT >= 0.55) setState(S_IDLE), g.lastPos = a.pos;
        return;
    }
    if (a.state == S_ENTER) {  // springt ins Portal
        if (mine) {
            const vec3 to = S.portal;
            a.pos = a.pos + (vec3{to.x, to.y, a.pos.z} - a.pos) * (float)std::min(1.0, dt * 8.0);
            a.yaw = std::atan2(to.y - a.pos.y, to.x - a.pos.x) + 0.0f;
        }
        if (a.stateT > 0.55) a.state = 9;
        return;
    }
    if (!mine) {  // ohne Bande (Portal zu): fliehen, ungesehen fort
        if (!a.hasGoal) fleeGoal(a, rng_, *c.world, feet, 8.0f, 14.0f);
        a.fly = 1.0f;
        if (!a.hasGoal || !walkTo(a, a.field, 6.0, dt, c) || hdist(a.pos, a.goal) < 0.6f) fleeGoal(a, rng_, *c.world, feet, 8.0f, 14.0f);
        if (a.unseenT > 1.5) a.state = 9;
        return;
    }

    // --- Wahrnehmung -------------------------------------------------------------------------------------------------
    const float d = hdist(a.pos, feet);
    lightReact(a, c, head, 9.0f, dt, 2.5);
    const bool lit = a.light > 0.3f;
    g.lightT = lit ? g.lightT + dt : std::max(0.0, g.lightT - dt);
    const bool watched = a.sight.level > 0.3f;
    const vec3 fl = nrmF(flat(c.view.look), {1, 0, 0});
    const float behind = std::acos(std::clamp(dot(nrmF(flat(a.pos - feet), fl), fl), -1.0f, 1.0f));  // 0 vorn .. pi hinten
    const vec3 pv{(float)c.player->vx, (float)c.player->vy, 0.0f};
    const float rush = dot(pv, nrmF(flat(a.pos - feet), fl));  // wie schnell der Spieler auf ihn zukommt
    glowTo(0.3f + 0.9f * a.light + (a.state == S_PEEK ? 0.2f : 0.0f));
    g.coolT -= qdt;
    g.scareT -= qdt;
    g.chatT -= qdt;
    // ein Laut ab und zu (auch einzeln: man hoert sie, bevor man sie sieht)
    if (g.chatT <= 0.0) {
        a.talk = 1.0f;
        g.chatT = rng_.uniform(3.0, 9.0);
    } else if (g.chatT > 0.0 && g.chatT < 0.35) a.talk = 1.0f;

    // --- Einzeln: verscheucht (Zurennen, langes Anleuchten) - nicht beim Bedraengen und nicht im Handel ----------------
    const bool canScare = S.phase != P_MOB && S.phase != P_LEAVE && a.state != S_RUMMAGE && a.state != S_LOOT;
    if (canScare && !g.fleeing && ((c.player->sprinting && rush > 2.0f && d < 5.0f) || (g.lightT > 0.7 && d < 6.0f && !S.friendly))) {
        g.fleeing = true, g.scareT = rng_.uniform(2.5, 4.5);
        setState(S_SCURRY);
        fleeGoal(a, rng_, *c.world, feet, 5.0f, 8.0f);
        out.push_back({Event::Croak, Role::Goblin, head, 1.0f});
        if (a.variant == 1 && S.phase == P_SCOUT) S.alarm = true;  // ein Spaeher warnt die Bande
    }
    if (g.fleeing) {
        a.fly = 1.0f, a.alert = 1.0f;
        a.perceive = 1.0f;
        if (!a.hasGoal || !walkTo(a, a.field, 6.2, qdt, c) || hdist(a.pos, a.goal) < 0.6f) {
            if (g.scareT <= 0.0) g.fleeing = false, setState(S_IDLE), a.hasGoal = false;
            else setState(S_PEEK);
        }
        return;
    }

    // --- Auf dem Ruecken: wuehlen ---------------------------------------------------------------------------------------
    if (a.state == S_RUMMAGE) {
        a.pos = feet - fl * 0.2f + vec3{0, 0, 0.62f};
        a.yaw = (float)c.player->angle;
        a.reach = 1.0f, a.alert = 1.0f, a.talk = 0.6f;
        const float turned = std::fabs(wrapA((float)c.player->angle - g.startYaw));
        if (turned > 1.2f || c.player->sprinting || !c.player->onGround) {  // abgeschuettelt
            a.pos = feet - fl * 0.5f;
            a.pos.z = floorAt(a.pos);
            setState(S_SCURRY);
            g.fleeing = true, g.scareT = 3.0;
            fleeGoal(a, rng_, *c.world, feet, 5.0f, 8.0f);
            out.push_back({Event::Croak, Role::Goblin, head, 1.0f});
            ++S.fails;
            return;
        }
        if (a.stateT * S.quick >= 1.2) {
            a.pos = feet - fl * 0.55f;
            a.pos.z = floorAt(a.pos);
            if (c.loot && !c.loot->empty()) {
                const std::string kind = (*c.loot)[(size_t)rng_.randint(0, (i64)c.loot->size() - 1)];
                Event e{Event::Steal, Role::Goblin, head, 1.0f};
                e.item = kind;
                out.push_back(e);
                g.stash.push_back(kind);
                ++S.steals;
                setState(S_LOOT);
                log::info("Praesenz: ein Plague Goblin stiehlt {}", kind);
            } else {
                out.push_back({Event::Steal, Role::Goblin, head, 0.5f});
                out.push_back({Event::Croak, Role::Goblin, head, 0.5f});
                ++S.fails;
                g.fleeing = true, g.scareT = 2.0;
                setState(S_SCURRY);
                fleeGoal(a, rng_, *c.world, feet, 5.0f, 8.0f);
            }
        }
        return;
    }
    // --- mit Beute: zeigen, verstauen, zum Portal ------------------------------------------------------------------------
    if (a.state == S_LOOT) {
        a.bait = 1.0f;
        if (a.stateT < 0.9) {
            a.look = true;
            return;
        }
        a.fly = 1.0f;
        if (S.phase == P_FRIEND && g.claimDropped) {  // Handel: zurueck zur Bande
            g.claimDropped = false;
            setState(S_IDLE);
            return;
        }
        if (S.homeOk && walkTo(a, S.home, 6.5, qdt, c) && hdist(a.pos, S.portal) > 0.7f) return;
        if (hdist(a.pos, S.portal) <= 0.8f) setState(S_ENTER);
        else if (!S.homeOk) {
            g.fleeing = true, g.scareT = 20.0;
            fleeGoal(a, rng_, *c.world, feet, 8.0f, 14.0f);
        }
        return;
    }
    // --- Angriff (Sprung) ----------------------------------------------------------------------------------------------
    if (a.state == S_ATTACK) {
        a.strike = (float)std::min(1.0, a.stateT / 0.9);
        a.growl = 1.0f, a.jaw = 1.0f, a.look = true;
        a.heard = feet + vec3{0, 0, 0.7f};
        a.yaw += wrapA(std::atan2(feet.y - a.pos.y, feet.x - a.pos.x) - a.yaw) * (float)std::min(1.0, dt * 10.0);
        if (a.strike >= 0.52f && !g.bit) {
            g.bit = true;
            if (hdist(a.pos, feet) < 1.45f && c.mode >= 2) out.push_back({Event::Hit, Role::Goblin, head, 5.0f});
        }
        if (a.stateT >= 0.9) setState(S_THREAT), g.coolT = rng_.uniform(1.4, 2.2);
        return;
    }

    // --- Was die Bande gerade will ----------------------------------------------------------------------------------------
    const bool alone = a.alone;
    const bool hasLoot = c.loot && !c.loot->empty();
    // Ware sammeln (nicht beim Bedraengen und Abzug): Liegendes bis 7 m, das kein anderer holt
    if ((S.phase == P_ARRIVE || S.phase == P_SCOUT || S.phase == P_STEAL || S.phase == P_FRIEND) && c.loose && g.job != 1 && a.variant != 2) {
        if (!g.hasClaim && a.stateT > 0.5) {
            float bestD = 7.0f;
            const Loose* best = nullptr;
            for (const Loose& l : *c.loose) {
                const float dl = hdist(l.pos, a.pos);
                if (dl >= bestD || l.pos.z - floorAt(l.pos) > 1.2f) continue;
                bool taken = false;
                for (const vec3& cl : S.claims) taken = taken || hdist(cl, l.pos) < 0.3f;
                if (!taken) bestD = dl, best = &l;
            }
            if (best) {
                g.hasClaim = true, g.claim = best->pos, g.claimKind = best->kind, g.claimDropped = best->dropped;
                S.claims.push_back(best->pos);
                a.hasGoal = false;
            }
        }
        if (g.hasClaim) {
            bool still = false;
            for (const Loose& l : *c.loose) still = still || hdist(l.pos, g.claim) < 0.3f;
            if (!still) {  // jemand anders war schneller
                g.hasClaim = false;
                std::erase_if(S.claims, [&](const vec3& p) { return hdist(p, g.claim) < 0.3f; });
            } else if (hdist(a.pos, g.claim) < 0.4f) {
                a.reach = 1.0f;
                a.heard = g.claim;
                setState(S_IDLE);
                if (a.stateT > 0.45) {  // aufgehoben - verschwindet im Umhang
                    Event e{Event::Take, Role::Goblin, g.claim, 1.0f};
                    e.item = g.claimKind;
                    out.push_back(e);
                    g.stash.push_back(g.claimKind);
                    g.hasClaim = false;
                    std::erase_if(S.claims, [&](const vec3& p) { return hdist(p, g.claim) < 0.3f; });
                    // Handel (die Seite: Mandelwasser und Essen anbieten): hingelegt vom Spieler, in seiner Naehe
                    if (g.claimDropped && (g.claimKind == "almond" || g.claimKind == "energy") && !S.friendly && S.phase != P_MOB &&
                        hdist(g.claim, feet) < 9.0f) {
                        S.friendly = true, S.phase = P_FRIEND, S.phaseT = 0.0, S.friendT = 0.0;
                        log::info("Praesenz: Plague Goblins nehmen das Angebot an ({})", g.claimKind);
                    } else {
                        g.claimDropped = false;
                    }
                    setState(S_LOOT);
                }
                return;
            } else {
                goalTo(a, *c.world, g.claim);
                a.crouch = 0.4f;
                setState(S_WALK);
                if (!a.hasGoal || !walkTo(a, a.field, 2.4, qdt, c)) g.hasClaim = false;
                return;
            }
        }
    }

    // Lichtreaktion: im Kegel duckt er sich, schirmt ab, sieht zur Lampe (im Handel und beim Bedraengen nur kurz)
    if (lit) a.crouch = std::max(a.crouch, 0.6f), a.alert = 1.0f;

    switch (S.phase) {
        case P_ARRIVE:
        case P_SCOUT: {
            if (a.variant == 1 && S.phase == P_SCOUT) {  // Spaeher: heran, dann aus der Deckung spaehen
                const int cost = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
                const bool view = clearSight(*c.world, head, eye);
                if (a.state == S_PEEK) {
                    a.perceive = 1.0f, a.alert = 1.0f, a.crouch = std::max(a.crouch, 0.45f);
                    a.lean = std::sin((float)a.age * 0.7f) * 0.25f;
                    a.yaw += wrapA(std::atan2(feet.y - a.pos.y, feet.x - a.pos.x) - a.yaw) * (float)std::min(1.0, dt * 3.0);
                    g.peekT += qdt;
                    if ((watched && a.seenT > 0.8) || g.lightT > 0.4) {  // entdeckt: Warnschrei, zurueck zur Bande
                        out.push_back({Event::Croak, Role::Goblin, head, 1.0f});
                        S.alarm = true;
                        g.fleeing = true, g.scareT = 3.0;
                        setState(S_SCURRY);
                        a.hasGoal = false;
                        if (S.homeOk) a.field = S.home, a.goal = S.portal, a.hasGoal = true;
                    }
                    if (cost > 150 || !view) setState(S_WALK);
                } else if (cost >= 0 && cost <= 125 && view) {
                    setState(S_PEEK);
                } else {
                    setState(S_WALK);
                    a.crouch = 0.5f;
                    walkTo(a, fld(), watched ? 0.0 : 1.4, qdt, c);
                }
                break;
            }
            // Henchmen, Torwaechterin: nahe am Portal suchen, picken, umsehen (kleine Wege bis 4 m um das Portal)
            if (hdist(a.pos, S.portal) > 5.0f && S.homeOk) {
                setState(S_WALK);
                walkTo(a, S.home, 1.0, qdt, c);
            } else if (a.hasGoal && hdist(a.pos, a.goal) > 0.4f && walkTo(a, a.field, 0.7, qdt, c)) {
                setState(S_WALK);
            } else {
                setState(S_IDLE);
                a.hasGoal = false;
                a.timer2 -= qdt;
                if (a.timer2 <= 0.0) {
                    a.timer2 = rng_.uniform(2.5, 7.0);
                    const float ang = (float)rng_.uniform(-kPiF, kPiF), r = (float)rng_.uniform(1.0, 4.0);
                    goalTo(a, *c.world, S.portal + vec3{std::cos(ang), std::sin(ang), 0.0f} * r);
                }
                const double ph = std::fmod(a.age + g.delay * 3.0, 6.0);
                if (ph < 1.6) a.reach = 1.0f, a.crouch = 0.5f;  // pickt am Boden
                if (ph > 4.0) a.perceive = 1.0f, a.perceiveAt = a.pos + dirYaw(a.yaw + (float)std::sin(a.age) * 1.4f) * 3.0f + vec3{0, 0, 0.4f};
            }
            if (a.variant == 2) a.perceive = 1.0f, a.alert = 0.5f;
            break;
        }
        case P_STEAL: {
            const bool canSteal = alone && hasLoot;
            // Aufgaben: die beiden ersten freien Henchmen stehlen, der Rest umkreist
            if (g.job == 0 && a.variant == 0) {
                int thieves = 0;
                for (const auto& ap : agents_)
                    if (ap->role == Role::Goblin && ap->group == S.group && ap->ext && gag(*ap).job == 1 && ap->state != S_DEAD) ++thieves;
                g.job = thieves < 2 ? 1 : 2;
            } else if (g.job == 0) {
                g.job = 2;
            }
            if (a.variant == 2) {  // Torwaechterin: bleibt am Portal, beobachtet
                setState(S_IDLE);
                a.perceive = 1.0f, a.alert = 0.4f;
                if (hdist(a.pos, S.portal) > 3.0f && S.homeOk) setState(S_WALK), walkTo(a, S.home, 1.0, qdt, c);
                if (std::fmod(a.age, 11.0) < 1.0) a.wave = 1.0f;
                break;
            }
            // Spaeher warnen vor anderen Wesen (die Seite: sie meiden andere Wesen)
            if (a.variant == 1) {
                for (const auto& ap : agents_)
                    if (hostile(ap->role) && !ap->showcase && hdist(ap->pos, feet) < 20.0f && !g.warned) {
                        g.warned = true;
                        out.push_back({Event::Croak, Role::Goblin, head, 1.0f});
                        S.phase = P_LEAVE, S.phaseT = 0.0;
                    }
            }
            const int cost = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
            if (g.job == 1 && canSteal) {
                // Dieb: nur ungesehen naeher; vor dem Beil Abstand
                const bool blade = c.hatchet && behind < 1.2f && d < 3.2f;
                if (watched || blade) {
                    setState(S_IDLE);
                    a.crouch = 1.0f, a.perceive = 1.0f, a.alert = 1.0f;
                    if (blade && d < 2.6f) walkTo(a, a.field, 0.0, qdt, c);
                } else if (d < 1.05f && behind > 1.9f && !c.player->sprinting) {
                    setState(S_RUMMAGE);
                    g.startYaw = (float)c.player->angle;
                    out.push_back({Event::Steal, Role::Goblin, feet + vec3{0, 0, 1.1f}, 0.0f});
                } else {
                    // hinter den Ruecken: ueber das Wegefeld zum Spieler, zuletzt direkt zur Stelle hinter ihm
                    setState(d > 6.0f ? S_SCURRY : S_WALK);
                    a.fly = d > 6.0f ? 1.0f : 0.0f;
                    a.crouch = 0.75f;
                    const vec3 spot = feet - fl * 0.75f;
                    if (d < 3.0f && goalTo(a, *c.world, spot)) walkTo(a, a.field, 1.6, qdt, c);
                    else walkTo(a, fld(), d > 6.0f ? 4.5 : 1.4, qdt, c);
                }
                break;
            }
            // umkreisen und spaehen (5-10 m); wer allein ist, wird von einem vorn abgelenkt
            if (cost > 100 && !watched) {
                setState(cost > 160 ? S_SCURRY : S_WALK);
                a.fly = cost > 160 ? 1.0f : 0.0f;
                a.crouch = 0.5f;
                walkTo(a, fld(), cost > 160 ? 4.0 : 1.2, qdt, c);
            } else if (cost >= 0 && cost < 50) {
                if (!a.hasGoal) fleeGoal(a, rng_, *c.world, feet, 3.0f, 5.0f);
                setState(S_WALK);
                if (!a.hasGoal || !walkTo(a, a.field, 1.3, qdt, c)) a.hasGoal = false;
            } else {
                setState(S_PEEK);
                a.perceive = 1.0f, a.alert = 0.7f, a.crouch = std::max(a.crouch, 0.35f);
                a.yaw += wrapA(std::atan2(feet.y - a.pos.y, feet.x - a.pos.x) - a.yaw) * (float)std::min(1.0, dt * 3.0);
                a.lean = std::sin((float)a.age * 0.9f + (float)g.delay) * 0.3f;
            }
            break;
        }
        case P_MOB: {
            a.look = true;
            a.yaw += wrapA(std::atan2(feet.y - a.pos.y, feet.x - a.pos.x) - a.yaw) * (float)std::min(1.0, dt * 8.0);
            if (a.variant == 2) {  // Torwaechterin: Stock hoch, kraechzt Befehle, bleibt auf Abstand
                a.wave = 1.0f, a.growl = 1.0f, a.jaw = 0.6f;
                setState(S_THREAT);
                if (d < 3.5f && !a.hasGoal) fleeGoal(a, rng_, *c.world, feet, 4.0f, 5.0f);
                if (a.hasGoal && !walkTo(a, a.field, 1.6, qdt, c)) a.hasGoal = false;
                break;
            }
            a.growl = 1.0f, a.jaw = 0.8f, a.crouch = 0.6f;
            if (S.mobT < 1.4) {  // Drohen: naeher heran (bis 3 m), Masken hoch, Umhang gespreizt
                setState(S_THREAT);
                if (d > 3.0f) walkTo(a, fld(), 2.5, qdt, c);
                break;
            }
            // Angreifer: hoechstens drei zugleich
            int attackers = 0;
            for (const auto& ap : agents_)
                if (ap->role == Role::Goblin && ap->group == S.group && ap->ext && gag(*ap).job == 3 && ap->state != S_DEAD) ++attackers;
            if (g.job != 3 && attackers < 3) g.job = 3;
            if (g.job != 3) {  // die anderen bleiben im Kreis (2,5-4 m)
                setState(S_THREAT);
                if (d > 4.0f) walkTo(a, fld(), 2.5, qdt, c);
                break;
            }
            if (g.coolT > 0.0) {  // nach einem Sprung: zurueck auf Abstand
                setState(S_THREAT);
                if (d < 2.4f) {
                    if (!a.hasGoal) fleeGoal(a, rng_, *c.world, feet, 2.5f, 3.5f);
                    if (a.hasGoal && !walkTo(a, a.field, 3.0, qdt, c)) a.hasGoal = false;
                }
                break;
            }
            a.hasGoal = false;
            if (d > 1.3f) {
                setState(S_SCURRY);
                a.fly = 1.0f;
                walkTo(a, fld(), 5.0, qdt, c);
            } else {
                setState(S_ATTACK);
                g.bit = false;
                out.push_back({Event::Screech, Role::Goblin, head, 1.0f});
            }
            break;
        }
        case P_FRIEND: {
            a.look = true;
            if (a.variant == 2) {  // Torwaechterin: tritt vor, bietet an, ahmt ein Wort nach, legt ein Geschenk hin
                if (d > 2.2f) {
                    setState(S_WALK);
                    walkTo(a, fld(), 1.0, qdt, c);
                } else {
                    setState(S_TRADE);
                    a.plea = 1.0f, a.talk = 0.5f, a.tilt = 0.35f;
                    a.yaw += wrapA(std::atan2(feet.y - a.pos.y, feet.x - a.pos.x) - a.yaw) * (float)std::min(1.0, dt * 4.0);
                    if (!S.mimicDone && a.stateT > 0.8) {
                        S.mimicDone = true;
                        out.push_back({Event::Mimic, Role::Goblin, head, 1.0f});
                    }
                    if (!S.giftDone && a.stateT > 2.0) {
                        S.giftDone = true;
                        ++S.gifts;
                        Event e{Event::Gift, Role::Goblin, a.pos + dirYaw(a.yaw) * 0.35f + vec3{0, 0, 0.15f}, 1.0f};
                        e.item = "battery";
                        out.push_back(e);
                    }
                }
                break;
            }
            // Spaeher warnen vor Gefahr (die Seite: "warn them of potential danger")
            if (a.variant == 1) {
                S.warnT -= qdt;
                for (const auto& ap : agents_)
                    if (hostile(ap->role) && !ap->showcase && hdist(ap->pos, feet) < 25.0f && S.warnT <= 0.0) {
                        S.warnT = 6.0;
                        out.push_back({Event::Croak, Role::Goblin, head, 1.0f});
                        a.perceive = 1.0f, a.perceiveAt = ap->pos + vec3{0, 0, 0.8f};
                        if (S.friendT > 8.0) S.phase = P_LEAVE, S.phaseT = 0.0;
                    }
            }
            // die anderen sitzen in 3-5 m herum, schnattern, ohne Scheu
            const int cost = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
            if (cost > 60) {
                setState(S_WALK);
                walkTo(a, fld(), 1.1, qdt, c);
            } else {
                setState(S_IDLE);
                a.perceive = std::fmod(a.age + g.delay, 5.0) < 2.5 ? 1.0f : 0.0f;
                a.tilt = a.perceive * 0.3f;
            }
            break;
        }
        case P_LEAVE: {
            const bool hurry = S.routed || !g.stash.empty();
            if (a.variant == 2) {  // die Torwaechterin geht zuletzt
                int others = 0;
                for (const auto& ap : agents_)
                    if (ap->role == Role::Goblin && ap->group == S.group && ap->variant != 3 && ap->variant != 2 && ap->state != S_DEAD) ++others;
                if (hdist(a.pos, S.portal) > 0.8f) {
                    setState(hurry ? S_SCURRY : S_WALK);
                    a.fly = hurry ? 0.3f : 0.0f;
                    if (!S.homeOk || !walkTo(a, S.home, hurry ? 3.2 : 1.6, qdt, c)) g.stuckT += qdt;
                } else if (others == 0 || S.phaseT > 16.0) {
                    a.wave = 1.0f;
                    setState(S_IDLE);
                    if (g.coolT < -0.8 || S.phaseT > 18.0) setState(S_ENTER);
                } else {
                    g.coolT = 0.0;
                    setState(S_IDLE);
                    a.perceive = 1.0f;
                    a.yaw += wrapA(S.portalYaw + kPiF - a.yaw) * (float)std::min(1.0, dt * 3.0);
                }
            } else if (hdist(a.pos, S.portal) <= 0.8f) {
                setState(S_ENTER);
            } else {
                setState(hurry ? S_SCURRY : S_WALK);
                a.fly = hurry ? 1.0f : 0.0f;
                if (!S.homeOk || !walkTo(a, S.home, hurry ? 6.5 : 2.2, qdt, c)) g.stuckT += qdt;
            }
            if (g.stuckT > 2.0) {  // kein Weg zum Portal: anders fort (ungesehen)
                g.fleeing = true, g.scareT = 30.0;
                fleeGoal(a, rng_, *c.world, feet, 8.0f, 14.0f);
                if (a.unseenT > 2.0) a.state = 9;
            }
            break;
        }
        default: break;
    }
    // Stillstand merken (fuer "kein Weg")
    if (hdist(a.pos, g.lastPos) > 0.05f) g.stuckT = std::max(0.0, g.stuckT - dt), g.lastPos = a.pos;
    (void)sc;
}

}  // namespace lim::game
