// V13.3 Teil 4: Klaenge der Randwesen von Level 2 (gameplay/apparitions_l2.hpp; KREATUR_CannibalCuisine.md,
// KREATUR_Swindlebird.md, KREATUR_Photoshop.md, KREATUR_Macchina.md). Alles synthetisch:
//   cc_hum       Schleife: Kompressor, Leuchtroehre, darunter kaum hoerbares Murmeln im Blech      - Automat
//   cc_whisper   die Leichen im Lager fluestern mit ihren Stimmbaendern (unverstaendlich, gedaempft) - Automat
//   cc_sob       Schluchzen in einer vertrauten, warmen Stimme (was Suechtige statt dessen hoeren)  - Automat
//   cc_keys      zwei Tasten, kein Muenzklang, ein dumpfes Klacken im Inneren                       - Ausgabe
//   cc_hand      Knochenfinger schieben ueber Blech, Knoechel klacken                                - Ausgabe
//   cc_drop      die weiche, eingewickelte Ware faellt ins Blechfach, die Schwenkklappe pendelt     - Ausgabe
//   cc_rattle    die Klappen klappern im Rahmen (Warnung)                                            - Zorn
//   cc_flap      Klappen schlagen auf, Knochengelenke knacken                                        - Zorn
//   cc_creak     Knochen unter Last: die unteren Arme stemmen den Automaten hoch                      - Zorn
//   cc_strike    ein ueberzogener Knochenarm schwingt und schlaegt                                   - Zorn
//   sb_coo       leises, gurrendes Papageienmurmeln (fast ein Wort)                                  - Swindlebird
//   sb_flutter   Fluegelschlaege beim Auffliegen                                                     - Swindlebird
//   ps_glitch    digitales Zerreissen, Zeilenspruenge, Bitrauschen                                   - Photoshop
//   ps_whine     Fiepen und Brummen eines alten Roehrenmonitors                                       - Photoshop
//   ps_box       ein trockenes Klicken, dann ein steigender, kalter Ton: die Box ist da               - Photoshop
//   ps_crop      Reissen und digitales Knirschen: ausgeschnitten                                     - Photoshop
//   ps_arm       nasses Klatschen, die Bildschirmscheibe beult sich, Glas knistert                  - Photoshop
//   ps_release   fallender Glitch, Rauschen verebbt                                                  - Photoshop
//   mc_hollow    Schleife: Luftzug pfeift durch den hohlen Anzug (Helm als Resonator)                 - Macchina
//   mc_creak     Blech arbeitet, eine Platte klirrt nach                                             - Macchina
#include <cmath>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

// gefluesterte Silbe: Rauschen durch wandernde Vokalformanten (kein Stimmton)
void whisperSyllable(Buf& b, float at, float len, float amp, float f1, float f2, Rand& r) {
    Biquad a = Biquad::make(Biquad::BP, f1, 4.0f, (float)b.rate), c = Biquad::make(Biquad::BP, f2, 5.0f, (float)b.rate);
    Biquad s = Biquad::make(Biquad::HP, 3500.0f, 0.7f, (float)b.rate);
    const size_t st = b.at(at), n = b.at(len);
    const float glide = r.range(-0.25f, 0.25f), sib = r.range(0.0f, 1.0f);
    for (size_t k = 0; k < n && st + k < b.n(); ++k) {
        const float u = (float)k / (float)n;
        if ((k & 127) == 0) {
            a = Biquad::make(Biquad::BP, f1 * (1.0f + glide * u), 4.0f, (float)b.rate);
            c = Biquad::make(Biquad::BP, f2 * (1.0f - glide * u), 5.0f, (float)b.rate);
        }
        const float w = r.bi();
        const float env = std::sin(3.14159f * u) * (0.8f + 0.2f * std::sin(u * 31.0f));
        b.x[st + k] += (a.run(w) * 1.2f + c.run(w) * 0.8f + (u < 0.25f ? s.run(w) * sib * 0.6f : 0.0f)) * env * amp;
    }
}

