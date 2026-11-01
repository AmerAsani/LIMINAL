#include "render/world_renderer.hpp"
#include "world/imported_textures.hpp"
#include "world/room.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/log.hpp"
#include "render/flicker.hpp"
#include "render/renderer.hpp"
#include "render/texture_entity.hpp"
#include "render/texture_gen.hpp"

namespace lim::gfx {

u16 floatToHalf(float f) {
    u32 x;
    std::memcpy(&x, &f, 4);
    u32 sign = (x >> 16) & 0x8000u;
    int exp = (int)((x >> 23) & 0xFF) - 127 + 15;
    u32 mant = x & 0x7FFFFFu;
    if (exp <= 0) return (u16)sign;
    if (exp >= 31) return (u16)(sign | 0x7BFFu);  // groesster endlicher Wert
    return (u16)(sign | ((u32)exp << 10) | ((mant + 0x1000u) >> 13));
}

static float srgbToLinear(double c) {
    float v = (float)c;
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

GpuMaterial toGpuMaterial(const world::Material& m) {
    using world::PlanePattern;
    using world::Surface;
    using world::WallPattern;
    GpuMaterial g{};
    for (int c = 0; c < 3; ++c) g.albedo[c] = srgbToLinear(m.rgb[(size_t)c]);
    u32 up = L_CONCRETE, down = L_PLASTER, wall = L_PLASTER;
    switch (m.plane) {
        case PlanePattern::None: up = L_CONCRETE, down = L_PLASTER; break;
        case PlanePattern::Carpet: up = down = L_CARPET; break;
        case PlanePattern::Slabs: up = L_SLABS, down = L_CONCRETE; break;
        case PlanePattern::BigTiles: up = down = L_BIGTILES; break;
        case PlanePattern::Grate: up = down = L_GRATE; break;
        case PlanePattern::Tiles: up = L_FLOORTILE, down = L_CEILTILE; break;
        case PlanePattern::Checker: up = down = L_CHECKER; break;
        case PlanePattern::Lines: up = down = L_SLABS; break;
        case PlanePattern::Planks: up = L_PLANKS, down = L_WOOD; break;  // V11: Dielen, Bretterdecke
        default: break;
    }
    switch (m.wall) {
        case WallPattern::Plain: wall = L_PLASTER; break;
        case WallPattern::Wallpaper: wall = L_WALLPAPER; break;
        case WallPattern::Blocks: wall = L_BLOCKS; break;
        case WallPattern::Bricks: wall = L_BRICKS; break;
        case WallPattern::Panels: wall = L_PANELS; break;
        case WallPattern::Concrete: wall = L_CONCRETE; break;
        case WallPattern::Boards: wall = L_WOOD; break;      // V11
        case WallPattern::Tiles: wall = L_FLOORTILE; break;  // V11
        default: break;
    }
    g.roughness = 1.0f;
    g.metalness = 0.0f;
    auto all = [&](u32 l) { up = down = wall = l; };
    switch (m.surface) {
        case Surface::Partition: all(L_FABRIC); break;
        case Surface::Shelf: all(L_METAL); break;
        case Surface::Crate:
            all(L_CRATE);
            // V6: Fichte statt Orange (nur Darstellung; die Weltdaten bleiben unveraendert)
            for (int c = 0; c < 3; ++c) g.albedo[c] = g.albedo[c] * 0.55f + 0.45f * (g.albedo[0] * 0.2126f + g.albedo[1] * 0.7152f + g.albedo[2] * 0.0722f) * 1.05f;
            break;
        case Surface::Machine: all(L_METAL); g.roughness = 1.2f; break;
        case Surface::Stage: up = L_BIGTILES; break;
        case Surface::Monolith: all(L_MONOLITH); break;
        case Surface::Counter: all(L_WOOD); break;
        case Surface::CounterTop: all(L_LAMINATE); break;
        case Surface::Vending: all(L_VENDING); break;
        case Surface::Trim: all(L_TRIM); break;
        case Surface::Metal: all(L_METAL); break;
        // V11: Ausstattung
        case Surface::PropPaint: all(L_PAINTED); break;
        case Surface::PropCarPaint: all(L_PAINTED); g.roughness = 0.5f; break;
        case Surface::PropMetal: all(L_METAL); g.metalness = 0.8f; break;
        case Surface::PropGalvanized: all(L_METAL); g.metalness = 0.65f; g.roughness = 1.35f; break;
        case Surface::PropPlastic: all(L_PLASTER); g.roughness = 0.5f; break;
        case Surface::PropRubber: all(L_RUBBER); break;
        case Surface::PropGlass: all(L_PLASTER); g.roughness = 0.06f; break;
        case Surface::PropWater: down = wall = L_PLASTER; g.roughness = 0.02f; break;  // V13.3: oben die Bodenebene (klares Wasser)
        case Surface::PropRoadPaint: all(L_ROADPAINT); break;
        case Surface::PropSign: all(L_SIGNS); break;
        case Surface::PropStencil: all(L_SIGNS); break;
        case Surface::PropWood: all(L_WOOD); break;
        case Surface::PropFabric: all(L_FABRIC); break;
        case Surface::PropConcrete: all(L_CONCRETE); break;
        case Surface::PropCardboard: all(L_LAMINATE); g.roughness = 1.4f; break;
        case Surface::PropGlow: all(L_DIFFUSER); break;
        case Surface::PropCeramic: all(L_PLASTER); g.roughness = 0.3f; break;
        default: break;
    }
    if (m.roughMul >= 0.0f) g.roughness = m.roughMul;  // V13.3: aus Blender
    if (m.metalAdd >= 0.0f) g.metalness = m.metalAdd;
    if (m.emissive) {
        if (m.surface == Surface::Default) all(L_DIFFUSER);
        g.emissive = m.surface == Surface::Vending ? 3.2f : 11.0f;
    }
    // V12: Texturen aus Bilddateien ueberschreiben das Muster je Richtung
    if (m.layer[0] != 255) up = m.layer[0];
    if (m.layer[1] != 255) down = m.layer[1];
    if (m.layer[2] != 255) wall = m.layer[2];
    g.layers = up | (down << 8) | (wall << 16);
    if (m.surface == Surface::PropSign || m.surface == Surface::PropStencil) g.layers |= kMatFlagAtlas;
    if (m.surface == Surface::PropStencil) g.layers |= kMatFlagCutout;
    g.variation = kLayers[wall].variation;
    if (wall >= L_IMP0 && wall < (u32)L_COUNT && (size_t)(wall - L_IMP0) < world::importedTextures().size())
        g.variation = world::importedTextures()[wall - L_IMP0].variation;
    return g;
}

bool WorldRenderer::init(Gpu& gpu, int ringSize) {
    ringSize_ = ringSize;
    height_ = gpu.createTexture(ringSize, ringSize, DXGI_FORMAT_R16G16_FLOAT, TEX_SRV);
    ambient_ = gpu.createTexture(ringSize, ringSize, DXGI_FORMAT_R16G16B16A16_FLOAT, TEX_SRV);
    if (!height_.valid() || !ambient_.valid()) return false;
    int slots = ringSize / world::CS;
    ringOwner_.assign((size_t)slots * slots, world::ChunkKey{});
    ringUsed_.assign((size_t)slots * slots, false);
    resetRing(gpu, 0, 0, ringSize, ringSize);

    // Feste Materialien: Items (Farbe steckt vollstaendig in der Textur)
    materialData_.assign(kFixedMaterials, GpuMaterial{{1, 1, 1}, 0, 0, 1, 0, 0});
    materialData_[FM_ITEM_ENERGY].layers = L_ITEM_ENERGY | (L_ITEM_ENERGY << 8) | (L_ITEM_ENERGY << 16);
    materialData_[FM_ITEM_ALMOND].layers = L_ITEM_ALMOND | (L_ITEM_ALMOND << 8) | (L_ITEM_ALMOND << 16);
    // V6: Strichmaennchen - schwarz und matt wie mit dem Filzstift gezogen (nur ein Hauch Glanz,
    // damit die Striche in hellen Raeumen nicht flach wirken). Augen und Grinsen hell, damit das
    // Gesicht auf dem schwarzen Kopf sichtbar ist.
    materialData_[FM_BODY] = GpuMaterial{{0.008f, 0.008f, 0.009f}, 0, L_PLASTER | (L_PLASTER << 8) | (L_PLASTER << 16), 0.95f, 0, 0.0f};
    materialData_[FM_BODY_FACE] = GpuMaterial{{0.85f, 0.83f, 0.78f}, 0, L_PLASTER | (L_PLASTER << 8) | (L_PLASTER << 16), 1.0f, 0, 0.0f};
    // V6: Automat - Gehaeuse (Atlas), Leuchtschild und Anzeige (gleicher Atlas, leuchtend),
    // Glasfront mit Faechern dahinter (Interior Mapping im Mesh-Shader)
    const u32 body = L_VEND_BODY | (L_VEND_BODY << 8) | (L_VEND_BODY << 16);
    materialData_[FM_VEND_BODY] = GpuMaterial{{1, 1, 1}, 0, body, 1.0f, 0, 0.0f};
    materialData_[FM_VEND_GLOW] = GpuMaterial{{1, 0.97f, 0.92f}, 2.6f, body, 1.0f, 0, 0.0f};
    materialData_[FM_VEND_GLASS] =
        GpuMaterial{{1, 0.96f, 0.9f}, 2.4f, L_VENDING | (L_VENDING << 8) | (L_VENDING << 16) | kMatFlagInteriorGlass, 1.0f, 0, 0.0f};
    // V7: Taschenlampe, Fundstuecke, Notausgang, Gestalt
    auto all = [](u32 l) { return l | (l << 8) | (l << 16); };
    // V12: eloxiertes Aluminium - dunkel mit seidigem Glanz (Lampe aus Blender, data/models/vm_flashlight.glb)
    materialData_[FM_FLASH_BODY] = GpuMaterial{{0.035f, 0.035f, 0.04f}, 0, all(L_METAL), 0.5f, 0.6f, 0.15f};
    materialData_[FM_FLASH_BEZEL] = GpuMaterial{{0.62f, 0.62f, 0.60f}, 0, all(L_METAL), 0.32f, 1.0f, 0.1f};
    materialData_[FM_FLASH_RUBBER] = GpuMaterial{{0.03f, 0.03f, 0.03f}, 0, all(L_PLASTER), 0.95f, 0, 0.1f};
    materialData_[FM_FLASH_GLASS] = GpuMaterial{{0.20f, 0.21f, 0.22f}, 0, all(L_DIFFUSER), 0.08f, 0.8f, 0.0f};
    materialData_[FM_FLASH_LENS] = GpuMaterial{{1.0f, 0.94f, 0.82f}, 7.0f, all(L_DIFFUSER), 1.0f, 0, 0.0f};
    materialData_[FM_ITEM_BATTERY] = GpuMaterial{{1, 1, 1}, 0, all(L_ITEM_BATTERY), 1.0f, 0, 0.0f};
    materialData_[FM_ITEM_NOTE] = GpuMaterial{{1, 1, 1}, 0, all(L_ITEM_NOTE), 1.0f, 0, 0.0f};
    materialData_[FM_EXIT_FRAME] = GpuMaterial{{1, 1, 1}, 0, all(L_EXIT), 1.0f, 0, 0.0f};
    materialData_[FM_EXIT_DOOR] = GpuMaterial{{1, 1, 1}, 0, all(L_EXIT), 1.0f, 0, 0.0f};
    materialData_[FM_EXIT_SIGN] = GpuMaterial{{0.9f, 1.0f, 0.92f}, 3.5f, all(L_EXIT), 1.0f, 0, 0.0f};
    materialData_[FM_GHOST] = GpuMaterial{{0.003f, 0.003f, 0.004f}, 0, all(L_PLASTER), 1.0f, 0, 0.0f};
    materialData_[FM_ELEV_DISPLAY] = GpuMaterial{{1, 1, 1}, 4.5f, all(L_ELEV), 1.0f, 0, 0.0f};  // V8
    // V9: Sicherungskasten (Atlas), Kontrolllampe (orange leuchtend), Sicherung
    materialData_[FM_FUSEBOX] = GpuMaterial{{1, 1, 1}, 0, all(L_FUSE), 1.0f, 0, 0.0f};
    materialData_[FM_FUSE_GLOW] = GpuMaterial{{1.0f, 0.4f, 0.07f}, 3.5f, all(L_FUSE), 1.0f, 0, 0.0f};
    materialData_[FM_ITEM_FUSE] = GpuMaterial{{1, 1, 1}, 0, all(L_FUSE), 1.0f, 0, 0.0f};
    // V13.3: Leuchtstab - Huelle aus mattem, durchscheinendem Kunststoff (ungeknickt blass gelbgruen, glaenzend); geknickt
    // leuchtet die Fluessigkeit gruen (hell, gegen Ende matt); Kappe aus festem, dunklerem Kunststoff
    materialData_[FM_ITEM_GLOWSTICK] = GpuMaterial{{0.58f, 0.72f, 0.42f}, 0, all(L_DIFFUSER), 0.3f, 0, 0.05f};
    materialData_[FM_ITEM_GLOWSTICK_LIT] = GpuMaterial{{0.30f, 1.0f, 0.18f}, 1.6f, all(L_DIFFUSER), 0.3f, 0, 0.0f};
    materialData_[FM_ITEM_GLOWSTICK_DIM] = GpuMaterial{{0.30f, 0.95f, 0.18f}, 0.7f, all(L_DIFFUSER), 0.3f, 0, 0.0f};
    materialData_[FM_ITEM_GLOWSTICK_CAP] = GpuMaterial{{0.30f, 0.42f, 0.24f}, 0, all(L_PLASTER), 0.55f, 0, 0.1f};
    // V10: Figuren - Haut, Stoff und Haar triplanar (Toenung x helle Textur), Augen und Ausweis mit UV
    auto tri = [&](u32 l) { return all(l) | kMatFlagTriplanar; };
    materialData_[FM_SKIN_PLAYER] = GpuMaterial{{0.95f, 0.86f, 0.8f}, 0, tri(L_SKIN), 1.0f, 0, 0.3f};
    materialData_[FM_NAIL] = GpuMaterial{{1.0f, 0.86f, 0.82f}, 0, tri(L_SKIN), 0.45f, 0, 0.1f};  // V12
    // V13: Hand aus Blender - etwas waermer als die Koerperhaut, Rauheit wie trockene Haut (Glanz nur an Knoecheln
    // durch die Textur), Durchscheinen; Nagel glatter und heller, kaum Durchscheinen
    materialData_[FM_HAND_SKIN] = GpuMaterial{{0.98f, 0.84f, 0.76f}, 0, tri(L_SKIN) | kMatFlagSkin, 0.92f, 0, 0.25f};
    materialData_[FM_HAND_NAIL] = GpuMaterial{{1.05f, 0.90f, 0.86f}, 0, tri(L_SKIN), 0.38f, 0, 0.05f};
    materialData_[FM_EYE] = GpuMaterial{{0.8f, 0.78f, 0.76f}, 0, all(L_FIGDETAIL), 1.0f, 0, 0.0f};
    materialData_[FM_HAIR_SHORT] = GpuMaterial{{0.2f, 0.16f, 0.13f}, 0, tri(L_HAIR), 1.0f, 0, 0.2f};
    materialData_[FM_HOODIE] = GpuMaterial{{0.15f, 0.18f, 0.15f}, 0, tri(L_WOOL), 1.0f, 0, 0.35f};
    materialData_[FM_JEANS] = GpuMaterial{{0.17f, 0.23f, 0.36f}, 0, tri(L_DENIM), 1.0f, 0, 0.4f};
    materialData_[FM_SNEAKER] = GpuMaterial{{0.5f, 0.5f, 0.5f}, 0, tri(L_LEATHER), 1.1f, 0, 0.4f};
    materialData_[FM_SOLE] = GpuMaterial{{0.82f, 0.81f, 0.78f}, 0, tri(L_LEATHER), 1.6f, 0, 0.3f};
    // V10: die Entities - Farbe, Rauheit und Hoehe stecken in den Texturen; variation = Staub/Schmutz.
    // Leuchten (emissive > 0): Farbe = Textur x albedo, Staerke je Bild aus ObjectCB.highlight
    auto ent = [](u32 layer, u32 micro) { return layer | (micro << 8) | kMatFlagEntity; };
    materialData_[FM_ENT_SKIN] = GpuMaterial{{1, 1, 1}, 0, ent(EL_SKIN, ML_PORES) | kMatFlagSkin, 1.0f, 0, 0.3f};
    // V13: Kopf des Smilers - fast schwarz, feucht glaenzend an den Wulsten, kaum Durchscheinen
    materialData_[FM_ENT_DARKSKIN] = GpuMaterial{{0.045f, 0.040f, 0.042f}, 0, ent(EL_SKIN, ML_PORES), 0.55f, 0, 0.1f};
    materialData_[FM_ENT_HAIR] = GpuMaterial{{1, 1, 1}, 0, ent(EL_HAIR, ML_FIBER), 0.6f, 0, 0.12f};  // V11: nass, glaenzt
    materialData_[FM_ENT_HORN] = GpuMaterial{{1, 1, 1}, 0, ent(EL_HORN, ML_CRAZE), 1.0f, 0, 0.15f};
    materialData_[FM_ENT_TOOTH] = GpuMaterial{{0.92f, 0.86f, 0.72f}, 0, ent(EL_TOOTH, ML_CRAZE), 1.0f, 0, 0.0f};  // V13: vergilbt
    materialData_[FM_ENT_MOUTH] = GpuMaterial{{0.62f, 0.34f, 0.32f}, 0, ent(EL_SKIN, ML_PORES) | kMatFlagSkin, 0.55f, 0, 0.0f};
    // Hound: weisse Augen ohne Farbstoff, im Dunkeln von weitem zu sehen (Fandom) - ein schwacher Schein
    // V13: nur ein schwacher Schimmer (Wikidot beschreibt die Augen nicht; das Haar verdeckt sie meist)
    materialData_[FM_ENT_EYESHINE] = GpuMaterial{{0.55f, 0.57f, 0.52f}, 0.35f, ent(EL_TOOTH, ML_PORES), 0.15f, 0, 0.0f};
    // Smiler: Augen und Zaehne aus leuchtender Substanz (Fandom), weiss (Wikidot)
    // V11: die Zaehne leuchten schwach und ungleich (zwei Staerken), die Augen kalt weiss
    materialData_[FM_ENT_GLOW] = GpuMaterial{{1.0f, 0.99f, 0.94f}, 5.0f, ent(EL_TOOTH, ML_CRAZE), 0.6f, 0, 0.0f};
    materialData_[FM_ENT_GLOW_DIM] = GpuMaterial{{0.92f, 0.94f, 0.88f}, 2.2f, ent(EL_TOOTH, ML_CRAZE), 0.6f, 0, 0.0f};
    materialData_[FM_ENT_GLOW_EYE] = GpuMaterial{{0.8f, 0.92f, 1.0f}, 7.5f, ent(EL_TOOTH, ML_PORES), 0.4f, 0, 0.0f};
    // V11: Buendel - Haut klebrig-feucht (wenig Rauheit), Arme in verschiedenen Hauttoenen (verschiedene
    // Menschen), der Kern in Blutergussfarben, wo alles verwachsen ist. Toenung x Hauttextur (grau-gelb)
    materialData_[FM_ENT_CLUMP_A] = GpuMaterial{{1.9f, 1.55f, 1.45f}, 0, ent(EL_SKIN, ML_PORES) | kMatFlagSkin, 0.42f, 0, 0.15f};
    materialData_[FM_ENT_CLUMP_B] = GpuMaterial{{1.45f, 1.15f, 0.98f}, 0, ent(EL_SKIN, ML_PORES) | kMatFlagSkin, 0.45f, 0, 0.15f};
    materialData_[FM_ENT_CLUMP_C] = GpuMaterial{{0.9f, 0.66f, 0.56f}, 0, ent(EL_SKIN, ML_PORES) | kMatFlagSkin, 0.45f, 0, 0.15f};
    materialData_[FM_ENT_CLUMP_CORE] = GpuMaterial{{1.3f, 0.82f, 0.98f}, 0, ent(EL_SKIN, ML_PORES) | kMatFlagSkin, 0.4f, 0, 0.2f};
    // Smiler-Koerper und die Gestalt: schwarz und matt wie ein Schatten, V11: mit rauchigem Rand. Metall = 1
    // macht auch den Glanz so dunkel wie die Farbe (f0 = Albedo statt 4 %) - die Lampe macht es nicht grau
    materialData_[FM_ENT_SHADOW] = GpuMaterial{{0.006f, 0.006f, 0.007f}, 0, tri(L_SKIN) | kMatFlagSmoke, 1.4f, 1.0f, 0.0f};
    materialData_[FM_ENT_RESIN] = GpuMaterial{{0.78f, 0.46f, 0.14f}, 0, ent(EL_HORN, ML_PORES), 0.28f, 0, 0.05f};
    materialData_[FM_ENT_SILHOUETTE] =
        GpuMaterial{{0.0025f, 0.0025f, 0.003f}, 0, tri(L_SKIN) | kMatFlagSmoke | kMatFlagSilhouette, 1.4f, 1.0f, 0.0f};  // schluckt Licht
    materialData_[FM_ENT_WING] = GpuMaterial{{1, 1, 1}, 0, ent(EL_WING, ML_SCALE), 1.0f, 0, 0.1f};
    materialData_[FM_ENT_CHITIN] = GpuMaterial{{1, 1, 1}, 0, ent(EL_CHITIN, ML_CRAZE), 1.0f, 0, 0.1f};
    materialData_[FM_ENT_FUR] = GpuMaterial{{1, 1, 1}, 0, ent(EL_FUR, ML_FIBER), 1.0f, 0, 0.1f};
    materialData_[FM_ENT_MOTHEYE] = GpuMaterial{{0.12f, 0.12f, 0.12f}, 0, ent(EL_CHITIN, ML_PORES), 0.45f, 0, 0.0f};
    materialData_[FM_ENT_NEST] = GpuMaterial{{1, 1, 1}, 0, ent(EL_NEST, ML_PORES), 1.0f, 0, 0.15f};
    // Runde 4: Textur des Nests (Fasern, Adern, Harz) statt glatter Haut, rotbraun getoent, matter
    materialData_[FM_ENT_NESTFLESH] = GpuMaterial{{1.05f, 0.62f, 0.55f}, 0, ent(EL_NEST, ML_PORES) | kMatFlagSkin, 0.74f, 0, 0.15f};
    materialData_[FM_ENT_NESTEGG] = GpuMaterial{{0.74f, 0.66f, 0.46f}, 0, ent(EL_TOOTH, ML_PORES), 0.55f, 0, 0.1f};
    // V13.2: neue Wesen. Kleidung mit den Stoffen der Spielerfigur (triplanar aus der Ruhepose), eigene Toenung
    materialData_[FM_ENT_CLOTH_A] = GpuMaterial{{0.30f, 0.29f, 0.27f}, 0, tri(L_WOOL), 1.0f, 0, 0.45f};        // Hemd, grau
    materialData_[FM_ENT_CLOTH_B] = GpuMaterial{{0.16f, 0.17f, 0.20f}, 0, tri(L_DENIM), 1.0f, 0, 0.45f};       // Hose, dunkel
    materialData_[FM_ENT_CLOTH_C] = GpuMaterial{{0.20f, 0.26f, 0.36f}, 0, tri(L_WOOL), 1.0f, 0, 0.4f};        // Bluse, gedecktes Blau
    materialData_[FM_ENT_CLOTH_D] = GpuMaterial{{0.17f, 0.20f, 0.14f}, 0, tri(L_DENIM), 1.0f, 0, 0.45f};      // Jacke, dunkles Oliv (Braun wirkte im Lampenlicht wie Haut)
    materialData_[FM_ENT_HUMANSKIN_B] = GpuMaterial{{0.62f, 0.52f, 0.46f}, 0, ent(EL_HUMAN, ML_PORES) | kMatFlagSkin, 0.6f, 0, 0.15f};
    materialData_[FM_ENT_HAIR_GREY] = GpuMaterial{{2.6f, 2.5f, 2.4f}, 0, ent(EL_HAIR, ML_FIBER), 0.8f, 0, 0.12f};
    materialData_[FM_ENT_CLOTH_PINK] = GpuMaterial{{0.78f, 0.42f, 0.50f}, 0, tri(L_WOOL), 0.95f, 0, 0.35f};    // Kleid (Level 188)
    // Skin-Stealer: blassgelb, Saugnaepfchen als Mikrostruktur (Schuppen), feucht
    materialData_[FM_ENT_PALESKIN] = GpuMaterial{{1, 1, 1}, 0, ent(EL_PALE, ML_SCALE) | kMatFlagSkin, 0.55f, 0, 0.1f};
    materialData_[FM_ENT_HUMANSKIN] = GpuMaterial{{1, 1, 1}, 0, ent(EL_HUMAN, ML_PORES) | kMatFlagSkin, 0.6f, 0, 0.15f};
    materialData_[FM_ENT_UMBRA] = GpuMaterial{{0.010f, 0.010f, 0.012f}, 0, tri(L_SKIN), 1.3f, 1.0f, 0.0f};
    materialData_[FM_ENT_RATSKIN] = GpuMaterial{{1.05f, 0.88f, 0.92f}, 0, ent(EL_HUMAN, ML_CRAZE) | kMatFlagSkin, 0.7f, 0, 0.2f};
    // Wretch: rotbraun, trocken und schuppig (Risse)
    materialData_[FM_ENT_WRETCHSKIN] = GpuMaterial{{1.05f, 0.55f, 0.38f}, 0, ent(EL_SKIN, ML_CRAZE) | kMatFlagSkin, 0.95f, 0, 0.35f};
    materialData_[FM_ENT_STONE] = GpuMaterial{{0.62f, 0.60f, 0.56f}, 0, ent(EL_HORN, ML_CRAZE), 1.0f, 0, 0.3f};
    materialData_[FM_ENT_RATFUR] = GpuMaterial{{0.16f, 0.15f, 0.15f}, 0, ent(EL_FUR, ML_FIBER), 0.9f, 0, 0.2f};
    // Hoernerspitzen der Death Rat: rot gluehend (Entity 24)
    materialData_[FM_ENT_HORNGLOW] = GpuMaterial{{1.0f, 0.18f, 0.08f}, 3.0f, ent(EL_HORN, ML_CRAZE), 0.5f, 0, 0.0f};
    // Fensterwesen: verschwommene Landschaft, schwach leuchtend (wie Licht von draussen)
    materialData_[FM_ENT_VIEW] = GpuMaterial{{1.0f, 1.0f, 1.0f}, 1.4f, ent(EL_VIEW, ML_PORES), 0.3f, 0, 0.0f};
    // Ausfluss (Wretch: rotbrauner Saft aus den Pusteln, brauner Schlamm aus dem Mund)
    materialData_[FM_ENT_OOZE] = GpuMaterial{{0.42f, 0.20f, 0.11f}, 0, ent(EL_HORN, ML_PORES), 0.22f, 0, 0.05f};
    // V13.3: Wrangler (KREATUR_Wrangler.md). Haut feucht und klebrig, durchscheinend (gerastert, kMatMembraneShift); das Maennchen
    // rauer und grauer, dichter (die Seite: mit dem Alter grauer und undurchsichtiger), das Weibchen blass rosa und klarer.
    // Innen: dunkelroter Darm, pflanzenartige Faserstraenge (olivbraun), Knochen; Schleim glaenzt, Staub und Schutt matt
    auto memb = [](u32 op) { return op << kMatMembraneShift; };
    materialData_[FM_ENT_WR_MEMBRANE] = GpuMaterial{{0.74f, 0.92f, 1.12f}, 0, ent(EL_HUMAN, ML_CRAZE) | kMatFlagSkin | memb(212), 0.38f, 0, 0.05f};
    materialData_[FM_ENT_WR_MEMBRANE_F] = GpuMaterial{{1.3f, 1.06f, 1.24f}, 0, ent(EL_HUMAN, ML_PORES) | kMatFlagSkin | memb(178), 0.22f, 0, 0.02f};
    materialData_[FM_ENT_WR_HIDE] = GpuMaterial{{1.62f, 1.64f, 1.7f}, 0, ent(EL_SKIN, ML_CRAZE) | kMatFlagSkin, 0.84f, 0, 0.2f};
    materialData_[FM_ENT_WR_GUT] = GpuMaterial{{1.5f, 0.62f, 0.72f}, 0, ent(EL_NEST, ML_PORES) | kMatFlagSkin, 0.3f, 0, 0.0f};
    materialData_[FM_ENT_WR_PITH] = GpuMaterial{{1.8f, 1.95f, 1.45f}, 0, ent(EL_NEST, ML_FIBER), 0.62f, 0, 0.0f};
    materialData_[FM_ENT_WR_BONE] = GpuMaterial{{0.95f, 0.92f, 0.85f}, 0, ent(EL_TOOTH, ML_CRAZE), 0.68f, 0, 0.05f};
    materialData_[FM_ENT_WR_SLIME] = GpuMaterial{{2.2f, 2.4f, 2.0f}, 0, ent(EL_HORN, ML_PORES) | memb(150), 0.05f, 0, 0.0f};
    materialData_[FM_ENT_WR_SEED] = GpuMaterial{{0.98f, 0.86f, 0.62f}, 0, ent(EL_TOOTH, ML_PORES), 0.38f, 0, 0.1f};
    materialData_[FM_ENT_WR_DUST] = GpuMaterial{{0.56f, 0.55f, 0.53f}, 0, ent(EL_TOOTH, ML_CRAZE), 1.0f, 0, 0.6f};
    materialData_[FM_ENT_WR_CRACK] = GpuMaterial{{0.04f, 0.038f, 0.035f}, 0, ent(EL_HORN, ML_CRAZE), 1.0f, 0, 0.0f};
    materialData_[FM_ENT_WR_PINCER] = GpuMaterial{{0.8f, 0.72f, 0.66f}, 0, ent(EL_CHITIN, ML_CRAZE), 0.24f, 0, 0.0f};
    materialData_[FM_ENT_WR_DEBRIS] = GpuMaterial{{0.47f, 0.46f, 0.44f}, 0, ent(EL_TOOTH, ML_CRAZE), 1.0f, 0, 0.5f};
    // V13.3: Duller - dunkelgrau, trocken und matt, Haarrisse; an Knochenkanten etwas Glanz (Textur); kein roetliches Durchscheinen
    materialData_[FM_ENT_DULLERSKIN] = GpuMaterial{{0.34f, 0.34f, 0.36f}, 0, ent(EL_SKIN, ML_CRAZE), 1.5f, 0, 0.25f};
    // V13.3: Arms - verwesende, graue Haut mit gruenlichem Stich, speckig-feucht (wenig Rauheit)
    materialData_[FM_ENT_ROTSKIN] = GpuMaterial{{0.44f, 0.47f, 0.40f}, 0, ent(EL_SKIN, ML_CRAZE), 0.8f, 0, 0.5f};
    // V13.3: frisch gebrochener Beton und Putz (Bruchstelle des Dullers, Riss der Arms): hell, stumpf, fleckig, Haarrisse
    materialData_[FM_ENT_CONCRETE] = GpuMaterial{{0.56f, 0.56f, 0.54f}, 0, ent(EL_TOOTH, ML_CRAZE), 1.9f, 0, 0.45f};
    materialData_[FM_ENT_SMOKE] = GpuMaterial{{0.32f, 0.31f, 0.30f}, 0, tri(L_SKIN) | kMatFlagSmoke, 1.0f, 0, 0.0f};  // Rauch verbrannter Haut
    writeMaterials(gpu);
    lights_ = gpu.createStructured(sizeof(GpuLight), 2048, true, false);
    puddles_ = gpu.createStructured(sizeof(vec4), kMaxPuddles * 2, true, false);  // V13.3
    return height_.valid() && ambient_.valid() && lights_.valid();
}

void WorldRenderer::clear(Gpu& gpu) {
    chunks_.clear();
    materialData_.resize(kFixedMaterials);
    syncedWorldMaterials_ = 0;
    // Neue Welt: keine Reste der vorigen im Hoehenfeld (sonst falsche Schatten am Sichtrand)
    std::fill(ringUsed_.begin(), ringUsed_.end(), false);
    resetRing(gpu, 0, 0, ringSize_, ringSize_);
}

// Unbekannte Bereiche: massiv, dunkel (Schattenstrahlen enden dort)
void WorldRenderer::resetRing(Gpu& gpu, int rx, int ry, int w, int h) {
    std::vector<u16> hf((size_t)w * h * 2);
    for (size_t i = 0; i < hf.size(); i += 2) {
        hf[i] = floatToHalf(60000.0f);
        hf[i + 1] = floatToHalf(-60000.0f);
    }
    std::vector<u16> am((size_t)w * h * 4, 0);
    D3D11_BOX box{(UINT)rx, (UINT)ry, 0, (UINT)(rx + w), (UINT)(ry + h), 1};
    gpu.ctx()->UpdateSubresource(height_.tex.get(), 0, &box, hf.data(), (UINT)w * 4, 0);
    gpu.ctx()->UpdateSubresource(ambient_.tex.get(), 0, &box, am.data(), (UINT)w * 8, 0);
}

int WorldRenderer::ringSlot(world::ChunkKey k) const {
    int slots = ringSize_ / world::CS;
    int sx = (int)(((k.x % slots) + slots) % slots), sy = (int)(((k.y % slots) + slots) % slots);
    return sy * slots + sx;
}

void WorldRenderer::writeMaterials(Gpu& gpu) {
    u32 cap = materials_.valid() ? materials_.count : 0;
    if (materialData_.size() > cap) {
        u32 n = std::max<u32>(1024, (u32)materialData_.size() * 2);
        materials_ = gpu.createStructured(sizeof(GpuMaterial), n, false, false);
    }
    D3D11_BOX box{0, 0, 0, (UINT)(materialData_.size() * sizeof(GpuMaterial)), 1, 1};
    gpu.ctx()->UpdateSubresource(materials_.buf.get(), 0, &box, materialData_.data(), 0, 0);
}

void WorldRenderer::syncMaterials(Gpu& gpu, const world::MaterialBank& bank) {
    size_t n = bank.size();
    if (n <= syncedWorldMaterials_) return;
    for (size_t i = syncedWorldMaterials_; i < n; ++i) materialData_.push_back(toGpuMaterial(bank[(u16)i]));
    syncedWorldMaterials_ = n;
    writeMaterials(gpu);
}

void WorldRenderer::addChunk(Gpu& gpu, const world::ChunkData& cd) {
    GpuChunk c;
    const auto& mesh = cd.mesh;
    if (!mesh.vertices.empty()) {
        // Materialindizes auf die GPU-Tabelle verschieben (feste Materialien davor)
        std::vector<world::WorldVertex> verts = mesh.vertices;
        for (auto& v : verts) {
            u32 mat = ((v.data >> 3) & 0x3FFFu) + kFixedMaterials;
            v.data = (v.data & ~(0x3FFFu << 3)) | ((mat & 0x3FFFu) << 3);
        }
        c.vb = gpu.createVertex(verts.data(), (u32)(verts.size() * sizeof(world::WorldVertex)), sizeof(world::WorldVertex));
        c.indexCount = (u32)mesh.indices.size();
        if (verts.size() < 65536) {
            std::vector<u16> idx(mesh.indices.begin(), mesh.indices.end());
            c.ib = gpu.createIndex(idx.data(), (u32)(idx.size() * 2));
            c.index16 = true;
        } else {
            c.ib = gpu.createIndex(mesh.indices.data(), (u32)(mesh.indices.size() * 4));
        }
        c.bounds = mesh.bounds;
    }
    c.lights = cd.lights;
    c.puddles = cd.puddles;  // V13.3
    // V11: Ausstattung
    if (!cd.props.vertices.empty()) {
        std::vector<world::PropVertex> pv = cd.props.vertices;
        for (auto& v : pv) {
            const u32 mat = (v.data & 0x3FFFu) + kFixedMaterials;
            v.data = (v.data & ~0x3FFFu) | (mat & 0x3FFFu);
        }
        c.pvb = gpu.createVertex(pv.data(), (u32)(pv.size() * sizeof(world::PropVertex)), sizeof(world::PropVertex));
        c.pib = gpu.createIndex(cd.props.indices.data(), (u32)(cd.props.indices.size() * 4));
        c.propIndexCount = (u32)cd.props.indices.size();
        c.propBounds = cd.props.bounds;
    }

    // Ring-Texturen: Hoehenfeld und Umgebungslicht dieses Chunks
    const int R = ringSize_;
    const int rx = (int)(((cd.x0() % R) + R) % R), ry = (int)(((cd.y0() % R) + R) % R);
    u16 hf[world::CS * world::CS * 2];
    u16 am[world::CS * world::CS * 4];
    for (int y = 0; y < world::CS; ++y)
        for (int x = 0; x < world::CS; ++x) {
            const world::Tile& t = cd.tile(x, y);
            size_t i = (size_t)(y * world::CS + x);
            // V12: niedrige unsichtbare Bloecke (Kisten, Bett: nur Kollision) werfen keinen Kastenschatten - das
            // Modell darauf ist kleiner als die Kacheln; hohe (Regale, Saeulen) bleiben als Schattenwerfer
            float fl = t.floor;
            if ((t.flags & world::TF_HIDDEN) && t.room && t.floor - (float)t.room->floor < 1.2f) fl = (float)t.room->floor;
            hf[i * 2] = floatToHalf(t.solid() ? 60000.0f : fl);
            hf[i * 2 + 1] = floatToHalf(t.solid() ? -60000.0f : t.ceil);
            vec3 a = cd.ambient[i];
            am[i * 4] = floatToHalf(a.x);
            am[i * 4 + 1] = floatToHalf(a.y);
            am[i * 4 + 2] = floatToHalf(a.z);
            am[i * 4 + 3] = floatToHalf(1.0f);
        }
    D3D11_BOX box{(UINT)rx, (UINT)ry, 0, (UINT)(rx + world::CS), (UINT)(ry + world::CS), 1};
    gpu.ctx()->UpdateSubresource(height_.tex.get(), 0, &box, hf, world::CS * 4, 0);
    gpu.ctx()->UpdateSubresource(ambient_.tex.get(), 0, &box, am, world::CS * 8, 0);
    int slot = ringSlot(cd.key);
    ringOwner_[(size_t)slot] = cd.key;
    ringUsed_[(size_t)slot] = true;
    chunks_[cd.key] = std::move(c);
}

void WorldRenderer::removeChunk(Gpu& gpu, const world::ChunkData& cd) {
    chunks_.erase(cd.key);
    // Ringfeld nur zuruecksetzen, wenn dort nicht schon ein anderer Chunk liegt
    int slot = ringSlot(cd.key);
    if (ringUsed_[(size_t)slot] && ringOwner_[(size_t)slot] == cd.key) {
        ringUsed_[(size_t)slot] = false;
        const int R = ringSize_;
        resetRing(gpu, (int)(((cd.x0() % R) + R) % R), (int)(((cd.y0() % R) + R) % R), world::CS, world::CS);
    }
}

u32 WorldRenderer::updateLights(Gpu& gpu, const mat4& view, const Frustum& frustum, vec3 camPos, float maxDist,
                                float time, float intensityScale, bool shadows, u32 maxLights,
                                const std::vector<ExtraLight>* extra, float worldScale) {
    struct Cand {
        float d2;
        const world::ChunkLight* l;
    };
    static std::vector<Cand> cands;
    cands.clear();
    // Reichweite: V4-Radius (waagrecht) mal Faktor; kuerzer = mehr Kontrast, weniger Arbeit.
    // V4 rechnete Licht rein waagrecht. Deckenlampen in hohen Hallen (ueber 3 m) reichen deshalb
    // um den Mehrabstand weiter und strahlen staerker, aber gebuendelter nach unten
    // (Hallenstrahler), damit der Boden so hell bleibt wie in normalen Raeumen. Ein weicher
    // Kern (grosse Leuchtflaeche) verhindert dabei grelle Flecken an Waenden direkt neben der Lampe.
    auto overOf = [](const world::ChunkLight& L) {
        return L.kind == world::ChunkLight::Ceiling ? std::clamp(L.drop - 3.0f, 0.0f, 9.0f) : 0.0f;
    };
    // Hallenlampen reichen 1,5x den Mehrabstand weiter, damit das Ausblenden am Rand der
    // Reichweite im schwachen Bereich liegt (sonst runde, scharf begrenzte Lichtflecken an Waenden).
    auto rangeOf = [&](const world::ChunkLight& L) { return std::max(L.radius * 1.7f, 5.0f) + 1.5f * overOf(L); };
    for (const auto& [k, c] : chunks_)
        for (const auto& L : c.lights) {
            float range = rangeOf(L);
            vec3 d = L.pos - camPos;
            float d2 = dot(d, d);
            float lim = maxDist + range;
            if (d2 > lim * lim) continue;
            if (!frustum.intersectsSphere(L.pos, range)) continue;
            cands.push_back({d2, &L});
        }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
    // V7: zusaetzliche Leuchten (Taschenlampe, Schilder) stehen vorn - sie sind nah und wichtig
    const size_t ne = extra ? std::min<size_t>(extra->size(), 16) : 0;
    const size_t cap = std::min<size_t>(lights_.count, maxLights);
    size_t n = worldScale > 0.0f ? std::min<size_t>(cands.size(), cap > ne ? cap - ne : 0) : 0;  // V13.3: 0 = Welt dunkel
    lightData_.resize(ne + n);
    for (size_t i = 0; i < ne; ++i) {
        const ExtraLight& e = (*extra)[i];
        GpuLight& g = lightData_[i];
        vec3 dir = normalize(e.dir);
        vec3 side = normalize(cross(dir, std::fabs(dir.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0}));
        vec3 up = cross(side, dir);
        g.posRange = {e.pos, e.range};
        g.colorRadius = {e.color * intensityScale, e.size};
        g.dirSpill = {dir, e.spot ? e.cosOuter : e.spill};
        vec4 vp = view * vec4(e.pos, 1.0f);
        u32 flags = (shadows && e.shadows ? 1u : 0u) | (e.spot ? 2u : 0u);
        float ff;
        std::memcpy(&ff, &flags, 4);
        g.viewPosFlags = {vp.x, vp.y, vp.z, ff};
        g.areaU = {side * e.size, e.soft};
        g.areaV = {up * e.size, e.spot ? e.cosInner : 0.0f};
    }
    for (size_t i = 0; i < n; ++i) {
        const world::ChunkLight& L = *cands[i].l;
        GpuLight& g = lightData_[ne + i];
        float range = rangeOf(L);
        float fl = flickerFactor(L.flickerMode, L.flickerSeed, time);
        if ((int)L.flickerSeed == blackoutSeed_ && L.kind != world::ChunkLight::Machine && L.kind != world::ChunkLight::Glow &&
            (blackoutFront_ < 0.0f || std::hypot(L.pos.x - blackoutOrigin_.x, L.pos.y - blackoutOrigin_.y) < blackoutFront_))
            fl *= 1.0f - blackoutAmount_;  // V7, V11: Welle
        vec3 lin{srgbToLinear(L.color.x), srgbToLinear(L.color.y), srgbToLinear(L.color.z)};
        float power = L.intensity * intensityScale * worldScale * fl;
        vec3 dir{0, 0, -1};
        float over = overOf(L);
        float boost = 1.0f + over / 3.0f;
        power *= boost * boost;
        float spill = 0.18f / boost;
        float soft2 = over * over;  // Kern der Abstandsdaempfung: 1 / (d^2 + 0.25 + soft2)
        vec3 au{L.area.x * 0.5f, 0, 0}, av{0, L.area.y * 0.5f, 0};
        if (L.kind == world::ChunkLight::Glow) {
            dir = {0, 0, 1};  // V11: von unten nach oben, waagrechte Leuchtflaeche
            spill = 0.3f;
        } else if (L.kind != world::ChunkLight::Ceiling) {
            dir = normalize(vec3{(float)L.dirX, (float)L.dirY, 0.0f});
            vec3 side{-dir.y, dir.x, 0.0f};
            au = side * (L.area.x * 0.5f);
            av = vec3{0, 0, L.area.y * 0.5f};
            spill = L.kind == world::ChunkLight::Wall ? 0.25f : 0.12f;
            if (L.kind == world::ChunkLight::Machine) power = L.intensity * intensityScale * worldScale * 0.75f * fl;
        }
        g.posRange = {L.pos, range};
        g.colorRadius = {lin * power, std::max(L.area.x, L.area.y) * 0.5f};
        g.dirSpill = {dir, spill};
        vec4 vp = view * vec4(L.pos, 1.0f);
        u32 flags = shadows ? 1u : 0u;
        float ff;
        std::memcpy(&ff, &flags, 4);
        g.viewPosFlags = {vp.x, vp.y, vp.z, ff};
        g.areaU = {au, soft2};
        g.areaV = {av, 0};
    }
    if (ne + n) gpu.update(lights_, lightData_.data(), (u32)((ne + n) * sizeof(GpuLight)));
    return (u32)(ne + n);
}

u32 WorldRenderer::updatePuddles(Gpu& gpu, vec3 camPos, float maxDist) {
    static std::vector<std::pair<float, const world::PuddleDecal*>> cand;
    cand.clear();
    for (const auto& [k, c] : chunks_)
        for (const auto& p : c.puddles) {
            const float d = std::hypot(p.x - camPos.x, p.y - camPos.y);
            if (d < maxDist + p.radius && std::fabs(p.z - camPos.z) < 6.0f) cand.push_back({d, &p});
        }
    std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const u32 n = (u32)std::min<size_t>(cand.size(), kMaxPuddles);
    if (n == 0) return 0;
    vec4 data[kMaxPuddles * 2];
    for (u32 i = 0; i < n; ++i) {
        const world::PuddleDecal& p = *cand[i].second;
        data[i * 2] = {p.x, p.y, p.z, p.radius};
        data[i * 2 + 1] = {p.stretch, p.angle, p.seed, p.wet};
    }
    gpu.update(puddles_, data, n * 2 * (u32)sizeof(vec4));
    return n;
}

u64 WorldRenderer::drawProps(Gpu& gpu, const Frustum& frustum, vec3 camPos, float maxDist) {
    auto* ctx = gpu.ctx();
    UINT stride = sizeof(world::PropVertex), offset = 0;
    u64 tris = 0;
    for (const auto& [k, c] : chunks_) {
        if (!c.propIndexCount || !frustum.intersects(c.propBounds)) continue;
        vec3 mid = (c.propBounds.lo + c.propBounds.hi) * 0.5f;
        if (length(mid - camPos) > maxDist + 24.0f) continue;
        ctx->IASetVertexBuffers(0, 1, c.pvb.buf.addr(), &stride, &offset);
        ctx->IASetIndexBuffer(c.pib.buf.get(), DXGI_FORMAT_R32_UINT, 0);
        ctx->DrawIndexed(c.propIndexCount, 0, 0);
        tris += c.propIndexCount / 3;
    }
    return tris;
}

u32 WorldRenderer::drawChunks(Gpu& gpu, const Frustum& frustum, vec3 camPos) {
    drawList_.clear();
    for (const auto& [k, c] : chunks_) {
        if (!c.indexCount || !frustum.intersects(c.bounds)) continue;
        vec3 mid = (c.bounds.lo + c.bounds.hi) * 0.5f;
        vec3 d = mid - camPos;
        drawList_.push_back({dot(d, d), &c});
    }
    // von vorne nach hinten: frueher Tiefentest spart Pixelarbeit
    std::sort(drawList_.begin(), drawList_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    auto* ctx = gpu.ctx();
    UINT stride = sizeof(world::WorldVertex), offset = 0;
    triangles_ = 0;
    for (const auto& [d, c] : drawList_) {
        ctx->IASetVertexBuffers(0, 1, c->vb.buf.addr(), &stride, &offset);
        ctx->IASetIndexBuffer(c->ib.buf.get(), c->index16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        ctx->DrawIndexed(c->indexCount, 0, 0);
        triangles_ += c->indexCount / 3;
    }
    return (u32)drawList_.size();
}

}  // namespace lim::gfx
