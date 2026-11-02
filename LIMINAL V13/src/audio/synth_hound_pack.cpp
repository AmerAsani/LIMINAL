// V13.3 Level 2/3 (KREATUR_Hound.md, Abschnitt "Rudel"; KREATUR_Clump.md, Abschnitt "Level 3"): Klaenge des Hound-Rudels und
// des grossen, bewaffneten Buendels. Alles synthetisch, keine Aufnahmen.
//
// Hound-Rudel (Stimme wie hound_bark: menschlich-heiser durch einen Hundevokaltrakt, tiefer und rauer im Rudel)
//   hp_call    der Ruf: drei tiefe, heisere Belllaute mit Hall im Gang          hp_answer  zwei Belllaute aus der Ferne
//   hp_fast    schnelles, aufgeregtes Bellen (sechs, immer kuerzer, hoeher, dazwischen Hecheln) - das Opfer laeuft in die Falle
//   hp_yip     Klaeffen, Zaehneklappern (Laute im Rudel)    hp_lunge   scharfes Klaeffen, das in ein Knurren hochzieht
//   hp_maul    Biss: Knirschen, Knurren durch die Zaehne mit Schuetteln (9 Hz), nasses Reissen
//   hp_flinch  eingeschuechtert: kurzes Winseln mit Knurren    hp_rush    viele Pfoten und Krallen, die heranstuermen
//   hp_lurk    langsamer, feuchter Atem (nur ganz nah)    hp_rasp    das eigene Roecheln mit zerbissener Kehle (Blasen, Gurgeln)
// Grosses Buendel
//   cl3_ceiling  Handflaechen an Beton ueber dir, Staub rieselt    cl3_bones  Knochen klappern an Knochen, das Rohr tickt
//   cl3_breath   Atem durch die Zaehne: zischend ein, tief grollend aus    cl3_drop  schwerer Aufschlag, Glieder klatschen nach
//   cl3_scrape   Knochen schabt ueber Ziegel (Haft-Gleiten, hell)    cl3_swing  schweres Sausen    cl3_hit  dumpfer Treffer, Knirschen
//   cl3_miss     Schlag auf Beton: Rohr klingt, Splitter    cl3_snap  das Rohr biegt sich, bricht, das Stueck rollt weg
//   cl3_tear     Fleisch reisst, Knochen bricht, es blutet    cl3_step  schwere Handflaeche auf Beton
#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {
void band(Buf& b, float at, float len, float hz, float q, float amp, Rand& r, const std::function<float(float)>& env) {
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) b.x[a + k] += bp.run(r.bi()) * env((float)k / (float)n) * amp;
}
void wet(Buf& b, float t0, float t1, float perSec, float amp, Rand& r) {
    grains(b, t0, t1, perSec, [&](Buf& g, float at, Rand& rr) { bubble(g, at, rr.range(0.6f, 2.2f), amp * rr.range(0.4f, 1.0f), rr); }, r);
}
// ein Belllaut: f0 Grundton, len Dauer, rough Rauheit
void barkAt(Buf& b, float at, float f0, float len, float rough, Rand& r) {
    VoiceSpec vs;
    vs.f0 = [=](float u) { return f0 * (1.0f - 0.45f * dsp::smooth01(u / 0.5f)); };
    vs.amp = [](float u) { return (u < 0.05f ? u / 0.05f : 1.0f) * std::pow(std::max(0.0f, 1.0f - u), 1.4f); };
    vs.formants = {r.range(560.0f, 700.0f), r.range(1050.0f, 1250.0f), 2500.0f, 3400.0f};
    vs.bandwidths = {150.0f, 190.0f, 260.0f, 330.0f};
    vs.jitter = 0.08f, vs.shimmer = 0.32f, vs.subharm = 0.5f + 0.3f * rough, vs.breath = 0.75f, vs.rough = 0.4f + 0.4f * rough;
    vs.tiltHz = 3300.0f;
    voice(b, at, len, vs, r);
    breathe(b, at, len * 0.3f, false, 0.5f, 1.0f, r);
}
void growlAt(Buf& b, float at, float len, float base, float amp0, float amp1, Rand& r, float shakeHz = 0.0f) {
    VoiceSpec vs;
    const float ph = r.range(0.0f, 6.0f);
    vs.f0 = [=](float u) { return base * (1.0f + 0.1f * std::sin(u * 9.0f + ph) + 0.25f * u); };
    vs.amp = [=](float u) {
        const float sh = shakeHz > 0.0f ? 0.6f + 0.4f * std::sin(u * len * kTwoPi * shakeHz) : 1.0f;
        return (amp0 + (amp1 - amp0) * u) * dsp::smooth01(u / 0.08f) * (1.0f - dsp::smooth01((u - 0.85f) / 0.15f)) * sh;
    };
    vs.formants = {r.range(380.0f, 460.0f), r.range(800.0f, 950.0f), 2300.0f, 3300.0f};
    vs.bandwidths = {120.0f, 170.0f, 230.0f, 300.0f};
    vs.jitter = 0.06f, vs.shimmer = 0.3f, vs.subharm = 0.8f, vs.breath = 0.5f, vs.rough = 0.65f, vs.roughHz = r.range(26.0f, 34.0f);
    vs.tiltHz = 2400.0f;
    voice(b, at, len, vs, r);
}
void thud(Buf& b, float at, float hz, float len, float amp) {
    const size_t a = b.at(at), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float t = (float)k / (float)b.rate;
        b.x[a + k] += std::sin(kTwoPi * hz * t * (1.0f - 0.3f * t / len)) * envAD(t, 0.003f, len) * amp;
    }
}
void crunch(Buf& b, float at, float amp, Rand& r) {  // Knochen/Knorpel bricht: dichte, harte Knackse
    grains(b, at, at + 0.12f, 90.0f, [&](Buf& g, float t, Rand& rr) {
        ImpactSpec s;
        s.modes = modesCeramic(rr.range(900.0f, 2600.0f), 0.02f, rr);
        s.contact = 0.0005f, s.noise = 0.7f, s.noiseHz = rr.range(1500.0f, 4000.0f);
        g.add(impact(s, 0.8f, 0.05f, g.rate, rr), t, amp * rr.range(0.4f, 1.0f));
    }, r);
}
void tearing(Buf& b, float at, float len, float amp, Rand& r) {  // nasses Reissen: Stoesse gefilterten Rauschens
    float t = at;
    while (t < at + len) {
        const float l = r.range(0.04f, 0.12f);
        band(b, t, l, r.range(700.0f, 2200.0f), 1.2f, amp * r.range(0.5f, 1.0f), r, [](float u) { return std::sin(3.14159f * u) * (0.6f + 0.4f * std::sin(u * 60.0f)); });
        t += l * r.range(0.6f, 1.1f);
    }
    wet(b, at, at + len, 30.0f, amp * 0.35f, r);
}
void slap(Buf& b, float at, float amp, float low, Rand& r) {  // Handflaeche auf Beton
    ImpactSpec s;
    s.modes = modesConcrete(r.range(140.0f, 220.0f) * low, r);
    s.contact = 0.004f, s.noise = 0.55f, s.noiseHz = 1400.0f * low, s.noiseQ = 0.6f;
    b.add(impact(s, 0.9f, 0.18f, b.rate, r), at, amp);
    thud(b, at, 70.0f * low, 0.12f, amp * 0.5f);
}
dsp::RoomSpec corridor() {
    dsp::RoomSpec rs;
    rs.t60Low = 1.5f, rs.t60Mid = 1.1f, rs.t60High = 0.5f, rs.size = 1.6f, rs.diffusion = 0.75f;
    return rs;
}

