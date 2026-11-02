// V13.3 Level 3 (KREATUR_Wretch.md, KREATUR_SkinStealer.md): Stimmen und Waffenklaenge. Alles synthetisch, keine Aufnahmen.
//
// Sprache: ein kleiner Formant-Synthesizer nach Klatt (Kaskade aus vier Resonatoren, Stimmlippenimpuls nach Rosenberg,
// Hauch, Reibe- und Verschlusslaute, Nasale) liest englische Laute (ARPAbet) mit Betonung. Die Formanten springen je Laut
// und werden ueber 35 ms geglaettet (Koartikulation); Verschlusslaute tragen Locus-Formanten (Lippen 900, Zahndamm 1700,
// Gaumen 2200 Hz), damit die Uebergaenge der Vokale stimmen.
//   skin_say_0..7, skin_ans_0..3, skin_wait, skin_bye  der Skin-Stealer im Mantel: einfache, leicht falsche Saetze
//       (A2-Englisch), vorgetaeuschter Akzent (gerolltes R, V statt W, Z/D statt TH, Auslaut stimmlos, Betonung auf der
//       falschen Silbe), zu gleichmaessig, ohne Atem; ein grosser Vokaltrakt (2,05 m); darunter, kaum hoerbar, eine zweite
//       Kehle eine Oktave tiefer; am Satzende ein feuchtes Schnalzen
//   skin_tear       die Verwandlung: Gelenke knacken, Haut dehnt sich nass - bei 1,45 s reissen Mantel und Haut, Knochen
//       springen, es tropft
//   wretch_frag_0..7 Sprachfetzen eines Wretch in Level 3: hohe, gepresste, rasselnde Stimme (die Seite: hohe Laute = klug
//       oder schnell), ohne Lippen (Stadium 3) - p, b, m, w, f werden zu einem feuchten Schnalzen, Woerter brechen ab,
//       Schlamm gurgelt (Blasen); 0-3 noch verstaendlich (er lockt: help, water, please, go home), 4-6 verstuemmelt
//       (hungry, it itches, don't look), 7 Kauderwelsch
//   wretch_cry_0..2  Schrei beim Ausholen ("NO", "DIE", ohne Wort)
//   wretch_drag/raise/miss/hit_<pipe|board|blade>  die Waffen: Rohr schleift ueber Beton und klingt nach, Brett klopft und
//       die Naegel kratzen, Blech flattert und das Kabel klappert; Ausholen, Fehlschlag (Aufschlag), Treffer
#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include "audio/synth_defs.hpp"
#include "audio/synth_kit.hpp"

