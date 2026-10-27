"""V13.3: Wrangler-Jungtiere (Entity 75, KREATUR_Wrangler.md). Die Seite: das Weibchen legt seine samenartigen Eier in Boden oder
Waende; daraus waechst ein Ast mit vielen Jungen, die die Mutter fuettert. Hier: aus einem geplatzten Samen in der Wand waechst
ein faseriger, fleischiger Ast; an ihm haengen sechs Junge (je 0,72 m, durchscheinend, mit dunklem Darm, zwei leuchtenden
Augenpunkten und einem kleinen, zu menschlichen Mund).

Gleiches Skelett wie das Maennchen (ent_wrangler): das Junge y nutzt die Ringknochen 2y und 2y+1, der Ast den Kranz 0 (B_COL0).
Kranz 0 liegt in der Ruhepose flach bei (-12, 0, 0): lokal x = Wandnormale (Ruhepose +z), y = seitlich (Ruhepose -y),
z = oben (Ruhepose +x). Die Aufhaengepunkte der Jungen entsprechen anim/creature_wrangler.cpp (hashF).
"""
import math

import numpy as np

import anat as A
import sdf
import wrangler as W

SKELETON = 'ent_wrangler'
LODS = (14000, 5000, 1800)
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])
C0 = np.array([-12.0, 0.0, 0.0])


def hashF(i):
    h = (i * 2654435761 + 0x9E3779B9) & 0xFFFFFFFF
    h ^= h >> 15
    h = (h * 2246822519) & 0xFFFFFFFF
    h ^= h >> 13
    return (h & 0xFFFFFF) / 16777216.0


def wall(lx, ly, lz):
    """Ortskoordinaten am Ast (x aus der Wand, y seitlich, z oben) -> Ruhepose."""
    return C0 + Z * lx - Y * ly + X * lz


def young_root(y):
    h, g = hashF(y * 7 + 3), hashF(y * 13 + 5)
    return 0.12 + 0.1 * g, (y - 2.5) * 0.16, 0.25 * h - 0.05


