// V11: Klangbetten der zehn Ebenen, ferne Ereignisse und die Musik aus dem Hotel.
//
// Klangbetten bestehen aus zwei bis drei Schichten mit teilerfremden Laengen (z. B. 23, 29, 37 s).
// Laufen sie gleichzeitig, wiederholt sich ihre Ueberlagerung erst nach Stunden. Sie liegen mit
// 24 kHz vor (tiefe, ruhige Klaenge brauchen keine 48 kHz; spart Speicher) und sind leise: Stille
// ist Teil der Gestaltung (AUDIO_DESIGN.md).
//
// Die Musik im Hotel ist eine eigene Komposition, die hier erzeugt wird: ein langsamer Akkordgang
// mit Bass, E-Piano, Vibraphon und Besen, gespielt ueber einen alten Lautsprecher, gehoert durch
// Waende.
#include <cmath>
#include <format>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;
using dsp::Mode;

namespace {

constexpr int kBedRate = 24000;

// Nachklang am Ende in den Anfang falten: musikalisch richtige Schleife (Laenge L)
void wrapTail(Buf& b, float L) {
    const size_t n = b.at(L);
    if (b.n() <= n) {
        b.x.resize(n, 0.0f);
        return;
    }
    for (size_t i = n; i < b.n(); ++i) b.x[(i - n) % n] += b.x[i];
    b.x.resize(n);
}

// --- Klangbetten ---------------------------------------------------------------------------------------
// Grundton eines Raums: rosa Rauschen, tief gefiltert, mit langsamen, unregelmaessigen Schwankungen
Buf roomTone(float L, float lp, float hp, unsigned seed, float wobble = 0.15f) {
    Rand r(seed);
    Buf b(L + 1.5f, kBedRate);
    dsp::Pink pink;
    Biquad flp = Biquad::make(Biquad::LP, lp, 0.6f, (float)kBedRate), fhp = Biquad::make(Biquad::HP, hp, 0.6f, (float)kBedRate);
    float m = 1.0f, mv = 0.0f;
    for (size_t i = 0; i < b.n(); ++i) {
        mv += r.bi() * 0.000002f - (m - 1.0f) * 0.0000004f;
        mv *= 0.9999f;
        m = dsp::clampf(m + mv, 1.0f - wobble, 1.0f + wobble);
        b.x[i] = fhp.run(flp.run(pink.run(r.bi()))) * m;
    }
    return b;
}

// tiefes Gebaeudegrollen (braunes Rauschen, langsames Atmen)
Buf rumble(float L, float lp, unsigned seed, float breath = 0.4f) {
    Rand r(seed);
    Buf b(L + 1.5f, kBedRate);
    dsp::Brown br;
    Biquad f = Biquad::make(Biquad::LP, lp, 0.7f, (float)kBedRate), h = Biquad::make(Biquad::HP, 22.0f, 0.7f, (float)kBedRate);
    const float p1 = r.range(0.0f, 6.0f), p2 = r.range(0.0f, 6.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float s = 1.0f - breath * 0.5f * (1.0f + std::sin(t * 0.21f + p1) * std::sin(t * 0.077f + p2));
        b.x[i] = h.run(f.run(br.run(r.bi()))) * s;
    }
    return b;
}

// ferne Rohre: Rauschen durch drifiende Rohrresonanzen (Kammfilter)
Buf pipeResonance(float L, unsigned seed) {
    Rand r(seed);
    Buf b(L + 1.5f, kBedRate);
    dsp::DelayLine d;
    d.init(kBedRate / 30);
    Biquad bp = Biquad::make(Biquad::BP, 300.0f, 0.5f, (float)kBedRate);
    const float f0 = r.range(70.0f, 110.0f);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float f = f0 * (1.0f + 0.03f * std::sin(t * 0.05f));
        const float x = bp.run(r.bi()) * (0.4f + 0.6f * std::pow(0.5f + 0.5f * std::sin(t * 0.13f + 1.0f), 3.0f));
        const float y = x + d.read((float)kBedRate / f) * 0.85f;
        d.push(y);
        b.x[i] = y;
    }
    return b;
}

