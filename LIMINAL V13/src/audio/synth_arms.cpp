// V13.3: Klaenge der Arms / Les Bras (KREATUR_Arms.md). Alles kommt aus einem Blechschacht: dumpf, mit hohlem, kurzem
// Metallnachhall. Die Stimme sitzt irgendwo hinten im Schacht - heiser, ein Rasseln ohne Worte; im Licht ein Todesschrei.
//   arms_scrape_0..3  dumpfes Schaben und Klopfen an Blech, Krallen kratzen, leichtes Streifen (leise - die Gefahr)
//   arms_vent         die Lueftung oeffnet sich: ein kaum hoerbares Knarzen, ein Blechticken
//   arms_hiss         Ausholen: Krallen klicken auseinander, Sehnen knarzen, ein heiseres Rasseln aus dem Schacht
//   arms_snap         die Krallen schnappen ins Leere
//   arms_burn         Licht: heiserer Todesschrei aus dem Schacht, Zischen verbrennender Haut, der Arm schnellt zurueck
//   arms_retract      Haut reibt an Blech, entfernt sich im Schacht
//   arms_take         toedlicher Zugriff: Krallen, Ruck nach oben, Schlaege gegen die Schachtwaende, die sich entfernen
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {
// Blechschacht: kurzer, hohler Nachhall mit metallischer Faerbung
void duct(Buf& b, float wet, std::uint32_t seed) {
    dsp::RoomSpec rs;
    rs.t60Low = 0.5f, rs.t60Mid = 0.75f, rs.t60High = 0.35f, rs.size = 0.35f, rs.preDelay = 0.004f, rs.diffusion = 0.45f;
    filter(b, Biquad::Peak, 620.0f, 2.5f, 6.0f);
    filter(b, Biquad::Peak, 1450.0f, 3.0f, 4.0f);
    reverb(b, rs, wet, 1.0f - wet * 0.5f, seed);
}
// Schlag auf Blech (Knoechel, Ellbogen)
void sheetHit(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesMetalPlate(r.range(140.0f, 320.0f), 0.35f, r);
    sp.contact = r.range(0.004f, 0.008f), sp.noise = 0.35f, sp.noiseHz = 900.0f;
    b.add(impact(sp, amp, 0.6f, b.rate, r), at);
}
// Kralle (Horn): harter, kurzer Klick
void clawClick(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesCeramic(r.range(1800.0f, 3200.0f), 0.01f, r);
    sp.contact = 0.0003f, sp.noise = 0.5f, sp.noiseHz = 4500.0f, sp.noiseT60 = 0.003f;
    b.add(impact(sp, amp, 0.04f, b.rate, r), at);
}
// Kratzen einer Kralle ueber Blech
void clawScratch(Buf& b, float at, float len, float amp, Rand& r) {
    dsp::Svf f;
    const float f0 = r.range(2200.0f, 3800.0f), fr = r.range(45.0f, 90.0f);
    for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
        const float t = b.t(i);
        f.set(f0 * (1.0f + 0.15f * std::sin(t * 9.0f)), 6.0f, (float)b.rate);
        const float stick = 0.4f + 0.6f * std::pow(std::max(0.0f, std::sin(kTwoPi * fr * t)), 2.0f);
        b.x[b.at(at) + i] += f.bp(r.bi()) * stick * amp * envAHR(t, 0.02f, len * 0.6f, len * 0.35f);
    }
}
// Haut streift Blech (leises Reiben)
void rub(Buf& b, float at, float len, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, r.range(500.0f, 900.0f), 0.9f, (float)b.rate);
    for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[b.at(at) + i] += bp.run(r.bi()) * amp * (0.7f + 0.3f * std::sin(t * 23.0f)) * envAHR(t, 0.1f, len * 0.5f, len * 0.4f);
    }
}
// heisere Stimme hinten im Schacht (ohne Worte): f0 faellt, rau, unterharmonisch
void rasp(Buf& b, float at, float len, float f0a, float f0b, float amp, float rough, Rand& r) {
    Buf v(len + 0.1f, b.rate);
    VoiceSpec vs;
    vs.f0 = [=](float u) { return f0a + (f0b - f0a) * u; };
    vs.amp = [=](float u) { return dsp::smooth01(u / 0.08f) * (1.0f - dsp::smooth01((u - 0.75f) / 0.25f)); };
    vs.formants = {r.range(420.0f, 520.0f), r.range(950.0f, 1150.0f), 2300.0f}, vs.bandwidths = {150.0f, 220.0f, 320.0f};
    vs.jitter = 0.08f, vs.shimmer = 0.35f, vs.subharm = 0.7f, vs.breath = 0.9f, vs.rough = rough, vs.roughHz = r.range(24.0f, 36.0f);
    vs.tiltHz = 2600.0f;
    voice(v, 0.0f, len, vs, r);
    b.add(v, at, amp);
}
void saturate(Buf& b, float drive) {
    for (float& v : b.x) v = std::tanh(v * drive) / std::tanh(drive);
}
}  // namespace

