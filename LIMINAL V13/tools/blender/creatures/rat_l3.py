"""V13.3 Level 3: Death Rat in Level 3 (KREATUR_DeathRat.md, Abschnitt Level 3; Wikidot Level 3 Rev. 18, Entity 24 Rev. 25).

Die Level-3-Seite: so gross und so gebaut wie ein Wasserschwein, die Zaehne kuenstlich geschaerft (an den Gitterstaeben
gewetzt), menschlich klug - sie lesen und schreiben Englisch und reden ueber Sprachgeraete. Entity 24: schwarzes Fell, braune
Hoerner mit rot gluehenden Spitzen (die haeufigste Form).

Eigener Entwurf (keine skalierte Hausratte): wuchtiger Fassleib mit hohem, rundem Hinterteil und schweren Keulen, kein
Schwanz; ein stumpfer Wasserschweinkopf (tiefe, breite Schnauze, Nasenpolster oben, Augen und kleine Ohren hoch am Schaedel).
Das schwarze Fell ist grob und verfilzt, ueber dem Ruecken stehen Grannen-Bueschel ab; durch das Fell drueckt eine Reihe
Wirbel. An den Flanken reisst die Haut in langen Spalten auf (zu schnell gewachsen - Auslegung): darunter rohes, rotbraunes
Fleisch. Die Lippen sind vernarbt zurueckgezogen: Zahnfleisch und die orangen Nagezaehne liegen frei, die Spitzen zu Meisseln
geschliffen (helle Schlifffacetten). Zwei dicke Hoerner biegen sich nach hinten, die Spitzen gluehen. Vorn lange, nackte,
graue, fast menschliche Finger mit dunklen Naegeln (sie bauen Fallen - Auslegung), hinten drei Zehen mit hufartigen Naegeln.
Links am Hals mit Kabelbindern das Sprachgeraet: Blechkasten, Messinggitter des Lautsprechers, rote Leuchte, ein Kabel.

Massstab: entworfen in Einheiten des Rattenskeletts (wie rat.py, SCALE 0,62). Das Spiel vergroessert den Leib um kG und
streckt die Beine zusaetzlich um kLegK (creature_rat_l3.cpp) - darum sind die Beine hier duenn und kurz gebaut.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (46000, 16000, 5500)
SKELETON = 'ent_rat'
SCALE = 0.62
PELVIS, SPINE, NECK, HEAD, JAW = 1, 2, 3, 4, 5
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def build(J, bones):
    rng = np.random.default_rng(2403)
    J = np.asarray(J)
    # --- Grundform (Fell): Hinterteil, Keulen, Bauch, Brustkorb, Schulterbuckel, Hals, Kopf -------------------------------
    core = [A.ell(PELVIS, (-0.17, 0, 0.132), X, (0.10, 0.088, 0.094), k=0.0),
            A.ell(SPINE, (-0.03, 0, 0.108), X, (0.13, 0.092, 0.084), k=0.035),
            A.ell(SPINE, (0.075, 0, 0.126), X, (0.085, 0.078, 0.082), k=0.03),
            A.ell(SPINE, (0.05, 0, 0.18), X, (0.07, 0.05, 0.032), k=0.03),        # Schulterbuckel
            A.ell(NECK, (0.165, 0, 0.138), X, (0.055, 0.057, 0.062), k=0.03)]
    for s in (1, -1):
        core.append(A.ell(PELVIS, (-0.15, s * 0.068, 0.105), X, (0.075, 0.034, 0.064), k=0.02))   # Keulen
        core.append(A.ell(SPINE, (0.1, s * 0.06, 0.11), X, (0.045, 0.026, 0.05), k=0.02))         # Oberarm am Leib
    head = [A.ell(HEAD, (0.248, 0, 0.142), X, (0.058, 0.042, 0.046), k=0.0),                     # Schaedel
            A.ell(HEAD, (0.31, 0, 0.116), A.unit((1.0, 0, -0.2)), (0.045, 0.032, 0.04), k=0.022),           # Schnauze, tief und stumpf
            A.ell(HEAD, (0.335, 0, 0.12), A.unit((1.0, 0, -0.3)), (0.02, 0.028, 0.03), k=0.012),
            A.ell(JAW, (0.296, 0, 0.074), A.unit((1.0, 0, -0.15)), (0.036, 0.026, 0.015), k=0.01)]
    for s in (1, -1):
        head.append(A.ell(HEAD, (0.258, s * 0.031, 0.166), X, (0.022, 0.012, 0.008), k=0.01))        # Brauenwulst
        head.append(A.ell(HEAD, (0.284, s * 0.033, 0.105), X, (0.04, 0.014, 0.03), k=0.014))         # Backen
        head.append(A.ell(HEAD, (0.262, s * 0.044, 0.157), X, (0.011, 0.01, 0.009), op='sub', k=0.004))  # Augenhoehle
    head.append(A.box(HEAD, (0.348, 0, 0.088), X, (0.014, 0.016, 0.011), 0.006, op='sub', k=0.006))     # Mund: Lippen zurueck
    # Wirbel: eine Reihe Knoten durch das Fell
    spine_knobs = []
    for i in range(11):
        t = i / 10
        x = -0.21 + 0.33 * t
        z = 0.218 - 0.03 * (2 * t - 1) ** 2 + (0.012 if 0.55 < t < 0.85 else 0.0)
        spine_knobs.append(A.ell(PELVIS if x < -0.07 else SPINE, (x, 0, z), X, (0.011, 0.012, 0.009), k=0.008))
    fur = core + head + spine_knobs
    # Spalten im Fell (Risse in der Haut): lang und schmal, schraeg ueber die Flanken
    rips = []
    for c, ax, r in (((-0.07, 0.093, 0.1), (1.0, 0, 0.35), (0.065, 0.016, 0.011)), ((-0.15, -0.098, 0.12), (1.0, 0, -0.5), (0.05, 0.016, 0.01)),
                     ((0.06, 0.085, 0.09), (1.0, 0, 0.7), (0.04, 0.016, 0.009)), ((-0.01, -0.09, 0.075), (1.0, 0, 0.15), (0.06, 0.016, 0.01)),
                     ((-0.2, 0.05, 0.2), (1.0, 0.4, 0.0), (0.035, 0.012, 0.016))):
        rips.append(A.ell(SPINE if c[0] > -0.06 else PELVIS, c, A.unit(ax), r, up=Y, op='sub', k=0.006))
    # kahl: Nasenpolster, Lippen, Augenraender
    rips.append(A.ell(HEAD, (0.35, 0, 0.13), A.unit((1.0, 0, 0.5)), (0.008, 0.021, 0.015), op='sub', k=0.006))
    fur_parts = fur + rips
    # Grannen-Bueschel: verfilzte Straehnen, die am Ruecken und an den Flanken abstehen (der Umriss franst aus)
    tufts = []
    for i in range(300):
        x = rng.uniform(-0.25, 0.16)
        a = rng.uniform(-1.9, 1.9)
        ry, rz = 0.088, 0.09
        c = np.array([x, math.sin(a) * ry, 0.12 + math.cos(a) * rz])
        if x > 0.12:
            c[2] -= 0.01
        n = A.unit(np.array([0.0, math.sin(a), math.cos(a)]))
        d = A.unit(n * 0.6 - X * 0.75 + rng.normal(0, 0.18, 3))
        L = rng.uniform(0.014, 0.03) * (1.35 if abs(a) < 0.5 else 1.0)
        bone = PELVIS if x < -0.07 else (SPINE if x < 0.13 else NECK)
        tufts.append(A.cone(bone, c - n * 0.004, c + d * L, 0.0042, 0.0006, k=0.003))
    for i in range(40):  # Kopf und Nacken
        a = rng.uniform(-1.6, 1.6)
        c = np.array([rng.uniform(0.2, 0.27), math.sin(a) * 0.045, 0.14 + math.cos(a) * 0.045])
        n = A.unit(np.array([0.0, math.sin(a), math.cos(a)]))
        d = A.unit(n * 0.5 - X * 0.8 + rng.normal(0, 0.15, 3))
        tufts.append(A.cone(HEAD if c[0] > 0.21 else NECK, c - n * 0.003, c + d * rng.uniform(0.008, 0.014), 0.003, 0.0005, k=0.002))

    def fur_disp(Q):
        strand = sdf.ridged(Q * np.array([0.25, 1.0, 1.0]), 95, 2, seed=2411)   # grobe Straehnen laengs
        clump = sdf.fbm(Q, 30, 3, seed=2412)                                    # Zotteln, verfilzt
        fine = sdf.ridged(Q * np.array([0.3, 1.0, 1.0]), 260, 1, seed=2422)     # einzelne Haarstraehnen
        return -0.0065 * (strand - 0.5) - 0.006 * (clump - 0.5) - 0.0025 * (fine - 0.5)

    # rohes Fleisch in den Spalten (fuellt den Grund der Rinne)
    flesh = []
    for r in rips[:5]:
        c = np.asarray(r.g['c'], dtype=float)
        inward = -np.array([0.0, np.sign(c[1]) if abs(c[1]) > 0.03 else 0.0, 1.0 if abs(c[1]) <= 0.03 else 0.0])
        flesh.append(A.ell(r.bone, c + inward * r.g['r'][2] * 0.5, r.g['R'][:, 0], r.g['r'] * np.array([0.88, 0.55, 0.55]), up=Y, k=0.0))

    def flesh_disp(Q):
        return -0.0016 * sdf.ridged(Q, 160, 2, seed=2418) - 0.0009 * sdf.fbm(Q, 300, 2, seed=2419)

    # nackte graue Haut: Nasenpolster, Lippenrand, Ohren
    skin = [A.ell(HEAD, (0.347, 0, 0.133), A.unit((1.0, 0, 0.5)), (0.006, 0.019, 0.013), k=0.0)]  # kleines Nasenpolster
    nostr = [A.ell(HEAD, (0.352, s * 0.009, 0.136), A.unit((1, s * 0.5, 0.6)), (0.005, 0.003, 0.0025), op='sub', k=0.002) for s in (1, -1)]
    lips = [A.ell(HEAD, (0.338, s * 0.019, 0.096), A.unit((0.3, s * 1.0, 0.1)), (0.012, 0.004, 0.007), k=0.003) for s in (1, -1)]
    gums = [A.ell(HEAD, (0.338, 0, 0.098), X, (0.01, 0.017, 0.009), k=0.003), A.ell(JAW, (0.326, 0, 0.074), X, (0.009, 0.014, 0.006), k=0.003)]
    ears = []
    for s in (1, -1):
        ears.append(A.ell(HEAD, (0.214, s * 0.041, 0.181), A.unit((-0.2, s * 0.5, 1.0)), (0.013, 0.011, 0.0042), up=X, k=0.004))
        ears.append(A.ell(HEAD, (0.216, s * 0.043, 0.183), A.unit((-0.2, s * 0.5, 1.0)), (0.0085, 0.0065, 0.003), up=X, op='sub', k=0.002))
    # --- Beine (duenn gebaut - das Spiel streckt sie um kLegK): oben Fell, vorn graue Haende, hinten Hufzehen --------------
    legs_fur, hands, nails = [], [], []
    for l in range(4):
        T, K, P, Tt = (J[8 + l * 4 + i] for i in range(4))
        b0 = 6 + l * 3
        front = l < 2
        sg = 1 if l % 2 == 0 else -1
        side = Y * sg
        fd = A.unit(np.array([Tt[0] - P[0], Tt[1] - P[1], 0.0]))
        if front:
            legs_fur.append(A.muscle(b0, T, K, 0.0125, flat=0.9, k=0.004))
            legs_fur.append(A.cone(b0, T, K, 0.0105, 0.0075, k=0.004))
            legs_fur.append(A.cone(b0 + 1, K, A.lerp(K, P, 0.55), 0.0078, 0.0064, k=0.003))
            hands.append(A.cone(b0 + 1, A.lerp(K, P, 0.4), P, 0.0062, 0.0055, k=0.003))                  # nackter Unterarm
            hands.append(A.ell(b0 + 2, P + fd * 0.006 + Z * 0.002, fd, (0.010, 0.0078, 0.0042), k=0.003))  # Handballen
            for f in range(5):  # Finger: lang, nackt, gegliedert - der aeussere als Daumen seitlich
                a = (f - 2.0) * 0.3
                d = A.unit(fd * math.cos(a) + np.cross(Z, fd) * math.sin(a) * sg)
                if f == 4:
                    d = A.unit(d * 0.4 - side * 0.9)
                L = 0.018 + (0.005 if f in (1, 2) else 0.0) - (0.006 if f == 4 else 0.0)
                p0 = P + fd * 0.008 + d * 0.004 + Z * 0.0016
                p1 = p0 + d * L * 0.45 + Z * 0.0014
                p2 = p1 + d * L * 0.33 - Z * 0.0008
                p3 = p2 + d * L * 0.25 - Z * 0.0026
                hands += A.chain(lambda i: b0 + 2, [p0, p1, p2, p3], [0.0019, 0.0017, 0.0015, 0.0011], k=0.0007)
                for q, rr in ((p1, 0.0019), (p2, 0.0016)):  # Knoechel
                    hands.append(A.ell(b0 + 2, q, d, (rr * 1.2, rr * 1.2, rr * 1.1), k=0.0005))
                nails.append(A.cone(b0 + 2, p3 - d * 0.001 + Z * 0.0004, p3 + d * 0.0035 - Z * 0.0016, 0.0012, 0.0003, k=0.0004))
        else:
            legs_fur.append(A.muscle(b0, T, K, 0.019, flat=0.9, k=0.005))     # Keule
            legs_fur.append(A.cone(b0, T, K, 0.013, 0.0085, k=0.004))
            legs_fur.append(A.cone(b0 + 1, K, A.lerp(K, P, 0.7), 0.0072, 0.0058, k=0.003))
            hands.append(A.cone(b0 + 1, A.lerp(K, P, 0.5), P, 0.0058, 0.005, k=0.003))
            hands.append(A.ell(b0 + 2, P + fd * 0.008 + Z * 0.0006, fd, (0.016, 0.0088, 0.004), k=0.003))  # Sohle
            for f in range(3):  # drei Zehen mit hufartigen Naegeln
                a = (f - 1.0) * 0.3
                d = A.unit(fd * math.cos(a) + np.cross(Z, fd) * math.sin(a))
                p0 = P + fd * 0.016 + Z * 0.001
                p1 = p0 + d * 0.012
                hands += A.chain(lambda i: b0 + 2, [p0, p1], [0.004, 0.0033], k=0.002)
                nails.append(A.ell(b0 + 2, p1 + d * 0.0024 - Z * 0.0006, d, (0.0036, 0.0032, 0.0022), k=0.0006))

    def hand_disp(Q):
        return -0.0005 * sdf.ridged(Q * np.array([1.0, 1.0, 0.6]), 500, 2, seed=2420) - 0.0003 * sdf.fbm(Q, 900, 1, seed=2421)

    # --- Zaehne: frei liegende Nagezaehne (orange Schmelzseite), Spitzen zu Meisseln geschliffen -----------------------------
    enamel, dentin = [], []
    for s in (1, -1):
        a = np.array([0.343, s * 0.006, 0.1])
        b = a + np.array([0.013, 0, -0.016])
        c = b + np.array([0.004, 0, -0.022])
        enamel += A.chain(lambda i: HEAD, [a, b, c], [0.0039, 0.0037, 0.0033], k=0.001)
        dentin.append(A.cone(HEAD, c + np.array([-0.0015, 0, 0.002]), c + np.array([0.0025, 0, -0.0085]), 0.0031, 0.0003, k=0.0005))
        a2 = np.array([0.326, s * 0.0052, 0.074])
        b2 = a2 + np.array([0.018, 0, 0.004])
        c2 = b2 + np.array([0.008, 0, 0.016])
        enamel += A.chain(lambda i: JAW, [a2, b2, c2], [0.0035, 0.0033, 0.003], k=0.001)
        dentin.append(A.cone(JAW, c2 + np.array([-0.002, 0, -0.001]), c2 + np.array([0.0035, 0, 0.0075]), 0.0028, 0.0003, k=0.0005))

    def tooth_disp(Q):
        return -0.0003 * sdf.ridged(Q * np.array([1.0, 1.0, 0.2]), 900, 1, seed=2416)  # Schleifriefen laengs

    # --- Augen, Hoerner ----------------------------------------------------------------------------------------------------
    eyes, shine, horns, tips = [], [], [], []
    for s in (1, -1):
        eyes.append(A.ell(HEAD, (0.262, s * 0.0425, 0.157), A.unit((1, s * 0.7, 0.1)), (0.0078, 0.0078, 0.007), k=0.0))
        shine.append(A.ell(HEAD, (0.266, s * 0.0476, 0.159), A.unit((1, s * 0.7, 0.1)), (0.0042, 0.0042, 0.0036), k=0.0))
        a = np.array([0.236, s * 0.024, 0.18])
        pts = A.bezier(a, a + np.array([-0.014, s * 0.018, 0.042]), a + np.array([-0.066, s * 0.044, 0.04]), 6)
        rad = [0.0098, 0.009, 0.008, 0.0068, 0.0056, 0.0045, 0.0035]
        horns += A.chain(lambda i: HEAD, pts, rad, k=0.002)
        for i in range(1, 6):
            horns.append(A.ell(HEAD, pts[i], A.unit(pts[i + 1] - pts[i - 1]), (0.0016, rad[i] * 1.12, rad[i] * 1.12), k=0.0008))
        e = pts[-1]
        dd = A.unit(pts[-1] - pts[-2])
        tips += A.chain(lambda i: HEAD, [e - dd * 0.002, e + dd * 0.008 - Z * 0.002, e + dd * 0.013 - Z * 0.006], [0.0032, 0.002, 0.0004], k=0.0)

    def horn_disp(Q):
        return -0.0004 * sdf.ridged(Q, 400, 2, seed=2417)

    whisk = []
    for s in (1, -1):
        for k in range(3):
            p0 = np.array([0.338, s * 0.031, 0.122 - 0.008 * k])
            d = A.unit(np.array([0.25, s * 1.0, 0.15 - 0.15 * k]))
            Lw = 0.03 + 0.02 * rng.random()
            whisk += A.chain(lambda i: HEAD, [p0, p0 + d * Lw * 0.6, p0 + d * Lw - Z * 0.006], [0.0011, 0.0008, 0.0003], k=0.0)
    # --- Sprachgeraet links am Hals: Kasten, Lautsprechergitter, rote Leuchte, Kabelbinder, Kabel ---------------------------
    dev_c = np.array([0.158, 0.084, 0.118])
    dev_n = A.unit(np.array([0.15, 1.0, 0.05]))  # Vorderseite nach aussen
    box = [A.box(NECK, dev_c, dev_n, (0.008, 0.026, 0.019), 0.003, up=Z, k=0.0)]
    grille = [A.ell(NECK, dev_c + dev_n * 0.0075 + X * 0.006, dev_n, (0.0022, 0.0135, 0.0135), up=Z, k=0.0)]
    for i in range(-3, 4):  # Schlitze im Gitter
        grille.append(A.box(NECK, dev_c + dev_n * 0.0095 + X * 0.006 + Z * (i * 0.0034), dev_n, (0.0016, 0.0115, 0.0008), 0.0003, up=Z, op='sub', k=0.0))
    led = [A.ell(NECK, dev_c + dev_n * 0.0085 - X * 0.017 + Z * 0.012, dev_n, (0.0024, 0.0024, 0.0024), k=0.0)]
    knob = [A.cone(NECK, dev_c + dev_n * 0.008 - X * 0.017 - Z * 0.008, dev_c + dev_n * 0.013 - X * 0.017 - Z * 0.008, 0.003, 0.003, k=0.0)]
    straps = []
    for dx in (0.012, -0.012):  # zwei Kabelbinder um den Hals
        ring = []
        for i in range(21):
            t = i / 20 * 2 * math.pi
            ring.append(np.array([0.158 + dx, 0.0735 * math.sin(t), 0.138 + 0.079 * math.cos(t)]))
        straps += A.chain(lambda i: NECK, ring, [0.0024] * 21, k=0.0)
        straps.append(A.box(NECK, np.array([0.158 + dx, -0.072, 0.13]), Y, (0.004, 0.003, 0.004), 0.001, k=0.0))  # Verschlusskopf
    cable = A.chain(lambda i: NECK, A.bezier(dev_c - X * 0.026 + Z * 0.004, np.array([0.11, 0.1, 0.16]), np.array([0.07, 0.07, 0.2]), 7),
                    [0.0019] * 8, k=0.0)

    return [
        {'name': 'fell', 'fm': 'ent_ratfur', 'parts': fur_parts, 'disp': fur_disp, 'h': 0.0018, 'cav_scale': 0.008},
        {'name': 'grannen', 'fm': 'ent_ratfur', 'parts': tufts, 'disp': None, 'h': 0.0012, 'cav_scale': 0.004, 'lod_weight': 0.7},
        {'name': 'beinfell', 'fm': 'ent_ratfur', 'parts': legs_fur, 'disp': fur_disp, 'h': 0.0009, 'cav_scale': 0.004},
        {'name': 'fleisch', 'fm': 'ent_ooze', 'parts': flesh, 'disp': flesh_disp, 'h': 0.0012, 'cav_scale': 0.004, 'lod_weight': 0.8},
        {'name': 'nase', 'fm': 'ent_darkskin', 'parts': skin + nostr, 'disp': hand_disp, 'h': 0.0006, 'cav_scale': 0.003},
        {'name': 'haut', 'fm': 'ent_dullerskin', 'parts': ears, 'disp': hand_disp, 'h': 0.0007, 'cav_scale': 0.003},
        {'name': 'lippen', 'fm': 'ent_mouth', 'parts': lips + gums, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003},
        {'name': 'haende', 'fm': 'ent_dullerskin', 'parts': hands, 'disp': hand_disp, 'h': 0.0006, 'cav_scale': 0.003},
        {'name': 'naegel', 'fm': 'ent_solifuge_dark', 'parts': nails, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002},
        {'name': 'zahnschmelz', 'fm': 'ent_resin', 'parts': enamel, 'disp': tooth_disp, 'h': 0.0005, 'cav_scale': 0.002, 'lod_weight': 2.0},
        {'name': 'schliff', 'fm': 'ent_tooth', 'parts': dentin, 'disp': None, 'h': 0.0004, 'cav_scale': 0.002, 'lod_weight': 2.0},
        {'name': 'augen', 'fm': 'ent_motheye', 'parts': eyes, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002, 'lod_weight': 2.0},
        {'name': 'glanz', 'fm': 'ent_eyeshine', 'parts': shine, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001},
        {'name': 'hoerner', 'fm': 'ent_horn', 'parts': horns, 'disp': horn_disp, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 1.5},
        {'name': 'spitzen', 'fm': 'ent_hornglow', 'parts': tips, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
        {'name': 'schnurrhaare', 'fm': 'ent_hair', 'parts': whisk, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001},
        {'name': 'geraet', 'fm': 'ent_ob_steel', 'parts': box + knob, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
        {'name': 'gitter', 'fm': 'ent_pg_brass', 'parts': grille, 'disp': None, 'h': 0.0004, 'cav_scale': 0.0015},
        {'name': 'leuchte', 'fm': 'ent_hornglow', 'parts': led, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001},
        {'name': 'binder', 'fm': 'ent_ob_rubber', 'parts': straps + cable, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
    ]
