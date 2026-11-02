// V13.3 Teil 4 Level 5.3: der Hausherr besucht die Bibliothek (KREATUR_Gentleman.md; Wikidot level-5-3 Rev. 40, Schluss-
// geschichte; entity-18 Rev. 54 - CC BY-SA 3.0).
//
// Wiki (Level 5.3): Der Herr des Hotels - die Seite nennt ihn dort "the Gentleman" und "Beast of Level 5" - geht wie gewohnt
// durch sein Reich, bemerkt am Ende eines Flurs eine kleine Holztuer und tritt in die Bibliothek, die er nie wiedersehen
// wollte. Aus dem naechsten Regal faellt ihm ein Notizbuch vor die Fuesse: Skizzen und Notizen ueber sein Personal. Er wirft
// es, ohne hinzusehen, hinter sich, spricht zur Bibliothek (sie sei nicht willkommen, er werde ihre Hoffnung fuer die Menschen
// wieder zerschlagen), bekommt nur Stille zur Antwort und geht mit unbewegtem Gesicht, das seinen Aerger verbirgt. Die
// Bibliothek ist dem Personal feindlich (Behavior); Zorn zeigt sie, indem Regale einstuerzen. Entity 18: er sieht alles, riecht
// Angst, zeigt sich nur, wem er will; wer ihn aus dem Augenwinkel sieht, den hat er schon bemerkt.
//
// Umsetzung (Auslegung im Briefing):
//   Besuch    einmal je Besuch der Ebene (Level-JSON presence.gentleman: every = fruehestens nach so vielen Sekunden, count),
//             wenn ein Spieler in der Halle steht und niemand im alten Gang: hinten im Gang geht eine Tuer (vor Sicht),
//             gemessene Schritte kommen naeher, er tritt durch die alte Tuer in die Halle; die Regale knarren.
//   Szene     an seinem Platz (Karte): er sieht sich um, seufzt, erkennt den Ort; das Notizbuch faellt, er hebt es steif aus
//             der Huefte auf, liest, ein Riss oeffnet sich in seinem gemalten Gesicht; er wirft es hinter sich (es wird ein
//             Fund); er spricht zur Bibliothek - Stille (die Klangbetten verstummen); dann geht er durch den Gang zurueck.
//   Zorn      ist er durch die Tuer, stuerzen hinter ihm Buecher aus dem Regal (Sitzung: Haufen am Boden, Klang).
//   Stoerung  wer ihm naeher als 3 m kommt, ihm 2,2 s in das Gesicht leuchtet oder das Notizbuch aufhebt, solange er da ist:
//             der Kopf dreht sich bis fast nach hinten (er hat dich bemerkt), dann kommt er - gemessen, solange man ihn ansieht,
//             schnell, sobald man wegsieht. Ansehen zehrt am Verstand. Bis 2,4 m: Ankuendigung (der Arm entfaltet sich Glied
//             fuer Glied, 1,5 s; ueber 3,8 m weg rettet), dann der Griff ins Gesicht (14 Schaden, viel Verstand), nie toedlich.
//             Danach richtet er die Revers und geht. Wer flieht (16 m, 5 s ohne Sicht), den laesst er.
// Mehrspieler: der Host rechnet alles; die Puppe beim Gast bekommt Zustand und Kanaele (fly: Notizbuch, glow: Stille).
#include "gameplay/kin_l3.hpp"
#include "gameplay/library_l53.hpp"
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
using namespace anim::gm;
constexpr float kNoticeNear = 3.0f;   // so nah, und er bemerkt einen
constexpr double kLightLimit = 2.2;   // so lange Licht im Gesicht (bis 12 m)
constexpr float kGiveUp = 16.0f;      // so weit weg: er laesst ab
constexpr double kLostLimit = 5.0;    // so lange ohne Sicht: er laesst ab
constexpr float kWarnAt = 2.4f;       // Ankuendigung
constexpr float kEscape = 3.8f;       // in der Ankuendigung so weit weg: gerettet
constexpr float kGripReach = 2.6f;    // Reichweite des Griffs (Arm mit drei Gliedern)
constexpr double kWarnTime = 1.5, kGripTime = 1.8;
constexpr double kWalk = 1.45, kHuntSeen = 1.7, kHuntUnseen = 4.0;  // m/s (der Spieler geht 3, rennt 5,7)
constexpr float kGripHit = 14.0f, kGripMind = 70.0f;  // Hit (Health), Whisper (x 0,4 Verstand)
// Ablauf der Szene (s)
constexpr double kFallT = 0.55, kPickT = 2.4, kReadT = 3.2, kThrowT = 1.25, kLieT = 0.5;

