"""Einstellungsmenue (V2).

ESC oeffnet das Menue, das Spiel pausiert, das Bild wird abgedunkelt. Die
Darstellung benutzt dieselben warmen Farben wie die uebrigen Einblendungen,
damit das Menue wie ein Teil der Welt wirkt und nicht wie ein Fremdkoerper.

Seiten: Hauptmenue, Mauseinstellungen, Grafik & Anzeige, Hilfe.
Jede Aenderung wird sofort angewendet und beim Schliessen gespeichert.
"""

PANEL = "\x1b[0;38;2;222;218;200;48;2;15;14;11m"
TITLE = "\x1b[0;1;38;2;255;214;120;48;2;15;14;11m"
HEAD = "\x1b[0;38;2;255;214;120;48;2;15;14;11m"
MUTED = "\x1b[0;38;2;138;132;112;48;2;15;14;11m"
RULE = "\x1b[0;38;2;84;78;58;48;2;15;14;11m"
SEL = "\x1b[0;38;2;18;16;10;48;2;246;205;112m"
SELVAL = "\x1b[0;1;38;2;18;16;10;48;2;246;205;112m"
VALUE = "\x1b[0;38;2;255;226;160;48;2;15;14;11m"

HELP_TEXT = [
    ("h", "Bewegung"),
    ("k", "W / S", "vorwärts / rückwärts gehen"),
    ("k", "A / D", "seitlich gehen"),
    ("k", "Shift", "sprinten (gedrückt halten)"),
    ("k", "+ / -", "Laufgeschwindigkeit ändern"),
    ("t", "Treppen und Stufen steigst du einfach hinauf, von Kanten fällst du hinunter."),
    ("", ""),
    ("h", "Umsehen mit der Maus"),
    ("k", "Maus links/rechts", "nach links / rechts drehen"),
    ("k", "Maus hoch/runter", "nach oben / unten schauen"),
    ("k", "Pfeiltasten", "Alternative zur Maus"),
    ("t", "Die Maus wird nur geführt, solange das Spielfenster aktiv ist und kein Menü offen "
          "ist. Mit ESC gibst du sie frei. Empfindlichkeit, Achsen, Invertieren und "
          "Glättung stellst du unter »Mauseinstellungen« ein."),
    ("", ""),
    ("h", "Menü"),
    ("k", "ESC", "Menü öffnen / schließen (Spiel pausiert)"),
    ("k", "↑ ↓  oder  W S", "Eintrag wählen"),
    ("k", "← →  oder  A D", "Wert ändern"),
    ("k", "Enter / Leertaste", "bestätigen, umschalten"),
    ("k", "ESC / Rücktaste", "eine Ebene zurück"),
    ("", ""),
    ("h", "Weitere Funktionen"),
    ("k", "M oder Tab", "Karte der geladenen Umgebung"),
    ("k", "F3 oder I", "Debug-Anzeige (FPS, Position, Raum, Seed ...)"),
    ("k", "V", "Darstellung: Halbblock-HiRes / ASCII / Mono"),
    ("k", "F", "Leistungsmodus (halbe horizontale Auflösung)"),
    ("k", "B", "Head-Bobbing an / aus"),
    ("k", "P", "Screenshot als HTML-Datei speichern"),
    ("k", "H", "diese Hilfe öffnen"),
    ("", ""),
    ("h", "Die Welt"),
    ("t", "LIMINAL hat kein Ziel, keine Gegner und keine Gegenstände. Du erkundest eine "
          "unendliche, prozedural erzeugte Welt aus Innenräumen: helle Büroflure, Beton "
          "und Industrie, unterirdische Tunnel, riesige leere Hallen und enge "
          "Wartungsbereiche. Türen sind offene Durchgänge - du gehst einfach hindurch."),
    ("t", "Die Zonen gehen fließend ineinander über. Je weiter du dich vom Start entfernst, "
          "desto häufiger begegnen dir seltene, seltsame Räume."),
    ("t", "Jede Welt entsteht aus einem Seed. Mit demselben Seed (siehe Debug-Anzeige) "
          "betrittst du exakt dieselbe Welt erneut:  LIMINAL.exe --seed 1234"),
]


