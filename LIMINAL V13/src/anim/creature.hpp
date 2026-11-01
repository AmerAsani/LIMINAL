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
//   Buendel (Entity 5)  V11 (ART_DIRECTION 5.2): eine etwa 1 m grosse Masse ineinander verwachsener
//                       menschlicher Arme und zweier Beine, unterschiedlich lang (der laengste 2,6 m),
//                       darunter ein Maul mit Zaehnen. Zieht sich Hand ueber Hand, das Ganze schleift nach.
//   V13.2 (ENTITY_ABGLEICH.md, nur Wikidot):
//   Faceling (Entity 9) ein Mensch ohne Gesicht (Haar ja, Augen, Nase, Mund, Ohren nein), schwaecher als ein Mensch;
//                       Varianten: gewoehnlich, "entwickelt" (Level 3: Ansaetze von Auge, Nase, Mund), im rosa Kleid
//                       (Level 188). Zweibeiner (bp), 1,70 m.
//   Skin-Stealer (E. 10) gross (2,05 m), blassgelb, eingesunkene weisse Augen, Haut mit Saugnaepfchen; traegt die Haut
//                       eines Menschen. Zweibeiner (bp) mit langen Gliedern.
//   Wretch (Entity 15)  ausgezehrter Mensch: rotbraune, schuppige Haut mit Loechern und Pusteln, offene Augen ohne
//                       Lider, Reste der Kleidung. Zweibeiner (bp), gebeugt.
//   Death Rat (E. 24)   eine Ratte mit langem Koerper, schwarzem Fell, braunen Hoernern mit rot gluehenden Spitzen.
//                       Vierbeiner (rt), langer Schwanz.
//   Fensterwesen (E. 2) ein Fenster: verschwommene Landschaft, darin ein dunkler Umriss, der winkt; greift mit einem Arm
//                       aus dem Fenster (wn). Wurzel: Mitte der Fensteroeffnung auf der Wand, x zeigt aus der Wand.
//   V13.3:
//   Wrangler (E. 75)    riesige, durchscheinende Schlange: ein Abschnitt des Leibs aus starren Ringen, Verschlucktes darin,
//                       Hals und Kopf (Laecheln oder Zangen), Kraenze an den Durchbruchstellen (wg).
//   Growler (E. 130)    gebeugte Masse aus bunten Pfeifenreinigern, Glieder enden in schleifendem Gewirr (gw).
//   Nguithr'xurh (E. 16) Walzenspinne mit 16 Gliedmassen; im selben Skelett die Netzkugeln der Kolonie (ng).
//   Ouroboros-Arbeiter  schattenhafte Gestalten im Overall (Level 1, Sektor Ouroboros); im selben Skelett ihre Werkzeuge,
//                       die Mauer, die sie bauen und einreissen, und ein Fahrgeruest, das umkippt (ob).
//   Plague Goblins (E. 25) katzengrosse Vogel-Katzen-Wesen in Umhang und Pestarztmaske; laufen halb aufrecht, rennen auf allen
//                       vieren; unter der Maske ein Kraehenschnabel voller Saegezaehne; im selben Skelett der Stock der
//                       Torwaechterin, die Beute in der Klaue und das Portal der Bande (gb).
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

enum class Entity : std::uint8_t { Smiler, Hound, Moth, Gestalt, Nest, Clump, Faceling, SkinStealer, Wretch, Rat, Window, Duller, Arms, Wrangler, Growler, Nguithr, Ouroboros, Goblin, Count };
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
    std::vector<float> s;  // V13.3: Massstab je Knochen um sein Startgelenk (leer: alle 1) - Groesse, verborgene Teile
};

const CreatureSkeleton& entitySkeleton(Entity e);
const char* entityModel(Entity e);  // Basisname der Modelle ("ent_smiler"), je Stufe "_l0" ...
const char* entityName(Entity e);   // Name im Spiel
// V13.2: weitere Modelle desselben Wesens (gleiches Skelett): Index 1.. = Variante (0 ist entityModel)
const std::vector<const char*>& entityVariants(Entity e);

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
    LIDH_L, LIDT_L, LIDH_R, LIDT_R,  // V11: Scharnier der Lider je Auge (Achse laengs des Auges)
    WISP0,                           // + k * kWispJ + i
    JCOUNT = WISP0 + kWisps * kWispJ
};
enum B : std::uint8_t {
    B_ROOT, B_LOW, B_MID, B_HEAD, B_FACE, B_JAW,
    B_LIDU_L, B_LIDD_L, B_LIDU_R, B_LIDD_R,  // V11: Ober- und Unterlid je Auge (schliessen von oben und unten)
    B_WISP0,                                 // + k * 3 + i
    BCOUNT = B_WISP0 + kWisps * 3
};
inline int wisp(int k, int i) { return WISP0 + k * kWispJ + i; }
inline int wispB(int k, int i) { return B_WISP0 + k * 3 + i; }
// V11: Kopfmasse (Ellipsoid: Mitte, Radien) - Augen, Lider und Laecheln sitzen auf ihrer Oberflaeche
constexpr float kHeadC[3] = {-0.02f, 0.0f, 1.84f}, kHeadR[3] = {0.218f, 0.37f, 0.28f};
// Winkel der Lidmitte um das Scharnier (rad): geschlossen +-kLidClosed, offen um kLidOpen weiter weggeklappt
constexpr float kLidClosed = 0.2f, kLidOpen = 0.95f, kLidHinge = 0.03f;
// Auge: Mitte auf der Haut, Normale n, Achse t (zeigt nach +y, zum aeusseren Winkel geneigt), u = n x t,
// Scharnier der Lider kLidHinge hinter der Haut; Laenge (halb) und Hoehe (halb) des Schlitzes
struct Eye {
    vec3 surf, n, t, u, hinge;
    float len, r;
};
Eye smilerEye(int side);  // 0 links (+y, etwas groesser), 1 rechts
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

