#include "render/gpu_timer.hpp"

#include <format>

namespace lim::gfx {

void GpuTimer::init(Gpu& gpu) {
    D3D11_QUERY_DESC dj{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    D3D11_QUERY_DESC ts{D3D11_QUERY_TIMESTAMP, 0};
    ok_ = true;
    for (auto& f : frames_) {
        ok_ = ok_ && SUCCEEDED(gpu.dev()->CreateQuery(&dj, f.disjoint.put()));
        for (auto& q : f.ts) ok_ = ok_ && SUCCEEDED(gpu.dev()->CreateQuery(&ts, q.put()));
    }
}

bool GpuTimer::begin(Gpu& gpu) {
    if (!ok_ || active_) return false;
    collect(gpu);
    Frame& f = frames_[(size_t)cur_];
    if (f.pending) return false;  // GPU haengt hinterher: dieses Bild nicht messen
    gpu.ctx()->Begin(f.disjoint.get());
    gpu.ctx()->End(f.ts[0].get());
    f.marks = 0;
    active_ = true;
    return true;
}

void GpuTimer::mark(Gpu& gpu, const char* name) {
    if (!active_) return;
    Frame& f = frames_[(size_t)cur_];
    if (f.marks >= kMaxMarks) return;
    f.names[(size_t)f.marks] = name;
    gpu.ctx()->End(f.ts[(size_t)f.marks + 1].get());
    ++f.marks;
}

void GpuTimer::end(Gpu& gpu) {
    if (!active_) return;
    Frame& f = frames_[(size_t)cur_];
    gpu.ctx()->End(f.disjoint.get());
    f.pending = true;
    active_ = false;
    cur_ = (cur_ + 1) % kFrames;
}

void GpuTimer::collect(Gpu& gpu) {
    // aeltestes ausstehendes Bild abholen, ohne zu warten
    for (int k = 1; k <= kFrames; ++k) {
        Frame& f = frames_[(size_t)((cur_ + k) % kFrames)];
        if (!f.pending) continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
        if (gpu.ctx()->GetData(f.disjoint.get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        std::array<UINT64, kMaxMarks + 1> t{};
        bool all = true;
        for (int i = 0; i <= f.marks && all; ++i)
            all = gpu.ctx()->GetData(f.ts[(size_t)i].get(), &t[(size_t)i], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
        if (!all) continue;
        f.pending = false;
        if (dj.Disjoint || dj.Frequency == 0) continue;
        double toMs = 1000.0 / (double)dj.Frequency;
        float total = 0.0f;
        // Passes, die in diesem Bild nicht liefen (z. B. abgeschaltet), klingen aus
        for (auto& r : results_) {
            bool seen = false;
            for (int i = 0; i < f.marks; ++i) seen = seen || r.name == f.names[(size_t)i];
            if (!seen) r.ms *= 0.9f;
        }
        for (int i = 0; i < f.marks; ++i) {
            float ms = (float)((double)(t[(size_t)i + 1] - t[(size_t)i]) * toMs);
            total += ms;
            std::string name = f.names[(size_t)i];
            Entry* e = nullptr;
            for (auto& r : results_)
                if (r.name == name) e = &r;
            if (!e) {
                results_.push_back({name, ms});
                continue;
            }
            e->ms += (ms - e->ms) * 0.1f;
        }
        total_ += (total - total_) * 0.1f;
    }
}

std::string GpuTimer::summary() const {
    std::string s = std::format("GPU {:.2f} ms:", total_);
    for (const auto& r : results_)
        if (r.ms >= 0.005f) s += std::format("  {} {:.2f}", r.name, r.ms);
    return s;
}

}  // namespace lim::gfx
