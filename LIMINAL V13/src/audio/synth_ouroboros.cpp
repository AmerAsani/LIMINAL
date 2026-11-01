// V13.3: Klaenge der Ouroboros-Arbeiter (KREATUR_OuroborosArbeiter.md). Die Level-1-Seite nennt nur Arbeiter und Maschinen; die
// Remodeling-Co-Seite sagt, dass sie ohne Mund sprechen und die Untersten nur kurze Saetze koennen. Alles hier ist Auslegung:
//
//   Schritte     schwere Gummistiefel auf Beton, Splitt knirscht
//   Werkzeug     Hammer auf Ziegel (hart, kurz), Stein setzt ins Moertelbett (dumpf, nass), Kelle (Klopfen, Schaben),
//                Brechstange (Mauerwerk knirscht und mahlt), Stein bricht (Knacken, Broeckeln)
//   Schweissen   Zuenden (Plopp, Zischen steigt - Ankuendigung), Lichtbogen (Brummen, Knistern wie heisses Fett)
//   Presslufthammer  Stoesse mit 22 Hz: Metall auf Meissel, Beton, Abluft
//   Warnklang    drei Pieptoene; der Kompressor laeuft an (Ankuendigung des Presslufthammers); die Warnleuchte des Geruests
//                piept schneller und hoeher
//   Geruest      knirscht und aechzt (2,8 s), Aufschlag: Rohre scheppern, Bretter, ein Rohr rollt nach; eine Mauerlage schlaegt auf
//   Stimmen      dumpf, ohne Worte - wie durch Stoff oder eine Wand; der Tonfall eines kurzen Satzes (Entschuldigung?)
//   Schub        kalter, dumpfer Luftzug, wenn einer durch einen hindurchgeht; Vergehen: langes, trockenes Zischen
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

void noiseBurst(Buf& b, float at, float len, float hz, float q, float amp, float decay, Rand& r, Biquad::Type t = Biquad::BP) {
    Biquad f = Biquad::make(t, hz, q, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) b.x[a + k] += f.run(r.bi()) * amp * std::exp(-(float)k / (float)b.rate * decay);
}

void hit(Buf& b, float at, const std::vector<dsp::Mode>& modes, float contact, float noise, float noiseHz, float strength, float len, Rand& r) {
    ImpactSpec sp;
    sp.modes = modes;
    sp.contact = contact, sp.noise = noise, sp.noiseHz = noiseHz, sp.noiseT60 = 0.02f;
    b.add(impact(sp, strength, len, b.rate, r), at);
}

Sound obStep(int rate, int v) {
    Rand r(17000u + (unsigned)v);
    Buf b(0.5f, rate);
    hit(b, 0.0f, modesConcrete(r.range(95.0f, 130.0f), r), 0.012f, 0.4f, 600.0f, 1.0f, 0.3f, r);  // schwerer, weicher Absatz
    hit(b, 0.05f, modesConcrete(r.range(140.0f, 190.0f), r), 0.008f, 0.3f, 900.0f, 0.6f, 0.25f, r);
    grains(b, 0.01f, 0.18f, 90.0f, [](Buf& bb, float t, Rand& rr) { noiseBurst(bb, t, 0.004f, rr.range(3000.0f, 7000.0f), 3.0f, rr.range(0.05f, 0.18f), 400.0f, rr); }, r,
           [](float t) { return std::exp(-t * 14.0f); });
    // Gummi quietscht kaum hoerbar beim Abrollen
    for (size_t i = b.at(0.08f); i < b.at(0.16f); ++i) b.x[i] += std::sin(kTwoPi * (900.0f + 300.0f * b.t(i)) * b.t(i)) * 0.012f * std::sin(3.14159f * (b.t(i) - 0.08f) / 0.08f);
    filter(b, Biquad::LP, 5200.0f, 0.7f);
    trim(b);
    normalize(b, level::kStep + 1.0f);
    return b.sound();
}

Sound obTap(int rate, int v) {  // Hammer auf Ziegel
    Rand r(17100u + (unsigned)v);
    Buf b(0.6f, rate);
    hit(b, 0.0f, modesCeramic(r.range(1500.0f, 2100.0f), 0.05f, r), 0.0007f, 0.6f, 4200.0f, 1.0f, 0.3f, r);
    hit(b, 0.0f, modesMetalPipe(r.range(2600.0f, 3400.0f), 0.08f, r), 0.0005f, 0.1f, 6000.0f, 0.25f, 0.3f, r);
    hit(b, 0.002f, modesConcrete(r.range(220.0f, 300.0f), r), 0.003f, 0.3f, 1200.0f, 0.5f, 0.2f, r);
    trim(b);
    normalize(b, level::kEvent - 1.0f);
    return b.sound();
}

