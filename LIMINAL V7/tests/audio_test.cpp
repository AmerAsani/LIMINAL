// V6: prueft Klangerzeugung und raeumliches Mischen ohne Audiogeraet.
//
//   audio_test
//
//   - Schleifen (Leuchten, Atmosphaere) sind nahtlos: kein Sprung an der Naht
//   - Leuchtstoffroehre: Grundton 100 Hz (doppelte Netzfrequenz) dominiert, kaum Energie im Bass
//   - Raumklang: Quelle rechts -> rechts lauter und links spaeter (Laufzeitunterschied)
//   - Verdeckung: hinter Waenden leiser und dumpfer (weniger Hoehen)
//   - Nachhall: lange Nachhallzeit klingt laenger nach als kurze
//   - Pegel: das Summen der Leuchten bleibt leiser als das alte, raumfuellende Brummen aus V5
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "core/jobs.hpp"

using namespace lim;
using namespace lim::audio;

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}

static double rms(const std::vector<float>& s, int ch, size_t from = 0, size_t to = 0) {
    if (to == 0) to = s.size() / 2;
    double a = 0;
    for (size_t i = from; i < to; ++i) a += (double)s[i * 2 + ch] * s[i * 2 + ch];
    return std::sqrt(a / std::max<size_t>(1, to - from));
}

// Energie oberhalb ~2 kHz (Differenzsignal als einfacher Hochpass)
static double highs(const std::vector<float>& s, int ch) {
    double a = 0;
    for (size_t i = 1; i < s.size() / 2; ++i) {
        double d = (double)s[i * 2 + ch] - s[(i - 1) * 2 + ch];
        a += d * d;
    }
    return std::sqrt(a / (s.size() / 2));
}

