// V11: Klaenge des Spielers - Schritte je Untergrund, Sprung und Landung, Atem, Herzschlag,
// Gegenstaende und Oberflaeche. Alles aus physikalisch gedachten Bausteinen (synth_kit.hpp).
//
// Schritt = Ferse (Gummisohle, Kontakt 3-15 ms je Untergrund) + Abrollen + Ballen 70-110 ms spaeter,
// dazu der Untergrund: Sandkoerner auf Beton, Fasern im Teppich, Wasser in der Pfuetze, Moden
// des Gitters, Knarren der Diele. Jede Variante hat eigene Zeiten, Staerken und Moden.
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;
using dsp::Mode;

namespace {

enum Surface { Concrete, ConcreteWet, Carpet, CarpetWet, Tile, Marble, Metal, Wood, Water, kSurfaces };
const char* kSurfaceName[kSurfaces] = {"step_concrete", "step_puddle", "step_carpet", "step_carpet_wet", "step_tile",
                                       "step_marble",   "step_metal",  "step_wood",   "step_water"};

// tiefer Koerperanteil (Gewicht, das auf den Boden kommt)
void thump(Buf& b, float at, float amp, float hz, float t60, Rand& r) {
    Biquad lp = Biquad::make(Biquad::LP, hz, 0.8f, (float)b.rate);
    const size_t a = b.at(at), n = b.at(t60 * 1.2f);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) b.x[a + i] += lp.run(r.bi()) * amp * envAD(b.t(i), 0.003f, t60) * 3.0f;
}

// feines Korn (Sand, Staub) unter der Sohle
void grit(Buf& b, float t0, float t1, float perSec, float amp, Rand& r) {
    grains(b, t0, t1, perSec, [&](Buf& bb, float t, Rand& rr) {
        const size_t a = bb.at(t), n = 60;
        if (a + n > bb.n()) bb.x.resize(a + n, 0.0f);
        const float g = amp * rr.range(0.3f, 1.0f);
        // Sandkorn zwischen Sohle und Beton: kurzer Knack im Bereich 2-7 kHz
        Biquad bp = Biquad::make(Biquad::BP, rr.range(2200.0f, 6500.0f), 1.4f, (float)bb.rate);
        for (size_t i = 0; i < n; ++i) bb.x[a + i] += bp.run(i < 3 ? rr.bi() : 0.0f) * g;
    }, r, [&](float t) { return 1.0f - (t - t0) / std::max(0.01f, t1 - t0); });
}

// Fasern (Teppich): leises, helles Rauschen beim Abrollen
void brush(Buf& b, float at, float len, float amp, Rand& r) {
    Biquad hp = Biquad::make(Biquad::HP, 2400.0f, 0.7f, (float)b.rate), lp = Biquad::make(Biquad::LP, 7000.0f, 0.7f, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) b.x[a + i] += lp.run(hp.run(r.bi())) * amp * envAHR(b.t(i), len * 0.25f, len * 0.2f, len * 0.55f);
}

void splash(Buf& b, float at, float amp, float size, Rand& r) {
    // Aufprall auf Wasser: breitbandiges Rauschen, dazu viele kleine Blasen und ein tiefer Schwall
    Biquad bp = Biquad::make(Biquad::BP, r.range(900.0f, 1500.0f), 0.6f, (float)b.rate);
    const size_t a = b.at(at), n = b.at(0.25f * size);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) b.x[a + i] += bp.run(r.bi()) * amp * envAD(b.t(i), 0.002f, 0.09f * size) * 1.5f;
    thump(b, at, amp * 0.5f, 260.0f, 0.06f * size, r);
    grains(b, at + 0.005f, at + 0.18f * size, 260.0f * size, [&](Buf& bb, float t, Rand& rr) {
        bubble(bb, t, rr.range(0.5f, 2.2f), amp * rr.range(0.05f, 0.22f), rr);
    }, r, [&](float t) { return 1.0f - (t - at) / (0.18f * size); });
}

