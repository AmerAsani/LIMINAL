"""V12/V13.3 Level 0 (LOOK_0.md): Rasterleuchte, Luftauslass, Steckdose, Lueftungsgitter, Boegen.

Deckenmodelle haben den Ursprung in der Deckenebene (z = 0) und haengen nach unten; die Rasterdecke hat ein
0,6-m-Raster (ceiling_office, 3,6 m Textur). Wandmodelle: Rueckseite bei x = 0, blicken entlang +x. Boegen stehen
mit dem Ursprung in der Mitte des Durchbruchs am Boden; die Laibung laeuft entlang x (Wandstaerke 1 m), die
Oeffnung entlang y.

V13.3 (PROPS_Level0.md): alles neu mit echten Profilen - Rahmen als umlaufende Profile (kit_items.ring_sweep),
Prismenscheibe mit hellen Baendern ueber den Roehren und vergilbtem, staubigem Rand, Luftauslass mit schraegen
Lamellen und Staubhof an der Decke, Schuko-Steckdose mit Topf, Schutzkontakten und Schraube, Gitter mit
gekanteten Lamellen, Schrauben und Staubfahne an der Wand, Bogen mit gerundeter Kante, Kaempferprofil (Kehle,
Plaettchen) und Haarrissen. Keine Varianten (die Karte setzt keine).
"""
import math
import random

import bmesh
import bpy

import common
from models import kit_items as K

TAU = 2 * math.pi


# ---------------------------------------------------------------------------------------------------
# Rasterleuchte 0,6 x 1,2 m (lange Achse x) - kommt rund 800-mal vor: sparsam (~420 Dreiecke)
# ---------------------------------------------------------------------------------------------------
_TUBES = (-0.15, 0.0, 0.15)  # drei T8-Roehren hinter der Scheibe


def _lens_class(cx, cy, hx, hy, weak=None):
    if abs(cx) > hx - 0.026 or abs(cy) > hy - 0.024:
        return 'rim'
    near_t = min(_TUBES, key=lambda t: abs(cy - t))
    w = '_w' if weak is not None and near_t == weak else ''
    if abs(cx) > hx - 0.07:
        return 'end' + w
    d = abs(cy - near_t)
    if d < 0.017:
        return 'tube' + w
    if d < 0.047:
        return 'near' + w
    return 'mid'


