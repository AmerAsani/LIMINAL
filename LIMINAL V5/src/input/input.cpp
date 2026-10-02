#include "input/input.hpp"

#include <chrono>
#include <cstring>

#include "core/json.hpp"
#include "core/log.hpp"

#include <windows.h>
#include <xinput.h>

namespace lim::input {

namespace {

struct KeyNameEntry {
    const char* name;
    int code;
};

const KeyNameEntry kKeyNames[] = {
    {"Shift", VK_SHIFT}, {"Ctrl", VK_CONTROL}, {"Alt", VK_MENU}, {"Space", VK_SPACE}, {"Enter", VK_RETURN},
    {"Esc", VK_ESCAPE}, {"Tab", VK_TAB}, {"Backspace", VK_BACK}, {"Delete", VK_DELETE}, {"Up", VK_UP},
    {"Down", VK_DOWN}, {"Left", VK_LEFT}, {"Right", VK_RIGHT}, {"Plus", VK_OEM_PLUS}, {"Minus", VK_OEM_MINUS},
    {"NumPlus", VK_ADD}, {"NumMinus", VK_SUBTRACT}, {"Home", VK_HOME}, {"End", VK_END}, {"PageUp", VK_PRIOR},
    {"PageDown", VK_NEXT}, {"Insert", VK_INSERT}, {"CapsLock", VK_CAPITAL},
    {"Mouse1", MOUSE_LEFT}, {"Mouse2", MOUSE_RIGHT}, {"Mouse3", MOUSE_MIDDLE}, {"Mouse4", MOUSE_X1},
    {"Mouse5", MOUSE_X2}, {"WheelUp", WHEEL_UP}, {"WheelDown", WHEEL_DOWN},
};

const char* kActionNames[] = {
    "move_forward", "move_back", "move_left", "move_right", "sprint",
    "look_left", "look_right", "look_up", "look_down",
    "interact", "use_item", "inventory", "pause", "map", "debug", "screenshot", "toggle_bob",
    "speed_up", "speed_down", "help",
    "hotbar_1", "hotbar_2", "hotbar_3", "hotbar_4", "hotbar_5", "hotbar_6", "hotbar_7", "hotbar_8", "hotbar_9",
    "hotbar_next", "hotbar_prev",
    "ui_up", "ui_down", "ui_left", "ui_right", "ui_confirm", "ui_back", "ui_delete",
};
static_assert(sizeof(kActionNames) / sizeof(kActionNames[0]) == (size_t)Action::Count);

const char* kPadNames[] = {"A", "B", "X", "Y", "LB", "RB", "Back", "Start", "LStick", "RStick",
                           "DUp", "DDown", "DLeft", "DRight", "LT", "RT"};

using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

const char* actionName(Action a) { return kActionNames[(size_t)a]; }

int keyFromName(const std::string& n) {
    if (n.size() == 1) {
        char c = n[0];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
        if (c == '+') return VK_OEM_PLUS;
        if (c == '-') return VK_OEM_MINUS;
    }
    if (n.size() >= 2 && (n[0] == 'F' || n[0] == 'f')) {
        int f = std::atoi(n.c_str() + 1);
        if (f >= 1 && f <= 12) return VK_F1 + f - 1;
    }
    if (n.rfind("Num", 0) == 0 && n.size() == 4 && n[3] >= '0' && n[3] <= '9') return VK_NUMPAD0 + (n[3] - '0');
    for (const auto& e : kKeyNames)
        if (_stricmp(e.name, n.c_str()) == 0) return e.code;
    return 0;
}

std::string keyName(int code) {
    if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9')) return std::string(1, (char)code);
    if (code >= VK_F1 && code <= VK_F12) return "F" + std::to_string(code - VK_F1 + 1);
    if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9) return "Num" + std::to_string(code - VK_NUMPAD0);
    for (const auto& e : kKeyNames)
        if (e.code == code) return e.name;
    return "?";
}

Input::Input() {
    pad_.fill(-1);
    auto b = [&](Action a, std::vector<int> k, Pad p = Pad::Count) {
        bind(a, std::move(k), p == Pad::Count ? -1 : (int)p);
    };
    // Belegung wie in V4
    b(Action::MoveForward, {'W'});
    b(Action::MoveBack, {'S'});
    b(Action::MoveLeft, {'A'});
    b(Action::MoveRight, {'D'});
    b(Action::Sprint, {VK_SHIFT}, Pad::LStick);
    b(Action::LookLeft, {VK_LEFT});
    b(Action::LookRight, {VK_RIGHT});
    b(Action::LookUp, {VK_UP});
    b(Action::LookDown, {VK_DOWN});
    b(Action::Interact, {'E'}, Pad::A);
    b(Action::UseItem, {'F'}, Pad::X);
    b(Action::Inventory, {VK_TAB}, Pad::Y);
    b(Action::Pause, {VK_ESCAPE}, Pad::Start);
    b(Action::Map, {'M'}, Pad::Back);
    b(Action::Debug, {VK_F3, 'I'});
    b(Action::Screenshot, {'P', VK_F12});
    b(Action::ToggleBob, {'B'});
    b(Action::SpeedUp, {VK_OEM_PLUS, VK_ADD});
    b(Action::SpeedDown, {VK_OEM_MINUS, VK_SUBTRACT});
    b(Action::Help, {'H', VK_F1});
    for (int i = 0; i < 9; ++i) b((Action)((int)Action::Hotbar1 + i), {'1' + i, VK_NUMPAD1 + i});
    b(Action::HotbarNext, {WHEEL_DOWN}, Pad::RB);
    b(Action::HotbarPrev, {WHEEL_UP}, Pad::LB);
    b(Action::UiUp, {VK_UP, 'W'}, Pad::DUp);
    b(Action::UiDown, {VK_DOWN, 'S'}, Pad::DDown);
    b(Action::UiLeft, {VK_LEFT, 'A'}, Pad::DLeft);
    b(Action::UiRight, {VK_RIGHT, 'D'}, Pad::DRight);
    b(Action::UiConfirm, {VK_RETURN, VK_SPACE}, Pad::A);
    b(Action::UiBack, {VK_ESCAPE, VK_BACK}, Pad::B);
    b(Action::UiDelete, {VK_DELETE}, Pad::X);

    HMODULE xi = LoadLibraryW(L"xinput1_4.dll");  // Windows 8+
    if (!xi) xi = LoadLibraryW(L"xinput9_1_0.dll");  // Windows 7+
    if (xi) xinputGetState_ = (void*)GetProcAddress(xi, "XInputGetState");
}

void Input::bind(Action a, std::vector<int> keys, int padButton) {
    keys_[(size_t)a] = std::move(keys);
    pad_[(size_t)a] = padButton;
}

bool Input::loadBindings(const std::string& path) {
    auto j = loadJsonFile(path);
    if (!j) return false;
    for (const auto& [name, val] : (*j)["bindings"].members()) {
        int idx = -1;
        for (int i = 0; i < (int)Action::Count; ++i)
            if (name == kActionNames[i]) idx = i;
        if (idx < 0) {
            log::warn("input.json: unbekannte Aktion '{}'", name);
            continue;
        }
        std::vector<int> keys;
        for (const auto& k : val["keys"].items()) {
            int code = keyFromName(k.asString());
            if (code) keys.push_back(code);
            else log::warn("input.json: unbekannte Taste '{}'", k.asString());
        }
        int pad = -1;
        std::string p = val["pad"].asString("");
        for (int i = 0; i < (int)Pad::Count; ++i)
            if (p == kPadNames[i]) pad = i;
        bind((Action)idx, std::move(keys), pad);
    }
    return true;
}

void Input::onKey(int vk, bool isDown) {
    if (vk <= 0 || vk >= KEY_CODE_COUNT) return;
    if (isDown && !down_[(size_t)vk]) {
        pressed_[(size_t)vk] = true;
        anyPressed_ = true;
    }
    down_[(size_t)vk] = isDown;
}

void Input::onChar(char32_t c) {
    if (c >= 32 && c != 127) text_.push_back(c);
}

void Input::onRawMouse(int dx, int dy) {
    mouseDelta_.x += (float)dx;
    mouseDelta_.y += (float)dy;
}

void Input::onWheel(int steps) {
    wheel_ += steps;
    int code = steps > 0 ? WHEEL_UP : WHEEL_DOWN;
    pressed_[(size_t)code] = true;
}

void Input::onFocus(bool focused) {
    if (!focused) {
        down_.fill(false);
        padDown_.fill(false);
    }
}

void Input::beginFrame() {
    pressed_.fill(false);
    padPressed_.fill(false);
    mouseDelta_ = {0, 0};
    wheel_ = 0;
    text_.clear();
    anyPressed_ = false;
    cursorMoved = false;
    // Das Mausrad hat keinen Haltezustand
    down_[WHEEL_UP] = down_[WHEEL_DOWN] = false;
}

void Input::consumePressed() {
    pressed_.fill(false);
    padPressed_.fill(false);
    wheel_ = 0;
    text_.clear();
}

void Input::clearState() {
    consumePressed();
    down_.fill(false);
    padDown_.fill(false);
    mouseDelta_ = {0, 0};
    lstick_ = rstick_ = {0, 0};
    anyPressed_ = false;
    cursorMoved = false;
}

void Input::pollGamepad() {
    if (!xinputGetState_) return;
    double now = nowSeconds();
    if (!padConnected_ && now < padRetry_) return;  // nicht verbundene Pads selten abfragen
    auto get = (XInputGetStateFn)xinputGetState_;
    XINPUT_STATE st{};
    bool found = false;
    for (DWORD i = 0; i < 4 && !found; ++i)
        if (get(i, &st) == ERROR_SUCCESS) found = true;
    padConnected_ = found;
    if (!found) {
        padRetry_ = now + 2.0;
        padDown_.fill(false);
        lstick_ = rstick_ = {0, 0};
        return;
    }
    const XINPUT_GAMEPAD& g = st.Gamepad;
    auto stick = [](SHORT x, SHORT y, float dead) {
        vec2 v{x / 32767.0f, y / 32767.0f};
        float l = std::sqrt(v.x * v.x + v.y * v.y);
        if (l < dead) return vec2{0, 0};
        float k = std::min(1.0f, (l - dead) / (1.0f - dead)) / l;
        return vec2{v.x * k, v.y * k};
    };
    lstick_ = stick(g.sThumbLX, g.sThumbLY, 0.24f);
    rstick_ = stick(g.sThumbRX, g.sThumbRY, 0.2f);
    bool now_[(size_t)Pad::Count] = {
        (g.wButtons & XINPUT_GAMEPAD_A) != 0,          (g.wButtons & XINPUT_GAMEPAD_B) != 0,
        (g.wButtons & XINPUT_GAMEPAD_X) != 0,          (g.wButtons & XINPUT_GAMEPAD_Y) != 0,
        (g.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0, (g.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0,
        (g.wButtons & XINPUT_GAMEPAD_BACK) != 0,       (g.wButtons & XINPUT_GAMEPAD_START) != 0,
        (g.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0, (g.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0,
        (g.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0,    (g.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0,
        (g.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0,  (g.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0,
        g.bLeftTrigger > 100,                          g.bRightTrigger > 100,
    };
    for (size_t i = 0; i < (size_t)Pad::Count; ++i) {
        if (now_[i] && !padDown_[i]) {
            padPressed_[i] = true;
            anyPressed_ = true;
        }
        padDown_[i] = now_[i];
    }
}

bool Input::actionDown(Action a) const {
    for (int k : keys_[(size_t)a])
        if (down(k)) return true;
    int p = pad_[(size_t)a];
    return p >= 0 && padDown_[(size_t)p];
}

bool Input::actionPressed(Action a) const {
    for (int k : keys_[(size_t)a])
        if (pressed(k)) return true;
    int p = pad_[(size_t)a];
    return p >= 0 && padPressed_[(size_t)p];
}

}  // namespace lim::input
