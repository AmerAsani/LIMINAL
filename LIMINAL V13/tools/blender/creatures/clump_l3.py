"""V13.3 Level 3 (KREATUR_Clump.md, Abschnitt "Level 3: die grosse, bewaffnete Form"): Clump in Level 3, bewaffnet.

Wikidot Level 3 (Rev. 18, Hyper-intelligent Entities/Clumps) und Entity 5 (Rev. 38): Biologie wie ein gewoehnlicher Clump,
aber viel groesser (gestreckt bis etwa 8 ft), die Muskeln wesentlich groesser und staerker, die Arme geordnet und koordiniert
statt chaotisch; traegt oft mehrere Waffen gleichzeitig und fuehrt sie muehelos; ist er entwaffnet, verstuemmelt er sich und
legt blanken Knochen frei, um ihn als Waffe zu nutzen. Augen und Ohren ragen aus der Masse (Entity 5), in der Mitte das Maul
mit rasiermesserscharfen Zaehnen.

Eigener Entwurf (kein vergroessertes Buendel): die Masse ist ein Knoten aus Muskeln - breite Muskelstraenge laufen vom
Scheitel zu den Schultern, die toten Glieder liegen geordnet in Baendern um den Kern (geflochten, nicht geknaeult). Sieben
lebende Arme, prall und sehnig. Drei davon sind Waffen:
  Arm 1  die Hand ist abgerissen; aus der zerfetzten Manschette ragen Speiche und Elle als gesplitterter Knochenspiess,
         dazwischen haengen Sehnen (frueher schon entwaffnet - Auslegung der Seite).
  Arm 5  der Unterarm ist laengs aufgerissen, die Elle herausgebrochen und steht als gekruemmte Knochensichel ab; die Hand
         lebt noch und krallt.
  Arm 3  haelt ein rostiges Rohr mit drahtumwickeltem Griff (bewaffnet, Variante 1). Variante 2 (clump_l3_bare.py): das Rohr
         ist zerbrochen, die Hand abgerissen - frischer Knochenspiess, rohes Fleisch, Hautlappen.
Fuenf Augen und zwei Ohren in der Masse, ein breiteres Maul mit drei Zahnreihen. Das Spiel vergroessert das Wesen um 1,5
(creature_clump_l3.cpp, kBig) - hier in Einheiten des Buendelskeletts gebaut (Knochen und Gelenke wie clump.py).
"""
import math

import numpy as np

import anat as A
import clump as CL
import sdf

LODS = (90000, 24000, 7000)
SKELETON = 'ent_clump'
CORE = 1
C = CL.C
Z = np.array([0.0, 0.0, 1.0])
X = np.array([1.0, 0.0, 0.0])
TONES = CL.TONES


def arm_joints(J, a):
    return (np.asarray(J[4 + 4 * a + i]) for i in range(4))


def arm_bones(a):
    return (2 + 3 * a, 3 + 3 * a, 4 + 3 * a)


