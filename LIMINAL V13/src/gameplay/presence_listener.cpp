// V13.3 Teil 4 Level 188: der Horcher (KREATUR_Horcher.md; Wikidot level-188 Rev. 69 - CC BY-SA 3.0).
//
// Wiki (Level 188, eigene Worte): In den weissen, grell beleuchteten Gaengen um den Hof wohnen ungebetene Bewohner - Facelings
// im rosa Kleid, Skin-Stealer "und andere boesartige menschenaehnliche Lebensformen" (ohne Beschreibung). Die meisten Tueren
// sind von innen verschlossen. Log 2: lautes, rhythmisches Haemmern, Rascheln, Knarren, Schritte. Log 4: ein Schatten tritt
// hinter Fred aus einer Tuer. Schutzregel der Seite, wenn man nicht entkommt: Sehen, Hoeren, Beruehren abschirmen, still
// bleiben, auf nichts reagieren.
//
// Umsetzung (Auslegung, im Briefing gekennzeichnet):
//   Klopfen   wer in den Gaengen ist, hoert es hinter einer verschlossenen Tuer klopfen (3-5 Schlaege im Takt), dann lauscht es
//             3,5 s. Wer in der Zeit antwortet - geht, rennt, landet hart, etwas wirft, an die Tuer tritt -, den hat es gehoert:
//             hinter der Tuer saugt es nass, und wenig spaeter geht hinter ihm eine Tuer auf (Zellentuer, ausser Sicht).
//             Wer stillsteht (oder schleicht, weiter als 2,3 m), dem antwortet nur Stille; nach drei stillen Runden laesst es lange ab.
//   Draussen  blind, es hoert: Rennen 28 m, harte Landung 20 m, Gehen 9 m, Schleichen 2,3 m, Stillstehen nichts; Aufprall
//             geworfener Dinge lockt es dorthin (nach Lautstaerke). Es schleicht zum Ort des Geraeuschs (1,3 m/s), bei Laerm jagt
//             es (4,4 m/s - langsamer als ein rennender Mensch). Bis 2,4 m: Ankuendigung - es richtet sich auf, die Faust hoch,
//             drei Schlaege in die Luft (1,3 s) -, dann schlaegt die Faust auf den Ort, an dem es dich zuletzt hoerte. Wer dort
//             nicht mehr steht (mehr als 1 m weg), wird verfehlt; sonst 34 Schaden (nach Schwierigkeit). Ganz nah (unter 0,9 m)
//             ertastet die duenne Hand, wer still steht.
//   Licht     das Ohr ist duenne Haut ueber Knorpel: 0,35 s Lampe darauf (bis 9 m) - es kreischt, das Ohr knittert, 1,6 s haelt
//             es inne (auch in der Ankuendigung: kein Schlag). Nach drei Mal in einer Minute gewoehnt es sich (60 s).
//   Grenzen   nur in Gang, Zellen und Treppenhaus; in den Hof folgt es nicht (es bleibt an der Tuer und lauscht). Nach 25 s Stille
//             oder wenn das Ziel im Hof ist, zieht es sich durch die naechste Zellentuer zurueck (fort, sobald ungesehen).
// Mehrspieler: der Host rechnet alles; die Puppe beim Gast bekommt Zustand und Kanaele (crouch, alert, perceive, shiver, jaw,
// growl, reach, wave, strike, light). Klaenge als Klangnummern (kinNetRoute).
#include <algorithm>
#include <cmath>

#include "core/log.hpp"
#include "gameplay/kin_l3.hpp"
#include "gameplay/listener_l188.hpp"
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

