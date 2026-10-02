#include "render/font.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/log.hpp"

#include <windows.h>

namespace lim::gfx {

std::u32string utf8To32(const std::string& s) {
    std::u32string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        char32_t cp;
        int n;
        if (c < 0x80) cp = c, n = 1;
        else if ((c >> 5) == 6) cp = c & 0x1F, n = 2;
        else if ((c >> 4) == 14) cp = c & 0x0F, n = 3;
        else if ((c >> 3) == 30) cp = c & 0x07, n = 4;
        else {
            ++i;
            continue;
        }
        if (i + (size_t)n > s.size()) break;
        for (int k = 1; k < n; ++k) cp = (cp << 6) | ((unsigned char)s[i + (size_t)k] & 0x3F);
        out.push_back(cp);
        i += (size_t)n;
    }
    return out;
}

std::string utf32To8(const std::u32string& s) {
    std::string out;
    for (char32_t cp : s) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

namespace {

// 1D-Distanztransformation (Felzenszwalb & Huttenlocher), quadrierte Abstaende
void edt1d(const float* f, float* d, int n, std::vector<int>& v, std::vector<float>& z) {
    int k = 0;
    v[0] = 0;
    z[0] = -std::numeric_limits<float>::infinity();
    z[1] = std::numeric_limits<float>::infinity();
    for (int q = 1; q < n; ++q) {
        float s;
        while (true) {
            s = ((f[q] + (float)q * q) - (f[v[(size_t)k]] + (float)v[(size_t)k] * v[(size_t)k])) / (2.0f * q - 2.0f * v[(size_t)k]);
            if (s <= z[(size_t)k] && k > 0) --k;
            else break;
        }
        ++k;
        v[(size_t)k] = q;
        z[(size_t)k] = s;
        z[(size_t)k + 1] = std::numeric_limits<float>::infinity();
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[(size_t)k + 1] < (float)q) ++k;
        float dq = (float)(q - v[(size_t)k]);
        d[q] = dq * dq + f[v[(size_t)k]];
    }
}

// Quadrierter Abstand jedes Pixels zum naechsten "true"-Pixel
std::vector<float> edt2d(const std::vector<u8>& mask, int w, int h) {
    const float INF = 1e20f;
    std::vector<float> g((size_t)w * h);
    for (size_t i = 0; i < g.size(); ++i) g[i] = mask[i] ? 0.0f : INF;
    int n = std::max(w, h);
    std::vector<float> f((size_t)n), d((size_t)n), zz((size_t)n + 1);
    std::vector<int> v((size_t)n);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) f[(size_t)y] = g[(size_t)y * w + x];
        edt1d(f.data(), d.data(), h, v, zz);
        for (int y = 0; y < h; ++y) g[(size_t)y * w + x] = d[(size_t)y];
    }
    for (int y = 0; y < h; ++y) {
        edt1d(&g[(size_t)y * w], d.data(), w, v, zz);
        std::copy(d.begin(), d.begin() + w, g.begin() + (size_t)y * w);
    }
    return g;
}

std::u32string charset() {
    std::u32string s;
    for (char32_t c = 32; c < 127; ++c) s.push_back(c);
    const char32_t extra[] = {U'Ä', U'Ö', U'Ü', U'ä', U'ö', U'ü', U'ß', U'é', U'è', U'à', U'°', U'·', U'–', U'—', U'…',
                              U'←', U'↑', U'→', U'↓', U'▸', U'◂', U'▲', U'▼', U'♥', U'─', U'⚡', U'×', U'■', U'◆', U'•',
                              U'‹', U'›', U'„', U'“', U'”', U'‚', U'‘', U'’', U'€', U'²', U'³', U'µ', U'±', U'»', U'«',
                              U'✓', U'✗', U'½', U'§', U'¼', U'−', U'≈', U'≤', U'≥', U'±', U'©', U'®', U'∞', U'°', U'…', U'⏎', U'⌫', U'▶', U'◀', U'●', U'○', U'□'};
    for (char32_t c : extra) s.push_back(c);
    s.push_back(U'▌');  // Textcursor
    // Lateinische Buchstaben mit Akzenten (Latin-1: À..ÿ, Latin Extended-A: Ā..ž) fuer Spielernamen
    for (char32_t c = 0xC0; c <= 0x17F; ++c)
        if (s.find(c) == std::u32string::npos) s.push_back(c);
    return s;
}

}  // namespace