namespace lim::audio {

using namespace kit;

namespace {
// --- Laute -------------------------------------------------------------------------------------------------------------
// Klasse: v Vokal, d Zwielaut, l Gleit-/Fliesslaut, n Nasal, f stimmloser Reibelaut, z stimmhafter Reibelaut, p stimmloser
// Verschluss, b stimmhafter Verschluss, h Hauch
struct PhDef {
    const char* name;
    char cls;
    float f1, f2, f3;     // Formanten (Ende bei Zwielauten: e1..e3)
    float e1, e2, e3;
    float len;            // Grunddauer (s)
    float ff, fq, famp;   // Reibung: Mitte, Guete, Staerke
    float bhz;            // Verschluss: Mitte des Knalls
};
const PhDef kPh[] = {
    {"IY", 'v', 280, 2250, 2890, 0, 0, 0, 0.105f, 0, 0, 0, 0},     {"IH", 'v', 400, 1920, 2560, 0, 0, 0, 0.075f, 0, 0, 0, 0},
    {"EH", 'v', 550, 1770, 2490, 0, 0, 0, 0.085f, 0, 0, 0, 0},     {"AE", 'v', 690, 1660, 2490, 0, 0, 0, 0.11f, 0, 0, 0, 0},
    {"AA", 'v', 720, 1100, 2540, 0, 0, 0, 0.115f, 0, 0, 0, 0},     {"AO", 'v', 590, 880, 2540, 0, 0, 0, 0.115f, 0, 0, 0, 0},
    {"UH", 'v', 450, 1030, 2380, 0, 0, 0, 0.075f, 0, 0, 0, 0},     {"UW", 'v', 310, 870, 2250, 0, 0, 0, 0.105f, 0, 0, 0, 0},
    {"AH", 'v', 620, 1200, 2390, 0, 0, 0, 0.08f, 0, 0, 0, 0},      {"AX", 'v', 500, 1450, 2500, 0, 0, 0, 0.05f, 0, 0, 0, 0},
    {"ER", 'v', 470, 1300, 1650, 0, 0, 0, 0.11f, 0, 0, 0, 0},
    {"EY", 'd', 480, 1950, 2550, 320, 2250, 2850, 0.14f, 0, 0, 0, 0}, {"AY", 'd', 710, 1180, 2500, 360, 2100, 2700, 0.15f, 0, 0, 0, 0},
    {"OW", 'd', 560, 920, 2400, 360, 780, 2300, 0.14f, 0, 0, 0, 0},  {"AW", 'd', 710, 1180, 2500, 420, 880, 2300, 0.15f, 0, 0, 0, 0},
    {"OY", 'd', 560, 880, 2420, 360, 2000, 2650, 0.16f, 0, 0, 0, 0},
    {"L", 'l', 360, 1000, 2750, 0, 0, 0, 0.06f, 0, 0, 0, 0},       {"R", 'l', 420, 1250, 1550, 0, 0, 0, 0.06f, 0, 0, 0, 0},
    {"W", 'l', 300, 650, 2200, 0, 0, 0, 0.05f, 0, 0, 0, 0},        {"Y", 'l', 270, 2100, 2950, 0, 0, 0, 0.05f, 0, 0, 0, 0},
    {"M", 'n', 270, 950, 2200, 0, 0, 0, 0.07f, 0, 0, 0, 0},        {"N", 'n', 270, 1550, 2500, 0, 0, 0, 0.06f, 0, 0, 0, 0},
    {"NG", 'n', 270, 2050, 2700, 0, 0, 0, 0.07f, 0, 0, 0, 0},
    {"S", 'f', 300, 1700, 2600, 0, 0, 0, 0.10f, 5600, 1.6f, 0.55f, 0}, {"Z", 'z', 300, 1700, 2600, 0, 0, 0, 0.08f, 5200, 1.6f, 0.35f, 0},
    {"SH", 'f', 300, 1900, 2500, 0, 0, 0, 0.10f, 2800, 1.4f, 0.55f, 0}, {"ZH", 'z', 300, 1900, 2500, 0, 0, 0, 0.08f, 2700, 1.4f, 0.35f, 0},
    {"F", 'f', 300, 900, 2200, 0, 0, 0, 0.09f, 4000, 0.45f, 0.14f, 0}, {"V", 'z', 300, 900, 2200, 0, 0, 0, 0.06f, 3500, 0.5f, 0.1f, 0},
    {"TH", 'f', 300, 1400, 2500, 0, 0, 0, 0.09f, 4800, 0.5f, 0.12f, 0}, {"DH", 'z', 300, 1400, 2500, 0, 0, 0, 0.05f, 4200, 0.5f, 0.09f, 0},
    {"HH", 'h', 500, 1500, 2500, 0, 0, 0, 0.06f, 0, 0, 0, 0},
    {"P", 'p', 300, 900, 2200, 0, 0, 0, 0.075f, 0, 0, 0, 900},     {"B", 'b', 300, 900, 2200, 0, 0, 0, 0.06f, 0, 0, 0, 900},
    {"T", 'p', 300, 1700, 2600, 0, 0, 0, 0.07f, 0, 0, 0, 4200},    {"D", 'b', 300, 1700, 2600, 0, 0, 0, 0.055f, 0, 0, 0, 3600},
    {"K", 'p', 300, 2200, 2700, 0, 0, 0, 0.08f, 0, 0, 0, 2300},    {"G", 'b', 300, 2200, 2700, 0, 0, 0, 0.06f, 0, 0, 0, 2100},
};
constexpr int kPhN = (int)(sizeof(kPh) / sizeof(kPh[0]));
int phIndex(const std::string& n) {
    for (int i = 0; i < kPhN; ++i)
        if (n == kPh[i].name) return i;
    return -1;
}
bool labial(const std::string& n) { return n == "P" || n == "B" || n == "M" || n == "F" || n == "V" || n == "W"; }

// Satz: Laute mit Betonung und Satzzeichen ("," Pause, "." fallend, "?" steigend, "!" Nachdruck, "#" Abbruch mit Luecke)
struct Seg {
    int ph = -1;          // -1: Pause
    float len = 0.1f;
    float stress = 0.0f;  // 0..1
    bool trill = false, smack = false, cut = false;
    float f0mul = 1.0f, amp = 1.0f;
};
struct Phrase {
    std::vector<Seg> s;
    char end = '.';
};

// Sprecher
struct Speaker {
    float f0 = 110.0f, f0End = 0.85f, stressUp = 0.18f, fmul = 1.0f;
    float jitter = 0.006f, shimmer = 0.04f, breath = 0.03f, rough = 0.0f, roughHz = 30.0f, subharm = 0.0f, tilt = 0.6f;
    float tempo = 1.0f, isochrony = 0.0f;  // 1: alle Silben gleich lang (zu gleichmaessig)
    bool accent = false, lipless = false, gurgle = false;
    float smooth = 0.035f;  // Glaettung der Formanten (s)
};

Phrase parse(const char* txt, const Speaker& sp) {
    Phrase out;
    std::string tok;
    std::vector<std::string> toks;
    for (const char* c = txt;; ++c) {
        if (*c == ' ' || *c == 0) {
            if (!tok.empty()) toks.push_back(tok), tok.clear();
            if (*c == 0) break;
        } else {
            tok += *c;
        }
    }
    for (size_t i = 0; i < toks.size(); ++i) {
        const std::string& t = toks[i];
        Seg g;
        if (t == "," || t == "|" || t == "#" || t == "." || t == "?" || t == "!") {
            g.ph = -1;
            g.len = t == "," ? 0.16f : t == "|" ? 0.06f : t == "#" ? 0.3f : 0.22f;
            g.cut = t == "#";
            if (t == "." || t == "?" || t == "!") out.end = t[0];
            // Dehnung des letzten Vokals vor der Pause, Melodie am Phrasenende
            for (size_t k = out.s.size(); k-- > 0;) {
                Seg& q = out.s[k];
                if (q.ph < 0) break;
                const char cl = kPh[q.ph].cls;
                if (cl == 'v' || cl == 'd') {
                    q.len *= t == "#" ? 1.0f : 1.35f;
                    if (t == "?") q.f0mul = 1.35f;
                    if (t == ".") q.f0mul = 0.86f;
                    if (t == "!") q.f0mul = 1.25f, q.amp = 1.25f;
                    if (t == "#") q.cut = true;  // bricht ab
                    break;
                }
            }
            out.s.push_back(g);
            continue;
        }
        std::string n = t;
        float stress = 0.0f;
        if (!n.empty() && n.back() >= '0' && n.back() <= '2') stress = n.back() == '1' ? 1.0f : (n.back() == '2' ? 0.5f : 0.0f), n.pop_back();
        if (n == "CH" || n == "JH") {  // Affrikaten
            Seg a, b;
            a.ph = phIndex(n == "CH" ? "T" : "D"), a.len = 0.05f;
            b.ph = phIndex(n == "CH" ? "SH" : "ZH"), b.len = 0.07f;
            out.s.push_back(a), out.s.push_back(b);
            continue;
        }
        g.ph = phIndex(n);
        if (g.ph < 0) continue;
        const PhDef& P = kPh[g.ph];
        g.stress = stress;
        g.len = P.len * ((P.cls == 'v' || P.cls == 'd') ? (stress > 0.9f ? 1.3f : (stress > 0.4f ? 1.0f : 0.75f)) : 1.0f);
        if (sp.accent && n == "R") g.trill = true, g.len *= 1.5f;  // gerolltes R
        if (sp.lipless && labial(n)) {  // ohne Lippen: ein feuchtes Schnalzen statt des Lauts
            g.smack = true;
            g.ph = n == "M" ? phIndex("N") : (n == "W" ? phIndex("UH") : (n == "F" || n == "V" ? phIndex("TH") : phIndex("HH")));
            g.len = n == "W" ? 0.05f : 0.06f;
            g.amp = 0.6f;
        }
        out.s.push_back(g);
    }
    // gleichmaessig: Vokaldauern zum Mittel ziehen
    if (sp.isochrony > 0.0f) {
        float sum = 0.0f;
        int n = 0;
        for (const Seg& g : out.s)
            if (g.ph >= 0 && (kPh[g.ph].cls == 'v' || kPh[g.ph].cls == 'd')) sum += g.len, ++n;
        const float mean = n ? sum / (float)n : 0.1f;
        for (Seg& g : out.s)
            if (g.ph >= 0 && (kPh[g.ph].cls == 'v' || kPh[g.ph].cls == 'd')) g.len += (mean - g.len) * sp.isochrony;
    }
    for (Seg& g : out.s) g.len /= sp.tempo;
    return out;
}

// --- Parameter je 2 ms und Synthese ----------------------------------------------------------------------------------------
struct Track {
    std::vector<float> f0, av, ah, af, ff, fq, nas, F1, F2, F3;
    void resize(size_t n) {
        for (auto* v : {&f0, &av, &ah, &af, &ff, &fq, &nas, &F1, &F2, &F3}) v->assign(n, 0.0f);
    }
};
constexpr float kFrame = 0.002f;

void smoothTrack(std::vector<float>& v, int win) {
    if (win <= 1 || v.empty()) return;
    std::vector<float> o(v.size());
    double acc = 0.0;
    int cnt = 0;
    const int h = win / 2;
    for (int i = -h; i < (int)v.size() + h; ++i) {
        const int add = i + h, rem = i - h - 1;
        if (add >= 0 && add < (int)v.size()) acc += v[(size_t)add], ++cnt;
        if (rem >= 0 && rem < (int)v.size()) acc -= v[(size_t)rem], --cnt;
        if (i >= 0 && i < (int)v.size()) o[(size_t)i] = (float)(acc / std::max(1, cnt));
    }
    v.swap(o);
}

struct Resonator {
    float a = 1, b = 0, c = 0, y1 = 0, y2 = 0;
    void set(float hz, float bw, float rate) {
        const float T = 1.0f / rate;
        c = -std::exp(-2.0f * kPi * bw * T);
        b = 2.0f * std::exp(-kPi * bw * T) * std::cos(2.0f * kPi * std::min(hz, rate * 0.45f) * T);
        a = 1.0f - b - c;
    }
    float run(float x) {
        const float y = a * x + b * y1 + c * y2;
        y2 = y1, y1 = y;
        return y;
    }
};
struct AntiResonator {  // Nullstelle (Nasal)
    float a = 1, b = 0, c = 0, x1 = 0, x2 = 0;
    void set(float hz, float bw, float rate) {
        Resonator r;
        r.set(hz, bw, rate);
        a = 1.0f / r.a, b = -r.b / r.a, c = -r.c / r.a;
    }
    float run(float x) {
        const float y = a * x + b * x1 + c * x2;
        x2 = x1, x1 = x;
        return y;
    }
};

// spricht den Satz in b ab at; Rueckgabe: Ende (s)
float speak(Buf& b, float at, const Phrase& ph, const Speaker& sp, Rand& r, float gain = 1.0f, float f0shift = 1.0f) {
    float total = 0.0f;
    for (const Seg& g : ph.s) total += g.len;
    const size_t nF = (size_t)(total / kFrame) + 4;
    Track tr;
    tr.resize(nF);
    struct Burst {
        float t, hz, amp;
        bool smack;
    };
    std::vector<Burst> bursts;
    std::vector<std::pair<float, float>> cuts;  // Abbrueche: Zeit, Dauer der Luecke
    // Stufen je Laut
    float t = 0.0f;
    std::vector<float> stressF(nF, 0.0f), mulF(nF, 1.0f);
    for (size_t si = 0; si < ph.s.size(); ++si) {
        const Seg& g = ph.s[si];
        const size_t i0 = (size_t)(t / kFrame), i1 = std::min(nF, (size_t)((t + g.len) / kFrame) + 1);
        if (g.ph < 0) {
            for (size_t i = i0; i < i1; ++i) {
                // Pause: Formanten halten (fuer die Glaettung), keine Quelle
                if (i > 0) tr.F1[i] = tr.F1[i - 1], tr.F2[i] = tr.F2[i - 1], tr.F3[i] = tr.F3[i - 1];
                else tr.F1[i] = 500, tr.F2[i] = 1500, tr.F3[i] = 2500;
            }
            if (g.cut) cuts.push_back({t, g.len});
            t += g.len;
            continue;
        }
        const PhDef& P = kPh[g.ph];
        const bool vow = P.cls == 'v' || P.cls == 'd';
        // Hauch: die Formanten des folgenden Vokals
        const PhDef* nxt = nullptr;
        for (size_t k = si + 1; k < ph.s.size() && !nxt; ++k)
            if (ph.s[k].ph >= 0 && (kPh[ph.s[k].ph].cls == 'v' || kPh[ph.s[k].ph].cls == 'd')) nxt = &kPh[ph.s[k].ph];
        for (size_t i = i0; i < i1; ++i) {
            const float u = i1 > i0 + 1 ? (float)(i - i0) / (float)(i1 - i0 - 1) : 0.0f;
            float f1 = P.f1, f2 = P.f2, f3 = P.f3;
            if (P.cls == 'd') {
                const float w = std::clamp((u - 0.25f) / 0.6f, 0.0f, 1.0f);
                f1 += (P.e1 - P.f1) * w, f2 += (P.e2 - P.f2) * w, f3 += (P.e3 - P.f3) * w;
            }
            if (P.cls == 'h' && nxt) f1 = nxt->f1, f2 = nxt->f2, f3 = nxt->f3;
            tr.F1[i] = f1 * sp.fmul, tr.F2[i] = f2 * sp.fmul, tr.F3[i] = f3 * sp.fmul;
            float av = 0, ah = 0, af = 0;
            switch (P.cls) {
                case 'v':
                case 'd': av = 1.0f; break;
                case 'l': av = 0.62f; break;
                case 'n': av = 0.42f, tr.nas[i] = 1.0f; break;
                case 'f': af = P.famp, ah = 0.02f; break;
                case 'z': af = P.famp, av = 0.35f; break;
                case 'h': ah = 0.32f; break;
                case 'p': av = 0.0f, ah = u > 0.72f ? 0.28f : 0.0f; break;  // Verschluss, dann Behauchung
                case 'b': av = u < 0.7f ? 0.12f : 0.3f; break;              // Stimmbalken
                default: break;
            }
            if (g.trill) av *= std::sin(2.0f * kPi * 26.0f * (float)(i - i0) * kFrame) > -0.2f ? 1.0f : 0.15f;
            tr.av[i] = av * g.amp, tr.ah[i] = ah * g.amp, tr.af[i] = af * g.amp;
            tr.ff[i] = P.ff, tr.fq[i] = P.fq > 0 ? P.fq : 1.0f;
            stressF[i] = vow ? g.stress : 0.0f;
            mulF[i] = g.f0mul;
        }
        if (P.cls == 'p' || P.cls == 'b') bursts.push_back({t + g.len * 0.72f, P.bhz * sp.fmul, P.cls == 'p' ? 0.5f : 0.3f, false});
        if (g.smack) bursts.push_back({t + g.len * 0.4f, 1300.0f, 0.55f, true});
        t += g.len;
    }
    const int win = std::max(1, (int)(sp.smooth / kFrame));
    smoothTrack(tr.F1, win), smoothTrack(tr.F2, win), smoothTrack(tr.F3, win);
    smoothTrack(tr.av, 4), smoothTrack(tr.ah, 3), smoothTrack(tr.af, 4), smoothTrack(tr.nas, 6);
    smoothTrack(stressF, 40), smoothTrack(mulF, 50);
    for (size_t i = 0; i < nF; ++i) {  // Melodie: Absinken ueber den Satz, Buckel auf betonten Silben, Phrasenende
        const float rel = (float)i / (float)nF;
        tr.f0[i] = sp.f0 * f0shift * (1.0f + (sp.f0End - 1.0f) * rel) * (1.0f + sp.stressUp * stressF[i]) * mulF[i];
    }
    // Abbrueche: ab dem Schnitt still (hart)
    for (auto [ct, cl] : cuts) {
        const size_t i0 = (size_t)(ct / kFrame), i1 = std::min(nF, (size_t)((ct + cl) / kFrame));
        for (size_t i = i0; i < i1; ++i) tr.av[i] = tr.ah[i] = tr.af[i] = 0.0f;
    }
    // Abtasten
    const int rate = b.rate;
    const size_t a0 = b.at(at), n = (size_t)(total * (float)rate) + (size_t)(0.05f * rate);
    if (a0 + n > b.n()) b.x.resize(a0 + n, 0.0f);
    Resonator R[4], RN, R5;
    AntiResonator AZ;
    // hoehere Formanten parallel (die Kaskade faellt oberhalb von F4 steil ab - sonst klingt es wie durch ein Rohr)
    Biquad hi4 = Biquad::make(Biquad::BP, 3600.0f * sp.fmul, 2.5f, (float)rate), hi5 = Biquad::make(Biquad::BP, 4700.0f * sp.fmul, 2.5f, (float)rate);
    R5.set(3800.0f * sp.fmul, 350.0f, (float)rate);
    RN.set(270.0f, 110.0f, (float)rate);
    AZ.set(560.0f, 120.0f, (float)rate);
    Biquad fric;
    float phase = 0.0f, uPrev = 0.0f, perAmp = 1.0f, perF = 1.0f, tiltY = 0.0f, curFF = -1, curFQ = -1;
    int period = 0;
    const float bw[4] = {90.0f, 110.0f, 170.0f, 250.0f};
    for (size_t i = 0; i < n; ++i) {
        const float ts = (float)i / (float)rate;
        const float fi = ts / kFrame;
        const size_t k0 = std::min(nF - 1, (size_t)fi), k1 = std::min(nF - 1, k0 + 1);
        const float w = std::clamp(fi - (float)k0, 0.0f, 1.0f);
        auto L = [&](const std::vector<float>& v) { return v[k0] + (v[k1] - v[k0]) * w; };
        if ((i & 7) == 0) {
            R[0].set(L(tr.F1), bw[0] * (1.0f + 0.3f * L(tr.nas)), (float)rate);
            R[1].set(L(tr.F2), bw[1], (float)rate);
            R[2].set(L(tr.F3), bw[2], (float)rate);
            R[3].set(3300.0f * sp.fmul, bw[3], (float)rate);
            const float ff = L(tr.ff), fq = L(tr.fq);
            if (ff > 100.0f && (std::fabs(ff - curFF) > 40.0f || std::fabs(fq - curFQ) > 0.05f)) {
                fric.set(Biquad::BP, ff * sp.fmul, fq, (float)rate);
                curFF = ff, curFQ = fq;
            }
        }
        // Stimmlippen: Rosenberg-Puls, Jitter und Shimmer je Periode, Unterharmonische (jede zweite Periode leiser)
        const float f0 = std::max(40.0f, L(tr.f0)) * perF;
        phase += f0 / (float)rate;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            ++period;
            perF = 1.0f + sp.jitter * r.bi();
            perAmp = (1.0f + sp.shimmer * r.bi()) * ((period & 1) ? 1.0f - sp.subharm : 1.0f);
        }
        float u;
        const float oa = 0.42f, ob = 0.16f;
        if (phase < oa) u = 0.5f * (1.0f - std::cos(kPi * phase / oa));
        else if (phase < oa + ob) u = std::cos(0.5f * kPi * (phase - oa) / ob);
        else u = 0.0f;
        float src = (u - uPrev) * (float)rate / (f0 * 3.0f);
        uPrev = u;
        tiltY += (src - tiltY) * (1.0f - sp.tilt);  // Neigung: gepresst (wenig) bis behaucht (viel)
        src = tiltY;
        const float rough = sp.rough > 0.0f ? 1.0f - sp.rough * (0.5f + 0.5f * std::sin(2.0f * kPi * sp.roughHz * ts + 0.7f * std::sin(ts * 9.0f))) : 1.0f;
        const float noise = r.bi();
        const float av = L(tr.av), ah = L(tr.ah) + sp.breath * av, af = L(tr.af);
        float x = src * av * perAmp * rough + noise * ah * (0.35f + 0.65f * u) * 0.5f;
        const float xs = x;
        for (Resonator& q : R) x = q.run(x);
        x = R5.run(x) * 0.6f + x * 0.4f + (hi4.run(xs) * 0.16f + hi5.run(xs) * 0.09f) * (0.3f + 0.7f * av);
        const float nas = L(tr.nas);
        if (nas > 0.01f) x = x * (1.0f - nas) + RN.run(AZ.run(x)) * nas;
        float fr = af > 0.001f ? fric.run(noise) * af * (av > 0.1f ? 0.5f + 0.5f * u : 1.0f) : 0.0f;
        b.x[a0 + i] += (x * 0.55f + fr * 0.9f) * gain;
    }
    // Knaelle und Schnalzer
    for (const Burst& q : bursts) {
        Biquad bp = Biquad::make(Biquad::BP, q.hz, q.smack ? 3.0f : 1.2f, (float)rate);
        const size_t s0 = b.at(at + q.t), len = (size_t)((q.smack ? 0.012f : 0.008f) * rate);
        for (size_t j = 0; j < len && s0 + j < b.n(); ++j) {
            const float e = std::exp(-(float)j / (float)len * 4.0f);
            b.x[s0 + j] += bp.run(r.bi()) * q.amp * e * gain * (q.smack ? 1.6f : 1.0f);
        }
        if (q.smack) {  // feucht: eine kleine Blase dahinter
            Buf bb(0.06f, rate);
            bubble(bb, 0.0f, r.range(1.2f, 2.2f), 0.25f * gain, r);
            b.add(bb, at + q.t + 0.004f);
        }
    }
    return at + total;
}

// --- Saetze -------------------------------------------------------------------------------------------------------------
// Skin-Stealer: Laute mit vorgetaeuschtem Akzent (V statt W, Z/D statt TH, Auslaut stimmlos, Betonung verschoben)
const char* const kSkinSay[8] = {
    "HH EH0 L OW1 . Y UW1 AA1 R AX0 L OW1 N , Y EH1 S ?",
    "AY1 AE1 M L AO1 S T AO1 L S OW1 . V IY1 G OW1 T UW0 G EH1 Z ER1 .",
    "IY1 S S EY1 F HH IY1 R . Y UW1 S T EY1 V IH1 Z M IY1 .",
    "V EH1 R Y UW1 AA1 R G OW0 IY1 NG ? AY1 K AH1 M V IH1 Z Y UW1 .",
    "M AY1 N EY1 M IY1 S D AE0 N IY0 EH1 L . IY1 S M AY1 N EY1 M .",
    "Y UW1 HH AE1 F V AA1 T ER0 ? AY1 AE1 M V EH1 R IY0 S ER1 S T .",
    "D UW1 N AA1 T B IY1 AX0 F R EY1 T . AY1 AE1 M P IY1 P OW1 L . HH Y UW0 M AE1 N .",
    "Y UW1 AA1 R T AY1 ER0 T . S IY1 T . R EH1 S T HH IY1 R .",
};
const char* const kSkinAns[4] = {
    "Y EH1 S . HH EH0 L OW1 . AY1 AE1 M F R EH1 N T .",
    "AY1 AE1 M HH Y UW0 M AE1 N . L AY1 K Y UW1 .",
    "AY1 K AH1 M F R AO1 M K AE1 M P . IY1 S F AA1 R .",  // ohne Ortsnamen des Wikis (LICENSE_REVIEW)
    "IY1 S OW0 K EY1 . EH1 V R IY0 S IY1 NG IY1 S OW0 K EY1 .",
};
const char* const kSkinWait = "V EY1 T ! V EH1 R Y UW1 G OW1 ?";
const char* const kSkinBye = "AY1 G OW1 N AW1 . G UH0 D B AY1 , F R EH1 N T .";

Speaker skinSpeaker() {
    Speaker s;
    s.f0 = 104.0f, s.f0End = 0.9f, s.stressUp = 0.22f, s.fmul = 0.93f;  // tief, grosser Vokaltrakt
    s.jitter = 0.002f, s.shimmer = 0.012f, s.breath = 0.0f, s.tilt = 0.22f;  // zu glatt, kein Atem
    s.tempo = 0.92f, s.isochrony = 0.6f, s.accent = true, s.smooth = 0.03f;
    return s;
}

Sound skinLine(int rate, const char* txt, unsigned seed, bool loud = false) {
    Rand r(seed);
    Buf b(4.5f, rate);
    Speaker sp = skinSpeaker();
    if (loud) sp.f0 *= 1.2f, sp.stressUp = 0.3f, sp.tempo = 1.1f;
    const Phrase p = parse(txt, sp);
    const float end = speak(b, 0.04f, p, sp, r);
    // die zweite Kehle: eine Oktave tiefer, 25 ms spaeter, kaum hoerbar
    Speaker s2 = sp;
    s2.fmul = 0.86f, s2.breath = 0.02f, s2.subharm = 0.2f;
    speak(b, 0.065f, p, s2, r, 0.11f, 0.5f);
    // am Satzende ein feuchtes Schnalzen (kein menschlicher Mund)
    {
        ImpactSpec sk;
        sk.modes = modesWood(r.range(1100.0f, 1500.0f), 0.015f, r);
        sk.contact = 0.0006f, sk.noise = 0.8f, sk.noiseHz = 1800.0f;
        b.add(impact(sk, 0.18f, 0.06f, rate, r), end + 0.16f);
    }
    filter(b, Biquad::HP, 75.0f, 0.7f);
    filter(b, Biquad::HighShelf, 2200.0f, 0.7f, 7.0f);  // Praesenz (die Kaskade faellt oben steil ab)
    filter(b, Biquad::LP, 7500.0f, 0.7f);
    trim(b);
    normalize(b, level::kCreature - (loud ? 0.0f : 2.0f));
    return b.sound();
}

// Wretch: hohe, gepresste, rasselnde Stimme ohne Lippen; Fetzen brechen ab, Schlamm gurgelt
const char* const kWretchFrag[7] = {
    "HH EH1 L P # HH EH1 EH1 L P .",
    "W AO1 T ER0 # W AO1 AO1 T ER0 .",
    "P L IY1 Z # # P L IY1 IY1 Z .",
    "G OW1 # # HH OW1 OW1 M .",
    "HH AH1 NG # G R IY1 .",
    "IH1 T # IH1 CH AX0 Z .",
    "D OW1 N T # L UH1 K .",
};

Speaker wretchSpeaker(Rand& r, bool clear) {
    Speaker s;
    s.f0 = r.range(185.0f, 225.0f), s.f0End = 0.78f, s.stressUp = 0.25f, s.fmul = 1.03f;
    s.jitter = clear ? 0.03f : 0.05f, s.shimmer = 0.25f, s.breath = clear ? 0.12f : 0.2f;
    s.rough = clear ? 0.35f : 0.55f, s.roughHz = r.range(26.0f, 34.0f), s.subharm = clear ? 0.25f : 0.4f, s.tilt = 0.35f;
    s.tempo = 0.62f, s.lipless = true, s.gurgle = true, s.smooth = 0.045f;
    return s;
}

void gurgle(Buf& b, float t0, float t1, float density, Rand& r) {
    grains(b, t0, t1, density, [](Buf& g, float at, Rand& rr) { bubble(g, at, rr.range(1.5f, 4.5f), rr.range(0.12f, 0.35f), rr); }, r);
    // nasses, tiefes Brodeln darunter
    Biquad lp = Biquad::make(Biquad::BP, 260.0f, 1.2f, (float)b.rate);
    const size_t a = b.at(t0), n = b.at(t1 - t0);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / (float)b.rate;
        b.x[a + i] += lp.run(r.bi()) * 0.04f * (0.5f + 0.5f * std::sin(t * 17.0f + std::sin(t * 5.0f))) * envAHR(t, 0.08f, (t1 - t0) - 0.2f, 0.12f);
    }
}

