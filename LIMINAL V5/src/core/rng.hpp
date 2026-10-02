// Deterministische Zufallsfunktionen (bitgenauer Port aus V4, rng.py).
//
// Die gesamte Weltgenerierung ist eine reine Funktion von (Seed, Koordinaten).
// Damit entstehen dieselben Welten wie in V4 - Seeds und alte Spielstaende
// bleiben gueltig.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace lim {

// Ein Argument fuer hash64: Ganzzahl oder Text (Text wird per FNV-1a gehasht).
struct HashArg {
    std::uint64_t v;
    HashArg(std::int64_t i) : v((std::uint64_t)i) {}
    HashArg(int i) : v((std::uint64_t)(std::int64_t)i) {}
    HashArg(std::uint64_t u) : v(u) {}
    HashArg(std::string_view s);
    HashArg(const char* s) : HashArg(std::string_view(s)) {}
};

std::uint64_t mix64(std::uint64_t z);
std::uint64_t hashSeq(const HashArg* args, std::size_t n);

template <class... A>
std::uint64_t hash64(A... args) {
    const HashArg arr[] = {HashArg(args)...};
    return hashSeq(arr, sizeof...(A));
}

// Stabiler Pseudozufallswert in [0, 1).
template <class... A>
double hfloat(A... args) {
    return (double)(hash64(args...) >> 11) * (1.0 / 9007199254740992.0);
}

// SplitMix64 wie in V4.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next64();
    double random();
    double uniform(double a, double b) { return a + (b - a) * random(); }
    // Ganzzahl in [a, b], beide inklusive (b <= a liefert a).
    std::int64_t randint(std::int64_t a, std::int64_t b);
    // Index proportional zum Gewicht (wie Rng.weighted in V4).
    std::size_t weighted(const double* weights, std::size_t n);
    std::size_t weighted(const std::vector<double>& w) { return weighted(w.data(), w.size()); }

private:
    std::uint64_t state_;
};

double valueNoise(std::uint64_t seed, double x, double y);
double fbm(std::uint64_t seed, double x, double y, int octaves = 3);

// Schnelle 32-Bit-Hashes fuer Laufzeiteffekte (Flackern, Texturen), nicht fuer die Welt.
inline std::uint32_t hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline float hash01(std::uint32_t x) { return (float)(hash32(x) >> 8) * (1.0f / 16777216.0f); }

}  // namespace lim
