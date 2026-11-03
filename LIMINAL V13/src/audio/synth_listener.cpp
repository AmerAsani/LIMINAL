// V13.3 Teil 4 Level 188: Klaenge des Horchers und der Wurzelpistole (KREATUR_Horcher.md, KREATUR_Wurzelpistole.md).
// Die Seite der Ebene nennt: schwere Stille, lautes rhythmisches Haemmern, Rascheln, Knarren, Schritte. Alles Weitere ist
// Auslegung nach Gestalt und Ort (weisse Betongaenge, Mahagonituern):
//   Klopfen   eine schwere, knochige Faust auf massivem Mahagoni von der anderen Seite: dumpfer Holzkoerper, das Blatt
//             schwingt im Rahmen, die Klinke klappert nach; dazu ein feuchter Anteil (die aufgeplatzten Knoechel).
//   Lauschen  durch den Gehoergang (Mund) eingesogene Luft, ein nasses Schmatzen und Klicken - als wuerde das Ohr trinken.
//   Gehen     drei Stuetzen: die Faust (schwer, Knochen auf Beton), dann zwei nackte Fuesse (klatschend, Haut klebt).
//   Stimme    kein Mund im Gesicht: Laute kommen pfeifend und blubbernd aus dem Gehoergang (Kehlsack, Klicken).
//   Licht     ein hohes Kreischen durch den Gang, Knorpel knistert, wenn das Ohr knittert.
//   Wurzelpistole (Entity 39 der Seite, Auslegung): Wurzeln knarren und reiben (Rinde), ziehen sich nass um den Arm, saugen
//             in Pulsen; der Schuss mit Feuersalz statt Pulver - ein trockener, heller Knall mit nassem Nachplatzen; Huelsen
//             roter Blutkoerperchen tropfen; welk knistert es trocken.
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