Sound wretchFrag(int rate, int k) {
    Rand r(15300u + (unsigned)k * 31u);
    Buf b(4.0f, rate);
    const bool clear = k < 4;
    Speaker sp = wretchSpeaker(r, clear);
    float end;
    if (k < 7) {
        const Phrase p = parse(kWretchFrag[k], sp);
        end = speak(b, 0.25f, p, sp, r);
    } else {  // Kauderwelsch: Silben ohne Sinn, Tonspruenge
        static const char* const syl[] = {"G AA1", "N UH1", "HH AY1", "K EH1", "D IY1", "L AA1 G", "R OW1", "N EY1 T", "S AA1", "T UH1 K", "IH1 NG"};
        std::string txt;
        for (int i = 0; i < 6; ++i) {
            txt += syl[r.irange(0, 10)];
            txt += i == 2 ? " # " : " | ";
        }
        txt += " .";
        sp.f0 *= 1.08f;
        const Phrase p = parse(txt.c_str(), sp);
        end = speak(b, 0.2f, p, sp, r);
    }
    // vorher und nachher rasselndes Einatmen (Schlamm in der Kehle), dabei gurgelt es
    breathe(b, 0.0f, 0.24f, true, 0.35f, 0.95f, r);
    breathe(b, end + 0.05f, 0.42f, true, 0.4f, 0.95f, r);
    gurgle(b, 0.1f, end + 0.4f, clear ? 9.0f : 16.0f, r);
    filter(b, Biquad::HP, 120.0f, 0.7f);
    filter(b, Biquad::HighShelf, 2400.0f, 0.7f, 6.0f);
    trim(b);
    normalize(b, level::kCreature - 1.0f);
    return b.sound();
}

