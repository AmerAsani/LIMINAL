// Ein Chunk (16 x 16 Kacheln): gerasterte Kacheln, Umgebungslicht, Lichtquellen,
// Items und das fertige Dreiecksnetz. Wird komplett auf einem Arbeitsthread
// erzeugt; danach unveraenderlich.
//
// Die Daten sind renderer-neutral (keine GPU-Typen): Der Renderer liest das
// Netz und die Lichter, das Gameplay die Kacheln und Items.
#pragma once

#include <array>
#include <memory>
#include <vector>

#include "core/math.hpp"
#include "world/tile.hpp"

namespace lim::world {

class Sector;
struct ItemSpawn;

constexpr int CS = 16;       // Chunkgroesse (Kacheln)
constexpr int CS_SHIFT = 4;
constexpr int CS_MASK = CS - 1;
constexpr int CB = CS + 2;   // Kacheln inkl. Rand (fuer Nachbarabfragen beim Meshing)

struct ChunkKey {
    i64 x = 0, y = 0;
    bool operator==(const ChunkKey& o) const { return x == o.x && y == o.y; }
};
struct ChunkKeyHash {
    size_t operator()(const ChunkKey& k) const { return std::hash<u64>()(((u64)(u32)k.x << 32) | (u32)k.y); }
};
inline ChunkKey chunkOf(i64 x, i64 y) { return {x >> CS_SHIFT, y >> CS_SHIFT}; }

// Vertex der Weltgeometrie: 16 Bytes. Alle Flaechen sind achsenparallel; die
// Normale ergibt sich aus der Achse, Texturkoordinaten aus der Weltposition.
//   bits 0-2   Normale: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
//   bits 3-16  Material-ID (14 Bit)
//   bits 17-18 Flackermodus des Raums (0 ruhig, 1 Pulsieren, 2 alte Roehre)
//   bits 19-23 Flacker-Seed (5 Bit)
//   bits 24-31 Zusatz (0..255): Leuchtstaerke-Faktor fuer emissive Flaechen
struct WorldVertex {
    float x, y, z;
    u32 data;
};
static_assert(sizeof(WorldVertex) == 16);

enum NormalAxis : u32 { NX_POS = 0, NX_NEG = 1, NY_POS = 2, NY_NEG = 3, NZ_POS = 4, NZ_NEG = 5 };

inline u32 packVertexData(u32 axis, u32 material, u32 flickerMode, u32 flickerSeed, u32 extra = 255) {
    return (axis & 7u) | ((material & 0x3FFFu) << 3) | ((flickerMode & 3u) << 17) | ((flickerSeed & 31u) << 19) |
           ((extra & 255u) << 24);
}

struct ChunkMesh {
    std::vector<WorldVertex> vertices;
    std::vector<u32> indices;
    Aabb bounds;
};

// Lichtquelle fuer den Renderer.
struct ChunkLight {
    enum Kind : u8 { Ceiling, Wall, Machine };
    vec3 pos;
    vec3 color;
    float intensity;    // V4-Staerke (Renderer skaliert physikalisch)
    float radius;       // V4-Reichweite in m (horizontal)
    vec2 area;          // Abmessung der leuchtenden Flaeche (m)
    float drop;         // Deckenlampe: Hoehe ueber dem Boden darunter (m)
    Kind kind;
    i8 dirX, dirY;      // Wand/Automat: Richtung in den Raum
    u8 flickerMode;
    u8 flickerSeed;
    // V6: Klang der Leuchte (0 still, 1 Leuchtstoffroehre, 2 Industrieleuchte, 3 Notleuchte, 4 Wandleuchte)
    u8 hum;
};

struct ChunkData {
    ChunkKey key;
    // Kacheln inkl. 1 Kachel Rand: Index (ly + 1) * CB + (lx + 1)
    std::array<Tile, CB * CB> tiles;
    // V4-Eckhelligkeit (inkl. Ambient Occlusion) und getoenter Umgebungsanteil
    std::array<float, (CS + 1) * (CS + 1)> corners;
    std::array<vec3, CS * CS> ambient;  // Ecke (i, j) des Chunks, RGB
    std::vector<ChunkLight> lights;
    std::vector<const ItemSpawn*> items;
    ChunkMesh mesh;
    // haelt die Sektoren am Leben, auf die Kacheln verweisen
    std::vector<std::shared_ptr<Sector>> sectors;

    const Tile& tile(int lx, int ly) const { return tiles[(size_t)((ly + 1) * CB + (lx + 1))]; }
    i64 x0() const { return key.x * CS; }
    i64 y0() const { return key.y * CS; }
};

// Baut Kacheln, Licht und Netz eines Chunks (reine Funktion, threadsicher).
class MaterialBank;
struct LevelDef;
using TileFetch = Tile (*)(void* ctx, i64 x, i64 y);
void buildChunk(ChunkData& out, TileFetch fetch, void* fetchCtx, const MaterialBank& bank);

}  // namespace lim::world
