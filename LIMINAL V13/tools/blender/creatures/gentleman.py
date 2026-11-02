"""V13.3 Teil 4 Level 5.3: der Hausherr (Entity 18, Wikidot Rev. 54; Level 5.3 Rev. 40, Schlussgeschichte) - KREATUR_Gentleman.md.
Die Seiten beschreiben kein Aussehen; gebaut ist die Auslegung im Briefing: ein Herr im Frack, 2,1 m, zu schmal, ein Hals eine
Handbreit zu lang. Die Arme haben drei Glieder (zwei Ellbogen, wie der Arm aus dem Sessel im Buero von Level 5) und haengen bis
unter die Knie; die Haende wachsbleich, die Finger um ein Glied zu lang, Siegelring. Das Gesicht ist glatt und unbewegt wie
gemalt - Elfenbein unter vergilbtem Firnis mit Haarrissen (Craquelure), gemalte Brauen; nur die Augen sind nass und lebendig.
Von der rechten Schlaefe ueber die Wange kann der Firnis aufreissen: darunter quillt rohes, nasses Fleisch (Knochen 7,
Massstab 0..1). Weste ochsenblutrot, Hemd gestaerkt mit hohem Kragen, schwarze Seidenkrawatte, Lackschuhe mit spitzer Kappe,
die Uhrkette; im selben Skelett das schwarze Notizbuch (Knochen 26).
Runde 0 (Blender): aus Ellipsoiden gesetzt las er sich wie eine Schaufensterpuppe (gestapelte Eier, Kugelgelenke) - Rumpf,
Kopf, Gesicht und Beine kommen jetzt aus dem Baukasten der Menschenwesen (human.py) auf einem Ersatz-Zweibeiner-Skelett aus
seinen Gelenken; die Knochen werden danach auf sein Skelett umgeschrieben.
Knochen (gm in src/anim/creature.hpp): 0 Wurzel, 1 Becken, 2 Ruecken, 3 Brust, 4 Hals, 5 Kopf, 6 Kiefer, 7 Riss,
8 + Seite * 5 + i (Schluesselbein, Oberarm, Mittelarm, Unterarm, Hand; Seite 0 links +y), 18 + Seite * 3 + i (Oberschenkel,
Unterschenkel, Fuss), 24/25 Rockschoesse, 26 Notizbuch.
Gelenke: 0 Wurzel, 1 Becken, 2 Ruecken, 3 Brust, 4 Hals, 5 Kopf, 6 Scheitel, 7/8 Kiefer, 9/10 Riss, 11 + Seite * 5 + i (Schulter,
Ellbogen 1, Ellbogen 2, Handgelenk, Fingerspitzen), 21 + Seite * 4 + i (Huefte, Knie, Knoechel, Zehen), 29-32 Rockschoesse,
33/34 Notizbuch.
"""
import math

import numpy as np

import anat as A
import human as Hm
import sdf

LODS = (42000, 14000, 5000)
HAND = 1.12  # Hand (human.py): um ein Glied zu lange Finger, etwas kraeftiger
PELVIS, SPINE, CHEST, NECK, HEAD, JAW, CRACK = 1, 2, 3, 4, 5, 6, 7
TAIL_L, TAIL_R, BOOK = 24, 25, 26
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])
# Knochen des Zweibeiners (human.py) -> Knochen des Hausherrn
BP2GM = {0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6, 7: 8, 8: 9, 9: 11, 10: 12, 11: 13, 12: 14, 13: 16, 14: 17,
         15: 18, 16: 19, 17: 20, 18: 21, 19: 22, 20: 23}


def armJ(s, i):
    return 11 + s * 5 + i


def armB(s, i):
    return 8 + s * 5 + i


def legJ(s, i):
    return 21 + s * 4 + i


def remap(parts):
    for p in parts:
        p.bone = BP2GM[p.bone]
    return parts


