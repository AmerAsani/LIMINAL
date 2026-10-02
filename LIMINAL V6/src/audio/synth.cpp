// Prozedurale Klangerzeugung. Jeder Klang ist eine kleine Rechenvorschrift aus
// Rauschen, Filtern, Oszillatoren und Huellkurven - deterministisch (feste Seeds).
#include <cmath>
#include <functional>

#include "audio/audio.hpp"
#include "core/jobs.hpp"
#include "core/rng.hpp"

namespace lim::audio {

namespace {

constexpr float kTwoPi = 6.2831853f;

struct Noise {
    u32 s;
    explicit Noise(u32 seed) : s(seed * 747796405u + 2891336453u) {}
    float white() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float)(s & 0xFFFFFF) / 8388608.0f - 1.0f;
    }
};

// Biquad-Filter (RBJ)
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    static Biquad make(int type, float f, float q, int rate) {  // 0 LP, 1 HP, 2 BP
        Biquad b;
        float w = kTwoPi * f / (float)rate, c = std::cos(w), s = std::sin(w), al = s / (2.0f * q);
        float a0 = 1 + al;
        if (type == 0) {
            b.b0 = (1 - c) / 2 / a0, b.b1 = (1 - c) / a0, b.b2 = (1 - c) / 2 / a0;
        } else if (type == 1) {
            b.b0 = (1 + c) / 2 / a0, b.b1 = -(1 + c) / a0, b.b2 = (1 + c) / 2 / a0;
        } else {
            b.b0 = al / a0, b.b1 = 0, b.b2 = -al / a0;
        }
        b.a1 = -2 * c / a0, b.a2 = (1 - al) / a0;
        return b;
    }
    float run(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = x, y2 = y1, y1 = y;
        return y;
    }
};

float env(float t, float attack, float decay) {
    if (t < attack) return t / attack;
    return std::exp(-(t - attack) / decay);
}

void normalize(std::vector<float>& s, float peak) {
    float m = 1e-6f;
    for (float v : s) m = std::max(m, std::fabs(v));
    for (float& v : s) v *= peak / m;
}

// Schleife nahtlos machen: Ende weich in den Anfang ueberblenden
void makeLoop(std::vector<float>& s, int fade) {
    int n = (int)s.size();
    fade = std::min(fade, n / 2);
    for (int i = 0; i < fade; ++i) {
        float t = (float)i / fade;
        s[(size_t)i] = s[(size_t)i] * t + s[(size_t)(n - fade + i)] * (1 - t);
    }
    s.resize((size_t)(n - fade));
}

// --- Klaenge ---------------------------------------------------------------------------------
// Leuchtstoffroehren: 100 Hz Netzbrummen mit Obertoenen, leise hohe Drossel
Sound hum(int rate) {
    int n = rate * 4;  // 4 s = ganzzahlige Perioden
    Sound s;
    s.loop = true;
    s.samples.resize((size_t)n);
    Noise nz(11);
    Biquad bp = Biquad::make(2, 3400.0f, 3.0f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float buzz = std::sin(kTwoPi * 100 * t) * 0.5f + std::sin(kTwoPi * 200 * t) * 0.32f +
                     std::sin(kTwoPi * 300 * t) * 0.14f + std::sin(kTwoPi * 400 * t) * 0.09f;
        // leichte Verzerrung = typisches Schnarren
        buzz = std::tanh(buzz * 1.6f);
        float mod = 1.0f + 0.05f * std::sin(kTwoPi * 0.5f * t);
        float hiss = bp.run(nz.white()) * 0.05f;
        s.samples[(size_t)i] = (buzz * 0.6f + hiss) * mod;
    }
    normalize(s.samples, 0.5f);
    return s;
}