def _troffer(state, var=0):
    """var 0: wie gebaut; 1: eine Roehre stirbt (dunkler, schwarze Enden) bzw. aus: Scheibe gesprungen, an einem
    Ende abgesackt; 2: staerker vergilbt, mehr Insekten, Wasserfleck (Variation nur im Kleinen, LEVEL0_STANDARD.md 2)."""
    P = K.Part('rasterleuchte')
    frame = P.mat(common.material('rahmen', (0.85, 0.84, 0.78), look='paint', r=0.8))
    frame2 = P.mat(common.material('tuerrahmen', (0.83, 0.82, 0.75), look='paint', r=0.7))
    dust = P.mat(common.material('rahmen_staub', (0.60, 0.58, 0.51), look='paint', r=1.25))
    gap = P.mat(common.material('spalt', (0.06, 0.06, 0.055), look='paint', r=1.3))
    kehle = P.mat(common.material('kehle', (0.64, 0.63, 0.58), look='paint', r=0.9))
    bug = P.mat(common.material('insekt', (0.07, 0.055, 0.04), look='paint', r=1.0))
    if state == 'off':
        stain = P.mat(common.material('wasserfleck', (0.36, 0.28, 0.16), look='paint', r=1.1))
    else:  # Runde 3: Wasserfleck in der leuchtenden Scheibe als bräunlicher, schwaecher leuchtender Hof statt Schwarz
        stain = P.mat(common.material('wasserfleck_licht', (0.95, 0.62, 0.30) if state != 'red' else (0.8, 0.2, 0.1), look='glow', glow=0.42))
    yel = 1.0 if var != 2 else 0.93
    weak = _TUBES[2] if (var == 1 and state != 'off') else None
    if state == 'off':
        dk = 0.86 if var == 2 else 1.0
        lens = {k: P.mat(common.material('scheibe_' + k, tuple(c[i] * dk * (1 if i < 2 else yel) for i in range(3)), look='plastic', r=rr))
                for k, c, rr in (('tube', (0.80, 0.80, 0.77), 0.55), ('near', (0.77, 0.77, 0.74), 0.6), ('mid', (0.72, 0.72, 0.69), 0.6),
                                 ('end', (0.66, 0.65, 0.61), 0.7), ('rim', (0.55, 0.53, 0.46), 1.0))}
    else:
        red = state == 'red'
        spec = (('tube', 1.25, (1.0, 0.99, 0.93), (1.0, 0.36, 0.27)), ('near', 1.04, (1.0, 0.985, 0.91), (1.0, 0.32, 0.24)),
                ('mid', 0.86, (1.0, 0.975, 0.88), (0.98, 0.29, 0.21)), ('end', 0.70, (1.0, 0.96, 0.84), (0.95, 0.27, 0.19)),
                ('rim', 0.50, (0.98, 0.91, 0.74), (0.85, 0.24, 0.16)),
                ('tube_w', 0.60, (0.98, 0.95, 0.90), (0.95, 0.30, 0.24)), ('near_w', 0.72, (0.98, 0.95, 0.88), (0.93, 0.28, 0.21)),
                ('end_w', 0.45, (0.86, 0.80, 0.70), (0.70, 0.20, 0.14)))
        lens = {k: P.mat(common.material('scheibe_' + k, rc if red else (wc[0], wc[1], wc[2] * yel), look='glow', glow=g))
                for k, g, wc, rc in spec}
        for k in ('tube', 'near', 'end'):
            lens.setdefault(k + '_w', lens[k])
    HX, HY = 0.606, 0.306
    # Profil von aussen nach innen: Gehaeuseflansch, Schattenfuge, Tuerrahmen, Kehle zur Scheibe
    prof = [(0.000, 0.000), (0.000, 0.0092), (0.0012, 0.0118), (0.0030, 0.0122), (0.0095, 0.0122), (0.0108, 0.0110),
            (0.0112, 0.0062), (0.0128, 0.0062), (0.0132, 0.0106), (0.0148, 0.0118), (0.0285, 0.0118), (0.0300, 0.0106),
            (0.0312, 0.0064), (0.0330, 0.0050)]
    mis = [dust, frame, frame, frame, frame, gap, gap, gap, frame2, frame2, frame2, kehle, kehle]
    K.ring_sweep(P, lambda ins: K.rrect(HX - ins, HY - ins, 0.0025, 1), prof, K.frame_ceiling, mis)
    # Prismenscheibe: Felder mit eigener Leuchtdichte (Baender ueber den Roehren, dunklere Enden, staubiger Rand),
    # leicht durchgebogen (alte Acrylscheibe)
    hx, hy = HX - 0.033, HY - 0.033
    xs = [-hx, -hx + 0.026, -hx + 0.07, -0.2, 0.2, hx - 0.07, hx - 0.026, hx]
    ys = [-hy, -hy + 0.024, -0.19, -0.167, -0.133, -0.103, -0.047, -0.017, 0.017, 0.047, 0.103, 0.133, 0.167, 0.19,
          hy - 0.024, hy]

    def lz(x, y):
        sag = 0.0050 + 0.0016 * (1 - (x / hx) ** 2) * (1 - (y / hy) ** 2)
        if state == 'off' and var == 1:  # an einem Ende aus dem Falz gerutscht
            sag += 0.006 * max(0.0, (x / hx) - 0.2) / 0.8
        return sag
    grid = [[P.v((x, y, -lz(x, y))) for x in xs] for y in ys]
    for j in range(len(ys) - 1):
        for i in range(len(xs) - 1):
            cx, cy = (xs[i] + xs[i + 1]) / 2, (ys[j] + ys[j + 1]) / 2
            P.face((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]), lens[_lens_class(cx, cy, hx, hy, weak)],
                   want=(0, 0, -1))
    # tote Insekten auf der Scheibe (Schatten von innen) in einer Ecke, zwei Riegelschlitze an der Laengsseite
    rnd = random.Random(7 + var)
    bugs = [(0.52, 0.226, 0.006), (0.505, 0.238, 0.004), (0.537, 0.205, 0.005), (-0.49, -0.24, 0.004)]
    if var == 2:
        bugs += [(rnd.uniform(-0.5, 0.5), rnd.choice((-1, 1)) * rnd.uniform(0.22, 0.245), rnd.uniform(0.003, 0.006)) for _ in range(7)]
        f = K.blob(P, (-0.30, 0.06), 0.07, 0.045, stain, seg=12, seed=91, rough=0.25, frame=K.frame_ceiling, h=lz(-0.3, 0.06) + 0.0003)
        P.fix([f], (0, 0, -1))
    if state == 'off' and var == 1:  # Sprung quer ueber die Scheibe
        x, y = -0.15, -hy + 0.03
        while y < hy - 0.03:
            x2, y2 = x + rnd.uniform(-0.035, 0.035), min(hy - 0.03, y + rnd.uniform(0.03, 0.06))
            w = 0.0011
            vs = [P.v((x - w, y, -lz(x, y) - 0.0003)), P.v((x + w, y, -lz(x, y) - 0.0003)), P.v((x2 + w, y2, -lz(x2, y2) - 0.0003)),
                  P.v((x2 - w, y2, -lz(x2, y2) - 0.0003))]
            P.face(vs, gap, want=(0, 0, -1))
            x, y = x2, y2
    for k, (bx, by, s) in enumerate(bugs):
        f = K.blob(P, (bx, by), s, s * rnd.uniform(0.5, 0.9), bug, seg=6, seed=11 + k, frame=K.frame_ceiling,
                   h=lz(bx, by) + 0.0004)
        P.fix([f], (0, 0, -1))
    for x in (-0.36, 0.36):
        vs = [P.v((x + dx, -HY + 0.0185 + dy, -0.01185)) for dx, dy in ((-0.011, -0.0022), (0.011, -0.0022), (0.011, 0.0022), (-0.011, 0.0022))]
        P.face(vs, gap, want=(0, 0, -1))
    P.finish(smooth_deg=30.0)