// V11: Buendel - Kern (Masse, unten das Maul), sieben Arme (Oberarm, Unterarm, Hand; Arm 0 ist der
// laengste: Koeder und Griff), zwei Beine (Oberschenkel, Unterschenkel), die nachschleifen
namespace cl {
constexpr int kArms = 7, kLegs = 2;
enum J : std::uint8_t {
    ROOT, CORE, TOP, MOUTH,
    ARM0,                     // + arm * 4 + i: Schulter, Ellbogen, Handgelenk, Fingerspitzen
    LEG0 = ARM0 + kArms * 4,  // + leg * 3 + i: Huefte, Knie, Fuss
    JCOUNT = LEG0 + kLegs * 3
};
enum B : std::uint8_t {
    B_ROOT, B_CORE,
    B_ARM0,                       // + arm * 3 + i: Oberarm, Unterarm, Hand
    B_LEG0 = B_ARM0 + kArms * 3,  // + leg * 2 + i
    BCOUNT = B_LEG0 + kLegs * 2
};
inline int armJ(int a, int i) { return ARM0 + a * 4 + i; }
inline int armB(int a, int i) { return B_ARM0 + a * 3 + i; }
inline int legJ(int l, int i) { return LEG0 + l * 3 + i; }
inline int legB(int l, int i) { return B_LEG0 + l * 2 + i; }
// Arme: Gesamtlaenge (m), Richtung der Schulter am Kern (rad um z, 0 = vorn), Hoehe der Schulter ueber der
// Mitte (m). Die Hand ist bei allen gleich lang (kHand); Ober- und Unterarm teilen sich den Rest.
struct Arm {
    float len, ang, h;
};
constexpr Arm kArm[kArms] = {{2.6f, 0.15f, 0.22f}, {1.3f, 1.15f, 0.12f}, {1.05f, 2.2f, 0.02f}, {1.45f, -1.0f, 0.16f},
                             {0.95f, -2.15f, 0.05f}, {1.2f, 2.85f, 0.26f}, {0.85f, -0.45f, 0.34f}};
constexpr float kHand = 0.19f, kCoreZ = 0.44f, kCoreR = 0.36f, kThigh = 0.46f, kShin = 0.44f;
inline float upperLen(int a) { return (kArm[a].len - kHand) * 0.52f; }
inline float foreLen(int a) { return (kArm[a].len - kHand) * 0.48f; }
vec3 shoulder(int a);  // Schulter in der Ruhepose (Modellraum)
}  // namespace cl

// Nest: starr. Ortsrahmen: Ursprung in der Tuermitte am Boden, auf der Wandflaeche des Raums mit
// dem Nest; x zeigt in diesen Raum, y entlang der Wand. Die Oeffnung ist 1 m breit.
namespace ns {
// V13.2: Neuentwurf (tools/blender/creatures/nest.py) - Haut an der Angel am Sturz, Wucherung im Flur, vier atmende
// Saecke, Gelege. Je Knochen zwei Gelenke (a -> b); die Saecke zeigen mit b nach aussen.
enum J : std::uint8_t {
    ROOT, TOP, MEMB_T, MEMB_B, LIP_A, LIP_B, SAC0_A, SAC0_B, SAC1_A, SAC1_B, SAC2_A, SAC2_B, SAC3_A, SAC3_B, EGG_A, EGG_B, JCOUNT
};
enum B : std::uint8_t { B_ROOT, B_BODY, B_MEMB, B_LIP, B_SAC0, B_SAC1, B_SAC2, B_SAC3, B_EGGS, BCOUNT };
constexpr float kOpenW = 1.0f;
}  // namespace ns

