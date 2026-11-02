// V13.3 Teil 4 Level 5: Klaenge der Wesen des Hotels (KREATUR_Samantha.md, KREATUR_Ants.md, KREATUR_Woodlins.md,
// KREATUR_Watchers.md, KREATUR_Gastgeber.md). Die Seiten nennen keine Laute ausser: Samantha spricht (mehrere Sprachen,
// gewandt), der Gastgeber spricht (hoeflich, beruhigend). Alles Weitere ist Auslegung nach dem Koerper:
//
//   Samantha   eine ruhige Frauenstimme (f0 um 205 Hz) aus einer kleinen Katze, darunter ein Schnurren als Rauheit;
//              vor jedem Satz ein kurzer Gurrlaut (Trillern der Katzen); Murmeln: leiser, gehaucht, mit anderer Satzmelodie
//              je Sprache. Schnurren (25 Hz), Fauchen (Rauschen durch die offene Kehle), Kreischen (Kampfschrei), Biss
//              (Knurren, Reissen von Stoff und Haut), Fressen (nasses Reissen, Kauen, ein Knacken), Papier beim Nehmen.
//   Kolonie    unzaehlige winzige, weiche Schritte und Fell an Fell (Trippeln wie Regen auf Filz), Abzupfen (Haare reissen,
//              ein feuchtes Ploppen).
//   Woodlins   Klacken von gedrechseltem Holz (Knoechel auf Moebeln, Kugeln, die gegeneinander rollen), Knarren eines
//              trockenen Gelenks (Ankuendigung), Biss: Holz knackt, ein Splittern, ein nasser Laut.
//   Watchers   Knacken langer Fingergelenke in Folge (Ankuendigung), Hieb: Luftzug, Stoff reisst, dumpfer Schlag.
//   Gastgeber  eine ruhige, warme Maennerstimme (f0 um 100 Hz), nah und trocken, dahinter ganz leise ein Grammophon.
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

struct Vow {
    float f1, f2, f3;
};
constexpr Vow kVow[] = {{730, 1090, 2440}, {530, 1840, 2480}, {300, 2250, 3000}, {570, 840, 2410}, {320, 900, 2250}, {660, 1720, 2410}, {490, 1350, 2500}};

