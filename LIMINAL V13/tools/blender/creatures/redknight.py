"""V13.3 Level 3: der Red Knight (Entity 71) - KREATUR_RedKnight.md. Wikidot entity-71 (Rev. 44, CC BY-SA 3.0), eigene Worte:
ein Ritter in Plattenruestung, wie sie im spaeten 16. Jahrhundert getragen wurde, mittelgross und von gewoehnlichem Bau; ein
Zweihaender, zu gross fuer die Huefte - er traegt ihn auf der Schulter; manchmal ein Schild (hier nicht); ein roter Umhang mit
Goldsaum, am Ende zerfetzt; alles gut erhalten, aber alt, die Ruestung verbeult. Durch das Visier sieht man nur Dunkelheit.
Level 3 (Rev. 18, Vorfall GB-03): die Ruestung schwer verbeult, mit Spuren der Purpifizierung. Aus Einstichen sickert in der
Erzaehlung der Seite eine Fluessigkeit von unbeschreiblicher Farbe (hier: dunkel, oelig - Interpretation).

Aufbau (Skelett des Wretch, Variante 4; anim/creature_knight.cpp):
  Helm       geschlossener Helm mit Kamm, vorspringendem Visier ("Spatzenschnabel") mit Sehschlitz und Atemloechern, Kinnreff;
             hinter Schlitz und Loechern nur Schwaerze; Halsberge in Lamellen
  Rumpf      Brustharnisch mit Mittelgrat und tiefem Bauch (Gaensebauch), Rueckenplatte, Bauchreifen, Beintaschen
  Arme       geschobene Schulterstuecke (vier Lamellen), Oberarm- und Unterarmroehren, Ellbogenkacheln mit Muschel, Stulpen-
             handschuhe mit Fingerlamellen
  Beine      Diechlinge, Kniebuckel mit Muschel, Beinroehren, breite "Baerentatzen"-Schuhe
  darunter   Kettengeflecht (Achsel, Armbeuge, Kniekehle, Schurz unter den Bauchreifen)
  Umhang     rote Wolle mit Goldsaum, von den Schultern bis zur Wade, unten in Fetzen, Loecher; an Brust, Ruecken und
             Oberschenkel gebunden (er schlaegt im Lauf mit den Beinen)
  Schwert    Zweihaender (1,6 m) an der rechten Hand: Knauf, langer Ledergriff, Parierstange mit eingerollten Enden,
             Fehlschaerfe mit Parierhaken, Klinge mit Hohlkehle, Scharten
  Schaden    tiefe Dellen (Brust, Helm, Schulter, Bein, Ruecken), Kratzer, zwei Einstiche mit oeliger Fluessigkeit;
             Purpurspuren an den Dellen: Glasur und gluehende Adern
"""
import math

import numpy as np

import anat as A
import human as Hm
import sdf
import wretch_l3 as WL

LODS = (42000, 14000, 5600)
SKELETON = 'ent_wretch'
HB = Hm.HAND + 4  # rechte Hand


def plate_disp(Q):  # gehaemmert, leicht wellig, feine Kratzer
    return -0.0005 * sdf.fbm(Q, 9, 2, seed=7101) - 0.00025 * np.clip(sdf.ridged(Q * np.array([1.0, 1.0, 4.0]), 60, 2, seed=7102) - 0.7, 0, 1)


def mail_disp(Q):  # Ringe: dichte kleine Mulden
    d1, d2 = sdf.worley(Q, 110, seed=7103)
    return -0.0012 * np.clip(1.0 - d1 / 0.45, 0, 1)


def cape_disp(Q):  # Wolle, Falten, Verschleiss
    return -0.003 * sdf.fbm(Q * np.array([2.0, 2.0, 0.6]), 2.2, 3, seed=7104) - 0.0008 * sdf.fbm(Q, 45, 2, seed=7105)


def blade_disp(Q):
    return -0.0003 * np.clip(sdf.ridged(Q, 80, 2, seed=7106) - 0.65, 0, 1)


