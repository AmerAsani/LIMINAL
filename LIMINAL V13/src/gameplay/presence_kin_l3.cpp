// V13.3: Level 3 "Electrical Station" - zwei Wesen in ihrer Level-3-Auspraegung (Wikidot level-3 Rev. 18, entity-15 Rev. 54,
// entity-10 Rev. 61, level-1 Rev. 83; CC BY-SA 3.0; eigene Worte) - KREATUR_Wretch.md und KREATUR_SkinStealer.md, je
// Abschnitt "V13.3: Level 3".
//
//   Wretch       selten; behaelt den Koerper und die Kleidung, rennt, traegt eine selbst gebaute Waffe (Rohrstueck,
//                Nagelbrett, Blechklinge mit Kabelgriff - welche, sagt die Seite nicht: Interpretation) und spricht in
//                Bruchstuecken. Man hoert ihn vor dem Sehen: die Waffe schleift und klappert. Sieht er einen von weitem,
//                lockt er erst (die Waffe hinter dem Bein, verstaendliche Fetzen: Hilfe, Wasser) - wer naeher kommt oder ihn
//                anspricht, wird angegriffen (Wiki, Level 1: nicht mit ihnen reden, auch wenn man die Stimme versteht).
//                Jeder Schlag kuendigt sich an: er holt aus und schreit (0,5-0,7 s); wer dann zurueckweicht, dem schlaegt
//                er ins Leere. Nach dem Schlag weicht er einen Schritt zurueck.
//   Skin-Stealer tarnt sich im Mantel (Kapuze, Schal, Sonnenbrille - die Seite: viel Kleidung gegen die faulende Haut) und
//                haelt die Tarnung lange: er schliesst sich einem an, haelt Abstand, spricht einfache, leicht falsche Saetze
//                (A2-Englisch, vorgetaeuschter Akzent). Er verwandelt sich erst nach langer Zeit (hungrig) oder wenn er
//                entlarvt wird: dreimal angesprochen (beim zweiten Mal wiederholt er den Satz genau gleich) oder die Lampe
//                zu lange im Gesicht (die faulende Haut). Satt verabschiedet er sich nach einer Weile. Die Verwandlung
//                (2,4 s, Haut und Mantel reissen) ist die Ankuendigung; danach wie in Level 1.
#include "gameplay/kin_l3.hpp"
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include <array>

#include "core/log.hpp"

