// V13.3 Level 2/3: Hound-Rudel (KREATUR_Hound.md, Abschnitt "Rudel"; Wikidot Entity 8 Rev. 56, Level 2 Rev. 93, Level 3 Rev. 18).
// Level-JSON presence.hound_pack: mode "l2" (Variante 1) oder "l3" (Variante 2), group, every. Rolle Hound - das Einzelverhalten
// (thinkHound in presence.cpp, Level 1, Level 5) bleibt, wie es ist; Level 2 und 3 schalten den einzelnen Hound ab ("hound": false).
//
// Die Seiten: Level 2 - Rudel ("murder") ziehen normalerweise langsam umher und werden, gestoert, bedroht oder jagend, schlagartig
// schnell. Level 3 - Hounds verstecken sich perfekt und springen von hinten an; in Gruppen verstaendigen sie sich beim Verfolgen
// durch Bellen, das schneller und aufgeregter wird, wenn das Opfer falsch abbiegt (in eine Sackgasse oder auf eine zweite Gruppe
// zu); sie toeten so langsam und schmerzhaft wie moeglich (Nackenbiss: gelaehmt, aber lebendig; Glieder: keine Flucht;
// Stimmbaender: kein Hilferuf). Entity 8: direkter Blickkontakt schuechtert ein - nicht lange, aber genug, um sich zu orientieren
// und zu fliehen; nicht sofort wegrennen. Was sie im Einzelnen tun, ist Auslegung (im Briefing so gekennzeichnet):
//
// Phasen des Rudels: 0 zieht umher (Level 2) / lauert (Level 3)  1 Alarm: einer bellt, die anderen antworten und kommen - Level 2
// schlagartig im Sprint, Level 3 ungesehen schnell, gesehen vorsichtig, auf Plaetze rund um das Opfer (einkreisen; in Gaengen:
// vor und hinter ihm)  2 kreist: geduckt, knurrend auf Abstand, immer nur einer springt (Marke) - erst ein scharfes Klaeffen
// und tiefes Ducken (Ankuendigung 0,8 s / 1,0 s), dann der Sprung. Wer ihn dabei ansieht, bricht ihn ab (er weicht winselnd
// zurueck). Level 2: Biss (Schaden); Level 3: Quaelen in Stufen - Nacken (Laehmung), Bein (Hinken), Kehle (kein Ruf), danach
// Bisse; nie ein Sofort-Tod.  3 zieht ab: das Rudel gibt auf (Interesse aufgebraucht, Opfer ausser Sicht).
// Interesse: sinkt mit der Zeit (Level 2: sie sind Wanderer), durch Anstarren und ruhiges Zurueckweichen; steigt, wenn das Opfer
// rennt oder blutet. Level 3: der Rufer haelt Abstand und bellt im Takt; laeuft das Opfer auf eine Sackgasse oder auf einen
// anderen Hound zu, wird das Bellen schneller (der Spieler kann es lesen).
// Klaenge ueber Klangnummern (gameplay/kin_l3.hpp, KC_HP_*), Synthese in audio/synth_hound_pack.cpp.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"
#include "gameplay/kin_l3.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
struct PackHound {  // je Hound im Rudel (Agent::ext)
    int id = 0, slot = 0;
    bool caller = false, struck = false, ambush = false;
    double fieldT = 0.0, chatT = 4.0, stare = 0.0, flinchT = 0.0, leapT = 0.0, holdT = 0.0, breathT = 3.0, answerT = -1.0, growlT = 1.0;
    vec3 lurk, slotAt;
    float lurkYaw = 0.0f;
};

struct Pack {
    bool active = false, gaveUp = false, encircled = false;
    int mode = 1;   // Variante der Hounds: 1 Level 2, 2 Level 3
    int phase = 0;  // 0 zieht umher / lauert, 1 Alarm, 2 kreist, 3 zieht ab
    double phaseT = 0.0, nextSpawn = 90.0, quick = 1.0, req = -1.0;
    double tokenCool = 0.0, callT = 0.0, lostT = 0.0, trapT = 0.0, rushT = 0.0;
    int token = 0, leadId = 0, nextId = 1;
    int bites = 0, barks = 0, answers = 0, fast = 0, lunges = 0, aborted = 0, torment = 0;
    float interest = 1.0f, barkEvery = 2.8f, trap = 0.0f, rot = 0.0f, rotDir = 1.0f, baseAng = 0.0f;
    vec3 caller, prey;
    std::array<double, 4> mute{}, limp{};
    std::array<int, 4> stage{};
};

Pack& packOf(std::shared_ptr<void>& p) {
    if (!p) p = std::make_shared<Pack>();
    return *static_cast<Pack*>(p.get());
}
template <class A>
PackHound& houndOf(A& a) {
    if (!a.ext) a.ext = std::make_shared<PackHound>();
    return *static_cast<PackHound*>(a.ext.get());
}
float cue(int k) { return kKinCue + (float)k; }

