// V11: Klaenge der Kreaturen und Halluzinationen (ENTITY_BEHAVIOR.md).
//
// Jede Kreatur hat eine eigene, wiedererkennbare Signatur aus ihrem Koerper heraus:
//   Laecheln   kaum hoerbar: Druck in der Tiefe, nasses Klicken von Zaehnen; oeffnet es sich, ein
//              langes Einatmen durch Zaehne und ein glasiger, steigender Ton (Ankuendigung)
//   Hund       menschlicher Rachen, tierisch benutzt: heiseres Knurren mit Unterharmonischen,
//              rauhes Bellen, nasales Winseln; Handflaechen und Krallen auf Beton
//   Buendel    Fleisch: Klatschen von Haenden, Knacken von Gelenken, klebriges Schleifen; lockt
//              mit Knoecheln an Rohren
//   Motten     Fluegelschlag (25-40 Hz), Schuppenrascheln
//   Tiefe      Unterbass, Wasser, das verdraengt wird
//   V13.2: Faceling, Skin-Stealer, Wretch, Death Rat, Fensterwesen (Abschnitt unten)
// Halluzinationen sind trocken (ohne eigenen Raum) und sprechen keine Woerter.
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;
using dsp::Mode;

namespace {

void wetClicks(Buf& b, float t0, float t1, float perSec, float amp, Rand& r) {
    grains(b, t0, t1, perSec, [&](Buf& bb, float t, Rand& rr) {
        if (rr.chance(0.5f)) bubble(bb, t, rr.range(0.9f, 2.2f), amp * rr.range(0.3f, 1.0f), rr);
        else {
            const size_t a = bb.at(t);
            Biquad bp = Biquad::make(Biquad::BP, rr.range(1800.0f, 4000.0f), 4.0f, (float)bb.rate);
            for (size_t i = 0; i < 120 && a + i < bb.n(); ++i) bb.x[a + i] += bp.run(i < 4 ? rr.bi() * 2.0f : 0.0f) * amp;
        }
    }, r);
}

void enamelClick(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesCeramic(r.range(2800.0f, 4600.0f), 0.012f, r);
    sp.contact = 0.0004f, sp.noise = 0.3f, sp.noiseHz = 6000.0f, sp.noiseT60 = 0.002f;
    b.add(impact(sp, amp, 0.05f, b.rate, r), at);
}

// --- Laecheln ---------------------------------------------------------------------------------------
Sound smilerPresence(int rate) {
    Rand r(6000u);
    const float L = 13.0f;
    Buf b(L + 1.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 55.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = lp.run(br.run(r.bi())) * 3.0f * (0.6f + 0.4f * std::sin(t * 0.9f + 1.0f) * std::sin(t * 0.31f));
    }
    // langsamer, nasser Atem der Dunkelheit
    for (float at = 0.4f; at < L - 1.0f; at += r.range(4.2f, 5.6f)) {
        breathe(b, at, r.range(1.6f, 2.2f), true, 0.12f, 0.1f, r);
        breathe(b, at + 2.3f, r.range(1.8f, 2.4f), false, 0.1f, 0.05f, r);
    }
    wetClicks(b, 0.0f, L, 0.9f, 0.06f, r);
    loopify(b, 1.0f);
    normalize(b, level::kLoop - 4.0f);
    return b.sound(true);
}

Sound smilerTeeth(int rate, int v) {
    Rand r(6100u + (unsigned)v);
    Buf b(0.7f, rate);
    float at = 0.0f;
    const int n = r.irange(3, 7);
    for (int k = 0; k < n; ++k) {
        enamelClick(b, at, r.range(0.5f, 1.0f), r);
        at += r.range(0.025f, 0.08f);
    }
    wetClicks(b, 0.0f, at + 0.1f, 40.0f, 0.15f, r);
    trim(b);
    normalize(b, level::kCreature - 8.0f);
    return b.sound();
}

// Ankuendigung: das Laecheln oeffnet sich (1,6 s)
Sound smilerOpen(int rate) {
    Rand r(6200u);
    Buf b(1.9f, rate);
    // Lippen loesen sich klebrig
    wetClicks(b, 0.0f, 0.5f, 90.0f, 0.12f, r);
    // langes Einatmen durch die Zaehne, wird heller
    breathe(b, 0.2f, 1.35f, true, 0.6f, 1.0f, r);
    // glasiger Ton, steigend, mit Schwebung
    float p1 = 0, p2 = 0;
    for (size_t i = 0; i < b.at(1.6f); ++i) {
        const float t = b.t(i), u = t / 1.6f;
        const float f = 1150.0f * std::pow(1.7f, u) * (1.0f + 0.006f * std::sin(kTwoPi * 5.3f * t));
        p1 += kTwoPi * f / (float)rate;
        p2 += kTwoPi * f * 1.012f / (float)rate;
        b.x[i] += (std::sin(p1) + 0.6f * std::sin(p2) + 0.15f * std::sin(p1 * 2.0f)) * 0.05f * dsp::smooth01(u * 1.3f);
    }
    for (int k = 0; k < 4; ++k) enamelClick(b, r.range(0.6f, 1.5f), 0.3f, r);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound smilerAttack(int rate) {
    Rand r(6300u);
    Buf b(1.0f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.at(0.35f); ++i) {  // Luftstoss
        const float t = b.t(i), u = t / 0.35f;
        f.set(300.0f + 3000.0f * u * u, 0.8f, (float)rate);
        b.x[i] += f.bp(r.bi()) * dsp::smooth01(u) * 1.4f;
    }
    ImpactSpec snap;
    snap.modes = modesCeramic(r.range(1600.0f, 2200.0f), 0.04f, r);
    snap.contact = 0.0005f, snap.noise = 0.8f, snap.noiseHz = 3500.0f;
    b.add(impact(snap, 1.0f, 0.2f, rate, r), 0.34f);
    Biquad lp = Biquad::make(Biquad::LP, 120.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.2f); ++i) b.x[b.at(0.34f) + i] += lp.run(r.bi()) * 1.5f * envAD(b.t(i), 0.002f, 0.12f);
    wetClicks(b, 0.34f, 0.6f, 120.0f, 0.2f, r);
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

Sound smilerFade(int rate) {
    Rand r(6400u);
    Buf b(2.0f, rate);
    breathe(b, 0.0f, 1.7f, false, 0.5f, 0.1f, r);
    filter(b, Biquad::LP, 1500.0f, 0.7f);
    enamelClick(b, 0.25f, 0.4f, r);
    trim(b);
    normalize(b, level::kCreature - 8.0f);
    return b.sound();
}

// --- Hund ---------------------------------------------------------------------------------------------
Sound houndGrowl(int rate, int v) {
    Rand r(6500u + (unsigned)v);
    const float len = r.range(1.5f, 2.3f);
    Buf b(len + 0.2f, rate);
    VoiceSpec vs;
    const float base = r.range(68.0f, 92.0f), ph = r.range(0.0f, 6.0f);
    vs.f0 = [=](float u) { return base * (1.0f + 0.12f * std::sin(u * 7.0f + ph) + 0.05f * std::sin(u * 23.0f)); };
    vs.amp = [=](float u) { return dsp::smooth01(u / 0.15f) * (1.0f - dsp::smooth01((u - 0.8f) / 0.2f)) * (0.75f + 0.25f * std::sin(u * 11.0f + ph)); };
    vs.formants = {r.range(360.0f, 450.0f), r.range(780.0f, 950.0f), r.range(2200.0f, 2500.0f), 3300.0f};
    vs.bandwidths = {120.0f, 160.0f, 220.0f, 300.0f};
    vs.jitter = 0.05f, vs.shimmer = 0.25f, vs.subharm = 0.75f, vs.breath = 0.4f, vs.rough = 0.6f, vs.roughHz = r.range(24.0f, 34.0f);
    vs.tiltHz = 2200.0f;
    voice(b, 0.0f, len, vs, r);
    wetClicks(b, 0.0f, len, 6.0f, 0.04f, r);  // Speichel
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound houndBark(int rate, int v) {
    Rand r(6600u + (unsigned)v);
    Buf b(0.7f, rate);
    VoiceSpec vs;
    const float f0 = r.range(360.0f, 460.0f);
    vs.f0 = [=](float u) { return f0 * (1.0f - 0.5f * dsp::smooth01(u / 0.45f)); };
    vs.amp = [](float u) { return (u < 0.04f ? u / 0.04f : 1.0f) * std::pow(std::max(0.0f, 1.0f - u), 1.5f); };
    vs.formants = {r.range(650.0f, 780.0f), r.range(1150.0f, 1350.0f), 2600.0f, 3500.0f};
    vs.bandwidths = {140.0f, 180.0f, 260.0f, 320.0f};
    vs.jitter = 0.07f, vs.shimmer = 0.3f, vs.subharm = 0.45f, vs.breath = 0.7f, vs.rough = 0.4f;
    vs.tiltHz = 3800.0f;
    voice(b, 0.0f, 0.42f, vs, r);
    breathe(b, 0.0f, 0.12f, false, 0.6f, 1.0f, r);
    trim(b);
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}

Sound houndWhine(int rate) {
    Rand r(6700u);
    Buf b(1.6f, rate);
    VoiceSpec vs;
    vs.f0 = [](float u) { return 640.0f + 260.0f * std::sin(u * 3.14159f) - 120.0f * u; };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.1f) * (1.0f - dsp::smooth01((u - 0.7f) / 0.3f)) * (0.8f + 0.2f * std::sin(u * 40.0f)); };
    vs.formants = {420.0f, 2000.0f, 3000.0f}, vs.bandwidths = {80.0f, 120.0f, 200.0f};  // nasal
    vs.jitter = 0.03f, vs.shimmer = 0.15f, vs.breath = 0.5f, vs.tiltHz = 4200.0f;
    voice(b, 0.0f, 1.4f, vs, r);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}

Sound houndPant(int rate, int v) {
    Rand r(6800u + (unsigned)v);
    Buf b(1.6f, rate);
    float at = 0.0f;
    while (at < 1.3f) {
        breathe(b, at, 0.11f, false, 0.6f, 0.7f, r);
        breathe(b, at + 0.13f, 0.1f, true, 0.45f, 0.8f, r);
        at += r.range(0.25f, 0.31f);
    }
    wetClicks(b, 0.0f, 1.3f, 10.0f, 0.05f, r);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

// Handflaeche und Krallen auf Beton
Sound houndStep(int rate, int v) {
    Rand r(6900u + (unsigned)v);
    Buf b(0.25f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 320.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.05f); ++i) b.x[i] += lp.run(r.bi()) * envAD(b.t(i), 0.006f, 0.03f) * 1.5f;
    const int claws = r.irange(1, 4);
    for (int k = 0; k < claws; ++k) {
        ImpactSpec sp;
        sp.modes = modesCeramic(r.range(2400.0f, 4200.0f), 0.008f, r);
        sp.contact = 0.0003f, sp.noise = 0.4f, sp.noiseHz = 5000.0f;
        b.add(impact(sp, r.range(0.3f, 0.7f), 0.03f, rate, r), r.range(0.004f, 0.035f));
    }
    trim(b);
    normalize(b, level::kStep + 1.0f);
    return b.sound();
}

Sound houndSniff(int rate, int v) {
    Rand r(7000u + (unsigned)v);
    Buf b(1.0f, rate);
    float at = 0.0f;
    const int n = r.irange(2, 4);
    for (int k = 0; k < n; ++k) {
        breathe(b, at, r.range(0.07f, 0.11f), true, 0.8f, 1.0f, r);
        at += r.range(0.12f, 0.2f);
    }
    trim(b);
    normalize(b, level::kCreature - 8.0f);
    return b.sound();
}

Sound houndSnap(int rate) {
    Rand r(7100u);
    Buf b(0.8f, rate);
    ImpactSpec jaw;
    jaw.modes = modesCeramic(r.range(1500.0f, 2200.0f), 0.03f, r);
    jaw.contact = 0.0005f, jaw.noise = 0.6f, jaw.noiseHz = 3000.0f;
    b.add(impact(jaw, 1.0f, 0.15f, rate, r), 0.12f);
    VoiceSpec vs;
    vs.f0 = [](float u) { return 150.0f - 40.0f * u; };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.1f) * (1.0f - u); };
    vs.formants = {550.0f, 1000.0f, 2400.0f}, vs.subharm = 0.8f, vs.rough = 0.7f, vs.breath = 0.6f, vs.tiltHz = 2600.0f;
    voice(b, 0.0f, 0.45f, vs, r);
    trim(b);
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}

