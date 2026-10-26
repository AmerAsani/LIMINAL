"""V13.2: Deathmoth-Nest (Entity 4) - Neuentwurf. Grundlage ist die Entity-4-Seite (Revision 6, 31.01.2026; DECISIONS E-026):
Nester sitzen in vorhandener Architektur, an Raeumen mit wenigen Zugaengen; Schichten aus verdautem Material,
Pflanzenfasern und Harz, 60-100 cm dick, feucht und halb organisch; Saeulen aus unverdaulichen Resten (Knochen, Holz,
Metall), mit derselben Masse ueberzogen; eine fleischige obere Schicht; Eier an der Auskleidung, in Saeulen gestapelt;
Larven verpuppen sich in den Waenden; Knochenreste rund um den Stock; Warnzeichen: fleischige Wucherungen ueber Tueren.

Lage im Spiel (Presence::spawnHive): Wurzel auf der Zimmerseite der Tueroeffnung, +X ins dunkle Zimmer. Die Tuer ist
eine Kachel tief: Laibung x -1..0, |y| <= 0.5, z 0..2.15; Wandflaeche Zimmer x = 0, Wandflaeche Flur x = -1.
Aufbau:
  Flur      die Warnung: eine fleischige Wucherung quillt ueber dem Sturz aus der Oeffnung und kriecht in Adern auf die
            Flurwand (von weitem im hellen Flur zu sehen - das alte Nest war von dort ohne Lampe unsichtbar)
  Laibung   Kruste auf Seiten und Sturz, verengt die Oeffnung; Eier in senkrechten Saeulen; Harz tropft vom Sturz
  Schleier  eine zerrissene Lage aus Fasern und Harz ueber der Oeffnung (Zimmerseite), darunter durchhaengende Faeden
  Zimmer    der Kragen: Schichten bis 0,9 m tief um die Oeffnung, oben eine fleischige Schicht; zwei Saeulen aus Resten;
            Puppen in der Masse, einige aufgeplatzt
  Boden     Knochen vor der Tuer, halb in Harz
Knochen (ent_nest): 0 Wurzel, 1 Masse, 2 Haut (Angel am Sturz), 3 Wucherung im Flur, 4-7 atmende Saecke, 8 Gelege.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (46000, 14000, 4200)
ROOT, BODY, MEMB, LIP, SAC0, SAC1, SAC2, SAC3, EGGS = range(9)
H, HW = 2.15, 0.5          # Tueroeffnung: Hoehe, halbe Breite
DEPTH = 1.0                # Laibung (eine Kachel)
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def build(J, bones):
    rng = np.random.default_rng(13)
    R = rng.random

    def jit(s):
        return (rng.random(3) - 0.5) * 2 * s

    kragen, kragen_fl, flur, flur_fl, laib_l, laib_r, laib_o = [], [], [], [], [], [], []
    haut, saeule, puppen, eier, knochen, harz, fasern, metall = [], [], [], [], [], [], [], []

    # ================= Zimmer: Kragen um die Oeffnung ==================================================================
    # Ring aus uebereinander gegossenen Lagen: an der Oeffnung am dicksten (0,6-0,9 m ins Zimmer), nach aussen flach
    # auslaufend; die Oeffnung setzt sich als unregelmaessiger Schlund in die Masse fort
    def ring_point(t):
        """t in [0,1): Umlauf um die Tuer (Rechteck mit runden Ecken), Punkt am Rand der Oeffnung und Aussen-Normale."""
        per = 2 * H + 2 * HW * 2
        s = (t % 1.0) * per
        if s < H:                       # rechte Seite (y = -HW) von unten nach oben
            return np.array([0, -HW, s]), -Y
        s -= H
        if s < 2 * HW:                  # Sturz von rechts nach links
            return np.array([0, -HW + s, H]), Z
        s -= 2 * HW
        if s < H:                       # linke Seite von oben nach unten
            return np.array([0, HW, H - s]), Y
        s -= H
        return np.array([0, HW - s, 0]), -Z   # Boden (wird nicht gebaut)

    for i in range(84):
        t = (i + 0.5 * R()) / 84
        p, nrm = ring_point(t)
        if p[2] < 0.02 and nrm[2] < 0:
            continue
        h01 = p[2] / H
        depth = 0.35 + 0.45 * h01 + 0.12 * R()          # oben am tiefsten (fleischige obere Schicht)
        for layer in range(3):                           # drei Lagen nach aussen, jede flacher
            off = 0.10 + 0.17 * layer + 0.05 * R()
            c = p + nrm * off + X * (depth * (0.55 - 0.15 * layer)) + jit(0.03)
            r = np.array([depth * (0.55 - 0.13 * layer), 0.13 + 0.06 * R() - 0.025 * layer, 0.11 + 0.05 * R()])
            up = A.unit(Z if abs(nrm[2]) < 0.5 else X)
            kragen.append(A.ell(BODY, c, A.unit(X + 0.25 * nrm), tuple(r), up=up, k=0.07))
    # flach auf die Zimmerwand gelaufene Auslaeufer (duenn, Adern) bis 1,3 m von der Oeffnung
    for k in range(28):
        sg = 1 if R() < 0.5 else -1
        a = np.array([0.04, sg * (HW + 0.35 + 0.2 * R()), 0.3 + 2.2 * R()])
        ang = R() * 2 * math.pi
        dirw = np.array([0.0, sg * abs(math.cos(ang)) * 0.9, math.sin(ang)])
        b = a + dirw * (0.35 + 0.5 * R())
        m = A.lerp(a, b, 0.5) + np.array([0.01, 0, 0]) + jit(0.06) * np.array([0, 1, 1])
        pts = A.bezier(a, m, b, 6)
        for q in range(6):  # flach auf der Wand (Band, nicht Stachel)
            w0, w1 = 0.045 * (1 - q / 6) + 0.01, 0.045 * (1 - (q + 1) / 6) + 0.01
            kragen.append(A.tube(BODY, pts[q], pts[q + 1], (w0, 0.014 * w0 / 0.055 + 0.004), (w1, 0.014 * w1 / 0.055 + 0.004), up=X, k=0.02))
    # Schlund: die Oeffnung setzt sich als Hoehle in die Masse fort (abgezogen)
    for j in range(7):
        c = np.array([0.05 + 0.11 * j, 0.03 * math.sin(j * 1.3), 1.0 + 0.04 * j])
        kragen.append(A.ell(BODY, c, X, (0.16, HW - 0.05 - 0.035 * j, 1.0 - 0.07 * j), op='sub', k=0.06))
    # fleischige obere Schicht: weiche, nasse Lappen ueber dem Schlund, Saecke 0 und 2 atmen
    for k in range(16):
        c = np.array([0.15 + 0.55 * R(), -0.9 + 1.8 * R(), H + 0.15 + 0.55 * R()])
        r = 0.10 + 0.08 * R()
        bone = SAC2 if (c[1] > -0.2 and c[1] < 0.6 and c[2] > H + 0.35) else BODY
        kragen_fl.append(A.ell(bone, c, A.unit(X + jit(0.4)), (r * 1.3, r * 1.1, r * 0.8), k=0.05))
    # Runde 4: keine Einzellappen am oberen Rand des Schlunds (lasen sich als Reisszaehne), sondern eine breite,
    # durchhaengende Falte, die in die Masse uebergeht
    # Runde 5: zwei versetzte, ungleich dicke Lagen statt einer glatten Rolle (las sich als Lippe)
    for k in range(6):
        y = -0.36 + 0.72 * (k + R()) / 6
        sag = 0.05 * (1 - (y / 0.45) ** 2)
        z = H - 0.02 - sag - (0.06 if k % 2 else 0.0)
        kragen.append(A.ell(BODY, (0.18 + 0.12 * R(), y, z), A.unit(Y + jit(0.3)), (0.13, 0.08 + 0.05 * R(), 0.045 + 0.03 * R()), up=Z, k=0.07))
    # atmende Saecke im Kragen (Knochen 4 und 5)
    for bone, c in ((SAC0, J[6]), (SAC1, J[8])):
        kragen_fl.append(A.ell(bone, c, X, (0.16, 0.14, 0.2), k=0.06))
        for q in range(5):
            kragen_fl.append(A.ell(bone, c + jit(0.09) + X * 0.05, A.unit(X + jit(0.3)), (0.06, 0.05, 0.07), k=0.04))

    # ================= Flur: Warnung ueber der Tuer =====================================================================
    fx = -DEPTH
    # Hauptwulst (Runde 3): quillt aus der oberen linken Ecke der Oeffnung und laeuft nach rechts duenner aus - eine
    # schwere Masse statt einer gleichmaessigen Wolke; ein Teil rollt unter dem Sturz heraus ueber die Kante
    flur_fl.append(A.ell(LIP, (fx - 0.05, 0.3, H + 0.14), A.unit(-X + 0.2 * Y), (0.12, 0.29, 0.21), up=Z, k=0.08))
    flur_fl.append(A.ell(BODY, (fx - 0.04, 0.52, H - 0.06), A.unit(-X + 0.1 * Z), (0.1, 0.16, 0.22), up=Z, k=0.08))
    for k in range(11):
        t = k / 10
        y = 0.5 - 1.1 * t + 0.08 * (R() - 0.5)
        r = 0.045 + 0.13 * (1 - t) * (0.4 + 0.6 * R())
        z = H + 0.03 + r * 0.6 + 0.22 * (1 - t) * R() + 0.06 * (R() - 0.5)
        bone = LIP if abs(y) < 0.35 else BODY
        flur_fl.append(A.ell(bone, (fx - 0.01 - r * 0.25, y, z), A.unit(-X + jit(0.25)), (r * 0.45, r * (0.9 + 0.6 * R()), r * 0.8), up=Z, k=0.06))
        if R() < 0.6:  # kleine Auflage (Schichtung)
            q = r * (0.35 + 0.25 * R())
            flur_fl.append(A.ell(bone, (fx - 0.01 - r * 0.45, y + (R() - 0.5) * r, z + r * 0.4), A.unit(-X + jit(0.3)), (q * 0.5, q, q * 0.7), up=Z, k=0.04))
    for k in range(4):  # unter dem Sturz hervorgequollen (Runde 5: kleiner, in den Wulst uebergehend)
        y = -0.1 + 0.55 * R() ** 0.6
        flur_fl.append(A.ell(LIP, (fx + 0.02, y, H - 0.02 - 0.03 * R()), A.unit(X + jit(0.2)), (0.07, 0.06 + 0.04 * R(), 0.045), up=Z, k=0.06))
    # Ablaeufe an den Pfosten: zusammenhaengende, flache Stroeme, nach unten schmaler, am Ende ein Tropfen
    for sg, ln in ((1, 0.7), (-1, 0.4)):
        a = np.array([fx - 0.02, sg * (HW + 0.05), H + 0.06])
        b = a + np.array([0.0, sg * 0.03, -ln])
        pts = A.bezier(a, A.lerp(a, b, 0.5) + np.array([0, sg * 0.04, 0]), b, 8)
        for q in range(8):
            w0, w1 = 0.085 - 0.055 * q / 8, 0.085 - 0.055 * (q + 1) / 8
            flur_fl.append(A.tube(BODY, pts[q], pts[q + 1], (w0, 0.016 * w0 / 0.085 + 0.006), (w1, 0.016 * w1 / 0.085 + 0.006), up=X, k=0.03))
        flur_fl.append(A.ell(BODY, b - Z * 0.02, Z, (0.045, 0.035, 0.03), up=X, k=0.03))

    # Adern auf der Flurwand: flach, gewunden (Zufallsweg statt Strahlen), zum Ende duenn, verzweigt
    def walk(a, d, ln, step=0.07, turn=0.5):
        pts, p = [a.copy()], a.copy()
        for _ in range(max(3, int(ln / step))):
            ang = (R() - 0.5) * turn
            ca, sa = math.cos(ang), math.sin(ang)
            d = A.unit(np.array([0.0, d[1] * ca - d[2] * sa, d[1] * sa + d[2] * ca]))
            p = p + d * step
            p[2] = max(p[2], 0.3)
            pts.append(p.copy())
        return pts, d

    for k in range(20):
        if k < 13:  # vom Wulst nach oben und zu den Seiten (links mehr)
            ang = math.pi * (0.02 + 0.96 * R() ** 0.8)
            d = np.array([0.0, math.cos(ang), math.sin(ang)])
            a = np.array([fx, 0.15 + 0.35 * d[1], H + 0.18 + 0.12 * d[2]])
        else:  # neben der Tuer herab (nie quer ueber die Oeffnung)
            sg = 1 if k % 3 else -1
            d = A.unit(np.array([0.0, sg * (0.15 + 0.35 * R()), -1.0]))
            a = np.array([fx, sg * (HW + 0.12 + 0.08 * R()), H + 0.05])
        pts, d = walk(a, d, 0.3 + 0.6 * R())
        n = len(pts)
        for q in range(n):
            pts[q][0] = fx - 0.004 - 0.007 * (1 - q / (n - 1))   # liegt flach auf der Wand
        flur += A.chain(lambda i: BODY, pts, list(np.linspace(0.016, 0.004, n + 1)), k=0.012)
        if R() < 0.7:  # Seitenast
            q0 = pts[1 + int(R() * (n - 2))]
            d2 = A.unit(d + np.array([0, (R() - 0.5) * 1.6, (R() - 0.5) * 1.6]))
            sp, _ = walk(q0, d2, 0.12 + 0.2 * R(), step=0.05)
            for q in sp:
                q[0] = fx - 0.004
            flur += A.chain(lambda i: BODY, sp, list(np.linspace(0.008, 0.003, len(sp) + 1)), k=0.008)
    # flache Kruste an der Flurwand um die Oeffnung (Untergrund der Adern)
    # Runde 4: ein unregelmaessiger Rand am Rahmen (ueberlappend, nach aussen duenner) statt einzelner Scheiben
    for sg in (1, -1):
        z = 0.25 + 0.3 * R()
        while z < H:
            w = 0.05 + 0.06 * R() * (z / H)
            flur.append(A.ell(BODY, (fx - 0.006, sg * (HW + 0.02 + w * 0.6), z), X, (0.008, w, 0.09 + 0.06 * R()), k=0.04))
            z += 0.09 + 0.05 * R()
    for k in range(8):
        y = -0.55 + 1.1 * (k + R()) / 8
        flur.append(A.ell(BODY, (fx - 0.006, y, H + 0.04 + 0.05 * R()), X, (0.008, 0.1 + 0.05 * R(), 0.05 + 0.04 * R()), k=0.04))
    # atmender Sack im Wulst (Knochen 7)
    flur_fl.append(A.ell(SAC3, J[12], -X, (0.09, 0.15, 0.11), k=0.05))

    # ================= Laibung: Kruste, verengt die Oeffnung ===========================================================
    for side, lst in ((1, laib_l), (-1, laib_r)):
        for k in range(18):  # grosse, ueberlappende Platten, weich verschmolzen (Runde 3: keine Einzelknollen)
            x = -0.72 + 0.5 * R()                          # Runde 4: Mitte und Ausdehnung bleiben in der Laibung
            z = 0.1 + 2.0 * (k + R()) / 18
            th = 0.06 + 0.04 * R() + 0.06 * (z / H)        # oben dicker
            c = np.array([x, side * (HW - th * 0.4), z])
            lst.append(A.ell(BODY, c, Z, (0.18 + 0.08 * R(), th, 0.2 + 0.12 * R()), up=X, k=0.1))
        # Ablaeufe: Masse ist an der Laibung heruntergeflossen
        for k in range(8):
            x = -DEPTH + 0.1 + 0.8 * R()
            z0 = 1.2 + 0.9 * R()
            lst.append(A.cone(BODY, (x, side * (HW - 0.06), z0), (x + 0.02 * (R() - 0.5), side * (HW - 0.07), z0 - 0.3 - 0.5 * R()),
                              0.04, 0.015, k=0.03))
    for k in range(20):  # Sturz: dickste Kruste, haengt durch
        x = -DEPTH + 0.05 + 0.95 * R()
        y = -HW + 1.0 * R()
        sag = 0.06 * (1 - (y / HW) ** 2)
        laib_o.append(A.ell(BODY, (x, y, H - 0.05 - sag), Y, (0.14 + 0.08 * R(), 0.16 + 0.1 * R(), 0.08 + 0.05 * R()), up=Z, k=0.05))

    # ================= Schleier ueber der Oeffnung (Zimmerseite) =======================================================
    # Runde 3: keine glatte Haut und keine spitzen Fetzen (lasen sich als Leinwand und Zaehne). Eine zerrissene Lage aus
    # derselben Masse (Fasern, Harz), oben am Sturz angewachsen, mit einem grossen Riss; darunter durchhaengende Faeden
    # zwischen den Seiten, einige gerissen. Der Schleier haengt an der Angel (Knochen 2) und schwingt.
    mx = -0.16
    for k in range(10):  # flache, ueberlappende Platten; Unterkante in runden Lappen, ungleich hoch
        y = -0.42 + 0.84 * (k + R()) / 10
        zb = 1.32 + 0.3 * R() + 0.15 * (y + 0.42)       # rechts tiefer herab
        zt = H - 0.02
        haut.append(A.ell(MEMB, (mx + 0.012 * (R() - 0.5), y, (zb + zt) / 2), X, (0.011, 0.11 + 0.06 * R(), (zt - zb) / 2), up=Z, k=0.05))
    haut.append(A.ell(BODY, (mx, 0.0, H - 0.05), X, (0.022, HW - 0.03, 0.1), up=Z, k=0.05))  # am Sturz angewachsen
    haut.append(A.ell(MEMB, (mx, 0.14, 1.66), X, (0.06, 0.15, 0.2), up=A.unit(Z + 0.5 * Y), op='sub', k=0.03))  # Riss
    # Runde 4: keine kleinen Loecher (zwei nebeneinander lasen sich als Augen) - nur ein Einriss von unten
    haut.append(A.ell(MEMB, (mx, -0.22, 1.4), X, (0.06, 0.07, 0.16), up=A.unit(Z - 0.35 * Y), op='sub', k=0.02))
    for k in range(7):  # Faeden: Kettenlinien zwischen den Laibungen, einige gerissen und herabhaengend
        za = 1.0 + 0.8 * R()
        zb = za + 0.3 * (R() - 0.5)
        a = np.array([mx + 0.03 * (R() - 0.5), HW - 0.06, za])
        b = np.array([mx + 0.03 * (R() - 0.5), -HW + 0.06, zb])
        sag = 0.1 + 0.25 * R()
        pts = [A.lerp(a, b, t) - Z * sag * 4 * t * (1 - t) + X * 0.02 * math.sin(t * 6 + k) for t in np.linspace(0, 1, 11)]
        if R() < 0.4:  # gerissen: das Ende haengt vom Anker herab
            cut = 4 + int(R() * 3)
            pts = pts[:cut] + [pts[cut - 1] - Z * (0.08 + 0.12 * R()) + Y * 0.02 * (R() - 0.5)]
        n = len(pts)
        rr = [0.0045 + 0.0025 * R() for _ in range(n + 1)]
        haut += A.chain(lambda i, n=n: MEMB if 2 <= i <= n - 4 else BODY, pts, rr, k=0.004)
        for q in range(1, n - 1, 3):  # Harzperlen auf den Faeden
            if R() < 0.5:
                harz.append(A.ell(BODY, pts[q], Z, (0.009, 0.009, 0.012), k=0.003))

    # ================= Saeulen aus Resten (Zimmerseite) ================================================================
    for sg in (1, -1):
        base = np.array([1.02 + 0.08 * R(), sg * (HW + 0.32), 0.0])  # frei im Zimmer vor dem Schlund, Boden bis Decke
        top = np.array([0.98, sg * (HW + 0.26), 2.95])
        pts = [A.lerp(base, top, t) + np.array([0.05 * math.sin(t * 7 + sg), 0.04 * math.cos(t * 5), 0]) for t in np.linspace(0, 1, 10)]
        saeule += A.chain(lambda i: BODY, pts, [0.15, 0.09, 0.07, 0.1, 0.06, 0.085, 0.065, 0.09, 0.11, 0.17], k=0.05)
        for q in range(26):  # Knoten: Reste unter dem Ueberzug
            t = R()
            c = A.lerp(base, top, t) + jit(0.06) * np.array([1, 1, 0.3])
            saeule.append(A.ell(BODY, c, A.unit(jit(1)), (0.06 + 0.04 * R(), 0.05, 0.045), k=0.04))
        for q in range(7):  # herausragende Knochen und Holz
            t = 0.1 + 0.8 * R()
            c = A.lerp(base, top, t)
            d = A.unit(np.array([0.6 * (R() - 0.3), sg * (R() - 0.5), 0.8 * (R() - 0.5)]))
            a, b = c - d * 0.06, c + d * (0.2 + 0.15 * R())
            rb = 0.012 + 0.006 * R()
            (knochen if q % 3 else metall).append(A.cone(BODY, a, b, rb, rb * 0.8, k=0.0))
            if q % 3:
                side = A.perp(d)
                knochen.append(A.ell(BODY, b + side * rb * 0.6, d, (rb * 1.6, rb * 1.5, rb * 1.4), k=0.006))
                knochen.append(A.ell(BODY, b - side * rb * 0.6, d, (rb * 1.5, rb * 1.4, rb * 1.3), k=0.006))

    # ================= Puppen in der Masse (einige aufgeplatzt) =========================================================
    spots = [np.array([0.55, 0.85, 0.9]), np.array([0.6, -0.95, 1.8]), np.array([0.5, 0.75, 2.2]), np.array([0.62, -0.8, 0.6]),
             np.array([0.45, 0.2, 2.55]), np.array([0.48, -0.35, 2.6]), np.array([0.7, 0.95, 1.5]), np.array([0.66, -1.05, 1.25])]
    for i, c in enumerate(spots):
        d = A.unit(X + jit(0.5))
        puppen.append(A.ell(BODY, c, d, (0.09, 0.045, 0.045), k=0.015))
        if i % 3 == 0:  # aufgeplatzt: oben offen und hohl
            puppen.append(A.ell(BODY, c + d * 0.03, d, (0.07, 0.034, 0.034), op='sub', k=0.008))
            puppen.append(A.ell(BODY, c + d * 0.09, d, (0.05, 0.05, 0.05), op='sub', k=0.008))

    # ================= Gelege: Eier in Saeulen an der Laibung (Knochen 8 zuckt) ======================================
    # Runde 3: halb in der Kruste der Laibung (nicht frei in der Oeffnung), je Saeule zwei versetzte Reihen
    for side in (1, -1):
        for col in range(3):
            x = -0.78 + 0.24 * col + 0.05 * R()
            z = 0.5 + 0.25 * R()
            ztop = 1.5 + 0.35 * R()
            row = 0
            while z < ztop:
                r = 0.018 + 0.006 * R()
                c = np.array([x + (0.021 if row % 2 else -0.021) + 0.006 * (R() - 0.5), side * (HW - 0.115 - 0.015 * R()), z + r])
                bone = EGGS if side > 0 else BODY
                eier.append(A.ell(bone, c, A.unit(Z + jit(0.2)), (r * 1.2, r, r), k=0.004))
                z += r * 1.05
                row += 1

    # ================= Boden: Knochen vor der Tuer, halb in Harz ========================================================
    for k in range(9):
        c = np.array([-DEPTH - 0.15 - 0.55 * R(), -0.6 + 1.2 * R(), 0.02])
        d = A.unit(np.array([R() - 0.5, R() - 0.5, 0.05]))
        Lb = 0.18 + 0.2 * R()
        a, b = c - d * Lb * 0.5, c + d * Lb * 0.5
        rb = 0.012 + 0.005 * R()
        knochen.append(A.cone(BODY, a + Z * rb, b + Z * rb, rb, rb * 0.85, k=0.0))
        for e in (a, b):
            side = A.perp(d)
            knochen.append(A.ell(BODY, e + Z * rb + side * rb * 0.6, d, (rb * 1.6, rb * 1.5, rb * 1.4), k=0.006))
            knochen.append(A.ell(BODY, e + Z * rb - side * rb * 0.6, d, (rb * 1.5, rb * 1.4, rb * 1.3), k=0.006))
    for k in range(4):  # Rippen
        c = np.array([-DEPTH - 0.3 - 0.4 * R(), -0.4 + 0.8 * R(), 0.0])
        d = A.unit(np.array([R() - 0.5, R() - 0.5, 0]))
        side = A.unit(np.cross(Z, d))
        pts = A.bezier(c, c + d * 0.12 + Z * 0.06 + side * 0.04, c + d * 0.25 + Z * 0.01, 6)
        knochen += A.chain(lambda i: BODY, pts, list(np.linspace(0.009, 0.005, 7)), k=0.003)
    for k in range(5):  # Harzlachen
        harz.append(A.ell(BODY, (-DEPTH - 0.1 - 0.5 * R(), -0.5 + 1.0 * R(), 0.0), X, (0.12 + 0.1 * R(), 0.1 + 0.08 * R(), 0.012), k=0.02))
    # Harz: Tropfen vom Sturz in der Laibung und vom Wulst im Flur
    for k in range(14):
        x = -DEPTH + 0.04 + 0.9 * R() if k < 9 else fx - 0.04
        top = np.array([x, -0.42 + 0.84 * R(), H - 0.12 - 0.05 * R()])
        ln = 0.1 + 0.35 * R()
        bot = top + np.array([0.0, 0.02 * (R() - 0.5), -ln])
        mid = A.lerp(top, bot, 0.5)
        harz += [A.cone(BODY, top, mid, 0.012, 0.006, k=0.006), A.cone(BODY, mid, bot, 0.006, 0.003, k=0.004),
                 A.ell(BODY, bot - Z * 0.004, Z, (0.0065, 0.0065, 0.0085), k=0.003)]
    # Pflanzenfasern, die aus der Masse ragen (Kragen und Laibung)
    for k in range(40):
        if k < 24:
            p, nrm = ring_point(R() * 0.75)
            c = p + nrm * (0.08 + 0.15 * R()) + X * (0.15 + 0.4 * R())
            d = A.unit(X * 0.6 + nrm + jit(0.5))
        else:
            side = 1 if k % 2 else -1
            c = np.array([-DEPTH + 0.1 + 0.8 * R(), side * (HW - 0.12), 0.3 + 1.7 * R()])
            d = A.unit(-side * Y + jit(0.6))
        pts = A.bezier(c - d * 0.02, c + d * 0.025 + Z * 0.006, c + d * (0.045 + 0.04 * R()) - Z * 0.012, 4)
        fasern += A.chain(lambda i: BODY, pts, [0.0024, 0.002, 0.0015, 0.0011, 0.0006], k=0.001)

    # ================= Verschiebungen =====================================================================================
    def mass_disp(Q):
        # gegossene Lagen (Lippen an der Unterkante) - schraeg zur Wand, damit sie nicht wie Teller wirken
        ph = (Q[:, 2] + 0.35 * Q[:, 0]) / 0.06 + 3.2 * sdf.fbm(Q * np.array([1.0, 1.0, 0.4]), 3, 3, seed=91)
        s = ph - np.floor(ph)
        d = -0.0028 * (1 - s) ** 2 * (0.4 + 0.6 * np.clip(sdf.fbm(Q, 4, 2, seed=99) + 0.5, 0, 1))  # Lagen nur stellenweise
        d -= 0.014 * sdf.fbm(Q, 5, 3, seed=92)                     # grosse Beulen
        d -= 0.0012 * (sdf.ridged(Q, 55, 2, seed=93) - 0.5)        # Fasern und Risse
        return d

    def flesh_disp(Q):  # nasse, runzlige Wucherung
        return -0.006 * sdf.fbm(Q, 10, 3, seed=94) - 0.0015 * (sdf.ridged(Q, 45, 2, seed=95) - 0.5) - 0.002 * sdf.fbm(Q, 35, 2, seed=97)

    def thin_disp(Q):
        return -0.0012 * sdf.fbm(Q, 30, 2, seed=98)

    def veil_disp(Q):  # Fasern und Harz in der Lage
        return -0.0016 * sdf.fbm(Q, 22, 2, seed=88) - 0.0008 * (sdf.ridged(Q * np.array([1.0, 1.0, 0.3]), 70, 2, seed=89) - 0.5)

    def bone_disp(Q):
        return -0.0006 * sdf.fbm(Q, 120, 2, seed=96)

    allmass = [p for p in kragen + laib_l + laib_r + laib_o + saeule if p.op == 'add']
    groups = [
        {'name': 'kragen', 'fm': 'ent_nest', 'parts': kragen, 'disp': mass_disp, 'h': 0.008, 'cav_scale': 0.03},
        {'name': 'kragen_fleisch', 'fm': 'ent_nestflesh', 'parts': kragen_fl, 'disp': flesh_disp, 'h': 0.006, 'cav_scale': 0.02,
         'cav_extra': allmass},
        {'name': 'flur_kruste', 'fm': 'ent_nest', 'parts': flur, 'disp': thin_disp, 'h': 0.004, 'cav_scale': 0.01},
        {'name': 'flur_wulst', 'fm': 'ent_nestflesh', 'parts': flur_fl, 'disp': flesh_disp, 'h': 0.005, 'cav_scale': 0.02, 'cav_extra': flur},
        {'name': 'laibung_l', 'fm': 'ent_nest', 'parts': laib_l, 'disp': mass_disp, 'h': 0.006, 'cav_scale': 0.02},
        {'name': 'laibung_r', 'fm': 'ent_nest', 'parts': laib_r, 'disp': mass_disp, 'h': 0.006, 'cav_scale': 0.02},
        {'name': 'laibung_o', 'fm': 'ent_nest', 'parts': laib_o, 'disp': mass_disp, 'h': 0.006, 'cav_scale': 0.02},
        {'name': 'schleier', 'fm': 'ent_nest', 'parts': haut, 'disp': veil_disp, 'h': 0.0035, 'cav_scale': 0.006},
        {'name': 'saeulen', 'fm': 'ent_nest', 'parts': saeule, 'disp': mass_disp, 'h': 0.006, 'cav_scale': 0.02},
        {'name': 'puppen', 'fm': 'ent_nest', 'parts': puppen, 'disp': thin_disp, 'h': 0.003, 'cav_scale': 0.01, 'cav_extra': allmass},
    ]
    for name, fm, parts, h, disp in (('eier', 'ent_nestegg', eier, 0.0018, None), ('knochen', 'ent_horn', knochen, 0.0022, bone_disp),
                                     ('metall', 'ent_chitin', metall, 0.0022, None), ('harz', 'ent_resin', harz, 0.0018, None),
                                     ('fasern', 'ent_fur', fasern, 0.0009, None)):
        for i, cl in enumerate(cluster(parts, 0.35)):
            groups.append({'name': f'{name}{i}', 'fm': fm, 'parts': cl, 'disp': disp, 'h': h, 'cav_scale': 0.004,
                           'cav_extra': allmass, 'merge': fm})
    return groups


def cluster(parts, size):
    """Teile nach Lage in Haufen mit hoechstens size Kantenlaenge (gierig, in Reihenfolge der Mitte)."""
    out = []
    for p in sorted(parts, key=lambda q: tuple(np.round((q.lo + q.hi) / 2 / size).tolist())):
        for cl in out:
            lo = np.minimum(cl['lo'], p.lo)
            hi = np.maximum(cl['hi'], p.hi)
            if np.all(hi - lo <= size):
                cl['lo'], cl['hi'] = lo, hi
                cl['parts'].append(p)
                break
        else:
            out.append({'lo': p.lo.copy(), 'hi': p.hi.copy(), 'parts': [p]})
    return [c['parts'] for c in out]
