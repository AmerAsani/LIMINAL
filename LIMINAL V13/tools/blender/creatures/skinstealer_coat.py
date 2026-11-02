"""V13.3 Level 3: Skin-Stealer im Mantel (KREATUR_SkinStealer.md, Abschnitt "V13.3: Level 3"). Die Seiten (Wikidot
level-3 Rev. 18, entity-10 Rev. 61): in Level 3 verdauen sie die getragene Haut langsamer - die Tarnung haelt Tage, aber
die Haut beginnt zu faulen; sie wissen das und verhuellen sich mit viel Kleidung (Maentel, Kapuzen, Masken, Sonnenbrillen).
Ein Ueberwachungsbild zeigt einen duennen, vermummten Mann.

Hier: der grosse, hagere Mann aus skinstealer_skin.py (2,05 m, gleiches Skelett) im langen, dunklen Mantel mit hochgeschlagenem
Kragen und Kapuze, ein grauer Schal ueber Mund und Nase (am Kiefer gebunden - er bewegt sich, wenn er spricht), eine
Sonnenbrille. Was von der Haut zu sehen ist - Wangenknochen und Schlaefen um die Brille, die Handruecken -, ist fleckig
grau-gruen und rissig (faulend). Taschen mit Klappen an der Huefte: dort stecken die Haende (Animation).
"""
import math

import numpy as np

import anat as A
import faceling as F
import human as Hm
import sdf

LODS = (30000, 10000, 3400)
SKELETON = 'ent_skinstealer'


def skin_disp(Q):
    return -0.0005 * sdf.fbm(Q, 70, 2, seed=1011) - 0.0003 * sdf.fbm(Q, 160, 2, seed=1012)


def coat_disp(Q):  # schwerer Stoff: lange Falten senkrecht, Knitter
    d = -0.005 * sdf.fbm(Q * np.array([3.5, 3.5, 0.8]), 1.4, 3, seed=1071)
    return d - 0.0012 * sdf.fbm(Q, 30, 2, seed=1072)


def rot_disp(Q):  # faulende Haut: Risse und Blasen
    d1, d2 = sdf.worley(Q, 90, seed=1081)
    return 0.0006 * np.clip(1.0 - d1 / 0.3, 0, 1) - 0.0009 * np.clip(0.12 - np.abs(d2 - d1), 0, 1) / 0.12