// V13.2: Zweibeiner (Faceling, Skin-Stealer, Wretch) - gleiche Gelenkfolge, eigene Masse je Wesen (bipedBuild)
namespace bp {
enum J : std::uint8_t {
    ROOT, PELVIS, SPINE, CHEST, NECK, HEAD, HEADTOP, JAW,
    SHOULDER_L, ELBOW_L, WRIST_L, HAND_L,
    SHOULDER_R, ELBOW_R, WRIST_R, HAND_R,
    HIP_L, KNEE_L, ANKLE_L, TOE_L,
    HIP_R, KNEE_R, ANKLE_R, TOE_R,
    JCOUNT
};
enum B : std::uint8_t {
    B_ROOT, B_PELVIS, B_SPINE, B_CHEST, B_NECK, B_HEAD, B_JAW,
    B_CLAV_L, B_UPPER_L, B_FORE_L, B_HAND_L,
    B_CLAV_R, B_UPPER_R, B_FORE_R, B_HAND_R,
    B_THIGH_L, B_SHIN_L, B_FOOT_L,
    B_THIGH_R, B_SHIN_R, B_FOOT_R,
    BCOUNT
};
constexpr int kArm = SHOULDER_R - SHOULDER_L, kArmB = B_CLAV_R - B_CLAV_L, kLeg = HIP_R - HIP_L, kLegB = B_THIGH_R - B_THIGH_L;
inline int arm(int left, int s) { return left + s * kArm; }
inline int armB(int left, int s) { return left + s * kArmB; }
inline int leg(int left, int s) { return left + s * kLeg; }
inline int legB(int left, int s) { return left + s * kLegB; }
// Koerperbau (m): Hoehen ueber dem Boden, halbe Breiten, Laengen der Glieder
struct Build {
    float hip, waist, chest, shoulder, neckTop, headTop;  // Hoehen
    float shoulderW, hipW;                                // halbe Breiten
    float upperArm, foreArm, hand, thigh, shin, ankle, foot;
    float armOut;                                         // Abspreizen der Arme in der Ruhepose (rad)
};
const Build& bipedBuild(Entity e);
}  // namespace bp

// V13.2: Death Rat - Rumpf in drei Abschnitten, Kopf mit Kiefer (Hoerner sitzen fest am Kopf), vier Beine zu je drei
// Gliedern, Schwanz in vier Gliedern
namespace rt {
constexpr int kTail = 5;
enum J : std::uint8_t {
    ROOT, PELVIS, SPINE, CHEST, NECK, HEAD, SNOUT, JAW,
    LEG0,                    // + leg * 4 + i (0 vorn links, 1 vorn rechts, 2 hinten links, 3 hinten rechts): oben, Knie, Fuss, Zehen
    TAIL0 = LEG0 + 4 * 4,    // + i
    JCOUNT = TAIL0 + kTail
};
enum B : std::uint8_t {
    B_ROOT, B_PELVIS, B_SPINE, B_NECK, B_HEAD, B_JAW,
    B_LEG0,                  // + leg * 3 + i
    B_TAIL0 = B_LEG0 + 4 * 3,
    BCOUNT = B_TAIL0 + kTail - 1
};
inline int legJ(int l, int i) { return LEG0 + l * 4 + i; }
inline int legB(int l, int i) { return B_LEG0 + l * 3 + i; }
}  // namespace rt

// V13.2: Fensterwesen - Hintergrund (Landschaft) und Umriss in der Fensteroeffnung (1,2 x 1,8 m, Mitte = Wurzel),
// der Umriss mit Rumpf, Kopf und zwei Armen (links winkt, rechts greift aus dem Fenster)
namespace wn {
enum J : std::uint8_t {
    ROOT, BACK, FIG_BASE, FIG_CHEST, FIG_NECK, FIG_TOP,
    SH_L, EL_L, WR_L, HAND_L, SH_R, EL_R, WR_R, HAND_R,
    JCOUNT
};
enum B : std::uint8_t { B_ROOT, B_BACK, B_BODY, B_CHEST, B_HEAD, B_UPPER_L, B_FORE_L, B_HAND_L, B_UPPER_R, B_FORE_R, B_HAND_R, BCOUNT };
constexpr float kW = 1.2f, kH = 1.8f, kFigX = 0.016f;  // Oeffnung; Ebene des Umrisses (x)
}  // namespace wn