// Ansturm: Haende und Fuesse in schneller werdender Folge
Sound houndCharge(int rate) {
    Rand r(7200u);
    Buf b(1.2f, rate);
    float at = 0.0f, gap = 0.16f;
    while (at < 0.95f) {
        Buf s(0.25f, rate);
        Biquad lp = Biquad::make(Biquad::LP, 300.0f, 0.7f, (float)rate);
        for (size_t i = 0; i < s.at(0.05f); ++i) s.x[i] = lp.run(r.bi()) * envAD(s.t(i), 0.004f, 0.025f);
        ImpactSpec sp;
        sp.modes = modesCeramic(r.range(2400.0f, 4200.0f), 0.008f, r);
        sp.contact = 0.0003f;
        s.add(impact(sp, 0.5f, 0.03f, rate, r), r.range(0.003f, 0.02f));
        b.add(s, at, r.range(0.6f, 1.0f));
        at += gap * r.range(0.7f, 1.2f);
        gap = std::max(0.07f, gap * 0.9f);
    }
    breathe(b, 0.1f, 0.8f, false, 0.3f, 1.0f, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

// --- Buendel ------------------------------------------------------------------------------------------
Sound clumpKnock(int rate, int v) {
    Rand r(7300u + (unsigned)v);
    Buf b(2.6f, rate);
    ImpactSpec sp;
    sp.modes = modesMetalPipe(r.range(80.0f, 160.0f), 1.0f, r);
    sp.contact = 0.006f, sp.noise = 0.4f, sp.noiseHz = 700.0f;  // Faust und Knoechel: weich, schwer
    float at = 0.0f;
    const int n = r.irange(2, 4);
    for (int k = 0; k < n; ++k) {
        b.add(impact(sp, r.range(0.7f, 1.0f), 1.4f, rate, r), at);
        at += r.range(0.22f, 0.5f);  // unregelmaessig, aber bewusst
    }
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}

Sound clumpSlap(int rate, int v) {
    Rand r(7400u + (unsigned)v);
    Buf b(0.3f, rate);
    // Haut auf Beton: scharfer Klatsch (1-3 kHz) und Koerper
    Biquad bp = Biquad::make(Biquad::BP, r.range(1200.0f, 2400.0f), 0.8f, (float)rate);
    for (size_t i = 0; i < b.at(0.04f); ++i) b.x[i] += bp.run(r.bi()) * envAD(b.t(i), 0.0008f, 0.02f) * 2.0f;
    Biquad lp = Biquad::make(Biquad::LP, 220.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.06f); ++i) b.x[i] += lp.run(r.bi()) * envAD(b.t(i), 0.004f, 0.04f) * 1.4f;
    wetClicks(b, 0.01f, 0.08f, 80.0f, 0.15f, r);
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

Sound clumpDrag(int rate, int v) {
    Rand r(7500u + (unsigned)v);
    const float len = r.range(0.9f, 1.4f);
    Buf b(len + 0.3f, rate);
    CreakSpec c;
    c.rate0 = r.range(140.0f, 200.0f), c.rate1 = r.range(220.0f, 320.0f), c.jitter = 0.25f;
    c.formants = {r.range(800.0f, 1000.0f), r.range(2000.0f, 2400.0f)}, c.q = 6.0f;
    creak(b, 0.0f, len, c, 0.4f, r);
    wetClicks(b, 0.0f, len, 50.0f, 0.12f, r);
    Biquad lp = Biquad::make(Biquad::LP, 400.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(len); ++i) b.x[i] += lp.run(r.bi()) * 0.5f * envAHR(b.t(i), 0.1f, len - 0.3f, 0.2f);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound clumpCrack(int rate, int v) {
    Rand r(7600u + (unsigned)v);
    Buf b(0.7f, rate);
    float at = 0.0f;
    const int n = r.irange(3, 6);
    for (int k = 0; k < n; ++k) {
        ImpactSpec sp;
        sp.modes = modesWood(r.range(800.0f, 1600.0f), 0.015f, r);
        sp.contact = 0.0003f, sp.noise = 0.7f, sp.noiseHz = 3000.0f, sp.noiseT60 = 0.004f;
        b.add(impact(sp, r.range(0.4f, 1.0f), 0.04f, rate, r), at);
        at += r.range(0.03f, 0.12f);
    }
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

// Ankuendigung: der lange Arm holt aus - Sehnen spannen sich, nasses Einatmen
Sound clumpReach(int rate) {
    Rand r(7700u);
    Buf b(1.5f, rate);
    CreakSpec c;
    c.rate0 = 8.0f, c.rate1 = 26.0f, c.jitter = 0.35f, c.formants = {300.0f, 720.0f, 1500.0f}, c.q = 7.0f;
    creak(b, 0.0f, 1.25f, c, 0.8f, r);
    breathe(b, 0.15f, 1.1f, true, 0.4f, 0.8f, r);
    wetClicks(b, 0.0f, 1.2f, 30.0f, 0.1f, r);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound clumpGrab(int rate) {
    Rand r(7800u);
    Buf b(0.9f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.at(0.18f); ++i) {
        const float t = b.t(i), u = t / 0.18f;
        f.set(3000.0f - 2400.0f * u, 1.0f, (float)rate);
        b.x[i] += f.bp(r.bi()) * dsp::smooth01(u * 2.0f) * 1.2f;
    }
    Buf s = Buf(0.3f, rate);
    Biquad bp = Biquad::make(Biquad::BP, 1700.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < s.at(0.05f); ++i) s.x[i] = bp.run(r.bi()) * envAD(s.t(i), 0.0008f, 0.03f) * 2.5f;
    b.add(s, 0.17f);
    Biquad lp = Biquad::make(Biquad::LP, 160.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.15f); ++i) b.x[b.at(0.17f) + i] += lp.run(r.bi()) * envAD(b.t(i), 0.003f, 0.1f) * 1.6f;
    wetClicks(b, 0.17f, 0.5f, 100.0f, 0.2f, r);
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

// --- Motten -------------------------------------------------------------------------------------------
void wingbeats(Buf& b, float t0, float t1, float hz, float amp, Rand& r, const std::function<float(float)>& env) {
    Biquad lp = Biquad::make(Biquad::LP, 1800.0f, 0.7f, (float)b.rate), hp = Biquad::make(Biquad::HP, 120.0f, 0.7f, (float)b.rate);
    float ph = r.uni();
    for (size_t i = b.at(t0); i < b.at(t1) && i < b.n(); ++i) {
        const float t = b.t(i);
        ph += hz * (1.0f + 0.08f * r.bi()) / (float)b.rate;
        if (ph >= 1.0f) ph -= 1.0f;
        const float stroke = std::pow(std::sin(kPi * ph), 6.0f);  // Abschlag: Luftstoss
        b.x[i] += hp.run(lp.run(r.bi())) * stroke * amp * env(t);
    }
}

Sound mothFlutter(int rate) {
    Rand r(7900u);
    const float L = 7.0f;
    Buf b(L + 0.6f, rate);
    for (int m = 0; m < 8; ++m) {
        const float hz = r.range(24.0f, 38.0f), ph = r.range(0.0f, 6.0f), amp = r.range(0.3f, 1.0f);
        wingbeats(b, 0.0f, L + 0.6f, hz, amp, r, [=](float t) { return 0.6f + 0.4f * std::sin(t * 1.3f + ph); });
    }
    loopify(b, 0.6f);
    normalize(b, level::kCreature - 6.0f);
    return b.sound(true);
}

Sound mothTakeoff(int rate) {
    Rand r(8000u);
    Buf b(1.4f, rate);
    for (int m = 0; m < 10; ++m) {
        const float start = r.range(0.0f, 0.5f), hz = r.range(26.0f, 40.0f);
        wingbeats(b, start, 1.3f, hz, r.range(0.4f, 1.0f), r, [=](float t) { return dsp::smooth01((t - start) / 0.25f) * (1.0f - dsp::smooth01((t - 0.9f) / 0.4f)); });
    }
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound mothShiver(int rate) {
    Rand r(8050u);
    Buf b(1.7f, rate);
    for (int m = 0; m < 5; ++m)
        wingbeats(b, 0.0f, 1.6f, r.range(48.0f, 62.0f), r.range(0.2f, 0.5f), r, [](float t) { return dsp::smooth01(t / 1.3f) * (1.0f - dsp::smooth01((t - 1.45f) / 0.15f)); });
    normalize(b, level::kCreature - 8.0f);
    return b.sound();
}

Sound mothSting(int rate) {
    Rand r(8100u);
    Buf b(0.6f, rate);
    wingbeats(b, 0.0f, 0.4f, 36.0f, 1.0f, r, [](float t) { return envAHR(t, 0.05f, 0.2f, 0.15f); });
    ImpactSpec sp;
    sp.modes = modesCeramic(4200.0f, 0.006f, r);
    sp.contact = 0.0002f, sp.noise = 0.5f, sp.noiseHz = 7000.0f;
    b.add(impact(sp, 0.8f, 0.03f, rate, r), 0.22f);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

// --- Gestalt, Tiefe -----------------------------------------------------------------------------------
Sound gestaltSight(int rate) {
    Rand r(8200u);
    Buf b(3.0f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 90.0f, 0.7f, (float)rate);
    float ph = 0;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        ph += kTwoPi * 6800.0f / (float)rate;
        b.x[i] = lp.run(r.bi()) * 1.5f * envAHR(t, 0.6f, 0.6f, 1.6f) + std::sin(ph) * 0.006f * envAHR(t, 1.0f, 0.4f, 1.4f);
    }
    normalize(b, level::kCreature - 14.0f);
    return b.sound();
}

Sound deepRumble(int rate) {
    Rand r(8300u);
    const float L = 23.0f;
    Buf b(L + 1.0f, 24000);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 48.0f, 0.8f, 24000.0f), lp2 = Biquad::make(Biquad::LP, 320.0f, 0.7f, 24000.0f);
    float ph = 0;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        ph += kTwoPi * (24.0f + 4.0f * std::sin(t * 0.17f)) / 24000.0f;
        b.x[i] = lp.run(br.run(r.bi())) * 2.5f + std::sin(ph) * 0.25f + lp2.run(r.bi()) * 0.2f * (0.5f + 0.5f * std::sin(t * 0.4f));
    }
    loopify(b, 1.0f);
    normalize(b, level::kCreature - 4.0f);
    return b.sound(true);
}

Sound deepSurge(int rate) {
    Rand r(8400u);
    Buf b(6.5f, 24000);
    dsp::Svf f;
    dsp::Brown br;
    Biquad sub = Biquad::make(Biquad::LP, 60.0f, 0.8f, 24000.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i), u = t / 6.2f;
        f.set(90.0f + 700.0f * u * u, 0.7f, 24000.0f);
        b.x[i] = f.lp(r.bi()) * dsp::smooth01(u) * 1.2f + sub.run(br.run(r.bi())) * 3.0f * dsp::smooth01(u * 1.5f);
    }
    grains(b, 2.0f, 6.2f, 60.0f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(4.0f, 12.0f), rr.range(0.05f, 0.25f), rr); }, r,
           [](float t) { return (t - 2.0f) / 4.2f; });
    normalize(b, level::kCreature);
    return b.sound();
}

Sound deepStrike(int rate) {
    Rand r(8500u);
    Buf b(2.0f, 24000);
    Biquad bp = Biquad::make(Biquad::BP, 700.0f, 0.5f, 24000.0f), lp = Biquad::make(Biquad::LP, 90.0f, 0.8f, 24000.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = bp.run(r.bi()) * envAD(t, 0.01f, 0.8f) + lp.run(r.bi()) * 4.0f * envAD(t, 0.02f, 1.2f);
    }
    grains(b, 0.05f, 1.2f, 200.0f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(1.0f, 6.0f), rr.range(0.05f, 0.3f), rr); }, r,
           [](float t) { return 1.0f - t / 1.2f; });
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}

