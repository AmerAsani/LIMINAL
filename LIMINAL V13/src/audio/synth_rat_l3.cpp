// V13.3 Level 3 (KREATUR_DeathRat.md, KREATUR_Deathmoth.md): Klaenge der grossen Death Rats, ihrer Fallen, des Mottenschwarms
// und der Praetorianer. Alles synthetisch, keine Aufnahmen.
//
//   rat_tts_0..11  das Sprachgeraet: Relais klickt, Rauschen, dann eine Maschinenstimme (Formant-Synthesizer aus
//                  synth_kin_l3.cpp, Grundton fest, Silben gleich lang) durch einen winzigen Lautsprecher: Bandpass 380-3300 Hz,
//                  8-kHz-Abtastung mit 6 Bit (billiger Wandler), Klirren, 50-Hz-Brumm, am Ende wieder das Relais
//   rat_grind      Zaehne an Stahl (Gitterstaebe): Haft-Gleit-Kreischen mit Metallresonanz, drei, vier Zuege
//   rat_chatter    Zaehneklappern (schnelle harte Klicks)   rat_hiss    Zischen mit tiefem Knurren (Drohung)
//   rat_bite_big   Biss: zwei harte Klicks, nasses Reissen, dumpfer Stoss
//   rat_step_big   schwerer gepolsterter Tritt mit zwei Krallenklicks
//   rat_tinker     Fallenbau: Draht wird verdrillt, Klicken, ein kleiner Funke
//   trap_crackle   Ankuendigung der Falle: Funkenknistern und anschwellendes Brummen (100 Hz)
//   trap_zap       der Schlag: Knall, harter Lichtbogen (Brumm voller Obertoene), Knistern, Plopp
//   trap_fizzle    versagt: Spucken, Zischen, der Draht reisst (Sirren)
//   moth_swarm_rise  der Schwarm erhebt sich: viele Fluegel, dichter werdend, ein tiefes Brummen
//   moth_face      Motten im Gesicht: dichtes, dumpfes Flattern im Ohr, kratzende Beine, feucht
//   moth_shake     eine Motte wird abgeschuettelt          prae_click  Chitin-Spitze auf Beton (zwei Ticks)
//   prae_fan       Fluegelstummel faecheln: trockenes Schnarren, Pfeifen     prae_rear  Zischen und Mandibelklicken (Drohung)
//   prae_stab      Stich: Luftstoss, Chitinschlag, nasses Durchstossen    prae_spit  Gurgeln, Spruehen, Zischen der Saeure
#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

float kinSpeakMachine(kit::Buf& b, float at, const char* arpabet, float f0, float tempo, unsigned seed);

using namespace kit;

namespace {
const char* const kTts[12] = {
    "W IY1 | S IY1 | Y UW1 |",                         // WE SEE YOU
    "T ER1 N | AO1 F | Y AO1 R | L AY1 T |",           // TURN OFF YOUR LIGHT
    "K IY1 P | W AO1 K IH0 NG |",                      // KEEP WALKING
    "D UW1 | N AA1 T | R AH1 N |",                     // DO NOT RUN
    "Y UW1 | K EY1 M | B AE1 K |",                     // YOU CAME BACK
    "W IY1 | R IH0 M EH1 M B ER0 | Y UW1 |",           // WE REMEMBER YOU
    "S T AA1 P |",                                     // STOP
    "N AW1 |",                                         // NOW
    "K L EH1 V ER0 |",                                 // CLEVER
    "IH1 T | OW1 N L IY0 | HH ER1 T S | W AH1 N S |",  // IT ONLY HURTS ONCE
    "DH IH1 S | W EY1 | IH1 Z | S EY1 F |",            // THIS WAY IS SAFE
    "G OW1 |",                                         // GO
};

// Rauschen durch einen Bandpass (Huellkurve env ueber 0..1)
void noiseBand(Buf& b, float at, float len, float hz, float q, float amp, Rand& r, const std::function<float(float)>& env) {
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) b.x[a + k] += bp.run(r.bi()) * env((float)k / (float)n) * amp;
}
// Funken: einzelne, sehr kurze Rauschknalle (hell)
void sparks(Buf& b, float t0, float t1, float perSec, float amp, Rand& r) {
    grains(b, t0, t1, perSec, [&](Buf& g, float at, Rand& rr) {
        const size_t a = g.at(at), n = (size_t)(g.rate * rr.range(0.0008f, 0.004f));
        const float aa = amp * rr.range(0.3f, 1.0f);
        for (size_t k = 0; k < n && a + k < g.n(); ++k) g.x[a + k] += rr.bi() * aa * (1.0f - (float)k / (float)n);
    }, r);
}
// Brumm aus dem Netz: 100 Hz mit Obertoenen (gleichgerichtet, verzerrt)
void buzz(Buf& b, float at, float len, float hz, float amp, float harsh, const std::function<float(float)>& env) {
    const size_t a = b.at(at), n = b.at(len);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float t = (float)k / (float)b.rate;
        float v = std::sin(kTwoPi * hz * t);
        v = std::tanh(v * (1.0f + 8.0f * harsh));
        b.x[a + k] += v * env((float)k / (float)n) * amp;
    }
}
// Klick (hart): Keramik- oder Kunststoffmoden
void click(Buf& b, float at, float hz, float amp, Rand& r, bool hard = true) {
    ImpactSpec s;
    s.modes = hard ? modesCeramic(hz, 0.03f, r) : modesPlastic(hz, 0.02f, r);
    s.contact = 0.0006f, s.noise = 0.5f, s.noiseHz = hz * 1.3f;
    b.add(impact(s, 0.8f, 0.08f, b.rate, r), at, amp);
}
// Fluegelschlaege: Rauschstoesse mit der Schlagfrequenz
void wings(Buf& b, float t0, float len, float hz, float amp, Rand& r, float bpHz, const std::function<float(float)>& env) {
    Biquad bp = Biquad::make(Biquad::BP, bpHz, 0.6f, (float)b.rate);
    const size_t a = b.at(t0), n = b.at(len);
    const float ph = r.range(0.0f, 6.0f);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float t = (float)k / (float)b.rate;
        const float beat = std::pow(0.5f + 0.5f * std::sin(kTwoPi * hz * t + ph), 3.0f);
        b.x[a + k] += bp.run(r.bi()) * beat * env((float)k / (float)n) * amp;
    }
}

