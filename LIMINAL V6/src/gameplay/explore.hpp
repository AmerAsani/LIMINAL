// V6: Erkundungskarte fuer die Minimap.
//
// Die Karte kennt anfangs nichts. Waehrend der Spieler laeuft, werden die Kacheln um
// ihn herum aufgedeckt - aber nur die, die er wirklich sieht: Strahlen laufen per
// Rasterverfahren (DDA) durch die Kachelwelt und enden an Waenden und hohen Einbauten.
// Ein Raum hinter einer Wand bleibt also unbekannt, auch wenn er nah ist; durch eine
// offene Tuer sieht man ein Stueck hinein.
//
// Aufgedeckt bleibt aufgedeckt: Die Karte merkt sich je Chunk (16 x 16 Kacheln), was
// bekannt ist, und wie es aussieht (Art, Helligkeit, Bodenfarbe) - auch wenn der Chunk
// laengst entladen ist. Im Spielstand stehen nur die Bitmasken; das Aussehen entsteht
// beim naechsten Laden des Chunks neu aus der (deterministischen) Welt.
#pragma once

#include <array>
#include <unordered_map>

#include "core/json.hpp"
#include "world/world.hpp"

namespace lim::game {

enum class MapKind : u8 {
    Unknown = 0,
    Floor,    // begehbarer Boden
    Wall,     // massive Wand (nur die gesehene Innenseite)
    Door,     // Durchgang
    Stair,    // Treppenstufe
    Low,      // niedriger Einbau (Theke, Kiste, Podest, Trennwand)
    High,     // hoher Einbau (Regal, Maschine) - versperrt auch die Sicht
    Machine,  // Automat
    Pending,  // aus dem Spielstand bekannt, Aussehen folgt beim Laden des Chunks
};

struct MapCell {
    MapKind kind = MapKind::Unknown;
    u8 light = 0;      // Helligkeit 0..255 (aus der V4-Kachelhelligkeit)
    u16 color = 0;     // Bodenfarbe RGB565
    bool known() const { return kind != MapKind::Unknown; }
};

class ExploreMap {
public:
    static constexpr double kRadius = 6.5;  // Sichtweite des Aufdeckens (m)

    // Deckt die Umgebung des Spielers auf (Sichtlinien). force: auch ohne Bewegung.
    void reveal(world::World& w, double px, double py, double pz, double dt, bool force = false);
    // Aussehen aus dem Spielstand bekannter Kacheln nachtragen (Chunk wurde geladen)
    void refreshChunk(const world::ChunkData& cd, const world::MaterialBank& bank);

    MapCell at(i64 x, i64 y) const;
    bool known(i64 x, i64 y) const { return at(x, y).known(); }
    // 16 x 16 Zellen eines Chunks (Zeile fuer Zeile) oder nullptr, wenn nichts davon bekannt ist
    const MapCell* chunkCells(world::ChunkKey k) const {
        auto it = chunks_.find(k);
        return it == chunks_.end() ? nullptr : it->second.cells.data();
    }
    // Zaehlt hoch, sobald sich etwas aendert (Minimap muss neu gezeichnet werden)
    u32 revision() const { return rev_; }
    size_t knownTiles() const { return known_; }
    size_t chunkCount() const { return chunks_.size(); }

    Json toJson() const;
    void fromJson(const Json& j);
    void clear();

private:
    struct Chunk {
        std::array<MapCell, world::CS * world::CS> cells{};
    };
    void mark(i64 x, i64 y, const world::Tile& t, const world::MaterialBank& bank);
    static MapCell describe(const world::Tile& t, const world::MaterialBank& bank);

    std::unordered_map<world::ChunkKey, Chunk, world::ChunkKeyHash> chunks_;
    u32 rev_ = 0;
    size_t known_ = 0;
    double lastX_ = 1e18, lastY_ = 1e18, timer_ = 0.0;
};

}  // namespace lim::game
