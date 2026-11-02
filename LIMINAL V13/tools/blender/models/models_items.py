"""V13.3: Fundstuecke und Interaktionsobjekte (PROPS_Level0.md, Teil B).

Ersetzen die Netze aus Code (src/render/meshes.cpp) ueber src/render/item_models.cpp: data/models/item_<name>.glb ->
Netz <name> im Spiel. Lage, Groesse und Ursprung wie bisher (das Spiel setzt die Objekte so):
  energy        Riegel 22 x 6 x 2,4 cm, Mitte im Ursprung, liegt auf z = 0
  almond        Flasche 26,2 cm, Boden in der Mitte (Griffhoehe 11 cm, data/items.json)
  battery       Monozelle D stehend, Boden in der Mitte
  note          A5-Zettel liegend, Mitte im Ursprung (u quer, v laengs wie die Textur)
  fuse          Sicherung liegend entlang x, Mitte im Ursprung
  fusebox(_empty) Sicherungskasten: Wand bei x = -0.04, Kasten z 1,08 .. 1,70, Tuer um 105 Grad offen
  vending       Automat: Ursprung Kachelmitte am Boden, Front bei x = 0,30 (render/vending_layout.hpp)
  exit_door     Notausgang: Ursprung am Boden in der Wandebene, Leuchtschild ueber der Tuer
  elev_frame    Aufzug: Rahmen, Kabine (x -2 .. 0, y -1 .. 1), Licht; die Anzeigen bleiben aus Code
  elev_door     ein Tuerfluegel 1 m (Mitte y = 0; das Spiel setzt zwei davon)
Etiketten, Automatenfront, Tuerblatt und Leuchtschild benutzen die Atlanten der festen Materialien (fm=...), damit
Schrift und Leuchtflaechen stimmen; alles andere hat eigene Materialien (look, tex, r, m). Blender-y ist Spiel-(-y).
"""
import math
import random

from mathutils import Matrix, Vector

import common
from models import kit_items as K

TAU = 2 * math.pi


def gen(name, rgb, look, r=None, m=None, tex=None, glow=0.0):
    return common.material(name, rgb, look=look, r=r, m=m, tex=tex, glow=glow)


def const_uv(u, v):
    return lambda co, n: (u, v)


def clamp(x, a, b):
    return a if x < a else (b if x > b else x)


def place_new(P, n0, M):
    K.transform_verts(K.new_verts_since(P, n0), M)


def tube_profile(r, n=8):
    return [(r * math.cos(TAU * i / n), r * math.sin(TAU * i / n)) for i in range(n)]


# =====================================================================================================
# Energieriegel: Schlauchbeutel aus Folie mit gezackten Quernaehten (Folie: Atlas FM_ITEM_ENERGY, u laengs, v ringsum)
# =====================================================================================================
def build_item_energy():
    P = K.Part('energieriegel')
    foil = P.mat(K.fm_material('item_energy', (0.80, 0.16, 0.10)), atlas=True)
    L2, XB, HW, HH = 0.11, 0.095, 0.030, 0.012
    NA = 28
    xs = [-XB + 2 * XB * (0.5 - 0.5 * math.cos(math.pi * i / 30)) for i in range(31)]
    rings = []
    for x in xs:
        t = 1 - abs(x) / XB
        k = min(1.0, t / 0.2)
        h = 0.0006 + (HH - 0.0006) * math.sqrt(k)
        w = HW * (1 + 0.12 * (1 - k))
        ring = []
        for j in range(NA):
            a = -math.pi / 2 + TAU * j / NA
            c, s = math.cos(a), math.sin(a)
            pc = math.copysign(abs(c) ** 0.32, c)
            ps = math.copysign(abs(s) ** 0.32, s)
            pleat = 0.0011 * (1 - k) ** 1.5 * math.sin(a * 8 + x * 40)
            wr = 0.00028 * math.sin(x * 93 + j * 1.7) * math.sin(x * 21 + j * 0.6) * k
            ring.append(P.v((x, pc * (w + pleat * 0.6 + wr), HH + ps * (h + pleat * 0.4 + wr))))
        rings.append(ring)
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        ua, ub = (xs[i] + L2) / (2 * L2), (xs[i + 1] + L2) / (2 * L2)
        for j in range(NA):
            j1 = (j + 1) % NA
            v0, v1 = j / NA, (j + 1) / NA
            f = P.face((A[j], A[j1], B[j1], B[j]), foil, uvs=[(ua, v0), (ua, v1), (ub, v1), (ub, v0)])
            if f is not None:
                K.fix_away(P, [f], ((xs[i] + xs[i + 1]) / 2, 0.0, HH))
    # Quernaehte: flach gepresst, laengs geriffelt, Aussenkante gezackt
    WS, NY, NX = 0.0338, 22, 4
    for sg in (-1, 1):
        ys = [-WS + 2 * WS * c / NY for c in range(NY + 1)]

        def xend(y):
            ph = (y + WS) / 0.0032
            tri = 1 - abs(2 * (ph - math.floor(ph)) - 1)
            notch = 0.0045 * max(0.0, 1 - abs(y - 0.012) / 0.0035) if sg > 0 else 0.0  # Runde 4: Einreisskerbe
            return sg * (L2 - 0.0012 * tri - notch)

        def zc(y):
            return 0.00032 * math.sin((y + WS) * TAU / 0.0034)
        top, bot = [], []
        for r in range(NX + 1):
            rowt, rowb = [], []
            for y in ys:
                x = sg * 0.092 + (xend(y) - sg * 0.092) * r / NX
                rowt.append(P.v((x, y, HH + 0.0005 + zc(y))))
                rowb.append(P.v((x, y, HH - 0.0005 + zc(y))))
            top.append(rowt)
            bot.append(rowb)

        def uvt(co):
            return ((co.x + L2) / (2 * L2), 0.5 - co.y * 5.0)

        def uvb(co):
            return ((co.x + L2) / (2 * L2), 0.03 + (co.y + WS) * 0.6)
        for r in range(NX):
            for c in range(NY):
                ft = P.face((top[r][c], top[r][c + 1], top[r + 1][c + 1], top[r + 1][c]), foil, want=(0, 0, 1))
                fb = P.face((bot[r][c], bot[r + 1][c], bot[r + 1][c + 1], bot[r][c + 1]), foil, want=(0, 0, -1))
                for f, fn in ((ft, uvt), (fb, uvb)):
                    if f is not None:
                        for loop in f.loops:
                            K.setuv(loop, P.uv, *fn(loop.vert.co))
        for c in range(NY):  # Aussenkante
            f = P.face((top[NX][c], top[NX][c + 1], bot[NX][c + 1], bot[NX][c]), foil, want=(sg, 0, 0))
            if f is not None:
                for loop in f.loops:
                    K.setuv(loop, P.uv, (loop.vert.co.x + L2) / (2 * L2), 0.05)
        for c, sy in ((0, -1), (NY, 1)):  # Seitenkanten
            for r in range(NX):
                f = P.face((top[r][c], top[r + 1][c], bot[r + 1][c], bot[r][c]), foil, want=(0, sy, 0))
                if f is not None:
                    for loop in f.loops:
                        K.setuv(loop, P.uv, (loop.vert.co.x + L2) / (2 * L2), 0.05)
    P.finish(smooth_deg=50.0)