// --- Halluzinationen ----------------------------------------------------------------------------------
// Fluestern ohne Woerter: Silben aus Vokal-Formanten (stimmlos) und Zischlauten
Sound whisper(int rate, int v) {
    Rand r(8600u + (unsigned)v);
    const float len = r.range(1.0f, 2.2f);
    Buf b(len + 0.2f, rate);
    static const float vowels[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
    float t = 0.05f;
    while (t < len) {
        const float syl = r.range(0.12f, 0.22f);
        const int vw = r.irange(0, 4);
        if (r.chance(0.45f)) {  // Zischlaut davor
            Biquad hp = Biquad::make(Biquad::HP, r.range(3500.0f, 6000.0f), 0.8f, (float)rate);
            const float sl = r.range(0.04f, 0.09f);
            for (size_t i = 0; i < b.at(sl) && b.at(t) + i < b.n(); ++i)
                b.x[b.at(t) + i] += hp.run(r.bi()) * 0.35f * envAHR(b.t(i), sl * 0.3f, 0.0f, sl * 0.7f);
            t += sl * 0.8f;
        }
        dsp::Svf f1, f2, f3;
        f1.set(vowels[vw][0], 6.0f, (float)rate), f2.set(vowels[vw][1], 8.0f, (float)rate), f3.set(vowels[vw][2], 10.0f, (float)rate);
        for (size_t i = 0; i < b.at(syl) && b.at(t) + i < b.n(); ++i) {
            const float w = r.bi();
            b.x[b.at(t) + i] += (f1.bp(w) + f2.bp(w) * 0.8f + f3.bp(w) * 0.5f) * envAHR(b.t(i), syl * 0.25f, syl * 0.3f, syl * 0.45f);
        }
        t += syl + (r.chance(0.2f) ? r.range(0.1f, 0.3f) : 0.0f);
    }
    trim(b);
    normalize(b, level::kCreature - 10.0f);
    return b.sound();
}

Sound scratchWall(int rate, int v) {
    Rand r(8700u + (unsigned)v);
    Buf b(2.2f, rate);
    float at = 0.0f;
    const int strokes = r.irange(2, 4);
    for (int k = 0; k < strokes; ++k) {
        const float len = r.range(0.3f, 0.7f);
        Biquad bp = Biquad::make(Biquad::BP, r.range(1500.0f, 3500.0f), 1.5f, (float)rate);
        const float fr = r.range(35.0f, 65.0f);
        for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
            const float t = b.t(i);
            const float stick = 0.5f + 0.5f * std::sin(kTwoPi * fr * t + r.bi() * 0.5f);  // Haften und Gleiten
            b.x[b.at(at) + i] += bp.run(r.bi()) * stick * envAHR(t, 0.05f, len * 0.6f, len * 0.3f);
        }
        at += len + r.range(0.1f, 0.4f);
    }
    filter(b, Biquad::LP, 1800.0f, 0.7f);  // hinter der Wand
    trim(b);
    normalize(b, level::kCreature - 10.0f);
    return b.sound();
}

