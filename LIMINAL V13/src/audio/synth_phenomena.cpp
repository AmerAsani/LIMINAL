// V13.3: Klaenge der Erscheinungen von Level 1 (KREATUR_Crawler.md, KREATUR_GameMaster.md, KREATUR_L1_Erscheinungen.md).
// Alles synthetisch:
//   itch_scratch   Kratzen auf der Haut (Fingernaegel, kurze Striche)            - Pilz, Stadium 1 bis 3
//   fever_pulse    Fieber: zwei schwere Herzschlaege, Blut rauscht in den Ohren  - Pilz, Stadium 3
//   garden_rustle  Blaetter rascheln dicht am Ohr                                - Garden, Zeichen und Ranken
//   garden_creak   Holz knarrt: die Haut wird zu Rinde                           - Garden, Verwandlung
//   jester_squeak  die roten Kugeln quietschen (Gummi, Luft)                     - Gemaelde der Spielleiterin
//   jester_giggle  Kichern aus einem kleinen Lautsprecher hinter Glas            - ihre Einladung
//   jester_buzz    schnarrender Fehlerton: "geschummelt"                         - der Hinauswurf
//   slap_step      nackter, nasser, flacher Fuss klatscht auf Beton             - hinter der Zimmertuer
//   claw_scrape    Krallen ziehen ueber Blech und Putz                           - an der Zimmertuer
//   wet_breath     schwerer, feuchter Atem dicht hinter der Tuer                 - an der Zimmertuer
#include <cmath>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

Sound itchScratch(int rate) {
    Rand r(14100u);
    Buf b(1.1f, rate);
    // sechs bis neun Striche hin und her, jeder ein Rauschband 2-6 kHz mit rauer Kornung (Hautrillen)
    float at = 0.02f;
    for (int k = 0; k < 8; ++k) {
        const float len = r.range(0.05f, 0.085f);
        Biquad bp = Biquad::make(Biquad::BP, r.range(2600.0f, 4200.0f), 1.1f, (float)rate);
        const size_t a = b.at(at), n = b.at(len);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = b.t(i);
            const float grain = 0.55f + 0.45f * std::sin(kTwoPi * r.range(70.0f, 130.0f) * t);
            b.x[a + i] += bp.run(r.bi()) * envAHR(t, len * 0.25f, len * 0.4f, len * 0.35f) * grain * (k % 2 ? 0.8f : 1.0f);
        }
        at += len + r.range(0.025f, 0.06f);
    }
    trim(b);
    normalize(b, level::kFoley - 6.0f);
    return b.sound();
}

Sound feverPulse(int rate) {
    Rand r(14200u);
    Buf b(2.2f, rate);
    dsp::Brown br;
    for (size_t i = 0; i < b.n(); ++i) {  // Rauschen des Bluts, schwillt mit jedem Schlag
        const float t = b.t(i);
        const float swell = 0.4f + 0.6f * (std::exp(-std::pow((t - 0.15f) * 4.0f, 2.0f)) + std::exp(-std::pow((t - 1.05f) * 4.0f, 2.0f)));
        b.x[i] = br.run(r.bi() * 0.1f) * 0.6f * swell * envAHR(t, 0.3f, 1.2f, 0.7f);
    }
    for (float t0 : {0.1f, 0.32f, 1.0f, 1.22f}) {  // lub-dub, lub-dub
        const float f0 = t0 == 0.1f || t0 == 1.0f ? 52.0f : 64.0f;
        const size_t a = b.at(t0), n = b.at(0.2f);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = b.t(i);
            b.x[a + i] += std::sin(kTwoPi * f0 * t * (1.0f - 0.3f * t)) * envAD(t, 0.006f, 0.12f) * (f0 < 60 ? 1.0f : 0.7f);
        }
    }
    filter(b, Biquad::LP, 900.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent - 5.0f);
    return b.sound();
}

