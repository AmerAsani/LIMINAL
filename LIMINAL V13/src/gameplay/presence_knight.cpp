// V13.3 Level 3 (KREATUR_RedKnight.md): der Red Knight (Entity 71). Wikidot, CC BY-SA 3.0, eigene Worte:
//   entity-71 (Rev. 44): ein Ritter in Plattenruestung des spaeten 16. Jahrhunderts, mittelgross, ein Zweihaender auf der
//     Schulter, ein roter Umhang mit Goldsaum, am Ende zerrissen; Dellen in der Ruestung; schwere Schritte ("Klirren");
//     uebermenschlich stark und schnell, aber verwundbar; schneidet mit dem Schwert einen leuchtend weissen Riss in die Luft
//     (Durchgang); rettet Neuankoemmlinge, spricht kaum.
//   level-3 (Rev. 18), Vorfall GB-03 (10.11.2017): er rannte in Panik durch das Atrium von Base Gamma, die Ruestung schwer
//     verbeult, mit Spuren der Purpifizierung; nur 10 s Aufnahme blieben, der Rest war zerstoert; Zeugen sahen ihn vorbei
//     laufen, aber niemand erinnert sich, wie er kam oder ging. Absicht unbekannt; Vermutung: eine antimemetische Kraft war
//     in der Basis, und er kam, um sich darum zu kuemmern.
//
// Umsetzung (Interpretation, gekennzeichnet im Briefing): kein Gegner. Traeger ist die Rolle Wretch, Variante 4 (keine eigene
// Rolle, kein eigenes anim::Entity). Selten (presence.red_knight_every, etwa alle 15 min, hoechstens zweimal je Besuch):
//   0 Anlauf    ausser Sicht 20-33 m Weg entfernt; Geraeusch vor Sicht (Platten, Kettengeflecht, Sabatons, Keuchen im Helm)
//   1 Lauf      in Panik (5 m/s) auf einem Weg, der 2,5-5 m am Wanderer vorbeifuehrt (Wegpunkt W)
//   2 Halt      bei W: er bremst, dreht sich zurueck und haut beidhaendig in die leere Luft hinter sich - gegen etwas, das
//               man nicht sieht (nie zum Wanderer hin, nie naeher als 2,5 m)
//   3 Blick     er sieht den Wanderer an (0,9 s), keuchend
//   4 weiter    zur anderen Seite (E, 16-33 m, dem Anlauf gegenueber)
//   5 Riss      bei E, ungesehen: er schneidet einen Riss in die Luft (weisses Licht um die Ecke, Klang) und ist fort
//   Verfolger   danach kommt, unsichtbar, das, wovor er floh: entlang seines Wegs flackern die Leuchten, ein Druck in den
//               Ohren zieht vorbei; nah am Wanderer zehrt es am Verstand (Vision, -3) - nichts greift an
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include <format>

#include "core/log.hpp"
#include "core/rng.hpp"
#include "gameplay/kin_l3.hpp"

