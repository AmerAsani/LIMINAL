"""Raeume und Tueren.

Ein Room ist ein achsenparalleles Rechteck (Innenraum, Waende liegen ausserhalb)
mit Metadaten: Position, Groesse, Boden- und Deckenhoehe, Typ, Zone, Seed,
Tueren und Verbindungen. Die eigentlichen Kacheln werden erst beim Rastern eines
Chunks erzeugt (Room.paint). Alle Details (Saeulen, Regale, Treppen, Lampen)
sind reine Funktionen des Raum-Seeds - dadurch sieht ein Raum nach dem
Entladen und erneuten Laden exakt gleich aus.

Eine Kachelzelle ist ein Tupel:
    (boden, decke, boden_mat, decken_mat, wand_mat, licht, besitzer)
oder None fuer massive Wand.
"""

import math

from .rng import Rng, hfloat

SOLID = "solid"
BUCKET = 8  # Rastergroesse fuer die Suche nach Lichtquellen


class Door:
    """Durchgang durch eine Wand zwischen zwei Raeumen.

    axis 0: der Durchgang verlaeuft in x-Richtung durch eine senkrechte Wand,
            room_a liegt links (x < x0), room_b rechts (x >= x1).
    axis 1: der Durchgang verlaeuft in y-Richtung, room_a oben, room_b unten.
    Die Laenge des Durchgangs entspricht der Wandstaerke.
    """

    __slots__ = ("x0", "y0", "x1", "y1", "axis", "room_a", "room_b", "kind",
                 "sill", "top", "portal", "owned", "remote", "remote_side", "mats")

    def __init__(self, x0, y0, x1, y1, axis, room_a, room_b, kind="door"):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.axis = axis
        self.room_a = room_a
        self.room_b = room_b
        self.kind = kind
        self.sill = 0.0
        self.top = 2.2
        self.portal = False
        self.owned = True          # False: Durchgangskacheln liegen im Nachbarsektor
        self.remote = None         # Sektor-Koordinaten des entfernten Raums (Portale)
        self.remote_side = None    # "a" oder "b"
        self.mats = None           # (boden, rahmen) - vom Generator gesetzt

    @property
    def width(self):
        return (self.y1 - self.y0) if self.axis == 0 else (self.x1 - self.x0)

    def other(self, room):
        return self.room_b if room is self.room_a else self.room_a

    def inner(self, room):
        """Kacheln direkt vor der Tuer im gegebenen Raum und Richtung in den Raum."""
        if self.axis == 0:
            if room is self.room_a:
                return [(self.x0 - 1, y) for y in range(self.y0, self.y1)], (-1, 0)
            return [(self.x1, y) for y in range(self.y0, self.y1)], (1, 0)
        if room is self.room_a:
            return [(x, self.y0 - 1) for x in range(self.x0, self.x1)], (0, -1)
        return [(x, self.y1) for x in range(self.x0, self.x1)], (0, 1)

    def center(self):
        return ((self.x0 + self.x1) * 0.5, (self.y0 + self.y1) * 0.5)

    def describe(self):
        names = {"door": "Tuer", "wide": "Doppeltuer", "gate": "Tor", "opening": "Offener Uebergang"}
        return names.get(self.kind, "Durchgang") + (" (Sektorgrenze)" if self.portal else "")

    def paint(self, cells, cx0, cy0, cs, light):
        cell = (self.sill, self.top, self.mats[0], self.mats[1], self.mats[1], light, self)
        for y in range(max(self.y0, cy0), min(self.y1, cy0 + cs)):
            row = (y - cy0) * cs - cx0
            for x in range(max(self.x0, cx0), min(self.x1, cx0 + cs)):
                cells[row + x] = cell


