// V13.3 Teil 4: Klaenge der Level-2-Auspraegungen (KREATUR_Fensterwesen.md, KREATUR_Deathmoth.md, KREATUR_DeathRat.md).
// Die Wikidot-Seiten beschreiben diese Laute nicht - alles hier ist Auslegung nach Koerper und Ort:
//
//   window_form     das Fenster bildet sich in der Betonwand: Glas setzt sich (heller Ton), der Stahlrahmen knackt, ein
//                   Zug von Aussenluft mit fernen Voegeln - hinter Glas, gedaempft (Ankuendigung, Geraeusch vor Sicht)
//   window_air_0..2 Aussenluft hinter Glas: Wind, ein paar Vogelrufe; leise (in der Naehe des Fensters, immer wieder)
//   moth_tap_0..2   eine kleine Motte stoesst an die Leuchtstoffroehre: winziges Klicken auf Glas, kurzes Schwirren
//   moth_buzz       gereizte Motten um den Kopf: hartes, pulsierendes Fluegelrasseln, das anschwillt (Ankuendigung)
//   moth_bite       das Weibchen beisst: trockenes Chitin-Knipsen, ein feuchtes Reissen
//   rat_lap_0..2    eine Ratte schleckt Fluessigkeit (am Spundloch, an der Lache): kleine nasse Zungenschlaege
//   rat_gnaw_0..2   eine Ratte nagt an Essensresten: schnelles Raspeln der Schneidezaehne, Knacken
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

// Vogelruf: kurze, schnelle Frequenzgleiter (Amsel/Spatz, fern) - Summe von Sinus mit Huellkurve
void chirp(Buf& b, float at, float len, float f0, float f1, float amp, Rand& r) {
    const size_t a = b.at(at), n = b.at(len);
    float ph = 0.0f;
    const float trill = r.range(18.0f, 34.0f), depth = r.range(0.0f, 0.12f);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float u = (float)k / (float)n;
        const float f = f0 + (f1 - f0) * u * u + f0 * depth * std::sin(kTwoPi * trill * (float)k / (float)b.rate);
        ph += kTwoPi * f / (float)b.rate;
        const float env = std::sin(3.14159f * u) * std::sin(3.14159f * u);
        b.x[a + k] += (std::sin(ph) + 0.25f * std::sin(2.0f * ph)) * env * amp;
    }
}

// Wind: Rauschen durch ein langsam wanderndes Bandpassfilter
void wind(Buf& b, float t0, float len, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, 500.0f, 0.8f, (float)b.rate);
    const float sp = r.range(0.25f, 0.5f), ph = r.range(0.0f, 6.0f);
    const size_t a = b.at(t0), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float t = (float)k / (float)b.rate, u = (float)k / (float)n;
        if ((k & 63) == 0) bp = Biquad::make(Biquad::BP, 380.0f + 420.0f * (0.5f + 0.5f * std::sin(kTwoPi * sp * t + ph)), 0.9f, (float)b.rate);
        const float env = std::sin(3.14159f * u) * (0.7f + 0.3f * std::sin(kTwoPi * 0.9f * t + ph));
        b.x[a + k] += bp.run(r.bi()) * env * amp;
    }
}

