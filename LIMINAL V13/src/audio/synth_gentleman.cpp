// V13.3 Teil 4 Level 5.3: Klaenge des Hausherrn und der Bibliothek (KREATUR_Gentleman.md, KREATUR_Bibliothekswissen.md).
// Die Seiten nennen: er spricht zur Bibliothek, ihre einzige Antwort ist Stille; die Bibliothek zeigt Zorn durch einstuerzende
// Regale. Alles Weitere ist Auslegung nach Gestalt und Ort:
//   Stimme    ruhig, tief (f0 um 92 Hz), sehr gleichmaessig (kaum Zittern - das unbewegte Gesicht), leicht doppelt
//             (zwei Stimmen, 0,4 % verstimmt: man hoert es kaum, aber es stimmt etwas nicht); in der Halle mit Hall.
//   Schritte  harte Ledersohlen auf Fischgraetparkett, gemessen; ein Brett gibt nach.
//   Gelenke   knacken, wenn sich der Arm entfaltet (drei Glieder); Stoff spannt sich.
//   Gesicht   der Firnis knistert wie duennes Eis, darunter ein nasser Laut (der Riss).
//   Bibliothek Regale knarren, wenn er eintritt; Stille (Druckabfall, ein feiner Ton im Ohr); Zorn: Holz aechzt, Buecher
//             stuerzen in Kaskaden, ein Brett bricht.
//   Diktiergeraet (rote Lichter, Entity 141): Klick, Bandrauschen, eine Stimme, zu glatt, wie aus einer Maschine.
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
constexpr Vow kVowG[] = {{730, 1090, 2440}, {530, 1840, 2480}, {300, 2250, 3000}, {570, 840, 2410}, {320, 900, 2250}, {660, 1720, 2410}, {490, 1350, 2500}};