// --- V6: Leuchten als einzelne, raeumliche Quellen ------------------------------------------------
// Magnetisches Vorschaltgeraet am 50-Hz-Netz: Die Drossel brummt mit 100 Hz (doppelte Netzfrequenz,
// Magnetostriktion) und Obertoenen, ein Rest 50 Hz durch Unsymmetrie. Leichte Saettigung gibt das
// typische Schnarren, die Roehre selbst zischt fein im Takt der Halbwellen. Dazu langsame
// Pegelschwankungen und vereinzelt winzige Unruhen der Roehre. Alle Frequenzen sind Vielfache von
// 1/12 Hz -> die 12-s-Schleife ist nahtlos.
//   kind 0: Bueroleuchte (Leuchtstoffroehre), 1: Industrieleuchte (tiefer, runder),
//   2: Notleuchte (duenn, schnarrend)
Sound lampHum(int rate, int kind, int variant) {
    const int n = rate * 12;
    Sound s;
    s.loop = true;
    s.samples.resize((size_t)n);
    Noise nz(300 + (u32)(kind * 17 + variant));
    Rng r(500 + (u64)(kind * 31 + variant));
    Biquad sizzleBp = Biquad::make(2, kind == 2 ? 5200.0f : 3600.0f + 400.0f * (float)variant, 2.2f, rate);
    Biquad bodyLp = Biquad::make(0, kind == 1 ? 900.0f : 1800.0f, 0.7f, rate);
    // Obertonanteile je Art, leicht verschieden je Variante
    float h[7] = {0.12f, 1.0f, 0.55f, 0.3f, 0.18f, 0.1f, 0.07f};  // 50, 100, 200 ... 600 Hz
    if (kind == 1) h[0] = 0.18f, h[2] = 0.4f, h[3] = 0.14f, h[4] = 0.06f, h[5] = 0.03f, h[6] = 0.02f;
    if (kind == 2) h[3] = 0.45f, h[4] = 0.35f, h[5] = 0.25f, h[6] = 0.2f;
    float ph[7];
    for (int k = 0; k < 7; ++k) {
        h[k] *= (float)r.uniform(0.8, 1.2);
        ph[k] = (float)r.uniform(0.0, kTwoPi);
    }
    const float drive = kind == 2 ? 2.6f : (kind == 1 ? 1.1f : 1.6f + 0.15f * (float)variant);
    // vereinzelte Unruhe der Roehre (Zeitpunkte je Variante verschieden)
    float evAt[2] = {(float)r.uniform(1.0, 5.0), (float)r.uniform(6.5, 11.0)};
    float evLen[2] = {(float)r.uniform(0.15, 0.4), (float)r.uniform(0.2, 0.5)};
    float evDepth = kind == 0 ? 0.09f : 0.05f;
    const float drift1 = (float)r.randint(1, 3) / 12.0f, drift2 = (float)r.randint(4, 9) / 12.0f;
    // Vorlauf, damit die Filter eingeschwungen sind (sonst knackt die Schleifennaht)
    const int pre = rate / 10;
    for (int i = -pre; i < n; ++i) {
        float t = (float)i / (float)rate;
        float w = kTwoPi * 50.0f * t;
        float x = h[0] * std::sin(w + ph[0]);
        for (int k = 1; k < 7; ++k) x += h[k] * std::sin(w * (float)(2 * k) + ph[k]);
        x = std::tanh(x * drive) / std::tanh(drive);
        // Zischen der Gasentladung, im Takt der Halbwellen (100 Hz) moduliert
        float half = std::fabs(std::sin(w));
        float sizzle = sizzleBp.run(nz.white()) * half * half * half * half * (kind == 1 ? 0.012f : 0.035f);
        float body = bodyLp.run(x);
        // langsame Schwankung + seltene kleine Einbrueche
        float mod = 1.0f + 0.025f * std::sin(kTwoPi * drift1 * t + ph[1]) + 0.012f * std::sin(kTwoPi * drift2 * t + ph[2]);
        float unrest = 0.0f;
        for (int e = 0; e < 2; ++e) {
            float u = (t - evAt[e]) / evLen[e];
            if (u > 0.0f && u < 1.0f) unrest = std::max(unrest, std::sin(u * 3.14159f));
        }
        mod *= 1.0f - evDepth * unrest;
        if (i >= 0) s.samples[(size_t)i] = (body * 0.9f + sizzle * (1.0f + 3.0f * unrest)) * mod;
    }
    normalize(s.samples, 0.5f);
    return s;
}

// Knacken beim kurzen Aussetzen einer alten Roehre: Klick plus kurzes, rauhes Brummen
Sound lampTick(int rate) {
    int n = rate * 22 / 100;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(611);
    Biquad hp = Biquad::make(1, 1800.0f, 0.7f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)rate;
        float click = hp.run(nz.white()) * env(t, 0.0003f, 0.004f);
        float buzz = std::tanh(std::sin(kTwoPi * 100.0f * t) * 3.0f) * env(std::max(0.0f, t - 0.01f), 0.004f, 0.05f) *
                     (t > 0.01f ? 0.35f : 0.0f);
        s.samples[(size_t)i] = click + buzz;
    }
    normalize(s.samples, 0.4f);
    return s;
}

