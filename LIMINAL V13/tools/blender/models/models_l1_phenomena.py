"""V13.3: Level 1 - Erscheinungen ohne eigene Figur (KREATUR_Crawler.md, KREATUR_GameMaster.md, KREATUR_L1_Erscheinungen.md).
Wiki (backrooms-wiki.wikidot.com, CC BY-SA 3.0; eigene Worte):
- Entity 17 "Crawlers" (Rev. 40): ein Pilz, der nur ueber Fluessigkeiten auf Menschen uebergeht und warme, dunkle, feuchte
  Orte liebt; leuchtend weisser Belag, aehnlich einem Kernkeulen-Pilz, der Kellerspinnen ueberzieht. Stadium 5: der Pilz
  steuert den Koerper, die Befallenen trennen die untere Koerperhaelfte ab und kriechen auf den Armen.
  -> fungus_wall (Myzel, Fruchtkoerper, ueberzogene Spinnen an einer feuchten Wand), fungus_floor (befallene Pfuetze,
     offene Flasche, verpilzter Kadaver), fungus_camp (Lager eines Infizierten: nur die Beine sind geblieben, Schleifspur),
     almond_open (offenes Mandelwasser).
- Entity 99 "The Game Master" (Rev. 39): Marionetten-Narrin mit Glasbildschirm als Gesicht (zwei rote X, Cartoon-Grinsen,
  andere Ausdruecke moeglich), Flickenkoerper aus blauem und gelbem Stoff, Glieder aus Holz, quietschende rote Kugeln an
  Muetzenspitzen, Schuhspitzen und Rumpfmitte, rote Faeden an Handgelenken und Knoecheln, die ins Unendliche reichen.
  Level 1: Gemaelde am Boden vor den Hallentueren; eines mit dieser Figur fuehrt nach Level 389.
  -> painting_floor@0..3 (liegende Bilder), painting_jester@0..2 (sie, mit echtem Bildschirm, Kugeln und Faeden;
     Ausdruck 0 Grinsen, 1 verzogen, 2 lachend offen).
- Level 1 (Rev. 83), Erzaehlung: ein dunkles, stilles Wesen kam mit klatschenden Schritten an die Schlafraumtuer und liess
  Krallenspuren und zaehe Fluessigkeit zurueck. -> claw_marks (Wand neben der Tuer), goo_trail (Spur an der Schwelle).
- Garden: nach etwa einer Stunde beginnt die Verwandlung in eine Pflanze. -> plant_wanderer@0..1 (wer zu lange blieb).
Konventionen (common.py): Meter, z oben, Blick entlang +x. Bodenmodelle: Ursprung in der Mitte am Boden. Wandmodelle:
Wandflaeche bei x = 0, Ursprung am Boden. Liegende Bilder: Mitte im Ursprung, Bild nach oben. Flasche: Ursprung unten.
"""
import math
import random

import bmesh  # noqa: F401
from mathutils import Matrix, Vector

import common
from common import material
from models import models_l1 as L1
from models.models_l1_sectors import Kit, blob, catmull, cyl, lathe, leaf, mats_plant, sweep, two_sided

V = Vector
TAU = 2.0 * math.pi


