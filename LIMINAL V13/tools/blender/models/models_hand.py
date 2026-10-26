"""V13 (HAND_ANALYSE.md, E-020): linke Hand des Spielers - Viereck-Kaefig mit Subdivision, Skelett mit 21 Knochen,
Gewichten aus dem Bau und Posen (Griff um die Lampe, Daumendruck auf den Schalter, locker).

Statt vereinigter Ellipsoide (V12) entsteht die Hand wie beim Box-Modelling: ein Handteller als Gitter (Ruecken,
Flaeche, Seiten), an dessen Stirnseite vier Fingerroehren und an dessen Daumenseite die Daumenroehre ansetzen,
hinten das Handgelenk mit dem Unterarmansatz. Ringe aus je acht Punkten geben jedem Glied seinen Querschnitt
(flacher Ruecken, Polster auf der Beugeseite, Knoechel und Beugefalten an den Gelenken). Catmull-Clark glaettet den
Kaefig zur Haut. Die Naegel sind eingesetzte Flaechen der Haut (eigenes Material). Gewichte werden Ring fuer
Ring vergeben (an den Gelenken gemischt) und von der Subdivision weich verteilt.

Masse: erwachsene Hand (Laenge ~18,5 cm), Fingerglieder nach anthropometrischen Mittelwerten (Grundglied Zeige
40 mm, Mittel 44,5 mm usw.). Handrahmen beim Bau: Handgelenk im Ursprung, +x zu den Fingern, +z Handruecken,
-y Daumenseite (linke Hand, Handflaeche unten). Danach wird alles in den Rahmen der Lampe gesetzt (Ursprung in der
Griffmitte auf der Achse, +x zum Glas, +z oben = Seite des Schalters unter dem Daumen, +y links), wie in V12.

Der Griff wird geloest, nicht von Hand eingestellt: die Lampe liegt schraeg in der Hohlhand (vom Kleinfinger-
ballen zur Falte zwischen Daumen und Zeigefinger); Ring- und Kleinfinger-Mittelhand beugen sich mit (Querwoelbung);
je Finger sucht ein Raster Grund-, Mittel- und Endgelenk so, dass die Beugeseite der Glieder anliegt, ohne mehr als
1 mm einzudringen (Haut gibt nach); der Daumen liegt laengs auf dem Schaft, seine Kuppe ueber dem Schalter.
"""
import math

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

import common

RB = 0.0165      # Schaftradius der Lampe (models_vm)
SKIN = (0.80, 0.63, 0.54)

# --- Masse ---------------------------------------------------------------------------------------------------
# Name, Grundgelenk (MCP), Richtung in Ruhe (Grad, + zur Kleinfingerseite), Glieder (Grund, Mittel, End bis zur
# Kuppe), Breite und Dicke am Mittelgelenk
FING = [
    ('f0', (0.0880, -0.0255, 0.0000), -3.0, (0.0400, 0.0235, 0.0195), 0.0188, 0.0164),
    ('f1', (0.0925, -0.0065, 0.0005), 0.0, (0.0445, 0.0275, 0.0200), 0.0190, 0.0168),
    ('f2', (0.0875, 0.0120, -0.0012), 3.5, (0.0415, 0.0265, 0.0195), 0.0178, 0.0158),
    ('f3', (0.0775, 0.0288, -0.0040), 9.0, (0.0330, 0.0190, 0.0175), 0.0156, 0.0140),
]
TH_CMC = Vector((0.0200, -0.0185, -0.0090))
TH_LEN = (0.0460, 0.0310, 0.0280)            # Mittelhand, Grundglied, Endglied bis zur Kuppe
TH_DIRS = [Vector((0.640, -0.660, -0.330)), Vector((0.700, -0.640, -0.230)), Vector((0.780, -0.590, -0.130))]
TH_NAIL = Vector((0.05, -0.80, 0.60))         # Richtung des Daumennagels (vor dem Orthogonalisieren)
INFL = 1.12      # Kaefig groesser als die Zielform: Catmull-Clark zieht Ringe aus 8 Punkten um ~10 % zusammen

# Ringpunkte (s quer, h hoch) in Kaefig-Reihenfolge. Finger: s+ = Kleinfingerseite; Daumen: s+ = Zeigefingerseite
P8_F = [(-0.70, 0.80), (0.0, 1.0), (0.70, 0.80), (1.0, -0.05), (0.72, -0.84), (0.0, -1.0), (-0.72, -0.84), (-1.0, -0.05)]
P8_T = [(-0.72, -0.84), (0.0, -1.0), (0.72, -0.84), (1.0, -0.05), (0.70, 0.80), (0.0, 1.0), (-0.70, 0.80), (-1.0, -0.05)]


def _norm(v):
    return v.normalized()


def _finger_dir(deg):
    a = math.radians(deg)
    return Vector((math.cos(a), math.sin(a), 0.0))