# =====================================================================================================
# Mandelwasser: trueb-weisse Flasche wie das offene Mandelwasser (models_l1_phenomena.py), Etikett und Deckel aus dem
# Atlas FM_ITEM_ALMOND (v = Hoehe: Etikett 0,30..0,66, Deckel > 0,86)
# =====================================================================================================
def build_item_almond():
    P = K.Part('mandelwasser')
    glass = P.mat(gen('flasche_trueb', (0.74, 0.77, 0.69), 'glass', r=0.5))
    label = P.mat(K.fm_material('item_almond', (0.86, 0.74, 0.52), name='etikett'), atlas=True)
    H = 0.262
    prof = [(0.0, 0.0065), (0.010, 0.0058), (0.020, 0.0035), (0.029, 0.0005), (0.033, 0.0), (0.040, 0.0008), (0.0465, 0.0040),
            (0.0510, 0.0095), (0.0533, 0.0165), (0.0540, 0.0240),
            (0.0540, 0.0300), (0.0524, 0.0325), (0.0540, 0.0350), (0.0540, 0.0420), (0.0524, 0.0445), (0.0540, 0.0470),
            (0.0540, 0.0640), (0.0546, 0.0690), (0.0546, 0.0740), (0.0528, 0.0765), (0.0528, 0.1750), (0.0546, 0.1775),
            (0.0546, 0.1820), (0.0540, 0.1870), (0.0525, 0.1930), (0.0485, 0.1990), (0.0420, 0.2050), (0.0340, 0.2105),
            (0.0270, 0.2150), (0.0215, 0.2190), (0.0200, 0.2215), (0.0200, 0.2235), (0.0222, 0.2242), (0.0224, 0.2260),
            (0.0200, 0.2268), (0.0196, 0.2295), (0.0, 0.2300)]
    n0 = K.count(P)
    K.revolve(P, prof, 32, glass, axis='z')
    # Petaloid-Boden: fuenf Fuesse, dazwischen hochgezogen
    for v in K.new_verts_since(P, n0):
        x, y, z = v.co
        r = math.hypot(x, y)
        if z < 0.0105 and 0.012 < r < 0.052:
            th = math.atan2(y, x)
            valley = 0.5 + 0.5 * math.cos(5 * th)
            bump = max(0.0, math.sin(math.pi * (r - 0.012) / 0.040))
            v.co.z = z + 0.0042 * valley * bump
            k = 1 - 0.035 * valley * bump
            v.co.x, v.co.y = x * k, y * k
    # Etikett (Schrumpfhuelle auf der Etikettenflaeche)
    lab = [(0.0529, 0.0784), (0.0533, 0.0786), (0.0533, 0.1730), (0.0529, 0.1732)]
    # Runde 4: Klebenaht des Etiketts (ueberlappende Kante, 0,3 mm hoeher)
    K.revolve(P, [(0.0533, 0.0790), (0.05365, 0.0792), (0.05365, 0.1724), (0.0533, 0.1726)], 4, label, axis='z', a0=1.2, arc=0.10,
              close=False, uvfn=lambda s, k, t: (0.97, clamp(0.30 + 0.36 * (t - 0.0786) / 0.0944, 0.30, 0.66)))
    K.revolve(P, lab, 32, label, axis='z',
              uvfn=lambda s, k, t: (s, clamp(0.30 + 0.36 * (t - 0.0786) / 0.0944, 0.30, 0.66)))
    # Deckel mit Garantieband (Atlas: blau, gerippt)
    cap = [(0.0198, 0.2290), (0.0232, 0.2292), (0.0232, 0.2335), (0.0222, 0.2340), (0.0232, 0.2346), (0.0232, 0.2560),
           (0.0226, 0.2602), (0.0212, 0.2618), (0.0196, 0.2620), (0.0, 0.2616)]
    K.revolve(P, cap, 40, label, axis='z', uvfn=lambda s, k, t: (s, 0.87 + 0.12 * (t - 0.229) / 0.033))
    P.finish(smooth_deg=40.0)


# =====================================================================================================
# Batterie (Monozelle D): Mantel mit umgelegten Kanten, Pluspol mit Isolierring, Minuspol mit Pressring
# Atlas FM_ITEM_BATTERY: v 0..0,08 Metall, 0,08..0,36 Kupferband, darunter schwarz mit Schrift
# =====================================================================================================
def build_item_battery():
    P = K.Part('batterie')
    m = P.mat(K.fm_material('item_battery', (0.1, 0.1, 0.1)), atlas=True)
    MET, BLK = 0.03, 0.40

    def side_v(z):
        return clamp(0.085 + (0.0600 - z) / 0.0600 * 0.88, 0.085, 0.965)
    prof = [((0.0, 0.0004), MET), ((0.0040, 0.0004), MET), ((0.0060, 0.0009), MET), ((0.0080, 0.0009), MET),
            ((0.0095, 0.0004), MET), ((0.0128, 0.0004), MET), ((0.0136, 0.0002), 0.985), ((0.0156, 0.0), 0.98),
            ((0.0164, 0.0010), 0.975), ((0.0166, 0.0030), 0.97)]
    for z in (0.008, 0.016, 0.024, 0.032, 0.040, 0.048, 0.056):
        prof.append(((0.0166, z), side_v(z)))
    prof += [((0.0166, 0.0592), side_v(0.0592)), ((0.0164, 0.0603), 0.09), ((0.0156, 0.0613), 0.095), ((0.0140, 0.0618), 0.10),
             ((0.0136, 0.0614), MET), ((0.0070, 0.0614), MET), ((0.0066, 0.0616), BLK), ((0.0052, 0.0617), BLK),
             ((0.0050, 0.0620), MET), ((0.0049, 0.0638), MET), ((0.0045, 0.0645), MET), ((0.0, 0.0645), MET)]
    pts = [p for p, _ in prof]
    vs = [v for _, v in prof]

    def rmod(th, t, r):  # kleine Delle im Mantel
        if 0.01 < t < 0.05 and r > 0.0165:
            d = (th - 1.0 + math.pi) % TAU - math.pi
            return r - 0.0005 * math.exp(-(d * d / 0.05 + (t - 0.03) ** 2 / 0.00006))
        return r
    K.revolve(P, pts, 36, m, axis='z', rmod=rmod, uvfn=lambda s, k, t: (s if vs[k] > 0.05 else 0.3 + 0.4 * s, vs[k]))
    P.finish(smooth_deg=35.0)


# =====================================================================================================
# Notizzettel (A5, einmal gefaltet, wieder aufgeklappt, Eselsohr)
# Atlas FM_ITEM_NOTE: u quer (15 cm), v laengs (21 cm); Rueckseite aus dem leeren Rand oben links
# =====================================================================================================
def build_item_note():
    P = K.Part('notiz')
    pap = P.mat(K.fm_material('item_note', (0.86, 0.82, 0.68)), atlas=True)
    W, L, NU, NV, K0 = 0.15, 0.21, 15, 20, 3
    rnd = random.Random(17)
    ph = [rnd.uniform(0, TAU) for _ in range(4)]

    def pos(i, j):
        u, v = i / NU, j / NV
        x, y = (v - 0.5) * L, (0.5 - u) * W
        z = 0.0006 + 0.009 * abs(2 * v - 1) ** 1.4 + 0.0024 * abs(2 * u - 1) ** 2.2
        z += 0.00045 * math.sin(x * 61 + ph[0]) * math.sin(y * 47 + ph[1]) + 0.0003 * math.sin(x * 23 + y * 31 + ph[2])
        z += 0.0016 * max(0.0, 1 - abs(u - 0.5) * NU / 1.0)  # Runde 4: zweiter, flacher Knick (Zettel war geviertelt)
        return Vector((x, y, z))
    corner = lambda i, j: (i - (NU - K0)) + (j - (NV - K0))  # >= K0: jenseits der Faltlinie des Eselsohrs
    V = {}
    for j in range(NV + 1):
        for i in range(NU + 1):
            if corner(i, j) <= K0:
                V[i, j] = P.v(pos(i, j))
    for j in range(NV):
        for i in range(NU):
            c = corner(i, j)
            if c <= K0 - 2 or i < NU - K0 or j < NV - K0:
                quad = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                P.face([V[q] for q in quad], pap, uvs=[(q[0] / NU, q[1] / NV) for q in quad], want=(0, 0, 1))
            elif c == K0 - 1:
                tri = [(i, j), (i + 1, j), (i, j + 1)]
                P.face([V[q] for q in tri], pap, uvs=[(q[0] / NU, q[1] / NV) for q in tri], want=(0, 0, 1))
    # umgeklappte Ecke: an der Faltlinie gespiegelt, liegt knapp darueber und zeigt die Rueckseite
    a, b = pos(NU, NV - K0), pos(NU - K0, NV)
    d = (b - a).normalized()
    nrm2 = Vector((-d.y, d.x, 0.0))

    def refl(p):
        q = p - a
        q2 = Vector((q.x, q.y, 0.0))
        r2 = q2 - 2 * q2.dot(nrm2) * nrm2
        return Vector((a.x + r2.x, a.y + r2.y, a.z + 0.0004 + 0.0015 * q2.length / 0.03))
    flap = {}
    for j in range(NV - K0, NV + 1):
        for i in range(NU - K0, NU + 1):
            if corner(i, j) >= K0:
                flap[i, j] = P.v(refl(pos(i, j)))

    def buv(q):
        return (0.01 + 0.06 * (NU - q[0]) / K0, 0.01 + 0.07 * (NV - q[1]) / K0)
    for j in range(NV - K0, NV):
        for i in range(NU - K0, NU):
            c = corner(i, j)
            if c >= K0:
                quad = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                P.face([flap[q] for q in quad], pap, uvs=[buv(q) for q in quad], want=(0, 0, 1))
            elif c == K0 - 1:
                tri = [(i + 1, j), (i + 1, j + 1), (i, j + 1)]
                P.face([flap[q] for q in tri], pap, uvs=[buv(q) for q in tri], want=(0, 0, 1))
    P.finish(smooth_deg=60.0)


