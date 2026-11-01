// V13.3: Klaenge des Wranglers (KREATUR_Wrangler.md). Die Seite: beim Graben sehr laut, man hoert sie kommen; rumpelnde
// Erschuetterungen kuendigen sie an. Eigene Laute nennt sie nicht - Auslegung: ein riesiger Leib, der sich durch Beton
// schiebt (Mahlen, Knirschen, rieselnder Staub, Unterbass), nasses, schweres Gleiten; das Maennchen mit dem Menschengesicht
// stoehnt und atmet wie ein Mensch, nur viel zu tief und zu gross; das Weibchen klappert mit den Zangen; die Jungen fiepen.
//   wrangler_rumble    Schleife: Rumpeln in der Tiefe, Mahlen, Knirschen, Kiesel (Beben, Lautstaerke nach dem Beben)
//   wrangler_slide     Schleife: der Leib gleitet - Schmatzen, Knarren der Haut, Anschwellen im Takt der Wellen
//   wrangler_drone     Schleife (ohne Ort): der Sog - ein tiefer, schwebender Ton aus wenigen Teiltoenen
//   wrangler_tremor_*  ein Stoss in der Tiefe, danach rieselt Staub
//   wrangler_breach_*  der Leib bricht durch: Beton reisst, Brocken fallen, nasses Reissen
//   wrangler_groan_*   Stoehnen des Maennchens (menschlich, eine Oktave unter einem Mann und tiefer)
//   wrangler_squelch_* Schmatzen und Glucksen des Leibs
//   wrangler_sense_*   Ankuendigung: es knackt in Wand oder Boden, tief wird Luft durch Zaehne eingesogen
//   wrangler_burst_m/f der Kopf bricht heraus: Brocken, Bruellen (Maennchen) / Zischen und Zangenschlag (Weibchen)
//   wrangler_snatch    Zugriff: Bersten, Zuschnappen, nasses Zerren
//   wrangler_sink_*    es versinkt: Mahlen entfernt sich, Schmatzen, Saugen
//   wrangler_settle    es hat einen verloren: langes Ausatmen, der Leib setzt sich
//   wrangler_clack_*   Zangen klappern (Weibchen)
//   wrangler_young_*   die Jungen fiepen (nass, hoch)
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

// Brocken und Kiesel auf Beton (Koerner)
void rubble(Buf& b, float t0, float t1, float perSec, float amp, Rand& r, const std::function<float(float)>& dens = nullptr) {
    grains(b, t0, t1, perSec, [&](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesConcrete(rr.range(500.0f, 2600.0f), rr);
        sp.contact = rr.range(0.0008f, 0.003f), sp.noise = 0.5f, sp.noiseHz = rr.range(1500.0f, 5000.0f), sp.noiseT60 = 0.01f;
        bb.add(impact(sp, amp * rr.range(0.2f, 1.0f), 0.12f, bb.rate, rr), t);
    }, r, dens);
}

// Mahlen: Rauschen durch tiefe Resonanzen, rauh moduliert (Beton auf Beton)
void grind(Buf& b, float t0, float t1, float amp, Rand& r, float hz = 140.0f) {
    Biquad bp = Biquad::make(Biquad::BP, hz, 1.2f, (float)b.rate), bp2 = Biquad::make(Biquad::BP, hz * 2.7f, 2.0f, (float)b.rate);
    dsp::Brown br;
    float m = 0.0f;
    const size_t a = b.at(t0), e = std::min(b.n(), b.at(t1));
    for (size_t i = a; i < e; ++i) {
        const float t = b.t(i) - t0, L = t1 - t0;
        if ((i & 63) == 0) m = 0.4f + 0.6f * std::fabs(r.bi());
        const float env = envAHR(t, 0.3f * L, 0.4f * L, 0.3f * L);
        const float n = br.run(r.bi()) * 3.0f + r.bi() * 0.3f;
        b.x[i] += (bp.run(n) * 1.4f + bp2.run(n) * 0.5f) * amp * env * m;
    }
}

