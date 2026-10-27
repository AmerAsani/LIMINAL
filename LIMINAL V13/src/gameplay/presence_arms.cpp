// V13.3: Arms / Les Bras (Entite 15-FR, Wikidot FR Rev. 44; Level 1 Rev. 83) - Auftreten und Verhalten (KREATUR_Arms.md).
//
// Die Seiten: ein langes, verdrehtes, verwesendes Glied mit drei Krallen, das in Lueftungsschaechten lebt. Es kommt nur
// heraus, wenn es darunter dunkel ist (Stromausfall in Level 1) - und von Laerm angelockt. Vorher: dumpfes Schaben an Metall
// im Schacht, das Oeffnen der Lueftung ist sehr leise. Dann packt es den Wanderer und zieht ihn in den Schacht. Trifft es
// starkes, direktes Licht, bevor es zupackt, gleitet es mit einem heiseren Todesschrei zurueck; unter dem Gitter liegen
// danach verbrannte, rauchende Hautfetzen.
//
//   0 im Schacht   hoert Laerm im Dunkeln (Weg <= Laerm x 14 m: rennen 14 m, gehen 6 m, schleichen 2 m)
//   1 schaben      2,4 s und laenger Schaben und Klopfen im Schacht (Ankuendigung); weiterer Laerm oder Naehe: es kommt
//   2 heraus       die Lueftung oeffnet sich (leise), der Arm gleitet herab und tastet zum Laerm, zum Spieler
//   3 ausholen     ist der Spieler in Reichweite (2,85 m von der Oeffnung) und laut oder nah: die Krallen spreizen sich (0,55 s)
//   4 zuschlagen   blitzschnell; wer noch in Reichweite ist, wird gepackt (toedlich) - sonst schnappen die Krallen ins Leere
//   5 verbrennt    Licht der Lampe auf dem Arm (0,25 s): Schrei, er schnellt zurueck, Hautfetzen fallen, Rauch
//   6 zurueck      Licht im Raum, oder lange nichts gehoert: langsam zurueck in den Schacht
//   7 Pause        im Schacht, unter der Oeffnung rauchen die Fetzen
//   10 Zugriff     die Krallen schliessen sich, der Arm zieht den Spieler hinauf in den Schacht
//
// Lager: Deckenlueftungen der Karte (ceiling_vent, auch die Lager des Buendels), Rohrgaenge (Rohre an der Decke) - sonst ein
// Riss in der Decke eines dunklen Gangs (die Level-1-Seite: durch Lueftungen und Risse).
// Felder des Agenten: home Oeffnung (Decke), restN Normale (0, 0, -1), hang 1 = eigener Riss, heard / lostT / provoke:
// Laerm (wo, seit wann, wie viel), timer naechstes Schaben, timer2 Licht auf dem Arm, burst Fetzen liegen, cool Pause.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kOut = anim::ar::kOutMax;  // so weit gleitet er heraus
constexpr float kStrike = 2.85f;     // Reichweite (Oeffnung - Brust)
constexpr float kHover = 1.45f;      // so weit heraus tastet er
constexpr float kMaxCeil = 3.4f;     // nur unter niedrigen Decken (Gaenge, Raeume)

struct Lair {
    vec3 p;  // an der Decke
    bool crack = false;
    float yaw = 0.0f;
};

// Lager in der Naehe: Lueftungen und Rohre der Karte (aus den Sektoren um den Spieler)
void lairsNear(const Presence::Context& c, std::vector<Lair>& out) {
    out.clear();
    static const props::Model* vents[2] = {props::model("ceiling_vent", 0), props::model("ceiling_vent", 1)};
    const vec3 eye = c.view.eye;
    for (int sy = -1; sy <= 1; ++sy)
        for (int sx = -1; sx <= 1; ++sx) {
            const double qx = c.player->x + sx * 40.0, qy = c.player->y + sy * 40.0;
            const world::Sector* sec = c.world->sectorIfReady(qx, qy);
            if (!sec) continue;
            for (const auto& pr : sec->props)
                if (pr.model && (pr.model == vents[0] || pr.model == vents[1]) && hdist({pr.x, pr.y, 0}, eye) < 40.0f) out.push_back({{pr.x, pr.y, pr.z}, false, pr.yaw});
            for (const auto& sp : sec->spots)
                if (sp.kind == "pipes" && hdist({sp.x, sp.y, 0}, eye) < 40.0f) out.push_back({{sp.x, sp.y, sp.z}, true, sp.yaw});
        }
    // doppelte (benachbarte Sektoren liefern dieselben) entfernen
    std::vector<Lair> u;
    for (const Lair& l : out) {
        bool d = false;
        for (const Lair& m : u) d = d || hdist(l.p, m.p) < 0.3f;
        if (!d) u.push_back(l);
    }
    out.swap(u);
}
}  // namespace

