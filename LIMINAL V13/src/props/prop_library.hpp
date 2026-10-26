// V11: Bibliothek der Ausstattungsmodelle. Jedes Modell entsteht einmal beim ersten Zugriff aus
// dem Geometrie-Baukasten (prop_mesh.hpp) und ist danach unveraenderlich (threadsicher lesbar).
//
// Materialien sind Beschreibungen (Look + Farbe + Leuchten); die Welt legt daraus Materialien in
// ihrer Materialbank an (world/chunk_builder). Teile mit Atlas (Schilder, Beschriftungen, Bilder)
// tragen Atlaskoordinaten der Texturebene L_SIGNS (render/texture_v11.cpp).
#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "props/prop_mesh.hpp"

namespace lim::props {

enum class Look : u8 {
    Paint,      // lackiertes Blech/Holz (dielektrisch, Abplatzer)
    CarPaint,   // Autolack: glatt, staubig
    Metal,      // blankes Metall, Stahl
    Galvanized, // verzinkt
    Plastic,
    Rubber,
    Glass,      // dunkle Scheibe, glatt
    Water,      // stehendes Wasser (spiegelt)
    RoadPaint,  // Fahrbahnfarbe, abgefahren
    Sign,       // Atlas (L_SIGNS)
    Stencil,    // Atlas, Grund ausgeschnitten (aufgespruehte Schrift)
    Wood,
    Fabric,
    Concrete,
    Cardboard,
    Glow,       // Leuchtflaeche (Diffusor, Glas einer Lampe)
    Ceramic,
};

struct MatDesc {
    Look look = Look::Paint;
    float r = 0.6f, g = 0.6f, b = 0.6f;
    float glow = 0.0f;  // > 0: leuchtet (Staerke relativ)
    std::string tex;    // V12: Textur aus data/textures (Name), leer = nach Look
    std::string fm;     // V12: festes Material des Renderers (Viewmodel: Hand, Taschenlampe), leer = keins
};

struct Part {
    Mesh mesh;
    MatDesc mat;
    // V13: Skinning (nur Modelle mit Skelett, props/glb_import): je Punkt vier Gelenke und Gewichte
    std::vector<std::array<u8, 4>> joints;
    std::vector<std::array<float, 4>> weights;
};

// V13: Skelett eines Modells aus Blender (glTF-Skin). Alles in glTF-Koordinaten (y oben); props::skinMatrices
// rechnet ins Spiel um. Posen sind Animationen mit einem Schluessel (Name wie die Aktion in Blender).
struct SkinJoint {
    std::string name;
    int parent = -1;   // Gelenk-Index, -1: Wurzel (darueber liegt rootPre)
    float t[3] = {0, 0, 0}, r[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};  // Ruhe: lokale Lage
    float ibm[16] = {};  // inverse Bindematrix (spaltenweise)
};
struct SkinPose {
    std::string name;
    std::vector<std::array<float, 10>> trs;  // je Gelenk t(3), r(4), s(3)
};
struct Skin {
    std::vector<SkinJoint> joints;
    float rootPre[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::vector<SkinPose> poses;
    int pose(const std::string& name) const;  // Index oder -1
};

struct Model {
    std::vector<Part> parts;
    Aabb bounds;
    std::shared_ptr<const Skin> skin;  // V13: nullptr = starr
    std::string file;                  // V13: Quelldatei (leer: im Code gebaut)
    u64 fileHash = 0;                  // V13: FNV-1a (64 Bit) der Datei - beweist im Protokoll, welche Fassung geladen ist
};

// V13: Skinning-Matrizen (Spielkoordinaten, Modellraum) fuer eine Mischung von Posen: weights[i] gehoert zu
// skin.poses[i] (Summe beliebig > 0; Rotationen werden normiert gemischt). Ergebnis je Gelenk.
void skinMatrices(const Skin& skin, const std::vector<float>& weights, std::vector<mat4>& out);
// V13: Lage eines Gelenks in Ruhe (Spielkoordinaten, Modellraum); false, wenn es fehlt
bool skinJointRest(const Skin& skin, const std::string& name, vec3& out);

// Modell nach Name und Variante (nullptr: unbekannt). Varianten ausserhalb des Bereichs werden
// auf den Bereich abgebildet.
const Model* model(const std::string& name, int variant = 0);
// V12: Modell aus einer Datei (props/glb_import) anmelden; ueberdeckt ein gleichnamiges Code-Modell
void registerModel(const std::string& name, Model m);
int variants(const std::string& name);
// alle Namen (Pruefung der Karten)
std::vector<std::string> modelNames();

// Atlas L_SIGNS: 8 x 8 Zellen. Zelle eines Zeichens (A-Z, 0-9, '-'), sonst -1
int glyphCell(char c);
constexpr int kCellExit = 38, kCellExitArrow = 39, kCellArrow = 40, kCellWarn = 41, kCellNoEntry = 42, kCellFire = 43;
constexpr int kCellPainting0 = 48;  // 8 Bilder
constexpr int kCellPaper0 = 56;     // 4 Zettel/Aushaenge
inline vec2 cellUv0(int c) { return {(float)(c % 8) / 8.0f, (float)(c / 8) / 8.0f}; }
inline vec2 cellUv1(int c) { return {(float)(c % 8 + 1) / 8.0f, (float)(c / 8 + 1) / 8.0f}; }

}  // namespace lim::props