# =====================================================================================================
# Sicherung (Patrone 17 cm): Porzellankoerper mit Aufdruck, Messingkappen mit Wulst, Kerben und Kontaktzapfen
# Atlas FM_ITEM_FUSE (Ebene L_FUSE): Porzellan u 0,5..0,75 / v 0,5..1, Messing u 0,75..1 / v 0,5..1
# =====================================================================================================
def fuse_cartridge(P, M, mat):
    n0 = K.count(P)
    body = [(0.0158, 0.0262), (0.0165, 0.0276), (0.0165, 0.1424), (0.0158, 0.1438)]
    K.revolve(P, body, 24, mat, axis='x', uvfn=lambda s, k, t: (0.5 + 0.25 * s, 0.5 + 0.5 * clamp((t - 0.026) / 0.118, 0, 1)))
    capp = [(0.0, -0.0030), (0.0045, -0.0030), (0.0050, -0.0006), (0.0052, 0.0), (0.0160, 0.0), (0.0180, 0.0012), (0.0185, 0.0032),
            (0.0185, 0.0205), (0.0192, 0.0222), (0.0192, 0.0244), (0.0182, 0.0261), (0.0160, 0.0268)]

    def dimple(th, t, r):
        if 0.006 < abs(t) < 0.017 and r >= 0.0185:
            return r - 0.0008 * max(0.0, math.cos(3 * th)) ** 10
        return r
    for sgn in (1, -1):
        prof = [(r, t if sgn > 0 else 0.170 - t) for r, t in capp]
        if sgn < 0:
            prof = list(prof)
        K.revolve(P, prof, 24, mat, axis='x', rmod=lambda th, t, r: dimple(th, min(t, 0.170 - t), r),
                  uvfn=lambda s, k, t: (0.76 + 0.23 * s, 0.5 + 0.5 * clamp(t / 0.17, 0, 1)))
    place_new(P, n0, M)


def build_item_fuse():
    P = K.Part('sicherung')
    mat = P.mat(K.fm_material('item_fuse', (0.9, 0.88, 0.82)), atlas=True)
    fuse_cartridge(P, Matrix.Translation((-0.085, 0.0, 0.0193)), mat)
    P.finish(smooth_deg=40.0)


# =====================================================================================================
# Sicherungskasten (Wand bei x = -0,04; Kasten 0,46 x 0,62 x 0,16 m, Tuer um 105 Grad offen)
# =====================================================================================================
FB_Z0, FB_Z1, FB_HW = 1.08, 1.70, 0.23
FB_ZC = (FB_Z0 + FB_Z1) / 2


