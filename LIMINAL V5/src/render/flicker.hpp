// Flackern der Raumbeleuchtung (identisch zu flickerFactor in shaders/common.hlsli).
// Modus 0: ruhig, 1: kaum wahrnehmbares Pulsieren, 2: alternde Leuchtstoffroehre
// mit weichen Einbruechen (wie in V4, update_flicker).
#pragma once

#include <cmath>

#include "core/rng.hpp"

namespace lim::gfx {

inline float flickerNoise(u32 seed, float t) {
    float i = std::floor(t);
    float f = t - i;
    u32 ii = (u32)(i32)i;
    float a = hash01(seed * 7919u + ii);
    float b = hash01(seed * 7919u + ii + 1u);
    float u = f * f * (3.0f - 2.0f * f);
    return a + (b - a) * u;
}

inline float flickerFactor(u32 mode, u32 seed, float t) {
    float phase = (float)seed * 0.2f;
    if (mode == 1) return 0.985f + 0.015f * std::sin(t * 1.6f + phase);
    if (mode == 2) {
        float n = flickerNoise(seed + 101u, t * 1.1f);
        float dip = n > 0.62f ? (n - 0.62f) / 0.38f : 0.0f;
        return (1.0f - 0.38f * dip * dip) * (0.975f + 0.025f * std::sin(t * 0.8f + phase));
    }
    return 1.0f;
}

}  // namespace lim::gfx
