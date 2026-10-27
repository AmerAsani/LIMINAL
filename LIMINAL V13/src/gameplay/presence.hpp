// V10: Die Entities der Welt und wann sie auftauchen ("Praesenz"). Welche Entity wo vorkommt und wie sie
// sich verhaelt, folgt den Backrooms-Wikis (Wikidot, Fandom); Quellen und Auslegung: docs/ENTITAETEN.md.
//
//   Innenraeume (Level 0)   keine bestaetigten Entities. Nur die Gestalt: ein dunkler, formloser Umriss
//                           am Ende eines Gangs (unbestaetigte Berichte ueber "dunkle Gestalten"). Sie
//                           veraendert sich nie vor deinen Augen - sieht man weg, ist sie fort; manchmal
//                           steht sie spaeter an einer anderen Gangmuendung. Sie greift nie an.
//   Tiefgeschoss (Level 1)  Stromausfaelle (das "Flackern" von Level 1). Im Dunkeln der Smiler: nur Augen
//                           und Laecheln. Haelt man den Blick, ruehrt er sich nicht; sieht man weg, kommt
//                           er lautlos durch das Dunkel naeher - nie ins Licht. Wer in seiner Naehe rennt,
//                           wird angegriffen. Licht aus und langsam zurueck: er verliert das Interesse.
//                           Der Hound streift umher, hoert Rennen, sieht das Licht der Lampe. Hat er dich,
//                           knurrt er und pirscht sich geduckt heran; Blickkontakt schuechtert ihn eine
//                           Weile ein. Dreht man ihm den Ruecken zu oder rennt, bellt er und stuermt los.
//   Buendel (V11)           haust in Deckenschaechten (Lager der Karte, Level 1-3); ein Arm haengt als Koeder
//                           heraus. Hoert nur: Laerm laesst es lauschen und klopfen, weiterer Laerm, und es
//                           faellt heraus und zieht sich zum Ort des letzten Geraeuschs - nicht zum Spieler.
//                           Wer naeher als 2,5 m Laerm macht, sieht den langen Arm ausholen; dann greift es.
//   Hotel (Level 5)         Deathmoth-Nester an den Tueren dunkler Zimmer; die Motten sitzen reglos an
//                           Wand und Decke. Trifft sie Licht oder kommt man zu nah, fliegt der Schwarm auf
//                           und umkreist die Lampe; ist sie an, stossen einzelne zu. Lampe aus: sie
//                           kreisen um die Deckenleuchten und kehren zum Nest zurueck. Selten ein Hound.
//
// Faire Regeln: niemand erscheint im Blickfeld oder direkt neben dir; jeder Angriff kuendigt sich an
// (Knurren und Bellen, ein naeher kommendes Laecheln mit tiefem Droehnen, das Flattern des Schwarms).
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "anim/animator.hpp"
#include "anim/creature_anim.hpp"
#include "core/rng.hpp"
#include "gameplay/figure.hpp"
#include "gameplay/navfield.hpp"
#include "gameplay/player.hpp"

namespace lim::world {
class World;
class Room;
struct KinDef;
}

namespace lim::game {

class Presence {
public:
    // V11: Clump (Buendel); V13.2: Faceling, Skin-Stealer, Wretch, Death Rat, Fensterwesen (gleiche Reihenfolge wie anim::Entity)
    enum class Role { Smiler, Hound, Moth, Gestalt, Nest, Clump, Faceling, SkinStealer, Wretch, Rat, Window, Duller, Arms, Wrangler };
    static constexpr int kRoleCount = 14;  // V13.3: Duller, Arms, Wrangler (KREATUR_Wrangler.md)
    static const char* roleName(Role r);

