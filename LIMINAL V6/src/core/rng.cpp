#include "core/rng.hpp"

#include <cmath>

namespace lim {

static constexpr std::uint64_t kGolden = 0x9E3779B97F4A7C15ULL;

std::uint64_t mix64(std::uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

HashArg::HashArg(std::string_view s) {
    std::uint64_t h = 0xCBF29CE484222325ULL;
    for (unsigned char b : s) h = (h ^ b) * 0x100000001B3ULL;
    v = h;
}

std::uint64_t hashSeq(const HashArg* args, std::size_t n) {
    std::uint64_t h = 0x6A09E667F3BCC909ULL;
    for (std::size_t i = 0; i < n; ++i) h = mix64((h ^ args[i].v) + kGolden);
    return h;
}

std::uint64_t Rng::next64() {
    state_ += kGolden;
    return mix64(state_);
}

double Rng::random() { return (double)(next64() >> 11) * (1.0 / 9007199254740992.0); }

std::int64_t Rng::randint(std::int64_t a, std::int64_t b) {
    if (b <= a) return a;
    return a + (std::int64_t)(next64() % (std::uint64_t)(b - a + 1));
}

std::size_t Rng::weighted(const double* w, std::size_t n) {
    double total = 0.0;
    for (std::size_t i = 0; i < n; ++i) total += w[i];
    if (total <= 0.0) return 0;
    double r = random() * total;
    for (std::size_t i = 0; i < n; ++i) {
        r -= w[i];
        if (r < 0.0) return i;
    }
    return n - 1;
}

static double smooth(double t) { return t * t * (3.0 - 2.0 * t); }

double valueNoise(std::uint64_t seed, double x, double y) {
    std::int64_t ix = (std::int64_t)std::floor(x);
    std::int64_t iy = (std::int64_t)std::floor(y);
    double ux = smooth(x - (double)ix);
    double uy = smooth(y - (double)iy);
    double a = hfloat(seed, ix, iy);
    double b = hfloat(seed, ix + 1, iy);
    double c = hfloat(seed, ix, iy + 1);
    double d = hfloat(seed, ix + 1, iy + 1);
    double top = a + (b - a) * ux;
    double bottom = c + (d - c) * ux;
    return top + (bottom - top) * uy;
}

double fbm(std::uint64_t seed, double x, double y, int octaves) {
    double total = 0.0, amp = 1.0, norm = 0.0, freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        total += valueNoise(seed + (std::uint64_t)i * 7919, x * freq, y * freq) * amp;
        norm += amp;
        amp *= 0.5;
        freq *= 2.03;
    }
    return total / norm;
}

}  // namespace lim
