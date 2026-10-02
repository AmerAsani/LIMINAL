"""Prozedurale Weltgenerierung (Graph + Sektoren).

Die unendliche Ebene ist in Sektoren von S x S Kacheln aufgeteilt. Jeder Sektor
wird unabhaengig von allen anderen nur aus (Seed, sx, sy) erzeugt:

1.  Portale: Auf jeder Sektorkante liegen 2-4 Durchgaenge, deren Position nur
    von der Kante selbst abhaengt. Beide angrenzenden Sektoren berechnen daher
    exakt dieselben Portale -> keine Tuer fuehrt ins Nichts.
2.  BSP-Teilung: Der Sektor wird rekursiv in Rechtecke geteilt. Zonenparameter
    (Raumgroesse, Korridorwahrscheinlichkeit, Wandstaerke) werden an jeder
    Stelle aus dem Zonenfeld gemischt. Teilungswaende duerfen niemals ein Portal
    treffen. Manche Teilungen fuegen einen Korridorstreifen ein.
3.  Raumtypen werden je Blatt gewaehlt (Groesse, Zone, Seltenheit).
4.  Tueren: Fuer jede Teilungswand wird mindestens ein Paar angrenzender Raeume
    verbunden (garantiert Zusammenhang, weil jeder Teilbaum zusammenhaengend
    ist). Weitere Tueren erzeugen Schleifen.
5.  Bodenhoehen: Manche Raeume sind abgesenkt. Eine Fixpunkt-Iteration hebt
    Raeume an, in denen eine Treppe keinen Platz haette - Treppen enden also
    immer auf der passenden Hoehe.
6.  Hoehen, Tuerhoehen, Materialien und Licht.

Rooms werden nur als Metadaten erzeugt; Kacheln entstehen erst in world.py.
"""

import math

from .rng import Rng, hash64, hfloat
from .rooms import Door, Room
from .roomtypes import LARGE, CORRIDOR, choose_type
from .zones import ZONES, ZoneField, TUNNEL, MAINT, OFFICE, HALLS, mix_rgb

S = 96  # Sektorgroesse in Kacheln (Vielfaches der Chunkgroesse)

_BIGGISH = ("kammer", "leere", "monolith", "podium", "niedriger_raum", "treppenhaus", "lager")


class Sector:
    """Ein Sektor: Liste von Raeumen und Tueren (reine Metadaten)."""

    def __init__(self, sx, sy):
        self.sx = sx
        self.sy = sy
        self.x0 = sx * S
        self.y0 = sy * S
        self.rooms = []
        self.doors = []
        self.item_rooms = []    # V3: Raeume mit Items
        self.supply_rooms = []  # V3: Versorgungsraeume mit Automat

    def room_at(self, x, y):
        for r in self.rooms:
            if r.x0 <= x < r.x1 and r.y0 <= y < r.y1:
                return r
        return None

    def rooms_in_rect(self, x0, y0, x1, y1):
        return [r for r in self.rooms if r.x0 < x1 and r.x1 > x0 and r.y0 < y1 and r.y1 > y0]

    def doors_in_rect(self, x0, y0, x1, y1):
        return [d for d in self.doors
                if d.owned and d.x0 < x1 and d.x1 > x0 and d.y0 < y1 and d.y1 > y0]


class _Leaf:
    __slots__ = ("x0", "y0", "x1", "y1", "corridor", "big", "endless")

    def __init__(self, x0, y0, x1, y1, corridor=False, big=False, endless=False):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.corridor = corridor
        self.big = big
        self.endless = endless


class _Ctx:
    """Arbeitskontext waehrend der Generierung eines Sektors."""

    def __init__(self, sector, rng, portals, rarity):
        self.sector = sector
        self.rng = rng
        self.portals = portals      # dict: "left"/"right"/"top"/"bottom" -> [(abs_pos, width)]
        self.rarity = rarity
        self.leaves = []
        self.walls = []


