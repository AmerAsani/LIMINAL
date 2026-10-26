"""Level 19 - "Der Dachboden": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.9).

Ein verwinkelter, feuchter Dachboden unter einem Satteldach: in der Mitte (First) hoch, zu den Traufen
hin stufenweise niedriger. Dielen, Sparren, rosa Daemmwolle, alte Moebel, Kisten. Licht kommt fast nur
als warmes, feuerartiges Gluehen durch Spalten zwischen den Dielen. Am Gluehen erholt sich der Verstand.

Ankunft: Sturz durch das Bodenloch aus Level 1 (mitten auf den Dachboden).
Ausgang: eine beschaedigte Tuer am Ostende fuehrt in das Hotel (Level 5).
Keine Entity; nur die eigene Wahrnehmung (eine Spieluhr, die es nicht gibt).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, partition_rooms  # noqa: E402

m = Map('level19', 104, 82, seed=1919)
rng = m.rng
RIDGE = 41.0  # First (y)


def attic_type(w, h, r):
    return 'dachkammer' if w * h < 40 else 'dachraum'


rooms = partition_rooms(m, 2, 2, 92, 80, 'dachboden', attic_type, min_leaf=3, max_leaf=11, height=2.6,
                        loop_chance=0.35, width=(1, 2))
# Satteldach: Hoehe nach dem Abstand zum First, in Stufen
for r in rooms:
    R = m.R(r)
    t = 1.0 - min(1.0, abs(R.cy - RIDGE) / 39.0)
    R.height = round(1.95 + 0.4 * round(t * 4.5), 2)

# --- Ausgang: Kammer am Ostende mit der beschaedigten Tuer ------------------------------------------------
east = min((r for r in rooms if m.R(r).x1 >= 91), key=lambda r: abs(m.R(r).cy - RIDGE))
ER = m.R(east)
door_room = m.room(93, int(ER.cy) - 2, 102, int(ER.cy) + 2, 'dachkammer', 'dachboden', 0.0, 2.4, label='Beschädigte Tür')
m.opening(east, door_room, width=2)
m.obj('damaged_door', 101.98, ER.cy, 0.0, math.pi, tag='door')
m.transition('l19_l5', 'door', 'level5', 98.5, ER.cy, math.pi, rect=[100, int(ER.cy) - 2, 102, int(ER.cy) + 2])

# --- Licht: Glut zwischen den Dielen ------------------------------------------------------------------------
glow_rooms = []
for r in rooms + [door_room]:
    R = m.R(r)
    if r != door_room and rng.random() > 0.5:
        continue
    glow_rooms.append(r)
    R.type = 'glutraum' if R.w * R.h >= 12 else R.type
    n = 2 + int(R.w * R.h / 25)
    for k in range(n):
        x = R.x0 + 0.8 + rng.random() * (R.w - 1.6)
        y = R.y0 + 0.8 + rng.random() * (R.h - 1.6)
        ln = 0.6 + rng.random() * 1.6
        m.obj('floor_glow', x, y, 0.001, 0.0, scale=[ln, 1.0, 1.0], variant=rng.randrange(2), tag='glow')
    m.light(r, R.cx, R.cy, 3, 0.9, 5.5, z=0.1, panel=False, color=(1.0, 0.58, 0.28))

# --- Sparren, Daemmwolle, Moebel ---------------------------------------------------------------------------
for r in rooms:
    R = m.R(r)
    top = R.height
    x = R.x0 + 0.6
    while x < R.x1 - 0.3:
        m.obj('rafter', x, R.cy, top - 0.005, math.pi / 2, scale=[R.h, 1.0, 1.0], tag='deco')
        if top < 2.6 and rng.random() < 0.6:
            m.obj('insulation', x + 0.6, R.cy, top - 0.005, math.pi / 2, scale=[R.h, 1.0, 1.0], variant=rng.randrange(4), tag='deco')
        x += 1.2
    roll = rng.random()
    on_trunk = False
    if R.w * R.h >= 12 and roll < 0.25:
        m.obj('wardrobe', R.x0 + 0.35, R.y0 + 1.2, 0.0, 0.0, variant=rng.randrange(2), tag='deco')
    elif roll < 0.45:
        m.obj('chair', R.cx + rng.random() - 0.5, R.cy + rng.random() - 0.5, 0.0, rng.random() * 6.28, variant=rng.randrange(2), tag='deco')
    elif roll < 0.6:
        m.obj('trunk', R.x1 - 0.6, R.cy, 0.0, math.pi / 2, tag='deco')
        on_trunk = True
    if rng.random() < 0.3:
        m.spawn('crate', R.x0 + 1.0 + rng.random() * max(0.1, R.w - 2.0), R.y0 + 1.0 + rng.random() * max(0.1, R.h - 2.0), 0.0,
                tags=['attic'])
    if rng.random() < 0.3:
        if on_trunk:  # V11: auf dem Deckel der Truhe
            m.spawn('item', R.x1 - 0.6, R.cy, 0.66, tags=['surface', 'trunk'], group=900 + r)
        else:
            m.spawn('item', R.x0 + 0.5, R.y1 - 0.5, 0.0, tags=['floor', 'corner'], group=900 + r)
for r in rng.sample(rooms, 8):
    R = m.R(r)
    m.spawn('note', R.cx + 0.3, R.cy, 0.0, tags=['floor'])

# --- Ankunft aus dem Bodenloch (Level 1): mitten auf dem Dachboden, an einer Glut ----------------------
arrive = min(glow_rooms, key=lambda r: math.hypot(m.R(r).cx - 40, m.R(r).cy - RIDGE))
AR = m.R(arrive)
m.transition('l1_l19', 'hole', '', AR.cx, AR.cy, 0.0)

# --- Sicherungspunkte (an der Glut; nichts toetet hier, aber der Verstand) ---------------------------------
def safe_point(cid, r, label):
    R = m.R(r)
    m.checkpoint(cid, R.cx + 0.5, R.cy, 0.0, label)


safe_point('ankunft', arrive, 'Unter dem Loch')
mid = min(glow_rooms, key=lambda r: math.hypot(m.R(r).cx - 70, m.R(r).cy - RIDGE))
safe_point('mitte', mid, 'Am Glühen')
safe_point('tuer', door_room, 'Vor der beschädigten Tür')

m.start = (AR.cx, AR.cy, 0.0)
m.save()
