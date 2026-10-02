// Einstellungen: sofort wirksam, dauerhaft gespeichert in
// %APPDATA%\LIMINAL\settings_v5.json. Beim ersten Start werden passende Werte
// aus V4 (settings_v3.json) uebernommen.
#pragma once

#include <string>

#include "core/json.hpp"
#include "render/renderer.hpp"

namespace lim {

struct Settings {
    // Maus & Steuerung (wie V4)
    bool mouseEnabled = true;
    float mouseSensitivity = 1.0f, mouseX = 1.0f, mouseY = 1.0f;
    bool invertX = false, invertY = false;
    float mouseSmoothing = 0.35f;
    float padSensitivity = 1.0f;
    // Bewegung
    bool bob = true;
    float walkSpeed = 3.0f;
    float fov = 0.0f;  // horizontales Sichtfeld in Grad, 0 = automatisch
    // Anzeige
    bool fullscreen = true;
    int maxFps = 0;    // 0 = unbegrenzt (bzw. VSync)
    int quality = 2;   // 0 niedrig, 1 mittel, 2 hoch, 3 ultra, 4 benutzerdefiniert
    int viewDistance = 4;  // Laderadius in Chunks
    gfx::RenderSettings render;
    // Audio
    float masterVolume = 0.8f, effectsVolume = 1.0f, ambienceVolume = 0.8f;

    void applyQualityPreset(int q);
    Json toJson() const;
    void fromJson(const Json& j);
    void migrateFromV4(const Json& j);

    bool load();
    bool save() const;
    static std::string path();
    void resetMouse();
};

}  // namespace lim