// nasses Schmatzen: Blasen und Klebeloesen
void squelch(Buf& b, float t0, float t1, float perSec, float amp, Rand& r) {
    grains(b, t0, t1, perSec, [&](Buf& bb, float t, Rand& rr) {
        bubble(bb, t, rr.range(2.5f, 9.0f), amp * rr.range(0.3f, 1.0f), rr);
        if (rr.chance(0.4f)) {  // Ablosen der klebrigen Haut: kurzes, gedaempftes Knistern
            const size_t s = bb.at(t);
            Biquad bp = Biquad::make(Biquad::BP, rr.range(600.0f, 1600.0f), 2.0f, (float)bb.rate);
            for (size_t i = 0; i < 900 && s + i < bb.n(); ++i)
                bb.x[s + i] += bp.run(rr.bi()) * amp * 0.6f * envAD((float)i / (float)bb.rate, 0.002f, 0.04f);
        }
    }, r);
}

Sound rumbleLoop(int rate) {
    Rand r(7500u);
    const float L = 12.0f;
    Buf b(L + 1.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 42.0f, 0.9f, (float)rate), lp2 = Biquad::make(Biquad::LP, 180.0f, 0.7f, (float)rate);
    float ph = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        ph += kTwoPi * (19.0f + 3.0f * std::sin(t * 0.37f)) / (float)rate;
        const float sw = 0.65f + 0.35f * std::sin(t * 0.52f + 1.0f) * std::sin(t * 0.21f);
        b.x[i] = (lp.run(br.run(r.bi())) * 3.2f + std::sin(ph) * 0.35f + lp2.run(br.run(r.bi())) * 0.5f) * sw;
    }
    for (float t = 0.3f; t < L; t += r.range(1.8f, 3.2f)) grind(b, t, t + r.range(1.2f, 2.4f), 0.5f, r, r.range(90.0f, 190.0f));
    rubble(b, 0.0f, L, 9.0f, 0.12f, r);
    for (float t = 1.0f; t < L - 1.0f; t += r.range(2.5f, 4.0f)) {  // Knarren der Bewehrung, Reissen von Beton
        CreakSpec cs;
        cs.rate0 = r.range(14.0f, 24.0f), cs.rate1 = r.range(8.0f, 16.0f);
        cs.formants = {r.range(160.0f, 260.0f), r.range(500.0f, 700.0f), 1400.0f};
        cs.q = 5.0f;
        creak(b, t, r.range(0.5f, 1.1f), cs, 0.25f, r);
    }
    loopify(b, 1.0f);
    normalize(b, level::kLoop + 4.0f);
    return b.sound(true);
}

Sound slideLoop(int rate) {
    Rand r(7510u);
    const float L = 10.0f;
    Buf b(L + 1.0f, rate);
    dsp::Pink pk;
    Biquad lp = Biquad::make(Biquad::LP, 900.0f, 0.7f, (float)rate), hp = Biquad::make(Biquad::HP, 80.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        // Anschwellen im Takt der Wellen (Peristaltik ~3 s)
        const float sw = 0.45f + 0.55f * std::pow(0.5f + 0.5f * std::sin(t * 2.1f), 2.0f);
        b.x[i] = hp.run(lp.run(pk.run(r.bi()))) * 0.9f * sw;
    }
    squelch(b, 0.0f, L, 7.0f, 0.22f, r);
    for (float t = 0.5f; t < L - 1.0f; t += r.range(1.4f, 2.6f)) {  // die Haut knarrt an den Ringen
        CreakSpec cs;
        cs.rate0 = r.range(30.0f, 55.0f), cs.rate1 = r.range(20.0f, 40.0f);
        cs.formants = {r.range(300.0f, 450.0f), r.range(900.0f, 1300.0f), 2400.0f};
        cs.q = 6.0f;
        creak(b, t, r.range(0.3f, 0.7f), cs, 0.18f, r);
    }
    loopify(b, 1.0f);
    normalize(b, level::kLoop + 2.0f);
    return b.sound(true);
}

