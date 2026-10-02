// V6: Massblatt des Getraenke- und Snackautomaten. Modell (meshes.cpp), Textur-Atlas
// (texture_vending.cpp) und Glas-Shader (mesh_gbuffer.hlsl: Interior Mapping) teilen diese Werte.
//
// Lokales Koordinatensystem des Modells: Ursprung in der Mitte der Kachel auf dem Boden,
// +x zeigt aus der Front in den Raum, die Rueckwand steht an der Wand (x = -0.5), z nach oben.
// Von vorn gesehen liegt +y links (die Bildschirmseite rechts ist -y, siehe viewMatrix).
#pragma once

namespace lim::gfx::vend {

// Gehaeuse
constexpr float kBack = -0.48f;   // Rueckseite (x)
constexpr float kFront = 0.30f;   // Frontebene (x)
constexpr float kHalfW = 0.46f;   // halbe Breite (y)
constexpr float kHeight = 1.86f;
constexpr float kBase = 0.10f;    // Sockel (zurueckgesetzt)
constexpr float kChamfer = 0.02f; // Fase der senkrechten Frontkanten

// Front, in "Frontkoordinaten": X von links nach rechts (von vorn gesehen, 0 .. 2 * kHalfW),
// Z von oben nach unten (0 .. kHeight). Umrechnung: X = kHalfW - y, Z = kHeight - z.
struct Rect {
    float x0, z0, x1, z1;
};
constexpr Rect kWindow{0.05f, 0.24f, 0.65f, 1.22f};   // Glasfenster (0.60 x 0.98 m)
constexpr Rect kPanel{0.68f, 0.24f, 0.88f, 1.22f};    // Bedienfeld
constexpr Rect kDisplay{0.705f, 0.31f, 0.855f, 0.39f}; // Anzeige (leuchtet)
constexpr Rect kHeader{0.04f, 0.04f, 0.88f, 0.20f};   // Leuchtschild oben
constexpr Rect kFlap{0.10f, 1.36f, 0.60f, 1.64f};     // Ausgabeklappe
constexpr float kPanelOut = 0.025f;                    // Bedienfeld steht vor der Front
constexpr float kHeaderOut = 0.04f;                    // Leuchtschild steht vor
constexpr float kGlassIn = 0.012f;                     // Glas liegt hinter der Frontebene
constexpr int kRows = 5;                               // Faecher hinter dem Glas
constexpr float kInteriorDepth = 0.5f;

// Atlas der Gehaeusetextur: Front links (u 0 .. kFrontU), Seite rechts (u kFrontU .. 1), v = Z / kHeight
constexpr float kFrontU = 0.62f;
constexpr float kDepth = kFront - kBack;
inline float frontU(float X) { return X / (2.0f * kHalfW) * kFrontU; }
inline float sideU(float depthFromFront) { return kFrontU + depthFromFront / kDepth * (1.0f - kFrontU); }
inline float atlasV(float Z) { return Z / kHeight; }

}  // namespace lim::gfx::vend
