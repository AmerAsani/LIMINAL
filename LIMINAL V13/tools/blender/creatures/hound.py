"""V13: Hound (Entity 8) - KREATUR_Hound.md. Ein abgemagerter Mensch auf allen vieren: Rumpf mit Rippen, Wirbeln und
Schulterblaettern unter straffer Haut, sehnige lange Arme mit flachen Haenden und langen Fingern samt Krallen,
Beine in der Hocke auf den Ballen (Ferse hoch), schmaler Schaedel mit weit nach hinten reichendem Maul, schwere,
nasse Haarstraehnen ueber dem Gesicht und den Nacken hinab.

Knochen (skeletons.json, ent_hound): 1 Becken, 2/3 Wirbel, 4-6 Hals, 7 Kopf, 8 Kiefer; vorn je Seite Schulterblatt
9, Oberarm 10, Unterarm 11, Hand 12, Finger 13-17 (rechts +9); hinten Oberschenkel 27, Schienbein 28, Mittelfuss 29,
Zehen 30-32 (rechts +6); Haar Kopf 39-47 (3 Straehnen x 3), Ruecken 48-59 (6 x 2).
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (56000, 16000, 4500)


def build(J, bones):
    rng = np.random.default_rng(8)
    P = lambda i: np.asarray(J[i], dtype=np.float64)  # noqa: E731
    skin, hair, teeth, mouth, eyes = [], [], [], [], []
    claws = [[], [], [], []]

    # --- Rumpf ---------------------------------------------------------------------------------------------
    pelvis, sp1, sp2, chest, n1, n2, skull, snout = (P(i) for i in (1, 2, 3, 4, 5, 6, 7, 8))
    skin += [
        A.ell(3, (0.125, 0, 0.752), (1, 0, 0.05), (0.205, 0.124, 0.104), k=0.0),          # Brustkorb
        A.ell(3, (0.262, 0, 0.735), (1, 0, -0.3), (0.075, 0.098, 0.082), k=0.04),        # obere Brust
        A.ell(2, (-0.150, 0, 0.758), (1, 0, -0.1), (0.190, 0.092, 0.070), k=0.05),       # Taille (eingefallen)
        A.ell(1, (-0.400, 0, 0.712), (1, 0, -0.15), (0.105, 0.122, 0.086), k=0.045),     # Becken
        A.ell(1, (-0.470, 0, 0.776), (1, 0, -0.4), (0.050, 0.034, 0.020), k=0.02),       # Kreuzbein
    ]
    for s in (1, -1):
        skin += [
            A.ell(1, (-0.330, 0.104 * s, 0.762), (1, 0, 0.3), (0.036, 0.019, 0.017), k=0.012),     # Beckenkamm
            A.ell(1, (-0.462, 0.060 * s, 0.705), (1, 0, -0.5), (0.050, 0.046, 0.056), k=0.045),    # Gesaess (mager)
            A.ell(9 if s > 0 else 18, (0.150, 0.088 * s, 0.842), (1, 0.45 * s, 0.05), (0.078, 0.046, 0.012),
                  up=(0, 0.45 * s, 1), k=0.035),                                                   # Schulterblatt
            A.ell(9 if s > 0 else 18, (0.175, 0.090 * s, 0.857), (1, 0.6 * s, 0.1), (0.055, 0.006, 0.006), k=0.008),
            A.ell(3, (0.205, 0.062 * s, 0.700), (1, 0.4 * s, -0.4), (0.055, 0.035, 0.018), k=0.025),   # Brustmuskel
        ]
    # Dornfortsaetze entlang des Rueckens: auf der gemessenen Rueckenlinie des Rumpfs, fast ganz eingesunken
    torso = list(skin)
    zs_probe = np.arange(0.95, 0.60, -0.002)
    for x in np.arange(-0.46, 0.30, 0.031):
        Q = np.stack([np.full(len(zs_probe), x), np.zeros(len(zs_probe)), zs_probe], axis=1)
        dd = sdf.sample(torso, Q)
        inside = np.nonzero(dd < 0)[0]
        if not len(inside):
            continue
        ztop = zs_probe[inside[0]]
        b = 3 if x > 0.04 else (2 if x > -0.18 else 1)
        r = 0.0085 + 0.0015 * math.sin(x * 61.0)
        skin.append(A.ell(b, (x, 0, ztop - r + 0.004), (1, 0, 0), (r, r * 0.75, r), k=0.012))
    # --- Hals und Kopf ---------------------------------------------------------------------------------------
    skin += [
        A.cone(4, (0.255, 0, 0.792), n1, 0.064, 0.050, k=0.035),
        A.cone(5, n1, n2, 0.050, 0.043, k=0.02),
        A.cone(6, n2, (0.535, 0, 0.640), 0.043, 0.044, k=0.02),
    ]
    for s in (1, -1):
        skin.append(A.cone(5, (0.555, 0.040 * s, 0.618), (0.300, 0.020 * s, 0.712), 0.011, 0.009, k=0.015))  # Kopfwender
    skin += [
        A.ell(7, (0.552, 0, 0.668), (1, 0, -0.2), (0.086, 0.069, 0.074), k=0.02),            # Schaedel
        A.cone(7, (0.575, 0, 0.628), (0.668, 0, 0.604), 0.054, 0.036, k=0.022),           # Oberkiefer
        A.ell(7, (0.612, 0, 0.676), (0.3, 0, -1), (0.013, 0.054, 0.011), up=(1, 0, 0), k=0.012),  # Brauen
        A.ell(8, (0.560, 0, 0.566), (1, 0, -0.3), (0.034, 0.058, 0.024), k=0.02),            # Kiefergelenk, breit
        A.cone(8, (0.565, 0, 0.560), (0.660, 0, 0.533), 0.044, 0.027, k=0.015),           # Unterkiefer
    ]
    for s in (1, -1):
        skin.append(A.ell(7, (0.590, 0.054 * s, 0.636), (1, 0, 0), (0.030, 0.012, 0.012), k=0.015))  # Jochbein
    # Maul: weiter Spalt bis unter die Wangen, Augenhoehlen
    skin.append(A.box(7, (0.632, 0, 0.555), (1, 0, 0.12), (0.060, 0.066, 0.0032), 0.002, op='sub', k=0.004))
    for s in (1, -1):
        skin.append(A.ell(7, (0.626, 0.034 * s, 0.657), (1, 0, 0), (0.015, 0.016, 0.012), op='sub', k=0.005))
        eyes.append(A.ell(7, (0.620, 0.034 * s, 0.655), (1, 0.2 * s, 0), (0.0085, 0.0095, 0.0080)))
        # Runde 5: schweres Oberlid ueber der halben Iris - das freie Auge las sich als Glubschauge
        skin.append(A.ell(7, (0.623, 0.034 * s, 0.6625), (1, 0.2 * s, 0), (0.0105, 0.0130, 0.0052), k=0.003))
        skin.append(A.ell(7, (0.622, 0.034 * s, 0.6455), (1, 0.2 * s, 0), (0.0090, 0.0120, 0.0030), k=0.003))  # Unterlid
    mouth += [A.ell(7, (0.610, 0, 0.554), (1, 0, 0.1), (0.060, 0.050, 0.012)),
              A.ell(8, (0.605, 0, 0.543), (1, 0, -0.2), (0.050, 0.030, 0.008), k=0.006)]
    # Zaehne: obere und untere Reihe auf einem Bogen, ungleich lang und schief
    for row, (bone, z, dz) in enumerate(((7, 0.5585, -1.0), (8, 0.5485, 1.0))):
        n = 24 if row == 0 else 22
        pts, dirs = [], []
        for i in range(n):
            th = math.radians(-82 + 164 * i / (n - 1))
            x = 0.588 + 0.078 * math.cos(th) * (1 - 0.06 * row)
            y = 0.056 * math.sin(th)
            pts.append((x, y, z))
            dirs.append((0.25 * math.cos(th), -0.15 * math.sin(th), dz))
        teeth += A.teeth_row(bone, pts, 0.0125 if row == 0 else 0.0105, 0.0032, dirs, rng, jitter=0.45)

    # --- Vorderglieder ---------------------------------------------------------------------------------------
    for side, s in ((0, 1), (1, -1)):
        o = 9 * side
        Sh, El, Wr, Kn = P(11 + o), P(12 + o), P(13 + o), P(14 + o)
        scap, up_b, fo_b, pa_b, d_b = 9 + o, 10 + o, 11 + o, 12 + o, 13 + o
        out = np.array([0, s, 0.0])
        skin += [
            A.ell(up_b, Sh + out * 0.012 + (0, 0, 0.004), Sh - El, (0.048, 0.040, 0.046), up=(0, s, 1), k=0.03),  # Schulter
            A.cone(scap, (0.205, 0.060 * s, 0.745), A.lerp(Sh, El, 0.28), 0.026, 0.018, k=0.03),                 # Achsel
            A.cone(up_b, Sh, El, 0.033, 0.025, k=0.02),
            A.muscle(up_b, Sh, El, 0.021, offset=(0.012, 0, 0), k=0.012),                                     # Bizeps
            A.muscle(up_b, Sh, El, 0.024, offset=(-0.013, 0.004 * s, 0), k=0.012, t0=0.1, t1=0.75),           # Trizeps
            A.ell(fo_b, El + (-0.020, 0.002 * s, 0.004), El - Sh, (0.016, 0.015, 0.016), k=0.012),             # Ellbogenspitze
            A.cone(fo_b, El, Wr, 0.029, 0.018, k=0.015),
            A.muscle(fo_b, El, Wr, 0.024, offset=(0.006, 0.006 * s, 0), k=0.014, t0=0.04, t1=0.55),           # Unterarmmuskel
            A.ell(fo_b, Wr + out * 0.006, Wr - El, (0.020, 0.019, 0.016), k=0.01),                               # Handgelenk
        ]
        ax = A.unit(Kn - Wr)
        upv = A.perp(ax, (0, 0, 1))
        lat = A.unit(np.cross(upv, ax))  # quer zur Hand
        hl = np.linalg.norm(Kn - Wr)
        skin.append(A.ell(pa_b, A.lerp(Wr, Kn, 0.55) + upv * 0.001, ax, (hl * 0.55, 0.031, 0.011), up=upv, k=0.014))
        for i in range(4):  # Mittelhandknochen unter der Haut des Handrueckens
            q = Kn + lat * (0.016 * (1.5 - i))
            skin.append(A.cone(pa_b, A.lerp(Wr, Kn, 0.25) + upv * 0.006, q + upv * 0.006, 0.0055, 0.0062, k=0.008))
        # Finger: lang, ueber die Mittelgelenke gewoelbt (greifen den Boden), Knoechel als Hoecker, Krallen
        tips = [P(15 + o + i) for i in range(5)]
        for i in range(5):
            tip = tips[i]
            if i == 0:  # Daumen setzt naeher am Handgelenk an
                base = A.lerp(Wr, Kn, 0.55) + (tip - Kn) * 0.0 + A.unit(tip - Kn) * 0.0
                base = base + A.unit(tips[0] - tips[4]) * 0.022
            else:
                base = Kn + A.unit(tips[0] - tips[4]) * (0.016 * (2.5 - i)) * 0.9
            d = A.unit(tip - base)
            mid = A.lerp(base, tip, 0.5) + np.array([0, 0, 0.016 if i else 0.010])
            mid2 = A.lerp(base, tip, 0.8) + np.array([0, 0, 0.010 if i else 0.006])
            r0 = 0.0085 if i else 0.0095
            skin += A.chain(lambda k, b=d_b + i: b, [base, mid, mid2, tip + (0, 0, 0.004)], [r0, r0 * 0.85, r0 * 0.72, r0 * 0.6],
                            k=0.004)
            skin.append(A.ell(d_b + i, base + (0, 0, 0.004), d, (0.010, 0.010, 0.009), k=0.006))           # Knoechel
            cdir = A.unit(d + np.array([0, 0, -0.9]))
            claws[side].append(A.cone(d_b + i, tip + (0, 0, 0.004) + d * 0.002, tip + cdir * 0.022, 0.0045, 0.0006, group='horn'))

    # --- Hinterglieder ---------------------------------------------------------------------------------------
    for side, s in ((0, 1), (1, -1)):
        o = 7 * side
        Hp, Kn, Hk, Ft = P(29 + o), P(30 + o), P(31 + o), P(32 + o)
        toes = [P(33 + o + i) for i in range(3)]
        th_b, sh_b, mt_b, t_b = 27 + 6 * side, 28 + 6 * side, 29 + 6 * side, 30 + 6 * side
        tax = A.unit(Kn - Hp)
        tup = A.perp(tax, (0, 0, 1))
        sax = A.unit(Hk - Kn)
        sup = A.perp(sax, (0, 0, 1))
        skin += [
            A.cone(th_b, Hp + (0.02, 0, 0.0), Kn, 0.055, 0.033, k=0.03),
            A.muscle(th_b, Hp, Kn, 0.038, offset=tup * 0.012, k=0.016, t0=0.12, t1=0.88),                 # Quadrizeps
            A.muscle(th_b, Hp, Kn, 0.028, offset=-tup * 0.014, k=0.016, t0=0.05, t1=0.75),                # hinten
            A.muscle(th_b, Hp, Kn, 0.020, offset=np.array([0, -0.014 * s, 0]), k=0.014, t0=0.05, t1=0.55),  # innen
            A.ell(th_b, Kn + tax * 0.016 + tup * 0.006, tax, (0.016, 0.018, 0.013), up=tup, k=0.01),        # Kniescheibe
            A.cone(sh_b, Kn, Hk, 0.029, 0.016, k=0.012),
            A.muscle(sh_b, Kn, Hk, 0.026, offset=sup * 0.012, k=0.014, t0=0.05, t1=0.5),                   # Wade
            A.cone(sh_b, A.lerp(Kn, Hk, 0.5) + sup * 0.010, Hk + sup * 0.006 + sax * 0.004, 0.008, 0.007, k=0.008),  # Achillessehne
            A.ell(mt_b, Hk + sax * 0.006 + np.array([0, 0, -0.004]), sax, (0.018, 0.016, 0.016), k=0.01),  # Ferse
        ]
        fax = A.unit(Ft - Hk)
        fup = A.perp(fax, (0, 0, 1))
        fl = np.linalg.norm(Ft - Hk)
        skin.append(A.ell(mt_b, A.lerp(Hk, Ft, 0.55) + fup * 0.001, fax, (fl * 0.55, 0.025, 0.012), up=fup, k=0.016))
        skin.append(A.ell(mt_b, Ft + fup * 0.004, fax, (0.016, 0.030, 0.010), up=fup, k=0.012))       # Ballen
        # fuenf Zehen auf drei Knochen (innen gross)
        inner = toes[2]
        outer = toes[0]
        for i in range(5):
            t = i / 4.0
            tip = A.lerp(inner, outer, t) + np.array([-0.010 * abs(t - 0.2), 0, 0.004])
            lat = A.unit(outer - inner)
            base = Ft + lat * ((t - 0.4) * 0.040) + np.array([0, 0, 0.006])
            bone = t_b + (2 if t < 0.3 else (1 if t < 0.7 else 0))
            r0 = 0.0100 if i == 0 else 0.0075 - 0.0005 * i
            mid = A.lerp(base, tip, 0.55) + np.array([0, 0, 0.006])
            skin += A.chain(lambda k, b=bone: b, [base, mid, tip], [r0, r0 * 0.85, r0 * 0.7], k=0.004)
            claws[2 + side].append(A.cone(bone, tip + A.unit(tip - base) * 0.002, tip + A.unit(tip - base) * 0.012 + np.array([0, 0, -0.006]),
                                          0.0035, 0.0006, group='horn'))

    # --- Haar --------------------------------------------------------------------------------------------------
    hair.append(A.ell(7, (0.548, 0, 0.700), (1, 0, -0.25), (0.092, 0.076, 0.058), k=0.0))   # Kappe
    def locks(joints, bone0, nlock, spread, r0, seed):
        r = np.random.default_rng(seed)
        pts0 = [P(j) for j in joints]
        out = []
        for li in range(nlock):
            off = np.array([r.normal(0, spread), r.normal(0, spread * 1.4), r.normal(0, spread * 0.3)])
            dense = []
            for a in range(len(pts0) - 1):  # weiche Kurve durch die Gelenke
                for t in np.linspace(0, 1, 3, endpoint=False):
                    dense.append((A.lerp(pts0[a], pts0[a + 1], t), a))
            dense.append((pts0[-1], len(pts0) - 2))
            pts, bs, rad = [], [], []
            for qi, (q, a) in enumerate(dense):
                f = qi / (len(dense) - 1)
                sway = np.array([r.normal(0, 0.004), r.normal(0, 0.004), 0]) * f
                pts.append(q + off * (0.45 + 0.8 * f) + sway)
                bs.append(bone0 + a)
                rad.append(r0 * (1.0 - 0.78 * f ** 1.4) * (0.8 + 0.4 * r.random()))  # spitz zulaufend
            seg = A.chain(lambda k, bs=bs: bs[k], pts, rad, k=0.006, group='hair')
            for sp in seg:  # Nachbarstraehnen verschmelzen weich zu einer Masse (Rillen kommen aus der Verschiebung)
                sp.k = 0.007
            out += seg
        return out
    for k in range(3):  # Kopfstraehnen: vom Scheitel nach vorn und unten, ein Vorhang vor dem Gesicht
        hair += locks([43 + 4 * k + i for i in range(4)], 39 + 3 * k, 34, 0.026, 0.0056, 100 + k)  # Runde 5: duenner, mehr
    # Wikidot nennt Haar nur am Kopf: vom Hinterkopf faellt es noch den Nacken hinab (die beiden vorderen Ruecken-
    # straehnen); die uebrigen Knochen der Maehne bleiben ohne Haar
    for k in range(2):
        hair += locks([55 + 3 * k + i for i in range(3)], 48 + 2 * k, 18, 0.024, 0.0060, 200 + k)

    # --- Verschiebungen ----------------------------------------------------------------------------------------
    rib_c = np.array([0.125, 0, 0.752])

    def skin_disp(Q):
        d = -0.0006 * sdf.fbm(Q, 60, 2, seed=11)                               # Hautunruhe (grob; fein im Shader)
        # Rippen an den Flanken: schraeg nach hinten unten, nur am Brustkorb
        rq = (Q - rib_c) / np.array([0.205, 0.124, 0.104])
        on = np.clip(1.25 - np.linalg.norm(rq, axis=1), 0, 1) * np.clip((np.abs(Q[:, 1]) - 0.045) / 0.04, 0, 1)
        on *= np.clip((0.835 - Q[:, 2]) / 0.03, 0, 1) * np.clip((Q[:, 0] + 0.07) / 0.05, 0, 1) * np.clip((0.30 - Q[:, 0]) / 0.04, 0, 1)
        ph = (Q[:, 0] + 0.45 * (0.83 - Q[:, 2])) / 0.027 * 2 * math.pi
        d -= 0.0050 * np.maximum(np.cos(ph), 0) ** 1.5 * on
        # Raeude und Schorf: Flecken mit Krusten
        patch = np.clip((sdf.fbm(Q, 7, 3, seed=21) - 0.18) * 4, 0, 1)
        d1, d2 = sdf.worley(Q, 70, seed=22)
        d -= 0.0012 * patch * np.clip(1 - (d2 - d1) * 4, 0, 1)
        # Adern an Unterarmen und Schienbeinen, Falten an Ellbogen und Knien
        for i in (12, 21, 30, 37):
            jt = np.asarray(J[i])
            near = np.exp(-np.sum((Q - jt) ** 2, axis=1) / (0.035 ** 2))
            d -= 0.0012 * near * (sdf.ridged(Q, 70, 2, seed=30 + i) - 0.5)
        limb = np.zeros(len(Q))
        for a, b in ((12, 13), (21, 22), (30, 31), (37, 38)):
            pa, pb = np.asarray(J[a]), np.asarray(J[b])
            ab = pb - pa
            t = np.clip((Q - pa) @ ab / (ab @ ab), 0, 1)
            dd = np.linalg.norm(Q - (pa + np.outer(t, ab)), axis=1)
            limb = np.maximum(limb, np.clip(1 - dd / 0.045, 0, 1))
        d -= 0.0010 * limb * np.clip((sdf.ridged(Q * np.array([1, 1, 0.35]), 45, 2, seed=41) - 0.72) * 4, 0, 1)
        return d

    def hair_disp(Q):
        # Straehnen haengen meist nach unten: Rillen laengs (senkrecht gestreckt), dazu Klumpen
        g = sdf.ridged(Q * np.array([1, 1, 0.12]), 190, 2, seed=51)  # Rillen ~5 mm (das Netz hat 2 mm)
        return -0.0011 * (g - 0.5) - 0.0012 * (sdf.fbm(Q, 45, 2, seed=52))

    def claw_disp(Q):
        return -0.0002 * sdf.ridged(Q * np.array([1, 1, 3]), 900, 1, seed=61)

    groups = [
        {'name': 'haut', 'fm': 'ent_skin', 'parts': skin, 'disp': skin_disp, 'h': 0.0026, 'cav_scale': 0.010},
        {'name': 'haar', 'fm': 'ent_hair', 'parts': hair, 'disp': hair_disp, 'h': 0.0016, 'cav_scale': 0.008},
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.0006, 'cav_scale': 0.003},
        {'name': 'maul', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.0015, 'cav_scale': 0.006},
        {'name': 'augen', 'fm': 'ent_eyeshine', 'parts': eyes, 'disp': None, 'h': 0.0008, 'cav_scale': 0.004},
    ]
    for i, c in enumerate(claws):
        groups.append({'name': f'krallen{i}', 'fm': 'ent_horn', 'parts': c, 'disp': claw_disp, 'h': 0.0007, 'cav_scale': 0.003})
    return groups
