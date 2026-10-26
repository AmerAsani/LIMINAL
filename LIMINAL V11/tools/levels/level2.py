"""Level 2 - "Die Rohrgaenge": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.3).

Gestalt (Draufsicht): ein Netz enger Versorgungstunnel (3 m) mit Kammern dazwischen.

    West: Ankunft aus Level 1 (Rohrgang)  ->  Tunnelnetz  ->  Ost: Ziegel und Kabel nehmen zu -> Naht zu Level 3
                                             |  Mitte: heisse Abschnitte (Dampf, Ausdauer)  |

- Lichtfelder in Reihe: jedes Feld (ein Streifen des Netzes) haengt an einem eigenen Stromkreis. Faellt
  der Strom aus, wird ein ganzes Feld auf einmal dunkel - und im Dunkel wartet das Laecheln.
- Die meisten Tueren in den Waenden sind verschlossene Stahltueren (Ausstattung). Offen sind nur die
  Kammern: Pumpen, Maschinen, Lager, und einige Raeume, in denen nur stilles Dunkel ist.
- Sicherungspunkte liegen in Pumpenkammern mit Notbeleuchtung (eigener Stromkreis).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level2', 184, 120, seed=2002)
rng = m.rng
H = 3.0

KAMMER = {'pumpenraum': 'kammer', 'maschinenraum': 'kammer', 'lagerkammer': 'kammer', 'leerraum': 'leer'}
J, S, C = corridor_maze(m, 12, 6, 150, 114, 12, 3, 'tunnel', 'rohr',
                        ['pumpenraum', 'maschinenraum', 'lagerkammer', 'lagerkammer', 'leerraum'],
                        lambda t: KAMMER[t], height=H, floor=0.0, density=0.72, room_chance=0.65, light_every=5)


# Lichtfelder: Streifen von 30 m Breite je ein Stromkreis (in Reihe verdrahtet)
def field(x):
    return 20 + int((x - 12) // 30)


FIELD_CIRCUITS = set()
for r in list(J.values()) + list(S.values()):
    if r is None:
        continue
    R = m.R(r)
    R.kw['circuit'] = field(R.cx)
    FIELD_CIRCUITS.add(R.kw['circuit'])
    # heisse Mitte und Ziegel im Osten
    if 62 <= R.cx < 102:
        R.type, R.zone = 'tunnel_heiss', 'heiss'
    elif R.cx >= 122:
        R.zone = 'ziegel'
for r in C.values():
    R = m.R(r)
    R.kw['circuit'] = field(R.cx)
m.blackout_circuits = set(FIELD_CIRCUITS)

# --- Ankunft aus Level 1 (West) und Naht zu Level 3 (Ost) ---------------------------------------------------
west_j = min((v for k, v in J.items() if v is not None and k[0] == 0), key=lambda v: abs(m.R(v).cy - 60))
WJ = m.R(west_j)
ank = m.room(2, int(WJ.y0), 10, int(WJ.y1), 'naht', 'naht2', 0.0, 2.8, label='Rohrgang', circuit=40)
m.opening(ank, west_j)
m.light(ank, 6.0, WJ.cy, 0, 0.55, 3.6)
m.transition('l1_l2', 'seam', 'level1', 5.5, WJ.cy, 0.0, rect=[2, int(WJ.y0), 4, int(WJ.y1)])
for x in range(2, 10):
    m.obj('pipe_bundle', x + 0.5, WJ.y0 + 0.02, H - 0.2, 0.0, variant=2, tag='pipes')

east_j = min((v for k, v in J.items() if v is not None), key=lambda v: (-m.R(v).cx, abs(m.R(v).cy - 60)))
EJ = m.R(east_j)
naht = [m.room(int(EJ.x1) + 1, int(EJ.y0), 166, int(EJ.y1), 'naht', 'ziegel', 0.0, 3.0, circuit=41),
        m.room(167, int(EJ.y0), 182, int(EJ.y1), 'naht', 'naht3', 0.0, 3.2, circuit=41, label='Ziegel und Kabel')]
m.opening(east_j, naht[0])
m.opening(naht[0], naht[1])
for r in naht:
    R = m.R(r)
    for x in range(int(R.x0) + 2, int(R.x1) - 1, 5):
        m.light(r, x + 0.5, R.cy, 0, 0.6, 4.0)
m.transition('l2_l3', 'seam', 'level3', 177.5, EJ.cy, math.pi, rect=[180, int(EJ.y0), 182, int(EJ.y1)])

# --- Ausstattung der Tunnel: Rohrbuendel unter der Decke, Stahltueren, Ventile, Manometer ----------------
for (a, b), r in S.items():
    R = m.R(r)
    hot = R.type == 'tunnel_heiss'
    if R.w > R.h:  # waagrecht: Rohre an der Nordwand
        for x in range(int(R.x0), int(R.x1)):
            m.obj('pipe_bundle', x + 0.5, R.y1 - 0.02, H - 0.32, math.pi, variant=(4 if hot else 0) + rng.randrange(3), tag='pipes')
        if rng.random() < 0.5 and R.w >= 6:
            sx, v = R.x0 + 1.5 + rng.random() * (R.w - 3), rng.randrange(3)  # (Zufallsfolge wie zuvor)
            if all(m.wall_ok(sx + d, R.y0 + 0.005, math.pi / 2) for d in (-0.55, 0.0, 0.55)):  # V11: nur vor Wand
                m.obj('steel_door', sx, R.y0 + 0.005, 0.0, math.pi / 2, variant=v, tag='deco')
        if rng.random() < 0.35:
            x = R.x0 + 1 + rng.random() * (R.w - 2)
            m.obj('valve_wheel', x, R.y1 - 0.4, 1.4, -math.pi / 2, variant=rng.randrange(2), tag='deco')
            m.obj('gauge', x + 0.6, R.y1 - 0.05, 1.7, -math.pi / 2, variant=rng.randrange(4), tag='deco')
    else:  # senkrecht: Rohre an der Westwand
        for y in range(int(R.y0), int(R.y1)):
            m.obj('pipe_bundle', R.x0 + 0.02, y + 0.5, H - 0.32, -math.pi / 2, variant=(4 if hot else 0) + rng.randrange(3), tag='pipes')
        if rng.random() < 0.5 and R.h >= 6:
            sy, v = R.y0 + 1.5 + rng.random() * (R.h - 3), rng.randrange(3)
            if all(m.wall_ok(R.x1 - 0.005, sy + d, math.pi) for d in (-0.55, 0.0, 0.55)):
                m.obj('steel_door', R.x1 - 0.005, sy, 0.0, math.pi, variant=v, tag='deco')
    if hot and rng.random() < 0.6:
        m.obj('pipe_run', R.cx, R.cy, H - 0.4, 0.0 if R.w > R.h else math.pi / 2, variant=1, tag='steam')
    # gelegentlich Rollwagen und Faesser im Gang
    if rng.random() < 0.15:
        m.obj('cart', R.cx, R.cy + (0.6 if R.w > R.h else 0.0), 0.0, 0.0 if R.w > R.h else math.pi / 2, tag='deco')

# --- Kammern ------------------------------------------------------------------------------------------------
pump_rooms = []
for (i, j), r in C.items():
    R = m.R(r)
    t = R.type
    if t == 'leerraum':
        R.kw['light'] = 'unlit'
        continue
    m.light(r, R.cx, R.cy, 0, 0.75, 4.5)
    if t == 'pumpenraum':
        m.obj('pump', R.cx, R.cy, 0.0, 0.0, tag='machine')
        pump_rooms.append(r)
    elif t == 'maschinenraum':
        m.obj('pump', R.x0 + 1.6, R.cy, 0.0, math.pi / 2, tag='machine')
        for dy in (0.0, -1.0, 1.0, -2.0, 2.0):  # V11: eine Wandstelle ohne Tuer
            if R.y0 + 0.5 <= R.cy + dy <= R.y1 - 0.5 and m.wall_ok(R.x1 - 0.05, R.cy + dy, math.pi):
                m.obj('power_box', R.x1 - 0.05, R.cy + dy, 1.2, math.pi, tag='machine')
                break
    else:
        for k in range(3):
            m.obj('barrel', R.x0 + 0.7 + k * 0.65, R.y0 + 0.7, 0.0, rng.random() * 6.28, variant=rng.randrange(3), tag='deco')
        m.spawn('crate', R.cx, R.cy, 0.0, tags=['storage'])
    if t == 'lagerkammer' and m.wall_ok(R.x1 - 0.01, R.y1 - 1.0, math.pi):  # V11: Stahlregal, Fund auf dem Boden darin
        m.obj('shelf_unit', R.x1 - 0.01, R.y1 - 1.0, 0.0, math.pi, variant=1, tag='deco')
        m.spawn('item', R.x1 - 0.25, R.y1 - 1.25, 0.625, tags=['surface', 'shelf', t], group=600 + i * 20 + j)
    else:
        m.spawn('item', R.x1 - 0.6, R.y1 - 0.6, 0.0, tags=['floor', 'corner', t], group=600 + i * 20 + j)
    if rng.random() < 0.4:
        m.spawn('note', R.cx + 0.4, R.cy - 0.3, 0.0, tags=['floor'])

# Gegenstaende in den Gaengen, Buendel an Kreuzungen (fuer spaeter), Laecheln in den Feldern
for (a, b), r in list(S.items())[::3]:
    R = m.R(r)
    # V11: an die Wand gegenueber den Rohren gestellt, nicht mitten in den Gang
    p = (R.cx, R.y0 + 0.3) if R.w > R.h else (R.x1 - 0.3, R.cy)
    m.spawn('item', p[0], p[1], 0.0, tags=['floor', 'tunnel', 'wall'], group=700 + int(R.cx) * 3 + int(R.cy))
for k, r in list(J.items())[::7]:
    if r is None:
        continue
    R = m.R(r)
    m.spawn('entity', R.cx, R.cy, 0.0, role='clump', tags=['vent'])

# --- Sicherungspunkte: Pumpenkammern mit Notbeleuchtung (eigener Stromkreis) ----------------------------
roamers = [s_['pos'] for s_ in m.spawns if s_.get('role') in ('hound', 'clump')]


def safe_room(cid, r, label):
    R = m.R(r)
    R.kw['circuit'] = 90 + len(m.checkpoints)
    m.checkpoint(cid, R.cx, R.cy - 0.6, 0.0, label)
    m.obj('safety_light', R.cx, R.cy - 0.6, R.floor + R.height - 0.01, 0.0, tag='emergency')


m.checkpoint('ankunft', 7.0, WJ.cy, 0.0, 'Ankunft aus dem Lagerdeck')
m.obj('safety_light', 7.0, WJ.cy, 2.79, 0.0, tag='emergency')
for name, tx, ty in (('west', 40, 40), ('nord', 50, 95), ('mitte', 85, 60), ('heiss', 80, 25), ('ost', 125, 85)):
    cands = sorted(pump_rooms + [r for r in C.values() if m.R(r).type == 'lagerkammer'],
                   key=lambda r: math.hypot(m.R(r).cx - tx, m.R(r).cy - ty))
    for r in cands:
        R = m.R(r)
        if all(math.hypot(p[0] - R.cx, p[1] - R.cy) >= 15 for p in roamers) and not any(
                math.hypot(c['pos'][0] - R.cx, c['pos'][1] - R.cy) < 20 for c in m.checkpoints):
            safe_room(f'kammer_{name}', r, f'Pumpenkammer {name.capitalize()}')
            break
m.checkpoint('naht', m.R(naht[1]).cx - 2.0, m.R(naht[1]).cy, 0.0, 'Ziegel und Kabel')
m.obj('safety_light', m.R(naht[1]).cx - 2.0, m.R(naht[1]).cy, 3.19, 0.0, tag='emergency')

m.start = (7.0, WJ.cy, 0.0)
m.save()