void hissN(Buf& b, float at, float len, float hz, float q, float amp, Rand& r) {  // Zischlaut
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        const float u = (b.t(i) - at) / len;
        b.x[i] += bp.run(r.bi()) * amp * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void plosive(Buf& b, float at, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, r.range(1800.0f, 4200.0f), 1.2f, (float)b.rate);
    const size_t a = b.at(at);
    for (size_t k = 0; k < (size_t)(0.02f * (float)b.rate) && a + k < b.n(); ++k)
        b.x[a + k] += bp.run(r.bi()) * amp * std::exp(-(float)k / (0.004f * (float)b.rate));
}
// gesprochener Satz (Silben mit Konsonanten; Satzmelodie): k - Formantmassstab (1 Mensch), purr - Rauheit des Schnurrens
void speech(Buf& b, float t0, float len, float f0, float breath, float amp, Rand& r, float k = 1.0f, float purr = 0.0f, float tempo = 1.0f,
            float melody = 0.18f) {
    float t = t0;
    int syl = 0;
    while (t < t0 + len - 0.1f) {
        const int c = r.irange(0, 4);
        float cl = 0.0f;
        if (c == 0) cl = r.range(0.05f, 0.08f), hissN(b, t, cl, r.range(4800.0f, 6800.0f), 1.6f, 0.11f * amp, r);
        else if (c == 1) cl = 0.025f, plosive(b, t + 0.005f, 0.45f * amp, r);
        else if (c == 2) cl = r.range(0.04f, 0.06f), hissN(b, t, cl, r.range(2400.0f, 3200.0f), 1.6f, 0.07f * amp, r);
        const float sl = r.range(0.1f, 0.19f) / tempo;
        const Vow& w = kVow[r.irange(0, 6)];
        const float u0 = (t - t0) / len;
        const float accent = (syl % 3 == 0) ? r.range(0.06f, 0.14f) : 0.0f;
        const float fa = f0 * (1.08f - melody * u0 + accent), fb = fa * r.range(0.92f, 1.03f);
        VoiceSpec v;
        v.f0 = [=](float tt) { return fa + (fb - fa) * std::clamp(tt / sl, 0.0f, 1.0f); };
        v.amp = [=](float tt) { return amp * envAHR(tt, 0.02f, sl * 0.6f, sl * 0.35f); };
        v.formants = {w.f1 * k, w.f2 * k, w.f3 * k};
        v.bandwidths = {70.0f, 100.0f, 150.0f};
        v.jitter = 0.012f, v.shimmer = 0.06f, v.subharm = 0.03f, v.breath = breath, v.rough = 0.08f + purr, v.roughHz = purr > 0.0f ? 26.0f : 48.0f;
        v.tiltHz = 5200.0f;
        if (c == 3) {
            VoiceSpec nv = v;
            nv.formants = {280.0f * k, 1250.0f * k, 2600.0f * k};
            nv.amp = [=](float tt) { return 0.5f * amp * envAHR(tt, 0.01f, 0.03f, 0.02f); };
            voice(b, t, 0.06f, nv, r);
            cl = 0.05f;
        }
        voice(b, t + cl, sl, v, r);
        t += cl + sl + r.range(0.0f, 0.03f);
        if (r.chance(0.2f)) t += r.range(0.07f, 0.14f);
        ++syl;
    }
}
// Katzentriller: kurzer, steigender Gurrlaut durch die Nase
void trill(Buf& b, float at, float f0, float amp, Rand& r) {
    VoiceSpec v;
    const float L = 0.22f;
    v.f0 = [=](float t) { return f0 * (0.9f + 0.35f * std::clamp(t / L, 0.0f, 1.0f)) + 25.0f * std::sin(t * 160.0f); };
    v.amp = [=](float t) { return amp * envAHR(t, 0.02f, L * 0.5f, L * 0.4f); };
    v.formants = {420.0f, 1300.0f, 2900.0f};
    v.bandwidths = {120.0f, 180.0f, 260.0f};
    v.rough = 0.7f, v.roughHz = 30.0f, v.breath = 0.2f;
    voice(b, at, L, v, r);
}
void purrLayer(Buf& b, float t0, float len, float amp, Rand& r) {  // Schnurren: 25 Hz Pulse, ein- und ausatmend
    Biquad lp = Biquad::make(Biquad::LP, 380.0f, 0.8f, (float)b.rate);
    Biquad bp = Biquad::make(Biquad::BP, 140.0f, 0.9f, (float)b.rate);
    for (size_t i = b.at(t0); i < b.at(t0 + len) && i < b.n(); ++i) {
        const float t = b.t(i) - t0;
        const float br = 0.55f + 0.45f * std::sin(t * 2.0f * 3.14159f / 1.6f);  // Atemzug 1,6 s
        const float pulse = std::pow(std::max(0.0f, std::sin(t * 2.0f * 3.14159f * (24.0f + 2.0f * br))), 6.0f);
        const float env = envAHR(t, 0.15f, len - 0.4f, 0.25f);
        b.x[i] += (lp.run(r.bi()) * 0.6f + bp.run(r.bi())) * pulse * br * env * amp;
    }
}
void tearing(Buf& b, float t0, float len, float amp, float hz, Rand& r) {  // Reissen: Rauschen in feinen Stoessen
    Biquad bp = Biquad::make(Biquad::BP, hz, 0.9f, (float)b.rate);
    float g = 0.0f, tgt = 0.0f;
    for (size_t i = b.at(t0); i < b.at(t0 + len) && i < b.n(); ++i) {
        if (r.chance(160.0f / (float)b.rate)) tgt = r.range(0.0f, 1.0f);
        g += (tgt - g) * 0.02f;
        const float u = (b.t(i) - t0) / len;
        b.x[i] += bp.run(r.bi()) * amp * g * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void wetSquish(Buf& b, float at, float amp, Rand& r) {  // nasses Schmatzen: Blasen plus tiefes Rauschen
    for (int k = 0; k < 5; ++k) bubble(b, at + r.range(0.0f, 0.06f), r.range(1.2f, 3.5f), amp * 0.5f, r);
    tearing(b, at, 0.09f, amp * 0.6f, 900.0f, r);
}
void woodKnock(Buf& b, float at, float f0, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesWood(f0, 0.05f, r);
    sp.contact = 0.0012f, sp.noise = 0.3f, sp.noiseHz = 2600.0f, sp.noiseT60 = 0.01f;
    b.add(impact(sp, amp, 0.2f, b.rate, r), at);
}
void finish(Buf& b, float lufs, float wet, std::uint32_t seed) {
    if (wet > 0.0f) reverb(b, dsp::RoomSpec{0.7f, 0.6f, 0.35f, 0.55f}, wet, 1.0f, seed);
    trim(b);
    normalize(b, lufs);
}

// --- Samantha ----------------------------------------------------------------------------------------------------------
Sound samSay(int rate, int var, float len, float f0, float breath, float lufs, bool trillFirst, float tempo = 1.0f, float melody = 0.18f) {
    Rand r(91000u + (unsigned)var * 17u);
    Buf b(len + 0.9f, rate);
    float t = 0.05f;
    if (trillFirst) trill(b, t, f0 * 1.4f, 0.5f, r), t += 0.3f;
    speech(b, t, len, f0, breath, 1.0f, r, 1.0f, 0.18f, tempo, melody);
    purrLayer(b, 0.0f, len + 0.4f, 0.08f, r);
    finish(b, lufs, 0.12f, 9100u + (unsigned)var);
    return b.sound();
}
Sound samPurr(int rate) {
    Rand r(91100u);
    Buf b(2.6f, rate);
    purrLayer(b, 0.0f, 2.5f, 1.0f, r);
    finish(b, level::kCreature - 9.0f, 0.0f, 9110u);
    return b.sound();
}
Sound samWarn(int rate) {  // Fauchen, dann die Worte, wieder Fauchen
    Rand r(91200u);
    Buf b(2.4f, rate);
    hissN(b, 0.0f, 0.75f, 3800.0f, 0.5f, 0.9f, r);
    hissN(b, 0.0f, 0.75f, 1600.0f, 0.7f, 0.5f, r);
    speech(b, 0.75f, 0.9f, 190.0f, 0.45f, 1.0f, r, 1.0f, 0.6f, 1.15f);
    hissN(b, 1.75f, 0.6f, 3600.0f, 0.5f, 1.0f, r);
    finish(b, level::kCreature + 2.0f, 0.1f, 9120u);
    return b.sound();
}
Sound samScream(int rate) {  // Kampfschrei: Jaulen mit Periodenverdopplung
    Rand r(91300u);
    const float L = 0.85f;
    Buf b(L + 0.5f, rate);
    VoiceSpec v;
    v.f0 = [=](float t) { return 620.0f + 380.0f * std::sin(std::clamp(t / L, 0.0f, 1.0f) * 3.14159f) + 40.0f * std::sin(t * 31.0f); };
    v.amp = [=](float t) { return envAHR(t, 0.02f, L * 0.65f, L * 0.3f); };
    v.formants = {900.0f, 1700.0f, 3300.0f};
    v.bandwidths = {200.0f, 300.0f, 420.0f};
    v.jitter = 0.07f, v.shimmer = 0.3f, v.subharm = 0.6f, v.breath = 0.35f, v.rough = 0.9f, v.roughHz = 37.0f, v.tiltHz = 7000.0f;
    voice(b, 0.0f, L, v, r);
    hissN(b, 0.0f, 0.3f, 4200.0f, 0.6f, 0.5f, r);
    finish(b, level::kCreature + 6.0f, 0.15f, 9130u);
    return b.sound();
}
Sound samBite(int rate, int var) {  // Knurren, Krallen reissen, Zaehne
    Rand r(91400u + (unsigned)var);
    Buf b(0.7f, rate);
    VoiceSpec v;
    v.f0 = [](float t) { return 150.0f + 30.0f * std::sin(t * 9.0f); };
    v.amp = [](float t) { return envAHR(t, 0.01f, 0.25f, 0.15f); };
    v.formants = {500.0f, 1200.0f, 2600.0f};
    v.subharm = 0.8f, v.rough = 0.9f, v.roughHz = 32.0f, v.breath = 0.4f;
    voice(b, 0.0f, 0.42f, v, r);
    tearing(b, 0.05f, 0.25f, 1.0f, 2600.0f, r);
    tearing(b, 0.2f, 0.18f, 0.8f, 5200.0f, r);
    wetSquish(b, 0.28f, 0.6f, r);
    finish(b, level::kCreature + 1.0f, 0.05f, 9140u + (unsigned)var);
    return b.sound();
}
Sound samEat(int rate) {  // nasses Reissen, Kauen, ein Knacken
    Rand r(91500u);
    Buf b(1.3f, rate);
    tearing(b, 0.0f, 0.4f, 0.9f, 1100.0f, r);
    wetSquish(b, 0.35f, 0.8f, r);
    for (int k = 0; k < 4; ++k) wetSquish(b, 0.55f + 0.17f * (float)k, 0.45f, r);
    ImpactSpec sp;
    sp.modes = modesWood(1800.0f, 0.02f, r);
    sp.contact = 0.0008f, sp.noise = 0.7f, sp.noiseHz = 3200.0f;
    b.add(impact(sp, 0.5f, 0.1f, b.rate, r), 0.9f);
    finish(b, level::kFoley - 1.0f, 0.05f, 9150u);
    return b.sound();
}
Sound samTake(int rate) {  // Papier raschelt, ein kurzes Zerren
    Rand r(91600u);
    Buf b(0.6f, rate);
    tearing(b, 0.0f, 0.45f, 1.0f, 3800.0f, r);
    finish(b, level::kFoley, 0.05f, 9160u);
    return b.sound();
}

// --- Kolonie (Entity 89) ------------------------------------------------------------------------------------------------
Sound antSwarm(int rate) {  // ein Schwall winziger, weicher Schritte
    Rand r(92000u);
    Buf b(2.2f, rate);
    grains(b, 0.0f, 2.0f, 900.0f, [](Buf& bb, float at, Rand& rr) {
        Biquad bp = Biquad::make(Biquad::BP, rr.range(2500.0f, 6500.0f), 2.0f, (float)bb.rate);
        const size_t a = bb.at(at);
        const float amp = rr.range(0.05f, 0.18f);
        for (size_t k = 0; k < (size_t)(0.006f * (float)bb.rate) && a + k < bb.n(); ++k)
            bb.x[a + k] += bp.run(rr.bi()) * amp * std::exp(-(float)k / (0.0012f * (float)bb.rate));
    }, r, [](float t) { return t < 0.3f ? t / 0.3f : std::exp(-(t - 0.3f) * 1.4f); });
    tearing(b, 0.0f, 1.6f, 0.25f, 7000.0f, r);  // Fell an Fell
    finish(b, level::kCreature - 4.0f, 0.08f, 9200u);
    return b.sound();
}
Sound antRustle(int rate) {
    Rand r(92100u);
    Buf b(1.4f, rate);
    tearing(b, 0.0f, 1.3f, 0.6f, 6500.0f, r);
    grains(b, 0.1f, 1.2f, 140.0f, [](Buf& bb, float at, Rand& rr) {
        Biquad bp = Biquad::make(Biquad::BP, rr.range(3000.0f, 6000.0f), 2.0f, (float)bb.rate);
        const size_t a = bb.at(at);
        for (size_t k = 0; k < (size_t)(0.005f * (float)bb.rate) && a + k < bb.n(); ++k)
            bb.x[a + k] += bp.run(rr.bi()) * 0.08f * std::exp(-(float)k / (0.001f * (float)bb.rate));
    }, r);
    finish(b, level::kCreature - 12.0f, 0.05f, 9210u);
    return b.sound();
}
Sound antPick(int rate) {  // ein Buschel abreissen: Haare, kleines feuchtes Ploppen, Trippeln am Boden
    Rand r(92200u);
    Buf b(0.7f, rate);
    tearing(b, 0.0f, 0.18f, 1.0f, 5000.0f, r);
    for (int k = 0; k < 6; ++k) bubble(b, 0.12f + r.range(0.0f, 0.12f), r.range(1.5f, 3.0f), 0.25f, r);
    grains(b, 0.25f, 0.65f, 300.0f, [](Buf& bb, float at, Rand& rr) {
        const size_t a = bb.at(at);
        for (size_t k = 0; k < (size_t)(0.004f * (float)bb.rate) && a + k < bb.n(); ++k) bb.x[a + k] += rr.bi() * 0.05f * std::exp(-(float)k / 40.0f);
    }, r);
    finish(b, level::kFoley, 0.0f, 9220u);
    return b.sound();
}

// --- Woodlins ----------------------------------------------------------------------------------------------------------
Sound wlClick(int rate, int var) {  // Knoechel aus Holz auf Holz, eine Kugel rollt nach
    Rand r(93000u + (unsigned)var * 7u);
    Buf b(1.0f, rate);
    const int n = 2 + var;
    float t = 0.0f;
    for (int k = 0; k < n; ++k) woodKnock(b, t, r.range(700.0f, 1400.0f), r.range(0.5f, 1.0f), r), t += r.range(0.06f, 0.16f);
    if (var == 2) {  // rollende Kugel
        for (int k = 0; k < 9; ++k) woodKnock(b, t + 0.04f * (float)k, 2200.0f - 60.0f * (float)k, 0.25f * (1.0f - (float)k / 9.0f), r);
    }
    finish(b, level::kEvent - 6.0f, 0.12f, 9300u + (unsigned)var);
    return b.sound();
}
Sound wlCreak(int rate) {
    Rand r(93100u);
    Buf b(1.0f, rate);
    CreakSpec c;
    c.rate0 = 22.0f, c.rate1 = 55.0f, c.jitter = 0.35f, c.formants = {420.0f, 1100.0f, 2300.0f}, c.q = 9.0f;
    creak(b, 0.0f, 0.75f, c, 1.0f, r);
    finish(b, level::kEvent - 2.0f, 0.1f, 9310u);
    return b.sound();
}
Sound wlBite(int rate) {
    Rand r(93200u);
    Buf b(0.8f, rate);
    woodKnock(b, 0.0f, 900.0f, 1.0f, r);
    tearing(b, 0.02f, 0.2f, 0.8f, 4200.0f, r);  // Splittern
    wetSquish(b, 0.1f, 0.6f, r);
    CreakSpec c;
    c.rate0 = 60.0f, c.rate1 = 18.0f, c.formants = {600.0f, 1500.0f}, c.q = 7.0f;
    creak(b, 0.2f, 0.35f, c, 0.6f, r);
    finish(b, level::kEvent + 1.0f, 0.08f, 9320u);
    return b.sound();
}

// --- Watchers ----------------------------------------------------------------------------------------------------------
Sound waCrack(int rate) {  // Fingergelenke knacken nacheinander
    Rand r(94000u);
    Buf b(1.0f, rate);
    float t = 0.0f;
    for (int k = 0; k < 6; ++k) {
        ImpactSpec sp;
        sp.modes = modesWood(r.range(1500.0f, 2600.0f), 0.01f, r);
        sp.contact = 0.0005f, sp.noise = 0.8f, sp.noiseHz = 3000.0f, sp.noiseT60 = 0.006f;
        b.add(impact(sp, r.range(0.5f, 1.0f), 0.06f, b.rate, r), t);
        t += r.range(0.07f, 0.13f);
    }
    finish(b, level::kEvent - 3.0f, 0.15f, 9400u);
    return b.sound();
}
Sound waStrike(int rate) {
    Rand r(94100u);
    Buf b(0.9f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 700.0f, 0.7f, (float)b.rate);
    for (size_t i = 0; i < b.at(0.3f); ++i) {
        const float u = b.t(i) / 0.3f;
        b.x[i] += lp.run(r.bi()) * 1.6f * (u < 0.4f ? u / 0.4f : std::exp(-(u - 0.4f) * 5.0f));
    }
    tearing(b, 0.18f, 0.25f, 1.0f, 3000.0f, r);  // Stoff reisst
    ImpactSpec sp;
    sp.modes = modesConcrete(140.0f, r);
    sp.contact = 0.008f, sp.noise = 0.4f, sp.noiseHz = 600.0f;
    b.add(impact(sp, 0.8f, 0.3f, b.rate, r), 0.2f);
    finish(b, level::kEvent + 2.0f, 0.06f, 9410u);
    return b.sound();
}

// --- der Gastgeber -----------------------------------------------------------------------------------------------------
Sound hostLine(int rate, int var) {
    static const float kLen[9] = {2.6f, 3.0f, 3.6f, 4.2f, 3.0f, 3.2f, 0.8f, 2.8f, 2.6f};
    Rand r(95000u + (unsigned)var * 23u);
    const float L = kLen[var % 9];
    Buf b(L + 1.2f, rate);
    // warm, ruhig, nah; kaum Atem; Satzmelodie faellt sanft (hoeflich); Nr. 7 gefluestert
    speech(b, 0.1f, L, var == 7 ? 96.0f : r.range(98.0f, 106.0f), var == 7 ? 0.85f : 0.07f, 1.0f, r, 0.97f, 0.0f, 0.92f, 0.14f);
    // ganz leise ein Grammophon im Hintergrund (das Buero): Knistern und gedaempfte Toene
    Biquad lp = Biquad::make(Biquad::LP, 1400.0f, 0.7f, (float)b.rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float tone = 0.012f * std::sin(t * 2.0f * 3.14159f * 220.0f) * (0.5f + 0.5f * std::sin(t * 1.7f)) +
                           0.008f * std::sin(t * 2.0f * 3.14159f * 330.0f);
        b.x[i] += lp.run(tone + (r.chance(0.0015f) ? r.bi() * 0.06f : 0.0f)) * (var == 7 ? 0.0f : 1.0f);
    }
    finish(b, level::kCreature - (var == 7 ? 6.0f : 2.0f), var == 7 ? 0.0f : 0.08f, 9500u + (unsigned)var);
    return b.sound();
}

}  // namespace

void addHotelSounds(std::vector<SoundDef>& d, int rate) {
    // Samantha: Saetze (Laenge, Grundton, Hauch, Lautheit, Triller vorweg, Tempo, Melodie)
    d.push_back({"sam_purr", [=] { return samPurr(rate); }});
    for (int v = 0; v < 3; ++v)  // Murmeln: franzoesisch (fliessend, steigend), italienisch (lebhaft), englisch (singend)
        d.push_back({std::format("sam_murmur_{}", v), [=] {
                         return samSay(rate, 10 + v, 1.6f, 200.0f, 0.4f, level::kCreature - 9.0f, false, v == 1 ? 1.25f : 0.95f, v == 0 ? -0.05f : 0.25f);
                     }});
    d.push_back({"sam_ask_0", [=] { return samSay(rate, 20, 2.6f, 205.0f, 0.12f, level::kCreature - 2.0f, true); }});
    d.push_back({"sam_ask_1", [=] { return samSay(rate, 21, 2.0f, 210.0f, 0.12f, level::kCreature - 2.0f, false); }});
    d.push_back({"sam_ask_2", [=] { return samSay(rate, 22, 1.7f, 198.0f, 0.15f, level::kCreature - 2.0f, false, 0.9f); }});
    d.push_back({"sam_thank_good", [=] { return samSay(rate, 23, 1.8f, 215.0f, 0.1f, level::kCreature - 2.0f, true); }});
    d.push_back({"sam_thank_poor", [=] { return samSay(rate, 24, 1.2f, 190.0f, 0.2f, level::kCreature - 3.0f, false, 0.85f, 0.3f); }});
    d.push_back({"sam_read_good", [=] { return samSay(rate, 25, 4.6f, 200.0f, 0.1f, level::kCreature - 2.0f, false, 0.9f, 0.12f); }});
    d.push_back({"sam_read_poor", [=] { return samSay(rate, 26, 2.0f, 188.0f, 0.2f, level::kCreature - 3.0f, false, 1.1f, 0.3f); }});
    d.push_back({"sam_warn", [=] { return samWarn(rate); }});
    d.push_back({"sam_rude", [=] { return samSay(rate, 27, 2.0f, 196.0f, 0.15f, level::kCreature - 1.0f, false, 1.05f, 0.25f); }});
    d.push_back({"sam_scream", [=] { return samScream(rate); }});
    d.push_back({"sam_bite", [=] { return samBite(rate, 0); }});
    d.push_back({"sam_eat", [=] { return samEat(rate); }});
    d.push_back({"sam_bye", [=] { return samSay(rate, 28, 2.1f, 205.0f, 0.15f, level::kCreature - 3.0f, false, 0.95f, 0.1f); }});
    d.push_back({"sam_take", [=] { return samTake(rate); }});
    // Kolonie, Woodlins, Watchers
    d.push_back({"ant_swarm", [=] { return antSwarm(rate); }});
    d.push_back({"ant_rustle", [=] { return antRustle(rate); }});
    d.push_back({"ant_pick", [=] { return antPick(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wl_click_{}", v), [=] { return wlClick(rate, v); }});
    d.push_back({"wl_creak", [=] { return wlCreak(rate); }});
    d.push_back({"wl_bite", [=] { return wlBite(rate); }});
    d.push_back({"wa_crack", [=] { return waCrack(rate); }});
    d.push_back({"wa_strike", [=] { return waStrike(rate); }});
    for (int v = 0; v < 9; ++v) d.push_back({std::format("e18_line_{}", v), [=] { return hostLine(rate, v); }});
}

}  // namespace lim::audio
