// Eine laufende Spielsitzung: Welt, Spieler, Werte, Inventar und Entities.
//
// Die Sitzung kennt weder Renderer noch Audio noch Oberflaeche. Sie liefert
// Zustaende (Kamera, Nebel, Objekte) und Ereignisse (Schritte, Meldungen,
// Aufnehmen ...), die die Anwendung an die jeweiligen Systeme weiterreicht.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "ecs/registry.hpp"
#include "gameplay/body.hpp"
#include "gameplay/content.hpp"
#include "gameplay/explore.hpp"
#include "gameplay/finds.hpp"
#include "gameplay/flashlight.hpp"
#include "gameplay/haunt.hpp"
#include "gameplay/runlog.hpp"
#include "gameplay/figure.hpp"
#include "gameplay/presence.hpp"
#include "gameplay/inventory.hpp"
#include "gameplay/lure.hpp"
#include "gameplay/pawn.hpp"
#include "gameplay/player.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/stats.hpp"
#include "gameplay/visible.hpp"
#include "world/world.hpp"

namespace lim::props {
struct Skin;
}
namespace lim::world {
struct MapTransition;
}

namespace lim::game {

enum class Surface { Carpet, Concrete, Tile, Metal, Wood, Puddle };  // V11: Puddle (Pfuetze auf Beton)

struct GameEvent {
    enum Type { Toast, ZoneTitle, Footstep, Land, Pickup, Consume, InventoryFull, Died, AutosaveDue, Jump, Exhausted, Drop, ItemLand,
                // V7
                FlashToggle, FlashEmpty, NoteRead, ExitOpened, PhantomStep, Blackout, GhostSeen,
                // V8
                RegionFound, DistantSlam, SoundAt, TeleportCut,
                // V9
                FuseTaken, StalkerAppear, StalkerStep, StalkerSeen, StalkerHit, StalkerGone, EscapeStart, Escaped,
                // V10: Ereignis einer Figur (index = Presence::Event::Type, role = Presence::Role)
                Figure,
                // V11: Sicherungspunkt erreicht (text = Name)
                Checkpoint,
                // V13.2 Koop: Schritt eines Mitspielers (pos, surface, intensity)
                OtherStep };
    Type type = Toast;
    std::string text;
    float duration = 2.2f;
    Surface surface = Surface::Concrete;
    std::string sound;
    float intensity = 1.0f;
    bool rare = false;  // V6: seltener Fund (eigener Klang)
    vec3 pos;           // V6: Ort des Ereignisses (Aufprall abgelegter Items)
    int index = -1;     // V7: Notiz-Nummer bzw. Ziel-Level
    int role = -1;      // V10: Figur (Presence::Role)
    std::string anchor; // V11: Uebergang der Karte (id; das Gegenstueck im Ziel heisst gleich)
};

// V7: Lichtkegel der Taschenlampe fuer dieses Bild
struct FlashBeam {
    bool on = false;
    vec3 pos, dir;
    float power = 0.0f;  // 0..1 (Flackern bei schwacher Batterie)
};

// V6: Kamera fuer dieses Bild (Ego-Perspektive im Kopf oder Schulterkamera)
struct ViewCamera {
    vec3 pos;
    float yaw = 0, pitch = 0, roll = 0;
    float fovMul = 1.0f;
};

// Klangquelle in der Welt (fuer das Audiosystem)
struct SoundSource {
    ecs::Entity entity;
    const std::string* sound;
    vec3 pos;
    float volume, radius;
    int* voice;
};

// V13: Geometrie der Hand aus dem Skelett von vm_hand_l (Lampenrahmen): Handgelenk, Unterarm (Ellbogen -> Handgelenk),
// Handrichtung, Handflaeche. false ohne passende Gelenke
bool vmGeometryFromSkin(const props::Skin& sk, vec3& wrist, vec3& forearm, vec3& handDir, vec3& palm);

class GameSession {
public:
    GameSession(const Content& content, const PrefabLibrary& prefabs, std::shared_ptr<const world::LevelDef> level,
                i64 seed, const std::string& difficulty, const std::string& name, JobSystem& jobs, int loadRadius);
    ~GameSession();

    // Weiterleitung der Chunk-Ereignisse (Renderer)
    std::function<void(const world::ChunkData&, bool loaded)> chunkListener;

