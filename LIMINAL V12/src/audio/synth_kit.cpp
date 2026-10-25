#include "audio/synth_kit.hpp"

#include <algorithm>
#include <cmath>

namespace lim::audio::kit {

using dsp::clampf;
using dsp::Mode;

void Buf::add(const std::vector<float>& s, float atSec, float gain) {
    const size_t a = at(atSec);
    if (a + s.size() > x.size()) x.resize(a + s.size(), 0.0f);
    for (size_t i = 0; i < s.size(); ++i) x[a + i] += s[i] * gain;
}

// Spitzen um hoechstens maxDb begrenzen (Vorausschau 1 ms, Rueckkehr 40 ms): spitze Anschlaege
// behalten ihren Biss, der Koerper des Klangs kann lauter werden
static void peakLimit(Buf& b, float ceiling, float maxDb) {
    const float floorGain = dsp::dbToGain(-maxDb);
    const int la = std::max(4, b.rate / 1000);
    std::vector<float> need(b.n(), 1.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        const float a = std::fabs(b.x[i]);
        need[i] = a > ceiling ? std::max(floorGain, ceiling / a) : 1.0f;
    }
    // Minimum ueber das Vorausschaufenster, dann weich zurueck
    float cur = 1.0f;
    const float rel = std::exp(-1.0f / (0.04f * (float)b.rate)), att = 1.0f - std::exp(-3.0f / (float)la);
    std::vector<float> win(b.n(), 1.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        float m = 1.0f;
        for (int k = 0; k < la && i + (size_t)k < b.n(); ++k) m = std::min(m, need[i + (size_t)k]);
        win[i] = m;
    }
    for (size_t i = 0; i < b.n(); ++i) {
        cur = win[i] < cur ? cur + (win[i] - cur) * att : win[i] + (cur - win[i]) * rel;
        b.x[i] *= cur;
    }
}

void normalize(Buf& b, float lufs, float peak) {
    if (b.x.empty()) return;
    float l = dsp::loudnessDb(b.x, (float)b.rate);
    float g = l > -99.0f ? dsp::dbToGain(lufs - l) : 1.0f;
    if (dsp::peakAbs(b.x) * g > peak) {
        // erst die Spitzen zaehmen (hoechstens 6 dB), dann neu messen
        for (float& v : b.x) v *= g;
        peakLimit(b, peak, 6.0f);
        l = dsp::loudnessDb(b.x, (float)b.rate);
        g = l > -99.0f ? dsp::dbToGain(lufs - l) : 1.0f;
    }
    const float pk = dsp::peakAbs(b.x) * g;
    if (pk > peak) g *= peak / pk;
    for (float& v : b.x) v *= g;
}

void fades(Buf& b, float inSec, float outSec) {
    const size_t n = b.n(), fi = std::min(n, b.at(inSec)), fo = std::min(n, b.at(outSec));
    for (size_t i = 0; i < fi; ++i) b.x[i] *= dsp::smooth01((float)i / (float)fi);
    for (size_t i = 0; i < fo; ++i) b.x[n - 1 - i] *= dsp::smooth01((float)i / (float)fo);
}

void trim(Buf& b, float keepSec) {
    const float pk = dsp::peakAbs(b.x);
    if (pk <= 0.0f) return;
    const float th = pk * 3.2e-4f;  // -70 dB
    size_t last = 0;
    for (size_t i = 0; i < b.n(); ++i)
        if (std::fabs(b.x[i]) > th) last = i;
    const size_t keep = std::min(b.n(), last + b.at(keepSec) + 1);
    b.x.resize(keep);
    fades(b, 0.0f, keepSec);
}

void loopify(Buf& b, float xfadeSec) {
    const size_t n = b.n();
    const size_t xf = std::min(n / 3, b.at(xfadeSec));
    if (xf < 2) return;
    for (size_t i = 0; i < xf; ++i) {
        const float th = (float)i / (float)xf * kPi * 0.5f;
        b.x[i] = b.x[i] * std::sin(th) + b.x[n - xf + i] * std::cos(th);
    }
    b.x.resize(n - xf);
}

void dcBlock(Buf& b) {
    float z = 0, xz = 0;
    const float a = dsp::onePoleCoef(12.0f, (float)b.rate);
    for (float& v : b.x) {
        const float y = v - xz + a * z;
        xz = v;
        z = y;
        v = y;
    }
}

void filter(Buf& b, Biquad::Type t, float hz, float q, float gainDb) {
    Biquad f = Biquad::make(t, hz, q, (float)b.rate, gainDb);
    for (float& v : b.x) v = f.run(v);
}

void applyEnv(Buf& b, const std::function<float(float)>& env) {
    for (size_t i = 0; i < b.n(); ++i) b.x[i] *= env(b.t(i));
}

void reverb(Buf& b, const dsp::RoomSpec& room, float wet, float dry, std::uint32_t seed, float tail) {
    const float len = tail > 0 ? tail : std::max(room.t60Low, room.t60Mid) * 1.1f;
    const auto ir = dsp::makeImpulse(room, (float)b.rate, seed, len);
    auto w = dsp::convolve(b.x, ir);
    // Faltung mit energienormierter Antwort: Pegel der Fahne relativ zum Original angleichen
    double e0 = 0, e1 = 0;
    for (float v : b.x) e0 += (double)v * v;
    for (float v : w) e1 += (double)v * v;
    const float norm = e1 > 0 ? (float)std::sqrt(e0 / e1) : 0.0f;
    std::vector<float> out(w.size(), 0.0f);
    for (size_t i = 0; i < w.size(); ++i) out[i] = w[i] * norm * wet + (i < b.n() ? b.x[i] * dry : 0.0f);
    b.x.swap(out);
}

void distantize(Buf& b, float lowpass, float t60, std::uint32_t seed) {
    filter(b, Biquad::LP, lowpass, 0.6f);
    filter(b, Biquad::LP, lowpass * 1.4f, 0.6f);
    filter(b, Biquad::HP, 60.0f, 0.7f);
    dsp::RoomSpec r;
    r.t60Mid = t60, r.t60Low = t60 * 1.3f, r.t60High = t60 * 0.35f, r.size = 2.2f, r.preDelay = 0.03f;
    reverb(b, r, 1.0f, 0.12f, seed, t60 * 1.2f);
    trim(b, 0.05f);
}

// --- Anschlaege -----------------------------------------------------------------------------------
std::vector<float> impact(const ImpactSpec& s, float strength, float len, int rate, Rand& r) {
    const size_t n = (size_t)std::max(1.0f, len * (float)rate);
    std::vector<float> out(n, 0.0f);
    dsp::ModalBank bank;
    for (const Mode& m : s.modes) bank.add(m.hz, m.t60, m.gain);
    bank.prepare((float)rate);
    Biquad nf = Biquad::make(Biquad::BP, s.noiseHz, s.noiseQ, (float)rate);
    // harter Kontakt: kurzer Stoss; staerker angeschlagen = minimal kuerzer (hellerer Klang)
    const float contact = s.contact * (1.1f - 0.2f * clampf(strength, 0.0f, 1.0f));
    const float norm = 1.0f / std::max(1.0f, contact * (float)rate * 0.637f);  // Flaeche des Stosses = 1
    // Kontaktgeraeusch folgt dem Stoss: weicher Kontakt (Faust, Sohle) setzt weich ein, hat also
    // keine Hoehen bis 20 kHz; zusaetzlich Tiefpass nach der Kontaktdauer
    const float noiseAttack = std::max(0.0003f, contact * 0.45f);
    dsp::OnePole nlp;
    nlp.setLp(std::min(16000.0f, 2.5f / std::max(contact, 1e-4f)), (float)rate);
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / (float)rate;
        const float f = dsp::hitForce(t, contact) * norm;
        float y = bank.run(f * strength);
        if (s.noise > 0.0f) y += nlp.lp(nf.run(r.bi())) * s.noise * strength * envAD(t, noiseAttack, s.noiseT60);
        out[i] = y;
    }
    return out;
}

