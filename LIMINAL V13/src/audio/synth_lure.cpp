// V13.3: Klaenge zu Leuchtstab und Ansprechen (KREATUR_Smiler.md, KREATUR_SkinStealer.md). Alles synthetisch, keine
// Sprachaufnahmen: die Saetze entstehen aus Lauten (Quelle-Filter-Stimme mit Formanten je Vokal, Reibe- und Verschlusslaute).
//   glow_crack          Leuchtstab knicken: Kunststoff biegt sich, die Glasampulle innen bricht, Schuetteln der Fluessigkeit
//   glow_throw          Wurf: Luftzug und Aermel
//   glow_land_0..2      Aufprall: hohler, leichter Kunststoff - Ticken, kurzes Nachspringen, Rollen
//   glow_splash         faellt ins Wasser
//   smiler_lured        der Smiler wendet sich dem Licht zu: das tiefe Droehnen biegt ab, Zaehne klicken nass
//   talk_call_0..3      der Spieler ruft (ruhig, fragend): "Hallo?", "Ist da wer?", "Hey ... hallo?", "Wer bist du?"
//   skin_reply_0..11    Antwort des Skin-Stealers auf Satz k (k*3 + Art): 0 falsch betont (nachgesprochen, Betonung und
//                       Melodie verkehrt, eine zweite Stimme schwingt mit), 1 abgehackt (Silben gekappt, gestottert, Tonsprung),
//                       2 verzoegert (gedehnt, absinkend wie ein langsamer werdendes Band, feucht)
//   skin_shriek         die Tarnung faellt: ein gepresster, unmenschlicher Schrei, Haut reisst
#include <cmath>
#include <format>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {
// --- Sprache aus Lauten ------------------------------------------------------------------------------------------------
// Laut: 'v' stimmhaft (Vokal, l, n - Formanten f1..f3), 'h' Hauch, 's' Zischlaut, 'f' stimmloser Reibelaut, 't' Verschluss
// mit hellem Knall, 'd'/'b' weicher Verschluss mit Stimmbalken, 'z' stimmhafter Reibelaut (w), 'p' Pause
struct Ph {
    char k = 'v';
    float len = 0.1f;
    float f1 = 500, f2 = 1500, f3 = 2500;
    float f0a = 1.0f, f0b = 1.0f;  // Grundton relativ zur Stimmlage (Anfang, Ende)
    float amp = 1.0f;
};
struct Syl {
    std::vector<Ph> ph;
};
using Phrase = std::vector<Syl>;

// Stimme: Lage, Formantverschiebung, Rauheit, Huellen (weich oder gekappt)
struct Voice {
    float f0 = 125.0f, fmul = 1.0f, jitter = 0.015f, shimmer = 0.08f, breath = 0.12f, rough = 0.0f, subharm = 0.0f;
    float att = 0.035f, rel = 0.05f;  // Ein- und Ausblenden je Laut (s)
    float vibrato = 0.0f, vibHz = 5.0f, droop = 0.0f;  // Vibrato (Anteil), Absinken ueber den ganzen Satz (Anteil)
    float gain = 1.0f;
};

Ph V(float len, float f1, float f2, float f3, float a0 = 1.0f, float a1 = 1.0f, float amp = 1.0f) { return {'v', len, f1, f2, f3, a0, a1, amp}; }
Ph C(char k, float len, float amp = 1.0f) { return {k, len, 0, 0, 0, 1, 1, amp}; }

