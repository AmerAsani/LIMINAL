// V13.3: Klaenge der purpifizierten Wesen in den Ziegelwaenden von Level 3 (KREATUR_L3_Purpur.md, gameplay/purpification.hpp).
// Die Seite beschreibt keinen Klang (Interpretation): eine unbekannte, starke Energie in Ziegeln und Kadaver.
// Alles synthetisch:
//   purple_hum        Schleife: tiefes, schwebendes Brummen (nicht der Netzbrumm der Ebene), darueber ein glasiges
//                     Singen aus eng beieinanderliegenden hohen Teiltoenen, ein Hauch, der im Takt des Lichts atmet
//   purple_crackle_N  Schub trockener Knackse (Entladung in den Ziegeln), dazwischen ein kurzes, glasiges "Ping"
//   purple_settle     Mauerwerk knackt, eine Sehne knarrt unter Spannung, Ziegelkruemel rieseln
//   purple_surge      Schwall: das Brummen schwillt an und steigt, das Singen wird lauter, Knistern verdichtet sich
#include <algorithm>
#include <cmath>
#include <format>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

// hohe, eng benachbarte Teiltoene (schwebend) - das "Violette" im Klang
void shimmer(Buf& b, float t0, float t1, float amp, float rise, Rand& r) {
    const float base[6] = {1318.5f, 1396.9f, 1975.5f, 2093.0f, 2637.0f, 2793.8f};
    float ph[6], det[6];
    for (int k = 0; k < 6; ++k) ph[k] = r.range(0.0f, kTwoPi), det[k] = r.range(-1.2f, 1.2f);
    const size_t a = b.at(t0), n = b.at(t1 - t0);
    for (size_t i = 0; i < n && a + i < b.n(); ++i) {
        const float t = b.t(i), u = t / std::max(0.01f, t1 - t0);
        float s = 0.0f;
        for (int k = 0; k < 6; ++k) {
            const float f = (base[k] + det[k]) * (1.0f + rise * u);
            ph[k] += kTwoPi * f / (float)b.rate;
            const float am = 0.55f + 0.45f * std::sin(t * (0.37f + 0.11f * k) + k * 1.7f);
            s += std::sin(ph[k]) * am * (k < 2 ? 1.0f : k < 4 ? 0.6f : 0.35f);
        }
        b.x[a + i] += s * amp;
    }
}

void click(Buf& b, float at, float amp, Rand& r) {
    Biquad hp = Biquad::make(Biquad::HP, r.range(2500.0f, 5000.0f), 0.7f, (float)b.rate);
    const size_t a = b.at(at), n = b.at(r.range(0.0004f, 0.0015f));
    for (size_t i = 0; i < n + 40 && a + i < b.n(); ++i) {
        const float e = i < n ? 1.0f : std::exp(-(float)(i - n) * 0.25f);
        b.x[a + i] += hp.run(r.bi() * e) * amp;
    }
}

void ping(Buf& b, float at, float amp, Rand& r) {
    const float f = r.range(2600.0f, 4800.0f), f2 = f * r.range(1.47f, 1.53f);
    const size_t a = b.at(at), n = b.at(0.25f);
    for (size_t i = 0; i < n && a + i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[a + i] += (std::sin(kTwoPi * f * t) + 0.4f * std::sin(kTwoPi * f2 * t)) * envAD(t, 0.0008f, 0.11f) * amp;
    }
}

Sound purpleHum(int rate) {
    Rand r(15100u);
    const float L = 13.0f;  // zwei Atemzuege des Lichts (6,5 s)
    Buf b(L + 1.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 160.0f, 0.7f, (float)rate);
    Biquad bp = Biquad::make(Biquad::BP, 420.0f, 0.9f, (float)rate);
    float p1 = 0.0f, p2 = 0.0f, p3 = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float breathe = 0.7f + 0.3f * std::sin(t * (kTwoPi / 6.5f));
        p1 += kTwoPi * 41.0f / (float)rate;
        p2 += kTwoPi * 43.4f / (float)rate;   // schwebt gegen den ersten (2,4 Hz)
        p3 += kTwoPi * 20.6f / (float)rate;   // Unteroktave, kaum hoerbar, eher Druck
        const float low = 0.5f * std::sin(p1) + 0.42f * std::sin(p2) + 0.25f * std::sin(p3) + 0.12f * std::sin(3.0f * p1 + 0.4f);
        const float air = bp.run(r.bi()) * 0.06f + lp.run(br.run(r.bi())) * 0.5f;
        b.x[i] = (low * 0.8f + air) * breathe;
    }
    shimmer(b, 0.0f, L + 1.0f, 0.035f, 0.0f, r);
    // vereinzelte, leise Knackse auch in der Schleife (die Stelle ist nie ganz still)
    grains(b, 0.2f, L, 2.2f, [&](Buf& g, float at, Rand& rr) { click(g, at, rr.range(0.05f, 0.18f), rr); }, r);
    loopify(b, 1.0f);
    normalize(b, level::kLoop - 1.0f);
    return b.sound(true);
}

