"""Level 37 - "Die Becken": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.10). Ziel der Reise.

Raeume und Gaenge aus makellos weissen Fliesen, grossteils geflutet: knoecheltief in den Gaengen, tiefe
Becken mit Treppen hinab, Saeulen im Wasser. Licht aus seltsamen Winkeln, manches kommt von unten aus dem
Wasser, manche Bereiche sind dunkel. Kurzer Hall, der abrupt abbricht. Keine Entity; das Wasser entspannt.

Ankunft: aufgetaucht in einem Becken (aus dem Meer von Level 7). Ende: das Becken, in dem das Licht von
unten kommt - wer hineintaucht, ist angekommen (Abspann).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, partition_rooms, link_regions  # noqa: E402

m = Map('level37', 150, 120, seed=3737)
rng = m.rng
WATER = 0.3  # knoecheltief ueber dem Fliesenboden


def tile_type(w, h, r):
    if min(w, h) <= 3 and max(w, h) >= 8:
        return 'fliesengang'
    return 'beckenraum' if w * h >= 60 else 'fliesenraum'


rooms = partition_rooms(m, 2, 2, 120, 118, 'fliesen', tile_type, min_leaf=4, max_leaf=16, height=(3.0, 4.6),
                        stop_chance=0.35, loop_chance=0.45, width=(2, 3))
pools = []
for r in rooms:
    R = m.R(r)
    R.kw['water'] = WATER
    if R.type == 'beckenraum' and rng.random() < 0.6:
        # tiefes Becken: Boden 3,5 m tiefer, Rand als Laufsteg (Block auf Bodenhoehe)
        R.floor = -3.5
        R.height = R.height + 3.5
        R.type = 'becken'
        pools.append(r)
        m.block(r, R.x0, R.y0, R.x1, R.y0 + 1, 3.5, hidden=False)
        m.block(r, R.x0, R.y1 - 1, R.x1, R.y1, 3.5, hidden=False)
        m.block(r, R.x0, R.y0 + 1, R.x0 + 1, R.y1 - 1, 3.5, hidden=False)
        m.block(r, R.x1 - 1, R.y0 + 1, R.x1, R.y1 - 1, 3.5, hidden=False)
        # Licht von unten (manche Becken)
        if rng.random() < 0.6:
            m.light(r, R.cx, R.cy, 3, 1.0, 9.0, z=-3.3, panel=False, color=(0.7, 0.95, 1.0))
    else:
        # Licht aus seltsamen Winkeln: Wandleuchten tief unten oder gar keins
        roll = rng.random()
        if roll < 0.55:
            m.light(r, R.x0 + 0.6, R.cy, 1, 0.7, 6.0, 1, 0)
        elif roll < 0.8:
            m.light(r, R.cx, R.cy, 0, 0.6, 5.0, color=(0.92, 1.0, 1.0))

# Das Ende: ein grosses Becken im Osten, Licht von unten, eine breite Treppe hinab
ende = m.room(121, 30, 148, 90, 'endbecken', 'ende', -4.0, 9.0, water=WATER, label='Das letzte Becken')
m.block(ende, 121, 30, 148, 40, 4.0, hidden=False)  # Rand am Eingang
for k, y in enumerate(range(40, 46)):  # Stufen hinab
    m.block(ende, 121, y, 148, y + 1, 4.0 - 0.6 * (k + 1), hidden=False)
for x in (128, 135, 142):
    for y in (52, 64, 76):
        m.block(ende, x, y, x + 1, y + 1, 12.0, hidden=False)  # Saeulen im Wasser
m.light(ende, 135.0, 70.0, 3, 3.0, 26.0, z=-3.8, panel=False, color=(0.85, 0.97, 1.0))
m.light(ende, 135.0, 34.0, 0, 0.5, 6.0, z=4.6, panel=False, color=(0.9, 1.0, 1.0))
link_regions(m, rooms, [ende], 2, width=(3, 4))
m.transition('l37_ende', 'end', '', 135.0, 72.0, 0.0, rect=[124, 56, 146, 88])

# Ankunft: aufgetaucht in einem Becken nahe dem Westrand
arrive = min(pools, key=lambda r: math.hypot(m.R(r).cx - 25, m.R(r).cy - 60)) if pools else rooms[0]
AR = m.R(arrive)
m.transition('l7_l37', 'dive', '', AR.cx, AR.cy, 0.0)
m.start = (AR.cx, AR.cy, 0.0)

for r in rng.sample(rooms, 6):
    R = m.R(r)
    if R.type != 'becken':
        m.spawn('note', R.cx, R.cy, 0.0, tags=['floor'])

m.checkpoint('ankunft', AR.x0 + 0.5, AR.y0 + 0.5, 0.0, 'Aufgetaucht', lit_by_style=True)
m.checkpoint('ende', 135.0, 35.0, math.pi / 2, 'Am letzten Becken')
m.save()