Sound halBreath(int rate, int v) {
    Rand r(8800u + (unsigned)v);
    Buf b(3.2f, rate);
    breathe(b, 0.0f, 1.2f, true, 0.6f, 0.3f, r);
    breathe(b, 1.3f, 1.6f, false, 0.6f, 0.2f, r);
    trim(b);
    normalize(b, level::kCreature - 10.0f);
    return b.sound();
}

// Spieluhr (Level 19): eigene, einfache Melodie auf Stahlzungen, die Feder laesst nach
Sound musicBox(int rate) {
    Rand r(8900u);
    Buf b(12.0f, rate);
    // Tonfolge in Halbtoenen ueber A5 (eigene Melodie, langsam fallend)
    static const int notes[] = {7, 3, 5, 0, 2, -2, 0, 3, 2, -4, -2, 0, -5};
    float at = 0.2f, gap = 0.62f;
    for (int n : notes) {
        const float f = 880.0f * std::pow(2.0f, (float)n / 12.0f) * r.range(0.997f, 1.003f);
        ImpactSpec tine;
        tine.modes = {{f, 2.2f, 1.0f}, {f * 6.27f, 0.25f, 0.12f}, {f * 2.0f * 1.002f, 0.6f, 0.08f}};
        tine.contact = 0.0006f, tine.noise = 0.05f, tine.noiseHz = 6000.0f;
        b.add(impact(tine, r.range(0.6f, 0.9f), 2.5f, rate, r), at);
        at += gap;
        gap *= 1.06f;  // die Feder laesst nach
        if (at > 9.5f) break;
    }
    trim(b);
    normalize(b, level::kCreature - 12.0f);
    return b.sound();
}

}  // namespace