namespace lim::game {

// --- Klaenge und Untertitel ----------------------------------------------------------------------------------------------
namespace {
const char* const kHintWretch = "Ein Wretch spricht. Nicht antworten, nicht näher gehen – auch wenn man ihn versteht.";
const char* const kHintSkin = "Ein Fremder im Mantel spricht. Hier tarnen sich Skin-Stealer als Menschen – auf Wiederholungen achten, Abstand halten.";
const std::array<KinLine, KC_COUNT>& lines() {
    static const std::array<KinLine, KC_COUNT> L = [] {
        std::array<KinLine, KC_COUNT> t{};
        // Wretch: Fetzen ohne Lippen (Stadium 3: Lippen aufgeloest) - p, b, m, w, f fehlen oder werden ein feuchtes Schnalzen
        const char* frag[KC_FRAGS] = {"„…hel’… hel’…“", "„…’ater… ’ater…“", "„…’lease… …’lease…“", "„…go… ho’…“",
                                      "„…hun… gry…“", "„…it… itches…“", "„…don’t… look…“", "„(unverständliches Kauderwelsch)“"};
        static const char* const fragSnd[KC_FRAGS] = {"wretch_frag_0", "wretch_frag_1", "wretch_frag_2", "wretch_frag_3",
                                                      "wretch_frag_4", "wretch_frag_5", "wretch_frag_6", "wretch_frag_7"};
        for (int k = 0; k < KC_FRAGS; ++k) t[(size_t)(KC_FRAG0 + k)] = {fragSnd[k], frag[k], 0.8f, 20.0f, 2.4f, kHintWretch};
        static const char* const crySnd[KC_CRIES] = {"wretch_cry_0", "wretch_cry_1", "wretch_cry_2"};
        const char* cry[KC_CRIES] = {"„NO—!“", "„MINE!“", nullptr};
        for (int k = 0; k < KC_CRIES; ++k) t[(size_t)(KC_CRY0 + k)] = {crySnd[k], cry[k], 1.0f, 30.0f, 1.4f, nullptr};
        static const char* const w[3] = {"pipe", "board", "blade"};
        static std::array<std::string, 12> names;
        for (int k = 0; k < 3; ++k) {
            names[(size_t)k] = std::string("wretch_drag_") + w[k];
            names[(size_t)(3 + k)] = std::string("wretch_raise_") + w[k];
            names[(size_t)(6 + k)] = std::string("wretch_miss_") + w[k];
            names[(size_t)(9 + k)] = std::string("wretch_hit_") + w[k];
            t[(size_t)(KC_DRAG0 + k)] = {names[(size_t)k].c_str(), nullptr, k == 2 ? 0.45f : 0.7f, 26.0f};
            t[(size_t)(KC_RAISE0 + k)] = {names[(size_t)(3 + k)].c_str(), nullptr, 0.8f, 16.0f};
            t[(size_t)(KC_MISS0 + k)] = {names[(size_t)(6 + k)].c_str(), nullptr, 1.0f, 30.0f};
            t[(size_t)(KC_HIT0 + k)] = {names[(size_t)(9 + k)].c_str(), nullptr, 1.0f, 18.0f};
        }
        // Skin-Stealer: einfache, leicht falsche Saetze (die Seite: Englisch etwa A2, vorgetaeuschter Akzent)
        const char* say[KC_SAYS] = {"„Hello. You are alone, yes?“",          "„I am lost also. We go together.“",
                                    "„Is safe here. You stay with me.“",     "„Where you are going? I come with you.“",
                                    "„My name is Daniel. Is my name.“",      "„You have water? I am very thirst.“",
                                    "„Do not be afraid. I am people. Human.“", "„You are tired. Sit. Rest here.“"};
        static const char* const saySnd[KC_SAYS] = {"skin_say_0", "skin_say_1", "skin_say_2", "skin_say_3",
                                                    "skin_say_4", "skin_say_5", "skin_say_6", "skin_say_7"};
        for (int k = 0; k < KC_SAYS; ++k) t[(size_t)(KC_SAY0 + k)] = {saySnd[k], say[k], 0.75f, 18.0f, 3.2f, kHintSkin};
        const char* ans[KC_ANSWERS] = {"„Yes. Hello. I am friend.“", "„I am human. Like you.“", "„I come from Base Gamma. Is far.“",
                                       "„Is okay. Everything is okay.“"};
        static const char* const ansSnd[KC_ANSWERS] = {"skin_ans_0", "skin_ans_1", "skin_ans_2", "skin_ans_3"};
        for (int k = 0; k < KC_ANSWERS; ++k) t[(size_t)(KC_ANS0 + k)] = {ansSnd[k], ans[k], 0.8f, 18.0f, 3.0f, kHintSkin};
        t[KC_WAIT] = {"skin_wait", "„Wait! Where you go?“", 0.95f, 26.0f, 2.2f, nullptr};
        t[KC_BYE] = {"skin_bye", "„I go now. Goodbye, friend.“", 0.75f, 18.0f, 2.8f, nullptr};
        t[KC_TEAR] = {"skin_tear", nullptr, 1.0f, 26.0f};
        return t;
    }();
    return L;
}

// Waffen der Wretches in Level 3 (Interpretation - die Seite sagt nur: sie tragen und benutzen Waffen)
struct Weapon {
    const char* name;
    float reachStart, reach;  // ab hier holt er aus; so weit trifft der Schlag (m)
    float windup, swing;      // Ausholen, Schlag (s) - getroffen wird bei 45 % des Schlags
    float damage;             // Health-Punkte (x Schwierigkeit)
    float lunge;              // Ausfall waehrend des Schlags (m/s)
    float recover;            // Rueckzug danach (s)
};
constexpr Weapon kWeapons[4] = {
    {"ohne Waffe", 1.6f, 1.6f, 0.5f, 0.3f, 26.0f, 0.0f, 0.8f},
    {"Rohrstueck", 2.3f, 2.05f, 0.72f, 0.40f, 32.0f, 1.2f, 1.0f},
    {"Nagelbrett", 2.4f, 2.15f, 0.62f, 0.36f, 30.0f, 1.0f, 0.9f},
    {"Blechklinge", 2.7f, 2.35f, 0.52f, 0.30f, 24.0f, 3.0f, 0.8f},
};

struct WretchL3 {
    vec3 prev;
    bool init = false;
    double dragT = 1.0, talkT = 0.0, lureLimit = 8.0, lostT = 0.0;
    int lastFrag = -1;
    bool struck = false;
};
template <class A>
WretchL3& wl3(A& a) {
    if (!a.ext) a.ext = std::make_shared<WretchL3>();
    return *static_cast<WretchL3*>(a.ext.get());
}

struct SkinL3 {
    int phase = 0;  // Zustand 7: 0 begleitet, 1 lauscht (angesprochen), 2 verwandelt sich
    double phaseT = 0.0, sayT = 1.5, replyT = 0.0, talkT = 0.0, company = 0.0, limit = 100.0, lightT = 0.0;
    double lastAddr = -1e9, waitCall = 0.0, unseen = 0.0, nearT = 0.0;
    int addrN = 0, lastAns = -1, lastSay = -1, reply = -1;
    bool swapped = false, greeted = false;
};
template <class A>
SkinL3& sl3(A& a) {
    if (!a.ext) a.ext = std::make_shared<SkinL3>();
    return *static_cast<SkinL3*>(a.ext.get());
}

// rueckwaerts (Blick bleibt): ein Stueck vom Spieler weg, wenn der Boden es erlaubt
template <class A>
void stepBack(A& a, const Presence::Context& c, vec3 toP, double speed, double dt) {
    const vec3 to = a.pos - toP * (float)(speed * dt);
    const world::Tile* t = c.world->tileIfLoaded(ifloor(to.x), ifloor(to.y));
    if (t && figureWalkable(t, 1.9f) && std::fabs(t->floor - a.pos.z) < 0.35f) a.pos.x = to.x, a.pos.y = to.y;
}
template <class A>
void turnTo(A& a, vec3 toP, double rate, double dt) {
    a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * rate);
}
}  // namespace

