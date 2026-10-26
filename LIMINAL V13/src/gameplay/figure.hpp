// V10: Eine dargestellte Figur - Pose -> Skinning-Matrizen, Detailstufe, Schattenkapseln.
// Mensch (der Spieler, sein Trugbild) mit dem Menschenskelett oder ein Wesen mit eigenem Skelett.
#pragma once

#include <string>
#include <vector>

#include "anim/creature.hpp"
#include "anim/figures.hpp"
#include "anim/skeleton.hpp"
#include "gameplay/visible.hpp"

namespace lim::game {

// Kapseln einer Figur fuer weiche Schatten (render/lighting.hlsl)
struct ShadowCaster {
    vec3 center;
    float radius = 1.2f;
    bool player = false;
    std::vector<vec4> caps;  // je Kapsel 2: (a, r), (b, 0)
};

class Body;
// V13.2: Pose des Koerpers (Body) als Skelett der Spielerfigur (Fuesse bei root)
anim::JointPose playerJointPose(const Body& body, vec3 root);

class FigureRig {
public:
    void init(anim::Figure f);        // Mensch
    void initEntity(anim::Entity e);  // Wesen
    bool creature() const { return creature_; }
    anim::Entity entity() const { return ent_; }
    anim::Figure figure() const { return fig_; }
    void setPose(const anim::JointPose& p);
    void setCreaturePose(const anim::CreaturePose& p);
    const anim::JointPose& pose() const { return pose_; }
    // detail: 0 niedrig .. 3 ultra. firstPerson: Spieler ohne Kopf/Haare/Augen (Kamera sitzt darin).
    // material >= 0: alle Teile mit diesem Material (das Trugbild des Spielers)
    void collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail, bool dynamic, bool attached = false,
                 bool firstPerson = false, int material = -1);
    void shadow(ShadowCaster& out) const;
    vec3 center() const;
    bool posed() const { return posed_; }
    void setGlow(float g) { glow_ = g; }  // Leuchten (Augen und Zaehne des Smilers, Augen des Hounds)
    // V12: Hand einer Skelettseite ausblenden (-1 keine): Hand, Finger und Daumen schrumpfen ins Handgelenk -
    // dort sitzt dann die Hand aus Blender (Viewmodel)
    void hideHand(int side) { hiddenHand_ = side; }

private:
    bool creature_ = false;
    anim::Entity ent_ = anim::Entity::Smiler;
    anim::Figure fig_ = anim::Figure::Player;
    anim::Build b_;
    std::vector<mat4> bindInv_, frames_;
    anim::JointPose pose_;
    anim::CreaturePose cpose_;
    vec3 origin_;
    float height_ = 1.8f, reach_ = 1.2f, glow_ = 0.0f;
    int hiddenHand_ = -1;  // V12
    std::string modelNh_[anim::kFigureLods], fpModelNh_;  // V13: Netze ohne die verborgene Hand (Seite 1)
    int meshIdNh_[anim::kFigureLods] = {-2, -2, -2}, fpIdNh_ = -2;
    std::string model_[anim::kFigureLods], fpModel_;
    int meshId_[anim::kFigureLods] = {-2, -2, -2};
    int fpId_ = -2;
    bool posed_ = false;
};

// Kapseln aller Figuren -> Zeilen fuer den Renderer (Koepfe zuerst, dann Kapseln)
void packShadows(const std::vector<ShadowCaster>& casters, std::vector<vec4>& rows, int& figures);

}  // namespace lim::game
