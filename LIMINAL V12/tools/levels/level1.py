"""Level 1 - "Die Lagerdecks" (V12, LEVELSPEC_1.md / LOOK_1.md; Wikidot Level 1, Revision 83).

Draufsicht (x nach rechts, y nach unten):

    +-------------------------+-----------------------+-------------------------+
    | Aquila A1 (Ankunft)     | Gothic G1             | Gild: Ocker | Blaugrau  |
    |  Stuetzen 1 m, Rohre    | runde Saeulen,        |  Regale, Kisten         |
    +-----------+  Aquila A2  +  Boegen zwischen      +-------------+-----------+
    |           |             | Gothic G2             | Altrosa     | Gruen     |
    | Aquila A3 (dunkler, 2-m-Stuetzen)               |             |           |
    +--+--+--+--+--+--+--+--+-+ Gothic G3             +-------------+-----------+
    | Vorraeume: Krankenzimmer, Bilderraum ... (Schwingtueren mit gruenem Schild zur Halle)
    +----------------------------------------------+
    | Gaenge: Betonlabyrinth mit kleinen Raeumen   |==== Rohrgang (Rohre und Messuhren nehmen zu) ==> Level 2
    +----------------------------------------------+

Hallen (Wiki): weit, fleckige Waende, vereinzelt Betonstuetzen, Tiefgarage x Lager, Kisten auf Regalen und am Boden,
Pfuetzen unter tropfenden Rohren, Haufen aus Holz, Verpackung und Blech, Schaltkaesten, Tische, farbige Stuetzen.
Gaenge (Wiki): eng, schwach beleuchtet, Beton, waermer; Schwingtueren, manchmal mit Notausgangsschild; Raeume ohne
Zweck: Bueros (Rechner, haengende Birne), Ziegelraeume, Krankenzimmer (Bett in der Mitte, Beistelltische),
Gummizellen (Stuhl in der Mitte), weite Raeume mit Bildern an den Waenden und auf dem Boden.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level1', 252, 212, seed=1201)
rng = m.rng

HALL_H, GOTH_H, GILD_H, GANG_H = 4.2, 5.6, 6.0, 2.6
COOL, WARM, GILDL, BULB = (1.0, 0.98, 0.92), (1.0, 0.91, 0.78), (1.0, 0.93, 0.80), (1.0, 0.84, 0.62)
C_AQUILA, C_GOTHIC, C_GILD, C_GANG, C_ROOMS = 1, 2, 3, 4, 5
m.blackout_circuits = {C_AQUILA, C_GOTHIC, C_GILD, C_GANG}

# --- Hallen: Aquila ----------------------------------------------------------------------------------------------
a1 = m.room(2, 2, 70, 58, 'halle', 'aquila', 0.0, HALL_H, pillars=[12, 12, 1, 8, 8], beams=0.55, circuit=C_AQUILA,
            label='Aquila', tags=['hall'])
a2 = m.room(71, 2, 120, 58, 'halle', 'aquila', 0.0, HALL_H, pillars=[12, 12, 1, 77, 8], beams=0.55, circuit=C_AQUILA,
            tags=['hall'])
a3 = m.room(2, 59, 120, 96, 'halle', 'aquila_dunkel', 0.0, HALL_H, pillars=[13, 12, 2, 9, 66], beams=0.55,
            circuit=C_AQUILA, tags=['hall'])
aquila = [a1, a2, a3]
for y in (9, 29, 47):
    m.opening(a1, a2, width=6, at=y)
for x in (16, 47):
    m.opening(a1, a3, width=8, at=x)
for x in (86, 109):
    m.opening(a2, a3, width=8, at=x)
# freistehende, fleckige Wandscheiben (die Hallen sind weit, aber nicht leer)
for (r, x0, y0, x1, y1) in ((a1, 24, 18, 25, 33), (a1, 40, 40, 56, 41), (a2, 92, 22, 93, 40), (a3, 30, 72, 46, 73),
                            (a3, 70, 80, 71, 92), (a3, 96, 68, 97, 82)):
    m.block(r, x0, y0, x1, y1, HALL_H, hidden=False)

# --- Hallen: Gothic (Boegen, runde Saeulen) -----------------------------------------------------------------------
g1 = m.room(121, 2, 182, 30, 'gothic', 'gothic', 0.0, GOTH_H, circuit=C_GOTHIC, label='Gothic', tags=['hall'])
g2 = m.room(121, 31, 182, 64, 'gothic', 'gothic', 0.0, GOTH_H, circuit=C_GOTHIC, tags=['hall'])
g3 = m.room(121, 65, 182, 96, 'gothic', 'gothic', 0.0, GOTH_H, circuit=C_GOTHIC, tags=['hall'])
gothic = [g1, g2, g3]


def arch_door(a, b, width, at):
    d = m.doors[m.door(a, b, 'wide', width, at=at, top=4.6)]
    x0, y0, x1, y1 = d['rect']
    yaw = 0.0 if d['axis'] == 0 else math.pi / 2
    m.obj(f'arch_tall{width}', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, yaw)


for x in (131, 151, 171):
    arch_door(g1, g2, 4, x + 0.0)
for x in (138, 162):
    arch_door(g2, g3, 4, x + 0.0)
arch_door(a2, g1, 3, 15.5)
arch_door(a2, g2, 4, 44)
arch_door(a3, g3, 4, 80)


def columns(r, step=7, rows=2):
    """Runde Saeulen in Reihen entlang der laengeren Seite; jede steht auf einem unsichtbaren Block (Halt)."""
    R = m.R(r)
    ys = [R.y0 + round((R.h) * (k + 1) / (rows + 1)) for k in range(rows)]
    for y in ys:
        for x in range(R.x0 + 5, R.x1 - 4, step):
            m.block(r, x, y, x + 1, y + 1, GOTH_H, hidden=True)
            m.obj('column_round', x + 0.5, y + 0.5, 0.0, 0.0)


for r in gothic:
    columns(r)

# --- Hallen: Gild (farbig, Regale, die meisten Kisten) -----------------------------------------------------------
gd = [m.room(183, 2, 214, 47, 'gild', 'gild_ocker', 0.0, GILD_H, circuit=C_GILD, label='Gild', tags=['hall']),
      m.room(215, 2, 249, 47, 'gild', 'gild_blau', 0.0, GILD_H, circuit=C_GILD, tags=['hall']),
      m.room(183, 48, 214, 96, 'gild', 'gild_rosa', 0.0, GILD_H, circuit=C_GILD, tags=['hall']),
      m.room(215, 48, 249, 96, 'gild', 'gild_gruen', 0.0, GILD_H, circuit=C_GILD, tags=['hall'])]
m.opening(gd[0], gd[1], width=6, at=24)
m.opening(gd[2], gd[3], width=6, at=72)
m.opening(gd[0], gd[2], width=6, at=198)
m.opening(gd[1], gd[3], width=8, at=232)
m.opening(g1, gd[0], width=5, at=16)
m.opening(g3, gd[2], width=5, at=80)


def racks(r, along_x=True):
    """Hochregale in Doppelreihen (2,7-m-Felder, 2,2 m tief) mit 3,2 m breiten Gassen; Kisten liegen darin."""
    R = m.R(r)
    y = R.y0 + 4
    while y + 3 <= R.y1 - 4:
        n = int((R.w - 8) // 2.7)
        x0 = R.x0 + 4
        for k in range(n):
            last = 4 if k == n - 1 else 0
            m.obj('rack', x0 + 1.35 + k * 2.7, y + 1.0, 0.0, 0.0, variant=rng.randrange(4) | last, tag='rack')
        m.block(r, x0, y, x0 + int(n * 2.7 + 0.99), y + 2, 5.25)  # Halt, Schatten, Wege: Regale sind nicht begehbar
        y += 6


for r in gd:
    racks(r)

# --- Vorraeume zwischen Halle und Gaengen (Krankenzimmer, Bilderraeume, Bueros, Schleusen) ------------------------
band = []
x = 2
plan = [('krankenzimmer', 12), ('schleuse', 4), ('bilderraum', 14), ('buero', 6), ('schleuse', 4), ('krankenzimmer', 10),
        ('ziegelraum', 6), ('schleuse', 4), ('bilderraum', 12), ('gummizelle', 5), ('schleuse', 4), ('buero', 6),
        ('krankenzimmer', 9)]
for t, w in plan:
    if x + w > 120:
        break
    zone = {'krankenzimmer': 'kranken', 'bilderraum': 'bilder', 'buero': 'buero', 'ziegelraum': 'ziegel',
            'gummizelle': 'polster', 'schleuse': 'gaenge'}[t]
    h = {'krankenzimmer': 3.0, 'bilderraum': 3.2}.get(t, GANG_H)
    band.append(m.room(x, 97, x + w, 109, t, zone, 0.0, h, circuit=C_ROOMS if t != 'schleuse' else C_GANG))
    x += w + 1
for a, b in zip(band, band[1:]):
    m.door(a, b, 'door', 1)

# Hallentueren: Schwingtueren mit gruenem Notausgangsschild darueber (Wiki: im Dunkeln das einzige Licht)
hall_doors = []
for r in band:
    if m.R(r).type == 'schleuse':
        d = m.door(a3, r, 'door', 2, top=2.2)
        hall_doors.append(d)
for r in [band[0], band[2]]:
    hall_doors.append(m.door(a3, r, 'door', 2, top=2.2))
for di in hall_doors:
    D = m.doors[di]
    x0, y0, x1, y1 = D['rect']
    cx = (x0 + x1) / 2
    m.obj('swing_door2', cx, (y0 + y1) / 2, 0.0, math.pi / 2)  # Fluegel stehen zum Vorraum hin offen
    m.obj('exit_sign_green', cx, y0 - 0.005, 2.42, -math.pi / 2, tag='emergency')  # an der Hallenwand, Blick in die Halle

# --- Gaenge: Betonlabyrinth mit kleinen Raeumen --------------------------------------------------------------------
GANG_ROOMS = ['buero', 'buero', 'ziegelraum', 'gummizelle', 'zimmer', 'zimmer', 'zimmer', 'krankenzimmer', 'bilderraum']
ZONE_OF = {'buero': 'buero', 'ziegelraum': 'ziegel', 'gummizelle': 'polster', 'zimmer': 'zimmer', 'krankenzimmer': 'kranken',
           'bilderraum': 'bilder'}
J, S, C = corridor_maze(m, 2, 110, 140, 209, 9, 2, 'gang', 'gaenge', GANG_ROOMS, lambda t: ZONE_OF[t], height=GANG_H,
                        density=0.78, room_chance=0.8, circuit=C_GANG, light_every=7)
maze = list(J.values()) + list(S.values())
maze = [r for r in maze if r is not None]
cells = list(C.values())
for r in cells:
    m.R(r).kw['circuit'] = C_ROOMS
# Vorraeume an das Labyrinth: jede Schleuse und jedes zweite Zimmer bekommt eine Tuer nach Sueden
for r in band:
    R = m.R(r)
    below = [g for g in maze if m.R(g).y0 == 110 and min(R.x1, m.R(g).x1) - max(R.x0, m.R(g).x0) >= 1]
    if below and (R.type == 'schleuse' or rng.random() < 0.5):
        m.door(r, below[0], 'door', 1)

# --- Rohrgang nach Level 2: Rohre und Messuhren nehmen zu, der Gang wird enger ------------------------------------
east = [g for g in maze if m.R(g).x1 >= 136]
gate = min(east, key=lambda g: abs(m.R(g).cy - 150))
G = m.R(gate)
pipe_segs = []
x, y = G.x1 + 1, int(G.cy) - 1
# (Laenge, Versatz quer, Breite, Hoehe): der Gang verspringt, wird schmaler und niedriger
specs = [(16, 0, 3, 2.9), (14, 1, 3, 2.8), (12, 0, 3, 2.7), (12, 1, 2, 2.6), (14, 1, 2, 2.6), (12, 1, 2, 2.5)]
prev = gate
for (L, dy, w, h) in specs:
    seg = m.room(x, y + dy, x + L, y + dy + w, 'rohrgang', 'rohr', 0.0, h, circuit=C_GANG, tags=['pipes'])
    m.opening(prev, seg, width=2)
    pipe_segs.append(seg)
    prev = seg
    x += L + 1
last = m.R(pipe_segs[-1])
m.transition('l1_l2', 'seam', 'level2', last.x1 - 1.5, last.cy, 0.0, rect=[last.x1 - 3, last.y0, last.x1, last.y1])

# --- Start: Aquila, an der Westwand (die meisten kommen hier aus Level 0 an) --------------------------------------
m.start = (3.6, 30.5, 0.0)
m.transition('l0_l1', 'seam', '', 3.6, 30.5, 0.0)  # Ankunft aus Level 0 (flackernde Wand), kein Rueckweg

# --- Licht ---------------------------------------------------------------------------------------------------------


def battens(r, step_x=6, step_y=6, inten=0.85, radius=6.5, color=COOL, dead=0.12, model='batten', z_off=0.0):
    """Leuchtbaender in Reihen unter der Decke, zwischen den Stuetzen; einige tot. In Hallen mit Stuetzenraster
    haengen sie in den Feldern zwischen den Unterzuegen (nie unter einem Unterzug)."""
    R = m.R(r)
    top = R.floor + R.height
    ox, oy = rng.randrange(step_x), rng.randrange(step_y)
    grid = R.kw.get('pillars')
    n = 0
    for y in range(R.y0 + 3 + oy, R.y1 - 2, step_y):
        for x in range(R.x0 + 3 + ox, R.x1 - 2, step_x):
            px, py = x + 0.5, y + 0.5
            if grid:
                sp, spy, th, gx, gy = (list(grid) + [0] * 5)[:5]
                if any((x - gx - d) % sp == 0 for d in range(-1, th + 1)) or any((y - gy - d) % spy == 0 for d in range(-1, th + 1)):
                    continue
            if m.blocked(px, py) or m.blocked(px - 0.8, py) or m.blocked(px + 0.8, py):
                continue
            if rng.random() < dead:
                continue
            m.obj(model, px, py, top, 0.0, tag='light')
            m.light(r, px, py, 0, inten, radius, z=top - 0.45 + z_off, panel=False, color=color)
            n += 1
    return n


for r in aquila:
    battens(r, 4, 4, 1.0 if r != a3 else 0.8, 7.5, COOL, 0.12 if r != a3 else 0.22)
    if rng.random() < 0.5:
        m.R(r).kw['flicker'] = 2 if r == a3 else 0
for r in gothic:
    R = m.R(r)
    # wenige Pendelstrahler zwischen den Saeulenreihen, warm
    for x in range(R.x0 + 8, R.x1 - 4, 8):
        for y in (R.y0 + R.h // 2, R.y0 + 3, R.y1 - 4):
            if not m.blocked(x + 0.5, y + 0.5):
                m.obj('highbay', x + 0.5, y + 0.5, GOTH_H, 0.0, tag='light')
                m.light(r, x + 0.5, y + 0.5, 0, 1.05, 11.0, z=GOTH_H - 0.8, panel=False, color=(1.0, 0.90, 0.74))
for r in gd:
    R = m.R(r)
    top = R.floor + R.height
    y = R.y0 + 4 + 2 + 2  # Mitte der Gasse hinter der ersten Doppelreihe (Reihen: y0+4, alle 6 m, 2 m tief)
    rows = [R.y0 + 2] + [R.y0 + 4 + 2 + 2 + 6 * k for k in range(10) if R.y0 + 8 + 6 * k < R.y1 - 2]
    for yy in rows:
        for x in range(R.x0 + 3, R.x1 - 2, 4):
            if rng.random() < 0.05:
                continue
            m.obj('batten', x + 0.5, yy + 0.0, top, 0.0, tag='light')
            m.light(r, x + 0.5, yy + 0.0, 0, 1.0, 7.5, z=top - 0.45, panel=False, color=GILDL)

# Gaenge: die Leuchten des Labyrinths sind schwach und warm (corridor_maze setzt sie); Raeume eigene
for r in maze:
    R = m.R(r)
    R.lights = [l[:5] + [0, 0, round(R.floor + R.height - 0.05, 3), 1] + list(WARM) for l in R.lights]
    for l in R.lights:
        l[3] = 0.45


def bulb(r, x=None, y=None, inten=0.7, radius=4.0):
    R = m.R(r)
    x = R.cx if x is None else x
    y = R.cy if y is None else y
    m.obj('hanging_bulb', x, y, R.floor + R.height, 0.0, tag='light')
    m.light(r, x, y, 0, inten, radius, z=R.floor + R.height - 0.71, panel=False, color=BULB)


def wall_spot(r, inward=0.35, margin=1):
    faces = m.wall_faces(r, margin=margin)
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


rooms_all = band + cells
for r in rooms_all:
    R = m.R(r)
    t = R.type
    top = R.floor + R.height
    if t == 'buero':
        bulb(r, R.cx + 0.3, R.cy - 0.3, inten=1.0, radius=5.0)
        p = wall_spot(r, 0.45)
        if p:
            m.obj('desk_crt', p[0], p[1], R.floor, p[2] + math.pi / 2, variant=rng.randrange(3))
            m.obj('chair', p[0] + math.cos(p[2]) * 0.75, p[1] + math.sin(p[2]) * 0.75, R.floor, p[2] + math.pi)
    elif t == 'ziegelraum':
        bulb(r, inten=0.55, radius=3.5)
    elif t == 'krankenzimmer':
        battens(r, 3, 3, 0.7, 4.5, (0.95, 1.0, 1.0), 0.0, z_off=0.0)
        if not R.lights:
            m.obj('batten', R.cx, R.cy, top, 0.0, tag='light')
            m.light(r, R.cx, R.cy, 0, 0.7, 4.5, z=top - 0.45, panel=False, color=(0.95, 1.0, 1.0))
        yaw = 0.0 if R.w >= R.h else math.pi / 2
        m.obj('hospital_bed', R.cx, R.cy, R.floor, yaw)
        m.block(r, int(R.cx - 1), int(R.cy - 1), int(R.cx + 1), int(R.cy + 1), 0.7, hidden=True)
        for s in (-1, 1):
            dx, dy = (-math.sin(yaw) * s * 0.95, math.cos(yaw) * s * 0.95)
            if not m.blocked(R.cx + dx, R.cy + dy):
                m.obj('side_table', R.cx + dx * 1.15, R.cy + dy * 1.15, R.floor, yaw)
    elif t == 'gummizelle':
        m.obj('batten', R.cx, R.cy, top, 0.0, tag='light')
        m.light(r, R.cx, R.cy, 0, 0.6, 3.6, z=top - 0.45, panel=False, color=(1.0, 0.97, 0.90))
        for (fx, fy, fyaw) in m.wall_faces(r, inset=0.0, margin=0):
            m.obj('padded_panel', fx, fy, R.floor, fyaw, tag='deco')
        m.obj('chair', R.cx, R.cy, R.floor, rng.random() * 6.28)
    elif t == 'bilderraum':
        n = 0
        prev_spot = None
        for (fx, fy, fyaw) in m.wall_faces(r, margin=1):
            if prev_spot and abs(fx - prev_spot[0]) + abs(fy - prev_spot[1]) < 1.6 and abs(fyaw - prev_spot[2]) < 0.1:
                continue
            u = rng.random()
            if u < 0.35:
                m.obj(rng.choice(['painting_a', 'painting_b', 'painting_c', 'painting_d', 'painting_e', 'painting_f']),
                      fx, fy, R.floor + 1.55, fyaw, tag='deco')
                prev_spot = (fx, fy, fyaw)
            elif u < 0.5:  # Wiki: auch auf dem Boden - an die Wand gelehnt
                m.obj(rng.choice(['painting_lean_a', 'painting_lean_b', 'painting_lean_c']), fx, fy, R.floor, fyaw, tag='deco')
                prev_spot = (fx, fy, fyaw)
        for k in range(max(2, int(R.w * R.h / 30))):
            px = R.x0 + 1.5 + rng.random() * (R.w - 3)
            py = R.y0 + 1.5 + rng.random() * (R.h - 3)
            m.light(r, px, py, 0, 0.75, 5.0, z=top - 0.05, panel=True, color=(1.0, 0.92, 0.80))
    elif t == 'zimmer':
        if rng.random() < 0.7:
            bulb(r, inten=0.5, radius=3.4)
    elif t == 'schleuse':
        m.light(r, R.cx, R.cy, 0, 0.45, 3.2, z=top - 0.05, panel=True, color=WARM)

# --- Rohrgang: Rohre unter der Decke und an den Waenden, Messuhren; je weiter, desto dichter --------------------
for k, s in enumerate(pipe_segs):
    R = m.R(s)
    top = R.floor + R.height
    dens = (k + 1) / len(pipe_segs)
    for j in range(1 + int(dens * 4)):
        y = R.y0 + 0.3 + (j % max(1, R.h)) * 0.35 if R.h > 1 else R.y0 + 0.3
        z = top - 0.12 - 0.17 * (j // max(1, R.h))
        m.obj('pipe_run', R.cx, min(R.y1 - 0.3, y), z, 0.0, scale=[R.w, 1, 1], variant=j % 2, tag='pipes')
    for (fx, fy, fyaw) in m.wall_faces(s, margin=1):
        if rng.random() < 0.08 + 0.25 * dens:
            m.obj('gauge', fx, fy, R.floor + 1.5 + rng.random() * 0.3, fyaw, variant=rng.randrange(2))
    for x in range(R.x0 + 2, R.x1 - 1, 5):
        m.light(s, x + 0.5, R.cy, 0, 0.5 - 0.2 * dens, 3.6, z=top - 0.05, panel=True, color=WARM)

# --- Hallen: Rohre unter der Decke, Pfuetzen darunter, Kisten, Haufen, Schaltkaesten, Tische, farbige Stuetzen -----
for r in aquila:
    R = m.R(r)
    top = R.floor + R.height
    # Rohrbuendel entlang x unter der Decke (neben den Unterzuegen), alle 18-24 m
    y = R.y0 + 5
    while y < R.y1 - 3:
        L = R.w - 2
        m.obj('pipe_run', R.cx, y + 0.3, top - 0.62, 0.0, scale=[L, 1, 1], variant=0, tag='pipes')
        m.obj('pipe_run', R.cx, y + 0.55, top - 0.68, 0.0, scale=[L, 1, 1], variant=1, tag='pipes')
        # undichte Stellen: Pfuetzen darunter (Wiki: Aquila)
        for k in range(int(L / 9)):
            px = R.x0 + 2 + rng.random() * (L - 2)
            if not m.blocked(px, y + 0.45):
                m.spawn('deco', px, y + 0.45 + rng.uniform(-0.4, 0.4), 0.0, rng.random() * 6.28, set='puddle', weight=1.0)
        y += rng.randint(18, 24)
    # Schaltkaesten an Waenden, Tische, Haufen
    faces = m.wall_faces(r, margin=1)
    for _ in range(max(2, int(R.w * R.h / 900))):
        if faces:
            fx, fy, fyaw = rng.choice(faces)
            m.obj('power_box', fx, fy, R.floor + 1.5, fyaw, tag='deco')
    for _ in range(int(R.w * R.h / 700)):
        px, py = R.x0 + 3 + rng.random() * (R.w - 6), R.y0 + 3 + rng.random() * (R.h - 6)
        if not m.blocked(px, py) and not m.blocked(px + 1, py) and not m.blocked(px - 1, py):
            m.obj(rng.choice(['debris_a', 'debris_b']), px, py, R.floor, rng.random() * 6.28)
    for _ in range(int(R.w * R.h / 1400)):
        px, py = R.x0 + 4 + rng.random() * (R.w - 8), R.y0 + 4 + rng.random() * (R.h - 8)
        if not any(m.blocked(px + dx, py + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
            m.obj('work_table', px, py, R.floor, rng.choice([0.0, math.pi / 2]))


def crate_stack(r, px, py, yaw):
    """Zwei bis vier Kisten, zum Teil gestapelt. Gibt True zurueck, wenn gesetzt."""
    if any(m.blocked(px + dx, py + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
        return False
    R = m.R(r)
    c, s = math.cos(yaw), math.sin(yaw)
    m.obj('crate_c', px, py, R.floor, yaw)
    m.obj('crate_b', px + c * 1.05 - s * 0.1, py + s * 1.05 + c * 0.1, R.floor, yaw + rng.uniform(-0.2, 0.2))
    if rng.random() < 0.6:
        m.obj('crate_a', px + rng.uniform(-0.15, 0.15), py + rng.uniform(-0.1, 0.1), R.floor + 0.75, yaw + rng.uniform(-0.4, 0.4))
    if rng.random() < 0.5:
        m.obj('crate_a', px - s * 0.9, py + c * 0.9, R.floor, yaw + rng.uniform(-0.5, 0.5))
    m.block(r, int(math.floor(px - 0.6)), int(math.floor(py - 0.6)), int(math.floor(px - 0.6)) + 2, int(math.floor(py - 0.6)) + 2, 0.75,
            hidden=True)
    return True


crate_spots = []
for r in aquila + gd + gothic:
    R = m.R(r)
    dens = 1400 if r in gd else (1100 if r in aquila else 2600)
    for _ in range(int(R.w * R.h / dens)):
        px, py = R.x0 + 3 + rng.random() * (R.w - 6), R.y0 + 3 + rng.random() * (R.h - 6)
        if crate_stack(r, px, py, rng.random() * 6.28):
            crate_spots.append((r, px, py))

# farbige Stuetzen (Merkpunkte): etwa jede sechste Stuetze in Aquila
for r in aquila:
    R = m.R(r)
    sp, spy, th, px, py = (list(R.kw['pillars']) + [0] * 5)[:5]
    colors = ['blau', 'gelb', 'rot', 'gruen'] if th == 1 else ['blau', 'gelb']
    for ty in range(R.y0 + 1, R.y1 - 1):
        for tx in range(R.x0 + 1, R.x1 - 1):
            if Map.pillar_at(R, tx, ty) and Map.pillar_at(R, tx - 1, ty) is False and Map.pillar_at(R, tx, ty - 1) is False:
                if rng.random() < 0.17:
                    name = 'pillar_band' if th == 1 else 'pillar_band2'
                    m.obj(f'{name}_{rng.choice(colors)}', tx + th / 2, ty + th / 2, R.floor, 0.0, tag='deco')

# --- Funde, Notizen, Wesen, Sicherungspunkte ------------------------------------------------------------------------
for (r, px, py) in crate_spots[::2]:
    a = rng.random() * 6.28
    qx, qy = px + math.cos(a) * 1.6, py + math.sin(a) * 1.6
    if m.at(qx, qy) == r and not m.blocked(qx, qy):
        m.spawn('item', qx, qy, 0.0, a, tags=['floor', 'crate'], group=r)
for r in rng.sample(cells, min(18, len(cells))):
    R = m.R(r)
    p = wall_spot(r, 0.4)
    if p and not m.blocked(p[0], p[1]):
        m.spawn('item', p[0], p[1], R.floor, p[2] + math.pi, tags=['floor', 'wall', 'room'], group=r)
for r in rng.sample(cells + band, 12):
    p = wall_spot(r, 0.5)
    if p and not m.blocked(p[0], p[1]):
        m.spawn('note', p[0], p[1], m.R(r).floor, p[2], tags=['floor'])
# Hounds und Clumps streifen durch die Hallen (Wiki); Smiler kommen mit dem Flackern (Regie)
for r in aquila + gothic + gd:
    R = m.R(r)
    for _ in range(2):
        for _try in range(10):
            px, py = R.x0 + 4 + rng.random() * (R.w - 8), R.y0 + 4 + rng.random() * (R.h - 8)
            if not m.blocked(px, py) and math.hypot(px - m.start[0], py - m.start[1]) > 30:
                m.spawn('entity', px, py, R.floor, role=rng.choice(['hound', 'hound', 'clump']), tags=['hall'])
                break


def safe_point(cid, r, label, at=None):
    R = m.R(r)
    x, y = at if at else (R.cx, R.cy)
    m.checkpoint(cid, x, y, 0.0, label)
    m.obj('safety_light', x, y, R.floor + R.height, 0.0, tag='emergency')


safe_point('vorraum', band[3], 'Büro an der Halle')
safe_point('kranken', band[0], 'Krankenzimmer', at=(m.R(band[0]).cx, m.R(band[0]).y0 + 1.5))
lit_cells = [c for c in cells if m.R(c).type in ('buero', 'zimmer', 'ziegelraum') and m.R(c).lights]
for k, r in enumerate(rng.sample(lit_cells, 4)):
    safe_point(f'gang{k}', r, 'Raum in den Gängen')
near = min(cells, key=lambda c: math.hypot(m.R(c).cx - G.cx, m.R(c).cy - G.cy))
safe_point('rohr', near, 'Vor dem Rohrgang')
# vor dem Uebergang: Sicherungspunkt am Anfang des letzten Rohrgang-Abschnitts (eigener Stromkreis)
last.kw['circuit'] = C_ROOMS
m.checkpoint('l2_tor', last.x0 + 2.5, last.cy, 0.0, 'Rohre und Messuhren')
m.obj('safety_light', last.x0 + 2.5, last.cy, last.floor + last.height, 0.0, tag='emergency')
m.save()