// --- Hound-Rudel ------------------------------------------------------------------------------------------------------------
Sound hpCall(int rate) {
    Rand r(7101u);
    Buf b(1.8f, rate);
    float at = 0.0f;
    for (int k = 0; k < 3; ++k) {
        barkAt(b, at, r.range(300.0f, 360.0f) * (k == 2 ? 0.92f : 1.0f), r.range(0.3f, 0.36f), 0.7f, r);
        at += r.range(0.4f, 0.48f);
    }
    reverb(b, corridor(), 0.35f, 1.0f, 71u);
    trim(b);
    normalize(b, level::kCreature + 2.5f);
    return b.sound();
}
Sound hpAnswer(int rate) {
    Rand r(7102u);
    Buf b(1.4f, rate);
    barkAt(b, 0.0f, r.range(320.0f, 380.0f), 0.32f, 0.6f, r);
    barkAt(b, r.range(0.42f, 0.5f), r.range(300.0f, 350.0f), 0.34f, 0.6f, r);
    distantize(b, 2200.0f, 1.6f, 72u);
    trim(b);
    normalize(b, level::kDistant + 1.0f);
    return b.sound();
}
Sound hpFast(int rate) {
    Rand r(7103u);
    Buf b(2.0f, rate);
    float at = 0.0f, gap = 0.26f;
    for (int k = 0; k < 6; ++k) {
        barkAt(b, at, r.range(360.0f, 420.0f) * (1.0f + 0.05f * (float)k), r.range(0.16f, 0.22f), 0.45f, r);
        breathe(b, at + 0.17f, 0.06f, true, 0.35f, 1.0f, r);
        at += gap;
        gap = std::max(0.15f, gap * 0.9f);
    }
    reverb(b, corridor(), 0.3f, 1.0f, 73u);
    trim(b);
    normalize(b, level::kCreature + 2.5f);
    return b.sound();
}
Sound hpYip(int rate) {
    Rand r(7104u);
    Buf b(0.9f, rate);
    VoiceSpec vs;
    vs.f0 = [](float u) { return 720.0f - 260.0f * u; };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.08f) * (1.0f - dsp::smooth01((u - 0.6f) / 0.4f)); };
    vs.formants = {700.0f, 1500.0f, 2900.0f}, vs.bandwidths = {120.0f, 180.0f, 260.0f};
    vs.jitter = 0.05f, vs.shimmer = 0.25f, vs.subharm = 0.3f, vs.breath = 0.6f, vs.tiltHz = 4200.0f;
    voice(b, 0.0f, 0.2f, vs, r);
    grains(b, 0.25f, 0.6f, 22.0f, [&](Buf& g, float t, Rand& rr) {  // Zaehneklappern
        ImpactSpec s;
        s.modes = modesCeramic(rr.range(2400.0f, 3600.0f), 0.015f, rr);
        s.contact = 0.0004f, s.noise = 0.4f;
        g.add(impact(s, 0.7f, 0.03f, g.rate, rr), t, rr.range(0.15f, 0.35f));
    }, r);
    growlAt(b, 0.3f, 0.5f, 85.0f, 0.4f, 0.2f, r);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}
