// V13.3: Ouroboros-Arbeiter (Wikidot level-1, Sektor Ouroboros, Abschnitt "Sonst auf der Seite genannt"; backrooms-remodeling-co;
// KREATUR_OuroborosArbeiter.md) - Auftreten und Verhalten.
//
// Der kleinste Sektor von Level 1 ist seit Jahrzehnten eine Baustelle: schattenhafte Arbeiter in Overalls reparieren, ohne dass
// je etwas fertig wird. Sie beachten Lebende nicht; verletzt wird man nur von ihren Maschinen. Hier: eine Mauer, die sie Stein um
// Stein hochziehen und, kaum fertig, wieder einreissen; ein Fahrgeruest, an dem geschweisst wird und das immer wieder umkippt.
//
//   Arbeiter   0 Maurer      holt Steine von der Palette und setzt sie, klopft mit der Kelle nach
//              1 Klopfer     haemmert auf die frisch gesetzten Steine
//              2 Brecher     (schwer) wartet gebueckt an der Ecke; reisst die Mauer ein: Brechstange, dann Presslufthammer
//              3 Schweisser  kniet am Geruest und schweisst (Funken); hebt das umgestuerzte Geruest wieder auf
//              4 Traeger     traegt Steine quer durch den Sektor zur Palette; hilft beim Aufrichten
//   Mauer      0 bauen -> 1 begutachten (alle halten inne, Stille) -> 2 Warnung (Piepen, Warnleuchte) -> 3 einreissen (die oberen
//              Lagen kippen nach hinten) -> 4 Presslufthammer (Kompressor laeuft an, dann Brocken) -> 5 aufraeumen -> 0
//   Geruest    0 steht (Schweissen: Zuenden, dann Funken) -> 1 knirscht und wankt (2,8 s) -> 2 kippt -> 3 liegt -> 4 aufrichten -> 0
// Sie weichen nicht aus: wer im Weg steht, wird beiseitegeschoben (shoveFor) - sie gehen durch einen hindurch. Ihr Blick gilt nur
// der Arbeit. Schaden nur im abgesperrten Feld (Absperrband am Boden) und nur, wenn die Maschine laeuft bzw. aufschlaegt - jede
// Gefahr kuendigt sich an. Vom eigenen Geruest getroffen, vergeht ein Arbeiter in Rauch; die anderen bemerken es nicht
// (Remodeling Co: sie nehmen verletzte Kollegen nicht wahr), nach einer Weile tritt ein neuer aus dem Dunkel.
// Koop: der Host rechnet; die Puppen bekommen Aufgabe (state), Phase (strike), Ziel (tilt: Hoehe / 2,5 m, lean: seitlich),
// setzen/heben (alert), Lichtbogen bzw. Beben (shiver), Vergehen (fly), Leuchten (glow); Mauer: reach, growl, jaw, alert, shiver;
// Geruest: tilt, shiver.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"
#include "props/prop_library.hpp"

