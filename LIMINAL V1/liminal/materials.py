"""Materialien: Farbe plus Oberflaechenmuster.

Die Weltgenerierung erzeugt Materialien nur ueber die MaterialBank. Dort werden
sie dedupliziert und bekommen eine kleine Integer-ID, die in den Kachelzellen
gespeichert wird. Der Renderer baut aus (Material-ID, Licht, Distanz) seine
Farbtabellen - Generierung und Darstellung bleiben so sauber getrennt.
"""

# Musterarten fuer Boden/Decke
PAT_NONE = 0
PAT_GRID = 1
PAT_CHECKER = 2

# Boden-/Deckenmuster: Name -> (Art, Rastergroesse in Metern, Linienbreite relativ)
PLANE_PATTERNS = {
    "none": (PAT_NONE, 1.0, 0.0),
    "plain": (PAT_NONE, 1.0, 0.0),
    "carpet": (PAT_GRID, 2.0, 0.035),
    "slabs": (PAT_GRID, 2.0, 0.05),
    "bigtiles": (PAT_GRID, 3.0, 0.035),
    "grate": (PAT_GRID, 0.5, 0.16),
    "tiles": (PAT_GRID, 1.0, 0.07),
    "checker": (PAT_CHECKER, 1.0, 0.0),
    "lines": (PAT_GRID, 4.0, 0.03),
}

# Wandmuster: Name -> (horizontaler Fugenabstand, vertikaler Fugenabstand)
WALL_PATTERNS = {
    "plain": (0.0, 0.0),
    "wallpaper": (0.0, 0.55),
    "blocks": (0.6, 1.2),
    "bricks": (0.3, 0.75),
    "panels": (0.0, 1.5),
    "concrete": (1.0, 2.0),
}


class Material:
    __slots__ = ("id", "rgb", "emissive", "kind", "scale", "line",
                 "hseam", "vseam", "bands", "seam_dark")

    def __init__(self, mid, rgb, emissive, kind, scale, line, hseam, vseam, bands, seam_dark):
        self.id = mid
        self.rgb = rgb
        self.emissive = emissive
        self.kind = kind
        self.scale = scale
        self.line = line
        self.hseam = hseam
        self.vseam = vseam
        self.bands = bands          # Tupel aus (z0, z1, material_id) relativ zum Boden
        self.seam_dark = seam_dark


def _q(v):
    """Quantisiert Farbkanaele, damit aehnliche Farben dasselbe Material teilen."""
    return round(max(0.0, min(1.0, v)) * 48.0) / 48.0


class MaterialBank:
    """Registry aller Materialien (dedupliziert)."""

    def __init__(self):
        self.items = []
        self._index = {}

    def get(self, rgb, plane="none", wall="plain", emissive=False, bands=(), seam_dark=0.72):
        rgb = (_q(rgb[0]), _q(rgb[1]), _q(rgb[2]))
        key = (rgb, plane, wall, emissive, bands, seam_dark)
        mid = self._index.get(key)
        if mid is not None:
            return mid
        kind, scale, line = PLANE_PATTERNS.get(plane, PLANE_PATTERNS["none"])
        hseam, vseam = WALL_PATTERNS.get(wall, (0.0, 0.0))
        mid = len(self.items)
        self.items.append(Material(mid, rgb, emissive, kind, scale, line, hseam, vseam, bands, seam_dark))
        self._index[key] = mid
        return mid

    def __getitem__(self, mid):
        return self.items[mid]

    def __len__(self):
        return len(self.items)