bool FontAtlas::build(int ppem) {
    const int S = ppem * 2;                       // Rasteraufloesung (2x ueberabgetastet)
    const int spreadHi = (int)std::lround(spreadEm_ * S);
    HDC dc = CreateCompatibleDC(nullptr);
    struct FaceDef {
        const wchar_t* name;
        int weight;
        const wchar_t* fallback;
    };
    const FaceDef faces[] = {{L"Segoe UI", FW_NORMAL, L"Segoe UI Symbol"},
                             {L"Segoe UI Semibold", FW_SEMIBOLD, L"Segoe UI Symbol"},
                             {L"Consolas", FW_NORMAL, L"Segoe UI Symbol"}};
    struct Pending {
        int face;
        char32_t c;
        std::vector<u8> sdf;  // ppem-Aufloesung
        int w, h;
        Glyph g;
    };
    std::vector<Pending> all;
    const std::u32string chars = charset();
    MAT2 ident{{0, 1}, {0, 0}, {0, 0}, {0, 1}};

    for (int fi = 0; fi < (int)FontFace::Count; ++fi) {
        HFONT font = CreateFontW(-S, 0, 0, 0, faces[fi].weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, DEFAULT_PITCH, faces[fi].name);
        HFONT fb = CreateFontW(-S, 0, 0, 0, faces[fi].weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, DEFAULT_PITCH, faces[fi].fallback);
        SelectObject(dc, font);
        TEXTMETRICW tm{};
        GetTextMetricsW(dc, &tm);
        ascent_[fi] = (float)tm.tmAscent / S;
        lineHeight_[fi] = (float)(tm.tmHeight + tm.tmExternalLeading) / S;
        for (char32_t c : chars) {
            wchar_t wc = (wchar_t)c;
            WORD gi = 0;
            SelectObject(dc, font);
            GetGlyphIndicesW(dc, &wc, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS);
            if (gi == 0xFFFF) SelectObject(dc, fb);
            GLYPHMETRICS gm{};
            DWORD size = GetGlyphOutlineW(dc, (UINT)c, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &ident);
            Pending p;
            p.face = fi;
            p.c = c;
            p.g.advance = (float)gm.gmCellIncX / S;
            if (size == GDI_ERROR || size == 0 || c == U' ') {
                p.w = p.h = 0;
                p.g.x0 = p.g.x1 = p.g.y0 = p.g.y1 = 0;
                all.push_back(std::move(p));
                continue;
            }
            std::vector<u8> buf(size);
            GetGlyphOutlineW(dc, (UINT)c, GGO_GRAY8_BITMAP, &gm, size, buf.data(), &ident);
            int bw = (int)gm.gmBlackBoxX, bh = (int)gm.gmBlackBoxY;
            int pitch = (bw + 3) & ~3;
            // Auf gerade Groesse mit Rand bringen (fuer die 2x-Verkleinerung)
            int W = bw + 2 * spreadHi, H = bh + 2 * spreadHi;
            W += W & 1;
            H += H & 1;
            std::vector<u8> in((size_t)W * H, 0), out((size_t)W * H, 0);
            for (int y = 0; y < bh; ++y)
                for (int x = 0; x < bw; ++x) {
                    bool on = buf[(size_t)y * pitch + x] >= 32;  // 0..64
                    in[(size_t)(y + spreadHi) * W + x + spreadHi] = on;
                    out[(size_t)(y + spreadHi) * W + x + spreadHi] = !on;
                }
            for (size_t i = 0; i < out.size(); ++i)
                if (!in[i]) out[i] = 1;
            auto dOut = edt2d(in, W, H);    // Abstand ausserhalb zur Form
            auto dIn = edt2d(out, W, H);    // Abstand innerhalb zum Rand
            int w2 = W / 2, h2 = H / 2;
            p.w = w2;
            p.h = h2;
            p.sdf.resize((size_t)w2 * h2);
            for (int y = 0; y < h2; ++y)
                for (int x = 0; x < w2; ++x) {
                    float acc = 0;
                    for (int k = 0; k < 4; ++k) {
                        size_t i = (size_t)(2 * y + (k >> 1)) * W + (2 * x + (k & 1));
                        float sd = std::sqrt(dOut[i]) - std::sqrt(dIn[i]);  // > 0 ausserhalb
                        acc += 0.5f - sd / (2.0f * spreadHi);
                    }
                    p.sdf[(size_t)y * w2 + x] = (u8)std::clamp((int)std::lround(acc * 0.25f * 255.0f), 0, 255);
                }
            p.g.x0 = ((float)gm.gmptGlyphOrigin.x - spreadHi) / S;
            p.g.y0 = (-(float)gm.gmptGlyphOrigin.y - spreadHi) / S;
            p.g.x1 = p.g.x0 + (float)W / S;
            p.g.y1 = p.g.y0 + (float)H / S;
            all.push_back(std::move(p));
        }
        DeleteObject(font);
        DeleteObject(fb);
    }
    DeleteDC(dc);

    // In Zeilen packen
    width_ = 1024;
    int x = 1, y = 1, rowH = 0;
    for (auto& p : all) {
        if (!p.w) continue;
        if (x + p.w + 1 > width_) {
            x = 1;
            y += rowH + 1;
            rowH = 0;
        }
        p.g.u0 = (float)x;
        p.g.v0 = (float)y;
        x += p.w + 1;
        rowH = std::max(rowH, p.h);
    }
    height_ = 1;
    while (height_ < y + rowH + 1) height_ *= 2;
    pixels_.assign((size_t)width_ * height_, 0);
    for (auto& p : all) {
        if (p.w) {
            int gx = (int)p.g.u0, gy = (int)p.g.v0;
            for (int yy = 0; yy < p.h; ++yy)
                std::copy(p.sdf.begin() + (size_t)yy * p.w, p.sdf.begin() + (size_t)(yy + 1) * p.w,
                          pixels_.begin() + (size_t)(gy + yy) * width_ + gx);
            p.g.u0 = (float)gx / width_;
            p.g.v0 = (float)gy / height_;
            p.g.u1 = (float)(gx + p.w) / width_;
            p.g.v1 = (float)(gy + p.h) / height_;
        }
        glyphs_[p.face][(u32)p.c] = p.g;
    }
    log::info("Schriftatlas: {} Glyphen, {}x{} px", all.size(), width_, height_);
    return true;
}

