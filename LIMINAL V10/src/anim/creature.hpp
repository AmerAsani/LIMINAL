// V10: Die Entities von LIMINAL - eigene Skelette fuer nicht-menschliche Koerper. Grundlage sind die
// Beschreibungen der Backrooms-Wikis (Wikidot, Fandom); Quellen und Auslegung: docs/ENTITAETEN.md.
//
//   Smiler (Entity 3)   im Dunkeln sichtbar ist fast nur das Gesicht: weiss leuchtende Augen und ein
//                       langes Laecheln aus vielen spitzen Zaehnen. Der Koerper ist eine formlose,
//                       schwarze Masse, die wie Rauch oder Flammen nach oben zuengelt. Keine Beine.
//   Hound (Entity 8)    ein aus einem Menschen entstandenes Wesen auf allen vieren: duenne, knochige
//                       Glieder, vorn Haende mit Schwimmhaeuten, hinten pfotenartige Fuesse mit
//                       erhobener Ferse. Langes, straehniges schwarzes Haar faellt ueber Kopf und
//                       Gesicht und laeuft als Maehne den Ruecken entlang. Grosses Maul, weisse Augen.
//   Deathmoth (Entity 4) eine Motte mit etwa 45 cm Spannweite: zwei Fluegelpaare, gefiederte Fuehler,
//                       Facettenaugen, Saugruessel, sechs Beine, gegliederter Hinterleib mit zwei
//                       Schwanzborsten. Wurzel = Mitte der Brust (sie fliegt).
//   Gestalt (Level 0)   unbestaetigte "dunkle Gestalt": ein dunkler, formloser Umriss mit Kopf und
//                       Schultern, unten zerfasert. Keine Beine, kein Gang.
//   Nest (Entity 4)     fleischige, harzige Wucherung um einen Tuerrahmen (Deathmoth-Nest). Starr.
//
// Wie beim Menschen (anim/skeleton) werden Gelenkpositionen animiert; jeder Knochen ist ein starrer
// Rahmen vom Start- zum Endgelenk, gedreht nach einem Hinweisvektor ("oben"). Skinning-Matrix =
// Rahmen(animiert) * Rahmen(Ruhe)^-1. Koordinaten: x vorn, y links, z oben; Wurzel am Boden
// (Deathmoth: in der Brust).
#pragma once

#include <cstdint>
#include <vector>

#include "core/math.hpp"

