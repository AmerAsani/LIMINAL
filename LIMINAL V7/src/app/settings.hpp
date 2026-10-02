// Einstellungen: sofort wirksam, dauerhaft gespeichert in
// %APPDATA%\LIMINAL\settings_v7.json. Beim ersten Start werden die Werte aus V6
// (settings_v6.json), sonst V5 (settings_v5.json) bzw. passende Werte aus V4 (settings_v3.json) uebernommen.
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
    // V6: Kamera und Koerper
    bool thirdPerson = false;  // Schulterkamera statt Ego-Perspektive (F5)
    bool showBody = true;      // eigenen Koerper (Strichmaennchen) zeigen
    // V6: Minimap
    bool minimap = true;
    bool minimapRotate = true;  // dreht sich mit dem Blick (sonst Norden oben)
    bool hallucinations = true;  // V7: Halluzinationen bei schwindender Sanity
    // Anzeige
    bool fullscreen = true;
    int windowWidth = 1600, windowHeight = 900;  // V6: Fenstergroesse (ohne Vollbild)
    int maxFps = 0;    // 0 = unbegrenzt (bzw. VSync)
    int quality = 1;   // 0 niedrig, 1 mittel, 2 hoch, 3 ultra, 4 benutzerdefiniert
    // V6: Qualitaet automatisch an die Hardware anpassen (Messung beim ersten Start bzw. nach
    // Wechsel der Grafikkarte/Aufloesung). calibratedFor: Grafikkarte und Aufloesung der letzten Messung.
    bool autoQuality = true;
    std::string calibratedFor;
    bool dynamicResolution = false;  // V6: interne Aufloesung haelt die Ziel-Bildrate (Bildrate max., sonst 60)
    int postQuality = 2;             // V6: Nachbearbeitung 0 aus, 1 dezent, 2 voll (setzt Bloom, Korn, Vignette ...)
    int viewDistance = 4;  // Laderadius in Chunks
    gfx::RenderSettings render;
    // Audio
    float masterVolume = 0.8f, effectsVolume = 1.0f, ambienceVolume = 0.8f;

    void applyQualityPreset(int q);
    void applyPostQuality(int q);
    int detectPostQuality() const;  // passende Stufe zu den Einzelwerten, -1 = eigene
    Json toJson() const;
    void fromJson(const Json& j);
    void migrateFromV4(const Json& j);

    bool load();
    bool save() const;
    static std::string path();
    void resetMouse();
};

}  // namespace lim
