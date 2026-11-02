"""V13.3: Arms / Les Bras (Entite 15-FR, Wikidot FR Rev. 44) - KREATUR_Arms.md. Ein einzelnes, sehr langes Glied (gut 4 m,
zum groessten Teil im Schacht), grau und verwesend: verdreht wie ausgewrungen (drei Sehnenstraenge laufen spiralig um den
Arm), mit mehreren knotigen Gelenken, Loechern in der Haut, unter denen nasses, dunkles Fleisch und blanke Sehnen liegen,
und abgeloesten Hautlappen. Am Ende eine knotige Hand mit drei langen Fingern, jeder mit einer grossen, gebogenen Kralle
(sie umschliessen einen Oberkoerper). Dazu: der Riss in der Decke (gebrochener Beton um ein dunkles Loch), drei verbrannte
Hautfetzen und vier Rauchfahnen.

Ruhepose (anim/creature.hpp, ar): Wurzel in der Decke (z = 0), die Kette haengt senkrecht: C0 bei z = 0,35 bis zum Handgelenk
bei z = -3,85; Knochen 1-14 Glieder, 15 Hand, 16-24 Krallen (je 3), 25 Riss, 26-28 Fetzen, 29-32 Rauch.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (70000, 20000, 6000)
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])
KSEG, KCLAWS, KSHREDS, KSMOKE = 14, 3, 3, 4
C0, WRIST, KN, CLAW0 = 1, 15, 16, 17
CRACK_A, SHRED0, SMOKE0 = 29, 31, 37
B_SEG0, B_HAND, B_CLAW0, B_CRACK, B_SHRED0, B_SMOKE0 = 1, 15, 16, 25, 26, 29


def claw(c, i): return CLAW0 + c * 4 + i
def bClaw(c, i): return B_CLAW0 + c * 3 + i


def radius(t):
    """Radius entlang der Kette (t = Gliedindex 0..14): im Schacht dick, zum Handgelenk duenner."""
    return 0.056 - 0.0011 * t


def build(J, bones):
    J = np.asarray(J, dtype=float)
    assert bones[B_CRACK]['a'] == CRACK_A and bones[B_SMOKE0]['a'] == SMOKE0, 'Skelett passt nicht zu arms.py'
    rng = np.random.default_rng(15)
    C = [J[C0 + i] for i in range(KSEG + 1)]
    z_of = lambda t: C[0][2] - t * 0.3  # noqa: E731
    seg_at = lambda z: int(np.clip((C[0][2] - z) / 0.3, 0, KSEG - 1))  # noqa: E731
    arm, flesh, tendon, flaps = [], [], [], []
    # --- Glieder -----------------------------------------------------------------------------------------------------
    for i in range(KSEG):
        arm.append(A.cone(B_SEG0 + i, C[i], C[i + 1], radius(i), radius(i + 1), k=0.015 if i else 0.0))
    # knotige Gelenke (er ist zu lang fuer einen Ellbogen): verdickt, ein Knochenhoecker
    for t, ang in ((5, 0.4), (9, 2.6), (12, 4.4)):
        p = C[t]
        r = radius(t)
        dvec = np.array([math.cos(ang), math.sin(ang), 0.0])
        arm += [A.ell(B_SEG0 + t, p, Z, (0.045, r * 1.32, r * 1.28), k=0.025),
                A.ell(B_SEG0 + t, p + dvec * r * 1.05 - Z * 0.01, Z, (0.03, 0.017, 0.017), up=dvec, k=0.012)]
    # verdreht: drei Sehnenstraenge laufen spiralig um den Arm (eine Windung je 1,3 m)
    for q in range(3):
        pts, rr = [], []
        for z in np.arange(C[1][2], C[KSEG][2] + 0.05, -0.08):
            t = (C[0][2] - z) / 0.3
            phi = q * 2 * math.pi / 3 + (C[0][2] - z) * 2 * math.pi / 1.3
            pts.append(np.array([math.cos(phi), math.sin(phi), 0.0]) * radius(t) * 0.8 + np.array([0, 0, z]))
            rr.append(0.014 - 0.0002 * t)
        for j in range(len(pts) - 1):
            arm.append(A.cone(B_SEG0 + seg_at(pts[j][2]), pts[j], pts[j + 1], rr[j], rr[j + 1], k=0.014))
    # Verwesung: Loecher in der Haut; darunter nasses Fleisch, in manchen blanke Sehnen; abgeloeste Hautlappen
    for w in range(16):
        z = rng.uniform(C[KSEG - 1][2] + 0.1, C[6][2])
        t = (C[0][2] - z) / 0.3
        ang = rng.uniform(0, 2 * math.pi)
        dvec = np.array([math.cos(ang), math.sin(ang), 0.0])
        r = radius(t)
        b = B_SEG0 + seg_at(z)
        L, Wd = rng.uniform(0.04, 0.09), rng.uniform(0.024, 0.042)
        c = np.array([0, 0, z]) + dvec * r
        arm.append(A.ell(b, c, Z, (L, Wd, 0.02), up=dvec, op='sub', k=0.006))
        flesh.append(A.ell(b, np.array([0, 0, z]) + dvec * (r * 0.5), Z, (L * 1.1, Wd * 1.1, r * 0.42), up=dvec, k=0.0))
        flesh.append(A.ell(b, np.array([0, 0, z]) + dvec * (r * 0.62) + Z * (L * 0.3), Z, (L * 0.4, Wd * 0.6, r * 0.42), up=dvec, k=0.01))
        if w % 3 == 0:  # blanke Sehne quer durch das Loch
            side = np.cross(dvec, Z)
            tendon.append(A.cone(b, c - Z * L * 0.9 - dvec * 0.006 + side * 0.004, c + Z * L * 0.9 - dvec * 0.006 - side * 0.004, 0.0045, 0.004, k=0.0))
        if w % 2 == 1:  # Hautlappen am oberen Rand, abgeloest und herabhaengend
            edge = c + Z * L * 0.95
            out = A.unit(dvec * 0.7 - Z * 0.7)
            flaps.append(A.ell(b, edge + out * 0.03 + dvec * 0.004, out, (0.032, Wd * 0.9, 0.0022), up=A.unit(np.cross(out, np.cross(dvec, Z))), k=0.0))
    # --- Hand: knotige Masse, drei Finger mit Knoecheln, Krallen aus Horn ------------------------------------------------
    hand, horn = [], []
    W, K = C[KSEG], J[KN]
    hand += [A.ell(B_HAND, A.lerp(W, K, 0.45), Z, (0.095, 0.05, 0.048), k=0.0),
             A.cone(B_HAND, W + Z * 0.06, W - Z * 0.02, 0.036, 0.042, k=0.02)]
    for c in range(KCLAWS):
        th = c * 2 * math.pi / KCLAWS
        rad = np.array([math.cos(th), math.sin(th), 0.0])
        p0, p1, p2, p3 = (J[claw(c, i)] for i in range(4))
        hand += [A.ell(B_HAND, K + rad * 0.032 + Z * 0.01, Z, (0.034, 0.03, 0.028), up=rad, k=0.02),   # Knoechel
                 A.cone(bClaw(c, 0), p0, p1, 0.026, 0.021, k=0.012),
                 A.muscle(bClaw(c, 0), p0, p1, 0.018, offset=tuple(rad * 0.01), k=0.01),
                 A.ell(bClaw(c, 1), p1, A.unit(p2 - p1), (0.026, 0.026, 0.024), k=0.01),
                 A.cone(bClaw(c, 1), p1, p2, 0.021, 0.018, k=0.01),
                 A.ell(bClaw(c, 2), p2 - A.unit(p2 - p1) * 0.004, A.unit(p3 - p2), (0.028, 0.022, 0.02), k=0.008)]  # Krallenbett
        # Kralle: lang, gebogen (zur Handmitte hin eingehakt), an der Wurzel breit
        d = A.unit(p3 - p2)
        inward = -A.unit(rad - d * float(rad @ d))
        pts = [p2 + d * (0.24 * u) + inward * (0.06 * math.sin(u * math.pi * 0.9) + 0.03 * u * u) for u in np.linspace(0, 1, 7)]
        rr = [0.019, 0.0165, 0.0135, 0.0105, 0.0075, 0.0045, 0.0012]
        horn += A.chain(lambda i, b=bClaw(c, 2): b, pts, rr, k=0.004)
    # --- Riss in der Decke: gebrochener Beton um ein dunkles Loch, Risse ------------------------------------------------
    crack, dark = [], []
    # gebrochener Beton: eine duenne, unregelmaessige Platte (Putzschicht) und darunter gesplitterte Brocken verschiedener
    # Groesse, schief, manche halb herausgebrochen; keine gleichmaessige Krone
    for i in range(26):
        a = rng.uniform(0, 2 * math.pi)
        dv = np.array([math.cos(a), math.sin(a), 0.0])
        rr = 0.14 + rng.uniform(0.0, 0.12) ** 1.4 * 1.6
        size = rng.uniform(0.012, 0.035) * (1.2 - (rr - 0.14) * 2.0)
        if size < 0.008:
            continue
        ax = A.unit(dv * rng.uniform(-1, 1) + np.cross(Z, dv) * rng.uniform(-1, 1) + Z * rng.uniform(-0.3, 0.3))
        crack.append(A.ell(B_CRACK, dv * rr - Z * rng.uniform(0.003, 0.012), ax, (size * rng.uniform(1.0, 1.8), size, size * rng.uniform(0.4, 0.8)),
                           up=A.perp(ax, Z), k=0.006))
    for i in range(9):  # Putzschicht, an den Raendern abgeblaettert
        a = i * 2 * math.pi / 9 + rng.uniform(-0.2, 0.2)
        dv = np.array([math.cos(a), math.sin(a), 0.0])
        r0, r1 = 0.13 + rng.uniform(0.0, 0.02), rng.uniform(0.17, 0.26)
        crack.append(A.box(B_CRACK, dv * ((r0 + r1) / 2) - Z * 0.003, dv, ((r1 - r0) / 2, rng.uniform(0.03, 0.06), 0.003), 0.002, k=0.012))
        if rng.random() < 0.45:  # Stuecke haengen schraeg herab
            q = dv * (r0 + rng.uniform(0.0, 0.03))
            crack.append(A.box(B_CRACK, q - Z * 0.024, A.unit(dv * 0.5 - Z * 0.85), (0.028, rng.uniform(0.012, 0.022), 0.006), 0.002, up=dv, k=0.004))
    crack.append(A.ell(B_CRACK, Z * 0.01, Z, (0.03, 0.135, 0.135), up=X, op='sub', k=0.01))
    dark.append(A.ell(B_CRACK, -Z * 0.002, Z, (0.0016, 0.14, 0.14), up=X))
    for i in range(7):
        a = i * 2 * math.pi / 7 + rng.uniform(-0.3, 0.3)
        pts = [np.array([math.cos(a), math.sin(a), 0.0]) * 0.16]
        ang = a
        for j in range(5):
            ang += rng.uniform(-0.5, 0.5)
            pts.append(pts[-1] + np.array([math.cos(ang), math.sin(ang), 0.0]) * rng.uniform(0.04, 0.08))
        for j in range(len(pts) - 1):
            dark.append(A.box(B_CRACK, (pts[j] + pts[j + 1]) / 2 - Z * 0.0012, pts[j + 1] - pts[j],
                              (np.linalg.norm(pts[j + 1] - pts[j]) / 2 + 0.002, 0.004 * (1 - j / 6) + 0.0015, 0.0012), 0.0005, up=Z))
    # --- verbrannte Hautfetzen (flach, gerollt) und Rauch -----------------------------------------------------------
    shreds, embers = [], []
    for k in range(KSHREDS):
        a = J[SHRED0 + k * 2]
        pts = [a + X * (u * 0.08 - 0.04) + Z * (0.02 * (u - 0.5) ** 2 * 4) + Y * (0.006 * math.sin(u * 5 + k)) for u in np.linspace(0, 1, 5)]
        for j in range(len(pts) - 1):
            shreds.append(A.ell(B_SHRED0 + k, (pts[j] + pts[j + 1]) / 2, pts[j + 1] - pts[j], (0.013, 0.022 + 0.006 * math.sin(j + k), 0.0022), k=0.004))
        embers += [A.ell(B_SHRED0 + k, pts[i] + Z * 0.004 + Y * 0.007 * (i - 2), X, (0.016, 0.011, 0.003), k=0.0) for i in (1, 3)]  # Glut
    smoke = []
    for k in range(KSMOKE):
        a = J[SMOKE0 + k * 2]
        pts = [a + Z * (0.07 * u) + X * (0.015 * math.sin(u * 3.0 + k)) + Y * (0.012 * math.cos(u * 2.0 + k)) for u in range(5)]
        for j in range(len(pts) - 1):
            smoke.append(A.cone(B_SMOKE0 + k, pts[j], pts[j + 1], 0.03 + 0.008 * j, 0.034 + 0.008 * j, k=0.02))

    def rot_disp(Q):  # verwesende Haut: Querfalten, Beulen, abgeschabte Stellen
        rings = sdf.ridged(Q * np.array([0.5, 0.5, 2.2]), 40, 2, seed=1501)
        d1, d2 = sdf.worley(Q, 45, seed=1502)
        lumps = np.clip(1.0 - d1 / 0.35, 0, 1) ** 2
        slough = np.clip(sdf.fbm(Q, 18, 3, seed=1504) * 3.0 - 0.6, 0, 1)  # abgeloeste, eingesunkene Flaechen
        return -0.0022 * rings + 0.0024 * lumps - 0.0012 * sdf.fbm(Q, 140, 2, seed=1503) - 0.002 * slough

    def flesh_disp(Q):  # nass, faserig, knotig
        return 0.0015 * sdf.fbm(Q, 120, 2, seed=1511) - 0.001 * sdf.ridged(Q * np.array([1, 1, 0.3]), 90, 2, seed=1512)

    def horn_disp(Q):  # Wachstumsrillen, Splitter
        return -0.0004 * sdf.ridged(Q, 160, 2, seed=1521)

    def concrete_disp(Q):
        d1, d2 = sdf.worley(Q, 70, seed=1531)
        return -0.0025 * np.clip(d2 - d1, 0, 0.4) + 0.0012 * sdf.fbm(Q, 160, 2, seed=1532)

    def smoke_disp(Q):
        return 0.006 * sdf.fbm(Q, 25, 3, seed=1541)

    return [
        {'name': 'arm', 'fm': 'ent_rotskin', 'parts': arm, 'disp': rot_disp, 'h': 0.003, 'merge': 'haut', 'cav_scale': 0.01, 'cav_extra': flesh, 'lod_weight': 1.4},
        {'name': 'hand', 'fm': 'ent_rotskin', 'parts': hand, 'disp': rot_disp, 'h': 0.0025, 'merge': 'haut', 'cav_scale': 0.008, 'lod_weight': 1.3},
        {'name': 'lappen', 'fm': 'ent_rotskin', 'parts': flaps, 'disp': None, 'h': 0.0012, 'merge': 'haut', 'lod_weight': 0.6},
        {'name': 'fleisch', 'fm': 'ent_nestflesh', 'parts': flesh, 'disp': flesh_disp, 'h': 0.0022, 'cav_scale': 0.006, 'lod_weight': 1.0},
        {'name': 'sehnen', 'fm': 'ent_tooth', 'parts': tendon, 'disp': None, 'h': 0.0012, 'lod_weight': 0.4},
        {'name': 'krallen', 'fm': 'ent_horn', 'parts': horn, 'disp': horn_disp, 'h': 0.0012, 'cav_scale': 0.004, 'lod_weight': 1.2},
        {'name': 'riss', 'fm': 'ent_concrete', 'parts': crack, 'disp': concrete_disp, 'h': 0.0025, 'cav_scale': 0.006, 'lod_weight': 0.35},
        {'name': 'loch', 'fm': 'ent_umbra', 'parts': dark, 'disp': None, 'h': 0.0012, 'lod_weight': 0.12},
        {'name': 'fetzen', 'fm': 'ent_darkskin', 'parts': shreds, 'disp': None, 'h': 0.0012, 'lod_weight': 0.3},
        {'name': 'glut', 'fm': 'ent_hornglow', 'parts': embers, 'disp': None, 'h': 0.001, 'lod_weight': 0.1},
        {'name': 'rauch', 'fm': 'ent_smoke', 'parts': smoke, 'disp': smoke_disp, 'h': 0.004, 'lod_weight': 0.15},
    ]
