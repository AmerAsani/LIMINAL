// V12: Texturen aus Bilddateien (CC0, ambientCG; Quelle und Lizenz in LICENSES.md).
//
// data/textures/textures.json nennt je Textur einen Namen, den Ordner mit albedo.jpg / normal.png / orm.png
// (aufbereitet von tools/textures/process.py) und die reale Kantenlaenge. Jede Textur belegt eine eigene
// Ebene im Texturblock ab kImportedLayerBase. Zonen waehlen sie in den Level-Daten ueber "wall_tex",
// "floor_tex" und "ceil_tex" (Name), Ausstattung ueber den Namen im Modell.
#pragma once

#include <string>
#include <vector>

#include "core/core.hpp"

namespace lim::world {

constexpr int kImportedLayerBase = 43;   // = gfx::L_IMP0 (static_assert in render/texture_gen.cpp)
constexpr int kImportedLayerCount = 48;

struct ImportedTexture {
    std::string name;     // Name in den Level-Daten
    std::string dir;      // absoluter Ordner mit albedo.jpg, normal.png, orm.png
    std::string source;   // ambientCG-ID (fuer Protokoll und LICENSES.md)
    float meters = 2.0f;  // reale Kantenlaenge einer Kachel
    float normalStrength = 1.0f;
    float roughBias = 0.0f;
    float variation = 0.25f;  // grossflaechige Variation im Shader (0..1)
    float antitile = 0.0f;    // V13: Kachelbruch im Shader (1 = an; nur fuer Texturen ohne Raster)
};

// Liest data/textures/textures.json (einmal; spaetere Aufrufe mit demselben Ordner tun nichts)
bool loadImportedTextures(const std::string& dataDir, std::string& err);
const std::vector<ImportedTexture>& importedTextures();
// Ebene (kImportedLayerBase + i) zu einem Namen, -1 = unbekannt
int importedLayer(const std::string& name);

}  // namespace lim::world
