// V13.3: Wrangler (Entity 75, KREATUR_Wrangler.md) - Auftreten und Verhalten.
//
// Ein Durchzug: erst bebt es (Kamera, Staub, flackernde Leuchten, ein Rumpeln, das naeher kommt), dann bricht ein Abschnitt
// des Leibs durch Boden, Wand oder Decke und zieht Ring um Ring vorbei, durchscheinend, mit Verschlucktem darin; zuletzt
// versinkt er. Es spuert Erschuetterungen: wer rennt, springt, laut auftritt oder es beruehrt, den bemerkt es (die Seite,
// Level 1: bebt es, den Bereich leise verlassen). Dann haelt der Leib an, an einer Wand oder im Boden neben einem knackt es,
// tief wird Luft eingesogen - wer jetzt still steht oder leise geht, den verliert es; wer weiter laut ist, den schnappt der
// Kopf aus dem Gang (toedlich). Geworfene Dinge lenken es ab: es schnappt nach dem Geraeusch. Der Anblick des Leibs zieht den
// Blick an und laehmt (die Seite: der Koerper versetzt in einen fast hypnotischen Zustand).
// Weibchen greifen nicht an und ziehen sich zurueck, sobald man sie bemerkt; traechtige Weibchen jagen wie die Maennchen.
// Jungtiere (Level 1, selten): ein Ast mit Jungen an einer Wand; wer sie anfasst, ruft die Mutter.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr int kMale = 0, kFemale = 1, kPregnant = 2, kBrood = 3;
constexpr float kBreakScale = 32.0f;  // Durchbruchstelle des Kopfs: pos + fwd * 32 m * bait (Koop: ein Kanal)
constexpr float kBodyR = 0.74f;

const world::Tile* tileAt(const Presence::Context& c, float x, float y) { return c.world->tileIfLoaded(ifloor(x), ifloor(y)); }

// frei begehbar von p in Richtung d (gleicher Boden, Platz nach oben); Laenge in m (hoechstens maxLen)
float freeRun(const Presence::Context& c, vec3 p, vec3 d, float maxLen) {
    float s = 0.0f;
    for (float t = 0.25f; t <= maxLen; t += 0.25f) {
        const world::Tile* tl = tileAt(c, p.x + d.x * t, p.y + d.y * t);
        if (!tl || !tl->open() || std::fabs(tl->floor - p.z) > 0.35f || tl->clearance() < 1.9f) break;
        s = t;
    }
    return s;
}

// Ringmitten (Leib) des Wesens; versunkene (tief in der Flaeche) zaehlen nicht
template <class A>
float bodyDist(const A& a, vec3 p, vec3* nearest = nullptr) {
    const auto& J = a.cre.pose().j;
    float best = 1e9f;
    for (int r = 0; r < anim::wg::kRings; ++r) {
        const vec3 m = (J[(size_t)anim::wg::ringA(r)] + J[(size_t)anim::wg::ringA(r) + 1]) * 0.5f;
        if (std::fabs(m.z - (a.pos.z + 1.0f)) > 2.6f) continue;
        const float d = length(p - m) - kBodyR;
        if (d < best) best = d, nearest && (*nearest = m, true);
    }
    return best;
}

template <class A>
bool isBrood(const A& a) {
    return a.mode == kBrood;
}
}  // namespace

bool Presence::wranglerActive() const {
    for (const auto& a : agents_)
        if (a->role == Role::Wrangler && !a->showcase && a->mode != kBrood && a->state != 9) return true;
    return false;
}

// --- Regie --------------------------------------------------------------------------------------------------------
void Presence::regieWrangler(double dt, const Context& c, std::vector<Event>& out) {
    if (!c.kin) return;
    const world::KinDef& K = c.kin[world::KIN_WRANGLER];
    if (!K.on) return;
    if (!broodPlaced_ && K.count > 0) broodPlaced_ = placeBrood(c);
    if (wranglerActive()) return;
    // nicht mitten in eine andere Jagd hinein (Hound, Laecheln): dann laeuft die Uhr langsamer
    const bool busy = has(Role::Smiler) || has(Role::Hound);
    nextWrangler_ -= dt * (busy ? 0.25 : 1.0);
    if (nextWrangler_ > 0.0) return;
    // Art: Level-JSON mode male|female|mixed, chance = Anteil der Weibchen (davon 40 % traechtig)
    int mode = kMale;
    if (K.mode == "female" || (K.mode == "mixed" && rng_.uniform(0.0, 1.0) < K.chance)) mode = rng_.uniform(0.0, 1.0) < 0.4 ? kPregnant : kFemale;
    nextWrangler_ = spawnWrangler(c, out, mode) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 8.0;
}