Sound step(int rate, Surface s, int v) {
    Rand r(1000u + (unsigned)s * 97u + (unsigned)v * 13u);
    Buf b(0.7f, rate);
    const float toe = r.range(0.07f, 0.11f), toeAmp = r.range(0.4f, 0.62f), heelAmp = r.range(0.85f, 1.0f);
    switch (s) {
        case Concrete:
        case ConcreteWet: {
            ImpactSpec sp;
            sp.modes = modesConcrete(r.range(170.0f, 260.0f), r);
            sp.contact = r.range(0.004f, 0.006f), sp.noise = 0.7f, sp.noiseHz = r.range(1500.0f, 2300.0f), sp.noiseT60 = 0.014f;
            b.add(impact(sp, heelAmp, 0.12f, rate, r), 0.0f);
            thump(b, 0.0f, 0.35f * heelAmp, 160.0f, 0.04f, r);
            sp.contact *= 0.8f;
            b.add(impact(sp, toeAmp, 0.1f, rate, r), toe);
            if (s == Concrete) {
                grit(b, 0.0f, 0.06f, r.range(70.0f, 140.0f), 0.035f, r);
                grit(b, toe, toe + 0.08f, r.range(90.0f, 170.0f), 0.045f, r);  // Abrollen: Sand knirscht
            } else {
                splash(b, 0.004f, 0.9f, 1.0f, r);
                splash(b, toe + 0.006f, 0.45f, 0.7f, r);
            }
            break;
        }
        case Carpet:
        case CarpetWet: {
            thump(b, 0.0f, 0.7f * heelAmp, 230.0f, 0.05f, r);
            thump(b, toe, 0.35f * toeAmp, 260.0f, 0.04f, r);
            brush(b, 0.01f, toe + 0.08f, 0.012f, r);
            if (s == CarpetWet) {
                // feuchter Teppich: Schmatzen beim Anheben, kleine Blasen
                Biquad bp = Biquad::make(Biquad::BP, r.range(350.0f, 520.0f), 2.2f, (float)rate);
                const float at = toe + 0.03f, len = r.range(0.1f, 0.16f);
                const size_t a = b.at(at), n = b.at(len);
                for (size_t i = 0; i < n && a + i < b.n(); ++i)
                    b.x[a + i] += bp.run(r.bi()) * 0.35f * envAHR(b.t(i), len * 0.5f, 0.0f, len * 0.5f);
                grains(b, 0.01f, toe + 0.12f, 45.0f, [&](Buf& bb, float t, Rand& rr) {
                    bubble(bb, t, rr.range(1.2f, 2.8f), rr.range(0.015f, 0.04f), rr);  // Wasser im Flor, dumpf
                }, r);
            }
            break;
        }
        case Tile:
        case Marble: {
            ImpactSpec sp;
            const bool m = s == Marble;
            sp.modes = modesCeramic(m ? r.range(900.0f, 1400.0f) : r.range(1700.0f, 2600.0f), m ? 0.09f : 0.05f, r);
            sp.contact = r.range(0.0025f, 0.0035f), sp.noise = 0.35f, sp.noiseHz = 4200.0f, sp.noiseT60 = 0.01f;
            b.add(impact(sp, heelAmp, 0.2f, rate, r), 0.0f);
            thump(b, 0.0f, 0.25f, 180.0f, 0.03f, r);
            b.add(impact(sp, toeAmp * 0.8f, 0.2f, rate, r), toe);
            if (v == 5) {  // Gummisohle quietscht kurz auf der glatten Flaeche
                CreakSpec c;
                c.rate0 = r.range(900.0f, 1100.0f), c.rate1 = c.rate0 * 1.25f, c.jitter = 0.04f;
                c.formants = {2100.0f, 3900.0f}, c.q = 12.0f;
                creak(b, toe + 0.01f, 0.05f, c, 0.25f, r);
            }
            break;
        }
        case Metal: {
            ImpactSpec sp;
            sp.modes = modesMetalPlate(r.range(190.0f, 320.0f), 0.35f, r);
            sp.contact = r.range(0.0022f, 0.003f), sp.noise = 0.3f, sp.noiseHz = 3000.0f;
            b.add(impact(sp, heelAmp, 0.6f, rate, r), 0.0f);
            // das Gitter klappert in seinem Rahmen nach
            for (int k = 1; k <= 2; ++k) b.add(impact(sp, heelAmp * 0.25f / (float)k, 0.3f, rate, r), r.range(0.012f, 0.026f) * (float)k);
            b.add(impact(sp, toeAmp, 0.5f, rate, r), toe);
            thump(b, 0.0f, 0.2f, 140.0f, 0.04f, r);
            break;
        }
        case Wood: {
            ImpactSpec sp;
            sp.modes = modesWood(r.range(110.0f, 175.0f), 0.13f, r);
            sp.contact = r.range(0.005f, 0.007f), sp.noise = 0.35f, sp.noiseHz = 1100.0f, sp.noiseT60 = 0.02f;
            b.add(impact(sp, heelAmp, 0.3f, rate, r), 0.0f);
            b.add(impact(sp, toeAmp, 0.25f, rate, r), toe);
            thump(b, 0.0f, 0.35f, 150.0f, 0.05f, r);
            if (v % 3 == 0) {  // die Diele knarrt unter dem Gewicht
                CreakSpec c;
                c.rate0 = r.range(25.0f, 45.0f), c.rate1 = r.range(55.0f, 95.0f), c.jitter = 0.3f;
                c.formants = {r.range(450.0f, 650.0f), r.range(1100.0f, 1500.0f), r.range(2400.0f, 3000.0f)};
                c.q = 9.0f;
                creak(b, r.range(0.03f, 0.07f), r.range(0.16f, 0.32f), c, 0.6f, r);
            }
            break;
        }
        case Water: {
            // Waten: Bein durchbricht die Oberflaeche, Schwall, Blasen
            splash(b, 0.0f, 1.0f, 1.6f, r);
            Biquad lp = Biquad::make(Biquad::LP, 500.0f, 0.7f, (float)rate);
            const size_t n = b.at(0.5f);
            for (size_t i = 0; i < n && i < b.n(); ++i) b.x[i] += lp.run(r.bi()) * 0.6f * envAHR(b.t(i), 0.12f, 0.08f, 0.3f);
            break;
        }
        default: break;
    }
    dcBlock(b);
    trim(b);
    normalize(b, level::kStep + (s == Metal ? 1.5f : 0.0f) + (s == Carpet ? -2.0f : 0.0f));
    return b.sound();
}

