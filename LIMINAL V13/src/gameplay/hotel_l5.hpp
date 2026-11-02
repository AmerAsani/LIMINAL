// V13.3 Teil 4 Level 5: Wesen des Hotels ohne eigene Rolle (Wikidot level-5 Rev. 184; trimmed:entity-89 Rev. 35; entity-18
// Rev. 54; Woodlins und Watchers nur als Namen - ihre verlinkten Seiten gibt es nicht mehr; alle CC BY-SA 3.0). Modelle:
// tools/blender/models/models_l5_wesen.py. Briefings: KREATUR_Ants.md, KREATUR_Woodlins.md, KREATUR_Watchers.md,
// KREATUR_Gastgeber.md, KREATUR_Samantha.md (Fleisch).
//
//   Kolonien (Entity 89): Dutzende bis Hunderte flauschige, runde, lichtschluckende Tierchen (2-6 cm) in den dunkelsten
//     Ecken der Zimmer - zusammengedraengt wie ein grosses schwarzes Wesen. Sie kommen nur, wer naeher als 0,9 m tritt (oder
//     sie reizt): dann klettern sie in Sekunden am Spieler hoch und beissen wie Blutegel (leichte Blutergüsse, nie toedlich).
//     Abzupfen: die Benutzen-Taste (ohne Ziel) - je Griff ein Buschel; was abfaellt, kehrt in die Ecke zurueck.
//   Woodlins (nur Name, Auslegung): kleine Wesen aus dunklem, gedrechseltem Holz, die in den Moebeln der Haupthalle hausen und
//     reglos wie Schnitzwerk sitzen, solange jemand hinsieht. Ungesehen huschen sie naeher (Klacken von Holz, Knarren); wer
//     ihnen lange den Ruecken kehrt, den beisst einer in den Knoechel (Knarren vorher), dann verschwinden alle im Holz.
//   Watchers (nur Name; Level 43: selten, feindselig - Auslegung): in den Tueroeffnungen dunkler Zimmer; lange Finger um den
//     Tuerrahmen, ein langer Kopf mit einer Saeule von Augen lugt um die Kante, wenn man nicht hinsieht. Wer hinsieht, dem
//     zieht er sich langsam zurueck. Wer ihm den Ruecken zukehrt und dicht vorbeigeht, den reisst ein langer Arm (Knacken vorher).
//   Der Gastgeber (Entity 18): hoeflicher Gastgeber und Geschaeftspartner, nennt das Hotel sein Reich; sieht alles, riecht
//     Angst, erscheint nur dem, den er waehlt; benutzt die Bildnisse und Muster der Wande. Eine Stimme bittet in sein Buero -
//     dort reicht eine Hand ueber den Schreibtisch: ein Handschlag, und man habe nie wieder Angst - gegen "ein paar Dinge".
//     Der Handel laesst einen nicht los. Rat: die Ebene verlassen, der Stimme nicht zuhoeren, die Bildnisse nicht ansehen.
//   Fleisch (fuer Samantha): Steaks auf Tabletts des Zimmerservice (vor Zimmertueren, in eingerichteten Zimmern), Dosenfleisch
//     in den Kesselraeumen (Auslegung: Vorrat der Arbeiter) - Spawnpunkte der Karte (kind "trace", set "meat").
//
// Orte: Spawnpunkte der Karte mit kind "trace" (tools/levels/level5.py, set = ants, woodlin, watcher, office, meat).
// Spielentscheidendes (Kolonie loest aus, Zahl am Spieler, Bisse, Hiebe, Handel) rechnet der Host fuer alle; die Gaeste
// bekommen es als Weltereignis Dropped "l5hotel" mit einer Kennzahl (Protokoll unveraendert) und rechnen daraus Bild und Klang.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/json.hpp"
#include "core/math.hpp"
#include "core/rng.hpp"
#include "gameplay/navfield.hpp"
#include "gameplay/visible.hpp"

namespace lim::world {
class World;
struct LevelDef;
}  // namespace lim::world

