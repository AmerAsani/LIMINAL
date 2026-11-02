"""Level 2 - "Die Versorgungsgaenge" (V12, LEVELSPEC_2.md / LOOK_2.md; Wikidot Level 2, Revision 93).

Draufsicht (x nach rechts, y nach unten):

    +== Gleistunnel (Ankunft aus Level 1, verrottende Schmalspur) ==========+ +-- Anlage ---------+ [3] Tuer nach Level 3
    | Tunnelnetz (10-m-Raster): Beton; Ziegel im Osten; heiss im Suedwesten | | Maschinen, Kisten |
    |  Maschinen an den Waenden, Rohrbuendel, verschlossene Tueren,         | +-- Packlager ------+
    |  Roehren in Gruppen (Reihenschaltung), violette Notlichter           | |  leer, hoch       |
    |                                                                        | +-- Buerogang ------+
    +-----------+ Leerraum (schwarz)                                       | | Schleife, Bueros  |
                                                                           +-+-------------------+
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level2', 198, 162, seed=1301)
rng = m.rng
TUN_H, GLEIS_H, ANL_H, LAGER_H, BUERO_H = 2.6, 3.2, 5.5, 6.0, 2.7
WHITE, ORANGE, VIOLET = (1.0, 0.98, 0.94), (1.0, 0.72, 0.42), (0.62, 0.32, 1.0)
C_GLEIS, C_TUN, C_HALLE, C_BUERO, C_ROOMS = 1, 2, 3, 4, 5
m.blackout_circuits = {C_GLEIS, C_TUN, C_HALLE, C_BUERO}

# --- Gleistunnel (Ankunft) ----------------------------------------------------------------------------------------
gleis = m.room(2, 2, 122, 5, 'gleistunnel', 'gleis', 0.0, GLEIS_H, circuit=C_GLEIS, label='Gleistunnel', tags=['tunnel'])
m.start = (4.0, 3.5, 0.0)
m.transition('l1_l2', 'seam', '', 4.0, 3.5, 0.0)  # Ankunft aus Level 1 ueber den langen Rohrgang

# --- Tunnelnetz -----------------------------------------------------------------------------------------------------
CELL_TYPES = ['kammer', 'kammer', 'maschinenraum', 'lagerraum']


def zone_at(x, y):
    if x >= 64 and y >= 64:
        return 'brownout'  # V13.3 (Wikidot Level 2): Bereiche im dauerhaften Brownout
    if x >= 64:
        return 'ziegel'
    if y >= 92 and x < 56:
        return 'heiss'
    return 'beton'


J, S, C = corridor_maze(m, 2, 7, 122, 127, 15, 2, 'tunnel', 'beton', CELL_TYPES, lambda t: 'kammer', height=TUN_H,
                        density=0.88, room_chance=0.32, circuit=C_TUN, light_every=99)
for d in (J, S):
    for k, r in d.items():
        if r is None:
            continue
        R = m.R(r)
        R.zone = zone_at(R.cx, R.cy)
        R.lights = []
        if R.zone == 'heiss':
            R.type = 'tunnel_heiss'
for r in C.values():
    R = m.R(r)
    R.kw['circuit'] = C_ROOMS
    R.zone = {'maschinenraum': 'maschine', 'lagerraum': 'kammer', 'kammer': 'kammer'}[R.type]
junctions = {k: v for k, v in J.items() if v is not None}
segments = list(S.values())
# Gleistunnel an das Netz: Durchgaenge zu einigen Kreuzungen der ersten Reihe
top = [junctions[(i, 0)] for i in range(12) if (i, 0) in junctions]
for r in top[::3]:
    m.door(gleis, r, 'opening', 2)

# --- Anlage, Packlager, Buerogang -------------------------------------------------------------------------------------
anlage = m.room(124, 2, 164, 30, 'anlage', 'anlage', 0.0, ANL_H, circuit=C_HALLE, label='Anlage', tags=['hall'])  # V13.3: 40 x 28 statt 64 x 48
m.opening(gleis, anlage, width=3, at=3.5)
lager = m.room(124, 32, 164, 58, 'packlager', 'lager', 0.0, LAGER_H, circuit=C_HALLE, label='Packlager', tags=['hall'])
m.door(anlage, lager, 'wide', 4, at=150, top=3.2)
# Bueroring: vier Gaenge um einen Block mit Bueros (fuehrt im Kreis zur Tuer zurueck)
ring_n = m.room(126, 61, 164, 63, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO, label='Bürogang')
ring_s = m.room(126, 92, 164, 94, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO)
ring_w = m.room(126, 64, 128, 91, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO)
ring_e = m.room(162, 64, 164, 91, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO)
for a, b in ((ring_n, ring_w), (ring_n, ring_e), (ring_s, ring_w), (ring_s, ring_e)):
    m.opening(a, b, width=2)
offices = []
for k, y in enumerate(range(65, 87, 6)):
    row = []
    for x0, x1 in ((129, 136), (137, 144), (145, 152), (153, 161)):
        row.append(m.room(x0, y, x1, y + 5, 'buero', 'buero', 0.0, BUERO_H, circuit=C_ROOMS))
    offices += row
    m.door(row[0], ring_w, 'door', 1)
    m.door(row[-1], ring_e, 'door', 1)
    for a, b in zip(row, row[1:]):
        m.door(a, b, 'door', 1)
m.door(lager, ring_n, 'door', 2, at=140)
# Tunnelnetz -> Bueroring und Anlage (Ostrand des Netzes)
east = [r for r in junctions.values() if m.R(r).x1 >= 120]
for r in sorted(east, key=lambda r: m.R(r).cy)[::4]:
    R = m.R(r)
    tgt = anlage if R.cy < 30 else (lager if R.cy < 58 else (ring_w if R.cy < 92 else None))
    if tgt is None:
        continue
    try:
        m.door(r, tgt, 'door', 1)
    except ValueError:
        pass
# Tuer nach Level 3: in der Ostwand der Anlage, dahinter ein kurzer Vorraum mit dem Uebergang
# V13.3: Tunnelnetz Ost (neben den Hallen) und Sued (unter dem Buerogang) - das Labyrinth ist die Grundstruktur
JE, SE, CE = corridor_maze(m, 166, 2, 188, 98, 10, 2, 'tunnel', 'ziegel', CELL_TYPES, lambda t: 'kammer', height=TUN_H,
                           density=0.8, room_chance=0.3, circuit=C_TUN, light_every=99)
JS, SS, CS = corridor_maze(m, 124, 98, 196, 158, 15, 2, 'tunnel', 'beton', CELL_TYPES, lambda t: 'kammer', height=TUN_H,
                           density=0.8, room_chance=0.32, circuit=C_TUN, light_every=99)
for dd in (JE, SE, JS, SS):
    for k, r in dd.items():
        if r is not None:
            m.R(r).lights = []
for cd in (CE, CS):
    for r in cd.values():
        R = m.R(r)
        R.kw['circuit'] = C_ROOMS
        R.zone = {'maschinenraum': 'maschine', 'lagerraum': 'kammer', 'kammer': 'kammer'}[R.type]
junctions.update({('e',) + k: v for k, v in JE.items() if v is not None})
junctions.update({('s',) + k: v for k, v in JS.items() if v is not None})
segments += [v for v in SE.values() if v is not None] + [v for v in SS.values() if v is not None]
C.update({('e',) + k: v for k, v in CE.items()})
C.update({('s',) + k: v for k, v in CS.items()})


def link(cands, target, step=3):
    for r in cands[::step]:
        try:
            m.door(r, target, 'door', 1)
        except ValueError:
            pass


east_w = sorted([v for v in JE.values() if v is not None and m.R(v).x0 == 166], key=lambda r: m.R(r).cy)
link([r for r in east_w if m.R(r).cy < 30], anlage, 2)
link([r for r in east_w if 32 <= m.R(r).cy < 58], lager, 2)
link([r for r in east_w if 64 <= m.R(r).cy < 91], ring_e, 2)
south_n = sorted([v for v in JS.values() if v is not None and m.R(v).y0 == 98], key=lambda r: m.R(r).cx)
link([r for r in south_n if 126 <= m.R(r).cx < 164], ring_s, 2)
e_bottom = [v for v in JE.values() if v is not None]
ymax = max(m.R(v).y1 for v in e_bottom)
for r in [r for r in south_n if m.R(r).cx >= 166]:
    for v in e_bottom:
        if m.R(v).y1 == ymax and m.R(v).x0 == m.R(r).x0:
            m.door(v, r, 'door', 1)
south_w = sorted([v for v in JS.values() if v is not None and m.R(v).x0 == 124], key=lambda r: m.R(r).cy)
west_net = {m.R(v).cy: v for v in junctions.values() if isinstance(v, int) and m.R(v).x1 == 122}
for r in south_w[::2]:
    best = min(west_net, key=lambda cy: abs(cy - m.R(r).cy)) if west_net else None
    if best is not None and abs(best - m.R(r).cy) < 1.5:
        try:
            m.door(west_net[best], r, 'door', 1)
        except ValueError:
            pass
vor3 = m.room(189, 22, 196, 25, 'vorraum', 'vorraum3', 0.0, TUN_H, circuit=C_ROOMS)
j3 = [v for v in JE.values() if v is not None and m.R(v).x1 == 188 and m.R(v).y0 <= 23 < m.R(v).y1][0]
d3 = m.doors[m.door(j3, vor3, 'door', 1, top=2.1)]
x0, y0, x1, y1 = d3['rect']
m.obj('door_level3', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, 0.0)
m.transition('l2_l3', 'door', 'level3', 194.5, 23.5, 0.0, rect=[193, 22, 196, 25])

# --- V13.3: Macchina-Werkstatt (Wikidot Level 2: verlassene Bereiche der Macchina mit Werkzeug und Teilen) -------------
mac_cells = sorted([v for v in CS.values()], key=lambda r: -m.R(r).w * m.R(r).h)[:2]
for r in mac_cells:
    R = m.R(r)
    R.type, R.zone = 'macchina_werkstatt', 'macchina'
    R.kw['label'] = 'Macchina-Werkstatt'

# --- Leerraum: hinter einer Tuer mit Fenster nur Schwarz ---------------------------------------------------------
ya = max(m.R(v).y1 for v in J.values() if v is not None)  # V13.3: unterste Kreuzungsreihe (Raster 15 m)
south = [r for r in junctions.values() if isinstance(r, int) and m.R(r).y1 == ya and 30 <= m.R(r).cx <= 60]
leer = m.room(28, ya + 2, 66, ya + 31, 'leerraum', 'leere', 0.0, 4.0, circuit=C_ROOMS, label='Leerraum')
dl = m.doors[m.door(south[0], leer, 'door', 1)]
x0, y0, x1, y1 = dl['rect']
m.obj('swing_door1', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, math.pi / 2)

# --- 45-Grad-Knicke: abgeschraegte Aussenecke an Kreuzungen mit genau zwei rechtwinkligen Durchgaengen ----------------
open_dirs = {r: set() for r in junctions.values()}
for d in m.doors:
    for a, b in ((d['a'], d['b']), (d['b'], d['a'])):
        if a in open_dirs:
            A, B = m.R(a), m.R(b)
            if B.x0 >= A.x1:
                open_dirs[a].add((1, 0))
            elif B.x1 <= A.x0:
                open_dirs[a].add((-1, 0))
            elif B.y0 >= A.y1:
                open_dirs[a].add((0, 1))
            else:
                open_dirs[a].add((0, -1))
chamfers = 0
for r, dirs in open_dirs.items():
    if len(dirs) != 2:
        continue
    (ax, ay), (bx, by) = sorted(dirs)
    if ax * bx + ay * by != 0:
        continue  # gerade durch
    R = m.R(r)
    sx, sy = -(ax + bx), -(ay + by)  # Aussenecke: weg von beiden Durchgaengen
    tx = R.x0 if sx < 0 else R.x1 - 1
    ty = R.y0 if sy < 0 else R.y1 - 1
    m.block(r, tx, ty, tx + 1, ty + 1, R.height, hidden=True)
    m.obj('chamfer_26', tx + 0.5, ty + 0.5, 0.0, math.atan2(sy, sx) - 3 * math.pi / 4)
    chamfers += 1

# --- Licht: Roehren in Reihenschaltung (Gruppen von 3-6, ganze Gruppen tot), farbig gemischt, violette Notlichter -------
DEAD_GROUPS = []


def tube_run(r, color, step=3, dead_group=0.2, odd_color=0.06, lit_near=None):
    R = m.R(r)
    top = R.floor + R.height
    along_x = R.w >= R.h
    pts = []
    if along_x:
        for x in range(R.x0 + 1, R.x1 - 1, step):
            pts.append((x + 0.5, R.cy, 0.0))
    else:
        for y in range(R.y0 + 1, R.y1 - 1, step):
            pts.append((R.cx, y + 0.5, math.pi / 2))
    i = 0
    while i < len(pts):
        n = rng.randint(3, 6)
        dead = rng.random() < dead_group
        if lit_near and any(math.hypot(x - lit_near[0], y - lit_near[1]) < 8 for (x, y, _) in pts[i:i + n]):
            dead = False
        if dead:
            DEAD_GROUPS.append((r, pts[i:i + n]))  # Abnahme Level 2: Notlichter am Ende (eigener Zufall, aendert sonst nichts)
        col = color if rng.random() > odd_color else (ORANGE if color == WHITE else WHITE)
        for (x, y, yaw) in pts[i:i + n]:
            if m.blocked(x, y):
                continue
            ox = rng.uniform(-0.15, 0.15)  # ungeordnet versetzt
            if dead:
                m.obj('tube_dead', x + (0 if along_x else ox), y + (ox if along_x else 0), top, yaw + rng.uniform(-0.1, 0.1), tag='deco')
            else:
                m.obj('tube_white' if col == WHITE else 'tube_orange', x + (0 if along_x else ox), y + (ox if along_x else 0), top,
                      yaw + rng.uniform(-0.1, 0.1), tag='light')
                m.light(r, x, y, 0, 0.8, 5.0, z=top - 0.12, panel=False, color=col)
        i += n


for r in segments:
    R = m.R(r)
    if max(R.w, R.h) >= 4:
        if R.zone == 'brownout':  # V13.3: Dauer-Brownout - wenige, schwache, flackernde Roehren
            R.kw['flicker'] = 1
            tube_run(r, ORANGE, step=4, dead_group=0.35)  # V13.3 Kritikrunden 1/2: war schwarz (0,55 tot, 0,35)
            for l_ in R.lights:
                l_[3] = 0.55
            continue
        tube_run(r, ORANGE if R.zone == 'heiss' else (WHITE if R.zone == 'beton' or rng.random() < 0.6 else ORANGE))
tube_run(gleis, WHITE, step=4, dead_group=0.35, lit_near=(m.start[0], m.start[1]))
for r in C.values():
    R = m.R(r)
    m.obj('tube_white', R.cx, R.cy, R.floor + R.height, 0.0, tag='light')
    m.light(r, R.cx, R.cy, 0, 0.6, 4.0, z=R.floor + R.height - 0.12, panel=False, color=WHITE)
for r in (ring_n, ring_s, ring_w, ring_e):
    tube_run(r, WHITE, step=4, dead_group=0.15)
for r in offices:
    R = m.R(r)
    if rng.random() < 0.5:
        m.obj('hanging_bulb', R.cx, R.cy, R.floor + R.height, 0.0, tag='light')
        m.light(r, R.cx, R.cy, 0, 0.8, 4.5, z=R.floor + R.height - 0.71, panel=False, color=(1.0, 0.84, 0.62))
    else:
        m.obj('tube_white', R.cx, R.cy, R.floor + R.height, 0.0, tag='light')
        m.light(r, R.cx, R.cy, 0, 0.7, 4.5, z=R.floor + R.height - 0.12, panel=False, color=WHITE)
for r in (anlage, lager):
    R = m.R(r)
    for x in range(R.x0 + 6, R.x1 - 4, 9):
        for y in range(R.y0 + 6, R.y1 - 4, 9):
            if m.blocked(x + 0.5, y + 0.5) or rng.random() < (0.35 if r == anlage else 0.55):
                continue
            m.obj('highbay', x + 0.5, y + 0.5, R.floor + R.height, 0.0, tag='light')
            m.light(r, x + 0.5, y + 0.5, 0, 1.0, 10.0, z=R.floor + R.height - 0.8, panel=False, color=(1.0, 0.86, 0.66))
vr = m.R(vor3)
m.obj('tube_white', vr.cx, vr.cy, vr.floor + vr.height, 0.0, tag='light')
m.light(vor3, vr.cx, vr.cy, 0, 0.7, 4.0, z=vr.floor + vr.height - 0.12, panel=False, color=WHITE)
# violette Notlichter (seit dem Ausfall, vereinzelt; eigene Batterie)
for r in list(junctions.values()) + [gleis]:
    if rng.random() < (1.0 if r == gleis else 0.5 if m.R(r).zone == 'brownout' else 0.12):  # V13.3: im Brownout oefter
        faces = m.wall_faces(r, margin=0)
        for (fx, fy, fyaw) in (rng.sample(faces, min(len(faces), 6 if r == gleis else 1)) if faces else []):
            m.obj('violet_lamp', fx, fy, m.R(r).floor + 2.1, fyaw, tag='emergency')

# --- Waende: Maschinen, Rohrbuendel, verschlossene Tueren -----------------------------------------------------------------
MACH = [('mach_cabinet', 1.0, 0.6), ('mach_pump', 1.6, 0.85), ('mach_tank', 1.0, 1.0), ('pipe_riser', 0.6, 0.4)]
DOORS = ['door_steel_white', 'door_steel_grey', 'door_steel_cream', 'door_window', 'door_plywood', 'door_wood_old']


def wall_runs(r):
    """Wandkacheln je Seite in zusammenhaengenden Laeufen: {yaw: [[(x, y), ...], ...]} (margin 1 zu Oeffnungen)."""
    runs = {}
    faces = m.wall_faces(r, margin=1)
    by = {}
    for (fx, fy, fyaw) in faces:
        by.setdefault(round(fyaw, 3), []).append((fx, fy))
    for yaw, pts in by.items():
        along_y = abs(math.cos(yaw)) > 0.5
        pts.sort(key=lambda p: p[1] if along_y else p[0])
        cur = [pts[0]]
        for p in pts[1:]:
            q = cur[-1]
            if abs(((p[1] - q[1]) if along_y else (p[0] - q[0])) - 1.0) < 0.01:
                cur.append(p)
            else:
                runs.setdefault(yaw, []).append(cur)
                cur = [p]
        runs.setdefault(yaw, []).append(cur)
    return runs


def furnish_tunnel(r):
    R = m.R(r)
    runs = wall_runs(r)
    sides = list(runs.keys())
    if not sides:
        return
    mach_side = rng.choice(sides)  # Maschinen nur an einer Seite je Abschnitt (der Tunnel bleibt begehbar)
    # V13.3: lichte Breite >= 1,0 m - Tiefe der Maschinen + Rohrbuendel (0,37 m) + Gegenseite (Schrank 0,6 m nur ab 3 m)
    width = min(R.w, R.h)
    other_ok = width >= 3
    max_depth = width - 1.0 - 0.37 - (0.6 if other_ok else 0.0)
    # V13.3: Maschinenseite zuerst, damit Tueren gegenueber ihren Platz pruefen koennen
    for yaw, rl in sorted(runs.items(), key=lambda kv: kv[0] != mach_side):
        c, s = math.cos(yaw), math.sin(yaw)
        for run in rl:
            k = 0
            while k < len(run):
                fx, fy = run[k]
                u = rng.random()
                fits = [x for x in MACH if x[2] <= max_depth]
                if yaw == mach_side and u < 0.62 and k + 1 < len(run) and fits:
                    md, w, depth = rng.choice(fits)
                    n = 2 if w > 1.1 else 1
                    px = (fx + run[min(k + n - 1, len(run) - 1)][0]) / 2
                    py = (fy + run[min(k + n - 1, len(run) - 1)][1]) / 2
                    m.obj(md, px, py, R.floor, yaw, tag='machine' if md != 'pipe_riser' else 'pipes')
                    for j in range(n):
                        tx, ty = run[min(k + j, len(run) - 1)]
                        bx, by = math.floor(tx + c * 0.5), math.floor(ty + s * 0.5)
                        m.block(r, bx, by, bx + 1, by + 1, 2.0 if md != 'pipe_riser' else R.height, hidden=True)
                    k += n + 1
                    continue
                if u < 0.22 and yaw != mach_side:
                    # V13.3 (Kritikrunde 1): keine Tuer gegenueber einer Maschine in engen Tunneln (Weg < 0,9 m)
                    if width < 3 and any(m.blocked(fx + c * (width - 0.5) - s * d, fy + s * (width - 0.5) + c * d)
                                         for d in (-0.7, 0.0, 0.7)):
                        k += 1
                        continue
                    m.obj(rng.choice(DOORS), fx, fy, R.floor, yaw, tag='deco')
                    k += 3
                    continue
                if u < 0.32 and yaw != mach_side and other_ok:
                    md, w, depth = ('mach_cabinet', 1.0, 0.6) if rng.random() < 0.5 else ('pipe_riser', 0.6, 0.4)
                    m.obj(md, fx, fy, R.floor, yaw, tag='machine')
                    bx, by = math.floor(fx + c * 0.5), math.floor(fy + s * 0.5)
                    m.block(r, bx, by, bx + 1, by + 1, 2.0 if md != 'pipe_riser' else R.height, hidden=True)
                    k += 3
                    continue
                if u < 0.42:
                    m.obj('junction_box', fx, fy, R.floor + 1.5, yaw, tag='deco')
                k += 1
        # Rohrbuendel ueber die ganze Laenge an der Maschinenseite (5-m-Stuecke)
        if yaw == mach_side:
            for run in rl:
                for j in range(0, len(run) - 4, 5):
                    fx, fy = (run[j][0] + run[j + 4][0]) / 2, (run[j][1] + run[j + 4][1]) / 2
                    m.obj(rng.choice(['pipe_bundle_a', 'pipe_bundle_b']), fx, fy, R.floor, yaw, tag='pipes')


for r in segments:
    if max(m.R(r).w, m.R(r).h) >= 4:
        furnish_tunnel(r)
furnish_tunnel(gleis)

# Gleis: Schmalspur in 5-m-Stuecken (einige fehlen), ein entgleister Trichterwagen
G = m.R(gleis)
for x in range(G.x0 + 3, G.x1 - 3, 5):
    if rng.random() < 0.88:
        m.obj('rail_track', x + 2.5, G.y0 + 1.5, 0.0, 0.0, tag='deco')
m.obj('hopper_wagon', 61.5, G.y0 + 2.32, 0.0, 0.03)  # V13.3: entgleist zur Suedwand hin - noerdlich bleibt der Gang frei
# V13.3: neben dem Wagen bleibt die Nordwand frei (Maschinen dort entfernen, Durchgang >= 1 m)
m.objects = [o for o in m.objects if not (o['model'].startswith(('mach_', 'pipe_riser', 'pipe_bundle')) and 57.5 <= o['pos'][0] <= 66.5
                                          and G.y0 <= o['pos'][1] < G.y0 + 1.2)]
for b_ in list(G.blocks):
    if 57 <= b_[0] <= 65 and b_[1] == G.y0 and b_[3] == G.y0 + 1:
        G.blocks.remove(b_)
m.block(gleis, 60, G.y0 + 2, 63, G.y0 + 3, 1.0, hidden=True)

# --- Anlage: grosse Maschinen in Reihen, Kisten, Wagen; Packlager fast leer -----------------------------------------------
A = m.R(anlage)
for y in range(A.y0 + 6, A.y1 - 5, 8):
    for x in range(A.x0 + 6, A.x1 - 6, 6):
        if rng.random() < 0.3:
            continue
        md = rng.choice(['mach_tank', 'mach_pump', 'mach_cabinet'])
        m.obj(md, x + 0.5, y + 0.5, 0.0, rng.choice([0.0, math.pi]), tag='machine')
        m.block(anlage, x, y, x + 1, y + 1, 2.0, hidden=True)
for _ in range(14):
    x, y = A.x0 + 3 + rng.random() * (A.w - 6), A.y0 + 3 + rng.random() * (A.h - 6)
    if not any(m.blocked(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
        m.obj(rng.choice(['crate_c', 'crate_b']), x, y, 0.0, rng.random() * 6.28)
        m.block(anlage, math.floor(x - 0.5), math.floor(y - 0.5), math.floor(x - 0.5) + 1, math.floor(y - 0.5) + 1, 0.7, hidden=True)
for y in range(A.y0 + 4, A.y1 - 2, 7):  # V13.3: 1-m-Stuecke mit Wanddurchfuehrung (wie Level 1)
    for (yy, zz, v_) in ((y + 0.5, A.floor + A.height - 0.5, 1), (y + 0.85, A.floor + A.height - 0.62, 0)):
        for x_ in range(A.x0, A.x1):
            m.obj('pipe_run', x_ + 0.5, yy, zz, 0.0, variant=v_, tag='pipes')
        if m.wall_ok(A.x0 + 0.05, yy, 0.0, zz):
            m.obj('pipe_wall', A.x0 + 0.004, yy, zz, 0.0, variant=v_, tag='pipes')
        if m.wall_ok(A.x1 - 0.05, yy, math.pi, zz):
            m.obj('pipe_wall', A.x1 - 0.004, yy, zz, math.pi, variant=v_, tag='pipes')
L_ = m.R(lager)
for _ in range(6):
    x, y = L_.x0 + 4 + rng.random() * (L_.w - 8), L_.y0 + 4 + rng.random() * (L_.h - 8)
    if not m.blocked(x, y):
        m.obj('pallet', x, y, 0.0, rng.random() * 6.28)

# --- Bueros: Schreibtisch mit Rechner, Stuhl -------------------------------------------------------------------------------
for r in offices:
    R = m.R(r)
    faces = m.wall_faces(r, margin=1)
    if faces:
        fx, fy, fyaw = rng.choice(faces)
        px, py = fx + math.cos(fyaw) * 0.45, fy + math.sin(fyaw) * 0.45
        m.obj('desk_crt', px, py, R.floor, fyaw + math.pi / 2, variant=rng.randrange(3))
        m.obj(rng.choice(['chair', 'folding_chair']), px + math.cos(fyaw) * 0.75, py + math.sin(fyaw) * 0.75, R.floor, fyaw + math.pi)

# --- Kammern und Maschinenraeume ------------------------------------------------------------------------------------------
for r in C.values():
    R = m.R(r)
    if R.type == 'maschinenraum':
        for (fx, fy, fyaw) in rng.sample(m.wall_faces(r, margin=1), min(2, len(m.wall_faces(r, margin=1)))):
            m.obj(rng.choice(['mach_cabinet', 'mach_tank']), fx, fy, R.floor, fyaw, tag='machine')
            bx, by = math.floor(fx + math.cos(fyaw) * 0.5), math.floor(fy + math.sin(fyaw) * 0.5)
            m.block(r, bx, by, bx + 1, by + 1, 2.0, hidden=True)
    elif R.type == 'lagerraum':
        x, y = R.cx, R.cy
        if not m.blocked(x, y):
            m.obj('crate_c', x, y, R.floor, rng.random() * 6.28)
            m.block(r, math.floor(x - 0.5), math.floor(y - 0.5), math.floor(x - 0.5) + 1, math.floor(y - 0.5) + 1, 0.7, hidden=True)

def wall_spot_early(r, inward=0.35):
    faces = [f for f in m.wall_faces(r, margin=1) if not m.blocked(f[0] + math.cos(f[2]) * inward, f[1] + math.sin(f[2]) * inward)]
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


# --- Gegenstaende am Boden: Faesser, Wagen, Werkzeug, Leitern, Kabeltrommeln, Klappstuehle --------------------------------
LOOSE = ['barrel', 'barrel', 'trolley_flatbed', 'toolbox', 'stepladder', 'cable_reel', 'folding_chair']
for r in segments + list(C.values()):
    R = m.R(r)
    if rng.random() > 0.35:
        continue
    # V13.3: an eine Wand gestellt, nur wo dahinter (quer zum Gang) frei bleibt - lichte Breite >= 1 m
    faces = m.wall_faces(r, margin=1)
    rng.shuffle(faces)
    for (fx, fy, fyaw) in faces[:8]:
        c_, s_ = math.cos(fyaw), math.sin(fyaw)
        x, y = fx + c_ * 0.4, fy + s_ * 0.4
        clear = all(not m.blocked(fx + c_ * d_, fy + s_ * d_) for d_ in (0.5, 1.5)) and min(R.w, R.h) >= 3  # 2-m-Tunnel bleiben frei
        if clear and m.in_door(math.floor(x), math.floor(y)) is None:
            md = rng.choice(LOOSE)
            m.obj(md, x, y, R.floor, rng.random() * 6.28)
            break

# --- V13.3: Macchina-Werkstatt: Werkbank, Teile, Werkzeug; blaues Pulsieren in den Rohren ------------------------------------
for r in mac_cells:
    R = m.R(r)
    m.light(r, R.cx, R.cy, 0, 0.5, 4.5, z=R.floor + R.height - 0.1, panel=False, color=(0.45, 0.6, 1.0))
    R.kw['flicker'] = 1
    for name in ('macchina_parts', 'macchina_tools', 'macchina_parts'):
        p_ = wall_spot_early(r, 0.6)
        if p_ and not m.blocked(p_[0], p_[1]):
            m.obj(name, p_[0], p_[1], R.floor, p_[2] + math.pi)

# --- Funde, Notizen, Wesen, Sicherungspunkte ------------------------------------------------------------------------------
def wall_spot(r, inward=0.35):
    faces = [f for f in m.wall_faces(r, margin=1) if not m.blocked(f[0] + math.cos(f[2]) * inward, f[1] + math.sin(f[2]) * inward)]
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


cells = list(C.values())
for r in rng.sample(cells, min(16, len(cells))) + rng.sample(offices, 6):
    p = wall_spot(r)
    if p:
        m.spawn('item', p[0], p[1], m.R(r).floor, p[2] + math.pi, tags=['floor', 'wall'], group=r)
for r in rng.sample(segments, 10):
    p = wall_spot(r, 0.3)
    if p:
        m.spawn('item', p[0], p[1], m.R(r).floor, p[2] + math.pi, tags=['floor', 'tunnel'], group=r)
for r in rng.sample(cells + offices, 12):
    p = wall_spot(r, 0.5)
    if p:
        m.spawn('note', p[0], p[1], m.R(r).floor, p[2], tags=['floor'])
# Clumps lauern an Kreuzungen, Hounds streifen (Wiki); Smiler kommen mit der Dunkelheit (Regie)
jl = list(junctions.values())
for r in rng.sample(jl, 10):
    R = m.R(r)
    if math.hypot(R.cx - m.start[0], R.cy - m.start[1]) > 30 and not m.blocked(R.cx, R.cy):
        m.spawn('entity', R.cx, R.cy, R.floor, role=rng.choice(['clump', 'hound']), tags=['junction'])
for r in (anlage, lager):
    R = m.R(r)
    for _ in range(2):
        x, y = R.x0 + 5 + rng.random() * (R.w - 10), R.y0 + 5 + rng.random() * (R.h - 10)
        if not m.blocked(x, y):
            m.spawn('entity', x, y, R.floor, role='hound', tags=['hall'])
# Clumps auch an den Einmuendungen des Netzes in den Gleistunnel (Kreuzungen)
for r in top[::2][1:]:  # V13.3: Raster 15 m - Lager wie bisher etwa 28 m vom Start
    x = m.R(r).cx
    m.spawn('entity', x, G.cy, G.floor, role='clump', tags=['junction', 'gleis'])
# ueber jedem Lager eines Clumps ein Deckenschacht (er lauert darin und laesst sich fallen)
for s_ in m.spawns:
    if s_['kind'] == 'entity' and s_.get('role') == 'clump':
        ri = m.at(s_['pos'][0], s_['pos'][1])
        m.obj('ceiling_vent', s_['pos'][0], s_['pos'][1], m.R(ri).floor + m.R(ri).height, 0.0, tag='deco')


def safe_point(cid, r, label):
    R = m.R(r)
    x, y = R.cx, R.cy
    if m.blocked(x, y):
        p = wall_spot(r, 0.8)
        x, y = p[0], p[1]
    m.checkpoint(cid, x, y, 0.0, label)
    m.obj('safety_light', x, y, R.floor + R.height, 0.0, tag='emergency')


roam = [s_['pos'] for s_ in m.spawns if s_['kind'] == 'entity' and s_.get('role') in ('hound', 'clump')]


def far(r):
    R = m.R(r)
    return all(math.hypot(p[0] - R.cx, p[1] - R.cy) >= 16 for p in roam)


lit_cells = [c for c in cells if far(c)]
lit_cells.sort(key=lambda c: (m.R(c).cy, m.R(c).cx))
for k, r in enumerate(lit_cells[::max(1, len(lit_cells) // 6)][:6]):
    safe_point(f'kammer{k}', r, 'Kammer im Tunnelnetz')
safe_point('vorraum3', vor3, 'Vor der Tür nach Level 3')
safe_point('buero', [o for o in offices if far(o)][0], 'Büro im Bürogang')

# V13.3 (Nachforderung Wiki: Maschinen verschiedener Bauarten aus frueheren Epochen, nicht aufeinander abgestimmt):
# Bauart je Objekt zufaellig; erst nach allen Platzierungen gezogen, damit die Anordnung der Ebene gleich bleibt
VARIANTS = {'mach_cabinet': 3, 'junction_box': 4, 'mach_pump': 2, 'mach_tank': 2, 'pipe_riser': 2}
for o in m.objects:
    if o['model'] in VARIANTS:
        v_ = rng.randrange(VARIANTS[o['model']])
        if v_:
            o['variant'] = v_

# --- V13.3 Teil 4 (KREATUR_Fensterwesen.md, KREATUR_L2_Spuren.md): Gangenden fuer die Fensterwesen, Spuren der
# Skin-Stealer. Eigener Zufallsgenerator, nach allen anderen Platzierungen - die Anordnung der Ebene bleibt gleich.
import random as _random  # noqa: E402

t4 = _random.Random(4402)
ends_open = {r: set() for r in junctions.values()}
for d in m.doors:
    for a, b in ((d['a'], d['b']), (d['b'], d['a'])):
        if a in ends_open:
            A, B = m.R(a), m.R(b)
            ends_open[a].add((1, 0) if B.x0 >= A.x1 else (-1, 0) if B.x1 <= A.x0 else (0, 1) if B.y0 >= A.y1 else (0, -1))


def t4_free(x, y, r=0.9, zmax=2.6):
    """Kein Objekt (ausser Deckenleuchten) und kein Spawnpunkt im Umkreis r."""
    for o in m.objects:
        if math.hypot(o['pos'][0] - x, o['pos'][1] - y) < r and o['model'] not in mapkit_ceiling:
            return False
    for s_ in m.spawns:
        if math.hypot(s_['pos'][0] - x, s_['pos'][1] - y) < r:
            return False
    return not m.blocked(x, y)


import mapkit as _mapkit  # noqa: E402
mapkit_ceiling = _mapkit.CEILING_MOUNT

# Fensterwesen (Wikidot Level 2: selten, am Ende von Gaengen): Stirnwand einer Sackgasse - dort entsteht das Fenster,
# wenn niemand hinsieht (presence_window_l2.cpp). Spawnpunkt 0,3 m vor der Wand, Blick in den Gang.
win_ends = []
for r, dirs in sorted(ends_open.items()):
    if len(dirs) != 1:
        continue
    R = m.R(r)
    (ox, oy), = dirs
    if ox:
        x = R.x0 + 0.3 if ox > 0 else R.x1 - 0.3
        y, yaw = R.y0 + R.h / 2, (0.0 if ox > 0 else math.pi)
        wx = R.x0 if ox > 0 else R.x1
        wy = y
    else:
        y = R.y0 + 0.3 if oy > 0 else R.y1 - 0.3
        x, yaw = R.x0 + R.w / 2, (math.pi / 2 if oy > 0 else -math.pi / 2)
        wx, wy = x, (R.y0 if oy > 0 else R.y1)
    if math.hypot(x - m.start[0], y - m.start[1]) < 35 or min(R.w, R.h) < 2 or not t4_free(wx, wy, 0.85):
        continue
    if any(math.hypot(x - p[0], y - p[1]) < 28 for p in win_ends):
        continue
    win_ends.append((x, y))
    m.spawn('entity', x, y, R.floor, yaw, role='window', tags=['end'])
print('Fensterwesen: Gangenden', len(win_ends))

# Spuren der Skin-Stealer (Wikidot Level 2: nach dem Stromausfall fort oder verhungert; Hitze reizte sie)
traces = []


def t4_place(model, x, y, z, yaw, variant=0, r=1.0):
    if not t4_free(x, y, r):
        return False
    m.obj(model, x, y, z, yaw, variant=variant, tag='deco')
    traces.append((model, x, y))
    return True


# die Lagerstelle: in einer Ecke einer kuehlen Kammer weit vom Start, dazu Kratzspuren, eine aufgehaengte Haut, ein Schaedel
lair_room = None
cand = [r for r in C.values() if m.R(r).type == 'kammer' and zone_at(m.R(r).cx, m.R(r).cy) not in ('heiss',)
        and min(m.R(r).w, m.R(r).h) >= 4 and math.hypot(m.R(r).cx - m.start[0], m.R(r).cy - m.start[1]) > 60]
t4.shuffle(cand)
for r in cand:
    R = m.R(r)
    for (cx_, cy_, yaw) in ((R.x0, R.y0, 0.0), (R.x1, R.y0, math.pi / 2), (R.x1, R.y1, math.pi), (R.x0, R.y1, -math.pi / 2)):
        c_, s_ = math.cos(yaw), math.sin(yaw)
        ox, oy = cx_ + (0.95 * c_ - 0.9 * s_), cy_ + (0.95 * s_ + 0.9 * c_)
        near_door = any(m.in_door(math.floor(cx_ + dx), math.floor(cy_ + dy)) for dx in (-2.5, -1.5, -0.5, 0.5, 1.5) for dy in (-2.5, -1.5, -0.5, 0.5, 1.5))
        if near_door or not all(t4_free(ox + ax, oy + ay, 0.6) for ax in (-0.6, 0.6) for ay in (-0.6, 0.6)):
            continue
        if t4_place('skin_lair', ox, oy, R.floor, yaw, r=0.3):
            lair_room = r
            m.obj('skull', ox + 0.75 * c_ + 0.55 * s_, oy + 0.75 * s_ - 0.55 * c_, R.floor, yaw + 2.2, tag='deco')
            break
    if lair_room is not None:
        break
if lair_room is not None:
    faces = [f for f in m.wall_faces(lair_room, margin=1) if t4_free(f[0] + math.cos(f[2]) * 0.3, f[1] + math.sin(f[2]) * 0.3, 0.7)]
    t4.shuffle(faces)
    used = []
    for f in faces:
        if any(math.hypot(f[0] - u[0], f[1] - u[1]) < 1.4 for u in used):
            continue
        model = 'skin_hung' if not any(t == 'skin_hung' for t, _, _ in traces) else 'skin_scratches'
        m.obj(model, f[0], f[1], m.R(lair_room).floor, f[2], tag='deco')
        traces.append((model, f[0], f[1]))
        used.append(f)
        if len(used) >= 3:
            break
# abgestreifte Haeute: zwei in kuehlen Gaengen, eine geschmolzen in den heissen Tunneln; Kratzspuren in der Hitze
segs = [r for r in segments if r is not None and min(m.R(r).w, m.R(r).h) == 2 and max(m.R(r).w, m.R(r).h) >= 6
        and math.hypot(m.R(r).cx - m.start[0], m.R(r).cy - m.start[1]) > 40]
t4.shuffle(segs)
want = [('skin_shed', 0, False), ('skin_shed', 0, False), ('skin_shed', 1, True)]
for model, var, hot in want:
    for r in segs:
        R = m.R(r)
        if (R.type == 'tunnel_heiss') != hot or any(math.hypot(R.cx - x, R.cy - y) < 25 for _, x, y in traces):
            continue
        yaw = (0.0 if R.w > R.h else math.pi / 2) + (math.pi if t4.random() < 0.5 else 0.0)
        if t4_place(model, R.cx, R.cy, R.floor, yaw, variant=var, r=1.3):
            break
hot_segs = [r for r in segs if m.R(r).type == 'tunnel_heiss']
for r in hot_segs[:6]:
    faces = [f for f in m.wall_faces(r, margin=1) if t4_free(f[0] + math.cos(f[2]) * 0.3, f[1] + math.sin(f[2]) * 0.3, 0.8)]
    if faces and sum(1 for t, _, _ in traces if t == 'skin_scratches') < 5:
        f = t4.choice(faces)
        m.obj('skin_scratches', f[0], f[1], m.R(r).floor, f[2], tag='deco')
        traces.append(('skin_scratches', f[0], f[1]))
print('Spuren der Skin-Stealer:', ', '.join(f'{t} ({x:.0f}, {y:.0f})' for t, x, y in traces))

# Death Rats (KREATUR_DeathRat.md; Wikidot Level 2: sie fressen die Fluessigkeiten in Faessern und Essensreste): neben
# einem Teil der Faesser liegen Essensreste in einer Lache aus dem leckenden Fass (presence_rat_l2.cpp sucht beides)
t4r = _random.Random(4403)
# die Ebene hatte nur drei Faesser - die Seite nennt Oelfaesser auf Wagen in den Gaengen: Fassgruppen an Kammerwaenden
barrel_spots = [tuple(o['pos'][:2]) for o in m.objects if o['model'] == 'barrel']
rooms_b = [r for r in C.values() if m.R(r).type in ('kammer', 'lagerraum') and r != lair_room] + \
          [r for r in segments if r is not None and min(m.R(r).w, m.R(r).h) >= 3]
t4r.shuffle(rooms_b)
for r in rooms_b:
    if len(barrel_spots) >= 20:
        break
    R = m.R(r)
    faces = [f for f in m.wall_faces(r, margin=1)]
    t4r.shuffle(faces)
    for (fx, fy, fyaw) in faces[:6]:
        c_, s_ = math.cos(fyaw), math.sin(fyaw)
        x, y = fx + c_ * 0.4, fy + s_ * 0.4
        if any(math.hypot(x - p[0], y - p[1]) < 15 for p in barrel_spots) or math.hypot(x - m.start[0], y - m.start[1]) < 25:
            continue
        if m.in_door(math.floor(x), math.floor(y)) is not None or not t4_free(x, y, 0.75):
            continue
        if min(R.w, R.h) < 4 and not all(not m.blocked(fx + c_ * d_, fy + s_ * d_) for d_ in (0.5, 1.5)):
            continue
        n_ = t4r.choice((1, 2, 2, 3))
        for j in range(n_):  # nebeneinander an der Wand
            off = (j - (n_ - 1) / 2) * 0.62
            px, py = x - s_ * off, y + c_ * off
            if j and (m.at(px, py) != r or not t4_free(px, py, 0.3) or m.in_door(math.floor(px), math.floor(py)) is not None):
                continue
            m.obj('barrel', px, py, R.floor, t4r.uniform(0, 6.28), variant=t4r.randrange(3), tag='deco')
        barrel_spots.append((x, y))
        break
food = 0
for o in list(m.objects):
    if o['model'] != 'barrel' or t4r.random() > 0.5:
        continue
    bx, by, bz = o['pos']
    away = None
    for (dx, dy) in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if m.at(bx - dx * 0.75, by - dy * 0.75) < 0 and m.at(bx + dx * 0.9, by + dy * 0.9) >= 0:
            away = (dx, dy)
            break
    if away is None:
        continue
    fx, fy = bx + away[0] * 0.55, by + away[1] * 0.55
    if m.at(fx, fy) < 0 or m.in_door(math.floor(fx), math.floor(fy)) is not None or not t4_free(fx, fy, 0.42):
        continue
    m.obj('food_scraps', fx, fy, bz, math.atan2(away[1], away[0]) + t4r.uniform(-0.4, 0.4), variant=t4r.randrange(2), tag='deco')
    food += 1
print('Essensreste an Faessern:', food)

# --- V13.3 Teil 4 (KREATUR_Jerry.md; Wikidot Level 2: Jerry ist an seinen Raum in der Basis der Followers gebunden; Entity 7:
# in Level 2 auf einem rostigen Rohr, die Followers bemalen alles blau, kleben Bilder von ihm an die Waende und locken mit
# Wegweisern). Eine Kammer mit nur einer Tuer weit vom Start: das Sitzrohr vor der Rueckwand (Spawnpunkt "entity"/"jerry"),
# dahinter das Wandbild, an der Seite der Opfertisch; blau gestrichen (Zone "jerry"), Licht gedimmt. Davor im Gang Fotos
# und Pfeile zur Tuer. Eigener Zufallsgenerator, nach allen anderen Platzierungen.
t5 = _random.Random(4404)


def t5_doors(r):
    return [d for d in m.doors if r in (d['a'], d['b'])]


def t5_dcenter(d):
    x0, y0, x1, y1 = d['rect']
    return (x0 + x1) / 2, (y0 + y1) / 2


jerry_room = None
cand = [r for r in C.values() if m.R(r).type in ('kammer', 'lagerraum') and r != lair_room and r not in mac_cells
        and min(m.R(r).w, m.R(r).h) >= 5 and len(t5_doors(r)) == 1
        and math.hypot(m.R(r).cx - m.start[0], m.R(r).cy - m.start[1]) > 70
        and all(math.hypot(m.R(r).cx - x, m.R(r).cy - y) > 25 for _, x, y in traces)]
t5.shuffle(cand)
for r in cand:
    R = m.R(r)
    d = t5_doors(r)[0]
    dx, dy = t5_dcenter(d)
    # Rueckwand: der Tuer gegenueber; Sitzplatz 0,65 m davor, Blick zur Tuer
    if abs(dx - R.cx) / R.w > abs(dy - R.cy) / R.h:
        bx = R.x1 if dx < R.cx else R.x0
        px, py, pyaw = bx + (-0.65 if bx == R.x1 else 0.65), R.cy, (math.pi if bx == R.x1 else 0.0)
    else:
        by = R.y1 if dy < R.cy else R.y0
        px, py, pyaw = R.cx, by + (-0.65 if by == R.y1 else 0.65), (-math.pi / 2 if by == R.y1 else math.pi / 2)
    if not (t4_free(px, py, 0.8) and t4_free(px - math.cos(pyaw) * 0.04 + math.sin(pyaw) * 0.5, py - math.sin(pyaw) * 0.04 - math.cos(pyaw) * 0.5, 0.25)):
        continue
    faces = m.wall_faces(r, margin=1)
    back = [f for f in faces if abs(math.cos(f[2]) - math.cos(pyaw)) < 0.01 and abs(math.sin(f[2]) - math.sin(pyaw)) < 0.01]
    side = [f for f in faces if abs(math.cos(f[2]) * math.cos(pyaw) + math.sin(f[2]) * math.sin(pyaw)) < 0.01]
    if not back or len(side) < 2:
        continue
    mural = min(back, key=lambda f: math.hypot(f[0] - px, f[1] - py))
    # Opfertisch: an einer Seitenwand, 1-2 m von der Rueckwand; frei
    shrine = None
    t5.shuffle(side)
    for f in side:
        dback = abs((f[0] - px) * math.cos(pyaw) + (f[1] - py) * math.sin(pyaw))
        sx, sy = f[0] + math.cos(f[2]) * 0.3, f[1] + math.sin(f[2]) * 0.3
        if 0.6 < dback < 2.2 and t4_free(sx, sy, 0.55) and m.in_door(math.floor(sx), math.floor(sy)) is None:
            shrine = (sx, sy, f[2], f)
            break
    if shrine is None:
        continue
    jerry_room = r
    m.obj('jerry_perch', px, py, R.floor, pyaw, tag='deco')
    m.spawn('entity', px, py, R.floor, pyaw, role='jerry', tags=['jerry'])
    m.obj('jerry_mural', mural[0], mural[1], R.floor, mural[2], tag='deco')
    m.obj('jerry_shrine', shrine[0], shrine[1], R.floor, shrine[2], tag='deco')
    inner = [f for f in side if f is not shrine[3] and math.hypot(f[0] - shrine[3][0], f[1] - shrine[3][1]) > 1.5
             and m.in_door(math.floor(f[0] + math.cos(f[2]) * 0.4), math.floor(f[1] + math.sin(f[2]) * 0.4)) is None]
    if inner:
        f = t5.choice(inner)
        m.obj('jerry_poster', f[0], f[1], R.floor, f[2], tag='deco')
    R.zone = 'jerry'  # blau gestrichen (Level-JSON, Zone "jerry")
    for l_ in R.lights:
        l_[3] = 0.45  # Halbdunkel: die Roehre glimmt nur (Runde 6: bei 0,3 war er auch aus 3 m nur ein Umriss)
    # Kerzen auf dem Opfertisch (Kritikrunde 1: der Raum war schwarz, Jerry nicht zu erkennen) - warmes, tiefes Licht
    m.light(r, shrine[0], shrine[1], 0, 0.55, 4.6, z=R.floor + 1.1, panel=False, color=(1.0, 0.64, 0.32))  # Runde 3: heller, weiter
    R.kw['flicker'] = 1
    break
# Wegweiser der Followers: in den Gaengen vor der Tuer Fotos und Pfeile, die zur Tuer zeigen (Modell: +y zeigt zum Ziel)
markers = []
if jerry_room is not None:
    d0 = t5_doors(jerry_room)[0]
    frontier = [((d0['b'] if d0['a'] == jerry_room else d0['a']), t5_dcenter(d0))]
    seen = {jerry_room}
    for depth in range(3):
        nxt = []
        for (r, goal) in frontier:
            if r in seen:
                continue
            seen.add(r)
            R = m.R(r)
            fs = []
            for f in m.wall_faces(r, margin=1):
                ly = (-math.sin(f[2]), math.cos(f[2]))  # +y des Wandmodells in der Welt
                to = (goal[0] - f[0], goal[1] - f[1])
                dist = math.hypot(*to)
                if dist < 1.5 or dist > 7.0 or (to[0] * ly[0] + to[1] * ly[1]) / dist < 0.7:
                    continue
                if not t4_free(f[0] + math.cos(f[2]) * 0.3, f[1] + math.sin(f[2]) * 0.3, 0.6) or any(math.hypot(f[0] - x, f[1] - y) < 4.0 for x, y in markers):
                    continue
                fs.append(f)
            if fs:
                f = t5.choice(fs)
                m.obj('jerry_poster', f[0], f[1], R.floor, f[2], variant=0 if depth == 0 else 1, tag='deco')
                markers.append((f[0], f[1]))
            for d in t5_doors(r):
                o_ = d['b'] if d['a'] == r else d['a']
                if o_ not in seen:
                    nxt.append((o_, t5_dcenter(d)))
        frontier = nxt
    print('Jerrys Raum:', m.R(jerry_room).label if hasattr(m.R(jerry_room), 'label') else jerry_room,
          f'({m.R(jerry_room).cx:.0f}, {m.R(jerry_room).cy:.0f}), Wegweiser {len(markers)}')
else:
    print('Jerrys Raum: keine passende Kammer')

# --- V13.3 Teil 4 (KREATUR_CannibalCuisine.md, KREATUR_Swindlebird.md, KREATUR_Photoshop.md, KREATUR_Macchina.md): Randwesen
# von Level 2 ohne Rolle (gameplay/apparitions_l2.cpp). Eigener Zufallsgenerator, nach allen anderen Platzierungen - die
# Anordnung der Ebene bleibt gleich. Spawnpunkte kind "trace" mit set cuisine / swindle / photoshop / macchina.
t6 = _random.Random(4406)
used6 = [(x, y) for x, y in win_ends]


def cc_block(r, x, y, yaw):
    """Der Automat steht fest: die Kacheln unter seiner Grundflaeche (1,22 x 0,76 m) als unsichtbarer Block (Kollision,
    Schatten, Wege). None, wenn er dabei eine Tuer verstellen wuerde."""
    R = m.R(r)
    c, s = math.cos(yaw), math.sin(yaw)
    pts = [(x + c * dx - s * dy, y + s * dx + c * dy) for dx in (-0.38, 0.38) for dy in (-0.61, 0.61)]
    x0 = max(R.x0, math.floor(min(p[0] for p in pts) + 0.05))
    x1 = min(R.x1, math.ceil(max(p[0] for p in pts) - 0.05))
    y0 = max(R.y0, math.floor(min(p[1] for p in pts) + 0.05))
    y1 = min(R.y1, math.ceil(max(p[1] for p in pts) - 0.05))
    if any(m.in_door(i, j) is not None for i in range(x0, x1) for j in range(y0, y1)):
        return None
    # vor jeder Tuer haelt die Welt drei Kacheln frei (room.cpp, planClearance) - dort gaebe es keinen Block
    for d in m.doors:
        if r not in (d.get('a'), d.get('b')):
            continue
        dx0, dy0, dx1, dy1 = d['rect']
        for i in range(x0, x1):
            for j in range(y0, y1):
                gx = max(dx0 - (i + 0.5), 0, (i + 0.5) - dx1)
                gy = max(dy0 - (j + 0.5), 0, (j + 0.5) - dy1)
                if max(gx, gy) < 4.2:
                    return None
    return (r, x0, y0, x1, y1)
# Cannibal Cuisines (Wikidot Level 2: recht haeufig, meist am Ende von Sackgassen oder an Waenden ohne Maschinen und Rohre;
# ihr Licht war im Blackout ein Wegpunkt). Der Automat steht mit dem Ruecken 0,40 m vor der Stirnwand, Blick in den Gang.
cuisines = []
ends6 = []
for r, dirs in sorted(ends_open.items()):
    if len(dirs) != 1:
        continue
    R = m.R(r)
    (ox, oy), = dirs
    if ox:
        x, y, yaw = (R.x0 + 0.42 if ox > 0 else R.x1 - 0.42), R.y0 + R.h / 2, (0.0 if ox > 0 else math.pi)
    else:
        x, y, yaw = R.x0 + R.w / 2, (R.y0 + 0.42 if oy > 0 else R.y1 - 0.42), (math.pi / 2 if oy > 0 else -math.pi / 2)
    if math.hypot(x - m.start[0], y - m.start[1]) < 30 or min(R.w, R.h) < 2 or R.zone == 'jerry':
        continue
    ends6.append((x, y, R.floor, yaw, r))
t6.shuffle(ends6)
for (x, y, z, yaw, r) in ends6:
    if len(cuisines) >= 4:
        break
    # eine andere Sackgasse als die Fensterwesen (12 m), zu anderen Automaten 24 m
    if any(math.hypot(x - p[0], y - p[1]) < 12 for p in win_ends) or any(math.hypot(x - p[0], y - p[1]) < 24 for p in cuisines) \
            or not t4_free(x, y, 0.8):
        continue
    blk = cc_block(r, x, y, yaw)
    if blk is None:
        continue
    m.block(*blk, 1.83)
    # der Spawnpunkt liegt vor dem Block (Tag o: so weit dahinter steht der Automat)
    m.spawn('trace', x + math.cos(yaw) * 0.64, y + math.sin(yaw) * 0.64, z, yaw, set='cuisine', tags=['end', f'v{len(cuisines) % 3}', 'o0.64'])
    cuisines.append((x, y))
    used6.append((x, y))
# zwei weitere an kahlen Kammerwaenden (keine Maschine, kein Rohr, keine Tuer im Umkreis von 1,6 m)
walls6 = []
for r in C.values():
    R = m.R(r)
    if R.type != 'kammer' or R.zone in ('jerry', 'macchina') or min(R.w, R.h) < 4:
        continue
    for (fx, fy, fyaw) in m.wall_faces(r, margin=1):
        x, y = fx + math.cos(fyaw) * 0.42, fy + math.sin(fyaw) * 0.42
        if math.hypot(x - m.start[0], y - m.start[1]) < 30 or m.in_door(math.floor(x), math.floor(y)) is not None:
            continue
        if any(math.hypot(o['pos'][0] - fx, o['pos'][1] - fy) < 1.6 for o in m.objects if o['model'] not in mapkit_ceiling):
            continue
        walls6.append((x, y, R.floor, fyaw, r))
t6.shuffle(walls6)
for (x, y, z, yaw, r) in walls6:
    if len(cuisines) >= 6:
        break
    if any(math.hypot(x - p[0], y - p[1]) < 24 for p in used6) or not t4_free(x, y, 0.8):
        continue
    blk = cc_block(r, x, y, yaw)
    if blk is None:
        continue
    m.block(*blk, 1.83)
    m.spawn('trace', x + math.cos(yaw) * 0.64, y + math.sin(yaw) * 0.64, z, yaw, set='cuisine', tags=['wall', f'v{len(cuisines) % 3}', 'o0.64'])
    cuisines.append((x, y))
    used6.append((x, y))
print('Cannibal Cuisines:', len(cuisines), ', '.join(f'({x:.0f}, {y:.0f})' for x, y in cuisines))

# Swindlebird (Wikidot Level 2: eine alte, nie bestaetigte Meldung eines blauen Exemplars; Entity 192: Nester am Boden):
# moegliche Stellen am Ende langer gerader Tunnelstuecke - die Sichtung waehlt zur Laufzeit eine, die niemand sieht
swindle = 0
segs6 = [r for r in segments if r is not None and max(m.R(r).w, m.R(r).h) >= 8 and min(m.R(r).w, m.R(r).h) <= 3]
t6.shuffle(segs6)
for r in segs6:
    if swindle >= 28:
        break
    R = m.R(r)
    horiz = R.w > R.h
    for end in (0, 1):
        if horiz:
            x, y, yaw = (R.x0 + 0.8 if end == 0 else R.x1 - 0.8), R.y0 + R.h / 2 + t6.uniform(-0.4, 0.4), (0.0 if end == 0 else math.pi)
        else:
            x, y, yaw = R.x0 + R.w / 2 + t6.uniform(-0.4, 0.4), (R.y0 + 0.8 if end == 0 else R.y1 - 0.8), (math.pi / 2 if end == 0 else -math.pi / 2)
        if math.hypot(x - m.start[0], y - m.start[1]) < 30 or not t4_free(x, y, 0.5):
            continue
        m.spawn('trace', x, y, R.floor, yaw, set='swindle')
        swindle += 1
        break
print('Swindlebird: moegliche Stellen', swindle)

# Photoshop (Wikidot Level 2: keine Art, sehr selten, vor allem in Raeumen mit PCs; erste Sichtung bei EL3A; seit dem
# Blackout laeuft kaum noch Elektronik). Stellen neben den Schreibtischen der Bueros; der Lauf waehlt hoechstens eine.
ps6 = 0
desks6 = [o for o in m.objects if o['model'] == 'desk_crt' and m.at(o['pos'][0], o['pos'][1]) in offices]
t6.shuffle(desks6)
for o in desks6:
    if ps6 >= 6:
        break
    dx, dy, dz = o['pos']
    dyaw = o.get('yaw', 0.0)
    if math.hypot(dx - m.start[0], dy - m.start[1]) < 40:
        continue
    # hinter dem Monitor (lokal x = -0,95, an der Wand), Blick wie der Monitor (lokal +x): ueber dem Roehrenbildschirm,
    # der sein Bild zeigt, ragt das Fotogesicht auf; wer an die Tastatur tritt, ist naeher als fuenf Fuss. Davor 1,6 m frei
    c_, s_ = math.cos(dyaw), math.sin(dyaw)
    for side in (1, -1):
        lx, ly = -0.95, -0.08 * side
        x, y = dx + c_ * lx - s_ * ly, dy + s_ * lx + c_ * ly
        fx_, fy_ = dx + c_ * 1.6, dy + s_ * 1.6
        if m.at(x, y) != m.at(dx, dy) or m.at(fx_, fy_) != m.at(dx, dy) or m.in_door(math.floor(x), math.floor(y)) is not None \
                or not t4_free(x, y, 0.5) or not t4_free(fx_, fy_, 0.5):
            continue
        m.spawn('trace', x, y, dz, dyaw, set='photoshop', tags=[f'pc{dx:.3f},{dy:.3f},{dyaw:.4f},{o.get("variant", 0)}'])
        ps6 += 1
        break
print('Photoshop: moegliche Stellen', ps6)

# Macchina (Wikidot Level 2 / Entity 189: seit etwa 1850 nicht mehr gesehen; ihre Anzuege trugen sie lebenslang, ohne Anzug
# galt man als nicht lebendig, jedes dritte Kind starb ohne Anzug; Bilder von Gestalten, die aus Lichtkugeln herabsteigen):
# in der Werkstatt ein leerer Anzug an der Wand, ein unfertiges Kindergestell, ein Wandtuch, ein Speer; Fundstelle am Anzug
mac6 = 0
for r in mac_cells:
    R = m.R(r)
    faces = [f for f in m.wall_faces(r, margin=1) if t4_free(f[0] + math.cos(f[2]) * 0.5, f[1] + math.sin(f[2]) * 0.5, 0.75)]
    t6.shuffle(faces)
    placed = []

    def far6(f, d=1.6):
        return all(math.hypot(f[0] - p[0], f[1] - p[1]) >= d for p in placed)
    for name, inward, kind in (('macchina_suit', 0.45, 'floor'), ('macchina_tapestry', 0.0, 'wall'), ('macchina_cradle', 0.4, 'floor'),
                               ('macchina_spear', 0.12, 'floor')):
        for f in faces:
            if not far6(f) or m.in_door(math.floor(f[0] + math.cos(f[2]) * 0.6), math.floor(f[1] + math.sin(f[2]) * 0.6)) is not None:
                continue
            x, y = f[0] + math.cos(f[2]) * inward, f[1] + math.sin(f[2]) * inward
            if kind == 'floor' and not t4_free(x, y, 0.45):
                continue
            m.obj(name, x, y, R.floor, f[2], tag='deco')
            placed.append(f)
            if name == 'macchina_suit':
                m.spawn('trace', x, y, R.floor, f[2], set='macchina', tags=['suit'])
            mac6 += 1
            break
print('Macchina-Reste:', mac6)

# V13.3 Abnahme Level 2 (Kritikrunde 6): Im Laufweg lagen bis zu 40 m tote Roehrengruppen am Stueck voellig schwarz. LOOK_2:
# violettes Notlicht, klein und schwach, brennt auch im Stromausfall (Wiki: seit dem Ausfall vereinzelt). In den meisten
# toten Gruppen haengt daher eine Notleuchte an der Wand - nur ihr eigenes Leuchten, keine Raumleuchte: das Licht der
# Kacheln (Dunkel fuer Smiler, Brownout) bleibt unveraendert. Eigener Zufall und als Letztes gesetzt: alles andere bleibt.
vr6 = _random.Random(260611)
# Abstand je Wandobjekt (Mitte zu Mitte): Tueren, Kessel und Notleuchten 1,0 m; Schraenke, Steigleitungen, Kaesten 0,6 m
# (die Leuchte haengt mit 2,05 m ueber Pumpen und unter den Rohrbuendeln ab 2,13 m)
WALLSTUFF = {'door_': 1.0, 'mach_tank': 1.0, 'violet_lamp': 1.6, 'mach_cabinet': 0.65, 'pipe_riser': 0.6, 'junction_box': 0.6,
             'pipe_wall': 0.6, 'skin_': 1.0, 'jerry_': 1.0, 'macchina_': 1.0, 'chamfer': 0.8}


def wall_clear(o):
    for k, d in WALLSTUFF.items():
        if o['model'].startswith(k):
            return d
    return 0.0


wall_objs = [(o['pos'][0], o['pos'][1], wall_clear(o)) for o in m.objects if wall_clear(o) > 0]
violet6 = 0
for (r, grp) in DEAD_GROUPS:
    R = m.R(r)
    if R.zone in ('brownout', 'leere') or vr6.random() > 0.85:
        continue
    mx = sum(p[0] for p in grp) / len(grp)
    my = sum(p[1] for p in grp) / len(grp)
    faces = [f for f in m.wall_faces(r, margin=1) if math.hypot(f[0] - mx, f[1] - my) < 3.5
             and m.in_door(math.floor(f[0] + math.cos(f[2]) * 0.6), math.floor(f[1] + math.sin(f[2]) * 0.6)) is None
             and all(math.hypot(f[0] - ox, f[1] - oy) > od for (ox, oy, od) in wall_objs)]
    if not faces:
        continue
    fx, fy, fyaw = min(faces, key=lambda f: math.hypot(f[0] - mx, f[1] - my) + vr6.random() * 1.5)
    m.obj('violet_lamp', fx, fy, R.floor + 2.05, fyaw, tag='emergency')
    wall_objs.append((fx, fy, 1.6))
    violet6 += 1
print('Notlichter in toten Gruppen:', violet6, 'von', sum(1 for (r_, g_) in DEAD_GROUPS if m.R(r_).zone not in ('brownout', 'leere')))
m.save()
print('Knicke mit Schraege:', chamfers)
