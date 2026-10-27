"""V13.3: Nguithr'xurh - die Netzkugeln einer Kolonie (Variante "web" von ent_nguithrxurh), KREATUR_Nguithrxurh.md.

Die Seite: kugelfoermige Netze, mit einem Betaeubungsstoff gefuellt, mit zusaetzlichen Faeden an die Decke gehaengt. Hier:
sechs Kugeln (8-13 cm Radius) aus Seide - eine dichte, klumpige Huelle mit Rissen, durch die der milchige Stoff
glaenzt, umwickelt von Faeden; unten haengen Tropfen. Jede haengt an einem Faden, der durch die Decke reicht (die
Laenge passt sich im Spiel der Decke an). An der Decke: ein Geflecht aus Faeden zwischen den Ankern, Seidenbueschel,
lose haengende Faeden. Am Boden je Kugel eine Pfuetze mit Seidenfetzen (verborgen, bis die Kugel zerplatzt).
Variante web_moth (Level 2: sie fressen Insekten und Deathmoths): zwei eingesponnene Motten im Geflecht.

Knochen: 0 Wurzel (Mitte an der Decke: Geflecht), 47 + 2k Kugel k (mit Faden), 48 + 2k Pfuetze k.
"""
import math

import numpy as np

import anat as A
import sdf

SKELETON = 'ent_nguithrxurh'
LODS = (36000, 12000, 4000)
MOTHS = False
try:
    import __main__
    __main__.PREVIEW_RGB.update({'ent_silk': (0.85, 0.84, 0.8), 'ent_sedative': (0.85, 0.82, 0.62), 'ent_solifuge': (0.55, 0.42, 0.28)})
except Exception:
    pass

SAC0 = 66
B_SAC0 = 47
NS = 6
Z = np.array([0.0, 0.0, 1.0])


def sacJ(k, i):
    return SAC0 + k * 4 + i


def sphere_pt(rng):
    v = rng.normal(0, 1, 3)
    return v / np.linalg.norm(v)


