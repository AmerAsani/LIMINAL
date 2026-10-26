// V10: Skelett der Figuren (gemeinsam fuer Animation und Modellierung).
//
// Eine Figur besteht aus 27 Knochen. Animiert werden nicht Winkel, sondern Gelenkpositionen
// (wie beim Strichmaennchen aus V6): Becken, Wirbel, Schultern, Ellbogen, Knie ... plus einige
// Richtungen (Blick, Ellbogen- und Kniestellung, Handflaeche). Daraus entsteht je Knochen ein
// starrer Rahmen: Ursprung im Startgelenk, x-Achse zum Endgelenk, z-Achse nach einem Hinweis.
// Das Modell wird in der Ruhepose (A-Pose) aus denselben Regeln aufgebaut - Skinning-Matrix je
// Knochen = Rahmen(animiert) * Rahmen(Ruhe)^-1.
//
// Koordinaten wie im Spiel: z nach oben, die Figur blickt in der Ruhepose entlang +x.
#pragma once

#include <array>
#include <cstdint>

#include "core/math.hpp"

namespace lim::anim {

enum Bone : std::uint8_t {
    B_ROOT, B_PELVIS, B_SPINE, B_CHEST, B_NECK, B_HEAD, B_JAW,
    B_CLAV_L, B_UARM_L, B_FARM_L, B_HAND_L, B_FING_L, B_THUMB_L,
    B_CLAV_R, B_UARM_R, B_FARM_R, B_HAND_R, B_FING_R, B_THUMB_R,
    B_THIGH_L, B_SHIN_L, B_FOOT_L, B_TOE_L,
    B_THIGH_R, B_SHIN_R, B_FOOT_R, B_TOE_R,
    kBoneCount
};

enum Joint : std::uint8_t {
    J_ROOT,      // Boden zwischen den Fuessen
    J_PELVIS,    // Beckenmitte (Hoehe der Hueftgelenke)
    J_SPINE,     // Lendenwirbel (Taille)
    J_CHEST,     // Brustwirbel (Hoehe Brustbein)
    J_NECK,      // Halsansatz (7. Halswirbel)
    J_HEAD,      // Schaedelbasis (Atlas)
    J_HEADTOP,   // Scheitel
    J_JAW,       // Kiefergelenk (Mitte)
    J_CHIN,      // Kinnspitze
    J_STERN_L, J_SHOULDER_L, J_ELBOW_L, J_WRIST_L, J_KNUCKLE_L, J_FINGERTIP_L, J_THUMBBASE_L, J_THUMBTIP_L,
    J_STERN_R, J_SHOULDER_R, J_ELBOW_R, J_WRIST_R, J_KNUCKLE_R, J_FINGERTIP_R, J_THUMBBASE_R, J_THUMBTIP_R,
    J_HIP_L, J_KNEE_L, J_ANKLE_L, J_BALL_L, J_TOETIP_L,
    J_HIP_R, J_KNEE_R, J_ANKLE_R, J_BALL_R, J_TOETIP_R,
    kJointCount
};

// Seite: 0 links, 1 rechts. Gelenk/Knochen der Seite s
constexpr Joint sideJoint(Joint left, int s) { return (Joint)(left + s * (J_STERN_R - J_STERN_L)); }
constexpr Joint legJoint(Joint left, int s) { return (Joint)(left + s * (J_HIP_R - J_HIP_L)); }
constexpr Bone sideBone(Bone left, int s) { return (Bone)(left + s * (B_CLAV_R - B_CLAV_L)); }
constexpr Bone legBone(Bone left, int s) { return (Bone)(left + s * (B_THIGH_R - B_THIGH_L)); }

// Elternknochen (fuer Gewichte an Gelenken)
extern const std::array<Bone, kBoneCount> kBoneParent;
// Start- und Endgelenk jedes Knochens (Root: kein Segment)
struct BoneSeg {
    Joint a, b;
};
extern const std::array<BoneSeg, kBoneCount> kBoneSeg;
const char* boneName(Bone b);

// Koerperbau: Laengen (m) und Staerken (Radien, m). Daraus entstehen Ruhepose und Modell.
struct Build {
    float height = 1.78f;
    // Laengen
    float hipHeight = 0.0f;        // Hueftgelenke ueber dem Boden (0 = aus height)
    float thigh = 0.0f, shin = 0.0f, ankle = 0.0f, foot = 0.0f;
    float spineLow = 0.0f, spineUp = 0.0f, neck = 0.0f;  // Becken->Taille, Taille->Brust, Brust->Halsansatz
    float headH = 0.0f;            // Kinn bis Scheitel
    float upperArm = 0.0f, forearm = 0.0f, hand = 0.0f;
    float shoulderW = 0.0f;        // Abstand der Schultergelenke
    float hipW = 0.0f;             // Abstand der Hueftgelenke
    // Staerken
    float neckR = 0.055f;
    float chestW = 0.17f, chestD = 0.115f;   // halbe Breite/Tiefe des Brustkorbs
    float waistW = 0.135f, waistD = 0.10f;
    float hipsW = 0.165f, hipsD = 0.11f;
    float bellyFwd = 0.0f;                    // Bauch (m nach vorn)
    float bust = 0.0f;                        // Brust (Frauen)
    float upperArmR = 0.045f, forearmR = 0.037f, wristR = 0.026f;
    float thighR = 0.075f, kneeR = 0.048f, calfR = 0.052f, ankleR = 0.032f;
    float handW = 0.042f;                     // halbe Handbreite
    float fingerLen = 1.0f;                   // Faktor auf die Fingerlaenge
    // Gestalt
    float thin = 0.0f;       // 0 normal .. 1 ausgemergelt (Rippen, Gelenke, Wangen)
    float muscle = 0.3f;     // Muskelrelief
    float headScale = 1.0f;  // Kopfgroesse
    float jawW = 1.0f;       // Kieferbreite
    float shoulderDrop = 0.0f;  // haengende Schultern (m)
    float armAbduct = 0.42f;    // Ruhepose: Armwinkel vom Koerper (rad)
    // aus height abgeleitete Werte setzen, wo 0 steht
    void finish();
};

// Gelenke und Richtungen einer Pose (Weltkoordinaten oder Modellraum)
struct JointPose {
    std::array<vec3, kJointCount> j{};
    vec3 fwd{1, 0, 0}, up{0, 0, 1};          // Koerperachsen (Becken)
    vec3 chestFwd{1, 0, 0};                  // Brustkorb (Drehung im Oberkoerper)
    vec3 headFwd{1, 0, 0}, headUp{0, 0, 1};  // Gesicht
    vec3 armPole[2]{{-1, 0, 0}, {-1, 0, 0}};  // wohin der Ellbogen zeigt
    vec3 legPole[2]{{1, 0, 0}, {1, 0, 0}};    // wohin das Knie zeigt
    vec3 palm[2]{{0, 1, 0}, {0, -1, 0}};      // Handflaechennormale
    vec3 footUp[2]{{0, 0, 1}, {0, 0, 1}};
    float jawOpen = 0.0f;                     // 0..1
    float fingerCurl[2]{0.15f, 0.15f};        // 0 gestreckt .. 1 Faust
    float thumbCurl[2]{0.2f, 0.2f};
};

// Starrer Rahmen eines Knochens (Spalten: x, y, z, Ursprung)
using Frames = std::array<mat4, kBoneCount>;

// Ruhepose (A-Pose, Blick +x, Fuesse bei z = 0)
JointPose bindPose(const Build& b);
// Finger und Daumen aus Handgelenk, Handrichtung, Handflaeche (p.palm) und Kruemmung setzen
void placeHand(JointPose& p, const Build& b, int side, vec3 handDir);
// Ballen und Zehenspitze aus Knoechel, Fussrichtung und p.footUp setzen
void placeFoot(JointPose& p, const Build& b, int side, vec3 footFwd);
// Rahmen aller Knochen aus einer Pose
void computeFrames(const JointPose& p, Frames& out);
// Inverse eines starren Rahmens
mat4 rigidInverse(const mat4& m);
// Skinning-Matrizen: anim * inverse(bind), Ergebnis relativ zu origin (Weltposition der Wurzel)
void skinMatrices(const Frames& anim, const Frames& bindInv, vec3 origin, Frames& out);

// --- kleine Rechenhilfen ---------------------------------------------------------------
inline vec3 rotateAround(vec3 v, vec3 axis, float ang) {
    float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
// Zwei-Knochen-IK: Wurzel A, Ziel T, Laengen l1/l2, pole: Richtung des Mittelgelenks
void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);
inline vec3 orthoTo(vec3 v, vec3 axis) { return v - axis * dot(v, axis); }

}  // namespace lim::anim