def _fusebox(with_fuse):
    P = K.Part('sicherungskasten')

    def paint_uv(co, n):
        X, Y, Z = co.x + 0.1, co.y + 0.5, co.z - 1.0
        a = (abs(n.x), abs(n.y), abs(n.z))
        if a[0] >= a[1] and a[0] >= a[2]:
            p, q = Y, Z
        elif a[1] >= a[2]:
            p, q = X, Z
        else:
            p, q = X, Y
        return (0.004 + 0.49 * clamp(p * 0.5, 0, 1), 0.004 + 0.49 * clamp(q * 0.5, 0, 1))
    paint = P.mat(K.fm_material('fusebox', (0.62, 0.62, 0.58), name='gehaeuse'), uvmap=paint_uv)
    atl = P.mat(K.fm_material('fusebox', (0.62, 0.62, 0.58), name='gehaeuse_atlas'), atlas=True)
    fusem = P.mat(K.fm_material('item_fuse', (0.9, 0.88, 0.82)), atlas=True)
    if with_fuse:
        glowm = P.mat(K.fm_material('fuse_glow', (1.0, 0.4, 0.07), name='lampe'), uvmap=const_uv(0.54, 0.54))
    else:
        glowm = P.mat(gen('lampe_aus', (0.36, 0.15, 0.05), 'plastic', r=0.5))
    rubber = P.mat(gen('dichtung', (0.03, 0.03, 0.03), 'rubber', r=1.1))
    steel = P.mat(gen('scharnier', (0.58, 0.58, 0.56), 'metal', r=0.75))
    zinc = P.mat(gen('verzinkt', (0.68, 0.68, 0.66), 'galvanized', r=0.9))
    slot = P.mat(gen('schlitz', (0.04, 0.04, 0.04), 'paint', r=1.3))
    pvc = P.mat(gen('rohr_pvc', (0.52, 0.53, 0.52), 'plastic', r=1.1))
    gland = P.mat(gen('verschraubung', (0.14, 0.14, 0.15), 'plastic', r=0.9))
    mcb = P.mat(gen('ls_schalter', (0.86, 0.86, 0.83), 'plastic', r=0.9))
    dark = P.mat(gen('hebel', (0.07, 0.07, 0.07), 'plastic', r=0.7))
    wires = [P.mat(gen(n, c, 'plastic', r=0.75)) for n, c in (('ader_braun', (0.33, 0.17, 0.08)), ('ader_blau', (0.10, 0.22, 0.55)),
                                                              ('ader_gruengelb', (0.55, 0.62, 0.12)))]

    def atlas_faces(n0f, fn):
        for f in K.new_faces_since(P, n0f):
            P.uvface[f] = fn

    # --- Gehaeuse: Wanne mit eingekanteter Frontlippe (eine geschlossene Blechschale) ---
    prof = [(0.0, -0.035), (0.0, 0.1220), (0.0010, 0.1252), (0.0030, 0.1265), (0.0145, 0.1265), (0.0152, 0.1258), (0.0152, 0.1240),
            (0.0140, 0.1233), (0.0013, 0.1233), (0.0013, -0.0335)]
    K.ring_sweep(P, lambda ins: [(a, b + FB_ZC) for a, b in K.rrect(FB_HW - ins, (FB_Z1 - FB_Z0) / 2 - ins, 0.004, 2)], prof,
                 K.frame_wall, paint, cap0=True, cap1=True)
    for sy in (-1, 1):  # Befestigungsschrauben in der Rueckwand
        for sz in (-1, 1):
            K.screw(P, (-0.0335, sy * 0.205, FB_ZC + sz * 0.285), (1, 0, 0), 0.0045, zinc, slot, seg=8, rot=0.5 * sy)
    # --- Montageplatte (verzinkt, beschriftet) auf Abstandshaltern ---
    f0 = K.fcount(P)
    K.box(P, (-0.0262, -0.205, 1.115), (-0.0240, 0.205, 1.665), atl)
    for f in K.new_faces_since(P, f0):
        f.normal_update()
        if f.normal.x > 0.5:
            P.uvface[f] = lambda co, n: (clamp((co.y + 0.205) / 0.41, 0, 1) * 0.46, 0.5 + clamp((1.665 - co.z) / 0.55, 0, 1) * 0.46)
        else:
            P.uvface[f] = const_uv(0.2, 0.62)
    # Hutschiene rechts mit zwei Leitungsschutzschaltern
    rail = [(-0.0240, -0.0125), (-0.0240, 0.0125), (-0.0170, 0.0125), (-0.0170, 0.0175), (-0.0160, 0.0175), (-0.0160, 0.0115),
            (-0.0230, 0.0115), (-0.0230, -0.0115), (-0.0160, -0.0115), (-0.0160, -0.0175), (-0.0170, -0.0175), (-0.0170, -0.0125)]
    zr = 1.544
    r0 = [P.v((x, 0.060, zr + z)) for x, z in rail]
    r1 = [P.v((x, 0.198, zr + z)) for x, z in rail]
    for i in range(len(rail)):
        j = (i + 1) % len(rail)
        P.face((r0[i], r0[j], r1[j], r1[i]), zinc)
    P.face(list(reversed(r0)), zinc)
    P.face(list(r1), zinc)
    for k, yc in enumerate((0.128, 0.147)):
        K.bevel_box(P, (-0.0160, yc - 0.0089, zr - 0.0425), (0.030, yc + 0.0089, zr + 0.0425), mcb, r=0.0015, seg=1)
        K.bevel_box(P, (0.030, yc - 0.0089, zr - 0.022), (0.044, yc + 0.0089, zr + 0.022), mcb, r=0.0015, seg=1)
        K.bevel_box(P, (0.044, yc - 0.0042, zr + 0.002 - 0.012 * k), (0.054, yc + 0.0042, zr + 0.016 - 0.012 * k), dark, r=0.001, seg=1)
    # --- Sicherungshalter: Bakelitsockel, zwei Federklemmen aus Messing ---
    f0 = K.fcount(P)
    K.bevel_box(P, (-0.0240, -0.045, 1.25), (0.004, 0.045, 1.53), atl, r=0.004, seg=2)
    atlas_faces(f0, const_uv(0.475, 0.975))
    zf0 = 1.305
    for zc in (zf0 + 0.0135, zf0 + 0.17 - 0.0135):
        f0 = K.fcount(P)
        ring = [(0.0192, -0.0065), (0.0204, -0.0065), (0.0204, 0.0065), (0.0192, 0.0065), (0.0192, -0.0065)]
        K.revolve(P, ring, 10, atl, axis='z', origin=(0.030, 0.0, zc), a0=math.radians(115), arc=math.radians(130), close=False)
        K.bevel_box(P, (0.004, -0.013, zc - 0.0065), (0.0128, 0.013, zc + 0.0065), atl, r=0.001, seg=1)
        K.screw(P, (0.0128, 0.0, zc), (1, 0, 0), 0.0028, zinc, slot, seg=6)
        atlas_faces(f0, lambda co, n: (0.80 + 0.14 * clamp((co.z - 1.3) * 3, 0, 1), 0.62 + 0.3 * clamp(co.y * 10 + 0.5, 0, 1)))
    if with_fuse:
        fuse_cartridge(P, Matrix.Translation((0.030, 0.0, zf0)) @ Matrix.Rotation(-math.pi / 2, 4, 'Y'), fusem)
    # --- Kontrolllampe: Bakelitfassung, Chromring, Kalotte ---
    f0 = K.fcount(P)
    K.bevel_box(P, (-0.0240, 0.120, 1.400), (0.0, 0.190, 1.470), atl, r=0.004, seg=2)
    atlas_faces(f0, const_uv(0.475, 0.975))
    K.revolve(P, [(0.0215, 0.0), (0.0215, 0.003), (0.0185, 0.0042), (0.0175, 0.0035)], 20, steel, axis='x', origin=(0.0, 0.155, 1.435))
    K.revolve(P, [(0.0176, 0.0030), (0.0172, 0.0068), (0.0140, 0.0125), (0.0080, 0.0165), (0.0, 0.0178)], 20, glowm, axis='x',
              origin=(0.0, 0.155, 1.435))
    # --- Adern vom Halter zum Rohr und zur Verschraubung unten ---
    paths = [[(0.012, 0.010, 1.468), (0.006, 0.024, 1.56), (-0.010, 0.095, 1.625), (-0.016, 0.140, 1.668), (-0.018, 0.148, 1.69)],
             [(0.012, -0.010, 1.312), (0.004, -0.030, 1.28), (-0.012, -0.085, 1.21), (-0.015, -0.100, 1.13), (-0.015, -0.100, 1.083)],
             [(-0.019, 0.200, 1.13), (-0.019, 0.206, 1.58), (-0.020, 0.172, 1.655), (-0.021, 0.155, 1.69)]]
    for p, mi in zip(paths, wires):
        K.sweep_open(P, p, tube_profile(0.0027, 8), mi, up=(1.0, 0.0, 0.0))
    # --- Verschraubungen unten, Rohr nach oben mit Muffen und Schellen ---
    for yc in (-0.100, 0.100):
        K.revolve(P, [(0.0, 1.080), (0.0130, 1.080), (0.0130, 1.071), (0.0105, 1.069), (0.0100, 1.058), (0.0070, 1.054), (0.0, 1.0535)],
                  6 if yc > 0 else 6, gland, axis='z', origin=(0.040, yc, 0.0))
    xc, yc = -0.0275, 0.150
    K.revolve(P, [(0.0, 1.700), (0.0150, 1.700), (0.0150, 1.712), (0.0135, 1.716), (0.0135, 1.736), (0.0105, 1.738)], 6, gland, axis='z',
              origin=(xc, yc, 0.0))
    K.revolve(P, [(0.0105, 1.738), (0.0105, 4.10)], 12, pvc, axis='z', origin=(xc, yc, 0.0))
    for zc in (2.62, 3.58):
        K.revolve(P, [(0.0105, zc - 0.028), (0.0125, zc - 0.026), (0.0128, zc - 0.02), (0.0128, zc + 0.02), (0.0125, zc + 0.026),
                      (0.0105, zc + 0.028)], 12, pvc, axis='z', origin=(xc, yc, 0.0))
    for zc in (2.15, 3.05, 3.95):
        ring = [(0.0111, -0.008), (0.0119, -0.008), (0.0119, 0.008), (0.0111, 0.008), (0.0111, -0.008)]
        K.revolve(P, ring, 10, zinc, axis='z', origin=(xc, yc, zc), a0=-math.pi / 2, arc=math.pi, close=False)
        for s in (-1, 1):
            y0, y1 = sorted((yc + s * 0.0115, yc + s * 0.0215))
            K.box(P, (-0.0400, y0, zc - 0.008), (-0.0388, y1, zc + 0.008), zinc)
            K.screw(P, (-0.0388, yc + s * 0.0170, zc), (1, 0, 0), 0.0028, zinc, slot, seg=6)
    # --- Tuer (geschlossen gebaut, dann um das Scharnier aufgeschwenkt) ---
    nd = K.count(P)
    door = [(0.0, 0.1170), (0.0, 0.1275), (0.0012, 0.1290), (0.0040, 0.1293)]
    K.ring_sweep(P, lambda ins: [(a, b + FB_ZC) for a, b in K.rrect(0.229 - ins, 0.309 - ins, 0.004, 2)], door, K.frame_wall,
                 paint, cap0=True, cap1=True)
    # Runde 5: Schaltplan in der Klarsichttasche innen an der Tuer (Papier aus dem Notizatlas, Tasche als Glanzfolie)
    note = P.mat(K.fm_material('item_note', (0.86, 0.82, 0.68), name='schaltplan'), atlas=True)
    film = P.mat(gen('klarsichtfolie', (0.55, 0.56, 0.55), 'glass', r=0.35))
    pq = ((0.1155, 0.14, 1.50), (0.1155, -0.10, 1.50), (0.1155, -0.10, 1.18), (0.1155, 0.14, 1.18))
    P.face([P.v(p) for p in pq], note, want=(-1, 0, 0), uvs=[(0.08, 0.05), (0.92, 0.05), (0.92, 0.95), (0.08, 0.95)])
    fq = ((0.1150, 0.15, 1.420), (0.1150, -0.11, 1.420), (0.1150, -0.11, 1.413), (0.1150, 0.15, 1.413))  # Taschenkante
    P.face([P.v(p) for p in fq], film, want=(-1, 0, 0))
    gas = [(0.0, 0.1170), (0.0, 0.1118), (0.0065, 0.1118), (0.0065, 0.1170)]
    K.ring_sweep(P, lambda ins: [(a, b + FB_ZC) for a, b in K.rrect(0.214 - ins, 0.294 - ins, 0.004, 1)], gas, K.frame_wall, rubber,
                 closed=True)
    sq = ((-0.10, 1.31), (0.10, 1.31), (0.10, 1.51), (-0.10, 1.51))
    vs = [P.v((0.1296, y, z)) for y, z in sq]  # Warnschild (UV vor dem Aufschwenken)
    P.face(vs, atl, want=(1, 0, 0), uvs=[(0.5 + 0.5 * (y + 0.10) / 0.20, 0.5 * (1.51 - z) / 0.20) for y, z in sq])
    K.revolve(P, [(0.0, 0.1293), (0.0115, 0.1293), (0.0115, 0.1345), (0.0100, 0.1370), (0.0, 0.1372)], 16, steel, axis='x',
              origin=(0.0, 0.192, FB_ZC))
    tri = [P.v((0.13725, 0.192 + 0.0052 * math.cos(a), FB_ZC + 0.0052 * math.sin(a))) for a in (math.pi / 2, math.pi / 2 + TAU / 3,
                                                                                              math.pi / 2 + 2 * TAU / 3)]
    P.face(tri, slot, want=(1, 0, 0))
    K.box(P, (0.1050, 0.180, FB_ZC - 0.006), (0.1170, 0.215, FB_ZC + 0.006), zinc)
    for zc in (1.16, 1.62):  # Scharnierlappen auf der Tuer
        K.box(P, (0.1293, -0.229, zc - 0.03), (0.1305, -0.212, zc + 0.03), steel)
    T = Matrix.Translation((0.125, -0.23, 0.0)) @ Matrix.Rotation(math.radians(-105), 4, 'Z') @ Matrix.Translation((-0.125, 0.23, 0.0))
    K.transform_verts(K.new_verts_since(P, nd), T)
    for zc in (1.16, 1.62):  # Scharnierrollen (bleiben an der Achse)
        K.revolve(P, [(0.0, zc - 0.032), (0.0058, zc - 0.032), (0.0058, zc + 0.032), (0.0, zc + 0.032)], 10, steel, axis='z',
                  origin=(0.1262, -0.2318, 0.0))
        K.box(P, (0.10, -0.2312, zc - 0.03), (0.124, -0.2300, zc + 0.03), steel)
    # --- Warnschild an der Wand darueber ---
    f0 = K.fcount(P)
    K.box(P, (-0.0400, -0.10, 1.75), (-0.0378, 0.10, 1.95), atl)
    for f in K.new_faces_since(P, f0):
        f.normal_update()
        P.uvface[f] = (lambda co, n: (0.5 + 0.5 * clamp((co.y + 0.10) / 0.20, 0, 1), 0.5 * clamp((1.95 - co.z) / 0.20, 0, 1))) \
            if f.normal.x > 0.5 else const_uv(0.52, 0.02)
    for sy in (-1, 1):
        for sz in (-1, 1):
            K.screw(P, (-0.0378, sy * 0.088, 1.85 + sz * 0.088), (1, 0, 0), 0.0035, zinc, slot, seg=6, cross=False)
    P.finish(smooth_deg=35.0)