// Ferne Klaenge: trocken erzeugt, dann gedaempft und mit langer, diffuser Fahne versehen
// (vier Kammfilter + zwei Allpaesse offline) - klingt nach "irgendwo weit weg im Gebaeude".
void distantize(std::vector<float>& x, int rate, float lowpass, float tail) {
    Biquad lp = Biquad::make(0, lowpass, 0.7f, rate);
    for (float& v : x) v = lp.run(v);
    x.resize(x.size() + (size_t)(tail * (float)rate), 0.0f);
    const int combs[4] = {1557, 1617, 1491, 1422};
    std::vector<float> wetSig(x.size(), 0.0f);
    for (int c = 0; c < 4; ++c) {
        int d = (int)((float)combs[c] * (float)rate / 44100.0f * 1.7f);
        float g = std::pow(10.0f, -3.0f * (float)d / (tail * (float)rate));
        std::vector<float> buf((size_t)d, 0.0f);
        float store = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) {
            float y = buf[i % (size_t)d];
            store = y * 0.6f + store * 0.4f;
            buf[i % (size_t)d] = x[i] + store * g;
            wetSig[i] += y * 0.25f;
        }
    }
    for (int a : {556, 341}) {
        int d = (int)((float)a * (float)rate / 44100.0f);
        std::vector<float> buf((size_t)d, 0.0f);
        for (size_t i = 0; i < wetSig.size(); ++i) {
            float y = buf[i % (size_t)d];
            buf[i % (size_t)d] = wetSig[i] + y * 0.5f;
            wetSig[i] = y - wetSig[i];
        }
    }
    for (size_t i = 0; i < x.size(); ++i) x[i] = x[i] * 0.35f + wetSig[i];
}

Sound distant(int rate, int kind) {
    Sound s;
    Noise nz(700 + (u32)kind);
    int n = rate * 2;
    s.samples.assign((size_t)n, 0.0f);
    auto& o = s.samples;
    float lowpass = 1200.0f, tail = 2.4f;
    switch (kind) {
        case 0: {  // schwere Tuer faellt ins Schloss
            Biquad lp = Biquad::make(0, 140.0f, 0.8f, rate);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (float)rate;
                float thud = std::sin(kTwoPi * (62.0f + 30.0f * std::exp(-t * 25.0f)) * t) * env(t, 0.002f, 0.12f);
                float body = lp.run(nz.white()) * env(t, 0.003f, 0.08f) * 3.0f;
                float latch = t > 0.16f ? nz.white() * env(t - 0.16f, 0.0005f, 0.012f) * 0.5f : 0.0f;
                o[(size_t)i] = thud + body + latch;
            }
            lowpass = 900.0f, tail = 2.8f;
            break;
        }
        case 1: {  // Metall schlaegt an (Rohr, Gitter)
            const float f[5] = {310.0f, 523.0f, 841.0f, 1270.0f, 1913.0f};
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (float)rate;
                float v = 0.0f;
                for (int k = 0; k < 5; ++k) v += std::sin(kTwoPi * f[k] * t) * env(t, 0.001f, 0.5f / (1.0f + (float)k * 0.6f)) / (1.0f + (float)k * 0.5f);
                o[(size_t)i] = v + nz.white() * env(t, 0.0005f, 0.01f) * 0.6f;
            }
            lowpass = 2200.0f, tail = 2.6f;
            break;
        }
        case 2: {  // tiefes Grollen, schwillt an und ab (Aufzug, Lueftung, irgendetwas Grosses)
            n = rate * 4;
            o.assign((size_t)n, 0.0f);
            Biquad lp = Biquad::make(0, 70.0f, 0.7f, rate);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (float)rate;
                float e = std::sin(std::min(1.0f, t / 4.0f) * 3.14159f);
                o[(size_t)i] = (lp.run(nz.white()) * 4.0f + std::sin(kTwoPi * 38.0f * t) * 0.3f) * e * e;
            }
            lowpass = 400.0f, tail = 2.0f;
            break;
        }
        case 3: {  // drei leise Schlaege, wie an eine Tuer
            for (float at : {0.0f, 0.34f, 0.66f}) {
                int i0 = (int)(at * (float)rate);
                for (int i = 0; i < rate / 6 && i0 + i < n; ++i) {
                    float t = (float)i / (float)rate;
                    o[(size_t)(i0 + i)] += (std::sin(kTwoPi * 170.0f * t) + 0.5f * std::sin(kTwoPi * 410.0f * t)) * env(t, 0.001f, 0.03f);
                }
            }
            lowpass = 1100.0f, tail = 2.2f;
            break;
        }
        case 4: {  // Dampf/Druckluft entweicht
            Biquad bp = Biquad::make(2, 3000.0f, 0.8f, rate);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (float)rate;
                float e = std::min(1.0f, t / 0.08f) * std::exp(-std::max(0.0f, t - 0.6f) * 2.5f);
                o[(size_t)i] = bp.run(nz.white()) * e;
            }
            lowpass = 4000.0f, tail = 1.6f;
            break;
        }
        default: {  // Knarzen (Metall/Holz unter Spannung)
            float phs = 0.0f;
            for (int i = 0; i < n; ++i) {
                float t = (float)i / (float)rate;
                float f = 300.0f - 90.0f * std::min(1.0f, t / 1.2f);
                phs += kTwoPi * f / (float)rate;
                float grit = 0.5f + 0.5f * nz.white();
                float e = std::sin(std::min(1.0f, t / 1.3f) * 3.14159f);
                o[(size_t)i] = std::tanh(std::sin(phs) * 4.0f) * grit * e * 0.6f;
            }
            lowpass = 1500.0f, tail = 2.4f;
            break;
        }
    }
    distantize(o, rate, lowpass, tail);
    normalize(o, 0.5f);
    return s;
}

