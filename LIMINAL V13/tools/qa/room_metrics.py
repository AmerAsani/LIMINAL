"""V13.3: Raummetrik je Ebene (PROMPT_V13_3 Teil 2) und automatischer Test gegen Mindestwerte.

Gemessen aus der festen Karte (data/maps) und der Ausstattung im Spiel (build/qa/props_<level>.json, tools/qa/propdump.py):
  - Deckenhoehe je Raum (Gaenge, Raeume, Hallen getrennt)
  - Gangbreite (kurze Seite der Gaenge)
  - Raumgroessen (Flaeche) und Raumhierarchie (Anteil grosser Raeume, groesster Raum / Median)
  - Sichtachsen: von Stichproben-Punkten (Raummitten und zufaellige begehbare Stellen) in 32 Richtungen; je Punkt die
    laengste freie Sicht; Kennwerte Median und 90. Perzentil
  - Abstand Stuetze-Wand: Spalt zwischen einer Stuetze des Rasters und der Raumwand (0 = buendig, sonst begehbar >= Grenze)
  - Moebelabstaende: in Raeumen mit Ausstattung am Boden muss zwischen allen Tueren ein Weg mit 0,9 m lichter Breite
    frei bleiben (Raster 0,25 m, Grundriss der Modelle aus dem Spiel)

    python tools/qa/room_metrics.py [level ...]           Kennwerte -> docs/metrics/<level>.json, Tabelle
    python tools/qa/room_metrics.py --check [level ...]   zusaetzlich gegen tools/qa/metric_limits.json (Exitcode 1)
"""
import json
import math
import os
import sys

import numpy as np
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from maplib import LEVELS, NAMES, ROOT, Grid  # noqa: E402

FLOOR_SKIP = ('puddle', 'water_surface', 'debris', 'books_fallen', 'paving_strip', 'rail_track', 'track_metro',
              'platform_edge', 'travelator', 'grate_stair', 'sludge_pool', 'birch_circle', 'skull', 'pillar_label')


def pct(a, q):
    return float(np.percentile(a, q)) if len(a) else 0.0


def sight_lines(g, rng):
    pts = []
    for i, r in enumerate(g.rooms):
        x0, y0, x1, y1 = r['rect']
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        if g.ok(cx, cy):
            pts.append((cx, cy))
    ys, xs = np.nonzero(g.walk)
    k = min(400, len(xs))
    for j in rng.choice(len(xs), k, replace=False):
        pts.append((xs[j] + 0.5, ys[j] + 0.5))
    best = []
    for (x, y) in pts:
        best.append(max(g.ray(x, y, a * math.pi / 16.0) for a in range(32)))
    return np.array(best)


def pillar_gaps(g):
    """Spalt zwischen Stuetze und Raumwand je Raum mit Raster (nur Spalte > 0)."""
    gaps = []
    for i, r in enumerate(g.rooms):
        if 'pillars' not in r:
            continue
        x0, y0, x1, y1 = r['rect']
        sub = g.pillar[y0:y1, x0:x1]
        if not sub.any():
            continue
        ys, xs = np.nonzero(sub)
        for gx in (xs.min(), (x1 - x0 - 1) - xs.max()):
            if gx > 0:
                gaps.append(float(gx))
        for gy in (ys.min(), (y1 - y0 - 1) - ys.max()):
            if gy > 0:
                gaps.append(float(gy))
    return gaps