// Laeuft der Weg vor 'from' in Richtung dir in eine Sackgasse? Breitensuche ueber begehbare Kacheln (nur nach vorn, 10 m):
// 1 - nichts kommt weiter als 6 m, 0,5 - hoechstens 8 m, 0 - frei
float deadEnd(world::World& w, vec3 from, vec3 dir) {
    const i64 x0 = ifloor(from.x), y0 = ifloor(from.y);
    std::vector<std::pair<i64, i64>> q{{x0, y0}};
    std::vector<std::pair<i64, i64>> seen{{x0, y0}};
    float reach = 0.0f;
    for (size_t i = 0; i < q.size() && q.size() < 420; ++i) {
        const auto [x, y] = q[i];
        const world::Tile* t0 = w.tileIfLoaded(x, y);
        if (!t0) continue;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; ++k) {
            const i64 nx = x + dx[k], ny = y + dy[k];
            const vec3 c{(float)nx + 0.5f, (float)ny + 0.5f, 0.0f};
            const vec3 rel{c.x - from.x, c.y - from.y, 0.0f};
            const float len = length(rel);
            if (len > 10.0f || dot(rel, dir) < -0.6f) continue;
            if (std::find(seen.begin(), seen.end(), std::pair<i64, i64>{nx, ny}) != seen.end()) continue;
            seen.push_back({nx, ny});
            const world::Tile* t = w.tileIfLoaded(nx, ny);
            if (!t || !figureWalkable(t, 1.1f) || std::fabs(t->floor - t0->floor) > 0.6f) continue;
            reach = std::max(reach, dot(rel, dir));
            q.push_back({nx, ny});
        }
    }
    return reach < 6.0f ? 1.0f : (reach < 8.0f ? 0.5f : 0.0f);
}
}  // namespace

// --- Zustand fuer Tests, Skripte ---------------------------------------------------------------------------------------------
Presence::PackInfo Presence::packInfo() const {
    PackInfo I;
    if (!pack_) return I;
    const Pack& P = *static_cast<const Pack*>(pack_.get());
    I.active = P.active, I.encircled = P.encircled, I.gaveUp = P.gaveUp;
    I.phase = P.phase, I.bites = P.bites, I.barks = P.barks, I.answers = P.answers, I.fast = P.fast, I.lunges = P.lunges;
    I.aborted = P.aborted, I.torment = P.torment, I.barkEvery = P.barkEvery, I.interest = P.interest, I.trap = P.trap, I.caller = P.caller;
    for (const auto& a : agents_) {
        if (a->role != Role::Hound || a->variant <= 0 || a->showcase) continue;
        ++I.hounds;
        if (a->variant == 2) ++I.l3;
    }
    return I;
}

bool Presence::packSet(const std::string& key, double v) {
    Pack& P = packOf(pack_);
    if (key == "spawn") P.req = std::max(0.0, v);
    else if (key == "phase") P.phase = (int)v, P.phaseT = 0.0;
    else if (key == "quick") P.quick = std::max(0.05, v);
    else if (key == "interest") P.interest = (float)v;
    else if (key == "bark") P.callT = 0.0, P.barkEvery = (float)std::max(0.3, v);
    else if (key == "lunge") P.tokenCool = 0.0, P.token = 0;
    else if (key == "next") P.nextSpawn = v;
    else return false;
    return true;
}

float Presence::houndMute(int target) const {
    if (!pack_ || target < 0 || target >= 4) return 0.0f;
    return (float)static_cast<const Pack*>(pack_.get())->mute[(size_t)target];
}

float Presence::packSlow(int target) const {
    if (!pack_ || target < 0 || target >= 4) return 1.0f;
    const double t = static_cast<const Pack*>(pack_.get())->limp[(size_t)target];
    return t <= 0.0 ? 1.0f : (t >= 8.0 ? 0.62f : (float)(0.62 + 0.38 * (1.0 - t / 8.0)));  // die letzten 8 s heilt es
}

// Groesse je Variante, die Augen des Lauerers leuchten nicht (auch Puppen und Vorfuehrungen)
void Presence::prepPackHound(Agent& a) {
    a.cre.setScale(a.variant == 2 ? 1.06f : 0.95f);
    a.rig.setGlow(a.hang > 0.5f ? 0.1f : 1.0f);
}

