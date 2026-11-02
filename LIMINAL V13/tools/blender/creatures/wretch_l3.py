"""V13.3 Level 3: Wretch mit selbst gebauter Waffe (KREATUR_Wretch.md, Abschnitt "V13.3: Level 3"). Die Ebenenseite
(Wikidot level-3, Rev. 18): in Level 3 behalten Wretches ihren Koerperbau (kein Zerfliessen), das Fleisch verrottet, meist
tragen sie noch Kleidung (schwerer zu erkennen); sie rennen und benutzen Waffen. Das Autopsiebild der Seite zeigt schweren
Wundbrand. Welche Waffen, sagt keine Seite (Interpretation: was eine Elektrostation hergibt).

Hier: der Wretch aus wretch.py mit geschlossener rechter Faust, einer zerrissenen, blutverschmierten Arbeitsjacke ueber
den Hemdresten, langer, zerrissener Hose, nur noch einem Stiefel; Wundbrand an Fingern und Zehen. Die Waffe liegt in der
rechten Hand (an den Handknochen gebunden): Griffmitte 8,5 cm vor dem Handgelenk und 2 cm zur Handflaeche, die Waffe zeigt
zur Daumenseite (in der Ruhepose nach vorn, +x) - wie es anim/creature_kin_l3.cpp erwartet. Schlagseite (Bogen des Rohrs,
Naegel des Bretts): zu den Fingern (+Handrichtung), denn dorthin bewegt sich die Spitze im Schlag.
  pipe   Rohrstueck (1,04 m, 42 mm) mit gusseisernem Winkel als Kopf, Gewinde, Rost, Isolierband am Griff, Blut am Kopf
  board  Brett (0,9 m) mit sieben durchgeschlagenen, teils krummen Naegeln, gesplittertes Ende, Stoffstreifen am Griff
  blade  Klinge aus dem Blech einer Schaltschranktuer (gezackt geschnitten, eine Seite noch lackiert), Griff mit Kabel
         umwickelt, Kupferadern stehen heraus
"""
import math

import numpy as np

import anat as A
import human as Hm
import sdf
import wretch as Wr

HB = Hm.HAND + 4  # rechte Hand


def grip_frame(J):
    B = Hm.Body(J)
    W, H = B.j(Hm.jWR, 1), B.j(Hm.jHAND, 1)
    hd = A.unit(H - W)
    D = np.array([1.0, 0.0, 0.0])
    p = A.unit(np.cross(D, hd))  # Handflaeche
    return W + hd * 0.085 + p * 0.02, D, hd, p


def metal_disp(seed):
    def f(Q):
        return -0.0006 * sdf.fbm(Q, 90, 2, seed=seed) - 0.0004 * np.clip(sdf.ridged(Q, 40, 2, seed=seed + 1) - 0.6, 0, 1)
    return f


def rust_disp(Q):
    return 0.0012 * sdf.fbm(Q, 120, 3, seed=1531) - 0.0004


def tape_disp(Q):  # Windungen des Isolierbands
    return -0.0007 * np.abs(np.sin(Q[:, 0] * 260.0 + Q[:, 1] * 40.0))


def wood_disp(Q):  # Maserung laengs (x), Kerben
    g = np.sin(Q[:, 1] * 900.0 + 6.0 * sdf.fbm(Q * np.array([0.3, 1.0, 1.0]), 8, 2, seed=1541))
    return -0.0008 * (g * 0.5 + 0.5) - 0.0012 * np.clip(sdf.ridged(Q, 25, 2, seed=1542) - 0.7, 0, 1)