// warme Maschinendrohne: Teiltoene mit Schwebungen
Buf drone(float L, const std::vector<float>& freqs, float detune, unsigned seed) {
    Rand r(seed);
    Buf b(L, kBedRate);
    // nur Frequenzen, die in L periodisch sind
    std::vector<std::pair<float, float>> parts;
    for (float f : freqs) {
        const float f1 = std::round(f * L) / L, f2 = std::round(f * (1.0f + detune) * L) / L;
        parts.push_back({f1, r.range(0.0f, 6.0f)});
        parts.push_back({f2, r.range(0.0f, 6.0f)});
    }
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        float x = 0;
        for (size_t k = 0; k < parts.size(); ++k) x += std::sin(kTwoPi * parts[k].first * t + parts[k].second) / (1.0f + (float)(k / 2));
        b.x[i] = x;
    }
    return b;
}

// Puffer der Laenge L + Ueberhang -> nahtlose Schleife der Laenge L
Sound bed(Buf b, float L, float lufs) {
    const size_t n = std::min(b.n(), b.at(L));
    const float extra = (float)(b.n() - n) / (float)b.rate;
    if (extra > 0.05f) loopify(b, extra);
    normalize(b, lufs);
    return b.sound(true);
}

Sound mix2(Buf a, const Buf& b, float gb) {
    const size_t n = std::min(a.n(), b.n());
    for (size_t i = 0; i < n; ++i) a.x[i] += b.x[i] * gb;
    a.x.resize(n);
    Sound s;
    s.samples = a.x;
    s.rate = a.rate;
    s.loop = true;
    return s;
}

// Knistern eines Feuers (Level 19: das Gluehen unter den Dielen)
Buf glowCrackle(float L, unsigned seed) {
    Rand r(seed);
    Buf b(L + 1.5f, kBedRate);
    dsp::Brown br;
    dsp::Pink pink;
    Biquad roar = Biquad::make(Biquad::LP, 160.0f, 0.7f, (float)kBedRate), hiss = Biquad::make(Biquad::HP, 1800.0f, 0.7f, (float)kBedRate);
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        b.x[i] = roar.run(br.run(r.bi())) * (0.8f + 0.2f * std::sin(t * 0.7f)) + hiss.run(pink.run(r.bi())) * 0.05f;
    }
    grains(b, 0.0f, L + 1.5f, 9.0f, [&](Buf& bb, float t, Rand& rr) {
        ImpactSpec sp;
        sp.modes = modesConcrete(rr.range(600.0f, 2400.0f), rr);
        sp.contact = 0.0003f, sp.noise = 0.8f, sp.noiseHz = rr.range(2000.0f, 5000.0f), sp.noiseT60 = 0.004f;
        bb.add(impact(sp, rr.range(0.05f, 0.4f), 0.04f, bb.rate, rr), t);
    }, r, [](float t) { return 0.4f + 0.6f * std::fabs(std::sin(t * 0.37f)); });
    return b;
}

Buf windRoof(float L, unsigned seed) {
    Rand r(seed);
    Buf b(L + 1.5f, kBedRate);
    dsp::Svf f;
    dsp::Pink pink;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        const float g = 0.5f + 0.5f * std::sin(t * 0.17f + 1.0f) * std::sin(t * 0.061f);
        f.set(350.0f + 300.0f * g, 3.0f, (float)kBedRate);
        b.x[i] = f.bp(pink.run(r.bi())) * (0.3f + 0.7f * g);
    }
    return b;
}

// --- Musik aus dem Hotel (eigene Komposition) --------------------------------------------------------
float midiHz(float m) { return 440.0f * std::pow(2.0f, (m - 69.0f) / 12.0f); }