Sound hpLunge(int rate) {
    Rand r(7105u);
    Buf b(1.0f, rate);
    barkAt(b, 0.0f, 470.0f, 0.15f, 0.3f, r);
    growlAt(b, 0.12f, 0.75f, 78.0f, 0.4f, 1.0f, r);
    breathe(b, 0.1f, 0.25f, true, 0.6f, 1.0f, r);
    trim(b);
    normalize(b, level::kCreature + 1.5f);
    return b.sound();
}
Sound hpMaul(int rate) {
    Rand r(7106u);
    Buf b(1.5f, rate);
    ImpactSpec s;
    s.modes = modesConcrete(110.0f, r);
    s.contact = 0.006f, s.noise = 0.4f, s.noiseHz = 600.0f;
    b.add(impact(s, 1.0f, 0.2f, rate, r), 0.0f, 0.9f);  // der Aufprall
    crunch(b, 0.02f, 0.8f, r);
    growlAt(b, 0.05f, 1.2f, 92.0f, 1.0f, 0.7f, r, 9.0f);  // Knurren durch die Zaehne, Schuetteln
    tearing(b, 0.15f, 0.9f, 0.6f, r);
    trim(b);
    normalize(b, level::kCreature + 3.0f);
    return b.sound();
}
Sound hpFlinch(int rate) {
    Rand r(7107u);
    Buf b(1.0f, rate);
    VoiceSpec vs;
    vs.f0 = [](float u) { return 760.0f - 300.0f * u + 60.0f * std::sin(u * 18.0f); };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.1f) * (1.0f - dsp::smooth01((u - 0.55f) / 0.45f)); };
    vs.formants = {430.0f, 1900.0f, 3000.0f}, vs.bandwidths = {80.0f, 130.0f, 200.0f};
    vs.jitter = 0.03f, vs.shimmer = 0.15f, vs.breath = 0.5f, vs.tiltHz = 4200.0f;
    voice(b, 0.0f, 0.55f, vs, r);
    growlAt(b, 0.45f, 0.45f, 80.0f, 0.5f, 0.1f, r);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}