Sound droneLoop(int rate) {
    Rand r(7520u);
    const float L = 14.0f;
    Buf b(L + 1.0f, rate);
    // wenige, leicht verstimmte Teiltoene (Schwebung), langsames Atmen der Lautstaerke; darueber ein glasiger Hauch
    const float f[5] = {41.0f, 61.8f, 82.3f, 123.1f, 164.9f}, a[5] = {1.0f, 0.55f, 0.5f, 0.22f, 0.12f};
    float ph[5] = {0, 1, 2, 3, 4};
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        float s = 0.0f;
        for (int k = 0; k < 5; ++k) {
            ph[k] += kTwoPi * f[k] * (1.0f + 0.004f * std::sin(t * (0.3f + 0.11f * (float)k))) / (float)rate;
            s += std::sin(ph[k]) * a[k];
        }
        const float breath = 0.6f + 0.4f * std::sin(t * kTwoPi / 7.0f);
        b.x[i] = s * breath * 0.3f;
    }
    {
        Biquad bp = Biquad::make(Biquad::BP, 2600.0f, 8.0f, (float)rate);
        for (size_t i = 0; i < b.n(); ++i) b.x[i] += bp.run(r.bi()) * 0.05f * (0.5f + 0.5f * std::sin(b.t(i) * 0.9f));
    }
    loopify(b, 1.5f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound tremorShot(int rate, int v) {
    Rand r(7530u + (unsigned)v);
    Buf b(4.5f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 55.0f, 0.9f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] = lp.run(br.run(r.bi())) * 4.0f * envAD(b.t(i), r.range(0.08f, 0.25f), r.range(1.6f, 2.6f));
    grind(b, 0.1f, r.range(1.4f, 2.0f), 0.6f, r, r.range(80.0f, 140.0f));
    rubble(b, 0.4f, 3.8f, 26.0f, 0.1f, r, [](float t) { return std::exp(-t * 0.6f); });  // Staub und Kiesel rieseln
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound breachShot(int rate, int v) {
    Rand r(7540u + (unsigned)v);
    Buf b(5.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 70.0f, 0.8f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] = lp.run(br.run(r.bi())) * 4.5f * envAHR(b.t(i), 0.15f, 0.8f, 2.5f);
    // Beton reisst: dichte Brocken, Knall am Anfang
    for (int k = 0; k < 6; ++k) {
        ImpactSpec sp;
        sp.modes = modesConcrete(r.range(180.0f, 600.0f), r);
        sp.contact = 0.004f, sp.noise = 0.7f, sp.noiseHz = 1200.0f, sp.noiseT60 = 0.05f;
        b.add(impact(sp, r.range(0.6f, 1.0f), 0.5f, rate, r), r.range(0.0f, 0.5f));
    }
    grind(b, 0.0f, 2.2f, 0.9f, r, 120.0f);
    rubble(b, 0.1f, 4.0f, 40.0f, 0.22f, r, [](float t) { return std::exp(-t * 0.8f); });
    squelch(b, 0.6f, 3.0f, 9.0f, 0.3f, r);  // nasses Reissen
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}

Sound groanShot(int rate, int v) {
    Rand r(7550u + (unsigned)v);
    const float len = r.range(2.6f, 3.8f);
    Buf b(len + 1.0f, rate);
    VoiceSpec vs;
    const float base = r.range(34.0f, 44.0f), ph = r.range(0.0f, 6.0f);
    vs.f0 = [=](float u) { return base * (1.0f + 0.1f * std::sin(u * 3.0f + ph) - 0.12f * u); };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.25f) * (1.0f - dsp::smooth01((u - 0.7f) / 0.3f)); };
    // ein menschliches "oh", aber die Formanten eines viel zu grossen Rachens
    vs.formants = {r.range(210.0f, 260.0f), r.range(420.0f, 520.0f), 1500.0f, 2100.0f};
    vs.bandwidths = {80.0f, 110.0f, 200.0f, 260.0f};
    vs.jitter = 0.05f, vs.shimmer = 0.25f, vs.subharm = 0.5f, vs.breath = 0.3f, vs.rough = 0.35f, vs.roughHz = 15.0f;
    vs.tiltHz = 900.0f;
    voice(b, 0.0f, len, vs, r);
    breathe(b, len - 0.2f, 1.0f, true, 0.1f, 0.2f, r);
    dsp::RoomSpec room;
    reverb(b, room, 0.35f, 1.0f, 7551u + (unsigned)v, 1.5f);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}

Sound squelchShot(int rate, int v) {
    Rand r(7560u + (unsigned)v);
    Buf b(1.6f, rate);
    squelch(b, 0.0f, 1.0f, 26.0f, 0.5f, r);
    CreakSpec cs;
    cs.rate0 = r.range(25.0f, 45.0f), cs.rate1 = r.range(15.0f, 30.0f);
    cs.formants = {r.range(280.0f, 420.0f), 1100.0f, 2300.0f};
    creak(b, r.range(0.1f, 0.4f), r.range(0.4f, 0.8f), cs, 0.25f, r);
    trim(b);
    normalize(b, level::kFoley + 2.0f);
    return b.sound();
}