Sound purpleCrackle(int rate, int v) {
    Rand r(15200u + (unsigned)v * 31u);
    const float L = r.range(0.35f, 0.8f);
    Buf b(L + 0.35f, rate);
    // Dichte steigt an und faellt ab (ein Schub), Knackse hart und trocken
    grains(b, 0.0f, L, r.range(45.0f, 90.0f), [&](Buf& g, float at, Rand& rr) { click(g, at, rr.range(0.25f, 1.0f), rr); }, r,
           [L](float t) { const float u = t / L; return std::sin(3.14159f * u) * (0.6f + 0.4f * std::sin(u * 17.0f)); });
    for (int k = 0, n = r.irange(1, 2); k < n; ++k) ping(b, r.range(0.05f, L * 0.8f), r.range(0.12f, 0.22f), r);
    trim(b);
    normalize(b, level::kEvent - 9.0f);
    return b.sound();
}

Sound purpleSettle(int rate) {
    Rand r(15300u);
    Buf b(2.0f, rate);
    // zwei, drei harte Risse im Ziegel
    float at = 0.05f;
    for (int k = 0; k < 3; ++k) {
        ImpactSpec s;
        s.modes = modesConcrete(r.range(380.0f, 700.0f), r);
        s.contact = 0.0012f;
        s.noise = 0.45f;
        s.noiseHz = 2600.0f;
        const std::vector<float> x = impact(s, r.range(0.5f, 1.0f), 0.35f, rate, r);
        for (size_t i = 0; i < x.size() && b.at(at) + i < b.n(); ++i) b.x[b.at(at) + i] += x[i];
        at += r.range(0.12f, 0.3f);
    }
    // eine Sehne knarrt unter Spannung (tiefe Formanten, langsam), dann rieseln Kruemel
    CreakSpec c;
    c.rate0 = 22.0f, c.rate1 = 9.0f, c.jitter = 0.35f, c.formants = {240.0f, 610.0f, 1500.0f}, c.q = 6.0f;
    creak(b, 0.35f, 0.75f, c, 0.6f, r);
    grains(b, 0.5f, 1.8f, 24.0f, [&](Buf& g, float tt, Rand& rr) {
        ImpactSpec s;
        s.modes = modesConcrete(rr.range(1800.0f, 3800.0f), rr);
        s.contact = 0.0006f;
        const std::vector<float> x = impact(s, rr.range(0.05f, 0.25f), 0.05f, g.rate, rr);
        for (size_t i = 0; i < x.size() && g.at(tt) + i < g.n(); ++i) g.x[g.at(tt) + i] += x[i];
    }, r, [](float t) { return std::exp(-(t - 0.5f) * 2.2f); });
    trim(b);
    normalize(b, level::kEvent - 5.0f);
    return b.sound();
}

Sound purpleSurge(int rate) {
    Rand r(15400u);
    const float L = 2.6f;
    Buf b(L + 0.4f, rate);
    float ph = 0.0f, ph2 = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i), u = std::min(1.0f, t / L);
        const float f = 38.0f + 22.0f * u * u;
        ph += kTwoPi * f / (float)rate;
        ph2 += kTwoPi * f * 1.5f * 1.006f / (float)rate;
        b.x[i] = (std::sin(ph) + 0.35f * std::sin(ph2)) * envAHR(t, 1.3f, 0.6f, 0.7f) * 0.7f;
    }
    shimmer(b, 0.2f, L, 0.12f, 0.06f, r);
    grains(b, 0.4f, L, 70.0f, [&](Buf& g, float at, Rand& rr) { click(g, at, rr.range(0.15f, 0.7f), rr); }, r,
           [L](float t) { return std::clamp((t - 0.3f) / (L * 0.6f), 0.0f, 1.0f) * std::clamp((L - t) / 0.5f, 0.0f, 1.0f); });
    ping(b, L * 0.72f, 0.25f, r);
    fades(b, 0.05f, 0.3f);
    normalize(b, level::kEvent - 6.0f);
    return b.sound();
}

}  // namespace

void addPurpleSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"purple_hum", [=] { return purpleHum(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("purple_crackle_{}", v), [=] { return purpleCrackle(rate, v); }});
    d.push_back({"purple_settle", [=] { return purpleSettle(rate); }});
    d.push_back({"purple_surge", [=] { return purpleSurge(rate); }});
}

}  // namespace lim::audio