def build(J, bones):
    rng = np.random.default_rng(1090)
    B = Hm.Body(J, slim=1.08, muscle=1.2, head=0.98, neck=1.15, hand=1.25, chest=1.12, hips=1.0)
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    c, r = B.headC, B.headR
    skin = B.head(featureless=False) + B.neck() + B.hand(0, curl=0.45) + B.hand(1, curl=0.45)
    fskin, eyes, brows, mouth = B.face()
    skin += fskin
    for s in (0, 1):  # Handgelenk unter dem Aermel
        skin += B.arm(s, upper=False, fore=True, hand=False, sleeve_to=1.0)[-2:]
    # faulende Flecken: Wangenknochen, Schlaefen, Stirn unter der Kapuze, Handruecken, Handgelenke
    rot = []
    for s in (0, 1):
        g = Hm.side(s)
        for (fx, fy, fz, rr) in ((0.80, 0.62, -0.18, 0.022), (0.55, 0.88, 0.12, 0.018), (0.86, 0.25, 0.38, 0.016)):
            n = A.unit(np.array([fx * 0.9, g * fy, fz * 0.7]))
            p0 = c + np.array([r[0] * fx, g * r[1] * fy, r[2] * fz]) * 1.0
            rot.append(A.ell(Hm.HEAD, p0 + n * 0.0015, n, (0.0035, rr * (0.8 + 0.4 * rng.random()), rr), up=Z, k=0.004))
        W, H = B.j(Hm.jWR, s), B.j(Hm.jHAND, s)
        hd = A.unit(H - W)
        outn = A.unit(Y * g - hd * float((Y * g) @ hd))  # Handruecken
        for k in range(2):
            rot.append(A.ell(Hm.HAND + 4 * s, W + hd * (0.05 + 0.04 * k) + outn * 0.017, hd, (0.025, 0.018, 0.0035), up=outn, k=0.003))
        E = B.j(Hm.jEL, s)
        rot.append(A.ell(Hm.FORE + 4 * s, A.lerp(E, W, 0.97) + outn * 0.02, A.unit(W - E), (0.02, 0.02, 0.004), up=outn, k=0.003))
    # langer Mantel: Rumpf, Aermel mit weiten Stulpen, Kragen hoch, zwei Schoesse (je am Oberschenkel - sie gehen auseinander)
    tc = 0.02
    Bc = Hm.Body(J, slim=1.06, muscle=0.7, head=0.98, neck=1.15, hand=1.25, chest=1.08, hips=1.05)  # der Mantel faellt glatt
    coat = Bc.torso(t=tc, upper=True, lower=True) + Bc.arm(0, t=0.024, hand=False, sleeve_to=0.97) + Bc.arm(1, t=0.024, hand=False, sleeve_to=0.97)
    for s in (0, 1):
        E, W = B.j(Hm.jEL, s), B.j(Hm.jWR, s)
        coat.append(A.cone(Hm.FORE + 4 * s, A.lerp(E, W, 0.78), A.lerp(E, W, 0.98), 0.044, 0.05, k=0.02))  # Stulpe
        coat.append(A.cone(Hm.FORE + 4 * s, A.lerp(E, W, 0.9), A.lerp(E, W, 1.02), 0.042, 0.042, op='sub', k=0.004))  # offen
    # Schoss: ein weiter Rock bis eine Handbreit ueber das Knie (am Becken), vorn offen, hinten ein Gehschlitz
    zt, zb = B.hip + 0.04, B.j(Hm.jKNEE, 0)[2] + 0.09
    coat.append(A.tube(Hm.PELVIS, np.array([0.0, 0, B.waist]), np.array([0.01, 0, zb]), (B.hipW + 0.12, 0.13), (B.hipW + 0.16, 0.165), up=X, k=0.05))
    coat.append(A.tube(Hm.PELVIS, np.array([0.0, 0, B.hip - 0.08]), np.array([0.01, 0, zb - 0.03]), (B.hipW + 0.1, 0.11), (B.hipW + 0.145, 0.15), up=X, op='sub', k=0.0))
    coat.append(A.box(Hm.PELVIS, np.array([0.17, 0, zb + 0.08]), X, (0.05, 0.022, 0.14), 0.01, op='sub', k=0.01))  # vorn offen
    coat.append(A.box(Hm.PELVIS, np.array([-0.17, 0, zb + 0.06]), X, (0.05, 0.012, 0.12), 0.006, op='sub', k=0.006))  # Gehschlitz
    coat.append(A.cone(Hm.CHEST, np.array([-0.01, 0, B.shZ - 0.02]), np.array([0.0, 0, B.shZ + 0.12]), 0.098, 0.092, k=0.03))  # Kragen
    coat.append(A.cone(Hm.CHEST, np.array([0.0, 0, B.shZ - 0.03]), np.array([0.01, 0, B.shZ + 0.14]), 0.078, 0.075, op='sub', k=0.01))
    # Revers: zwei Klappen, die vom Kragen schraeg zur Knopfleiste laufen
    for s in (0, 1):
        g = Hm.side(s)
        a0 = np.array([0.075, g * 0.07, B.shZ + 0.02])
        a1 = np.array([0.125, g * 0.015, B.chestZ - 0.06])
        coat.append(A.tube(Hm.CHEST, a0, a1, (0.03, 0.007), (0.012, 0.006), up=A.unit(np.array([1.0, g * 0.3, 0.0])), k=0.008))
    # Taschenklappen (die Haende stecken darin)
    flaps = []
    for s in (0, 1):
        g = Hm.side(s)
        p0 = np.array([0.075, g * 0.165, B.hip + 0.07])
        n = A.unit(np.array([0.55, g * 0.83, 0.0]))
        flaps.append(A.box(Hm.PELVIS, p0 + n * 0.035, A.unit(np.cross(Z, n)), (0.075, 0.03, 0.006), 0.004, up=n, k=0.006))
    # Knoepfe
    buttons = []
    for k in range(4):
        z = B.chestZ + 0.12 - 0.13 * k
        buttons.append(A.ell(Hm.CHEST if z > B.chestZ - 0.08 else Hm.SPINE, np.array([0.105 * 1.08 * 1.12 + tc + 0.004, -0.025, z]), X, (0.004, 0.011, 0.011), k=0.0))
    # Kapuze: Schale um den Kopf, vorn weit offen (Brille und Schal schauen heraus), oben spitz nach hinten
    hood = [A.ell(Hm.HEAD, c + Z * r[2] * 0.15 - X * 0.012, X, (r[0] + 0.03, r[1] + 0.034, r[2] * 0.95 + 0.03), k=0.0),
            A.ell(Hm.HEAD, c - X * r[0] * 0.55 + Z * r[2] * 0.35, A.unit(-X + Z * 0.6), (0.07, r[1] * 0.75, 0.05), k=0.04),
            A.ell(Hm.NECK, c - Z * r[2] * 0.95 - X * 0.02, X, (r[0] * 0.95 + 0.025, r[1] + 0.045, 0.07), k=0.04)]
    hood.append(A.ell(Hm.HEAD, c + X * (r[0] * 0.55) + Z * r[2] * 0.62, A.unit(X - Z * 0.35), (r[0] * 0.75, r[1] * 0.95, 0.03), k=0.03))  # Schild ueber der Stirn
    hood.append(A.ell(Hm.HEAD, c + X * (r[0] * 1.25) - Z * r[2] * 0.2, X, (r[0] * 0.95, r[1] * 0.9, r[2] * 0.88), op='sub', k=0.02))
    hood.append(A.ell(Hm.HEAD, c - X * 0.01 + Z * r[2] * 0.1, X, (r[0] + 0.012, r[1] + 0.014, r[2] * 0.95 + 0.012), op='sub', k=0.0))
    # Schal ueber Mund und Nase (am Kiefer), Wicklung um den Hals (am Hals)
    scarf = [A.ell(Hm.JAW, c + X * r[0] * 0.62 - Z * r[2] * 0.45, X, (r[0] * 0.62, r[1] * 0.98, r[2] * 0.42), k=0.0),
             A.ell(Hm.HEAD, c + X * r[0] * 0.78 - Z * r[2] * 0.22, X, (r[0] * 0.5, r[1] * 0.8, r[2] * 0.2), k=0.02),
             A.cone(Hm.NECK, np.array([0.0, 0, B.shZ + 0.04]), np.array([0.01, 0, B.neckTop - 0.01]), 0.085, 0.08, k=0.02)]
    tail = [np.array([0.07, -0.05, B.shZ + 0.02]), np.array([0.11, -0.06, B.shZ - 0.1]), np.array([0.13, -0.065, B.shZ - 0.24])]
    scarf.append(A.tube(Hm.CHEST, tail[0], tail[1], (0.045, 0.01), (0.05, 0.009), up=X, k=0.02))
    scarf.append(A.tube(Hm.CHEST, tail[1], tail[2], (0.05, 0.009), (0.048, 0.008), up=X, k=0.0))
    # Sonnenbrille: zwei dunkle Glaeser, Steg und Buegel
    glass, frame = [], []
    for s in (0, 1):
        g = Hm.side(s)
        e = c + X * r[0] * 0.80 + Y * g * r[1] * 0.40 + Z * r[2] * 0.04
        lens = e + X * 0.03
        glass.append(A.ell(Hm.HEAD, lens, X, (0.0035, 0.024, 0.018), k=0.0))
        frame.append(A.ell(Hm.HEAD, lens - X * 0.0005, X, (0.0045, 0.0265, 0.0205), k=0.0))
        frame.append(A.ell(Hm.HEAD, lens - X * 0.0005, X, (0.006, 0.0235, 0.0175), op='sub', k=0.0))
        frame += A.chain(lambda i: Hm.HEAD, [lens + Y * g * 0.024, c + Y * g * (r[1] + 0.006) + Z * r[2] * 0.06 + X * r[0] * 0.3],
                         [0.0022, 0.002], k=0.001)
    frame += A.chain(lambda i: Hm.HEAD, [c + X * (r[0] * 0.8 + 0.03) + Y * r[1] * 0.18 + Z * r[2] * 0.08,
                                         c + X * (r[0] * 0.82 + 0.034) + Z * r[2] * 0.1,
                                         c + X * (r[0] * 0.8 + 0.03) - Y * r[1] * 0.18 + Z * r[2] * 0.08], [0.0022, 0.0024, 0.0022], k=0.001)
    pants = B.leg(0, t=0.015) + B.leg(1, t=0.015)
    boots = B.foot(0, t=0.016) + B.foot(1, t=0.016)
    for s in (0, 1):
        An = B.j(Hm.jANK, s)
        boots.append(A.cone(Hm.SHIN + 3 * s, An - Z * 0.01, An + Z * 0.14, 0.056, 0.054, k=0.02))
    return [
        {'name': 'haut', 'fm': 'ent_humanskin', 'parts': skin, 'disp': skin_disp, 'h': 0.0032, 'cav_scale': 0.01, 'lod_weight': 1.3},
        {'name': 'faeule', 'fm': 'ent_rotskin', 'parts': rot, 'disp': rot_disp, 'h': 0.0016, 'cav_scale': 0.004, 'lod_weight': 1.4},
        {'name': 'augen', 'fm': 'ent_tooth', 'parts': eyes, 'disp': None, 'h': 0.0012, 'cav_scale': 0.003, 'lod_weight': 0.5},
        {'name': 'mantel', 'fm': 'ent_cloth_b', 'parts': coat, 'disp': coat_disp, 'h': 0.005, 'cav_scale': 0.014},
        {'name': 'klappen', 'fm': 'ent_cloth_b', 'parts': flaps + buttons, 'disp': None, 'h': 0.0025, 'cav_scale': 0.005},
        {'name': 'kapuze', 'fm': 'ent_cloth_b', 'parts': hood, 'disp': coat_disp, 'h': 0.0038, 'cav_scale': 0.012},
        {'name': 'schal', 'fm': 'ent_cloth_a', 'parts': scarf, 'disp': F.cloth_disp(1093), 'h': 0.0032, 'cav_scale': 0.01},
        {'name': 'glas', 'fm': 'ent_pg_glass', 'parts': glass, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002, 'lod_weight': 1.6},
        {'name': 'brille', 'fm': 'ent_ob_rubber', 'parts': frame, 'disp': None, 'h': 0.001, 'cav_scale': 0.002, 'lod_weight': 1.4},
        {'name': 'hose', 'fm': 'ent_cloth_a', 'parts': pants, 'disp': F.cloth_disp(1094), 'h': 0.0048, 'cav_scale': 0.012},
        {'name': 'stiefel', 'fm': 'ent_ob_rubber', 'parts': boots, 'disp': None, 'h': 0.0038, 'cav_scale': 0.006},
    ]