class Generator:
    def __init__(self, seed, bank, item_odds=(0.28, 0.80)):
        self.item_odds = item_odds      # V4: Item-Menge je Schwierigkeitsgrad
        self.seed = seed
        self.bank = bank
        self.zones = ZoneField(seed)
        self._portal_cache = {}

    # --- Portale ----------------------------------------------------------------
    def edge_portals(self, kind, ex, ey):
        """Portale einer Sektorkante als [(offset, breite)], offset relativ zur Kante.

        kind "v": senkrechte Kante am linken Rand von Sektor (ex, ey)
        kind "h": waagrechte Kante am oberen Rand von Sektor (ex, ey)
        """
        key = (kind, ex, ey)
        res = self._portal_cache.get(key)
        if res is not None:
            return res
        rng = Rng(hash64(self.seed, "portal", 0 if kind == "v" else 1, ex, ey))
        n = rng.randint(2, 4)
        res = []
        tries = 0
        while len(res) < n and tries < 40:
            tries += 1
            w = 1 if rng.random() < 0.7 else 2
            p = rng.randint(6, S - 9)
            if all(abs(p - q) >= 11 for q, _ in res):
                res.append((p, w))
        res.sort()
        if len(self._portal_cache) > 512:
            self._portal_cache.clear()
        self._portal_cache[key] = res
        return res

    def rarity(self, x, y):
        """Je weiter vom Ursprung entfernt, desto haeufiger Ungewoehnliches."""
        return 1.0 + min(1.6, math.hypot(x, y) / 700.0)

    # --- Sektor -------------------------------------------------------------------
    def generate_sector(self, sx, sy):
        sec = Sector(sx, sy)
        X0, Y0 = sec.x0, sec.y0
        portals = {
            "left": [(Y0 + p, w) for p, w in self.edge_portals("v", sx, sy)],
            "right": [(Y0 + p, w) for p, w in self.edge_portals("v", sx + 1, sy)],
            "top": [(X0 + p, w) for p, w in self.edge_portals("h", sx, sy)],
            "bottom": [(X0 + p, w) for p, w in self.edge_portals("h", sx, sy + 1)],
        }
        rng = Rng(hash64(self.seed, "sector", sx, sy))
        ctx = _Ctx(sec, rng, portals, self.rarity(X0 + S / 2, Y0 + S / 2))

        root = self._bsp(ctx, X0 + 1, Y0 + 1, X0 + S, Y0 + S, 0)
        self._make_rooms(ctx)
        self._collect(ctx, root)
        self._make_doors(ctx)
        self._make_portals(ctx)
        self._resolve_floors(sec)
        self._resolve_heights(sec, rng)
        for room in sec.rooms:
            self._style(room)
        for door in sec.doors:
            self._door_style(door)
        # V3: Versorgungsraeume mit Theke und (seltenen) Items
        for room in sec.rooms:
            if room.type.key == "versorgung":
                room.plan_supply(self.item_odds)
                room.avg_light = min(1.4, room.avg_light + 0.2)   # Licht faellt in den Flur
        sec.item_rooms = [r for r in sec.rooms if r.items]
        sec.supply_rooms = [r for r in sec.rooms if r._machine]
        return sec

    # --- BSP ------------------------------------------------------------------------
    def _wall_ok(self, ctx, axis, s, t, x0, y0, x1, y1):
        """Prueft, ob eine Teilungswand [s, s+t) kein Portal blockiert."""
        sec = ctx.sector
        if axis == 0:   # senkrechte Wand -> betrifft Portale oben/unten
            edges = []
            if y0 == sec.y0 + 1:
                edges += ctx.portals["top"]
            if y1 == sec.y0 + S:
                edges += ctx.portals["bottom"]
        else:           # waagrechte Wand -> betrifft Portale links/rechts
            edges = []
            if x0 == sec.x0 + 1:
                edges += ctx.portals["left"]
            if x1 == sec.x0 + S:
                edges += ctx.portals["right"]
        for p, w in edges:
            if not (s + t <= p - 1 or s >= p + w + 1):
                return False
        return True

    def _pick(self, ctx, axis, lo, hi, t, rect):
        rng = ctx.rng
        for _ in range(16):
            s = (rng.randint(lo, hi) + rng.randint(lo, hi)) // 2
            if self._wall_ok(ctx, axis, s, t, *rect):
                return s
        return None

    def _leaf(self, ctx, x0, y0, x1, y1, **flags):
        ctx.leaves.append(_Leaf(x0, y0, x1, y1, **flags))
        return ("leaf", len(ctx.leaves) - 1)

    def _bsp(self, ctx, x0, y0, x1, y1, depth):
        rng = ctx.rng
        w, h = x1 - x0, y1 - y0
        zw = self.zones.weights((x0 + x1) * 0.5, (y0 + y1) * 0.5)
        min_leaf = max(2, int(round(ZoneField.blend(zw, "min_leaf"))))
        max_leaf = ZoneField.blend(zw, "max_leaf")
        dom = ZONES[rng.weighted(list(range(5)), zw)]
        t = rng.randint(*dom.wall_thickness)

        # Sehr selten: ein Gang durch den gesamten Sektor ("endloser Gang")
        if depth == 0 and rng.random() < 0.07 * ctx.rarity:
            node = self._corridor_split(ctx, x0, y0, x1, y1, depth, t, rng.randint(3, 4),
                                        rng.randint(0, 1), min_leaf, endless=True)
            if node:
                return node

        if depth > 0 and w <= max_leaf and h <= max_leaf and rng.random() < 0.55:
            return self._leaf(ctx, x0, y0, x1, y1)
        if (depth > 0 and min(w, h) >= 12 and max(w, h) <= max(30.0, max_leaf * 1.5)
                and rng.random() < ZoneField.blend(zw, "big_chance")):
            return self._leaf(ctx, x0, y0, x1, y1, big=True)

        if w > h * 1.25:
            axis = 0
        elif h > w * 1.25:
            axis = 1
        else:
            axis = rng.randint(0, 1)
        for attempt in range(2):
            length = w if axis == 0 else h
            span = h if axis == 0 else w
            if length >= 2 * min_leaf + t:
                break
            axis = 1 - axis
        else:
            return self._leaf(ctx, x0, y0, x1, y1)

        # Korridorstreifen einfuegen (Flur mit Raeumen links und rechts)
        if (span >= 12 and length >= 2 * min_leaf + 2 * t + 2
                and rng.random() < ZoneField.blend(zw, "corridor_chance") * (1.0 if depth < 5 else 0.35)):
            cw = rng.randint(*dom.corridor_width)
            node = self._corridor_split(ctx, x0, y0, x1, y1, depth, t, cw, axis, min_leaf)
            if node:
                return node

        start = x0 if axis == 0 else y0
        end = x1 if axis == 0 else y1
        s = self._pick(ctx, axis, start + min_leaf, end - min_leaf - t, t, (x0, y0, x1, y1))
        if s is None:
            return self._leaf(ctx, x0, y0, x1, y1)
        if axis == 0:
            left = self._bsp(ctx, x0, y0, s, y1, depth + 1)
            right = self._bsp(ctx, s + t, y0, x1, y1, depth + 1)
        else:
            left = self._bsp(ctx, x0, y0, x1, s, depth + 1)
            right = self._bsp(ctx, x0, s + t, x1, y1, depth + 1)
        return ("split", axis, s, t, left, right)

    def _corridor_split(self, ctx, x0, y0, x1, y1, depth, t, cw, axis, min_leaf, endless=False):
        rng = ctx.rng
        start = x0 if axis == 0 else y0
        end = x1 if axis == 0 else y1
        lo = start + min_leaf
        hi = end - min_leaf - 2 * t - cw
        if hi < lo:
            return None
        rect = (x0, y0, x1, y1)
        for _ in range(16):
            s1 = (rng.randint(lo, hi) + rng.randint(lo, hi)) // 2
            s2 = s1 + t + cw
            if self._wall_ok(ctx, axis, s1, t, *rect) and self._wall_ok(ctx, axis, s2, t, *rect):
                break
        else:
            return None
        span = (y1 - y0) if axis == 0 else (x1 - x0)
        if axis == 0:
            left = self._bsp(ctx, x0, y0, s1, y1, depth + 1)
            corr = self._leaf(ctx, s1 + t, y0, s2, y1, corridor=True, endless=endless and span >= 60)
            right = self._bsp(ctx, s2 + t, y0, x1, y1, depth + 1)
        else:
            left = self._bsp(ctx, x0, y0, x1, s1, depth + 1)
            corr = self._leaf(ctx, x0, s1 + t, x1, s2, corridor=True, endless=endless and span >= 60)
            right = self._bsp(ctx, x0, s2 + t, x1, y1, depth + 1)
        return ("split", axis, s1, t, left, ("split", axis, s2, t, corr, right))

    # --- Raeume -----------------------------------------------------------------------
    def _make_rooms(self, ctx):
        sec = ctx.sector
        for i, lf in enumerate(ctx.leaves):
            cx = (lf.x0 + lf.x1) * 0.5
            cy = (lf.y0 + lf.y1) * 0.5
            zw = self.zones.weights(cx, cy)
            zone = ctx.rng.weighted(list(range(5)), zw)
            rtype = choose_type(ctx.rng, lf.x1 - lf.x0, lf.y1 - lf.y0, lf.corridor, lf.big,
                                lf.endless, zw, ctx.rarity)
            seed = hash64(self.seed, "room", sec.sx, sec.sy, i)
            room = Room((sec.sx, sec.sy, i), (sec.sx, sec.sy), i, lf.x0, lf.y0, lf.x1, lf.y1,
                        rtype, zw, zone, seed, lf.corridor or rtype.category == CORRIDOR)
            sec.rooms.append(room)

    def _collect(self, ctx, node):
        """Sammelt Blaetter je Teilbaum und die angrenzenden Paare je Teilungswand."""
        if node[0] == "leaf":
            return [node[1]]
        _, axis, s, t, left, right = node
        la = self._collect(ctx, left)
        lb = self._collect(ctx, right)
        L = ctx.leaves
        pairs = []
        if axis == 0:
            A = [i for i in la if L[i].x1 == s]
            B = [i for i in lb if L[i].x0 == s + t]
            for a in A:
                for b in B:
                    lo, hi = max(L[a].y0, L[b].y0), min(L[a].y1, L[b].y1)
                    if hi - lo >= 1:
                        pairs.append((a, b, lo, hi))
        else:
            A = [i for i in la if L[i].y1 == s]
            B = [i for i in lb if L[i].y0 == s + t]
            for a in A:
                for b in B:
                    lo, hi = max(L[a].x0, L[b].x0), min(L[a].x1, L[b].x1)
                    if hi - lo >= 1:
                        pairs.append((a, b, lo, hi))
        ctx.walls.append((axis, s, t, pairs))
        return la + lb

    # --- Tueren -----------------------------------------------------------------------
    def _door_spec(self, rng, ra, rb, overlap):
        big_a = ra.type.category == LARGE or ra.type.key in _BIGGISH
        big_b = rb.type.category == LARGE or rb.type.key in _BIGGISH
        if big_a and big_b and overlap >= 8 and rng.random() < 0.55:
            return "opening", min(overlap - 2, rng.randint(3, 7))
        if (big_a or big_b) and overlap >= 5 and rng.random() < 0.45:
            return "gate", min(overlap - 2, rng.randint(2, 3))
        if overlap >= 4 and ra.zone != MAINT and rb.zone != MAINT and rng.random() < 0.18:
            return "wide", 2
        return "door", 1

    def _add_door(self, sec, axis, s, t, ra, rb, p, wd, kind):
        if axis == 0:
            door = Door(s, p, s + t, p + wd, 0, ra, rb, kind)
        else:
            door = Door(p, s, p + wd, s + t, 1, ra, rb, kind)
        ra.doors.append(door)
        rb.doors.append(door)
        sec.doors.append(door)
        return door

    @staticmethod
    def _free(used, p, wd):
        for u0, u1 in used:
            if not (p + wd + 1 <= u0 or p >= u1 + 1):
                return False
        return True

    def _make_doors(self, ctx):
        rng = ctx.rng
        sec = ctx.sector
        rooms = sec.rooms
        for axis, s, t, pairs in ctx.walls:
            if not pairs:
                continue
            used = []
            good = [p for p in pairs if p[3] - p[2] >= 3]
            if good:
                weights = [3.0 if (rooms[a].is_corridor or rooms[b].is_corridor) else 1.0
                           for a, b, lo, hi in good]
                tree = rng.weighted(good, weights)
            else:
                tree = max(pairs, key=lambda p: p[3] - p[2])
            # Pflichttuer (spannender Baum -> alles bleibt erreichbar)
            a, b, lo, hi = tree
            ra, rb = rooms[a], rooms[b]
            kind, wd = self._door_spec(rng, ra, rb, hi - lo)
            margin = 1 if hi - lo >= wd + 2 else 0
            if hi - lo < wd + 2 * margin:
                kind, wd = "door", 1
            p = rng.randint(lo + margin, max(lo + margin, hi - margin - wd))
            used.append((p, p + wd))
            self._add_door(sec, axis, s, t, ra, rb, p, wd, kind)

            # Zusaetzliche Tueren -> Schleifen, Flure mit vielen Tueren
            mid = (lo + hi) * 0.5
            wx, wy = (s, mid) if axis == 0 else (mid, s)
            loop = ZoneField.blend(self.zones.weights(wx, wy), "loop_chance")
            for pair in pairs:
                a, b, lo, hi = pair
                ra, rb = rooms[a], rooms[b]
                overlap = hi - lo
                if overlap < 3:
                    continue
                if ra.type.multi_doors or rb.type.multi_doors:
                    p = lo + 1
                    while p + 1 <= hi - 1:
                        if self._free(used, p, 1) and rng.random() < 0.85:
                            used.append((p, p + 1))
                            self._add_door(sec, axis, s, t, ra, rb, p, 1, "door")
                        p += 3
                    continue
                if pair is tree:
                    continue
                prob = loop
                if ra.is_corridor or rb.is_corridor:
                    prob = max(prob, 0.6)
                for rt in (ra.type, rb.type):
                    if rt.extra_doors:
                        prob = max(prob, rt.extra_doors)
                if rng.random() >= prob:
                    continue
                kind, wd = self._door_spec(rng, ra, rb, overlap)
                if overlap < wd + 2:
                    kind, wd = "door", 1
                for _ in range(6):
                    p = rng.randint(lo + 1, max(lo + 1, hi - 1 - wd))
                    if self._free(used, p, wd):
                        used.append((p, p + wd))
                        self._add_door(sec, axis, s, t, ra, rb, p, wd, kind)
                        break

    def _make_portals(self, ctx):
        sec = ctx.sector
        X0, Y0 = sec.x0, sec.y0
        sx, sy = sec.sx, sec.sy
        specs = (
            ("left", lambda p: (X0 + 1, p), lambda p, w: (X0, p, X0 + 1, p + w, 0), "a", (sx - 1, sy), True),
            ("top", lambda p: (p, Y0 + 1), lambda p, w: (p, Y0, p + w, Y0 + 1, 1), "a", (sx, sy - 1), True),
            ("right", lambda p: (X0 + S - 1, p), lambda p, w: (X0 + S, p, X0 + S + 1, p + w, 0), "b", (sx + 1, sy), False),
            ("bottom", lambda p: (p, Y0 + S - 1), lambda p, w: (p, Y0 + S, p + w, Y0 + S + 1, 1), "b", (sx, sy + 1), False),
        )
        for edge, inside, rect, remote_side, remote, owned in specs:
            for p, w in ctx.portals[edge]:
                room = sec.room_at(*inside(p))
                if room is None:
                    continue
                x0, y0, x1, y1, axis = rect(p, w)
                if remote_side == "a":
                    door = Door(x0, y0, x1, y1, axis, None, room, "door")
                else:
                    door = Door(x0, y0, x1, y1, axis, room, None, "door")
                door.portal = True
                door.owned = owned
                door.remote = remote
                door.remote_side = remote_side
                door.sill = 0.0
                door.top = 2.2
                room.fixed_floor = True
                room.doors.append(door)
                sec.doors.append(door)

    # --- Hoehen -----------------------------------------------------------------------
    def _resolve_floors(self, sec):
        for room in sec.rooms:
            rng = Rng(room.seed ^ 0xF100)
            depth = 0.0
            rt = room.type
            if room.fixed_floor or rt.key == "versorgung":
                depth = 0.0
            elif rt.sunken and rng.random() < rt.sunken[0]:
                depth = rng.uniform(rt.sunken[1], rt.sunken[2])
            elif room.zone == TUNNEL and rng.random() < ZONES[TUNNEL].sunken:
                depth = 2.4
            if depth > 0.0:
                step = room.stair_step
                for door in room.doors:
                    depth = min(depth, (room.depth_from(door) - 2) * step)
                depth = round(math.floor(depth / 0.6 + 1e-6) * 0.6, 2)
            room.floor = -depth if depth > 0.0 else 0.0

        # Fixpunkt: Raeume anheben, in denen die Treppe zur Tuer nicht passt.
        changed = True
        guard = 0
        while changed and guard < 50:
            changed = False
            guard += 1
            for door in sec.doors:
                a, b = door.room_a, door.room_b
                if a is None or b is None or door.portal:
                    continue
                if abs(a.floor - b.floor) < 1e-6:
                    continue
                lo, hi = (a, b) if a.floor < b.floor else (b, a)
                diff = hi.floor - lo.floor
                n = max(1, int(math.ceil(diff / lo.stair_step - 1e-6)) - 1)
                if n + 3 > lo.depth_from(door) and not lo.fixed_floor:
                    lo.floor = hi.floor
                    changed = True
        for door in sec.doors:
            if door.portal:
                continue
            door.sill = max(door.room_a.floor, door.room_b.floor)

    def _resolve_heights(self, sec, rng):
        for room in sec.rooms:
            r = Rng(room.seed ^ 0x4E16)
            rt = room.type
            z = ZONES[room.zone]
            if rt.height:
                hgt = r.uniform(*rt.height)
            elif room.is_corridor:
                hgt = r.uniform(*z.h_corridor)
            elif rt.category == LARGE:
                k = min(1.0, max(0.0, (min(room.w, room.h) - 12) / 30.0))
                lo, hi = z.h_big
                hgt = lo + (hi - lo) * (0.5 * k + 0.5 * r.random())
            else:
                hgt = r.uniform(*z.h_room)
            for door in room.doors:
                need = (door.sill - room.floor) + (2.0 if z.id == MAINT else 2.4)
                hgt = max(hgt, need)
            if room.fixed_floor:
                hgt = max(hgt, 2.6)
            room.height = round(hgt * 20.0) / 20.0

        for door in sec.doors:
            if door.portal:
                continue
            a, b = door.room_a, door.room_b
            dh = min(ZONES[a.zone].door_height, ZONES[b.zone].door_height)
            cmin = min(a.ceil, b.ceil)
            if door.kind == "wide":
                top = door.sill + dh + 0.3
            elif door.kind == "gate":
                top = door.sill + max(dh, min(4.5, (cmin - door.sill) * 0.6))
            elif door.kind == "opening":
                top = cmin - hfloat(a.seed, b.seed, 7) * 1.5
            else:
                top = door.sill + dh
            top = min(top, cmin - 0.05)
            door.top = round(max(top, door.sill + 1.9) * 20.0) / 20.0

    # --- Stil: Materialien, Licht, Nebel ------------------------------------------------
    def _style(self, room):
        bank = self.bank
        rng = Rng(room.seed ^ 0x57A1)
        zw = room.zone_w
        z = ZONES[room.zone]
        rt = room.type
        var = rng.uniform(0.9, 1.06)

        def col(attr, scale=1.0):
            c = mix_rgb([getattr(zz, attr) for zz in ZONES], zw)
            return (c[0] * var * scale, c[1] * var * scale, c[2] * var * scale)

        wall = col("wall")
        floor = col("floor")
        ceil = col("ceil")
        trim = col("trim")
        lamp = mix_rgb([zz.lamp for zz in ZONES], zw)
        # Uebergangsbereiche: Boden aus der zweitstaerksten Zone
        order = sorted(range(5), key=lambda i: -zw[i])
        floor_pat = z.floor_pattern
        if zw[order[1]] > 0.27:
            floor_pat = ZONES[order[1]].floor_pattern
        wall_pat = z.wall_pattern
        if rt.key == "lichtraum":
            wall, floor, ceil, lamp = (0.95, 0.95, 0.93), (0.9, 0.9, 0.88), (0.97, 0.97, 0.95), (1.0, 1.0, 0.97)
            wall_pat, floor_pat = "plain", "tiles"
        if rt.key == "schachbrett":
            floor_pat = "checker"
            floor = (0.85, 0.85, 0.82)

        bands = ()
        trim_id = bank.get(trim, wall="plain")
        if room.zone in (OFFICE, HALLS):
            bands = ((0.0, 0.12, trim_id),)
        lamp_id = bank.get(lamp, emissive=True)
        room.lampmat = lamp_id
        sd = 0.86 if wall_pat == "wallpaper" else 0.72
        room.wmat = bank.get(wall, wall=wall_pat, bands=bands, seam_dark=sd)
        room.walllampmat = bank.get(wall, wall=wall_pat, bands=bands + ((1.75, 2.05, lamp_id),), seam_dark=sd)
        room.fmat = bank.get(floor, plane=floor_pat, seam_dark=0.3 if floor_pat == "checker" else 0.72)
        room.cmat = bank.get(ceil, plane=z.ceil_pattern if rt.key != "lichtraum" else "tiles")
        stair = (floor[0] * 0.95, floor[1] * 0.95, floor[2] * 0.95)
        room.stairmat = bank.get((stair[0] * 0.8, stair[1] * 0.8, stair[2] * 0.8), wall="plain")
        room.stairtop = bank.get(stair, plane="none")
        m = room.mats
        m["part"] = bank.get((0.42, 0.45, 0.54), wall="plain")
        m["parttop"] = bank.get((0.32, 0.34, 0.40))
        m["shelf"] = bank.get((0.26, 0.34, 0.56), wall="blocks")
        m["shelftop"] = bank.get((0.30, 0.30, 0.34))
        m["crate"] = bank.get((0.56, 0.40, 0.22), wall="panels")
        m["cratetop"] = bank.get((0.50, 0.36, 0.20))
        m["machine"] = bank.get((0.34, 0.40, 0.34), wall="blocks")
        m["machinetop"] = bank.get((0.28, 0.30, 0.28))
        m["stage"] = bank.get((floor[0] * 0.8, floor[1] * 0.8, floor[2] * 0.8), wall="plain")
        m["stagetop"] = bank.get(floor, plane="bigtiles")
        m["mono"] = bank.get((0.05, 0.05, 0.06), wall="plain")
        # V3: Versorgungsraum - Theke (Holzdekor, helle Arbeitsplatte) und leuchtender Automat
        m["counter"] = bank.get((0.50, 0.37, 0.24), wall="panels")
        m["countertop"] = bank.get((0.80, 0.79, 0.74))
        m["vend"] = bank.get((0.78, 0.30, 0.24), emissive=True)
        m["vendtop"] = bank.get((0.22, 0.20, 0.20))

        # Licht
        style = rt.light or z.light_style
        if room.zone == HALLS and rt.category == LARGE and style == "panels":
            style = "panels_wide"
        room.light_style = style
        amb = ZoneField.blend(zw, "ambient") * rt.ambient_mul * rng.uniform(0.8, 1.15)
        if rng.random() < 0.08 and rt.key not in ("lichtraum",):
            amb *= 0.35     # vereinzelt deutlich dunklere Raeume
        extra = {"panels": 0.35, "panels_wide": 0.3, "panels_long": 0.35, "pools": 0.25,
                 "wall": 0.22, "emergency": 0.15, "void": 0.05, "bright": 0.9, "dark": 0.05}
        inten = {"panels": 0.55, "panels_wide": 0.6, "panels_long": 0.55, "pools": 1.0,
                 "wall": 0.9, "emergency": 0.8, "void": 1.4, "bright": 0.5, "dark": 1.2}
        if style == "void":
            amb = 0.09
        elif style == "dark":
            amb = 0.04
        elif style == "bright":
            amb = 1.15
        room.ambient = amb
        room.lamp_int = inten.get(style, 0.6)
        room.avg_light = min(1.4, amb + extra.get(style, 0.3))

        # Nebel
        room.fog = mix_rgb([zz.fog for zz in ZONES], zw)
        room.fog_density = ZoneField.blend(zw, "fog_density") * rt.fog_mul
        if rt.key == "lichtraum":
            room.fog = (0.75, 0.75, 0.72)
        elif style in ("void", "dark"):
            room.fog = (0.012, 0.013, 0.02)

        # Flackern (V2): fast alle Lampen ruhig, selten ein leises Pulsieren,
        # sehr selten eine alternde Roehre mit weichen Helligkeitsschwankungen.
        r = rng.random()
        room.flicker = 2 if r < 0.012 else (1 if r < 0.06 else 0)

    def _door_style(self, door):
        room = door.room_b if door.room_a is None else door.room_a
        trim = mix_rgb([zz.trim for zz in ZONES], room.zone_w)
        frame = self.bank.get(trim, wall="plain")
        sill = self.bank.get((trim[0] * 0.7, trim[1] * 0.7, trim[2] * 0.7))
        if door.kind == "opening":
            frame = room.wmat
            sill = room.fmat
        door.mats = (sill, frame)
