// V13.3: Klaenge der Nguithr'xurh (Entity 16, KREATUR_Nguithrxurh.md). Die Seite beschreibt keine Laute - alles hier ist
// Auslegung nach dem Koerper: Walzenspinnen erzeugen tatsaechlich Laute, indem sie die Cheliceren aneinander reiben.
//
//   Huschen     viele winzige Krallen-Tipper (zwoelf Beine) und das Streichen der Borsten
//   Zirpen      Reiben der Cheliceren: ein trockenes, rasches Raspeln (leise in Ruhe - man hoert es, bevor man sie sieht;
//               laut, wenn die Lampe sie trifft)
//   Schnitt     ein winziges Knipsen, der Faden schnellt
//   Platzen     ein weiches, nasses Ploppen, Spritzen, Zischen des Stoffs
//   Benommen    (Schleife) leises Klingen in den Ohren und gedehnter Atem - die Wirkung des Stoffs
//   Fressen     nasses Schneiden, Knacken
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

void tick(Buf& b, float at, float amp, float hz, Rand& r) {
    const size_t a = b.at(at);
    Biquad bp = Biquad::make(Biquad::BP, hz, 3.0f, (float)b.rate);
    for (size_t k = 0; k < 70 && a + k < b.n(); ++k) b.x[a + k] += bp.run(k < 2 ? r.bi() * 2.5f : 0.0f) * amp;
}

void rasp(Buf& b, float t0, float len, float pps, float amp, Rand& r) {  // Raspeln: Folge kurzer Reibimpulse
    float at = t0;
    while (at < t0 + len) {
        const float u = (at - t0) / len;
        const float env = std::sin(3.14159f * u) * (0.7f + 0.3f * r.uni());
        const size_t a = b.at(at);
        Biquad bp = Biquad::make(Biquad::BP, r.range(3200.0f, 6800.0f), 2.2f, (float)b.rate);
        const int n = r.irange(80, 200);
        for (int k = 0; k < n && a + (size_t)k < b.n(); ++k) b.x[a + (size_t)k] += bp.run(r.bi()) * amp * env * std::exp(-(float)k / (float)n * 3.0f);
        at += 1.0f / pps * r.range(0.8f, 1.2f);
    }
}

Sound nguiSkitter(int rate, int v) {
    Rand r(16000u + (unsigned)v);
    const float L = r.range(0.28f, 0.45f);
    Buf b(L + 0.15f, rate);
    grains(b, 0.0f, L, 110.0f, [&](Buf& bb, float t, Rand& rr) { tick(bb, t, rr.range(0.3f, 1.0f), rr.range(2500.0f, 5500.0f), rr); }, r,
           [=](float t) { return std::sin(3.14159f * std::clamp(t / L, 0.0f, 1.0f)); });
    // Borsten streichen ueber die Flaeche
    Biquad hp = Biquad::make(Biquad::HP, 6000.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(L); ++i) b.x[i] += hp.run(r.bi()) * 0.06f * std::sin(3.14159f * b.t(i) / L);
    trim(b);
    normalize(b, level::kFoley - 4.0f);
    return b.sound();
}

Sound nguiChitter(int rate, int v) {
    Rand r(16100u + (unsigned)v);
    Buf b(1.4f, rate);
    const float L = v == 0 ? r.range(0.35f, 0.5f) : r.range(0.7f, 1.0f);
    rasp(b, 0.0f, L, v == 0 ? 28.0f : 42.0f, 1.0f, r);
    if (v == 1) rasp(b, L + 0.12f, L * 0.6f, 50.0f, 0.8f, r);
    trim(b);
    normalize(b, v == 0 ? level::kFoley : level::kCreature - 3.0f);
    return b.sound();
}