// V13.3: Duller (Entity 6, KREATUR_Duller.md) - gross, duerr, ohne Gesicht. Der Unterarm besteht aus kSeg ineinander
// steckenden Gliedern (Glied 0 ist der Unterarm, die inneren je duenner): eingefahren liegen sie im Unterarm, ausgefahren
// reicht der Arm gut 3 m weit (durch eine Wand). Dazu die Bruchstelle in der Wand (sie gehoert zum Wesen): Risse, Rand mit
// Loch und fuenf Brocken, die zusammen die Woelbung bilden. Code: anim/creature_duller.cpp
namespace du {
constexpr int kSeg = 7, kFing = 5, kChunks = 5;
// Gelenke: Rumpf, je Arm (Schulter, Ellbogen, Anfang und Ende je Glied, Knoechel, Finger zu je 3), Beine, Bruchstelle
constexpr int kArmJ = 2 + 2 * kSeg + 1 + kFing * 3;
constexpr int ROOT = 0, PELVIS = 1, SPINE = 2, CHEST = 3, NECK = 4, HEAD = 5, HEADTOP = 6, ARM0 = 7;
constexpr int LEG0 = ARM0 + 2 * kArmJ;  // + s * 4: Huefte, Knie, Knoechel, Zehen
constexpr int CRACK_A = LEG0 + 8, CRACK_B = CRACK_A + 1, RIM_A = CRACK_A + 2, RIM_B = CRACK_A + 3, CHUNK0 = CRACK_A + 4;  // + k * 2 + i
constexpr int JCOUNT = CHUNK0 + kChunks * 2;
inline int sh(int s) { return ARM0 + s * kArmJ; }
inline int el(int s) { return sh(s) + 1; }
inline int segS(int s, int k) { return sh(s) + 2 + k; }
inline int segE(int s, int k) { return sh(s) + 2 + kSeg + k; }
inline int kn(int s) { return sh(s) + 2 + 2 * kSeg; }
inline int fin(int s, int f, int i) { return kn(s) + 1 + f * 3 + i; }
inline int leg(int s, int i) { return LEG0 + s * 4 + i; }
// Knochen: Rumpf, je Arm (Schluesselbein, Oberarm, Glieder, Hand, Finger zu je 2), Beine (je 3), Risse, Rand, Brocken
constexpr int kArmB = 2 + kSeg + 1 + kFing * 2;
constexpr int B_ROOT = 0, B_PELVIS = 1, B_SPINE = 2, B_CHEST = 3, B_NECK = 4, B_HEAD = 5, B_ARM0 = 6;
constexpr int B_LEG0 = B_ARM0 + 2 * kArmB;  // + s * 3
constexpr int B_CRACK = B_LEG0 + 6, B_RIM = B_CRACK + 1, B_CHUNK0 = B_CRACK + 2, BCOUNT = B_CHUNK0 + kChunks;
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
inline int bClav(int s) { return B_ARM0 + s * kArmB; }
inline int bUpper(int s) { return bClav(s) + 1; }
inline int bSeg(int s, int k) { return bClav(s) + 2 + k; }
inline int bHand(int s) { return bClav(s) + 2 + kSeg; }
inline int bFin(int s, int f, int i) { return bHand(s) + 1 + f * 2 + i; }
inline int bLeg(int s, int i) { return B_LEG0 + s * 3 + i; }
// Masse (m): Koerper; Glied k ist kSegLen - k * kSegShrink lang und steckt kSegOverlap im vorigen; Bruchstelle der Ruhepose
constexpr float kHip = 1.10f, kWaist = 1.29f, kChest = 1.52f, kShoulder = 1.80f, kNeckTop = 1.93f, kHeadTop = 2.19f;
constexpr float kShoulderW = 0.165f, kHipW = 0.095f, kUpper = 0.44f, kSegLen = 0.47f, kSegShrink = 0.01f, kSegOverlap = 0.07f;
constexpr float kHand = 0.13f, kThigh = 0.50f, kShin = 0.49f, kAnkle = 0.06f, kFoot = 0.21f, kArmOut = 0.45f;
constexpr float kBreachX = 1.5f, kBreachZ = 1.45f, kBreachH = 1.45f;  // Ruhepose: Mitte der Bruchstelle; Hoehe ueber dem Boden
inline float segLen(int k) { return kSegLen - kSegShrink * (float)k; }
float extendedLen();  // Unterarm ganz ausgefahren (Ellbogen bis Handgelenk)
CreatureSkeleton skeleton();
}  // namespace du