// Die vier Saetze des Spielers (deutsche Laute, fragende Melodie: die letzte Silbe steigt)
Phrase phrase(int p) {
    switch (p & 3) {
        case 0:  // Hal-lo?
            return {{{C('h', 0.06f, 0.5f), V(0.15f, 760, 1250, 2550, 1.0f, 0.97f), V(0.06f, 360, 1050, 2600, 0.97f, 0.98f, 0.6f)}},
                    {{V(0.26f, 470, 860, 2450, 1.0f, 1.38f)}}};
        case 1:  // Ist - da - wer?
            return {{{V(0.08f, 400, 1900, 2550, 1.04f, 1.02f, 0.8f), C('s', 0.08f, 0.6f), C('t', 0.05f, 0.5f)}},
                    {{C('d', 0.03f, 0.5f), V(0.13f, 740, 1250, 2500, 1.02f, 0.98f)}},
                    {{C('z', 0.05f, 0.6f), V(0.13f, 420, 2050, 2650, 1.0f, 1.25f), V(0.12f, 600, 1300, 2500, 1.25f, 1.42f, 0.8f)}}};
        case 2:  // Hey ... hal-lo?
            return {{{C('h', 0.05f, 0.5f), V(0.10f, 560, 1800, 2550, 1.1f, 1.14f), V(0.10f, 350, 2200, 2800, 1.14f, 1.08f, 0.8f), C('p', 0.30f)}},
                    {{C('h', 0.05f, 0.45f), V(0.14f, 760, 1250, 2550, 1.04f, 1.0f, 0.9f), V(0.06f, 360, 1050, 2600, 1.0f, 1.0f, 0.55f)}},
                    {{V(0.25f, 470, 860, 2450, 1.03f, 1.4f, 0.9f)}}};
        default:  // Wer - bist - du?
            return {{{C('z', 0.05f, 0.6f), V(0.12f, 420, 2050, 2650, 1.1f, 1.05f), V(0.07f, 600, 1300, 2500, 1.05f, 1.0f, 0.75f)}},
                    {{C('b', 0.03f, 0.5f), V(0.08f, 400, 1900, 2550, 1.0f, 1.0f, 0.8f), C('s', 0.08f, 0.55f), C('t', 0.04f, 0.45f)}},
                    {{C('d', 0.03f, 0.5f), V(0.24f, 320, 800, 2300, 1.02f, 1.4f)}}};
    }
}

void noiseBand(Buf& b, float at, float len, float hz, float q, float amp, bool burst, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const float t = b.t(i);
        const float e = burst ? envAD(t, 0.002f, len * 0.7f) : envAHR(t, len * 0.3f, len * 0.35f, len * 0.35f);
        b.x[a + i] += bp.run(r.bi()) * amp * e;
    }
}

