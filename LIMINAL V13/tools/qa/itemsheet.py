"""V13.3: Kontaktboegen der Fundstuecke und Interaktionsobjekte (PROPS_Level0.md, Teil B).

Gleiches Prinzip wie propsheet.py (4 Winkel, 50 cm / 2 m, Licht der Ebene und streifende Lampe), aber die Objekte
stehen so im Spiel, wie das Spiel sie setzt - mit ihren festen Materialien (Etiketten, Automatenfront, Schild):
  - Gegenstaende auf dem Boden ueber Spawnpunkte der Karte (je Lauf eine Art: energy, almond, battery, fuse_office)
  - Notiz und Sicherungskasten ueber Spawnpunkte der Karte (der zweite Kasten wird per Taste E geleert)
  - Automat und Aufzug als Entities der Leveldatei in Nischen; der Raum-Seed wird so gesucht, dass sie gerade stehen
  - Notausgang ueber die Fundregeln (weit vom Ursprung); seine Lage liest ein Vorlauf (propdump)
Testszene: Level 0 (Zonen, Licht), Karte build/qa/itemstudio/. Boegen: docs/screens/props/items/<runde>/<klasse>.jpg,
Uebersicht docs/screens/props/items/uebersicht_<runde>.jpg.

    python tools/qa/itemsheet.py --round r1 [--only energy,vending] [--size 1280x720]
"""
import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'levels'))
from game import ROOT, run  # noqa: E402
import mapkit  # noqa: E402
from propsheet import load_jsonc  # noqa: E402

QA = os.path.join(ROOT, 'build', 'qa', 'itemstudio')
OUT = os.path.join(ROOT, 'docs', 'screens', 'props', 'items')
MASK = (1 << 64) - 1
GOLD = 0x9E3779B97F4A7C15
LEVEL_ID = 'level0'  # Notizen fester Karten gehoeren zur Ebene (data/notes.json "maps")
MAP_ID = 'itemstudio'
ITEMS = {'energy': 'energy', 'almond': 'almond', 'battery': 'battery', 'fuse': 'fuse_office'}


# --- core/rng.cpp (bitgenau) ---------------------------------------------------------------------
def mix64(z):
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
    return z ^ (z >> 31)


def harg(a):
    if isinstance(a, str):
        h = 0xCBF29CE484222325
        for b in a.encode('utf-8'):
            h = ((h ^ b) * 0x100000001B3) & MASK
        return h
    return a & MASK


def hash64(*args):
    h = 0x6A09E667F3BCC909
    for a in args:
        h = mix64(((h ^ harg(a)) + GOLD) & MASK)
    return h


def hfloat(*args):
    return (hash64(*args) >> 11) * (1.0 / 9007199254740992.0)


def seed_for_yaw(k, want, start):
    """Raum-Seed, mit dem die Entity k der Leveldatei (world: spawnLevelEntities) in Richtung want steht."""
    salt = hash64(LEVEL_ID, 'map')
    for s in range(start, start + 400000):
        y = hfloat((s ^ salt) & MASK, 0x5EA8, k) * 2 * math.pi
        d = (y - want + math.pi) % (2 * math.pi) - math.pi
        if abs(d) < 0.004:
            return s, y
    raise SystemExit('kein Seed gefunden')


