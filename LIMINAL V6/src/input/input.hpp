// Eingabe: Tastatur, Maus (Raw Input fuer praezises Umsehen), Texteingabe und
// Gamepads (XInput). Das Spiel fragt keine Tasten direkt ab, sondern Aktionen
// (Action), die ueber data/input.json frei belegt werden koennen.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"

namespace lim::input {

// Zusaetzliche virtuelle Codes fuer Maustasten und Mausrad (ueber dem VK-Bereich).
enum MouseCode : int {
    MOUSE_LEFT = 256,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_X1,
    MOUSE_X2,
    WHEEL_UP,
    WHEEL_DOWN,
    KEY_CODE_COUNT
};

enum class Pad : int {
    A, B, X, Y, LB, RB, Back, Start, LStick, RStick, DUp, DDown, DLeft, DRight, LT, RT, Count
};

enum class Action : int {
    MoveForward, MoveBack, MoveLeft, MoveRight, Sprint,
    LookLeft, LookRight, LookUp, LookDown,
    Interact, UseItem, Inventory, Pause, Map, Debug, Screenshot, ToggleBob,
    SpeedUp, SpeedDown, Help,
    Hotbar1, Hotbar2, Hotbar3, Hotbar4, Hotbar5, Hotbar6, Hotbar7, Hotbar8, Hotbar9,
    HotbarNext, HotbarPrev,
    // V6
    Jump, ToggleCamera, DropItem,
    // Menues
    UiUp, UiDown, UiLeft, UiRight, UiConfirm, UiBack, UiDelete,
    Count
};

const char* actionName(Action a);
int keyFromName(const std::string& name);  // "W", "Shift", "F3", "Mouse1" ... -> Code (0 = unbekannt)
std::string keyName(int code);

class Input {
public:
    Input();

    // --- vom Fenster aufgerufen ----------------------------------------------------------
    void onKey(int vk, bool down);
    void onChar(char32_t c);
    void onRawMouse(int dx, int dy);
    void onWheel(int steps);
    void onFocus(bool focused);

    // Einmal pro Frame vor dem Verarbeiten der Fenster-Nachrichten.
    void beginFrame();
    // Nach den Nachrichten: Gamepad abfragen.
    void pollGamepad();
    // Tastendruecke dieses Bildes verwerfen (ein Druck soll nur eine Sache ausloesen,
    // z. B. ESC oeffnet das Menue und schliesst es nicht im selben Bild wieder).
    void consumePressed();
    // Alle Zustaende loeschen, Belegung behalten (automatische Tests/Aufnahmen)
    void clearState();

    // --- Abfragen ------------------------------------------------------------------------
    bool down(int code) const { return code > 0 && code < KEY_CODE_COUNT && down_[(size_t)code]; }
    bool pressed(int code) const { return code > 0 && code < KEY_CODE_COUNT && pressed_[(size_t)code]; }
    bool padDown(Pad b) const { return padDown_[(size_t)b]; }
    bool padPressed(Pad b) const { return padPressed_[(size_t)b]; }
    vec2 mouseDelta() const { return mouseDelta_; }
    int wheel() const { return wheel_; }
    const std::u32string& text() const { return text_; }
    vec2 leftStick() const { return lstick_; }
    vec2 rightStick() const { return rstick_; }
    bool gamepadConnected() const { return padConnected_; }
    bool anyKeyPressed() const { return anyPressed_; }

    // Aktionen (Tastatur, Maus und Gamepad zusammen)
    bool actionDown(Action a) const;
    bool actionPressed(Action a) const;
    // Belegungen: bis zu 3 Tasten und 1 Gamepad-Knopf je Aktion.
    void bind(Action a, std::vector<int> keys, int padButton = -1);
    const std::vector<int>& bindings(Action a) const { return keys_[(size_t)a]; }
    bool loadBindings(const std::string& path);
    // V6: Anzeigename der Belegung (Gamepad-Knopf, wenn ein Gamepad verbunden ist, sonst erste Taste)
    std::string bindingLabel(Action a) const;

    // Mausposition im Fenster (fuer Menues), vom Fenster gesetzt
    vec2 cursor{0, 0};
    bool cursorMoved = false;

private:
    std::array<bool, KEY_CODE_COUNT> down_{}, pressed_{};
    std::array<bool, (size_t)Pad::Count> padDown_{}, padPressed_{};
    vec2 mouseDelta_{0, 0};
    int wheel_ = 0;
    std::u32string text_;
    vec2 lstick_{0, 0}, rstick_{0, 0};
    bool padConnected_ = false;
    bool anyPressed_ = false;
    std::array<std::vector<int>, (size_t)Action::Count> keys_;
    std::array<int, (size_t)Action::Count> pad_{};
    void* xinputGetState_ = nullptr;
    double padRetry_ = 0.0;
};

}  // namespace lim::input