// V13.3: Arms / Les Bras (Entite 15-FR, KREATUR_Arms.md) - ein langer, verdrehter, verwesender Arm mit drei Krallen, der
// in Lueftungsschaechten lebt. Wurzel: Mitte der Oeffnung an der Decke; die Kette (kSeg Glieder) laeuft im Schacht und
// gleitet durch die Oeffnung heraus. Dazu: Riss in der Decke (falls keine Lueftung da ist), verbrannte Hautfetzen, Rauch.
// Code: anim/creature_arms.cpp
namespace ar {
constexpr int kSeg = 14, kClaws = 3, kShreds = 3, kSmoke = 4;
constexpr int ROOT = 0, C0 = 1;                       // Kette: C0 (tief im Schacht) .. C0 + kSeg (Handgelenk)
constexpr int WRIST = C0 + kSeg, KN = WRIST + 1;      // Hand: Handgelenk bis Knoechel
constexpr int CLAW0 = KN + 1;                         // + c * 4 + i: Ansatz, Mitte, Kralle, Spitze
constexpr int CRACK_A = CLAW0 + kClaws * 4, CRACK_B = CRACK_A + 1;
constexpr int SHRED0 = CRACK_A + 2;                   // + k * 2 + i
constexpr int SMOKE0 = SHRED0 + kShreds * 2;          // + k * 2 + i
constexpr int JCOUNT = SMOKE0 + kSmoke * 2;
constexpr int B_ROOT = 0, B_SEG0 = 1, B_HAND = B_SEG0 + kSeg, B_CLAW0 = B_HAND + 1;  // + c * 3 + i
constexpr int B_CRACK = B_CLAW0 + kClaws * 3, B_SHRED0 = B_CRACK + 1, B_SMOKE0 = B_SHRED0 + kShreds, BCOUNT = B_SMOKE0 + kSmoke;
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
inline int claw(int c, int i) { return CLAW0 + c * 4 + i; }
inline int bClaw(int c, int i) { return B_CLAW0 + c * 3 + i; }
constexpr float kSegLen = 0.3f, kTop = 0.35f;  // Gliedlaenge; Ruhepose: C0 so hoch ueber der Wurzel, die Kette haengt senkrecht
constexpr float kHand = 0.17f, kOutMax = 3.0f;  // Handgelenk bis Knoechel; so weit (m) gleitet der Arm hoechstens heraus
CreatureSkeleton skeleton();
}  // namespace ar
// V13.3: Wrangler (Entity 75, KREATUR_Wrangler.md) - eine riesige, durchscheinende Schlange, von der man nie das Ganze sieht.
// Ein Abschnitt des Leibs zieht als Kette starrer Ringe vorbei (je Ring ein Knochen; jeder laeuft fuer sich die Bahn entlang
// und springt tief im Boden an ihren Anfang zurueck), darin Verschlucktes (eigene Knochen, die nur einmal vorbeiziehen); der
// Hals mit dem Kopf bricht aus Wand oder Boden (Maennchen: Gesicht mit breitem Laecheln und Unterkiefer, Weibchen: zwei
// Zangen); Kraenze aus Schutt und Schleim, wo der Leib eine Flaeche durchstoesst. Ruhepose: alles auf einer Geraden entlang
// x in 1 m Hoehe (Ringe -8,25..8,25 m, Hals und Kopf ab 9,5 m), die Kraenze flach am Boden. Wurzel: Mitte der Bahn am Boden.
namespace wg {
constexpr int kRings = 30, kItems = 4, kNeck = 6, kCollars = 3;
constexpr float kRingLen = 0.55f, kRingR = 0.74f, kNeckLen = 0.5f, kAxisZ = 1.0f;
enum J : std::uint8_t {
    ROOT,
    RING0,                           // + 2 * r: hinten, + 2 * r + 1: vorn
    ITEM0 = RING0 + 2 * kRings,      // + 2 * k
    NECK0 = ITEM0 + 2 * kItems,      // .. NECK0 + kNeck (= Schaedelbasis)
    SNOUT = NECK0 + kNeck + 1,
    JAWH, JAWTIP, PINC_L0, PINC_L1, PINC_R0, PINC_R1,
    COL0,                            // + 2 * c (0 Eintritt, 1 Austritt, 2 Kopf): Mitte auf der Flaeche, + 1: Normale
    JCOUNT = COL0 + 2 * kCollars
};
enum B : std::uint8_t {
    B_ROOT,
    B_RING0,                         // + r
    B_ITEM0 = B_RING0 + kRings,      // + k
    B_NECK0 = B_ITEM0 + kItems,      // + i
    B_SKULL = B_NECK0 + kNeck,
    B_JAW, B_PINC_L, B_PINC_R,
    B_COL0,                          // + c
    BCOUNT = B_COL0 + kCollars
};
static_assert(BCOUNT <= 64, "hoechstens 64 Knochen");
inline int ringA(int r) { return RING0 + 2 * r; }
inline int itemA(int k) { return ITEM0 + 2 * k; }
inline int colA(int c) { return COL0 + 2 * c; }
CreatureSkeleton makeWrangler();  // creature_wrangler.cpp
}  // namespace wg
// V13.3: Growler (Entity 130, KREATUR_Growler.md) - grob menschenaehnliche, vornuebergebeugte Masse aus bunten
// Pfeifenreinigern ohne Kopf: der "Kopf" ist ein Knoten auf der Brust, aus dem die Kabel auseinanderlaufen. Kurze Beine,
// lange Arme; alle Glieder enden in Gewirr, das nachschleift. Brustklappen (oeffnen sich zum Aufnehmen der Beute),
// sechs Buendel rippenartiger Drahtspitzen (fahren beim Angriff aus), drei Peitschenranken am Ruecken.
// Masse eines ausgewachsenen Tiers (2,1 m gebeugt); Junge und Verschmolzene ueber den Massstab je Knochen.
namespace gw {
constexpr int kSpikes = 6, kArmJ = 7, kLegJ = 6, kTend = 3, kTendJ = 5;
enum J : std::uint8_t {
    ROOT, PELVIS, SPINE, CHEST, HUMP, KNOT,
    DOORH_L, DOORT_L, DOORH_R, DOORT_R,
    SPK0,                          // + k * 2 + (0 Ansatz, 1 Spitze)
    ARM0 = SPK0 + kSpikes * 2,     // + Seite * kArmJ + i: Schulter, Ellbogen, Handgelenk, Gewirr 1..4
    LEG0 = ARM0 + 2 * kArmJ,       // + Seite * kLegJ + i: Huefte, Knie, Knoechel, Zehen, Gewirr 1, 2
    TEND0 = LEG0 + 2 * kLegJ,      // + k * kTendJ + i
    JCOUNT = TEND0 + kTend * kTendJ
};
enum B : std::uint8_t {
    B_ROOT, B_PELVIS, B_SPINE, B_HUMP, B_KNOT, B_DOOR_L, B_DOOR_R,
    B_SPK0,                         // + k
    B_ARM0 = B_SPK0 + kSpikes,      // + Seite * 6 + i: Oberarm, Unterarm, Gewirr 1..4
    B_LEG0 = B_ARM0 + 12,           // + Seite * 5 + i: Oberschenkel, Unterschenkel, Fuss, Gewirr 1, 2
    B_TEND0 = B_LEG0 + 10,          // + k * 4 + i
    BCOUNT = B_TEND0 + kTend * 4
};
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
inline int armJ(int s, int i) { return ARM0 + s * kArmJ + i; }
inline int armB(int s, int i) { return B_ARM0 + s * 6 + i; }
inline int legJ(int s, int i) { return LEG0 + s * kLegJ + i; }
inline int legB(int s, int i) { return B_LEG0 + s * 5 + i; }
inline int tendJ(int k, int i) { return TEND0 + k * kTendJ + i; }
inline int tendB(int k, int i) { return B_TEND0 + k * 4 + i; }
}  // namespace gw

