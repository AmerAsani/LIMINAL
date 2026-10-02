#include "app/settings.hpp"

#include <algorithm>

#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim {

std::string Settings::path() { return paths::join(paths::userDataDir(), "settings_v5.json"); }

void Settings::applyQualityPreset(int q) {
    quality = q;
    auto& r = render;
    switch (q) {
        case 0:  // niedrig: schwache Grafikchips
            r.renderScale = 0.75f, r.shadows = 0, r.ssao = 0, r.taa = true, r.bloom = true, r.textureSize = 512,
            r.anisotropy = 2, r.ssr = false, r.volumetric = 0.0f;
            viewDistance = 3;
            break;
        case 1:
            r.renderScale = 1.0f, r.shadows = 1, r.ssao = 0, r.taa = true, r.bloom = true, r.textureSize = 512,
            r.anisotropy = 4, r.ssr = false, r.volumetric = 0.8f;
            viewDistance = 4;
            break;
        case 2:
            r.renderScale = 1.0f, r.shadows = 2, r.ssao = 1, r.taa = true, r.bloom = true, r.textureSize = 1024,
            r.anisotropy = 8, r.ssr = true, r.volumetric = 1.0f;
            viewDistance = 4;
            break;
        case 3:
            r.renderScale = 1.25f, r.shadows = 2, r.ssao = 1, r.taa = true, r.bloom = true, r.textureSize = 2048,
            r.anisotropy = 16, r.ssr = true, r.volumetric = 1.0f;
            viewDistance = 5;
            break;
        default: break;
    }
}

Json Settings::toJson() const {
    Json j = Json::object();
    j.set("mouse_enabled", mouseEnabled);
    j.set("mouse_sensitivity", (double)mouseSensitivity);
    j.set("mouse_x", (double)mouseX);
    j.set("mouse_y", (double)mouseY);
    j.set("invert_x", invertX);
    j.set("invert_y", invertY);
    j.set("mouse_smoothing", (double)mouseSmoothing);
    j.set("pad_sensitivity", (double)padSensitivity);
    j.set("bob", bob);
    j.set("walk_speed", (double)walkSpeed);
    j.set("fov", (double)fov);
    j.set("fullscreen", fullscreen);
    j.set("max_fps", maxFps);
    j.set("quality", quality);
    j.set("view_distance", viewDistance);
    j.set("render_scale", (double)render.renderScale);
    j.set("shadows", render.shadows);
    j.set("ssao", render.ssao);
    j.set("taa", render.taa);
    j.set("bloom", render.bloom);
    j.set("ssr", render.ssr);
    j.set("volumetric", (double)render.volumetric);
    j.set("texture_size", render.textureSize);
    j.set("anisotropy", render.anisotropy);
    j.set("film_grain", (double)render.filmGrain);
    j.set("vignette", (double)render.vignette);
    j.set("chromatic", (double)render.chromatic);
    j.set("display_mode", render.displayMode);
    j.set("vsync", render.vsync);
    j.set("brightness", (double)render.exposureBias);
    j.set("ambient_strength", (double)render.ambientStrength);
    j.set("light_intensity", (double)render.lightIntensity);
    j.set("fog_brightness", (double)render.fogBrightness);
    j.set("fog_density", (double)render.fogDensity);
    j.set("ao_direct", (double)render.aoDirect);
    j.set("exposure_auto", (double)render.exposureAuto);
    j.set("exposure_base", (double)render.exposureBase);
    j.set("bloom_strength", (double)render.bloomStrength);
    j.set("master_volume", (double)masterVolume);
    j.set("effects_volume", (double)effectsVolume);
    j.set("ambience_volume", (double)ambienceVolume);
    return j;
}

