"""V13.3: Nguithr'xurh (Entity 16, Wikidot entity-16, Rev. 39) - KREATUR_Nguithrxurh.md. Die Spinne, Neubau (Runde 6).

Eine Walzenspinne (Solifugae): acht Laufbeine (das vordere Paar duenn, tastet wie Fuehler, ohne Krallen; das hinterste
mit den faecherfoermigen Malleoli unter der Huefte), zwei lange, dicke, beinartige Pedipalpen mit blassem Haftende, zwei
maechtige zweigliedrige Cheliceren (oben ein fester, unten ein beweglicher Finger, beide mit Zaehnen, dunkel). Gewoelbter
Kopfschild mit kleinem Augenhuegel (zwei kleine dunkle Augen), weicher Hinterleib mit Querbaendern, zur Spitze schmaler.
Matt, sandig-staubig, hellbraun bis ocker, dunklere Gelenke; dicht behaart (echte duenne Haare). Mittlere Groesse (die
Seite: etwa 10 x 18 cm); im Spiel je Ebene skaliert. Dazu der Faden aus der Hinterleibsspitze (3 m, nur beim Abseilen).

Aufbau: Koerper, Cheliceren und Augen als Distanzfeld (werden je Stufe ausgeduennt). Beine, Taster, Haare und der Faden
als fertige Netze je Stufe ('lod_mesh', build.py) - duenne Glieder und Haare zerfielen beim Ausduennen zu flachen
Dreiecken (Runde 5 des Koordinators: "Wuerstchen", Dreiecksblaetter). Glieder: durchgehende Roehren, an jedem Gelenk
verbunden (Gelenkring mit halbem Gewicht auf beiden Knochen), leichte Verdickung am Gliedende, schmale Gelenkhaut.

Knochen (ent_nguithrxurh, ng in src/anim/creature.hpp): 1 Kopfschild, 2/3 Hinterleib, 4/5 Cheliceren (fester Finger),
6/7 untere Finger, 8..10 und 11..13 Pedipalpen, 14 + 3 * Bein + i Beine (Bein = Paar * 2 + Seite), 38 Faden.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (14000, 6000, 2600)  # nur der Teil aus dem Distanzfeld; Beine, Haare, Faden kommen fertig dazu
PREVIEW_HIDE = ('faden',)
try:  # Vorschaufarben (build.py laeuft als __main__)
    import __main__
    __main__.PREVIEW_RGB.update({'ent_solifuge': (0.62, 0.47, 0.30), 'ent_solifuge_dark': (0.20, 0.12, 0.08),
                                 'ent_solifuge_hair': (0.80, 0.70, 0.54), 'ent_solifuge_joint': (0.36, 0.24, 0.15), 'ent_silk': (0.85, 0.84, 0.8),
                                 'ent_solifuge_head': (0.50, 0.30, 0.17),
                                 'ent_sedative': (0.85, 0.82, 0.62)})
except Exception:
    pass

ROOT, PRO, EYES, OPI0, OPI1, OPI2 = 0, 1, 2, 3, 4, 5
CHJ = (6, 10)   # + 0 Ansatz, 1 Gelenk unterer Finger, 2 Spitze oben, 3 Spitze unten
PPJ = (14, 18)  # + 0 Ansatz, 1 Knie, 2 Handgelenk, 3 Haftende
LEG0 = 22
DRAG_A = 54
B_PRO, B_OPI0, B_OPI1 = 1, 2, 3
B_CH = (4, 5)
B_CHM = (6, 7)
B_PP = (8, 11)
B_LEG0, B_DRAG = 14, 38
Z = np.array([0.0, 0.0, 1.0])
X = np.array([1.0, 0.0, 0.0])
SIDES = (10, 7, 5)  # Roehren je Stufe
RINGS = (1.0, 0.5, 0.28)  # Ringdichte je Stufe


def legJ(l, i):
    return LEG0 + l * 4 + i


def legB(l, i):
    return B_LEG0 + l * 3 + i


def unit(v):
    v = np.asarray(v, dtype=float)
    return v / max(np.linalg.norm(v), 1e-12)


def smooth(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


# --- Roehren: durchgehende, gegliederte Glieder ----------------------------------------------------------------
def limb(pts, bones, radii, nb, lod, root_bone, tip='point', mids=(), root_r=1.0, bows=(0.035, 0.015, -0.02), cav_seg=(0.0, 0.0, 0.0)):
    """Ein Glied durch die Gelenke pts (Abschnitt i auf bones[i]); radii[i] = (r am Anfang, r am Ende).
    Je Abschnitt: schmale Gelenkhaut am Anfang, leichte Verdickung am Ende; am Gelenk ein Ring auf der Winkelhalbierenden
    mit halbem Gewicht auf beiden Knochen (so reisst das Glied beim Beugen nicht auf). mids: (Abschnitt, t) - zusaetzliche
    Gliederung ohne eigenen Knochen (Patella/Tibia, Metatarsus/Tarsus). Rueckgabe V, T, W, Hoehlung, Kante und die
    Ringe (fuer die Haare)."""
    pts = [np.asarray(p, dtype=float) for p in pts]
    n = len(pts) - 1
    sides = SIDES[lod]
    dirs = [unit(pts[i + 1] - pts[i]) for i in range(n)]
    lens = [float(np.linalg.norm(pts[i + 1] - pts[i])) for i in range(n)]
    rings = []  # (Mitte, Achse, Radius, {Knochen: Gewicht}, Hoehlung, Abschnitt, t)
    joints = []  # Index der Gelenkringe (dunkle Manschetten)
    for i in range(n):
        r0, r1 = radii[i]
        if i == 0:  # Huefte im Koerper: Wurzelring (halb am Elternknochen)
            rings.append((pts[0], dirs[0], r0 * 1.25 * root_r, {root_bone: 0.5, bones[0]: 0.5}, 0.5, 0, 0.0))
        steps = max(3, int(round(lens[i] / 0.004 * RINGS[lod])) + 2)
        ts = list(np.linspace(0.05, 0.95, steps))
        for (si, tm) in mids:  # Gliederung ohne Knochen: Einschnuerung
            if si == i and lod < 2:
                ts += [tm - 0.025, tm, tm + 0.025]
        ts = sorted(set(round(t, 4) for t in ts))
        for t in ts:
            r = r0 + (r1 - r0) * t
            r *= 1.0 + 0.16 * smooth((t - 0.72) / 0.2) * (1.0 - smooth((t - 0.93) / 0.05))  # Gelenkkopf
            r *= 1.0 - 0.14 * (1.0 - smooth(t / 0.1))  # Gelenkhaut am Anfang
            cav = 0.08 + cav_seg[i] * smooth((t - 0.1) / 0.5) + 0.45 * (1.0 - smooth(t / 0.1)) + 0.25 * smooth((t - 0.9) / 0.06)
            for (si, tm) in mids:
                if si == i and lod < 2 and abs(t - tm) < 0.03:
                    r *= 0.9 if abs(t - tm) < 1e-3 else 0.97
                    cav = max(cav, 0.55 if abs(t - tm) < 1e-3 else 0.3)
            if i == 0 and t < 0.25:
                r *= 1.0 + 0.25 * root_r * (1.0 - t / 0.25)  # Huefte/Trochanter dicker
            w = {bones[i]: 1.0}
            if t > 0.9 and i + 1 < n:
                w = {bones[i]: 0.82, bones[i + 1]: 0.18}
            if t < 0.1 and i > 0:
                w = {bones[i]: 0.82, bones[i - 1]: 0.18}
            up = A.perp(dirs[i], Z)  # Runde 6e: leicht gebogen (vorher gerade Stecken)
            bw = bows[i] * lens[i]
            rings.append((pts[i] + (pts[i + 1] - pts[i]) * t + up * (bw * math.sin(math.pi * t)),
                          unit(dirs[i] + up * (bows[i] * math.pi * math.cos(math.pi * t))), r, w, cav, i, t))
        if i + 1 < n:  # Gelenkring: schmal, dunkel, halb und halb
            rj = 0.86 * min(r1, radii[i + 1][0])
            joints.append(len(rings))
            rings.append((pts[i + 1], unit(dirs[i] + dirs[i + 1]), rj, {bones[i]: 0.5, bones[i + 1]: 0.5}, 0.8, i, 1.0))
    # Spitze: Punkt (Krallenglied) oder rund (Haftende)
    e = pts[-1]
    if tip == 'round':
        rr = radii[-1][1]
        for f, cz in ((0.75, 0.25), (0.35, 0.55)):
            rings.append((e + dirs[-1] * rr * cz, dirs[-1], rr * f, {bones[-1]: 1.0}, 0.1, n - 1, 1.0))
        pole = e + dirs[-1] * rr * 0.75
    else:
        pole = e + dirs[-1] * 0.0005
    # Ringe mit Paralleltransport
    V, W, C, E = [], [], [], []
    e1 = A.perp(rings[0][1], Z)
    for (c, ax, r, w, cav, si, t) in rings:
        e1 = unit(e1 - ax * float(e1 @ ax))
        e2 = np.cross(ax, e1)
        for j in range(sides):
            th = 2 * math.pi * j / sides
            V.append(c + (e1 * math.cos(th) + e2 * math.sin(th)) * r)
            wv = np.zeros(nb)
            for b, x in w.items():
                wv[b] += x
            W.append(wv)
            C.append(cav)
            E.append(0.12)
    nr = len(rings)
    T = []
    for k in range(nr - 1):
        a0, b0 = k * sides, (k + 1) * sides
        for j in range(sides):
            j1 = (j + 1) % sides
            T += [(a0 + j, a0 + j1, b0 + j1), (a0 + j, b0 + j1, b0 + j)]
    # Kappen: Wurzel (im Koerper) und Spitze
    root = len(V)
    V.append(rings[0][0] - rings[0][1] * rings[0][2] * 0.6)
    W.append(W[0].copy())
    C.append(0.6)
    E.append(0.0)
    for j in range(sides):
        T.append((root, (j + 1) % sides, j))
    tipi = len(V)
    V.append(pole)
    wv = np.zeros(nb)
    wv[bones[-1]] = 1.0
    W.append(wv)
    C.append(0.2 if tip == 'round' else 0.5)
    E.append(0.2)
    last = (nr - 1) * sides
    for j in range(sides):
        T.append((last + j, last + (j + 1) % sides, tipi))
    # Runde 6c: dunkle Gelenke - eine kurze Manschette ueber jedem Gelenk (eigenes, dunkles Material)
    V = np.array(V)
    W = np.array(W)
    sV, sT, sW = [], [], []
    for jk in joints:
        base = len(sV)
        for k in (jk - 1, jk, jk + 1):
            c = rings[k][0]
            for j in range(sides):
                sV.append(c + (V[k * sides + j] - c) * 1.07)
                sW.append(W[k * sides + j])
        for a in range(2):
            a0, b0 = base + a * sides, base + (a + 1) * sides
            for j in range(sides):
                j1 = (j + 1) % sides
                sT += [(a0 + j, a0 + j1, b0 + j1), (a0 + j, b0 + j1, b0 + j)]
    sleeve = (np.array(sV), np.array(sT, dtype=np.int64), np.array(sW), np.full(len(sV), 0.35), np.full(len(sV), 0.1))
    return V, np.array(T, dtype=np.int64), W, np.array(C), np.array(E), rings, sleeve


def surface_on_rings(rings, rng, count, skip_joint=True):
    """Zufaellige Punkte auf einer Roehre: Wurzel, Normale, Laengsrichtung, Gewichte, Radius."""
    segs = [(k, rings[k], rings[k + 1]) for k in range(len(rings) - 1)]
    lens = np.array([np.linalg.norm(b[0] - a[0]) for _, a, b in segs])
    lens = np.where([skip_joint and (a[4] > 0.6 or b[4] > 0.6) for _, a, b in segs], lens * 0.15, lens)
    p = lens / lens.sum()
    out = []
    for k in rng.choice(len(segs), size=count, p=p):
        _, a, b = segs[k]
        u = rng.random()
        c = a[0] + (b[0] - a[0]) * u
        ax = unit(b[0] - a[0]) if np.linalg.norm(b[0] - a[0]) > 1e-6 else a[1]
        r = a[2] + (b[2] - a[2]) * u
        e1 = A.perp(ax, Z)
        e2 = np.cross(ax, e1)
        th = rng.uniform(0, 2 * math.pi)
        nrm = e1 * math.cos(th) + e2 * math.sin(th)
        w = a[3] if u < 0.5 else b[3]
        out.append((c + nrm * r * 0.85, nrm, ax, w, r))
    return out


# --- Haare: echte duenne Straehnen (drei Seiten, spitz) ---------------------------------------------------------
class Hair:
    def __init__(self, nb):
        self.nb = nb
        self.items = []  # (Wurzel, Richtung, Laenge, Radius, Kruemmung, Gewichte, Behalten)

    def add(self, root, d, length, rad, bend, w, keep):
        self.items.append((np.asarray(root, float), unit(d), float(length), float(rad), np.asarray(bend, float), w, float(keep)))

    def mesh(self, lod, rng_seed=7):
        V, T, W, C, E = [], [], [], [], []
        rng = np.random.default_rng(rng_seed)
        for (p0, d, L, r, bend, w, keep) in self.items:
            if lod == 2 or (lod == 1 and keep > 0.12):
                continue
            wv = np.zeros(self.nb)
            for b, x in w.items():
                wv[b] += x
            e1 = A.perp(d, rng.normal(0, 1, 3))
            e2 = np.cross(d, e1)
            if lod == 0 and L >= 0.0035:  # lange: zwei Abschnitte, gebogen
                p1 = p0 + d * L * 0.5
                d2 = unit(d + bend)
                tip = p1 + d2 * L * 0.5
                rs = ((p0, r), (p1, r * 0.55))
            else:  # kurze (und Stufe 1): eine Spitze; auf Stufe 1 an der Wurzel etwas dicker
                tip = p0 + unit(d + bend * 0.5) * L
                rs = ((p0, r * (1.35 if lod else 1.0)),)
            base = len(V)
            for (c, rr) in rs:
                for j in range(3):
                    th = 2 * math.pi * j / 3
                    V.append(c + (e1 * math.cos(th) + e2 * math.sin(th)) * rr)
                    W.append(wv)
                    C.append(0.3 if c is p0 else 0.12)
                    E.append(0.0)
            ti = len(V)
            V.append(tip)
            W.append(wv)
            C.append(0.0)
            E.append(0.25)
            for k in range(len(rs) - 1):
                a0, b0 = base + 3 * k, base + 3 * (k + 1)
                for j in range(3):
                    j1 = (j + 1) % 3
                    T += [(a0 + j, a0 + j1, b0 + j1), (a0 + j, b0 + j1, b0 + j)]
            last = base + 3 * (len(rs) - 1)
            for j in range(3):
                T.append((last + j, last + (j + 1) % 3, ti))
        if not V:
            return np.zeros((0, 3)), np.zeros((0, 3), dtype=np.int64), np.zeros((0, self.nb)), np.zeros(0), np.zeros(0)
        return np.array(V), np.array(T, dtype=np.int64), np.array(W), np.array(C), np.array(E)


def project(parts, P, iters=10):
    """Punkte auf die Oberflaeche des Distanzfelds ziehen (Newton); Rueckgabe Punkte, Normalen, Abstand."""
    e = 2e-5
    for _ in range(iters):
        d = sdf.sample(parts, P)
        g = np.stack([(sdf.sample(parts, P + np.array([e, 0, 0])) - d) / e, (sdf.sample(parts, P + np.array([0, e, 0])) - d) / e,
                      (sdf.sample(parts, P + np.array([0, 0, e])) - d) / e], axis=1)
        gl = np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9)
        P = P - (d[:, None]) * g / gl ** 2
    d = sdf.sample(parts, P)
    g = np.stack([(sdf.sample(parts, P + np.array([e, 0, 0])) - d) / e, (sdf.sample(parts, P + np.array([0, e, 0])) - d) / e,
                  (sdf.sample(parts, P + np.array([0, 0, e])) - d) / e], axis=1)
    return P, g / np.maximum(np.linalg.norm(g, axis=1, keepdims=True), 1e-9), d


def build(J, bones):
    rng = np.random.default_rng(16)
    J = np.asarray(J, dtype=np.float64)
    nb = len(bones)
    groups = []
    hair = Hair(nb)
    # --- Kopfschild (Propeltidium): breit, stark gewoelbt, vorn gerade; Augenhuegel vorn in der Mitte ----------------
    pro = [A.ell(B_PRO, (0.030, 0, 0.033), X, (0.019, 0.0175, 0.0125), k=0.0),
           A.ell(B_PRO, (0.041, 0, 0.034), X, (0.010, 0.019, 0.011), k=0.006),
           A.ell(B_PRO, (0.024, 0, 0.0235), X, (0.024, 0.014, 0.007), k=0.006),  # Brust/Hueften unten
           A.ell(B_PRO, (0.008, 0, 0.031), X, (0.006, 0.0125, 0.0095), k=0.003)]  # Meso-/Metapeltidium
    for l in range(8):  # Hueftsockel
        c = J[legJ(l, 0)]
        d = unit(J[legJ(l, 1)] - c) * np.array([1, 1, 0.3])
        pro.append(A.ell(B_PRO, c + d * 0.003, d, (0.0055, 0.0048, 0.0042), k=0.003))
    for si in range(2):
        c = J[PPJ[si]]
        pro.append(A.ell(B_PRO, c + unit(J[PPJ[si] + 1] - c) * 0.003, J[PPJ[si] + 1] - c, (0.006, 0.0055, 0.005), k=0.003))
    # --- Hinterleib: weich, zehn Ringe (Rillen aus der Verschiebung), zur Spitze schmaler ----------------------------
    xs = [0.002, -0.008, -0.020, -0.033, -0.046, -0.058, -0.068, -0.076, -0.082]  # Runde 6b: kuerzer, breiter (war eine Made)
    zs = [0.0300, 0.0305, 0.0312, 0.0315, 0.0308, 0.0296, 0.0282, 0.0270, 0.0262]
    rs = [(0.0120, 0.0090), (0.0190, 0.0135), (0.0245, 0.0165), (0.0265, 0.0176), (0.0250, 0.0170), (0.0210, 0.0150),
          (0.0160, 0.0120), (0.0100, 0.0080), (0.0045, 0.0040)]
    opi = []
    for i in range(len(xs) - 1):
        b = B_OPI0 if xs[i] > -0.040 else B_OPI1
        opi.append(A.tube(b, (xs[i], 0, zs[i]), (xs[i + 1], 0, zs[i + 1]), rs[i], rs[i + 1], k=0.002 if i else 0.0))
    body_parts = pro + opi

    def disp(P):  # Querrillen zwischen den Hinterleibsringen, Seitenfalte, feiner Flaum
        x = P[:, 0]
        g = np.zeros(len(P))
        for k in range(1, 10):
            xk = -0.002 - k * 0.0084
            g += np.exp(-((x - xk) / 0.0012) ** 2)
        back = (x < -0.002).astype(float)
        side = np.exp(-((P[:, 2] - 0.029) / 0.0018) ** 2) * back * (np.abs(P[:, 1]) > 0.008)
        fuzz = sdf.fbm(P * np.array([1.0, 2.0, 2.0]), 700, 2, seed=1602) - 0.5
        return 0.0011 * g * back + 0.0004 * side - 0.00025 * fuzz
    groups.append({'name': 'kopf', 'fm': 'ent_solifuge_head', 'parts': pro, 'disp': disp, 'h': 0.00034, 'cav_scale': 0.0035,
                   'merge': 'kopf', 'cav_extra': opi})  # Runde 6f: zweifarbig (vorher einheitlich wie Teig)
    groups.append({'name': 'koerper', 'fm': 'ent_solifuge', 'parts': opi, 'disp': disp, 'h': 0.00034, 'cav_scale': 0.0035,
                   'merge': 'koerper', 'cav_extra': pro})
    # --- Cheliceren: maechtige Grundglieder (behaart, Koerperfarbe), dunkle Finger mit Zaehnen -----------------------
    dark = []
    teeth = []  # Runde 7b: Zaehne und Hakenspitzen fast schwarz
    for si, s in enumerate((1, -1)):
        b, bm = B_CH[si], B_CHM[si]
        cj = CHJ[si]
        base = [A.ell(b, (0.060, s * 0.0098, 0.0348), (1, 0, -0.15), (0.0190, 0.0100, 0.0138), k=0.0),  # Runde 6c: kein Wuerstchen -
                A.ell(b, (0.049, s * 0.0095, 0.0375), X, (0.0100, 0.0094, 0.0118), k=0.004),            # kuerzer, hoch, Buckel hinten,
                A.ell(b, (0.074, s * 0.0092, 0.0318), (1, 0, -0.3), (0.0100, 0.0080, 0.0098), k=0.004)]  # vorn zum Finger schmaler
        # Runde 6e: Finger kurz, kraeftig, stark gekruemmt (wie eine Krebsschere), seitlich flach; der Ansatz in Koerperfarbe,
        # die Spitzen dunkel; grosse Zaehne an den Innenkanten (vorher lange glatte "Bananen")
        fx = [(0.075, 0.0092, 0.0348), (0.089, 0.0087, 0.0350), (0.100, 0.0080, 0.0322), (0.1065, 0.0074, 0.0276), (0.1072, 0.0072, 0.0230)]
        fr = [(0.0046, 0.0082), (0.0035, 0.0062), (0.0025, 0.0043), (0.0014, 0.0025), (0.0004, 0.0006)]
        Fp = [np.array((q[0], s * q[1], q[2])) for q in fx]
        for i in range(4):  # Runde 6g: ganz dunkel (der helle Ansatz sah von vorn aus wie Haut)
            (teeth if i == 3 else dark).append(A.tube(b, Fp[i], Fp[i + 1], fr[i], fr[i + 1], k=0.001 if i else 0.0))
        # Runde 7b: Kiel oben auf dem festen Finger (von vorn kein glatter Stosszahn mehr), aussen eine Reihe kleiner Hoecker
        for i in range(2):
            dark.append(A.tube(b, Fp[i] + Z * (fr[i][1] * 0.78), Fp[i + 1] + Z * (fr[i + 1][1] * 0.78), (0.0011, 0.0016), (0.0008, 0.0012), k=0.0008))

        def at(pts, rads, x):  # Mitte und Hoehe der Kette an der Stelle x (ersten drei Abschnitte)
            for i in range(3):
                if pts[i][0] <= x <= pts[i + 1][0]:
                    f = (x - pts[i][0]) / (pts[i + 1][0] - pts[i][0])
                    return pts[i] + (pts[i + 1] - pts[i]) * f, rads[i][1] + (rads[i + 1][1] - rads[i][1]) * f
            return pts[3], rads[3][1]
        for x, L in ((0.0825, 0.0034), (0.0875, 0.0052), (0.0925, 0.0042), (0.0975, 0.0055), (0.1015, 0.0032)):  # Zaehne oben, nach unten
            c, rh = at(Fp, fr, x)
            q0 = c - Z * (rh * 0.65)
            teeth.append(A.cone(b, q0, q0 - Z * L + X * 0.0008, 0.0015, 0.0002))
        for x in np.linspace(0.080, 0.099, 6):  # kleine Hoecker aussen unten
            c, rh = at(Fp, fr, x)
            q0 = c - Z * (rh * 0.45) + np.array([0, s, 0]) * 0.0022
            teeth.append(A.cone(b, q0, q0 - Z * 0.0016 + np.array([0, s, 0]) * 0.0008, 0.0007, 0.0001))
        hj = J[cj + 1]
        Mp = [hj, np.array((0.090, s * 0.0084, 0.0238)), np.array((0.0995, s * 0.0078, 0.0248)), np.array((0.1045, s * 0.0070, 0.0284))]
        mr = [(0.0038, 0.0056), (0.0029, 0.0043), (0.0020, 0.0030), (0.0006, 0.0008)]
        mbase = [A.ell(bm, Mp[0], Mp[1] - Mp[0], (0.0036, 0.0034, 0.0046), k=0.0)]  # Gelenkkopf in Koerperfarbe
        for i in range(3):
            (teeth if i == 2 else dark).append(A.tube(bm, Mp[i], Mp[i + 1], mr[i], mr[i + 1], k=0.001 if i else 0.0))
        for x, L in ((0.0855, 0.0036), (0.0905, 0.0047), (0.0955, 0.0039), (0.100, 0.0026)):  # Zaehne unten, nach oben
            c, rh = at(Mp + [Mp[-1]], mr + [mr[-1]], x)
            q0 = c + Z * (rh * 0.65)
            teeth.append(A.cone(bm, q0, q0 + Z * L + X * 0.0006, 0.0014, 0.0002))
        groups.append({'name': f'chel{si}', 'fm': 'ent_solifuge_head', 'parts': base, 'disp': lambda P: -0.00025 * (sdf.fbm(P, 700, 2, seed=1603) - 0.5),
                       'h': 0.00028, 'cav_scale': 0.003, 'merge': 'kopf', 'cav_extra': pro})
        groups.append({'name': f'cheln{si}', 'fm': 'ent_solifuge_head', 'parts': mbase, 'disp': None, 'h': 0.00022, 'cav_scale': 0.002, 'merge': 'kopf'})
    dark.append(A.ell(B_PRO, (0.046, 0, 0.0443), X, (0.0042, 0.0050, 0.0036), k=0.0))  # Augenhuegel dunkel (Runde 6: Comic-Gesicht)
    # Querbaender: dunkle Rueckenplatten auf den weichen Hinterleibsringen
    for k in range(9):
        xc = -0.002 - (k + 0.5) * 0.0084
        i = max(0, min(len(xs) - 2, int(np.searchsorted(-np.array(xs), -xc)) - 1))
        f = (xc - xs[i]) / (xs[i + 1] - xs[i])
        ry = rs[i][0] + (rs[i + 1][0] - rs[i][0]) * f
        rz = rs[i][1] + (rs[i + 1][1] - rs[i][1]) * f
        zc = zs[i] + (zs[i + 1] - zs[i]) * f
        dark.append(A.ell(B_OPI0 if xc > -0.040 else B_OPI1, (xc, 0, zc + rz - 0.0005), X, (0.0034, ry * 0.6, 0.0011), k=0.0))
    groups.append({'name': 'zaehne', 'fm': 'ent_darkskin', 'parts': teeth, 'disp': None, 'h': 0.00016, 'cav_scale': 0.001, 'merge': 'zaehne',
                   'lod_weight': 1.6})
    groups.append({'name': 'scheren', 'fm': 'ent_solifuge_dark', 'parts': dark, 'disp': None, 'h': 0.0002, 'cav_scale': 0.0015,
                   'merge': 'dunkel', 'lod_weight': 1.6})
    # --- Augen: klein, dunkel, auf dem Huegel ----------------------------------------------------------------------
    eyes = [A.ell(B_PRO, (0.0484, s * 0.0024, 0.0458), unit((1, s * 0.6, 0.5)), (0.00105, 0.00105, 0.00105), k=0.0) for s in (1, -1)]
    groups.append({'name': 'augen', 'fm': 'ent_motheye', 'parts': eyes, 'disp': None, 'h': 0.00012, 'cav_scale': 0.0006, 'lod_weight': 2.0})
    # --- Haftenden der Taster (blass) und Malleoli unter den Hinterbeinen --------------------------------------------
    pale = []
    for si in range(2):
        t, w = J[PPJ[si] + 3], J[PPJ[si] + 2]
        d = unit(t - w)
        pale.append(A.ell(B_PP[si] + 2, t + d * 0.0002, d, (0.0029, 0.0035, 0.0031), k=0.0))  # Runde 6c/6f: kleiner (Wattestaebchen)
    for l in (6, 7):
        a, k2 = J[legJ(l, 0)], J[legJ(l, 1)]
        d = unit(k2 - a)
        for m in range(5):
            c = a + d * (0.004 + 0.0032 * m) - Z * 0.0034 + A.perp(d, Z) * 0.0
            pale.append(A.cone(legB(l, 0), a + d * (0.004 + 0.0032 * m) - Z * 0.001, c, 0.0004, 0.0004))
            pale.append(A.ell(legB(l, 0), c - Z * 0.0005, d, (0.0011, 0.0019, 0.0004), up=Z, k=0.0))
    groups.append({'name': 'blass', 'fm': 'ent_solifuge', 'parts': pale, 'disp': None, 'h': 0.00016, 'cav_scale': 0.001, 'merge': 'blass'})

    # --- Beine und Taster: fertige Roehren je Stufe ----------------------------------------------------------------
    # Radien je Abschnitt (Mittelgroesse): Tastbein duenn, Hinterbein am dicksten, Taster dick wie ein Bein
    LR = {0: [(0.0038, 0.0031), (0.0029, 0.0022), (0.0020, 0.0009)],  # Runde 6b/6d: kraeftiger (waren Stecken)
          1: [(0.0047, 0.0040), (0.0037, 0.0029), (0.0025, 0.0010)],
          2: [(0.0050, 0.0042), (0.0039, 0.0030), (0.0026, 0.0010)],
          3: [(0.0057, 0.0049), (0.0045, 0.0035), (0.0030, 0.0011)]}
    PR = [(0.0056, 0.0051), (0.0051, 0.0048), (0.0047, 0.0042)]
    limbs = []  # (pts, bones, radii, root, tip, mids, Haare)
    for l in range(8):
        p = l // 2
        limbs.append(([J[legJ(l, i)] for i in range(4)], [legB(l, i) for i in range(3)], LR[p], B_PRO, 'point',
                      ((1, 0.28), (2, 0.5), (2, 0.75)), (480, 480, 540, 670)[p], p))
    for si in range(2):
        limbs.append(([J[PPJ[si] + i] for i in range(4)], [B_PP[si] + i for i in range(3)], PR, B_PRO, 'round',
                      ((1, 0.3), (2, 0.45)), 900, 9))
    lod_meshes = [[], [], []]
    palp_meshes = [[], [], []]
    sleeves = [[], [], []]
    claws = [[], [], []]
    for (pts, bs, radii, root, tip, mids, nh, kind) in limbs:
        for lod in range(3):
            V, T, W, C, E, rings, slv = limb(pts, bs, radii, nb, lod, root, tip, mids, root_r=1.15 if kind == 9 else 1.0,
                                             cav_seg=(0.0, 0.12, 0.4) if kind == 9 else (0.0, 0.0, 0.0))
            (palp_meshes if kind == 9 else lod_meshes)[lod].append((V, T, W, C, E))
            sleeves[lod].append(slv)
            if lod == 0:
                ring0 = rings
        # Krallen an den Laufbeinen (zwei kleine, dunkle)
        if kind in (1, 2, 3):
            e = np.asarray(pts[-1], float)
            d = unit(pts[-1] - pts[-2])
            side = A.perp(d, Z)
            for sgn in (1, -1):
                q = e + side * sgn * 0.0004 + unit(d - Z * 0.9 + side * sgn * 0.5) * 0.0024
                for lod in range(2):
                    claws[lod].append((e - d * 0.0004, q, bs[-1]))
        # Haare auf dem Glied: kurze anliegende und lange abstehende Borsten (Taster am dichtesten)
        for (root_p, nrm, ax, w, r) in surface_on_rings(ring0, rng, nh):
            longh = rng.random() < (0.45 if kind == 9 else 0.25)
            if longh:
                L = rng.uniform(0.005, 0.012 if kind == 9 else 0.009)
                a = rng.uniform(0.7, 1.25)
            else:
                L = rng.uniform(0.0016, 0.0042)
                a = rng.uniform(0.35, 0.75)
            d = ax * math.cos(a) + nrm * math.sin(a)
            hair.add(root_p, d, L, 0.00026 if longh else 0.00017, ax * 0.45 - Z * 0.12 + rng.normal(0, 0.15, 3), w, rng.random() * (0.6 if longh else 1.4))
    # Haare auf Koerper und Cheliceren: Wurzeln auf der Oberflaeche
    all_body = body_parts
    cand = []
    for part in pro + opi:
        if part.kind == 'ell':
            c, R, r = part.g['c'], part.g['R'], part.g['r']
            u = rng.normal(0, 1, (800, 3))
            u /= np.linalg.norm(u, axis=1, keepdims=True)
            cand.append(c + (u * r * 1.06) @ R.T)
        else:
            a, b = part.g['a'], part.g['b']
            t = rng.random(1400)[:, None]
            th = rng.uniform(0, 2 * math.pi, 1400)
            r0, r1 = part.g['r0'], part.g['r1']
            ry = r0[0] + (r1[0] - r0[0]) * t[:, 0]
            rz = r0[1] + (r1[1] - r0[1]) * t[:, 0]
            cand.append(a + (b - a) * t + np.stack([np.zeros(1400), np.cos(th) * ry, np.sin(th) * rz], axis=1) * 1.06)
    P = np.vstack(cand)
    P, N, d = project(all_body, P)
    ok = (np.abs(d) < 2.5e-4) & (N[:, 2] > -0.55)
    P, N = P[ok], N[ok]
    sel = rng.choice(len(P), size=min(len(P), 8000), replace=False)
    Wb = sdf.weights(all_body, P[sel], nb)
    for k, i in enumerate(sel):
        w = {int(b): float(x) for b, x in enumerate(Wb[k]) if x > 0.05}
        p, n = P[i], N[i]
        backward = p[0] < 0.0
        flow = unit(-X if backward else X + Z * 0.4)
        longh = rng.random() < 0.2
        L = rng.uniform(0.004, 0.009) if longh else rng.uniform(0.0012, 0.0032)
        a = rng.uniform(0.5, 1.1) if longh else rng.uniform(0.25, 0.6)
        tang = unit(flow - n * float(flow @ n))
        dd = n * math.sin(a) + tang * math.cos(a)
        hair.add(p - n * 0.0002, dd, L, 0.00025 if longh else 0.00016, tang * 0.4 - Z * 0.1 + rng.normal(0, 0.15, 3), w, rng.random() * (0.6 if longh else 1.4))
    for si, s in enumerate((1, -1)):  # Cheliceren oben und seitlich behaart
        cp = [A.ell(B_CH[si], (0.060, s * 0.0098, 0.0348), (1, 0, -0.15), (0.0190, 0.0100, 0.0138))]
        u = rng.normal(0, 1, (1400, 3))
        u /= np.linalg.norm(u, axis=1, keepdims=True)
        u = u[(u[:, 2] > -0.2) & (u[:, 0] < 0.7)]
        Pc = cp[0].g['c'] + (u * cp[0].g['r'] * 1.04) @ cp[0].g['R'].T
        Pc, Nc, dc = project(cp, Pc, 6)
        for k in range(170):
            t = rng.uniform(0.0, 0.75)
            c = Fp[0] + (Fp[2] - Fp[0]) * t if False else None
            seg = 0 if t < 0.5 else 1
            f = (t - 0.5 * seg) / 0.5
            fp = [np.array((q[0], s * q[1], q[2])) for q in ((0.075, 0.0092, 0.0348), (0.089, 0.0087, 0.0350), (0.100, 0.0080, 0.0322))]
            frr = ((0.0046, 0.0082), (0.0035, 0.0062), (0.0025, 0.0043))
            c = fp[seg] + (fp[seg + 1] - fp[seg]) * f
            ry = frr[seg][0] + (frr[seg + 1][0] - frr[seg][0]) * f
            rz = frr[seg][1] + (frr[seg + 1][1] - frr[seg][1]) * f
            th = rng.uniform(-1.9, 1.9)  # oben und seitlich
            n = np.array([0.0, math.sin(th), math.cos(th)])
            p = c + np.array([0.0, math.sin(th) * ry, math.cos(th) * rz]) * 0.9
            hair.add(p, unit(n * 0.7 + X * 0.7), rng.uniform(0.0025, 0.006), 0.0002, X * 0.2 + rng.normal(0, 0.12, 3), {B_CH[si]: 1.0}, rng.random())
        for i in range(min(len(Pc), 500)):
            p, n = Pc[i], Nc[i]
            L = rng.uniform(0.0018, 0.0065)
            dd = unit(n * 0.8 + X * 0.6)
            hair.add(p - n * 0.0002, dd, L, 0.00018, X * 0.25 + rng.normal(0, 0.15, 3), {B_CH[si]: 1.0}, rng.random())
    for lod in range(3):
        Vs, Ts, Ws, Cs, Es = [], [], [], [], []
        off = 0
        for (V, T, W, C, E) in lod_meshes[lod]:
            Vs.append(V), Ts.append(T + off), Ws.append(W), Cs.append(C), Es.append(E)
            off += len(V)
        lod_meshes[lod] = (np.vstack(Vs), np.vstack(Ts), np.vstack(Ws), np.concatenate(Cs), np.concatenate(Es))
    groups.append({'name': 'glieder', 'fm': 'ent_solifuge', 'lod_mesh': lod_meshes, 'parts': []})
    for lod in range(3):
        Vs, Ts, Ws, Cs, Es = [], [], [], [], []
        off = 0
        for (V, T, W, C, E) in palp_meshes[lod]:
            Vs.append(V), Ts.append(T + off), Ws.append(W), Cs.append(C), Es.append(E)
            off += len(V)
        palp_meshes[lod] = (np.vstack(Vs), np.vstack(Ts), np.vstack(Ws), np.concatenate(Cs), np.concatenate(Es))
    groups.append({'name': 'taster', 'fm': 'ent_solifuge_head', 'lod_mesh': palp_meshes, 'parts': []})
    for lod in range(3):
        Vs, Ts, Ws, Cs, Es = [], [], [], [], []
        off = 0
        for (V, T, W, C, E) in sleeves[lod]:
            if len(V):
                Vs.append(V), Ts.append(T + off), Ws.append(W), Cs.append(C), Es.append(E)
                off += len(V)
        sleeves[lod] = (np.vstack(Vs), np.vstack(Ts), np.vstack(Ws), np.concatenate(Cs), np.concatenate(Es))
    groups.append({'name': 'gelenke', 'fm': 'ent_solifuge_joint', 'lod_mesh': sleeves, 'parts': []})  # Runde 6e: heller (Bambus)
    groups.append({'name': 'haare', 'fm': 'ent_solifuge_hair', 'lod_mesh': [hair.mesh(0), hair.mesh(1), hair.mesh(2)], 'parts': []})
    # Krallen (dunkel, drei Seiten)
    cl = []
    for lod in range(3):
        h = Hair(nb)
        for (a, q, b) in claws[min(lod, 1)] if lod < 2 else []:
            h.add(a, q - a, float(np.linalg.norm(q - a)), 0.00035, -Z * 0.3, {b: 1.0}, 0.0)
        cl.append(h.mesh(0 if lod < 2 else 2))
    groups.append({'name': 'krallen', 'fm': 'ent_solifuge_dark', 'lod_mesh': cl, 'parts': []})
    # Faden aus der Hinterleibsspitze: 3 m nach oben (nur beim Abseilen zu sehen)
    tip = J[OPI2]
    fv, ft = [], []
    for z in (0.0, 3.0):
        for j in range(5):
            th = 2 * math.pi * j / 5
            fv.append(tip + np.array([math.cos(th) * 0.0006, math.sin(th) * 0.0006, z]))
    for j in range(5):
        j1 = (j + 1) % 5
        ft += [(j, j1, 5 + j1), (j, 5 + j1, 5 + j)]
    fw = np.zeros((10, nb))
    fw[:, B_DRAG] = 1.0
    fm = (np.array(fv), np.array(ft, dtype=np.int64), fw, np.zeros(10), np.zeros(10))
    groups.append({'name': 'faden', 'fm': 'ent_silk', 'lod_mesh': [fm, fm, fm], 'parts': []})
    return groups