# =====================================================================================================================
# Materialien und kleine Bausteine
# =====================================================================================================================
def mats():
    return {
        'fluff': material('crawler_flaum', (0.95, 0.94, 0.90), look='fabric', tex='plaster_white', rough=1.0, r=1.5),
        'fluff_hi': material('crawler_flaum_hell', (0.99, 0.98, 0.96), look='fabric', rough=1.0, r=1.5),
        'fluff_old': material('crawler_flaum_alt', (0.86, 0.81, 0.67), look='fabric', tex='plaster_white', rough=1.0, r=1.5),
        'thread': material('crawler_faden', (0.90, 0.89, 0.83), look='fabric', rough=0.9, r=1.3),
        'club': material('crawler_keule', (0.95, 0.93, 0.84), look='plastic', rough=0.6, r=1.0),
        'tip': material('crawler_keule_spitze', (0.86, 0.68, 0.38), look='plastic', rough=0.6, r=0.9),
        # feuchter Beton: die Textur der Wand, nur dunkler und etwas gruenstichig (kein flacher Ausschnitt)
        'stain': material('feuchte_wand', (0.80, 0.80, 0.72), look='concrete', tex='concrete_wall_old', rough=0.7, r=0.8),
        'stain_mid': material('feuchte_wand_mittel', (0.64, 0.64, 0.55), look='concrete', tex='concrete_wall_old', rough=0.6, r=0.7),
        'stain_dark': material('feuchte_wand_dunkel', (0.48, 0.48, 0.40), look='concrete', tex='concrete_wall_old', rough=0.5, r=0.6),
        'halo': material('crawler_hof', (0.50, 0.49, 0.44), look='concrete', tex='concrete_floor', rough=0.4, r=0.55),
        'water': material('pfuetze_trueb', (0.15, 0.15, 0.12), look='water', rough=0.1),
        'slime': material('crawler_schleim', (0.72, 0.70, 0.58), look='plastic', rough=0.25, r=0.35),
        'flesh': material('kadaver_haut', (0.30, 0.21, 0.17), look='rubber', rough=0.8, r=1.1),
        'scratch': material('putz_frei', (0.70, 0.68, 0.62), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
    }


def poly_on(bm, pts2, o, u, v):
    """Flaeche aus 2D-Punkten (a, b) -> o + u a + v b; Normale u x v."""
    pts2 = L1.ccw(pts2)
    o, u, v = V(o), V(u), V(v)
    if len(pts2) >= 3:
        bm.faces.new([bm.verts.new(o + u * a + v * b) for (a, b) in pts2])


def blob2(rng, cx, cy, rx, ry, n=16, rough=0.3):
    ph = rng.uniform(0, TAU)
    out = []
    for k in range(n):
        a = TAU * k / n
        f = 1.0 + rough * (0.6 * math.sin(3 * a + ph) + 0.4 * math.sin(5 * a + 2 * ph)) + rng.uniform(-rough, rough) * 0.4
        out.append((cx + rx * f * math.cos(a), cy + ry * f * math.sin(a)))
    return out


def basis(nrm):
    n = V(nrm).normalized()
    u = n.orthogonal().normalized()
    return n, u, n.cross(u)


def tuft(bm, base, d, h, r):
    """Kleiner Flaumkegel (drei Seiten) - der Belag wird struppig."""
    n, u, w = basis(d)
    ring = [bm.verts.new(V(base) + (u * math.cos(a) + w * math.sin(a)) * r) for a in (0.0, TAU / 3, 2 * TAU / 3)]
    tip = bm.verts.new(V(base) + n * h)
    for i in range(3):
        bm.faces.new((ring[i], ring[(i + 1) % 3], tip))


def ellipsoid(bm, c, axis, rl, rr, segs=10, rings=6):
    """Ellipsoid entlang axis (Halbachse rl) mit Radius rr."""
    a = V(axis).normalized()
    prof = [(rr * math.sin(math.pi * i / rings), rl * -math.cos(math.pi * i / rings)) for i in range(rings + 1)]
    prof[0], prof[-1] = (0.0, -rl), (0.0, rl)
    z = V((0, 0, 1))
    q = z.rotation_difference(a).to_matrix().to_4x4()
    lathe(bm, prof, segs, Matrix.Translation(V(c)) @ q)


def mound(k, P, c, nrm, rad, h, rng, mat='fluff', tufts=1.0, squash=1.0):
    """Wattiges Polster (wie der Kernkeulen-Pilz auf Kellerspinnen): viele weiche, sich ueberlappende Kuegelchen,
    oben dichter und heller, dazu Faeden, die vom Rand ins Umland ausfransen."""
    n, u, w = basis(nrm)
    cnt = int(rad * 190 * tufts) + 4
    for i in range(cnt):
        a, rr = rng.uniform(0, TAU), rad * math.sqrt(rng.random()) * 0.9
        dome = max(0.0, 1.0 - (rr / rad) ** 2)
        pr = rad * rng.uniform(0.14, 0.32) * (0.55 + 0.6 * dome)
        p = V(c) + (u * math.cos(a) + w * math.sin(a) * squash) * rr + n * (h * dome * 0.8 + pr * 0.25)
        m_ = 'fluff_hi' if (dome > 0.5 and rng.random() < 0.6) else (mat if rng.random() < 0.8 else 'fluff')
        ellipsoid(k(P[m_], 70), p, n + (u * rng.uniform(-0.3, 0.3) + w * rng.uniform(-0.3, 0.3)), pr * rng.uniform(0.55, 0.8), pr, 6, 3)
    for _ in range(int(rad * 40 * tufts) + 2):  # Faeden fransen vom Rand ins Umland aus
        a = rng.uniform(0, TAU)
        e_ = (u * math.cos(a) + w * math.sin(a) * squash)
        p0 = V(c) + e_ * rad * 0.8 + n * 0.002
        p1 = p0 + e_ * rad * rng.uniform(0.3, 0.9) + (u * rng.uniform(-1, 1) + w * rng.uniform(-1, 1)) * rad * 0.15
        sweep(k(P['thread'], 30), [p0, p0.lerp(p1, 0.5) + n * 0.002, p1], [0.0016, 0.0012, 0.0007], 3, caps=False)


def club(k, P, base, d, length, rng):
    """Fruchtkoerper: duenner, leicht gebogener Stiel mit keulenfoermigem Kopf (ocker an der Spitze)."""
    n, u, w = basis(d)
    bend = (u * rng.uniform(-1, 1) + w * rng.uniform(-1, 1)) * length * 0.25
    pts = catmull([V(base), V(base) + n * length * 0.4 + bend * 0.3, V(base) + n * length * 0.75 + bend * 0.8,
                   V(base) + n * length + bend], 3)
    sweep(k(P['club'], 50), pts, [0.0036 - 0.0014 * i / (len(pts) - 1) for i in range(len(pts))], 6, caps=False)
    tipd = (pts[-1] - pts[-2]).normalized()
    ellipsoid(k(P['club'], 50), pts[-1] + tipd * 0.008, tipd, 0.012, 0.0062 * rng.uniform(0.9, 1.25), 8, 5)
    ellipsoid(k(P['tip'], 50), pts[-1] + tipd * 0.017, tipd, 0.0045, 0.0042, 7, 4)


def veins(k, P, start, nrm, plane_u, plane_w, rng, steps=8, seg=0.04, depth=0, clip=None):
    """Myzelfaeden: verzweigter Zufallsweg auf einer Flaeche."""
    n = V(nrm).normalized()
    a = rng.uniform(0, TAU)
    p = V(start)
    pts = [p.copy()]
    for i in range(steps):
        a += rng.uniform(-0.6, 0.6)
        q = p + (V(plane_u) * math.cos(a) + V(plane_w) * math.sin(a)) * seg * rng.uniform(0.7, 1.2)
        if clip and not clip(q):
            break
        p = q
        pts.append(p + n * 0.0025)
        if depth < 2 and rng.random() < 0.22:
            veins(k, P, p, nrm, plane_u, plane_w, rng, max(2, steps - i - 2), seg * 0.8, depth + 1, clip)
    if len(pts) >= 2:
        r0 = 0.0024 if depth == 0 else 0.0015
        sweep(k(P['thread'], 40), pts, [r0 * (1.0 - 0.55 * i / (len(pts) - 1)) for i in range(len(pts))], 4, caps=False)


def spider(k, P, c, nrm, heading, rng, s=1.0):
    """Ueberzogene Kellerspinne (Wiki: der Pilz aehnelt einem, der Kellerspinnen befaellt): Leib und lange Beine unter
    weissem Flaum, an den Gelenken dicker."""
    n, u, w = basis(nrm)
    hd = (u * math.cos(heading) + w * math.sin(heading)).normalized()
    sd = n.cross(hd).normalized()
    body = V(c) + n * 0.006 * s
    ellipsoid(k(P['fluff_hi'], 40), body - hd * 0.012 * s, hd, 0.014 * s, 0.009 * s, 9, 5)        # Hinterleib
    ellipsoid(k(P['fluff'], 40), body + hd * 0.006 * s, hd, 0.0075 * s, 0.0058 * s, 8, 4)        # Kopfbrust
    for _ in range(9):
        a, rr = rng.uniform(0, TAU), rng.uniform(0.0, 0.012) * s
        tuft(k(P['fluff_hi']), body - hd * 0.012 * s + (hd * math.cos(a) * 0.6 + sd * math.sin(a)) * rr + n * 0.008 * s,
             n + hd * rng.uniform(-0.5, 0.5), rng.uniform(0.006, 0.012) * s, 0.002 * s)
    for side in (-1, 1):
        for i in range(4):
            ang = (-0.9 + 0.6 * i) * side
            out = (hd * math.cos(ang) * 0.9 + sd * side * abs(math.sin(ang) + 0.6)).normalized()
            hip = body + hd * 0.006 * s + out * 0.005 * s
            knee = hip + out * 0.022 * s + n * 0.016 * s
            foot = hip + out * rng.uniform(0.05, 0.065) * s + hd * (0.02 * (1.5 - i)) * s - n * 0.006 * s
            sweep(k(P['fluff'], 40), [hip, knee, foot], [0.0016 * s, 0.0013 * s, 0.0009 * s], 4, caps=True)
            ellipsoid(k(P['fluff_hi'], 40), knee, (knee - hip), 0.0035 * s, 0.0026 * s, 6, 3)


# =====================================================================================================================
# Crawler-Pilz an der Wand (Wandflaeche bei x = 0, Ursprung am Boden)
# =====================================================================================================================
def _fungus_wall(variant):
    rng = random.Random(1701 + variant)
    P = mats()
    k = Kit()
    W, H = ((1.5, 1.25), (0.9, 1.85), (1.4, 1.1))[variant]
    U, Wv, N = V((0, 1, 0)), V((0, 0, 1)), V((1, 0, 0))
    # Feuchtfleck mit dunklem Kern und Laeufern (von unten hochgezogen)
    # Kratzspuren und Strichliste (Variante 2: das Lager - der Infizierte hat gezaehlt und gekratzt)
    keep_out = None
    if variant == 2:
        keep_out = (-0.42, 0.12, 0.5, 1.06)
        for i in range(4):
            y0 = -0.34 + i * 0.045
            pts = [(y0, 1.0), (y0 + 0.03, 0.82), (y0 + 0.07, 0.58)]
            poly_on(k(P['stain_dark']), L1.stroke(pts, 0.012), (0.0022, 0, 0), U, Wv)
            poly_on(k(P['scratch']), L1.stroke([(p[0] + 0.007, p[1]) for p in pts], 0.004), (0.0026, 0, 0), U, Wv)
        for g in range(3):  # Strichliste: je vier Striche und einer quer
            gx = -0.16 + g * 0.1
            for j in range(4):
                poly_on(k(P['scratch']), L1.stroke([(gx + j * 0.016, 0.95), (gx + j * 0.016 + 0.004, 1.04)], 0.0035), (0.0024, 0, 0), U, Wv)
            poly_on(k(P['scratch']), L1.stroke([(gx - 0.01, 0.96), (gx + 0.06, 1.03)], 0.0035), (0.0025, 0, 0), U, Wv)

    def free(y, z):
        return keep_out is None or not (keep_out[0] < y < keep_out[1] and keep_out[2] < z < keep_out[3])

    # Flaumpolster, unten dicht, nach oben lichter; an der Bodenkante ein Wulst
    seeds = []
    for _ in range((14, 11, 12)[variant]):
        y, z = rng.uniform(-0.45, 0.45) * W, H * rng.random() ** 1.7 * 0.9 + 0.04
        if not free(y, z):
            continue
        rad = rng.uniform(0.04, 0.15) * (1.15 - z / H * 0.5)
        mat = 'fluff_old' if rng.random() < 0.25 else ('fluff_hi' if rng.random() < 0.3 else 'fluff')
        mound(k, P, (0.002, y, z), N, rad, rad * rng.uniform(0.18, 0.3), rng, mat)
        seeds.append((y, z, rad))
        for _ in range(rng.randint(2, 4)):  # Ableger: die Kolonie waechst zusammen
            a = rng.uniform(0, TAU)
            yy, zz = y + math.cos(a) * rad * rng.uniform(1.0, 1.7), z + math.sin(a) * rad * rng.uniform(1.0, 1.7)
            if free(yy, zz) and zz > 0.02:
                mound(k, P, (0.002, yy, zz), N, rad * rng.uniform(0.3, 0.5), rad * 0.1, rng, 'fluff', 0.8)
    nb = int(W * 11)
    for i in range(nb):  # dichter Wulst an der Bodenkante, wo das Wasser steht
        y = -0.5 * W + i / max(1, nb - 1) * W + rng.uniform(-0.03, 0.03)
        mound(k, P, (0.03, y, 0.012), V((0.7, 0, 0.71)), rng.uniform(0.05, 0.1), 0.025, rng, rng.choice(['fluff', 'fluff', 'fluff_old']))
    # Myzelfaeden zwischen den Polstern, nach oben ausfransend
    for (y, z, rad) in seeds[: (8, 7, 7)[variant]]:
        veins(k, P, (0.0015, y, z), N, U, Wv, rng, steps=rng.randint(5, 9), seg=0.035,
              clip=lambda q: abs(q.y) < W * 0.56 and 0.0 < q.z < H * 1.05 and free(q.y, q.z))
    # Fruchtkoerper aus den Polstern (schraeg nach oben)
    for (y, z, rad) in seeds:
        for _ in range(rng.randint(1, 3)):
            a = rng.uniform(0, TAU)
            base = V((0.002 + rad * 0.2, y + math.cos(a) * rad * 0.5, z + math.sin(a) * rad * 0.5))
            club(k, P, base, V((1.0, rng.uniform(-0.4, 0.4), rng.uniform(0.2, 0.9))), rng.uniform(0.03, 0.085), rng)
    # ueberzogene Spinnen
    for _ in range((3, 2, 2)[variant]):
        y, z = rng.uniform(-0.38, 0.38) * W, H * rng.uniform(0.35, 0.85)
        if free(y, z):
            spider(k, P, (0.0015, y, z), N, rng.uniform(0, TAU), rng, rng.uniform(0.9, 1.25))
    k.finish()


def build_fungus_wall():
    _fungus_wall(0)


def build_fungus_wall__v1():
    _fungus_wall(1)


def build_fungus_wall__v2():
    _fungus_wall(2)


# =====================================================================================================================
# Befallene Pfuetze, offene Flasche darin, verpilzter Kadaver (Boden, Ursprung Mitte)
# =====================================================================================================================
def _bottle(k, M, open_=True, fluff=True, P=None, rng=None):
    """Mandelwasserflasche wie im Spiel (render/meshes.cpp: makeBottle), ohne Deckel; M setzt sie hin."""
    glass = material('flasche_trueb', (0.74, 0.77, 0.69), look='glass', rough=0.15, r=0.5)
    label = material('etikett_mandel', (0.86, 0.74, 0.52), look='cardboard', rough=0.8, r=1.2)
    stripe = material('etikett_streifen', (0.35, 0.22, 0.12), look='cardboard', rough=0.8, r=1.2)
    inner = material('flasche_innen', (0.10, 0.11, 0.09), look='plastic', rough=0.4)
    prof = [(0.0, 0.0), (0.046, 0.0), (0.052, 0.006), (0.054, 0.02), (0.054, 0.17), (0.05, 0.19), (0.034, 0.215), (0.02, 0.226),
            (0.02, 0.228), (0.023, 0.23), (0.023, 0.258), (0.02, 0.262)]
    lathe(k(glass, 40), prof, 18, M)
    if open_:
        lathe(k(inner, 40), [(0.0175, 0.262), (0.0172, 0.236), (0.0, 0.236)], 14, M)
    lathe(k(label, 40), [(0.0546, 0.07), (0.0546, 0.15)], 18, M)
    lathe(k(stripe, 40), [(0.0549, 0.102), (0.0549, 0.116)], 18, M)
    if fluff and P is not None:  # Flaum am offenen Hals
        for i in range(9):
            a = TAU * i / 9 + rng.uniform(-0.2, 0.2)
            p = M @ V((math.cos(a) * 0.021, math.sin(a) * 0.021, 0.259))
            d = (M.to_3x3() @ V((math.cos(a), math.sin(a), 1.2))).normalized()
            tuft(k(P['fluff_hi']), p, d, rng.uniform(0.006, 0.014), 0.003)
        for _ in range(4):
            a = rng.uniform(0, TAU)
            p = M @ V((math.cos(a) * 0.0545, math.sin(a) * 0.0545, rng.uniform(0.02, 0.06)))
            blob(k(P['fluff'], 30), p, M.to_3x3() @ V((math.cos(a), math.sin(a), 0)), rng.uniform(0.006, 0.012), 0.003, rng, 7)


def _puddle(k, P, rng, rx, ry, rafts=6):
    pts = blob2(rng, 0.0, 0.0, rx, ry, 26, 0.3)
    for _ in range(rafts):  # treibende Myzelflocken auf dem Wasser
        a, r = rng.uniform(0, TAU), rng.uniform(0.1, 0.75)
        c = V((math.cos(a) * rx * r, math.sin(a) * ry * r, 0.0026))
        poly_on(k(P['slime']), blob2(rng, c.x, c.y, rng.uniform(0.02, 0.06), rng.uniform(0.015, 0.04), 10, 0.4), (0, 0, 0.0028),
                V((1, 0, 0)), V((0, 1, 0)))
        if rng.random() < 0.6:
            mound(k, P, c + V((0, 0, 0.001)), V((0, 0, 1)), rng.uniform(0.012, 0.03), 0.006, rng, 'fluff_hi', 0.6)
    return pts


def _fungus_floor(variant):
    rng = random.Random(1801 + variant)
    P = mats()
    k = Kit()
    Z = V((0, 0, 1))
    if variant < 2:
        rx, ry = (0.62, 0.45) if variant == 0 else (0.5, 0.38)
        _puddle(k, P, rng, rx, ry, 7)
        # Rand der Pfuetze: dichter Flaum, Faeden ueber den Beton, Fruchtkoerper in Gruppen
        for i in range(16):
            a = TAU * i / 16 + rng.uniform(-0.15, 0.15)
            f = rng.uniform(0.92, 1.12)
            c = V((math.cos(a) * rx * f, math.sin(a) * ry * f, 0.002))
            rad = rng.uniform(0.04, 0.1)
            mound(k, P, c, Z, rad, rad * 0.3, rng, rng.choice(['fluff', 'fluff', 'fluff_hi', 'fluff_old']))
            if rng.random() < 0.6:
                for _ in range(rng.randint(2, 5)):
                    b = c + V((rng.uniform(-1, 1) * rad * 0.5, rng.uniform(-1, 1) * rad * 0.5, rad * 0.2))
                    club(k, P, b, V((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 1.0)), rng.uniform(0.03, 0.1), rng)
            if rng.random() < 0.45:
                veins(k, P, c, Z, V((1, 0, 0)), V((0, 1, 0)), rng, rng.randint(4, 8), 0.05,
                      clip=lambda q: (q.x / (rx * 1.9)) ** 2 + (q.y / (ry * 1.9)) ** 2 < 1.0)
        if variant == 1:  # offene Flasche liegt im Wasser, aus dem Hals waechst es
            M = Matrix.Translation(V((0.1, -0.05, 0.054))) @ Matrix.Rotation(1.5, 4, 'Z') @ Matrix.Rotation(math.pi / 2 - 0.04, 4, 'Y') \
                @ Matrix.Translation(V((0, 0, -0.13)))
            _bottle(k, M, True, True, P, rng)
    else:
        # verpilzter Kadaver einer Ratte (Wiki: kleine Tiere tragen den Pilz weiter), dunkler Hof, Fruchtkoerper
        grime = material('kadaver_fleck', (0.55, 0.50, 0.42), look='paint', rough=0.6, r=0.7)
        grime2 = material('kadaver_fleck_dunkel', (0.38, 0.32, 0.25), look='paint', rough=0.5, r=0.6)
        poly_on(k(grime), blob2(rng, 0.0, 0.0, 0.16, 0.1, 20, 0.35), (0, 0, 0.0008), V((1, 0, 0)), V((0, 1, 0)))
        poly_on(k(grime2), blob2(rng, -0.02, 0.0, 0.1, 0.06, 16, 0.3), (0, 0, 0.0011), V((1, 0, 0)), V((0, 1, 0)))
        ellipsoid(k(P['fluff_old'], 40), (0.0, 0.0, 0.034), (1, 0.15, 0), 0.085, 0.036, 12, 7)
        ellipsoid(k(P['fluff'], 40), (0.095, 0.015, 0.026), (1, 0.25, -0.2), 0.04, 0.022, 10, 6)  # Kopf
        sweep(k(P['flesh'], 40), catmull([(-0.08, -0.01, 0.02), (-0.16, -0.05, 0.008), (-0.25, -0.02, 0.004), (-0.31, 0.05, 0.003)], 3),
              [0.008, 0.006, 0.004, 0.0025, 0.002, 0.0015, 0.0012, 0.001, 0.0009, 0.0008], 5)
        for (x, y) in ((0.05, 0.035), (0.05, -0.035), (-0.05, 0.038), (-0.05, -0.036)):  # steif abstehende Beine
            sweep(k(P['flesh'], 40), [(x, y * 0.6, 0.02), (x + 0.01, y * 1.6, 0.012), (x + 0.02, y * 2.0, 0.004)], [0.006, 0.004, 0.003], 5)
        for _ in range(120):
            a, b = rng.uniform(0, TAU), rng.uniform(-1, 1)
            p = V((b * 0.08, math.cos(a) * 0.034 * math.sqrt(1 - b * b), 0.034 + math.sin(a) * 0.034 * math.sqrt(1 - b * b)))
            if p.z > 0.01:
                tuft(k(P['fluff_hi']), p, (p - V((b * 0.08, 0, 0.034))).normalized() + V((0, 0, 0.4)), rng.uniform(0.006, 0.016), 0.0022)
        for _ in range(9):  # Keulen aus dem Ruecken
            b = V((rng.uniform(-0.06, 0.06), rng.uniform(-0.015, 0.015), 0.066))
            club(k, P, b, V((rng.uniform(-0.4, 0.4), rng.uniform(-0.5, 0.5), 1.0)), rng.uniform(0.05, 0.11), rng)
        for i in range(10):
            a = TAU * i / 10
            mound(k, P, (math.cos(a) * 0.15, math.sin(a) * 0.1, 0.001), Z, rng.uniform(0.03, 0.06), 0.012, rng, 'fluff')
        for _ in range(5):
            veins(k, P, (rng.uniform(-0.1, 0.1), rng.uniform(-0.06, 0.06), 0.001), Z, V((1, 0, 0)), V((0, 1, 0)), rng, 7, 0.05)
        # eine ueberzogene Schabe daneben
        ellipsoid(k(P['fluff_hi'], 40), (0.22, 0.12, 0.008), (1, 0.4, 0), 0.016, 0.008, 8, 4)
    k.finish()


def build_fungus_floor():
    _fungus_floor(0)


def build_fungus_floor__v1():
    _fungus_floor(1)


def build_fungus_floor__v2():
    _fungus_floor(2)


# =====================================================================================================================
# Lager eines Infizierten (Boden; Wand hinter dem Lager bei x = -0.95, Schlafsack laengs y)
# =====================================================================================================================
def build_fungus_camp():
    rng = random.Random(1901)
    P = mats()
    k = Kit()
    Z = V((0, 0, 1))
    bag = material('schlafsack', (0.27, 0.30, 0.20), look='fabric', rough=0.9, r=1.3)
    bag_in = material('schlafsack_futter', (0.46, 0.18, 0.12), look='fabric', rough=0.9, r=1.3)
    pants = material('hose_grau', (0.22, 0.24, 0.27), look='fabric', rough=0.9, r=1.35)
    boot = material('stiefel', (0.20, 0.14, 0.10), look='rubber', rough=0.7, r=1.0)
    pack = material('rucksack', (0.36, 0.23, 0.15), look='fabric', rough=0.9, r=1.3)
    paper = material('papier', (0.84, 0.81, 0.71), look='cardboard', rough=0.9, r=1.3)
    smear = material('schleifspur', (0.16, 0.11, 0.09), look='paint', rough=0.5, r=0.75)
    wax = material('kerze', (0.86, 0.82, 0.70), look='plastic', rough=0.6, r=1.0)
    # Schlafsack: zerknuellter Schlauch laengs y, oben aufgeschlagen
    ring_pts = [V((-0.35 + 0.03 * math.sin(i * 1.7), -0.85 + i * 0.18, 0.0)) for i in range(11)]
    rad = [0.075 + 0.02 * math.sin(i * 2.3) + (0.02 if i > 6 else 0.0) for i in range(11)]
    sweep(k(bag, 35), [p + V((0, 0, r * 0.9)) for p, r in zip(ring_pts, rad)], rad, 12, caps=True,
          profile=[(math.cos(TAU * j / 12) * 0.9, math.sin(TAU * j / 12) * 3.6) for j in range(12)], up=Z)
    two_sided(k(bag_in, 0), [[V((-0.35 + 0.17, -0.85, 0.02)), V((-0.35 + 0.38, -0.8, 0.012)), V((-0.35 + 0.36, -0.1, 0.01)),
                               V((-0.35 + 0.17, -0.12, 0.05))]])
    # nur die Beine sind geblieben (Wiki, Stadium 5: die Befallenen trennen die untere Haelfte ab und kriechen fort)
    for side in (-1, 1):
        hip = V((-0.33 + side * 0.09, -0.12, 0.16))
        knee = V((-0.33 + side * 0.1, 0.32, 0.17 + 0.02 * side))
        ankle = V((-0.33 + side * 0.11, 0.74, 0.1))
        sweep(k(pants, 40), catmull([hip, knee, ankle], 3), [0.075, 0.067, 0.072, 0.063, 0.057, 0.061, 0.05], 10)
        toe = ankle + V((side * 0.02, 0.17, -0.05))
        sweep(k(boot, 40), [ankle + V((0, -0.02, 0.03)), ankle + V((0, 0.03, -0.02)), toe], [0.058, 0.06, 0.042], 10)
        sole = material('sohle', (0.45, 0.40, 0.33), look='rubber', rough=0.9, r=1.2)
        ellipsoid(k(sole, 30), ankle + V((side * 0.01, 0.1, -0.075)), (0, 1, 0), 0.13, 0.045, 10, 4)
        for j in range(4):  # Schnuersenkel
            q = ankle + V((0, -0.01 + j * 0.03, 0.035 - j * 0.012))
            cyl(k(material('senkel', (0.55, 0.50, 0.40), look='fabric', rough=0.9), 30), q + V((-0.03, 0, 0)), q + V((0.03, 0, 0)), 0.004, 4)
    # an der Taille: der Pilz - ein Polster aus Flaum und Keulen, wo der Rumpf sein muesste
    for i in range(10):
        a = TAU * i / 10
        c = V((-0.33 + math.cos(a) * 0.12, -0.17 + math.sin(a) * 0.08, 0.13 + rng.uniform(-0.03, 0.05)))
        mound(k, P, c, (c - V((-0.33, -0.17, 0.0))).normalized(), rng.uniform(0.05, 0.09), 0.03, rng,
              rng.choice(['fluff_hi', 'fluff', 'fluff_old']), 1.3)
    mound(k, P, (-0.33, -0.2, 0.2), Z, 0.14, 0.07, rng, 'fluff_hi', 1.4)
    for _ in range(14):
        b = V((-0.33 + rng.uniform(-0.1, 0.1), -0.2 + rng.uniform(-0.08, 0.08), 0.24))
        club(k, P, b, V((rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5), 1.0)), rng.uniform(0.05, 0.12), rng)
    # Schleifspur: vom Lager weg in den Raum, zwei Hand-Spuren und ein breiter Streifen, Flaumflocken darin
    path = [V((-0.3, -0.35, 0)), V((-0.05, -0.6, 0)), V((0.35, -0.8, 0)), V((0.85, -0.9, 0)), V((1.3, -1.0, 0))]
    for off, wdt in ((0.0, 0.16), (-0.17, 0.035), (0.17, 0.035)):
        q = catmull(path, 3)
        side = [(q[min(i + 1, len(q) - 1)] - q[max(i - 1, 0)]).normalized().cross(Z) for i in range(len(q))]
        st = [((p + s * off).x, (p + s * off).y) for p, s in zip(q, side)]
        ww = [wdt * (1.0 - 0.6 * i / (len(q) - 1)) for i in range(len(q))]
        left = [(x + s.x * w * 0.5, y + s.y * w * 0.5) for (x, y), s, w in zip(st, side, ww)]
        right = [(x - s.x * w * 0.5, y - s.y * w * 0.5) for (x, y), s, w in zip(st, side, ww)]
        poly_on(k(smear), left + list(reversed(right)), (0, 0, 0.0012 + 0.0002 * (off != 0)), V((1, 0, 0)), V((0, 1, 0)))
    q = catmull(path, 3)
    for i in range(2, len(q) - 1, 2):
        t = (q[i + 1] - q[i - 1]).normalized()
        sd = t.cross(Z)
        for s_ in (-1, 1):
            if rng.random() < 0.25:
                continue
            c = q[i] + sd * s_ * 0.2 + t * rng.uniform(-0.05, 0.05)
            ang = math.atan2(t.y, t.x) + s_ * 0.35 + math.pi
            palm = L1.rot2(blob2(rng, 0.0, 0.0, 0.05, 0.042, 12, 0.15), ang)
            poly_on(k(smear), [(c.x + px, c.y + py) for (px, py) in palm], (0, 0, 0.0016), V((1, 0, 0)), V((0, 1, 0)))
            for f in range(4):  # Finger, zum Lager hin gespreizt
                fa = ang + (-0.45 + 0.3 * f) * s_
                b0 = c + V((math.cos(fa), math.sin(fa), 0)) * 0.045
                b1 = b0 + V((math.cos(fa), math.sin(fa), 0)) * (0.05 if f in (1, 2) else 0.04)
                poly_on(k(smear), L1.stroke([(b0.x, b0.y), (b1.x, b1.y)], 0.016), (0, 0, 0.0017), V((1, 0, 0)), V((0, 1, 0)))
    for _ in range(14):
        p = catmull(path, 3)[rng.randint(0, 12)]
        mound(k, P, p + V((rng.uniform(-0.06, 0.06), rng.uniform(-0.06, 0.06), 0.001)), Z, rng.uniform(0.01, 0.025), 0.005, rng, 'fluff', 0.5)
    # Rucksack an der Wand, offene Flaschen, Deckel, Zettel, Kerzenstummel
    M = Matrix.Translation(V((-0.78, -0.62, 0.0))) @ Matrix.Rotation(0.3, 4, 'Z')
    from models.models_l1_sectors import cube
    cube(k(pack, 0), (-0.12, -0.17, 0.0), (0.12, 0.17, 0.42), M, bevel=0.03)
    cube(k(pack, 0), (0.12, -0.12, 0.05), (0.18, 0.12, 0.3), M, bevel=0.02)
    _bottle(k, Matrix.Translation(V((0.12, 0.45, 0.0))), True, True, P, rng)
    _bottle(k, Matrix.Translation(V((0.25, 0.28, 0.054))) @ Matrix.Rotation(2.2, 4, 'Z') @ Matrix.Rotation(math.pi / 2 - 0.05, 4, 'Y')
            @ Matrix.Translation(V((0, 0, -0.13))), True, False, P, rng)
    lathe(k(material('deckel_blau', (0.25, 0.45, 0.85), look='plastic', rough=0.5), 30), [(0.0, 0.0), (0.024, 0.0), (0.024, 0.028), (0.0, 0.028)],
          12, Matrix.Translation(V((0.4, 0.62, 0.0))))
    for (x, y, a, w, h) in ((0.3, -0.25, 0.4, 0.21, 0.29), (0.45, -0.05, -0.3, 0.15, 0.21), (0.18, 0.05, 1.2, 0.21, 0.29)):
        q = L1.rot2(L1.rect(-w / 2, -h / 2, w / 2, h / 2), a)
        poly_on(k(paper), [(x + px, y + py) for (px, py) in q], (0, 0, 0.0015), V((1, 0, 0)), V((0, 1, 0)))
    mound(k, P, (0.31, -0.24, 0.002), Z, 0.03, 0.006, rng, 'fluff', 0.6)
    lathe(k(wax, 30), [(0.0, 0.0), (0.02, 0.0), (0.02, 0.035), (0.016, 0.04), (0.0, 0.04)], 10, Matrix.Translation(V((-0.7, 0.3, 0.0))))
    k.finish()


