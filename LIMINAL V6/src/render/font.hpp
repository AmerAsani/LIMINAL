// Schrift fuer die Oberflaeche: Glyphen werden beim Start mit den
// Windows-Schriften (Segoe UI, Consolas) gerastert und in ein
// Distanzfeld-Atlas umgerechnet. Text bleibt dadurch in jeder Groesse
// scharf (4K, Skalierung), ohne Schriftdateien mitzuliefern.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/core.hpp"

namespace lim::gfx {

enum class FontFace : u8 { Regular = 0, Bold = 1, Mono = 2, Count = 3 };

struct Glyph {
    float u0, v0, u1, v1;   // Atlas-Koordinaten
    float x0, y0, x1, y1;   // Rechteck relativ zur Grundlinie in em-Einheiten (y nach unten)
    float advance;          // em-Einheiten
};

class FontAtlas {
public:
    // Erzeugt den Atlas (CPU). pixelsPerEm: Aufloesung im Atlas.
    bool build(int pixelsPerEm = 48);
    const Glyph* glyph(FontFace face, char32_t c) const;
    bool has(char32_t c) const { return glyphs_[0].count((u32)c) != 0; }
    float lineHeight(FontFace face) const { return lineHeight_[(int)face]; }
    float ascent(FontFace face) const { return ascent_[(int)face]; }
    float spread() const { return spreadEm_; }

    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<u8>& pixels() const { return pixels_; }  // R8 Distanzfeld

    // Zeichen fuer die Retro-Darstellungen (16 Felder a 32x64 px, R8 Deckung)
    std::vector<u8> buildGlyphStrip(int& w, int& h) const;

private:
    std::unordered_map<u32, Glyph> glyphs_[(int)FontFace::Count];
    float lineHeight_[(int)FontFace::Count] = {1.3f, 1.3f, 1.2f};
    float ascent_[(int)FontFace::Count] = {1.0f, 1.0f, 1.0f};
    float spreadEm_ = 0.15f;
    int width_ = 0, height_ = 0;
    std::vector<u8> pixels_;
};

// UTF-8 -> UTF-32
std::u32string utf8To32(const std::string& s);
std::string utf32To8(const std::u32string& s);

}  // namespace lim::gfx
