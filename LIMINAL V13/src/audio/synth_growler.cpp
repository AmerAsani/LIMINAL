// V13.3: Klaenge des Growlers (Entity 130, KREATUR_Growler.md).
//
//   Rauschen    das telepathische "Knurren": weisses Rauschen wie zwischen zwei Sendern - ein Bandpass wandert, Knistern,
//               ein tiefes, rauhes Brummen darin, pulsierende Stoerung. Ohne Raum (es ist im Kopf), als Schleife.
//   Klicks      Echoortung: trockene, metallische Klickfolgen (Draht, der zuckt), schneller werdend
//   Schleifen   der Schritt: ein dumpfer, weicher Aufschlag (die Masse aus Pfeifenreinigern), Draht kratzt ueber Beton,
//               das Gewirr raschelt nach
//   Ausholen    das Rauschen schwillt an, Draht kreischt unter Spannung (Ankuendigung)
//   Aufspiessen dichte metallische Stiche, ein nasses Knirschen
//   Verschmelzen Draht verdreht sich knarrend, zwei Brummen schweben gegeneinander
//   Zusammenbruch das Rauschen faellt in sich zusammen, der Draht erschlafft raschelnd
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

// Rauschen zwischen den Sendern: Bandpass wandert, Knistern, Aussetzer; amp(t) als Huelle
void radioNoise(Buf& b, float t0, float t1, float amp, Rand& r, const std::function<float(float)>& env, float sweepHz = 0.13f) {
    dsp::Svf bp;
    dsp::Pink pk;
    const float ph = r.range(0.0f, 6.0f);
    for (size_t i = b.at(t0); i < b.at(t1) && i < b.n(); ++i) {
        const float t = b.t(i);
        const float fc = 900.0f * std::exp(1.2f * std::sin(t * sweepHz * kTwoPi + ph) + 0.4f * std::sin(t * 0.71f + ph * 2.0f));
        if ((i & 31u) == 0) bp.set(fc, 1.6f, (float)b.rate);
        const float w = r.bi();
        float y = bp.bp(w) * 0.9f + pk.run(w) * 0.55f;
        // pulsierende Stoerung (7-13 Hz) und kurze Aussetzer
        const float trem = 0.65f + 0.35f * std::sin(t * kTwoPi * (9.0f + 3.0f * std::sin(t * 0.37f + ph)));
        y *= trem;
        b.x[i] += y * amp * env(t - t0);
    }
    grains(b, t0, t1, 22.0f, [&](Buf& bb, float t, Rand& rr) {  // Knistern
        const size_t a = bb.at(t);
        const float g = rr.range(0.2f, 1.0f) * amp * 1.6f * env(t - t0);
        Biquad hp = Biquad::make(Biquad::HP, rr.range(1500.0f, 4000.0f), 0.7f, (float)bb.rate);
        for (size_t k = 0; k < 90 && a + k < bb.n(); ++k) bb.x[a + k] += hp.run(k < 3 ? rr.bi() * 2.0f : rr.bi() * 0.1f * std::exp(-(float)k / 18.0f)) * g;
    }, r);
}

// tiefes, rauhes Brummen (kein Tier - Draht, der schwingt, und etwas darunter)
void wireGrowl(Buf& b, float at, float len, float base, float amp, Rand& r) {
    VoiceSpec vs;
    const float ph = r.range(0.0f, 6.0f);
    vs.f0 = [=](float u) { return base * (1.0f + 0.08f * std::sin(u * 5.0f + ph) + 0.04f * std::sin(u * 17.0f)); };
    vs.amp = [=](float u) { return amp * dsp::smooth01(u / 0.2f) * (1.0f - dsp::smooth01((u - 0.75f) / 0.25f)); };
    vs.formants = {r.range(250.0f, 320.0f), r.range(620.0f, 760.0f), 1900.0f};
    vs.bandwidths = {140.0f, 220.0f, 400.0f};
    vs.jitter = 0.06f, vs.shimmer = 0.3f, vs.subharm = 0.9f, vs.breath = 0.6f, vs.rough = 0.8f, vs.roughHz = r.range(19.0f, 27.0f);
    vs.tiltHz = 1400.0f;
    voice(b, at, len, vs, r);
}

