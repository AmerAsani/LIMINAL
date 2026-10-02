// 2D-Zeichnen fuer Menues und HUD: abgerundete Flaechen, Text, Bilder.
// Alle Aufrufe eines Bildes werden gesammelt und in wenigen Draw-Calls gezeichnet.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/math.hpp"
#include "render/font.hpp"
#include "render/gpu.hpp"

namespace lim::gfx {

struct ShaderSet;

enum class Align : u8 { Left, Center, Right };

class UiRenderer {
public:
    bool init(Gpu& gpu, const ShaderSet& shaders, const FontAtlas& font);
    void begin(int width, int height);
    void render(Gpu& gpu);

    int width() const { return width_; }
    int height() const { return height_; }

    // Farben in sRGB (0..1) mit gerader Deckkraft.
    void rect(float x, float y, float w, float h, vec4 color, float radius = 0.0f, float border = 0.0f,
              vec4 borderColor = {0, 0, 0, 0});
    // y ist die Oberkante der Zeile. Liefert die Breite.
    float text(float x, float y, float size, std::string_view utf8, vec4 color, FontFace face = FontFace::Regular,
               Align align = Align::Left);
    float textWidth(std::string_view utf8, float size, FontFace face = FontFace::Regular) const;
    float lineHeight(float size, FontFace face = FontFace::Regular) const { return font_->lineHeight(face) * size; }
    bool hasGlyph(char32_t c) const { return font_ && font_->has(c); }
    // Zeilenumbruch nach Woertern
    std::vector<std::string> wrap(std::string_view utf8, float size, float maxWidth, FontFace face = FontFace::Regular) const;
    void image(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, vec4 tint = {1, 1, 1, 1},
               vec4 uv = {0, 0, 1, 1});
    // V6: rundes Bildausschnitt (Minimap). Kreis um (cx, cy) mit Radius r zeigt den Bildbereich
    // um uvCenter mit Radius uvRadius, gedreht um angle (rad, im Uhrzeigersinn). Weicher Rand.
    void imageDisc(ID3D11ShaderResourceView* srv, float cx, float cy, float r, vec2 uvCenter, float uvRadius, float angle,
                   vec4 tint = {1, 1, 1, 1});
    // V6: Richtungspfeil (Spielerposition) mit dunklem Rand; angle 0 = nach oben, im Uhrzeigersinn
    void arrow(float cx, float cy, float size, float angle, vec4 color, vec4 outline);
    // V6: weicher Sichtkegel (halber Oeffnungswinkel halfAngle), angle wie bei arrow
    void cone(float cx, float cy, float radius, float angle, float halfAngle, vec4 color);
    void pushClip(float x, float y, float w, float h);
    void popClip();
    // V6: Deckkraft-Faktor fuer alle folgenden Zeichenbefehle (z. B. HUD dezent ausblenden)
    void setAlpha(float a) { alphaMul_ = std::clamp(a, 0.0f, 1.0f); }
    float alpha() const { return alphaMul_; }

private:
    struct Vertex {
        vec2 pos, uv;
        vec4 color, rect, extra, border;
    };
    struct Batch {
        u32 first = 0, count = 0;  // Quads
        ID3D11ShaderResourceView* image = nullptr;
        D3D11_RECT clip{};
    };
    void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, vec4 col, vec4 rect,
              float mode, vec4 border, ID3D11ShaderResourceView* img, float sharp = 1.0f);
    Batch& currentBatch(ID3D11ShaderResourceView* img);

    const ShaderSet* shaders_ = nullptr;
    const FontAtlas* font_ = nullptr;
    Texture fontTex_;
    Buffer vb_, ib_, cb_;
    u32 capacity_ = 0;
    std::vector<Vertex> verts_;
    std::vector<Batch> batches_;
    std::vector<D3D11_RECT> clips_;
    int width_ = 0, height_ = 0;
    float alphaMul_ = 1.0f;
};

vec4 srgbToLinear(vec4 c);

}  // namespace lim::gfx
