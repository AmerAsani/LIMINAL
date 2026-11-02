"""V13.3 Level 3: Deathmoth-Praetorianer (KREATUR_Deathmoth.md, Abschnitt Level 3; Wikidot Entity 4 Rev. 6, Level 3 Rev. 18).

Entity 4: die zweitgroesste Kaste, knapp 1 m hoch, Leib 15-20 cm dick, Spannweite etwa 60 cm. Acht gegliederte Beine von 70-75
cm mit inneren Giftorganen (starkes Laehmungsgift, wie das der Larven, aber viel staerker); die Fluegel halb verkuemmert - sie
faecheln nur Lockstoffe (bis 5 km). Zwitter; wie die Weibchen ein Organ, das aetzende Fluessigkeit ausstoesst. Wendig, greift
sofort an, sticht und schlaegt gezielt auf verwundbare Stellen, zerlegt die Beute mit mehreren Beinen. Level 3: ueber 40 % der
Motten sind Praetorianer.

Eigener Entwurf: ein schwerer, gepanzerter Leib (Chitinplatten statt Pelz, nur ein verfilzter Kragen), auf acht viel zu langen,
duennen Beinen hoch getragen - das vierte Paar sitzt falsch, am Hinterleib (das macht den Gang unheimlich). In jedem Schenkel
schimmert das Giftorgan als prall gefuellter, eitergelber Beutel durch die duenne Haut; die Fussspitzen sind dunkle,
nasse Stachelhaken. Der Kopf traegt riesige Facettenaugen, lange gefiederte Fuehler und gekreuzte Saegemandibeln; darunter
haengt die Druese mit der aetzenden Fluessigkeit, ein Tropfen faedelt ab. Die Fluegel sind zerknitterte, zerrissene Stummel,
am Hinterleibsende zwei Duftpinsel (Haarbuesche), die sich beim Faecheln spreizen.

Massstab: gebaut im Mottenskelett (ent_moth, Ruhepose in Mottengroesse). Das Spiel vergroessert den Leib um 3,6 und die Beine
zusaetzlich um 2,3 (creature_moth_l3.cpp, kG, kLegK) - darum sind die Beine hier duenn und kurz.

Knochen: 1 Brust, 2 Kopf, 3-5 Hinterleib, 6-7 Duftpinsel, 8-9 Fuehler, 10/12 Vorderfluegel, 11/13 Hinterfluegel,
14 + 3l .. 16 + 3l Bein l (0..7; 6 und 7 am Hinterleib).
"""
import math

import numpy as np

import anat as A
import sdf
from moth import DEFS, Wing, rmax, bez

LODS = (46000, 18000, 6500)
SKELETON = 'ent_moth'
THORAX, HEAD, ABD1, ABD2, ABD3 = 1, 2, 3, 4, 5
F = np.array([1.0, 0, 0])
L = np.array([0, 1.0, 0])
U = np.array([0, 0, 1.0])


class Stub(Wing):
    """Verkuemmerter Fluegel: nur der innere Teil der Platte (frac des Umrisses), der Rand zerrissen, die Platte zerknittert."""

    def __init__(self, w, sg, bone, frac):
        self.frac = frac
        super().__init__(w, sg, bone)

    def _bounds(self):
        super()._bounds()
        rx, ry, z = DEFS[self.w][:3]
        c = np.array([rx, ry * self.sg, z])
        r = 0.21 * self.frac + 0.004
        self.lo = np.maximum(self.lo, c - r)
        self.hi = np.minimum(self.hi, c + r)

    def dist(self, P):
        rx, ry, z0, th0, th1 = DEFS[self.w][:5]
        x, y, z = P[:, 0], P[:, 1] * self.sg, P[:, 2]
        dx, dy = x - rx, y - ry
        th = np.arctan2(dx, dy)
        r = np.sqrt(dx * dx + dy * dy)
        # zerrissener Rand: Fransen und Loecher
        tear = 0.25 * self.frac * rmax(self.w, np.clip(th, th1, th0)) * (sdf.fbm(P * np.array([1, 1, 0.1]), 260, 3, seed=4402 + self.w) - 0.35)
        rm = rmax(self.w, np.clip(th, th1, th0)) * self.frac + tear
        base = super().dist(P)
        # zerknittert: die Platte wellt sich (Falten quer)
        return np.maximum(base, r - rm) + 0.0004 * np.sin(th * 23.0 + r * 900.0) * (r / 0.08)


