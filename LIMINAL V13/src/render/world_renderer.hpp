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

struct ExtraLight;  // V7, renderer.hpp

// Die ersten GPU-Materialien sind fest vergeben (Items, Requisiten);
// Weltmaterialien folgen dahinter.
constexpr u32 kFixedMaterials = 160;  // V9: 16 -> 32 (Sicherungen ...), V10: 64 (Figuren), V13.2: 80, V13.3: 160 (weitere Wesen)
enum FixedMaterial : u32 {
    FM_ITEM_ENERGY = 0, FM_ITEM_ALMOND = 1, FM_BODY = 2, FM_BODY_FACE = 3,
    FM_VEND_BODY = 4, FM_VEND_GLOW = 5, FM_VEND_GLASS = 6,  // V6: Automat
    FM_FLASH_BODY = 7, FM_FLASH_LENS = 8,                   // V7: Taschenlampe
    FM_ITEM_BATTERY = 9, FM_ITEM_NOTE = 10,                 // V7: Fundstuecke
    FM_EXIT_FRAME = 11, FM_EXIT_DOOR = 12, FM_EXIT_SIGN = 13,  // V7: Notausgang
    FM_GHOST = 14,                                          // V7: Gestalt (Halluzination)
    FM_ELEV_DISPLAY = 15,                                   // V8: Etagenanzeige der Aufzuege
    FM_FUSEBOX = 16, FM_FUSE_GLOW = 17, FM_ITEM_FUSE = 18,  // V9: Sicherungskasten, Kontrolllampe, Sicherung
    // V10: der Spieler - Haut, Augen, Haare, Kleidung
    FM_SKIN_PLAYER = 19, FM_EYE, FM_HAIR_SHORT, FM_HOODIE, FM_JEANS, FM_SNEAKER, FM_SOLE,
    // V10: die Entities (eigene Texturebenen, render/texture_entity.hpp)
    FM_ENT_SKIN, FM_ENT_HAIR, FM_ENT_HORN, FM_ENT_TOOTH, FM_ENT_MOUTH, FM_ENT_EYESHINE, FM_ENT_GLOW, FM_ENT_SHADOW,
    FM_ENT_WING, FM_ENT_CHITIN, FM_ENT_FUR, FM_ENT_MOTHEYE, FM_ENT_NEST,
    FM_ENT_GLOW_EYE, FM_ENT_GLOW_DIM,  // V11: Smiler - kalte Augen, matter leuchtende Zaehne
    FM_ENT_CLUMP_A, FM_ENT_CLUMP_B, FM_ENT_CLUMP_C, FM_ENT_CLUMP_CORE,  // V11: Buendel - Hauttoene, Kern
    FM_FLASH_BEZEL, FM_FLASH_RUBBER, FM_FLASH_GLASS, FM_NAIL,  // V12: Viewmodel (Lampe aus Blender, Fingernaegel)
    FM_HAND_SKIN, FM_HAND_NAIL,  // V13: Hand mit Skelett (data/models/vm_hand_l.glb) - Haut mit Durchscheinen, Nagel
    FM_ENT_DARKSKIN,             // V13: dunkle, feuchte Haut (Kopf des Smilers)
    FM_ENT_SILHOUETTE,           // V13: Gestalt - Rauchrand ohne das Ausduennen um das Smiler-Gesicht
    FM_ENT_RESIN,                // V13: Nest - nasses, bernsteinfarbenes Harz (Tropfen, Faeden)
    FM_ENT_NESTFLESH,            // V13.2: Nest - nasse, dunkel rotbraune Wucherung (statt des rosa Kerns des Buendels)
    FM_ENT_NESTEGG,              // V13.2: Nest - Eier, matt cremegelb (nicht das glaenzende Weiss der Zaehne)
    // V13.2: neue Wesen - Kleidung (Faceling, Reste beim Wretch), blasse Haut (Skin-Stealer), Haut des Wretch, Steinwerkzeug,
    // Rattenfell, gluehende Hornspitzen, Landschaft im Fenster, Ausfluss
    FM_ENT_CLOTH_A, FM_ENT_CLOTH_B, FM_ENT_CLOTH_PINK, FM_ENT_PALESKIN, FM_ENT_WRETCHSKIN, FM_ENT_STONE, FM_ENT_RATFUR,
    FM_ENT_HORNGLOW, FM_ENT_VIEW, FM_ENT_OOZE,
    FM_ENT_HUMANSKIN, FM_ENT_RATSKIN,  // V13.2: Menschenhaut (Faceling, getarnter Skin-Stealer), nackte Rattenhaut
    FM_ENT_UMBRA,                      // V13.2: Fensterwesen - schwarz und matt, aber fest (ohne den Rauchrand der Gestalt)
    // V13.2: Facelings sind verschieden (Haar, Haut, Kleidung) - weitere Stoffe, dunklere Haut, graues Haar
    FM_ENT_CLOTH_C, FM_ENT_CLOTH_D, FM_ENT_HUMANSKIN_B, FM_ENT_HAIR_GREY,
    FM_ENT_DULLERSKIN, FM_ENT_ROTSKIN, FM_ENT_SMOKE, FM_ENT_CONCRETE,  // V13.3: Duller (dunkelgrau, trocken), Arms (verwesend), Rauch, Bruchbeton
    // V13.3: Wrangler - durchscheinende Haut (Maennchen grau, Weibchen blass rosa), raue Kopfhaut, Darm, Faserstraenge, Knochen,
    // Schleim, Samen, Staub, Risse, Zangen, Schutt
    FM_ENT_WR_MEMBRANE, FM_ENT_WR_MEMBRANE_F, FM_ENT_WR_HIDE, FM_ENT_WR_GUT, FM_ENT_WR_PITH, FM_ENT_WR_BONE, FM_ENT_WR_SLIME,
    FM_ENT_WR_SEED, FM_ENT_WR_DUST, FM_ENT_WR_CRACK, FM_ENT_WR_PINCER, FM_ENT_WR_DEBRIS,
    // V13.3: Growler - Pfeifenreiniger in vier Farben (a, b leuchten bei den Jungen), blanker Draht; Nguithr'xurh - behaarter
    // Koerper, Seide, Betaeubungsstoff
    FM_ENT_PIPE_A, FM_ENT_PIPE_B, FM_ENT_PIPE_C, FM_ENT_PIPE_D, FM_ENT_WIRE, FM_ENT_SOLIFUGE, FM_ENT_SILK, FM_ENT_SEDATIVE,
    // V13.3: Ouroboros-Arbeiter - Overall (dunkel, staubig, Rauchrand), Leere (Gesicht, Haende), blasse Augenscheiben, Gummi
    // (Stiefel, Schlaeuche), Werkzeugstahl, rostiger Stahl, Maschinenlack, Holz, Ziegel, Absperrband rot/weiss, Funken,
    // Warnleuchte, Anhaenger am Guertel, Zementstaub
    FM_ENT_OB_OVERALL, FM_ENT_OB_VOID, FM_ENT_OB_EYE, FM_ENT_OB_RUBBER, FM_ENT_OB_STEEL, FM_ENT_OB_RUST, FM_ENT_OB_PAINT,
    FM_ENT_OB_WOOD, FM_ENT_OB_BRICK, FM_ENT_OB_TAPE_R, FM_ENT_OB_TAPE_W, FM_ENT_OB_SPARK, FM_ENT_OB_BEACON, FM_ENT_OB_CARD,
    FM_ENT_OB_DUST,
    FM_FIG_END,
};
static_assert(FM_FIG_END <= kFixedMaterials);
// V10: Material wird triplanar aus der Ruheposition texturiert (Figuren), Bit 25 von GpuMaterial::layers
constexpr u32 kMatFlagTriplanar = 1u << 25;
// V10: Wesen - Bits 0-7 Ebene (EntLayer), 8-15 Mikroebene, Bit 26 gesetzt
constexpr u32 kMatFlagEntity = 1u << 26;
// V6: Sonderbehandlung im Mesh-Shader (Bits 24.. von GpuMaterial::layers)
constexpr u32 kMatFlagInteriorGlass = 1u << 24;
// V11: Ausstattung - Atlaskoordinaten statt Weltprojektion; Grund ausschneiden (Schablonenschrift)
constexpr u32 kMatFlagAtlas = 1u << 27;
constexpr u32 kMatFlagCutout = 1u << 28;
// V11: Rauchrand (Smiler, Gestalt) - Umriss, Saum und Scheitel gerastert ausduennen (mesh_gbuffer.hlsl)
constexpr u32 kMatFlagSmoke = 1u << 29;
// V13: Haut - Licht dringt ein (lighting.hlsl: gewickeltes, roetliches Streulicht, Durchleuchten duenner Stellen).
// Der Mesh-Shader schreibt 1 - Anteil in den Glanzkanal (misc.a); 1 = kein Durchscheinen
constexpr u32 kMatFlagSkin = 1u << 30;
// V13.3: Rauchrand der Ouroboros-Arbeiter (nur mit kMatFlagSmoke | kMatFlagSilhouette an triplanaren Figurenmaterialien, Bits
// 16-23): Rand und Glieder zerfasern, waagrechte Baender flackern kurz aus - ohne Saum, Scheitel und Ausduennen nach aussen
constexpr u32 kMatOuroSmoke = 0x0Bu << 16;
// V13: Umriss (Gestalt) - mit kMatFlagSmoke: Rand, Saum und Fasern ausduennen, den Koerper aber dicht lassen
constexpr u32 kMatFlagSilhouette = 1u << 31;
// V13.3: durchscheinende Haut (Wrangler) - nur bei Wesen (kMatFlagEntity): Bits 16-23 = Deckkraft (1..255, 0 = deckend). Der
// Mesh-Shader duennt gerastert aus (schraeg gesehen dichter), das TAA mischt es zu einer milchigen Haut
constexpr u32 kMatMembraneShift = 16;

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
                     float intensityScale, bool shadows, u32 maxLights = 2048,
                     const std::vector<ExtraLight>* extra = nullptr, float worldScale = 1.0f);
    // V7: Stromausfall - Leuchten mit diesem Flacker-Seed abdunkeln (-1 keiner)
    void setBlackout(int seed, float amount, vec3 origin = {}, float front = -1.0f) {
        blackoutSeed_ = seed, blackoutAmount_ = amount, blackoutOrigin_ = origin, blackoutFront_ = front;
    }
    // Chunks zeichnen (G-Buffer-Pass; Shader und Ziele sind bereits gesetzt)
    u32 drawChunks(Gpu& gpu, const Frustum& frustum, vec3 camPos);
    // V11: Ausstattung der Chunks (Shader und Ziele sind gesetzt); liefert Dreiecke
    u64 drawProps(Gpu& gpu, const Frustum& frustum, vec3 camPos, float maxDist);
    // V13.3: die naechsten Pfuetzen (hoechstens kMaxPuddles) hochladen; liefert die Anzahl
    static constexpr u32 kMaxPuddles = 32;
    u32 updatePuddles(Gpu& gpu, vec3 camPos, float maxDist);
    ID3D11ShaderResourceView* puddleSrv() const { return puddles_.srv.get(); }

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
        Buffer pvb, pib;          // V11: Ausstattung
        u32 propIndexCount = 0;
        Aabb propBounds;
        std::vector<world::ChunkLight> lights;
        std::vector<world::PuddleDecal> puddles;  // V13.3
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
    Buffer puddles_;  // V13.3: je Pfuetze 2 x float4
    std::vector<GpuLight> lightData_;
    Texture height_, ambient_;
    int ringSize_ = 256;
    u64 triangles_ = 0;
    int blackoutSeed_ = -1;
    float blackoutAmount_ = 0.0f;
    vec3 blackoutOrigin_{};
    float blackoutFront_ = -1.0f;  // V11: Welle
    std::vector<std::pair<float, const GpuChunk*>> drawList_;
};

// Umrechnung Weltmaterial -> GPU-Material (Texturebenen und PBR-Werte)
GpuMaterial toGpuMaterial(const world::Material& m);
u16 floatToHalf(float f);

}  // namespace lim::gfx
