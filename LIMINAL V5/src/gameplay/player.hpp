// Spielerfigur: First-Person-Bewegung mit Traegheit, Kollision, Stufen,
// Schwerkraft und Head-Bobbing (Port von V4, player.py).
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
    float turn = 0, look = 0;       // Pfeiltasten/Gamepad: -1 .. 1 (Drehrate)
    float yawDelta = 0, pitchDelta = 0;  // Maus: bereits in Radiant
};

class Player {
public:
    static constexpr double kEye = 1.62;
    static constexpr double kPitchMax = 1.45;  // V5: echte Kamera, fast senkrecht

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
};

}  // namespace lim::game
