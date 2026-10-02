"""Einblendungen: Debug-Anzeige, Minikarte, Starthinweis und kurze Meldungen.

Alle Einblendungen werden als Overlay-Segmente {zeile: [(spalte, text, laenge)]}
an den Renderer uebergeben und dort in den Frame eingesetzt.
V2: Die ausfuehrliche Hilfe befindet sich im Menue (ESC -> Hilfe).
"""

import math

from .generator import S
from .roomtypes import CATEGORY_NAMES
from .rooms import Door
from .world import CS
from .zones import ZONES

TEXT = "\x1b[38;2;222;220;205;48;2;12;12;14m"
DIM = "\x1b[38;2;150;150;140;48;2;12;12;14m"
ACCENT = "\x1b[38;2;255;214;120;48;2;12;12;14m"

HINT = "  W A S D  gehen   ·   Maus  umsehen   ·   Shift  sprinten   ·   ESC  Menü & Hilfe  "


def _seg(overlays, row, col, text, style=TEXT):
    overlays.setdefault(row, []).append((col, style + text, len(text)))


class Hud:
    def __init__(self):
        self.debug = False
        self.map = False
        self.help_until = 7.0
        self.message = ""
        self.message_until = 0.0
        self.zone_title = ""
        self.zone_until = 0.0
        self._zone = None

    def toast(self, text, now, dur=2.2):
        self.message = text
        self.message_until = now + dur

    def track_zone(self, room, now):
        """Blendet dezent den Namen einer neu betretenen Zone ein."""
        if room is None:
            return
        z = room.zone
        if self._zone is None:
            self._zone = z
            return
        if z != self._zone and room.zone_w[z] > 0.6:
            self._zone = z
            self.zone_title = ZONES[z].name
            self.zone_until = now + 3.5

    # --- Aufbau -----------------------------------------------------------------------
    def build(self, game, cols, rows, now):
        ov = {}
        if self.debug:
            self._debug(ov, game, cols)
        if self.map:
            self._minimap(ov, game, cols, rows)
        if now < self.help_until:
            _seg(ov, rows - 2, max(0, (cols - len(HINT)) // 2), HINT, ACCENT)
        elif now < self.message_until:
            t = " " + self.message + " "
            _seg(ov, rows - 2, max(0, (cols - len(t)) // 2), t, ACCENT)
        if now < self.zone_until:
            t = "  " + self.zone_title.upper() + "  "
            _seg(ov, rows - 4, 2, t, DIM)
        # Nichts ueber den Bildschirmrand hinaus schreiben
        for r in list(ov):
            if r < 0 or r >= rows:
                del ov[r]
                continue
            ov[r] = [(c, txt[: len(txt) - max(0, c + vis - cols)], min(vis, cols - c))
                     for c, txt, vis in ov[r] if c < cols]
        return ov

    def _debug(self, ov, game, cols):
        p = game.player
        w = game.world
        cell = w.cell(int(math.floor(p.x)), int(math.floor(p.y)))
        owner = cell[6] if cell else None
        lines = [
            "FPS %5.1f   Raycast %4.1f ms   Ausgabe %4.1f ms   %dx%d Pixel" % (
                game.fps, game.t_render * 1000, game.t_present * 1000, game.W, game.H),
            "Position x=%.2f y=%.2f z=%.2f   Blick %3d Grad   Neigung %+.2f" % (
                p.x, p.y, p.z, int(math.degrees(p.angle)) % 360, p.pitch),
            "Seed %d   Sektor (%d,%d)   Chunk (%d,%d)" % (
                w.seed, int(p.x) // S, int(p.y) // S, int(math.floor(p.x)) // CS, int(math.floor(p.y)) // CS),
        ]
        if isinstance(owner, Door):
            lines.append("Aktuell: %s  Schwelle %.2f m  Sturz %.2f m  Breite %d" % (
                owner.describe(), owner.sill, owner.top, owner.width))
        elif owner is not None:
            r = owner
            lines.append("Raum %s: %s [%s]  %dx%d  Hoehe %.1f m  Boden %.1f m" % (
                "%d,%d#%d" % r.rid, r.type.label, CATEGORY_NAMES[r.type.category], r.w, r.h, r.height, r.floor))
            zs = sorted(range(len(ZONES)), key=lambda i: -r.zone_w[i])
            lines.append("Zone: %s %d%%  / %s %d%%   Licht %s  Grundhelligkeit %.2f" % (
                ZONES[zs[0]].name, r.zone_w[zs[0]] * 100, ZONES[zs[1]].name, r.zone_w[zs[1]] * 100,
                r.light_style, r.ambient))
            lines.append("Tueren %d   Verbindungen %d   davon geladen %d   Raum-Seed %016x" % (
                len(r.doors), len({id(n) for n in r.neighbors if n is not None}),
                len(w.loaded_neighbors(r)), r.seed))
        lines.append("Geladen: %d Sektoren  %d Raeume  %d Chunks   erzeugt %d   DDA %d" % (
            len(w.sectors), w.loaded_rooms(), len(w.chunks), w.generated_chunks, game.raycaster.steps))
        lines.append("Modus %s  Farben %s  Tempo %.1f m/s  Bobbing %s  Sicht %.0f m  Zeilen gesendet %d/%d" % (
            game.renderer.mode, game.renderer.colors, p.walk_speed, "an" if p.bob_enabled else "aus",
            game.shader.max_dist, game.renderer.rows_sent, game.rows))
        ms = game.settings
        lines.append("Maus %s  Fenster %s  Empf. %.1f (x%.1f / y%.1f)  Glaettung %d%%  letzte Bewegung %s" % (
            "an" if ms["mouse_enabled"] else "aus", "erkannt" if game.mouse.window_found else "nicht erkannt",
            ms["mouse_sensitivity"], ms["mouse_x"], ms["mouse_y"], ms["mouse_smoothing"] * 100,
            game.mouse.last_raw))
        width = min(cols, max(len(l) for l in lines) + 2)
        for i, l in enumerate(lines):
            _seg(ov, i, 0, (" " + l).ljust(width)[:cols])

    def _minimap(self, ov, game, cols, rows):
        """Draufsicht (Norden oben) aus Halbblock-Zeichen: 1 Zeichen = 1x2 Kacheln."""
        p = game.player
        w = game.world
        mw = min(49, cols // 3) | 1
        mh = min(21, rows // 2 - 1) | 1
        px = int(math.floor(p.x))
        py = int(math.floor(p.y))
        x0 = px - mw // 2
        y0 = py - mh
        fx = px + int(round(math.cos(p.angle) * 2.0))
        fy = py + int(round(math.sin(p.angle) * 2.0))
        bank = w.bank
        cells = w.cells

        def color(x, y):
            if x == px and y == py:
                return (255, 70, 60)
            if x == fx and y == fy:
                return (255, 170, 90)
            cc = cells.get((x >> 4, y >> 4))
            if cc is None:
                return (0, 0, 0)
            c = cc[((y & 15) << 4) | (x & 15)]
            if c is None:
                return (22, 22, 26)
            if isinstance(c[6], Door):
                return (205, 150, 70)
            r, g, b = bank[c[2]].rgb
            k = 0.35 + 0.45 * min(1.0, c[5]) + max(-0.2, min(0.0, c[0] * 0.06))
            if c[0] > c[6].floor + 0.3:
                k *= 0.6       # Einbauten / Podeste dunkler
            return (int(r * 255 * k), int(g * 255 * k), int(b * 255 * k))

        left = cols - mw - 1
        _seg(ov, 0, left - 1, (" KARTE  %d,%d " % (px, py)).ljust(mw + 2)[: mw + 2], ACCENT)
        for row in range(mh):
            parts = []
            ty = y0 + row * 2
            for col in range(mw):
                tx = x0 + col
                a = color(tx, ty)
                b = color(tx, ty + 1)
                parts.append("\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm▀" % (a + b))
            ov.setdefault(row + 1, []).append((left, "".join(parts), mw))