Sound hotelMusic(int rate) {
    (void)rate;
    Rand r(9100u);
    const int sr = kBedRate;
    const float beat = 60.0f / 64.0f;  // 64 Schlaege je Minute, sehr langsam
    const int bars = 8;
    const float L = beat * 4.0f * (float)bars;
    Buf b(L + 4.0f, sr);
    // Akkordgang ii-V-I-VI, je zwei Takte (Grundtoene, Stimmen als MIDI-Noten)
    struct Chord {
        int root;
        std::vector<int> voicing;
        std::vector<int> scale;
    };
    const Chord prog[4] = {
        {38, {53, 57, 60, 64}, {62, 64, 65, 67, 69, 72}},  // d-Moll 9
        {43, {53, 59, 64, 69}, {62, 64, 67, 69, 71}},      // G 13
        {36, {52, 55, 59, 62}, {60, 62, 64, 67, 69, 71}},  // C maj 9
        {45, {55, 58, 61, 65}, {61, 64, 67, 69, 70}},      // A 7 b9
    };
    auto ep = [&](float at, float hz, float amp) {  // E-Piano: Zwei-Operator-FM, weich
        const size_t a = b.at(at), n = b.at(2.8f);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = (float)i / (float)sr;
            const float idx = 1.4f * std::exp(-t / 0.25f);
            b.x[a + i] += std::sin(kTwoPi * hz * t + idx * std::sin(kTwoPi * hz * t)) * amp * envAD(t, 0.004f, 2.4f);
        }
    };
    auto bass = [&](float at, float hz, float amp) {
        const size_t a = b.at(at), n = b.at(1.6f);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = (float)i / (float)sr;
            const float w = kTwoPi * hz * t;
            b.x[a + i] += (std::sin(w) + 0.3f * std::sin(2.0f * w) + 0.1f * std::sin(3.0f * w)) * amp * envAD(t, 0.006f, 1.3f);
        }
    };
    auto vibe = [&](float at, float hz, float amp) {  // Vibraphon: Klangstab (1 : 4 : 10), Tremolo
        const size_t a = b.at(at), n = b.at(3.0f);
        for (size_t i = 0; i < n && a + i < b.n(); ++i) {
            const float t = (float)i / (float)sr;
            const float trem = 0.8f + 0.2f * std::sin(kTwoPi * 5.2f * t);
            const float y = std::sin(kTwoPi * hz * t) * envAD(t, 0.002f, 2.6f) + 0.15f * std::sin(kTwoPi * hz * 4.0f * t) * envAD(t, 0.001f, 0.5f);
            b.x[a + i] += y * amp * trem;
        }
    };
    int mel = 69;
    for (int bar = 0; bar < bars; ++bar) {
        const Chord& c = prog[(bar / 2) % 4];
        const float t0 = (float)bar * 4.0f * beat;
        // Bass: Grundton auf 1, Quinte auf 3, ein Durchgang auf 4
        bass(t0, midiHz((float)c.root), 0.9f);
        bass(t0 + 2.0f * beat, midiHz((float)c.root + 7.0f), 0.6f);
        if (r.chance(0.5f)) bass(t0 + 3.0f * beat, midiHz((float)c.root + (r.chance(0.5f) ? 5.0f : 10.0f)), 0.4f);
        // E-Piano: Akkord auf 1 (bei jedem zweiten Takt), kurzes Anschlagen auf 2+
        if (bar % 2 == 0)
            for (int nte : c.voicing) ep(t0 + r.range(0.0f, 0.03f), midiHz((float)nte), 0.18f);
        for (int nte : c.voicing) ep(t0 + 1.5f * beat + r.range(0.0f, 0.02f), midiHz((float)nte), 0.08f);
        // Melodie: ruhige Linie aus Akkordtoenen, nahe am vorigen Ton
        for (int k = 0; k < 2; ++k) {
            if (r.chance(0.25f)) continue;
            int best = c.scale[0], bd = 99;
            for (int s : c.scale) {
                const int cand = s + 12 * ((mel - s + 6) / 12);
                const int dd = std::abs(cand - mel - r.irange(-3, 3));
                if (dd < bd) bd = dd, best = cand;
            }
            mel = std::clamp(best, 64, 79);
            vibe(t0 + (float)(k * 2) * beat + r.range(0.0f, 0.04f), midiHz((float)mel), 0.22f);
        }
    }
    // Besen: weiches Wischen auf der Trommel, Akzent auf 2 und 4
    Biquad bh = Biquad::make(Biquad::BP, 5000.0f, 0.6f, (float)sr);
    for (size_t i = 0; i < b.at(L); ++i) {
        const float t = b.t(i);
        const float bpos = std::fmod(t / beat, 1.0f);
        const int bt = (int)(t / beat) % 4;
        const float acc = (bt == 1 || bt == 3) ? 1.0f : 0.45f;
        b.x[i] += bh.run(r.bi()) * 0.05f * acc * (0.3f + 0.7f * std::exp(-bpos * 5.0f));
    }
    wrapTail(b, L);
    // alter Lautsprecher: Band begrenzt, leicht verzerrt, Gleichlaufschwankung, Knistern
    filter(b, Biquad::HP, 330.0f, 0.7f);
    filter(b, Biquad::LP, 3200.0f, 0.7f);
    filter(b, Biquad::Peak, 1400.0f, 1.2f, 4.0f);
    for (float& v : b.x) v = std::tanh(v * 1.8f) * 0.7f;
    {
        Buf w = b;
        double pos = 0.0;
        for (size_t i = 0; i < b.n(); ++i) {
            const float t = b.t(i);
            pos += 1.0 + 0.0025 * std::sin(kTwoPi * 0.5f * t) + 0.0008 * std::sin(kTwoPi * 6.1f * t);
            const size_t i0 = (size_t)pos % w.n(), i1 = (i0 + 1) % w.n();
            const float f = (float)(pos - std::floor(pos));
            b.x[i] = w.x[i0] * (1.0f - f) + w.x[i1] * f;
        }
    }
    grains(b, 0.0f, L, 9.0f, [&](Buf& bb, float t, Rand& rr) {
        const size_t a = bb.at(t);
        if (a < bb.n()) bb.x[a] += rr.bi() * 0.08f;
    }, r);
    // durch Waende und Flure: dumpf und verhallt
    filter(b, Biquad::LP, 1500.0f, 0.6f);
    dsp::RoomSpec room;
    room.t60Mid = 1.4f, room.t60Low = 1.8f, room.t60High = 0.5f, room.size = 1.8f;
    reverb(b, room, 0.7f, 0.5f, 9101u);
    wrapTail(b, L);
    normalize(b, level::kBed + 1.0f);
    return b.sound(true);
}

