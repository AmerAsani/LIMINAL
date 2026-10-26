"""V13: Deathmoth, gewoehnliches Weibchen - KREATUR_Deathmoth.md. Kurzer, dicker, behaarter Koerper: Brust mit dichtem
Haarpelz (einzelne Buschel), kleiner Kopf mit grossen gewoelbten Facettenaugen (Facetten als Geometrie) und zwei
Punktaugen, kurze gebogene Mandibeln mit Innenzaehnen und behaarte Taster (kein Ruessel - den hat laut Wikidot nur
das Maennchen), doppelt gekaemmte Fuehler. Hinterleib des Weibchens dick, in Ringen mit Haarsaeumen, an der Spitze
der eingezogene Legebohrer. Sechs Beine mit Huefte, Schenkel, Schiene (Dornen und Sporne), fuenfgliedrigem Fuss und
Doppelkralle. Zwei Paar Fluegel als duenne, gewoelbte Platten mit erhabenen Adern; der Umriss folgt genau
render/moth_wing.hpp, damit das Fluegelmuster (render/texture_entity.cpp, Atlas der Ruhepose) passt.

Knochen (ent_moth): 1 Brust, 2 Kopf, 3-5 Hinterleib, 6-7 Spitze, 8-9 Fuehler, 10 Vorderfluegel l, 11 Hinterfluegel l,
12 Vorderfluegel r, 13 Hinterfluegel r, Bein l (0 vorn l, 1 vorn r, 2 Mitte l, ...): 14+3l Schenkel, 15+3l Schiene,
16+3l Fuss.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (34000, 10000, 3000)
THORAX, HEAD, ABD1, ABD2, ABD3 = 1, 2, 3, 4, 5
F = np.array([1.0, 0, 0])
L = np.array([0, 1.0, 0])
U = np.array([0, 0, 1.0])

# --- Fluegelumriss: genaue Uebertragung von render/moth_wing.hpp ------------------------------------------------
DEFS = [  # rx, ry, z, th0, th1, jag0, jag1, teeth, jagAmp
    (0.012, 0.016, 0.010, 0.30, -1.05, 0.22, -0.95, 13, 0.0068),
    (-0.012, 0.014, 0.006, -0.95, -1.55, -0.98, -1.50, 7, 0.0058),
]
FA = np.array([0.30, 0.24, 0.12, -0.05, -0.30, -0.55, -0.78, -0.93, -1.05])
FR = np.array([0.200, 0.208, 0.204, 0.192, 0.170, 0.143, 0.112, 0.082, 0.050])
HA = np.array([-0.95, -1.02, -1.15, -1.30, -1.42, -1.50, -1.55])
HR = np.array([0.100, 0.138, 0.152, 0.150, 0.135, 0.115, 0.090])


def hash01(x):
    x = np.asarray(x, dtype=np.uint64) & 0xFFFFFFFF
    x ^= x >> 16
    x = (x * 0x7feb352d) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x846ca68b) & 0xFFFFFFFF
    x ^= x >> 16
    return (x & 0xFFFFFF).astype(np.float64) / 16777216.0


def smooth_r(w, th):
    Aa, R = (FA, FR) if w == 0 else (HA, HR)
    n = len(Aa)
    th = np.asarray(th, dtype=np.float64)
    i = np.sum(Aa[None, 1:n - 1] > th[:, None], axis=1)
    t = (th - Aa[i]) / (Aa[i + 1] - Aa[i])
    p0, p1, p2, p3 = R[np.maximum(0, i - 1)], R[i], R[i + 1], R[np.minimum(n - 1, i + 2)]
    t2, t3 = t * t, t * t * t
    out = 0.5 * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)
    out = np.where(th >= Aa[0], R[0], out)
    return np.where(th <= Aa[n - 1], R[n - 1], out)


def jag(w, th):
    rx, ry, z, th0, th1, jag0, jag1, teeth, amp = DEFS[w]
    u = (jag0 - th) / (jag0 - jag1)
    x = u * teeth
    k = np.floor(np.maximum(x, 0)).astype(np.int64)
    f = x - k
    skew = 0.35 + 0.3 * hash01(k * 7 + w * 101 + 3)
    depth = 0.55 + 0.9 * hash01(k * 13 + w * 211 + 5)
    tri = np.where(f < skew, f / skew, (1 - f) / (1 - skew))
    taper = np.minimum(1.0, np.minimum(u, 1 - u) * 6)
    out = amp * depth * (1 - tri) * taper
    return np.where((th > jag0) | (th < jag1), 0.0, out)


def rmax(w, th):
    return smooth_r(w, th) - jag(w, th)


class Wing(sdf.Part):
    """Duenne, gewoelbte Fluegelplatte: Sektor um die Wurzel bis zum (gezackten) Rand, Dicke zum Rand abnehmend,
    erhabene Laengsadern und eine Querader (wie im Muster)."""

    def __init__(self, w, sg, bone):
        self.w, self.sg = w, sg
        super().__init__('wing', bone, k=0.0, op='add', group='wing')

    def _bounds(self):
        rx, ry, z = DEFS[self.w][:3]
        ths = np.linspace(DEFS[self.w][4], DEFS[self.w][3], 200)
        rm = rmax(self.w, ths)
        xs = rx + rm * np.sin(ths)
        ys = ry + rm * np.cos(ths)
        lo = np.array([min(xs.min(), rx), min(ys.min(), ry), z - 0.008])
        hi = np.array([max(xs.max(), rx), max(ys.max(), ry), z + 0.004])
        if self.sg < 0:
            lo[1], hi[1] = -hi[1], -lo[1]
        self.lo, self.hi = lo - 0.002, hi + 0.002

    def dist(self, P):
        rx, ry, z0, th0, th1 = DEFS[self.w][:5]
        x, y, z = P[:, 0], P[:, 1] * self.sg, P[:, 2]
        dx, dy = x - rx, y - ry
        th = np.arctan2(dx, dy)
        r = np.sqrt(dx * dx + dy * dy)
        rm = rmax(self.w, np.clip(th, th1, th0))
        d2 = np.maximum(r - rm, np.maximum((th - th0) * r, (th1 - th) * r))
        rho = np.clip(r / rm, 0, 1.2)
        zc = z0 - 0.004 * rho * rho + 0.00012 * rho * np.sin(th * 46.0)  # Runde 2: kaum Falten (vorher Faecher)
        half = 0.5 * (0.0011 * (1 - np.minimum(rho, 1)) + 0.0006)
        # Adern (wie texture_entity.cpp wingTex): strahlenfoermig, dazu die Querader der Mittelzelle
        nv = 9 if self.w == 0 else 7
        vein = np.zeros(len(P))
        for kk in range(nv):
            ak = (kk + 0.5) / nv
            tk = th1 + (th0 - th1) * ak + 0.06 * rho * rho * (ak - 0.5)
            dist = np.abs(th - tk) * r
            wd = 0.0006 + 0.0006 * (1 - np.minimum(rho, 1))
            vein = np.maximum(vein, np.exp(-dist * dist / (wd * wd)) * (rho > 0.12))
        a = (th - th1) / (th0 - th1)
        rEdge = r / np.maximum(rho, 1e-3)
        cross = np.exp(-((rho - 0.42) * rEdge / 0.0007) ** 2) * np.clip((a - 0.2) / 0.15, 0, 1) * np.clip((0.85 - a) / 0.15, 0, 1)
        vein = np.maximum(vein, cross)
        top = zc + half + 0.00035 * vein
        bot = zc - half - 0.00015 * vein
        dz = np.maximum(z - top, bot - z)
        return np.maximum(d2, dz)


def bez(p0, p1, p2, n):
    return A.bezier(p0, p1, p2, n)


def build(J, bones):
    rng = np.random.default_rng(21)
    P = lambda i: np.asarray(J[i], dtype=np.float64)
    fur, eyes, chit_groups = [], [], []
    H = P(1)
    # --- Brust mit Pelz -----------------------------------------------------------------------------------------
    TC, TR = np.array([0.0, 0.0, 0.002]), np.array([0.030, 0.023, 0.022])
    fur += [
        A.ell(THORAX, TC, F, TR, k=0.0),
        A.ell(THORAX, (0.026, 0.0, 0.006), F, (0.012, 0.017, 0.016), k=0.006),   # Halskragen
        A.ell(THORAX, (-0.018, 0.0, 0.016), F, (0.010, 0.010, 0.006), k=0.005),  # Schildchen
    ]
    for s in (1, -1):
        fur.append(A.ell(THORAX, (0.008, s * 0.016, 0.012), F, (0.016, 0.007, 0.005), k=0.004))  # Schulterplatten
    # Haarbueschel: nach aussen und hinten gekaemmt, oben dichter
    for i in range(700):  # Runde 2: dichter, kuerzer, anliegend nach hinten gekaemmt (vorher Stacheln)
        u = A.unit(rng.normal(0, 1, 3))
        if u[2] < -0.35:
            continue
        p = TC + u * TR * 0.96
        n = A.unit(u / TR ** 2)
        d = A.unit(n * 0.45 - F * 0.85 + rng.normal(0, 0.12, 3))
        Lh = 0.003 + 0.0025 * rng.random()
        fur.append(A.cone(THORAX, p, p + d * Lh, 0.0010, 0.0002, k=0.0009))
    # --- Kopf -----------------------------------------------------------------------------------------------------
    fur.append(A.ell(HEAD, H + F * 0.001, F, (0.0105, 0.0100, 0.0105), k=0.004))
    for i in range(40):  # Stirnschopf zwischen den Fuehlern
        a = rng.uniform(-0.8, 0.8)
        p = H + F * 0.006 + U * 0.007 + L * (0.005 * a)
        d = A.unit(F * 0.6 + U * 0.6 + L * (0.4 * a) + rng.normal(0, 0.12, 3))
        fur.append(A.cone(HEAD, p, p + d * (0.003 + 0.002 * rng.random()), 0.0010, 0.0002, k=0.0006))
    for s in (1, -1):  # behaarte Lippentaster vor dem Gesicht, nach oben gebogen
        b = H + F * 0.009 - U * 0.006 + L * s * 0.0028
        fur += A.chain(lambda k: HEAD, bez(b, b + F * 0.004 - U * 0.001, b + F * 0.006 + U * 0.004, 4), [0.0014, 0.0013, 0.0012, 0.0010, 0.0006], k=0.0008)
    # --- Facettenaugen (Facetten als Geometrie) und Punktaugen --------------------------------------------------------
    for s in (1, -1):
        # Runde 3: Augen weiter nach aussen gewoelbt (lagen wie Brillenglaeser in einer Fassung aus Kopfhaar), Punktaugen
        # winzig und halb im Haar (zwei dunkle Punkte oben ergaben mit den Augen ein Gesicht)
        eyes.append(A.ell(HEAD, H + F * 0.0045 + L * s * 0.0108 + U * 0.0015, A.unit(F + L * s * 0.25), (0.0084, 0.0080, 0.0086), k=0.0))
        eyes.append(A.ell(HEAD, H + F * 0.0035 + L * s * 0.0035 + U * 0.0100, F, (0.0006, 0.0006, 0.0005), k=0.0))  # Punktauge

    def eye_disp(Q):
        d1, d2 = sdf.worley(Q, 1500, seed=31)
        return -0.00005 * np.clip(1 - d1 * 1.6, 0, 1)  # gewoelbte Facetten, Rinnen zwischen ihnen

    # --- Mandibeln (gekreuzt, mit Innenzaehnen) ------------------------------------------------------------------------
    mand = []
    for s in (1, -1):
        b = H + F * 0.008 - U * 0.006 + L * s * 0.0045
        pts = bez(b, b + F * 0.006 - U * 0.002 + L * s * 0.001, H + F * 0.0165 - U * 0.0095 - L * s * 0.002, 5)
        mand += A.chain(lambda k: HEAD, pts, [0.0019, 0.0017, 0.0014, 0.0010, 0.0006, 0.0002], k=0.0006, group='chitin')
        for t in (0.45, 0.7):
            q = A.bezier(b, b + F * 0.006 - U * 0.002 + L * s * 0.001, H + F * 0.0165 - U * 0.0095 - L * s * 0.002, 10)[int(t * 10)]
            mand.append(A.cone(HEAD, q, q - L * s * 0.0022 + F * 0.0006, 0.0006, 0.0001, group='chitin'))
    # --- Hinterleib: dicke Ringe mit Haarsaum, Legebohrer -----------------------------------------------------------------
    line = [P(3) + F * 0.006, P(4), P(5), P(6)]

    def along(t):
        seg = min(int(t * 3), 2)
        f = t * 3 - seg
        return A.lerp(line[seg], line[seg + 1], f), A.unit(line[seg + 1] - line[seg])

    nseg = 7
    for k in range(nseg):
        t = (k + 0.5) / nseg
        c, d = along(t)
        r = 0.021 * (1 - 0.52 * t ** 1.5) * (1 + 0.08 * math.exp(-((t - 0.3) / 0.2) ** 2))
        bone = ABD1 if t < 0.35 else (ABD2 if t < 0.7 else ABD3)
        fur.append(A.ell(bone, c, d, (0.0090, r, r * 0.92), up=U, k=0.003))   # Runde 2: Ringe weicher (vorher Raupe)
        fur.append(A.ell(bone, c - d * 0.0075, d, (0.0035, r * 0.95, r * 0.88), up=U, k=0.003))  # Zwischenhaut
        for i in range(44):  # Haarsaum am Hinterrand des Rings, anliegend
            ang = rng.uniform(-math.pi, math.pi)
            rr = A.unit(L * math.cos(ang) + U * math.sin(ang) * 0.92)
            p = c - d * 0.006 + rr * r * 0.93
            hd = A.unit(-d * 0.9 + rr * 0.25 + rng.normal(0, 0.08, 3))
            fur.append(A.cone(bone, p, p + hd * (0.0035 + 0.0025 * rng.random()), 0.0008, 0.00015, k=0.0006))
    tip = P(6)
    ovi = []
    for s in (1, -1):  # zwei Klappen des Legebohrers, eingezogen - nur die Spitzen ragen heraus
        ovi.append(A.cone(ABD3, tip + F * 0.004 + L * s * 0.0008, tip - F * 0.009 + L * s * 0.0004 - U * 0.0008, 0.0016, 0.0003, group='chitin'))
    # --- Fuehler: doppelt gekaemmt ---------------------------------------------------------------------------------------
    antennae = []
    for s, (jb, jt, bone) in ((1, (9, 10, 8)), (-1, (11, 12, 9))):
        b, t = P(jb), P(jt)
        mid = A.lerp(b, t, 0.5) + U * 0.006
        shaft = bez(b, mid, t, 36)
        part = A.chain(lambda k: bone, shaft, list(np.linspace(0.0007, 0.0003, len(shaft))), k=0.0002, group='chitin')
        side = A.unit(np.cross(A.unit(t - b), U))
        for i in range(2, len(shaft) - 1):
            f = i / (len(shaft) - 1)
            q = shaft[i]
            ad = A.unit(shaft[i + 1] - shaft[i - 1])
            Lb = 0.0065 * (1 - 0.8 * f) + 0.0010
            for sd in (1, -1):
                bd = A.unit(side * sd + ad * 0.55 - U * 0.25)
                part.append(A.cone(bone, q, q + bd * Lb, 0.00026, 0.00012, k=0.0001, group='chitin'))
        antennae.append(part)
    # --- Beine ---------------------------------------------------------------------------------------------------------
    legs = []
    for l in range(6):
        sg = 1 if l % 2 == 0 else -1
        j0, j1, j2, j3 = (P(21 + 4 * l + i) for i in range(4))
        bf, bt, bta = 14 + 3 * l, 15 + 3 * l, 16 + 3 * l
        lp = [A.ell(bf, j0 + U * 0.001, A.unit(j1 - j0), (0.0045, 0.0030, 0.0028), k=0.001)]  # Huefte
        lp.append(A.cone(bf, j0, j1, 0.0027, 0.0021, k=0.0008))                                # Schenkel
        lp.append(A.ell(bf, A.lerp(j0, j1, 0.45), A.unit(j1 - j0), (0.010, 0.0029, 0.0026), k=0.001))
        lp.append(A.ell(bt, j1, A.unit(j2 - j0), (0.0028, 0.0025, 0.0025), k=0.0008))           # Knie
        lp.append(A.cone(bt, j1, j2, 0.0021, 0.0016, k=0.0006))                                 # Schiene
        td = A.unit(j2 - j1)
        out = A.unit(np.cross(td, U)) * (1 if np.cross(td, U)[1] * sg > 0 else -1)
        for k in range(5 if l >= 4 else 3):                                                     # Dornen
            q = A.lerp(j1, j2, 0.25 + 0.6 * k / 4)
            dd = A.unit(out * 0.7 - U * 0.4 + td * 0.5)
            lp.append(A.cone(bt, q, q + dd * 0.0022, 0.00045, 0.0001))
        for sd in (1, -1):                                                                      # Sporne am Schienenende
            dd = A.unit(td * 0.8 + out * 0.35 * sd - U * 0.3)
            lp.append(A.cone(bt, j2 - td * 0.001, j2 + dd * 0.0040, 0.00055, 0.0001))
        tars = [A.lerp(j2, j3, f) for f in (0.0, 0.32, 0.55, 0.72, 0.86, 1.0)]
        rad = [0.0013, 0.0012, 0.0011, 0.0010, 0.0009, 0.0007]
        lp += A.chain(lambda k: bta, tars, rad, k=0.0003)
        for q, r0 in zip(tars[1:-1], rad[1:-1]):
            lp.append(A.ell(bta, q, A.unit(j3 - j2), (r0 * 1.3, r0 * 1.2, r0 * 1.2), k=0.0002))
        fd = A.unit(j3 - j2)
        for sd in (1, -1):                                                                      # Doppelkralle
            c0 = j3 + fd * 0.0003
            lp += A.chain(lambda k: bta, [c0, c0 + fd * 0.0012 + L * sd * 0.0005, c0 + fd * 0.0018 - U * 0.0016 + L * sd * 0.0007],
                          [0.00045, 0.00030, 0.00008], k=0.0001)
        legs.append(lp)
    # --- Fluegel ---------------------------------------------------------------------------------------------------------
    wings = [[Wing(0, 1, 10)], [Wing(1, 1, 11)], [Wing(0, -1, 12)], [Wing(1, -1, 13)]]

    def fur_disp(Q):
        return -0.00008 * sdf.fbm(Q, 600, 2, seed=41)

    def chit_disp(Q):
        return -0.00003 * sdf.ridged(Q, 900, 1, seed=42)

    groups = [
        {'name': 'pelz', 'fm': 'ent_fur', 'parts': fur, 'disp': fur_disp, 'h': 0.00025, 'cav_scale': 0.002, 'lod_weight': 1.6},
        {'name': 'augen', 'fm': 'ent_motheye', 'parts': eyes, 'disp': eye_disp, 'h': 0.00010, 'cav_scale': 0.0008, 'lod_weight': 1.5},
        {'name': 'mandibeln', 'fm': 'ent_chitin', 'parts': mand, 'disp': None, 'h': 0.00010, 'cav_scale': 0.0008, 'merge': 'chitin'},
        {'name': 'legebohrer', 'fm': 'ent_chitin', 'parts': ovi, 'disp': None, 'h': 0.00012, 'cav_scale': 0.0008, 'merge': 'chitin'},
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