# --- Testszene -----------------------------------------------------------------------------------
def build_studio(kind):
    L = load_jsonc(os.path.join(ROOT, 'data', 'levels', 'level0.json'))
    m = mapkit.Map(MAP_ID, 220, 24, seed=7)
    zone, rt = 'vorzimmer', 'zimmer'
    booths = {}
    corr = m.room(2, 17, 212, 20, 'flur', zone, 0.0, 3.0, label='Gang')
    for x in range(6, 210, 8):
        m.light(corr, x + 0.5, 18.5, 0, 0.6, 4.5)

    def booth(key, x0, y0, x1, y1, h=3.0, rtype=rt, **kw):
        r = m.room(x0, y0, x1, y1, rtype, zone, 0.0, h, label=key, **kw)
        for fx in (0.3, 0.7):
            for fy in (0.3, 0.7):
                m.light(r, x0 + (x1 - x0) * fx, y0 + (y1 - y0) * fy, 0, 0.8, 5.0)
        m.door(r, corr, 'door', 1, at=x0 + 1.5)
        return r
    booth('item', 8, 6, 15, 13)
    m.spawn('item', 11.5, 9.5)
    booths['item'] = {'mount': 'floor', 'pos': (11.5, 9.5, 0.0), 'yaw': math.pi / 2, 'room': (8, 6, 15, 13), 'h': 3.0}
    booth('note', 17, 6, 24, 13)
    m.spawn('note', 20.5, 9.5, 0.0, 0.4)
    booths['note'] = {'mount': 'floor', 'pos': (20.5, 9.5, 0.0), 'yaw': 0.4, 'room': (17, 6, 24, 13), 'h': 3.0,
                      'lo': (-0.105, -0.075, 0.0), 'hi': (0.105, 0.075, 0.012)}
    for key, x0 in (('fusebox', 26), ('fusebox_empty', 35)):
        booth(key, x0, 6, x0 + 7, 13, 4.2)
        m.spawn('fusebox', x0 + 3.5, 6.0, 0.0, math.pi / 2, set='fuse_office')
        # Spiel: Hoehe = Boden + 1,2 m (Sector::fuseboxes), Ursprung 4 cm vor der Wand
        booths[key] = {'mount': 'wall', 'pos': (x0 + 3.5, 6.04, 1.2), 'yaw': math.pi / 2, 'room': (x0, 6, x0 + 7, 13), 'h': 4.2,
                       'lo': (-0.04, -0.23, 1.08), 'hi': (0.14, 0.23, 1.70)}
    # Automat und Aufzug in Nischen (Entities der Leveldatei, Seed fuer gerade Ausrichtung)
    s_v, yv = seed_for_yaw(0, math.pi / 2, 1000)
    s_e, ye = seed_for_yaw(1, math.pi / 2, 5000)
    v = booth('vending', 44, 6, 53, 15)
    a = m.room(47, 4, 50, 5, 'st_vend', zone, 0.0, 3.0, seed=s_v)
    m.light(a, 48.5, 4.5, 0, 0.5, 3.0)
    m.door(a, v, 'opening', 3, at=48.5)
    booths['vending'] = {'mount': 'wall', 'pos': (48.5, 4.5, 0.0), 'yaw': yv, 'room': (47.2, 4, 49.8, 15), 'h': 3.0,
                         'lo': (-0.48, -0.46, 0.0), 'hi': (0.33, 0.46, 1.86)}
    e = booth('elevator', 55, 6, 64, 15)
    a = m.room(58, 1, 61, 5, 'st_elev', zone, 0.0, 3.0, seed=s_e)
    m.door(a, e, 'opening', 3, at=59.5)
    booths['elevator'] = {'mount': 'wall', 'pos': (59.5, 3.5, 0.0), 'yaw': ye, 'room': (58.2, 3.7, 60.8, 15), 'h': 3.0,
                          'lo': (-0.07, -1.14, 0.0), 'hi': (0.05, 1.14, 2.64)}
    booth('exit_door', 200, 6, 208, 14, 2.8)
    booths['exit_door'] = {'mount': 'wall', 'pos': None, 'yaw': 0.0, 'room': (200, 6, 208, 14), 'h': 2.8,
                           'lo': (0.0, -0.53, 0.0), 'hi': (0.09, 0.53, 2.40)}
    m.start = (5.5, 18.5, 0.0)
    sd = QA
    os.makedirs(os.path.join(sd, 'levels'), exist_ok=True)
    os.makedirs(os.path.join(sd, 'maps'), exist_ok=True)
    with open(os.path.join(sd, 'maps', MAP_ID + '.json'), 'w', encoding='utf-8') as fh:
        json.dump(m.to_json(), fh, separators=(',', ':'))
    L['id'] = LEVEL_ID
    L['map'] = MAP_ID
    L['name'] = L['title'] = L['display_title'] = 'Studio Fundstuecke'
    L['presence'] = {'hound': False, 'smiler': False, 'gestalt': False, 'blackout': False, 'clump': 0.0}
    L['room_types'] = L['room_types'] + [{'key': 'st_vend', 'label': 'Nische', 'category': 'normal'},
                                         {'key': 'st_elev', 'label': 'Nische', 'category': 'normal'}]
    L['entities'] = [{'prefab': 'vending_machine', 'room_types': ['st_vend'], 'chance': 1.0},
                     {'prefab': 'elevator', 'room_types': ['st_elev'], 'chance': 1.0}]
    f = dict(L.get('finds', {}))
    f.update({'note': 0.0, 'battery': 0.0, 'battery_dark': 0.0, 'exit_start': 150, 'exit_range': 1, 'exit_max': 1.0})
    L['finds'] = f
    sr = dict(L.get('spawn_rules', {}))
    sr['item'] = {'count': [1, 1], 'table': {ITEMS[kind]: 1.0}}
    sr['note'] = {'count': [1, 1]}
    sr['fusebox'] = {'count': [2, 2]}
    L['spawn_rules'] = sr
    lp = os.path.join(sd, 'levels', 'itemstudio_' + kind + '.json')
    with open(lp, 'w', encoding='utf-8') as fh:
        json.dump(L, fh, indent=1)
    return lp, booths


