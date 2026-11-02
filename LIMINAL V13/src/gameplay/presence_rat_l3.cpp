// V13.3 Level 3: Death Rats wie Wasserschweine, die Fallen bauen (KREATUR_DeathRat.md, Abschnitt Level 3; Wikidot Level 3
// Rev. 18, Entity 24 Rev. 25). Level-JSON presence.rat.mode "l3"; Rolle Rat, Varianten 2 (Ratte) und 3 (Falle).
//
// Die Seite: capybaragross, kuenstlich geschaerfte Zaehne (an den Gittern gewetzt), bauen die ausgefeiltesten Fallen mit
// primitiver Elektrik (sie wirken nicht immer, sind aber eine ernste Gefahr), menschlich klug, sprechen ueber
// Text-zu-Sprache-Geraete, fuehren Brigaden an; koerperlich schwach. Was sie im Einzelnen tun, sagt die Seite nicht - das
// Verhalten hier ist Auslegung (im Briefing so gekennzeichnet):
//
// Brigade (2-3 Ratten, presence.rat.group), je Ratte ein Auftrag (job):
//   0 wetzt     sitzt (am Gitter, wenn eines nah ist) und wetzt die Zaehne - das Kreischen von Zahn auf Stahl hoert man
//               zuerst (Geraeusch vor Sicht); bemerkt sie den Spieler (Sicht bis 16 m, Laerm), spricht das Geraet
//   1 lauert    wartet ausserhalb der Lampe: Abstand = Reichweite der Lampe (an: 7,5 m, aus: 3,5 m) + was sie gelernt hat;
//               im Lichtkegel tritt sie seitlich aus dem Strahl (keine Flucht); die Augen leuchten im Dunkel. Spricht.
//   2 baut      geht ungesehen zu einer Stelle auf dem Weg des Spielers (vor ihm oder dort, wo er schon war - sie merken sich
//               seinen Weg) und baut eine Falle (Wetzen, Draht, Klicken, Knistern, 7-10 s); kommt er nah, bricht sie ab
//   3 treibt    liegt eine Falle vor ihm, kommen sie von hinten (der Spieler zwischen Ratten und Falle), langsam, sprechen
//               ("keep walking"); rennt er auf sie zu, drohen sie
//   4 droht     richtet sich auf, Zaehne frei, Zischen und "STOP." (1,1 s) - dann Biss, wenn er noch nah ist (Ankuendigung)
//   5 beisst    stoesst zu (14 Punkte x Schwierigkeit, nur wer nah bleibt), danach
//   6 weicht    zurueck ins Dunkel (koerperlich schwach), dann wieder lauern
//   7 stuerzt   der Spieler haengt in einer Falle (Betaeubung): alle in 25 m rennen hin, kurz drohen ("NOW."), beissen
// Gedaechtnis (je Ebene, ueber Brigaden hinweg): der Weg des Spielers (24 Punkte), wie oft ihn die Lampe getroffen hat
// (laengerer Abstand), Fallen, ueber die er gesprungen ist (die naechste hinter eine Ecke, "CLEVER."), Treffer, Beil.
// Falle (Variante 3): blanker Draht in 18 cm Hoehe quer durch den Gang; Quelle an der Wand: rote Leuchte, Knistern alle
//   2,5-4 s (hoerbar bis 12 m, mit Funken), Brummen. Wer mit den Fuessen darunter durchgeht (nicht springt), bekommt den
//   Schlag: 10 Punkte, Betaeubung (Muskelkrampf: langsam, kein Sprint, 6 s), die Lichter flackern; jede vierte Falle versagt
//   (die Seite: nicht immer wirksam) - es zischt und der Draht reisst. Danach liegt sie zerrissen da.
// Klaenge und Saetze: Klangnummern (gameplay/kin_l3.hpp, KC_RAT_*), Synthese in audio/synth_rat_l3.cpp.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"
#include "gameplay/kin_l3.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kWire = 0.18f;          // Drahthoehe ueber dem Boden
constexpr double kDud = 0.25;           // Anteil der Fallen, die versagen (Auslegung von "nicht immer wirksam")
constexpr float kRatClear = 0.75f;      // freie Hoehe fuer die grosse Ratte (Wegefeld)

struct RatMem {  // Gedaechtnis der Death Rats (je Ebene)
    std::array<vec3, 24> trail{};
    int nTrail = 0, head = 0;
    double trailT = 0.0;
    int encounters = 0, lit = 0, jumped = 0, shocked = 0, watched = 0, bitten = 0;
    bool hatchet = false;
    int lastSay = -1;
    double quick = 1.0;  // Tests, Bilder: Zeitraffer der Bauzeit
    double reqTrap = -1.0;  // Skript/Test: Falle so weit vor dem Spieler bauen
    bool noBuild = false;   // Skript/Test: Fallenbau aus (build 0)
};

struct RatL3 {
    int job = 0;
    double t = 0.0, sayT = 3.0, grindT = 0.0, fieldT = 0.0, beamT = 0.0, buildT = 0.0, life = 0.0, holdT = 0.0;
    vec3 hold, site;
    float siteYaw = 0.0f;
    bool hasHold = false, bit = false, gone = false, saw = false, warned = false;
    int prevJob = 0;
};

struct Trap {
    vec3 aL, aR;           // Isolatoren (Wandflaeche, Drahthoehe)
    bool geo = false, snapped = false, dud = false, triggered = false;
    double crackT = 2.0, spark = 0.0, hum = 0.6, deadT = 0.0;
    int lastId = -1;
    vec3 lastFeet;
    bool hasLast = false, wasAir = false;
};

template <class T, class A>
T& extOf(A& a) {
    if (!a.ext) a.ext = std::make_shared<T>();
    return *static_cast<T*>(a.ext.get());
}

float cue(int k) { return kKinCue + (float)k; }

