// V10: Menschen in LIMINAL - nur noch du selbst (Kapuzenjacke, Jeans, Turnschuhe) und dein
// Trugbild. Alle anderen Gestalten sind keine Menschen (anim/creature.hpp).
#pragma once

#include <cstdint>

#include "anim/skeleton.hpp"

namespace lim::anim {

enum class Figure : std::uint8_t { Player, Count };
constexpr int kFigureCount = (int)Figure::Count;
constexpr int kFigureLods = 3;

// Koerperbau (fertig, mit finish())
const Build& figureBuild(Figure f);
// Basisname der Modelle ("fig_watcher"); je Detailstufe "<name>_l0", "_l1", "_l2"
const char* figureModel(Figure f);
const char* figureName(Figure f);  // deutsch, fuer Protokolle

}  // namespace lim::anim
