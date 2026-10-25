// V11: schreibt alle erzeugten Klaenge als WAV-Dateien (16 Bit, mono) und eine Messliste.
//
//   sound_export <ordner> [filter]
//
// Je Klang: Dauer, Lautheit (LUFS, K-gewichtet), Spitze (dBFS), Schwerpunkt des Spektrums (Hz),
// bei Schleifen der Sprung an der Naht. So lassen sich die Klaenge anhoeren und vergleichen.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "audio/dsp.hpp"
#include "core/jobs.hpp"

using namespace lim;
using namespace lim::audio;

static void writeWav(const std::string& path, const Sound& s) {
    std::ofstream f(path, std::ios::binary);
    const unsigned rate = (unsigned)s.rate, n = (unsigned)s.samples.size();
    auto u32 = [&](unsigned v) { f.write((const char*)&v, 4); };
    auto u16 = [&](unsigned short v) { f.write((const char*)&v, 2); };
    f.write("RIFF", 4);
    u32(36 + n * 2);
    f.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    f.write("data", 4);
    u32(n * 2);
    for (float v : s.samples) {
        const float c = std::max(-1.0f, std::min(1.0f, v));
        const short q = (short)std::lround(c * 32767.0f);
        f.write((const char*)&q, 2);
    }
}

// Schwerpunkt des Spektrums (Hz), gemittelt ueber Bloecke mit Energie
static double centroid(const Sound& s) {
    const size_t N = 2048;
    if (s.samples.size() < N) return 0;
    double num = 0, den = 0;
    for (size_t off = 0; off + N <= s.samples.size(); off += N) {
        std::vector<float> re(N), im(N, 0.0f);
        for (size_t i = 0; i < N; ++i) re[i] = s.samples[off + i] * (0.5f - 0.5f * std::cos(6.2831853f * (float)i / (float)N));
        dsp::fft(re, im, false);
        for (size_t k = 1; k < N / 2; ++k) {
            const double m = std::sqrt((double)re[k] * re[k] + (double)im[k] * im[k]);
            num += m * (double)k * s.rate / N;
            den += m;
        }
    }
    return den > 0 ? num / den : 0;
}

// Sprung an der Schleifennaht im Verhaeltnis zu typischen Nachbarschritten
static bool seamOk(const Sound& s) {
    double typical = 0;
    for (size_t i = 1; i < s.samples.size(); ++i) typical = std::max(typical, (double)std::fabs(s.samples[i] - s.samples[i - 1]));
    return std::fabs(s.samples.front() - s.samples.back()) <= typical * 1.05 + 1e-4;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::printf("sound_export <ordner> [filter]\n");
        return 2;
    }
    std::filesystem::path dir(argv[1]);
    std::filesystem::create_directories(dir);
    std::wstring wf = argc > 2 ? argv[2] : L"";
    std::string filter(wf.begin(), wf.end());
    JobSystem jobs;
    AudioSystem a;
    a.initOffline(jobs);
    FILE* list = _wfopen((dir / "messung.txt").wstring().c_str(), L"w");
    int count = 0;
    for (const auto& name : a.names()) {
        if (!filter.empty() && name.find(filter) == std::string::npos) continue;
        const Sound* s = a.sound(name);
        if (!s || s->samples.empty()) continue;
        writeWav((dir / (name + ".wav")).string(), *s);
        const float lufs = dsp::loudnessDb(s->samples, (float)s->rate);
        const float pk = 20.0f * std::log10(std::max(1e-9f, dsp::peakAbs(s->samples)));
        char line[256];
        std::snprintf(line, sizeof line, "%-24s %6.2f s %6.1f LUFS %6.1f dBFS %6.0f Hz %s%s\n", name.c_str(),
                      (double)s->samples.size() / s->rate, lufs, pk, centroid(*s), s->loop ? "Schleife" : "", s->loop ? (seamOk(*s) ? " (Naht ok)" : " (NAHT!)") : "");
        std::fputs(line, stdout);
        if (list) std::fputs(line, list);
        ++count;
    }
    if (list) std::fclose(list);
    std::printf("%d Klaenge -> %ls\n", count, dir.wstring().c_str());
    return 0;
}
