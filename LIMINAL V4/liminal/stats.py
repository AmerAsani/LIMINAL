"""Spielerwerte (V3): Health und Sanity.

Sanity sinkt stetig um 15 Prozentpunkte pro Minute. Bei 0 % faerbt sich der
Bildschirmrand zunehmend rot, Health faellt in 15 Sekunden auf 0 - dann stirbt
der Spieler. Steigt Sanity wieder (Item), erholt sich Health langsam und das
Rot blendet weich aus.
"""

from .items import ITEMS

SANITY_DRAIN = 15.0 / 60.0     # Prozentpunkte pro Sekunde (Medium)
DEATH_TIME = 15.0              # Sekunden bei 0 % Sanity bis zum Tod (Medium)
HEALTH_REGEN = 4.0             # Prozentpunkte pro Sekunde, solange Sanity > 0 (Medium)

# V4: Schwierigkeitsgrade. "items" = Chancen (kein Item, hoechstens 1 Item) je Versorgungsraum.
DIFFICULTY = {
    "easy":   {"label": "Easy", "drain": 10.0, "death": 25.0, "regen": 6.0, "items": (0.10, 0.55),
               "desc": "Sanity -10 %/min, 25 s bis zum Tod, deutlich mehr Items"},
    "medium": {"label": "Medium", "drain": 15.0, "death": 15.0, "regen": 4.0, "items": (0.28, 0.80),
               "desc": "Sanity -15 %/min, 15 s bis zum Tod, normale Item-Menge"},
    "hard":   {"label": "Hard", "drain": 22.0, "death": 10.0, "regen": 2.0, "items": (0.50, 0.92),
               "desc": "Sanity -22 %/min, 10 s bis zum Tod, wenige Items"},
}


class Stats:
    def __init__(self, difficulty="medium"):
        d = DIFFICULTY.get(difficulty, DIFFICULTY["medium"])
        self.drain = d["drain"] / 60.0
        self.death_time = d["death"]
        self.regen = d["regen"]
        self.health = 100.0
        self.sanity = 100.0
        self.zero_time = 0.0       # Sekunden bei 0 % Sanity
        self.red = 0.0             # Staerke des roten Rands (0..1), weich nachgefuehrt
        self.speed_left = 0.0      # verbleibende Sekunden Tempobonus
        self.speed_bonus = 0.0
        self.play_time = 0.0
        self.dead = False

    def update(self, dt):
        if self.dead:
            return
        self.play_time += dt
        self.sanity = max(0.0, self.sanity - self.drain * dt)
        if self.sanity <= 0.0:
            self.zero_time += dt
            self.health = max(0.0, 100.0 * (1.0 - self.zero_time / self.death_time))
            target = min(1.0, 0.25 + 0.75 * self.zero_time / self.death_time)
            if self.zero_time >= self.death_time:
                self.health = 0.0
                self.dead = True
        else:
            self.zero_time = 0.0
            self.health = min(100.0, self.health + self.regen * dt)
            target = 0.0
        # Rot weich nachfuehren (einblenden ~0.3 s, ausblenden ~1.5 s)
        k = min(1.0, dt * (3.0 if target > self.red else 0.7))
        self.red += (target - self.red) * k
        if self.speed_left > 0.0:
            self.speed_left = max(0.0, self.speed_left - dt)
            if self.speed_left == 0.0:
                self.speed_bonus = 0.0

    @property
    def speed_mul(self):
        return 1.0 + self.speed_bonus if self.speed_left > 0.0 else 1.0

    @property
    def death_countdown(self):
        return max(0.0, self.death_time - self.zero_time) if self.sanity <= 0.0 else None

    def consume(self, kind):
        """Wendet die Wirkung eines Items an und liefert eine kurze Meldung."""
        it = ITEMS[kind]
        parts = []
        if it.sanity:
            before = self.sanity
            self.sanity = min(100.0, self.sanity + it.sanity)
            parts.append("Sanity +%d %%" % round(self.sanity - before))
        if it.speed:
            self.speed_bonus = it.speed
            self.speed_left = min(3 * it.speed_time, self.speed_left + it.speed_time)
            parts.append("Tempo +%d %% (%d s)" % (round(it.speed * 100), round(self.speed_left)))
        return "%s: %s" % (it.name, ", ".join(parts))

    def to_dict(self):
        return {"health": self.health, "sanity": self.sanity, "zero_time": self.zero_time,
                "speed_left": self.speed_left, "speed_bonus": self.speed_bonus,
                "play_time": self.play_time}

    def from_dict(self, d):
        self.health = float(d.get("health", 100.0))
        self.sanity = float(d.get("sanity", 100.0))
        self.zero_time = float(d.get("zero_time", 0.0))
        self.speed_left = float(d.get("speed_left", 0.0))
        self.speed_bonus = float(d.get("speed_bonus", 0.0))
        self.play_time = float(d.get("play_time", 0.0))
        self.red = 0.0
        self.dead = False
