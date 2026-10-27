// V13.3: Duller (Entity 6, Wikidot Rev. 52; Level 1 Rev. 83) - Auftreten und Verhalten (KREATUR_Duller.md).
//
// Die Seite: der Duller jagt, indem er in einem Gang steht und sein Opfer im Gang direkt hinter der Wand hat - er streckt
// den Arm durch die Wand, packt und zieht es zu sich. Bemerkt man ihn ausserhalb dieser Jagd, rennt er lautlos davon
// (laeuft man auf ihn zu, flieht er). Er meidet Mandelwasser. Level 1: Schaben oder Rascheln in den Waenden - Abstand halten.
//
//   0 lauern     ungesehen in der Naehe, hoert den Spieler durch die Waende (Schritte), sucht eine Wand, hinter der er
//                ihn hat (Hinterhalt: Spieler hoechstens 1,25 m vor der Wand, dahinter 1-3 Kacheln Wand und ein Gang)
//   1 anschleichen  zum Standplatz hinter der Wand (folgt dem Spieler die Wand entlang)
//   2 schaben    3 s Schaben in der Wand neben dem Spieler (die Ankuendigung), ab 1,4 s woelbt sich der Putz
//   3 Durchbruch die Wand bricht, der Arm schiesst heraus; wer noch in Reichweite (1,75 m) ist, wird gepackt (toedlich)
//   4 tasten     daneben: der Arm tastet 1,6 s nach dem Spieler, dann
//   5 zurueck    der Arm gleitet zurueck; ist die Bruchstelle ungesehen, ist die Wand wieder heil (Level 1: unbeobachtet
//                verschwindet, was sich veraendert hat), er lauert wieder (Pause)
//   7/6 gesehen  er erstarrt kurz (der Kopf schnellt herum) und rennt lautlos fort; ungesehen verschwindet er
//   10 Zugriff   die Hand schliesst sich um den Spieler und zieht ihn zur Wand
//
// Felder des Agenten: rest Standplatz, restN Normale der Wand (zum Opfer), home Mitte der Bruchstelle (Wandflaeche des
// Opfers, 1,45 m hoch), radius Abstand Standplatz -> Wandflaeche des Opfers, heard / lostT wo und seit wann er den Spieler
// zuletzt hoerte, recog so lange ist die Bruchstelle ungesehen, burst Bruchstelle sichtbar, knocks Knacken der Woelbung,
// timer naechstes Schaben, timer2 naechste Planung, cool Pause bis zum naechsten Anlauf.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kStandOff = 0.42f;  // Abstand zur eigenen Wandseite (anim/creature_duller.cpp: nearFace)
constexpr float kReach = 1.75f;     // so weit (waagrecht) greift der Arm von der Bruchstelle
constexpr float kNear = 1.25f;      // so nah muss der Spieler an der Wand stehen
constexpr double kScrape = 3.0;     // Schaben bis zum Durchbruch (Ankuendigung)
constexpr double kBulgeAt = 1.4;    // ab hier woelbt sich der Putz
constexpr float kClear = 2.0f;      // freie Hoehe fuer seine Wege (gebueckt)

struct Ambush {
    vec3 stand, n, breach;
    float D = 0.0f, dist = 9.0f;
    bool ok = false;
};