def pipe(G, D, hd, p, rng):
    steel, rust, tape, blood = [], [], [], []
    a, b = G - D * 0.12, G + D * 0.86
    steel.append(A.cone(HB, a, b, 0.0212, 0.0212))
    steel.append(A.cone(HB, a - D * 0.01, a + D * 0.04, 0.0155, 0.0155, op='sub'))  # offenes Ende
    # gusseiserner Winkel: Muffe, Bogen, Abgang zur Schlagseite mit Gewinde
    e0 = b - D * 0.015
    steel.append(A.cone(HB, e0, e0 + D * 0.055, 0.031, 0.031, k=0.004))
    bend = e0 + D * 0.07
    steel.append(A.ell(HB, bend, D, (0.034, 0.033, 0.033), up=hd, k=0.008))
    out1 = bend + hd * 0.065
    steel.append(A.cone(HB, bend, out1, 0.031, 0.031, k=0.006))
    for k in range(4):  # Gewinde am Abgang
        c = out1 + hd * (0.006 + 0.007 * k)
        steel.append(A.cone(HB, c - hd * 0.002, c + hd * 0.002, 0.0255, 0.0255, k=0.001))
    steel.append(A.cone(HB, out1 - hd * 0.01, out1 + hd * 0.04, 0.0235, 0.0235))
    steel.append(A.cone(HB, out1 - hd * 0.005, out1 + hd * 0.045, 0.018, 0.018, op='sub'))
    # Rost: als duenne Kruste am Winkel, am Gewinde und von dort in Laeufen das Rohr entlang (dunkle Flecken mitten auf dem
    # Rohr sahen im Spiel wie Loecher aus - Runde 2)
    for k in range(6):
        t = 0.82 + 0.16 * rng.random()
        ang = rng.random() * 2 * math.pi
        n = hd * math.cos(ang) + p * math.sin(ang)
        c = a + (b - a) * t + n * 0.0205
        rust.append(A.ell(HB, c, D, (0.05 + 0.04 * rng.random(), 0.016, 0.0028), up=n, k=0.004))
    for k in range(3):  # am abgesaegten Ende
        ang = rng.random() * 2 * math.pi
        n = hd * math.cos(ang) + p * math.sin(ang)
        rust.append(A.ell(HB, a + D * (0.01 + 0.02 * k) + n * 0.0205, D, (0.025, 0.018, 0.0028), up=n, k=0.004))
    rust.append(A.ell(HB, bend, D, (0.036, 0.035, 0.035), up=hd, k=0.006))
    rust.append(A.ell(HB, bend - hd * 0.02 + D * 0.01, D, (0.03, 0.03, 0.02), up=hd, op='sub', k=0.01))
    # Isolierband am Griff
    tape.append(A.cone(HB, G - D * 0.115, G + D * 0.085, 0.0238, 0.0238))
    # Blut am Kopf: Flecken und Laeufer zum Griff hin
    for k in range(6):
        ang = rng.random() * 2 * math.pi
        n = hd * math.cos(ang) + p * math.sin(ang)
        c = bend + n * 0.03 + D * (rng.random() - 0.5) * 0.05
        blood.append(A.ell(HB, c, D, (0.02 + 0.015 * rng.random(), 0.014, 0.004), up=n, k=0.003))
    for k in range(3):
        n = hd * 0.9 + p * (rng.random() - 0.5)
        c0 = b - D * (0.02 + 0.05 * k) + A.unit(n) * 0.0215
        blood += A.chain(lambda i: HB, [c0, c0 - D * (0.05 + 0.06 * rng.random())], [0.004, 0.002], k=0.002)
    return [
        {'name': 'rohr', 'fm': 'ent_ob_steel', 'parts': steel, 'disp': metal_disp(1532), 'h': 0.0016, 'cav_scale': 0.004, 'lod_weight': 1.4},
        {'name': 'rost', 'fm': 'ent_ob_rust', 'parts': rust, 'disp': rust_disp, 'h': 0.0016, 'cav_scale': 0.003, 'cav_extra': steel},
        {'name': 'band', 'fm': 'ent_ob_rubber', 'parts': tape, 'disp': tape_disp, 'h': 0.0014, 'cav_scale': 0.003},
        {'name': 'blut', 'fm': 'ent_ooze', 'parts': blood, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002},
    ]


