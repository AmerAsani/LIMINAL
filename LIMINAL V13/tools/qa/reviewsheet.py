"""V13.3: kompakter Pruefbogen einer Runde - je Klasse zwei Nahansichten (50 cm im Licht, 50 cm mit Lampe) aus den
Einzelbildern von propsheet.py (build/qa/shots/<level>/<runde>/).

    python tools/qa/reviewsheet.py <level> <runde> [--only name,...] [--out datei.jpg] [--cols 8]
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import ROOT  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('round')
    ap.add_argument('--only', default='')
    ap.add_argument('--out', default='')
    ap.add_argument('--cols', type=int, default=8)
    ap.add_argument('--views', default='50cm_0,50cm_lampe_1')
    a = ap.parse_args()
    d = os.path.join(ROOT, 'build', 'qa', 'shots', a.level, a.round)
    keys = sorted({f.split('__')[0] for f in os.listdir(d) if f.endswith('.png')})
    if a.only:
        want = {s.replace('@', '_v') for s in a.only.split(',')}
        keys = [k for k in keys if k in want or k.split('_v')[0] in want]
    views = a.views.split(',')
    tw, th = 300, 169
    tiles = [(k, v) for k in keys for v in views]
    rows = (len(tiles) + a.cols - 1) // a.cols
    S = Image.new('RGB', (a.cols * tw, rows * th), (20, 20, 20))
    dr = ImageDraw.Draw(S)
    for i, (k, v) in enumerate(tiles):
        x, y = (i % a.cols) * tw, (i // a.cols) * th
        try:
            im = Image.open(os.path.join(d, f'{k}__{v}.png')).convert('RGB').resize((tw, th))
        except OSError:
            im = Image.new('RGB', (tw, th), (90, 0, 0))
        S.paste(im, (x, y))
        dr.rectangle([x, y, x + 8 + 6 * len(k), y + 13], fill=(0, 0, 0))
        dr.text((x + 3, y + 1), k, fill=(255, 255, 255))
    out = a.out or os.path.join(ROOT, 'build', 'qa', f'review_{a.level}_{a.round}.jpg')
    S.save(out, quality=85)
    print(out)


if __name__ == '__main__':
    main()