Sound ccHum(int rate) {
    Rand r(36100u);
    Buf b(4.0f, rate);
    dsp::Pink pk;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        // Kompressor (Brummen mit Oberwellen), die Roehre (Netzbrummen 100 Hz, Sirren)
        float v = 0.5f * std::sin(kTwoPi * 50.0f * t) + 0.32f * std::sin(kTwoPi * 100.0f * t + 0.4f) + 0.16f * std::sin(kTwoPi * 150.0f * t) +
                  0.09f * std::sin(kTwoPi * 200.0f * t + 1.1f);
        v *= 0.75f + 0.25f * std::sin(kTwoPi * 0.5f * t);
        v += 0.05f * std::sin(kTwoPi * 7800.0f * t) * (0.6f + 0.4f * std::sin(kTwoPi * 0.25f * t));
        b.x[i] = v + pk.run(r.bi()) * 0.04f;
    }
    // ganz leise, gedaempft: Murmeln im Blech (die Leichen im Lager)
    Buf m(4.0f, rate);
    for (int k = 0; k < 7; ++k) whisperSyllable(m, 0.2f + k * 0.55f, r.range(0.25f, 0.45f), 0.25f, r.range(500.0f, 800.0f), r.range(1200.0f, 1900.0f), r);
    filter(m, Biquad::LP, 700.0f, 0.7f);
    b.add(m, 0.0f, 0.5f);
    filter(b, Biquad::HP, 40.0f, 0.7f);
    loopify(b, 0.4f);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound ccWhisper(int rate) {
    Rand r(36200u);
    Buf b(2.8f, rate);
    // drei Stimmen durcheinander, nicht zu verstehen; dumpf durch das Gehaeuse
    float at = 0.05f;
    for (int k = 0; k < 11; ++k) {
        whisperSyllable(b, at, r.range(0.14f, 0.32f), r.range(0.5f, 1.0f), r.range(420.0f, 900.0f), r.range(1100.0f, 2300.0f), r);
        at += r.range(0.12f, 0.28f);
    }
    filter(b, Biquad::LP, 2200.0f, 0.7f);
    reverb(b, dsp::RoomSpec{}, 0.25f, 0.8f, 36201u);
    trim(b);
    normalize(b, level::kCreature - 7.0f);
    return b.sound();
}