// Querschnitt: von p aus entlang d bis zur Wand (freie Kachel, Hoehe); Laenge (m), 0: keine Wand bis max
float toWall(world::World& w, vec3 p, vec3 d, float max) {
    for (float s = 0.05f; s <= max; s += 0.05f) {
        const vec3 q = p + d * s;
        const world::Tile* t = w.tileIfLoaded(ifloor(q.x), ifloor(q.y));
        if (!t || !t->open() || t->floor > p.z + 0.25f || t->clearance() < 1.2f) return s;
    }
    return 0.0f;
}

// Isolatoren einer Falle: die Wurzel steht an der linken Wand, die Gierrichtung quer durch den Gang
bool trapGeometry(world::World& w, vec3 pos, float yaw, vec3& aL, vec3& aR) {
    const vec3 d = dirYaw(yaw);
    const float wR = toWall(w, pos + d * 0.02f + vec3{0, 0, kWire}, d, 3.6f);
    if (wR < 0.7f) return false;
    aL = pos + vec3{0, 0, kWire};
    aR = pos + d * (wR + 0.02f) + vec3{0, 0, kWire};
    return true;
}

// Strecke p0-p1 schneidet a-b (in der Ebene)?
bool crosses(vec3 p0, vec3 p1, vec3 a, vec3 b) {
    auto side = [](vec3 o, vec3 x, vec3 y) { return (x.x - o.x) * (y.y - o.y) - (x.y - o.y) * (y.x - o.x); };
    const float d1 = side(a, b, p0), d2 = side(a, b, p1), d3 = side(p0, p1, a), d4 = side(p0, p1, b);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}
}  // namespace

bool Presence::ratL3(const Context& c) const { return c.kin && c.kin[world::KIN_RAT].on && c.kin[world::KIN_RAT].mode == "l3"; }

// Klang- und Satznummern der Ratten (Sprachgeraet): Ereignis Mimic mit Klangnummer (kin_l3.hpp)
void Presence::ratSay(Agent& a, int line, std::vector<Event>& out) {
    if (!l3Mem_) l3Mem_ = std::make_shared<RatMem>();
    auto& M = *static_cast<RatMem*>(l3Mem_.get());
    M.lastSay = line;
    a.talk = 1.0f;
    a.speakT = 1.6;
    out.push_back({Event::Mimic, Role::Rat, a.cre.headPos() + vec3{0, 0, -0.15f}, cue(KC_RAT_SAY0 + line)});
}

void Presence::prepRatL3(Agent& a, const Context& c) {
    if (a.variant == 2) {
        a.cre.setExtra({2.0f});
        a.rig.setGlow(1.0f);  // Hornspitzen
        return;
    }
    // Falle: Isolatoren aus der Karte (Wirt und Gast rechnen dasselbe), Funke und Brummen aus den Kanaelen
    if (!a.ext && (a.puppet || a.showcase)) {
        auto T = std::make_shared<Trap>();
        T->geo = trapGeometry(*c.world, a.pos, a.yaw, T->aL, T->aR);
        a.ext = T;
    }
    Trap* T = a.ext ? static_cast<Trap*>(a.ext.get()) : nullptr;
    if (!T || !T->geo) {
        a.cre.setExtra({3.0f, a.pos.x, a.pos.y, a.pos.z + kWire, a.pos.x, a.pos.y, a.pos.z + kWire, 0.0f, 0.0f, 0.0f});
        return;
    }
    const bool snapped = a.puppet || a.showcase ? a.hang > 0.5f : T->snapped;
    a.cre.setExtra({3.0f, T->aL.x, T->aL.y, T->aL.z, T->aR.x, T->aR.y, T->aR.z, snapped ? 1.0f : 0.0f, a.strike, a.shiver});
    // die rote Leuchte blinkt (aus, wenn zerrissen)
    a.rig.setGlow(snapped ? 0.0f : (std::fmod(time_, 1.3) < 0.12 || a.strike > 0.3f ? 1.0f : 0.25f));  // Funken: voll
}

// Baustelle: Gangkachel auf dem Weg des Spielers (vor ihm oder auf seinem frueheren Weg), ausser Sicht, quer 0,8-3,2 m
bool Presence::trapSite(const Context& c, vec3& pos, float& yaw, bool cornered) {
    if (!fld().valid()) return false;
    auto* M = static_cast<RatMem*>(l3Mem_.get());
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(60, 340, cand);
    const vec3 eye = c.view.eye;
    const vec3 look = nrmF(flat(c.view.look), {1, 0, 0});
    float best = -1e9f;
    int why[5] = {};  // Ablehnungen: Raum, Achse, Breite, gesehen, Nachbarfalle
    for (int tries = 0; tries < 320 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!figureWalkable(t, 1.9f) || (t->flags & world::TF_STAIR) || !t->room || !t->room->type) {
            ++why[0];
            continue;
        }
        const std::string& rk = t->room->type->key;
        if (rk != "gang" && rk != "gang_breit" && rk != "paranoia") {
            ++why[0];
            continue;
        }
        // Gangachse: offene Nachbarn in x oder y
        auto open = [&](i64 xx, i64 yy) {
            const world::Tile* n = c.world->tileIfLoaded(xx, yy);
            return n && n->open() && std::fabs(n->floor - t->floor) < 0.2f;
        };
        const bool ax = open(x + 1, y) && open(x - 1, y), ay = open(x, y + 1) && open(x, y - 1);
        if (ax == ay) {  // Kreuzung oder Sackgasse
            ++why[1];
            continue;
        }
        const vec3 across = ax ? vec3{0, 1, 0} : vec3{1, 0, 0};
        const vec3 mid{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        const float l = toWall(*c.world, mid + vec3{0, 0, kWire}, -across, 3.0f), r = toWall(*c.world, mid + vec3{0, 0, kWire}, across, 3.0f);
        if (l <= 0.0f || r <= 0.0f || l + r < 0.8f || l + r > 3.2f) {
            ++why[2];
            continue;
        }
        const vec3 root = mid - across * (l - 0.02f);
        // nicht im Blick (Quelle und Drahtmitte)
        bool seen = false;
        for (const vec3& q : {root + vec3{0, 0, 0.3f}, mid + vec3{0, 0, 0.3f}})
            if (dot(nrmF(q - eye, look), c.view.look) > 0.2f && clearSight(*c.world, eye, q)) seen = true;
        if (seen) {
            ++why[3];
            continue;
        }
        // nicht neben einer anderen Falle
        bool near = false;
        for (const auto& o : agents_)
            if (o->role == Role::Rat && o->variant == 3 && hdist(o->pos, root) < 6.0f) near = true;
        if (near) {
            ++why[4];
            continue;
        }
        float score = (float)rng_.uniform(0.0, 1.0);
        score += 2.0f * dot(nrmF(flat(mid - eye), look), look);  // vor ihm
        if (M)
            for (int i = 0; i < M->nTrail; ++i)
                if (hdist(M->trail[(size_t)i], mid) < 1.6f) score += 1.4f;  // dort ging er schon (Rueckweg)
        if (cornered) {  // nach einer Ecke: aus der Ferne nicht zu sehen (er ist schon gesprungen)
            const vec3 along = ax ? vec3{1, 0, 0} : vec3{0, 1, 0};
            const bool blind = toWall(*c.world, mid + vec3{0, 0, 1.0f}, along, 4.0f) > 0.0f || toWall(*c.world, mid + vec3{0, 0, 1.0f}, -along, 4.0f) > 0.0f;
            score += blind ? 2.5f : 0.0f;
        }
        if (score > best) best = score, pos = root, yaw = std::atan2(across.y, across.x);
    }
    if (best <= -1e8f)
        log::info("Praesenz: keine Stelle fuer eine Falle ({} Kacheln; abgelehnt: Raum {}, Achse {}, Breite {}, gesehen {}, Nachbarfalle {})", cand.size(), why[0],
                  why[1], why[2], why[3], why[4]);
    return best > -1e8f;
}

