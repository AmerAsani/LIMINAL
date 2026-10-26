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
    bool hands = false; // V13: die Haende (fuer Varianten ohne eine Hand)
};
// V13: Teile ohne die Hand einer Seite (Dreiecke, deren Punkte vor allem an Hand, Fingern, Daumen oder Unterarm dieser
// Seite haengen, entfallen) - die Hand aus Blender ersetzt sie
std::vector<SkinPart> withoutHand(const std::vector<SkinPart>& parts, int side);

struct FigureModel {
    std::array<std::vector<SkinPart>, anim::kFigureLods> lods;
    std::vector<mat4> bindInv;  // je Knochen: Ruherahmen^-1
    float radius = 1.2f;  // Huellkugel um die Beckenmitte
};

FigureModel buildFigure(anim::Figure f, JobSystem* jobs);
// V10: die Wesen (render/entity_models.cpp)
FigureModel buildEntity(anim::Entity e, JobSystem* jobs);
// V13: ein Wesen aus Blender (data/models/<entityModel>_l<Stufe>.glb, Skin mit Knochen "B00".., Materialien fm=ent_*).
// false, wenn eine Stufe fehlt oder nicht passt (dann bleibt das prozedurale Modell); info: Quelle fuer das Protokoll
bool entityFromGlb(anim::Entity e, FigureModel& out, std::string& info, const std::string& base = "");  // V13.2: base = Variante
// V13: Inhalt-Hash eines Modells (Punkte und Indizes aller Stufen) - fuer den Nachweis im Protokoll
u64 modelHash(const FigureModel& m);
// Kennzahlen fuer das Protokoll
std::string figureStats(const FigureModel& m);

}  // namespace lim::gfx
