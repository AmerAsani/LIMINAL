"""V13.3 Teil 4 Level 188: der Horcher - KREATUR_Horcher.md (Wikidot level-188 Rev. 69, Abschnitt Corridors - CC BY-SA 3.0).
Die Seite nennt in den Gaengen um den Hof "andere boesartige menschenaehnliche Lebensformen", ohne sie zu beschreiben. Gebaut ist
die Auslegung im Briefing: ein ausgezehrter, kreideweisser Leib (weiss wie die Waende der Gaenge), gebueckt, jede Rippe und jeder
Wirbel zeichnet sich ab. Statt eines Kopfes sitzt am Ende eines viel zu langen Halses ein riesiges Ohr (0,62 m): Helix, Anthelix,
Muschel, Tragus, Laeppchen - duenne Haut ueber Knorpel, Adern scheinen durch. Der Gehoergang ist der Mund: nass, rot, innen zwei
Ringe kleiner gelber Zaehne. Im Laeppchen ein verkuemmertes Gesicht mit geschlossenen Augen und zugewachsenem Mund. Der linke Arm
duenn, die Finger zu lang, gelbe Naegel; der rechte Arm zu einer Klopf-Faust ausgewachsen (Unterarm dicker als ein Oberschenkel),
die Knoechel schwielig, aufgeplatzt, mit Splittern dunklen Mahagonis von den Tueren, an die es klopft.
Knochen (hr in src/anim/creature.hpp): 0 Wurzel, 1 Becken, 2 Ruecken, 3 Brust, 4/5 Hals, 6 Ohr (Muschel, Schaedel dahinter),
7 Helix (Rand), 8 Laeppchen, 9 Gehoergang, 10 + Seite * 4 + i (Schluesselbein, Oberarm, Unterarm, Hand/Faust; Seite 0 links +y),
18 + Seite * 3 + i (Oberschenkel, Unterschenkel, Fuss).
Gelenke: 1 Becken, 2 Ruecken, 3 Brust, 4-6 Hals, 7 Ohrmitte, 8 Ohrspitze, 9 Laeppchen, 10 Gehoergang, 11 + Seite * 4 + i (Schulter,
Ellbogen, Handgelenk, Spitze/Knoechel), 19 + Seite * 4 + i (Huefte, Knie, Knoechel, Zehen).
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (44000, 15000, 5200)
PELVIS, SPINE, CHEST, NECK0, NECK1, HEAD, HELIX, LOBE, CANAL = 1, 2, 3, 4, 5, 6, 7, 8, 9
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def armJ(s, i):
    return 11 + s * 4 + i


def armB(s, i):
    return 10 + s * 4 + i


def legJ(s, i):
    return 19 + s * 4 + i


def legB(s, i):
    return 18 + s * 3 + i


def surf(parts, o, d, tmax=0.6):
    """Strahl von o in Richtung d bis zur Oberflaeche der Teile (None: kein Treffer)."""
    d = A.unit(d)
    t = 0.0
    for _ in range(600):
        q = o + d * t
        v = float(sdf.sample(parts, q[None, :])[0])
        if v < 0.0004:
            return q
        t += min(0.006, max(0.0005, v * 0.85))  # ausserhalb aller Teile liefert sample 1,0 - kleine Schritte
        if t > tmax:
            return None
    return None


def normal_at(parts, p, e=0.002):
    g = np.zeros(3)
    for i in range(3):
        dv = np.zeros(3)
        dv[i] = e
        g[i] = float(sdf.sample(parts, (p + dv)[None, :])[0]) - float(sdf.sample(parts, (p - dv)[None, :])[0])
    return A.unit(g)


def build(J, bones):
    J = np.asarray(J)
    rng = np.random.default_rng(1881)
    skin, ear, canal, teeth, callus, raw, nails, veins_ear, veins_arm, veins_neck, wood = ([] for _ in range(11))

    # --- Rumpf: ausgezehrt und gebueckt - Brustkorb mit Rippen, Wirbel, Schulterblaetter, eingefallener Bauch ------------------
    P0, S0, C0, N0 = J[1], J[2], J[3], J[4]
    a0 = A.unit(S0 - P0)              # Becken -> Ruecken
    f0 = A.unit(np.cross(Y, a0))      # vorn (Bauch) - im Buecken nach vorn unten
    a = A.unit(C0 - S0)               # Ruecken -> Brust
    f = A.unit(np.cross(Y, a))
    RC = S0 + a * 0.12 + f * 0.02     # Mitte des Brustkorbs
    RA, RY, RF = 0.175, 0.128, 0.098  # Halbachsen: entlang, quer, vorn/hinten
    skin.append(A.ell(SPINE, RC, a, (RA, RY, RF), up=f, k=0.0))
    skin.append(A.ell(CHEST, RC + a * 0.13 - f * 0.01, a, (0.09, RY * 0.95, RF * 0.92), up=f, k=0.05))  # obere Brust
    # Rippen: sieben Paare, jede ein Wulst ueber dem Brustkorb, von der Wirbelsaeule nach vorn abfallend
    for k_ in range(7):
        s = -0.115 + k_ * 0.037
        q = math.sqrt(max(0.05, 1.0 - (s / RA) ** 2))
        for g in (1, -1):
            pts, rr = [], []
            for i in range(10):
                th = 0.25 + i / 9 * (2.45 if k_ > 1 else 2.2)
                lift = -0.04 * th / math.pi
                pts.append(RC + a * (s + lift) - f * math.cos(th) * (RF * q + 0.0035) + Y * g * math.sin(th) * (RY * q + 0.0035))
                rr.append(0.0075 if 0 < i < 9 else 0.004)
            skin += A.chain(lambda i, s=s: CHEST if s > 0.07 else SPINE, pts, rr, k=0.008)
    # Wirbel: eine Reihe Knubbel den ganzen Ruecken hinauf
    for i in range(12):
        s = -0.17 + i * 0.03
        q = math.sqrt(max(0.05, 1.0 - (s / RA) ** 2))
        skin.append(A.ell(CHEST if s > 0.07 else SPINE, RC + a * s - f * (RF * q + 0.004), a, (0.012, 0.016, 0.012), up=-f, k=0.012))
    for t in (0.25, 0.5, 0.75):
        skin.append(A.ell(PELVIS, P0 + a0 * (np.linalg.norm(S0 - P0) * t) - f0 * 0.068, a0, (0.013, 0.017, 0.013), up=-f0, k=0.012))
    # Schulterblaetter: flache, abstehende Platten
    for g in (1, -1):
        sb = RC + a * 0.09 - f * (RF * 0.82) + Y * g * 0.075
        skin.append(A.ell(CHEST, sb, a, (0.075, 0.05, 0.016), up=A.unit(-f + Y * g * 0.5), k=0.025))
    # Bauch: eng, unter den Rippen eingefallen; Becken mit vorstehenden Beckenkaemmen
    skin.append(A.ell(PELVIS, P0 + a0 * 0.17 - f0 * 0.012, a0, (0.15, 0.092, 0.066), up=f0, k=0.05))
    skin.append(A.ell(PELVIS, P0 + a0 * 0.02, a0, (0.09, 0.135, 0.085), up=f0, k=0.05))
    skin.append(A.ell(SPINE, RC - a * 0.15 + f * 0.11, a, (0.06, 0.08, 0.03), up=f, op='sub', k=0.035))  # unter dem Rippenbogen hohl
    for g in (1, -1):
        skin.append(A.ell(PELVIS, P0 + a0 * 0.08 + f0 * 0.03 + Y * g * 0.118, a0, (0.04, 0.022, 0.03), up=f0, k=0.02))
        skin.append(A.ell(PELVIS, P0 - f0 * 0.055 + Y * g * 0.06 - a0 * 0.02, a0, (0.06, 0.06, 0.05), up=-f0, k=0.04))  # Gesaess, mager

    # --- Schultern: rechts ein Muskelberg (Klopf-Arm), links schmal --------------------------------------------------------
    SL, SR = J[armJ(0, 0)], J[armJ(1, 0)]
    skin.append(A.ell(CHEST, (C0 + SL) / 2 + Z * 0.01, A.unit(SL - C0), (0.11, 0.045, 0.04), up=Z, k=0.04))
    skin.append(A.ell(armB(0, 1), SL + Z * 0.005, A.unit(J[armJ(0, 1)] - SL), (0.045, 0.035, 0.038), up=X, k=0.03))
    skin.append(A.ell(armB(1, 0), (C0 + SR) / 2 + Z * 0.05, A.unit(SR - C0), (0.17, 0.1, 0.085), up=Z, k=0.06))   # Trapez rechts
    skin.append(A.ell(armB(1, 1), SR + Z * 0.01 - Y * 0.015, A.unit(J[armJ(1, 1)] - SR), (0.11, 0.095, 0.1), up=X, k=0.05))  # Deltamuskel
    skin.append(A.cone(armB(1, 0), N0 - a * 0.04, SR + Z * 0.04, 0.06, 0.07, k=0.05))  # Nacken zum rechten Arm

    # --- Hals: viel zu lang, Sehnen, Wirbelknubbel oben, Kehlsack unten ----------------------------------------------------
    N1, N2 = J[5], J[6]
    skin.append(A.ell(CHEST, N0 - a * 0.03, A.unit(N1 - N0), (0.085, 0.082, 0.07), up=Z, k=0.05))
    skin.append(A.cone(NECK0, N0, N1, 0.056, 0.047, k=0.04))
    skin.append(A.cone(NECK1, N1, N2, 0.047, 0.052, k=0.03))
    for (pA, pB, b_) in ((N0, N1, NECK0), (N1, N2, NECK1)):
        ad = A.unit(pB - pA)
        top = A.unit(np.cross(ad, Y))
        for t in (0.2, 0.5, 0.8):
            skin.append(A.ell(b_, pA + (pB - pA) * t + top * 0.045, ad, (0.013, 0.017, 0.012), up=top, k=0.012))
        for g in (1, -1):  # Sehnen seitlich
            skin.append(A.muscle(b_, pA + Y * g * 0.03 - top * 0.012, pB + Y * g * 0.034 - top * 0.015, 0.013, k=0.012, t0=0.0, t1=1.0))
    ad = A.unit(N2 - N1)
    top = A.unit(np.cross(A.unit(N1 - N0), Y))
    sac = N1 - top * 0.05 + A.unit(N1 - N0) * 0.01
    skin.append(A.ell(NECK1, sac, A.unit(N1 - N0), (0.07, 0.042, 0.038), up=-top, k=0.03))  # Kehlsack: pumpt beim Laut

    # --- linker Arm: duenn, Spinnenhand mit gelben Naegeln -----------------------------------------------------------------
    S, E, W, T = (J[armJ(0, i)] for i in range(4))
    skin += [A.cone(armB(0, 1), S, E, 0.03, 0.023, k=0.03),
             A.ell(armB(0, 2), E, A.unit(W - E), (0.026, 0.024, 0.026), up=X, k=0.015),     # spitzer Ellbogen
             A.cone(armB(0, 2), E, W, 0.024, 0.016, k=0.02),
             A.muscle(armB(0, 2), E, W, 0.02, offset=Z * 0.004, k=0.012, t0=0.05, t1=0.6)]
    hd = A.unit(T - W)
    across = A.unit(np.cross(Z, hd))
    down = A.unit(np.cross(across, hd))      # Handflaeche nach unten
    skin.append(A.ell(armB(0, 3), W + hd * 0.038, hd, (0.044, 0.027, 0.009), up=-down, k=0.012))
    skin.append(A.cone(armB(0, 3), W - hd * 0.01, W + hd * 0.012, 0.016, 0.019, k=0.01))
    for fi, L in enumerate((0.13, 0.152, 0.146, 0.118)):
        base = W + hd * 0.07 + across * ((fi - 1.5) * 0.0155)
        p1 = base + hd * L * 0.42
        p2 = p1 + A.unit(hd + down * 0.35) * L * 0.33
        p3 = p2 + A.unit(hd + down * 0.85) * L * 0.25
        skin += A.chain(lambda i: armB(0, 3), [base, p1, p2, p3], [0.0068, 0.0058, 0.005, 0.0032], k=0.004)
        for kn in (base, p1, p2):
            skin.append(A.ell(armB(0, 3), kn - down * 0.0015, hd, (0.0075, 0.0072, 0.0068), up=-down, k=0.003))
        nd = A.unit(p3 - p2)
        nails.append(A.ell(armB(0, 3), p3 + nd * 0.003 - down * 0.0033, nd, (0.0125, 0.0052, 0.0016), up=-down, k=0.0))
    tb = W + hd * 0.02 - across * 0.024
    t1 = tb + A.unit(hd - across * 0.6 + down * 0.4) * 0.045
    t2 = t1 + A.unit(hd + down * 0.6) * 0.04
    skin += A.chain(lambda i: armB(0, 3), [tb, t1, t2], [0.009, 0.0068, 0.0045], k=0.006)
    nails.append(A.ell(armB(0, 3), t2 + A.unit(t2 - t1) * 0.002 - down * 0.003, A.unit(t2 - t1), (0.011, 0.005, 0.0015), up=-down, k=0.0))

    # --- rechter Arm: die Klopf-Faust ---------------------------------------------------------------------------------------
    S, E, W, K = (J[armJ(1, i)] for i in range(4))
    du, dfo = A.unit(E - S), A.unit(W - E)
    skin += [A.cone(armB(1, 1), S, E, 0.083, 0.068, k=0.05),
             A.muscle(armB(1, 1), S, E, 0.06, offset=X * 0.035, k=0.03),         # Bizeps
             A.muscle(armB(1, 1), S, E, 0.058, offset=-X * 0.03 - Y * 0.02, k=0.03),  # Trizeps
             A.ell(armB(1, 2), E - du * 0.01, dfo, (0.07, 0.07, 0.072), up=X, k=0.04),   # Ellbogen, knochig
             # Runde 1: ein gerader, ueberall gleich dicker Unterarm las sich wie eine Saeule (Prothese) - jetzt Muskelbaeuche
             # unter dem Ellbogen, schmales Handgelenk, darunter die breite Faust
             A.cone(armB(1, 2), E, W, 0.068, 0.07, k=0.05),
             A.muscle(armB(1, 2), E, W, 0.095, offset=X * 0.035 - Y * 0.02, k=0.04, t0=0.05, t1=0.62),   # Oberarmspeichenmuskel
             A.muscle(armB(1, 2), E, W, 0.078, offset=-X * 0.03 + Y * 0.03, k=0.04, t0=0.1, t1=0.7),
             A.muscle(armB(1, 2), E, W, 0.06, offset=Y * 0.045, k=0.03, t0=0.35, t1=0.85)]
    for t in (0.9, 0.95):  # Hautfalten am Handgelenk
        skin.append(A.cone(armB(1, 2), W - dfo * (1.0 - t) * 0.9 - dfo * 0.007, W - dfo * (1.0 - t) * 0.9 + dfo * 0.007, 0.08, 0.08, k=0.012))
    d = A.unit(K - W)
    acr = A.unit(np.cross(Z, d))
    back = A.unit(np.cross(d, acr))   # Handruecken (nach vorn)
    FB = armB(1, 3)
    skin.append(A.cone(FB, W - d * 0.03, W + d * 0.06, 0.075, 0.1, k=0.04))
    # Faust: Mittelhand gewoelbt, vier geschwollene Knoechel, eingeschlagene Finger, Daumen quer darueber (Runde 1: ein Klotz)
    skin.append(A.ell(FB, W + d * 0.13 + back * 0.012, d, (0.11, 0.118, 0.07), up=back, k=0.035))
    knuckles = []
    for i in range(4):
        sp = (i - 1.5)
        kp = W + d * (0.232 - 0.012 * abs(sp)) + acr * (sp * 0.056) + back * 0.028
        knuckles.append(kp)
        skin.append(A.ell(FB, kp, d, (0.038, 0.032, 0.036), up=back, k=0.016))
        # eingeschlagene Finger: zwei Glieder, die Kuppen in der Handflaeche
        f0 = kp + d * 0.02 - back * 0.005
        f1 = f0 + d * 0.012 - back * 0.055
        f2 = f1 - d * 0.05 - back * 0.03
        skin += A.chain(lambda i: FB, [f0, f1, f2], [0.029, 0.027, 0.024], k=0.012)
        nails.append(A.ell(FB, f2 - d * 0.012 - back * 0.012, -d, (0.014, 0.016, 0.004), up=-back, k=0.0))
    thumb = [W + d * 0.07 + acr * 0.105 - back * 0.03, W + d * 0.14 + acr * 0.085 - back * 0.08, W + d * 0.19 + acr * 0.03 - back * 0.1]
    skin += A.chain(lambda i: FB, thumb, [0.038, 0.032, 0.028], k=0.015)
    nails.append(A.ell(FB, thumb[2] + A.unit(thumb[2] - thumb[1]) * 0.008 - back * 0.022, A.unit(thumb[2] - thumb[1]), (0.019, 0.017, 0.004), up=-back, k=0.0))
    for i in range(4):  # Sehnen auf dem Handruecken
        skin.append(A.muscle(FB, W + d * 0.03 + acr * ((i - 1.5) * 0.03), knuckles[i] - d * 0.02, 0.009, offset=back * 0.075, k=0.01, t0=0.1, t1=0.9))

    # --- Beine: duenn, knotige Knie; lange nackte Fuesse mit greifenden Zehen ------------------------------------------------
    for s in (0, 1):
        g = 1.0 if s == 0 else -1.0
        H, Kn, An, To = (J[legJ(s, i)] for i in range(4))
        dt_, ds_ = A.unit(Kn - H), A.unit(An - Kn)
        skin += [A.cone(legB(s, 0), H + Z * 0.02, Kn, 0.06, 0.036, k=0.04),
                 A.muscle(legB(s, 0), H, Kn, 0.034, offset=X * 0.018, k=0.03),
                 A.ell(legB(s, 1), Kn + X * 0.012, dt_, (0.04, 0.042, 0.04), up=X, k=0.02),           # Knie
                 A.ell(legB(s, 1), Kn + X * 0.038 + Z * 0.008, dt_, (0.022, 0.024, 0.016), up=X, k=0.012),  # Kniescheibe
                 A.cone(legB(s, 1), Kn, An, 0.036, 0.022, k=0.03),
                 A.muscle(legB(s, 1), Kn, An, 0.026, offset=-X * 0.016, k=0.02, t0=0.1, t1=0.55),     # Wade, mager
                 A.ell(legB(s, 1), An + Y * g * 0.024, ds_, (0.018, 0.012, 0.016), up=X, k=0.012),     # Knoechel aussen
                 A.ell(legB(s, 1), An - Y * g * 0.022 + Z * 0.008, ds_, (0.016, 0.011, 0.014), up=X, k=0.012)]
        fd = A.unit(np.array([To[0] - An[0], To[1] - An[1], 0.0]))
        fa = A.unit(np.cross(Z, fd))
        heel = np.array([An[0] - 0.04, An[1], 0.032])
        ball = np.array([To[0] - 0.035, To[1], 0.024])
        FT = legB(s, 2)
        skin += [A.cone(FT, An + Z * 0.01, heel, 0.03, 0.03, k=0.02),
                 A.ell(FT, (heel + ball) / 2 + Z * 0.008, fd, (np.linalg.norm(ball - heel) * 0.6, 0.036, 0.024), up=Z, k=0.025),
                 A.ell(FT, ball, fd, (0.032, 0.044, 0.02), up=Z, k=0.02)]
        for ti, L in enumerate((0.07, 0.062, 0.056, 0.05, 0.042)):
            off = (1.5 - ti) * 0.017 * g - (0.004 * g if ti == 0 else 0.0)
            b0 = ball + fd * 0.022 + fa * off + Z * 0.004
            b1 = b0 + A.unit(fd + Z * 0.25) * L * 0.45
            b2 = b1 + A.unit(fd - Z * 0.9) * L * 0.35
            b3 = b2 + A.unit(fd * 0.3 - Z) * L * 0.22
            b3[2] = max(b3[2], 0.006)
            r0 = 0.0105 if ti == 0 else 0.0078 - ti * 0.0005
            skin += A.chain(lambda i: FT, [b0, b1, b2, b3], [r0, r0 * 0.9, r0 * 0.78, r0 * 0.55], k=0.004)
            skin.append(A.ell(FT, b1 + Z * 0.002, fd, (r0 * 1.2, r0 * 1.15, r0 * 1.1), up=Z, k=0.003))  # Gelenkknoten
            nd = A.unit(b3 - b2)
            nails.append(A.ell(FT, b3 + nd * 0.001 + A.unit(fd + Z * 0.4) * r0 * 0.45, nd, (r0 * 0.9, r0 * 0.75, 0.0014), up=A.unit(fd + Z * 0.5), k=0.0))

    # --- das Ohr (Kopf) --------------------------------------------------------------------------------------------------------
    c = J[7]

    def E_(u, v, w):
        return c + Y * u + Z * v + X * w
    # Platte (oben breit, unten schmal), dahinter der Schaedelknorpel, in den der Hals von hinten oben steckt
    ear += [A.ell(HEAD, E_(0.045, 0.07, -0.025), X, (0.028, 0.152, 0.232), up=Z, k=0.0),
            A.ell(HEAD, E_(0.012, -0.15, -0.015), X, (0.032, 0.112, 0.118), up=Z, k=0.045),
            A.ell(HEAD, E_(0.02, -0.01, -0.085), X, (0.072, 0.118, 0.165), up=Z, k=0.05),
            A.cone(HEAD, J[6] - A.unit(J[6] - J[5]) * 0.03, E_(0.025, 0.06, -0.085), 0.05, 0.085, k=0.05)]
    # Helix: ein dicker, eingerollter Rand vom Schenkel in der Muschel ueber den Scheitel bis zum Laeppchen
    hx = [(-0.02, 0.02, 0.03), (-0.083, 0.12, 0.036), (-0.088, 0.22, 0.034), (-0.04, 0.293, 0.03), (0.04, 0.31, 0.03),
          (0.12, 0.27, 0.03), (0.172, 0.18, 0.028), (0.193, 0.07, 0.026), (0.188, -0.04, 0.025), (0.158, -0.13, 0.024),
          (0.108, -0.2, 0.024), (0.05, -0.245, 0.028)]
    hr_ = [0.016, 0.025, 0.031, 0.034, 0.035, 0.035, 0.034, 0.032, 0.03, 0.028, 0.026, 0.026]
    ear += A.chain(lambda i: HELIX, [E_(*p) for p in hx], hr_, k=0.012)
    # Kahnfurche zwischen Helix und Anthelix
    sc = [(-0.02, 0.235), (0.06, 0.245), (0.118, 0.18), (0.142, 0.075), (0.138, -0.03), (0.112, -0.115)]
    for i in range(len(sc) - 1):
        ear.append(A.cone(HEAD, E_(sc[i][0], sc[i][1], 0.024), E_(sc[i + 1][0], sc[i + 1][1], 0.024), 0.017, 0.017, op='sub', k=0.01))
    # Anthelix mit zwei Schenkeln, Tragus und Antitragus
    ah = [(0.118, -0.13), (0.112, -0.03), (0.088, 0.07), (0.05, 0.14)]
    ear += A.chain(lambda i: HEAD, [E_(u, v, 0.008) for u, v in ah], [0.021, 0.022, 0.021, 0.019], k=0.012)
    ear += A.chain(lambda i: HEAD, [E_(0.05, 0.14, 0.008), E_(0.072, 0.21, 0.004)], [0.018, 0.013], k=0.01)
    ear += A.chain(lambda i: HEAD, [E_(0.05, 0.14, 0.008), E_(-0.008, 0.166, 0.006), E_(-0.06, 0.176, 0.004)], [0.017, 0.015, 0.011], k=0.01)
    ear.append(A.ell(CANAL, E_(-0.076, -0.035, 0.05), X, (0.022, 0.026, 0.032), up=Z, k=0.016))   # Tragus
    ear.append(A.ell(CANAL, E_(0.052, -0.112, 0.042), X, (0.02, 0.028, 0.022), up=Z, k=0.016))    # Antitragus
    ear.append(A.ell(CANAL, E_(-0.005, -0.035, 0.02), X, (0.03, 0.07, 0.072), up=Z, k=0.03))     # Wulst um den Gang (weitet sich)
    # Muschel (Schuessel) und Gehoergang (Mund) - tief in den Schaedelknorpel
    ear.append(A.ell(HEAD, E_(0.0, -0.032, 0.04), X, (0.056, 0.07, 0.078), up=Z, op='sub', k=0.022))
    ear.append(A.cone(CANAL, E_(0.0, -0.032, 0.09), E_(0.004, -0.012, -0.135), 0.041, 0.017, op='sub', k=0.012))
    # das Laeppchen: dick, schwer; darin ein verkuemmertes Gesicht (geschlossene Augen, Nase, zugewachsener Mund)
    lc = E_(-0.028, -0.272, 0.0)
    ear.append(A.ell(LOBE, lc, X, (0.046, 0.084, 0.076), up=Z, k=0.04))
    fc = lc + X * 0.043
    for g in (1, -1):
        ec = fc + Y * g * 0.022 + Z * 0.02
        ear.append(A.ell(LOBE, ec + X * 0.004, X, (0.007, 0.012, 0.0075), up=Z, op='sub', k=0.004))     # Augenhoehle
        ear.append(A.ell(LOBE, ec - X * 0.0015, X, (0.0062, 0.0105, 0.0062), up=Z, k=0.002))           # geschlossenes Lid
        ear.append(A.box(LOBE, ec + X * 0.0045, X, (0.002, 0.0105, 0.0006), 0.0004, up=Z, op='sub', k=0.0008))  # Lidspalt
    ear.append(A.ell(LOBE, fc + X * 0.004 - Z * 0.004, X, (0.009, 0.0065, 0.011), up=Z, k=0.005))         # Nasenstumpf
    for g in (1, -1):
        ear.append(A.ell(LOBE, fc + X * 0.008 - Z * 0.011 + Y * g * 0.0035, X, (0.003, 0.0022, 0.0018), up=Z, op='sub', k=0.001))
    ear.append(A.tube(LOBE, fc - Y * 0.016 - Z * 0.028 + X * 0.001, fc + Y * 0.016 - Z * 0.028 + X * 0.001, (0.0016, 0.0016), up=Z, op='sub', k=0.0015))
    for i in range(5):  # zugewachsen: Hautbruecken ueber dem Mundspalt
        y = -0.012 + i * 0.006
        ear.append(A.ell(LOBE, fc + Y * y - Z * 0.028 + X * 0.0025, Z, (0.003, 0.0016, 0.0012), up=X, k=0.0008))

    # Gehoergang: nasse rote Auskleidung (hinter der Haut eingebettet) und zwei Ringe kleiner Zaehne
    ca, cb = E_(0.0, -0.032, 0.052), E_(0.004, -0.012, -0.132)
    axis = A.unit(cb - ca)
    canal.append(A.cone(CANAL, ca, cb, 0.035 + 0.005, 0.018 + 0.004, k=0.0))
    canal.append(A.cone(CANAL, ca - axis * 0.02, cb + axis * 0.02, 0.031, 0.011, op='sub', k=0.004))
    u_ = A.unit(np.cross(axis, Z))
    v_ = A.unit(np.cross(u_, axis))
    for ring, (t, rad, ln, n) in enumerate(((0.12, 0.032, 0.017, 11), (0.42, 0.025, 0.013, 9))):
        cc = ca + (cb - ca) * t
        for i in range(n):
            ang = i / n * 2 * math.pi + ring * 0.3 + float(rng.uniform(-0.12, 0.12))
            rd = u_ * math.cos(ang) + v_ * math.sin(ang)
            base = cc + rd * rad
            tip = cc + rd * (rad - ln * float(rng.uniform(0.8, 1.15))) - axis * 0.004 * float(rng.uniform(0.0, 1.0))
            teeth.append(A.cone(CANAL, base, tip, 0.0048 * float(rng.uniform(0.85, 1.15)), 0.0006, k=0.0))

    # --- Adern: scheinen durch die duenne Haut von Ohr, Hals und Unterarm ------------------------------------------------------
    def vein(store, bone, pts_uv, project, r0, r1):
        P = []
        for p in pts_uv:
            q = project(p)
            if q is not None:
                P.append(q)
        if len(P) >= 3:
            n = len(P)
            store += A.chain(lambda i: bone, P, [r0 + (r1 - r0) * i / (n - 1) for i in range(n)], k=0.002)

    def walk(start, heading, n, step, turn, rs):
        pts, h = [np.array(start, dtype=float)], heading
        for _ in range(n):
            h += float(rs.uniform(-turn, turn))
            pts.append(pts[-1] + np.array([math.cos(h), math.sin(h)]) * step)
        return pts

    ear_shape = ear[:4]
    back_proj = lambda p: (lambda q: None if q is None else q - X * 0.0012)(surf(ear_shape, E_(p[0], p[1], -0.35), X, 0.4))
    rs = np.random.default_rng(1883)
    for k_ in range(7):  # hinten: vom Schaedelknorpel ueber die Platte zum Rand
        h0 = -math.pi / 2 + k_ * (math.pi / 6) + float(rs.uniform(-0.2, 0.2))
        st = (0.02 + 0.03 * math.cos(h0), -0.02 + 0.05 * math.sin(h0))
        pts = walk(st, h0, 9, 0.022, 0.35, rs)
        vein(veins_ear, HEAD, pts, back_proj, 0.0042, 0.0018)
        if k_ % 2 == 0:
            br = walk(pts[4], h0 + 0.7, 5, 0.018, 0.4, rs)
            vein(veins_ear, HEAD, br, back_proj, 0.003, 0.0014)
    front_shape = [p for p in ear if p.op == 'add' and p.bone in (HEAD, HELIX)] + [p for p in ear if p.op == 'sub' and p.bone == HEAD][:5]
    front_proj = lambda p: (lambda q: None if q is None else q + X * 0.0009)(surf(front_shape, E_(p[0], p[1], 0.3), -X, 0.38))
    for k_ in range(5):  # vorn: von der Muschel strahlenfoermig in die Platte
        h0 = 0.4 + k_ * 0.45 + float(rs.uniform(-0.15, 0.15))
        st = (0.0 + 0.085 * math.cos(h0), -0.03 + 0.1 * math.sin(h0))
        pts = walk(st, h0, 6, 0.02, 0.3, rs)
        vein(veins_ear, HEAD, pts, front_proj, 0.0026, 0.0012)
    # Unterarm der Faust: dicke, geschlaengelte Adern
    arm_shape = [p for p in skin if p.bone in (armB(1, 2), FB)]
    S, E, W, K = (J[armJ(1, i)] for i in range(4))
    ua, ub = A.unit(np.cross(dfo, Z)), None
    ub = A.unit(np.cross(ua, dfo))
    for k_ in range(5):
        ang0 = -1.2 + k_ * 0.6 + float(rs.uniform(-0.15, 0.15))
        pts = []
        for i in range(12):
            t = 0.15 + i * 0.075
            ang = ang0 + 0.25 * math.sin(i * 0.9 + k_)
            if t > 0.95:
                break
            pts.append((t, ang))
        def arm_proj(p, ang_shift=0.0):
            t, ang = p
            cc = E + (W - E) * t
            rd = ua * math.cos(ang) + ub * math.sin(ang)
            q = surf(arm_shape, cc + rd * 0.3, -rd, 0.32)
            return None if q is None else q + rd * 0.0015
        vein(veins_arm, armB(1, 2), pts, arm_proj, 0.0055, 0.0035)
    # Hals: zwei Adern an den Seiten
    neck_shape = [p for p in skin if p.bone in (NECK0, NECK1)]
    for g in (1, -1):
        pts = [(t, g) for t in np.linspace(0.05, 0.95, 9)]

        def neck_proj(p):
            t, gg = p
            q0 = N0 + (N1 - N0) * min(1.0, t * 2) if t < 0.5 else N1 + (N2 - N1) * (t - 0.5) * 2
            rd = A.unit(Y * gg + Z * 0.15 * math.sin(t * 7.0))
            q = surf(neck_shape, q0 + rd * 0.25, -rd, 0.27)
            return None if q is None else q + rd * 0.0012
        vein(veins_neck, NECK0, pts, neck_proj, 0.0045, 0.0035)

    # --- Schwielen, aufgeplatzte Haut und Splitter an den Knoecheln ------------------------------------------------------------
    for i, kp in enumerate(knuckles):
        callus.append(A.ell(FB, kp + d * 0.006 + back * 0.004, d, (0.034, 0.03, 0.028), up=back, k=0.006))
    callus.append(A.box(FB, W + d * 0.262 - back * 0.03, d, (0.022, 0.1, 0.05), 0.02, up=back, k=0.01))  # Schlagflaeche unten
    fist_shape = [p for p in skin if p.bone == FB] + callus
    rs2 = np.random.default_rng(1884)
    for i, kp in enumerate(knuckles):
        for j in range(2):  # Risse quer ueber den Knoechel, nass und rot
            o = kp + back * 0.06 + d * float(rs2.uniform(-0.01, 0.025))
            pts = []
            for m in range(4):
                q = surf(fist_shape, o + acr * (-0.022 + m * 0.015) + d * float(rs2.uniform(-0.004, 0.004)), -back, 0.12)
                if q is not None:
                    pts.append(q + back * 0.0006)
            if len(pts) >= 3:
                raw += A.chain(lambda i: FB, pts, [0.0025, 0.0045, 0.0042, 0.002][:len(pts)], k=0.002)
    for i in range(8):  # Mahagonisplitter, schraeg im Fleisch
        kp = knuckles[i % 4]
        o = kp + back * 0.07 + acr * float(rs2.uniform(-0.025, 0.025)) + d * float(rs2.uniform(-0.035, 0.02))
        q = surf(fist_shape, o, -back, 0.12)
        if q is None:
            continue
        n = normal_at(fist_shape, q)
        tilt = A.unit(n + acr * float(rs2.uniform(-0.6, 0.6)) + d * float(rs2.uniform(-0.5, 0.3)))
        ln = float(rs2.uniform(0.022, 0.05))
        wood.append(A.cone(FB, q - tilt * 0.008, q + tilt * ln, 0.0032, 0.0007, k=0.0))
        raw.append(A.ell(FB, q + n * 0.0005, n, (0.0016, 0.0045, 0.0045), k=0.0))  # Wundrand

    # --- Oberflaechen ------------------------------------------------------------------------------------------------------
    def chalk(Q):  # trockene, kreidige Haut: feine Poren, Knitter an Hals und Gelenken
        # Runde 1: grobe Mulden lasen sich mit der Hoehlung als braune Flecken (Tarnmuster) - feiner und flacher
        return -0.00016 * sdf.fbm(Q, 220, 2, seed=1885) - 0.0001 * sdf.fbm(Q, 520, 1, seed=1886) + 0.0001 * sdf.ridged(Q * np.array([1.0, 1.0, 0.5]), 90, 2, seed=1887)

    def cartilage(Q):  # duenne Haut ueber Knorpel: kaum Poren, zarte Falten
        return -0.00018 * sdf.fbm(Q, 300, 2, seed=1888) + 0.00018 * sdf.ridged(Q, 110, 2, seed=1889)

    def cracked(Q):  # Schwiele: hart, rissig
        d1, d2 = sdf.worley(Q, 70, seed=1890)
        return -0.0011 * np.exp(-((d2 - d1) * 26.0) ** 2) + 0.0005 * sdf.fbm(Q, 160, 2, seed=1891)

    def wet(Q):
        return -0.00025 * sdf.fbm(Q, 420, 2, seed=1892)

    return [
        {'name': 'haut', 'fm': 'ent_hr_skin', 'parts': skin, 'disp': chalk, 'h': 0.0034, 'cav_scale': 0.008, 'cav_extra': callus},
        {'name': 'ohr', 'fm': 'ent_hr_ear', 'parts': ear, 'disp': cartilage, 'h': 0.0016, 'cav_scale': 0.006, 'lod_weight': 2.2},
        {'name': 'gang', 'fm': 'ent_hr_canal', 'parts': canal, 'disp': wet, 'h': 0.0012, 'cav_scale': 0.004, 'lod_weight': 1.4},
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.0007, 'cav_scale': 0.0015, 'lod_weight': 1.2},
        {'name': 'schwielen', 'fm': 'ent_hr_callus', 'parts': callus, 'disp': cracked, 'h': 0.0018, 'cav_scale': 0.005},
        {'name': 'risse', 'fm': 'ent_hr_raw', 'parts': raw, 'disp': wet, 'h': 0.0009, 'cav_scale': 0.002, 'merge': 'roh'},
        {'name': 'naegel', 'fm': 'ent_hr_nail', 'parts': nails, 'disp': None, 'h': 0.0008, 'cav_scale': 0.002, 'merge': 'naegel'},
        {'name': 'adern_ohr', 'fm': 'ent_hr_vein', 'parts': veins_ear, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002, 'merge': 'adern'},
        {'name': 'adern_arm', 'fm': 'ent_hr_vein', 'parts': veins_arm, 'disp': None, 'h': 0.0016, 'cav_scale': 0.003, 'merge': 'adern'},
        {'name': 'adern_hals', 'fm': 'ent_hr_vein', 'parts': veins_neck, 'disp': None, 'h': 0.0014, 'cav_scale': 0.003, 'merge': 'adern'},
        {'name': 'splitter', 'fm': 'ent_hr_wood', 'parts': wood, 'disp': None, 'h': 0.0008, 'cav_scale': 0.002},
    ]
