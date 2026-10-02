"""Spieleroberflaeche (V3): Schnellleiste, Werte, Inventarfenster, Todesbildschirm.

Aufbau im Stil von Minecraft: unten mittig die Schnellleiste mit 9 Plaetzen,
darueber links Health und rechts Sanity. Tab oeffnet das Inventar (27 Plaetze
+ Schnellleiste). Farben und Rahmen folgen dem warmen Stil der uebrigen
Einblendungen und des Menues.
"""

import math

from .inventory import HOTBAR, SIZE
from .items import ITEMS
from .menu import HEAD, MUTED, PANEL, RULE, TITLE, VALUE

SLOT_BG = (26, 24, 20)
SEL_BG = (92, 74, 34)
BORDER = (82, 74, 54)
SEL_BORDER = (246, 205, 112)
HEALTH = (214, 62, 52)
SANITY = (176, 156, 236)
LOW = (236, 84, 96)
EMPTY = (60, 56, 48)
BAR_BG = (15, 14, 11)


def _fg(c):
    return "\x1b[38;2;%d;%d;%dm" % c


def _bg(c):
    return "\x1b[48;2;%d;%d;%dm" % c


def _pal(kind):
    return {ch: tuple(int(v * 255) for v in rgb) for ch, rgb in ITEMS[kind].palette.items()}


def icon_rows(pixels, palette, bg):
    """Pixelbild -> Terminalzeilen mit ▀ (je Zeichen 1x2 Pixel)."""
    out = []
    for r in range(0, len(pixels), 2):
        top, bot = pixels[r], pixels[r + 1]
        parts = []
        for c in range(len(top)):
            ct = palette.get(top[c], bg)
            cb = palette.get(bot[c], bg)
            parts.append("\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm▀" % (ct + cb))
        out.append("".join(parts))
    return out


def _seg(ov, row, col, text, vis):
    ov.setdefault(row, []).append((col, text, vis))


def _bar(label, value, color, width, label_color):
    filled = int(round(value / 100.0 * width))
    s = _bg(BAR_BG) + _fg(label_color) + label + " "
    s += _fg(color) + "█" * filled + _fg(EMPTY) + "░" * (width - filled)
    s += _fg(label_color) + " %3d%%" % round(value)
    return s, len(label) + 1 + width + 5