# =====================================================================================================================
# Spuren an der Zimmertuer: Krallen an der Wand (Wandflaeche x = 0), zaehe Fluessigkeit, Spur auf dem Boden
# =====================================================================================================================
def _goo():
    return material('zaeher_schleim', (0.05, 0.06, 0.045), look='plastic', rough=0.15, r=0.28)


def build_claw_marks():
    rng = random.Random(2001)
    P = mats()
    k = Kit()
    goo = _goo()
    dark = material('kratzer_tief', (0.07, 0.065, 0.06), look='paint', rough=0.9, r=1.4)
    U, Wv = V((0, 1, 0)), V((0, 0, 1))
    sets = [(-0.05, 1.95, 0.95, 0.18), (0.18, 1.5, 0.62, 0.12), (-0.25, 2.05, 1.5, -0.1)]
    for si, (y0, z0, z1, lean) in enumerate(sets):
        for j in range(4):
            yy = y0 + j * 0.05 + rng.uniform(-0.006, 0.006)
            n = 7
            pts = [(yy + lean * (i / (n - 1)) + 0.012 * math.sin(i * 1.3 + j), z0 - (z0 - z1) * i / (n - 1) * rng.uniform(0.95, 1.0))
                   for i in range(n)]
            wdt = [0.024 - 0.016 * i / (n - 1) for i in range(n)]
            L, Rr = [], []
            for i, (y, z) in enumerate(pts):
                d = V((pts[min(i + 1, n - 1)][0] - pts[max(i - 1, 0)][0], pts[min(i + 1, n - 1)][1] - pts[max(i - 1, 0)][1], 0))
                s = V((-d.y, d.x, 0)).normalized() * wdt[i] * 0.5
                L.append((y + s.x, z + s.y))
                Rr.append((y - s.x, z - s.y))
            poly_on(k(P['scratch']), L + list(reversed(Rr)), (0.0014, 0, 0), U, Wv)
            poly_on(k(dark), L1.stroke(pts, 0.006), (0.0018, 0, 0), U, Wv)
            # aufgerissene Raender: helle, gezackte Streifen beidseits
            for side in (L, Rr):
                jag = [(y + rng.uniform(-0.003, 0.003), z) for (y, z) in side]
                poly_on(k(P['scratch']), L1.stroke(jag, 0.0065), (0.0022, 0, 0), U, Wv)
            for _ in range(3):  # abgeplatzte Brocken
                y, z = pts[rng.randint(0, n - 1)]
                b = rng.uniform(0.004, 0.009)
                from models.models_l1_sectors import blk
                blk(k(P['scratch'], 0), (b, b * 1.3, b), (0.004, y + rng.uniform(-0.01, 0.01), z), rx=rng.uniform(0, 3), rz=rng.uniform(0, 3))
        # zaehe Fluessigkeit entlang der Krallen, mit Laeufern und tropfenfoermigen Enden
        if si < 2:
            ym = y0 + 0.075 + lean * 0.4
            zm = (z0 + z1) * 0.5
            poly_on(k(goo), blob2(rng, ym, zm, 0.09, 0.2, 18, 0.45), (0.0026, 0, 0), U, Wv)
            for _ in range(5):
                y = ym + rng.uniform(-0.07, 0.07)
                ztop = zm + rng.uniform(-0.1, 0.05)
                zb = ztop - rng.uniform(0.15, 0.55)
                pts3 = [V((0.004, y + rng.uniform(-0.004, 0.004), ztop - (ztop - zb) * t)) for t in (0.0, 0.3, 0.6, 0.85, 1.0)]
                sweep(k(goo, 45), pts3, [0.005, 0.0042, 0.0038, 0.0042, 0.0055], 6, caps=True)
                ellipsoid(k(goo, 45), pts3[-1] + V((0.001, 0, -0.004)), (0, 0, 1), 0.009, 0.0068, 8, 4)
    # Putzbrocken und eine Lache Schleim unten an der Wand
    from models.models_l1_sectors import blk
    for _ in range(14):
        b = rng.uniform(0.006, 0.018)
        blk(k(P['scratch'], 0), (b, b * 1.2, b * 0.6), (rng.uniform(0.04, 0.3), rng.uniform(-0.3, 0.4), b * 0.3), rz=rng.uniform(0, 3), bevel=0.001)
    blob(k(goo, 45), (0.1, 0.05, 0.0), (0, 0, 1), 0.14, 0.008, rng, 13, 1.0, 0.45)
    blob(k(goo, 45), (0.03, 0.1, 0.0), (0.4, 0, 1), 0.07, 0.01, rng, 9, 1.0, 0.4)
    k.finish()


