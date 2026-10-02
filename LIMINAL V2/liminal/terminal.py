"""Terminal-Ansteuerung und Tastatureingabe ohne Enter.

Fuer fluessige Bewegung braucht das Spiel den Zustand "Taste gehalten":

* Windows: ReadConsoleInputW liefert echte Key-Down- und Key-Up-Ereignisse
  (conhost und Windows Terminal). Nur Standardbibliothek (ctypes).
* Linux/macOS: Das Terminal wird in den cbreak-Modus geschaltet. Unterstuetzt
  das Terminal das Kitty-Tastaturprotokoll (kitty, WezTerm, foot, Ghostty,
  neuere Konsolen), gibt es ebenfalls echte Loslass-Ereignisse. Andernfalls
  wird "gehalten" aus der Tastenwiederholung des Systems geschaetzt.
"""

import os
import sys
import time

INF = float("inf")


class Input:
    """Plattformunabhaengiger Tastenzustand."""

    def __init__(self):
        self._down = {}           # Taste -> Ablaufzeit (INF, solange echte Key-Ups kommen)
        self._pressed = []        # Flanken seit dem letzten Abholen (fuer Umschalter)
        self.release_events = False
        self.now = time.perf_counter()
        self._mdx = 0             # Mausbewegung (nur Unix-Terminals, in ~Pixeln)
        self._mdy = 0

    def mouse_move(self, dx, dy):
        self._mdx += dx
        self._mdy += dy

    def take_mouse(self):
        d = (self._mdx, self._mdy)
        self._mdx = self._mdy = 0
        return d

    def key(self, name, down, now=None):
        now = self.now if now is None else now
        if down:
            if name not in self._down or self._down[name] <= now:
                self._pressed.append(name)
                first = True
            else:
                first = False
            if self.release_events:
                self._down[name] = INF
            else:
                # Ohne Loslass-Ereignisse: bis zur ersten Wiederholung ueberbruecken
                self._down[name] = now + (0.55 if first else 0.13)
        else:
            self.release_events = True
            self._down.pop(name, None)

    def held(self, name):
        exp = self._down.get(name)
        return exp is not None and exp > self.now

    def sprint(self):
        return self.held("shift")

    def pressed(self):
        p = self._pressed
        self._pressed = []
        return p

    def clear(self):
        self._down.clear()


# ----------------------------------------------------------------------------------
# Windows
# ----------------------------------------------------------------------------------
_VK = {
    0x57: "w", 0x41: "a", 0x53: "s", 0x44: "d", 0x51: "q", 0x45: "e",
    0x25: "left", 0x26: "up", 0x27: "right", 0x28: "down",
    0x10: "shift", 0xA0: "shift", 0xA1: "shift", 0x1B: "esc",
    0x70: "f1", 0x72: "f3", 0x49: "i", 0x4D: "m", 0x56: "v", 0x42: "b",
    0x48: "h", 0x50: "p", 0x43: "c", 0x4E: "n", 0x46: "f", 0x09: "tab",
    0xBB: "+", 0x6B: "+", 0xBD: "-", 0x6D: "-",
    0x0D: "enter", 0x20: "space", 0x08: "backspace",
}