Sound wretchCry(int rate, int k) {
    Rand r(15400u + (unsigned)k * 17u);
    Buf b(2.0f, rate);
    Speaker sp = wretchSpeaker(r, false);
    sp.f0 = 320.0f, sp.f0End = 1.3f, sp.rough = 0.6f, sp.subharm = 0.45f, sp.breath = 0.35f, sp.tilt = 0.0f, sp.tempo = 1.0f, sp.fmul = 1.12f;
    if (k == 0) speak(b, 0.02f, parse("N AA1 AA1 OW1 OW1 !", sp), sp, r);
    else if (k == 1) speak(b, 0.02f, parse("D AY1 AY1 AY1 !", sp), sp, r);
    else {  // ohne Wort: ein gepresster, sich ueberschlagender Schrei
        sp.f0 = 340.0f, sp.f0End = 1.4f;
        speak(b, 0.02f, parse("AA1 AA1 AE1 AE1 AE1 !", sp), sp, r);
    }
    gurgle(b, 0.0f, 0.9f, 20.0f, r);
    filter(b, Biquad::HP, 150.0f, 0.7f);
    filter(b, Biquad::HighShelf, 1800.0f, 0.7f, 9.0f);  // ein Schrei ist hell
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}

// --- Verwandlung ---------------------------------------------------------------------------------------------------------
void noiseBurst(Buf& b, float at, float len, float hz, float q, float amp, Rand& r) {
    Biquad bp = Biquad::make(Biquad::BP, hz, q, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) b.x[a + i] += bp.run(r.bi()) * amp * envAD((float)i / (float)b.rate, 0.001f, len * 0.8f);
}