Sound vendingHum(int rate) {
    int n = rate * 3;
    Sound s;
    s.loop = true;
    s.samples.resize((size_t)n);
    Noise nz(21);
    Biquad lp = Biquad::make(0, 180.0f, 0.8f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float motor = std::sin(kTwoPi * 50 * t) * 0.6f + std::sin(kTwoPi * 150 * t) * 0.2f;
        float rumble = lp.run(nz.white()) * 1.6f;
        float rattle = std::sin(kTwoPi * 50 * t) > 0.97f ? nz.white() * 0.25f : 0.0f;
        s.samples[(size_t)i] = motor * 0.5f + rumble + rattle;
    }
    normalize(s.samples, 0.5f);
    return s;
}

// Tropfendes Wasser im Keller: unregelmaessige "Plinks" (Luftblasen-Resonanz mit steigender
// Tonhoehe) und zwei kurze Echos als Kellerhall. Schleife mit Stille am Ende.
Sound drip(int rate) {
    int n = rate * 5;
    Sound s;
    s.loop = true;
    s.samples.assign((size_t)n, 0.0f);
    const float at[4] = {0.35f, 2.05f, 2.42f, 3.85f};
    const float pitch[4] = {900.0f, 1150.0f, 820.0f, 1020.0f};
    const float gain[4] = {1.0f, 0.8f, 0.45f, 0.9f};
    for (int k = 0; k < 4; ++k) {
        int i0 = (int)(at[k] * (float)rate);
        float ph = 0.0f;
        for (int i = 0; i < rate / 4 && i0 + i < n; ++i) {
            float t = (float)i / (float)rate;
            float f = pitch[k] * (1.0f + 0.9f * std::min(1.0f, t / 0.035f));
            ph += kTwoPi * f / (float)rate;
            s.samples[(size_t)(i0 + i)] += std::sin(ph) * env(t, 0.001f, 0.03f) * gain[k];
        }
    }
    int d1 = (int)(0.11f * (float)rate), d2 = (int)(0.23f * (float)rate);
    for (int i = n - 1; i >= d1; --i) {
        s.samples[(size_t)i] += s.samples[(size_t)(i - d1)] * 0.28f;
        if (i >= d2) s.samples[(size_t)i] += s.samples[(size_t)(i - d2)] * 0.14f;
    }
    normalize(s.samples, 0.45f);
    return s;
}