    struct Event {
        enum Type {
            Step,        // Schritt (strength: Lautstaerke)
            Breath,      // Hound hechelt ganz nah
            Rustle,      // Smiler kommt naeher (tiefes Droehnen)
            Appear, Gone,
            FirstSight,  // zum ersten Mal deutlich gesehen (je Auftritt)
            Stare,       // Blickkontakt mit dem Smiler (je Auftritt einmal)
            Touch,       // (frei)
            Attack,      // Smiler oder Hound greift zu (strength: Schaden x Schwierigkeit)
            Flicker,     // Leuchten flackern an pos
            Lost,        // hat die Spur verloren
            Vision,      // die Gestalt gesehen
            Growl,       // Hound knurrt (strength: Naehe)
            Whine,       // Hound winselt (verliert die Spur)
            Bark,        // Hound bellt kurz - gleich stuermt er los
            TakeOff,     // ein Mottenschwarm fliegt auf
            Sting,       // eine Motte sticht zu
            Blackout,    // Stromausfall im Raum des Spielers (Level-1-Flackern); strength: Dauer (s)
            Announce,    // V11: das Laecheln oeffnet sich weit, der Ton steigt - in 1,5 s greift es zu (F2)
            Calm,        // V11: der Spieler hat sich beruhigt, es schliesst sich wieder
            Knock,       // V11 Buendel: Schlaege an Rohr oder Wand (lauscht)
            Drop,        // V11 Buendel: faellt aus dem Schacht
            Drag,        // V11 Buendel: zieht sich ueber den Boden
            Windup,      // V11 Buendel: der lange Arm holt aus - in 1-1,5 s greift es zu (F2)
            // V13.2: neue Wesen
            Hit,         // ein Schlag, Biss oder Hieb (strength: Schaden in Health-Punkten) - nicht toedlich
            Mimic,       // Skin-Stealer spricht nach (Worte ohne Sinn, um zu locken)
            Whisper,     // Fensterwesen fluestert (trostend, lockend)
            Plead,       // Wretch im zweiten Stadium fleht (um Wasser, Essen, Hilfe)
            Gurgle,      // Wretch gurgelt und faucht (strength 0: tief und langsam - traeger; 1: hoch - schnell)
            Hum,         // Faceling: vokalartiger Laut (strength 1: aufgebracht)
            Squeak,      // Death Rat quiekt
            Scream,      // Death Rats fallen ueber ein anderes Wesen her ("Screaming Event"): Kreischen, Flackern
            Swing,       // Schlag mit dem Steinwerkzeug (Ausholen)
            Pull,        // Fensterwesen: der Arm faehrt aus dem Fenster
            // V13.3: Duller, Arms
            Scrape,      // Schaben: der Duller in der Wand, die Arms im Schacht (strength: Lautstaerke)
            Bulge,       // Duller: der Putz woelbt sich, knackt
            Burst,       // Duller: die Wand bricht auf; Arms: die Lueftung oeffnet sich (sehr leise)
            Burn,        // Arms: Licht verbrennt den Arm - heiserer Todesschrei, Zischen
            Retract,     // Duller/Arms: der Arm gleitet zurueck
            // V13.3: Wrangler
            Tremor,      // Beben: Rumpeln in der Tiefe, das naeher kommt (strength: Staerke)
            Breach,      // der Leib bricht durch Boden, Wand oder Decke
            Slither,     // der Leib gleitet (strength 1: Stoehnen des Maennchens)
            Sense,       // es hat einen gespuert: Knacken und Einatmen an der Durchbruchstelle - in 1,8 s schnappt es zu (F2)
            Sink,        // der Leib versinkt, es zieht ab
        } type;
        Role role = Role::Smiler;
        vec3 pos;
        float strength = 1.0f;
        int target = 0;  // V13.2 Koop: Spieler, den es betrifft (Context::selfId: dieser Rechner)
    };

    // V13.2 Koop: ein weiterer Spieler (Host: vom Host bewegte Figur eines Gasts; Gast: Mitspieler fuer Blick und Licht)
    struct Other {
        int id = 0;  // 0..3
        const Player* player = nullptr;
        Viewer view;
        double noise = 0.0;
        const world::Room* room = nullptr;
        bool canBlackout = false;
    };