// --- Auftritt ------------------------------------------------------------------------------------------------------------------
bool Presence::spawnHoundPack(const Context& c, std::vector<Event>& out, double dist) {
    if (!c.houndPack || !fld().valid()) return false;
    Pack& P = packOf(pack_);
    const int mode = c.houndPack->mode == "l3" ? 2 : 1;
    const int n = (int)rng_.randint(c.houndPack->groupMin, c.houndPack->groupMax);
    const int lo = dist > 0.0 ? (int)(dist * 8.5) : 250, hi = dist > 0.0 ? (int)(dist * 12.5) : 400;
    std::vector<vec3> spots;
    const vec3 eye = c.view.eye;
    auto hiddenAt = [&](vec3 p) {
        for (float h : {1.0f, 0.4f})
            if (dot(nrmF(p + vec3{0, 0, h} - eye, {1, 0, 0}), c.view.look) > 0.3f && clearSight(*c.world, eye, p + vec3{0, 0, h})) return false;
        return true;
    };
    if (mode == 1) {  // Level 2: beisammen
        vec3 p;
        if (!hiddenSpawn(c, lo, hi, true, p)) return false;
        spots.push_back(p);
        for (int k = 1; k < n; ++k)
            for (int tries = 0; tries < 40; ++tries) {
                const float ang = (float)rng_.uniform(-3.14, 3.14), r = (float)rng_.uniform(1.0, 3.2);
                const vec3 q{p.x + std::cos(ang) * r, p.y + std::sin(ang) * r, p.z};
                const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
                if (!t || !figureWalkable(t, 1.1f) || std::fabs(t->floor - p.z) > 0.4f || fld().at(ifloor(q.x), ifloor(q.y)) < 0) continue;
                const vec3 qq{q.x, q.y, t->floor};
                bool near = false;
                for (const vec3& s : spots) near = near || hdist(s, qq) < 0.8f;
                if (near || !hiddenAt(qq)) continue;
                spots.push_back(qq);
                break;
            }
    } else {  // Level 3: jeder allein, in einer Nische (zwei Seiten zu), weit auseinander
        for (int k = 0; k < n; ++k) {
            vec3 q;
            auto ok = [&](vec3 p) {
                for (const vec3& s : spots)
                    if (hdist(s, p) < 9.0f) return false;
                int walls = 0;
                static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x) + dx[d], ifloor(p.y) + dy[d]);
                    walls += !t || !figureWalkable(t, 1.1f);
                }
                return walls >= 2;
            };
            if (hiddenSpawn(c, lo, hi, k == 0, q, ok) || hiddenSpawn(c, lo, hi, false, q, [&](vec3 p) {
                    for (const vec3& s : spots)
                        if (hdist(s, p) < 7.0f) return false;
                    return true;
                }))
                spots.push_back(q);
        }
    }
    if ((int)spots.size() < 2) return false;
    const auto keep = std::make_tuple(P.mute, P.limp, P.req, P.quick);
    P = Pack{};
    std::tie(P.mute, P.limp, P.req, P.quick) = keep;
    P.active = true, P.mode = mode, P.phase = 0;
    P.rotDir = rng_.uniform(0.0, 1.0) < 0.5 ? -1.0f : 1.0f;
    for (size_t k = 0; k < spots.size(); ++k) {
        auto a = std::make_unique<Agent>();
        a->role = Role::Hound;
        a->ent = Entity::Hound;
        a->variant = mode;
        const vec3 p = spots[k];
        float yaw = std::atan2(eye.y - p.y, eye.x - p.x) + (float)rng_.uniform(-1.2, 1.2);
        if (mode == 2) {  // der Lauerer sieht aus der Nische heraus (zur offenen Seite)
            vec3 open{};
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d)
                if (const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x) + dx[d], ifloor(p.y) + dy[d]); t && figureWalkable(t, 1.1f))
                    open = open + vec3{(float)dx[d], (float)dy[d], 0.0f};
            if (length(open) > 0.1f) yaw = std::atan2(open.y, open.x);
        }
        a->initBody(p, yaw, (unsigned)rng_.randint(1, 1 << 30));
        a->state = 0;
        a->stareLimit = (float)rng_.uniform(6.0, 9.0);
        a->timer = rng_.uniform(2.0, 6.0);
        a->hang = mode == 2 ? 1.0f : 0.0f;
        PackHound& h = houndOf(*a);
        h.id = P.nextId++;
        h.slot = (int)k;
        h.lurk = p, h.lurkYaw = yaw;
        h.chatT = rng_.uniform(3.0, 9.0);
        prepPackHound(*a);
        if (mode == 1 && k == 0) pickGoal(c, *a, 90, 260, false);
        out.push_back({Event::Appear, Role::Hound, p});
        agents_.push_back(std::move(a));
    }
    log::info("Praesenz: Hound-Rudel ({}) aus {} Hounds, {:.0f} m", mode == 2 ? "Level 3, jeder lauert allein" : "Level 2, zieht umher",
              spots.size(), hdist(spots[0], eye));
    return true;
}

void Presence::regieHoundPack(double dt, const Context& c, std::vector<Event>& out) {
    Pack& P = packOf(pack_);
    for (size_t k = 0; k < 4; ++k) P.mute[k] = std::max(0.0, P.mute[k] - dt), P.limp[k] = std::max(0.0, P.limp[k] - dt);
    if (!c.houndPack || c.mode <= 0 || c.suspended) return;
    int n = 0, lead = 1 << 30;
    for (const auto& a : agents_)
        if (a->role == Role::Hound && a->variant > 0 && !a->showcase && a->state != 9) {
            ++n;
            if (a->ext) lead = std::min(lead, static_cast<const PackHound*>(a->ext.get())->id);
        }
    P.leadId = lead;
    if (n == 0 && P.active) {  // das Rudel ist fort
        P.active = false;
        P.nextSpawn = rng_.uniform(c.houndPack->every * 0.7, c.houndPack->every * 1.3) * c.stalkerPause;
        log::info("Praesenz: das Hound-Rudel ist fort ({} Bisse, {} Rufe)", P.bites, P.barks);
    }
    if (P.req >= 0.0) {
        const double d = P.req;
        P.req = -1.0;
        if (!P.active && !spawnHoundPack(c, out, d)) log::info("Praesenz: erzwungenes Hound-Rudel nicht moeglich");
        return;
    }
    if (P.active) return;
    // nicht mitten in eine andere Jagd hinein; Dunkel und Lampe locken (wie beim einzelnen Hound)
    const bool busy = has(Role::Smiler) || wranglerActive();
    const world::Tile* pt = c.world->tileIfLoaded(ifloor(c.player->x), ifloor(c.player->y));
    const bool dark = darkTile(c, pt, (float)c.player->x, (float)c.player->y);
    P.nextSpawn -= dt * (busy ? 0.25 : 1.0) * (dark ? (c.view.flashOn ? 1.6 : 1.25) : 0.8);
    if (P.nextSpawn <= 0.0) P.nextSpawn = spawnHoundPack(c, out, 0.0) ? 1e9 : 8.0;
}