# --- Kameras -------------------------------------------------------------------------------------
ROWS = [('50cm', 0.5, False), ('2m', 2.0, False), ('50cm_lampe', 0.5, True), ('2m_lampe', 2.0, True)]
ITEM_BOX = {'energy': ((-0.11, -0.034, 0.0), (0.11, 0.034, 0.024)), 'almond': ((-0.055, -0.055, 0.0), (0.055, 0.055, 0.262)),
            'battery': ((-0.017, -0.017, 0.0), (0.017, 0.017, 0.0645)), 'fuse': ((-0.088, -0.019, 0.0), (0.088, 0.019, 0.038))}


def center_half(b):
    lo, hi = b['lo'], b['hi']
    c = [(lo[i] + hi[i]) / 2 for i in range(3)]
    h = [(hi[i] - lo[i]) / 2 for i in range(3)]
    cy, sy = math.cos(b['yaw']), math.sin(b['yaw'])
    p = b['pos']
    # Spielkoordinaten: Modell-y ist gespiegelt gegenueber Blender, die Huelle ist symmetrisch genug
    return (p[0] + c[0] * cy - c[1] * sy, p[1] + c[0] * sy + c[1] * cy, p[2] + c[2]), h


def support(h, yaw, d):
    cy, sy = math.cos(-yaw), math.sin(-yaw)
    lx, ly = d[0] * cy - d[1] * sy, d[0] * sy + d[1] * cy
    return abs(h[0] * lx) + abs(h[1] * ly) + abs(h[2] * d[2])


def shots(key, b):
    t, h = center_half(b)
    if b['mount'] == 'wall':
        az, el = [-55, -18, 18, 55], 8
    else:
        az, el = [35, 125, 215, 305], 24
    x0, y0, x1, y1 = b['room']
    out = []
    for rname, dist, lamp in ROWS:
        for i, a in enumerate(az):
            ar, er = b['yaw'] + math.radians(a), math.radians(el + (8 if dist < 1 else 0))
            d = (math.cos(er) * math.cos(ar), math.cos(er) * math.sin(ar), math.sin(er))
            r = support(h, b['yaw'], d)
            cam = [t[k] + d[k] * (r + dist) for k in range(3)]
            cam[0] = min(max(cam[0], x0 + 0.2), x1 - 0.2)
            cam[1] = min(max(cam[1], y0 + 0.2), y1 - 0.2)
            cam[2] = min(max(cam[2], 0.12), b['h'] - 0.15)
            light = None
            if lamp:
                la, le = ar + math.radians(72), math.radians(8)
                ld = (math.cos(le) * math.cos(la), math.cos(le) * math.sin(la), math.sin(le))
                lr = support(h, b['yaw'], ld) + 1.0
                lp = [t[k] + ld[k] * lr for k in range(3)]
                lp[0] = min(max(lp[0], x0 + 0.15), x1 - 0.15)
                lp[1] = min(max(lp[1], y0 + 0.15), y1 - 0.15)
                lp[2] = min(max(lp[2], 0.1), b['h'] - 0.15)
                light = lp
            fov = 55 if b['mount'] == 'wall' else (26 if dist < 1 else 40)  # kleine Fundstuecke: enger Ausschnitt, gleicher Abstand
            out.append((rname, i, cam, t, light, fov))
    return out


def script_for(classes, booths, shotdir, settle):
    lines = ['wait 90', 'vmdebug 7']
    plan = []
    for key in classes:
        b = booths[key]
        x0, y0, x1, y1 = b['room']
        if key == 'fusebox_empty':  # Sicherung herausnehmen: vor den Kasten, hinsehen, E
            px, py = b['pos'][0], b['pos'][1] + 1.0
            lines += [f'pos {px:.2f} {py:.2f} {-math.pi / 2:.4f} 0.75', 'wait 30', 'key E', 'wait 60']
        lines += [f'pos {(x0 + x1) / 2:.2f} {y1 - 1.0:.2f}', 'wait 25']
        for rname, i, cam, t, light, fov in shots(key, b):
            fn = os.path.join(shotdir, f'{key}__{rname}_{i}.png').replace('\\', '/')
            lines.append('camat {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {}'.format(*cam, *t, fov))
            if light:
                lines += ['worldlight 0', 'testlight {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} 1.0 26'.format(*light, *t)]
            else:
                lines += ['worldlight 1', 'testlight off']
            lines += [f'wait {settle}', f'shot {fn}']
            plan.append((key, rname, i, fn))
    lines.append('quit')
    return lines, plan


