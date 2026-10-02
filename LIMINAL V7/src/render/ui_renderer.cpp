#include "render/ui_renderer.hpp"

#include <cmath>

#include "render/shaders.hpp"

namespace lim::gfx {

vec4 srgbToLinear(vec4 c) {
    auto f = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    return {f(c.x), f(c.y), f(c.z), c.w};
}

static vec4 premul(vec4 srgb) {
    vec4 l = srgbToLinear(srgb);
    return {l.x * l.w, l.y * l.w, l.z * l.w, l.w};
}

bool UiRenderer::init(Gpu& gpu, const ShaderSet& shaders, const FontAtlas& font) {
    shaders_ = &shaders;
    font_ = &font;
    D3D11_SUBRESOURCE_DATA sd{font.pixels().data(), (UINT)font.width(), 0};
    fontTex_ = gpu.createTexture(font.width(), font.height(), DXGI_FORMAT_R8_UNORM, TEX_SRV, 1, 1, &sd);
    capacity_ = 16384;  // Quads
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = capacity_ * 4 * sizeof(Vertex);
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    gpu.dev()->CreateBuffer(&d, nullptr, vb_.buf.put());
    vb_.size = d.ByteWidth;
    std::vector<u32> idx((size_t)capacity_ * 6);
    for (u32 q = 0; q < capacity_; ++q) {
        u32 b = q * 4;
        u32* p = &idx[(size_t)q * 6];
        p[0] = b, p[1] = b + 1, p[2] = b + 2, p[3] = b, p[4] = b + 2, p[5] = b + 3;
    }
    ib_ = gpu.createIndex(idx.data(), (u32)(idx.size() * 4));
    cb_ = gpu.createConstant(16);
    return fontTex_.valid() && vb_.valid();
}

void UiRenderer::begin(int w, int h) {
    width_ = w;
    height_ = h;
    alphaMul_ = 1.0f;
    verts_.clear();
    batches_.clear();
    clips_.clear();
    clips_.push_back({0, 0, w, h});
}

UiRenderer::Batch& UiRenderer::currentBatch(ID3D11ShaderResourceView* img) {
    const D3D11_RECT& clip = clips_.back();
    if (!batches_.empty()) {
        Batch& b = batches_.back();
        bool sameClip = b.clip.left == clip.left && b.clip.top == clip.top && b.clip.right == clip.right &&
                        b.clip.bottom == clip.bottom;
        if (sameClip && (img == nullptr || b.image == nullptr || b.image == img)) {
            if (img) b.image = img;
            return b;
        }
    }
    Batch nb;
    nb.first = (u32)(verts_.size() / 4);
    nb.image = img;
    nb.clip = clip;
    batches_.push_back(nb);
    return batches_.back();
}

void UiRenderer::quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, vec4 col,
                      vec4 rect, float mode, vec4 border, ID3D11ShaderResourceView* img, float sharp) {
    if (verts_.size() / 4 >= capacity_) return;
    if (alphaMul_ < 1.0f) col = col * alphaMul_, border = border * alphaMul_;  // vormultipliziert: alle Kanaele
    Batch& b = currentBatch(img);
    float w = x1 - x0, h = y1 - y0;
    const float lx[4] = {0, w, w, 0}, ly[4] = {0, 0, h, h};
    const float px[4] = {x0, x1, x1, x0}, py[4] = {y0, y0, y1, y1};
    const float us[4] = {u0, u1, u1, u0}, vs[4] = {v0, v0, v1, v1};
    for (int i = 0; i < 4; ++i)
        verts_.push_back({{px[i], py[i]}, {us[i], vs[i]}, col, rect, {mode, lx[i], ly[i], sharp}, border});
    ++b.count;
}

void UiRenderer::rect(float x, float y, float w, float h, vec4 color, float radius, float border, vec4 borderColor) {
    if (w <= 0 || h <= 0 || (color.w <= 0.001f && (border <= 0 || borderColor.w <= 0.001f))) return;
    quad(x, y, x + w, y + h, 0, 0, 1, 1, premul(color), {w, h, radius, border}, 0.0f, premul(borderColor), nullptr);
}

float UiRenderer::textWidth(std::string_view s, float size, FontFace face) const {
    float w = 0;
    for (char32_t c : utf8To32(std::string(s))) {
        const Glyph* g = font_->glyph(face, c);
        if (g) w += g->advance * size;
    }
    return w;
}

