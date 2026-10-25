// V11: Klaenge der Welt - Leuchten, Elektrik, Wasser, Rohre, Maschinen, Tueren, Aufzug.
//
// Leuchtstoffroehre (magnetisches Vorschaltgeraet am 50-Hz-Netz):
//   - Die Drossel brummt mit 100 Hz (Magnetostriktion, zweimal je Netzperiode) und Obertoenen; lose
//     Bleche schnarren (weiche Saettigung).
//   - Bei jedem Nulldurchgang zuendet die Gasentladung neu: ein kurzes, helles Knistern 100-mal je
//     Sekunde.
//   - Das Blechgehaeuse faerbt den Klang mit ein paar Eigenresonanzen.
//   - Langsame, unregelmaessige Schwankungen.
// Schleifen: Tonanteile sind exakt periodisch in der Schleifenlaenge (Vielfache von 1/L Hz), die
// Rauschanteile werden mit gleicher Leistung ueberblendet.
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;
using dsp::Mode;

namespace {

// Schleife aus periodischem Ton und ueberblendetem Rauschen. tone(t) muss in L Sekunden periodisch
// sein; noise(buf) fuellt einen Puffer der Laenge L + xf, der danach nahtlos gemacht wird.
Buf periodicLoop(int rate, float L, const std::function<float(float)>& tone, const std::function<void(Buf&)>& noise,
                 float xf = 0.6f) {
    Buf nz(L + xf, rate);
    const size_t full = nz.n();
    if (noise) {
        noise(nz);
        nz.x.resize(full);  // Ereignisse am Ende duerfen den Puffer nicht verlaengern
        loopify(nz, xf);
    }
    Buf b(L, rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] = (tone ? tone(b.t(i)) : 0.0f) + (i < nz.n() ? nz.x[i] : 0.0f);
    return b;
}

// --- Leuchten ---------------------------------------------------------------------------------------
// kind 0 Bueroleuchte (Rasterleuchte), 1 Leuchtband/Industrie, 2 Notleuchte (Ausgangsschild),
// 3 Wandleuchte (Gluehlampe, fast still)
Sound lampHum(int rate, int kind, int v) {
    Rand r(3000u + (unsigned)kind * 41u + (unsigned)v * 7u);
    const float L = 12.0f;
    // Obertoene des Netzbrummens (50 Hz Grundraster): 100 Hz dominiert
    float h[12];
    for (int k = 0; k < 12; ++k) h[k] = 0.0f;
    if (kind == 0) {
        const float base[12] = {0.06f, 1.0f, 0.18f, 0.5f, 0.08f, 0.28f, 0.05f, 0.16f, 0.03f, 0.09f, 0.02f, 0.05f};
        for (int k = 0; k < 12; ++k) h[k] = base[k] * r.range(0.7f, 1.3f);
    } else if (kind == 1) {
        const float base[12] = {0.12f, 1.0f, 0.1f, 0.38f, 0.04f, 0.14f, 0.02f, 0.06f, 0.0f, 0.02f, 0.0f, 0.01f};
        for (int k = 0; k < 12; ++k) h[k] = base[k] * r.range(0.8f, 1.2f);
    } else if (kind == 2) {
        const float base[12] = {0.02f, 1.0f, 0.3f, 0.6f, 0.25f, 0.45f, 0.2f, 0.35f, 0.15f, 0.25f, 0.1f, 0.18f};
        for (int k = 0; k < 12; ++k) h[k] = base[k] * r.range(0.8f, 1.2f);
    } else {
        h[1] = 1.0f, h[3] = 0.15f;
    }
    float ph[12];
    for (float& p : ph) p = r.range(0.0f, kTwoPi);
    const float drive = kind == 2 ? 2.4f : (kind == 1 ? 1.2f : 1.5f + 0.2f * (float)v);
    // langsame Schwankung: periodisch in L (Vielfache von 1/L Hz)
    const float m1 = (float)r.irange(1, 3) / L, m2 = (float)r.irange(4, 9) / L, mph = r.range(0.0f, kTwoPi);
    auto tone = [&](float t) {
        const float w = kTwoPi * 50.0f * t;
        float x = 0.0f;
        for (int k = 0; k < 12; ++k)
            if (h[k] > 0.0f) x += h[k] * std::sin(w * (float)(k + 1) + ph[k]);
        x = std::tanh(x * drive) / std::tanh(drive);  // schnarrende Bleche
        const float mod = 1.0f + 0.03f * std::sin(kTwoPi * m1 * t + mph) + 0.015f * std::sin(kTwoPi * m2 * t);
        return x * mod * (kind == 3 ? 0.25f : 1.0f);
    };
    // Gehaeuseresonanzen (Blech): faerben das Brummen
    ImpactSpec hs;
    hs.modes = modesMetalPlate(kind == 1 ? r.range(160.0f, 240.0f) : r.range(260.0f, 420.0f), 0.08f, r);
    dsp::ModalBank housing;
    for (const Mode& m : hs.modes) housing.add(m.hz, m.t60, m.gain);
    housing.prepare((float)rate);
    const float sizzleAmt = kind == 0 ? 0.25f : (kind == 1 ? 0.08f : (kind == 2 ? 0.12f : 0.0f));
    auto noise = [&](Buf& nz) {
        Biquad hp = Biquad::make(Biquad::HP, kind == 2 ? 6000.0f : 3500.0f, 0.7f, (float)rate);
        Biquad lp = Biquad::make(Biquad::LP, 12000.0f, 0.7f, (float)rate);
        float walk = 1.0f;
        // vereinzelte Unruhe der Roehre: zwei kurze Phasen, in denen das Knistern zunimmt
        const float u0 = r.range(1.0f, 5.0f), u1 = r.range(6.5f, 11.0f), ul = r.range(0.2f, 0.5f);
        for (size_t i = 0; i < nz.n(); ++i) {
            const float t = nz.t(i);
            // Neuzuendung bei jedem Nulldurchgang (100 Hz): kurzer, heller Puls
            const float half = std::fmod(t * 100.0f, 1.0f);
            const float strike = std::exp(-half * 60.0f);
            walk += (r.bi() * 0.02f) - (walk - 1.0f) * 0.0005f;
            const float unrest = (std::fabs(t - u0) < ul || std::fabs(t - u1) < ul) ? 2.5f : 1.0f;
            nz.x[i] = lp.run(hp.run(r.bi())) * strike * sizzleAmt * walk * unrest;
        }
    };
    Buf b = periodicLoop(rate, L, tone, noise);
    // Gehaeuse: der Ton regt die Blechresonanzen an (leise beigemischt). Vorlauf ueber das Ende der
    // (periodischen) Schleife, damit die Resonanzen am Anfang schon eingeschwungen sind
    for (size_t i = b.n() - b.at(1.0f); i < b.n(); ++i) housing.run(b.x[i] * 0.02f);
    std::vector<float> res(b.n());
    for (size_t i = 0; i < b.n(); ++i) res[i] = housing.run(b.x[i] * 0.02f) * 0.6f;
    for (size_t i = 0; i < b.n(); ++i) b.x[i] += res[i];
    normalize(b, level::kLoop + (kind == 3 ? -10.0f : (kind == 2 ? -4.0f : 0.0f)));
    return b.sound(true);
}

// Flackern einer alten Roehre: Starter klickt, Bogen zuendet kurz und bricht ab
Sound lampTick(int rate, int v) {
    Rand r(3100u + (unsigned)v);
    Buf b(0.4f, rate);
    ImpactSpec sp;
    sp.modes = modesMetalPlate(r.range(1800.0f, 2600.0f), 0.03f, r);
    sp.contact = 0.0004f, sp.noise = 0.8f, sp.noiseHz = 5000.0f, sp.noiseT60 = 0.003f;
    b.add(impact(sp, 1.0f, 0.06f, rate, r), 0.0f);
    // kurzer Brummstoss mit Netzobertoenen
    const float len = r.range(0.05f, 0.12f);
    for (size_t i = 0; i < b.at(len); ++i) {
        const float t = b.t(i), w = kTwoPi * 50.0f * t;
        const float x = std::sin(w * 2.0f) + 0.5f * std::sin(w * 4.0f) + 0.3f * std::sin(w * 6.0f);
        b.x[b.at(0.01f) + i] += std::tanh(x * 2.0f) * 0.25f * envAHR(t, 0.005f, len * 0.5f, len * 0.5f);
    }
    trim(b);
    normalize(b, level::kEvent - 8.0f);
    return b.sound();
}

// Wiederanlaufen nach Stromausfall: Starterklicks, dann schwillt das Brummen an
Sound lampStart(int rate, int v) {
    Rand r(3150u + (unsigned)v);
    Buf b(2.4f, rate);
    float at = r.range(0.05f, 0.15f);
    const int clicks = r.irange(2, 4);
    for (int k = 0; k < clicks; ++k) {
        ImpactSpec sp;
        sp.modes = modesMetalPlate(r.range(2000.0f, 3000.0f), 0.02f, r);
        sp.contact = 0.0004f, sp.noise = 0.6f, sp.noiseHz = 6000.0f;
        b.add(impact(sp, r.range(0.5f, 1.0f), 0.05f, rate, r), at);
        at += r.range(0.25f, 0.55f);
    }
    for (size_t i = b.at(at); i < b.n(); ++i) {
        const float t = b.t(i), u = (t - at) / 0.6f, w = kTwoPi * 50.0f * t;
        const float x = std::sin(w * 2.0f) + 0.5f * std::sin(w * 4.0f) + 0.25f * std::sin(w * 6.0f);
        b.x[i] += std::tanh(x * 1.6f) * 0.12f * dsp::smooth01(u) * (1.0f - dsp::smooth01((t - 2.0f) / 0.4f));
    }
    normalize(b, level::kEvent - 8.0f);
    return b.sound();
}

// Roehre platzt (Level 3): Knall, Glasscherben regnen
Sound lampPop(int rate, int v) {
    Rand r(3200u + (unsigned)v);
    Buf b(1.6f, rate);
    ImpactSpec pop;
    pop.modes = modesGlass(r.range(1800.0f, 2600.0f), 0.3f, r);
    pop.contact = 0.0005f, pop.noise = 1.5f, pop.noiseHz = 3000.0f, pop.noiseQ = 0.5f, pop.noiseT60 = 0.03f;
    b.add(impact(pop, 1.0f, 0.5f, rate, r), 0.0f);
    grains(b, 0.25f, 1.1f, 40.0f, [&](Buf& bb, float t, Rand& rr) {
        ImpactSpec sh;
        sh.modes = modesGlass(rr.range(2500.0f, 6500.0f), 0.08f, rr);
        sh.contact = 0.0003f, sh.noise = 0.2f;
        bb.add(impact(sh, rr.range(0.05f, 0.3f), 0.1f, bb.rate, rr), t);
    }, r, [](float t) { return 1.0f - (t - 0.25f) / 0.85f; });
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

// --- Elektrik ----------------------------------------------------------------------------------------
void crackle(Buf& b, float t0, float t1, float perSec, float amp, Rand& r) {
    grains(b, t0, t1, perSec, [&](Buf& bb, float t, Rand& rr) {
        Biquad bp = Biquad::make(Biquad::BP, rr.range(2500.0f, 9000.0f), 0.8f, (float)bb.rate);
        const size_t a = bb.at(t), n = 90;
        if (a + n > bb.n()) bb.x.resize(a + n, 0.0f);
        const float g = amp * rr.range(0.2f, 1.0f);
        for (size_t i = 0; i < n; ++i) bb.x[a + i] += bp.run(i < 6 ? rr.bi() * 4.0f : 0.0f) * g;
    }, r);
}

Sound spark(int rate, int v) {
    Rand r(3300u + (unsigned)v);
    Buf b(0.8f, rate);
    const float len = r.range(0.08f, 0.45f);
    crackle(b, 0.0f, len, r.range(60.0f, 220.0f), 1.0f, r);
    // Netzbrummen im Lichtbogen
    for (size_t i = 0; i < b.at(len); ++i) {
        const float t = b.t(i);
        b.x[i] += std::tanh(std::sin(kTwoPi * 100.0f * t) * 4.0f) * 0.15f * r.range(0.3f, 1.0f) * envAHR(t, 0.005f, len * 0.6f, len * 0.4f);
    }
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound arcLoop(int rate) {
    Rand r(3350u);
    const float L = 6.0f;
    auto tone = [](float t) { return std::tanh(std::sin(kTwoPi * 100.0f * t) * 3.0f) * 0.12f; };
    auto noise = [&](Buf& nz) { crackle(nz, 0.0f, nz.t(nz.n() - 1), 380.0f, 0.6f, r); };
    Buf b = periodicLoop(rate, L, tone, noise, 0.3f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

// Transformator: grosser Kern, tiefes 100-Hz-Brummen mit kraeftigen Obertoenen, kaum Hoehen
Sound transformer(int rate, int v) {
    Rand r(3400u + (unsigned)v);
    const float L = 10.0f;
    float ph[8];
    for (float& p : ph) p = r.range(0.0f, kTwoPi);
    const float amps[8] = {1.0f, 0.55f, 0.35f, 0.22f, 0.12f, 0.08f, 0.05f, 0.03f};
    auto tone = [&](float t) {
        float x = 0;
        for (int k = 0; k < 8; ++k) x += amps[k] * std::sin(kTwoPi * 100.0f * (float)(k + 1) * t + ph[k]);
        return x * (1.0f + 0.02f * std::sin(kTwoPi * 0.3f * t));
    };
    auto noise = [&](Buf& nz) {  // Luefter des Trafos
        Biquad lp = Biquad::make(Biquad::LP, 600.0f, 0.7f, (float)rate);
        for (size_t i = 0; i < nz.n(); ++i) nz.x[i] = lp.run(r.bi()) * 0.12f;
    };
    Buf b = periodicLoop(rate, L, tone, noise);
    normalize(b, level::kLoop + 2.0f);
    return b.sound(true);
}

// Schuetz faellt / zieht an: Anker schlaegt auf den Rahmen, Spule brummt kurz
std::vector<float> contactorClunk(int rate, Rand& r, float strength) {
    Buf b(0.5f, rate);
    ImpactSpec sp;
    sp.modes = modesMetalPlate(r.range(320.0f, 520.0f), 0.1f, r);
    sp.contact = 0.0012f, sp.noise = 0.5f, sp.noiseHz = 2500.0f;
    b.add(impact(sp, strength, 0.3f, rate, r), 0.0f);
    ImpactSpec body;
    body.modes = modesConcrete(140.0f, r);
    body.contact = 0.003f, body.noise = 0.2f;
    b.add(impact(body, strength * 0.6f, 0.1f, rate, r), 0.0f);
    return b.x;
}

Sound powerOff(int rate) {
    Rand r(3500u);
    Buf b(1.8f, rate);
    b.add(contactorClunk(rate, r, 1.0f), 0.0f);
    // Brummen der Leuchten bricht zusammen (Tonhoehe faellt kaum, Pegel schnell)
    for (size_t i = 0; i < b.at(0.6f); ++i) {
        const float t = b.t(i), w = kTwoPi * 50.0f * t;
        const float x = std::sin(w * 2.0f) + 0.4f * std::sin(w * 4.0f) + 0.3f * std::sin(w * 6.0f);
        b.x[i] += std::tanh(x * 1.5f) * 0.18f * std::exp(-t * 7.0f);
    }
    // tiefer Nachklang im Gebaeude
    Biquad lp = Biquad::make(Biquad::LP, 90.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] += lp.run(r.bi()) * 0.6f * envAD(b.t(i), 0.01f, 0.8f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound powerOn(int rate) {
    Rand r(3510u);
    Buf b(2.6f, rate);
    b.add(contactorClunk(rate, r, 0.9f), 0.0f);
    Biquad lp = Biquad::make(Biquad::LP, 140.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] += (lp.run(r.bi()) * 0.5f + std::sin(kTwoPi * 100.0f * t) * 0.12f) * envAHR(t, 0.08f, 0.3f, 1.2f);
    }
    float at = 0.3f;
    for (int k = 0; k < 5; ++k) {
        ImpactSpec sp;
        sp.modes = modesMetalPlate(r.range(2000.0f, 3000.0f), 0.02f, r);
        sp.contact = 0.0004f, sp.noise = 0.6f, sp.noiseHz = 6000.0f;
        b.add(impact(sp, r.range(0.2f, 0.5f), 0.05f, rate, r), at);
        at += r.range(0.12f, 0.35f);
    }
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

// ferne Schuetze (die Welle des Stromausfalls)
Sound breakerFar(int rate, int v) {
    Rand r(3520u + (unsigned)v);
    Buf b(0.6f, rate);
    b.add(contactorClunk(rate, r, 1.0f), 0.0f);
    distantize(b, 1400.0f, 2.2f, 3521u + (unsigned)v);
    normalize(b, level::kDistant);
    return b.sound();
}

Sound fuseBuzz(int rate) {
    Rand r(3600u);
    const float L = 8.0f;
    auto tone = [](float t) {
        const float w = kTwoPi * 50.0f * t;
        const float x = std::sin(w * 2.0f) + 0.6f * std::sin(w * 4.0f) + 0.4f * std::sin(w * 6.0f) + 0.2f * std::sin(w * 8.0f);
        return std::tanh(x * 2.2f) * 0.5f;
    };
    auto noise = [&](Buf& nz) { crackle(nz, 0.0f, nz.t(nz.n() - 1), 25.0f, 0.5f, r); };
    Buf b = periodicLoop(rate, L, tone, noise, 0.3f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound fuseTake(int rate) {
    Rand r(3650u);
    Buf b(1.2f, rate);
    // Messingklemme federt auf, Porzellan gleitet heraus, Funke, das Brummen bricht ab
    ImpactSpec clamp;
    clamp.modes = modesMetalPlate(1800.0f, 0.12f, r);
    clamp.contact = 0.0008f, clamp.noise = 0.3f;
    b.add(impact(clamp, 1.0f, 0.3f, rate, r), 0.0f);
    Biquad bp = Biquad::make(Biquad::BP, 2600.0f, 2.0f, (float)rate);
    for (size_t i = 0; i < b.at(0.18f); ++i) b.x[b.at(0.06f) + i] += bp.run(r.bi()) * 0.3f * envAHR(b.t(i), 0.03f, 0.08f, 0.07f);
    crackle(b, 0.07f, 0.16f, 160.0f, 0.8f, r);
    ImpactSpec por;
    por.modes = modesCeramic(2200.0f, 0.06f, r);
    por.contact = 0.001f;
    b.add(impact(por, 0.6f, 0.2f, rate, r), 0.3f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

// --- Wasser ------------------------------------------------------------------------------------------
Sound drip(int rate, int v, bool metal) {
    Rand r(3700u + (unsigned)v * 3u + (metal ? 100u : 0u));
    Buf b(0.5f, rate);
    if (metal) {
        ImpactSpec sp;
        sp.modes = modesMetalPipe(r.range(700.0f, 1400.0f), 0.25f, r);
        sp.contact = 0.0004f, sp.noise = 0.2f, sp.noiseHz = 5000.0f;
        b.add(impact(sp, 0.6f, 0.4f, rate, r), 0.0f);
    } else {
        // Tropfen in die Pfuetze: Aufschlag + Blase (Minnaert), manchmal eine zweite kleine
        Biquad hp = Biquad::make(Biquad::HP, 2500.0f, 0.7f, (float)rate);
        for (size_t i = 0; i < 60; ++i) b.x[i] += hp.run(r.bi()) * 0.4f * envAD(b.t(i), 0.0001f, 0.0008f);
        bubble(b, r.range(0.001f, 0.004f), r.range(1.8f, 3.6f), 1.0f, r);
        if (r.chance(0.5f)) bubble(b, r.range(0.03f, 0.08f), r.range(0.8f, 1.5f), 0.3f, r);
    }
    trim(b);
    normalize(b, level::kEvent - 8.0f);
    return b.sound();
}

// Wasser schwappt an Beton (lange, unregelmaessige Schleife)
Sound waterLap(int rate, float L, float energy, unsigned seed) {
    Rand r(seed);
    Buf b(L + 1.0f, rate);
    // Grundrauschen der Oberflaeche
    Biquad lp = Biquad::make(Biquad::LP, 900.0f, 0.6f, (float)rate), hp = Biquad::make(Biquad::HP, 120.0f, 0.6f, (float)rate);
    float swell = 0.5f, sv = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        sv += (r.bi() * 0.00002f - (swell - 0.5f) * 0.0000006f);
        swell = dsp::clampf(swell + sv, 0.15f, 1.0f);
        b.x[i] = hp.run(lp.run(r.bi())) * 0.25f * swell * energy;
    }
    // einzelne Wellen schlagen an: Schwall + Blasen + Plaetschern
    float t = r.range(0.2f, 1.0f);
    while (t < L) {
        const float a = r.range(0.4f, 1.0f) * energy;
        Biquad bp = Biquad::make(Biquad::BP, r.range(500.0f, 1100.0f), 0.7f, (float)rate);
        const float len = r.range(0.25f, 0.6f);
        for (size_t i = 0; i < b.at(len) && b.at(t) + i < b.n(); ++i)
            b.x[b.at(t) + i] += bp.run(r.bi()) * a * 0.6f * envAHR(b.t(i), len * 0.3f, 0.0f, len * 0.7f);
        grains(b, t, t + len, 140.0f * energy, [&](Buf& bb, float tt, Rand& rr) {
            bubble(bb, tt, rr.range(0.6f, 3.0f), rr.range(0.05f, 0.25f) * a, rr);
        }, r);
        t += r.range(1.2f, 3.4f);
    }
    loopify(b, 1.0f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound trickle(int rate) {
    Rand r(3800u);
    const float L = 14.0f;
    Buf b(L + 0.8f, rate);
    grains(b, 0.0f, L + 0.8f, 380.0f, [&](Buf& bb, float t, Rand& rr) {
        bubble(bb, t, rr.range(0.4f, 2.0f), rr.range(0.05f, 0.25f), rr);
    }, r, [](float t) { return 0.55f + 0.45f * std::sin(t * 2.3f) * std::sin(t * 0.7f); });
    loopify(b, 0.8f);
    normalize(b, level::kLoop - 2.0f);
    return b.sound(true);
}

Sound splashSmall(int rate, int v) {
    Rand r(3850u + (unsigned)v);
    Buf b(0.8f, rate);
    Biquad bp = Biquad::make(Biquad::BP, r.range(900.0f, 1600.0f), 0.6f, (float)rate);
    for (size_t i = 0; i < b.at(0.2f); ++i) b.x[i] += bp.run(r.bi()) * envAD(b.t(i), 0.002f, 0.12f);
    grains(b, 0.0f, 0.35f, 300.0f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(0.5f, 2.5f), rr.range(0.05f, 0.3f), rr); },
           r, [](float t) { return 1.0f - t / 0.35f; });
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

Sound swimStroke(int rate, int v) {
    Rand r(3870u + (unsigned)v);
    Buf b(1.0f, rate);
    // Arm zieht durchs Wasser (Schwall), taucht ein (Plaetschern), Blasen
    Biquad lp = Biquad::make(Biquad::LP, 700.0f, 0.6f, (float)rate);
    for (size_t i = 0; i < b.at(0.6f); ++i) b.x[i] += lp.run(r.bi()) * 0.8f * envAHR(b.t(i), 0.2f, 0.1f, 0.3f);
    Biquad bp = Biquad::make(Biquad::BP, 1300.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.15f); ++i) b.x[b.at(0.05f) + i] += bp.run(r.bi()) * 0.5f * envAD(b.t(i), 0.004f, 0.08f);
    grains(b, 0.05f, 0.5f, 220.0f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(0.5f, 3.0f), rr.range(0.03f, 0.15f), rr); }, r);
    trim(b);
    normalize(b, level::kStep);
    return b.sound();
}

Sound underwater(int rate) {
    Rand r(3890u);
    const float L = 16.0f;
    Buf b(L + 1.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 380.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] = lp.run(br.run(r.bi())) * 1.2f;
    grains(b, 0.0f, L, 2.0f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(4.0f, 10.0f), rr.range(0.1f, 0.4f), rr); }, r);
    loopify(b, 1.0f);
    normalize(b, level::kBed + 4.0f);
    b.rate = rate;
    return b.sound(true);
}

// --- Rohre und Maschinen ------------------------------------------------------------------------------
Sound pipeKnock(int rate, int v) {
    Rand r(3900u + (unsigned)v);
    Buf b(2.5f, rate);
    ImpactSpec sp;
    sp.modes = modesMetalPipe(r.range(70.0f, 150.0f), r.range(1.0f, 2.0f), r);
    sp.contact = r.range(0.0012f, 0.002f), sp.noise = 0.25f, sp.noiseHz = 1800.0f;
    b.add(impact(sp, 1.0f, 2.4f, rate, r), 0.0f);
    if (r.chance(0.5f)) b.add(impact(sp, r.range(0.3f, 0.6f), 2.0f, rate, r), r.range(0.12f, 0.3f));  // Waermedehnung: zweiter Schlag
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound pipeGroan(int rate, int v) {
    Rand r(3950u + (unsigned)v);
    Buf b(3.0f, rate);
    CreakSpec c;
    c.rate0 = r.range(10.0f, 18.0f), c.rate1 = r.range(20.0f, 35.0f), c.jitter = 0.35f;
    const float f0 = r.range(90.0f, 140.0f);
    c.formants = {f0 * 2.756f, f0 * 5.404f, f0 * 8.933f}, c.q = 25.0f;
    creak(b, 0.0f, r.range(1.2f, 2.4f), c, 1.0f, r);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound pipeFlow(int rate, int v) {
    Rand r(4000u + (unsigned)v);
    const float L = 17.0f;
    Buf b(L + 0.8f, rate);
    // Turbulenz, durch die Rohrresonanz (Kammfilter) gefaerbt; langsame Druckschwankung
    dsp::DelayLine comb;
    const float f0 = r.range(85.0f, 140.0f);
    comb.init((int)(rate / f0) + 8);
    Biquad bp = Biquad::make(Biquad::BP, r.range(350.0f, 700.0f), 0.5f, (float)rate);
    float y = 0;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float press = 0.7f + 0.3f * std::sin(t * 0.37f + 1.0f) * std::sin(t * 0.13f);
        const float x = bp.run(r.bi()) * press;
        y = x + comb.read((float)rate / f0) * 0.6f;
        comb.push(y);
        b.x[i] = y;
    }
    grains(b, 0.0f, L, 1.5f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(5.0f, 9.0f), rr.range(0.1f, 0.3f), rr); }, r);
    loopify(b, 0.8f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

// zaeh fliessende schwarze Fluessigkeit (Level 3): tiefes Rauschen, grosse, langsame Blasen
Sound sludgeFlow(int rate) {
    Rand r(4050u);
    const float L = 19.0f;
    Buf b(L + 1.0f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 260.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = lp.run(r.bi()) * (0.8f + 0.4f * std::sin(t * 0.8f) * std::sin(t * 0.21f));
    }
    grains(b, 0.0f, L, 3.0f, [&](Buf& bb, float t, Rand& rr) {
        bubble(bb, t, rr.range(8.0f, 16.0f), rr.range(0.3f, 0.7f), rr);
        bubble(bb, t + rr.range(0.03f, 0.1f), rr.range(6.0f, 10.0f), rr.range(0.1f, 0.3f), rr);
    }, r);
    loopify(b, 1.0f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound steamHiss(int rate, int v) {
    Rand r(4100u + (unsigned)v);
    const float len = r.range(0.8f, 1.8f);
    Buf b(len + 0.1f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i), u = t / len;
        f.set(dsp::lerpf(5200.0f, 2600.0f, u), 1.2f, (float)rate);  // Druck faellt: Zischen wird tiefer
        b.x[i] = (f.bp(r.bi()) * 0.8f + f.hp(r.bi()) * 0.2f) * envAHR(t, 0.02f, len * 0.2f, len * 0.75f);
    }
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound steamLoop(int rate) {
    Rand r(4150u);
    const float L = 11.0f;
    Buf b(L + 0.6f, rate);
    dsp::Svf f;
    f.set(3800.0f, 1.0f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = f.bp(r.bi()) * (0.75f + 0.25f * std::sin(t * 2.1f) * std::sin(t * 0.6f));
    }
    loopify(b, 0.6f);
    normalize(b, level::kLoop - 2.0f);
    return b.sound(true);
}

// Luefter: Blattfrequenz (Drehzahl x Blaetter) mit Obertoenen, Turbulenz, Lagergeraeusch
Sound ventFan(int rate, int v) {
    Rand r(4200u + (unsigned)v);
    const float L = 10.0f;
    const float rot = (float)r.irange(15 * (int)L, 25 * (int)L) / L;  // Drehzahl, periodisch in L
    const int blades = r.irange(5, 7);
    const float bpf = rot * (float)blades;
    auto tone = [=](float t) {
        return 0.35f * std::sin(kTwoPi * bpf * t) + 0.15f * std::sin(kTwoPi * 2.0f * bpf * t + 1.0f) + 0.06f * std::sin(kTwoPi * rot * t);
    };
    auto noise = [&](Buf& nz) {
        Biquad lp = Biquad::make(Biquad::LP, 900.0f, 0.6f, (float)rate);
        Biquad bear = Biquad::make(Biquad::BP, r.range(2500.0f, 3500.0f), 8.0f, (float)rate);
        for (size_t i = 0; i < nz.n(); ++i) nz.x[i] = lp.run(r.bi()) * 0.5f + bear.run(r.bi()) * 0.04f;
    };
    Buf b = periodicLoop(rate, L, tone, noise);
    normalize(b, level::kLoop);
    return b.sound(true);
}

// Motor einer halb defekten Maschine: Rotor, Netzbrummen, Lager, gelegentliches Stocken
Sound machineWhir(int rate, int v) {
    Rand r(4250u + (unsigned)v);
    const float L = 15.0f;
    const float rot = (float)r.irange(22 * (int)L, 30 * (int)L) / L;
    auto tone = [=](float t) {
        float x = 0;
        for (int k = 1; k <= 6; ++k) x += std::sin(kTwoPi * rot * (float)k * t + (float)k) / (float)k;
        return x * 0.3f + 0.25f * std::sin(kTwoPi * 100.0f * t);
    };
    auto noise = [&](Buf& nz) {
        Biquad bp = Biquad::make(Biquad::BP, r.range(1500.0f, 2500.0f), 4.0f, (float)rate);
        for (size_t i = 0; i < nz.n(); ++i) nz.x[i] = bp.run(r.bi()) * 0.1f;
        // unregelmaessiges Stocken: zwei, drei dumpfe Schlaege an zufaelligen Stellen
        const int knocks = r.irange(2, 3);
        for (int k = 0; k < knocks; ++k) {
            ImpactSpec sp;
            sp.modes = modesMetalPlate(r.range(150.0f, 250.0f), 0.2f, r);
            sp.contact = 0.002f;
            nz.add(impact(sp, 0.4f, 0.3f, rate, r), r.range(0.5f, L - 1.0f));
        }
    };
    Buf b = periodicLoop(rate, L, tone, noise);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound vendingHum(int rate) {
    Rand r(4300u);
    const float L = 12.0f;
    auto tone = [](float t) {
        const float w = kTwoPi * 50.0f * t;
        return std::tanh((std::sin(w) * 0.4f + std::sin(w * 2.0f) + 0.3f * std::sin(w * 3.0f)) * 1.3f) * 0.6f;
    };
    auto noise = [&](Buf& nz) {
        Biquad lp = Biquad::make(Biquad::LP, 1200.0f, 0.6f, (float)rate);
        for (size_t i = 0; i < nz.n(); ++i) nz.x[i] = lp.run(r.bi()) * 0.18f;
    };
    Buf b = periodicLoop(rate, L, tone, noise);
    normalize(b, level::kLoop);
    return b.sound(true);
}

// --- Tueren, Mechanik, Bauwerk ----------------------------------------------------------------------
Sound doorSlam(int rate, bool steel, int v) {
    Rand r(4400u + (unsigned)v + (steel ? 50u : 0u));
    Buf b(2.0f, rate);
    ImpactSpec sp;
    sp.modes = steel ? modesMetalPlate(r.range(80.0f, 120.0f), 0.7f, r) : modesWood(r.range(85.0f, 120.0f), 0.25f, r);
    sp.contact = steel ? 0.003f : 0.005f, sp.noise = 0.5f, sp.noiseHz = steel ? 1800.0f : 1000.0f, sp.noiseT60 = 0.03f;
    b.add(impact(sp, 1.0f, 1.8f, rate, r), 0.0f);
    ImpactSpec latch;
    latch.modes = modesMetalPlate(r.range(1400.0f, 2200.0f), 0.08f, r);
    latch.contact = 0.0008f;
    b.add(impact(latch, 0.5f, 0.2f, rate, r), r.range(0.03f, 0.07f));
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound doorCreak(int rate, int v) {
    Rand r(4450u + (unsigned)v);
    Buf b(2.4f, rate);
    CreakSpec c;
    c.rate0 = r.range(12.0f, 20.0f), c.rate1 = r.range(30.0f, 55.0f), c.jitter = 0.3f;
    c.formants = {r.range(280.0f, 360.0f), r.range(800.0f, 1000.0f), r.range(1900.0f, 2300.0f)}, c.q = 10.0f;
    creak(b, 0.0f, r.range(1.0f, 1.9f), c, 1.0f, r);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

// Rolltor: Lamellen klappern beim Hochrollen, Rumpeln der Welle, am Ende ein Schlag
Sound rolltor(int rate, int v) {
    Rand r(4500u + (unsigned)v);
    const float len = r.range(2.0f, 3.0f);
    Buf b(len + 1.0f, rate);
    grains(b, 0.0f, len, r.range(22.0f, 34.0f), [&](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesMetalPlate(rr.range(250.0f, 450.0f), 0.12f, rr);
        sp.contact = 0.0015f, sp.noise = 0.3f;
        bb.add(impact(sp, rr.range(0.3f, 0.8f), 0.2f, bb.rate, rr), t);
    }, r);
    Biquad lp = Biquad::make(Biquad::LP, 160.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(len); ++i) b.x[i] += lp.run(r.bi()) * 0.8f * envAHR(b.t(i), 0.2f, len - 0.5f, 0.3f);
    ImpactSpec end;
    end.modes = modesMetalPlate(110.0f, 0.5f, r);
    end.contact = 0.003f;
    b.add(impact(end, 1.0f, 0.8f, rate, r), len);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound knockMetal(int rate, int v) {
    Rand r(4550u + (unsigned)v);
    Buf b(2.0f, rate);
    ImpactSpec sp;
    sp.modes = modesMetalPipe(r.range(110.0f, 220.0f), 0.8f, r);
    sp.contact = 0.004f, sp.noise = 0.3f, sp.noiseHz = 900.0f;  // Knoechel: weich
    float at = 0.0f;
    const int n = r.irange(2, 4);
    for (int k = 0; k < n; ++k) {
        b.add(impact(sp, r.range(0.7f, 1.0f), 1.0f, rate, r), at);
        at += r.range(0.18f, 0.32f);
    }
    trim(b);
    normalize(b, level::kEvent - 1.0f);
    return b.sound();
}

Sound elevDing(int rate) {
    Rand r(4600u);
    Buf b(2.6f, rate);
    ImpactSpec sp;
    sp.modes = modesBell(830.0f, 1.6f, r);
    sp.contact = 0.0012f, sp.noise = 0.05f;
    b.add(impact(sp, 1.0f, 2.5f, rate, r), 0.0f);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound elevDoors(int rate) {
    Rand r(4650u);
    Buf b(2.6f, rate);
    // Tuerantrieb (Motor laeuft an und aus), Rollen in der Schiene, Anschlag
    float ph = 0;
    Biquad lp = Biquad::make(Biquad::LP, 200.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(2.0f); ++i) {
        const float t = b.t(i), e = envAHR(t, 0.4f, 1.0f, 0.6f);
        ph += kTwoPi * (180.0f + 220.0f * e) / (float)rate;
        b.x[i] += (std::sin(ph) * 0.12f + std::sin(ph * 2.0f) * 0.05f) * e + lp.run(r.bi()) * 0.5f * e;
    }
    ImpactSpec stop;
    stop.modes = modesMetalPlate(160.0f, 0.3f, r);
    stop.contact = 0.004f;
    b.add(impact(stop, 0.8f, 0.5f, rate, r), 2.0f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound elevRide(int rate) {
    Rand r(4700u);
    const float L = 8.0f;
    auto tone = [](float t) {
        return 0.3f * std::sin(kTwoPi * 50.0f * t) + 0.2f * std::sin(kTwoPi * 100.0f * t) + 0.08f * std::sin(kTwoPi * 150.0f * t);
    };
    auto noise = [&](Buf& nz) {
        Biquad lp = Biquad::make(Biquad::LP, 400.0f, 0.7f, (float)rate);
        for (size_t i = 0; i < nz.n(); ++i) nz.x[i] = lp.run(r.bi()) * 0.8f;
        grains(nz, 0.0f, nz.t(nz.n() - 1), 3.0f, [&](Buf& bb, float t, Rand& rr) {  // Kabine klappert
            ImpactSpec sp;
            sp.modes = modesMetalPlate(rr.range(300.0f, 600.0f), 0.1f, rr);
            sp.contact = 0.002f;
            bb.add(impact(sp, rr.range(0.1f, 0.3f), 0.15f, bb.rate, rr), t);
        }, r);
    };
    Buf b = periodicLoop(rate, L, tone, noise);
    normalize(b, level::kLoop + 2.0f);
    return b.sound(true);
}

// die letzte Fahrt: der Antrieb laeuft hoch, Wind, ein heller Klang schwillt an (Abspann)
Sound escapeRide(int rate) {
    Rand r(4750u);
    Buf b(11.5f, rate);
    float ph = 0;
    Biquad lp = Biquad::make(Biquad::LP, 600.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i), u = t / 11.5f;
        ph += kTwoPi * (50.0f + 60.0f * u * u) / (float)rate;
        const float motor = std::sin(ph) * 0.3f + std::sin(ph * 2.0f) * 0.15f;
        const float wind = lp.run(r.bi()) * (0.3f + 1.2f * u);
        // heller Klang aus reinen Intervallen, sehr leise beginnend
        const float pad = (std::sin(kTwoPi * 220.0f * t) + 0.6f * std::sin(kTwoPi * 330.0f * t) + 0.4f * std::sin(kTwoPi * 440.0f * t) +
                           0.25f * std::sin(kTwoPi * 554.4f * t)) * 0.08f * u * u * u;
        b.x[i] = (motor + wind) * (1.0f - 0.5f * u * u) + pad;
    }
    fades(b, 0.3f, 1.2f);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound exitDoor(int rate) {
    Rand r(4800u);
    Buf b(1.8f, rate);
    ImpactSpec bar;
    bar.modes = modesMetalPlate(700.0f, 0.15f, r);
    bar.contact = 0.0015f, bar.noise = 0.4f;
    b.add(impact(bar, 1.0f, 0.3f, rate, r), 0.0f);
    Biquad lp = Biquad::make(Biquad::LP, 500.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.8f); ++i) b.x[b.at(0.1f) + i] += lp.run(r.bi()) * 0.5f * envAHR(b.t(i), 0.3f, 0.1f, 0.4f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound carTick(int rate, int v) {
    Rand r(4850u + (unsigned)v);
    Buf b(0.3f, rate);
    ImpactSpec sp;
    sp.modes = modesMetalPlate(r.range(1800.0f, 3800.0f), 0.06f, r);
    sp.contact = 0.0004f, sp.noise = 0.2f;
    b.add(impact(sp, 1.0f, 0.2f, rate, r), 0.0f);
    trim(b);
    normalize(b, level::kEvent - 12.0f);
    return b.sound();
}

// das Gebaeude setzt sich: tiefes Knarren und ein Knacken im Beton
Sound structureCreak(int rate, int v) {
    Rand r(4900u + (unsigned)v);
    Buf b(3.0f, rate);
    CreakSpec c;
    c.rate0 = r.range(6.0f, 10.0f), c.rate1 = r.range(12.0f, 20.0f), c.jitter = 0.4f;
    c.formants = {r.range(120.0f, 180.0f), r.range(350.0f, 480.0f), r.range(850.0f, 1100.0f)}, c.q = 12.0f;
    creak(b, 0.0f, r.range(1.0f, 2.2f), c, 1.0f, r);
    ImpactSpec crack;
    crack.modes = modesConcrete(r.range(300.0f, 500.0f), r);
    crack.contact = 0.0006f, crack.noise = 0.6f;
    b.add(impact(crack, 0.5f, 0.1f, rate, r), r.range(0.3f, 1.5f));
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

Sound quake(int rate) {
    Rand r(4950u);
    Buf b(5.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 70.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] = lp.run(br.run(r.bi())) * 3.0f * envAHR(b.t(i), 0.8f, 1.5f, 2.2f);
    grains(b, 0.5f, 3.5f, 30.0f, [&](Buf& bb, float t, Rand& rr) {  // Rohre klirren in den Halterungen
        ImpactSpec sp;
        sp.modes = modesMetalPipe(rr.range(200.0f, 500.0f), 0.3f, rr);
        sp.contact = 0.0015f;
        bb.add(impact(sp, rr.range(0.05f, 0.25f), 0.4f, bb.rate, rr), t);
    }, r);
    grains(b, 1.0f, 4.5f, 120.0f, [&](Buf& bb, float t, Rand& rr) {  // Staub und Kiesel rieseln
        const size_t a = bb.at(t);
        for (size_t i = 0; i < 30 && a + i < bb.n(); ++i) bb.x[a + i] += rr.bi() * 0.05f * envAD((float)i / (float)bb.rate, 0.0001f, 0.001f);
    }, r);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound debris(int rate, int v) {
    Rand r(5000u + (unsigned)v);
    Buf b(1.4f, rate);
    grains(b, 0.0f, 0.9f, 80.0f, [&](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesConcrete(rr.range(500.0f, 1500.0f), rr);
        sp.contact = 0.0008f, sp.noise = 0.5f;
        bb.add(impact(sp, rr.range(0.1f, 0.6f), 0.06f, bb.rate, rr), t);
    }, r, [](float t) { return 1.0f - t / 0.9f; });
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

// Regen an Scheiben: dichte Tropfen auf Glas, dazu das Prasseln dahinter
Sound rainGlass(int rate) {
    Rand r(5100u);
    const float L = 29.0f;
    Buf b(L + 1.0f, 24000);
    dsp::Pink pink;
    Biquad lp = Biquad::make(Biquad::LP, 1600.0f, 0.6f, 24000.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = lp.run(pink.run(r.bi())) * (0.8f + 0.2f * std::sin(t * 0.21f) * std::sin(t * 0.05f + 2.0f));
    }
    grains(b, 0.0f, L + 1.0f, 140.0f, [&](Buf& bb, float t, Rand& rr) {
        const size_t a = bb.at(t);
        Biquad bp = Biquad::make(Biquad::BP, rr.range(2500.0f, 6000.0f), 6.0f, 24000.0f);
        for (size_t i = 0; i < 160 && a + i < bb.n(); ++i)
            bb.x[a + i] += bp.run(i < 3 ? rr.bi() * 3.0f : 0.0f) * rr.range(0.05f, 0.3f);
    }, r);
    loopify(b, 1.0f);
    normalize(b, level::kBed + 2.0f);
    return b.sound(true);
}

}  // namespace

void addWorldSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 4; ++v) d.push_back({std::format("lamp_hum_{}", v), [=] { return lampHum(rate, 0, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("lamp_ind_{}", v), [=] { return lampHum(rate, 1, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("lamp_emg_{}", v), [=] { return lampHum(rate, 2, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("lamp_sconce_{}", v), [=] { return lampHum(rate, 3, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("lamp_tick_{}", v), [=] { return lampTick(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("lamp_start_{}", v), [=] { return lampStart(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("lamp_pop_{}", v), [=] { return lampPop(rate, v); }});
    for (int v = 0; v < 6; ++v) d.push_back({std::format("spark_{}", v), [=] { return spark(rate, v); }});
    d.push_back({"arc_loop", [=] { return arcLoop(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("transformer_{}", v), [=] { return transformer(rate, v); }});
    d.push_back({"power_off", [=] { return powerOff(rate); }});
    d.push_back({"power_on", [=] { return powerOn(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("breaker_far_{}", v), [=] { return breakerFar(rate, v); }});
    d.push_back({"fuse_buzz", [=] { return fuseBuzz(rate); }});
    d.push_back({"fuse_take", [=] { return fuseTake(rate); }});
    for (int v = 0; v < 6; ++v) d.push_back({std::format("drip_{}", v), [=] { return drip(rate, v, false); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("drip_metal_{}", v), [=] { return drip(rate, v, true); }});
    d.push_back({"water_lap", [=] { return waterLap(rate, 24.0f, 0.7f, 3820u); }});
    d.push_back({"sea_loop", [=] { return waterLap(rate, 37.0f, 1.0f, 3830u); }});
    d.push_back({"pool_loop", [=] { return waterLap(rate, 31.0f, 0.35f, 3840u); }});
    d.push_back({"trickle", [=] { return trickle(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("splash_{}", v), [=] { return splashSmall(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("swim_{}", v), [=] { return swimStroke(rate, v); }});
    d.push_back({"underwater", [=] { return underwater(rate); }});
    for (int v = 0; v < 5; ++v) d.push_back({std::format("pipe_knock_{}", v), [=] { return pipeKnock(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("pipe_groan_{}", v), [=] { return pipeGroan(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("pipe_flow_{}", v), [=] { return pipeFlow(rate, v); }});
    d.push_back({"sludge_flow", [=] { return sludgeFlow(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("steam_hiss_{}", v), [=] { return steamHiss(rate, v); }});
    d.push_back({"steam_loop", [=] { return steamLoop(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("vent_fan_{}", v), [=] { return ventFan(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("machine_{}", v), [=] { return machineWhir(rate, v); }});
    d.push_back({"vending_hum", [=] { return vendingHum(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("door_steel_{}", v), [=] { return doorSlam(rate, true, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("door_wood_{}", v), [=] { return doorSlam(rate, false, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("door_creak_{}", v), [=] { return doorCreak(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("rolltor_{}", v), [=] { return rolltor(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("knock_metal_{}", v), [=] { return knockMetal(rate, v); }});
    d.push_back({"elev_ding", [=] { return elevDing(rate); }});
    d.push_back({"elev_doors", [=] { return elevDoors(rate); }});
    d.push_back({"elev_ride", [=] { return elevRide(rate); }});
    d.push_back({"escape_ride", [=] { return escapeRide(rate); }});
    d.push_back({"exit_door", [=] { return exitDoor(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("car_tick_{}", v), [=] { return carTick(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("structure_creak_{}", v), [=] { return structureCreak(rate, v); }});
    d.push_back({"quake", [=] { return quake(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("debris_{}", v), [=] { return debris(rate, v); }});
    d.push_back({"rain_glass", [=] { return rainGlass(rate); }});
}

}  // namespace lim::audio