def _smooth(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


class Cage:
    """Kaefig im Bau: Punkte mit Gewichten (Knochen -> Anteil) und Toenung (Roete, Dunkel)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.w = {}
        self.tint = {}
        self.nail_faces = []

    def v(self, co, weights, tint=(0.0, 0.0)):
        vert = self.bm.verts.new(co)
        s = sum(weights.values())
        self.w[vert] = {k: x / s for k, x in weights.items() if x > 1e-4}
        self.tint[vert] = tint
        return vert

    def quad(self, a, b, c, d):
        return self.bm.faces.new((a, b, c, d))

    def bridge(self, r0, r1):
        n = len(r0)
        return [self.quad(r0[i], r0[(i + 1) % n], r1[(i + 1) % n], r1[i]) for i in range(n)]

    def cap8(self, ring, center):
        return [self.quad(ring[2 * m], ring[2 * m + 1], ring[(2 * m + 2) % 8], center) for m in range(4)]


def _ring(cage, c, s, h, w, hd, hp, order, weights, tint=(0.0, 0.0), dorsal_bump=0.0, palmar_crease=0.0,
          dorsal_tint=None, palmar_tint=None):
    """Acht Kaefigpunkte um c (Querachse s, Hochachse h), Halbbreite w, Hoehe ueber/unter der Achse hd/hp."""
    out = []
    for (a, b) in order:
        hh = b * (hd if b > 0 else hp)
        p = c + s * (a * w * INFL) + h * (hh * INFL)
        if a == 0.0 and b > 0:
            p = p + h * dorsal_bump
        if a == 0.0 and b < 0:
            p = p + h * palmar_crease
        t = tint
        if b > 0.5 and dorsal_tint is not None:
            t = dorsal_tint
        if b < -0.5 and palmar_tint is not None:
            t = palmar_tint
        out.append(cage.v(p, weights, t))
    return out


# --- Knochen (Ruhe) ------------------------------------------------------------------------------------------
def bone_layout():
    """Ruhepose: Name -> (Kopf, Ende, Rueckenrichtung, Eltern)."""
    Z = Vector((0, 0, 1))
    # Wurzel ist die Hand (sie haengt starr an der Lampe); der Unterarm zeigt vom Handgelenk zurueck und traegt die
    # Haltung des Handgelenks (Beugen/Strecken, Abknicken zur Elle)
    B = {'hand': (Vector((0, 0, 0)), Vector((0.050, 0, 0)), Z, None),
         'forearm': (Vector((0, 0, 0)), Vector((-0.10, 0, 0.002)), Z, 'hand')}
    for (n, mcp, deg, lens, wd, th) in FING:
        m = Vector(mcp)
        base = Vector((0.016, m.y * 0.55, m.z * 0.5))
        B[n + '_mc'] = (base, m, Z, 'hand')
        u = _finger_dir(deg)
        p = [m, m + u * lens[0], m + u * (lens[0] + lens[1]), m + u * sum(lens)]
        for k in range(3):
            B[f'{n}_{k + 1}'] = (p[k], p[k + 1], Z, (n + '_mc') if k == 0 else f'{n}_{k}')
    p = [TH_CMC]
    for k in range(3):
        p.append(p[-1] + _norm(TH_DIRS[k]) * TH_LEN[k])
    for k in range(3):
        u = _norm(p[k + 1] - p[k])
        nail = _norm(TH_NAIL - u * TH_NAIL.dot(u))
        B[f'th_{k}'] = (p[k], p[k + 1], nail, 'hand' if k == 0 else f'th_{k - 1}')
    return B


# --- Kaefig --------------------------------------------------------------------------------------------------
def build_cage():
    cg = Cage()
    B = bone_layout()
    # Handteller: Stationen i = 0..4 (Handgelenk bis vor die Knoechel), Spalten j = 0..8 (Daumenseite -> Kleinfinger)
    mcp_x = [f[1][0] for f in FING]
    mcp_y = [f[1][1] for f in FING]
    half_w = [0.5 * f[4] * 1.06 for f in FING]
    # Spaltengrenzen an der Stirnseite: Finger k belegt die Spalten 2k..2k+2
    yb = [mcp_y[0] - half_w[0]]
    for k in range(3):
        yb.append(mcp_y[k] + 0.5 * (mcp_y[k + 1] - mcp_y[k]))
    yb.append(mcp_y[3] + half_w[3])
    ydist = []
    for k in range(4):
        ydist += [yb[k], mcp_y[k]]
    ydist.append(yb[4])
    ywr = [-0.0265 + 0.053 * j / 8 for j in range(9)]
    xdist = [mcp_x[min(3, j // 2)] if j % 2 else 0.5 * (mcp_x[max(0, j // 2 - 1)] + mcp_x[min(3, j // 2)]) for j in range(9)]
    xdist[0], xdist[8] = mcp_x[0], mcp_x[3]
    ST = [0.0, 0.20, 0.42, 0.63, 0.82]   # Anteil bis zum Grundgelenk

    def mc_weights(y):
        """Mittelhand nach Querlage (zwischen den Fingermitten linear)."""
        if y <= mcp_y[0]:
            return {'f0_mc': 1.0}
        if y >= mcp_y[3]:
            return {'f3_mc': 1.0}
        for k in range(3):
            if mcp_y[k] <= y <= mcp_y[k + 1]:
                t = (y - mcp_y[k]) / (mcp_y[k + 1] - mcp_y[k])
                return {f'f{k}_mc': 1 - t, f'f{k + 1}_mc': t}

    def palm_weights(x, y):
        fa = 1.0 - _smooth(-0.008, 0.010, x)
        w = {'forearm': fa}
        rest = 1.0 - fa
        m = _smooth(0.040, 0.078, x) * 0.85
        w['hand'] = rest * (1 - m)
        for k, x2 in mc_weights(y).items():
            w[k] = w.get(k, 0.0) + rest * m * x2
        return w

    def zt(i, j):  # Handruecken
        u = (ydist[j] if i > 0 else ywr[j])
        f = ST[i]
        y = ywr[j] + (ydist[j] - ywr[j]) * f
        half = 0.0265 + (0.040 - 0.0265) * f
        un = y / half
        if i == 0:
            return 0.0010 + 0.0135 * math.sqrt(max(0.0, 1 - min(1.0, un * un) * 0.93))
        return 0.0075 + 0.0040 * (1 - un * un) + 0.0012 * f

    def zb(i, j):  # Handflaeche mit Ballen
        f = ST[i]
        y = ywr[j] + (ydist[j] - ywr[j]) * f
        half = 0.0265 + (0.040 - 0.0265) * f
        un = y / half
        if i == 0:
            return -0.0015 - 0.0150 * math.sqrt(max(0.0, 1 - min(1.0, un * un) * 0.93))
        z = -0.0125
        # Daumenballen (Daumenseite, nahe am Handgelenk), Kleinfingerballen, Polster unter den Grundgelenken,
        # flache Hohlhand in der Mitte
        z -= 0.0095 * math.exp(-((f - 0.36) / 0.22) ** 2) * math.exp(-((un + 0.85) / 0.45) ** 2)
        z -= 0.0075 * math.exp(-((f - 0.40) / 0.28) ** 2) * math.exp(-((un - 0.80) / 0.40) ** 2)
        z -= 0.0040 * math.exp(-((f - 0.86) / 0.12) ** 2)
        z += 0.0020 * math.exp(-((f - 0.55) / 0.18) ** 2) * math.exp(-(un / 0.5) ** 2)
        return z

    top, bot, midR, midU = {}, {}, {}, {}
    for i in range(5):
        f = ST[i]
        for j in range(9):
            x = xdist[j] * f
            y = ywr[j] + (ydist[j] - ywr[j]) * f
            # die Ruecken- und Flaechenpunkte der Seitenspalten liegen etwas innen, die Seitenmitte traegt die Kante
            if j in (0, 8):
                y *= 0.97
            red = 0.0
            top[i, j] = cg.v(Vector((x, y, zt(i, j))), palm_weights(x, y), (red, 0.0))
            dark = 0.10 if (i in (2, 3) and 2 <= j <= 6) else 0.0  # Handlinien angedeutet
            bot[i, j] = cg.v(Vector((x, y, zb(i, j))), palm_weights(x, y), (0.12, dark))
        xr, xu = xdist[0] * f, xdist[8] * f
        yr = (ywr[0] + (ydist[0] - ywr[0]) * f) - 0.0040 - 0.0040 * math.sin(math.pi * min(1.0, f / 0.7))
        yu = (ywr[8] + (ydist[8] - ywr[8]) * f) + 0.0040 + 0.0030 * math.sin(math.pi * min(1.0, f / 0.8))
        midR[i] = cg.v(Vector((xr, yr, -0.0035 - 0.004 * f)), palm_weights(xr, yr))
        midU[i] = cg.v(Vector((xu, yu, -0.0030 - 0.002 * f)), palm_weights(xu, yu))
    # Daumenballen-Punkte folgen teilweise der Daumen-Mittelhand
    for i in (1, 2, 3):
        for j in (0, 1, 2):
            w = cg.w[bot[i, j]]
            k = 0.40 if j == 0 else (0.28 if j == 1 else 0.12)
            for b in list(w):
                w[b] *= (1 - k)
            w['th_0'] = w.get('th_0', 0.0) + k
    # Station 5: Fingeransatz (Ruecken am Knoechel, Beugeseite ein Stueck weiter vorn, Seiten = Fingerfalten)
    top5, bot5, mid5 = {}, {}, {}
    for j in range(9):
        k0, k1 = (j - 1) // 2, j // 2
        ks = sorted({min(3, max(0, k0)), min(3, max(0, k1))}) if j % 2 == 0 else [j // 2]
        cx = sum(FING[k][1][0] for k in ks) / len(ks)
        cz = sum(FING[k][1][2] for k in ks) / len(ks)
        y = ydist[j]
        hd = 0.0098 if j % 2 else 0.0080
        shared = len(ks) == 2

        def wts(mc_share, p1_share):
            w = {}
            for k in ks:
                w[FING[k][0] + '_mc'] = w.get(FING[k][0] + '_mc', 0) + mc_share / len(ks)
                w[FING[k][0] + '_1'] = w.get(FING[k][0] + '_1', 0) + p1_share / len(ks)
            return w

        knuckle = 0.0024 if j % 2 else 0.0
        top5[j] = cg.v(Vector((cx - 0.0060, y, cz + hd + knuckle)), wts(0.85, 0.15), (0.22 if j % 2 else 0.05, 0.0))
        bot5[j] = cg.v(Vector((cx + 0.0125, y, cz - 0.0120)), wts(0.40, 0.60), (0.18, 0.20 if shared else 0.05))
        if j % 2 == 0:
            yy = y + (-0.0030 if j == 0 else (0.0030 if j == 8 else 0.0))
            mid5[j] = cg.v(Vector((cx + 0.0110 + (0.004 if shared else 0.0), yy, cz - 0.0010)), wts(0.50, 0.50),
                           (0.05, 0.35 if shared else 0.0))
    # Flaechen des Handtellers
    for i in range(4):
        for j in range(8):
            cg.quad(top[i, j], top[i, j + 1], top[i + 1, j + 1], top[i + 1, j])
            cg.quad(bot[i, j + 1], bot[i, j], bot[i + 1, j], bot[i + 1, j + 1])
    for j in range(8):
        cg.quad(top[4, j], top[4, j + 1], top5[j + 1], top5[j])
        cg.quad(bot[4, j + 1], bot[4, j], bot5[j], bot5[j + 1])
    # Seiten: Kleinfingerseite ganz, Daumenseite ohne das Feld i = 1..3 (dort setzt der Daumen an)
    for i in range(4):
        cg.quad(top[i, 8], midU[i], midU[i + 1], top[i + 1, 8])
        cg.quad(midU[i], bot[i, 8], bot[i + 1, 8], midU[i + 1])
        if i in (0, 3):
            cg.quad(midR[i], top[i, 0], top[i + 1, 0], midR[i + 1])
            cg.quad(bot[i, 0], midR[i], midR[i + 1], bot[i + 1, 0])
    cg.quad(top[4, 8], midU[4], mid5[8], top5[8])
    cg.quad(midU[4], bot[4, 8], bot5[8], mid5[8])
    cg.quad(midR[4], top[4, 0], top5[0], mid5[0])
    cg.quad(bot[4, 0], midR[4], mid5[0], bot5[0])

    # --- Finger ---------------------------------------------------------------------------------------------
    for k, (n, mcp, deg, lens, wd, th) in enumerate(FING):
        base = [top5[2 * k], top5[2 * k + 1], top5[2 * k + 2], mid5[2 * k + 2], bot5[2 * k + 2], bot5[2 * k + 1],
                bot5[2 * k], mid5[2 * k]]
        m = Vector(mcp)
        u = _finger_dir(deg)
        Z = Vector((0, 0, 1))
        s = u.cross(Z) * -1.0  # zur Kleinfingerseite (+y)
        s = Vector((-u.y, u.x, 0.0))
        s.normalize()
        L1, L2, L3 = lens
        hw = wd * 0.5
        tk = th
        # Halbachsen je Ring: (Breite, Ruecken ueber der Achse, Beugeseite unter der Achse)
        def dims(scale_w, scale_t):
            return hw * scale_w, tk * 0.42 * scale_t, tk * 0.58 * scale_t
        J = [0.0, L1, L1 + L2, L1 + L2 + L3]

        def wt(t):
            """Gewichte entlang des Fingers (t ab dem Grundgelenk)."""
            if t < J[1] - 0.005:
                a = _smooth(0.004, 0.012, t)
                return {n + '_mc': 1 - a, n + '_1': a} if a < 1 else {n + '_1': 1.0}
            if t < J[1] + 0.005:
                a = _smooth(J[1] - 0.005, J[1] + 0.005, t)
                return {n + '_1': 1 - a, n + '_2': a}
            if t < J[2] - 0.004:
                return {n + '_2': 1.0}
            if t < J[2] + 0.004:
                a = _smooth(J[2] - 0.004, J[2] + 0.004, t)
                return {n + '_2': 1 - a, n + '_3': a}
            return {n + '_3': 1.0}

        rings_t = [
            # t, Breite, Dicke, Knoechel (Ruecken), Falte (Beugeseite), Toenung Ruecken/Beugeseite
            (0.016, 0.98, 0.98, 0.0, 0.0006, None, None),
            (L1 * 0.55, 0.93, 0.95, 0.0, 0.0, None, None),
            (L1 - 0.0045, 0.97, 0.98, 0.0, 0.0, None, None),
            (L1, 1.00, 1.00, 0.0016, 0.0015, (0.30, 0.0), (0.10, 0.45)),
            (L1 + 0.0045, 0.97, 0.97, 0.0, 0.0, None, None),
            (L1 + L2 * 0.5, 0.91, 0.90, 0.0, 0.0, None, None),
            (L1 + L2 - 0.0035, 0.90, 0.87, 0.0, 0.0, None, None),
            (L1 + L2, 0.91, 0.85, 0.0011, 0.0012, (0.24, 0.0), (0.10, 0.40)),
            (L1 + L2 + 0.0040, 0.88, 0.82, 0.0, 0.0, None, (0.25, 0.0)),
            (L1 + L2 + L3 * 0.55, 0.86, 0.76, 0.0, 0.0, None, (0.32, 0.0)),
            (L1 + L2 + L3 * 0.86, 0.72, 0.58, 0.0, 0.0, None, (0.38, 0.0)),
        ]
        prev = base
        rings = []
        for (t, sw, st, bump, crease, dt, pt) in rings_t:
            w_, hd, hp = dims(sw, st)
            c = m + u * t
            r = _ring(cg, c, s, Z, w_, hd, hp, P8_F, wt(t), (0.06, 0.0), dorsal_bump=bump, palmar_crease=crease,
                      dorsal_tint=dt, palmar_tint=pt)
            cg.bridge(prev, r)
            rings.append(r)
            prev = r
        tip = cg.v(m + u * J[3] + Z * (-tk * 0.12), {n + '_3': 1.0}, (0.40, 0.0))
        cg.cap8(prev, tip)
        # Nagel: Rueckenflaechen zwischen den letzten drei Ringen
        for a, b in ((rings[-3], rings[-2]), (rings[-2], rings[-1])):
            for i0 in (7, 0):
                i1 = (i0 + 1) % 8
                f = cg.bm.faces.get((a[i0], a[i1], b[i1], b[i0])) if hasattr(cg.bm.faces, 'get') else None
                if f is None:
                    for ff in a[i0].link_faces:
                        if set(ff.verts) == {a[i0], a[i1], b[i1], b[i0]}:
                            f = ff
                cg.nail_faces.append(f)

    # --- Daumen ---------------------------------------------------------------------------------------------
    patch = [bot[1, 0], bot[2, 0], bot[3, 0], midR[3], top[3, 0], top[2, 0], top[1, 0], midR[1]]
    hb = B
    th_pts = [hb['th_0'][0], hb['th_1'][0], hb['th_2'][0], hb['th_2'][1]]
    tl = [0.0, TH_LEN[0], TH_LEN[0] + TH_LEN[1], sum(TH_LEN)]

    def th_frame(t):
        k = 0 if t < tl[1] else (1 if t < tl[2] else 2)
        a, b, nail = hb[f'th_{k}'][0], hb[f'th_{k}'][1], hb[f'th_{k}'][2]
        u = _norm(b - a)
        c = a + u * (t - tl[k])
        h = _norm(nail - u * nail.dot(u))
        s = _norm(h.cross(u))  # zur Zeigefingerseite
        if s.x < 0:
            s = -s
        return c, u, s, h

    def th_wt(t):
        if t < 0.024:
            a = _smooth(0.006, 0.024, t)
            return {'hand': 0.35 * (1 - a), 'th_0': 0.65 + 0.35 * a}
        if t < tl[1] + 0.0055:
            a = _smooth(tl[1] - 0.0055, tl[1] + 0.0055, t)
            return {'th_0': 1 - a, 'th_1': a}
        if t < tl[2] - 0.0045:
            return {'th_1': 1.0}
        if t < tl[2] + 0.0045:
            a = _smooth(tl[2] - 0.0045, tl[2] + 0.0045, t)
            return {'th_1': 1 - a, 'th_2': a}
        return {'th_2': 1.0}

    for v in patch:
        w = cg.w[v]
        for b in list(w):
            w[b] *= 0.55
        w['th_0'] = w.get('th_0', 0.0) + 0.45
    TW, TT = 0.0215, 0.0185
    th_rings = [
        (0.030, 1.18, 1.25, 0.0, 0.0, None, None),
        (tl[1] - 0.0045, 1.02, 1.02, 0.0, 0.0, None, None),
        (tl[1], 1.03, 1.03, 0.0018, 0.0012, (0.26, 0.0), (0.10, 0.35)),
        (tl[1] + 0.0050, 0.98, 0.98, 0.0, 0.0, None, None),
        (tl[1] + TH_LEN[1] * 0.55, 0.94, 0.92, 0.0, 0.0, None, None),
        (tl[2] - 0.0035, 0.93, 0.88, 0.0, 0.0, None, None),
        (tl[2], 0.94, 0.86, 0.0012, 0.0012, (0.22, 0.0), (0.10, 0.40)),
        (tl[2] + 0.0045, 0.92, 0.84, 0.0, 0.0, None, (0.25, 0.0)),
        (tl[2] + TH_LEN[2] * 0.55, 0.90, 0.78, 0.0, 0.0, None, (0.32, 0.0)),
        (tl[2] + TH_LEN[2] * 0.86, 0.74, 0.58, 0.0, 0.0, None, (0.38, 0.0)),
    ]
    prev = patch
    rings = []
    for (t, sw, st, bump, crease, dt, pt) in th_rings:
        c, u, s, h = th_frame(t)
        r = _ring(cg, c, s, h, TW * 0.5 * sw, TT * 0.42 * st, TT * 0.58 * st, P8_T, th_wt(t), (0.06, 0.0),
                  dorsal_bump=bump, palmar_crease=crease, dorsal_tint=dt, palmar_tint=pt)
        cg.bridge(prev, r)
        rings.append(r)
        prev = r
    c, u, s, h = th_frame(sum(TH_LEN))
    tip = cg.v(c - h * (TT * 0.12), {'th_2': 1.0}, (0.40, 0.0))
    cg.cap8(prev, tip)
    for a, b in ((rings[-3], rings[-2]), (rings[-2], rings[-1])):
        for i0 in (4, 5):  # Rueckenpunkte der Daumenreihenfolge: 4, 5, 6
            i1 = i0 + 1
            for ff in a[i0].link_faces:
                if set(ff.verts) == {a[i0], a[i1], b[i1], b[i0]}:
                    cg.nail_faces.append(ff)

    # --- Handgelenk und Unterarmansatz ------------------------------------------------------------------------
    wr = [top[0, j] for j in range(9)] + [midU[0]] + [bot[0, j] for j in range(8, -1, -1)] + [midR[0]]
    prev = wr
    # nur so weit, wie das Buendchen des Aermels deckt; schlanker als der Aermel
    for (x, sc, fa) in ((-0.012, 1.00, 0.8), (-0.026, 0.98, 1.0), (-0.040, 0.96, 1.0)):
        r = []
        for v in wr:
            p = v.co.copy()
            q = Vector((x, p.y * sc, (p.z + 0.001) * sc))
            r.append(cg.v(q, {'forearm': fa, 'hand': 1 - fa}))
        cg.bridge(prev, r)
        prev = r
    end = cg.v(Vector((-0.043, 0.0, 0.001)), {'forearm': 1.0})
    for i in range(len(prev)):
        cg.bm.faces.new((prev[i], prev[(i + 1) % len(prev)], end))
    return cg


def _to_object(cg, skin, nail):
    bm = cg.bm
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in cg.nail_faces:
        if f is not None:
            f.material_index = 1
    uvl = bm.loops.layers.uv.new('UVMap')
    for f in bm.faces:
        for lp in f.loops:
            lp[uvl].uv = cg.tint[lp.vert]
    bm.verts.index_update()
    weights = [(v.index, cg.w[v]) for v in bm.verts]
    me = bpy.data.meshes.new('hand')
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new('hand', me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(skin)
    me.materials.append(nail)
    groups = {}
    for idx, w in weights:
        for b, x in w.items():
            if b not in groups:
                groups[b] = ob.vertex_groups.new(name=b)
            groups[b].add([idx], x, 'REPLACE')
    return ob


def _apply_subsurf(ob, levels=2):
    mod = ob.modifiers.new('sub', 'SUBSURF')
    mod.levels = levels
    mod.render_levels = levels
    mod.uv_smooth = 'PRESERVE_BOUNDARIES'
    bpy.context.view_layer.objects.active = ob
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob]):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    ob.data.shade_smooth()


def _armature():
    B = bone_layout()
    arm = bpy.data.armatures.new('hand_rig')
    ob = bpy.data.objects.new('hand_rig', arm)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode='EDIT')
    eb = {}
    for name, (h, t, up, par) in B.items():
        e = arm.edit_bones.new(name)
        e.head, e.tail = h, t
        e.align_roll(up)
        e.use_deform = True
        eb[name] = e
    for name, (h, t, up, par) in B.items():
        if par:
            eb[name].parent = eb[par]
            eb[name].use_connect = False
    # Markierung ohne Haut: Ellbogenseite des Unterarms (das Spiel liest daraus die Richtung des Unterarms)
    e = arm.edit_bones.new('elbow')
    e.head = eb['forearm'].tail
    e.tail = eb['forearm'].tail + (eb['forearm'].tail - eb['forearm'].head).normalized() * 0.01
    e.align_roll(Vector((0, 0, 1)))
    e.use_deform = False
    e.parent = eb['forearm']
    bpy.ops.object.mode_set(mode='OBJECT')
    return ob


# --- Vorwaertskinematik und Loeser (numpy) ---------------------------------------------------------------------
class Rig:
    def __init__(self, arm_ob, mesh_ob):
        order, seen = [], set()
        bones = list(arm_ob.data.bones)
        while len(order) < len(bones):
            for b in bones:
                if b.name not in seen and (b.parent is None or b.parent.name in seen):
                    order.append(b)
                    seen.add(b.name)
        self.names = [b.name for b in order]
        self.bones = order
        self.idx = {n: i for i, n in enumerate(self.names)}
        self.parent = [self.idx[b.parent.name] if b.parent else -1 for b in order]
        self.rest = [np.array(b.matrix_local) for b in order]
        self.rest_inv = [np.linalg.inv(m) for m in self.rest]
        me = mesh_ob.data
        self.V = np.array([v.co for v in me.vertices])
        self.N = np.array([v.normal for v in me.vertices])
        W = np.zeros((len(me.vertices), len(self.names)))
        gi = {g.index: self.idx[g.name] for g in mesh_ob.vertex_groups}
        for v in me.vertices:
            for g in v.groups:
                W[v.index, gi[g.group]] = g.weight
        W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
        self.W = W
        self.Vh = np.hstack([self.V, np.ones((len(self.V), 1))])

    def fk(self, rot):
        """rot: Name -> Quaternion (im Knochenraum). Rueckgabe: Liste der Pose-Matrizen (Armature-Raum)."""
        M = [None] * len(self.names)
        for i, n in enumerate(self.names):
            q = rot.get(n)
            R = np.array(q.to_matrix().to_4x4()) if q is not None else np.eye(4)
            p = self.parent[i]
            if p < 0:
                M[i] = self.rest[i] @ R
            else:
                M[i] = M[p] @ (self.rest_inv[p] @ self.rest[i]) @ R
        return M

    def skin(self, M, sel=None):
        S = np.stack([M[i] @ self.rest_inv[i] for i in range(len(M))])  # B x 4 x 4
        W = self.W if sel is None else self.W[sel]
        Vh = self.Vh if sel is None else self.Vh[sel]
        P = np.einsum('vb,bij,vj->vi', W, S, Vh)
        return P[:, :3]


def q_axis(axis, deg):
    return Quaternion(Vector(axis), math.radians(deg))


def flex(deg, abd=0.0, twist=0.0):
    """Beugung (Fingerspitze zur Beugeseite) im Knochenraum: Knochen-x zeigt zur Daumenseite (Y laengs, Z Ruecken)."""
    q = q_axis((1, 0, 0), -deg)
    if abd:
        q = q_axis((0, 0, 1), abd) @ q
    if twist:
        q = q_axis((0, 1, 0), twist) @ q
    return q


def axis_dist(P, c, a):
    d = P - c
    t = d @ a
    return np.linalg.norm(d - np.outer(t, a), axis=1), t


def make_hand(levels=2):
    """Haut (mit Gewichten, Subdivision angewendet) und Skelett in Ruhe, Handrahmen."""
    skin = common.material('haut', SKIN, look='plastic', rough=0.55, fm='hand_skin')
    nail = common.material('nagel', (0.88, 0.74, 0.70), look='plastic', rough=0.30, fm='hand_nail')
    cg = build_cage()
    ob = _to_object(cg, skin, nail)
    if levels:
        _apply_subsurf(ob, levels)
    arm = _armature()
    ob.parent = arm
    mod = ob.modifiers.new('skelett', 'ARMATURE')
    mod.object = arm
    return ob, arm


def _set_pose(arm, rot):
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
        pb.rotation_quaternion = rot.get(pb.name, Quaternion())
    bpy.context.view_layer.update()


def _lamp_cylinder(c, a, name='lampe_vorschau'):
    """Vorschau: Schaft als Zylinder auf der Achse (nur zum Pruefen)."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=48, radius1=RB, radius2=RB, depth=0.23)
    ob = common.obj_from_bmesh(name, bm, common.material('schaft', (0.08, 0.08, 0.09), look='metal', rough=0.4))
    z = Vector((0, 0, 1))
    q = z.rotation_difference(Vector(a))
    ob.matrix_world = Matrix.Translation(Vector(c) + Vector(a) * 0.03) @ q.to_matrix().to_4x4()
    return ob


def build_posed(pose='grip', lamp=True):
    """Hand im Griff (Vorschau und Bau): loest den Griff, setzt die Pose im Skelett."""
    from models import hand_grip as G
    ob, arm = make_hand(2)
    rig = Rig(arm, ob)
    log = []
    rot, c, a, tx = G.solve(rig, log)
    for line in log:
        print(line, flush=True)
    _set_pose(arm, rot)
    # Kontrolle: eigene Vorwaertskinematik = Blender
    M = rig.fk(rot)
    err = max(float(np.abs(M[i] - np.array(arm.pose.bones[n].matrix)).max()) for i, n in enumerate(rig.names))
    print(f'FK-Abweichung zu Blender: {err:.2e}', flush=True)
    if lamp:
        _lamp_cylinder(c, a)
    build_posed.result = (rot, c, a, rig)
    return ob, arm


def _apply_modifier(ob, name):
    bpy.context.view_layer.objects.active = ob
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob]):
        bpy.ops.object.modifier_apply(modifier=name)


