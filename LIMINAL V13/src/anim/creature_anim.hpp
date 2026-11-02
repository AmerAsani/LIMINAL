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
//   Nest       atmet (V13.2).
//   V13.2:
//   Zweibeiner Faceling, Skin-Stealer, Wretch: Gang auf zwei Beinen, Haltung je Wesen (ANIMATIONEN.md); Gesten:
//              winken, zuschlagen, zugreifen, flehen, Hand vor das Gesicht.
//   Ratte      Trab, im Rennen Galopp in Spruengen; richtet sich witternd auf, frisst, beisst; der Schwanz schleift nach.
//   Fenster    der Umriss wiegt sich und winkt; greift er zu, lehnt er sich heraus, der Arm streckt sich bis 2 m.
//   V13.3:
//   Wrangler   der Leib zieht Ring um Ring durch Boden, Wand oder Decke, dreht sich wie ein Bohrer und zieht sich in Wellen
//              zusammen; der Kopf bricht heraus und schnappt zu; die Jungtiere winden sich am Ast (creature_wrangler.cpp).
#pragma once

#include <memory>
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
        float hang = 0.0f;        // V11 Buendel: 0 am Boden .. 1 verborgen ueber der Decke (clearance)
        float bait = 0.0f;        // V11 Buendel: 0 .. 1 wie weit der Koederarm aus der Decke haengt
        float light = 0.0f;       // V13.2: 0..1 im Lichtkegel der Lampe (Lichtreaktion je Wesen)
        float perceive = 0.0f;    // V13.2: 0..1 nimmt etwas wahr (wittert, lauscht) - Richtung perceiveAt
        vec3 perceiveAt;
        vec3 lightFrom;           // V13.2: woher das Licht kommt (Lampe)
        // V13.2 Zweibeiner und Fensterwesen: Gesten
        float wave = 0.0f;        // 0..1 winken (Faceling) / locken (Fensterwesen)
        float talk = 0.0f;        // 0..1 spricht (Skin-Stealer ahmt Worte nach): Kiefer im Takt von Silben
        float plea = 0.0f;        // 0..1 fleht (Wretch im zweiten Stadium): halb aufgerichtet, Haende vor
        float strike = 0.0f;      // Phase 0..1 eines Schlags (entwickelter Faceling mit Steinwerkzeug), 0 = keiner
    };

    void init(Entity e, vec3 pos, float yaw, unsigned seed = 1);
    void update(double dt, const Input& in);
    void snap(vec3 pos, float yaw);
    void setHidden(bool hidden);  // V11 Buendel: sofort im Schacht (ohne Aufsteigen)
    const CreaturePose& pose() const { return pose_; }
    Entity entity() const { return e_; }
    // ein Fuss hat in diesem Bild aufgesetzt (Klang): Ort und Gewicht (0 leicht .. 1 schwer)
    bool stepped() const { return stepped_; }
    vec3 stepPos() const { return stepPos_; }
    float stepWeight() const { return stepWeight_; }
    float hidden() const { return hang_; }  // V11 Buendel: 0 am Boden .. 1 im Schacht
    float speed() const { return speed_; }
    bool running() const { return run_; }
    float height() const;  // aktuelle Hoehe (geduckt)
    vec3 headPos() const;
    float wingbeat() const { return flapAmp_; }  // Deathmoth: 0 .. 1 Fluegelschlag (Klang)
    // V13.3: Groesse des Wesens (Massstab um die Wurzel: junge, ausgewachsene, verschmolzene Growler; Spinnen je Ebene) und
    // weitere Eingaben je Wesen (Nguithr'xurh: Anker und Zustand der Netzkugeln, siehe creature_nguithrxurh.cpp)
    void setScale(float s) { scale_ = s; }
    float scale() const { return scale_; }
    void setExtra(const std::vector<float>& v) { extra_ = v; }

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
    void updateNest(float dt, const Input& in);  // V13.2: atmet, Haut schwingt, Gelege zuckt
    void updateClump(float dt, const Input& in);  // V11
    void updateBiped(float dt, const Input& in);   // V13.2: Faceling, Skin-Stealer, Wretch
    void bipedKinL3(float dt, const Input& in);    // V13.3 Level 3: Waffe des Wretch, Mantel des Skin-Stealers (creature_kin_l3.cpp)
    void updateRat(float dt, const Input& in);     // V13.2: Death Rat
    void updateWindow(float dt, const Input& in);  // V13.2: Fensterwesen
    void updateDuller(float dt, const Input& in);  // V13.3: Duller (anim/creature_duller.cpp)
    void updateArms(float dt, const Input& in);    // V13.3: Arms (anim/creature_arms.cpp)
    void updateWrangler(float dt, const Input& in);  // V13.3: Wrangler (creature_wrangler.cpp)
    void updateGrowler(float dt, const Input& in);  // V13.3: creature_growler.cpp
    void updateNguithr(float dt, const Input& in);  // V13.3: creature_nguithrxurh.cpp
    void updateOuroboros(float dt, const Input& in);  // V13.3: creature_ouroboros.cpp (Arbeiter, Mauer, Geruest)
    float ouroHeight() const;
    vec3 ouroHead() const;
    void updateGoblin(float dt, const Input& in);  // V13.3: creature_goblin.cpp (Plague Goblins, Portal)
    float goblinHeight() const;
    vec3 goblinHead() const;
    void updateJerry(float dt, const Input& in);  // V13.3 Teil 4: creature_jerry.cpp (Jerry auf dem Rohr)
    // V13.3 Level 3 (KREATUR_DeathRat.md, KREATUR_Deathmoth.md): grosse Death Rat und ihre Falle (creature_rat_l3.cpp),
    // Praetorianer (creature_moth_l3.cpp) - Variante ueber extra_[0] (2 Ratte, 3 Falle; 5 Praetorianer)
    void updateRatL3(float dt, const Input& in);
    void updateRatTrap(float dt, const Input& in);
    float ratL3Height() const;
    void updatePraetorian(float dt, const Input& in);
    bool praetorian() const;
    // V13.3 Level 3 (KREATUR_Clump.md): das grosse, bewaffnete Buendel (creature_clump_l3.cpp) - extra_[0] Variante 1/2
    void updateClumpL3(float dt, const Input& in);
    bool bigClump() const { return e_ == Entity::Clump && !extra_.empty() && extra_[0] > 0.5f; }
    // Gangzyklus fuer n Beine; rest: Ruhepunkte (Welt) dieses Bildes
    void stepLegs(float dt, int n, const float* offs, float duty, float S, const vec3* rest, float standDur, const float* weight);
    vec3 footPos(int i, const float* lift) const;
    void initChain(Chain& c, const std::vector<vec3>& pts);
    void simChain(Chain& c, vec3 root, const std::vector<vec3>& target, float stiff, float damp, float gravity, float dt);
    // Kette, deren Ruheform starr an Knochen 'parent' haengt (Haar, Zungen): Gelenke idx[0..n)
    void hangChain(int chain, int parent, const int* idx, int n, float stiff, float damp, float gravity, vec3 wind, float dt);
    mat4 boneNow(int b) const;  // Rahmen des Knochens in der aktuellen Pose (pose_.up muss stimmen)
    void autoUps();
    // V11: Oben-Richtungen einer Kette vom Elternknochen weitertragen (keine Verdrehung gegen den Kopf)
    void chainUps(int parent, const int* bones, int n);
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
    // V13.2: Licht, Wahrnehmen, Ruhe (Gewicht verlagern, Pfote neu setzen), Schnueffeln in Stoessen
    float light_ = 0.0f, lightV_ = 0.0f, perc_ = 0.0f, percV_ = 0.0f, idleW_ = 1.0f, idleWV_ = 0.0f;
    float shuffleT_ = 5.0f, sniff_ = 0.0f, sniffV_ = 0.0f, swayPh_ = 0.0f;
    // V13.2: Gesten der Zweibeiner und des Fensterwesens
    float wave_ = 0.0f, waveV_ = 0.0f, talk_ = 0.0f, talkV_ = 0.0f, plea_ = 0.0f, pleaV_ = 0.0f;
    bool walk_ = false;  // V13.2 Zweibeiner: ging im letzten Bild (Anlauf: der hintere Fuss tritt zuerst)
    float flow_ = 0.0f;  // V13.3 Wrangler: Weg, den der Leib gezogen ist (m)
    // V11 Buendel: je Arm die aufgesetzte Hand und der Schwung zum naechsten Griff; der Kern zieht ruckweise
    // nach und schaukelt; die Beine schleifen
    struct Grip {
        vec3 plant, from, to;
        bool swing = false;
        float t = 0.0f, dur = 0.3f, stray = 0.35f;
    };
    Grip grip_[cl::kArms];
    vec3 core_, coreV_, foot_[cl::kLegs];
    float hang_ = 0.0f, hangV_ = 0.0f, bait_ = 0.0f, baitV_ = 0.0f, windup_ = 0.0f, windupV_ = 0.0f;
    float grab_ = 0.0f, grabV_ = 0.0f, grope_ = 0.0f, gropeV_ = 0.0f, pull_ = 0.0f;
    // Smiler: Masse, die nachzieht; V11: Lider (0 offen .. 1 zu), Zeit bis zum naechsten Lidschlag
    vec3 mass_, massV_;
    float lids_ = 0.0f, blinkT_ = 4.0f, blinkPh_ = -1.0f;
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
    std::vector<float> xs_;  // V13.3: Zustand weiterer Wesen (Duller: Brocken, Stolpern; Arms: Fetzen, Rauch) - Belegung je Datei
    bool stepped_ = false;
    vec3 stepPos_;
    float stepWeight_ = 0.5f;
    unsigned rng_ = 1;
    bool init_ = false;
    // V13.3: Massstab, weitere Eingaben, eigener Zustand je Wesen (in dessen Datei definiert)
    float scale_ = 1.0f;
    std::vector<float> extra_;
    std::shared_ptr<void> ext_;
};

}  // namespace lim::anim