namespace lim::game {

// --- Klaenge und Untertitel ---------------------------------------------------------------------------------------------
namespace l188 {
const KinLine* listenerLine(int cue) {
    static const KinLine k[HC_COUNT] = {
        {"hr_knock_0", "(Klopfen hinter einer verschlossenen Tür – schwer, im Takt.)", 0.95f, 34.0f, 2.4f,
         "Rat der Reisenden: Nicht antworten. Stehen bleiben und still sein, bis das Klopfen aufgehört hat."},
        {"hr_knock_1", nullptr, 0.95f, 34.0f},
        {"hr_knock_2", nullptr, 0.95f, 34.0f},
        {"hr_listen", "(Hinter der Tür: ein nasses Saugen. Etwas lauscht dort, ganz nah am Holz.)", 0.6f, 14.0f, 3.2f},
        {"hr_lost", "(Hinter der Tür entfernt sich etwas. Es hat nichts gehört.)", 0.35f, 10.0f, 2.6f},
        {"hr_door", "(Hinter dir geht eine Tür auf.)", 0.85f, 26.0f, 2.4f},
        {"hr_unfold", nullptr, 0.6f, 16.0f},
        {"hr_step_0", nullptr, 0.55f, 20.0f},
        {"hr_step_1", nullptr, 0.55f, 20.0f},
        {"hr_step_2", nullptr, 0.55f, 20.0f},
        {"hr_step_3", nullptr, 0.55f, 20.0f},
        {"hr_breath", nullptr, 0.45f, 12.0f},
        {"hr_hum", nullptr, 0.5f, 16.0f},
        {"hr_heard", "(Ein nasses Schnalzen. Das Ohr schnappt herum.)", 0.75f, 18.0f, 2.2f},
        {"hr_rush", nullptr, 0.95f, 30.0f},
        {"hr_warn", "(Drei Schläge, ganz nah. Dann zischt es.)", 1.0f, 22.0f, 2.2f,
         "Weg von der Faust – sie schlägt dorthin, wo es dich zuletzt gehört hat."},
        {"hr_strike", nullptr, 1.0f, 18.0f},
        {"hr_miss", nullptr, 0.95f, 26.0f},
        {"hr_flinch", "(Ein Kreischen – das Licht tut dem Ohr weh.)", 0.95f, 24.0f, 2.0f},
        {"hr_gone", nullptr, 0.5f, 22.0f},
        {"hr_scrape", nullptr, 0.55f, 14.0f},
    };
    if (cue < 0 || cue >= HC_COUNT) return nullptr;
    return k[cue].sound ? &k[cue] : nullptr;
}
const KinLine* listenerLineBySound(const std::string& s) {
    for (int i = 0; i < HC_COUNT; ++i)
        if (const KinLine* L = listenerLine(i); L && L->sound && s == L->sound) return L;
    return nullptr;
}
}  // namespace l188

namespace {
using namespace anim::hr;
using namespace l188;
constexpr double kListenWin = 3.5;    // s nach dem letzten Schlag: es lauscht
constexpr float kAnswerNoise = 0.4f;  // so laut antwortet man (Gehen 0,45; Schleichen 0,15)
constexpr float kAnswerRange = 16.0f; // so weit von der Tuer zaehlt die Antwort
constexpr float kStalk = 1.3f, kHunt = 4.4f, kEmergeWalk = 0.8f;  // m/s (der Spieler geht 3, rennt 5,7)
constexpr float kWarnAt = 2.4f, kHitRadius = 1.0f, kStrikeReach = 2.7f;
constexpr double kWarnTime = 1.3, kStrikeTime = 0.9, kFlinchTime = 1.6;
constexpr float kStrikeHit = 34.0f;
constexpr float kEmergeDark = 7.0f;    // V13.3 Abnahme Level 188: so lange fallen die Roehren beim Heraustreten aus (s)
constexpr double kQuietLimit = 25.0;   // so lange nichts gehoert: es zieht sich zurueck
constexpr double kLightNeed = 0.35;    // s Lampe auf dem Ohr
constexpr float kLightRange = 9.0f;

struct HrCtl {
    std::vector<vec3> knock, emerge;
    std::vector<float> emergeYaw;
    bool ok = false;
    double clock = 0.0, next = 0.0, quick = 1.0;
    // eine Klopf-Runde: Tuer, Zeitpunkte der Schlaege (s seit Beginn), geschlagen, Ende (letzter Schlag)
    int door = -1, lastDoor = -1, knocksDone = 0, knocksN = 0;
    double seqT = -1.0;
    float at[6] = {0, 0, 0, 0, 0, 0};
    bool answered = false;
    double emergeAt = -1.0, emergeUntil = -1.0;
    vec3 heardAt;  // wo die Antwort herkam
    int sequences = 0, answers = 0, emerges = 0, strikes = 0, flinches = 0, quiet = 0, quietTotal = 0;
    bool hold = false;
    int pose = -1;
    float place = 0.0f;
};
struct HrAg {
    double heardT = 99.0, quietT = 0.0, breathT = 1.5, humT = 3.0, lightT = 0.0, lightCool = 0.0, flinchWin = 0.0, fieldT = 0.0;
    double hofT = 0.0, scrapeT = 0.0, touchT = 0.0;
    int flinchN = 0, prevState = S_LISTEN, home = -1;
    vec3 last, fieldAt{1e9f, 1e9f, 0.0f}, exitTo;
    bool said = false, struck = false, hit = false, fieldOk = false, warned = false;
};
HrCtl& hctl(std::shared_ptr<void>& p) {
    if (!p) p = std::make_shared<HrCtl>();
    return *static_cast<HrCtl*>(p.get());
}
template <class A>
HrAg& hag(A& a) {
    if (!a.ext) a.ext = std::make_shared<HrAg>();
    return *static_cast<HrAg*>(a.ext.get());
}
float cueOf(int c) { return kListenerCueBase + (float)c; }
// in den Gaengen (Gang, Zellen, Treppenhaus) - dort wohnt es
bool corridorRoom(const world::Room* r) {
    return r && r->type && (r->type->key == "gang" || r->type->key == "zelle" || r->type->key == "treppenhaus");
}
// Tuerkacheln gehoeren keinem Raum (Test: sonst blieb es in der Zellentuer stecken) - nur der Hof ist verboten
bool corridorTile(const world::Tile* t) {
    return t && t->open() && (corridorRoom(t->room) || !t->room || !t->room->type || t->room->type->key != "hof");
}
// so weit hoert es diesen Laerm (0 nichts)
float hearRange(double noise) {
    if (noise >= 0.95) return 28.0f;
    if (noise >= 0.75) return 20.0f;
    if (noise >= 0.4) return 9.0f;
    if (noise >= 0.12) return 2.3f;
    return 0.0f;
}
}  // namespace

void Presence::listenerSpots(const std::vector<vec3>& knock, const std::vector<vec3>& emerge, const std::vector<float>& emergeYaw) {
    HrCtl& H = hctl(lis_);
    if (H.ok) return;
    H.knock = knock, H.emerge = emerge, H.emergeYaw = emergeYaw;
    H.ok = !knock.empty() && !emerge.empty() && emerge.size() == emergeYaw.size();
}

// --- Auftreten: aus einer Zellentuer hinter dem Ziel, ausser Sicht ---------------------------------------------------------
bool Presence::spawnListener(const Context& c, std::vector<Event>& out, bool forced) {
    HrCtl& H = hctl(lis_);
    if (!H.ok || !c.world) return false;
    for (const auto& a : agents_)
        if (a->role == Role::Listener && !a->showcase && !a->puppet) return false;  // es gibt nur einen
    const vec3 feet = c.player ? vec3{(float)c.player->x, (float)c.player->y, (float)c.player->z} : c.view.eye - vec3{0, 0, 1.6f};
    const vec3 look = nrmF(flat(c.view.look), {1, 0, 0});
    int best = -1;
    float bestScore = -1e9f;
    for (size_t i = 0; i < H.emerge.size(); ++i) {
        const vec3 p = H.emerge[i];
        const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        if (!t || !t->open() || std::fabs(t->floor - feet.z) > 1.6f) continue;
        const float d = hdist(p, feet);
        if (!forced && (d < 4.0f || d > 18.0f)) continue;
        if (!forced) {
            if (perceive(c.view, *c.world, {p.x, p.y, t->floor}, 2.0f).level > 0.05f) continue;
            if (c.others) {
                bool seen = false;
                for (const Other& o : *c.others) seen = seen || perceive(o.view, *c.world, {p.x, p.y, t->floor}, 2.0f).level > 0.05f;
                if (seen) continue;
            }
        }
        // lieber hinter dem Ziel (Log 4: hinter ihm trat einer aus einer Tuer), nicht zu nah, nicht zu fern
        const float behind = -dot(nrmF(flat(p - feet), look), look);
        const float score = behind * 4.0f - std::fabs(d - 8.0f) * 0.4f + (forced ? -d * 0.2f : 0.0f);
        if (score > bestScore) bestScore = score, best = (int)i;
    }
    if (best < 0) return false;
    const vec3 p = H.emerge[(size_t)best];
    const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
    auto a = std::make_unique<Agent>();
    a->role = Role::Listener;
    a->ent = Entity::Listener;
    a->initBody({p.x, p.y, t->floor}, H.emergeYaw[(size_t)best], (unsigned)rng_.randint(1, 1 << 30));
    a->state = S_EMERGE;
    a->crouch = 1.0f, a->alert = 0.5f;
    a->heard = H.heardAt;
    HrAg& M = hag(*a);
    M.home = best;
    M.last = H.heardAt;
    M.heardT = 0.5;
    M.exitTo = vec3{p.x, p.y, t->floor} + dirYaw(H.emergeYaw[(size_t)best]) * 2.2f;
    out.push_back({Event::Mimic, Role::Listener, p + dirYaw(H.emergeYaw[(size_t)best]) * 0.9f + vec3{0, 0, 1.1f}, cueOf(HC_DOOR)});
    out.push_back({Event::Appear, Role::Listener, p});
    // V13.3 Abnahme Level 188 (Auslegung, KREATUR_Horcher.md): die Gaenge sind grell (Seite) - beim ersten Heraustreten eines
    // Besuchs, spaeter in 2 von 5 Faellen, stottern die Roehren und fallen einige Sekunden aus: hinter dir geht die Tuer auf,
    // dann wird es dunkel, und es entfaltet sich im Halbdunkel (Geraeusch vor Sicht; die Lampe zeigt zuerst das Ohr). Es ist
    // blind - das Dunkel hilft ihm nicht, es nimmt nur dem Spieler die Sicherheit des Lichts.
    // (Dauer negativ: der ganze Stromkreis auf einmal - der Gang ist 24 m lang und galt sonst als Halle mit Welle)
    if (H.emerges == 0 || rng_.uniform(0.0, 1.0) < 0.4) out.push_back({Event::Blackout, Role::Listener, c.view.eye, -kEmergeDark});
    log::info("Praesenz: Horcher tritt aus einer Tuer ({:.1f}, {:.1f}), {:.0f} m vom Spieler", p.x, p.y, hdist(p, feet));
    agents_.push_back(std::move(a));
    ++H.emerges;
    H.emergeAt = H.emergeUntil = -1.0;
    return true;
}

// --- Regie: Klopfen hinter den verschlossenen Tueren ------------------------------------------------------------------------
void Presence::regieListener(double dt, const Context& c, std::vector<Event>& out) {
    // fort: entfernen - ausserhalb der Wesen-Schleife (B-026)
    std::erase_if(agents_, [&](const auto& a) { return a->role == Role::Listener && !a->showcase && !a->puppet && a->state == S_GONE; });
    if (!lis_) return;
    HrCtl& H = hctl(lis_);
    if (!H.ok || !c.kin || !c.kin[world::KIN_LISTENER].on || !c.world || !c.player) return;
    const world::KinDef& K = c.kin[world::KIN_LISTENER];
    H.clock += dt * H.quick;
    if (has(Role::Listener)) {
        H.seqT = -1.0, H.door = -1;
        return;
    }
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    // nur wer in den Gaengen ist, hoert es klopfen; im Hof bricht die Runde ab
    if (!corridorRoom(c.room)) {
        if (H.seqT >= 0.0) H.seqT = -1.0, H.door = -1;
        H.emergeAt = -1.0;
        return;
    }
    // Takt: andere Bewohner der Gaenge zuerst (Faceling, Skin-Stealer) - sonst draengen sich die Gefahren
    for (const auto& a : agents_)
        if (!a->showcase && !a->puppet && (a->role == Role::Faceling || a->role == Role::SkinStealer) && hdist(a->pos, feet) < 18.0f) return;
    // gehoert: es tritt heraus (Versuche, bis eine Tuer hinter dem Ziel ungesehen ist)
    if (H.emergeAt >= 0.0) {
        if (H.clock >= H.emergeAt) {
            if (!spawnListener(c, out, false)) {
                H.emergeAt = H.clock + 0.5;
                if (H.emergeUntil >= 0.0 && H.clock > H.emergeUntil) {
                    log::info("Praesenz: Horcher - keine ungesehene Tuer, es bleibt drinnen");
                    H.emergeAt = H.emergeUntil = -1.0;
                    H.next = H.clock + K.every * 0.5;
                }
            }
        }
        return;
    }
    // eine Runde beginnen: die Tuer naechst 7 m vom Ziel (2,5 .. 15 m, gleiche Etage, nicht dieselbe wie eben)
    if (H.seqT < 0.0) {
        if (H.next <= 0.0) H.next = H.clock + K.every * rng_.uniform(0.25, 0.45);
        if (H.clock < H.next) return;
        int best = -1;
        float bs = 1e9f;
        for (size_t i = 0; i < H.knock.size(); ++i) {
            const vec3 d = H.knock[i];
            const float hd = hdist(d, feet);
            if (hd < 2.5f || hd > 15.0f || std::fabs(d.z - feet.z) > 1.6f) continue;
            const float s = std::fabs(hd - 7.0f) + ((int)i == H.lastDoor ? 6.0f : 0.0f) + (float)rng_.uniform(0.0, 2.0);
            if (s < bs) bs = s, best = (int)i;
        }
        if (best < 0) {
            H.next = H.clock + 4.0;
            return;
        }
        H.door = H.lastDoor = best;
        H.seqT = 0.0, H.knocksDone = 0, H.answered = false;
        // Takt: drei Muster (gleichmaessig, zwei und drei, schleppend)
        static const float kPat[3][5] = {{0.0f, 0.55f, 1.1f, 1.65f, -1}, {0.0f, 0.4f, 1.35f, 1.75f, 2.15f}, {0.0f, 0.9f, 1.8f, -1, -1}};
        const int pat = (int)rng_.randint(0, 2);
        H.knocksN = 0;
        for (int k = 0; k < 5; ++k)
            if (kPat[pat][k] >= 0.0f) H.at[H.knocksN++] = kPat[pat][k] * (float)rng_.uniform(0.95, 1.08);
        ++H.sequences;
        log::info("Praesenz: Horcher klopft an Tuer {} ({:.1f}, {:.1f}), {:.1f} m vom Spieler, {} Schlaege", best, H.knock[(size_t)best].x,
                  H.knock[(size_t)best].y, hdist(H.knock[(size_t)best], feet), H.knocksN);
    }
    // die Runde: Schlaege, dann lauschen
    const vec3 door = H.knock[(size_t)H.door];
    H.seqT += dt * H.quick;
    while (H.knocksDone < H.knocksN && H.seqT >= H.at[H.knocksDone]) {
        out.push_back({Event::Mimic, Role::Listener, door + vec3{0, 0, 1.3f}, cueOf(HC_KNOCK0 + H.knocksDone % 3)});
        ++H.knocksDone;
    }
    // Antwort: Laerm des Ziels in Hoerweite der Tuer, an die Tuer treten, ein Aufprall nahe der Tuer
    if (!H.answered) {
        const float hd = hdist(door, feet);
        bool ans = (c.noise >= kAnswerNoise && hd < kAnswerRange) || hd < 1.3f;
        vec3 from = feet;
        if (c.pings)
            for (const auto& pg : *c.pings)
                if (hdist(pg.first, door) < 6.0f + 10.0f * pg.second) ans = true, from = pg.first;
        if (ans) {
            H.answered = true;
            H.heardAt = from;
            ++H.answers;
            H.quiet = 0;
            out.push_back({Event::Mimic, Role::Listener, door + vec3{0, 0, 1.2f}, cueOf(HC_LISTEN)});
            H.emergeAt = H.clock + rng_.uniform(2.4, 3.6);
            H.emergeUntil = H.emergeAt + 8.0;
            H.seqT = -1.0, H.door = -1;
            H.next = H.clock + K.every * rng_.uniform(0.5, 0.8);
            log::info("Praesenz: Horcher hat eine Antwort gehoert ({:.1f} m von der Tuer, Laerm {:.2f})", hd, c.noise);
            return;
        }
    }
    const float last = H.knocksN > 0 ? H.at[H.knocksN - 1] : 0.0f;
    if (H.seqT > last + kListenWin) {  // Stille: es laesst ab
        out.push_back({Event::Mimic, Role::Listener, door + vec3{0, 0, 1.0f}, cueOf(HC_LOST)});
        ++H.quiet, ++H.quietTotal;
        H.seqT = -1.0, H.door = -1;
        H.next = H.clock + K.every * (H.quiet >= 3 ? 1.6 : rng_.uniform(0.3, 0.5));
        if (H.quiet >= 3) H.quiet = 0;
        log::info("Praesenz: Horcher - keine Antwort, es laesst ab (still: {})", H.quietTotal);
    }
}

// --- Denken -------------------------------------------------------------------------------------------------------------
void Presence::thinkListener(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    HrCtl& H = hctl(lis_);
    HrAg& M = hag(a);
    const double q = dt * H.quick;
    a.stateT += q;
    const vec3 head = a.cre.headPos();
    const vec3 eye = c.view.eye;
    const vec3 feet = c.player ? vec3{(float)c.player->x, (float)c.player->y, (float)c.player->z} : eye - vec3{0, 0, 1.6f};
    const float hd = hdist(a.pos, feet);
    const vec3 F = dirYaw(a.yaw);
    auto say = [&](int cue, vec3 at) { out.push_back({Event::Mimic, Role::Listener, at, cueOf(cue)}); };
    auto faceTo = [&](vec3 p, float rate) {
        const float want = std::atan2(p.y - a.pos.y, p.x - a.pos.x);
        a.yaw = wrapA(a.yaw + std::clamp(wrapA(want - a.yaw), -rate * (float)q, rate * (float)q));
    };
    auto setState = [&](int s) {
        log::info("Praesenz: Horcher Zustand {} -> {} ({:.1f} m)", a.state, s, hd);
        if (a.state != S_FLINCH) M.prevState = a.state;
        a.state = s, a.stateT = 0.0, M.said = false;
    };
    // ein Stueck zum Ziel p (eigenes Feld, alle 0,5 s neu, wenn sich das Ziel verschoben hat); nie hinaus in den Hof
    auto moveTo = [&](vec3 p, float speed) -> bool {
        M.fieldT -= q;
        if (!M.fieldOk || (hdist(p, M.fieldAt) > 1.2f && M.fieldT <= 0.0)) {
            a.field.build(*c.world, ifloor(p.x), ifloor(p.y), 1.9f);
            M.fieldAt = p, M.fieldT = 0.5, M.fieldOk = a.field.valid() && a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) >= 0;
        }
        if (!M.fieldOk) return false;
        const vec3 before = a.pos;
        const float yaw0 = a.yaw;
        if (!walkTo(a, a.field, speed, q, c)) return false;
        if (!corridorTile(c.world->tileIfLoaded(ifloor(a.pos.x), ifloor(a.pos.y)))) {  // nicht in den Hof
            a.pos = before, a.yaw = yaw0;
            return false;
        }
        return true;
    };
    lightReact(a, c, head, kLightRange, dt, 99.0);
    // Haltung fuer Bilder (Skript): fest in einer Pose, keine Entscheidungen
    if (H.hold) {
        if (H.place > 0.0f) {  // vor den Spieler stellen
            const vec3 lk = nrmF(flat(c.view.look), F);
            const vec3 at = flat(eye) + lk * H.place;
            const world::Tile* t = c.world->tileIfLoaded(ifloor(at.x), ifloor(at.y));
            if (t && t->open()) a.snapBody({at.x, at.y, t->floor}, std::atan2(-lk.y, -lk.x));
            H.place = 0.0f;
        }
        const int ps = H.pose >= 0 ? H.pose : a.state;
        a.state = ps;
        a.heard = feet + vec3{0, 0, 1.0f};
        a.perceive = ps == S_LISTEN || ps == S_STALK || ps == S_WARN ? 1.0f : 0.0f, a.perceiveAt = eye;
        a.crouch = ps == S_EMERGE ? 1.0f : (ps == S_STALK ? 0.6f : (ps == S_HUNT ? 0.25f : 0.0f));
        a.alert = ps == S_LISTEN || ps == S_EMERGE ? 0.5f : 1.0f;
        a.shiver = ps == S_LISTEN ? 1.0f : 0.0f;
        a.growl = ps == S_HUNT || ps == S_WARN ? 0.8f : 0.0f, a.jaw = ps == S_STRIKE ? 1.0f : 0.0f;
        a.reach = ps == S_WARN || ps == S_STRIKE ? 1.0f : 0.0f;
        a.wave = ps == S_WARN ? 1.0f : 0.0f;
        a.strike = ps == S_STRIKE ? (float)std::fmod(a.age * 0.6, 1.0) : 0.0f;
        if (ps == S_FLINCH) a.light = 1.0f, a.lightFrom = eye;
        return;
    }
    // --- hoeren: das Ziel (Laerm nach Abstand), Aufpralle; ganz nah ertastet die Hand, wer still steht -------------------------
    M.heardT += q;
    M.lightCool = std::max(0.0, M.lightCool - q);
    M.flinchWin = std::max(0.0, M.flinchWin - q);
    if (M.flinchWin <= 0.0) M.flinchN = 0;
    bool heardNow = false;
    float loud = 0.0f;
    const bool alive = c.player != nullptr;  // Ziel ist immer ein lebender Spieler (die Praesenz waehlt nur Lebende)
    if (alive && a.state != S_EMERGE && a.state != S_RETREAT && std::fabs(feet.z - a.pos.z) < 2.0f) {
        const float r = hearRange(c.noise);
        if (r > 0.0f && hd < r) heardNow = true, loud = (float)c.noise, M.last = feet;
        if (!heardNow && hd < 0.9f && a.state >= S_LISTEN && a.state <= S_HUNT) {  // die Hand ertastet ihn
            M.touchT += q;
            if (M.touchT > 0.4) heardNow = true, loud = 0.5f, M.last = feet;
        } else {
            M.touchT = 0.0;
        }
    }
    if (c.pings && a.state != S_EMERGE && a.state != S_RETREAT)
        for (const auto& pg : *c.pings)
            if (hdist(pg.first, a.pos) < 30.0f * pg.second && pg.second > loud * 0.5f) heardNow = true, loud = std::max(loud, pg.second), M.last = pg.first;
    if (heardNow) {
        M.heardT = 0.0, M.quietT = 0.0;
        a.heard = M.last + vec3{0, 0, 1.0f};
    } else {
        M.quietT += q;
    }
    // --- Licht auf dem Ohr ---------------------------------------------------------------------------------------------
    if (a.light > 0.5f && a.state >= S_LISTEN && a.state <= S_WARN && M.lightCool <= 0.0) {
        M.lightT += q;
        if (M.lightT > kLightNeed) {
            M.lightT = 0.0;
            ++M.flinchN, ++H.flinches;
            M.flinchWin = 60.0;
            M.lightCool = M.flinchN >= 3 ? 60.0 : 2.5;  // dreimal in einer Minute: es gewoehnt sich
            say(HC_FLINCH, head);
            setState(S_FLINCH);
        }
    } else if (a.light < 0.5f) {
        M.lightT = 0.0;
    }
    // Ziel im Hof: es folgt nicht
    M.hofT = corridorRoom(c.room) ? 0.0 : M.hofT + q;
    a.look = false, a.wave = 0.0f, a.reach = 0.0f, a.jaw = 0.0f;
    a.shiver = std::max(0.0f, a.shiver - (float)q * 2.0f);
    a.growl = std::max(0.0f, a.growl - (float)q * 0.6f);
    if (a.state != S_STRIKE) a.strike = 0.0f;
    switch (a.state) {
        case S_EMERGE: {  // aus der Zellentuer heraus, tief geduckt, das Ohr voran
            a.crouch = 1.0f, a.alert = 0.6f, a.perceive = 0.5f, a.perceiveAt = M.last + vec3{0, 0, 1.0f};
            if (!M.said && a.stateT > 0.2) M.said = true, say(HC_UNFOLD, a.pos + F * 0.6f + vec3{0, 0, 0.8f});
            if (a.stateT > 0.6) moveTo(M.exitTo, kEmergeWalk);
            if (hdist(a.pos, M.exitTo) < 0.4f || a.stateT > 4.5) setState(S_LISTEN);
            break;
        }
        case S_LISTEN: {  // steht, das Ohr schwenkt; der Rand zittert
            a.crouch = 0.15f, a.alert = 0.5f;
            a.perceive = M.heardT < 4.0 ? 0.8f : 0.0f, a.perceiveAt = M.last + vec3{0, 0, 1.0f};
            a.shiver = 1.0f;
            M.breathT -= q;
            if (M.breathT <= 0.0) M.breathT = rng_.uniform(2.2, 3.4), say(HC_BREATH, head);
            if (heardNow && a.stateT > 0.5) {
                say(HC_HEARD, head);
                setState(loud >= 0.75f || hd < 4.0f ? S_HUNT : S_STALK);
                if (a.state == S_HUNT) say(HC_RUSH, head);
            } else if (M.quietT > kQuietLimit || M.hofT > 5.0 || !alive) {
                setState(S_RETREAT);
            }
            break;
        }
        case S_STALK: {  // zum Ort des Geraeuschs, geduckt; es brummt leise
            a.crouch = 0.6f, a.alert = 0.85f, a.perceive = 1.0f, a.perceiveAt = M.last + vec3{0, 0, 1.0f};
            a.growl = std::max(a.growl, 0.25f);
            M.humT -= q;
            if (M.humT <= 0.0) M.humT = rng_.uniform(2.6, 4.0), say(HC_HUM, head);
            const float dL = hdist(a.pos, M.last);
            if (dL > 0.8f) {
                if (!moveTo(M.last, kStalk)) faceTo(M.last, 2.0f);
            } else {  // am Ort: die Faust tastet und klopft kurz auf den Boden
                a.wave = 0.6f;
                M.scrapeT -= q;
                if (M.scrapeT <= 0.0) M.scrapeT = 1.6, say(HC_SCRAPE, a.pos + F * 0.9f);
                faceTo(M.last + F, 1.0f);
            }
            if (heardNow && loud >= 0.75f) setState(S_HUNT), say(HC_RUSH, head);
            else if (M.heardT < 1.2 && hdist(a.pos, M.last) < kWarnAt && alive && hd < kWarnAt + 0.6f) setState(S_WARN), say(HC_WARN, head);
            else if (M.quietT > 6.0 && dL <= 0.8f) setState(S_LISTEN);
            else if (M.hofT > 2.0) setState(S_LISTEN);
            break;
        }
        case S_HUNT: {  // Laerm: auf drei Stuetzen heran, pfeifend
            a.crouch = 0.25f, a.alert = 1.0f, a.perceive = 1.0f, a.perceiveAt = M.last + vec3{0, 0, 1.0f}, a.growl = 0.7f;
            const float dL = hdist(a.pos, M.last);
            if (dL > 0.6f && !moveTo(M.last, kHunt)) faceTo(M.last, 3.0f);
            if (M.heardT < 1.0 && hd < kWarnAt && alive) setState(S_WARN), say(HC_WARN, head);
            else if (M.quietT > 2.0 && dL < 1.0f) setState(S_STALK);
            else if (M.hofT > 1.0) setState(S_LISTEN);
            break;
        }
        case S_WARN: {  // richtet sich auf, die Faust hoch, drei Schlaege in die Luft (Ankuendigung)
            a.crouch = 0.0f, a.alert = 1.0f, a.reach = 1.0f, a.growl = 0.9f, a.perceive = 1.0f, a.perceiveAt = M.last + vec3{0, 0, 1.0f};
            a.wave = a.stateT > 0.15 && a.stateT < 1.05 ? 1.0f : 0.0f;
            faceTo(M.last, 3.5f);
            if (a.stateT >= kWarnTime) {
                // die Faust schlaegt dorthin, wo es das Ziel zuletzt hoerte (wer seitdem leise weg ist, wird verfehlt)
                M.hit = alive && hdist(feet, M.last) < kHitRadius && hd < kStrikeReach && c.mode >= 2;
                a.heard = M.hit ? feet + vec3{0, 0, 1.05f} : M.last + vec3{0, 0, 0.05f};
                M.struck = false;
                setState(S_STRIKE);
            }
            break;
        }
        case S_STRIKE: {  // herab
            a.crouch = 0.0f, a.alert = 1.0f, a.reach = 1.0f, a.jaw = 0.8f, a.growl = 1.0f;
            a.strike = (float)std::min(1.0, a.stateT / kStrikeTime);
            if (!M.struck && a.stateT > kStrikeTime * 0.36) {
                M.struck = true;
                ++H.strikes;
                if (M.hit) {
                    out.push_back({Event::Hit, Role::Listener, head, kStrikeHit});
                    say(HC_STRIKE, feet + vec3{0, 0, 1.0f});
                    log::info("Praesenz: Horcher - Schlag trifft ({:.1f} m)", hd);
                } else {
                    say(HC_MISS, vec3{a.heard.x, a.heard.y, a.pos.z + 0.05f});
                    log::info("Praesenz: Horcher - Schlag verfehlt ({:.1f} m vom Ort)", hdist(feet, M.last));
                }
            }
            if (a.stateT >= kStrikeTime + 0.5) setState(S_LISTEN), M.quietT = 0.0;
            break;
        }
        case S_FLINCH: {  // das Licht brennt auf dem Ohr: es haelt inne, kreischt, dreht das Ohr weg
            a.crouch = 0.45f, a.alert = 1.0f, a.light = 1.0f, a.lightFrom = c.view.flashPos;
            if (a.stateT >= kFlinchTime) {
                const int back = M.prevState == S_WARN || M.prevState == S_STRIKE ? S_LISTEN : M.prevState;
                a.state = back, a.stateT = 0.0, M.said = false;
                log::info("Praesenz: Horcher erholt sich vom Licht -> {}", back);
            }
            break;
        }
        case S_RETREAT: {  // durch die naechste Zellentuer zurueck
            a.crouch = 0.7f, a.alert = 0.3f, a.perceive = 0.0f;
            if (M.home < 0 || M.home >= (int)H.emerge.size()) M.home = 0;
            if (a.stateT < q * 1.5) {  // die naechste Tuer
                float bd = 1e9f;
                for (size_t i = 0; i < H.emerge.size(); ++i)
                    if (const float d = hdist(H.emerge[i], a.pos); d < bd && std::fabs(H.emerge[i].z - a.pos.z) < 1.6f) bd = d, M.home = (int)i;
                M.fieldOk = false;
            }
            const vec3 home = H.emerge[(size_t)M.home];
            if (hdist(a.pos, home) > 0.5f) moveTo(home, kStalk);
            else faceTo(home + dirYaw(H.emergeYaw[(size_t)M.home]), 2.0f);
            const bool atDoor = hdist(a.pos, home) <= 0.6f;
            if ((atDoor && a.unseenT > 0.5) || a.stateT > 14.0 || (a.unseenT > 3.0 && hd > 12.0f)) {
                say(HC_GONE, home + dirYaw(H.emergeYaw[(size_t)M.home]) * 0.9f + vec3{0, 0, 1.0f});
                setState(S_GONE);
                log::info("Praesenz: Horcher ist fort");
            }
            // gehoert auf dem Rueckweg: es kehrt um
            if (heardNow && a.state == S_RETREAT && hd < 9.0f && alive) setState(S_STALK), say(HC_HEARD, head);
            break;
        }
        default: break;
    }
}

