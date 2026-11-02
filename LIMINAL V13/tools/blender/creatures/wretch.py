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
    gums = [A.ell(Hm.JAW, mouthC - X * 0.01, X, (0.016, 0.036, 0.016), k=0.0)]  # Runde 3: zurueck (lag wie eine graue Lippe vor dem Mund)
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
    # V13.3 Wow-Check Runde 3: der Schlamm aus dem Mund duenner, kuerzer und unregelmaessig (las sich wie ein Stab)
    sludge = A.chain(lambda i: Hm.JAW, [mouthC + X * 0.012 - Z * 0.015, mouthC + X * 0.024 - Z * 0.035 + Y * 0.004, mouthC + X * 0.026 - Z * 0.06 - Y * 0.003,
                                        mouthC + X * 0.024 - Z * 0.075], [0.006, 0.0045, 0.0035, 0.002], k=0.004)
    sludge.append(A.ell(Hm.JAW, mouthC + X * 0.024 - Z * 0.079, Z, (0.003, 0.003, 0.0045)))
    ooze += sludge
    # V13.3 Wow-Check (KREATUR_Wretch.md, Abschnitt "Wow-Check Level 3"): im vollen Lampenlicht war das Gesicht zu sauber.
    # Jetzt am Kopf: Schorfplatten (Kruste), gelbe, nass glaenzende Pusteln, aufgeplatzte Pusteln mit Krater, Loecher mit
    # rohem Grund, aus denen Saft laeuft, rohe, entzuendete Raender um die lidlosen Augen und den lippenlosen Mund
    hr = np.random.default_rng(1515)
    crust, pus, raw = [], [], []
    sk = c + Z * r[2] * 0.12
    skr = np.array([r[0], r[1], r[2] * 0.86])

    def head_pt(th, ph, out=0.0):
        n = np.array([math.cos(ph) * math.cos(th), math.cos(ph) * math.sin(th), math.sin(ph)])
        return sk + n * (skr + out), A.unit(n / skr)

    for k in range(16):  # Schorf auf Schaedel, Stirn, Schlaefen
        th, ph = (hr.random() - 0.5) * 3.6 + (0.0 if hr.random() < 0.7 else math.pi), 0.1 + hr.random() * 1.2
        p, nrm = head_pt(th, ph)
        rr = (0.0018, 0.008 + 0.012 * hr.random(), 0.007 + 0.01 * hr.random())
        crust.append(A.ell(Hm.HEAD, p + nrm * 0.0006, nrm, rr, up=A.unit(np.cross(nrm, Z) + X * 0.01), k=0.0))
    for k in range(14):  # Pusteln (gelb, prall, nass)
        th, ph = (hr.random() - 0.5) * 2.6, -0.35 + hr.random() * 1.2
        p, nrm = head_pt(th, ph)
        rad = 0.0028 + 0.003 * hr.random()
        pus.append(A.ell(Hm.HEAD, p + nrm * rad * 0.15, nrm, (rad * 0.85, rad, rad), k=0.0))
        raw.append(A.ell(Hm.HEAD, p - nrm * 0.0005, nrm, (0.0012, rad * 1.7, rad * 1.7), k=0.0))  # entzuendeter Hof
    for k in range(5):  # aufgeplatzt: Krater mit Krustenrand
        th, ph = (hr.random() - 0.5) * 2.2, -0.2 + hr.random() * 0.9
        p, nrm = head_pt(th, ph)
        skin.append(A.ell(Hm.HEAD, p + nrm * 0.002, nrm, (0.004, 0.006, 0.006), op='sub', k=0.002))
        crust.append(A.ell(Hm.HEAD, p + nrm * 0.0005, nrm, (0.0016, 0.0085, 0.0085), k=0.0))
        raw.append(A.ell(Hm.HEAD, p - nrm * 0.002, nrm, (0.0015, 0.005, 0.005), k=0.0))
    holes = [head_pt(0.75, -0.25), head_pt(-0.55, 0.45), head_pt(-1.1, -0.05), head_pt(0.25, 0.75)]
    fc = c + X * r[0] * 0.42 - Z * r[2] * 0.30
    holes.append((fc + np.array([r[0] * 0.5, -r[1] * 0.62, -r[2] * 0.2]), A.unit(np.array([0.6, -1.0, -0.2]))))  # Wange
    for p, nrm in holes:  # Loecher: tief, roher Grund, Saft laeuft heraus
        skin.append(A.ell(Hm.HEAD, p + nrm * 0.004, nrm, (0.009, 0.007, 0.006), op='sub', k=0.002))
        raw.append(A.ell(Hm.HEAD, p - nrm * 0.004, nrm, (0.002, 0.0065, 0.0055), k=0.0))
        a0 = p + nrm * 0.001 - Z * 0.005
        ln = 0.02 + 0.03 * hr.random()
        ooze += A.chain(lambda i: Hm.HEAD, [a0, a0 - Z * ln * 0.5 + nrm * 0.001, a0 - Z * ln], [0.0026, 0.0022, 0.0015], k=0.002)
    for s in (0, 1):  # rohe Raender um die Augen
        g = Hm.side(s)
        e = c + X * r[0] * 0.80 + Y * g * r[1] * 0.40 + Z * r[2] * 0.04
        ring = [e + X * 0.012 + (Y * math.cos(t) * 0.024 + Z * math.sin(t) * 0.02) for t in np.linspace(0, 2 * math.pi, 11)]
        raw += A.chain(lambda i: Hm.HEAD, ring[:7], [0.002] * 7, k=0.0015)  # Runde 2: duenner, nur unten (las sich wie eine Brille)
    for k in range(7):  # Runde 2: Krustenbroeckchen statt eines roten Lippenrings (las sich wie Lippenstift)
        q = mouthC + X * 0.022 + Y * (k - 3.0) * 0.012 + Z * (0.017 + 0.004 * hr.random())
        crust.append(A.ell(Hm.HEAD, q, X, (0.0025, 0.004 + 0.003 * hr.random(), 0.003), k=0.0))

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
        {'name': 'schorf', 'fm': 'ent_wre_crust', 'parts': crust, 'disp': None, 'h': 0.0011, 'cav_scale': 0.002},
        {'name': 'pusteln', 'fm': 'ent_wre_pus', 'parts': pus, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002, 'lod_weight': 1.4},
        {'name': 'roh', 'fm': 'ent_cl_raw', 'parts': raw, 'disp': None, 'h': 0.001, 'cav_scale': 0.002},
        {'name': 'hose', 'fm': 'ent_cloth_b', 'parts': rags, 'disp': rag_disp, 'h': 0.0045, 'cav_scale': 0.01},
        {'name': 'hemd', 'fm': 'ent_cloth_a', 'parts': shirt, 'disp': rag_disp, 'h': 0.0045, 'cav_scale': 0.01},
    ]
