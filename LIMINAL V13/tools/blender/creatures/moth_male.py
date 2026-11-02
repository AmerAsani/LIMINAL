"""V13.3 Teil 4: Deathmoth-Maennchen fuer Level 2 (KREATUR_Deathmoth.md, Abschnitt Level 2; Wikidot Entity 4 Rev. 6).

Entity 4: das gewoehnliche Maennchen hat statt der Reisskiefer eine lange, nadelartige Saugroehre (es ist ein friedlicher
Aasfresser, der jede Fluessigkeit trinkt), einen schlankeren Leib als das Weibchen und die besten Fuehler der Art (es
meldet ueber weite Strecken an die Kolonie). Level 2: kleiner als anderswo, meist friedlich. Gleiches Skelett wie das
Weibchen (ent_moth), gleicher Fluegelumriss (Atlas des Fluegelmusters, render/moth_wing.hpp); kleiner macht es das Spiel
(Massstab 0,52, presence_moth_l2.cpp).

Gegenueber moth.py: Saugroehre (zur Uhrfeder eingerollt, die Spitze haengt heraus) statt Mandibeln; schlanker Leib mit
schuetterem Haarsaum, an der Spitze zwei Haltezangen statt des Legebohrers; groessere, doppelt gekaemmte Fuehler; die
Brust etwas schmaler, der Pelz staubig und duenner (Hunger - die Seite: Nahrungsmangel).
"""
import math

import numpy as np

import anat as A
import sdf
from moth import ABD1, ABD2, ABD3, HEAD, THORAX, F, L, U, Wing, bez

LODS = (26000, 8000, 2400)
SKELETON = 'ent_moth'