def build_goo_trail():
    """Spur zaeher Fluessigkeit laengs x: breite, flache Abdruecke mit drei Krallen, dazwischen Faeden und Tropfen."""
    rng = random.Random(2101)
    k = Kit()
    goo = _goo()
    Z = V((0, 0, 1))
    blob(k(goo, 45), (-0.85, 0.0, 0.0), Z, 0.2, 0.012, rng, 15, 0.7, 0.4)
    for i in range(5):
        x = -0.55 + i * 0.33
        y = (0.09 if i % 2 else -0.09) + rng.uniform(-0.02, 0.02)
        a = rng.uniform(-0.15, 0.15)
        pad = L1.rot2(blob2(rng, 0.0, 0.0, 0.09, 0.05, 14, 0.2), a)
        poly_on(k(goo), [(x + px, y + py) for (px, py) in pad], (0, 0, 0.0012), V((1, 0, 0)), V((0, 1, 0)))
        blob(k(goo, 45), (x, y, 0.001), Z, 0.05, 0.004, rng, 9, 0.6, 0.3)
        for c in (-1, 0, 1):  # drei Krallen voraus
            ca = a + c * 0.35
            base = V((x + math.cos(ca) * 0.085, y + math.sin(ca) * 0.05, 0.0))
            tipp = base + V((math.cos(ca), math.sin(ca), 0)) * rng.uniform(0.05, 0.07)
            sd = V((-math.sin(ca), math.cos(ca), 0)) * 0.008
            poly_on(k(goo), [(base.x - sd.x, base.y - sd.y), (base.x + sd.x, base.y + sd.y), (tipp.x, tipp.y)], (0, 0, 0.0014),
                    V((1, 0, 0)), V((0, 1, 0)))
        for _ in range(4):
            q = V((x + rng.uniform(-0.2, 0.2), y + rng.uniform(-0.12, 0.12), 0.0))
            blob(k(goo, 45), q, Z, rng.uniform(0.008, 0.02), 0.003, rng, 7, 1.0, 0.3)
    for _ in range(6):  # gezogene Faeden
        x0 = rng.uniform(-0.8, 0.6)
        pts = [V((x0 + t * 0.25, rng.uniform(-0.03, 0.03) + 0.05 * math.sin(t * 3 + x0), 0.0015)) for t in (0.0, 0.33, 0.66, 1.0)]
        sweep(k(goo, 40), pts, [0.003, 0.0018, 0.0015, 0.002], 4, caps=True)
    k.finish()