void hissG(Buf& b, float at, float len, float hz, float q, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        const float u = (b.t(i) - at) / len;
        b.x[i] += bp.run(r.bi()) * amp * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void plosiveG(Buf& b, float at, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, r.range(1800.0f, 4200.0f), 1.2f, (float)b.rate);
    const size_t a = b.at(at);
    for (size_t k = 0; k < (size_t)(0.02f * (float)b.rate) && a + k < b.n(); ++k)
        b.x[a + k] += bp.run(r.bi()) * amp * std::exp(-(float)k / (0.004f * (float)b.rate));
}
// Satz: Silben mit Konsonanten; fallende, hoefliche Melodie; detune > 0: zweite Stimme, leicht verstimmt
void speechG(Buf& b, float t0, float len, float f0, float breath, float amp, Rand& r, float tempo, float melody, float detune) {
    float t = t0;
    int syl = 0;
    while (t < t0 + len - 0.1f) {
        const int c = r.irange(0, 4);
        float cl = 0.0f;
        if (c == 0) cl = r.range(0.05f, 0.08f), hissG(b, t, cl, r.range(4800.0f, 6800.0f), 1.6f, 0.1f * amp, r);
        else if (c == 1) cl = 0.025f, plosiveG(b, t + 0.005f, 0.4f * amp, r);
        else if (c == 2) cl = r.range(0.04f, 0.06f), hissG(b, t, cl, r.range(2400.0f, 3200.0f), 1.6f, 0.06f * amp, r);
        const float sl = r.range(0.11f, 0.2f) / tempo;
        const Vow& w = kVowG[r.irange(0, 6)];
        const float u0 = (t - t0) / len;
        const float accent = (syl % 4 == 0) ? r.range(0.04f, 0.09f) : 0.0f;
        const float fa = f0 * (1.06f - melody * u0 + accent), fb = fa * r.range(0.96f, 1.01f);
        for (int k = 0; k < (detune > 0.0f ? 2 : 1); ++k) {
            const float m = k == 0 ? 1.0f : 1.0f + detune;
            VoiceSpec v;
            v.f0 = [=](float tt) { return m * (fa + (fb - fa) * std::clamp(tt / sl, 0.0f, 1.0f)); };
            v.amp = [=](float tt) { return amp * (k == 0 ? 1.0f : 0.45f) * envAHR(tt, 0.02f, sl * 0.62f, sl * 0.33f); };
            v.formants = {w.f1 * 0.93f, w.f2 * 0.93f, w.f3 * 0.95f};
            v.bandwidths = {60.0f, 90.0f, 140.0f};
            v.jitter = 0.004f, v.shimmer = 0.03f, v.breath = breath, v.rough = 0.03f, v.tiltHz = 4200.0f;
            voice(b, t + cl, sl, v, r);
        }
        t += cl + sl + r.range(0.0f, 0.03f);
        if (r.chance(0.18f)) t += r.range(0.08f, 0.16f);
        ++syl;
    }
}
void finishG(Buf& b, float lufs, float wet, float size, std::uint32_t seed) {
    if (wet > 0.0f) reverb(b, dsp::RoomSpec{1.6f, 1.3f, 0.6f, size, 0.02f, 0.75f}, wet, 1.0f, seed);
    trim(b);
    normalize(b, lufs);
}
void jointCrack(Buf& b, float at, float amp, Rand& r) {  // ein Gelenk knackt: kurzer, trockener Knall mit Nachknistern
    ImpactSpec sp;
    sp.modes = modesWood(r.range(900.0f, 1500.0f), 0.02f, r);
    sp.contact = 0.0006f, sp.noise = 0.8f, sp.noiseHz = 2600.0f, sp.noiseT60 = 0.008f;
    b.add(impact(sp, amp, 0.08f, b.rate, r), at);
    for (int k = 0; k < 4; ++k) plosiveG(b, at + 0.01f + r.range(0.0f, 0.04f), amp * 0.15f, r);
}
void cloth(Buf& b, float at, float len, float amp, float hz, Rand& r) {  // gestaerkter Stoff: Rauschen in Stoessen
    Biquad bp = Biquad::make(Biquad::BP, hz, 0.8f, (float)b.rate);
    float g = 0.0f, tgt = 0.0f;
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        if (r.chance(90.0f / (float)b.rate)) tgt = r.range(0.0f, 1.0f);
        g += (tgt - g) * 0.01f;
        const float u = (b.t(i) - at) / len;
        b.x[i] += bp.run(r.bi()) * amp * g * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void bookHit(Buf& b, float at, float amp, Rand& r) {  // Buch auf Holzboden: dumpf, Papier flattert nach
    ImpactSpec sp;
    sp.modes = modesWood(r.range(120.0f, 200.0f), 0.1f, r);
    sp.contact = r.range(0.006f, 0.01f), sp.noise = 0.7f, sp.noiseHz = r.range(700.0f, 1200.0f), sp.noiseQ = 0.6f, sp.noiseT60 = 0.06f;
    b.add(impact(sp, amp, 0.6f, b.rate, r), at);
    hissG(b, at + 0.01f, 0.12f, 3600.0f, 0.9f, amp * 0.18f, r);
}

// --- der Hausherr -------------------------------------------------------------------------------------------------------
Sound gmStep(int rate, int var) {
    Rand r(70100u + (unsigned)var * 17u);
    Buf b(0.7f, rate);
    // Absatz (hart, hell), dann die Sohle; manchmal gibt ein Brett nach
    ImpactSpec heel;
    heel.modes = modesWood(r.range(210.0f, 260.0f), 0.07f, r);
    heel.contact = 0.0011f, heel.noise = 0.5f, heel.noiseHz = 3200.0f, heel.noiseT60 = 0.012f;
    b.add(impact(heel, 1.0f, 0.3f, rate, r), 0.0f);
    ImpactSpec sole = heel;
    sole.modes = modesWood(r.range(150.0f, 190.0f), 0.05f, r);
    sole.contact = 0.004f, sole.noise = 0.3f, sole.noiseHz = 1500.0f;
    b.add(impact(sole, 0.45f, 0.3f, rate, r), r.range(0.05f, 0.07f));
    if (var % 2 == 1) {
        CreakSpec c;
        c.rate0 = 18.0f, c.rate1 = 26.0f, c.formants = {r.range(380.0f, 520.0f), 1150.0f, 2500.0f}, c.q = 10.0f;
        creak(b, 0.09f, r.range(0.12f, 0.2f), c, 0.25f, r);
    }
    finishG(b, level::kStep + 1.0f, 0.12f, 1.4f, 70150u + (unsigned)var);
    return b.sound();
}
Sound gmDoor(int rate) {  // weit hinten im alten Gang: eine schwere, alte Tuer - Klinke, Angel, leises Zufallen
    Rand r(70200u);
    Buf b(3.4f, rate);
    jointCrack(b, 0.05f, 0.5f, r);  // Klinke
    CreakSpec c;
    c.rate0 = 11.0f, c.rate1 = 34.0f, c.jitter = 0.3f, c.formants = {420.0f, 980.0f, 2300.0f}, c.q = 12.0f;
    creak(b, 0.25f, 1.6f, c, 0.9f, r);
    ImpactSpec sp;
    sp.modes = modesWood(95.0f, 0.18f, r);
    sp.contact = 0.012f, sp.noise = 0.4f, sp.noiseHz = 500.0f;
    b.add(impact(sp, 0.6f, 0.8f, rate, r), 2.2f);
    distantize(b, 2600.0f, 1.4f, 70201u);
    trim(b);
    normalize(b, level::kDistant + 2.0f);
    return b.sound();
}
Sound gmSigh(int rate) {
    Rand r(70300u);
    Buf b(2.2f, rate);
    breathe(b, 0.0f, 0.5f, true, 0.35f, 0.2f, r);
    breathe(b, 0.55f, 1.4f, false, 0.6f, 0.1f, r);
    filter(b, Biquad::LP, 2600.0f, 0.7f);  // durch die Nase
    finishG(b, level::kCreature - 9.0f, 0.1f, 1.2f, 70301u);
    return b.sound();
}
Sound gmLine(int rate, int var, float len, float lufs, float breath, float wet, float tempo, float melody) {
    Rand r(70400u + (unsigned)var * 31u);
    Buf b(len + 1.4f, rate);
    speechG(b, 0.1f, len, r.range(90.0f, 95.0f), breath, 1.0f, r, tempo, melody, 0.004f);
    finishG(b, lufs, wet, 1.6f, 70450u + (unsigned)var);
    return b.sound();
}
Sound gmNotice(int rate) {  // Halswirbel knacken (der Kopf dreht sich zu weit), dann zwei Worte
    Rand r(70500u);
    Buf b(3.6f, rate);
    for (int k = 0; k < 5; ++k) jointCrack(b, 0.02f + 0.11f * (float)k + r.range(0.0f, 0.03f), r.range(0.35f, 0.6f), r);
    speechG(b, 0.8f, 1.9f, 91.0f, 0.06f, 1.0f, r, 0.9f, 0.1f, 0.004f);
    finishG(b, level::kCreature - 1.0f, 0.18f, 1.6f, 70501u);
    return b.sound();
}
Sound gmAnnounce(int rate) {  // der Arm entfaltet sich: Gelenk um Gelenk, Stoff spannt sich, "Erlauben Sie."
    Rand r(70600u);
    Buf b(2.4f, rate);
    for (int k = 0; k < 3; ++k) {
        jointCrack(b, 0.05f + 0.36f * (float)k, 0.9f, r);
        cloth(b, 0.02f + 0.36f * (float)k, 0.3f, 0.5f, 1800.0f, r);
    }
    speechG(b, 1.15f, 0.8f, 89.0f, 0.05f, 0.9f, r, 0.95f, 0.06f, 0.004f);
    finishG(b, level::kCreature + 1.0f, 0.12f, 1.4f, 70601u);
    return b.sound();
}
Sound gmGrip(int rate) {  // ganz nah: kalter Atem, die Finger auf der Haut, gefluesterte Worte
    Rand r(70700u);
    Buf b(3.6f, rate);
    cloth(b, 0.0f, 0.25f, 0.7f, 2400.0f, r);
    for (int k = 0; k < 6; ++k) bubble(b, 0.05f + r.range(0.0f, 0.2f), r.range(1.5f, 3.0f), 0.08f, r);  // Haut
    breathe(b, 0.2f, 0.6f, true, 0.4f, 0.4f, r);
    speechG(b, 0.8f, 2.4f, 88.0f, 0.85f, 0.85f, r, 0.85f, 0.08f, 0.0f);
    finishG(b, level::kCreature, 0.0f, 1.0f, 70701u);
    return b.sound();
}
Sound gmCompose(int rate) {  // zwei kurze Zuege an den Revers, dann der Satz
    Rand r(70800u);
    Buf b(5.0f, rate);
    cloth(b, 0.0f, 0.18f, 0.9f, 2000.0f, r);
    cloth(b, 0.32f, 0.18f, 0.9f, 2200.0f, r);
    speechG(b, 0.7f, 3.4f, 93.0f, 0.06f, 1.0f, r, 0.9f, 0.14f, 0.004f);
    finishG(b, level::kCreature - 2.0f, 0.18f, 1.6f, 70801u);
    return b.sound();
}
Sound gmCrack(int rate) {  // der Firnis seines Gesichts knistert wie duennes Eis, darunter etwas Nasses
    Rand r(70900u);
    Buf b(1.4f, rate);
    grains(b, 0.0f, 0.8f, 160.0f, [&](Buf& bb, float at, Rand& rr) { plosiveG(bb, at, rr.range(0.1f, 0.4f), rr); }, r,
           [](float t) { return std::exp(-t * 2.5f); });
    for (int k = 0; k < 4; ++k) bubble(b, 0.4f + r.range(0.0f, 0.3f), r.range(1.0f, 2.2f), 0.12f, r);
    finishG(b, level::kCreature - 8.0f, 0.04f, 1.0f, 70901u);
    return b.sound();
}
Sound gmBookDrop(int rate) {  // das Notizbuch faellt vor seine Fuesse: dumpf, Blaetter flattern
    Rand r(71000u);
    Buf b(1.2f, rate);
    bookHit(b, 0.0f, 1.0f, r);
    hissG(b, 0.05f, 0.35f, 5200.0f, 0.8f, 0.08f, r);
    finishG(b, level::kEvent - 2.0f, 0.15f, 1.6f, 71001u);
    return b.sound();
}
Sound gmPages(int rate) {  // blaettern: zwei, drei Seiten
    Rand r(71100u);
    Buf b(1.2f, rate);
    for (int k = 0; k < 3; ++k) {
        const float at = 0.05f + 0.28f * (float)k + r.range(0.0f, 0.08f);
        hissG(b, at, 0.16f, r.range(3800.0f, 6200.0f), 0.7f, 0.5f, r);
        plosiveG(b, at + 0.14f, 0.15f, r);
    }
    finishG(b, level::kFoley - 4.0f, 0.06f, 1.0f, 71101u);
    return b.sound();
}
Sound gmThrow(int rate) {  // Stoff schnappt, das Buch saust durch die Luft
    Rand r(71200u);
    Buf b(1.0f, rate);
    cloth(b, 0.0f, 0.12f, 0.9f, 2600.0f, r);
    Biquad bp = Biquad::make(Biquad::BP, 900.0f, 1.2f, (float)b.rate);
    for (size_t i = b.at(0.05f); i < b.at(0.55f) && i < b.n(); ++i) {
        const float u = (b.t(i) - 0.05f) / 0.5f;
        bp = Biquad::make(Biquad::BP, 500.0f + 2600.0f * u, 1.2f, (float)b.rate);
        b.x[i] += bp.run(r.bi()) * std::sin(3.14159f * u) * 0.8f;
    }
    finishG(b, level::kFoley, 0.1f, 1.4f, 71201u);
    return b.sound();
}
Sound gmLand(int rate) {  // schlaegt auf und rutscht ueber das Parkett
    Rand r(71300u);
    Buf b(1.4f, rate);
    bookHit(b, 0.0f, 1.0f, r);
    Biquad bp = Biquad::make(Biquad::BP, 1400.0f, 0.7f, (float)b.rate);
    for (size_t i = b.at(0.03f); i < b.at(0.6f) && i < b.n(); ++i) {
        const float u = (b.t(i) - 0.03f) / 0.57f;
        b.x[i] += bp.run(r.bi()) * 0.35f * (1.0f - u) * (1.0f - u);
    }
    bookHit(b, 0.62f, 0.25f, r);
    finishG(b, level::kEvent - 1.0f, 0.2f, 1.8f, 71301u);
    return b.sound();
}
Sound gmGone(int rate) {  // weit hinten: eine Tuer faellt leise ins Schloss
    Rand r(71400u);
    Buf b(2.2f, rate);
    CreakSpec c;
    c.rate0 = 30.0f, c.rate1 = 12.0f, c.formants = {460.0f, 1000.0f, 2400.0f}, c.q = 11.0f;
    creak(b, 0.0f, 0.7f, c, 0.6f, r);
    ImpactSpec sp;
    sp.modes = modesWood(90.0f, 0.2f, r);
    sp.contact = 0.01f, sp.noise = 0.35f, sp.noiseHz = 450.0f;
    b.add(impact(sp, 0.9f, 0.9f, rate, r), 0.75f);
    jointCrack(b, 0.8f, 0.3f, r);
    distantize(b, 2200.0f, 1.6f, 71401u);
    trim(b);
    normalize(b, level::kDistant);
    return b.sound();
}

// --- die Bibliothek -----------------------------------------------------------------------------------------------------
Sound libHush(int rate) {  // die Luft wird dicht: ein Druckabfall, das Rauschen der Halle erlischt, ein feiner Ton im Ohr
    Rand r(71500u);
    Buf b(4.6f, rate);
    Biquad lp = Biquad::make(Biquad::LP, 180.0f, 0.7f, (float)b.rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float drop = std::exp(-t * 1.6f);
        b.x[i] += lp.run(r.bi()) * 0.5f * drop + 0.25f * std::sin(t * kTwoPi * (46.0f - 8.0f * t)) * drop;
        b.x[i] += 0.012f * std::sin(t * kTwoPi * 5240.0f) * envAHR(t, 0.8f, 2.6f, 1.0f);  // Ton im Ohr
    }
    trim(b);
    normalize(b, level::kEvent - 8.0f);
    return b.sound();
}
Sound libCreak(int rate) {  // die Regale knarren rundum (die Bibliothek bemerkt ihn)
    Rand r(71600u);
    Buf b(3.0f, rate);
    for (int k = 0; k < 5; ++k) {
        CreakSpec c;
        c.rate0 = r.range(10.0f, 20.0f), c.rate1 = r.range(24.0f, 50.0f), c.jitter = 0.35f;
        c.formants = {r.range(300.0f, 520.0f), r.range(900.0f, 1300.0f), r.range(2200.0f, 2800.0f)}, c.q = 9.0f;
        creak(b, r.range(0.0f, 1.6f), r.range(0.4f, 1.0f), c, r.range(0.4f, 0.9f), r);
    }
    finishG(b, level::kEvent - 6.0f, 0.3f, 2.2f, 71601u);
    return b.sound();
}
Sound libWrath(int rate) {  // Holz aechzt lange, ein Brett bricht, Buecher stuerzen in Kaskaden
    Rand r(71700u);
    Buf b(5.5f, rate);
    CreakSpec c;
    c.rate0 = 8.0f, c.rate1 = 30.0f, c.jitter = 0.4f, c.formants = {260.0f, 760.0f, 1900.0f}, c.q = 8.0f;
    creak(b, 0.0f, 1.4f, c, 1.0f, r);
    ImpactSpec sp;  // das Brett bricht
    sp.modes = modesWood(170.0f, 0.12f, r);
    sp.contact = 0.002f, sp.noise = 0.9f, sp.noiseHz = 1800.0f, sp.noiseT60 = 0.05f;
    b.add(impact(sp, 1.0f, 0.5f, rate, r), 1.35f);
    float t = 1.45f;
    for (int k = 0; k < 26; ++k) {
        bookHit(b, t, r.range(0.35f, 1.0f) * (k < 18 ? 1.0f : 0.5f), r);
        t += r.range(0.03f, 0.16f) * (k < 14 ? 1.0f : 1.8f);
    }
    finishG(b, level::kEvent + 1.0f, 0.35f, 2.4f, 71701u);
    return b.sound();
}
Sound libDicta(int rate) {  // Diktiergeraet: Klick, Bandrauschen, eine glatte, maschinenhafte Stimme, Klick
    Rand r(71800u);
    Buf b(5.6f, rate);
    jointCrack(b, 0.0f, 0.4f, r);
    Buf v(4.8f, rate);
    speechG(v, 0.1f, 3.6f, 118.0f, 0.02f, 1.0f, r, 0.95f, 0.02f, 0.0f);  // fast feste Tonhoehe
    for (size_t i = 0; i < v.n(); ++i) {  // zu glatt: hart gestuft (wie aus einer Maschine), bandbegrenzt
        const float q = 24.0f;
        v.x[i] = std::round(v.x[i] * q) / q;
        v.x[i] *= 0.85f + 0.15f * std::sin(b.t(i) * kTwoPi * 63.0f);
    }
    filter(v, Biquad::HP, 380.0f, 0.7f);
    filter(v, Biquad::LP, 3000.0f, 0.7f);
    b.add(v, 0.35f, 0.9f);
    Biquad hp = Biquad::make(Biquad::HP, 2500.0f, 0.7f, (float)b.rate);
    for (size_t i = b.at(0.25f); i < b.at(4.6f) && i < b.n(); ++i) b.x[i] += hp.run(r.bi()) * 0.03f;  // Band
    jointCrack(b, 4.65f, 0.4f, r);
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

}  // namespace

void addGentlemanSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 4; ++v) d.push_back({std::format("gm_step_{}", v), [=] { return gmStep(rate, v); }});
    d.push_back({"gm_door", [=] { return gmDoor(rate); }});
    d.push_back({"gm_sigh", [=] { return gmSigh(rate); }});
    d.push_back({"gm_mutter", [=] { return gmLine(rate, 0, 2.8f, level::kCreature - 8.0f, 0.12f, 0.12f, 0.85f, 0.12f); }});
    d.push_back({"gm_bookdrop", [=] { return gmBookDrop(rate); }});
    d.push_back({"gm_pages", [=] { return gmPages(rate); }});
    d.push_back({"gm_throw", [=] { return gmThrow(rate); }});
    d.push_back({"gm_land", [=] { return gmLand(rate); }});
    d.push_back({"gm_speak_0", [=] { return gmLine(rate, 1, 5.0f, level::kCreature, 0.05f, 0.3f, 0.9f, 0.14f); }});
    d.push_back({"gm_speak_1", [=] { return gmLine(rate, 2, 4.5f, level::kCreature, 0.05f, 0.3f, 0.92f, 0.16f); }});
    d.push_back({"gm_hush", [=] { return libHush(rate); }});
    d.push_back({"gm_speak_2", [=] { return gmLine(rate, 3, 5.0f, level::kCreature + 1.0f, 0.05f, 0.3f, 0.88f, 0.2f); }});
    d.push_back({"gm_notice", [=] { return gmNotice(rate); }});
    d.push_back({"gm_hunt", [=] { return gmLine(rate, 4, 3.4f, level::kCreature - 1.0f, 0.06f, 0.2f, 0.9f, 0.1f); }});
    d.push_back({"gm_announce", [=] { return gmAnnounce(rate); }});
    d.push_back({"gm_grip", [=] { return gmGrip(rate); }});
    d.push_back({"gm_compose", [=] { return gmCompose(rate); }});
    d.push_back({"gm_light", [=] { return gmLine(rate, 5, 2.4f, level::kCreature - 2.0f, 0.06f, 0.18f, 0.95f, 0.12f); }});
    d.push_back({"gm_crack", [=] { return gmCrack(rate); }});
    d.push_back({"gm_gone", [=] { return gmGone(rate); }});
    d.push_back({"lib_wrath", [=] { return libWrath(rate); }});
    d.push_back({"lib_creak", [=] { return libCreak(rate); }});
    d.push_back({"lib_dicta", [=] { return libDicta(rate); }});
}

}  // namespace lim::audio