def board(G, D, hd, p, rng):
    wood, nails, rag, blood = [], [], [], []
    # Brett: Dicke zur Schlagseite (hd), Breite quer (p); am Griff schmaler geschnitzt
    w = A.unit(np.cross(hd, D))  # Breite
    c = G + D * 0.40
    wood.append(A.box(HB, c, D, (0.42, 0.052, 0.0125), 0.003, up=hd, k=0.0))
    wood.append(A.box(HB, G - D * 0.02, D, (0.12, 0.024, 0.015), 0.006, up=hd, k=0.03))  # Griff
    for s in (-1, 1):  # Kerben, wo der Griff geschnitzt ist
        wood.append(A.ell(HB, G + D * 0.11 + w * s * 0.05, D, (0.08, 0.026, 0.03), up=hd, op='sub', k=0.02))
    # gesplittertes Ende: Zacken abziehen
    tip = c + D * 0.42
    for k in range(5):
        y = (k - 2) * 0.022
        wood.append(A.ell(HB, tip + w * y + D * (0.01 + 0.03 * rng.random()), D, (0.03 + 0.03 * rng.random(), 0.008, 0.03), up=hd, op='sub', k=0.002))
    # Naegel: durchgeschlagen, Spitzen ragen zur Schlagseite heraus (manche krumm), Koepfe auf der Rueckseite
    for k in range(7):
        t = 0.52 + 0.3 * rng.random()
        y = (rng.random() - 0.5) * 0.08
        base = G + D * t + w * y
        head = base - hd * 0.0135
        nails.append(A.cone(HB, head - hd * 0.001, head + hd * 0.001, 0.006, 0.006))
        bendDir = A.unit(D * (rng.random() - 0.5) + w * (rng.random() - 0.5))
        ln = 0.045 + 0.025 * rng.random()
        mid = base + hd * (0.0125 + ln * 0.55)
        end = mid + A.unit(hd + bendDir * (0.9 if k in (1, 5) else 0.15)) * ln * 0.45
        nails += A.chain(lambda i: HB, [head, mid, end], [0.0024, 0.0022, 0.0009], k=0.0005)
        if k % 2 == 0:
            blood.append(A.ell(HB, mid + hd * 0.006, hd, (0.008, 0.006, 0.006), up=D, k=0.002))
    # Blut und dunkle Flecken auf der Schlagseite
    for k in range(5):
        cc = G + D * (0.45 + 0.35 * rng.random()) + w * (rng.random() - 0.5) * 0.08 + hd * 0.0125
        blood.append(A.ell(HB, cc, D, (0.03 + 0.03 * rng.random(), 0.02, 0.0018), up=hd, k=0.002))
    # Stoffstreifen um den Griff
    for k in range(5):
        cc = G + D * (-0.1 + 0.04 * k)
        rag.append(A.box(HB, cc, D, (0.014, 0.03, 0.021), 0.006, up=hd, k=0.004))
    rag.append(A.chain(lambda i: HB, [G - D * 0.1 - hd * 0.02, G - D * 0.16 - hd * 0.06], [0.006, 0.003], k=0.003)[0])  # loses Ende
    return [
        {'name': 'brett', 'fm': 'ent_ob_wood', 'parts': wood, 'disp': wood_disp, 'h': 0.0018, 'cav_scale': 0.004, 'lod_weight': 1.3},
        {'name': 'naegel', 'fm': 'ent_ob_rust', 'parts': nails, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002, 'lod_weight': 1.6},
        {'name': 'lappen', 'fm': 'ent_cloth_a', 'parts': rag, 'disp': None, 'h': 0.0016, 'cav_scale': 0.004},
        {'name': 'blut', 'fm': 'ent_ooze', 'parts': blood, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002},
    ]