// Stimmengewirr hinter einer Wand (Level 5): keine Woerter, nur Silben, Lachen
Sound murmur(int rate, unsigned seed, int voices, float L, float lowpass, float lufs, bool laughter) {
    (void)rate;
    Rand r(seed);
    Buf b(L + 2.0f, kBedRate);
    static const float vowels[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
    for (int v = 0; v < voices; ++v) {
        const float f0 = r.range(95.0f, 230.0f);
        float t = r.range(0.0f, 2.0f);
        while (t < L) {
            const int phrase = r.irange(3, 9);
            for (int s = 0; s < phrase && t < L; ++s) {
                const float syl = r.range(0.11f, 0.24f);
                const int vw = r.irange(0, 4);
                VoiceSpec vs;
                const float fs = f0 * r.range(0.9f, 1.15f);
                vs.f0 = [=](float u) { return fs * (1.0f + 0.08f * std::sin(u * 3.0f)); };
                vs.amp = [](float u) { return std::sin(3.14159f * u); };
                vs.formants = {vowels[vw][0], vowels[vw][1], vowels[vw][2]};
                vs.breath = 0.3f, vs.jitter = 0.02f;
                voice(b, t, syl, vs, r);
                t += syl + r.range(0.0f, 0.06f);
            }
            if (laughter && r.chance(0.12f)) {  // kurzes Lachen: schnelle, hohe Silben
                for (int k = 0; k < r.irange(3, 6); ++k) {
                    VoiceSpec vs;
                    const float fh = f0 * 1.5f;
                    vs.f0 = [=](float u) { return fh * (1.0f - 0.2f * u); };
                    vs.amp = [](float u) { return std::sin(3.14159f * u); };
                    vs.formants = {700, 1200, 2600}, vs.breath = 0.6f;
                    voice(b, t, 0.1f, vs, r);
                    t += 0.15f;
                }
            }
            t += r.range(0.6f, 2.5f);
        }
    }
    filter(b, Biquad::LP, lowpass, 0.6f);
    filter(b, Biquad::LP, lowpass * 1.3f, 0.6f);
    wrapTail(b, L);
    normalize(b, lufs);
    return b.sound(true);
}

// ferne Wellen (Level 6): das Ziel, hoerbar durch lange Gaenge
Sound wavesFar(int rate) {
    Rand r(9300u);
    const float L = 33.0f;
    Buf b(L + 1.0f, kBedRate);
    Biquad lp = Biquad::make(Biquad::LP, 700.0f, 0.6f, (float)kBedRate);
    float t = 0.5f;
    while (t < L + 1.0f) {  // Wellen brechen in unregelmaessigem Abstand
        const float len = r.range(2.0f, 3.6f), a = r.range(0.6f, 1.0f);
        for (size_t i = 0; i < b.at(len) && b.at(t) + i < b.n(); ++i) {
            const float u = b.t(i) / len;
            b.x[b.at(t) + i] += lp.run(r.bi()) * a * std::pow(std::sin(3.14159f * u), 2.0f) * (u < 0.35f ? u / 0.35f : 1.0f);
        }
        t += r.range(4.0f, 7.5f);
    }
    loopify(b, 1.0f);
    dsp::RoomSpec room;
    room.t60Mid = 2.0f, room.t60Low = 2.4f, room.t60High = 0.6f, room.size = 2.0f;
    reverb(b, room, 0.5f, 0.6f, 9301u);
    wrapTail(b, L);
    normalize(b, level::kLoop);
    return b.sound(true);
}

Sound woodCreak(int rate, int v) {
    Rand r(9400u + (unsigned)v);
    Buf b(2.0f, rate);
    CreakSpec c;
    c.rate0 = r.range(14.0f, 24.0f), c.rate1 = r.range(30.0f, 60.0f), c.jitter = 0.35f;
    c.formants = {r.range(400.0f, 600.0f), r.range(1000.0f, 1400.0f), r.range(2300.0f, 2900.0f)}, c.q = 9.0f;
    creak(b, 0.0f, r.range(0.4f, 1.2f), c, 1.0f, r);
    trim(b);
    normalize(b, level::kEvent - 6.0f);
    return b.sound();
}

// V12 Level 5.3: herabfallende Buecher (Wiki: der Library Mind laesst Buecher fallen) - zwei bis vier dumpfe
// Schlaege kurz nacheinander, Holz und Papier, leicht entfernt
Sound bookFall(int rate, int v) {
    Rand r(9600u + (unsigned)v);
    Buf b(3.0f, rate);
    float t = 0.0f;
    const int n = 2 + v;
    for (int k = 0; k < n; ++k) {
        ImpactSpec sp;
        sp.modes = modesWood(r.range(140.0f, 260.0f), 0.12f, r);
        sp.contact = r.range(0.006f, 0.012f);
        sp.noise = 0.7f, sp.noiseHz = r.range(700.0f, 1400.0f), sp.noiseQ = 0.6f, sp.noiseT60 = 0.06f;
        b.add(impact(sp, k == 0 ? 1.0f : r.range(0.4f, 0.8f), 0.6f, rate, r), t);
        t += r.range(0.08f, 0.35f);
    }
    filter(b, Biquad::LP, 3500.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent - 4.0f);
    return b.sound();
}

// --- ferne Ereignisse -----------------------------------------------------------------------------------
Sound distant(int rate, int kind, int v) {
    Rand r(9500u + (unsigned)kind * 11u + (unsigned)v);
    Buf b(3.0f, rate);
    float lp = 1200.0f, t60 = 2.4f;
    switch (kind) {
        case 0: {  // schwere Tuer
            ImpactSpec sp;
            sp.modes = modesMetalPlate(r.range(80.0f, 110.0f), 0.6f, r);
            sp.contact = 0.004f, sp.noise = 0.5f, sp.noiseHz = 1200.0f;
            b.add(impact(sp, 1.0f, 1.5f, rate, r), 0.0f);
            lp = 900.0f, t60 = 2.8f;
            break;
        }
        case 1: {  // Metall schlaegt an
            ImpactSpec sp;
            sp.modes = modesMetalPipe(r.range(140.0f, 300.0f), 1.4f, r);
            sp.contact = 0.0015f;
            b.add(impact(sp, 1.0f, 2.0f, rate, r), 0.0f);
            lp = 2200.0f, t60 = 2.6f;
            break;
        }
        case 2: {  // Grollen
            dsp::Brown br;
            Biquad f = Biquad::make(Biquad::LP, 70.0f, 0.7f, (float)rate);
            for (size_t i = 0; i < b.at(2.8f); ++i) b.x[i] = f.run(br.run(r.bi())) * 3.0f * std::pow(std::sin(3.14159f * b.t(i) / 2.8f), 2.0f);
            lp = 400.0f, t60 = 2.0f;
            break;
        }
        case 3: {  // Klopfen an Holz
            ImpactSpec sp;
            sp.modes = modesWood(r.range(140.0f, 220.0f), 0.15f, r);
            sp.contact = 0.004f;
            float at = 0.0f;
            for (int k = 0; k < r.irange(2, 4); ++k) {
                b.add(impact(sp, 1.0f, 0.3f, rate, r), at);
                at += r.range(0.25f, 0.4f);
            }
            lp = 1100.0f, t60 = 2.2f;
            break;
        }
        case 4: {  // Dampf
            dsp::Svf f;
            const float len = r.range(1.0f, 1.8f);
            for (size_t i = 0; i < b.at(len); ++i) {
                f.set(dsp::lerpf(4500.0f, 2500.0f, b.t(i) / len), 1.2f, (float)rate);
                b.x[i] = f.bp(r.bi()) * envAHR(b.t(i), 0.03f, len * 0.3f, len * 0.6f);
            }
            lp = 2600.0f, t60 = 2.2f;
            break;
        }
        case 5: {  // Knarren im Bau
            CreakSpec c;
            c.rate0 = 8.0f, c.rate1 = 16.0f, c.formants = {150.0f, 420.0f, 1000.0f}, c.q = 12.0f;
            creak(b, 0.0f, r.range(1.0f, 2.0f), c, 1.0f, r);
            lp = 900.0f, t60 = 2.4f;
            break;
        }
        case 6: {  // Rolltor
            grains(b, 0.0f, 1.8f, 28.0f, [&](Buf& bb, float t, Rand& rr) {
                ImpactSpec sp;
                sp.modes = modesMetalPlate(rr.range(250.0f, 450.0f), 0.12f, rr);
                sp.contact = 0.0015f;
                bb.add(impact(sp, rr.range(0.3f, 0.8f), 0.2f, bb.rate, rr), t);
            }, r);
            lp = 1300.0f, t60 = 2.8f;
            break;
        }
        default: {  // Rollwagen ueber Beton
            Biquad f = Biquad::make(Biquad::LP, 300.0f, 0.7f, (float)rate);
            for (size_t i = 0; i < b.at(2.2f); ++i) b.x[i] = f.run(r.bi()) * envAHR(b.t(i), 0.3f, 1.4f, 0.5f) * 1.5f;
            grains(b, 0.2f, 2.0f, 14.0f, [&](Buf& bb, float t, Rand& rr) {
                ImpactSpec sp;
                sp.modes = modesMetalPlate(rr.range(400.0f, 900.0f), 0.08f, rr);
                sp.contact = 0.001f;
                bb.add(impact(sp, rr.range(0.1f, 0.4f), 0.1f, bb.rate, rr), t);
            }, r);
            lp = 1500.0f, t60 = 2.5f;
            break;
        }
    }
    distantize(b, lp, t60, 9600u + (unsigned)kind * 7u + (unsigned)v);
    normalize(b, level::kDistant);
    return b.sound();
}

}  // namespace