// --- V13.2: neue Wesen ----------------------------------------------------------------------------------------------
//   Faceling     ohne Mund: gedaempfte Vokale hinter geschlossenen Lippen (die Seite: vokalartige, undeutliche Laute);
//                aufgebracht schneller, hoeher, stossweise
//   Skin-Stealer spricht nach, ohne zu verstehen: Silben mit Mitlauten, flache, zu gleichmaessige Betonung, Wiederholungen
//   Wretch       traege: tiefes, nasses Gurgeln; schnell: hohes, rauhes Fauchen (die Seite: der Laut verraet die Gefahr);
//                zweites Stadium: heiseres Flehen in zwei, drei Silben
//   Ratte        Quieken (2,5-6 kHz); das Schrei-Ereignis: viele Stimmen, kreischend, durcheinander
//   Fenster      warmes, langsames Fluestern (trostend), der Arm faehrt mit einem Sog heraus
namespace {
struct Vowel {
    float f1, f2, f3;
};
constexpr Vowel kVowels[] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}, {500, 1500, 2500}};

// eine stimmhafte Silbe: Grundton f0a -> f0b, Vokal v, Rauheit
void syllable(Buf& b, float at, float len, float f0a, float f0b, const Vowel& v, float breath, float rough, float subharm, Rand& r) {
    VoiceSpec vs;
    vs.f0 = [=](float u) { return f0a + (f0b - f0a) * u; };
    vs.amp = [=](float u) { return dsp::smooth01(u / 0.18f) * (1.0f - dsp::smooth01((u - 0.7f) / 0.3f)); };
    vs.formants = {v.f1 * r.range(0.95f, 1.05f), v.f2 * r.range(0.95f, 1.05f), v.f3, 3400.0f};
    vs.bandwidths = {90.0f, 120.0f, 180.0f, 260.0f};
    vs.jitter = 0.03f, vs.shimmer = 0.15f, vs.breath = breath, vs.rough = rough, vs.subharm = subharm;
    voice(b, at, len, vs, r);
}
// Mitlaut vor der Silbe: Verschluss (p t k) oder Reibelaut (s f h)
void consonant(Buf& b, float at, Rand& r, float amp) {
    const int k = r.irange(0, 4);
    const float hz = k == 0 ? 900.0f : k == 1 ? 3500.0f : k == 2 ? 1800.0f : k == 3 ? 6200.0f : 1200.0f;
    const float len = k <= 2 ? 0.025f : r.range(0.05f, 0.09f);
    Biquad bp = Biquad::make(Biquad::BP, hz, k <= 2 ? 1.2f : 0.9f, (float)b.rate);
    for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i)
        b.x[b.at(at) + i] += bp.run(r.bi()) * amp * (k <= 2 ? envAD(b.t(i), 0.002f, 0.02f) : envAHR(b.t(i), len * 0.3f, len * 0.3f, len * 0.4f));
}
}  // namespace