struct GentCtl {
    vec3 door, stand;
    float yaw = 0.0f;
    bool ok = false;
    double clock = 0.0, next = 0.0, quick = 1.0;
    int visits = 0, grips = 0, notices = 0;
    bool landed = false, wrath = false;
    float hush = 0.0f;
    int provoke = -1;
    bool hold = false;
    int pose = -1;
    float book = -1.0f, place = 0.0f;  // Haltung fuer Bilder: Phase des Notizbuchs, Abstand vor dem Spieler
};
struct GentAg {
    double lineT = 0.0, whisperT = 0.0, lightT = 0.0, lostT = 0.0, wrathT = -1.0, endT = 0.0;
    float book = 0.0f;  // Phase 0..6 (anim::gm::kBookPhases)
    bool sighed = false, muttered = false, lightSaid = false, seen = false, cracked = false, disturbed = false, pending = false;
    bool creaked = false, said = false, hushed = false;
    int line = 0;
};
GentCtl& gctl(std::shared_ptr<void>& p) {
    if (!p) p = std::make_shared<GentCtl>();
    return *static_cast<GentCtl*>(p.get());
}
template <class A>
GentAg& gag(A& a) {
    if (!a.ext) a.ext = std::make_shared<GentAg>();
    return *static_cast<GentAg*>(a.ext.get());
}
float cueOf(int c) { return lib53::kGentCueBase + (float)c; }
vec3 pileOf(const GentCtl& G) {  // wie Library53::pileAt: rechts von seinem Platz, vor dem Regal
    const vec3 f = dirYaw(G.yaw), l{-f.y, f.x, 0.0f};
    return G.stand + f * 1.6f - l * 0.95f + vec3{0, 0, 1.2f};
}
}  // namespace

void Presence::gentScene(vec3 door, vec3 stand, float standYaw) {
    GentCtl& G = gctl(gent_);
    G.door = door, G.stand = stand, G.yaw = standYaw, G.ok = true;
}

// --- Auftreten: hinten im alten Gang, ausser Sicht ---------------------------------------------------------------------
bool Presence::spawnGentleman(const Context& c, std::vector<Event>& out, bool forced) {
    GentCtl& G = gctl(gent_);
    if (!G.ok || !c.world) return false;
    for (const auto& a : agents_)
        if (a->role == Role::Gentleman && !a->showcase && !a->puppet) return false;  // es gibt nur einen
    const world::Tile* t = c.world->tileIfLoaded(ifloor(G.door.x), ifloor(G.door.y));
    const world::Tile* ts = c.world->tileIfLoaded(ifloor(G.stand.x), ifloor(G.stand.y));
    if (!t || !ts) return false;
    const vec3 p{G.door.x, G.door.y, t->floor};
    if (!forced) {
        if (perceive(c.view, *c.world, p, 2.1f).level > 0.05f) return false;
        if (c.others)
            for (const Other& o : *c.others)
                if (perceive(o.view, *c.world, p, 2.1f).level > 0.05f) return false;
    }
    auto a = std::make_unique<Agent>();
    a->role = Role::Gentleman;
    a->ent = Entity::Gentleman;
    a->initBody(p, std::atan2(G.stand.y - p.y, G.stand.x - p.x), (unsigned)rng_.randint(1, 1 << 30));
    a->field.build(*c.world, ifloor(G.stand.x), ifloor(G.stand.y), 1.9f);
    if (a->field.at(ifloor(p.x), ifloor(p.y)) < 0) {
        log::info("Praesenz: Hausherr - kein Weg vom Gang zu seinem Platz");
        return false;
    }
    a->goal = {G.stand.x, G.stand.y, ts->floor};
    a->hasGoal = true;
    a->state = S_ENTER;
    a->alert = 0.0f;
    gag(*a);
    G.landed = G.wrath = false, G.hush = 0.0f, G.provoke = -1;
    out.push_back({Event::Mimic, Role::Gentleman, p + vec3{0, 0, 1.2f}, cueOf(lib53::GM_DOOR)});
    log::info("Praesenz: Hausherr kommt durch den Gang ({:.1f}, {:.1f}), {:.0f} m vom Spieler", p.x, p.y, hdist(p, c.view.eye));
    agents_.push_back(std::move(a));
    ++G.visits;
    return true;
}

