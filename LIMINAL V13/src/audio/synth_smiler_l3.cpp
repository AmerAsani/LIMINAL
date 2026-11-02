// V13.3 Level 3 (KREATUR_Smiler.md, Abschnitt "V13.3: Level 3 - Angriff ohne Anlass"): die Ankuendigung des Smilers, der
// ohne Anlass angreift. Die Seite beschreibt keinen Klang (Interpretation); er muss sich klar vom gewohnten "smiler_open"
// (Panik) unterscheiden, damit man sofort weiss: diesmal hilft Stillstehen nicht.
//   smiler_peel  Lider und Lippen pressen sich zu (dumpfes, feuchtes Quetschen von Fleisch), dann reissen sie von Augen und
//                Zaehnen ab (klebriges Abloesen, ein Riss), dichtes Zaehneklappern, darueber ein duenner, schneidender Ton, der
//                steigt und schwebt, und ein scharfes Einziehen von Luft durch die Zaehne.
#include <algorithm>
#include <cmath>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

// klebriges Abloesen: kurze, nasse Knackse (gefiltertes Rauschen mit Abfall), Dichte steigt
void stickyPop(Buf& b, float at, float amp, float hz, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, hz, r.range(1.5f, 3.5f), (float)b.rate);
    const size_t a = b.at(at), n = b.at(r.range(0.004f, 0.012f));
    for (size_t i = 0; i < n && a + i < b.n(); ++i) b.x[a + i] += bp.run(r.bi()) * envAD(b.t(i), 0.0004f, (float)n / (float)b.rate) * amp;
}

// Zahnschmelz auf Zahnschmelz: hartes, helles Klicken mit kurzer Resonanz
void enamel(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec s;
    s.modes = modesCeramic(r.range(2400.0f, 3600.0f), 0.025f, r);
    s.contact = 0.0004f, s.noise = 0.6f, s.noiseHz = 5200.0f;
    b.add(impact(s, amp, 0.06f, b.rate, r), at);
}

Sound smilerPeel(int rate) {
    Rand r(31337u);
    Buf b(2.6f, rate);
    // 1) zupressen: dumpfes Quetschen (Reibung mit tiefen Formanten), feuchtes Saugen
    CreakSpec sq;
    sq.rate0 = 70.0f, sq.rate1 = 38.0f, sq.jitter = 0.35f, sq.formants = {240.0f, 520.0f, 1150.0f}, sq.q = 5.0f;
    creak(b, 0.0f, 0.38f, sq, 0.5f, r);
    {
        Biquad lp = Biquad::make(Biquad::LP, 380.0f, 0.9f, (float)rate);
        for (size_t i = 0; i < b.at(0.4f); ++i) b.x[i] += lp.run(r.bi()) * 0.35f * envAHR(b.t(i), 0.05f, 0.2f, 0.15f);
    }
    // 2) aufreissen: klebrige Knackse immer dichter, ein Riss (Rauschen, das hell wird)
    for (float t = 0.36f; t < 0.78f;) {
        stickyPop(b, t, r.range(0.25f, 0.6f), r.range(900.0f, 2600.0f), r);
        t += r.range(0.004f, 0.03f) * (1.0f - 0.7f * (t - 0.36f) / 0.42f);
    }
    {
        dsp::Svf f;
        const size_t a = b.at(0.42f), n = b.at(0.3f);
        for (size_t i = 0; i < n; ++i) {
            const float u = (float)i / (float)n;
            f.set(700.0f + 3800.0f * u, 1.6f, (float)rate);
            b.x[a + i] += f.bp(r.bi()) * 0.75f * std::sin(3.14159f * u) * (0.6f + 0.4f * r.uni());
        }
    }
    // 3) Zaehneklappern: ungleichmaessig, etwa 16 je Sekunde, wird schneller
    for (float t = 0.7f; t < 2.35f;) {
        enamel(b, t, r.range(0.35f, 0.7f) * (t < 2.1f ? 1.0f : (2.35f - t) / 0.25f), r);
        t += r.range(0.035f, 0.085f) * (1.15f - 0.4f * (t - 0.7f) / 1.65f);
    }
    // 4) duenner Ton: steigt von 1,3 auf 3,4 kHz, schwebt (zwei nahe Teiltoene), zittert
    float p1 = 0.0f, p2 = 0.0f;
    const size_t a = b.at(0.62f), n = b.at(1.8f);
    for (size_t i = 0; i < n && a + i < b.n(); ++i) {
        const float t = b.t(i), u = t / 1.8f;
        const float f = 1300.0f * std::pow(2.6f, u) * (1.0f + 0.011f * std::sin(kTwoPi * 7.7f * t));
        p1 += kTwoPi * f / (float)rate;
        p2 += kTwoPi * f * 1.017f / (float)rate;
        const float env = dsp::smooth01(std::min(1.0f, u * 4.0f)) * (u > 0.88f ? (1.0f - u) / 0.12f : 1.0f);
        b.x[a + i] += (std::sin(p1) + 0.7f * std::sin(p2) + 0.2f * std::sin(p1 * 2.01f)) * 0.06f * env;
    }
    // 5) scharfes Einziehen von Luft durch die Zaehne
    breathe(b, 0.75f, 1.3f, true, 0.55f, 1.0f, r);
    filter(b, Biquad::HP, 90.0f, 0.7f);
    trim(b);
    fades(b, 0.002f, 0.04f);
    normalize(b, level::kCreature + 0.5f);
    return b.sound();
}

}  // namespace

void addSmilerL3Sounds(std::vector<SoundDef>& d, int rate) { d.push_back({"smiler_peel", [=] { return smilerPeel(rate); }}); }

}  // namespace lim::audio
