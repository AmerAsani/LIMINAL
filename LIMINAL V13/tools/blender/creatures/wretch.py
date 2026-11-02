"""V13.2: Wretch (Entity 15, Wikidot Rev. 54) - KREATUR_Wretch.md. Ein Mensch im dritten Stadium des "Wretched Cycle":
hager bis auf die Knochen, rotbraune, trockene, schuppige Haut voller Loecher und Pusteln, aus denen roetlich-brauner Saft
sickert; Haar ausgefallen; Lider, Lippen und Knorpel aufgeloest - die Augen bleiben offen, die Zaehne liegen frei (zu viele,
schief). Aus dem Mund laeuft brauner Schlamm. Reste der Kleidung haengen zerrissen am Koerper.
"""
import math

import numpy as np

import anat as A
import human as Hm
import sdf

LODS = (24000, 8000, 2600)


def build(J, bones, fist=False):
    """fist (V13.3, wretch_l3.py): die rechte Hand geschlossen um einen Griff - sonst unveraendert."""
    rng = np.random.default_rng(15)
    B = Hm.Body(J, slim=0.78, muscle=0.6, belly=0.72, chest=0.92, hips=0.85, hand=1.05)
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    c, r = B.headC, B.headR
    skin = B.head(featureless=False) + B.neck() + B.torso() + B.leg(0) + B.leg(1) + B.foot(0, bare=True) + B.foot(1, bare=True)
    for s in (0, 1):
        skin += B.arm(s, hand=False) + B.hand(s, curl=1.15 if fist and s == 1 else 0.6, claw=0.05 if fist and s == 1 else 0.15)
    # ausgezehrt: Rippen, Becken- und Schluesselbeine, Wangen eingefallen, Kniegelenke knotig
    # Rippen: duenne Grate knapp unter der Haut, auf der Brustkorb-Ellipse (sie woelben die Haut nur leicht)
    rx, ry, rz = 0.105 * 0.78 * 0.92, 0.150 * 0.78, 0.175
    for k in range(6):
        z = B.chestZ + 0.04 - 0.04 * k
        f = np.sqrt(max(0.05, 1.0 - ((z - B.chestZ) / rz) ** 2))
        for s in (0, 1):
            g = Hm.side(s)
            pts = [np.array([rx * f * np.cos(t) * 0.985, g * ry * f * np.sin(t) * 0.985, z - 0.022 * t]) for t in np.linspace(0.25, 2.0, 6)]
            skin += A.chain(lambda i: Hm.CHEST, pts, [0.0055, 0.0065, 0.0065, 0.006, 0.0055, 0.005], k=0.012)
    skin.append(A.ell(Hm.SPINE, (0.06, 0, B.waist + 0.02), X, (0.04, 0.09, 0.06), op='sub', k=0.04))  # eingefallener Bauch
    for s in (0, 1):
        g = Hm.side(s)
        skin.append(A.ell(Hm.HEAD, c + X * r[0] * 0.62 + Y * g * r[1] * 0.55 - Z * r[2] * 0.42, X, (0.03, 0.022, 0.03), op='sub', k=0.02))  # Wangen
        skin.append(A.ell(Hm.THIGH + 3 * s, B.j(Hm.jKNEE, s) + X * 0.01, Z, (0.04, 0.045, 0.045), k=0.02))  # Knie
        skin.append(A.ell(Hm.PELVIS, (0.04, g * 0.13, B.hip + 0.05), X, (0.025, 0.02, 0.02), k=0.02))  # Darmbein
    # Augen ohne Lider: grosse Hoehlen, Augaepfel gross und vorstehend (rollen beim Bewegen - der Kopf zuckt)
    eyes = []
    for s in (0, 1):
        g = Hm.side(s)
        e = c + X * r[0] * 0.80 + Y * g * r[1] * 0.40 + Z * r[2] * 0.04
        skin.append(A.ell(Hm.HEAD, e + X * 0.016, X, (0.03, 0.026, 0.022), op='sub', k=0.008))
        eyes.append(A.ell(Hm.HEAD, e + X * 0.004, X, (0.017, 0.018, 0.017), k=0.0))
    # Mund ohne Lippen: Zaehne frei, mehr als ein Mensch hat
    mouthC = c + X * r[0] * 0.84 - Z * r[2] * 0.46
    skin.append(A.ell(Hm.JAW, mouthC + X * 0.018, X, (0.026, 0.045, 0.024), op='sub', k=0.006))
    teeth = []
    for row, dz, dirz in ((0, 0.012, -1.0), (1, -0.012, 1.0)):
        pts = [mouthC + X * (0.008 - 0.0004 * k * k) + Y * (k - 4.5) * 0.0085 + Z * dz for k in range(10)]
        teeth += A.teeth_row(Hm.HEAD if row == 0 else Hm.JAW, pts, 0.011, 0.0045, [Z * dirz] * 10, rng, jitter=0.5)
    gums = [A.ell(Hm.JAW, mouthC, X, (0.022, 0.04, 0.02), k=0.0)]
    # Zaehne, die anderswo nachgewachsen sind (die Seite: an ungewoehnlichen Stellen, in ungewoehnlicher Zahl und Groesse):
    # durch die linke Wange und unter dem Kinn
    cheek = c + X * r[0] * 0.66 + Y * r[1] * 0.62 - Z * r[2] * 0.36
    teeth += A.teeth_row(Hm.HEAD, [cheek + Z * (k - 1) * 0.011 + X * 0.004 * k for k in range(3)], 0.016, 0.005,
                         [A.unit(Y + X * 0.6)] * 3, rng, jitter=0.5)
    chin = c + X * r[0] * 0.5 - Z * r[2] * 0.98
    teeth += A.teeth_row(Hm.JAW, [chin + Y * (k - 0.5) * 0.014 for k in range(2)], 0.013, 0.0045, [-Z + X * 0.3] * 2, rng, jitter=0.5)
    # Pusteln (Erhebungen) und Loecher (Mulden) ueber den Koerper verteilt - in der Verschiebung; dazu Saft, der herabrinnt
    ooze = []
    for k in range(22):
        z = 0.35 + rng.random() * (B.shZ - 0.35)
        a = rng.random() * 2 * math.pi
        ry = 0.13 if z > B.hip else 0.07
        p = np.array([math.cos(a) * 0.09, math.sin(a) * ry + (0 if z > B.hip else (0.09 if rng.random() < 0.5 else -0.09)), z])
        bone = Hm.CHEST if z > B.chestZ - 0.1 else (Hm.SPINE if z > B.hip else (Hm.THIGH if p[1] > 0 else Hm.THIGH + 3))
        ln = 0.03 + 0.08 * rng.random()
        ooze += A.chain(lambda i, b=bone: b, [p, p - Z * ln * 0.5 + X * 0.004, p - Z * ln], [0.004, 0.0035, 0.002], k=0.003)
        ooze.append(A.ell(bone, p - Z * ln, Z, (0.004, 0.004, 0.006), k=0.002))
    sludge = A.chain(lambda i: Hm.JAW, [mouthC + X * 0.01 - Z * 0.02, mouthC + X * 0.03 - Z * 0.06, mouthC + X * 0.03 - Z * 0.12],
                     [0.009, 0.007, 0.004], k=0.006)
    ooze += sludge

    def skin_disp(Q):
        d1, d2 = sdf.worley(Q, 26, seed=1501)
        pust = np.clip(1.0 - d1 / 0.18, 0, 1) ** 2                          # Pusteln (aufgewoelbt)
        e1, e2 = sdf.worley(Q, 18, seed=1502)
        hole = np.clip(1.0 - e1 / 0.12, 0, 1) ** 2                          # Loecher
        flake = sdf.ridged(Q, 70, 2, seed=1503)                             # Schuppen und Risse
        return 0.003 * pust - 0.006 * hole - 0.0012 * (flake - 0.5) - 0.002 * sdf.fbm(Q, 12, 2, seed=1504)

    # Reste der Kleidung: zerrissene Hose (nur Fetzen), Hemdreste an Schultern und Bauch
    rags = []
    for s in (0, 1):
        rags += B.leg(s, t=0.014, to=0.45)
    rags += B.torso(t=0.016, upper=False, lower=True)[1:2]
    rags.append(A.box(Hm.PELVIS, np.array([0, 0, 0.25]), X, (0.5, 0.5, 0.12), 0.03, op='sub', k=0.03))
    for k in range(12):  # Loecher in den Fetzen
        z = 0.35 + 0.55 * rng.random()
        p = np.array([(rng.random() - 0.5) * 0.2, (rng.random() - 0.5) * 0.3, z])
        rags.append(A.ell(Hm.PELVIS if z > 0.7 else (Hm.THIGH if p[1] > 0 else Hm.THIGH + 3), p, X, (0.06, 0.04, 0.05), op='sub', k=0.01))
    shirt = B.torso(t=0.014, upper=True, lower=False)[:2]
    shirt.append(A.box(Hm.CHEST, np.array([0.1, 0, B.chestZ - 0.05]), X, (0.12, 0.3, 0.16), 0.04, op='sub', k=0.03))  # vorn weggerissen
    for k in range(6):
        p = np.array([-0.1, (rng.random() - 0.5) * 0.3, B.chestZ - 0.1 + 0.2 * rng.random()])
        shirt.append(A.ell(Hm.CHEST, p, X, (0.04, 0.05, 0.04), op='sub', k=0.01))

    def rag_disp(Q):
        return -0.004 * sdf.fbm(Q * np.array([2.0, 2.0, 1.0]), 2.0, 3, seed=1505) - 0.001 * sdf.fbm(Q, 40, 2, seed=1506)

    return [
        {'name': 'haut', 'fm': 'ent_wretchskin', 'parts': skin, 'disp': skin_disp, 'h': 0.004, 'cav_scale': 0.014},
        {'name': 'augen', 'fm': 'ent_tooth', 'parts': eyes, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003, 'lod_weight': 2.0},
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.0011, 'cav_scale': 0.003, 'lod_weight': 1.6, 'merge': 'zaehne'},
        {'name': 'gaumen', 'fm': 'ent_mouth', 'parts': gums, 'disp': None, 'h': 0.002, 'cav_scale': 0.004},
        {'name': 'saft', 'fm': 'ent_ooze', 'parts': ooze, 'disp': None, 'h': 0.0012, 'cav_scale': 0.003},
        {'name': 'hose', 'fm': 'ent_cloth_b', 'parts': rags, 'disp': rag_disp, 'h': 0.0045, 'cav_scale': 0.01},
        {'name': 'hemd', 'fm': 'ent_cloth_a', 'parts': shirt, 'disp': rag_disp, 'h': 0.0045, 'cav_scale': 0.01},
    ]