bool Presence::spawnArms(const Context& c, std::vector<Event>& out, bool forced) {
    if (!c.kin || !fld().valid()) return false;
    const world::KinDef& K = c.kin[world::KIN_ARMS];
    const vec3 eye = c.view.eye;
    std::vector<Lair> lairs;
    lairsNear(c, lairs);
    // Kandidaten: Weg 3..16 m (erzwungen: 0..6 m), dunkel darunter, niedrige Decke, Buendel nicht dort
    Lair best;
    float bestScore = 1e9f;
    auto consider = [&](Lair l) {
        const i64 x = ifloor(l.p.x), y = ifloor(l.p.y);
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || !t->open() || t->clearance() > kMaxCeil || t->clearance() < 2.0f) return;
        l.p.z = t->ceil;
        const int cost = fld().at(x, y);
        if (cost < 0) return;
        const float pathD = (float)cost / 10.0f;
        if (forced ? pathD > 7.0f : (pathD < 3.0f || pathD > 16.0f)) return;
        if (!forced && !darkTile(c, t, l.p.x, l.p.y)) return;
        for (const auto& a : agents_)
            if (!a->showcase && hdist(a->pos, l.p) < 1.5f && (a->role == Role::Clump || a->role == Role::Arms)) return;
        // ein eigener Riss darf nicht vor den Augen entstehen
        if (l.crack && !forced && dot(nrmF(l.p - vec3{0, 0, 0.05f} - eye, {1, 0, 0}), c.view.look) > 0.35f &&
            clearSight(*c.world, eye, l.p - vec3{0, 0, 0.08f}))
            return;
        const float score = (forced ? pathD : std::fabs(pathD - 7.0f)) + (l.crack ? 3.0f : 0.0f) + (float)rng_.uniform(0.0, 2.0);
        if (score < bestScore) bestScore = score, best = l;
    };
    for (const Lair& l : lairs) consider(l);
    // keine Lueftung in der Naehe: ein Riss in der Decke eines dunklen Gangs oder Raums (die Level-1-Seite: "und Risse")
    if (bestScore > 1e8f && (forced || rng_.uniform(0.0, 1.0) < K.chance)) {
        std::vector<std::pair<i64, i64>> cand;
        fld().ring(forced ? 10 : 30, forced ? 45 : 100, cand);
        for (int tries = 0; tries < 30 && !cand.empty() && bestScore > 1e8f; ++tries) {
            auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
            const world::Tile* t = c.world->tileIfLoaded(x, y);
            if (!t || !t->open() || !t->room || (t->flags & (world::TF_LAMP | world::TF_DOOR | world::TF_STAIR))) continue;
            if (!roomAllowed(c, {(float)x + 0.5f, (float)y + 0.5f, t->floor}, world::KIN_ARMS)) continue;
            consider({{(float)x + 0.5f, (float)y + 0.5f, t->ceil}, true, (float)rng_.uniform(-kPiF, kPiF)});
        }
    }
    if (bestScore > 1e8f) return false;
    auto a = std::make_unique<Agent>();
    a->role = Role::Arms;
    a->ent = Entity::Arms;
    a->noShadow = true;
    a->glow = 1.0f;
    a->home = best.p;
    a->restN = {0, 0, -1};
    a->up = a->restN;
    a->hang = best.crack ? 1.0f : 0.0f;
    // der Schacht laeuft quer zur Richtung des Spielers weg (er verschwindet in der Decke, nicht ueber ihm)
    const float yaw = std::atan2(best.p.y - eye.y, best.p.x - eye.x);
    a->initBody(best.p, yaw, (unsigned)rng_.randint(1, 1 << 30));
    a->rig.setGlow(1.0f);
    a->heard = {(float)c.player->x, (float)c.player->y, (float)c.player->z};
    a->lostT = 99.0;
    if (forced) a->state = 2, a->stateT = 0.0, a->lostT = 0.0;  // Skript/Test: gleich heraus
    out.push_back({Event::Appear, Role::Arms, best.p});
    log::info("Praesenz: Arms in {} bei ({:.1f}, {:.1f}), Decke {:.1f} m{}", best.crack ? "einem Riss" : "einer Lueftung", best.p.x, best.p.y, best.p.z,
              forced ? " (erzwungen)" : "");
    agents_.push_back(std::move(a));
    return true;
}