Sound ccSob(int rate) {
    Rand r(36300u);
    Buf b(3.4f, rate);
    // Schluchzen: abgehackte Einatmer, dazwischen ein zitternder, klagender Ton in warmer Stimme
    for (float t0 : {0.05f, 0.42f, 0.7f}) breathe(b, t0, 0.22f, true, 0.7f, 0.8f, r);
    VoiceSpec v;
    v.f0 = [](float t) { return 290.0f - 60.0f * t + 18.0f * std::sin(t * 37.0f); };
    v.amp = [](float t) { return std::sin(3.14159f * std::min(1.0f, t / 1.4f)) * (0.75f + 0.25f * std::sin(t * 23.0f)); };
    v.formants = {700, 1200, 2600};
    v.breath = 0.45f, v.jitter = 0.035f, v.shimmer = 0.2f;
    voice(b, 1.0f, 1.4f, v, r);
    breathe(b, 2.55f, 0.5f, false, 0.6f, 0.6f, r);
    filter(b, Biquad::LP, 2600.0f, 0.7f);
    reverb(b, dsp::RoomSpec{}, 0.3f, 0.75f, 36301u);
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound ccKeys(int rate) {
    Rand r(36400u);
    Buf b(0.9f, rate);
    ImpactSpec key;
    key.modes = modesPlastic(r.range(1800.0f, 2300.0f), 0.04f, r);
    key.contact = 0.002f, key.noise = 0.35f, key.noiseHz = 4000.0f;
    b.add(impact(key, 0.7f, 0.12f, rate, r), 0.02f);
    b.add(impact(key, 0.8f, 0.12f, rate, r), 0.24f);
    ImpactSpec in;  // im Inneren: ein Relais, ein dumpfes Blech
    in.modes = modesMetalPlate(r.range(180.0f, 240.0f), 0.25f, r);
    in.contact = 0.006f, in.noise = 0.2f, in.noiseHz = 900.0f;
    b.add(impact(in, 0.6f, 0.5f, rate, r), 0.42f, 0.8f);
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

Sound ccHand(int rate) {
    Rand r(36500u);
    Buf b(1.0f, rate);
    // Fingerknochen schaben ueber das Fachblech (rau, metallisch), Knoechel klacken
    Biquad bp = Biquad::make(Biquad::BP, 2600.0f, 2.5f, (float)rate);
    for (size_t i = 0; i < b.at(0.7f); ++i) {
        const float t = b.t(i);
        const float grain = 0.5f + 0.5f * std::sin(kTwoPi * 38.0f * t) * std::sin(kTwoPi * 11.0f * t);
        b.x[i] += bp.run(r.bi()) * grain * envAHR(t, 0.05f, 0.45f, 0.2f) * 0.8f;
    }
    ImpactSpec bone;
    bone.modes = modesCeramic(r.range(2400.0f, 3200.0f), 0.05f, r);
    bone.contact = 0.0015f, bone.noise = 0.3f;
    for (float t0 : {0.12f, 0.3f, 0.52f, 0.7f}) b.add(impact(bone, r.range(0.4f, 0.8f), 0.08f, rate, r), t0, 0.7f);
    trim(b);
    normalize(b, level::kFoley - 3.0f);
    return b.sound();
}

Sound ccDrop(int rate) {
    Rand r(36600u);
    Buf b(0.9f, rate);
    ImpactSpec soft;  // weich, eingewickelt (Papier, Fleisch) auf Blech
    soft.modes = modesMetalPlate(r.range(320.0f, 420.0f), 0.18f, r);
    soft.contact = 0.012f, soft.noise = 0.5f, soft.noiseHz = 1400.0f, soft.noiseT60 = 0.05f;
    b.add(impact(soft, 0.9f, 0.6f, rate, r), 0.02f);
    Biquad hp = Biquad::make(Biquad::HP, 3000.0f, 0.7f, (float)rate);  // Papier knistert
    for (size_t i = b.at(0.02f); i < b.at(0.18f); ++i) b.x[i] += hp.run(r.bi()) * 0.25f * envAD(b.t(i) - 0.02f, 0.002f, 0.08f);
    ImpactSpec flap;  // die Schwenkklappe pendelt nach
    flap.modes = modesMetalPlate(r.range(700.0f, 900.0f), 0.08f, r);
    flap.contact = 0.003f;
    b.add(impact(flap, 0.35f, 0.15f, rate, r), 0.32f);
    b.add(impact(flap, 0.2f, 0.12f, rate, r), 0.5f);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound ccRattle(int rate) {
    Rand r(36700u);
    Buf b(0.8f, rate);
    ImpactSpec sheet;
    sheet.modes = modesMetalPlate(r.range(400.0f, 600.0f), 0.08f, r);
    sheet.contact = 0.002f, sheet.noise = 0.25f;
    for (float t = 0.0f; t < 0.62f; t += r.range(0.025f, 0.055f)) b.add(impact(sheet, r.range(0.3f, 0.9f), 0.08f, rate, r), t, 0.8f);
    trim(b);
    normalize(b, level::kEvent - 2.0f);
    return b.sound();
}

Sound ccFlap(int rate) {
    Rand r(36800u);
    Buf b(0.9f, rate);
    ImpactSpec bang;
    bang.modes = modesMetalPlate(r.range(260.0f, 340.0f), 0.35f, r);
    bang.contact = 0.004f, bang.noise = 0.3f, bang.noiseHz = 2000.0f;
    b.add(impact(bang, 1.0f, 0.7f, rate, r), 0.01f);
    b.add(impact(bang, 0.8f, 0.6f, rate, r), 0.07f);
    ImpactSpec joint;
    joint.modes = modesCeramic(r.range(1800.0f, 2600.0f), 0.03f, r);
    joint.contact = 0.001f, joint.noise = 0.5f, joint.noiseHz = 5000.0f;
    for (float t0 : {0.18f, 0.24f, 0.33f, 0.41f}) b.add(impact(joint, r.range(0.5f, 0.9f), 0.05f, rate, r), t0, 0.6f);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound ccCreak(int rate) {
    Rand r(36900u);
    Buf b(1.1f, rate);
    CreakSpec c;
    c.rate0 = 22.0f, c.rate1 = 48.0f, c.jitter = 0.4f;
    c.formants = {900.0f, 1900.0f, 3600.0f};
    c.q = 7.0f;
    creak(b, 0.02f, 0.75f, c, 1.0f, r);
    ImpactSpec joint;
    joint.modes = modesCeramic(r.range(1500.0f, 2100.0f), 0.04f, r);
    joint.contact = 0.001f;
    b.add(impact(joint, 0.7f, 0.06f, rate, r), 0.82f);
    trim(b);
    normalize(b, level::kCreature - 4.0f);
    return b.sound();
}

Sound ccStrike(int rate) {
    Rand r(37000u);
    Buf b(0.7f, rate);
    // Luft: ein kurzer, tiefer Rausch-Schwung
    Biquad bp = Biquad::make(Biquad::BP, 600.0f, 1.2f, (float)rate);
    for (size_t i = 0; i < b.at(0.3f); ++i) {
        const float t = b.t(i);
        if ((i & 63) == 0) bp = Biquad::make(Biquad::BP, 300.0f + 1600.0f * t / 0.3f, 1.2f, (float)rate);
        b.x[i] += bp.run(r.bi()) * std::sin(3.14159f * t / 0.3f) * 0.9f;
    }
    ImpactSpec hit;  // ueberzogener Knochen schlaegt auf
    hit.modes = modesMetalPipe(r.range(380.0f, 520.0f), 0.12f, r);
    hit.contact = 0.004f, hit.noise = 0.45f, hit.noiseHz = 1600.0f;
    b.add(impact(hit, 1.0f, 0.35f, rate, r), 0.28f);
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}

Sound sbCoo(int rate) {
    Rand r(19200u);
    Buf b(1.1f, rate);
    // tiefes, rundes Gurren (Taube), am Ende ein Papageienlaut wie eine halbe Silbe
    VoiceSpec v;
    v.f0 = [](float t) { return 360.0f + 70.0f * std::sin(t * 9.0f) - 60.0f * t; };
    v.amp = [](float t) { return std::sin(3.14159f * std::min(1.0f, t / 0.55f)) * (0.6f + 0.4f * std::sin(t * 30.0f)); };
    v.formants = {450, 900, 2400};
    v.breath = 0.25f, v.rough = 0.3f;
    voice(b, 0.02f, 0.55f, v, r);
    VoiceSpec p;
    p.f0 = [](float t) { return 900.0f + 500.0f * t; };
    p.amp = [](float t) { return std::sin(3.14159f * std::min(1.0f, t / 0.22f)); };
    p.formants = {1100, 2200, 3600};
    p.breath = 0.3f;
    voice(b, 0.7f, 0.22f, p, r);
    filter(b, Biquad::HP, 200.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - 9.0f);
    return b.sound();
}

Sound sbFlutter(int rate) {
    Rand r(19300u);
    Buf b(1.1f, rate);
    // schnelle Fluegelschlaege: je Schlag ein Federrauschen, anfangs laut und dicht, dann leiser
    float at = 0.0f;
    for (int k = 0; k < 14; ++k) {
        const float amp = 1.0f - k * 0.06f, len = r.range(0.035f, 0.05f);
        Biquad bp = Biquad::make(Biquad::BP, r.range(900.0f, 1600.0f), 0.9f, (float)rate);
        const size_t a = b.at(at), n = b.at(len);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) b.x[a + i] += bp.run(r.bi()) * envAHR(b.t(i), len * 0.2f, len * 0.3f, len * 0.5f) * amp;
        at += r.range(0.055f, 0.075f);
    }
    trim(b);
    normalize(b, level::kFoley - 2.0f);
    return b.sound();
}

Sound psGlitch(int rate) {
    Rand r(10400u);
    Buf b(0.5f, rate);
    // Bloecke aus Bitrauschen und Rechtecktoenen, hart geschnitten
    float at = 0.0f;
    while (at < 0.42f) {
        const float len = r.range(0.012f, 0.06f);
        const float f = r.range(120.0f, 2400.0f);
        const int mode = (int)r.range(0.0f, 3.0f);
        const size_t a = b.at(at), n = b.at(len);
        float hold = 0.0f;
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = b.t(i);
            float v = mode == 0 ? (std::sin(kTwoPi * f * t) > 0.0f ? 0.6f : -0.6f) : mode == 1 ? r.bi() : 0.0f;
            if (mode == 2) {  // Abtastrate gebrochen
                if (i % 23 == 0) hold = r.bi();
                v = hold;
            }
            b.x[a + i] += v * r.range(0.5f, 1.0f);
        }
        at += len + (r.uni() < 0.3f ? r.range(0.01f, 0.04f) : 0.0f);
    }
    filter(b, Biquad::LP, 7000.0f, 0.7f);
    fades(b, 0.002f, 0.02f);
    normalize(b, level::kEvent - 6.0f);
    return b.sound();
}

Sound psWhine(int rate) {
    Rand r(10500u);
    Buf b(1.6f, rate);
    dsp::Pink pk;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = 0.25f * std::sin(kTwoPi * 15625.0f * 0.5f * t) + 0.35f * std::sin(kTwoPi * 50.0f * t) + 0.2f * std::sin(kTwoPi * 100.0f * t) +
                 pk.run(r.bi()) * 0.05f;
        b.x[i] *= std::sin(3.14159f * t / 1.6f);
    }
    normalize(b, level::kLoop + 2.0f);
    return b.sound();
}

Sound psBox(int rate) {
    Rand r(10600u);
    Buf b(0.9f, rate);
    ImpactSpec click;
    click.modes = modesPlastic(r.range(2600.0f, 3200.0f), 0.02f, r);
    click.contact = 0.001f, click.noise = 0.4f, click.noiseHz = 6000.0f;
    b.add(impact(click, 0.8f, 0.05f, rate, r), 0.0f);
    float ph = 0.0f;
    for (size_t i = b.at(0.05f); i < b.n(); ++i) {
        const float t = b.t(i) - 0.05f;
        const float f = 440.0f * std::pow(2.0f, t * 1.4f);
        ph += kTwoPi * f / (float)rate;
        b.x[i] += (std::sin(ph) + 0.3f * std::sin(3.0f * ph)) * 0.35f * envAHR(t, 0.04f, 0.5f, 0.25f);
    }
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound psCrop(int rate) {
    Rand r(10700u);
    Buf b(1.2f, rate);
    // Reissen (breites, rauhes Rauschen mit Stoessen), darin digitales Knirschen; am Ende ein harter Schnitt
    Biquad lp = Biquad::make(Biquad::LP, 2500.0f, 0.7f, (float)rate);
    for (size_t i = 0; i < b.at(0.8f); ++i) {
        const float t = b.t(i);
        const float tear = (r.uni() < 0.04f ? 1.0f : 0.35f) * (0.6f + 0.4f * std::sin(kTwoPi * 23.0f * t));
        b.x[i] += lp.run(r.bi()) * tear * envAHR(t, 0.01f, 0.6f, 0.2f);
        if ((i / 300) % 7 == 0) b.x[i] += ((i / 40) % 2 ? 0.4f : -0.4f) * envAD(t, 0.001f, 0.6f);
    }
    ImpactSpec cut;
    cut.modes = modesMetalPlate(r.range(150.0f, 200.0f), 0.2f, r);
    cut.contact = 0.002f, cut.noise = 0.6f;
    b.add(impact(cut, 1.0f, 0.35f, rate, r), 0.8f);
    trim(b);
    normalize(b, level::kEvent + 2.0f);
    return b.sound();
}

Sound psArm(int rate) {
    Rand r(10800u);
    Buf b(1.0f, rate);
    // die Scheibe beult sich (tiefes Wummern), Glas knistert, ein nasser Arm klatscht heraus
    for (size_t i = 0; i < b.at(0.4f); ++i) {
        const float t = b.t(i);
        b.x[i] += std::sin(kTwoPi * (55.0f + 40.0f * t) * t) * std::sin(3.14159f * t / 0.4f) * 0.7f;
    }
    ImpactSpec glass;
    glass.modes = modesGlass(r.range(2500.0f, 3400.0f), 0.3f, r);
    glass.contact = 0.001f, glass.noise = 0.2f;
    for (int k = 0; k < 6; ++k) b.add(impact(glass, r.range(0.15f, 0.4f), 0.2f, rate, r), r.range(0.1f, 0.45f), 0.5f);
    Biquad bp = Biquad::make(Biquad::BP, 700.0f, 0.8f, (float)rate);  // nasser Klatscher
    for (size_t i = b.at(0.42f); i < b.at(0.62f); ++i) b.x[i] += bp.run(r.bi()) * envAD(b.t(i) - 0.42f, 0.003f, 0.12f) * 1.2f;
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound psRelease(int rate) {
    Rand r(10900u);
    Buf b(1.0f, rate);
    float ph = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float f = 1600.0f * std::pow(0.08f, t);
        ph += kTwoPi * f / (float)rate;
        const float q = std::floor(std::sin(ph) * 4.0f) / 4.0f;  // grob quantisiert
        b.x[i] = (q * 0.5f + r.bi() * 0.25f * (1.0f - t)) * envAHR(t, 0.01f, 0.4f, 0.55f);
    }
    filter(b, Biquad::LP, 5000.0f, 0.7f);
    normalize(b, level::kEvent - 5.0f);
    return b.sound();
}

Sound mcHollow(int rate) {
    Rand r(18900u);
    Buf b(6.0f, rate);
    // Luft streicht ueber die Oeffnungen: Rauschen durch zwei Resonanzen (Helm, Brustraum), langsam an- und abschwellend
    Biquad h1 = Biquad::make(Biquad::BP, 310.0f, 18.0f, (float)rate), h2 = Biquad::make(Biquad::BP, 470.0f, 14.0f, (float)rate);
    dsp::Pink pk;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float w = pk.run(r.bi());
        const float g = 0.55f + 0.45f * std::sin(kTwoPi * t / 6.0f) * std::sin(kTwoPi * t / 3.0f + 0.7f);
        b.x[i] = (h1.run(w) * 1.0f + h2.run(w) * 0.6f) * g + w * 0.04f;
    }
    loopify(b, 0.6f);
    normalize(b, level::kLoop - 2.0f);
    return b.sound(true);
}

Sound mcCreak(int rate) {
    Rand r(18950u);
    Buf b(1.4f, rate);
    CreakSpec c;
    c.rate0 = 9.0f, c.rate1 = 18.0f, c.jitter = 0.5f;
    c.formants = {240.0f, 610.0f, 1500.0f};
    c.q = 9.0f;
    creak(b, 0.02f, 0.8f, c, 1.0f, r);
    ImpactSpec plate;
    plate.modes = modesMetalPlate(r.range(500.0f, 700.0f), 0.5f, r);
    plate.contact = 0.003f, plate.noise = 0.15f;
    b.add(impact(plate, 0.5f, 0.55f, rate, r), 0.85f);
    reverb(b, dsp::RoomSpec{}, 0.3f, 0.8f, 18951u);
    trim(b);
    normalize(b, level::kEvent - 5.0f);
    return b.sound();
}

}  // namespace

