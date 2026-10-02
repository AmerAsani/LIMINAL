// Kachelbare Rauschfunktionen fuer die prozedurale Texturerzeugung.
// Alle Funktionen sind periodisch (period in Gitterzellen), damit Texturen
// nahtlos wiederholt werden koennen.
#pragma once

#include <cmath>
#include <cstdint>

namespace lim::gfx::noise {

inline std::uint32_t h2(int x, int y, std::uint32_t seed) {
    std::uint32_t h = (std::uint32_t)x * 0x8da6b343u ^ (std::uint32_t)y * 0xd8163841u ^ seed * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}
inline float r01(std::uint32_t h) { return (float)(h & 0xFFFFFFu) * (1.0f / 16777216.0f); }
inline int wrap(int v, int p) {
    int m = v % p;
    return m < 0 ? m + p : m;
}
inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// Periodisches Gradientenrauschen (Perlin), Ergebnis etwa in [-1, 1].
inline float perlin(float x, float y, int px, int py, std::uint32_t seed) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - (float)ix, fy = y - (float)iy;
    auto grad = [&](int gx, int gy, float dx, float dy) {
        std::uint32_t h = h2(wrap(gx, px), wrap(gy, py), seed);
        float a = r01(h) * 6.2831853f;
        return std::cos(a) * dx + std::sin(a) * dy;
    };
    float n00 = grad(ix, iy, fx, fy), n10 = grad(ix + 1, iy, fx - 1, fy);
    float n01 = grad(ix, iy + 1, fx, fy - 1), n11 = grad(ix + 1, iy + 1, fx - 1, fy - 1);
    float u = fade(fx), v = fade(fy);
    float a = n00 + (n10 - n00) * u, b = n01 + (n11 - n01) * u;
    return (a + (b - a) * v) * 1.41421356f;
}

// Fraktales Rauschen in [-1, 1]; u, v in [0, 1), Grundfrequenz f (Zellen je Kachel).
inline float fbm(float u, float v, int f, int octaves, std::uint32_t seed, float gain = 0.5f) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum += perlin(u * (float)f, v * (float)f, f, f, seed + (std::uint32_t)o * 131u) * amp;
        norm += amp;
        amp *= gain;
        f *= 2;
    }
    return sum / norm;
}

// Wertrauschen (glatt, periodisch) in [0, 1].
inline float value(float x, float y, int px, int py, std::uint32_t seed) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - (float)ix, fy = y - (float)iy;
    float a = r01(h2(wrap(ix, px), wrap(iy, py), seed));
    float b = r01(h2(wrap(ix + 1, px), wrap(iy, py), seed));
    float c = r01(h2(wrap(ix, px), wrap(iy + 1, py), seed));
    float d = r01(h2(wrap(ix + 1, px), wrap(iy + 1, py), seed));
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
}

// Zellrauschen (Worley): Abstand zum naechsten und zweitnaechsten Punkt, Zell-ID.
struct Cell {
    float d1, d2;
    std::uint32_t id;
};
inline Cell worley(float u, float v, int f, std::uint32_t seed) {
    float x = u * (float)f, y = v * (float)f;
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    Cell c{9.0f, 9.0f, 0};
    for (int oy = -1; oy <= 1; ++oy)
        for (int ox = -1; ox <= 1; ++ox) {
            int cx = ix + ox, cy = iy + oy;
            std::uint32_t h = h2(wrap(cx, f), wrap(cy, f), seed);
            float px = (float)cx + r01(h), py = (float)cy + r01(h * 747796405u + 1u);
            float d = std::sqrt((px - x) * (px - x) + (py - y) * (py - y));
            if (d < c.d1) {
                c.d2 = c.d1;
                c.d1 = d;
                c.id = h;
            } else if (d < c.d2) {
                c.d2 = d;
            }
        }
    return c;
}

inline float saturate(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline float smooth(float a, float b, float x) {
    float t = saturate((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}
inline float mix(float a, float b, float t) { return a + (b - a) * t; }
// Abstand zur naechsten Fuge eines Rasters mit Periode p (in u), Ergebnis in Einheiten von u.
inline float lineDist(float u, float p) {
    float f = u / p - std::floor(u / p);
    return std::fmin(f, 1.0f - f) * p;
}

}  // namespace lim::gfx::noise