void Settings::fromJson(const Json& j) {
    auto b = [&](const char* k, bool& v) { v = j[k].asBool(v); };
    auto f = [&](const char* k, float& v, float lo, float hi) { v = std::clamp(j[k].asFloat(v), lo, hi); };
    auto i = [&](const char* k, int& v, int lo, int hi) { v = std::clamp((int)j[k].asInt(v), lo, hi); };
    b("mouse_enabled", mouseEnabled);
    f("mouse_sensitivity", mouseSensitivity, 0.1f, 5.0f);
    f("mouse_x", mouseX, 0.2f, 3.0f);
    f("mouse_y", mouseY, 0.2f, 3.0f);
    b("invert_x", invertX);
    b("invert_y", invertY);
    f("mouse_smoothing", mouseSmoothing, 0.0f, 0.9f);
    f("pad_sensitivity", padSensitivity, 0.2f, 3.0f);
    b("bob", bob);
    f("walk_speed", walkSpeed, 1.0f, 8.0f);
    f("fov", fov, 0.0f, 120.0f);
    b("fullscreen", fullscreen);
    i("max_fps", maxFps, 0, 360);
    i("quality", quality, 0, 4);
    i("view_distance", viewDistance, 2, 6);
    f("render_scale", render.renderScale, 0.5f, 1.5f);
    i("shadows", render.shadows, 0, 2);
    i("ssao", render.ssao, 0, 1);
    b("taa", render.taa);
    b("bloom", render.bloom);
    b("ssr", render.ssr);
    f("volumetric", render.volumetric, 0.0f, 3.0f);
    i("texture_size", render.textureSize, 256, 2048);
    i("anisotropy", render.anisotropy, 1, 16);
    f("film_grain", render.filmGrain, 0.0f, 1.0f);
    f("vignette", render.vignette, 0.0f, 1.0f);
    f("chromatic", render.chromatic, 0.0f, 1.0f);
    i("display_mode", render.displayMode, 0, 3);
    b("vsync", render.vsync);
    f("brightness", render.exposureBias, -2.0f, 2.0f);
    f("ambient_strength", render.ambientStrength, 0.0f, 4.0f);
    f("light_intensity", render.lightIntensity, 0.0f, 100.0f);
    f("fog_brightness", render.fogBrightness, 0.0f, 4.0f);
    f("fog_density", render.fogDensity, 0.0f, 3.0f);
    f("ao_direct", render.aoDirect, 0.0f, 1.0f);
    f("exposure_auto", render.exposureAuto, 0.0f, 1.0f);
    f("exposure_base", render.exposureBase, 0.05f, 20.0f);
    i("debug_view", render.debugView, 0, 9);
    f("bloom_strength", render.bloomStrength, 0.0f, 0.5f);
    f("master_volume", masterVolume, 0.0f, 1.0f);
    f("effects_volume", effectsVolume, 0.0f, 1.0f);
    f("ambience_volume", ambienceVolume, 0.0f, 1.0f);
}

void Settings::migrateFromV4(const Json& j) {
    // Uebernommen wird, was es in V5 noch gibt (Maus, Bewegung, Sichtfeld)
    Json m = Json::object();
    for (const char* k : {"mouse_enabled", "mouse_sensitivity", "mouse_x", "mouse_y", "invert_x", "invert_y",
                          "mouse_smoothing", "bob", "walk_speed", "fov", "fullscreen"})
        if (j.has(k)) m.set(k, j[k]);
    if (j["render_mode"].asString("") == "ascii") m.set("display_mode", 2);
    if (j["render_mode"].asString("") == "mono") m.set("display_mode", 3);
    fromJson(m);
    log::info("Einstellungen aus V4 uebernommen");
}

bool Settings::load() {
    applyQualityPreset(quality);
    if (auto j = loadJsonFile(path())) {
        fromJson(*j);
        return true;
    }
    if (auto j = loadJsonFile(paths::join(paths::userDataDir(), "settings_v3.json"))) migrateFromV4(*j);
    return false;
}

bool Settings::save() const { return paths::writeFileAtomic(path(), toJson().dump(true), false); }

void Settings::resetMouse() {
    Settings d;
    mouseEnabled = d.mouseEnabled;
    mouseSensitivity = d.mouseSensitivity;
    mouseX = d.mouseX;
    mouseY = d.mouseY;
    invertX = d.invertX;
    invertY = d.invertY;
    mouseSmoothing = d.mouseSmoothing;
    padSensitivity = d.padSensitivity;
}

}  // namespace lim