// Schritt: kurzer Rauschimpuls, gefiltert je Untergrund, plus Resonanzen
Sound step(int rate, int kind, u32 seed) {
    // kind: 0 Teppich, 1 Beton, 2 Fliese, 3 Metall, 4 Holz
    const float lens[5] = {0.14f, 0.16f, 0.14f, 0.35f, 0.2f};
    int n = (int)(rate * lens[kind]);
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(seed);
    Rng r(seed);
    Biquad f;
    switch (kind) {
        case 0: f = Biquad::make(0, 380.0f + (float)r.uniform(-60, 60), 0.7f, rate); break;
        case 1: f = Biquad::make(2, 900.0f + (float)r.uniform(-150, 150), 0.9f, rate); break;
        case 2: f = Biquad::make(2, 2200.0f + (float)r.uniform(-300, 300), 1.4f, rate); break;
        case 3: f = Biquad::make(2, 1400.0f, 2.0f, rate); break;
        default: f = Biquad::make(2, 600.0f, 1.2f, rate); break;
    }
    Biquad thud = Biquad::make(0, 120.0f, 0.7f, rate);
    float heel = (float)r.uniform(0.012, 0.022);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float w = nz.white();
        float e = env(t, 0.002f, kind == 0 ? 0.03f : 0.02f) + 0.6f * env(std::max(0.0f, t - heel), 0.002f, 0.025f);
        float v = f.run(w) * e;
        v += thud.run(w) * env(t, 0.003f, 0.04f) * (kind == 0 ? 2.5f : 1.6f);
        if (kind == 3)  // Metall klingt nach
            v += (std::sin(kTwoPi * 823 * t) * 0.5f + std::sin(kTwoPi * 1327 * t) * 0.35f + std::sin(kTwoPi * 2113 * t) * 0.2f) *
                 env(t, 0.001f, 0.09f) * 0.4f;
        if (kind == 4) v += std::sin(kTwoPi * 190 * t) * env(t, 0.001f, 0.03f) * 0.5f;
        s.samples[(size_t)i] = v;
    }
    const float peak[5] = {0.28f, 0.4f, 0.42f, 0.45f, 0.4f};
    normalize(s.samples, peak[kind]);
    return s;
}

Sound land(int rate) {
    int n = rate * 3 / 10;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(31);
    Biquad lp = Biquad::make(0, 160.0f, 0.9f, rate);
    Biquad bp = Biquad::make(2, 700.0f, 0.8f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float w = nz.white();
        s.samples[(size_t)i] = lp.run(w) * env(t, 0.003f, 0.08f) * 3.0f + bp.run(w) * env(t, 0.002f, 0.03f);
    }
    normalize(s.samples, 0.6f);
    return s;
}

// Knisternde Verpackung + zarter Ton
Sound pickup(int rate) {
    int n = rate * 45 / 100;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(41);
    Biquad hp = Biquad::make(1, 2500.0f, 0.7f, rate);
    Rng r(42);
    std::vector<float> crackle((size_t)n, 0.0f);
    for (int k = 0; k < 18; ++k) {
        int at = (int)(r.uniform(0.0, 0.22) * rate);
        float amp = (float)r.uniform(0.3, 1.0);
        for (int j = 0; j < rate / 200 && at + j < n; ++j) crackle[(size_t)(at + j)] += amp * env((float)j / rate, 0.0005f, 0.002f);
    }
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float c = hp.run(nz.white()) * crackle[(size_t)i];
        float tone = (std::sin(kTwoPi * 880 * t) + 0.5f * std::sin(kTwoPi * 1320 * t)) * env(std::max(0.0f, t - 0.12f), 0.01f, 0.12f) * (t > 0.12f ? 0.25f : 0.0f);
        s.samples[(size_t)i] = c * 0.8f + tone;
    }
    normalize(s.samples, 0.55f);
    return s;
}

// V6: seltener Fund - kurzes, glaesernes Arpeggio mit leichtem Schweben
Sound pickupRare(int rate) {
    int n = rate * 12 / 10;
    Sound s;
    s.samples.assign((size_t)n, 0.0f);
    const float notes[4] = {659.25f, 830.61f, 987.77f, 1318.5f};  // E5, G#5, H5, E6
    for (int k = 0; k < 4; ++k) {
        int i0 = (int)(0.07f * (float)k * (float)rate);
        for (int i = i0; i < n; ++i) {
            float t = (float)(i - i0) / (float)rate;
            float f = notes[k];
            float v = std::sin(kTwoPi * f * t) + 0.35f * std::sin(kTwoPi * f * 2.003f * t) +
                      0.12f * std::sin(kTwoPi * f * 3.01f * t);
            v *= 1.0f + 0.08f * std::sin(kTwoPi * 5.5f * t);  // Schweben
            s.samples[(size_t)i] += v * env(t, 0.004f, 0.32f) * (1.0f - 0.12f * (float)k);
        }
    }
    normalize(s.samples, 0.38f);
    return s;
}

