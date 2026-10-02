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
};

}  // namespace lim::game
