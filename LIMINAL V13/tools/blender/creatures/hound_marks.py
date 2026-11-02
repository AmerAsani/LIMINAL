"""V13.3 Level 2/3 (KREATUR_Hound.md, Abschnitt "Rudel"): Zeichen der Rudel-Hounds auf dem Grundmodell hound.py.

Wikidot (Entity 8, Level 2, Level 3) beschreibt keine Unterschiede im Aussehen - die Zeichen sind Auslegung aus der Ebene,
in der die Hounds leben (im Briefing so gekennzeichnet):
  overlay      Ueberzug der unteren Glieder (Unterarm bis Fingerspitzen, Schienbein bis Zehen) als duenne Schale ueber der
               Haut: Level 2 schwarze, glaenzende Fluessigkeit aus den Rohren, Level 3 Russ. Der Rand laeuft ausgefranst
               den Unterarm hinauf (dort sinkt die Schale unter die Haut).
  lichtenberg  Level 3: verzweigte Brandnarben eines Stromschlags (Lichtenberg-Figur) quer ueber Ruecken und Flanken -
               erhabene, glaenzende Wuelste auf der Hautoberflaeche (eigene Gruppe, eigenes Material).
  sores        Level 2: offene Stellen (Hitze, Reibung) als rohe, nasse Flecken auf Flanken und Keulen.
Knochen und Gelenke wie hound.py (skeletons.json, ent_hound).
"""
import math

import numpy as np

import anat as A
import sdf

LOWER_BONES = {11, 12, 13, 14, 15, 16, 17, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32, 34, 35, 36, 37, 38}
TORSO_BONES = {1, 2, 3, 9, 18}
# Glieder fuer den Rand des Ueberzugs: (Gelenk oben, Gelenk unten, Rand bei t)
LIMBS = ((12, 13, 0.30), (21, 22, 0.34), (30, 31, 0.42), (37, 38, 0.38))


def part_of(groups, name):
    for g in groups:
        if g['name'] == name:
            return g
    raise KeyError(name)


def overlay(J, skin_group, fm, name, seed, lift=0.0007, drips=0):
    """Schale ueber den unteren Gliedern (Material fm). Ausserhalb des Rands liegt sie 1 cm unter der Haut."""
    J = np.asarray(J)
    parts = [p for p in skin_group['parts'] if p.bone in LOWER_BONES and p.op == 'add']
    base = skin_group['disp']
    segs = [(J[a], J[b], t0) for a, b, t0 in LIMBS]

    def disp(Q):
        d = base(Q) - lift
        best = np.full(len(Q), 9.0)
        tt = np.zeros(len(Q))
        t0s = np.zeros(len(Q))
        for a, b, t0 in segs:
            ab = b - a
            t = (Q - a) @ ab / float(ab @ ab)
            dd = np.linalg.norm(Q - (a + np.outer(np.clip(t, 0, 1), ab)), axis=1)
            m = dd < best
            best[m], tt[m], t0s[m] = dd[m], t[m], t0
        # ausgefranster Rand (Runde 1: der glatte Rand las sich wie Stiefel/Handschuhe): lange Zungen laufen den Arm hinauf,
        # darueber Spritzer und Flecken, unten reisst der Ueberzug an den Knoecheln auf
        n = sdf.fbm(Q, 9, 3, seed=seed) - 0.5
        n2 = sdf.fbm(Q, 34, 2, seed=seed + 1) - 0.5
        edge = t0s + 0.55 * n + 0.18 * n2
        covered = np.clip((tt - edge) / 0.03, 0, 1)
        spots = np.clip((sdf.fbm(Q, 48, 2, seed=seed + 3) - 0.66) * 9, 0, 1) * (tt > t0s - 0.45)
        covered = np.maximum(covered, spots)
        covered *= 1.0 - np.clip((sdf.fbm(Q, 60, 2, seed=seed + 4) - 0.74) * 9, 0, 1) * (tt > t0s + 0.2)
        return d + 0.012 * (1.0 - covered)

    groups = [{'name': name, 'fm': fm, 'parts': parts, 'disp': disp, 'h': 0.0022, 'cav_scale': 0.008}]
    if drips:  # Tropfen unter Handgelenken und Fersen (Level 2: die Fluessigkeit laeuft ab)
        rng = np.random.default_rng(seed + 7)
        dp = []
        for wj, kj, bone in ((13, 14, 12), (22, 23, 21), (31, 32, 29), (38, 39, 35)):
            for _ in range(drips):
                t = rng.uniform(0.0, 0.8)
                c = A.lerp(J[wj], J[kj], t) + np.array([rng.normal(0, 0.006), rng.normal(0, 0.006), -0.012 - rng.uniform(0, 0.006)])
                L = rng.uniform(0.008, 0.022)
                dp.append(A.cone(bone, c + (0, 0, 0.006), c - (0, 0, L), 0.0034, 0.0024, k=0.004))
                dp.append(A.ell(bone, c - (0, 0, L), (0, 0, 1), (0.0036, 0.0034, 0.0042), k=0.003))
        groups.append({'name': name + '_tropfen', 'fm': fm, 'parts': dp, 'disp': None, 'h': 0.0010, 'cav_scale': 0.004})
    return groups