def furniture_blocks(g, props):
    """Raeume, in denen Ausstattung am Boden den Weg zwischen den Tueren auf unter 0,9 m verengt."""
    R = 4  # Zellen je Meter
    by_room = {}
    for p in props:
        n = p['name']
        if n.startswith(FLOOR_SKIP) or not p['room'] or len(p['room']) < 3 or not p['room'][2]:
            continue
        lo, hi, s = p['lo'], p['hi'], p['s']
        if (hi[2] - lo[2]) * s[2] < 0.3 or p['p'][2] - p['floor'] > 0.5 or p['p'][2] + lo[2] * s[2] - p['floor'] > 1.0:
            continue  # flach, nicht am Boden (Wand, Decke) oder erst ueber 1 m (Rohrbuendel ueber Kopf)
        by_room.setdefault(tuple(p['room'][2]), []).append(p)
    bad, checked = [], 0
    door_rects = [d['rect'] for d in g.m['doors']]
    for i, r in enumerate(g.rooms):
        rc = tuple(r['rect'])
        if rc not in by_room:
            continue
        x0, y0, x1, y1 = rc
        doors = [d for d in door_rects if d[0] <= x1 and d[2] >= x0 and d[1] <= y1 and d[3] >= y0]
        if len(doors) < 2:
            continue
        checked += 1
        ox, oy = x0 - 1, y0 - 1
        Wc, Hc = (x1 - x0 + 2) * R, (y1 - y0 + 2) * R
        free = np.zeros((Hc, Wc), bool)
        for ty in range(y0 - 1, y1 + 1):
            for tx in range(x0 - 1, x1 + 1):
                if 0 <= tx < g.W and 0 <= ty < g.H and g.walk[ty, tx]:
                    free[(ty - oy) * R:(ty - oy + 1) * R, (tx - ox) * R:(tx - ox + 1) * R] = True
        for p in by_room[rc]:
            lo, hi, s = p['lo'], p['hi'], p['s']
            c, sn = math.cos(p['yaw']), math.sin(p['yaw'])
            corners = [(lo[0] * s[0], lo[1] * s[1]), (hi[0] * s[0], lo[1] * s[1]), (hi[0] * s[0], hi[1] * s[1]), (lo[0] * s[0], hi[1] * s[1])]
            wx = [p['p'][0] + a * c - b * sn for a, b in corners]
            wy = [p['p'][1] + a * sn + b * c for a, b in corners]
            cx0, cx1 = int(math.floor((min(wx) - ox) * R)), int(math.ceil((max(wx) - ox) * R))
            cy0, cy1 = int(math.floor((min(wy) - oy) * R)), int(math.ceil((max(wy) - oy) * R))
            free[max(cy0, 0):max(cy1, 0), max(cx0, 0):max(cx1, 0)] = False
        dist = ndimage.distance_transform_edt(free) / R
        passable = dist >= 0.45
        lab, n = ndimage.label(passable)
        comps = set()
        for d in doors:
            cells = lab[max(0, (d[1] - oy) * R):max(0, (d[3] - oy) * R), max(0, (d[0] - ox) * R):max(0, (d[2] - ox) * R)]
            ids = set(np.unique(cells)) - {0}
            comps.add(min(ids) if ids else -1)
        if len(comps) > 1:
            bad.append({'room': list(rc), 'type': r['type']})
    return bad, checked


def measure(level, exempt=()):
    g = Grid(level)
    rng = np.random.default_rng(7)
    # V13.3: Raumtypen mit begruendeter Ausnahme (metric_limits.json exempt_types, Begruendung in STRUKTURTYP.md) zaehlen
    # nicht fuer Decke und Gangbreite (z. B. Aufzugskabinen, laut Wiki beengte Kesselgaenge)
    rooms = [(i, r) for i, r in enumerate(g.rooms) if r['type'] not in exempt]
    area = np.array([(r['rect'][2] - r['rect'][0]) * (r['rect'][3] - r['rect'][1]) for _, r in rooms], float)
    corr = np.array([g.corridor(i) for i, _ in rooms])
    height = np.array([r['height'] for _, r in rooms], float)
    width = np.array([min(r['rect'][2] - r['rect'][0], r['rect'][3] - r['rect'][1]) for _, r in rooms], float)
    halls = area >= 150
    sl = sight_lines(g, rng)
    props = []
    pp = os.path.join(ROOT, 'build', 'qa', f'props_{level}.json')
    if os.path.exists(pp):
        with open(pp, encoding='utf-8') as fh:
            props = json.load(fh)['props']
    bad, checked = furniture_blocks(g, props)
    gaps = pillar_gaps(g)
    rm = ~corr
    out = {
        'level': level,
        'rooms': len(rooms),
        'corridors': int(corr.sum()),
        'area_total': float(area.sum()),
        'ceiling_rooms': {'min': float(height[rm].min()) if rm.any() else 0, 'p10': pct(height[rm], 10), 'median': pct(height[rm], 50)},
        'ceiling_corridors': {'min': float(height[corr].min()) if corr.any() else 0, 'median': pct(height[corr], 50)},
        'corridor_width': {'min': float(width[corr].min()) if corr.any() else 0, 'p10': pct(width[corr], 10), 'median': pct(width[corr], 50)},
        'room_area': {'p10': pct(area[rm], 10), 'median': pct(area[rm], 50), 'p90': pct(area[rm], 90), 'max': float(area.max())},
        'hierarchy': {'large_rooms': int(halls.sum()), 'share_large': float(area[halls].sum() / area.sum()),
                      'max_over_median': float(area.max() / max(pct(area[rm], 50), 1.0))},
        'sight': {'median': pct(sl, 50), 'p90': pct(sl, 90), 'points': int(len(sl))},
        'pillar_wall_gaps': {'count': len(gaps), 'narrow': int(sum(1 for x in gaps if x < 1.0)), 'min': min(gaps) if gaps else 0},
        'furniture': {'rooms_checked': checked, 'blocked': bad},
    }
    return out