def bone_spike(W, df, side, bone, length, rng, fresh):
    """Speiche und Elle ragen aus dem Stumpf: zwei gesplitterte Knochen, Manschette aus zerfetztem Fleisch, Sehnen."""
    bone_p, raw, skin = [], [], []
    up = A.perp(df)
    lat = A.unit(np.cross(df, up))
    for k, (o, L, r) in enumerate(((0.012, length, 0.0125), (-0.011, length * 0.78, 0.0105))):
        b0 = W - df * 0.04 + lat * o
        tip = W + df * L + lat * (o * 0.4) + up * rng.normal(0, 0.008)
        mid = A.lerp(b0, tip, 0.55) + up * 0.004
        bone_p += A.chain(lambda q: bone, [b0, mid, tip], [r, r * 0.88, 0.0016], k=0.004)
        bone_p.append(A.ell(bone, b0 + df * 0.015, df, (0.018, r * 1.25, r * 1.15), up=up, k=0.004))  # Gelenkkopf
        for _ in range(3):  # Splitter an der Spitze
            s0 = A.lerp(mid, tip, rng.uniform(0.3, 0.8))
            sd = A.unit(df + lat * rng.normal(0, 0.5) + up * rng.normal(0, 0.5))
            bone_p.append(A.cone(bone, s0, s0 + sd * rng.uniform(0.02, 0.045), 0.0042, 0.0007, k=0.002))
        # Bruchflaechen: schraeg abgeschnitten
        bone_p.append(A.box(bone, tip - df * 0.005 + up * 0.006, A.unit(df + up * 0.9), (0.02, 0.02, 0.006), 0.001, op='sub', k=0.002))
    # Manschette: Fleischfetzen rund um den Stumpf, nach aussen umgeschlagen
    for i in range(11):
        ang = 2 * math.pi * i / 11 + rng.normal(0, 0.2)
        rd = A.unit(up * math.cos(ang) + lat * math.sin(ang))
        c = W + rd * 0.03 + df * rng.uniform(-0.015, 0.015)
        raw.append(A.ell(bone, c, A.unit(rd * 0.6 + df * 0.8), (rng.uniform(0.016, 0.03), 0.012, 0.006), up=rd, k=0.006))
    skin.append(A.cone(bone - 1, W - df * 0.09, W + df * 0.005, 0.034, 0.039, k=0.02))  # Unterarm laeuft in den Stumpf
    # Sehnen haengen heraus (frisch: mehr, laenger)
    for i in range(5 if fresh else 3):
        a0 = W + up * rng.normal(0, 0.015) + lat * rng.normal(0, 0.015)
        pts = [a0]
        d = A.unit(df * 0.5 - Z * 0.8 + lat * rng.normal(0, 0.4))
        for s in range(3):
            pts.append(pts[-1] + d * rng.uniform(0.03, 0.06) * (1.4 if fresh else 1.0))
            d = A.unit(d - Z * 0.4)
        raw += A.chain(lambda q: bone, pts, [0.0035, 0.003, 0.0024, 0.0016], k=0.002)
    if fresh:  # ein Hautlappen haengt herab
        flap = W - Z * 0.035 + df * 0.02
        skin.append(A.ell(bone, flap, A.unit(df - Z * 1.5), (0.045, 0.028, 0.004), up=A.unit(lat + df * 0.2), k=0.008))
    return bone_p, raw, skin


def sickle(E, W, df, side, bone, rng):
    """Arm 5: der Unterarm laengs aufgerissen, die Elle herausgebrochen - eine gekruemmte Knochensichel quer zum Arm."""
    bone_p, raw, sub = [], [], []
    up = A.perp(df)
    lat = A.unit(np.cross(df, up)) * side
    lf = CL.norm(W - E)
    # der Riss: eine Furche laengs der Aussenseite (aus der Haut ausgeschnitten), darin rohes Fleisch
    for i in range(6):
        t = 0.2 + 0.11 * i
        c = A.lerp(E, W, t) + lat * 0.03
        sub.append(A.ell(bone, c + lat * 0.012, df, (0.05, 0.016, 0.022), up=lat, op='sub', k=0.008))
        raw.append(A.ell(bone, c - lat * 0.004, df, (0.05, 0.012, 0.018), up=lat, k=0.006))
    # Sichel: tritt am Ellbogen aus, laeuft in einem Bogen ueber die Hand hinaus
    pts, rad = [], []
    for i in range(9):
        t = i / 8
        q = A.lerp(E, W, 0.25 + 0.95 * t) + lat * (0.035 + 0.13 * math.sin(t * math.pi * 0.9)) + up * (0.02 * t)
        pts.append(q)
        rad.append(0.011 * (1 - 0.82 * t ** 1.5) + 0.0012)
    bone_p += A.chain(lambda q: bone, pts, rad, k=0.004)
    # Schneide: eine flache Klinge innen am Bogen (abgeschabt), Kerben
    for i in range(1, 8):
        a, b = pts[i], pts[i + 1]
        n = A.unit(np.cross(b - a, up))
        bone_p.append(A.tube(bone, a - n * 0.006, b - n * 0.006, (0.0016, 0.007 * (1 - i / 9)), up=up, k=0.003))
        if rng.random() < 0.5:
            bone_p.append(A.ell(bone, A.lerp(a, b, 0.5) - n * 0.01, b - a, (0.004, 0.004, 0.006), up=up, op='sub', k=0.001))
    raw.append(A.ell(bone, pts[0] - lat * 0.004, df, (0.03, 0.02, 0.016), up=lat, k=0.006))  # Austrittswunde
    return bone_p, raw, sub