// --- Denken je Hound -----------------------------------------------------------------------------------------------------------
void Presence::thinkPackHound(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    Pack& P = packOf(pack_);
    PackHound& h = houndOf(a);
    const bool l3 = a.variant == 2;
    const vec3 eye = c.view.eye;
    const vec3 ppos{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    const bool playerLit = c.view.flashOn || litPoint(c.view, *c.world, eye - vec3{0, 0, 0.6f});
    const float seeRange = l3 ? (c.view.flashOn ? 22.0f : (playerLit ? 16.0f : 9.0f)) : (c.view.flashOn ? 30.0f : (playerLit ? 20.0f : 8.0f));
    const bool sees = los && d < seeRange && (facing > 0.15f || d < 5.0f);
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    const bool hears = (c.player->sprinting && pathD < (l3 ? 30.0f : 35.0f)) || (pathD < (l3 ? 12.0f : 8.0f) && c.player->gait > 0.3f && !c.player->sneaking);
    a.contact = los && a.sight.inView && a.sight.angle < 0.3f && (a.sight.level > 0.2f || d < 12.0f);
    a.stateT += dt;
    a.cool -= dt;
    a.growl = 0.0f, a.reach = 0.0f, a.perceive = 0.0f, a.strike = 0.0f;
    a.talk = std::max(0.0f, a.talk - (float)dt * 4.0f);
    if (a.state < 3) a.jaw = 0.0f;
    lightReact(a, c, head, 18.0f, dt, 1.5);
    h.flinchT = std::max(0.0, h.flinchT - dt);
    auto setState = [&](int s) {
        a.state = s;
        a.stateT = 0.0;
    };
    auto bark = [&](int k, vec3 at) {
        out.push_back({Event::Bark, Role::Hound, at, cue(k)});
        a.talk = 1.0f;
    };
    auto faceP = [&](double rate) { a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * rate); };
    // das ganze Rudel alarmieren: dieser ruft, die anderen antworten nacheinander (aus ihrer Lage - man hoert, wo sie sind)
    auto alarm = [&](bool callNow) {
        if (P.phase != 0) return;
        P.phase = 1, P.phaseT = 0.0, P.caller = a.pos, P.prey = eye;
        P.baseAng = std::atan2(a.pos.y - eye.y, a.pos.x - eye.x);
        P.callT = l3 ? 1.6 : 99.0;
        h.caller = true;
        if (callNow) bark(KC_HP_CALL, head), ++P.barks;
        double t = rng_.uniform(0.5, 1.0);
        for (auto& o : agents_)
            if (o->role == Role::Hound && o->variant > 0 && o.get() != &a && o->ext) {
                houndOf(*o).answerT = t;
                t += rng_.uniform(0.35, 0.8);
            }
        if (!l3) P.rushT = 0.4;  // Level 2: sie stuermen gleich los (Pfoten vieler Hounds)
        log::info("Praesenz: Hound-Rudel alarmiert ({})", l3 ? "Ruf" : "gestoert");
    };

    // --- Rudel (einmal je Bild, beim ersten Hound) ---------------------------------------------------------------------------
    if (h.id == P.leadId) {
        P.phaseT += dt;
        P.tokenCool -= dt * P.quick;
        P.prey = eye;
        // sieht ihn noch einer?
        bool anySee = false;
        int near = 0;
        std::vector<float> angs;
        for (const auto& o : agents_)
            if (o->role == Role::Hound && o->variant > 0 && !o->showcase && o->state != 9) {
                const float od = hdist(o->pos, eye);
                anySee = anySee || (od < 26.0f && clearSight(*c.world, o->cre.headPos() + vec3{0, 0, 0.05f}, eye));
                if (od < (l3 ? 9.0f : 7.5f)) angs.push_back(std::atan2(o->pos.y - eye.y, o->pos.x - eye.x)), ++near;
            }
        // umzingelt: zwei auf verschiedenen Seiten (mehr als 100 Grad auseinander)
        bool around = false;
        for (size_t i = 0; i < angs.size(); ++i)
            for (size_t j = i + 1; j < angs.size(); ++j) around = around || std::fabs(wrapA(angs[i] - angs[j])) > 1.75f;
        P.encircled = around;
        P.lostT = anySee ? 0.0 : P.lostT + dt;
        if (P.rushT > 0.0) {
            P.rushT -= dt;
            if (P.rushT <= 0.0) out.push_back({Event::Growl, Role::Hound, a.pos + vec3{0, 0, 0.4f}, cue(KC_HP_RUSH)});
        }
        if (P.phase == 1 && (around || (near >= 2 && P.phaseT > 4.0) || P.phaseT > (l3 ? 40.0 : 9.0))) {
            P.phase = 2, P.phaseT = 0.0, P.tokenCool = l3 ? 3.0 : 2.5;
            log::info("Praesenz: Hound-Rudel kreist ein ({} nah)", near);
        }
        if (P.phase >= 1 && P.phase <= 2) {
            // Interesse (Auslegung): Level 2 sind Wanderer - es verfliegt; Anstarren und ruhiges Zurueckweichen kosten,
            // Rennen und Blut reizen. Level 3: nur ausser Sicht sinkt es
            const vec3 v{(float)c.player->vx, (float)c.player->vy, 0.0f};
            const vec3 fromPack = nrmF(flat(eye - P.caller), toP);
            bool watched = false;
            for (const auto& o : agents_)
                if (o->role == Role::Hound && o->variant > 0 && o->contact) watched = true;
            const bool backing = length(v) > 0.4f && !c.player->sprinting && dot(nrmF(v, {}), fromPack) > 0.3f;
            if (c.player->sprinting) P.interest += (float)dt * (l3 ? 0.1f : 0.2f);
            else if (!l3) P.interest -= (float)dt * (0.022f + (watched && backing ? 0.06f : 0.0f));
            else P.interest -= (float)dt * ((anySee ? 0.0f : 0.05f) + (watched && backing ? 0.025f : 0.0f));
            P.interest = std::min(P.interest, 1.4f);
            P.rot += (float)dt * 0.13f * P.rotDir;
            if (P.interest <= 0.0f || P.lostT > (l3 ? 14.0 : 9.0)) {
                P.phase = 3, P.phaseT = 0.0, P.gaveUp = true;
                log::info("Praesenz: das Hound-Rudel gibt auf ({})", P.interest <= 0.0f ? "kein Interesse mehr" : "aus den Augen");
                for (auto& o : agents_)
                    if (o->role == Role::Hound && o->variant > 0 && o->state != 9) {
                        o->state = 4, o->stateT = 0.0, o->timer2 = 0.0;
                        o->hasGoal = false;
                    }
                out.push_back({Event::Whine, Role::Hound, a.cre.headPos()});
            }
        }
        // Level 3: der Rufer bellt im Takt - schneller, wenn das Opfer in eine Sackgasse oder auf einen anderen Hound zulaeuft
        if (l3 && (P.phase == 1 || P.phase == 2)) {
            P.trapT -= dt;
            if (P.trapT <= 0.0) {
                P.trapT = 0.5;
                const vec3 v{(float)c.player->vx, (float)c.player->vy, 0.0f};
                float trap = 0.0f;
                if (length(v) > 0.3f) {
                    const vec3 dir = nrmF(v, {1, 0, 0});
                    trap = deadEnd(*c.world, ppos, dir) * 0.85f;
                    for (const auto& o : agents_) {
                        if (o->role != Role::Hound || o->variant != 2 || !o->ext || static_cast<const PackHound*>(o->ext.get())->caller) continue;
                        const vec3 to = flat(o->pos - eye);
                        if (length(to) < 22.0f && dot(nrmF(to, {}), dir) > 0.75f) trap = 1.0f;
                    }
                }
                P.trap = P.trap + (trap - P.trap) * 0.6f;
            }
            P.barkEvery = 2.8f - 2.05f * P.trap;
            P.callT -= dt;
        }
        // die Marke: immer nur einer springt (Level 2 rennt man, springt der naechste sofort)
        if (P.phase == 2 && P.token == 0 && (P.tokenCool <= 0.0 || (!l3 && c.player->sprinting && P.tokenCool < 1.5))) {
            Agent* best = nullptr;
            float bestScore = -1e9f;
            for (auto& o : agents_) {
                if (o->role != Role::Hound || o->variant <= 0 || o->state != 2 || !o->ext) continue;
                const PackHound& oh = *static_cast<const PackHound*>(o->ext.get());
                const float od = hdist(o->pos, eye);
                if (oh.flinchT > 0.0 || od > 9.0f || o->contact) continue;
                // nur wer das Opfer frei vor sich hat (der Sprung geht ueber freie Flaeche - und man kann ihn sehen, wenn man sich umdreht)
                if (!clearSight(*c.world, o->cre.headPos() + vec3{0, 0, 0.05f}, eye)) continue;
                const vec3 dd = nrmF(o->pos - eye, {1, 0, 0});
                const float behind = -dot(dd, c.view.look);  // hinter dem Opfer bevorzugt
                const float score = behind * 2.0f - od * 0.25f + (o->sight.inView ? -1.0f : 0.5f) + (float)rng_.uniform(0.0, 0.6);
                if (score > bestScore) bestScore = score, best = o.get();
            }
            if (best) {
                PackHound& bh = houndOf(*best);
                P.token = bh.id;
                best->state = 3, best->stateT = 0.0;
                best->timer = l3 ? 1.0 : (c.player->sprinting ? 0.55 : 0.8);  // Ankuendigung
                bh.leapT = 0.0, bh.struck = false, bh.holdT = 0.0, bh.stare = 0.0;
                out.push_back({Event::Bark, Role::Hound, best->cre.headPos(), cue(KC_HP_LUNGE)});
                best->talk = 1.0f;
                ++P.lunges;
            }
        }
    }
    // Antwort auf den Ruf (aus der eigenen Lage)
    if (h.answerT >= 0.0) {
        h.answerT -= dt;
        if (h.answerT < 0.0) {
            bark(KC_HP_ANSWER, head);
            ++P.answers;
            if (a.state == 0) setState(1);
        }
    }

    switch (a.state) {
        case 0: {  // Level 2: zieht umher (langsam, schnueffelnd); Level 3: lauert in der Nische
            if (l3) {
                a.hang = 1.0f, a.crouch = 1.0f;
                a.look = hears || sees;
                a.lookAt = eye;
                a.pos = h.lurk;
                a.yaw += wrapA(h.lurkYaw - a.yaw) * (float)std::min(1.0, dt * 2.0);
                h.breathT -= dt;
                if (h.breathT <= 0.0) {  // kaum hoerbarer, langsamer Atem - nur ganz nah
                    h.breathT = rng_.uniform(5.0, 8.0);
                    if (d < 8.0f) out.push_back({Event::Breath, Role::Hound, head, cue(KC_HP_LURK)});
                }
                if ((sees || hears || a.light > 0.3f) && P.phase == 0) {
                    // Hinterhalt (die Seite): nah und der Ruecken zugekehrt - er springt an (mit Ankuendigung); sonst der Ruf
                    const bool back = dot(nrmF(flat(a.pos - eye), {1, 0, 0}), nrmF(flat(c.view.look), {1, 0, 0})) < -0.2f;
                    if (d < 4.5f && back && c.mode >= 2) {
                        alarm(false);
                        h.ambush = true;
                        a.hang = 0.0f;
                        setState(3);
                        a.timer = 0.75;
                        P.token = h.id;
                        out.push_back({Event::Growl, Role::Hound, head, cue(KC_HP_LUNGE)});
                        a.talk = 1.0f;
                        ++P.lunges;
                    } else {
                        alarm(true);
                        a.hang = 0.0f;
                        setState(2);  // der Rufer pirscht auf Abstand
                    }
                }
                break;
            }
            a.hang = 0.0f;
            a.crouch = 0.0f;
            a.alert = 0.3f;
            a.look = false;
            h.chatT -= dt;
            if (h.chatT <= 0.0) {  // Laute im Rudel - man hoert es, bevor man es sieht
                h.chatT = rng_.uniform(6.0, 13.0);
                out.push_back({Event::Growl, Role::Hound, head, cue(KC_HP_YIP)});
            }
            if (sees || hears || a.light > 0.3f) {
                alarm(true);
                setState(1);
                break;
            }
            if (h.slot == 0 || P.leadId == h.id) {  // der vorderste fuehrt
                a.timer -= dt;
                if (a.timer <= 0.0) a.timer = rng_.uniform(6.0, 12.0), a.timer2 = rng_.uniform(1.5, 3.0);
                if (a.timer2 > 0.0) {
                    a.timer2 -= dt;
                    a.perceive = 1.0f;
                    a.perceiveAt = a.pos + dirYaw(a.yaw + 0.6f * (float)std::sin(a.stateT)) * 3.0f + vec3{0, 0, 0.6f};
                    break;
                }
                if (!a.hasGoal || !walkTo(a, a.field, 0.62, dt, c) || hdist(a.pos, a.goal) < 1.2f)
                    if (!pickGoal(c, a, 90, 260, false)) a.timer2 = 1.5;
            } else {  // die anderen folgen dem Fuehrer (versetzt)
                const Agent* lead = nullptr;
                for (const auto& o : agents_)
                    if (o->role == Role::Hound && o->variant > 0 && o->ext && static_cast<const PackHound*>(o->ext.get())->id == P.leadId) lead = o.get();
                if (!lead) break;
                const vec3 back = dirYaw(lead->yaw + 3.14159f + 0.7f * (h.slot % 2 ? 1.0f : -1.0f)) * (1.2f + 0.6f * (float)h.slot);
                h.slotAt = lead->pos + back;
                h.fieldT -= dt;
                if (h.fieldT <= 0.0) h.fieldT = 1.0, a.field.build(*c.world, ifloor(h.slotAt.x), ifloor(h.slotAt.y), 1.1f);
                const float far = hdist(a.pos, h.slotAt);
                if (far > 0.9f) walkTo(a, a.field, far > 4.0f ? 1.5 : 0.7, dt, c);
                else a.perceive = 0.6f, a.perceiveAt = lead->cre.headPos();
            }
            break;
        }
        case 1:    // Alarm: zum Platz rund um das Opfer
        case 2: {  // kreist: geduckt, knurrend auf Abstand; der Rufer (Level 3) pirscht und bellt
            a.hang = 0.0f;
            a.look = true;
            a.lookAt = eye;
            a.alert = 1.0f;
            a.crouch = a.state == 2 ? 1.0f : 0.5f;
            int n = 0, k = 0;
            for (const auto& o : agents_)
                if (o->role == Role::Hound && o->variant > 0 && o->state != 9 && o->ext) {
                    if (o.get() == &a) k = n;
                    ++n;
                }
            const float R = h.caller && l3 ? 8.0f : (l3 ? 6.0f : 5.0f) + (h.flinchT > 0.0 ? 2.5f : 0.0f);
            // Platz auf dem Ring (in Gaengen: der naechste freie, erreichbare Punkt in dieser Richtung)
            const float ang = P.baseAng + P.rot + 6.2832f * (float)k / (float)std::max(1, n);
            vec3 slot = a.pos;
            bool found = false;
            for (float dr : {0.0f, 0.35f, -0.35f, 0.7f, -0.7f, 1.1f, -1.1f})
                for (float rr : {R, R * 0.7f, R * 0.45f}) {
                    if (found) break;
                    const vec3 q{eye.x + std::cos(ang + dr) * rr, eye.y + std::sin(ang + dr) * rr, ppos.z};
                    const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
                    if (!t || !figureWalkable(t, 1.1f) || fld().at(ifloor(q.x), ifloor(q.y)) < 0) continue;
                    slot = {q.x, q.y, t->floor}, found = true;
                }
            if (found && (hdist(slot, h.slotAt) > 1.5f || h.fieldT <= 0.0)) {
                h.slotAt = slot;
                h.fieldT = 0.6;
                a.field.build(*c.world, ifloor(slot.x), ifloor(slot.y), 1.1f);
            }
            h.fieldT -= dt;
            const float far = hdist(a.pos, h.slotAt);
            double speed;
            if (a.state == 1) speed = l3 ? (a.sight.level > 0.25f ? 1.6 : 3.4) : 4.6;  // Level 2: schlagartig schnell
            else speed = far > 2.5f ? 2.2 : 1.1;
            if (found && far > 0.7f && (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) >= 0)) walkTo(a, a.field, speed, dt, c);
            else if (!found && d > R) walkTo(a, fld(), speed, dt, c);
            if (far < 1.2f || d < R + 1.0f) {
                faceP(4.0);
                if (a.state == 1 && P.phase >= 2) setState(2);
            }
            if (a.state == 1 && P.phase == 2 && a.stateT > 6.0) setState(2);
            // Knurren, Klaeffen
            a.growl = d < 14.0f ? 1.0f : 0.4f;
            h.growlT -= dt;
            if (h.growlT <= 0.0 && d < 16.0f) {
                h.growlT = rng_.uniform(2.6, 4.6);
                if (rng_.uniform(0.0, 1.0) < 0.35) out.push_back({Event::Growl, Role::Hound, head, cue(KC_HP_YIP)});
                else out.push_back({Event::Growl, Role::Hound, head, std::clamp(1.0f - d / 16.0f, 0.3f, 1.0f)});
            }
            // Level 3: der Rufer bellt im Takt (schneller, wenn das Opfer in die Falle laeuft)
            if (l3 && h.caller && P.callT <= 0.0 && P.phase >= 1 && P.phase <= 2) {
                P.callT = P.barkEvery * rng_.uniform(0.85, 1.15);
                const bool fast = P.trap > 0.5f;
                bark(fast ? KC_HP_FAST : KC_HP_CALL, head);
                ++P.barks;
                if (fast) ++P.fast;
            }
            // Blickkontakt: er erstarrt, weicht; haelt man ihn lange, bricht er winselnd aus dem Kreis (kurz)
            if (a.contact) {
                h.stare += dt;
                if (d < 4.5f) {
                    const vec3 back = a.pos - toP * (float)(0.6 * dt);
                    if (figureWalkable(c.world->tileIfLoaded(ifloor(back.x), ifloor(back.y)), 1.1f)) a.pos.x = back.x, a.pos.y = back.y;
                }
                faceP(5.0);
                if (h.stare > (l3 ? 1.3 : 1.7) && h.flinchT <= 0.0) {
                    h.flinchT = l3 ? 3.0 : 4.0;
                    h.stare = 0.0;
                    P.interest -= l3 ? 0.06f : 0.1f;
                    out.push_back({Event::Whine, Role::Hound, head, cue(KC_HP_FLINCH)});
                }
            } else {
                h.stare = std::max(0.0, h.stare - dt * 0.5);
            }
            break;
        }
        case 3: {  // Sprung: erst die Ankuendigung (tief geduckt, Maul auf), dann der Satz
            a.hang = 0.0f;
            a.look = true;
            a.lookAt = eye - vec3{0, 0, 0.4f};
            if (a.timer > 0.0) {
                a.timer -= dt;
                a.crouch = 1.0f;
                a.growl = 1.0f;
                a.jaw = 0.55f;
                faceP(8.0);
                if (a.contact) h.stare += dt;
                if (h.stare > 0.25) {  // angesehen: er bricht ab
                    out.push_back({Event::Whine, Role::Hound, head, cue(KC_HP_FLINCH)});
                    ++P.aborted;
                    P.token = 0, P.tokenCool = l3 ? 2.5 : 2.0;
                    h.flinchT = 3.0, h.stare = 0.0, h.ambush = false;
                    setState(2);
                }
                break;
            }
            h.leapT += dt;
            if (h.holdT > 0.0) {  // Level 3: festgebissen - er schuettelt
                h.holdT -= dt;
                a.strike = 1.0f;
                a.jaw = 0.25f;
                a.pos = ppos - toP * 0.85f;
                a.pos.z = ppos.z;
                faceP(10.0);
                if (h.holdT <= 0.0) {
                    P.token = 0, P.tokenCool = rng_.uniform(5.0, 8.0);
                    h.flinchT = 2.5, h.ambush = false;
                    setState(2);
                }
                break;
            }
            a.crouch = 0.4f;
            a.jaw = d < 3.0f ? 1.0f - d / 3.0f : 0.3f;
            a.reach = d < 2.2f ? 1.0f - d / 2.2f : 0.0f;
            if (d < 1.2f && std::fabs(a.pos.z - ppos.z) < 1.0f && !h.struck && c.mode >= 2) {
                h.struck = true;
                ++P.bites;
                out.push_back({Event::Bark, Role::Hound, eye - vec3{0, 0, 0.5f}, cue(KC_HP_MAUL)});
                const size_t tg = (size_t)std::clamp(a.target, 0, 3);
                if (!l3) {  // Level 2: ein schwerer Biss - er laesst los und kreist weiter
                    out.push_back({Event::Hit, Role::Hound, eye - vec3{0, 0, 0.6f}, 22.0f});
                    P.interest += 0.15f;
                    P.token = 0, P.tokenCool = rng_.uniform(3.5, 5.5);
                    h.flinchT = 2.0;
                    setState(2);
                    break;
                }
                // Level 3: quaelen in Stufen (nie sofort toedlich) - Nacken, Bein, Kehle, dann Bisse
                const int st = P.stage[tg]++;
                ++P.torment;
                if (st == 0) {
                    out.push_back({Event::Hit, Role::Hound, eye - vec3{0, 0, 0.2f}, 12.0f});
                    addDose(a.target, 0.95f);  // gelaehmt, aber lebendig
                } else if (st == 1) {
                    out.push_back({Event::Hit, Role::Hound, ppos + vec3{0, 0, 0.4f}, 12.0f});
                    P.limp[tg] = 55.0;  // das Bein - er kann nicht mehr fliehen (hinkt, kein Sprint)
                    addDose(a.target, 0.45f);
                } else if (st == 2) {
                    out.push_back({Event::Hit, Role::Hound, eye - vec3{0, 0, 0.15f}, 12.0f});
                    P.mute[tg] = 90.0;  // die Kehle - kein Ruf
                    out.push_back({Event::Breath, Role::Hound, eye, cue(KC_HP_RASP)});
                } else {
                    out.push_back({Event::Hit, Role::Hound, eye - vec3{0, 0, 0.4f}, 18.0f});
                }
                h.holdT = 0.9;
                break;
            }
            if (!walkTo(a, fld(), l3 ? 6.0 : 6.5, dt, c) || h.leapT > 1.5) {  // daneben
                P.token = 0, P.tokenCool = 2.0;
                h.ambush = false;
                setState(2);
            }
            break;
        }
        case 4: {  // zieht ab
            a.hang = 0.0f;
            a.crouch = 0.2f;
            a.look = false;
            a.jaw = std::max(0.0f, a.jaw - (float)dt);
            a.timer2 -= dt;
            if ((!a.hasGoal || !walkTo(a, a.field, a.stateT < 3.0 ? 3.0 : 1.6, dt, c)) && a.timer2 <= 0.0) {
                if (!pickGoal(c, a, 260, 380, true) && a.unseenT > 1.5) a.state = 9;
                a.timer2 = 1.0;
            }
            if ((d > 24.0f && a.unseenT > 1.5) || (a.stateT > 40.0 && a.unseenT > 1.5)) {
                out.push_back({Event::Gone, Role::Hound, a.pos});
                a.state = 9;
            }
            break;
        }
        default: break;
    }
    // Level 2: die Hounds sind Wanderer - nach langer Zeit ohne Begegnung zieht das Rudel weiter
    if (a.age > 320.0 && a.state == 0 && P.phase == 0 && a.unseenT > 5.0) setState(4), pickGoal(c, a, 260, 380, true);
}

