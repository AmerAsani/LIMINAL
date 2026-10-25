"""Level 4 - "Das stille Stockwerk": Entwurf der festen Karte (LEVEL_SELECTION.md, Abschnitt 3.5).

Ein leeres Buerogeschoss: ein Kreuz aus breiten Fluren, dazwischen Grossraumbueros (nur noch Abdruecke
von Moebeln im Teppich), Einzelbueros, Teekuechen mit Wasserspendern, der Aufzugsvorraum (Ankunft aus
Level 3) und ein Treppenhaus hinab ins Hotel (Level 5). An den Aussenwaenden Fenster: die meisten
geschwaerzt, einige zeigen Regen und Nebel. Ruhe nach dem Umspannwerk: keine Angriffe.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, link_regions, partition_rooms  # noqa: E402

m = Map('level4', 150, 110, seed=4004)
rng = m.rng
H = 2.9

# --- Flurkreuz ------------------------------------------------------------------------------------------------
fw = m.room(2, 52, 72, 57, 'flur4', 'flur', 0.0, H, label='Flur')
fe = m.room(78, 52, 148, 57, 'flur4', 'flur', 0.0, H)
fn = m.room(72, 58, 77, 108, 'flur4', 'flur', 0.0, H)
fs = m.room(72, 2, 77, 51, 'flur4', 'flur', 0.0, H)
kreuz = m.room(73, 52, 77, 57, 'flur4', 'flur', 0.0, H)
m.opening(fw, kreuz)
m.opening(kreuz, fe)
m.opening(kreuz, fn)
m.opening(kreuz, fs)


def office_type(w, h, r):
    if w * h >= 150:
        return 'grossraum'
    if min(w, h) <= 3 and max(w, h) >= 8:
        return 'flur4'
    return 'einzelbuero'


# --- Quadranten -------------------------------------------------------------------------------------------------
nw_off = partition_rooms(m, 2, 58, 44, 108, 'buero', office_type, min_leaf=4, max_leaf=10, height=H)
nw_open = m.room(45, 58, 71, 108, 'grossraum', 'buero', 0.0, H, pillars=[8, 8, 1, 49, 63], label='Großraumbüro')
ne_open = m.room(78, 58, 122, 108, 'grossraum', 'buero', 0.0, H, pillars=[8, 8, 1, 82, 63])
ne_kit = partition_rooms(m, 123, 58, 148, 108, 'buero', lambda w, h, r: 'teekueche' if w * h < 40 else 'besprechung',
                         min_leaf=4, max_leaf=9, height=H)
sw_off = partition_rooms(m, 2, 2, 71, 39, 'buero', office_type, min_leaf=4, max_leaf=11, height=H)
lobby = m.room(20, 40, 46, 51, 'aufzugsvorraum', 'lobby', 0.0, 3.2, label='Aufzüge')
sw_rest = partition_rooms(m, 47, 40, 71, 51, 'buero', office_type, min_leaf=4, max_leaf=10, height=H)
cabin = m.room(13, 42, 19, 49, 'kabine', 'lobby', 0.0, 2.6)
side = partition_rooms(m, 2, 40, 12, 51, 'buero', office_type, min_leaf=3, max_leaf=9, height=H)
se_open = m.room(78, 2, 124, 51, 'grossraum', 'buero', 0.0, H, pillars=[8, 8, 1, 82, 6])
# Treppenhaus: oberer Absatz, Treppe hinab, unterer Absatz (Uebergang ins Hotel)
oben = m.room(126, 34, 146, 51, 'treppenhaus', 'treppe', 0.0, 3.2, label='Treppenhaus')
treppe = m.room(130, 16, 142, 33, 'treppe', 'treppe', -3.2, 6.4)
unten = m.room(130, 3, 142, 15, 'treppenhaus', 'treppe', -3.2, 3.2)
se_rest = partition_rooms(m, 125, 2, 129, 33, 'buero', office_type, min_leaf=2, max_leaf=40, height=H)
se_rest2 = partition_rooms(m, 143, 2, 148, 33, 'buero', office_type, min_leaf=2, max_leaf=40, height=H)

m.opening(lobby, cabin, width=2)
m.door(oben, treppe, 'opening', 4)
m.door(treppe, unten, 'opening', 4)
m.transition('l3_l4', 'elevator', 'level3', 16.0, 45.5, 0.0, rect=[13, 42, 17, 49])
m.transition('l4_l5', 'stairs', 'level5', 136.0, 9.0, math.pi / 2, rect=[131, 3, 141, 6])

# --- Verbindungen ----------------------------------------------------------------------------------------------
link_regions(m, nw_off, [fw], 3, width=(1, 2))
link_regions(m, [nw_open], [fw], 2, width=(2, 3))
link_regions(m, [nw_open], [fn], 2, width=(2, 3))
link_regions(m, nw_off, [nw_open], 2, width=(1, 2))
link_regions(m, [ne_open], [fe], 2, width=(2, 3))
link_regions(m, [ne_open], [fn], 2, width=(2, 3))
link_regions(m, ne_kit, [fe], 2, width=(1, 2))
link_regions(m, ne_kit, [ne_open], 2, width=(1, 2))
link_regions(m, sw_off, [fs], 2, width=(1, 2))
link_regions(m, [lobby], [fw], 1, width=(3, 3))
link_regions(m, sw_off, [lobby], 1, width=(1, 2))
link_regions(m, sw_rest, [fw], 1, width=(1, 2))
link_regions(m, sw_rest, [lobby], 1, width=(1, 2))
link_regions(m, side, [fw], 1, width=(1, 2))
link_regions(m, [se_open], [fs], 2, width=(2, 3))
link_regions(m, [se_open], [fe], 2, width=(2, 3))
link_regions(m, [oben], [fe], 1, width=(2, 2))
link_regions(m, se_rest, [se_open], 1, width=(1, 2))
link_regions(m, se_rest2, [oben], 1, width=(1, 1))

# --- Fenster an den Aussenwaenden (meist schwarz, selten Regen) --------------------------------------------------
for R in m.rooms:
    if R.type in ('treppe', 'kabine'):
        continue
    sides = []
    if R.x0 == 2:
        sides.append(('w', R.x0 + 0.005, None, 0.0))
    if R.x1 == 148:
        sides.append(('e', R.x1 - 0.005, None, math.pi))
    if R.y0 == 2:
        sides.append(('s', None, R.y0 + 0.005, math.pi / 2))
    if R.y1 == 108:
        sides.append(('n', None, R.y1 - 0.005, -math.pi / 2))
    for side_, fx, fy, yaw in sides:
        span = (R.y0, R.y1) if fx is not None else (R.x0, R.x1)
        t = span[0] + 2.0
        while t < span[1] - 1.5:
            v = 1 if rng.random() < 0.15 else 0
            if fx is not None:
                m.obj('window', fx, t, R.floor, yaw, variant=v, tag='window' if v else 'deco')
            else:
                m.obj('window', t, fy, R.floor, yaw, variant=v, tag='window' if v else 'deco')
            t += 3.0

# --- Ausstattung und Belegung ------------------------------------------------------------------------------------
for r in [nw_open, ne_open, se_open]:
    R = m.R(r)
    for k in range(int(R.w * R.h / 14)):
        m.spawn('deco', R.x0 + 1.5 + rng.random() * (R.w - 3), R.y0 + 1.5 + rng.random() * (R.h - 3), 0.0,
                rng.choice((0.0, math.pi / 2)), set='carpet_imprint', weight=0.9)
    for k in range(4):
        m.spawn('item', R.x0 + 2 + rng.random() * (R.w - 4), R.y0 + 2 + rng.random() * (R.h - 4), 0.0, tags=['floor'], group=r * 10 + k)
for r in ne_kit:
    R = m.R(r)
    if R.type == 'teekueche':
        m.obj('water_cooler', R.x0 + 0.5, R.y0 + 0.5, 0.0, math.pi / 4, tag='deco')
        for k in range(2):
            m.spawn('item', R.cx + 0.5 * k, R.cy, 0.0, tags=['table'], group=r * 10 + k)
for r in nw_off + sw_off + sw_rest + side:
    R = m.R(r)
    if R.type == 'einzelbuero':
        if rng.random() < 0.6:
            m.spawn('deco', R.cx, R.cy, 0.0, 0.0, set='carpet_imprint', weight=0.9)
        if rng.random() < 0.3:
            m.spawn('item', R.cx + 0.4, R.cy - 0.4, 0.0, tags=['floor'], group=r * 10)
for r in rng.sample(nw_off + sw_off + ne_kit, 8):
    R = m.R(r)
    m.spawn('note', R.cx, R.cy + 0.4, 0.0, tags=['floor'])
for k in range(3):
    m.obj('water_cooler', 20.6 + k * 1.0, 50.4, 0.0, -math.pi / 2, tag='deco')

# --- Sicherungspunkte -------------------------------------------------------------------------------------------
def safe_point(cid, x, y, yaw, label):
    m.checkpoint(cid, x, y, yaw, label, lit_by_style=True)
    ri = m.at(x, y)
    m.obj('safety_light', x, y, m.R(ri).floor + m.R(ri).height - 0.01, yaw, tag='emergency')


safe_point('aufzug', 30.0, 45.5, 0.0, 'Aufzugsvorraum')
kit = min((r for r in ne_kit if m.R(r).type == 'teekueche'), key=lambda r: abs(m.R(r).cy - 80))
safe_point('kueche', m.R(kit).cx, m.R(kit).cy, 0.0, 'Teeküche')
safe_point('treppe', 136.0, 42.0, -math.pi / 2, 'Treppenhaus')
safe_point('kreuz', 74.5, 54.5, 0.0, 'Flurkreuz')
safe_point('unten', 136.0, 12.0, math.pi / 2, 'Unten im Treppenhaus')

m.start = (30.0, 45.5, 0.0)
m.save()
