// Farben der Oberflaeche: warme Toene wie in V4, damit Menues wie ein Teil der
// Welt wirken und nicht wie ein Fremdkoerper. Alle Werte sRGB.
#pragma once

#include "core/math.hpp"

namespace lim::ui::theme {

constexpr vec4 kPanel = rgba8(15, 14, 11, 236);
constexpr vec4 kPanelSoft = rgba8(15, 14, 11, 190);
constexpr vec4 kBorder = rgba8(84, 78, 58, 255);
constexpr vec4 kText = rgba8(222, 218, 200);
constexpr vec4 kTitle = rgba8(255, 214, 120);
constexpr vec4 kMuted = rgba8(138, 132, 112);
constexpr vec4 kRule = rgba8(84, 78, 58);
constexpr vec4 kValue = rgba8(255, 226, 160);
constexpr vec4 kSelBg = rgba8(246, 205, 112);
constexpr vec4 kSelText = rgba8(18, 16, 10);
constexpr vec4 kSlot = rgba8(26, 24, 20, 225);
constexpr vec4 kSlotSel = rgba8(92, 74, 34, 235);
constexpr vec4 kSlotBorder = rgba8(82, 74, 54);
constexpr vec4 kHealth = rgba8(214, 62, 52);
constexpr vec4 kSanity = rgba8(176, 156, 236);
constexpr vec4 kLow = rgba8(236, 84, 96);
constexpr vec4 kBarBg = rgba8(15, 14, 11, 215);
constexpr vec4 kEmpty = rgba8(60, 56, 48);
constexpr vec4 kDanger = rgba8(255, 120, 110);
constexpr vec4 kDangerBg = rgba8(40, 6, 6, 225);

}  // namespace lim::ui::theme