// Ort des Durchzugs: 7..16 m Weg vom Spieler, wo man ihn sehen wird. In Gaengen quer von Wand zu Wand (er reicht in beide
// Waende und taucht dahinter in den Boden), in Hallen als Bogen aus dem Boden (Level 1 manchmal von der Decke herab).
// near (Mutter der Jungtiere): ohne Leib, nur der Kopf - dort bricht er heraus
bool Presence::spawnWrangler(const Context& c, std::vector<Event>& out, int mode, vec3* near) {
    if (!fld().valid() && !near) return false;
    const vec3 P{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const vec3 look = nrmF(flat(c.view.look), {1, 0, 0});
    struct Site {
        vec3 at;
        float yaw = 0, S = 0, W = 0, H = 0, hw = 0, score = 1e9f;
    } best;
    if (near) {
        best.at = *near, best.S = 4.0f, best.W = 0.5f, best.H = 0.9f, best.score = 0.0f;
    } else {
        std::vector<std::pair<i64, i64>> cand;
        fld().ring(70, 170, cand);
        for (int tries = 0; tries < 90 && !cand.empty(); ++tries) {
            auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
            const world::Tile* t = c.world->tileIfLoaded(x, y);
            if (!t || !t->open() || t->clearance() < 2.1f || (t->flags & world::TF_STAIR)) continue;
            const vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->floor};
            if (!roomAllowed(c, p, world::KIN_WRANGLER)) continue;
            // die engste Richtung (quer zum Gang)
            float wMin = 1e9f, rp = 0, rm = 0, th = 0;
            for (int k = 0; k < 8; ++k) {
                const float a = (float)k * kPiF / 8.0f;
                const vec3 d{std::cos(a), std::sin(a), 0.0f};
                const float r1 = freeRun(c, p, d, 12.0f), r2 = freeRun(c, p, d * -1.0f, 12.0f);
                if (r1 + r2 < wMin) wMin = r1 + r2, rp = r1, rm = r2, th = a;
            }
            Site s;
            const vec3 d{std::cos(th), std::sin(th), 0.0f};
            if (wMin <= 6.5f) {  // Gang: quer von Wand zu Wand
                s.at = p + d * ((rp - rm) * 0.5f);
                const world::Tile* ta = tileAt(c, s.at.x, s.at.y);
                if (!ta || !ta->open()) continue;
                s.at.z = ta->floor;
                s.hw = (wMin + 0.25f) * 0.5f + 0.12f;
                s.H = std::min(1.05f, ta->clearance() - 0.86f);
                if (s.H < 0.8f) continue;
                s.W = s.hw + 1.1f, s.S = s.W + 3.0f;
                s.yaw = th + (rng_.uniform(0.0, 1.0) < 0.5 ? kPiF : 0.0f);
            } else {  // Halle: Bogen quer zum Blick des Spielers (man sieht ihn von der Seite)
                s.at = p;
                const vec3 to = nrmF(flat(p - P), look);
                s.yaw = std::atan2(to.y, to.x) + kPiF * 0.5f + (float)rng_.uniform(-0.4, 0.4);
                const vec3 f{std::cos(s.yaw), std::sin(s.yaw), 0.0f};
                if (freeRun(c, p, f, 3.0f) < 2.75f || freeRun(c, p, f * -1.0f, 3.0f) < 2.75f) continue;
                s.W = 1.4f, s.S = 6.8f, s.H = std::min(1.45f, t->clearance() - 1.0f);  // ein hoher Buckel wie ein auftauchender Wal
                if (c.kin && c.kin[world::KIN_WRANGLER].mode == "mixed" && t->clearance() > 3.7f && rng_.uniform(0.0, 1.0) < 0.35)
                    s.H = -(t->clearance() - 1.65f) / 1.0f;  // von der Decke herab (die Seite, Level 1: die Decke bebt)
            }
            // die tiefen Enden der Bahn duerfen nirgends sichtbar sein (keine tieferen Boeden, keine hoeheren Decken)
            const vec3 f{std::cos(s.yaw), std::sin(s.yaw), 0.0f};
            bool safe = true;
            for (float u = -s.S - 2.0f; u <= s.S + 2.0f && safe; u += 0.5f) {
                const world::Tile* tu = tileAt(c, s.at.x + f.x * u, s.at.y + f.y * u);
                if (!tu || tu->solid()) continue;
                if (s.H >= 0.0f && tu->floor < s.at.z - 0.45f) safe = false;
                if (s.H < 0.0f && tu->ceil > s.at.z + t->clearance() + 0.45f) safe = false;
            }
            if (!safe) continue;
            // nicht auf dem Spieler: Abstand zur Bahn
            const vec3 rel = flat(P - s.at);
            const float along = dot(rel, f), across = std::fabs(dot(rel, vec3{-f.y, f.x, 0.0f}));
            const float dLine = std::fabs(along) < s.W + 1.0f ? across : length(rel);
            const float dist = length(rel);
            if (dist < 6.0f || dLine < 3.0f) continue;
            const bool front = dot(nrmF(flat(s.at - P), look), look) > 0.35f;
            s.score = std::fabs(dist - 11.0f) + (front ? 0.0f : 2.5f) + (float)rng_.uniform(0.0, 2.0);
            if (s.score < best.score) best = s;
        }
        if (best.score > 1e8f) return false;
    }
    auto a = std::make_unique<Agent>();
    a->role = Role::Wrangler;
    a->ent = Entity::Wrangler;
    a->mode = a->variant = mode;
    a->lean = best.S / 16.0f, a->crouch = best.W / 10.0f, a->tilt = best.H / 4.0f, a->wave = best.hw / 5.0f;
    a->hang = 0.0f;
    a->fly = (float)rng_.uniform(1.25, 1.75) / 4.0f;
    a->glow = 1.0f;
    a->timer = near ? 2.4 : rng_.uniform(5.5, 8.0);  // Beben bis zum Durchbruch
    a->burst = rng_.uniform(12.0, 18.0);              // Dauer des Durchzugs
    a->timer2 = 0.0;
    a->hungry = near != nullptr;  // Mutter: sie kommt wegen der Jungen
    a->home = near ? *near : best.at;
    a->initBody(best.at, best.yaw, (unsigned)rng_.randint(1, 1 << 30));
    a->rig.setGlow(1.0f);
    out.push_back({Event::Tremor, Role::Wrangler, best.at - vec3{0, 0, 2.0f}, 0.3f});
    log::info("Praesenz: Wrangler ({}) bei ({:.1f}, {:.1f}), {:.0f} m, {} S={:.1f} W={:.1f} H={:.2f}", mode == kMale ? "Maennchen" : (mode == kFemale ? "Weibchen" : "traechtig"),
              best.at.x, best.at.y, hdist(best.at, P), best.hw > 0.0f ? "quer" : (best.H < 0.0f ? "Decke" : "Bogen"), best.S, best.W, best.H);
    agents_.insert(agents_.begin(), std::move(a));  // vor den Jungtieren (posOf, stateOf ... meinen den Durchzug)
    return true;
}

