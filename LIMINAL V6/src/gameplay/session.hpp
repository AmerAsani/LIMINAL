// Eine laufende Spielsitzung: Welt, Spieler, Werte, Inventar und Entities.
//
// Die Sitzung kennt weder Renderer noch Audio noch Oberflaeche. Sie liefert
// Zustaende (Kamera, Nebel, Objekte) und Ereignisse (Schritte, Meldungen,
// Aufnehmen ...), die die Anwendung an die jeweiligen Systeme weiterreicht.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "ecs/registry.hpp"
#include "gameplay/body.hpp"
#include "gameplay/content.hpp"
#include "gameplay/explore.hpp"
#include "gameplay/inventory.hpp"
#include "gameplay/player.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/stats.hpp"
#include "gameplay/visible.hpp"
#include "world/world.hpp"

namespace lim::game {

enum class Surface { Carpet, Concrete, Tile, Metal, Wood };

struct GameEvent {
    enum Type { Toast, ZoneTitle, Footstep, Land, Pickup, Consume, InventoryFull, Died, AutosaveDue, Jump, Exhausted, Drop, ItemLand };
    Type type = Toast;
    std::string text;
    float duration = 2.2f;
    Surface surface = Surface::Concrete;
    std::string sound;
    float intensity = 1.0f;
    bool rare = false;  // V6: seltener Fund (eigener Klang)
    vec3 pos;           // V6: Ort des Ereignisses (Aufprall abgelegter Items)
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

class GameSession {
public:
    GameSession(const Content& content, const PrefabLibrary& prefabs, std::shared_ptr<const world::LevelDef> level,
                i64 seed, const std::string& difficulty, const std::string& name, JobSystem& jobs, int loadRadius);
    ~GameSession();

    // Weiterleitung der Chunk-Ereignisse (Renderer)
    std::function<void(const world::ChunkData&, bool loaded)> chunkListener;

    void placeAtSpawn();
    void preload();
    void update(double dt, const PlayerControl& control, bool paused);

    // Interaktion
    ecs::Entity target() const { return target_; }
    std::string targetPrompt() const;
    void interact();
    void useSlot(int idx);
    void selectSlot(int idx);
    // V6: Item eines Platzes ablegen (vor dem Spieler leicht werfen); all = ganzer Stapel
    bool dropSlot(int idx, bool all);
    // V6: Stapel aus der Hand (Inventar) ablegen
    void dropStack(const std::string& kind, int count);
    size_t droppedCount() const { return dropped_.size(); }

    // Darstellung
    const world::Room* currentRoom() const { return room_; }
    vec3 fogColor() const { return fog_; }
    float fogDensity() const { return (float)fogDensity_; }
    void collectMeshes(std::vector<VisibleMesh>& out);
    void collectSounds(std::vector<SoundSource>& out);
    // V6: eigener Koerper (Strichmaennchen); firstPerson: ohne Kopf
    void collectBody(std::vector<VisibleMesh>& out, bool firstPerson) { body_.collect(out, firstPerson); }
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

    std::string name, difficulty, slot, levelId;
    i64 seed;
    std::unordered_set<std::string> picked;
    double autosaveTimer = 0.0;
    static constexpr double kAutosave = 180.0;

private:
    class TileAdapter;
    void onChunk(const world::ChunkData& cd, bool loaded);
    void findTarget();
    void supplyHint(double dt);
    void updateFog(double dt);
    void updateWanderers(double dt);
    void spawnLevelEntities(const world::ChunkData& cd);
    // V6: abgelegte Items
    struct DroppedItem {
        std::string id, kind;
        vec3 pos;
        float yaw = 0;
        ecs::Entity entity = ecs::kNull;
    };
    ecs::Entity spawnDropped(DroppedItem& d, vec3 vel, bool resting);
    void throwItem(const std::string& kind, int count);
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
    static constexpr size_t kMaxDropped = 400;
};

}  // namespace lim::game
