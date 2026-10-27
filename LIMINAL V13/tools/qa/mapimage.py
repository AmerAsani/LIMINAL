"""V13.3: Draufsicht einer festen Karte nach Zonen (fuer BIOM_AUDIT.md) und Flaechenanteile der Zonen.

    python tools/qa/mapimage.py <level> <ausgabe.png> [--map pfad.json]
Druckt die Flaechenanteile je Zone und je Raumtyp (Markdown-Zeile).
"""
import argparse
import colorsys
import hashlib
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from maplib import NAMES, Grid  # noqa: E402


def color(key):
    h = int(hashlib.md5(key.encode('utf-8')).hexdigest()[:6], 16)
    r, g, b = colorsys.hsv_to_rgb((h % 360) / 360.0, 0.45 + (h >> 9) % 30 / 100.0, 0.62 + (h >> 15) % 30 / 100.0)
    return int(r * 255), int(g * 255), int(b * 255)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('out')
    ap.add_argument('--map', default=None)
    a = ap.parse_args()
    g = Grid(a.level, a.map)
    sc = max(2, int(1100 / max(g.W, g.H)))
    legend_h = 18 * (1 + len({r['zone'] for r in g.rooms}) // 4 + 1)
    im = Image.new('RGB', (g.W * sc, g.H * sc + legend_h), (22, 22, 22))
    d = ImageDraw.Draw(im)
    area = {}
    tarea = {}
    for r in g.rooms:
        x0, y0, x1, y1 = r['rect']
        d.rectangle([x0 * sc, y0 * sc, x1 * sc - 1, y1 * sc - 1], fill=color(r['zone']))
        area[r['zone']] = area.get(r['zone'], 0) + (x1 - x0) * (y1 - y0)
        tarea[r['type']] = tarea.get(r['type'], 0) + (x1 - x0) * (y1 - y0)
    for dd in g.m['doors']:
        x0, y0, x1, y1 = dd['rect']
        d.rectangle([x0 * sc, y0 * sc, x1 * sc - 1, y1 * sc - 1], fill=(235, 220, 140))
    for y in range(g.H):
        for x in range(g.W):
            if g.room[y, x] >= 0 and not g.walk[y, x] and not g.door[y, x]:
                d.rectangle([x * sc, y * sc, x * sc + sc - 1, y * sc + sc - 1], fill=(40, 40, 50))
    tot = sum(area.values())
    yy = g.H * sc + 4
    for k, z in enumerate(sorted(area, key=lambda z: -area[z])):
        x = (k % 4) * (g.W * sc // 4)
        y = yy + (k // 4) * 18
        d.rectangle([x + 4, y + 2, x + 16, y + 14], fill=color(z))
        d.text((x + 20, y + 2), f'{z} {100 * area[z] / tot:.0f} %', fill=(230, 230, 230))
    d.text((6, 4), NAMES.get(a.level, a.level), fill=(255, 255, 255))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    im.save(a.out)
    print('Zonen: ' + ', '.join(f'{z} {100 * v / tot:.0f} %' for z, v in sorted(area.items(), key=lambda kv: -kv[1])))
    print('Raumtypen: ' + ', '.join(f'{z} {100 * v / tot:.0f} %' for z, v in sorted(tarea.items(), key=lambda kv: -kv[1])))


if __name__ == '__main__':
    main()
