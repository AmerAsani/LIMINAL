"""Level 7 - "Das Meer unter der Decke": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.8).

Ein moeblierter Raum (Regal, Tischchen, Stuhl, Deckenleuchte, nasser Teppich) sitzt in einer hohen
Betondecke; seine Tuer liegt 4,5 m ueber einem endlosen Meer im Daemmerlicht. Vor der Tuer ein flacher
Schelf, dann bricht der Grund steil ab. Weit draussen ein fahler Schein auf der Decke: dort, tief unten,
geht es weiter (Untertauchen -> Level 37).

Kaltes Wasser kostet Gesundheit. Wer lange im tiefen Wasser bleibt, ruft etwas herauf: es kuendigt sich
mit Grollen und Wellen an; Licht nach unten haelt es kurz fern.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level7', 220, 230, seed=7007)
rng = m.rng
SEA_FLOOR = -30.0

meer = m.room(10, 10, 210, 210, 'meer', 'meer', SEA_FLOOR, 30.0 + 9.0, water=0.0, label='Meer')
# Schelf vor der Tuer (flach, 3 m tief), dann steiler Abbruch
m.block(meer, 82, 176, 130, 210, 27.0, hidden=False)
m.block(meer, 92, 160, 120, 176, 22.0, hidden=False)

# Der Raum in der Decke
# Wikidot "Level 7 - Thalassophobia" (Rev. 48): ein Buecherregal, ein kleiner Couchtisch, ein einzelner Stuhl, eine
# Leuchtstoffroehre an der Decke, Teppich mit einer pfuetzentiefen Wasserschicht; die Treppe aus Level 6 an der
# einen Wand, gegenueber die Tuer zum Meer.
raum = m.room(98, 211, 112, 220, 'eingangsraum', 'raum7', 4.5, 2.7, label='Der Raum in der Decke', water=4.53)
m.door(raum, meer, 'opening', 2, at=105)
m.light(raum, 105.0, 215.5, 0, 0.8, 5.0, color=(0.92, 0.97, 1.0))
m.obj('bookcase', 98.01, 214.5, 4.5, 0.0, variant=1, tag='deco')
m.obj('coffee_table', 108.0, 217.0, 4.5, 0.3, tag='deco')
m.obj('chair', 109.6, 218.3, 4.5, 3.6, tag='deco')
treppe = m.room(100, 221, 110, 228, 'treppe', 'raum7', 4.5, 3.0)
m.opening(raum, treppe, width=4)
m.light(treppe, 105.0, 224.5, 0, 0.4, 3.5)
m.transition('l6_l7', 'stairs', 'level6', 105.0, 217.0, -math.pi / 2, rect=[101, 225, 109, 228])
m.spawn('item', 108.1, 217.05, 4.5 + 0.45, tags=['surface', 'coffee_table'], group=1)  # auf dem Couchtisch
m.spawn('item', 98.2, 214.3, 4.5 + 0.86, tags=['surface', 'bookcase'], group=2)          # im Regal
m.spawn('note', 106.0, 218.5, 4.5, tags=['floor'])
m.spawn('note', 101.0, 217.0, 4.5, tags=['floor'])

# Daemmerlicht ueber dem Meer: wenige, schwache, hohe Flaechen
for x in range(30, 200, 40):
    for y in range(30, 200, 40):
        m.light(meer, x + 0.5, y + 0.5, 0, 0.5, 26.0, z=8.6, panel=False, color=(0.72, 0.8, 0.86))
# Der fahle Schein weit draussen: Licht aus der Tiefe, darunter geht es weiter
DX, DY = 70.0, 46.0
m.light(meer, DX, DY, 3, 2.2, 22.0, z=0.05, panel=False, color=(0.6, 0.9, 1.0))
m.transition('l7_l37', 'dive', 'level37', DX, DY, 0.0, rect=[int(DX) - 5, int(DY) - 5, int(DX) + 5, int(DY) + 5])
m.obj('pipe_run', DX, DY, -1.0, 0.0, variant=0, tag='water')

m.checkpoint('raum', 104.0, 216.0, -math.pi / 2, 'Der Raum in der Decke')
m.obj('safety_light', 104.0, 216.0, 7.19, 0.0, tag='emergency')
m.start = (104.0, 216.0, -math.pi / 2)
m.save()
