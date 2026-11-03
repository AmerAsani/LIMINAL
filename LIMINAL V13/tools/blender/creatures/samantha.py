"""V13.3 Teil 4 Level 5: Samantha (Entity 26, Wikidot Rev. 22; Level 5 Rev. 184) - KREATUR_Samantha.md. Eine kleine
Hauskatze, fast ein Kaetzchen: schwarz-weiss (Frack: weisse Brust, weisses Maul, weisse Socken), gruene Augen. Was nicht
stimmt (Wiki: niemand haelt sie fuer eine gewoehnliche Katze; kraeftiger Kiefer, gezackte Zaehne, die Haut und Fleisch
muehelos zerreissen): das Kiefergelenk sitzt weit hinten unter dem Ohr - offen reisst das Maul bis hinter die Augen auf;
oben und unten Reihen saegeartiger Zaehne bis tief in den Rachen, die Fangzaehne zu lang; ein nasser, dunkelroter Gaumen;
die Krallen bleiben ausgefahren. Die Zunge ist breiter als bei einer Katze (sie spricht - Auslegung).
Knochen (sa in src/anim/creature.hpp): 0 Wurzel, 1 Becken, 2 Ruecken, 3 Brust, 4 Hals, 5 Kopf, 6 Kiefer, 7 Zunge, 8/9 Ohren,
10 + Bein * 3 + i (Beine: 0 vorn links, 1 vorn rechts, 2 hinten links, 3 hinten rechts), 22 + i Schwanz.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (16000, 5600, 1900)
PELVIS, SPINE, CHEST, NECK, HEAD, JAW, TONGUE, EAR_L, EAR_R = 1, 2, 3, 4, 5, 6, 7, 8, 9
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def legJ(l, i):
    return 15 + l * 4 + i


def legB(l, i):
    return 10 + l * 3 + i


def build(J, bones):
    rng = np.random.default_rng(26)
    J = np.asarray(J)
    black, white, nose, eyes, pupils, teeth, gum, whisk = [], [], [], [], [], [], [], []
    # --- Rumpf: klein, schlank, kraeftige Schultern -------------------------------------------------------------------------
    black += [A.ell(PELVIS, (-0.095, 0, 0.158), X, (0.066, 0.046, 0.052), k=0.0),
              A.ell(SPINE, (-0.02, 0, 0.166), X, (0.088, 0.047, 0.054), k=0.03),
              A.ell(CHEST, (0.058, 0, 0.162), X, (0.056, 0.044, 0.056), k=0.03),
              A.ell(CHEST, (0.07, 0, 0.185), X, (0.04, 0.042, 0.03), k=0.02),  # Schulterblaetter
              A.cone(NECK, (0.095, 0, 0.19), (0.138, 0, 0.232), 0.034, 0.031, k=0.02)]
    white += [A.ell(CHEST, (0.083, 0, 0.142), A.unit((1, 0, -0.4)), (0.04, 0.034, 0.045), k=0.0),     # Latz
              A.ell(SPINE, (-0.01, 0, 0.122), X, (0.07, 0.03, 0.016), k=0.02),                         # Bauchstreif
              A.ell(NECK, (0.122, 0, 0.198), A.unit((1, 0, -0.8)), (0.022, 0.026, 0.02), k=0.01)]
    # --- Kopf: rund, breite Wangen, kurze Schnauze; der Kiefer reicht weit nach hinten -------------------------------------------
    black += [A.ell(HEAD, (0.146, 0, 0.243), X, (0.041, 0.037, 0.035), k=0.0),
              A.ell(HEAD, (0.158, 0.023, 0.23), X, (0.024, 0.02, 0.019), k=0.012),
              A.ell(HEAD, (0.158, -0.023, 0.23), X, (0.024, 0.02, 0.019), k=0.012)]
    white += [A.ell(HEAD, (0.188, 0, 0.225), X, (0.021, 0.024, 0.0155), k=0.0),   # Maul
              A.ell(HEAD, (0.176, 0, 0.247), X, (0.016, 0.006, 0.02), k=0.006)]    # Blesse auf der Stirn
    nose += [A.ell(HEAD, (0.2075, 0, 0.233), A.unit((1, 0, 0.3)), (0.006, 0.0075, 0.0045), k=0.0)]
    # Unterkiefer (Knochen 6): kraeftig, mit weissem Kinn; innen Zahnfleisch
    # Runde 2: offen hing das weisse Kinn wie ein Lappen - Unterkiefer schwarz, nur die Kinnspitze weiss
    black += [A.ell(JAW, (0.16, 0, 0.207), X, (0.034, 0.02, 0.0085), k=0.0),
              A.ell(JAW, (0.142, 0.017, 0.209), X, (0.022, 0.006, 0.009), k=0.004),
              A.ell(JAW, (0.142, -0.017, 0.209), X, (0.022, 0.006, 0.009), k=0.004)]
    white += [A.ell(JAW, (0.186, 0, 0.2045), X, (0.012, 0.012, 0.006), k=0.0)]
    # Runde 3: offen war das Maul ein schwarzes Loch - die Mundhoehle aus Kopf und Unterkiefer ausschneiden, damit Gaumen und
    # Zahnfleisch frei liegen
    black += [A.ell(HEAD, (0.168, 0, 0.2115), X, (0.037, 0.0155, 0.0052), op='sub', k=0.003),
              A.ell(JAW, (0.163, 0, 0.2128), X, (0.031, 0.0135, 0.0036), op='sub', k=0.002)]
    gum += [A.ell(JAW, (0.165, 0, 0.2135), X, (0.03, 0.0145, 0.0035), k=0.0),          # Unterkiefer innen
            A.ell(HEAD, (0.163, 0, 0.2155), X, (0.034, 0.017, 0.004), k=0.0)]           # Gaumen (sichtbar, wenn offen)
    gum += [A.ell(TONGUE, (0.168, 0, 0.2155), X, (0.024, 0.0105, 0.0032), k=0.0)]       # breite Zunge
    # Saegezaehne: oben und unten je Seite eine Reihe bis tief hinter die Augen, vorn zu lange Fangzaehne
    for s in (1, -1):
        up_pts = [np.array([0.198 - 0.0072 * i, s * (0.0115 + 0.0016 * i), 0.2148]) for i in range(9)]
        lo_pts = [np.array([0.192 - 0.007 * i, s * (0.011 + 0.0015 * i), 0.2108]) for i in range(9)]
        # Abnahme Level 5, Runde 2: die Reihen lasen sich nah als bleiche Schuppenstapel - duenner, spitzer, laenger (Nadeln)
        teeth += A.teeth_row(HEAD, up_pts, 0.0088, 0.0015, [(0.12, s * 0.1, -1.0)] * 9, rng, jitter=0.35)
        teeth += A.teeth_row(JAW, lo_pts, 0.0082, 0.0014, [(0.12, s * 0.1, 1.0)] * 9, rng, jitter=0.35)
        # zweite, innere Reihe (Saege bis in den Rachen)
        teeth += A.teeth_row(HEAD, [p * np.array([1, 0.72, 1]) - X * 0.004 for p in up_pts[1:7]], 0.0058, 0.0012, [(0.0, -s * 0.3, -1.0)] * 6, rng, jitter=0.4)
        teeth += A.teeth_row(JAW, [p * np.array([1, 0.72, 1]) - X * 0.004 for p in lo_pts[1:7]], 0.0055, 0.0011, [(0.0, -s * 0.3, 1.0)] * 6, rng, jitter=0.4)
        teeth += [A.cone(HEAD, (0.199, s * 0.0118, 0.2165), (0.2005, s * 0.0112, 0.2035), 0.0026, 0.0004, k=0.0),   # Fang oben
                  A.cone(JAW, (0.193, s * 0.0105, 0.2095), (0.195, s * 0.0102, 0.2195), 0.0022, 0.0004, k=0.0)]     # Fang unten
    # Augen: gross, gruen, leicht vorstehend; schmale senkrechte Pupillen
    for s in (1, -1):
        c = np.array([0.1715, s * 0.0195, 0.2455])
        d = A.unit((0.86, s * 0.48, 0.05))
        black.append(A.ell(HEAD, c - d * 0.002, d, (0.011, 0.0115, 0.0095), op='sub', k=0.002))
        eyes.append(A.ell(HEAD, c, d, (0.0088, 0.0098, 0.0088), up=Z, k=0.0))
        # Runde 2: Katzenschlitz las sich niedlich - runde, kleine Menschenpupillen (Auslegung: keine gewoehnliche Katze)
        pupils.append(A.ell(HEAD, c + d * 0.0083, d, (0.0012, 0.0034, 0.0034), up=Z, k=0.0))
    # Ohren: dreieckig, aussen schwarz, innen rosa
    for s, base, tip, bone in ((1, 11, 12, EAR_L), (-1, 13, 14, EAR_R)):
        b, t = J[base], J[tip]
        n = A.unit((0.9, s * 0.35, 0.0))
        black.append(A.tube(bone, b - Z * 0.004, t, (0.017, 0.006), (0.0015, 0.0018), up=n, k=0.006))
        nose.append(A.tube(bone, b + n * 0.0035, t - Z * 0.004 + n * 0.002, (0.011, 0.0025), (0.001, 0.001), up=n, k=0.0))
    # --- Beine: vorn schlank mit weissen Socken, hinten kraeftige Keulen; Pfoten mit Ballen, Krallen ausgefahren ------------------
    for l in range(4):
        T, K, W, P = (J[legJ(l, i)] for i in range(4))
        front = l < 2
        s = 1 if l % 2 == 0 else -1
        b0 = legB(l, 0)
        if front:
            black.append(A.cone(b0, T + Z * 0.012, K, 0.019, 0.012, k=0.012))
            black.append(A.cone(b0 + 1, K, A.lerp(K, W, 0.4), 0.0115, 0.0095, k=0.006))
            white.append(A.cone(b0 + 1, A.lerp(K, W, 0.35), W, 0.0098, 0.0085, k=0.004))
        else:
            black.append(A.ell(b0, A.lerp(T, K, 0.45) + X * 0.006, A.unit(K - T), (0.04, 0.022, 0.026), k=0.015))  # Keule
            black.append(A.cone(b0 + 1, K, A.lerp(K, W, 0.6), 0.013, 0.008, k=0.006))
            white.append(A.cone(b0 + 1, A.lerp(K, W, 0.55), W, 0.0085, 0.008, k=0.004))
        fd = A.unit(np.array([P[0] - W[0], P[1] - W[1], 0.0]))
        white.append(A.ell(b0 + 2, W + fd * 0.012 - Z * 0.008, fd, (0.016, 0.0115, 0.0075), k=0.006))
        for t in range(4):  # Zehen mit Krallen
            a = (t - 1.5) * 0.32
            d = A.unit(fd * math.cos(a) + np.cross(Z, fd) * math.sin(a))
            toe = W + fd * 0.017 + d * 0.006 - Z * 0.012
            white.append(A.ell(b0 + 2, toe, d, (0.0055, 0.0045, 0.004), k=0.003))
            nose.append(A.ell(b0 + 2, toe - Z * 0.0035, d, (0.0028, 0.0024, 0.0012), k=0.0))   # Zehenballen
            teeth.append(A.cone(b0 + 2, toe + d * 0.004, toe + d * 0.0105 - Z * 0.004, 0.0013, 0.0002, k=0.0))  # Kralle
        nose.append(A.ell(b0 + 2, W + fd * 0.008 - Z * 0.0155, fd, (0.006, 0.006, 0.0015), k=0.0))  # Ballen
    # --- Schwanz: lang, duenn auslaufend --------------------------------------------------------------------------------------
    tp = [J[31 + i] for i in range(6)]
    black += A.chain(lambda i: 22 + min(i, 4), tp, [0.0145, 0.012, 0.011, 0.0095, 0.008, 0.0055], k=0.006)
    black.append(A.cone(PELVIS, (-0.12, 0, 0.17), tp[0], 0.02, 0.0145, k=0.01))
    # --- Schnurrhaare und Brauenhaare (weiss) --------------------------------------------------------------------------------
    for s in (1, -1):
        for k in range(5):
            p0 = np.array([0.196, s * 0.0155, 0.2275 - 0.0018 * k])
            d = A.unit(np.array([0.25 - 0.06 * k, s * 1.0, 0.12 - 0.1 * k]))
            whisk += A.chain(lambda i: HEAD, [p0, p0 + d * 0.035, p0 + d * 0.066 - Z * (0.006 + 0.002 * k)], [0.00055, 0.0004, 0.00015], k=0.0)
        for k in range(2):
            p0 = np.array([0.178, s * 0.017, 0.258])
            d = A.unit(np.array([0.5, s * 0.6, 0.6 + 0.2 * k]))
            whisk += A.chain(lambda i: HEAD, [p0, p0 + d * 0.025], [0.0004, 0.00012], k=0.0)

    def fur_disp(Q):  # kurzes, dichtes Fell: feine Straehnen, entlang des Koerpers gekaemmt
        streak = sdf.ridged(Q * np.array([0.3, 1.0, 1.0]), 210, 2, seed=2601)
        return -0.0009 * (streak - 0.5) - 0.0007 * sdf.fbm(Q, 70, 2, seed=2602)

    def wet(Q):
        return -0.0002 * sdf.fbm(Q, 300, 2, seed=2603)

    return [
        {'name': 'fell', 'fm': 'ent_sm_fur', 'parts': black, 'disp': fur_disp, 'h': 0.0014, 'cav_scale': 0.004},
        {'name': 'weiss', 'fm': 'ent_sm_furw', 'parts': white, 'disp': fur_disp, 'h': 0.0013, 'cav_scale': 0.004, 'cav_extra': black},
        {'name': 'haut', 'fm': 'ent_sm_nose', 'parts': nose, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002},
        {'name': 'augen', 'fm': 'ent_sm_eye', 'parts': eyes, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002, 'lod_weight': 2.5},
        {'name': 'pupillen', 'fm': 'ent_jy_pupil', 'parts': pupils, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'lod_weight': 2.0},
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.00045, 'cav_scale': 0.001, 'lod_weight': 1.6},
        {'name': 'gaumen', 'fm': 'ent_sm_gum', 'parts': gum, 'disp': wet, 'h': 0.0006, 'cav_scale': 0.002},
        {'name': 'schnurrhaare', 'fm': 'ent_sm_whisker', 'parts': whisk, 'disp': None, 'h': 0.00035, 'cav_scale': 0.001},
    ]
