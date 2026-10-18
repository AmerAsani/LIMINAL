// Eintrag fuer das Zeichnen (vom Renderer in MeshDraw umgesetzt). Das Gameplay kennt
// nur Modellnamen; die Netz-ID loest die Anwendung einmalig auf und merkt sie sich.
#pragma once

#include <string>

#include "core/math.hpp"

namespace lim::game {

struct VisibleMesh {
    const std::string* model = nullptr;
    int* meshId = nullptr;
    mat4 world;
    float highlight = 0;
    bool attached = false;  // V6: bewegt sich mit dem Spieler (eigener Koerper) -> TAA-Sonderfall
    // V10: Figuren mit Skinning
    int bones = -1;         // erste Matrix in der Knochenpalette dieses Bildes (-1: starres Modell)
    bool dynamic = false;   // bewegt sich (TAA)
    vec3 cullCenter;        // Huellkugel (cullRadius > 0: ausserhalb des Bildes nicht zeichnen)
    float cullRadius = 0.0f;
    int material = -1;      // >= 0: alle Teile mit diesem Material; kShadowLook: das Trugbild
};
constexpr int kShadowLook = 1000;  // die Anwendung setzt dafuer das Schattenmaterial

}  // namespace lim::game