std::vector<Mode> modesMetalPipe(float f0, float t60, Rand& r) {
    static const float ratio[] = {1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.8f};
    std::vector<Mode> m;
    for (int k = 0; k < 7; ++k) {
        const float f = f0 * ratio[k] * r.range(0.985f, 1.015f);
        const float t = t60 * std::pow(ratio[k], -0.45f) * r.range(0.8f, 1.2f);
        const float g = std::pow(ratio[k], -0.55f) * r.range(0.6f, 1.0f);
        m.push_back({f, t, g});
        m.push_back({f * r.range(1.002f, 1.006f), t * 0.9f, g * 0.6f});  // Schwebung (fast entartete Paare)
    }
    // Ringmoden der Rohrwand (hoch, kurz)
    for (int k = 0; k < 4; ++k) m.push_back({f0 * r.range(9.0f, 30.0f), t60 * 0.15f, 0.12f});
    return m;
}

std::vector<Mode> modesMetalPlate(float f0, float t60, Rand& r) {
    std::vector<Mode> m;
    float f = f0;
    for (int k = 0; k < 28 && f < 14000.0f; ++k) {
        m.push_back({f, t60 * std::pow(f0 / f, 0.5f) * r.range(0.7f, 1.3f), r.range(0.3f, 1.0f) * std::pow(f0 / f, 0.35f)});
        f *= r.range(1.08f, 1.32f);
    }
    return m;
}

