"""Level 1 - "Das Lagerdeck": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 4).

Gestalt (Draufsicht, Norden oben):

    +--------------------------------------------------------------------------------+
    |                Gang-Labyrinth: enge Betongaenge mit Kammern                     |
    +---------+---------------------------+------+--------------------+-----+--------+
    |         |                           |Rampe |                    |Rampe|        |
    | Wartung |  Deck A (blau, +-0)       |      |  Deck B (gelb,-1,6)|     | Deck C |
    | (weiss) |                           |      |                    |     |(rot,   |
    |         +---------------------------+      +---------+----------+     | -3,2)  |
    |         |  Lagerboxen               |      |Lagerh. 1| Lagerh. 2|     |        |
    +---------+---------------------------+------+---------+----------+     +--+-----+
      Lastenflur (Ankunft aus Level 0, West)                                    Rohrgang
                                                                              (Level 2, Sued)

Raumfolge fuer den Spieler: Lastenflur (gelbe Tapete weicht Beton) -> Deck A (weit, hell, Lichtinseln
ueber den Fahrgassen) -> Rampe hinab -> Deck B -> Lagerhallen / Labyrinth / das Bodenloch -> Rampe
-> Deck C (am dunkelsten) -> Rohrgang nach Level 2.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level1', 256, 236, seed=1101)
rng = m.rng

DECK_H = 4.4
GRID = 8          # Stuetzenraster (m)
BEAM = 0.55       # Unterzug
BAND = {'deck_a': 'A', 'deck_b': 'B', 'deck_c': 'C'}

# --- Hilfen ---------------------------------------------------------------------------------------


def deck(x0, y0, x1, y1, zone, floor, circuit, label):
    """Ein Parkdeck: Stuetzen alle 8 m, Unterzuege darueber, Fahrgassen in jeder zweiten Bucht.

    Gibt den Raumindex und die Mittellinien der Fahrgassen (y) zurueck.
    """
    ox, oy = x0 + 4, y0 + 5
    r = m.room(x0, y0, x1, y1, 'deck', zone, floor, DECK_H, pillars=[GRID, GRID, 1, ox, oy], beams=BEAM,
               light='battens', circuit=circuit, label=label, tags=['deck'])
    # Buchten zwischen Stuetzenreihen: abwechselnd Stellplaetze und Fahrgasse
    aisles = []
    y = oy + 1
    k = 0
    while y + GRID - 1 <= y1 - 1:
        if k % 2 == 0:
            aisles.append(y + (GRID - 1) / 2)
        y += GRID
        k += 1
    # Leuchtbaender ueber den Fahrgassen, zwischen den Stuetzen; einige tot oder fehlend
    ceil = floor + DECK_H - 0.01
    for ay in aisles:
        x = ox + 1 + (GRID - 1) / 2
        while x < x1 - 2:
            alive = rng.random() > 0.12
            if alive:
                m.light(r, x, ay, 0, 1.0, 6.5, z=ceil - 0.42, panel=False)
            m.obj('batten', x, ay, ceil, 0.0, variant=0 if alive else 1, tag='light')
            x += GRID
    # Stellplatzlinien beidseits der Fahrgassen (je 3 Plaetze zwischen zwei Stuetzen)
    for ay in aisles:
        for side in (-1, 1):
            yl = ay + side * (GRID / 2 + 2.5)  # Mitte der Stellplatzreihe
            x = ox + 1
            while x + GRID <= x1:
                for s in range(4):
                    m.obj('stall_line', x + s * (GRID - 1) / 3.0, yl, floor + 0.005, math.pi / 2, tag='paint')
                x += GRID
        # Fahrpfeile in der Gasse
        for x in range(ox + 6, x1 - 6, 24):
            m.obj('arrow', x, ay, floor + 0.005, 0.0 if rng.random() < 0.7 else math.pi, tag='paint')
    # Stuetzennummern (Abschnitt + Spalte/Reihe), eine Plakette je Stuetze
    num = 1
    letter = ord(BAND[zone]) - ord('A')
    x = ox
    while x < x1 - 1:
        y = oy
        while y < y1 - 1:
            if x0 + 1 <= x < x1 - 1 and y0 + 1 <= y < y1 - 1:
                # V11: an Durchgaengen raeumt die Engine Stuetzen weg (Kopffreiheit) - dort keine Plakette
                near = any(d['rect'][0] - 3.5 <= x + 0.5 <= d['rect'][2] + 3.5 and d['rect'][1] - 3.5 <= y + 0.5 <= d['rect'][3] + 3.5
                           for d in m.doors)
                if not near:
                    m.obj('pillar_label', x + 0.5, y + 0.5, floor + 1.75, 0.0, variant=letter * 256 + num, tag=BAND[zone])
                num += 1
            y += GRID
        x += GRID
    return r, aisles


def ramp(x0, y0, x1, y1, zone, floor_low, circuit, label):
    r = m.room(x0, y0, x1, y1, 'rampe', zone, floor_low, DECK_H + 1.6, light='battens', circuit=circuit, label=label,
               tags=['ramp'])
    cx = (y0 + y1) / 2
    top = floor_low + DECK_H + 1.6 - 0.01
    for x in range(x0 + 3, x1 - 1, 6):
        m.light(r, x + 0.5, cx, 0, 0.9, 5.5, z=top - 0.42, panel=False)
        m.obj('batten', x + 0.5, cx, top, 0.0, tag='light')
    return r


def exit_sign(x, y, z, yaw):
    """Gruenes Notausgangsschild (eigene Batterie: leuchtet auch im Stromausfall)."""
    m.obj('exit_sign', x, y, z, yaw, tag='emergency')


# --- Ankunft: Lastenflur aus Level 0 (West) ---------------------------------------------------------
la = m.room(2, 112, 13, 116, 'lastenflur', 'naht0', 0.0, 2.8, label='Lastenflur', tags=['seam'])
lb = m.room(14, 112, 24, 116, 'lastenflur', 'naht0', 0.0, 2.9, ambient=0.08, tags=['seam'])
lc = m.room(25, 112, 35, 116, 'gang', 'gang', 0.0, 3.0, tags=['seam'])
m.opening(la, lb)
m.opening(lb, lc)
for r_, xs in ((la, (4, 9)), (lb, (17, 22)), (lc, (28, 32))):
    for x in xs:
        m.light(r_, x + 0.5, 114.0, 0, 0.7 if r_ != lc else 0.55, 3.8)
m.start = (3.5, 114.0, 0.0)
m.transition('l0_l1', 'seam', 'level0', 5.0, 114.0, 0.0, rect=[2, 112, 4, 116])

# --- Deck A (blau) -----------------------------------------------------------------------------------
deckA, aislesA = deck(36, 80, 100, 141, 'deck_a', 0.0, 1, 'Deck A')
m.door(lc, deckA, 'wide', 4, top=2.4)  # V11: mit Sturz, darauf das Notausgangsschild
exit_sign(36.0, 114.0, 2.55, 0.0)

# --- Rampe A -> B und Deck B (gelb) -------------------------------------------------------------------
# Rampen mit eigenem Stromkreis: faellt in einem Deck der Strom aus, bleiben sie hell (Fluchtweg ins Licht)
rampAB = ramp(101, 108, 119, 115, 'deck_a', -1.6, 10, 'Rampe')
m.opening(deckA, rampAB)
deckB, aislesB = deck(120, 64, 180, 141, 'deck_b', -1.6, 2, 'Deck B')
m.opening(rampAB, deckB)

# --- Rampe B -> C und Deck C (rot, am dunkelsten) -----------------------------------------------------
rampBC = ramp(181, 96, 199, 103, 'deck_b', -3.2, 11, 'Rampe')
m.opening(deckB, rampBC)
deckC, aislesC = deck(200, 24, 252, 141, 'deck_c', -3.2, 3, 'Deck C')
m.opening(rampBC, deckC)

# --- Lagerboxen unter Deck A --------------------------------------------------------------------------
boxen = m.room(40, 75, 96, 78, 'boxenreihe', 'boxen', 0.0, 2.6, circuit=8, label='Lagerboxen', tags=['lockers'])
m.door(deckA, boxen, 'door', 1, at=44, style='steel')
m.door(deckA, boxen, 'door', 1, at=92, style='steel')
for x in range(44, 96, 14):
    m.light(boxen, x + 0.5, 76.5, 0, 0.45, 3.2)
for x in range(41, 95, 2):
    m.obj('locker_shutter', x + 1.0, 75.02, 0.0, math.pi / 2, variant=rng.randrange(4), tag='locker')
    v = rng.randrange(4)  # (Zufallsfolge unveraendert lassen: das Labyrinth haengt daran)
    if m.wall_ok(x + 1.0, 77.98, -math.pi / 2, 1.0):  # V11: nicht vor die Stahltueren ins Deck
        m.obj('locker_shutter', x + 1.0, 77.98, 0.0, -math.pi / 2, variant=v, tag='locker')
m.spawn('entity', 60.5, 76.5, role='hound', tags=['locker', 'dark'])
m.spawn('entity', 84.5, 76.5, role='hound', tags=['locker', 'dark'])

# --- Lagerhallen unter Deck B (hinter Holztueren, eigener Stromkreis) -----------------------------------
vorraeume = []
for i, (hx0, hx1, style) in enumerate(((122, 148, 'wood_pink'), (152, 178, 'wood_brown'))):
    vor = m.room(hx0 + 10, 58, hx0 + 16, 63, 'gang', 'gang', -1.6, 3.0, circuit=4, tags=['warehouse_entry'])
    vorraeume.append(vor)
    m.opening(deckB, vor)
    halle = m.room(hx0, 14, hx1, 56, 'lagerhalle', 'lager', -1.6, 8.6, circuit=5, label=f'Lagerhalle {i + 1}',
                   tags=['warehouse'])
    m.door(vor, halle, 'wide', 2, style=style)
    m.light(vor, hx0 + 13, 60.5, 0, 0.5, 3.5)
    # Hochregale: vier Doppelreihen (2 m tief) entlang y, je zwei Abschnitte mit Quergang in der Mitte;
    # Gassen 3 m. Der Block traegt Kollision, Schatten und Wege, das Modell zeigt Rahmen, Traversen, Ladung.
    RACK_X = (4, 9, 14, 19)
    for rx in RACK_X:
        for y0, y1 in ((20, 34), (36, 50)):
            m.block(halle, hx0 + rx, y0, hx0 + rx + 2, y1, 5.25)
            bays = int((y1 - y0) // 2.7)
            start = y0 + ((y1 - y0) - bays * 2.7) / 2
            for b in range(bays):
                v = rng.randrange(4) | (4 if b == bays - 1 else 0)
                m.obj('rack', hx0 + rx + 1.0, start + 1.35 + b * 2.7, -1.6, math.pi / 2, variant=v, tag='rack')
    # Hallenstrahler ueber den Gassen (die Gassen dazwischen bleiben dunkler)
    for ax in (7.5, 12.5, 17.5):
        for y in (17, 26, 35, 44, 53):
            m.light(halle, hx0 + ax, y + 0.5, 0, 1.2, 8.0, z=-1.6 + 8.6 - 1.0, panel=False)
            m.obj('highbay', hx0 + ax, y + 0.5, -1.6 + 8.6 - 0.01, 0.0, tag='light')
    # Gegenstaende in den Regalgassen, Kisten
    AISLES = (2.0, 7.5, 12.5, 17.5, 23.5)
    for k in range(10):
        m.spawn('item', hx0 + rng.choice(AISLES) + (rng.random() - 0.5), 16.5 + rng.random() * 37,
                tags=['floor', 'warehouse'], group=100 + i * 10 + k // 3)
    for k in range(4):
        m.spawn('crate', hx0 + rng.choice((2.0, 23.5)), 17 + rng.random() * 35, tags=['warehouse'])
    # Rolltore an der Rueckwand (Laderampe)
    for x in range(hx0 + 3, hx1 - 3, 6):
        m.obj('rollgate', x + 1.5, 14.02, -1.6, math.pi / 2, variant=1, tag='gate')

# --- Gang-Labyrinth im Norden (enge Betongaenge mit Kammern) ---------------------------------------------
KAMMER = {'buero': 'kammer', 'krankenzimmer': 'kranken', 'gummizelle': 'polster', 'bilderraum': 'kammer',
          'ziegelraum': 'ziegel', 'schlaflager': 'kammer', 'abstellraum': 'gang'}
J, S, C = corridor_maze(m, 36, 148, 196, 232, 12, 2, 'gang', 'gang',
                        ['buero', 'buero', 'krankenzimmer', 'gummizelle', 'bilderraum', 'ziegelraum', 'schlaflager',
                         'abstellraum', 'abstellraum'],
                        lambda t: KAMMER[t], height=2.7, floor=0.0, density=0.8, room_chance=0.75, circuit=6)
# Kammern: Licht in der Mitte (manche dunkel), Notizen und Vorraete
for (i, j), r in C.items():
    R = m.R(r)
    t = R.type
    if rng.random() < 0.8:
        m.light(r, R.cx, R.cy, 0, 0.7, 4.0)
    m.spawn('note', R.cx + 0.3, R.cy - 0.2, tags=['table', t])
    if t == 'buero':  # V11: auf dem Schreibtisch neben dem Monitor
        m.spawn('item', R.cx - 0.05, R.y1 - 0.7, 0.75, tags=['surface', 'desk', t], group=300 + i * 10 + j)
    else:
        m.spawn('item', R.x0 + 0.6, R.y0 + 0.6, tags=['floor', 'corner', t], group=300 + i * 10 + j)
    if t == 'schlaflager':
        m.obj('camp', R.cx, R.cy, 0.0, rng.random() * 6.28, tag='story')
    elif t == 'bilderraum':
        for k in range(3):
            m.obj('painting', R.x0 + 0.06, R.y0 + 1.5 + k * (R.h - 3) / 2, 1.5, 0.0, variant=rng.randrange(8), tag='story')
    elif t == 'buero':
        m.obj('desk_crt', R.cx, R.y1 - 1.2, 0.0, math.pi, variant=rng.randrange(3), tag='story')
    elif t == 'krankenzimmer':
        m.obj('bed', R.x0 + 1.6, R.cy, 0.0, 0.0, tag='story')
# Zugaenge vom Labyrinth zu den Decks (Gaenge durch die Wand, gruenes Schild darueber)
bottom = sorted([k for k, v in J.items() if v is not None and k[1] == 0])
links = 0
connectors = []
for (i, j) in bottom:
    jr = m.R(J[(i, j)])
    x = jr.x0
    for dk, dname in ((deckA, 'A'), (deckB, 'B')):
        D = m.R(dk)
        if D.x0 + 2 <= x and x + 2 <= D.x1 - 2 and links < 6:
            con = m.room(x, D.y1 + 1, x + 2, jr.y0 - 1, 'gang', 'gang', 0.0 if dname == 'A' else -1.6, 2.7, circuit=6)
            connectors.append((con, dname))
            m.door(dk, con, 'wide', 2, top=2.3)  # V11: Durchgang mit Sturz - das Schild sitzt darauf
            m.opening(con, J[(i, j)])
            exit_sign(x + 1.0, D.y1, D.floor + 2.5, -math.pi / 2)
            m.light(con, x + 1.0, (D.y1 + jr.y0) / 2, 0, 0.5, 3.2)
            links += 1
            break

# --- Wartungsflure im Suedwesten (weiss, eigener Strom, sicher im Stromausfall) -----------------------
JW, SW, CW = corridor_maze(m, 2, 8, 34, 98, 10, 2, 'wartungsflur', 'wartung', ['schaltraum', 'abstellraum'],
                           lambda t: 'wartung', height=2.6, floor=0.0, density=0.7, room_chance=0.6, circuit=7,
                           light_every=4)
for (i, j), r in CW.items():
    R = m.R(r)
    m.light(r, R.cx, R.cy, 0, 0.9, 4.0)
    m.spawn('item', R.cx, R.cy, tags=['floor', 'maint'], group=500 + i * 10 + j)
    m.obj('power_box', R.x1 - 0.1, R.cy, 1.2, math.pi, tag='machine')
# schwarze Metalltuer ins Deck A
east = sorted([k for k, v in JW.items() if v is not None], key=lambda k: (-k[0], abs(k[1] - 9)))
for k in east:
    jr = m.R(JW[k])
    if jr.y0 >= 81 and jr.y1 <= 140 and m._wall(JW[k], deckA):
        m.door(JW[k], deckA, 'door', 1, style='black')
        wartung_tuer = JW[k]
        break
else:
    raise SystemExit('Wartung ohne Zugang zu Deck A')

# --- Bodenloch zu Level 19 (Deck B, Nordrand): die Deckenplatte ist durchgebrochen ---------------------
# Ein unregelmaessiges Loch im Boden zwischen zwei Stuetzen, 7,4 m tief. Unten glimmt es warm (Level 19).
# Bruchkanten mit Bewehrung rundum, Brocken am Rand und unten, Absperrschranken (eine umgekippt).
HOLE_DEPTH = 7.4
HOLE = ({(x, y) for x in range(150, 155) for y in range(136, 139)} | {(x, 135) for x in range(151, 155)}
        | {(x, 139) for x in range(151, 154)})
for (x, y) in sorted(HOLE):
    m.block(deckB, x, y, x + 1, y + 1, -HOLE_DEPTH, hidden=False)
for (x, y) in sorted(HOLE):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if (x + dx, y + dy) not in HOLE:
            m.obj('slab_edge', x + 0.5 + dx * 0.5, y + 0.5 + dy * 0.5, -1.6, math.atan2(-dy, -dx),
                  variant=rng.randrange(4), tag='hole')
for k, (x, y) in enumerate(((149.2, 134.6), (155.9, 139.4), (152.8, 134.2), (149.4, 139.7))):
    m.obj('rubble', x, y, -1.6, rng.random() * 6.28, variant=k, tag='hole')
for k in range(3):
    m.obj('rubble', 151.2 + k * 1.4, 136.4 + (k % 2) * 1.5, -1.6 - HOLE_DEPTH, rng.random() * 6.28, variant=k + 1, tag='hole')
m.obj('barrier', 152.6, 133.7, -1.6, 0.0, tag='hole')
m.obj('barrier', 156.4, 137.2, -1.6, math.pi / 2 + 0.12, tag='hole')
m.obj('barrier', 148.7, 137.4, -1.6, math.pi / 2 - 0.5, variant=1, tag='hole')
m.light(deckB, 152.5, 137.3, 3, 6.0, 11.0, z=-1.6 - HOLE_DEPTH + 0.4, panel=False, color=(1.0, 0.42, 0.12))
m.obj('hole_glow', 152.5, 137.3, -1.6 - HOLE_DEPTH + 0.02, 0.0, tag='glow')
m.transition('l1_l19', 'hole', 'level19', 152.5, 137.5, 0.0, rect=[150, 135, 155, 140])


def free_pos(D, margin):
    """Zufaelliger Punkt im Raum D, nicht in einem Block (Loch, Hochregal) und nicht direkt am Loch."""
    while True:
        x = D.x0 + margin + rng.random() * (D.w - 2 * margin)
        y = D.y0 + margin + rng.random() * (D.h - 2 * margin)
        if not m.blocked(x, y) and not (147 <= x < 158 and 133 <= y < 141):
            return x, y

# --- Rohrgang nach Level 2 (Sued, aus Deck C): Rohre werden dichter ------------------------------------
r1 = m.room(226, 12, 230, 23, 'rohrgang', 'naht2', -3.2, 3.0, circuit=9, tags=['seam'])
m.opening(deckC, r1)
r2 = m.room(226, 2, 230, 11, 'rohrgang', 'naht2', -3.2, 2.7, circuit=9, tags=['seam'])
m.opening(r1, r2)
for y in (20, 15):
    m.light(r1, 228.0, y + 0.5, 0, 0.55, 3.5)
m.light(r2, 228.0, 7.5, 0, 0.45, 3.2)
for y in range(3, 23, 1):
    m.obj('pipe_run', 226.3, y + 0.5, -3.2 + 2.3, math.pi / 2, variant=0, tag='pipes')
    if y < 16:
        m.obj('pipe_run', 229.7, y + 0.5, -3.2 + 2.0, math.pi / 2, variant=1, tag='pipes')
m.transition('l1_l2', 'seam', 'level2', 228.0, 6.5, math.pi / 2, rect=[226, 2, 230, 5])

# --- Belegung der Decks ---------------------------------------------------------------------------------
for dk, aisles in ((deckA, aislesA), (deckB, aislesB), (deckC, aislesC)):
    D = m.R(dk)
    for ay in aisles:
        for side in (-1, 1):
            yl = ay + side * (GRID / 2 + 2.5)
            if not (D.y0 + 1 < yl < D.y1 - 1):
                continue
            x = D.x0 + 6
            while x < D.x1 - 4:
                # abgestellte Wagen (Dekoration, je Lauf), Pfuetzen, Oelflecken
                m.spawn('deco', x + 0.3, yl, D.floor, math.pi / 2 if side > 0 else -math.pi / 2, set='car', weight=0.35)
                if rng.random() < 0.3:
                    m.spawn('deco', x + 1.5 + rng.random() * 3, ay + (rng.random() - 0.5) * 4, D.floor, rng.random() * 6.28,
                            set='puddle')
                x += GRID * 1.0
    # Gefahren: das Laecheln in den Stellplatzreihen (nur im Stromausfall), Vorraete an Stuetzen
    for k in range(6):
        m.spawn('entity', *free_pos(D, 4), D.floor, role='smiler', tags=['dark'])
    for k in range(5):
        m.spawn('item', *free_pos(D, 3), D.floor, tags=['floor', 'deck'], group=700 + dk * 10 + k)
    for k in range(3):
        m.spawn('crate', *free_pos(D, 4), D.floor, tags=['deck'])
    m.spawn('entity', D.x0 + 2.5, D.y1 - 2.5, D.floor, role='hound', tags=['corner'])
# Buendel: selten, in den Gaengen des Labyrinths
for k, r in list(S.items())[::9]:
    R = m.R(r)
    m.spawn('entity', R.cx, R.cy, 0.0, role='clump', tags=['vent'])

# --- Sicherungspunkte: Wiedereinstieg nach dem Tod (WORLD_GENERATION.md, Abschnitt Sicherungspunkte) -----
# Am Eingang jedes Bereichs, damit ein Tod hoechstens einen Bereich zurueckwirft; in Raeumen mit eigenem
# Stromkreis (Gaenge, Vorraeume, Wartung, Rampen), die im Stromausfall eines Decks hell bleiben; mit Blick
# in die Richtung, in die es weitergeht. mapkit.validate() prueft die Regeln. Jeder Punkt bekommt eine
# gruene Sicherheitsleuchte mit eigener Batterie (brennt immer): man erkennt den sicheren Ort von weitem.
roamers = [s_['pos'] for s_ in m.spawns if s_.get('role') in ('hound', 'clump')]


def far_from_roamers(x, y):
    return all(math.hypot(p[0] - x, p[1] - y) >= 15.0 for p in roamers)


def safe_point(cid, x, y, yaw, label):
    ri = m.at(x, y)
    R = m.R(ri)
    top = R.floor + R.height
    m.checkpoint(cid, x, y, yaw, label)
    m.obj('safety_light', x, y, top - 0.01, yaw, tag='emergency')
    if not R.lights:  # Gang ohne eigene Leuchte: ein schwaches, gruenliches Licht
        m.light(ri, x, y, 0, 0.45, 3.2, z=top - 0.12, panel=False, color=(0.75, 1.0, 0.82))


safe_point('start', 5.5, 114.0, 0.0, 'Lastenflur')
safe_point('vor_deck_a', 31.0, 114.0, 0.0, 'Gang vor Deck A')
RA = m.R(rampAB)
safe_point('rampe_ab', RA.x0 + 3.5, RA.cy, 0.0, 'Rampe A–B')
RB = m.R(rampBC)
safe_point('rampe_bc', RB.x0 + 3.5, RB.cy, 0.0, 'Rampe B–C')
for i, vr in enumerate(vorraeume):
    VR = m.R(vr)
    safe_point(f'lager_{i + 1}', VR.cx, VR.cy, -math.pi / 2, f'Vorraum Lagerhalle {i + 1}')
WT = m.R(wartung_tuer)
safe_point('wartung', WT.cx, WT.cy, 0.0, 'Wartungsflur')
safe_point('rohrgang', 228.0, 18.5, -math.pi / 2, 'Rohrgang')
# je Deck ein Zugang ins Labyrinth (der erste Verbindungsgang fern vom Hund in der Deckecke)
seen_decks = set()
for con, dname in connectors:
    CR = m.R(con)
    if dname in seen_decks or not far_from_roamers(CR.cx, CR.cy):
        continue
    seen_decks.add(dname)
    safe_point(f'zugang_{dname.lower()}', CR.cx, CR.cy, math.pi / 2, f'Gang ins Labyrinth (Deck {dname})')
# im Labyrinth: drei Kreuzungen (West, Mitte, Ost), weit genug von den Spawnpunkten des Buendels
for name, tx in (('West', 60.0), ('Mitte', 116.0), ('Ost', 172.0)):
    cands = sorted((v for v in J.values() if v is not None),
                   key=lambda v: math.hypot(m.R(v).cx - tx, m.R(v).cy - 190.0))
    for v in cands:
        RJ = m.R(v)
        if far_from_roamers(RJ.cx, RJ.cy):
            safe_point(f'labyrinth_{name.lower()}', RJ.cx, RJ.cy, math.pi / 2, f'Labyrinth {name}')
            break

# V11: Stuetzenplaketten an Stuetzen, die die Engine fuer einen Durchgang wegraeumt (Kopffreiheit), entfernen -
# die Durchgaenge entstehen teils erst nach den Decks
m.objects = [o for o in m.objects if not (o['model'] == 'pillar_label' and any(
    d['rect'][0] - 3.5 <= o['pos'][0] <= d['rect'][2] + 3.5 and d['rect'][1] - 3.5 <= o['pos'][1] <= d['rect'][3] + 3.5
    for d in m.doors))]
m.save()