class Item:
    def __init__(self, label, kind, key=None, lo=0.0, hi=1.0, step=0.1, fmt="%.1f",
                 choices=None, target=None, desc=""):
        self.label = label
        self.kind = kind          # page | action | toggle | number | choice
        self.key = key
        self.lo, self.hi, self.step, self.fmt = lo, hi, step, fmt
        self.choices = choices or []
        self.target = target
        self.desc = desc


class Menu:
    def __init__(self, game):
        self.game = game
        self.open = False
        self.page = "main"
        self.sel = 0
        self.scroll = 0
        self._sel_memory = {}

    # --- Seiten ---------------------------------------------------------------------
    def items(self):
        if self.page == "main":
            return [
                Item("Weiter erkunden", "action", target="close",
                     desc="Zurück ins Spiel. Die Maus wird wieder zum Umsehen verwendet."),
                Item("Mauseinstellungen", "page", target="mouse",
                     desc="Empfindlichkeit, Achsen, Invertieren und Glättung der Maussteuerung."),
                Item("Grafik & Anzeige", "page", target="graphics",
                     desc="Darstellung, Beleuchtung, Sichtfeld, Bildrate und Bewegung."),
                Item("Hilfe", "page", target="help",
                     desc="Alle Steuerungen und Funktionen des Spiels im Überblick."),
                Item("Spiel beenden", "action", target="quit",
                     desc="LIMINAL schließen. Einstellungen werden gespeichert."),
            ]
        if self.page == "mouse":
            return [
                Item("Maussteuerung", "toggle", key="mouse_enabled",
                     desc="Umsehen mit der Maus ein- oder ausschalten (Pfeiltasten funktionieren immer)."),
                Item("Empfindlichkeit", "number", key="mouse_sensitivity", lo=0.1, hi=5.0, step=0.1,
                     desc="Grundempfindlichkeit für beide Achsen."),
                Item("X-Achse  (links / rechts)", "number", key="mouse_x", lo=0.2, hi=3.0, step=0.1,
                     fmt="%.1fx", desc="Zusätzlicher Faktor für das Drehen nach links und rechts."),
                Item("Y-Achse  (oben / unten)", "number", key="mouse_y", lo=0.2, hi=3.0, step=0.1,
                     fmt="%.1fx", desc="Zusätzlicher Faktor für das Schauen nach oben und unten."),
                Item("X-Achse invertieren", "toggle", key="invert_x",
                     desc="Maus nach rechts dreht nach links (und umgekehrt)."),
                Item("Y-Achse invertieren", "toggle", key="invert_y",
                     desc="Maus nach oben schaut nach unten - wie im Flugsimulator."),
                Item("Glättung", "number", key="mouse_smoothing", lo=0.0, hi=0.9, step=0.05,
                     fmt="pct", desc="0 % = direkte Reaktion, höher = weichere, fließendere Kamerabewegung."),
                Item("Standardwerte", "action", target="reset_mouse",
                     desc="Alle Mauseinstellungen auf die Standardwerte zurücksetzen."),
                Item("Zurück", "page", target="main", desc=""),
            ]
        if self.page == "graphics":
            fovs = [(0, "Auto")] + [(v, "%d°" % v) for v in range(60, 115, 5)]
            return [
                Item("Darstellung", "choice", key="render_mode",
                     choices=[("hires", "Halbblock-HiRes"), ("ascii", "ASCII-Schattierung"), ("mono", "Monochrom")],
                     desc="HiRes nutzt ▀-Halbblöcke für doppelte vertikale Auflösung (empfohlen)."),
                Item("Weiche Beleuchtung", "toggle", key="smooth_light",
                     desc="Fließende Lichtverläufe, Kontakt- und Säulenschatten. Aus = etwas schneller."),
                Item("Leistungsmodus", "toggle", key="fast",
                     desc="Halbe horizontale Auflösung - für langsame Rechner oder sehr große Fenster."),
                Item("Sichtfeld", "choice", key="fov", choices=fovs,
                     desc="Horizontales Sichtfeld. Auto passt es an das Seitenverhältnis des Fensters an."),
                Item("Bildrate (max.)", "choice", key="max_fps",
                     choices=[(30, "30 FPS"), (45, "45 FPS"), (60, "60 FPS"), (90, "90 FPS"), (144, "144 FPS")],
                     desc="Obergrenze der Bildrate. Weniger entlastet Terminal und Prozessor."),
                Item("Head-Bobbing", "toggle", key="bob",
                     desc="Leichtes Mitschwingen der Kamera beim Gehen."),
                Item("Laufgeschwindigkeit", "number", key="walk_speed", lo=1.0, hi=8.0, step=0.5,
                     fmt="%.1f m/s", desc="Normales Gehtempo. Mit Shift sprintest du etwa doppelt so schnell."),
                Item("Fenster beim Start maximieren", "toggle", key="maximize",
                     desc="Mehr Fläche = höhere Auflösung. Wirkt beim nächsten Start."),
                Item("Zurück", "page", target="main", desc=""),
            ]
        return [Item("Zurück", "page", target="main", desc="")]

    # --- Steuerung ------------------------------------------------------------------
    def show(self, page="main"):
        self.open = True
        self.page = page
        self.sel = self._sel_memory.get(page, 0)
        self.scroll = 0

    def close(self):
        self.open = False
        self.page = "main"
        self.game.settings.save()

    def _goto(self, page):
        self._sel_memory[self.page] = self.sel
        self.page = page
        self.sel = self._sel_memory.get(page, 0)
        self.scroll = 0

    def _back(self):
        if self.page == "main":
            self.close()
        else:
            self._goto("main")

    def key(self, k):
        """Verarbeitet eine Taste. Gibt 'quit' zurueck, wenn beendet werden soll."""
        if k in ("esc", "backspace"):
            self._back()
            return None
        if self.page == "help":
            if k in ("up", "w"):
                self.scroll = max(0, self.scroll - 1)
            elif k in ("down", "s"):
                self.scroll += 1
            elif k in ("enter", "space", "h"):
                self._goto("main")
            return None
        items = self.items()
        if k in ("up", "w"):
            self.sel = (self.sel - 1) % len(items)
        elif k in ("down", "s"):
            self.sel = (self.sel + 1) % len(items)
        elif k in ("left", "a"):
            self._adjust(items[self.sel], -1)
        elif k in ("right", "d"):
            self._adjust(items[self.sel], +1)
        elif k in ("enter", "space"):
            it = items[self.sel]
            if it.kind == "page":
                self._goto(it.target)
            elif it.kind == "action":
                if it.target == "close":
                    self.close()
                elif it.target == "quit":
                    self.game.settings.save()
                    return "quit"
                elif it.target == "reset_mouse":
                    self.game.settings.reset_mouse()
                    self.game.apply_settings()
            else:
                self._adjust(it, +1)
        return None

    def _adjust(self, it, direction):
        s = self.game.settings
        if it.kind == "toggle":
            s[it.key] = not s[it.key]
        elif it.kind == "number":
            v = s[it.key] + direction * it.step
            v = max(it.lo, min(it.hi, round(v / it.step) * it.step))
            s[it.key] = round(v, 3)
        elif it.kind == "choice":
            values = [c[0] for c in it.choices]
            try:
                i = values.index(s[it.key])
            except ValueError:
                i = 0
            s[it.key] = values[(i + direction) % len(values)]
        else:
            return
        self.game.apply_settings()

    def _value(self, it):
        s = self.game.settings
        if it.kind == "toggle":
            return "An" if s[it.key] else "Aus"
        if it.kind == "number":
            if it.fmt == "pct":
                return "%d %%" % round(s[it.key] * 100)
            return it.fmt % s[it.key]
        if it.kind == "choice":
            for v, label in it.choices:
                if v == s[it.key]:
                    return label
            return str(s[it.key])
        if it.kind == "page" and it.target != "main":
            return "›"
        return ""

    # --- Darstellung ----------------------------------------------------------------
    def build(self, cols, rows):
        w = max(40, min(cols - 4, 72))
        lines = []          # (stil, text)

        def add(style, text=""):
            lines.append((style, text))

        titles = {"main": "", "mouse": "Mauseinstellungen", "graphics": "Grafik & Anzeige", "help": "Hilfe"}
        head = "L I M I N A L"
        sub = titles.get(self.page, "")
        add(PANEL)
        add(TITLE, ("   " + head).ljust(w - len(sub) - 3) + sub + "   ")
        add(RULE, "   " + "─" * (w - 6) + "   ")
        if self.page == "help":
            body = self._help_lines(w - 6)
            avail = max(5, rows - 10)
            self.scroll = max(0, min(self.scroll, len(body) - avail))
            view = body[self.scroll:self.scroll + avail]
            for style, text in view:
                add(style, "   " + text)
            add(PANEL)
            more = []
            if self.scroll > 0:
                more.append("▲ mehr")
            if self.scroll + avail < len(body):
                more.append("▼ mehr")
            add(RULE, "   " + "─" * (w - 6) + "   ")
            add(MUTED, "   ↑↓ blättern     ESC zurück" + ("     " + "  ".join(more) if more else ""))
        else:
            items = self.items()
            self.sel %= len(items)
            add(PANEL)
            for i, it in enumerate(items):
                val = self._value(it)
                sel = i == self.sel
                label = ("  ▸ " if sel else "    ") + it.label
                if it.kind in ("number", "choice"):
                    val = ("◂ %s ▸" % val) if sel else val
                text = label.ljust(w - len(val) - 5) + val + "   "
                if sel:
                    lines.append((SEL, text))
                else:
                    lines.append((PANEL if it.kind != "page" or it.target != "main" else MUTED, text))
            add(PANEL)
            desc = items[self.sel].desc
            for part in _wrap(desc, w - 6) or [""]:
                add(MUTED, "   " + part)
            if self.page == "main":
                g = self.game
                add(PANEL)
                add(MUTED, "   Seed %d   ·   %.0f m zurückgelegt   ·   %s" % (
                    g.world.seed, g.player.distance,
                    "Maus aktiv" if g.mouse.window_found else "Maus: Fenster nicht erkannt"))
            add(RULE, "   " + "─" * (w - 6) + "   ")
            add(MUTED, "   ↑↓ Auswahl    ←→ Wert    Enter OK    ESC zurück")
        add(PANEL)

        top = max(0, (rows - len(lines)) // 2)
        left = max(0, (cols - w) // 2)
        ov = {}
        for i, (style, text) in enumerate(lines):
            r = top + i
            if r >= rows:
                break
            t = text[:w].ljust(w)
            ov.setdefault(r, []).append((left, style + t, w))
        return ov

    def _help_lines(self, width):
        out = []
        for entry in HELP_TEXT:
            kind = entry[0]
            if kind == "h":
                out.append((HEAD, entry[1].upper()))
            elif kind == "k":
                key, text = entry[1], entry[2]
                out.append((PANEL, "  " + key.ljust(22) + text))
            elif kind == "t":
                for part in _wrap(entry[1], width - 2):
                    out.append((MUTED, "  " + part))
            else:
                out.append((PANEL, ""))
        return out


def _wrap(text, width):
    words = text.split()
    lines = []
    cur = ""
    for wd in words:
        if cur and len(cur) + 1 + len(wd) > width:
            lines.append(cur)
            cur = wd
        else:
            cur = (cur + " " + wd) if cur else wd
    if cur:
        lines.append(cur)
    return lines