// Durchbruchstelle des Kopfs nahe q (Fuesse): die naechste Wand bis 2,4 m (der Kopf bricht in 1 m Hoehe heraus, Normale zum
// Spieler hin), sonst der Boden 1,1..1,6 m von q in Richtung toward
bool Presence::wranglerBreak(const Context& c, vec3 q, vec3 toward, vec3& at, vec3& n) const {
    float best = 1e9f;
    for (int k = 0; k < 16; ++k) {
        const float a = (float)k * kPiF / 8.0f, dx = std::cos(a), dy = std::sin(a);
        float px = q.x, py = q.y;
        for (float s = 0.2f; s <= 2.4f; s += 0.1f) {
            const float x = q.x + dx * s, y = q.y + dy * s;
            const world::Tile* t = tileAt(c, x, y);
            const bool wall = !t || t->solid() || t->ceil - q.z < 1.5f || t->floor > q.z + 0.5f;
            if (!wall && std::fabs(t->floor - q.z) > 0.4f) break;  // Kante, Stufe: dort nicht
            if (wall) {
                const i64 ix0 = ifloor(px), iy0 = ifloor(py), ix1 = ifloor(x), iy1 = ifloor(y);
                vec3 h, nn;
                if (ix1 != ix0 && std::fabs(dx) > 1e-3f) {
                    const float bx = (float)std::max(ix0, ix1), tt = (bx - q.x) / dx;
                    h = {bx, q.y + dy * tt, 0}, nn = {ix1 > ix0 ? -1.0f : 1.0f, 0, 0};
                } else if (std::fabs(dy) > 1e-3f) {
                    const float by = (float)std::max(iy0, iy1), tt = (by - q.y) / dy;
                    h = {q.x + dx * tt, by, 0}, nn = {0, iy1 > iy0 ? -1.0f : 1.0f, 0};
                } else {
                    break;
                }
                const float d = hdist(h, q);
                if (d < best) best = d, at = vec3{h.x, h.y, q.z + 1.0f}, n = nn;
                break;
            }
            px = x, py = y;
        }
    }
    if (best < 2.4f) return true;
    const vec3 d0 = nrmF(flat(toward - q), {1, 0, 0});
    for (int k = 0; k < 8; ++k) {
        const vec3 d = rotateYaw(d0, (float)((k + 1) / 2) * 0.8f * (k % 2 ? 1.0f : -1.0f));
        for (float s = 1.6f; s >= 1.05f; s -= 0.1f) {
            const vec3 p = q + d * s;
            const world::Tile* t = tileAt(c, p.x, p.y);
            if (t && t->open() && std::fabs(t->floor - q.z) < 0.25f && t->clearance() > 2.0f) {
                at = {p.x, p.y, t->floor}, n = {0, 0, 1};
                return true;
            }
        }
    }
    return false;
}

