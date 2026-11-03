"""Level 3 - "Das Umspannwerk" (V12, LEVELSPEC_3.md / LOOK_3.md; Wikidot Level 3, Revision 18).

Draufsicht (x nach rechts, y nach unten):

        +--------- Fertigungsstrasse (hell, Foerderbaender, Fenster) ---------+      +-- Sanktuarium --+  [Gitter]
  [Gitter]|                                                                    |======|  Saeulen, Engel  |
        +------------------------------+-----+----------------------------+      +-----------------+
   Generatorraum 1  |                       | breiter Gang
        |  Gang                              |
   [Gitter] Abgang ==== Hauptgang (Ziegel wechselnd) =============================+
        Buero |     Paranoia-Gang (dunkel, eng) ===== Kesselraum      Kesselraum   | Gang
                                                                      Generatorraum 2 -- Aufzug (Level 5)
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level3', 172, 132, seed=1401)
rng = m.rng
HALL_H = 3.0
DIM, BULB, COLD, WARM = (0.9, 0.85, 0.72), (1.0, 0.82, 0.58), (0.95, 1.0, 1.0), (1.0, 0.9, 0.76)
C_GANG, C_HALLE, C_ROOMS, C_SANKT = 1, 2, 5, 6
m.blackout_circuits = {C_GANG, C_HALLE}

# --- Raeume ----------------------------------------------------------------------------------------------------
abgang = m.room(2, 70, 9, 76, 'abgang', 'ziegel_braun', 0.0, 3.0, circuit=C_ROOMS, label='Abgang')
h1 = m.room(10, 71, 100, 74, 'gang', 'ziegel_rot', 0.0, HALL_H, circuit=C_GANG, label='Hauptgang')
m.opening(abgang, h1, width=3)
g1 = m.room(14, 24, 44, 45, 'generatorraum', 'generator', 0.0, 6.0, circuit=C_ROOMS, label='Generatorraum')
h2 = m.room(30, 46, 32, 70, 'gang', 'ziegel_grau', 0.0, 2.6, circuit=C_GANG)
m.opening(g1, h2)
m.opening(h2, h1)
A = m.room(50, 4, 110, 36, 'fertigung', 'fertigung', 0.0, 8.0, circuit=C_HALLE, label='Fertigungsstraße')  # V13.3: 60 x 32
h3 = m.room(70, 37, 75, 70, 'gang_breit', 'ziegel_hell', 0.0, 3.4, circuit=C_GANG)
m.opening(A, h3)
m.opening(h3, h1)
S = m.room(122, 10, 150, 50, 'sanktuarium', 'sanktum', 0.0, 7.9, circuit=C_SANKT, label='Sanktuarium')  # V13.3: 28 x 40
h4 = m.room(111, 24, 121, 26, 'gang', 'ziegel_braun', 0.0, 2.6, circuit=C_GANG)
m.opening(A, h4)
m.door(h4, S, 'wide', 2, top=2.6)
h5 = m.room(101, 71, 104, 99, 'gang', 'ziegel_grau', 0.0, HALL_H, circuit=C_GANG)
m.opening(h1, h5)
g2 = m.room(96, 100, 130, 124, 'generatorraum', 'generator', 0.0, 6.0, circuit=C_ROOMS, label='Generatorraum 2')
m.opening(h5, g2)
# Paranoia-Gang: eng, niedrig, dunkel; endet am Kesselraum
p1 = m.room(40, 75, 42, 98, 'paranoia', 'ziegel_dunkel', 0.0, 2.6, circuit=C_GANG, label='Paranoia')  # V13.3: 2 m breit, 2,6 m
p2 = m.room(40, 99, 78, 101, 'paranoia', 'ziegel_dunkel', 0.0, 2.6, circuit=C_GANG)
m.opening(h1, p1)
m.opening(p1, p2)
b1 = m.room(79, 96, 88, 104, 'kesselraum', 'kessel', 0.0, 3.2, circuit=C_ROOMS, label='Kesselraum')
m.opening(p2, b1)
b2 = m.room(60, 76, 68, 84, 'kesselraum', 'kessel', 0.0, 3.2, circuit=C_ROOMS)
m.door(h1, b2, 'door', 1)
b3 = m.room(106, 80, 114, 88, 'kesselraum', 'kessel', 0.0, 3.2, circuit=C_ROOMS)
m.door(h5, b3, 'door', 1)
buero = m.room(12, 76, 22, 84, 'kammer', 'ziegel_braun', 0.0, 2.8, circuit=C_ROOMS, label='Kammer')  # V13.3: kein Buero (Wiki: nur Basen)
m.door(h1, buero, 'door', 1)
# Aufzug nach Level 5
vor = m.room(132, 113, 144, 120, 'aufzugsvorraum', 'ziegel_hell', 0.0, 3.2, circuit=C_ROOMS, label='Aufzug')
m.opening(g2, vor, width=3)
kab = m.room(145, 114, 149, 119, 'kabine', 'aufzug', 0.0, 2.6, circuit=C_ROOMS)
dk = m.doors[m.door(vor, kab, 'opening', 2)]  # V13.3: 2 m wie in Level 5 (das Aufzugstor ist fuer 1,4 m gebaut, skaliert 1,35)
kab_door = dk

# Raeume hinter Gittern (unerreichbar): Licht, Tische, Bilder, Fenster
barred = []


def barred_room(x0, y0, x1, y1, front, width, zone='ziegel_weiss', h=3.0, kind='buero'):
    r = m.room(x0, y0, x1, y1, 'hinter_gitter', zone, 0.0, h, circuit=C_ROOMS, sealed=True)
    d = m.doors[m.door(front, r, 'opening', width)]
    bx0, by0, bx1, by1 = d['rect']
    yaw = 0.0 if d['axis'] == 0 else math.pi / 2
    R_ = m.R(r)
    # an der Grenze zum versiegelten Raum (dort beginnt die Sperre): man steht direkt davor, nicht darin
    if d['axis'] == 0:
        gx, gy = (bx1 if R_.x0 >= bx1 else bx0), (by0 + by1) / 2
    else:
        gx, gy = (bx0 + bx1) / 2, (by1 if R_.y0 >= by1 else by0)
    m.obj(f'bars_w{width}', gx, gy, 0.0, yaw)
    # Kollision: die Gitter sperren (Block in der Oeffnung, unsichtbar, volle Hoehe)
    m.R(front).kw.setdefault('_bars', []).append(d)
    barred.append((r, d, kind))
    return r


barred_room(12, 60, 26, 69, h1, 3)
barred_room(84, 76, 98, 90, h1, 3, kind='zeichnungen')
barred_room(36, 8, 48, 20, A, 2, zone='ziegel_rot', h=4.0, kind='fenster')
barred_room(154, 20, 166, 34, S, 2, zone='ziegel_weiss', kind='zeichnungen')
barred_room(132, 100, 146, 110, g2, 2, zone='ziegel_grau', kind='leer')

# Ziegel-Labyrinth im Suedwesten: Gaenge mit wechselnder Ziegelfarbe, kleine Kammern, einige hinter Gittern
from mapkit import corridor_maze  # noqa: E402
BRICKS = ['ziegel_braun', 'ziegel_rot', 'ziegel_grau', 'ziegel_hell']
MJ, MS, MC = corridor_maze(m, 2, 88, 38, 129, 9, 2, 'gang', 'ziegel_braun', ['kammer', 'kammer', 'kesselraum'],
                           lambda t: 'kessel' if t == 'kesselraum' else 'ziegel_braun', height=2.8, density=0.7,
                           room_chance=0.55, circuit=C_GANG, light_every=99)
maze = [r for r in list(MJ.values()) + list(MS.values()) if r is not None]
for r in maze:
    m.R(r).zone = rng.choice(BRICKS)
    m.R(r).lights = []
for r in MC.values():
    m.R(r).kw['circuit'] = C_ROOMS
# V13.3: weitere Gangnetze im Osten, unter dem Sanktuarium und im Sueden
NETS = []
for (nx0, ny0, nx1, ny1) in ((156, 52, 170, 112), (116, 52, 154, 98), (42, 105, 94, 129)):
    J_, S_, C_ = corridor_maze(m, nx0, ny0, nx1, ny1, 9, 2, 'gang', 'ziegel_grau', ['kammer', 'kammer', 'kesselraum'],
                               lambda t: 'kessel' if t == 'kesselraum' else 'ziegel_braun', height=2.8, density=0.75,
                               room_chance=0.5, circuit=C_GANG, light_every=99)
    net = [r for r in list(J_.values()) + list(S_.values()) if r is not None]
    for r in net:
        m.R(r).zone = rng.choice(BRICKS)
        m.R(r).lights = []
    for r in C_.values():
        m.R(r).kw['circuit'] = C_ROOMS
    NETS.append((net, [r for r in J_.values() if r is not None]))
    maze += net


def link_any(cands, target, n=2):
    k = 0
    for r in cands:
        if k >= n:
            break
        try:
            m.door(r, target, 'door', 1)
            k += 1
        except ValueError:
            pass


(n1, j1), (n2, j2), (n3, j3) = NETS
link_any([r for r in j2 if m.R(r).y0 == 52 and m.R(r).x0 >= 122 and m.R(r).x1 <= 150], S, 1)  # Sanktuarium -> Netz darunter
link_any([r for r in j2 if m.R(r).x0 == 116], b3, 1)                                   # Netz -> Kesselraum am Gang
link_any([r for r in j1 if m.R(r).x0 == 156], [r for r in j2 if m.R(r).x1 == max(m.R(q).x1 for q in j2)][0], 1)
for a_ in [r for r in j1 if m.R(r).x0 == 156][1::2]:                                   # Ost <-> Mitte an mehreren Stellen
    for b_ in [r for r in j2 if m.R(r).x1 == max(m.R(q).x1 for q in j2)]:
        if abs(m.R(a_).cy - m.R(b_).cy) < 1.0:
            try:
                m.door(a_, b_, 'door', 1)
            except ValueError:
                pass
link_any([r for r in j3 if m.R(r).y0 == 105], p2, 2)                                  # Sued -> Paranoia-Gang
link_any([r for r in j3 if m.R(r).y0 == 105], b1, 1)                                   # Sued -> Kesselraum
conn = m.room(29, 75, 31, 87, 'gang', 'ziegel_grau', 0.0, 2.8, circuit=C_GANG)
m.opening(h1, conn)
top_j = [r for r in MJ.values() if r is not None and m.R(r).y0 == 88 and m.R(r).x0 == 29]
m.opening(conn, top_j[0] if top_j else min(maze, key=lambda r: abs(m.R(r).cx - 30) + abs(m.R(r).y0 - 88)))

m.start = (3.6, 73.0, 0.0)
m.transition('l2_l3', 'seam', '', 3.6, 73.0, 0.0)  # Ankunft aus Level 2 (Tuer hinab)
m.transition('l3_l5', 'elevator', 'level5', 147.0, 116.5, math.pi, rect=[145, 114, 149, 119])
x0, y0, x1, y1 = kab_door['rect']
m.obj('elevator_door', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, 0.0, scale=[1, 1.35, 1])

gaenge = [h1, h2, h3, h4, h5, conn] + maze
paranoia = [p1, p2]

# --- Licht -------------------------------------------------------------------------------------------------------


def tubes(r, step=4, inten=0.55, radius=4.5, color=DIM, dead=0.25, bulb_chance=0.15):
    R = m.R(r)
    top = R.floor + R.height
    along_x = R.w >= R.h
    rng_pts = range(R.x0 + 1, R.x1 - 1, step) if along_x else range(R.y0 + 1, R.y1 - 1, step)
    for t in rng_pts:
        x, y = (t + 0.5, R.cy) if along_x else (R.cx, t + 0.5)
        if m.blocked(x, y):
            continue
        u = rng.random()
        if u < dead:
            m.obj('tube_dead', x, y, top, 0.0 if along_x else math.pi / 2, tag='deco')
        elif u < dead + bulb_chance:
            m.obj('hanging_bulb', x, y, top, 0.0, tag='light')
            m.light(r, x, y, 0, inten * 0.9, radius * 0.8, z=top - 0.71, panel=False, color=BULB)
        else:
            m.obj('tube_white', x, y, top, 0.0 if along_x else math.pi / 2, tag='light')
            m.light(r, x, y, 0, inten, radius, z=top - 0.12, panel=False, color=color)


for r in gaenge:
    tubes(r, step=4, dead=0.2)  # V13.3 Kritikrunde 1: enge Gaenge hatten 5 m und 25 % tot - lange Strecken unlesbar
for r in (abgang, buero, vor):
    tubes(r, step=3, inten=0.6, dead=0.0, bulb_chance=0.0)
R = m.R(kab)
m.light(kab, R.cx, R.cy, 0, 0.6, 3.0, z=R.floor + R.height - 0.05, panel=True, color=(1.0, 1.0, 0.98))
for r in (g1, g2):
    R = m.R(r)
    for x in range(R.x0 + 3, R.x1 - 2, 5):
        for y in range(R.y0 + 3, R.y1 - 2, 6):
            if rng.random() < 0.2:
                m.obj('tube_dead', x + 0.5, y + 0.5, R.floor + R.height, 0.0, tag='deco')
                continue
            m.obj('batten', x + 0.5, y + 0.5, R.floor + R.height, 0.0, tag='light')
            m.light(r, x + 0.5, y + 0.5, 0, 0.75, 7.0, z=R.floor + R.height - 0.45, panel=False, color=(1.0, 0.92, 0.72))
R = m.R(A)
for x in range(R.x0 + 4, R.x1 - 3, 6):
    for y in range(R.y0 + 4, R.y1 - 3, 6):
        m.obj('highbay', x + 0.5, y + 0.5, R.floor + R.height, 0.0, tag='light')
        m.light(A, x + 0.5, y + 0.5, 0, 1.2, 11.0, z=R.floor + R.height - 0.8, panel=False, color=COLD)
kessel = [b1, b2, b3] + [c for c in MC.values() if m.R(c).type == 'kesselraum']
for r in kessel + [c for c in MC.values() if m.R(c).type == 'kammer']:
    R = m.R(r)
    m.obj('hanging_bulb', R.cx, R.cy, R.floor + R.height, 0.0, tag='light')
    m.light(r, R.cx, R.cy, 0, 1.0, 5.5, z=R.floor + R.height - 0.71, panel=False, color=BULB)
# Paranoia-Gang: eine einzige, sterbende Roehre am Eingang
R = m.R(p1)
m.obj('tube_white', R.cx, R.y0 + 2.5, R.floor + R.height, math.pi / 2, tag='light')
m.light(p1, R.cx, R.y0 + 2.5, 2, 0.3, 3.0, z=R.floor + R.height - 0.12, panel=False, color=DIM)
R.kw['flicker'] = 2
# Sanktuarium: warm von oben, die Statue angestrahlt
R = m.R(S)
for y in range(R.y0 + 6, R.y1 - 4, 8):
    for x in (R.x0 + 8.5, R.x1 - 8.5):
        m.obj('ceiling_globe', x, y + 0.5, R.floor + R.height, 0.0, tag='light')
        m.light(S, x, y + 0.5, 0, 0.8, 9.0, z=R.floor + R.height - 0.6, panel=False, color=WARM)
# Raeume hinter Gittern: hell (Wiki: manchmal mit hellen Fenstern)
for (r, d, kind) in barred:
    R = m.R(r)
    if kind == 'leer':
        continue
    m.obj('tube_white', R.cx, R.cy, R.floor + R.height, 0.0, tag='light')
    m.light(r, R.cx, R.cy, 0, 0.95, 6.0, z=R.floor + R.height - 0.12, panel=False, color=COLD)

# --- Kollision der Gitter ------------------------------------------------------------------------------------------
for (r, d, kind) in barred:
    x0, y0, x1, y1 = d['rect']
    # die Oeffnung gehoert zu keinem Raum: Block im vorderen Raum direkt davor waere sichtbar - stattdessen eine Reihe
    # unsichtbarer Bloecke auf der ersten Kachel im versiegelten Raum (man sieht hindurch, kommt aber nicht hinein)
    R = m.R(r)
    if d['axis'] == 0:
        bx = x1 if R.x0 >= x1 else x0 - 1
        m.block(r, bx, y0, bx + 1, y1, R.height, hidden=True)
    else:
        by = y1 if R.y0 >= y1 else y0 - 1
        m.block(r, x0, by, x1, by + 1, R.height, hidden=True)

# --- Ausstattung ----------------------------------------------------------------------------------------------------


def wall_spot(r, inward=0.35, margin=1):
    faces = [f for f in m.wall_faces(r, margin=margin)
             if not m.blocked(f[0] + math.cos(f[2]) * inward, f[1] + math.sin(f[2]) * inward)]
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


def wall_runs(r, margin=1):
    by = {}
    for (fx, fy, fyaw) in m.wall_faces(r, margin=margin):
        by.setdefault(round(fyaw, 3), []).append((fx, fy))
    out = []
    for yaw, pts in by.items():
        along_y = abs(math.cos(yaw)) > 0.5
        pts.sort(key=lambda p: p[1] if along_y else p[0])
        cur = [pts[0]]
        for p in pts[1:]:
            q = cur[-1]
            if abs(((p[1] - q[1]) if along_y else (p[0] - q[0])) - 1.0) < 0.01:
                cur.append(p)
            else:
                out.append((yaw, cur))
                cur = [p]
        out.append((yaw, cur))
    return out


# Gaenge: Rohre mit schwarzer Fluessigkeit, Elektrokaesten, Kabelpritschen, Gemaelde, Tuecher, Schlammlachen
for r in gaenge:
    R = m.R(r)
    top = R.floor + R.height
    pipe_yaw = None  # V13.3: in Gaengen unter 3 m nur an einer Wand (sonst bleibt kein Durchgang)
    for yaw, run in wall_runs(r):
        for j in range(0, len(run) - 4, 7):
            if rng.random() < 0.55:
                if min(R.w, R.h) < 3 and pipe_yaw is not None and abs(pipe_yaw - yaw) > 0.1:
                    continue
                pipe_yaw = yaw
                fx, fy = (run[j][0] + run[j + 4][0]) / 2, (run[j][1] + run[j + 4][1]) / 2
                m.obj('pipe_black', fx, fy, R.floor, yaw, tag='pipes')
                m.spawn('deco', fx + math.cos(yaw) * 0.45, fy + math.sin(yaw) * 0.45, R.floor, 0.0, set='puddle', weight=0.3)
        for k in range(2, len(run) - 2, 6):
            u = rng.random()
            fx, fy = run[k]
            if u < 0.35:
                m.obj('junction_box', fx, fy, R.floor + 1.5, yaw, tag='deco')
            elif u < 0.45:
                m.obj('painting_angel' if rng.random() < 0.6 else 'painting_angel_scribbled', fx, fy, R.floor + 1.55, yaw, tag='deco')
    along_x = R.w >= R.h
    if along_x:
        for x in range(R.x0, R.x1):
            m.obj('cable_tray', x + 0.5, R.y0 + 0.4, top - 0.01, 0.0, variant=rng.randrange(4), tag='deco')
    else:
        for y in range(R.y0, R.y1):
            m.obj('cable_tray', R.x0 + 0.4, y + 0.5, top - 0.01, math.pi / 2, variant=rng.randrange(4), tag='deco')
# Paranoia: Warnschilder an beiden Enden
for r, at in ((p1, 'y0'), (p2, 'x1')):
    R = m.R(r)
    faces = m.wall_faces(r, margin=0)
    if faces:
        f = min(faces, key=lambda f: (f[1] - R.y0) if at == 'y0' else (R.x1 - f[0]))
        m.obj('sign_warning', f[0], f[1], R.floor + 1.6, f[2], tag='deco')
for f in m.wall_faces(h1, margin=0):
    if abs(f[0] - 40.5) < 1.6 and f[2] < 0:  # neben dem Eingang in den Paranoia-Gang, an der Suedwand des Hauptgangs
        m.obj('sign_warning', f[0], f[1], 1.6, f[2], tag='deco')  # V13.3: Eingang ist 2 m breit - Schild daneben
        break
# Abgang: Warnschilder (Wiki: sie weisen nach Level 4)
for (fx, fy, fyaw) in m.wall_faces(abgang, margin=1)[::3][:3]:
    m.obj('sign_warning', fx, fy, 1.6, fyaw, tag='deco')

# Generatorraeume: Generatoren, Schaltschraenke, Trafo, Kabel
for r in (g1, g2):
    R = m.R(r)
    for y in range(R.y0 + 4, R.y1 - 3, 6):
        for x in range(R.x0 + 4, R.x1 - 4, 7):
            if rng.random() < 0.25:
                continue
            m.obj('generator', x + 0.5, y + 0.5, R.floor, rng.choice([0.0, math.pi]), tag='machine')
            m.block(r, x - 1, y, x + 2, y + 1, 2.0, hidden=True)
    for yaw, run in wall_runs(r):
        for k in range(1, len(run) - 1, 3):
            fx, fy = run[k]
            m.obj(rng.choice(['switch_cabinet', 'mach_cabinet']), fx, fy, R.floor, yaw, tag='machine')
            bx, by = math.floor(fx + math.cos(yaw) * 0.5), math.floor(fy + math.sin(yaw) * 0.5)
            m.block(r, bx, by, bx + 1, by + 1, 2.0, hidden=True)

# Fertigungsstrasse: Foerderbaender in Reihen, Fenster mit Ziegel, Gitterrosttreppe, Kisten
R = m.R(A)
for y in range(R.y0 + 6, R.y1 - 5, 7):
    for x0 in range(R.x0 + 6, R.x1 - 8, 9):
        m.obj('conveyor', x0 + 3.0, y + 0.5, R.floor, 0.0, tag='machine')
        m.block(A, x0, y, x0 + 6, y + 1, 0.9, hidden=True)
for yaw, run in wall_runs(A, margin=2):
    for k in range(2, len(run) - 2, 5):
        fx, fy = run[k]
        m.obj('window_brick', fx, fy, R.floor + 4.2, yaw, tag='window')
fx, fy = R.x0 + 2, R.y1 - 1.7
m.obj('grate_stair', fx, fy, R.floor, 0.0)
m.block(A, R.x0 + 1, R.y1 - 3, R.x0 + 12, R.y1, 4.0, hidden=True)
for _ in range(8):
    x, y = R.x0 + 4 + rng.random() * (R.w - 8), R.y0 + 3 + rng.random() * (R.h - 6)
    if not any(m.blocked(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
        m.obj(rng.choice(['crate_b', 'crate_c']), x, y, R.floor, rng.random() * 6.28)
        m.block(A, math.floor(x - 0.5), math.floor(y - 0.5), math.floor(x - 0.5) + 1, math.floor(y - 0.5) + 1, 0.7, hidden=True)

# Kesselraeume: Kessel, Rohre mit Schwarzem, Schlammlachen, Schaedel
for r in kessel:
    R = m.R(r)
    p = wall_spot(r, 0.9)
    if p:
        m.obj('boiler', p[0], p[1], R.floor, p[2] + math.pi, tag='machine')
        m.block(r, math.floor(p[0] - 0.5), math.floor(p[1] - 0.5), math.floor(p[0] - 0.5) + 2, math.floor(p[1] - 0.5) + 2, 2.0, hidden=True)
    for yaw, run in wall_runs(r):
        if len(run) >= 5:
            fx, fy = (run[0][0] + run[4][0]) / 2, (run[0][1] + run[4][1]) / 2
            m.obj('pipe_black', fx, fy, R.floor, yaw, tag='pipes')
            break
    x, y = R.cx + rng.uniform(-1, 1), R.cy + rng.uniform(-1, 1)
    if not m.blocked(x, y):
        m.obj('sludge_pool', x, y, R.floor, rng.random() * 6.28)
    if rng.random() < 0.6:
        x, y = R.cx + rng.uniform(-1.5, 1.5), R.cy + rng.uniform(-1.5, 1.5)
        if not m.blocked(x, y):
            m.obj('skull', x, y, R.floor, rng.random() * 6.28)

# Sanktuarium: zwei Saeulenreihen, Engel auf Sockel am Nordende, Bilder, Tuecher, Frauenstatuen
R = m.R(S)
for y in range(R.y0 + 6, R.y1 - 3, 6):
    for x in (R.x0 + 7, R.x1 - 8):
        m.obj('column_greek', x + 0.5, y + 0.5, R.floor, 0.0)
        m.block(S, x, y, x + 1, y + 1, R.height, hidden=True)
ax, ay = R.cx, R.y0 + 4.5
m.obj('angel_statue', ax, ay, R.floor, math.pi / 2)
m.block(S, int(ax) - 1, int(ay) - 1, int(ax) + 1, int(ay) + 1, 1.1, hidden=True)
m.light(S, ax, ay + 3.5, 0, 0.9, 6.0, z=R.floor + 4.5, panel=False, color=(1.0, 0.92, 0.80))
for yaw, run in wall_runs(S, margin=2):
    for k in range(3, len(run) - 3, 6):
        fx, fy = run[k]
        if m.blocked(fx + math.cos(yaw) * 0.5, fy + math.sin(yaw) * 0.5):
            continue
        m.obj('painting_angel' if rng.random() < 0.7 else 'painting_angel_scribbled', fx, fy, R.floor + 2.2, yaw, tag='deco')
for (x, y) in ((R.x0 + 3.0, R.y1 - 4.0), (R.x1 - 3.0, R.y1 - 4.0)):
    m.obj('greek_torso', x, y, R.floor, -math.pi / 2)
    m.block(S, int(x), int(y), int(x) + 1, int(y) + 1, 1.7, hidden=True)
for (fx, fy, fyaw) in rng.sample(m.wall_faces(S, margin=2), 2):
    m.obj('canvas_wall', fx, fy, R.floor, fyaw, tag='deco')

# Raeume hinter Gittern: Tische und Schreibtische, Zeichnungen des Engels, Fenster
for (r, d, kind) in barred:
    R = m.R(r)
    if kind == 'leer':
        continue
    if kind in ('buero', 'zeichnungen'):
        m.obj('work_table', R.cx, R.cy, R.floor, 0.0)
        p = wall_spot(r, 0.45, margin=1)
        if p:
            m.obj('desk_crt', p[0], p[1], R.floor, p[2] + math.pi / 2, variant=rng.randrange(3))
    for (fx, fy, fyaw) in m.wall_faces(r, margin=1)[1::3][:5]:
        if kind == 'fenster':
            m.obj('window_brick', fx, fy, R.floor + 1.6, fyaw, tag='window')
        else:
            m.obj('painting_angel_scribbled' if kind == 'zeichnungen' else 'painting_angel', fx, fy, R.floor + 1.5, fyaw, tag='deco')
    if kind == 'zeichnungen':
        m.obj('canvas_drape_a', R.cx + 2.0, R.cy + 1.5, R.floor, 0.4)

# Buero: Schreibtisch, Tisch, Stuehle, Tuch ueber einem Moebel
R = m.R(buero)
m.obj('canvas_drape_b', R.cx + 1.0, R.cy + 1.0, R.floor, 0.0)  # V13.3: Kammer - nur ein abgedecktes Geraet
m.block(buero, int(R.cx), int(R.cy), int(R.cx) + 2, int(R.cy) + 2, 1.6, hidden=True)
# Teile von Zuegen im breiten Gang
R = m.R(h3)
m.obj('train_bogie', R.cx, R.cy - 4, R.floor, math.pi / 2 + 0.2)
m.block(h3, int(R.cx) - 1, int(R.cy) - 6, int(R.cx) + 1, int(R.cy) - 2, 0.9, hidden=True)

# --- Funde, Notizen, Wesen, Sicherungspunkte -------------------------------------------------------------------------
for r in (g1, g2, A):  # Wiki: Mandelwasser in Generatorraeumen und Fertigungsstrassen haeufig
    for _ in range(4):
        p = wall_spot(r, 0.6)
        if p:
            m.spawn('item', p[0], p[1], m.R(r).floor, p[2] + math.pi, tags=['floor', 'wall'], group=r)
for r in (buero, b1, b2, b3, vor):
    p = wall_spot(r, 0.4)
    if p:
        m.spawn('item', p[0], p[1], m.R(r).floor, p[2] + math.pi, tags=['floor', 'wall'], group=r)
for r in (buero, h1, h3, h5, g1, b1):
    p = wall_spot(r, 0.5)
    if p:
        m.spawn('note', p[0], p[1], m.R(r).floor, p[2], tags=['floor'])
for (r, role) in ((A, 'hound'), (A, 'clump'), (h1, 'hound'), (h3, 'clump'), (g2, 'hound'), (h5, 'clump')):
    R = m.R(r)
    for _try in range(20):
        x, y = R.x0 + 1 + rng.random() * (R.w - 2), R.y0 + 1 + rng.random() * (R.h - 2)
        if not m.blocked(x, y) and math.hypot(x - m.start[0], y - m.start[1]) > 35:
            m.spawn('entity', x, y, R.floor, role=role, tags=['level3'])
            break
for s_ in m.spawns:
    if s_['kind'] == 'entity' and s_.get('role') == 'clump':
        ri = m.at(s_['pos'][0], s_['pos'][1])
        m.obj('ceiling_vent', s_['pos'][0], s_['pos'][1], m.R(ri).floor + m.R(ri).height, 0.0, tag='deco')


def safe_point(cid, r, label, at=None):
    R = m.R(r)
    x, y = at if at else (R.cx, R.cy)
    if m.blocked(x, y):
        p = wall_spot(r, 1.0)
        x, y = p[0], p[1]
    m.checkpoint(cid, x, y, 0.0, label)
    m.obj('safety_light', x, y, R.floor + R.height, 0.0, tag='emergency')


safe_point('abgang', abgang, 'Abgang aus Level 2')
safe_point('sanktum', S, 'Sanktuarium (Zuflucht)', at=(m.R(S).cx, m.R(S).y1 - 8.5))
safe_point('kessel', b1, 'Kesselraum')
safe_point('aufzug', vor, 'Vor dem Aufzug')
safe_point('buero', buero, 'Kammer am Hauptgang')

# V13.3: Bauarten der haeufig gesetzten Ausstattung (PROPS_Level3.md). Erst nach allen Platzierungen gezogen, damit die
# Anordnung der Ebene unveraendert bleibt; nur die Bauart wechselt.
VARIANTS = {'switch_cabinet': 3, 'generator': 2, 'window_brick': 3, 'pipe_black': 2, 'painting_angel': 2,
            'painting_angel_scribbled': 2, 'conveyor': 2, 'column_greek': 2, 'mach_cabinet': 3, 'junction_box': 4}
for o in m.objects:
    if o['model'] in VARIANTS:
        v_ = rng.randrange(VARIANTS[o['model']])
        if v_:
            o['variant'] = v_

# V13.3 (KREATUR_L3_Purpur.md): "purpifizierte" tote Wesen, halb in der Ziegelwand (Wiki: selten, 17 Faelle; ein
# Skin-Stealer in einem dunkleren Bereich, ein Hound mit gemalter Botschaft daneben). Eigener Zufall, ganz am Ende - die
# Anordnung der Ebene bleibt unveraendert. Nur Gaenge mit der Ziegeltextur der Modelle (bricks_grey_black). Das Modell
# braucht eine durchgehende Wand ueber seine Breite (violette Ziegel, Schrift) und keine andere Wandausstattung davor.
# Lage im Modell: +x in den Gang, +y nach rechts, wenn man auf die Wand blickt (Welt: sin(yaw), -cos(yaw)).
import random as _random  # noqa: E402

prng = _random.Random(3303)
PURPLE = [  # (Modell, Raeume, Breite links/rechts des Ursprungs in m, Tag)
    ('purple_stealer', [p2, p1], (1.55, 1.75), 'stealer'),
    ('purple_hound', [h5, h2], (1.05, 3.05), 'hound'),
]
PURPLE_BODY = (1.0, 0.7)  # Koerper: halbe Breite entlang der Wand, Tiefe in den Gang (Hound 0,68 m, Skin-Stealer 0,51 m)
PURPLE_OPPOSITE = {'pipe_black': (2.95, 0.9)}  # gegenueber: halbe Laenge, Tiefe (Spiel: build/qa/props_level3.json); sonst 0,7/0,7


def purple_spot(r, ext):
    R = m.R(r)
    others = [o for o in m.objects if R.contains(o['pos'][0], o['pos'][1]) and o['pos'][2] < R.floor + R.height - 0.4]
    spawns = [s_ for s_ in m.spawns if R.contains(s_['pos'][0], s_['pos'][1])]
    out = []
    for yaw, run in wall_runs(r, margin=1):
        if len(run) < 2:
            continue
        rx, ry = math.sin(yaw), -math.cos(yaw)  # rechts, wenn man auf die Wand blickt
        a0 = run[0][0] * rx + run[0][1] * ry - 0.5  # Wandkacheln: je 1 m, Mitte +-0,5
        a1 = run[-1][0] * rx + run[-1][1] * ry + 0.5
        lo, hi = min(a0, a1), max(a0, a1)
        for (fx, fy) in run:
            s0 = fx * rx + fy * ry
            if s0 - ext[0] < lo or s0 + ext[1] > hi:
                continue
            nx, ny = math.cos(yaw), math.sin(yaw)
            bad = False
            for o in others:
                s = o['pos'][0] * rx + o['pos'][1] * ry
                dn = (o['pos'][0] - fx) * nx + (o['pos'][1] - fy) * ny
                if dn < 1.0 and s0 - ext[0] - 0.5 < s < s0 + ext[1] + 0.5:
                    bad = True
                    break
            # gegenueber: Ausstattung am Boden darf mit dem Koerper den Gang nicht unter 0,9 m verengen (room_metrics)
            for o in others:
                if bad:
                    break
                dn = (o['pos'][0] - fx) * nx + (o['pos'][1] - fy) * ny
                if dn < 1.0 or o['pos'][2] - R.floor > 0.5:
                    continue
                half, depth = PURPLE_OPPOSITE.get(o['model'], (0.7, 0.7))
                s = o['pos'][0] * rx + o['pos'][1] * ry
                if abs(s - s0) < half + PURPLE_BODY[0] and dn - depth - PURPLE_BODY[1] < 1.05:
                    bad = True
            for s_ in spawns:
                s = s_['pos'][0] * rx + s_['pos'][1] * ry
                dn = (s_['pos'][0] - fx) * nx + (s_['pos'][1] - fy) * ny
                if dn < 1.6 and s0 - ext[0] - 0.8 < s < s0 + ext[1] + 0.8:
                    bad = True
                    break
            if not bad:
                out.append((fx, fy, yaw))
    return out


for (model, rooms, ext, tag) in PURPLE:
    cands = [(r, c) for r in rooms for c in purple_spot(r, ext)]
    assert cands, model
    r, (fx, fy, fyaw) = cands[prng.randrange(len(cands))]
    R = m.R(r)
    # tag 'emergency': eigene Energie - leuchtet ohne Stromkreis weiter (world/generator.cpp: kein Ausfall, kein Flackern)
    m.obj(model, fx, fy, R.floor, fyaw, tag='emergency')
    m.spawn('trace', fx + math.cos(fyaw) * 0.3, fy + math.sin(fyaw) * 0.3, R.floor, fyaw, tags=[tag], set='purple')  # 0,3 m vor der Wand

# V13.3 Abnahme Level 3 (KRITIK_Level3.md, Runde 3): die Gangnetze waren auf langen Strecken kahl - nur Ziegel und die
# Kabelleiter unter der Decke (Laufweg 80-200 m). Das Wiki: Elektrokaesten, Kabel und alte Geraete ueberall, Rohre mit
# schwarzer Fluessigkeit an den Waenden. Eigener Zufall, ganz am Ende: alles andere (auch die purpifizierten Wesen) bleibt
# unveraendert. Nur Wandteile ab 0,9 m Hoehe (Verteiler, Schaltkaesten mit Panzerrohr zur Decke, Bilder) und Rohre nur an
# der Rohrwand enger Gaenge - kein Gang wird enger (room_metrics). Abstand zu Ausstattung, Wesen-Plaetzen, Spuren.
nrng = _random.Random(3304)
NET_ADDED = {}


def net_free(R, fx, fy, yaw, half):
    """Wandstelle frei: kein Wandteil derselben Wand naeher als half (laengs), keine Wesen-Plaetze, keine Spuren."""
    nx, ny = math.cos(yaw), math.sin(yaw)
    for o in m.objects:
        if o['model'] == 'cable_tray' or not R.contains(o['pos'][0], o['pos'][1]):
            continue
        dx, dy = o['pos'][0] - fx, o['pos'][1] - fy
        dn, ds = dx * nx + dy * ny, abs(-dx * ny + dy * nx)
        if dn < 0.9 and ds < (max(half, 3.4) if o['model'] == 'pipe_black' else half):
            return False
    for s_ in m.spawns:
        d = math.hypot(s_['pos'][0] - fx, s_['pos'][1] - fy)
        if d < (3.6 if s_['kind'] == 'trace' else 1.3):
            return False
    return True


PIPE_NET = 0.82  # Rohrstrang auf 82 %: 4,8 m statt 5,8 m - passt auf die 5-m-Wand eines Netzstuecks (Bogen in die Wand)
for r in maze:
    R = m.R(r)
    pipes = [o for o in m.objects if o['model'] == 'pipe_black' and R.contains(o['pos'][0], o['pos'][1])]
    pipe_yaw = pipes[0].get('yaw', 0.0) if pipes else None
    narrow = min(R.w, R.h) < 3
    if not pipes and nrng.random() < 0.6:
        runs = [(yaw, run) for yaw, run in wall_runs(r, margin=0) if len(run) >= 5]
        nrng.shuffle(runs)
        for yaw, run in runs:
            j = (len(run) - 5) // 2
            fx, fy = (run[j][0] + run[j + 4][0]) / 2, (run[j][1] + run[j + 4][1]) / 2
            if net_free(R, fx, fy, yaw, 2.6):
                m.obj('pipe_black', fx, fy, R.floor, yaw, scale=[PIPE_NET] * 3, variant=nrng.randrange(2), tag='pipes')
                pipes, pipe_yaw = [m.objects[-1]], yaw
                NET_ADDED['pipe_black'] = NET_ADDED.get('pipe_black', 0) + 1
                break
    for yaw, run in wall_runs(r):
        for k in range(1, len(run) - 1, 2):
            fx, fy = run[k]
            u = nrng.random()
            if u < 0.3:
                pick = ('junction_box', R.floor + 1.5, nrng.randrange(4))
            elif u < 0.48:
                pick = ('power_box', R.floor + 1.5, 0)
            elif u < 0.55:
                pick = ('painting_angel' if nrng.random() < 0.6 else 'painting_angel_scribbled', R.floor + 1.55, nrng.randrange(2))
            else:
                continue
            if not net_free(R, fx, fy, yaw, 1.35):
                continue
            m.obj(pick[0], fx, fy, pick[1], yaw, variant=pick[2], tag='deco')
            NET_ADDED[pick[0]] = NET_ADDED.get(pick[0], 0) + 1
print('Gangnetze, ergaenzt:', NET_ADDED)

m.save()