    void placeAtSpawn();
    // V11: Uebergang der Karte fuehrt in eine Ebene, die es (noch) nicht gibt. Nach einem Sturz ins
    // Bodenloch kaeme man sonst nicht mehr heraus: zurueck zum Anfang. true = Spieler wurde versetzt.
    bool rescueAfterFailedTransition();
    // V11: Wiedereinstieg nach dem Tod am letzten Sicherungspunkt (feste Karten; sonst am Startpunkt):
    // volle Health, Verstand mindestens 60 %, Inventar bleibt; Stromausfall vorbei, Entities fort, Schonzeit
    void respawnAtCheckpoint();
    // V11: Ankunft ueber einen Uebergang: am gleichnamigen Anker der Karte; der naechste Sicherungspunkt
    // gilt als erreicht. false = kein solcher Anker (dann Startpunkt)
    bool placeAtAnchor(const std::string& id);
    const std::string& checkpointLabel() const { return checkpointLabel_; }
    const std::string& checkpointId() const { return checkpoint_; }
    int killRole() const { return killRole_; }       // V11: Rolle der toetenden Entity (-1 keine)
    bool killCamActive() const { return killCam_; }  // V11: die Kamera sieht dem Toetenden ins Gesicht
    bool underwater() const { return player_.underwater; }  // V11
    double air() const { return air_; }
    void preload();
    void update(double dt, const PlayerControl& control, bool paused);

    // Interaktion
    ecs::Entity target() const { return target_; }
    std::string targetPrompt() const;
    void interact();
    void useSlot(int idx);
    void selectSlot(int idx);
    // V7: Taschenlampe
    void toggleFlashlight();
    const Flashlight& flashlight() const { return flash_; }
    Flashlight& flashlight() { return flash_; }
    FlashBeam flashBeam() const;
    // V7: Notizbuch (gelesene Notizen in Fundreihenfolge), Notausgaenge
    const std::vector<int>& journal() const { return journal_; }
    struct KnownExit {
        std::string id;
        vec3 pos;
        float yaw;
    };
    const std::vector<KnownExit>& knownExits() const { return knownExits_; }
    // Leuchtschilder in der Naehe (Position, Blickrichtung des Schilds) fuer gruenes Licht
    void exitSigns(std::vector<std::pair<vec3, vec3>>& out, float radius) const;
    // Uebernahme beim Ebenenwechsel (Inventar, Werte, Lampe, Notizbuch, Laufbuch)
    void carryOver(const GameSession& from);
    // V7: Halluzinationen (Einstellung; Stromausfall fuer Renderer und Klang)
    bool hallucinations = true;
    Haunt::Blackout blackout() const { return haunt_.blackout(); }
    void forceHaunt(int what) { haunt_.force(what); }
    const Haunt& haunt() const { return haunt_; }
    // V10: Figuren (Praesenz) - Einstellung (0 aus, 1 ohne Angriffe, 2 alle), Detailstufe (0..3),
    // Zustand fuer Klang und Bild, Test
    int figureMode = 2;
    int figureDetail = 2;
    Presence& presence() { return presence_; }
    const Presence& presence() const { return presence_; }
    void forceStalker(double dist) { presence_.force(Presence::Role::Hound, dist); }
    // Knochenmatrizen der Figuren dieses Bildes (collectMeshes/collectBody fuellen sie)
    const std::vector<mat4>& bonePalette() const { return palette_; }
    void collectShadows(std::vector<ShadowCaster>& out);
    // V10: eigener Koerper realistisch (sonst das Strichmaennchen aus V6)
    bool realisticBody = true;
    // V9: Sicherungen (eine aus jeder Ebene) und der Weg hinaus
    static bool isFuse(const std::string& kind) { return kind.rfind("fuse_", 0) == 0; }
    const std::vector<std::string>& fuseKinds() const;
    bool hasFuse(const std::string& kind) const { return inventory_.count(kind) > 0; }
    int fusesHeld() const;
    std::string fuseLabel(const std::string& kind) const;  // "Büros" aus "Sicherung (Büros)"
    struct FuseLight {
        vec3 pos, dir;
        float phase = 0.0f;
    };
    void fuseLights(std::vector<FuseLight>& out, float radius) const;
    struct KnownFuse {
        std::string id, item;
        vec3 pos;
        float yaw = 0.0f;
    };
    const std::vector<KnownFuse>& knownFuses() const { return knownFuses_; }
    bool escaping() const { return ride_.active && ride_.escape; }
    float escapeGlow() const;  // 0..1 waehrend der letzten Fahrt (die Kabine wird immer heller)
    void finishEscape();       // nach dem Abspann: die Tuer geht auf, weiter erkunden
    const std::string& deathCause() const { return deathCause_; }
    // V8: Aufzuege - faehrt gerade einer? Kabinenlicht (Position) in der Naehe
    bool riding() const { return ride_.active; }
    void elevatorLights(std::vector<vec3>& out, float radius) const;
    // V8: Ebene, in der sich der Spieler gerade befindet (in der Welt; sonst das Level selbst)
    int currentRegion() const { return region_ < 0 ? 0 : region_; }
    std::string regionName() const;  // "Level 1 · Das Tiefgeschoss"
    // V8: Ist die Ebene dieser Zone schon entdeckt? (Zonennamen unbekannter Ebenen bleiben verborgen)
    bool zoneKnown(int zone) const;
    // V7: Laufbuch
    const RunLog& runLog() const { return runLog_; }
    RunLog& runLog() { return runLog_; }
    // V6: Item eines Platzes ablegen (vor dem Spieler leicht werfen); all = ganzer Stapel
    bool dropSlot(int idx, bool all);
    // V6: Stapel aus der Hand (Inventar) ablegen
    void dropStack(const std::string& kind, int count);
    size_t droppedCount() const { return dropped_.size(); }
    // V13.3 (session_lure.cpp): Leuchtstab (KREATUR_Smiler.md) - Licht der Staebe (geworfen und in der Hand) fuer die
    // Darstellung; Ansprechen (KREATUR_SkinStealer.md) - eine menschenaehnliche Gestalt in wenigen Metern im Blick
    struct GlowLight {
        vec3 pos;
        float power = 0.0f;  // 0..1
    };
    void glowLights(std::vector<GlowLight>& out, float radius) const;
    const LureSystem& lures() const { return lures_; }
    bool glowThrowPending() const { return glowPending_ > 0.0; }
    bool speakTarget(vec3* head = nullptr) const;
    void speak();

