"""World: Streaming von Sektoren und Chunks.

- Sektoren (S x S Kacheln) enthalten nur Raum-/Tuer-Metadaten und sind billig.
- Chunks (CS x CS Kacheln) enthalten die gerasterten Zellen fuer Raycaster und
  Kollision. Sie werden rund um den Spieler nachgeladen (mit Budget pro Frame)
  und in grosser Entfernung wieder verworfen.
- Weil alles deterministisch ist, liefert ein erneut geladener Chunk exakt
  dieselben Zellen wie beim ersten Besuch.
"""

import math

from .generator import S, Generator
from .materials import MaterialBank
from .rng import hfloat

CS = 16                 # Chunkgroesse (Zweierpotenz, S muss Vielfaches sein)
CS_SHIFT = 4
CS_MASK = CS - 1


class Chunk:
    __slots__ = ("key", "cells", "sector")

    def __init__(self, key, cells, sector):
        self.key = key
        self.cells = cells
        self.sector = sector


class World:
    def __init__(self, seed, load_radius=4):
        self.seed = seed
        self.bank = MaterialBank()
        self.gen = Generator(seed, self.bank)
        self.sectors = {}
        self.chunks = {}
        self.cells = {}          # key -> Zellliste (schneller Zugriff fuer den Raycaster)
        self.load_radius = load_radius
        self.flicker = {}        # Room -> aktueller Lichtfaktor (nur flackernde Raeume)
        self.generated_chunks = 0

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
            a, b = door.room_a, door.room_b
            la = a.avg_light if a is not None else 0.4
            lb = b.avg_light if b is not None else 0.4
            light = int((la + lb) * 0.5 * 32.0 + 0.5) / 32.0
            door.paint(cells, x0, y0, CS, light)
        chunk = Chunk(key, cells, sec)
        self.chunks[key] = chunk
        self.cells[key] = cells
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

    def update(self, px, py, budget=2):
        """Laedt fehlende Chunks um den Spieler (naechste zuerst) und entlaedt ferne."""
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
            for _, k in missing[:budget]:
                self.build_chunk(k)
        # Entladen
        U = R + 2
        far = [k for k in self.chunks if abs(k[0] - pcx) > U or abs(k[1] - pcy) > U]
        for k in far:
            del self.chunks[k]
            del self.cells[k]
        if far:
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
        """Berechnet Lichtfaktoren flackernder Raeume in geladenen Chunks."""
        fl = {}
        # Raeume der geladenen Sektoren durchgehen (klein: wenige hundert)
        for sec in self.sectors.values():
            for room in sec.rooms:
                if room.flicker == 0:
                    continue
                if room.flicker == 1:
                    # leises Netzbrummen
                    f = 0.95 + 0.05 * math.sin(t * 7.0 + room.seed % 97)
                else:
                    # defekte Leuchtstoffroehre: meist an, gelegentlich Aussetzer
                    slot = int(t * 12.0)
                    r = hfloat(room.seed, slot)
                    if r < 0.12:
                        f = 0.25 + r
                    elif r < 0.2:
                        f = 0.7
                    else:
                        f = 1.0
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