def _flatten_contact(ob, c, a, inner=0.0003, iters=4):
    """Haut, die in den Schaft ragt, legt sich an ihn an (mit weichem Uebergang zu den Nachbarn)."""
    me = ob.data
    V = np.array([v.co for v in me.vertices])
    d, t = axis_dist(V, c, a)
    radial = V - c - np.outer(t, a)
    radial /= np.maximum(np.linalg.norm(radial, axis=1, keepdims=True), 1e-9)
    push = np.maximum(0.0, (RB - inner) - d)
    nb = [[] for _ in range(len(V))]
    for e in me.edges:
        i, j = e.vertices
        nb[i].append(j)
        nb[j].append(i)
    p = push.copy()
    for _ in range(iters):
        q = p.copy()
        for i in np.nonzero(p > 0)[0]:
            for j in nb[i]:
                q[j] = max(q[j], 0.5 * p[i])
        p = np.maximum(push, q)
    V = V + radial * p[:, None]
    for v, co in zip(me.vertices, V):
        v.co = co
    me.update()
    return int((push > 0).sum()), float(push.max())


def _apply_pose_as_rest(arm):
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.armature_apply(selected=False)
    bpy.ops.object.mode_set(mode='OBJECT')


def _transform_all(ob, arm, M):
    """Haut und Ruhepose des Skeletts mit M (4x4) bewegen."""
    ob.data.transform(M)
    ob.data.update()
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode='EDIT')
    for eb in arm.data.edit_bones:
        eb.transform(M, scale=True, roll=True)
    bpy.ops.object.mode_set(mode='OBJECT')


