// V11: Bibliothek der Ausstattungsmodelle. Jedes Modell entsteht einmal beim ersten Zugriff aus
// dem Geometrie-Baukasten (prop_mesh.hpp) und ist danach unveraenderlich (threadsicher lesbar).
//
// Materialien sind Beschreibungen (Look + Farbe + Leuchten); die Welt legt daraus Materialien in
// ihrer Materialbank an (world/chunk_builder). Teile mit Atlas (Schilder, Beschriftungen, Bilder)
// tragen Atlaskoordinaten der Texturebene L_SIGNS (render/texture_v11.cpp).
#pragma once

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
};

struct Part {
    Mesh mesh;
    MatDesc mat;
};

struct Model {
    std::vector<Part> parts;
    Aabb bounds;
};

// Modell nach Name und Variante (nullptr: unbekannt). Varianten ausserhalb des Bereichs werden
// auf den Bereich abgebildet.
const Model* model(const std::string& name, int variant = 0);
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