Sound obSet(int rate, int v) {  // Stein ins Moertelbett
    Rand r(17200u + (unsigned)v);
    Buf b(0.8f, rate);
    hit(b, 0.0f, modesConcrete(r.range(80.0f, 110.0f), r), 0.02f, 0.5f, 500.0f, 1.0f, 0.35f, r);
    noiseBurst(b, 0.005f, 0.25f, 700.0f, 0.8f, 0.25f, 14.0f, r, Biquad::LP);  // nass
    for (int k = 0; k < 4; ++k) bubble(b, 0.02f + 0.03f * (float)k, r.range(2.0f, 4.0f), 0.08f, r);
    noiseBurst(b, 0.12f, 0.2f, 2500.0f, 1.2f, 0.06f, 8.0f, r);  // ruckelt nach
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound obTrowel(int rate, int v) {  // Kelle: zwei Klopfer mit dem Griff, Schaben
    Rand r(17300u + (unsigned)v);
    Buf b(0.9f, rate);
    for (int k = 0; k < 2; ++k) hit(b, 0.0f + 0.22f * (float)k, modesWood(r.range(600.0f, 800.0f), 0.03f, r), 0.002f, 0.4f, 2000.0f, 0.8f, 0.2f, r);
    Biquad bp = Biquad::make(Biquad::BP, 3200.0f, 1.5f, (float)rate);
    for (size_t i = b.at(0.45f); i < b.at(0.8f); ++i) {
        const float t = (b.t(i) - 0.45f) / 0.35f;
        b.x[i] += bp.run(r.bi()) * 0.18f * std::sin(3.14159f * t) * (0.7f + 0.3f * std::sin(t * 60.0f));
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound obPry(int rate) {  // Brechstange: Mauerwerk knirscht, mahlt, die Lage loest sich
    Rand r(17400u);
    Buf b(2.4f, rate);
    CreakSpec c;
    c.rate0 = 18.0f, c.rate1 = 45.0f, c.jitter = 0.5f, c.formants = {220.0f, 640.0f, 1500.0f}, c.q = 5.0f;
    creak(b, 0.1f, 1.6f, c, 0.7f, r);
    grains(b, 0.3f, 2.0f, 60.0f, [](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesConcrete(rr.range(300.0f, 900.0f), rr);
        sp.contact = 0.001f, sp.noise = 0.7f, sp.noiseHz = 3000.0f, sp.noiseT60 = 0.01f;
        bb.add(impact(sp, rr.range(0.1f, 0.4f), 0.06f, bb.rate, rr), t);
    }, r, [](float t) { return 0.3f + t; });
    hit(b, 0.05f, modesMetalPipe(r.range(700.0f, 900.0f), 0.3f, r), 0.001f, 0.3f, 4000.0f, 0.6f, 0.6f, r);  // Stange setzt an
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound obBreak(int rate, int v) {  // ein Stein / eine Lage bricht
    Rand r(17500u + (unsigned)v);
    Buf b(1.6f, rate);
    hit(b, 0.0f, modesCeramic(r.range(900.0f, 1300.0f), 0.04f, r), 0.001f, 0.8f, 3500.0f, 1.0f, 0.25f, r);
    hit(b, 0.0f, modesConcrete(r.range(120.0f, 170.0f), r), 0.01f, 0.5f, 800.0f, 0.9f, 0.3f, r);
    grains(b, 0.05f, 1.2f, 70.0f, [](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesConcrete(rr.range(250.0f, 1200.0f), rr);
        sp.contact = 0.0015f, sp.noise = 0.6f, sp.noiseHz = 2500.0f, sp.noiseT60 = 0.01f;
        bb.add(impact(sp, rr.range(0.1f, 0.6f), 0.08f, bb.rate, rr), t);
    }, r, [](float t) { return std::exp(-t * 3.0f); });
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound obWeld(int rate, int v) {  // Lichtbogen: Brummen (100 Hz) und Knistern
    Rand r(17600u + (unsigned)v);
    const float L = 1.2f;
    Buf b(L + 0.1f, rate);
    for (size_t i = 0; i < b.at(L); ++i) {
        const float t = b.t(i);
        float s = 0.0f;
        for (int h = 1; h <= 7; h += 2) s += std::sin(kTwoPi * 100.0f * (float)h * t) / (float)h;
        b.x[i] += s * 0.05f * (0.7f + 0.3f * std::sin(t * 23.0f + (float)v));
    }
    grains(b, 0.0f, L, 260.0f, [](Buf& bb, float t, Rand& rr) { noiseBurst(bb, t, 0.003f, rr.range(1800.0f, 7000.0f), 2.5f, rr.range(0.08f, 0.5f), 900.0f, rr); }, r,
           [](float) { return 1.0f; });
    noiseBurst(b, 0.0f, L, 5200.0f, 0.6f, 0.06f, 0.0f, r);  // Zischen des Gases
    fades(b, 0.04f, 0.06f);
    normalize(b, level::kFoley + 2.0f);
    return b.sound();
}

Sound obIgnite(int rate) {  // Zuenden: Plopp, das Zischen steigt (Ankuendigung der Funken)
    Rand r(17700u);
    Buf b(1.3f, rate);
    hit(b, 0.0f, modesConcrete(r.range(70.0f, 90.0f), r), 0.015f, 0.8f, 300.0f, 0.8f, 0.3f, r);
    noiseBurst(b, 0.0f, 0.12f, 400.0f, 0.7f, 0.5f, 25.0f, r, Biquad::LP);
    Biquad bp = Biquad::make(Biquad::BP, 3000.0f, 1.0f, (float)rate);
    for (size_t i = b.at(0.08f); i < b.at(1.2f); ++i) {
        const float t = b.t(i) - 0.08f;
        bp.set(Biquad::BP, 2000.0f + 3500.0f * std::min(1.0f, t / 1.0f), 1.0f, (float)rate);
        b.x[i] += bp.run(r.bi()) * 0.3f * std::min(1.0f, t * 3.0f);
    }
    fades(b, 0.0f, 0.08f);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound obJack(int rate, int v) {  // Presslufthammer, ein Stoss (0,6 s)
    Rand r(17800u + (unsigned)v);
    const float L = 0.62f;
    Buf b(L + 0.2f, rate);
    for (float at = 0.0f; at < L; at += 1.0f / 22.0f * r.range(0.95f, 1.05f)) {
        hit(b, at, modesMetalPipe(r.range(380.0f, 460.0f), 0.05f, r), 0.0006f, 0.4f, 3000.0f, r.range(0.7f, 1.0f), 0.08f, r);
        hit(b, at + 0.002f, modesConcrete(r.range(140.0f, 220.0f), r), 0.002f, 0.6f, 1500.0f, r.range(0.6f, 1.0f), 0.06f, r);
    }
    Biquad bp = Biquad::make(Biquad::BP, 1800.0f, 0.7f, (float)rate);  // Abluft, im Takt
    for (size_t i = 0; i < b.at(L); ++i) b.x[i] += bp.run(r.bi()) * 0.22f * (0.6f + 0.4f * std::sin(kTwoPi * 22.0f * b.t(i)));
    grains(b, 0.05f, L, 40.0f, [](Buf& bb, float t, Rand& rr) { noiseBurst(bb, t, 0.01f, rr.range(1500.0f, 4000.0f), 2.0f, rr.range(0.1f, 0.3f), 200.0f, rr); }, r,
           [](float) { return 1.0f; });
    fades(b, 0.005f, 0.05f);
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}

Sound obWarn(int rate, int v) {  // v0: drei Pieptoene; v1: Kompressor laeuft an; v2: Warnleuchte des Geruests (schnell, hoch)
    Rand r(17900u + (unsigned)v);
    Buf b(v == 1 ? 2.0f : 1.6f, rate);
    if (v == 1) {
        for (size_t i = 0; i < b.at(1.8f); ++i) {
            const float t = b.t(i), f = 18.0f + 32.0f * std::min(1.0f, t / 1.2f);
            float s = std::sin(kTwoPi * f * t) * 0.5f + std::sin(kTwoPi * f * 2.0f * t) * 0.25f + std::sin(kTwoPi * f * 3.0f * t) * 0.12f;
            b.x[i] += s * std::min(1.0f, t * 4.0f) * 0.6f;
        }
        noiseBurst(b, 0.0f, 1.8f, 600.0f, 0.8f, 0.12f, 0.3f, r);
        hit(b, 0.0f, modesMetalPlate(r.range(150.0f, 200.0f), 0.3f, r), 0.003f, 0.3f, 1500.0f, 0.5f, 0.5f, r);
        filter(b, Biquad::LP, 2500.0f, 0.7f);
    } else {
        const int n = v == 0 ? 3 : 6;
        const float hz = v == 0 ? 1180.0f : 1650.0f, len = v == 0 ? 0.22f : 0.1f, gap = v == 0 ? 0.42f : 0.2f;
        for (int k = 0; k < n; ++k)
            for (size_t i = b.at(gap * (float)k); i < b.at(gap * (float)k + len) && i < b.n(); ++i) {
                const float t = b.t(i) - gap * (float)k;
                const float sq = std::sin(kTwoPi * hz * t) > 0.0f ? 1.0f : -1.0f;
                b.x[i] += (sq * 0.25f + std::sin(kTwoPi * hz * t) * 0.6f) * std::min(1.0f, std::min(t, len - t) * 200.0f) * 0.4f;
            }
        filter(b, Biquad::LP, 6000.0f, 0.7f);
    }
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound obCreak(int rate, int v) {  // das Geruest knirscht und aechzt
    Rand r(18000u + (unsigned)v);
    Buf b(3.0f, rate);
    CreakSpec c;
    c.rate0 = 8.0f, c.rate1 = 30.0f, c.jitter = 0.45f, c.formants = {480.0f, 1300.0f, 3100.0f}, c.q = 14.0f;
    creak(b, 0.0f, 1.2f, c, 0.6f, r);
    c.rate0 = 25.0f, c.rate1 = 12.0f, c.formants = {350.0f, 900.0f, 2400.0f};
    creak(b, 1.1f, 1.5f, c, 0.8f, r);
    for (int k = 0; k < 5; ++k)  // Kupplungen klappern
        hit(b, r.range(0.3f, 2.6f), modesMetalPipe(r.range(600.0f, 1100.0f), 0.2f, r), 0.0008f, 0.2f, 4000.0f, r.range(0.2f, 0.5f), 0.4f, r);
    // tiefes Aechzen des Rahmens
    for (size_t i = b.at(1.4f); i < b.at(2.9f); ++i) {
        const float t = b.t(i) - 1.4f;
        b.x[i] += std::sin(kTwoPi * (55.0f - 8.0f * t) * t) * 0.12f * std::sin(3.14159f * t / 1.5f);
    }
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound obCrash(int rate, int v) {  // v0: das Geruest schlaegt auf; v1: eine Mauerlage schlaegt auf
    Rand r(18100u + (unsigned)v);
    Buf b(v == 0 ? 3.2f : 1.6f, rate);
    if (v == 0) {
        for (int k = 0; k < 14; ++k)
            hit(b, r.range(0.0f, 0.12f) + (k > 9 ? r.range(0.2f, 0.6f) : 0.0f), modesMetalPipe(r.range(250.0f, 900.0f), r.range(0.4f, 1.2f), r), 0.001f, 0.5f,
                3500.0f, r.range(0.4f, 1.0f), 1.6f, r);
        hit(b, 0.01f, modesWood(r.range(120.0f, 180.0f), 0.1f, r), 0.006f, 0.6f, 900.0f, 1.0f, 0.5f, r);
        hit(b, 0.0f, modesConcrete(r.range(60.0f, 80.0f), r), 0.03f, 0.6f, 300.0f, 1.0f, 0.4f, r);
        for (float at = 0.9f; at < 2.8f; at += r.range(0.12f, 0.25f))  // ein Rohr rollt nach
            hit(b, at, modesMetalPipe(r.range(500.0f, 560.0f), 0.3f, r), 0.002f, 0.2f, 2500.0f, 0.15f * (2.8f - at), 0.4f, r);
    } else {
        hit(b, 0.0f, modesConcrete(r.range(70.0f, 95.0f), r), 0.025f, 0.7f, 500.0f, 1.0f, 0.4f, r);
        for (int k = 0; k < 6; ++k) hit(b, r.range(0.0f, 0.06f), modesCeramic(r.range(700.0f, 1400.0f), 0.04f, r), 0.001f, 0.7f, 3000.0f, r.range(0.4f, 0.9f), 0.2f, r);
        grains(b, 0.05f, 1.2f, 50.0f, [](Buf& bb, float t, Rand& rr) { noiseBurst(bb, t, 0.02f, rr.range(1000.0f, 3000.0f), 1.5f, rr.range(0.05f, 0.2f), 150.0f, rr); },
               r, [](float t) { return std::exp(-t * 3.0f); });
    }
    trim(b);
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}

Sound obMurmur(int rate, int v) {  // dumpfe Stimme ohne Worte: Tonfall eines kurzen Satzes, wie durch Stoff
    Rand r(18200u + (unsigned)v);
    const int syl = r.irange(4, 7);
    float at = 0.05f;
    Buf b(2.6f, rate);
    const float f0 = r.range(88.0f, 118.0f);
    for (int k = 0; k < syl; ++k) {
        const float len = r.range(0.11f, 0.22f) * (k == syl - 1 ? 1.6f : 1.0f);
        VoiceSpec vs;
        const float pitch = f0 * (1.0f + 0.12f * std::sin((float)k * 1.3f + (float)v) - 0.1f * (float)k / (float)syl);
        vs.f0 = [=](float t) { return pitch * (1.0f - 0.08f * t / len); };
        vs.amp = [=](float t) { return std::sin(3.14159f * std::clamp(t / len, 0.0f, 1.0f)) * 0.9f; };
        const float o = r.range(0.0f, 1.0f);  // ein Vokal ohne Mund: nur zwischen "o" und "u"
        vs.formants = {380.0f + 120.0f * o, 800.0f + 250.0f * o, 2300.0f};
        vs.bandwidths = {120.0f, 180.0f, 300.0f};
        vs.breath = 0.35f, vs.rough = 0.15f, vs.jitter = 0.03f;
        voice(b, at, len, vs, r);
        at += len + r.range(0.02f, 0.09f);
    }
    filter(b, Biquad::LP, 520.0f, 0.8f);  // durch Stoff
    filter(b, Biquad::LP, 900.0f, 0.6f);
    filter(b, Biquad::HP, 70.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound obShove(int rate) {  // kalter, dumpfer Luftzug, der durch einen hindurchgeht
    Rand r(18300u);
    Buf b(1.4f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 300.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.at(1.2f); ++i) {
        const float t = b.t(i);
        lp.set(Biquad::LP, 180.0f + 900.0f * std::sin(3.14159f * std::min(1.0f, t / 1.2f)), 0.8f, (float)rate);
        b.x[i] += lp.run(r.bi()) * 0.9f * std::sin(3.14159f * std::min(1.0f, t / 1.2f));
        b.x[i] += std::sin(kTwoPi * 38.0f * t) * 0.15f * std::exp(-t * 4.0f);
    }
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound obDissolve(int rate) {  // trockenes, langes Zischen, darunter ein Brummen, das verklingt
    Rand r(18400u);
    Buf b(3.0f, rate);
    Biquad bp = Biquad::make(Biquad::BP, 2500.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.at(2.8f); ++i) {
        const float t = b.t(i);
        b.x[i] += bp.run(r.bi()) * 0.4f * std::min(1.0f, t * 3.0f) * std::exp(-t * 0.9f);
        b.x[i] += std::sin(kTwoPi * (62.0f - 10.0f * t) * t) * 0.18f * std::exp(-t * 1.2f);
    }
    grains(b, 0.0f, 2.2f, 40.0f, [](Buf& bb, float t, Rand& rr) { noiseBurst(bb, t, 0.006f, rr.range(2000.0f, 6000.0f), 2.0f, rr.range(0.05f, 0.2f), 500.0f, rr); }, r,
           [](float t) { return std::exp(-t); });
    fades(b, 0.02f, 0.4f);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

}  // namespace

void addOuroborosSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 3; ++v) d.push_back({std::format("ouro_step_{}", v), [=] { return obStep(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("ouro_tap_{}", v), [=] { return obTap(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_set_{}", v), [=] { return obSet(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_trowel_{}", v), [=] { return obTrowel(rate, v); }});
    d.push_back({"ouro_pry", [=] { return obPry(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_break_{}", v), [=] { return obBreak(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_weld_{}", v), [=] { return obWeld(rate, v); }});
    d.push_back({"ouro_ignite", [=] { return obIgnite(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_jack_{}", v), [=] { return obJack(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("ouro_warn_{}", v), [=] { return obWarn(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_creak_{}", v), [=] { return obCreak(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ouro_crash_{}", v), [=] { return obCrash(rate, v); }});
    for (int v = 0; v < 6; ++v) d.push_back({std::format("ouro_murmur_{}", v), [=] { return obMurmur(rate, v); }});
    d.push_back({"ouro_shove", [=] { return obShove(rate); }});
    d.push_back({"ouro_dissolve", [=] { return obDissolve(rate); }});
}

}  // namespace lim::audio
