"""V13.2: Baukasten fuer menschenaehnliche Wesen (Faceling, Skin-Stealer, Wretch) auf dem Zweibeiner-Skelett (bp in
src/anim/creature.hpp). Liefert Teile (sdf.Part) fuer Haut, Kleidung und Haar; die Proportionen kommen aus der Ruhepose
(skeletons.json), Dicken aus den Optionen. Ruhepose: aufrecht, Arme 35-40 Grad abgespreizt, Beine gerade.

Gelenke (J): 0 Wurzel, 1 Becken, 2 Ruecken, 3 Brust, 4 Hals, 5 Kopf, 6 Scheitel, 7 Kinn,
  8-11 Schulter, Ellbogen, Handgelenk, Fingerspitzen links (+y), 12-15 rechts, 16-19 Huefte, Knie, Knoechel, Zehen links,
  20-23 rechts.
Knochen: 0 Wurzel, 1 Becken, 2 Ruecken, 3 Brust, 4 Hals, 5 Kopf, 6 Kiefer, 7-10 Schluesselbein, Oberarm, Unterarm, Hand
  links, 11-14 rechts, 15-17 Oberschenkel, Unterschenkel, Fuss links, 18-20 rechts.
"""
import math

import numpy as np

import anat as A

# Knochen
ROOT, PELVIS, SPINE, CHEST, NECK, HEAD, JAW = range(7)
CLAV, UPPER, FORE, HAND = 7, 8, 9, 10   # + 4 * s
THIGH, SHIN, FOOT = 15, 16, 17          # + 3 * s
# Gelenke
jPELVIS, jSPINE, jCHEST, jNECK, jHEAD, jTOP, jJAW = 1, 2, 3, 4, 5, 6, 7
jSH, jEL, jWR, jHAND = 8, 9, 10, 11     # + 4 * s
jHIP, jKNEE, jANK, jTOE = 16, 17, 18, 19  # + 4 * s
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def side(s):
    return 1.0 if s == 0 else -1.0