// V6: Absprung - kurzes Kleiderrascheln (gefiltertes Rauschen, schnell an- und abschwellend)
Sound jump(int rate) {
    int n = rate * 26 / 100;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(77);
    Biquad bp = Biquad::make(2, 1500.0f, 0.7f, rate);
    Biquad lp = Biquad::make(0, 420.0f, 0.7f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)rate;
        float w = nz.white();
        float e = env(t, 0.03f, 0.07f);
        s.samples[(size_t)i] = bp.run(w) * e * 0.8f + lp.run(w) * env(t, 0.005f, 0.04f) * 1.4f;
    }
    normalize(s.samples, 0.32f);
    return s;
}

// V6: Keuchen nach dem Sprinten - Schleife aus Einatmen (hell) und Ausatmen (dunkler, lauter)
Sound breath(int rate) {
    int n = rate * 9 / 10;
    Sound s;
    s.loop = true;
    s.samples.assign((size_t)n, 0.0f);
    Noise nz(91);
    Biquad in = Biquad::make(2, 1900.0f, 1.1f, rate), out = Biquad::make(2, 950.0f, 0.9f, rate);
    Biquad lp = Biquad::make(0, 420.0f, 0.7f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)rate;
        float w = nz.white();
        // Einatmen 0.03 .. 0.33 s, Ausatmen 0.42 .. 0.84 s (weiche Huellkurven)
        float ein = t > 0.03f && t < 0.33f ? std::sin((t - 0.03f) / 0.30f * 3.14159f) : 0.0f;
        float aus = t > 0.42f && t < 0.84f ? std::sin((t - 0.42f) / 0.42f * 3.14159f) : 0.0f;
        s.samples[(size_t)i] = in.run(w) * ein * ein * 0.55f + (out.run(w) * 0.9f + lp.run(w) * 0.6f) * aus * aus;
    }
    normalize(s.samples, 0.42f);
    return s;
}

// V6: Nach-Luft-Schnappen, wenn die Ausdauer verbraucht ist
Sound gasp(int rate) {
    int n = rate * 45 / 100;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(93);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)rate;
        s.samples[(size_t)i] = nz.white() * env(t, 0.04f, 0.12f);
    }
    // Band wandert nach oben: in Bloecken filtern (stabiler als je Sample neu zu berechnen)
    Biquad f = Biquad::make(2, 1000.0f, 1.3f, rate);
    for (int i = 0; i < n; ++i) {
        if (i % 480 == 0) {
            float t = (float)i / (float)rate;
            Biquad nf = Biquad::make(2, 1000.0f + 2400.0f * std::min(1.0f, t / 0.3f), 1.3f, rate);
            f.b0 = nf.b0, f.b1 = nf.b1, f.b2 = nf.b2, f.a1 = nf.a1, f.a2 = nf.a2;
        }
        s.samples[(size_t)i] = f.run(s.samples[(size_t)i]);
    }
    normalize(s.samples, 0.4f);
    return s;
}

// V6: abgelegtes Item trifft auf - kurzes, hohles Klopfen
Sound itemLand(int rate) {
    int n = rate * 18 / 100;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(95);
    Biquad bp = Biquad::make(2, 1300.0f, 1.2f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / (float)rate;
        float knock = std::sin(kTwoPi * 230.0f * t) * env(t, 0.001f, 0.035f) +
                      0.5f * std::sin(kTwoPi * 610.0f * t) * env(t, 0.001f, 0.02f);
        s.samples[(size_t)i] = knock + bp.run(nz.white()) * env(t, 0.0005f, 0.012f) * 0.8f;
    }
    normalize(s.samples, 0.45f);
    return s;
}

Sound eat(int rate) {
    int n = rate * 9 / 10;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(51);
    Biquad bp = Biquad::make(2, 1800.0f, 0.9f, rate);
    Biquad lp = Biquad::make(0, 400.0f, 0.7f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float bite = 0;
        for (float at : {0.05f, 0.33f, 0.58f}) bite += env(std::max(0.0f, t - at), 0.004f, 0.05f) * (t >= at ? 1.0f : 0.0f);
        float w = nz.white();
        s.samples[(size_t)i] = bp.run(w) * bite + lp.run(w) * bite * 1.5f;
    }
    normalize(s.samples, 0.5f);
    return s;
}

