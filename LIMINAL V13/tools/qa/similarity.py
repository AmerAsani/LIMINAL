"""V13.3: Aehnlichkeit der Raumlayouts je Ebene (PROMPT_V13_3 Teil 2).

Die Karte wird in Fenster von 24 m x 24 m zerlegt (nur Fenster mit mindestens 30 % begehbarer Flaeche). Je Fenster ein
Merkmalsvektor: Anteile der Raumtypen an der Flaeche, Gang-Anteil, mittlere Raumflaeche (log), Tuerdichte, Anteil
begehbarer Flaeche, Deckenhoehe. Kennwerte:
  S       mittlere Kosinus-Aehnlichkeit jedes Fensters zum Mittel der Ebene (1 = ueberall gleich)
  Ausreisser  Anteil der Fenster mit Aehnlichkeit < 0,75 zum Mittel (Sonderzonen, Wahrzeichen)
Fuer Labyrinth-Ebenen (STRUKTURTYP.md) muss S hoch und der Ausreisseranteil klein sein; nur Zonen, die das Wiki
beschreibt, duerfen abweichen (Grenzen: tools/qa/metric_limits.json similarity_min, outliers_max).

    python tools/qa/similarity.py [--check] [level ...]
"""
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from maplib import LEVELS, NAMES, ROOT, Grid  # noqa: E402

WIN = 24


def windows(g):
    types = sorted({r['type'] for r in g.rooms})
    tix = {t: i for i, t in enumerate(types)}
    feats = []
    for wy in range(0, g.H - WIN + 1, WIN // 2):
        for wx in range(0, g.W - WIN + 1, WIN // 2):
            sub = g.room[wy:wy + WIN, wx:wx + WIN]
            walk = g.walk[wy:wy + WIN, wx:wx + WIN]
            if walk.mean() < 0.3:
                continue
            v = np.zeros(len(types) + 5)
            ids, cnt = np.unique(sub[sub >= 0], return_counts=True)
            areas, corr, hs = [], 0.0, []
            for i, c in zip(ids, cnt):
                r = g.rooms[i]
                v[tix[r['type']]] += c
                x0, y0, x1, y1 = r['rect']
                areas.append((x1 - x0) * (y1 - y0))
                hs.append(r['height'])
                if g.corridor(i):
                    corr += c
            tot = max(cnt.sum(), 1)
            v[:len(types)] /= tot
            v[len(types)] = corr / tot
            v[len(types) + 1] = math.log(1 + np.mean(areas)) / 8.0
            v[len(types) + 2] = g.door[wy:wy + WIN, wx:wx + WIN].mean() * 5.0
            v[len(types) + 3] = walk.mean()
            v[len(types) + 4] = np.mean(hs) / 10.0
            feats.append(v)
    return np.array(feats)


def measure(level):
    g = Grid(level)
    F = windows(g)
    if len(F) < 2:
        return {'level': level, 'windows': int(len(F)), 'S': 1.0, 'outliers': 0.0}
    mean = F.mean(0)
    sim = F @ mean / (np.linalg.norm(F, axis=1) * np.linalg.norm(mean) + 1e-9)
    return {'level': level, 'windows': int(len(F)), 'S': float(sim.mean()), 'outliers': float((sim < 0.75).mean()),
            'min': float(sim.min())}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    do_check = '--check' in sys.argv
    with open(os.path.join(ROOT, 'tools', 'qa', 'metric_limits.json'), encoding='utf-8') as fh:
        limits = json.load(fh)
    fails = 0
    for lv in args or LEVELS:
        m = measure(lv)
        print(f"{NAMES[lv]:10} Fenster {m['windows']:4d}  S {m['S']:.3f}  Ausreisser {m['outliers'] * 100:5.1f} %  min {m.get('min', 1):.2f}")
        lim = limits.get(lv, {})
        if do_check and ('similarity_min' in lim or 'outliers_max' in lim):
            e = []
            if m['S'] < lim.get('similarity_min', 0):
                e.append(f"S {m['S']:.3f} < {lim['similarity_min']}")
            if m['outliers'] > lim.get('outliers_max', 1):
                e.append(f"Ausreisser {m['outliers']:.2f} > {lim['outliers_max']}")
            for x in e:
                print('   FEHLER', x)
            fails += bool(e)
    if do_check:
        print('Aehnlichkeit:', 'OK' if not fails else f'{fails} Ebenen mit Fehlern')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