void addLevelSounds(std::vector<SoundDef>& d, int rate) {
    // Level 0 – Vorzimmer: Raumrauschen, entfernte Lueftung
    d.push_back({"bed_l0_room", [] { return bed(roomTone(29.0f, 520.0f, 45.0f, 9700u), 29.0f, level::kBed); }});
    d.push_back({"bed_l0_air", [] { return bed(rumble(23.0f, 180.0f, 9701u, 0.5f), 23.0f, level::kBed - 3.0f); }});
    // Level 1 – Lagerdeck: grosser Raum, Lueftung, ferne Rohre
    d.push_back({"bed_l1_space", [] { return bed(rumble(31.0f, 90.0f, 9710u, 0.6f), 31.0f, level::kBed); }});
    d.push_back({"bed_l1_vent", [] { return bed(roomTone(37.0f, 900.0f, 220.0f, 9711u, 0.25f), 37.0f, level::kBed - 4.0f); }});
    d.push_back({"bed_l1_pipes", [] { return bed(pipeResonance(41.0f, 9712u), 41.0f, level::kBed - 6.0f); }});
    // Level 2 – Rohrgaenge: Fliessen, warme Maschinen
    d.push_back({"bed_l2_flow", [] { return bed(pipeResonance(29.0f, 9720u), 29.0f, level::kBed + 1.0f); }});
    d.push_back({"bed_l2_warm", [] {
        Buf b = drone(37.0f, {49.0f, 98.0f, 147.0f, 196.0f}, 0.004f, 9721u);
        Buf rmb = rumble(37.0f, 140.0f, 9722u, 0.3f);
        loopify(rmb, (float)(rmb.n() - rmb.at(37.0f)) / (float)rmb.rate);
        Sound s = mix2(b, rmb, 2.0f);
        Buf t;
        t.x = s.samples, t.rate = s.rate;
        normalize(t, level::kBed - 2.0f);
        return t.sound(true);
    }});
    // Level 3 – Umspannwerk: Netzbrummen, Luefter
    d.push_back({"bed_l3_mains", [] {
        Buf b = drone(24.0f, {100.0f, 200.0f, 300.0f, 400.0f, 600.0f}, 0.0f, 9730u);
        normalize(b, level::kBed - 1.0f);
        return b.sound(true);
    }});
    d.push_back({"bed_l3_fans", [] { return bed(roomTone(31.0f, 1100.0f, 150.0f, 9731u, 0.2f), 31.0f, level::kBed - 3.0f); }});
    // Level 4 – Stilles Stockwerk: Raum, gedaempfter Regen
    d.push_back({"bed_l4_room", [] { return bed(rumble(29.0f, 70.0f, 9740u, 0.7f), 29.0f, level::kBed - 3.0f); }});
    d.push_back({"bed_l4_rain", [] { return bed(roomTone(37.0f, 900.0f, 120.0f, 9741u, 0.1f), 37.0f, level::kBed); }});
    // Level 5 – Hotel: fast still
    d.push_back({"bed_l5_room", [] { return bed(roomTone(31.0f, 300.0f, 40.0f, 9750u, 0.1f), 31.0f, level::kBed - 6.0f); }});
    d.push_back({"hotel_music", [=] { return hotelMusic(rate); }});
    d.push_back({"party_murmur", [=] { return murmur(rate, 9760u, 7, 23.0f, 700.0f, level::kLoop - 2.0f, true); }});
    // Level 6 – Dunkel: beinahe schalltot; das Ziel hoert man
    d.push_back({"bed_l6", [] { return bed(roomTone(37.0f, 180.0f, 30.0f, 9770u, 0.05f), 37.0f, level::kBed - 10.0f); }});
    d.push_back({"waves_far", [=] { return wavesFar(rate); }});
    // Level 7 – Meer unter der Decke: weite Duenung
    d.push_back({"bed_l7", [] { return bed(rumble(41.0f, 200.0f, 9780u, 0.7f), 41.0f, level::kBed); }});
    // Level 19 – Dachboden: Knistern des Gluehens, Wind auf dem Dach
    d.push_back({"bed_l19_glow", [] { return bed(glowCrackle(29.0f, 9790u), 29.0f, level::kBed + 1.0f); }});
    d.push_back({"bed_l19_wind", [] { return bed(windRoof(37.0f, 9791u), 37.0f, level::kBed - 4.0f); }});
    for (int v = 0; v < 4; ++v) d.push_back({std::format("wood_creak_{}", v), [=] { return woodCreak(rate, v); }});
    // Level 5.3 – Bibliothek: sehr still, weiter Raum; Buecher fallen
    d.push_back({"bed_l53_room", [] { return bed(roomTone(31.0f, 260.0f, 35.0f, 9830u, 0.08f), 31.0f, level::kBed - 7.0f); }});
    for (int v = 0; v < 3; ++v) d.push_back({std::format("book_fall_{}", v), [=] { return bookFall(rate, v); }});
    // Level 37 – Becken: ruhige Luft (das Wasser kommt aus pool_loop)
    d.push_back({"bed_l37_air", [] { return bed(roomTone(31.0f, 1200.0f, 80.0f, 9800u, 0.1f), 31.0f, level::kBed - 6.0f); }});
    // Stimmen in den Zonen von Level 3 (Halluzination)
    d.push_back({"hal_voices", [=] { return murmur(rate, 9810u, 3, 19.0f, 1100.0f, level::kLoop - 6.0f, false); }});
    // ferne Ereignisse
    const char* dn[8] = {"dist_door", "dist_clank", "dist_rumble", "dist_knock", "dist_steam", "dist_creak", "dist_rolltor", "dist_cart"};
    for (int k = 0; k < 8; ++k)
        for (int v = 0; v < 3; ++v) d.push_back({std::format("{}_{}", dn[k], v), [=] { return distant(rate, k, v); }});
}

}  // namespace lim::audio