// V13.3: Nguithr'xurh (Entity 16, KREATUR_Nguithrxurh.md) - Walzenspinne mit 16 Gliedmassen (12 Laufbeine, 2 Pedipalpen,
// 2 Cheliceren), im Mittel 18 cm lang (Massstab je Ebene). Dazu im selben Skelett die Netzkugeln einer Kolonie (Variante
// "web"): je Kugel ein Knochen vom Anker an der Decke bis zur Mitte (Faden und Kugel starr) und eine Pfuetze am Boden
// (zerplatzt). Wurzel: Spinne - unter dem Koerper auf der Flaeche; Netz - Mitte der Kolonie an der Decke (z nach unten
// haengen die Kugeln, die Pfuetzen liegen in der Ruhepose kHang tiefer).
namespace ng {
constexpr int kLegs = 12, kSacs = 6;
constexpr float kHang = 2.6f;  // Deckenhoehe der Ruhepose (Pfuetzen)
enum J : std::uint8_t {
    ROOT, PRO, EYES, OPI0, OPI1, OPI2,
    CHB_L, CHT_L, CHB_R, CHT_R,                // Cheliceren
    PPB_L, PPK_L, PPT_L, PPB_R, PPK_R, PPT_R,  // Pedipalpen
    LEG0,                                      // + Bein * 4 + i: Huefte, Knie, Ferse, Klaue (Bein = Paar * 2 + Seite)
    DRAG_A = LEG0 + kLegs * 4, DRAG_B,         // Faden aus den Spinnwarzen
    SAC0,                                      // + k * 4 + i: Anker, Kugelmitte, Pfuetze a, Pfuetze b
    JCOUNT = SAC0 + kSacs * 4
};
enum B : std::uint8_t {
    B_ROOT, B_PRO, B_OPI0, B_OPI1, B_CH_L, B_CH_R, B_PP0_L, B_PP1_L, B_PP0_R, B_PP1_R,
    B_LEG0,                     // + Bein * 3 + i
    B_DRAG = B_LEG0 + kLegs * 3,
    B_SAC0,                     // + k * 2: Kugel (mit Faden), Pfuetze
    BCOUNT = B_SAC0 + kSacs * 2
};
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
inline int legJ(int l, int i) { return LEG0 + l * 4 + i; }
inline int legB(int l, int i) { return B_LEG0 + l * 3 + i; }
inline int sacJ(int k, int i) { return SAC0 + k * 4 + i; }
inline int sacB(int k) { return B_SAC0 + k * 2; }
inline int splatB(int k) { return B_SAC0 + k * 2 + 1; }
// Kugeln der Ruhepose: Lage des Ankers (x, y), Fadenlaenge, Radius der Kugel
struct Sac {
    float x, y, len, r;
};
constexpr Sac kSac[kSacs] = {{0.05f, 0.02f, 0.62f, 0.13f}, {0.42f, 0.18f, 0.44f, 0.11f}, {-0.30f, 0.36f, 0.72f, 0.12f},
                             {-0.36f, -0.28f, 0.38f, 0.10f}, {0.30f, -0.40f, 0.56f, 0.12f}, {-0.02f, -0.62f, 0.30f, 0.09f}};
}  // namespace ng
CreatureSkeleton makeGrowlerSkeleton();  // creature_growler.cpp
CreatureSkeleton makeNguithrSkeleton();  // creature_nguithrxurh.cpp