    struct Context {
        const Player* player = nullptr;
        Viewer view;
        world::World* world = nullptr;
        double sanity = 100.0;
        int region = 0;            // 0 Innenraeume, 1 Tiefgeschoss, 2 Hotel
        int mode = 2;              // 0 aus, 1 ohne Angriffe, 2 alle
        bool hallucinations = true;
        bool suspended = false;    // Aufzugfahrt, Tod, Menue-Pause
        int fusesHeld = 0;
        double stalkerSpeed = 4.0, stalkerPause = 1.0;
        const world::Room* room = nullptr;  // Raum des Spielers
        bool canBlackout = false;           // der Raum hat Leuchten, die ausfallen koennen
        // V11: welche Entities die Ebene kennt (Level-JSON "presence")
        bool allowHound = true, allowSmiler = true, allowGestalt = true, allowBlackout = true;
        bool allowClump = false;  // V11: Buendel (Level-JSON presence.clump)
        double clumpRate = 1.0;   // V11: Anteil der Gelegenheiten, bei denen es einzieht
        // V11: Laerm des Spielers (0 still, 0,15 leise, 0,45 gehen, 1 rennen, 0,8 Landung; ENTITY_BEHAVIOR 1),
        // Laerm an anderen Orten (Aufprall geworfener Dinge: Ort, Lautstaerke), Lager des Buendels (Decke)
        double noise = 0.0;
        const std::vector<std::pair<vec3, float>>* pings = nullptr;
        const std::vector<vec3>* lairs = nullptr;
        // V11: Abstand und Dauer der Stromausfaelle (Level-JSON "audio.blackout")
        double blackoutEvery0 = 200.0, blackoutEvery1 = 340.0, blackoutLen0 = 20.0, blackoutLen1 = 32.0;
        // V13.2 Koop (Host): weitere Spieler. Jedes Wesen richtet sich nach dem naechsten lebenden Spieler (dessen
        // Sicht, Laerm und Weg); die Regie verteilt Auftritte reihum; niemand erscheint im Blickfeld irgendeines
        // Spielers. selfId/selfAlive: der Spieler dieses Rechners (tot: nur die anderen sind Ziel)
        const std::vector<Other>* others = nullptr;
        int selfId = 0;
        bool selfAlive = true;
        // V13.2 Koop (Gast): keine KI - die Wesen kommen vom Host (netApply), hier nur ihre Bewegung
        bool puppet = false;
        // V13.2: neue Wesen der Ebene (world::KinDef[KIN_COUNT]: Faceling, Skin-Stealer, Wretch, Ratte, Fenster) oder nullptr
        const world::KinDef* kin = nullptr;
        bool almond = false;  // V13.3: der Spieler haelt Mandelwasser in der Hand (der Duller meidet es)
    };

    // V13.2 Koop: Zustand eines Wesens fuer die Momentaufnahme (Host) und die Puppe beim Gast
    struct NetState {
        u16 netId = 0;
        int role = 0;
        vec3 pos;
        float yaw = 0.0f;
        int state = 0;
        int target = -1;
        int variant = 0;  // V13.2: Modellvariante (anim::entityVariants)
        bool look = false, lit = false, frozen = false;
        float jaw = 0, alert = 0, crouch = 0, growl = 0, reach = 0, fly = 0, shiver = 0, hang = 0, bait = 0, light = 0, perceive = 0, glow = 0;
        float tilt = 0, lean = 0;
        float wave = 0, talk = 0, plea = 0, strike = 0;  // V13.2: Gesten
        vec3 up{0, 0, 1}, fwd{1, 0, 0};
    };
    // Host: die Wesen, nach Abstand zu near sortiert, hoechstens max (ohne Vorfuehrungen)
    void netStates(std::vector<NetState>& out, vec3 near, size_t max) const;
    // Gast: Puppen anlegen, nachfuehren (Zeitstempel auf der Uhr dieser Praesenz, netTime) und entfernen
    void netApply(const std::vector<NetState>& in, double stamp);
    double netTime() const { return time_; }
    // V13.3 Wrangler: Beben am eigenen Spieler (0..1; Kamera, Staub) und der Sog des Leibs (0..1; wohin er den Blick zieht)
    float quake() const { return quake_; }
    // V13.3: Klangquellen des naechsten Wranglers (app/wrangler_audio.cpp): Rumpeln und Leib (Ort, Staerke 0..1); false: keiner
    bool wranglerSound(vec3 ear, vec3& rumbleAt, float& rumble, vec3& bodyAt, float& body) const;
    float trance(vec3* at = nullptr) const {
        if (at) *at = tranceAt_;
        return trance_;
    }

    Presence();
    ~Presence();
    void update(double dt, const Context& c, std::vector<Event>& out);
    void collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail);
    void shadows(std::vector<ShadowCaster>& out) const;
    void clear();             // alle fort (Laden, Aufzug)
    void setGrace(double s);  // fruehestens dann der naechste Auftritt
    double threatIn() const { return nextThreat_; }

