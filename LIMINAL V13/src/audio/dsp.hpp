// V11: Bausteine der Signalverarbeitung - fuer die Klangerzeugung (offline, beim Start) und den
// Mischer (Echtzeit).
//
//   Rauschen      weiss, rosa (Kellet), braun; Zufall mit Normalverteilung
//   Filter        Einpol-Tief/Hochpass, Biquads nach RBJ (inkl. Kuhschwanz- und Glockenfilter),
//                 Zustandsvariablenfilter (TPT, Grenzfrequenz pro Sample modulierbar)
//   Resonanz      modale Resonatorbank: gedaempfte Schwinger je Mode, angeregt von einem Kraftstoss
//   Verzoegerung  Verzoegerungsleitung mit kubischem Lesen (Hermite)
//   Raum          Feedback-Delay-Netzwerk (8 Leitungen, Hadamard) mit frequenzabhaengiger
//                 Nachhallzeit und Modulation; Fruehreflexionen aus den Raummassen
//   Faltung       FFT (Radix 2) und Faltung fuer lange Raumantworten (offline)
//   Dynamik       Begrenzer mit Vorausschau
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace lim::audio::dsp {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1e-9f)); }
inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float smooth01(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
// Daempfungsfaktor je Sample fuer eine 60-dB-Abklingzeit
inline float t60Coef(float t60, float rate) { return std::exp(-6.9078f / std::max(1e-4f, t60 * rate)); }
// Einpol-Koeffizient fuer eine Grenzfrequenz
inline float onePoleCoef(float hz, float rate) { return std::exp(-kTwoPi * clampf(hz, 1.0f, rate * 0.49f) / rate); }

// --- Zufall und Rauschen ----------------------------------------------------------------------
struct Rand {
    std::uint32_t s;
    explicit Rand(std::uint32_t seed = 1) : s(seed * 747796405u + 2891336453u) {
        if (!s) s = 1;
    }
    std::uint32_t u32() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uni() { return (float)(u32() >> 8) * (1.0f / 16777216.0f); }  // [0, 1)
    float range(float a, float b) { return a + (b - a) * uni(); }
    float bi() { return uni() * 2.0f - 1.0f; }                         // [-1, 1)
    int irange(int a, int b) { return b <= a ? a : a + (int)(u32() % (std::uint32_t)(b - a + 1)); }
    bool chance(float p) { return uni() < p; }
    float gauss();  // Normalverteilung (Mittel 0, Streuung 1)
};

// rosa Rauschen (-3 dB/Oktave), Filter nach Paul Kellet
struct Pink {
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    float run(float w) {
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        float y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return y * 0.11f;
    }
};

// braunes Rauschen (-6 dB/Oktave), leckender Integrator
struct Brown {
    float y = 0;
    float run(float w) {
        y = (y + 0.02f * w) * 0.998f;
        return y * 3.5f;
    }
};

// --- Filter -------------------------------------------------------------------------------------
struct OnePole {  // Tiefpass; hp() liefert den passenden Hochpass
    float a = 0, z = 0;
    void setLp(float hz, float rate) { a = onePoleCoef(hz, rate); }
    float lp(float x) { return z = x + (z - x) * a; }
    float hp(float x) { return x - lp(x); }
};

struct Biquad {
    enum Type { LP, HP, BP, Notch, Peak, LowShelf, HighShelf, AllPass };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;  // transponierte Direktform II
    void set(Type t, float hz, float q, float rate, float gainDb = 0.0f);
    static Biquad make(Type t, float hz, float q, float rate, float gainDb = 0.0f) {
        Biquad b;
        b.set(t, hz, q, rate, gainDb);
        return b;
    }
    float run(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// Zustandsvariablenfilter (Zavalishin, TPT): stabil bei schneller Modulation der Grenzfrequenz
struct Svf {
    float ic1 = 0, ic2 = 0;
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    void set(float hz, float q, float rate) {
        g = std::tan(kPi * clampf(hz, 5.0f, rate * 0.49f) / rate);
        k = 1.0f / std::max(q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // liefert Tief-, Band- und Hochpass gleichzeitig
    void run(float x, float& lp, float& bp, float& hp) {
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2, bp = v1, hp = x - k * v1 - v2;
    }
    float lp(float x) {
        float l, b, h;
        run(x, l, b, h);
        return l;
    }
    // Bandpass mit 0 dB in der Mitte (normiert: k * v1), unabhaengig von der Guete
    float bp(float x) {
        float l, b, h;
        run(x, l, b, h);
        return b * k;
    }
    float hp(float x) {
        float l, b, h;
        run(x, l, b, h);
        return h;
    }
};

// --- Modale Resonanz ----------------------------------------------------------------------------
// Ein Koerper (Rohr, Brett, Platte, Glas) klingt in seinen Eigenmoden. Jede Mode ist ein gedaempfter
// Schwinger 2. Ordnung (Frequenz, Abklingzeit, Gewicht). Der Eingang ist die Kraft des Anschlags.
struct Mode {
    float hz, t60, gain;
};
class ModalBank {
public:
    void add(float hz, float t60, float gain);
    void prepare(float rate);
    float run(float force);
    bool empty() const { return modes_.empty(); }
    void clear() { modes_.clear(), st_.clear(); }

private:
    struct St {
        float c1, c2, g, y1 = 0, y2 = 0;
    };
    std::vector<Mode> modes_;
    std::vector<St> st_;
};

// Kraftstoss eines Anschlags: halbe Sinuswelle der Dauer len (s). Harte Koerper: kurz.
inline float hitForce(float t, float len) { return (t >= 0.0f && t < len) ? std::sin(kPi * t / len) : 0.0f; }

// --- Verzoegerung -------------------------------------------------------------------------------
class DelayLine {
public:
    void init(int maxSamples) {
        buf_.assign((size_t)maxSamples + 4, 0.0f);
        w_ = 0;
    }
    void push(float x) {
        buf_[w_] = x;
        if (++w_ >= buf_.size()) w_ = 0;
    }
    // Verzoegerung in Samples (>= 1), kubisch interpoliert
    float read(float d) const;
    float readInt(int d) const {
        int i = (int)w_ - d;
        while (i < 0) i += (int)buf_.size();
        return buf_[(size_t)i];
    }
    int size() const { return (int)buf_.size(); }

private:
    std::vector<float> buf_;
    size_t w_ = 0;
};

// Schroeder-Allpass (Diffusion)
struct Allpass {
    std::vector<float> buf;
    size_t i = 0;
    float g = 0.6f;
    void init(int len, float gain) {
        buf.assign((size_t)std::max(1, len), 0.0f);
        i = 0;
        g = gain;
    }
    float run(float x) {
        float d = buf[i];
        float v = x + d * g;
        buf[i] = v;
        if (++i >= buf.size()) i = 0;
        return d - v * g;
    }
};

// --- Raum ---------------------------------------------------------------------------------------
// Beschreibung eines Raumklangs (Nachhallzeiten je Band, Groesse, Vorverzoegerung)
struct RoomSpec {
    float t60Low = 1.0f, t60Mid = 0.8f, t60High = 0.5f;  // s (unter ~250 Hz, Mitte, ueber ~4 kHz)
    float size = 1.0f;       // Faktor auf die Leitungslaengen (0.4 kleiner Raum .. 2.5 Halle)
    float preDelay = 0.01f;  // s
    float diffusion = 0.7f;  // 0..1 Dichte der Fahne
    float gate = 0.0f;       // > 0: Fahne bricht nach so vielen Sekunden hart ab (Level 37)
};

// Feedback-Delay-Netzwerk: 8 Leitungen, orthogonale Rueckkopplung (Hadamard), je Leitung ein
// Absorptionsfilter fuer die Nachhallzeiten der drei Baender, leicht modulierte Laengen.
class Fdn {
public:
    void init(float rate);
    void set(const RoomSpec& r);  // weicher Uebergang geschieht ueber setSmooth
    // Ein Block: Eingang mono, Ausgang wird zu outL/outR addiert
    void process(const float* in, float* outL, float* outR, int n);
    void clear();

private:
    static constexpr int N = 8;
    float rate_ = 48000;
    DelayLine line_[N];
    float len_[N] = {};
    float g_[N] = {}, lpA_[N] = {}, lpZ_[N] = {}, lsA_[N] = {}, lsZ_[N] = {}, lsG_[N] = {};
    float modPh_[N] = {}, modRate_[N] = {};
    Allpass diff_[4];
    DelayLine pre_;
    float preSamples_ = 0;
    float gateLen_ = 0, gateEnv_ = 1, gateTimer_ = 0, inEnv_ = 0;
    float sizeCur_ = 1.0f;
};

// Fruehreflexionen: Spiegelquellen 1. und 2. Ordnung eines Quaders um den Hoerer.
struct EarlyTap {
    float delay;  // s
    float gainL, gainR;
};
std::vector<EarlyTap> shoeboxReflections(float w, float d, float h, float lx, float ly, float lz, float absorption,
                                         float yaw);
class EarlyReflections {
public:
    void init(float rate) {
        rate_ = rate;
        line_.init((int)(rate * 0.25f));
    }
    void set(const std::vector<EarlyTap>& taps) { taps_ = taps; }
    void process(const float* in, float* outL, float* outR, int n);

private:
    float rate_ = 48000;
    DelayLine line_;
    std::vector<EarlyTap> taps_;
};

// --- Faltung (offline) ----------------------------------------------------------------------------
void fft(std::vector<float>& re, std::vector<float>& im, bool inverse);
// x * h, Laenge x + h - 1
std::vector<float> convolve(const std::vector<float>& x, const std::vector<float>& h);
// kuenstliche Raumantwort: abklingendes Rauschen mit frequenzabhaengiger Daempfung, dichter werdend
std::vector<float> makeImpulse(const RoomSpec& r, float rate, std::uint32_t seed, float length);

// --- Dynamik ------------------------------------------------------------------------------------
// Spitzenbegrenzer mit Vorausschau (Stereo): keine Verzerrung unterhalb der Schwelle.
class Limiter {
public:
    void init(float rate, float lookaheadMs = 1.5f, float releaseMs = 120.0f, float ceilingDb = -1.0f);
    void process(float* interleaved, int frames);
    float reduction() const { return gainNow_; }

private:
    std::vector<float> bufL_, bufR_, peak_;
    size_t pos_ = 0;
    int la_ = 72;
    float ceiling_ = 0.89f, rel_ = 0.999f, gainNow_ = 1.0f;
};

// --- Messung (Tests, Normalisierung) -----------------------------------------------------------
// Effektivwert mit K-Gewichtung (Hoererlautheit nach ITU-R BS.1770, vereinfacht, mono), in dB
float loudnessDb(const std::vector<float>& x, float rate);
float peakAbs(const std::vector<float>& x);

}  // namespace lim::audio::dsp
