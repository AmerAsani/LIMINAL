// Farben und Masse der Oberflaeche. V11: ruhiger und dunkler; ein warmer Akzent nur fuer Auswahlmarke,
// aktive Werte und Fortschritt - keine gefuellten Gelbflaechen mehr. Texte gebrochen weiss, Trennlinien als
// feine helle Haarlinien, Paneele als dunkles Glas. Alle Werte sRGB.
#pragma once

#include "core/math.hpp"

namespace lim::ui::theme {

constexpr vec4 kPanel = rgba8(12, 12, 11, 238);
constexpr vec4 kPanelSoft = rgba8(10, 10, 9, 150);
constexpr vec4 kBorder = rgba8(255, 255, 255, 20);
constexpr vec4 kText = rgba8(212, 208, 196);
constexpr vec4 kTitle = rgba8(234, 228, 212);
constexpr vec4 kAccent = rgba8(206, 170, 102);     // sparsam: Auswahlmarke, aktiver Wert, Fortschritt
constexpr vec4 kMuted = rgba8(132, 128, 118);
constexpr vec4 kRule = rgba8(255, 255, 255, 26);
constexpr vec4 kValue = rgba8(226, 220, 202);
constexpr vec4 kSelBg = kAccent;                    // Kante ausgewaehlter Plaetze
constexpr vec4 kSelRow = rgba8(255, 255, 255, 8);  // Grund der ausgewaehlten Zeile
constexpr vec4 kSelText = rgba8(250, 246, 236);
constexpr vec4 kSlot = rgba8(10, 10, 9, 96);
constexpr vec4 kSlotFull = rgba8(10, 10, 9, 150);
constexpr vec4 kSlotSel = rgba8(206, 170, 102, 40);
constexpr vec4 kSlotBorder = rgba8(255, 255, 255, 36);
constexpr vec4 kHealth = rgba8(196, 82, 68);
constexpr vec4 kSanity = rgba8(150, 142, 200);
constexpr vec4 kLow = rgba8(230, 92, 82);
constexpr vec4 kBarBg = rgba8(8, 8, 8, 150);
constexpr vec4 kEmpty = rgba8(255, 255, 255, 36);
constexpr vec4 kDanger = rgba8(238, 126, 110);
constexpr vec4 kDangerBg = rgba8(30, 6, 6, 200);

// Schriftgroessen bei 1080 p (mit S() skaliert): Titel, Listenzeile, Fliesstext, Hinweis, Kapitaelchen
constexpr float kFsTitle = 26.0f, kFsRow = 22.0f, kFsBody = 18.0f, kFsSmall = 15.0f, kFsCaps = 14.0f;

}  // namespace lim::ui::theme
