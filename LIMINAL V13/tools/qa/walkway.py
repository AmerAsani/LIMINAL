"""V13.3: Laufweg-Test (PROMPT_V13_3, Abnahmeschranke e): ein zusammenhaengender Weg durch mindestens 15 Raeume/Bereiche
am Stueck und mindestens 200 m, alle paar Meter ein Bild aus der Spielerkamera (Augenhoehe, Blick in Laufrichtung).

Der Weg beginnt am Startpunkt der Karte und geht von Raum zu Raum (Tueren), bevorzugt zu noch nicht besuchten Raeumen;
zwischen den Raeumen A* auf begehbaren Kacheln mit Abstand zur Wand. Wesen sind aus (Bilder der Bauten).

    python tools/qa/walkway.py <level> [--round vorher] [--step 4] [--rooms 15] [--meters 200] [--seed 1]
Ausgabe: Einzelbilder build/qa/walk/<level>/<runde>/NNN.png, Kontaktboegen docs/screens/<level>/wegbilder_<runde>_<seite>.jpg, Weg als Karte
wegkarte_<runde>.png und Kennwerte (Laenge, Raeume) in wegbilder_<runde>.json.
"""
import argparse
import json
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import run  # noqa: E402
from maplib import NAMES, ROOT, Grid, path  # noqa: E402


def plan(g, rooms_min, meters_min, seed):
    rnd = random.Random(seed)
    adj = g.neighbours()
    sx, sy = g.m['start'][0], g.m['start'][1]
    cur = int(g.room[int(sy), int(sx)])
    pts = [(sx, sy)]
    seq = [cur]
    visited = {cur}
    length = 0.0
    guard = 0
    if not adj.get(cur):  # V13.3: Ebene aus einem Raum (Level 903): entlang der langen Seite hin und zurueck
        r = g.rooms[cur]
        x0, y0, x1, y1 = r['rect']
        along_x = (x1 - x0) >= (y1 - y0)
        lanes = [y0 + (y1 - y0) * f for f in (0.35, 0.65)] if along_x else [x0 + (x1 - x0) * f for f in (0.35, 0.65)]
        for k, lane in enumerate(lanes):
            ends = (x0 + 1.5, x1 - 1.5) if k % 2 == 0 else (x1 - 1.5, x0 + 1.5)
            for e in ends:
                q = (e, lane) if along_x else (lane, e)
                if g.ok(*q):
                    p_ = path(g, pts[-1], q)
                    if p_:
                        for qq in p_[1:]:
                            length += math.hypot(qq[0] - pts[-1][0], qq[1] - pts[-1][1])
                            pts.append(qq)
        return pts, seq, length
    while (len(visited) < rooms_min or length < meters_min) and guard < 400:
        guard += 1
        nb = sorted(adj.get(cur, ()))
        if not nb:
            break
        fresh = [n for n in nb if n not in visited]
        nxt = rnd.choice(fresh) if fresh else rnd.choice(nb)
        r = g.rooms[nxt]
        x0, y0, x1, y1 = r['rect']
        # Ziel: begehbare Kachel nahe der Raummitte
        best = None
        for t in range(60):
            tx = rnd.uniform(x0 + 0.3 * (x1 - x0), x1 - 0.3 * (x1 - x0)) if t > 0 else (x0 + x1) / 2.0
            ty = rnd.uniform(y0 + 0.3 * (y1 - y0), y1 - 0.3 * (y1 - y0)) if t > 0 else (y0 + y1) / 2.0
            if g.ok(tx, ty):
                best = (tx, ty)
                break
        if not best:
            cur = nxt
            continue
        p = path(g, pts[-1], best)
        if not p or len(p) < 2:
            cur = nxt
            continue
        for q in p[1:]:
            length += math.hypot(q[0] - pts[-1][0], q[1] - pts[-1][1])
            pts.append(q)
        cur = nxt
        visited.add(cur)
        seq.append(cur)
    return pts, seq, length