// --- Verhalten ------------------------------------------------------------------------------------------------------
// Zustaende: 0 Beben (naehert sich), 1 Durchzug, 2 gespuert (Ankuendigung), 3 Zugriff, 5 Kopf zurueck, 4 versinkt, 10 toetet
void Presence::thinkWrangler(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (isBrood(a)) {
        thinkBrood(a, dt, c, out);
        return;
    }
    const vec3 eye = c.view.eye;
    const vec3 P{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const vec3 chest = P + vec3{0, 0, 1.0f};
    a.stateT += dt;
    a.cool -= dt;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    const vec3 Hp = a.pos + nrmF(a.fwd, {1, 0, 0}) * (kBreakScale * a.bait);
    vec3 nearRing = a.pos;
    const float dBody = a.hang > 0.25f ? std::min(bodyDist(a, chest, &nearRing), hdist(a.pos, P) + 1.5f) : hdist(a.pos, P) - 1.0f;
    const bool female = a.mode == kFemale && !a.hungry;
    a.look = a.state == 3 || a.state == 10;
    a.lookAt = eye;
    a.glow = 1.0f;
    // Beben und Leuchten: je naeher, desto staerker; im Raum des Spielers flackert das Licht
    auto flickers = [&](double every) {
        a.timer2 -= dt;
        if (a.timer2 <= 0.0) {
            a.timer2 = rng_.uniform(every * 0.6, every * 1.4);
            out.push_back({Event::Flicker, Role::Wrangler, eye + vec3{0, 0, 0.6f}});
        }
    };
    // Rumpeln: einzelne Stoesse ueber dem Dauerklang (app.cpp)
    auto rumble = [&](double every, float strength) {
        a.growlT -= dt;
        if (a.growlT <= 0.0) {
            a.growlT = rng_.uniform(every * 0.7, every * 1.3);
            out.push_back({Event::Tremor, Role::Wrangler, a.pos - vec3{0, 0, 1.5f}, strength});
        }
    };
    // Erschuetterungen spueren (Maennchen, traechtige Weibchen): Rennen, Springen, laute Schritte nah am Leib, Beruehren;
    // geworfene Dinge in der Naehe. a.provoke: Aufmerksamkeit (ab 1 bemerkt es), a.heard: Quelle, a.knocks: 1 = Geraeusch
    auto sense = [&]() -> bool {
        if (female || a.hungry || a.cool > 0.0 || c.mode < 2) return false;
        const double n = c.noise;
        double gain = 0.0;
        if (n >= 0.75 && dBody < 24.0f) gain = 1.15;
        else if (n >= 0.4) gain = dBody < 6.0f ? 0.6 : (dBody < 11.0f ? 0.25 : 0.0);
        else if (n >= 0.1) gain = dBody < 1.5f ? 0.35 : 0.0;
        if (a.hang > 0.6f && dBody < 0.3f) gain += 4.0;  // beruehrt
        if (gain > 0.0) {
            a.provoke += gain * dt;
            a.heard = P, a.knocks = 0;
        } else {
            a.provoke = std::max(0.0, a.provoke - 0.45 * dt);
        }
        if (c.pings)
            for (const auto& [p, s] : *c.pings) {
                const float dp = a.hang > 0.25f ? bodyDist(a, p) : hdist(p, a.pos);
                if (dp < 14.0f) a.provoke += 0.9 * s, a.heard = p, a.knocks = 1;
            }
        return a.provoke >= 1.0;
    };
    bool sensed = false;
    switch (a.state) {
        case 0: {  // Beben: es kommt naeher
            const float k = (float)std::min(1.0, a.stateT / std::max(0.1, a.timer));
            a.growl = 0.25f + 0.6f * k;
            a.hang = 0.0f, a.alert = 0.0f, a.reach = 0.0f, a.strike = 0.0f, a.jaw = 0.0f, a.shiver = 0.0f, a.perceive = 0.0f;
            rumble(2.6, a.growl);
            if (a.growl > 0.4f) flickers(1.0);
            if (a.hungry) {  // Mutter: kein Leib - gleich der Kopf bei den Jungen
                if (a.stateT >= a.timer) {
                    a.heard = hdist(P, a.home) < 9.0f ? P : a.home, a.knocks = 0, a.provoke = 1.0;
                    sensed = true;
                }
                break;
            }
            if (sense()) {
                sensed = true;
                break;
            }
            if (a.stateT >= a.timer) {
                // nie auf jemandem durchbrechen: steht einer auf der Bahn, wartet es (es bebt weiter) und zieht sonst woanders durch
                const vec3 f = dirYaw(a.yaw), rel = flat(P - a.pos);
                const float W = 10.0f * a.crouch + 1.0f, along = dot(rel, f), across = std::fabs(dot(rel, vec3{-f.y, f.x, 0.0f}));
                const bool onPath = std::fabs(along) < W && across < 2.0f;
                if (onPath && a.stateT < a.timer + 6.0) break;
                if (onPath) {
                    setState(4);
                    out.push_back({Event::Sink, Role::Wrangler, a.pos, 0.5f});
                    break;
                }
                setState(1);
                a.hang = 1.0f;
                out.push_back({Event::Breach, Role::Wrangler, a.pos + vec3{0, 0, 0.3f}, 1.0f});
            }
            break;
        }
        case 1: {  // Durchzug
            a.growl = 0.38f, a.hang = 1.0f, a.alert = 0.0f, a.shiver = 0.0f, a.reach = 0.0f, a.strike = 0.0f, a.jaw = 0.0f;
            a.timer2 -= dt;
            if (a.timer2 <= 0.0 && dBody < 30.0f) {  // Gleiten; das Maennchen stoehnt dann und wann tief
                const bool groan = a.mode == kMale && rng_.uniform(0.0, 1.0) < 0.22;
                a.timer2 = rng_.uniform(1.5, 2.6);
                out.push_back({Event::Slither, Role::Wrangler, nearRing, groan ? 1.0f : 0.5f});
            }
            rumble(4.0, 0.35f);
            if (female && a.seenT > 1.2) {  // bemerkt: das Weibchen zieht sich zurueck
                a.fly = 0.8f;
                setState(4);
                out.push_back({Event::Sink, Role::Wrangler, a.pos, 1.0f});
                break;
            }
            if (sense()) {
                sensed = true;
                break;
            }
            if (a.stateT >= a.burst) {
                setState(4);
                out.push_back({Event::Sink, Role::Wrangler, a.pos, 0.6f});
            }
            break;
        }
        case 2: {  // gespuert: der Leib haelt an und bebt, an der Durchbruchstelle knackt es - in 1,8 s schnappt es zu
            a.alert = 1.0f, a.shiver = 1.0f, a.growl = 0.95f, a.perceive = 1.0f, a.reach = 0.16f, a.jaw = 0.0f, a.strike = 0.0f;
            const bool loud = c.noise >= 0.4 || (a.hang > 0.6f && dBody < 0.3f) || a.hungry;
            if (loud && (!a.hungry || hdist(P, a.home) < 9.0f)) {
                a.lostT = 0.0, a.knocks = 0;
                if ((a.timer2 -= dt) <= 0.0) {  // die Stelle folgt dem, der laut ist
                    a.timer2 = 0.3;
                    vec3 at, n;
                    if (wranglerBreak(c, P, a.pos, at, n)) {
                        a.fwd = nrmF(at - a.pos, {1, 0, 0});
                        a.bait = std::min(1.0f, length(at - a.pos) / kBreakScale);
                        a.up = n;
                    }
                }
            } else {
                a.lostT += dt;
            }
            a.goal = a.knocks ? a.heard + vec3{0, 0, 0.3f} : chest;
            if (a.stateT >= (a.hungry ? 1.3 : 1.8)) {
                if (a.lostT < 0.7 && c.mode >= 2) {
                    setState(3);
                    out.push_back({Event::Burst, Role::Wrangler, Hp, a.mode == kMale ? 1.0f : 0.5f});
                } else {  // es hat einen verloren: der Leib zieht weiter (oder es geht)
                    out.push_back({Event::Calm, Role::Wrangler, Hp, 1.0f});
                    a.provoke = 0.3, a.cool = 3.0, a.reach = 0.0f, a.alert = 0.0f, a.shiver = 0.0f, a.perceive = 0.0f;
                    if (a.hungry) setState(5);
                    else setState(a.hang > 0.5f ? 1 : 0), a.stateT = a.hang > 0.5f ? std::min(a.stateT, a.burst - 4.0) : std::max(0.0, a.timer - 2.0);
                }
            }
            break;
        }
        case 3: {  // Zugriff: der Kopf bricht heraus und schnappt
            a.alert = 1.0f, a.shiver = 0.4f, a.growl = 1.0f, a.reach = 1.0f, a.jaw = 1.0f, a.perceive = 0.0f;
            a.strike = (float)std::clamp((a.stateT - 0.18) / 0.3, 0.0, 1.0);
            a.goal = a.knocks ? a.heard + vec3{0, 0, 0.3f} : chest;
            // bis zum Zuschnappen folgt die Stelle dem, der laut ist (wer rennt, laeuft ihm nicht davon)
            if (!a.knocks && a.stateT < 0.2 && c.noise >= 0.4) {
                vec3 at, nn;
                if (wranglerBreak(c, P, a.pos, at, nn) && length(at - Hp) > 0.6f) {
                    a.fwd = nrmF(at - a.pos, {1, 0, 0});
                    a.bait = std::min(1.0f, length(at - a.pos) / kBreakScale);
                    a.up = nn;
                }
            }
            if (a.stateT >= 0.45 && a.stateT - dt < 0.45) {
                const vec3 nn = nrmF(a.up, {0, 0, 1});
                const bool reach = length(chest - Hp) < 3.9f && std::fabs((float)c.player->z - (Hp.z - (std::fabs(nn.z) > 0.8f ? 0.0f : 1.0f))) < 1.2f &&
                                   clearSight(*c.world, Hp + nn * 0.4f, chest);
                if (!a.knocks && reach && c.mode >= 2) {
                    out.push_back({Event::Attack, Role::Wrangler, a.cre.headPos(), 1.0f});
                    setState(10);
                    log::info("Praesenz: Wrangler - aus dem Gang geschnappt");
                    break;
                }
            }
            if (a.stateT > 0.9) setState(5);
            break;
        }
        case 5: {  // der Kopf zieht sich zurueck
            a.reach = 0.0f, a.strike = 0.0f, a.jaw = 0.25f, a.alert = 0.4f, a.shiver = 0.0f, a.growl = 0.6f;
            if (a.stateT > 1.4) {
                setState(4);
                out.push_back({Event::Sink, Role::Wrangler, a.pos, 0.8f});
            }
            break;
        }
        case 4: {  // versinkt und zieht ab
            a.hang = 0.0f, a.reach = 0.0f, a.alert = 0.0f, a.shiver = 0.0f, a.jaw = 0.0f, a.strike = 0.0f;
            a.growl = std::max(0.0f, 0.4f - 0.08f * (float)a.stateT);
            if (a.stateT > 5.0) a.state = 9;
            break;
        }
        case 10: {  // toetet: der Kopf vor dem Gesicht
            a.reach = 1.0f, a.strike = 1.0f, a.jaw = 1.0f, a.growl = 1.0f, a.alert = 1.0f;
            // das ganze grinsende Gesicht vor der Kamera: die Schnauze haelt gut einen Meter vor dem Auge
            a.goal = eye + nrmF(flat(a.cre.headPos() - eye), nrmF(flat(c.view.look), {1, 0, 0})) * 1.05f - vec3{0, 0, 0.12f};  // auf Augenhoehe
            if (a.stateT > 2.6) a.state = 9;
            break;
        }
        default: break;
    }
    // bemerkt: Stelle waehlen (nahe der Quelle), Ankuendigung
    vec3 at, n;
    if (sensed && !wranglerBreak(c, a.heard, a.pos, at, n)) sensed = false, a.provoke = 0.4;
    if (sensed) {
        a.fwd = nrmF(at - a.pos, {1, 0, 0});
        a.bait = std::min(1.0f, length(at - a.pos) / kBreakScale);
        a.up = n;
        a.lostT = 0.0, a.timer2 = 0.3;
        setState(2);
        out.push_back({Event::Sense, Role::Wrangler, at, a.mode == kMale ? 1.0f : 0.5f});
        log::info("Praesenz: Wrangler spuert ({}) - Durchbruch bei ({:.1f}, {:.1f}, {:.1f})", a.knocks ? "Geraeusch" : "Spieler", at.x, at.y, at.z);
    }
    a.rig.setGlow(a.glow);
}

// --- Jungtiere (Level 1) ----------------------------------------------------------------------------------------------
// Ein Ast mit Jungen an einer Wand (die Seite: das Weibchen legt die Samen in Boden oder Waende, daraus waechst ein Ast mit
// vielen Jungen). Selten: einmal je Ebene, ausser Sicht, 15..45 m Weg vom Spieler.
bool Presence::placeBrood(const Context& c) {
    if (!fld().valid()) return false;
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(150, 450, cand);
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int tries = 0; tries < 120 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || !t->open() || t->clearance() < 2.3f || (t->flags & world::TF_STAIR)) continue;
        const vec3 p{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        if (!roomAllowed(c, p, world::KIN_WRANGLER)) continue;
        for (int k = 0; k < 4; ++k) {
            const world::Tile* w = c.world->tileIfLoaded(x + dx[k], y + dy[k]);
            if (!w || !w->solid()) continue;
            const vec3 n{(float)-dx[k], (float)-dy[k], 0.0f};
            const vec3 at = p - n * 0.5f + vec3{0, 0, 1.55f};
            if (clearSight(*c.world, c.view.eye, at + n * 0.3f)) break;  // nicht vor den Augen
            auto a = std::make_unique<Agent>();
            a->role = Role::Wrangler;
            a->ent = Entity::Wrangler;
            a->mode = a->variant = kBrood;
            a->noShadow = true;
            a->plea = 1.0f;
            a->up = n;
            a->restN = n;
            a->glow = 1.0f;
            a->initBody(at, std::atan2(n.y, n.x), (unsigned)rng_.randint(1, 1 << 30));
            a->rig.setGlow(1.0f);
            log::info("Praesenz: Wrangler-Jungtiere bei ({:.1f}, {:.1f}), {:.0f} m", at.x, at.y, hdist(at, c.view.eye));
            agents_.push_back(std::move(a));
            return true;
        }
    }
    return false;
}