class _WinConsole:
    def __init__(self):
        import ctypes
        from ctypes import wintypes
        self.ctypes = ctypes
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.GetStdHandle.restype = wintypes.HANDLE
        k32.GetStdHandle.argtypes = [wintypes.DWORD]
        for fn in ("GetConsoleMode",):
            getattr(k32, fn).argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        k32.SetConsoleMode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        k32.GetNumberOfConsoleInputEvents.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]

        class KEY_EVENT_RECORD(ctypes.Structure):
            _fields_ = [("bKeyDown", wintypes.BOOL), ("wRepeatCount", wintypes.WORD),
                        ("wVirtualKeyCode", wintypes.WORD), ("wVirtualScanCode", wintypes.WORD),
                        ("uChar", wintypes.WCHAR), ("dwControlKeyState", wintypes.DWORD)]

        class _EV(ctypes.Union):
            _fields_ = [("KeyEvent", KEY_EVENT_RECORD), ("raw", ctypes.c_byte * 16)]

        class INPUT_RECORD(ctypes.Structure):
            _fields_ = [("EventType", wintypes.WORD), ("Event", _EV)]

        self.INPUT_RECORD = INPUT_RECORD
        k32.ReadConsoleInputW.argtypes = [wintypes.HANDLE, ctypes.POINTER(INPUT_RECORD),
                                          wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
        self.k32 = k32
        self.DWORD = wintypes.DWORD
        self.hin = k32.GetStdHandle(-10 & 0xFFFFFFFF)
        self.hout = k32.GetStdHandle(-11 & 0xFFFFFFFF)
        self.buf = (INPUT_RECORD * 128)()
        self.old_in = self.old_out = None

    def start(self):
        m = self.DWORD()
        if not self.k32.GetConsoleMode(self.hin, self.ctypes.byref(m)):
            # stdin umgeleitet? Dann direkt die Konsoleneingabe oeffnen.
            from ctypes import wintypes
            self.k32.CreateFileW.restype = wintypes.HANDLE
            self.k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
                                             wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
            self.hin = self.k32.CreateFileW("CONIN$", 0xC0000000, 3, None, 3, 0, None)
            if not self.k32.GetConsoleMode(self.hin, self.ctypes.byref(m)):
                raise RuntimeError("Kein Konsolen-Eingabegeraet gefunden. Bitte direkt in einem Terminal starten.")
        self.old_in = m.value
        self.k32.GetConsoleMode(self.hout, self.ctypes.byref(m))
        self.old_out = m.value
        # Keine Zeilenpufferung, kein Echo, kein QuickEdit (Mausklick wuerde das Spiel anhalten).
        # V2: Mauseingaben gehen an das Spiel (und werden ignoriert) statt Text zu markieren.
        self.k32.SetConsoleMode(self.hin, 0x0080 | 0x0008 | 0x0010)
        # VT-Sequenzen aktivieren
        self.k32.SetConsoleMode(self.hout, self.old_out | 0x0001 | 0x0004)
        self.k32.SetConsoleOutputCP(65001)

    def stop(self):
        if self.old_in is not None:
            self.k32.SetConsoleMode(self.hin, self.old_in)
        if self.old_out is not None:
            self.k32.SetConsoleMode(self.hout, self.old_out)

    def poll(self, inp):
        n = self.DWORD()
        k32 = self.k32
        byref = self.ctypes.byref
        while True:
            if not k32.GetNumberOfConsoleInputEvents(self.hin, byref(n)) or n.value == 0:
                return
            got = self.DWORD()
            k32.ReadConsoleInputW(self.hin, self.buf, min(128, n.value), byref(got))
            for i in range(got.value):
                rec = self.buf[i]
                if rec.EventType == 0x10:            # Fokus verloren/erhalten
                    inp.clear()
                elif rec.EventType == 1:
                    ke = rec.Event.KeyEvent
                    vk = ke.wVirtualKeyCode
                    if vk == 0x43 and (ke.dwControlKeyState & 0x000C) and ke.bKeyDown:
                        inp.key("quit", True)
                        continue
                    name = _VK.get(vk)
                    if name is None and ke.uChar in ("+", "-"):
                        name = ke.uChar
                    if name is not None:
                        inp.key(name, bool(ke.bKeyDown))


# ----------------------------------------------------------------------------------
# Unix
# ----------------------------------------------------------------------------------
_KITTY_KEYS = {27: "esc", 57441: "shift", 57447: "shift", 9: "tab", 43: "+", 45: "-", 61: "+",
               13: "enter", 32: "space", 127: "backspace"}
_ARROWS = {"A": "up", "B": "down", "C": "right", "D": "left", "P": "f1", "R": "f3"}
_TILDE = {11: "f1", 13: "f3"}


class _UnixConsole:
    def __init__(self):
        import termios
        import tty
        import select
        self.termios, self.tty, self.select = termios, tty, select
        self.fd = sys.stdin.fileno()
        self.old = None
        self.pending = ""
        self.kitty = False
        self.mouse_pos = None

    def start(self):
        if not os.isatty(self.fd):
            raise RuntimeError("Die Standardeingabe ist kein Terminal.")
        self.old = self.termios.tcgetattr(self.fd)
        self.tty.setcbreak(self.fd)
        # Kitty-Tastaturprotokoll anfragen: 1 disambiguieren, 2 Ereignistypen, 8 alle Tasten.
        # V2: Mausbewegungen melden lassen (jede Bewegung, SGR-Format).
        os.write(sys.stdout.fileno(), b"\x1b[>11u\x1b[?u\x1b[?1003h\x1b[?1006h")

    def stop(self):
        os.write(sys.stdout.fileno(), b"\x1b[?1003l\x1b[?1006l\x1b[<u")
        if self.old is not None:
            self.termios.tcsetattr(self.fd, self.termios.TCSADRAIN, self.old)

    def poll(self, inp):
        data = ""
        while self.select.select([self.fd], [], [], 0)[0]:
            chunk = os.read(self.fd, 4096)
            if not chunk:
                break
            data += chunk.decode("utf-8", "ignore")
        if not data and not self.pending:
            return
        self._parse(self.pending + data, inp)

    def _emit(self, inp, name, ev, shift):
        if shift is not None and ev != 3:
            inp.key("shift", shift)
        if ev == 3:
            inp.key(name, False)
        else:
            inp.key(name, True)

    def _parse(self, s, inp):
        i = 0
        n = len(s)
        self.pending = ""
        while i < n:
            ch = s[i]
            if ch == "\x1b":
                if i + 1 >= n:
                    inp.key("esc", True)
                    i += 1
                    continue
                nx = s[i + 1]
                if nx == "[":
                    j = i + 2
                    while j < n and not ("\x40" <= s[j] <= "\x7e"):
                        j += 1
                    if j >= n:
                        self.pending = s[i:]
                        return
                    self._csi(s[i + 2:j], s[j], inp)
                    i = j + 1
                elif nx == "O":
                    if i + 2 >= n:
                        self.pending = s[i:]
                        return
                    name = _ARROWS.get(s[i + 2])
                    if name:
                        inp.key(name, True)
                    i += 3
                else:
                    inp.key("esc", True)
                    i += 1
                continue
            if ch == "\x03":
                inp.key("quit", True)
            elif ch.isalpha():
                low = ch.lower()
                if ch != low:
                    inp.key("shift", True)
                inp.key(low, True)
            elif ch in "+-=":
                inp.key("+" if ch in "+=" else "-", True)
            elif ch == "\t":
                inp.key("tab", True)
            elif ch in "\r\n":
                inp.key("enter", True)
            elif ch == " ":
                inp.key("space", True)
            elif ch in "\x7f\x08":
                inp.key("backspace", True)
            i += 1

    def _csi(self, params, final, inp):
        if params.startswith("?"):
            if final == "u":
                self.kitty = True       # Terminal beherrscht das Kitty-Protokoll
            return
        if params.startswith("<") and final in "Mm":
            # SGR-Mausmeldung: <knopf;spalte;zeile
            try:
                b, x, y = (int(v) for v in params[1:].split(";"))
            except ValueError:
                return
            if self.mouse_pos is not None and (b & 32):
                inp.mouse_move((x - self.mouse_pos[0]) * 8, (y - self.mouse_pos[1]) * 16)
            self.mouse_pos = (x, y)
            return
        parts = params.split(";") if params else []
        ev = 1
        shift = None
        ctrl = False
        if len(parts) > 1:
            m = parts[1].split(":")
            mods = int(m[0]) if m[0].isdigit() else 1
            shift = bool((mods - 1) & 1)
            ctrl = bool((mods - 1) & 4)
            if len(m) > 1 and m[1].isdigit():
                ev = int(m[1])
        if final == "u":
            try:
                code = int(parts[0].split(":")[0])
            except (ValueError, IndexError):
                return
            name = _KITTY_KEYS.get(code)
            if name is None and 32 < code < 127:
                name = chr(code).lower()
            if name == "shift":
                inp.key("shift", ev != 3)
                return
            if code == 99 and ctrl:
                name = "quit"
            if name:
                self._emit(inp, name, ev, shift)
        elif final in _ARROWS:
            self._emit(inp, _ARROWS[final], ev, shift)
        elif final == "~":
            try:
                name = _TILDE.get(int(parts[0]))
            except (ValueError, IndexError):
                name = None
            if name:
                self._emit(inp, name, ev, shift)


# ----------------------------------------------------------------------------------
class Terminal:
    def __init__(self):
        self.backend = _WinConsole() if os.name == "nt" else _UnixConsole()
        self.active = False
        if os.name == "nt":
            self._out = sys.stdout.buffer
        else:
            self._fd = sys.stdout.fileno()

    def size(self):
        try:
            s = os.get_terminal_size(sys.stdout.fileno())
            return s.columns, s.lines
        except OSError:
            return 120, 40

    def start(self):
        self.backend.start()
        self.active = True
        # Alternativer Bildschirm, Cursor aus, kein Zeilenumbruch am Rand, Titel
        self.write("\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[2J\x1b]0;LIMINAL\x07")

    def stop(self):
        if not self.active:
            return
        self.active = False
        try:
            self.write("\x1b[0m\x1b[2J\x1b[?7h\x1b[?25h\x1b[?1049l")
        finally:
            self.backend.stop()

    def write(self, s):
        data = s.encode("utf-8")
        if os.name == "nt":
            self._out.write(data)
            self._out.flush()
        else:
            view = memoryview(data)
            while view:
                n = os.write(self._fd, view)
                view = view[n:]

    def poll(self, inp):
        inp.now = time.perf_counter()
        self.backend.poll(inp)