Sound windowForm(int rate) {
    Rand r(24000u);
    Buf b(3.4f, rate);
    // Glas setzt sich: ein heller, weicher Ton
    ImpactSpec gl;
    gl.modes = modesGlass(r.range(1900.0f, 2400.0f), 1.6f, r);
    gl.contact = 0.004f, gl.noise = 0.1f, gl.noiseHz = 5000.0f;
    b.add(impact(gl, 0.6f, 2.2f, rate, r), 0.05f, 0.7f);
    // der Stahlrahmen knackt im Beton
    CreakSpec ck;
    ck.rate0 = 18.0f, ck.rate1 = 9.0f, ck.formants = {260.0f, 780.0f, 1900.0f}, ck.q = 10.0f;
    creak(b, 0.12f, 0.45f, ck, 0.5f, r);
    ImpactSpec st;
    st.modes = modesMetalPlate(r.range(300.0f, 420.0f), 0.25f, r);
    st.contact = 0.002f, st.noise = 0.35f, st.noiseHz = 1800.0f;
    b.add(impact(st, 0.7f, 0.6f, rate, r), 0.6f, 0.6f);
    // ein Zug von Aussenluft, darin ferne Voegel
    Buf air(3.4f, rate);
    wind(air, 0.5f, 2.8f, 0.5f, r);
    for (int i = 0; i < 4; ++i) {
        const float f = r.range(2600.0f, 4200.0f);
        chirp(air, r.range(0.9f, 2.6f), r.range(0.07f, 0.16f), f, f * r.range(0.75f, 1.3f), 0.18f, r);
    }
    filter(air, Biquad::LP, 2400.0f, 0.7f);  // hinter Glas
    b.add(air, 0.0f, 0.9f);
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

Sound windowAir(int rate, int v) {
    Rand r(24100u + (unsigned)v);
    const float L = r.range(2.4f, 3.2f);
    Buf b(L + 0.2f, rate);
    wind(b, 0.0f, L, 0.6f, r);
    const int birds = 2 + v;
    for (int i = 0; i < birds; ++i) {
        const float f = r.range(2400.0f, 4500.0f), at = r.range(0.3f, L - 0.4f);
        for (int k = 0, nk = r.irange(1, 3); k < nk; ++k)  // ein Ruf aus ein bis drei Silben
            chirp(b, at + (float)k * r.range(0.09f, 0.14f), r.range(0.05f, 0.11f), f, f * r.range(0.7f, 1.35f), 0.22f, r);
    }
    filter(b, Biquad::LP, 2200.0f, 0.7f);
    fades(b, 0.4f, 0.6f);
    normalize(b, level::kDistant - 3.0f);
    return b.sound();
}

// Fluegelschlag einer kleinen Motte: Rauschstoesse mit der Schlagfrequenz
void flutter(Buf& b, float t0, float len, float hz, float amp, Rand& r, float bpHz = 900.0f) {
    Biquad bp = Biquad::make(Biquad::BP, bpHz, 0.7f, (float)b.rate);
    const size_t a = b.at(t0), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float t = (float)k / (float)b.rate, u = (float)k / (float)n;
        const float beat = std::pow(0.5f + 0.5f * std::sin(kTwoPi * hz * t), 4.0f);
        b.x[a + k] += bp.run(r.bi()) * beat * std::sin(3.14159f * u) * amp;
    }
}

Sound mothTap(int rate, int v) {
    Rand r(24200u + (unsigned)v);
    Buf b(0.5f, rate);
    ImpactSpec gl;
    gl.modes = modesGlass(r.range(3800.0f, 5200.0f), 0.08f, r);
    gl.contact = 0.0008f, gl.noise = 0.5f, gl.noiseHz = 6500.0f, gl.noiseT60 = 0.004f;
    b.add(impact(gl, 0.8f, 0.2f, rate, r), 0.03f);
    if (v != 1) b.add(impact(gl, 0.4f, 0.15f, rate, r), r.range(0.09f, 0.14f));  // gleich noch einmal
    flutter(b, 0.0f, r.range(0.25f, 0.4f), r.range(34.0f, 46.0f), 0.25f, r, 1100.0f);
    trim(b);
    normalize(b, level::kFoley - 6.0f);
    return b.sound();
}