def resample(pts, step):
    out = []
    acc = 0.0
    for i in range(1, len(pts)):
        (ax, ay), (bx, by) = pts[i - 1], pts[i]
        seg = math.hypot(bx - ax, by - ay)
        while acc <= seg and seg > 0:
            t = acc / seg
            out.append((ax + (bx - ax) * t, ay + (by - ay) * t))
            acc += step
        acc -= seg
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('--round', default='aktuell')
    ap.add_argument('--step', type=float, default=4.0)
    ap.add_argument('--rooms', type=int, default=15)
    ap.add_argument('--meters', type=float, default=200.0)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--size', default='1280x720')
    a = ap.parse_args()
    g = Grid(a.level)
    pts, seq, length = plan(g, a.rooms, a.meters, a.seed)
    shots = resample(pts, a.step)
    od = os.path.join(ROOT, 'docs', 'screens', a.level, f'wegbilder_{a.round}')
    os.makedirs(os.path.dirname(od), exist_ok=True)  # Einzelbilder bleiben in build/qa/walk (nur Boegen ins Repository)
    raw = os.path.join(ROOT, 'build', 'qa', 'walk', a.level, a.round)
    os.makedirs(raw, exist_ok=True)
    lines = ['wait 90']
    for i, (x, y) in enumerate(shots):
        j = min(i + 1, len(shots) - 1)
        k = max(i - 1, 0)
        yaw = math.atan2(shots[j][1] - shots[k][1], shots[j][0] - shots[k][0]) if j != k else 0.0
        lines.append(f'pos {x:.2f} {y:.2f} {yaw:.4f} -0.04')
        lines.append('wait 10')
        lines.append('shot ' + os.path.join(raw, f'{i:03d}.png').replace('\\', '/'))
    lines.append('quit')
    sp = os.path.join(raw, 'script.txt')
    with open(sp, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(lines))
    code, log = run(f'--level {a.level} --seed {a.seed} --set figures=0', '@' + sp, os.path.join(ROOT, 'build', 'qa', 'ud_walk_' + a.level),
                    size=a.size, timeout=7200)
    from PIL import Image, ImageDraw
    ims = []
    for i in range(len(shots)):
        fn = os.path.join(raw, f'{i:03d}.png')
        try:
            im = Image.open(fn).convert('RGB')
        except OSError:
            continue
        ims.append((i, im.resize((320, 180))))
    cols, per = 6, 36  # Seiten zu 36 Bildern (lesbar)
    for page in range(0, max(len(ims), 1), per):
        chunk = ims[page:page + per]
        rows = (len(chunk) + cols - 1) // cols
        S = Image.new('RGB', (cols * 320, max(rows, 1) * 180), (20, 20, 20))
        d = ImageDraw.Draw(S)
        for n, (i, im) in enumerate(chunk):
            x, y = (n % cols) * 320, (n // cols) * 180
            S.paste(im, (x, y))
            d.rectangle([x, y, x + 64, y + 14], fill=(0, 0, 0))
            d.text((x + 3, y + 2), f'{i:03d} {i * a.step:.0f} m', fill=(255, 255, 255))
        S.save(os.path.join(od, '..', f'wegbilder_{a.round}_{page // per + 1}.jpg'), quality=80)
    # Karte mit Weg
    sc = max(1, int(900 / max(g.W, g.H)))
    M = Image.new('RGB', (g.W * sc, g.H * sc), (25, 25, 25))
    px = M.load()
    for yy in range(g.H):
        for xx in range(g.W):
            if g.walk[yy, xx]:
                c = (150, 150, 140) if g.room[yy, xx] in seq else (85, 85, 80)
                for u in range(sc):
                    for v in range(sc):
                        px[xx * sc + u, yy * sc + v] = c
    dm = ImageDraw.Draw(M)
    dm.line([(x * sc, y * sc) for x, y in pts], fill=(220, 60, 40), width=2)
    for i, (x, y) in enumerate(shots):
        dm.ellipse([x * sc - 2, y * sc - 2, x * sc + 2, y * sc + 2], fill=(255, 210, 60))
    M.save(os.path.join(od, '..', f'wegkarte_{a.round}.png'))
    info = {'level': a.level, 'round': a.round, 'length_m': round(length, 1), 'rooms': len(set(seq)), 'room_sequence': seq,
            'shots': len(ims), 'step_m': a.step, 'exit_code': code}
    with open(os.path.join(od, '..', f'wegbilder_{a.round}.json'), 'w', encoding='utf-8') as fh:
        json.dump(info, fh, indent=1)
    print(f"{NAMES[a.level]}: Weg {length:.0f} m durch {len(set(seq))} Raeume, {len(ims)} Bilder -> {od}")
    return 0 if len(set(seq)) >= a.rooms and length >= a.meters and len(ims) == len(shots) else 1


if __name__ == '__main__':
    sys.exit(main())