def build_item_fusebox():
    _fusebox(True)


def build_item_fusebox_empty():
    _fusebox(False)


# =====================================================================================================
# Getraenkeautomat (render/vending_layout.hpp): Tuer mit Fenster, Bedienfeld mit echten Tasten, Muenzschlitz,
# Rueckgabeknopf, Geldscheinpruefer, Rueckgabeschale, Ausgabeklappe, Leuchtschild, Sockel mit Stellfuessen
# Atlas FM_VEND_BODY: Front u 0..0,62 (X von links), Seite u 0,62..1, v = Z / Hoehe (Z von oben)
# =====================================================================================================
V_BACK, V_FRONT, V_HW, V_H, V_BASE = -0.48, 0.30, 0.46, 1.86, 0.10


def vend_uv(co, n):
    X = clamp(co.y + V_HW, 0.0, 2 * V_HW)
    Z = clamp(V_H - co.z, 0.0, V_H)
    D = clamp(V_FRONT - co.x, 0.0, V_FRONT - V_BACK)
    side = abs(n.y) > 0.7 and abs(co.y) > V_HW - 0.012
    top = n.z > 0.7 and co.z > V_H - 0.006
    if co.x > 0.245 and not side and not top:
        return (X / (2 * V_HW) * 0.62, Z / V_H)
    if top:
        return (0.62 + D / (V_FRONT - V_BACK) * 0.38, 0.005)
    if n.z < -0.7:
        return (0.62 + D / (V_FRONT - V_BACK) * 0.38, 0.999)
    return (0.62 + D / (V_FRONT - V_BACK) * 0.38, Z / V_H)


def build_item_vending():
    P = K.Part('automat')
    body = P.mat(K.fm_material('vend_body', (0.6, 0.1, 0.1)), uvmap=vend_uv)
    glowm = P.mat(K.fm_material('vend_glow', (1.0, 0.97, 0.92)), uvmap=vend_uv)
    glass = P.mat(K.fm_material('vend_glass', (0.1, 0.1, 0.1)),
                  uvmap=lambda co, n: (clamp((co.y + V_HW - 0.05) / 0.60, 0, 1), clamp((V_H - co.z - 0.24) / 0.98, 0, 1)))
    seam = P.mat(gen('fuge', (0.02, 0.02, 0.02), 'paint', r=1.3))
    foot = P.mat(gen('stellfuss', (0.55, 0.55, 0.53), 'metal', r=0.8))
    pad = P.mat(gen('gummifuss', (0.03, 0.03, 0.03), 'rubber', r=1.2))

    def B(x, X, Z):
        return (x, X - V_HW, V_H - Z)

    # Korpus (hinter der Tuer)
    K.bevel_box(P, (V_BACK, -V_HW, V_BASE), (V_FRONT - 0.050, V_HW, V_H), body, r=0.010, seg=2)
    # Tuer: umlaufende Kante mit gerundeter Frontkante, Frontflaeche mit Fenster und Ausgabeoeffnung
    zc = (V_BASE + 0.003 + V_H - 0.003) / 2
    hz = (V_H - 0.003 - V_BASE - 0.003) / 2
    edge = [(0.0, V_FRONT - 0.049), (0.0, V_FRONT - 0.016), (0.003, V_FRONT - 0.006), (0.008, V_FRONT - 0.0015), (0.018, V_FRONT)]
    K.ring_sweep(P, lambda ins: [(a, b + zc) for a, b in K.rrect(V_HW - 0.002 - ins, hz - ins, 0.018 - ins, 3)], edge, K.frame_wall,
                 body, cap0=True)
    X0, X1 = 0.002 + 0.018, 2 * V_HW - 0.002 - 0.018
    Z0, Z1 = V_H - (zc + hz) + 0.018, V_H - (zc - hz) - 0.018
    xs = [X0, 0.05, 0.10, 0.60, 0.65, X1]
    zs = [Z0, 0.24, 1.22, 1.36, 1.64, Z1]
    G = [[P.v(B(V_FRONT, X, Z)) for X in xs] for Z in zs]
    for j in range(len(zs) - 1):
        for i in range(len(xs) - 1):
            if j == 1 and 1 <= i <= 3:
                continue  # Fenster
            if j == 3 and i == 2:
                continue  # Ausgabeoeffnung
            P.face((G[j][i], G[j][i + 1], G[j + 1][i + 1], G[j + 1][i]), body, want=(1, 0, 0))
    # Fenster: Chromrahmen, Laibung, Glas (Interior Mapping)
    wx0, wz0, wx1, wz1 = 0.05, 0.24, 0.65, 1.22
    wcy, wcz = (wx0 + wx1) / 2 - V_HW, V_H - (wz0 + wz1) / 2
    chrome = [(0.0, V_FRONT + 0.0002), (0.002, V_FRONT + 0.0035), (0.006, V_FRONT + 0.0046), (0.012, V_FRONT + 0.0042),
              (0.016, V_FRONT + 0.0006)]
    K.ring_sweep(P, lambda ins: [(a + wcy, b + wcz) for a, b in K.rrect((wx1 - wx0) / 2 + 0.016 - ins, (wz1 - wz0) / 2 + 0.016 - ins,
                                                                        0.003, 1)], chrome, K.frame_wall, body)

    def recess(xa, za, xb, zb, x_front, depth, mat, back=True):
        q = [B(x_front, xa, za), B(x_front, xb, za), B(x_front, xb, zb), B(x_front, xa, zb)]
        r = [(p[0] - depth, p[1], p[2]) for p in q]
        f = [P.v(p) for p in q]
        g = [P.v(p) for p in r]
        cy, cz = B(0, (xa + xb) / 2, (za + zb) / 2)[1:]
        for i in range(4):
            k = (i + 1) % 4
            my, mz = (q[i][1] + q[k][1]) / 2, (q[i][2] + q[k][2]) / 2
            P.face((f[i], f[k], g[k], g[i]), mat, want=(0.0, cy - my, cz - mz))
        if back:
            P.face(list(g), mat, want=(1, 0, 0))
        return g
    recess(wx0, wz0, wx1, wz1, V_FRONT + 0.0006, 0.0126, body, back=False)
    vs = [P.v(B(V_FRONT - 0.012, X, Z)) for X, Z in ((wx0, wz0), (wx1, wz0), (wx1, wz1), (wx0, wz1))]
    P.face(vs, glass, want=(1, 0, 0))
    # Ausgabeoeffnung mit schraeger Klappe (unten 2 cm eingedrueckt)
    recess(0.10, 1.36, 0.60, 1.64, V_FRONT, 0.050, body)
    n0 = K.count(P)
    K.box(P, (-0.0015, 0.105 - V_HW, -0.265), (0.0015, 0.595 - V_HW, 0.0), body)
    T = Matrix.Translation((V_FRONT - 0.006, 0.0, V_H - 1.365)) @ Matrix.Rotation(math.radians(5), 4, 'Y')
    place_new(P, n0, T)
    # Leuchtschild: Kasten mit Chromrand, Streuscheibe leuchtet
    hx0, hz0, hx1, hz1 = 0.04, 0.04, 0.88, 0.20
    hcy, hcz = (hx0 + hx1) / 2 - V_HW, V_H - (hz0 + hz1) / 2
    hp = [(0.0, V_FRONT), (0.0, V_FRONT + 0.0355), (0.0015, V_FRONT + 0.0392), (0.004, V_FRONT + 0.0400), (0.0085, V_FRONT + 0.0400),
          (0.0092, V_FRONT + 0.0386)]
    rings = K.ring_sweep(P, lambda ins: [(a + hcy, b + hcz) for a, b in K.rrect((hx1 - hx0) / 2 - ins, (hz1 - hz0) / 2 - ins, 0.008 - ins * 0.5, 2)],
                         hp, K.frame_wall, body)
    P.face(list(rings[-1]), glowm, want=(1, 0, 0))
    # Bedienfeld: steht 25 mm vor, mit Anzeige, Tasten, Muenzschlitz, Rueckgabeknopf, Geldscheinpruefer, Schale
    px0, pz0, px1, pz1 = 0.68, 0.24, 0.88, 1.22
    pcy, pcz = (px0 + px1) / 2 - V_HW, V_H - (pz0 + pz1) / 2
    PF = V_FRONT + 0.025
    pp = [(0.0, V_FRONT), (0.0, PF - 0.0035), (0.0015, PF - 0.0005), (0.004, PF)]
    K.ring_sweep(P, lambda ins: [(a + pcy, b + pcz) for a, b in K.rrect((px1 - px0) / 2 - ins, (pz1 - pz0) / 2 - ins, 0.006 - ins, 2)],
                 pp, K.frame_wall, body)
    qx = [px0 + 0.004, 0.699, 0.74, 0.82, 0.861, px1 - 0.004]
    qz = [pz0 + 0.004, 0.304, 0.396, 0.95, 1.01, pz1 - 0.004]
    Q = [[P.v(B(PF, X, Z)) for X in qx] for Z in qz]
    for j in range(5):
        for i in range(5):
            if j == 1 and 1 <= i <= 3:
                continue
            if j == 3 and i == 2:
                continue
            P.face((Q[j][i], Q[j][i + 1], Q[j + 1][i + 1], Q[j + 1][i]), body, want=(1, 0, 0))
    recess(0.699, 0.304, 0.861, 0.396, PF, 0.0035, body)
    vs = [P.v(B(PF - 0.003, X, Z)) for X, Z in ((0.705, 0.31), (0.855, 0.31), (0.855, 0.39), (0.705, 0.39))]
    P.face(vs, glowm, want=(1, 0, 0))
    recess(0.74, 0.95, 0.82, 1.01, PF, 0.022, body)
    for kc in range(3):
        for kr in range(4):
            Xa, Za = 0.70 + kc * 0.05 + 0.006, 0.43 + kr * 0.05 + 0.006
            p0, p1 = B(PF, Xa, Za + 0.038), B(PF + 0.0038, Xa + 0.038, Za)
            K.bevel_box(P, p0, p1, body, r=0.0025, seg=1)
    pcx = (px0 + px1) / 2
    K.bevel_box(P, B(PF, pcx - 0.03, 0.715), B(PF + 0.003, pcx + 0.03, 0.675), body, r=0.002, seg=1)
    vs = [P.v(B(PF + 0.0031, X, Z)) for X, Z in ((pcx - 0.0025, 0.682), (pcx + 0.0025, 0.682), (pcx + 0.0025, 0.708), (pcx - 0.0025, 0.708))]
    P.face(vs, seam, want=(1, 0, 0))
    by, bz = B(0, pcx + 0.055, 0.695)[1:]
    K.revolve(P, [(0.0140, 0.0), (0.0136, 0.0022), (0.0100, 0.0048), (0.0055, 0.0058), (0.0, 0.0060)], 20, body, axis='x',
              origin=(PF, by, bz))
    K.bevel_box(P, B(PF, pcx - 0.06, 0.83), B(PF + 0.008, pcx + 0.06, 0.77), body, r=0.003, seg=1)
    vs = [P.v(B(PF + 0.0081, X, Z)) for X, Z in ((pcx - 0.045, 0.797), (pcx + 0.045, 0.797), (pcx + 0.045, 0.803), (pcx - 0.045, 0.803))]
    P.face(vs, seam, want=(1, 0, 0))
    # Fuge zwischen Korpus und Tuer an Seiten und Oberseite, Sockel, Stellfuesse
    for sy in (-1, 1):
        vs = [P.v((V_FRONT - 0.0505, sy * (V_HW + 0.0004), V_BASE + 0.01)), P.v((V_FRONT - 0.0475, sy * (V_HW + 0.0004), V_BASE + 0.01)),
              P.v((V_FRONT - 0.0475, sy * (V_HW + 0.0004), V_H - 0.012)), P.v((V_FRONT - 0.0505, sy * (V_HW + 0.0004), V_H - 0.012))]
        P.face(vs, seam, want=(0, sy, 0))
    vs = [P.v((V_FRONT - 0.0505, -V_HW + 0.012, V_H + 0.0004)), P.v((V_FRONT - 0.0475, -V_HW + 0.012, V_H + 0.0004)),
          P.v((V_FRONT - 0.0475, V_HW - 0.012, V_H + 0.0004)), P.v((V_FRONT - 0.0505, V_HW - 0.012, V_H + 0.0004))]
    P.face(vs, seam, want=(0, 0, 1))
    K.box(P, (V_BACK + 0.02, -V_HW + 0.02, 0.014), (V_FRONT - 0.04, V_HW - 0.02, V_BASE), body)
    for fx in (V_FRONT - 0.08, V_BACK + 0.07):
        for fy in (-0.38, 0.38):
            K.revolve(P, [(0.0, 0.0), (0.024, 0.0), (0.024, 0.006), (0.010, 0.007)], 12, pad, axis='z', origin=(fx, fy, 0.0))
            K.revolve(P, [(0.010, 0.007), (0.010, 0.010), (0.014, 0.010), (0.014, 0.0145), (0.0, 0.0145)], 6, foot, axis='z',
                      origin=(fx, fy, 0.0))
    P.finish(smooth_deg=35.0)


