"""Level 5 - "Das Hotel": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.6).

    Nord:  Wartungsflur | Zimmerreihe 2 | Flur B | Zimmerreihe 1 | Flur A
    Mitte: Restaurant | Ballsaal (Spieltisch, Kronleuchter) | Haupthalle (Marmor, Gold) | Lounge | Kesselraum -> Treppe ins Dunkel (Level 6)
    Sued:  Flur C | Zimmerreihe 3 | Flur D | Zimmerreihe 4 | Wartungsflur
    Suedwest: Treppenhaus hinauf zum Buero (Level 4).  Nordost: Abstellkammer mit der beschaedigten Tuer (Level 19).

Prunk, der nicht troestet: Jazz aus alten Lautsprechern, Partymurmeln hinter Waenden, Portraets, Moebel,
die unverrueckbar im Flur stehen. Viele Zimmertueren sind verschlossen; hinter offenen, dunklen Zimmern
haben Motten ihre Nester. Der Kesselraum ist heiss (Ausdauer) und fuehrt tiefer.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level5', 172, 128, seed=5005)
rng = m.rng
H = 3.3

# --- Mitte -----------------------------------------------------------------------------------------------------
lobby = m.room(62, 50, 102, 78, 'haupthalle', 'halle', 0.0, 6.0, label='Haupthalle')
ball = m.room(30, 50, 61, 78, 'ballsaal', 'ballsaal', 0.0, 6.5, label='Ballsaal')
rest = m.room(8, 50, 29, 78, 'restaurant', 'zimmer', 0.0, H, label='Restaurant')
lounge = m.room(103, 50, 135, 78, 'lounge', 'zimmer', 0.0, 4.0, label='Lounge')
kessel = m.room(136, 46, 160, 83, 'kesselraum', 'kessel', 0.0, 5.0, label='Kesselraum')
treppe6 = m.room(161, 50, 170, 66, 'treppe', 'kessel', -3.6, 7.2)
dunkel = m.room(161, 67, 170, 80, 'treppenhaus', 'dunkel', -3.6, 3.0, label='Hinab')
m.opening(lobby, ball, width=6)
m.opening(ball, rest, width=3)
m.opening(lobby, lounge, width=6)
m.door(lounge, kessel, 'door', 2, style='steel')
m.door(kessel, treppe6, 'opening', 3)
m.door(treppe6, dunkel, 'opening', 3)
m.transition('l5_l6', 'stairs', 'level6', 165.5, 72.0, -math.pi / 2, rect=[162, 76, 170, 80])

# --- Flure und Zimmerreihen --------------------------------------------------------------------------------------
fA = m.room(30, 79, 135, 83, 'flur5', 'flur', 0.0, H, label='Flur A')
fB = m.room(30, 91, 135, 95, 'flur5', 'flur', 0.0, H, label='Flur B')
fC = m.room(30, 45, 135, 49, 'flur5', 'flur', 0.0, H, label='Flur C')
fD = m.room(30, 33, 135, 37, 'flur5', 'flur', 0.0, H, label='Flur D')
wN = m.room(30, 104, 135, 106, 'wartung', 'wartung', 0.0, 2.6, label='Wartungsflur')
wS = m.room(30, 22, 135, 24, 'wartung', 'wartung', 0.0, 2.6)
for a, b, w in ((fA, lobby, 5), (fA, ball, 3), (fA, lounge, 3), (fC, lobby, 5), (fC, ball, 3), (fC, lounge, 3),
                (fA, kessel, 2), (fC, kessel, 2)):
    m.opening(a, b, width=w)

rooms = []  # (Raum, Flur, Nummer, offen)


def row(y0, y1, corridor, other, base, door_side):
    for k in range(15):
        x0 = 30 + 7 * k
        r = m.room(x0, y0, x0 + 6, y1, 'hotelzimmer', 'zimmer', 0.0, H)
        num = base + k + 1
        is_open = rng.random() < 0.45
        R = m.R(r)
        if is_open:
            m.door(r, corridor, 'door', 1, at=x0 + 3, style='wood_brown')
        else:
            R.kw['sealed'] = True
        # Tuer (Ausstattung) mit Nummer an der Flurwand - bei offenen Zimmern daneben das Schild
        fy = (y0 - 1.0 + 0.005) if door_side < 0 else (y1 + 1.0 - 0.005)
        yaw = -math.pi / 2 if door_side < 0 else math.pi / 2
        if not is_open:
            m.obj('hotel_door', x0 + 3.0, fy, 0.0, yaw, variant=num, tag='door')
        rooms.append((r, corridor, num, is_open))


row(84, 90, fA, fB, 100, -1)    # Reihe 1: Tueren nach Sueden (Flur A)
row(96, 103, fB, wN, 200, -1)   # Reihe 2: Tueren nach Sueden (Flur B)
row(38, 44, fC, fD, 300, 1)     # Reihe 3: Tueren nach Norden (Flur C)
row(25, 32, fD, wS, 400, 1)     # Reihe 4: Tueren nach Norden (Flur D)
# Flure untereinander und mit der Wartung verbinden (Durchgaenge am Rand)
west_link_n = m.room(24, 79, 29, 106, 'flur5', 'flur', 0.0, H)
west_link_s = m.room(24, 22, 29, 49, 'flur5', 'flur', 0.0, H)
for c in (fA, fB, wN):
    m.opening(west_link_n, c, width=2)
for c in (fC, fD, wS):
    m.opening(west_link_s, c, width=2)
m.opening(rest, west_link_n, width=2)
m.opening(rest, west_link_s, width=2)

# --- Treppenhaus hinauf (Level 4) im Suedwesten, beschaedigte Tuer (Level 19) im Nordosten ---------------------
halle_sw = m.room(8, 22, 23, 49, 'treppenhaus', 'zimmer', 0.0, H, label='Treppenhaus')
m.opening(halle_sw, west_link_s, width=2)
m.opening(halle_sw, rest, width=2)
treppe4 = m.room(8, 10, 20, 21, 'treppe', 'zimmer', 0.0, 6.4)
oben4 = m.room(8, 2, 20, 9, 'treppenhaus', 'zimmer', 3.2, 3.2)
m.door(halle_sw, treppe4, 'opening', 4)
m.door(treppe4, oben4, 'opening', 4)
m.transition('l4_l5', 'stairs', 'level4', 14.0, 30.0, math.pi / 2, rect=[9, 3, 19, 6])

kammer19 = m.room(136, 86, 150, 96, 'abstellkammer', 'wartung', 0.0, 2.8, label='Abstellkammer')
m.opening(fB, kammer19, width=2)
m.obj('damaged_door', 149.98, 91.0, 0.0, math.pi, tag='door')
m.transition('l19_l5', 'door', 'level19', 144.0, 91.0, math.pi, rect=[148, 89, 150, 93])

# --- Licht ---------------------------------------------------------------------------------------------------------
# V11: Hotel der 1920/30er (Wikidot "Level 5 - Terror Hotel", Rev. 184): Kronleuchter, Wandleuchter wie alte
# Kerzenhalter, Deckenkugeln - keine Rasterleuchten. Punktlichter sitzen in den Leuchten (panel=False), die
# Leuchte selbst ist Ausstattung. Wartungsgaenge bekommen Roehren (Zone "wartung", fixture "tube").
def globe(r, x, y, inten=0.7, radius=5.0):
    R = m.R(r)
    top = R.floor + R.height
    m.obj('ceiling_globe', x, y, top, 0.0, tag='light')
    m.light(r, x, y, 0, inten, radius, z=top - 0.5, panel=False, color=(1.0, 0.84, 0.62))



for x, y, big in ((72, 64, 1), (92, 64, 1), (45.5, 64, 1)):
    r = m.at(x, y)
    top = m.R(r).height
    m.obj('chandelier', x, y, top - 0.01, 0.0, variant=big, tag='light')
    m.light(r, x, y, 0, 1.3, 9.0, z=top - 1.1, panel=False, color=(1.0, 0.82, 0.58))
for c in (fA, fB, fC, fD):
    R = m.R(c)
    for x in range(int(R.x0) + 3, int(R.x1) - 2, 6):
        if not m.wall_ok(x + 0.5, R.y1 - 0.005, -math.pi / 2):  # V11: keine Leuchte vor einer Tuer
            continue
        m.obj('wall_sconce', x + 0.5, R.y1 - 0.005, 1.85, -math.pi / 2, tag='light')
        m.light(c, x + 0.5, R.y1 - 0.3, 0, 0.55, 4.0, z=1.95, panel=False, color=(1.0, 0.82, 0.55))
for r in (rest, lounge):
    R = m.R(r)
    for x in range(int(R.x0) + 3, int(R.x1) - 2, 7):
        for y in range(int(R.y0) + 3, int(R.y1) - 2, 7):
            globe(r, x + 0.5, y + 0.5)
for r in (west_link_n, west_link_s, halle_sw):
    R = m.R(r)
    if R.w >= R.h:
        for x in range(int(R.x0) + 2, int(R.x1) - 1, 7):
            globe(r, x + 0.5, R.cy, 0.5, 4.0)
    else:
        for y in range(int(R.y0) + 2, int(R.y1) - 1, 7):
            globe(r, R.cx, y + 0.5, 0.5, 4.0)
for r in (wN, wS, kammer19):
    R = m.R(r)
    step = 7
    if R.w >= R.h:
        for x in range(int(R.x0) + 2, int(R.x1) - 1, step):
            m.light(r, x + 0.5, R.cy, 0, 0.45, 3.5, color=(1.0, 0.9, 0.7))
    else:
        for y in range(int(R.y0) + 2, int(R.y1) - 1, step):
            m.light(r, R.cx, y + 0.5, 0, 0.45, 3.5, color=(1.0, 0.9, 0.7))
for r in (treppe4, oben4):
    globe(r, 14.0, m.R(r).cy, 0.6, 5.0)
KR = m.R(kessel)
for x, y in ((142, 54), (154, 54), (142, 74), (154, 74)):
    m.light(kessel, x + 0.5, y + 0.5, 0, 1.3, 11.0, z=4.0, panel=False, color=(1.0, 0.62, 0.3))
m.light(treppe6, 165.5, 56.0, 0, 0.35, 4.0, z=m.R(treppe6).floor + 3.0, panel=False, color=(1.0, 0.6, 0.3))
m.light(dunkel, 165.5, 69.0, 0, 0.2, 3.0, z=m.R(dunkel).floor + 2.4, panel=False, color=(1.0, 0.6, 0.3))

# --- Ausstattung ---------------------------------------------------------------------------------------------------
# Haupthalle: Sofagruppen, Lautsprecher
for x, y in ((70, 56), (94, 56), (70, 72), (94, 72)):
    m.obj('sofa', x, y, 0.0, 0.0 if x < 80 else math.pi, variant=rng.randrange(2), tag='deco')
    m.obj('side_table', x + (1.4 if x < 80 else -1.4), y, 0.0, 0.0, variant=1, tag='deco')
    m.block(lobby, int(x) - 1, int(y) - 1, int(x) + 1, int(y) + 2, 0.9)
for x in (66, 98):
    m.obj('speaker', x, 77.99, 4.2, -math.pi / 2, tag='music')
# Ballsaal: Spieltisch in der Mitte
m.obj('game_table', 45.5, 64.0, 0.0, 0.0, tag='deco')
m.block(ball, 44, 62, 48, 66, 0.8)
# Restaurant: Tische und Stuehle
for x in range(11, 27, 5):
    for y in range(53, 76, 5):
        m.obj('side_table', x + 0.5, y + 0.5, 0.0, 0.0, tag='deco')
        m.obj('chair', x + 1.3, y + 0.5, 0.0, math.pi, tag='deco')
        m.obj('chair', x - 0.3, y + 0.5, 0.0, 0.0, variant=1 if rng.random() < 0.1 else 0, tag='deco')
# Lounge: Sessel, Portraets
for k in range(6):
    m.obj('armchair', 106 + k * 5, 53, 0.0, math.pi / 2, variant=rng.randrange(2), tag='deco')
for x in range(108, 133, 6):
    m.obj('painting', x, 77.95, 1.7, -math.pi / 2, variant=rng.randrange(8), tag='story')
# Flure: Portraets, Lautsprecher, unverrueckbare Moebel
for c in (fA, fB, fC, fD):
    R = m.R(c)
    for x in range(int(R.x0) + 6, int(R.x1) - 3, 12):
        if m.wall_ok(x + 0.5, R.y0 + 0.005, math.pi / 2):  # V11: Bilder nur an Wand, nicht auf Tueren
            m.obj('painting', x + 0.5, R.y0 + 0.005, 1.65, math.pi / 2, variant=rng.randrange(8), tag='story')
    for x in range(int(R.x0) + 10, int(R.x1) - 3, 26):
        if m.wall_ok(x + 0.5, R.y0 + 0.005, math.pi / 2, 2.7):
            m.obj('speaker', x + 0.5, R.y0 + 0.005, 2.7, math.pi / 2, tag='music')
    for k in range(2):
        x = int(R.x0) + 8 + rng.randrange(int(R.w) - 16)
        m.obj('armchair', x + 0.5, R.cy, 0.0, rng.random() * 6.28, variant=rng.randrange(2), tag='deco')
        m.block(c, x, int(R.cy), x + 1, int(R.cy) + 1, 1.0)
# Zimmer: offene Zimmer moebliert, manche dunkel (Nester); verschlossene leer
for r, corridor, num, is_open in rooms:
    if not is_open:
        continue
    R = m.R(r)
    dark = rng.random() < 0.45
    if dark:
        R.kw['light'] = 'unlit'
        R.type = 'dunkles_zimmer'
    else:
        globe(r, R.cx, R.cy, 0.5, 3.8)
    m.obj('bed', R.x0 + 1.6, R.cy, 0.0, 0.0, tag='deco')
    m.obj('side_table', R.x0 + 0.5, R.y1 - 0.6, 0.0, 0.0, variant=1, tag='deco')
    m.obj('painting', R.x1 - 0.05, R.cy, 1.6, math.pi, variant=rng.randrange(8), tag='story')
    if rng.random() < 0.5:
        m.obj('armchair', R.x1 - 1.0, R.y0 + 1.0, 0.0, math.pi * 0.75, variant=rng.randrange(2), tag='deco')
    # V11: auf dem Nachttisch (Platte 0,66 m), nicht mitten auf dem Teppich
    m.spawn('item', R.x0 + 0.5, R.y1 - 0.6, R.floor + 0.66, tags=['surface', 'hotel'], group=num)
    if rng.random() < 0.3:
        m.spawn('note', R.cx - 0.3, R.cy, 0.0, tags=['floor'])
# Kesselraum: Kessel, Dampf
for x, y in ((143, 62), (153, 70)):
    m.obj('boiler', x, y, 0.0, 0.0, tag='machine')
    m.block(kessel, x - 2, y - 1, x + 2, y + 1, 2.0)
    m.obj('pipe_run', x, y + 1.5, 3.6, 0.0, variant=1, tag='steam')
# V11: Haupthalle stark moebliert (Wikidot: Buecherregale, Schraenke, Sofas, alte Gemaelde) - an den Laengswaenden
# Buecherregale und Schraenke, in den Regalen und auf den Tischchen der Sitzgruppen liegt mitunter etwas
LR = m.R(lobby)
k = 0
for x in range(int(LR.x0) + 2, int(LR.x1) - 2, 3):
    for wall_y, yaw in ((LR.y0 + 0.01, math.pi / 2), (LR.y1 - 0.01, -math.pi / 2)):
        if not m.wall_ok(x + 0.5, wall_y, yaw):
            continue
        k += 1
        if k % 4 == 0:
            m.obj('wardrobe', x + 0.5, wall_y + (0.3 if yaw > 0 else -0.3), 0.0, yaw, tag='deco')
        elif k % 4 != 2:
            m.obj('bookcase', x + 0.5, wall_y, 0.0, yaw, variant=k % 4, tag='deco')
            if k % 3 == 0:
                m.spawn('item', x + 0.5, wall_y + (0.2 if yaw > 0 else -0.2), 0.86, tags=['surface', 'bookcase'],
                        group=1000 + k)
        else:
            m.obj('painting', x + 0.5, wall_y + (0.005 if yaw > 0 else -0.005), 2.0, yaw, variant=rng.randrange(8), tag='story')
for x, y in ((70, 56), (94, 56), (70, 72), (94, 72)):
    m.spawn('item', x + (1.4 if x < 80 else -1.4), y, 0.66, tags=['surface', 'side_table'], group=1100 + x + y)
# Wandleuchter zwischen den Regalen (die Halle ist sonst nur unter den Kronleuchtern hell), weitere Sitzgruppen
for x in range(int(LR.x0) + 4, int(LR.x1) - 3, 6):
    for wall_y, yaw in ((LR.y0 + 0.005, math.pi / 2), (LR.y1 - 0.005, -math.pi / 2)):
        if m.wall_ok(x + 0.0, wall_y, yaw):
            m.obj('wall_sconce', x + 0.0, wall_y, 2.2, yaw, tag='light')
            m.light(lobby, x + 0.0, wall_y + (0.3 if yaw > 0 else -0.3), 0, 0.5, 4.5, z=2.3, panel=False, color=(1.0, 0.82, 0.55))
for x, y, yaw in ((78, 58, 0.0), (86, 58, math.pi), (78, 70, 0.0), (86, 70, math.pi)):
    m.obj('armchair', x, y, 0.0, yaw + math.pi / 2, variant=1, tag='deco')
    m.obj('armchair', x, y + 2.0, 0.0, yaw - math.pi / 2, variant=0, tag='deco')
    m.obj('coffee_table', x, y + 1.0, 0.0, math.pi / 2, tag='deco')
    m.block(lobby, int(x) - 1, int(y), int(x) + 1, int(y) + 3, 0.9)

# --- Sicherungspunkte -------------------------------------------------------------------------------------------------
def safe_point(cid, x, y, yaw, label):
    m.checkpoint(cid, x, y, yaw, label)
    ri = m.at(x, y)
    m.obj('safety_light', x, y, m.R(ri).floor + m.R(ri).height - 0.01, yaw, tag='emergency')


safe_point('treppenhaus', 15.0, 34.0, math.pi / 2, 'Treppenhaus')
safe_point('halle', 82.0, 60.0, 0.0, 'Haupthalle')
safe_point('flur_a', 120.0, 81.0, math.pi, 'Flur A')
safe_point('flur_d', 60.0, 35.0, 0.0, 'Flur D')
safe_point('kammer', 140.0, 91.0, 0.0, 'Abstellkammer')
safe_point('kessel', 139.0, 60.0, 0.0, 'Kesselraum')
safe_point('hinab', 165.5, 69.0, -math.pi / 2, 'Hinab')

m.start = (15.0, 34.0, math.pi / 2)
m.save()