void wireClick(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesMetalPlate(r.range(2600.0f, 5200.0f), 0.025f, r);
    sp.contact = 0.0003f, sp.noise = 0.5f, sp.noiseHz = 6500.0f, sp.noiseT60 = 0.003f;
    b.add(impact(sp, amp, 0.06f, b.rate, r), at);
}

void rustle(Buf& b, float t0, float t1, float amp, Rand& r) {  // Gewirr aus Pfeifenreinigern: weiches, faseriges Rascheln
    grains(b, t0, t1, 140.0f, [&](Buf& bb, float t, Rand& rr) {
        const size_t a = bb.at(t);
        Biquad bp = Biquad::make(Biquad::BP, rr.range(1800.0f, 6000.0f), 1.2f, (float)bb.rate);
        const float g = rr.range(0.1f, 1.0f) * amp;
        const int n = rr.irange(60, 240);
        for (int k = 0; k < n && a + (size_t)k < bb.n(); ++k) bb.x[a + (size_t)k] += bp.run(rr.bi()) * g * std::sin(3.14159f * (float)k / (float)n);
    }, r, [&](float t) { return envAHR(t - t0, 0.05f, std::max(0.01f, t1 - t0 - 0.2f), 0.15f); });
}

Sound growlerStatic(int rate) {
    Rand r(13000u);
    const float L = 11.0f;
    Buf b(L + 1.0f, rate);
    radioNoise(b, 0.0f, L + 1.0f, 0.5f, r, [](float) { return 1.0f; });
    // darin, mal lauter, mal fort, ein tiefes Brummen
    for (float at = 0.3f; at < L - 1.0f; at += r.range(2.2f, 3.6f)) wireGrowl(b, at, r.range(1.6f, 2.6f), r.range(44.0f, 58.0f), r.range(0.5f, 0.9f), r);
    loopify(b, 1.0f);
    normalize(b, level::kLoop + 2.0f);
    return b.sound(true);
}

