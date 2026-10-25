"""Level 0 - "Die Vorzimmer": Entwurf der festen Karte (LEVEL_SELECTION.md 3.1, LEVEL_CHECKLIST.md Level 0).

Grundlage (Wikidot "Level 0 - Threshold", Revision 50 vom 06.10.2026, eigene Worte): ein zufaellig
gegliedertes Labyrinth aus Raeumen, Fluren und Treppen wie die Hinterzimmer eines Bueros oder Ladens; gelbe
Tapete, feuchter Teppich, brummende Leuchtstoffroehren; keine Tueren, keine Moebel. Dazu Bogenraeume (blasse
Waende, Bogen-Durchgaenge, an Uebergaengen, die ruhigsten Orte), Saeulenfelder, Lochfelder (Gruben im
Raster, ohne Boden), Dunkelzonen (rauhe Waende, Stille, knoecheltiefe Fluessigkeit), rote Raeume (meiden) und
selten ein kleiner Raum in Manila-Toenen. Ausgang: eine flackernde Wand nach Level 1.

Gestalt (Draufsicht, Norden oben = groesseres y):

    +------+--+--------+-------------------+----------------+
    |      |F |        |  Rote Raeume (Sackgasse)           |
    | West |l | West   +-------------------+                |
    |  A   |u |  B     |  Sued (+0,85 m,   |   Ost          |
    |      |r |        |  Treppen)         |                |=== Naht (flackernde Wand) ===> Level 1
    |Start |  | Manila +-------------------+                |
    |      |  |        |  Boegen           |                |
    |      |  |        +-------------------+----------------+
    |      |  |        |  Saeulenfeld      |  Dunkelzone    |
    |      |  |        +-------------------+  (nass, still) |
    |      |  |        |  Lochfeld         |                |
    +------+--+--------+-------------------+----------------+

Keine erfundenen Raumfunktionen: es gibt nur Abschnitte des Labyrinths, Flure und die belegten Sonderbereiche.
Bewusste Abweichungen und eigene Ergaenzungen stehen in LEVEL_CHECKLIST.md.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, link_regions, partition_rooms  # noqa: E402

m = Map('level0', 176, 134, seed=1001)
rng = m.rng


def yellow_type(w, h, r):
    if min(w, h) <= 3 and max(w, h) >= 8:
        return 'flur'
    if w * h >= 110:
        return 'halle'
    return 'zimmer'


def segmented(x0, y0, x1, y1, zone='vorzimmer', floor=0.0, extra=0.55):
    """Ein Abschnitt des Labyrinths: grosse Teile (4-14 m), zwischen fast allen Nachbarn breite Durchbrueche
    bis zur Decke (die Rasterdecke laeuft durch), oft zwei je Wand - Raeume fliessen ineinander, statt als
    geschlossene Zimmer mit einer Tuer aneinanderzuhaengen."""
    rooms = partition_rooms(m, x0, y0, x1, y1, zone, yellow_type, min_leaf=4, max_leaf=14, height=(2.85, 3.15),
                            stop_chance=0.25, loop_chance=0.9, width=(2, 4), floor=floor)
    have = {(d['a'], d['b']) for d in m.doors} | {(d['b'], d['a']) for d in m.doors}
    for a in rooms:
        for b in rooms:
            if a >= b:
                continue
            wl = m._wall(a, b)
            if not wl or wl[2] != 1:
                continue
            span = wl[4] - wl[3]
            if span < 4:
                continue
            if (a, b) not in have and rng.random() < extra:
                m.opening(a, b, width=rng.randint(2, min(4, span - 1)))
            elif span >= 9 and rng.random() < extra * 0.6:  # eine zweite Luecke: ein Wandstueck bleibt stehen
                m.opening(a, b, width=2, at=wl[3] + 1.5 + rng.random() * (span - 3))
    return rooms


# --- West: zwei Labyrinth-Abschnitte und dazwischen der lange Flur ----------------------------------------
westA = segmented(2, 2, 27, 132)
flur = m.room(28, 6, 31, 128, 'flur', 'vorzimmer', 0.0, 2.9, label='Der lange Flur')
westB = segmented(32, 2, 58, 132)
link_regions(m, westA, [flur], 7, width=(2, 3))
link_regions(m, westB, [flur], 7, width=(2, 3))

# --- Manila: ein kleiner Raum in Manila-Toenen, versteckt im West-B-Abschnitt -------------------------------
manila = min((r for r in westB if 12 <= m.R(r).w * m.R(r).h <= 40), key=lambda r: abs(m.R(r).cy - 40))
M_ = m.R(manila)
M_.type, M_.zone = 'manila', 'manila'
M_.kw['label'] = 'Vergilbtes Zimmer'  # im Spiel ohne den Wiki-Namen (Weg B)

# --- Nord: das Lochfeld (Gruben im Raster, kein Boden), ringsum Labyrinth ---------------------------------
loch = m.room(66, 10, 95, 32, 'lochfeld', 'loecher', 0.0, 3.0, label='Lochfeld')
nl, nr = segmented(59, 2, 65, 39), segmented(96, 2, 104, 39)
nt, nb = segmented(66, 2, 95, 9), segmented(66, 33, 95, 39)
nord = nl + nr + nt + nb
for strip in (nl, nr, nt, nb):  # das Lochfeld ist von allen Seiten zugaenglich
    link_regions(m, [loch], strip, 1, width=(2, 3))
for a, b in ((nl, nt), (nl, nb), (nr, nt), (nr, nb)):
    link_regions(m, a, b, 1, width=(2, 3))
for gx in range(69, 93, 4):
    for gy in range(13, 30, 4):
        m.block(loch, gx, gy, gx + 1 + (1 if rng.random() < 0.3 else 0), gy + 1, -14.0, hidden=False)
m.transition('l0_loch', 'hole', '', 80.5, 21.0, 0.0, rect=[66, 10, 95, 32])

# --- Saeulenfeld: weite Halle, Stuetzen alle 4 m -----------------------------------------------------------
sf = m.room(59, 40, 104, 80, 'saeulenhalle', 'saeulen', 0.0, 3.6, pillars=[4, 4, 1, 61, 42], label='Säulenfeld')

# --- Boegen: blasse Raeume, Bogen-Durchgaenge (Sturz 2,35 m, Bogen als Ausstattung) ------------------------
boegen = []
x = 59
while x + 6 <= 104:
    boegen.append(m.room(x, 81, x + 6, 90, 'bogen', 'boegen', 0.0, 3.2, label='Bögen' if not boegen else ''))
    x += 7


def arch_door(a, b, width=3, at=None):
    d = m.doors[m.door(a, b, 'wide', width, at=at, top=2.35)]
    x0, y0, x1, y1 = d['rect']
    yaw = 0.0 if d['axis'] == 0 else math.pi / 2
    m.obj('arch', (x0 + x1) / 2, (y0 + y1) / 2, m.R(a).floor, yaw, variant=x1 - x0 if d['axis'] == 1 else y1 - y0)


for a, b in zip(boegen, boegen[1:]):
    arch_door(a, b, 3)
for b in boegen[1::2]:
    arch_door(sf, b, 3)

# --- Sued: eine Stufe hoeher (Treppen an den Durchgaengen) --------------------------------------------------
sued = segmented(59, 91, 104, 132, floor=0.85)

# --- Dunkelzone: nasser Teppich, kein Licht, rauhe Waende, knoecheltiefe Fluessigkeit ----------------------
dunkel = partition_rooms(m, 105, 2, 142, 44, 'dunkel', lambda w, h, r: 'dunkelraum', min_leaf=4, max_leaf=12,
                         height=(2.7, 3.0), loop_chance=0.4, water=0.12)

# --- Ost --------------------------------------------------------------------------------------------------
ost = segmented(105, 45, 142, 110)

# --- Rote Raeume: eine Sackgasse, die am Verstand zehrt (meiden) -------------------------------------------
rot = partition_rooms(m, 105, 111, 142, 132, 'rot', lambda w, h, r: 'roter_raum', min_leaf=4, max_leaf=10,
                      height=(2.8, 3.0))
for r in rot:
    R = m.R(r)
    m.light(r, R.cx, R.cy, 0, 0.55, 4.0, color=(1.0, 0.16, 0.12))

# --- Naht zu Level 1: die Tapete bekommt Flecken, Beton kommt durch, eine Wandstelle flackert ---------------
naht = [m.room(143, 62, 151, 66, 'flur', 'vorzimmer', 0.0, 2.9)]
naht.append(m.room(152, 62, 160, 66, 'naht', 'naht0', 0.0, 2.9))
naht.append(m.room(161, 62, 167, 66, 'naht', 'naht1', 0.0, 2.9))
naht.append(m.room(168, 62, 174, 66, 'naht', 'naht1', 0.0, 3.0, flicker=2, label='Naht'))
for a, b in zip(naht, naht[1:]):
    m.opening(a, b, width=2)
for r in naht[1:]:
    R = m.R(r)
    m.light(r, R.cx, R.cy, 0, 0.55 if r != naht[-1] else 0.5, 3.6)
m.transition('l0_l1', 'seam', 'level1', 168.5, 64.0, math.pi, rect=[171, 62, 174, 66])

# --- Verbindungen zwischen den Bereichen ---------------------------------------------------------------------
link_regions(m, westB, nord, 2)
link_regions(m, westB, [sf], 2, width=(2, 3))
link_regions(m, westB, [boegen[0]], 1, width=(2, 2))
link_regions(m, westB, sued, 2)
link_regions(m, nord, [sf], 3, width=(2, 3))
link_regions(m, boegen, sued, 2)
link_regions(m, nord, dunkel, 1)
link_regions(m, [sf], ost, 2, width=(2, 3))
link_regions(m, sued, ost, 2)
link_regions(m, ost, dunkel, 2)
link_regions(m, ost, rot, 1)
link_regions(m, ost, [naht[0]], 1, width=(2, 2))

# --- Start: ein mittlerer Abschnitt im Westen -------------------------------------------------------------------
start = min((r for r in westA if m.R(r).type == 'zimmer' and m.R(r).w >= 5 and m.R(r).h >= 5),
            key=lambda r: math.hypot(m.R(r).cx - 14, m.R(r).cy - 70))
S = m.R(start)
m.start = (S.cx, S.cy, 0.0)

# --- Ausstattung: Steckdosen und Lueftungsgitter nur an echten Wandflaechen ----------------------------------
yellow = westA + westB + nord + sued + ost
for r in yellow:
    R = m.R(r)
    if R.type == 'manila':
        continue
    faces = m.wall_faces(r, margin=1)
    if not faces or rng.random() > 0.5:
        continue
    fx, fy, fyaw = rng.choice(faces)
    m.obj('outlet', fx, fy, R.floor + 0.32, fyaw, tag='deco')
    if R.height >= 2.85 and rng.random() < 0.35:
        vx, vy, vyaw = rng.choice(faces)
        if math.hypot(vx - fx, vy - fy) > 2.0:
            m.obj('vent_grille', vx, vy, R.floor + R.height - 0.45, vyaw, tag='deco')
for r in dunkel:
    R = m.R(r)
    for k in range(1 + int(R.w * R.h / 40)):
        m.spawn('deco', R.x0 + 1 + rng.random() * (R.w - 2), R.y0 + 1 + rng.random() * (R.h - 2), 0.0,
                rng.random() * 6.28, set='puddle', weight=0.8)


# --- Funde: wenige, an der Wand abgestellt, wo man rastet (Boegen, Manila, Sackgassen, Dunkelzone) ----------
def wall_spot(r, inward=0.35):
    faces = m.wall_faces(r, margin=1)
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


doors_of = {}
for d in m.doors:
    doors_of.setdefault(d['a'], []).append(d)
    doors_of.setdefault(d['b'], []).append(d)
dead_ends = [r for r in yellow if len(doors_of.get(r, [])) == 1 and m.R(r).type != 'flur']
spots = [(r, 'bogen') for r in boegen] + [(manila, 'manila')] + [(r, 'sackgasse') for r in rng.sample(dead_ends, min(14, len(dead_ends)))]
spots += [(r, 'dunkel') for r in rng.sample(dunkel, 3)]
for r, why in spots:
    p = wall_spot(r)
    if p:
        m.spawn('item', p[0], p[1], m.R(r).floor, p[2] + math.pi, tags=['floor', 'wall', why], group=r)
for r in rng.sample([r for r in yellow + boegen if m.R(r).type != 'flur'], 10):
    p = wall_spot(r, 0.5)
    if p:
        m.spawn('note', p[0], p[1], m.R(r).floor, p[2], tags=['floor'])
# die Gestalt am Ende langer Flure (greift nie an)
for r in [r for r in yellow if m.R(r).type == 'flur'][::3] + [flur]:
    R = m.R(r)
    end = (R.cx, R.y1 - 1.5) if R.h > R.w else (R.x1 - 1.5, R.cy)
    m.spawn('entity', end[0], end[1], R.floor, role='gestalt', tags=['corridor_end'])


# --- Sicherungspunkte (hier toetet nichts, aber der Verstand kann versagen) -----------------------------------
def safe_point(cid, ri, yaw, label, styled=True, at=None):
    R = m.R(ri)
    x, y = at if at else (R.cx, R.cy)
    m.checkpoint(cid, x, y, yaw, label, lit_by_style=styled)
    m.obj('safety_light', x, y, R.floor + R.height, yaw, tag='emergency')


safe_point('start', start, 0.0, 'Erster Raum')
safe_point('flur', flur, math.pi / 2, 'Der lange Flur', at=(29.5, 67.5))
safe_point('loch', min(nord, key=lambda r: math.hypot(m.R(r).cx - 63, m.R(r).cy - 21)), 0.0, 'Vor dem Lochfeld')
safe_point('saeulen', sf, 0.0, 'Säulenfeld')
safe_point('boegen', boegen[len(boegen) // 2], 0.0, 'Bögen')
safe_point('ost', min(ost, key=lambda r: math.hypot(m.R(r).cx - 140, m.R(r).cy - 64)), 0.0, 'Östliche Räume')
safe_point('naht', naht[1], 0.0, 'Wo die Tapete endet', styled=False)

m.save()
