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
    const char* amb[5] = {"amb_office", "amb_industrial", "amb_tunnel", "amb_halls", "amb_maint"};
    for (int k = 0; k < 5; ++k) defs.push_back({amb[k], [=] { return ambience(rate, k); }});

    std::vector<Sound> out(defs.size());
    jobs.parallelFor((int)defs.size(), [&](int i) { out[(size_t)i] = defs[(size_t)i].fn(); });
    std::unordered_map<std::string, Sound> m;
    for (size_t i = 0; i < defs.size(); ++i) m[defs[i].name] = std::move(out[i]);
    return m;
}

}  // namespace lim::audio
