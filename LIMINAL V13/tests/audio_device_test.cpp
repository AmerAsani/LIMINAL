// V13.3 (AUDIO_BUG.md): Audio mit dem echten Geraet (WASAPI) - Start-Stresstest, Geraetewechsel-Simulation,
// Stille-Erkennung am Master-Ausgang. Spielt leise Klaenge ueber das Standardgeraet.
//
//   audio_device_test [starts] [wiederholungen]      (Standard: 200 Starts, 20 Wiederholungen je Fehlerart)
//
// Was echt ist und was simuliert (ausfuehrlich in AUDIO_BUG.md):
//   echt:       Oeffnen/Schliessen des WASAPI-Stroms, Neuoeffnen nach jedem Fehler, Abtastratenwechsel des Stroms
//               (44,1 kHz <-> 48 kHz, Windows rechnet ins Geraeteformat um), Mischen in Echtzeit, Pegel am Master-Ausgang
//   simuliert:  der Ausloeser - Benachrichtigungen (OnDefaultDeviceChanged, OnDeviceStateChanged, OnSessionDisconnected)
//               werden ueber dieselben Rueckrufe aufgerufen, die Windows aufruft; Fehlercodes (AUDCLNT_E_DEVICE_INVALIDATED,
//               AUDCLNT_E_DEVICE_IN_USE, E_NOTFOUND, AUDCLNT_E_UNSUPPORTED_FORMAT) werden an der Stelle eingesetzt, an der
//               WASAPI sie liefert; ein stummer Mischerzustand und NaN werden gestellt.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "audio/audio.hpp"
#include "core/jobs.hpp"
#include "core/log.hpp"

using namespace lim;
using namespace lim::audio;
using Clock = std::chrono::steady_clock;

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}
static double dbOf(double x) { return x > 1e-9 ? 20.0 * std::log10(x) : -180.0; }
static double msSince(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }

// wartet, bis cond() gilt (oder timeoutMs), ruft dabei den Watchdog wie das Spiel je Bild (~60 Hz)
template <class F>
static double waitFor(AudioSystem& a, F cond, double timeoutMs) {
    const auto t0 = Clock::now();
    while (msSince(t0) < timeoutMs) {
        a.watchdog();
        if (cond()) return msSince(t0);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return -1.0;
}

static bool audible(AudioSystem& a) {
    const AudioStats s = a.stats();
    return s.deviceOpen && dbOf(s.outRmsFast) > -50.0;
}

struct Pct {
    std::vector<double> v;
    double at(double p) {
        if (v.empty()) return -1;
        std::sort(v.begin(), v.end());
        return v[std::min(v.size() - 1, (size_t)(p * (double)(v.size() - 1) + 0.5))];
    }
};

int main(int argc, char** argv) {
    const int starts = argc > 1 ? std::max(1, std::atoi(argv[1])) : 200;
    const int reps = argc > 2 ? std::max(1, std::atoi(argv[2])) : 20;
    // Protokoll des Audiosystems (Geraet, Fehlercodes, Neustarts) in eine Datei neben dem Test
    log::init(argc > 3 ? argv[3] : "audio_device_test.log");
    JobSystem jobs(4);
    AudioSystem bank;
    bank.initOffline(jobs);
    std::printf("Klangbank: %zu Klaenge\n", bank.names().size());
    if (argc > 4 && std::string(argv[4]) == "debug") {  // Einzelfall mit Verlauf (Fehlersuche)
        AudioSystem a;
        a.initShared(bank);
        a.setVolumes(0.25f, 1.0f, 1.0f);
        a.startDevice();
        a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
        waitFor(a, [&] { return audible(a); }, 3000.0);
        a.injectFault(argc > 5 ? (Fault)std::atoi(argv[5]) : Fault::MuteOutput, argc > 6 ? std::atoi(argv[6]) : 0);
        for (int i = 0; i < 20; ++i) {
            waitFor(a, [] { return false; }, 200.0);
            const AudioStats s = a.stats();
            std::printf("%4d ms: offen %d, Ausgang %.1f dBFS (Sekunde %.1f), erwartet %.1f dBFS, Alarme %llu, NaN %llu, Neustarts %u, Grund '%s'\n", i * 200,
                        (int)s.deviceOpen, dbOf(s.outRmsFast), dbOf(s.outRms), dbOf(s.expectedRms), s.silenceAlarms, s.nonFinite, s.restarts,
                        s.lastReason.c_str());
        }
        return 0;
    }

    // --- 1. Start-Stresstest: Geraet oeffnen, Klang am Master-Ausgang, schliessen -------------------------------
    {
        Pct open, sound;
        int failOpen = 0, failSound = 0;
        unsigned long long underruns = 0, restarts = 0;
        for (int i = 0; i < starts; ++i) {
            AudioSystem a;
            a.initShared(bank);
            a.setVolumes(0.25f, 1.0f, 1.0f);
            const auto t0 = Clock::now();
            a.startDevice();
            // wie das Spiel: Klaenge werden schon angenommen, waehrend das Geraet noch oeffnet
            a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
            const double to = waitFor(a, [&] { return a.available(); }, 3000.0);
            const double ts = waitFor(a, [&] { return audible(a); }, 3000.0);
            if (to < 0) ++failOpen;
            else open.v.push_back(to);
            if (ts < 0) ++failSound;
            else sound.v.push_back(msSince(t0));
            const AudioStats st = a.stats();
            underruns += st.underruns;
            restarts += st.restarts;
            if (i == 0) std::printf("Geraet: '%s', %d Hz, %d Kanaele, %d Bit; Strom %d Hz, Puffer %u Bilder (%.1f ms), Periode %.1f ms\n",
                                    st.device.c_str(), st.deviceRate, st.deviceChannels, st.deviceBits, st.streamRate, st.bufferFrames,
                                    1000.0 * st.bufferFrames / std::max(1, st.streamRate), st.periodMs);
        }
        std::printf("Start-Stresstest: %d Starts, Geraet offen %d, Klang am Ausgang %d; bis offen Median %.0f ms, max %.0f ms; bis zum Klang Median "
                    "%.0f ms, 95 %% %.0f ms, max %.0f ms; Unterlaeufe %llu, Neustarts %llu\n",
                    starts, starts - failOpen, starts - failSound, open.at(0.5), open.at(1.0), sound.at(0.5), sound.at(0.95), sound.at(1.0), underruns,
                    restarts);
        check(failOpen == 0, "jeder Start oeffnet das Geraet");
        check(failSound == 0, "jeder Start klingt (Pegel am Master-Ausgang ueber -50 dBFS)");
        check(restarts == 0, "kein Neustart ohne Anlass");
    }

    // --- 2. Geraetewechsel und Fehlerpfade: Erholung ohne Zutun ------------------------------------------------
    {
        AudioSystem a;
        a.initShared(bank);
        a.setVolumes(0.25f, 1.0f, 1.0f);
        a.startDevice();
        const int loop = a.startLoop("bed_l1_space", 0.8f, Bus::Ambience);
        check(waitFor(a, [&] { return audible(a); }, 3000.0) >= 0, "Geraet offen und Klang vor den Fehlertests");
        struct Case {
            const char* name;
            Fault f;
            int param;
        };
        const Case cases[] = {
            {"Geraet ungueltig (AUDCLNT_E_DEVICE_INVALIDATED)", Fault::DeviceInvalidated, 0},
            {"Standardgeraet gewechselt (Kopfhoerer rein/raus)", Fault::DefaultChanged, 0},
            {"Abtastrate gewechselt -> 44100 Hz", Fault::FormatChanged, 44100},
            {"Abtastrate gewechselt -> 48000 Hz", Fault::FormatChanged, 48000},
            {"exklusiver Modus eines anderen Programms (1,5 s)", Fault::ExclusiveOverride, 1500},
            {"Geraet abgezogen (2 s kein Geraet)", Fault::DeviceRemoved, 2000},
            {"Treiber meldet sich nicht (1,5 s)", Fault::StallEvents, 1500},
            {"Master-Ausgang stumm (haengender Zustand)", Fault::MuteOutput, 0},
            {"NaN im Filterzustand", Fault::NanOutput, 0},
            {"Format abgelehnt (UNSUPPORTED_FORMAT) -> Mischformat", Fault::InitFails, 1},
        };
        for (const Case& c : cases) {
            Pct rec;
            int failed = 0, loopLost = 0;
            for (int r = 0; r < reps; ++r) {
                const AudioStats before = a.stats();
                const auto t0 = Clock::now();
                a.injectFault(c.f, c.param);
                if (c.f == Fault::InitFails) a.requestRestart("Test");
                // erholt: (je nach Art) neu geoeffnet bzw. Alarm/NaN gezaehlt, Geraet offen, Klang am Ausgang
                const bool windowed = c.f == Fault::StallEvents || c.f == Fault::ExclusiveOverride || c.f == Fault::DeviceRemoved;
                unsigned long long lastFrames = before.frames;
                auto done = [&] {
                    const AudioStats s = a.stats();
                    // es wird wirklich gemischt (der Pegel allein koennte vom letzten Block vor einem Stillstand stammen)
                    const bool flowing = s.frames != lastFrames;
                    lastFrames = s.frames;
                    if (!flowing || (windowed && msSince(t0) < c.param)) return false;
                    bool event = s.restarts > before.restarts;
                    if (c.f == Fault::NanOutput) event = s.nonFinite > before.nonFinite;
                    if (c.f == Fault::MuteOutput) event = s.silenceAlarms > before.silenceAlarms;  // nur die Stille-Erkennung zaehlt
                    return event && s.deviceOpen && dbOf(s.outRmsFast) > -50.0;
                };
                // vor der Pruefung kurz warten, damit der alte Pegel nicht als "erholt" zaehlt
                std::this_thread::sleep_for(std::chrono::milliseconds(c.f == Fault::NanOutput ? 30 : 150));
                const double t = waitFor(a, done, 10000.0);
                if (t < 0) {
                    ++failed;
                    const AudioStats s = a.stats();
                    std::printf("  %s: keine Erholung (offen %d, %.1f dBFS, Neustarts %u)\n", c.name, (int)s.deviceOpen, dbOf(s.outRmsFast), s.restarts);
                } else {
                    rec.v.push_back(msSince(t0));
                }
                if (!a.loopActive(loop)) ++loopLost;
                if (c.f == Fault::FormatChanged) check(a.stats().streamRate == c.param && a.mixRate() == c.param, std::string(c.name) + ": Strom und Mischer in der neuen Rate");
                // Fehler mit Zeitfenster: erst nach dessen Ende weiter (sonst zaehlt ein Nachlaeufer fuer den naechsten Fall)
                if (c.f == Fault::StallEvents || c.f == Fault::ExclusiveOverride || c.f == Fault::DeviceRemoved) {
                    const double rest = c.param + 300.0 - msSince(t0);
                    if (rest > 0) waitFor(a, [] { return false; }, rest);
                    waitFor(a, [&] { return audible(a); }, 5000.0);
                }
            }
            const AudioStats s = a.stats();
            std::printf("%-58s %2d/%d erholt, Dauer bis Klang Median %5.0f ms, max %5.0f ms | Strom %d Hz, geoeffnet %u, Neustarts %u, Stille-Alarme %llu, NaN %llu\n",
                        c.name, reps - failed, reps, rec.at(0.5), rec.at(1.0), s.streamRate, s.opens, s.restarts, s.silenceAlarms, s.nonFinite);
            check(failed == 0, std::string(c.name) + ": jede Wiederholung erholt sich ohne Zutun");
            check(loopLost == 0, std::string(c.name) + ": laufende Dauerklaenge bleiben erhalten");
        }
        const AudioStats s = a.stats();
        std::printf("nach allen Fehlertests: %d Stimmen, haengend %d, Unterlaeufe %llu, Zeitueberschreitungen %llu, Mischen max %.2f ms\n", s.voices, s.stuck,
                    s.underruns, s.timeouts, s.mixMsMax);
        check(s.stuck == 0, "keine haengenden Stimmen");
    }

    // --- 3. Kein Fehlalarm im Normalbetrieb: 20 s mit wechselnden Klaengen, Master 0, Pause ------------------
    {
        AudioSystem a;
        a.initShared(bank);
        a.setVolumes(0.25f, 1.0f, 1.0f);
        a.startDevice();
        waitFor(a, [&] { return a.available(); }, 3000.0);
        const int bed = a.startLoop("bed_l0_room", 0.6f, Bus::Ambience);
        const auto t0 = Clock::now();
        int k = 0;
        while (msSince(t0) < 20000.0) {
            a.watchdog();
            if (++k % 20 == 0) a.play("step_concrete_0", 0.5f);
            if (k == 300) a.setVolumes(0.0f, 1.0f, 1.0f);  // Spieler dreht Master auf 0
            if (k == 500) a.setVolumes(0.25f, 1.0f, 1.0f), a.setPaused(true);
            if (k == 700) a.setPaused(false), a.stopLoop(bed);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        const AudioStats s = a.stats();
        std::printf("Normalbetrieb 20 s: Stille-Alarme %llu, Neustarts %u, Unterlaeufe %llu, Mischen max %.2f ms\n", s.silenceAlarms, s.restarts, s.underruns,
                    s.mixMsMax);
        check(s.silenceAlarms == 0 && s.restarts == 0, "kein Fehlalarm, kein Neustart im Normalbetrieb");
    }

    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
