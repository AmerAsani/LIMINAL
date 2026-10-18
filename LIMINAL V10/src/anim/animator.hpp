// V10: Prozedurale Animation der Figuren.
//
// Die KI bewegt nur die Wurzel (Position auf dem Boden, Blickrichtung des Koerpers) und sagt,
// wohin der Kopf sehen soll. Der Animator macht daraus einen Koerper:
//   Gehen      Gangzyklus aus der zurueckgelegten Strecke; Fuesse stehen waehrend des Stands
//              fest am Boden (kein Gleiten), der Schwungfuss landet dort, wo der Koerper hinwill.
//              Becken wippt, schwankt zum Standbein und dreht mit; Oberkoerper dreht gegen,
//              Arme pendeln gegengleich.
//   Stehen     Gewicht verlagert sich langsam, Atmung hebt Brust und Schultern. Stehen die
//              Fuesse nach einer Drehung schief, werden sie mit kleinen Schritten nachgesetzt.
//   Kopf       dreht sich mit eigener Geschwindigkeit zum Ziel - auf Wunsch weiter, als ein
//              Mensch es koennte, oder unnatuerlich langsam.
//   Haltung    vorgebeugt, Kopf gesenkt, Schultern haengend, Knie gebeugt, seitlich geneigt,
//              Arme haengend oder vorgestreckt, Finger gekruemmt; Hinken (ungleiche Schritte).
// Eingefroren (die Figur wird beobachtet) wird update() einfach nicht aufgerufen.
#pragma once

#include "anim/figures.hpp"
#include "anim/skeleton.hpp"

namespace lim::anim {

// Bewegungscharakter einer Figur
struct Style {
    float stride = 1.0f;        // Schrittlaenge (Faktor)
    float liftHeight = 1.0f;    // Fusshub
    float armSwing = 1.0f;
    float armHang = 0.0f;       // Arme haengen weiter vorn (rad)
    float armOut = 0.0f;        // Arme etwas vom Koerper weg (rad)
    float elbowBend = 0.2f;     // Grundbeugung der Ellbogen (rad)
    float kneeBend = 0.0f;      // dauerhaft gebeugte Knie (m Absenkung des Beckens)
    float lean = 0.04f;         // Oberkoerper vor (rad)
    float headDrop = 0.0f;      // Kopf gesenkt (rad)
    float headTilt = 0.0f;      // Kopf seitlich geneigt (rad)
    float slump = 0.0f;         // Schultern haengend (m)
    float hipSway = 1.0f;
    float bob = 1.0f;
    float limp = 0.0f;          // Hinken: ein Bein schneller, Becken knickt ein (0..1)
    float breath = 1.0f;        // Atemtiefe
    float breathRate = 0.25f;   // Atemzuege je Sekunde
    float idleSway = 1.0f;      // Gewichtsverlagerung im Stehen
    float fingerCurl = 0.2f;
    float headSpeed = 2.2f;     // rad/s
    float headMax = 1.35f;      // groesster Drehwinkel des Kopfes gegen den Rumpf (rad)
    float footOut = 0.12f;      // Fussspitzen nach aussen (rad)
    float stanceWidth = 1.0f;
};

const Style& figureStyle(Figure f);

class Animator {
public:
    struct Input {
        vec3 pos;                 // Wurzel auf dem Boden
        float yaw = 0.0f;         // Blickrichtung des Koerpers
        bool look = false;        // Kopf zu lookAt drehen (sonst geradeaus)
        vec3 lookAt;
        float headTiltAdd = 0.0f; // zusaetzliche Neigung (rad)
        float leanSide = 0.0f;    // Oberkoerper seitlich (rad, + = nach links), z. B. hinter einer Kante hervor
        float crouch = 0.0f;      // 0..1
        float reach = 0.0f;       // 0..1: Arme greifen nach vorn
        vec3 reachAt;             // Ziel der Haende
        float breathMul = 1.0f;   // 0 = haelt den Atem an
    };

    void init(Figure f, vec3 pos, float yaw);
    void setStyle(const Style& s) { style_ = s; }
    const Style& style() const { return style_; }
    void update(double dt, const Input& in);
    const JointPose& pose() const { return pose_; }
    // Gangphase und Schritt (fuer Schrittgeraeusche): true, wenn in diesem Bild ein Fuss aufsetzte
    bool stepped() const { return stepped_; }
    int stepFoot() const { return stepFoot_; }
    float footT(int i) const { return foot_[i].swing ? foot_[i].t : -1.0f; }  // Pruefung: Schwungfortschritt (-1 steht)
    float phase() const { return phase_; }
    float speed() const { return speed_; }
    float headYaw() const { return headYaw_; }  // aktuelle Kopfdrehung gegen den Rumpf
    vec3 headPos() const { return pose_.j[J_HEAD] + pose_.headUp * 0.08f; }
    vec3 eyePos() const;
    // sofort in eine Stehpose an pos (z. B. nach dem Versetzen)
    void snap(vec3 pos, float yaw);

private:
    struct Foot {
        vec3 plant, from, to;  // Standpunkt; Schwung von -> nach
        float yaw = 0, fromYaw = 0, toYaw = 0;
        bool swing = false;
        float t = 0.0f, dur = 0.4f;  // Schwungfortschritt
    };
    vec3 restFoot(int i, vec3 pos, float yaw) const;
    void startSwing(int i, vec3 target, float yaw, float dur);

    Figure fig_ = Figure::Player;
    Build b_;
    Style style_;
    JointPose pose_, bind_;
    Foot foot_[2];
    vec3 pos_, vel_;
    float yaw_ = 0.0f;
    float phase_ = 0.0f;      // Gangzyklus 0..1
    float speed_ = 0.0f;      // geglaettet
    float time_ = 0.0f, breathT_ = 0.0f;
    float headYaw_ = 0.0f, headPitch_ = 0.0f;
    float sway_ = 0.0f;
    bool stepped_ = false;
    int stepFoot_ = 0;
    bool init_ = false;
    int lastLead_ = 0;
};

}  // namespace lim::anim
