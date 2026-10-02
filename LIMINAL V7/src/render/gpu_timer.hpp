// V6: GPU-Zeitmessung je Render-Pass (D3D11-Zeitstempel).
//
// Jedes Bild setzt Marken zwischen den Passes. Die Ergebnisse werden drei Bilder
// spaeter abgeholt (ohne die CPU auf die GPU warten zu lassen) und weich gemittelt.
// Grundlage fuer Debug-Anzeige (F3) und Benchmark: Qualitaet gezielt dort einsetzen,
// wo Budget ist, und teure Passes erkennen.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "render/gpu.hpp"

namespace lim::gfx {

class GpuTimer {
public:
    static constexpr int kMaxMarks = 16;
    static constexpr int kFrames = 4;

    void init(Gpu& gpu);
    // Bildbeginn; liefert false, wenn in diesem Bild bereits gemessen wird (z. B. Vorschaubild)
    bool begin(Gpu& gpu);
    void mark(Gpu& gpu, const char* name);  // Zeit seit der vorigen Marke gehoert zu "name"
    void end(Gpu& gpu);
    bool active() const { return active_; }

    struct Entry {
        std::string name;
        float ms = 0.0f;  // geglaettet
    };
    const std::vector<Entry>& results() const { return results_; }
    float totalMs() const { return total_; }
    std::string summary() const;

private:
    void collect(Gpu& gpu);
    struct Frame {
        Com<ID3D11Query> disjoint;
        std::array<Com<ID3D11Query>, kMaxMarks + 1> ts;
        std::array<const char*, kMaxMarks> names{};
        int marks = 0;
        bool pending = false;
    };
    std::array<Frame, kFrames> frames_;
    int cur_ = 0;
    bool active_ = false, ok_ = false;
    std::vector<Entry> results_;
    float total_ = 0.0f;
};

}  // namespace lim::gfx