    // Darstellung
    const world::Room* currentRoom() const { return room_; }
    vec3 fogColor() const { return fog_; }
    float fogDensity() const { return (float)fogDensity_; }
    void collectMeshes(std::vector<VisibleMesh>& out);
    void collectSounds(std::vector<SoundSource>& out);
    // V6: eigener Koerper (Strichmaennchen); firstPerson: ohne Kopf
    void collectBody(std::vector<VisibleMesh>& out, bool firstPerson);
    // V6: Kamera dieses Bildes (Ego-Perspektive oder Schulterkamera mit Wandkollision)
    ViewCamera camera(bool thirdPerson, double dt);

    // Spielstand
    Json toJson() const;
    void fromJson(const Json& j);  // V3/V4/V5/V6
    void reviveAfterDeath();

    std::vector<GameEvent>& events() { return events_; }
    world::World& world() { return *world_; }
    Player& player() { return player_; }
    Stats& stats() { return stats_; }
    Inventory& inventory() { return inventory_; }
    ecs::Registry& registry() { return reg_; }
    const Content& content() const { return content_; }
    Body& body() { return body_; }
    ExploreMap& explore() { return explore_; }
    const ExploreMap& explore() const { return explore_; }

    // --- V13.2 Koop (MULTIPLAYER_ARCHITEKTUR.md) ---------------------------------------------------------------
    // Host: der Rechner rechnet die Welt, die Figuren der Gaeste (pawns) bewegt er mit deren Eingaben. Gast: die
    // eigene Figur wird vorhergesagt und vom Host korrigiert; Absichten gehen an den Host (netAction), die Wesen
    // sind Puppen, Werte und Inventar kommen vom Host.
    enum class NetRole { Solo, Host, Guest };
    NetRole netRole = NetRole::Solo;
    int netSelfId = 0;          // Spielernummer dieses Rechners (Host 0)
    std::vector<Pawn*> pawns;   // die anderen Spieler (gehoeren der Koop-Schicht)
    // Host
    void pawnInput(Pawn& p, const PlayerControl& c, double dt);  // eine Eingabe des Gasts (dieselbe Physik)
    void pawnTick(Pawn& p, double dt);                           // je Bild: Werte, Lampe, Koerper, Laerm, Tod
    // Absichten des Gasts, am Zustand des Hosts geprueft (Reichweite, Sicht, Inventar); false: abgelehnt
    bool pawnInteract(Pawn& p, u32 itemHash, std::string& msg, int& note);
    bool pawnUse(Pawn& p, int slot, std::string& msg, std::string& sound);
    bool pawnDrop(Pawn& p, int slot, bool all);
    bool pawnSpeak(Pawn& p, int phrase);  // V13.3: Interact mit 0..3 statt Kennzahl - der Gast spricht an, wen er ansieht
    void pawnRespawn(Pawn& p);
    // Uebergang, in dem dieser Spieler gerade steht (nullptr: keiner); needsOk: die noetige Sache ist dabei
    const world::MapTransition* transitionAt(const Player& p, const Inventory& inv, bool& needsOk) const;
    // Ein Uebergang fuer die Gruppe: Ziel und Anker (Host und Gast setzen damit dieselbe Ebene)
    void respawnCoop();  // eigener Spieler (Host oder Gast): am Sicherungspunkt, ohne die Wesen zu entfernen
    struct NetWorldEvent {
        enum Kind { Picked, Dropped } kind = Picked;
        u32 hash = 0;
        std::string itemKind;
        vec3 pos;
        int by = 0;
    };
    std::vector<NetWorldEvent> netWorldOut;                // Host: an alle Gaeste
    std::vector<Presence::Event> netFigureOut;             // Host: Ereignisse der Wesen, die einen Gast betreffen
    std::vector<std::pair<vec3, float>> netBlackoutOut;    // Host: Stromausfaelle (Ort, Dauer)
    struct NetPersonal {
        int to = 0;
        std::string text;
    };
    std::vector<NetPersonal> netPersonalOut;               // Host: Meldungen an einen Gast (leere Lampe ...)
    struct NetSound {
        std::string sound;
        vec3 pos;
        float volume = 1.0f;
        int except = -1;  // dieser Spieler hoert es schon selbst (-1: alle)
    };
    std::vector<NetSound> netSoundOut;                     // V13.3 Host: Klaenge fuer die Gaeste (Ansprechen, Wurf)
    // Gast
    std::function<void(int kind, u32 arg)> netAction;      // Absichten an den Host (statt sie hier auszufuehren)
    void netRemoveItem(u32 hash);
    void netAddDropped(u32 hash, const std::string& kind, vec3 pos);
    void netPowerCut(vec3 pos, float seconds);
    void netFlicker(vec3 pos);
    std::vector<Presence::NetState> netCreatures;          // letzte Momentaufnahme der Wesen
    double netCreatureStamp = -1.0;
    bool netCreaturesFresh = false;
    u32 targetHash() const;   // Kennzahl des anvisierten Gegenstands (0: keiner)
    void netDie(int role);    // Gast: der Host meldet den eigenen Tod (Schreck wie im Einzelspiel)
    // Gast: Stand des Hosts nach der zuletzt verarbeiteten Eingabe setzen und die unbestaetigten Eingaben nachrechnen;
    // kleine Abweichungen gleichen sich weich aus, grosse springen. Liefert die Abweichung (m)
    struct NetReplay {
        PlayerControl c;
        double dt = 0.0;
        float yaw = 0.0f, pitch = 0.0f;
    };
    double netReconcile(vec3 pos, vec3 vel, const std::vector<NetReplay>& replay, bool snap);
    u32 sharedHash() const;   // Pruefsumme des geteilten Zustands (aufgenommen, abgelegt) - Desync-Pruefung
    int itemCount() const;    // Gegenstaende in der Welt (Tests)
    // Vorraete fuer mehrere Spieler (1 + 0,6 x (n - 1)); vor placeAtSpawn/preload setzen, gilt fuer die ganze Ebene
    void setSupplyScale(double s) {
        supplyScale_ = s;
        world_->setSupplyScale(s);
    }
    double supplyScale() const { return supplyScale_; }
    double supplyScale_ = 1.0;
    bool pawnBeam(const Pawn& p, vec3& pos, vec3& dir, float& power) const { return p.beam(pos, dir, power, stats_.playTime); }

