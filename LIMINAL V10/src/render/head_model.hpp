// V10: Der Kopf als radiales Hoehenfeld.
//
// Vom Kopfmittelpunkt (Augenhoehe, mittig zwischen Stirn und Hinterkopf) aus hat jede Richtung
// einen Radius. Er entsteht aus drei Profilen (Gesicht von der Seite gesehen, Seite, Hinterkopf),
// die ueber den Winkel ineinander uebergehen, plus anatomischen Merkmalen als weiche Erhebungen
// und Vertiefungen: Brauenbogen, Augenhoehlen, Nasenruecken und -spitze, Nasenfluegel, Lippen,
// Mundspalte, Kinnfurche, Kinn, Jochbeine, Wangen, Nasolabialfalten, Schlaefen. So bleibt das
// Profil (Stirn - Nase - Lippen - Kinn), das ein Gesicht ausmacht, unter voller Kontrolle.
// Masse in Metern fuer einen Norm-Kopf (1,78 m Koerpergroesse); der Aufrufer skaliert.
#pragma once

#include "core/math.hpp"

namespace lim::gfx {

struct HeadParams {
    bool features = true;  // false: glattes Gesicht (Der Andere)
    bool female = false;
    float eyeDeep = 1.0f;  // Tiefe der Augenhoehlen
    float faceLong = 1.0f; // Laenge des Untergesichts
    float cheeks = 0.0f;   // -1 eingefallen .. 1 voll
    float thin = 0.0f;     // ausgemergelt (Schlaefen, Wangen)
    float jawW = 1.0f;
    float nose = 1.0f;     // Nasengroesse
    float lips = 1.0f;
    float brow = 1.0f;
    float eyeOpen = 1.0f;  // Lidspalte: <1 schwere Lider, >1 aufgerissen (Lederhaut ueber der Iris sichtbar)
    bool openEyes = true;  // false (grobe Stufen): Lider geschlossen, das Auge nur als Schatten (Faerbung)
};

// Abstand (m, negativ innen) eines Punktes in Kopfkoordinaten (x vorn, y links, z oben)
float headDistance(vec3 q, const HeadParams& hp);
// Mittelpunkt des Augapfels (Seite 0: +y) und Radius, in Kopfkoordinaten
vec3 headEyeCenter(int side, const HeadParams& hp);
float headEyeRadius();
// Kopfmittelpunkt relativ zur Schaedelbasis (anim::J_HEAD) in Norm-Massen (x vorn, z oben)
vec3 headOriginFromAtlas();

}  // namespace lim::gfx
