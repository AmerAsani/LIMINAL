// V13.3: Klaenge des Dullers (KREATUR_Duller.md). Er hat keinen Mund - seine Stimme sind die Gelenke: trockenes Knacken
// von Knorpel, das in schnellen Folgen klickt, wenn die Glieder des Arms einrasten. Dazu, was er an der Wand tut:
//   duller_scrape_0..3  Fingerkuppen und Naegel schaben ueber Beton, trockene Haut raschelt - hinter/in der Wand (dumpf)
//   duller_bulge_0..1   der Putz woelbt sich: Knistern, Risse, rieselnde Kruemel, Aechzen des Betons
//   duller_burst        die Wand bricht: Schlag, Brocken prasseln zu Boden, der Arm gleitet heraus (Glieder rasten ein)
//   duller_retract      der Arm gleitet zurueck: Reiben von Haut an Beton, Klicken rueckwaerts
//   duller_step_0..3    nackte, weiche Sohlen, ab und zu ein knackendes Gelenk (leise; auf der Flucht lautlos)
//   duller_grab         toedlicher Zugriff: Druckstoss, Griff, Knacken, Schleifen durch den Beton
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {
// Knorpel: ein trockener, kurzer Klick mit hoher Resonanz (wie ein knackendes Fingergelenk)
void jointClick(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesWood(r.range(1400.0f, 2600.0f), 0.012f, r);
    sp.contact = 0.00025f, sp.noise = 0.55f, sp.noiseHz = r.range(2500.0f, 4500.0f), sp.noiseT60 = 0.003f;
    b.add(impact(sp, amp, 0.05f, b.rate, r), at);
    Biquad lp = Biquad::make(Biquad::LP, 260.0f, 0.7f, (float)b.rate);  // dumpfer Koerper des Gelenks
    for (size_t i = 0; i < b.at(0.025f) && b.at(at) + i < b.n(); ++i) b.x[b.at(at) + i] += lp.run(r.bi()) * amp * 0.5f * envAD(b.t(i), 0.001f, 0.012f);
}
// Beton: Bruchstueck, Riss
void concreteHit(Buf& b, float at, float f0, float amp, Rand& r, float len = 0.12f) {
    ImpactSpec sp;
    sp.modes = modesConcrete(f0, r);
    sp.contact = r.range(0.0004f, 0.0015f), sp.noise = 0.75f, sp.noiseHz = r.range(1500.0f, 3500.0f), sp.noiseT60 = 0.01f;
    b.add(impact(sp, amp, len, b.rate, r), at);
}
// Reibung von Haut an Beton (Haften und Gleiten), Band bp
void skinRub(Buf& b, float at, float len, float hz, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, hz, 1.2f, (float)b.rate);
    const float fr = r.range(30.0f, 55.0f);
    for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
        const float t = b.t(i);
        const float stick = 0.45f + 0.55f * std::pow(std::max(0.0f, std::sin(kTwoPi * fr * t + 0.4f * r.bi())), 3.0f);
        b.x[b.at(at) + i] += bp.run(r.bi()) * stick * amp * envAHR(t, 0.04f, len * 0.6f, len * 0.35f);
    }
}
void saturate(Buf& b, float drive) {
    for (float& v : b.x) v = std::tanh(v * drive) / std::tanh(drive);
}
}  // namespace