# =====================================================================================================
# Notausgang: Stahlzarge (Profil mit Gehrung), Tuerblatt (Atlas: Lack, Aufkleber, Abrieb), Panikstange mit
# Gehaeusen, Baender, Obentuerschliesser, Trittblech, Schwelle, Rettungszeichenleuchte
# Atlas FM_EXIT_*: Tuerblatt u 0..0,5, Schild u 0,5..1 / v 0..0,25, gebuersteter Stahl im Rest
# =====================================================================================================
E_HW, E_H = 0.47, 2.06


def steel_uv(co, n):
    a = (abs(n.x), abs(n.y), abs(n.z))
    if a[0] >= a[1] and a[0] >= a[2]:
        p, q = co.y, co.z
    elif a[1] >= a[2]:
        p, q = co.x, co.z
    else:
        p, q = co.x, co.y + 0.6
    return (0.56 + 0.37 * clamp((p + 0.7) / 1.4, 0, 1), 0.31 + 0.62 * clamp((q + 0.1) / 2.7, 0, 1))


def build_item_exit_door():
    P = K.Part('notausgang')
    frame = P.mat(K.fm_material('exit_frame', (0.5, 0.5, 0.52)), uvmap=steel_uv)
    leaf = P.mat(K.fm_material('exit_door', (0.42, 0.48, 0.44)),
                 uvmap=lambda co, n: (clamp((E_HW + co.y) / (2 * E_HW), 0, 0.999) * 0.5, clamp((E_H - co.z) / (E_H - 0.008), 0, 1)))
    sign = P.mat(K.fm_material('exit_sign', (0.04, 0.62, 0.28)),
                 uvmap=lambda co, n: (0.5 + 0.5 * clamp((co.y + 0.28) / 0.56, 0, 1), 0.25 * clamp((2.38 - co.z) / 0.18, 0, 1)))
    housing = P.mat(gen('leuchtengehaeuse', (0.86, 0.86, 0.83), 'plastic', r=0.9))
    rubber = P.mat(gen('dichtung', (0.03, 0.03, 0.03), 'rubber', r=1.1))
    hinge = P.mat(gen('band', (0.60, 0.60, 0.58), 'metal', r=0.65))
    closer = P.mat(gen('tuerschliesser', (0.70, 0.70, 0.68), 'paint', r=0.6))
    alu = P.mat(gen('schwelle', (0.74, 0.74, 0.73), 'metal', r=0.95))
    caps = P.mat(gen('panik_kappe', (0.10, 0.10, 0.10), 'plastic', r=0.8))
    led = P.mat(gen('led', (0.25, 1.0, 0.35), 'glow', glow=0.35))
    screw = P.mat(gen('schraube', (0.66, 0.66, 0.64), 'metal', r=0.6))
    slot = P.mat(gen('schlitz', (0.04, 0.04, 0.04), 'paint', r=1.3))
    # Zarge: Profil laeuft links hoch, oben quer, rechts runter
    g = 0.004
    path = [(0.0, -(E_HW + g), 0.0), (0.0, -(E_HW + g), E_H + g), (0.0, E_HW + g, E_H + g), (0.0, E_HW + g, 0.0)]
    zprof = [(0.0, 0.0), (0.0, 0.043), (0.0015, 0.0465), (0.004, 0.0475), (0.054, 0.0475), (0.0585, 0.045), (0.060, 0.040), (0.060, 0.0)]
    K.sweep_open(P, path, zprof, frame, up=(1.0, 0.0, 0.0))
    # Dichtung in der Zarge (sichtbar im Spalt)
    K.sweep_open(P, [(0.0, -(E_HW + 0.0015), 0.01), (0.0, -(E_HW + 0.0015), E_H + 0.0015), (0.0, E_HW + 0.0015, E_H + 0.0015),
                     (0.0, E_HW + 0.0015, 0.01)], [(0.0, 0.004), (0.0, 0.012), (0.0025, 0.012), (0.0025, 0.004)], rubber, up=(1.0, 0.0, 0.0))
    # Tuerblatt: Kante (Stahl) und Front (Atlas)
    lz = (E_H + 0.008) / 2
    lp = [(0.0, 0.004), (0.0, 0.0275), (0.0012, 0.0296), (0.003, 0.0300)]
    rings = K.ring_sweep(P, lambda ins: [(a, b + lz) for a, b in K.rrect(E_HW - ins, (E_H - 0.008) / 2 - ins, 0.003 - ins * 0.5, 1)], lp,
                         K.frame_wall, frame, cap0=True)
    P.face(list(rings[-1]), leaf, want=(1, 0, 0))
    # Trittblech mit Schrauben
    K.box(P, (0.0300, -E_HW + 0.03, 0.025), (0.0312, E_HW - 0.03, 0.25), frame)
    for sy in (-1, 0, 1):
        for zz in (0.04, 0.235):
            K.screw(P, (0.0312, sy * (E_HW - 0.05), zz), (1, 0, 0), 0.0028, screw, slot, seg=6, rot=sy + zz)
    # Panikstange: Riegelgehaeuse (Schlossseite rechts), Halter (Bandseite links), Druckstange
    K.bevel_box(P, (0.030, 0.270, 0.935), (0.082, 0.430, 1.065), frame, r=0.012, seg=2)
    K.bevel_box(P, (0.078, 0.272, 0.940), (0.0835, 0.428, 1.060), caps, r=0.004, seg=1)
    K.bevel_box(P, (0.030, -0.405, 0.955), (0.074, -0.330, 1.045), frame, r=0.010, seg=2)
    bar = [(0.0, -0.020), (0.010, -0.020), (0.014, -0.016), (0.014, 0.016), (0.010, 0.020), (0.0, 0.020), (-0.010, 0.020), (-0.014, 0.016),
           (-0.014, -0.016), (-0.010, -0.020)]
    K.sweep_open(P, [(0.056, -0.332, 1.0), (0.056, 0.272, 1.0)], bar, frame)
    # Baender (drei Rollen mit Kappen)
    for zc in (0.30, 1.03, 1.78):
        K.revolve(P, [(0.0, zc - 0.058), (0.004, zc - 0.058), (0.0075, zc - 0.052), (0.0075, zc + 0.052), (0.004, zc + 0.058),
                      (0.0, zc + 0.058)], 10, hinge, axis='z', origin=(0.036, -(E_HW + 0.0045), 0.0))
    # Obentuerschliesser mit Gleitarm
    K.bevel_box(P, (0.031, -0.430, 1.900), (0.086, -0.150, 1.985), closer, r=0.012, seg=2)
    K.revolve(P, [(0.0, 0.086), (0.013, 0.086), (0.013, 0.094), (0.0, 0.094)], 12, closer, axis='x', origin=(0.0, -0.405, 1.955))
    K.sweep_open(P, [(0.093, -0.405, 1.955), (0.098, -0.25, 2.035), (0.054, -0.10, 2.093)], [(-0.006, -0.002), (0.006, -0.002),
                                                                                          (0.006, 0.002), (-0.006, 0.002)], closer,
                 up=(0.0, 0.0, 1.0))
    K.bevel_box(P, (0.0475, -0.130, 2.083), (0.060, -0.070, 2.108), closer, r=0.003, seg=1)
    # Schwelle mit Rillen
    K.bevel_box(P, (-0.025, -(E_HW + 0.06), 0.0), (0.070, E_HW + 0.06, 0.006), alu, r=0.002, seg=1)
    for xr in (0.005, 0.020, 0.035, 0.050):
        K.box(P, (xr, -(E_HW + 0.05), 0.006), (xr + 0.004, E_HW + 0.05, 0.0075), alu)
    # Rettungszeichenleuchte ueber der Tuer
    K.bevel_box(P, (0.0, -0.305, 2.185), (0.064, 0.305, 2.395), housing, r=0.010, seg=2)
    vs = [P.v((0.0645, y, z)) for y, z in ((-0.28, 2.20), (0.28, 2.20), (0.28, 2.38), (-0.28, 2.38))]
    P.face(vs, sign, want=(1, 0, 0))
    K.revolve(P, [(0.0035, 0.064), (0.0035, 0.0655), (0.0, 0.0662)], 8, led, axis='x', origin=(0.0, 0.29, 2.1925))
    P.finish(smooth_deg=35.0)


