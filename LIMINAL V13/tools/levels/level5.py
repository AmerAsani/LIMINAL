"""Level 5 - "Das Hotel" (V12, LEVELSPEC_5.md / LOOK_5.md; Wikidot Level 5, Revision 184).

Draufsicht (x nach rechts, y nach unten):

                 Gang nach Level 5.3 (Norden)
                         ||
   +-- Fluegel Nord: Zimmer | Gang | Zimmer ---------------+     +-- Kesselraum (heiss, Oefen, Rohre) --+== Metro (903)
   |                                                         |     |                                       |
 [Aufzugshalle] ===== Hauptgang (Tapete, Vertaefelung, Leuchter, Sessel) ==========+                       |
   |   (Ankunft aus Level 3)            ||                                           |
   +-- Fluegel Sued: Zimmer | Gang | Zimmer     +--- Beverly Room (Ballsaal) ---+    Fenster (hell) -> Level 188
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level5', 172, 150, seed=1501)
rng = m.rng
FLUR_H, HALL_H, BALL_H, ZIMMER_H, KESSEL_H = 3.6, 4.4, 7.0, 3.0, 3.2
CANDLE, CHAND, BULB, FIRE = (1.0, 0.80, 0.55), (1.0, 0.86, 0.66), (1.0, 0.82, 0.58), (1.0, 0.5, 0.2)
C_FLUR, C_BALL, C_KESSEL, C_ROOMS = 1, 2, 3, 5
DOOR_PROPS = ['hotel_door_a', 'hotel_door_b', 'hotel_door_c', 'hotel_door_d', 'hotel_door_e', 'hotel_door_f']

# --- Aufzugshalle (Ankunft aus Level 3) ------------------------------------------------------------------------
lobby = m.room(4, 58, 22, 74, 'aufzugshalle', 'halle', 0.0, HALL_H, circuit=C_ROOMS, label='Aufzugshalle')
cabins = []
for k, y in enumerate((60, 65, 70)):
    cab = m.room(1, y, 3, y + 2, 'kabine', 'kabine', 0.0, 2.8, circuit=C_ROOMS, sealed=True)
    d = m.doors[m.door(cab, lobby, 'opening', 2)]
    x0, y0, x1, y1 = d['rect']
    m.obj('elevator_cage' if k != 1 else 'elevator_door', (x0 + x1) / 2, (y0 + y1) / 2, 0.0, math.pi, scale=[1, 1.35, 1])
    m.block(cab, 1, y, 3, y + 2, 2.8, hidden=True)
    cabins.append(cab)
m.start = (6.5, 66.0, 0.0)
m.transition('l3_l5', 'elevator', '', 6.5, 66.0, 0.0)  # Ankunft mit dem Aufzug aus Level 3

# --- Hauptgang und Fluegel ---------------------------------------------------------------------------------------
main = m.room(23, 64, 152, 68, 'flur', 'flur', 0.0, FLUR_H, circuit=C_FLUR, label='Hauptgang')
m.opening(lobby, main)
north = m.room(60, 16, 64, 63, 'flur', 'flur', 0.0, FLUR_H, circuit=C_FLUR)
south = m.room(60, 69, 64, 140, 'flur', 'flur', 0.0, FLUR_H, circuit=C_FLUR)
m.opening(north, main)
m.opening(main, south)
west_n = m.room(23, 26, 59, 29, 'flur', 'flur', 0.0, FLUR_H, circuit=C_FLUR)
m.opening(west_n, north, width=3)
east_s = m.room(65, 118, 120, 121, 'flur', 'flur', 0.0, FLUR_H, circuit=C_FLUR)
m.opening(south, east_s, width=3)
corridors = [main, north, south, west_n, east_s]

# --- Kesselraum (Wiki: eng, alte Maschinen, Rohre, heiss, Dampf, Oefen) --------------------------------------------
k_cor = m.room(120, 30, 160, 32, 'kesselgang', 'kessel', 0.0, KESSEL_H, circuit=C_KESSEL, label='Kesselraum')
k_link = m.room(145, 33, 147, 63, 'kesselgang', 'kessel', 0.0, KESSEL_H, circuit=C_KESSEL)
m.opening(k_cor, k_link)
m.door(k_link, main, 'door', 1, top=2.2)
k_rooms = []
for x in range(121, 157, 9):
    for side in ('n', 's'):
        y0, y1 = (21, 29) if side == 'n' else (33, 41)
        if side == 's' and x <= 147 and 145 <= x + 7:
            continue
        r = m.room(x, y0, x + 7, y1, 'kesselraum', 'kessel', 0.0, KESSEL_H + 0.6, circuit=C_KESSEL)
        m.door(k_cor, r, 'door', 2, top=2.2)
        k_rooms.append(r)
# Halt der Metro (eigene Ergaenzung): Nische am Ostende des Kesselgangs, ein Wagen steht darin
metro = m.room(161, 26, 169, 36, 'metro', 'metro', 0.0, 3.0, circuit=C_ROOMS, label='Metro')
m.opening(k_cor, metro, width=2)
m.transition('l5_903', 'door', 'level903', 165.0, 33.0, math.pi / 2, rect=[163, 33, 168, 36])

# --- Gang nach Level 5.3 (nahe dem Stuetzpunkt "Housekeeping", nicht dargestellt) -----------------------------------
to53 = m.room(40, 2, 43, 25, 'flur', 'flur_alt', 0.0, FLUR_H, circuit=C_FLUR, label='Gang zu 5.3')
m.opening(to53, west_n, width=3)
m.transition('l5_53', 'seam', 'level5_3', 41.5, 3.5, -math.pi / 2, rect=[40, 2, 43, 5])
m.transition('l53_l5', 'door', '', 41.5, 8.0, math.pi / 2)  # Rueckkehr durch die alte Holztuer der Bibliothek (Wiki: zum Eingang)

# --- Fenster nach Level 188 (Wiki 188: Fenster in Innen-Leveln fuehren hinein) ------------------------------------
fen = m.room(121, 112, 128, 121, 'fensternische', 'flur', 0.0, FLUR_H, circuit=C_FLUR)
m.opening(east_s, fen, width=3)
FN = m.R(fen)
m.obj('window_bright', FN.x1 - 0.005, FN.cy, 1.55, math.pi, tag='window')
m.light(fen, FN.x1 - 0.6, FN.cy, 0, 0.9, 5.0, z=1.6, panel=False, color=(1.0, 1.0, 0.98))
m.transition('l5_188', 'seam', 'level188', FN.x1 - 1.0, FN.cy, 0.0, rect=[FN.x1 - 1, int(FN.cy) - 1, FN.x1, int(FN.cy) + 1])
m.transition('l188_l5', 'seam', '', FN.x1 - 2.6, FN.cy, math.pi)  # Rueckkehr durch dasselbe Fenster (Wiki 188)

# --- Beverly Room (vor den Zimmern angelegt, damit diese seinen Platz frei lassen) ------------------------------------
ball = m.room(66, 69, 108, 103, 'ballsaal', 'ballsaal', 0.0, BALL_H, circuit=C_BALL, label='Beverly Room')
for at in (76, 98):
    m.door(main, ball, 'wide', 2, at=at, top=2.6)
m.door(south, ball, 'wide', 2, at=86, top=2.6)

# Zimmer beidseits der Gaenge (Wiki: die meisten leer, manche eingerichtet; viele Tueren verschlossen)
rooms_dark, rooms_furn, rooms_empty = [], [], []


def free(x0, y0, x1, y1):
    """Rechteck frei und durch mindestens eine Kachel Wand von allen Raeumen getrennt?"""
    for R in m.rooms:
        if x0 < R.x1 + 1 and R.x0 - 1 < x1 and y0 < R.y1 + 1 and R.y0 - 1 < y1:
            return False
    return 1 <= x0 and 1 <= y0 and x1 <= m.w - 1 and y1 <= m.h - 1


def hotel_rooms(cor, side, step=7, depth=6, every=1):
    """Zimmer an einer Gangseite: 'n'/'s' (Gang laeuft entlang x) oder 'w'/'e' (entlang y)."""
    C = m.R(cor)
    out = []
    if side in ('n', 's'):
        for x in range(C.x0 + 1, C.x1 - step + 1, step):
            y0, y1 = (C.y0 - 1 - depth, C.y0 - 1) if side == 'n' else (C.y1 + 1, C.y1 + 1 + depth)
            if not free(x, y0, x + step - 1, y1):
                continue
            out.append(m.room(x, y0, x + step - 1, y1, 'zimmer', 'zimmer', 0.0, ZIMMER_H, circuit=C_ROOMS))
    else:
        for y in range(C.y0 + 1, C.y1 - step + 1, step):
            x0, x1 = (C.x0 - 1 - depth, C.x0 - 1) if side == 'w' else (C.x1 + 1, C.x1 + 1 + depth)
            if not free(x0, y, x1, y + step - 1):
                continue
            out.append(m.room(x0, y, x1, y + step - 1, 'zimmer', 'zimmer', 0.0, ZIMMER_H, circuit=C_ROOMS))
    for r in out:
        u = rng.random()
        if u < 0.42:
            continue  # verschlossen: nur die Tuer als Wandteil (s. u.), der Raum bleibt unerreichbar
        m.door(cor, r, 'door', 1, top=2.15)
        (rooms_dark if u < 0.7 else (rooms_furn if u < 0.86 else rooms_empty)).append(r)
    return out


all_rooms = []
all_rooms += hotel_rooms(main, 'n', 7, 6)
all_rooms += hotel_rooms(main, 's', 7, 6)
all_rooms += hotel_rooms(north, 'w', 7, 6)
all_rooms += hotel_rooms(north, 'e', 7, 6)
all_rooms += hotel_rooms(west_n, 'n', 7, 6)
all_rooms += hotel_rooms(west_n, 's', 7, 6)
all_rooms += hotel_rooms(south, 'w', 7, 6)
all_rooms += hotel_rooms(east_s, 's', 7, 6)
for r in all_rooms:  # verschlossene Zimmer bleiben versiegelt
    if r not in rooms_dark + rooms_furn + rooms_empty:
        m.R(r).kw['sealed'] = True


# --- Ausstattung: Gaenge ---------------------------------------------------------------------------------------------
surfaces = []  # Tischplatten fuer Funde (Auftrag: wenige Items, auf Flaechen; Wiki: die Dinge des Levels liegen im Hauptgang)


def wall_runs(r, margin=1):
    by = {}
    for (fx, fy, fyaw) in m.wall_faces(r, margin=margin):
        by.setdefault(round(fyaw, 3), []).append((fx, fy))
    out = []
    for yaw, pts in by.items():
        along_y = abs(math.cos(yaw)) > 0.5
        pts.sort(key=lambda p: p[1] if along_y else p[0])
        cur = [pts[0]]
        for p in pts[1:]:
            q = cur[-1]
            if abs(((p[1] - q[1]) if along_y else (p[0] - q[0])) - 1.0) < 0.01:
                cur.append(p)
            else:
                out.append((yaw, cur))
                cur = [p]
        out.append((yaw, cur))
    return out


def cornice(r):
    """Deckengesims ringsum (auch ueber Tueren mit Sturz; Oeffnungen bis zur Decke bleiben frei)."""
    R = m.R(r)
    top = R.floor + R.height
    sides = [([(R.x0 + 0.005, y + 0.5) for y in range(R.y0, R.y1)], 0.0), ([(R.x1 - 0.005, y + 0.5) for y in range(R.y0, R.y1)], math.pi),
             ([(x + 0.5, R.y0 + 0.005) for x in range(R.x0, R.x1)], math.pi / 2), ([(x + 0.5, R.y1 - 0.005) for x in range(R.x0, R.x1)], -math.pi / 2)]
    for pts, yaw in sides:
        for (x, y) in pts:
            if m.wall_ok(x, y, yaw, rel_z=R.height - 0.13):
                m.obj('cornice', x, y, top, yaw, tag='deco')


# verschlossene Zimmertueren an den Gangwaenden vor den versiegelten Zimmern und dazwischen
for r in all_rooms:
    if not m.R(r).kw.get('sealed'):
        continue
    R = m.R(r)
    for cor in corridors:
        C = m.R(cor)
        if R.y1 == C.y0 - 1 or R.y0 == C.y1 + 1:  # Zimmer noerdlich/suedlich des Gangs
            y = C.y0 + 0.005 if R.y1 == C.y0 - 1 else C.y1 - 0.005
            yaw = math.pi / 2 if R.y1 == C.y0 - 1 else -math.pi / 2
            if C.x0 <= R.cx <= C.x1:
                m.obj(rng.choice(DOOR_PROPS), R.cx, y, 0.0, yaw, tag='deco')
        elif R.x1 == C.x0 - 1 or R.x0 == C.x1 + 1:
            x = C.x0 + 0.005 if R.x1 == C.x0 - 1 else C.x1 - 0.005
            yaw = 0.0 if R.x1 == C.x0 - 1 else math.pi
            if C.y0 <= R.cy <= C.y1:
                m.obj(rng.choice(DOOR_PROPS), x, R.cy, 0.0, yaw, tag='deco')
for cor in corridors + [to53, fen]:
    R = m.R(cor)
    cornice(cor)
    for yaw, run in wall_runs(cor, margin=0):
        c, s_ = math.cos(yaw), math.sin(yaw)
        for k, (fx, fy) in enumerate(run):
            if any(o['model'].startswith('hotel_door') and math.hypot(o['pos'][0] - fx, o['pos'][1] - fy) < 0.9 for o in m.objects[-60:]):
                continue
            m.obj('wainscot', fx, fy, 0.0, yaw, tag='deco')
            if k % (6 if cor == to53 else 4) == 2:  # zu 5.3 hin seltener und schwaecher (LOOK_5: gedunkelt)
                m.obj('candle_sconce', fx, fy, 2.05, yaw, tag='light')
                m.light(cor, fx + c * 0.3, fy + s_ * 0.3, 0, 0.4 if cor == to53 else 0.55, 4.5, z=2.2, panel=False, color=CANDLE)
            elif k % 9 == 5 and rng.random() < 0.6:
                m.obj('painting_portrait', fx, fy, 1.75, yaw, tag='deco')
    # Moebel in breiten Gaengen: Sofa, Sessel, Beistelltisch, Buecherregal (an der Wand)
    if min(R.w, R.h) >= 4 and cor in corridors:
        faces = [f for f in m.wall_faces(cor, margin=2)]
        rng.shuffle(faces)
        n = 0
        for (fx, fy, fyaw) in faces:
            if n >= max(2, (R.w + R.h) // 18):
                break
            if any(math.hypot(o['pos'][0] - fx, o['pos'][1] - fy) < 1.2 for o in m.objects if o['model'].startswith('hotel_door')):
                continue
            px, py = fx + math.cos(fyaw) * 0.55, fy + math.sin(fyaw) * 0.55
            if m.blocked(px, py):
                continue
            md = rng.choice(['sofa', 'armchair', 'bookcase', 'side_table', 'side_table'])
            if md == 'bookcase':  # V13.3: Ursprung des Regals liegt hinten - an die Wand, nicht 0,55 m davor (PROPS_Level5.md)
                px, py = fx + math.cos(fyaw) * 0.03, fy + math.sin(fyaw) * 0.03
            m.obj(md, px, py, 0.0, fyaw)
            if md == 'side_table':
                surfaces.append((px, py, 0.66, cor))
            n += 1

# --- Aufzugshalle: Marmor, Kronleuchter, Sessel ----------------------------------------------------------------------
R = m.R(lobby)
m.obj('chandelier_grand', R.cx + 1, R.cy, HALL_H, 0.0, tag='light')
m.light(lobby, R.cx + 1, R.cy, 0, 0.8, 10.0, z=HALL_H - 1.9, panel=False, color=CHAND)
cornice(lobby)
for yaw, run in wall_runs(lobby, margin=0):
    c, s_ = math.cos(yaw), math.sin(yaw)
    for k, (fx, fy) in enumerate(run):
        m.obj('wainscot', fx, fy, 0.0, yaw, tag='deco')
        if k % 4 == 2:
            m.obj('candle_sconce', fx, fy, 2.2, yaw, tag='light')
            m.light(lobby, fx + c * 0.3, fy + s_ * 0.3, 0, 0.5, 4.5, z=2.35, panel=False, color=CANDLE)
for cab in cabins:  # Licht ueber jedem Aufzug (Anzeige, Gitter)
    C = m.R(cab)
    m.light(lobby, R.x0 + 0.6, C.cy, 0, 0.45, 3.5, z=3.0, panel=False, color=CHAND)
for x, y in ((R.x1 - 3, R.y0 + 2), (R.x1 - 3, R.y1 - 2)):
    m.obj('armchair', x, y, 0.0, math.pi)

# --- Beverly Room: Kronleuchter, Mahjong-Tisch, Grammophon, Tueren ringsum, Schild -----------------------------------------
B = m.R(ball)
m.obj('chandelier_grand', B.cx, B.cy, BALL_H, 0.0, tag='light')
m.light(ball, B.cx, B.cy, 0, 1.3, 16.0, z=BALL_H - 2.0, panel=False, color=CHAND)
cornice(ball)
m.obj('mahjong_table', B.cx, B.cy, 0.0, 0.3)
m.block(ball, int(B.cx), int(B.cy), int(B.cx) + 1, int(B.cy) + 1, 0.8, hidden=True)
for (cx, cy) in ((B.x0 + 8, B.y0 + 8), (B.x1 - 8, B.y0 + 8), (B.x0 + 8, B.y1 - 8), (B.x1 - 8, B.y1 - 8)):
    m.obj('chandelier_grand', cx, cy, BALL_H, 0.0, tag='light')
    m.light(ball, cx, cy, 0, 0.8, 11.0, z=BALL_H - 2.0, panel=False, color=CHAND)
gx, gy = B.x0 + 1.0, B.y0 + 1.0
m.obj('gramophone', gx + 0.4, gy + 0.4, 0.0, math.pi / 4, tag='music')
for (fx, fy, fyaw) in ((B.x0 + 0.005, B.y0 + 1.5, 0.0), (B.x0 + 1.5, B.y0 + 0.005, math.pi / 2)):  # Ecke mit dem Grammophon
    m.obj('candle_sconce', fx, fy, 2.4, fyaw, tag='light')
    m.light(ball, fx + math.cos(fyaw) * 0.3, fy + math.sin(fyaw) * 0.3, 0, 0.5, 5.0, z=2.55, panel=False, color=CANDLE)
m.block(ball, B.x0, B.y0, B.x0 + 1, B.y0 + 1, 0.9, hidden=True)
# Wiki: "Tueren auf allen Seiten" - verschlossene Tueren ringsum, Vertaefelung, Wandleuchter
for yaw, run in wall_runs(ball, margin=1):
    for k, (fx, fy) in enumerate(run):
        if k % 3 == 1:
            m.obj(rng.choice(DOOR_PROPS), fx, fy, 0.0, yaw, tag='deco')
        else:
            m.obj('wainscot', fx, fy, 0.0, yaw, tag='deco')
            if k % 3 == 2:
                m.obj('candle_sconce', fx, fy, 2.4, yaw, tag='light')
                if k % 6 == 5:
                    m.light(ball, fx + math.cos(yaw) * 0.3, fy + math.sin(yaw) * 0.3, 0, 0.5, 5.0, z=2.55, panel=False, color=CANDLE)
# Schild "The Beverly Room" neben den Eingaengen (auf der Gangseite)
for d in m.doors:
    if ball in (d['a'], d['b']) and d['kind'] == 'wide':
        other = d['a'] if d['b'] == ball else d['b']
        O = m.R(other)
        x0, y0, x1, y1 = d['rect']
        if d['axis'] == 1:  # Tuer in einer waagrechten Wand
            y = y0 - 0.005 if O.y1 <= y0 else y1 + 0.005
            yaw = -math.pi / 2 if O.y1 <= y0 else math.pi / 2
            m.obj('placard_beverly', x1 + 0.6, y, 1.55, yaw, tag='deco')
        else:
            x = x0 - 0.005 if O.x1 <= x0 else x1 + 0.005
            yaw = math.pi if O.x1 <= x0 else 0.0
            m.obj('placard_beverly', x, y1 + 0.6, 1.55, yaw, tag='deco')

# --- Zimmer: eingerichtet (Wohnraeume), leer, dunkel (dort nisten Motten) -------------------------------------------------
for r in rooms_furn:
    R = m.R(r)
    faces = m.wall_faces(r, margin=1)
    if not faces:
        continue
    fx, fy, fyaw = rng.choice(faces)
    m.obj('hotel_bed', fx + math.cos(fyaw) * 1.1, fy + math.sin(fyaw) * 1.1, 0.0, fyaw)
    tx_, ty_ = fx + math.cos(fyaw) * 0.3 + math.cos(fyaw + math.pi / 2) * 1.4, fy + math.sin(fyaw) * 0.3 + math.sin(fyaw + math.pi / 2) * 1.4
    m.obj('side_table', tx_, ty_, 0.0, fyaw, variant=1)
    surfaces.append((tx_ + math.cos(fyaw) * 0.12, ty_ + math.sin(fyaw) * 0.12, 0.66, r))
    m.light(r, tx_, ty_, 0, 0.6, 5.5, z=1.0, panel=False, color=CANDLE)  # Tischlampe
    m.obj('wardrobe', *rng.choice([(f[0] + math.cos(f[2]) * 0.35, f[1] + math.sin(f[2]) * 0.35, 0.0, f[2]) for f in faces]))
    if rng.random() < 0.5:  # Sessel in der Ecke gegenueber, zum Bett gedreht
        ax, ay = R.cx - math.cos(fyaw) * 1.6 + math.cos(fyaw + math.pi / 2) * 1.4, R.cy - math.sin(fyaw) * 1.6 + math.sin(fyaw + math.pi / 2) * 1.4
        if not m.blocked(ax, ay):
            m.obj('armchair', ax, ay, 0.0, math.atan2(fy - ay, fx - ax))
for r in rooms_empty:
    R = m.R(r)
    m.light(r, R.cx, R.cy, 0, 0.4, 3.5, z=R.floor + R.height - 0.1, panel=True, color=CANDLE)
    m.obj('side_table', R.cx, R.cy, 0.0, 0.0)
    surfaces.append((R.cx, R.cy, 0.66, r))

# --- Kesselraum: Oefen, Rohre mit Ventilen, Dampf, Lecks (Mandelwasser), Wartungsaufzug ----------------------------------
def foot_block(r, fx, fy, yaw, depth, half, h):
    """Unsichtbarer Block unter einem Wandgeraet (Rueckseite an der Wand, depth in den Raum, +-half breit)."""
    c, s_ = math.cos(yaw), math.sin(yaw)
    xs = [fx + c * d + -s_ * w for d in (0.0, depth) for w in (-half, half)]
    ys = [fy + s_ * d + c * w for d in (0.0, depth) for w in (-half, half)]
    m.block(r, math.floor(min(xs) + 0.01), math.floor(min(ys) + 0.01), math.ceil(max(xs) - 0.01), math.ceil(max(ys) - 0.01), h, hidden=True)


# Wiki: meist Kesselraeume, einige Ofenraeume
for i, r in enumerate(k_rooms):
    R = m.R(r)
    faces = m.wall_faces(r, margin=1)
    far = [f for f in faces if (f[1] < R.cy - 2 if R.y0 < 30 else f[1] > R.cy + 2)]
    if far:
        fx, fy, fyaw = far[len(far) // 2]
        c, s_ = math.cos(fyaw), math.sin(fyaw)
        if i in (1, 4):
            m.obj('furnace', fx, fy, 0.0, fyaw, tag='machine')
            foot_block(r, fx, fy, fyaw, 1.7, 1.0, 2.3)
            m.light(r, fx + c * 1.9, fy + s_ * 1.9, 0, 0.7, 4.0, z=0.6, panel=False, color=FIRE)
        else:
            m.obj('boiler_tank', fx, fy, 0.0, fyaw, tag='machine')
            foot_block(r, fx, fy, fyaw, 2.55, 0.8, 2.4)
            m.light(r, fx + c * 2.9, fy + s_ * 2.9, 0, 0.22, 2.2, z=0.3, panel=False, color=FIRE)
    # Rohrbuendel an den Seitenwaenden (Wiki: verschlungene Rohre, Ablassventile; Mandelwasser leckt)
    for yaw, run in wall_runs(r, margin=1):
        if abs(math.sin(yaw)) > 0.5 or len(run) < 5:
            continue
        mid = len(run) // 2
        fx, fy = (run[mid - 2][0] + run[mid + 2][0]) / 2, (run[mid - 2][1] + run[mid + 2][1]) / 2
        m.obj(rng.choice(['pipe_bundle_a', 'pipe_bundle_b']), fx, fy, 0.0, yaw, tag='steam' if i % 3 == 0 else 'pipes')
        if rng.random() < 0.6:
            m.spawn('deco', fx + math.cos(yaw) * 0.45, fy + math.sin(yaw) * 0.45 + rng.uniform(-1.5, 1.5), 0.0, 0.0, set='puddle', weight=0.7)
    m.obj('hanging_bulb', R.cx, R.cy, R.floor + R.height, 0.0, tag='light')
    m.light(r, R.cx, R.cy, 0, 0.75, 6.0, z=R.floor + R.height - 0.71, panel=False, color=BULB)
for r in (k_cor, k_link):
    R = m.R(r)
    for yaw, run in wall_runs(r):
        for j in range(0, len(run) - 4, 5):
            fx, fy = (run[j][0] + run[j + 4][0]) / 2, (run[j][1] + run[j + 4][1]) / 2
            m.obj(rng.choice(['pipe_bundle_a', 'pipe_bundle_b']), fx, fy, 0.0, yaw, tag='steam' if j % 10 == 0 else 'pipes')
            if rng.random() < 0.5:
                m.spawn('deco', fx + math.cos(yaw) * 0.4, fy + math.sin(yaw) * 0.4, 0.0, 0.0, set='puddle', weight=0.7)
    along_x = R.w >= R.h
    for t in range(2, (R.w if along_x else R.h) - 1, 5):
        x, y = (R.x0 + t + 0.5, R.cy) if along_x else (R.cx, R.y0 + t + 0.5)
        m.obj('hanging_bulb', x, y, R.floor + R.height, 0.0, tag='light')
        m.light(r, x, y, 0, 0.6, 4.0, z=R.floor + R.height - 0.71, panel=False, color=BULB)
M_ = m.R(metro)
m.obj('metro_car_end', M_.cx, M_.cy + 0.5, 0.0, 0.0)
m.light(metro, M_.cx, M_.cy - 1.0, 0, 0.6, 5.0, z=2.6, panel=False, color=(1.0, 0.98, 0.9))

# --- Funde, Notizen, Wesen, Sicherungspunkte ---------------------------------------------------------------------------------


def wall_spot(r, inward=0.4):
    faces = [f for f in m.wall_faces(r, margin=1) if not m.blocked(f[0] + math.cos(f[2]) * inward, f[1] + math.sin(f[2]) * inward)]
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


for (x, y, z, r) in surfaces:
    m.spawn('item', x, y, z, rng.random() * 6.28, tags=['surface', 'side_table'], group=r)
for r in rng.sample(rooms_furn + rooms_empty + corridors, 8):
    p = wall_spot(r, 0.5)
    if p:
        m.spawn('note', p[0], p[1], 0.0, p[2], tags=['floor'])
def safe_point(cid, r, label, at=None):
    R = m.R(r)
    x, y = at if at else (R.cx, R.cy)
    if m.blocked(x, y):
        p = wall_spot(r, 1.0)
        x, y = p[0], p[1]
    m.checkpoint(cid, x, y, 0.0, label)
    # im Hotel eine Laterne mit gruenem Glas statt des Notlichtschilds (LOOK_5: keine modernen Schilder)
    m.obj('lantern_green', x, y, R.floor + R.height, 0.0, tag='emergency')
    m.light(r, x, y, 0, 0.25, 3.0, z=R.floor + R.height - 0.75, panel=False, color=(0.55, 1.0, 0.65))


safe_point('lobby', lobby, 'Aufzugshalle', at=(m.R(lobby).x1 - 4.0, m.R(lobby).cy))
safe_point('beverly', ball, 'Beverly Room', at=(m.R(ball).cx - 6, m.R(ball).cy))
for k, r in enumerate(rooms_furn[:3]):
    safe_point(f'zimmer{k}', r, 'Eingerichtetes Zimmer')
safe_point('metro', metro, 'Metro-Halt', at=(m.R(metro).cx - 2.5, m.R(metro).y0 + 1.0))
safe_point('kessel', k_rooms[0], 'Kesselraum')
safe_point('gang53', to53, 'Gang zu 5.3', at=(m.R(to53).cx, m.R(to53).y0 + 9.0))
safe_point('fenster', fen, 'Vor dem Fenster', at=(m.R(fen).x0 + 1.5, m.R(fen).cy))

for r in (main, south, east_s):  # selten ein Hound (Wiki: andere Wesen eher selten)
    R = m.R(r)
    x = R.x0 + 0.5 + rng.random() * (R.w - 1)
    y = R.y0 + 0.5 + rng.random() * (R.h - 1)
    if (math.hypot(x - m.start[0], y - m.start[1]) > 40 and not m.blocked(x, y)
            and all(math.hypot(x - c['pos'][0], y - c['pos'][1]) >= 16 for c in m.checkpoints)):
        m.spawn('entity', x, y, 0.0, role='hound', tags=['hotel'])

# V13.3: Varianten der haeufig gesetzten Klassen (PROPS_Level5.md). Eigener Zufall nach allen Platzierungen - die
# Anordnung bleibt gleich. Tueren: 6 Bauarten x 4 Varianten mit je eigener Nummer (24 Nummern ohne Muster, Wiki).
# Beistelltische: mit Lampe 1 oder 3, ohne Lampe 0 oder 2 (die Lichter der Zimmer haengen an den Lampen).
VARIANTS = {'hotel_door_a': 4, 'hotel_door_b': 4, 'hotel_door_c': 4, 'hotel_door_d': 4, 'hotel_door_e': 4, 'hotel_door_f': 4,
            'candle_sconce': 3, 'painting_portrait': 3, 'chandelier_grand': 2, 'sofa': 2, 'armchair': 2, 'wardrobe': 2, 'bookcase': 4}
vr = random.Random(1505)
for o in m.objects:
    if o['model'] in VARIANTS:
        v_ = vr.randrange(VARIANTS[o['model']])
    elif o['model'] == 'side_table':
        v_ = vr.choice((1, 3) if o.get('variant', 0) == 1 else (0, 2))
    else:
        continue
    o.pop('variant', None)
    if v_:
        o['variant'] = v_


# V13.3 Teil 4 (gameplay/hotel_l5.hpp, KREATUR_*.md): Orte der Wesen ohne Rolle und das Fleisch - eigener Zufall nach allen
# Platzierungen (die Anordnung oben bleibt gleich). Spawnpunkte kind "trace": ants (dunkelste Zimmerecke), woodlin (vor den
# Moebeln der Gaenge), watcher (Tueroeffnung dunkler/leerer Zimmer, Seite der lugenden Kante), office (Buero des Gastgebers:
# Mitte des Schreibtischs, Blick zur Tuer), meat (Steak vor Zimmertueren - Zimmerservice -, Dosen im Kesselraum).
hr = random.Random(5260)


def door_of(r):
    """Tuer des Zimmers r zu einem Gang: (Mitte auf der Gangwand, Blick in den Gang, Gang)."""
    for d in m.doors:
        if r not in (d['a'], d['b']):
            continue
        other = d['b'] if d['a'] == r else d['a']
        if other not in corridors:
            continue
        x0, y0, x1, y1 = d['rect']
        R, C = m.R(r), m.R(other)
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        if C.y0 >= R.y1 or C.y1 <= R.y0:  # Gang noerdlich/suedlich
            y = C.y0 if C.y0 >= R.y1 else C.y1
            return cx, y, (math.pi / 2 if C.y0 >= R.y1 else -math.pi / 2), other
        x = C.x0 if C.x0 >= R.x1 else C.x1
        return x, cy, (0.0 if C.x0 >= R.x1 else math.pi), other
    return None


# Kolonien: je Zimmer die Ecke, die am weitesten von Tuer und Lampe liegt (die dunkelste)
for r in rooms_dark + rooms_empty + rooms_furn:
    R = m.R(r)
    dd = door_of(r)
    if not dd:
        continue
    best = None
    for (cx, cy) in ((R.x0 + 0.3, R.y0 + 0.3), (R.x1 - 0.3, R.y0 + 0.3), (R.x0 + 0.3, R.y1 - 0.3), (R.x1 - 0.3, R.y1 - 0.3)):
        if m.blocked(cx, cy) or any(math.hypot(o['pos'][0] - cx, o['pos'][1] - cy) < 1.0 for o in m.objects if o['model'] in ('wardrobe', 'hotel_bed', 'side_table', 'armchair')):
            continue
        dist = math.hypot(cx - dd[0], cy - dd[1])
        if best is None or dist > best[0]:
            best = (dist, cx, cy)
    if best and (r in rooms_dark or hr.random() < 0.6):
        _, cx, cy = best
        m.spawn('trace', cx, cy, R.floor, math.atan2(R.cy - cy, R.cx - cx), set='ants')
# Woodlins: vor den Moebeln der Gaenge und der Aufzugshalle (Sofa, Sessel, Regal, Beistelltisch)
for o in list(m.objects):
    if o['model'] not in ('sofa', 'armchair', 'bookcase', 'side_table'):
        continue
    x, y = o['pos'][0], o['pos'][1]
    ri = m.at(x, y)
    if ri < 0 or ri not in corridors + [lobby]:
        continue
    yaw = o.get('yaw', 0.0)
    fx, fy = x + math.cos(yaw) * 0.55, y + math.sin(yaw) * 0.55
    if m.blocked(fx, fy):
        continue
    m.spawn('trace', fx, fy, 0.0, yaw, set='woodlin')
# Watchers: Tueroeffnungen dunkler und leerer Zimmer (nicht die eingerichteten - dort brennt Licht)
for r in rooms_dark + rooms_empty:
    dd = door_of(r)
    if dd:
        # 6 cm vor der Wandflucht im Gang (der Punkt muss in einem Raum liegen); das Spiel rechnet ihn zurueck (o0.06)
        m.spawn('trace', dd[0] + math.cos(dd[2]) * 0.06, dd[1] + math.sin(dd[2]) * 0.06, 0.0, dd[2], set='watcher', tags=[f'side{hr.choice((-1, 1))}', 'o0.06'])
# Buero des Gastgebers: ein leeres Zimmer am Hauptgang, weit vom Start; Schreibtisch zur Tuer, dahinter der abgewandte
# Ohrensessel, an der Rueckwand ein Bildnis (eigene Modelle; Kollision als Block)
cand = [r for r in rooms_empty if door_of(r) and door_of(r)[3] == main and math.hypot(m.R(r).cx - m.start[0], m.R(r).cy - m.start[1]) > 45]
if not cand:
    cand = [r for r in rooms_empty if door_of(r)]
if cand:
    off = hr.choice(cand)
    R = m.R(off)
    dx, dy, dyaw, _ = door_of(off)
    nx, ny = -math.cos(dyaw), -math.sin(dyaw)  # von der Tuer in das Zimmer
    depth = (R.h if abs(ny) > 0.5 else R.w)
    back = depth + 1.0  # Gangwand bis Rueckwand (eine Kachel Wand)
    deskx, desky = dx + nx * (back - 1.7), dy + ny * (back - 1.7)  # vor dem Beistelltisch in der Mitte
    face = dyaw  # der Schreibtisch sieht zur Tuer
    m.obj('e18_desk', deskx, desky, R.floor, face)
    m.block(off, int(math.floor(deskx - 0.8)), int(math.floor(desky - 0.8)), int(math.floor(deskx - 0.8)) + 2, int(math.floor(desky - 0.8)) + 2, 0.8, hidden=True)
    m.obj('e18_wingchair', deskx + nx * 0.85, desky + ny * 0.85, R.floor, face + math.pi)  # abgewandt: zur Rueckwand
    m.obj('e18_portrait', dx + nx * (back - 0.005), dy + ny * (back - 0.005), 1.7, face)
    m.spawn('trace', deskx + math.cos(face) * 0.9, desky + math.sin(face) * 0.9, R.floor, face, set='office', tags=['o0.9'])  # vor dem Tisch
# Fleisch: Tabletts des Zimmerservice vor verschlossenen Zimmertueren (Steak), Dosen in den Kesselraeumen
doors_deco = [o for o in m.objects if o['model'].startswith('hotel_door') and math.hypot(o['pos'][0] - m.start[0], o['pos'][1] - m.start[1]) > 20]
for o in hr.sample(doors_deco, min(5, len(doors_deco))):
    yaw = o.get('yaw', 0.0)
    x, y = o['pos'][0] + math.cos(yaw) * 0.32 + math.cos(yaw + math.pi / 2) * 0.35, o['pos'][1] + math.sin(yaw) * 0.32 + math.sin(yaw + math.pi / 2) * 0.35
    if not m.blocked(x, y):
        m.spawn('trace', x, y, 0.0, yaw + hr.uniform(-0.4, 0.4), set='meat', tags=['good'])
for r in hr.sample(k_rooms, min(3, len(k_rooms))):
    p = wall_spot(r, 0.35)
    if p:
        m.spawn('trace', p[0], p[1], m.R(r).floor, p[2] + hr.uniform(-0.6, 0.6), set='meat', tags=['poor'])
m.save()