void addL2RandSounds(std::vector<SoundDef>& d, int rate) {
    d.push_back({"cc_hum", [=] { return ccHum(rate); }});
    d.push_back({"cc_whisper", [=] { return ccWhisper(rate); }});
    d.push_back({"cc_sob", [=] { return ccSob(rate); }});
    d.push_back({"cc_keys", [=] { return ccKeys(rate); }});
    d.push_back({"cc_hand", [=] { return ccHand(rate); }});
    d.push_back({"cc_drop", [=] { return ccDrop(rate); }});
    d.push_back({"cc_rattle", [=] { return ccRattle(rate); }});
    d.push_back({"cc_flap", [=] { return ccFlap(rate); }});
    d.push_back({"cc_creak", [=] { return ccCreak(rate); }});
    d.push_back({"cc_strike", [=] { return ccStrike(rate); }});
    d.push_back({"sb_coo", [=] { return sbCoo(rate); }});
    d.push_back({"sb_flutter", [=] { return sbFlutter(rate); }});
    d.push_back({"ps_glitch", [=] { return psGlitch(rate); }});
    d.push_back({"ps_whine", [=] { return psWhine(rate); }});
    d.push_back({"ps_box", [=] { return psBox(rate); }});
    d.push_back({"ps_crop", [=] { return psCrop(rate); }});
    d.push_back({"ps_arm", [=] { return psArm(rate); }});
    d.push_back({"ps_release", [=] { return psRelease(rate); }});
    d.push_back({"mc_hollow", [=] { return mcHollow(rate); }});
    d.push_back({"mc_creak", [=] { return mcCreak(rate); }});
}

}  // namespace lim::audio