# =====================================================================================================================
# Garden: wer zu lange blieb (Boden, Ursprung Mitte)
# =====================================================================================================================
def _plant_wanderer(variant):
    rng = random.Random(2201 + variant)
    P = mats_plant()
    k = Kit()
    Z = V((0, 0, 1))
    bark, bark_d, root = P['bark'], material('rinde_kopf', (0.15, 0.12, 0.09), look='wood', tex='concrete_rough', rough=0.95, r=1.5), P['root']
    cloth = material('jacke_verblasst', (0.46, 0.20, 0.16), look='fabric', rough=0.9, r=1.35)
    shoe = material('schuh_alt', (0.18, 0.15, 0.13), look='rubber', rough=0.8, r=1.1)
    hollow = material('rinde_hoehle', (0.05, 0.04, 0.03), look='wood', rough=1.0, r=1.5)
    leaves = []

    def limb(pts, r0, r1, mat=bark, segs=9):
        q = catmull([V(p) for p in pts], 3)
        rr = [r0 + (r1 - r0) * i / (len(q) - 1) for i in range(len(q))]
        rr = [r * (1.0 + 0.12 * math.sin(i * 2.1 + r0 * 50)) for i, r in enumerate(rr)]
        sweep(k(mat, 45), q, rr, segs)
        return q

    def twig(p, d, length, depth):
        d = V(d).normalized()
        pts = [V(p)]
        for i in range(4):
            d = (d + V((rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), rng.uniform(-0.05, 0.25)))).normalized()
            pts.append(pts[-1] + d * length / 4)
        r = 0.018 * (0.6 ** depth)
        sweep(k(P['twig'] if depth else bark, 40), pts, [r, r * 0.8, r * 0.6, r * 0.45, r * 0.3], 6)
        for i in range(1, 5):
            for _ in range(2 if depth else 1):
                leaves.append((pts[i], d))
        if depth < 2:
            for i in (2, 3):
                twig(pts[i], d + V((rng.uniform(-0.8, 0.8), rng.uniform(-0.8, 0.8), 0.3)), length * 0.6, depth + 1)

    if variant == 0:
        # kniend, Kopf gesenkt; der rechte Arm ist zum Ast geworden, der linke wurzelt im Boden
        hip = V((0.0, 0.0, 0.5))
        neck = V((0.14, 0.0, 1.02))
        limb([hip, V((0.05, 0.0, 0.75)), neck], 0.15, 0.11)
        limb([neck, V((0.2, 0.0, 1.08))], 0.06, 0.055)
        limb([V((0.08, -0.21, 0.93)), V((0.13, 0.0, 1.0)), V((0.08, 0.21, 0.93))], 0.075, 0.07)  # Schultern
        head = V((0.27, 0.0, 1.11))
        ellipsoid(k(bark_d, 45), head, (0.6, 0, -0.8), 0.135, 0.105, 12, 7)
        for side in (-1, 1):
            limb([hip + V((0, side * 0.1, 0)), V((0.3, side * 0.12, 0.13)), V((-0.1, side * 0.13, 0.06)), V((-0.32, side * 0.14, 0.05))], 0.085, 0.06)
        sh_r, sh_l = V((0.1, -0.2, 0.95)), V((0.1, 0.2, 0.95))
        arm = limb([sh_r, V((0.22, -0.32, 1.25)), V((0.25, -0.35, 1.6)), V((0.2, -0.38, 1.85))], 0.055, 0.035)
        twig(arm[-1], (0.1, -0.2, 1.0), 0.6, 0)
        twig(arm[-4], (0.6, -0.5, 0.6), 0.45, 1)
        limb([sh_l, V((0.3, 0.32, 0.6)), V((0.42, 0.34, 0.2)), V((0.5, 0.36, 0.03))], 0.05, 0.04)
        roots_from = [V((0.5, 0.36, 0.02)), V((-0.3, -0.14, 0.03)), V((-0.3, 0.14, 0.03)), V((0.3, -0.12, 0.05)), V((0.3, 0.12, 0.05))]
        shoes = [(V((-0.38, -0.14, 0.04)), -1), (V((-0.38, 0.14, 0.04)), 1)]
        face_c, face_d = head + V((0.08, 0, -0.06)), V((0.75, 0, -0.65))
    else:
        # stehend, die Beine zu einem Stamm verwachsen, beide Arme zur Krone erhoben, Kopf im Nacken, offener Mund
        trunk = limb([V((0, 0, 0.0)), V((0.02, 0.0, 0.5)), V((0.0, 0.03, 0.95)), V((-0.03, 0.0, 1.35))], 0.17, 0.12)
        neck = trunk[-1]
        head = neck + V((-0.08, 0.0, 0.16))
        ellipsoid(k(bark_d, 45), head, (-0.5, 0, 0.85), 0.135, 0.105, 12, 7)
        for side in (-1, 1):
            arm = limb([neck + V((0, side * 0.18, -0.05)), neck + V((0.05, side * 0.35, 0.25)), neck + V((0.0, side * 0.42, 0.6)),
                        neck + V((-0.05, side * 0.4, 0.85))], 0.06, 0.035)
            twig(arm[-1], (0.0, side * 0.3, 1.0), 0.65, 0)
            twig(arm[-5], (0.3, side * 0.8, 0.5), 0.5, 1)
        twig(head + V((0.06, 0, 0.0)), (0.5, 0.1, 0.8), 0.35, 1)  # Trieb aus dem Mund
        roots_from = [V((math.cos(a) * 0.12, math.sin(a) * 0.12, 0.08)) for a in (0.3, 1.5, 2.7, 3.9, 5.1)]
        shoes = [(V((0.18, -0.1, 0.04)), -1)]
        face_c, face_d = head + V((0.07, 0, 0.03)), V((0.85, 0, 0.5))
    # Kopf: Knorren und Aststummel in der Rinde, Moos, Efeu quer uebers Gesicht (kein glattes Ei)
    for _ in range(9):
        q = V((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))).normalized()
        ellipsoid(k(bark_d, 45), head + q * 0.105, q, rng.uniform(0.018, 0.03), rng.uniform(0.022, 0.04), 7, 4)
    for s_ in (-1, 1):  # Aststummel seitlich und nach hinten (nicht nach oben - das sah aus wie Ohren)
        q = V((rng.uniform(-0.9, -0.4), s_ * rng.uniform(0.6, 1.0), rng.uniform(-0.3, 0.15))).normalized()
        sweep(k(P['twig'], 40), [head + q * 0.1, head + q * 0.15, head + q * 0.19 + V((0, 0, -0.02))], [0.011, 0.007, 0.004], 5)
    blob(k(P['moss'], 0), head + V((0, 0, 0.12)), (0, 0, 1), 0.06, 0.012, rng, 9)
    ivy = [head + V((math.cos(a) * 0.125, math.sin(a) * 0.125, 0.05 - 0.09 * (a + 1.2) / 2.4)) for a in [-1.2 + 2.4 * i / 8 for i in range(9)]]
    sweep(k(P['stem'], 40), ivy, 0.006, 5)
    for p in ivy[1::2]:
        leaves.append((p, V((rng.uniform(-1, 1), rng.uniform(-1, 1), 0.3))))
    # Gesicht: Augenhoehlen und Mund in der Rinde
    fd = face_d.normalized()
    side = fd.cross(Z).normalized()
    up = side.cross(fd).normalized()
    for s_ in (-1, 1):
        ellipsoid(k(hollow, 30), face_c + side * 0.035 * s_ + up * 0.035 + fd * 0.012, fd, 0.012, 0.019, 8, 4)
    ellipsoid(k(hollow, 30), face_c - up * 0.035 + fd * 0.01, fd, 0.012, 0.022 if variant == 0 else 0.03, 8, 4)
    # Wurzeln ueber den Boden, Risse im Estrich
    for p in roots_from:
        for _ in range(2):
            a = rng.uniform(0, TAU)
            pts = [p]
            for i in range(5):
                a += rng.uniform(-0.5, 0.5)
                pts.append(pts[-1] + V((math.cos(a) * 0.12, math.sin(a) * 0.12, 0)) * rng.uniform(0.8, 1.3))
                pts[-1].z = max(0.004, pts[-1].z * 0.5)
            sweep(k(root, 40), catmull(pts, 2), [0.03 * (1 - 0.85 * i / 10) for i in range(11)], 6)
            crack = [(q.x + rng.uniform(-0.01, 0.01), q.y + rng.uniform(-0.01, 0.01)) for q in pts[2:]]
            if len(crack) >= 2:
                poly_on(k(P['soil']), L1.stroke(crack, 0.03), (0, 0, 0.0012), V((1, 0, 0)), V((0, 1, 0)))
    # Reste: ein Schuh, den die Wurzeln sprengen; Fetzen einer Jacke an den Schultern
    for (p, s_) in shoes:
        sweep(k(shoe, 40), [p + V((-0.05, 0, 0.0)), p + V((0.07, 0, -0.005)), p + V((0.13, 0.0, -0.02))], [0.045, 0.05, 0.035], 9)
    for _ in range(4):
        c = (neck if variant == 0 else neck) + V((rng.uniform(-0.1, 0.1), rng.uniform(-0.2, 0.2), rng.uniform(-0.35, -0.05)))
        d = V((rng.uniform(-0.2, 0.2), rng.uniform(-0.2, 0.2), -1)).normalized()
        sd = V((0, 1, 0)).cross(d).normalized() * 0.06
        two_sided(k(cloth, 0), [[c - sd, c + sd, c + sd * 0.4 + d * rng.uniform(0.12, 0.25), c - sd * 0.6 + d * rng.uniform(0.15, 0.3)]])
    # Efeu windet sich um den Leib
    for t in range(2):
        pts = [V((math.cos(i * 0.9 + t * 3) * 0.16, math.sin(i * 0.9 + t * 3) * 0.16, 0.1 + i * 0.09)) +
               (V((0.08 * i / 10, 0, 0)) if variant == 0 else V((0, 0, 0))) for i in range(11)]
        sweep(k(P['stem'], 40), pts, 0.006, 5)
        for p in pts[1::2]:
            leaves.append((p, V((math.cos(rng.uniform(0, TAU)), math.sin(rng.uniform(0, TAU)), 0.4))))
    for (p, d) in leaves:
        dd = (V(d) + V((rng.uniform(-0.9, 0.9), rng.uniform(-0.9, 0.9), rng.uniform(-0.2, 0.5)))).normalized()
        mat = P[random.Random(int(p.x * 1e4 + p.y * 1e3)).choice(['leaf_dark', 'leaf_mid', 'leaf_mid', 'leaf_light', 'leaf_yellow'])]
        leaf(k(mat, 0), p, dd, Z, rng.uniform(0.06, 0.1), rng.uniform(0.035, 0.05), rng, droop=0.5)
    k.finish()


