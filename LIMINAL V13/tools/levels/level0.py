"""Level 0 - "Die Vorzimmer": Entwurf der festen Karte. V12: LEVELSPEC_0.md (Quelle), LOOK_0.md (Gestaltung).

Grundlage (Wikidot "Level 0 - Threshold", Revision 50 vom 06.10.2026, eigene Worte): ein zufaellig
gegliedertes Labyrinth aus Raeumen, Fluren und Treppen wie die Hinterzimmer eines Bueros oder Ladens; gelbe
Tapete, feuchter Teppich, brummende Leuchtstoffroehren; keine Tueren, keine Moebel. Dazu Bogenraeume (blasse
Waende, Bogen-Durchgaenge, an Uebergaengen, die ruhigsten Orte), Saeulenfelder, Lochfelder (Gruben im
Raster, ohne Boden), Dunkelzonen (rauhe Waende, Stille, knoecheltiefe Fluessigkeit), rote Raeume (meiden) und
selten ein kleiner Raum in Manila-Toenen. Ausgang: eine flackernde Wand nach Level 1.

Gestalt (Draufsicht, Norden oben = groesseres y):

    +------+--+--------+-------------------+----------------+
    |      |F |        |  Rote Raeume (Sackgasse)           |
    | West |l | West   +-------------------+                |
    |  A   |u |  B     |  Sued (+0,85 m,   |   Ost          |
    |      |r |        |  Treppen)         |                |=== Naht (flackernde Wand) ===> Level 1
    |Start |  | Manila +-------------------+                |
    |      |  |        |  Boegen           |                |
    |      |  |        +-------------------+----------------+
    |      |  |        |  Saeulenfeld      |  Dunkelzone    |
    |      |  |        +-------------------+  (nass, still) |
    |      |  |        |  Lochfeld         |                |
    +------+--+--------+-------------------+----------------+

Keine erfundenen Raumfunktionen: es gibt nur Abschnitte des Labyrinths, Flure und die belegten Sonderbereiche.
Bewusste Abweichungen und eigene Ergaenzungen stehen in LEVELSPEC_0.md.

V12 (LOOK_0.md): kein langer gerader Roehrenflur - der Flur verspringt alle 7-12 m und wechselt die Breite;
Deckenhoehen 2,7-3,1 m je Raum; Rasterleuchten (Blender-Modell troffer) exakt im 0,6-m-Raster der Decke, in
Reihen je Raum, einige tot oder flackernd; Luftauslaesse in der Decke; Boegen als Modell mit Laibung und
Kaempfer; die Dunkelzone ist abgesenkt und steht knoecheltief unter Wasser.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from mapkit import Map, link_regions, partition_rooms  # noqa: E402

m = Map('level0', 176, 134, seed=1001)
rng = m.rng


def var_at(x, y, n):
    """V13.3: Modellvariante aus der Lage (PROPS_Level0.md) - nicht aus rng, damit die Karte sonst gleich bleibt."""
    return int(abs(math.sin(x * 12.9898 + y * 78.233)) * 43758.5453) % n


def yellow_type(w, h, r):
    if min(w, h) <= 3 and max(w, h) >= 8:
        return 'flur'
    if w * h >= 110:
        return 'halle'
    return 'zimmer'


def segmented(x0, y0, x1, y1, zone='vorzimmer', floor=0.0, extra=0.55):
    """Ein Abschnitt des Labyrinths: grosse Teile (4-14 m), zwischen fast allen Nachbarn breite Durchbrueche
    bis zur Decke (die Rasterdecke laeuft durch), oft zwei je Wand - Raeume fliessen ineinander, statt als
    geschlossene Zimmer mit einer Tuer aneinanderzuhaengen."""
    rooms = partition_rooms(m, x0, y0, x1, y1, zone, yellow_type, min_leaf=4, max_leaf=14, height=(2.7, 3.1),
                            stop_chance=0.25, loop_chance=0.9, width=(3, 5), floor=floor)  # V13.3: breitere Durchbrueche, tiefere Blicke
    have = {(d['a'], d['b']) for d in m.doors} | {(d['b'], d['a']) for d in m.doors}
    for a in rooms:
        for b in rooms:
            if a >= b:
                continue
            wl = m._wall(a, b)
            if not wl or wl[2] != 1:
                continue
            span = wl[4] - wl[3]
            if span < 4:
                continue
            if (a, b) not in have and rng.random() < extra:
                m.opening(a, b, width=rng.randint(2, min(4, span - 1)))
            elif span >= 9 and rng.random() < extra * 0.6:  # eine zweite Luecke: ein Wandstueck bleibt stehen
                m.opening(a, b, width=2, at=wl[3] + 1.5 + rng.random() * (span - 3))
    return rooms


# --- West: zwei Labyrinth-Abschnitte und dazwischen der lange Flur ----------------------------------------
westA = segmented(2, 2, 24, 132)
# V12 (LOOK_0.md): der lange Flur ist ein Zickzack - Abschnitte von 7-12 m wechseln zwischen drei Spuren
# (x 25, 28, 31), verbunden durch kurze Querstuecke; man sieht nie weiter als einen Abschnitt. Je Abschnitt eine
# eigene Breite und Deckenhoehe. Die Restflaechen neben den Spuren werden Nischen zum Labyrinth hin.
flur_segs = []
lanes = (25, 28, 31)
fy, lane = 4, 1
while fy < 124:
    fy1 = min(128, fy + rng.randint(7, 12))
    fw = rng.choice((2, 3))
    fx = lanes[lane]
    seg = m.room(fx, fy, fx + fw, fy1, 'flur', 'vorzimmer', 0.0, rng.choice((2.7, 2.8, 2.9)),
                 label='')  # V13.3: keine Benennung (Labyrinth ohne Wahrzeichen)
    if flur_segs:
        m.opening(flur_segs[-1], seg, width=2)
    flur_segs.append(seg)
    if fy1 >= 124:
        break
    # Querstueck zur naechsten Spur (2 m tief), dann weiter nach Norden
    nxt = rng.choice([l for l in range(3) if l != lane])
    cx0, cx1 = min(lanes[lane], lanes[nxt]), max(lanes[lane], lanes[nxt]) + 3
    cross = m.room(cx0, fy1 + 1, cx1, fy1 + 3, 'flur', 'vorzimmer', 0.0, 2.7)
    m.opening(seg, cross, width=2)
    flur_segs.append(cross)
    lane, fy = nxt, fy1 + 4
flur = flur_segs[len(flur_segs) // 2]
westB = segmented(35, 2, 58, 132)
link_regions(m, westA, flur_segs, 6, width=(2, 2))
link_regions(m, westB, flur_segs, 6, width=(2, 2))

# --- Manila: ein kleiner Raum in Manila-Toenen, versteckt im West-B-Abschnitt -------------------------------
manila = min((r for r in westB if 12 <= m.R(r).w * m.R(r).h <= 40), key=lambda r: abs(m.R(r).cy - 40))
M_ = m.R(manila)
M_.type, M_.zone = 'manila', 'manila'
M_.kw['label'] = ''  # V13.3: Manila-Raum (Wiki) ohne hervorgehobenen Namen

# --- Nord: das Lochfeld (Gruben im Raster, kein Boden), ringsum Labyrinth ---------------------------------
loch = m.room(66, 10, 95, 32, 'lochfeld', 'loecher', 0.0, 3.0, label='Lochfeld')
nl, nr = segmented(59, 2, 65, 39), segmented(96, 2, 104, 39)
nt, nb = segmented(66, 2, 95, 9), segmented(66, 33, 95, 39)
nord = nl + nr + nt + nb
for strip in (nl, nr, nt, nb):  # das Lochfeld ist von allen Seiten zugaenglich
    link_regions(m, [loch], strip, 1, width=(2, 3))
for a, b in ((nl, nt), (nl, nb), (nr, nt), (nr, nb)):
    link_regions(m, a, b, 1, width=(2, 3))
for gx in range(69, 93, 4):
    for gy in range(13, 30, 4):
        m.block(loch, gx, gy, gx + 1 + (1 if rng.random() < 0.3 else 0), gy + 1, -14.0, hidden=False)
m.transition('l0_loch', 'hole', '', 80.5, 21.0, 0.0, rect=[66, 10, 95, 32])

# --- Saeulenfeld: weite Halle, Stuetzen alle 4 m -----------------------------------------------------------
sf = m.room(59, 40, 104, 80, 'saeulenhalle', 'saeulen', 0.0, 3.6, label='Säulenfeld')
# V12 (Wikidot: Stuetzen im Gitter, durch das Verschieben ungleichmaessig): Gitter 4 m, jede Stuetze um bis zu
# eine Kachel versetzt, etwa jede achte fehlt, einige sind doppelt breit; raumhohe, tapezierte Bloecke
for gx in range(62, 102, 4):
    for gy in range(43, 78, 4):
        if rng.random() < 0.12:
            continue
        px, py = gx + rng.choice((0, 0, 0, 1, -1)), gy + rng.choice((0, 0, 0, 1, -1))
        wx, wy = (2, 1) if rng.random() < 0.12 else ((1, 2) if rng.random() < 0.08 else (1, 1))
        if 60 <= px and px + wx <= 103 and 41 <= py and py + wy <= 79:
            m.block(sf, px, py, px + wx, py + wy, 3.6, hidden=False)

# --- Boegen: blasse Raeume, Bogen-Durchgaenge (Sturz 2,35 m, Bogen als Ausstattung) ------------------------
boegen = []
x = 59
while x + 6 <= 104:
    boegen.append(m.room(x, 81, x + 6, 90, 'bogen', 'boegen', 0.0, 3.2, label='Bögen' if not boegen else ''))
    x += 7


def arch_door(a, b, width=3, at=None):
    # V13.3 Abnahme: Boden und Laibung wie im Bogenraum (Teppich, Putz) statt der hellen Schwelle einer Tuer -
    # style 'room_a'/'room_b': die Flaechen des Raums a bzw. b der Tuer (der Bogenraum, auch am Saeulenfeld)
    d = m.doors[m.door(a, b, 'wide', width, at=at, top=2.35)]
    d['style'] = 'room_a' if d['a'] in boegen else 'room_b'
    x0, y0, x1, y1 = d['rect']
    yaw = 0.0 if d['axis'] == 0 else math.pi / 2
    w = x1 - x0 if d['axis'] == 1 else y1 - y0
    # V12: Segmentbogen mit Laibung (1 m Wandstaerke) und Kaempferprofil, Blender (tools/blender/models/models_l0.py)
    m.obj(f'arch{max(2, min(4, w))}', (x0 + x1) / 2, (y0 + y1) / 2, m.R(a).floor, yaw)


for a, b in zip(boegen, boegen[1:]):
    arch_door(a, b, 3)
for b in boegen[1::2]:
    arch_door(sf, b, 3)

# --- Sued: eine Stufe hoeher (Treppen an den Durchgaengen) --------------------------------------------------
sued = segmented(59, 91, 104, 132, floor=0.85)

# --- Dunkelzone: nasser Teppich, kein Licht, rauhe Waende, knoecheltiefe Fluessigkeit ----------------------
# V12 (Wikidot Rev. 50): Boeden dort teils abgesenkt und oft knoecheltief mit Fluessigkeit gefuellt - 25 cm tiefer,
# Wasser 12 cm hoch (Stufen an den Durchbruechen setzt die Engine)
dunkel = partition_rooms(m, 105, 2, 142, 44, 'dunkel', lambda w, h, r: 'dunkelraum', min_leaf=4, max_leaf=12,
                         height=(2.7, 3.0), loop_chance=0.4, floor=-0.25, water=-0.13)  # V13.3: Decke >= 2,7 m

# --- Ost --------------------------------------------------------------------------------------------------
ost = segmented(105, 45, 142, 110)

# --- Rote Raeume: eine Sackgasse, die am Verstand zehrt (meiden) -------------------------------------------
rot = partition_rooms(m, 105, 111, 142, 132, 'rot', lambda w, h, r: 'roter_raum', min_leaf=4, max_leaf=10,
                      height=(2.8, 3.0))
# (Leuchten: siehe "Rasterleuchten" unten - in den roten Raeumen rote Scheiben)

# --- Naht zu Level 1: die Tapete bekommt Flecken, Beton kommt durch, eine Wandstelle flackert ---------------
naht = [m.room(143, 62, 151, 66, 'flur', 'vorzimmer', 0.0, 2.9)]
naht.append(m.room(152, 62, 160, 66, 'naht', 'naht0', 0.0, 2.9))
naht.append(m.room(161, 62, 167, 66, 'naht', 'naht1', 0.0, 2.9))
naht.append(m.room(168, 62, 174, 66, 'naht', 'naht1', 0.0, 3.0, flicker=2, label='Naht'))
for a, b in zip(naht, naht[1:]):
    m.opening(a, b, width=2)
# (Leuchten der Naht: Rasterleuchten unten; die letzte Kammer flackert)
m.transition('l0_l1', 'seam', 'level1', 168.5, 64.0, math.pi, rect=[171, 62, 174, 66])

# --- Verbindungen zwischen den Bereichen ---------------------------------------------------------------------
link_regions(m, westB, nord, 2)
link_regions(m, westB, [sf], 2, width=(2, 3))
link_regions(m, westB, [boegen[0]], 1, width=(2, 2))
link_regions(m, westB, sued, 2)
link_regions(m, nord, [sf], 3, width=(2, 3))
link_regions(m, boegen, sued, 2)
link_regions(m, nord, dunkel, 1)
link_regions(m, [sf], ost, 2, width=(2, 3))
link_regions(m, sued, ost, 2)
link_regions(m, ost, dunkel, 2)
link_regions(m, ost, rot, 1)
link_regions(m, ost, [naht[0]], 1, width=(2, 2))

# --- Start: ein mittlerer Abschnitt im Westen -------------------------------------------------------------------
start = min((r for r in westA if m.R(r).type == 'zimmer' and m.R(r).w >= 5 and m.R(r).h >= 5),
            key=lambda r: math.hypot(m.R(r).cx - 14, m.R(r).cy - 70))
S = m.R(start)
m.start = (S.cx, S.cy, 0.0)

# --- Ausstattung: Steckdosen und Lueftungsgitter nur an echten Wandflaechen ----------------------------------
yellow = westA + westB + flur_segs + nord + sued + ost
for r in yellow:
    R = m.R(r)
    if R.type == 'manila':
        continue
    faces = m.wall_faces(r, margin=1)
    if not faces or rng.random() > 0.5:
        continue
    fx, fy, fyaw = rng.choice(faces)
    m.obj('outlet', fx, fy, R.floor + 0.32, fyaw, tag='deco', variant=var_at(fx, fy, 3))
    if R.height >= 2.85 and rng.random() < 0.35:
        vx, vy, vyaw = rng.choice(faces)
        if math.hypot(vx - fx, vy - fy) > 2.0:
            m.obj('vent_grille', vx, vy, R.floor + R.height - 0.45, vyaw, tag='deco', variant=var_at(vx, vy, 2))
for r in dunkel:
    R = m.R(r)
    for k in range(1 + int(R.w * R.h / 40)):
        m.spawn('deco', R.x0 + 1 + rng.random() * (R.w - 2), R.y0 + 1 + rng.random() * (R.h - 2), 0.0,
                rng.random() * 6.28, set='puddle', weight=0.8)


# --- Funde: wenige, an der Wand abgestellt, wo man rastet (Boegen, Manila, Sackgassen, Dunkelzone) ----------
def wall_spot(r, inward=0.35):
    faces = m.wall_faces(r, margin=1)
    if not faces:
        return None
    fx, fy, fyaw = rng.choice(faces)
    return fx + math.cos(fyaw) * inward, fy + math.sin(fyaw) * inward, fyaw


doors_of = {}
for d in m.doors:
    doors_of.setdefault(d['a'], []).append(d)
    doors_of.setdefault(d['b'], []).append(d)
dead_ends = [r for r in yellow if len(doors_of.get(r, [])) == 1 and m.R(r).type != 'flur']
spots = [(r, 'bogen') for r in boegen] + [(manila, 'manila')] + [(r, 'sackgasse') for r in rng.sample(dead_ends, min(14, len(dead_ends)))]
spots += [(r, 'dunkel') for r in rng.sample(dunkel, 3)]
for r, why in spots:
    p = wall_spot(r)
    if p:
        m.spawn('item', p[0], p[1], m.R(r).floor, p[2] + math.pi, tags=['floor', 'wall', why], group=r)
for r in rng.sample([r for r in yellow + boegen if m.R(r).type != 'flur'], 10):
    p = wall_spot(r, 0.5)
    if p:
        m.spawn('note', p[0], p[1], m.R(r).floor, p[2], tags=['floor'])
# die Gestalt am Ende langer Flure (greift nie an)
for r in [r for r in yellow if m.R(r).type == 'flur'][::3] + flur_segs[::3]:
    R = m.R(r)
    end = (R.cx, R.y1 - 1.5) if R.h > R.w else (R.x1 - 1.5, R.cy)
    m.spawn('entity', end[0], end[1], R.floor, role='gestalt', tags=['corridor_end'])


# --- Sicherungspunkte (hier toetet nichts, aber der Verstand kann versagen) -----------------------------------
def safe_point(cid, ri, yaw, label, styled=True, at=None):
    # V13.3 Abnahme Level 0: ohne die gruene Sicherheitsleuchte mit Notausgang-Piktogramm an der Decke - die Seite
    # nennt keine Schilder (LOOK_0 "Nicht", STRUKTURTYP: 0), und jede Leuchte waere ein Wahrzeichen (LEVEL0_STANDARD 2).
    # Der Sicherungspunkt selbst bleibt (Speichern, Meldung beim Vorbeigehen).
    R = m.R(ri)
    x, y = at if at else (R.cx, R.cy)
    m.checkpoint(cid, x, y, yaw, label, lit_by_style=styled)


safe_point('start', start, 0.0, 'Erster Raum')
safe_point('flur', flur, math.pi / 2, 'Flur', at=(m.R(flur).cx, m.R(flur).cy))  # V13.3: ohne "Der lange" (STRUKTURTYP: kein Wahrzeichen)
safe_point('loch', min(nord, key=lambda r: math.hypot(m.R(r).cx - 63, m.R(r).cy - 21)), 0.0, 'Vor dem Lochfeld')
safe_point('saeulen', sf, 0.0, 'Säulenfeld')
safe_point('boegen', boegen[len(boegen) // 2], 0.0, 'Bögen')
safe_point('ost', min(ost, key=lambda r: math.hypot(m.R(r).cx - 140, m.R(r).cy - 64)), 0.0, 'Östliche Räume')
safe_point('naht', naht[1], 0.0, 'Wo die Tapete endet', styled=False)

# --- Rasterleuchten (LOOK_0.md) ----------------------------------------------------------------------------------
# Die Rasterdecke hat 0,6-m-Felder (ceiling_office, 3,6 m Textur): eine Leuchte 0,6 x 1,2 m liegt mit der langen
# Seite ueber zwei Feldern, ihre Mitte auf einer Rasterlinie (lange Achse) bzw. in Feldmitte (kurze Achse).
# Reihen je Raum mit 2,4-3,6 m Abstand und eigenem Versatz; etwa jede neunte Leuchte ist tot, jeder zwoelfte Raum
# flackert. Kein Licht in der Dunkelzone. Die Engine plant fuer Raeume mit eigenen Leuchten nichts dazu.
G = 0.6
FLUO = (1.0, 0.99, 0.90)


VARIANTS = {'troffer': 3, 'troffer_off': 2, 'troffer_red': 2}


def troffers(r, along=None, step_a=(4, 6), step_c=(4, 5), inten=0.62, radius=4.2, model='troffer', color=FLUO,
             dead=0.11):
    R = m.R(r)
    top = R.floor + R.height
    along = along or ('x' if R.w >= R.h else 'y')
    sa, sc = rng.randint(*step_a), rng.randint(*step_c)  # Abstand in Rasterfeldern
    # Rasterlinien innerhalb des Raums mit mindestens 0,6 m (bzw. 0,9 m quer) Abstand zur Wand
    a0, a1 = (R.x0, R.x1) if along == 'x' else (R.y0, R.y1)
    c0, c1 = (R.y0, R.y1) if along == 'x' else (R.x0, R.x1)
    alines = [k * G for k in range(math.ceil((a0 + 1.2) / G), math.floor((a1 - 1.2) / G) + 1)]
    cmids = [k * G + G / 2 for k in range(math.ceil((c0 + 0.6) / G), math.floor((c1 - 1.2) / G) + 1)]
    if not alines or not cmids:
        return 0
    off_a, off_c = rng.randrange(sa), rng.randrange(sc)
    a_sel = alines[off_a::sa] or alines[len(alines) // 2:len(alines) // 2 + 1]
    c_sel = cmids[off_c::sc] or cmids[len(cmids) // 2:len(cmids) // 2 + 1]
    # quer zentrieren, wenn nur eine Reihe passt
    if len(c_sel) == 1:
        c_sel = [cmids[len(cmids) // 2]]
    n = 0
    yaw = 0.0 if along == 'x' else math.pi / 2
    for a in a_sel:
        for c in c_sel:
            x, y = (a, c) if along == 'x' else (c, a)
            if m.blocked(x, y) or m.blocked(x - 0.6, y) or m.blocked(x + 0.6, y):
                continue
            if rng.random() < dead:
                m.obj('troffer_off', x, y, top, yaw, tag='deco', variant=var_at(x, y, VARIANTS['troffer_off']))
                continue
            m.obj(model, x, y, top, yaw, tag='light', variant=var_at(x, y, VARIANTS.get(model, 1)))
            m.light(r, x, y, 0, inten, radius, z=top - 0.03, panel=False, color=color)
            n += 1
    return n


def diffusers(r, count):
    R = m.R(r)
    top = R.floor + R.height
    for _ in range(count):
        for _try in range(8):
            x = (math.floor(R.x0 / G) + 2 + rng.randrange(max(1, int(R.w / G) - 4))) * G + G / 2
            y = (math.floor(R.y0 / G) + 2 + rng.randrange(max(1, int(R.h / G) - 4))) * G + G / 2
            if not (R.x0 + 0.6 < x < R.x1 - 0.6 and R.y0 + 0.6 < y < R.y1 - 0.6) or m.blocked(x, y):
                continue
            if any(o['model'].startswith('troffer') and math.hypot(o['pos'][0] - x, o['pos'][1] - y) < 1.0 for o in m.objects[-40:]):
                continue
            m.obj('ceiling_diffuser', x, y, top, 0.0, tag='deco', variant=var_at(x, y, 3))
            break


lit_rooms = yellow + [manila] if manila not in yellow else yellow
for r in yellow:
    R = m.R(r)
    if R.type == 'flur':
        troffers(r, step_a=(5, 7), step_c=(9, 9), inten=0.55)
    elif R.w * R.h >= 110:
        troffers(r, step_a=(5, 6), step_c=(5, 6), inten=0.66, radius=4.8)
    else:
        if troffers(r) == 0:  # schmale Kammer: eine Leuchte in der Mitte der Rasterlinien
            troffers(r, step_a=(99, 99), step_c=(99, 99))
    if rng.random() < 1 / 12:
        R.kw['flicker'] = 2
    if R.w * R.h >= 24 and rng.random() < 0.6:
        diffusers(r, 1 if R.w * R.h < 80 else 2)
for r in boegen:
    troffers(r, step_a=(6, 7), step_c=(9, 9), inten=0.45, radius=4.0, color=(1.0, 0.93, 0.80), dead=0.0)
troffers(sf, step_a=(6, 6), step_c=(6, 6), inten=0.62, radius=5.0, dead=0.07)
troffers(loch, step_a=(6, 6), step_c=(6, 6), inten=0.55, radius=5.0, dead=0.15)
for r in rot:
    troffers(r, inten=0.5, radius=4.0, model='troffer_red', color=(1.0, 0.22, 0.15), dead=0.0)
for r in naht:
    troffers(r, step_a=(99, 99), step_c=(99, 99), inten=0.5, radius=3.8, dead=0.0)

m.save()