void Presence::thinkBrood(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    const vec3 P{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, P);
    a.stateT += dt;
    a.cool -= dt;
    a.plea = 1.0f;
    a.up = a.restN;
    a.glow = 1.0f;
    lightReact(a, c, a.pos + a.restN * 0.25f - vec3{0, 0, 0.3f}, 9.0f, dt, 1e9);
    // naehert man sich, werden sie unruhig und fiepen (Warnung)
    const bool near = d < 3.5f && std::fabs(P.z - (a.pos.z - 1.55f)) < 1.0f;
    a.alert = near ? 1.0f : 0.0f;
    a.perceive = near || a.light > 0.5f ? 1.0f : 0.0f;
    a.timer -= dt;
    if (a.timer <= 0.0 && d < 14.0f) {
        a.timer = near ? rng_.uniform(0.8, 1.8) : rng_.uniform(5.0, 10.0);
        out.push_back({Event::Squeak, Role::Wrangler, a.pos + a.restN * 0.3f - vec3{0, 0, 0.35f}, near ? 0.8f : 0.3f});
    }
    // angefasst (oder lange angeleuchtet, ganz nah): die Mutter spuert es und kommt
    if (near && d < 3.0f && a.lightT > 4.0) a.provoke += dt;
    if (d < 1.05f) a.provoke += dt * 3.0;
    a.provoke = std::max(0.0, a.provoke - dt * 0.2);
    if (a.provoke > 0.5 && a.cool <= 0.0 && c.mode >= 2) {
        a.provoke = 0.0;
        a.cool = 45.0;
        vec3 at = a.pos - vec3{0, 0, 1.55f} + a.restN * 0.6f;
        out.push_back({Event::Squeak, Role::Wrangler, a.pos + a.restN * 0.3f, 1.0f});
        if (spawnWrangler(c, out, kFemale, &at)) log::info("Praesenz: die Jungen wurden beruehrt - die Mutter kommt");
    }
    a.rig.setGlow(a.glow);
}