Presence::Agent* Presence::buildTrap(const Context& c, vec3 pos, float yaw, std::vector<Event>& out) {
    vec3 aL, aR;
    if (!trapGeometry(*c.world, pos, yaw, aL, aR)) return nullptr;
    int traps = 0;
    for (const auto& o : agents_) traps += o->role == Role::Rat && o->variant == 3 && !o->showcase ? 1 : 0;
    const int maxTraps = c.kin ? std::max(1, c.kin[world::KIN_RAT].count) : 3;
    if (traps >= maxTraps)  // die aelteste zerrissene (oder aelteste) raeumen sie ab
        for (size_t i = 0; i < agents_.size(); ++i)
            if (agents_[i]->role == Role::Rat && agents_[i]->variant == 3 && !agents_[i]->showcase) {
                remove(i);
                break;
            }
    auto a = std::make_unique<Agent>();
    a->role = Role::Rat;
    a->ent = Entity::Rat;
    a->variant = 3;
    a->noShadow = true;
    a->glow = 1.0f;
    auto T = std::make_shared<Trap>();
    T->aL = aL, T->aR = aR, T->geo = true;
    T->dud = rng_.uniform(0.0, 1.0) < kDud;
    a->ext = T;
    a->initBody(pos, yaw, (unsigned)rng_.randint(1, 1 << 30));
    a->shiver = 0.6f;
    Agent* raw = a.get();
    agents_.push_back(std::move(a));
    log::info("Praesenz: Death-Rat-Falle bei ({:.1f}, {:.1f}), {:.1f} m breit{}", pos.x, pos.y, length(aR - aL), T->dud ? " (versagt)" : "");
    (void)out;
    return raw;
}

bool Presence::spawnRatL3(const Context& c, std::vector<Event>& out, bool forced) {
    if (!ratL3(c) || !fld().valid()) return false;
    if (!l3Mem_) l3Mem_ = std::make_shared<RatMem>();
    auto& M = *static_cast<RatMem*>(l3Mem_.get());
    const world::KinDef& K = c.kin[world::KIN_RAT];
    // Ort: ausser Sicht, 14-30 m Weg; bevorzugt hinter Gittern (dort wetzen sie die Zaehne)
    vec3 p;
    bool ok = false, bars = false;
    const auto barsRoom = [&](vec3 q) {
        const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
        return t && t->room && t->room->type && t->room->type->key == "hinter_gitter" && figureWalkable(t, kRatClear);
    };
    const auto walk = [&](vec3 q) { return figureWalkable(c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y)), 1.2f); };
    if (!forced && rng_.uniform(0.0, 1.0) < 0.6) ok = bars = hiddenSpawn(c, 140, 320, false, p, barsRoom);
    if (!ok) ok = hiddenSpawn(c, forced ? 60 : 140, forced ? 160 : 300, false, p, walk) || hiddenSpawn(c, 100, 600, false, p, walk);
    if (!ok) return false;
    const int n = (int)rng_.randint(std::max(1, K.groupMin), std::max(1, K.groupMax));
    const int group = nextGroup_++;
    int made = 0;
    for (int i = 0; i < n * 4 && made < n; ++i) {
        vec3 q = p;
        if (made > 0) {
            q = p + vec3{(float)rng_.uniform(-1.8, 1.8), (float)rng_.uniform(-1.8, 1.8), 0.0f};
            const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
            if (!figureWalkable(t, kRatClear) || std::fabs(t->floor - p.z) > 0.3f) continue;
            q.z = t->floor;
            // auch die anderen ausser Sicht (niemand erscheint im Blick)
            bool seen = false;
            const vec3 fl = nrmF(flat(c.view.look), {1, 0, 0});
            for (float h : {0.25f, 0.6f})
                if (dot(nrmF(q + vec3{0, 0, h} - c.view.eye, fl), c.view.look) > 0.25f && clearSight(*c.world, c.view.eye, q + vec3{0, 0, h})) seen = true;
            if (seen) continue;
        }
        auto a = std::make_unique<Agent>();
        a->role = Role::Rat;
        a->ent = Entity::Rat;
        a->variant = 2;
        a->group = group;
        a->glow = 1.0f;
        a->initBody(q, (float)rng_.uniform(-kPiF, kPiF), (unsigned)rng_.randint(1, 1 << 30));
        RatL3& R = extOf<RatL3>(*a);
        R.job = 0;
        R.grindT = rng_.uniform(0.5, 3.0);
        R.sayT = rng_.uniform(2.0, 5.0);
        a->plea = 1.0f;  // sitzt
        out.push_back({Event::Appear, Role::Rat, q});
        agents_.push_back(std::move(a));
        ++made;
    }
    if (!made) return false;
    // eine Falle haben sie schon gebaut: vor dem Spieler, ausser Sicht
    vec3 tp;
    float ty = 0.0f;
    if (trapSite(c, tp, ty, M.jumped > 0)) buildTrap(c, tp, ty, out);
    ++M.encounters;
    log::info("Praesenz: Death Rats (Level 3) x{} bei ({:.1f}, {:.1f}), {:.0f} m{}", made, p.x, p.y, hdist(p, c.view.eye), bars ? ", hinter Gittern" : "");
    return true;
}