def compose(rnd, plan, keys):
    from PIL import Image, ImageDraw
    od = os.path.join(OUT, rnd)
    os.makedirs(od, exist_ok=True)
    tw, th = 400, 225
    over = []
    for key in keys:
        S = Image.new('RGB', (tw * 4 + 110, th * 4 + 30), (24, 24, 24))
        d = ImageDraw.Draw(S)
        d.text((8, 8), f'Fundstuecke  {key}  Zeilen: 50 cm / 2 m im Licht von Level 0, 50 cm / 2 m nur Lampe streifend', fill=(235, 235, 235))
        for k, rname, i, fn in plan:
            if k != key:
                continue
            row = [r[0] for r in ROWS].index(rname)
            try:
                im = Image.open(fn).convert('RGB').resize((tw, th))
            except OSError:
                im = Image.new('RGB', (tw, th), (90, 0, 0))
            S.paste(im, (110 + i * tw, 30 + row * th))
            if i == 0:
                d.text((6, 30 + row * th + th // 2), rname, fill=(235, 235, 235))
            if rname == '50cm' and i == 1:
                over.append((key, im.copy()))
        S.save(os.path.join(od, key + '.jpg'), quality=84)
    cols = 4
    rows = (len(over) + cols - 1) // cols
    U = Image.new('RGB', (cols * 400, rows * 245), (24, 24, 24))
    d = ImageDraw.Draw(U)
    for j, (k, im) in enumerate(over):
        x, y = (j % cols) * 400, (j // cols) * 245
        U.paste(im, (x, y))
        d.text((x + 4, y + 228), k, fill=(255, 255, 255))
    U.save(os.path.join(OUT, f'uebersicht_{rnd}.jpg'), quality=82)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--round', default='aktuell')
    ap.add_argument('--only', default='')
    ap.add_argument('--size', default='1280x720')
    ap.add_argument('--settle', type=int, default=14)
    a = ap.parse_args()
    allk = ['energy', 'almond', 'battery', 'fuse', 'note', 'fusebox', 'fusebox_empty', 'vending', 'elevator', 'exit_door']
    want = [k for k in allk if not a.only or k in a.only.split(',')]
    shotdir = os.path.join(QA, 'shots', a.round)
    os.makedirs(shotdir, exist_ok=True)
    plan = []
    # Vorlauf: Lage des Notausgangs
    lp, booths = build_studio('energy')
    if 'exit_door' in want:
        dp = os.path.join(QA, 'exits.json').replace('\\', '/')
        run(['--level-file', lp, '--seed', '1'], 'wait 40; pos 204 12; wait 40; propdump ' + dp + '; quit',
            os.path.join(QA, 'ud'), size='640x360', quality=0)
        with open(dp, encoding='utf-8') as fh:
            ex = json.load(fh).get('exits', [])
        ex = [e for e in ex if e[0] > 190]
        if not ex:
            print('kein Notausgang gefunden')
            want.remove('exit_door')
        else:
            booths['exit_door']['pos'] = (ex[0][0], ex[0][1], ex[0][2])
            booths['exit_door']['yaw'] = ex[0][3]
    first = True
    for item in ['energy', 'almond', 'battery', 'fuse']:
        keys = [k for k in want if k == item or (first and k not in ITEMS)]
        if not keys:
            continue
        lp, b2 = build_studio(item)
        for k in b2:
            if k != 'exit_door':
                booths[k] = b2[k]
        if item in keys:
            lo, hi = ITEM_BOX[item]
            booths[item] = dict(booths['item'], lo=lo, hi=hi)
        lines, pl = script_for(keys, booths, shotdir, a.settle)
        sp = os.path.join(QA, f'script_{a.round}_{item}.txt')
        with open(sp, 'w', encoding='utf-8') as fh:
            fh.write('\n'.join(lines))
        code, log = run(['--level-file', lp, '--seed', '1'], '@' + sp, os.path.join(QA, 'ud'), size=a.size, timeout=3600)
        if code != 0:
            print('Spiel beendet mit', code)
        for ln in log.splitlines():
            if 'Fundstueck' in ln or 'FEHLER' in ln.upper() or 'unbekannt' in ln:
                print('  ', ln[:180])
        plan += pl
        first = False
    compose(a.round, plan, want)
    print(len(want), 'Klassen ->', os.path.join(OUT, a.round))


if __name__ == '__main__':
    main()