namespace lim::game {

namespace {
namespace ob = anim::ob;
constexpr int kJobs = 5;
enum Job { J_MASON, J_HAMMER, J_BREAKER, J_WELDER, J_CARRIER };
// abgesperrte Flaechen (Ortskoordinaten: Mauer x = Normale zur Arbeitsseite, y entlang; Geruest x = Kipprichtung)
constexpr float kWallX0 = -1.4f, kWallX1 = 1.6f, kWallY = 2.35f;
constexpr float kTowX0 = -0.8f, kTowX1 = 4.25f, kTowY = 1.05f;
constexpr float kPalX = 0.95f, kPalY = 1.85f, kPalTop = 0.394f;  // Palette mit Steinen (Oberkante des obersten Steins unten)
enum WallPhase { WP_BUILD, WP_INSPECT, WP_WARN, WP_PRY, WP_JACK, WP_CLEAR };
enum TowerPhase { TP_STAND, TP_CREAK, TP_FALL, TP_DOWN, TP_LIFT };

struct Site {
    bool ok = false;
    vec3 wallC, towerC, heap, heapLook;
    float wallYaw = 0.0f, towerYaw = 0.0f;
    u16 wallId = 0, towerId = 0;
    std::array<u16, kJobs> worker{};
    std::array<double, kJobs> respawnT{};
    // Mauer
    int wallPhase = WP_BUILD, laid = 0, broken = 0;
    double wallT = 0.0;
    float topple = 0.0f, debris = 0.0f, beacon = 0.0f;
    bool jackOn = false, pryStarted = false;
    double jackT = 0.0, pryT = 0.0;
    // Geruest
    int towerPhase = TP_STAND;
    double towerT = 0.0, standDur = 70.0;
    float tilt = 0.0f, tiltV = 0.0f, creak = 0.0f, towerBeacon = 0.0f;
    bool weldOn = false, weldIgnite = false;
    double weldT = 0.0, weldDur = 4.0;
    vec3 sparkAt;
    std::array<double, 4> hitT{};  // je Spieler: naechster Schaden durch Funken / Presslufthammer frueher nicht
    NavField toHeap, toPallet;
    unsigned seed = 1;
};
struct WAg {
    int job = 0, step = 0;
    double t = 0.0, shoveT = 0.0;
    float act = 0.0f;
    bool carrying = false;
    vec3 prev;
    int lastEv = -1;
    int idx = 0;  // Maurer: der Stein, den er gerade setzt
};
template <class A>
WAg& wag(A& a) {
    if (!a.ext) a.ext = std::make_shared<WAg>();
    return *static_cast<WAg*>(a.ext.get());
}
float wrap01(float x) { return x - std::floor(x); }
vec3 wl(const Site& s, float x, float y, float z = 0.0f) {  // Mauer -> Welt
    const vec3 F = dirYaw(s.wallYaw), L{-F.y, F.x, 0.0f};
    return s.wallC + F * x + L * y + vec3{0, 0, z};
}
vec3 tl(const Site& s, float x, float y, float z = 0.0f) {  // Geruest -> Welt
    const vec3 F = dirYaw(s.towerYaw), L{-F.y, F.x, 0.0f};
    return s.towerC + F * x + L * y + vec3{0, 0, z};
}
vec3 toLocal(vec3 p, vec3 c, float yaw) {
    const vec3 F = dirYaw(yaw), L{-F.y, F.x, 0.0f}, d = p - c;
    return {dot(d, F), dot(d, L), d.z};
}
bool inRect(vec3 lp, float x0, float x1, float y0, float y1) { return lp.x >= x0 && lp.x <= x1 && lp.y >= y0 && lp.y <= y1; }
float slotY(int k, int course) { return -ob::kWallL * 0.5f + ((float)k + 0.5f) * ob::kBlockL + ((course & 1) ? ob::kBlockL * 0.5f : 0.0f); }
float distanceForTask(int task) {
    switch (task) {
        case ob::K_LAY: return 0.55f;
        case ob::K_HAMMER: return 0.58f;
        case ob::K_WELD: return 0.52f;
        case ob::K_JACK: return 0.42f;
        case ob::K_PRY: return 0.62f;
        case ob::K_LIFT: return 0.38f;
        case ob::K_INSPECT: return 1.3f;
        default: return 1.0f;
    }
}
}  // namespace

// --- Abfragen ------------------------------------------------------------------------------------------------------------
Presence::OuroInfo Presence::ouroInfo() const {
    OuroInfo o;
    const Site* s = static_cast<const Site*>(ouroSite_.get());
    if (!s || !s->ok || !ouroPlaced_) return o;
    o.placed = true;
    o.wall = s->wallC, o.tower = s->towerC, o.wallYaw = s->wallYaw, o.towerYaw = s->towerYaw;
    auto box = [](vec3 (&out)[2], vec3 a, vec3 b, vec3 c, vec3 d) {
        out[0] = {std::min({a.x, b.x, c.x, d.x}), std::min({a.y, b.y, c.y, d.y}), a.z};
        out[1] = {std::max({a.x, b.x, c.x, d.x}), std::max({a.y, b.y, c.y, d.y}), a.z};
    };
    box(o.wallZone, wl(*s, kWallX0, -kWallY), wl(*s, kWallX1, -kWallY), wl(*s, kWallX0, kWallY), wl(*s, kWallX1, kWallY));
    box(o.towerZone, tl(*s, kTowX0, -kTowY), tl(*s, kTowX1, -kTowY), tl(*s, kTowX0, kTowY), tl(*s, kTowX1, kTowY));
    o.wallPhase = s->wallPhase, o.towerPhase = s->towerPhase;
    for (const auto& a : agents_)
        if (a->role == Role::Ouroboros && !a->showcase && a->variant <= 1) ++o.workers;
    o.sparkAt = s->weldOn ? s->sparkAt : vec3{};
    o.laid = s->laid, o.broken = s->broken;
    for (const auto& a : agents_)
        if (a->role == Role::Ouroboros && !a->showcase && a->variant <= 1 && a->ext) {
            const int j = static_cast<const WAg*>(a->ext.get())->job;
            if (j < 0 || j >= kJobs) continue;
            o.has[(size_t)j] = true, o.pos[(size_t)j] = a->pos, o.head[(size_t)j] = a->cre.headPos();
            o.face[(size_t)j] = a->cre.pose().up[(size_t)ob::B_HEAD], o.task[(size_t)j] = a->state, o.speed[(size_t)j] = a->cre.speed();
        }
    return o;
}

vec3 Presence::shoveFor(int target, vec3 feet) const {
    (void)target;
    vec3 sum{};
    for (const auto& ap : agents_) {
        const Agent& a = *ap;
        if (a.role != Role::Ouroboros || a.variant > 1 || a.showcase || a.state == ob::K_DISSOLVE) continue;
        const float sp = a.cre.speed();
        if (sp < 0.2f || std::fabs(feet.z - a.pos.z) > 1.2f) continue;
        const vec3 d = flat(feet - a.pos);
        const float dist = length(d), rad = a.variant == 1 ? 0.5f : 0.42f;
        if (dist > rad) continue;
        // beiseite: quer zur Gehrichtung, vom Weg weg, und ein wenig mit
        const vec3 fw = dirYaw(a.yaw), side{-fw.y, fw.x, 0.0f};
        const float sg = dot(d, side) >= 0.0f ? 1.0f : -1.0f;
        const float k = 1.0f - dist / rad;
        sum = sum + (side * (sg * 1.6f) + fw * 0.7f) * (0.35f + 0.65f * k) * std::min(1.0f, sp);
    }
    return sum;
}

void Presence::ouroLights(std::vector<std::pair<vec3, vec3>>& out, vec3 near) const {
    for (const auto& ap : agents_) {
        const Agent& a = *ap;
        if (a.role != Role::Ouroboros || length(a.pos - near) > 40.0f) continue;
        if (a.variant <= 1 && a.state == ob::K_WELD && a.shiver > 0.1f && a.fly < 0.05f) {  // Lichtbogen: kalt, flackernd
            const float fl = 0.6f + 0.8f * hash01((u32)(time_ * 37.0) * 2654435761u + a.netId);
            out.push_back({a.cre.pose().j[(size_t)ob::toolJ(ob::T_TORCH, 1)] + vec3{0, 0, 0.04f}, vec3{0.72f, 0.84f, 1.0f} * (2.6f * fl)});
        } else if (a.variant == 2 && a.glow > 0.05f) {  // Warnleuchte der Mauer (Ecke des Absperrbands)
            const vec3 F = dirYaw(a.yaw), L{-F.y, F.x, 0.0f};
            out.push_back({a.pos + F * kWallX1 - L * kWallY + vec3{0, 0, 1.08f}, vec3{1.0f, 0.55f, 0.1f} * (1.6f * a.glow)});
        } else if (a.variant == 3 && a.glow > 0.05f) {
            const vec3 F = dirYaw(a.yaw), L{-F.y, F.x, 0.0f};
            out.push_back({a.pos + F * kTowX0 + L * kTowY + vec3{0, 0, 1.08f}, vec3{1.0f, 0.55f, 0.1f} * (1.6f * a.glow)});
        }
    }
}

bool Presence::ouroSet(const std::string& key, double v) {
    Site* s = static_cast<Site*>(ouroSite_.get());
    if (key == "quick") return ouroQuick_ = std::clamp(v, 0.1, 20.0), true;
    if (!s || !s->ok) return false;
    if (key == "wall") {
        s->wallPhase = std::clamp((int)v, 0, (int)WP_CLEAR), s->wallT = 0.0, s->topple = 0.0f, s->jackOn = false, s->pryStarted = false;
        if (s->wallPhase >= WP_INSPECT && s->wallPhase <= WP_PRY) s->laid = ob::kCourses * ob::kLay, s->broken = 0;
        if (s->wallPhase == WP_JACK) s->laid = ob::kCourses * ob::kLay, s->broken = ob::kCourses - 3;
        if (s->wallPhase == WP_BUILD) s->laid = 0, s->broken = 0, s->debris = 0.0f;
    } else if (key == "fill") {
        s->laid = std::clamp((int)std::lround(v * ob::kCourses * ob::kLay), 0, ob::kCourses * ob::kLay), s->broken = 0;
    } else if (key == "tower") {
        s->towerPhase = std::clamp((int)v, 0, (int)TP_LIFT), s->towerT = 0.0;
        if (s->towerPhase == TP_STAND) s->tilt = 0.0f, s->tiltV = 0.0f, s->standDur = 600.0;
        if (s->towerPhase == TP_DOWN || s->towerPhase == TP_LIFT) s->tilt = 1.0f, s->tiltV = 0.0f;
        if (s->towerPhase == TP_FALL) s->tilt = 0.04f, s->tiltV = 0.2f;
    } else if (key == "weld") {  // 1: gleich zuenden, 0: aus
        s->weldOn = false, s->weldIgnite = v > 0.5, s->weldT = 0.0;
    } else if (key == "kill") {  // Arbeiter mit dieser Aufgabe vergeht (Bilder)
        for (auto& a : agents_)
            if (a->role == Role::Ouroboros && a->variant <= 1 && !a->showcase && wag(*a).job == (int)v) a->state = ob::K_DISSOLVE, a->stateT = 0.0;
    } else {
        return false;
    }
    return true;
}

// --- Baustelle anlegen -----------------------------------------------------------------------------------------------------
bool Presence::spawnOuroboros(const Context& c, std::vector<Event>& out, bool forced) {
    world::KinDef K;
    K.groupMin = K.groupMax = 5, K.rooms = {"ouroboros"};
    if (c.kin && c.kin[world::KIN_OUROBOROS].on) K = c.kin[world::KIN_OUROBOROS];
    else if (!forced) return false;
    if (K.rooms.empty()) K.rooms = {"ouroboros"};
    world::World& w = *c.world;
    const vec3 eye = c.view.eye;
    // der Sektor: eine geladene Kachel des Raumtyps in bis zu 130 m
    const world::Room* room = nullptr;
    for (int r = 0; r <= 130 && !room; r += 2)
        for (int k = 0; k < std::max(1, r * 3) && !room; ++k) {
            const float a = (float)k / (float)std::max(1, r * 3) * 2.0f * kPiF;
            const i64 x = ifloor(eye.x + std::cos(a) * (float)r), y = ifloor(eye.y + std::sin(a) * (float)r);
            const world::Tile* t = w.tileIfLoaded(x, y);
            if (!t || !t->room || !t->room->type) continue;
            for (const std::string& key : K.rooms)
                if (t->room->type->key == key) room = t->room;
        }
    if (!room) return false;
    // freie Flaeche fuer ein Rechteck (Ortskoordinaten um c, Drehung yaw): offene Kacheln desselben Raums, gleicher Boden, frei
    // von Ausstattung (Grundriss der Modelle) und mit Abstand zu den Waenden
    struct PropFoot {
        vec3 p;
        float r;
        std::string name;
    };
    std::vector<PropFoot> props;
    for (i64 sy = room->y0; sy < room->y1 + 96; sy += 96)
        for (i64 sx = room->x0; sx < room->x1 + 96; sx += 96)
            if (const world::Sector* sec = w.sectorIfReady((double)std::min(sx, room->x1 - 1), (double)std::min(sy, room->y1 - 1)))
                for (const auto& pr : sec->props) {
                    if (!pr.model || pr.x < room->x0 - 1 || pr.x > room->x1 + 1 || pr.y < room->y0 - 1 || pr.y > room->y1 + 1) continue;
                    const vec3 ext = (pr.model->bounds.hi - pr.model->bounds.lo) * 0.5f;
                    PropFoot f{{pr.x, pr.y, pr.z}, std::hypot(ext.x * pr.sx, ext.y * pr.sy), {}};
                    int var = 0;
                    props::modelName(pr.model, f.name, var);
                    bool dup = false;
                    for (const auto& q : props) dup = dup || (hdist(q.p, f.p) < 0.01f && q.name == f.name);
                    if (!dup) props.push_back(std::move(f));
                }
    const float floorZ = (float)room->floor;
    auto free = [&](vec3 ctr, float yaw, float x0, float x1, float y0, float y1, float margin) {
        const vec3 F = dirYaw(yaw), L{-F.y, F.x, 0.0f};
        for (float x = x0 - margin; x <= x1 + margin + 1e-3f; x += 0.5f)
            for (float y = y0 - margin; y <= y1 + margin + 1e-3f; y += 0.5f) {
                const vec3 p = ctr + F * x + L * y;
                const world::Tile* t = w.tileIfLoaded(ifloor(p.x), ifloor(p.y));
                if (!t || !t->open() || t->room != room || std::fabs(t->floor - floorZ) > 0.05f || t->clearance() < 3.6f) return false;
                if (p.x < room->x0 + 1.2f || p.x > room->x1 - 1.2f || p.y < room->y0 + 1.2f || p.y > room->y1 - 1.2f) return false;
            }
        for (const auto& f : props) {
            const vec3 lp = toLocal(f.p, ctr, yaw);
            const float r = std::min(f.r, 1.6f) + 0.15f;
            if (lp.x > x0 - r && lp.x < x1 + r && lp.y > y0 - r && lp.y < y1 + r) return false;
        }
        return true;
    };
    auto lightNear = [&](vec3 p) {
        float best = 99.0f;
        for (const auto& f : props)
            if (f.name == "work_light") best = std::min(best, hdist(f.p, p));
        return best;
    };
    // feste Reihenfolge, kein Zufall: die Baustelle liegt bei jedem Lauf und bei Host und Gast gleich
    float bestW = 1e9f;
    vec3 wallC;
    float wallYaw = 0.0f;
    const vec3 rc{(float)(room->x0 + room->x1) * 0.5f, (float)(room->y0 + room->y1) * 0.5f, floorZ};
    for (float y = (float)room->y0 + 3.0f; y < (float)room->y1 - 3.0f; y += 0.5f)
        for (float x = (float)room->x0 + 3.0f; x < (float)room->x1 - 3.0f; x += 0.5f)
            for (int q = 0; q < 4; ++q) {
                const float yaw = (float)q * 0.5f * kPiF;
                const vec3 p{x, y, floorZ};
                if (!free(p, yaw, kWallX0, kWallX1, -kWallY, kWallY, 0.4f)) continue;
                const float ln = lightNear(p);
                const float score = std::fabs(ln - 4.5f) * 1.5f + hdist(p, rc) * 0.25f + (float)q * 0.01f;
                if (score < bestW) bestW = score, wallC = p, wallYaw = yaw;
            }
    if (bestW > 1e8f) {
        log::info("Praesenz: Ouroboros - keine freie Flaeche fuer die Mauer");
        return false;
    }
    float bestT = 1e9f;
    vec3 towerC;
    float towerYaw = 0.0f;
    for (float y = (float)room->y0 + 2.0f; y < (float)room->y1 - 2.0f; y += 0.5f)
        for (float x = (float)room->x0 + 2.0f; x < (float)room->x1 - 2.0f; x += 0.5f)
            for (int q = 0; q < 4; ++q) {
                const float yaw = (float)q * 0.5f * kPiF;
                const vec3 p{x, y, floorZ};
                const float dw = hdist(p, wallC);
                if (dw < 6.0f || dw > 14.0f) continue;
                // nicht in die Mauerflaeche (Ecken des Geruestfelds pruefen)
                bool clash = false;
                for (float lx : {kTowX0 - 0.6f, kTowX1 + 0.3f})
                    for (float ly : {-kTowY - 0.3f, kTowY + 0.3f}) {
                        const vec3 lp = toLocal(p + dirYaw(yaw) * lx + vec3{-std::sin(yaw), std::cos(yaw), 0.0f} * ly, wallC, wallYaw);
                        clash = clash || inRect(lp, kWallX0 - 1.0f, kWallX1 + 1.0f, -kWallY - 1.0f, kWallY + 1.0f);
                    }
                if (clash || !free(p, yaw, kTowX0 - 0.5f, kTowX1, -kTowY, kTowY, 0.3f)) continue;
                const float score = std::fabs(lightNear(p) - 4.0f) + std::fabs(dw - 8.5f) * 0.6f + (float)q * 0.01f;
                if (score < bestT) bestT = score, towerC = p, towerYaw = yaw;
            }
    if (bestT > 1e8f) {
        log::info("Praesenz: Ouroboros - keine freie Flaeche fuer das Geruest");
        return false;
    }
    // Lager, aus dem der Traeger holt: eine Plane, Zementsaecke oder Stahl, 5-16 m von der Mauer
    vec3 heap = wallC + dirYaw(wallYaw) * 3.0f + vec3{-std::sin(wallYaw), std::cos(wallYaw), 0.0f} * 6.0f;
    {
        float best = 1e9f;
        for (const char* want : {"tarp_pile", "cement_bags", "rebar_bundle"})
            for (const auto& f : props) {
                if (f.name != want) continue;
                const float d = hdist(f.p, wallC);
                if (d < 5.0f || d > 16.0f) continue;
                const float sc = std::fabs(d - 9.0f) + (std::string(want) == "tarp_pile" ? 0.0f : 2.0f);
                if (sc < best) best = sc, heap = f.p;
            }
    }
    // nicht vor den Augen: weder die Mauer noch das Geruest im Blick eines Spielers (ausser erzwungen)
    auto seen = [&](vec3 p) {
        auto one = [&](const Viewer& v) {
            const vec3 d = p - v.eye;
            const float len = length(d);
            return len < 70.0f && dot(d / std::max(len, 1e-3f), v.look) > 0.3f && (clearSight(w, v.eye, p) || len < 4.0f);
        };
        bool s = one(c.view);
        if (c.others)
            for (const Other& o : *c.others) s = s || one(o.view);
        return s;
    };
    if (!forced) {
        for (vec3 p : {wallC + vec3{0, 0, 1.0f}, towerC + vec3{0, 0, 1.6f}, heap + vec3{0, 0, 1.0f}, rc + vec3{0, 0, 1.5f}})
            if (seen(p)) return false;
    }
    // anlegen
    if (!ouroSite_) ouroSite_ = std::make_shared<Site>();
    Site& s = *static_cast<Site*>(ouroSite_.get());
    s = Site{};
    s.ok = true, s.wallC = wallC, s.wallYaw = wallYaw, s.towerC = towerC, s.towerYaw = towerYaw, s.heap = heap;
    s.seed = (unsigned)(std::lround(wallC.x * 7.0f) * 73856093 ^ std::lround(wallC.y * 7.0f) * 19349663);
    s.laid = 14 + (int)(hash01(s.seed) * 16.0f);  // man kommt mitten in die Arbeit
    s.standDur = 45.0 + 30.0 * hash01(s.seed + 7u);
    {
        const vec3 hp = heap;
        s.toHeap.build(w, ifloor(hp.x), ifloor(hp.y), 1.9f);
        const vec3 pp = wl(s, kWallX1 + 0.6f, kWallY + 0.6f);
        s.toPallet.build(w, ifloor(pp.x), ifloor(pp.y), 1.9f);
    }
    auto make = [&](int variant, vec3 p, float yaw, int job) -> Agent& {
        auto a = std::make_unique<Agent>();
        a->role = Role::Ouroboros;
        a->ent = Entity::Ouroboros;
        a->variant = variant;
        a->netId = nextNetId_++;
        a->noShadow = false;
        WAg& g = wag(*a);
        g.job = job;
        g.prev = p;
        a->initBody(p, yaw, (unsigned)(s.seed + (unsigned)job * 977u + (unsigned)variant * 31u));
        Agent& ref = *a;
        agents_.push_back(std::move(a));
        prepOuroboros(ref, c);
        anim::CreatureAnimator::Input in;
        in.pos = p, in.yaw = yaw;
        ref.cre.update(0.0, in);
        ref.rig.setCreaturePose(ref.cre.pose());
        return ref;
    };
    const float wy = wallYaw + kPiF;  // die Arbeiter sehen zur Mauer
    s.wallId = make(2, wallC, wallYaw, -1).netId;
    s.towerId = make(3, towerC, towerYaw, -1).netId;
    const int n = std::clamp(K.groupMax, 1, kJobs);
    const vec3 start[kJobs] = {wl(s, kPalX, kPalY - 0.55f), wl(s, 0.58f, slotY(1, 0)), wl(s, 1.25f, -2.0f),
                               tl(s, -0.45f - distanceForTask(ob::K_WELD), 0.3f), heap + dirYaw(wallYaw) * 0.8f};
    const float yaws[kJobs] = {wallYaw + 0.5f * kPiF, wy, wy + 0.4f, towerYaw, wallYaw};
    for (int j = 0; j < n; ++j) s.worker[(size_t)j] = make(j == J_BREAKER ? 1 : 0, start[j], yaws[j], j).netId;
    ouroPlaced_ = true;
    out.push_back({Event::Appear, Role::Ouroboros, wallC});
    log::info("Praesenz: Ouroboros-Baustelle - Mauer bei ({:.1f}, {:.1f}), Geruest bei ({:.1f}, {:.1f}), Lager ({:.1f}, {:.1f}), {} Arbeiter, {:.0f} m",
              wallC.x, wallC.y, towerC.x, towerC.y, heap.x, heap.y, n, hdist(wallC, eye));
    return true;
}

// --- Regie: die Baustelle als Ganzes (Mauer, Geruest, Gefahren, Stimmen, Ersatz) -----------------------------------------------
void Presence::regieOuroboros(double dtIn, const Context& c, std::vector<Event>& out) {
    if (!c.kin || !c.kin[world::KIN_OUROBOROS].on) {
        if (!ouroPlaced_) return;
    }
    Site* sp = static_cast<Site*>(ouroSite_.get());
    if (ouroPlaced_) {  // weg (Einstellung aus, Laden)? dann neu anlegen
        bool any = false;
        for (const auto& a : agents_) any = any || (a->role == Role::Ouroboros && !a->showcase && a->variant == 2);
        if (!any || !sp || !sp->ok) ouroPlaced_ = false;
    }
    if (!ouroPlaced_) {
        ouroTry_ -= dtIn;
        if (ouroTry_ > 0.0) return;
        ouroTry_ = 2.0;
        spawnOuroboros(c, out, false);
        return;
    }
    Site& s = *sp;
    const double dt = dtIn * ouroQuick_;
    const vec3 eye = c.view.eye;
    if (hdist(eye, s.wallC) > 90.0f && hdist(eye, s.towerC) > 90.0f) return;  // weit weg: die Baustelle ruht
    Agent* wallA = nullptr;
    Agent* towerA = nullptr;
    for (auto& a : agents_)
        if (a->role == Role::Ouroboros && !a->showcase) {
            if (a->netId == s.wallId) wallA = a.get();
            if (a->netId == s.towerId) towerA = a.get();
        }
    // Spieler (Ort, Kennung) fuer Gefahren
    struct Pl {
        vec3 feet;
        int id;
    };
    std::vector<Pl> pls;
    if (c.selfAlive) pls.push_back({{(float)c.player->x, (float)c.player->y, (float)c.player->z}, c.selfId});
    if (c.others)
        for (const Other& o : *c.others)
            if (o.player) pls.push_back({{(float)o.player->x, (float)o.player->y, (float)o.player->z}, o.id});
    auto hit = [&](const Pl& p, float dmg) { out.push_back({Event::Hit, Role::Ouroboros, p.feet + vec3{0, 0, 1.0f}, dmg, p.id}); };

    // ---------------- Mauer ----------------
    s.wallT += dt;
    const int total = ob::kCourses * ob::kLay;
    const float blink = (float)(std::fmod(time_, 0.8) < 0.4 ? 1.0 : 0.15);
    switch (s.wallPhase) {
        case WP_BUILD:
            s.beacon = 0.0f;
            if (s.laid >= total) s.wallPhase = WP_INSPECT, s.wallT = 0.0;
            break;
        case WP_INSPECT:  // alle halten inne - Stille
            if (s.wallT > 7.0) {
                s.wallPhase = WP_WARN, s.wallT = 0.0;
                out.push_back({Event::Warn, Role::Ouroboros, wl(s, kWallX1, -kWallY, 1.0f), 0.0f});
            }
            break;
        case WP_WARN:
            s.beacon = blink;
            if (s.wallT > 2.8) s.wallPhase = WP_PRY, s.wallT = 0.0, s.pryStarted = false, s.topple = 0.0f;
            break;
        case WP_PRY: {  // die oberen Lagen: ansetzen (0,9 s), hebeln (Knirschen), die Lage kippt nach hinten und schlaegt auf
            s.beacon = blink;
            const int topC = ob::kCourses - s.broken - 1;
            if (topC < 3) {
                s.wallPhase = WP_JACK, s.wallT = 0.0, s.jackOn = false;
                out.push_back({Event::Warn, Role::Ouroboros, wl(s, 0.4f, 0.0f, 0.5f), 0.5f});  // Kompressor laeuft an
                break;
            }
            if (!s.pryStarted && s.wallT > 0.9) {
                s.pryStarted = true, s.pryT = 0.0;
                out.push_back({Event::Work, Role::Ouroboros, wl(s, 0.12f, 0.0f, (float)topC * ob::kCourseH), 0.75f});
            }
            if (s.pryStarted) {
                s.pryT += dt;
                const float before = s.topple;
                s.topple = (float)std::clamp((s.pryT - 1.1) / 1.6, 0.0, 1.0);
                if (before < 0.75f && s.topple >= 0.75f) {  // Aufschlag hinter der Mauer
                    out.push_back({Event::Crash, Role::Ouroboros, wl(s, -0.7f, 0.0f, 0.1f), 0.5f});
                    for (const Pl& p : pls) {
                        const vec3 lp = toLocal(p.feet, s.wallC, s.wallYaw);
                        if (inRect(lp, kWallX0, -0.1f, -1.35f, 1.35f) && std::fabs(lp.z) < 1.0f) hit(p, 18.0f);
                    }
                }
                if (s.topple >= 1.0f) ++s.broken, s.topple = 0.0f, s.pryStarted = false, s.wallT = 0.0;
            }
            break;
        }
        case WP_JACK: {  // Presslufthammer: Kompressor (1,6 s), dann je Lage 3,2 s Stoesse; Brocken fliegen im abgesperrten Feld
            s.beacon = blink;
            const int topC = ob::kCourses - s.broken - 1;
            if (topC < 0) {
                s.wallPhase = WP_CLEAR, s.wallT = 0.0, s.jackOn = false;
                break;
            }
            if (!s.jackOn && s.wallT > 1.6) s.jackOn = true, s.jackT = 0.0;
            if (s.jackOn) {
                const double before = s.jackT;
                s.jackT += dt;
                if (std::floor(before / 0.55) != std::floor(s.jackT / 0.55) || before == 0.0)
                    out.push_back({Event::Jackhammer, Role::Ouroboros, wl(s, 0.0f, 0.0f, (float)(topC + 1) * ob::kCourseH), 1.0f});
                for (size_t i = 0; i < pls.size(); ++i) {
                    const Pl& p = pls[i];
                    const vec3 lp = toLocal(p.feet, s.wallC, s.wallYaw);
                    const size_t k = (size_t)std::clamp(p.id, 0, 3);
                    if (inRect(lp, kWallX0, kWallX1, -kWallY, kWallY) && std::fabs(lp.z) < 1.0f && time_ >= s.hitT[k])
                        hit(p, 3.0f), s.hitT[k] = time_ + 0.35;
                }
                if (s.jackT > 3.2) {  // die Lage bricht
                    ++s.broken, s.jackT = 0.0, s.debris = 0.001f;
                    out.push_back({Event::Work, Role::Ouroboros, wl(s, 0.0f, 0.0f, (float)topC * ob::kCourseH), 1.0f});
                    if (topC == 0) s.jackOn = false, s.wallPhase = WP_CLEAR, s.wallT = 0.0;
                }
            }
            break;
        }
        case WP_CLEAR:
            s.beacon = 0.0f;
            if (s.wallT > 9.0) s.wallPhase = WP_BUILD, s.wallT = 0.0, s.laid = 0, s.broken = 0, s.debris = 0.0f;
            break;
        default: break;
    }
    if (s.debris > 0.0f) s.debris = std::min(1.0f, s.debris + (float)dt / 1.6f);

    // ---------------- Geruest ----------------
    s.towerT += dt;
    switch (s.towerPhase) {
        case TP_STAND: {
            s.creak = 0.0f, s.towerBeacon = 0.0f, s.tilt = 0.0f, s.tiltV = 0.0f;
            // Schweissen in Stoessen: Zuenden (Ankuendigung, 1 s), dann Lichtbogen 5-9 s, Pause 3-6 s
            s.weldT -= dt;
            if (s.weldT <= 0.0) {
                if (s.weldOn) s.weldOn = false, s.weldT = 3.0 + 3.0 * hash01(s.seed + (u32)(time_ * 10.0));
                else if (s.weldIgnite) s.weldIgnite = false, s.weldOn = true, s.weldT = 5.0 + 4.0 * hash01(s.seed + (u32)(time_ * 13.0));
                else {
                    s.weldIgnite = true, s.weldT = 1.0;
                    out.push_back({Event::Weld, Role::Ouroboros, tl(s, -0.45f, 0.3f, 0.9f), 0.0f});
                }
            }
            if (s.towerT > s.standDur) {
                s.towerPhase = TP_CREAK, s.towerT = 0.0;
                out.push_back({Event::Creak, Role::Ouroboros, tl(s, 0.0f, 0.0f, 2.4f)});
                out.push_back({Event::Warn, Role::Ouroboros, tl(s, kTowX0, kTowY, 1.0f), 1.0f});
            }
            break;
        }
        case TP_CREAK:  // knirscht und wankt - in 2,8 s kippt es
            s.creak = std::min(1.0f, (float)s.towerT / 1.2f);
            s.towerBeacon = blink;
            if (s.towerT > 2.8) s.towerPhase = TP_FALL, s.towerT = 0.0, s.tilt = 0.03f, s.tiltV = 0.15f;
            break;
        case TP_FALL: {  // Kippen um die Kante: d2th = 3g/(2L) sin(th)
            s.towerBeacon = blink, s.creak = 0.3f;
            const float th = s.tilt * 0.5f * kPiF;
            const float acc = 1.5f * 9.81f / ob::kTowerH * std::sin(std::max(th, 0.02f)) / (0.5f * kPiF);
            s.tiltV += acc * (float)dt;
            s.tilt += s.tiltV * (float)dt;
            if (s.tilt >= 1.0f) {
                s.tilt = 1.0f, s.tiltV = -0.12f * s.tiltV;  // federt einmal kurz
                s.towerPhase = TP_DOWN, s.towerT = 0.0;
                out.push_back({Event::Crash, Role::Ouroboros, tl(s, 2.2f, 0.0f, 0.3f), 1.0f});
                for (const Pl& p : pls) {
                    const vec3 lp = toLocal(p.feet, s.towerC, s.towerYaw);
                    if (inRect(lp, 0.15f, 3.95f, -0.95f, 0.95f) && std::fabs(lp.z) < 1.0f) hit(p, 30.0f);
                }
                // Arbeiter darunter vergehen - die anderen bemerken es nicht
                for (auto& a : agents_)
                    if (a->role == Role::Ouroboros && !a->showcase && a->variant <= 1 && a->state != ob::K_DISSOLVE) {
                        const vec3 lp = toLocal(a->pos, s.towerC, s.towerYaw);
                        if (inRect(lp, 0.3f, 3.85f, -0.85f, 0.85f)) a->state = ob::K_DISSOLVE, a->stateT = 0.0;
                    }
            }
            break;
        }
        case TP_DOWN:
            s.creak = 0.0f, s.towerBeacon = 0.0f;
            if (s.tiltV != 0.0f) {  // nachfedern
                s.tilt += s.tiltV * (float)dt;
                s.tiltV += 9.0f * (float)dt;
                if (s.tilt >= 1.0f) s.tilt = 1.0f, s.tiltV = 0.0f;
            }
            if (s.towerT > 5.0) s.towerPhase = TP_LIFT, s.towerT = 0.0;
            break;
        case TP_LIFT:  // zwei Arbeiter richten es auf (9 s); das Aufrichten wartet, bis beide da sind
            if (s.towerT > 2.5) s.tilt = std::max(0.0f, s.tilt - (float)dt / 8.0f);
            if (s.tilt <= 0.0f) {
                s.towerPhase = TP_STAND, s.towerT = 0.0, s.standDur = 50.0 + 35.0 * hash01(s.seed + (u32)time_);
                out.push_back({Event::Work, Role::Ouroboros, tl(s, 0.45f, 0.0f, 0.1f), 0.75f});
            }
            break;
        default: break;
    }
    if (s.towerPhase != TP_STAND) s.weldOn = s.weldIgnite = false;
    s.sparkAt = tl(s, -0.6f, 0.3f, 0.0f);
    if (s.weldOn)  // Funken treffen den Boden vor dem Brenner (im abgesperrten Feld)
        for (const Pl& p : pls) {
            const vec3 lp = toLocal(p.feet, s.towerC, s.towerYaw);
            const size_t k = (size_t)std::clamp(p.id, 0, 3);
            if (hdist(p.feet, s.sparkAt) < 0.95f && inRect(lp, kTowX0, kTowX1, -kTowY, kTowY) && time_ >= s.hitT[k]) hit(p, 2.0f), s.hitT[k] = time_ + 0.5;
        }
    // Kanaele der Mauer und des Geruests
    if (wallA) {
        wallA->reach = (float)s.laid / (float)total, wallA->growl = (float)s.broken / (float)ob::kCourses;
        wallA->jaw = s.wallPhase == WP_PRY ? s.topple : 0.0f, wallA->alert = s.debris, wallA->shiver = s.jackOn ? 1.0f : 0.0f;
        wallA->glow = s.beacon, wallA->state = s.wallPhase;
    }
    if (towerA) towerA->tilt = s.tilt, towerA->shiver = s.creak, towerA->glow = s.towerBeacon, towerA->state = s.towerPhase;

    // ---------------- Stimmen ----------------
    ouroMurmurT_ -= dt;
    if (ouroMurmurT_ <= 0.0) {
        ouroMurmurT_ = 7.0 + 9.0 * hash01(s.seed + (u32)(time_ * 3.0));
        std::vector<Agent*> ws;
        for (auto& a : agents_)
            if (a->role == Role::Ouroboros && !a->showcase && a->variant <= 1 && a->state != ob::K_DISSOLVE) ws.push_back(a.get());
        if (!ws.empty() && s.wallPhase != WP_INSPECT) {
            Agent* a = ws[(size_t)(hash01(s.seed * 3u + (u32)(time_ * 7.0)) * (float)ws.size()) % ws.size()];
            out.push_back({Event::Murmur, Role::Ouroboros, a->cre.headPos(), 0.3f});
        }
    }
    // ---------------- Ersatz: wer vergangen ist, wird nach einer Weile ersetzt (aus dem Dunkel, ausser Sicht) ----------------
    for (int j = 0; j < kJobs; ++j) {
        if (s.worker[(size_t)j] == 0) continue;
        bool alive = false;
        for (const auto& a : agents_) alive = alive || (a->role == Role::Ouroboros && a->netId == s.worker[(size_t)j]);
        if (alive) continue;
        s.respawnT[(size_t)j] -= dt;
        if (s.respawnT[(size_t)j] > 0.0) continue;
        // ein Platz im Sektor ausser Sicht, moeglichst dunkel (Rand)
        vec3 best;
        float bs = 1e9f;
        const vec3 F = dirYaw(s.wallYaw), L{-F.y, F.x, 0.0f};
        for (int k = 0; k < 24; ++k) {
            const float a = (float)k / 24.0f * 2.0f * kPiF;
            const vec3 p = s.wallC + (F * std::cos(a) + L * std::sin(a)) * 8.0f;
            const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
            if (!t || !t->open() || !t->room || !t->room->type || t->room->type->key != "ouroboros") continue;
            const vec3 d = p + vec3{0, 0, 1.2f} - eye;
            if (length(d) < 70.0f && dot(nrmF(d, {1, 0, 0}), c.view.look) > 0.2f && clearSight(*c.world, eye, p + vec3{0, 0, 1.2f})) continue;
            const float sc = (float)k * 0.01f + (t->light < 0.2f ? 0.0f : 1.0f);
            if (sc < bs) bs = sc, best = p;
        }
        if (bs > 1e8f) {
            s.respawnT[(size_t)j] = 3.0;
            continue;
        }
        auto a = std::make_unique<Agent>();
        a->role = Role::Ouroboros, a->ent = Entity::Ouroboros, a->variant = j == J_BREAKER ? 1 : 0, a->netId = nextNetId_++;
        wag(*a).job = j, wag(*a).prev = best;
        a->state = ob::K_EMERGE, a->fly = 1.0f;
        a->initBody(best, std::atan2(s.wallC.y - best.y, s.wallC.x - best.x), s.seed + (unsigned)j * 131u + (unsigned)time_);
        s.worker[(size_t)j] = a->netId;
        agents_.push_back(std::move(a));
        log::info("Praesenz: Ouroboros - ein neuer Arbeiter ({}) tritt aus dem Dunkel bei ({:.1f}, {:.1f})", j, best.x, best.y);
    }
}

// Kanaele -> Animation (Host, Puppe, Vorfuehrung)
void Presence::prepOuroboros(Agent& a, const Context& c) {
    (void)c;
    if (a.showcase && a.variant <= 1) {  // Vorfuehrung: die Aufgabe in Schleife, das Ziel vor dem Arbeiter
        const double t = a.age;
        switch (a.state) {
            case ob::K_LAY: a.strike = (float)wrap01((float)(t / 2.0)), a.alert = std::fmod(t, 4.0) < 2.0 ? 0.0f : 1.0f, a.tilt = a.alert < 0.5f ? 0.16f : 0.3f; break;
            case ob::K_HAMMER: a.strike = (float)wrap01((float)(t / 0.85)), a.tilt = 0.42f; break;
            case ob::K_WELD: a.shiver = 1.0f, a.tilt = 0.36f, a.glow = 1.0f; break;
            case ob::K_JACK: a.shiver = 1.0f, a.tilt = 0.2f; break;
            case ob::K_PRY: a.strike = (float)wrap01((float)(t / 2.8)), a.tilt = 0.4f; break;
            case ob::K_LIFT: a.tilt = 0.75f; break;
            case ob::K_INSPECT: a.tilt = 0.6f; break;
            case ob::K_CARRY: a.alert = 1.0f; break;
            case ob::K_DISSOLVE: a.fly = (float)std::min(1.0, wrap01((float)(t / 5.0)) * 1.4); break;
            default: break;
        }
    }
    a.cre.setExtra({(float)a.variant, (float)a.state});
    a.rig.setGlow(a.variant <= 1 ? (a.state == ob::K_WELD && a.shiver > 0.1f ? 1.0f + 0.6f * (float)hash01((u32)(time_ * 30.0) + a.netId) : 0.0f) : a.glow);
}

// --- ein Wesen der Baustelle -----------------------------------------------------------------------------------------------------
void Presence::thinkOuroboros(Agent& a, double dtIn, const Context& c, std::vector<Event>& out) {
    Site* sp = static_cast<Site*>(ouroSite_.get());
    const double dt = dtIn * ouroQuick_;
    a.stateT += dt;
    if (a.variant >= 2 || !sp || !sp->ok) {  // Mauer und Geruest: Kanaele setzt die Regie
        prepOuroboros(a, c);
        return;
    }
    Site& s = *sp;
    WAg& g = wag(a);
    const vec3 eye = c.view.eye;
    if (hdist(eye, a.pos) > 90.0f) return;
    // Licht: keine Reaktion im Verhalten - nur der Rauch (Animation)
    a.light = inBeam(c.view, *c.world, a.cre.headPos(), 20.0f) ? 1.0f : 0.0f;
    a.look = false;
    const float speed = a.variant == 1 ? 0.8f : 1.05f;
    auto faceTo = [&](float yaw, double k = 4.0) { a.yaw += wrapA(yaw - a.yaw) * (float)std::min(1.0, dt * k); };
    auto moveTo = [&](vec3 goal, float spd) {  // geradeaus, ohne auszuweichen; true: angekommen
        const vec3 d = flat(goal - a.pos);
        const float len = length(d);
        if (len < 0.05f) return true;
        const float st = std::min(len, spd * (float)dt);
        a.pos = a.pos + d / len * st;
        faceTo(std::atan2(d.y, d.x), 6.0);
        if (const world::Tile* t = c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)); t && t->open()) a.pos.z += (t->floor - a.pos.z) * (float)std::min(1.0, dt * 12.0);
        return false;
    };
    auto setTarget = [&](vec3 T) {  // Ziel der Arbeit -> Kanaele (Hoehe, seitlich) bei Blick a.yaw
        const vec3 F = dirYaw(a.yaw), L{-F.y, F.x, 0.0f};
        a.tilt = std::clamp((T.z - a.pos.z) / 2.5f, 0.0f, 1.2f);
        a.lean = std::clamp(dot(T - a.pos, L), -0.6f, 0.6f);
    };
    auto setTask = [&](int task) {
        if (a.state != task) a.state = task, g.t = 0.0;
    };
    // Schritte
    if (a.cre.stepped() && hdist(eye, a.pos) < 30.0f) out.push_back({Event::Step, Role::Ouroboros, a.cre.stepPos(), a.cre.stepWeight() * (a.variant == 1 ? 1.3f : 0.8f)});
    g.t += dt;
    // --- vergehen / erscheinen -----------------------------------------------------------------------------------
    if (a.state == ob::K_DISSOLVE) {
        if (g.t < dt * 1.5) out.push_back({Event::Dissolve, Role::Ouroboros, a.pos + vec3{0, 0, 1.0f}});
        a.fly = (float)std::min(1.0, g.t / 2.6);
        a.shiver = 0.0f, a.glow = 0.0f;
        if (a.fly >= 1.0f) {
            a.state = 9;  // Abgang
            s.respawnT[(size_t)g.job] = 20.0 + 10.0 * hash01(s.seed + (u32)time_);
        }
        prepOuroboros(a, c);
        return;
    }
    if (a.state == ob::K_EMERGE) {
        a.fly = std::max(0.0f, 1.0f - (float)(g.t / 2.4));
        if (a.fly <= 0.0f) setTask(ob::K_IDLE), g.step = 0;
        prepOuroboros(a, c);
        return;
    }
    a.fly = 0.0f;
    // --- durch den Spieler hindurch: Schub rechnet shoveFor (jeder Rechner fuer seinen Spieler); hier Klang und Stimme ---------
    g.shoveT -= dt;
    {
        const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
        if (g.shoveT <= 0.0 && hdist(feet, a.pos) < (a.variant == 1 ? 0.5f : 0.42f) && std::fabs(feet.z - a.pos.z) < 1.2f) {
            out.push_back({Event::Shove, Role::Ouroboros, a.cre.headPos(), 1.0f, c.selfId});
            out.push_back({Event::Murmur, Role::Ouroboros, a.cre.headPos(), 1.0f, c.selfId});
            g.shoveT = 2.5;
        }
    }
    const int total = ob::kCourses * ob::kLay;
    const bool inspect = s.wallPhase == WP_INSPECT;
    const float wy = s.wallYaw + kPiF;  // Blick zur Mauer (von der Arbeitsseite)
    a.shiver = 0.0f;
    // --- alle halten inne, sehen die fertige Mauer an ----------------------------------------------------------------
    if (inspect) {
        setTask(ob::K_INSPECT);
        const vec3 d = flat(wl(s, 0.0f, 0.0f) - a.pos);
        faceTo(std::atan2(d.y, d.x), 2.0);
        setTarget(wl(s, 0.0f, 0.0f, 1.2f));
        a.strike = 0.0f, a.alert = 0.0f;
        prepOuroboros(a, c);
        return;
    }
    switch (g.job) {
        case J_MASON: {
            if (s.wallPhase != WP_BUILD || s.laid >= total) {  // waehrend des Einreissens: steht an der Palette und sieht zu
                const vec3 at = wl(s, 1.3f, 1.25f);
                if (moveTo(at, speed)) {
                    setTask(ob::K_INSPECT);
                    const vec3 d = flat(wl(s, 0.0f, 0.0f) - a.pos);
                    faceTo(std::atan2(d.y, d.x));
                    setTarget(wl(s, 0.0f, 0.0f, 0.8f));
                } else {
                    setTask(ob::K_WALK);
                }
                g.step = 0;
                break;
            }
            if (g.step <= 1) g.idx = s.laid;  // der naechste Stein
            const int course = g.idx / ob::kLay, slot = g.idx % ob::kLay;
            const vec3 palStand = wl(s, kPalX, kPalY - 0.55f);
            const vec3 slotStand = wl(s, 0.55f, slotY(slot, course));
            const vec3 slotAt = wl(s, 0.0f, slotY(slot, course), (float)course * ob::kCourseH);
            switch (g.step) {
                case 0:  // zur Palette
                    setTask(ob::K_WALK);
                    if (moveTo(palStand, speed)) g.step = 1, g.t = 0.0, g.lastEv = -1;
                    break;
                case 1: {  // Stein heben
                    setTask(ob::K_LAY);
                    faceTo(s.wallYaw + 0.5f * kPiF);
                    a.alert = 0.0f;
                    a.strike = (float)std::min(1.0, g.t / 1.5);
                    setTarget(wl(s, kPalX, kPalY, kPalTop));
                    if (a.strike >= 0.45f && g.lastEv < 0) out.push_back({Event::Work, Role::Ouroboros, wl(s, kPalX, kPalY, 0.5f), 0.5f}), g.lastEv = 0;
                    if (a.strike >= 1.0f) g.step = 2;
                    break;
                }
                case 2:  // tragen
                    setTask(ob::K_CARRY);
                    a.alert = 1.0f;
                    if (moveTo(slotStand, speed)) g.step = 3, g.t = 0.0, g.lastEv = -1;
                    break;
                case 3: {  // setzen, nachklopfen
                    setTask(ob::K_LAY);
                    faceTo(wy);
                    a.alert = 1.0f;
                    a.strike = (float)std::min(1.0, g.t / 1.7);
                    setTarget(slotAt);
                    if (a.strike >= 0.55f && g.lastEv < 0) {
                        s.laid = std::max(s.laid, g.idx + 1), g.lastEv = 0;
                        out.push_back({Event::Work, Role::Ouroboros, slotAt, 0.5f});
                    }
                    if (a.strike >= 0.75f && g.lastEv == 0) out.push_back({Event::Work, Role::Ouroboros, slotAt + vec3{0, 0, ob::kCourseH}, 0.25f}), g.lastEv = 1;
                    if (a.strike >= 1.0f) g.step = 0;
                    break;
                }
                default: g.step = 0; break;
            }
            break;
        }
        case J_HAMMER: {
            if (s.wallPhase != WP_BUILD || s.laid < 2) {
                const vec3 at = wl(s, 1.2f, -1.55f);
                if (moveTo(at, speed)) {
                    setTask(ob::K_INSPECT);
                    const vec3 d = flat(wl(s, 0.0f, 0.0f) - a.pos);
                    faceTo(std::atan2(d.y, d.x));
                    setTarget(wl(s, 0.0f, 0.0f, 0.8f));
                } else {
                    setTask(ob::K_WALK);
                }
                break;
            }
            // zwei Steine hinter dem Maurer (seine Lage, sonst die darunter)
            const int idx = std::max(0, s.laid - 3);
            const int course = idx / ob::kLay, slot = idx % ob::kLay;
            const vec3 stand = wl(s, 0.58f, slotY(slot, course));
            if (!moveTo(stand, speed)) {
                setTask(ob::K_WALK);
                break;
            }
            setTask(ob::K_HAMMER);
            faceTo(wy);
            setTarget(wl(s, 0.0f, slotY(slot, course), (float)(course + 1) * ob::kCourseH));
            const float before = a.strike;
            a.strike = wrap01((float)(g.t / 0.85));
            if (before < 0.52f && a.strike >= 0.52f) out.push_back({Event::Work, Role::Ouroboros, wl(s, 0.0f, slotY(slot, course), (float)(course + 1) * ob::kCourseH), 0.0f});
            break;
        }
        case J_BREAKER: {
            const int topC = ob::kCourses - s.broken - 1;
            if (s.wallPhase == WP_PRY && topC >= 3) {
                const vec3 stand = wl(s, 0.12f + distanceForTask(ob::K_PRY), 0.0f);
                if (!moveTo(stand, speed)) {
                    setTask(ob::K_WALK);
                    break;
                }
                setTask(ob::K_PRY);
                faceTo(wy, 8.0);
                setTarget(wl(s, 0.12f, 0.0f, (float)topC * ob::kCourseH));
                a.strike = s.pryStarted ? (float)std::clamp(s.pryT / 2.7, 0.0, 1.0) : 0.0f;
            } else if (s.wallPhase == WP_JACK && topC >= 0) {
                const vec3 stand = wl(s, distanceForTask(ob::K_JACK), 0.0f);
                if (!moveTo(stand, speed)) {
                    setTask(ob::K_WALK);
                    break;
                }
                setTask(ob::K_JACK);
                faceTo(wy, 8.0);
                setTarget(wl(s, 0.0f, 0.0f, (float)(topC + 1) * ob::kCourseH));
                a.shiver = s.jackOn ? 1.0f : 0.0f;
            } else if (s.wallPhase == WP_WARN) {  // tritt an die Mauer
                const vec3 stand = wl(s, 0.12f + distanceForTask(ob::K_PRY), 0.0f);
                setTask(moveTo(stand, speed) ? ob::K_INSPECT : ob::K_WALK);
                if (a.state == ob::K_INSPECT) faceTo(wy), setTarget(wl(s, 0.0f, 0.0f, 1.5f));
            } else {  // wartet gebueckt an der Ecke
                const vec3 at = wl(s, 1.25f, -2.0f);
                if (moveTo(at, speed)) {
                    setTask(ob::K_IDLE);
                    const vec3 d = flat(wl(s, 0.0f, 0.0f) - a.pos);
                    faceTo(std::atan2(d.y, d.x), 1.5);
                } else {
                    setTask(ob::K_WALK);
                }
            }
            break;
        }
        case J_WELDER:
        case J_CARRIER: {
            // beide richten das Geruest auf, wenn es liegt
            if (s.towerPhase == TP_LIFT || s.towerPhase == TP_DOWN) {
                const float th = s.tilt * 0.5f * kPiF;
                const float side = g.job == J_WELDER ? 0.45f : -0.45f;
                const float r = 1.9f;
                const vec3 T = tl(s, ob::kTowerD * 0.5f + std::sin(th) * r, side * 0.7f, std::cos(th) * r + 0.45f * std::sin(th));
                const vec3 stand = tl(s, ob::kTowerD * 0.5f + std::sin(th) * r + distanceForTask(ob::K_LIFT), side);
                if (s.towerPhase == TP_DOWN) {  // hingehen
                    setTask(moveTo(stand, speed) ? ob::K_IDLE : ob::K_WALK);
                    if (a.state == ob::K_IDLE) faceTo(s.towerYaw + kPiF);
                } else {
                    moveTo(stand, speed * 1.6f);
                    setTask(ob::K_LIFT);
                    faceTo(s.towerYaw + kPiF, 8.0);
                    setTarget(T);
                }
                g.step = 0;
                break;
            }
            if (g.job == J_WELDER) {
                const vec3 stand = tl(s, -0.45f - distanceForTask(ob::K_WELD), 0.3f);
                if (s.towerPhase != TP_STAND && s.towerPhase != TP_CREAK) {  // es kippt / liegt: steht auf, sieht hin
                    setTask(ob::K_IDLE);
                    break;
                }
                if (!moveTo(stand, speed)) {
                    setTask(ob::K_WALK);
                    break;
                }
                setTask(ob::K_WELD);
                faceTo(s.towerYaw, 8.0);
                setTarget(tl(s, -0.45f, 0.3f, 0.9f));
                a.shiver = s.weldOn ? 1.0f : 0.0f;
                if (s.weldOn) {
                    a.glow = 1.0f;
                    if (std::floor((g.t - dt) / 1.05) != std::floor(g.t / 1.05)) out.push_back({Event::Weld, Role::Ouroboros, tl(s, -0.45f, 0.3f, 0.9f), 1.0f});
                } else {
                    a.glow = 0.0f;
                }
                break;
            }
            // Traeger: vom Lager zur Palette und zurueck (Wege um Stuetzen und Mauerreste)
            const vec3 palStand = wl(s, kPalX + 0.55f, kPalY);
            const vec3 corner = wl(s, kWallX1 + 0.6f, kWallY + 0.6f);
            switch (g.step) {
                case 0: {  // zum Lager
                    setTask(ob::K_WALK);
                    const vec3 at = s.heap - flat(nrmF(s.heap - s.wallC, {1, 0, 0})) * 0.75f;
                    if (hdist(a.pos, at) > 1.6f && s.toHeap.valid() && s.toHeap.at(ifloor(a.pos.x), ifloor(a.pos.y)) >= 0) walkTo(a, s.toHeap, speed, dt, c);
                    else if (moveTo(at, speed)) g.step = 1, g.t = 0.0, g.lastEv = -1;
                    break;
                }
                case 1: {  // aufnehmen
                    setTask(ob::K_LAY);
                    const vec3 d = flat(s.heap - a.pos);
                    faceTo(std::atan2(d.y, d.x), 6.0);
                    a.alert = 0.0f;
                    a.strike = (float)std::min(1.0, g.t / 1.6);
                    setTarget(s.heap + vec3{0, 0, 0.22f});
                    if (a.strike >= 0.45f && g.lastEv < 0) out.push_back({Event::Work, Role::Ouroboros, s.heap + vec3{0, 0, 0.4f}, 0.5f}), g.lastEv = 0;
                    if (a.strike >= 1.0f) g.step = 2;
                    break;
                }
                case 2:  // tragen bis zur Ecke, dann zur Palette
                    setTask(ob::K_CARRY);
                    a.alert = 1.0f;
                    if (hdist(a.pos, corner) > 1.0f && s.toPallet.valid() && s.toPallet.at(ifloor(a.pos.x), ifloor(a.pos.y)) >= 0) {
                        if (!walkTo(a, s.toPallet, speed, dt, c)) moveTo(corner, speed);
                    } else if (moveTo(palStand, speed)) {
                        g.step = 3, g.t = 0.0, g.lastEv = -1;
                    }
                    break;
                case 3: {  // absetzen
                    setTask(ob::K_LAY);
                    faceTo(s.wallYaw + kPiF, 6.0);
                    a.alert = 1.0f;
                    a.strike = (float)std::min(1.0, g.t / 1.5);
                    setTarget(wl(s, kPalX, kPalY, kPalTop + ob::kCourseH));
                    if (a.strike >= 0.55f && g.lastEv < 0) out.push_back({Event::Work, Role::Ouroboros, wl(s, kPalX, kPalY, 0.6f), 0.5f}), g.lastEv = 0;
                    if (a.strike >= 1.0f) g.step = 4, g.t = 0.0;
                    break;
                }
                case 4:  // ueber die Ecke zurueck
                    setTask(ob::K_WALK);
                    if (moveTo(corner, speed)) g.step = 0;
                    break;
                default: g.step = 0; break;
            }
            break;
        }
        default: break;
    }
    g.prev = a.pos;
    prepOuroboros(a, c);
}

}  // namespace lim::game