std::vector<Mode> modesWood(float f0, float t60, Rand& r) {
    static const float ratio[] = {1.0f, 2.08f, 3.57f, 5.26f, 7.4f, 9.9f};
    std::vector<Mode> m;
    for (int k = 0; k < 6; ++k) {
        const float rr = ratio[k] * r.range(0.94f, 1.06f);
        m.push_back({f0 * rr, t60 * std::pow(rr, -0.9f) * r.range(0.8f, 1.2f), r.range(0.6f, 1.0f) / rr});
    }
    return m;
}

std::vector<Mode> modesConcrete(float f0, Rand& r) {
    std::vector<Mode> m;
    float f = f0;
    for (int k = 0; k < 22 && f < 7000.0f; ++k) {
        m.push_back({f, r.range(0.008f, 0.035f), r.range(0.4f, 1.0f)});
        f *= r.range(1.12f, 1.3f);
    }
    return m;
}

std::vector<Mode> modesCeramic(float f0, float t60, Rand& r) {
    static const float ratio[] = {1.0f, 1.59f, 2.14f, 2.65f, 3.6f, 4.1f, 5.2f};
    std::vector<Mode> m;
    for (int k = 0; k < 7; ++k) {
        const float rr = ratio[k] * r.range(0.97f, 1.03f);
        m.push_back({f0 * rr, t60 * std::pow(rr, -0.6f), r.range(0.5f, 1.0f) / std::sqrt(rr)});
    }
    return m;
}

std::vector<Mode> modesGlass(float f0, float t60, Rand& r) {
    static const float ratio[] = {1.0f, 2.32f, 4.25f, 6.63f, 9.38f};
    std::vector<Mode> m;
    for (int k = 0; k < 5; ++k) {
        const float rr = ratio[k] * r.range(0.99f, 1.01f);
        m.push_back({f0 * rr, t60 * std::pow(rr, -0.35f), 1.0f / rr});
        m.push_back({f0 * rr * 1.003f, t60 * std::pow(rr, -0.35f) * 0.8f, 0.5f / rr});
    }
    return m;
}