    std::string name, difficulty, slot, levelId;
    i64 seed;
    std::unordered_set<std::string> picked;
    // V11: Seed des ganzen Laufs (jede Ebene leitet ihren Seed davon ab) und Gedaechtnis besuchter Ebenen:
    // wer zurueckkehrt, findet sie so vor, wie er sie verlassen hat
    i64 runSeed = 0;
    i64 runSeedOr() const { return runSeed ? runSeed : seed; }
    struct LevelMemory {
        std::unordered_set<std::string> picked;
        std::unordered_map<std::string, int> notes;
    };
    std::unordered_map<std::string, LevelMemory> levelMemory;
    double autosaveTimer = 0.0;
    static constexpr double kAutosave = 180.0;

private:
    class TileAdapter;
    phys::TileSource& tileSrc();  // V13.2 (session_coop.cpp)
    void onChunk(const world::ChunkData& cd, bool loaded);
    void findTarget();
    void supplyHint(double dt);
    void updateFog(double dt);
    void updateWanderers(double dt);
    void spawnLevelEntities(const world::ChunkData& cd);
    void spawnFinds(const world::ChunkData& cd);  // V7
    void trackExits(double dt);
    void trackTransitions();          // V11
    std::string transitionInside_;    // V11: id des Ausloesebereichs, in dem der Spieler gerade ist
    std::string lastTransitionKind_;  // V11
    bool transitionsArmed_ = false;   // V11: erst nach dem ersten Bild (Ankunft im Ausloesebereich zaehlt nicht)
    std::unordered_map<std::string, int> mapNotes_;  // V11: Notiz je Spawnpunkt der Karte (stabil, keine doppelt)
    void trackCheckpoints();                         // V11
    void updateWater(double dt);                     // V11: Atem, Kaelte, die Tiefe
    double air_ = 30.0;                              // V11: Luft unter Wasser (s)
    double deepT_ = 0.0;                             // V11: Zeit im tiefen Wasser (Level 7)
    int deepStage_ = 0;
    std::string checkpoint_, checkpointLabel_;       // V11: letzter Sicherungspunkt
    int killRole_ = -1;                              // V11
    double hitScale_ = 1.0;                          // V13.2: Schaden der Schlaege (stalker_hit / 35)
    bool killCam_ = false;                           // V11
    double killT_ = 0.0;                             // V11: Zeit seit dem Zugriff
    float killYaw0_ = 0.0f, killPitch0_ = 0.0f;      // V11: Blick im Moment des Zugriffs
    // V6: abgelegte Items
    struct DroppedItem {
        std::string id, kind;
        vec3 pos;
        float yaw = 0;
        ecs::Entity entity = ecs::kNull;
        u32 netHash = 0;  // V13.2 Koop (Gast): Kennzahl beim Host
    };
    ecs::Entity spawnDropped(DroppedItem& d, vec3 vel, bool resting);
    void throwItem(const std::string& kind, int count);
    // V13.2 Koop: werfen aus der Hand eines beliebigen Spielers (by: Spielernummer)
    void throwFrom(const Player& p, vec3 eye, const std::string& kind, int count, int by);
    void updatePawnsPresence(Presence::Context& c);
    std::vector<Presence::Other> others_;
    // V11: Laerm an anderen Orten in diesem Bild (Aufprall geworfener Dinge) und Lager des Buendels
    std::vector<std::pair<vec3, float>> noisePings_;
    std::vector<vec3> clumpLairs_;
    bool lairsRead_ = false;
    double landNoise_ = 0.0;
    void updateDropped(double dt);
    Surface surfaceUnder() const;