def build_troffer():
    _troffer('on')


def build_troffer__v1():
    _troffer('on', 1)


def build_troffer__v2():
    _troffer('on', 2)


def build_troffer_off():
    _troffer('off')


def build_troffer_off__v1():
    _troffer('off', 1)


def build_troffer_red():
    """Rote Raeume (Wikidot: roetlicher Schimmer) - die Scheibe gibt rotes Licht."""
    _troffer('red')


def build_troffer_red__v1():
    _troffer('red', 1)


# ---------------------------------------------------------------------------------------------------
# Luftauslass 0,6 x 0,6 m (Vierwege-Kegelauslass) mit Staubhof an der Decke
# ---------------------------------------------------------------------------------------------------
def build_ceiling_diffuser():
    _diffuser(0)


def build_ceiling_diffuser__v1():
    _diffuser(1)


def build_ceiling_diffuser__v2():
    _diffuser(2)


def _diffuser(var):
    """var 0: Staubhof; 1: fast sauber (neu gestrichen); 2: starker Staubhof, eine Lamelle verbogen."""
    P = K.Part('luftauslass')
    paint = P.mat(common.material('blech', (0.86, 0.85, 0.81), look='paint', r=0.7))
    lip = P.mat(common.material('blech_kante', (0.70, 0.69, 0.64), look='paint', r=1.0))
    dusty = P.mat(common.material('blech_staub', (0.56, 0.54, 0.49), look='paint', r=1.3))
    dark = P.mat(common.material('schacht', (0.11, 0.11, 0.105), look='galvanized', r=1.3))
    screw = P.mat(common.material('schraube', (0.72, 0.72, 0.70), look='metal', r=0.6))
    slot = P.mat(common.material('schlitz', (0.04, 0.04, 0.04), look='paint', r=1.3))
    soot = [P.mat(common.material(f'deckenstaub{k}', c, look='paint', tex='ceiling_office', r=1.1))
            for k, c in enumerate(((0.50, 0.48, 0.42), (0.63, 0.61, 0.54), (0.75, 0.73, 0.66)))]
    H = 0.298
    # Einlegerand: flach, aussen gerollt, innen zur ersten Lamelle abgekantet
    prof = [(0.000, 0.000), (0.000, 0.005), (0.0015, 0.0085), (0.005, 0.0098), (0.034, 0.0100), (0.037, 0.0090),
            (0.039, 0.0040), (0.041, 0.0010)]
    K.ring_sweep(P, lambda ins: K.rrect(H - ins, H - ins, 0.003, 1), prof, K.frame_ceiling,
                 [paint, paint, paint, paint, paint, lip, lip])
    # vier schraege Lamellen (Blech 1,2 mm, unten eingerollt), nach innen tiefer
    for k, s in enumerate((0.252, 0.198, 0.144, 0.090)):
        h0, h1 = 0.004 + 0.0055 * k, 0.019 + 0.0062 * k
        w = 0.046
        t = 0.0012
        blade = [(0.0, h0), (w * 0.55, h0 + (h1 - h0) * 0.62), (w - 0.003, h1), (w, h1 + 0.0016), (w - 0.0005, h1 + 0.0032),
                 (w - 0.003, h1 + t * 0.6), (w * 0.55, h0 + (h1 - h0) * 0.62 - t), (0.0, h0 - t)]
        n0 = K.count(P)
        K.ring_sweep(P, lambda ins, s=s: K.rrect(s - ins, s - ins, 0.0015, 1), blade, K.frame_ceiling,
                     [paint, paint, lip, dusty, dusty, paint, paint, paint], closed=True)
        if var == 2 and k == 1:  # eine Seite der zweiten Lamelle haengt durch (jemand hat mit dem Besenstiel gestossen)
            for v in K.new_verts_since(P, n0):
                if v.co.x > 0.12:
                    v.co.z -= 0.0045 * max(0.0, 1.0 - abs(v.co.y) / 0.2)
    # Kern: flache Platte mit Schraube in der Mitte
    K.bevel_box(P, (-0.036, -0.036, -0.047), (0.036, 0.036, -0.030), paint, r=0.004, seg=2)
    K.screw(P, (0.0, 0.0, -0.047), (0, 0, -1), 0.0042, screw, slot, seg=8, rot=0.4)
    # dunkler Schacht hinter den Lamellen
    vs = [P.v((x, y, -0.0012)) for x, y in ((-0.258, -0.258), (0.258, -0.258), (0.258, 0.258), (-0.258, 0.258))]
    P.face(vs, dark, want=(0, 0, -1))
    # Staubhof an der Decke: vier Zungen in Blasrichtung, innen dunkler (Deckentextur, getoent)
    halo = {0: ((0.027, soot[2]), (0.018, soot[1]), (0.009, soot[0])), 1: ((0.012, soot[2]),),
            2: ((0.030, soot[1]), (0.021, soot[0]), (0.012, soot[0]))}[var]
    for lvl, (ext, mi) in enumerate(halo):
        for side in range(4):
            rnd = random.Random(31 + side * 7 + lvl)
            pts = []
            n = 9
            for i in range(n + 1):
                t = -0.9 + 1.8 * i / n
                bulge = ext * (1.0 - 0.55 * t * t) * (0.8 + 0.4 * rnd.random())
                pts.append((t * H, H + max(bulge, 0.002)))
            pts += [(0.9 * H, H - 0.0005), (-0.9 * H, H - 0.0005)]
            ang = side * math.pi / 2
            ca, sa = math.cos(ang), math.sin(ang)
            vs = [P.v((a * ca - b * sa, a * sa + b * ca, -0.0006 - 0.0002 * lvl)) for a, b in pts]
            P.face(vs, mi, want=(0, 0, -1))
    P.finish(smooth_deg=35.0)


