"""V13.3 Level 3: Skin-Stealer nach der Verwandlung (KREATUR_SkinStealer.md, Abschnitt "V13.3: Level 3"). Die wahre Gestalt
(skinstealer.py: blassgelb, Saugnoppen, eingesunkene weisse Augen) ist aus der getragenen Haut und dem Mantel gebrochen.
Die Seiten: die Noppen halten die Haut fest; in der Erzaehlung von Level 1 haengt die Haut lose, als die Gestalt sich
umdreht, und rutscht ab, als sie getroffen wird; das Blut ist durchsichtig (das sicherste Merkmal).

Hier (Auslegung): die Menschenhaut ist am Ruecken aufgeplatzt und bis zur Huefte heruntergerutscht - dort liegt sie als
faltiger Wulst, die leeren Arme der Haut haengen schlaff an den Seiten bis zu den Knien, mit platten Haenden. Das Gesicht des
Opfers hat sich geloest und haengt am Hals wie eine leere Maske auf der Brust (Augenloecher, Nase, Lippen, oben das kurze
Haar). Am Ruecken die aufgerollten Hautkanten entlang des Risses. Vom Mantel Fetzen, die am Wulst nach hinten haengen.
An den Rissen klare, nasse Tropfen.
"""
import math

import numpy as np

import anat as A
import faceling as F
import human as Hm
import sdf
import skinstealer as SS

LODS = (30000, 10000, 3400)
SKELETON = 'ent_skinstealer'


def skin_disp(Q):  # lose, faltige Menschenhaut
    return -0.0022 * sdf.fbm(Q * np.array([1.6, 1.6, 3.0]), 16, 3, seed=1111) - 0.0004 * sdf.fbm(Q, 120, 2, seed=1112)


def rag_disp(Q):
    return -0.004 * sdf.fbm(Q * np.array([2.5, 2.5, 1.0]), 2.0, 3, seed=1113) - 0.001 * sdf.fbm(Q, 36, 2, seed=1114)