// --- Klaenge (Klangnummern) ----------------------------------------------------------------------------------------------------
namespace {
const char* const kHintCall = "Bellen, dann Antworten aus anderen Gängen – sie rufen einander zur Jagd. Blickkontakt hält sie kurz "
                              "zurück; langsam zurückweichen, nicht rennen.";
const char* const kHintFast = "Das Bellen wird schneller – der Weg vor dir endet oder führt zu den anderen. Umkehren.";
const char* const kHintLunge = "Ein scharfes Kläffen – gleich springt einer. Umdrehen und ihn ansehen.";
const char* const kHintMute = "Die Kehle ist zerbissen – du bringst keinen Ruf mehr heraus.";
const char* const kHintBig = "Über dir an der Decke bewegt sich etwas Großes. Es ist blind – still sein, leise weitergehen.";
const char* const kHintStrike = "Knochen schabt über Stein – es holt aus. Leise zur Seite.";
const char* const kHintTear = "Es reißt sich den eigenen Knochen frei.";
const std::array<KinLine, KC_PACK_END - KC_L3_END>& packLines() {
    static const std::array<KinLine, KC_PACK_END - KC_L3_END> L = [] {
        std::array<KinLine, KC_PACK_END - KC_L3_END> t{};
        auto at = [&](int k) -> KinLine& { return t[(size_t)(k - KC_L3_END)]; };
        at(KC_HP_CALL) = {"hp_call", nullptr, 1.0f, 60.0f, 2.6f, kHintCall};
        at(KC_HP_ANSWER) = {"hp_answer", nullptr, 0.95f, 70.0f};
        at(KC_HP_FAST) = {"hp_fast", nullptr, 1.0f, 60.0f, 2.6f, kHintFast};
        at(KC_HP_YIP) = {"hp_yip", nullptr, 0.55f, 26.0f};
        at(KC_HP_LUNGE) = {"hp_lunge", nullptr, 1.0f, 30.0f, 2.6f, kHintLunge};
        at(KC_HP_MAUL) = {"hp_maul", nullptr, 1.0f, 18.0f};
        at(KC_HP_FLINCH) = {"hp_flinch", nullptr, 0.7f, 20.0f};
        at(KC_HP_RUSH) = {"hp_rush", nullptr, 0.9f, 40.0f};
        at(KC_HP_LURK) = {"hp_lurk", nullptr, 0.35f, 7.0f};
        at(KC_HP_RASP) = {"hp_rasp", nullptr, 0.8f, 6.0f, 2.6f, kHintMute};
        at(KC_CL_CEIL) = {"cl3_ceiling", nullptr, 0.75f, 22.0f, 2.6f, kHintBig};
        at(KC_CL_BONES) = {"cl3_bones", nullptr, 0.55f, 16.0f};
        at(KC_CL_BREATH) = {"cl3_breath", nullptr, 0.6f, 14.0f};
        at(KC_CL_DROP) = {"cl3_drop", nullptr, 1.0f, 34.0f};
        at(KC_CL_SCRAPE) = {"cl3_scrape", nullptr, 0.95f, 24.0f, 2.6f, kHintStrike};
        at(KC_CL_SWING) = {"cl3_swing", nullptr, 0.9f, 16.0f};
        at(KC_CL_HIT) = {"cl3_hit", nullptr, 1.0f, 16.0f};
        at(KC_CL_MISS) = {"cl3_miss", nullptr, 1.0f, 30.0f};
        at(KC_CL_SNAP) = {"cl3_snap", nullptr, 1.0f, 30.0f};
        at(KC_CL_TEAR) = {"cl3_tear", nullptr, 1.0f, 22.0f, 2.6f, kHintTear};
        at(KC_CL_STEP) = {"cl3_step", nullptr, 0.5f, 16.0f};
        return t;
    }();
    return L;
}
}  // namespace

const KinLine* packLine(int cue) {
    if (cue < KC_L3_END || cue >= KC_PACK_END) return nullptr;
    const KinLine& L = packLines()[(size_t)(cue - KC_L3_END)];
    return L.sound ? &L : nullptr;
}

const KinLine* packLineBySound(const std::string& s) {
    for (const KinLine& L : packLines())
        if (L.sound && s == L.sound) return &L;
    return nullptr;
}

}  // namespace lim::game