def build(J, bones):
    rng = np.random.default_rng(4401)
    P = lambda i: np.asarray(J[i], dtype=np.float64)  # noqa: E731
    H = P(1)
    # --- Brust: gepanzert (Platten), Kragen aus verfilztem Haar ----------------------------------------------------------
    TC = np.array([0.0, 0.0, 0.003])
    chitin = [A.ell(THORAX, TC, F, (0.034, 0.026, 0.025), k=0.0),
              A.ell(THORAX, (0.024, 0.0, 0.004), F, (0.016, 0.019, 0.018), k=0.008)]
    plates = []
    for i, (x0, w0) in enumerate(((0.016, 0.022), (-0.004, 0.025), (-0.022, 0.02))):  # Rueckenplatten mit Kiel
        plates.append(A.ell(THORAX, (x0, 0.0, 0.021), F, (0.012, w0, 0.009), k=0.002))
        plates.append(A.ell(THORAX, (x0 - 0.009, 0.0, 0.024), F, (0.0025, w0 * 0.9, 0.006), op='sub', k=0.0015))  # Fuge
    plates.append(A.cone(THORAX, (0.03, 0, 0.026), (-0.03, 0, 0.03), 0.0025, 0.002, k=0.002))  # Kiel
    for s in (1, -1):
        plates.append(A.ell(THORAX, (0.0, s * 0.024, 0.006), F, (0.026, 0.006, 0.016), k=0.003))  # Seitenschilde
    collar = []
    for i in range(260):  # verfilzter Kragen hinter dem Kopf: dicke, verklebte Straehnen nach hinten
        a = rng.uniform(-math.pi, math.pi)
        rr = A.unit(L * math.cos(a) + U * math.sin(a) * 0.95)
        if rr[2] < -0.5:
            continue
        p = np.array([0.03, 0, 0.004]) + rr * 0.019
        d = A.unit(-F * 0.8 + rr * 0.5 + rng.normal(0, 0.15, 3))
        collar.append(A.cone(THORAX, p, p + d * (0.006 + 0.004 * rng.random()), 0.0016, 0.0004, k=0.001))
    # --- Kopf: Facettenaugen, gefiederte Fuehler, Saegemandibeln, Saeuredruese -----------------------------------------
    head = [A.ell(HEAD, H + F * 0.005, F, (0.02, 0.018, 0.019), k=0.0),
            A.ell(HEAD, H + F * 0.019 - U * 0.007, F, (0.011, 0.013, 0.01), k=0.006)]   # Kopfschild
    eyes = []
    for s in (1, -1):
        eyes.append(A.ell(HEAD, H + F * 0.01 + L * s * 0.0165 + U * 0.004, A.unit(F * 0.6 + L * s * 0.8), (0.0155, 0.0145, 0.017), k=0.0))

    def eye_disp(Q):
        d1, _ = sdf.worley(Q, 1100, seed=4403)
        return -0.00007 * np.clip(1 - d1 * 1.6, 0, 1)

    mand = []
    for s in (1, -1):  # Saegemandibeln: gekreuzt, Innenkante gezahnt
        b = H + F * 0.018 - U * 0.009 + L * s * 0.007
        c1, c2 = b + F * 0.012 - U * 0.002 + L * s * 0.004, H + F * 0.038 - U * 0.012 - L * s * 0.005
        pts = bez(b, c1, c2, 6)
        mand += A.chain(lambda k: HEAD, pts, [0.0034, 0.0031, 0.0027, 0.0022, 0.0016, 0.0010, 0.0003], k=0.0008)
        for t in range(2, 9):
            q = A.bezier(b, c1, c2, 10)[t]
            mand.append(A.cone(HEAD, q, q - L * s * 0.0032 + F * 0.0008 - U * 0.0004, 0.0009, 0.0001))
    gland = [A.ell(HEAD, H + F * 0.02 - U * 0.016, A.unit(F * 0.3 - U), (0.0075, 0.0062, 0.0055), k=0.002)]  # Druese
    drip = A.chain(lambda k: HEAD, [H + F * 0.022 - U * 0.021, H + F * 0.023 - U * 0.028, H + F * 0.023 - U * 0.034],
                   [0.0016, 0.0007, 0.0006], k=0.0006)
    drip.append(A.ell(HEAD, H + F * 0.023 - U * 0.0365, U, (0.0018, 0.0018, 0.0024), k=0.0006))  # Tropfen
    antennae = []
    for s, (jb, jt, bone) in ((1, (9, 10, 8)), (-1, (11, 12, 9))):
        b, t = P(jb), P(jt)
        mid = A.lerp(b, t, 0.5) + U * 0.008
        shaft = bez(b, mid, t, 28)
        part = A.chain(lambda k: bone, shaft, list(np.linspace(0.0011, 0.0004, len(shaft))), k=0.0002)
        side = A.unit(np.cross(A.unit(t - b), U))
        for i in range(2, len(shaft) - 1):
            f = i / (len(shaft) - 1)
            q = shaft[i]
            ad = A.unit(shaft[i + 1] - shaft[i - 1])
            Lb = 0.006 * (1 - 0.75 * f) + 0.0012
            for sd in (1, -1):
                bd = A.unit(side * sd + ad * 0.5 - U * 0.3)
                part.append(A.cone(bone, q, q + bd * Lb, 0.0003, 0.00012, k=0.0001))
        antennae.append(part)
    # --- Hinterleib: gepanzerte Ringe, helle Zwischenhaut, Duftpinsel ------------------------------------------------------
    line = [P(3) + F * 0.006, P(4), P(5), P(6)]

    def along(t):
        seg = min(int(t * 3), 2)
        f = t * 3 - seg
        return A.lerp(line[seg], line[seg + 1], f), A.unit(line[seg + 1] - line[seg])

    rings, membrane = [], []
    nseg = 6
    for k in range(nseg):
        t = (k + 0.5) / nseg
        c, d = along(t)
        r = 0.025 * (1 - 0.45 * t ** 1.4)
        bone = ABD1 if t < 0.35 else (ABD2 if t < 0.7 else ABD3)
        rings.append(A.ell(bone, c + d * 0.002, d, (0.0072, r, r * 0.9), up=U, k=0.0015))
        rings.append(A.cone(bone, c + d * 0.006 + U * r * 0.82, c - d * 0.004 + U * r * 0.86, 0.0018, 0.0012, k=0.0015))  # Rueckenkiel
        membrane.append(A.ell(bone, c - d * 0.006, d, (0.006, r * 0.9, r * 0.82), up=U, k=0.002))
    tip = P(6)
    pencils = []
    for s, bone, jb in ((1, 6, 7), (-1, 7, 8)):  # Duftpinsel an den Spitzenknochen: Haarbuesche
        base = tip
        end = P(jb)
        for i in range(36):
            d = A.unit(A.unit(end - base) + rng.normal(0, 0.28, 3))
            pencils.append(A.cone(bone, base + L * s * 0.002, base + L * s * 0.002 + d * (0.022 + 0.012 * rng.random()), 0.0007, 0.0001, k=0.0002))
    # --- Beine: acht, duenn (das Spiel streckt sie), Giftbeutel im Schenkel, nasse Stachelhaken -------------------------------
    leg_groups = []
    for l in range(8):
        j0, j1, j2, j3 = (P(21 + 4 * l + i) for i in range(4))
        bf, bt, bta = 14 + 3 * l, 15 + 3 * l, 16 + 3 * l
        fd = A.unit(j1 - j0)
        femur = [A.ell(bf, j0 + fd * 0.001, fd, (0.0032, 0.0024, 0.0022), k=0.0006),          # Huefte (klein - sie waechst mit)
                 A.cone(bf, j0, j1, 0.0017, 0.0014, k=0.0005)]
        nrm = A.unit(np.cross(fd, U)) if abs(fd[2]) < 0.95 else L
        venom = [A.ell(bf, A.lerp(j0, j1, 0.52) - U * 0.0014, fd, (0.0088, 0.0026, 0.0029), up=U, k=0.0)]  # Giftbeutel (unten)
        for t in (0.3, 0.5, 0.7):  # Adern ueber dem Beutel
            q = A.lerp(j0, j1, t)
            femur.append(A.cone(bf, q - fd * 0.002 + U * 0.0022 + nrm * 0.0008, q + fd * 0.002 + U * 0.0018 - nrm * 0.0006, 0.00035, 0.00035, k=0.0002))
        td = A.unit(j2 - j1)
        joint = [A.ell(bt, j1, td, (0.0022, 0.0019, 0.0019), k=0.0)]
        tibia = [A.cone(bt, j1, j2, 0.0013, 0.0010, k=0.0004)]
        out = A.unit(np.cross(td, U))
        for k in range(6):  # Dornen
            q = A.lerp(j1, j2, 0.2 + 0.65 * k / 5)
            dd = A.unit(out * (1 if k % 2 else -1) * 0.6 - U * 0.5 + td * 0.5)
            tibia.append(A.cone(bt, q, q + dd * 0.0026, 0.0004, 0.00008))
        tars = [A.lerp(j2, j3, f) for f in (0.0, 0.3, 0.55, 0.78, 1.0)]
        rad = [0.0009, 0.00085, 0.0008, 0.0007, 0.0006]
        tarsus = A.chain(lambda k: bta, tars, rad, k=0.0002)
        for q, r0 in zip(tars[1:-1], rad[1:-1]):
            tarsus.append(A.ell(bta, q, A.unit(j3 - j2), (r0 * 1.35, r0 * 1.25, r0 * 1.25), k=0.0002))
        xd = A.unit(j3 - j2)
        hook = A.chain(lambda k: bta, [j3, j3 + xd * 0.003 - U * 0.0006, j3 + xd * 0.0048 - U * 0.0032], [0.0007, 0.00045, 0.00008], k=0.0001)
        leg_groups.append((femur, venom, joint, tibia, tarsus, hook))
    # --- Fluegelstummel ----------------------------------------------------------------------------------------------------
    stubs = [[Stub(0, 1, 10, 0.55)], [Stub(1, 1, 11, 0.42)], [Stub(0, -1, 12, 0.55)], [Stub(1, -1, 13, 0.42)]]

    def chit_disp(Q):
        return -0.00005 * sdf.ridged(Q, 700, 2, seed=4404) - 0.00003 * sdf.fbm(Q, 1800, 1, seed=4405)

    def plate_disp(Q):  # Narben und Kratzer auf dem Panzer
        return -0.00009 * sdf.ridged(Q * np.array([1.0, 0.4, 1.0]), 900, 1, seed=4406)

    def sac_disp(Q):  # der Beutel: Adern und Knoten
        return -0.00006 * sdf.ridged(Q, 1500, 2, seed=4407)

    groups = [
        {'name': 'panzer', 'fm': 'ent_chitin', 'parts': chitin + plates + head + rings, 'disp': plate_disp, 'h': 0.00024, 'cav_scale': 0.0018,
         'lod_weight': 1.3},
        {'name': 'zwischenhaut', 'fm': 'ent_wr_gut', 'parts': membrane, 'disp': sac_disp, 'h': 0.00026, 'cav_scale': 0.0015},
        {'name': 'kragen', 'fm': 'ent_fur', 'parts': collar, 'disp': None, 'h': 0.0002, 'cav_scale': 0.0015},
        {'name': 'augen', 'fm': 'ent_motheye', 'parts': eyes, 'disp': eye_disp, 'h': 0.00012, 'cav_scale': 0.0008, 'lod_weight': 1.6},
        {'name': 'mandibeln', 'fm': 'ent_solifuge_dark', 'parts': mand, 'disp': None, 'h': 0.00011, 'cav_scale': 0.0008, 'lod_weight': 1.5},
        {'name': 'druese', 'fm': 'ent_wr_slime', 'parts': gland + drip, 'disp': None, 'h': 0.00012, 'cav_scale': 0.0008},
        {'name': 'pinsel', 'fm': 'ent_fur', 'parts': pencils, 'disp': None, 'h': 0.00014, 'cav_scale': 0.0008},
    ]
    for i, a in enumerate(antennae):
        groups.append({'name': f'fuehler{i}', 'fm': 'ent_chitin', 'parts': a, 'disp': None, 'h': 0.0001, 'cav_scale': 0.0006, 'merge': 'fuehler',
                       'lod_weight': 1.8})
    for i, (femur, venom, joint, tibia, tarsus, hook) in enumerate(leg_groups):
        groups.append({'name': f'schenkel{i}', 'fm': 'ent_chitin', 'parts': femur, 'disp': chit_disp, 'h': 0.00016, 'cav_scale': 0.0009, 'merge': 'beine'})
        groups.append({'name': f'gift{i}', 'fm': 'ent_ooze', 'parts': venom, 'disp': sac_disp, 'h': 0.00014, 'cav_scale': 0.0008, 'merge': 'gift'})
        groups.append({'name': f'gelenk{i}', 'fm': 'ent_solifuge_joint', 'parts': joint, 'disp': None, 'h': 0.00014, 'cav_scale': 0.0008, 'merge': 'gelenke'})
        groups.append({'name': f'schiene{i}', 'fm': 'ent_chitin', 'parts': tibia, 'disp': chit_disp, 'h': 0.00014, 'cav_scale': 0.0008, 'merge': 'beine'})
        groups.append({'name': f'fuss{i}', 'fm': 'ent_chitin', 'parts': tarsus, 'disp': None, 'h': 0.00012, 'cav_scale': 0.0007, 'merge': 'beine'})
        groups.append({'name': f'haken{i}', 'fm': 'ent_solifuge_dark', 'parts': hook, 'disp': None, 'h': 0.0001, 'cav_scale': 0.0006, 'merge': 'haken'})
    for i, wp in enumerate(stubs):
        groups.append({'name': f'stummel{i}', 'fm': 'ent_wing', 'parts': wp, 'disp': None, 'h': 0.00022, 'cav_scale': 0.0015, 'merge': 'stummel',
                       'lod_weight': 0.3})
    return groups