// spricht den Satz ab at; Rueckgabe: Ende (s). tAll: Gesamtdauer fuer das Absinken (droop)
float say(Buf& b, float at, const Phrase& phr, const Voice& vo, Rand& r, float tAll = 1.2f) {
    float t = at;
    for (const Syl& s : phr)
        for (const Ph& p : s.ph) {
            const float t0 = t;
            switch (p.k) {
                case 'v': {
                    VoiceSpec vs;
                    const float rel = (t0 - at) / std::max(0.1f, tAll), len = p.len + vo.rel;
                    const float base = vo.f0 * (1.0f - vo.droop * rel), dro = vo.droop * p.len / std::max(0.1f, tAll);
                    const float vib = vo.vibrato, vhz = vo.vibHz, a0 = p.f0a, a1 = p.f0b;
                    vs.f0 = [=](float u) { return base * (a0 + (a1 - a0) * u) * (1.0f - dro * u) * (1.0f + vib * std::sin(u * len * vhz * 6.2832f)); };
                    const float att = std::min(0.45f, vo.att / len), rl = std::min(0.45f, vo.rel / len), amp = p.amp * vo.gain;
                    vs.amp = [=](float u) { return amp * dsp::smooth01(u / att) * (1.0f - dsp::smooth01((u - (1.0f - rl)) / rl)); };
                    vs.formants = {p.f1 * vo.fmul, p.f2 * vo.fmul, p.f3 * vo.fmul, 3500.0f * vo.fmul};
                    vs.bandwidths = {80.0f, 110.0f, 170.0f, 250.0f};
                    vs.jitter = vo.jitter, vs.shimmer = vo.shimmer, vs.breath = vo.breath, vs.rough = vo.rough, vs.subharm = vo.subharm;
                    vs.tiltHz = 3200.0f;
                    voice(b, std::max(0.0f, t0 - vo.att * 0.6f), len, vs, r);
                    break;
                }
                case 'h': breathe(b, t0, p.len + 0.02f, false, 0.05f * p.amp * vo.gain, 0.4f, r); break;
                case 's': noiseBand(b, t0, p.len, 5600.0f * vo.fmul, 1.4f, 0.0045f * p.amp * vo.gain, false, r); break;
                case 'f': noiseBand(b, t0, p.len, 2400.0f * vo.fmul, 0.8f, 0.004f * p.amp * vo.gain, false, r); break;
                case 't': noiseBand(b, t0 + p.len * 0.6f, 0.02f, 3800.0f, 1.1f, 0.025f * p.amp * vo.gain, true, r); break;
                case 'd':
                case 'b': {
                    noiseBand(b, t0 + p.len * 0.7f, 0.012f, p.k == 'd' ? 2600.0f : 900.0f, 1.0f, 0.02f * p.amp * vo.gain, true, r);
                    VoiceSpec vs;
                    const float base = vo.f0;
                    vs.f0 = [=](float) { return base * 0.95f; };
                    vs.amp = [=](float u) { return 0.25f * p.amp * vo.gain * dsp::smooth01(u / 0.3f); };
                    vs.formants = {250.0f, 900.0f};
                    vs.bandwidths = {90.0f, 300.0f};
                    voice(b, t0, p.len * 0.8f, vs, r);
                    break;
                }
                case 'z': {
                    VoiceSpec vs;
                    const float base = vo.f0;
                    vs.f0 = [=](float) { return base; };
                    vs.amp = [=](float u) { return 0.35f * p.amp * vo.gain * dsp::smooth01(u / 0.3f) * (1.0f - dsp::smooth01((u - 0.7f) / 0.3f)); };
                    vs.formants = {280.0f * vo.fmul, 1300.0f * vo.fmul, 2400.0f * vo.fmul};
                    vs.bandwidths = {90.0f, 200.0f, 300.0f};
                    vs.breath = 0.05f;
                    voice(b, t0, p.len + 0.02f, vs, r);
                    noiseBand(b, t0, p.len, 3200.0f, 0.9f, 0.006f * p.amp * vo.gain, false, r);
                    break;
                }
                default: break;  // 'p' Pause
            }
            t += p.len;
        }
    return t;
}

float lengthOf(const Phrase& p) {
    float t = 0.0f;
    for (const Syl& s : p)
        for (const Ph& ph : s.ph) t += ph.len;
    return t;
}