bool kinRole(int role) { return role == (int)Presence::Role::Wretch || role == (int)Presence::Role::SkinStealer; }

const KinLine* kinLine(int cue) {
    if (cue < 0 || cue >= KC_COUNT) return nullptr;
    const KinLine& L = lines()[(size_t)cue];
    return L.sound ? &L : nullptr;
}

const KinLine* kinLineBySound(const std::string& s) {
    for (const KinLine& L : lines())
        if (L.sound && s == L.sound) return &L;
    return nullptr;
}

// Verwandlung ohne Neuanlage (auch die Puppe beim Gast): nur das Modell wechselt, Animation und Lage bleiben
void Presence::swapVariant(Agent& a, int v) {
    const auto& vars = anim::entityVariants(a.ent);
    if (v < 0 || v >= (int)vars.size() || v == a.variant) return;
    a.variant = v;
    a.rig.initEntity(a.ent, v > 0 ? vars[(size_t)v] : "");
    a.rig.setCreaturePose(a.cre.pose());
    a.rig.setGlow(1.0f);
}

// --- Wretch (Level 3, bewaffnet) --------------------------------------------------------------------------------------
// Zustaende: 0 lauert/schlurft (die Waffe schleift), 5 lockt, 2 jagt (in Stoessen), 6 holt aus, 7 schlaegt, 8 weicht
// zurueck, 4 laesst ab, 9 fort
void Presence::thinkWretchL3(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    WretchL3& w = wl3(a);
    const Weapon& W = kWeapons[std::clamp(a.variant, 0, 3)];
    const int wi = std::clamp(a.variant - 1, 0, 2);
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    const float dz = std::fabs(a.pos.z - (float)c.player->z);
    if (!w.init) w.init = true, w.prev = a.pos, w.lureLimit = rng_.uniform(7.0, 10.0);
    const float vel = (float)(hdist(a.pos, w.prev) / std::max(1e-3, dt));
    w.prev = a.pos;
    a.stateT += dt;
    a.cool -= dt;
    const vec3 chest = a.pos + vec3{0, 0, 1.1f};
    lightReact(a, c, head, 12.0f, dt, 1.2, &chest);
    const bool playerLit = c.view.flashOn || litPoint(c.view, *c.world, eye - vec3{0, 0, 0.6f});
    const bool sees = los && d < (playerLit ? 18.0f : 7.0f) && (facing > 0.1f || d < 4.0f);
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    const bool hears = (c.player->sprinting && pathD < 24.0f) || (pathD < 6.0f && c.player->gait > 0.3f);
    a.reach = 0.0f, a.plea = 0.0f, a.growl = 0.0f, a.alert = 0.0f, a.strike = 0.0f, a.talk = 0.0f, a.perceive = 0.0f, a.crouch = 0.0f;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    auto cue = [&](Event::Type t, int k, vec3 at) { out.push_back({t, Role::Wretch, at, kKinCue + (float)k}); };
    auto frag = [&](int lo, int hi) {  // ein Fetzen aus [lo, hi), nicht zweimal derselbe
        int k = (int)rng_.randint(lo, hi - 1);
        if (k == w.lastFrag && hi - lo > 1) k = lo + (k - lo + 1) % (hi - lo);
        w.lastFrag = k;
        cue(Event::Mimic, KC_FRAG0 + k, head);
        w.talkT = 1.6;
    };
    auto charge = [&](const char* why) {
        setState(2);
        a.burst = rng_.uniform(4.0, 6.0), a.timer2 = 0.0, a.lostT = 0.0;
        out.push_back({Event::Gurgle, Role::Wretch, head, 1.0f});  // hoch: klug und schnell (der Laut verraet die Gefahr)
        a.timer = rng_.uniform(2.5, 4.0);
        a.growlT = 1.0;
        log::info("Praesenz: Wretch ({}) - {}: er stuermt los ({:.1f} m)", W.name, why, d);
    };
    if (a.growlT > 0.0) a.growlT -= dt, a.growl = 1.0f;
    if (w.talkT > 0.0) w.talkT -= dt, a.talk = 1.0f;
    // angesprochen (Wiki: nicht interagieren) - er hat einen
    if (a.addressed) {
        a.addressed = false;
        if (a.state == 0 || a.state == 4 || a.state == 5) {
            if (a.state == 5) frag(0, KC_FRAG_LURE);
            charge("angesprochen");
        }
    }
    a.timer -= dt;
    switch (a.state) {
        case 0: {  // lauert im Schatten, schlurft umher - die Waffe schleift ueber den Boden
            a.look = false;
            if (sees || hears) {
                if (!hears && d > 5.5f && los) {  // von weitem: erst locken (man kann ihn hoeren und ansprechen - man soll nicht)
                    setState(5);
                    a.provoke = 0.0, a.timer = rng_.uniform(0.4, 1.2), w.lostT = 0.0;
                    w.lureLimit = rng_.uniform(7.0, 10.0);
                    log::info("Praesenz: Wretch ({}) - sieht einen ({:.1f} m), lockt", W.name, d);
                    break;
                }
                charge(hears ? "gehoert" : "gesehen");
                break;
            }
            if (a.timer <= 0.0 && d < 28.0f) {  // grundlos gurgeln und murmeln (die Seite)
                a.timer = rng_.uniform(6.0, 12.0);
                if (rng_.uniform(0.0, 1.0) < 0.6) frag(KC_FRAG_LURE, KC_FRAGS);
                else out.push_back({Event::Gurgle, Role::Wretch, head, 1.0f}), a.growlT = 1.2;
            }
            wander(a, dt, c, 1.1, (int)Role::Wretch - (int)Role::Faceling);
            break;
        }
        case 5: {  // lockt: halb aufgerichtet, die freie Hand bittend vor, die Waffe hinter dem Bein; verstaendliche Fetzen
            a.look = los, a.lookAt = eye;
            a.plea = 0.75f, a.perceive = 1.0f, a.perceiveAt = eye;
            if (a.timer <= 0.0) a.timer = rng_.uniform(2.6, 4.2), frag(0, KC_FRAG_LURE);
            if (d > 6.2f && los) walkTo(a, fld(), 0.45, dt, c);  // kriecht naeher, bis man ihn ansprechen koennte
            else turnTo(a, toP, 2.0, dt);
            if (a.light > 0.5f) a.provoke += dt;  // im Lichtkegel: er ist erkannt
            w.lostT = los ? 0.0 : w.lostT + dt;
            if (d < 4.5f) charge("man kommt naeher");
            else if (hears) charge("Laerm");
            else if (a.provoke > 1.4) charge("im Licht erkannt");
            else if (a.stateT > w.lureLimit) charge("lange genug gelockt");
            else if (w.lostT > 2.5) charge("ausser Sicht - er folgt");
            break;
        }
        case 2: {  // jagt in Stoessen: 4-6 s Lauf, 2-3 s ringt er nach Luft
            a.look = los, a.lookAt = eye;
            a.alert = 1.0f;
            double speed = 4.9;
            a.burst -= dt;
            if (a.burst <= 0.0 && a.timer2 <= 0.0) a.timer2 = rng_.uniform(2.0, 3.0);
            if (a.timer2 > 0.0) {
                a.timer2 -= dt;
                speed = 0.5;
                a.growl = 1.0f;
                if (a.timer2 <= 0.0) a.burst = rng_.uniform(4.0, 6.0);
            }
            if (a.timer <= 0.0 && d < 26.0f) {
                a.timer = rng_.uniform(2.5, 5.0);
                if (rng_.uniform(0.0, 1.0) < 0.5) frag(KC_FRAG_LURE, KC_FRAGS);
                else out.push_back({Event::Gurgle, Role::Wretch, head, 1.0f}), a.growlT = 1.0;
            }
            if (d < W.reachStart && los && dz < 1.0f && a.cool <= 0.0) {  // holt aus - Ankuendigung (F2)
                setState(6);
                w.struck = false;
                cue(Event::Mimic, KC_CRY0 + (int)rng_.randint(0, KC_CRIES - 1), head);
                cue(Event::Swing, KC_RAISE0 + wi, a.pos + vec3{0, 0, 1.4f});
                w.talkT = 0.0;
                log::info("Praesenz: Wretch ({}) - holt aus ({:.1f} m)", W.name, d);
                break;
            }
            if (d > 1.1f) walkTo(a, fld(), speed, dt, c);
            else turnTo(a, toP, 6.0, dt);
            if (sees || hears) a.lostT = 0.0;
            else a.lostT += dt;
            if (a.lostT > 4.0 || d > 34.0f) setState(4), pickGoal(c, a, 220, 360, true);
            break;
        }
        case 6: {  // holt aus: die Waffe hoch (Rohr ueber den Kopf, Brett hinter die Schulter, Klinge zurueck), Schrei
            a.look = true, a.lookAt = eye;
            a.alert = 1.0f, a.jaw = 0.8f;
            a.strike = 0.5f * (float)std::min(1.0, a.stateT / W.windup);
            a.crouch = wi == 2 ? 0.35f : (wi == 1 ? 0.12f : 0.0f);  // Klinge: geduckt zum Stich
            turnTo(a, toP, 6.0, dt);
            if (d > W.reach * 0.75f) walkTo(a, fld(), 0.6, dt, c), turnTo(a, toP, 8.0, dt);
            if (a.stateT >= W.windup) setState(7);
            break;
        }
        case 7: {  // Schlag (mit Ausfall) - trifft, wer noch in Reichweite vor ihm steht; sonst ins Leere
            a.look = true, a.lookAt = eye;
            a.alert = 1.0f, a.jaw = 0.9f;
            a.strike = 0.5f + 0.5f * (float)std::min(1.0, a.stateT / W.swing);
            a.crouch = wi == 2 ? 0.25f : 0.0f;
            if (a.stateT < W.swing * 0.6 && d > 0.9f) walkTo(a, fld(), W.lunge, dt, c);
            turnTo(a, toP, 3.0, dt);
            if (!w.struck && a.stateT >= W.swing * 0.45) {
                w.struck = true;
                const bool inFront = dot(toP, dirYaw(a.yaw)) > 0.45f;
                if (d < W.reach && dz < 1.0f && inFront && los && c.mode >= 2) {
                    out.push_back({Event::Hit, Role::Wretch, head, W.damage});
                    cue(Event::Swing, KC_HIT0 + wi, eye - vec3{0, 0, 0.4f});
                    log::info("Praesenz: Wretch ({}) - Treffer ({:.1f} m)", W.name, d);
                } else {
                    cue(Event::Swing, KC_MISS0 + wi, a.pos + dirYaw(a.yaw) * (W.reach * 0.8f) + vec3{0, 0, 0.1f});
                    log::info("Praesenz: Wretch ({}) - schlaegt ins Leere ({:.1f} m)", W.name, d);
                }
            }
            if (a.stateT >= W.swing) setState(8);
            break;
        }
        case 8: {  // weicht einen Schritt zurueck, ringt nach Luft, dann wieder auf ihn
            a.look = true, a.lookAt = eye;
            a.alert = 0.6f, a.growl = 1.0f;
            if (a.stateT < W.recover * 0.7) stepBack(a, c, toP, 0.9, dt);
            turnTo(a, toP, 4.0, dt);
            if (a.stateT >= W.recover) setState(2), a.cool = 0.35, a.lostT = 0.0;
            break;
        }
        case 4: {  // laesst ab, zurueck ins Dunkel
            a.look = false;
            if (sees && d < 10.0f) {
                charge("wieder gesehen");
                break;
            }
            if (!a.hasGoal || !walkTo(a, a.field, 1.2, dt, c)) pickGoal(c, a, 220, 360, true);
            if ((d > 30.0f && a.unseenT > 2.0) || (a.stateT > 45.0 && a.unseenT > 2.0)) a.state = 9;
            break;
        }
        default: break;
    }
    // die Waffe schleift (Rohr), klopft (Brett), klappert (Klinge) - man hoert ihn, bevor man ihn sieht
    w.dragT -= dt;
    if (a.variant > 0 && vel > 0.3f && vel < 2.6f && (a.state == 0 || a.state == 4 || a.state == 5) && w.dragT <= 0.0 && d < 32.0f) {
        w.dragT = wi == 2 ? rng_.uniform(2.2, 3.6) : rng_.uniform(1.3, 2.2);
        cue(Event::Drag, KC_DRAG0 + wi, a.pos - dirYaw(a.yaw) * 0.5f + vec3{0, 0, 0.05f});
    }
    if (a.state == 0 && ((d > 50.0f && a.unseenT > 3.0) || (a.age > 420.0 && a.unseenT > 5.0))) a.state = 9;
}

