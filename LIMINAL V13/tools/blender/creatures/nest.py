"""V13: Deathmoth-Nest (Entity 4) - KREATUR_Deathmoth.md, Abschnitt Nest. Keine Blasenwuelste mehr: eine Masse, die
Schicht um Schicht ueber einem Tuerrahmen aufgebaut ist - flache, uebereinander gegossene Lagen wie Wachs und Harz,
nach oben dichter und dicker, am Sturz die dickste Masse. In den Lagen stecken Reste: Knochenstuecke, Rippen,
Stofffetzen, Pflanzenfasern. Am inneren Pfosten eine Wabe aus Kammern mit Eiern (Gelege). Bernsteinfarbenes Harz
haengt in Tropfen vom Sturz und spannt Faeden zur Wand; ueber der Oeffnung eine fleischige Wucherung und Reste einer
zerrissenen Haut. Masse wie das alte Nest: Oeffnung 1 m breit, 2,2 m hoch (anim/creature.hpp ns::kOpenW).

Knochen (ent_nest): 1 Koerper (die ganze Masse pulsiert mit ihm).
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (38000, 11000, 3200)
BODY = 1
H, HW = 2.2, 0.5
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def build(J, bones):
    rng = np.random.default_rng(7)
    R = rng.random
    mass, flesh, bone_p, cloth, fibers, cells, eggs, resin, membrane = [], [], [], [], [], [], [], [], []
    # --- Pfosten: uebereinander gegossene, flache Lagen, nach oben dicker -------------------------------------------
    for sg in (1, -1):
        # Runde 2: ein Kern fuellt die Luecken, die Lagen ueberlappen und verschmelzen (vorher Stapel aus Tellern)
        mass.append(A.cone(BODY, (-0.03, sg * (HW + 0.05), 0.0), (-0.02, sg * (HW + 0.04), H), 0.085, 0.13, k=0.0))
        z = 0.0
        while z < H + 0.05:
            t = z / H
            th = 0.022 + 0.030 * R()
            r = 0.06 + 0.13 * t + 0.07 * R()
            c = np.array([-0.05 + 0.18 * (R() - 0.5), sg * (HW + 0.04 + 0.09 * (R() - 0.35)), z + th * 0.5])
            ax = np.array([1.0, 0.6 * (R() - 0.5), 0.0])
            up = A.unit(Z + 0.35 * (R() - 0.5) * Y + 0.35 * (R() - 0.5) * X)
            mass.append(A.ell(BODY, c, ax, (r * (0.8 + 0.6 * R()), r * (0.6 + 0.5 * R()), th), up=up, k=0.04))
            z += th * (0.55 + 0.35 * R())
    # --- Sturz: dickste Masse, Lagen ueber die ganze Breite, unten durchhaengend --------------------------------------
    for l in range(9):
        z = H - 0.09 + 0.042 * l
        n = 4 + int(R() * 2)
        for i in range(n):
            y = -0.72 + 1.44 * (i + 0.5 + 0.4 * (R() - 0.5)) / n
            sag = 0.05 * (1 - (y / 0.75) ** 2) if l < 3 else 0.0
            c = np.array([-0.04 + 0.12 * (R() - 0.5), y, z - sag])
            mass.append(A.ell(BODY, c, Y + 0.3 * (R() - 0.5) * X, (0.22 + 0.14 * R(), 0.14 + 0.12 * R(), 0.026 + 0.02 * R()),
                              up=A.unit(Z + 0.25 * (R() - 0.5) * Y), k=0.04))
    # Runde 3: senkrechte Laeufe ueber die Lagen - die Masse ist heruntergeflossen, nicht gestapelt
    for k in range(16):
        sg = 1 if k % 2 == 0 else -1
        ang = R() * 2 * math.pi
        z0 = 0.8 + 1.4 * R()
        c0 = np.array([-0.04 + 0.16 * math.cos(ang), sg * (HW + 0.05) + 0.12 * math.sin(ang), z0])
        ln = 0.25 + 0.55 * R()
        mass.append(A.cone(BODY, c0, c0 - Z * ln + np.array([0.02 * (R() - 0.5), 0.02 * (R() - 0.5), 0]),
                           0.030 + 0.02 * R(), 0.008, k=0.03))
    # --- auf die Wand gekrochen und am Boden ausgelaufen: flache Fladen ------------------------------------------------
    for k in range(18):
        above = R() < 0.4
        y = -0.8 + 1.6 * R() if above else (1 if R() < 0.5 else -1) * (HW + 0.10 + 0.22 * R())
        z = H + 0.06 + 0.3 * R() if above else 0.15 + 2.0 * R() ** 0.6
        r = 0.06 + 0.08 * R()
        mass.append(A.ell(BODY, (0.015 + 0.02 * R(), y, z), X, (0.022 + 0.012 * R(), r, r * (0.7 + 0.5 * R())), k=0.04))
    for k in range(8):
        y = (1 if k % 2 == 0 else -1) * (0.3 + 0.35 * R())
        mass.append(A.ell(BODY, (0.22 * R() - 0.06, y, 0.0), X, (0.13, 0.10, 0.025 + 0.015 * R()), k=0.05))
    # --- fleischige Wucherung ueber der Oeffnung -----------------------------------------------------------------------
    for k in range(9):
        c = np.array([0.06 + 0.06 * R(), -0.45 + 0.9 * R(), H + 0.12 + 0.14 * R()])
        r = 0.06 + 0.06 * R()
        flesh.append(A.ell(BODY, c, A.unit(X + 0.5 * (R() - 0.5) * Y), (r * 1.1, r * 1.3, r * 0.9), k=0.04))
    # Reste einer zerrissenen Haut unter dem Sturz - Runde 3: duenne, zum Ende auslaufende Lappen (vorher rechteckige
    # Bahnen wie Pappe), dunkles, nasses Gewebe
    for k in range(9):
        y0 = -0.44 + 0.88 * (k + 0.5) / 9 + 0.03 * (R() - 0.5)
        top = np.array([0.02, y0, H - 0.07])
        ln = 0.14 + 0.36 * R()
        pts = A.bezier(top, top + np.array([0.012 * (R() - 0.5), 0.03 * (R() - 0.5), -ln * 0.5]),
                       top + np.array([0.02 * (R() - 0.5), 0.05 * (R() - 0.5), -ln]), 5)
        w0 = 0.035 + 0.025 * R()
        for i in range(len(pts) - 1):
            f0, f1 = i / (len(pts) - 1), (i + 1) / (len(pts) - 1)
            membrane.append(A.tube(BODY, pts[i], pts[i + 1], (0.004, w0 * (1 - 0.8 * f0)), (0.004, w0 * (1 - 0.8 * f1)), up=Y, k=0.006))
        for h in range(int(R() * 2)):  # einzelne Risse
            ang = R() * math.pi
            membrane.append(A.ell(BODY, pts[1 + int(R() * 3)], A.unit(Y * math.cos(ang) + Z * math.sin(ang)),
                                  (0.008 + 0.014 * R(), 0.02, 0.004 + 0.006 * R()), up=X, op='sub', k=0.003))
    # --- Reste in den Lagen: Knochen, Rippen, Stoff, Fasern -----------------------------------------------------------
    for k in range(7):
        sg = 1 if k % 2 == 0 else -1
        on_lintel = k >= 5
        c = np.array([0.04 + 0.05 * R(), -0.4 + 0.8 * R(), H + 0.05]) if on_lintel else np.array([0.06 + 0.04 * R(), sg * (HW + 0.03), 0.3 + 1.7 * R()])
        d = A.unit(np.array([0.3 * (R() - 0.5), 1.0 if on_lintel else 0.4 * (R() - 0.5), 0.0 if on_lintel else 1.0]) + 0.4 * (rng.random(3) - 0.5))
        Lb = 0.22 + 0.16 * R()
        a, b = c - d * Lb * 0.5, c + d * Lb * 0.5
        rb = 0.011 + 0.004 * R()
        bone_p.append(A.cone(BODY, a, b, rb, rb * 0.85, k=0.0))
        for e, s2 in ((a, 1), (b, -1)):  # Gelenkenden mit zwei Knorren
            side = A.perp(d)
            bone_p.append(A.ell(BODY, e + side * rb * 0.6, d, (rb * 1.6, rb * 1.5, rb * 1.4), k=0.006))
            bone_p.append(A.ell(BODY, e - side * rb * 0.6, d, (rb * 1.5, rb * 1.4, rb * 1.3), k=0.006))
    for k in range(5):  # Rippenboegen
        sg = 1 if k % 2 == 0 else -1
        c = np.array([0.08, sg * (HW + 0.02), 0.6 + 1.3 * R()])
        pts = A.bezier(c + np.array([0, -sg * 0.06, 0.1]), c + np.array([0.09, 0, 0]), c + np.array([0.02, sg * 0.08, -0.12]), 6)
        bone_p += A.chain(lambda i: BODY, pts, list(np.linspace(0.008, 0.005, len(pts))), k=0.003)
    for k in range(6):  # Stofffetzen: gewellte, duenne Streifen, halb eingegossen
        sg = 1 if k % 2 == 0 else -1
        c = np.array([0.07 + 0.03 * R(), sg * (HW + 0.04 * (R() - 0.5)), 0.4 + 1.6 * R()]) if k < 4 else np.array([0.07, -0.5 + R(), H - 0.02])
        d = A.unit(np.array([0.2, 0.6 * (R() - 0.5), -1.0]))
        side = A.unit(np.cross(d, X))
        for i in range(6):
            p = c + d * (0.025 * i) + X * (0.008 * math.sin(i * 1.7 + k))
            cloth.append(A.box(BODY, p, d, (0.016, 0.035 - 0.003 * i, 0.0018), 0.001, up=X + 0.4 * math.cos(i * 1.7 + k) * side, k=0.004))
    for k in range(26):  # Pflanzenfasern, die aus den Lagen ragen
        sg = 1 if k % 2 == 0 else -1
        c = np.array([0.05 + 0.04 * R(), sg * (HW + 0.06 * (R() - 0.3)), 0.2 + 2.0 * R()])
        d = A.unit(np.array([0.6 + 0.4 * R(), sg * (R() - 0.3), 0.6 * (R() - 0.5)]))
        pts = A.bezier(c - d * 0.03, c + d * 0.05 + Z * 0.01, c + d * (0.09 + 0.06 * R()) - Z * 0.02, 4)
        fibers += A.chain(lambda i: BODY, pts, [0.0022, 0.0018, 0.0014, 0.0010, 0.0005], k=0.001)
    # --- Wabe mit Gelege am inneren linken Pfosten ----------------------------------------------------------------------
    base = np.array([0.03, HW - 0.05, 1.55])
    n = -Y  # zur Oeffnung hin
    cells.append(A.ell(BODY, base, Z, (0.20, 0.07, 0.045), up=n, k=0.02))  # erhabene Wabe
    for i in range(5):
        for j in range(4):
            off = Z * (-0.15 + 0.075 * i + 0.0375 * (j % 2)) + X * (-0.04 + 0.032 * j)
            p = base + off + n * 0.045
            cells.append(A.cone(BODY, p + n * 0.015, p - n * 0.03, 0.0145, 0.012, op='sub', k=0.003))
            if R() < 0.8:
                eggs.append(A.ell(BODY, p - n * 0.014, n, (0.008, 0.006, 0.006), k=0.0))
                if R() < 0.5:
                    eggs.append(A.ell(BODY, p - n * 0.016 + X * 0.007, n, (0.007, 0.0055, 0.0055), k=0.0))
    # --- Harz: Tropfen vom Sturz, Faeden zur Wand ------------------------------------------------------------------------
    for k in range(11):
        top = np.array([0.04 * R(), -0.44 + 0.88 * (k + R()) / 11, H - 0.10 - 0.04 * R()])
        ln = 0.12 + 0.40 * R()
        bot = top + np.array([0.02 * (R() - 0.5), 0.02 * (R() - 0.5), -ln])
        mid = A.lerp(top, bot, 0.5)
        resin += [A.cone(BODY, top, mid, 0.012, 0.006, k=0.006), A.cone(BODY, mid, bot, 0.006, 0.003, k=0.004),
                  A.ell(BODY, bot - Z * 0.004, Z, (0.0065, 0.0065, 0.0085), k=0.003)]
    for k in range(10):
        sg = 1 if k % 2 == 0 else -1
        a = np.array([0.05 + 0.03 * R(), sg * (HW + 0.10 + 0.05 * R()), 0.4 + 1.8 * R()])
        b = np.array([0.012, sg * (HW + 0.28 + 0.25 * R()), a[2] + 0.2 * (R() - 0.5)])
        m = A.lerp(a, b, 0.5) - Z * (0.03 + 0.04 * R())
        resin += A.chain(lambda i: BODY, A.bezier(a, m, b, 5), [0.004, 0.003, 0.0022, 0.0018, 0.0022, 0.003], k=0.003)

    # --- Verschiebungen ----------------------------------------------------------------------------------------------------
    def mass_disp(Q):
        # Lagen: jede Lage woelbt sich an ihrem unteren Rand vor (Lippe), darueber ein Absatz - wie gegossenes Wachs
        ph = Q[:, 2] / 0.030 + 1.4 * sdf.fbm(Q * np.array([1.0, 1.0, 0.3]), 4, 2, seed=91)  # Runde 2: feiner, welliger
        s = ph - np.floor(ph)
        d = -0.0035 * (1 - s) ** 2  # Runde 3: flachere Lippen (vorher Schraube)
        d -= 0.004 * sdf.fbm(Q, 12, 3, seed=92)                    # Beulen
        d -= 0.0009 * (sdf.ridged(Q, 70, 2, seed=93) - 0.5)        # Fasern und Risse in der Oberflaeche
        return d

    def flesh_disp(Q):  # Runde 2: ausgefranste Raender, Runzeln
        return -0.004 * sdf.fbm(Q, 16, 3, seed=94) - 0.0012 * (sdf.ridged(Q, 60, 2, seed=95) - 0.5) - 0.002 * sdf.fbm(Q, 45, 2, seed=97)

    def bone_disp(Q):
        return -0.0006 * sdf.fbm(Q, 120, 2, seed=96)

    allmass = [p for p in mass if p.op == 'add']
    groups = [
        {'name': 'masse', 'fm': 'ent_nest', 'parts': mass, 'disp': mass_disp, 'h': 0.0045, 'cav_scale': 0.02},
        {'name': 'fleisch', 'fm': 'clump_core', 'parts': flesh, 'disp': flesh_disp, 'h': 0.0035, 'cav_scale': 0.015, 'cav_extra': allmass},
        {'name': 'wabe', 'fm': 'ent_nest', 'parts': cells, 'disp': None, 'h': 0.0015, 'cav_scale': 0.008},
        {'name': 'haut', 'fm': 'ent_mouth', 'parts': membrane, 'disp': flesh_disp, 'h': 0.0015, 'cav_scale': 0.006, 'cav_extra': allmass},
        {'name': 'eier', 'fm': 'ent_tooth', 'parts': eggs, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
    ]
    # verstreute feine Teile: in kleine Haufen teilen (je ein kleines Gitter), danach je Material ein Netz
    for name, fm, parts, h, disp in (('knochen', 'ent_horn', bone_p, 0.0018, bone_disp), ('stoff', 'ent_fur', cloth, 0.0012, None),
                                     ('fasern', 'ent_fur', fibers, 0.0008, None), ('harz', 'ent_resin', resin, 0.0012, None)):
        for i, cl in enumerate(cluster(parts, 0.30)):
            groups.append({'name': f'{name}{i}', 'fm': fm, 'parts': cl, 'disp': disp, 'h': h, 'cav_scale': 0.004,
                           'cav_extra': allmass, 'merge': fm})
    return groups


def cluster(parts, size):
    """Teile nach Lage in Haufen mit hoechstens size Kantenlaenge (gierig, in Reihenfolge der Mitte)."""
    out = []
    for p in sorted(parts, key=lambda q: tuple(np.round((q.lo + q.hi) / 2 / size).tolist())):
        for cl in out:
            lo = np.minimum(cl['lo'], p.lo)
            hi = np.maximum(cl['hi'], p.hi)
            if np.all(hi - lo <= size):
                cl['lo'], cl['hi'] = lo, hi
                cl['parts'].append(p)
                break
        else:
            out.append({'lo': p.lo.copy(), 'hi': p.hi.copy(), 'parts': [p]})
    return [c['parts'] for c in out]