namespace lim::game {

namespace {
// Klangnummern (kin_l3.hpp: Staerke kKinCue + Nummer); eigener Bereich ab 200
enum : int { RK_STEP = 200, RK_BREATH, RK_HALT, RK_SLASH, RK_RIFT, RK_UNSEEN, RK_END };

const std::array<KinLine, RK_END - RK_STEP>& knightLines() {
    static const std::array<KinLine, RK_END - RK_STEP> L = {{
        {"rk_step", nullptr, 0.95f, 34.0f, 2.6f, "Metall klirrt im Laufschritt – jemand in Rüstung rennt."},
        {"rk_breath", nullptr, 0.7f, 16.0f, 2.6f, nullptr},
        {"rk_halt", nullptr, 0.9f, 26.0f, 2.6f, nullptr},
        {"rk_slash", nullptr, 1.0f, 30.0f, 2.6f, nullptr},
        {"rk_rift", nullptr, 1.0f, 40.0f, 2.6f, nullptr},
        {"rk_unseen", nullptr, 1.0f, 30.0f, 2.6f, "Da ist etwas. Du siehst es nicht – du hörst nur, dass es nicht da ist."},
    }};
    return L;
}

constexpr double kRun = 5.0;  // m/s - Panik in voller Ruestung (der Sprint des Wanderers: 5,7)

struct KnightRun {
    vec3 start, pass, end, from;
    int leg = 0;  // 0 zum Wegpunkt, 1 zum Ziel
    double breathT = 0.5, seenT = 0.0, legT = 0.0, retry = 0.0;
    int retries = 0;
    bool slashed = false, seen = false;
    float closest = 1e9f;
    std::vector<vec3> trail;
};
struct KnightRegie {
    double next = -1.0;
    int runs = 0;
    bool quick = false, forced = false;
    // der Verfolger: Weg, Zeit bis zum Start, Fortschritt (m), Klang, ob er schon am Verstand zehrte
    std::vector<vec3> trail;
    double pursuerIn = -1.0, pursuerS = -1.0, humT = 0.0, flickT = 0.0;
    bool pursuerHit = false;
    // Riss (Licht): Ort und Alter
    vec3 riftPos;
    double riftT = -1.0;
    Presence::KnightInfo last;
    // eigener Zufall: die Laeufe des Ritters verschieben die Wuerfe der anderen Wesen nicht
    Rng rng{0x71A5EEDu};
};
KnightRegie& kreg(std::shared_ptr<void>& p) {
    if (!p) p = std::make_shared<KnightRegie>();
    return *static_cast<KnightRegie*>(p.get());
}
template <class A>
KnightRun& krun(A& a) {
    if (!a.ext) a.ext = std::make_shared<KnightRun>();
    return *static_cast<KnightRun*>(a.ext.get());
}
float trailLen(const std::vector<vec3>& t) {
    float s = 0.0f;
    for (size_t i = 1; i < t.size(); ++i) s += hdist(t[i - 1], t[i]);
    return s;
}
// ungesehen? (wie hiddenSpawn: Kopf, Brust, Knie ausserhalb des Blicks oder verdeckt)
bool hiddenFrom(const Presence::Context& c, vec3 p) {
    for (float h : {1.75f, 1.0f, 0.4f}) {
        const vec3 tgt = p + vec3{0, 0, h};
        if (dot(nrmF(tgt - c.view.eye, c.view.look), c.view.look) > 0.3f && clearSight(*c.world, c.view.eye, tgt)) return false;
    }
    return true;
}
// naechster Abstand eines Weges (dem Feld f von 'from' aus gefolgt) zum Punkt P
float pathClosest(const NavField& f, vec3 from, vec3 P, int maxSteps = 120) {
    double x = from.x, y = from.y;
    float best = hdist(from, P);
    for (int i = 0; i < maxSteps; ++i) {
        double tx, ty;
        if (!f.next(x, y, tx, ty) || (std::fabs(tx - x) < 1e-6 && std::fabs(ty - y) < 1e-6)) break;
        x = tx, y = ty;
        best = std::min(best, (float)std::hypot(x - P.x, y - P.y));
    }
    return best;
}
vec3 trailAt(const std::vector<vec3>& t, float s) {
    for (size_t i = 1; i < t.size(); ++i) {
        const float l = hdist(t[i - 1], t[i]);
        if (s <= l) return t[i - 1] + (t[i] - t[i - 1]) * (l > 1e-4f ? s / l : 0.0f);
        s -= l;
    }
    return t.empty() ? vec3{} : t.back();
}
}  // namespace

const KinLine* knightLine(int cue) {
    if (cue < RK_STEP || cue >= RK_END) return nullptr;
    return &knightLines()[(size_t)(cue - RK_STEP)];
}
const KinLine* knightLineBySound(const std::string& s) {
    for (const KinLine& L : knightLines())
        if (L.sound && s == L.sound) return &L;
    return nullptr;
}

// --- Regie --------------------------------------------------------------------------------------------------------------
void Presence::regieKnight(double dt, const Context& c, std::vector<Event>& out) {
    KnightRegie& R = kreg(knight_);
    // der Verfolger (auch nach dem Abgang des Ritters)
    if (R.riftT >= 0.0) R.riftT += dt;
    if (R.riftT > 2.5) R.riftT = -1.0;
    if (R.pursuerIn >= 0.0) {
        R.pursuerIn -= dt;
        if (R.pursuerIn < 0.0) R.pursuerS = 0.0, R.pursuerHit = false, R.humT = 0.0, R.flickT = 0.0, R.pursuerIn = -1.0;
    }
    if (R.pursuerS >= 0.0) {
        R.pursuerS += dt * 6.5;
        const float L = trailLen(R.trail);
        const vec3 p = trailAt(R.trail, (float)R.pursuerS);
        const float d = hdist(p, c.view.eye);
        R.humT -= dt, R.flickT -= dt;
        if (R.humT <= 0.0 && d < 30.0f) {
            // der Druck zieht vorbei (Klang); nah am Wanderer einmal als Vision: es zehrt am Verstand (-3)
            const bool hit = d < 8.0f && !R.pursuerHit;
            out.push_back({hit ? Event::Vision : Event::Step, Role::Wretch, p + vec3{0, 0, 1.4f}, kKinCue + (float)RK_UNSEEN});
            R.pursuerHit = R.pursuerHit || hit;
            R.humT = 1.3;
        }
        if (R.flickT <= 0.0 && d < 16.0f) {
            out.push_back({Event::Flicker, Role::Wretch, p + vec3{0, 0, 2.0f}});
            R.flickT = 0.45;
        }
        if ((float)R.pursuerS > L) {
            R.pursuerS = -1.0;
            log::info("Praesenz: Red Knight - der Verfolger ist vorbei");
        }
    }
    if (c.redKnightEvery <= 0.0 || c.mode <= 0 || c.suspended) return;
    if (R.next < 0.0) R.next = R.rng.uniform(0.45, 1.0) * c.redKnightEvery;
    bool active = false;
    for (const auto& a : agents_) active = active || (a->role == Role::Wretch && a->variant == kKnightVariant && !a->showcase);
    if (active || R.pursuerS >= 0.0 || R.runs >= 2) return;
    R.next -= dt * (R.quick ? 20.0 : 1.0);
    if (R.next > 0.0) return;
    if (hunting()) {  // nicht mitten in eine Jagd
        R.next = 20.0;
        return;
    }
    R.next = spawnKnight(c, out, R.forced) ? R.rng.uniform(0.7, 1.3) * c.redKnightEvery : (R.forced ? 1.0 : 15.0);
    if (R.next > 1.0) R.forced = false;
}

bool Presence::spawnKnight(const Context& c, std::vector<Event>& out, bool forced) {
    if (!fld().valid()) return false;
    KnightRegie& R = kreg(knight_);
    const vec3 eye = c.view.eye, P{eye.x, eye.y, (float)c.player->z};
    const auto walk = [&](vec3 q) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
        return t && figureWalkable(t, 2.0f) && !(t->flags & world::TF_STAIR);
    };
    auto a = std::make_unique<Agent>();
    std::vector<std::pair<i64, i64>> ringS;
    fld().ring(170, 285, ringS);
    if (ringS.empty()) return false;
    for (int tries = 0; tries < 8; ++tries) {
        vec3 S;
        bool okS = false;
        for (int k = 0; k < 60 && !okS; ++k) {
            auto [x, y] = ringS[(size_t)R.rng.randint(0, (i64)ringS.size() - 1)];
            const world::Tile* t = c.world->tileIfLoaded(x, y);
            if (!t || t->clearance() < 2.1f) continue;
            S = {(float)x + 0.5f, (float)y + 0.5f, t->floor};
            okS = walk(S) && hiddenFrom(c, S);
        }
        if (!okS) break;
        const vec3 toS = nrmF(flat(S - P), {1, 0, 0});
        // Wegpunkt W: 2,5-5 m vom Wanderer, frei sichtbar, seitlich zum Anlauf (nicht genau zwischen ihm und S)
        std::vector<std::pair<i64, i64>> ring;
        fld().ring(25, 52, ring);
        vec3 W;
        float bestW = 1e9f;
        for (int k = 0; k < 60 && !ring.empty(); ++k) {
            auto [x, y] = ring[(size_t)R.rng.randint(0, (i64)ring.size() - 1)];
            const world::Tile* t = c.world->tileIfLoaded(x, y);
            if (!t || !figureWalkable(t, 2.0f) || std::fabs(t->floor - P.z) > 0.4f) continue;
            const vec3 q{(float)x + 0.5f, (float)y + 0.5f, t->floor};
            if (!clearSight(*c.world, eye, q + vec3{0, 0, 1.4f})) continue;
            const float side = std::fabs(dot(nrmF(flat(q - P), toS), toS));
            const float score = side + (float)R.rng.uniform(0.0, 0.4);
            if (score < bestW) bestW = score, W = q;
        }
        if (bestW > 1e8f) continue;
        a->field.build(*c.world, ifloor(W.x), ifloor(W.y), 2.0f);
        if (a->field.at(ifloor(S.x), ifloor(S.y)) < 0 || pathClosest(a->field, S, P) < 2.2f) continue;  // nicht durch den Wanderer
        // Ziel E: dem Anlauf gegenueber, 16-33 m Weg, von W aus erreichbar
        std::vector<std::pair<i64, i64>> far;
        fld().ring(140, 285, far);
        NavField fe;
        vec3 E;
        bool okE = false;
        for (int k = 0; k < 40 && !far.empty() && !okE; ++k) {
            auto [x, y] = far[(size_t)R.rng.randint(0, (i64)far.size() - 1)];
            const world::Tile* t = c.world->tileIfLoaded(x, y);
            if (!t || !figureWalkable(t, 2.0f) || (t->flags & world::TF_STAIR)) continue;
            const vec3 q{(float)x + 0.5f, (float)y + 0.5f, t->floor};
            if (dot(nrmF(flat(q - P), toS), toS) > -0.2f + 0.12f * (float)tries) continue;  // gegenueber (spaetere Versuche lockerer)
            fe.build(*c.world, x, y, 2.0f);
            if (fe.at(ifloor(W.x), ifloor(W.y)) < 0 || pathClosest(fe, W, P) < 2.2f) continue;
            E = q, okE = true;
        }
        if (!okE) continue;
        a->role = Role::Wretch;
        a->ent = Entity::Wretch;
        a->variant = kKnightVariant;
        a->glow = 1.0f;
        a->initBody(S, std::atan2(W.y - S.y, W.x - S.x), (unsigned)R.rng.randint(1, 1 << 30));
        a->rig.setGlow(1.0f);
        a->state = 0;
        KnightRun& k = krun(*a);
        k.start = S, k.pass = W, k.end = E, k.from = S;
        k.trail.push_back(S);
        ++R.runs;
        R.last = {};
        log::info("Praesenz: Red Knight laeuft{} - von ({:.1f}, {:.1f}) ueber ({:.1f}, {:.1f}) nach ({:.1f}, {:.1f}), {:.0f} m vom Wanderer",
                  forced ? " (erzwungen)" : "", S.x, S.y, W.x, W.y, E.x, E.y, hdist(S, eye));
        out.push_back({Event::Appear, Role::Wretch, S});
        agents_.push_back(std::move(a));
        return true;
    }
    log::info("Praesenz: Red Knight - kein Weg am Wanderer vorbei (spaeter)");
    return false;
}