// --- die Saetze ---------------------------------------------------------------------------------------------------------
Sound talkCall(int rate, int v) {
    Rand r(12100u + (unsigned)v);
    Buf b(2.0f, rate);
    Voice vo;
    vo.f0 = 124.0f + 6.0f * (float)(v % 2);  // eine ruhige, etwas belegte Stimme (gedaempft, man will nicht laut sein)
    vo.breath = 0.035f;
    const Phrase p = phrase(v);
    say(b, 0.03f, p, vo, r, lengthOf(p));
    filter(b, Biquad::HP, 90.0f, 0.7f);
    filter(b, Biquad::LP, 6500.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}

Sound skinReply(int rate, int idx) {
    const int p = idx / 3, style = idx % 3;
    Rand r(12200u + (unsigned)idx * 17u);
    Buf b(4.0f, rate);
    Phrase src = phrase(p);
    Voice vo;
    vo.f0 = 128.0f;  // ahmt die Lage nach - fast richtig
    float at = 0.03f;
    if (style == 0) {
        // falsch betont: die erste Silbe laut und hoch, die letzte faellt flach ab (Aussage statt Frage); erste Silbe zu lang,
        // letzte gekappt; der Vokaltrakt eine Spur zu klein (Formanten +8 %); kein Atem, kein Zittern - leblos glatt
        Phrase q = src;
        for (size_t s = 0; s < q.size(); ++s)
            for (Ph& ph : q[s].ph) {
                if (ph.k != 'v') continue;
                if (s == 0) ph.f0a = 1.42f, ph.f0b = 1.30f, ph.len *= 1.55f, ph.amp *= 1.15f;
                else if (s + 1 == q.size()) ph.f0a = 0.98f, ph.f0b = 0.80f, ph.len *= 0.6f, ph.amp *= 0.8f;
                else ph.f0a = ph.f0b = 1.0f;
            }
        vo.fmul = 1.08f, vo.jitter = 0.002f, vo.shimmer = 0.01f, vo.breath = 0.0f;
        const float len = lengthOf(q);
        say(b, at, q, vo, r, len);
        // eine zweite Stimme schwingt mit: tiefer (Quinte darunter), 30 ms spaeter, leise - wie aus mehreren Kehlen
        Voice v2 = vo;
        v2.f0 = vo.f0 * 0.667f, v2.fmul = 0.92f, v2.gain = 0.32f;
        say(b, at + 0.03f, q, v2, r, len);
        // dann die letzte Silbe noch einmal - zu hoch, abgebrochen
        Phrase tail{q.back()};
        for (Ph& ph : tail[0].ph)
            if (ph.k == 'v') ph.f0a = 1.6f, ph.f0b = 1.55f, ph.len *= 0.7f;
        say(b, at + len + 0.42f, tail, vo, r, 0.3f);
    } else if (style == 1) {
        // abgehackt: Silben hart gekappt, die erste gestottert, Pausen am falschen Ort, flacher Ton mit einem Sprung
        vo.att = 0.004f, vo.rel = 0.003f, vo.jitter = 0.004f, vo.breath = 0.05f;
        const int jump = r.irange(0, (int)src.size() - 1);
        float t = at;
        for (size_t s = 0; s < src.size(); ++s) {
            Syl sy = src[s];
            for (Ph& ph : sy.ph)
                if (ph.k == 'v') {
                    const float j = (int)s == jump ? 1.9f : 0.95f;
                    ph.f0a = ph.f0b = j;
                    ph.len *= 0.75f;
                }
            const int reps = s == 0 ? r.irange(2, 3) : 1;
            for (int k = 0; k < reps; ++k) {
                Syl cut = sy;
                if (k + 1 < reps)  // gestotterte Wiederholungen: nur der Anfang der Silbe
                    for (Ph& ph : cut.ph) ph.len *= 0.5f;
                t = say(b, t, {cut}, vo, r, 0.4f) + r.range(0.05f, 0.11f);
            }
            t += r.range(0.12f, 0.3f);
        }
    } else {
        // verzoegert und gedehnt: anderthalb mal so lang, die Stimme sinkt wie ein langsamer werdendes Band, leise rau,
        // Vokaltrakt zu gross; am Ende ein feuchtes Schnalzen und Einatmen durch die Zaehne
        Phrase q = src;
        for (Syl& sy : q)
            for (Ph& ph : sy.ph) ph.len *= 1.6f;
        vo.fmul = 0.9f, vo.droop = 0.38f, vo.vibrato = 0.035f, vo.vibHz = 3.2f, vo.rough = 0.25f, vo.subharm = 0.25f, vo.breath = 0.06f;
        const float len = lengthOf(q);
        const float end = say(b, at + 0.15f, q, vo, r, len);
        ImpactSpec sp;
        sp.modes = modesWood(r.range(900.0f, 1300.0f), 0.02f, r);
        sp.contact = 0.0008f, sp.noise = 0.7f, sp.noiseHz = 2200.0f;
        b.add(impact(sp, 0.35f, 0.08f, rate, r), end + 0.12f);
        breathe(b, end + 0.2f, 0.45f, true, 0.3f, 0.9f, r);
    }
    filter(b, Biquad::HP, 85.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}

Sound skinShriek(int rate) {
    Rand r(12400u);
    Buf b(1.6f, rate);
    VoiceSpec vs;
    vs.f0 = [](float u) { return 260.0f + 700.0f * dsp::smooth01(u / 0.35f) - 180.0f * u; };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.06f) * (1.0f - dsp::smooth01((u - 0.6f) / 0.4f)); };
    vs.formants = {950.0f, 1750.0f, 3100.0f, 4200.0f};
    vs.bandwidths = {160.0f, 220.0f, 300.0f, 400.0f};
    vs.jitter = 0.08f, vs.shimmer = 0.4f, vs.subharm = 0.6f, vs.breath = 0.7f, vs.rough = 0.8f, vs.roughHz = 31.0f, vs.tiltHz = 5200.0f;
    voice(b, 0.0f, 1.2f, vs, r);
    // eine tiefe Kehle darunter (die eigene Stimme des Wesens)
    VoiceSpec lo = vs;
    lo.f0 = [](float u) { return 95.0f + 40.0f * std::sin(u * 9.0f); };
    lo.formants = {420.0f, 900.0f, 2300.0f};
    lo.bandwidths = {120.0f, 180.0f, 260.0f};
    voice(b, 0.05f, 1.1f, lo, r);
    // Haut reisst: nasse, reissende Koerner
    grains(b, 0.02f, 0.5f, 60.0f, [](Buf& g, float at, Rand& rr) {
        noiseBand(g, at, rr.range(0.01f, 0.03f), rr.range(1200.0f, 3200.0f), 1.5f, rr.range(0.15f, 0.4f), true, rr);
    }, r);
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}