Sound nguiSnip(int rate) {
    Rand r(16200u);
    Buf b(0.5f, rate);
    ImpactSpec sp;
    sp.modes = modesPlastic(r.range(3800.0f, 5200.0f), 0.01f, r);
    sp.contact = 0.0002f, sp.noise = 0.6f, sp.noiseHz = 7000.0f, sp.noiseT60 = 0.002f;
    b.add(impact(sp, 1.0f, 0.05f, rate, r), 0.0f);
    // der Faden schnellt: ein kurzes, hohes Sirren
    for (size_t i = 0; i < b.at(0.12f); ++i) {
        const float t = b.t(i);
        b.x[i] += std::sin(kTwoPi * (2200.0f + 3000.0f * t) * t) * 0.08f * std::exp(-t * 30.0f);
    }
    trim(b);
    normalize(b, level::kFoley - 2.0f);
    return b.sound();
}

Sound nguiBurst(int rate, int v) {
    Rand r(16300u + (unsigned)v);
    Buf b(1.4f, rate);
    bubble(b, 0.0f, r.range(5.0f, 8.0f), 0.9f, r);  // weiches Ploppen (grosse Blase)
    ImpactSpec sp;
    sp.modes = modesConcrete(r.range(110.0f, 160.0f), r);
    sp.contact = 0.02f, sp.noise = 0.7f, sp.noiseHz = 900.0f, sp.noiseT60 = 0.05f;
    b.add(impact(sp, 0.6f, 0.25f, rate, r), 0.005f);
    grains(b, 0.01f, 0.35f, 60.0f, [&](Buf& bb, float t, Rand& rr) { bubble(bb, t, rr.range(0.6f, 1.8f), rr.range(0.1f, 0.35f), rr); }, r,
           [](float t) { return std::exp(-t * 7.0f); });
    Biquad bp = Biquad::make(Biquad::BP, 5000.0f, 0.8f, (float)rate);  // Zischen des Stoffs
    for (size_t i = b.at(0.02f); i < b.at(1.2f); ++i) b.x[i] += bp.run(r.bi()) * 0.12f * std::exp(-(b.t(i) - 0.02f) * 3.5f);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound nguiDaze(int rate) {  // Schleife: Klingen in den Ohren, gedehnter Atem
    Rand r(16400u);
    const float L = 9.0f;
    Buf b(L + 1.0f, rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] += (std::sin(kTwoPi * 6120.0f * t) * 0.6f + std::sin(kTwoPi * 6187.0f * t) * 0.4f) * 0.03f * (0.7f + 0.3f * std::sin(t * 0.8f));
    }
    for (float at = 0.5f; at < L - 1.0f; at += r.range(3.6f, 4.4f)) {
        breathe(b, at, r.range(1.5f, 1.9f), true, 0.1f, 0.05f, r);
        breathe(b, at + 1.9f, r.range(1.6f, 2.0f), false, 0.08f, 0.03f, r);
    }
    filter(b, Biquad::LP, 7000.0f, 0.7f);
    loopify(b, 1.0f);
    normalize(b, level::kLoop - 2.0f);
    return b.sound(true);
}

Sound nguiFeed(int rate) {
    Rand r(16500u);
    Buf b(1.8f, rate);
    for (float at = 0.0f; at < 1.4f; at += r.range(0.06f, 0.12f)) {
        ImpactSpec sp;
        sp.modes = modesWood(r.range(900.0f, 1600.0f), 0.02f, r);
        sp.contact = 0.001f, sp.noise = 0.7f, sp.noiseHz = 3500.0f, sp.noiseT60 = 0.01f;
        b.add(impact(sp, r.range(0.4f, 1.0f), 0.08f, rate, r), at);
        if (r.chance(0.5f)) bubble(b, at + 0.01f, r.range(0.8f, 1.6f), 0.25f, r);
    }
    rasp(b, 0.2f, 1.0f, 30.0f, 0.5f, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

}  // namespace

void addNguithrSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 3; ++v) d.push_back({std::format("ngui_skitter_{}", v), [=] { return nguiSkitter(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ngui_chitter_{}", v), [=] { return nguiChitter(rate, v); }});
    d.push_back({"ngui_snip", [=] { return nguiSnip(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("ngui_burst_{}", v), [=] { return nguiBurst(rate, v); }});
    d.push_back({"ngui_daze", [=] { return nguiDaze(rate); }});
    d.push_back({"ngui_feed", [=] { return nguiFeed(rate); }});
}

}  // namespace lim::audio