void Presence::regieRatL3(double dt, const Context& c, std::vector<Event>& out) {
    if (!ratL3(c)) return;
    if (!l3Mem_) l3Mem_ = std::make_shared<RatMem>();
    auto& M = *static_cast<RatMem*>(l3Mem_.get());
    const world::KinDef& K = c.kin[world::KIN_RAT];
    // den Weg des Spielers merken (alle 2 s ein Punkt)
    M.trailT -= dt;
    if (M.trailT <= 0.0) {
        M.trailT = 2.0;
        const vec3 f{(float)c.player->x, (float)c.player->y, (float)c.player->z};
        if (M.nTrail == 0 || hdist(M.trail[(size_t)((M.head + 23) % 24)], f) > 1.5f) {
            M.trail[(size_t)M.head] = f;
            M.head = (M.head + 1) % 24;
            M.nTrail = std::min(24, M.nTrail + 1);
        }
    }
    if (c.hatchet) M.hatchet = true;
    if (M.reqTrap > 0.0) {  // Skript/Test: Falle quer im Gang vor dem Spieler
        const vec3 lk = nrmF(flat(c.view.look), {1, 0, 0});
        const vec3 p0 = vec3{(float)c.player->x, (float)c.player->y, (float)c.player->z} + lk * (float)M.reqTrap;
        const vec3 across = std::fabs(lk.x) > std::fabs(lk.y) ? vec3{0, 1, 0} : vec3{1, 0, 0};
        const float l = toWall(*c.world, p0 + vec3{0, 0, kWire}, -across, 3.0f);
        if (l > 0.0f) buildTrap(c, p0 - across * (l - 0.02f), std::atan2(across.y, across.x), out);
        else log::warn("Praesenz: keine Wand fuer die Falle vor dem Spieler");
        M.reqTrap = -1.0;
    }
    bool any = false;
    for (const auto& a : agents_) any = any || (a->role == Role::Rat && a->variant == 2 && !a->showcase);
    if (any) return;
    nextKin_[world::KIN_RAT] -= dt;
    if (nextKin_[world::KIN_RAT] <= 0.0)
        nextKin_[world::KIN_RAT] = spawnRatL3(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 8.0;
}

// --- die Falle ----------------------------------------------------------------------------------------------------------
void Presence::thinkRatTrap(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    Trap& T = extOf<Trap>(a);
    if (!T.geo) T.geo = trapGeometry(*c.world, a.pos, a.yaw, T.aL, T.aR);
    if (!T.geo) {
        a.state = 9;
        return;
    }
    a.stateT += dt;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const vec3 mid = (T.aL + T.aR) * 0.5f;
    const float d = hdist(mid, feet);
    a.hang = T.snapped ? 1.0f : 0.0f;
    if (T.snapped) {
        a.shiver = 0.0f;
        T.spark = std::max(0.0, T.spark - dt * 3.0);
        a.strike = (float)T.spark;
        T.deadT += dt;
        if (d > 70.0f && a.unseenT > 5.0) a.state = 9;
        return;
    }
    // Ankuendigung: Knistern mit Funken (hoerbar bis 12 m), Brummen im Draht
    a.shiver = 0.6f;
    T.crackT -= dt;
    if (T.crackT <= 0.0) {
        T.crackT = rng_.uniform(2.5, 4.0);
        T.spark = 1.0;
        if (d < 16.0f) out.push_back({Event::Mimic, Role::Rat, a.pos + vec3{0, 0, 0.1f}, cue(KC_TRAP_CRACKLE)});
    }
    T.spark = std::max(0.0, T.spark - dt * 5.0);
    a.strike = (float)T.spark;
    // Ausloesen: die Fuesse kreuzen den Draht (nicht im Sprung darueber)
    const bool air = !c.player->onGround && feet.z > a.pos.z + kWire + 0.03f;
    if (T.hasLast && T.lastId == a.target && crosses(T.lastFeet, feet, T.aL, T.aR) && !(air && T.wasAir) && !air && c.mode >= 1) {
        T.snapped = true;
        T.spark = 1.0;
        auto* M = static_cast<RatMem*>(l3Mem_.get());
        if (T.dud) {  // versagt: es zischt, der Draht reisst
            out.push_back({Event::Mimic, Role::Rat, mid, cue(KC_TRAP_FIZZLE)});
            log::info("Praesenz: Death-Rat-Falle versagt");
        } else {
            out.push_back({Event::Mimic, Role::Rat, mid, cue(KC_TRAP_ZAP)});
            out.push_back({Event::Flicker, Role::Rat, feet + vec3{0, 0, 2.0f}});
            if (c.mode >= 2) {
                out.push_back({Event::Hit, Role::Rat, mid, 10.0f});
                addDose(a.target, 0.85f);
            }
            if (M) ++M->shocked;
            log::info("Praesenz: Death-Rat-Falle - Schlag fuer Spieler {}", a.target);
            // die Ratten hoeren es: alle in 25 m stuerzen sich auf die Beute
            for (auto& o : agents_)
                if (o->role == Role::Rat && o->variant == 2 && !o->showcase && hdist(o->pos, feet) < 25.0f) {
                    RatL3& R = extOf<RatL3>(*o);
                    R.job = 7, R.t = 0.0, R.bit = false, R.hasHold = false;
                }
        }
    } else if (T.hasLast && T.lastId == a.target && crosses(T.lastFeet, feet, T.aL, T.aR)) {
        // darueber gesprungen: sie merken es sich (die naechste Falle hinter eine Ecke)
        if (auto* M = static_cast<RatMem*>(l3Mem_.get())) {
            ++M->jumped;
            for (auto& o : agents_)
                if (o->role == Role::Rat && o->variant == 2 && !o->showcase && hdist(o->pos, feet) < 20.0f && o->sight.level < 0.5f) {
                    ratSay(*o, 8, out);  // "CLEVER."
                    break;
                }
        }
        log::info("Praesenz: ueber die Falle gesprungen");
    }
    T.lastFeet = feet, T.hasLast = true, T.lastId = a.target, T.wasAir = air;
}

// --- die grosse Ratte -----------------------------------------------------------------------------------------------------
void Presence::thinkRatL3(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (!l3Mem_) l3Mem_ = std::make_shared<RatMem>();
    auto& M = *static_cast<RatMem*>(l3Mem_.get());
    RatL3& R = extOf<RatL3>(a);
    const vec3 eye = c.view.eye;
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, feet);
    a.stateT += dt;
    a.cool -= dt;
    R.t += dt;
    R.life += dt;
    a.speakT -= dt;
    if (a.speakT <= 0.0) a.talk = 0.0f;
    a.reach = 0.0f, a.jaw = 0.0f, a.strike = 0.0f;
    a.look = true, a.lookAt = eye;
    // Sinne: sieht (Lampe, nah, frei), hoert Rennen
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head, eye);
    const bool sees = los && (d < 16.0f || (c.view.flashOn && d < 26.0f));
    const bool hears = (c.player->sprinting && d < 22.0f) || (c.noise > 0.4 && d < 12.0f);
    lightReact(a, c, head, 12.0f, dt, 1e9);
    const bool inBeamNow = inBeam(c.view, *c.world, head, 12.0f) || inBeam(c.view, *c.world, a.pos + vec3{0, 0, 0.35f}, 12.0f);
    R.beamT = inBeamNow ? R.beamT + dt : std::max(0.0, R.beamT - dt);
    if (inBeamNow && R.beamT > 0.5 && R.beamT - dt <= 0.5) ++M.lit;
    // Schritte (schwer, gepolstert), Zaehne
    if (a.cre.stepped() && d < 18.0f) out.push_back({Event::Mimic, Role::Rat, a.cre.stepPos(), cue(KC_RAT_STEP)});
    auto setJob = [&](int j) {
        if (R.job != j) R.prevJob = R.job, R.job = j, R.t = 0.0, R.hasHold = false, R.warned = false;
    };
    auto say = [&](int line, double minGap) {
        if (R.sayT > 0.0 || a.speakT > 0.0) return;
        // nicht alle zugleich: die Brigade spricht abwechselnd
        for (const auto& o : agents_)
            if (o.get() != &a && o->role == Role::Rat && o->variant == 2 && o->speakT > 0.0) return;
        ratSay(a, line, out);
        R.sayT = minGap + rng_.uniform(0.0, minGap * 0.6);
    };
    R.sayT -= dt;
    // Lampe: wie weit sie reicht - so weit (und was sie gelernt haben) bleiben sie weg
    const float lampReach = (c.view.flashOn ? 7.5f : 3.5f) + std::min(3.0f, 0.6f * (float)M.lit) + (M.hatchet ? 1.0f : 0.0f);
    auto goTo = [&](vec3 goal, double speed) {
        R.fieldT -= dt;
        if (!a.hasGoal || hdist(goal, a.goal) > 0.8f || R.fieldT <= 0.0) {
            a.field.build(*c.world, ifloor(goal.x), ifloor(goal.y), kRatClear);
            a.goal = goal, a.hasGoal = true, R.fieldT = 1.5;
        }
        if (hdist(a.pos, goal) < 0.4f) return false;
        if (!walkTo(a, a.field, speed, dt, c)) {
            const vec3 to = nrmF(flat(goal - a.pos), dirYaw(a.yaw));
            const vec3 np = a.pos + to * (float)(speed * dt);
            if (figureWalkable(c.world->tileIfLoaded(ifloor(np.x), ifloor(np.y)), kRatClear)) a.pos = np;
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 4.0);
        }
        return true;
    };
    // ein Platz im Abstand r vom Spieler, moeglichst dunkel und ausser dem Strahl (Seite: weg vom Strahl)
    auto pickHold = [&](float r, vec3 from) {
        float best = -1e9f;
        vec3 res = a.pos;
        const vec3 beam = nrmF(flat(c.view.flashDir), {1, 0, 0});
        for (int k = 0; k < 18; ++k) {
            const float ang = (float)rng_.uniform(-kPiF, kPiF);
            const vec3 q = feet + dirYaw(ang) * (r + (float)rng_.uniform(-0.6, 0.8));
            const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
            if (!figureWalkable(t, kRatClear) || std::fabs(t->floor - feet.z) > 0.6f) continue;
            const int cost = fld().at(ifloor(q.x), ifloor(q.y));
            if (cost < 0 || cost > (int)(r * 16.0f) + 40) continue;
            const vec3 qq{q.x, q.y, t->floor};
            float s = -0.3f * hdist(qq, from) + (darkTile(c, t, q.x, q.y) ? 2.0f : 0.0f);
            if (c.view.flashOn && dot(nrmF(flat(qq - feet), beam), beam) > 0.8f) s -= 4.0f;  // nicht im Strahl
            if (clearSight(*c.world, qq + vec3{0, 0, 0.5f}, eye)) s += 0.8f;              // sie wollen ihn sehen
            if (s > best) best = s, res = qq;
        }
        return res;
    };
    // die naechste Falle vor ihm (fuer das Treiben)
    const Agent* trap = nullptr;
    for (const auto& o : agents_)
        if (o->role == Role::Rat && o->variant == 3 && !o->showcase && o->ext && !static_cast<const Trap*>(o->ext.get())->snapped &&
            hdist(o->pos, feet) < 18.0f && (!trap || hdist(o->pos, feet) < hdist(trap->pos, feet)))
            trap = o.get();
    // wer ist der Anfuehrer (die erste der Gruppe)? Er treibt; andere bauen
    bool leader = true;
    for (const auto& o : agents_) {
        if (o.get() == &a) break;
        if (o->role == Role::Rat && o->variant == 2 && o->group == a.group && !o->showcase) leader = false;
    }
    bool someoneBuilds = false;
    for (const auto& o : agents_)
        if (o.get() != &a && o->role == Role::Rat && o->variant == 2 && o->ext && static_cast<const RatL3*>(o->ext.get())->job == 2) someoneBuilds = true;
    // Abzug: nach 3-4 Minuten oder weit weg und ungesehen
    if ((R.life > 210.0 && a.unseenT > 4.0) || (d > 55.0f && a.unseenT > 6.0)) {
        a.state = 9;
        return;
    }
    a.plea = 0.0f, a.crouch = 0.0f, a.perceive = 0.0f;
    switch (R.job) {
        case 0: {  // sitzt und wetzt
            a.plea = 1.0f;
            R.grindT -= dt;
            const bool grinding = std::fmod(R.t, 7.0) < 4.2;
            a.jaw = grinding ? 1.0f : 0.0f;
            if (grinding && R.grindT <= 0.0 && d < 30.0f) {
                R.grindT = rng_.uniform(1.4, 2.2);
                out.push_back({Event::Mimic, Role::Rat, head, cue(KC_RAT_GRIND)});
            }
            if (sees || hears || d < 6.0f) {
                R.saw = true;
                setJob(1);
                R.sayT = 0.0;  // der erste Satz sofort
                say(M.encounters > 1 ? (M.encounters > 2 ? 5 : 4) : 0, 9.0);  // "WE SEE YOU." / "YOU CAME BACK." / "WE REMEMBER YOU."
            }
            break;
        }
        case 1: {  // lauert ausserhalb der Lampe
            a.crouch = 0.45f;
            a.perceive = 1.0f, a.perceiveAt = eye;
            R.holdT -= dt;
            const bool lit = R.beamT > 0.35;
            if (!R.hasHold || R.holdT <= 0.0 || lit || std::fabs(hdist(R.hold, feet) - lampReach) > 2.5f) {
                R.hold = pickHold(lampReach + 1.0f, a.pos), R.hasHold = true, R.holdT = rng_.uniform(2.5, 4.5);
            }
            const bool moving = goTo(R.hold, lit ? 2.0 : (d < lampReach - 1.0f ? 1.6 : 1.0));
            if (!moving) {  // steht: sieht ihn an
                const vec3 to = nrmF(flat(eye - a.pos), dirYaw(a.yaw));
                a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 2.0);
                a.plea = std::fmod(R.t, 11.0) < 6.0 ? 1.0f : 0.0f;  // setzt sich ab und zu (wartet)
                if (a.plea > 0.5f) a.crouch = 0.0f;
            }
            if (lit) say(1, 10.0);  // "TURN OFF YOUR LIGHT."
            else if (d < 14.0f) say(rng_.uniform(0.0, 1.0) < 0.5 ? 2 : (M.encounters > 1 ? 5 : 3), 12.0);
            // Auftraege: der Anfuehrer treibt zur Falle, ein anderer baut eine neue
            if (leader && trap && hdist(trap->pos, feet) < 16.0f) setJob(3);
            else if (!leader && !someoneBuilds && R.t > 6.0 && !M.noBuild) {
                vec3 tp;
                float ty = 0.0f;
                if (trapSite(c, tp, ty, M.jumped > 0)) R.site = tp, R.siteYaw = ty, setJob(2), R.buildT = 0.0;
            }
            if (d < 2.2f && a.cool <= 0.0) setJob(4);  // zu nah: droht
            break;
        }
        case 2: {  // baut eine Falle
            const vec3 workAt = R.site + dirYaw(R.siteYaw) * 0.55f;
            if (d < 6.0f && a.sight.level > 0.4f) {  // gesehen: abbrechen
                ++M.watched;
                setJob(6);
                break;
            }
            if (goTo(workAt, 1.4) && R.buildT <= 0.0) break;
            // arbeiten: hockt, die Finger am Draht, wetzt, Klicken und Knistern
            const vec3 to = dirYaw(R.siteYaw + kPiF);
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 3.0);
            a.crouch = 0.8f, a.reach = 0.35f + 0.25f * (float)std::sin(R.t * 4.0);
            a.look = false;
            a.perceive = 1.0f, a.perceiveAt = R.site + vec3{0, 0, 0.1f};
            a.jaw = std::fmod(R.buildT, 3.0) < 1.0 ? 1.0f : 0.0f;
            R.buildT += dt * M.quick;
            R.grindT -= dt;
            if (R.grindT <= 0.0 && d < 24.0f) {
                R.grindT = rng_.uniform(1.0, 1.8);
                out.push_back({Event::Mimic, Role::Rat, R.site + vec3{0, 0, 0.15f}, cue(KC_RAT_TINKER)});
            }
            if (R.buildT > 8.0) {
                buildTrap(c, R.site, R.siteYaw, out);
                setJob(1);
            }
            break;
        }
        case 3: {  // treibt ihn zur Falle: von hinten (er zwischen Ratten und Falle)
            if (!trap) {
                setJob(1);
                break;
            }
            const vec3 away = nrmF(flat(feet - trap->pos), dirYaw(a.yaw));
            const float side = (float)((int)(((std::uintptr_t)&a >> 6) % 3) - 1) * 0.9f;  // je Ratte fest: links, Mitte, rechts
            const vec3 goal = feet + away * 3.8f + vec3{-away.y, away.x, 0.0f} * side;
            goTo(goal, d > 6.0f ? 2.0 : 0.9);
            a.crouch = 0.3f;
            a.perceive = 1.0f, a.perceiveAt = eye;
            if (rng_.uniform(0.0, 1.0) < dt * 0.5 && d < 10.0f) out.push_back({Event::Mimic, Role::Rat, head, cue(KC_RAT_CHATTER)});
            say(rng_.uniform(0.0, 1.0) < 0.5 ? 2 : 10, 8.0);  // "KEEP WALKING." / "THIS WAY IS SAFE."
            if (d < 2.2f && a.cool <= 0.0) setJob(4);
            if (hdist(trap->pos, feet) > 20.0f || R.t > 40.0) setJob(1);
            break;
        }
        case 4: {  // droht (Ankuendigung des Bisses)
            a.strike = 1.0f;
            a.jaw = 1.0f;
            const vec3 to = nrmF(flat(eye - a.pos), dirYaw(a.yaw));
            a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 6.0);
            if (!R.warned) {
                R.warned = true;
                out.push_back({Event::Mimic, Role::Rat, head, cue(KC_RAT_HISS)});
                R.sayT = 0.0, a.speakT = 0.0;
                say(6, 4.0);  // "STOP."
            }
            if (R.t > 1.1) setJob(d < 1.7f ? 5 : 6);
            break;
        }
        case 5: {  // beisst
            a.reach = 1.0f;
            const vec3 to = nrmF(flat(feet - a.pos), dirYaw(a.yaw));
            if (R.t < 0.3) a.pos += to * (float)(2.2 * dt);
            if (!R.bit && R.t > 0.22) {
                R.bit = true;
                if (d < 1.55f && c.mode >= 2) {
                    out.push_back({Event::Hit, Role::Rat, head, 14.0f});
                    ++M.bitten;
                }
                out.push_back({Event::Mimic, Role::Rat, head, cue(KC_RAT_BITE)});
            }
            if (R.t > 0.6) {
                a.cool = rng_.uniform(4.0, 6.0);
                setJob(6);
            }
            break;
        }
        case 6: {  // weicht ins Dunkel zurueck
            if (!R.hasHold) R.hold = pickHold(lampReach + 4.0f, a.pos), R.hasHold = true;
            goTo(R.hold, 2.4);
            if (R.t > 4.5) setJob(1);
            break;
        }
        case 7: {  // die Beute haengt in der Falle: hin, kurz drohen, beissen
            const float st = stupor(a.target);
            if (R.t > 14.0 || (st < 0.2f && R.t > 2.0)) {
                setJob(6);
                break;
            }
            if (R.t < dt * 1.5) say(7, 3.0);  // "NOW."
            if (d > 1.4f) {
                goTo(feet, 2.7);
                a.crouch = 0.4f;
            } else {
                setJob(4);
                R.t = 0.45;  // kuerzer drohen - er ist gelaehmt (die Ankuendigung bleibt hoerbar)
            }
            break;
        }
        default: break;
    }
    if (R.job != 0 && R.job != 2 && R.t > 0.1 && rng_.uniform(0.0, 1.0) < dt * 0.08 && d < 20.0f)
        out.push_back({Event::Mimic, Role::Rat, head, cue(KC_RAT_CHATTER)});
}