void Presence::regieGentleman(double dt, const Context& c, std::vector<Event>& out) {
    // fort: entfernen - ausserhalb der Wesen-Schleife (B-026)
    std::erase_if(agents_, [&](const auto& a) { return a->role == Role::Gentleman && !a->showcase && !a->puppet && a->state == S_GONE; });
    if (!gent_) return;
    GentCtl& G = gctl(gent_);
    if (!has(Role::Gentleman)) G.hush = std::max(0.0f, G.hush - (float)dt * 0.35f);
    if (!G.ok || !c.kin || !c.kin[world::KIN_GENTLEMAN].on) return;
    const world::KinDef& K = c.kin[world::KIN_GENTLEMAN];
    G.clock += dt * G.quick;
    if (has(Role::Gentleman)) return;
    if (K.count > 0 && G.visits >= K.count) return;  // einmal je Besuch der Ebene (Wiki: eine Geschichte)
    if (G.next <= 0.0) G.next = K.every * rng_.uniform(0.85, 1.25);
    if (G.clock < G.next) return;
    // jemand steht unten in der Halle (nicht auf Galerie oder im Obergeschoss), niemand im alten Gang
    const vec3 eye = c.view.eye;
    const float dS = hdist(eye, G.stand), dD = hdist(eye, G.door);
    const bool hall = std::fabs(eye.z - (float)Player::kEye - G.stand.z) < 1.0f;
    const bool corridor = c.room && c.room->type && c.room->type->key == "eingang";
    const bool ok = !corridor && dD > 14.0f && ((hall && dS > 6.0f && dS < 34.0f) || G.clock > G.next * 2.5);
    if (!ok) return;
    if (!spawnGentleman(c, out, false)) G.next += 4.0;
}