# --------------------------------------------------------------------------------------
def player_hud(ov, game, cols, rows, now):
    """Schnellleiste, Health/Sanity, Hinweise. Schreibt in die Overlay-Tabelle."""
    inv = game.inventory
    st = game.stats
    slot_w = 7
    total = HOTBAR * slot_w + 1
    left = max(0, (cols - total) // 2)
    y_icon = rows - 3
    y_num = rows - 1
    if rows < 12 or cols < total:
        return
    # --- Schnellleiste
    for line in range(3):
        parts = []
        for i in range(HOTBAR):
            s = inv.slots[i]
            sel = i == inv.selected
            bg = SEL_BG if sel else SLOT_BG
            bc = SEL_BORDER if sel or (i > 0 and inv.selected == i - 1) else BORDER
            parts.append(_bg(BAR_BG) + _fg(bc) + "│")
            if line < 2:
                if s:
                    ico = icon_rows(ITEMS[s[0]].icon, _pal(s[0]), bg)[line]
                else:
                    ico = _bg(bg) + "    "
                parts.append(_bg(bg) + " " + ico + _bg(bg) + " ")
            else:
                cnt = ("×%d" % s[1]) if s and s[1] > 1 else ""
                parts.append(_bg(bg) + _fg(SEL_BORDER if sel else (150, 140, 116)) + str(i + 1) + ("%5s" % cnt))
        bc = SEL_BORDER if inv.selected == HOTBAR - 1 else BORDER
        parts.append(_bg(BAR_BG) + _fg(bc) + "│")
        _seg(ov, y_icon + line, left, "".join(parts), total)

    # --- Health (links) und Sanity (rechts) ueber der Schnellleiste
    hcol = LOW if st.health < 35 else HEALTH
    text, vis = _bar("♥", st.health, hcol, 12, (230, 200, 190))
    _seg(ov, rows - 4, left, text, vis)
    pulse = st.sanity < 20 and int(now * 3) % 2 == 0
    scol = LOW if pulse or st.sanity <= 0 else SANITY
    text, vis = _bar("SANITY", st.sanity, scol, 12, (210, 200, 236))
    _seg(ov, rows - 4, left + total - vis, text, vis)
    # Tempobonus in der Mitte
    if st.speed_left > 0:
        t = " ⚡ +%d %%  %d:%02d " % (round(st.speed_bonus * 100), int(st.speed_left) // 60, int(st.speed_left) % 60)
        _seg(ov, rows - 4, left + (total - len(t)) // 2, _bg(BAR_BG) + _fg((255, 214, 120)) + t, len(t))

    # --- Hinweise ueber der Leiste
    cd = st.death_countdown
    if cd is not None and not st.dead:
        t = "  Du verlierst den Verstand ...  %d s  " % math.ceil(cd)
        _seg(ov, rows - 6, max(0, (cols - len(t)) // 2), _bg((40, 6, 6)) + _fg((255, 120, 110)) + t, len(t))
    elif game.target_item is not None:
        t = "  [E]  %s aufnehmen  " % game.target_item.type.name
        _seg(ov, rows - 6, max(0, (cols - len(t)) // 2), _bg(BAR_BG) + _fg((255, 226, 160)) + t, len(t))
    if now < game.saved_until:
        t = " ◆ Gespeichert "
        _seg(ov, 0, max(0, cols - len(t) - 1), _bg(BAR_BG) + _fg((170, 164, 140)) + t, len(t))


# --------------------------------------------------------------------------------------
class InventoryScreen:
    """Inventarfenster (Tab). Cursor mit Pfeilen/WASD, Enter nimmt/legt ab, F benutzt."""

    def __init__(self, game):
        self.game = game
        self.open = False
        self.row = 3          # 0-2 Hauptinventar, 3 Schnellleiste
        self.col = 0
        self.hand = None      # [art, anzahl] - aufgenommener Stapel
        self.hand_from = None

    def slot_index(self, row=None, col=None):
        row = self.row if row is None else row
        col = self.col if col is None else col
        return col if row == 3 else HOTBAR + row * 9 + col

    def show(self):
        self.open = True
        self.row = 3
        self.col = self.game.inventory.selected

    def close(self):
        inv = self.game.inventory
        if self.hand:
            if self.hand_from is not None and inv.slots[self.hand_from] is None:
                inv.slots[self.hand_from] = self.hand
            else:
                for _ in range(self.hand[1]):
                    inv.add(self.hand[0])
            self.hand = None
            self.hand_from = None
        self.open = False

    def key(self, k, now):
        inv = self.game.inventory
        if k in ("tab", "esc", "backspace"):
            self.close()
            return
        if k in ("left", "a"):
            self.col = (self.col - 1) % 9
        elif k in ("right", "d"):
            self.col = (self.col + 1) % 9
        elif k in ("up", "w"):
            self.row = (self.row - 1) % 4
        elif k in ("down", "s"):
            self.row = (self.row + 1) % 4
        elif k in ("enter", "space"):
            idx = self.slot_index()
            cur = inv.slots[idx]
            if self.hand is None:
                if cur:
                    self.hand, self.hand_from = cur, idx
                    inv.slots[idx] = None
            else:
                if cur is None:
                    inv.slots[idx] = self.hand
                    self.hand = None
                    self.hand_from = None
                elif cur[0] == self.hand[0]:
                    from .inventory import MAX_STACK
                    move = min(MAX_STACK - cur[1], self.hand[1])
                    cur[1] += move
                    self.hand[1] -= move
                    if self.hand[1] <= 0:
                        self.hand = None
                        self.hand_from = None
                else:
                    inv.slots[idx], self.hand = self.hand, cur
                    self.hand_from = idx
        elif k == "f" and self.hand is None:
            self.game.consume_slot(self.slot_index(), now)
        elif k in "123456789" and len(k) == 1 and self.hand is None:
            inv.swap(self.slot_index(), int(k) - 1)

    # --- Darstellung ----------------------------------------------------------------
    def build(self, cols, rows):
        game = self.game
        inv = game.inventory
        st = game.stats
        big = cols >= 100 and rows >= 42
        sw, ih = (10, 4) if big else (7, 2)          # Slotbreite, Iconzeilen
        gw = 9 * sw + 1
        w = gw + 6
        lines = []

        def text(style, t=""):
            lines.append((style + t[:w].ljust(w), w))

        text(PANEL)
        title = "   I N V E N T A R"
        info = "♥ %d %%    SANITY %d %%   " % (round(st.health), round(st.sanity))
        text(TITLE, title.ljust(w - len(info)) + info)
        if st.speed_left > 0:
            text(MUTED, "   Energie: Tempo +%d %% noch %d s" % (round(st.speed_bonus * 100), st.speed_left))
        text(RULE, "   " + "─" * (w - 6))

        def grid_rows(row_ids):
            out = []
            for line in range(ih + 1):
                parts = [PANEL + "   "]
                for c in range(9):
                    r = row_ids
                    idx = c if r == 3 else HOTBAR + r * 9 + c
                    s = inv.slots[idx]
                    cur = (r == self.row and c == self.col)
                    bg = SEL_BG if cur else SLOT_BG
                    bcol = SEL_BORDER if cur or (c > 0 and r == self.row and self.col == c - 1) else BORDER
                    parts.append(_bg((15, 14, 11)) + _fg(bcol) + "│")
                    inner = sw - 1
                    if line < ih:
                        if s:
                            pix = ITEMS[s[0]].icon_big if big else ITEMS[s[0]].icon
                            ico = icon_rows(pix, _pal(s[0]), bg)[line]
                            pad = inner - len(pix[0])
                            parts.append(_bg(bg) + " " * (pad // 2) + ico + _bg(bg) + " " * (pad - pad // 2))
                        else:
                            parts.append(_bg(bg) + " " * inner)
                    else:
                        cnt = ("×%d" % s[1]) if s and s[1] > 1 else ""
                        lab = (str(c + 1) if r == 3 else "")
                        parts.append(_bg(bg) + _fg((150, 140, 116)) + lab + cnt.rjust(inner - len(lab)))
                last = (self.row == row_ids and self.col == 8)
                parts.append(_bg((15, 14, 11)) + _fg(SEL_BORDER if last else BORDER) + "│" + PANEL + "   ")
                out.append(("".join(parts), w))
            return out

        def sep(first="├", mid="┼", last="┤"):
            s = PANEL + "   " + _bg((15, 14, 11)) + _fg(BORDER) + first
            s += mid.join("─" * (sw - 1) for _ in range(9)) + last + PANEL + "   "
            lines.append((s, w))

        text(PANEL)
        sep("┌", "┬", "┐")
        for r in range(3):
            lines.extend(grid_rows(r))
            if r < 2:
                sep()
        sep("└", "┴", "┘")
        text(MUTED, "   Schnellleiste")
        sep("┌", "┬", "┐")
        lines.extend(grid_rows(3))
        sep("└", "┴", "┘")
        text(PANEL)
        # Beschreibung des Platzes unter dem Cursor
        s = inv.slots[self.slot_index()]
        if self.hand:
            it = ITEMS[self.hand[0]]
            text(HEAD, "   In der Hand: %s ×%d  -  Enter: ablegen / tauschen" % (it.name, self.hand[1]))
        elif s:
            it = ITEMS[s[0]]
            text(HEAD, "   %s ×%d" % (it.name, s[1]))
            text(VALUE, "   " + it.desc)
        else:
            text(MUTED, "   (leerer Platz)")
            text(PANEL)
        text(RULE, "   " + "─" * (w - 6))
        text(MUTED, "   ←↑↓→ bewegen    Enter nehmen / ablegen    F benutzen")
        text(MUTED, "   1-9 in die Schnellleiste legen    Tab schließen")
        text(PANEL)
        top = max(0, (rows - len(lines)) // 2)
        left = max(0, (cols - w) // 2)
        ov = {}
        for i, (t, vis) in enumerate(lines):
            if top + i < rows:
                ov.setdefault(top + i, []).append((left, t, min(vis, cols - left)))
        return ov


# --------------------------------------------------------------------------------------
def death_screen(game, cols, rows):
    st = game.stats
    w = max(40, min(cols - 4, 66))
    red = "\x1b[0;1;38;2;255;96;84;48;2;18;6;6m"
    body = "\x1b[0;38;2;230;200;190;48;2;18;6;6m"
    dim = "\x1b[0;38;2;160;120;110;48;2;18;6;6m"
    mins = int(st.play_time) // 60
    lines = [
        (body, ""),
        (red, "   D U   H A S T   D E N   V E R S T A N D   V E R L O R E N"),
        (dim, "   " + "─" * (w - 6)),
        (body, ""),
        (body, "   Spielzeit %d:%02d min   ·   %.0f m zurückgelegt" % (mins, int(st.play_time) % 60, game.player.distance)),
        (body, ""),
        (body, "   Enter   Letzten Spielstand laden" + ("" if game.has_save else "  (keiner vorhanden)")),
        (body, "   N       Neue Welt beginnen"),
        (body, "   ESC     Beenden"),
        (body, ""),
    ]
    if game.has_save:
        lines.insert(9, (dim, "   Beim Laden startest du mit voller Health und mindestens 30 % Sanity."))
    top = max(0, (rows - len(lines)) // 2)
    left = max(0, (cols - w) // 2)
    ov = {}
    for i, (style, t) in enumerate(lines):
        if top + i < rows:
            ov.setdefault(top + i, []).append((left, style + t[:w].ljust(w), w))
    return ov
