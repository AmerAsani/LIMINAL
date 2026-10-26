"""Level 6 - "Das Dunkel": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.7).

Endlose, enge Gaenge aus glattem, kaltem Beton. Kein Licht - auch die eigene Lampe gibt hier keins.
Waende in Armlaenge zeichnen sich schwach ab (Tasten). Orientierung nur ueber den Klang: irgendwo rauscht
leise Wasser. Wer dem Rauschen folgt, findet die Treppe hinab zum Meer (Level 7).
Keine Entity. Der Verstand sinkt langsam; Halluzinationen (Atmen, Schritte, Fluestern) greifen nie an.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level6', 140, 112, seed=6006)
rng = m.rng
H = 2.5

J, S, C = corridor_maze(m, 12, 6, 132, 106, 9, 2, 'gang6', 'dunkel', ['kammer6'], lambda t: 'dunkel', height=H, floor=0.0,
                        density=0.78, room_chance=0.25, light_every=999)
for R in m.rooms:
    R.lights = []  # kein Licht, nirgends

# Ankunft (West): Fuss der Treppe aus dem Kesselraum
west_j = min((v for k, v in J.items() if v is not None and k[0] == 0), key=lambda v: abs(m.R(v).cy - 56))
WJ = m.R(west_j)
ank = m.room(2, int(WJ.y0), 11, int(WJ.y1), 'gang6', 'dunkel', 0.0, H)
m.opening(ank, west_j)
m.transition('l5_l6', 'stairs', 'level5', 5.5, WJ.cy, 0.0, rect=[2, int(WJ.y0), 4, int(WJ.y1)])

# Die Treppe hinab zum Meer: am Ostrand, nicht geradeaus erreichbar; dort rauscht das Wasser
east_js = [v for k, v in J.items() if v is not None and k[0] == max(kk[0] for kk, vv in J.items() if vv is not None)]
EJ = m.R(max(east_js, key=lambda v: abs(m.R(v).cy - 56)))
treppe = m.room(int(EJ.x1) + 1, int(EJ.y0), 138, int(EJ.y1), 'treppe', 'dunkel', -3.0, 5.5)
m.opening(m.at(EJ.cx, EJ.cy), treppe)
m.obj('pipe_run', 136.5, EJ.cy, -2.8, math.pi / 2, variant=0, tag='waves')
m.transition('l6_l7', 'stairs', 'level7', 134.0, EJ.cy, math.pi, rect=[136, int(EJ.y0), 138, int(EJ.y1)])

# Belegung: wenige Dinge, die man im Dunkeln ertastet
for (a, b), r in list(S.items())[::6]:
    R = m.R(r)
    m.spawn('item', R.cx, R.cy, 0.0, tags=['floor'], group=r)
for r in list(C.values())[:6]:
    R = m.R(r)
    m.spawn('note', R.cx, R.cy, 0.0, tags=['floor'])

# Sicherungspunkte (ohne Licht - hier toetet nur der eigene Verstand)
m.checkpoint('ankunft', 8.0, WJ.cy, 0.0, 'Am Fuß der Treppe', lit_by_style=True)
mids = sorted((v for v in J.values() if v is not None), key=lambda v: math.hypot(m.R(v).cx - 70, m.R(v).cy - 56))
m.checkpoint('mitte', m.R(mids[0]).cx, m.R(mids[0]).cy, 0.0, 'Irgendwo in der Mitte', lit_by_style=True)
m.checkpoint('rauschen', EJ.cx, EJ.cy, 0.0, 'Wo das Rauschen laut ist', lit_by_style=True)

m.start = (8.0, WJ.cy, 0.0)
m.save()