Sound land(int rate, int v) {
    Rand r(1500u + (unsigned)v);
    Buf b(0.6f, rate);
    ImpactSpec sp;
    sp.modes = modesConcrete(r.range(120.0f, 180.0f), r);
    sp.contact = 0.007f, sp.noise = 0.6f, sp.noiseHz = 1400.0f, sp.noiseT60 = 0.02f;
    b.add(impact(sp, 1.0f, 0.15f, rate, r), 0.0f);
    b.add(impact(sp, 0.7f, 0.15f, rate, r), r.range(0.015f, 0.03f));  // zweiter Fuss
    thump(b, 0.0f, 1.0f, 110.0f, 0.08f, r);
    // Kleidung und Knie
    brush(b, 0.0f, 0.18f, 0.08f, r);
    breathe(b, 0.03f, 0.22f, false, 0.25f, 0.6f, r);
    dcBlock(b);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound jump(int rate, int v) {
    Rand r(1600u + (unsigned)v);
    Buf b(0.5f, rate);
    // Abstoss: Sohle reibt, Kleidung raschelt, kurzer Atemstoss
    grit(b, 0.0f, 0.05f, 150.0f, 0.06f, r);
    brush(b, 0.0f, 0.22f, 0.07f, r);
    breathe(b, 0.02f, 0.2f, false, 0.4f, 0.7f, r);
    trim(b);
    normalize(b, level::kFoley - 4.0f);
    return b.sound();
}

// Atemzug (ein und aus), erschoepft: kuerzer, rauer
Sound breathCycle(int rate, int v) {
    Rand r(1700u + (unsigned)v);
    const float in = r.range(0.55f, 0.75f), out = r.range(0.7f, 0.95f);
    Buf b(in + out + 0.3f, rate);
    breathe(b, 0.0f, in, true, 0.6f, 0.8f, r);
    breathe(b, in + r.range(0.03f, 0.08f), out, false, 0.7f, 0.6f, r);
    trim(b);
    normalize(b, level::kFoley - 3.0f);
    return b.sound();
}

Sound gasp(int rate) {
    Rand r(1750u);
    Buf b(0.8f, rate);
    breathe(b, 0.0f, 0.32f, true, 1.0f, 1.0f, r);
    breathe(b, 0.38f, 0.35f, false, 0.5f, 0.4f, r);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

// Herzschlag (Schleife, ein Zyklus): Erster Ton tiefer und staerker, zweiter kuerzer
Sound heartbeat(int rate) {
    const float cycle = 0.86f;
    Buf b(cycle, rate);
    Rand r(1800u);
    auto beat = [&](float at, float f0, float f1, float len, float amp) {
        Biquad lp = Biquad::make(Biquad::LP, 120.0f, 0.7f, (float)rate);
        float ph = 0;
        const size_t a = b.at(at), n = b.at(len);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = b.t(i), u = t / len;
            ph += kTwoPi * dsp::lerpf(f0, f1, u) / (float)rate;
            b.x[a + i] += (std::sin(ph) * 0.8f + lp.run(r.bi()) * 1.2f) * amp * envAD(t, 0.008f, len * 0.7f);
        }
    };
    beat(0.02f, 55.0f, 38.0f, 0.13f, 1.0f);
    beat(0.30f, 62.0f, 45.0f, 0.1f, 0.65f);
    normalize(b, level::kFoley - 2.0f);
    return b.sound(true);
}

Sound death(int rate) {
    Rand r(1900u);
    Buf b(3.0f, rate);
    // Zusammenbrechen: Knie, Koerper, Kleidung; dann ein langer, leiser Ausatem
    thump(b, 0.0f, 0.8f, 120.0f, 0.12f, r);
    thump(b, 0.18f, 1.0f, 90.0f, 0.2f, r);
    brush(b, 0.0f, 0.5f, 0.1f, r);
    grit(b, 0.18f, 0.35f, 120.0f, 0.06f, r);
    breathe(b, 0.6f, 1.8f, false, 0.35f, 0.2f, r);
    filter(b, Biquad::LP, 2500.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

// --- Gegenstaende ------------------------------------------------------------------------------------
Sound pickup(int rate, int v, bool rare) {
    Rand r(2000u + (unsigned)v * 7u + (rare ? 50u : 0u));
    Buf b(rare ? 1.4f : 0.5f, rate);
    brush(b, 0.0f, 0.12f, 0.12f, r);
    ImpactSpec sp;
    sp.modes = modesPlastic(r.range(700.0f, 1100.0f), 0.05f, r);
    sp.contact = 0.002f, sp.noise = 0.2f;
    b.add(impact(sp, 0.6f, 0.15f, rate, r), r.range(0.04f, 0.07f));
    if (rare) {  // leises, glasiges Klingen
        ImpactSpec g;
        g.modes = modesGlass(r.range(2200.0f, 2600.0f), 0.9f, r);
        g.contact = 0.0012f, g.noise = 0.05f;
        b.add(impact(g, 0.25f, 1.2f, rate, r), 0.09f);
    }
    trim(b);
    normalize(b, level::kFoley - (rare ? 0.0f : 3.0f));
    return b.sound();
}

Sound drink(int rate) {
    Rand r(2100u);
    Buf b(2.0f, rate);
    // Verschluss: Ratsche der Plastikkappe, dann Ploppen
    grains(b, 0.0f, 0.22f, 60.0f, [&](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesPlastic(rr.range(2400.0f, 3200.0f), 0.01f, rr);
        sp.contact = 0.0006f, sp.noise = 0.3f, sp.noiseHz = 5000.0f;
        bb.add(impact(sp, rr.range(0.2f, 0.4f), 0.03f, bb.rate, rr), t);
    }, r);
    bubble(b, 0.26f, 5.0f, 0.4f, r);
    // drei Schlucke: Luftblase im Hals der Flasche, Kehle
    for (float at : {0.55f, 0.92f, 1.3f}) {
        bubble(b, at, r.range(6.0f, 8.5f), 0.7f, r);
        bubble(b, at + 0.05f, r.range(4.0f, 6.0f), 0.35f, r);
        thump(b, at + 0.02f, 0.35f, 140.0f, 0.06f, r);
    }
    breathe(b, 1.55f, 0.35f, false, 0.25f, 0.3f, r);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound eat(int rate) {
    Rand r(2200u);
    Buf b(1.6f, rate);
    // Folie knistert
    grains(b, 0.0f, 0.45f, 260.0f, [&](Buf& bb, float t, Rand& rr) {
        Biquad bp = Biquad::make(Biquad::BP, rr.range(2000.0f, 6500.0f), 3.0f, (float)bb.rate);
        const size_t a = bb.at(t), n = 120;
        if (a + n > bb.n()) bb.x.resize(a + n, 0.0f);
        const float g = rr.range(0.1f, 0.5f);
        for (size_t i = 0; i < n; ++i) bb.x[a + i] += bp.run(rr.bi()) * g * envAD((float)i / (float)bb.rate, 0.0001f, 0.0025f);
    }, r, [](float t) { return 0.4f + 0.6f * std::fabs(std::sin(t * 9.0f)); });
    // Biss und Kauen: Krachen, Krumen
    for (float at : {0.7f, 1.0f, 1.25f}) {
        ImpactSpec sp;
        sp.modes = modesConcrete(r.range(400.0f, 700.0f), r);
        sp.contact = 0.003f, sp.noise = 0.8f, sp.noiseHz = 2500.0f, sp.noiseT60 = 0.03f;
        b.add(impact(sp, at == 0.7f ? 1.0f : 0.45f, 0.08f, rate, r), at);
        grit(b, at, at + 0.07f, 300.0f, 0.12f, r);
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound battery(int rate) {
    Rand r(2300u);
    Buf b(0.8f, rate);
    ImpactSpec sp;
    sp.modes = modesPlastic(1500.0f, 0.02f, r);
    sp.contact = 0.0008f, sp.noise = 0.4f, sp.noiseHz = 4000.0f;
    b.add(impact(sp, 1.0f, 0.06f, rate, r), 0.0f);
    // Batterie gleitet in das Fach (Metall auf Kunststoff)
    Biquad bp = Biquad::make(Biquad::BP, 3200.0f, 1.5f, (float)rate);
    const size_t a = b.at(0.12f), n = b.at(0.14f);
    for (size_t i = 0; i < n; ++i) b.x[a + i] += bp.run(r.bi()) * 0.25f * envAHR(b.t(i), 0.03f, 0.06f, 0.05f);
    ImpactSpec m;
    m.modes = modesMetalPlate(2400.0f, 0.04f, r);
    m.contact = 0.0007f, m.noise = 0.1f;
    b.add(impact(m, 0.4f, 0.08f, rate, r), 0.27f);
    b.add(impact(sp, 0.9f, 0.06f, rate, r), 0.42f);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound paper(int rate) {
    Rand r(2400u);
    Buf b(0.9f, rate);
    // Papier auffalten: Knistern in Wellen, dazu das tiefe "Wusch" der Flaeche
    grains(b, 0.0f, 0.6f, 180.0f, [&](Buf& bb, float t, Rand& rr) {
        Biquad bp = Biquad::make(Biquad::BP, rr.range(1200.0f, 4500.0f), 2.0f, (float)bb.rate);
        const size_t a = bb.at(t), n = 200;
        if (a + n > bb.n()) bb.x.resize(a + n, 0.0f);
        const float g = rr.range(0.1f, 0.4f);
        for (size_t i = 0; i < n; ++i) bb.x[a + i] += bp.run(rr.bi()) * g * envAD((float)i / (float)bb.rate, 0.0002f, 0.004f);
    }, r, [](float t) { return t < 0.15f ? 1.0f : (t < 0.35f ? 0.3f : 0.8f); });
    Biquad lp = Biquad::make(Biquad::LP, 350.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.5f); ++i) b.x[i] += lp.run(r.bi()) * 0.5f * envAHR(b.t(i), 0.1f, 0.1f, 0.3f);
    trim(b);
    normalize(b, level::kFoley - 2.0f);
    return b.sound();
}

Sound flashClick(int rate, bool on) {
    Rand r(on ? 2500u : 2501u);
    Buf b(0.15f, rate);
    ImpactSpec sp;
    sp.modes = modesPlastic(on ? 2800.0f : 2500.0f, 0.015f, r);
    sp.contact = 0.0006f, sp.noise = 0.5f, sp.noiseHz = 5000.0f, sp.noiseT60 = 0.004f;
    b.add(impact(sp, 1.0f, 0.04f, rate, r), 0.0f);
    b.add(impact(sp, 0.55f, 0.04f, rate, r), 0.009f);
    ImpactSpec spring;
    spring.modes = modesMetalPlate(4500.0f, 0.05f, r);
    spring.contact = 0.0004f, spring.noise = 0.0f;
    b.add(impact(spring, 0.15f, 0.08f, rate, r), 0.01f);
    trim(b);
    normalize(b, level::kFoley - 4.0f);
    return b.sound();
}

Sound itemLand(int rate, int v) {
    Rand r(2600u + (unsigned)v);
    Buf b(0.6f, rate);
    ImpactSpec sp;
    sp.modes = modesPlastic(r.range(550.0f, 900.0f), 0.06f, r);
    sp.contact = 0.0018f, sp.noise = 0.4f, sp.noiseHz = 2600.0f;
    b.add(impact(sp, 1.0f, 0.15f, rate, r), 0.0f);
    float at = 0.0f, s = 0.4f;
    for (int k = 0; k < 3; ++k) {  // springt zweimal auf, rollt kurz
        at += r.range(0.07f, 0.12f) * s * 2.0f;
        b.add(impact(sp, s, 0.12f, rate, r), at);
        s *= 0.4f;
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

// --- Oberflaeche -------------------------------------------------------------------------------------
// leise, taktile Klicks statt Pieptoene
Sound uiClick(int rate, float hz, float body, float amp, unsigned seed) {
    Rand r(seed);
    Buf b(0.12f, rate);
    ImpactSpec sp;
    sp.modes = modesWood(body, 0.04f, r);
    sp.contact = 0.0015f, sp.noise = 0.5f, sp.noiseHz = hz, sp.noiseQ = 1.2f, sp.noiseT60 = 0.006f;
    b.add(impact(sp, 1.0f, 0.1f, rate, r), 0.0f);
    trim(b);
    normalize(b, level::kUi + amp);
    return b.sound();
}

Sound regionFound(int rate) {
    // Ankunft in einer neuen Ebene: das Gebaeude "atmet" - tiefes Anschwellen, kein Musikton
    Rand r(2700u);
    Buf b(4.5f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 180.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = lp.run(br.run(r.bi())) * envAHR(t, 1.8f, 0.6f, 2.0f) * 2.0f;
    }
    dsp::RoomSpec room;
    room.t60Mid = 3.0f, room.t60Low = 3.5f, room.t60High = 1.0f, room.size = 2.4f;
    reverb(b, room, 0.6f, 0.6f, 2701u);
    trim(b);
    normalize(b, level::kEvent - 6.0f);
    return b.sound();
}

}  // namespace

void addPlayerSounds(std::vector<SoundDef>& d, int rate) {
    for (int s = 0; s < kSurfaces; ++s)
        for (int v = 0; v < 8; ++v)
            d.push_back({std::format("{}_{}", kSurfaceName[s], v), [=] { return step(rate, (Surface)s, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("land_{}", v), [=] { return land(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("jump_{}", v), [=] { return jump(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("breath_{}", v), [=] { return breathCycle(rate, v); }});
    d.push_back({"gasp", [=] { return gasp(rate); }});
    d.push_back({"heartbeat", [=] { return heartbeat(rate); }});
    d.push_back({"death", [=] { return death(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("pickup_{}", v), [=] { return pickup(rate, v, false); }});
    d.push_back({"pickup_rare", [=] { return pickup(rate, 0, true); }});
    d.push_back({"drink", [=] { return drink(rate); }});
    d.push_back({"eat", [=] { return eat(rate); }});
    d.push_back({"battery", [=] { return battery(rate); }});
    d.push_back({"paper", [=] { return paper(rate); }});
    d.push_back({"flash_on", [=] { return flashClick(rate, true); }});
    d.push_back({"flash_off", [=] { return flashClick(rate, false); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("item_land_{}", v), [=] { return itemLand(rate, v); }});
    d.push_back({"ui_move", [=] { return uiClick(rate, 3200.0f, 900.0f, -2.0f, 2800u); }});
    d.push_back({"ui_select", [=] { return uiClick(rate, 2400.0f, 420.0f, 0.0f, 2801u); }});
    d.push_back({"ui_back", [=] { return uiClick(rate, 1800.0f, 300.0f, -1.0f, 2802u); }});
    d.push_back({"toast", [=] { return uiClick(rate, 2000.0f, 600.0f, -4.0f, 2803u); }});
    d.push_back({"region_found", [=] { return regionFound(rate); }});
}

}  // namespace lim::audio
