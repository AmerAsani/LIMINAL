"""Level 903 - "The Metro" (V12, LEVELSPEC_903.md / LOOK_903.md; Wikidot Level 903, Revision 60).

Querschnitt (y nach unten):            Draufsicht (Ausschnitt):

  Wand mit Stationsschildern            ----------------------------------------------------------------
  linkes Gleis (Schotter, -1,1 m)  ->   ==== Gleis ======[ fahrender Wagen: <- 37 | 36 -> ]==== Gleis ====
  Mittelsteig (graue Ziegel, Roehren)    Mittelsteig  (Treppe zugemauert)       (Treppe zugemauert)
  rechtes Gleis: ruhender Zug           [ruhender Zug ueber die ganze Laenge  ... | bewohnter Wagen | ...]
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level903', 200, 26, seed=9030)
rng = m.rng
HALL_H, PLAT = 5.3, 1.1  # Hallenhoehe ueber dem Gleisbett, Hoehe des Mittelsteigs
TUBE = (0.95, 1.0, 1.0)
C_ALL = 1

H = m.room(2, 2, 198, 24, 'station', 'station', 0.0, HALL_H, circuit=C_ALL, label='Station')
R = m.R(H)

# Mittelsteig als Block (begehbar), ausgespart fuer die zugemauerten Treppen; rechtes Gleis: der ruhende Zug (Block)
STAIRS = [(58, 10, 64, 13), (140, 10, 146, 13)]
m.block(H, R.x0, 7, R.x1, 10, PLAT, hidden=False)
m.block(H, R.x0, 13, R.x1, 15, PLAT, hidden=False)
xs = [R.x0] + [v for (a, _, b, _) in STAIRS for v in (a, b)] + [R.x1]
for k in range(0, len(xs), 2):
    m.block(H, xs[k], 10, xs[k + 1], 13, PLAT, hidden=False)
for (a, y0, b, y1) in STAIRS:
    m.block(H, a, y0, b, y1, 3.0, hidden=True)  # Treppe: man kommt nicht hinauf
m.block(H, R.x0, 15, R.x1, R.y1, 4.4, hidden=True)  # ruhender Zug auf dem rechten Gleis

# Fahrender Wagen am linken Gleis (Wiki: haelt fuer etwa zehn Minuten; nimmt Ziele an)
CX, CY = 100.5, 5.0
m.block(H, 92, 4, 109, 6, PLAT, hidden=False)  # Wagenboden (begehbar; das Modell deckt ihn ab)
for (x0, y0, x1, y1) in ((91, 3, 110, 4), (91, 4, 92, 7), (109, 4, 110, 7)):
    m.block(H, x0, y0, x1, y1, 3.6, hidden=True)  # Wagenwaende (Kollision)
DOORS = (94, 100, 106)
xs = [92] + [v for d in DOORS for v in (d, d + 1)] + [109]
for k in range(0, len(xs), 2):
    m.block(H, xs[k], 6, xs[k + 1], 7, 3.6, hidden=True)  # Seitenwand zum Steig
for d in DOORS:
    m.block(H, d, 6, d + 1, 7, PLAT, hidden=False)  # Tuer: Boden
m.obj('metro_car_903', CX, CY, PLAT + 0.01, 0.0, tag='light')
for x in (95.0, 100.5, 106.0):
    m.light(H, x, CY, 0, 0.6, 6.0, z=PLAT + 2.3, panel=False, color=(1.0, 0.98, 0.92))
m.start = (CX, CY, math.pi / 2)
m.transition('l5_903', 'door', '', CX, CY, math.pi / 2)  # Ankunft im Wagen aus Level 5
m.transition('l37_903', 'door', '', CX, 9.5, -math.pi / 2)  # Ankunft aus Level 37 (ein Wagen hielt dort)
# Ziele (Wiki: man nennt im Wagen ein Ziel): vorn Wellen = Level 37, hinten Flugzeug = Level 36 (Bildzeichen im Modell)
m.transition('l903_37', 'door', 'level37', 92.6, CY, math.pi, rect=[92, 4, 94, 6])
m.transition('l903_36', 'door', 'level36', 108.4, CY, 0.0, rect=[107, 4, 109, 6])

# Gleise mit Schotter (links ausserhalb des Wagens)
for x in range(R.x0 + 3, R.x1 - 2, 6):
    if 88 <= x <= 113:
        continue
    m.obj('track_metro', x, 4.5, 0.0, 0.0, tag='deco')

# Bahnsteigkanten
for x in range(R.x0, R.x1):
    m.obj('platform_edge', x + 0.5, 7.5, PLAT, math.pi, tag='deco')
    m.obj('platform_edge', x + 0.5, 14.5, PLAT, 0.0, tag='deco')

# Ruhender Zug: Abschnitte 6 m, alle 19 m eine Kupplung; ein Wagen bewohnt (Wiki: Stuetzpunkte, Motels)
x0 = R.x0 + 1.0
car = 0
while x0 + 18 < R.x1:
    for k, off in enumerate((3.0, 9.0, 15.0)):
        model = 'metro_dormant_home' if (car == 4 and k == 1) else ('metro_dormant_a' if (car + k) % 2 else 'metro_dormant_b')
        m.obj(model, x0 + off, 15.0 - 0.005, PLAT, -math.pi / 2, tag='light' if 'home' in model else 'deco')
        if 'home' in model:
            m.light(H, x0 + off, 14.0, 0, 0.35, 4.0, z=PLAT + 1.5, panel=False, color=(1.0, 0.7, 0.45))
    if x0 + 19 < R.x1:
        m.obj('metro_dormant_coupling', x0 + 18.5, 15.0 - 0.005, PLAT, -math.pi / 2, tag='deco')
    x0 += 19.0
    car += 1

# Lange, duenne Leuchtstoffroehren in zwei Reihen ueber dem Steig (Wiki); an den Enden wird es dunkel
for row in (8.5, 13.5):
    for x in range(R.x0 + 22, R.x1 - 22, 4):
        m.obj('tube_long', x + 0.5, row, HALL_H, 0.0, tag='light')
        if (x - R.x0) % 8 == 2:
            m.light(H, x + 0.5, row, 0, 0.7, 6.5, z=HALL_H - 0.3, panel=False, color=TUBE)
# Leuchtschilder in unbekannter Schrift
for k, x in enumerate(range(R.x0 + 30, R.x1 - 30, 24)):
    m.obj('sign_903_a' if k % 2 == 0 else 'sign_903_b', x + 0.5, 11.5, HALL_H, 0.0, tag='light')
for x in range(R.x0 + 14, R.x1 - 10, 20):
    if 86 <= x <= 115:
        continue
    m.obj('sign_903_wall', x + 0.5, R.y0 + 0.005, 2.6, math.pi / 2, tag='deco')
# zugemauerte Treppen (Wiki: Treppen tauchen auf, sind aber immer versperrt)
for (a, y0, b, y1) in STAIRS:
    m.obj('stairs_barricaded', a - 0.1, (y0 + y1) / 2, PLAT, 0.0, tag='deco')

# --- Funde (auf den Baenken im Wagen), Notiz, Sicherungspunkt ----------------------------------------------------------------
for (x, y) in ((96.0, 4.35), (103.5, 5.65), (98.0, 5.65)):
    m.spawn('item', x, y, PLAT + 0.45, 0.0, tags=['surface', 'seat'])
m.spawn('note', 104.5, 4.35, PLAT + 0.45, 0.0, tags=['surface', 'seat'])
m.checkpoint('steig', CX, 11.0, 0.0, 'Mittelsteig am Wagen')
m.obj('safety_light', CX, 11.0, HALL_H, 0.0, tag='emergency')

m.save()