std::vector<Mode> modesBell(float f0, float t60, Rand& r) {
    // Glockenteiltoene: Unterton, Prime, kleine Terz, Quinte, Nominal, ...
    static const float ratio[] = {0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f};
    static const float gain[] = {0.35f, 0.9f, 0.55f, 0.3f, 1.0f, 0.35f, 0.25f, 0.3f, 0.2f, 0.12f};
    std::vector<Mode> m;
    for (int k = 0; k < 10; ++k)
        m.push_back({f0 * ratio[k] * r.range(0.998f, 1.002f), t60 * (k == 0 ? 1.4f : std::pow(ratio[k], -0.7f)), gain[k]});
    return m;
}

std::vector<Mode> modesPlastic(float f0, float t60, Rand& r) {
    static const float ratio[] = {1.0f, 1.83f, 2.71f, 4.07f, 5.6f};
    std::vector<Mode> m;
    for (int k = 0; k < 5; ++k) {
        const float rr = ratio[k] * r.range(0.95f, 1.05f);
        m.push_back({f0 * rr, t60 * std::pow(rr, -0.8f), r.range(0.5f, 1.0f) / rr});
    }
    return m;
}

// --- Koerner, Blasen, Reibung -----------------------------------------------------------------------
void grains(Buf& b, float t0, float t1, float perSecond, const std::function<void(Buf&, float, Rand&)>& grain, Rand& r,
            const std::function<float(float)>& density) {
    float t = t0;
    const float maxRate = std::max(perSecond, 1e-3f);
    while (true) {
        t += -std::log(std::max(r.uni(), 1e-6f)) / maxRate;
        if (t >= t1) break;
        if (density && r.uni() > clampf(density(t), 0.0f, 1.0f)) continue;  // Ausduennen (inhomogener Poisson)
        grain(b, t, r);
    }
}

void bubble(Buf& b, float atSec, float radiusMm, float amp, Rand& r) {
    const float f0 = 3290.0f / std::max(0.2f, radiusMm);  // Minnaert (Wasser, 1 bar)
    const float t60 = clampf(0.012f * radiusMm + 0.01f, 0.012f, 0.12f);
    const float rise = r.range(0.25f, 0.6f);  // die Blase steigt: Ton geht nach oben
    const size_t a = b.at(atSec), n = (size_t)(t60 * (float)b.rate * 1.2f);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    float ph = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / (float)b.rate;
        const float f = f0 * (1.0f + rise * t / t60);
        ph += kTwoPi * f / (float)b.rate;
        b.x[a + i] += std::sin(ph) * amp * envAD(t, 0.0003f, t60);
    }
}

void creak(Buf& b, float atSec, float len, const CreakSpec& c, float amp, Rand& r) {
    const size_t a = b.at(atSec), n = (size_t)(len * (float)b.rate) + (size_t)(0.05f * (float)b.rate);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    std::vector<Biquad> f;
    for (size_t k = 0; k < c.formants.size(); ++k)
        f.push_back(Biquad::make(Biquad::BP, c.formants[k] * r.range(0.95f, 1.05f), c.q, (float)b.rate));
    float next = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / (float)b.rate;
        float pulse = 0.0f;
        if (t >= next && t < len) {
            const float u = t / len;
            const float rate = dsp::lerpf(c.rate0, c.rate1, u) * (1.0f + c.jitter * r.bi());
            next = t + 1.0f / std::max(5.0f, rate);
            pulse = r.range(0.6f, 1.0f);  // ungleich starke Rutscher
        }
        float y = 0.0f;
        for (size_t k = 0; k < f.size(); ++k) y += f[k].run(pulse) / (1.0f + (float)k * 0.6f);
        b.x[a + i] += y * amp * envAHR(t, len * 0.08f, len * 0.75f, len * 0.17f);
    }
}