def build_plant_wanderer():
    _plant_wanderer(0)


def build_plant_wanderer__v1():
    _plant_wanderer(1)


# =====================================================================================================================
# Gemaelde am Boden (liegend) und das Bild der Spielleiterin (Entity 99)
# =====================================================================================================================
def build_painting_floor():
    L1._painting(0.9, 0.6, 51, 'landschaft:haus', 'gold', flat=True)


def build_painting_floor__v1():
    L1._painting(0.6, 0.75, 52, 'stillleben', 'dunkel', flat=True)


def build_painting_floor__v2():
    L1._painting(0.8, 0.6, 53, 'meer:abend', 'eiche', flat=True)


def build_painting_floor__v3():
    L1._painting(0.5, 0.7, 54, 'interieur', 'schwarz', flat=True)


JW, JH = 0.62, 0.82
# Lage der Figur auf der Leinwand (y quer, z hoch; Mitte 0)
J_HEAD = (0.0, 0.12)        # Mitte des Bildschirms
J_HAT = [(-0.115, 0.27), (0.0, 0.31), (0.12, 0.26)]
J_WRIST = [(-0.17, 0.12), (0.15, -0.02)]
J_ANKLE = [(-0.06, -0.27), (0.07, -0.25)]
J_SHOE = [(-0.12, -0.31), (0.135, -0.29)]
J_TORSO = (0.0, -0.06)


