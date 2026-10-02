// Autopilot fuer Demo und Benchmark (Port von V4, _AutoInput): waehlt
// regelmaessig die Richtung mit der laengsten freien Strecke und dreht sich
// weich dorthin - ein ziellos wandernder Besucher.
#pragma once

#include "core/rng.hpp"
#include "gameplay/player.hpp"

namespace lim::game {

class Autopilot {
public:
    Autopilot(const Player& p, u64 seed) : rng_(seed), target_(p.angle) {}
    PlayerControl step(double dt, const Player& p, phys::TileSource& w);

private:
    double freeDistance(const Player& p, phys::TileSource& w, double ang, double limit = 24.0) const;
    double delta(const Player& p) const;

    Rng rng_;
    double target_;
    double timer_ = 0.0, stuck_ = 0.0;
};

}  // namespace lim::game