def _make_action(arm, name, rot):
    """Eine Pose als Aktion mit einem Schluessel (glTF: Animation mit diesem Namen)."""
    ad = arm.animation_data or arm.animation_data_create()
    act = bpy.data.actions.new(name)
    ad.action = act
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
        pb.rotation_quaternion = rot.get(pb.name, Quaternion())
        pb.location = (0, 0, 0)
        pb.scale = (1, 1, 1)
        pb.keyframe_insert('rotation_quaternion', frame=1)
        pb.keyframe_insert('location', frame=1)
    tr = ad.nla_tracks.new()
    tr.name = name
    tr.strips.new(name, 1, act)
    ad.action = None
    return act


PALM_WANT = (0.0, -1.0, 0.0)  # Handflaeche im Lampenrahmen (Blender: y links): nach rechts zur Koerpermitte


def build_final(log):
    """Hand im Rahmen der Lampe, Griff = Ruhepose, Aktionen grip und press. Rueckgabe: Kennwerte."""
    from models import hand_grip as G
    ob, arm = build_posed('grip', lamp=False)
    rot, c, a, rig = build_posed.result
    # Handgelenk: der Unterarm (Kind der Hand) knickt zur Elle ab und streckt (Grad aus hand_grip)
    q_wrist = Quaternion(Vector((0, 0, 1)), math.radians(-G.WRIST_ULNAR)) @ Quaternion(Vector((1, 0, 0)), math.radians(G.WRIST_EXT))
    rot = dict(rot)
    rot['forearm'] = q_wrist
    _set_pose(arm, rot)
    _apply_modifier(ob, 'skelett')
    n, mx = _flatten_contact(ob, c, a)
    log.append(f'KONTAKT: {n} Punkte an den Schaft gelegt (bis {mx * 1000:.2f} mm)')
    _apply_pose_as_rest(arm)
    mod = ob.modifiers.new('skelett', 'ARMATURE')
    mod.object = arm
    # Rahmen der Lampe: x Achse zum Glas, Ursprung unter der Mitte der Handflaeche. Die Drehung um die Achse legt
    # die Handflaeche zur Koerpermitte und etwas nach unten (PALM_WANT, Lampenrahmen, Blender: y links); der Daumen
    # liegt damit oben am Schaft
    rig = Rig(arm, ob)
    bones = arm.data.bones
    fh = np.array([0.0, 0.0, -1.0])  # Handflaechennormale im Handrahmen
    p = fh - a * (fh @ a)
    p /= np.linalg.norm(p)
    pc = np.cross(a, p)
    q = np.array(PALM_WANT[1:]) / np.linalg.norm(PALM_WANT[1:])
    y = q[0] * p - q[1] * pc
    up = q[1] * p + q[0] * pc
    x = a
    palm_c = np.array([G.PALM_C[0], G.PALM_C[1], 0.0])
    t0 = float((palm_c - c) @ a)
    o = c + a * t0
    th2 = rig.W[:, rig.idx['th_2']]
    V = rig.V
    d, t = axis_dist(V, c, a)
    sel = np.where(th2 > 0.9)[0]
    pad = sel[np.argmin(d[sel])]
    rv = V[pad] - c - a * t[pad]
    switch_ang = math.degrees(math.atan2(float(rv @ y), float(rv @ up)))  # 0 = oben, + = nach links (Blender +y)
    Mw = Matrix(((x[0], y[0], up[0], o[0]), (x[1], y[1], up[1], o[1]), (x[2], y[2], up[2], o[2]), (0, 0, 0, 1)))
    M = Mw.inverted()
    _transform_all(ob, arm, M)
    switch_x = float(t[pad] - t0)
    log.append(f'SCHALTER unter der Daumenkuppe bei x = {switch_x * 1000:.1f} mm, {switch_ang:.1f} Grad von oben (Lampenrahmen)')
    # Druck auf den Schalter: Daumen-Grund- und Endgelenk beugen, bis die Kuppe 1,8 mm tiefer liegt
    rig = Rig(arm, ob)
    tsel = np.where(rig.W[:, rig.idx['th_2']] > 0.9)[0]
    ax_c, ax_a = np.zeros(3), np.array([1.0, 0, 0])
    d0 = axis_dist(rig.V[tsel], ax_c, ax_a)[0].min()
    best = None
    for d1 in np.arange(0.0, 14.0, 0.5):
        for d2 in np.arange(0.0, 22.0, 0.5):
            S = G.chain(rig, ['th_1', 'th_2'], [flex(d1), flex(d2)])
            # th_0 bleibt: Kette ab th_1 haengt an th_0 (in Ruhe) - Eltern-Ruhe korrekt mitnehmen
            P = G.skin_sel(rig, S, tsel)
            dd, tt = axis_dist(P, ax_c, ax_a)
            k = int(np.argmin(dd))
            err = (dd[k] - (d0 - 0.0018)) ** 2 * 1e6 + 0.02 * (d1 * d1 + 0.5 * d2 * d2) / 100.0
            if best is None or err < best[0]:
                best = (err, d1, d2, dd[k] - d0)
    _, d1, d2, dz = best
    log.append(f'DRUCK: Grundgelenk +{d1:.1f}, Endgelenk +{d2:.1f} Grad, Kuppe {dz * 1000:.2f} mm zur Achse')
    _make_action(arm, 'grip', {})
    _make_action(arm, 'press', {'th_1': flex(d1), 'th_2': flex(d2)})
    # Kennwerte fuer gameplay/body.cpp (Lampenrahmen; im Spiel ist y gespiegelt)
    bones = arm.data.bones
    wrist = bones['hand'].head_local
    fore = -(bones['forearm'].tail_local - bones['forearm'].head_local).normalized()  # Ellbogen -> Handgelenk
    # wie im Spiel (GameSession::vmGeometryFromSkin): Handgelenk -> Mittelfinger-Grundgelenk; Handflaeche senkrecht zur
    # Ebene Handgelenk - Zeige- - Kleinfinger-Grundgelenk (Blender ist rechtshaendig: Kreuzprodukt zeigt zum Ruecken)
    hdir = (bones['f1_1'].head_local - wrist).normalized()
    palm = -(bones['f0_1'].head_local - wrist).cross(bones['f3_1'].head_local - wrist).normalized()
    g = lambda v: (round(v.x, 4), round(-v.y, 4), round(v.z, 4))
    log.append(f'KVM kVmWrist{g(wrist)} kVmForearm{g(fore)} kVmHandDir{g(hdir)} kVmPalm{g(palm)}')
    build_final.kvm = {'wrist': g(wrist), 'forearm': g(fore), 'hand_dir': g(hdir), 'palm': g(palm)}
    return ob, arm, (switch_x, switch_ang)


def build_vm_hand_l():
    log = []
    ob, arm, (switch_x, switch_ang) = build_final(log)
    for line in log:
        print(line, flush=True)
    import json
    import os
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'vm_grip.json'), 'w') as fh:
        json.dump({'switch_x': round(switch_x, 4), 'switch_deg': round(switch_ang, 1), 'kvm': build_final.kvm, 'log': log},
                  fh, indent=1)
    common.EXPORT_EXTRA = {'export_animations': True, 'export_animation_mode': 'ACTIONS', 'export_skins': True,
                           'export_force_sampling': False, 'export_def_bones': False,
                           'export_optimize_animation_size': False}