def build(J, bones):
    rng = np.random.default_rng(22)
    P = lambda i: np.asarray(J[i], dtype=np.float64)  # noqa: E731
    fur, eyes = [], []
    H = P(1)
    # --- Brust: schmaler, Pelz duenner und kuerzer ---------------------------------------------------------------
    TC, TR = np.array([0.0, 0.0, 0.002]), np.array([0.026, 0.019, 0.019])
    fur += [A.ell(THORAX, TC, F, TR, k=0.0),
            A.ell(THORAX, (0.024, 0.0, 0.006), F, (0.011, 0.015, 0.014), k=0.006),
            A.ell(THORAX, (-0.017, 0.0, 0.014), F, (0.009, 0.009, 0.005), k=0.005)]
    for s in (1, -1):
        fur.append(A.ell(THORAX, (0.007, s * 0.014, 0.011), F, (0.014, 0.006, 0.0045), k=0.004))
    for i in range(420):
        u = A.unit(rng.normal(0, 1, 3))
        if u[2] < -0.35:
            continue
        p = TC + u * TR * 0.96
        n = A.unit(u / TR ** 2)
        d = A.unit(n * 0.45 - F * 0.85 + rng.normal(0, 0.14, 3))
        Lh = 0.0024 + 0.002 * rng.random()
        fur.append(A.cone(THORAX, p, p + d * Lh, 0.0009, 0.0002, k=0.0008))
    # --- Kopf -------------------------------------------------------------------------------------------------
    fur.append(A.ell(HEAD, H + F * 0.001, F, (0.0098, 0.0095, 0.0098), k=0.004))
    for i in range(30):
        a = rng.uniform(-0.8, 0.8)
        p = H + F * 0.006 + U * 0.007 + L * (0.005 * a)
        d = A.unit(F * 0.6 + U * 0.6 + L * (0.4 * a) + rng.normal(0, 0.12, 3))
        fur.append(A.cone(HEAD, p, p + d * (0.0028 + 0.0018 * rng.random()), 0.0009, 0.0002, k=0.0006))
    for s in (1, -1):  # Lippentaster seitlich der Saugroehre
        b = H + F * 0.009 - U * 0.006 + L * s * 0.0032
        fur += A.chain(lambda k: HEAD, bez(b, b + F * 0.004 - U * 0.001, b + F * 0.006 + U * 0.004, 4), [0.0013, 0.0012, 0.0011, 0.0009, 0.0005], k=0.0008)
    for s in (1, -1):  # grosse Facettenaugen, winzige Punktaugen
        eyes.append(A.ell(HEAD, H + F * 0.0045 + L * s * 0.0104 + U * 0.0015, A.unit(F + L * s * 0.25), (0.0086, 0.0082, 0.0088), k=0.0))
        eyes.append(A.ell(HEAD, H + F * 0.0035 + L * s * 0.0035 + U * 0.0100, F, (0.0006, 0.0006, 0.0005), k=0.0))

    def eye_disp(Q):
        d1, d2 = sdf.worley(Q, 1500, seed=31)
        return -0.00005 * np.clip(1 - d1 * 1.6, 0, 1)

    # --- Saugroehre: unter dem Kopf zur Uhrfeder eingerollt, die Nadelspitze haengt ein Stueck heraus ---------------------
    prob = []
    c0 = H + F * 0.010 - U * 0.011
    pts = []
    for i in range(30):
        t = i / 29.0
        ang = 0.4 + t * 2.2 * math.pi
        r = 0.0062 * (1.0 - 0.72 * t)
        pts.append(c0 + F * (r * math.cos(ang)) - U * (r * math.sin(ang)) + L * (0.0006 * math.sin(t * 9)))
    start = H + F * 0.0085 - U * 0.0058
    pts = [start] + pts
    prob += A.chain(lambda k: HEAD, pts, list(np.linspace(0.0011, 0.00055, len(pts))), k=0.0003, group='chitin')
    # die Spitze: aus der Rolle nach vorn unten, nadelfein (trinkt aus Lachen)
    tip0 = c0 + F * 0.0062 - U * 0.001
    prob += A.chain(lambda k: HEAD, bez(tip0, tip0 + F * 0.007 - U * 0.002, tip0 + F * 0.012 - U * 0.009, 8),
                    list(np.linspace(0.0007, 0.00012, 9)), k=0.0002, group='chitin')
    # --- Hinterleib: schlank, schuetterer Haarsaum, Haltezangen ---------------------------------------------------------
    line = [P(3) + F * 0.006, P(4), P(5), P(6)]

    def along(t):
        seg = min(int(t * 3), 2)
        f = t * 3 - seg
        return A.lerp(line[seg], line[seg + 1], f), A.unit(line[seg + 1] - line[seg])

    nseg = 7
    for k in range(nseg):
        t = (k + 0.5) / nseg
        c, d = along(t)
        r = 0.0135 * (1 - 0.45 * t ** 1.4)
        bone = ABD1 if t < 0.35 else (ABD2 if t < 0.7 else ABD3)
        fur.append(A.ell(bone, c, d, (0.0088, r, r * 0.9), up=U, k=0.003))
        fur.append(A.ell(bone, c - d * 0.0075, d, (0.0033, r * 0.94, r * 0.86), up=U, k=0.003))
        for i in range(26):
            ang = rng.uniform(-math.pi, math.pi)
            rr = A.unit(L * math.cos(ang) + U * math.sin(ang) * 0.9)
            p = c - d * 0.006 + rr * r * 0.93
            hd = A.unit(-d * 0.9 + rr * 0.25 + rng.normal(0, 0.08, 3))
            fur.append(A.cone(bone, p, p + hd * (0.003 + 0.002 * rng.random()), 0.0007, 0.00012, k=0.0006))
    tip = P(6)
    clasp = []
    for s in (1, -1):  # Haltezangen des Maennchens: zwei kleine, nach innen gebogene Haken
        a0 = tip + F * 0.003 + L * s * 0.0022
        clasp += A.chain(lambda k: ABD3, bez(a0, a0 - F * 0.006 + L * s * 0.002, a0 - F * 0.009 - L * s * 0.0006 - U * 0.001, 5),
                         [0.0013, 0.0011, 0.0008, 0.0005, 0.0003, 0.0001], k=0.0003, group='chitin')
    # --- Fuehler: gross und doppelt gekaemmt (Maennchen: die besten Fuehler der Art) -----------------------------------------
    antennae = []
    for s, (jb, jt, bone) in ((1, (9, 10, 8)), (-1, (11, 12, 9))):
        b, t = P(jb), P(jt)
        t = b + (t - b) * 1.12
        mid = A.lerp(b, t, 0.5) + U * 0.007
        shaft = bez(b, mid, t, 40)
        part = A.chain(lambda k: bone, shaft, list(np.linspace(0.0007, 0.0003, len(shaft))), k=0.0002, group='chitin')
        side = A.unit(np.cross(A.unit(t - b), U))
        for i in range(2, len(shaft) - 1):
            f = i / (len(shaft) - 1)
            q = shaft[i]
            ad = A.unit(shaft[i + 1] - shaft[i - 1])
            Lb = 0.0098 * (1 - 0.78 * f) + 0.0012
            for sd in (1, -1):
                bd = A.unit(side * sd + ad * 0.5 - U * 0.22)
                part.append(A.cone(bone, q, q + bd * Lb, 0.00024, 0.0001, k=0.0001, group='chitin'))
        antennae.append(part)
    # --- Beine (wie das Weibchen, etwas zierlicher) -----------------------------------------------------------------------
    legs = []
    for l in range(6):
        sg = 1 if l % 2 == 0 else -1
        j0, j1, j2, j3 = (P(21 + 4 * l + i) for i in range(4))
        bf, bt, bta = 14 + 3 * l, 15 + 3 * l, 16 + 3 * l
        lp = [A.ell(bf, j0 + U * 0.001, A.unit(j1 - j0), (0.0040, 0.0027, 0.0025), k=0.001)]
        lp.append(A.cone(bf, j0, j1, 0.0024, 0.0019, k=0.0008))
        lp.append(A.ell(bt, j1, A.unit(j2 - j0), (0.0025, 0.0022, 0.0022), k=0.0008))
        lp.append(A.cone(bt, j1, j2, 0.0019, 0.0014, k=0.0006))
        td = A.unit(j2 - j1)
        out = A.unit(np.cross(td, U)) * (1 if np.cross(td, U)[1] * sg > 0 else -1)
        for k in range(4 if l >= 4 else 2):
            q = A.lerp(j1, j2, 0.3 + 0.55 * k / 3)
            lp.append(A.cone(bt, q, q + A.unit(out * 0.7 - U * 0.4 + td * 0.5) * 0.002, 0.0004, 0.0001))
        tars = [A.lerp(j2, j3, f) for f in (0.0, 0.32, 0.55, 0.72, 0.86, 1.0)]
        lp += A.chain(lambda k: bta, tars, [0.0011, 0.0010, 0.0009, 0.0008, 0.0007, 0.0006], k=0.0003)
        fd = A.unit(j3 - j2)
        for sd in (1, -1):
            c0_ = j3 + fd * 0.0003
            lp += A.chain(lambda k: bta, [c0_, c0_ + fd * 0.0011 + L * sd * 0.0005, c0_ + fd * 0.0016 - U * 0.0014 + L * sd * 0.0006],
                          [0.0004, 0.00028, 0.00008], k=0.0001)
        legs.append(lp)
    wings = [[Wing(0, 1, 10)], [Wing(1, 1, 11)], [Wing(0, -1, 12)], [Wing(1, -1, 13)]]

    def fur_disp(Q):
        return -0.00008 * sdf.fbm(Q, 600, 2, seed=43)

    def chit_disp(Q):
        return -0.00003 * sdf.ridged(Q, 900, 1, seed=44)

    groups = [
        {'name': 'pelz', 'fm': 'ent_fur', 'parts': fur, 'disp': fur_disp, 'h': 0.00025, 'cav_scale': 0.002, 'lod_weight': 1.6},
        {'name': 'augen', 'fm': 'ent_motheye', 'parts': eyes, 'disp': eye_disp, 'h': 0.00010, 'cav_scale': 0.0008, 'lod_weight': 1.5},
        {'name': 'ruessel', 'fm': 'ent_chitin', 'parts': prob, 'disp': None, 'h': 0.00009, 'cav_scale': 0.0008, 'merge': 'chitin', 'lod_weight': 1.6},
        {'name': 'zangen', 'fm': 'ent_chitin', 'parts': clasp, 'disp': None, 'h': 0.00012, 'cav_scale': 0.0008, 'merge': 'chitin'},
    ]
    for i, a in enumerate(antennae):
        groups.append({'name': f'fuehler{i}', 'fm': 'ent_chitin', 'parts': a, 'disp': None, 'h': 0.00009, 'cav_scale': 0.0006, 'merge': 'chitin',
                       'lod_weight': 2.0})
    for i, lp in enumerate(legs):
        groups.append({'name': f'bein{i}', 'fm': 'ent_chitin', 'parts': lp, 'disp': chit_disp, 'h': 0.00015, 'cav_scale': 0.001, 'merge': 'chitin'})
    for i, wp in enumerate(wings):
        groups.append({'name': f'fluegel{i}', 'fm': 'ent_wing', 'parts': wp, 'disp': None, 'h': 0.00020, 'cav_scale': 0.0015, 'merge': 'fluegel',
                       'lod_weight': 0.12})
    return groups