// --- Denken -------------------------------------------------------------------------------------------------------------
void Presence::thinkGentleman(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    GentCtl& G = gctl(gent_);
    GentAg& M = gag(a);
    const double q = dt * G.quick;
    a.stateT += q;
    const vec3 head = a.cre.headPos();
    const vec3 eye = c.view.eye;
    const float hd = hdist(a.pos, eye);
    const bool clear = length(eye - head) < 30.0f && clearSight(*c.world, head, eye);
    const bool seen = a.sight.level > 0.25f;
    const vec3 F = dirYaw(a.yaw);
    auto say = [&](int cue, vec3 at) { out.push_back({Event::Mimic, Role::Gentleman, at, cueOf(cue)}); };
    auto faceTo = [&](float want, float rate) { a.yaw = wrapA(a.yaw + std::clamp(wrapA(want - a.yaw), -rate * (float)q, rate * (float)q)); };
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0, M.said = false; };
    lightReact(a, c, head, 12.0f, dt, 99.0);
    a.glow = 0.0f;
    // Haltung fuer Bilder (Skript): fest in einer Pose, keine Entscheidungen
    if (G.hold) {
        if (G.place > 0.0f) {  // vor den Spieler stellen
            const vec3 lk = nrmF(flat(c.view.look), F);
            const vec3 at = flat(eye) + lk * G.place;
            const world::Tile* t = c.world->tileIfLoaded(ifloor(at.x), ifloor(at.y));
            if (t && t->open()) a.snapBody({at.x, at.y, t->floor}, std::atan2(-lk.y, -lk.x));
            G.place = 0.0f;
        }
        const int ps = G.pose;
        a.look = ps >= S_NOTICE || ps == S_SPEAK, a.lookAt = ps == S_SPEAK ? head + F * 6.0f + vec3{0, 0, 0.8f} : eye;
        a.alert = ps >= S_NOTICE ? 1.0f : (ps == S_SPEAK ? 0.3f : 0.0f);
        a.perceive = ps == S_NOTICE ? 1.0f : 0.0f, a.perceiveAt = eye;
        a.growl = ps == S_READ ? 0.6f : (ps == S_NOTICE ? 0.5f : (ps == S_WARN ? 0.8f : (ps == S_GRIP ? 1.0f : 0.0f)));
        a.plea = ps == S_SPEAK ? 1.0f : 0.0f, a.talk = ps == S_SPEAK || ps == S_NOTICE ? 1.0f : 0.0f;
        a.reach = ps == S_WARN ? 0.55f : (ps == S_GRIP ? 1.0f : 0.0f), a.strike = ps == S_GRIP ? 1.0f : 0.0f;
        a.wave = ps == S_COMPOSE ? 1.0f : 0.0f;
        a.shiver = ps == S_GLANCE ? 1.0f : 0.0f;
        const float bk = G.book >= 0.0f ? G.book : (ps == S_FALL ? 1.5f : ps == S_PICK ? 2.55f : ps == S_READ ? 3.5f : ps == S_THROW ? 4.22f : 0.0f);
        a.fly = std::clamp(bk / kBookPhases, 0.0f, 1.0f);
        a.state = ps >= 0 ? ps : a.state;
        return;
    }
    a.look = false, a.talk = 0.0f, a.plea = 0.0f, a.wave = 0.0f, a.reach = 0.0f, a.strike = 0.0f, a.perceive = 0.0f, a.shiver = 0.0f;
    a.growl = std::max(0.0f, a.growl - (float)q * 0.25f);
    // zum ersten Mal deutlich gesehen: er sieht alles (Entity 18) - es zehrt
    if (!M.seen && clear && seen && hd < 26.0f) {
        M.seen = true;
        out.push_back({Event::FirstSight, Role::Gentleman, head, 1.0f});
        out.push_back({Event::Whisper, Role::Gentleman, head, 6.0f});
    }
    // --- Stoerung: Naehe, Licht im Gesicht, das Notizbuch genommen ----------------------------------------------------------
    const bool scene = a.state <= S_LEAVE;
    if (scene && !M.disturbed) {
        bool trig = false;
        if (hd < kNoticeNear && clear && std::fabs(eye.z - head.z) < 2.5f) trig = true;
        if (a.light > 0.5f && hd < 12.0f) {
            M.lightT += q;
            if (M.lightT > 0.6 && !M.lightSaid) M.lightSaid = true, say(lib53::GM_LIGHT, head), a.growl = std::max(a.growl, 0.4f);
            if (M.lightT > kLightLimit) trig = true;
        }
        if (G.provoke >= 0) trig = true, G.provoke = -1;
        if (trig) M.pending = true;
        // er bringt zu Ende, was er tut (das Buch faellt, er hebt es auf, liest, wirft) - dann wendet er sich dir zu
        const bool busy = a.state >= S_FALL && a.state <= S_THROW;
        if (M.pending && !busy) {
            M.pending = false, M.disturbed = true;
            ++G.notices;
            a.hasGoal = false;
            G.hush = 0.0f;
            setState(S_NOTICE);
            log::info("Praesenz: Hausherr bemerkt Spieler {} ({:.1f} m)", a.target, hd);
        }
    }
    switch (a.state) {
        case S_ENTER: {  // durch den Gang und die alte Tuer bis an seinen Platz
            a.alert = 0.0f;
            const float dS = hdist(a.pos, G.stand);
            if (!M.creaked && dS < 11.0f) {  // die Bibliothek bemerkt ihn: die Regale knarren
                M.creaked = true;
                say(lib53::GM_CREAK, pileOf(G));
            }
            if (dS > 0.25f && a.hasGoal) {
                if (!walkTo(a, a.field, kWalk * (dS < 1.5f ? 0.6 : 1.0), q, c)) a.hasGoal = false;
            } else {
                a.pos.x = G.stand.x, a.pos.y = G.stand.y;  // genau an seinem Platz (das Notizbuch landet, wo die Karte es will)
                faceTo(G.yaw, 1.4f);
                if (std::fabs(wrapA(G.yaw - a.yaw)) < 0.03f) a.yaw = G.yaw, setState(S_GLANCE);
            }
            break;
        }
        case S_GLANCE: {  // sieht sich um, seufzt, erkennt den Ort
            if (!M.sighed && a.stateT > 0.8) M.sighed = true, say(lib53::GM_SIGH, head);
            a.shiver = a.stateT > 0.8 && a.stateT < 2.4 ? 1.0f : 0.0f;
            if (!M.muttered && a.stateT > 2.8) M.muttered = true, M.lineT = 3.0, say(lib53::GM_MUTTER, head);
            a.talk = a.stateT > 2.8 && a.stateT < 5.0 ? 1.0f : 0.0f;
            if (a.stateT > 5.6) setState(S_FALL), M.book = 0.001f;
            break;
        }
        case S_FALL: {  // das Notizbuch faellt aus dem Regal vor seine Fuesse
            M.book = (float)std::min(1.0, 0.001 + a.stateT / kFallT);
            if (a.stateT >= kFallT && !M.said) {
                M.said = true, M.book = 1.5f;
                const vec3 f = F, l{-f.y, f.x, 0.0f}, b = bookFeet();
                say(lib53::GM_BOOKDROP, a.pos + f * b.x + l * b.y + vec3{0, 0, 0.05f});
            }
            if (a.stateT > kFallT) M.book = 1.5f;
            if (a.stateT > kFallT + 1.1) setState(S_PICK);
            break;
        }
        case S_PICK: {  // steif aus der Huefte, der Arm entfaltet sich zum Boden
            M.book = 2.0f + (float)std::min(1.0, a.stateT / kPickT);
            if (a.stateT >= kPickT) setState(S_READ), M.book = 3.0f;
            break;
        }
        case S_READ: {  // liest; der Aerger zeigt sich nur als Riss im gemalten Gesicht
            M.book = 3.0f + (float)std::min(0.999, a.stateT / kReadT);
            if (std::fmod(a.stateT, 1.3) < q && a.stateT > 0.4) say(lib53::GM_PAGES, a.cre.headPos() - vec3{0, 0, 0.4f});
            if (a.stateT > kReadT * 0.7) {
                a.growl = 0.65f;
                if (!M.cracked) M.cracked = true, say(lib53::GM_CRACK, head);
            }
            if (a.stateT >= kReadT) setState(S_THROW), M.book = 4.0f;
            break;
        }
        case S_THROW: {  // ueber die Schulter hinter sich, ohne hinzusehen
            const double u = a.stateT / kThrowT;
            M.book = 4.0f + (float)std::min(1.0, u);
            a.growl = 0.4f;
            if (u > 0.2 && !M.said) M.said = true, say(lib53::GM_THROW, a.cre.headPos() + vec3{0, 0, 0.1f});
            if (u >= 1.0) {
                if (!G.landed) {
                    G.landed = true;
                    const vec3 l{-F.y, F.x, 0.0f}, b = bookLand();
                    say(lib53::GM_LAND, a.pos + F * b.x + l * b.y + vec3{0, 0, 0.05f});
                    log::info("Praesenz: Hausherr wirft das Notizbuch");
                }
                M.book = 5.5f;  // liegt hinter ihm (die Sitzung legt dort den Fund hin, sobald es aus seinem Bild verschwindet)
            }
            if (a.stateT >= kThrowT + kLieT) M.book = 0.0f, setState(S_SPEAK);
            break;
        }
        case S_SPEAK: {  // spricht zur Bibliothek; Stille ist die einzige Antwort
            a.alert = 0.3f;
            a.look = true, a.lookAt = head + F * 8.0f + vec3{0, 0, 1.2f};
            const double t = a.stateT;
            static const double kAt[3] = {0.5, 6.4, 16.4};
            static const double kLen[3] = {5.3, 4.8, 5.3};
            for (int i = 0; i < 3; ++i)
                if (M.line == i && t >= kAt[i]) {
                    say(i == 0 ? lib53::GM_SPEAK0 : (i == 1 ? lib53::GM_SPEAK1 : lib53::GM_SPEAK2), head);
                    ++M.line;
                }
            bool talking = false;
            for (int i = 0; i < 3; ++i) talking = talking || (t >= kAt[i] && t < kAt[i] + kLen[i]);
            a.talk = talking ? 1.0f : 0.0f;
            a.plea = t > 0.3 && t < 22.0 ? 1.0f : 0.0f;
            if (!M.hushed && t > 11.6) M.hushed = true, say(lib53::GM_HUSH, head);
            G.hush = t > 11.4 && t < 16.2 ? 1.0f : 0.4f;
            if (t > 22.6) {
                setState(S_LEAVE);
                G.hush = 0.4f;
            }
            break;
        }
        case S_LEAVE: {  // dreht sich um und geht durch den Gang zurueck; hinter ihm stuerzen Buecher
            a.alert = 0.0f;
            if (a.stateT < q * 1.5 || !a.hasGoal) {
                a.field.build(*c.world, ifloor(G.door.x), ifloor(G.door.y), 1.9f);
                a.goal = G.door, a.hasGoal = a.field.valid();
            }
            const float want = std::atan2(G.door.y - a.pos.y, G.door.x - a.pos.x);
            if (a.stateT < 2.0 && std::fabs(wrapA(want - a.yaw)) > 0.6f && hdist(a.pos, G.stand) < 1.0f) {
                faceTo(want, 1.6f);  // erst umdrehen (gemessen), dann gehen
            } else if (a.hasGoal && hdist(a.pos, G.door) > 0.6f) {
                walkTo(a, a.field, kWalk, q, c);
            } else {
                M.endT += q;
                // am Ende des Gangs: fort, sobald ihn niemand ansieht - sonst flackert das Licht, und er ist fort
                if ((a.unseenT > 0.3 && !seen) || M.endT > 10.0) {
                    if (M.endT > 10.0) out.push_back({Event::Flicker, Role::Gentleman, a.pos + vec3{0, 0, 2.0f}, 1.0f});
                    say(lib53::GM_GONE, a.pos + vec3{0, 0, 1.0f});
                    setState(S_GONE);
                    log::info("Praesenz: Hausherr ist fort");
                }
            }
            // fern und ungesehen: er ist einfach nicht mehr da
            if (a.state == S_LEAVE && hdist(a.pos, G.stand) > 10.0f && hd > 24.0f && a.unseenT > 2.0) {
                say(lib53::GM_GONE, G.door + vec3{0, 0, 1.0f});
                setState(S_GONE);
            }
            if (M.wrathT < 0.0 && hdist(a.pos, G.stand) > 9.0f) M.wrathT = 0.0;
            break;
        }
        case S_NOTICE: {  // der Kopf dreht sich bis fast nach hinten, der Koerper bleibt; dann wendet er sich zu
            a.alert = 1.0f, a.perceive = 1.0f, a.perceiveAt = eye, a.growl = std::max(a.growl, 0.5f);
            a.talk = a.stateT > 0.4 && a.stateT < 2.0 ? 1.0f : 0.0f;
            if (!M.said && a.stateT > 0.4) M.said = true, say(lib53::GM_NOTICE, head);
            if (a.stateT > 1.2) faceTo(std::atan2(eye.y - a.pos.y, eye.x - a.pos.x), 1.2f);
            if (a.stateT > 2.4) setState(S_HUNT);
            break;
        }
        case S_HUNT: {  // gemessen, solange man ihn ansieht; schnell, sobald man wegsieht
            a.alert = 1.0f, a.look = true, a.lookAt = eye, a.perceive = 0.6f, a.perceiveAt = eye;
            if (!M.said && a.stateT > 0.6) M.said = true, say(lib53::GM_HUNT, head);
            a.talk = a.stateT > 0.6 && a.stateT < 3.8 ? 1.0f : 0.0f;
            walkTo(a, fld(), seen ? kHuntSeen : kHuntUnseen, q, c);
            M.lostT = clear ? 0.0 : M.lostT + q;
            M.whisperT -= q;
            if (clear && seen && hd < 10.0f && M.whisperT <= 0.0) {  // ihn ansehen zehrt (er sieht dich an)
                M.whisperT = 0.5;
                out.push_back({Event::Whisper, Role::Gentleman, head, 1.25f});
            }
            if (hd < kWarnAt && clear) {
                setState(S_WARN);
                say(lib53::GM_ANNOUNCE, head);
                log::info("Praesenz: Hausherr - Ankuendigung ({:.1f} m)", hd);
            } else if (hd > kGiveUp || M.lostT > kLostLimit) {
                setState(S_COMPOSE);
            }
            break;
        }
        case S_WARN: {  // der Arm entfaltet sich Glied fuer Glied (Ankuendigung)
            a.alert = 1.0f, a.look = true, a.lookAt = eye, a.growl = 0.8f;
            a.reach = 0.55f * (float)std::min(1.0, a.stateT / kWarnTime);
            faceTo(std::atan2(eye.y - a.pos.y, eye.x - a.pos.x), 3.0f);
            if (hd > kEscape || !clear) {
                setState(S_COMPOSE);
            } else if (a.stateT >= kWarnTime) {
                if (hd < kGripReach && c.mode >= 2) {
                    setState(S_GRIP);
                    ++G.grips;
                } else {
                    setState(S_COMPOSE);
                }
            }
            break;
        }
        case S_GRIP: {  // die Hand ins Gesicht, die Finger wachsen
            a.alert = 1.0f, a.look = true, a.lookAt = eye, a.growl = 1.0f, a.reach = 1.0f;
            a.strike = (float)std::min(1.0, a.stateT / 0.35);
            faceTo(std::atan2(eye.y - a.pos.y, eye.x - a.pos.x), 4.0f);
            if (!M.said && a.stateT > 0.3) {
                M.said = true;
                out.push_back({Event::Hit, Role::Gentleman, head, kGripHit});
                out.push_back({Event::Whisper, Role::Gentleman, head, kGripMind});
                say(lib53::GM_GRIP, head);
                log::info("Praesenz: Hausherr - Griff ({:.1f} m)", hd);
            }
            if (a.stateT > kGripTime) setState(S_COMPOSE);
            break;
        }
        case S_COMPOSE: {  // zieht die Revers glatt und geht
            a.alert = 0.6f, a.wave = a.stateT < 2.0 ? 1.0f : 0.0f;
            a.look = a.stateT < 1.0, a.lookAt = eye;
            if (!M.said && a.stateT > 0.3) M.said = true, say(lib53::GM_COMPOSE, head);
            a.talk = a.stateT > 0.3 && a.stateT < 3.0 ? 1.0f : 0.0f;
            if (a.stateT > 3.2) setState(S_LEAVE), a.hasGoal = false;
            break;
        }
        default: break;
    }
    // Zorn der Bibliothek: kaum ist er durch die Tuer, stuerzen hinter ihm Buecher aus dem Regal
    if (M.wrathT >= 0.0 && !G.wrath) {
        M.wrathT += q;
        if (M.wrathT > 1.4) {
            G.wrath = true;
            say(lib53::GM_WRATH, pileOf(G));
            log::info("Praesenz: Zorn der Bibliothek");
        }
    }
    a.fly = std::clamp(M.book / kBookPhases, 0.0f, 1.0f);
    a.glow = G.hush;  // Stille (Kanal glow: die Gaeste hoeren sie auch)
}

