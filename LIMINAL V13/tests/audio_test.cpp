// Prueft Klangerzeugung und raeumliches Mischen ohne Audiogeraet.
//
//   audio_test
//
// V6/V10 (weiter gueltig):
//   - Schleifen sind nahtlos; Leuchtstoffroehre: 100 Hz dominiert
//   - Richtung: Quelle rechts -> rechts lauter und links spaeter (Laufzeitunterschied)
//   - Verdeckung: hinter Waenden leiser und dumpfer
//   - Nachhall: lange Nachhallzeit klingt laenger nach als kurze
// V11:
//   - alle Klaenge vorhanden, endlich, nicht uebersteuert; Lautheit nahe am Ziel ihrer Art
//   - Abstand nach 1/d, Kopfschatten (abgewandtes Ohr dumpfer), Luftdaempfung
//   - Begrenzer: laute Summen ohne Clipping, leise Klaenge unveraendert (keine Dauersaettigung)
//   - Hall-Abbruch (Level 37), Ducking der Atmosphaere, Varianten ohne direkte Wiederholung
// V13.3 (AUDIO_BUG.md):
//   - gestoppte unhoerbare Schleifen geben ihre Stimme frei (B-017), 200 Ebenenwechsel ohne Stau
//   - Stille-Erkennung am Master-Ausgang (Alarm bei erwartetem Klang, kein Fehlalarm), NaN-Schutz,
//     Pause/Daempfung bleiben nicht haengen
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "audio/dsp.hpp"
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
static double highs(const std::vector<float>& s, int ch, size_t from = 1) {
    double a = 0;
    for (size_t i = std::max<size_t>(1, from); i < s.size() / 2; ++i) {
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

static void quietRoom(AudioSystem& a) {
    a.setVolumes(1, 1, 1);
    a.setListener({0, 0, 1.6f}, 0.0f);  // Blick nach +x; rechts ist +y
    Acoustics ac{0.2f, 0.5f, 0.0f, 0.01f};
    a.setAcoustics(ac);
}

int main() {
    const int rate = 48000;
    JobSystem jobs(4);
    auto t0 = std::chrono::steady_clock::now();
    AudioSystem base;
    base.initOffline(jobs);
    const double synthMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("Klangerzeugung: %zu Klaenge in %.0f ms (4 Threads)\n", base.names().size(), synthMs);

    // --- Bestand, Gueltigkeit, Lautheit ----------------------------------------------------------
    {
        const char* required[] = {"step_concrete_0", "step_puddle_0", "step_carpet_wet_0", "step_metal_0", "step_wood_0", "step_water_0",
                                  "lamp_hum_0", "lamp_ind_0", "lamp_emg_0", "lamp_tick_0", "lamp_start_0", "power_off", "power_on",
                                  "breaker_far_0", "drip_0", "pipe_knock_0", "rolltor_0", "elev_ding", "elev_doors", "elev_ride",
                                  "smiler_presence", "smiler_open", "smiler_attack", "hound_growl_0", "hound_bark_0", "hound_whine",
                                  "hound_step_0", "clump_knock_0", "clump_reach", "clump_grab", "moth_flutter", "moth_takeoff",
                                  "deep_surge", "hal_whisper_0", "hotel_music", "waves_far", "bed_l0_room", "bed_l1_space",
                                  "bed_l37_air", "dist_door_0", "rain_glass", "pool_loop", "heartbeat", "ui_select"};
        for (const char* n : required) check(base.sound(n) != nullptr, std::string(n) + " fehlt");
        int bad = 0, loud = 0;
        for (const auto& n : base.names()) {
            const Sound* s = base.sound(n);
            bool ok = s && !s->samples.empty();
            float pk = 0;
            if (ok)
                for (float v : s->samples) {
                    if (!std::isfinite(v)) ok = false;
                    pk = std::max(pk, std::fabs(v));
                }
            if (!ok || pk > 0.95f || pk < 0.003f) {
                ++bad;
                std::printf("  ungueltig/uebersteuert/stumm: %s (Spitze %.3f)\n", n.c_str(), pk);
            }
            if (ok && dsp::loudnessDb(s->samples, (float)s->rate) > -14.0f) ++loud;
        }
        check(bad == 0, "alle Klaenge gueltig, nicht uebersteuert, hoerbar");
        check(loud == 0, "kein Klang lauter als -14 LUFS (Lautheit nach Art normiert)");
        // Lautheit der Schritte nahe am Ziel (-26 LUFS +- 3)
        for (const char* n : {"step_concrete_0", "step_tile_2", "step_wood_4", "step_puddle_1"}) {
            const float l = dsp::loudnessDb(base.sound(n)->samples, (float)rate);
            check(l > -30.0f && l < -22.0f, std::string(n) + ": Lautheit ausserhalb des Ziels");
        }
        // Varianten
        check(base.variants("step_concrete") == 8, "8 Varianten je Schritt");
        check(base.variants("hound_growl") >= 3, "Knurren mit Varianten");
    }

    // --- Schleifen und Klangfarbe --------------------------------------------------------------------
    {
        for (const char* n : {"lamp_hum_0", "lamp_hum_3", "lamp_ind_0", "lamp_ind_2", "lamp_emg_0", "lamp_sconce_0", "bed_l0_room",
                              "bed_l1_space", "bed_l2_warm", "bed_l3_mains", "water_lap", "pool_loop", "smiler_presence",
                              "moth_flutter", "hotel_music", "elev_ride", "rain_glass"}) {
            const Sound* s = base.sound(n);
            check(s && s->loop && !s->samples.empty(), std::string(n) + ": fehlt oder keine Schleife");
            if (!s || s->samples.size() < 10) continue;
            const auto& x = s->samples;
            double typical = 0;
            for (size_t i = 1; i < x.size(); ++i) typical = std::max(typical, (double)std::fabs(x[i] - x[i - 1]));
            double seam = std::fabs(x.front() - x.back());
            check(seam <= typical * 1.05 + 1e-4, std::string(n) + ": Sprung an der Schleifennaht");
        }
        const Sound* hum = base.sound("lamp_hum_0");
        if (hum) {
            std::vector<float> seg(hum->samples.begin(), hum->samples.begin() + rate);
            double f100 = tone(seg, 100, rate), f50 = tone(seg, 50, rate), f30 = tone(seg, 30, rate), f200 = tone(seg, 200, rate);
            std::printf("Leuchtstoffroehre: 50 Hz %.3f, 100 Hz %.3f, 200 Hz %.3f, 30 Hz %.4f\n", f50, f100, f200, f30);
            check(f100 > f50 * 2.5 && f100 > f200, "Grundton 100 Hz dominiert nicht");
            check(f30 < f100 * 0.05, "Energie im tiefen Bass (Brummschleife?)");
        }
        // Tropfen: steigender Ton (Minnaert) - Nulldurchgaenge im zweiten Fenster haeufiger als im ersten
        const Sound* dr = base.sound("drip_0");
        if (dr && dr->samples.size() > 1500) {
            auto zc = [&](size_t a, size_t b) {
                int c = 0;
                for (size_t i = a + 1; i < b; ++i)
                    if ((dr->samples[i] >= 0) != (dr->samples[i - 1] >= 0)) ++c;
                return c;
            };
            const int z1 = zc(240, 720), z2 = zc(720, 1200);
            std::printf("Tropfen: Nulldurchgaenge %d -> %d (steigender Ton)\n", z1, z2);
            check(z2 > z1, "Tropfen: Ton steigt nicht an");
        }
    }

    // --- Richtung: Quelle rechts --------------------------------------------------------------------
    {
        AudioSystem a;
        a.initShared(base);  // V13.3: Klangbank teilen (schneller)
        quietRoom(a);
        int id = a.startLoop("lamp_hum_0", 1.0f, Bus::Effects);
        a.setLoopSend(id, 0.0f);
        a.setLoopSpatial(id, {{0, 3, 1.6f}, 10.0f, 0.0f});
        auto out = run(a, rate);
        double l = rms(out, 0, rate / 2), r = rms(out, 1, rate / 2);
        std::printf("Quelle rechts: links %.4f, rechts %.4f\n", l, r);
        check(r > l * 1.8, "Quelle rechts nicht deutlich rechts");
        int best = 0;
        double bestC = -1e9;
        for (int lag = -40; lag <= 40; ++lag) {
            double c = 0;
            for (int i = rate / 2; i < rate / 2 + 4800; ++i) c += (double)out[(size_t)i * 2 + 1] * out[(size_t)(i + lag) * 2];
            if (c > bestC) bestC = c, best = lag;
        }
        std::printf("Laufzeitunterschied: linkes Ohr %d Samples spaeter\n", best);
        check(best >= 10 && best <= 35, "kein plausibler Laufzeitunterschied fuer eine Quelle rechts");
        // Kopfschatten: das abgewandte (linke) Ohr hat relativ weniger Hoehen
        AudioSystem b;
        b.initShared(base);  // V13.3: Klangbank teilen (schneller)
        quietRoom(b);
        int id2 = b.startLoop("rain_glass", 1.0f, Bus::Effects);
        b.setLoopSend(id2, 0.0f);
        b.setLoopSpatial(id2, {{0, 3, 1.6f}, 10.0f, 0.0f});
        auto o2 = run(b, rate / 2);
        const double hl = highs(o2, 0, rate / 8) / std::max(1e-9, rms(o2, 0, rate / 8)), hr = highs(o2, 1, rate / 8) / std::max(1e-9, rms(o2, 1, rate / 8));
        std::printf("Kopfschatten: Hoehenanteil links %.3f, rechts %.3f\n", hl, hr);
        check(hl < hr * 0.8, "abgewandtes Ohr nicht dumpfer (Kopfschatten)");
    }

    // --- Abstand 1/d und Luftdaempfung ------------------------------------------------------------------
    {
        double level[2], hf[2];
        const float dist[2] = {2.0f, 8.0f};
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initShared(base);  // V13.3: Klangbank teilen (schneller)
            quietRoom(a);
            int id = a.startLoop("rain_glass", 1.0f, Bus::Effects);
            a.setLoopSend(id, 0.0f);
            a.setLoopSpatial(id, {{dist[k], 0, 1.6f}, 60.0f, 0.0f});
            auto out = run(a, rate / 2);
            level[k] = rms(out, 0, rate / 8);
            hf[k] = highs(out, 0, rate / 8) / std::max(1e-9, level[k]);
        }
        const double ratio = level[0] / std::max(1e-9, level[1]);
        std::printf("Abstand 2 m / 8 m: Pegelverhaeltnis %.2f (1/d erwartet ~4), Hoehen %.3f -> %.3f\n", ratio, hf[0], hf[1]);
        check(ratio > 3.0 && ratio < 5.5, "Pegelabfall folgt nicht ungefaehr 1/d");
    }

    // --- Verdeckung ----------------------------------------------------------------------------------
    {
        double level[2], hf[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initShared(base);  // V13.3: Klangbank teilen (schneller)
            quietRoom(a);
            int id = a.startLoop("rain_glass", 1.0f, Bus::Effects);
            a.setLoopSend(id, 0.0f);
            a.setLoopSpatial(id, {{4, 0, 2.8f}, 12.0f, k == 0 ? 0.0f : 1.0f});
            auto out = run(a, rate / 2);
            level[k] = rms(out, 0, rate / 8);
            hf[k] = highs(out, 0, rate / 8) / std::max(1e-9, level[k]);
        }
        std::printf("Verdeckung: Pegel %.4f -> %.4f, Hoehenanteil %.3f -> %.3f\n", level[0], level[1], hf[0], hf[1]);
        check(level[1] < level[0] * 0.6, "hinter der Wand nicht leiser");
        check(hf[1] < hf[0] * 0.6, "hinter der Wand nicht dumpfer");
    }

    // --- Nachhall --------------------------------------------------------------------------------------
    {
        double tail[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initShared(base);  // V13.3: Klangbank teilen (schneller)
            a.setVolumes(1, 1, 1);
            a.setListener({0, 0, 1.6f}, 0.0f);
            Acoustics ac{k == 0 ? 0.4f : 2.5f, 0.3f, 0.3f, 0.02f};
            a.setAcoustics(ac);
            run(a, rate * 3);
            a.play("item_land_0", 1.0f);
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
        // Level 37: die Fahne bricht ab
        double gated[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initShared(base);  // V13.3: Klangbank teilen (schneller)
            a.setVolumes(1, 1, 1);
            Acoustics ac{2.5f, 0.3f, 0.3f, 0.02f};
            ac.gate = k == 0 ? 0.0f : 0.25f;
            a.setAcoustics(ac);
            run(a, rate * 3);
            a.play("item_land_0", 1.0f);
            auto out = run(a, rate * 2);
            gated[k] = rms(out, 0, (size_t)(rate * 0.8), (size_t)(rate * 1.4));
        }
        std::printf("Hall-Abbruch (Level 37): Fahne %.5f -> %.5f\n", gated[0], gated[1]);
        check(gated[1] < gated[0] * 0.2, "Hall bricht mit gate nicht ab");
        // Fruehreflexionen eines kleinen Raums: Spiegelquellen aus den Raummassen veraendern den Klang
        {
            auto taps = dsp::shoeboxReflections(4, 5, 2.7f, 1.2f, 2.0f, 1.6f, 0.2f, 0.0f);
            float first = 1e9f;
            for (auto& t : taps) first = std::min(first, t.delay);
            std::printf("Fruehreflexionen: %zu Spiegelquellen, erste nach %.1f ms (Decke 1,1 m -> 6,4 ms)\n", taps.size(), first * 1000.0f);
            check(taps.size() >= 18 && std::fabs(first - 2.2f / 343.0f) < 0.0005f, "Spiegelquellen des Quaders falsch");
            std::vector<float> o[2];
            for (int k = 0; k < 2; ++k) {
                AudioSystem a;
                a.initShared(base);  // V13.3: Klangbank teilen (schneller)
                a.setVolumes(1, 1, 1);
                Acoustics ac{0.3f, 0.5f, 0.3f, 0.06f};
                if (k == 1) ac.roomW = 4, ac.roomD = 5, ac.roomH = 2.7f, ac.lx = 1.2f, ac.ly = 2.0f, ac.lz = 1.6f, ac.early = 1.0f;
                a.setAcoustics(ac);
                run(a, rate);
                a.play("flash_on", 1.0f);
                o[k] = run(a, rate / 4);
            }
            double d = 0, e = 0;
            for (size_t i = 0; i < o[0].size(); ++i) d += (double)(o[1][i] - o[0][i]) * (o[1][i] - o[0][i]), e += (double)o[0][i] * o[0][i];
            std::printf("Fruehreflexionen im Mischer: Anteil %.1f %%\n", 100.0 * std::sqrt(d / std::max(1e-12, e)));
            check(d > e * 0.0025, "Mischer erzeugt keine Fruehreflexionen aus den Raummassen");
        }
    }

    // --- Begrenzer: laut ohne Clipping, leise unveraendert -------------------------------------------
    {
        AudioSystem a;
        a.initShared(base);  // V13.3: Klangbank teilen (schneller)
        quietRoom(a);
        for (int k = 0; k < 12; ++k) a.play("hound_bark_0", 1.0f);
        auto out = run(a, rate / 2);
        float pk = 0;
        for (float v : out) pk = std::max(pk, std::fabs(v));
        std::printf("Begrenzer: Spitze bei 12 gleichzeitigen Klaengen %.3f\n", pk);
        check(pk <= 0.95f, "Ausgang uebersteuert (> -0.45 dBFS)");
        AudioSystem q;
        q.initShared(base);  // V13.3: Klangbank teilen (schneller)
        quietRoom(q);
        q.play("step_concrete_0", 0.5f);
        run(q, rate / 4);
        check(q.limiterGain() > 0.999f, "leise Klaenge werden begrenzt (Dauersaettigung?)");
    }

    // --- Ducking: laute Ereignisse druecken die Atmosphaere -------------------------------------------
    {
        double amb[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initShared(base);  // V13.3: Klangbank teilen (schneller)
            quietRoom(a);
            a.startLoop("bed_l1_space", 1.0f, Bus::Ambience);
            run(a, rate / 2);
            if (k == 1)
                for (int j = 0; j < 4; ++j) a.play("rolltor_0", 1.0f);
            run(a, rate / 5);
            // Atmosphaere allein messen: Effektbus stumm schalten, Ducking-Zustand bleibt kurz bestehen
            a.setBusVolume(Bus::Effects, 0.0f);
            auto out = run(a, rate / 10);
            amb[k] = rms(out, 0, 480);  // erste 10 ms: Vorausschau des Begrenzers enthaelt noch den alten Klang
        }
        std::printf("Ducking: Atmosphaere %.5f -> %.5f\n", amb[0], amb[1]);
        check(amb[1] < amb[0] * 0.9, "Atmosphaere wird bei lauten Ereignissen nicht abgesenkt");
    }

    // --- Varianten: nie zweimal dieselbe hintereinander ----------------------------------------------
    {
        AudioSystem a;
        a.initShared(base);  // V13.3: Klangbank teilen (schneller)
        quietRoom(a);
        int repeats = 0;
        const Sound* last = nullptr;
        std::vector<const Sound*> seen;
        for (int k = 0; k < 60; ++k) {
            const Sound* s = a.pickVariant("step_concrete");
            if (s == last) ++repeats;
            last = s;
            if (std::find(seen.begin(), seen.end(), s) == seen.end()) seen.push_back(s);
        }
        std::printf("Varianten: %zu von 8 benutzt, %d direkte Wiederholungen in 60 Schritten\n", seen.size(), repeats);
        check(repeats == 0, "dieselbe Variante zweimal hintereinander");
        check(seen.size() == 8, "nicht alle Varianten werden benutzt");
        check(a.variants("hound_bark") == 2, "hound_bark: 2 Varianten");
    }

    // --- V13.3 (AUDIO_BUG.md): Stimmen werden frei, Ebenenwechsel, Stille-Erkennung, NaN, haengende Zustaende ----------
    auto dbOf = [](double x) { return x > 1e-9 ? 20.0 * std::log10(x) : -180.0; };
    {
        // B-017: eine gestoppte Schleife, die gerade nicht zu hoeren ist (ausser Reichweite, Lautstaerke 0), blieb fuer
        // immer belegt. Nach 96 solchen Schleifen nahm das System keinen Klang mehr an.
        AudioSystem a;
        a.initShared(base);
        quietRoom(a);
        int startFails = 0;
        for (int k = 0; k < 500; ++k) {
            const int id = a.startLoop(k % 2 ? "lamp_hum_1" : "lamp_ind_0", k % 3 ? 0.13f : 0.0f, Bus::Ambience);
            if (id < 0) {
                ++startFails;
                continue;
            }
            a.setLoopSpatial(id, {{30.0f, 0.0f, 1.6f}, 9.0f, 0.0f});  // jenseits der Reichweite: unhoerbar
            run(a, 480);
            a.stopLoop(id);
            run(a, 480);
        }
        const AudioStats st = a.stats();
        std::printf("Stimmen nach 500 gestoppten unhoerbaren Schleifen: %d aktiv, %d haengend, %llu abgewiesen, %d Startfehler\n", st.voices,
                    st.stuck, st.allocFails, startFails);
        check(st.voices == 0 && st.stuck == 0, "gestoppte unhoerbare Schleifen belegen keine Stimmen mehr (B-017)");
        check(startFails == 0 && st.allocFails == 0, "Schleifen starten immer (keine erschoepften Stimmen)");
        a.play("step_concrete_0", 1.0f);
        auto out = run(a, rate / 10);
        check(rms(out, 0) > 0.003, "nach 500 Schleifen klingt ein Schritt (Pegel am Master-Ausgang)");
    }
    {
        // Ebenenwechsel-Stresstest (ohne Geraet): je Ebene Klangbetten, Leuchten in und ausser Reichweite, Automaten,
        // Einzelklaenge, Hoerer laeuft; dann stopAll + resetMix wie App::enterPlaying. 200 Wechsel.
        AudioSystem a;
        a.initShared(base);
        quietRoom(a);
        a.enableSilenceCheck(true);
        int worstVoices = 0, notDrained = 0, quiet = 0, peakVoices = 0;
        double minDb = 0.0;
        dsp::Rand r{77};
        for (int lvl = 0; lvl < 200; ++lvl) {
            std::vector<int> ids;
            for (const char* bed : {"bed_l0_room", "bed_l1_space"}) {
                const int id = a.startLoop(bed, 0.0f, Bus::Ambience);
                a.setLoopSend(id, 0.0f);
                ids.push_back(id);
            }
            for (int k = 0; k < 12; ++k) {
                const int id = a.startLoop(std::string("lamp_hum_") + std::to_string(k % 4), 0.0f, Bus::Ambience);
                a.setLoopSpatial(id, {{r.range(-14.0f, 14.0f), r.range(-14.0f, 14.0f), 2.6f}, 9.0f, r.range(0.0f, 1.0f)});
                ids.push_back(id);
            }
            if (lvl % 3 == 0) a.setPaused(true);    // Pausenmenue offen beim Wechsel
            if (lvl % 5 == 0) a.setMuffle(0.75f);   // Tod (Schreck) vor dem Wechsel
            for (int step = 0; step < 10; ++step) {
                for (size_t k = 0; k < ids.size(); ++k) a.setLoopVolume(ids[k], k < 2 ? 0.4f : 0.13f * (float)step / 9.0f);
                a.setListener({r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f), 1.6f}, r.range(0.0f, 6.28f));
                PlayOpts o;
                o.volume = 0.6f;
                a.play2d("step_concrete", o);
                a.playAt("dist_door_0", {r.range(-30.0f, 30.0f), r.range(-30.0f, 30.0f), 1.0f}, 0.8f, 20.0f);
                run(a, 960);
                // Leuchten verlassen den Bereich und werden gestoppt (wie Soundscape::updateLamps)
                if (step == 6)
                    for (size_t k = 2; k < ids.size(); k += 2) {
                        a.setLoopSpatial(ids[k], {{40.0f, 40.0f, 2.6f}, 9.0f, 0.0f});
                        a.stopLoop(ids[k]);
                    }
            }
            a.setPaused(false);
            a.setMuffle(0.0f);
            auto out = run(a, rate / 5);
            const double db = dbOf(rms(out, 0));
            minDb = std::min(minDb, db);
            if (db < -45.0) ++quiet;
            peakVoices = std::max(peakVoices, a.stats().voices);
            // Wechsel
            a.stopAll();
            a.resetMix();
            run(a, 480 * 8);  // 80 ms: alles ausgeblendet
            const AudioStats st = a.stats();
            worstVoices = std::max(worstVoices, st.voices);
            if (st.voices != 0) ++notDrained;
        }
        const AudioStats st = a.stats();
        std::printf("Ebenenwechsel 200x: nach dem Wechsel hoechstens %d Stimmen aktiv (%d nicht leer), Spitze %d Stimmen, leisester Ebenenpegel %.1f dBFS, "
                    "%llu abgewiesen, %llu Stille-Alarme, %llu NaN\n",
                    worstVoices, notDrained, peakVoices, minDb, st.allocFails, st.silenceAlarms, st.nonFinite);
        check(notDrained == 0, "nach jedem Ebenenwechsel sind alle Stimmen frei");
        check(quiet == 0, "jede Ebene klingt (Master-Ausgang ueber -45 dBFS)");
        check(st.allocFails == 0 && st.silenceAlarms == 0 && st.nonFinite == 0, "keine erschoepften Stimmen, kein Fehlalarm, kein NaN");
        check(st.stuck == 0 && st.reclaimed == 0, "keine Stimme haengt im Ausblenden (Watchdog musste nichts zurueckholen)");
    }
    {
        // Stille-Erkennung: Pegelmessung am Master-Ausgang schlaegt bei erwartetem Klang Alarm (gestellter stummer Zustand),
        // der Mischer setzt sich zurueck und klingt wieder; kein Alarm bei gewollter Stille (Master 0, nichts spielt, Pause)
        AudioSystem a;
        a.initShared(base);
        quietRoom(a);
        a.enableSilenceCheck(true);
        const int id = a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
        run(a, rate / 2);
        a.injectFault(Fault::MuteOutput);
        auto o1 = run(a, rate * 3 / 4);
        const AudioStats s1 = a.stats();
        auto o2 = run(a, rate * 3 / 4);
        const AudioStats s2 = a.stats();
        const double tail = rms(o2, 0, (size_t)(rate / 2));
        std::printf("Stille-Erkennung: stumm %.1f dBFS -> Alarme %llu nach 0,75 s, %llu nach 1,5 s; danach %.1f dBFS\n", dbOf(rms(o1, 0)),
                    s1.silenceAlarms, s2.silenceAlarms, dbOf(tail));
        check(s1.silenceAlarms == 0 && s2.silenceAlarms == 1, "Stille bei erwartetem Klang wird nach 1 s erkannt (genau ein Alarm)");
        check(tail > 0.003, "nach dem Alarm klingt der Master-Ausgang wieder");
        // keine Fehlalarme
        a.setVolumes(0.0f, 1.0f, 1.0f);
        run(a, rate * 2);
        a.setVolumes(1.0f, 1.0f, 1.0f);
        a.setPaused(true);
        run(a, rate * 2);
        a.setPaused(false);
        a.stopLoop(id);
        run(a, rate * 2);
        check(a.stats().silenceAlarms == 1, "kein Fehlalarm bei Master 0, Pause oder ohne Klang");
    }
    {
        // NaN im DSP-Zustand: vorher blieb der Ausgang fuer immer ungueltig (Hall und Begrenzer tragen den Wert weiter)
        AudioSystem a;
        a.initShared(base);
        quietRoom(a);
        a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
        run(a, rate / 2);
        a.injectFault(Fault::NanOutput);
        auto o = run(a, rate / 2);
        bool finite = true;
        for (float v : o) finite = finite && std::isfinite(v);
        const AudioStats st = a.stats();
        std::printf("NaN im Filterzustand: %llu ungueltige Bloecke, danach %.1f dBFS\n", st.nonFinite, dbOf(rms(o, 0, (size_t)(rate / 4))));
        check(finite && st.nonFinite == 1, "NaN wird erkannt, der Block stumm geschaltet, der Zustand zurueckgesetzt");
        check(rms(o, 0, (size_t)(rate / 4)) > 0.003, "nach dem NaN klingt es wieder");
        // ungueltige Angaben des Spiels werden abgewiesen statt den Mischer zu vergiften
        const int id = a.startLoop("lamp_hum_0", 1.0f, Bus::Effects);
        a.setLoopSpatial(id, {{std::nanf(""), 0.0f, 1.6f}, 9.0f, 0.0f});
        a.setListener({0.0f, std::nanf(""), 1.6f}, 0.0f);
        a.playAt("dist_door_0", {std::nanf(""), 0.0f, 0.0f}, 1.0f, 20.0f);
        auto o2 = run(a, rate / 4);
        bool fin2 = true;
        for (float v : o2) fin2 = fin2 && std::isfinite(v);
        check(fin2 && a.stats().nonFinite == 1 && a.stats().badInput == 3, "NaN-Positionen werden abgewiesen (3), Ausgang bleibt gueltig");
    }
    {
        // Pause und Daempfung bleiben nicht haengen: resetMix (neue Sitzung, Hauptmenue) stellt den vollen Pegel her
        double lvl[2];
        for (int k = 0; k < 2; ++k) {
            AudioSystem a;
            a.initShared(base);
            quietRoom(a);
            a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
            if (k == 1) a.setPaused(true), a.setMuffle(0.75f);
            run(a, rate / 2);  // beide Laeufe an derselben Stelle des Klangs messen
            a.resetMix();
            run(a, rate / 2);
            lvl[k] = rms(run(a, rate / 2), 0);
        }
        std::printf("Pause + Daempfung, dann resetMix: %.1f dBFS (ohne: %.1f dBFS)\n", dbOf(lvl[1]), dbOf(lvl[0]));
        check(std::fabs(dbOf(lvl[1]) - dbOf(lvl[0])) < 1.0, "nach resetMix voller Pegel (Pause/Daempfung haengen nicht)");
    }
    {
        // Erwarteter Pegel (Grundlage der Stille-Erkennung) passt zum gemessenen
        AudioSystem a;
        a.initShared(base);
        quietRoom(a);
        a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
        run(a, rate * 2);
        const AudioStats st = a.stats();
        std::printf("Pegel: gemessen %.1f dBFS, erwartet %.1f dBFS\n", dbOf(st.outRms), dbOf(st.expectedRms));
        check(std::fabs(dbOf(st.outRms) - dbOf(st.expectedRms)) < 6.0, "erwarteter und gemessener Pegel liegen nah beieinander");
    }

    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