const Glyph* FontAtlas::glyph(FontFace face, char32_t c) const {
    const auto& m = glyphs_[(int)face];
    auto it = m.find((u32)c);
    if (it != m.end()) return &it->second;
    it = m.find((u32)U'?');
    return it != m.end() ? &it->second : nullptr;
}

std::vector<u8> FontAtlas::buildGlyphStrip(int& w, int& h) const {
    // 8 Stufen wie das ASCII-Raster aus V4, 10 Stufen Monochrom, 2 Reserve
    const wchar_t* ramp[20] = {L" ", L"\x00B7", L".", L":", L"\x2591", L"\x2592", L"\x2593", L"\x2588",
                               L" ", L".", L":", L"-", L"=", L"+", L"*", L"#", L"%", L"@", L" ", L" "};
    const int cw = 32, ch = 64;
    w = cw * 20;
    h = ch;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(dc, bmp);
    HFONT font = CreateFontW(-56, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
    SelectObject(dc, font);
    SetBkColor(dc, RGB(0, 0, 0));
    SetTextColor(dc, RGB(255, 255, 255));
    SetTextAlign(dc, TA_CENTER | TA_TOP);
    RECT all{0, 0, w, h};
    FillRect(dc, &all, (HBRUSH)GetStockObject(BLACK_BRUSH));
    for (int i = 0; i < 20; ++i) {
        // Vollblock als echtes Rechteck (Schriften zeichnen ihn nicht immer randlos)
        if (ramp[i][0] == 0x2588) {
            RECT r{i * cw, 0, (i + 1) * cw, ch};
            FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
            continue;
        }
        TextOutW(dc, i * cw + cw / 2, 0, ramp[i], 1);
    }
    GdiFlush();
    std::vector<u8> out((size_t)w * h);
    const u8* px = static_cast<const u8*>(bits);
    for (size_t i = 0; i < out.size(); ++i) out[i] = px[i * 4 + 1];
    DeleteObject(font);
    DeleteObject(bmp);
    DeleteDC(dc);
    return out;
}

}  // namespace lim::gfx