def build(J, bones):
    rng = np.random.default_rng(1110)
    groups = SS.build(J, bones)
    B = Hm.Body(J, slim=1.12, muscle=1.35, head=0.98, neck=1.2, hand=1.4, chest=1.18, hips=1.0)
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    skin, hair, slime, rag = [], [], [], []
    # Wulst der heruntergerutschten Haut um die Huefte (vorn tiefer), Falten
    zw = B.hip + 0.13
    for k in range(3):
        z = zw + 0.035 * k - 0.02
        skin.append(A.tube(Hm.PELVIS, np.array([0.0, 0.0, z]), np.array([0.0, 0.0, z - 0.04]), (0.205 - 0.01 * k, 0.16 - 0.008 * k),
                           (0.215 - 0.01 * k, 0.165 - 0.008 * k), up=X, k=0.02))
    skin.append(A.tube(Hm.PELVIS, np.array([0.0, 0.0, zw + 0.12]), np.array([0.0, 0.0, zw - 0.1]), (0.17, 0.125), (0.18, 0.13), up=X, op='sub', k=0.01))
    # die leeren Arme der Haut: platt (ohne Inhalt), von den Seiten des Wulsts bis zum Knie, unten eine schlaffe Hand
    for s in (0, 1):
        g = Hm.side(s)
        p0 = np.array([-0.02, g * 0.2, zw])
        p1 = np.array([0.0, g * 0.235, zw - 0.2])
        p2 = np.array([0.03, g * 0.22, zw - 0.42])
        p3 = np.array([0.05, g * 0.205, zw - 0.55])
        n = A.unit(np.array([0.0, g, 0.0]))
        skin.append(A.tube(Hm.PELVIS, p0, p1, (0.062, 0.018), (0.052, 0.013), up=n, k=0.02))
        skin.append(A.tube(Hm.THIGH + 3 * s, p1, p2, (0.052, 0.013), (0.04, 0.01), up=n, k=0.015))
        skin.append(A.tube(Hm.THIGH + 3 * s, p2, p3, (0.045, 0.009), (0.05, 0.007), up=n, k=0.01))  # Hand (platt)
        for f in range(4):  # Finger, schlaff
            fb = p3 + X * (f - 1.5) * 0.017
            skin += A.chain(lambda i, s=s: Hm.THIGH + 3 * s, [fb, fb - Z * 0.04 + X * 0.003 * (f - 1.5), fb - Z * 0.075 + n * 0.006],
                            [0.006, 0.005, 0.004], k=0.003)
        skin += A.chain(lambda i, s=s: Hm.THIGH + 3 * s, [p3 + X * 0.03 + Z * 0.02, p3 + X * 0.05 - Z * 0.02], [0.007, 0.005], k=0.004)  # Daumen
    # aufgerollte Hautkanten entlang des Risses am Ruecken (Hals bis Wulst)
    for s in (0, 1):
        g = Hm.side(s)
        pts = [np.array([-0.07, g * 0.05, B.neckTop - 0.02]), np.array([-0.12, g * 0.045, B.shZ]), np.array([-0.135, g * 0.05, B.chestZ]),
               np.array([-0.12, g * 0.07, B.waist]), np.array([-0.1, g * 0.12, zw + 0.02])]
        bns = [Hm.NECK, Hm.CHEST, Hm.CHEST, Hm.SPINE, Hm.SPINE]
        skin += A.chain(lambda i, bns=bns: bns[i], pts, [0.009, 0.012, 0.013, 0.011, 0.012], k=0.006)
    # das Gesicht des Opfers: eine leere Maske, am Kinn noch mit dem Hals verbunden, die kopfueber auf der Brust haengt
    # (die Stirn unten, das Haar haengt nach unten) - gross genug, um sie zu erkennen
    mc = np.array([0.185, 0.0, B.shZ - 0.2])
    up = A.unit(np.array([0.12, 0.0, -1.0]))  # "oben" der Maske (Stirn) zeigt nach unten
    fw = A.unit(np.cross(up, Y))  # aus der Brust heraus
    raw = []  # rohe Unterseite der Haut an den Risskanten
    for k in range(18):  # Rand der Maske
        a = 2 * math.pi * k / 18
        q = mc - fw * 0.006 + Y * math.cos(a) * 0.088 + up * math.sin(a) * 0.118
        raw.append(A.ell(Hm.CHEST, q, fw, (0.007, 0.012, 0.012), up=up, k=0.004))
    skin.append(A.ell(Hm.CHEST, mc, fw, (0.016, 0.09, 0.12), up=up, k=0.0))
    skin.append(A.ell(Hm.CHEST, mc - fw * 0.014, fw, (0.014, 0.08, 0.108), up=up, op='sub', k=0.004))  # hohl, liegt auf
    for s in (0, 1):
        g = Hm.side(s)
        e = mc + Y * g * 0.036 + up * (0.03 - 0.01 * s)
        skin.append(A.ell(Hm.CHEST, e + fw * 0.012, fw, (0.004, 0.026, 0.008), up=up, k=0.006))  # Brauen
        skin.append(A.ell(Hm.CHEST, e, fw, (0.04, 0.016, 0.009 + 0.004 * s), up=up, op='sub', k=0.002))  # Augenloecher (verzogen)
        skin.append(A.ell(Hm.CHEST, e - up * 0.012 + fw * 0.004, fw, (0.006, 0.016, 0.004), up=up, k=0.003))  # Unterlid
    skin.append(A.cone(Hm.CHEST, mc + up * 0.012 + fw * 0.008, mc - up * 0.025 + fw * 0.03, 0.007, 0.015, k=0.008))  # Nase
    for s in (0, 1):
        g = Hm.side(s)
        skin.append(A.ell(Hm.CHEST, mc - up * 0.027 + fw * 0.02 + Y * g * 0.013, fw, (0.008, 0.008, 0.006), up=up, k=0.004))  # Nasenfluegel
    lip = mc - up * 0.062 + fw * 0.01
    skin.append(A.ell(Hm.CHEST, lip + up * 0.006, fw, (0.008, 0.03, 0.006), up=up, k=0.005))
    skin.append(A.ell(Hm.CHEST, lip - up * 0.007, fw, (0.009, 0.028, 0.0075), up=up, k=0.005))
    skin.append(A.ell(Hm.CHEST, lip + fw * 0.004, fw, (0.03, 0.022, 0.003), up=up, op='sub', k=0.002))  # Mundspalt
    for s in (0, 1):  # Ohren
        g = Hm.side(s)
        skin.append(A.ell(Hm.CHEST, mc + Y * g * 0.088 + up * 0.012, Y * g, (0.007, 0.018, 0.028), up=up, k=0.006))
    # Hautkragen des Halses, an dem die Maske haengt
    skin.append(A.tube(Hm.NECK, np.array([0.02, 0, B.shZ + 0.1]), np.array([0.01, 0, B.shZ - 0.0]), (0.085, 0.078), (0.095, 0.085), up=X, k=0.02))
    skin.append(A.tube(Hm.NECK, np.array([0.02, 0, B.shZ + 0.13]), np.array([0.01, 0, B.shZ - 0.03]), (0.07, 0.064), (0.08, 0.07), up=X, op='sub', k=0.004))
    skin.append(A.tube(Hm.CHEST, np.array([0.1, 0, B.shZ + 0.02]), mc - up * 0.11, (0.06, 0.012), (0.075, 0.013), up=fw, k=0.02))  # vom Hals zum Kinn
    for k in range(15):  # kurzes Haar an der Stirn (unten) - es haengt nach unten
        y = (k - 7) * 0.012
        b0 = mc + up * (0.112 - 0.0005 * (k - 7) ** 2) + Y * y + fw * 0.004
        hair += A.chain(lambda i: Hm.CHEST, [b0, b0 - Z * 0.03 + Y * y * 0.12 + fw * 0.006, b0 - Z * 0.05 + fw * 0.01], [0.006, 0.004, 0.0018], k=0.003)
    # rohe Kanten: oben am Hautwulst der Huefte, an den Rissen des Rueckens
    for k in range(24):
        a = 2 * math.pi * k / 24
        raw.append(A.ell(Hm.PELVIS, np.array([0.205 * math.cos(a), 0.215 * math.sin(a), zw + 0.045]), Z, (0.014, 0.016, 0.008), k=0.006))
    for s in (0, 1):
        g = Hm.side(s)
        for z in np.linspace(B.waist, B.shZ, 6):
            raw.append(A.ell(Hm.CHEST if z > B.chestZ - 0.1 else Hm.SPINE, np.array([-0.128, g * 0.062, z]), Z, (0.02, 0.008, 0.006), k=0.006))
    # Mantelfetzen, die hinten am Wulst haengen (eingeklemmt)
    for k in range(5):
        ang = math.pi + (k - 2) * 0.42 + (rng.random() - 0.5) * 0.15
        a0 = np.array([0.2 * math.cos(ang), 0.2 * math.sin(ang), zw + 0.01])
        ln = 0.28 + 0.2 * rng.random()
        out = A.unit(np.array([math.cos(ang), math.sin(ang), 0.0]))
        bn = Hm.PELVIS if abs(math.sin(ang)) < 0.4 else (Hm.THIGH if math.sin(ang) > 0 else Hm.THIGH + 3)
        rag.append(A.tube(bn, a0, a0 - Z * ln + out * 0.05, (0.06 + 0.02 * rng.random(), 0.006), (0.035, 0.005),
                          up=out, k=0.02))
        rag.append(A.ell(bn, a0 - Z * ln + out * 0.05, out, (0.01, 0.05, 0.03 + 0.02 * rng.random()), up=Z, op='sub', k=0.004))
    # klare Tropfen (durchsichtiges Blut): Rissende am Hals, Maske, Wulst
    starts = [(np.array([-0.07, 0.04, B.neckTop - 0.04]), Hm.NECK), (np.array([-0.07, -0.05, B.neckTop - 0.05]), Hm.NECK),
              (mc + up * 0.1 + fw * 0.01 + Y * 0.04, Hm.CHEST), (mc - up * 0.1 + fw * 0.008, Hm.CHEST),
              (np.array([0.19, 0.08, zw - 0.03]), Hm.PELVIS), (np.array([0.18, -0.1, zw - 0.03]), Hm.PELVIS)]
    for p0, bn in starts:
        ln = 0.04 + 0.05 * rng.random()
        slime += A.chain(lambda i, bn=bn: bn, [p0, p0 - Z * ln * 0.6 + X * 0.004, p0 - Z * ln], [0.0045, 0.0036, 0.002], k=0.003)
        slime.append(A.ell(bn, p0 - Z * ln, Z, (0.005, 0.005, 0.0065), k=0.002))
    groups += [
        {'name': 'menschenhaut', 'fm': 'ent_humanskin', 'parts': skin, 'disp': skin_disp, 'h': 0.0022, 'cav_scale': 0.008, 'lod_weight': 1.4},
        {'name': 'haar', 'fm': 'ent_hair', 'parts': hair, 'disp': F.hair_disp, 'h': 0.0025, 'cav_scale': 0.005},
        {'name': 'roh', 'fm': 'ent_mouth', 'parts': raw, 'disp': None, 'h': 0.002, 'cav_scale': 0.004},
        {'name': 'fetzen', 'fm': 'ent_cloth_b', 'parts': rag, 'disp': rag_disp, 'h': 0.004, 'cav_scale': 0.012},
        {'name': 'klar', 'fm': 'ent_wr_slime', 'parts': slime, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002},
    ]
    return groups