class Body:
    """Masse aus der Ruhepose; o: Optionen (Dicken)."""

    def __init__(self, J, **o):
        self.J = np.asarray(J)
        self.o = dict(slim=1.0, muscle=1.0, belly=1.0, chest=1.0, hips=1.0, head=1.0, neck=1.0, hand=1.0, gaunt=0.0)
        self.o.update(o)
        J = self.J
        self.hip = J[jPELVIS][2]
        self.waist = J[jSPINE][2]
        self.chestZ = J[jCHEST][2]
        self.shZ = J[jSH][2]
        self.neckTop = J[jHEAD][2]
        self.top = J[jTOP][2]
        self.headH = self.top - self.neckTop
        self.shW = abs(J[jSH][1])
        self.hipW = abs(J[jHIP][1])
        # Streckung gegenueber dem Grundmass (Faceling: Rumpf 0,51 m, Oberarm 0,29, Unterarm 0,25, Oberschenkel 0,44,
        # Unterschenkel 0,42): laengere Glieder bekommen laengere Muskeln, ein laengerer Rumpf hoehere Wuelste
        self.sz = (self.shZ - self.hip) / 0.51
        self.la = np.linalg.norm(J[jEL] - J[jSH]) / 0.29, np.linalg.norm(J[jWR] - J[jEL]) / 0.25
        self.ll = np.linalg.norm(J[jKNEE] - J[jHIP]) / 0.44, np.linalg.norm(J[jANK] - J[jKNEE]) / 0.42
        # Kopfmitte: etwas ueber dem Halsende, leicht vorn
        hh = self.headH * self.o['head']
        self.headC = np.array([0.012, 0.0, self.neckTop + 0.52 * self.headH])
        self.headR = np.array([0.098, 0.078, 0.112]) * (self.headH / 0.15) * self.o['head']

    def j(self, i, s=0):
        return self.J[i + 4 * s] if i >= 8 else self.J[i]

    # --- Haut und Grundform ------------------------------------------------------------------------------------
    def head(self, t=0.0, bone=HEAD, featureless=True):
        """Schaedel, Gesichtsschaedel ohne Zuege (Faceling), Kiefer, Kinn. t: Aufdickung (Haar-, Kleidungslagen)."""
        c, r = self.headC, self.headR
        P = [A.ell(bone, c + Z * r[2] * 0.12, X, (r[0] + t, r[1] + t, r[2] * 0.86 + t), k=0.0),
             A.ell(bone, c - X * r[0] * 0.45 + Z * 0.01, X, (r[0] * 0.62 + t, r[1] * 0.9 + t, r[2] * 0.62 + t), k=0.03),   # Hinterkopf
             A.ell(bone, c + X * r[0] * 0.42 - Z * r[2] * 0.30, X, (r[0] * 0.62 + t, r[1] * 0.78 + t, r[2] * 0.66 + t), k=0.035)]  # Gesicht
        P.append(A.ell(JAW, c + X * r[0] * 0.55 - Z * r[2] * 0.78, X, (r[0] * 0.40 + t, r[1] * 0.42 + t, r[2] * 0.26 + t), k=0.03))  # Kinn
        for s in (0, 1):
            g = side(s)
            P.append(A.ell(JAW, c + X * r[0] * 0.05 + Y * g * r[1] * 0.70 - Z * r[2] * 0.55, X, (r[0] * 0.45 + t, r[1] * 0.30 + t, r[2] * 0.30 + t), k=0.03))
            if featureless:  # Wangen und Brauenbogen nur als sanfte Woelbung (keine Augenhoehlen)
                P.append(A.ell(bone, c + X * r[0] * 0.70 + Y * g * r[1] * 0.42 - Z * r[2] * 0.12, X, (r[0] * 0.26 + t, r[1] * 0.32 + t, r[2] * 0.26 + t), k=0.03))
        return P

    def neck(self, t=0.0):
        n0 = np.array([-0.005, 0, self.shZ - 0.02])
        n1 = np.array([0.0, 0, self.neckTop + 0.02])
        r = 0.052 * self.o['neck'] * self.o['slim']
        return [A.cone(NECK, n0, n1, r + t + 0.004, r * 0.92 + t, k=0.04)]

    def torso(self, t=0.0, upper=True, lower=True):
        o = self.o
        sl, m, g = o['slim'], o['muscle'], o['gaunt']
        q = self.sz
        P = []
        if upper:
            P += [A.ell(CHEST, (0.0, 0, self.chestZ), (1, 0, 0.05), (0.105 * sl * o['chest'] + t, 0.150 * sl + t, 0.175 * q + t), k=0.07),
                  A.ell(CHEST, (-0.01, 0, self.shZ - 0.05), (0, 1, 0), (self.shW * 0.95 + t, 0.085 * sl + t, 0.07 + t), up=(1, 0, 0), k=0.06)]  # Schulterguertel
            for s in (0, 1):
                gs = side(s)
                P.append(A.ell(CHEST, (0.045 * sl, gs * 0.07, self.chestZ + 0.06), (1, 0, -0.3), (0.045 * m + t, 0.07 + t, 0.05 + t), k=0.05))  # Brustmuskel
                P.append(A.ell(CHEST, (-0.05, gs * 0.09, self.chestZ - 0.04 * q), (0.2, gs * 0.3, 1), (0.12 * q + t, 0.05 * m + t, 0.035 + t), up=(-1, 0, 0), k=0.045))  # Ruecken
                P.append(A.cone(CHEST, (-0.012, gs * 0.03, self.shZ + 0.11), (-0.012, gs * self.shW * 0.92, self.shZ + 0.01), 0.038 + t, 0.03 + t, k=0.045))  # Nacken
        if lower:
            P += [A.ell(SPINE, (0.006, 0, self.waist), (1, 0, 0), (0.088 * sl * o['belly'] + t, 0.125 * sl + t, 0.13 * q + t), k=0.06),
                  A.ell(PELVIS, (0.0, 0, self.hip + 0.03 * q), (1, 0, 0), (0.098 * o['hips'] + t, (self.hipW + 0.075) * o['hips'] + t, 0.105 * q + t), k=0.06)]
            for s in (0, 1):
                gs = side(s)
                P.append(A.ell(PELVIS, (-0.05, gs * 0.07, self.hip - 0.035), (1, 0, 0), (0.065 + t, 0.072 * o['hips'] + t, 0.085 + t), k=0.045))  # Gesaess
            if g > 0:  # ausgezehrt: Rippen und eingefallener Bauch
                pass
        return P

    def arm(self, s, t=0.0, upper=True, fore=True, hand=True, sleeve_to=1.0):
        """Ein Arm. sleeve_to: bis wohin (0..1 vom Ellbogen zum Handgelenk) der Unterarm reicht (Aermel)."""
        o = self.o
        S, E, W, H = self.j(jSH, s), self.j(jEL, s), self.j(jWR, s), self.j(jHAND, s)
        m, sl = o['muscle'], o['slim']
        P = []
        du, df = A.unit(E - S), A.unit(W - E)
        if upper:
            P.append(A.ell(UPPER + 4 * s, S + Y * side(s) * 0.01, du, (0.06 + t, 0.052 * m + t, 0.058 + t), up=X, k=0.04))  # Deltamuskel
            P.append(A.cone(UPPER + 4 * s, S, E, 0.043 * sl + t, 0.034 * sl + t, k=0.04))
            P.append(A.ell(UPPER + 4 * s, A.lerp(S, E, 0.5) + X * 0.01, du, (0.11 * self.la[0], 0.034 * m * sl + t, 0.036 * m * sl + t), up=X, k=0.03))
        if fore:
            Wc = A.lerp(E, W, sleeve_to)
            P.append(A.cone(FORE + 4 * s, E, Wc, 0.034 * sl + t, (0.034 + (0.022 - 0.034) * sleeve_to) * sl + t, k=0.03))
            P.append(A.ell(FORE + 4 * s, A.lerp(E, W, 0.28), df, (0.085 * self.la[1] * min(1.0, sleeve_to + 0.3), 0.032 * m * sl + t, 0.028 * m * sl + t), up=X, k=0.03))
        if hand:
            P += self.hand(s, t)
        return P

    def hand(self, s, t=0.0, curl=0.35, claw=0.0):
        """Hand mit vier Fingern und Daumen, leicht gekruemmt; claw: lange, spitze Finger."""
        o = self.o
        W, H = self.j(jWR, s), self.j(jHAND, s)
        hd = A.unit(H - W)
        L = np.linalg.norm(H - W) * o['hand']
        palmN = A.unit(np.cross(hd, np.array([0, side(s), 0.0])) if abs(hd[1]) < 0.9 else X)
        # Handflaeche zeigt nach innen (zum Koerper): Normale ~ -y*side
        inward = A.unit(-Y * side(s) - hd * float((-Y * side(s)) @ hd))
        across = A.unit(np.cross(inward, hd))
        P = [A.ell(HAND + 4 * s, W + hd * L * 0.25, hd, (L * 0.27 + t, 0.04 * o['hand'] + t, 0.014 + t), up=inward, k=0.015)]
        for f in range(4):
            off = (f - 1.5) * 0.017 * o['hand']
            base = W + hd * L * 0.48 + across * off
            fl = L * (0.48 - abs(f - 1.3) * 0.05) * (1.0 + claw)
            p1 = base + hd * fl * 0.42 + inward * fl * 0.08 * curl
            p2 = p1 + A.unit(hd + inward * curl * 0.9) * fl * 0.32
            p3 = p2 + A.unit(hd + inward * curl * 1.8) * fl * 0.26
            rr = 0.0085 * o['hand'] + t
            P += A.chain(lambda i: HAND + 4 * s, [base, p1, p2, p3], [rr, rr * 0.92, rr * 0.82, rr * (0.3 if claw > 0 else 0.7)], k=0.006)
        tb = W + hd * L * 0.12 + across * 0.022 * o['hand'] + inward * 0.012
        t1 = tb + A.unit(hd + inward * 0.9 + across * 0.3) * 0.035
        t2 = t1 + A.unit(hd + inward * 0.5) * 0.03
        P += A.chain(lambda i: HAND + 4 * s, [tb, t1, t2], [0.012 + t, 0.0095 + t, 0.007 + t], k=0.008)
        return P

    def leg(self, s, t=0.0, thigh=True, shin=True, to=1.0):
        """Ein Bein (bis zum Knoechel). to: Anteil des Unterschenkels (Hose/Rock)."""
        o = self.o
        Hh, K, An = self.j(jHIP, s), self.j(jKNEE, s), self.j(jANK, s)
        m, sl = o['muscle'], o['slim']
        P = []
        dt, ds = A.unit(K - Hh), A.unit(An - K)
        if thigh:
            P.append(A.cone(THIGH + 3 * s, Hh + Z * 0.02, K, 0.078 * sl * o['hips'] + t, 0.05 * sl + t, k=0.05))
            P.append(A.ell(THIGH + 3 * s, A.lerp(Hh, K, 0.42) + X * 0.022, dt, (0.17 * self.ll[0], 0.058 * m * sl + t, 0.056 * m * sl + t), up=X, k=0.04))
            P.append(A.ell(THIGH + 3 * s, K + X * 0.035 + Z * 0.01, dt, (0.03 + t, 0.028 + t, 0.02 + t), up=X, k=0.02))  # Kniescheibe
        if shin:
            Ae = A.lerp(K, An, to)
            P.append(A.cone(SHIN + 3 * s, K, Ae, 0.048 * sl + t, (0.048 + (0.029 - 0.048) * to) * sl + t, k=0.035))
            P.append(A.ell(SHIN + 3 * s, A.lerp(K, An, 0.26 * min(1.0, 1.0 / self.ll[1] + 0.2)) - X * 0.025, ds, (0.11 * self.ll[1] * min(1.0, to + 0.3), 0.043 * m * sl + t, 0.04 * m * sl + t), up=-X, k=0.03))
        return P

    def foot(self, s, t=0.0, bare=False):
        An, T = self.j(jANK, s), self.j(jTOE, s)
        fd = A.unit(np.array([T[0] - An[0], T[1] - An[1], 0.0]))
        heel = np.array([An[0] - 0.045, An[1], 0.035])
        ball = np.array([T[0] - 0.02, T[1], 0.03])
        P = [A.cone(FOOT + 3 * s, An + Z * 0.01, heel, 0.035 + t, 0.032 + t, k=0.02),
             A.ell(FOOT + 3 * s, A.lerp(heel, ball, 0.5) + Z * 0.005, fd, (np.linalg.norm(ball - heel) * 0.58 + t, 0.045 + t, 0.032 + t), k=0.025),
             A.ell(FOOT + 3 * s, ball + fd * 0.03, fd, (0.05 + t, 0.048 + t, 0.026 + t), k=0.02)]
        if not bare:  # Schuh: flache Sohle
            P.append(A.box(FOOT + 3 * s, A.lerp(heel, ball, 0.55) + Z * (-0.024) + fd * 0.02, fd, (np.linalg.norm(ball - heel) * 0.62 + 0.03, 0.052, 0.009), 0.006, k=0.01))
        return P

    def face(self):
        """Menschliches Gesicht: Augenhoehlen mit Lidern, Brauenbogen, Nase, Lippen, Ohren. Liefert (Haut, Augen,
        Brauen, Mund): Haut kommt zur Kopfhaut, Augen und Mundinneres sind eigene Gruppen."""
        c, r = self.headC, self.headR
        skin, eyes, brows = [], [], []
        for s in (0, 1):
            g = side(s)
            e = c + X * r[0] * 0.80 + Y * g * r[1] * 0.40 + Z * r[2] * 0.04
            skin.append(A.ell(HEAD, e + X * 0.013, X, (0.019, 0.02, 0.0135), op='sub', k=0.01))         # Hoehle
            skin.append(A.ell(HEAD, e + X * 0.004 + Z * 0.0055, X, (0.0135, 0.0165, 0.0068), k=0.003))  # Oberlid
            skin.append(A.ell(HEAD, e + X * 0.004 - Z * 0.0075, X, (0.012, 0.015, 0.004), k=0.003))     # Unterlid
            skin.append(A.ell(HEAD, e + X * 0.006 + Z * 0.021, X, (0.014, 0.024, 0.008), k=0.012))      # Brauenbogen
            eyes.append(A.ell(HEAD, e - X * 0.0015, X, (0.0125, 0.0125, 0.0125), k=0.0))
            brows.append(A.ell(HEAD, e + X * 0.013 + Z * 0.025 + Y * g * 0.003, X - Y * g * 0.15, (0.004, 0.021, 0.0035), k=0.002))
            ear = c + Y * g * r[1] * 0.97 - Z * r[2] * 0.06 - X * 0.014
            skin.append(A.ell(HEAD, ear, Y * g, (0.009, 0.021, 0.031), up=Z, k=0.008))
            skin.append(A.ell(HEAD, ear + Y * g * 0.006 + X * 0.002, Y * g, (0.004, 0.012, 0.02), up=Z, op='sub', k=0.003))
        # Nase: Ruecken, Spitze, Fluegel, Loecher
        root = c + X * r[0] * 0.93 + Z * r[2] * 0.0
        tip = c + X * r[0] * 1.13 - Z * r[2] * 0.26
        skin.append(A.cone(HEAD, root, tip, 0.008, 0.011, k=0.012))
        for s in (0, 1):
            g = side(s)
            skin.append(A.ell(HEAD, tip - X * 0.011 + Y * g * 0.012 - Z * 0.004, X, (0.009, 0.007, 0.007), k=0.006))
            skin.append(A.ell(HEAD, tip - X * 0.006 + Y * g * 0.006 - Z * 0.01, Z, (0.003, 0.004, 0.003), op='sub', k=0.002))
        # Lippen: Oberlippe am Kopf, Unterlippe am Kiefer, dazwischen ein Spalt
        m = c + X * r[0] * 0.98 - Z * r[2] * 0.45
        skin.append(A.ell(HEAD, m + Z * 0.0055, X, (0.008, 0.025, 0.0058), k=0.006))
        skin.append(A.ell(JAW, m - Z * 0.0065, X, (0.0085, 0.023, 0.0068), k=0.006))
        skin.append(A.ell(HEAD, m + X * 0.004, X, (0.009, 0.022, 0.0011), op='sub', k=0.002))
        mouth = [A.ell(JAW, m - X * 0.008, X, (0.006, 0.02, 0.006), k=0.0)]
        return skin, eyes, brows, mouth

    def hair_short(self, t=0.012, bone=HEAD):
        """Kurzes Haar als Kappe ueber Scheitel und Hinterkopf (gerader Ansatz an Stirn und Nacken)."""
        c, r = self.headC, self.headR
        P = [A.ell(bone, c + Z * r[2] * 0.2 - X * 0.008, X, (r[0] + t, r[1] + t * 0.9, r[2] * 0.82 + t), k=0.0),
             A.ell(bone, c - X * r[0] * 0.5 + Z * 0.01, X, (r[0] * 0.6 + t, r[1] * 0.88 + t, r[2] * 0.6 + t), k=0.03)]
        # Gesicht frei: alles vor der Stirn und unter dem Ansatz abziehen
        P.append(A.box(bone, c + X * (r[0] * 0.95) - Z * r[2] * 0.25, X, (r[0] * 0.55, r[1] * 1.3, r[2] * 0.75), 0.03, op='sub', k=0.02))
        P.append(A.box(bone, c - Z * r[2] * 0.95, X, (r[0] * 1.4, r[1] * 1.4, r[2] * 0.45), 0.03, op='sub', k=0.03))
        return P

    def hair_long(self, rng, length=0.38, bone=HEAD, t=0.014):
        """Langes Haar: Kappe und Straehnen ueber Ruecken und Schultern (an Kopf und Brust gebunden)."""
        c, r = self.headC, self.headR
        P = self.hair_short(t=t, bone=bone)[:2]
        P.append(A.box(bone, c + X * (r[0] * 1.0) - Z * r[2] * 0.3, X, (r[0] * 0.5, r[1] * 1.3, r[2] * 0.7), 0.03, op='sub', k=0.02))
        for k in range(14):
            a = -2.2 + 4.4 * k / 13
            p0 = c + np.array([math.cos(a) * r[0] * 0.95 - 0.02, math.sin(a) * r[1] * 1.0, r[2] * 0.3])
            if p0[0] > c[0] + r[0] * 0.25:
                continue
            ln = length * (0.85 + 0.3 * rng.random())
            p1 = p0 + np.array([-0.03 - 0.02 * rng.random(), math.sin(a) * 0.035, -ln * 0.45])
            p2 = p1 + np.array([-0.015, math.sin(a) * 0.02, -ln * 0.55])
            b2 = CHEST if p2[2] < self.shZ else bone
            P += A.chain(lambda i, b2=b2: bone if i == 0 else b2, [p0, p1, p2], [0.03, 0.024, 0.008], k=0.02)
        return P