namespace lim::game {

struct KinLine;

namespace hotel {
// Samantha: Klangnummern (Presence::Event Mimic mit Staerke kSamCueBase + Nummer; kinLine ab 300, kin_l3.hpp)
constexpr float kSamCueBase = 400.0f;  // kKinCue (100) + 300
enum SamCue : int {
    SAM_PURR = 0, SAM_MURMUR0 = 1,  // + 0..2 (Franzoesisch, Italienisch, Englisch)
    SAM_ASK0 = 4, SAM_ASK1 = 5, SAM_ASK2 = 6, SAM_THANK_GOOD = 7, SAM_THANK_POOR = 8, SAM_READ_GOOD = 9, SAM_READ_POOR = 10,
    SAM_WARN = 11, SAM_RUDE = 12, SAM_SCREAM = 13, SAM_BITE = 14, SAM_EAT = 15, SAM_BYE = 16,
    SAM_COUNT = 17
};
float meatQuality(const std::string& item);          // 1 Steak, 0,3 Dosenfleisch, 0 sonst (Wiki: Steak gut, Dosenfleisch schlecht)
const KinLine* samLine(int cue);                      // 0..SAM_COUNT-1
const KinLine* samLineBySound(const std::string& s);
}  // namespace hotel

class HotelL5 {
public:
    static constexpr int kPlayers = 4;  // 0 = dieser Rechner, 1..3 Gaeste beim Host
    static constexpr const char* kNetName = "l5hotel";
    static u32 netKind(u32 code) { return code >> 24; }
    // Masse aus models_l5_wesen.py (gleichlautend)
    static constexpr float kAntR = 0.02f;                       // Halbmesser des Modells ant_ball (Massstab je Tierchen)
    static constexpr float kWlHip = 0.0f, kWlShoulder = 0.22f, kWlLegA = 0.15f, kWlLegB = 0.16f, kWlJaw[3] = {0.049f, 0.0f, 0.307f};  // Runde 4: Kopf 1,4-fach
    static constexpr float kWaArmA = 0.55f, kWaArmB = 0.62f;
    static constexpr float kE18Arm = 0.42f;

    struct Config {
        bool ants = false, woodlins = false, watchers = false, host = false, meat = false;
        double colonyChance = 0.65;  // je Ecke: wohnt hier eine Kolonie? (sie wandern - je Lauf anders)
        int woodlinGroup = 3, watcherCount = 3;
        double woodlinEvery = 140.0;                 // mittlerer Abstand der Auftritte einer Gruppe (s)
        double antBite = 0.012, antFloor = 30.0;     // Gesundheit je Tier und s; nie unter antFloor
        double woodlinBite = 6.0, watcherStrike = 12.0, floor = 10.0;
        double hostAfter = 240.0, hostSanity = 70.0, fearless = 180.0;
    };

    struct Event {
        enum Kind { Toast, SoundAt, Sound2d, Hurt, Sanity, Slow, Deal, Net } kind = Toast;
        int player = 0;      // wen es trifft (Klaenge: nur fuer diesen Rechner)
        std::string text;    // Meldung, Klang
        vec3 pos;
        float value = 1.0f;  // Lautstaerke, Schaden, Dauer der Meldung
        u32 code = 0;        // Net: Kennzahl fuer die Gaeste
    };

    struct PlayerIn {
        int id = -1;
        vec3 feet, eye, look;
        bool alive = false, flash = false;
        vec3 flashPos, flashDir;
        double health = 100.0, sanity = 100.0;
        bool corridor = false;  // steht in einem Gang der Haupthalle
        Viewer view;
    };

    struct Part {
        std::string model;
        int id = -2;
    };

    struct Ball {
        vec3 home, p, v;
        float r = 0.02f, ph = 0.0f, h = 0.0f, ang = 0.0f, hMax = 1.0f;
        int state = 0;   // 0 in der Ecke, 1 stroemt zum Ziel, 2 am Koerper, 3 kehrt zurueck
        int target = -1;
        int model = 0;
    };
    struct Colony {
        vec3 pos;
        float yaw = 0.0f;
        int spot = -1, state = 0, target = -1, triggers = 0;  // 0 Ruhe, 1 unruhig, 2 stroemt aus
        double t = 0.0, cool = 0.0, rustleT = 0.0;
        float rouse = 0.0f;
        std::vector<Ball> balls;
    };

    struct Woodlin {
        int state = 0;     // 0 verborgen, 1 reglos (Schnitzwerk), 2 huscht, 3 Ankuendigung, 4 Biss, 5 flieht ins Holz
        int spot = -1, target = -1;
        vec3 pos, from, to;
        float yaw = 0.0f, upright = 1.0f, gait = 0.0f, jaw = 0.0f;
        double t = 0.0, dur = 1.0, unseen = 0.0, seenT = 0.0, clickT = 3.0;
        Part body, jaw_, legA[4], legB[4];
    };

    struct Watcher {
        int spot = -1, state = 0;   // 0 lauert (nur Finger), 1 lugt, 2 zieht sich zurueck, 3 Ankuendigung, 4 Hieb, 5 fort
        int side = 1, target = -1;
        float peek = 0.0f, grip = 0.0f, arm = 0.0f, tilt = 1.1f;
        double t = 0.0, seenT = 0.0, cool = 0.0, blinkT = 2.0, hiddenT = 0.0, drainT = 0.0;
        int blinkEye = -1;
        float blink = 0.0f;
        vec3 look{1, 0, 0}, armTo;
        bool told = false;
        Part head, face, eye[5], hand[2], armA, armB, claw, body;
    };

    struct Host {
        int chosen = -1, phase = 0;   // 0 nichts, 1 Einladung (Stimme), 2 im Buero (Hand), 3 Handel, 4 Rechnung, 5 vorbei
        double t = 0.0, lineT = 0.0, onLevel = 0.0, outT = 0.0, fearUntil = -1.0, lookDrain = 0.0;
        int line = 0;
        float arm = 0.0f, lamp = 0.0f;
        bool dealt = false, refused = false;
        Part armA, armB, armC, hand, eyes[6][2];
        std::vector<int> portraits;  // Bildnisse mit Augen (Index in portraits_)
    };

