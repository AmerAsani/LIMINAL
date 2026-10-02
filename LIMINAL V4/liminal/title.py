"""Hauptmenue (V4): Neues Spiel, Spiel fortsetzen, Einstellungen, Beenden.

Liegt ueber einer langsam kreisenden, abgedunkelten Ansicht der Welt und nutzt
dieselben Farben wie das Pausenmenue.
"""

from . import savegame
from .menu import HEAD, MUTED, PANEL, RULE, SEL, TITLE, VALUE
from .stats import DIFFICULTY

DIFFS = ("easy", "medium", "hard")
MAIN = ("Neues Spiel", "Spiel fortsetzen", "Einstellungen", "Beenden")


def _rgb(ck):
    return (((ck >> 10) & 31) * 255 // 31, ((ck >> 5) & 31) * 255 // 31, (ck & 31) * 255 // 31)


class TitleScreen:
    def __init__(self, game):
        self.game = game
        self.page = "main"
        self.sel = 0
        self.name = ""
        self.diff = 1
        self.saves = []
        self.confirm_delete = False
        self.msg = ""

    # --- Eingabe -----------------------------------------------------------------------
    def key(self, k, text=()):
        """Liefert eine Aktion: ('new', name, diff), ('load', slot), 'settings', 'quit' oder None."""
        if self.page == "new":
            return self._key_new(k, text)
        if self.page == "load":
            return self._key_load(k)
        if k in ("up", "w"):
            self.sel = (self.sel - 1) % len(MAIN)
        elif k in ("down", "s"):
            self.sel = (self.sel + 1) % len(MAIN)
        elif k in ("enter", "space"):
            self.msg = ""
            if self.sel == 0:
                self.page, self.sel, self.name = "new", 0, ""
            elif self.sel == 1:
                self.saves = savegame.list_saves()
                if self.saves:
                    self.page, self.sel = "load", 0
                else:
                    self.msg = "Noch keine Spielstände vorhanden."
            elif self.sel == 2:
                return "settings"
            else:
                return "quit"
        elif k == "esc":
            return "quit"
        return None

    def _key_new(self, k, text):
        # Zeile 0: Name, 1: Schwierigkeit, 2: Starten, 3: Zurueck
        if self.sel == 0:
            for c in text:
                if len(self.name) < 18 and (c.isalnum() or c in " -_.'"):
                    self.name += c
            if k == "backspace":
                self.name = self.name[:-1]
                return None
            if k == "space":
                return None
            if len(k) == 1 and k not in "+-":
                return None                      # Buchstaben gehoeren in den Namen
        if k in ("up",) or (k == "w" and self.sel):
            self.sel = (self.sel - 1) % 4
        elif k in ("down", "tab") or (k == "s" and self.sel):
            self.sel = (self.sel + 1) % 4
        elif k in ("left", "a") and self.sel == 1:
            self.diff = (self.diff - 1) % 3
        elif k in ("right", "d") and self.sel == 1:
            self.diff = (self.diff + 1) % 3
        elif k in ("enter", "space"):
            if self.sel in (0, 1):
                self.sel += 1
            elif self.sel == 2:
                return ("new", self.name.strip() or "Wanderer", DIFFS[self.diff])
            else:
                self.page, self.sel = "main", 0
        elif k in ("esc", "backspace"):
            self.page, self.sel = "main", 0
        return None

    def _key_load(self, k):
        n = len(self.saves)
        if k != "delete":
            self.confirm_delete = False
        if k in ("up", "w"):
            self.sel = (self.sel - 1) % n
        elif k in ("down", "s"):
            self.sel = (self.sel + 1) % n
        elif k in ("enter", "space"):
            return ("load", self.saves[self.sel]["slot"])
        elif k == "delete":
            if self.confirm_delete:
                savegame.delete(self.saves[self.sel]["slot"])
                self.saves = savegame.list_saves()
                self.confirm_delete = False
                if not self.saves:
                    self.page, self.sel = "main", 1
                    return None
                self.sel = min(self.sel, len(self.saves) - 1)
            else:
                self.confirm_delete = True
        elif k in ("esc", "backspace"):
            self.page, self.sel = "main", 1
        return None

    # --- Darstellung ---------------------------------------------------------------------
    def build(self, cols, rows, now):
        w = max(44, min(cols - 4, 76))
        L = []

        def add(style, text=""):
            L.append((style + text[:w].ljust(w), w))

        add(PANEL)
        add(TITLE, "   L I M I N A L")
        add(MUTED, "   unendliche Innenräume")
        add(RULE, "   " + "─" * (w - 6))
        add(PANEL)
        if self.page == "main":
            for i, label in enumerate(MAIN):
                add(SEL if i == self.sel else PANEL, ("  ▸ " if i == self.sel else "    ") + label)
            add(PANEL)
            add(MUTED, "   " + (self.msg or "↑↓ wählen    Enter bestätigen    ESC beenden"))
        elif self.page == "new":
            cursor = "▌" if int(now * 2) % 2 == 0 and self.sel == 0 else " "
            d = DIFFICULTY[DIFFS[self.diff]]
            rows_ = [("Name", (self.name or ("" if self.sel == 0 else "Wanderer")) + cursor),
                     ("Schwierigkeit", ("◂ %s ▸" % d["label"]) if self.sel == 1 else d["label"]),
                     ("Spiel starten", ""), ("Zurück", "")]
            for i, (label, val) in enumerate(rows_):
                t = ("  ▸ " if i == self.sel else "    ") + label.ljust(18) + val
                add(SEL if i == self.sel else (MUTED if i == 3 else PANEL), t)
            add(PANEL)
            add(VALUE, "   " + d["label"] + ": " + d["desc"])
            add(MUTED, "   Namen eintippen, ←→ Schwierigkeit, Enter weiter, ESC zurück")
        else:
            per = 9
            avail = max(1, (rows - 12) // per)
            start = max(0, min(self.sel - avail // 2, len(self.saves) - avail))
            for i in range(start, min(len(self.saves), start + avail)):
                self._entry(L, self.saves[i], i == self.sel, w)
            add(PANEL)
            hint = "Entf nochmal drücken = Spielstand löschen" if self.confirm_delete else \
                "↑↓ wählen    Enter laden    Entf löschen    ESC zurück"
            add(MUTED, "   " + hint)
        add(PANEL)
        top = max(0, (rows - len(L)) // 2)
        left = max(0, (cols - w) // 2)
        ov = {}
        for i, (t, vis) in enumerate(L):
            if top + i < rows:
                ov.setdefault(top + i, []).append((left, t, min(vis, cols - left)))
        return ov

    def _entry(self, L, d, sel, w):
        """Spielstand: Vorschaubild (32x16 Pixel = 32x8 Zeichen) links, Angaben rechts."""
        thumb = d.get("thumb") or []
        st = d.get("stats", {})
        pt = int(st.get("play_time", 0))
        info = [
            (HEAD, d.get("name", "?")),
            (PANEL, DIFFICULTY.get(d.get("difficulty"), DIFFICULTY["medium"])["label"] + "   ·   Seed %d" % d["seed"]),
            (PANEL, "Gespeichert %s" % d.get("saved_at", "")),
            (PANEL, "Spielzeit %d:%02d min   ·   %.0f m" % (pt // 60, pt % 60, d["player"].get("distance", 0))),
            (PANEL, "Health %d %%   ·   Sanity %d %%" % (round(st.get("health", 100)), round(st.get("sanity", 100)))),
        ]
        bg = (60, 48, 24) if sel else (15, 14, 11)
        mark = "\x1b[0;38;2;246;205;112;48;2;15;14;11m" + ("▸ " if sel else "  ")
        for r in range(8):
            if len(thumb) == 32 * 16:
                cells = []
                for c in range(32):
                    a = _rgb(thumb[(2 * r) * 32 + c])
                    b = _rgb(thumb[(2 * r + 1) * 32 + c])
                    cells.append("\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm▀" % (a + b))
                pic = "".join(cells)
            else:
                pic = "\x1b[48;2;30;28;22m" + " " * 32
            style, text = info[r] if r < len(info) else (PANEL, "")
            if sel and r == 0:
                style = "\x1b[0;1;38;2;255;226;160;48;2;%d;%d;%dm" % bg
            line = PANEL + " " + mark + pic + style + "  " + text[:w - 37].ljust(w - 37)
            L.append((line, w))
        L.append((PANEL + " " * w, w))