def build_web(J, moths):
    rng = np.random.default_rng(1616 + (7 if moths else 0))
    J = np.asarray(J, dtype=np.float64)
    groups = []
    R = [0.13, 0.11, 0.12, 0.10, 0.12, 0.09]
    for k in range(NS):
        b, bs = B_SAC0 + 2 * k, B_SAC0 + 2 * k + 1
        c = J[sacJ(k, 1)]
        r = R[k]
        # Stoff: Kugel, leicht klumpig
        fluid = [A.ell(b, c, (1, 0, 0), (r * 0.9, r * 0.88, r * 0.92), k=0.0)]
        for _ in range(3):
            u = sphere_pt(rng)
            fluid.append(A.ell(b, c + u * r * 0.4, u, (r * 0.55, r * 0.5, r * 0.5), k=0.03))
        # Tropfen unten
        for i in range(int(rng.integers(2, 4))):
            q = c + np.array([rng.uniform(-0.4, 0.4) * r, rng.uniform(-0.4, 0.4) * r, -r * 0.92])
            L = rng.uniform(0.012, 0.03)
            fluid.append(A.cone(b, q + Z * 0.01, q - Z * L, 0.006, 0.0035, k=0.006))
            fluid.append(A.ell(b, q - Z * (L + 0.004), (0, 0, 1), (0.008, 0.0075, 0.0075), k=0.004))
        groups.append({'name': f'stoff{k}', 'fm': 'ent_sedative', 'parts': fluid,
                       'disp': lambda P, k=k: -0.0012 * sdf.fbm(P, 60, 2, seed=1610 + k), 'h': 0.0012, 'cav_scale': 0.006, 'merge': 'stoff'})
        # Huelle aus Seide: birnenfoermig (zum Faden hin zusammengezogen), klumpig, matt und faserig; wenige schmale Risse, durch
        # die der Stoff glaenzt (Runde 1: runde, glaenzende Kugeln mit Loechern - wirkten wie Christbaumkugeln, Lampen)
        shell = [A.ell(b, c - Z * r * 0.08, (1, 0, 0), (r, r * 0.97, r * 0.92), k=0.0),
                 A.cone(b, c + Z * r * 0.2, c + Z * (r + 0.1), r * 0.78, 0.012, k=0.04)]
        for _ in range(4):
            u = sphere_pt(rng)
            shell.append(A.ell(b, c + u * r * 0.45, u, (r * 0.6, r * 0.52, r * 0.52), k=0.03))
        shell.append(A.ell(b, c - Z * r * 0.06, (1, 0, 0), (r * 0.88, r * 0.85, r * 0.8), op='sub', k=0.004))
        for _ in range(2):  # schmale Risse seitlich
            u = A.unit(np.array([rng.normal(), rng.normal(), rng.uniform(-0.3, 0.3)]))
            shell.append(A.ell(b, c + u * r * 1.0, u, (r * 0.15, r * rng.uniform(0.3, 0.5), r * 0.07), op='sub', k=0.008))
        wraps = []
        for _ in range(22):
            n = sphere_pt(rng)
            e1 = A.perp(n)
            e2 = np.cross(n, e1)
            a0, span = rng.uniform(0, 2 * math.pi), rng.uniform(1.5, 4.5)
            pts = [c + (e1 * math.cos(a0 + span * t) + e2 * math.sin(a0 + span * t)) * (r * rng.uniform(1.0, 1.06)) for t in np.linspace(0, 1, 12)]
            wraps += [A.tube(b, pts[i], pts[i + 1], (0.0024, 0.0024)) for i in range(len(pts) - 1)]
        fluff = []
        for _ in range(30):  # lose Faeden stehen ab
            u = sphere_pt(rng)
            p0 = c + u * r * 0.98
            p1 = p0 + A.unit(u + rng.normal(0, 0.7, 3)) * rng.uniform(0.03, 0.09)
            fluff.append(A.tube(b, p0, p1, (0.0014, 0.0014)))
        for _ in range(5):  # Faeden haengen unten heraus
            p0 = c + np.array([rng.uniform(-0.5, 0.5) * r, rng.uniform(-0.5, 0.5) * r, -r * 0.85])
            L = rng.uniform(0.08, 0.26)
            p1 = p0 + np.array([rng.uniform(-0.03, 0.03), rng.uniform(-0.03, 0.03), -L * 0.55])
            p2 = p0 + np.array([rng.uniform(-0.05, 0.05), rng.uniform(-0.05, 0.05), -L])
            fluff += [A.tube(b, p0, p1, (0.0013, 0.0013)), A.tube(b, p1, p2, (0.0013, 0.0013), (0.0009, 0.0009))]
        groups.append({'name': f'flaum{k}', 'fm': 'ent_silk', 'parts': fluff, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002, 'merge': 'seide',
                       'lod_weight': 0.6})

        def silk_disp(P, k=k):
            return -0.0009 * (sdf.ridged(P * np.array([1.0, 1.0, 2.5]), 180, 2, seed=1620 + k) - 0.5) - 0.0006 * sdf.fbm(P, 400, 1, seed=1630 + k)

        groups.append({'name': f'huelle{k}', 'fm': 'ent_silk', 'parts': shell + wraps, 'disp': silk_disp, 'h': 0.0012, 'cav_scale': 0.008,
                       'merge': 'seide'})
        # Faden: von der Kugel bis 2 m ueber den Anker (reicht durch die Decke)
        top = J[sacJ(k, 0)] + Z * 2.0
        groups.append({'name': f'faden{k}', 'fm': 'ent_silk', 'parts': [A.cone(b, c + Z * (r + 0.03), top, 0.0017, 0.0013)], 'disp': None,
                       'h': 0.0007, 'cav_scale': 0.002, 'merge': 'seide', 'lod_weight': 0.25})
        # Pfuetze am Boden mit Spritzern und Fetzen der Huelle
        p0 = J[sacJ(k, 2)]
        puddle = []
        for i in range(7):
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 0.09) if i else 0.0
            q = p0 + np.array([math.cos(a) * d, math.sin(a) * d, 0.0])
            rr = rng.uniform(0.05, 0.11)
            puddle.append(A.ell(bs, q, (math.cos(a), math.sin(a), 0), (rr * rng.uniform(1.0, 1.6), rr, 0.0032), k=0.02))
        groups.append({'name': f'pfuetze{k}', 'fm': 'ent_sedative', 'parts': puddle, 'disp': None, 'h': 0.0012, 'cav_scale': 0.004,
                       'merge': 'stoff'})
        for i in range(10):  # Spritzer
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.18, 0.38)
            q = p0 + np.array([math.cos(a) * d, math.sin(a) * d, 0.0])
            groups.append({'name': f'spritzer{k}_{i}', 'fm': 'ent_sedative', 'parts': [A.ell(bs, q, (math.cos(a), math.sin(a), 0),
                           (rng.uniform(0.008, 0.02), rng.uniform(0.005, 0.01), 0.0025), k=0.0)], 'disp': None, 'h': 0.0009,
                           'cav_scale': 0.002, 'merge': 'stoff'})
        shreds = []
        for i in range(5):
            a, d = rng.uniform(0, 2 * math.pi), rng.uniform(0.02, 0.15)
            q = p0 + np.array([math.cos(a) * d, math.sin(a) * d, 0.006])
            u = A.unit(rng.normal(0, 1, 3) * np.array([1, 1, 0.3]))
            shreds.append(A.ell(bs, q, u, (rng.uniform(0.03, 0.06), rng.uniform(0.02, 0.035), 0.003), up=Z + u * 0.3, k=0.0))
        for i in range(6):
            a = rng.uniform(0, 2 * math.pi)
            q0 = p0 + np.array([math.cos(a) * 0.03, math.sin(a) * 0.03, 0.004])
            q1 = q0 + np.array([math.cos(a + 0.6), math.sin(a + 0.6), 0.02]) * rng.uniform(0.08, 0.18)
            shreds.append(A.tube(bs, q0, q1, (0.0018, 0.0018)))
        groups.append({'name': f'fetzen{k}', 'fm': 'ent_silk', 'parts': shreds, 'disp': None, 'h': 0.0012, 'cav_scale': 0.004,
                       'merge': 'seide'})
    # --- Geflecht an der Decke (Wurzel) ------------------------------------------------------------------------------
    anchors = [J[sacJ(k, 0)] for k in range(NS)]
    center = np.array([0.0, 0.0, -0.012])
    mats = []

    def thread(a, b, r, sag=0.0, n=6):
        pts = [a + (b - a) * t - Z * (sag * 4 * t * (1 - t)) for t in np.linspace(0, 1, n)]
        return [A.tube(0, pts[i], pts[i + 1], (r, r)) for i in range(n - 1)]

    for a in anchors:  # Speichen
        mats.append(thread(center, a - Z * 0.01, 0.0028, 0.02))
        for _ in range(3):  # aufgefaechert zur Decke
            q = a + np.array([rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), -0.002])
            mats.append(thread(a - Z * 0.01, q, 0.0011, 0.0, 3))
    order = np.argsort([math.atan2(a[1], a[0]) for a in anchors])
    for i in range(len(order)):  # Runde 1: an der Decke kaum zu sehen - dichte Seidenlappen zwischen den Speichen
        a1, a2 = anchors[order[i]], anchors[order[(i + 1) % len(order)]]
        m = (a1 + a2) * 0.3
        d = A.unit(np.array([(a2 - a1)[0], (a2 - a1)[1], 0.0]) + 1e-6)
        mats.append([A.ell(0, np.array([m[0], m[1], -0.008]), d, (rng.uniform(0.08, 0.16), rng.uniform(0.04, 0.08), 0.003), k=0.0)])
    for ring_r in (0.22, 0.45):  # zwei Ringe zwischen den Speichen
        pts = []
        for i in range(len(order) + 1):
            a = anchors[order[i % len(order)]]
            d = a[:2] / max(np.linalg.norm(a[:2]), 1e-6)
            rr = min(ring_r, float(np.linalg.norm(a[:2])) * 0.9)
            pts.append(np.array([d[0] * rr, d[1] * rr, -0.012 - 0.01 * rng.random()]))
        for i in range(len(pts) - 1):
            mats.append(thread(pts[i], pts[i + 1], 0.0012, 0.03, 5))
    for a in anchors:  # Bueschel am Anker
        mats.append([A.ell(0, a - Z * 0.008, (1, 0, 0), (0.032, 0.026, 0.011), k=0.0),
                     A.ell(0, a - Z * 0.016 + np.array([0.01, -0.006, 0]), (0, 1, 0), (0.016, 0.014, 0.01), k=0.006)])
    for _ in range(9):  # lose haengende Faeden
        a = anchors[int(rng.integers(0, NS))] * rng.uniform(0.3, 1.0)
        a[2] = -0.012
        L = rng.uniform(0.15, 0.6)
        q = a + np.array([rng.uniform(-0.05, 0.05), rng.uniform(-0.05, 0.05), -L])
        mats.append(thread(a, q, 0.0009, 0.0, 4))
    if moths:  # zwei eingesponnene Deathmoths
        for (x, y, hang, tilt) in ((0.22, -0.16, 0.30, 0.4), (-0.17, 0.2, 0.46, -0.5)):
            top = np.array([x, y, -0.012])
            c = np.array([x, y, -hang])
            mats.append(thread(top, c + Z * 0.06, 0.0012, 0.0, 4))
            ax = A.unit(np.array([math.sin(tilt) * 0.3, 0.1, 1.0]))
            cocoon = [A.ell(0, c, ax, (0.07, 0.03, 0.028), k=0.0), A.ell(0, c - ax * 0.03, ax, (0.04, 0.034, 0.03), k=0.02)]
            for i in range(10):
                n = A.unit(rng.normal(0, 1, 3))
                e1 = A.perp(n)
                e2 = np.cross(n, e1)
                pts = [c + ax * (0.06 * math.cos(t * 3.1)) + (e1 * math.cos(t * 5) + e2 * math.sin(t * 5)) * 0.031 for t in np.linspace(0, 1, 8)]
                cocoon += [A.tube(0, pts[j], pts[j + 1], (0.0016, 0.0016)) for j in range(7)]
            mats.append(cocoon)
            wing = [A.ell(0, c + np.array([0.035, 0.0, -0.02]), A.unit(np.array([0.8, 0.3, -0.5])), (0.055, 0.03, 0.0016), up=(0, 1, 0.3), k=0.0)]
            groups.append({'name': f'fluegel{x}', 'fm': 'ent_wing', 'parts': wing, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002, 'merge': 'fluegel'})
            legs = []
            for j in range(3):
                p = c + np.array([-0.02, 0.01 * (j - 1), -0.03])
                legs.append(A.cone(0, p, p + A.unit(np.array([-0.5, 0.3 * (j - 1), -0.8])) * 0.03, 0.0012, 0.0005))
            groups.append({'name': f'beine{x}', 'fm': 'ent_chitin', 'parts': legs, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'merge': 'beine'})
    for i, m in enumerate(mats):
        groups.append({'name': f'geflecht{i}', 'fm': 'ent_silk', 'parts': m, 'disp': None, 'h': 0.0008, 'cav_scale': 0.002, 'merge': 'seide',
                       'lod_weight': 0.7})
    return groups


def build(J, bones):
    return build_web(J, MOTHS)
