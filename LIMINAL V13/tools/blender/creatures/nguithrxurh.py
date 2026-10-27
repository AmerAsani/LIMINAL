"""V13.3: Nguithr'xurh (Entity 16, Wikidot entity-16, Rev. 39) - KREATUR_Nguithrxurh.md. Die Spinne.

Eine Walzenspinne (Solifugae) mit 16 Gliedmassen: zwoelf Laufbeine, zwei Pedipalpen (dick, beinartig, mit blassen
Haftkissen an der Spitze), zwei riesige Cheliceren (zweiteilige Scheren mit Zahnreihen, glaenzend dunkle Spitzen). Grosses,
gewoelbtes Prosoma mit Augenhuegel (zwei Augen, die das Lampenlicht zurueckwerfen), weicher, gegliederter Hinterleib.
Alles dicht behaart: sandfarben, lange helle Borsten stehen ab. Mittlere Groesse (die Seite: etwa 10 x 18 cm); im Spiel je
Ebene skaliert. Dazu der Faden aus den Spinnwarzen (3 m, nur beim Abseilen sichtbar).

Knochen (ent_nguithrxurh, ng in src/anim/creature.hpp): 1 Prosoma, 2/3 Hinterleib, 4/5 Cheliceren, 6..9 Pedipalpen,
10 + 3 * Bein + i Beine (Bein = Paar * 2 + Seite), 46 Faden.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (26000, 9000, 3000)
try:  # Vorschaufarben (build.py laeuft als __main__)
    import __main__
    __main__.PREVIEW_RGB.update({'ent_solifuge': (0.55, 0.42, 0.28), 'ent_silk': (0.85, 0.84, 0.8), 'ent_sedative': (0.85, 0.82, 0.62),
                                 'ent_pipe_a': (0.85, 0.15, 0.5), 'ent_pipe_b': (0.12, 0.62, 0.8), 'ent_pipe_c': (0.88, 0.75, 0.12),
                                 'ent_pipe_d': (0.4, 0.8, 0.15), 'ent_wire': (0.7, 0.7, 0.68)})
except Exception:
    pass

ROOT, PRO, EYES, OPI0, OPI1, OPI2 = 0, 1, 2, 3, 4, 5
CHB = (6, 8)
PPB = (10, 13)
LEG0 = 16
DRAG_A = 64
B_PRO, B_OPI0, B_OPI1 = 1, 2, 3
B_CH = (4, 5)
B_PP = (6, 8)
B_LEG0, B_DRAG = 10, 46
Z = np.array([0.0, 0.0, 1.0])


def legJ(l, i):
    return LEG0 + l * 4 + i


def legB(l, i):
    return B_LEG0 + l * 3 + i


def bristles(bone, a, b, r, n, rng, lo=0.004, hi=0.012, up_bias=0.4):
    """Abstehende Borsten entlang a-b auf einem Glied mit Radius r."""
    out = []
    d = A.unit(b - a)
    for _ in range(n):
        t = rng.random()
        e1 = A.perp(d)
        e2 = np.cross(d, e1)
        ang = rng.uniform(0, 2 * math.pi)
        nrm = e1 * math.cos(ang) + e2 * math.sin(ang)
        p = a + (b - a) * t + nrm * r * 0.8
        q = p + A.unit(nrm + d * 0.6 + Z * up_bias + rng.normal(0, 0.2, 3)) * rng.uniform(lo, hi)
        out.append(A.cone(bone, p, q, 0.00045, 0.00012))
    return out


def build(J, bones):
    rng = np.random.default_rng(16)
    J = np.asarray(J, dtype=np.float64)
    groups = []
    body, chit, eyes, pale, silk = [], [], [], [], []
    # --- Prosoma: gewoelbter Kopfschild, Augenhuegel -----------------------------------------------------------------
    body.append(A.ell(B_PRO, (0.022, 0, 0.034), (1, 0, 0), (0.031, 0.027, 0.019), k=0.0))
    body.append(A.ell(B_PRO, (0.046, 0, 0.040), (1, 0, 0.2), (0.019, 0.024, 0.017), k=0.008))
    body.append(A.ell(B_PRO, (0.051, 0, 0.051), (1, 0, 0), (0.007, 0.008, 0.006), k=0.004))  # Augenhuegel
    for s in (1, -1):
        eyes.append(A.ell(B_PRO, (0.0545, s * 0.0047, 0.0548), A.unit((1, s * 0.5, 0.3)), (0.0042, 0.0042, 0.0042), k=0.0))
    # Brustplatten unten, wo die Beine ansetzen
    body.append(A.ell(B_PRO, (0.012, 0, 0.024), (1, 0, 0), (0.03, 0.022, 0.009), k=0.006))
    # --- Hinterleib: weich, gegliedert (Ringe in der Verschiebung), wulstig ------------------------------------------
    segs = [(-0.012, 0.020, 0.019, 0.035), (-0.03, 0.026, 0.024, 0.036), (-0.05, 0.030, 0.027, 0.037), (-0.07, 0.030, 0.026, 0.036),
            (-0.088, 0.026, 0.022, 0.034), (-0.103, 0.019, 0.016, 0.031), (-0.114, 0.011, 0.009, 0.029)]
    for x, ry, rz, z in segs:
        b = B_OPI0 if x > -0.065 else B_OPI1
        body.append(A.ell(b, (x, 0, z), (1, 0, 0), (0.014, ry, rz), k=0.008))
    # Runde 1: einfarbig wie Holz - dunkle Platten: Kopfschild (Propeltidium) und je Hinterleibsring ein Rueckenschild
    plates = [A.ell(B_PRO, (0.03, 0, 0.050), (1, 0, 0.15), (0.024, 0.019, 0.0045), k=0.0)]
    for x, ry, rz, z in segs[:-1]:
        b = B_OPI0 if x > -0.065 else B_OPI1
        plates.append(A.ell(b, (x, 0, z + rz * 0.93), (1, 0, 0), (0.0085, ry * 0.62, 0.0032), k=0.0))
    # --- Cheliceren: Grundglied behaart, Scheren dunkel und glaenzend mit Zaehnen --------------------------------------
    for si, s in enumerate((1, -1)):
        b = B_CH[si]
        base = J[CHB[si]]
        tip = J[CHB[si] + 1]
        d = A.unit(tip - base)
        body.append(A.ell(b, base + d * 0.014 + Z * 0.004, d, (0.028, 0.0135, 0.0175), up=Z, k=0.004))  # Runde 1: zu klein
        # fester Finger (oben) und beweglicher Finger (unten), leicht gebogen
        for fz, curl, rr in ((0.004, -0.5, 0.0052), (-0.004, 0.6, 0.0046)):
            p0 = base + d * 0.032 + Z * fz
            p1 = p0 + d * 0.024 + Z * (fz * 0.4)
            p2 = p1 + A.unit(d + Z * curl * 0.5 - np.array([0, s * 0.15, 0])) * 0.022
            chit += A.chain(lambda i, b=b: b, [p0, p1, p2], [rr * 1.25, rr * 0.9, 0.0008], k=0.002)
            for t in np.linspace(0.12, 0.88, 7):  # Zahnreihe innen
                q = p0 + (p2 - p0) * t
                inward = A.unit(-Z * np.sign(fz) + np.array([0, -s * 0.3, 0]))
                chit.append(A.cone(b, q, q + inward * rng.uniform(0.0025, 0.004), 0.0012, 0.0002))
        body += bristles(b, base, base + d * 0.03, 0.012, 14, rng, 0.004, 0.009)
    # --- Pedipalpen: dick, beinartig, behaart, mit blassem Haftkissen ---------------------------------------------------
    for si in range(2):
        a, k, t = J[PPB[si]], J[PPB[si] + 1], J[PPB[si] + 2]
        body += A.chain(lambda i, si=si: B_PP[si] + min(i, 1), [a, k, t], [0.0062, 0.0052, 0.0042], k=0.002)
        pale.append(A.ell(B_PP[si] + 1, t + A.unit(t - k) * 0.002, t - k, (0.0055, 0.0047, 0.0047), k=0.0))
        body += bristles(B_PP[si], a, k, 0.006, 10, rng, 0.004, 0.01)
        body += bristles(B_PP[si] + 1, k, t, 0.005, 10, rng, 0.004, 0.01)
    # --- Beine: zwoelf, behaart, dunkle Gelenkringe --------------------------------------------------------------------
    legs = []
    for l in range(12):
        p = l // 2
        pts = [J[legJ(l, i)] for i in range(4)]
        radii = [0.0050 - 0.0001 * p, 0.0037, 0.0024, 0.0010]  # Runde 1: gleich dick wie Staebe
        lp = A.chain(lambda i, l=l: legB(l, i), pts, radii, k=0.0015)
        lp.append(A.ell(legB(l, 0), pts[0], pts[1] - pts[0], (0.005, 0.0045, 0.0045), k=0.002))  # Huefte
        lp += bristles(legB(l, 0), pts[0], pts[1], radii[0], 18, rng, 0.006, 0.016)
        lp += bristles(legB(l, 1), pts[1], pts[2], radii[1], 20, rng, 0.005, 0.015)
        lp += bristles(legB(l, 2), pts[2], pts[3], radii[2], 10, rng, 0.003, 0.009)
        rings = [A.ell(legB(l, 0), pts[1], pts[2] - pts[1], (0.0052, 0.0052, 0.0052), k=0.0),
                 A.ell(legB(l, 1), pts[2], pts[3] - pts[2], (0.0036, 0.0036, 0.0036), k=0.0),
                 A.cone(legB(l, 2), pts[3] + A.unit(pts[3] - pts[2]) * -0.004, pts[3], 0.0012, 0.0004)]  # Klaue
        legs.append((lp, rings))
    # Borsten am Koerper
    body += bristles(B_PRO, np.array([0.0, 0, 0.05]), np.array([0.05, 0, 0.055]), 0.022, 40, rng, 0.005, 0.012, 0.9)
    body += bristles(B_OPI0, np.array([-0.02, 0, 0.05]), np.array([-0.06, 0, 0.06]), 0.026, 34, rng, 0.004, 0.01, 0.7)
    body += bristles(B_OPI1, np.array([-0.07, 0, 0.055]), np.array([-0.11, 0, 0.045]), 0.022, 30, rng, 0.004, 0.01, 0.6)
    # Faden aus den Spinnwarzen: 3 m nach oben (nur beim Abseilen zu sehen)
    tip = J[OPI2]
    silk.append(A.cone(B_DRAG, tip, tip + Z * 3.0, 0.0006, 0.0005))

    def hair_disp(P):  # Haarstraehnen, Ringe des Hinterleibs
        ring = 0.5 + 0.5 * np.sin(P[:, 0] * 330.0)
        back = (P[:, 0] < -0.012).astype(float)
        return -0.0005 * (sdf.ridged(P * np.array([1.0, 2.2, 2.2]), 900, 2, seed=1601) - 0.5) + 0.0007 * ring * back

    groups.append({'name': 'koerper', 'fm': 'ent_solifuge', 'parts': [p for p in body if p.bone in (B_PRO, B_OPI0, B_OPI1) and p.g.get('ra', 1) > 0.001],
                   'disp': hair_disp, 'h': 0.00035, 'cav_scale': 0.003, 'merge': 'koerper'})
    rest = [p for p in body if not (p.bone in (B_PRO, B_OPI0, B_OPI1) and p.g.get('ra', 1) > 0.001)]
    # Borsten und Glieder in kleinen Gruppen (verstreut)
    by_bone = {}
    for p in rest:
        by_bone.setdefault(p.bone, []).append(p)
    for b, ps in by_bone.items():
        groups.append({'name': f'haar{b}', 'fm': 'ent_solifuge', 'parts': ps, 'disp': None, 'h': 0.00025, 'cav_scale': 0.0015,
                       'merge': 'koerper', 'lod_weight': 1.3})
    for l, (lp, rings) in enumerate(legs):  # je Glied ein kleines Gitter (die Beine spreizen weit)
        for i in range(3):
            ps = [p for p in lp if p.bone == legB(l, i)]
            groups.append({'name': f'bein{l}_{i}', 'fm': 'ent_solifuge', 'parts': ps, 'disp': None, 'h': 0.00034 if i == 0 else 0.00026,
                           'cav_scale': 0.0015, 'merge': 'koerper', 'lod_weight': 1.3})
        for i, rp in enumerate(rings):
            groups.append({'name': f'ring{l}_{i}', 'fm': 'ent_chitin', 'parts': [rp], 'disp': None, 'h': 0.00022, 'cav_scale': 0.001,
                           'merge': 'chitin'})
    for si in range(2):
        groups.append({'name': f'schere{si}', 'fm': 'ent_chitin', 'parts': [p for p in chit if p.bone == B_CH[si]], 'disp': None,
                       'h': 0.0002, 'cav_scale': 0.0012, 'merge': 'chitin', 'lod_weight': 1.5})
        groups.append({'name': f'kissen{si}', 'fm': 'ent_nestegg', 'parts': [p for p in pale if p.bone == B_PP[si] + 1], 'disp': None,
                       'h': 0.0002, 'cav_scale': 0.001, 'merge': 'kissen'})
    for i, pl in enumerate(plates):
        groups.append({'name': f'platte{i}', 'fm': 'ent_chitin', 'parts': [pl], 'disp': None, 'h': 0.00025, 'cav_scale': 0.001, 'merge': 'chitin'})
    groups.append({'name': 'augen', 'fm': 'ent_eyeshine', 'parts': eyes, 'disp': None, 'h': 0.00015, 'cav_scale': 0.0008, 'lod_weight': 2.0})
    groups.append({'name': 'faden', 'fm': 'ent_silk', 'parts': silk, 'disp': None, 'h': 0.0003, 'cav_scale': 0.0008, 'lod_weight': 0.3})
    return groups
