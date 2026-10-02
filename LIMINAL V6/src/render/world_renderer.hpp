// GPU-Seite der Welt: Chunk-Netze, Materialtabelle, Lichtquellen und die
// Ring-Texturen (Hoehenfeld fuer Schatten, Umgebungslicht) rund um die Kamera.
#pragma once

#include <unordered_map>
#include <vector>

#include "render/gpu.hpp"
#include "render/gpu_types.hpp"
#include "world/chunk.hpp"
#include "world/materials.hpp"

namespace lim::gfx {

// Die ersten GPU-Materialien sind fest vergeben (Items, Requisiten);
// Weltmaterialien folgen dahinter.
constexpr u32 kFixedMaterials = 16;
enum FixedMaterial : u32 {
    FM_ITEM_ENERGY = 0, FM_ITEM_ALMOND = 1, FM_BODY = 2, FM_BODY_FACE = 3,
    FM_VEND_BODY = 4, FM_VEND_GLOW = 5, FM_VEND_GLASS = 6,  // V6: Automat
};
// V6: Sonderbehandlung im Mesh-Shader (Bits 24.. von GpuMaterial::layers)
constexpr u32 kMatFlagInteriorGlass = 1u << 24;

struct LightStats {
    u32 total = 0, visible = 0;
};

class WorldRenderer {
public:
    bool init(Gpu& gpu, int ringSize);
    void clear(Gpu& gpu);

    void addChunk(Gpu& gpu, const world::ChunkData& cd);
    void removeChunk(Gpu& gpu, const world::ChunkData& cd);
    // Neue Materialien der Bank uebernehmen (nur Zuwachs).
    void syncMaterials(Gpu& gpu, const world::MaterialBank& bank);

    // Sichtbare Lichter sammeln und hochladen. Liefert die Anzahl.
    // maxLights (V6): Obergrenze gleichzeitig gerechneter Leuchten (die naechsten zuerst)
    u32 updateLights(Gpu& gpu, const mat4& view, const Frustum& frustum, vec3 camPos, float maxDist, float time,
                     float intensityScale, bool shadows, u32 maxLights = 2048);
    // Chunks zeichnen (G-Buffer-Pass; Shader und Ziele sind bereits gesetzt)
    u32 drawChunks(Gpu& gpu, const Frustum& frustum, vec3 camPos);

    ID3D11ShaderResourceView* materialSrv() const { return materials_.srv.get(); }
    ID3D11ShaderResourceView* lightSrv() const { return lights_.srv.get(); }
    ID3D11ShaderResourceView* heightSrv() const { return height_.srv.get(); }
    ID3D11ShaderResourceView* ambientSrv() const { return ambient_.srv.get(); }
    int ringSize() const { return ringSize_; }
    size_t chunkCount() const { return chunks_.size(); }
    u64 triangles() const { return triangles_; }

private:
    struct GpuChunk {
        Buffer vb, ib;
        u32 indexCount = 0;
        bool index16 = false;
        Aabb bounds;
        std::vector<world::ChunkLight> lights;
    };
    void writeMaterials(Gpu& gpu);
    // Ring-Bereich auf "unbekannt" (massiv, dunkel) setzen; Rechteck in Ringkoordinaten
    void resetRing(Gpu& gpu, int rx, int ry, int w, int h);
    int ringSlot(world::ChunkKey k) const;
    std::vector<world::ChunkKey> ringOwner_;  // welcher Chunk liegt in welchem Ringfeld
    std::vector<bool> ringUsed_;

    std::unordered_map<world::ChunkKey, GpuChunk, world::ChunkKeyHash> chunks_;
    std::vector<GpuMaterial> materialData_;
    size_t syncedWorldMaterials_ = 0;
    Buffer materials_;
    Buffer lights_;
    std::vector<GpuLight> lightData_;
    Texture height_, ambient_;
    int ringSize_ = 256;
    u64 triangles_ = 0;
    std::vector<std::pair<float, const GpuChunk*>> drawList_;
};

// Umrechnung Weltmaterial -> GPU-Material (Texturebenen und PBR-Werte)
GpuMaterial toGpuMaterial(const world::Material& m);
u16 floatToHalf(float f);

}  // namespace lim::gfx