Sound skinTear(int rate) {
    Rand r(15500u);
    Buf b(3.4f, rate);
    // 0-1,4 s: Gelenke knacken (helle, kurze Resonanzen), die Haut dehnt sich (tiefes, nasses Knarren)
    for (int i = 0; i < 14; ++i) {
        const float t = 0.05f + 1.35f * std::pow(r.uni(), 0.7f);
        ImpactSpec cr;
        cr.modes = modesCeramic(r.range(1400.0f, 2600.0f), 0.02f, r);
        cr.contact = 0.0004f, cr.noise = 0.5f, cr.noiseHz = 2400.0f;
        b.add(impact(cr, r.range(0.25f, 0.6f), 0.05f, rate, r), t);
    }
    CreakSpec ck;
    ck.rate0 = 18.0f, ck.rate1 = 60.0f, ck.formants = {380.0f, 900.0f, 2100.0f}, ck.q = 5.0f;
    creak(b, 0.2f, 1.25f, ck, 0.35f, r);
    // 1,45 s: Mantel und Haut reissen - Stoff (helle, koernige Reissfront), nasse Haut (tiefer), Knochen springen
    grains(b, 1.45f, 1.85f, 900.0f, [](Buf& g, float at, Rand& rr) { noiseBurst(g, at, rr.range(0.002f, 0.006f), rr.range(2500.0f, 5200.0f), 2.0f, rr.range(0.15f, 0.4f), rr); }, r);
    grains(b, 1.45f, 1.95f, 160.0f, [](Buf& g, float at, Rand& rr) { noiseBurst(g, at, rr.range(0.01f, 0.03f), rr.range(500.0f, 1300.0f), 1.4f, rr.range(0.25f, 0.6f), rr); }, r);
    for (int i = 0; i < 4; ++i) {
        ImpactSpec bo;
        bo.modes = modesWood(r.range(500.0f, 800.0f), 0.04f, r);
        bo.contact = 0.001f, bo.noise = 0.5f;
        b.add(impact(bo, r.range(0.5f, 0.9f), 0.12f, rate, r), 1.5f + 0.09f * (float)i + r.range(0.0f, 0.04f));
    }
    // danach tropft es
    for (int i = 0; i < 9; ++i) bubble(b, 2.0f + r.range(0.0f, 1.2f), r.range(1.0f, 2.6f), r.range(0.1f, 0.3f), r);
    filter(b, Biquad::HP, 60.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

// Mantel: schwerer Stoff schwingt (gefiltertes Rauschen, langsam an- und abschwellend), Leder der Stiefel knarrt kurz
Sound skinCoat(int rate) {
    Rand r(15560u);
    Buf b(1.0f, rate);
    dsp::Svf f;
    for (size_t i = 0; i < b.n(); ++i) {
        const float t = b.t(i);
        f.set(700.0f + 900.0f * std::sin(kPi * std::min(1.0f, t / 0.7f)), 0.9f, (float)rate);
        const float env = envAHR(t, 0.18f, 0.12f, 0.4f) * (0.75f + 0.25f * std::sin(t * 23.0f));
        b.x[i] = f.bp(r.bi()) * env * 0.5f;
    }
    CreakSpec ck;
    ck.rate0 = 45.0f, ck.rate1 = 70.0f, ck.formants = {380.0f, 1100.0f, 2300.0f}, ck.q = 6.0f;
    creak(b, 0.32f, 0.12f, ck, 0.12f, r);
    filter(b, Biquad::HP, 120.0f, 0.7f);
    trim(b);
    normalize(b, level::kFoley - 4.0f);
    return b.sound();
}

// --- Waffen -------------------------------------------------------------------------------------------------------------
// Luftzug eines Schwungs (Bandpass steigt und faellt, Laenge len)
void whoosh(Buf& b, float at, float len, float hz0, float hz1, float amp, Rand& r) {
    dsp::Svf f;
    const size_t a = b.at(at), n = b.at(len);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const float u = (float)i / (float)n;
        f.set(hz0 + (hz1 - hz0) * std::sin(kPi * u), 1.8f, (float)b.rate);
        b.x[a + i] += f.bp(r.bi()) * amp * std::pow(std::sin(kPi * u), 2.0f);
    }
}
// Schaben: Stick-Slip mit Resonanzen und rauem Rauschen
void scrape(Buf& b, float at, float len, float rate0, float rate1, std::vector<float> formants, float q, float amp, float noiseHz, Rand& r) {
    CreakSpec ck;
    ck.rate0 = rate0, ck.rate1 = rate1, ck.formants = std::move(formants), ck.q = q, ck.jitter = 0.45f;
    creak(b, at, len, ck, amp, r);
    Biquad bp = Biquad::make(Biquad::BP, noiseHz, 0.9f, (float)b.rate);
    const size_t a = b.at(at), n = b.at(len);
    if (a + n > b.n()) b.x.resize(a + n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        const float t = (float)i / (float)b.rate;
        const float grit = 0.6f + 0.4f * std::sin(t * 53.0f + 3.0f * std::sin(t * 7.0f));
        b.x[a + i] += bp.run(r.bi()) * amp * 0.25f * grit * envAHR(t, 0.05f, len - 0.15f, 0.1f);
    }
}
std::vector<float> ringOf(float f0, float t60, float amp, int rate, Rand& r, float strength = 1.0f) {
    ImpactSpec sp;
    sp.modes = modesMetalPipe(f0, t60, r);
    sp.contact = 0.0007f, sp.noise = 0.35f, sp.noiseHz = 3500.0f;
    std::vector<float> v = impact(sp, strength, t60 * 1.2f, rate, r);
    for (float& x : v) x *= amp;
    return v;
}

Sound wretchDrag(int rate, int w) {
    Rand r(15600u + (unsigned)w);
    Buf b(2.0f, rate);
    if (w == 0) {  // Rohr: die Spitze schleift ueber Beton, hakt, springt und klingt nach
        scrape(b, 0.0f, 1.25f, 70.0f, 110.0f, {1250.0f, 2600.0f, 4300.0f}, 9.0f, 0.5f, 2400.0f, r);
        for (int i = 0; i < 9; ++i) b.add(ringOf(r.range(620.0f, 700.0f), 0.9f, 0.4f, rate, r, r.range(0.15f, 0.45f)), 0.05f + 0.13f * (float)i + r.range(0.0f, 0.06f));
        b.add(ringOf(660.0f, 1.3f, 0.7f, rate, r, 0.8f), 1.2f);  // Aufsetzen nach einem Hopser
        ImpactSpec cc;
        cc.modes = modesConcrete(r.range(900.0f, 1200.0f), r);
        cc.contact = 0.0008f, cc.noise = 0.6f;
        b.add(impact(cc, 0.5f, 0.08f, rate, r), 1.2f);
    } else if (w == 1) {  // Brett: schlaegt gegen das Bein und den Boden, die Naegel kratzen
        for (int i = 0; i < 3; ++i) {
            ImpactSpec wd;
            wd.modes = modesWood(r.range(320.0f, 420.0f), 0.08f, r);
            wd.contact = 0.002f, wd.noise = 0.4f, wd.noiseHz = 1500.0f;
            b.add(impact(wd, r.range(0.5f, 0.9f), 0.25f, rate, r), 0.05f + 0.38f * (float)i + r.range(0.0f, 0.06f));
        }
        scrape(b, 0.3f, 0.5f, 140.0f, 220.0f, {2200.0f, 3800.0f, 5600.0f}, 12.0f, 0.16f, 3800.0f, r);  // Naegel auf Beton
    } else {  // Blechklinge: das Blech flattert dumpf, das Kabel am Griff klappert
        for (int i = 0; i < 2; ++i) {
            ImpactSpec pl;
            pl.modes = modesMetalPlate(r.range(240.0f, 330.0f), 0.7f, r);
            pl.contact = 0.004f, pl.noise = 0.2f;
            b.add(impact(pl, r.range(0.25f, 0.45f), 0.8f, rate, r), 0.05f + 0.45f * (float)i);
        }
        grains(b, 0.0f, 0.8f, 18.0f, [](Buf& g, float at, Rand& rr) {
            ImpactSpec ti;
            ti.modes = modesPlastic(rr.range(2200.0f, 3200.0f), 0.01f, rr);
            ti.contact = 0.0004f, ti.noise = 0.4f;
            g.add(impact(ti, rr.range(0.08f, 0.2f), 0.03f, g.rate, rr), at);
        }, r);
    }
    filter(b, Biquad::HP, 70.0f, 0.7f);
    trim(b);
    normalize(b, level::kFoley + (w == 2 ? -3.0f : 0.0f));
    return b.sound();
}

Sound wretchRaise(int rate, int w) {
    Rand r(15700u + (unsigned)w);
    Buf b(1.4f, rate);
    if (w == 0) {  // das Rohr reisst vom Boden hoch (kurzes Schaben, Ring), Luftzug, die Hand packt nach
        scrape(b, 0.0f, 0.18f, 120.0f, 60.0f, {1700.0f, 3300.0f}, 9.0f, 0.5f, 3800.0f, r);
        b.add(ringOf(650.0f, 0.8f, 0.4f, rate, r, 0.5f), 0.12f);
        whoosh(b, 0.15f, 0.42f, 300.0f, 900.0f, 0.5f, r);
    } else if (w == 1) {  // das Brett knarrt in der Faust, die Naegel klirren, Luftzug nach hinten
        CreakSpec ck;
        ck.rate0 = 30.0f, ck.rate1 = 55.0f, ck.formants = {520.0f, 1250.0f, 2600.0f}, ck.q = 7.0f;
        creak(b, 0.0f, 0.3f, ck, 0.3f, r);
        whoosh(b, 0.2f, 0.4f, 250.0f, 700.0f, 0.5f, r);
    } else {  // das Blech singt hell auf, das Kabel knirscht
        ImpactSpec pl;
        pl.modes = modesMetalPlate(r.range(900.0f, 1100.0f), 1.0f, r);
        pl.contact = 0.0015f, pl.noise = 0.3f;
        b.add(impact(pl, 0.45f, 1.0f, rate, r), 0.02f);
        whoosh(b, 0.1f, 0.3f, 600.0f, 1600.0f, 0.4f, r);
    }
    filter(b, Biquad::HP, 80.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent - 3.0f);
    return b.sound();
}

Sound wretchMiss(int rate, int w) {
    Rand r(15800u + (unsigned)w);
    Buf b(2.6f, rate);
    whoosh(b, 0.0f, 0.3f, 350.0f, w == 2 ? 1800.0f : 1100.0f, 0.7f, r);
    const float at = 0.24f;
    ImpactSpec cc;  // Beton darunter
    cc.modes = modesConcrete(r.range(700.0f, 1000.0f), r);
    cc.contact = 0.001f, cc.noise = 0.7f, cc.noiseHz = 2000.0f;
    b.add(impact(cc, 0.8f, 0.12f, rate, r), at);
    if (w == 0) {  // Rohr auf Beton: lauter, langer, inharmonischer Klang, ein Nachprellen
        b.add(ringOf(r.range(560.0f, 640.0f), 1.8f, 1.0f, rate, r, 1.0f), at);
        b.add(ringOf(r.range(560.0f, 640.0f), 1.2f, 0.45f, rate, r, 0.5f), at + 0.16f);
    } else if (w == 1) {  // Brett: dumpfer Schlag, splitterndes Holz, die Naegel kreischen ueber den Boden
        ImpactSpec wd;
        wd.modes = modesWood(r.range(260.0f, 340.0f), 0.12f, r);
        wd.contact = 0.003f, wd.noise = 0.5f;
        b.add(impact(wd, 1.0f, 0.35f, rate, r), at);
        grains(b, at, at + 0.15f, 120.0f, [](Buf& g, float t, Rand& rr) { noiseBurst(g, t, 0.004f, rr.range(2000.0f, 4500.0f), 2.5f, rr.range(0.1f, 0.3f), rr); }, r);
        scrape(b, at + 0.02f, 0.3f, 220.0f, 120.0f, {2400.0f, 4200.0f}, 14.0f, 0.24f, 4000.0f, r);
    } else {  // Blech: scheppert auf den Boden, schabt, zittert nach
        ImpactSpec pl;
        pl.modes = modesMetalPlate(r.range(420.0f, 560.0f), 1.1f, r);
        pl.contact = 0.0012f, pl.noise = 0.5f;
        b.add(impact(pl, 1.0f, 1.2f, rate, r), at);
        scrape(b, at + 0.03f, 0.25f, 160.0f, 90.0f, {2500.0f, 4800.0f}, 10.0f, 0.3f, 5000.0f, r);
    }
    filter(b, Biquad::HP, 60.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent);
    return b.sound();
}

Sound wretchHit(int rate, int w) {
    Rand r(15900u + (unsigned)w);
    Buf b(1.6f, rate);
    whoosh(b, 0.0f, 0.22f, 400.0f, 1200.0f, 0.6f, r);
    const float at = 0.18f;
    // Koerper: tiefer, gedaempfter Schlag (Stoff und Fleisch)
    Biquad lp = Biquad::make(Biquad::LP, w == 2 ? 900.0f : 380.0f, 0.8f, (float)rate);
    const size_t a = b.at(at), n = b.at(0.12f);
    for (size_t i = 0; i < n && a + i < b.n(); ++i) {
        const float t = (float)i / (float)rate;
        b.x[a + i] += lp.run(r.bi()) * 1.4f * envAD(t, 0.002f, 0.08f) + std::sin(2.0f * kPi * 70.0f * t) * 0.5f * envAD(t, 0.002f, 0.06f);
    }
    if (w == 0) b.add(ringOf(r.range(580.0f, 640.0f), 0.35f, 0.3f, rate, r, 0.4f), at);  // gedaempft: das Rohr klingt kaum
    else if (w == 1) scrape(b, at + 0.03f, 0.15f, 260.0f, 150.0f, {900.0f, 2400.0f}, 4.0f, 0.3f, 1500.0f, r);  // die Naegel reissen
    else {  // Schnitt: nasser, kurzer Riss, ein heller Klang des Blechs
        grains(b, at, at + 0.08f, 300.0f, [](Buf& g, float t, Rand& rr) { noiseBurst(g, t, 0.006f, rr.range(1500.0f, 3500.0f), 1.6f, rr.range(0.2f, 0.4f), rr); }, r);
        ImpactSpec pl;
        pl.modes = modesMetalPlate(r.range(1200.0f, 1500.0f), 0.4f, r);
        pl.contact = 0.001f, pl.noise = 0.2f;
        b.add(impact(pl, 0.3f, 0.4f, rate, r), at);
    }
    filter(b, Biquad::HP, 50.0f, 0.7f);
    trim(b);
    normalize(b, level::kEvent + 1.0f);
    return b.sound();
}
}  // namespace

