#include "platform/window.hpp"

#include "core/log.hpp"

#include <windows.h>
#include <windowsx.h>

namespace lim::plat {

namespace {

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* w = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (w) {
        w->attach(hwnd);
        return (LRESULT)w->handleMessage(msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void enableDpiAwareness() {
    // Windows 10 1703+: pro Monitor (V2); sonst systemweit
    HMODULE user = GetModuleHandleW(L"user32.dll");
    using Fn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto fn = user ? (Fn)(void*)GetProcAddress(user, "SetProcessDpiAwarenessContext") : nullptr;
    if (!fn || !fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) SetProcessDPIAware();
}

}  // namespace

Window::~Window() {
    if (hwnd_) {
        setMouseCaptured(false);
        DestroyWindow(hwnd_);
    }
}

bool Window::create(const std::wstring& title, int width, int height, DisplayMode mode, input::Input* input) {
    enableDpiAwareness();
    input_ = input;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"LiminalWindow";
    RegisterClassExW(&wc);

    // Fenster auf dem Hauptmonitor zentrieren (Groesse an Arbeitsflaeche anpassen)
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int aw = work.right - work.left, ah = work.bottom - work.top;
    width = std::min(width, aw);
    height = std::min(height, ah);
    windowedStyle_ = WS_OVERLAPPEDWINDOW;
    RECT r{0, 0, width, height};
    AdjustWindowRect(&r, (DWORD)windowedStyle_, FALSE);
    int ow = std::min<int>(r.right - r.left, aw), oh = std::min<int>(r.bottom - r.top, ah);
    wx_ = work.left + (aw - ow) / 2;
    wy_ = work.top + (ah - oh) / 2;
    ww_ = ow;
    wh_ = oh;
    hwnd_ = CreateWindowExW(0, wc.lpszClassName, title.c_str(), (DWORD)windowedStyle_, wx_, wy_, ow, oh, nullptr,
                            nullptr, inst, this);
    if (!hwnd_) {
        log::error("CreateWindowEx fehlgeschlagen ({})", GetLastError());
        return false;
    }
    UINT dpi = 96;
    HMODULE user = GetModuleHandleW(L"user32.dll");
    using GetDpiFn = UINT(WINAPI*)(HWND);
    if (auto fn = user ? (GetDpiFn)(void*)GetProcAddress(user, "GetDpiForWindow") : nullptr) dpi = fn(hwnd_);
    dpiScale_ = dpi / 96.0f;

    // Raw Input fuer die Maus (ungefilterte, hochaufloesende Bewegung)
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.hwndTarget = hwnd_;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) log::warn("Raw Input nicht verfuegbar");

    ShowWindow(hwnd_, SW_SHOW);
    setDisplayMode(mode);
    RECT cr;
    GetClientRect(hwnd_, &cr);
    width_ = cr.right - cr.left;
    height_ = cr.bottom - cr.top;
    resized_ = true;
    return true;
}

bool Window::pump() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            closeRequested_ = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !closeRequested_;
}

bool Window::consumeResize() {
    bool r = resized_;
    resized_ = false;
    return r;
}

void Window::setTitle(const std::wstring& title) { SetWindowTextW(hwnd_, title.c_str()); }

void Window::setDisplayMode(DisplayMode mode) {
    if (!hwnd_) return;
    if (mode == DisplayMode::Borderless) {
        if (mode_ == DisplayMode::Windowed) {
            RECT r;
            GetWindowRect(hwnd_, &r);
            wx_ = r.left, wy_ = r.top, ww_ = r.right - r.left, wh_ = r.bottom - r.top;
        }
        HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(mon, &mi);
        SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    } else {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_ | WS_VISIBLE);
        SetWindowPos(hwnd_, nullptr, wx_, wy_, ww_, wh_, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }
    mode_ = mode;
    applyCapture();
}

void Window::setMouseCaptured(bool captured) {
    if (captured == captured_) return;
    captured_ = captured;
    applyCapture();
}

void Window::applyCapture() {
    if (!hwnd_) return;
    haveAbs_ = false;  // absolute Mauskoordinaten (Remote Desktop) neu beginnen
    if (captured_ && focused_) {
        RECT r;
        GetClientRect(hwnd_, &r);
        POINT tl{r.left, r.top}, br{r.right, r.bottom};
        ClientToScreen(hwnd_, &tl);
        ClientToScreen(hwnd_, &br);
        RECT clip{tl.x + (br.x - tl.x) / 2 - 1, tl.y + (br.y - tl.y) / 2 - 1, tl.x + (br.x - tl.x) / 2 + 1,
                  tl.y + (br.y - tl.y) / 2 + 1};
        ClipCursor(&clip);
        while (ShowCursor(FALSE) >= 0) {
        }
    } else {
        ClipCursor(nullptr);
        while (ShowCursor(TRUE) < 0) {
        }
    }
}

long long Window::handleMessage(unsigned msg, unsigned long long wp, long long lp) {
    switch (msg) {
        case WM_CLOSE:
            closeRequested_ = true;
            return 0;
        case WM_DESTROY:
            return 0;
        case WM_SIZE: {
            minimized_ = (wp == SIZE_MINIMIZED);
            int w = LOWORD(lp), h = HIWORD(lp);
            if (!minimized_ && w > 0 && h > 0 && (w != width_ || h != height_)) {
                width_ = w;
                height_ = h;
                resized_ = true;
                applyCapture();
            }
            return 0;
        }
        case WM_DPICHANGED: {
            dpiScale_ = HIWORD(wp) / 96.0f;
            if (mode_ == DisplayMode::Windowed) {
                auto* r = reinterpret_cast<RECT*>(lp);
                SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }
        case WM_ACTIVATE:
            focused_ = LOWORD(wp) != WA_INACTIVE;
            if (input_) input_->onFocus(focused_);
            applyCapture();
            return 0;
        case WM_SETFOCUS:
            focused_ = true;
            applyCapture();
            return 0;
        case WM_KILLFOCUS:
            focused_ = false;
            if (input_) input_->onFocus(false);
            applyCapture();
            return 0;
        case WM_INPUT: {
            RAWINPUT raw;
            UINT size = sizeof(raw);
            if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
                raw.header.dwType == RIM_TYPEMOUSE && input_ && focused_) {
                const RAWMOUSE& m = raw.data.mouse;
                if (m.usFlags & MOUSE_MOVE_ABSOLUTE) {
                    // Remote-Desktop/Tablet: absolute Koordinaten -> Differenz
                    bool virt = (m.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
                    int sw = GetSystemMetrics(virt ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
                    int sh = GetSystemMetrics(virt ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
                    int ax = (int)(m.lLastX / 65535.0f * sw), ay = (int)(m.lLastY / 65535.0f * sh);
                    if (haveAbs_) input_->onRawMouse(ax - lastAbsX_, ay - lastAbsY_);
                    lastAbsX_ = ax;
                    lastAbsY_ = ay;
                    haveAbs_ = true;
                } else if (m.lLastX || m.lLastY) {
                    input_->onRawMouse(m.lLastX, m.lLastY);
                }
            }
            return DefWindowProcW(hwnd_, msg, wp, lp);
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (input_) input_->onKey((int)wp, true);
            if (msg == WM_SYSKEYDOWN && wp == VK_F4) closeRequested_ = true;  // Alt+F4
            if (msg == WM_SYSKEYDOWN && wp == VK_RETURN) setDisplayMode(mode_ == DisplayMode::Windowed
                                                                            ? DisplayMode::Borderless
                                                                            : DisplayMode::Windowed);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (input_) input_->onKey((int)wp, false);
            return 0;
        case WM_CHAR: {
            // UTF-16 -> UTF-32 (Ersatzpaare zusammensetzen)
            static wchar_t high = 0;
            wchar_t c = (wchar_t)wp;
            if (c >= 0xD800 && c < 0xDC00) {
                high = c;
                return 0;
            }
            char32_t cp = c;
            if (c >= 0xDC00 && c < 0xE000 && high) cp = 0x10000 + ((high - 0xD800) << 10) + (c - 0xDC00);
            high = 0;
            if (input_) input_->onChar(cp);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (input_) {
                input_->cursor = {(float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp)};
                input_->cursorMoved = true;
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            if (input_) input_->onKey(input::MOUSE_LEFT, msg == WM_LBUTTONDOWN);
            if (msg == WM_LBUTTONDOWN) SetCapture(hwnd_);
            else ReleaseCapture();
            return 0;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            if (input_) input_->onKey(input::MOUSE_RIGHT, msg == WM_RBUTTONDOWN);
            return 0;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            if (input_) input_->onKey(input::MOUSE_MIDDLE, msg == WM_MBUTTONDOWN);
            return 0;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
            if (input_)
                input_->onKey(GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? input::MOUSE_X1 : input::MOUSE_X2,
                              msg == WM_XBUTTONDOWN);
            return TRUE;
        case WM_MOUSEWHEEL: {
            static int acc = 0;
            acc += GET_WHEEL_DELTA_WPARAM(wp);
            while (acc >= WHEEL_DELTA) {
                if (input_) input_->onWheel(1);
                acc -= WHEEL_DELTA;
            }
            while (acc <= -WHEEL_DELTA) {
                if (input_) input_->onWheel(-1);
                acc += WHEEL_DELTA;
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU) return 0;  // Alt allein oeffnet kein Menue
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace lim::plat
