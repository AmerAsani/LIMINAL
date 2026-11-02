// V13.3 Level 3 (KREATUR_RedKnight.md): Klaenge des Red Knight (Entity 71) und dessen, wovor er flieht. Die Seiten nennen nur
// das Klirren seiner Schritte (entity-71); alles andere ist Interpretation. Alles synthetisch:
//   rk_step    Sabaton auf Beton: harter Stahlschlag mit kurzem Nachklang, dumpfer Tritt, die Lamellen der Beinruestung
//              schlagen nach, Kettengeflecht klirrt
//   rk_breath  Keuchen im geschlossenen Helm: kurzes Ein- und Ausatmen, metallisch eng (Resonanzen des Visiers), ein Stoehnen
//   rk_halt    abruptes Bremsen: die Platten rasseln, Stahlsohlen schaben ueber den Boden
//   rk_slash   der Zweihaender durch leere Luft: tiefes Fauchen, das anschwillt und wieder faellt; am Ende knistert die
//              Luft, als risse sie ein wenig (der Hieb gilt etwas Unsichtbarem)
//   rk_rift    der Schnitt in die Luft: ein heller, steigender Riss, ein glasiger Akkord, ein tiefer Sog, dann schliesst es
//   rk_unseen  der Verfolger: ein Druck in den Ohren - tiefes, schwellendes Grollen, darueber ein feiner Ton wie Tinnitus, die
//              Luft wird dumpf (man hoert, dass da etwas fehlt)
#include <algorithm>
#include <cmath>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

void plateHit(Buf& b, float at, float f0, float amp, float t60, Rand& r) {
    ImpactSpec s;
    s.modes = modesMetalPlate(f0, t60, r);
    s.contact = 0.0015f, s.noise = 0.35f, s.noiseHz = 4200.0f;
    b.add(impact(s, amp, std::max(0.12f, t60 * 1.4f), b.rate, r), at);
}

// Kettengeflecht: viele winzige Ringe, die aneinander schlagen
void mail(Buf& b, float t0, float t1, float density, float amp, Rand& r) {
    grains(b, t0, t1, density, [&](Buf& g, float at, Rand& rr) {
        ImpactSpec s;
        s.modes = modesMetalPlate(rr.range(3800.0f, 7200.0f), 0.025f, rr);
        s.contact = 0.0004f, s.noise = 0.5f, s.noiseHz = 7000.0f;
        g.add(impact(s, amp * rr.range(0.3f, 1.0f), 0.05f, g.rate, rr), at);
    }, r);
}

Sound rkStep(int rate) {
    Rand r(71001u);
    Buf b(0.7f, rate);
    // Sabaton: Stahl auf Beton (hart, kurz klingend) und der dumpfe Tritt des Koerpers
    plateHit(b, 0.0f, r.range(780.0f, 900.0f), 1.0f, 0.09f, r);
    ImpactSpec th;
    th.modes = modesConcrete(r.range(95.0f, 120.0f), r);
    th.contact = 0.008f, th.noise = 0.5f, th.noiseHz = 600.0f;
    b.add(impact(th, 0.9f, 0.18f, rate, r), 0.003f);
    // die Lamellen von Knie und Schenkel schlagen nach, das Geflecht klirrt
    plateHit(b, 0.045f, r.range(1300.0f, 1500.0f), 0.45f, 0.12f, r);
    plateHit(b, 0.075f, r.range(1900.0f, 2200.0f), 0.3f, 0.1f, r);
    mail(b, 0.02f, 0.2f, 140.0f, 0.18f, r);
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}