void Presence::regieArms(double dt, const Context& c, std::vector<Event>& out) {
    const world::KinDef& K = c.kin[world::KIN_ARMS];
    if (!K.on || has(Role::Arms)) return;
    // nur im Dunkeln (Stromausfall, unbeleuchtete Stellen)
    const world::Tile* pt = c.world->tileIfLoaded(ifloor(c.player->x), ifloor(c.player->y));
    if (!darkTile(c, pt, (float)c.player->x, (float)c.player->y)) return;
    nextArms_ -= dt;
    if (nextArms_ <= 0.0) nextArms_ = spawnArms(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 5.0;
}

void Presence::thinkArms(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const vec3 chest = eye - vec3{0, 0, 0.35f};
    const vec3 O = a.home;
    a.pos = O;
    a.stateT += dt;
    a.cool -= dt;
    a.timer -= dt;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    const world::Tile* below = c.world->tileIfLoaded(ifloor(O.x), ifloor(O.y));
    const bool dark = darkTile(c, below, O.x, O.y);
    // Laerm: Weg vom Spieler zur Oeffnung
    const int cost = fld().at(ifloor(O.x), ifloor(O.y));
    const float pathD = cost >= 0 ? (float)cost / 10.0f : 1e9f;
    bool heard = c.noise > 0.05 && pathD < (float)c.noise * 14.0f + 0.5f;
    vec3 heardAt = feet;
    if (c.pings)
        for (const auto& [q, s] : *c.pings)
            if (hdist(O, q) < s * 14.0f) heard = true, heardAt = q;
    if (heard) a.heard = heardAt, a.lostT = 0.0;
    else a.lostT += dt;
    const float dC = length(chest - O);  // Oeffnung - Brust
    const float dH = hdist(feet, O);
    // Licht: trifft der Lichtkegel den Arm (Hand, Unterarm), verbrennt er
    const vec3 hand = a.cre.headPos();
    const vec3 mid = O + (hand - O) * 0.55f;
    const bool out_ = a.state >= 2 && a.state <= 4;
    const bool lit = out_ && a.reach > 0.15f && (inBeam(c.view, *c.world, hand, 12.0f) || inBeam(c.view, *c.world, mid, 12.0f));
    a.timer2 = lit ? a.timer2 + dt : 0.0;
    a.alert = 0.0f, a.jaw = 0.0f, a.light = 0.0f, a.perceive = 0.0f;
    a.shiver = std::max(0.0f, a.shiver - (float)dt / 9.0f);  // Rauch verfliegt
    // gesehen (die Hand im Blick): zum ersten Mal - Verstand
    if (out_ && !a.sighted && a.reach > 0.2f && dot(nrmF(hand - eye, {1, 0, 0}), c.view.look) > 0.6f && clearSight(*c.world, eye, hand)) {
        a.sighted = true;
        ++encounters;
        out.push_back({Event::FirstSight, Role::Arms, hand, 1.0f});
    }
    auto burn = [&]() {
        setState(5);
        a.burst = 1.0, a.shiver = 1.0f;
        out.push_back({Event::Burn, Role::Arms, hand, 1.0f});
        log::info("Praesenz: Arms verbrennt im Licht");
    };
    switch (a.state) {
        case 0: {  // im Schacht: lauscht
            a.reach = 0.0f;
            if (dark && heard && a.cool <= 0.0) {
                setState(1);
                a.timer = 0.0, a.knocks = 0, a.provoke = 0.0;
            }
            // weit weg (oder lange hell): fort - die Fetzen nur, wenn man sie nicht sieht
            if ((pathD > 24.0f || (!dark && a.stateT > 25.0)) && (a.burst < 0.5 || hdist(O, eye) > 16.0f)) a.state = 9;
            break;
        }
        case 1: {  // Schaben und Klopfen im Schacht (dumpf, an Metall)
            a.reach = 0.0f;
            if (a.timer <= 0.0) {
                a.timer = rng_.uniform(0.75, 1.3);
                out.push_back({Event::Scrape, Role::Arms, O + vec3{0, 0, 0.25f}, (float)std::min(1.0, 0.4 + 0.15 * a.knocks)});
                ++a.knocks;
            }
            if (heard && a.stateT > 0.6) a.provoke += dt;
            if (!dark) {
                setState(0), a.cool = 4.0;
            } else if (a.stateT > 2.4 && (a.provoke > 0.4 || dH < 3.0f) && pathD < 9.0f) {
                setState(2);
                out.push_back({Event::Burst, Role::Arms, O, 0.3f});  // die Lueftung oeffnet sich, sehr leise
                log::info("Praesenz: Arms kommt aus dem Schacht");
            } else if (a.lostT > 6.0) {
                setState(0), a.cool = 3.0;
            }
            break;
        }
        case 2: {  // heraus: gleitet herab, tastet zum Laerm
            a.reach = kHover / kOut;
            a.perceive = 1.0f, a.perceiveAt = a.heard;
            if (a.timer2 > 0.25) {
                burn();
                break;
            }
            if (!dark || (a.lostT > 10.0 && dH > 4.0f)) {
                setState(6);
                out.push_back({Event::Retract, Role::Arms, O, 0.6f});
                break;
            }
            if (a.stateT > 1.2 && a.cool <= 0.0 && dC < kStrike && (heard || dH < 1.6f) && c.mode >= 2) {
                setState(3);
                out.push_back({Event::Windup, Role::Arms, hand, 1.0f});
            }
            break;
        }
        case 3: {  // ausholen: die Krallen spreizen sich
            a.reach = kHover / kOut;
            a.alert = 1.0f;
            if (a.timer2 > 0.2) {
                burn();
                break;
            }
            if (a.stateT > 0.55) setState(4);
            break;
        }
        case 4: {  // zuschlagen
            a.reach = 1.0f;
            if (a.timer2 > 0.15) {
                burn();
                break;
            }
            if (a.stateT > 0.2) {
                if (dC < kStrike + 0.1f && c.mode >= 2 && std::fabs(feet.z - (O.z - (below ? below->clearance() : 2.6f))) < 1.0f) {
                    out.push_back({Event::Attack, Role::Arms, hand, 1.0f});
                    setState(10);
                    log::info("Praesenz: Arms - toedlicher Zugriff");
                } else {  // ins Leere: die Krallen schnappen zu
                    out.push_back({Event::Drop, Role::Arms, hand, 0.7f});
                    setState(2), a.cool = 1.5;
                }
            }
            break;
        }
        case 5: {  // verbrannt: Schrei, schnellt zurueck, Fetzen fallen
            a.light = 1.0f;
            a.reach = 0.0f;
            if (a.stateT > 0.9) setState(7), a.cool = rng_.uniform(30.0, 50.0);
            break;
        }
        case 6: {  // langsam zurueck
            a.reach = 0.0f;
            if (a.stateT > 2.2) setState(0), a.cool = 4.0;
            break;
        }
        case 7: {  // Pause im Schacht (die Fetzen rauchen)
            a.reach = 0.0f;
            if (a.cool <= 0.0) setState(0);
            if (pathD > 24.0f && hdist(O, eye) > 16.0f) a.state = 9;
            break;
        }
        case 10: {  // Zugriff: die Krallen schliessen sich um die Brust, dann hinauf in den Schacht
            a.jaw = 1.0f;
            a.reach = a.stateT < 0.45 ? 1.0f : 0.0f;
            if (a.stateT > 2.6) a.state = 9;
            break;
        }
        default: break;
    }
}

}  // namespace lim::game