def torso_probe(skin_group):
    parts = [p for p in skin_group['parts'] if p.bone in TORSO_BONES and p.op == 'add']
    disp = skin_group['disp']
    zc = 0.735

    def surface(x, th):
        """Punkt auf der Haut des Rumpfs: Strahl von der Achse (x, 0, zc) in Richtung (0, sin th, cos th)."""
        d = np.array([0.0, math.sin(th), math.cos(th)])
        ts = np.arange(0.02, 0.24, 0.0015)
        Q = np.array([x, 0.0, zc]) + np.outer(ts, d)
        F = sdf.sample(parts, Q) + disp(Q)
        out = np.nonzero(F > 0)[0]
        if not len(out):
            return None, d
        i = out[0]
        return Q[i], d
    return surface


def bone_at(x):
    return 1 if x < -0.25 else (2 if x < 0.04 else 3)


def lichtenberg(skin_group, seed, roots):
    """Verzweigte Brandnarben (Lichtenberg-Figur) vom Ruecken die Flanken hinab. roots: (x, Seite)."""
    surface = torso_probe(skin_group)
    rng = np.random.default_rng(seed)
    R = 0.115
    cones = []

    def grow(x, th, head, r, depth, budget):
        p0, n0 = surface(x, th)
        if p0 is None:
            return
        for _ in range(budget):
            head += rng.normal(0, 0.42)
            side = 1.0 if th >= 0 else -1.0
            # meist abwaerts (|th| waechst), etwas laengs des Rumpfs
            want = math.atan2(side * 1.0, 0.35 * math.cos(head))
            head = head * 0.55 + want * 0.45
            ds = 0.0075 + 0.004 * rng.random()
            x2 = x + ds * math.cos(head)
            th2 = th + ds * math.sin(head) / R
            if abs(th2) > 1.85 or x2 < -0.47 or x2 > 0.27:
                return
            p1, n1 = surface(x2, th2)
            if p1 is None:
                return
            nrm = A.unit(np.array([0.0, p1[1], p1[2] - 0.735]))
            cones.append((bone_at(x2), p0 - nrm * r * 0.45, p1 - nrm * r * 0.45, r, r * 0.97, x2, side))
            x, th, p0 = x2, th2, p1
            r *= 0.975
            if r < 0.0016:
                return
            if depth < 4 and rng.random() < 0.20:
                grow(x, th, head + rng.choice((-1, 1)) * rng.uniform(0.5, 1.0), r * 0.72, depth + 1, int(budget * 0.6))

    for x, side in roots:
        th0 = side * rng.uniform(0.04, 0.22)
        for k in range(2):  # zwei Hauptaeste je Wurzel
            grow(x + rng.normal(0, 0.01), th0, (1.0 if side > 0 else -1.0) * (1.2 + 0.4 * k) + rng.normal(0, 0.2), 0.0042, 0, 46)
    # in Haufen teilen (je Gruppe ein eigenes Gitter), danach ein Netz
    groups = []
    for xi, (x0, x1) in enumerate(((-0.5, -0.22), (-0.22, 0.04), (0.04, 0.3))):
        for si, sd in enumerate((1.0, -1.0)):
            ps = [A.cone(b, a, c, ra, rb, k=0.0035) for (b, a, c, ra, rb, x, s) in cones if x0 <= x < x1 and s == sd]
            if ps:
                groups.append({'name': f'narbe{xi}{si}', 'fm': 'ent_hd_scar', 'parts': ps, 'disp': scar_disp, 'h': 0.0013,
                               'cav_scale': 0.004, 'merge': 'narben'})
    return groups


def scar_disp(Q):
    # Narbengewebe: wulstig, quer gerunzelt
    return -0.00025 * (sdf.ridged(Q, 260, 1, seed=91) - 0.5) - 0.0003 * sdf.fbm(Q, 120, 1, seed=92)


def sores(skin_group, seed, count):
    """Offene, nasse Stellen auf Flanken und Keulen (eigene Gruppe, rohes Fleisch), mit wulstigem Rand in der Haut."""
    surface = torso_probe(skin_group)
    rng = np.random.default_rng(seed)
    raw, rims = [], []
    for _ in range(count * 4):
        if len(raw) >= count:
            break
        x = rng.uniform(-0.45, 0.2)
        th = rng.choice((-1, 1)) * rng.uniform(0.55, 1.5)
        p, _ = surface(x, th)
        if p is None:
            continue
        n = A.unit(np.array([0.0, p[1], p[2] - 0.735]))
        r = rng.uniform(0.007, 0.016)
        ax = A.perp(n)
        # Wulst in der Haut, in der Mitte ausgehoehlt; darin das rohe Fleisch (etwas unter dem Wulstrand)
        rims.append(A.ell(bone_at(x), p, ax, (r * 1.75, r * 1.4, 0.0026), up=n, k=0.004))
        rims.append(A.ell(bone_at(x), p + n * 0.0021, ax, (r * 1.15, r * 0.88, 0.0030), up=n, op='sub', k=0.002))
        raw.append(A.ell(bone_at(x), p - n * 0.0004, ax, (r * 1.22, r * 0.94, 0.0022), up=n, k=0.002))
    return raw, rims