    // Zustand fuer Klang, Bild, Tests
    bool threatActive() const;      // Smiler oder Hound unterwegs
    float threatProximity() const;  // 0 .. 1
    bool hunting() const;           // der Hound jagt oder der Smiler kommt naeher
    int activeCount() const { return (int)agents_.size(); }
    bool has(Role r) const;
    int count(Role r) const;
    vec3 posOf(Role r) const;
    float distOf(Role r) const;
    bool seenRole(Role r) const;
    bool seenRoleBy(Role r, int target) const;  // V13.2 Koop: von diesem Spieler gesehen (er ist das Ziel)
    int stateOf(Role r) const;      // Zustand der ersten Entity dieser Rolle (Tests)
    int stageOf(Role r) const;      // V11: Stufe (Smiler: 1 ruhig, 2 Ankuendigung, 3 Zugriff) - Tests
    bool killerHead(vec3& out) const;  // V11: Kopf der Entity, die gerade toetet (Kamera des Schrecks)
    float yawOf(Role r) const;      // Blickrichtung (Nest: in das dunkle Zimmer)
    int encounters = 0;             // Auftritte, die man gesehen hat
    // fliegende Motten: Anzahl, Mittelpunkt (Klang des Schwarms)
    int swarm(vec3& center) const;

    // Tests und Skripte: Entity erscheinen lassen (dist > 0: etwa so weit entfernt), unabhaengig vom Bereich
    void force(Role r, double dist = 0.0);
    void forceBlackout() { forceBlackout_ = true; }
    // V13.2 (Skript, Bilder): das Nest genau an dieser Tuer (Kachel x, y) - ohne Abstands- und Sichtpruefung
    void forceHiveAt(int x, int y) { hiveDoorX_ = x, hiveDoorY_ = y; }
    // V11 (Skript): die Begegnung von Level 1 - Stromausfall als Welle, danach im Dunkeln das Laecheln
    void forceEncounter() { forceEncounter_ = true; }
    // Vorfuehrung: Entity steht an pos (walkSpeed > 0: geht/fliegt mit diesem Tempo im Kreis).
    // kind 0..5: die Entities (anim::Entity), 6: Mensch (der Spieler)
    void showcase(int kind, vec3 pos, float yaw, float walkSpeed = 0.0f);
    void clearShowcase();  // V13 (Tests): alle Vorfuehrungen entfernen
    // V13.2 (Tests): Eingabe der Animation der zuletzt gezeigten Vorfuehrung setzen (jaw, alert, crouch, growl, reach,
    // fly, shiver, lean, tilt, hang, bait, light, perceive, speed, look); false: unbekannt
    bool showcaseSet(const std::string& key, float v);
    bool showcasePos(vec3& pos, float& yaw) const;  // V13 (Tests): Lage der zuletzt gezeigten Vorfuehrung
    // V13.2 (Tests): Eigenschaft des ersten Wesens dieser Rolle setzen (hungry, pleads, mode, provoke) bzw. lesen (variant)
    bool kinSet(Role r, const std::string& key, double v);
    int variantOf(Role r) const;
    vec3 headOf(Role r) const;  // V13.2 (Tests): Kopf des ersten Wesens dieser Rolle

private:
    struct Agent;
    struct Hive {
        bool active = false, alarmed = false;
        vec3 nest;
        const world::Room* room = nullptr;
        double darkT = 0.0, age = 0.0;
        vec3 target;
        bool hasTarget = false;
        NavField home;  // Weg zurueck in das Zimmer hinter dem Nest
    };
    // V13.2 Koop
    void regie(double dt, const Context& c, std::vector<Event>& out);
    void animate(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void puppetStep(Agent& a, const Context& c);
    const NavField& fld() const { return *curField_; }
    const NavField* curField_ = &playerField_;
    struct OtherField {
        int id = 0;
        NavField field;
        double t = 0.0;
    };
    std::vector<OtherField> otherFields_;
    const NavField& fieldOf(int id) const;
    int focus_ = -1;
    double focusT_ = 0.0;
    u16 nextNetId_ = 1;
    void spawnSmiler(const Context& c, std::vector<Event>& out, double dist, bool blackout);
    void spawnHound(const Context& c, std::vector<Event>& out, double dist);
    bool spawnHive(const Context& c, double dist);
    void spawnGestalt(const Context& c, std::vector<Event>& out, double dist);
    bool spawnClump(const Context& c, std::vector<Event>& out, bool forced);  // V11
    bool clumpHears(const Agent& a, const Context& c, vec3& where, float& loud) const;
    void thinkClump(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void think(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    // V11: toedlicher Zugriff - Zustand 10: das Gesicht faehrt in die Kamera, dann ist es fort
    void startKill(Agent& a, const Context& c);
    void killPose(Agent& a, double dt, const Context& c);
    void thinkHound(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkMoth(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkGestalt(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    // V13.2: neue Wesen (KREATUR_<Name>.md)
    bool spawnKin(Role r, const Context& c, std::vector<Event>& out);
    void placeWindows(const Context& c);
    void thinkFaceling(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkSkinStealer(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkWretch(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkRat(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkWindow(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    // V13.3: Duller (presence_duller.cpp), Arms (presence_arms.cpp)
    void regieDuller(double dt, const Context& c, std::vector<Event>& out);
    bool spawnDuller(const Context& c, std::vector<Event>& out, bool forced);
    void thinkDuller(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void regieArms(double dt, const Context& c, std::vector<Event>& out);
    bool spawnArms(const Context& c, std::vector<Event>& out, bool forced);
    void thinkArms(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    // V13.3: Wrangler (presence_wrangler.cpp)
    void regieWrangler(double dt, const Context& c, std::vector<Event>& out);
    bool spawnWrangler(const Context& c, std::vector<Event>& out, int mode, vec3* near = nullptr);
    bool placeBrood(const Context& c);
    void thinkWrangler(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkBrood(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    bool wranglerBreak(const Context& c, vec3 q, vec3 toward, vec3& at, vec3& n) const;
    void wranglerFeel(double dt, const Context& c);
    bool wranglerActive() const;
    bool roomAllowed(const Context& c, vec3 p, int kin) const;
    bool wander(Agent& a, double dt, const Context& c, double speed, int kin);
    void updateHive(double dt, const Context& c, std::vector<Event>& out);
    bool walkTo(Agent& a, const NavField& f, double speed, double dt, const Context& c);
    bool hiddenSpawn(const Context& c, int lo, int hi, bool behind, vec3& out, const std::function<bool(vec3)>& ok = {});
    bool pickGoal(const Context& c, Agent& a, int lo, int hi, bool away);
    void remove(size_t i);

    std::vector<std::unique_ptr<Agent>> agents_;
    NavField playerField_;
    double fieldT_ = 0.0;
    Rng rng_{0x9E3779B1u};
    double time_ = 0.0;
    double nextThreat_ = 220.0, nextBlackout_ = 150.0, nextHive_ = 40.0, nextGestalt_ = 120.0, nextClump_ = 25.0;
    bool blackoutTimed_ = false;  // V11: erster Abstand aus dem Level gesetzt
    int smilerTries_ = 0;         // V11: noch Versuche, im Stromausfall einen Platz zu finden
    double pendingSmiler_ = -1.0;  // nach einem Stromausfall: in so vielen Sekunden der Smiler
    int threatsSeen_ = 0, gestaltRepeat_ = 0;
    int forced_ = -1;              // erzwungene Rolle (Tests, Skripte)
    bool forceBlackout_ = false, forceEncounter_ = false;
    int hiveDoorX_ = -1, hiveDoorY_ = -1;  // V13.2
    double nextKin_[5] = {30.0, 150.0, 90.0, 25.0, 0.0};  // V13.2: naechster Auftritt je neuem Wesen
    bool windowsPlaced_ = false;                            // V13.2: Fensterwesen der Ebene gesetzt
    double nextDuller_ = 70.0, nextArms_ = 15.0;            // V13.3: naechster Auftritt
    double nextScream_ = 120.0, screamT_ = 0.0, screamTick_ = 0.0;  // V13.2: Schrei-Ereignis der Ratten (Dauer, Flackern)
    vec3 screamPos_;
    int nextGroup_ = 1;
    double nextWrangler_ = 160.0;  // V13.3: naechster Durchzug eines Wranglers
    bool broodPlaced_ = false;     // V13.3: Jungtiere der Ebene gesetzt
    float quake_ = 0.0f, trance_ = 0.0f;
    vec3 tranceAt_;
    double forcedDist_ = 0.0;
    Hive hive_;
};

}  // namespace lim::game
