// V13.3 Teil 4: Klaenge von Jerry (Entity 7, KREATUR_Jerry.md). Die Seite: ein Ara, der spricht und denkt wie ein
// erwachsener Mensch; er springt mit einem Kreischen; der Unterworfene sagt danach ruhig, er verstehe. Alles Weitere ist
// Auslegung nach dem Koerper (Ara: Syrinx statt Kehlkopf, Hakenschnabel, Schwingen):
//
//   Sprechen   ruhige, deutliche Menschenstimme (Mann, f0 um 120 Hz) - Silben mit wechselnden Vokalen, Zischlaute,
//              Verschluss- und Nasallaute, Satzmelodie faellt; durch die Syrinx etwas gepresst und naeselnd (Formanten
//              hoeher, wenig Atem, ein feiner Schnarrton). Keine verstaendlichen Worte - man versteht nur, dass er spricht.
//   Murmeln    dasselbe leiser, langsamer, gehaucht, hinter einer Tuer (man hoert ihn vor dem Raum)
//   Kreischen  Ara-Schrei: sehr laut, rau, Periodenverdopplung, 0,6-0,9 s
//   Fluegel    schuetteln und strecken (Rascheln) bzw. ausbreiten und schlagen (drei, vier Luftstoesse)
//   Putzen     Federn rascheln, kleine Schnabelklicks; Knirschen: rasches feines Reiben der Schnabelhaelften (Papageien
//              knirschen, wenn sie zufrieden sind)
//   Bann       Einsetzen: ein Klingeln schwillt im Kopf, tiefes Anschwellen, Fluestern; Schleife: anhaltendes Klingeln mit
//              Schwebung und vielen leisen Stimmen (die Followers - Auslegung)
//   Zugriff    ganz nah und ruhig gesprochen, ein Schlag der Schwingen, dumpfer Herzschlag
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

