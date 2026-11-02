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
    vec4 clip{0, 0, 0, 0};  // V13: Schnittkugel (w > 0): Pixel darin verwerfen (Haut des Koerpers am Handgelenk)
    bool dualQuat = false;  // V13.2 (B-001): Skinning mit Dual Quaternions (Menschen)
    vec4 fx{0, 0, 0, 0};    // V13.3: Zeichen auf der Haut (x Pilz, y Ranken; 0..1) - nur Haut der Figuren (KREATUR_Crawler.md)
};
constexpr int kShadowLook = 1000;  // die Anwendung setzt dafuer das Schattenmaterial
constexpr int kUmbraLook = 1001;   // V13.3 Teil 4 Level 5: schwarz und stumpf, schluckt das Licht (Kolonie, KREATUR_Ants.md)
constexpr int kRotSkinLook = 1002;  // V13.3 Teil 4 Level 5: graugruene, wachsige Haut der Wesen (Watchers, KREATUR_Watchers.md)
constexpr int kDarkSkinLook = 1003; // V13.3 Teil 4 Level 5: dunkelgraue, trockene Haut (Leib der Watchers)

}  // namespace lim::game
