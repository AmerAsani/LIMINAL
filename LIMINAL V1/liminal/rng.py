"""Deterministische Zufallsfunktionen.

Alles in der Weltgenerierung basiert auf reinen Funktionen von (Seed, Koordinaten).
Dadurch entsteht ein Bereich beim erneuten Laden exakt identisch - unabhaengig
davon, in welcher Reihenfolge der Spieler die Welt erkundet hat.

Pythons eingebautes hash() ist pro Prozess randomisiert und random.Random kann
sich zwischen Versionen aendern, deshalb gibt es hier eigene Implementierungen.
"""

import math

MASK64 = 0xFFFFFFFFFFFFFFFF
_GOLDEN = 0x9E3779B97F4A7C15
_INV53 = 1.0 / 9007199254740992.0  # 2**-53


def _mix(z):
    """SplitMix64-Finalizer: verteilt Bits eines 64-Bit-Werts gleichmaessig."""
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
    return z ^ (z >> 31)


def _str_hash(text):
    """Stabiler FNV-1a-Hash fuer Strings."""
    h = 0xCBF29CE484222325
    for b in text.encode("utf-8"):
        h = ((h ^ b) * 0x100000001B3) & MASK64
    return h


def hash64(*values):
    """Kombiniert beliebig viele ints/Strings zu einem stabilen 64-Bit-Hash."""
    h = 0x6A09E667F3BCC909
    for v in values:
        if isinstance(v, str):
            v = _str_hash(v)
        h = _mix(((h ^ (v & MASK64)) + _GOLDEN) & MASK64)
    return h


def hfloat(*values):
    """Stabiler Pseudozufallswert in [0, 1) fuer die gegebenen Koordinaten."""
    return (hash64(*values) >> 11) * _INV53


class Rng:
    """Kleiner, schneller und versionsstabiler Zufallsgenerator (SplitMix64)."""

    __slots__ = ("state",)

    def __init__(self, seed):
        self.state = seed & MASK64

    def next64(self):
        self.state = (self.state + _GOLDEN) & MASK64
        return _mix(self.state)

    def random(self):
        return (self.next64() >> 11) * _INV53

    def uniform(self, a, b):
        return a + (b - a) * self.random()

    def randint(self, a, b):
        """Ganzzahl in [a, b] (beide inklusive)."""
        if b <= a:
            return a
        return a + self.next64() % (b - a + 1)

    def chance(self, p):
        return self.random() < p

    def choice(self, seq):
        return seq[self.next64() % len(seq)]

    def weighted(self, items, weights):
        """Waehlt ein Element proportional zu seinem Gewicht."""
        total = 0.0
        for w in weights:
            total += w
        if total <= 0.0:
            return items[0]
        r = self.random() * total
        for item, w in zip(items, weights):
            r -= w
            if r < 0.0:
                return item
        return items[-1]


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(seed, x, y):
    """Glattes 2D-Value-Noise in [0, 1)."""
    ix = math.floor(x)
    iy = math.floor(y)
    ux = _smooth(x - ix)
    uy = _smooth(y - iy)
    a = hfloat(seed, ix, iy)
    b = hfloat(seed, ix + 1, iy)
    c = hfloat(seed, ix, iy + 1)
    d = hfloat(seed, ix + 1, iy + 1)
    top = a + (b - a) * ux
    bottom = c + (d - c) * ux
    return top + (bottom - top) * uy


def fbm(seed, x, y, octaves=3):
    """Fraktales Rauschen (mehrere Oktaven Value-Noise), Ergebnis in [0, 1)."""
    total = 0.0
    amp = 1.0
    norm = 0.0
    freq = 1.0
    for i in range(octaves):
        total += value_noise(seed + i * 7919, x * freq, y * freq) * amp
        norm += amp
        amp *= 0.5
        freq *= 2.03
    return total / norm
