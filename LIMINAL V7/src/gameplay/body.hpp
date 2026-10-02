// V6: Der eigene Koerper - ein absichtlich simples, etwas albernes Strichmaennchen.
//
// Alles ist prozedural animiert und haengt direkt am Spielerzustand:
//   Gehen     Beine im Gangzyklus der Schritte (Fuss setzt genau beim Schrittgeraeusch
//             auf), Arme schwingen gegengleich, Becken wippt mit
//   Sprinten  weite Schritte, angewinkelte "Pump"-Arme, Oberkoerper nach vorne
//   Springen  Arme fliegen hoch, Knie ziehen an
//   Fallen    Arme rudern, Beine strampeln (je laenger, desto wilder)
//   Landen    Federn in den Knien (die Kamera taucht mit ein)
//   Gesten    Item aufnehmen: Hand greift zum Item; Item benutzen: Hand fuehrt es zum Mund;
//             Item ablegen: kurzer Wurf aus dem Handgelenk
//
// Die Kamera sitzt im Kopf: Die Augenposition ergibt sich aus Becken, Wirbelsaeule und
// einem Nackengelenk, das sich mit dem Blick neigt. Schaut man nach unten, wandert das
// Auge nach vorne - man sieht Brust, Beine und Fuesse statt ins eigene Innere.
//
// Die Darstellung kennt nur Modellnamen (Netze registriert der Renderer) - das Gameplay
// bleibt unabhaengig von der Grafik.
#pragma once

#include <string>
#include <vector>

#include "core/math.hpp"
#include "gameplay/player.hpp"

namespace lim::game {

struct VisibleMesh;

struct BodyPose {
    vec3 pelvis, neck, head, chest;
    vec3 shoulder[2], elbow[2], hand[2];  // 0 links, 1 rechts
    vec3 hip[2], knee[2], ankle[2], toe[2];
    vec3 fwd, right, up;                  // Koerperachsen (Blickrichtung waagrecht)
    vec3 headFwd, headUp;                 // Kopfachsen (mit Neigung)
};

class Body {
public:
    enum class Gesture { None, Grab, Use, Toss };

    // Nach Player::update aufrufen (pausiert: dt = 0)
    void update(double dt, const Player& p);
    // Haende: Item aufnehmen (zum Ziel greifen) bzw. benutzen (zum Mund fuehren).
    // holdOffset: Griffhoehe am Modell (m), z. B. Flaschenmitte statt Flaschenboden
    void gesture(Gesture g, vec3 target, const std::string& itemModel, float holdOffset = 0.0f, float scale = 1.0f);
    // Netze fuer den Renderer. firstPerson: ohne Kopf (die Kamera sitzt darin)
    void collect(std::vector<VisibleMesh>& out, bool firstPerson);

    const BodyPose& pose() const { return pose_; }
    vec3 eye() const { return eye_; }           // Kameraposition (Ego-Perspektive)
    float roll() const { return roll_; }        // leichtes Neigen beim Seitwaertsgehen
    float fovKick() const { return fovKick_; }  // Sichtfeld-Zuschlag beim Sprinten (Anteil)
    // Wurzel (Fussmitte, Koerperdrehung): fuer die TAA-Rueckprojektion mitbewegter Pixel
    mat4 rootMatrix() const;
    bool gestureActive() const { return gesture_ != Gesture::None; }
    // V7: Taschenlampe in der linken Hand (lit: Glas leuchtet)
    void setFlashlight(bool held, bool lit) { flashWant_ = held, flashLit_ = lit; }
    vec3 flashlightPos() const { return flashPos_; }  // Lampenkopf (Lichtquelle)
    vec3 flashlightDir() const { return flashDir_; }
    bool flashlightHeld() const { return flashHold_ > 0.5; }
    void reset() { initialized_ = false; }
    // V7: "Der Andere" - gleiche Figur, aber gestreckt, ohne Gesicht, nicht an die Kamera gebunden
    void setGhost() {
        ghost_ = true;
        mLimb_ = "ghost_limb", mJoint_ = "ghost_joint", mHead_ = "ghost_head";
        idLimb_ = idJoint_ = idHead_ = -2;
    }

private:
    void solveLeg(int i, vec3 target, const vec3& kneeDir);
    void solveArm(int i, vec3 target, const vec3& pole);

    BodyPose pose_{};
    vec3 root_;
    double yaw_ = 0;
    bool initialized_ = false;
    double time_ = 0, breathPhase_ = 0;
    double lean_ = 0, side_ = 0, sprint_ = 0, air_ = 0, rise_ = 0, fall_ = 0;
    double squash_ = 0, squashV_ = 0;
    double flail_ = 0;
    vec3 eye_;
    float roll_ = 0, fovKick_ = 0;
    // Gesten
    Gesture gesture_ = Gesture::None;
    double gestureT_ = 0;
    vec3 gestureTarget_;
    std::string heldModel_;
    float heldOffset_ = 0.0f, heldScale_ = 1.0f;
    bool showHeld_ = false;
    double spd_ = 0;              // geglaettete Bodengeschwindigkeit
    double dirF_ = 1, dirR_ = 0;  // Bewegungsrichtung im Koerperraum
    double rollS_ = 0;
    vec3 heldPos_;
    mat4 heldRot_ = mat4::identity();
    // Modellnamen und vom Renderer aufgeloeste Netz-IDs (stabile Zeiger fuer VisibleMesh)
    std::string mLimb_ = "stick_limb", mJoint_ = "stick_joint", mHead_ = "stick_head", mEye_ = "stick_eye",
                mSmile_ = "stick_smile";
    int idLimb_ = -2, idJoint_ = -2, idHead_ = -2, idEye_ = -2, idSmile_ = -2, idHeld_ = -2;
    std::string heldResolved_;
    // V7: Taschenlampe
    bool flashWant_ = false, flashLit_ = false;
    double flashHold_ = 0.0;
    vec3 flashPos_, flashDir_{1, 0, 0}, flashGrip_, flashUp_{0, 0, 1};
    std::string mFlash_ = "flashlight", mFlashLit_ = "flashlight_on";
    int idFlash_ = -2, idFlashLit_ = -2;
    bool ghost_ = false;
};

}  // namespace lim::game