# =====================================================================================================
# Aufzug: Edelstahlrahmen mit Profil, Anzeigegehaeuse, Ruftableau, Schwelle; Kabine mit Paneelen, Sockelleiste,
# Handlauf, Tableau, Lichtdecke. Die Etagenanzeigen (aussen x = 0,045, innen x = -0,065) bleiben aus Code.
# =====================================================================================================
def _stainless(P, name='edelstahl', rgb=(0.80, 0.80, 0.79), r=0.9, m=0.45):
    """Runde 3: lackiertes Stahlblech (hellgrau, abgegriffen) statt blankem Edelstahl - Metall mit Metalltextur wirkte im
    schwachen Licht der Ebenen schwarz; alte Aufzuege in Buerobauten sind oft lackiert."""
    if name == 'edelstahl':
        # Runde 4: ohne die Rost-Textur (painted_metal_grey war fleckig rot) - glatter, leicht abgegriffener Lack
        return P.mat(gen('aufzug_lack', (0.72, 0.71, 0.67), 'paint', r=0.7))
    return P.mat(gen(name, rgb, 'metal', r=r, m=m))


def build_item_elev_frame():
    P = K.Part('aufzug')
    ss = _stainless(P)
    ss_dark = _stainless(P, 'edelstahl_dunkel', (0.46, 0.46, 0.45), 1.0, 0.35)
    shell = P.mat(gen('schacht', (0.55, 0.54, 0.52), 'concrete', r=1.2))
    joint = P.mat(gen('fuge', (0.03, 0.03, 0.03), 'paint', r=1.3))
    alu = P.mat(gen('schwelle', (0.78, 0.78, 0.76), 'metal', r=0.95, tex='metal_aluminum'))
    floor = P.mat(gen('noppenboden', (0.11, 0.11, 0.11), 'rubber', r=1.2))
    glassd = P.mat(gen('anzeige_glas', (0.02, 0.02, 0.02), 'glass', r=0.3))
    btn = P.mat(gen('taster', (0.84, 0.83, 0.79), 'plastic', r=0.55))
    ring = P.mat(gen('tasterring', (1.0, 0.72, 0.35), 'glow', glow=0.25))
    yellow = P.mat(gen('notruf', (0.85, 0.65, 0.05), 'plastic', r=0.6))
    light = P.mat(K.fm_material('flash_lens', (1.0, 0.94, 0.82), name='kabinenlicht'), uvmap=const_uv(0.5, 0.5))
    # Rahmen (Raumseite): Profil um die Oeffnung
    path = [(0.0, -0.98, 0.0), (0.0, -0.98, 2.14), (0.0, 0.98, 2.14), (0.0, 0.98, 0.0)]
    prof = [(0.0, -0.065), (0.0, 0.042), (0.002, 0.048), (0.008, 0.050), (0.152, 0.050), (0.158, 0.046), (0.160, 0.040), (0.160, -0.020),
            (0.020, -0.020), (0.020, -0.065)]
    K.sweep_open(P, path, prof, ss, up=(1.0, 0.0, 0.0))
    # Anzeigegehaeuse mit dunkler Scheibe (die Anzeige aus Code sitzt 5 mm davor)
    K.bevel_box(P, (0.0, -0.26, 2.40), (0.040, 0.26, 2.64), ss, r=0.006, seg=2)
    vs = [P.v((0.0402, y, z)) for y, z in ((-0.15, 2.44), (0.15, 2.44), (0.15, 2.60), (-0.15, 2.60))]
    P.face(vs, glassd, want=(1, 0, 0))
    # Ruftableau (Spiel: y 1,26 .. 1,38 -> Blender -1,38 .. -1,26)
    K.bevel_box(P, (0.0, -1.38, 1.00), (0.006, -1.26, 1.26), ss, r=0.003, seg=1)
    K.revolve(P, [(0.0170, 0.0), (0.0170, 0.0042)], 20, ss_dark, axis='x', origin=(0.006, -1.32, 1.10))
    K.revolve(P, [(0.0170, 0.0042), (0.0125, 0.0042)], 20, ring, axis='x', origin=(0.006, -1.32, 1.10))
    K.revolve(P, [(0.0125, 0.0042), (0.0122, 0.0058), (0.0090, 0.0068), (0.0, 0.0070)], 20, btn, axis='x', origin=(0.006, -1.32, 1.10))
    K.revolve(P, [(0.0085, 0.0), (0.0085, 0.006), (0.0, 0.0065)], 12, ss_dark, axis='x', origin=(0.006, -1.32, 1.20))
    vs = [P.v((0.0126, -1.32 + dy, 1.20 + dz)) for dy, dz in ((-0.0012, -0.004), (0.0012, -0.004), (0.0012, 0.004), (-0.0012, 0.004))]
    P.face(vs, joint, want=(1, 0, 0))
    # Schwelle mit zwei Fuehrungsnuten
    K.bevel_box(P, (-0.070, -1.0, 0.0), (0.060, 1.0, 0.012), alu, r=0.002, seg=1)
    for x0 in (-0.048, -0.028):
        vs = [P.v((x0, -0.98, 0.0122)), P.v((x0 + 0.006, -0.98, 0.0122)), P.v((x0 + 0.006, 0.98, 0.0122)), P.v((x0, 0.98, 0.0122))]
        P.face(vs, joint, want=(0, 0, 1))
    # Kabine: Rueckwand und Seitenwaende aus je drei Paneelen mit Fugen, Sockelleiste
    BX, WY, TOP, FZ = -1.98, 0.98, 2.28, 0.012

    def panels(axis_fixed, fixed, lo, hi, want):
        n = 3
        w = (hi - lo) / n
        for k in range(n):
            a0, a1 = lo + k * w + (0.003 if k else 0.0), lo + (k + 1) * w - (0.003 if k < n - 1 else 0.0)
            if axis_fixed == 'x':
                q = [(fixed, a0, 0.11), (fixed, a1, 0.11), (fixed, a1, TOP), (fixed, a0, TOP)]
                bq = [(fixed - want[0] * 0.004, lo + k * w, FZ), (fixed - want[0] * 0.004, lo + (k + 1) * w, FZ),
                      (fixed - want[0] * 0.004, lo + (k + 1) * w, TOP), (fixed - want[0] * 0.004, lo + k * w, TOP)]
            else:
                q = [(a0, fixed, 0.11), (a1, fixed, 0.11), (a1, fixed, TOP), (a0, fixed, TOP)]
                bq = [(lo + k * w, fixed - want[1] * 0.004, FZ), (lo + (k + 1) * w, fixed - want[1] * 0.004, FZ),
                      (lo + (k + 1) * w, fixed - want[1] * 0.004, TOP), (lo + k * w, fixed - want[1] * 0.004, TOP)]
            P.face([P.v(p) for p in q], ss, want=want)
            P.face([P.v(p) for p in bq], joint, want=want)
    panels('x', BX, -WY, WY, (1, 0, 0))
    panels('y', WY, BX, -0.06, (0, -1, 0))
    panels('y', -WY, BX, -0.06, (0, 1, 0))
    for (lo, hi), want in ((((BX, -WY, FZ), (BX + 0.006, WY, 0.11)), (1, 0, 0)), (((BX, WY - 0.006, FZ), (-0.06, WY, 0.11)), (0, -1, 0)),
                           (((BX, -WY, FZ), (-0.06, -WY + 0.006, 0.11)), (0, 1, 0))):
        K.bevel_box(P, lo, hi, ss_dark, r=0.001, seg=1)
    # Boden und Decke mit Lichtfeld
    vs = [P.v(p) for p in ((0.0, -WY, FZ), (0.0, WY, FZ), (BX, WY, FZ), (BX, -WY, FZ))]
    P.face(vs, floor, want=(0, 0, 1))
    xs = [BX, -1.6, -0.4, 0.0]
    ys = [-WY, -0.6, 0.6, WY]
    for j in range(3):
        for i in range(3):
            if i == 1 and j == 1:
                continue
            vs = [P.v((xs[i], ys[j], TOP)), P.v((xs[i + 1], ys[j], TOP)), P.v((xs[i + 1], ys[j + 1], TOP)), P.v((xs[i], ys[j + 1], TOP))]
            P.face(vs, ss, want=(0, 0, -1))
    K.ring_sweep(P, lambda ins: [(a - 1.0, b) for a, b in K.rrect(0.6 - ins, 0.6 - ins, 0.002, 1)],
                         [(0.0, 0.0), (0.0, 0.010), (0.012, 0.010)], lambda a, b, h: (a, b, TOP - h), ss)
    vs = [P.v((x, y, TOP - 0.009)) for x, y in ((-1.588, -0.588), (-0.412, -0.588), (-0.412, 0.588), (-1.588, 0.588))]
    P.face(vs, light, want=(0, 0, -1))
    # Handlauf an der Rueckwand
    K.revolve(P, [(0.0, -0.82), (0.012, -0.818), (0.019, -0.808), (0.019, 0.808), (0.012, 0.818), (0.0, 0.82)], 14, ss, axis='y',
              origin=(BX + 0.055, 0.0, 0.90))
    for yb in (-0.62, 0.62):
        K.revolve(P, [(0.0, 0.0), (0.008, 0.0), (0.008, 0.052), (0.0, 0.052)], 10, ss, axis='x', origin=(BX + 0.004, yb, 0.90))
        K.revolve(P, [(0.0, 0.0), (0.026, 0.0), (0.026, 0.004), (0.0, 0.005)], 14, ss, axis='x', origin=(BX + 0.0, yb, 0.90))
    # Tableau innen (Spiel y 0,93 .. 0,98 -> Blender an der Wand y = -0,98)
    K.bevel_box(P, (-0.52, -WY, 0.95), (-0.22, -WY + 0.006, 1.55), ss, r=0.003, seg=1)
    for c in range(2):
        for rr in range(5):
            xc, zc = -0.43 + c * 0.12, 1.12 + rr * 0.075
            K.revolve(P, [(0.0135, 0.0), (0.0135, 0.0025)], 16, ss_dark, axis='y', origin=(xc, -WY + 0.006, zc))
            K.revolve(P, [(0.0135, 0.0025), (0.0100, 0.0025)], 16, ring if (c, rr) == (0, 0) else ss_dark, axis='y',
                      origin=(xc, -WY + 0.006, zc))
            K.revolve(P, [(0.0100, 0.0025), (0.0097, 0.0040), (0.0070, 0.0048), (0.0, 0.0050)], 16, btn, axis='y',
                      origin=(xc, -WY + 0.006, zc))
    K.revolve(P, [(0.017, 0.0), (0.017, 0.006), (0.012, 0.009), (0.0, 0.0095)], 18, yellow, axis='y', origin=(-0.37, -WY + 0.006, 1.02))
    for k in range(4):
        vs = [P.v((-0.42 + dx, -WY + 0.0062, 1.48 + k * 0.012 + dz)) for dx, dz in ((0.0, 0.0), (0.10, 0.0), (0.10, 0.004), (0.0, 0.004))]
        P.face(vs, joint, want=(0, 1, 0))
    # Sturz innen (die Innenanzeige aus Code sitzt 5 mm davor)
    K.box(P, (-0.06, -WY, 2.14), (0.0, WY, TOP), ss)
    # Schachtwand aussen um die Kabine (im Spiel im massiven Kern, im Studio sichtbar)
    for q, w in ((((-2.0, -1.0, 0.0), (-2.0, 1.0, 0.0), (-2.0, 1.0, 2.30), (-2.0, -1.0, 2.30)), (-1, 0, 0)),
                 (((-2.0, 1.0, 0.0), (-0.02, 1.0, 0.0), (-0.02, 1.0, 2.30), (-2.0, 1.0, 2.30)), (0, 1, 0)),
                 (((-2.0, -1.0, 0.0), (-0.02, -1.0, 0.0), (-0.02, -1.0, 2.30), (-2.0, -1.0, 2.30)), (0, -1, 0)),
                 (((-2.0, -1.0, 2.30), (-0.02, -1.0, 2.30), (-0.02, 1.0, 2.30), (-2.0, 1.0, 2.30)), (0, 0, 1))):
        P.face([P.v(p) for p in q], shell, want=w)
    P.finish(smooth_deg=35.0)


def build_item_elev_door():
    P = K.Part('aufzugtuer')
    ss = _stainless(P)
    rubber = P.mat(gen('tuergummi', (0.03, 0.03, 0.03), 'rubber', r=1.1))
    K.bevel_box(P, (-0.050, -0.4975, 0.004), (-0.015, 0.4975, 2.14), ss, r=0.003, seg=2)
    for sy in (-1, 1):  # Gummikanten an beiden Seiten (beide Fluegel sind dasselbe Netz)
        K.bevel_box(P, (-0.044, sy * 0.4975 - (0.003 if sy > 0 else 0.0), 0.02), (-0.021, sy * 0.4975 + (0.0 if sy > 0 else 0.003), 2.12),
                    rubber, r=0.001, seg=1)
    P.finish(smooth_deg=35.0)
