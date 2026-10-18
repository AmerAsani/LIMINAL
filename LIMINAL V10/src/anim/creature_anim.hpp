// V10: Prozedurale Bewegung der Entities - passend zur Anatomie, nicht menschlich.
//
//   Smiler     hat keine Beine: die schwarze Masse schwebt knapp ueber dem Boden und zieht beim
//              Gleiten nach; ihre Zungen zuengeln nach oben wie Rauch ueber einer Flamme. Das Gesicht
//              dreht sich langsam zu dir, das Laecheln oeffnet sich weiter.
//   Hound      geht auf allen vieren im Kreuzgang, rennt im Galopp mit Flugphase, hinkt vorn links.
//              Pirscht er, duckt er sich tief, der Kopf haengt vorgestreckt unter den Schultern. Beim
//              Knurren zittert der Kiefer, beim Zuschnappen stoesst der Kopf vor. Das Haar haengt und
//              schwingt mit Traegheit nach.
//   Deathmoth  fliegt mit zwei Fluegelpaaren (das hintere Paar schlaegt etwas spaeter), der Koerper
//              wippt gegen den Schlag und legt sich in die Kurven; die Beine sind im Flug angezogen.
//              Sitzt sie, liegen die Fluegel flach an der Flaeche, die Beine greifen - vor dem Abflug
//              zittern die Fluegel (Aufwaermen).
//   Gestalt    steht. Kaum merkliches Wanken, die unteren Fasern treiben.
//   Nest       starr.
#pragma once

#include <vector>

#include "anim/creature.hpp"

namespace lim::anim {

class CreatureAnimator {
public:
    struct Input {
        vec3 pos;                 // Wurzel am Boden (Deathmoth: Mitte der Brust)
        float yaw = 0.0f;         // Blickrichtung des Koerpers
        bool look = false;        // Kopf/Gesicht zu lookAt
        vec3 lookAt;
        float tilt = 0.0f;        // Kopf seitlich geneigt (rad)
        float lean = 0.0f;        // seitlich hervorlehnen (rad, + links)
        float reach = 0.0f;       // 0..1: der Hound schnappt nach reachAt
        vec3 reachAt;
        float clearance = 10.0f;  // freie Hoehe ueber dem Boden (m)
        float jaw = 0.0f;         // 0..1: Maul / Laecheln oeffnen
        float alert = 0.0f;       // 0..1: unruhig, lauscht
        float crouch = 0.0f;      // 0..1: der Hound pirscht geduckt
        float growl = 0.0f;       // 0..1: der Hound knurrt
        vec3 up{0, 0, 1};         // Deathmoth: Koerper-oben (sitzend: Normale der Flaeche)
        float fly = 0.0f;         // Deathmoth: 0 sitzt .. 1 fliegt
        float shiver = 0.0f;      // Deathmoth: 0..1 Fluegelzittern vor dem Abflug
        vec3 fwd{0, 0, 0};        // Deathmoth: Koerper-vorn (0: aus yaw)
    };

    void init(Entity e, vec3 pos, float yaw, unsigned seed = 1);
    void update(double dt, const Input& in);
    void snap(vec3 pos, float yaw);
    const CreaturePose& pose() const { return pose_; }
    Entity entity() const { return e_; }
    // ein Fuss hat in diesem Bild aufgesetzt (Klang): Ort und Gewicht (0 leicht .. 1 schwer)
    bool stepped() const { return stepped_; }
    vec3 stepPos() const { return stepPos_; }
    float stepWeight() const { return stepWeight_; }
    float speed() const { return speed_; }
    bool running() const { return run_; }
    float height() const;  // aktuelle Hoehe (geduckt)
    vec3 headPos() const;
    float wingbeat() const { return flapAmp_; }  // Deathmoth: 0 .. 1 Fluegelschlag (Klang)

private:
    struct Leg {
        vec3 plant, from, to;
        bool swing = false;
        float t = 0.0f, dur = 0.5f;
    };
    struct Chain {
        std::vector<vec3> p, prev;
        std::vector<float> len;
    };

    void updateSmiler(float dt, const Input& in);
    void updateHound(float dt, const Input& in);
    void updateMoth(float dt, const Input& in);
    void updateGestalt(float dt, const Input& in);
    void updateNest();
    // Gangzyklus fuer n Beine; rest: Ruhepunkte (Welt) dieses Bildes
    void stepLegs(float dt, int n, const float* offs, float duty, float S, const vec3* rest, float standDur, const float* weight);
    vec3 footPos(int i, const float* lift) const;
    void initChain(Chain& c, const std::vector<vec3>& pts);
    void simChain(Chain& c, vec3 root, const std::vector<vec3>& target, float stiff, float damp, float gravity, float dt);
    // Kette, deren Ruheform starr an Knochen 'parent' haengt (Haar, Zungen): Gelenke idx[0..n)
    void hangChain(int chain, int parent, const int* idx, int n, float stiff, float damp, float gravity, vec3 wind, float dt);
    mat4 boneNow(int b) const;  // Rahmen des Knochens in der aktuellen Pose (pose_.up muss stimmen)
    void autoUps();
    float rnd();

    Entity e_ = Entity::Smiler;
    const CreatureSkeleton* sk_ = nullptr;
    std::vector<mat4> bindFrames_;
    CreaturePose pose_;
    vec3 pos_, vel_;
    float yaw_ = 0.0f, speed_ = 0.0f, phase_ = 0.0f, time_ = 0.0f;
    Leg leg_[4];
    int legs_ = 0;
    bool run_ = false;
    std::vector<Chain> chain_;
    bool chainInit_ = false;
    // Kopf / Gesicht
    float headYaw_ = 0.0f, headPitch_ = 0.0f, headYawV_ = 0.0f, headPitchV_ = 0.0f;
    vec3 headDir_{1, 0, 0}, headDirV_, headPos_, headPosV_;
    float tilt_ = 0.0f, tiltV_ = 0.0f, listenT_ = 4.0f, listenOn_ = 0.0f, listen_ = 0.0f, listenV_ = 0.0f, listenSide_ = 1.0f;
    // Haltung
    float crouch_ = 0.0f, crouchV_ = 0.0f, lean_ = 0.0f, leanV_ = 0.0f, reach_ = 0.0f, reachV_ = 0.0f;
    float jaw_ = 0.0f, jawV_ = 0.0f, growl_ = 0.0f, growlV_ = 0.0f;
    float curl_[2][5]{}, curlT_[2][5]{}, curlV_[2][5]{};
    float twitchT_ = 5.0f;
    // Smiler: Masse, die nachzieht
    vec3 mass_, massV_;
    // Deathmoth
    vec3 bodyUp_{0, 0, 1}, bodyUpV_, bodyFwd_{1, 0, 0};
    float fly_ = 0.0f, flyV_ = 0.0f, flap_ = 0.0f, flapAmp_ = 0.0f, bank_ = 0.0f, bankV_ = 0.0f, shiverPh_ = 0.0f;
    float antT_ = 1.0f, ant_[2]{}, antTg_[2]{}, antV_[2]{};
    // Hindernisse fuer Haar und Zungen (Kapseln a..b, Radius): nicht durch den Koerper
    struct Cap {
        vec3 a, b;
        float r;
    };
    std::vector<Cap> cols_;
    Input last_;
    bool stepped_ = false;
    vec3 stepPos_;
    float stepWeight_ = 0.5f;
    unsigned rng_ = 1;
    bool init_ = false;
};

}  // namespace lim::anim