def _jester_scene(c):
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    # Buehne: schwarzer Samt, rote Vorhaenge mit Falten, Schachbrettboden in Fluchtperspektive
    c.poly(L1.rect(-hw - 0.02, -hh - 0.02, hw + 0.02, hh + 0.02), (0.05, 0.035, 0.06), 0.0)
    for side in (-1, 1):
        for i in range(6):
            x0 = side * (hw - i * 0.022)
            x1 = side * (hw - (i + 1) * 0.022 - 0.004)
            col = (0.50, 0.06, 0.07) if i % 2 == 0 else (0.32, 0.035, 0.05)
            c.poly([(x0, hh + 0.02), (x1, hh + 0.02), (x1 + side * 0.01 * math.sin(i), -hh * 0.35), (x0 + side * 0.006, -hh * 0.38)], col, 0.002)
    c.poly(L1.rect(-hw - 0.02, hh - 0.07, hw + 0.02, hh + 0.02), (0.42, 0.05, 0.06), 0.003)  # Querbehang
    for i in range(9):
        x = -hw + (i + 0.5) * W / 9
        c.blob(x, hh - 0.075, 0.03, (0.36, 0.04, 0.05), 1.0, 9, 0.2)
    zf, zb = -hh, -hh * 0.42
    n = 8
    for row in range(4):
        t0, t1 = row / 4, (row + 1) / 4
        z0, z1 = zf + (zb - zf) * t0 ** 0.8, zf + (zb - zf) * t1 ** 0.8
        w0, w1 = hw * (1.25 - 0.55 * t0), hw * (1.25 - 0.55 * t1)
        for col in range(n):
            a0, a1 = -1 + 2 * col / n, -1 + 2 * (col + 1) / n
            light = (row + col) % 2 == 0
            c.poly([(a0 * w0, z0), (a1 * w0, z0), (a1 * w1, z1), (a0 * w1, z1)], (0.82, 0.80, 0.74) if light else (0.06, 0.06, 0.07), 0.001)
    # Faeden von Handgelenken, Knoecheln und Muetze nach oben aus dem Bild (rot)
    for (y, z) in J_WRIST + J_ANKLE + [J_HAT[1]]:
        c.stroke([(y, z), (y * 0.7 + rg.uniform(-0.01, 0.01), hh + 0.02)], 0.0028, (0.78, 0.07, 0.06))
    wood, wood_d = (0.58, 0.40, 0.22), (0.36, 0.23, 0.12)
    blue, yellow = (0.14, 0.24, 0.62), (0.88, 0.70, 0.16)
    # Beine: Holz mit Gelenkzapfen, schlaff; Schuhe mit gerollten Spitzen
    for (hy, (ay, az), (sy, sz)) in ((-0.035, J_ANKLE[0], J_SHOE[0]), (0.035, J_ANKLE[1], J_SHOE[1])):
        ky, kz = (hy + ay) * 0.5 + (-0.03 if hy < 0 else 0.035), (-0.14 + az) * 0.5
        c.stroke([(hy, -0.14), (ky, kz)], 0.026, wood)
        c.stroke([(ky, kz), (ay, az)], 0.022, wood)
        c.blob(ky, kz, 0.016, wood_d, 1.0, 10, 0.05)
        c.poly([(ay - 0.025, az + 0.01), (ay + 0.025, az + 0.01), (sy + (0.02 if sy > 0 else -0.02), sz + 0.02), (sy, sz), (ay - 0.02, az - 0.025)],
               blue if hy < 0 else yellow, 0.002)
    # Rumpf: Rauten aus blauem und gelbem Stoff, dunkle Naehte, gezackter Kragen
    ty, tz = J_TORSO
    body = [(ty - 0.09, tz + 0.12), (ty + 0.09, tz + 0.12), (ty + 0.075, tz - 0.08), (ty - 0.075, tz - 0.08)]
    c.poly(body, (0.08, 0.08, 0.14), 0.002)
    for i in range(-3, 4):
        for j in range(-3, 4):
            cy, cz = ty + i * 0.042 + (j % 2) * 0.021, tz + j * 0.042
            if abs(cy - ty) > 0.085 - (tz + 0.12 - cz) * 0.0 or cz > tz + 0.13 or cz < tz - 0.09:
                continue
            d = [(cy, cz + 0.024), (cy + 0.02, cz), (cy, cz - 0.024), (cy - 0.02, cz)]
            d = [(min(max(p[0], ty - 0.088), ty + 0.088), min(max(p[1], tz - 0.08), tz + 0.12)) for p in d]
            c.poly(d, blue if (i + j) % 2 == 0 else yellow, 0.0015)
    c.poly([(ty - 0.06, -0.14), (ty + 0.06, -0.14), (ty + 0.075, tz - 0.08), (ty - 0.075, tz - 0.08)], yellow, 0.002)  # Hose
    kz = tz + 0.125
    zig = [(ty - 0.11 + i * 0.0275, kz + (0.022 if i % 2 else -0.004)) for i in range(9)]
    c.poly(zig + [(ty + 0.11, kz - 0.02), (ty - 0.11, kz - 0.02)], (0.92, 0.88, 0.76), 0.001)
    # Arme: Holz, an den Faeden hochgezogen bzw. haengend
    for (sy, (wy, wz)) in ((-0.08, J_WRIST[0]), (0.08, J_WRIST[1])):
        ey, ez = (sy + wy) * 0.5 + (-0.04 if sy < 0 else 0.04), (tz + 0.1 + wz) * 0.5
        c.stroke([(sy, tz + 0.1), (ey, ez)], 0.02, wood)
        c.stroke([(ey, ez), (wy, wz)], 0.017, wood)
        c.blob(ey, ez, 0.013, wood_d, 1.0, 9, 0.05)
        c.blob(wy, wz, 0.016, wood, 1.0, 8, 0.1)  # Hand (Holzkugel)
    # Kopf: Narrenkappe mit drei Zipfeln (die Kugeln sind echt, siehe _jester_extra), darunter der Bildschirm
    hy, hz = J_HEAD
    c.poly(L1.rect(hy - 0.06, hz - 0.05, hy + 0.06, hz + 0.05), (0.03, 0.03, 0.035), 0.0005)  # Rahmen des Bildschirms (gemalt)
    for i, (py, pz) in enumerate(J_HAT):
        col = blue if i != 1 else yellow
        c.poly([(hy - 0.065 + i * 0.045, hz + 0.045), (hy - 0.02 + i * 0.045, hz + 0.045), ((py + hy) * 0.5 + 0.01, (pz + hz) * 0.5 + 0.04),
                (py, pz)], col, 0.002)
    c.poly(L1.rect(hy - 0.07, hz + 0.04, hy + 0.07, hz + 0.065), yellow, 0.002)  # Krempe


