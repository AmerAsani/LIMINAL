"""V13.3: Bilder einer Stelle aus mehreren Kameras (z. B. Pfuetze, Wasser, Objekt im Spiel) - im Licht der Ebene und mit
streifender Lampe. Fuer Vorher/Nachher denselben Aufruf mit LIM_EXE auf das alte Spiel.

    python tools/qa/spotshots.py <level> <x> <y> <z> <name> [--out dir] [--seed 1] [--dist 2.6] [--height 1.6]
Kameras: vier Winkel auf Augenhoehe (dist), zwei nah (0,9 m, flach), dazu je eine Lampenansicht. Bogen <out>/<name>.jpg.
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import ROOT, run  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('x', type=float)
    ap.add_argument('y', type=float)
    ap.add_argument('z', type=float)
    ap.add_argument('name')
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'qa', 'spots'))
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--dist', type=float, default=2.6)
    ap.add_argument('--height', type=float, default=1.6)
    ap.add_argument('--settle', type=int, default=30)
    ap.add_argument('--legacy', action='store_true', help='altes Spiel ohne camat/testlight/@datei: Spielerkamera (pos) auf Augenhoehe')
    a = ap.parse_args()
    raw = os.path.join(a.out, a.name + '_raw')
    os.makedirs(raw, exist_ok=True)
    x, y, z = a.x, a.y, a.z
    views = []
    for k, az in enumerate((20, 110, 200, 290)):
        r = math.radians(az)
        views.append((f'licht_{k}', (x + math.cos(r) * a.dist, y + math.sin(r) * a.dist, z + a.height), None, 1.0))
    for k, az in enumerate((45, 225)):
        r = math.radians(az)
        views.append((f'nah_{k}', (x + math.cos(r) * 0.9, y + math.sin(r) * 0.9, z + 0.55), None, 1.0))
    for k, az in enumerate((20, 200)):
        r = math.radians(az)
        cam = (x + math.cos(r) * a.dist, y + math.sin(r) * a.dist, z + a.height)
        lr = r + math.radians(65)
        lamp = (x + math.cos(lr) * 1.6, y + math.sin(lr) * 1.6, z + 0.35)
        views.append((f'lampe_{k}', cam, lamp, 0.0))
    sc = ['wait 90', f'pos {x + 6:.2f} {y + 6:.2f}']
    for n, cam, lamp, wl in views:
        sc.append('camat {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} 60'.format(*cam, x, y, z))
        sc.append(f'worldlight {wl}')
        sc.append('testlight off' if lamp is None else 'testlight {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} 1.0 30'.format(*lamp, x, y, z))
        sc.append(f'wait {a.settle}')
        sc.append('shot ' + os.path.join(raw, n + '.png').replace('\\', '/'))
    sc.append('quit')
    if a.legacy:  # Spielerkamera: vier Winkel aus dist, Blick auf die Stelle
        views = [v for v in views if v[0].startswith('licht')]
        sc = ['wait 90']
        for n, cam, lamp, wl in views:
            yaw = math.atan2(y - cam[1], x - cam[0])
            pitch = -math.atan2(1.6, math.hypot(x - cam[0], y - cam[1]))
            sc += [f'pos {cam[0]:.2f} {cam[1]:.2f} {yaw:.4f} {pitch:.4f}', f'wait {a.settle}',
                   'shot ' + os.path.join(raw, n + '.png').replace('\\', '/')]
        sc.append('quit')
        run(['--level', a.level, '--seed', str(a.seed), '--set', 'figures=0'], '; '.join(sc), os.path.join(ROOT, 'build', 'qa', 'ud_spot'))
        views += [('-', None, None, 0)] * 4
    sp = os.path.join(raw, 'script.txt')
    with open(sp, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(sc))
    if not a.legacy:
        run(['--level', a.level, '--seed', str(a.seed), '--set', 'figures=0'], '@' + sp, os.path.join(ROOT, 'build', 'qa', 'ud_spot'))
    from PIL import Image, ImageDraw
    tw, th = 480, 270
    S = Image.new('RGB', (tw * 4, th * 2), (20, 20, 20))
    d = ImageDraw.Draw(S)
    for i, (n, *_r) in enumerate(views):
        try:
            im = Image.open(os.path.join(raw, n + '.png')).convert('RGB').resize((tw, th))
        except OSError:
            im = Image.new('RGB', (tw, th), (90, 0, 0))
        S.paste(im, ((i % 4) * tw, (i // 4) * th))
        d.text(((i % 4) * tw + 4, (i // 4) * th + 4), n, fill=(255, 255, 255))
    out = os.path.join(a.out, a.name + '.jpg')
    S.save(out, quality=86)
    print(out)


if __name__ == '__main__':
    main()