# ---------------------------------------------------------------------------------------------------
# Boegen: Segmentbogen im Durchbruch (Laibung 1 m), gerundete Kante, Kaempferprofil, Haarrisse
# ---------------------------------------------------------------------------------------------------
def _arch(width):
    P = K.Part('bogen')
    putz = P.mat(common.material('putz', (0.93, 0.89, 0.78), look='concrete', tex='plaster_white', r=1.05))
    kante = P.mat(common.material('putz_kante', (0.83, 0.79, 0.69), look='concrete', tex='plaster_white', r=0.95))
    riss = P.mat(common.material('riss', (0.30, 0.27, 0.22), look='concrete', tex='plaster_white', r=1.2))
    profil = P.mat(common.material('profil', (0.92, 0.89, 0.80), look='paint', r=0.75))
    half = width / 2.0
    spring, crown, top = 1.85, 2.30, 2.355
    rise = crown - spring
    radius = (half * half + rise * rise) / (2 * rise)
    cz = crown - radius
    depth = 0.5
    face_x = depth + 0.0012     # Stirnflaechen knapp vor der Wand (kein Flimmern)
    rf = 0.014                  # Kantenrundung
    n = 44
    a0 = math.atan2(spring - cz, -half)
    a1 = math.atan2(spring - cz, half)
    curve = []
    for i in range(n + 1):
        t = i / n
        a = a0 + (a1 - a0) * t
        # von Hand gezogener Putz: leicht unregelmaessiger Radius (nicht an den Kaempfern)
        e = (0.0022 * math.sin(t * 9.1 + 0.4) + 0.0013 * math.sin(t * 23.0 + 1.3)) * math.sin(math.pi * t)
        r = radius + e
        p = (r * math.cos(a), cz + r * math.sin(a))
        nrm = (-math.cos(a), -math.sin(a))  # zur Oeffnung (Bogenmitte)
        curve.append((p, nrm))
    nf = 3
    # Ringe: Laibung bei +/-(depth - rf), Rundung bis zur Stirn
    rows = []
    for (p, nrm) in curve:
        ring = []
        for sgn in (1, -1):
            for k in range(nf + 1):
                ph = math.pi / 2 * k / nf
                cxp = sgn * (face_x - rf)
                y = p[0] - nrm[0] * rf + rf * math.cos(ph) * nrm[0]
                z = p[1] - nrm[1] * rf + rf * math.cos(ph) * nrm[1]
                x = cxp + sgn * rf * math.sin(ph)
                ring.append(P.v((x, y, z)))
        rows.append(ring)
    for i in range(n):
        (p, nrm) = curve[i]
        (p1, nrm1) = curve[i + 1]
        nm = ((nrm[0] + nrm1[0]) / 2, (nrm[1] + nrm1[1]) / 2)
        A, B = rows[i], rows[i + 1]
        # Laibung zwischen den beiden Rundungen
        P.face((A[0], B[0], B[nf + 1], A[nf + 1]), putz, want=(0, nm[0], nm[1]))
        for sgn, off in ((1, 0), (-1, nf + 1)):
            for k in range(nf):
                ph = math.pi / 2 * (k + 0.5) / nf
                w = (sgn * math.sin(ph), nm[0] * math.cos(ph), nm[1] * math.cos(ph))
                P.face((A[off + k], A[off + k + 1], B[off + k + 1], B[off + k]), kante, want=w)
    # Stirnflaechen (Zwickel bis zum Sturz)
    for sgn, off in ((1, nf), (-1, 2 * nf + 1)):
        vs = [rows[i][off] for i in range(n + 1)]
        e0, e1 = vs[0].co, vs[-1].co
        vs += [P.v((e1.x, e1.y, top)), P.v((e0.x, e0.y, top))]
        P.face(vs, putz, want=(sgn, 0, 0))
    # Kaempferprofil: Rundstab, Kehle, Plaettchen; laeuft um die Ecke auf die Wandflaechen (Gehrung)
    prof = [(0.0, -0.070), (0.005, -0.070), (0.0085, -0.0665), (0.0095, -0.0615), (0.0090, -0.0585)]
    for k in range(6):
        th = math.radians(90 * k / 5)
        prof.append((0.034 - 0.022 * math.cos(th), -0.058 + 0.022 * math.sin(th)))
    prof += [(0.0365, -0.0345), (0.0365, -0.0075), (0.0345, -0.0025), (0.030, 0.0), (0.0, 0.0)]
    ret = 0.055
    for sy in (1, -1):
        y = sy * half
        if sy > 0:
            path = [(face_x, y + ret, spring), (face_x, y, spring), (-face_x, y, spring), (-face_x, y + ret, spring)]
        else:
            path = [(-face_x, y - ret, spring), (-face_x, y, spring), (face_x, y, spring), (face_x, y - ret, spring)]
        K.sweep_open(P, path, prof, profil)
    # Haarrisse: vom Scheitel nach oben (beide Stirnseiten verschieden) und ein kurzer in der Laibung
    for sgn, seed in ((1, 3), (-1, 8)):
        rnd = random.Random(seed + int(width * 10))
        y, z = rnd.uniform(-0.12, 0.12), crown + 0.004
        pts = [(y, z)]
        while z < top - 0.01:
            y += rnd.uniform(-0.018, 0.018)
            z = min(top - 0.004, z + rnd.uniform(0.008, 0.016))
            pts.append((y, z))
        for (ya, za), (yb, zb) in zip(pts, pts[1:]):
            w = 0.0007
            vs = [P.v((sgn * (face_x + 0.0004), ya - w, za)), P.v((sgn * (face_x + 0.0004), ya + w, za)),
                  P.v((sgn * (face_x + 0.0004), yb + w * 0.8, zb)), P.v((sgn * (face_x + 0.0004), yb - w * 0.8, zb))]
            P.face(vs, riss, want=(sgn, 0, 0))
    rnd = random.Random(int(width * 13))
    x0 = rnd.uniform(-0.3, 0.1)
    for k in range(6):
        xa, xb = x0 + k * 0.028, x0 + (k + 1) * 0.028
        ya, yb = 0.01 * math.sin(k * 1.7), 0.01 * math.sin((k + 1) * 1.7)
        za, zb = crown - 0.0006, crown - 0.0006
        vs = [P.v((xa, ya - 0.0006, za)), P.v((xb, yb - 0.0006, zb)), P.v((xb, yb + 0.0006, zb)), P.v((xa, ya + 0.0006, za))]
        P.face(vs, riss, want=(0, 0, -1))
    P.finish(smooth_deg=40.0)


