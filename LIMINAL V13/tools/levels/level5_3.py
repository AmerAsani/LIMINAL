"""Level 5.3 - "Promethei Bibliotheca" (V12, LEVELSPEC_5.3.md / LOOK_5.3.md; Wikidot Level 5.3, Revision 40).

Draufsicht (x nach rechts, y nach unten):

                     Obergeschoss (+3 m): Gaenge mit Regalen ===== Treppe ===== Alte Gaenge (+6 m, Wappen)
                                   |
                              Treppenpodest
                                   ||  (Holztreppe)
   [Leere hinter    Westfluegel:  +===== Grosse Halle (3 Geschosse Regale, =====+  Ostfluegel:
    Gittern]  =##   Gaenge alt    |      Tuffsaeulen, Kandelaber,             |  Gaenge  ==== Sackgasse: alte Holztuer
                    (dunkler)     +===== eiserne Kronleuchter) ===============+                (zurueck nach Level 5)
                                              ||
                                       alte Tuer, Eingangsgang (abgeloeste Tapete) <- Ankunft aus Level 5
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, corridor_maze  # noqa: E402

m = Map('level5_3', 140, 136, seed=5303)
rng = m.rng
HALL_H, AISLE_H, OLD_H = 10.2, 4.6, 4.2
CANDLE, IRON = (1.0, 0.78, 0.52), (1.0, 0.84, 0.62)
C_ALL = 5  # kein Stromausfall in der Bibliothek (Kerzen)

# --- Eingangsgang (Wiki: aelter als der Rest des Hotels, Tapete abgeblaettert; die Tuer am Ende sehr alt) ----------
E = m.room(62, 108, 65, 132, 'eingang', 'flur_alt', 0.0, 3.6, circuit=C_ALL, label='Eingangsgang')
m.start = (63.5, 129.5, -math.pi / 2)
m.transition('l5_53', 'seam', '', 63.5, 129.5, -math.pi / 2)  # Ankunft aus Level 5 (Gang nahe "Housekeeping")

# --- Grosse Halle -------------------------------------------------------------------------------------------------------
A = m.room(44, 50, 84, 107, 'halle', 'halle', 0.0, HALL_H, circuit=C_ALL, label='Grosse Halle')
dE = m.doors[m.door(E, A, 'door', 1, at=63, top=2.15)]
x0, y0, x1, y1 = dE['rect']
m.obj('door_old_open', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, -math.pi / 2)

# Obergeschoss: Podest ueber eine breite Holztreppe (die Stufen legt die Engine in die Halle vor die Tuer)
U = m.room(54, 38, 74, 49, 'podest', 'obergeschoss', 3.0, AISLE_H, circuit=C_ALL, label='Obergeschoss')
dU = m.doors[m.door(A, U, 'wide', 4, at=62, top=2.6)]

# --- Fluegel: Gaenge zwischen Regalen (Wiki: hunderte Gaenge in alle Richtungen) ---------------------------------------
WJ, WS, _ = corridor_maze(m, 12, 52, 43, 106, 9, 3, 'gang_alt', 'gang_alt', [], 'gang_alt', height=AISLE_H, density=0.8,
                          room_chance=0.0, circuit=C_ALL, light_every=99)
EJ, ES, _ = corridor_maze(m, 86, 52, 131, 106, 9, 3, 'gang', 'gang', [], 'gang', height=AISLE_H, density=0.8,
                          room_chance=0.0, circuit=C_ALL, light_every=99)
UJ, US, _ = corridor_maze(m, 40, 4, 88, 37, 9, 3, 'gang', 'gang', [], 'gang', floor=3.0, height=AISLE_H, density=0.8,
                          room_chance=0.0, circuit=C_ALL, light_every=99)
TJ, TS, _ = corridor_maze(m, 102, 4, 138, 37, 9, 3, 'gang_alt', 'alt', [], 'alt', floor=6.0, height=OLD_H, density=0.8,
                          room_chance=0.0, circuit=C_ALL, light_every=99)
for S in (WS, ES, US, TS):  # keine Deckenleuchten in den Gaengen: nur Kerzen (unten)
    for r in S.values():
        m.R(r).lights.clear()

# Anschluesse der Fluegel an die Halle (Durchgaenge mit Sturz; darueber laufen die Galerien weiter)
aisle_doors = []
for (i, j), r in WJ.items():
    if r is not None and i == 3 and j in (1, 3, 5):
        aisle_doors.append(m.doors[m.door(r, A, 'door', 3, top=2.6)])
for (i, j), r in EJ.items():
    if r is not None and i == 0 and j in (1, 3, 5):
        aisle_doors.append(m.doors[m.door(A, r, 'door', 3, top=2.6)])
m.door(UJ[(2, 3)], U, 'door', 3, top=2.6)

# Treppe zu den alten Gaengen (+6 m): langer Treppengang, Stufen vor der Tuer im unteren Raum
ST = m.room(89, 22, 101, 25, 'treppe', 'gang', 3.0, AISLE_H + 3.0, circuit=C_ALL, label='Treppe')
m.opening(UJ[(5, 2)], ST)
m.door(ST, TJ[(0, 2)], 'door', 3, top=2.6)

# --- Leere hinter eisernen Gittern (Wiki: Gitter an einem Gang, der zu einer Leere fuehrt) -------------------------------
VOID = m.room(1, 60, 11, 100, 'leere', 'leere', 0.0, 40.0, circuit=C_ALL, sealed=True)
vj = WJ[(0, 3)]
dv = m.doors[m.door(vj, VOID, 'opening', 3)]
bx0, by0, bx1, by1 = dv['rect']
m.obj('bars_w3', bx0, (by0 + by1) / 2, 0.0, 0.0)  # an der Grenze zur Leere
m.block(VOID, bx0 - 1, by0, bx0, by1, 40.0, hidden=True)

# --- Ausgang: Sackgasse mit sehr alter Holztuer (Wiki: fuehrt zurueck zum Eingang der Bibliothek) ------------------------
ej = EJ[(4, 3)]
DE = m.room(126, m.R(ej).y0, 134, m.R(ej).y1, 'gang', 'gang', 0.0, AISLE_H, circuit=C_ALL, label='Sackgasse')
m.opening(ej, DE)
VE = m.room(135, m.R(ej).y0, 138, m.R(ej).y1, 'vorraum', 'flur_alt', 0.0, 3.0, circuit=C_ALL)
dx_ = m.doors[m.door(DE, VE, 'door', 1, top=2.15)]
x0, y0, x1, y1 = dx_['rect']
m.obj('door_old_open', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, 0.0)
VR = m.R(VE)
m.transition('l53_l5', 'door', 'level5', VR.x0 + 1.5, VR.cy, 0.0, rect=[VR.x0 + 1, VR.y0, VR.x1, VR.y1])
m.light(VE, VR.cx, VR.cy, 0, 0.3, 3.0, z=2.4, panel=False, color=CANDLE)


# --- Ausstattung -----------------------------------------------------------------------------------------------------------
def sides(r):
    """Wandflaechen eines Raums je Kachel (x, y, yaw) inklusive Tueren (fuer Teile oberhalb des Sturzes)."""
    R = m.R(r)
    out = []
    for y in range(R.y0, R.y1):
        out.append((R.x0 + 0.005, y + 0.5, 0.0))
        out.append((R.x1 - 0.005, y + 0.5, math.pi))
    for x in range(R.x0, R.x1):
        out.append((x + 0.5, R.y0 + 0.005, math.pi / 2))
        out.append((x + 0.5, R.y1 - 0.005, -math.pi / 2))
    return out


shelf_spots = []  # Fach 3 (1,125 m) mit Ablage vor den Buechern: Funde


def near_door(x, y, d, pad):
    rx0, ry0, rx1, ry1 = d['rect']
    return rx0 - pad <= x <= rx1 + pad and ry0 - pad <= y <= ry1 + pad


# Halle: Regalwaende mit Galerien; ueber Durchgaengen nur die oberen Geschosse; Portal (Sued) und Treppenwand (Nord) Marmor
R = m.R(A)
for (fx, fy, yaw) in sides(A):
    c, s_ = math.cos(yaw), math.sin(yaw)
    if near_door(fx, fy, dU, 1.0) or (yaw == -math.pi / 2 and 57 <= fx <= 70):
        continue  # Marmorwand: Treppenaufgang, Portal mit Inschrift
    corner = fx < R.x0 + 1.3 or fx > R.x1 - 1.3 if abs(s_) > 0.5 else fy < R.y0 + 1.3 or fy > R.y1 - 1.3
    if corner:
        continue  # Ecken: die Regale der beiden Waende wuerden sich schneiden
    if m.wall_ok(fx, fy, yaw, rel_z=1.0):
        along = fy if abs(c) > 0.5 else fx
        if int(along) % 4 == 2:  # Kerzen an den Handlaeufen beider Galerien
            m.obj('library_wall_lamp', fx, fy, 0.0, yaw, tag='light')
            for zl in (4.3, 7.3):
                m.light(A, fx + c * 1.1, fy + s_ * 1.1, 0, 0.4, 6.5, z=zl, panel=False, color=CANDLE)
        else:
            m.obj('library_wall', fx, fy, 0.0, yaw, tag='deco')
        if int(along) % 8 == 6 and abs(c) > 0.5:  # Kandelaber vor den Regalen (Wiki-Bild)
            px, py = fx + c * 1.9, fy + s_ * 1.9
            if not m.blocked(px, py) and not any(near_door(px, py, d, 1.5) for d in m.doors if A in (d['a'], d['b'])):
                m.obj('candelabrum', px, py, 0.0, rng.random() * 6.28, tag='light')
                m.light(A, px, py, 0, 0.5, 6.0, z=1.85, panel=False, color=CANDLE)
        if rng.random() < 0.08:
            shelf_spots.append((fx + c * 0.36, fy + s_ * 0.36, 1.125, A))
    elif m.wall_ok(fx, fy, yaw, rel_z=3.2):
        m.obj('library_wall_upper', fx, fy, 3.0, yaw, tag='deco')
# Inschriften (Wiki: "Promethei Bibliotheca" an einigen Waenden): ueber dem Portal und ueber der Treppe
m.obj('inscription_bibliotheca', 63.5, R.y1 - 0.005, 3.6, -math.pi / 2, tag='deco')
m.obj('inscription_bibliotheca', 64.0, R.y0 + 0.005, 7.6, math.pi / 2, scale=[1.0, 1.7, 1.7], tag='deco')
for x in (59.5, 67.5):
    m.obj('painting_renaissance_a' if x < 63 else 'painting_renaissance_b', x, R.y1 - 0.005, 2.0, -math.pi / 2, tag='deco')
# Tuffsaeulen in zwei Reihen, Kandelaber daneben, eiserne Kronleuchter in der Mitte
for y in (64.5, 71.5, 78.5, 85.5, 92.5, 99.5):
    for x in (49.5, 78.5):
        m.obj('column_tuff', x, y, 0.0, 0.0)
        m.block(A, int(x), int(y), int(x) + 1, int(y) + 1, HALL_H, hidden=True)
        cx = x + (1.3 if x < 64 else -1.3)
        m.obj('candelabrum', cx, y + 0.6, 0.0, rng.random() * 6.28, tag='light')
        m.light(A, cx, y + 0.6, 0, 0.55, 6.5, z=1.85, panel=False, color=CANDLE)
for y in (64.0, 78.0, 92.0):
    m.obj('chandelier_iron', 64.0, y, HALL_H, 0.0, tag='light')
    m.light(A, 64.0, y, 0, 0.9, 14.0, z=HALL_H - 2.4, panel=False, color=IRON)
for y in (71.0, 85.0, 99.0):  # gemalte Wappen an der Decke der Halle (Wiki-Bild), zwischen den Kronleuchtern
    m.obj('ceiling_arms', 64.0, y, HALL_H, 0.0, scale=[2.4, 2.4, 1.0], tag='deco')
for x in (58.5, 68.5):  # Portal: zwei Kandelaber
    m.obj('candelabrum', x, R.y1 - 1.2, 0.0, 0.3, tag='light')
    m.light(A, x, R.y1 - 1.2, 0, 0.6, 6.5, z=1.85, panel=False, color=CANDLE)
m.light(A, 63.5, R.y1 - 2.0, 0, 0.35, 5.0, z=4.4, panel=False, color=CANDLE)  # Schein auf der Inschrift
m.light(A, 64.0, R.y0 + 2.5, 0, 0.35, 5.5, z=6.8, panel=False, color=IRON)  # Inschrift ueber der Treppe
m.obj('books_fallen', 71.0, 74.0, 0.0, 0.7, tag='deco')

# Gaenge: Regale an beiden Laengswaenden, darueber Gemaelde (Wiki: im Stil der Renaissance), Kerzen an den Kreuzungen;
# in den alten Gaengen Wappen an der Decke (Wiki-Bild)
PAINT = ['painting_renaissance_a', 'painting_renaissance_b']


def dress_aisles(J, S, old):
    for (a, b), r in S.items():
        R = m.R(r)
        along_x = R.w > R.h
        for k, (fx, fy, yaw) in enumerate(m.wall_faces(r, margin=0)):
            if along_x != (abs(math.sin(yaw)) > 0.5):
                continue  # nur die Laengswaende
            c, s_ = math.cos(yaw), math.sin(yaw)
            m.obj('bookcase_aisle' if (int(fx) + int(fy)) % 2 else 'bookcase_aisle_b', fx, fy, 0.0 + R.floor, yaw, tag='deco')
            if rng.random() < 0.1:
                shelf_spots.append((fx + c * 0.36, fy + s_ * 0.36, R.floor + 1.125, r))
        # ein Gemaelde ueber den Regalen (Mitte einer Laengswand)
        if (R.w if along_x else R.h) >= 4 and rng.random() < 0.75:
            if along_x:
                fx, fy, yaw = R.cx, (R.y0 + 0.005 if rng.random() < 0.5 else R.y1 - 0.005), 0.0
                yaw = math.pi / 2 if fy < R.cy else -math.pi / 2
            else:
                fy, fx = R.cy, (R.x0 + 0.005 if rng.random() < 0.5 else R.x1 - 0.005)
                yaw = 0.0 if fx < R.cx else math.pi
            m.obj(rng.choice(PAINT), fx, fy, R.floor + 3.78, yaw, tag='deco')
        if old:
            m.obj('ceiling_arms', R.cx, R.cy, R.floor + R.height, 0.0 if along_x else math.pi / 2, tag='deco')
        if rng.random() < 0.12:
            m.obj('books_fallen', R.cx, R.cy, R.floor, rng.random() * 6.28, tag='deco')
        # V13.3 (Kritikrunde 1): in hellen Gaengen Kandelaber zwischen den Regalen - vorher standen sie nur an den
        # Kreuzungen und die Regale der Gaenge blieben schwarz (Wiki: nur ein aelterer Gang liegt im Dunkeln)
        if not old:
            L_ = R.w if along_x else R.h
            for k_ in range(3, L_ - 2, 6):
                side = 1 if (k_ // 6) % 2 else -1
                if along_x:
                    cx, cy = R.x0 + k_ + 0.5, R.cy + side * (R.h / 2 - 0.75)
                else:
                    cx, cy = R.cx + side * (R.w / 2 - 0.75), R.y0 + k_ + 0.5
                if not m.blocked(cx, cy):
                    m.obj('candelabrum', cx, cy, R.floor, rng.random() * 6.28, tag='light')
                    m.light(r, cx, cy, 0, 0.55, 6.0, z=R.floor + 1.85, panel=False, color=CANDLE)
    for (i, j), r in J.items():
        if r is None:
            continue
        R = m.R(r)
        # Kandelaber in einer Ecke der Kreuzung, die an Wand grenzt (nicht im Durchgang)
        for (cx, cy) in ((R.x0 + 0.45, R.y0 + 0.45), (R.x1 - 0.45, R.y1 - 0.45), (R.x0 + 0.45, R.y1 - 0.45), (R.x1 - 0.45, R.y0 + 0.45)):
            if not m.blocked(cx, cy):
                break
        lit = (i + j) % 2 == 0 or not old
        if lit:
            m.obj('candelabrum', cx, cy, R.floor, rng.random() * 6.28, tag='light')
            m.light(r, cx, cy, 0, 0.6 if old else 0.75, 8.0, z=R.floor + 1.85, panel=False, color=CANDLE)
        if old:
            m.obj('ceiling_arms', R.cx, R.cy, R.floor + R.height, 0.0, tag='deco')


dress_aisles(WJ, WS, True)
dress_aisles(EJ, ES, False)
dress_aisles(UJ, US, False)
dress_aisles(TJ, TS, True)
for r in (DE,):
    for (fx, fy, yaw) in m.wall_faces(r, margin=0):
        if abs(math.sin(yaw)) > 0.5:
            m.obj('bookcase_aisle', fx, fy, 0.0, yaw, tag='deco')
    R_ = m.R(r)
    m.obj('candelabrum', R_.x1 - 1.0, R_.y0 + 0.5, 0.0, 0.0, tag='light')
    m.light(r, R_.x1 - 1.0, R_.y0 + 0.5, 0, 0.5, 6.0, z=1.85, panel=False, color=CANDLE)

# Podest und Treppengang
for r in (U, ST):
    R_ = m.R(r)
    for (fx, fy, yaw) in m.wall_faces(r, margin=1):
        if r == ST and abs(math.sin(yaw)) < 0.5:
            continue
        if r == ST and fx > R_.x0 + 2.5:
            continue  # dort liegen die Stufen der Treppe (Regalboeden muessen auf Vielfachen von 0,375 m liegen)
        m.obj('bookcase_aisle', fx, fy, R_.floor, yaw, tag='deco')
U_ = m.R(U)
# frei stehende Regale im Obergeschoss: Block (Kollision) mit Regalen auf beiden Seiten und Stirnbrettern
for bx in (U_.x0 + 2, U_.x1 - 5):  # laengs, abseits der Tueren (vor Tueren haelt die Engine Platz frei)
    by0, by1 = U_.y0 + 2, U_.y1 - 2
    m.block(U, bx, by0, bx + 1, by1, 3.3, hidden=True)
    for y in range(by0, by1):
        m.obj('bookcase_aisle' if y % 2 else 'bookcase_aisle_b', bx - 0.005, y + 0.5, U_.floor, math.pi, tag='deco')
        m.obj('bookcase_aisle_b' if y % 2 else 'bookcase_aisle', bx + 1.005, y + 0.5, U_.floor, 0.0, tag='deco')
        if rng.random() < 0.15:
            shelf_spots.append((bx + 1.365, y + 0.5, U_.floor + 1.125, U))
    m.obj('bookcase_end', bx + 0.5, by0 - 0.005, U_.floor, -math.pi / 2, tag='deco')
    m.obj('bookcase_end', bx + 0.5, by1 + 0.005, U_.floor, math.pi / 2, tag='deco')
for (cx, cy) in ((U_.x0 + 2.5, U_.y0 + 0.9), (U_.x1 - 4.5, U_.y0 + 0.9), (U_.x0 + 2.5, U_.y1 - 0.9), (U_.x1 - 4.5, U_.y1 - 0.9)):
    m.obj('candelabrum', cx, cy, U_.floor, 0.2, tag='light')
    m.light(U, cx, cy, 0, 0.55, 7.0, z=U_.floor + 1.85, panel=False, color=CANDLE)
m.obj('chandelier_iron', U_.cx, U_.cy, U_.floor + U_.height, 0.0, tag='light')
m.light(U, U_.cx, U_.cy, 0, 0.6, 9.0, z=U_.floor + U_.height - 2.4, panel=False, color=IRON)
S_ = m.R(ST)
m.obj('candelabrum', S_.x0 + 0.5, S_.y0 + 0.5, S_.floor, 0.0, tag='light')
m.light(ST, S_.x0 + 0.5, S_.y0 + 0.5, 0, 0.5, 7.0, z=S_.floor + 1.85, panel=False, color=CANDLE)

# Eingangsgang: Vertaefelung, abgeloeste Tapete, wenige Kerzenwandleuchter
ER = m.R(E)
for k, (fx, fy, yaw) in enumerate(m.wall_faces(E, margin=0)):
    m.obj('wainscot', fx, fy, 0.0, yaw, tag='deco')
    if k % 7 == 3:
        m.obj('wallpaper_peel', fx, fy, 0.0, yaw, tag='deco')
    elif k % 7 == 5 and fy > ER.y0 + 2:
        m.obj('candle_sconce', fx, fy, 2.05, yaw, tag='light')
        m.light(E, fx + math.cos(yaw) * 0.3, fy + math.sin(yaw) * 0.3, 0, 0.4, 4.5, z=2.2, panel=False, color=CANDLE)

# Die Leere: kein Licht. Ein Kandelaber an der Kreuzung vor den Gittern
VJ = m.R(vj)
m.obj('candelabrum', VJ.x1 - 0.45, VJ.y0 + 0.45, 0.0, 0.0, tag='light')
m.light(vj, VJ.x1 - 0.45, VJ.y0 + 0.45, 0, 0.45, 6.0, z=1.85, panel=False, color=CANDLE)

# --- Funde (Auftrag: wenige, auf Flaechen), Notizen, Sicherungspunkte ----------------------------------------------------------
rng.shuffle(shelf_spots)
for (x, y, z, r) in shelf_spots[:14]:
    m.spawn('item', x, y, z, rng.random() * 6.28, tags=['surface', 'shelf'], group=r)
for (x, y, z, r) in shelf_spots[14:18]:
    m.spawn('note', x, y, z, rng.random() * 6.28, tags=['surface', 'shelf'], group=r)


def safe_point(cid, r, label, at=None):
    R_ = m.R(r)
    x, y = at if at else (R_.cx, R_.cy)
    m.checkpoint(cid, x, y, 0.0, label)
    m.obj('lantern_green', x, y, R_.floor + R_.height, 0.0, tag='emergency')
    m.light(r, x, y, 0, 0.08, 2.2, z=R_.floor + R_.height - 0.75, panel=False, color=(0.75, 1.0, 0.8))


safe_point('eingang', E, 'Eingangsgang', at=(ER.cx, ER.y0 + 6.0))
safe_point('halle_sued', A, 'Grosse Halle', at=(64.0, 99.0))
safe_point('halle_nord', A, 'Grosse Halle, Treppe', at=(70.0, 66.0))
safe_point('obergeschoss', U, 'Obergeschoss')
safe_point('alte_gaenge', TJ[(0, 2)], 'Alte Gaenge')
safe_point('ostfluegel', EJ[(4, 3)], 'Ostfluegel')
safe_point('westfluegel', vj, 'Vor der Leere', at=(VJ.cx + 0.5, VJ.cy + 0.5))
safe_point('treppe', ST, 'Treppe', at=(S_.x0 + 1.5, S_.cy))

# V13.3: Varianten der haeufigen Klassen (Blender, name@N.glb) zufaellig gewaehlt; Anordnung unveraendert
VARIANTS = {'bookcase_aisle': 3, 'bookcase_aisle_b': 3, 'library_wall': 3, 'candelabrum': 2, 'books_fallen': 3,
            'ceiling_arms': 2, 'painting_renaissance_a': 2, 'painting_renaissance_b': 2}
vrng = random.Random(53031)
for o in m.objects:
    if o['model'] in VARIANTS:
        v_ = vrng.randrange(VARIANTS[o['model']])
        if v_:
            o['variant'] = v_

m.save()