// --- Denken ---------------------------------------------------------------------------------------------------------------
void Presence::thinkKnight(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    KnightRun& k = krun(a);
    KnightRegie& R = kreg(knight_);
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool seen = a.sight.level > 0.25f;
    a.stateT += dt;
    k.legT += dt;
    k.closest = std::min(k.closest, d);
    if (seen) k.seen = true, k.seenT += dt;
    a.strike = 0.0f, a.wave = 0.0f, a.crouch = 0.0f, a.perceive = 0.0f, a.look = false;
    a.alert = 1.0f;  // Panik: der Atem fliegt, der Kopf zuckt
    auto cue = [&](int n, vec3 at) { out.push_back({Event::Step, Role::Wretch, at, kKinCue + (float)n}); };
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    if (k.trail.empty() || hdist(k.trail.back(), a.pos) > 1.5f) k.trail.push_back(a.pos);
    // Schritte und Atem (Geraeusch vor Sicht)
    if (a.cre.stepped()) cue(RK_STEP, a.cre.stepPos());
    k.breathT -= dt;
    if (k.breathT <= 0.0 && a.state != 5) cue(RK_BREATH, head), k.breathT = a.cre.running() ? R.rng.uniform(0.9, 1.3) : R.rng.uniform(1.6, 2.2);
    R.last.active = true, R.last.state = a.state, R.last.pos = a.pos, R.last.closest = k.closest, R.last.slashed = k.slashed, R.last.seen = k.seen;
    R.last.runs = R.runs;
    // Blick zurueck im Lauf (ueber die Schulter zu dem, was ihm folgt)
    const vec3 back = a.pos - dirYaw(a.yaw) * 4.0f + vec3{0, 0, 1.6f};
    switch (a.state) {
        case 0:  // zum Wegpunkt am Wanderer vorbei
        case 4: {  // zum Ziel
            const vec3 goal = a.state == 0 ? k.pass : k.end;
            if (std::fmod(a.age, 3.1) > 2.35) a.perceive = 1.0f, a.perceiveAt = back;
            const bool moved = walkTo(a, a.field, kRun, dt, c);
            if (a.state == 0 && (hdist(a.pos, goal) < 1.0f || !moved || k.legT > 14.0)) {
                // Halt: nur mit Abstand zum Wanderer und wenn er ihn sieht - sonst gleich weiter
                const bool slash = d > 2.5f && d < 10.0f && clearSight(*c.world, head, eye);
                k.from = k.trail.size() > 3 ? k.trail[k.trail.size() - 4] : k.start;
                if (slash) {
                    setState(2);
                    cue(RK_HALT, a.pos + vec3{0, 0, 1.0f});
                } else {
                    a.field.build(*c.world, ifloor(k.end.x), ifloor(k.end.y), 2.0f);
                    setState(4), k.legT = 0.0;
                }
            } else if (a.state == 4 && (hdist(a.pos, goal) < 1.2f || !moved || k.legT > 16.0)) {
                if (!seen || k.retries >= 2 || k.seenT > 12.0) {
                    setState(5);
                } else {  // man sieht ihn noch: weiter weg, ausser Sicht
                    ++k.retries;
                    std::vector<std::pair<i64, i64>> far;
                    fld().ring(200, 330, far);
                    const vec3 away = nrmF(flat(a.pos - eye), dirYaw(a.yaw));
                    bool ok = false;
                    for (int n = 0; n < 40 && !far.empty() && !ok; ++n) {
                        auto [x, y] = far[(size_t)R.rng.randint(0, (i64)far.size() - 1)];
                        const world::Tile* t = c.world->tileIfLoaded(x, y);
                        if (!t || !figureWalkable(t, 2.0f)) continue;
                        const vec3 g{(float)x + 0.5f, (float)y + 0.5f, t->floor};
                        if (dot(nrmF(flat(g - eye), away), away) < 0.3f) continue;
                        a.field.build(*c.world, x, y, 2.0f);
                        if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) continue;
                        k.end = g, k.legT = 0.0, ok = true;
                    }
                    if (!ok) setState(5);
                }
            }
            break;
        }
        case 2: {  // Halt, zurueckdrehen, beidhaendiger Hieb in die leere Luft (weg vom Wanderer)
            vec3 dir = nrmF(flat(k.from - a.pos), -dirYaw(a.yaw));
            const vec3 toP = nrmF(flat(eye - a.pos), dir);
            if (dot(dir, toP) > 0.64f) dir = rotateYaw(dir, dot(cross(toP, dir), vec3{0, 0, 1}) >= 0.0f ? 1.05f : -1.05f);  // nie zum Wanderer
            a.yaw += wrapA(std::atan2(dir.y, dir.x) - a.yaw) * (float)std::min(1.0, dt * 9.0);
            const double t = a.stateT - 0.35;  // erst bremsen und drehen
            if (t > 0.0) a.strike = (float)std::min(1.0, t / 1.15);
            if (!k.slashed && a.strike > 0.47f) {
                k.slashed = true;
                cue(RK_SLASH, a.pos + dir * 1.0f + vec3{0, 0, 1.2f});
                log::info("Praesenz: Red Knight - Hieb in die leere Luft ({:.1f} m vom Wanderer)", d);
            }
            if (a.stateT > 1.65) setState(3);
            break;
        }
        case 3: {  // er sieht den Wanderer an, keuchend
            a.look = true, a.lookAt = eye;
            a.perceive = 1.0f, a.perceiveAt = eye;
            const vec3 toP = nrmF(flat(eye - a.pos), dirYaw(a.yaw));
            a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 3.0);
            if (a.stateT > 0.9) {
                a.field.build(*c.world, ifloor(k.end.x), ifloor(k.end.y), 2.0f);
                setState(4), k.legT = 0.0;
            }
            break;
        }
        case 5: {  // der Riss: drehen, Schnitt von links unten nach rechts oben, hindurch
            a.strike = (float)std::min(1.0, a.stateT / 0.95);
            a.wave = 1.0f;
            if (a.stateT > 0.42 && R.riftT < 0.0 && a.stateT < 0.6) {
                R.riftPos = a.pos + dirYaw(a.yaw) * 0.9f + vec3{0, 0, 1.1f};
                R.riftT = 0.0;
                cue(RK_RIFT, R.riftPos);
                log::info("Praesenz: Red Knight - Riss in der Luft ({})", seen ? "gesehen" : "ungesehen");
            }
            if (a.stateT > 1.25) {
                a.state = 9;
                out.push_back({Event::Gone, Role::Wretch, head});
                R.trail = k.trail;
                R.pursuerIn = R.rng.uniform(2.5, 4.0);
                R.last.active = false, R.last.state = 9, R.last.rift = true;
                log::info("Praesenz: Red Knight fort (naechster Abstand {:.1f} m, Hieb {}, gesehen {})", k.closest, k.slashed, k.seen);
            }
            break;
        }
        default: break;
    }
}

