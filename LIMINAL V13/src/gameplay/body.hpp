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

// V10: Proportionen (Meter). Voreinstellung: das Strichmaennchen aus V6 (Augenhoehe = Player::kEye);
// der realistische Koerper uebernimmt die Masse seines Koerperbaus (anim::Build).
struct BodyProportions {
    float hip = 0.94f;     // Beckenhoehe im Stand
    float spine = 0.55f;   // Becken -> Halsansatz
    float neck = 0.06f;
    float headR = 0.11f;   // runder Kopf
    float upperArm = 0.30f, forearm = 0.28f;
    float thigh = 0.47f, shin = 0.45f;
    float hipW = 0.075f, shoulderW = 0.11f, footW = 0.11f;
    float ankle = 0.055f;
    float eyeUp = 0.13f, eyeFwd = 0.07f;  // Auge relativ zum Halsansatz
    float gripFwd = 0.0f;  // Taschenlampe liegt so weit vor dem Handgelenk in der Hand
    float gripOut = 0.0f;  // V11: Mitte der Lampe vor der Handflaeche (> 0: Faustgriff, realistischer Koerper)
};

struct BodyPose {
    vec3 pelvis, neck, head, chest;
    vec3 shoulder[2], elbow[2], hand[2];  // 0 links, 1 rechts
    vec3 hip[2], knee[2], ankle[2], toe[2];
    vec3 fwd, right, up;                  // Koerperachsen (Blickrichtung waagrecht)
    vec3 headFwd, headUp;                 // Kopfachsen (mit Neigung)
};

class Body {
public:
    enum class Gesture { None, Grab, Use, Toss, Swing, Throw };  // V13.3: Swing - das Beil (ausholen, Hieb, zurueck); Throw: Leuchtstab knicken, ausholen, werfen (Loslassen 0,62 s)

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
    // V11: freier Raum vor der Lampe (m, Blickrichtung) - an einer Wand zieht die Hand die Lampe zurueck
    void setFlashClearance(float meters) { flashClear_ = meters; }
    // V11: so weit darf das Auge waagrecht vor der Koerpermitte liegen (Sprint-Vorlage an der Wand)
    void setEyeRoom(float meters) { eyeRoom_ = meters; }
    vec3 flashlightPos() const { return flashPos_; }  // Lampenkopf (Lichtquelle)
    vec3 flashlightDir() const { return flashDir_; }
    bool flashlightHeld() const { return flashHold_ > 0.5; }
    // V11: Faustgriff - Handrichtung (Handgelenk -> Knoechel), Handflaeche und Anteil (0..1) fuer das Skelett
    vec3 flashHandDir() const { return flashHandDir_; }
    vec3 flashPalm() const { return flashPalm_; }
    float flashWeight() const { return flashW_; }
    void reset() { initialized_ = false; }
    // V10: Proportionen (realistischer Koerper) und nur Requisiten zeichnen (Lampe, gehaltenes Item)
    void setProportions(const BodyProportions& p) { pr_ = p, initialized_ = false; }
    const BodyProportions& proportions() const { return pr_; }
    void collectProps(std::vector<VisibleMesh>& out);
    // V12: Hand im Faustgriff aus Blender zur Lampe zeichnen (nur realistischer Koerper; das Skelett blendet
    // seine eigene linke Hand dann aus)
    void setViewmodelHand(bool on, bool skinned = false) { vmHand_ = on, vmSkinned_ = skinned; }
    // V13.2 (B-001): von aussen gesehen haelt der Koerper die Lampe vor der Brust (Ellbogen unten), nicht in der
    // Haltung fuer das Ego-Bild (Hand knapp unter dem Auge, Ellbogen hoch)
    void setOutsideView(bool outside) { outside_ = outside; }
    bool outsideView() const { return outside_; }
    bool viewmodelHand() const { return vmHand_ && pr_.gripOut > 0.0f; }
    // V13: Geometrie der Hand im Rahmen der Lampe (aus dem Skelett, data/models/vm_hand_l.glb): Handgelenk, Richtung
    // des Unterarms (Ellbogen -> Handgelenk), Handrichtung (Handgelenk -> Knoechel), Handflaeche
    void setViewmodelGeometry(vec3 wrist, vec3 forearm, vec3 handDir, vec3 palm);
    // V13: die Lampe bleibt in der Hand, auch ausgeschaltet (gesenkt; Rahmen dann aus dem Unterarm)
    void setCarryLamp(bool on) { carry_ = on; }
    bool lampVisible() const { return carry_ ? true : flashHold_ > 0.5; }
    // V13: Daumen drueckt den Schalter (0..1 ueber 0,24 s); Rahmen der Lampe (x Glas, z oben, Ursprung Griffmitte)
    void pressSwitch() { pressT_ = 0.0; }
    bool pressing() const { return pressT_ >= 0.0; }
    float thumbPress() const;
    mat4 lampFrame() const;
    vec3 flashGrip() const { return flashGrip_; }
    vec3 flashUp() const { return flashUp_; }
    // V12 (Tests): Winkel zwischen Unterarm des Skeletts und Unterarm des Handmodells (rad), Gleiten der Hand
    // entlang der Lampenachse (m)
    float vmForearmError() const { return vmForearmErr_; }
    float vmSlide() const { return vmSlide_; }
    bool holdsItem() const { return showHeld_; }
    vec3 heldPos() const { return heldPos_; }  // V13.3: Ort des gehaltenen Items (Licht des Leuchtstabs in der Hand)
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
    vec3 flashHandDir_{1, 0, 0}, flashPalm_{0, 1, 0};  // V11
    float flashW_ = 0.0f;
    float flashClear_ = 9.0f, flashRetract_ = 0.0f, flashTilt_ = 0.0f;  // V11: Wandabstand, Zurueckziehen, Senken
    float eyeRoom_ = 9.0f;
    std::string mFlash_ = "flashlight", mFlashLit_ = "flashlight_on";
    int idFlash_ = -2, idFlashLit_ = -2;
    bool vmHand_ = false;  // V12
    bool outside_ = false;  // V13.2: von aussen gesehen (Schulteransicht, Mitspieler)
    bool vmSkinned_ = false, carry_ = false;  // V13
    double pressT_ = -1.0;
    vec3 vmWrist_{-0.0246f, -0.0633f, 0.0077f}, vmForearm_{0.8891f, 0.4574f, 0.0155f};  // V12-Werte, bis das Skelett sie setzt
    vec3 vmHandDir_{0.5592f, 0.6992f, -0.4454f}, vmPalm_{0.0f, 0.5373f, 0.8434f};
    float vmSlide_ = 0.0f, vmForearmErr_ = 0.0f;
    std::string mHand_ = "vm_hand_l";
    int idHand_ = -2;
    bool ghost_ = false;
    BodyProportions pr_;
};

}  // namespace lim::game