Sound facelingHum(int rate, int v) {
    Rand r(9100u + (unsigned)v);
    Buf b(1.6f, rate);
    float at = 0.05f;
    const int n = r.irange(1, 3);
    const float f0 = r.range(115.0f, 190.0f);
    for (int k = 0; k < n; ++k) {
        const float len = r.range(0.35f, 0.7f);
        syllable(b, at, len, f0 * r.range(0.95f, 1.08f), f0 * r.range(0.85f, 1.05f), kVowels[r.irange(0, 5)], 0.25f, 0.0f, 0.0f, r);
        at += len + r.range(0.05f, 0.2f);
    }
    filter(b, Biquad::LP, 750.0f, 0.7f);  // hinter geschlossener Haut: nur das Summen dringt durch
    filter(b, Biquad::LP, 1100.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound facelingOutburst(int rate, int v) {
    Rand r(9200u + (unsigned)v);
    Buf b(1.4f, rate);
    float at = 0.02f;
    const float f0 = r.range(230.0f, 300.0f);
    for (int k = 0; k < 4; ++k) {
        const float len = r.range(0.14f, 0.26f);
        syllable(b, at, len, f0 * r.range(1.0f, 1.2f), f0 * r.range(0.8f, 1.0f), kVowels[r.irange(0, 3)], 0.4f, 0.5f, 0.15f, r);
        at += len + r.range(0.03f, 0.08f);
    }
    filter(b, Biquad::LP, 1300.0f, 0.8f);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound stoneSwing(int rate) {  // Ausholen und Durchziehen: ein Luftzug
    Rand r(9300u);
    Buf b(0.5f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        f.set(500.0f + 1800.0f * dsp::smooth01(t / 0.3f), 2.0f, (float)rate);
        b.x[i] = f.bp(r.bi()) * envAHR(t, 0.18f, 0.05f, 0.2f);
    }
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound hitBody(int rate, int v) {  // ein Schlag trifft (dumpf), dazu ein Stoss Atem
    Rand r(9400u + (unsigned)v);
    Buf b(0.6f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 180.0f + 60.0f * (float)v, 0.9f, (float)rate);
    for (size_t i = 0; i < b.at(0.15f); ++i) b.x[i] += lp.run(r.bi()) * envAD(b.t(i), 0.003f, 0.08f) * 3.0f;
    ImpactSpec sp;
    sp.modes = modesWood(r.range(140.0f, 220.0f), 0.05f, r);
    sp.contact = 0.004f, sp.noise = 0.5f, sp.noiseHz = 900.0f;
    b.add(impact(sp, 0.8f, 0.15f, rate, r), 0.0f);
    breathe(b, 0.02f, 0.25f, false, 0.5f, 1.0f, r);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound skinMimic(int rate, int v) {
    Rand r(9500u + (unsigned)v);
    Buf b(2.4f, rate);
    // ein "Wort" aus 2-3 Silben, zwei- oder dreimal, mit derselben flachen Melodie - nie ganz gleich, nie ganz richtig
    struct Syl {
        int vow;
        bool cons;
        float len;
    };
    std::vector<Syl> word;
    const int ns = r.irange(2, 3);
    for (int k = 0; k < ns; ++k) word.push_back({r.irange(0, 5), r.chance(0.7f), r.range(0.13f, 0.22f)});
    const bool ask = (v % 3) == 0;  // fragend (steigt am Ende)
    const float f0 = r.range(102.0f, 125.0f);
    float at = 0.05f;
    const int reps = r.irange(1, 3);
    for (int rep = 0; rep < reps; ++rep) {
        for (size_t k = 0; k < word.size(); ++k) {
            const Syl& s = word[k];
            if (s.cons) consonant(b, at, r, 0.5f), at += 0.035f;
            const bool last = k + 1 == word.size();
            const float a0 = f0 * (1.0f + 0.01f * (float)rep), a1 = ask && last ? f0 * 1.35f : f0 * 0.97f;
            syllable(b, at, s.len * r.range(0.95f, 1.15f), a0, a1, kVowels[s.vow], 0.2f, 0.05f, 0.0f, r);
            at += s.len + 0.03f;
        }
        at += r.range(0.35f, 0.6f);
    }
    filter(b, Biquad::HP, 90.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

Sound wretchGurgle(int rate, int v) {  // traege: tief, nass, langsam
    Rand r(9600u + (unsigned)v);
    const float len = r.range(1.2f, 2.0f);
    Buf b(len + 0.3f, rate);
    VoiceSpec vs;
    const float base = r.range(62.0f, 84.0f), ph = r.range(0.0f, 6.0f);
    vs.f0 = [=](float u) { return base * (1.0f + 0.15f * std::sin(u * 5.0f + ph)); };
    vs.amp = [=](float u) { return dsp::smooth01(u / 0.2f) * (1.0f - dsp::smooth01((u - 0.75f) / 0.25f)) * (0.6f + 0.4f * std::sin(u * 17.0f + ph)); };
    vs.formants = {r.range(320.0f, 420.0f), r.range(700.0f, 900.0f), 2300.0f, 3200.0f};
    vs.bandwidths = {140.0f, 180.0f, 240.0f, 320.0f};
    vs.jitter = 0.08f, vs.shimmer = 0.4f, vs.subharm = 0.8f, vs.breath = 0.35f, vs.rough = 0.7f, vs.roughHz = r.range(18.0f, 26.0f);
    vs.tiltHz = 1600.0f;
    voice(b, 0.0f, len, vs, r);
    wetClicks(b, 0.0f, len, 14.0f, 0.12f, r);  // Schlamm im Rachen
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound wretchSnarl(int rate, int v) {  // schnell: hoch, rauh, fauchend
    Rand r(9700u + (unsigned)v);
    const float len = r.range(0.6f, 1.0f);
    Buf b(len + 0.2f, rate);
    VoiceSpec vs;
    const float base = r.range(320.0f, 460.0f);
    vs.f0 = [=](float u) { return base * (1.1f - 0.25f * u); };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.08f) * std::pow(std::max(0.0f, 1.0f - u), 0.8f); };
    vs.formants = {r.range(700.0f, 850.0f), r.range(1300.0f, 1600.0f), 2900.0f, 3800.0f};
    vs.bandwidths = {160.0f, 200.0f, 280.0f, 360.0f};
    vs.jitter = 0.1f, vs.shimmer = 0.35f, vs.subharm = 0.3f, vs.breath = 0.9f, vs.rough = 0.5f, vs.roughHz = 34.0f;
    vs.tiltHz = 4200.0f;
    voice(b, 0.0f, len, vs, r);
    breathe(b, 0.0f, len * 0.8f, false, 0.6f, 1.0f, r);
    wetClicks(b, 0.0f, len, 8.0f, 0.06f, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound wretchPlead(int rate, int v) {  // heiser, gebrochen: zwei, drei Silben, fallend
    Rand r(9800u + (unsigned)v);
    Buf b(2.0f, rate);
    const float f0 = r.range(140.0f, 175.0f);
    float at = 0.05f;
    const int n = r.irange(2, 3);
    for (int k = 0; k < n; ++k) {
        if (r.chance(0.6f)) consonant(b, at, r, 0.35f), at += 0.04f;
        const float len = r.range(0.22f, 0.4f);
        syllable(b, at, len, f0 * (1.05f - 0.08f * (float)k), f0 * (0.92f - 0.08f * (float)k), kVowels[r.irange(0, 5)], 0.7f, 0.35f, 0.1f, r);
        at += len + r.range(0.06f, 0.16f);
    }
    breathe(b, at, 0.4f, true, 0.4f, 0.8f, r);  // rasselnd einatmen
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

Sound ratSqueak(int rate, int v) {
    Rand r(9900u + (unsigned)v);
    Buf b(0.5f, rate);
    float at = 0.0f;
    const int n = r.irange(1, 3);
    for (int k = 0; k < n; ++k) {
        const float len = r.range(0.04f, 0.11f), f0 = r.range(2600.0f, 4200.0f), f1 = f0 * r.range(1.1f, 1.5f);
        float ph = 0.0f;
        for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
            const float u = b.t(i) / len;
            ph += kTwoPi * (f0 + (f1 - f0) * std::sin(kPi * u)) / (float)rate;
            b.x[b.at(at) + i] += (std::sin(ph) + 0.3f * std::sin(2.0f * ph) + 0.15f * r.bi()) * envAHR(b.t(i), len * 0.15f, len * 0.5f, len * 0.35f);
        }
        at += len + r.range(0.02f, 0.08f);
    }
    trim(b);
    normalize(b, level::kCreature - 8.0f);
    return b.sound();
}

Sound ratScream(int rate) {  // das Schrei-Ereignis: viele Ratten kreischen durcheinander (gedaempft, hinter Waenden)
    Rand r(9950u);
    const float L = 4.2f;
    Buf b(L + 0.4f, rate);
    for (int k = 0; k < 90; ++k) {
        const float at = r.range(0.0f, L - 0.3f), len = r.range(0.08f, 0.3f);
        const float f0 = r.range(1800.0f, 3800.0f), f1 = f0 * r.range(0.8f, 1.6f);
        float ph = 0.0f;
        const float amp = r.range(0.2f, 0.7f) * (1.0f - 0.5f * dsp::smooth01((at - L * 0.6f) / (L * 0.4f)));
        for (size_t i = 0; i < b.at(len) && b.at(at) + i < b.n(); ++i) {
            const float u = b.t(i) / len;
            ph += kTwoPi * (f0 + (f1 - f0) * u + 120.0f * std::sin(u * 40.0f)) / (float)rate;
            b.x[b.at(at) + i] += (std::sin(ph) + 0.4f * std::sin(3.0f * ph) + 0.3f * r.bi()) * amp * envAHR(b.t(i), len * 0.1f, len * 0.5f, len * 0.4f);
        }
    }
    // Rangeln, Krallen
    grains(b, 0.0f, L, 30.0f, [](Buf& bb, float t, Rand& rr) {
        Biquad bp = Biquad::make(Biquad::BP, rr.range(2000.0f, 5000.0f), 3.0f, (float)bb.rate);
        const size_t a = bb.at(t);
        for (size_t i = 0; i < 200 && a + i < bb.n(); ++i) bb.x[a + i] += bp.run(i < 6 ? rr.bi() * 1.5f : 0.0f) * 0.3f;
    }, r);
    filter(b, Biquad::LP, 3200.0f, 0.7f);  // nicht im selben Raum
    trim(b);
    normalize(b, level::kDistant + 4.0f);
    return b.sound();
}

Sound windowWhisper(int rate, int v) {  // warm, langsam, nah am Ohr - trostend (die Seite)
    Rand r(9970u + (unsigned)v);
    const float len = r.range(1.6f, 2.6f);
    Buf b(len + 0.4f, rate);
    float t = 0.1f;
    while (t < len) {
        const float syl = r.range(0.2f, 0.34f);
        const Vowel& vw = kVowels[r.irange(0, 5)];
        if (r.chance(0.35f)) {
            Biquad hp = Biquad::make(Biquad::HP, r.range(2500.0f, 4000.0f), 0.7f, (float)rate);
            const float sl = r.range(0.05f, 0.1f);
            for (size_t i = 0; i < b.at(sl) && b.at(t) + i < b.n(); ++i)
                b.x[b.at(t) + i] += hp.run(r.bi()) * 0.2f * envAHR(b.t(i), sl * 0.4f, 0.0f, sl * 0.6f);
            t += sl * 0.7f;
        }
        dsp::Svf f1, f2;
        f1.set(vw.f1 * 0.9f, 5.0f, (float)rate), f2.set(vw.f2 * 0.85f, 6.0f, (float)rate);
        for (size_t i = 0; i < b.at(syl) && b.at(t) + i < b.n(); ++i) {
            const float w = r.bi();
            b.x[b.at(t) + i] += (f1.bp(w) + f2.bp(w) * 0.6f) * envAHR(b.t(i), syl * 0.35f, syl * 0.25f, syl * 0.4f);
        }
        t += syl + (r.chance(0.3f) ? r.range(0.15f, 0.4f) : 0.02f);
    }
    filter(b, Biquad::LP, 2600.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 9.0f);
    return b.sound();
}

Sound windowPull(int rate) {  // der Arm faehrt heraus: ein Sog, dann ein Zug wie durch Stoff
    Rand r(9990u);
    Buf b(0.9f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        f.set(200.0f + 900.0f * dsp::smooth01(t / 0.5f), 1.5f, (float)rate);
        b.x[i] = f.bp(r.bi()) * envAHR(t, 0.25f, 0.15f, 0.4f) * 0.8f;
    }
    Biquad lp = Biquad::make(Biquad::LP, 90.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] += lp.run(r.bi()) * 1.5f * envAHR(b.t(i), 0.3f, 0.1f, 0.4f);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}

// --- Schreck (V11): der toedliche Zugriff -----------------------------------------------------------
// Aufbau beider Schrecken: Druckstoss (Unterbass mit fallendem Ton) und Rauschschlag im selben Moment,
// darueber der Schrei der Kreatur, angeraut und gesaettigt; danach Ohrenklingen (hoher Sinus) und ein
// dumpfes Nachgrollen. Der lauteste Klang des Spiels (-15 LUFS), Spitze begrenzt.
namespace {
void scareHit(Buf& b, Rand& r, float boom) {
    // Druckstoss: 95 -> 32 Hz in 0,25 s, Abklingen 0,5 s
    float ph = 0.0f;
    for (size_t i = 0; i < b.at(0.9f) && i < b.n(); ++i) {
        const float t = b.t(i);
        const float f = 32.0f + 63.0f * std::exp(-t * 11.0f);
        ph += kTwoPi * f / (float)b.rate;
        b.x[i] += std::sin(ph) * boom * envAD(t, 0.003f, 0.5f);
    }
    // Rauschschlag (breitbandig, 40 ms)
    Biquad hp = Biquad::make(Biquad::HP, 180.0f, 0.7f, (float)b.rate);
    for (size_t i = 0; i < b.at(0.06f); ++i) b.x[i] += hp.run(r.bi()) * 0.9f * envAD(b.t(i), 0.0008f, 0.035f);
}

void tinnitus(Buf& b, float at, float len, float hz, float amp) {
    float ph = 0.0f;
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        const float t = b.t(i) - at, u = t / len;
        ph += kTwoPi * hz / (float)b.rate;
        b.x[i] += std::sin(ph) * amp * dsp::smooth01(t / 0.08f) * (1.0f - u) * (1.0f - u);
    }
}

void saturate(Buf& b, float drive) {
    for (float& v : b.x) v = std::tanh(v * drive) / std::tanh(drive);
}
}  // namespace

// Das Laecheln: ein Schrei aus zu vielen Zaehnen - zwei verstimmte, steigende Stimmen mit harten,
// hohen Formanten, darin der glasige Ton der Ankuendigung, jetzt schrill und laut
Sound scareSmiler(int rate) {
    Rand r(6900u);
    Buf b(2.6f, rate);
    Buf scream(1.1f, rate);
    for (float det : {1.0f, 1.07f}) {
        VoiceSpec vs;
        vs.f0 = [det](float u) { return (420.0f + 520.0f * dsp::smooth01(u * 2.5f)) * det * (1.0f - 0.25f * u); };
        vs.amp = [](float u) { return dsp::smooth01(u / 0.04f) * (1.0f - u * u); };
        vs.formants = {950.0f, 2700.0f, 4300.0f}, vs.bandwidths = {160.0f, 260.0f, 420.0f};
        vs.subharm = 0.45f, vs.rough = 0.9f, vs.roughHz = 37.0f, vs.breath = 0.85f, vs.jitter = 0.06f, vs.shimmer = 0.25f;
        vs.tiltHz = 7500.0f;
        voice(scream, 0.0f, 1.0f, vs, r);
    }
    float p1 = 0, p2 = 0;
    for (size_t i = 0; i < scream.at(0.9f); ++i) {
        const float t = scream.t(i), u = t / 0.9f;
        const float f = 2100.0f * std::pow(1.6f, u);
        p1 += kTwoPi * f / (float)rate;
        p2 += kTwoPi * f * 1.031f / (float)rate;
        scream.x[i] += (std::sin(p1) + 0.8f * std::sin(p2)) * 0.12f * dsp::smooth01(t / 0.03f) * (1.0f - u);
    }
    normalize(scream, -16.0f, 0.9f);
    saturate(scream, 2.2f);
    b.add(scream, 0.0f, 0.9f);
    scareHit(b, r, 0.9f);
    for (int k = 0; k < 6; ++k) enamelClick(b, r.range(0.02f, 0.5f), 0.5f, r);
    tinnitus(b, 0.75f, 1.8f, 7300.0f, 0.035f);
    Biquad lp = Biquad::make(Biquad::LP, 90.0f, 0.7f, (float)rate);
    for (size_t i = b.at(0.6f); i < b.n(); ++i) b.x[i] += lp.run(r.bi()) * 0.6f * envAD(b.t(i) - 0.6f, 0.2f, 1.2f);
    trim(b);
    normalize(b, -15.0f, 0.9f);
    return b.sound();
}

// Der Hund: Bruellen aus einem menschlichen Rachen (Unterharmonische, sehr rau), das Zuschnappen
// des Kiefers mitten im Schrei, Krallen
Sound scareHound(int rate) {
    Rand r(7900u);
    Buf b(2.6f, rate);
    Buf roar(1.1f, rate);
    VoiceSpec vs;
    vs.f0 = [](float u) { return 210.0f - 90.0f * u; };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.03f) * (1.0f - u); };
    vs.formants = {620.0f, 1150.0f, 2600.0f}, vs.bandwidths = {140.0f, 200.0f, 320.0f};
    vs.subharm = 0.95f, vs.rough = 1.0f, vs.roughHz = 31.0f, vs.breath = 0.8f, vs.jitter = 0.08f, vs.shimmer = 0.3f;
    vs.tiltHz = 5200.0f;
    voice(roar, 0.0f, 0.95f, vs, r);
    normalize(roar, -16.0f, 0.9f);
    saturate(roar, 2.6f);
    b.add(roar, 0.0f, 0.95f);
    scareHit(b, r, 1.0f);
    ImpactSpec jaw;
    jaw.modes = modesCeramic(r.range(1400.0f, 1900.0f), 0.04f, r);
    jaw.contact = 0.0004f, jaw.noise = 0.9f, jaw.noiseHz = 3200.0f;
    b.add(impact(jaw, 1.0f, 0.2f, rate, r), 0.22f, 1.2f);
    dsp::Svf f;
    for (size_t i = b.at(0.05f); i < b.at(0.35f); ++i) {  // Krallen schaben
        f.set(2400.0f + 900.0f * std::sin(b.t(i) * 40.0f), 2.0f, (float)rate);
        b.x[i] += f.bp(r.bi()) * 0.25f * envAD(b.t(i) - 0.05f, 0.01f, 0.25f);
    }
    tinnitus(b, 0.7f, 1.8f, 6800.0f, 0.035f);
    Biquad lp = Biquad::make(Biquad::LP, 80.0f, 0.7f, (float)rate);
    for (size_t i = b.at(0.6f); i < b.n(); ++i) b.x[i] += lp.run(r.bi()) * 0.6f * envAD(b.t(i) - 0.6f, 0.2f, 1.2f);
    trim(b);
    normalize(b, -15.0f, 0.9f);
    return b.sound();
}

void addCreatureSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"smiler_presence", [=] { return smilerPresence(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("smiler_teeth_{}", v), [=] { return smilerTeeth(rate, v); }});
    d.push_back({"smiler_open", [=] { return smilerOpen(rate); }});
    d.push_back({"smiler_attack", [=] { return smilerAttack(rate); }});
    d.push_back({"smiler_fade", [=] { return smilerFade(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("hound_growl_{}", v), [=] { return houndGrowl(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("hound_bark_{}", v), [=] { return houndBark(rate, v); }});
    d.push_back({"hound_whine", [=] { return houndWhine(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("hound_pant_{}", v), [=] { return houndPant(rate, v); }});
    for (int v = 0; v < 6; ++v) d.push_back({std::format("hound_step_{}", v), [=] { return houndStep(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("hound_sniff_{}", v), [=] { return houndSniff(rate, v); }});
    d.push_back({"hound_snap", [=] { return houndSnap(rate); }});
    d.push_back({"hound_charge", [=] { return houndCharge(rate); }});
    d.push_back({"scare_smiler", [=] { return scareSmiler(rate); }});  // V11: toedlicher Zugriff
    d.push_back({"scare_hound", [=] { return scareHound(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("clump_knock_{}", v), [=] { return clumpKnock(rate, v); }});
    for (int v = 0; v < 5; ++v) d.push_back({std::format("clump_slap_{}", v), [=] { return clumpSlap(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("clump_drag_{}", v), [=] { return clumpDrag(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("clump_crack_{}", v), [=] { return clumpCrack(rate, v); }});
    d.push_back({"clump_reach", [=] { return clumpReach(rate); }});
    d.push_back({"clump_grab", [=] { return clumpGrab(rate); }});
    d.push_back({"moth_flutter", [=] { return mothFlutter(rate); }});
    d.push_back({"moth_takeoff", [=] { return mothTakeoff(rate); }});
    d.push_back({"moth_shiver", [=] { return mothShiver(rate); }});
    d.push_back({"moth_sting", [=] { return mothSting(rate); }});
    d.push_back({"gestalt_sight", [=] { return gestaltSight(rate); }});
    d.push_back({"deep_rumble", [=] { return deepRumble(rate); }});
    d.push_back({"deep_surge", [=] { return deepSurge(rate); }});
    d.push_back({"deep_strike", [=] { return deepStrike(rate); }});
    for (int v = 0; v < 6; ++v) d.push_back({std::format("hal_whisper_{}", v), [=] { return whisper(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("hal_scratch_{}", v), [=] { return scratchWall(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("hal_breath_{}", v), [=] { return halBreath(rate, v); }});
    d.push_back({"music_box", [=] { return musicBox(rate); }});
    // V13.2: neue Wesen
    for (int v = 0; v < 4; ++v) d.push_back({std::format("faceling_hum_{}", v), [=] { return facelingHum(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("faceling_outburst_{}", v), [=] { return facelingOutburst(rate, v); }});
    d.push_back({"stone_swing", [=] { return stoneSwing(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("hit_body_{}", v), [=] { return hitBody(rate, v); }});
    for (int v = 0; v < 6; ++v) d.push_back({std::format("skin_mimic_{}", v), [=] { return skinMimic(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wretch_gurgle_{}", v), [=] { return wretchGurgle(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wretch_snarl_{}", v), [=] { return wretchSnarl(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wretch_plead_{}", v), [=] { return wretchPlead(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("rat_squeak_{}", v), [=] { return ratSqueak(rate, v); }});
    d.push_back({"rat_scream", [=] { return ratScream(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("window_whisper_{}", v), [=] { return windowWhisper(rate, v); }});
    d.push_back({"window_pull", [=] { return windowPull(rate); }});
}

}  // namespace lim::audio
