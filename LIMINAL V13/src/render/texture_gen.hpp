// Prozedurale PBR-Texturen (Albedo, Normalen, Rauheit/AO/Metall).
//
// Alle Oberflaechen der Welt entstehen beim Start aus mathematischen
// Beschreibungen - es werden keine Bilddateien benoetigt. Jede Ebene ist
// nahtlos kachelbar und hat eine reale Groesse in Metern (Fugen, Fliesen,
// Tapetenbahnen stimmen mit der Weltgeometrie ueberein).
#pragma once

#include <string>
#include <functional>
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
    L_VEND_BODY,  // V6: Gehaeuse-Atlas des Automaten (L_VENDING: Faecher hinter dem Glas)
    L_ITEM_BATTERY, L_ITEM_NOTE, L_EXIT,  // V7
    L_ELEV,                               // V8: Etagenanzeige der Aufzuege
    L_FUSE,                               // V9: Sicherungskasten und Sicherung (Atlas)
    // V10: Figuren (triplanar, reale Groesse)
    L_SKIN, L_WOOL, L_COTTON, L_WORSTED, L_DENIM, L_LEATHER, L_HAIR,
    L_FIGDETAIL,                          // Atlas: Ausweis, Iris, Krawattenmuster
    // V11: Ausstattung
    L_SIGNS,                              // Atlas 8 x 8: Schablonenschrift, Schilder, Bilder (props/prop_library.hpp)
    L_PAINTED,                            // lackiertes Blech mit Abplatzern
    L_ROADPAINT,                          // abgefahrene Fahrbahnfarbe
    L_RUBBER,                             // Gummi (Reifen)
    L_PLANKS,                             // Dielenboden (Dachboden, Hotel)
    // V12: Texturen aus Bilddateien (world/imported_textures.hpp), 48 Ebenen
    L_IMP0,
    L_COUNT = L_IMP0 + 48
};

struct LayerDesc {
    const char* name;
    float meters;          // reale Kantenlaenge der Textur
    float normalStrength;
    float roughBias;
    float variation;       // Standardstaerke der grossflaechigen Variation
};
extern const LayerDesc kLayers[L_COUNT];

// Aufbau der Texturebenen: Mip-Kette Ebene fuer Ebene, je Ebene alle Mips (Versaetze innerhalb einer Ebene: layer 0).
// V13.2 (B-002): die Daten liegen nicht mehr gesammelt hier, sondern kommen Ebene fuer Ebene als TextureLayer.
struct TextureSet {
    int size = 0;
    int mips = 0;
    bool compressed = false;  // V6: BC1 Albedo, BC5 Normalen (texture_compress.hpp)
    size_t mipOffset(int layer, int mip, int bytesPerPixel) const;
    size_t bcOffset(int layer, int mip, int blockBytes) const;
};

// Eine fertige Ebene mit ihrer ganzen Mip-Kette
struct TextureLayer {
    int layer = 0;
    std::vector<u8> albedo;  // RGBA8 (sRGB) oder BC1
    std::vector<u8> normal;  // RG8 (Normale xy * 0.5 + 0.5) oder BC5
    std::vector<u8> orm;     // RGBA8: AO, Rauheit, Metall, Hoehe (unkomprimiert: die Hoehe steuert die Parallaxe)
};

// V13.2 (B-002): Ebene fuer Ebene erzeugen; compress: Albedo BC1, Normalen BC5 (sofort je Ebene, V6: ~45 % weniger
// Grafikspeicher). Gleichzeitig nur textureLayerSlots() Ebenen (Speicherbudget ~450 MB fuer Zwischenpuffer).
// sink wird aus Arbeitsthreads aufgerufen (je Ebene einmal, in beliebiger Reihenfolge).
TextureSet generateTextures(int size, JobSystem& jobs, bool compress, const std::function<void(TextureLayer&&)>& sink);
int textureLayerSlots(int size, unsigned workers);
int textureMips(int size);
// Grossflaechiges, kachelbares Variationsrauschen (RGBA8)
std::vector<u8> generateMacroNoise(int size);
// Symbole fuer die Schnellleiste/Inventar (RGBA8, vormultipliziert, sRGB)
std::vector<u8> generateItemIcon(const std::string& kind, int size);

}  // namespace lim::gfx
