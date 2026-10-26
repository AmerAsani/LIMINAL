"""V13: Gestalt (Level 0) - KREATUR_Gestalt.md. Ein Mensch im Gegenlicht, kein Kegel und keine Puppe: etwas zu lang,
Arme bis zu den Knien, langer Hals, haengende Schultern, der Kopf leicht schief. Gewicht auf dem rechten Bein (rechte
Huefte hoeher, linkes Knie locker vor, Schultern gegengleich). Erkennbare Masse - Schaedel mit Hinterkopf und Kinn,
Kopfnicker, Kapuzenmuskel, Schluesselbeine, Brustkorb mit Rippenbogen, Becken mit Darmbeinkamm, Knie - aber keine
Gesichtszuege, keine Finger, keine Kleidung. Unter den Knien loest sich die Gestalt in Fasern auf.

Knochen (ent_gestalt): 1 unten (Beine), 2 Mitte (Bauch), 3 Brust (Schultern, Arme), 4 Hals, 5 Kopf,
Fasern w = 0..3: 6+2w, 7+2w (Gelenke 7+3w .. 9+3w).
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (24000, 8000, 2500)
LEGS, BELLY, CHEST, NECK, HEAD = 1, 2, 3, 4, 5
Y = np.array([0.0, 1.0, 0.0])
Z = np.array([0.0, 0.0, 1.0])


def build(J, bones):
    rng = np.random.default_rng(11)
    P = []
    # --- Kopf: leicht nach links geneigt und etwas gesenkt ----------------------------------------------------
    HC = np.array([0.035, 0.008, 1.815])
    roll, pitch = 0.12, 0.10
    up = A.unit(np.array([math.sin(pitch), math.sin(roll), math.cos(roll) * math.cos(pitch)]))
    fw = A.unit(np.array([1.0, 0.0, 0.0]) - up * up[0])
    sd = np.cross(up, fw)
    P += [
        A.ell(HEAD, HC + up * 0.025, fw, (0.095, 0.075, 0.090), up=up, k=0.0),                     # Schaedel
        A.ell(HEAD, HC - fw * 0.050, fw, (0.050, 0.064, 0.062), up=up, k=0.03),                    # Hinterkopf
        A.ell(HEAD, HC + fw * 0.040 - up * 0.045, fw, (0.060, 0.058, 0.070), up=up, k=0.03),       # Gesichtsschaedel
        A.ell(HEAD, HC + fw * 0.074 - up * 0.096, fw, (0.024, 0.030, 0.024), up=up, k=0.025),      # Kinn
        A.ell(HEAD, HC + fw * 0.090 + up * 0.028, fw, (0.012, 0.050, 0.012), up=up, k=0.02),       # Brauenwulst (nur Umriss)
        A.ell(HEAD, HC + fw * 0.098 - up * 0.015, fw - up * 0.4, (0.022, 0.011, 0.010), up=up, k=0.015),  # Nasenruecken
    ]
    for s in (1, -1):
        P.append(A.ell(HEAD, HC + sd * s * 0.050 - up * 0.058 + fw * 0.005, fw, (0.030, 0.020, 0.030), up=up, k=0.03))  # Kieferwinkel
    # --- Hals und Schultern (haengend; rechte Schulter tiefer) --------------------------------------------------
    sh = {1: np.array([0.0, 0.205, 1.478]), -1: np.array([0.0, -0.200, 1.455])}
    P.append(A.cone(NECK, np.array([0.010, 0.0, 1.55]), HC - up * 0.07 - fw * 0.012, 0.062, 0.050, k=0.05))
    for s in (1, -1):
        P.append(A.cone(NECK, HC - fw * 0.022 + sd * s * 0.045 - up * 0.035, np.array([0.058, s * 0.016, 1.50]), 0.019, 0.015, k=0.04))  # Kopfnicker
        P.append(A.cone(CHEST, np.array([-0.010, s * 0.030, 1.61]), sh[s] + np.array([-0.012, -s * 0.03, 0.035]), 0.040, 0.030, k=0.045))  # Kapuzenmuskel
        P.append(A.ell(CHEST, sh[s], np.array([0.0, s * 1.0, -0.5]), (0.052, 0.046, 0.058), up=np.array([1.0, 0, 0]), k=0.035))  # Deltamuskel
        P.append(A.cone(CHEST, np.array([0.060, s * 0.022, 1.488]), sh[s] + np.array([0.015, -s * 0.03, 0.020]), 0.012, 0.011, k=0.035))  # Schluesselbein
    # --- Rumpf -------------------------------------------------------------------------------------------------
    P += [
        A.ell(CHEST, (0.004, 0.0, 1.320), (1, 0, 0.08), (0.098, 0.150, 0.180), k=0.08),               # Brustkorb
        A.ell(BELLY, (0.004, 0.0, 1.085), (1, 0, 0), (0.084, 0.122, 0.135), k=0.055),                # Bauch, schmaler (Rippenbogen)
        A.ell(BELLY, (0.0, 0.0, 0.930), (1, 0, 0), (0.100, 0.162, 0.100), up=(0, 0.06, 1), k=0.06),  # Becken (links tiefer)
    ]
    for s in (1, -1):
        P.append(A.ell(CHEST, (0.052, s * 0.072, 1.385), (1, 0, -0.3), (0.042, 0.068, 0.048), k=0.05))       # Brustmuskel
        # Runde 4: obere Brust bis in die Schulter geschlossen - zwischen Brustmuskel, Deltamuskel und Schluesselbein
        # schien sonst der Hintergrund in kleinen Flecken durch
        P.append(A.ell(CHEST, (0.025, s * 0.135, 1.425), (0, 1, 0), (0.075, 0.062, 0.070), up=(0.2, 0, 1), k=0.06))
        P.append(A.ell(CHEST, (-0.055, s * 0.095, 1.26), (0.2, s * 0.3, 1), (0.13, 0.055, 0.035), up=(-1, 0, 0), k=0.04))  # breiter Rueckenmuskel
        P.append(A.ell(BELLY, (0.025, s * 0.135, 0.975 - 0.015 * s), (1, 0, 0), (0.034, 0.020, 0.016), k=0.05))  # Darmbeinkamm
        P.append(A.ell(LEGS, (-0.052, s * 0.075, 0.862 - 0.012 * s), (1, 0, 0), (0.068, 0.074, 0.085), k=0.04))  # Gesaess
    # --- Beine: rechts traegt (gestreckt), links locker (Knie vor) ----------------------------------------------
    legs = {-1: (np.array([0.0, -0.090, 0.905]), np.array([0.012, -0.095, 0.50]), np.array([-0.012, -0.100, 0.10])),
            1: (np.array([0.0, 0.090, 0.880]), np.array([0.062, 0.110, 0.485]), np.array([0.000, 0.130, 0.10]))}
    for s, (H, K, F) in legs.items():
        dt, ds = A.unit(K - H), A.unit(F - K)
        P.append(A.cone(LEGS, H, K, 0.074, 0.048, k=0.05))
        P.append(A.ell(LEGS, A.lerp(H, K, 0.45) + np.array([0.022, 0, 0]), dt, (0.17, 0.058, 0.052), up=(1, 0, 0), k=0.04))  # Oberschenkelstrecker
        P.append(A.ell(LEGS, K + np.array([0.040, 0, 0.005]), dt, (0.026, 0.026, 0.016), up=(1, 0, 0), k=0.015))  # Kniescheibe
        P.append(A.cone(LEGS, K, F, 0.046, 0.022, k=0.03))
        P.append(A.ell(LEGS, A.lerp(K, F, 0.28) - np.array([0.026, 0, 0]), ds, (0.11, 0.040, 0.036), up=(-1, 0, 0), k=0.03))  # Wade
    # --- Arme: lang, haengend bis zu den Knien; Haende ohne Finger ------------------------------------------------
    arms = {1: (sh[1] + np.array([0.0, 0.015, -0.02]), np.array([-0.015, 0.255, 1.08]), np.array([0.030, 0.250, 0.72]), np.array([0.050, 0.238, 0.54])),
            -1: (sh[-1] + np.array([0.0, -0.015, -0.02]), np.array([-0.020, -0.250, 1.06]), np.array([0.020, -0.245, 0.70]), np.array([0.030, -0.233, 0.52]))}
    for s, (S, E, W, T) in arms.items():
        du, df = A.unit(E - S), A.unit(W - E)
        P.append(A.cone(CHEST, S, E, 0.043, 0.033, k=0.045))
        P.append(A.ell(CHEST, S + np.array([-0.01, -s * 0.035, -0.12]), (0, 0, 1), (0.08, 0.035, 0.05), k=0.04))  # Achsel (keine Luecke)
        P.append(A.ell(CHEST, A.lerp(S, E, 0.45) + np.array([0.012, 0, 0]), du, (0.13, 0.036, 0.040), up=(1, 0, 0), k=0.03))  # Oberarm
        P.append(A.ell(CHEST, E - np.array([0.020, 0, 0]), du, (0.024, 0.024, 0.020), up=(1, 0, 0), k=0.02))   # Ellbogen
        P.append(A.cone(CHEST, E, W, 0.036, 0.021, k=0.025))
        P.append(A.ell(CHEST, A.lerp(E, W, 0.25), df, (0.10, 0.034, 0.030), up=(1, 0, 0), k=0.03))           # Unterarm
        hd = A.unit(T - W)
        inward = A.unit(np.array([0.0, -s, 0.0]) - hd * float(np.array([0.0, -s, 0.0]) @ hd))
        P.append(A.ell(CHEST, W + hd * 0.075, hd, (0.090, 0.044, 0.016), up=inward, k=0.025))            # Hand (Faeustling)
        P.append(A.ell(CHEST, W + hd * 0.035 + np.array([0.022, 0, 0]), hd + np.array([0.6, 0, 0]), (0.032, 0.014, 0.012), up=inward, k=0.02))  # Daumenballen
    # --- Fasern unten: an den Schienbeinen beginnend, zu den Fasergelenken -----------------------------------------
    fibers = []
    for w in range(4):
        a, b, c = (np.asarray(J[7 + 3 * w + i]) for i in range(3))
        s = 1 if a[1] > 0 else -1
        H, K, F = legs[s]
        for f in range(9):  # Runde 1: mehr, duennere, ungleich lange Fasern (vorher wie Wurzeln oder Spinnenbeine)
            jit = lambda amp: np.array([rng.normal(0, amp), rng.normal(0, amp), 0.0])
            p0 = A.lerp(K, F, 0.22 + 0.30 * rng.random()) + jit(0.014)
            end = 0.55 + 0.45 * rng.random()
            pts = [p0, a + jit(0.022), b + jit(0.034), A.lerp(b, c, end) + jit(0.045)]
            rad = [0.013 + 0.006 * rng.random(), 0.009, 0.005, 0.0015]
            bs = [LEGS, 6 + 2 * w, 7 + 2 * w]
            fibers += A.chain(lambda k, bs=bs: bs[k], pts, rad, k=0.02)

    def disp(Q):
        low = 1.0 - np.clip((Q[:, 2] - 0.3) / 0.7, 0.0, 1.0)
        d = -(0.004 + 0.010 * low) * sdf.fbm(Q * np.array([6.0, 6.0, 2.0]), 1.0, 3, seed=801)  # rauchiger Rand, unten unruhiger
        d -= 0.0018 * sdf.fbm(Q, 22, 2, seed=803)
        return d

    return [
        {'name': 'gestalt', 'fm': 'ent_silhouette', 'parts': P + fibers, 'disp': disp, 'h': 0.0040, 'cav_scale': 0.02},
    ]