def blade(G, D, hd, p, rng):
    steel, paint, cable, copper = [], [], [], []
    # Klinge: flach (Breite zur Schlagseite hd, Dicke quer), spitz zulaufend, eine Kante gezackt
    w = A.unit(np.cross(hd, D))  # Dicke
    b0, b1 = G + D * 0.06, G + D * 0.47
    steel.append(A.tube(HB, b0, b1, (0.034, 0.0022), (0.004, 0.0016), up=w))
    steel.append(A.tube(HB, G - D * 0.09, b0 + D * 0.01, (0.016, 0.003), (0.03, 0.0024), up=w))  # Angel
    for k in range(7):  # gezackte Schneide (Blech mit der Zange geschnitten)
        t = 0.1 + 0.11 * k
        cc = A.lerp(b0, b1, t) - hd * (0.034 * (1.0 - t * 0.85) + 0.004)
        steel.append(A.ell(HB, cc, D, (0.008 + 0.006 * rng.random(), 0.01, 0.006), up=w, op='sub', k=0.0015))
    # Lack der Schranktuer auf einer Seite (ab der Mitte abgeplatzt)
    paint.append(A.tube(HB, b0 + D * 0.02 + w * 0.0012, A.lerp(b0, b1, 0.7) + w * 0.0012, (0.031, 0.0013), (0.012, 0.0011), up=w))
    for k in range(4):
        cc = A.lerp(b0, b1, 0.2 + 0.18 * k) + w * 0.0019 + hd * (rng.random() - 0.5) * 0.02
        paint.append(A.ell(HB, cc, D, (0.012 + 0.01 * rng.random(), 0.01, 0.002), up=w, op='sub', k=0.002))
    # Kabel um den Griff: Wendel aus Kegeln
    turns, n = 5.5, 44
    pts = []
    for i in range(n + 1):
        t = i / n
        ang = t * turns * 2 * math.pi
        pts.append(G - D * 0.10 + D * 0.17 * t + (hd * math.cos(ang) + w * math.sin(ang)) * 0.019)
    cable += A.chain(lambda i: HB, pts, [0.0058] * len(pts), k=0.002)
    cable.append(A.tube(HB, G - D * 0.1, G + D * 0.075, (0.016, 0.006), (0.018, 0.006), up=w))  # Kern unter der Wicklung
    tail = [pts[0], pts[0] - D * 0.04 - hd * 0.02, pts[0] - D * 0.07 - hd * 0.05]
    cable += A.chain(lambda i: HB, tail, [0.0058, 0.0055, 0.005], k=0.002)
    for k in range(4):  # Kupferadern am Ende des Kabels
        d = A.unit(-D * 0.6 - hd * 0.8 + w * (k - 1.5) * 0.4)
        copper += A.chain(lambda i: HB, [tail[-1], tail[-1] + d * (0.02 + 0.01 * k)], [0.0021, 0.0017], k=0.0)
    return [
        {'name': 'klinge', 'fm': 'ent_ob_steel', 'parts': steel, 'disp': metal_disp(1552), 'h': 0.0009, 'cav_scale': 0.003, 'lod_weight': 1.5},
        {'name': 'lack', 'fm': 'ent_ob_paint', 'parts': paint, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002},
        {'name': 'kabel', 'fm': 'ent_ob_rubber', 'parts': cable, 'disp': None, 'h': 0.0012, 'cav_scale': 0.003},
        {'name': 'kupfer', 'fm': 'ent_wire', 'parts': copper, 'disp': None, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 1.8},
    ]


def cloth_disp(seed):
    def f(Q):
        return -0.004 * sdf.fbm(Q * np.array([2.5, 2.5, 1.2]), 1.6, 3, seed=seed) - 0.0012 * sdf.fbm(Q, 38, 2, seed=seed + 1)
    return f