// --- Klangquellen (app/wrangler_audio.cpp): der naechste Wrangler ---------------------------------------------------------
bool Presence::wranglerSound(vec3 ear, vec3& rumbleAt, float& rumble, vec3& bodyAt, float& body) const {
    const Agent* best = nullptr;
    for (const auto& a : agents_)
        if (a->role == Role::Wrangler && a->mode != kBrood && (!best || length(a->pos - ear) < length(best->pos - ear))) best = a.get();
    if (!best) return false;
    const Agent& a = *best;
    const vec3 Hp = a.pos + nrmF(a.fwd, {1, 0, 0}) * (kBreakScale * a.bait);
    // das Rumpeln kommt aus der Tiefe unter der Bahn; spuert es einen, von der Durchbruchstelle
    rumbleAt = a.reach > 0.05f ? Hp : a.pos - vec3{0, 0, 1.5f};
    rumble = a.growl;
    bodyAt = a.pos + vec3{0, 0, 1.0f};
    if (a.hang > 0.25f) bodyDist(a, ear, &bodyAt);
    body = a.hang * (1.0f - 0.85f * a.alert) * std::clamp(a.fly * 2.5f, 0.0f, 1.0f);
    return true;
}

// --- Beben und Sog am eigenen Spieler (Host und Gast: aus Lage und Kanaelen der Wesen) -------------------------------------
void Presence::wranglerFeel(double dt, const Context& c) {
    if (!c.world) return;
    const vec3 eye = c.view.eye, look = c.view.look;
    float q = 0.0f, lure = 0.0f, wsum = 0.0f;
    vec3 sum{};
    for (const auto& ap : agents_) {
        const Agent& a = *ap;
        if (a.role != Role::Wrangler || a.showcase || a.mode == kBrood) continue;
        const vec3 Hp = a.pos + nrmF(a.fwd, {1, 0, 0}) * (kBreakScale * a.bait);
        float d = length(eye - a.pos);
        if (a.reach > 0.05f) d = std::min(d, length(eye - Hp));
        q = std::max(q, a.growl * std::clamp(1.0f - (d - 3.0f) / 28.0f, 0.0f, 1.0f));
        if (a.hang < 0.5f) continue;
        // der Leib im Blick (jeder zweite Ring), nicht zu weit
        const auto& J = a.cre.pose().j;
        int vis = 0;
        vec3 s{};
        for (int r = 0; r < anim::wg::kRings; r += 2) {
            const vec3 m = (J[(size_t)anim::wg::ringA(r)] + J[(size_t)anim::wg::ringA(r) + 1]) * 0.5f;
            const vec3 dv = m - eye;
            const float l = length(dv);
            if (l > 17.0f || l < 0.3f || dot(dv / l, look) < 0.86f) continue;
            if (!clearSight(*c.world, eye, m)) continue;
            ++vis, s += m;
        }
        if (!vis) continue;
        const vec3 cm = s / (float)vis;
        const float k = std::clamp((float)vis / 4.0f, 0.0f, 1.0f) * std::clamp(1.0f - (length(cm - eye) - 8.0f) / 9.0f, 0.0f, 1.0f);
        if (k > lure) lure = k;
        sum += cm * k, wsum += k;
    }
    quake_ += (q - quake_) * (float)std::min(1.0, dt * 4.0);
    if (lure > 0.0f) {
        trance_ = std::min(1.0f, trance_ + (float)dt * 0.4f * lure);
        if (wsum > 1e-4f) tranceAt_ = sum / wsum;
    } else {
        trance_ = std::max(0.0f, trance_ - (float)dt * 0.5f);
    }
}

}  // namespace lim::game