Sound ratTts(int rate, int k) {
    Rand r(31000u + (unsigned)k * 17u);
    Buf v(3.2f, rate);
    const float end = kinSpeakMachine(v, 0.14f, kTts[k], 94.0f, 0.82f, 31100u + (unsigned)k);
    // winziger Lautsprecher: Bandpass, billiger Wandler (8 kHz, 6 Bit), Klirren
    filter(v, Biquad::HP, 380.0f, 0.9f);
    filter(v, Biquad::LP, 3300.0f, 0.9f);
    filter(v, Biquad::Peak, 1800.0f, 2.0f, 6.0f);  // Resonanz der Membran
    float peak = 1e-4f;
    for (float x : v.x) peak = std::max(peak, std::fabs(x));
    const int hold = std::max(1, rate / 8000);
    float held = 0.0f;
    for (size_t i = 0; i < v.n(); ++i) {
        if (i % (size_t)hold == 0) held = std::round(v.x[i] / peak * 31.0f) / 31.0f;
        v.x[i] = std::tanh(held * 2.2f) * 0.8f;
    }
    Buf b(end + 0.5f, rate);
    b.add(v, 0.0f);
    // Relais vorn und hinten, Rauschen beim Einschalten, Netzbrumm darunter
    click(b, 0.02f, 2200.0f, 0.5f, r, false);
    noiseBand(b, 0.04f, 0.1f, 2500.0f, 0.5f, 0.12f, r, [](float u) { return 1.0f - u; });
    buzz(b, 0.04f, end + 0.1f, 50.0f, 0.025f, 0.3f, [](float u) { return std::min(1.0f, u * 20.0f) * std::min(1.0f, (1.0f - u) * 20.0f); });
    click(b, end + 0.16f, 1900.0f, 0.4f, r, false);
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}