// --- Licht, Abfragen, Skripte ---------------------------------------------------------------------------------------------
void Presence::knightLights(std::vector<std::pair<vec3, vec3>>& out, vec3 near) const {
    // der Riss: weiss, kurz hell, dann verglimmend; beim Gast aus den Kanaelen der Puppe (Schnitt im Gang)
    if (knight_) {
        const KnightRegie& R = *static_cast<const KnightRegie*>(knight_.get());
        if (R.riftT >= 0.0 && length(R.riftPos - near) < 40.0f) {
            const float t = (float)R.riftT, k = t < 0.12f ? t / 0.12f : std::exp(-(t - 0.12f) * 2.2f);
            out.push_back({R.riftPos, vec3{2.6f, 2.8f, 3.2f} * (3.2f * k)});
        }
    }
    for (const auto& ap : agents_) {
        const Agent& a = *ap;
        if (a.role != Role::Wretch || a.variant != kKnightVariant || length(a.pos - near) > 30.0f) continue;
        if (a.puppet && a.wave > 0.5f && a.strike > 0.45f)
            out.push_back({a.pos + dirYaw(a.yaw) * 0.9f + vec3{0, 0, 1.1f}, vec3{2.6f, 2.8f, 3.2f} * (3.2f * std::min(1.0f, (a.strike - 0.45f) * 4.0f))});
        // die Purpurspuren glimmen schwach (Interpretation: die Energie der Purpifizierung)
        out.push_back({a.pos + vec3{0, 0, 1.2f}, vec3{0.55f, 0.18f, 0.95f} * 0.12f});
    }
}

Presence::KnightInfo Presence::knightInfo() const {
    KnightInfo i;
    if (knight_) {
        const KnightRegie& R = *static_cast<const KnightRegie*>(knight_.get());
        i = R.last;
        i.runs = R.runs;
        i.pursuer = R.pursuerS >= 0.0 || R.pursuerIn >= 0.0;
    }
    i.active = false;
    for (const auto& a : agents_)
        if (a->role == Role::Wretch && a->variant == kKnightVariant && !a->showcase) i.active = true, i.pos = a->pos, i.state = a->state;
    return i;
}

bool Presence::knightSet(const std::string& key, double v) {
    KnightRegie& R = kreg(knight_);
    if (key == "quick") R.quick = v > 0.5;
    else if (key == "runs") R.runs = (int)v;
    else if (key == "next") R.next = v;
    else if (key == "run") R.next = 0.0001, R.runs = 0, R.forced = true;  // gleich ein Lauf (Skripte, Tests; versucht es jede Sekunde)
    else return false;
    return true;
}

}  // namespace lim::game
