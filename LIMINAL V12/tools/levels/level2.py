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
    if x >= 64:
        return 'ziegel'
    if y >= 92 and x < 56:
        return 'heiss'
    return 'beton'


J, S, C = corridor_maze(m, 2, 7, 122, 127, 10, 2, 'tunnel', 'beton', CELL_TYPES, lambda t: 'kammer', height=TUN_H,
                        density=0.62, room_chance=0.32, circuit=C_TUN, light_every=99)
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
anlage = m.room(124, 2, 188, 50, 'anlage', 'anlage', 0.0, ANL_H, circuit=C_HALLE, label='Anlage', tags=['hall'])
m.opening(gleis, anlage, width=3, at=3.5)
lager = m.room(124, 52, 188, 100, 'packlager', 'lager', 0.0, LAGER_H, circuit=C_HALLE, label='Packlager', tags=['hall'])
m.door(anlage, lager, 'wide', 4, at=150, top=3.2)
# Bueroring: vier Gaenge um einen Block mit Bueros (fuehrt im Kreis zur Tuer zurueck)
ring_n = m.room(126, 104, 186, 106, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO, label='Bürogang')
ring_s = m.room(126, 154, 186, 156, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO)
ring_w = m.room(126, 107, 128, 153, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO)
ring_e = m.room(184, 107, 186, 153, 'bueroflur', 'buero_flur', 0.0, BUERO_H, circuit=C_BUERO)
for a, b in ((ring_n, ring_w), (ring_n, ring_e), (ring_s, ring_w), (ring_s, ring_e)):
    m.opening(a, b, width=2)
offices = []
for k, y in enumerate(range(108, 149, 6)):
    row = []
    for x0, x1 in ((129, 137), (138, 146), (147, 155), (157, 165), (166, 174), (175, 183)):
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
    tgt = anlage if R.cy < 50 else (lager if R.cy < 100 else ring_w)
    try:
        m.door(r, tgt, 'door', 1)
    except ValueError:
        pass
# Tuer nach Level 3: in der Ostwand der Anlage, dahinter ein kurzer Vorraum mit dem Uebergang
vor3 = m.room(189, 22, 196, 25, 'vorraum', 'vorraum3', 0.0, TUN_H, circuit=C_ROOMS)
d3 = m.doors[m.door(anlage, vor3, 'door', 1, top=2.1)]
x0, y0, x1, y1 = d3['rect']
m.obj('door_level3', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, 0.0)
m.transition('l2_l3', 'door', 'level3', 194.5, 23.5, 0.0, rect=[193, 22, 196, 25])

# --- Leerraum: hinter einer Tuer mit Fenster nur Schwarz ---------------------------------------------------------
south = [r for r in junctions.values() if m.R(r).y1 == 119 and 30 <= m.R(r).cx <= 60]
leer = m.room(28, 121, 66, 150, 'leerraum', 'leere', 0.0, 4.0, circuit=C_ROOMS, label='Leerraum')
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
    if rng.random() < (0.12 if r != gleis else 1.0):
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
    for yaw, rl in runs.items():
        c, s = math.cos(yaw), math.sin(yaw)
        for run in rl:
            k = 0
            while k < len(run):
                fx, fy = run[k]
                u = rng.random()
                if yaw == mach_side and u < 0.62 and k + 1 < len(run):
                    md, w, depth = rng.choice(MACH)
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
                    m.obj(rng.choice(DOORS), fx, fy, R.floor, yaw, tag='deco')
                    k += 3
                    continue
                if u < 0.32 and yaw != mach_side:
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
m.obj('hopper_wagon', 61.5, G.y0 + 1.6, 0.0, 0.0)
m.block(gleis, 60, G.y0 + 1, 63, G.y0 + 3, 1.0, hidden=True)

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
for y in range(A.y0 + 4, A.y1 - 2, 7):
    m.obj('pipe_run', A.cx, y + 0.5, A.floor + A.height - 0.5, 0.0, scale=[A.w - 1, 1, 1], variant=1, tag='pipes')
    m.obj('pipe_run', A.cx, y + 0.85, A.floor + A.height - 0.62, 0.0, scale=[A.w - 1, 1, 1], variant=0, tag='pipes')
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

# --- Gegenstaende am Boden: Faesser, Wagen, Werkzeug, Leitern, Kabeltrommeln, Klappstuehle --------------------------------
LOOSE = ['barrel', 'barrel', 'trolley_flatbed', 'toolbox', 'stepladder', 'cable_reel', 'folding_chair']
for r in segments + list(C.values()):
    R = m.R(r)
    if rng.random() > 0.35:
        continue
    for _try in range(6):
        x, y = R.x0 + 0.5 + rng.random() * (R.w - 1), R.y0 + 0.5 + rng.random() * (R.h - 1)
        if not m.blocked(x, y) and m.in_door(math.floor(x), math.floor(y)) is None:
            md = rng.choice(LOOSE)
            m.obj(md, x, y, R.floor, rng.random() * 6.28)
            break

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
for r in top[::3][1:]:
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

m.save()
print('Knicke mit Schraege:', chamfers)
