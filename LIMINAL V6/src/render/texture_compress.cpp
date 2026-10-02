#include "render/texture_compress.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace lim::gfx {

namespace {

inline u16 to565(float r, float g, float b) {
    int R = std::clamp((int)std::lround(r * 31.0f / 255.0f), 0, 31);
    int G = std::clamp((int)std::lround(g * 63.0f / 255.0f), 0, 63);
    int B = std::clamp((int)std::lround(b * 31.0f / 255.0f), 0, 31);
    return (u16)((R << 11) | (G << 5) | B);
}

inline void from565(u16 c, float out[3]) {
    int R = (c >> 11) & 31, G = (c >> 5) & 63, B = c & 31;
    out[0] = (float)((R << 3) | (R >> 2));
    out[1] = (float)((G << 2) | (G >> 4));
    out[2] = (float)((B << 3) | (B >> 2));
}

}  // namespace

void encodeBC1(const u8* px, u8* out) {
    // Mittelwert und Hauptachse (Potenzmethode auf der Kovarianz)
    float mean[3] = {0, 0, 0};
    for (int i = 0; i < 16; ++i)
        for (int c = 0; c < 3; ++c) mean[c] += px[i * 4 + c];
    for (float& m : mean) m /= 16.0f;
    float cov[6] = {0, 0, 0, 0, 0, 0};  // rr rg rb gg gb bb
    for (int i = 0; i < 16; ++i) {
        float r = px[i * 4] - mean[0], g = px[i * 4 + 1] - mean[1], b = px[i * 4 + 2] - mean[2];
        cov[0] += r * r, cov[1] += r * g, cov[2] += r * b, cov[3] += g * g, cov[4] += g * b, cov[5] += b * b;
    }
    float axis[3] = {1.0f, 1.0f, 1.0f};
    for (int it = 0; it < 6; ++it) {
        float x = cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2];
        float y = cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2];
        float z = cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2];
        float len = std::sqrt(x * x + y * y + z * z);
        if (len < 1e-6f) break;
        axis[0] = x / len, axis[1] = y / len, axis[2] = z / len;
    }
    float tmin = 1e9f, tmax = -1e9f;
    for (int i = 0; i < 16; ++i) {
        float t = (px[i * 4] - mean[0]) * axis[0] + (px[i * 4 + 1] - mean[1]) * axis[1] + (px[i * 4 + 2] - mean[2]) * axis[2];
        tmin = std::min(tmin, t), tmax = std::max(tmax, t);
    }
    // leicht nach innen ziehen (bessere Mitte, weniger Ausreisser-Einfluss)
    float inset = (tmax - tmin) / 16.0f;
    tmin += inset, tmax -= inset;
    float e0[3], e1[3];
    for (int c = 0; c < 3; ++c) e0[c] = mean[c] + axis[c] * tmax, e1[c] = mean[c] + axis[c] * tmin;
    u16 c0 = to565(e0[0], e0[1], e0[2]), c1 = to565(e1[0], e1[1], e1[2]);
    u32 indices = 0;
    if (c0 == c1) {
        // einfarbig: alle Pixel auf Endpunkt 0 (4-Farben-Modus braucht c0 > c1)
        if (c0 > 0) c1 = (u16)(c0 - 1);
        else c0 = 1, c1 = 0;
        float p[3];
        from565(c0, p);
        float q[3];
        from565(c1, q);
        for (int i = 0; i < 16; ++i) {
            float d0 = 0, d1 = 0;
            for (int c = 0; c < 3; ++c) d0 += (px[i * 4 + c] - p[c]) * (px[i * 4 + c] - p[c]), d1 += (px[i * 4 + c] - q[c]) * (px[i * 4 + c] - q[c]);
            indices |= (u32)(d1 < d0 ? 1 : 0) << (i * 2);
        }
    } else {
        if (c0 < c1) std::swap(c0, c1);
        float pal[4][3];
        from565(c0, pal[0]);
        from565(c1, pal[1]);
        for (int c = 0; c < 3; ++c) {
            pal[2][c] = (2.0f * pal[0][c] + pal[1][c]) / 3.0f;
            pal[3][c] = (pal[0][c] + 2.0f * pal[1][c]) / 3.0f;
        }
        for (int i = 0; i < 16; ++i) {
            int best = 0;
            float bd = 1e30f;
            for (int k = 0; k < 4; ++k) {
                float d = 0;
                for (int c = 0; c < 3; ++c) d += (px[i * 4 + c] - pal[k][c]) * (px[i * 4 + c] - pal[k][c]);
                if (d < bd) bd = d, best = k;
            }
            indices |= (u32)best << (i * 2);
        }
    }
    out[0] = (u8)(c0 & 255), out[1] = (u8)(c0 >> 8), out[2] = (u8)(c1 & 255), out[3] = (u8)(c1 >> 8);
    std::memcpy(out + 4, &indices, 4);
}

void encodeBC4(const u8* v, u8* out) {
    int lo = 255, hi = 0;
    for (int i = 0; i < 16; ++i) lo = std::min<int>(lo, v[i]), hi = std::max<int>(hi, v[i]);
    out[0] = (u8)hi, out[1] = (u8)lo;  // a0 > a1: 8-Werte-Modus (bei a0 == a1 sind alle Indizes 0)
    u64 bits = 0;
    if (hi > lo) {
        float pal[8];
        pal[0] = (float)hi, pal[1] = (float)lo;
        for (int k = 1; k <= 6; ++k) pal[k + 1] = ((7 - k) * (float)hi + k * (float)lo) / 7.0f;
        for (int i = 0; i < 16; ++i) {
            int best = 0;
            float bd = 1e30f;
            for (int k = 0; k < 8; ++k) {
                float d = std::fabs((float)v[i] - pal[k]);
                if (d < bd) bd = d, best = k;
            }
            bits |= (u64)best << (i * 3);
        }
    }
    for (int b = 0; b < 6; ++b) out[2 + b] = (u8)(bits >> (b * 8));
}

void compressBC1(const u8* rgba, int w, int h, u8* out) {
    u8 block[64];
    const int bw = w / 4, bh = h / 4;
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            for (int y = 0; y < 4; ++y) std::memcpy(block + y * 16, rgba + ((size_t)(by * 4 + y) * w + bx * 4) * 4, 16);
            encodeBC1(block, out + ((size_t)by * bw + bx) * 8);
        }
}

void compressBC5(const u8* rg, int w, int h, u8* out) {
    u8 r[16], g[16];
    const int bw = w / 4, bh = h / 4;
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x) {
                    const u8* p = rg + ((size_t)(by * 4 + y) * w + bx * 4 + x) * 2;
                    r[y * 4 + x] = p[0], g[y * 4 + x] = p[1];
                }
            u8* o = out + ((size_t)by * bw + bx) * 16;
            encodeBC4(r, o);
            encodeBC4(g, o + 8);
        }
}

}  // namespace lim::gfx