def pipe(W, hd, back, hs, bone, rng):
    """Rostiges Rohr quer durch die Faust, drahtumwickelter Griff, am langen Ende ein Winkelstueck."""
    side = A.unit(np.cross(back, hd)) * hs
    g = W + hd * 0.065 - back * 0.022
    ax = A.unit(side * 0.94 + hd * 0.34)
    steel, wire = [], []
    a, b = g - ax * 0.17, g + ax * 0.6
    steel.append(A.cone(bone, a, b, 0.0205, 0.0205, k=0.0))
    steel.append(A.cone(bone, a - ax * 0.002, a + ax * 0.03, 0.024, 0.024, k=0.003))       # Muffe
    el = b + ax * 0.03
    bend = A.unit(ax * 0.3 - back * 0.95)
    steel.append(A.ell(bone, el, ax, (0.034, 0.029, 0.029), k=0.006))                         # Winkelstueck
    steel.append(A.cone(bone, el, el + bend * 0.11, 0.022, 0.022, k=0.004))
    for k in range(3):  # Gewindegaenge
        steel.append(A.cone(bone, el + bend * (0.07 + 0.012 * k), el + bend * (0.076 + 0.012 * k), 0.0235, 0.0235, k=0.001))
    # Draht um den Griff, schraeg gewickelt
    pts = []
    for i in range(70):
        t = i / 69
        ang = t * 2 * math.pi * 9
        q = g + ax * (-0.11 + 0.24 * t)
        rv = A.perp(ax)
        rv2 = np.cross(ax, rv)
        pts.append(q + (rv * math.cos(ang) + rv2 * math.sin(ang)) * 0.0235)
    wire += A.chain(lambda q: bone, pts, [0.0022] * len(pts), k=0.0015)
    return steel, wire