Sound armsScrape(int rate, int v) {
    Rand r(17000u + (unsigned)v);
    Buf b(2.2f, rate);
    const float len = r.range(0.9f, 1.4f);
    rub(b, 0.0f, len, 0.35f, r);
    float at = r.range(0.0f, 0.3f);
    const int knocks = r.irange(1, 3);
    for (int k = 0; k < knocks; ++k) {
        sheetHit(b, at, r.range(0.4f, 0.8f), r);
        at += r.range(0.25f, 0.6f);
    }
    if (v % 2 == 0) clawScratch(b, r.range(0.1f, 0.6f), r.range(0.25f, 0.5f), 0.35f, r);
    else for (int k = 0; k < 3; ++k) clawClick(b, r.range(0.1f, len), 0.25f, r);
    filter(b, Biquad::LP, 2400.0f, 0.7f);  // durch Blech und Decke
    duct(b, 0.45f, 17100u + (unsigned)v);
    trim(b);
    normalize(b, level::kCreature - 9.0f);
    return b.sound();
}

Sound armsVent(int rate) {
    Rand r(17200u);
    Buf b(1.0f, rate);
    CreakSpec c;
    c.rate0 = 60.0f, c.rate1 = 90.0f, c.jitter = 0.3f, c.formants = {1400.0f, 3100.0f}, c.q = 9.0f;
    creak(b, 0.05f, 0.45f, c, 0.25f, r);
    clawClick(b, 0.55f, 0.2f, r);
    duct(b, 0.3f, 17201u);
    trim(b);
    normalize(b, level::kCreature - 14.0f);
    return b.sound();
}