class Room:
    """Ein Raum mit allen Metadaten und der Logik fuer seine Kacheln."""

    def __init__(self, rid, sector, index, x0, y0, x1, y1, rtype, zone_w, zone, seed, corridor):
        self.rid = rid
        self.sector = sector            # (sx, sy)
        self.index = index
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.type = rtype
        self.zone_w = zone_w
        self.zone = zone
        self.seed = seed
        self.is_corridor = corridor
        self.doors = []
        self.floor = 0.0
        self.desired_floor = 0.0
        self.fixed_floor = False
        self.height = 3.0
        self.stair_step = rtype.stair_step
        # Stil (vom Generator gesetzt)
        self.fmat = self.cmat = self.wmat = 0
        self.lampmat = self.walllampmat = self.stairmat = self.stairtop = 0
        self.mats = {}
        self.ambient = 0.5
        self.lamp_int = 0.8
        self.light_style = "panels"
        self.avg_light = 0.5
        self.fog = (0.1, 0.1, 0.1)
        self.fog_density = 0.05
        self.flicker = 0
        self._prepared = False

    # --- Geometrie ------------------------------------------------------------
    @property
    def w(self):
        return self.x1 - self.x0

    @property
    def h(self):
        return self.y1 - self.y0

    @property
    def ceil(self):
        return self.floor + self.height

    @property
    def axis(self):
        """Laengsachse: 0 = x, 1 = y."""
        return 0 if self.w >= self.h else 1

    def contains(self, x, y):
        return self.x0 <= x < self.x1 and self.y0 <= y < self.y1

    def center(self):
        return ((self.x0 + self.x1) * 0.5, (self.y0 + self.y1) * 0.5)

    def depth_from(self, door):
        """Ausdehnung des Raums entlang der Tuernormalen."""
        return self.w if door.axis == 0 else self.h

    @property
    def neighbors(self):
        return [d.other(self) for d in self.doors]

    def describe(self):
        return self.type.label

    # --- Vorbereitung (einmal pro Raum, lazy) ---------------------------------
    def prepare(self, world):
        if self._prepared:
            return
        self._prepared = True
        rng = Rng(self.seed ^ 0x5EED)
        self._cells = {}
        self._stairs = {}
        self._clear = set()
        for door in self.doors:
            world.resolve_door(door)
            self._plan_stairs(door)
            self._plan_clearance(door)
        self._plan_features(rng)
        self._plan_lights(rng)
        # Treppen einheitlich beleuchten -> sie lesen sich als ein Bauteil
        if self._stairs:
            keys = list(self._stairs)[:24]
            self._stair_light = sum(self.light_at(x, y) for x, y in keys) / len(keys)

    def _plan_stairs(self, door):
        diff = door.sill - self.floor
        if diff <= 1e-6:
            return
        n = max(1, int(math.ceil(diff / self.stair_step - 1e-6)) - 1)
        sub = diff / (n + 1)
        tiles, (dx, dy) = door.inner(self)
        # Treppe eine Kachel breiter als die Tuer (falls Platz)
        ext = list(tiles)
        if door.axis == 0:
            ext.append((tiles[0][0], tiles[0][1] - 1))
            ext.append((tiles[-1][0], tiles[-1][1] + 1))
        else:
            ext.append((tiles[0][0] - 1, tiles[0][1]))
            ext.append((tiles[-1][0] + 1, tiles[-1][1]))
        for k in range(n):
            hgt = door.sill - (k + 1) * sub
            for (tx, ty) in ext:
                x = tx + dx * k
                y = ty + dy * k
                if self.contains(x, y):
                    old = self._stairs.get((x, y))
                    if old is None or hgt > old:
                        self._stairs[(x, y)] = hgt

    def _plan_clearance(self, door):
        tiles, (dx, dy) = door.inner(self)
        depth = 3 + max(0, int((door.sill - self.floor) / self.stair_step) + 1)
        for (tx, ty) in tiles:
            for k in range(depth):
                for s in (-1, 0, 1):
                    if door.axis == 0:
                        self._clear.add((tx + dx * k, ty + s))
                    else:
                        self._clear.add((tx + s, ty + dy * k))

    # --- Einbauten --------------------------------------------------------------
    def _plan_features(self, rng):
        self._feat = []
        feats = self.type.features
        w, h = self.w, self.h
        if "pillars" in feats:
            self._grid_pillars(rng.randint(3, 5), 1, 2)
        if "pillars_sparse" in feats:
            self._grid_pillars(rng.randint(6, 8), 1, 3)
        if "thick_pillars" in feats:
            self._grid_pillars(rng.randint(6, 9), 2, 3)
        if "giant_pillars" in feats:
            self._grid_pillars(rng.randint(11, 15), 3, 4)
        if "colonnade" in feats and min(w, h) >= 4:
            self._feat.append(self._f_colonnade)
            self._col_step = rng.randint(2, 4)
        if "cubicles" in feats:
            self._feat.append(self._f_cubicles)
        if "shelves" in feats:
            self._shelf_h = rng.uniform(2.2, min(3.4, self.height - 0.6))
            self._shelf_seg = rng.randint(6, 10)
            self._feat.append(self._f_shelves)
        if "crates" in feats:
            self._feat.append(self._f_crates)
        if "machines" in feats and min(w, h) >= 5:
            self._feat.append(self._f_machines)
        if "podium" in feats:
            self._feat.append(self._f_podium)
        if "monolith" in feats:
            self._mono_h = min(self.height - 3.0, 16.0)
            self._feat.append(self._f_monolith)
        self._ceil_fn = []
        if "beams" in feats and self.height >= 3.4:
            self._beam_drop = rng.uniform(0.4, 0.9)
            self._beam_step = rng.randint(3, 5)
            self._ceil_fn.append(self._c_beams)
        if "vault" in feats:
            self._ceil_fn.append(self._c_vault)
        if "pipes" in feats and self.height >= 2.3:
            self._ceil_fn.append(self._c_pipes)

    def _grid_pillars(self, sp, th, margin):
        w, h = self.w, self.h
        if w < 2 * margin + th or h < 2 * margin + th:
            return
        ox = margin + ((w - 2 * margin - th) % sp) // 2
        oy = margin + ((h - 2 * margin - th) % sp) // 2

        def f(lx, ly, sp=sp, th=th, ox=ox, oy=oy, margin=margin, w=w, h=h):
            if margin <= lx < w - margin and margin <= ly < h - margin:
                if (lx - ox) % sp < th and (ly - oy) % sp < th and lx >= ox and ly >= oy:
                    return SOLID
            return None
        self._feat.append(f)

    def _across_along(self, lx, ly):
        if self.axis == 0:
            return ly, lx, self.h, self.w
        return lx, ly, self.w, self.h

    def _f_colonnade(self, lx, ly):
        a, b, wa, la = self._across_along(lx, ly)
        if (a == 1 or a == wa - 2) and 1 <= b < la - 1 and b % self._col_step == 1:
            return SOLID
        return None

    def _f_cubicles(self, lx, ly):
        w, h = self.w, self.h
        if 2 <= lx < w - 3 and 2 <= ly < h - 3:
            i = (lx - 2) % 5
            j = (ly - 2) % 5
            if (j == 0 and i <= 3) or (i == 0 and j <= 2):
                return (1.35, self.mats["part"], self.mats["parttop"])
        return None

    def _f_shelves(self, lx, ly):
        a, b, wa, la = self._across_along(lx, ly)
        if 2 <= a < wa - 2 and (a - 2) % 3 == 0 and 2 <= b < la - 2:
            if (b - 2) % self._shelf_seg != self._shelf_seg - 1:
                return (self._shelf_h, self.mats["shelf"], self.mats["shelftop"])
        return None

    def _f_crates(self, lx, ly):
        if not (2 <= lx < self.w - 2 and 2 <= ly < self.h - 2):
            return None
        ci, ri = divmod(lx - 2, 6)
        cj, rj = divmod(ly - 2, 6)
        s = self.seed
        if hfloat(s, 11, ci, cj) > 0.45:
            return None
        cw = 1 + int(hfloat(s, 12, ci, cj) * 3)
        ch = 1 + int(hfloat(s, 13, ci, cj) * 3)
        ox = 1 + int(hfloat(s, 14, ci, cj) * (4 - cw))
        oy = 1 + int(hfloat(s, 15, ci, cj) * (4 - ch))
        if ox <= ri < ox + cw and oy <= rj < oy + ch and lx < self.w - 2 and ly < self.h - 2:
            hgt = 0.9 + hfloat(s, 16, ci, cj) * 1.4
            return (hgt, self.mats["crate"], self.mats["cratetop"])
        return None

    def _f_machines(self, lx, ly):
        if not (1 <= lx < self.w - 1 and 1 <= ly < self.h - 1):
            return None
        ci, ri = divmod(lx - 1, 4)
        cj, rj = divmod(ly - 1, 4)
        s = self.seed
        if hfloat(s, 21, ci, cj) > 0.55:
            return None
        if 1 <= ri <= 2 and 1 <= rj <= 1 + int(hfloat(s, 22, ci, cj) * 2) and lx < self.w - 1 and ly < self.h - 1:
            hgt = 0.8 + hfloat(s, 23, ci, cj) * 1.0
            return (hgt, self.mats["machine"], self.mats["machinetop"])
        return None

    def _f_podium(self, lx, ly):
        w, h = self.w, self.h
        x0, x1 = w // 4, w - w // 4
        y0, y1 = h // 4, h - h // 4
        if x0 <= lx < x1 and y0 <= ly < y1:
            edge = lx == x0 or lx == x1 - 1 or ly == y0 or ly == y1 - 1
            return (0.45 if edge else 0.9, self.mats["stage"], self.mats["stagetop"])
        return None

    def _f_monolith(self, lx, ly):
        mw = max(2, self.w // 6)
        mh = max(2, self.h // 6)
        cx = self.w // 2
        cy = self.h // 2
        if cx - mw // 2 <= lx < cx - mw // 2 + mw and cy - mh // 2 <= ly < cy - mh // 2 + mh:
            return (self._mono_h, self.mats["mono"], self.mats["mono"])
        return None

    def _c_beams(self, lx, ly, ce, fl):
        a, b, wa, la = self._across_along(lx, ly)
        if b % self._beam_step == 0 and ce - self._beam_drop - fl >= 2.4:
            return ce - self._beam_drop
        return ce

    def _c_vault(self, lx, ly, ce, fl):
        a, b, wa, la = self._across_along(lx, ly)
        if wa < 2:
            return ce
        t = abs((a + 0.5) - wa * 0.5) / (wa * 0.5)
        side = max(2.1, self.height * 0.7)
        v = self.floor + side + (self.height - side) * (1.0 - t * t)
        return v if v - fl >= 2.0 else ce

    def _c_pipes(self, lx, ly, ce, fl):
        a, b, wa, la = self._across_along(lx, ly)
        if (a == 0 or a == wa - 1) and ce - 0.3 - fl >= 1.95:
            return ce - 0.3
        return ce

    # --- Licht ----------------------------------------------------------------------
    def _plan_lights(self, rng):
        """Legt Leuchten fest (deterministisch) und sortiert sie in ein Suchraster."""
        w, h = self.w, self.h
        style = self.light_style
        self._lamps = set()
        self._walllamps = set()
        fixtures = []
        inten = self.lamp_int
        dead = 0.06

        def grid(sx, sy, radius, margin=1):
            ox = margin + ((w - 2 * margin - 1) % sx) // 2
            oy = margin + ((h - 2 * margin - 1) % sy) // 2
            for ly in range(oy, h - margin, sy):
                for lx in range(ox, w - margin, sx):
                    if hfloat(self.seed, 31, lx, ly) < dead:
                        continue
                    self._lamps.add((lx, ly))
                    fixtures.append((self.x0 + lx + 0.5, self.y0 + ly + 0.5, inten, radius))

        if style == "panels":
            grid(rng.randint(3, 4), rng.randint(3, 4), 3.6)
        elif style == "panels_wide":
            grid(rng.randint(5, 7), rng.randint(5, 7), 5.5, 2)
        elif style == "panels_long":
            if self.axis == 0:
                grid(3, max(1, h), 3.4, 0)
            else:
                grid(max(1, w), 3, 3.4, 0)
        elif style == "pools":
            s = rng.randint(7, 9)
            grid(s, s, 5.5, 2)
        elif style == "emergency":
            grid(7, 7, 3.8, 1)
        elif style == "void":
            dead = 0.3
            grid(14, 14, 7.0, 3)
        elif style == "bright":
            grid(2, 2, 3.0, 0)
        elif style == "dark":
            cx, cy = w // 2, h // 2
            self._lamps.add((cx, cy))
            fixtures.append((self.x0 + cx + 0.5, self.y0 + cy + 0.5, 1.1, 3.8))
        elif style == "wall":
            # Wandleuchten entlang der Laengswaende
            step = rng.randint(5, 7)
            if self.axis == 0:
                for lx in range(2, w - 1, step):
                    for ly in ((0, h - 1) if h > 2 else (0,)):
                        if hfloat(self.seed, 32, lx, ly) < 0.12:
                            continue
                        self._walllamps.add((lx, ly))
                        fixtures.append((self.x0 + lx + 0.5, self.y0 + ly + 0.5, inten, 4.5))
            else:
                for ly in range(2, h - 1, step):
                    for lx in ((0, w - 1) if w > 2 else (0,)):
                        if hfloat(self.seed, 32, lx, ly) < 0.12:
                            continue
                        self._walllamps.add((lx, ly))
                        fixtures.append((self.x0 + lx + 0.5, self.y0 + ly + 0.5, inten, 4.5))

        buckets = {}
        for fx, fy, it, r in fixtures:
            key = (int(fx) // BUCKET, int(fy) // BUCKET)
            buckets.setdefault(key, []).append((fx, fy, it, 1.0 / (r * r)))
        self._fix = buckets

    def light_at(self, x, y):
        cx = x + 0.5
        cy = y + 0.5
        lum = self.ambient
        bx = x // BUCKET
        by = y // BUCKET
        fix = self._fix
        for kx in (bx - 1, bx, bx + 1):
            for ky in (by - 1, by, by + 1):
                lst = fix.get((kx, ky))
                if lst:
                    for fx, fy, it, inv in lst:
                        t = 1.0 - ((cx - fx) ** 2 + (cy - fy) ** 2) * inv
                        if t > 0.0:
                            lum += it * t * t
        # Licht, das durch Tueren aus Nachbarraeumen faellt
        for door in self.doors:
            dx, dy = door.center()
            d = math.hypot(cx - dx, cy - dy)
            if d < 5.5:
                other = door.other(self)
                ol = other.avg_light if other is not None else 0.4
                b = ol * 0.6 * (1.0 - d / 5.5)
                if b > lum:
                    lum = lum * 0.4 + b * 0.6 if b > lum * 1.2 else b
        return min(1.6, lum)

    # --- Rastern -------------------------------------------------------------------
    def tile(self, x, y):
        lx = x - self.x0
        ly = y - self.y0
        fl = self.floor
        fm, cm, wm = self.fmat, self.cmat, self.wmat
        st = self._stairs.get((x, y))
        if st is not None:
            fl = st
            fm = self.stairtop
            wm = self.stairmat
        elif (x, y) not in self._clear:
            for f in self._feat:
                r = f(lx, ly)
                if r is None:
                    continue
                if r is SOLID:
                    return None
                fl = self.floor + r[0]
                wm = r[1]
                fm = r[2]
                break
        ce = self.floor + self.height
        for c in self._ceil_fn:
            ce = c(lx, ly, ce, fl)
        if (lx, ly) in self._lamps:
            cm = self.lampmat
        if (lx, ly) in self._walllamps and st is None:
            wm = self.walllampmat
        lum = self._stair_light if st is not None else self.light_at(x, y)
        light = int(lum * 32.0 + 0.5) / 32.0
        key = (fl, ce, fm, cm, wm, light)
        cell = self._cells.get(key)
        if cell is None:
            cell = (fl, ce, fm, cm, wm, light, self)
            self._cells[key] = cell
        return cell

    def paint(self, cells, cx0, cy0, cs, world):
        self.prepare(world)
        for y in range(max(self.y0, cy0), min(self.y1, cy0 + cs)):
            row = (y - cy0) * cs - cx0
            for x in range(max(self.x0, cx0), min(self.x1, cx0 + cs)):
                cells[row + x] = self.tile(x, y)