// Hinterhalt fuer einen Spieler an p (Fuesse), Auge eye: die naechste Wand in einer der vier Richtungen
Ambush findAmbush(const Presence::Context& c, vec3 p, vec3 eye, const std::function<bool(vec3)>& allowed) {
    world::World& w = *c.world;
    Ambush best;
    const i64 px = ifloor(p.x), py = ifloor(p.y);
    const world::Tile* pt = w.tileIfLoaded(px, py);
    if (!pt || !pt->open()) return best;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
        const bool ax = dx[k] != 0;  // Wand quer zu x
        const int sg = dx[k] + dy[k];
        i64 tx = px, ty = py;
        bool hit = false;
        for (int st = 0; st < 2 && !hit; ++st) {
            tx += dx[k], ty += dy[k];
            const world::Tile* t = w.tileIfLoaded(tx, ty);
            if (!t) break;
            hit = t->solid();
        }
        if (!hit) continue;
        const float face = ax ? (sg > 0 ? (float)tx : (float)tx + 1.0f) : (sg > 0 ? (float)ty : (float)ty + 1.0f);
        const float pc = ax ? p.x : p.y;
        const float dist = sg > 0 ? face - pc : pc - face;
        if (dist > kNear || dist < 0.05f || dist > best.dist) continue;
        // Dicke 1..3 Kacheln, dahinter frei; quer +-0,38 m alles Wand (Platz fuer die Bruchstelle, keine Tuer)
        const float lat = ax ? p.y : p.x;
        int thick = 0;
        bool wallOk = true;
        for (; thick < 4; ++thick) {
            const i64 ox = tx + dx[k] * thick, oy = ty + dy[k] * thick;
            const world::Tile* t = w.tileIfLoaded(ox, oy);
            if (!t) {
                wallOk = false;
                break;
            }
            if (!t->solid()) break;
            for (float o : {-0.38f, 0.38f}) {
                const i64 qx = ax ? ox : ifloor(lat + o), qy = ax ? ifloor(lat + o) : oy;
                const world::Tile* q = w.tileIfLoaded(qx, qy);
                wallOk = wallOk && q && q->solid();
            }
        }
        if (!wallOk || thick < 1 || thick > 3) continue;
        // Standplatz hinter der Wand
        const float sc = face + (float)sg * ((float)thick + kStandOff);
        const vec3 stand2{ax ? sc : lat, ax ? lat : sc, 0.0f};
        const world::Tile* stT = w.tileIfLoaded(ifloor(stand2.x), ifloor(stand2.y));
        if (!figureWalkable(stT, kClear + 0.05f) || std::fabs(stT->floor - pt->floor) > 0.3f) continue;
        bool body = true;  // Platz fuer den Koerper quer
        for (float o : {-0.26f, 0.26f}) {
            const vec3 q = stand2 + (ax ? vec3{0, o, 0} : vec3{o, 0, 0});
            body = body && figureWalkable(w.tileIfLoaded(ifloor(q.x), ifloor(q.y)), 1.9f);
        }
        if (!body) continue;
        const vec3 stand{stand2.x, stand2.y, stT->floor};
        if (clearSight(w, eye, stand + vec3{0, 0, 1.0f}) || clearSight(w, eye, stand + vec3{0, 0, 1.9f})) continue;  // nie in Sicht
        if (allowed && !allowed(stand)) continue;
        best.ok = true;
        best.stand = stand;
        best.n = {-(float)dx[k], -(float)dy[k], 0.0f};
        best.breach = {ax ? face : lat, ax ? lat : face, pt->floor + anim::du::kBreachH};
        best.D = (float)thick + kStandOff;
        best.dist = dist;
    }
    return best;
}
}  // namespace

bool Presence::spawnDuller(const Context& c, std::vector<Event>& out, bool forced) {
    if (!c.kin || !fld().valid()) return false;
    const auto allowed = [&](vec3 q) { return roomAllowed(c, q, world::KIN_DULLER); };
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    auto make = [&](vec3 p, float yaw) {
        auto a = std::make_unique<Agent>();
        a->role = Role::Duller;
        a->ent = Entity::Duller;
        a->glow = 1.0f;
        a->initBody(p, yaw, (unsigned)rng_.randint(1, 1 << 30));
        a->rig.setGlow(1.0f);
        a->heard = feet;
        a->timer2 = rng_.uniform(0.3, 1.0);
        out.push_back({Event::Appear, Role::Duller, p});
        log::info("Praesenz: Duller bei ({:.1f}, {:.1f}), {:.0f} m{}", p.x, p.y, hdist(p, c.view.eye), forced ? " (erzwungen)" : "");
        agents_.push_back(std::move(a));
        return agents_.back().get();
    };
    if (forced) {  // Skript/Test: gleich hinter die Wand neben dem Spieler, sofort schaben
        const Ambush am = findAmbush(c, feet, c.view.eye, {});
        if (am.ok) {
            Agent* a = make(am.stand, std::atan2(am.n.y, am.n.x));
            a->rest = am.stand, a->restN = am.n, a->home = am.breach, a->radius = am.D;
            a->state = 2, a->stateT = 0.0, a->timer = 0.0, a->burst = 1.0, a->knocks = 0;
            return true;
        }
    }
    vec3 p;
    if (!hiddenSpawn(c, 120, 300, false, p, [&](vec3 q) {
            const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
            return figureWalkable(t, kClear) && allowed(q);
        }))
        return false;
    make(p, (float)rng_.uniform(-kPiF, kPiF));
    return true;
}