Sound armsHiss(int rate) {
    Rand r(17300u);
    Buf b(1.2f, rate);
    for (int k = 0; k < 3; ++k) clawClick(b, 0.02f + 0.03f * (float)k + r.range(0.0f, 0.01f), 0.7f, r);  // Krallen auseinander
    CreakSpec c;
    c.rate0 = 14.0f, c.rate1 = 40.0f, c.jitter = 0.35f, c.formants = {280.0f, 700.0f, 1600.0f}, c.q = 7.0f;
    creak(b, 0.0f, 0.55f, c, 0.6f, r);  // Sehnen spannen sich
    rasp(b, 0.05f, 0.6f, 95.0f, 75.0f, 0.55f, 0.8f, r);
    duct(b, 0.35f, 17301u);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound armsSnap(int rate) {
    Rand r(17400u);
    Buf b(0.6f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.at(0.12f); ++i) {  // Luft
        const float t = b.t(i), u = t / 0.12f;
        f.set(800.0f + 2400.0f * u, 0.8f, (float)rate);
        b.x[i] += f.bp(r.bi()) * dsp::smooth01(u) * 0.6f;
    }
    for (int k = 0; k < 3; ++k) clawClick(b, 0.12f + r.range(0.0f, 0.012f), 1.0f, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

// Licht: heiserer, todesschreiartiger Laut aus dem Schacht; Zischen und Knistern verbrennender Haut; Zurueckschnellen
Sound armsBurn(int rate) {
    Rand r(17500u);
    Buf b(2.6f, rate);
    rasp(b, 0.0f, 1.3f, 260.0f, 120.0f, 1.0f, 1.0f, r);
    rasp(b, 0.03f, 1.2f, 245.0f, 105.0f, 0.6f, 1.0f, r);
    grains(b, 0.0f, 1.8f, 160.0f, [&](Buf& bb, float t, Rand& rr) {  // Fett knistert, Haut zischt
        const size_t a = bb.at(t);
        Biquad hp = Biquad::make(Biquad::HP, rr.range(3000.0f, 7000.0f), 0.7f, (float)bb.rate);
        for (size_t i = 0; i < 160 && a + i < bb.n(); ++i) bb.x[a + i] += hp.run(rr.bi()) * rr.range(0.05f, 0.25f) * std::exp(-(float)i / 40.0f);
    }, r, [](float t) { return std::exp(-t * 1.2f); });
    Biquad hp = Biquad::make(Biquad::HP, 2500.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(1.6f); ++i) b.x[i] += hp.run(r.bi()) * 0.1f * envAHR(b.t(i), 0.05f, 0.6f, 0.9f);  // Zischen
    rub(b, 0.15f, 0.6f, 0.5f, r);
    for (int k = 0; k < 3; ++k) sheetHit(b, 0.3f + 0.18f * (float)k, 0.5f - 0.12f * (float)k, r);
    duct(b, 0.4f, 17501u);
    saturate(b, 1.4f);
    trim(b);
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}

Sound armsRetract(int rate) {
    Rand r(17600u);
    Buf b(2.0f, rate);
    rub(b, 0.0f, 1.4f, 0.6f, r);
    sheetHit(b, 0.6f, 0.4f, r);
    applyEnv(b, [](float t) { return std::exp(-t * 1.4f); });  // entfernt sich
    filter(b, Biquad::LP, 1800.0f, 0.7f);
    duct(b, 0.5f, 17601u);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

// Zugriff: Druckstoss, die Krallen schliessen sich, ein Ruck nach oben; dann im Schacht Schlaege gegen das Blech und ein
// erstickter Schrei, die sich rasch entfernen
Sound armsTake(int rate) {
    Rand r(17700u);
    Buf b(3.2f, rate);
    float ph = 0.0f;
    for (size_t i = 0; i < b.at(0.9f); ++i) {
        const float t = b.t(i);
        ph += kTwoPi * (34.0f + 70.0f * std::exp(-t * 10.0f)) / (float)rate;
        b.x[i] += std::sin(ph) * envAD(t, 0.003f, 0.5f);
    }
    for (int k = 0; k < 3; ++k) clawClick(b, 0.01f * (float)k, 1.0f, r);
    Buf far(3.0f, rate);
    VoiceSpec vs;  // der eigene, erstickte Schrei
    vs.f0 = [](float u) { return 380.0f + 120.0f * std::sin(u * 3.0f) - 150.0f * u; };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.05f) * (1.0f - u); };
    vs.formants = {750.0f, 1200.0f, 2600.0f}, vs.bandwidths = {120.0f, 180.0f, 300.0f};
    vs.jitter = 0.05f, vs.shimmer = 0.2f, vs.breath = 0.5f, vs.rough = 0.5f, vs.tiltHz = 3000.0f;
    voice(far, 0.2f, 1.4f, vs, r);
    float at = 0.25f;
    for (int k = 0; k < 7; ++k) {
        sheetHit(far, at, 1.0f - 0.1f * (float)k, r);
        at += r.range(0.18f, 0.38f);
    }
    rub(far, 0.1f, 2.0f, 0.5f, r);
    applyEnv(far, [](float t) { return std::exp(-t * 1.1f); });
    filter(far, Biquad::LP, 1600.0f, 0.7f);
    duct(far, 0.55f, 17701u);
    normalize(far, -18.0f, 0.9f);
    b.add(far, 0.15f, 0.9f);
    normalize(b, -16.0f, 0.9f);
    saturate(b, 1.6f);
    trim(b);
    normalize(b, -15.0f, 0.9f);
    return b.sound();
}

void addArmsSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 4; ++v) d.push_back({std::format("arms_scrape_{}", v), [=] { return armsScrape(rate, v); }});
    d.push_back({"arms_vent", [=] { return armsVent(rate); }});
    d.push_back({"arms_hiss", [=] { return armsHiss(rate); }});
    d.push_back({"arms_snap", [=] { return armsSnap(rate); }});
    d.push_back({"arms_burn", [=] { return armsBurn(rate); }});
    d.push_back({"arms_retract", [=] { return armsRetract(rate); }});
    d.push_back({"arms_take", [=] { return armsTake(rate); }});
}

}  // namespace lim::audio
