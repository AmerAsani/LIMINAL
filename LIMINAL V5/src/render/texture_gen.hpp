// Prozedurale PBR-Texturen (Albedo, Normalen, Rauheit/AO/Metall).
//
// Alle Oberflaechen der Welt entstehen beim Start aus mathematischen
// Beschreibungen - es werden keine Bilddateien benoetigt. Jede Ebene ist
// nahtlos kachelbar und hat eine reale Groesse in Metern (Fugen, Fliesen,
// Tapetenbahnen stimmen mit der Weltgeometrie ueberein).
#pragma once

#include <string>
#include <vector>

#include "core/core.hpp"

namespace lim {
class JobSystem;
}

namespace lim::gfx {

enum TexLayer : u8 {
    L_PLASTER, L_CARPET, L_SLABS, L_BIGTILES, L_GRATE, L_CEILTILE, L_FLOORTILE, L_CHECKER,
    L_WALLPAPER, L_BLOCKS, L_BRICKS, L_PANELS, L_CONCRETE, L_WOOD, L_METAL, L_LAMINATE,
    L_FABRIC, L_MONOLITH, L_DIFFUSER, L_CRATE, L_VENDING, L_TRIM, L_ITEM_ENERGY, L_ITEM_ALMOND,
    L_COUNT
};

struct LayerDesc {
    const char* name;
    float meters;          // reale Kantenlaenge der Textur
    float normalStrength;
    float roughBias;
    float variation;       // Standardstaerke der grossflaechigen Variation
};
extern const LayerDesc kLayers[L_COUNT];

// Mip-Kette aller Ebenen in einem Block: Ebene fuer Ebene, je Ebene alle Mips.
struct TextureSet {
    int size = 0;
    int mips = 0;
    std::vector<u8> albedo;  // RGBA8 (sRGB)
    std::vector<u8> normal;  // RG8 (Normale xy * 0.5 + 0.5)
    std::vector<u8> orm;     // RGBA8: AO, Rauheit, Metall, Hoehe
    size_t mipOffset(int layer, int mip, int bytesPerPixel) const;
};

TextureSet generateTextures(int size, JobSystem& jobs);
// Grossflaechiges, kachelbares Variationsrauschen (RGBA8)
std::vector<u8> generateMacroNoise(int size);
// Symbole fuer die Schnellleiste/Inventar (RGBA8, vormultipliziert, sRGB)
std::vector<u8> generateItemIcon(const std::string& kind, int size);

}  // namespace lim::gfx