// --- Klaenge und Untertitel (Klangnummern ab KC_RAT_SAY0, kin_l3.hpp) -------------------------------------------------------
namespace {
const char* const kHintRat = "Eine Death Rat spricht über ein Sprachgerät. Sie sind klug: sie bauen Fallen und treiben dich hinein – "
                             "auf Knistern und blanken Draht achten.";
const char* const kHintTrap = "Ein Knistern wie an einem Weidezaun – irgendwo quer im Gang liegt ein Draht. Darüber springen oder umkehren.";
const char* const kHintSwarm = "Licht lockt Deathmoths an. Lampe aus und weg – sie greifen das Gesicht an.";
const char* const kHintPrae = "Ein Praetorianer: er sticht mit Gift. Bäumt er sich auf – sofort zurück.";
const std::array<KinLine, KC_L3_END - KC_RAT_SAY0>& l3Lines() {
    static const std::array<KinLine, KC_L3_END - KC_RAT_SAY0> L = [] {
        std::array<KinLine, KC_L3_END - KC_RAT_SAY0> t{};
        auto at = [&](int cue) -> KinLine& { return t[(size_t)(cue - KC_RAT_SAY0)]; };
        static const char* const say[KC_RAT_SAYS] = {"„WE SEE YOU.“",    "„TURN OFF YOUR LIGHT.“", "„KEEP WALKING.“",     "„DO NOT RUN.“",
                                                      "„YOU CAME BACK.“", "„WE REMEMBER YOU.“",     "„STOP.“",             "„NOW.“",
                                                      "„CLEVER.“",        "„IT ONLY HURTS ONCE.“",  "„THIS WAY IS SAFE.“", "„GO.“"};
        static std::array<std::string, KC_RAT_SAYS> names;
        for (int k = 0; k < KC_RAT_SAYS; ++k) {
            names[(size_t)k] = "rat_tts_" + std::to_string(k);
            at(KC_RAT_SAY0 + k) = {names[(size_t)k].c_str(), say[k], 0.85f, 22.0f, 2.2f, kHintRat};
        }
        at(KC_RAT_GRIND) = {"rat_grind", nullptr, 0.8f, 34.0f};
        at(KC_RAT_CHATTER) = {"rat_chatter", nullptr, 0.6f, 14.0f};
        at(KC_RAT_HISS) = {"rat_hiss", nullptr, 1.0f, 18.0f};
        at(KC_RAT_BITE) = {"rat_bite_big", nullptr, 1.0f, 14.0f};
        at(KC_RAT_STEP) = {"rat_step_big", nullptr, 0.45f, 14.0f};
        at(KC_RAT_TINKER) = {"rat_tinker", nullptr, 0.6f, 20.0f};
        at(KC_TRAP_CRACKLE) = {"trap_crackle", nullptr, 0.7f, 14.0f, 2.6f, kHintTrap};
        at(KC_TRAP_ZAP) = {"trap_zap", nullptr, 1.0f, 40.0f};
        at(KC_TRAP_FIZZLE) = {"trap_fizzle", nullptr, 0.9f, 26.0f};
        at(KC_MOTH_SWARM) = {"moth_swarm_rise", nullptr, 1.0f, 30.0f, 2.6f, kHintSwarm};
        at(KC_MOTH_FACE) = {"moth_face", nullptr, 1.0f, 6.0f};
        at(KC_MOTH_SHAKE) = {"moth_shake", nullptr, 0.7f, 8.0f};
        at(KC_PR_CLICK) = {"prae_click", nullptr, 0.45f, 14.0f};
        at(KC_PR_FAN) = {"prae_fan", nullptr, 0.8f, 20.0f};
        at(KC_PR_REAR) = {"prae_rear", nullptr, 1.0f, 18.0f, 2.6f, kHintPrae};
        at(KC_PR_STAB) = {"prae_stab", nullptr, 1.0f, 14.0f};
        at(KC_PR_SPIT) = {"prae_spit", nullptr, 0.9f, 16.0f};
        return t;
    }();
    return L;
}
}  // namespace