def build_arch2():
    _arch(2.0)


def build_arch3():
    _arch(3.0)


def build_arch4():
    _arch(4.0)


# ---------------------------------------------------------------------------------------------------
# Schuko-Doppelsteckdose (Rahmen 151 x 80 mm, ueber dem Sockel): leicht vergilbter Kunststoff, beim
# Streichen der Wand am Rand mit ueberstrichen, Staubrand unten
# ---------------------------------------------------------------------------------------------------
def build_outlet():
    _outlet(0)


def build_outlet__v1():
    _outlet(1)


def build_outlet__v2():
    _outlet(2)


def _outlet(var):
    """var 0: beim Streichen ueberstrichen; 1: sauber abgeklebt, nur Griffschmutz; 2: Ecke gesprungen, Schraube fehlt."""
    P = K.Part('steckdose')
    plast = P.mat(common.material('kunststoff', (0.88, 0.86, 0.78), look='plastic', r=1.15))
    insert = P.mat(common.material('einsatz', (0.85, 0.83, 0.75), look='plastic', r=1.05))
    cup = P.mat(common.material('topf', (0.78, 0.76, 0.68), look='plastic', r=1.2))
    grime = P.mat(common.material('griffschmutz', (0.62, 0.59, 0.50), look='plastic', r=1.35))
    overpaint = P.mat(common.material('ueberstrichen', (0.88, 0.77, 0.43), look='paint', tex='wallpaper_threshold', r=1.1))
    hole = P.mat(common.material('loch', (0.025, 0.025, 0.025), look='plastic', r=1.4))
    metal = P.mat(common.material('schutzkontakt', (0.74, 0.72, 0.66), look='metal', r=0.55))
    screw = P.mat(common.material('schraube', (0.70, 0.69, 0.66), look='metal', r=0.6))
    slot = P.mat(common.material('schlitz', (0.05, 0.05, 0.05), look='plastic', r=1.3))
    HY, HZ = 0.0755, 0.0405
    FH = 0.0102  # Hoehe der Rahmenflaeche ueber der Wand
    prof = [(0.000, 0.000), (0.0004, 0.0035), (0.0016, 0.0072), (0.0036, 0.0093), (0.0065, 0.0100), (0.0100, FH)]
    K.ring_sweep(P, lambda ins: K.rrect(HY - ins, HZ - ins, 0.0065 - ins * 0.5, 3), prof, K.frame_wall,
                 [overpaint if var != 1 else grime, grime if var != 1 else plast, plast, plast, plast])
    # Rahmenflaeche mit zwei quadratischen Ausschnitten (Einsaetze 55 x 55 mm, Abstand 71 mm)
    iy, iz = HY - 0.0100, HZ - 0.0100
    cy0 = 0.0355
    a = 0.0272
    ys = [-iy, -cy0 - a, -cy0 + a, cy0 - a, cy0 + a, iy]
    zs = [-iz, -a, a, iz]
    G = [[P.v((FH, y, z)) for y in ys] for z in zs]
    for j in range(3):
        for i in range(5):
            if j == 1 and i in (1, 3):
                continue
            mi = grime if (j == 0 and i in (0, 4)) else plast
            P.face((G[j][i], G[j][i + 1], G[j + 1][i + 1], G[j + 1][i]), mi, want=(1, 0, 0))
    for sy in (-1, 1):
        cy = sy * cy0
        # Ausschnittkante (vom Rahmen zurueck zur Einsatzplatte)
        sq = [(cy - a, -a), (cy + a, -a), (cy + a, a), (cy - a, a)]
        top = [P.v((FH, y, z)) for y, z in sq]
        bot = [P.v((0.0062, y, z)) for y, z in sq]
        for i in range(4):
            j = (i + 1) % 4
            my, mz = (sq[i][0] + sq[j][0]) / 2, (sq[i][1] + sq[j][1]) / 2
            P.face((top[i], top[j], bot[j], bot[i]), plast, want=(0.0, cy - my, -mz))
        # Einsatzplatte: Quadrat mit runder Oeffnung des Topfs (r 19,5 mm), leicht gewoelbt
        N = 24
        R = 0.0196
        ring_in, ring_out = [], []
        for k in range(N):
            th = 2 * math.pi * k / N
            c, s = math.cos(th), math.sin(th)
            m = max(abs(c), abs(s))
            ring_in.append(P.v((0.0086, cy + R * c, R * s)))
            ring_mid = (0.0081, cy + (R + 0.0035) * c, (R + 0.0035) * s)
            ring_out.append((P.v(ring_mid), P.v((0.0064, cy + (a - 0.0006) * c / m, (a - 0.0006) * s / m))))
        for k in range(N):
            k1 = (k + 1) % N
            P.face((ring_in[k], ring_in[k1], ring_out[k1][0], ring_out[k][0]), insert, want=(1, 0, 0))
            P.face((ring_out[k][0], ring_out[k1][0], ring_out[k1][1], ring_out[k][1]), insert, want=(1, 0, 0))
        # Topf: gerundeter Rand, Wand, Boden mit Stiftloechern
        prof_c = [(R, 0.0086), (R - 0.0010, 0.0081), (R - 0.0013, 0.0070), (R - 0.0014, 0.0016), (R - 0.0024, 0.0008),
                  (0.0, 0.0008)]
        f0 = K.fcount(P)
        K.revolve(P, prof_c, N, cup, axis='x', origin=(0.0, cy, 0.0))
        K.fix_toward(P, K.new_faces_since(P, f0), (0.03, cy, 0.0))  # Innenseite des Topfs zeigt nach vorn
        for dz in (-0.0095, 0.0095):  # Stiftloecher 4,8 mm (horizontal nebeneinander)
            vs = []
            for k in range(10):
                th = 2 * math.pi * k / 10
                vs.append(P.v((0.0011, cy + dz + 0.0024 * math.cos(th), 0.0024 * math.sin(th))))
            P.face(vs, hole, want=(1, 0, 0))
        # Schutzkontakte oben und unten (Federbuegel aus Blech)
        for sz in (-1, 1):
            n0 = K.count(P)
            K.bevel_box(P, (-0.0046, -0.0011, 0.0), (0.0046, 0.0011, 0.0062), metal, r=0.0005, seg=1)
            vs = K.new_verts_since(P, n0)
            for v in vs:
                x, y, z = v.co
                v.co = (0.0010 + z, cy + x, sz * (R - 0.0018) - y * sz)
        # Mittelschraube des Einsatzes
        if var == 2 and sy > 0:  # Schraube fehlt: nur das Loch
            vs = [P.v((0.0010, cy + 0.0016 * math.cos(a), 0.0016 * math.sin(a))) for a in [TAU * i / 8 for i in range(8)]]
            P.face(vs, hole, want=(1, 0, 0))
        else:
            K.screw(P, (0.0009, cy, 0.0), (1, 0, 0), 0.0026, screw, slot, seg=8, rot=0.3 + sy * 0.5)
    # Ueberstrichene Stellen am oberen Rand (Farbnasen) bzw. Griffschmutz rechts unten
    if var != 1:
        for k, (y, z, ry, rz) in enumerate(((0.035, HZ - 0.0035, 0.009, 0.0022), (-0.052, HZ - 0.003, 0.006, 0.0018))):
            f = K.blob(P, (y, z), ry, rz, overpaint, seg=8, seed=40 + k, frame=K.frame_wall, h=0.0098)
            P.fix([f], (1, 0, 0))
    if var == 1:
        f = K.blob(P, (0.050, -HZ + 0.006), 0.016, 0.0045, grime, seg=10, seed=44, frame=K.frame_wall, h=0.01025)
        P.fix([f], (1, 0, 0))
    if var == 2:  # Sprung in der Ecke links oben (Haarriss durch den Rahmen)
        y, z = -HY + 0.004, HZ - 0.012
        rnd = random.Random(5)
        for i in range(5):
            y2, z2 = y + rnd.uniform(0.003, 0.006), z + rnd.uniform(-0.004, 0.003)
            vs = [P.v((0.01026, y, z - 0.0003)), P.v((0.01026, y2, z2 - 0.0003)), P.v((0.01026, y2, z2 + 0.0003)), P.v((0.01026, y, z + 0.0003))]
            P.face(vs, hole, want=(1, 0, 0))
            y, z = y2, z2
    P.finish(smooth_deg=35.0)


