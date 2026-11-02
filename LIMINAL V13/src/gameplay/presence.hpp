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

#include <array>
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
    // V13.3: Growler, Nguithr'xurh hinter Duller, Arms, Wrangler (presence_growler.cpp, presence_nguithrxurh.cpp)
    // V13.3: Ouroboros-Arbeiter hinter Nguithr'xurh (presence_ouroboros.cpp)
    // V13.3: Plague Goblins hinter den Ouroboros-Arbeitern (presence_goblin.cpp)
    // V13.3 Teil 4: Jerry hinter den Plague Goblins (presence_jerry.cpp)
    enum class Role { Smiler, Hound, Moth, Gestalt, Nest, Clump, Faceling, SkinStealer, Wretch, Rat, Window, Duller, Arms, Wrangler, Growler, Nguithr, Ouroboros, Goblin, Jerry };
    static constexpr int kRoleCount = 19;  // V13.3: Duller, Arms, Wrangler (KREATUR_Wrangler.md), Growler, Nguithr'xurh, Ouroboros-Arbeiter, Plague Goblins, Jerry
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
            // V13.3: Growler, Nguithr'xurh
            Click,       // Growler: Klicks der Echoortung (strength: Groesse 0..1)
            Static,      // Growler: das telepathische Rauschen schadet aus der Naehe (strength: Health-Punkte dieses Takts, 0: nur Verstand)
            Lurch,       // Growler: richtet sich auf, die Drahtrippen straeuben sich, das Rauschen schwillt - gleich stoesst er zu
            Merge,       // zwei Growler verschmelzen
            Collapse,    // Growler bricht zusammen (verhungert, verwirrt, Schock)
            Snip,        // Nguithr'xurh schneidet eine Netzkugel ab
            SacBurst,    // eine Netzkugel zerplatzt am Boden (strength: Betaeubung fuer das Ziel, 0: daneben)
            Skitter,     // Nguithr'xurh huscht
            Stridulate,  // Nguithr'xurh zirpt (Cheliceren reiben - im Licht, gestoert)
            // V13.3: Ouroboros-Arbeiter (KREATUR_OuroborosArbeiter.md) - Werkzeug, Maschinen, Stimmen
            Work,        // Werkzeug (strength: 0 Hammer, 0,25 Kelle, 0,5 Stein gesetzt, 0,75 Brechstange, 1 Stein bricht)
            Weld,        // Schweissen (strength 0: Zuenden - Ankuendigung, 1: Lichtbogen knistert)
            Jackhammer,  // Presslufthammer (ein Stoss von 0,6 s)
            Warn,        // Warnklang (strength 0: Kompressor laeuft an und piept - Presslufthammer, 1: Warnleuchte des Geruests)
            Creak,       // das Geruest knirscht und wankt (in 2,8 s kippt es)
            Crash,       // Aufschlag (strength 1: das Geruest, 0,5: eine Lage der Mauer)
            Murmur,      // dumpfe Stimme ohne Worte (strength 1: dicht am Spieler)
            Shove,       // ein Arbeiter geht durch den Spieler hindurch / schiebt ihn
            Dissolve,    // ein Arbeiter vergeht (vom eigenen Geruest getroffen); die anderen bemerken es nicht
            // V13.3: Plague Goblins (KREATUR_PlagueGoblins.md) - Laute, Diebstahl, Portal
            Croak,       // Kraechzen, Schnattern (strength 0: leises Schnattern, 0,5: Kraechzen, 1: Warnschrei)
            Steal,       // am Ruecken des Ziels (strength 0: es zerrt am Rucksack - Ankuendigung, 0,5: nichts gefunden,
                         // 1: genommen - item: was; die Sitzung nimmt es aus dem Inventar)
            Take,        // hebt etwas Liegendes auf (item: was; die Sitzung entfernt den naechsten Fund bei pos)
            Hiss,        // Drohen: Masken hoch, Fauchen - in 1,4 s greift die Bande an (F2)
            Screech,     // Sprung auf das Ziel (Kreischen; der Biss folgt als Hit)
            Portal,      // das Portal der Torwaechterin (strength 1: oeffnet, 0: schliesst)
            Spill,       // ein toter Goblin verliert, was er im Umhang trug (item: was; die Sitzung legt es hin)
            Gift,        // Handel: die Bande legt etwas hin (item: was)
            Perish,      // ein Goblin stirbt (Todesschrei); die anderen fliehen
            // V13.3: Leuchtkoeder und Ansprechen (KREATUR_Smiler.md, KREATUR_SkinStealer.md)
            Lured,       // Smiler wendet sich dem staerkeren Licht eines Leuchtstabs zu (strength 1: folgt, 0: stutzt nur)
            Reply,       // Skin-Stealer antwortet auf das Ansprechen - nachgeahmt, falsch betont (strength: (Satz*3+Art+0,5)/16)
            Shriek,      // Skin-Stealer: die Tarnung faellt, er stuermt los (Rennen, erneutes Ansprechen, zu lange Naehe)
            // V13.3 Teil 4: Jerry (KREATUR_Jerry.md) - Stimme, Gefieder, Bann
            Speak,       // er spricht mit ruhiger Menschenstimme (strength 0,35: murmelt vor sich hin, 1: spricht den Spieler an)
            Flap,        // Fluegel (strength 0,3: schuettelt und streckt sie, 1: breitet sie aus, schlaegt)
            Preen,       // putzt sich, knabbert, Schnabel knirscht (strength 0: Knirschen, 1: Federn)
            Spell,       // der Bann waechst bei Blickkontakt (strength: Stufe 0,3 / 0,6) - Klingeln, Fluestern im Kopf
        } type;
        Role role = Role::Smiler;
        vec3 pos;
        float strength = 1.0f;
        int target = 0;  // V13.2 Koop: Spieler, den es betrifft (Context::selfId: dieser Rechner)
        std::string item;  // V13.3: Plague Goblins - Gegenstand (Steal, Take, Spill, Gift)
    };
    // V13.3 Plague Goblins: Liegendes in der Naehe (Funde, Abgelegtes; dropped: vom Spieler hingelegt/geworfen)
    struct Loose {
        vec3 pos;
        std::string kind;
        bool dropped = false;
    };

    // V13.2 Koop: ein weiterer Spieler (Host: vom Host bewegte Figur eines Gasts; Gast: Mitspieler fuer Blick und Licht)
    struct Other {
        int id = 0;  // 0..3
        const Player* player = nullptr;
        Viewer view;
        double noise = 0.0;
        const world::Room* room = nullptr;
        bool canBlackout = false;
        const std::vector<std::string>* loot = nullptr;  // V13.3 Plague Goblins: was im Inventar zu holen ist
        bool hatchet = false;                            // V13.3: haelt das Beil
        double health = 100.0;                           // V13.3: Geschwaechte (die Seite: Allein, Schwache) werden bedraengt
    };

    // V13.3: Leuchtkoeder - geworfener, leuchtender Leuchtstab: Ort und Leuchtkraft (die eingeschaltete Lampe zaehlt 1)
    struct Lure {
        vec3 pos;
        float strength = 0.0f;
        int id = 0;
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
        // V13.3 Plague Goblins: was im Inventar zu holen ist (je Stueck ein Eintrag), Beil in der Hand, Liegendes
        const std::vector<std::string>* loot = nullptr;
        bool hatchet = false;
        const std::vector<Loose>* loose = nullptr;
        double health = 100.0;
        // V13.3 (KREATUR_Smiler.md): leuchtende Leuchtstaebe; Smiler folgen dem staerkeren Licht (0) oder stutzen nur (1, Level 3)
        const std::vector<Lure>* lures = nullptr;
        int smilerBait = 0;
        double smilerUnprovoked = 0.0;  // V13.3 Level 3: Anteil der Smiler, die ohne Anlass angreifen (KREATUR_Smiler.md)
        double redKnightEvery = 0.0;    // V13.3 Level 3: mittlerer Abstand der Laeufe des Red Knight (s; 0 = nie)
        // V13.3 Level 2/3 (KREATUR_Hound.md, KREATUR_Clump.md): Hound-Rudel, grosses bewaffnetes Buendel (nullptr: keins)
        const world::KinDef* houndPack = nullptr;
        const world::KinDef* clumpBig = nullptr;
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
    // V13.3 Teil 4 (Tests, Skripte): naechster Auftritt eines Fensters am Gangende und einer Mottengruppe in Level 2 (s)
    void l2Timers(double window, double moth) { nextWindowL2_ = window, nextMothL2_ = moth; }
    // V13.3 Level 3 (Tests, Skripte; KREATUR_DeathRat.md, KREATUR_Deathmoth.md): l3Set - trap <abstand> (Falle vor dem Spieler),
    // say <satz>, job <Auftrag der ersten Ratte>, quick <Bauzeit-Faktor>, rats <Sekunden bis zur Brigade>, swarm <1: aufscheuchen>,
    // pstate <Zustand des ersten Praetorianers>, mem <0: Gedaechtnis loeschen>. l3Info: Zahlen fuer Tests.
    struct L3Info {
        int rats = 0, traps = 0, armed = 0, commons = 0, praetorians = 0, onFace = 0, ratJob = -1, pState = -1;
        int jumped = 0, shocked = 0, lit = 0, encounters = 0, trail = 0;
        bool roused = false;
        vec3 trapL, trapR, trapPos;
    };
    bool l3Set(const std::string& key, double v, const Context* c = nullptr);
    L3Info l3Info() const;
    // V13.3 Teil 4 (Tests): das i-te Wesen dieser Rolle (ohne Vorfuehrungen) - Ort, Zustand, Variante; false: keins
    bool agentInfo(Role r, int i, vec3& pos, int& state, int& variant) const;
    int variantOf(Role r) const;
    vec3 headOf(Role r) const;  // V13.2 (Tests): Kopf des ersten Wesens dieser Rolle
    // V13.3: Growler - Staerke des telepathischen Rauschens beim Spieler dieses Rechners (0..1: Bild und Klang verzerrt),
    // Groesse eines Growlers (Tests). Nguithr'xurh - Betaeubung je Spieler (0..1), Tempo-Faktor (betaeubt langsamer).
    float staticLevel() const { return static_; }
    float growlerScale(int i = 0) const;
    float stupor(int target) const { return target >= 0 && target < 4 ? stupor_[(size_t)target] : 0.0f; }
    void addDose(int target, float d);
    float slowFor(int target, vec3 feet) const;  // auch: Gewirr einer Growler-Leiche
    vec3 webOf(int i = 0) const;  // Mitte der i-ten Netzkolonie an der Decke (Tests, Skripte); 0: keine
    // V13.3 (Tests, Skripte): i-te Spinne (Lage, Zustand); i-tes Wesen der Rolle versetzen; Werte setzen (Growler: scale,
    // state; Netz: hungry; Spieler: stupor)
    vec3 spiderOf(int i, int* state = nullptr) const;
    bool placeAgent(Role r, int i, vec3 p);
    bool gnSet(Role r, const std::string& key, double v);
    // V13.3: Ouroboros-Arbeiter (presence_ouroboros.cpp). Baustelle: Mauer und Geruest mit Gefahrenzone (Absperrband);
    // ouroSet (Tests, Skripte): wall <phase>, tower <phase>, fill <0..1>, quick <Faktor>, kill <Arbeiter>; ouroInfo: Lage
    // und Zonen (Tests); shoveFor: Schub (m/s) eines Arbeiters, der durch den Spieler geht; ouroLights: Lichtbogen und
    // Warnleuchten (Ort, Farbe x Staerke)
    struct OuroInfo {
        bool placed = false;
        vec3 wall, tower;            // Mitte der Mauer / der Standflaeche am Boden
        float wallYaw = 0.0f, towerYaw = 0.0f;
        vec3 wallZone[2], towerZone[2];  // Ecken (Welt, achsparallel) der abgesperrten Flaechen
        int wallPhase = 0, towerPhase = 0, workers = 0;
        vec3 sparkAt;                // wo die Funken auftreffen (0: nicht geschweisst)
        int laid = 0, broken = 0;    // gesetzte Steine, abgetragene Lagen
        // je Aufgabe (0 Maurer, 1 Klopfer, 2 Brecher, 3 Schweisser, 4 Traeger): vorhanden, Ort, Kopf, Blickrichtung, Aufgabe, Tempo
        std::array<bool, 5> has{};
        std::array<vec3, 5> pos{}, head{}, face{};
        std::array<int, 5> task{};
        std::array<float, 5> speed{};
    };
    OuroInfo ouroInfo() const;
    bool ouroSet(const std::string& key, double v);
    vec3 shoveFor(int target, vec3 feet) const;
    void ouroLights(std::vector<std::pair<vec3, vec3>>& out, vec3 near) const;
    // V13.3: Plague Goblins (presence_goblin.cpp). goblinSwing: ein Spieler schwingt das Beil (Auge, Blick) - 2: einen
    // getroffen (stirbt, die Bande flieht und kommt nicht wieder), 1: verscheucht, 0: nichts. goblinInfo (Tests): Lage der
    // Bande; goblinSet (Tests, Skripte): phase, mob, friendly, quick, rout, open
    struct GoblinInfo {
        bool active = false, friendly = false, routed = false, portalOpen = false;
        int phase = 0, bodies = 0, dead = 0, steals = 0, fails = 0, gifts = 0;
        vec3 portal;
        std::vector<vec3> pos;    // je Wesen (ohne Portal)
        std::vector<int> state, variant;
        std::vector<float> mask;  // Maske hoch (Drohen, Angriff)
    };
    int goblinSwing(int target, vec3 eye, vec3 look);
    GoblinInfo goblinInfo() const;
    bool goblinSet(const std::string& key, double v);
    // V13.3 Ansprechen (KREATUR_SkinStealer.md): die menschenaehnliche Gestalt (Skin-Stealer, Faceling, Wretch), die man
    // von eye aus in Blickrichtung look in hoechstens maxDist m ansieht (beim Gast: Puppen); head: ihr Kopf
    bool speakable(world::World& w, vec3 eye, vec3 look, float maxDist, vec3* head = nullptr) const;
    // ein Spieler spricht sie an (Einzelspieler/Host; by: Spieler, phrase: welcher Satz) - nur der Skin-Stealer antwortet
    bool address(world::World& w, vec3 eye, vec3 look, int by, int phrase);
    bool exposedOf(Role r) const;  // V13.3 (Tests): Skin-Stealer entlarvt (hat verraeterisch geantwortet)
    bool luredOf(Role r) const;    // V13.3 (Tests): Smiler folgt gerade einem Leuchtstab
    // V13.3 Level 3 (KREATUR_RedKnight.md, presence_knight.cpp): der Red Knight - Variante 4 der Rolle Wretch (keine eigene
    // Rolle). knightLights: der Riss in der Luft (weiss) und das Glimmen der Purpurspuren; knightInfo (Tests); knightSet
    // (Tests, Skripte): run <Abstand> (gleich einen Lauf am Spieler vorbei), quick, unseen
    struct KnightInfo {
        bool active = false, pursuer = false;
        int state = -1, runs = 0;
        vec3 pos;
        float closest = 1e9f;  // naechster Abstand zum Spieler in diesem Lauf
        bool slashed = false, rift = false, seen = false;
    };
    void knightLights(std::vector<std::pair<vec3, vec3>>& out, vec3 near) const;
    KnightInfo knightInfo() const;
    bool knightSet(const std::string& key, double v);
    static constexpr int kKnightVariant = 4;
    // V13.3 Teil 4: Jerry (presence_jerry.cpp). Bann je Spieler (0..1; beim Gast aus der Puppe): ab 0,45 zieht er den Blick zu
    // seinem Kopf (at), ab 1 ist der Spieler unterworfen. jerryHold: wie stark der Bann die Schritte laehmt (0..1).
    // jerryInfo (Tests): Lage, Zustand, Blick; jerrySet (Tests, Skripte): state, spell <Wert> (eigener Spieler), quick
    float jerrySpell(int target, vec3* at = nullptr) const;
    float jerryHold(int target) const;
    struct JerryInfo {
        bool active = false;
        int state = 0, fixOn = -1;
        vec3 pos, head, gaze;
        float yaw = 0.0f;
    };
    JerryInfo jerryInfo() const;
    bool jerrySet(const std::string& key, double v);
    // V13.3 Level 2/3 (KREATUR_Hound.md, Abschnitt "Rudel"; presence_hound_pack.cpp): Hound-Rudel aus der Rolle Hound -
    // Variante 1 (Level 2: zieht langsam umher, gestoert schlagartig schnell, kreist ein) und 2 (Level 3: lauern allein, bellen,
    // jagen gemeinsam, quaelen). packSet (Tests, Skripte): spawn <abstand>, phase, quick, interest, bark, lunge; packInfo: Zahlen.
    // houndMute: Kehle verletzt (Sekunden) - kein Ruf; packSlow: Bein verletzt (Tempo-Faktor 0..1)
    struct PackInfo {
        bool active = false, encircled = false, gaveUp = false;
        int hounds = 0, phase = 0, bites = 0, barks = 0, answers = 0, fast = 0, lunges = 0, aborted = 0, l3 = 0, torment = 0;
        float barkEvery = 0.0f, interest = 0.0f, trap = 0.0f;
        vec3 caller;
    };
    PackInfo packInfo() const;
    bool packSet(const std::string& key, double v);
    float houndMute(int target) const;
    float packSlow(int target) const;
    // V13.3 Level 3 (KREATUR_Clump.md, Abschnitt "Level 3"; presence_clump_l3.cpp): das grosse, bewaffnete Buendel aus der Rolle
    // Clump - Variante 1 (Rohr, Knochenspiess, Knochensichel) und 2 (entwaffnet: zweiter Knochenspiess). bigClumpSet (Tests,
    // Skripte): spawn <abstand>, state, ceil <0|1>, disarm, quick; bigClumpInfo: Zahlen
    struct BigClumpInfo {
        bool active = false;
        int state = 0, variant = 0, strikes = 0, hits = 0, misses = 0, tears = 0, drops = 0, fled = 0;
        float ceil = 0.0f;
        vec3 pos;
    };
    BigClumpInfo bigClumpInfo() const;
    bool bigClumpSet(const std::string& key, double v);

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
    // V13.3: Growler, Nguithr'xurh (eigene Dateien)
    void regieGrowler(double dt, const Context& c, std::vector<Event>& out);
    bool spawnGrowler(const Context& c, std::vector<Event>& out, bool forced);
    void thinkGrowler(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void regieNguithr(double dt, const Context& c, std::vector<Event>& out);
    bool spawnNguithr(const Context& c, std::vector<Event>& out, bool forced);
    void thinkNguithr(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepGN(Agent& a, const Context& c);  // Massstab, Leuchten, Netzkugeln an die Animation (auch Puppen und Vorfuehrungen)
    void preGN(double dt, const Context& c);  // je Bild vor allem anderen: Rauschen neu sammeln, Betaeubung klingt ab
    void prepNguithr(Agent& a, const Context& c);  // Spinne (Groesse, Lage, Augenschein) und Netzkugeln an die Animation
    // V13.3: Ouroboros-Arbeiter (presence_ouroboros.cpp)
    void regieOuroboros(double dt, const Context& c, std::vector<Event>& out);
    bool spawnOuroboros(const Context& c, std::vector<Event>& out, bool forced);
    void thinkOuroboros(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepOuroboros(Agent& a, const Context& c);  // Kanaele -> Animation (auch Puppen und Vorfuehrungen)
    // V13.3: Plague Goblins (presence_goblin.cpp)
    void regieGoblin(double dt, const Context& c, std::vector<Event>& out);
    bool spawnGoblin(const Context& c, std::vector<Event>& out, bool forced);
    void thinkGoblin(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepGoblin(Agent& a, const Context& c);  // Zustand, Groesse, Leuchten -> Animation (auch Puppen und Vorfuehrungen)
    // V13.3 Teil 4, Level 2: Fenster am Gangende (presence_window_l2.cpp), kleine Motten an den Roehren
    // (presence_moth_l2.cpp), Ratten an Faessern und Essensresten (presence_rat_l2.cpp)
    bool windowL2(const Context& c) const;
    void regieWindowL2(double dt, const Context& c, std::vector<Event>& out);
    bool spawnWindowL2(const Context& c, std::vector<Event>& out, bool forced);
    bool mothL2(const Context& c) const;
    void regieMothL2(double dt, const Context& c, std::vector<Event>& out);
    bool spawnMothL2(const Context& c, std::vector<Event>& out, bool forced);
    void thinkMothL2(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepMothL2(Agent& a);  // Massstab nach Variante (auch Puppen und Vorfuehrungen)
    bool ratFoodSpawn(const Context& c, vec3& out);
    bool ratFeed(Agent& a, double dt, const Context& c, std::vector<Event>& out, bool scare);
    // V13.3 Teil 4: Jerry auf seinem Rohr (presence_jerry.cpp): Auftreten, Denken, Blick und Bann aller Spieler, Kanaele
    void regieJerry(double dt, const Context& c, std::vector<Event>& out);
    bool spawnJerry(const Context& c, std::vector<Event>& out, bool forced);
    void thinkJerry(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void jerryGaze(double dt, const Context& c, std::vector<Event>& out);
    // V13.3 Level 3: Death Rats wie Wasserschweine mit Fallen (presence_rat_l3.cpp), Mottenschwarm mit Praetorianern
    // (presence_moth_l3.cpp) - Varianten der Rollen Rat (2 Ratte, 3 Falle) und Moth (3, 4 gewoehnlich, 5 Praetorianer)
    bool ratL3(const Context& c) const;
    void regieRatL3(double dt, const Context& c, std::vector<Event>& out);
    bool spawnRatL3(const Context& c, std::vector<Event>& out, bool forced);
    void thinkRatL3(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkRatTrap(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    bool trapSite(const Context& c, vec3& pos, float& yaw, bool cornered);
    Agent* buildTrap(const Context& c, vec3 pos, float yaw, std::vector<Event>& out);
    void ratSay(Agent& a, int line, std::vector<Event>& out);
    void prepRatL3(Agent& a, const Context& c);  // Variante -> Animation (auch Puppen und Vorfuehrungen)
    bool mothL3(const Context& c) const;
    void regieMothL3(double dt, const Context& c, std::vector<Event>& out);
    bool spawnMothL3(const Context& c, std::vector<Event>& out, bool forced);
    void thinkMothL3(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkPraetorian(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepMothL3(Agent& a, const Context& c);
    void swarmRouse(bool on);
    bool swarmRoused() const;
    bool roomAllowed(const Context& c, vec3 p, int kin) const;
    bool wander(Agent& a, double dt, const Context& c, double speed, int kin);
    // V13.3 (presence_lure.cpp): Smiler am Leuchtkoeder (true: das Licht hat ihn, der Rest von thinkSmiler entfaellt);
    // Skin-Stealer angesprochen (true: diese Runde erledigt)
    bool baitSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out, float d, bool dark);
    bool unprovokedSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out, float d, bool dark);  // V13.3 Level 3 (presence_smiler_l3.cpp)
    // V13.3 Level 3: Red Knight (presence_knight.cpp)
    void regieKnight(double dt, const Context& c, std::vector<Event>& out);
    bool spawnKnight(const Context& c, std::vector<Event>& out, bool forced);
    void thinkKnight(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    bool talkSkinStealer(Agent& a, double dt, const Context& c, std::vector<Event>& out, float d, bool los);
    // V13.3 Level 3 (presence_kin_l3.cpp): Wretch mit Waffe, Skin-Stealer im Mantel; Modellwechsel ohne Neuanlage
    void thinkWretchL3(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void thinkSkinL3(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void swapVariant(Agent& a, int v);
    // V13.3 Level 2/3: Hound-Rudel (presence_hound_pack.cpp), grosses Buendel (presence_clump_l3.cpp)
    void regieHoundPack(double dt, const Context& c, std::vector<Event>& out);
    bool spawnHoundPack(const Context& c, std::vector<Event>& out, double dist);
    void thinkPackHound(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepPackHound(Agent& a);
    void regieClumpL3(double dt, const Context& c, std::vector<Event>& out);
    bool spawnClumpL3(const Context& c, std::vector<Event>& out, double dist);
    void thinkClumpL3(Agent& a, double dt, const Context& c, std::vector<Event>& out);
    void prepClumpL3(Agent& a, const Context& c);
    void updateHive(double dt, const Context& c, std::vector<Event>& out);
    bool walkTo(Agent& a, const NavField& f, double speed, double dt, const Context& c);
    bool hiddenSpawn(const Context& c, int lo, int hi, bool behind, vec3& out, const std::function<bool(vec3)>& ok = {});
    bool pickGoal(const Context& c, Agent& a, int lo, int hi, bool away);
    void remove(size_t i);

    std::vector<std::unique_ptr<Agent>> agents_;
    std::vector<const Agent*> cull_;  // V13.3 (B-026): nach der Wesen-Schleife zu entfernen (abgeraeumte Fallen)
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
    // V13.3: Growler, Nguithr'xurh - naechster Auftritt, Rauschen beim Spieler, Betaeubung je Spieler
    double nextGrowler_ = 60.0, nextNguithr_ = 20.0, corpseT_ = 45.0;
    float static_ = 0.0f;
    vec3 selfEye_;
    std::array<float, 4> stupor_{};
    std::array<double, 4> stuporHold_{};  // so lange klingt die Betaeubung noch nicht ab (s)
    // V13.3: Ouroboros-Arbeiter - Baustelle gesetzt, Zeitraffer (Tests, Bilder), Stimmen, Ersatz fuer vergangene Arbeiter
    bool ouroPlaced_ = false;
    double ouroQuick_ = 1.0, ouroMurmurT_ = 6.0, ouroTry_ = 0.0;
    std::shared_ptr<void> ouroSite_;
    // V13.3: Plague Goblins - die Bande (Zustand in presence_goblin.cpp), naechster Auftritt
    std::shared_ptr<void> goblinCtl_;
    double nextGoblin_ = 50.0;
    // V13.3 Teil 4, Level 2: naechstes Fenster, naechste Mottengruppe
    double nextWindowL2_ = 45.0, nextMothL2_ = 8.0;
    // V13.3 Teil 4: Jerry - Bann je Spieler, erreichte Stufe, naechster Versuch, ihn auf sein Rohr zu setzen
    std::array<float, 4> spell_{};
    std::array<int, 4> spellStage_{};
    double jerryTry_ = 0.0, jerryQuick_ = 1.0;
    // V13.3 Level 3: Gedaechtnis der Death Rats (Weg des Spielers, Lampe, Fallen), der Mottenschwarm
    std::shared_ptr<void> l3Mem_, l3Swarm_;
    // V13.3 Level 2/3: Rudel (Zustand in presence_hound_pack.cpp), grosses Buendel (presence_clump_l3.cpp)
    std::shared_ptr<void> pack_, bigClump_;
    // V13.3 Level 3 (KREATUR_RedKnight.md): Regie des Red Knight (presence_knight.cpp)
    std::shared_ptr<void> knight_;
};

}  // namespace lim::game