float UiRenderer::text(float x, float y, float size, std::string_view s, vec4 color, FontFace face, Align align) {
    float w = textWidth(s, size, face);
    if (align == Align::Center) x -= w * 0.5f;
    else if (align == Align::Right) x -= w;
    x = std::round(x);
    float base = std::round(y + font_->ascent(face) * size);
    vec4 col = premul(color);
    // Schaerfe: kleine Schrift etwas weicher, grosse knackiger
    float sharp = size < 16 ? 0.85f : 1.0f;
    float pen = x;
    for (char32_t c : utf8To32(std::string(s))) {
        const Glyph* g = font_->glyph(face, c);
        if (!g) continue;
        if (g->u1 > g->u0)
            quad(pen + g->x0 * size, base + g->y0 * size, pen + g->x1 * size, base + g->y1 * size, g->u0, g->v0, g->u1,
                 g->v1, col, {0, 0, 0, 0}, 1.0f, {0, 0, 0, 0}, nullptr, sharp);
        pen += g->advance * size;
    }
    return w;
}

std::vector<std::string> UiRenderer::wrap(std::string_view s, float size, float maxWidth, FontFace face) const {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flushWord = [&] {
        if (word.empty()) return;
        std::string trial = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && textWidth(trial, size, face) > maxWidth) {
            lines.push_back(cur);
            cur = word;
        } else {
            cur = trial;
        }
        word.clear();
    };
    for (char c : s) {
        if (c == ' ') flushWord();
        else if (c == '\n') {
            flushWord();
            lines.push_back(cur);
            cur.clear();
        } else word += c;
    }
    flushWord();
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

void UiRenderer::image(ID3D11ShaderResourceView* srv, float x, float y, float w, float h, vec4 tint, vec4 uv) {
    if (!srv) return;
    quad(x, y, x + w, y + h, uv.x, uv.y, uv.z, uv.w, premul(tint), {w, h, 0, 0}, 2.0f, {0, 0, 0, 0}, srv);
}

void UiRenderer::imageDisc(ID3D11ShaderResourceView* srv, float cx, float cy, float r, vec2 uvCenter, float uvRadius,
                           float angle, vec4 tint) {
    if (!srv || r <= 0) return;
    // uv: lokale Koordinaten -1..1; rect: Bildmitte (u, v), Bildradius, Drehung
    quad(cx - r, cy - r, cx + r, cy + r, -1, -1, 1, 1, premul(tint), {uvCenter.x, uvCenter.y, uvRadius, angle}, 3.0f,
         {0, 0, 0, 0}, srv);
}

void UiRenderer::arrow(float cx, float cy, float size, float angle, vec4 color, vec4 outline) {
    quad(cx - size, cy - size, cx + size, cy + size, -1, -1, 1, 1, premul(color), {size, 0, angle, 0}, 4.0f, premul(outline),
         nullptr);
}

void UiRenderer::cone(float cx, float cy, float radius, float angle, float halfAngle, vec4 color) {
    quad(cx - radius, cy - radius, cx + radius, cy + radius, -1, -1, 1, 1, premul(color), {radius, halfAngle, angle, 0}, 5.0f,
         {0, 0, 0, 0}, nullptr);
}

void UiRenderer::pushClip(float x, float y, float w, float h) {
    const D3D11_RECT& p = clips_.back();
    D3D11_RECT r{std::max<LONG>(p.left, (LONG)x), std::max<LONG>(p.top, (LONG)y), std::min<LONG>(p.right, (LONG)(x + w)),
                 std::min<LONG>(p.bottom, (LONG)(y + h))};
    clips_.push_back(r);
}

void UiRenderer::popClip() {
    if (clips_.size() > 1) clips_.pop_back();
}

void UiRenderer::render(Gpu& gpu) {
    if (verts_.empty()) return;
    auto* ctx = gpu.ctx();
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(vb_.buf.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    std::memcpy(m.pData, verts_.data(), verts_.size() * sizeof(Vertex));
    ctx->Unmap(vb_.buf.get(), 0);
    float vp[4] = {(float)width_, (float)height_, 0, 0};
    gpu.update(cb_, vp, sizeof(vp));

    ID3D11RenderTargetView* rtv = gpu.backbufferRtv();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    gpu.setViewport(width_, height_);
    float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(gpu.bsPremul(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(gpu.dsNone(), 0);
    ctx->RSSetState(gpu.rsScissor());
    ctx->IASetInputLayout(shaders_->uiLayout.get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(Vertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, vb_.buf.addr(), &stride, &offset);
    ctx->IASetIndexBuffer(ib_.buf.get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->VSSetShader(shaders_->uiVS.get(), nullptr, 0);
    ctx->PSSetShader(shaders_->uiPS.get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, cb_.buf.addr());
    ctx->PSSetConstantBuffers(0, 1, cb_.buf.addr());
    gpu.bindCommonSamplers();
    for (const Batch& b : batches_) {
        if (!b.count) continue;
        ID3D11ShaderResourceView* srvs[2] = {fontTex_.srv.get(), b.image};
        ctx->PSSetShaderResources(0, 2, srvs);
        ctx->RSSetScissorRects(1, &b.clip);
        ctx->DrawIndexed(b.count * 6, b.first * 6, 0);
    }
    ctx->RSSetState(gpu.rsCullBack());
}

}  // namespace lim::gfx
