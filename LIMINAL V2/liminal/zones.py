"""Zonen: grossraeumige Architekturstile der Welt.

Jede Zone definiert Farbpalette, Licht, Nebel, typische Raumgroessen und
Deckenhoehen. Die Zugehoerigkeit eines Ortes zu einer Zone ergibt sich aus
sehr niederfrequentem Rauschen. Weil mehrere Zonen gleichzeitig ein Gewicht
haben koennen, entstehen weiche Uebergaenge: Farben werden gemischt und
Raumtypen werden anteilig aus beiden Zonen gezogen.
"""

import math

from .rng import fbm, hash64

OFFICE, INDUSTRIAL, TUNNEL, HALLS, MAINT = range(5)
ZONE_COUNT = 5


class Zone:
    """Parametersatz einer Zone (reine Daten)."""

    def __init__(self, zid, name, **p):
        self.id = zid
        self.name = name
        # Farben (RGB 0..1)
        self.wall = p["wall"]
        self.floor = p["floor"]
        self.ceil = p["ceil"]
        self.trim = p["trim"]          # Tuerrahmen / Sockelleisten
        self.lamp = p["lamp"]          # Farbe der Leuchtmittel
        self.fog = p["fog"]            # Nebelfarbe
        self.fog_density = p["fog_density"]
        # Licht
        self.ambient = p["ambient"]
        self.light_style = p["light_style"]
        # Muster
        self.wall_pattern = p["wall_pattern"]
        self.floor_pattern = p["floor_pattern"]
        self.ceil_pattern = p["ceil_pattern"]
        # BSP-Parameter
        self.min_leaf = p["min_leaf"]
        self.max_leaf = p["max_leaf"]
        self.big_chance = p["big_chance"]
        self.corridor_chance = p["corridor_chance"]
        self.corridor_width = p["corridor_width"]
        self.wall_thickness = p["wall_thickness"]
        self.loop_chance = p["loop_chance"]
        # Hoehen (Meter) je Kategorie: (min, max)
        self.h_room = p["h_room"]
        self.h_corridor = p["h_corridor"]
        self.h_big = p["h_big"]
        self.door_height = p["door_height"]
        self.sunken = p.get("sunken", 0.0)   # Wahrscheinlichkeit abgesenkter Boeden