Sound ratGrind(int rate) {
    Rand r(31200u);
    Buf b(1.7f, rate);
    // drei, vier Zuege der Zaehne ueber den Stab: Haft-Gleiten (Impulse) regt die Metallmoden an
    const int strokes = 4;
    for (int s = 0; s < strokes; ++s) {
        const float t0 = 0.05f + (float)s * 0.38f + r.range(-0.03f, 0.03f), len = r.range(0.22f, 0.3f);
        CreakSpec ck;
        ck.rate0 = r.range(220.0f, 300.0f), ck.rate1 = r.range(380.0f, 520.0f), ck.jitter = 0.35f;
        ck.formants = {r.range(1300.0f, 1700.0f), r.range(3100.0f, 3600.0f), r.range(5200.0f, 6100.0f)};
        ck.q = 14.0f;
        creak(b, t0, len, ck, 0.75f, r);
        ImpactSpec m;
        m.modes = modesMetalPipe(r.range(520.0f, 640.0f), 0.5f, r);
        m.contact = 0.002f, m.noise = 0.3f, m.noiseHz = 3000.0f;
        b.add(impact(m, 0.4f, 0.6f, rate, r), t0 + len * 0.8f, 0.35f);
    }
    filter(b, Biquad::HP, 300.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound ratChatter(int rate) {
    Rand r(31300u);
    Buf b(0.7f, rate);
    const int n = r.irange(9, 13);
    for (int i = 0; i < n; ++i) click(b, 0.02f + (float)i * r.range(0.038f, 0.05f), r.range(2600.0f, 3400.0f), r.range(0.4f, 0.8f), r);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound ratHiss(int rate) {
    Rand r(31400u);
    Buf b(1.1f, rate);
    noiseBand(b, 0.0f, 1.0f, 3200.0f, 0.7f, 0.9f, r, [](float u) { return std::min(1.0f, u * 8.0f) * (1.0f - u * u); });
    VoiceSpec v;
    v.f0 = [](float t) { return 68.0f - 10.0f * t; };
    v.amp = [](float t) { return std::min(1.0f, t * 6.0f) * std::max(0.0f, 1.0f - t * 1.1f); };
    v.formants = {380.0f, 900.0f, 2300.0f}, v.subharm = 0.6f, v.rough = 0.5f, v.breath = 0.5f;
    voice(b, 0.05f, 0.9f, v, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound ratBite(int rate) {
    Rand r(31500u);
    Buf b(0.6f, rate);
    click(b, 0.0f, 2900.0f, 1.0f, r);
    click(b, 0.035f, 2500.0f, 0.9f, r);
    noiseBand(b, 0.04f, 0.18f, 1100.0f, 0.8f, 0.6f, r, [](float u) { return (1.0f - u) * (1.0f - u); });  // nasses Reissen
    ImpactSpec th;
    th.modes = modesConcrete(120.0f, r);
    th.contact = 0.01f, th.noise = 0.2f, th.noiseHz = 400.0f;
    b.add(impact(th, 0.8f, 0.3f, rate, r), 0.01f, 0.6f);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound ratStep(int rate) {
    Rand r(31600u);
    Buf b(0.4f, rate);
    ImpactSpec th;
    th.modes = modesConcrete(r.range(90.0f, 120.0f), r);
    th.contact = 0.022f, th.noise = 0.35f, th.noiseHz = 500.0f;  // weich gepolstert
    b.add(impact(th, 0.9f, 0.3f, rate, r), 0.0f, 1.0f);
    noiseBand(b, 0.01f, 0.09f, 1800.0f, 0.6f, 0.12f, r, [](float u) { return 1.0f - u; });  // Schaben
    click(b, 0.03f, 3600.0f, 0.18f, r);
    click(b, 0.055f, 3300.0f, 0.12f, r);
    filter(b, Biquad::LP, 5000.0f, 0.7f);
    trim(b);
    normalize(b, level::kStep + 2.0f);
    return b.sound();
}

Sound ratTinker(int rate) {
    Rand r(31700u);
    Buf b(1.4f, rate);
    CreakSpec ck;  // Draht wird verdrillt
    ck.rate0 = 60.0f, ck.rate1 = 140.0f, ck.formants = {2400.0f, 4600.0f, 7000.0f}, ck.q = 10.0f;
    creak(b, 0.05f, 0.5f, ck, 0.4f, r);
    for (int i = 0; i < 5; ++i) {  // Klicken: Klemme, Schraube, Federblech
        ImpactSpec m;
        m.modes = modesMetalPlate(r.range(1800.0f, 3200.0f), 0.08f, r);
        m.contact = 0.0008f, m.noise = 0.3f, m.noiseHz = 5000.0f;
        b.add(impact(m, 0.6f, 0.15f, rate, r), r.range(0.55f, 1.15f), 0.5f);
    }
    sparks(b, 1.12f, 1.22f, 90.0f, 0.5f, r);
    filter(b, Biquad::HP, 400.0f, 0.7f);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound trapCrackle(int rate) {
    Rand r(31800u);
    Buf b(0.8f, rate);
    sparks(b, 0.02f, 0.5f, 140.0f, 0.9f, r);
    buzz(b, 0.0f, 0.7f, 100.0f, 0.18f, 0.6f, [](float u) { return std::sin(3.14159f * u); });
    filter(b, Biquad::HP, 120.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

Sound trapZap(int rate) {
    Rand r(31900u);
    Buf b(1.3f, rate);
    // Knall des Ueberschlags
    noiseBand(b, 0.0f, 0.03f, 4000.0f, 0.4f, 1.0f, r, [](float u) { return 1.0f - u; });
    // Lichtbogen: harter Brumm (100 Hz, stark verzerrt), darin dichtes Knistern
    buzz(b, 0.0f, 0.75f, 100.0f, 0.55f, 1.0f, [](float u) { return u < 0.05f ? u / 0.05f : (1.0f - u) * (0.8f + 0.2f * std::sin(u * 60.0f)); });
    buzz(b, 0.0f, 0.75f, 300.0f, 0.15f, 1.0f, [](float u) { return 1.0f - u; });
    sparks(b, 0.0f, 0.8f, 600.0f, 0.8f, r);
    // Plopp am Ende (die Sicherung der Batterie)
    ImpactSpec p;
    p.modes = modesPlastic(700.0f, 0.06f, r);
    p.contact = 0.002f, p.noise = 0.5f, p.noiseHz = 1500.0f;
    b.add(impact(p, 1.0f, 0.3f, rate, r), 0.8f, 0.7f);
    trim(b);
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}

Sound trapFizzle(int rate) {
    Rand r(32000u);
    Buf b(1.1f, rate);
    sparks(b, 0.0f, 0.35f, 60.0f, 0.7f, r);
    noiseBand(b, 0.05f, 0.6f, 5000.0f, 0.5f, 0.35f, r, [](float u) { return (1.0f - u) * (0.6f + 0.4f * std::sin(u * 40.0f)); });
    ImpactSpec tw;  // der Draht reisst und sirrt
    tw.modes = modesMetalPipe(r.range(900.0f, 1200.0f), 0.7f, r);
    tw.contact = 0.0008f, tw.noise = 0.2f, tw.noiseHz = 6000.0f;
    b.add(impact(tw, 0.9f, 0.8f, rate, r), 0.3f, 0.6f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound mothSwarmRise(int rate) {
    Rand r(32100u);
    Buf b(2.6f, rate);
    for (int i = 0; i < 14; ++i) {
        const float t0 = r.range(0.0f, 1.4f);
        wings(b, t0, r.range(0.8f, 1.2f), r.range(14.0f, 26.0f), r.range(0.25f, 0.5f), r, r.range(500.0f, 1300.0f),
              [](float u) { return std::sin(3.14159f * u); });
    }
    VoiceSpec h;  // tiefes Brummen der Masse
    h.f0 = [](float t) { return 85.0f + 25.0f * t; };
    h.amp = [](float t) { return std::min(1.0f, t * 1.2f) * std::max(0.0f, 1.0f - (t - 1.8f) * 2.0f); };
    h.formants = {300.0f, 700.0f, 1600.0f}, h.breath = 0.7f, h.rough = 0.4f;
    voice(b, 0.0f, 2.4f, h, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound mothFace(int rate) {
    Rand r(32200u);
    Buf b(1.15f, rate);
    for (int i = 0; i < 6; ++i)
        wings(b, r.range(0.0f, 0.2f), r.range(0.8f, 0.95f), r.range(22.0f, 36.0f), 0.6f, r, r.range(250.0f, 600.0f),
              [](float u) { return std::min(1.0f, u * 10.0f) * std::min(1.0f, (1.0f - u) * 6.0f); });
    for (int i = 0; i < 10; ++i) click(b, r.range(0.0f, 1.0f), r.range(3000.0f, 4500.0f), 0.08f, r);  // Beine kratzen
    filter(b, Biquad::LP, 1800.0f, 0.7f);  // im Ohr: dumpf
    noiseBand(b, 0.2f, 0.4f, 900.0f, 1.2f, 0.15f, r, [](float u) { return std::sin(3.14159f * u); });  // feucht
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

Sound mothShake(int rate) {
    Rand r(32300u);
    Buf b(0.5f, rate);
    wings(b, 0.0f, 0.35f, 28.0f, 0.7f, r, 900.0f, [](float u) { return 1.0f - u; });
    click(b, 0.3f, 1500.0f, 0.2f, r, false);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound praeClick(int rate) {
    Rand r(32400u);
    Buf b(0.2f, rate);
    click(b, 0.0f, r.range(2600.0f, 3200.0f), 1.0f, r);
    click(b, 0.032f, r.range(2200.0f, 2800.0f), 0.6f, r);
    trim(b);
    normalize(b, level::kStep);
    return b.sound();
}

Sound praeFan(int rate) {
    Rand r(32500u);
    Buf b(1.7f, rate);
    // trockenes Schnarren: Papier-Rauschen, 16 Hz moduliert
    wings(b, 0.0f, 1.5f, 16.0f, 0.8f, r, 2600.0f, [](float u) { return std::min(1.0f, u * 6.0f) * std::min(1.0f, (1.0f - u) * 4.0f); });
    wings(b, 0.05f, 1.4f, 16.5f, 0.5f, r, 4800.0f, [](float u) { return std::sin(3.14159f * u); });
    // ein duennes Pfeifen (Luft durch die Duftpinsel)
    const size_t a = b.at(0.2f), n = b.at(1.1f);
    for (size_t k = 0; k < n && a + k < b.n(); ++k) {
        const float u = (float)k / (float)n, t = (float)k / (float)rate;
        b.x[a + k] += std::sin(kTwoPi * (2400.0f + 200.0f * std::sin(t * 7.0f)) * t) * std::sin(3.14159f * u) * 0.03f;
    }
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}

Sound praeRear(int rate) {
    Rand r(32600u);
    Buf b(1.0f, rate);
    noiseBand(b, 0.0f, 0.85f, 3800.0f, 0.8f, 0.9f, r, [](float u) { return std::min(1.0f, u * 5.0f) * (1.0f - 0.6f * u); });
    const int n = 14;
    for (int i = 0; i < n; ++i) click(b, 0.05f + (float)i * (0.06f - 0.0025f * (float)i), r.range(3200.0f, 4200.0f), 0.5f + 0.03f * (float)i, r);
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

Sound praeStab(int rate) {
    Rand r(32700u);
    Buf b(0.6f, rate);
    noiseBand(b, 0.0f, 0.12f, 900.0f, 0.5f, 0.6f, r, [](float u) { return std::sin(3.14159f * u); });  // Luftstoss
    click(b, 0.1f, 1800.0f, 1.0f, r);
    noiseBand(b, 0.11f, 0.2f, 700.0f, 0.9f, 0.7f, r, [](float u) { return (1.0f - u) * (1.0f - u); });  // nasses Durchstossen
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound praeSpit(int rate) {
    Rand r(32800u);
    Buf b(1.3f, rate);
    for (int i = 0; i < 9; ++i) bubble(b, r.range(0.0f, 0.3f), r.range(1.5f, 4.0f), 0.3f, r);  // Gurgeln in der Druese
    noiseBand(b, 0.3f, 0.18f, 2500.0f, 0.5f, 0.8f, r, [](float u) { return std::sin(3.14159f * u); });  // Spruehen
    noiseBand(b, 0.42f, 0.8f, 6000.0f, 0.6f, 0.3f, r, [](float u) { return (1.0f - u) * (0.7f + 0.3f * std::sin(u * 90.0f)); });  // Zischen
    sparks(b, 0.45f, 1.1f, 50.0f, 0.15f, r);  // Blasen platzen auf der Haut
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}
}  // namespace

void addRatL3Sounds(std::vector<SoundDef>& d, int rate) {
    for (int k = 0; k < 12; ++k) d.push_back({std::format("rat_tts_{}", k), [=] { return ratTts(rate, k); }});
    d.push_back({"rat_grind", [=] { return ratGrind(rate); }});
    d.push_back({"rat_chatter", [=] { return ratChatter(rate); }});
    d.push_back({"rat_hiss", [=] { return ratHiss(rate); }});
    d.push_back({"rat_bite_big", [=] { return ratBite(rate); }});
    d.push_back({"rat_step_big", [=] { return ratStep(rate); }});
    d.push_back({"rat_tinker", [=] { return ratTinker(rate); }});
    d.push_back({"trap_crackle", [=] { return trapCrackle(rate); }});
    d.push_back({"trap_zap", [=] { return trapZap(rate); }});
    d.push_back({"trap_fizzle", [=] { return trapFizzle(rate); }});
    d.push_back({"moth_swarm_rise", [=] { return mothSwarmRise(rate); }});
    d.push_back({"moth_face", [=] { return mothFace(rate); }});
    d.push_back({"moth_shake", [=] { return mothShake(rate); }});
    d.push_back({"prae_click", [=] { return praeClick(rate); }});
    d.push_back({"prae_fan", [=] { return praeFan(rate); }});
    d.push_back({"prae_rear", [=] { return praeRear(rate); }});
    d.push_back({"prae_stab", [=] { return praeStab(rate); }});
    d.push_back({"prae_spit", [=] { return praeSpit(rate); }});
}

}  // namespace lim::audio