// Goertzel: Amplitude einer Frequenz
static double tone(const std::vector<float>& x, double f, int rate) {
    double w = 2.0 * 3.14159265358979 * f / rate, c = 2.0 * std::cos(w), s0 = 0, s1 = 0, s2 = 0;
    for (float v : x) {
        s0 = v + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return std::sqrt(std::max(0.0, p)) / (x.size() * 0.5);
}

static std::vector<float> run(AudioSystem& a, int frames) {
    std::vector<float> out((size_t)frames * 2, 0.0f);
    const int block = 480;
    for (int i = 0; i < frames; i += block) a.mixForTest(out.data() + (size_t)i * 2, std::min(block, frames - i));
    return out;
}

int main() {
    JobSystem jobs(2);
    const int rate = 48000;

    // --- Schleifen und Klangfarbe -----------------------------------------------------------
    {
        AudioSystem a;
        a.initOffline(jobs);
        for (const char* n : {"lamp_hum_0", "lamp_hum_1", "lamp_hum_2", "lamp_hum_3", "lamp_ind_0", "lamp_ind_1", "lamp_emg_0",
                              "lamp_emg_1", "hum", "amb_office", "amb_halls"}) {
            const Sound* s = a.sound(n);
            check(s && s->loop && !s->samples.empty(), std::string(n) + ": fehlt oder keine Schleife");
            if (!s || s->samples.size() < 10) continue;
            // Sprung an der Naht im Vergleich zu typischen Nachbarschritten
            const auto& x = s->samples;
            double typical = 0;
            for (size_t i = 1; i < x.size(); ++i) typical = std::max(typical, (double)std::fabs(x[i] - x[i - 1]));
            double seam = std::fabs(x.front() - x.back());
            check(seam <= typical * 1.05 + 1e-4, std::string(n) + ": Sprung an der Schleifennaht");
        }
        const Sound* hum = a.sound("lamp_hum_0");
        if (hum) {
            std::vector<float> seg(hum->samples.begin(), hum->samples.begin() + rate);
            double f100 = tone(seg, 100, rate), f50 = tone(seg, 50, rate), f30 = tone(seg, 30, rate);
            double f200 = tone(seg, 200, rate);
            std::printf("Leuchtstoffroehre: 50 Hz %.3f, 100 Hz %.3f, 200 Hz %.3f, 30 Hz %.4f\n", f50, f100, f200, f30);
            check(f100 > f50 * 2.5 && f100 > f200, "Grundton 100 Hz dominiert nicht");
            check(f30 < f100 * 0.05, "Energie im tiefen Bass (Brummschleife?)");
        }
        for (const char* n : {"dist_door", "dist_clank", "dist_rumble", "dist_knock", "dist_steam", "dist_creak", "lamp_tick"})
            check(a.sound(n) && !a.sound(n)->samples.empty(), std::string(n) + " fehlt");
    }

    // --- Richtung: Quelle rechts --------------------------------------------------------------
    {
        AudioSystem a;
        a.initOffline(jobs);
        a.setVolumes(1, 1, 1);
        a.setListener({0, 0, 1.6f}, 0.0f);  // Blick nach +x; rechts ist +y (wie die Kamera, math.hpp viewMatrix)
        a.setAcoustics({0.2f, 0.5f, 0.0f, 0.01f});
        int id = a.startLoop("lamp_hum_0", 1.0f, Bus::Effects);
        a.setLoopSend(id, 0.0f);
        a.setLoopSpatial(id, {{0, 3, 1.6f}, 10.0f, 0.0f});
        auto out = run(a, rate);
        double l = rms(out, 0, rate / 2), r = rms(out, 1, rate / 2);
        std::printf("Quelle rechts: links %.4f, rechts %.4f\n", l, r);
        check(r > l * 1.8, "Quelle rechts nicht deutlich rechts");
        // Laufzeit: Kreuzkorrelation zwischen links und rechts
        int best = 0;
        double bestC = -1e9;
        for (int lag = -40; lag <= 40; ++lag) {
            double c = 0;
            for (int i = rate / 2; i < rate / 2 + 4800; ++i) c += (double)out[(size_t)i * 2 + 1] * out[(size_t)(i + lag) * 2];
            if (c > bestC) bestC = c, best = lag;
        }
        std::printf("Laufzeitunterschied: linkes Ohr %d Samples spaeter\n", best);
        check(best >= 10 && best <= 35, "kein plausibler Laufzeitunterschied fuer eine Quelle rechts");
    }

    // --- Verdeckung ----------------------------------------------------------------------------
    {
        double level[2], hf[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initOffline(jobs);
            a.setVolumes(1, 1, 1);
            a.setListener({0, 0, 1.6f}, 0.0f);
            a.setAcoustics({0.2f, 0.5f, 0.0f, 0.01f});
            // breitbandiger Klang (Atmosphaere), damit die Hoehen messbar sind
            int id = a.startLoop("amb_halls", 1.0f, Bus::Effects);
            a.setLoopSend(id, 0.0f);
            a.setLoopSpatial(id, {{4, 0, 2.8f}, 12.0f, k == 0 ? 0.0f : 1.0f});
            auto out = run(a, rate / 2);
            level[k] = rms(out, 0, rate / 8);
            hf[k] = highs(out, 0) / std::max(1e-9, level[k]);
        }
        std::printf("Verdeckung: Pegel %.4f -> %.4f, Hoehenanteil %.3f -> %.3f\n", level[0], level[1], hf[0], hf[1]);
        check(level[1] < level[0] * 0.6, "hinter der Wand nicht leiser");
        check(hf[1] < hf[0] * 0.6, "hinter der Wand nicht dumpfer");
    }

    // --- Nachhall --------------------------------------------------------------------------------
    {
        double tail[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initOffline(jobs);
            a.setVolumes(1, 1, 1);
            a.setListener({0, 0, 1.6f}, 0.0f);
            audio::Acoustics ac{k == 0 ? 0.4f : 2.5f, 0.3f, 0.3f, 0.02f};
            a.setAcoustics(ac);
            run(a, rate * 3);  // Hall auf Zielwerte einschwingen lassen
            a.play("item_land", 1.0f);
            auto out = run(a, rate * 2);
            tail[k] = rms(out, 0, (size_t)(rate * 0.8), (size_t)(rate * 1.4));
            if (k == 1) {
                double direct = rms(out, 0, 0, (size_t)(rate * 0.04)), early = rms(out, 0, (size_t)(rate * 0.15), (size_t)(rate * 0.45));
                std::printf("Halle: Direktschall %.4f, Nachhall 0.15-0.45 s %.4f (%.0f %%)\n", direct, early, 100.0 * early / direct);
                check(early > direct * 0.04 && early < direct * 0.6, "Nachhall der Halle unplausibel laut/leise");
            }
        }
        std::printf("Nachhall 0.8-1.4 s nach dem Klang: kurzer Raum %.5f, Halle %.5f\n", tail[0], tail[1]);
        check(tail[1] > tail[0] * 4.0, "lange Nachhallzeit klingt nicht laenger nach");
    }

    // --- Pegel: Buero mit Leuchten gegen das alte Brummen (V5: 'hum' mit 0.22, ueberall gleich) --
    {
        AudioSystem oldA;
        oldA.initOffline(jobs);
        oldA.setVolumes(0.8f, 1.0f, 0.8f);
        oldA.startLoop("hum", 0.22f, Bus::Ambience);
        double oldLevel = rms(run(oldA, rate), 0);

        AudioSystem a;
        a.initOffline(jobs);
        a.setVolumes(0.8f, 1.0f, 0.8f);
        a.setListener({0, 0, 1.6f}, 0.0f);
        a.setAcoustics({0.6f, 0.45f, 0.08f, 0.012f});
        // vier Deckenleuchten wie im Bueroraster (Lautstaerken wie die Klanglandschaft sie setzt)
        const float r = 9.0f;
        const float pos[4][2] = {{1.0f, 0.5f}, {-2.5f, 1.0f}, {1.5f, -3.5f}, {4.5f, 3.0f}};
        for (int i = 0; i < 4; ++i) {
            int id = a.startLoop("lamp_hum_" + std::to_string(i), 0.13f, Bus::Ambience);
            a.setLoopSend(id, 0.55f);
            a.setLoopSpatial(id, {{pos[i][0], pos[i][1], 2.95f}, r, 0.0f});
        }
        double newLevel = rms(run(a, rate), 0);
        std::printf("Pegel Buero: V5-Brummen %.4f, V6-Leuchten %.4f (%.0f %%)\n", oldLevel, newLevel, 100.0 * newLevel / oldLevel);
        check(newLevel > oldLevel * 0.15, "Leuchten praktisch unhoerbar");
        check(newLevel < oldLevel * 0.9, "Leuchten lauter als das alte Brummen (soll dezent sein)");
    }
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