Sound drink(int rate) {
    int n = rate * 11 / 10;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(61);
    Biquad lp = Biquad::make(0, 900.0f, 1.5f, rate);
    float phase = 0;
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float g = 0, f = 0;
        for (int k = 0; k < 3; ++k) {
            float at = 0.08f + k * 0.32f, lt = t - at;
            if (lt >= 0 && lt < 0.22f) {
                g += std::sin(kTwoPi * lt / 0.44f);
                f = 180.0f + 260.0f * lt;
            }
        }
        phase += kTwoPi * std::max(f, 1.0f) / rate;
        s.samples[(size_t)i] = std::sin(phase) * g * 0.6f + lp.run(nz.white()) * g * 0.4f;
    }
    normalize(s.samples, 0.5f);
    return s;
}

Sound blip(int rate, float f0, float f1, float len, float amp) {
    int n = (int)(rate * len);
    Sound s;
    s.samples.resize((size_t)n);
    float ph = 0;
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float f = f0 + (f1 - f0) * (t / len);
        ph += kTwoPi * f / rate;
        s.samples[(size_t)i] = (std::sin(ph) + 0.25f * std::sin(ph * 2.0f)) * env(t, 0.002f, len * 0.35f);
    }
    normalize(s.samples, amp);
    return s;
}

Sound heartbeat(int rate) {
    int n = rate;  // eine Sekunde
    Sound s;
    s.loop = true;
    s.samples.resize((size_t)n);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float v = 0;
        for (auto [at, a] : {std::pair{0.0f, 1.0f}, std::pair{0.22f, 0.7f}}) {
            float lt = t - at;
            if (lt >= 0) v += std::sin(kTwoPi * (48.0f + 20.0f * std::exp(-lt * 30)) * lt) * env(lt, 0.006f, 0.07f) * a;
        }
        s.samples[(size_t)i] = v;
    }
    normalize(s.samples, 0.9f);
    return s;
}

Sound death(int rate) {
    int n = rate * 3;
    Sound s;
    s.samples.resize((size_t)n);
    Noise nz(71);
    Biquad lp = Biquad::make(0, 300.0f, 0.9f, rate);
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float drone = std::sin(kTwoPi * 55 * t) + 0.6f * std::sin(kTwoPi * 58.3f * t) + 0.4f * std::sin(kTwoPi * 110.7f * t);
        float e = std::min(1.0f, t / 0.4f) * std::exp(-std::max(0.0f, t - 1.2f) * 1.5f);
        s.samples[(size_t)i] = (std::tanh(drone * 1.5f) * 0.7f + lp.run(nz.white()) * 1.2f) * e;
    }
    normalize(s.samples, 0.7f);
    return s;
}