Presence::GentInfo Presence::gentInfo() const {
    GentInfo r;
    const auto* G = static_cast<const GentCtl*>(gent_.get());
    if (G) r.landed = G->landed, r.wrath = G->wrath, r.visits = G->visits, r.grips = G->grips, r.notices = G->notices, r.hush = G->hush;
    for (const auto& a : agents_) {
        if (a->role != Role::Gentleman || a->showcase) continue;
        r.active = a->state != S_GONE, r.state = a->state, r.pos = a->pos, r.head = a->cre.headPos(), r.target = a->target;
        r.book = a->fly * kBookPhases;
        r.hush = std::max(r.hush, a->glow);
        // Gast: der Zorn folgt aus dem Weg der Puppe (der Host meldet ihn nicht eigens)
        if (a->puppet && G && G->ok && (a->state == S_LEAVE || a->state == S_GONE) && hdist(a->pos, G->stand) > 10.0f) r.wrath = true;
        break;
    }
    return r;
}

void Presence::gentProvoke(int player) {
    if (!gent_) return;
    gctl(gent_).provoke = std::max(0, player);
}

bool Presence::gentSet(const std::string& key, double v) {
    GentCtl& G = gctl(gent_);
    if (key == "quick") return G.quick = std::clamp(v, 0.1, 20.0), true;
    if (key == "next") return G.next = G.clock + std::max(0.0, v), true;
    if (key == "hold") return G.hold = v > 0.5, true;
    if (key == "pose") return G.pose = (int)v, G.hold = v >= 0.0, true;
    if (key == "book") return G.book = (float)v, true;
    if (key == "place") return G.place = (float)v, true;
    if (key == "provoke") return G.provoke = (int)v, true;
    for (auto& a : agents_) {
        if (a->role != Role::Gentleman || a->showcase || a->puppet) continue;
        if (key == "stand" && G.ok) {  // Bilder: sofort an seinem Platz (Blick wie in der Karte), Zustand v
            a->snapBody(G.stand, G.yaw);
            a->hasGoal = false;
            return a->state = (int)v, a->stateT = 0.0, true;
        }
        if (key == "state") return a->state = (int)v, a->stateT = 0.0, true;
        if (key == "yaw") return a->yaw = (float)v, true;
    }
    return false;
}

}  // namespace lim::game
