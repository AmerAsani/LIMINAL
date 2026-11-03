r"""Level 188 - "The Courtyard of Windows" (V12, LEVELSPEC_188.md / LOOK_188.md; Wikidot Level 188, Revision 69).

Draufsicht (x nach rechts, y nach unten):

        +--------------------------------------------+
        |  Fensterreihen ringsum (5 Reihen, ~30 m)   |##
   ->[E]|      Moos   /\  fuenfeckiger Weg           |##  Tuer (Mahagoni) -> weisser Gang -> leere Zellen,
   Eingangs-        /    \  vier Wege zum Kreis    [T]|== Treppenhaus
   fenster  (Hotel) \ () /  Kreis aus Birkenholz      |
        |           Tisch, Grammophon, Stuetzen      |
        +--------------------------------------------+
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level188', 70, 70, seed=1880)
rng = m.rng
HOF_H = 30.0  # Wiki: rund 33 Yard hoch
SKY = (0.90, 0.95, 1.00)
TUBE = (0.92, 0.97, 1.00)
C_HOF, C_GANG = 1, 2

# --- Hof (Wiki: unregelmaessiges Atrium, rund 50 Yard) --------------------------------------------------------------------
A = m.room(10, 10, 56, 56, 'hof', 'hof', 0.0, HOF_H, circuit=C_HOF, label='Hof der Fenster')
for (x0, y0, x1, y1) in ((48, 10, 56, 16), (10, 49, 16, 56), (53, 18, 56, 26), (10, 10, 13, 18), (13, 10, 18, 13), (40, 53, 56, 56)):
    m.block(A, x0, y0, x1, y1, HOF_H, hidden=False)  # Vorspruenge der Wand: der Hof ist kein Rechteck
R = m.R(A)
CX, CY = 33.0, 33.0

# Eingangsfenster (Rueckweg nach Level 5; Wiki: hinaus nur durch das Fenster, durch das man kam)
m.obj('window_entry_188', R.x0 + 0.005, 33.5, 1.9, 0.0, tag='window')
m.light(A, R.x0 + 0.8, 33.5, 0, 0.5, 4.5, z=1.9, panel=False, color=(1.0, 0.72, 0.48))
m.start = (12.2, 33.5, 0.0)
m.transition('l5_188', 'seam', '', 12.2, 33.5, 0.0)  # Ankunft aus Level 5 durch das helle Fenster
m.transition('l188_l5', 'seam', 'level5', 10.5, 33.5, math.pi, rect=[10, 32, 11, 35])

# Gang hinter einer Mahagonituer (Wiki: weisse, grell beleuchtete Gaenge; die meisten Tueren von innen verschlossen,
# die zugaenglichen Raeume sind leere Zellen aus weissem Beton; Treppenhaeuser ueber mehrere Etagen)
G = m.room(57, 30, 60, 54, 'gang', 'weiss', 0.0, 3.0, circuit=C_GANG, label='Weisser Gang')
dG = m.doors[m.door(A, G, 'door', 1, at=38, top=2.2)]
x0, y0, x1, y1 = dG['rect']
m.obj('door_mahogany_open', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, 0.0)
cells = []
for y in (31, 37, 43):
    c = m.room(61, y, 66, y + 4, 'zelle', 'zelle', 0.0, 2.8, circuit=C_GANG)
    d = m.doors[m.door(G, c, 'door', 1, top=2.2)]
    x0, y0, x1, y1 = d['rect']
    m.obj('door_mahogany_open', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, 0.0)
    cells.append(c)
# V13.3 (Kritikrunde 1): die Treppe stieg im 3 m hohen Gang auf 3 m - oben ragte der Kopf ueber die Waende (Hof und
# schwarzer Himmel sichtbar). Jetzt steigt sie im eigenen, 6 m hohen Treppenhaus zu einem Absatz auf 2,7 m.
ST = m.room(57, 55, 60, 66, 'treppenhaus', 'weiss', 0.0, 6.0, circuit=C_GANG, label='Treppenhaus')
m.door(G, ST, 'door', 3, top=2.4)
ST2 = m.room(57, 67, 60, 69, 'treppenhaus', 'weiss', 2.7, 3.0, circuit=C_GANG, label='Treppenabsatz')
m.door(ST, ST2, 'door', 3, top=2.4)


# --- Wandflaechen des Hofs (Raender und Vorspruenge) ----------------------------------------------------------------------
def in_block(r, x, y):
    """Kachel in einem Block des Raums (auch in sichtbaren, die mapkit als begehbares Podest zaehlt)."""
    return any(b[0] <= x < b[2] and b[1] <= y < b[3] for b in m.R(r).blocks)


def faces_full(r):
    """Alle Wandflaechen je Kachel: nach aussen (Wand) und zu Bloecken hin. Liste von Laeufen (yaw, [(x, y)])."""
    R_ = m.R(r)
    pts = {}
    for x in range(R_.x0, R_.x1):
        for y in range(R_.y0, R_.y1):
            if in_block(r, x + 0.5, y + 0.5):
                continue
            for (dx, dy, yaw) in ((-1, 0, 0.0), (1, 0, math.pi), (0, -1, math.pi / 2), (0, 1, -math.pi / 2)):
                nx, ny = x + dx, y + dy
                inside = R_.x0 <= nx < R_.x1 and R_.y0 <= ny < R_.y1
                if inside and not in_block(r, nx + 0.5, ny + 0.5):
                    continue
                if not inside and m.in_door(nx, ny):
                    continue
                fx = x + 0.5 + dx * 0.495
                fy = y + 0.5 + dy * 0.495
                key = (round(yaw, 3), round(fx, 3) if dx else round(fy, 3))
                pts.setdefault(key, []).append((fx, fy))
    runs = []
    for (yaw, _), p in pts.items():
        along_y = abs(math.cos(yaw)) > 0.5
        p.sort(key=lambda q: q[1] if along_y else q[0])
        cur = [p[0]]
        for q in p[1:]:
            if abs((q[1] - cur[-1][1]) if along_y else (q[0] - cur[-1][0]) - 1.0) < 0.01:
                cur.append(q)
            else:
                runs.append((yaw, cur))
                cur = [q]
        runs.append((yaw, cur))
    return runs


runs = faces_full(A)
sills = []  # Fensterbaenke im Erdgeschoss: Funde und Notizen
ROWS = (1.9, 6.5, 11.0, 15.5, 20.0, 24.5)
n_win = 0
for yaw, run in runs:
    c, s_ = math.cos(yaw), math.sin(yaw)
    for k, (fx, fy) in enumerate(run):
        if k % 4 != 1 or k == len(run) - 1:
            continue
        for z in ROWS:
            if z < 3 and (abs(fy - 33.5) < 3 and fx < 12 or any(abs(fx - (d['rect'][0] + d['rect'][2]) / 2) < 2.5 and abs(fy - (d['rect'][1] + d['rect'][3]) / 2) < 2.5 for d in m.doors)):
                continue  # Eingangsfenster und Tueren frei halten
            u = rng.random()
            model = ('window_188_a' if u < 0.38 else 'window_188_b' if u < 0.62 else 'window_188_c' if u < 0.84
                     else 'window_188_lit' if u < 0.93 else 'window_188_fig')
            m.obj(model, fx, fy, z, yaw, tag='window')
            n_win += 1
            if z < 3:
                sills.append((fx + c * 0.12, fy + s_ * 0.12, 0.93, yaw))
# verschlossene Mahagonituren im Erdgeschoss zwischen den Fenstern
locked = 0
for yaw, run in runs:
    for k, (fx, fy) in enumerate(run):
        if k % 8 == 3 and len(run) > 6 and locked < 7 and not (fx < 12 and abs(fy - 33.5) < 4):
            m.obj('door_mahogany', fx, fy, 0.0, yaw, tag='deco')
            locked += 1

# Galerien mit Glasgelaender an zwei Waenden (Wiki: Glasgelaender, Stege), nicht begehbar
for yaw, run in runs:
    if len(run) >= 14 and abs(yaw - math.pi / 2) < 0.01:
        for (fx, fy) in run[2:14]:
            m.obj('walkway_glass', fx, fy, 9.2, yaw, tag='deco')
    if len(run) >= 10 and abs(yaw) < 0.01 and run[0][1] > 20:
        for (fx, fy) in run[1:9]:
            m.obj('walkway_glass', fx, fy, 17.9, yaw, tag='deco')

# Oberlicht (eigene Ergaenzung: die Seite nennt keine Lichtquelle des Hofs)
for gx in range(14, 54, 8):
    for gy in range(14, 54, 8):
        if all(not in_block(A, gx + dx, gy + dy) and R.x0 <= gx + dx < R.x1 and R.y0 <= gy + dy < R.y1
               for dx in (-3.9, 0.5, 3.9) for dy in (-3.9, 0.5, 3.9)):
            m.obj('skylight_188', gx + 0.5, gy + 0.5, HOF_H, 0.0, tag='light')
            if (gx // 8 + gy // 8) % 2 == 0:  # jedes zweite Feld traegt ein Licht (Kosten der Beleuchtung)
                m.light(A, gx + 0.5, gy + 0.5, 0, 0.85, 36.0, z=HOF_H - 0.5, panel=False, color=SKY)

# Boden: Moos (Zone), fuenfeckiger Weg aus Pflaster, vier Wege zum Kreis aus Birkenholz
m.obj('birch_circle', CX, CY, 0.0, 0.0, tag='deco')
RP = 12.0
verts = [(CX + RP * math.cos(-math.pi / 2 + k * 2 * math.pi / 5), CY + RP * math.sin(-math.pi / 2 + k * 2 * math.pi / 5)) for k in range(5)]


def strip(ax, ay, bx, by):
    L = math.hypot(bx - ax, by - ay)
    yaw = math.atan2(by - ay, bx - ax)
    n = max(1, int(round(L)))
    for k in range(n):
        t = (k + 0.5) / n
        m.obj('paving_strip', ax + (bx - ax) * t, ay + (by - ay) * t, 0.0, yaw, scale=[L / n, 1.0, 1.0], tag='deco')


for k in range(5):
    strip(*verts[k], *verts[(k + 1) % 5])
for k in range(1, 5):  # vier Wege nach innen (von den Ecken, ausser der noerdlichen)
    vx, vy = verts[k]
    ang = math.atan2(vy - CY, vx - CX)
    strip(vx - math.cos(ang) * 0.8, vy - math.sin(ang) * 0.8, CX + math.cos(ang) * 3.55, CY + math.sin(ang) * 3.55)

# Tisch mit Gegenstaenden, Grammophon (Wiki: erscheinen an beliebiger Stelle; Musik = sofort gehen)
TX, TY = 40.5, 40.5
m.obj('table_188', TX, TY, 0.0, 0.4, tag='deco')
m.block(A, int(TX), int(TY), int(TX) + 1, int(TY) + 1, 0.75, hidden=True)
m.obj('gramophone', TX + 1.5, TY + 0.5, 0.0, 2.6, tag='music')
m.block(A, int(TX + 1.5), int(TY + 0.5), int(TX + 1.5) + 1, int(TY + 0.5) + 1, 0.9, hidden=True)
# Stuetzen (Wiki: Saeulen erscheinen)
for (px, py) in ((22.5, 21.5), (46.5, 30.5), (24.5, 46.5)):
    m.obj('pillar_188', px, py, 0.0, 0.0, tag='deco')
    m.block(A, int(px), int(py), int(px) + 1, int(py) + 1, HOF_H, hidden=True)

# --- Gang, Zellen, Treppenhaus: Leuchtstoffroehren, grell -----------------------------------------------------------------
GR = m.R(G)
for y in range(GR.y0 + 1, GR.y1, 3):
    m.obj('tube_white', GR.cx, y + 0.5, GR.floor + GR.height, math.pi / 2, tag='light')
    m.light(G, GR.cx, y + 0.5, 0, 0.9, 5.0, z=GR.floor + GR.height - 0.1, panel=False, color=TUBE)
for c in cells:
    C = m.R(c)
    m.obj('tube_white', C.cx, C.cy, C.floor + C.height, 0.0, tag='light')
    m.light(c, C.cx, C.cy, 0, 0.9, 4.5, z=C.floor + C.height - 0.1, panel=False, color=TUBE)
SR = m.R(ST)
for ly_ in (SR.y0 + 2.5, SR.y1 - 2.5):
    m.obj('tube_white', SR.cx, ly_, SR.floor + SR.height, math.pi / 2, tag='light')
    m.light(ST, SR.cx, ly_, 0, 0.9, 5.5, z=SR.floor + SR.height - 0.1, panel=False, color=TUBE)
SR = m.R(ST2)
m.obj('tube_white', SR.cx, SR.cy, SR.floor + SR.height, math.pi / 2, tag='light')
m.light(ST2, SR.cx, SR.cy, 0, 0.9, 4.5, z=SR.floor + SR.height - 0.1, panel=False, color=TUBE)
for (fx, fy, yaw) in m.wall_faces(ST2, margin=0):
    if fy > SR.y1 - 1:
        m.obj('door_mahogany', fx, fy, SR.floor, yaw, tag='deco')  # verschlossen (Wiki)
        break
for (fx, fy, yaw) in m.wall_faces(G, margin=1)[::7]:
    if abs(yaw) < 0.01 and fy < GR.y1 - 11:  # Westwand des Gangs: verschlossene Tueren (nicht an der Treppe)
        m.obj('door_mahogany', fx, fy, 0.0, yaw, tag='deco')

# --- Funde, Notizen, Sicherungspunkte ---------------------------------------------------------------------------------------
rng.shuffle(sills)
for (x, y, z, yaw) in sills[:5]:
    m.spawn('item', x, y, z, yaw, tags=['surface', 'sill'])
for (x, y, z, yaw) in sills[5:7]:
    m.spawn('note', x, y, z, yaw, tags=['surface', 'sill'])


def safe_point(cid, r, label, x, y):
    R_ = m.R(r)
    m.checkpoint(cid, x, y, 0.0, label)
    m.obj('safety_light', x, y, R_.floor + R_.height, 0.0, variant=1, tag='emergency')  # V13.3: ohne Notausgang-Piktogramm (STRUKTURTYP, signs.py)


m.checkpoint('fenster', 14.5, 33.5, 0.0, 'Am Eingangsfenster')
# V13.3: kein Notausgangsschild (Wiki nennt keine Schilder; der Weg hinaus ist das Eingangsfenster selbst)
safe_point('gang', G, 'Weisser Gang', GR.cx, GR.y0 + 4.5)

print('Fenster:', n_win)
# V13.3: Varianten der haeufigen Klassen (Blender, name@N.glb) zufaellig gewaehlt; Anordnung unveraendert.
# window_188_c@1 behaelt den Freiraum fuer das Fensterwesen (presence.cpp waehlt nach Modellname).
VARIANTS = {'window_188_a': 2, 'window_188_b': 2, 'window_188_c': 2, 'paving_strip': 3}
vrng = random.Random(18831)
for o in m.objects:
    if o['model'] in VARIANTS:
        v_ = vrng.randrange(VARIANTS[o['model']])
        if v_:
            o['variant'] = v_
# --- V13.3 Teil 4 (KREATUR_Horcher.md, KREATUR_Wurzelpistole.md): Orte der Wesen - am Ende, eigener Zufall ----------------
# Die Anordnung oben bleibt unveraendert; hier kommen nur Spawnpunkte (trace) und wenige Ausstattungsstuecke dazu.
hrng = random.Random(18841)
GR = m.R(G)
doors_ = [o for o in m.objects if o['model'] in ('door_mahogany', 'door_mahogany_open')]


def free_wall(fx, fy, gap=2.2):
    return all(math.hypot(fx - o['pos'][0], fy - o['pos'][1]) > gap for o in doors_)


# weitere verschlossene Mahagonituren an der Ostwand des Gangs hinter den Zellen (Wiki: die meisten Tueren sind von innen
# verschlossen) - hinter ihnen klopft es
east = [f for f in m.wall_faces(G, margin=1) if abs(abs(f[2]) - math.pi) < 0.01 and f[1] > 48.4 and free_wall(f[0], f[1])]
hrng.shuffle(east)
placed = []
for (fx, fy, yaw) in east:
    if len(placed) >= 2 or any(abs(fy - q) < 2.6 for q in placed):
        continue
    m.obj('door_mahogany', fx, fy, 0.0, yaw, tag='deco')
    doors_.append(m.objects[-1])
    placed.append(fy)
# Klopfen: vor jeder verschlossenen Tuer in Gang und Treppenhaus (0,75 m davor, Blick zur Tuer)
n_knock = 0
for o in doors_:
    if o['model'] != 'door_mahogany':
        continue
    ox, oy, oz = o['pos']
    yaw = o.get('yaw', 0.0)
    if not (56.5 <= ox <= 60.5 and 30.0 <= oy <= 70.0):
        continue
    kx, ky = ox + math.cos(yaw) * 0.75, oy + math.sin(yaw) * 0.75
    m.spawn('trace', kx, ky, oz, yaw + math.pi, set='hr', tags=['knock'])
    n_knock += 1
# Heraustreten: in jeder Zelle hinter der offenen Tuer, Blick in den Gang
n_emerge = 0
for d in m.doors:
    if {d['a'], d['b']} & set(cells) and G in (d['a'], d['b']):
        x0, y0, x1, y1 = d['rect']
        cy = (y0 + y1) / 2
        m.spawn('trace', x1 + 0.85, cy, 0.0, math.pi, set='hr', tags=['emerge'])
        n_emerge += 1
# Wurzelpistolen (Entity 39): eine im Moos des Hofs (abseits der Wege, bei ihrem leeren Kasten und den Probenstichen), eine im
# Riss des Gangbodens vor der Treppe (dort hoert sie der Horcher); Keimorte in Reichweite vor dem Lauf
blocked = [(o['pos'][0], o['pos'][1]) for o in m.objects if o.get('tag') not in ('light', 'window', 'emergency') and o['pos'][2] < 1.0]


def clear(x, y, r):
    return all(math.hypot(x - bx, y - by) > r for bx, by in blocked)


cand = []
for _ in range(400):
    a = hrng.uniform(0.0, 2.0 * math.pi)
    rr = hrng.uniform(9.0, 15.0)
    x, y = CX + math.cos(a) * rr, CY + math.sin(a) * rr
    if m.at(x, y) == A and clear(x, y, 2.2) and all(m.at(x + dx, y + dy) == A for dx in (-2.5, 2.5) for dy in (-2.5, 2.5)):
        cand.append((x, y, a))
gx, gy, ga = cand[0]
gyaw = ga + math.pi * 0.5
m.spawn('trace', gx, gy, 0.0, gyaw, set='e39', tags=['moss'])
for k in range(6):  # Keimorte im Moos vor dem Lauf
    a = gyaw + hrng.uniform(-0.6, 0.6)
    r_ = hrng.uniform(1.8, 5.0)
    sx, sy = gx + math.cos(a) * r_, gy + math.sin(a) * r_
    if m.at(sx, sy) == A and clear(sx, sy, 0.8):
        m.spawn('trace', sx, sy, 0.0, 0.0, set='e39', tags=['seed'])
cx_, cy_ = gx - math.cos(gyaw) * 1.4 + math.cos(gyaw + 1.57) * 0.9, gy - math.sin(gyaw) * 1.4 + math.sin(gyaw + 1.57) * 0.9
m.obj('e39_case', cx_, cy_, 0.0, gyaw + 0.4, tag='deco')
sx_, sy_ = gx + math.cos(gyaw + 1.57) * 2.2, gy + math.sin(gyaw + 1.57) * 2.2
m.obj('e39_samples', sx_, sy_, 0.0, gyaw + 1.57, tag='deco')
m.spawn('trace', sx_ + math.cos(gyaw) * 0.45, sy_ + math.sin(gyaw) * 0.45, 0.0, gyaw + 0.3, set='e39', tags=['note'])
fx_, fy_ = GR.x0 + 0.42, GR.y1 - 2.6  # Gangboden, Westseite, vor der Treppe
m.spawn('trace', fx_, fy_, 0.0, math.pi / 2, set='e39', tags=['floor'])
for k in range(4):
    m.spawn('trace', GR.x0 + hrng.uniform(0.4, 2.6), fy_ - hrng.uniform(2.0, 5.5), 0.0, 0.0, set='e39', tags=['seed'])
print('Horcher: Klopftueren', n_knock, 'Zellentueren', n_emerge, '- Wurzelpistole im Moos bei', round(gx, 1), round(gy, 1))
# --- V13.3 Abnahme Level 188 (KRITIK_Level188.md Runde 3) - am Ende, ohne Zufall; die Karte bleibt sonst gleich --------------
# Die gruene Leuchte des Sicherungspunkts hing genau unter einer Roehre (y0 + 4,5): die Roehre lief quer durch den Kasten, von
# weitem ein gruenes Kreuz (Apotheke/Erste Hilfe) und Geometrie in Geometrie. Leuchte und Sicherungspunkt 1,5 m nach Norden in
# die Luecke zwischen zwei Roehren (y0 + 1,5 und y0 + 4,5).
SAFE_Y = GR.y0 + 3.0
for o in m.objects:
    if o['model'] == 'safety_light' and abs(o['pos'][1] - (GR.y0 + 4.5)) < 0.01:
        o['pos'][1] = SAFE_Y
for cp in m.checkpoints:
    if cp['id'] == 'gang':
        cp['pos'][1] = round(SAFE_Y, 2)
m.save()