void hissL(Buf& b, float at, float len, float hz, float q, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    for (size_t i = b.at(at); i < b.at(at + len) && i < b.n(); ++i) {
        const float u = (b.t(i) - at) / len;
        b.x[i] += bp.run(r.bi()) * amp * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
void clickL(Buf& b, float at, float amp, float hz, Rand& r) {  // nasses Klicken (Zunge im Gehoergang)
    Biquad bp = Biquad::make(Biquad::BP, hz, 3.0f, (float)b.rate);
    const size_t a = b.at(at);
    for (size_t k = 0; k < (size_t)(0.015f * (float)b.rate) && a + k < b.n(); ++k)
        b.x[a + k] += bp.run(r.bi()) * amp * std::exp(-(float)k / (0.0025f * (float)b.rate));
}
void finishL(Buf& b, float lufs, float wet, float size, std::uint32_t seed) {
    if (wet > 0.0f) reverb(b, dsp::RoomSpec{0.7f, 0.8f, 0.5f, size, 0.02f, 0.9f}, wet, 1.0f, seed);  // harter, leerer Betongang
    trim(b);
    normalize(b, lufs);
}
// ein Faustschlag auf eine schwere Holztuer (von der anderen Seite)
void doorKnock(Buf& b, float at, float amp, Rand& r) {
    ImpactSpec sp;
    sp.modes = modesWood(r.range(88.0f, 112.0f), 0.16f, r);
    sp.contact = r.range(0.006f, 0.009f), sp.noise = 0.35f, sp.noiseHz = r.range(500.0f, 800.0f), sp.noiseQ = 0.7f, sp.noiseT60 = 0.04f;
    b.add(impact(sp, amp, 0.7f, b.rate, r), at);
    ImpactSpec bone;  // Knochen auf Holz: hell und kurz
    bone.modes = modesWood(r.range(520.0f, 680.0f), 0.03f, r);
    bone.contact = 0.0012f, bone.noise = 0.6f, bone.noiseHz = 2600.0f, bone.noiseT60 = 0.01f;
    b.add(impact(bone, amp * 0.45f, 0.15f, b.rate, r), at + 0.001f);
    // das Blatt klappert im Rahmen, die Klinke schwingt nach
    ImpactSpec latch;
    latch.modes = modesMetalPlate(r.range(1900.0f, 2400.0f), 0.18f, r);
    latch.contact = 0.0006f, latch.noise = 0.1f;
    b.add(impact(latch, amp * 0.12f, 0.3f, b.rate, r), at + r.range(0.025f, 0.04f));
    // feucht: aufgeplatzte Haut der Knoechel
    clickL(b, at + 0.004f, amp * 0.12f, r.range(900.0f, 1400.0f), r);
}
Sound hrKnock(int rate, int var) {
    Rand r(18800u + (unsigned)var * 31u);
    Buf b(0.9f, rate);
    doorKnock(b, 0.0f, 1.0f, r);
    finishL(b, level::kEvent + 1.0f, 0.28f, 0.8f, 18801u + (unsigned)var);
    return b.sound();
}
Sound hrListen(int rate) {  // hinter der Tuer: nasses Einsaugen, Klicken, das Holz knarrt unter Druck
    Rand r(18810u);
    Buf b(2.4f, rate);
    breathe(b, 0.1f, 1.3f, true, 0.6f, 0.9f, r);
    for (int k = 0; k < 9; ++k) clickL(b, 0.2f + r.range(0.0f, 1.6f), r.range(0.1f, 0.25f), r.range(700.0f, 1800.0f), r);
    for (int k = 0; k < 4; ++k) bubble(b, 0.3f + r.range(0.0f, 1.4f), r.range(1.5f, 3.0f), 0.12f, r);
    CreakSpec cs;
    cs.rate0 = 18.0f, cs.rate1 = 30.0f, cs.formants = {180.0f, 420.0f, 900.0f}, cs.q = 6.0f;
    creak(b, 0.6f, 1.2f, cs, 0.25f, r);
    filter(b, Biquad::LP, 1800.0f, 0.7f);  // durch das Holz
    finishL(b, level::kCreature - 7.0f, 0.15f, 0.6f, 18811u);
    return b.sound();
}
Sound hrLost(int rate) {  // es entfernt sich hinter der Tuer: schleifende Faust, ein Fuss, dann nichts
    Rand r(18815u);
    Buf b(2.0f, rate);
    for (int k = 0; k < 3; ++k) {
        ImpactSpec sp;
        sp.modes = modesConcrete(r.range(140.0f, 200.0f), r);
        sp.contact = 0.01f, sp.noise = 0.5f, sp.noiseHz = 700.0f, sp.noiseT60 = 0.03f;
        b.add(impact(sp, 0.5f - 0.13f * (float)k, 0.3f, rate, r), 0.2f + 0.5f * (float)k);
    }
    hissL(b, 0.1f, 1.4f, 600.0f, 0.8f, 0.05f, r);
    filter(b, Biquad::LP, 1100.0f, 0.7f);
    finishL(b, level::kDistant, 0.3f, 0.7f, 18816u);
    return b.sound();
}
Sound hrDoor(int rate) {  // eine Tuer hinter dir: Klinke, langsames Knarren der Angel
    Rand r(18820u);
    Buf b(2.6f, rate);
    ImpactSpec latch;
    latch.modes = modesMetalPlate(2100.0f, 0.12f, r);
    latch.contact = 0.0008f, latch.noise = 0.3f, latch.noiseHz = 3500.0f;
    b.add(impact(latch, 0.6f, 0.25f, rate, r), 0.0f);
    b.add(impact(latch, 0.4f, 0.25f, rate, r), 0.09f);
    CreakSpec cs;
    cs.rate0 = 9.0f, cs.rate1 = 26.0f, cs.jitter = 0.35f, cs.formants = {320.0f, 880.0f, 2100.0f}, cs.q = 9.0f;
    creak(b, 0.35f, 1.8f, cs, 0.6f, r);
    finishL(b, level::kEvent - 1.0f, 0.3f, 0.8f, 18821u);
    return b.sound();
}
Sound hrUnfold(int rate) {  // Gelenke knacken nacheinander, Haut loest sich vom Holz, Knoechel setzen auf
    Rand r(18825u);
    Buf b(1.8f, rate);
    for (int k = 0; k < 6; ++k) {
        ImpactSpec sp;
        sp.modes = modesWood(r.range(700.0f, 1300.0f), 0.02f, r);
        sp.contact = 0.0006f, sp.noise = 0.8f, sp.noiseHz = 2400.0f, sp.noiseT60 = 0.008f;
        b.add(impact(sp, r.range(0.3f, 0.7f), 0.08f, rate, r), 0.1f + 0.22f * (float)k + r.range(0.0f, 0.06f));
    }
    hissL(b, 0.0f, 1.2f, 1500.0f, 0.6f, 0.06f, r);  // Haut, die sich vom Lack loest
    for (int k = 0; k < 6; ++k) clickL(b, r.range(0.05f, 1.2f), 0.15f, r.range(600.0f, 1100.0f), r);
    ImpactSpec fist;
    fist.modes = modesConcrete(r.range(110.0f, 150.0f), r);
    fist.contact = 0.012f, fist.noise = 0.4f, fist.noiseHz = 500.0f;
    b.add(impact(fist, 1.0f, 0.4f, rate, r), 1.25f);
    finishL(b, level::kFoley + 1.0f, 0.22f, 0.7f, 18826u);
    return b.sound();
}
Sound hrStep(int rate, int var) {  // Faust (schwer) oder nackter Fuss (klatschend, Haut klebt am Beton)
    Rand r(18830u + (unsigned)var * 13u);
    Buf b(0.6f, rate);
    if (var % 2 == 0) {
        ImpactSpec fist;
        fist.modes = modesConcrete(r.range(90.0f, 130.0f), r);
        fist.contact = r.range(0.01f, 0.016f), fist.noise = 0.45f, fist.noiseHz = r.range(350.0f, 520.0f), fist.noiseT60 = 0.04f;
        b.add(impact(fist, 1.0f, 0.4f, rate, r), 0.0f);
        ImpactSpec bone;
        bone.modes = modesCeramic(r.range(900.0f, 1300.0f), 0.02f, r);
        bone.contact = 0.001f, bone.noise = 0.3f;
        b.add(impact(bone, 0.25f, 0.1f, rate, r), 0.002f);
        hissL(b, 0.02f, 0.18f, 1600.0f, 0.7f, 0.05f, r);  // die Knoechel schleifen
    } else {
        ImpactSpec slap;
        slap.modes = modesConcrete(r.range(170.0f, 230.0f), r);
        slap.contact = 0.004f, slap.noise = 0.9f, slap.noiseHz = r.range(1100.0f, 1600.0f), slap.noiseQ = 0.6f, slap.noiseT60 = 0.03f;
        b.add(impact(slap, 0.7f, 0.25f, rate, r), 0.0f);
        clickL(b, r.range(0.06f, 0.1f), 0.18f, 2200.0f, r);  // Haut loest sich
    }
    finishL(b, level::kStep + 2.0f, 0.2f, 0.6f, 18831u + (unsigned)var);
    return b.sound();
}
// Laut durch den Gehoergang: pfeifend, blubbernd, ohne Lippen (f0 hoch, schmale Formanten)
void canalVoice(Buf& b, float at, float len, float f0a, float f0b, float amp, float rough, Rand& r) {
    VoiceSpec v;
    v.f0 = [=](float tt) { return f0a + (f0b - f0a) * std::clamp(tt / len, 0.0f, 1.0f); };
    v.amp = [=](float tt) { return amp * envAHR(tt, 0.05f, len * 0.6f, len * 0.35f); };
    v.formants = {900.0f, 2600.0f, 4100.0f};
    v.bandwidths = {220.0f, 300.0f, 400.0f};
    v.jitter = 0.05f, v.shimmer = 0.2f, v.breath = 0.6f, v.rough = rough, v.roughHz = 31.0f, v.subharm = 0.2f, v.tiltHz = 5200.0f;
    voice(b, at, len, v, r);
}
Sound hrBreath(int rate) {
    Rand r(18840u);
    Buf b(1.6f, rate);
    breathe(b, 0.0f, 1.0f, true, 0.7f, 1.0f, r);
    for (int k = 0; k < 7; ++k) clickL(b, r.range(0.1f, 1.3f), r.range(0.12f, 0.3f), r.range(800.0f, 2400.0f), r);
    bubble(b, 1.1f, 2.0f, 0.15f, r);
    finishL(b, level::kCreature - 6.0f, 0.18f, 0.6f, 18841u);
    return b.sound();
}
Sound hrHum(int rate) {
    Rand r(18845u);
    Buf b(2.2f, rate);
    VoiceSpec v;
    v.f0 = [](float tt) { return 62.0f + 6.0f * std::sin(tt * 3.0f); };
    v.amp = [](float tt) { return 0.8f * envAHR(tt, 0.3f, 1.2f, 0.5f); };
    v.formants = {380.0f, 1200.0f, 2400.0f};
    v.bandwidths = {120.0f, 180.0f, 260.0f};
    v.jitter = 0.04f, v.shimmer = 0.15f, v.breath = 0.3f, v.subharm = 0.5f, v.rough = 0.4f;
    voice(b, 0.0f, 2.0f, v, r);
    for (int k = 0; k < 10; ++k) clickL(b, r.range(0.1f, 1.9f), 0.2f, r.range(900.0f, 2000.0f), r);
    finishL(b, level::kCreature - 5.0f, 0.2f, 0.6f, 18846u);
    return b.sound();
}
Sound hrHeard(int rate) {  // Schnalzen, das Ohr schnappt herum (Knorpel)
    Rand r(18850u);
    Buf b(0.9f, rate);
    for (int k = 0; k < 4; ++k) clickL(b, 0.02f * (float)k, 0.6f, r.range(1400.0f, 2600.0f), r);
    hissL(b, 0.08f, 0.25f, 3200.0f, 1.2f, 0.25f, r);  // der Rand faltet sich auf
    canalVoice(b, 0.12f, 0.45f, 420.0f, 610.0f, 0.5f, 0.3f, r);
    finishL(b, level::kCreature - 2.0f, 0.18f, 0.6f, 18851u);
    return b.sound();
}
Sound hrRush(int rate) {  // Pfeifen durch den Gehoergang, Kehlsack pumpt, schwere Stoesse
    Rand r(18855u);
    Buf b(2.0f, rate);
    canalVoice(b, 0.0f, 1.6f, 520.0f, 880.0f, 0.9f, 0.7f, r);
    for (int k = 0; k < 4; ++k) {
        ImpactSpec fist;
        fist.modes = modesConcrete(r.range(90.0f, 120.0f), r);
        fist.contact = 0.012f, fist.noise = 0.4f, fist.noiseHz = 420.0f;
        b.add(impact(fist, 0.8f, 0.35f, rate, r), 0.15f + 0.38f * (float)k);
    }
    finishL(b, level::kCreature + 1.0f, 0.25f, 0.8f, 18856u);
    return b.sound();
}
Sound hrWarn(int rate) {  // drei schwere Schlaege in die Luft (die Faust schlaegt sich selbst auf die Brust), steigendes Zischen
    Rand r(18860u);
    Buf b(1.6f, rate);
    for (int k = 0; k < 3; ++k) {
        ImpactSpec chest;
        chest.modes = modesWood(r.range(70.0f, 95.0f), 0.08f, r);
        chest.contact = 0.014f, chest.noise = 0.5f, chest.noiseHz = 380.0f, chest.noiseT60 = 0.05f;
        b.add(impact(chest, 1.0f, 0.4f, rate, r), 0.15f + 0.3f * (float)k);
        clickL(b, 0.155f + 0.3f * (float)k, 0.25f, 1200.0f, r);
    }
    hissL(b, 0.9f, 0.55f, 3800.0f, 1.0f, 0.4f, r);
    canalVoice(b, 0.95f, 0.45f, 700.0f, 1150.0f, 0.5f, 0.2f, r);
    finishL(b, level::kCreature + 2.0f, 0.18f, 0.6f, 18861u);
    return b.sound();
}
Sound hrStrike(int rate) {  // die Faust trifft einen Koerper: dumpf, Knochen knackt
    Rand r(18865u);
    Buf b(1.0f, rate);
    ImpactSpec body;
    body.modes = modesWood(r.range(55.0f, 70.0f), 0.1f, r);
    body.contact = 0.02f, body.noise = 0.6f, body.noiseHz = 300.0f, body.noiseT60 = 0.06f;
    b.add(impact(body, 1.0f, 0.6f, rate, r), 0.0f);
    ImpactSpec crack;
    crack.modes = modesWood(r.range(900.0f, 1300.0f), 0.02f, r);
    crack.contact = 0.0008f, crack.noise = 0.7f, crack.noiseHz = 2400.0f;
    b.add(impact(crack, 0.5f, 0.1f, rate, r), 0.008f);
    finishL(b, level::kEvent + 3.0f, 0.15f, 0.6f, 18866u);
    return b.sound();
}
Sound hrMiss(int rate) {  // Faust auf Beton: Knall, Splitter rieseln
    Rand r(18870u);
    Buf b(1.4f, rate);
    ImpactSpec fl;
    fl.modes = modesConcrete(r.range(70.0f, 95.0f), r);
    fl.contact = 0.006f, fl.noise = 0.7f, fl.noiseHz = 900.0f, fl.noiseT60 = 0.05f;
    b.add(impact(fl, 1.0f, 0.6f, rate, r), 0.0f);
    grains(b, 0.05f, 0.7f, 30.0f, [](Buf& bb, float at, Rand& rr) {
        ImpactSpec g;
        g.modes = modesCeramic(rr.range(2500.0f, 5000.0f), 0.02f, rr);
        g.contact = 0.0004f, g.noise = 0.2f;
        bb.add(impact(g, rr.range(0.04f, 0.12f), 0.05f, bb.rate, rr), at);
    }, r, [](float t) { return std::exp(-t * 4.0f); });
    finishL(b, level::kEvent + 2.0f, 0.3f, 0.8f, 18871u);
    return b.sound();
}
Sound hrFlinch(int rate) {  // Kreischen durch den Gehoergang, Knorpel knistert
    Rand r(18875u);
    Buf b(1.6f, rate);
    canalVoice(b, 0.0f, 1.1f, 1300.0f, 1900.0f, 1.0f, 0.9f, r);
    canalVoice(b, 0.03f, 1.0f, 1335.0f, 2010.0f, 0.5f, 0.6f, r);
    grains(b, 0.0f, 0.9f, 60.0f, [](Buf& bb, float at, Rand& rr) { clickL(bb, at, rr.range(0.05f, 0.18f), rr.range(2500.0f, 5000.0f), rr); }, r);
    finishL(b, level::kCreature + 2.0f, 0.25f, 0.8f, 18876u);
    return b.sound();
}
Sound hrGone(int rate) {  // eine Tuer schliesst sich leise, die Klinke rastet ein
    Rand r(18880u);
    Buf b(1.4f, rate);
    CreakSpec cs;
    cs.rate0 = 22.0f, cs.rate1 = 10.0f, cs.formants = {300.0f, 800.0f, 1900.0f}, cs.q = 9.0f;
    creak(b, 0.0f, 0.8f, cs, 0.35f, r);
    ImpactSpec shut;
    shut.modes = modesWood(r.range(95.0f, 120.0f), 0.12f, r);
    shut.contact = 0.008f, shut.noise = 0.3f, shut.noiseHz = 600.0f;
    b.add(impact(shut, 0.6f, 0.5f, rate, r), 0.8f);
    ImpactSpec latch;
    latch.modes = modesMetalPlate(2300.0f, 0.1f, r);
    latch.contact = 0.0007f, latch.noise = 0.2f;
    b.add(impact(latch, 0.35f, 0.2f, rate, r), 0.83f);
    finishL(b, level::kEvent - 3.0f, 0.3f, 0.8f, 18881u);
    return b.sound();
}
Sound hrScrape(int rate) {  // die Faust tastet ueber den Boden und klopft zweimal kurz
    Rand r(18885u);
    Buf b(1.2f, rate);
    hissL(b, 0.0f, 0.55f, 1300.0f, 0.6f, 0.2f, r);
    for (int k = 0; k < 2; ++k) {
        ImpactSpec sp;
        sp.modes = modesConcrete(r.range(120.0f, 160.0f), r);
        sp.contact = 0.009f, sp.noise = 0.5f, sp.noiseHz = 600.0f;
        b.add(impact(sp, 0.7f, 0.3f, rate, r), 0.6f + 0.22f * (float)k);
    }
    finishL(b, level::kFoley, 0.22f, 0.7f, 18886u);
    return b.sound();
}

// --- Wurzelpistole ---------------------------------------------------------------------------------------------------------
Sound e39Creak(int rate) {  // Wurzeln heben sich: Rinde reibt, feines Knarren
    Rand r(3900u);
    Buf b(1.6f, rate);
    CreakSpec cs;
    cs.rate0 = 60.0f, cs.rate1 = 35.0f, cs.jitter = 0.4f, cs.formants = {900.0f, 2300.0f, 4200.0f}, cs.q = 5.0f;
    creak(b, 0.05f, 1.3f, cs, 0.3f, r);
    hissL(b, 0.0f, 1.4f, 2600.0f, 0.8f, 0.05f, r);
    finishL(b, level::kFoley - 4.0f, 0.15f, 0.6f, 3901u);
    return b.sound();
}
Sound e39Wrap(int rate) {  // die Wurzeln schlingen sich um Hand und Unterarm: schnelles Reiben, nasses Zuziehen
    Rand r(3905u);
    Buf b(1.5f, rate);
    for (int k = 0; k < 5; ++k) hissL(b, 0.06f * (float)k, 0.22f, r.range(1800.0f, 3200.0f), 1.0f, 0.25f, r);
    CreakSpec cs;
    cs.rate0 = 80.0f, cs.rate1 = 140.0f, cs.formants = {1200.0f, 2800.0f, 4600.0f}, cs.q = 6.0f;
    creak(b, 0.2f, 0.7f, cs, 0.4f, r);
    for (int k = 0; k < 6; ++k) clickL(b, 0.3f + r.range(0.0f, 0.8f), 0.2f, r.range(700.0f, 1400.0f), r);
    finishL(b, level::kFoley + 1.0f, 0.12f, 0.5f, 3906u);
    return b.sound();
}
Sound e39Suck(int rate) {  // es trinkt: Pulse, nasses Saugen, ein Pochen (der eigene Puls)
    Rand r(3910u);
    Buf b(2.6f, rate);
    for (int k = 0; k < 4; ++k) {
        const float at = 0.1f + 0.58f * (float)k;
        hissL(b, at, 0.32f, 900.0f, 1.4f, 0.22f, r);
        bubble(b, at + 0.12f, r.range(2.5f, 4.0f), 0.2f, r);
        ImpactSpec pulse;
        pulse.modes = modesWood(48.0f, 0.12f, r);
        pulse.contact = 0.03f, pulse.noise = 0.1f;
        b.add(impact(pulse, 0.6f, 0.3f, rate, r), at + 0.02f);
    }
    finishL(b, level::kFoley, 0.1f, 0.5f, 3911u);
    return b.sound();
}
Sound e39Bang(int rate) {  // Feuersalz statt Pulver: trockener, heller Knall, nasses Nachplatzen, langer Hall im Hof
    Rand r(3915u);
    Buf b(2.8f, rate);
    for (size_t i = 0; i < b.at(0.012f); ++i) b.x[i] += r.bi() * std::exp(-(float)i / (0.0025f * (float)rate));
    ImpactSpec body;
    body.modes = modesMetalPipe(r.range(380.0f, 460.0f), 0.06f, r);
    body.contact = 0.0004f, body.noise = 0.9f, body.noiseHz = 2400.0f, body.noiseT60 = 0.03f;
    b.add(impact(body, 1.0f, 0.4f, rate, r), 0.0f);
    for (int k = 0; k < 5; ++k) bubble(b, 0.06f + 0.04f * (float)k, r.range(2.0f, 4.0f), 0.18f, r);
    hissL(b, 0.02f, 0.4f, 1200.0f, 0.5f, 0.15f, r);
    reverb(b, dsp::RoomSpec{2.6f, 2.8f, 1.4f, 2.6f, 0.03f, 0.6f}, 0.45f, 1.0f, 3916u);  // der Hof hallt
    trim(b);
    normalize(b, level::kEvent + 6.0f);
    return b.sound();
}
Sound e39Drip(int rate) {
    Rand r(3920u);
    Buf b(0.5f, rate);
    bubble(b, 0.0f, 1.6f, 0.5f, r);
    clickL(b, 0.01f, 0.2f, 2400.0f, r);
    finishL(b, level::kFoley - 8.0f, 0.1f, 0.5f, 3921u);
    return b.sound();
}
Sound e39Wilt(int rate) {  // welk: trockenes Knistern, ein Seufzen der Fasern
    Rand r(3925u);
    Buf b(1.8f, rate);
    grains(b, 0.0f, 1.5f, 40.0f, [](Buf& bb, float at, Rand& rr) { clickL(bb, at, rr.range(0.04f, 0.12f), rr.range(3000.0f, 6000.0f), rr); }, r,
           [](float t) { return std::exp(-t * 1.5f); });
    hissL(b, 0.1f, 1.4f, 700.0f, 0.6f, 0.08f, r);
    finishL(b, level::kFoley - 4.0f, 0.12f, 0.5f, 3926u);
    return b.sound();
}
Sound e39Tear(int rate) {  // losgerissen: Fasern reissen aus dem Fleisch - nasses Reissen, ein Peitschen der Wurzeln
    Rand r(3930u);
    Buf b(1.4f, rate);
    for (int k = 0; k < 4; ++k) hissL(b, 0.03f * (float)k, 0.18f, r.range(1400.0f, 2600.0f), 0.9f, 0.32f, r);
    grains(b, 0.0f, 0.35f, 90.0f, [](Buf& bb, float at, Rand& rr) { clickL(bb, at, rr.range(0.08f, 0.25f), rr.range(900.0f, 2200.0f), rr); }, r,
           [](float t) { return std::exp(-t * 6.0f); });
    for (int k = 0; k < 3; ++k) bubble(b, 0.05f + 0.07f * (float)k, r.range(2.0f, 3.5f), 0.25f, r);
    CreakSpec cs;
    cs.rate0 = 160.0f, cs.rate1 = 40.0f, cs.formants = {800.0f, 2100.0f, 3900.0f}, cs.q = 5.0f;
    creak(b, 0.25f, 0.6f, cs, 0.3f, r);
    finishL(b, level::kFoley + 2.0f, 0.12f, 0.5f, 3931u);
    return b.sound();
}

}  // namespace

void addListenerSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 3; ++v) d.push_back({std::format("hr_knock_{}", v), [=] { return hrKnock(rate, v); }});
    d.push_back({"hr_listen", [=] { return hrListen(rate); }});
    d.push_back({"hr_lost", [=] { return hrLost(rate); }});
    d.push_back({"hr_door", [=] { return hrDoor(rate); }});
    d.push_back({"hr_unfold", [=] { return hrUnfold(rate); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("hr_step_{}", v), [=] { return hrStep(rate, v); }});
    d.push_back({"hr_breath", [=] { return hrBreath(rate); }});
    d.push_back({"hr_hum", [=] { return hrHum(rate); }});
    d.push_back({"hr_heard", [=] { return hrHeard(rate); }});
    d.push_back({"hr_rush", [=] { return hrRush(rate); }});
    d.push_back({"hr_warn", [=] { return hrWarn(rate); }});
    d.push_back({"hr_strike", [=] { return hrStrike(rate); }});
    d.push_back({"hr_miss", [=] { return hrMiss(rate); }});
    d.push_back({"hr_flinch", [=] { return hrFlinch(rate); }});
    d.push_back({"hr_gone", [=] { return hrGone(rate); }});
    d.push_back({"hr_scrape", [=] { return hrScrape(rate); }});
    d.push_back({"e39_creak", [=] { return e39Creak(rate); }});
    d.push_back({"e39_wrap", [=] { return e39Wrap(rate); }});
    d.push_back({"e39_suck", [=] { return e39Suck(rate); }});
    d.push_back({"e39_bang", [=] { return e39Bang(rate); }});
    d.push_back({"e39_drip", [=] { return e39Drip(rate); }});
    d.push_back({"e39_wilt", [=] { return e39Wilt(rate); }});
    d.push_back({"e39_tear", [=] { return e39Tear(rate); }});  // V13.3 Teil 4 Level 188: losgerissen
}

}  // namespace lim::audio
