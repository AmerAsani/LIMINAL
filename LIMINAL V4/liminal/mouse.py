"""Maussteuerung (V2): Umsehen mit der Maus wie in einem First-Person-Spiel.

Windows: Das Terminal meldet Mausbewegungen nur als Zellpositionen innerhalb
des Fensters. Fuer echtes "Mouse-Look" wird deshalb die Zeigerposition direkt
ueber die Win32-API gelesen und der Zeiger nach jedem Frame an einen festen
Punkt im Fenster zurueckgesetzt (wie es Spiele tun). Das passiert nur, solange
das Spielfenster im Vordergrund ist und kein Menue offen ist - sonst bleibt die
Maus frei.

Linux/macOS: Mausbewegungen werden ueber die SGR-Mausmeldungen des Terminals
gelesen (Einschraenkung: am Fensterrand endet die Bewegung).

Alle Bewegungen laufen durch eine zeitbasierte Glaettung: Die gemessene Drehung
wird nicht sofort, sondern ueber wenige Millisekunden verteilt angewendet -
fluessig statt ruckartig, ohne dass Bewegung verloren geht.
"""

import math
import os

YAW_PER_PX = 0.0022      # Drehung (rad) pro Pixel bei Empfindlichkeit 1.0
PITCH_PER_PX = 0.0017    # Neigung (Bildhoehen-Anteil) pro Pixel

TERMINAL_CLASSES = (
    "ConsoleWindowClass",              # klassische Windows-Konsole
    "CASCADIA_HOSTING_WINDOW_CLASS",   # Windows Terminal
    "mintty",                          # Git Bash / MSYS2
    "VirtualConsoleClass",             # ConEmu
    "org.wezfurlong.wezterm",
    "Alacritty",
)


class WinWindow:
    """Terminalfenster finden, maximieren und den Mauszeiger fuehren (nur Windows)."""

    def __init__(self):
        import ctypes
        from ctypes import wintypes
        self.ctypes = ctypes
        u = ctypes.WinDLL("user32", use_last_error=True)
        k = ctypes.WinDLL("kernel32", use_last_error=True)
        u.GetForegroundWindow.restype = wintypes.HWND
        u.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        u.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
        u.IsWindowVisible.argtypes = [wintypes.HWND]
        u.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
        u.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
        u.SetCursorPos.argtypes = [ctypes.c_int, ctypes.c_int]
        k.GetConsoleWindow.restype = wintypes.HWND
        self.u = u
        self.k = k
        self.RECT = wintypes.RECT
        self.POINT = wintypes.POINT
        self.hwnd = None
        self.captured = False
        self._pt = wintypes.POINT()
        self._rect = wintypes.RECT()

    def _class(self, hwnd):
        if not hwnd:
            return ""
        buf = self.ctypes.create_unicode_buffer(128)
        self.u.GetClassNameW(hwnd, buf, 128)
        return buf.value

    def find(self):
        """Ermittelt das Fenster, in dem das Spiel laeuft. True bei Erfolg."""
        con = self.k.GetConsoleWindow()
        if con and self.u.IsWindowVisible(con) and self._class(con) == "ConsoleWindowClass":
            self.hwnd = con
            return True
        fg = self.u.GetForegroundWindow()
        if fg and self._class(fg) in TERMINAL_CLASSES:
            self.hwnd = fg
            return True
        return False

    def maximize(self):
        if self.hwnd:
            self.u.ShowWindow(self.hwnd, 3)   # SW_MAXIMIZE

    def focused(self):
        return self.hwnd is not None and self.u.GetForegroundWindow() == self.hwnd

    def fullscreen(self):
        """V4: Alt+Enter an das eigene (aktive) Terminalfenster -> Vollbild."""
        if not self.focused():
            return False
        kb = self.u.keybd_event
        kb(0x12, 0, 0, 0)
        kb(0x0D, 0, 0, 0)
        kb(0x0D, 0, 2, 0)
        kb(0x12, 0, 2, 0)
        return True

    def release(self):
        self.captured = False

    def read_delta(self):
        """Mausbewegung seit dem letzten Aufruf (Pixel); setzt den Zeiger zurueck."""
        if not self.focused():
            self.captured = False
            return 0, 0
        r = self._rect
        if not self.u.GetWindowRect(self.hwnd, self.ctypes.byref(r)) or r.right - r.left < 80:
            return 0, 0
        # Parkposition: horizontal mittig, im unteren Fensterbereich (unauffaellig)
        px = (r.left + r.right) // 2
        py = r.top + int((r.bottom - r.top) * 0.8)
        if not self.captured:
            self.u.SetCursorPos(px, py)
            self.captured = True
            return 0, 0
        pt = self._pt
        self.u.GetCursorPos(self.ctypes.byref(pt))
        dx = pt.x - px
        dy = pt.y - py
        if dx or dy:
            self.u.SetCursorPos(px, py)
        return dx, dy


class MouseLook:
    """Wandelt Mausbewegung in weiche Dreh- und Neigebewegungen um."""

    def __init__(self, settings, inp):
        self.settings = settings
        self.inp = inp
        self.win = None
        if os.name == "nt":
            try:
                self.win = WinWindow()
            except (OSError, AttributeError):
                self.win = None
        self.pend_yaw = 0.0
        self.pend_pitch = 0.0
        self.last_raw = (0, 0)

    def find_window(self):
        return self.win.find() if self.win else False

    def maximize(self):
        if self.win:
            self.win.maximize()

    def fullscreen(self):
        return self.win.fullscreen() if self.win else False

    @property
    def window_found(self):
        return bool(self.win and self.win.hwnd)

    def update(self, dt, active):
        """Liefert (Drehung in rad, Neigungsaenderung) fuer diesen Frame."""
        s = self.settings
        if not (active and s["mouse_enabled"]):
            if self.win:
                self.win.release()
            self.inp.take_mouse()
            self.pend_yaw = self.pend_pitch = 0.0
            return 0.0, 0.0
        if self.win:
            dx, dy = self.win.read_delta()
        else:
            dx, dy = self.inp.take_mouse()
        self.last_raw = (dx, dy)
        sens = s["mouse_sensitivity"]
        sx = -1.0 if s["invert_x"] else 1.0
        sy = -1.0 if s["invert_y"] else 1.0
        self.pend_yaw += dx * YAW_PER_PX * sens * s["mouse_x"] * sx
        self.pend_pitch -= dy * PITCH_PER_PX * sens * s["mouse_y"] * sy
        sm = s["mouse_smoothing"]
        if sm <= 0.001:
            a = 1.0
        else:
            a = 1.0 - math.exp(-dt / (sm * 0.07))
        oy = self.pend_yaw * a
        op = self.pend_pitch * a
        self.pend_yaw -= oy
        self.pend_pitch -= op
        return oy, op

    def release(self):
        if self.win:
            self.win.release()
