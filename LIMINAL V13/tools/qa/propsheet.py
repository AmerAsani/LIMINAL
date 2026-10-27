"""V13.3: Kontaktboegen der Ausstattung (PROMPT_V13_3 Teil 1).

Je Objektklasse (Modellname + Variante) einer Ebene eine Testszene: eine Kabine mit Boden, Waenden, Decke und Licht
der Ebene (Zone und Raumtyp des ersten Vorkommens), das Objekt so befestigt wie im Spiel (Boden, Wand, Decke,
Durchbruch). Aufnahmen aus 4 Winkeln, aus 50 cm und 2 m Abstand (zur Oberflaeche des Objekts), einmal im Licht der
Ebene und einmal nur mit einer Lampe, die streifend von der Seite leuchtet (wie die Taschenlampe). Bogen: 4 x 4
Bilder je Klasse -> docs/screens/props/<level>/<runde>/<name>[_v<N>].jpg, Uebersicht docs/screens/<level>/.

    python tools/qa/propsheet.py level1 [--round vorher] [--only crate_a,rack@3] [--size 1280x720]

Voraussetzung: build/qa/props_<level>.json (tools/qa/propdump.py; fehlt die Datei, wird sie erzeugt).
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

QA = os.path.join(ROOT, 'build', 'qa')

CEILING_EXTRA = {'batten', 'highbay', 'safety_light', 'ceiling_vent', 'ceiling_globe', 'hanging_bulb', 'cable_tray'}
WALL_EXTRA = {'gauge', 'power_box', 'outlet', 'vent_grille', 'junction_box', 'switch_cabinet'}


def load_jsonc(path):
    with open(path, encoding='utf-8') as fh:
        lines = [ln for ln in fh if not ln.lstrip().startswith('//')]
    return json.loads(''.join(lines))


def class_key(name, v):
    return f'{name}@{v}' if v else name


def mount_of(p):
    """Wie haelt das Objekt? floor | wall | ceiling | opening."""
    n = p['name']
    lo, hi, s = p['lo'], p['hi'], p['s']
    if not p['room'] or not p['room'][0] and not p['solid']:
        if n in mapkit.IN_OPENING or n in mapkit.DOOR_MODELS:
            return 'opening'
    if n in mapkit.IN_OPENING:
        return 'opening'
    if n in mapkit.CEILING_MOUNT or n in CEILING_EXTRA:
        return 'ceiling'
    if n in mapkit.WALL_MOUNT or n in WALL_EXTRA:
        return 'wall'
    if n in mapkit.FLOOR_MOUNT:
        return 'floor'
    top = p['p'][2] + hi[2] * s[2]
    if p['ceil'] - top < 0.15 and p['p'][2] > p['floor'] + 0.5 * (p['ceil'] - p['floor']):
        return 'ceiling'
    if p['back'] < 0.25 and lo[0] * s[0] > -0.08:
        return 'wall'
    return 'floor'


def pick_classes(dump, only):
    """Erstes Vorkommen je Klasse (Name, Variante)."""
    seen = {}
    for p in dump['props']:
        if p['name'] in ('?', 'pillar_label'):
            continue
        k = class_key(p['name'], p['v'])
        if k not in seen:
            seen[k] = p
    keys = sorted(seen)
    if only:
        want = set(only)
        keys = [k for k in keys if k in want or k.split('@')[0] in want]
    return [(k, seen[k]) for k in keys]


def room_of(p, rooms):
    """Boden und Hoehe des Raums, in dem das Objekt steht (nicht der Kachel - die kann ein Block sein)."""
    if p['room'] and len(p['room']) > 2 and p['room'][2]:
        rc = p['room'][2]
        for r in rooms:
            if r[:4] == rc:
                return r[4], r[5]
    return p['floor'], max(2.6, p['ceil'] - p['floor'])


def build_studio(level, classes, dump_rooms):
    """Karte und Level der Testszenen. Liefert die Kabinen (Klasse, Raum, Lage des Objekts)."""
    L = load_jsonc(os.path.join(ROOT, 'data', 'levels', level + '.json'))
    sid = 'studio_' + level
    # Zone/Raumtyp muessen in der Ebene bekannt sein
    zones = {z['key'] for z in L.get('zones', [])}
    types = {t['key'] for t in L.get('room_types', L.get('types', []))} if isinstance(L.get('room_types', L.get('types')), list) else set()
    booths = []
    X0 = 7  # links ein Gang ueber die ganze Hoehe (x 2..5), daran die erste Kabine jeder Reihe
    x, y, rowh = X0, 2, 0
    W = 240
    lay = []
    for k, p in classes:
        lo, hi, s = p['lo'], p['hi'], p['s']
        ext = max(abs(lo[0]), abs(hi[0]), abs(lo[1]), abs(hi[1])) * max(s[0], s[1])
        n = max(7, int(math.ceil(2 * ext + 4.6)))
        if x + n + 2 > W and x > X0:
            x, y, rowh = X0, y + rowh + 2, 0
        lay.append((k, p, x, y + 5, n))  # 5 Kacheln Vorraum (Durchbrueche) oberhalb jeder Kabine
        x += n + 2
        rowh = max(rowh, n + 5)
    H = y + rowh + 14
    m = mapkit.Map(sid, W, H, seed=7)
    first = classes[0][1]
    zone0 = first['room'][1] if first['room'] and first['room'][1] else (L['zones'][0]['key'] if L.get('zones') else 'a')
    type0 = first['room'][0] if first['room'] and first['room'][0] else 'room'
    corridor = m.room(2, 2, 5, H - 11, type0, zone0, 0.0, 3.0, label='Gang')
    for yy in range(6, H - 11, 8):
        m.light(corridor, 3.5, yy + 0.5, 0, 0.6, 4.5)
    lobby = m.room(2, H - 9, 9, H - 2, type0, zone0, 0.0, 3.0, label='Eingang')
    m.light(lobby, 5.5, H - 5.5, 0, 0.8, 5.0)
    m.door(corridor, lobby, 'door', 1, at=3.5)
    m.start = (5.5, H - 4.5, -math.pi / 2)
    last, last_y = None, None
    for k, p, bx, by, n in lay:
        lo, hi, s = p['lo'], p['hi'], p['s']
        rtype = p['room'][0] if p['room'] and p['room'][0] else type0
        zone = p['room'][1] if p['room'] and p['room'][1] else zone0
        if zones and zone not in zones:
            zone = zone0
        rfl, rh = room_of(p, dump_rooms)
        hgt = max(2.6, min(12.0, rh))
        r = m.room(bx, by, bx + n, by + n, rtype, zone, 0.0, round(hgt, 2), label=k)
        for fx in (0.3, 0.7):
            for fy in (0.3, 0.7):
                m.light(r, bx + n * fx, by + n * fy, 0, 0.75 + 0.05 * hgt, max(4.5, n * 0.55))
        mt = mount_of(p)
        cx, cy = bx + n / 2.0, by + n / 2.0
        if mt == 'wall':
            pos = (cx, by + max(0.004, min(p['back'], 0.3)), p['p'][2] - rfl)
            yaw = math.pi / 2
        elif mt == 'ceiling':
            pos = (cx, cy, hgt - (rfl + rh - p['p'][2]))
            yaw = p['yaw']
        elif mt == 'opening':
            ext_x = (hi[0] - lo[0]) * s[0]
            ext_y = (hi[1] - lo[1]) * s[1]
            width = max(1, min(n - 2, int(math.ceil(max(ext_x, ext_y) - 0.05))))
            ax0 = int(cx - width / 2.0) - 1
            ante = m.room(ax0, by - 4, ax0 + width + 2, by - 2, rtype, zone, 0.0, round(hgt, 2), sealed=True)
            m.light(ante, ax0 + width / 2.0 + 1, by - 3, 0, 0.6, 4.0)
            d = m.door(r, ante, 'opening', width, at=cx)
            dx0, dy0, dx1, dy1 = m.doors[d]['rect']
            yaw = 0.0 if ext_x > ext_y else math.pi / 2
            pos = ((dx0 + dx1) / 2.0, (dy0 + dy1) / 2.0, 0.0)
        else:
            pos = (cx, cy, 0.0)
            yaw = p['yaw']
        sc = p['s'] if any(abs(v - 1) > 1e-3 for v in p['s']) else None
        m.obj(p['name'], pos[0], pos[1], pos[2], yaw, scale=sc, variant=p['v'])
        booths.append({'key': k, 'mount': mt, 'room': [bx, by, bx + n, by + n], 'h': hgt, 'pos': pos, 'yaw': yaw,
                       'lo': lo, 'hi': hi, 's': p['s']})
        if last is not None and last_y == by:
            m.door(last, r, 'door', 1, at=by + 1.5)
        else:
            m.door(corridor, r, 'door', 1, at=by + 1.5)
        last, last_y = r, by
    errs = [e for e in m.validate() if 'Sicherungspunkt' not in e]
    if errs:
        for e in errs[:30]:
            print('Studio-Karte:', e)
    # Dateien: build/qa/studio/levels/<sid>.json und build/qa/studio/maps/<sid>.json
    sd = os.path.join(QA, 'studio')
    os.makedirs(os.path.join(sd, 'levels'), exist_ok=True)
    os.makedirs(os.path.join(sd, 'maps'), exist_ok=True)
    with open(os.path.join(sd, 'maps', sid + '.json'), 'w', encoding='utf-8') as fh:
        json.dump(m.to_json(), fh, separators=(',', ':'))
    L['id'] = sid
    L['map'] = sid
    L['name'] = L['title'] = L['display_title'] = 'Studio ' + level
    L['presence'] = {'hound': False, 'smiler': False, 'gestalt': False, 'blackout': False, 'clump': 0.0}
    L.pop('next_level', None)
    lp = os.path.join(sd, 'levels', sid + '.json')
    with open(lp, 'w', encoding='utf-8') as fh:
        json.dump(L, fh, indent=1)
    return lp, booths


def aabb_world(b):
    """Mittelpunkt und Halbachsen (lokal) des Objekts, Drehung yaw."""
    lo, hi, s = b['lo'], b['hi'], b['s']
    c = [(lo[i] + hi[i]) * 0.5 * s[i] for i in range(3)]
    h = [(hi[i] - lo[i]) * 0.5 * s[i] for i in range(3)]
    cy_, sy_ = math.cos(b['yaw']), math.sin(b['yaw'])
    wc = (b['pos'][0] + c[0] * cy_ - c[1] * sy_, b['pos'][1] + c[0] * sy_ + c[1] * cy_, b['pos'][2] + c[2])
    return wc, h


def support(h, yaw, d):
    """Halbe Ausdehnung der Box in Richtung d (Welt)."""
    cy_, sy_ = math.cos(-yaw), math.sin(-yaw)
    lx, ly = d[0] * cy_ - d[1] * sy_, d[0] * sy_ + d[1] * cy_
    return abs(h[0] * lx) + abs(h[1] * ly) + abs(h[2] * d[2])


ROWS = [('50cm', 0.5, False), ('2m', 2.0, False), ('50cm_lampe', 0.5, True), ('2m_lampe', 2.0, True)]


def shots_for(b):
    if b['mount'] == 'wall':
        az, el = [-55, -18, 18, 55], 8
    elif b['mount'] == 'ceiling':
        az, el = [35, 125, 215, 305], -32
    elif b['mount'] == 'opening':
        az, el = [25, 155, 205, 335], 8
    else:
        az, el = [35, 125, 215, 305], 20
    base = b['yaw'] if b['mount'] != 'wall' else math.pi / 2
    t, h = aabb_world(b)
    x0, y0, x1, y1 = b['room']
    if b['mount'] == 'opening':  # beide Seiten des Durchbruchs (Vorraum mitnutzen)
        y0 -= 4
    out = []
    for rname, dist, lamp in ROWS:
        for i, a in enumerate(az):
            ar, er = base + math.radians(a), math.radians(el + (6 if dist < 1 else 0))
            d = (math.cos(er) * math.cos(ar), math.cos(er) * math.sin(ar), math.sin(er))
            r = support(h, b['yaw'], d)
            cam = [t[k] + d[k] * (r + dist) for k in range(3)]
            cam[0] = min(max(cam[0], x0 + 0.2), x1 - 0.2)
            cam[1] = min(max(cam[1], y0 + 0.2), y1 - 0.2)
            cam[2] = min(max(cam[2], 0.25), b['h'] - 0.2)
            light = None
            if lamp:  # streifend: von der Seite (+70 Grad), knapp ueber der Hoehe des Ziels
                la, le = ar + math.radians(72), math.radians(6 if b['mount'] != 'ceiling' else -20)
                ld = (math.cos(le) * math.cos(la), math.cos(le) * math.sin(la), math.sin(le))
                lr = support(h, b['yaw'], ld) + 1.1
                lp = [t[k] + ld[k] * lr for k in range(3)]
                lp[0] = min(max(lp[0], x0 + 0.15), x1 - 0.15)
                lp[1] = min(max(lp[1], y0 + 0.15), y1 - 0.15)
                lp[2] = min(max(lp[2], 0.15), b['h'] - 0.15)
                light = lp
            out.append((rname, i, cam, t, light))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('--round', default='aktuell')
    ap.add_argument('--only', default='')
    ap.add_argument('--size', default='1280x720')
    ap.add_argument('--settle', type=int, default=14)
    a = ap.parse_args()
    dp = os.path.join(QA, f'props_{a.level}.json')
    if not os.path.exists(dp):
        import propdump
        propdump.dump(a.level)
    with open(dp, encoding='utf-8') as fh:
        dump = json.load(fh)
    only = [s for s in a.only.split(',') if s]
    classes = pick_classes(dump, only)
    if not classes:
        raise SystemExit('keine Klassen')
    lp, booths = build_studio(a.level, classes, dump['rooms'])
    shotdir = os.path.join(QA, 'shots', a.level, a.round)
    os.makedirs(shotdir, exist_ok=True)
    lines = ['wait 90']
    plan = []
    for b in booths:
        for rname, i, cam, t, light in shots_for(b):
            fn = os.path.join(shotdir, f"{b['key'].replace('@', '_v')}__{rname}_{i}.png").replace('\\', '/')
            lines.append('camat {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} 55'.format(*cam, *t))
            if light:
                lines.append('worldlight 0')
                lines.append('testlight {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} 1.0 26'.format(*light, *t))
            else:
                lines.append('worldlight 1')
                lines.append('testlight off')
            lines.append(f'wait {a.settle}')
            lines.append(f'shot {fn}')
            plan.append((b['key'], rname, i, fn))
    lines.append('quit')
    sp = os.path.join(QA, 'studio', f'script_{a.level}_{a.round}.txt')
    with open(sp, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(lines))
    code, log = run(['--level-file', lp, '--seed', '1'], '@' + sp, os.path.join(QA, 'ud_sheet_' + a.level), size=a.size,
                    timeout=7200)
    if code != 0:
        print('Spiel beendet mit', code)
        for ln in log.splitlines():
            if 'FEHLER' in ln.upper() or 'error' in ln:
                print('  ', ln[:200])
    compose(a.level, a.round, booths, plan)


def compose(level, rnd, booths, plan):
    from PIL import Image, ImageDraw
    out = os.path.join(ROOT, 'docs', 'screens', 'props', level, rnd)
    os.makedirs(out, exist_ok=True)
    tw, th = 400, 225
    over = []
    for b in booths:
        S = Image.new('RGB', (tw * 4 + 110, th * 4 + 30), (24, 24, 24))
        d = ImageDraw.Draw(S)
        d.text((8, 8), f"{level}  {b['key']}  ({b['mount']})  Zeilen: 50 cm / 2 m im Licht der Ebene, 50 cm / 2 m nur Lampe streifend",
               fill=(235, 235, 235))
        for k, b2, rname, i, fn in [(None, b, *x[1:]) for x in plan if x[0] == b['key']]:
            row = [r[0] for r in ROWS].index(rname)
            try:
                im = Image.open(fn).convert('RGB').resize((tw, th))
            except OSError:
                im = Image.new('RGB', (tw, th), (90, 0, 0))
            S.paste(im, (110 + i * tw, 30 + row * th))
            if i == 0:
                d.text((6, 30 + row * th + th // 2), rname, fill=(235, 235, 235))
            if rname == '2m' and i == 0:
                over.append((b['key'], im.copy()))
        S.save(os.path.join(out, b['key'].replace('@', '_v') + '.jpg'), quality=84)
    # Uebersicht der Ebene (2 m, erster Winkel)
    cols = 6
    rows = (len(over) + cols - 1) // cols
    U = Image.new('RGB', (cols * 320, rows * 200), (24, 24, 24))
    d = ImageDraw.Draw(U)
    for j, (k, im) in enumerate(over):
        x, y = (j % cols) * 320, (j // cols) * 200
        U.paste(im.resize((320, 180)), (x, y))
        d.rectangle([x, y + 180, x + 320, y + 200], fill=(0, 0, 0))
        d.text((x + 4, y + 184), k, fill=(255, 255, 255))
    od = os.path.join(ROOT, 'docs', 'screens', level)
    os.makedirs(od, exist_ok=True)
    U.save(os.path.join(od, f'props_uebersicht_{rnd}.jpg'), quality=82)
    print(f'{len(booths)} Klassen -> {out}')


if __name__ == '__main__':
    main()