// --- Skin-Stealer (Level 3, im Mantel) ----------------------------------------------------------------------------------
// Zustaende: 0 geht umher, 7 begleitet (Phasen: begleitet, lauscht, verwandelt sich), 6 entlarvt und satt (wahre Gestalt,
// steht), 2 Angriff, 4 zieht ab, 9 fort, 10 Zugriff
void Presence::thinkSkinL3(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    SkinL3& s = sl3(a);
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    a.stateT += dt;
    a.cool -= dt;
    s.phaseT += dt;
    s.waitCall -= dt;
    const vec3 chest = a.pos + vec3{0, 0, 1.45f};
    lightReact(a, c, head, 9.0f, dt, 1e9, &chest);
    const bool sees = los && d < 18.0f && (facing > 0.1f || d < 4.0f);
    a.talk = 0.0f, a.reach = 0.0f, a.perceive = 0.0f, a.alert = 0.0f, a.jaw = 0.0f, a.shiver = 0.0f, a.tilt = 0.0f, a.crouch = 0.0f;
    auto setState = [&](int st) { a.state = st, a.stateT = 0.0; };
    auto cue = [&](int k) { out.push_back({Event::Mimic, Role::SkinStealer, head, kKinCue + (float)k}); };
    auto talkFor = [&](double sec) { s.talkT = sec; };
    auto transform = [&](const char* why) {
        setState(7);
        s.phase = 2, s.phaseT = 0.0, s.swapped = false;
        a.exposed = true;
        out.push_back({Event::Mimic, Role::SkinStealer, head, kKinCue + (float)KC_TEAR});
        log::info("Praesenz: Skin-Stealer (Level 3) - {}: die Tarnung faellt ({:.1f} m)", why, d);
    };
    if (s.talkT > 0.0) s.talkT -= dt, a.talk = 1.0f;
    switch (a.state) {
        case 0: {  // geht umher (getarnt) - wer ihn sieht, dem schliesst er sich an
            a.look = sees && d < 14.0f, a.lookAt = eye;
            if (a.light > 0.5f && d < 7.0f) s.lightT += dt;  // die Lampe im Gesicht: man sieht die faulende Haut
            else s.lightT = std::max(0.0, s.lightT - dt * 0.5);
            if (s.lightT > 2.4 && a.variant == 2) {
                transform("im Licht erkannt");
                break;
            }
            if (a.addressed && a.variant == 2) {  // angesprochen, bevor er sich anschliesst: er kommt und antwortet (naechstes Bild)
                setState(7);
                s.phase = 0, s.company = 0.0, s.greeted = true, s.sayT = rng_.uniform(9.0, 14.0);
                s.limit = a.hungry ? rng_.uniform(95.0, 130.0) : rng_.uniform(70.0, 100.0);
                return;
            }
            a.addressed = false;
            if (sees && a.cool <= 0.0 && a.variant == 2) {
                setState(7);
                s.phase = 0, s.company = 0.0, s.sayT = rng_.uniform(1.0, 2.5), s.greeted = false;
                s.limit = a.hungry ? rng_.uniform(95.0, 130.0) : rng_.uniform(70.0, 100.0);
                log::info("Praesenz: Skin-Stealer (Level 3) - schliesst sich an ({:.1f} m, {}, Tarnung haelt {:.0f} s)", d,
                          a.hungry ? "hungrig" : "satt", s.limit);
                break;
            }
            wander(a, dt, c, 0.9, (int)Role::SkinStealer - (int)Role::Faceling);
            break;
        }
        case 7: {
            if (s.phase == 2) {  // Verwandlung: Zucken, der Kiefer reisst auf - bei 1,5 s reissen Haut und Mantel, er richtet sich auf
                a.addressed = false;
                a.look = true, a.lookAt = eye;
                a.alert = 1.0f;
                const float u = (float)s.phaseT;
                a.shiver = u < 1.5f ? 1.0f : std::max(0.0f, 1.0f - (u - 1.5f) / 0.9f);
                a.crouch = u < 1.5f ? 0.45f * std::min(1.0f, u / 0.4f) : std::max(0.0f, 0.45f - (u - 1.5f) * 0.9f);  // kruemmt sich, richtet sich auf
                a.jaw = std::min(1.0f, u / 1.2f);
                a.tilt = u < 1.5f ? 0.35f * std::sin(u * 7.0f) + 0.2f : 0.1f;
                if (!s.swapped && u >= 1.5f) {
                    s.swapped = true;
                    swapVariant(a, 3);
                    out.push_back({Event::Shriek, Role::SkinStealer, head, 1.0f});
                }
                if (u >= 2.4f) {
                    if (a.hungry) {
                        setState(2);
                    } else {
                        setState(6);
                        a.burst = d;
                    }
                    s.phase = 0;
                }
                break;
            }
            a.look = true, a.lookAt = eye;
            a.perceive = 1.0f, a.perceiveAt = eye;
            a.tilt = 0.08f;
            // Licht im Gesicht: er wendet sich ab, die Hand vor die Brille - zu lange, und man sieht die faulende Haut
            if (a.light > 0.5f && d < 8.0f && los) s.lightT += dt;
            else s.lightT = std::max(0.0, s.lightT - dt * 0.5);
            if (s.lightT > 2.4) {
                transform("im Licht erkannt");
                break;
            }
            // angesprochen: er haelt inne, legt den Kopf schief und antwortet - beim zweiten Mal genau gleich, beim dritten
            // Mal keine Antwort mehr: entlarvt
            if (a.addressed) {
                a.addressed = false;
                s.addrN = a.age - s.lastAddr < 25.0 ? s.addrN + 1 : 1;
                s.lastAddr = a.age;
                if (s.addrN >= 3) {
                    transform("zu oft angesprochen");
                    break;
                }
                s.phase = 1, s.phaseT = 0.0, s.replyT = rng_.uniform(1.0, 1.6);
                log::info("Praesenz: Skin-Stealer (Level 3) - angesprochen ({}. Mal)", s.addrN);
            }
            if (s.phase == 1) {
                a.tilt = s.addrN >= 2 ? 0.3f : 0.16f;
                turnTo(a, toP, 3.0, dt);
                if (s.phaseT >= s.replyT) {
                    int k = s.lastAns;
                    if (s.addrN < 2 || k < 0) {
                        k = (int)rng_.randint(0, KC_ANSWERS - 1);
                        if (k == s.lastAns) k = (k + 1) % KC_ANSWERS;
                    }
                    s.lastAns = k;
                    cue(KC_ANS0 + k);
                    talkFor(2.2);
                    s.phase = 0, s.sayT = rng_.uniform(9.0, 14.0);
                    log::info("Praesenz: Skin-Stealer (Level 3) - antwortet (Satz {}{})", k, s.addrN >= 2 ? ", genau wie eben" : "");
                }
                break;
            }
            // Begleiten: 3,2-5,5 m Abstand, nie rennen; kommt man ihm zu nah, tritt er hoeflich zurueck
            s.company += d < 16.0f ? dt : 0.0;
            s.unseen = los ? 0.0 : s.unseen + dt;
            s.nearT = d < 1.2f ? s.nearT + dt : 0.0;
            if (c.player->sprinting && d < 14.0f && s.waitCall <= 0.0) {
                cue(KC_WAIT);
                talkFor(1.4);
                s.waitCall = 20.0;
            }
            if (d > 5.5f) walkTo(a, fld(), c.player->sprinting || d > 12.0f ? 2.6 : 1.5, dt, c);
            else if (d < 2.4f) stepBack(a, c, toP, 0.8, dt), turnTo(a, toP, 3.0, dt);
            else turnTo(a, toP, 2.5, dt);
            s.sayT -= dt;
            if (s.sayT <= 0.0 && d < 12.0f && los) {
                int k = s.greeted ? (int)rng_.randint(1, KC_SAYS - 1) : 0;
                if (k == s.lastSay) k = 1 + k % (KC_SAYS - 1);
                s.greeted = true, s.lastSay = k;
                cue(KC_SAY0 + k);
                talkFor(2.6);
                s.sayT = rng_.uniform(9.0, 15.0);
            }
            // nicht allein (ein Mitspieler dabei): er wartet mit der Verwandlung
            if (!a.alone) s.company = std::min(s.company, s.limit * 0.9);
            if (s.company > s.limit) {
                if (a.hungry) {
                    transform("lange genug begleitet");
                } else {
                    cue(KC_BYE);
                    talkFor(2.4);
                    a.cool = 150.0;
                    setState(4), pickGoal(c, a, 260, 380, true);
                    log::info("Praesenz: Skin-Stealer (Level 3) - satt, verabschiedet sich");
                }
                break;
            }
            if (s.unseen > 14.0 && d > 18.0f) {
                a.cool = 60.0;
                setState(4), pickGoal(c, a, 260, 380, true);
                log::info("Praesenz: Skin-Stealer (Level 3) - hat ihn verloren");
            }
            break;
        }
        case 6: {  // entlarvt und satt: steht in wahrer Gestalt, beobachtet - ruhig zurueckweichen laesst ihn ziehen
            a.look = true, a.lookAt = eye;
            a.perceive = 1.0f, a.perceiveAt = eye;
            a.tilt = 0.32f + 0.05f * std::sin((float)a.stateT * 1.3f);
            a.alert = (float)std::min(1.0, 0.5 + a.stateT / 6.0);
            a.jaw = 0.25f;
            turnTo(a, toP, 2.0, dt);
            const bool run = c.player->sprinting && d < 14.0f, again = a.addressed;
            a.addressed = false;
            if (run || again || (d < 1.6f && a.stateT > 0.8) || (a.stateT > 9.0 && d < a.burst + 2.0f)) {
                out.push_back({Event::Shriek, Role::SkinStealer, head, 1.0f});
                setState(2);
                log::info("Praesenz: Skin-Stealer (Level 3) - {}: Angriff", run ? "Rennen" : "zu nah oder zu lange");
                break;
            }
            if (d > a.burst + 3.5f || d > 11.0f) {
                a.cool = 120.0;
                setState(4), pickGoal(c, a, 260, 380, true);
                log::info("Praesenz: Skin-Stealer (Level 3) - ruhig zurueckgewichen ({:.1f} m), er laesst ihn ziehen", d);
            }
            break;
        }
        case 2: {  // Angriff (wahre Gestalt): stuermt, greift mit beiden Haenden
            a.look = true, a.lookAt = eye;
            a.alert = 1.0f, a.jaw = 0.8f;
            a.reach = d < 2.6f ? 1.0f - d / 2.6f : 0.0f;
            if (d < 1.2f && std::fabs(a.pos.z - (float)c.player->z) < 1.0f && c.mode >= 2) {
                out.push_back({Event::Attack, Role::SkinStealer, head, 1.0f});
                startKill(a, c);
                break;
            }
            walkTo(a, fld(), 4.0, dt, c);
            if ((a.unseenT > 6.0 && d > 10.0f) || d > 28.0f) setState(4), pickGoal(c, a, 260, 380, true);
            break;
        }
        case 4: {  // zieht ab
            a.look = false;
            if (!a.hasGoal || !walkTo(a, a.field, 1.4, dt, c)) pickGoal(c, a, 260, 380, true);
            if ((d > 30.0f && a.unseenT > 2.0) || (a.stateT > 40.0 && a.unseenT > 2.0)) a.state = 9;
            break;
        }
        default: break;
    }
    if (a.state == 2 || a.state == 4) a.addressed = false;
    if (a.state == 0 && ((d > 50.0f && a.unseenT > 3.0) || (a.age > 480.0 && a.unseenT > 5.0))) a.state = 9;
}

}  // namespace lim::game
