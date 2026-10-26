"""Level 3 - "Das Umspannwerk": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.4).

    West: Naht aus Level 2  ->  Ziegelgaenge (rot, gelb, grau) mit Schaltraeumen, Stimmenraeumen und
    stillen Andachtsraeumen  ->  Ost: zwei Transformatorhallen (Kern hinter Gittern) und der Aufzug.

- Ziel: Strom fuer den Aufzug. In einigen Schaltraeumen haengen Sicherungskaesten (je Lauf 2-3).
- Der Aufzug (Nordost) faehrt erst, wenn man eine Sicherung bei sich hat -> Level 4.
- Gefahr: Hund (lauert) und das Laecheln im Stromausfall; Funken an den Schaltschraenken als Warnung.
- Andachtsraeume mit einer Statue: Sicherungspunkte, eigene Notbeleuchtung.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level3', 172, 132, seed=3003)
rng = m.rng
H = 3.0

KAMMER = {'schaltraum': 'schalt', 'lager3': 'schalt', 'stimmenraum': 'stimmen', 'andachtsraum': 'andacht'}
J, S, C = corridor_maze(m, 14, 6, 120, 126, 13, 3, 'gang3', 'ziegel_rot',
                        ['schaltraum', 'schaltraum', 'lager3', 'stimmenraum', 'andachtsraum'],
                        lambda t: KAMMER[t], height=H, floor=0.0, density=0.7, room_chance=0.7, light_every=6)


def brick_zone(x):
    return 'ziegel_rot' if x < 50 else ('ziegel_gelb' if x < 85 else 'ziegel_grau')


FIELDS = set()
for r in list(J.values()) + list(S.values()):
    if r is None:
        continue
    R = m.R(r)
    R.zone = brick_zone(R.cx)
    R.kw['circuit'] = 30 + int((R.cx - 14) // 27)
    FIELDS.add(R.kw['circuit'])
for r in C.values():
    R = m.R(r)
    if R.type != 'andachtsraum':
        R.kw['circuit'] = 30 + int((R.cx - 14) // 27)
m.blackout_circuits = set(FIELDS)

# --- Ankunft aus Level 2 (West) -------------------------------------------------------------------------------
west_j = min((v for k, v in J.items() if v is not None and k[0] == 0), key=lambda v: abs(m.R(v).cy - 66))
WJ = m.R(west_j)
ank = m.room(2, int(WJ.y0), 13, int(WJ.y1), 'naht', 'naht2', 0.0, 3.0, label='Ziegel und Kabel', circuit=50)
m.opening(ank, west_j)
for x in (4, 9):
    m.light(ank, x + 0.5, WJ.cy, 0, 0.6, 4.0)
m.transition('l2_l3', 'seam', 'level2', 6.0, WJ.cy, 0.0, rect=[2, int(WJ.y0), 4, int(WJ.y1)])

# --- Transformatorhallen (Ost): Kern hinter Gittern ---------------------------------------------------------
halls = []
for hy0, hy1 in ((8, 54), (58, 104)):
    hall = m.room(121, hy0, 168, hy1, 'trafohalle', 'halle', 0.0, 7.5, label='Transformatorhalle', circuit=51 + len(halls))
    halls.append(hall)
    HR = m.R(hall)
    # Kern: Rechteck 8 m vom Rand, Zaun mit Kollision (unsichtbarer Block unter dem Gitter)
    fx0, fy0, fx1, fy1 = HR.x0 + 8, HR.y0 + 8, HR.x1 - 8, HR.y1 - 8
    for x in range(fx0, fx1):
        for y in (fy0, fy1 - 1):
            m.obj('fence_panel', x + 0.5, y + 0.5, 0.0, 0.0, tag='fence')
    for y in range(fy0 + 1, fy1 - 1):
        for x in (fx0, fx1 - 1):
            m.obj('fence_panel', x + 0.5, y + 0.5, 0.0, math.pi / 2, tag='fence')
    m.block(hall, fx0, fy0, fx1, fy0 + 1, 3.0)
    m.block(hall, fx0, fy1 - 1, fx1, fy1, 3.0)
    m.block(hall, fx0, fy0 + 1, fx0 + 1, fy1 - 1, 3.0)
    m.block(hall, fx1 - 1, fy0 + 1, fx1, fy1 - 1, 3.0)
    for k, (tx, ty) in enumerate(((fx0 + 7, fy0 + 8), (fx1 - 8, fy0 + 8), (fx0 + 7, fy1 - 9), (fx1 - 8, fy1 - 9))):
        m.obj('transformer', tx + 0.5, ty + 0.5, 0.0, 0.0, variant=k, tag='machine')
        m.block(hall, tx - 1, ty - 1, tx + 2, ty + 2, 2.4)
    for x in range(int(HR.x0) + 5, int(HR.x1) - 3, 9):
        for y in range(int(HR.y0) + 4, int(HR.y1) - 2, 9):
            m.light(hall, x + 0.5, y + 0.5, 0, 1.0, 8.0, z=7.5 - 1.0, panel=False)
            m.obj('highbay', x + 0.5, y + 0.5, 7.5 - 0.01, 0.0, tag='light')
# Hallen an das Ganglabyrinth anschliessen: Verbindungsgaenge von den oestlichsten Kreuzungen
east_x = max(k[0] for k, v in J.items() if v is not None)
for hall in halls:
    HR = m.R(hall)
    js = [v for k, v in J.items() if v is not None and k[0] == east_x and HR.y0 + 2 <= m.R(v).y0 and m.R(v).y1 <= HR.y1 - 2]
    for v in js[:2]:
        JR = m.R(v)
        con = m.room(int(JR.x1) + 1, int(JR.y0), int(HR.x0) - 1, int(JR.y1), 'gang3', 'ziegel_grau', 0.0, H, circuit=52)
        m.opening(v, con)
        m.opening(con, hall)
        m.light(con, (JR.x1 + HR.x0) / 2, JR.cy, 0, 0.55, 4.0)
m.opening(halls[0], halls[1], width=4)

# --- Aufzug (Nordost): Vorraum und Kabine ---------------------------------------------------------------------
vor = m.room(140, 107, 160, 117, 'aufzugsvorraum', 'aufzug', 0.0, 3.2, label='Aufzug', circuit=55)
m.opening(halls[1], vor, width=3)
kabine = m.room(161, 109, 166, 115, 'kabine', 'aufzug', 0.0, 2.6, circuit=55)
m.door(vor, kabine, 'opening', 2)
m.light(vor, 150.5, 112.0, 0, 0.8, 6.0)
m.light(kabine, 163.5, 112.0, 0, 0.6, 3.0)
m.transition('l3_l4', 'elevator', 'level4', 163.5, 112.0, math.pi, rect=[162, 110, 166, 114], requires='fuse_station')

# --- Ausstattung: Kabelpritschen, Schaltschraenke, Sicherungskaesten, Statuen --------------------------------
for (a, b), r in S.items():
    R = m.R(r)
    if R.w > R.h:
        for x in range(int(R.x0), int(R.x1)):
            m.obj('cable_tray', x + 0.5, R.cy, H - 0.01, 0.0, variant=rng.randrange(4), tag='deco')
    else:
        for y in range(int(R.y0), int(R.y1)):
            m.obj('cable_tray', R.cx, y + 0.5, H - 0.01, math.pi / 2, variant=rng.randrange(4), tag='deco')
sanctums, switches = [], []
for (i, j), r in C.items():
    R = m.R(r)
    t = R.type
    if t == 'andachtsraum':
        # V11 (Wikidot "Level 3 - Electrical Station": Sanctums wie Kathedralen, klassische Saeulen, eine grosse
        # Statue, Zeichen am Sockel): hoeher als alles ringsum, vier Saeulen, ein Kronleuchter, die Statue
        R.kw['circuit'] = 60 + len(sanctums)
        R.height = 5.6
        m.obj('statue', R.cx, R.cy, 0.0, rng.random() * 6.28, tag='deco')
        m.block(r, int(R.cx), int(R.cy), int(R.cx) + 1, int(R.cy) + 1, 2.5, hidden=True)
        if R.w >= 6 and R.h >= 6:
            for px, py in ((R.x0 + 1, R.y0 + 1), (R.x1 - 2, R.y0 + 1), (R.x0 + 1, R.y1 - 2), (R.x1 - 2, R.y1 - 2)):
                m.obj('column', px + 0.5, py + 0.5, R.floor, 0.0, variant=int(round(R.height * 10)), tag='deco')
                m.block(r, px, py, px + 1, py + 1, R.height, hidden=True)
        m.obj('chandelier', R.cx, R.cy - 1.5, R.floor + R.height, 0.0, variant=0, tag='light')
        m.light(r, R.cx, R.cy - 1.5, 0, 0.8, 6.0, z=R.floor + R.height - 1.0, panel=False, color=(1.0, 0.86, 0.66))
        sanctums.append(r)
        continue
    m.light(r, R.cx, R.cy, 0, 0.65, 4.5)
    if t == 'schaltraum':
        switches.append(r)
        for k in range(min(3, int(R.h) - 1)):
            m.obj('switch_cabinet', R.x0 + 0.02, R.y0 + 1.0 + k * 1.3, 0.0, 0.0, variant=rng.randrange(3), tag='machine')
        m.spawn('fusebox', R.x1 - 0.02, R.cy, 0.0, math.pi, set='fuse_station')
    elif t == 'lager3':
        for k in range(2):
            m.obj('barrel', R.x1 - 0.7, R.y0 + 0.7 + k * 0.7, 0.0, 0.0, variant=2, tag='deco')
        m.spawn('crate', R.cx, R.cy, 0.0, tags=['storage'])
    if t == 'lager3' and m.wall_ok(R.x0 + 0.01, R.y1 - 1.0, 0.0):  # V11: Stahlregal mit Fund
        m.obj('shelf_unit', R.x0 + 0.01, R.y1 - 1.0, 0.0, 0.0, variant=1, tag='deco')
        m.spawn('item', R.x0 + 0.25, R.y1 - 0.75, 0.625, tags=['surface', 'shelf', t], group=800 + i * 20 + j)
    else:
        m.spawn('item', R.x1 - 0.6, R.y1 - 0.6, 0.0, tags=['floor', 'corner', t], group=800 + i * 20 + j)
    if rng.random() < 0.35:
        m.spawn('note', R.cx - 0.4, R.cy + 0.3, 0.0, tags=['floor'])
for (a, b), r in list(S.items())[::4]:
    R = m.R(r)
    p = (R.cx, R.y0 + 0.3) if R.w > R.h else (R.x1 - 0.3, R.cy)  # V11: an der Wand, nicht mitten im Gang
    m.spawn('item', p[0], p[1], 0.0, tags=['floor', 'tunnel', 'wall'], group=880 + int(R.cx) * 3 + int(R.cy))
# Hund: in den Hallen und an fernen Kreuzungen
for hall in halls:
    HR = m.R(hall)
    m.spawn('entity', HR.x0 + 3, HR.y0 + 3, 0.0, role='hound', tags=['hall'])
# V11: Buendel - Deckenschaechte in den Gaengen (ENTITY_BEHAVIOR 2.3: Level 3)
for (a, b), r in list(S.items())[2::6]:
    R = m.R(r)
    m.spawn('entity', R.cx, R.cy, 0.0, role='clump', tags=['vent'])

# --- Sicherungspunkte: Andachtsraeume (die Wesen meiden sie), Ankunft, Aufzug --------------------------------
roamers = [s_['pos'] for s_ in m.spawns if s_.get('role') in ('hound', 'clump')]
m.checkpoint('ankunft', 8.0, WJ.cy, 0.0, 'Ankunft aus den Rohrgängen')
m.obj('safety_light', 8.0, WJ.cy, 2.99, 0.0, tag='emergency')
for k, r in enumerate(sorted(sanctums, key=lambda r: m.R(r).cx)):
    R = m.R(r)
    if any(math.hypot(c['pos'][0] - R.cx, c['pos'][1] - R.cy) < 22 for c in m.checkpoints):
        continue
    if not all(math.hypot(p[0] - R.cx, p[1] - R.cy) >= 15 for p in roamers):
        continue
    m.checkpoint(f'andacht_{k}', R.cx - 1.0, R.cy, 0.0, 'Raum mit der Statue')
m.checkpoint('aufzug', 152.0, 112.0, 0.0, 'Vor dem Aufzug')
m.obj('safety_light', 152.0, 112.0, 3.19, 0.0, tag='emergency')

m.start = (8.0, WJ.cy, 0.0)
m.save()
