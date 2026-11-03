"""Level 37 - "Sublimity" (V12, LEVELSPEC_37.md / LOOK_37.md; Wikidot Level 37, Revision 52).

Draufsicht (x nach rechts, y nach unten):

   ##[Wagen (halb in der Wand)]~~~~~~~~ Tunnel mit Wasser ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   Raeume und Gaenge aus weissen Fliesen, knoecheltief geflutet; Hallen voller Saeulen; Gruben, in die Treppen
   direkt ins tiefe Wasser fuehren (oft dunkel); Licht aus seltsamen Winkeln, manche Bereiche ganz dunkel.

Ankunft: der Metro-Wagen aus Level 903 haelt im Tunnel, sein vorderes Ende steckt in der Wand (er faehrt durch feste
Materie, Wiki 903). Rueckweg: hinteres Wagenende (Bildzeichen Wagen) -> Level 903.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, partition_rooms, link_regions  # noqa: E402

m = Map('level37', 150, 120, seed=3737)
rng = m.rng
WATER = 0.3  # knoecheltief ueber dem Fliesenboden
PLAT = 1.1
POOL = (0.70, 0.95, 1.0)
CALM = (0.92, 1.0, 1.0)

# --- Tunnel mit dem haltenden Wagen ---------------------------------------------------------------------------------------
T = m.room(4, 4, 60, 11, 'tunnel', 'fliesen', 0.0, 4.2, water=WATER, label='Tunnel')
CX, CY = 12.5, 6.0
m.block(T, 4, 5, 21, 7, PLAT, hidden=False)  # Wagenboden (der Wagen steckt vorn in der Westwand)
for (x0, y0, x1, y1) in ((4, 4, 22, 5), (21, 5, 22, 8)):
    m.block(T, x0, y0, x1, y1, 3.6, hidden=True)
DOORS = (6, 12, 18)
xs = [4] + [v for d in DOORS for v in (d, d + 1)] + [21]
for k in range(0, len(xs), 2):
    m.block(T, xs[k], 7, xs[k + 1], 8, 3.6, hidden=True)
for d in DOORS:
    m.block(T, d, 7, d + 1, 8, PLAT, hidden=False)
    m.block(T, d, 8, d + 1, 9, 0.55, hidden=False)  # Stufe aus dem Wasser
m.obj('metro_car_37', CX, CY, PLAT + 0.01, 0.0, tag='light')
for x in (7.0, 12.5, 18.0):
    m.light(T, x, CY, 0, 0.6, 6.0, z=PLAT + 2.3, panel=False, color=(1.0, 0.98, 0.92))
m.start = (CX, CY, math.pi / 2)
m.transition('l903_37', 'door', '', CX, CY, math.pi / 2)  # Ankunft im Wagen aus Level 903
m.transition('l37_903', 'door', 'level903', 19.6, CY, 0.0, rect=[19, 5, 21, 7])
TR = m.R(T)
for x in range(TR.x0 + 3, TR.x1 - 1, 7):  # Licht aus seltsamen Winkeln: tief an der Suedwand
    m.light(T, x + 0.5, TR.y1 - 0.6, 1, 0.55, 6.0, 0, -1, z=0.7, panel=False, color=CALM)


# --- Becken: Raeume und Gaenge, geflutet ----------------------------------------------------------------------------------
def pick(w, h, r):
    if min(w, h) <= 3 and max(w, h) >= 7:
        return 'fliesengang'
    return 'saeulenhalle' if w * h >= 130 else 'fliesenraum'


rooms = partition_rooms(m, 4, 12, 146, 118, 'fliesen', pick, min_leaf=3, max_leaf=16, height=(3.0, 5.0),
                        stop_chance=0.35, loop_chance=0.45, width=(2, 3), water=WATER)
link_regions(m, [T], rooms, 3, width=(2, 3))
pits, halls, lit_rooms = [], [], []
for r in rooms:
    R = m.R(r)
    area = R.w * R.h
    u = rng.random()
    if R.type == 'saeulenhalle':
        R.kw['pillars'] = [3, 3, 1, 1 + rng.randrange(2), 1 + rng.randrange(2)]  # Wiki: zu viele Saeulen fuer einen Zweck
        halls.append(r)
    elif R.type == 'fliesenraum' and 30 <= area <= 160 and u < 0.33:
        # Grube: Treppen fuehren direkt ins tiefe Wasser (die Engine legt sie an den Durchgaengen an)
        R.floor, R.height = -4.2, R.height + 4.2
        R.type = 'grube'
        pits.append(r)
    roll = rng.random()
    if R.type == 'grube':
        if roll < 0.4:  # Wiki: tiefe Bereiche meist schlecht beleuchtet
            m.light(r, R.cx, R.cy, 3, 1.0, 8.0, z=R.floor + 0.2, panel=False, color=POOL)
            lit_rooms.append(r)
    elif R.type == 'fliesengang' and roll < 0.2:
        R.type = 'dunkelgang'  # dunkle, geschlossene Gaenge
    elif roll < 0.12:
        R.type = 'dunkelraum'
    elif roll < 0.55:  # Wandlicht tief ueber dem Wasser, schraeg in den Raum
        side = rng.randrange(4)
        lx, ly, dx, dy = ((R.x0 + 0.4, R.cy, 1, 0), (R.x1 - 0.4, R.cy, -1, 0), (R.cx, R.y0 + 0.4, 0, 1), (R.cx, R.y1 - 0.4, 0, -1))[side]
        m.light(r, lx, ly, 1, 0.7, 7.0, dx, dy, z=0.6 + rng.random() * 0.8, panel=False, color=CALM)
        lit_rooms.append(r)
    elif roll < 0.8:  # Deckenlicht aus einer Ecke
        m.light(r, R.x0 + 1.5 + rng.random() * (R.w - 3), R.y0 + 1.5 + rng.random() * (R.h - 3), 0, 0.6, 6.0, color=CALM)
        lit_rooms.append(r)
    else:  # Licht von unten aus dem Wasser
        m.light(r, R.cx, R.cy, 3, 0.8, 7.0, z=R.floor + 0.05, panel=False, color=POOL)
        lit_rooms.append(r)
for r in rooms + [T]:  # Wasser: flach und klar ueber den Fliesen, in den Gruben tief (V12 water_look)
    m.R(r).kw['water_look'] = 2 if m.R(r).type == 'grube' else 1
for r in halls:  # Saeulenhallen: zwei weiche Lichter von unten
    R = m.R(r)
    for (fx, fy) in ((0.3, 0.3), (0.7, 0.7)):
        m.light(r, R.x0 + R.w * fx + 0.5, R.y0 + R.h * fy + 0.5, 3, 0.8, 10.0, z=R.floor + 0.05, panel=False, color=POOL)
    lit_rooms.append(r)

# --- Notiz, Sicherungspunkte ------------------------------------------------------------------------------------------------
m.spawn('note', 18.5, 8.5, 0.55, 0.0, tags=['surface', 'step'])
m.checkpoint('tunnel', 14.5, 9.5, 0.0, 'Am Wagen')
m.obj('safety_light', 14.5, 9.5, TR.floor + TR.height, 0.0, variant=1, tag='emergency')  # V13.3: ohne Notausgang-Piktogramm (STRUKTURTYP, signs.py)
for k, r in enumerate(sorted([h for h in halls if h in lit_rooms], key=lambda r: m.R(r).cx)[:2]):
    R = m.R(r)
    x, y = R.x0 + 0.5, R.y0 + 0.5
    if not m.blocked(x, y):
        m.checkpoint(f'halle{k}', x, y, 0.0, 'Saeulenhalle')
m.save()
