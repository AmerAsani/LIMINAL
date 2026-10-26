"""V13.2: Death Rat (Entity 24, Wikidot Rev. 25, als "in Ueberarbeitung" markiert) - KREATUR_DeathRat.md. Eine Ratte mit
langem Koerper und schwarzem Fell; auf dem Kopf zwei braune Hoerner mit rot gluehenden Spitzen. Langer, nackter Schwanz,
rosa-graue Pfoten, kleine dunkle Augen, Schnurrhaare. Variante rat_bald: haarlos (Level 2, an die Hitze angepasst).
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (9000, 3200, 1100)
BALD = False
SCALE = 0.62  # entworfen in Spielgroesse / 0,62 (Skelett: kRatScale in src/anim/creature.cpp)
# Knochen (rt): 0 Wurzel, 1 Becken, 2 Ruecken, 3 Hals, 4 Kopf, 5 Kiefer, 6 + 3 * Bein + i, 18 + i Schwanz
PELVIS, SPINE, NECK, HEAD, JAW = 1, 2, 3, 4, 5
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def build_rat(J, bald):
    rng = np.random.default_rng(24)
    J = np.asarray(J)
    body = [A.ell(PELVIS, (-0.13, 0, 0.105), X, (0.11, 0.065, 0.06), k=0.0),
            A.ell(SPINE, (0.0, 0, 0.115), X, (0.12, 0.06, 0.058), k=0.05),
            A.ell(SPINE, (0.1, 0, 0.11), X, (0.08, 0.05, 0.05), k=0.04),
            A.ell(NECK, (0.18, 0, 0.11), X, (0.05, 0.04, 0.042), k=0.03),
            A.ell(HEAD, (0.25, 0, 0.115), X, (0.055, 0.036, 0.038), k=0.025),
            A.cone(HEAD, (0.26, 0, 0.11), (0.345, 0, 0.096), 0.03, 0.011, k=0.02),       # Schnauze
            A.ell(JAW, (0.29, 0, 0.083), X, (0.035, 0.016, 0.01), k=0.01)]
    # Beine: Oberarm/-schenkel als Muskel am Rumpf, Unterbein, Pfote mit Zehen
    paws = []
    for l in range(4):
        T, K, P, Tt = (J[8 + l * 4 + i] for i in range(4))
        b0 = 6 + l * 3
        body.append(A.cone(b0, T + Z * 0.01, K, 0.022, 0.011, k=0.02))
        body.append(A.cone(b0 + 1, K, P, 0.01, 0.007, k=0.01))
        fd = A.unit(np.array([Tt[0] - P[0], Tt[1] - P[1], 0.0]))
        paws.append(A.ell(b0 + 2, P + fd * 0.012 + Z * 0.002, fd, (0.018, 0.011, 0.005), k=0.006))
        for t in range(4):
            a = (t - 1.5) * 0.35
            d = A.unit(fd * math.cos(a) + np.cross(Z, fd) * math.sin(a))
            paws += A.chain(lambda i: b0 + 2, [P + fd * 0.018 + Z * 0.002, P + fd * 0.018 + d * 0.014], [0.003, 0.0018], k=0.003)
    # Schwanz: nackt, geringelt (Verschiebung), duenn auslaufend
    tail = A.chain(lambda i: 18 + min(i, 3), [J[24 + i] for i in range(5)], [0.013, 0.01, 0.007, 0.0045, 0.0018], k=0.008)
    tail = [A.cone(PELVIS, (-0.2, 0, 0.1), J[24], 0.02, 0.013, k=0.02)] + tail
    # Ohren, Augen
    ears = []
    eyes = []
    for s in (1, -1):
        ears.append(A.ell(HEAD, (0.22, s * 0.035, 0.155), A.unit(np.array([-0.3, s * 0.6, 1.0])), (0.02, 0.016, 0.004), up=X, k=0.008))
        body.append(A.ell(HEAD, (0.275, s * 0.03, 0.128), X, (0.009, 0.008, 0.007), op='sub', k=0.004))
        eyes.append(A.ell(HEAD, (0.271, s * 0.028, 0.128), X, (0.0075, 0.0075, 0.0065), k=0.0))
    # Hoerner: braun, leicht nach hinten gebogen; die Spitzen gluehen rot
    horns, tips = [], []
    for s in (1, -1):
        a = np.array([0.245, s * 0.02, 0.145])
        b = a + np.array([-0.01, s * 0.012, 0.035])
        c = b + np.array([-0.025, s * 0.006, 0.022])
        horns += A.chain(lambda i: HEAD, [a, b, c], [0.0075, 0.005, 0.0032], k=0.004)
        tips += A.chain(lambda i: HEAD, [c, c + np.array([-0.012, s * 0.002, 0.008])], [0.0034, 0.0006], k=0.0)
    # Schnurrhaare
    whisk = []
    for s in (1, -1):
        for k in range(4):
            p0 = np.array([0.33, s * 0.012, 0.1 - 0.003 * k])
            d = A.unit(np.array([0.2, s * 1.0, 0.1 - 0.12 * k]))
            whisk += A.chain(lambda i: HEAD, [p0, p0 + d * 0.035, p0 + d * 0.06 - Z * 0.008], [0.0007, 0.0005, 0.0002], k=0.0)

    def fur_disp(Q):
        if bald:  # Falten und Runzeln
            return -0.0012 * sdf.ridged(Q * np.array([1.0, 1.0, 0.5]), 90, 2, seed=2401) - 0.0006 * sdf.fbm(Q, 150, 2, seed=2402)
        streak = sdf.ridged(Q * np.array([0.35, 1.0, 1.0]), 160, 2, seed=2403)  # Haarstraehnen laengs
        return -0.0016 * (streak - 0.5) - 0.0012 * sdf.fbm(Q, 50, 2, seed=2404)

    def tail_disp(Q):
        ring = 0.5 + 0.5 * np.sin(Q[:, 0] * 520.0)
        return -0.0005 * ring

    return [
        {'name': 'fell', 'fm': 'ent_ratskin' if bald else 'ent_ratfur', 'parts': body, 'disp': fur_disp, 'h': 0.0018, 'cav_scale': 0.006},
        {'name': 'pfoten', 'fm': 'ent_ratskin', 'parts': paws, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002},
        {'name': 'schwanz', 'fm': 'ent_ratskin', 'parts': tail + ears, 'disp': tail_disp, 'h': 0.001, 'cav_scale': 0.002},
        {'name': 'augen', 'fm': 'ent_motheye', 'parts': eyes, 'disp': None, 'h': 0.0008, 'cav_scale': 0.002, 'lod_weight': 2.0},
        {'name': 'hoerner', 'fm': 'ent_horn', 'parts': horns, 'disp': None, 'h': 0.001, 'cav_scale': 0.002, 'lod_weight': 1.5},
        {'name': 'spitzen', 'fm': 'ent_hornglow', 'parts': tips, 'disp': None, 'h': 0.0008, 'cav_scale': 0.002},
        {'name': 'schnurrhaare', 'fm': 'ent_hair', 'parts': whisk, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001},
    ]


def build(J, bones):
    return build_rat(J, BALD)
