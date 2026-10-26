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
        // V13.2 (B-001): Ruhepose mit weiter abgespreizten Armen (37 statt 24 Grad). Die Brust wird mit 7,5 cm
        // Rundung vernetzt; bei 24 Grad lag die Innenseite des Oberarms darin und Aermel und Rumpf verschmolzen zu
        // einer Flaeche - beim Bewegen riss der Aermel innen auf
        b.armAbduct = 0.95f;
        // V13.3: Unterarm 27 cm (Ellbogen- bis Handgelenk; vorher 0,146 x Groesse = 25,4 cm), Oberarm bleibt 0,172 x
        // Groesse = 29,9 cm (HAND_ANALYSE.md, Abschnitt V13.3)
        b.forearm = 0.27f;
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