Presence::ListenerInfo Presence::listenerInfo() const {
    ListenerInfo r;
    const auto* H = static_cast<const HrCtl*>(lis_.get());
    if (H) {
        r.knocking = H->seqT >= 0.0, r.door = H->door, r.listening = H->seqT >= 0.0 && H->knocksDone >= H->knocksN;
        r.sequences = H->sequences, r.answers = H->answers, r.emerges = H->emerges, r.strikes = H->strikes;
        r.flinches = H->flinches, r.quiet = H->quietTotal;
    }
    for (const auto& a : agents_) {
        if (a->role != Role::Listener || a->showcase) continue;
        r.active = a->state != S_GONE, r.state = a->state, r.pos = a->pos, r.head = a->cre.headPos(), r.target = a->target, r.heard = a->heard;
        break;
    }
    return r;
}

bool Presence::listenerSet(const std::string& key, double v) {
    HrCtl& H = hctl(lis_);
    if (key == "quick") return H.quick = std::clamp(v, 0.1, 20.0), true;
    if (key == "next") return H.next = H.clock + std::max(0.0, v), true;
    if (key == "hold") return H.hold = v > 0.5, true;
    if (key == "pose") return H.pose = (int)v, H.hold = v >= 0.0, true;
    if (key == "place") return H.place = (float)v, true;
    if (key == "emerge") return H.emergeAt = H.clock + std::max(0.0, v), H.emergeUntil = H.emergeAt + 8.0, true;
    for (auto& a : agents_) {
        if (a->role != Role::Listener || a->showcase || a->puppet) continue;
        if (key == "state") return a->state = (int)v, a->stateT = 0.0, true;
        if (key == "yaw") return a->yaw = (float)v, true;
    }
    return false;
}

}  // namespace lim::game