void voice(Buf& b, float atSec, float len, const VoiceSpec& v, Rand& r) {
    const float rate = (float)b.rate;
    const size_t a = b.at(atSec), n = (size_t)(len * rate);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    std::vector<dsp::Svf> form(v.formants.size());
    for (size_t k = 0; k < form.size(); ++k)
        form[k].set(v.formants[k], v.formants[k] / std::max(20.0f, k < v.bandwidths.size() ? v.bandwidths[k] : 150.0f), rate);
    std::vector<dsp::Svf> nform = form;
    float phase = 1.0f, period = 0.01f, periodAmp = 1.0f;
    int periodIdx = 0;
    float lp = 0.0f;
    dsp::OnePole tilt1, tilt2;
    tilt1.setLp(v.tiltHz, rate);
    tilt2.setLp(v.tiltHz * 1.4f, rate);
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / rate;
        const float u = t / len;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            ++periodIdx;
            float f0 = std::max(20.0f, v.f0 ? v.f0(u) : 120.0f);
            float jit = 1.0f + v.jitter * r.gauss() * 0.5f;
            // Unterharmonische: jede zweite Periode laenger und schwaecher (Knurren, Rauheit)
            if (v.subharm > 0.0f && (periodIdx & 1)) jit *= 1.0f + 0.12f * v.subharm;
            period = jit / f0;
            periodAmp = 1.0f + v.shimmer * r.gauss() * 0.5f;
            if (v.subharm > 0.0f && (periodIdx & 1)) periodAmp *= 1.0f - 0.5f * v.subharm;
        }
        // Stimmlippenimpuls (Rosenberg): langsames Oeffnen, schnelles Schliessen
        float g;
        const float open = 0.6f, close = 0.25f;
        if (phase < open) g = 0.5f * (1.0f - std::cos(kPi * phase / open));
        else if (phase < open + close) g = std::cos(0.5f * kPi * (phase - open) / close);
        else g = 0.0f;
        phase += 1.0f / (period * rate);
        // Ableitung (abgestrahlter Schall), weich begrenzt
        const float src = (g - lp) * periodAmp;
        lp = g;
        const float noise = r.bi() * v.breath;
        float y = 0.0f;
        for (size_t k = 0; k < form.size(); ++k) {
            const float w = 1.0f / (1.0f + (float)k * 0.7f);
            y += form[k].bp(src) * w;
            y += nform[k].bp(noise) * w * 0.6f;
        }
        if (v.rough > 0.0f) y *= 1.0f - v.rough * 0.5f * (1.0f + std::sin(kTwoPi * v.roughHz * t + r.bi() * 0.3f));
        y = tilt2.lp(tilt1.lp(y));
        b.x[a + i] += y * (v.amp ? v.amp(u) : 1.0f);
    }
}

void breathe(Buf& b, float atSec, float len, bool inhale, float amp, float tension, Rand& r) {
    const float rate = (float)b.rate;
    const size_t a = b.at(atSec), n = (size_t)(len * rate);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    dsp::Svf f1, f2, f3;
    f1.set(inhale ? 900.0f : 650.0f, 2.5f, rate);
    f2.set(inhale ? 1700.0f : 1250.0f, 3.0f, rate);
    f3.set(2700.0f + 600.0f * tension, 4.0f, rate);
    dsp::Pink pink;
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / rate;
        const float u = t / len;
        // Einatmen schwillt an und bricht ab, Ausatmen setzt ein und verebbt
        const float e = inhale ? dsp::smooth01(u / 0.7f) * (1.0f - dsp::smooth01((u - 0.85f) / 0.15f))
                               : dsp::smooth01(u / 0.15f) * std::pow(1.0f - u, 1.3f);
        const float w = pink.run(r.bi()) * 2.0f + r.bi() * 0.3f;
        const float y = f1.bp(w) * 0.8f + f2.bp(w) * (0.5f + 0.3f * tension) + f3.bp(w) * (0.25f + 0.35f * tension);
        b.x[a + i] += y * e * amp;
    }
}

}  // namespace lim::audio::kit
