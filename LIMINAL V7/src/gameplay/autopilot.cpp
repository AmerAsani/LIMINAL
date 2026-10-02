#include "gameplay/autopilot.hpp"

#include <cmath>

namespace lim::game {

static constexpr double kPiD = 3.14159265358979;

double Autopilot::freeDistance(const Player& p, phys::TileSource& w, double ang, double limit) const {
    double dx = std::cos(ang), dy = std::sin(ang);
    double z = p.z, d = 0.0;
    while (d < limit) {
        d += 0.3;
        const world::Tile& c = w.at(ifloor(p.x + dx * d), ifloor(p.y + dy * d));
        if (c.solid() || c.floor - z > 0.5 || c.ceil - c.floor < 1.8) return d;
        z = c.floor;
    }
    return limit;
}

double Autopilot::delta(const Player& p) const {
    double d = std::fmod(target_ - p.angle + kPiD, 2.0 * kPiD);
    if (d < 0) d += 2.0 * kPiD;
    return d - kPiD;
}

PlayerControl Autopilot::step(double dt, const Player& p, phys::TileSource& w) {
    timer_ -= dt;
    double moved = std::hypot(p.vx, p.vy);
    stuck_ = (moved < 0.3 && std::fabs(delta(p)) < 0.5) ? stuck_ + dt : 0.0;
    if (stuck_ > 0.6) {
        stuck_ = 0.0;
        target_ = p.angle + rng_.uniform(1.0, 5.3);
        timer_ = 1.5;
    } else {
        bool aligned = std::fabs(delta(p)) < 0.3;
        if (timer_ <= 0.0 || (aligned && freeDistance(p, w, p.angle, 2.5) < 1.6)) {
            double bestScore = -1.0, bestA = p.angle;
            for (int i = 0; i < 16; ++i) {
                double a = p.angle + (i / 16.0) * 2.0 * kPiD;
                double score = freeDistance(p, w, a) * rng_.uniform(0.6, 1.0);
                if (i >= 7 && i <= 9) score *= 0.3;  // nicht staendig umkehren
                if (score > bestScore) {
                    bestScore = score;
                    bestA = a;
                }
            }
            target_ = bestA;
            timer_ = rng_.uniform(2.0, 5.0);
        }
    }
    PlayerControl c;
    double d = delta(p);
    c.forward = std::fabs(d) < 0.5 ? 1.0f : 0.0f;
    c.turn = d > 0.08 ? 1.0f : (d < -0.08 ? -1.0f : 0.0f);
    c.sprint = true;
    return c;
}

}  // namespace lim::game
