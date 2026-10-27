"""V13.3: feste Karten lesen und als Kachelraster auswerten (Raummetrik, Schilder, Aehnlichkeit, Wegbilder).

Eine Kachel ist 1 m x 1 m. Begehbar: Kacheln in Raeumen und Tueren, ohne Stuetzen des Rasters, ohne unsichtbare
Bloecke (Regale, Wagen) und ohne Gruben - wie tools/levels/mapkit.py (blocked) und die Engine.
"""
import json
import math
import os

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
LEVELS = ['level0', 'level1', 'level2', 'level3', 'level5', 'level5_3', 'level188', 'level903', 'level37', 'level36']
NAMES = {'level0': 'Level 0', 'level1': 'Level 1', 'level2': 'Level 2', 'level3': 'Level 3', 'level5': 'Level 5',
         'level5_3': 'Level 5.3', 'level188': 'Level 188', 'level903': 'Level 903', 'level37': 'Level 37', 'level36': 'Level 36'}


def load_jsonc(path):
    with open(path, encoding='utf-8') as fh:
        return json.loads(''.join(ln for ln in fh if not ln.lstrip().startswith('//')))


class Grid:
    """Kachelraster einer Karte: room[y, x] (Index, -1 Wand), walk[y, x], door[y, x], floor/height je Raum."""

    def __init__(self, level, path=None):
        self.level = level
        self.m = load_jsonc(path or os.path.join(ROOT, 'data', 'maps', level + '.json'))
        W, H = self.m['size']
        self.W, self.H = W, H
        self.room = np.full((H, W), -1, np.int32)
        self.walk = np.zeros((H, W), bool)
        self.door = np.zeros((H, W), bool)
        self.pillar = np.zeros((H, W), bool)
        self.block = np.zeros((H, W), bool)
        self.rooms = self.m['rooms']
        for i, r in enumerate(self.rooms):
            x0, y0, x1, y1 = r['rect']
            self.room[y0:y1, x0:x1] = i
            self.walk[y0:y1, x0:x1] = True
            if 'pillars' in r:
                sp, spy, th, px, py = (list(r['pillars']) + [0, 0, 0, 0, 0])[:5]
                spy = spy or sp
                if sp > 0:
                    for ty in range(y0 + 1, y1 - 1):
                        for tx in range(x0 + 1, x1 - 1):
                            lx, ly = tx - x0, ty - y0
                            ox, oy = (px - x0) % sp, (py - y0) % spy
                            if lx >= ox and ly >= oy and (lx - ox) % sp < th and (ly - oy) % spy < th:
                                self.pillar[ty, tx] = True
            for b in r.get('blocks', []):
                bx0, by0, bx1, by1, h, hidden = b[:6]
                if hidden == 1 or h < 0:
                    self.block[by0:by1, bx0:bx1] = True
        for d in self.m['doors']:
            x0, y0, x1, y1 = d['rect']
            self.door[y0:y1, x0:x1] = True
            self.walk[y0:y1, x0:x1] = True
        self.walk &= ~self.pillar & ~self.block
        self.objects = self.m.get('objects', [])

    def ok(self, x, y):
        ix, iy = int(math.floor(x)), int(math.floor(y))
        return 0 <= ix < self.W and 0 <= iy < self.H and self.walk[iy, ix]

    def ray(self, x, y, a, maxd=200.0, step=0.25):
        """Sichtweite (m) von (x, y) in Richtung a ueber begehbare Kacheln."""
        c, s = math.cos(a), math.sin(a)
        d = 0.0
        while d < maxd:
            d += step
            if not self.ok(x + c * d, y + s * d):
                return d - step
        return maxd

    def corridor(self, i):
        """Gang? Raumtyp mit Gang-Namen oder schmal und lang (kurze Seite <= 4 m, lang >= 3x)."""
        r = self.rooms[i]
        t = r['type']
        x0, y0, x1, y1 = r['rect']
        w, h = x1 - x0, y1 - y0
        named = any(k in t for k in ('gang', 'flur', 'tunnel', 'corridor', 'concourse'))
        return named or (min(w, h) <= 4 and max(w, h) >= 3 * min(w, h))

    def neighbours(self):
        """Raum-Nachbarn ueber Tueren."""
        adj = {i: set() for i in range(len(self.rooms))}
        for d in self.m['doors']:
            a, b = d['a'], d['b']
            if 0 <= a < len(self.rooms) and 0 <= b < len(self.rooms):
                adj[a].add(b)
                adj[b].add(a)
        return adj


def path(grid, start, goal):
    """A* auf begehbaren Kacheln (8 Nachbarn, keine Ecken schneiden). Liste von (x, y) Kachelmitten oder None."""
    import heapq
    sx, sy = int(start[0]), int(start[1])
    gx, gy = int(goal[0]), int(goal[1])
    if not grid.walk[sy, sx] or not grid.walk[gy, gx]:
        return None
    openh = [(0.0, sx, sy)]
    came = {(sx, sy): None}
    cost = {(sx, sy): 0.0}
    steps = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
    while openh:
        _, x, y = heapq.heappop(openh)
        if (x, y) == (gx, gy):
            out = []
            n = (x, y)
            while n:
                out.append((n[0] + 0.5, n[1] + 0.5))
                n = came[n]
            return out[::-1]
        for dx, dy, c in steps:
            nx, ny = x + dx, y + dy
            if not (0 <= nx < grid.W and 0 <= ny < grid.H) or not grid.walk[ny, nx]:
                continue
            if dx and dy and (not grid.walk[y, nx] or not grid.walk[ny, x]):
                continue
            # Abstand zur Wand bevorzugen (Kosten neben Waenden hoeher)
            wall = 0.0
            for ex, ey in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if not (0 <= nx + ex < grid.W and 0 <= ny + ey < grid.H) or not grid.walk[ny + ey, nx + ex]:
                    wall += 0.35
            nc = cost[(x, y)] + c + wall
            if nc < cost.get((nx, ny), 1e18):
                cost[(nx, ny)] = nc
                came[(nx, ny)] = (x, y)
                heapq.heappush(openh, (nc + math.hypot(gx - nx, gy - ny), nx, ny))
    return None