def check(m, lim):
    errs = []
    def need(cond, msg):
        if not cond:
            errs.append(msg)
    need(m['ceiling_rooms']['min'] >= lim['ceiling_min'], f"Deckenhoehe Raeume min {m['ceiling_rooms']['min']:.2f} < {lim['ceiling_min']}")
    if m['corridors']:
        need(m['ceiling_corridors']['min'] >= lim.get('ceiling_corridor_min', lim['ceiling_min']),
             f"Deckenhoehe Gaenge min {m['ceiling_corridors']['min']:.2f} < {lim.get('ceiling_corridor_min', lim['ceiling_min'])}")
        need(m['corridor_width']['min'] >= lim['corridor_width_min'], f"Gangbreite min {m['corridor_width']['min']:.1f} < {lim['corridor_width_min']}")
        need(m['corridor_width']['median'] >= lim.get('corridor_width_median', 0), f"Gangbreite Median {m['corridor_width']['median']:.1f} < {lim.get('corridor_width_median', 0)}")
    need(m['room_area']['median'] >= lim.get('room_area_median', 0), f"Raumflaeche Median {m['room_area']['median']:.0f} < {lim.get('room_area_median', 0)}")
    need(m['sight']['median'] >= lim['sight_median'], f"Sichtachse Median {m['sight']['median']:.1f} m < {lim['sight_median']}")
    need(m['sight']['p90'] >= lim.get('sight_p90', 0), f"Sichtachse P90 {m['sight']['p90']:.1f} m < {lim.get('sight_p90', 0)}")
    need(m['pillar_wall_gaps']['narrow'] == 0, f"{m['pillar_wall_gaps']['narrow']} Spalte Stuetze-Wand unter 1 m")
    need(len(m['furniture']['blocked']) == 0, f"{len(m['furniture']['blocked'])} Raeume, in denen Ausstattung den Weg unter 0,9 m verengt")
    need(m['hierarchy']['large_rooms'] >= lim.get('large_rooms_min', 0), f"nur {m['hierarchy']['large_rooms']} grosse Raeume (>= 150 m2), Grenze {lim.get('large_rooms_min', 0)}")
    return errs


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    do_check = '--check' in sys.argv
    levels = args or LEVELS
    with open(os.path.join(ROOT, 'tools', 'qa', 'metric_limits.json'), encoding='utf-8') as fh:
        limits = json.load(fh)
    od = os.path.join(ROOT, 'docs', 'metrics')
    os.makedirs(od, exist_ok=True)
    fails = 0
    print(f"{'Ebene':10} {'Raeume':>6} {'Decke min/med':>13} {'Gang min/med':>12} {'Flaeche med':>11} {'Sicht med/p90':>13} {'gross':>5} {'Stuetze<1m':>10} {'verstellt':>9}")
    for lv in levels:
        m = measure(lv, tuple(limits.get(lv, {}).get('exempt_types', ())))
        with open(os.path.join(od, f'{lv}.json'), 'w', encoding='utf-8') as fh:
            json.dump(m, fh, indent=1)
        print(f"{NAMES[lv]:10} {m['rooms']:6d} {m['ceiling_rooms']['min']:6.2f}/{m['ceiling_rooms']['median']:<6.2f} "
              f"{m['corridor_width']['min']:5.1f}/{m['corridor_width']['median']:<5.1f} {m['room_area']['median']:11.0f} "
              f"{m['sight']['median']:6.1f}/{m['sight']['p90']:<6.1f} {m['hierarchy']['large_rooms']:5d} {m['pillar_wall_gaps']['narrow']:10d} "
              f"{len(m['furniture']['blocked']):4d}/{m['furniture']['rooms_checked']:<4d}")
        if do_check:
            errs = check(m, limits[lv])
            for e in errs:
                print(f'   FEHLER {NAMES[lv]}: {e}')
            fails += bool(errs)
    if do_check:
        print('Raummetrik:', 'OK' if not fails else f'{fails} Ebenen mit Fehlern')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
