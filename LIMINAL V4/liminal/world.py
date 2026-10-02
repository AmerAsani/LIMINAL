"""World: Streaming von Sektoren und Chunks.

- Sektoren (S x S Kacheln) enthalten nur Raum-/Tuer-Metadaten und sind billig.
- Chunks (CS x CS Kacheln) enthalten die gerasterten Zellen fuer Raycaster und
  Kollision. Sie werden rund um den Spieler nachgeladen (mit Zeitbudget pro
  Frame) und in grosser Entfernung wieder verworfen.
- V2: Zu jedem Chunk gehoert ein Gitter aus Eckhelligkeiten. Jede Ecke mittelt
  das Licht der vier angrenzenden Kacheln und wird dunkler, je mehr davon Wand
  sind (Ambient Occlusion). Der Raycaster interpoliert dazwischen -> weiche
  Lichtverlaeufe, Kontaktschatten in Ecken und am Wandfuss.
- Weil alles deterministisch ist, liefert ein erneut geladener Chunk exakt
  dieselben Zellen wie beim ersten Besuch.
"""

import math
import time

from .generator import S, Generator
from .materials import MaterialBank
from .rng import hfloat

CS = 16                 # Chunkgroesse (Zweierpotenz, S muss Vielfaches sein)
CS_SHIFT = 4
CS_MASK = CS - 1
CM = CS + 1             # Eckpunkte je Chunkzeile

# Abdunklung einer Ecke je Anzahl angrenzender massiver Kacheln (0..4)
AO = (1.0, 0.86, 0.74, 0.64, 0.6)
WALL_AO = AO[2]         # gerade Wand = Referenz (Waende werden darauf normiert)


def _snoise(seed, t):
    """Glattes 1D-Rauschen in [0, 1) ueber die Zeit (keine Spruenge)."""
    i = math.floor(t)
    f = t - i
    a = hfloat(seed, i)
    b = hfloat(seed, i + 1)
    u = f * f * (3.0 - 2.0 * f)
    return a + (b - a) * u


class Chunk:
    __slots__ = ("key", "cells", "corners", "sector")

    def __init__(self, key, cells, corners, sector):
        self.key = key
        self.cells = cells
        self.corners = corners
        self.sector = sector