Sound gardenRustle(int rate) {
    Rand r(14300u);
    Buf b(1.8f, rate);
    // trockene Blaetter streifen aneinander: viele kurze, helle Knackser, in Wellen dichter
    grains(b, 0.05f, 1.65f, 140.0f, [&](Buf& g, float at, Rand& rr) {
        Biquad hp = Biquad::make(Biquad::BP, rr.range(3000.0f, 7000.0f), 0.9f, (float)g.rate);
        const size_t a = g.at(at), n = g.at(rr.range(0.004f, 0.016f));
        const float amp = rr.range(0.2f, 1.0f);
        for (size_t i = 0; i < n && a + i < g.n(); ++i) g.x[a + i] += hp.run(rr.bi()) * envAD(g.t(i), 0.0008f, 0.008f) * amp;
    }, r, [](float t) { return 0.3f + 0.7f * (0.5f + 0.5f * std::sin(t * 7.0f)) * envAHR(t, 0.4f, 0.8f, 0.5f); });
    fades(b, 0.05f, 0.2f);
    normalize(b, level::kFoley - 5.0f);
    return b.sound();
}

Sound gardenCreak(int rate) {
    Rand r(14400u);
    Buf b(1.6f, rate);
    CreakSpec c;
    c.rate0 = 14.0f, c.rate1 = 34.0f, c.jitter = 0.35f;
    c.formants = {180.0f, 420.0f, 1150.0f};
    c.q = 6.0f;
    creak(b, 0.05f, 1.1f, c, 1.0f, r);
    CreakSpec c2 = c;
    c2.rate0 = 40.0f, c2.rate1 = 26.0f, c2.formants = {260.0f, 700.0f, 1900.0f};
    creak(b, 0.7f, 0.6f, c2, 0.5f, r);
    filter(b, Biquad::LP, 2500.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent - 6.0f);
    return b.sound();
}

Sound jesterSqueak(int rate) {
    Rand r(14500u);
    Buf b(0.7f, rate);
    // Gummiball wird gedrueckt und losgelassen: Luft pfeift durch ein Ventil (steigend, dann fallend)
    for (int k = 0; k < 2; ++k) {
        const float t0 = k * 0.27f, len = k ? 0.2f : 0.24f;
        float ph = 0.0f;
        const size_t a = b.at(t0), n = b.at(len);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = b.t(i), u = t / len;
            const float f = (k ? 1250.0f : 980.0f) * (1.0f + 0.45f * std::sin(u * kPi)) * (1.0f + 0.01f * r.bi());
            ph += kTwoPi * f / (float)rate;
            const float v = std::sin(ph) + 0.3f * std::sin(2.0f * ph) + 0.15f * std::sin(3.0f * ph);
            b.x[a + i] += (v * 0.8f + r.bi() * 0.12f) * envAHR(t, 0.02f, len * 0.6f, len * 0.3f);
        }
    }
    filter(b, Biquad::BP, 1600.0f, 0.8f);
    trim(b);
    normalize(b, level::kFoley - 3.0f);
    return b.sound();
}

// kleiner Lautsprecher hinter Glas: Bandpass, grobe Abtastung, etwas Brummen
void tinySpeaker(Buf& b, int crush) {
    float hold = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        if (i % (size_t)crush == 0) hold = std::round(b.x[i] * 24.0f) / 24.0f;
        b.x[i] = hold + 0.004f * std::sin(kTwoPi * 100.0f * b.t(i));
    }
    filter(b, Biquad::HP, 650.0f, 0.7f);
    filter(b, Biquad::LP, 4200.0f, 0.9f);
}