// --- Leuchtstab ---------------------------------------------------------------------------------------------------------
Sound glowCrack(int rate) {
    Rand r(12500u);
    Buf b(1.0f, rate);
    // Kunststoff biegt sich (kurzes Knarren), die Ampulle bricht (heller Glasknack), dann schuetteln: Fluessigkeit schwappt,
    // Splitter klickern
    CreakSpec ck;
    ck.rate0 = 90.0f, ck.rate1 = 160.0f, ck.formants = {1400.0f, 3100.0f, 5200.0f}, ck.q = 6.0f;
    creak(b, 0.0f, 0.09f, ck, 0.25f, r);
    ImpactSpec gl;
    gl.modes = modesGlass(r.range(3300.0f, 3900.0f), 0.05f, r);
    gl.contact = 0.0003f, gl.noise = 0.6f, gl.noiseHz = 5000.0f, gl.noiseT60 = 0.01f;
    b.add(impact(gl, 0.8f, 0.12f, rate, r), 0.085f);
    ImpactSpec pl;
    pl.modes = modesPlastic(r.range(900.0f, 1200.0f), 0.03f, r);
    pl.contact = 0.001f, pl.noise = 0.5f;
    b.add(impact(pl, 0.5f, 0.08f, rate, r), 0.09f);
    for (int k = 0; k < 4; ++k) {  // schuetteln
        const float t = 0.26f + 0.12f * (float)k;
        noiseBand(b, t, 0.08f, r.range(700.0f, 1100.0f), 0.8f, 0.10f, false, r);
        for (int s = 0; s < 3; ++s) {
            ImpactSpec sh;
            sh.modes = modesGlass(r.range(5000.0f, 7000.0f), 0.015f, r);
            sh.contact = 0.0002f, sh.noise = 0.3f;
            b.add(impact(sh, r.range(0.04f, 0.1f), 0.03f, rate, r), t + r.range(0.0f, 0.07f));
        }
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound glowThrow(int rate) {
    Rand r(12600u);
    Buf b(0.45f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        f.set(500.0f + 1700.0f * dsp::smooth01(t / 0.22f), 1.6f, (float)rate);
        b.x[i] = f.bp(r.bi()) * envAHR(t, 0.1f, 0.04f, 0.2f);
    }
    trim(b);
    normalize(b, level::kFoley - 3.0f);
    return b.sound();
}

Sound glowLand(int rate, int v) {
    Rand r(12700u + (unsigned)v);
    Buf b(0.6f, rate);
    // hohler, leichter Kunststoff auf Stein/Teppich: ein Tick, ein kleineres Nachspringen, bei 2 noch ein Rollen
    ImpactSpec sp;
    sp.modes = modesPlastic(r.range(1700.0f, 2500.0f), 0.035f, r);
    sp.contact = 0.0006f, sp.noise = 0.35f, sp.noiseHz = 4200.0f;
    b.add(impact(sp, 1.0f, 0.1f, rate, r), 0.0f);
    b.add(impact(sp, 0.35f, 0.08f, rate, r), r.range(0.06f, 0.09f));
    if (v == 2) {
        for (int k = 0; k < 6; ++k) {
            ImpactSpec rl = sp;
            rl.modes = modesPlastic(r.range(2200.0f, 2900.0f), 0.015f, r);
            b.add(impact(rl, r.range(0.04f, 0.1f), 0.03f, rate, r), 0.14f + 0.045f * (float)k + r.range(0.0f, 0.01f));
        }
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound glowSplash(int rate) {
    Rand r(12800u);
    Buf b(0.7f, rate);
    noiseBand(b, 0.0f, 0.06f, 1500.0f, 0.7f, 0.4f, true, r);
    for (int k = 0; k < 5; ++k) bubble(b, 0.02f + r.range(0.0f, 0.25f), r.range(2.0f, 4.5f), r.range(0.2f, 0.5f), r);
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound smilerLured(int rate) {
    Rand r(12900u);
    Buf b(2.0f, rate);
    // das Droehnen, das ihn sonst begleitet, biegt ab und sinkt - darueber ein glasiges Sirren, nasses Zaehneklicken
    float ph = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float f = 68.0f - 16.0f * dsp::smooth01(t / 1.4f);
        ph += kTwoPi * f / (float)rate;
        const float env = dsp::smooth01(t / 0.35f) * (1.0f - dsp::smooth01((t - 1.2f) / 0.7f));
        b.x[i] = (std::sin(ph) + 0.35f * std::sin(2.0f * ph + 0.3f * std::sin(t * 3.0f))) * env * 0.5f;
        b.x[i] += std::sin(kTwoPi * (2350.0f + 25.0f * std::sin(t * 2.2f)) * t) * env * 0.025f * (f / 68.0f);
    }
    for (int k = 0; k < 3; ++k) {
        ImpactSpec sp;
        sp.modes = modesCeramic(r.range(2600.0f, 3400.0f), 0.02f, r);
        sp.contact = 0.0004f, sp.noise = 0.6f, sp.noiseHz = 3500.0f;
        b.add(impact(sp, r.range(0.2f, 0.35f), 0.05f, rate, r), 0.3f + 0.22f * (float)k + r.range(0.0f, 0.05f));
    }
    trim(b);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}
}  // namespace

void addLureSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"glow_crack", [=] { return glowCrack(rate); }});
    d.push_back({"glow_throw", [=] { return glowThrow(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("glow_land_{}", v), [=] { return glowLand(rate, v); }});
    d.push_back({"glow_splash", [=] { return glowSplash(rate); }});
    d.push_back({"smiler_lured", [=] { return smilerLured(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("talk_call_{}", v), [=] { return talkCall(rate, v); }});
    for (int v = 0; v < 12; ++v) d.push_back({std::format("skin_reply_{}", v), [=] { return skinReply(rate, v); }});
    d.push_back({"skin_shriek", [=] { return skinShriek(rate); }});
}

}  // namespace lim::audio