// Schaben in der Wand: zwei bis vier Finger zugleich (versetzte Striche), Kruemel, trockenes Rascheln, ein Knoechel klopft
Sound dullerScrape(int rate, int v) {
    Rand r(16000u + (unsigned)v);
    Buf b(1.6f, rate);
    const int fingers = r.irange(2, 4);
    const float len = r.range(0.55f, 0.95f);
    for (int f = 0; f < fingers; ++f) {
        const float at = r.range(0.0f, 0.12f);
        Biquad bp = Biquad::make(Biquad::BP, r.range(1400.0f, 3200.0f), 1.6f, (float)rate);
        const float fr = r.range(40.0f, 80.0f), ph = r.range(0.0f, 6.0f);
        for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
            const float t = b.t(i);
            const float stick = 0.4f + 0.6f * std::pow(std::max(0.0f, std::sin(kTwoPi * fr * t + ph + 0.6f * r.bi())), 2.0f);
            b.x[b.at(at) + i] += bp.run(r.bi()) * stick * 0.6f * envAHR(t, 0.03f, len * 0.7f, len * 0.25f);
        }
    }
    grains(b, 0.05f, len, 26.0f, [&](Buf& bb, float t, Rand& rr) { concreteHit(bb, t, rr.range(2500.0f, 5000.0f), rr.range(0.05f, 0.15f), rr, 0.02f); }, r);
    skinRub(b, r.range(0.1f, 0.4f), r.range(0.4f, 0.7f), r.range(500.0f, 900.0f), 0.35f, r);  // Haut, Rascheln
    if (r.chance(0.6f)) concreteHit(b, r.range(0.0f, len), r.range(150.0f, 260.0f), 0.5f, r, 0.2f);  // Knoechel
    if (r.chance(0.5f)) jointClick(b, r.range(0.2f, len), 0.25f, r);
    filter(b, Biquad::LP, 2200.0f, 0.7f);  // in der Wand: dumpf
    filter(b, Biquad::Peak, 180.0f, 1.0f, 5.0f);  // der Beton klingt mit
    trim(b);
    normalize(b, level::kCreature - 7.0f);
    return b.sound();
}

