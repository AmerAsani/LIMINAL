#include "anim/figures.hpp"

#include <array>

namespace lim::anim {

namespace {
std::array<Build, kFigureCount> makeBuilds() {
    std::array<Build, kFigureCount> a{};
    {   // Spieler: durchschnittlich, Augenhoehe ~1,62 m
        Build b;
        b.height = 1.74f;
        b.muscle = 0.35f;
        a[(int)Figure::Player] = b;
    }
    for (auto& b : a) b.finish();
    return a;
}
}  // namespace

const Build& figureBuild(Figure f) {
    static const std::array<Build, kFigureCount> builds = makeBuilds();
    return builds[(size_t)f];
}

const char* figureModel(Figure f) {
    static const char* names[kFigureCount] = {"fig_player"};
    return names[(int)f];
}

const char* figureName(Figure f) {
    static const char* names[kFigureCount] = {"Spieler"};
    return names[(int)f];
}

}  // namespace lim::anim
