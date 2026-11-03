"""V13.3 Teil 4 Level 188: Bilder fuer die Kritikrunden des Horchers und der Wurzelpistole (KREATUR_Horcher.md,
KREATUR_Wurzelpistole.md). Kein Studio: Licht, Nebel und Raum der Ebene; die Orte kommen aus der Karte (trace/hr, trace/e39).

    python tools/qa/shots_l188.py --round r1 [--only horcher,pistole] [--size 1280x720]
Einzelbilder build/qa/creatures/level188/<runde>/<job>_<n>.png; Boegen (3 Spalten, beschriftet)
docs/screens/creatures/horcher/<runde>.jpg und docs/screens/creatures/wurzelpistole/<runde>.jpg.
"""
import argparse
import json
import math
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import ROOT, run  # noqa: E402

W0 = 'wait 40; grace 9999; '


def _map():
    with open(os.path.join(ROOT, 'data', 'maps', 'level188.json'), encoding='utf-8') as fh:
        s = '\n'.join(ln for ln in fh.read().splitlines() if not ln.lstrip().startswith('//'))
    return json.loads(s)


def _view(gx, gy, ang, d, eye=1.62, aim=0.08):
    """Spieler d m von (gx, gy) in Richtung ang, Blick auf den Punkt (aim m hoch)."""
    px, py = gx + math.cos(ang) * d, gy + math.sin(ang) * d
    yaw = math.atan2(gy - py, gx - px)
    pitch = -math.atan2(eye - aim, d)
    return f'pos {px:.3f} {py:.3f} {yaw:.4f} {pitch:.4f}; '


def jobs():
    M = _map()
    tr = [s for s in M['spawns'] if s['kind'] == 'trace']
    guns = [s for s in tr if s.get('set') == 'e39' and ({'moss', 'floor'} & set(s.get('tags', [])))]
    note = [s for s in tr if s.get('set') == 'e39' and 'note' in s.get('tags', [])]
    moss = [g for g in guns if 'moss' in g['tags']][0]
    floor = [g for g in guns if 'floor' in g['tags']][0]
    mx, my, myaw = moss['pos'][0], moss['pos'][1], moss.get('yaw', 0.0)
    fx, fy, fyaw = floor['pos'][0], floor['pos'][1], floor.get('yaw', 0.0)
    lamp = 'key T; wait 90; '
    H = ('Horcher (Auslegung, Level 188)',
         'pos 58.6 46.0 1.5708 0; ' + W0 + 'presence listener 6; wait 20; horcher pose 1; horcher place 3.6; wait 45; shot {out}/horcher_1.png; ' +
         lamp + 'shot {out}/horcher_2.png; key T; wait 30; '
         'horcher pose 2; horcher place 1.9; wait 40; shot {out}/horcher_3.png; ' +
         'horcher pose 4; horcher place 2.5; wait 45; shot {out}/horcher_4.png; ' +
         lamp + 'horcher pose 6; wait 25; shot {out}/horcher_5.png; key T; wait 30; '
         'horcher pose 3; horcher place 3.0; horcher yaw 0.0; wait 40; shot {out}/horcher_6.png; '
         'horcher pose 1; horcher place 9.0; wait 40; shot {out}/horcher_7.png; ' +
         lamp + 'shot {out}/horcher_8.png; key T; wait 20; '
         'horcher pose 5; horcher place 2.2; wait 30; shot {out}/horcher_9.png; quit')
    P = ('Wurzelpistole (Entity 39)',
         _view(mx, my, myaw + 2.4, 1.7) + W0 + 'wait 30; shot {out}/pistole_1.png; ' + lamp + 'shot {out}/pistole_2.png; key T; wait 30; ' +
         _view(mx, my, myaw + 2.0, 0.85, aim=0.1) + 'wait 60; shot {out}/pistole_3.png; ' +
         'court wrap 0; wait 50; shot {out}/pistole_4.png; ' + lamp + 'shot {out}/pistole_5.png; key T; wait 20; ' +
         _view(mx, my, myaw + 2.4, 2.4) + 'court recover 0; wait 30; court fire 0; wait 1; shot {out}/pistole_6.png; '
         'court wilt 0; court sprouts 5; wait 80; shot {out}/pistole_7.png; ' +
         _view(fx, fy, fyaw + 1.2, 1.3) + 'wait 40; ' + lamp + 'shot {out}/pistole_8.png; key T; wait 20; ' +
         _view(note[0]['pos'][0], note[0]['pos'][1], myaw + 3.6, 2.6) + 'wait 40; shot {out}/pistole_9.png; quit')
    return {'horcher': H, 'pistole': P}


def sheet(files, labels, out):
    tw, th, cols = 640, 360, 3
    rows = (len(files) + cols - 1) // cols
    S = Image.new('RGB', (cols * tw, rows * th), (16, 16, 16))
    dr = ImageDraw.Draw(S)
    for i, f in enumerate(files):
        x, y = (i % cols) * tw, (i // cols) * th
        S.paste(Image.open(f).convert('RGB').resize((tw, th), Image.LANCZOS), (x, y))
        t = f'{labels} - {os.path.basename(f)[:-4]}'
        dr.rectangle((x, y, x + 8 + 7 * len(t), y + 16), fill=(0, 0, 0))
        dr.text((x + 4, y + 2), t, fill=(255, 230, 120))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    S.save(out, quality=82)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--round', default='r1')
    ap.add_argument('--only', default='')
    ap.add_argument('--size', default='1280x720')
    ap.add_argument('--seed', type=int, default=3)
    a = ap.parse_args()
    J = jobs()
    if a.only:
        J = {k: v for k, v in J.items() if k in a.only.split(',')}
    raw = os.path.join(ROOT, 'build', 'qa', 'creatures', 'level188', a.round)
    os.makedirs(raw, exist_ok=True)
    dirs = {'horcher': 'horcher', 'pistole': 'wurzelpistole'}
    for job, (label, script) in J.items():
        for f in os.listdir(raw):
            if f.startswith(job + '_'):
                os.remove(os.path.join(raw, f))
        code, log = run(f'--level level188 --seed {a.seed}', script.format(out=raw.replace('\\', '/')),
                        userdir=os.path.join(ROOT, 'build', 'qa', 'ud_l188', job), size=a.size)
        warn = [ln.strip() for ln in log.splitlines() if 'Skript' in ln and ('kein' in ln or 'unbekannt' in ln or 'nicht' in ln)]
        files = sorted((os.path.join(raw, f) for f in os.listdir(raw) if f.startswith(job + '_') and f.endswith('.png')),
                       key=lambda p: int(os.path.basename(p)[len(job) + 1:-4]))
        out = sheet(files, label, os.path.join(ROOT, 'docs', 'screens', 'creatures', dirs[job], f'{a.round}.jpg')) if files else None
        print(f'{job}: exit {code}, {len(files)} Bilder, Bogen {out}', warn[:4], flush=True)


if __name__ == '__main__':
    main()