Sound mothBuzz(int rate) {
    Rand r(24300u);
    Buf b(1.9f, rate);
    // mehrere Fluegelpaare, leicht verstimmt, schwellen an; harte Kanten (Schuppen, Chitin)
    for (int i = 0; i < 4; ++i) {
        const float hz = r.range(38.0f, 52.0f);
        Buf w(1.9f, rate);
        flutter(w, r.range(0.0f, 0.15f), 1.65f, hz, 0.6f, r, r.range(700.0f, 1400.0f));
        applyEnv(w, [](float t) { return std::clamp(t / 1.2f, 0.15f, 1.0f); });
        b.add(w, 0.0f, 0.8f);
    }
    for (int i = 0; i < 14; ++i) {  // Klicken: Motten stossen aneinander, an die Haut
        ImpactSpec c;
        c.modes = modesPlastic(r.range(2500.0f, 4200.0f), 0.01f, r);
        c.contact = 0.0006f, c.noise = 0.6f, c.noiseHz = 5000.0f;
        b.add(impact(c, r.range(0.3f, 0.8f), 0.05f, rate, r), r.range(0.3f, 1.7f), 0.35f);
    }
    trim(b);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

Sound mothBite(int rate) {
    Rand r(24400u);
    Buf b(0.5f, rate);
    ImpactSpec c;
    c.modes = modesPlastic(r.range(3000.0f, 4000.0f), 0.015f, r);
    c.contact = 0.0005f, c.noise = 0.7f, c.noiseHz = 6000.0f;
    b.add(impact(c, 1.0f, 0.08f, rate, r), 0.0f);
    b.add(impact(c, 0.7f, 0.08f, rate, r), 0.045f);
    // feuchtes Reissen
    Biquad bp = Biquad::make(Biquad::BP, 2200.0f, 1.2f, (float)rate);
    for (size_t i = b.at(0.06f); i < b.at(0.22f); ++i) {
        const float t = b.t(i) - 0.06f;
        b.x[i] += bp.run(r.bi() * (r.uni() < 0.3f ? 1.0f : 0.2f)) * 0.5f * std::exp(-t * 14.0f);
    }
    flutter(b, 0.0f, 0.3f, 48.0f, 0.2f, r, 1000.0f);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound ratLap(int rate, int v) {
    Rand r(24500u + (unsigned)v);
    const float L = r.range(0.7f, 1.0f);
    Buf b(L + 0.1f, rate);
    float at = 0.02f;
    while (at < L) {  // Zungenschlaege: kleines Ploppen und Schmatzen
        bubble(b, at, r.range(1.0f, 1.8f), r.range(0.4f, 0.8f), r);
        Biquad bp = Biquad::make(Biquad::BP, r.range(2500.0f, 4000.0f), 2.0f, (float)rate);
        for (size_t k = 0; k < (size_t)(0.018f * (float)rate) && b.at(at) + k < b.n(); ++k)
            b.x[b.at(at) + k] += bp.run(r.bi()) * 0.35f * std::exp(-(float)k / (0.006f * (float)rate));
        at += 1.0f / r.range(6.0f, 8.5f);
    }
    trim(b);
    normalize(b, level::kFoley - 7.0f);
    return b.sound();
}

Sound ratGnaw(int rate, int v) {
    Rand r(24600u + (unsigned)v);
    const float L = r.range(0.8f, 1.1f);
    Buf b(L + 0.1f, rate);
    float at = 0.0f;
    while (at < L) {  // Schneidezaehne raspeln (12-16 Bisse je Sekunde), dazwischen Knacken
        const size_t a = b.at(at);
        Biquad bp = Biquad::make(Biquad::BP, r.range(3500.0f, 6500.0f), 1.6f, (float)rate);
        const int n = r.irange(220, 420);
        for (int k = 0; k < n && a + (size_t)k < b.n(); ++k) b.x[a + (size_t)k] += bp.run(r.bi()) * 0.5f * std::sin(3.14159f * (float)k / (float)n);
        if (r.chance(0.18f)) {
            ImpactSpec c;
            c.modes = modesWood(r.range(900.0f, 1600.0f), 0.03f, r);
            c.contact = 0.001f, c.noise = 0.5f, c.noiseHz = 3000.0f;
            b.add(impact(c, r.range(0.4f, 0.8f), 0.06f, rate, r), at, 0.5f);
        }
        at += 1.0f / r.range(12.0f, 16.0f);
    }
    trim(b);
    normalize(b, level::kFoley - 6.0f);
    return b.sound();
}

}  // namespace

void addL2Sounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"window_form", [=] { return windowForm(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("window_air_{}", v), [=] { return windowAir(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("moth_tap_{}", v), [=] { return mothTap(rate, v); }});
    d.push_back({"moth_buzz", [=] { return mothBuzz(rate); }});
    d.push_back({"moth_bite", [=] { return mothBite(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("rat_lap_{}", v), [=] { return ratLap(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("rat_gnaw_{}", v), [=] { return ratGnaw(rate, v); }});
}

}  // namespace lim::audio
