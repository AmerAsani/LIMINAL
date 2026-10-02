#include "app/settings.hpp"

#include <algorithm>
#include <cmath>

#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim {

// V6 hat eine eigene Datei: V5 bleibt mit seinen Einstellungen unangetastet nebenher spielbar.
std::string Settings::path() { return paths::join(paths::userDataDir(), "settings_v6.json"); }

// V6: Stufen nach Aufwand, nicht nach einer bestimmten Grafikkarte. Welche Stufe passt, misst
// das Spiel beim ersten Start (app/autotune.cpp); Ultra darf teuer sein.
void Settings::applyQualityPreset(int q) {
    quality = q;
    auto& r = render;
    switch (q) {
        case 0:  // niedrig: schwache und aeltere Grafikchips (Nachschaerfen gleicht die geringere Aufloesung aus)
            r.renderScale = 0.75f, r.shadows = 0, r.ssao = 0, r.taa = true, r.bloom = true, r.textureSize = 512,
            r.anisotropy = 4, r.ssr = false, r.volumetric = 0.0f, r.dust = true, r.particles = 0, r.sharpen = 0.5f;
            r.indirect = false, r.volumetricQuality = 1, r.contactShadows = false, r.parallax = false, r.lightQuality = 0;
            viewDistance = 3;
            break;
        case 1:  // mittel: normale Gaming-PCs und gute integrierte Grafik
            r.renderScale = 1.0f, r.shadows = 1, r.ssao = 1, r.taa = true, r.bloom = true, r.textureSize = 1024,
            r.anisotropy = 8, r.ssr = false, r.volumetric = 1.0f, r.dust = true, r.particles = 1, r.sharpen = 0.35f;
            r.indirect = false, r.volumetricQuality = 1, r.contactShadows = false, r.parallax = true, r.lightQuality = 1;
            viewDistance = 4;
            break;
        case 2:  // hoch: alle Verfahren an, AO/Volumetrie in reduzierter Aufloesung
            r.renderScale = 1.0f, r.shadows = 2, r.ssao = 1, r.taa = true, r.bloom = true, r.textureSize = 1024,
            r.anisotropy = 16, r.ssr = true, r.volumetric = 1.0f, r.dust = true, r.particles = 2, r.sharpen = 0.35f;
            r.indirect = true, r.volumetricQuality = 2, r.contactShadows = true, r.parallax = true, r.lightQuality = 2;
            viewDistance = 5;
            break;
        case 3:  // ultra: hoechste Stufe jedes Verfahrens, Supersampling - fuer leistungsstarke PCs
            r.renderScale = 1.25f, r.shadows = 2, r.ssao = 2, r.taa = true, r.bloom = true, r.textureSize = 2048,
            r.anisotropy = 16, r.ssr = true, r.volumetric = 1.0f, r.dust = true, r.particles = 3, r.sharpen = 0.25f;
            r.indirect = true, r.volumetricQuality = 3, r.contactShadows = true, r.parallax = true, r.lightQuality = 3;
            viewDistance = 6;
            break;
        default: break;
    }
}

// V6: Nachbearbeitung als Stufe (Bloom, Filmkorn, Vignette, Farbsaeume, Nachschaerfen)
void Settings::applyPostQuality(int q) {
    postQuality = q;
    auto& r = render;
    switch (q) {
        case 0: r.bloom = false, r.filmGrain = 0.0f, r.vignette = 0.0f, r.chromatic = 0.0f, r.sharpen = 0.0f; break;
        case 1: r.bloom = true, r.filmGrain = 0.2f, r.vignette = 0.2f, r.chromatic = 0.0f, r.sharpen = 0.25f; break;
        default: r.bloom = true, r.filmGrain = 0.35f, r.vignette = 0.3f, r.chromatic = 0.2f, r.sharpen = 0.35f; break;
    }
}

int Settings::detectPostQuality() const {
    for (int q = 0; q <= 2; ++q) {
        Settings t;
        t.applyPostQuality(q);
        auto same = [](float a, float b) { return std::fabs(a - b) < 0.01f; };
        if (t.render.bloom == render.bloom && same(t.render.filmGrain, render.filmGrain) &&
            same(t.render.vignette, render.vignette) && same(t.render.chromatic, render.chromatic))
            return q;
    }
    return -1;
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
    j.set("third_person", thirdPerson);
    j.set("show_body", showBody);
    j.set("minimap", minimap);
    j.set("minimap_rotate", minimapRotate);
    j.set("fullscreen", fullscreen);
    j.set("window_width", windowWidth);
    j.set("window_height", windowHeight);
    j.set("auto_quality", autoQuality);
    j.set("calibrated_for", calibratedFor);
    j.set("dynamic_resolution", dynamicResolution);
    j.set("post_quality", postQuality);
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
    j.set("dust", render.dust);
    j.set("particles", render.particles);
    j.set("light_quality", render.lightQuality);
    j.set("sharpen", (double)render.sharpen);
    j.set("indirect", render.indirect);
    j.set("volumetric_quality", render.volumetricQuality);
    j.set("contact_shadows", render.contactShadows);
    j.set("parallax", render.parallax);
    j.set("texture_size", render.textureSize);
    j.set("texture_compression", render.textureCompression);
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
    b("third_person", thirdPerson);
    b("show_body", showBody);
    b("minimap", minimap);
    b("minimap_rotate", minimapRotate);
    b("fullscreen", fullscreen);
    i("window_width", windowWidth, 640, 7680);
    i("window_height", windowHeight, 360, 4320);
    b("auto_quality", autoQuality);
    if (j.has("calibrated_for")) calibratedFor = j["calibrated_for"].asString("");
    b("dynamic_resolution", dynamicResolution);
    i("post_quality", postQuality, 0, 2);
    i("max_fps", maxFps, 0, 360);
    i("quality", quality, 0, 4);
    i("view_distance", viewDistance, 2, 6);  // Ringtexturen fassen 16 Chunks: mehr ergaebe Ueberschneidungen
    f("render_scale", render.renderScale, 0.5f, 1.5f);
    i("shadows", render.shadows, 0, 2);
    i("ssao", render.ssao, 0, 2);
    b("taa", render.taa);
    b("bloom", render.bloom);
    b("ssr", render.ssr);
    f("volumetric", render.volumetric, 0.0f, 3.0f);
    b("dust", render.dust);
    // V6: Partikelmenge (aeltere Dateien kennen nur "dust" an/aus)
    if (!j.has("particles") && j.has("dust")) render.particles = render.dust ? 2 : 0;
    i("particles", render.particles, 0, 3);
    i("light_quality", render.lightQuality, 0, 3);
    f("sharpen", render.sharpen, 0.0f, 1.0f);
    b("indirect", render.indirect);
    i("volumetric_quality", render.volumetricQuality, 1, 3);
    b("contact_shadows", render.contactShadows);
    b("parallax", render.parallax);
    i("texture_size", render.textureSize, 256, 2048);
    b("texture_compression", render.textureCompression);
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
        // V6: Dateien von vor der Automatik behalten ihre gewaehlte Qualitaet
        if (!j->has("auto_quality")) autoQuality = false;
        fromJson(*j);
        return true;
    }
    // Erster Start von V6: Einstellungen aus V5 uebernehmen (nur lesen), sonst aus V3/V4
    if (auto j = loadJsonFile(paths::join(paths::userDataDir(), "settings_v5.json"))) {
        fromJson(*j);
        log::info("Einstellungen aus V5 uebernommen");
        return false;
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