class World:
    def __init__(self, seed, load_radius=4, item_odds=(0.28, 0.80)):
        self.seed = seed
        self.bank = MaterialBank()
        self.gen = Generator(seed, self.bank, item_odds)
        self.sectors = {}
        self.chunks = {}
        self.cells = {}          # key -> Zellliste (schneller Zugriff fuer den Raycaster)
        self.corners = {}        # key -> Eckhelligkeiten ((CS+1)^2)
        self.load_radius = load_radius
        self.flicker = {}        # Room -> aktueller Lichtfaktor (nur flackernde Raeume)
        self.generated_chunks = 0
        self._peek_room = None
        self.picked = set()      # V3: IDs bereits aufgenommener Items (wird gespeichert)

    # --- Items (V3) ------------------------------------------------------------------
    def items_near(self, x, y, radius):
        """Noch nicht aufgenommene Items im Umkreis (nur geladene Sektoren)."""
        r2 = radius * radius
        out = []
        for sec in self.sectors.values():
            for room in sec.item_rooms:
                for it in room.items:
                    if it.id in self.picked:
                        continue
                    dx = it.x - x
                    dy = it.y - y
                    if dx * dx + dy * dy <= r2:
                        out.append(it)
        return out

    # --- Sektoren ------------------------------------------------------------------
    def sector(self, sx, sy):
        key = (sx, sy)
        sec = self.sectors.get(key)
        if sec is None:
            sec = self.gen.generate_sector(sx, sy)
            self.sectors[key] = sec
        return sec

    def sector_of(self, x, y):
        return self.sector(x // S, y // S)

    def room_at(self, x, y):
        return self.sector_of(x, y).room_at(x, y)

    def resolve_door(self, door):
        """Ergaenzt bei Portaltueren den Raum auf der anderen Sektorseite."""
        if not door.portal:
            return
        if door.remote_side == "a" and door.room_a is None:
            sec = self.sector(*door.remote)
            tx, ty = (door.x0 - 1, door.y0) if door.axis == 0 else (door.x0, door.y0 - 1)
            door.room_a = sec.room_at(tx, ty)
        elif door.remote_side == "b" and door.room_b is None:
            sec = self.sector(*door.remote)
            tx, ty = (door.x1, door.y0) if door.axis == 0 else (door.x0, door.y1)
            door.room_b = sec.room_at(tx, ty)

    # --- Chunks --------------------------------------------------------------------------
    def _peek_cell(self, x, y):
        """Zelle einer Kachel, ohne dafuer den ganzen Chunk zu erzeugen."""
        cells = self.cells.get((x >> CS_SHIFT, y >> CS_SHIFT))
        if cells is not None:
            return cells[((y & CS_MASK) << CS_SHIFT) | (x & CS_MASK)]
        r = self._peek_room
        if r is None or not r.contains(x, y):
            sec = self.sector(x // S, y // S)
            r = sec.room_at(x, y)
            if r is None:
                for d in sec.doors_in_rect(x, y, x + 1, y + 1):
                    self.resolve_door(d)
                    return d.cell()
                return None
            self._peek_room = r
        r.prepare(self)
        return r.tile(x, y)

    def _make_corners(self, key, cells):
        x0 = key[0] * CS
        y0 = key[1] * CS
        N = CS + 2
        grid = [None] * (N * N)
        peek = self._peek_cell
        for j in range(N):
            y = y0 - 1 + j
            inner_row = 0 < j < N - 1
            for i in range(N):
                if inner_row and 0 < i < N - 1:
                    grid[j * N + i] = cells[(j - 1) * CS + (i - 1)]
                else:
                    grid[j * N + i] = peek(x0 - 1 + i, y)
        out = [0.0] * (CM * CM)
        ao = AO
        for j in range(CM):
            base = j * N
            for i in range(CM):
                s = 0.0
                n = 0
                for c in (grid[base + i], grid[base + i + 1], grid[base + N + i], grid[base + N + i + 1]):
                    if c is not None:
                        s += c[5]
                        n += 1
                out[j * CM + i] = (s / n) * ao[4 - n] if n else 0.0
        return out

    def build_chunk(self, key):
        cx, cy = key
        x0 = cx * CS
        y0 = cy * CS
        sec = self.sector(x0 // S, y0 // S)
        cells = [None] * (CS * CS)
        for room in sec.rooms_in_rect(x0, y0, x0 + CS, y0 + CS):
            room.paint(cells, x0, y0, CS, self)
        for door in sec.doors_in_rect(x0, y0, x0 + CS, y0 + CS):
            self.resolve_door(door)
            door.paint(cells, x0, y0, CS)
        corners = self._make_corners(key, cells)
        chunk = Chunk(key, cells, corners, sec)
        self.chunks[key] = chunk
        self.cells[key] = cells
        self.corners[key] = corners
        self.generated_chunks += 1
        return cells

    def load_chunk_now(self, key):
        cells = self.cells.get(key)
        if cells is None:
            cells = self.build_chunk(key)
        return cells

    def cell(self, x, y):
        key = (x >> CS_SHIFT, y >> CS_SHIFT)
        cells = self.cells.get(key)
        if cells is None:
            cells = self.build_chunk(key)
        return cells[((y & CS_MASK) << CS_SHIFT) | (x & CS_MASK)]

    def update(self, px, py, budget_ms=5.0):
        """Laedt fehlende Chunks um den Spieler (naechste zuerst, mit Zeitbudget)
        und entlaedt ferne Chunks und Sektoren."""
        pcx = int(math.floor(px)) >> CS_SHIFT
        pcy = int(math.floor(py)) >> CS_SHIFT
        R = self.load_radius
        # Direkte Umgebung immer sofort
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                k = (pcx + dx, pcy + dy)
                if k not in self.cells:
                    self.build_chunk(k)
        missing = []
        for dy in range(-R, R + 1):
            for dx in range(-R, R + 1):
                k = (pcx + dx, pcy + dy)
                if k not in self.cells:
                    missing.append((dx * dx + dy * dy, k))
        if missing:
            missing.sort()
            t0 = time.perf_counter()
            limit = budget_ms / 1000.0
            for _, k in missing:
                self.build_chunk(k)
                if time.perf_counter() - t0 > limit:
                    break
        # Entladen
        U = R + 2
        far = [k for k in self.chunks if abs(k[0] - pcx) > U or abs(k[1] - pcy) > U]
        for k in far:
            del self.chunks[k]
            del self.cells[k]
            del self.corners[k]
        if far:
            self._peek_room = None
            psx = (pcx * CS) // S
            psy = (pcy * CS) // S
            for sk in [sk for sk in self.sectors if abs(sk[0] - psx) > 2 or abs(sk[1] - psy) > 2]:
                del self.sectors[sk]

    def preload(self, px, py):
        R = self.load_radius
        pcx = int(math.floor(px)) >> CS_SHIFT
        pcy = int(math.floor(py)) >> CS_SHIFT
        for dy in range(-R, R + 1):
            for dx in range(-R, R + 1):
                self.load_chunk_now((pcx + dx, pcy + dy))

    # --- Licht-Animation -----------------------------------------------------------------
    def update_flicker(self, t):
        """Lichtfaktoren flackernder Raeume (V2: weich, selten, ohne harte Spruenge)."""
        fl = {}
        for sec in self.sectors.values():
            for room in sec.rooms:
                if room.flicker == 0:
                    continue
                phase = (room.seed % 6283) / 1000.0
                if room.flicker == 1:
                    # kaum wahrnehmbares, langsames Pulsieren
                    f = 0.985 + 0.015 * math.sin(t * 1.6 + phase)
                else:
                    # alternde Leuchtstoffroehre: sanfte Einbrueche, die weich ein- und ausblenden
                    n = _snoise(room.seed, t * 1.1)
                    dip = (n - 0.62) / 0.38 if n > 0.62 else 0.0
                    f = (1.0 - 0.38 * dip * dip) * (0.975 + 0.025 * math.sin(t * 0.8 + phase))
                fl[room] = f
        self.flicker = fl

    # --- Statistik -------------------------------------------------------------------------
    def loaded_rooms(self):
        return sum(len(s.rooms) for s in self.sectors.values())

    def loaded_neighbors(self, room):
        """Bereits generierte Nachbarraeume (Sektor geladen)."""
        out = []
        for d in room.doors:
            other = d.other(room)
            if other is not None and other.sector in self.sectors:
                out.append(other)
        return out

    def find_spawn(self):
        """Startpunkt: moeglichst ein heller Flur nahe dem Ursprung."""
        sec = self.sector(0, 0)
        best = None
        cx, cy = S * 0.5, S * 0.5
        for room in sec.rooms:
            if room.floor != 0.0:
                continue
            score = math.hypot(room.center()[0] - cx, room.center()[1] - cy)
            if room.is_corridor:
                score -= 25.0
            if room.type.key in ("korridor", "buero", "zimmer"):
                score -= 10.0
            if best is None or score < best[0]:
                best = (score, room)
        room = best[1] if best else sec.rooms[0]
        rx, ry = room.center()
        # naechste begehbare Kachel suchen
        for rad in range(0, 12):
            for dy in range(-rad, rad + 1):
                for dx in range(-rad, rad + 1):
                    x = int(rx) + dx
                    y = int(ry) + dy
                    c = self.cell(x, y)
                    if c is not None and c[1] - c[0] >= 1.9:
                        angle = 0.0 if room.w >= room.h else math.pi * 0.5
                        return x + 0.5, y + 0.5, c[0], angle
        return rx, ry, 0.0, 0.0
