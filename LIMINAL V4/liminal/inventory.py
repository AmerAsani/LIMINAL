"""Inventar (V3) im Stil von Minecraft.

36 Plaetze: 0-8 sind die Schnellleiste (unten im Bild, Auswahl mit 1-9 oder
Mausrad), 9-35 das Hauptinventar (Taste Tab). Gleiche Items stapeln sich.
"""

from .items import ITEMS

SIZE = 36
HOTBAR = 9
MAX_STACK = 16


class Inventory:
    def __init__(self):
        self.slots = [None] * SIZE      # je Platz: [art, anzahl] oder None
        self.selected = 0               # gewaehlter Platz der Schnellleiste

    def add(self, kind):
        """Legt ein Item ab (erst auf passende Stapel, dann in freie Plaetze)."""
        for s in self.slots:
            if s and s[0] == kind and s[1] < MAX_STACK:
                s[1] += 1
                return True
        for i in range(SIZE):
            if self.slots[i] is None:
                self.slots[i] = [kind, 1]
                return True
        return False

    def take(self, idx):
        """Nimmt ein Item aus einem Platz. Liefert die Art oder None."""
        s = self.slots[idx]
        if not s:
            return None
        s[1] -= 1
        if s[1] <= 0:
            self.slots[idx] = None
        return s[0]

    def swap(self, a, b):
        sa, sb = self.slots[a], self.slots[b]
        if sa and sb and sa[0] == sb[0] and a != b:
            move = min(MAX_STACK - sb[1], sa[1])
            sb[1] += move
            sa[1] -= move
            if sa[1] <= 0:
                self.slots[a] = None
            return
        self.slots[a], self.slots[b] = sb, sa

    def count(self, kind=None):
        return sum(s[1] for s in self.slots if s and (kind is None or s[0] == kind))

    def selected_item(self):
        s = self.slots[self.selected]
        return ITEMS[s[0]] if s else None

    def to_list(self):
        return [list(s) if s else None for s in self.slots]

    def from_list(self, data):
        self.slots = [None] * SIZE
        for i, s in enumerate(data[:SIZE]):
            if s and s[0] in ITEMS and int(s[1]) > 0:
                self.slots[i] = [s[0], min(MAX_STACK, int(s[1]))]