Sound jesterGiggle(int rate) {
    Rand r(14600u);
    Buf b(1.3f, rate);
    // "hi-hi-hi-hi": hohe Stimme, Vokal i, Stoesse mit 7 Hz, die Tonhoehe faellt; dann der Lautsprecher
    VoiceSpec v;
    v.f0 = [](float t) { return 420.0f - 120.0f * t + 25.0f * std::sin(t * 44.0f); };
    v.amp = [](float t) { return std::pow(std::max(0.0f, std::sin(t * kTwoPi * 6.5f)), 0.6f) * (1.0f - 0.5f * t); };
    v.formants = {330.0f, 2400.0f, 3100.0f};
    v.bandwidths = {80.0f, 160.0f, 220.0f};
    v.breath = 0.3f, v.jitter = 0.03f, v.shimmer = 0.15f;
    voice(b, 0.03f, 1.05f, v, r);
    tinySpeaker(b, 3);
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

Sound jesterBuzz(int rate) {
    Rand r(14700u);
    Buf b(0.9f, rate);
    // zwei verstimmte Rechtecke (140/147 Hz), hart an und aus, knisternd
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float sq = (std::sin(kTwoPi * 140.0f * t) > 0 ? 1.0f : -1.0f) + (std::sin(kTwoPi * 147.0f * t) > 0 ? 0.8f : -0.8f);
        b.x[i] = (sq * 0.4f + (r.chance(0.002f) ? r.bi() * 2.0f : 0.0f)) * envAHR(t, 0.01f, 0.62f, 0.08f);
    }
    tinySpeaker(b, 2);
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

Sound slapStep(int rate) {
    Rand r(14800u);
    Buf b(0.5f, rate);
    // flacher, nasser Fuss: dumpfer Aufsatz, klatschendes Wasser, saugendes Abloesen
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = std::sin(kTwoPi * 95.0f * t * (1.0f - 0.4f * t)) * envAD(t, 0.003f, 0.07f) * 0.9f;
    }
    Biquad bp = Biquad::make(Biquad::BP, 1500.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.at(0.09f); ++i) b.x[i] += bp.run(r.bi()) * envAD(b.t(i), 0.0015f, 0.035f) * 1.4f;
    Biquad bp2 = Biquad::make(Biquad::BP, 900.0f, 1.2f, (float)rate);
    const size_t a = b.at(0.17f);
    for (size_t i = 0; i < b.at(0.12f) && a + i < b.n(); ++i) b.x[a + i] += bp2.run(r.bi()) * envAHR(b.t(i), 0.03f, 0.03f, 0.06f) * 0.35f;
    for (int k = 0; k < 4; ++k) bubble(b, 0.03f + r.range(0.0f, 0.2f), r.range(1.5f, 3.5f), r.range(0.05f, 0.15f), r);
    trim(b);
    normalize(b, level::kStep - 1.0f);
    return b.sound();
}

Sound clawScrape(int rate) {
    Rand r(14900u);
    Buf b(1.1f, rate);
    // drei, vier Krallen zugleich: hohes Stick-Slip-Kreischen auf Blech, darunter Putz, der abbroeckelt
    for (int k = 0; k < 4; ++k) {
        CreakSpec c;
        c.rate0 = r.range(260.0f, 380.0f), c.rate1 = r.range(150.0f, 230.0f), c.jitter = 0.18f;
        c.formants = {r.range(2100.0f, 2700.0f), r.range(3600.0f, 4400.0f), r.range(5600.0f, 6600.0f)};
        c.q = 14.0f;
        creak(b, 0.02f + 0.03f * (float)k, r.range(0.6f, 0.8f), c, r.range(0.5f, 0.9f), r);
    }
    grains(b, 0.05f, 0.85f, 70.0f, [&](Buf& g, float at, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesConcrete(rr.range(1800.0f, 3200.0f), rr);
        sp.contact = 0.0005f, sp.noise = 0.5f;
        g.add(impact(sp, rr.range(0.1f, 0.4f), 0.03f, g.rate, rr), at);
    }, r);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound wetBreath(int rate) {
    Rand r(15000u);
    Buf b(2.2f, rate);
    // langsam, tief, feucht: Einatmen mit Rasseln, Ausatmen durch Fluessigkeit (Blasen)
    breathe(b, 0.05f, 0.8f, true, 1.0f, 0.35f, r);
    breathe(b, 0.95f, 1.0f, false, 0.9f, 0.2f, r);
    for (int k = 0; k < 22; ++k) bubble(b, 0.98f + r.range(0.0f, 0.9f), r.range(1.0f, 4.0f), r.range(0.05f, 0.22f), r);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] *= 1.0f + 0.35f * std::sin(kTwoPi * 31.0f * b.t(i));  // Rasseln im Hals
    filter(b, Biquad::LP, 3000.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

}  // namespace

void addPhenomenaSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"itch_scratch", [=] { return itchScratch(rate); }});
    d.push_back({"fever_pulse", [=] { return feverPulse(rate); }});
    d.push_back({"garden_rustle", [=] { return gardenRustle(rate); }});
    d.push_back({"garden_creak", [=] { return gardenCreak(rate); }});
    d.push_back({"jester_squeak", [=] { return jesterSqueak(rate); }});
    d.push_back({"jester_giggle", [=] { return jesterGiggle(rate); }});
    d.push_back({"jester_buzz", [=] { return jesterBuzz(rate); }});
    d.push_back({"slap_step", [=] { return slapStep(rate); }});
    d.push_back({"claw_scrape", [=] { return clawScrape(rate); }});
    d.push_back({"wet_breath", [=] { return wetBreath(rate); }});
}

}  // namespace lim::audio