Sound rkBreath(int rate) {
    Rand r(71002u);
    Buf b(1.4f, rate);
    breathe(b, 0.0f, 0.38f, true, 0.9f, 1.0f, r);
    breathe(b, 0.42f, 0.5f, false, 1.0f, 0.8f, r);
    // gequaeltes Stoehnen im Ausatmen (tief, rau)
    VoiceSpec v;
    v.f0 = [](float u) { return 118.0f - 20.0f * u; };
    v.amp = [](float u) { return envAHR(u, 0.15f, 0.4f, 0.45f) * 0.35f; };
    v.formants = {520.0f, 980.0f, 2500.0f};
    v.breath = 0.6f, v.rough = 0.4f, v.jitter = 0.04f;
    voice(b, 0.46f, 0.42f, v, r);
    // im Helm: eng, metallisch (Resonanzen des Visiers), ohne Hoehen
    filter(b, Biquad::Peak, 940.0f, 6.0f, 7.0f);
    filter(b, Biquad::Peak, 2150.0f, 7.0f, 5.0f);
    filter(b, Biquad::LP, 4200.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

Sound rkHalt(int rate) {
    Rand r(71003u);
    Buf b(0.9f, rate);
    grains(b, 0.0f, 0.45f, 38.0f, [&](Buf& g, float at, Rand& rr) { plateHit(g, at, rr.range(600.0f, 2400.0f), rr.range(0.3f, 0.8f), 0.08f, rr); }, r,
           [](float t) { return 1.0f - t / 0.45f; });
    mail(b, 0.0f, 0.5f, 220.0f, 0.2f, r);
    // Stahlsohlen schaben (Reibung, hell)
    dsp::Svf f;
    for (size_t i = 0; i < b.at(0.32f); ++i) {
        const float u = (float)i / (float)b.at(0.32f);
        f.set(1800.0f + 1200.0f * std::sin(u * 9.0f), 3.0f, (float)rate);
        b.x[i] += f.bp(r.bi()) * 0.5f * envAHR(u, 0.05f, 0.5f, 0.45f);
    }
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound rkSlash(int rate) {
    Rand r(71004u);
    Buf b(1.0f, rate);
    // Fauchen: Bandpass-Rauschen, Mitte und Lautheit schwellen an und fallen (die Klinge kommt nah vorbei)
    dsp::Svf f;
    const float L = 0.42f;
    for (size_t i = 0; i < b.at(L); ++i) {
        const float u = (float)i / (float)b.at(L);
        const float bell = std::exp(-std::pow((u - 0.55f) / 0.2f, 2.0f));
        f.set(260.0f + 1100.0f * bell, 1.4f, (float)rate);
        b.x[i] += f.bp(r.bi()) * (0.15f + 1.2f * bell);
    }
    // die Luft knistert, als risse sie ein wenig
    grains(b, 0.3f, 0.75f, 70.0f, [&](Buf& g, float at, Rand& rr) {
        Biquad hp = Biquad::make(Biquad::HP, rr.range(3000.0f, 6000.0f), 0.7f, (float)g.rate);
        const size_t a = g.at(at), n = g.at(rr.range(0.0005f, 0.002f));
        for (size_t i = 0; i < n + 30 && a + i < g.n(); ++i) g.x[a + i] += hp.run(rr.bi()) * (i < n ? 1.0f : std::exp(-(float)(i - n) * 0.2f)) * 0.25f;
    }, r, [](float t) { return std::max(0.0f, 1.0f - (t - 0.3f) / 0.45f); });
    trim(b);
    normalize(b, level::kEvent + 1.5f);
    return b.sound();
}

Sound rkRift(int rate) {
    Rand r(71005u);
    Buf b(2.8f, rate);
    // heller, steigender Riss
    dsp::Svf f;
    for (size_t i = 0; i < b.at(0.55f); ++i) {
        const float u = (float)i / (float)b.at(0.55f);
        f.set(500.0f * std::pow(12.0f, u), 2.2f, (float)rate);
        b.x[i] += f.bp(r.bi()) * (0.3f + 0.9f * u) * (0.7f + 0.3f * r.uni());
    }
    // glasiger Akkord (offen, dann verklingend)
    const float fr[5] = {1046.5f, 1318.5f, 1568.0f, 2093.0f, 2637.0f};
    for (int k = 0; k < 5; ++k) {
        float ph = 0.0f;
        const float det = r.range(-2.0f, 2.0f);
        for (size_t i = 0; i < b.at(2.2f); ++i) {
            const float t = b.t(i);
            ph += kTwoPi * (fr[k] + det) / (float)rate;
            b.x[b.at(0.45f) + i] += std::sin(ph) * envAD(t, 0.03f, 1.6f - 0.15f * (float)k) * 0.12f;
        }
    }
    // tiefer Sog (Druckabfall)
    float p = 0.0f;
    for (size_t i = 0; i < b.at(1.1f); ++i) {
        const float t = b.t(i), u = t / 1.1f;
        p += kTwoPi * (85.0f - 55.0f * u) / (float)rate;
        b.x[b.at(0.5f) + i] += std::sin(p) * envAHR(u, 0.08f, 0.35f, 0.57f) * 0.8f;
    }
    // schliesst: kurzes, sinkendes Zischen
    for (size_t i = 0; i < b.at(0.35f); ++i) {
        const float u = (float)i / (float)b.at(0.35f);
        f.set(5000.0f * std::pow(0.15f, u), 2.0f, (float)rate);
        b.x[b.at(1.7f) + i] += f.bp(r.bi()) * 0.5f * (1.0f - u);
    }
    trim(b);
    fades(b, 0.002f, 0.08f);
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}

Sound rkUnseen(int rate) {
    Rand r(71006u);
    Buf b(2.8f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 70.0f, 0.8f, (float)rate);
    float p1 = 0.0f, p2 = 0.0f, pt = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i), u = t / 2.8f;
        const float sw = std::sin(3.14159f * u);  // schwillt an und ab
        p1 += kTwoPi * 31.0f / (float)rate;
        p2 += kTwoPi * 33.5f / (float)rate;  // Schwebung 2,5 Hz: Druck
        pt += kTwoPi * 6150.0f / (float)rate;
        const float low = (0.6f * std::sin(p1) + 0.5f * std::sin(p2)) * sw * sw + lp.run(br.run(r.bi())) * 1.8f * sw;
        const float tin = std::sin(pt) * 0.018f * dsp::smooth01(std::min(1.0f, u * 3.0f)) * (u > 0.75f ? (1.0f - u) / 0.25f : 1.0f);
        b.x[i] = low + tin;
    }
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

}  // namespace

void addKnightSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"rk_step", [=] { return rkStep(rate); }});
    d.push_back({"rk_breath", [=] { return rkBreath(rate); }});
    d.push_back({"rk_halt", [=] { return rkHalt(rate); }});
    d.push_back({"rk_slash", [=] { return rkSlash(rate); }});
    d.push_back({"rk_rift", [=] { return rkRift(rate); }});
    d.push_back({"rk_unseen", [=] { return rkUnseen(rate); }});
}

}  // namespace lim::audio