// V13.3: Ouroboros-Arbeiter (Wikidot level-1 Sektor Ouroboros, backrooms-remodeling-co; KREATUR_OuroborosArbeiter.md) -
// schattenhafte Arbeiter im Overall, 2,05 m, ueberlange Arme mit einem zweiten Ellbogen, der Kopf haengt vor der Brust.
// Varianten im selben Skelett: 0 Geselle (schmal), 1 Brecher (schwer, geht auf den Knoecheln), 2 die Mauer (sieben Lagen,
// sechs lose Steine der Lage im Bau, vier Brocken; Absperrband am Boden), 3 das Fahrgeruest (kippt um die Kante bei +x).
// Werkzeuge (Hammer, Kelle, Stein, Brenner, Presslufthammer, Brechstange) sind Knochen, die in der Hand liegen oder auf
// Massstab 0 verschwinden; Rauchfahnen und Funken ebenso. Wurzel: Arbeiter am Boden zwischen den Fuessen; Mauer: Mitte der
// Mauer am Boden (sie laeuft entlang y); Geruest: Mitte der Standflaeche.
namespace ob {
constexpr int kTools = 6, kWisps = 4, kSparks = 6, kCourses = 7, kLay = 6, kDebris = 4;
constexpr float kBlockL = 0.375f, kCourseH = 0.25f, kWallT = 0.24f, kWallL = kBlockL * kLay;  // Mauer: 2,25 x 1,75 m
constexpr float kTowerH = 3.4f, kTowerD = 0.9f, kTowerW = 1.6f;                               // Geruest
enum Tool : std::uint8_t { T_HAMMER, T_TROWEL, T_BRICK, T_TORCH, T_JACK, T_BAR };
enum J : std::uint8_t {
    ROOT, PELVIS, BELLY, CHEST, NECK, HEAD, HEADTOP,
    ARM0,                          // + Seite * 5 + i: Schulter, Ellbogen 1, Ellbogen 2, Handgelenk, Fingerspitzen
    LEG0 = ARM0 + 10,              // + Seite * 4 + i: Huefte, Knie, Knoechel, Zehen
    TOOL0 = LEG0 + 8,              // + t * 2 + (0 Griff/Spitze, 1 Ende)
    WISP0 = TOOL0 + kTools * 2,    // + k * 3 + i
    SPARK0 = WISP0 + kWisps * 3,   // + k * 2 + i
    COURSE0 = SPARK0 + kSparks * 2,  // + c * 2 + i: Lage c (linkes, rechtes Ende unten, Mitte der Mauerdicke)
    LAY0 = COURSE0 + kCourses * 2,   // + k * 2 + i: loser Stein (Mitte unten, Mitte oben)
    DEB0 = LAY0 + kLay * 2,          // + k * 2 + i: Brocken
    TOWER_A = DEB0 + kDebris * 2, TOWER_B,  // Geruest: Kippkante (Mitte), oben
    JCOUNT
};
enum B : std::uint8_t {
    B_ROOT, B_PELVIS, B_BELLY, B_CHEST, B_NECK, B_HEAD,
    B_ARM0,                        // + Seite * 4 + i: Oberarm, Unterarm 1, Unterarm 2, Hand
    B_LEG0 = B_ARM0 + 8,           // + Seite * 3 + i: Oberschenkel, Unterschenkel, Fuss
    B_TOOL0 = B_LEG0 + 6,          // + t
    B_WISP0 = B_TOOL0 + kTools,    // + k * 2 + i
    B_SPARK0 = B_WISP0 + kWisps * 2,
    B_COURSE0 = B_SPARK0 + kSparks,
    B_LAY0 = B_COURSE0 + kCourses,
    B_DEB0 = B_LAY0 + kLay,
    B_TOWER = B_DEB0 + kDebris,
    BCOUNT
};
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
inline int armJ(int s, int i) { return ARM0 + s * 5 + i; }
inline int armB(int s, int i) { return B_ARM0 + s * 4 + i; }
inline int legJ(int s, int i) { return LEG0 + s * 4 + i; }
inline int legB(int s, int i) { return B_LEG0 + s * 3 + i; }
inline int toolJ(int t, int i) { return TOOL0 + t * 2 + i; }
inline int wispJ(int k, int i) { return WISP0 + k * 3 + i; }
inline int wispB(int k, int i) { return B_WISP0 + k * 2 + i; }
inline int sparkJ(int k, int i) { return SPARK0 + k * 2 + i; }
inline int courseJ(int c, int i) { return COURSE0 + c * 2 + i; }
inline int layJ(int k, int i) { return LAY0 + k * 2 + i; }
inline int debJ(int k, int i) { return DEB0 + k * 2 + i; }
// Aufgaben der Arbeiter (Zustand; 9 und 10 vergibt die Praesenz anders: Abgang, toedlicher Zugriff)
enum Task : int { K_IDLE = 0, K_WALK = 1, K_CARRY = 2, K_LAY = 3, K_HAMMER = 4, K_WELD = 5, K_JACK = 6, K_PRY = 7, K_INSPECT = 8,
                  K_LIFT = 11, K_DISSOLVE = 12, K_EMERGE = 13 };
CreatureSkeleton makeSkeleton();  // creature_ouroboros.cpp
}  // namespace ob

