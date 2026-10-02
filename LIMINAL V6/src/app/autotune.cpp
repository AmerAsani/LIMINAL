#include "app/autotune.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "render/renderer.hpp"

namespace lim::app {

namespace {
// Aufwand der Stufen relativ zu "Hoch" bei gleicher Ausgabeaufloesung (gemessen ueber viele Szenen;
// Ultra rechnet intern mit 125 % und den teuersten Verfahren)
constexpr float kCost[4] = {0.42f, 0.78f, 1.0f, 1.8f};
const char* kNames[4] = {"Niedrig", "Mittel", "Hoch", "Ultra"};
}  // namespace

std::string AutoTune::hardwareKey(const gfx::GpuInfo& gpu, int outW, int outH) {
    return std::format("{}|{:x}:{:x}|{}x{}", gpu.adapter, gpu.vendorId, gpu.deviceId, outW, outH);
}

int AutoTune::estimateFromHardware(const gfx::GpuInfo& gpu, int outW, int outH) {
    if (gpu.warp) return 0;
    const double mpix = (double)outW * outH / 1e6;
    const double vramGB = (double)gpu.dedicatedVram / (1024.0 * 1024.0 * 1024.0);
    int q;
    if (gpu.integrated) q = 1;            // integrierte Grafik: Mittel, die Messung entscheidet
    else if (vramGB >= 6.0) q = 2;        // aktuelle Grafikkarte
    else if (vramGB >= 3.0) q = 2;
    else if (vramGB >= 1.5) q = 1;
    else q = 0;
    if (mpix > 4.5 && q > 1) q = 1;       // 4K: zuerst vorsichtig, die Messung hebt ggf. an
    if (gpu.featureLevel < D3D_FEATURE_LEVEL_11_0) q = 0;
    return q;
}

int AutoTune::chooseFromMeasurement(float gpuMs, int measured, float targetFps, float* scaleOut) {
    measured = std::clamp(measured, 0, 3);
    const float budget = 1000.0f / std::max(targetFps, 20.0f);
    const float high = gpuMs / kCost[measured];  // geschaetzte Zeit fuer "Hoch"
    if (scaleOut) *scaleOut = 1.0f;
    // Ultra nur mit grosser Reserve, sonst die hoechste Stufe mit ~20 % Luft fuer volle Szenen
    if (high * kCost[3] <= budget * 0.62f) return 3;
    for (int q = 2; q >= 0; --q)
        if (high * kCost[q] <= budget * 0.8f) return q;
    // selbst Niedrig zu teuer: interne Aufloesung so weit senken, dass es passt (Pixel ~ Aufwand)
    if (scaleOut) *scaleOut = std::clamp(std::sqrt(budget * 0.8f / std::max(high * kCost[0], 0.1f)) * 0.75f, 0.5f, 0.75f);
    return 0;
}

bool AutoTune::prepare(Settings& s, const gfx::GpuInfo& gpu, int outW, int outH) {
    key_ = hardwareKey(gpu, outW, outH);
    if (!s.autoQuality) return false;
    if (s.calibratedFor == key_) return false;  // schon fuer diese Hardware gemessen
    int q = estimateFromHardware(gpu, outW, outH);
    s.applyQualityPreset(q);
    log::info("Grafik-Automatik: Schaetzung aus der Hardware -> {} ({})", kNames[q], key_);
    requestCalibration();
    return true;
}

std::string AutoTune::update(double dt, Settings& s, gfx::Renderer& r, bool sceneVisible, float gpuMs, float frameMs) {
    std::string msg;
    if (pending_ && sceneVisible && r.texturesReady()) {
        ++frames_;
        // Einschwingen (Shader, Streaming, Belichtung), dann mitteln
        if (frames_ > 45) {
            sum_ += gpuMs > 0.05f ? gpuMs : frameMs;  // ohne GPU-Zeitstempel: Bildzeit
            ++samples_;
        }
        if (samples_ >= 90) {
            pending_ = false;
            const float ms = (float)(sum_ / samples_);
            const float target = s.maxFps > 0 ? (float)std::min(s.maxFps, 144) : 60.0f;
            int measured = std::clamp(s.quality, 0, 3);
            float scale = 1.0f;
            int q = chooseFromMeasurement(ms, measured, target, &scale);
            s.applyQualityPreset(q);
            if (scale < 0.999f) {
                s.render.renderScale = scale;
                s.dynamicResolution = true;
            }
            s.calibratedFor = key_;
            s.autoQuality = true;
            r.applySettings(s.render);
            log::info("Grafik-Automatik: {:.1f} ms bei {} -> {} (Ziel {:.0f} FPS{})", ms, kNames[measured], kNames[q], target,
                      scale < 0.999f ? std::format(", interne Aufloesung {:.0f} %", scale * 100.0f) : std::string());
            msg = std::format("Grafik automatisch eingestellt: {}", kNames[q]);
        }
    }
    if (s.dynamicResolution) dynamicResolution(dt, s, r, gpuMs, frameMs);
    else if (r.dynamicScale() < 0.999f) r.setDynamicScale(1.0f);
    return msg;
}

void AutoTune::dynamicResolution(double dt, Settings& s, gfx::Renderer& r, float gpuMs, float frameMs) {
    float ms = gpuMs > 0.05f ? gpuMs : frameMs;
    if (ms <= 0.0f) return;
    ema_ = ema_ <= 0.0f ? ms : ema_ + (ms - ema_) * (float)std::min(1.0, dt * 3.0);
    const float target = s.maxFps > 0 ? (float)std::min(s.maxFps, 144) : 60.0f;
    const float budget = 1000.0f / target;
    sinceChange_ += dt;
    overFor_ = ema_ > budget * 1.05f ? overFor_ + dt : 0.0;
    underFor_ = ema_ < budget * 0.7f ? underFor_ + dt : 0.0;
    float cur = r.dynamicScale();
    if (overFor_ > 1.0 && sinceChange_ > 1.5 && cur > 0.5f) {
        // Pixelzahl ~ Aufwand: Schritt so, dass das Budget ungefaehr passt (hoechstens 15 %)
        float want = cur * std::sqrt(budget * 0.9f / ema_);
        r.setDynamicScale(std::max(0.5f, std::max(want, cur - 0.15f)));
        sinceChange_ = overFor_ = 0.0;
        ema_ = 0.0f;
    } else if (underFor_ > 4.0 && sinceChange_ > 4.0 && cur < 1.0f) {
        r.setDynamicScale(std::min(1.0f, cur + 0.05f));
        sinceChange_ = underFor_ = 0.0;
        ema_ = 0.0f;
    }
}

}  // namespace lim::app