    const Content& content_;
    const PrefabLibrary& prefabs_;
    std::unique_ptr<world::World> world_;
    std::unique_ptr<TileAdapter> tiles_;
    Player player_;
    Stats stats_;
    Inventory inventory_;
    ecs::Registry reg_;
    std::vector<GameEvent> events_;
    ecs::Entity target_ = ecs::kNull;
    const world::Room* room_ = nullptr;
    int zone_ = -1;
    vec3 fog_{0.1f, 0.1f, 0.1f};
    double fogDensity_ = 0.05;
    bool fogInit_ = false;
    double hintTimer_ = 0.0;
    std::unordered_set<std::string> hinted_;
    bool deathReported_ = false;
    // V6
    Body body_;
    ExploreMap explore_;
    double tpDist_ = 0.3;  // Schulterkamera: aktueller Abstand (weich nach Kollision)
    float tpPivotZ_ = 0.0f;
    bool tpInit_ = false;
    std::vector<DroppedItem> dropped_;
    long long nextDrop_ = 1;
    // V7
    Flashlight flash_;
    // V13: Schaltfolge - an: Arm hebt die Lampe, der Daumen drueckt, dann Licht; aus: Daumen drueckt, Licht aus, dann
    // senken. pending 1 = heben fuer an, 2 = Druck fuer an laeuft, -1 = Druck fuer aus laeuft
    bool flashRaise_ = false;
    int flashPending_ = 0;
    double flashClickT_ = -1.0, flashLowerT_ = -1.0;
    void flashClick();
    std::string vmHandMesh_ = "vm_hand_l";
    int vmHandId_ = -2;
    std::vector<mat4> vmHandBones_;
public:
    int debugDraw = 0;  // V13 (Tests): Bit 0 Hand mit Skelett aus, Bit 1 Koerper des Spielers aus, Bit 2 Lampe aus
private:
    std::vector<int> journal_;
    std::vector<KnownExit> knownExits_;
    double exitScan_ = 0.0;
    Haunt haunt_;
    std::vector<Haunt::Event> hauntEvents_;
    RunLog runLog_;
    // V8: Ebenen der zusammenhaengenden Welt
    void trackRegion(double dt);
    // V8: Aufzuege (gameplay/elevator.cpp)
    struct ElevatorLink {
        std::string to;
        vec3 door;
        vec2 dir;
        int layer = 0;
    };
    struct Ride {
        bool active = false, moved = false, arrived = false;
        std::string fromId, destId;
        vec3 fromDoor, destDoor;
        vec2 fromDir, destDir;
        int fromLayer = 0, destLayer = 0;
        double t = 0.0, travel = -1.0;
        bool escape = false, escaped = false;  // V9: die letzte Fahrt (mit allen drei Sicherungen)
    };
    std::map<std::string, ElevatorLink> elevLinks_;  // gefahrene Verbindungen (beide Richtungen, gespeichert)
    Ride ride_;
    void spawnElevators(const world::ChunkData& cd);
    void updateElevators(double dt, double prevX, double prevY);
    bool startRide(ecs::Entity e);
    bool findElevatorTarget(const std::string& fromId, vec3 door, int layer, ElevatorLink& out);
    ecs::Entity elevatorById(const std::string& id);
    int strataLayer(double x, double y) const;
    static bool inCabin(const vec3& door, float yaw, double x, double y, double grow = 0.0);
    ecs::Entity cabinAt(double x, double y);
    int region_ = -1, regionCand_ = -1;
    double regionTime_ = 0.0;
    bool slamPending_ = false;
    void updateHaunt(double dt);
    static constexpr size_t kMaxDropped = 400;
    // V9
    // V10
    Presence presence_;
    std::vector<Presence::Event> presenceEvents_;
    void updatePresence(double dt);
    // V13.3 (session_lure.cpp): Leuchtstaebe, Wurf (Geste, dann Loslassen), Koeder fuer die Praesenz, Ansprechen
    LureSystem lures_;
    std::vector<Presence::Lure> presenceLures_;
    double glowPending_ = -1.0, speakCool_ = 0.0;
    int glowSlot_ = -1;
    bool startGlowThrow(int idx);
    void launchGlowstick(const Player& p, vec3 eye, int by);
    void updateLures(double dt);
    void netAddLure(vec3 pos);
    std::vector<mat4> palette_;
    FigureRig playerRig_;
    int bodyStyle_ = -1;  // angewendete Koerperart (0 Strichmaennchen, 1 realistisch)
    void applyBodyStyle();
    static bool vmHandModel();  // V12: Hand aus Blender vorhanden
    std::string deathCause_;
    std::vector<KnownFuse> knownFuses_;
    double fuseScan_ = 0.0;
    void trackFuses(double dt);
    void cabinHints();
    bool cabinHinted_ = false;
    int cabinFuses_ = -1;
    bool startEscape(ecs::Entity e);
    Surface surfaceAt(double x, double y) const;
};

}  // namespace lim::game
