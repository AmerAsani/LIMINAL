// V13.3: Klaenge der Plague Goblins (Entity 25, KREATUR_PlagueGoblins.md). Die Seite: heiseres Kraechzen, Kreischen und
// Schnattern; manche ahmen menschliche Sprache nach (maessig). Alles Weitere ist Auslegung nach dem Koerper (Kraehenschnabel,
// Krallen, Umhang) und der Sanduhr am Stock der Torwaechterin:
//
//   Schritt     zwei, drei harte Krallentipper und ein Streifen des Umhangs
//   Schnattern  rasches Klappern des Schnabels unter der Maske, dazwischen heisere, gehauchte Silben (leise - man hoert die
//               Bande, bevor man sie sieht)
//   Kraechzen   rauhes "Kraa" (Raben-Formanten, Periodenverdopplung), fallend
//   Warnschrei  lautes, scharfes Kreischen in zwei Stoessen
//   Wuehlen     Stoff, der gezerrt wird, Reissverschluss-Ticken, Krallen auf Plastik - dicht am Ohr
//   Verstauen   ein Schlucken des Umhangs: Rascheln, dumpfer Schlag, ein kurzer, fallender, gedaempfter Ton (verschwindet)
//   Fauchen     Zischen mit ratterndem Schnabel und einem tiefen, gequetschten Ton darunter (1,4 s - die Ankuendigung)
//   Kreischen   der Sprung: hoher, rauher Schrei
//   Portal      rieselnder Sand, ein Glaston, ein tiefes Einatmen der Luft (oeffnen) bzw. Zusammenfallen (schliessen)
//   Tod         erstickter, fallender Schrei, ein leichter Koerper schlaegt auf
//   Nachahmen   zwei gekraechzte Silben, die wie ein Wort klingen sollen ("Hal-lo?"), Tonhoehe wackelt
//   Beil        Zischen des Schwungs; Treffer: dumpfer Schlag, Knacken
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {

void click(Buf& b, float at, float amp, float hz, float q, Rand& r) {
    const size_t a = b.at(at);
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    for (size_t k = 0; k < 160 && a + k < b.n(); ++k) b.x[a + k] += bp.run(k < 3 ? r.bi() * 3.0f : 0.0f) * amp;
}
void clack(Buf& b, float at, float amp, Rand& r) {  // Schnabel: kleines, hartes Holz
    ImpactSpec sp;
    sp.modes = modesWood(r.range(1700.0f, 2600.0f), 0.012f, r);
    sp.contact = 0.0008f, sp.noise = 0.5f, sp.noiseHz = 4200.0f, sp.noiseT60 = 0.006f;
    b.add(impact(sp, amp, 0.05f, b.rate, r), at);
}
void rustle(Buf& b, float t0, float len, float amp, float hz, Rand& r) {  // Stoff: Rauschen in Stoessen
    Biquad bp = Biquad::make(Biquad::BP, hz, 0.8f, (float)b.rate);
    float g = 0.0f, tgt = 0.0f;
    for (size_t i = b.at(t0); i < b.at(t0 + len) && i < b.n(); ++i) {
        if (r.chance(60.0f / (float)b.rate)) tgt = r.range(0.1f, 1.0f);
        g += (tgt - g) * 0.004f;
        const float u = (b.t(i) - t0) / len;
        b.x[i] += bp.run(r.bi()) * amp * g * std::sin(3.14159f * std::clamp(u, 0.0f, 1.0f));
    }
}
VoiceSpec corvid(float f0a, float f0b, float len, float rough) {
    VoiceSpec v;
    v.f0 = [=](float t) { return f0a + (f0b - f0a) * std::clamp(t / len, 0.0f, 1.0f); };
    v.amp = [=](float t) { return envAHR(t, 0.02f, len * 0.55f, len * 0.4f); };
    v.formants = {950.0f, 1650.0f, 2900.0f};
    v.bandwidths = {160.0f, 220.0f, 320.0f};
    v.jitter = 0.05f, v.shimmer = 0.25f, v.subharm = 0.35f, v.breath = 0.3f, v.rough = rough, v.roughHz = 34.0f, v.tiltHz = 4200.0f;
    return v;
}

Sound gStep(int rate, int var) {
    Rand r(25000u + (unsigned)var);
    Buf b(0.35f, rate);
    const int n = r.irange(2, 3);
    for (int i = 0; i < n; ++i) click(b, 0.01f + 0.022f * (float)i + r.range(0.0f, 0.006f), r.range(0.5f, 1.0f), r.range(3200.0f, 5200.0f), 6.0f, r);
    rustle(b, 0.0f, 0.22f, 0.08f, 1800.0f, r);
    trim(b);
    normalize(b, level::kStep - 6.0f);
    return b.sound();
}

Sound gChatter(int rate, int var) {
    Rand r(25100u + (unsigned)var);
    const float L = r.range(0.5f, 0.9f);
    Buf b(L + 0.3f, rate);
    grains(b, 0.0f, L, 26.0f, [&](Buf& bb, float t, Rand& rr) { clack(bb, t, rr.range(0.3f, 1.0f), rr); }, r,
           [=](float t) { return 0.4f + 0.6f * std::sin(3.14159f * std::clamp(t / L, 0.0f, 1.0f)); });
    for (float at = 0.05f; at < L; at += r.range(0.12f, 0.24f)) {  // gehauchte Silben
        const float sl = r.range(0.05f, 0.1f);
        VoiceSpec v = corvid(r.range(620.0f, 820.0f), r.range(520.0f, 700.0f), sl, 0.5f);
        v.breath = 0.6f;
        voice(b, at, sl, v, r);
    }
    trim(b);
    normalize(b, level::kCreature - 6.0f);
    return b.sound();
}

Sound gCroak(int rate, int var) {
    Rand r(25200u + (unsigned)var);
    const float L = r.range(0.32f, 0.52f);
    Buf b(L + 0.4f, rate);
    voice(b, 0.0f, L, corvid(r.range(420.0f, 520.0f), r.range(300.0f, 380.0f), L, 0.65f), r);
    if (r.chance(0.6f)) voice(b, L + 0.08f, L * 0.55f, corvid(r.range(400.0f, 470.0f), 320.0f, L * 0.55f, 0.7f), r);
    clack(b, L + 0.02f, 0.6f, r);
    trim(b);
    normalize(b, level::kCreature - 2.0f);
    return b.sound();
}

Sound gSquawk(int rate, int var) {
    Rand r(25300u + (unsigned)var);
    Buf b(1.0f, rate);
    for (int k = 0; k < 2; ++k) {
        const float L = k == 0 ? r.range(0.28f, 0.36f) : r.range(0.2f, 0.26f);
        VoiceSpec v = corvid(r.range(980.0f, 1150.0f), r.range(720.0f, 820.0f), L, 0.85f);
        v.formants = {1300.0f, 2300.0f, 3600.0f};
        v.subharm = 0.55f;
        voice(b, k == 0 ? 0.0f : 0.36f, L, v, r);
    }
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound gRummage(int rate) {
    Rand r(25400u);
    Buf b(1.4f, rate);
    rustle(b, 0.0f, 1.25f, 0.5f, 2600.0f, r);
    rustle(b, 0.1f, 1.1f, 0.3f, 5200.0f, r);
    for (float at = 0.05f; at < 1.2f; at += r.range(0.03f, 0.09f)) click(b, at, r.range(0.2f, 0.6f), r.range(2600.0f, 4800.0f), 5.0f, r);
    for (float at = 0.2f; at < 1.1f; at += r.range(0.15f, 0.3f)) {  // Krallen auf Plastik, Flaschen ruecken
        ImpactSpec sp;
        sp.modes = modesPlastic(r.range(700.0f, 1300.0f), 0.05f, r);
        sp.contact = 0.002f, sp.noise = 0.3f;
        b.add(impact(sp, r.range(0.3f, 0.7f), 0.12f, rate, r), at);
    }
    trim(b);
    normalize(b, level::kFoley + 2.0f);
    return b.sound();
}

Sound gStash(int rate) {
    Rand r(25500u);
    Buf b(0.9f, rate);
    rustle(b, 0.0f, 0.45f, 0.6f, 1600.0f, r);
    ImpactSpec sp;
    sp.modes = modesWood(r.range(160.0f, 220.0f), 0.05f, r);
    sp.contact = 0.006f, sp.noise = 0.2f, sp.noiseHz = 600.0f;
    b.add(impact(sp, 0.7f, 0.25f, rate, r), 0.28f);
    // fallender, gedaempfter Ton - was hineinfaellt, ist fort
    Biquad lp = Biquad::make(Biquad::LP, 900.0f, 0.7f, (float)rate);
    float ph = 0.0f;
    for (size_t i = b.at(0.3f); i < b.at(0.8f); ++i) {
        const float u = (b.t(i) - 0.3f) / 0.5f;
        ph += 6.2831853f * (520.0f - 380.0f * u) / (float)rate;
        b.x[i] += lp.run(std::sin(ph)) * 0.12f * (1.0f - u) * (1.0f - u);
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound gHiss(int rate) {
    Rand r(25600u);
    Buf b(1.7f, rate);
    Biquad hp = Biquad::make(Biquad::HP, 3800.0f, 0.7f, (float)rate), bp = Biquad::make(Biquad::BP, 6500.0f, 1.2f, (float)rate);
    for (size_t i = 0; i < b.at(1.45f); ++i) {
        const float t = b.t(i);
        const float env = envAHR(t, 0.08f, 1.0f, 0.35f) * (0.75f + 0.25f * std::sin(t * 23.0f));
        b.x[i] += (hp.run(r.bi()) * 0.5f + bp.run(r.bi()) * 0.4f) * env;
    }
    for (float at = 0.15f; at < 1.4f; at += r.range(0.018f, 0.03f)) clack(b, at, r.range(0.15f, 0.4f), r);  // Schnabel rattert
    VoiceSpec v = corvid(190.0f, 150.0f, 1.3f, 0.9f);
    v.formants = {600.0f, 1100.0f, 2500.0f};
    v.subharm = 0.8f, v.breath = 0.7f;
    voice(b, 0.1f, 1.3f, v, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound gScreech(int rate, int var) {
    Rand r(25700u + (unsigned)var);
    Buf b(0.8f, rate);
    const float L = r.range(0.36f, 0.48f);
    VoiceSpec v = corvid(r.range(1150.0f, 1300.0f), r.range(1450.0f, 1650.0f), L, 0.95f);
    v.formants = {1500.0f, 2700.0f, 4000.0f};
    v.subharm = 0.6f, v.breath = 0.45f;
    voice(b, 0.0f, L, v, r);
    rustle(b, 0.0f, 0.3f, 0.25f, 2200.0f, r);  // der Umhang beim Absprung
    trim(b);
    normalize(b, level::kCreature + 1.0f);
    return b.sound();
}

Sound gPortal(int rate, bool open) {
    Rand r(open ? 25800u : 25850u);
    const float L = open ? 2.2f : 1.4f;
    Buf b(L + 1.5f, rate);
    // Sand rieselt (dichte Koerner), beim Oeffnen anschwellend
    grains(b, 0.0f, L, 900.0f, [&](Buf& bb, float t, Rand& rr) { click(bb, t, rr.range(0.05f, 0.25f), rr.range(2500.0f, 7000.0f), 3.0f, rr); }, r,
           [=](float t) { const float u = std::clamp(t / L, 0.0f, 1.0f); return open ? 0.2f + 0.8f * std::sin(3.14159f * u) : 1.0f - u; });
    // tiefes Einatmen der Luft (oeffnen) / Zusammenfallen (schliessen)
    Biquad lp = Biquad::make(Biquad::LP, 260.0f, 0.9f, (float)rate);
    for (size_t i = 0; i < b.at(L); ++i) {
        const float u = b.t(i) / L;
        const float env = open ? std::sin(3.14159f * u) * (0.3f + 0.7f * u) : (1.0f - u) * (1.0f - u);
        b.x[i] += lp.run(r.bi()) * 1.6f * env;
    }
    // Glaston der Sanduhr
    ImpactSpec sp;
    sp.modes = modesGlass(open ? 2350.0f : 1980.0f, 1.6f, r);
    sp.contact = 0.0007f, sp.noise = 0.05f;
    b.add(impact(sp, 0.5f, 2.0f, rate, r), open ? 0.35f : L * 0.8f);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound gDeath(int rate) {
    Rand r(25900u);
    Buf b(1.4f, rate);
    VoiceSpec v = corvid(900.0f, 260.0f, 0.55f, 0.95f);
    v.formants = {1100.0f, 2000.0f, 3300.0f};
    v.subharm = 0.7f, v.breath = 0.6f;
    voice(b, 0.0f, 0.55f, v, r);
    ImpactSpec sp;
    sp.modes = modesWood(r.range(120.0f, 170.0f), 0.06f, r);
    sp.contact = 0.008f, sp.noise = 0.25f, sp.noiseHz = 500.0f;
    b.add(impact(sp, 0.9f, 0.3f, rate, r), 0.5f);
    rustle(b, 0.48f, 0.45f, 0.35f, 1500.0f, r);
    trim(b);
    normalize(b, level::kCreature);
    return b.sound();
}

Sound gMimic(int rate, int var) {
    Rand r(26000u + (unsigned)var);
    Buf b(1.3f, rate);
    // zwei Silben: offenes "a" (heiser), dann ein gerundetes "o" mit steigender Frage
    VoiceSpec a = corvid(330.0f, 300.0f, 0.3f, 0.55f);
    a.formants = {780.0f, 1250.0f, 2600.0f};
    a.f0 = [](float t) { return 320.0f + 25.0f * std::sin(t * 31.0f); };
    voice(b, 0.0f, 0.3f, a, r);
    VoiceSpec o = corvid(300.0f, 410.0f, 0.42f, 0.6f);
    o.formants = {480.0f, 880.0f, 2500.0f};
    voice(b, 0.36f, 0.42f, o, r);
    clack(b, 0.33f, 0.4f, r);
    trim(b);
    normalize(b, level::kCreature - 3.0f);
    return b.sound();
}

Sound hatchetSwing(int rate) {
    Rand r(26100u);
    Buf b(0.5f, rate);
    Biquad bp;
    for (size_t i = 0; i < b.at(0.32f); ++i) {
        const float u = b.t(i) / 0.32f;
        if ((i & 31u) == 0) bp.set(Biquad::BP, 420.0f + 1500.0f * std::sin(3.14159f * u), 1.6f, (float)rate);
        b.x[i] += bp.run(r.bi()) * std::pow(std::sin(3.14159f * u), 2.0f);
    }
    trim(b);
    normalize(b, level::kFoley);
    return b.sound();
}

Sound hatchetHit(int rate) {
    Rand r(26200u);
    Buf b(0.6f, rate);
    ImpactSpec sp;
    sp.modes = modesWood(r.range(140.0f, 190.0f), 0.08f, r);
    sp.contact = 0.004f, sp.noise = 0.5f, sp.noiseHz = 900.0f, sp.noiseT60 = 0.03f;
    b.add(impact(sp, 1.0f, 0.4f, rate, r), 0.0f);
    for (int k = 0; k < 4; ++k) click(b, 0.01f + 0.012f * (float)k, r.range(0.3f, 0.6f), r.range(1800.0f, 3200.0f), 4.0f, r);  // Knacken
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

}  // namespace

void addGoblinSounds(std::vector<SoundDef>& d, int rate) {
    for (int v = 0; v < 3; ++v) d.push_back({std::format("goblin_step_{}", v), [=] { return gStep(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("goblin_chatter_{}", v), [=] { return gChatter(rate, v); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("goblin_croak_{}", v), [=] { return gCroak(rate, v); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("goblin_squawk_{}", v), [=] { return gSquawk(rate, v); }});
    d.push_back({"goblin_rummage", [=] { return gRummage(rate); }});
    d.push_back({"goblin_stash", [=] { return gStash(rate); }});
    d.push_back({"goblin_hiss", [=] { return gHiss(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("goblin_screech_{}", v), [=] { return gScreech(rate, v); }});
    d.push_back({"goblin_portal_open", [=] { return gPortal(rate, true); }});
    d.push_back({"goblin_portal_close", [=] { return gPortal(rate, false); }});
    d.push_back({"goblin_death", [=] { return gDeath(rate); }});
    for (int v = 0; v < 2; ++v) d.push_back({std::format("goblin_mimic_{}", v), [=] { return gMimic(rate, v); }});
    d.push_back({"hatchet_swing", [=] { return hatchetSwing(rate); }});
    d.push_back({"hatchet_hit", [=] { return hatchetHit(rate); }});
}

}  // namespace lim::audio