namespace lim::anim {

enum class Entity : std::uint8_t { Smiler, Hound, Moth, Gestalt, Nest, Count };
constexpr int kEntityCount = (int)Entity::Count;

struct CBone {
    std::uint8_t a = 0, b = 0;  // Start- und Endgelenk (a == b: Wurzel)
    std::int8_t parent = -1;
    float radius = 0.05f;       // Staerke (Gewichte, Schattenkapseln)
    bool shadow = true;         // als Kapsel im Schatten
};

struct CreatureSkeleton {
    Entity kind = Entity::Smiler;
    std::vector<CBone> bones;
    std::vector<vec3> bind;    // Gelenke der Ruhepose
    std::vector<vec3> bindUp;  // je Knochen: Hinweis "oben" der Ruhepose
    float height = 2.0f;       // Standhoehe (Wahrnehmung)
    float reach = 1.2f;        // Huellradius um die Mitte (Sichtbarkeitstest)
    vec3 center{0, 0, 1};      // Mitte der Ruhepose
    int boneCount() const { return (int)bones.size(); }
};

struct CreaturePose {
    std::vector<vec3> j;   // Gelenke (Welt)
    std::vector<vec3> up;  // je Knochen
};

const CreatureSkeleton& entitySkeleton(Entity e);
const char* entityModel(Entity e);  // Basisname der Modelle ("ent_smiler"), je Stufe "_l0" ...
const char* entityName(Entity e);   // Name im Spiel

// starrer Rahmen (Spalten x, y, z, Ursprung): x von a nach b, z nach up
mat4 boneFrame(vec3 a, vec3 b, vec3 up);
// Rahmen aller Knochen einer Pose (Wurzel: x = up[0] als Blickrichtung, z = Welt oben)
void creatureFrames(const CreatureSkeleton& s, const CreaturePose& p, std::vector<mat4>& out);
// Ruhepose als Pose (Welt = Modellraum)
CreaturePose bindPose(const CreatureSkeleton& s);

// --- Gelenke und Knochen je Entity -----------------------------------------------------------
// Seiten: 0 links (+y), 1 rechts (-y). Gelenk der rechten Seite = linkes + Abstand.

// Smiler: Masse aus drei Abschnitten, Gesicht (Augen, obere Zahnreihe), Kiefer (untere Reihe),
// sechs Zungen schwarzer Masse (je 3 Knochen)
namespace sm {
constexpr int kWisps = 6, kWispJ = 4;
enum J : std::uint8_t {
    ROOT, BASE, CORE, UPPER, FACE, FACETIP, JAWH, JAWTIP,
    WISP0,  // + k * kWispJ + i
    JCOUNT = WISP0 + kWisps * kWispJ
};
enum B : std::uint8_t {
    B_ROOT, B_LOW, B_MID, B_HEAD, B_FACE, B_JAW,
    B_WISP0,  // + k * 3 + i
    BCOUNT = B_WISP0 + kWisps * 3
};
inline int wisp(int k, int i) { return WISP0 + k * kWispJ + i; }
inline int wispB(int k, int i) { return B_WISP0 + k * 3 + i; }
}  // namespace sm

// Hound: Rumpf, Hals, Schaedel mit Kiefer; Arme als Vorderbeine mit fuenf Fingern; Hinterbeine mit
// Sprunggelenk und drei Zehen; Haar: drei lange Straehnen am Kopf (je 3 Knochen), sechs kuerzere
// am Ruecken (je 2 Knochen, links und rechts)
namespace hd {
constexpr int kHeadHair = 3, kHeadHairJ = 4, kBackHair = 6, kBackHairJ = 3;
enum J : std::uint8_t {
    ROOT, PELVIS, SPINE1, SPINE2, CHEST, NECK1, NECK2, SKULL, SNOUT, JAWH, JAW,
    SHOULDER_L, ELBOW_L, WRIST_L, KNUCK_L, D0_L, D1_L, D2_L, D3_L, D4_L,
    SHOULDER_R, ELBOW_R, WRIST_R, KNUCK_R, D0_R, D1_R, D2_R, D3_R, D4_R,
    HIP_L, KNEE_L, HOCK_L, FOOT_L, T0_L, T1_L, T2_L,
    HIP_R, KNEE_R, HOCK_R, FOOT_R, T0_R, T1_R, T2_R,
    HAIRH0,                                  // + k * kHeadHairJ + i (Kopf)
    HAIRB0 = HAIRH0 + kHeadHair * kHeadHairJ,  // + k * kBackHairJ + i (Ruecken)
    JCOUNT = HAIRB0 + kBackHair * kBackHairJ
};
enum B : std::uint8_t {
    B_ROOT, B_PELVIS, B_SPINE1, B_SPINE2, B_NECK1, B_NECK2, B_NECK3, B_HEAD, B_JAW,
    B_SCAP_L, B_UPPER_L, B_FORE_L, B_PALM_L, B_D0_L, B_D1_L, B_D2_L, B_D3_L, B_D4_L,
    B_SCAP_R, B_UPPER_R, B_FORE_R, B_PALM_R, B_D0_R, B_D1_R, B_D2_R, B_D3_R, B_D4_R,
    B_THIGH_L, B_SHIN_L, B_META_L, B_T0_L, B_T1_L, B_T2_L,
    B_THIGH_R, B_SHIN_R, B_META_R, B_T0_R, B_T1_R, B_T2_R,
    B_HAIRH0,                               // + k * 3 + i
    B_HAIRB0 = B_HAIRH0 + kHeadHair * 3,    // + k * 2 + i
    BCOUNT = B_HAIRB0 + kBackHair * 2
};
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
constexpr int kFront = SHOULDER_R - SHOULDER_L, kFrontB = B_SCAP_R - B_SCAP_L, kHind = HIP_R - HIP_L, kHindB = B_THIGH_R - B_THIGH_L;
inline int front(int left, int s) { return left + s * kFront; }
inline int frontB(int left, int s) { return left + s * kFrontB; }
inline int hind(int left, int s) { return left + s * kHind; }
inline int hindB(int left, int s) { return left + s * kHindB; }
inline int hairH(int k, int i) { return HAIRH0 + k * kHeadHairJ + i; }
inline int hairHB(int k, int i) { return B_HAIRH0 + k * 3 + i; }
inline int hairB(int k, int i) { return HAIRB0 + k * kBackHairJ + i; }
inline int hairBB(int k, int i) { return B_HAIRB0 + k * 2 + i; }
}  // namespace hd

// Deathmoth: Brust (Wurzel), Kopf mit Ruessel, Hinterleib in drei Gliedern, zwei Schwanzborsten,
// zwei Fuehler, vier Fluegel (starre Platten), sechs Beine zu je drei Gliedern
namespace mo {
enum J : std::uint8_t {
    ROOT, HEAD, PROB, ABD0, ABD1, ABD2, ABDTIP, BRISTLE_L, BRISTLE_R,
    ANTB_L, ANTT_L, ANTB_R, ANTT_R,
    WF_L, WFT_L, WH_L, WHT_L, WF_R, WFT_R, WH_R, WHT_R,
    LEG0,  // + leg * 4 + i (Bein 0..5: vorn links, vorn rechts, Mitte l, Mitte r, hinten l, hinten r)
    JCOUNT = LEG0 + 6 * 4
};
enum B : std::uint8_t {
    B_ROOT, B_THORAX, B_HEAD, B_ABD1, B_ABD2, B_ABD3, B_BRISTLE_L, B_BRISTLE_R,
    B_ANT_L, B_ANT_R,
    B_WF_L, B_WH_L, B_WF_R, B_WH_R,
    B_LEG0,  // + leg * 3 + i
    BCOUNT = B_LEG0 + 6 * 3
};
inline int legJ(int leg, int i) { return LEG0 + leg * 4 + i; }
inline int legB(int leg, int i) { return B_LEG0 + leg * 3 + i; }
}  // namespace mo

// Gestalt: Saeule mit Kopf und Schultern, unten vier Fasern (je 2 Knochen)
namespace ge {
constexpr int kWisps = 4;
enum J : std::uint8_t {
    ROOT, LOW, MID, CHEST, NECK, HEAD, TOP,
    WISP0,  // + k * 3 + i
    JCOUNT = WISP0 + kWisps * 3
};
enum B : std::uint8_t {
    B_ROOT, B_LOW, B_MID, B_CHEST, B_NECK, B_HEAD,
    B_WISP0,  // + k * 2 + i
    BCOUNT = B_WISP0 + kWisps * 2
};
inline int wisp(int k, int i) { return WISP0 + k * 3 + i; }
inline int wispB(int k, int i) { return B_WISP0 + k * 2 + i; }
}  // namespace ge

// Nest: starr. Ortsrahmen: Ursprung in der Tuermitte am Boden, auf der Wandflaeche des Raums mit
// dem Nest; x zeigt in diesen Raum, y entlang der Wand. Die Oeffnung ist 1 m breit.
namespace ns {
enum J : std::uint8_t { ROOT, TOP, JCOUNT };
enum B : std::uint8_t { B_ROOT, B_BODY, BCOUNT };
constexpr float kOpenW = 1.0f;
}  // namespace ns

}  // namespace lim::anim
