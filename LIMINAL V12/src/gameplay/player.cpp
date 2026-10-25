#include "gameplay/player.hpp"

#include <cmath>

namespace lim::game {

static constexpr double kTau = 6.283185307179586;
static constexpr double kPiD = 3.141592653589793;

void Player::update(double dt, const PlayerControl& c, phys::TileSource& world) {
    stepped = false;
    landed = false;
    jumped = false;
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
    // V11: leise gehen - knapp halbes Tempo, kein Sprint
    sneaking = c.sneak;
    double speed = walkSpeed * speedMul * (c.sneak ? 0.45 : (c.sprint ? sprintMul : 1.0));
    if (n > 0.0) {
        double k = std::min(1.0, n);  // analoge Sticks: Teilgeschwindigkeit
        wx = wx / n * speed * k;
        wy = wy / n * speed * k;
    }
    // V11: Wasser - waten (langsamer), schwimmen (ab Brusttiefe: wohin man blickt, ohne Schwerkraft)
    stroke = false;
    const bool hasWater = waterZ > -900.0;
    wading = hasWater && z < waterZ - 0.25;
    swimming = hasWater && z < waterZ - 1.15;
    if (swimming) {
        const double sw = (c.sprint ? 2.5 : 1.7) * speedMul;
        double fx = ca * fwd - sa * side, fy = sa * fwd + ca * side;
        const double nn = std::hypot(fx, fy);
        if (nn > 1.0) fx /= nn, fy /= nn;
        const double cp = std::cos(pitch);
        vx += (fx * sw * cp - vx) * std::min(1.0, dt * 2.5);
        vy += (fy * sw * cp - vy) * std::min(1.0, dt * 2.5);
        // senkrecht: Blick (vorwaerts), Sprungtaste hinauf, sonst Auftrieb bis der Kopf aus dem Wasser ragt
        const double floatZ = waterZ - kEye + 0.12;
        double want = fwd * std::sin(pitch) * sw;
        if (c.jumpHeld) want = 1.6;
        else if (std::fabs(want) < 0.2) want = std::clamp((floatZ - z) * 1.2, -0.6, 0.8);
        vz += (want - vz) * std::min(1.0, dt * 2.5);
        phys::MoveResult mr2 = phys::moveAndSlide(world, x, y, z, vx * dt, vy * dt, shape);
        // an einen Rand geschwommen (Becken, Treppe, Ufer): bis 1,8 m hoch hinaufziehen
        if ((mr2.hitX || mr2.hitY) && fwd > 0.2) {
            const double ax = x + ca * 0.5, ay = y + sa * 0.5;
            const double ga = phys::ground(world, ax, ay, z + 1.9, shape);
            if (ga > z + 0.1 && ga - z < 1.8 && ga > waterZ - 1.1) {
                z = ga + 0.01;
                phys::MoveResult up = phys::moveAndSlide(world, x, y, z, ca * 0.35, sa * 0.35, shape);
                (void)up;
                vz = 0.0;
                onGround = true;
                swimming = false;
                camStep = std::clamp(camStep - 0.6, -0.9, 0.9);
                return;
            }
        }
        if (mr2.hitX) vx = 0.0;
        if (mr2.hitY) vy = 0.0;
        distance += mr2.moved;
        z += vz * dt;
        const double g0 = phys::ground(world, x, y, z, shape);
        if (z < g0) z = g0, vz = std::max(0.0, vz);
        const double cap = phys::headroom(world, x, y, shape);
        if (z > cap && cap > g0) z = cap, vz = std::min(0.0, vz);
        z = std::min(z, waterZ - 0.2);  // aus dem Wasser steigt man ueber den Boden (waten), nicht durch Auftrieb
        onGround = false;
        airTime = 0.0;
        underwater = eye() < waterZ - 0.05;
        const double sp = std::hypot(vx, vy) + std::fabs(vz);
        const double before = strokePhase;
        strokePhase += dt * (1.2 + sp * 0.8);
        if (sp > 0.4 && std::floor(before) != std::floor(strokePhase)) stroke = true;
        groundSpeed = 0.0;
        gait += (0.0 - gait) * std::min(1.0, dt * 6.0);
        sprinting = false;
        camStep *= std::exp(-dt * 13.0);
        return;
    }
    if (wading) {
        wx *= 0.75;
        wy *= 0.75;
    }
    underwater = hasWater && eye() < waterZ - 0.05;
    // In der Luft nur wenig Steuerung: der Schwung des Absprungs bleibt erhalten
    double accel = !onGround ? 3.0 : (n > 0.0 ? 10.0 : 12.0);
    double k = std::min(1.0, dt * accel);
    vx += (wx - vx) * k;
    vy += (wy - vy) * k;
    if (std::fabs(vx) < 1e-3 && std::fabs(vy) < 1e-3) vx = vy = 0.0;

    // Bewegung in Teilschritten, getrennt nach Achsen -> sauberes Gleiten an Waenden
    phys::MoveResult mr = phys::moveAndSlide(world, x, y, z, vx * dt, vy * dt, shape);
    if (mr.hitX) vx = 0.0;
    if (mr.hitY) vy = 0.0;
    distance += mr.moved;

    // --- Vertikal ----------------------------------------------------------------------
    jumpBuffer_ = c.jump ? 0.14 : std::max(0.0, jumpBuffer_ - dt);
    coyote_ = onGround ? 0.12 : std::max(0.0, coyote_ - dt);
    double g = phys::ground(world, x, y, z, shape);
    if (jumpBuffer_ > 0.0 && coyote_ > 0.0 && vz <= 0.0) {
        // nur mit genug Kopffreiheit (niedrige Wartungsgaenge: kein Zappeln an der Decke)
        double base = std::max(z, g);
        if (phys::headroom(world, x, y, shape) - base > 0.15) {
            camStep = std::clamp(camStep - (base - z), -0.9, 0.9);
            z = base;
            vz = kJumpSpeed;
            onGround = false;
            jumped = true;
            airTime = 0.0;
            coyote_ = jumpBuffer_ = 0.0;
        }
    }
    if (onGround) {
        double d = g - z;
        if (d > 0.0 || (d < 0.0 && -d <= shape.maxStep + 0.02)) {
            // Stufe hinauf bzw. hinunter "abgehen" statt fallen (kein Landegeraeusch je Stufe);
            // die Kamera folgt der Hoehenaenderung weich ueber camStep
            z = g;
            camStep = std::clamp(camStep - d, -0.9, 0.9);
        } else if (d < 0.0) {
            onGround = false;  // ueber eine echte Kante hinaus: fallen
            vz = 0.0;
            airTime = 0.0;
        }
    }
    if (!onGround) {
        airTime += dt;
        vz -= kGravity * dt;
        z += vz * dt;
        double cap = phys::headroom(world, x, y, shape);
        if (z > cap && cap > g) {  // Kopf an der Decke
            z = cap;
            if (vz > 0.0) vz = 0.0;
        }
        double g2 = phys::ground(world, x, y, z, shape);
        if (z <= g2) {
            if (vz > 0.0) {
                // beim Aufsteigen ueber eine Stufe/Theke: mitnehmen, Sprung geht weiter (Kamera weich)
                camStep = std::clamp(camStep - (g2 - z), -0.9, 0.9);
                z = g2;
            } else {
                fallSpeed = -vz;
                landed = fallSpeed > 2.5;
                landImpact = std::clamp((fallSpeed - 1.5) / 8.0, 0.0, 1.0);
                z = g2;
                vz = 0.0;
                onGround = true;
                airTime = 0.0;
            }
        }
    } else {
        vz = 0.0;
    }
    camStep *= std::exp(-dt * 13.0);

    // Gang und Head-Bobbing proportional zur tatsaechlichen Geschwindigkeit am Boden
    double spd = (onGround && dt > 0) ? mr.moved / dt : 0.0;
    groundSpeed = spd;
    double targetGait = std::min(1.0, spd / 3.0);
    gait += (targetGait - gait) * std::min(1.0, dt * 6.0);
    double targetBob = bobEnabled ? targetGait : 0.0;
    bobAmount += (targetBob - bobAmount) * std::min(1.0, dt * 6.0);
    double before = bobPhase;
    if (onGround) bobPhase = std::fmod(bobPhase + dt * (5.5 + spd * 1.3), 4096.0 * kPiD);
    // Ein Schritt je halber Gangperiode: Fuss setzt auf, Kopf am tiefsten Punkt
    if (onGround && spd > 0.4 && std::floor(before / kPiD) != std::floor(bobPhase / kPiD)) stepped = true;
    sprinting = c.sprint && !c.sneak && onGround && spd > walkSpeed * speedMul * 1.25;
}

void Player::look(double dyaw, double dpitch) {
    if (dyaw != 0.0) angle = std::fmod(angle + dyaw + kTau, kTau);
    if (dpitch != 0.0) pitch = std::clamp(pitch + dpitch, -kPitchMax, kPitchMax);
}

double Player::eye() const {
    // tiefster Punkt, wenn ein Fuss aufsetzt (bobPhase = k * pi)
    double bob = -std::cos(bobPhase * 2.0) * 0.04 * bobAmount;
    return z + camStep + kEye + bob;
}

double Player::sway() const { return std::sin(bobPhase) * 0.006 * bobAmount; }

}  // namespace lim::game
