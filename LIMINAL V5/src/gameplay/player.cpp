#include "gameplay/player.hpp"

#include <cmath>

namespace lim::game {

static constexpr double kGravity = 18.0;
static constexpr double kTau = 6.283185307179586;

void Player::update(double dt, const PlayerControl& c, phys::TileSource& world) {
    stepped = false;
    landed = false;
    // Drehung per Pfeiltasten/Gamepad mit weicher Beschleunigung
    double target = c.turn * turnSpeed;
    turnV += (target - turnV) * std::min(1.0, dt * 14.0);
    angle = std::fmod(angle + turnV * dt + kTau, kTau);
    pitchV += (c.look * 1.1 - pitchV) * std::min(1.0, dt * 14.0);
    pitch = std::clamp(pitch + pitchV * dt, -kPitchMax, kPitchMax);
    look(c.yawDelta, c.pitchDelta);

    // Gewuenschte Bewegungsrichtung
    double fwd = std::clamp((double)c.forward, -1.0, 1.0), side = std::clamp((double)c.side, -1.0, 1.0);
    double ca = std::cos(angle), sa = std::sin(angle);
    double wx = ca * fwd - sa * side;
    double wy = sa * fwd + ca * side;
    double n = std::hypot(wx, wy);
    double speed = walkSpeed * speedMul * (c.sprint ? sprintMul : 1.0);
    if (n > 0.0) {
        double k = std::min(1.0, n);  // analoge Sticks: Teilgeschwindigkeit
        wx = wx / n * speed * k;
        wy = wy / n * speed * k;
    }
    double accel = n > 0.0 ? 10.0 : 12.0;
    double k = std::min(1.0, dt * accel);
    vx += (wx - vx) * k;
    vy += (wy - vy) * k;
    if (std::fabs(vx) < 1e-3 && std::fabs(vy) < 1e-3) vx = vy = 0.0;

    // Bewegung in Teilschritten, getrennt nach Achsen -> sauberes Gleiten an Waenden
    phys::MoveResult mr = phys::moveAndSlide(world, x, y, z, vx * dt, vy * dt, shape);
    if (mr.hitX) vx = 0.0;
    if (mr.hitY) vy = 0.0;
    distance += mr.moved;

    // Vertikal: Stufen weich hinauf, Absaetze mit Schwerkraft hinunter
    double g = phys::ground(world, x, y, z, shape);
    if (g > z) {
        z = std::min(g, z + dt * 5.0);
        vz = 0.0;
    } else if (g < z) {
        vz -= kGravity * dt;
        z += vz * dt;
        if (z <= g) {
            z = g;
            fallSpeed = -vz;
            landed = fallSpeed > 2.0;
            vz = 0.0;
        }
    } else {
        vz = 0.0;
    }

    // Head-Bobbing proportional zur tatsaechlichen Geschwindigkeit
    double spd = dt > 0 ? mr.moved / dt : 0.0;
    double targetBob = bobEnabled ? std::min(1.0, spd / 3.0) : 0.0;
    bobAmount += (targetBob - bobAmount) * std::min(1.0, dt * 6.0);
    double before = bobPhase;
    bobPhase += dt * (5.5 + spd * 1.3);
    // Ein Schritt je halber Bob-Periode (Tiefpunkt der Kopfbewegung)
    if (spd > 0.4 && std::floor(before / 3.14159265) != std::floor(bobPhase / 3.14159265)) stepped = true;
}

void Player::look(double dyaw, double dpitch) {
    if (dyaw != 0.0) angle = std::fmod(angle + dyaw + kTau, kTau);
    if (dpitch != 0.0) pitch = std::clamp(pitch + dpitch, -kPitchMax, kPitchMax);
}

double Player::eye() const {
    double bob = std::sin(bobPhase * 2.0) * 0.045 * bobAmount;
    return z + kEye + bob;
}

double Player::sway() const { return std::sin(bobPhase) * 0.006 * bobAmount; }

}  // namespace lim::game