const KinLine* ratMothLine(int cue) {
    if (cue < KC_RAT_SAY0 || cue >= KC_L3_END) return nullptr;
    const KinLine& L = l3Lines()[(size_t)(cue - KC_RAT_SAY0)];
    return L.sound ? &L : nullptr;
}

const KinLine* ratMothLineBySound(const std::string& s) {
    for (const KinLine& L : l3Lines())
        if (L.sound && s == L.sound) return &L;
    return nullptr;
}

// --- Tests, Skripte ---------------------------------------------------------------------------------------------------------
bool Presence::l3Set(const std::string& key, double v, const Context* c) {
    (void)c;
    if (!l3Mem_) l3Mem_ = std::make_shared<RatMem>();
    auto& M = *static_cast<RatMem*>(l3Mem_.get());
    Agent* rat = nullptr;
    for (auto& a : agents_)
        if (!rat && a->role == Role::Rat && a->variant == 2 && !a->showcase) rat = a.get();
    if (key == "trap") M.reqTrap = std::max(0.5, v);
    else if (key == "quick") M.quick = std::max(0.1, v);
    else if (key == "build") M.noBuild = v < 0.5;
    else if (key == "rats") nextKin_[world::KIN_RAT] = v;
    else if (key == "mem" && v <= 0.0) M = RatMem{};
    else if (key == "say" && rat) {
        std::vector<Event> tmp;
        ratSay(*rat, std::clamp((int)v, 0, KC_RAT_SAYS - 1), tmp);
    } else if (key == "job" && rat) {  // alle Ratten
        for (auto& a : agents_)
            if (a->role == Role::Rat && a->variant == 2 && !a->showcase) {
                RatL3& R = extOf<RatL3>(*a);
                R.job = (int)v, R.t = 0.0, R.hasHold = false, R.warned = false;
            }
    } else if (key == "spark") {  // Bilder: alle Fallen funken jetzt
        for (auto& a : agents_)
            if (a->role == Role::Rat && a->variant == 3 && a->ext) static_cast<Trap*>(a->ext.get())->spark = v;
    } else if (key == "clear") {  // alle Fallen fort
        std::erase_if(agents_, [](const auto& a) { return a->role == Role::Rat && a->variant == 3 && !a->showcase; });
    } else if (key == "swarm") {
        swarmRouse(v > 0.5);
    } else if (key == "pstate") {
        for (auto& a : agents_)
            if (a->role == Role::Moth && a->variant == 5 && !a->showcase) {
                a->state = (int)v, a->stateT = 0.0;
                break;
            }
    } else {
        return false;
    }
    return true;
}