def bp_joints(J):
    """Ersatz-Zweibeiner (Gelenke wie bp): Kopfgelenk und Scheitel so, dass human.Body den Kopf in normaler Groesse auf
    seinen Hals setzt (Mitte 1,985 m)."""
    Jb = np.zeros((24, 3))
    for i in range(5):
        Jb[i] = J[i]
    Jb[5] = (0.012, 0.0, 1.902)
    Jb[6] = (0.02, 0.0, 2.06)
    Jb[7] = J[8]
    for s in range(2):
        Jb[8 + 4 * s] = J[armJ(s, 0)]
        Jb[9 + 4 * s] = J[armJ(s, 1)]
        Jb[10 + 4 * s] = J[armJ(s, 3)]
        Jb[11 + 4 * s] = J[armJ(s, 4)]
        for i in range(4):
            Jb[16 + 4 * s + i] = J[legJ(s, i)]
    return Jb


def build(J, bones):
    J = np.asarray(J)
    B = Hm.Body(bp_joints(J), slim=0.94, muscle=0.7, chest=1.05, belly=0.85, hips=0.95, head=1.0, neck=0.82, hand=HAND)
    Bc = Hm.Body(bp_joints(J), slim=1.0, muscle=0.6, chest=1.12, belly=0.9, hips=1.0, head=1.0, neck=0.82, hand=HAND)
    c, r = B.headC, B.headR
    suit, legs, vest, shirt, cuffs, shoes, hair, brows = [], [], [], [], [], [], [], []
    hand_l, hand_r, nails_l, nails_r, ring, gold, raw, flake, book, pages = [], [], [], [], [], [], [], [], [], []
    links = ([], [])

    # --- Haut: das Gesicht wie bemalt, langer duenner Hals ----------------------------------------------------------------------
    # Runde 1 (Spiel): ein gewoehnliches Menschengesicht, im Kerzenlicht braun, im Lampenlicht eine Puppe - "ganz okay". Jetzt
    # eine glatte, ovale Flaeche wie ein altes Bildnis: flache Nase, keine Lider, gemalte Brauen und Lippen, ein Netz aus Rissen
    # im Firnis; die echten, nassen Augen sitzen lidlos in Loechern mit rohem, nassem Rand und bewegen sich.
    skin = remap(B.head(featureless=True))
    skin += remap(B.neck())
    skin.append(A.cone(NECK, (0.0, 0, 1.72), (0.008, 0, 1.9), 0.047, 0.044, k=0.03))  # der Hals: eine Handbreit zu lang
    root, tip = c + X * r[0] * 0.9 + Z * r[2] * 0.02, c + X * r[0] * 1.05 - Z * r[2] * 0.26
    skin.append(A.cone(HEAD, root, tip, 0.0065, 0.0095, k=0.016))  # flache, gerade Nase
    eyes, iris, pupil, lid, paint, brows = [], [], [], [], [], []
    for s in (0, 1):
        g = Hm.side(s)
        e = c + X * r[0] * 0.80 + Y * g * r[1] * 0.40 + Z * r[2] * 0.04
        skin.append(A.ell(HEAD, e + X * 0.013, X, (0.017, 0.0165, 0.0125), op='sub', k=0.003))  # Loch, kein Lid
        skin.append(A.ell(HEAD, c + Y * g * r[1] * 0.97 - Z * r[2] * 0.06 - X * 0.014, Y * g, (0.008, 0.019, 0.029), up=Z, k=0.008))  # Ohren
        eyes.append(A.ell(HEAD, e - X * 0.0005, X, (0.0128, 0.0136, 0.0128), k=0.0))  # Runde 2: nur die Iris war zu sehen
        lid.append(A.ell(HEAD, e - X * 0.004, X, (0.0085, 0.0168, 0.013), k=0.0))   # roher, nasser Rand um das Auge (hinter dem Augapfel)
        iris.append(A.ell(HEAD, e + X * 0.0112, X, (0.0022, 0.0066, 0.0066), k=0.0))   # dunkle Iris
        pupil.append(A.ell(HEAD, e + X * 0.0128, X, (0.001, 0.0027, 0.0027), k=0.0))

    # gemalte Zuege: auf die Oberflaeche gesetzt (Strahl vom Kopf aus bis zur Haut), 0,6 mm erhaben
    def surf(p_hint):
        d = A.unit(p_hint - c)
        t = 0.02
        for _ in range(60):
            q = c + d * t
            v = float(sdf.sample(skin, q[None, :])[0])
            if v > 0.0:
                break
            t += max(0.0008, -v * 0.8)
        return c + d * t, d
    for s in (0, 1):
        g = Hm.side(s)
        for k in range(5):  # Brauen: duenn, zu gerade, zu hoch
            u = (k - 2) / 2.0
            p, n = surf(c + X * r[0] + Y * g * (r[1] * 0.40 + u * 0.016) + Z * (r[2] * 0.28 + 0.003 * (1 - u * u)))
            brows.append(A.ell(HEAD, p + n * 0.0004, A.unit(Y * g + Z * 0.05 * u), (0.0055, 0.0018, 0.0012), up=n, k=0.0))
    mo, mn = surf(c + X * r[0] - Z * r[2] * 0.47)
    paint.append(A.ell(HEAD, mo + mn * 0.0004 + Z * 0.0025, mn, (0.0012, 0.019, 0.0026), up=Z, k=0.0))   # Oberlippe (gemalt)
    paint.append(A.ell(JAW, mo + mn * 0.0004 - Z * 0.003, mn, (0.0012, 0.017, 0.0034), up=Z, k=0.0))     # Unterlippe
    eyes = eyes  # (Augen ohne Lider - sie blinzeln nie)
    # Haar: glatt nach hinten, pomadisiert, Scheitel links
    hair = remap(B.hair_short(t=0.009))
    hair.append(A.box(HEAD, c + Z * (r[2] * 0.98) + Y * r[1] * 0.35, X, (r[0] * 0.8, 0.0025, 0.012), 0.002, op='sub', k=0.003))

    # --- Frack: Rumpf (vorn offen, Revers), Rockschoesse; Aermel in drei Gliedern -------------------------------------------------
    tc = 0.018
    suit += remap(Bc.torso(t=tc, upper=True, lower=True))
    for g in (1, -1):  # gepolsterte, eckige Schultern
        suit.append(A.ell(CHEST, (-0.012, 0.185 * g, 1.615), A.unit((0, g, -0.15)), (0.062, 0.058, 0.04), up=Z, k=0.045))
    suit.append(A.cone(CHEST, (-0.012, 0, 1.6), (0.0, 0, 1.69), 0.085, 0.08, k=0.03))  # Kragen des Fracks im Nacken
    suit.append(A.cone(CHEST, (0.0, 0, 1.6), (0.01, 0, 1.71), 0.068, 0.064, op='sub', k=0.008))
    # vorn offen: oben schmal (Hemd), zur Taille breiter (Weste); vorn an der Taille kurz geschnitten
    suit.append(A.box(CHEST, (0.14, 0, 1.43), X, (0.045, 0.05, 0.13), 0.02, k=0.015, op='sub'))
    suit.append(A.box(SPINE, (0.135, 0, 1.2), X, (0.05, 0.1, 0.11), 0.03, k=0.03, op='sub'))
    suit.append(A.box(PELVIS, (0.13, 0, 1.0), X, (0.06, 0.2, 0.09), 0.03, k=0.03, op='sub'))
    for g in (1, -1):  # Revers mit steigenden Spitzen
        # Runde 0: die Revers hingen als zwei schwarze Zungen in der Oeffnung - jetzt flach auf der Brust neben der Oeffnung
        top, bot = np.array([0.072, 0.088 * g, 1.6]), np.array([0.118, 0.072 * g, 1.31])
        d = A.unit(bot - top)
        suit.append(A.ell(CHEST, (top + bot) / 2 + X * 0.003, d, (0.16, 0.03, 0.007), up=A.unit(X + Y * g * 0.5), k=0.01))
        suit.append(A.ell(CHEST, top + np.array([0.004, 0.03 * g, 0.02]), A.unit((0.15, g * 0.9, 0.5)), (0.03, 0.014, 0.006),
                          up=A.unit(X + Y * g * 0.3), k=0.006))
    for b, a_, e_ in ((TAIL_L, 29, 30), (TAIL_R, 31, 32)):  # Schoesse bis in die Kniekehlen, in der Mitte geteilt
        a, e = J[a_], J[e_]
        mid = a + (e - a) * 0.5 - X * 0.012
        suit.append(A.tube(b, a + X * 0.012, mid, (0.072, 0.017), (0.066, 0.013), up=-X, k=0.035))
        suit.append(A.tube(b, mid, e, (0.066, 0.013), (0.05, 0.01), up=-X, k=0.01))
    for s in (0, 1):
        g = Hm.side(s)
        S, E1, E2, W, T = (J[armJ(s, i)] for i in range(5))
        suit += [A.ell(armB(s, 1), S + Y * g * 0.012, A.unit(E1 - S), (0.062, 0.058, 0.06), up=X, k=0.04),
                 A.cone(armB(s, 1), S, E1, 0.054, 0.046, k=0.03),
                 A.cone(armB(s, 2), E1, E2, 0.046, 0.042, k=0.014),
                 A.cone(armB(s, 3), E2, W - A.unit(W - E2) * 0.035, 0.042, 0.04, k=0.014)]
        for e, b, rr in ((E1, armB(s, 1), 0.049), (E2, armB(s, 2), 0.045)):  # beide Gelenke zeichnen sich ab, Stauchfalten innen
            suit.append(A.ell(b, e, A.unit(E2 - E1), (0.036, rr, rr), k=0.02))
            for k_ in (-1, 0, 1):
                suit.append(A.ell(b, e + A.unit(E2 - S) * 0.02 * k_ + X * (rr - 0.007), A.unit(Y * g), (0.026, 0.005, 0.004), up=X, k=0.006))
        d = A.unit(T - W)
        cuffs.append(A.cone(armB(s, 3), W - d * 0.05, W + d * 0.008, 0.036, 0.035, k=0.0))
        links[s].append(A.ell(armB(s, 3), W - d * 0.02 + X * 0.035, X, (0.003, 0.007, 0.007), k=0.0))
        # Hand: wachsbleich, die Finger um ein Glied zu lang, Knoechel (human.py, claw verlaengert)
        hp = remap(B.hand(s, curl=0.3, claw=0.28))
        (hand_l if s == 0 else hand_r).extend(hp)
        out = A.unit(Y * g - d * float((Y * g) @ d))
        across = A.unit(np.cross(-out, d))
        L = np.linalg.norm(T - W) * HAND
        for f in range(4):
            off = (f - 1.5) * 0.017 * HAND
            base = W + d * L * 0.48 + across * off
            fl = L * (0.48 - abs(f - 1.3) * 0.05) * 1.28
            p1 = base + d * fl * 0.42 - out * fl * 0.08 * 0.3
            p2 = p1 + A.unit(d - out * 0.27) * fl * 0.32
            p3 = p2 + A.unit(d - out * 0.54) * fl * 0.26
            nd = A.unit(p3 - p2)
            (nails_l if s == 0 else nails_r).append(A.ell(armB(s, 4), p3 - nd * 0.008 + out * 0.0045, nd, (0.0095, 0.0058, 0.0016), up=out, k=0.0))
            if s == 1 and f == 3:  # Siegelring (rechte Hand, kleiner Finger)
                rc = base + d * fl * 0.2
                ring.append(A.cone(armB(s, 4), rc - d * 0.006, rc + d * 0.006, 0.0118, 0.0118, k=0.0))
                ring.append(A.ell(armB(s, 4), rc + out * 0.01, out, (0.0032, 0.0085, 0.007), up=d, k=0.002))

    # --- Weste (ochsenblutrot) mit Knoepfen und Uhrkette, Hemd mit hohem Kragen, Krawatte --------------------------------------
    vest += remap(B.torso(t=0.008, upper=True, lower=False))
    vest += [A.ell(SPINE, (0.006, 0, 1.24), X, (0.085, 0.12, 0.15), k=0.04)]  # Runde 0: stand als roter Bauch vor dem Frack
    vest.append(A.box(CHEST, (0.15, 0, 1.55), X, (0.05, 0.035, 0.06), 0.012, k=0.012, op='sub'))  # V-Ausschnitt
    vest.append(A.box(CHEST, (0.0, 0, 1.75), X, (0.2, 0.3, 0.1), 0.02, k=0.02, op='sub'))  # Schultern frei (unter dem Frack)

    def vest_x(z):
        return 0.006 + 0.085 * math.sqrt(max(0.0, 1.0 - ((z - 1.24) / 0.15) ** 2)) + 0.003
    for k_ in range(4):
        z = 1.16 + 0.048 * k_
        gold.append(A.ell(SPINE, (vest_x(z) + 0.001, 0, z), X, (0.004, 0.0055, 0.0055), k=0.0))
    chain_pts = [np.array([vest_x(z) - 0.004 * abs(y) / 0.09, y, z]) for y, z in ((0.09, 1.22), (0.07, 1.198), (0.05, 1.188), (0.03, 1.192), (0.012, 1.2), (0.0, 1.208))]
    gold += A.chain(lambda i: SPINE, chain_pts, [0.0018] * 6, k=0.0)
    shirt += [A.ell(CHEST, (0.07, 0, 1.54), X, (0.062, 0.062, 0.1), k=0.0),
              A.cone(NECK, (0.0, 0, 1.675), (0.006, 0, 1.79), 0.06, 0.055, k=0.0)]
    shirt.append(A.cone(NECK, (0.0, 0, 1.69), (0.006, 0, 1.8), 0.051, 0.048, k=0.004, op='sub'))
    for g in (1, -1):
        shirt.append(A.ell(NECK, (0.058, 0.022 * g, 1.71), A.unit((0.5, 0.3 * g, -0.8)), (0.022, 0.012, 0.003), up=X, k=0.002))
    # Runde 1: die Hemdbrust stand als weisses Ei im Frack - jetzt eine schwarze Seidenkrawatte mit Knoten, deren Enden ueber
    # die Hemdbrust fallen, zwei schwarze Knoepfe
    suit += [A.ell(NECK, (0.06, 0, 1.70), X, (0.014, 0.04, 0.018), k=0.004),
             A.ell(NECK, (0.068, 0, 1.692), X, (0.014, 0.016, 0.015), k=0.004),
             A.tube(CHEST, (0.072, 0.004, 1.685), (0.118, 0.012, 1.56), (0.017, 0.0045), (0.024, 0.004), up=X, k=0.006),
             A.tube(CHEST, (0.07, -0.006, 1.682), (0.112, -0.016, 1.575), (0.015, 0.004), (0.021, 0.0035), up=X, k=0.006)]
    for z in (1.51, 1.47):
        shoes.append(A.ell(CHEST, (0.07 + 0.062 * math.sqrt(max(0.0, 1.0 - ((z - 1.54) / 0.1) ** 2)) + 0.001, 0, z), X, (0.003, 0.0045, 0.0045), k=0.0))

    # --- Hose mit Buegelfalte, Lackschuhe mit langer, spitzer Kappe -----------------------------------------------------------
    for s in (0, 1):
        legs += remap(B.leg(s, t=0.016))
        Hp, K, Ak, Tz = (J[legJ(s, i)] for i in range(4))
        legs += [A.ell(18 + 3 * s, Hp + (K - Hp) * 0.5 + X * 0.07, A.unit(K - Hp), (0.2, 0.004, 0.007), up=X, k=0.02),
                 A.ell(19 + 3 * s, K + (Ak - K) * 0.45 + X * 0.064, A.unit(Ak - K), (0.21, 0.004, 0.007), up=X, k=0.02),
                 A.cone(19 + 3 * s, K + (Ak - K) * 0.6, Ak + Z * 0.02, 0.06, 0.062, k=0.03)]   # das Hosenbein faellt bis auf den Schuh
        shoes += remap(B.foot(s, t=0.01))
        fd = A.unit(np.array([Tz[0] - Ak[0], Tz[1] - Ak[1], 0.0]))
        shoes.append(A.ell(20 + 3 * s, Tz + fd * 0.035 + Z * 0.008, fd, (0.075, 0.034, 0.024), k=0.03))   # spitze Kappe, zu lang
        shoes.append(A.box(20 + 3 * s, Ak - fd * 0.045 - Z * 0.07, fd, (0.03, 0.034, 0.018), 0.006, k=0.008))  # Absatz

    # --- der Riss (Knochen 7): rohes Fleisch quillt aus dem Firnis, die Raender blaettern auf -----------------------------------
    a, b = J[9], J[10]
    pts = []
    hc, hr = c + Z * r[2] * 0.12, np.array([r[0], r[1], r[2] * 0.86])
    for i in range(9):
        t = i / 8
        p = a + (b - a) * t
        q = (p - hc) / hr
        p = hc + q / np.linalg.norm(q) * hr * 1.01
        side = A.unit(np.cross(b - a, p - hc))
        p = p + side * (0.0035 * (1 if i % 2 else -1) * math.sin(math.pi * t))
        pts.append(p)
    raw += A.chain(lambda i: CRACK, pts, [0.0022] + [0.0062 * math.sin(math.pi * (i / 8)) + 0.002 for i in range(1, 8)] + [0.0018], k=0.002)
    for i in range(1, 8):
        p = pts[i]
        side = A.unit(np.cross(b - a, p - hc))
        nrm = A.unit(p - hc)
        for k_ in (-1, 1):
            flake.append(A.ell(CRACK, p + side * k_ * 0.0075 + nrm * 0.0016, A.unit(b - a), (0.0075, 0.005, 0.0011),
                               up=A.unit(nrm + side * k_ * 0.6), k=0.0))

    # --- das Notizbuch (Knochen 26): schwarzes Leder, Ruecken -> Vorderkante 13 cm, 19 cm hoch, 2,2 cm dick ----------------------
    bc = (J[33] + J[34]) / 2
    book.append(A.box(BOOK, bc, X, (0.065, 0.095, 0.011), 0.0025, k=0.0))
    book.append(A.box(BOOK, bc + X * 0.006, X, (0.062, 0.091, 0.0085), 0.001, k=0.0, op='sub'))
    book.append(A.ell(BOOK, bc - X * 0.064, Y, (0.096, 0.004, 0.012), up=Z, k=0.003))
    pages.append(A.box(BOOK, bc + X * 0.003, X, (0.0605, 0.0905, 0.0082), 0.0008, k=0.0))

    # --- Oberflaechen ------------------------------------------------------------------------------------------------------
    def cloth_disp(Q):  # Kammgarn: feine Falten, hier und da ein Zug
        return -0.0005 * sdf.fbm(Q * np.array([1.0, 1.0, 0.45]), 38, 2, seed=1801) - 0.00025 * sdf.ridged(Q, 90, 2, seed=1802)

    def leg_disp(Q):  # Hosenbein: senkrechte Falten, unten gestaucht
        z = Q[:, 2]
        k = np.clip((0.25 - z) / 0.2, 0.0, 1.0)
        return -0.0006 * sdf.fbm(Q * np.array([1.0, 1.0, 0.25]), 30, 2, seed=1803) - 0.0012 * k * sdf.ridged(Q * np.array([0.6, 0.6, 2.5]), 40, 2, seed=1804)

    def varnish(Q):  # Craquelure: Runde 1 nicht zu sehen, Runde 2 ein klumpiger Teig - jetzt feine, scharfe Rissnetze
        d1, d2 = sdf.worley(Q, 85, seed=1805)
        d1b, d2b = sdf.worley(Q, 34, seed=1806)
        return 0.00035 * np.exp(-((d2 - d1) * 22.0) ** 2) + 0.0007 * np.exp(-((d2b - d1b) * 16.0) ** 2)

    def skin_hand(Q):  # wachsbleich: kaum Poren, feine Querfalten an den Fingern
        return -0.00012 * sdf.fbm(Q, 300, 2, seed=1807) + 0.0002 * sdf.ridged(Q * np.array([0.2, 0.2, 1.0]), 400, 1, seed=1808)

    def combed(Q):  # Pomade: Kammrillen nach hinten
        return 0.0006 * sdf.ridged(Q * np.array([0.12, 1.0, 1.0]), 240, 2, seed=1809)

    def wet(Q):
        return -0.0003 * sdf.fbm(Q, 500, 2, seed=1810)

    return [
        {'name': 'frack', 'fm': 'ent_gm_suit', 'parts': suit, 'disp': cloth_disp, 'h': 0.0036, 'cav_scale': 0.012, 'merge': 'frack'},
        {'name': 'hose', 'fm': 'ent_gm_suit', 'parts': legs, 'disp': leg_disp, 'h': 0.0038, 'cav_scale': 0.012, 'merge': 'frack'},
        {'name': 'weste', 'fm': 'ent_gm_vest', 'parts': vest, 'disp': cloth_disp, 'h': 0.003, 'cav_scale': 0.008, 'cav_extra': suit},
        {'name': 'hemd', 'fm': 'ent_gm_shirt', 'parts': shirt, 'disp': None, 'h': 0.0018, 'cav_scale': 0.005, 'merge': 'hemd', 'cav_extra': suit},
        {'name': 'manschetten', 'fm': 'ent_gm_shirt', 'parts': cuffs, 'disp': None, 'h': 0.0018, 'cav_scale': 0.004, 'merge': 'hemd'},
        {'name': 'schuhe', 'fm': 'ent_gm_shoe', 'parts': shoes, 'disp': None, 'h': 0.0022, 'cav_scale': 0.006},
        {'name': 'gesicht', 'fm': 'ent_gm_face', 'parts': skin, 'disp': varnish, 'h': 0.0009, 'cav_scale': 0.003, 'lod_weight': 2.4},
        {'name': 'brauen', 'fm': 'ent_gm_hair', 'parts': brows, 'disp': None, 'h': 0.0006, 'cav_scale': 0.0015, 'merge': 'haar'},
        {'name': 'haar', 'fm': 'ent_gm_hair', 'parts': hair, 'disp': combed, 'h': 0.0016, 'cav_scale': 0.004, 'merge': 'haar', 'cav_extra': skin},
        {'name': 'hand_l', 'fm': 'ent_gm_hand', 'parts': hand_l, 'disp': skin_hand, 'h': 0.0011, 'cav_scale': 0.003, 'merge': 'haende', 'lod_weight': 1.8},
        {'name': 'hand_r', 'fm': 'ent_gm_hand', 'parts': hand_r, 'disp': skin_hand, 'h': 0.0011, 'cav_scale': 0.003, 'merge': 'haende', 'lod_weight': 1.8},
        {'name': 'naegel_l', 'fm': 'ent_tooth', 'parts': nails_l, 'disp': None, 'h': 0.0006, 'cav_scale': 0.001, 'merge': 'naegel'},
        {'name': 'naegel_r', 'fm': 'ent_tooth', 'parts': nails_r, 'disp': None, 'h': 0.0006, 'cav_scale': 0.001, 'merge': 'naegel'},
        {'name': 'augen', 'fm': 'ent_gm_sclera', 'parts': eyes, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002, 'lod_weight': 2.0},
        {'name': 'iris', 'fm': 'ent_gm_eye', 'parts': iris, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'lod_weight': 2.0},
        {'name': 'pupillen', 'fm': 'ent_jy_pupil', 'parts': pupil, 'disp': None, 'h': 0.0003, 'cav_scale': 0.001, 'lod_weight': 2.0},
        {'name': 'lidrand', 'fm': 'ent_gm_raw', 'parts': lid, 'disp': wet, 'h': 0.0004, 'cav_scale': 0.001, 'merge': 'roh'},
        {'name': 'lippen', 'fm': 'ent_gm_raw', 'parts': paint, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'merge': 'roh'},
        {'name': 'riss', 'fm': 'ent_gm_raw', 'parts': raw, 'disp': wet, 'h': 0.0005, 'cav_scale': 0.0015, 'merge': 'roh', 'lod_weight': 1.6},
        {'name': 'schuppen', 'fm': 'ent_gm_face', 'parts': flake, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'merge': 'schuppen'},
        {'name': 'ring', 'fm': 'ent_gm_gold', 'parts': ring, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'merge': 'gold'},
        {'name': 'knoepfe', 'fm': 'ent_gm_gold', 'parts': gold, 'disp': None, 'h': 0.0008, 'cav_scale': 0.002, 'merge': 'gold'},
        {'name': 'manschettenknopf_l', 'fm': 'ent_gm_gold', 'parts': links[0], 'disp': None, 'h': 0.0007, 'cav_scale': 0.002, 'merge': 'gold'},
        {'name': 'manschettenknopf_r', 'fm': 'ent_gm_gold', 'parts': links[1], 'disp': None, 'h': 0.0007, 'cav_scale': 0.002, 'merge': 'gold'},
        {'name': 'buch', 'fm': 'ent_gm_book', 'parts': book, 'disp': None, 'h': 0.0018, 'cav_scale': 0.002, 'lod_weight': 0.5},
        {'name': 'seiten', 'fm': 'ent_gm_shirt', 'parts': pages, 'disp': None, 'h': 0.0018, 'cav_scale': 0.0015, 'merge': 'seiten', 'lod_weight': 0.4},
    ]