    struct Spot {
        vec3 pos;
        float yaw = 0.0f;
        std::vector<std::string> tags;
        int side = 1;
    };

    void setup(const world::LevelDef& level, u64 seed);
    const Config& config() const { return cfg_; }
    Config& config() { return cfg_; }
    bool any() const { return cfg_.ants || cfg_.woodlins || cfg_.watchers || cfg_.host || cfg_.meat; }

    // ein Bild: players[0] dieser Rechner, beim Host 1..3 die Gaeste (beim Gast: die Mitspieler fuer das Bild)
    void update(double dt, const std::array<PlayerIn, kPlayers>& players, bool authoritative, world::World& w, std::vector<Event>& out);

    // Kolonien: wie viele Tierchen am Spieler (0..), abzupfen (Host/Einzelspieler; false: keine dran)
    int antsOn(int player) const;
    float antsCrawl(int player) const;  // 0..1 fuer das Bild (Krabbeln am Rand)
    bool pickAnts(int player, std::vector<Event>& out);
    // der Gastgeber: Phase fuer diesen Spieler, Ort der Hand (zum Benutzen), Handschlag (Host), furchtlos (Restzeit s)
    int hostPhase(int player) const { return host_.chosen == player ? host_.phase : 0; }
    bool handAt(vec3& pos) const;
    bool shake(int player, std::vector<Event>& out);
    double fearless(int player) const;
    const Host& host() const { return host_; }
    vec3 officeDesk() const { return office_.pos; }
    float officeYaw() const { return office_.yaw; }
    bool hasOffice() const { return hasOffice_; }

    // Fleisch: Liegeplaetze (die Sitzung legt die Funde hin)
    struct Meat {
        vec3 pos;
        float yaw = 0.0f;
        std::string item, id;
    };
    const std::vector<Meat>& meats() const { return meat_; }

    // Koop (Gast): Ereignis des Hosts anwenden
    void applyNet(u32 code, vec3 pos, int selfId, std::vector<Event>& out);

    // Skript/Test
    bool force(const std::string& what, int idx, int player, std::vector<Event>& out);
    void setState(const std::string& field, double value);
    const std::vector<Colony>& colonies() const { return col_; }
    const std::vector<Woodlin>& woodlins() const { return wl_; }
    const std::vector<Watcher>& watchers() const { return wa_; }
    const std::vector<Spot>& spots(const std::string& set) const;
    void setNest(vec3 p) { nest_ = p; }  // Deathmoth-Nest (Watchers meiden seine Tuer)
    bool portraitNear(vec3 p, vec3& at, float& yaw) const;  // naechstes Bildnis (Skript, Bilder)

    // Bild
    void collect(std::vector<VisibleMesh>& out, vec3 eye);
    struct Light {
        vec3 pos, color;
        float range = 4.0f;
        vec3 dir{0.0f, 0.0f, -1.0f};
    };
    void lights(std::vector<Light>& out, vec3 eye) const;

    struct Stats {
        int antTriggers = 0, antPicks = 0, woodBites = 0, woodMoves = 0, waStrikes = 0, waPeeks = 0, deals = 0, hostLines = 0;
    };
    const Stats& stats() const { return st_; }

private:
    Config cfg_;
    Stats st_;
    std::vector<Spot> sAnts_, sWood_, sWatch_, sNone_;
    std::vector<Colony> col_;
    std::vector<Woodlin> wl_;
    std::vector<Watcher> wa_;
    std::vector<Meat> meat_;
    struct Portrait {
        vec3 pos;
        float yaw = 0.0f;
    };
    std::vector<Portrait> portraits_;
    Spot office_;
    bool hasOffice_ = false;
    Host host_;
    std::array<float, kPlayers> slowT_{};
    std::array<PlayerIn, kPlayers> lastPl_{};
    world::World* lastW_ = nullptr;
    double clock_ = 0.0, woodT_ = 60.0, woodCool_ = 0.0;
    bool hold_ = false;  // Skript (Bilder): keine Entscheidungen
    vec3 nest_{-1000, -1000, 0};
    Rng rng_{1};
    Part antPart_[4];

    void setupParts();
    void updateAnts(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, std::vector<Event>& out);
    void updateWood(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out);
    void updateWatch(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out);
    void updateHost(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out);
    void triggerColony(Colony& c, int k, int player, std::vector<Event>& out, bool net);
    void detach(Colony& c, int player, int n);
    void placeWatcher(Watcher& a, int spot);
    void hostLine(int line, const std::array<PlayerIn, kPlayers>& pl, std::vector<Event>& out);
    void hostPhaseSet(int phase, std::vector<Event>& out, bool net);
    void woodGroup(const std::array<PlayerIn, kPlayers>& pl, world::World& w, std::vector<Event>& out, bool net, int forceTarget = -1);
};

}  // namespace lim::game