ZONES = [
    Zone(OFFICE, "Bueroflure",
         wall=(0.86, 0.80, 0.54), floor=(0.56, 0.48, 0.30), ceil=(0.82, 0.80, 0.70),
         trim=(0.42, 0.30, 0.18), lamp=(1.00, 0.97, 0.84), fog=(0.20, 0.18, 0.10),
         fog_density=0.050, ambient=0.55, light_style="panels",
         wall_pattern="wallpaper", floor_pattern="carpet", ceil_pattern="tiles",
         min_leaf=3, max_leaf=13, big_chance=0.05, corridor_chance=0.50,
         corridor_width=(3, 4), wall_thickness=(1, 1), loop_chance=0.20,
         h_room=(2.6, 3.0), h_corridor=(2.5, 2.8), h_big=(4.0, 6.0), door_height=2.2),
    Zone(INDUSTRIAL, "Beton & Industrie",
         wall=(0.55, 0.56, 0.58), floor=(0.40, 0.40, 0.42), ceil=(0.34, 0.34, 0.37),
         trim=(0.62, 0.50, 0.20), lamp=(1.00, 0.74, 0.42), fog=(0.05, 0.055, 0.07),
         fog_density=0.045, ambient=0.22, light_style="pools",
         wall_pattern="blocks", floor_pattern="slabs", ceil_pattern="plain",
         min_leaf=4, max_leaf=22, big_chance=0.14, corridor_chance=0.32,
         corridor_width=(4, 6), wall_thickness=(1, 2), loop_chance=0.24,
         h_room=(3.2, 4.5), h_corridor=(3.4, 5.0), h_big=(9.0, 16.0), door_height=2.4),
    Zone(TUNNEL, "Unterirdische Tunnel",
         wall=(0.46, 0.42, 0.32), floor=(0.32, 0.30, 0.25), ceil=(0.36, 0.34, 0.29),
         trim=(0.30, 0.26, 0.20), lamp=(0.95, 0.92, 0.60), fog=(0.012, 0.016, 0.010),
         fog_density=0.075, ambient=0.12, light_style="wall",
         wall_pattern="bricks", floor_pattern="slabs", ceil_pattern="plain",
         min_leaf=3, max_leaf=15, big_chance=0.06, corridor_chance=0.58,
         corridor_width=(3, 4), wall_thickness=(2, 3), loop_chance=0.28,
         h_room=(2.8, 3.6), h_corridor=(2.4, 3.0), h_big=(5.0, 8.0), door_height=2.2,
         sunken=0.75),
    Zone(HALLS, "Leere Hallen",
         wall=(0.80, 0.82, 0.86), floor=(0.64, 0.66, 0.70), ceil=(0.72, 0.74, 0.78),
         trim=(0.50, 0.52, 0.56), lamp=(0.88, 0.94, 1.00), fog=(0.26, 0.28, 0.33),
         fog_density=0.022, ambient=0.50, light_style="panels_wide",
         wall_pattern="panels", floor_pattern="bigtiles", ceil_pattern="tiles",
         min_leaf=6, max_leaf=42, big_chance=0.40, corridor_chance=0.16,
         corridor_width=(5, 7), wall_thickness=(1, 2), loop_chance=0.32,
         h_room=(4.0, 6.0), h_corridor=(5.0, 8.0), h_big=(11.0, 24.0), door_height=2.8),
    Zone(MAINT, "Wartungsbereiche",
         wall=(0.36, 0.42, 0.37), floor=(0.27, 0.27, 0.27), ceil=(0.22, 0.23, 0.22),
         trim=(0.70, 0.55, 0.12), lamp=(1.00, 0.36, 0.20), fog=(0.035, 0.008, 0.006),
         fog_density=0.085, ambient=0.14, light_style="emergency",
         wall_pattern="panels", floor_pattern="grate", ceil_pattern="plain",
         min_leaf=3, max_leaf=9, big_chance=0.02, corridor_chance=0.62,
         corridor_width=(2, 3), wall_thickness=(1, 1), loop_chance=0.22,
         h_room=(2.1, 2.4), h_corridor=(2.0, 2.2), h_big=(3.2, 4.2), door_height=1.95),
]


def mix_rgb(colors, weights):
    r = g = b = 0.0
    for c, w in zip(colors, weights):
        r += c[0] * w
        g += c[1] * w
        b += c[2] * w
    return (r, g, b)


class ZoneField:
    """Liefert fuer jede Weltposition die Gewichte der fuenf Zonen.

    Die Gewichte stammen aus fuenf unabhaengigen Rauschfeldern, die per
    Softmax geschaerft werden: Meist dominiert eine Zone klar, dazwischen
    liegen breite Uebergangsbaender.
    """

    SCALE = 1.0 / 190.0
    SHARPNESS = 11.0

    def __init__(self, seed):
        self.seeds = [hash64(seed, "zone", i) & 0xFFFFFFFF for i in range(ZONE_COUNT)]

    def weights(self, x, y):
        s = self.SCALE
        vals = [fbm(zs, x * s + i * 3.7, y * s - i * 1.3, 2) for i, zs in enumerate(self.seeds)]
        # Rund um den Startpunkt bevorzugt die Bueroflure - ein vertrauter Anfang.
        r2 = (x * x + y * y) / (130.0 * 130.0)
        vals[OFFICE] += 0.8 * math.exp(-r2)
        # Hallen und Wartung etwas seltener als grosse, dominante Flaechen.
        vals[MAINT] -= 0.03
        top = max(vals)
        ex = [math.exp(self.SHARPNESS * (v - top)) for v in vals]
        total = sum(ex)
        return [e / total for e in ex]

    @staticmethod
    def dominant(weights):
        best = 0
        for i in range(1, ZONE_COUNT):
            if weights[i] > weights[best]:
                best = i
        return best

    @staticmethod
    def blend(weights, attr):
        """Gewichteter Mittelwert eines numerischen Zonenattributs."""
        total = 0.0
        for z, w in zip(ZONES, weights):
            total += getattr(z, attr) * w
        return total