Sound hpRush(int rate) {
    Rand r(7108u);
    Buf b(1.8f, rate);
    grains(b, 0.0f, 1.5f, 26.0f, [&](Buf& g, float t, Rand& rr) {
        ImpactSpec s;
        s.modes = modesConcrete(rr.range(160.0f, 260.0f), rr);
        s.contact = 0.003f, s.noise = 0.6f, s.noiseHz = rr.range(1200.0f, 2600.0f);
        g.add(impact(s, 0.8f, 0.1f, g.rate, rr), t, rr.range(0.3f, 0.9f));
        if (rr.uni() < 0.5f) {  // Krallen
            ImpactSpec c;
            c.modes = modesCeramic(rr.range(3000.0f, 4500.0f), 0.01f, rr);
            c.contact = 0.0003f, c.noise = 0.5f;
            g.add(impact(c, 0.6f, 0.03f, g.rate, rr), t + 0.012f, rr.range(0.1f, 0.25f));
        }
    }, r, [](float t) { return 0.4f + 0.6f * std::min(1.0f, t / 0.8f); });
    for (float t = 0.1f; t < 1.4f; t += 0.27f) breathe(b, t, 0.1f, t > 0.6f, 0.3f, 0.8f, r);
    reverb(b, corridor(), 0.25f, 1.0f, 78u);
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}
Sound hpLurk(int rate) {
    Rand r(7109u);
    Buf b(2.6f, rate);
    breathe(b, 0.0f, 0.9f, true, 0.6f, 0.5f, r);
    breathe(b, 1.05f, 1.2f, false, 0.7f, 0.4f, r);
    band(b, 1.1f, 0.9f, 140.0f, 2.0f, 0.15f, r, [](float u) { return std::sin(3.14159f * u) * (0.6f + 0.4f * std::sin(u * 70.0f)); });  // Lippen flattern
    wet(b, 0.9f, 2.2f, 4.0f, 0.05f, r);
    trim(b);
    normalize(b, level::kCreature - 9.0f);
    return b.sound();
}
Sound hpRasp(int rate) {
    Rand r(7110u);
    Buf b(1.6f, rate);
    breathe(b, 0.0f, 0.5f, true, 0.7f, 1.0f, r);
    breathe(b, 0.6f, 0.8f, false, 0.8f, 1.0f, r);
    wet(b, 0.05f, 1.3f, 26.0f, 0.4f, r);  // Blut in der Luftroehre
    band(b, 0.6f, 0.8f, 260.0f, 3.0f, 0.3f, r, [](float u) { return std::sin(3.14159f * u) * (0.5f + 0.5f * std::sin(u * 90.0f)); });
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

// --- grosses Buendel --------------------------------------------------------------------------------------------------------
Sound clCeiling(int rate) {
    Rand r(7201u);
    Buf b(1.6f, rate);
    float t = 0.0f;
    for (int k = 0; k < 5; ++k) {
        slap(b, t, r.range(0.5f, 0.9f), 1.0f, r);
        t += r.range(0.18f, 0.3f);
    }
    grains(b, 0.1f, 1.4f, 140.0f, [&](Buf& g, float at, Rand& rr) {  // Staub, Putzkoerner
        ImpactSpec s;
        s.modes = modesConcrete(rr.range(2500.0f, 5000.0f), rr);
        s.contact = 0.0003f, s.noise = 0.8f, s.noiseHz = 6000.0f;
        g.add(impact(s, 0.4f, 0.02f, g.rate, rr), at, rr.range(0.02f, 0.08f));
    }, r, [](float x) { return std::max(0.0f, 1.0f - x / 1.4f); });
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}
Sound clBones(int rate) {
    Rand r(7202u);
    Buf b(0.9f, rate);
    for (int k = 0; k < 6; ++k) {
        ImpactSpec s;
        s.modes = modesWood(r.range(700.0f, 1300.0f), 0.05f, r);  // trocken, hohl
        s.contact = 0.0008f, s.noise = 0.45f, s.noiseHz = 2500.0f;
        b.add(impact(s, 0.8f, 0.12f, rate, r), r.range(0.0f, 0.55f), r.range(0.4f, 1.0f));
    }
    ImpactSpec m;
    m.modes = modesMetalPipe(r.range(600.0f, 800.0f), 0.6f, r);
    m.contact = 0.0006f, m.noise = 0.2f;
    b.add(impact(m, 0.5f, 0.8f, rate, r), r.range(0.1f, 0.4f), 0.25f);  // das Rohr tickt
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}
Sound clBreath(int rate) {
    Rand r(7203u);
    Buf b(3.0f, rate);
    band(b, 0.0f, 0.9f, 4200.0f, 1.5f, 0.35f, r, [](float u) { return std::sin(3.14159f * u); });  // durch die Zaehne ein
    band(b, 0.0f, 0.9f, 2600.0f, 2.0f, 0.2f, r, [](float u) { return std::sin(3.14159f * u); });
    growlAt(b, 1.05f, 1.6f, 52.0f, 0.7f, 0.3f, r);  // tief grollend aus
    breathe(b, 1.05f, 1.5f, false, 0.6f, 0.6f, r);
    wet(b, 1.0f, 2.6f, 6.0f, 0.08f, r);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}
Sound clDrop(int rate) {
    Rand r(7204u);
    Buf b(1.6f, rate);
    thud(b, 0.0f, 48.0f, 0.5f, 1.0f);
    ImpactSpec s;
    s.modes = modesConcrete(90.0f, r);
    s.contact = 0.012f, s.noise = 0.5f, s.noiseHz = 500.0f;
    b.add(impact(s, 1.0f, 0.4f, rate, r), 0.0f, 0.9f);
    for (int k = 0; k < 5; ++k) slap(b, 0.08f + r.range(0.0f, 0.4f), r.range(0.3f, 0.7f), 0.85f, r);
    crunch(b, 0.03f, 0.25f, r);
    trim(b);
    normalize(b, level::kEvent + 1.5f);
    return b.sound();
}
Sound clScrape(int rate) {
    Rand r(7205u);
    Buf b(1.4f, rate);
    CreakSpec c;
    c.rate0 = 60.0f, c.rate1 = 140.0f, c.jitter = 0.35f, c.formants = {1900.0f, 3300.0f, 5200.0f}, c.q = 6.0f;
    creak(b, 0.0f, 1.1f, c, 0.9f, r);
    band(b, 0.0f, 1.1f, 3000.0f, 0.8f, 0.25f, r, [](float u) { return dsp::smooth01(u / 0.2f) * (0.7f + 0.3f * std::sin(u * 40.0f)); });  // Grus
    growlAt(b, 0.3f, 0.9f, 60.0f, 0.2f, 0.6f, r);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}
Sound clSwing(int rate) {
    Rand r(7206u);
    Buf b(0.7f, rate);
    Biquad bp = Biquad::make(Biquad::BP, 400.0f, 0.9f, (float)rate);
    for (size_t k = 0; k < b.n(); ++k) {
        const float u = (float)k / (float)b.n();
        bp.set(Biquad::BP, 300.0f + 900.0f * std::sin(3.14159f * u), 0.9f, (float)rate);
        b.x[k] += bp.run(r.bi()) * std::pow(std::sin(3.14159f * u), 2.0f);
    }
    trim(b);
    normalize(b, level::kEvent - 1.0f);
    return b.sound();
}
Sound clHit(int rate) {
    Rand r(7207u);
    Buf b(0.9f, rate);
    thud(b, 0.0f, 62.0f, 0.3f, 1.0f);
    crunch(b, 0.01f, 0.7f, r);
    tearing(b, 0.05f, 0.35f, 0.4f, r);
    trim(b);
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}
Sound clMiss(int rate) {
    Rand r(7208u);
    Buf b(1.6f, rate);
    ImpactSpec s;
    s.modes = modesConcrete(130.0f, r);
    s.contact = 0.003f, s.noise = 0.6f, s.noiseHz = 1800.0f;
    b.add(impact(s, 1.0f, 0.3f, rate, r), 0.0f, 0.9f);
    ImpactSpec m;
    m.modes = modesMetalPipe(r.range(420.0f, 520.0f), 1.1f, r);
    m.contact = 0.0007f, m.noise = 0.25f;
    b.add(impact(m, 1.0f, 1.4f, rate, r), 0.002f, 0.5f);
    grains(b, 0.02f, 0.5f, 60.0f, [&](Buf& g, float at, Rand& rr) {  // Splitter
        ImpactSpec p;
        p.modes = modesConcrete(rr.range(2000.0f, 4500.0f), rr);
        p.contact = 0.0004f, p.noise = 0.7f;
        g.add(impact(p, 0.5f, 0.03f, g.rate, rr), at, rr.range(0.05f, 0.2f));
    }, r, [](float x) { return std::max(0.0f, 1.0f - x / 0.5f); });
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}
Sound clSnap(int rate) {
    Rand r(7209u);
    Buf b(2.0f, rate);
    CreakSpec c;
    c.rate0 = 30.0f, c.rate1 = 90.0f, c.jitter = 0.2f, c.formants = {380.0f, 900.0f, 2100.0f}, c.q = 10.0f;
    creak(b, 0.0f, 0.45f, c, 0.7f, r);  // biegt sich
    ImpactSpec m;
    m.modes = modesMetalPipe(r.range(900.0f, 1100.0f), 0.5f, r);
    m.contact = 0.0003f, m.noise = 0.4f, m.noiseHz = 5000.0f;
    b.add(impact(m, 1.0f, 0.6f, rate, r), 0.46f, 0.9f);  // bricht
    float t = 0.75f;
    for (int k = 0; k < 5; ++k) {  // das Stueck faellt und rollt weg
        ImpactSpec p;
        p.modes = modesMetalPipe(r.range(1200.0f, 1500.0f), 0.3f, r);
        p.contact = 0.0006f, p.noise = 0.2f;
        b.add(impact(p, 0.6f, 0.4f, rate, r), t, 0.5f * std::pow(0.7f, (float)k));
        t += r.range(0.12f, 0.2f);
    }
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}
Sound clTear(int rate) {
    Rand r(7210u);
    Buf b(2.4f, rate);
    tearing(b, 0.0f, 1.2f, 0.9f, r);
    crunch(b, 0.9f, 1.0f, r);  // der Knochen bricht frei
    ImpactSpec s;
    s.modes = modesWood(r.range(500.0f, 700.0f), 0.08f, r);
    s.contact = 0.0006f, s.noise = 0.5f;
    b.add(impact(s, 1.0f, 0.2f, rate, r), 0.92f, 0.9f);
    band(b, 1.0f, 1.0f, 300.0f, 0.7f, 0.25f, r, [](float u) { return (1.0f - u) * (0.6f + 0.4f * std::sin(u * 25.0f)); });  // es blutet
    wet(b, 1.0f, 2.0f, 18.0f, 0.3f, r);
    growlAt(b, 0.2f, 1.6f, 48.0f, 0.4f, 0.8f, r);
    trim(b);
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}
Sound clStep(int rate, int v) {
    Rand r(7220u + (unsigned)v);
    Buf b(0.5f, rate);
    slap(b, 0.0f, 1.0f, 0.7f + 0.1f * (float)v, r);
    wet(b, 0.02f, 0.2f, 20.0f, 0.06f, r);
    trim(b);
    normalize(b, level::kStep + 4.0f);
    return b.sound();
}
}  // namespace

void addHoundPackSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"hp_call", [=] { return hpCall(rate); }});
    d.push_back({"hp_answer", [=] { return hpAnswer(rate); }});
    d.push_back({"hp_fast", [=] { return hpFast(rate); }});
    d.push_back({"hp_yip", [=] { return hpYip(rate); }});
    d.push_back({"hp_lunge", [=] { return hpLunge(rate); }});
    d.push_back({"hp_maul", [=] { return hpMaul(rate); }});
    d.push_back({"hp_flinch", [=] { return hpFlinch(rate); }});
    d.push_back({"hp_rush", [=] { return hpRush(rate); }});
    d.push_back({"hp_lurk", [=] { return hpLurk(rate); }});
    d.push_back({"hp_rasp", [=] { return hpRasp(rate); }});
    d.push_back({"cl3_ceiling", [=] { return clCeiling(rate); }});
    d.push_back({"cl3_bones", [=] { return clBones(rate); }});
    d.push_back({"cl3_breath", [=] { return clBreath(rate); }});
    d.push_back({"cl3_drop", [=] { return clDrop(rate); }});
    d.push_back({"cl3_scrape", [=] { return clScrape(rate); }});
    d.push_back({"cl3_swing", [=] { return clSwing(rate); }});
    d.push_back({"cl3_hit", [=] { return clHit(rate); }});
    d.push_back({"cl3_miss", [=] { return clMiss(rate); }});
    d.push_back({"cl3_snap", [=] { return clSnap(rate); }});
    d.push_back({"cl3_tear", [=] { return clTear(rate); }});
    d.push_back({"cl3_step", [=] { return clStep(rate, 0); }});
}

}  // namespace lim::audio
