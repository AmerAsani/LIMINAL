"""V13.3: Wrangler (Entity 75, Wikidot Rev. 41) - KREATUR_Wrangler.md. Eine riesige Schlange, von der man nur Abschnitte sieht.

Leib: 30 starre Ringe (je ein Knochen; das Spiel laesst jeden Ring fuer sich die Bahn entlanglaufen). Jeder Ring ist eine
durchscheinende Haut (ent_wr_membrane, im Spiel gerastert ausgeduennt), hinten schmal (steckt im vorigen Ring), vorn mit
einem Wulst, der den naechsten ueberdeckt - so klafft beim Biegen nichts. Darin, wie bei einem Ringelwurm je Abschnitt
wiederholt: ein Darmstueck, ein knoecherner Reifen (C-foermig - an ihm sieht man die Bohrerdrehung durch die Haut), paarige
Organe, pflanzenartige Faserstraenge (die Seite: der tote Riese bestand innen aus zerfallendem, pflanzenartigem Fleisch).
Vier Ringarten, je auf jeden vierten Ringknochen geklont (build.py 'clones').
Kopf (Maennchen): ein grosses, menschenaehnliches Gesicht auf einem Schlangenkopf - zu breites Laecheln bis an die Seiten,
viel zu viele Menschenzaehne, tiefe Falten, raue graue Haut, kleine leuchtend weisse Augen. Unterkiefer klappt hinten auf.
Kopf (Weibchen, wrangler_f): wurmartig, durchscheinend, statt des Mauls zwei Zangen.
Verschlucktes (eigene Knochen, zieht einmal vorbei): Brustkorb mit Schaedel, Rucksack, Arm, Stiefel.
Kraenze (drei Knochen, ein Netz geklont): Schleim, Schutt, Risse und Staub, wo der Leib durch eine Flaeche bricht.

Knochen (wg): 0 Wurzel, 1..30 Ringe, 31..34 Verschlucktes, 35..40 Hals, 41 Schaedel, 42 Unterkiefer, 43/44 Zangen,
45..47 Kraenze. Ruhepose: Achse in 1 m Hoehe entlang x, Ringe ab x = -8,25 (je 0,55 m), Hals ab 9,5 m, Schaedelbasis 12,5 m.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (118000, 50000, 15000)  # Abnahme Level 2: Stufe 0 unter der Grenze der Modellpruefung (120 000; vorher 160 000)
FEMALE = False
SEEDS = False
BROOD = False

NR, NI, NN = 30, 4, 6
B_RING0, B_ITEM0, B_NECK0 = 1, 31, 35
B_SKULL, B_JAW, B_PL, B_PR, B_COL0 = 41, 42, 43, 44, 45
RL, AZ = 0.55, 1.0
X0 = -0.5 * NR * RL
HB = np.array([12.5, 0.0, AZ])  # Schaedelbasis (Ruhepose)
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])


def smooth01(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


class Rev(sdf.Part):
    """Rotationskoerper um eine Achse parallel zu x durch (y0, z0); Radius R(x) als dichte Stuetzstellen (xs, rs).
    shell > 0: nur eine Haut dieser Dicke, an den Enden offen; sonst massiv mit flachen Enden."""

    def __init__(self, bone, xs, rs, y0=0.0, z0=AZ, shell=0.0, group='skin', op='add', k=0.0):
        self.kind, self.bone, self.k, self.op, self.group, self.g = 'rev', bone, k, op, group, {}
        self.xs, self.rs = np.asarray(xs, float), np.asarray(rs, float)
        self.y0, self.z0, self.shell = y0, z0, shell
        sl = np.gradient(self.rs, self.xs)
        self.sl = sl
        r = float(self.rs.max()) + shell
        m = k + 0.004
        self.lo = np.array([self.xs[0], y0 - r, z0 - r]) - m
        self.hi = np.array([self.xs[-1], y0 + r, z0 + r]) + m

    def dist(self, P):
        x = P[:, 0]
        rho = np.hypot(P[:, 1] - self.y0, P[:, 2] - self.z0)
        R = np.interp(x, self.xs, self.rs)
        s = np.interp(x, self.xs, self.sl)
        d = (rho - R) / np.sqrt(1.0 + s * s)
        if self.shell > 0:
            d = np.abs(d) - self.shell * 0.5
        cap = np.maximum(self.xs[0] - x, x - self.xs[-1])
        return np.maximum(d, cap)


def profile(ctrl, xs, passes=6):
    """Stuetzpunkte (x, r) -> geglaettete Kurve an xs."""
    c = np.asarray(ctrl, float)
    r = np.interp(xs, c[:, 0], c[:, 1])
    for _ in range(passes):
        r[1:-1] = 0.25 * r[:-2] + 0.5 * r[1:-1] + 0.25 * r[2:]
    return r


# --- Ringe -----------------------------------------------------------------------------------------------------
RING_XS = np.linspace(-0.21, 0.63, 64)


def ring_radius(fem):
    # hinten schmal (steckt im vorigen Ring), bauchig, vorn ein Wulst, der den naechsten ueberdeckt, der Rand rollt ein
    if fem:
        ctrl = [(-0.21, 0.635), (-0.05, 0.655), (0.12, 0.70), (0.30, 0.715), (0.46, 0.735), (0.55, 0.745), (0.60, 0.725), (0.63, 0.69)]
    else:
        ctrl = [(-0.21, 0.645), (-0.05, 0.665), (0.10, 0.715), (0.26, 0.73), (0.34, 0.722), (0.47, 0.755), (0.55, 0.77), (0.60, 0.75),
                (0.63, 0.71)]
    return profile(ctrl, RING_XS)


def ring_groups(v, fem, seeds, rng):
    """Ringart v (0..3): Haut, Darm, Reifen, Faserstraenge (und Samen), gebaut am Ringknochen v, geklont auf v + 4k."""
    xa = X0 + v * RL
    bone = B_RING0 + v
    clones = [(((r - v) * RL, 0.0, 0.0), B_RING0 + r) for r in range(v + 4, NR, 4)]
    R = ring_radius(fem)
    skin = [Rev(bone, xa + RING_XS, R, shell=0.032)]
    rel = R.copy()

    def skin_disp(P):
        xr = P[:, 0] - xa
        rho = np.hypot(P[:, 1], P[:, 2] - AZ)
        Rr = np.interp(xr, RING_XS, rel)
        outer = smooth01((rho - Rr + 0.004) / 0.008)  # nur die Aussenseite
        th = np.arctan2(P[:, 2] - AZ, P[:, 1])
        # Gewinde (Bohrer): drei flache Wendeln; Ringfurchen; Runzeln (Maennchen rau, Weibchen glatt mit Adern)
        thread = np.maximum(0.0, np.cos(3 * th + xr * (2 * math.pi / 0.95) + v * 0.7)) ** 6
        grooves = np.exp(-((xr - 0.17) / 0.014) ** 2) + 0.7 * np.exp(-((xr - 0.355) / 0.012) ** 2)
        if fem:
            vein = sdf.ridged(P * np.array([0.6, 1.0, 1.0]), 9, 3, seed=7501 + v)
            rel_ = 0.0024 * thread - 0.0035 * grooves + 0.0022 * smooth01((vein - 0.72) / 0.2) - 0.0008 * sdf.fbm(P, 30, 2, seed=7511 + v)
        else:
            wr = sdf.ridged(P * np.array([1.0, 0.7, 0.7]), 16, 3, seed=7521 + v)
            rel_ = 0.0035 * thread - 0.0055 * grooves - 0.0034 * smooth01((wr - 0.55) / 0.3) - 0.0018 * sdf.fbm(P, 22, 3, seed=7531 + v)
        # Narben (Maennchen): einzelne tiefe, ausgefranste Kerben
        return -rel_ * outer if not fem else -rel_ * outer

    groups = [{'name': f'haut{v}', 'merge': 'leib', 'fm': 'ent_wr_membrane_f' if fem else 'ent_wr_membrane', 'parts': skin,
               'disp': skin_disp, 'h': 0.0075, 'cav_scale': 0.02, 'tris': 2600, 'clones': clones,
               # die Nachbarringe (im Spiel 0,5 m weiter) fuer die Verdeckung: dunkle Falte, wo ein Ring im anderen steckt
               'cav_extra': [Rev(bone, xa - 0.5 + RING_XS, R, shell=0.032), Rev(bone, xa + 0.5 + RING_XS, R, shell=0.032)]}]
    # Darm: in der Mitte, an den Ringenden eingeschnuert (dort stoesst er an den Darm des Nachbarn), ein bis zwei Wuelste
    bul = [(0.28 + 0.06 * rng.uniform(-1, 1), 0.2 + 0.03 * rng.uniform(-1, 1))]
    if v % 2:
        bul = [(0.14, 0.17), (0.40, 0.19)]
    gx = np.linspace(-0.04, 0.6, 40)
    gr = np.full_like(gx, 0.075)
    for (cx, cr) in bul:
        gr = np.maximum(gr, cr * np.exp(-((gx - cx) / 0.13) ** 2))
    gy, gz = 0.03 * rng.uniform(-1, 1), AZ - 0.05 + 0.03 * rng.uniform(-1, 1)
    gut = [Rev(bone, xa + gx, gr, y0=gy, z0=gz)]
    # paarige Organe (wie Nephridien) und Aussackungen, mit dem Darm verwachsen - klumpig, dunkel
    ang0 = rng.uniform(0, 2 * math.pi)
    for sgn in (1, -1):
        a = ang0 + sgn * 1.2
        c = np.array([xa + 0.27 + 0.05 * sgn, gy + 0.26 * math.cos(a), gz + 0.26 * math.sin(a)])
        gut.append(A.ell(bone, c, (1, 0.2 * sgn, 0.1), (0.14, 0.09, 0.075), k=0.07))
    for j in range(3):
        a = ang0 + 2.6 + rng.uniform(-0.8, 0.8)
        c = np.array([xa + rng.uniform(0.08, 0.48), gy + 0.17 * math.cos(a), gz + 0.17 * math.sin(a)])
        gut.append(A.ell(bone, c, A.unit(rng.normal(0, 1, 3)), (rng.uniform(0.06, 0.1), rng.uniform(0.05, 0.08), rng.uniform(0.05, 0.07)), k=0.06))

    def gut_disp(P):
        return -0.006 * sdf.fbm(P, 14, 3, seed=7541 + v) - 0.003 * np.maximum(0, np.sin(P[:, 0] * 60.0))

    groups.append({'name': f'darm{v}', 'merge': 'darm', 'fm': 'ent_wr_gut', 'parts': gut, 'disp': gut_disp, 'h': 0.012,
                   'cav_scale': 0.02, 'tris': 520, 'clones': clones})
    # Reifen: C-foermig um den Darm (Knochen), um die Achse versetzt je Ringart
    rr = 0.52 if not fem else 0.5
    Rf = sdf.frame_from(Y * math.cos(ang0 + 2.4) + Z * math.sin(ang0 + 2.4), X)  # lokales z = Achse x
    hoop = [sdf.Part('torus', bone, k=0.0, c=np.array([xa + 0.24, 0, AZ]), R=Rf, major=rr, minor=0.032, arc=2.1)]
    # Knoten an den Enden des Reifens und ein Dorn zur Mitte (Wirbel)
    for sgn in (1, -1):
        a = ang0 + 2.4 + sgn * 2.1
        e = np.array([xa + 0.24, rr * math.cos(a), AZ + rr * math.sin(a)])
        hoop.append(A.ell(bone, e, X, (0.05, 0.045, 0.045), k=0.02))
    a = ang0 + 2.4
    top = np.array([xa + 0.24, rr * math.cos(a), AZ + rr * math.sin(a)])
    hoop.append(A.cone(bone, top, top + (np.array([xa + 0.24, 0, AZ]) - top) * 0.45, 0.04, 0.012, k=0.02))

    def hoop_disp(P):
        return -0.003 * sdf.fbm(P, 40, 2, seed=7551 + v)

    groups.append({'name': f'reifen{v}', 'merge': 'reifen', 'fm': 'ent_wr_bone', 'parts': hoop, 'disp': hoop_disp, 'h': 0.008,
                   'cav_scale': 0.01, 'tris': 380, 'clones': clones})
    # Faserstraenge: laengs, spindelfoermig (wie Leitbuendel eines Stengels), nicht bis an die Ringenden
    pith = []
    n = 6 + v % 2
    for i in range(n):
        a = ang0 + 2 * math.pi * i / n + rng.uniform(-0.2, 0.2)
        rad = rng.uniform(0.47, 0.58)
        x1, x2 = xa + rng.uniform(0.02, 0.1), xa + rng.uniform(0.42, 0.52)
        mid = 0.5 * (x1 + x2)
        c = np.array([mid, rad * math.cos(a), AZ + rad * math.sin(a)])
        pith.append(A.ell(bone, c, X, ((x2 - x1) * 0.5, rng.uniform(0.028, 0.045), rng.uniform(0.028, 0.045)), up=c - np.array([mid, 0, AZ]), k=0.0))
    # Nervenknoten neben dem Reifen
    pith.append(A.ell(bone, np.array([xa + 0.32, 0.12 * math.cos(ang0), AZ + 0.12 * math.sin(ang0) - 0.12]), X, (0.05, 0.035, 0.035)))

    def pith_disp(P):
        return -0.004 * sdf.ridged(P * np.array([0.15, 1.0, 1.0]), 60, 2, seed=7561 + v)

    groups.append({'name': f'mark{v}', 'merge': 'mark', 'fm': 'ent_wr_pith', 'parts': pith, 'disp': pith_disp, 'h': 0.007,
                   'cav_scale': 0.01, 'tris': 380, 'clones': clones})
    if seeds:
        groups += seed_groups(v, xa, bone, clones, rng, R)
    return groups


def seed_groups(v, xa, bone, clones, rng, R):
    """Traechtiges Weibchen: die Eier wachsen als Samen am Leib - Buckel voller Gruebchen, in jedem ein dunkler Same."""
    pods, holes = [], []
    th0 = rng.uniform(0, 2 * math.pi)
    for i in range(int(rng.integers(7, 12))):
        th = th0 + rng.normal(0, 0.45)
        xr = rng.uniform(0.05, 0.5)
        rad = float(np.interp(xr, RING_XS, R))
        n = np.array([0.0, math.cos(th), math.sin(th)])
        c = np.array([xa + xr, 0, AZ]) + n * (rad + 0.01)
        s = rng.uniform(0.045, 0.085)
        pods.append(A.ell(bone, c, n, (s * 0.7, s, s), up=X, k=0.025))
        t1 = A.perp(n, X)
        t2 = np.cross(n, t1)
        for j in range(int(rng.integers(3, 7))):
            a = rng.uniform(0, 2 * math.pi)
            q = c + n * (s * 0.62) + (t1 * math.cos(a) + t2 * math.sin(a)) * s * rng.uniform(0.0, 0.55)
            hr = s * rng.uniform(0.16, 0.26)
            pods.append(A.ell(bone, q, n, (hr, hr, hr), op='sub', k=0.006))
            holes.append(A.ell(bone, q - n * hr * 0.55, n, (hr * 0.7, hr * 0.7, hr * 0.7)))
    return [{'name': f'samen{v}', 'merge': 'samen', 'fm': 'ent_wr_seed', 'parts': pods, 'disp': None, 'h': 0.005, 'cav_scale': 0.01,
             'tris': 900, 'clones': clones},
            {'name': f'kern{v}', 'merge': 'samenkern', 'fm': 'ent_wr_gut', 'parts': holes, 'disp': None, 'h': 0.005, 'tris': 300,
             'clones': clones}]


# --- Hals ------------------------------------------------------------------------------------------------------
def neck_groups(fem):
    """Hals: dieselbe Haut, glatt vom Leib (0,72 m) zur Schaedelbasis (0,6 m); je Halsknochen ein Abschnitt, weich verbunden."""
    skin, gut = [], []
    for i in range(NN):
        x0, x1 = 9.5 + i * 0.5, 9.5 + (i + 1) * 0.5
        xs = np.linspace(x0 - (0.1 if i else 0.3), x1 + (0.1 if i < NN - 1 else 0.3), 30)
        t = (xs - 9.5) / 3.0
        base = 0.72 - 0.12 * smooth01(t)
        # flache Ringfurchen wie am Leib
        rs = base - 0.012 * np.exp(-(((xs - 9.5) % 0.5 - 0.25) / 0.05) ** 2)
        skin.append(Rev(B_NECK0 + i, xs, rs, shell=0.032, k=0.04))
        gx = np.linspace(x0, x1, 20)
        gut.append(Rev(B_NECK0 + i, gx, 0.16 + 0.04 * np.sin((gx - x0) * 12.0), z0=AZ - 0.04, k=0.05))
    return [{'name': 'hals', 'fm': 'ent_wr_membrane_f' if fem else 'ent_wr_membrane', 'parts': skin, 'disp': None, 'h': 0.0085,
             'cav_scale': 0.012, 'tris': 7000},
            {'name': 'halsdarm', 'fm': 'ent_wr_gut', 'parts': gut, 'disp': lambda P: -0.005 * sdf.fbm(P, 12, 2, seed=7571),
             'h': 0.014, 'tris': 1000}]


# --- Kopf des Maennchens -----------------------------------------------------------------------------------------
def surf_x(parts, y, z, x_from=14.2, x_to=12.4):
    """Erste Stelle (von vorn), an der der Kopf bei (y, z) beginnt."""
    xs = np.linspace(x_from, x_to, 600)
    P = np.stack([xs, np.full_like(xs, y), np.full_like(xs, z)], axis=1)
    d = sdf.sample(parts, P)
    i = np.nonzero(d < 0)[0]
    return float(xs[i[0]]) if len(i) else x_to


def smile_z(y):
    """Laechellinie: Mitte tief, die Winkel ziehen weit hinauf in die Wangen (zu breit, bis an die Seiten des Kopfs)."""
    t = np.abs(y) / 0.64
    return 0.85 + 0.06 * t ** 2 + 0.5 * np.maximum(0.0, t - 0.78) ** 2


def mouth_h(y):
    """Hoehe der Mundoeffnung (die Lippen weit von den Zaehnen zurueckgezogen), zu den Winkeln schmal."""
    t = np.clip(np.abs(y) / 0.66, 0.0, 1.0)
    return 0.125 * (1.0 - t ** 1.7) + 0.014


def male_head(rng):
    skull = [A.ell(B_SKULL, (12.93, 0, 1.08), X, (0.64, 0.66, 0.46), k=0.0),          # Hirnschaedel, breit
             A.ell(B_SKULL, (13.22, 0, 1.04), X, (0.34, 0.6, 0.42), k=0.14),          # Gesichtsflaeche
             A.cone(B_SKULL, (12.45, 0, 1.0), (12.75, 0, 1.03), 0.6, 0.64, k=0.12)]   # Uebergang zum Hals
    jaw = [A.ell(B_JAW, (12.98, 0, 0.8), X, (0.6, 0.6, 0.25), k=0.1),                  # Unterkiefer, breit
           A.ell(B_JAW, (13.4, 0, 0.66), X, (0.14, 0.24, 0.13), k=0.08)]               # Kinn
    for s in (1, -1):  # Hamsterbacken, die am Kiefer haengen
        jaw.append(A.ell(B_JAW, (13.12, s * 0.5, 0.74), (1, s * 0.4, -0.2), (0.16, 0.12, 0.12), k=0.08))
    body = skull + jaw
    feat, cuts = [], []

    def SX(y, z, parts=None):
        return surf_x(parts or body, y, z)

    # Brauen: zwei Wuelste, innen hochgezogen (bang, flehend) - ueber einem grinsenden Mund
    for s in (1, -1):
        a = np.array([SX(s * 0.07, 1.31) - 0.03, s * 0.07, 1.31])
        b = np.array([SX(s * 0.36, 1.22) - 0.05, s * 0.36, 1.22])
        feat.append(A.ell(B_SKULL, 0.5 * (a + b), b - a, (np.linalg.norm(b - a) * 0.62, 0.05, 0.045), up=(1, 0, 0.6), k=0.05))
    # Wangen: hochgedrueckt vom Grinsen, Jochbeine
    for s in (1, -1):
        feat.append(A.ell(B_SKULL, (SX(s * 0.4, 0.98) - 0.06, s * 0.4, 0.98), (1, s * 0.3, 0.2), (0.15, 0.16, 0.13), k=0.08))
        feat.append(A.ell(B_SKULL, (SX(s * 0.52, 0.95) - 0.08, s * 0.52, 0.95), (1, s * 0.6, 0.2), (0.12, 0.12, 0.11), k=0.07))
        feat.append(A.ell(B_SKULL, (SX(s * 0.42, 1.14) - 0.05, s * 0.42, 1.14), X, (0.09, 0.12, 0.05), k=0.05))
    face = body + feat
    for s in (1, -1):
        ey = np.array([SX(s * 0.2, 1.155, face), s * 0.2, 1.155])
        feat.append(A.ell(B_SKULL, ey + np.array([-0.025, 0, -0.06]), (1, s * 0.2, 0), (0.05, 0.1, 0.035), k=0.03))  # Traenensack
        feat.append(A.ell(B_SKULL, ey + np.array([-0.02, s * 0.01, 0.05]), (1, s * 0.15, -0.3), (0.06, 0.115, 0.03), k=0.025))  # Oberlid
    face = body + feat
    # Nase: schmaler Ruecken, knollige Spitze, Nasenfluegel
    nb = np.array([SX(0, 1.2, face) - 0.01, 0, 1.2])
    ntx = SX(0, 1.0, face)
    nt = np.array([ntx + 0.07, 0, 1.0])
    feat.append(A.cone(B_SKULL, nb, nt - np.array([0.02, 0, -0.02]), 0.03, 0.045, k=0.04))
    feat.append(A.ell(B_SKULL, nt, (1, 0, -0.3), (0.055, 0.058, 0.05), k=0.03))
    for s in (1, -1):
        feat.append(A.ell(B_SKULL, nt + np.array([-0.055, s * 0.06, -0.02]), (1, s * 0.5, 0), (0.045, 0.035, 0.032), k=0.03))
    face = body + feat
    for s in (1, -1):
        cuts.append(A.ell(B_SKULL, nt + np.array([-0.03, s * 0.042, -0.04]), (0.4, 0, -1), (0.022, 0.017, 0.013), op='sub', k=0.008))
        # Augenhoehlen: tief - das Auge liegt weit hinten im Schatten
        ey = np.array([SX(s * 0.2, 1.155, face), s * 0.2, 1.155])
        cuts.append(A.ell(B_SKULL, ey, (1, s * 0.25, 0), (0.085, 0.085, 0.052), op='sub', k=0.03))
        # Kraehenfuesse am aeusseren Augenwinkel, Furche zwischen den Brauen
        for k in range(3):
            a0 = np.array([0, s * 0.3, 1.16 + 0.03 * (k - 1)])
            a1 = np.array([0, s * 0.42, 1.16 + 0.06 * (k - 1)])
            a0[0] = SX(a0[1], a0[2], face) + 0.006
            a1[0] = SX(a1[1], a1[2], face) + 0.006
            cuts.append(A.cone(B_SKULL, a0, a1, 0.009, 0.004, op='sub', k=0.006))
        g0 = np.array([0, s * 0.035, 1.2])
        g1 = np.array([0, s * 0.045, 1.3])
        g0[0], g1[0] = SX(g0[1], g0[2], face) + 0.006, SX(g1[1], g1[2], face) + 0.006
        cuts.append(A.cone(B_SKULL, g0, g1, 0.012, 0.008, op='sub', k=0.008))
        # Nasolabialfalte: tiefe Rinne vom Nasenfluegel weit hinaus, um den Mundwinkel herum
        pts = A.bezier(np.array([0, s * 0.1, 1.0]), np.array([0, s * 0.36, 1.0]), np.array([0, s * 0.62, float(smile_z(0.62)) - 0.04]), 10)
        for i in range(len(pts) - 1):
            a, b = pts[i].copy(), pts[i + 1].copy()
            a[0] = SX(a[1], a[2], face) + 0.006
            b[0] = SX(b[1], b[2], face) + 0.006
            cuts.append(A.cone(B_SKULL, a, b, 0.02 + 0.006 * i / 10, 0.02 + 0.006 * (i + 1) / 10, op='sub', k=0.014))
    # Stirnfalten
    for zz in (1.37, 1.42, 1.47):
        prev = None
        for y in np.linspace(-0.32, 0.32, 9):
            p = np.array([0, y, zz + 0.02 * (y / 0.32) ** 2])
            p[0] = SX(p[1], p[2], face) + 0.005
            if prev is not None:
                cuts.append(A.cone(B_SKULL, prev, p, 0.008, 0.008, op='sub', k=0.008))
            prev = p
    # Philtrum, Kinngruebchen
    cuts.append(A.cone(B_SKULL, np.array([SX(0, 0.97, face), 0, 0.97]), np.array([SX(0, 0.93, face), 0, 0.93]), 0.012, 0.014, op='sub', k=0.01))
    cuts.append(A.ell(B_JAW, (SX(0, 0.64, face) + 0.01, 0, 0.64), X, (0.02, 0.014, 0.03), op='sub', k=0.01))
    # Mund: weit offenes Grinsen, die Lippen von den Zaehnen zurueckgezogen - Oeffnung, Lippen, Zahnfleisch, zwei Zahnreihen
    ys = np.linspace(-0.63, 0.63, 47)
    open_, lip_u, lip_l, gums_u, gums_l = [], [], [], [], []
    prev = None
    for y in ys:
        z = float(smile_z(y))
        x = SX(y, z, face)
        p = np.array([x, y, z])
        h = float(mouth_h(y))
        if x < 12.75:  # dort trifft die Linie den Kopf nicht mehr (kein Stab neben dem Kopf)
            prev = None
            continue
        if prev is not None:
            pp, ph = prev
            nrm = A.unit(np.array([1.0, 0.5 * (p[1] + pp[1]) * 1.1, 0.0]))
            open_.append(A.tube(B_SKULL, pp - nrm * 0.02, p - nrm * 0.02, (0.13, ph * 0.5), (0.13, h * 0.5), up=Z, op='sub', k=0.01))
            w = 1.0 - 0.5 * (abs(y) / 0.68) ** 2
            lip_u.append(A.cone(B_SKULL, pp + Z * (ph * 0.5 + 0.004) - nrm * 0.012, p + Z * (h * 0.5 + 0.004) - nrm * 0.012,
                                0.024 * w, 0.024 * w, k=0.025))
            lip_l.append(A.cone(B_JAW, pp - Z * (ph * 0.5 + 0.006) - nrm * 0.012, p - Z * (h * 0.5 + 0.006) - nrm * 0.012,
                                0.028 * w, 0.028 * w, k=0.025))
            gums_u.append(A.cone(B_SKULL, pp + Z * (ph * 0.5 - 0.008) - nrm * 0.05, p + Z * (h * 0.5 - 0.008) - nrm * 0.05, 0.026, 0.026, k=0.02))
            gums_l.append(A.cone(B_JAW, pp - Z * (ph * 0.5 - 0.008) - nrm * 0.05, p - Z * (h * 0.5 - 0.008) - nrm * 0.05, 0.026, 0.026, k=0.02))
        prev = (p, h)
    # Mundhoehle: hinter den Zaehnen weit (klappt der Kiefer auf, sieht man hinein)
    cav = [A.ell(B_SKULL, (13.0, 0, 0.86), X, (0.42, 0.5, 0.13), op='sub', k=0.03)]
    head = skull + jaw + feat + lip_u + lip_l + cuts + open_ + cav

    def hide_disp(P):
        wr = sdf.ridged(P * np.array([0.6, 1.0, 0.8]), 11, 3, seed=7581)
        return (-0.006 * smooth01((wr - 0.5) / 0.35) - 0.0025 * sdf.fbm(P, 28, 3, seed=7582)
                - 0.0012 * sdf.fbm(P, 80, 2, seed=7583))

    # Zaehne: viel zu viele Menschenzaehne entlang des ganzen Grinsens - vorn Schneidezaehne, Eckzaehne, dann Backenzaehne
    # bis in die Winkel; oben haengen sie vom Zahnfleisch, unten stehen sie; dahinter eine zweite, schiefe Reihe
    teeth = []
    n_side = 17
    for row, bone in (('u', B_SKULL), ('l', B_JAW)):
        for s in (1, -1):
            for i in range(n_side):
                t = (i + 0.5) / n_side
                y = s * 0.6 * t ** 0.92
                if i > 3 and rng.random() < 0.1:  # Luecke (ausgefallen)
                    continue
                z = float(smile_z(y))
                h = float(mouth_h(y))
                x = SX(y, z, face) - 0.045 - 0.012 * t - (0.012 if rng.random() < 0.25 else 0.0)
                nrm = A.unit(np.array([1.0, y * 1.1, 0.0]))
                wid = (0.027 if i < 2 else (0.022 if i < 3 else 0.026)) * (1.0 - 0.3 * t)
                top = z + h * 0.5 + 0.01 if row == 'u' else z - h * 0.5 - 0.01
                tip = z - 0.006 if row == 'u' else z + 0.004
                if i == 2:  # Eckzahn etwas laenger
                    tip += -0.012 if row == 'u' else 0.012
                tip += rng.uniform(-0.006, 0.006)
                c = np.array([x, y, 0.5 * (top + tip)])
                tilt = A.unit(Z + nrm * rng.uniform(-0.15, 0.15) + np.cross(Z, nrm) * rng.uniform(-0.1, 0.1))
                teeth.append(A.box(bone, c, nrm, (0.016, wid * 0.82, abs(top - tip) * 0.5), 0.009, up=tilt, k=0.0))
        # zweite Reihe dahinter (gedraengt, schief)
        for k in range(12):
            y = rng.uniform(-0.5, 0.5)
            z = float(smile_z(y))
            h = float(mouth_h(y))
            x = SX(y, z, face) - 0.085
            nrm = A.unit(np.array([1.0, y * 1.1, 0.0]))
            top = z + h * 0.5 + 0.01 if row == 'u' else z - h * 0.5 - 0.01
            tip = z + (0.01 if row == 'u' else -0.01)
            c = np.array([x, y, 0.5 * (top + tip)])
            teeth.append(A.box(bone, c, A.unit(nrm + rng.normal(0, 0.3, 3)), (0.014, 0.018, abs(top - tip) * 0.5), 0.008, up=Z, k=0.0))
    # Maul innen: Zahnfleisch, Auskleidung der Hoehle (oben am Schaedel, unten am Kiefer), Zunge
    mouth = gums_u + gums_l + [A.ell(B_SKULL, (13.0, 0, 0.905), X, (0.41, 0.49, 0.085), k=0.02),
                               A.ell(B_JAW, (13.0, 0, 0.815), X, (0.41, 0.49, 0.085), k=0.04),
                               A.ell(B_SKULL, (12.98, 0, 0.86), X, (0.40, 0.48, 0.12), op='sub', k=0.0),
                               A.ell(B_JAW, (13.08, 0, 0.775), X, (0.27, 0.22, 0.05), k=0.05)]
    # Augen: klein, tief in den Hoehlen, leuchtend weiss (die Seite: sie sehen damit im Dunkeln)
    eyes = []
    for s in (1, -1):
        ey = np.array([SX(s * 0.2, 1.155, face) - 0.055, s * 0.2, 1.155])
        eyes.append(A.ell(B_SKULL, ey, X, (0.035, 0.038, 0.032)))
    return [
        {'name': 'kopf', 'fm': 'ent_wr_hide', 'parts': head, 'disp': hide_disp, 'h': 0.0055, 'cav_scale': 0.014, 'tris': 16000},
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': lambda P: -0.0015 * sdf.fbm(P, 90, 2, seed=7584), 'h': 0.0028,
         'cav_scale': 0.004, 'tris': 7000},
        {'name': 'maul', 'fm': 'ent_mouth', 'parts': mouth, 'disp': lambda P: -0.004 * sdf.fbm(P, 30, 2, seed=7585), 'h': 0.006,
         'tris': 2600},
        {'name': 'augen', 'fm': 'ent_glow_eye', 'parts': eyes, 'disp': None, 'h': 0.003, 'tris': 400},
    ]


# --- Kopf des Weibchens -----------------------------------------------------------------------------------------
def female_head(rng):
    head = [A.ell(B_SKULL, (12.95, 0, 1.03), X, (0.6, 0.6, 0.5), k=0.0),
            A.ell(B_SKULL, (13.35, 0, 1.07), X, (0.24, 0.42, 0.32), k=0.12),           # Kopflappen
            A.cone(B_SKULL, (12.45, 0, 1.0), (12.75, 0, 1.03), 0.6, 0.6, k=0.12)]
    # Mundoeffnung vorn unten: weit, rund, wulstig; tiefer Schlund mit Zahnkraenzen
    head.append(Part_torus(B_SKULL, (13.46, 0, 0.88), X, 0.22, 0.07, k=0.06))
    head.append(A.ell(B_SKULL, (13.52, 0, 0.88), X, (0.3, 0.2, 0.2), op='sub', k=0.03))
    # Taster: kurze, fleischige Fortsaetze um den Mund (oben und unten)
    for a in (1.2, 1.94, 4.4, 5.02):
        b0 = np.array([13.5, 0.27 * math.cos(a), 0.88 + 0.27 * math.sin(a)])
        b1 = b0 + np.array([0.14, 0.06 * math.cos(a), 0.06 * math.sin(a) - 0.03])
        head.append(A.cone(B_SKULL, b0, b1, 0.045, 0.018, k=0.04))
    # Ringfurchen am Kopf (wurmartig)
    cuts = []
    for xx in (12.62, 12.82, 13.0):
        cuts.append(Part_torus(B_SKULL, (xx, 0, 1.03), X, 0.62, 0.018, op='sub', k=0.01))
    core = [A.ell(B_SKULL, (12.9, 0, 1.08), X, (0.36, 0.32, 0.26), k=0.0),             # dunkler Knoten (Hirn) im Kopf
            A.cone(B_SKULL, (12.9, 0, 1.0), (13.3, 0, 0.88), 0.16, 0.12, k=0.08)]
    for s in (1, -1):  # Muskelstraenge zu den Zangen
        core.append(A.cone(B_SKULL, (12.95, s * 0.12, 0.95), (13.3, s * 0.3, 0.85), 0.09, 0.07, k=0.05))
    throat = [A.ell(B_SKULL, (13.3, 0, 0.88), X, (0.14, 0.17, 0.17))]
    ring_teeth = []
    for ring, (rx, rr, n) in enumerate(((13.47, 0.17, 18), (13.4, 0.13, 14))):
        for i in range(n):
            a = 2 * math.pi * (i + 0.5 * ring) / n
            p = np.array([rx, rr * math.cos(a), 0.88 + rr * math.sin(a)])
            ring_teeth.append(A.cone(B_SKULL, p, p + np.array([-0.05, -0.1 * math.cos(a), -0.1 * math.sin(a)]), 0.018, 0.002))
    # Zangen: schwer, sichelfoermig nach innen gebogen (geschlossen kreuzen sich die Spitzen), innen gezahnt, glaenzendes Chitin
    pinc = []
    for b, s in ((B_PL, 1), (B_PR, -1)):
        p0 = np.array([13.3, s * 0.34, 0.86])
        mid = np.array([13.9, s * 0.56, 0.86])
        tip = np.array([14.36, s * -0.04, 0.8])
        pts = A.bezier(p0, mid, tip, 14)
        rad = [0.12 * (1 - i / 15) ** 0.75 + 0.007 for i in range(15)]
        pinc += A.chain(lambda i, b=b: b, pts, rad, k=0.025)
        pinc.append(A.ell(b, p0, A.unit(mid - p0), (0.16, 0.13, 0.11), k=0.05))
        for i in range(3, 13):
            q = pts[i]
            tang = A.unit(pts[i + 1] - pts[i - 1])
            inward = A.unit(np.cross(tang, np.array([0, 0, 1.0])) * s)
            if inward[1] * s > 0:
                inward = -inward
            ln = (0.07 - 0.004 * i) * (1.3 if i % 3 == 0 else 1.0)
            pinc.append(A.cone(b, q + inward * rad[i] * 0.6, q + inward * (rad[i] * 0.6 + ln) + tang * 0.02, 0.02, 0.002, k=0.008))
    eyes = []
    for s in (1, -1):
        eyes.append(A.ell(B_SKULL, (13.47, s * 0.22, 1.2), (1, s * 0.4, 0.2), (0.04, 0.045, 0.04)))
        for j in range(2):
            eyes.append(A.ell(B_SKULL, (13.32 - 0.12 * j, s * (0.4 + 0.05 * j), 1.27 - 0.03 * j), (1, s, 0.5), (0.02, 0.02, 0.02)))
    head_parts = head + cuts

    def memb_disp(P):
        vein = sdf.ridged(P * np.array([0.6, 1.0, 1.0]), 9, 3, seed=7591)
        return -0.0022 * smooth01((vein - 0.72) / 0.2) + 0.0008 * sdf.fbm(P, 30, 2, seed=7592)

    return [
        {'name': 'kopf', 'fm': 'ent_wr_membrane_f', 'parts': head_parts, 'disp': memb_disp, 'h': 0.007, 'cav_scale': 0.014, 'tris': 12000},
        {'name': 'kopfkern', 'fm': 'ent_wr_gut', 'parts': core, 'disp': lambda P: -0.006 * sdf.fbm(P, 12, 3, seed=7593), 'h': 0.012,
         'tris': 1500},
        {'name': 'schlund', 'fm': 'ent_mouth', 'parts': throat, 'disp': None, 'h': 0.008, 'tris': 400},
        {'name': 'ringzaehne', 'fm': 'ent_tooth', 'parts': ring_teeth, 'disp': None, 'h': 0.003, 'tris': 800},
        {'name': 'zangen', 'fm': 'ent_wr_pincer', 'parts': pinc, 'disp': lambda P: -0.0015 * sdf.fbm(P, 60, 2, seed=7594), 'h': 0.004,
         'cav_scale': 0.006, 'tris': 6000},
        {'name': 'augen', 'fm': 'ent_glow_eye', 'parts': eyes, 'disp': None, 'h': 0.003, 'tris': 400},
    ]


def Part_torus(bone, c, axis, major, minor, op='add', k=0.0):
    """Ring um die Achse 'axis' (lokales z des Torus)."""
    R = sdf.frame_from(A.perp(axis, Z if abs(np.asarray(axis) @ Z) < 0.9 else X), axis)
    return sdf.Part('torus', bone, k=k, op=op, c=np.asarray(c, float), R=R, major=major, minor=minor)


# --- Verschlucktes -----------------------------------------------------------------------------------------------
def item_groups(rng):
    bones, cloth_a, cloth_b = [], [], []
    # 0: Brustkorb mit Wirbelsaeule und Schaedel (ein Mensch, halb verdaut), zusammengekruemmt
    c = np.array([-6.0, 0.0, AZ - 0.08])
    b = B_ITEM0
    spine = A.bezier(c + np.array([-0.28, 0, -0.05]), c + np.array([0, 0, 0.1]), c + np.array([0.26, 0.04, 0.02]), 12)
    for i in range(len(spine) - 1):
        bones.append(A.ell(b, spine[i], A.unit(spine[i + 1] - spine[i]), (0.022, 0.03, 0.024), k=0.008))
    for i in range(9):
        t = 0.15 + 0.07 * i
        p = spine[min(11, int(t * 12))]
        for s in (1, -1):
            arc = A.bezier(p, p + np.array([0.02, s * 0.13, -0.06]), p + np.array([0.04, s * 0.08, -0.17]), 6)
            bones += A.chain(lambda i, b=b: b, arc, [0.011] * 7, k=0.004)
    sk = c + np.array([0.36, 0.06, 0.04])
    bones.append(A.ell(b, sk, (1, 0.2, 0.1), (0.1, 0.075, 0.085), k=0.0))
    bones.append(A.ell(b, sk + np.array([0.075, 0, -0.07]), X, (0.05, 0.05, 0.03), k=0.02))
    for s in (1, -1):
        bones.append(A.ell(b, sk + np.array([0.085, s * 0.032, 0.0]), X, (0.025, 0.022, 0.02), op='sub', k=0.004))
    # 2: ein Arm (Elle, Speiche, Handknochen)
    c = np.array([2.0, 0.0, AZ - 0.08])
    b = B_ITEM0 + 2
    e = c + np.array([-0.2, 0.02, 0.0])
    w = c + np.array([0.12, -0.03, 0.03])
    bones.append(A.cone(b, c + np.array([-0.5, 0.06, 0.03]), e, 0.025, 0.02, k=0.01))   # Oberarm
    bones.append(A.ell(b, e, X, (0.03, 0.03, 0.03), k=0.01))
    bones.append(A.cone(b, e, w, 0.014, 0.012, k=0.006))
    bones.append(A.cone(b, e + np.array([0, 0.02, 0]), w + np.array([0, 0.025, 0]), 0.012, 0.014, k=0.006))
    for f in range(5):
        a = (f - 2) * 0.25
        d = A.unit(np.array([math.cos(a), math.sin(a), -0.15]))
        pts = [w + np.array([0, 0.012, 0]) + d * 0.03, w + d * 0.1, w + d * 0.15 + np.array([0, 0, -0.03])]
        bones += A.chain(lambda i, b=b: b, pts, [0.007, 0.006, 0.004], k=0.003)
    # 1: Rucksack mit Riemen
    c = np.array([-2.0, 0.0, AZ - 0.04])
    b = B_ITEM0 + 1
    cloth_b.append(A.box(b, c, (1, 0.15, 0.1), (0.17, 0.12, 0.22), 0.05, k=0.0))
    cloth_b.append(A.box(b, c + np.array([0.0, 0.0, -0.17]), (1, 0.15, 0.1), (0.13, 0.13, 0.05), 0.04, k=0.03))
    for s in (1, -1):
        cloth_b.append(A.cone(b, c + np.array([0.06, s * 0.07, 0.2]), c + np.array([0.12, s * 0.09, -0.12]), 0.016, 0.016, k=0.01))
    # 3: Stiefel
    c = np.array([6.0, 0.0, AZ - 0.1])
    b = B_ITEM0 + 3
    cloth_a.append(A.cone(b, c + np.array([0, 0, 0.0]), c + np.array([0, 0, 0.22]), 0.055, 0.06, k=0.0))
    cloth_a.append(A.ell(b, c + np.array([0.09, 0, -0.03]), X, (0.15, 0.055, 0.045), k=0.03))
    cloth_a.append(A.box(b, c + np.array([0.06, 0, -0.07]), X, (0.17, 0.06, 0.012), 0.006, k=0.01))
    return [
        {'name': 'beute_knochen', 'fm': 'ent_wr_bone', 'parts': bones, 'disp': lambda P: -0.002 * sdf.fbm(P, 50, 2, seed=7601),
         'h': 0.004, 'cav_scale': 0.006, 'tris': 3000},
        {'name': 'beute_rucksack', 'fm': 'ent_cloth_b', 'parts': cloth_b, 'disp': lambda P: -0.004 * sdf.fbm(P, 20, 3, seed=7602),
         'h': 0.008, 'tris': 800},
        {'name': 'beute_stiefel', 'fm': 'ent_cloth_a', 'parts': cloth_a, 'disp': lambda P: -0.002 * sdf.fbm(P, 30, 2, seed=7603),
         'h': 0.006, 'tris': 600},
    ]


# --- Kraenze -----------------------------------------------------------------------------------------------------
def collar_groups(rng):
    """Kranz 0 bei (-12, 0, 0) flach am Boden (die Flaeche ist z = 0), geklont auf die Kraenze 1 und 2. Wo der Leib durchbricht:
    aufgeworfene Platten der Flaeche (schraeg vom Loch weg gekippt), ein dunkler Spalt um den Leib, Risse, Brocken, nasse
    Flecken und Schleimfaeden, Haeufchen aus Staub und Splitt."""
    c = np.array([-12.0, 0.0, 0.0])
    b = B_COL0
    clones = [((-3.0 * k, 0.0, 0.0), B_COL0 + k) for k in (1, 2)]
    # Platten: Stuecke der Flaeche, vom Loch weg schraeg aufgestellt (der Leib hat sie hochgedrueckt)
    slabs = []
    n = 8
    a0 = rng.uniform(0, 2 * math.pi)
    for i in range(n):
        a = a0 + 2 * math.pi * i / n + rng.uniform(-0.25, 0.25)
        rad = rng.uniform(0.95, 1.2)
        d = np.array([math.cos(a), math.sin(a), 0.0])
        tilt = rng.uniform(0.18, 0.55)
        size = rng.uniform(0.22, 0.34)
        up = A.unit(Z * math.cos(tilt) - d * math.sin(tilt))  # innere Kante hoch
        p = c + d * rad + Z * (math.sin(tilt) * size * 0.6 - 0.02)
        side = np.cross(Z, d)
        slabs.append(A.box(b, p, A.unit(d * math.cos(tilt) + Z * math.sin(tilt)), (size, size * rng.uniform(0.6, 0.9), 0.035), 0.008,
                           up=up, k=0.0))
    # Brocken
    for i in range(22):
        a = rng.uniform(0, 2 * math.pi)
        r = min(1.75, 0.82 + abs(rng.normal(0, 0.32)))
        p = c + np.array([math.cos(a), math.sin(a), 0]) * r
        s = rng.uniform(0.02, 0.07)
        slabs.append(A.box(b, p + Z * (s * 0.25), A.unit(rng.normal(0, 1, 3)), (s, s * rng.uniform(0.5, 1.0), s * rng.uniform(0.35, 0.7)), s * 0.15,
                           up=A.unit(rng.normal(0, 1, 3)), k=0.0))
    # dunkler Spalt rund um den Leib
    gap = [A.ell(b, c + Z * 0.003, X, (0.9, 0.9, 0.006), k=0.0), A.ell(b, c, X, (0.7, 0.7, 0.03), op='sub', k=0.01)]
    # Risse: duenne dunkle Baender, zackig nach aussen
    for i in range(12):
        a = rng.uniform(0, 2 * math.pi)
        p = c + np.array([math.cos(a), math.sin(a), 0]) * 0.85
        L = rng.uniform(0.5, 1.2)
        for j in range(5):
            a += rng.normal(0, 0.2)
            q = p + np.array([math.cos(a), math.sin(a), 0]) * (L / 5)
            w = 0.012 * (1 - j / 5) + 0.003
            gap.append(A.box(b, 0.5 * (p + q) + Z * 0.002, A.unit(q - p), (np.linalg.norm(q - p) * 0.5 + 0.005, w, 0.004), 0.0))
            if rng.random() < 0.35:
                a2 = a + rng.choice([-1, 1]) * rng.uniform(0.5, 1.0)
                q2 = q + np.array([math.cos(a2), math.sin(a2), 0]) * L / 5 * 0.8
                gap.append(A.box(b, 0.5 * (q + q2) + Z * 0.002, A.unit(q2 - q), (np.linalg.norm(q2 - q) * 0.5, w * 0.6, 0.004), 0.0))
            p = q
    # nasse Flecken und Faeden (Schleim), unregelmaessig
    slime = []
    for i in range(9):
        a = rng.uniform(0, 2 * math.pi)
        p = c + np.array([math.cos(a), math.sin(a), 0]) * rng.uniform(0.72, 1.05)
        slime.append(A.ell(b, p + Z * 0.004, (math.cos(a), math.sin(a), 0), (rng.uniform(0.1, 0.22), rng.uniform(0.06, 0.14), 0.01), k=0.05))
    for i in range(5):  # Faeden vom Leib zum Boden
        a = rng.uniform(0, 2 * math.pi)
        p0 = c + np.array([math.cos(a), math.sin(a), 0]) * 0.74 + Z * rng.uniform(0.15, 0.4)
        p1 = c + np.array([math.cos(a), math.sin(a), 0]) * rng.uniform(0.9, 1.1)
        slime += A.chain(lambda i: b, [p0, 0.5 * (p0 + p1) - Z * 0.05, p1], [0.008, 0.005, 0.012], k=0.01)
    # Staub und Splitt: flache, unruhige Haeufchen
    dust = []
    for i in range(7):
        a = rng.uniform(0, 2 * math.pi)
        p = c + np.array([math.cos(a), math.sin(a), 0]) * rng.uniform(0.95, 1.5)
        dust.append(A.ell(b, p, (math.cos(a), math.sin(a), 0), (rng.uniform(0.15, 0.3), rng.uniform(0.1, 0.2), rng.uniform(0.02, 0.05)), k=0.06))
    dust.append(A.box(b, c - Z * 0.2, X, (2.0, 2.0, 0.2), 0.0, op='sub', k=0.0))

    def dust_disp(P):
        return -0.012 * sdf.fbm(P, 9, 3, seed=7611) - 0.005 * sdf.fbm(P, 30, 2, seed=7612)

    return [
        {'name': 'kranz_platten', 'fm': 'ent_wr_debris', 'parts': slabs, 'disp': lambda P: -0.004 * sdf.fbm(P, 30, 2, seed=7613),
         'h': 0.006, 'tris': 2600, 'clones': clones},
        {'name': 'kranz_spalt', 'fm': 'ent_wr_crack', 'parts': gap, 'disp': None, 'h': 0.003, 'tris': 1400, 'clones': clones},
        {'name': 'kranz_schleim', 'fm': 'ent_wr_slime', 'parts': slime, 'disp': None, 'h': 0.005, 'tris': 1200, 'clones': clones},
        {'name': 'kranz_staub', 'fm': 'ent_wr_dust', 'parts': dust, 'disp': dust_disp, 'h': 0.008, 'tris': 900, 'clones': clones},
    ]


def build(J, bones):
    rng = np.random.default_rng(75)
    groups = []
    for v in range(4):
        groups += ring_groups(v, FEMALE, SEEDS, np.random.default_rng(750 + v))
    groups += neck_groups(FEMALE)
    groups += female_head(rng) if FEMALE else male_head(rng)
    groups += item_groups(np.random.default_rng(760))
    groups += collar_groups(np.random.default_rng(770))
    return groups
