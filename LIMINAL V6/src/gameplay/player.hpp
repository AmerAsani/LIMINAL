// Spielerfigur: First-Person-Bewegung mit Traegheit, Kollision, Stufen,
// Schwerkraft und Head-Bobbing (Port von V4, player.py).
//
// V6: Springen (mit kurzer Nachsicht an Kanten und Tastenpuffer), Treppen werden
// beim Hinabgehen sauber "abgegangen" statt in Mikro-Spruengen hinuntergefallen,
// die Kamera folgt Stufen weich, Schritte zaehlen nur am Boden.
//
// Die Steuerung kommt als PlayerControl herein - egal ob von Tastatur/Maus,
// Gamepad oder dem Autopiloten der Demo.
#pragma once

#include "core/math.hpp"
#include "physics/collision.hpp"

namespace lim::game {

struct PlayerControl {
    float forward = 0, side = 0;    // -1 .. 1
    bool sprint = false;
    bool jump = false;              // V6: Sprungtaste in diesem Bild gedrueckt
    float turn = 0, look = 0;       // Pfeiltasten/Gamepad: -1 .. 1 (Drehrate)
    float yawDelta = 0, pitchDelta = 0;  // Maus: bereits in Radiant
};

class Player {
public:
    static constexpr double kEye = 1.62;
    static constexpr double kPitchMax = 1.45;  // V5: echte Kamera, fast senkrecht
    static constexpr double kGravity = 18.0;
    static constexpr double kJumpSpeed = 4.7;  // etwa 0,6 m Sprunghoehe

    Player() = default;
    Player(double x, double y, double z, double angle) : x(x), y(y), z(z), angle(angle) {}

    void update(double dt, const PlayerControl& c, phys::TileSource& world);
    void look(double dyaw, double dpitch);

    double eye() const;
    double sway() const;
    vec3 eyePos() const { return {(float)x, (float)y, (float)eye()}; }
    double speed() const { return std::hypot(vx, vy); }

    double x = 0, y = 0, z = 0;   // z = Fusshoehe
    double vz = 0;
    double angle = 0, pitch = 0;
    double vx = 0, vy = 0;
    double turnV = 0, pitchV = 0;
    double walkSpeed = 3.0, sprintMul = 1.9, turnSpeed = 2.3;
    bool bobEnabled = true;
    double bobPhase = 0, bobAmount = 0;
    double distance = 0;
    double speedMul = 1.0;        // Bonus durch Items
    phys::Shape shape;
    bool stepped = false;         // in diesem Bild einen Schritt gemacht (Schrittgeraeusch)
    bool landed = false;          // nach einem Fall aufgekommen
    double fallSpeed = 0;

    // --- V6 -------------------------------------------------------------------------
    bool onGround = true;
    bool jumped = false;          // in diesem Bild abgesprungen (Klang, Animation)
    double airTime = 0;           // Sekunden in der Luft
    double gait = 0;              // Gangstaerke 0..1 (unabhaengig von der Head-Bobbing-Einstellung)
    double groundSpeed = 0;       // tatsaechliche Geschwindigkeit am Boden (m/s)
    bool sprinting = false;       // sprintet wirklich (Taste + Bewegung)
    double camStep = 0;           // Kamera folgt Stufen weich: Rest der Hoehenaenderung (m)
    double landImpact = 0;        // Staerke der letzten Landung 0..1 (fuer Animation/Kamera)
    double breathless = 0;        // Atemnot 0..1 nach dem Sprinten (von der Sitzung gesetzt; Keuchen)

private:
    double coyote_ = 0;           // Sprung kurz nach Verlassen einer Kante noch erlaubt
    double jumpBuffer_ = 0;       // Sprungtaste kurz vor der Landung wird gemerkt
};

}  // namespace lim::game
