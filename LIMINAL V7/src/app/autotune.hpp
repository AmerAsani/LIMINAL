// V6: Grafik passt sich der Hardware an - nicht einem bestimmten PC.
//
//   1. Schaetzung aus der Hardware (Grafikkarte, eigener/geteilter Speicher, Software-Rendering):
//      sofort beim Start, damit niemand mit Ultra auf einer schwachen Grafik beginnt.
//   2. Messung: Beim ersten Start (und nach einem Wechsel von Grafikkarte oder Aufloesung) misst
//      das Spiel im Hauptmenue einige Sekunden die echte GPU-Zeit eines Bildes und waehlt die
//      hoechste Stufe, die bei 60 Bildern pro Sekunde Luft laesst. Ultra nur, wenn reichlich
//      Reserve bleibt. Reicht selbst Niedrig nicht, wird die interne Aufloesung gesenkt und die
//      dynamische Aufloesung eingeschaltet.
//   3. Dynamische Aufloesung (optional): Waehrend des Spiels haelt die interne Aufloesung die
//      Ziel-Bildrate - bei Last weniger Pixel, bei Reserve wieder mehr (mit Abstand und Traegheit,
//      damit nichts pumpt).
#pragma once

#include <string>

#include "app/settings.hpp"

namespace lim::app {

class AutoTune {
public:
    // Kennung von Grafikkarte und Ausgabeaufloesung (aendert sie sich, wird neu gemessen)
    static std::string hardwareKey(const gfx::GpuInfo& gpu, int outW, int outH);
    // Startwert aus der Hardware (0..3)
    static int estimateFromHardware(const gfx::GpuInfo& gpu, int outW, int outH);
    // Stufe aus gemessener GPU-Zeit (ms) bei Stufe 'measured' und Ziel-Bildrate
    static int chooseFromMeasurement(float gpuMs, int measured, float targetFps, float* scaleOut = nullptr);

    // Beim Start: Automatik vorbereiten (Schaetzung anwenden, Messung anfordern). true = Einstellungen geaendert
    bool prepare(Settings& s, const gfx::GpuInfo& gpu, int outW, int outH);
    // Jedes Bild. Liefert eine Meldung, wenn die Messung abgeschlossen wurde (sonst leer).
    // gpuMs: geglaettete GPU-Zeit des letzten Bildes (0 = unbekannt), frameMs: Bildzeit (CPU-Sicht)
    std::string update(double dt, Settings& s, gfx::Renderer& r, bool sceneVisible, float gpuMs, float frameMs);
    bool calibrating() const { return pending_; }
    void requestCalibration() { pending_ = true, frames_ = 0, sum_ = 0.0, samples_ = 0; }

private:
    void dynamicResolution(double dt, Settings& s, gfx::Renderer& r, float gpuMs, float frameMs);
    bool pending_ = false;
    int frames_ = 0, samples_ = 0;
    double sum_ = 0.0;
    std::string key_;
    // dynamische Aufloesung
    float ema_ = 0.0f;
    double overFor_ = 0.0, underFor_ = 0.0, sinceChange_ = 0.0;
};

}  // namespace lim::app
