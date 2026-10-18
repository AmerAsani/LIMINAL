// V10: Modelle der Figuren (prozedural modelliert, mit Skinning-Gewichten und Detailstufen).
//
// Jede Figur besteht aus Teilen mit eigenem Material: Haut (Koerper, Kopf, Haende), Kleidung,
// Haare, Augen, Kleinteile (Ausweis). Die Formen kommen aus render/sculpt (Distanzfelder), die
// Gewichte aus dem Abstand zu den Knochen der Ruhepose (anim/skeleton). Stoff, Haut und Haar
// werden im Shader triplanar texturiert (Ruheposition je Vertex) - ohne Naehte.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "anim/creature.hpp"
#include "anim/figures.hpp"
#include "core/core.hpp"
#include "core/math.hpp"

namespace lim {
class JobSystem;
}

namespace lim::gfx {

struct SkinVertex {
    vec3 pos;
    vec3 normal;
    vec4 tangent;  // triplanare Materialien: xyz = Normale der Ruhepose
    vec2 uv;
    vec3 bind;     // Position der Ruhepose (triplanare Texturen)
    u8 bones[4];
    u8 weights[4];
};
static_assert(sizeof(SkinVertex) == 68);

struct SkinPart {
    std::vector<SkinVertex> v;
    std::vector<u32> idx;
    u32 material = 0;
    bool head = false;  // Kopf, Haare, Augen (fehlen im Ego-Modell des Spielers)
};

struct FigureModel {
    std::array<std::vector<SkinPart>, anim::kFigureLods> lods;
    std::vector<mat4> bindInv;  // je Knochen: Ruherahmen^-1
    float radius = 1.2f;  // Huellkugel um die Beckenmitte
};

FigureModel buildFigure(anim::Figure f, JobSystem* jobs);
// V10: die Wesen (render/entity_models.cpp)
FigureModel buildEntity(anim::Entity e, JobSystem* jobs);
// Kennzahlen fuer das Protokoll
std::string figureStats(const FigureModel& m);

}  // namespace lim::gfx
