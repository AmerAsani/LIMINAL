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
}

namespace lim::game {

class Presence {
public:
    enum class Role { Smiler, Hound, Moth, Gestalt, Nest, Clump };  // V11: Clump (Buendel)
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
        } type;
        Role role = Role::Smiler;
        vec3 pos;
        float strength = 1.0f;
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
    };

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
    void updateHive(double dt, const Context& c, std::vector<Event>& out);
    bool walkTo(Agent& a, const NavField& f, double speed, double dt, const Context& c);
    bool hiddenSpawn(const Context& c, int lo, int hi, bool behind, vec3& out);
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
    double forcedDist_ = 0.0;
    Hive hive_;
};

}  // namespace lim::game
