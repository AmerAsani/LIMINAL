"""Level 36 - "Timeless Airport" (V12, LEVELSPEC_36.md / LOOK_36.md; Wikidot Level 36, Revision 33). Das Ende.

Draufsicht (x nach rechts, y nach unten):

                 [Sackgasse: leere Schilder]   <- "EXIT" (nicht trauen)
          +====== Rundgang mit Laufbaendern ======================+   +== Gate-Halle ==+-- C30
          ||        Ladenzeile (Teppich)                          ||   |                +-- C31
          +====== Rundgang ===================================== =+===|                +-- C32 (die Wartenden)
          ||                                                           |                +-- C33  BOARDING -> Ende
   [Gepaeckausgabe] = [Check-in: Schalter, Abflugtafeln, BTA] ======== Gang =========== +-- C34
   Ankunft aus der Metro
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map  # noqa: E402

m = Map('level36', 172, 92, seed=3636)
rng = m.rng
COLD, WARM = (0.95, 0.98, 1.0), (1.0, 0.93, 0.82)
C_ALL = 1

BC = m.room(4, 60, 40, 84, 'gepaeck', 'halle', 0.0, 6.0, circuit=C_ALL, label='Gepaeckausgabe')
CI = m.room(41, 60, 101, 80, 'checkin', 'halle', 0.0, 8.0, circuit=C_ALL, label='Check-in')
m.opening(BC, CI, width=8)
N1 = m.room(60, 40, 66, 59, 'gang', 'gang', 0.0, 4.5, circuit=C_ALL)
m.opening(N1, CI, width=6)
RT = m.room(60, 10, 140, 16, 'gang', 'gang', 0.0, 4.5, circuit=C_ALL, label='Rundgang')
RL = m.room(60, 17, 66, 39, 'gang', 'gang', 0.0, 4.5, circuit=C_ALL)
RR = m.room(134, 17, 140, 39, 'gang', 'gang', 0.0, 4.5, circuit=C_ALL)
RB = m.room(67, 33, 133, 39, 'gang', 'gang', 0.0, 4.5, circuit=C_ALL)
m.opening(RT, RL, width=6)
m.opening(RT, RR, width=6)
m.opening(RL, RB, width=6)
m.opening(RB, RR, width=6)
m.opening(RL, N1, width=6)
S = m.room(67, 17, 133, 32, 'laeden', 'laeden', 0.0, 4.6, circuit=C_ALL, label='Ladenzeile')
for (r, at) in ((RT, 82), (RT, 118), (RB, 100)):
    m.door(r, S, 'opening', 6, at=at)
D = m.room(96, 2, 100, 9, 'sackgasse', 'sackgasse', 0.0, 4.5, circuit=C_ALL)
m.opening(RT, D, width=4)
G = m.room(142, 10, 150, 88, 'gatehalle', 'gang', 0.0, 5.0, circuit=C_ALL, label='Gates C')
m.door(RR, G, 'opening', 6)
K = m.room(102, 66, 141, 72, 'gang', 'gang', 0.0, 4.5, circuit=C_ALL)
m.opening(CI, K, width=6)
m.opening(K, G, width=6)
GATES = {}
for name, (y0, y1) in (('c30', (10, 22)), ('c31', (24, 36)), ('c32', (38, 56)), ('c33', (58, 70)), ('c34', (72, 86))):
    g = m.room(151, y0, 168, y1, 'gate', 'gate', 0.0, 4.5, circuit=C_ALL, label='Gate ' + name.upper())
    m.opening(G, g, width=8)
    GATES[name] = g

# Ankunft (Wiki: die Metro ist ein Eingang; Ankunft wie alle am Gepaeckband)
m.start = (24.0, 72.0, 0.0)
m.transition('l903_36', 'door', '', 24.0, 72.0, 0.0)


def lights_grid(r, step, intensity, radius, color, flicker_some=0.0):
    R = m.R(r)
    for x in range(R.x0 + step // 2, R.x1, step):
        for y in range(R.y0 + step // 2, R.y1, step):
            if not m.blocked(x + 0.5, y + 0.5):
                m.light(r, x + 0.5, y + 0.5, 0, intensity, radius, color=color)


def solid(r, x0, y0, x1, y1, h=1.0):
    m.block(r, int(math.floor(x0)), int(math.floor(y0)), int(math.ceil(x1)), int(math.ceil(y1)), h, hidden=True)


# --- Gepaeckausgabe: zwei Baender, Fundsachen ----------------------------------------------------------------------------------
for cy in (66.0, 78.0):
    m.obj('carousel_36', 18.0, cy, 0.0, 0.0, tag='machine')
    solid(BC, 12, cy - 1.8, 24, cy + 1.8, 1.0)
for y in (63.5, 67.5, 71.5, 75.5):  # Wiki: Lost and Found
    m.obj('luggage_pile', BC_x := m.R(BC).x0 + 0.42, y, 0.0, math.pi / 2)
    solid(BC, m.R(BC).x0, y - 1.5, m.R(BC).x0 + 1, y + 1.5, 2.3)
lights_grid(BC, 6, 0.7, 7.0, COLD)
m.obj('clock_36', m.R(BC).x1 - 0.005, 64.0, 3.6, math.pi, tag='deco')
m.obj('sign_36_a', 30.0, 72.0, 6.0, 0.0, tag='light')

# --- Check-in: Schalterreihe, Abflugtafeln, BTA-Stand ----------------------------------------------------------------------------
R = m.R(CI)
surfaces = []
for k, x in enumerate(range(R.x0 + 4, R.x1 - 6, 3)):
    m.obj('checkin_counter', x + 0.5, 74.5, 0.0, -math.pi / 2, tag='deco')
solid(CI, R.x0 + 3, 74, R.x1 - 5, 76, 1.1)
for x in (55.0, 71.0, 87.0):
    m.obj('departure_board', x, R.y0 + 0.005, 4.4, math.pi / 2, tag='light')
m.obj('bta_booth', 93.5, 63.6, 0.0, math.pi, tag='light')  # Tresen zur Halle (der Import spiegelt y)
solid(CI, 92, 62, 95, 64, 1.1)
m.light(CI, 93.5, 64.5, 0, 0.4, 5.0, z=3.0, panel=False, color=(0.75, 0.55, 1.0))
lights_grid(CI, 7, 0.75, 9.0, COLD)
m.obj('clock_36_b', R.x1 - 0.005, 63.5, 4.0, math.pi, tag='deco')

# --- Rundgang: Laufbaender, widerspruechliche Schilder, Sackgasse ---------------------------------------------------------------
TR = m.R(RT)
for k, x in enumerate(range(TR.x0 + 6, TR.x1 - 12, 16)):
    model = 'travelator_stopped' if k == 2 else 'travelator'
    m.obj(model, x + 6.0, TR.y0 + 1.5, 0.0, 0.0, tag='deco')
    solid(RT, x, TR.y0 + 1, x + 12, TR.y0 + 2, 1.0)
for k, x in enumerate(range(TR.x0 + 10, TR.x1 - 4, 18)):
    m.obj('sign_36_a' if k % 2 == 0 else 'sign_36_b', x + 0.5, TR.y1 - 1.5, TR.height, math.pi / 2, tag='light')
for r in (RL, RR, RB, N1, K):
    R_ = m.R(r)
    along_x = R_.w > R_.h
    for k, t in enumerate(range(4, (R_.w if along_x else R_.h) - 3, 14)):
        x, y = (R_.x0 + t + 0.5, R_.cy) if along_x else (R_.cx, R_.y0 + t + 0.5)
        m.obj('sign_36_b' if (k + r) % 3 == 0 else 'sign_36_a', x, y, R_.height, math.pi / 2 if along_x else 0.0, tag='light')
for r in (RT, RL, RR, RB, N1, K):
    lights_grid(r, 6, 0.6, 6.5, COLD)
m.obj('exit_sign_36', 101.5, TR.y0 + 0.005, 2.6, math.pi / 2, tag='light')  # Wiki: Ausgangsschildern nicht trauen
DR = m.R(D)
m.obj('sign_36_empty', DR.cx, DR.cy, DR.height, 0.0, tag='deco')  # Sackgasse: leeres Schild, kein Licht

# --- Ladenzeile (Teppich, Laeden wie in der Frontrooms-Welt) ---------------------------------------------------------------------
SR = m.R(S)
FRONTS = ['storefront_a', 'storefront_b', 'storefront_closed', 'storefront_c', 'storefront_closed', 'storefront_a']
k = 0
for x in range(SR.x0 + 4, SR.x1 - 4, 7):
    for (fy, yaw) in ((SR.y0 + 0.005, math.pi / 2), (SR.y1 - 0.005, -math.pi / 2)):
        if not m.wall_ok(x + 0.5, fy, yaw, rel_z=1.0) or not m.wall_ok(x - 2.5, fy, yaw) or not m.wall_ok(x + 3.5, fy, yaw):
            continue
        model = FRONTS[k % len(FRONTS)]
        m.obj(model, x + 0.5, fy, 0.0, yaw, tag='light' if 'closed' not in model else 'deco')
        if 'closed' not in model:
            m.light(S, x + 0.5, fy + math.sin(yaw) * 1.5, 0, 0.45, 5.0, z=2.6, panel=False, color=WARM)
        k += 1
lights_grid(S, 6, 0.55, 6.5, WARM)

# --- Gate-Halle und Gates ------------------------------------------------------------------------------------------------------
GR = m.R(G)
lights_grid(G, 6, 0.7, 7.0, COLD)
for y in (16.5, 40.5, 58.5):
    m.obj('clock_36' if y != 40.5 else 'clock_36_b', GR.x0 + 0.005, y, 3.2, 0.0, tag='deco')
for y in (50.5, 80.5):
    m.obj('water_fountain', GR.x0 + 0.005, y, 0.0, 0.0, tag='deco')
    m.obj('restroom_sign', GR.x0 + 0.005, y + 2.0, 2.4, 0.0, tag='deco')
for y in range(GR.y0 + 8, GR.y1 - 4, 16):
    m.obj('sign_36_a', GR.cx, y + 0.5, GR.height, 0.0, tag='light')
for name, g in GATES.items():
    R_ = m.R(g)
    m.obj(f'gate_sign_{name}', R_.x0 + 3.5, R_.y0 + 0.005, 3.1, math.pi / 2, tag='light')
    boarding = name == 'c33'
    m.obj('gate_door_boarding' if boarding else 'gate_door_closed', R_.x1 - 0.005, R_.cy, 0.0, math.pi, tag='light' if boarding else 'deco')
    m.obj('gate_counter', R_.x1 - 3.0, R_.cy - 3.0, 0.0, 0.0, tag='deco')
    solid(g, R_.x1 - 3.5, R_.cy - 4.0, R_.x1 - 2.5, R_.cy - 2.0, 1.1)
    rows = [(R_.x0 + 3.5, R_.cy - 3.0), (R_.x0 + 3.5, R_.cy + 2.0), (R_.x0 + 7.5, R_.cy - 3.0), (R_.x0 + 7.5, R_.cy + 2.0)]
    for (sx, sy) in rows:
        m.obj('seats_row', sx, sy, 0.0, 0.0, tag='deco')
        solid(g, sx - 0.3, sy - 1.3, sx + 0.3, sy + 1.0, 0.9)  # Tischchen am Ende frei: dort liegen Funde
        surfaces.append((sx, sy + 1.6, 0.65))
    lights_grid(g, 5, 0.65, 6.5, COLD)
    if boarding:
        m.light(g, R_.x1 - 0.8, R_.cy, 1, 0.6, 5.0, -1, 0, z=1.4, panel=False, color=(0.9, 1.0, 0.92))
C32 = m.R(GATES['c32'])
m.obj('camp_c32', C32.x0 + 12.0, C32.cy + 4.0, 0.0, 0.2, tag='music')  # Wiki: Stimmengewirr um Gate C32
C33 = m.R(GATES['c33'])
m.transition('l36_ende', 'end', '', C33.x1 - 1.0, C33.cy, 0.0, rect=[C33.x1 - 2, int(C33.cy) - 1, C33.x1, int(C33.cy) + 2])

# --- Funde, Notizen, Sicherungspunkte -------------------------------------------------------------------------------------------
rng.shuffle(surfaces)
for (x, y, z) in surfaces[:6]:
    m.spawn('item', x, y, z, 0.0, tags=['surface', 'table'])
for (x, y, z) in surfaces[6:8]:
    m.spawn('note', x, y, z, 0.0, tags=['surface', 'table'])


def safe_point(cid, r, label, x, y):
    R_ = m.R(r)
    m.checkpoint(cid, x, y, 0.0, label)
    m.obj('safety_light', x, y, R_.floor + R_.height, 0.0, tag='emergency')


safe_point('gepaeck', BC, 'Gepaeckausgabe', 30.0, 66.5)
safe_point('checkin', CI, 'Check-in', 70.0, 66.5)
safe_point('rundgang', RT, 'Rundgang', 100.5, 13.5)
safe_point('gates', G, 'Gates C', GR.cx, 64.5)
m.save()