def build_armed(J, bones, weapon):
    rng = np.random.default_rng({'pipe': 151, 'board': 152, 'blade': 153}[weapon])
    groups = Wr.build(J, bones, fist=True)
    B = Hm.Body(J, slim=0.78, muscle=0.6, belly=0.72, chest=0.92, hips=0.85, hand=1.05)
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    groups = [g for g in groups if g['name'] != 'hose']  # statt der Fetzen: lange, zerrissene Hose
    # der Saft der Haut nur, wo sie frei liegt (Brust in der offenen Jacke, Hals, Mund) - sonst stechen die Laeufer
    # durch die Kleidung
    for g in groups:
        if g['name'] == 'saft':
            keep = []
            for q in g['parts']:
                c = (q.lo + q.hi) * 0.5
                if c[2] > B.shZ - 0.02 or (c[0] > 0.05 and abs(c[1]) < 0.06 and c[2] > B.hip + 0.12):
                    keep.append(q)
            g['parts'] = keep
    # Arbeitsjacke (offen, zerrissen, links ab dem Unterarm, rechts am Ellbogen abgerissen), Kragen
    tj = 0.017
    jacket = B.torso(t=tj, upper=True, lower=True)[:-3] + B.arm(0, t=0.016, hand=False, sleeve_to=0.35) + \
        B.arm(1, t=0.016, hand=False, sleeve_to=0.08)
    for s, f in ((0, 0.35), (1, 0.08)):  # ausgefranster Rand der Aermel
        E, W = B.j(Hm.jEL, s), B.j(Hm.jWR, s)
        for k in range(4):
            ang = k * 1.57 + rng.random()
            n = A.unit(np.array([math.cos(ang), 0.0, math.sin(ang)]))
            jacket.append(A.ell(Hm.FORE + 4 * s, A.lerp(E, W, f) + n * 0.035, A.unit(W - E), (0.03 + 0.02 * rng.random(), 0.02, 0.02), up=n, op='sub', k=0.004))
    jacket.append(A.cone(Hm.CHEST, np.array([-0.03, 0, B.shZ + 0.0]), np.array([-0.01, 0, B.shZ + 0.06]), 0.085, 0.075, k=0.02))
    jacket.append(A.box(Hm.CHEST, np.array([0.13, 0, B.chestZ - 0.02]), X, (0.07, 0.055, 0.32), 0.02, op='sub', k=0.02))  # offen
    jacket.append(A.box(Hm.PELVIS, np.array([0, 0, B.hip - 0.12]), X, (0.4, 0.4, 0.1), 0.02, op='sub', k=0.02))  # Saum
    for k in range(10):  # Risse und Loecher
        z = B.hip + 0.05 + (B.shZ - B.hip) * rng.random()
        side = 1 if rng.random() < 0.5 else -1
        pp = np.array([(rng.random() - 0.3) * 0.2, side * (0.06 + 0.08 * rng.random()), z])
        bone = Hm.CHEST if z > B.chestZ - 0.1 else Hm.SPINE
        jacket.append(A.ell(bone, pp, A.unit(np.array([rng.random() - 0.5, 0.0, 1.0])), (0.05 + 0.03 * rng.random(), 0.06, 0.018), up=X, op='sub', k=0.006))
    # lange Hose, Knie aufgerissen; ein Stiefel (rechts), der linke Fuss ist nackt
    pants = B.torso(t=0.016, upper=False, lower=True)[1:] + B.leg(0, t=0.016, to=0.92) + B.leg(1, t=0.016, to=0.92)
    for s in (0, 1):
        K = B.j(Hm.jKNEE, s)
        pants.append(A.ell(Hm.THIGH + 3 * s, K + X * 0.06, X, (0.05, 0.05, 0.06), op='sub', k=0.01))
        pants.append(A.ell(Hm.SHIN + 3 * s, B.j(Hm.jANK, s) + Z * 0.06 + X * 0.03, X, (0.05, 0.045, 0.04), op='sub', k=0.01))
    boot = B.foot(1, t=0.014)
    boot.append(A.cone(Hm.SHIN + 3, B.j(Hm.jANK, 1) - Z * 0.01, B.j(Hm.jANK, 1) + Z * 0.12, 0.05, 0.048, k=0.02))
    # Wundbrand: die letzten Fingerglieder der linken Hand (Geometrie wie human.hand, curl 0,6, claw 0,15) und die Zehen
    rot = []
    W, H = B.j(Hm.jWR, 0), B.j(Hm.jHAND, 0)
    hd0 = A.unit(H - W)
    L = np.linalg.norm(H - W) * B.o['hand']
    inward = A.unit(-Y - hd0 * float((-Y) @ hd0))
    across = A.unit(np.cross(inward, hd0))
    for f in range(4):
        base = W + hd0 * L * 0.48 + across * (f - 1.5) * 0.017 * B.o['hand']
        fl = L * (0.48 - abs(f - 1.3) * 0.05) * 1.15
        p1 = base + hd0 * fl * 0.42 + inward * fl * 0.08 * 0.6
        p2 = p1 + A.unit(hd0 + inward * 0.6 * 0.9) * fl * 0.32
        p3 = p2 + A.unit(hd0 + inward * 0.6 * 1.8) * fl * 0.26
        rr = 0.0085 * B.o['hand']
        rot += A.chain(lambda i: Hm.HAND, [A.lerp(p1, p2, 0.6), p2, p3], [rr * 0.95 + 0.0012, rr * 0.86 + 0.0012, rr * 0.35 + 0.0012], k=0.002)
    An, T0 = B.j(Hm.jANK, 0), B.j(Hm.jTOE, 0)
    fd = A.unit(np.array([T0[0] - An[0], T0[1] - An[1], 0.0]))
    ball = np.array([T0[0] - 0.02, T0[1], 0.03])
    rot.append(A.ell(Hm.FOOT, ball + fd * 0.05, fd, (0.032, 0.05, 0.0275), k=0.004))
    # Blut verschmiert: Laeufer und Wischspuren auf der Jacke (auf ihrer Oberflaeche), eine Hand voll auf dem rechten Aermel
    blood = []
    rx, ry, rz = 0.105 * 0.78 * 0.92 + tj, 0.150 * 0.78 + tj, 0.175 * B.sz + tj
    for k in range(8):
        th = (0.35 + 0.5 * rng.random()) * (1 if rng.random() < 0.45 else -1)
        z0 = B.chestZ + (rng.random() - 0.3) * 0.2
        pts = []
        for i in range(4):
            z = z0 - i * (0.025 + 0.02 * rng.random())
            f = math.sqrt(max(0.05, 1.0 - ((z - B.chestZ) / rz) ** 2))
            t2 = th + (rng.random() - 0.5) * 0.08
            pts.append(np.array([rx * f * math.cos(t2) - 0.002, ry * f * math.sin(t2) - 0.002 * math.sin(t2), z]))
        blood += A.chain(lambda i: Hm.CHEST, pts, [0.0075, 0.006, 0.0045, 0.002], k=0.003)
    E1, W1 = B.j(Hm.jEL, 1), B.j(Hm.jWR, 1)
    for k in range(3):
        c = A.lerp(B.j(Hm.jSH, 1), E1, 0.4 + 0.2 * k) + X * 0.035
        blood.append(A.ell(Hm.UPPER + 4, c, A.unit(E1 - B.j(Hm.jSH, 1)), (0.03, 0.022, 0.006), up=X, k=0.003))
    G, D, hd, p = grip_frame(J)
    wg = {'pipe': pipe, 'board': board, 'blade': blade}[weapon](G, D, hd, p, rng)
    groups += [
        {'name': 'jacke', 'fm': 'ent_cloth_d', 'parts': jacket, 'disp': cloth_disp(1561), 'h': 0.0045, 'cav_scale': 0.012},
        {'name': 'hose', 'fm': 'ent_cloth_b', 'parts': pants, 'disp': cloth_disp(1563), 'h': 0.0045, 'cav_scale': 0.012},
        {'name': 'stiefel', 'fm': 'ent_ob_rubber', 'parts': boot, 'disp': None, 'h': 0.0035, 'cav_scale': 0.006},
        {'name': 'brand', 'fm': 'ent_darkskin', 'parts': rot, 'disp': None, 'h': 0.002, 'cav_scale': 0.004},
        {'name': 'blutjacke', 'fm': 'ent_ooze', 'parts': blood, 'disp': None, 'h': 0.002, 'cav_scale': 0.004},
    ] + wg
    return groups
