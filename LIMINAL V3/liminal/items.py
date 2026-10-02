"""Items (V3): Definitionen, Aussehen und in der Welt liegende Exemplare.

Items sind selten. Sie liegen ausschliesslich auf den Theken kleiner
Versorgungsraeume (siehe rooms.Room.plan_supply) - nie zufaellig auf dem Boden.
Jedes Exemplar hat eine stabile ID aus Sektor, Raum und Platz. Aufgenommene
IDs werden gespeichert, damit ein Item nach dem Neuladen der Gegend nicht
wieder auftaucht.

Pixelbilder: '.' = transparent, andere Zeichen = Farbe laut Palette.
"""


class ItemType:
    def __init__(self, key, name, desc, sanity=0.0, speed=0.0, speed_time=0.0,
                 size=(0.2, 0.2), sprite=(), icon=(), icon_big=(), palette=None):
        self.key = key
        self.name = name
        self.desc = desc
        self.sanity = sanity            # Prozentpunkte Sanity
        self.speed = speed              # +Anteil Laufgeschwindigkeit
        self.speed_time = speed_time    # Dauer des Tempobonus (s)
        self.size = size                # Breite/Hoehe des Sprites in Metern
        self.sprite = sprite
        self.icon = icon                # 4x4 Pixel (Schnellleiste)
        self.icon_big = icon_big        # 8x8 Pixel (Inventar)
        self.palette = palette or {}


ITEMS = {
    "energy": ItemType(
        "energy", "Energy Bar", "+20 % Geschwindigkeit (60 s) und +20 % Sanity",
        sanity=20.0, speed=0.20, speed_time=60.0, size=(0.22, 0.075),
        sprite=(".rrrrrrrr.",
                "ryyyyyyyyr",
                "ryykkyyyyr",
                ".rrrrrrrr."),
        icon=("....",
              "rrrr",
              "yyyy",
              "rrrr"),
        icon_big=("........",
                  "......rr",
                  "....rryr",
                  "..rryyrr",
                  "rryyrr..",
                  "ryrr....",
                  "rr......",
                  "........"),
        palette={"r": (0.80, 0.16, 0.10), "y": (0.98, 0.78, 0.22), "k": (0.35, 0.12, 0.06)}),
    "almond": ItemType(
        "almond", "Mandelwasser", "+35 % Sanity",
        sanity=35.0, size=(0.11, 0.26),
        sprite=("..c..",
                "..c..",
                ".www.",
                ".www.",
                "wwwww",
                "wlllw",
                "wlllw",
                "wlllw",
                "wwwww",
                "wwwww",
                "wwwww",
                ".www."),
        icon=(".cc.",
              "wwww",
              "wllw",
              "wwww"),
        icon_big=("...cc...",
                  "...cc...",
                  "..wwww..",
                  ".wwwwww.",
                  ".wllllw.",
                  ".wllllw.",
                  ".wwwwww.",
                  "..wwww.."),
        palette={"c": (0.25, 0.45, 0.85), "w": (0.90, 0.92, 0.90), "l": (0.86, 0.74, 0.52)}),
}


def random_kind(rng):
    """Energy Bar etwas haeufiger als Mandelwasser."""
    return "energy" if rng.random() < 0.55 else "almond"


class WorldItem:
    """Ein Item, das in der Welt liegt."""

    __slots__ = ("id", "kind", "x", "y", "z", "room")

    def __init__(self, iid, kind, x, y, z, room):
        self.id = iid
        self.kind = kind
        self.x = x
        self.y = y
        self.z = z
        self.room = room

    @property
    def type(self):
        return ITEMS[self.kind]