# ---------------------------------------------------------------------------------------------------
# Lueftungsgitter (Wand, nahe der Decke): 540 x 320 mm, gekantete Lamellen, vier Schrauben, Staub auf den
# Lamellen, Staubfahne an der Wand darueber
# ---------------------------------------------------------------------------------------------------
def build_vent_grille():
    _vent(0)


def build_vent_grille__v1():
    _vent(1)


def _vent(var):
    """var 0: wie gebaut; 1: eine Lamelle eingedrueckt, staerkere Staubfahne."""
    P = K.Part('lueftungsgitter')
    paint = P.mat(common.material('lack', (0.85, 0.84, 0.79), look='paint', r=0.8))
    edge = P.mat(common.material('lack_kante', (0.70, 0.69, 0.64), look='paint', r=1.0))
    dusty = P.mat(common.material('lack_staub', (0.55, 0.53, 0.48), look='paint', r=1.35))
    dark = P.mat(common.material('kanal', (0.075, 0.075, 0.072), look='galvanized', r=1.3))
    chip = P.mat(common.material('abplatzer', (0.45, 0.45, 0.44), look='metal', r=0.9))
    screw = P.mat(common.material('schraube', (0.66, 0.66, 0.63), look='metal', r=0.65))
    slot = P.mat(common.material('schlitz', (0.04, 0.04, 0.04), look='paint', r=1.3))
    streak = [P.mat(common.material(f'wandstaub{k}', c, look='paint', tex='wallpaper_threshold', r=1.1))
              for k, c in enumerate(((0.50, 0.45, 0.29), (0.65, 0.57, 0.35)))]
    HY, HZ = 0.270, 0.160
    prof = [(0.000, 0.000), (0.000, 0.0026), (0.0018, 0.0046), (0.0055, 0.0052), (0.0225, 0.0058), (0.0255, 0.0078),
            (0.0285, 0.0124), (0.0310, 0.0136), (0.0335, 0.0128), (0.0352, 0.0098), (0.0362, 0.0040), (0.0366, 0.0004)]
    K.ring_sweep(P, lambda ins: K.rrect(HY - ins, HZ - ins, 0.0045, 2), prof, K.frame_wall,
                 [edge, paint, paint, paint, paint, paint, edge, paint, paint, paint, paint])
    iy, iz = HY - 0.0366, HZ - 0.0366
    # Kanal dahinter
    vs = [P.v((0.0006, y, z)) for y, z in ((-iy, -iz), (iy, -iz), (iy, iz), (-iy, iz))]
    P.face(vs, dark, want=(1, 0, 0))
    # Lamellen: gekantetes Blech, vorn tiefer als hinten (Sichtschutz von unten), Enden geschlossen
    pitch = 0.0225
    nbl = int((2 * iz - 0.006) / pitch)
    z0 = -iz + 0.004 + (2 * iz - 0.008 - (nbl - 1) * pitch) / 2
    for b in range(nbl):
        zc = z0 + b * pitch
        sec = [(0.0012, zc + 0.0095), (0.0050, zc + 0.0068), (0.0112, zc - 0.0015), (0.0130, zc - 0.0062),
               (0.0119, zc - 0.0070), (0.0100, zc - 0.0024), (0.0040, zc + 0.0056), (0.0008, zc + 0.0082)]
        dent = (lambda y: 0.0) if not (var == 1 and b == nbl - 3) else (lambda y: -0.0035 * max(0.0, 1 - abs(y - 0.08) / 0.12))
        ys = [-iy, -0.04, 0.02, 0.08, 0.14, 0.20, iy] if var == 1 and b == nbl - 3 else [-iy, iy]
        rings_b = [[P.v((x + dent(y), y, z + dent(y) * 0.6)) for x, z in sec] for y in ys]
        for ra, rb in zip(rings_b, rings_b[1:]):
            for i in range(len(sec)):
                j = (i + 1) % len(sec)
                P.face((ra[i], ra[j], rb[j], rb[i]), paint)
        P.face(list(reversed(rings_b[0])), paint)
        P.face(list(rings_b[-1]), paint)
    # senkrechter Steg hinten in der Mitte
    K.box(P, (0.0008, -0.004, -iz), (0.0045, 0.004, iz), edge)
    # vier Schrauben in den Ecken des Flansches, zwei Abplatzer
    for sy in (-1, 1):
        for sz in (-1, 1):
            K.screw(P, (0.0057, sy * (HY - 0.014), sz * (HZ - 0.014)), (1, 0, 0), 0.0038, screw, slot, seg=8,
                    rot=0.6 * sy + 0.3 * sz)
    for k, (y, z, r) in enumerate(((0.21, 0.152, 0.004), (-0.247, -0.11, 0.003))):
        f = K.blob(P, (y, z), r, r * 0.7, chip, seg=7, seed=60 + k, frame=K.frame_wall, h=0.0060)
        P.fix([f], (1, 0, 0))
    # Staubfahne ueber dem Gitter (Wandtextur der Zone, dunkler getoent)
    for lvl, (ext, mi) in enumerate(((0.028, streak[1]), (0.014, streak[0])) if var == 0 else ((0.040, streak[1]), (0.022, streak[0]))):
        rnd = random.Random(70 + lvl)
        pts = []
        n = 12
        for i in range(n + 1):
            t = -1.0 + 2.0 * i / n
            h = ext * (1.0 - 0.7 * t ** 4) * (0.75 + 0.5 * rnd.random())
            pts.append((t * (HY - 0.01), HZ - 0.003 + h))
        pts += [(HY - 0.01, HZ - 0.004), (-(HY - 0.01), HZ - 0.004)]
        vs = [P.v((0.0003 + 0.0002 * lvl, y, z)) for y, z in pts]
        P.face(vs, mi, want=(1, 0, 0))
    ob = P.finish(smooth_deg=35.0)
    # Staub auf den Oberseiten der Lamellen
    if ob is not None:
        me = ob.data
        mi_d = [i for i, m in enumerate(me.materials) if m.name.endswith('#lack_staub')][0]
        mi_p = [i for i, m in enumerate(me.materials) if m.name.endswith('#lack')][0]
        for p in me.polygons:
            if p.material_index == mi_p and p.normal.z > 0.55 and 0.002 < p.center.x < 0.019 and abs(p.center.z) < HZ - 0.03:
                p.material_index = mi_d