Sound growlerClick(int rate, int v) {
    Rand r(13100u + (unsigned)v);
    Buf b(0.8f, rate);
    const int n = r.irange(4, 9);
    float at = 0.0f, gap = r.range(0.035f, 0.05f);
    for (int k = 0; k < n; ++k) {
        wireClick(b, at, r.range(0.6f, 1.0f), r);
        at += gap;
        gap *= r.range(0.78f, 0.9f);  // schneller werdend
    }
    trim(b);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

Sound growlerDrag(int rate, int v) {
    Rand r(13200u + (unsigned)v);
    Buf b(1.0f, rate);
    // weicher, schwerer Aufschlag
    ImpactSpec sp;
    sp.modes = modesConcrete(r.range(60.0f, 90.0f), r);
    sp.contact = 0.03f, sp.noise = 0.25f, sp.noiseHz = 500.0f, sp.noiseT60 = 0.08f;
    b.add(impact(sp, 0.9f, 0.4f, rate, r), 0.0f);
    // Draht kratzt ueber Beton
    CreakSpec c;
    c.rate0 = r.range(260.0f, 380.0f), c.rate1 = r.range(150.0f, 220.0f), c.jitter = 0.5f;
    c.formants = {r.range(2500.0f, 3200.0f), r.range(5200.0f, 6400.0f)}, c.q = 5.0f;
    creak(b, 0.02f, r.range(0.25f, 0.4f), c, 0.25f, r);
    rustle(b, 0.0f, r.range(0.4f, 0.65f), 0.35f, r);
    trim(b);
    normalize(b, level::kCreature - 7.0f);
    return b.sound();
}

Sound growlerSurge(int rate) {  // Ausholen: das Rauschen schwillt, Draht kreischt unter Spannung
    Rand r(13300u);
    const float L = 1.4f;
    Buf b(L + 0.6f, rate);
    radioNoise(b, 0.0f, L + 0.3f, 0.9f, r, [=](float t) { return std::pow(std::min(1.0f, t / L), 2.0f) * (t < L ? 1.0f : std::exp(-(t - L) * 9.0f)); }, 1.2f);
    CreakSpec c;
    c.rate0 = 90.0f, c.rate1 = 420.0f, c.jitter = 0.3f;
    c.formants = {1800.0f, 3900.0f, 7200.0f}, c.q = 9.0f;
    creak(b, 0.2f, L - 0.1f, c, 0.45f, r);
    wireGrowl(b, 0.0f, L + 0.2f, 52.0f, 1.0f, r);
    for (float at = 0.6f; at < L; at += r.range(0.05f, 0.11f)) wireClick(b, at, r.range(0.3f, 0.7f), r);
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

Sound growlerImpale(int rate) {
    Rand r(13400u);
    Buf b(1.4f, rate);
    grains(b, 0.0f, 0.35f, 90.0f, [&](Buf& bb, float t, Rand& rr) { wireClick(bb, t, rr.range(0.5f, 1.0f), rr); }, r);
    for (int k = 0; k < 6; ++k) bubble(b, r.range(0.05f, 0.4f), r.range(1.0f, 2.5f), 0.3f, r);
    ImpactSpec sp;
    sp.modes = modesWood(r.range(140.0f, 200.0f), 0.05f, r);  // dumpfes Knacken
    sp.contact = 0.006f, sp.noise = 0.6f, sp.noiseHz = 1200.0f, sp.noiseT60 = 0.05f;
    b.add(impact(sp, 1.0f, 0.3f, rate, r), 0.08f);
    radioNoise(b, 0.0f, 1.2f, 0.7f, r, [](float t) { return std::exp(-t * 2.5f); }, 2.0f);
    trim(b);
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}

Sound growlerMerge(int rate) {
    Rand r(13500u);
    const float L = 3.2f;
    Buf b(L + 0.6f, rate);
    for (int k = 0; k < 5; ++k) {
        CreakSpec c;
        c.rate0 = r.range(30.0f, 70.0f), c.rate1 = r.range(60.0f, 140.0f), c.jitter = 0.35f;
        c.formants = {r.range(600.0f, 1100.0f), r.range(1900.0f, 2800.0f), r.range(4200.0f, 5600.0f)}, c.q = 7.0f;
        creak(b, r.range(0.0f, L * 0.6f), r.range(0.6f, 1.4f), c, 0.3f, r);
    }
    wireGrowl(b, 0.0f, L, 47.0f, 0.9f, r);
    wireGrowl(b, 0.2f, L - 0.2f, 49.5f, 0.8f, r);  // zwei Brummen schweben gegeneinander
    rustle(b, 0.0f, L, 0.45f, r);
    radioNoise(b, 0.0f, L + 0.3f, 0.5f, r, [=](float t) { return std::sin(3.14159f * std::min(1.0f, t / L)); }, 0.6f);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound growlerCollapse(int rate) {
    Rand r(13600u);
    const float L = 2.6f;
    Buf b(L + 0.5f, rate);
    radioNoise(b, 0.0f, L, 0.8f, r, [=](float t) { return std::exp(-t * 1.6f) * (0.6f + 0.4f * std::sin(t * 40.0f)); }, 0.4f);
    rustle(b, 0.1f, 1.6f, 0.5f, r);
    ImpactSpec sp;
    sp.modes = modesConcrete(55.0f, r);
    sp.contact = 0.05f, sp.noise = 0.3f, sp.noiseHz = 300.0f, sp.noiseT60 = 0.12f;
    b.add(impact(sp, 1.0f, 0.6f, rate, r), 0.35f);
    filter(b, Biquad::LP, 5000.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

}  // namespace

void addGrowlerSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"growler_static", [=] { return growlerStatic(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("growler_click_{}", v), [=] { return growlerClick(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("growler_drag_{}", v), [=] { return growlerDrag(rate, v); }});
    d.push_back({"growler_surge", [=] { return growlerSurge(rate); }});
    d.push_back({"growler_impale", [=] { return growlerImpale(rate); }});
    d.push_back({"growler_merge", [=] { return growlerMerge(rate); }});
    d.push_back({"growler_collapse", [=] { return growlerCollapse(rate); }});
}

}  // namespace lim::audio