void addKinL3Sounds(std::vector<SoundDef>& d, int rate) {
    for (int k = 0; k < 8; ++k) d.push_back({std::format("skin_say_{}", k), [=] { return skinLine(rate, kSkinSay[k], 15100u + (unsigned)k * 13u); }});
    for (int k = 0; k < 4; ++k) d.push_back({std::format("skin_ans_{}", k), [=] { return skinLine(rate, kSkinAns[k], 15200u + (unsigned)k * 13u); }});
    d.push_back({"skin_wait", [=] { return skinLine(rate, kSkinWait, 15250u, true); }});
    d.push_back({"skin_bye", [=] { return skinLine(rate, kSkinBye, 15260u); }});
    d.push_back({"skin_tear", [=] { return skinTear(rate); }});
    d.push_back({"skin_coat", [=] { return skinCoat(rate); }});
    for (int k = 0; k < 8; ++k) d.push_back({std::format("wretch_frag_{}", k), [=] { return wretchFrag(rate, k); }});
    for (int k = 0; k < 3; ++k) d.push_back({std::format("wretch_cry_{}", k), [=] { return wretchCry(rate, k); }});
    static const char* const w[3] = {"pipe", "board", "blade"};
    for (int k = 0; k < 3; ++k) {
        d.push_back({std::format("wretch_drag_{}", w[k]), [=] { return wretchDrag(rate, k); }});
        d.push_back({std::format("wretch_raise_{}", w[k]), [=] { return wretchRaise(rate, k); }});
        d.push_back({std::format("wretch_miss_{}", w[k]), [=] { return wretchMiss(rate, k); }});
        d.push_back({std::format("wretch_hit_{}", w[k]), [=] { return wretchHit(rate, k); }});
    }
}

}  // namespace lim::audio
