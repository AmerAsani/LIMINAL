"""V13.3: Duller (Entity 6, Wikidot Rev. 52) - KREATUR_Duller.md. Gross (2,19 m), dunkelgrau, ausgemergelt bis auf die
Knochen (Rippen, Wirbel, Schulterblaetter, Beckenkaemme, Knie als Knoten), ohne Gesicht und Ohren: ein glatter,
laenglicher Schaedel, vorn abgeflacht, wo das Gesicht fehlt. Sehr lange Arme: der Unterarm besteht aus sieben ineinander
steckenden Gliedern (je duenner), jedes mit einem knochigen Knoten und einer runzligen Manschette am Ende, aus der das
naechste gleitet - ausgefahren ein vielgliedriger, peitschenduenner Arm. Lange, duenne Finger ohne Naegel.
Dazu die Bruchstelle in der Wand (gehoert zum Wesen): Risse, ein Rand aus gebrochenem Beton um ein dunkles Loch und fuenf
Brocken, die zusammen die Woelbung bilden.

Knochen (anim/creature.hpp, du): 1 Becken, 2 Wirbel, 3 Brust, 4 Hals, 5 Kopf; je Arm s: 6+20s Schluesselbein, +1 Oberarm,
+2..+8 Glieder 0..6, +9 Hand, +10.. Finger (je 2); Beine 46+3s (Ober-, Unterschenkel, Fuss); 52 Risse, 53 Rand, 54-58 Brocken.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (90000, 26000, 7500)
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])

# Gelenke (wie anim/creature.hpp)
ROOT, PELVIS, SPINE, CHEST, NECK, HEAD, HEADTOP, ARM0 = 0, 1, 2, 3, 4, 5, 6, 7
KSEG, KFING, KCHUNKS = 7, 5, 5
KARMJ = 2 + 2 * KSEG + 1 + KFING * 3
LEG0 = ARM0 + 2 * KARMJ
CRACK_A, RIM_A = LEG0 + 8, LEG0 + 10
CHUNK0 = LEG0 + 12
KARMB = 2 + KSEG + 1 + KFING * 2
B_PELVIS, B_SPINE, B_CHEST, B_NECK, B_HEAD, B_ARM0 = 1, 2, 3, 4, 5, 6
B_LEG0 = B_ARM0 + 2 * KARMB
B_CRACK, B_RIM, B_CHUNK0 = B_LEG0 + 6, B_LEG0 + 7, B_LEG0 + 8
BREACH = np.array([1.5, 0.0, 1.45])


def sh(s): return ARM0 + s * KARMJ
def el(s): return sh(s) + 1
def segS(s, k): return sh(s) + 2 + k
def segE(s, k): return sh(s) + 2 + KSEG + k
def kn(s): return sh(s) + 2 + 2 * KSEG
def fin(s, f, i): return kn(s) + 1 + f * 3 + i
def leg(s, i): return LEG0 + s * 4 + i
def bUpper(s): return B_ARM0 + s * KARMB + 1
def bSeg(s, k): return B_ARM0 + s * KARMB + 2 + k
def bHand(s): return B_ARM0 + s * KARMB + 2 + KSEG
def bFin(s, f, i): return bHand(s) + 1 + f * 2 + i
def bLeg(s, i): return B_LEG0 + s * 3 + i


def seg_r(k):
    """Radius des Glieds k am Ende (das aeussere ist der Unterarm); je innen duenner (ineinander gesteckt)."""
    return 0.034 if k == 0 else 0.031 - 0.0024 * (k - 1)


def build(J, bones):
    J = np.asarray(J, dtype=float)
    assert bones[B_CRACK]['a'] == CRACK_A and bones[B_CHUNK0]['a'] == CHUNK0, 'Skelett passt nicht zu duller.py'
    rng = np.random.default_rng(6)
    P = lambda i: J[i]  # noqa: E731
    skin = []

    # --- Rumpf: schmaler Brustkorb mit Rippen, eingefallener Bauch, Becken mit vorstehenden Kaemmen ---------------------
    cage_c, cage_r = np.array([0.015, 0, 1.56]), np.array([0.100, 0.128, 0.165])
    torso = [
        A.ell(B_CHEST, cage_c, X, cage_r, k=0.0),
        A.ell(B_CHEST, (0.0, 0, 1.715), X, (0.075, 0.165, 0.055), k=0.045),           # Schulterguertel
        A.ell(B_SPINE, (0.0, 0, 1.29), X, (0.062, 0.082, 0.15), k=0.05),              # Taille (duenn), bis ins Becken
        A.cone(B_SPINE, (-0.035, 0, 1.12), (-0.03, 0, 1.5), 0.045, 0.05, k=0.04),     # Wirbelsaeule traegt die Taille
        A.ell(B_PELVIS, (-0.005, 0, 1.11), X, (0.072, 0.115, 0.075), k=0.045),        # Becken
    ]
    skin += torso
    skin.append(A.ell(B_SPINE, (0.07, 0, 1.32), X, (0.035, 0.065, 0.085), op='sub', k=0.035))  # eingefallener Bauch
    skin.append(A.ell(B_CHEST, (0.105, 0, 1.43), X, (0.03, 0.09, 0.03), op='sub', k=0.03))     # unter dem Rippenbogen
    for s, g in ((0, 1.0), (1, -1.0)):
        skin += [
            A.ell(B_PELVIS, (0.03, g * 0.112, 1.155), X, (0.032, 0.014, 0.018), k=0.015),     # Beckenkamm
            A.ell(B_PELVIS, (0.055, g * 0.085, 1.10), X, (0.016, 0.012, 0.03), k=0.015),      # Darmbeinstachel
            A.ell(B_PELVIS, (-0.05, g * 0.05, 1.04), X, (0.038, 0.045, 0.055), k=0.03),       # Gesaess, flach
            A.ell(B_CHEST, (-0.082, g * 0.088, 1.655), X, (0.012, 0.055, 0.075), up=Z, k=0.03),  # Schulterblatt
            A.cone(B_CHEST, (-0.088, g * 0.045, 1.71), (-0.083, g * 0.12, 1.60), 0.009, 0.008, k=0.012),  # Rand
            A.cone(B_CHEST, (0.045, g * 0.022, 1.785), (0.02, g * 0.158, 1.805), 0.0095, 0.0085, k=0.012),  # Schluesselbein
            A.ell(B_CHEST, (0.0, g * 0.168, 1.81), X, (0.025, 0.02, 0.018), k=0.015),         # Schulterhoehe
            A.cone(B_CHEST, (-0.02, g * 0.035, 1.87), (0.0, g * 0.15, 1.815), 0.024, 0.018, k=0.035),  # Kapuzenmuskel
        ]
        # Rippen: Grate auf dem Brustkorb, vorn hoch, hinten tiefer
        for k in range(7):
            z0 = 1.67 - 0.038 * k
            pts = []
            for t in np.linspace(0.3, 2.75, 9):
                z = z0 - 0.03 * (t - 0.3)
                f = math.sqrt(max(0.05, 1.0 - ((z - cage_c[2]) / cage_r[2]) ** 2))
                pts.append(cage_c + np.array([cage_r[0] * f * math.cos(t) * 0.985, g * cage_r[1] * f * math.sin(t) * 0.985, z - cage_c[2]]))
            skin += A.chain(lambda i: B_CHEST, pts, [0.0068] * 9, k=0.009)
    skin.append(A.ell(B_CHEST, (0.112, 0, 1.585), X, (0.01, 0.016, 0.1), k=0.02))  # Brustbein
    # Dornfortsaetze den Ruecken hinab (Rueckenlinie gemessen)
    zs = np.arange(1.84, 1.02, -0.034)
    for z in zs:
        xs = np.arange(-0.25, 0.0, 0.002)
        Q = np.stack([xs, np.zeros(len(xs)), np.full(len(xs), z)], axis=1)
        d = sdf.sample(torso, Q)
        inside = np.nonzero(d < 0)[0]
        if not len(inside):
            continue
        xb = xs[inside[0]]
        b = B_CHEST if z > 1.42 else (B_SPINE if z > 1.2 else B_PELVIS)
        skin.append(A.ell(b, (xb + 0.006, 0, z), X, (0.011, 0.009, 0.012), k=0.012))

    # --- Hals und Kopf: lang und duenn; ein Schaedel ohne Gesicht und ohne Ohren. Die Haut ist ueber allem zugewachsen und
    # spannt sich straff ueber die Knochen: Brauenwulst, tiefe Mulden, wo Augen waeren, Wangenknochen, eingefallene Schlaefen
    # und Wangen, ein schmaler Kiefer - keine Oeffnung (Interpretation: kein Gesicht = zugewachsen, nicht glatt wie ein Ei)
    skin += [
        A.cone(B_NECK, (0.0, 0, 1.74), (0.025, 0, 1.96), 0.042, 0.033, k=0.035),
        A.ell(B_HEAD, (-0.005, 0, 2.10), X, (0.108, 0.078, 0.112), k=0.02),            # Hirnschaedel, laenglich
        A.ell(B_HEAD, (-0.07, 0, 2.125), X, (0.065, 0.066, 0.085), k=0.04),           # Hinterkopf, ausladend
        A.ell(B_HEAD, (0.088, 0, 2.112), Y, (0.066, 0.016, 0.014), up=Z, k=0.018),    # Brauenwulst
        A.ell(B_HEAD, (0.082, 0, 2.03), X, (0.032, 0.03, 0.035), k=0.03),             # Oberkiefer
        A.ell(B_HEAD, (0.058, 0, 1.978), X, (0.05, 0.042, 0.024), k=0.03),            # Unterkiefer, schmal
        A.ell(B_HEAD, (0.098, 0, 1.968), X, (0.014, 0.016, 0.014), k=0.02),           # Kinn
        A.ell(B_HEAD, (0.103, 0, 2.065), Z, (0.03, 0.007, 0.006), up=X, k=0.012),     # Nasenbein
        A.ell(B_HEAD, (0.12, 0, 2.0), X, (0.006, 0.026, 0.0025), op='sub', k=0.01),   # zugewachsene Mundfalte
    ]
    for g in (1.0, -1.0):
        skin += [
            A.ell(B_HEAD, (0.098, g * 0.032, 2.078), X, (0.022, 0.022, 0.018), op='sub', k=0.012),  # Mulde, wo ein Auge waere
            A.ell(B_HEAD, (0.072, g * 0.052, 2.048), X, (0.026, 0.012, 0.013), k=0.018),             # Wangenknochen
            A.ell(B_HEAD, (0.03, g * 0.074, 2.09), X, (0.032, 0.012, 0.03), op='sub', k=0.02),      # Schlaefe, eingefallen
            A.ell(B_HEAD, (0.088, g * 0.046, 2.0), X, (0.02, 0.014, 0.018), op='sub', k=0.016),     # Wange, eingefallen
            A.cone(B_NECK, (0.045, g * 0.026, 1.94), (0.035, g * 0.05, 1.775), 0.0095, 0.0085, k=0.015),  # Halssehnen
        ]

    # --- Beine: lang, duenn, knotige Knie und Knoechel, lange Zehen ---------------------------------------------------
    for s, g in ((0, 1.0), (1, -1.0)):
        H, K, An, T = P(leg(s, 0)), P(leg(s, 1)), P(leg(s, 2)), P(leg(s, 3))
        th, sn, ft = bLeg(s, 0), bLeg(s, 1), bLeg(s, 2)
        skin += [
            A.cone(th, H + (0, g * 0.005, 0.03), K, 0.058, 0.04, k=0.03),
            A.muscle(th, H, K, 0.035, offset=(0.012, 0, 0), k=0.015, t0=0.1, t1=0.7),
            A.ell(th, H + (0.0, g * 0.03, -0.04), Z, (0.05, 0.03, 0.035), k=0.03),       # Trochanter
            A.ell(sn, K + X * 0.034 + Z * 0.01, Z, (0.032, 0.032, 0.03), k=0.016),       # Kniescheibe
            A.ell(sn, K + Y * (g * 0.022) - X * 0.005, Z, (0.03, 0.022, 0.03), k=0.016),  # Gelenkknorren
            A.ell(sn, K - Y * (g * 0.022) - X * 0.005, Z, (0.03, 0.022, 0.03), k=0.016),
            A.cone(sn, K, An + Z * 0.02, 0.036, 0.022, k=0.02),
            A.muscle(sn, K, An, 0.024, offset=(-0.014, 0, 0), k=0.012, t0=0.08, t1=0.55),  # Wade, mager
            A.cone(sn, K + X * 0.028, An + X * 0.02 + Z * 0.05, 0.01, 0.008, k=0.012),     # Schienbeinkante
            A.ell(sn, An + Y * (g * 0.026), Z, (0.016, 0.012, 0.014), k=0.012),             # Knoechel
            A.ell(sn, An - Y * (g * 0.022), Z, (0.015, 0.012, 0.013), k=0.012),
            A.cone(ft, An - X * 0.04, An + Z * 0.0, 0.024, 0.026, k=0.015),                 # Ferse
            A.ell(ft, A.lerp(An, T, 0.5) - Z * 0.022, A.unit(T - An), (0.1, 0.03, 0.016), k=0.02),  # Fuss, lang und flach
        ]
        for t in range(5):  # lange Zehen
            base = T + Y * (g * (0.024 - 0.012 * t)) - X * (0.012 * t) - Z * 0.005
            L = 0.07 - 0.009 * t
            skin += A.chain(lambda i, b=ft: b, [base, base + X * L * 0.5 + Z * 0.004, base + X * L], [0.0085 - 0.0008 * t, 0.007 - 0.0007 * t, 0.005], k=0.006)

    # --- Arme: Oberarm und Unterarm (Glied 0) am Koerper; die inneren Glieder und die Hand je fuer sich -----------------
    # Jedes Glied wie ein langgezogener Fingerknochen unter straffer Haut: Schaft mit Knochenkante und zwei Sehnen, am Ende
    # ein Knoten aus zwei Gelenkknorren mit Ringfalten, aus dem das naechste (duennere) Glied gleitet
    def segment(s, k, d, waist):
        a, e = P(segS(s, k)), P(segE(s, k))
        r, b = seg_r(k), bSeg(s, k)
        L = float(np.linalg.norm(e - a))
        n1 = A.perp(d, X)
        n2 = A.unit(np.cross(d, n1))
        q = lambda t: a + d * (L * t)  # noqa: E731
        parts = [
            A.tube(b, q(0.0), q(0.3), (r * 0.95, r * 0.95), (r * waist * 0.8, r * waist), up=n1, k=0.0),  # ovaler Schaft
            A.tube(b, q(0.3), q(0.72), (r * waist * 0.8, r * waist), (r * waist * 0.85, r * (waist + 0.1)), up=n1, k=0.012),
            A.tube(b, q(0.72), q(0.9), (r * waist * 0.85, r * (waist + 0.1)), (r, r), up=n1, k=0.012),
            A.cone(b, q(0.1) + n2 * (r * 0.5) + n1 * (r * 0.2), q(0.6) - n2 * (r * 0.45), 0.0032, 0.003, k=0.006),  # Ader, schraeg
            A.cone(b, q(0.6) - n2 * (r * 0.45), q(0.9) - n1 * (r * 0.5), 0.003, 0.0028, k=0.006),
            A.cone(b, q(0.2) + n1 * (r * 0.6), q(0.86) + n1 * (r * 0.68), 0.0042, 0.0048, k=0.008),   # Knochenkante
            A.cone(b, q(0.15) + n2 * (r * 0.58), q(0.88) + n2 * (r * 0.7), 0.0046, 0.0052, k=0.008),  # Sehnen
            A.cone(b, q(0.15) - n2 * (r * 0.58), q(0.88) - n2 * (r * 0.7), 0.0042, 0.0048, k=0.008),
            A.ell(b, e - d * 0.032 + n2 * (r * 0.5), d, (0.034, r * 0.92, r * 0.92), k=0.012),          # Gelenkknorren
            A.ell(b, e - d * 0.032 - n2 * (r * 0.5), d, (0.034, r * 0.92, r * 0.92), k=0.012),
            A.ell(b, e - d * 0.03, d, (0.036, r * 1.2, r * 1.3), up=n1, k=0.012),
            A.ell(b, e - d * 0.03 + n1 * (r * 0.8), d, (0.022, r * 0.45, r * 0.4), up=n1, k=0.01),       # Knochenhoecker
        ]
        if k < KSEG - 1:
            parts.append(A.ell(b, e + d * 0.004, d, (0.012, seg_r(k + 1) + 0.0015, seg_r(k + 1) + 0.0015), op='sub', k=0.004))
        return parts

    groups_arm = []
    for s, g in ((0, 1.0), (1, -1.0)):
        S, E = P(sh(s)), P(el(s))
        d = A.unit(P(segE(s, 0)) - P(segS(s, 0)))
        up_b = bUpper(s)
        skin += [
            A.ell(up_b, S + Y * (g * 0.012), A.unit(E - S), (0.05, 0.036, 0.034), up=Z, k=0.045),  # Schulter, mager
            A.cone(up_b, S, E, 0.031, 0.025, k=0.02),
            A.muscle(up_b, S, E, 0.02, offset=(0.008, 0, 0), k=0.012, t0=0.15, t1=0.7),
            A.cone(up_b, A.lerp(S, E, 0.15) - X * 0.02, A.lerp(S, E, 0.9) - X * 0.022, 0.006, 0.0055, k=0.01),  # Trizepssehne
            A.ell(bSeg(s, 0), E - d * 0.012 - X * 0.014, d, (0.028, 0.026, 0.026), k=0.015),  # Ellbogen, knochig
        ]
        skin += segment(s, 0, d, 0.9)
        for k in range(1, KSEG):
            parts = segment(s, k, d, 0.66)
            if k == KSEG - 1:  # Hand und Finger am innersten Glied: grosse, flache Hand, sehr lange Finger ohne Naegel
                W, KN = P(segE(s, k)), P(kn(s))
                hd = A.unit(KN - W)
                r = seg_r(k)
                parts += [
                    A.ell(bHand(s), A.lerp(W, KN, 0.55), hd, (0.08, 0.013, 0.045), up=X, k=0.012),
                    A.ell(bHand(s), W + hd * 0.012, hd, (0.022, r * 1.15, r * 1.25), k=0.012),
                ]
                for q in range(4):  # Mittelhandknochen unter der Haut
                    b0 = W + hd * 0.025 + X * (0.03 - 0.02 * q)
                    parts.append(A.cone(bHand(s), b0 + Y * (g * 0.009), KN + X * (0.039 - 0.026 * q) + Y * (g * 0.009), 0.0048, 0.0058, k=0.008))
                for f in range(KFING):
                    p0, p1, p2 = P(fin(s, f, 0)), P(fin(s, f, 1)), P(fin(s, f, 2))
                    rf = 0.0098 if f == 0 else 0.0088
                    parts += [
                        A.ell(bFin(s, f, 0), p0, A.unit(p1 - p0), (0.014, rf * 1.2, rf * 1.15), k=0.006),  # Knoechel
                        A.cone(bFin(s, f, 0), p0, p1, rf, rf * 0.8, k=0.006),
                        A.ell(bFin(s, f, 1), p1, A.unit(p2 - p1), (0.013, rf * 1.05, rf * 1.0), k=0.004),
                        A.cone(bFin(s, f, 1), p1, p2, rf * 0.78, rf * 0.42, k=0.005),
                        A.ell(bFin(s, f, 1), p2 - A.unit(p2 - p1) * 0.012, A.unit(p2 - p1), (0.014, rf * 0.55, rf * 0.5), k=0.006),  # Kuppe
                    ]
            groups_arm.append({'name': f'glied_{s}_{k}', 'fm': 'ent_dullerskin', 'parts': parts, 'disp': seg_disp,
                               'h': 0.0018 if k == KSEG - 1 else 0.002, 'merge': 'arme', 'cav_scale': 0.006, 'lod_weight': 1.5})

    # --- Bruchstelle -----------------------------------------------------------------------------------------------
    breach = build_breach(rng)

    def skin_disp(Q):  # trockene, straffe Haut: feine Falten, leichte Runzeln quer zu den Gliedern
        w = sdf.ridged(Q * np.array([1.0, 1.0, 3.0]), 55, 2, seed=601)
        return -0.0007 * w + 0.0004 * sdf.fbm(Q, 120, 2, seed=602)

    return [
        {'name': 'koerper', 'fm': 'ent_dullerskin', 'parts': skin, 'disp': skin_disp, 'h': 0.0034, 'merge': 'arme', 'cav_scale': 0.011, 'lod_weight': 1.3},
    ] + groups_arm + breach


def seg_disp(Q):  # Glieder: Ringfalten (Haut, die sich zusammenschiebt), wenig Tiefe - die Glieder stecken ineinander
    return -0.0007 * sdf.ridged(Q * np.array([2.5, 2.5, 0.6]), 60, 2, seed=611) + 0.0004 * sdf.fbm(Q, 150, 2, seed=612)


def build_breach(rng):
    """Risse, Rand mit Loch, fuenf Brocken der Woelbung - Ruhepose in der Ebene x = 1,5 (Normale +x) um BREACH."""
    c = BREACH
    groups = []

    def zigzag(p, ang, length, steps, jitter):
        pts = [p]
        for j in range(steps):
            ang += rng.uniform(-jitter, jitter)
            pts.append(pts[-1] + (Y * math.cos(ang) + Z * math.sin(ang)) * (length / steps))
        return pts, ang

    # Risse: haarfeine, gezackte Linien mit Verzweigungen, ungleich lang; an ihren Raendern platzt der Putz ab
    cracks, flakes = [], []
    lines = []
    for i in range(7):
        a = i * 2 * math.pi / 7 + rng.uniform(-0.3, 0.3)
        p0 = c + (Y * math.cos(a) + Z * math.sin(a)) * rng.uniform(0.17, 0.24)
        pts, _ = zigzag(p0, a, rng.uniform(0.12, 0.42), 8, 0.6)
        lines.append((pts, 0.0032))
        for br in range(rng.integers(1, 3)):
            j = int(rng.integers(2, 6))
            pb, _ = zigzag(pts[j], a + rng.choice([-1.0, 1.0]) * rng.uniform(0.5, 1.1), rng.uniform(0.05, 0.16), 5, 0.7)
            lines.append((pb, 0.0018))
    for i in range(6):  # kurze Haarrisse dazwischen
        a = rng.uniform(0, 2 * math.pi)
        p0 = c + (Y * math.cos(a) + Z * math.sin(a)) * rng.uniform(0.25, 0.4)
        pb, _ = zigzag(p0, a + rng.uniform(-1, 1), rng.uniform(0.04, 0.09), 4, 0.8)
        lines.append((pb, 0.0013))
    for pts, w0 in lines:
        n = len(pts) - 1
        for j in range(n):
            w = w0 * (1.0 - 0.6 * j / n) + 0.0006
            cracks.append(A.box(B_CRACK, (pts[j] + pts[j + 1]) / 2 + X * 0.0004, pts[j + 1] - pts[j],
                                (np.linalg.norm(pts[j + 1] - pts[j]) / 2 + 0.0015, w, 0.001), 0.0004, up=X))
            if rng.random() < 0.3:
                side = A.unit(np.cross(X, pts[j + 1] - pts[j]))
                f = pts[j] + side * rng.choice([-1.0, 1.0]) * (w + 0.004)
                flakes.append(A.ell(B_CRACK, f + X * 0.0012, X, (0.0022, rng.uniform(0.006, 0.013), rng.uniform(0.004, 0.01)), up=Z, k=0.0))
    groups.append({'name': 'risse', 'fm': 'ent_umbra', 'parts': cracks, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002, 'lod_weight': 0.25})
    if flakes:
        groups.append({'name': 'abplatzer', 'fm': 'ent_concrete', 'parts': flakes, 'disp': None, 'h': 0.001, 'merge': 'beton', 'lod_weight': 0.4})
    # Rand: gebrochener Beton um das Loch, ungleich weit (innen ~0,1 m, aussen 0,16-0,27 m), 1-4 cm vorstehend, Kanten
    # schraeg herausgebrochen; im Bruch liegen Kiesel (Zuschlag) frei
    rim = []
    n = 30
    for i in range(n):
        a = i * 2 * math.pi / n + rng.uniform(-0.05, 0.05)
        r0 = 0.1 + rng.uniform(-0.012, 0.02)
        r1 = rng.uniform(0.16, 0.27)
        dirv = Y * math.cos(a) + Z * math.sin(a)
        mid = c + dirv * ((r0 + r1) / 2)
        hgt = rng.uniform(0.008, 0.03) * (1.0 - 0.5 * (r1 - 0.16) / 0.11)
        rim.append(A.box(B_RIM, mid + X * (hgt * 0.5), A.unit(dirv - X * 0.25), ((r1 - r0) / 2, 0.02 + 0.012 * rng.random(), hgt),
                         0.002, up=X, k=0.01))
        if rng.random() < 0.55:  # aufgebrochene Stuecke, schraeg heraus
            q = c + dirv * (r0 + rng.uniform(0.0, 0.025))
            rim.append(A.box(B_RIM, q + X * 0.022, A.unit(dirv * 0.5 + X * 0.85), (0.026, 0.017, 0.007), 0.002, up=dirv, k=0.004))
        if rng.random() < 0.6:  # Kiesel
            q = c + dirv * (r0 + rng.uniform(0.005, 0.05))
            rim.append(A.ell(B_RIM, q + X * rng.uniform(0.015, 0.03), X, (0.008, 0.007, 0.006), k=0.003))
    rim.append(A.ell(B_RIM, c - X * 0.01, X, (0.035, 0.1, 0.1), op='sub', k=0.012))  # das Loch
    groups.append({'name': 'rand', 'fm': 'ent_concrete', 'parts': rim, 'disp': concrete_disp, 'h': 0.002, 'merge': 'beton', 'cav_scale': 0.006, 'lod_weight': 0.4})
    # Loch: dunkel, unregelmaessig (mehrere ueberlagerte Scheiben)
    hole = [A.ell(B_RIM, c + X * 0.002 + Y * dy + Z * dz, X, (0.0015, ry, rz)) for dy, dz, ry, rz in
            ((0.0, 0.0, 0.1, 0.098), (0.018, 0.01, 0.085, 0.09), (-0.015, -0.012, 0.09, 0.08), (0.006, -0.02, 0.08, 0.09))]
    groups.append({'name': 'loch', 'fm': 'ent_umbra', 'parts': hole, 'disp': None, 'h': 0.0018, 'lod_weight': 0.1})
    # Brocken: Ausschnitte einer Kuppel (Grundradius 0,28 m, 10 cm hoch), 4 Grad Fuge zwischen ihnen
    for k in range(KCHUNKS):
        th = (k + 0.5) * 2 * math.pi / KCHUNKS
        half = math.pi / KCHUNKS - math.radians(2.0)
        # 'sub' wirkt nur im Kasten des Teils: die Schnittkoerper decken die ganze Kuppel ab (das Gitter bleibt klein)
        parts = [A.ell(B_CHUNK0 + k, c - X * 0.02, X, (0.12, 0.29, 0.29), k=0.0),
                 A.box(B_CHUNK0 + k, c - X * 0.1, X, (0.08, 0.32, 0.32), 0.0, op='sub')]   # hinten flach (in der Wand)
        for sgn in (-1.0, 1.0):  # zwei Halbebenen durch die Achse: was ausserhalb des Ausschnitts liegt, weg
            a = th + sgn * half
            lin = Y * math.cos(a) + Z * math.sin(a)
            nrm = A.unit(np.cross(X, lin)) * (-sgn)  # zeigt in den Ausschnitt
            parts.append(A.box(B_CHUNK0 + k, c - nrm * 0.17, lin, (0.34, 0.17, 0.14), 0.0, up=X, op='sub'))
        groups.append({'name': f'brocken_{k}', 'fm': 'ent_concrete', 'parts': parts, 'disp': concrete_disp, 'h': 0.0028, 'merge': 'beton',
                       'cav_scale': 0.006, 'lod_weight': 0.4})
    return groups


def concrete_disp(Q):  # gebrochener Beton: koernig, kantig
    d1, d2 = sdf.worley(Q, 70, seed=621)
    return -0.0025 * np.clip(d2 - d1, 0, 0.4) + 0.0012 * sdf.fbm(Q, 160, 2, seed=622)