void Presence::regieDuller(double dt, const Context& c, std::vector<Event>& out) {
    const world::KinDef& K = c.kin[world::KIN_DULLER];
    if (!K.on || has(Role::Duller)) return;
    nextDuller_ -= dt;
    if (nextDuller_ <= 0.0) nextDuller_ = spawnDuller(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 8.0;
}

void Presence::thinkDuller(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const world::KinDef& K = c.kin ? c.kin[world::KIN_DULLER] : world::KinDef{};
    const auto allowed = [&](vec3 q) { return roomAllowed(c, q, world::KIN_DULLER); };
    a.stateT += dt;
    a.cool -= dt;
    a.timer -= dt;
    a.timer2 -= dt;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    auto leave = [&]() {
        a.state = 9;
        nextDuller_ = std::max(nextDuller_, rng_.uniform(K.every * 0.6, K.every * 1.1) * c.stalkerPause);
    };
    // Bruchstelle in Sicht? (sonst heilt die Wand, sobald er fort ist)
    const vec3 n = a.restN;
    const vec3 bv = a.home + n * 0.06f;
    const bool breachSeen = a.burst > 0.5 && hdist(bv, eye) < 30.0f && dot(nrmF(bv - eye, {1, 0, 0}), c.view.look) > 0.45f && clearSight(*c.world, eye, bv);
    a.recog = breachSeen ? 0.0 : a.recog + dt;
    // was er hoert: Schritte des Spielers durch die Waende (Weg oder Luftlinie x 1,6), geworfene Dinge
    {
        const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
        const float pathD = std::min(here >= 0 ? (float)here / 10.0f : 1e9f, hdist(a.pos, feet) * 1.6f);
        bool heard = c.noise > 0.05 && pathD < (float)c.noise * 18.0f;
        if (c.pings)
            for (const auto& [q, s] : *c.pings) heard = heard || hdist(a.pos, q) * 1.6f < s * 18.0f;
        if (heard) a.heard = feet, a.lostT = 0.0;
        else a.lostT += dt;
    }
    const vec3 rel = feet - a.home;
    const float dn = dot(flat(rel), n), dl = std::fabs(dot(flat(rel), vec3{-n.y, n.x, 0.0f}));
    const float dB = hdist(feet, a.home);  // waagrecht: Spieler - Bruchstelle
    const bool seen = a.sight.level > 0.3f;
    a.alert = 0.0f, a.perceive = 0.0f, a.growl = 0.0f, a.jaw = 0.0f, a.look = false;
    auto wallMode = [&](bool on) {  // Eingaben der Bruchstelle fuer die Animation (auch im Netz)
        a.up = on ? n : vec3{0, 0, 1};
        a.hang = on ? a.radius / 4.0f : 0.0f;
        a.shiver = on && a.burst > 0.5 ? 1.0f : 0.0f;
        if (!on) a.bait = 0.0f, a.reach = 0.0f;
    };
    auto heal = [&]() { a.burst = 0.0, a.bait = 0.0f, a.reach = 0.0f, a.knocks = 0; };
    auto steps = [&]() {  // leise, nackte Schritte hinter der Wand (nicht auf der Flucht: lautlos)
        if (a.cre.stepped()) out.push_back({Event::Step, Role::Duller, a.cre.stepPos(), 0.35f});
    };

    if (a.state == 10) {  // Zugriff: die Hand schliesst sich, zieht zur Wand
        wallMode(true);
        a.reach = a.stateT < 0.9 ? 1.0f : 0.55f;
        a.jaw = a.stateT > 0.08 ? 1.0f : 0.0f;
        a.look = true, a.lookAt = eye;
        if (a.stateT > 2.6) leave();
        return;
    }
    // gesehen (ausserhalb der Jagd): erstarren, dann lautlos fort. Die Bruchstelle sieht er von dort nicht - sie heilt.
    if (seen && a.state != 6 && a.state != 7) {
        setState(7);
        if (!breachSeen) heal();
        log::info("Praesenz: Duller gesehen - er flieht");
    }
    switch (a.state) {
        case 0: {  // lauern
            wallMode(false);
            steps();
            if (a.timer2 <= 0.0) {
                a.timer2 = rng_.uniform(0.45, 0.75);
                if (a.lostT < 6.0 && a.cool <= 0.0) {  // er weiss, wo der Spieler ist: eine Wand, hinter der er ihn hat?
                    const Ambush am = findAmbush(c, feet, eye, allowed);
                    if (am.ok) {
                        a.field.build(*c.world, ifloor(am.stand.x), ifloor(am.stand.y), kClear);
                        const int cost = a.field.at(ifloor(a.pos.x), ifloor(a.pos.y));
                        if (cost >= 0 && cost < 170) {
                            a.rest = am.stand, a.restN = am.n, a.home = am.breach, a.radius = am.D;
                            a.hasGoal = true, a.goal = am.stand;
                            setState(1);
                            break;
                        }
                    }
                }
            }
            // in der Naehe bleiben, ungesehen: ein Platz 5-14 m vom Spieler, den er nicht sieht
            if (a.hasGoal && hdist(a.pos, a.goal) > 0.6f && walkTo(a, a.field, 1.4, dt, c)) break;
            a.hasGoal = false;
            std::vector<std::pair<i64, i64>> cand;
            fld().ring(50, 140, cand);
            for (int tries = 0; tries < 24 && !cand.empty(); ++tries) {
                auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
                const world::Tile* t = c.world->tileIfLoaded(x, y);
                if (!figureWalkable(t, kClear)) continue;
                const vec3 g{(float)x + 0.5f, (float)y + 0.5f, t->floor};
                if (hdist(g, a.pos) > 16.0f || !allowed(g) || clearSight(*c.world, eye, g + vec3{0, 0, 1.2f})) continue;
                a.field.build(*c.world, x, y, kClear);
                if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) continue;
                a.goal = g, a.hasGoal = true;
                break;
            }
            if ((a.lostT > 70.0 || a.age > 360.0) && a.unseenT > 5.0) leave();
            break;
        }
        case 1: {  // zum Standplatz hinter der Wand
            wallMode(false);
            steps();
            if (a.timer2 <= 0.0) {  // steht der Spieler noch dort? sonst die Wand entlang folgen
                a.timer2 = 0.5;
                if (dn < -0.1f || dn > kNear + 0.3f || dl > 1.3f) {
                    const Ambush am = findAmbush(c, feet, eye, allowed);
                    bool ok = false;
                    if (am.ok) {
                        a.field.build(*c.world, ifloor(am.stand.x), ifloor(am.stand.y), kClear);
                        const int cost = a.field.at(ifloor(a.pos.x), ifloor(a.pos.y));
                        ok = cost >= 0 && cost < 170;
                        if (ok) a.rest = am.stand, a.restN = am.n, a.home = am.breach, a.radius = am.D;
                    }
                    if (!ok) {
                        setState(0), a.hasGoal = false;
                        break;
                    }
                }
            }
            if (hdist(a.pos, a.rest) < 0.3f) {  // angekommen: genau auf den Platz, zur Wand gedreht - schaben
                a.pos = a.rest, a.yaw = std::atan2(a.restN.y, a.restN.x);
                setState(2);
                a.timer = 0.0, a.knocks = 0, a.burst = 1.0;
                log::info("Praesenz: Duller schabt in der Wand bei ({:.1f}, {:.1f})", a.home.x, a.home.y);
                break;
            }
            if (!walkTo(a, a.field, hdist(a.pos, a.rest) > 4.0f ? 2.4 : 1.6, dt, c) && a.stateT > 1.0) setState(0), a.hasGoal = false;
            if (a.stateT > 25.0) setState(0), a.hasGoal = false;
            break;
        }
        case 2: {  // schaben: die Ankuendigung; der Putz woelbt sich
            a.pos = a.rest, a.yaw = std::atan2(n.y, n.x);
            wallMode(true);
            a.alert = 1.0f;
            a.look = true, a.lookAt = eye;  // er "sieht" durch die Wand zu ihm hin
            a.bait = (float)std::clamp((a.stateT - kBulgeAt) / (kScrape - kBulgeAt), 0.0, 1.0);
            if (a.timer <= 0.0) {
                a.timer = rng_.uniform(0.5, 0.72);
                out.push_back({Event::Scrape, Role::Duller, a.home, (float)(0.45 + 0.55 * std::min(1.0, a.stateT / kScrape))});
            }
            if ((a.knocks == 0 && a.stateT > kBulgeAt + 0.1) || (a.knocks == 1 && a.stateT > 2.4)) {
                ++a.knocks;
                out.push_back({Event::Bulge, Role::Duller, a.home, a.knocks == 1 ? 0.6f : 1.0f});
            }
            if (a.stateT >= kScrape) {
                const bool front = dn > -0.2f && dB < 4.0f && std::fabs(feet.z - a.rest.z) < 1.2f;
                if (front && c.almond) {  // Mandelwasser in der Hand: er weicht zurueck (die Seite: er meidet es)
                    log::info("Praesenz: Duller weicht vor dem Mandelwasser zurueck");
                    setState(5), a.cool = 60.0;
                    out.push_back({Event::Retract, Role::Duller, a.home, 0.5f});
                } else if (front && c.mode >= 2) {
                    setState(3);
                    out.push_back({Event::Burst, Role::Duller, a.home, 1.0f});
                } else {
                    setState(5), a.cool = 15.0;
                }
            }
            break;
        }
        case 3:    // Durchbruch: der Arm schiesst heraus
        case 4: {  // tasten
            a.pos = a.rest, a.yaw = std::atan2(n.y, n.x);
            wallMode(true);
            a.bait = 1.0f;
            a.reach = 1.0f;
            a.look = true, a.lookAt = eye;
            const bool inReach = dB <= kReach && dn > -0.1f && std::fabs(feet.z - a.rest.z) < 1.0f;
            if (((a.state == 3 && a.stateT >= 0.3) || (a.state == 4 && dB <= kReach - 0.1f)) && inReach && c.mode >= 2) {
                out.push_back({Event::Attack, Role::Duller, a.home + n * 0.6f, 1.0f});
                setState(10);
                log::info("Praesenz: Duller - toedlicher Zugriff durch die Wand");
                break;
            }
            if (a.state == 3 && a.stateT >= 0.3) setState(4);
            else if (a.state == 4 && a.stateT > 1.6) {
                setState(5), a.cool = rng_.uniform(22.0, 36.0);
                out.push_back({Event::Retract, Role::Duller, a.home, 1.0f});
            }
            break;
        }
        case 5: {  // der Arm gleitet zurueck; heilt die Wand ungesehen, geht er
            a.pos = a.rest, a.yaw = std::atan2(n.y, n.x);
            wallMode(true);
            a.reach = 0.0f;
            if (a.stateT > 1.2 && (a.recog > 1.5 || a.burst < 0.5)) {
                heal();
                wallMode(false);
                setState(0), a.hasGoal = false, a.timer2 = 1.0;
            }
            break;
        }
        case 7: {  // bemerkt: erstarrt, der Kopf schnellt zum Spieler
            wallMode(a.burst > 0.5 && a.radius > 0.0f);
            a.reach = 0.0f;
            a.perceive = 1.0f, a.perceiveAt = eye;
            if (a.stateT > 0.35) {
                heal();
                wallMode(false);
                setState(6);
                pickGoal(c, a, 260, 420, true);
            }
            break;
        }
        case 6: {  // Flucht: sehr schnell, lautlos
            wallMode(false);
            a.growl = 1.0f;
            if (!a.hasGoal || !walkTo(a, a.field, 6.0, dt, c) || hdist(a.pos, a.goal) < 0.8f) pickGoal(c, a, 260, 420, true);
            if ((a.unseenT > 1.5 && hdist(a.pos, feet) > 12.0f) || (a.stateT > 12.0 && a.unseenT > 1.0)) leave();
            break;
        }
        default: break;
    }
}

}  // namespace lim::game