void hiss(Buf& b, float at, float len, float hz, float amp, Rand& r) {  // Zischlaut (s, sch, f)
    Biquad bp = Biquad::make(Biquad::BP, hz, 1.6f, (float)b.rate);
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        const float u = (b.t(i) - at) / len;
        b.x[i] += bp.run(r.bi()) * amp * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void plosive(Buf& b, float at, float amp, Rand& r) {  // Verschlusslaut (t, k, p): kurzer Knack mit Nachrauschen
    Biquad bp = Biquad::make(Biquad::BP, r.range(1800.0f, 4200.0f), 1.2f, (float)b.rate);
    const size_t a = b.at(at);
    for (size_t k = 0; k < (size_t)(0.02f * (float)b.rate) && a + k < b.n(); ++k)
        b.x[a + k] += bp.run(r.bi()) * amp * std::exp(-(float)k / (0.004f * (float)b.rate));
}
void rustle(Buf& b, float t0, float len, float amp, float hz, Rand& r) {  // Federn: Rauschen in Stoessen
    Biquad bp = Biquad::make(Biquad::BP, hz, 0.7f, (float)b.rate);
    float g = 0.0f, tgt = 0.0f;
    for (size_t i = b.at(t0); i < b.at(t0 + len) && i < b.n(); ++i) {
        if (r.chance(45.0f / (float)b.rate)) tgt = r.range(0.1f, 1.0f);
        g += (tgt - g) * 0.003f;
        const float u = (b.t(i) - t0) / len;
        b.x[i] += bp.run(r.bi()) * amp * g * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void whoosh(Buf& b, float at, float len, float amp, Rand& r) {  // Fluegelschlag: Luftstoss, tief, mit Federknistern
    Biquad lp = Biquad::make(Biquad::LP, r.range(500.0f, 800.0f), 0.7f, (float)b.rate);
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        const float u = (b.t(i) - at) / len;
        const float env = u < 0.3f ? u / 0.3f : std::exp(-(u - 0.3f) * 5.0f);
        b.x[i] += lp.run(r.bi()) * amp * env * 2.2f;
    }
    rustle(b, at, len * 0.8f, amp * 0.35f, 3800.0f, r);
}
void clickB(Buf& b, float at, float amp, Rand& r) {  // Schnabel: hartes Horn
    ImpactSpec sp;
    sp.modes = modesWood(r.range(2200.0f, 3400.0f), 0.01f, r);
    sp.contact = 0.0006f, sp.noise = 0.6f, sp.noiseHz = 5200.0f, sp.noiseT60 = 0.005f;
    b.add(impact(sp, amp, 0.04f, b.rate, r), at);
}

// gesprochener Satz: Silben mit Vokal, davor ein Konsonant; Satzmelodie (Deklination, Betonungen); breath > 0.5: gehaucht
void speech(Buf& b, float t0, float len, float f0, float breath, float amp, Rand& r, float tempo = 1.0f) {
    float t = t0;
    int syl = 0;
    while (t < t0 + len - 0.1f) {
        const int c = r.irange(0, 4);
        float cl = 0.0f;
        if (c == 0) cl = r.range(0.05f, 0.08f), hiss(b, t, cl, r.range(4800.0f, 6800.0f), 0.12f * amp, r);
        else if (c == 1) cl = 0.025f, plosive(b, t + 0.005f, 0.5f * amp, r);
        else if (c == 2) cl = r.range(0.04f, 0.06f), hiss(b, t, cl, r.range(2400.0f, 3200.0f), 0.08f * amp, r);
        const float sl = r.range(0.1f, 0.2f) / tempo;
        const Vow& w = kVow[r.irange(0, 6)];
        const float u0 = (t - t0) / len;
        const float accent = (syl % 3 == 0) ? r.range(0.06f, 0.14f) : 0.0f;
        const float fa = f0 * (1.08f - 0.18f * u0 + accent), fb = fa * r.range(0.92f, 1.02f);
        VoiceSpec v;
        v.f0 = [=](float tt) { return fa + (fb - fa) * std::clamp(tt / sl, 0.0f, 1.0f); };
        v.amp = [=](float tt) { return amp * envAHR(tt, 0.02f, sl * 0.6f, sl * 0.35f); };
        const float k = 1.1f;  // Syrinx: etwas gepresst, Formanten hoeher
        v.formants = {w.f1 * k, w.f2 * k, w.f3 * k};
        v.bandwidths = {70.0f, 100.0f, 150.0f};
        v.jitter = 0.012f, v.shimmer = 0.06f, v.subharm = 0.04f, v.breath = breath, v.rough = 0.12f, v.roughHz = 52.0f, v.tiltHz = 5200.0f;
        if (c == 3) {  // Nasal: n, m davor
            VoiceSpec nv = v;
            nv.formants = {280.0f, 1250.0f, 2600.0f};
            nv.amp = [=](float tt) { return 0.5f * amp * envAHR(tt, 0.01f, 0.03f, 0.02f); };
            voice(b, t, 0.06f, nv, r);
            cl = 0.05f;
        }
        voice(b, t + cl, sl, v, r);
        t += cl + sl + r.range(0.0f, 0.03f);
        if (r.chance(0.22f)) t += r.range(0.07f, 0.14f);  // Wortgrenze
        ++syl;
    }
}

Sound jTalk(int rate, int var) {
    Rand r(70000u + (unsigned)var * 17u);
    const float L = r.range(1.4f, 1.9f);
    Buf b(L + 0.6f, rate);
    speech(b, 0.05f, L, r.range(112.0f, 132.0f), 0.08f, 1.0f, r);
    reverb(b, dsp::RoomSpec{0.5f, 0.4f, 0.25f, 0.5f}, 0.18f, 1.0f, 7001u + (unsigned)var);
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}

Sound jMutter(int rate, int var) {
    Rand r(70100u + (unsigned)var * 13u);
    const float L = r.range(1.2f, 1.6f);
    Buf b(L + 0.8f, rate);
    speech(b, 0.05f, L, r.range(104.0f, 120.0f), 0.35f, 0.8f, r, 0.8f);
    distantize(b, 2600.0f, 0.5f, 7101u + (unsigned)var);  // hinter Waenden, in einem Raum
    trim(b);
    normalize(b, level::kCreature - 7.0f);
    return b.sound();
}

Sound jScreech(int rate, int var) {
    Rand r(70200u + (unsigned)var);
    const float L = r.range(0.6f, 0.85f);
    Buf b(L + 0.4f, rate);
    VoiceSpec v;
    const float f0a = r.range(980.0f, 1150.0f), f0b = f0a * r.range(0.72f, 0.82f);
    v.f0 = [=](float t) { return f0a + (f0b - f0a) * std::clamp(t / L, 0.0f, 1.0f) + 60.0f * std::sin(t * 38.0f); };
    v.amp = [=](float t) { return envAHR(t, 0.015f, L * 0.6f, L * 0.35f); };
    v.formants = {1500.0f, 2900.0f, 4300.0f};
    v.bandwidths = {260.0f, 380.0f, 520.0f};
    v.jitter = 0.08f, v.shimmer = 0.3f, v.subharm = 0.55f, v.breath = 0.3f, v.rough = 0.8f, v.roughHz = 41.0f, v.tiltHz = 7500.0f;
    voice(b, 0.0f, L, v, r);
    reverb(b, dsp::RoomSpec{0.6f, 0.5f, 0.3f, 0.6f}, 0.25f, 1.0f, 7201u + (unsigned)var);
    trim(b);
    normalize(b, level::kCreature + 6.0f);
    return b.sound();
}

Sound jFlap(int rate, int var) {
    Rand r(70300u + (unsigned)var);
    if (var == 0) {  // schuetteln, strecken
        Buf b(0.9f, rate);
        rustle(b, 0.0f, 0.7f, 0.5f, 3000.0f, r);
        whoosh(b, 0.25f, 0.22f, 0.25f, r);
        trim(b);
        normalize(b, level::kCreature - 8.0f);
        return b.sound();
    }
    Buf b(1.3f, rate);  // ausbreiten und schlagen
    rustle(b, 0.0f, 0.3f, 0.5f, 3400.0f, r);
    for (int i = 0; i < 4; ++i) whoosh(b, 0.18f + 0.2f * (float)i + r.range(-0.02f, 0.02f), 0.2f, 0.8f - 0.12f * (float)i, r);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound jPreen(int rate) {
    Rand r(70400u);
    Buf b(1.3f, rate);
    rustle(b, 0.0f, 1.2f, 0.6f, 4200.0f, r);
    for (float t = 0.1f; t < 1.1f; t += r.range(0.06f, 0.18f)) clickB(b, t, r.range(0.2f, 0.5f), r);
    trim(b);
    normalize(b, level::kCreature - 12.0f);
    return b.sound();
}

Sound jGrind(int rate) {
    Rand r(70500u);
    Buf b(1.0f, rate);
    for (float t = 0.0f; t < 0.85f; t += r.range(0.012f, 0.03f)) clickB(b, t, r.range(0.1f, 0.35f) * std::sin(3.14159f * t / 0.85f), r);
    trim(b);
    normalize(b, level::kCreature - 13.0f);
    return b.sound();
}

// Klingeln im Kopf: zwei nahe Sinustoene (Schwebung), dazu ein hoher Oberton
void ringing(Buf& b, float t0, float len, float amp, float fadeIn) {
    for (size_t i = b.at(t0); i < b.at(t0 + len) && i < b.n(); ++i) {
        const float t = b.t(i) - t0;
        const float e = std::min(1.0f, t / std::max(0.01f, fadeIn));
        const float s = std::sin(6.2831853f * 3150.0f * t) + 0.8f * std::sin(6.2831853f * 3153.5f * t) + 0.25f * std::sin(6.2831853f * 6302.0f * t);
        b.x[i] += s * amp * e;
    }
}

Sound jSpellOn(int rate) {
    Rand r(70600u);
    Buf b(2.4f, rate);
    ringing(b, 0.0f, 2.2f, 0.06f, 0.5f);
    for (size_t i = 0; i < b.n(); ++i) {  // tiefes Anschwellen
        const float t = b.t(i);
        b.x[i] += std::sin(6.2831853f * 44.0f * t) * 0.5f * envAHR(t, 0.6f, 0.4f, 1.0f);
    }
    speech(b, 0.4f, 1.4f, 118.0f, 0.95f, 0.5f, r, 0.9f);  // gefluestert
    fades(b, 0.01f, 0.4f);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

Sound jSpellLoop(int rate) {
    Rand r(70700u);
    const float L = 6.0f;
    Buf b(L, rate);
    ringing(b, 0.0f, L, 0.05f, 0.01f);
    for (int k = 0; k < 7; ++k)  // viele leise Stimmen, gefluestert, ueberlagert
        speech(b, r.range(0.0f, 1.5f), r.range(3.0f, 4.4f), r.range(100.0f, 190.0f), 0.97f, 0.22f, r, r.range(0.7f, 1.1f));
    reverb(b, dsp::RoomSpec{1.6f, 1.4f, 1.0f, 1.4f}, 0.4f, 0.8f, 7701u);
    loopify(b, 0.8f);
    normalize(b, level::kCreature - 8.0f);
    return b.sound();
}

Sound jSubdue(int rate) {
    Rand r(70800u);
    Buf b(3.0f, rate);
    whoosh(b, 0.0f, 0.3f, 0.9f, r);
    speech(b, 0.35f, 1.5f, 122.0f, 0.05f, 1.0f, r, 0.85f);  // ganz nah, ruhig
    for (float t : {0.4f, 1.3f, 2.2f}) {  // dumpfer Herzschlag
        for (size_t i = b.at(t); i < b.at(t + 0.25f) && i < b.n(); ++i) {
            const float u = b.t(i) - t;
            b.x[i] += std::sin(6.2831853f * 52.0f * u) * std::exp(-u * 18.0f) * 0.9f;
        }
    }
    ringing(b, 0.2f, 2.6f, 0.03f, 1.2f);
    normalize(b, level::kCreature + 2.0f);
    return b.sound();
}

}  // namespace

void addJerrySounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 6; ++v) d.push_back({std::format("jerry_talk_{}", v), [=] { return jTalk(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("jerry_mutter_{}", v), [=] { return jMutter(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("jerry_screech_{}", v), [=] { return jScreech(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("jerry_flap_{}", v), [=] { return jFlap(rate, v); }});
    d.push_back({"jerry_preen", [=] { return jPreen(rate); }});
    d.push_back({"jerry_grind", [=] { return jGrind(rate); }});
    d.push_back({"jerry_spell_on", [=] { return jSpellOn(rate); }});
    d.push_back({"jerry_spell", [=] { return jSpellLoop(rate); }});
    d.push_back({"jerry_subdue", [=] { return jSubdue(rate); }});
}

}  // namespace lim::audio