def build(J, bones):
    rng = np.random.default_rng(7171)
    B = Hm.Body(J, slim=1.0, muscle=1.05, chest=1.08, belly=1.0, hips=1.0, hand=1.05)
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    c, r = B.headC, B.headR
    dark, mail, cape, gold, blade, leather, purple, glow, void, oil, rivet = ([] for _ in range(11))
    # Platten in eigenen Netzen (sonst verschmelzen sie weich wie ein Koerper): Helm, Rumpf, Schultern, Arme, Beine, Schwertstahl
    helm, brust, schulter, arme, beine, knauf = ([] for _ in range(6))

    # --- darunter: Kettengeflecht ueber dem ganzen Koerper (sichtbar nur in den Luecken der Platten) -----------------
    mail += B.torso(t=0.014) + B.neck(t=0.012)
    for s in (0, 1):
        mail += B.arm(s, t=0.012, hand=False) + B.leg(s, t=0.012)
    # Kettenschurz unter den Bauchreifen bis zur Mitte der Oberschenkel
    mail.append(A.cone(Hm.PELVIS, (0.0, 0, B.hip + 0.06), (0.0, 0, B.hip - 0.17), 0.165, 0.18, k=0.02))
    mail.append(A.cone(Hm.PELVIS, (0.0, 0, B.hip + 0.07), (0.0, 0, B.hip - 0.19), 0.13, 0.145, op='sub', k=0.01))
    for k in range(10):  # ausgefranster Saum (Ringe fehlen)
        a_ = k * 0.628 + rng.random() * 0.3
        mail.append(A.ell(Hm.PELVIS, (0.18 * math.cos(a_), 0.18 * math.sin(a_), B.hip - 0.17), Z, (0.03, 0.02 + 0.015 * rng.random(), 0.03), op='sub', k=0.004))

    # --- Helm -------------------------------------------------------------------------------------------------------
    plate = helm
    hc = c + Z * r[2] * 0.08
    plate.append(A.ell(Hm.HEAD, hc, X, (r[0] + 0.03, r[1] + 0.03, r[2] * 0.9 + 0.035), k=0.0))
    plate.append(A.ell(Hm.HEAD, hc - X * r[0] * 0.5 - Z * r[2] * 0.45, X, (r[0] * 0.75 + 0.02, r[1] + 0.028, r[2] * 0.6), k=0.03))  # Nacken
    plate.append(A.ell(Hm.HEAD, hc + Z * (r[2] * 0.9 + 0.03), X, (r[0] + 0.035, 0.007, 0.028), k=0.01))  # Kamm
    # Visier: Stirnteil mit Sehschlitz, darunter der vorspringende Schnabel mit Atemloechern
    vis = hc + X * (r[0] + 0.012) - Z * r[2] * 0.12
    plate.append(A.box(Hm.HEAD, vis + Z * 0.035 - X * 0.01, X, (0.04, r[1] + 0.012, 0.04), 0.012, k=0.01))
    beak0, beak1 = vis - Z * 0.035, vis + X * 0.06 - Z * 0.04
    for sgn in (-1.0, 1.0):  # zwei Wangenflaechen, die sich vorn in einem scharfen Grat treffen
        mid = A.lerp(beak0, beak1, 0.45) + Y * sgn * 0.03
        plate.append(A.box(Hm.HEAD, mid - X * 0.01, A.unit(X - Y * sgn * 0.75), (0.05, 0.004, 0.042), 0.003, up=Z, k=0.012))
    plate.append(A.cone(Hm.HEAD, beak0 - X * 0.04, beak1, 0.03, 0.006, k=0.012))
    for sgn in (-1.0, 1.0):  # Drehpunkt des Visiers mit Niet
        rivet.append(A.ell(Hm.HEAD, hc + Y * sgn * (r[1] + 0.032) - Z * 0.01, Y * sgn, (0.009, 0.009, 0.009)))
    plate.append(A.ell(Hm.HEAD, hc + X * r[0] * 0.55 - Z * r[2] * 0.8, X, (r[0] * 0.7, r[1] + 0.02, 0.05), k=0.03))  # Kinnreff
    slit = vis + X * 0.035 + Z * 0.018
    plate.append(A.box(Hm.HEAD, slit, X, (0.03, 0.062, 0.0055), 0.002, op='sub', k=0.002))
    plate.append(A.box(Hm.HEAD, slit + Z * 0.0, X, (0.03, 0.004, 0.012), 0.002, op='sub', k=0.002))  # Mittelsteg-Kerbe
    void.append(A.box(Hm.HEAD, slit - X * 0.012, X, (0.012, 0.06, 0.0052), 0.001))
    for i in range(3):  # Atemloecher auf der rechten Schnabelseite
        for j in range(2):
            hpnt = A.lerp(beak0, beak1, 0.25 + 0.17 * i) - Y * (0.03 - 0.008 * i) - Z * (0.006 * j - 0.004)
            plate.append(A.cone(Hm.HEAD, hpnt + Y * 0.012, hpnt - Y * 0.03, 0.0035, 0.0035, op='sub'))
            void.append(A.ell(Hm.HEAD, hpnt + Y * 0.006, Y, (0.004, 0.0033, 0.0033)))
    # Halsberge in drei Lamellen
    plate = brust
    for k in range(3):
        z = B.shZ + 0.1 - 0.035 * k
        plate.append(A.cone(Hm.NECK if k == 0 else Hm.CHEST, (0.005, 0, z + 0.03), (0.0, 0, z - 0.012), 0.075 + 0.012 * k, 0.088 + 0.012 * k, k=0.004))

    # --- Rumpf --------------------------------------------------------------------------------------------------------
    plate = brust
    # Runde 3: eigener Harnisch statt der Koerperform (Brustmuskeln und Schulterblaetter drueckten durch - es las sich wie
    # eine Gummipuppe): ein glatter Brustpanzer, eine Rueckenplatte, das Schulterjoch; Grate, gerollte Kanten
    plate.append(A.ell(Hm.CHEST, (0.012, 0, B.chestZ + 0.01), X, (0.152, 0.186, 0.2), k=0.0))
    plate.append(A.ell(Hm.CHEST, (-0.012, 0, B.shZ - 0.045), Y, (B.shW * 0.98, 0.135, 0.075), up=Z, k=0.03))
    plate.append(A.ell(Hm.SPINE, (0.02, 0, B.waist + 0.03), X, (0.128, 0.158, 0.12), k=0.04))
    for s_ in (0, 1):  # gerollte Kanten an den Armloechern
        g_ = Hm.side(s_)
        Sx = B.j(Hm.jSH, s_)
        pts = [Sx + np.array([0.11 * math.cos(t_), -g_ * 0.075 + g_ * 0.0, -0.1 * math.sin(t_)]) * np.array([1, 1, 1]) for t_ in np.linspace(-0.3, 3.4, 9)]
        pts = [np.array([q[0], g_ * (B.shW - 0.045), q[2] - 0.02]) for q in pts]
        plate += A.chain(lambda i: Hm.CHEST, pts, [0.009] * len(pts), k=0.004)
    neck = [np.array([0.07 * math.cos(t_) + 0.01, 0.085 * math.sin(t_), B.shZ + 0.04]) for t_ in np.linspace(0, 2 * math.pi, 13)]
    plate += A.chain(lambda i: Hm.CHEST, neck, [0.009] * len(neck), k=0.004)  # gerollter Halsrand
    plate.append(A.ell(Hm.CHEST, (0.145, 0, B.chestZ - 0.02), (0.15, 0, 1), (0.05, 0.007, 0.19), up=X, k=0.02))  # Mittelgrat (Runde 3: kraeftiger)
    plate.append(A.ell(Hm.SPINE, (0.085, 0, B.waist + 0.04), X, (0.075, 0.13, 0.1), k=0.05))  # Gaensebauch
    # Bauchreifen (Lamellen) und Beintaschen
    for k in range(3):
        z = B.waist - 0.03 - 0.045 * k
        plate.append(A.cone(Hm.SPINE if k == 0 else Hm.PELVIS, (0.01, 0, z + 0.03), (0.01, 0, z - 0.015), 0.14 + 0.02 * k, 0.16 + 0.02 * k, k=0.004))
    for s in (0, 1):
        g = Hm.side(s)
        for k in range(4):  # Runde 2: vier Lamellen bis zur Mitte des Oberschenkels, breiter (der Kettenschurz las sich wie Unterwaesche)
            z = B.hip - 0.04 - 0.055 * k
            plate.append(A.box(Hm.THIGH + 3 * s, (0.1 + 0.006 * k, g * 0.095, z), Z, (0.042, 0.115 - 0.008 * k, 0.011), 0.006, up=A.unit(X + Z * 0.22), k=0.003))
            plate.append(A.box(Hm.THIGH + 3 * s, (0.02, g * (0.165 + 0.004 * k), z), Z, (0.042, 0.06, 0.01), 0.006, up=A.unit(Y * g + Z * 0.2), k=0.003))
        for k in range(3):  # Nieten an den Beintaschen
            rivet.append(A.ell(Hm.THIGH + 3 * s, (0.09, g * (0.04 + 0.04 * k), B.hip - 0.035), X, (0.006, 0.006, 0.006)))

    # --- Arme -----------------------------------------------------------------------------------------------------------
    for s in (0, 1):
        g = Hm.side(s)
        S, E, W, H = B.j(Hm.jSH, s), B.j(Hm.jEL, s), B.j(Hm.jWR, s), B.j(Hm.jHAND, s)
        du = A.unit(E - S)
        # Schulterstueck: vier Lamellen, die obere die groesste
        plate = schulter
        for k in range(4):
            cc = S + Y * g * (0.02 + 0.012 * k) + Z * (0.045 - 0.045 * k) - du * 0.0
            plate.append(A.ell(Hm.UPPER + 4 * s if k else Hm.CLAV + 4 * s, cc, du, (0.125 - 0.01 * k, 0.13 - 0.012 * k, 0.1 - 0.008 * k), up=Y * g, k=0.004))
            rivet.append(A.ell(Hm.UPPER + 4 * s if k else Hm.CLAV + 4 * s, cc + Y * g * (0.08 - 0.006 * k) + Z * 0.02, Y * g, (0.006, 0.006, 0.006)))
        schulter.append(A.box(Hm.CLAV + 4 * s, S + Y * g * -0.06 + Z * 0.13, Y * g, (0.007, 0.075, 0.045), 0.004, up=Z, k=0.006))  # Brechrand
        plate = arme
        plate += B.arm(s, t=0.024, upper=True, fore=False, hand=False)
        plate += B.arm(s, t=0.02, upper=False, fore=True, hand=False, sleeve_to=0.86)
        # gerollte Kanten der Roehren (Wulst an den Enden)
        dfw = A.unit(W - E)
        arme.append(A.cone(Hm.UPPER + 4 * s, E - du * 0.06, E - du * 0.045, 0.064, 0.064, k=0.002))
        arme.append(A.cone(Hm.FORE + 4 * s, E + dfw * 0.06, E + dfw * 0.075, 0.06, 0.06, k=0.002))
        arme.append(A.cone(Hm.FORE + 4 * s, A.lerp(E, W, 0.8), A.lerp(E, W, 0.8) + dfw * 0.013, 0.052, 0.052, k=0.002))
        for f in (0.3, 0.55):  # Runde 3: Lamellenkanten (die Roehren lasen sich wie glatte Schlaeuche)
            q = A.lerp(S, E, f)
            arme.append(A.cone(Hm.UPPER + 4 * s, q, q + du * 0.01, 0.067 - 0.009 * f + 0.004, 0.067 - 0.009 * f + 0.002, k=0.001))
        # Ellbogenkachel mit Muschel (aussen)
        plate.append(A.ell(Hm.UPPER + 4 * s, E - X * 0.03, du, (0.055, 0.05, 0.045), up=-X, k=0.01))
        plate.append(A.ell(Hm.UPPER + 4 * s, E - X * 0.02 + Y * g * 0.045, Y * g, (0.012, 0.055, 0.06), up=-X, k=0.004))
        rivet.append(A.ell(Hm.UPPER + 4 * s, E - X * 0.075, -X, (0.007, 0.007, 0.007)))
        # Stulpenhandschuh: Stulpe, Handruecken, Finger als Lamellen (Faust)
        dw = A.unit(W - E)
        plate.append(A.cone(Hm.FORE + 4 * s, W - dw * 0.07, W + dw * 0.015, 0.045, 0.062, k=0.004))
        plate.append(A.cone(Hm.FORE + 4 * s, W - dw * 0.07, W + dw * 0.02, 0.036, 0.05, op='sub'))
        plate += B.hand(s, t=0.007, curl=1.15, claw=0.0)
        leather.append(A.cone(Hm.FORE + 4 * s, W - dw * 0.04, W - dw * 0.025, 0.043, 0.043, k=0.002))  # Riemen

    # --- Beine ----------------------------------------------------------------------------------------------------------
    plate = beine
    for s in (0, 1):
        g = Hm.side(s)
        K, An = B.j(Hm.jKNEE, s), B.j(Hm.jANK, s)
        plate += B.leg(s, t=0.024, thigh=True, shin=False)
        plate += B.leg(s, t=0.02, thigh=False, shin=True, to=0.95)
        plate.append(A.ell(Hm.THIGH + 3 * s, K + X * 0.06 + Z * 0.01, X, (0.035, 0.06, 0.065), k=0.01))  # Kniebuckel
        dsn = A.unit(An - K)
        Hh = B.j(Hm.jHIP, s)
        dth = A.unit(K - Hh)
        for f in (0.35, 0.62):  # Lamellenkanten am Diechling
            q = A.lerp(Hh, K, f)
            plate.append(A.cone(Hm.THIGH + 3 * s, q, q + dth * 0.012, 0.102 - 0.028 * f + 0.004, 0.102 - 0.028 * f + 0.002, k=0.001))
        plate.append(A.cone(Hm.THIGH + 3 * s, K - dsn * 0.1, K - dsn * 0.085, 0.077, 0.077, k=0.002))  # Wulst Diechling
        plate.append(A.cone(Hm.SHIN + 3 * s, K + dsn * 0.08, K + dsn * 0.095, 0.072, 0.072, k=0.002))  # Wulst Beinroehre
        plate.append(A.cone(Hm.SHIN + 3 * s, An + Z * 0.05, An + Z * 0.065, 0.053, 0.053, k=0.002))
        plate.append(A.ell(Hm.THIGH + 3 * s, K + X * 0.04 + Y * g * 0.06, Y * g, (0.012, 0.055, 0.06), up=X, k=0.004))  # Muschel
        rivet.append(A.ell(Hm.THIGH + 3 * s, K + X * 0.096 + Z * 0.01, X, (0.007, 0.007, 0.007)))
        # Schuhe: breite Baerentatzen, Lamellen ueber dem Rist
        sab = B.foot(s, t=0.018)
        plate += sab
        T0 = B.j(Hm.jTOE, s)
        plate.append(A.box(Hm.FOOT + 3 * s, (T0[0] - 0.01, T0[1], 0.035), X, (0.05, 0.068, 0.03), 0.02, k=0.02))
        for k in range(3):
            plate.append(A.ell(Hm.FOOT + 3 * s, (An[0] + 0.04 + 0.035 * k, An[1], 0.08 - 0.012 * k), X, (0.03, 0.06, 0.02), k=0.004))

    # --- Schaden: Dellen, Kratzer, Einstiche ------------------------------------------------------------------------
    def owner(bone):
        if bone == Hm.HEAD:
            return helm
        if bone in (Hm.CLAV, Hm.CLAV + 4):
            return schulter
        if bone in (Hm.CHEST, Hm.SPINE, Hm.PELVIS, Hm.NECK):
            return brust
        return beine
    dents = [  # (Knochen, Mitte, Richtung in die Platte, Radien)
        (Hm.CHEST, np.array([0.135, 0.075, B.chestZ + 0.03]), -X + Y * 0.3, (0.025, 0.06, 0.045)),
        (Hm.CHEST, np.array([0.11, -0.1, B.chestZ - 0.09]), -X - Y * 0.4, (0.02, 0.04, 0.035)),
        (Hm.HEAD, hc + Y * -(r[1] + 0.03) + Z * 0.03 + X * 0.01, Y, (0.02, 0.045, 0.04)),
        (Hm.CLAV + 4, B.j(Hm.jSH, 1) - Y * 0.1 + Z * 0.06, Y - Z * 0.5, (0.02, 0.05, 0.04)),
        (Hm.SHIN, A.lerp(B.j(Hm.jKNEE, 0), B.j(Hm.jANK, 0), 0.35) + X * 0.06, -X, (0.018, 0.035, 0.05)),
        (Hm.CHEST, np.array([-0.15, 0.06, B.chestZ - 0.04]), X, (0.022, 0.05, 0.05)),
        (Hm.THIGH + 3, A.lerp(B.j(Hm.jHIP, 1), B.j(Hm.jKNEE, 1), 0.45) + X * 0.08 - Y * 0.02, -X, (0.016, 0.03, 0.04)),
    ]
    for bone, p, into, rr in dents:
        into = A.unit(into)
        owner(bone).append(A.ell(bone, p + into * rr[0] * 0.2, into, rr, k=0.008, op='sub'))
        # Purpurspur in der Delle: Glasur, darin gluehende Adern (Interpretation: die Energie der Purpifizierung)
        purple.append(A.ell(bone, p + into * rr[0] * 0.95, into, (0.003, rr[1] * 0.85, rr[2] * 0.85), k=0.0))
        ang0 = rng.random() * math.pi
        for v in range(3):
            a = ang0 + v * 2.1
            side = A.unit(np.cross(into, Z if abs(into[2]) < 0.9 else X))
            up2 = A.unit(np.cross(side, into))
            dvec = side * math.cos(a) + up2 * math.sin(a)
            pts = [p + into * rr[0] * 0.75 + dvec * (rr[1] * t) + up2 * 0.006 * math.sin(t * 7.0) for t in (0.0, 0.45, 0.95, 1.5)]
            glow += A.chain(lambda i, b=bone: b, pts, [0.0026, 0.0022, 0.0017, 0.0009], k=0.001)
    for k in range(14):  # Kratzer
        z = B.hip + (B.shZ - B.hip) * rng.random()
        th = (rng.random() - 0.5) * 1.6
        p = np.array([0.12 * math.cos(th) * 1.05, 0.16 * math.sin(th), z])
        ax = A.unit(np.array([0.0, 0.3 + rng.random(), (rng.random() - 0.5) * 1.2]))
        brust.append(A.box(Hm.CHEST if z > B.chestZ - 0.1 else Hm.SPINE, p, ax, (0.03 + 0.04 * rng.random(), 0.0012, 0.004), 0.0008, up=A.unit(p * np.array([1, 1, 0])), op='sub'))
    # zwei Einstiche (Brust links unten, Ruecken rechts) - darin Dunkel, darunter laeuft oelige Fluessigkeit
    for bone, p, into in ((Hm.SPINE, np.array([0.115, 0.07, B.waist + 0.06]), -X), (Hm.CHEST, np.array([-0.135, -0.07, B.chestZ - 0.08]), X)):
        brust.append(A.ell(bone, p, into, (0.03, 0.009, 0.006), up=Z, op='sub', k=0.002))
        dark.append(A.ell(bone, p + into * 0.012, into, (0.012, 0.008, 0.005), up=Z))
        out = -into
        pts = [p + out * 0.002 - Z * 0.004, p + out * 0.006 - Z * 0.05, p + out * 0.006 - Z * 0.11, p + out * 0.004 - Z * 0.16]
        oil += A.chain(lambda i, b=bone: b, pts, [0.0055, 0.0048, 0.004, 0.0025], k=0.002)
        oil.append(A.ell(bone, pts[-1] - Z * 0.004, Z, (0.0045, 0.0045, 0.006)))
    # Russ und Schwaerze um zwei Dellen (verbrannt)
    dark.append(A.ell(Hm.CHEST, np.array([0.14, 0.09, B.chestZ + 0.06]), -X, (0.002, 0.035, 0.02)))

    # --- Umhang -------------------------------------------------------------------------------------------------------
    top, hem = B.shZ + 0.03, 0.36
    nst, nrow = 11, 12

    def cape_pt(i, j):
        u, v = i / (nst - 1) - 0.5, j / (nrow - 1)
        z = top + (hem - top) * v
        fold = 0.02 * math.cos(i * 2.3 + 0.6) * min(1.0, v * 2.5)
        wy = (0.2 + 0.16 * v) * u * 2.0                      # wird nach unten breiter
        back = 0.168 + 0.075 * v + fold                       # haengt frei hinter Ruecken und Beinen
        back += 0.02 * (2.0 * abs(u)) ** 2 * v                # die Raender fallen weiter nach hinten
        return np.array([-back, wy, z])

    def cape_bone(p):
        z, wy = p[2], p[1]
        if z > B.chestZ - 0.1:
            return Hm.CHEST
        if z > B.hip - 0.02:
            return Hm.SPINE if z > B.waist else Hm.PELVIS
        return Hm.THIGH if wy > 0.06 else (Hm.THIGH + 3 if wy < -0.06 else Hm.PELVIS)

    for i in range(nst):
        for j in range(nrow - 1):
            pa, pb = cape_pt(i, j), cape_pt(i, j + 1)
            v = (j + 0.5) / (nrow - 1)
            hw = (0.4 + 0.32 * v) / (nst - 1) * 0.68
            seg = pb - pa
            cape.append(A.box(cape_bone(0.5 * (pa + pb)), 0.5 * (pa + pb), A.unit(seg), (np.linalg.norm(seg) * 0.68, hw, 0.0045), 0.002, up=-X, k=0.006))
    # Kragen: ueber die Schultern nach vorn gelegt, von einer Spange gehalten
    for s in (0, 1):
        g = Hm.side(s)
        cape.append(A.ell(Hm.CHEST, np.array([-0.04, g * 0.15, top - 0.005]), X, (0.11, 0.06, 0.012), up=Z, k=0.03))
        gold.append(A.ell(Hm.CHEST, np.array([0.02, g * 0.12, top + 0.012]), Z, (0.012, 0.016, 0.016), k=0.0))  # Spange
    # Fetzen unten, Loecher
    for k in range(16):
        y = (k / 15 - 0.5) * 0.72 + (rng.random() - 0.5) * 0.04
        depth = 0.03 + 0.16 * rng.random() ** 1.5
        cape.append(A.ell(Hm.THIGH if y > 0.06 else (Hm.THIGH + 3 if y < -0.06 else Hm.PELVIS), np.array([-0.24, y, hem + depth * 0.5]), Z, (depth, 0.012 + 0.018 * rng.random(), 0.08), up=-X, op='sub', k=0.004))
    for k in range(5):
        z = hem + 0.15 + 0.45 * rng.random()
        y = (rng.random() - 0.5) * 0.4
        bone = Hm.PELVIS if z < B.hip else Hm.SPINE
        cape.append(A.ell(bone, np.array([-0.21 - 0.05 * (1.0 - (z - hem) / (top - hem)), y, z]), Z, (0.02 + 0.025 * rng.random(), 0.012 + 0.012 * rng.random(), 0.08), up=-X, op='sub', k=0.004))
    # Goldsaum entlang der Seiten (am Rand des Umhangs, bis knapp ueber die Fetzen)
    for i in (0, nst - 1):
        g = 1.0 if i == nst - 1 else -1.0
        pts = []
        for j in range(nrow - 2):
            q = cape_pt(i, j)
            pts.append(q + np.array([-0.001, g * ((0.4 + 0.32 * j / (nrow - 1)) / (nst - 1) * 0.72), 0.0]))
        for a_, b_ in zip(pts[:-1], pts[1:]):
            gold.append(A.tube(cape_bone(0.5 * (a_ + b_)), a_, b_, (0.0065, 0.0062), up=-X, k=0.002))
    # Goldsaum oben am Kragen
    for i in range(nst - 1):
        a_, b_ = cape_pt(i, 0) + Z * 0.004, cape_pt(i + 1, 0) + Z * 0.004
        gold.append(A.tube(Hm.CHEST, a_, b_, (0.0065, 0.0062), up=-X, k=0.002))

    # --- Schwert (rechte Hand) ----------------------------------------------------------------------------------------
    plate = knauf
    G, D, hd, p = WL.grip_frame(J)
    w = A.unit(np.cross(hd, D))  # Dicke der Klinge
    leather.append(A.tube(HB, G - D * 0.31, G + D * 0.05, (0.017, 0.016), up=w, k=0.002))
    for k in range(8):  # Wicklung
        leather.append(A.tube(HB, G - D * (0.29 - 0.042 * k), G - D * (0.283 - 0.042 * k), (0.0185, 0.0175), up=w, k=0.001))
    plate.append(A.ell(HB, G - D * 0.355, D, (0.04, 0.032, 0.032), up=w, k=0.0))  # Knauf (Birne)
    plate.append(A.cone(HB, G - D * 0.39, G - D * 0.32, 0.012, 0.022, k=0.004))
    # Parierstange mit eingerollten Enden
    cg = G + D * 0.075
    plate.append(A.tube(HB, cg - hd * 0.22, cg + hd * 0.22, (0.011, 0.011), up=w, k=0.004))
    plate.append(A.ell(HB, cg, D, (0.02, 0.03, 0.016), up=w, k=0.006))
    for sgn in (-1.0, 1.0):
        e0 = cg + hd * sgn * 0.22
        pts = [e0, e0 + hd * sgn * 0.03 + D * 0.025, e0 + hd * sgn * 0.02 + D * 0.055, e0 - hd * sgn * 0.005 + D * 0.05]
        plate += A.chain(lambda i: HB, pts, [0.011, 0.01, 0.009, 0.008], k=0.004)
    # Fehlschaerfe (stumpf, schmaler) mit Parierhaken
    r0 = G + D * 0.09
    blade.append(A.tube(HB, r0, r0 + D * 0.2, (0.022, 0.006), (0.022, 0.0055), up=w, k=0.002))
    for sgn in (-1.0, 1.0):
        hk = r0 + D * 0.2
        blade += A.chain(lambda i: HB, [hk, hk + hd * sgn * 0.05 - D * 0.005, hk + hd * sgn * 0.065 - D * 0.03], [0.007, 0.006, 0.004], k=0.003)
    # Klinge mit Hohlkehle, zur Spitze schmaler, Scharten
    b0, b1 = r0 + D * 0.2, G + D * 1.22
    blade.append(A.tube(HB, b0, b1, (0.027, 0.0055), (0.003, 0.0016), up=w, k=0.002))
    for sgn in (-1.0, 1.0):
        blade.append(A.tube(HB, b0 + D * 0.02 + w * sgn * 0.0052, b0 + D * 0.55 + w * sgn * 0.0045, (0.0055, 0.0018), (0.004, 0.0015), up=w, op='sub', k=0.001))
    for k in range(6):
        t = 0.1 + 0.8 * rng.random()
        e = A.lerp(b0, b1, t)
        width = 0.027 + (0.003 - 0.027) * t
        sgn = 1.0 if rng.random() < 0.5 else -1.0
        blade.append(A.ell(HB, e + hd * sgn * width, D, (0.004 + 0.004 * rng.random(), 0.004, 0.008), up=w, op='sub', k=0.0008))
    # Purpur auch auf der Klinge (nahe der Spitze, wo sie getroffen hat)
    pt = A.lerp(b0, b1, 0.72)
    glow += A.chain(lambda i: HB, [pt + w * 0.0058, pt + D * 0.05 + hd * 0.008 + w * 0.0055, pt + D * 0.11 - hd * 0.004 + w * 0.0045], [0.0018, 0.0015, 0.0009], k=0.0005)
    purple.append(A.ell(HB, pt + D * 0.04 + w * 0.0052, D, (0.07, 0.016, 0.0012), up=w))

    return [
        {'name': 'helm', 'fm': 'ent_rk_plate', 'parts': helm, 'disp': plate_disp, 'h': 0.0022, 'cav_scale': 0.008, 'lod_weight': 1.6, 'merge': 'platte'},
        {'name': 'brust', 'fm': 'ent_rk_plate', 'parts': brust, 'disp': plate_disp, 'h': 0.0028, 'cav_scale': 0.012, 'lod_weight': 1.3, 'merge': 'platte'},
        {'name': 'schulter', 'fm': 'ent_rk_plate', 'parts': schulter, 'disp': plate_disp, 'h': 0.0026, 'cav_scale': 0.01, 'lod_weight': 1.2, 'merge': 'platte'},
        {'name': 'arme', 'fm': 'ent_rk_plate', 'parts': arme, 'disp': plate_disp, 'h': 0.0024, 'cav_scale': 0.01, 'lod_weight': 1.2, 'merge': 'platte'},
        {'name': 'beine', 'fm': 'ent_rk_plate', 'parts': beine, 'disp': plate_disp, 'h': 0.0028, 'cav_scale': 0.012, 'lod_weight': 1.1, 'merge': 'platte'},
        {'name': 'knauf', 'fm': 'ent_rk_plate', 'parts': knauf, 'disp': plate_disp, 'h': 0.0016, 'cav_scale': 0.004, 'lod_weight': 1.2, 'merge': 'platte'},
        {'name': 'kette', 'fm': 'ent_rk_mail', 'parts': mail, 'disp': mail_disp, 'h': 0.004, 'cav_scale': 0.01, 'lod_weight': 0.5},
        {'name': 'umhang', 'fm': 'ent_rk_cape', 'parts': cape, 'disp': cape_disp, 'h': 0.004, 'cav_scale': 0.014},
        {'name': 'saum', 'fm': 'ent_rk_gold', 'parts': gold, 'disp': None, 'h': 0.002, 'cav_scale': 0.004, 'lod_weight': 0.8},
        {'name': 'klinge', 'fm': 'ent_rk_blade', 'parts': blade, 'disp': blade_disp, 'h': 0.0016, 'cav_scale': 0.004, 'lod_weight': 1.4},
        {'name': 'leder', 'fm': 'ent_pg_leather', 'parts': leather, 'disp': None, 'h': 0.002, 'cav_scale': 0.004},
        {'name': 'nieten', 'fm': 'ent_pg_brass', 'parts': rivet, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003, 'lod_weight': 0.6},
        {'name': 'purpur', 'fm': 'ent_rk_purple', 'parts': purple, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003},
        {'name': 'glut', 'fm': 'ent_rk_purple_glow', 'parts': glow, 'disp': None, 'h': 0.0011, 'cav_scale': 0.002, 'lod_weight': 1.5},
        {'name': 'dunkel', 'fm': 'ent_rk_plate_dark', 'parts': dark, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003},
        {'name': 'leere', 'fm': 'ent_ob_void', 'parts': void, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002},
        {'name': 'saft', 'fm': 'ent_hd_oil', 'parts': oil, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002},
    ]