Sound senseShot(int rate, int v) {
    Rand r(7570u + (unsigned)v);
    Buf b(2.4f, rate);
    // knacken in Wand oder Boden: Risse, die sich ausbreiten
    for (float t = 0.0f; t < 1.1f; t += r.range(0.03f, 0.16f)) {
        ImpactSpec sp;
        sp.modes = modesConcrete(r.range(900.0f, 3200.0f), r);
        sp.contact = 0.0006f, sp.noise = 0.6f, sp.noiseHz = 4000.0f, sp.noiseT60 = 0.006f;
        b.add(impact(sp, r.range(0.25f, 0.8f) * (0.4f + 0.6f * t), 0.08f, rate, r), t);
    }
    grind(b, 0.2f, 1.6f, 0.35f, r, 160.0f);
    // tiefes Einatmen durch Zaehne (viel zu grosser Rachen)
    breathe(b, 0.35f, 1.6f, true, 0.6f, 0.9f, r);
    {
        Biquad lp = Biquad::make(Biquad::LP, 380.0f, 1.2f, (float)rate);
        dsp::Brown br;
        for (size_t i = b.at(0.35f); i < b.n(); ++i)
            b.x[i] += lp.run(br.run(r.bi())) * 1.2f * envAHR(b.t(i) - 0.35f, 0.5f, 0.7f, 0.5f);
    }
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound burstShot(int rate, bool male, int v) {
    Rand r((male ? 7580u : 7590u) + (unsigned)v);
    Buf b(3.4f, rate);
    for (int k = 0; k < 8; ++k) {  // die Flaeche birst
        ImpactSpec sp;
        sp.modes = modesConcrete(r.range(150.0f, 700.0f), r);
        sp.contact = 0.003f, sp.noise = 0.8f, sp.noiseHz = 1800.0f, sp.noiseT60 = 0.06f;
        b.add(impact(sp, r.range(0.7f, 1.0f), 0.5f, rate, r), r.range(0.0f, 0.12f));
    }
    rubble(b, 0.05f, 2.4f, 50.0f, 0.25f, r, [](float t) { return std::exp(-t * 1.2f); });
    if (male) {  // Bruellen: ein Mensch, viel zu gross
        VoiceSpec vs;
        const float base = r.range(55.0f, 68.0f);
        vs.f0 = [=](float u) { return base * (1.25f - 0.4f * u); };
        vs.amp = [](float u) { return dsp::smooth01(u / 0.06f) * (1.0f - dsp::smooth01((u - 0.6f) / 0.4f)); };
        vs.formants = {r.range(520.0f, 640.0f), r.range(950.0f, 1150.0f), 2300.0f, 3000.0f};
        vs.bandwidths = {120.0f, 160.0f, 240.0f, 320.0f};
        vs.jitter = 0.09f, vs.shimmer = 0.4f, vs.subharm = 0.9f, vs.breath = 0.5f, vs.rough = 0.8f, vs.roughHz = 24.0f;
        vs.tiltHz = 2200.0f;
        voice(b, 0.08f, 1.6f, vs, r);
    } else {  // Zischen, die Zangen schlagen zu
        breathe(b, 0.05f, 1.1f, false, 0.9f, 1.0f, r);
        for (int k = 0; k < 3; ++k) {
            ImpactSpec sp;
            sp.modes = modesPlastic(r.range(1400.0f, 2600.0f), 0.04f, r);
            sp.contact = 0.0005f, sp.noise = 0.4f, sp.noiseHz = 5000.0f, sp.noiseT60 = 0.004f;
            b.add(impact(sp, 1.0f, 0.15f, rate, r), 0.35f + 0.11f * (float)k);
        }
    }
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}

Sound snatchShot(int rate) {
    Rand r(7600u);
    Buf b(3.0f, rate);
    b.add(burstShot(rate, true, 7).samples, 0.0f, 0.9f);
    {
        ImpactSpec sp;  // Zuschnappen: dumpfer Schlag
        sp.modes = modesWood(r.range(90.0f, 140.0f), 0.08f, r);
        sp.contact = 0.006f, sp.noise = 0.5f, sp.noiseHz = 700.0f, sp.noiseT60 = 0.05f;
        b.add(impact(sp, 1.0f, 0.4f, rate, r), 0.38f);
    }
    squelch(b, 0.4f, 2.4f, 30.0f, 0.5f, r);  // nasses Zerren
    normalize(b, level::kEvent + 3.0f);
    return b.sound();
}

Sound sinkShot(int rate, int v) {
    Rand r(7610u + (unsigned)v);
    Buf b(5.0f, rate);
    dsp::Brown br;
    Biquad lp = Biquad::make(Biquad::LP, 50.0f, 0.9f, (float)rate);
    for (size_t i = 0; i < b.n(); ++i) b.x[i] = lp.run(br.run(r.bi())) * 3.0f * envAHR(b.t(i), 0.3f, 1.2f, 3.0f);
    grind(b, 0.0f, 3.5f, 0.45f, r, 100.0f);
    squelch(b, 0.2f, 2.0f, 12.0f, 0.3f, r);
    for (int k = 0; k < 3; ++k) bubble(b, 1.0f + 0.5f * (float)k, r.range(12.0f, 20.0f), 0.5f, r);  // Saugen
    rubble(b, 0.5f, 4.5f, 10.0f, 0.06f, r);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound settleShot(int rate) {
    Rand r(7620u);
    Buf b(3.5f, rate);
    breathe(b, 0.0f, 2.6f, false, 0.7f, 0.5f, r);  // langes Ausatmen
    {
        Biquad lp = Biquad::make(Biquad::LP, 300.0f, 1.1f, (float)rate);
        dsp::Brown br;
        for (size_t i = 0; i < b.n(); ++i) b.x[i] += lp.run(br.run(r.bi())) * 1.0f * envAHR(b.t(i), 0.4f, 1.0f, 1.5f);
    }
    grind(b, 0.5f, 2.8f, 0.2f, r, 110.0f);
    trim(b);
    normalize(b, level::kCreature - 5.0f);
    return b.sound();
}

Sound clackShot(int rate, int v) {
    Rand r(7630u + (unsigned)v);
    Buf b(0.9f, rate);
    float t = 0.0f;
    for (int k = 0; k < r.irange(2, 5); ++k) {
        ImpactSpec sp;
        sp.modes = modesPlastic(r.range(1100.0f, 2400.0f), 0.05f, r);
        sp.contact = 0.0006f, sp.noise = 0.35f, sp.noiseHz = 4500.0f, sp.noiseT60 = 0.005f;
        b.add(impact(sp, r.range(0.6f, 1.0f), 0.12f, rate, r), t);
        t += r.range(0.05f, 0.14f);
    }
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound youngShot(int rate, int v) {
    Rand r(7640u + (unsigned)v);
    const float len = r.range(0.25f, 0.5f);
    Buf b(len + 0.3f, rate);
    VoiceSpec vs;
    const float base = r.range(700.0f, 1100.0f);
    vs.f0 = [=](float u) { return base * (1.0f + 0.4f * std::sin(u * 9.0f) - 0.2f * u); };
    vs.amp = [](float u) { return dsp::smooth01(u / 0.1f) * (1.0f - dsp::smooth01((u - 0.6f) / 0.4f)); };
    vs.formants = {1300.0f, 2600.0f, 4200.0f};
    vs.bandwidths = {200.0f, 300.0f, 400.0f};
    vs.jitter = 0.06f, vs.shimmer = 0.3f, vs.breath = 0.4f;
    voice(b, 0.0f, len, vs, r);
    squelch(b, 0.0f, len, 20.0f, 0.15f, r);
    trim(b);
    normalize(b, level::kCreature - 10.0f);
    return b.sound();
}

}  // namespace

void addWranglerSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"wrangler_rumble", [=] { return rumbleLoop(rate); }});
    d.push_back({"wrangler_slide", [=] { return slideLoop(rate); }});
    d.push_back({"wrangler_drone", [=] { return droneLoop(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wrangler_tremor_{}", v), [=] { return tremorShot(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("wrangler_breach_{}", v), [=] { return breachShot(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wrangler_groan_{}", v), [=] { return groanShot(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("wrangler_squelch_{}", v), [=] { return squelchShot(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("wrangler_sense_{}", v), [=] { return senseShot(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("wrangler_burst_m{}", v), [=] { return burstShot(rate, true, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("wrangler_burst_f{}", v), [=] { return burstShot(rate, false, v); }});
    d.push_back({"wrangler_snatch", [=] { return snatchShot(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("wrangler_sink_{}", v), [=] { return sinkShot(rate, v); }});
    d.push_back({"wrangler_settle", [=] { return settleShot(rate); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("wrangler_clack_{}", v), [=] { return clackShot(rate, v); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("wrangler_young_{}", v), [=] { return youngShot(rate, v); }});
}

}  // namespace lim::audio
