"""V13.3 Level 3: Stromfalle der Death Rats (KREATUR_DeathRat.md, Abschnitt Level 3; Wikidot Level 3 Rev. 18).

Die Level-3-Seite: die Death Rats bauen die ausgefeiltesten Fallen, die man dort kennt, immer mit primitiver Elektrik; sie
wirken nicht immer, sind aber eine ernste Gefahr. Wie eine Falle gebaut ist, sagt die Seite nicht - das hier ist Auslegung:
ein blanker Kupferdraht quer durch den Gang in Knoechelhoehe, an beiden Waenden an Naegeln mit Porzellanisolatoren; an der
linken Wand auf einem Brett die Quelle: eine Motorradbatterie, eine von Hand gewickelte Zuendspule auf einer Holzspule, ein
Federkontakt (Unterbrecher wie bei einem Weidezaun), eine rote Leuchte auf verdrilltem Draht, Kabel mit Isolierband, die Enden
abgenagt (Zahnspuren - so bauen Nager).

Knochen (Rattenskelett ent_rat, das Spiel stellt sie, creature_rat_l3.cpp): 0 Brett und Quelle (Wurzel, Gangquerung = +x),
6 linker Isolator, 9 rechter Isolator (beide zeigen mit dem Nagel nach -x in ihre Wand), 18-21 der Draht (vier Stuecke,
das Spiel dehnt sie auf die Gangbreite und laesst ihn durchhaengen; das Netz ist hier fertig gebaut, nicht ausgeduennt),
5 der Funke am Unterbrecher (sonst auf null verkleinert).
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (16000, 6000, 2000)
SKELETON = 'ent_rat'
ROOT, SPARK, ANCHOR_L, ANCHOR_R = 0, 5, 6, 9
WIRE0 = 18
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def tube_mesh(pts, bones_of, radius, sides, nb, twist=0.0, strands=1, strand_off=0.0):
    """Rohr (oder verdrillte Litzen) durch Punkte; Gewichte je Punkt an den Knochen des Abschnitts (an den Gelenken gemischt)."""
    V, T, W = [], [], []
    pts = [np.asarray(p, dtype=float) for p in pts]
    n = len(pts)
    for st in range(strands):
        base = len(V)
        for i, p in enumerate(pts):
            d = A.unit(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)])
            u = A.perp(d, Z)
            v = np.cross(d, u)
            ang0 = twist * i + st * 2 * math.pi / max(1, strands)
            c = p + (u * math.cos(ang0) + v * math.sin(ang0)) * strand_off
            b0, b1, f = bones_of(i)
            for k in range(sides):
                a = 2 * math.pi * k / sides
                V.append(c + (u * math.cos(a) + v * math.sin(a)) * radius)
                w = np.zeros(nb)
                w[b0] += 1.0 - f
                w[b1] += f
                W.append(w)
        for i in range(n - 1):
            for k in range(sides):
                a = base + i * sides + k
                b = base + i * sides + (k + 1) % sides
                c2 = a + sides
                d2 = b + sides
                T += [(a, c2, b), (b, c2, d2)]
        # Enden zu
        for end, sgn in ((0, 1), (n - 1, -1)):
            cidx = len(V)
            V.append(pts[end])
            b0, b1, f = bones_of(end)
            w = np.zeros(nb)
            w[b0] += 1.0 - f
            w[b1] += f
            W.append(w)
            for k in range(sides):
                a = base + end * sides + k
                b = base + end * sides + (k + 1) % sides
                T.append((cidx, b, a) if sgn > 0 else (cidx, a, b))
    return np.array(V), np.array(T, dtype=np.int64), np.array(W)


def build(J, bones):
    rng = np.random.default_rng(2424)
    J = np.asarray(J, dtype=float)
    nb = len(bones)
    # --- Quelle auf dem Brett (Wurzel; Wand hinten bei x < 0, der Gang liegt bei +x) -----------------------------------------
    board = [A.box(ROOT, (0.10, 0.03, 0.012), X, (0.11, 0.25, 0.012), 0.004, k=0.0)]
    board += [A.box(ROOT, (0.10, 0.03 + 0.12 * s, 0.025), X, (0.105, 0.004, 0.0015), 0.001, op='sub', k=0.0) for s in (-1, 1)]  # Fugen
    battery = [A.box(ROOT, (0.075, -0.06, 0.098), X, (0.055, 0.085, 0.074), 0.006, k=0.0)]
    battery += [A.box(ROOT, (0.075, -0.06 + 0.03 * i, 0.174), X, (0.05, 0.008, 0.004), 0.002, k=0.002) for i in (-2, -1, 0, 1, 2)]  # Deckelrippen
    poles = [A.cone(ROOT, (0.075 + 0.03 * s, -0.12, 0.172), (0.075 + 0.03 * s, -0.12, 0.192), 0.008, 0.007, k=0.0) for s in (-1, 1)]
    redcap = [A.cone(ROOT, (0.105, -0.12, 0.186), (0.105, -0.12, 0.198), 0.0105, 0.009, k=0.001)]
    # Zuendspule: Holzspule mit Kupferwicklung, quer auf dem Brett
    spool = [A.cone(ROOT, (0.09, 0.09, 0.07), (0.09, 0.24, 0.07), 0.016, 0.016, k=0.0)]
    spool += [A.ell(ROOT, (0.09, y, 0.07), Y, (0.0055, 0.046, 0.046), up=Z, k=0.0) for y in (0.084, 0.246)]  # Scheiben
    # Kondensator: eine rostige Konservendose (innen und aussen Folie - wie eine Leidener Flasche), Deckel mit Schraube
    can = [A.cone(ROOT, (0.172, -0.17, 0.026), (0.172, -0.17, 0.118), 0.034, 0.034, k=0.0)]
    can += [A.cone(ROOT, (0.172, -0.17, z), (0.172, -0.17, z + 0.002), 0.0355, 0.0355, k=0.001) for z in (0.045, 0.07, 0.095)]  # Sicken
    can_top = [A.cone(ROOT, (0.172, -0.17, 0.118), (0.172, -0.17, 0.142), 0.004, 0.004, k=0.0)]
    winding = [A.cone(ROOT, (0.09, 0.092, 0.07), (0.09, 0.238, 0.07), 0.038, 0.038, k=0.0)]

    def wind_disp(Q):
        return -0.0012 * (0.5 + 0.5 * np.sin(Q[:, 1] * 2400.0))  # Windungen

    # Unterbrecher: Federblech mit Kontakt und Schraube, darunter die Feder
    breaker = [A.box(ROOT, (0.17, 0.0, 0.03), Y, (0.05, 0.007, 0.0015), 0.0008, up=Z, k=0.0),
               A.cone(ROOT, (0.17, -0.045, 0.026), (0.17, -0.045, 0.06), 0.004, 0.004, k=0.0),
               A.cone(ROOT, (0.17, 0.045, 0.026), (0.17, 0.045, 0.05), 0.003, 0.003, k=0.0)]
    for i in range(7):  # Feder
        t = i / 6
        breaker.append(A.cone(ROOT, (0.17 + 0.007 * math.cos(t * 12), 0.02 + 0.007 * math.sin(t * 12), 0.026 + 0.02 * t),
                              (0.17 + 0.007 * math.cos(t * 12 + 2), 0.02 + 0.007 * math.sin(t * 12 + 2), 0.029 + 0.02 * t), 0.0012, 0.0012, k=0.0))
    led_leg = A.chain(lambda i: ROOT, A.bezier((0.04, 0.17, 0.026), (0.02, 0.2, 0.09), (0.035, 0.18, 0.14), 6), [0.0014] * 7, k=0.0)
    led = [A.ell(ROOT, (0.035, 0.18, 0.148), Z, (0.0055, 0.0055, 0.008), k=0.0)]
    # Kabel: Batterie -> Spule -> Unterbrecher, und die Leitung die Wand hinauf zum linken Isolator (abgenagte Enden)
    cables_r = A.chain(lambda i: ROOT, A.bezier((0.105, -0.12, 0.192), (0.16, -0.05, 0.25), (0.13, 0.09, 0.11), 8), [0.0045] * 9, k=0.0)
    cables_b = A.chain(lambda i: ROOT, A.bezier((0.045, -0.12, 0.192), (0.0, -0.02, 0.2), (0.0, 0.0, 0.02), 8), [0.0045] * 9, k=0.0)
    cables_b += A.chain(lambda i: ROOT, A.bezier((0.12, 0.24, 0.07), (0.2, 0.15, 0.03), (0.17, 0.045, 0.05), 6), [0.004] * 7, k=0.0)
    up_lead = A.chain(lambda i: ROOT, [np.array([0.008, 0.0, 0.02]), np.array([0.006, 0.0, 0.12]), np.array([0.01, 0.0, 0.17])], [0.004] * 3, k=0.0)
    tape = []
    for p in (np.array([0.15, -0.02, 0.2]), np.array([0.006, 0.0, 0.08]), np.array([0.15, 0.2, 0.05])):
        tape.append(A.ell(ROOT, p, A.unit(rng.normal(0, 1, 3)), (0.012, 0.008, 0.008), k=0.002))
    copper_ends = []
    for p, d in ((np.array([0.01, 0.0, 0.17]), Z), (np.array([0.13, 0.09, 0.11]), A.unit((0.2, -1, 0.4)))):
        for k in range(5):  # aufgespleisste, abgenagte Litzen
            dd = A.unit(d + rng.normal(0, 0.35, 3))
            copper_ends.append(A.cone(ROOT, p, p + dd * (0.008 + 0.006 * rng.random()), 0.0009, 0.0005, k=0.0))

    def wood_disp(Q):
        return -0.0012 * sdf.ridged(Q * np.array([0.15, 1.0, 1.0]), 160, 2, seed=2425) - 0.0006 * sdf.fbm(Q, 300, 2, seed=2426)

    def gnaw_disp(Q):  # Zahnspuren in der Isolierung
        return -0.0008 * np.clip(sdf.worley(Q, 260, seed=2427)[0] * 3 - 1, -1, 0)

    # --- Isolatoren an den Waenden: Nagel, Porzellanspule, die Drahtschlinge darum --------------------------------------------
    def insulator(bone, at):
        c = np.asarray(at, dtype=float)
        porcelain = [A.cone(bone, c + X * 0.0, c + X * 0.03, 0.013, 0.013, k=0.0),
                     A.cone(bone, c + X * 0.0, c + X * 0.006, 0.019, 0.019, k=0.002),
                     A.cone(bone, c + X * 0.024, c + X * 0.03, 0.019, 0.019, k=0.002)]
        nail = [A.cone(bone, c + X * 0.034, c - X * 0.06, 0.0028, 0.0022, k=0.0), A.cone(bone, c + X * 0.036, c + X * 0.032, 0.0065, 0.0065, k=0.0)]
        loop = []
        for i in range(13):
            t = i / 12 * 2 * math.pi
            loop.append(c + X * 0.015 + Y * 0.0155 * math.cos(t) + Z * 0.0155 * math.sin(t))
        loop_parts = A.chain(lambda i: bone, loop, [0.0012] * 13, k=0.0)
        return porcelain, nail, loop_parts

    pl, nl, ll = insulator(ANCHOR_L, J[8])
    pr, nr, lr = insulator(ANCHOR_R, J[12])
    # --- Funke am Unterbrecher (Knochen 5; das Spiel blendet ihn ein) ---------------------------------------------------------
    sc = J[5]
    spark = [A.ell(SPARK, sc, X, (0.006, 0.006, 0.006), k=0.0)]
    for i in range(14):
        d = A.unit(rng.normal(0, 1, 3))
        pts = [sc, sc + d * 0.012 + rng.normal(0, 0.004, 3), sc + d * 0.024 + rng.normal(0, 0.006, 3), sc + d * (0.03 + 0.02 * rng.random())]
        spark += A.chain(lambda k: SPARK, pts, [0.0018, 0.0012, 0.0008, 0.0002], k=0.0)
    # --- Draht: zwei verdrillte blanke Kupferlitzen entlang der Schwanzknochen (fertiges Netz) --------------------------------
    chain = [J[24 + i] for i in range(5)]
    pts, segs = [], 16
    for i in range(4):
        for k in range(segs):
            pts.append((i, k / segs, A.lerp(chain[i], chain[i + 1], k / segs)))
    pts.append((3, 1.0, chain[4]))

    def bones_of_idx(idx):
        i, f, _ = pts[idx]
        b = WIRE0 + i
        # an den Gelenken weich in den Nachbarn mischen (sonst knickt der Draht)
        if f < 0.15 and i > 0:
            return b - 1, b, 0.5 + f / 0.3
        if f > 0.85 and i < 3:
            return b, b + 1, (f - 0.85) / 0.3
        return b, b, 0.0

    lods = []
    for sides, per in ((6, 1), (5, 2), (4, 4)):
        sel = list(range(0, len(pts), per))
        if sel[-1] != len(pts) - 1:
            sel.append(len(pts) - 1)
        P = [pts[i][2] for i in sel]
        V, T, W = tube_mesh(P, lambda j: bones_of_idx(sel[j]), 0.00011, sides, nb, twist=0.9 * per, strands=2, strand_off=0.00013)
        cav = np.zeros(len(V))
        edge = np.full(len(V), 0.6)
        lods.append((V, T, W, cav, edge))
    # Spleisse mit Isolierband (dunkle Knoten) auf dem Draht - auch fertig, an den Schwanzknochen
    knots = []
    for i, f in ((1, 0.5), (2, 0.7)):
        c = A.lerp(chain[i], chain[i + 1], f)
        knots.append(A.ell(WIRE0 + i, c, A.unit(chain[i + 1] - chain[i]), (0.0016, 0.00045, 0.00045), k=0.0))

    return [
        {'name': 'brett', 'fm': 'ent_ob_wood', 'parts': board + spool, 'disp': wood_disp, 'h': 0.0016, 'cav_scale': 0.006},
        {'name': 'batterie', 'fm': 'ent_ob_rubber', 'parts': battery, 'disp': None, 'h': 0.0016, 'cav_scale': 0.006},
        {'name': 'pole', 'fm': 'ent_ob_steel', 'parts': poles + breaker + can_top, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003},
        {'name': 'dose', 'fm': 'ent_ob_rust', 'parts': can, 'disp': None, 'h': 0.0009, 'cav_scale': 0.004},
        {'name': 'kappe', 'fm': 'ent_ob_tape_r', 'parts': redcap, 'disp': None, 'h': 0.0008, 'cav_scale': 0.003},
        {'name': 'wicklung', 'fm': 'ent_pg_brass', 'parts': winding + copper_ends, 'disp': wind_disp, 'h': 0.0006, 'cav_scale': 0.003},
        {'name': 'kabel', 'fm': 'ent_ob_rubber', 'parts': cables_r + cables_b + up_lead + tape + led_leg, 'disp': gnaw_disp, 'h': 0.0009,
         'cav_scale': 0.003, 'merge': 'kabel'},
        {'name': 'leuchte', 'fm': 'ent_hornglow', 'parts': led, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
        {'name': 'porzellan_l', 'fm': 'ent_tooth', 'parts': pl, 'disp': None, 'h': 0.0008, 'cav_scale': 0.003, 'merge': 'porzellan'},
        {'name': 'porzellan_r', 'fm': 'ent_tooth', 'parts': pr, 'disp': None, 'h': 0.0008, 'cav_scale': 0.003, 'merge': 'porzellan'},
        {'name': 'nagel_l', 'fm': 'ent_ob_rust', 'parts': nl, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002, 'merge': 'nagel'},
        {'name': 'nagel_r', 'fm': 'ent_ob_rust', 'parts': nr, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002, 'merge': 'nagel'},
        {'name': 'schlinge_l', 'fm': 'ent_pg_brass', 'parts': ll, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002, 'merge': 'schlinge'},
        {'name': 'schlinge_r', 'fm': 'ent_pg_brass', 'parts': lr, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002, 'merge': 'schlinge'},
        {'name': 'funke', 'fm': 'ent_ob_spark', 'parts': spark, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
        {'name': 'knoten', 'fm': 'ent_ob_rubber', 'parts': knots, 'disp': None, 'h': 0.00012, 'cav_scale': 0.001},
        {'name': 'draht', 'fm': 'ent_pg_brass', 'parts': [], 'lod_mesh': lods},
    ]