def _jester_extra(expr):
    """Echte Teile auf dem Bild: Glasbildschirm mit leuchtendem Gesicht, rote Kugeln (Muetze, Schuhe, Rumpf)."""
    def extra(mb, x0):
        glass = material('bildschirm_glas', (0.02, 0.022, 0.028), look='glass', rough=0.05, r=0.3)
        bezel = material('bildschirm_rahmen', (0.06, 0.06, 0.07), look='plastic', rough=0.5, r=0.8)
        glow = material('gesicht_rot', (1.0, 0.09, 0.05), look='glow', glow=3.0)
        red = material('kugel_rot', (0.72, 0.05, 0.05), look='plastic', rough=0.3, r=0.45)
        M = L1.frame(Vector((x0, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)))  # lokal (y, z, Tiefe)
        hy, hz = J_HEAD
        mb.add(L1.b_box((hy - 0.058, hz - 0.046, 0.0), (hy + 0.058, hz + 0.046, 0.007)), bezel, M)
        mb.add(L1.b_box((hy - 0.05, hz - 0.039, 0.007), (hy + 0.05, hz + 0.039, 0.0085)), glass, M)
        z = 0.0089
        for ey in (hy - 0.022, hy + 0.022):  # zwei rote X
            for a in (0.785, -0.785):
                mb.add(L1.b_plate(L1.rot2(L1.rect(-0.013, -0.0022, 0.013, 0.0022), a, 0, 0), z), glow, M @ L1.T(ey, hz + 0.011, 0))
        if expr == 0:  # Cartoon-Grinsen
            arc = [(hy + 0.03 * math.cos(a), hz - 0.004 + 0.022 * math.sin(a)) for a in [math.pi * (1.1 + 0.8 * i / 10) for i in range(11)]]
            mb.add(L1.b_plate(L1.stroke(arc, 0.004), z), glow, M)
        elif expr == 1:  # verzogen, schief
            arc = [(hy - 0.026 + 0.052 * i / 10, hz - 0.026 + 0.012 * math.sin(math.pi * i / 10) + 0.006 * i / 10) for i in range(11)]
            mb.add(L1.b_plate(L1.stroke(arc, 0.004), z), glow, M)
        else:  # lachend offen
            mouth = [(hy + 0.028 * math.cos(a), hz - 0.012 + 0.02 * math.sin(a)) for a in [math.pi * (1.0 + i / 12) for i in range(13)]]
            mb.add(L1.b_plate(mouth, z), glow, M)
        for (py, pz) in J_HAT + J_SHOE + [(J_TORSO[0], J_TORSO[1] + 0.03)]:
            mb.add(L1.b_lathe([(0.0, -0.011), (0.0078, -0.0078), (0.011, 0.0), (0.0078, 0.0078), (0.0, 0.011)], 10), red,
                   M @ L1.T(py, pz, 0.011))
    return extra


def _painting_jester(expr):
    L1._painting(JW, JH, 61, _jester_scene, 'gold', flat=True, extra=_jester_extra(expr))
    # die roten Faeden reichen weiter, als man sieht: aus dem liegenden Bild nach oben ins Dunkel
    k = Kit()
    thread = material('faden_rot', (0.70, 0.06, 0.05), look='fabric', rough=0.8, r=1.1)
    rng = random.Random(62)
    for (y, z) in J_WRIST + J_ANKLE + [J_HAT[1]]:
        base = V((-z, y, 0.026))
        top = V((-z * 0.7 + rng.uniform(-0.06, 0.06), y * 0.7 + rng.uniform(-0.06, 0.06), 2.45))
        mid = base.lerp(top, 0.5) + V((rng.uniform(-0.02, 0.02), rng.uniform(-0.02, 0.02), 0))
        sweep(k(thread, 30), [base, mid, top], [0.0018, 0.0016, 0.0012], 4, caps=False)
    k.finish()


def build_painting_jester():
    _painting_jester(0)


def build_painting_jester__v1():
    _painting_jester(1)


def build_painting_jester__v2():
    _painting_jester(2)


# =====================================================================================================================
# Offenes Mandelwasser (Item, aufrecht, Ursprung unten)
# =====================================================================================================================
def build_almond_open():
    rng = random.Random(2301)
    P = mats()
    k = Kit()
    _bottle(k, Matrix.Identity(4), True, True, P, rng)
    # truebes Wasser innen (durch den offenen Hals sichtbar), ein paar weisse Flocken auf dem Glas
    lathe(k(material('mandelwasser_trueb', (0.62, 0.62, 0.50), look='water', rough=0.2), 30), [(0.0, 0.2), (0.016, 0.2)], 12)
    k.finish()