def build_big(J, bones, bare):
    rng = np.random.default_rng(5301 if not bare else 5302)
    tone_parts = {t: [] for t in TONES}
    nails_by_hand = []
    core, teeth, throat, eyeballs, irises = [], [], [], [], []
    bone_parts, raw, steel, wire = [], [], [], []

    # --- Masse: Muskelknoten -------------------------------------------------------------------------------------
    core += [A.ell(CORE, C, (1, 0, 0), CL.CORE_R * np.array([1.05, 1.05, 1.0]), k=0.0),
             A.ell(CORE, (0.06, 0.0, 0.19), (1, 0, 0), (0.15, 0.15, 0.12), k=0.12)]
    # breite Muskelstraenge vom Scheitel zu den Schultern der lebenden Arme (geordnet, wie Speichen)
    top = C + Z * 0.2
    for a in range(7):
        S = np.asarray(J[4 + 4 * a])
        mid = A.lerp(top, S, 0.55) + A.unit(A.lerp(top, S, 0.55) - C) * 0.05
        core.append(A.tube(CORE, top, mid, (0.05, 0.03), (0.07, 0.04), up=A.unit(mid - C), k=0.05))
        core.append(A.tube(CORE, mid, S, (0.07, 0.04), (0.06, 0.05), up=A.unit(S - C), k=0.05))

    # --- lebende Arme --------------------------------------------------------------------------------------------
    tone_of_arm = (0, 1, 2, 0, 2, 1, 0)
    for a in range(7):
        S, E, W, T = arm_joints(J, a)
        bb = arm_bones(a)
        back_hint = bones[bb[2]]['frame'][:3, 2]
        hs = 1 if a % 2 == 0 else -1
        th = 1.18 if a == 0 else 1.55 + 0.1 * ((a * 37) % 3) / 2
        curl = None
        if a == 3 and not bare:
            curl = [(55, 80, 60), (58, 84, 62), (60, 86, 64), (62, 88, 66)]  # Faust um das Rohr
        if a == 5:
            curl = [(30, 60, 50), (34, 64, 52), (36, 66, 54), (40, 70, 56)]  # krallt
        skin, nails = CL.arm(S, E, W, T, bb, th, hs, back_hint, floor=0.004, gaunt=False, root=CORE, sc=1.42,
                             flen=1.3 if a == 0 else 1.05, curl=curl)
        df = A.unit(W - E)
        if a == 1 or (a == 3 and bare):  # Hand ab, Knochenspiess
            skin = [p for p in skin if p.bone != bb[2]]
            nails = []
            bp, rw, sk = bone_spike(W, df, hs, bb[2], 0.24 if a == 1 else 0.2, rng, fresh=a == 3)
            bone_parts += bp
            raw += rw
            skin += sk
        if a == 5:
            bp, rw, sb = sickle(E, W, df, hs, bb[1], rng)
            bone_parts += bp
            raw += rw
            skin += sb
        if a == 3 and not bare:
            hd = A.unit(T - W)
            back = CL.orth(back_hint, hd)
            st, wi = pipe(W, hd, back, hs, bb[2], rng)
            steel += st
            wire += wi
        # dicke Adern ueber den Unterarmen (Kraft)
        for k in range(3):
            p0 = A.lerp(E, W, 0.1) + A.perp(df) * rng.normal(0, 0.03)
            p1 = A.lerp(E, W, 0.5) + A.perp(df) * rng.normal(0, 0.03) + A.unit(np.cross(df, Z)) * rng.normal(0, 0.02)
            p2 = A.lerp(E, W, 0.85) + A.perp(df) * rng.normal(0, 0.02)
            skin += A.chain(lambda q, b=bb[1]: b, [p0, p1, p2], [0.0055, 0.005, 0.004], k=0.004)
        tone_parts[TONES[tone_of_arm[a]]].append((f'arm{a}', skin))
        if nails:
            nails_by_hand.append(nails)

    # Beine (schleifen nach, stark)
    for l in range(2):
        H, K, Fo = (np.asarray(J[32 + 3 * l + i]) for i in range(3))
        tone_parts['clump_b'].append((f'bein{l}', CL.leg(H, K, Fo, (23 + 2 * l, 24 + 2 * l), 1.35, 1 if l == 0 else -1, root=CORE)))

    # tote Glieder in Baendern um den Kern (geflochten): je Band drei Glieder mit gleichem Kurs
    DEAD = []
    for band, (el, head0) in enumerate(((0.95, 200), (0.45, 230), (0.05, 260))):
        for k in range(5):
            az = -2.8 + 1.25 * k + 0.45 * band
            kind = 'leg' if (k + band) % 4 == 0 else 'arm'
            DEAD.append((az, el, head0 + 6 * k, 0.32, 0.28, (85 if k % 2 else -85), 0.060 + 0.006 * (band == 1), (k + band) % 3, kind))
    for i, (az, el, hd_, lu, lf, turn, r, tone, kind) in enumerate(DEAD):
        if kind == 'leg':
            skin = CL.dead_leg(az, el, hd_, lu, lf, turn, r, 1 if i % 2 else -1)
            nails = []
        else:
            skin, nails = CL.dead_arm(az, el, hd_, lu, lf, turn, r, 1 if i % 2 else -1)
        tone_parts[TONES[tone]].append((f'tot{i}', skin))
        if nails:
            nails_by_hand.append(nails)

    # --- Augen (fuenf) und Ohren (zwei) auf der aeussersten Flaeche ------------------------------------------------
    solid = [p for p in core if p.op == 'add'] + [p for t in TONES for _, ps in tone_parts[t] for p in ps if p.op == 'add']

    def outer(u):
        ts = np.arange(0.85, 0.0, -0.002)
        d = sdf.sample(solid, C + np.outer(ts, u))
        return C + u * ts[int(np.argmax(d < 0))]

    for u, size, roll, shut in (((0.80, 0.42, 0.42), 1.15, 0.25, 0.2), ((0.70, -0.60, 0.30), 1.0, -0.7, 0.1),
                                ((0.25, 0.10, 0.96), 0.85, 1.1, 0.45), ((-0.35, -0.55, 0.75), 0.9, -0.3, 0.6),
                                ((0.55, 0.05, 0.80), 0.7, 0.4, 0.0)):
        u = A.unit(u)
        n = u
        core.append(A.ell(CORE, outer(u) - n * 0.004, CL.orth(Z, n), (0.044 * size, 0.037 * size, 0.024), up=n, k=0.02))
        p = outer(u) + n * 0.015
        e0 = CL.orth(Z, n)
        up = CL.rot(e0, n, roll)
        sd = np.cross(up, n)
        r = 0.0125 * size
        core.append(A.ell(CORE, p + n * 0.004, sd, (r * 1.9, r * 1.25, r * 1.1), up=n, op='sub', k=0.006))
        eyeballs.append([A.ell(CORE, p - n * r * 0.25, sd, (r, r, r), up=n)])
        irises.append([A.ell(CORE, p + n * r * 0.70, sd, (r * 0.46, r * 0.46, r * 0.12), up=n)])
        core.append(A.ell(CORE, p + n * r * 0.55 + up * r * (0.95 - shut * 0.9), sd, (r * 1.55, r * 0.75, r * 0.42), up=n, k=0.004))
        core.append(A.ell(CORE, p + n * r * 0.45 - up * r * 0.95, sd, (r * 1.45, r * 0.45, r * 0.32), up=n, k=0.004))
    for u, roll in (((-0.03, -0.95, 0.30), 0.5), ((-0.30, 0.90, 0.30), -0.4)):
        u = A.unit(u)
        n = u
        core.append(A.ell(CORE, outer(u) - n * 0.006, CL.orth(Z, n), (0.048, 0.037, 0.021), up=n, k=0.02))
        p = outer(u) + n * 0.010
        upe = CL.rot(CL.orth(Z, n), n, roll)
        fwe = np.cross(n, upe)
        c = p + n * 0.006
        Rf = np.stack([-fwe, upe, n], axis=1)
        core.append(A.Part('torus', CORE, k=0.006, c=c, R=Rf, major=0.024, minor=0.006, arc=2.3))
        core.append(A.ell(CORE, c - n * 0.002, upe, (0.028, 0.019, 0.006), up=n, k=0.008))
        core.append(A.ell(CORE, c + n * 0.005 + fwe * 0.004 - upe * 0.004, upe, (0.012, 0.010, 0.006), up=n, op='sub', k=0.004))
        core.append(A.ell(CORE, c - upe * 0.028 + fwe * 0.002, upe, (0.010, 0.009, 0.005), up=n, k=0.006))

    # --- Maul: breiter, drei Zahnreihen -----------------------------------------------------------------------------
    ys = np.linspace(-0.15, 0.15, 29)
    op_of = lambda y: 0.005 + 0.02 * (1 - (y / 0.15) ** 2)  # noqa: E731
    for i in range(len(ys) - 1):
        ya, yb = ys[i], ys[i + 1]
        pa, pb = np.array([0.075, ya, 0.07]), np.array([0.075, yb, 0.07])
        oa, ob = op_of(ya), op_of(yb)
        core.append(A.tube(CORE, pa, pb, (oa, 0.06), (ob, 0.06), op='sub', k=0.004))
        for sx in (1, -1):
            core.append(A.tube(CORE, pa + X * sx * (oa + 0.009) + Z * 0.006, pb + X * sx * (ob + 0.009) + Z * 0.006,
                               (0.012, 0.013), (0.012, 0.013), k=0.006))
    for sx in (1, -1):
        for row in (0, 1, 2):
            cnt = (26, 20, 14)[row]
            for i in range(cnt):
                y = -0.13 + 0.26 * (i + 0.5 + 0.3 * (rng.random() - 0.5)) / cnt
                o = op_of(y)
                base = np.array([0.075 + sx * (o + 0.001 + 0.006 * row), y, 0.082 + 0.015 * row])
                d = A.unit(np.array([-sx * 0.75, 0.15 * (rng.random() - 0.5), 0.65]))
                L = (0.024 + 0.016 * rng.random()) * (1.0, 0.8, 0.6)[row] * (1 - 0.5 * (y / 0.15) ** 2)
                if rng.random() < 0.1:
                    L *= 0.45
                teeth.append(A.cone(CORE, base, base + d * L, 0.0038, 0.0003, group='tooth'))
    throat.append(A.ell(CORE, (0.075, 0.0, 0.14), (1, 0, 0), (0.04, 0.14, 0.055)))

    # --- Verschiebungen ------------------------------------------------------------------------------------------
    def core_disp(Q):
        d = -0.0018 * (sdf.ridged(Q * np.array([1.0, 1.0, 0.6]), 24, 2, seed=172) - 0.5)
        d -= 0.005 * sdf.fbm(Q, 6, 2, seed=173)
        # Muskelfasern: feine Rillen entlang der Straenge (radial vom Scheitel)
        rq = Q - (C + Z * 0.2)
        ang = np.arctan2(rq[:, 1], rq[:, 0])
        d -= 0.0009 * np.cos(ang * 60 + 4 * sdf.fbm(Q, 12, 1, seed=176)) * np.clip((Q[:, 2] - 0.45) / 0.1, 0, 1)
        d -= 0.0004 * sdf.fbm(Q, 90, 2, seed=174)
        return d

    def limb_disp(Q):
        d = -0.0004 * sdf.fbm(Q, 90, 2, seed=181)
        d -= 0.0013 * np.clip((sdf.ridged(Q, 28, 2, seed=182) - 0.76) * 5, 0, 1)   # Adern
        d -= 0.0005 * (sdf.ridged(Q * np.array([1, 1, 0.5]), 70, 1, seed=183) - 0.5)  # Muskelfasern, Falten
        return d

    def bone_disp(Q):
        return -0.0003 * sdf.fbm(Q, 140, 2, seed=191) - 0.0002 * (sdf.ridged(Q * np.array([1, 1, 4]), 300, 1, seed=192) - 0.5)

    def raw_disp(Q):
        return -0.0012 * (sdf.ridged(Q, 110, 2, seed=193) - 0.5) - 0.0006 * sdf.fbm(Q, 60, 2, seed=194)

    def rust_disp(Q):
        return -0.0007 * np.clip((sdf.fbm(Q, 45, 3, seed=195) - 0.45) * 3, 0, 1) - 0.0002 * sdf.fbm(Q, 220, 1, seed=196)

    core_add = [p for p in core if p.op == 'add']
    limb_add = [p for t in TONES for _, ps in tone_parts[t] for p in ps if p.op == 'add']
    groups = [{'name': 'kern', 'fm': 'clump_core', 'parts': core, 'disp': core_disp, 'h': 0.0032, 'cav_scale': 0.016,
               'cav_extra': limb_add}]
    for t in TONES:
        for name, ps in tone_parts[t]:
            groups.append({'name': name, 'fm': t, 'parts': ps, 'disp': limb_disp, 'h': 0.0022, 'cav_scale': 0.010,
                           'cav_extra': core_add, 'merge': 'glieder_' + t[-1]})
    for i, ns in enumerate(nails_by_hand):
        groups.append({'name': f'naegel{i}', 'fm': 'ent_horn', 'parts': ns, 'disp': None, 'h': 0.0006, 'cav_scale': 0.003,
                       'merge': 'naegel'})
    # Knochen und rohes Fleisch: je Arm ein eigenes Gitter (verstreut), danach ein Netz
    def split(parts):
        out = {}
        for p in parts:
            out.setdefault(p.bone, []).append(p)
        return out
    for b, ps in split(bone_parts).items():
        groups.append({'name': f'knochen{b}', 'fm': 'ent_cl_bone', 'parts': ps, 'disp': bone_disp, 'h': 0.0011, 'cav_scale': 0.004,
                       'merge': 'knochen'})
    for b, ps in split(raw).items():
        groups.append({'name': f'fleisch{b}', 'fm': 'ent_cl_raw', 'parts': ps, 'disp': raw_disp, 'h': 0.0013, 'cav_scale': 0.005,
                       'merge': 'fleisch', 'cav_extra': limb_add})
    if steel:
        groups.append({'name': 'rohr', 'fm': 'ent_ob_rust', 'parts': steel, 'disp': rust_disp, 'h': 0.0016, 'cav_scale': 0.006})
        groups.append({'name': 'draht', 'fm': 'ent_wire', 'parts': wire, 'disp': None, 'h': 0.0008, 'cav_scale': 0.003})
    groups += [
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003},
        {'name': 'schlund', 'fm': 'ent_mouth', 'parts': throat, 'disp': None, 'h': 0.0030, 'cav_scale': 0.01},
    ]
    for i, (eb, ir) in enumerate(zip(eyeballs, irises)):
        groups.append({'name': f'auge{i}', 'fm': 'ent_tooth', 'parts': eb, 'disp': None, 'h': 0.0008, 'cav_scale': 0.004,
                       'merge': 'augen'})
        groups.append({'name': f'iris{i}', 'fm': 'ent_darkskin', 'parts': ir, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002,
                       'merge': 'iris'})
    return groups


def build(J, bones):
    return build_big(J, bones, bare=False)