// V13.3: Plague Goblins (Wikidot entity-25, Rev. 28; KREATUR_PlagueGoblins.md) - katzengross (Henchman aufrecht 0,58 m bis
// zum Scheitel), halb zweibeinig: Laufbeine wie ein Vogel (Ferse hoch), lange duenne Arme mit Krallen, zwei lange Ohren mit
// Pinseln, ein gefiederter Schwanz, der unter dem Umhang hervorsieht. Die Maske sitzt an einem Scharnier an der Stirn (klappt
// hoch); darunter Kraehenschnabel mit Unterkiefer. Der Umhang haengt an sechs Straengen (je zwei Knochen). Varianten im selben
// Skelett: 0 Henchman (glatte helle Maske), 1 Spaeher (dunkles Leder, Glasaugen), 2 Torwaechterin (Hut, Cape, verzierte Maske,
// Silberstock mit Sanduhr), 3 das Portal (aufrechtes Oval ueber dem Boden mit sechs Sandstroemen). Wurzel: zwischen den Fuessen
// am Boden; Portal: Mitte unten.
namespace gb {
constexpr int kStrands = 6, kSwirls = 6;
constexpr float kPortalH = 1.25f, kPortalW = 0.9f;
enum J : std::uint8_t {
    ROOT, PELVIS, SPINE, CHEST, NECK, HEAD, SNOUT, CROWN, JAWP, JAWT, MASKP, MASKT,
    EAR0,                          // + Seite * 3 + i: Ansatz, Mitte, Spitze
    ARM0 = EAR0 + 6,               // + Seite * 4 + i: Schulter, Ellbogen, Handgelenk, Krallenspitze
    LEG0 = ARM0 + 8,               // + Seite * 5 + i: Huefte, Knie, Ferse, Ballen, Zehenspitze
    TAIL0 = LEG0 + 10,             // + i (4)
    CLOAK0 = TAIL0 + 4,            // + k * 3 + i: Ansatz an den Schultern, Mitte, Saum
    CANE_A = CLOAK0 + kStrands * 3, CANE_B,  // Stock: Spitze am Boden, Sanduhr oben
    ITEM_A, ITEM_B,                // Beute in der linken Klaue (unten, oben)
    PORT_A, PORT_B,                // Portal: Mitte unten, Mitte oben
    SWIRL0,                        // + k * 2 + i: Sandstrom (Anfang, Ende)
    JCOUNT = SWIRL0 + kSwirls * 2
};
enum B : std::uint8_t {
    B_ROOT, B_PELVIS, B_SPINE, B_CHEST, B_NECK, B_HEAD, B_JAW, B_MASK,
    B_EAR0,                        // + Seite * 2 + i
    B_ARM0 = B_EAR0 + 4,           // + Seite * 3 + i: Oberarm, Unterarm, Hand
    B_LEG0 = B_ARM0 + 6,           // + Seite * 4 + i: Oberschenkel, Unterschenkel, Lauf, Zehen
    B_TAIL0 = B_LEG0 + 8,          // + i (3)
    B_CLOAK0 = B_TAIL0 + 3,        // + k * 2 + i
    B_CANE = B_CLOAK0 + kStrands * 2,
    B_ITEM,
    B_PORTAL,
    B_SWIRL0,                      // + k
    BCOUNT = B_SWIRL0 + kSwirls
};
static_assert(BCOUNT <= 64, "Gewichtsmasken fassen 64 Knochen");
inline int earJ(int s, int i) { return EAR0 + s * 3 + i; }
inline int earB(int s, int i) { return B_EAR0 + s * 2 + i; }
inline int armJ(int s, int i) { return ARM0 + s * 4 + i; }
inline int armB(int s, int i) { return B_ARM0 + s * 3 + i; }
inline int legJ(int s, int i) { return LEG0 + s * 5 + i; }
inline int legB(int s, int i) { return B_LEG0 + s * 4 + i; }
inline int cloakJ(int k, int i) { return CLOAK0 + k * 3 + i; }
inline int cloakB(int k, int i) { return B_CLOAK0 + k * 2 + i; }
inline int swirlJ(int k, int i) { return SWIRL0 + k * 2 + i; }
// Winkel der Umhangstraenge (rad, 0 = vorn): vorn offen, damit die Arme herauskommen
constexpr float kStrandAng[kStrands] = {0.62f, 1.62f, 2.62f, -2.62f, -1.62f, -0.62f};
// Zustaende (Presence, Animation): 0 Ruhe/Suchen, 1 Gehen/Schleichen, 2 Huschen (auf allen vieren), 3 Spaehen, 4 Wuehlen
// (Diebstahl), 5 mit Beute fort, 6 Drohen (Maske hoch, Umhang gespreizt), 7 Angriff (Sprung), 8 aus dem Portal, 11 tot,
// 12 ins Portal, 13 Handel (bietet an), 14 noch im Portal (unsichtbar); Portal: 0 zu, 1 oeffnet, 2 offen, 3 schliesst
enum State : int { S_IDLE = 0, S_WALK = 1, S_SCURRY = 2, S_PEEK = 3, S_RUMMAGE = 4, S_LOOT = 5, S_THREAT = 6, S_ATTACK = 7,
                   S_EMERGE = 8, S_DEAD = 11, S_ENTER = 12, S_TRADE = 13, S_INSIDE = 14 };
CreatureSkeleton makeSkeleton();  // creature_goblin.cpp
}  // namespace gb

}  // namespace lim::anim