// Atmosphaere je Zone (lange, nahtlose Schleifen)
Sound ambience(int rate, int kind) {
    int n = rate * 12;
    Sound s;
    s.loop = true;
    s.samples.resize((size_t)n);
    Noise nz(80 + (u32)kind);
    Rng r(90 + (u64)kind);
    Biquad lp = Biquad::make(0, kind == 2 ? 700.0f : 260.0f, 0.7f, rate);
    Biquad lp2 = Biquad::make(0, 90.0f, 0.7f, rate);
    Biquad bp = Biquad::make(2, 480.0f, 0.6f, rate);
    std::vector<float> events((size_t)n, 0.0f);
    if (kind == 2) {  // Tunnel: vereinzelte Tropfen
        for (int k = 0; k < 7; ++k) {
            int at = (int)(r.uniform(0.0, 11.5) * rate);
            float f = (float)r.uniform(1400, 2600);
            for (int j = 0; j < rate / 5 && at + j < n; ++j) {
                float t = (float)j / rate;
                events[(size_t)(at + j)] += std::sin(kTwoPi * f * (1.0f + t * 1.5f) * t) * env(t, 0.001f, 0.03f) * 0.5f;
            }
        }
    }
    if (kind == 4) {  // Wartung: rhythmisches Klacken einer Maschine
        for (float at = 0.3f; at < 11.8f; at += 1.45f) {
            int a = (int)(at * rate);
            for (int j = 0; j < rate / 8 && a + j < n; ++j) {
                float t = (float)j / rate;
                events[(size_t)(a + j)] += (std::sin(kTwoPi * 310 * t) + std::sin(kTwoPi * 517 * t)) * env(t, 0.001f, 0.02f) * 0.25f;
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        float t = (float)i / rate;
        float w = nz.white();
        float v;
        switch (kind) {
            case 0: v = lp.run(w) * 0.5f + std::sin(kTwoPi * 60 * t) * 0.02f; break;          // Buero: Raumrauschen
            case 1: v = lp2.run(w) * 2.2f + lp.run(w) * 0.4f; break;                            // Industrie: Grollen
            case 2: v = bp.run(w) * (0.5f + 0.4f * std::sin(kTwoPi * 0.11f * t)) + events[(size_t)i]; break;  // Tunnel: Wind
            case 3: v = bp.run(w) * 0.35f + lp.run(w) * 0.3f; break;                            // Hallen: Luft
            default: v = lp2.run(w) * 1.4f + std::sin(kTwoPi * 45 * t) * 0.08f + events[(size_t)i]; break;
        }
        s.samples[(size_t)i] = v;
    }
    makeLoop(s.samples, rate);
    normalize(s.samples, 0.35f);
    return s;
}

}  // namespace

std::unordered_map<std::string, Sound> synthesizeAll(JobSystem& jobs, int rate) {
    struct Def {
        std::string name;
        std::function<Sound()> fn;
    };
    std::vector<Def> defs = {
        {"hum", [=] { return hum(rate); }},
        {"vending_hum", [=] { return vendingHum(rate); }},
        {"drip", [=] { return drip(rate); }},
        {"land", [=] { return land(rate); }},
        {"pickup", [=] { return pickup(rate); }},
        {"pickup_rare", [=] { return pickupRare(rate); }},
        {"jump", [=] { return jump(rate); }},
        {"breath", [=] { return breath(rate); }},
        {"gasp", [=] { return gasp(rate); }},
        {"item_land", [=] { return itemLand(rate); }},
        {"eat", [=] { return eat(rate); }},
        {"drink", [=] { return drink(rate); }},
        {"ui_move", [=] { return blip(rate, 1500, 1400, 0.035f, 0.18f); }},
        {"ui_select", [=] { return blip(rate, 900, 1350, 0.09f, 0.25f); }},
        {"ui_back", [=] { return blip(rate, 900, 600, 0.08f, 0.22f); }},
        {"toast", [=] { return blip(rate, 700, 700, 0.12f, 0.12f); }},
        {"heartbeat", [=] { return heartbeat(rate); }},
        {"death", [=] { return death(rate); }},
    };
    const char* stepNames[5] = {"step_carpet", "step_concrete", "step_tile", "step_metal", "step_wood"};
    for (int k = 0; k < 5; ++k)
        for (int v = 0; v < 4; ++v)
            defs.push_back({std::string(stepNames[k]) + "_" + std::to_string(v),
                            [=] { return step(rate, k, (u32)(1000 + k * 17 + v * 101)); }});
    // V6: Leuchten als raeumliche Quellen (4 Varianten Buero, je 2 Industrie/Notlicht), ferne Klaenge
    for (int v = 0; v < 4; ++v) defs.push_back({"lamp_hum_" + std::to_string(v), [=] { return lampHum(rate, 0, v); }});
    for (int v = 0; v < 2; ++v) defs.push_back({"lamp_ind_" + std::to_string(v), [=] { return lampHum(rate, 1, v); }});
    for (int v = 0; v < 2; ++v) defs.push_back({"lamp_emg_" + std::to_string(v), [=] { return lampHum(rate, 2, v); }});
    defs.push_back({"lamp_tick", [=] { return lampTick(rate); }});
    const char* dist[6] = {"dist_door", "dist_clank", "dist_rumble", "dist_knock", "dist_steam", "dist_creak"};
    for (int k = 0; k < 6; ++k) defs.push_back({dist[k], [=] { return distant(rate, k); }});
    const char* amb[5] = {"amb_office", "amb_industrial", "amb_tunnel", "amb_halls", "amb_maint"};
    for (int k = 0; k < 5; ++k) defs.push_back({amb[k], [=] { return ambience(rate, k); }});

    std::vector<Sound> out(defs.size());
    jobs.parallelFor((int)defs.size(), [&](int i) { out[(size_t)i] = defs[(size_t)i].fn(); });
    std::unordered_map<std::string, Sound> m;
    for (size_t i = 0; i < defs.size(); ++i) m[defs[i].name] = std::move(out[i]);
    return m;
}

}  // namespace lim::audio
