// V6: Blockkompression der prozeduralen Texturen (spart Grafikspeicher, BC = DirectX-Standard).
//   Albedo  RGBA8 -> BC1 (4 Bit je Pixel, 8:1)  - Farbe, sRGB
//   Normale RG8   -> BC5 (8 Bit je Pixel, 2:1)  - zwei unabhaengige Kanaele (x, y)
// Rauheit/AO/Metall/Hoehe bleiben unkomprimiert (Hoehe steuert die Parallaxe genau).
// Kodierung: Hauptachse der Blockfarben (Range-Fit), Endpunkte leicht nach innen gezogen;
// schnell genug fuer den Start (parallel je Ebene).
#pragma once

#include <vector>

#include "core/core.hpp"

namespace lim::gfx {

// Ein 4x4-Block RGBA8 (16 Pixel, Zeilen hintereinander) -> 8 Byte BC1
void encodeBC1(const u8* rgba16, u8* out8);
// Ein 4x4-Block eines Kanals (16 Werte) -> 8 Byte BC4
void encodeBC4(const u8* values16, u8* out8);

// Ganze Ebene (w x h, w und h Vielfache von 4)
void compressBC1(const u8* rgba, int w, int h, u8* out);
void compressBC5(const u8* rg, int w, int h, u8* out);

}  // namespace lim::gfx