// Der Putz woelbt sich: Aechzen, Knistern und Risse, rieselnde Kruemel
Sound dullerBulge(int rate, int v) {
    Rand r(16100u + (unsigned)v);
    Buf b(1.6f, rate);
    CreakSpec c;  // Aechzen des Betons unter Druck (tief, langsam)
    c.rate0 = r.range(9.0f, 14.0f), c.rate1 = r.range(18.0f, 30.0f), c.jitter = 0.4f, c.formants = {120.0f, 340.0f, 900.0f}, c.q = 5.0f;
    creak(b, 0.0f, 1.1f, c, 0.6f, r);
    float at = r.range(0.05f, 0.2f);
    const int n = r.irange(5, 9);
    for (int k = 0; k < n; ++k) {
        concreteHit(b, at, r.range(1800.0f, 4200.0f), r.range(0.3f, 0.9f), r, 0.05f);
        at += r.range(0.04f, 0.18f);
    }
    grains(b, 0.2f, 1.3f, 40.0f, [&](Buf& bb, float t, Rand& rr) { concreteHit(bb, t, rr.range(4000.0f, 7000.0f), rr.range(0.03f, 0.1f), rr, 0.015f); }, r,
           [](float t) { return std::exp(-t * 1.5f); });  // Kruemel rieseln
    filter(b, Biquad::LP, 4500.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

// Durchbruch: Schlag in den Beton, Brocken prasseln, der Arm gleitet heraus - die Glieder rasten in schneller Folge ein
Sound dullerBurst(int rate) {
    Rand r(16200u);
    Buf b(2.0f, rate);
    concreteHit(b, 0.0f, 90.0f, 1.0f, r, 0.4f);
    concreteHit(b, 0.004f, 1800.0f, 0.8f, r, 0.08f);
    Biquad lp = Biquad::make(Biquad::LP, 140.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.25f); ++i) b.x[i] += lp.run(r.bi()) * 1.4f * envAD(b.t(i), 0.002f, 0.18f);
    // Brocken treffen den Boden (0,3 - 0,8 s), Kruemel
    for (int k = 0; k < 5; ++k) concreteHit(b, r.range(0.3f, 0.8f), r.range(400.0f, 900.0f), r.range(0.4f, 0.8f), r, 0.12f);
    grains(b, 0.0f, 1.2f, 70.0f, [&](Buf& bb, float t, Rand& rr) { concreteHit(bb, t, rr.range(3000.0f, 6500.0f), rr.range(0.04f, 0.15f), rr, 0.015f); }, r,
           [](float t) { return std::exp(-t * 2.0f); });
    // der Arm: Haut gleitet ueber Bruchkanten, sieben Glieder rasten ein
    skinRub(b, 0.06f, 0.5f, 700.0f, 0.5f, r);
    float at = 0.08f;
    for (int k = 0; k < 7; ++k) {
        jointClick(b, at, r.range(0.5f, 0.85f), r);
        at += r.range(0.035f, 0.07f);
    }
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

Sound dullerRetract(int rate) {
    Rand r(16300u);
    Buf b(1.5f, rate);
    skinRub(b, 0.0f, 0.9f, 650.0f, 0.6f, r);
    float at = 0.1f;
    for (int k = 0; k < 7; ++k) {  // die Glieder gleiten ineinander - Klicken, langsamer werdend
        jointClick(b, at, r.range(0.3f, 0.6f), r);
        at += 0.05f + 0.02f * (float)k;
    }
    grains(b, 0.0f, 0.8f, 18.0f, [&](Buf& bb, float t, Rand& rr) { concreteHit(bb, t, rr.range(3000.0f, 6000.0f), rr.range(0.03f, 0.1f), rr, 0.015f); }, r);
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

// nackte, weiche Sohle auf Beton; ab und zu ein knackendes Gelenk
Sound dullerStep(int rate, int v) {
    Rand r(16400u + (unsigned)v);
    Buf b(0.3f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 380.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.07f); ++i) b.x[i] += lp.run(r.bi()) * envAD(b.t(i), 0.008f, 0.05f) * 1.2f;
    Biquad bp = Biquad::make(Biquad::BP, r.range(900.0f, 1500.0f), 0.9f, (float)rate);  // Haut loest sich vom Boden
    for (size_t i = 0; i < b.at(0.05f); ++i) b.x[b.at(0.03f) + i] += bp.run(r.bi()) * envAD(b.t(i), 0.004f, 0.03f) * 0.4f;
    if (v % 2 == 1) jointClick(b, r.range(0.0f, 0.06f), 0.45f, r);
    trim(b);
    normalize(b, level::kStep - 2.0f);
    return b.sound();
}

// Zugriff (der lauteste Klang): Druckstoss, die Hand schliesst sich (Knacken, Griff), dann das Schleifen durch den Beton
Sound dullerGrab(int rate) {
    Rand r(16500u);
    Buf b(2.6f, rate);
    float ph = 0.0f;
    for (size_t i = 0; i < b.at(0.9f); ++i) {  // Druckstoss
        const float t = b.t(i);
        ph += kTwoPi * (30.0f + 60.0f * std::exp(-t * 10.0f)) / (float)rate;
        b.x[i] += std::sin(ph) * envAD(t, 0.003f, 0.5f);
    }
    concreteHit(b, 0.0f, 110.0f, 1.0f, r, 0.3f);
    float at = 0.02f;
    for (int k = 0; k < 9; ++k) {  // die Finger schliessen sich, Gelenke knacken
        jointClick(b, at, r.range(0.6f, 1.0f), r);
        at += r.range(0.012f, 0.03f);
    }
    // Schleifen: der Koerper wird gegen die Wand gezogen - Rauschen mit Haften und Gleiten, tiefer werdend
    Buf g(1.8f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < g.at(1.6f); ++i) {
        const float t = g.t(i), u = t / 1.6f;
        f.set(1400.0f - 900.0f * u, 1.3f, (float)rate);
        const float stick = 0.5f + 0.5f * std::sin(kTwoPi * (22.0f - 10.0f * u) * t);
        g.x[i] = f.bp(r.bi()) * stick * envAHR(t, 0.05f, 1.1f, 0.45f);
    }
    b.add(g, 0.25f, 0.9f);
    for (int k = 0; k < 6; ++k) concreteHit(b, r.range(0.3f, 1.6f), r.range(300.0f, 800.0f), r.range(0.3f, 0.7f), r, 0.1f);
    normalize(b, -16.0f, 0.9f);
    saturate(b, 1.8f);
    trim(b);
    normalize(b, -15.0f, 0.9f);
    return b.sound();
}

void addDullerSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 4; ++v) d.push_back({std::format("duller_scrape_{}", v), [=] { return dullerScrape(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("duller_bulge_{}", v), [=] { return dullerBulge(rate, v); }});
    d.push_back({"duller_burst", [=] { return dullerBurst(rate); }});
    d.push_back({"duller_retract", [=] { return dullerRetract(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("duller_step_{}", v), [=] { return dullerStep(rate, v); }});
    d.push_back({"duller_grab", [=] { return dullerGrab(rate); }});
}

}  // namespace lim::audio
