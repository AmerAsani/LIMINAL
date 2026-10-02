// Win32-Fenster: Erzeugung, Nachrichtenschleife, High-DPI, randloses Vollbild
// und Mausfang fuer das Umsehen. Leitet Eingaben an input::Input weiter.
#pragma once

#include <string>

#include "core/core.hpp"
#include "input/input.hpp"

struct HWND__;
using HWND = HWND__*;

namespace lim::plat {

enum class DisplayMode { Windowed, Borderless };

class Window {
public:
    Window() = default;
    ~Window();

    bool create(const std::wstring& title, int width, int height, DisplayMode mode, input::Input* input);
    // Verarbeitet alle anstehenden Nachrichten. false, wenn das Fenster geschlossen wurde.
    bool pump();

    HWND hwnd() const { return hwnd_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool focused() const { return focused_; }
    bool minimized() const { return minimized_; }
    bool closeRequested() const { return closeRequested_; }
    void requestClose() { closeRequested_ = true; }
    // true, wenn sich die Groesse seit dem letzten Aufruf geaendert hat
    bool consumeResize();
    float dpiScale() const { return dpiScale_; }

    void setDisplayMode(DisplayMode mode);
    // V6: Innengroesse im Fenstermodus (Lage bleibt, passt notfalls auf den Bildschirm)
    void setClientSize(int w, int h);
    DisplayMode displayMode() const { return mode_; }
    // Maus fangen: Zeiger verstecken, im Fenster halten, Raw-Input liefert Bewegung.
    void setMouseCaptured(bool captured);
    bool mouseCaptured() const { return captured_; }
    void setTitle(const std::wstring& title);

    // intern (Fensterprozedur)
    void attach(HWND h) {
        if (!hwnd_) hwnd_ = h;
    }
    long long handleMessage(unsigned msg, unsigned long long wp, long long lp);

private:
    void applyCapture();

    HWND hwnd_ = nullptr;
    input::Input* input_ = nullptr;
    int width_ = 0, height_ = 0;
    bool focused_ = true, minimized_ = false, closeRequested_ = false, resized_ = false;
    bool captured_ = false;
    DisplayMode mode_ = DisplayMode::Windowed;
    long windowedStyle_ = 0;
    int wx_ = 100, wy_ = 100, ww_ = 1280, wh_ = 720;  // Fensterlage vor dem Vollbild
    float dpiScale_ = 1.0f;
    int lastAbsX_ = 0, lastAbsY_ = 0;
    bool haveAbs_ = false;
};

}  // namespace lim::plat
