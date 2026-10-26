// Farben und Masse der Oberflaeche.
// V12: Beschilderung statt Glas. Die Oberflaeche spricht die Sprache der Orte, durch die man geht: Wegweiser,
// Stationstafeln, Gate-Schilder. Tiefes Blauschwarz als Grund, ein Signalgelb als einziger Akzent (Auswahl,
// aktive Werte, Fortschritt, die Linie des Streckenplans), gerade Kanten, Kapitaelchen mit Sperrsatz fuer
// Beschriftungen, feine Haarlinien. Alle Werte sRGB.
#pragma once

#include "core/math.hpp"

namespace lim::ui::theme {

constexpr vec4 kPanel = rgba8(9, 11, 13, 236);
constexpr vec4 kPanelSoft = rgba8(8, 10, 12, 168);
constexpr vec4 kBorder = rgba8(255, 255, 255, 18);
constexpr vec4 kText = rgba8(214, 216, 212);
constexpr vec4 kTitle = rgba8(246, 244, 236);
constexpr vec4 kAccent = rgba8(240, 196, 70);      // Signalgelb: Auswahl, aktiver Wert, Fortschritt, Strecke
constexpr vec4 kMuted = rgba8(126, 132, 134);
constexpr vec4 kRule = rgba8(255, 255, 255, 30);
constexpr vec4 kValue = rgba8(232, 232, 224);
constexpr vec4 kSelBg = kAccent;                    // Kante ausgewaehlter Plaetze
constexpr vec4 kSelRow = rgba8(240, 196, 70, 20);  // Grund der ausgewaehlten Zeile (Hauch Signalgelb)
constexpr vec4 kSelText = rgba8(255, 252, 238);
constexpr vec4 kSlot = rgba8(8, 10, 12, 110);
constexpr vec4 kSlotFull = rgba8(8, 10, 12, 170);
constexpr vec4 kSlotSel = rgba8(240, 196, 70, 36);
constexpr vec4 kSlotBorder = rgba8(255, 255, 255, 40);
constexpr vec4 kHealth = rgba8(220, 92, 72);
constexpr vec4 kSanity = rgba8(124, 170, 218);
constexpr vec4 kLow = rgba8(240, 84, 70);
constexpr vec4 kBarBg = rgba8(8, 8, 8, 150);
constexpr vec4 kEmpty = rgba8(255, 255, 255, 34);
constexpr vec4 kDanger = rgba8(246, 124, 104);
constexpr vec4 kDangerBg = rgba8(34, 6, 6, 210);
constexpr vec4 kSidebar = rgba8(7, 9, 11, 244);     // V12: Seitenleiste von Titel- und Pausenmenue

// Schriftgroessen bei 1080 p (mit S() skaliert): Titel, Listenzeile, Fliesstext, Hinweis, Kapitaelchen
constexpr float kFsTitle = 28.0f, kFsRow = 24.0f, kFsBody = 18.0f, kFsSmall = 15.0f, kFsCaps = 14.0f;

}  // namespace lim::ui::theme