Presence::L3Info Presence::l3Info() const {
    L3Info r;
    if (const auto* M = static_cast<const RatMem*>(l3Mem_.get()))
        r.jumped = M->jumped, r.shocked = M->shocked, r.lit = M->lit, r.encounters = M->encounters, r.trail = M->nTrail;
    for (const auto& a : agents_) {
        if (a->showcase) continue;
        if (a->role == Role::Rat && a->variant == 2) {
            ++r.rats;
            if (r.ratJob < 0 && a->ext) r.ratJob = static_cast<const RatL3*>(a->ext.get())->job;
        }
        if (a->role == Role::Rat && a->variant == 3) {
            ++r.traps;
            if (const auto* T = static_cast<const Trap*>(a->ext.get()); T && !T->snapped) {
                ++r.armed;
                r.trapL = T->aL, r.trapR = T->aR, r.trapPos = a->pos;
            }
        }
        if (a->role == Role::Moth && (a->variant == 3 || a->variant == 4)) ++r.commons, r.onFace += a->state == 4 ? 1 : 0;
        if (a->role == Role::Moth && a->variant == 5) {
            ++r.praetorians;
            if (r.pState < 0) r.pState = a->state;
        }
    }
    r.roused = swarmRoused();
    return r;
}

}  // namespace lim::game