def build(J, bones):
    rng = np.random.default_rng(7575)
    b = W.B_COL0
    # Samen, aus dem der Ast waechst (geplatzt, in der Wand), dazu Risse in der Wand
    seed = [A.ell(b, wall(0.02, -0.05, 0.02), Z, (0.09, 0.16, 0.12), up=X, k=0.0),
            A.ell(b, wall(0.08, -0.05, 0.02), Z, (0.06, 0.1, 0.08), up=X, op='sub', k=0.02)]
    cracks = []
    for i in range(9):
        a = rng.uniform(0, 2 * math.pi)
        p = np.array([0.003, -0.05 + 0.15 * math.cos(a), 0.02 + 0.12 * math.sin(a)])
        for j in range(4):
            a += rng.normal(0, 0.25)
            q = p + np.array([0.0, math.cos(a), math.sin(a)]) * rng.uniform(0.06, 0.12)
            pa, qa = wall(*p), wall(*q)
            cracks.append(A.box(b, 0.5 * (pa + qa), A.unit(qa - pa), (np.linalg.norm(qa - pa) * 0.5 + 0.004, 0.006 * (1 - j / 4) + 0.002, 0.004),
                                0.0, up=Z))
            p = q
    # Ast: faserig, fleischig; quer an der Wand, leicht durchhaengend, Knoten an den Aufhaengepunkten
    ys = np.linspace(-0.52, 0.5, 14)
    pts = [wall(0.12 + 0.03 * math.sin(v * 5.0), v, 0.06 - 0.06 * v * v + 0.02 * math.sin(v * 9.0)) for v in ys]
    rad = [0.035 + 0.025 * math.exp(-((v + 0.05) / 0.25) ** 2) for v in ys]
    branch = A.chain(lambda i: b, pts, rad, k=0.02)
    branch.append(A.cone(b, wall(0.02, -0.05, 0.02), wall(0.12, -0.05, 0.05), 0.07, 0.055, k=0.04))  # aus dem Samen heraus
    nod = []
    for y in range(6):
        lx, ly, lz = young_root(y)
        branch.append(A.cone(b, wall(0.12, ly, 0.06 - 0.06 * ly * ly), wall(lx, ly, lz), 0.03, 0.022, k=0.02))
        nod.append(A.ell(b, wall(lx, ly, lz), Z, (0.035, 0.04, 0.04), up=X, k=0.02))
    # die Jungen: in der Ruhepose entlang x (Knochen 2y: x0 + 0,19 .. 0,55; Knochen 2y+1: 0,55 .. 0,91)
    young, gut, eyes, mouth = [], [], [], []
    for y in range(6):
        x0 = W.X0 + 2 * y * W.RL
        ba, bb = W.B_RING0 + 2 * y, W.B_RING0 + 2 * y + 1
        xs = np.linspace(x0 + 0.19, x0 + 0.91, 13)
        r = [0.022 + 0.04 * math.sin(math.pi * min(1.0, (t + 0.08) / 1.0)) ** 0.7 for t in np.linspace(0, 1, 13)]
        for i in range(12):
            bone = ba if xs[i + 1] <= x0 + 0.55 + 1e-6 else bb
            young.append(A.cone(bone, (xs[i], 0, W.AZ), (xs[i + 1], 0, W.AZ), r[i], r[i + 1], k=0.012 if i else 0.0))
        for i in range(5):  # Ringfurchen
            young.append(W.Part_torus(ba if i < 2 else bb, (x0 + 0.3 + 0.13 * i, 0, W.AZ), X, r[3 + 2 * i] + 0.004, 0.005, op='sub', k=0.004))
        gut.append(A.cone(ba, (x0 + 0.24, 0, W.AZ - 0.005), (x0 + 0.55, 0, W.AZ - 0.01), 0.012, 0.016, k=0.0))
        gut.append(A.cone(bb, (x0 + 0.55, 0, W.AZ - 0.01), (x0 + 0.84, 0, W.AZ), 0.016, 0.01, k=0.01))
        tip = x0 + 0.91
        for s in (1, -1):
            eyes.append(A.ell(bb, (tip - 0.035, s * 0.02, W.AZ + 0.018), X, (0.007, 0.007, 0.007)))
        mouth.append(A.ell(bb, (tip - 0.012, 0, W.AZ - 0.008), X, (0.006, 0.018, 0.004)))
    return [
        {'name': 'samen', 'fm': 'ent_wr_seed', 'parts': seed, 'disp': lambda P: -0.004 * sdf.fbm(P, 40, 2, seed=7701), 'h': 0.004},
        {'name': 'risse', 'fm': 'ent_wr_crack', 'parts': cracks, 'disp': None, 'h': 0.003},
        {'name': 'ast', 'fm': 'ent_wr_pith', 'parts': branch, 'disp': lambda P: -0.004 * sdf.ridged(P * np.array([0.4, 1.0, 1.0]), 60, 2, seed=7702),
         'h': 0.004, 'cav_scale': 0.008},
        {'name': 'knoten', 'fm': 'ent_wr_gut', 'parts': nod, 'disp': lambda P: -0.003 * sdf.fbm(P, 50, 2, seed=7703), 'h': 0.004},
        {'name': 'junge', 'fm': 'ent_wr_membrane_f', 'parts': young, 'disp': None, 'h': 0.003, 'cav_scale': 0.006, 'lod_weight': 1.5},
        {'name': 'jungdarm', 'fm': 'ent_wr_gut', 'parts': gut, 'disp': None, 'h': 0.003},
        {'name': 'jungaugen', 'fm': 'ent_glow_eye', 'parts': eyes, 'disp': None, 'h': 0.0015, 'lod_weight': 0.4},
        {'name': 'jungmaul', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.0015, 'lod_weight': 0.4},
    ]
