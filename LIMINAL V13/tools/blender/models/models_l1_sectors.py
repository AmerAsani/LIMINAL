"""V13.3: Level 1 - Ausstattung der Sektoren Ouroboros (Baustelle), Garden (ueberwuchert) und Fabled (Neon ueber altem Holz).

Wiki (Level 1, Abschnitt Sectors, docs: PROPS_Level1_Sektoren.md):
- Ouroboros: kleinster Sektor, ewige Baustelle; schattenhafte Arbeiter in Overalls reparieren seit Jahrzehnten ohne
  Fortschritt, ihre Maschinen verletzen manchmal Menschen -> Geruest, halbe Mauern, Material, Maschinen, Absperrungen.
- Garden: ueppig ueberwuchert -> Ranken, Efeu, Moos, Farne, Wurzeln, Straeucher, junge Baeume durch den Betonboden.
- Fabled: einst menschliche Hochburg mit Holz- und Zinnoberrot-Ausstattung, heute voller Neonleuchten und farbiger Kabel.

Konventionen (common.py): Meter, z oben, Blick entlang +x.
- Bodenmodelle: Ursprung in der Mitte des Fusspunkts (z = 0 = Boden).
- Wandmodelle: Rueckseite (Wandflaeche) bei x = 0. Ursprung am Boden (formwork, ivy_wall, panel_vermilion) bzw. in der
  Mitte der Leuchtform (neon_tube, neon_sign - die Karte setzt z = Montagehoehe, wie bei Schildern und Bildern).
- Deckenmodelle: Oberseite bei z = 0, haengen nach unten (vines_ceiling, cable_hang).
Texturen projiziert das Spiel aus Weltkoordinaten (shaders/props_gbuffer.hlsl) - keine UVs noetig. Die Ausstattung wird
einseitig gezeichnet (Rueckseiten entfallen); Blaetter, Planen und Folien sind deshalb zweiseitig (zweite Flaeche mit
umgekehrtem Umlauf). Ausschnitte (Alpha) gibt es nur im Schilderatlas - Blaetter sind echte Geometrie.
"""
import math
import random

import bmesh
import bpy  # noqa: F401
from mathutils import Euler, Matrix, Vector

import common
from common import box, material, profile_extrude, shade_auto  # noqa: F401

V = Vector
TAU = 2.0 * math.pi


# =====================================================================================================================
# Hilfen: Geometrie wird je (Material, Glaettung) in einem bmesh gesammelt und am Ende als ein Objekt angelegt.
# =====================================================================================================================
class Kit:
    def __init__(self):
        self.parts = {}

    def __call__(self, mat, smooth=0.0, weld=False):
        """bmesh fuer Material mat; smooth > 0: weiche Normalen bis zu diesem Kantenwinkel (Grad);
        weld: deckungsgleiche Punkte am Ende verschmelzen (Flaechen aus emit() werden wieder zusammenhaengend)."""
        k = (mat.name, smooth, weld)
        if k not in self.parts:
            self.parts[k] = (mat, bmesh.new(), smooth, weld)
        return self.parts[k][1]

    def finish(self):
        for i, (mat, bm, smooth, weld) in enumerate(self.parts.values()):
            if not bm.faces:
                bm.free()
                continue
            if weld:
                bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-5)
            ob = common.obj_from_bmesh(f'teil{i}', bm, mat)
            if smooth > 0:
                common.shade_auto(ob, smooth)
        self.parts = {}


def emit(src, pick, mtx=None):
    """Flaechen eines Hilfs-bmesh einzeln in die bmeshes kopieren, die pick(face) waehlt (None = weglassen).
    So bekommt ein Teil mehrere Materialien (Aufdruck, Abplatzer, Staub) ohne eigene Geometrie."""
    for f in src.faces:
        bm = pick(f)
        if bm is None:
            continue
        bm.faces.new([bm.verts.new(mtx @ v.co if mtx is not None else v.co.copy()) for v in f.verts])
    src.free()


def quad_n(bm, pts, n):
    """Flaeche aus Punkten, Umlauf so, dass die Normale zu n zeigt."""
    pts = [V(p) for p in pts]
    e = (pts[1] - pts[0]).cross(pts[2] - pts[1])
    if e.dot(V(n)) < 0:
        pts = list(reversed(pts))
    bm.faces.new([bm.verts.new(p) for p in pts])


def rot(rx=0.0, ry=0.0, rz=0.0):
    return Euler((rx, ry, rz), 'XYZ').to_matrix().to_4x4()


def tr(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def cube(bm, lo, hi, mtx=None, bevel=0.0):
    """Quader lo..hi (lokal), mit mtx in die Lage gebracht, optional mit Fase (Breite bevel)."""
    r = bmesh.ops.create_cube(bm, size=1.0)
    vs = r['verts']
    for v in vs:
        p = V((lo[0] + (v.co.x + 0.5) * (hi[0] - lo[0]), lo[1] + (v.co.y + 0.5) * (hi[1] - lo[1]),
               lo[2] + (v.co.z + 0.5) * (hi[2] - lo[2])))
        v.co = mtx @ p if mtx is not None else p
    if bevel > 0:
        es = list({e for v in vs for e in v.link_edges})
        bmesh.ops.bevel(bm, geom=es, offset=bevel, offset_type='OFFSET', segments=1, profile=0.5, affect='EDGES',
                        clamp_overlap=True)


def blk(bm, size, loc, rx=0.0, ry=0.0, rz=0.0, bevel=0.0):
    """Quader der Groesse size, Mitte loc, gedreht (Euler XYZ)."""
    sx, sy, sz = size
    cube(bm, (-sx / 2, -sy / 2, -sz / 2), (sx / 2, sy / 2, sz / 2), tr(*loc) @ rot(rx, ry, rz), bevel)


def frames_along(pts, up=None):
    """Tangenten und drehungsminimierte Normalen entlang einer Polylinie."""
    n = len(pts)
    T = []
    for i in range(n):
        if i == 0:
            t = pts[1] - pts[0]
        elif i == n - 1:
            t = pts[-1] - pts[-2]
        else:
            a, b = pts[i] - pts[i - 1], pts[i + 1] - pts[i]
            t = (a.normalized() if a.length > 1e-9 else b) + (b.normalized() if b.length > 1e-9 else a)
        if t.length < 1e-9:
            t = V((0, 0, 1))
        T.append(t.normalized())
    a = V(up) if up is not None else (V((0, 0, 1)) if abs(T[0].z) < 0.9 else V((1, 0, 0)))
    N0 = a - T[0] * a.dot(T[0])
    if N0.length < 1e-6:
        N0 = V((1, 0, 0)) - T[0] * T[0].x
    Ns = [N0.normalized()]
    for i in range(1, n):
        q = T[i - 1].rotation_difference(T[i])
        nn = q @ Ns[-1]
        nn = nn - T[i] * nn.dot(T[i])
        Ns.append(nn.normalized() if nn.length > 1e-9 else Ns[-1])
    return T, Ns


def circle(segs, sx=1.0, sy=1.0, a0=0.0):
    return [(sx * math.cos(a0 + TAU * k / segs), sy * math.sin(a0 + TAU * k / segs)) for k in range(segs)]


def sweep(bm, pts, r, segs=8, caps=True, profile=None, up=None, twist=0.0):
    """Profil (Standard: Kreis) entlang der Polylinie pts ziehen; r Zahl oder Liste je Punkt. Normalen aussen."""
    pts = [V(p) for p in pts]
    n = len(pts)
    rs = list(r) if isinstance(r, (list, tuple)) else [r] * n
    T, Ns = frames_along(pts, up)
    prof = profile or circle(segs)
    rings = []
    for i in range(n):
        t, nn = T[i], Ns[i]
        b = t.cross(nn)
        ca, sa = math.cos(twist * i), math.sin(twist * i)
        nn, b = nn * ca + b * sa, b * ca - nn * sa
        rings.append([bm.verts.new(pts[i] + (nn * u + b * v) * rs[i]) for (u, v) in prof])
    m = len(prof)
    for i in range(n - 1):
        for k in range(m):
            k2 = (k + 1) % m
            bm.faces.new((rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]))
    if caps:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    return rings


def cyl(bm, p0, p1, r, segs=8, caps=True, up=None):
    return sweep(bm, [p0, p1], r, segs, caps, up=up)


def catmull(pts, steps=6):
    """Catmull-Rom durch die Punkte (fuer Kabel, Ranken, Wurzeln)."""
    P = [V(p) for p in pts]
    if len(P) < 3:
        return P
    out = []
    for i in range(len(P) - 1):
        p0 = P[i - 1] if i > 0 else P[i] * 2 - P[i + 1]
        p1, p2 = P[i], P[i + 1]
        p3 = P[i + 2] if i + 2 < len(P) else P[i + 1] * 2 - P[i]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(P[-1])
    return out


def lathe(bm, prof, segs, mtx=None, a0=0.0, a1=TAU):
    """Drehkoerper: prof = [(r, z), ...] um die z-Achse (aussen nach oben = Normale aussen), dann mtx."""
    full = abs(a1 - a0 - TAU) < 1e-6
    n = segs if full else segs + 1
    rings = []
    for (r, z) in prof:
        if r < 1e-5:
            p = V((0, 0, z))
            rings.append([bm.verts.new(mtx @ p if mtx is not None else p)])
            continue
        ring = []
        for k in range(n):
            a = a0 + (a1 - a0) * k / segs
            p = V((r * math.cos(a), r * math.sin(a), z))
            ring.append(bm.verts.new(mtx @ p if mtx is not None else p))
        rings.append(ring)
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        if len(A) == 1 and len(B) == 1:
            continue
        for k in range(segs):
            k2 = (k + 1) % n if full else k + 1
            if len(A) == 1:
                bm.faces.new((A[0], B[k2], B[k]))
            elif len(B) == 1:
                bm.faces.new((A[k], A[k2], B[0]))
            else:
                bm.faces.new((A[k], A[k2], B[k2], B[k]))
    return rings


def poly_prism(bm, outline, z0, z1, mtx=None):
    """Senkrechtes Prisma ueber einem Umriss (gegen den Uhrzeigersinn, von oben)."""
    def P(x, y, z):
        p = V((x, y, z))
        return mtx @ p if mtx is not None else p
    lo = [bm.verts.new(P(x, y, z0)) for (x, y) in outline]
    hi = [bm.verts.new(P(x, y, z1)) for (x, y) in outline]
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.faces.new(hi)
    bm.faces.new(list(reversed(lo)))


def blob(bm, c, nrm, rad, h, rng, nseg=9, squash=1.0, rough=0.3):
    """Flacher Klecks (Moertel, Beton, Schmutz) auf einer Flaeche: unregelmaessiger Rand, gewoelbte Mitte.
    c Mittelpunkt auf der Flaeche, nrm Flaechennormale."""
    nrm = V(nrm).normalized()
    u = nrm.orthogonal().normalized()
    w = nrm.cross(u)
    ph = rng.uniform(0, TAU)
    ring, ring2 = [], []
    for k in range(nseg):
        a = ph + TAU * k / nseg
        rr = rad * (1.0 + rng.uniform(-rough, rough))
        p = V(c) + (u * math.cos(a) + w * math.sin(a) * squash) * rr
        ring.append(bm.verts.new(p - nrm * 0.0015))
        ring2.append(bm.verts.new(p * 0.4 + V(c) * 0.6 + (u * math.cos(a) + w * math.sin(a) * squash) * rr * 0.05
                                  + nrm * h * rng.uniform(0.55, 0.9)))
    top = bm.verts.new(V(c) + nrm * h)
    for k in range(nseg):
        j = (k + 1) % nseg
        bm.faces.new((ring[k], ring[j], ring2[j], ring2[k]))
        bm.faces.new((ring2[k], ring2[j], top))


def two_sided(bm, faces_pts, eps=0.0006):
    """Flaechen aus Punktlisten anlegen - vorn und (um eps versetzt) hinten mit umgekehrtem Umlauf."""
    for pts in faces_pts:
        vs = [bm.verts.new(p) for p in pts]
        f = bm.faces.new(vs)
        f.normal_update()
        nn = f.normal.copy()
        bm.faces.new([bm.verts.new(p - nn * eps) for p in reversed(pts)])


def leaf(bm, base, fwd, up, length, width, rng=None, stations=((0.28, 0.9), (0.62, 0.72)), droop=0.0, fold=0.18,
         curl=0.0, twist=0.0):
    """Blatt als echte Flaeche (zweiseitig): Stiel-Ansatz base, Richtung fwd, Oberseite etwa up.
    stations: (Anteil der Laenge, relative Breite); droop: Biegung zur Unterseite (rad, ueber die Laenge);
    fold: Mittelrippe hochgezogen (Anteil der Breite), curl: Raender hoch."""
    fwd = V(fwd).normalized()
    side = fwd.cross(V(up))
    if side.length < 1e-6:
        side = fwd.orthogonal()
    side.normalize()
    if twist:
        side = Matrix.Rotation(twist, 3, fwd) @ side
    nrm = side.cross(fwd).normalized()  # Oberseite (etwa up)
    side = fwd.cross(nrm).normalized()
    # Mittellinie mit Biegung um die Querachse
    st = [s for s in stations] + [(1.0, 0.0)]
    prev_t = 0.0
    p, d, n_ = V(base), fwd.copy(), nrm.copy()
    out = []
    for (t, wrel) in st:
        dt = t - prev_t
        R = Matrix.Rotation(-droop * dt, 3, side)
        d = (R @ d).normalized()
        n_ = (R @ n_).normalized()
        p = p + d * (length * dt)
        out.append((p.copy(), n_.copy(), wrel))
        prev_t = t
    B = V(base)
    front, back = [], []
    rows = []
    for (c, nn, wrel) in out[:-1]:
        w = width * 0.5 * wrel
        L = c - side * w + nn * (curl * w)
        R_ = c + side * w + nn * (curl * w)
        M = c + nn * (fold * w)
        rows.append((L, M, R_))
    tip = out[-1][0]
    faces = []
    L1, M1, R1 = rows[0]
    faces.append((B, L1, M1))
    faces.append((B, M1, R1))
    for i in range(len(rows) - 1):
        La, Ma, Ra = rows[i]
        Lb, Mb, Rb = rows[i + 1]
        faces.append((La, Lb, Mb, Ma))
        faces.append((Ma, Mb, Rb, Ra))
    Ln, Mn, Rn = rows[-1]
    faces.append((Ln, tip, Mn))
    faces.append((Mn, tip, Rn))
    # Umlauf: (B, L, M) hat die Normale fwd x side; die Oberseite soll nrm sein -> ggf. umdrehen
    sgn = fwd.cross(side).dot(nrm)
    for f in faces:
        pts = list(f) if sgn > 0 else list(reversed(f))
        vs = [bm.verts.new(q) for q in pts]
        bm.faces.new(vs)
        bm.faces.new([bm.verts.new(q - nrm * 0.0005) for q in reversed(pts)])
    return tip


def leaf_fan(bm, center, outline, cup_nrm, cup=0.01):
    """Blatt aus Umriss (Liste von Punkten, gegen den Uhrzeigersinn um cup_nrm) und gewoelbter Mitte - zweiseitig."""
    c = V(center) + V(cup_nrm) * cup
    n = len(outline)
    for i in range(n):
        j = (i + 1) % n
        tri = (c, V(outline[i]), V(outline[j]))
        e = (tri[1] - tri[0]).cross(tri[2] - tri[0])
        if e.dot(V(cup_nrm)) < 0:
            tri = (tri[0], tri[2], tri[1])
        bm.faces.new([bm.verts.new(q) for q in tri])
        bm.faces.new([bm.verts.new(q - V(cup_nrm) * 0.0005) for q in reversed(tri)])


def jitter(rng, a):
    return rng.uniform(-a, a)


# =====================================================================================================================
# Materialien
# =====================================================================================================================
def mats_site():
    return {
        'galv': material('verzinkt', (0.66, 0.67, 0.66), look='galvanized', rough=0.5, metal=0.8, r=1.05, m=0.6),
        'galv_old': material('verzinkt_alt', (0.50, 0.50, 0.48), look='galvanized', rough=0.6, metal=0.7, r=1.4, m=0.45),
        'rust': material('rost', (0.62, 0.44, 0.32), look='metal', tex='metal_rusty_iron', rough=0.9, metal=0.3, r=1.35, m=0.15),
        'steel_dark': material('stahl_dunkel', (0.20, 0.20, 0.21), look='metal', rough=0.6, metal=0.8, r=1.15, m=0.55),
        'mortar': material('moertel', (0.68, 0.67, 0.64), look='concrete', tex='concrete_chalky', rough=0.95, r=1.35),
        'concrete_crust': material('betonkruste', (0.66, 0.65, 0.62), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'plank': material('bohle', (0.74, 0.64, 0.48), look='wood', tex='wood_birch', rough=0.85, r=1.25),
        'plank_old': material('bohle_alt', (0.56, 0.52, 0.44), look='wood', tex='wood_birch', rough=0.9, r=1.4),
        'timber': material('kantholz', (0.70, 0.58, 0.42), look='wood', tex='wood_birch', rough=0.85, r=1.3),
        'red': material('rot_lack', (0.66, 0.10, 0.07), look='paint', rough=0.5, r=0.8),
        'red_worn': material('rot_matt', (0.52, 0.12, 0.09), look='paint', rough=0.7, r=1.15),
        'white': material('weiss_lack', (0.86, 0.86, 0.83), look='paint', rough=0.5, r=0.85),
        'yellow': material('gelb_lack', (0.86, 0.66, 0.10), look='paint', rough=0.55, r=0.9),
        'orange': material('orange_lack', (0.86, 0.38, 0.07), look='paint', rough=0.5, r=0.85),
        'black_rubber': material('gummi', (0.045, 0.045, 0.045), look='rubber', rough=0.9, r=1.2),
        'black_plastic': material('kunststoff_schwarz', (0.06, 0.06, 0.065), look='plastic', rough=0.6, r=1.0),
        'paper': material('papiersack', (0.74, 0.66, 0.52), look='cardboard', rough=0.95, r=1.4),
        'paper_dirty': material('papiersack_alt', (0.60, 0.56, 0.48), look='cardboard', rough=0.95, r=1.45),
        'print_red': material('aufdruck_rot', (0.62, 0.14, 0.10), look='paint', rough=0.8, r=1.3),
        'print_grey': material('aufdruck_grau', (0.34, 0.36, 0.38), look='paint', rough=0.8, r=1.3),
        'cement': material('zementstaub', (0.70, 0.70, 0.68), look='concrete', tex='concrete_chalky', rough=1.0, r=1.5),
        'pallet': material('palette', (0.70, 0.60, 0.45), look='wood', tex='wood_birch', rough=0.9, r=1.3),
        'tarp_blue': material('plane_blau', (0.13, 0.27, 0.50), look='plastic', rough=0.7, r=0.95),
        'tarp_green': material('plane_gruen', (0.20, 0.30, 0.20), look='plastic', rough=0.75, r=1.05),
        'tarp_white': material('plane_weiss', (0.78, 0.79, 0.77), look='plastic', rough=0.7, r=1.0),
        'tarp_hem': material('plane_saum', (0.85, 0.86, 0.84), look='plastic', rough=0.75, r=1.1),
        'zinc': material('zink', (0.70, 0.70, 0.68), look='metal', rough=0.45, metal=0.9, r=0.8, m=0.75),
        'brick': material('ziegel', (0.86, 0.56, 0.44), look='concrete', tex='bricks_red_factory', rough=0.9, r=1.3),
        'clay': material('hochlochziegel', (0.72, 0.40, 0.28), look='concrete', tex='concrete_rough', rough=0.9, r=1.35),
        'clay2': material('hochlochziegel_hell', (0.80, 0.48, 0.34), look='concrete', tex='concrete_rough', rough=0.9, r=1.35),
        'block': material('betonstein', (0.70, 0.70, 0.68), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'block2': material('betonstein_dunkel', (0.58, 0.58, 0.57), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'bitumen': material('bitumenbahn', (0.07, 0.07, 0.07), look='rubber', rough=0.8, r=1.0),
        'glass_dirty': material('glas_staub', (0.55, 0.56, 0.55), look='glass', rough=0.3, r=0.5),
    }


# =====================================================================================================================
# OUROBOROS - ewige Baustelle
# =====================================================================================================================
def _coupler(k, m, p, axis_a, rng):
    """Kupplung (Normal-/Drehkupplung) an einem Rohrkreuz: Gussbacken, Hammerkopfschraube, Mutter."""
    a = V(axis_a).normalized()
    u = a.orthogonal().normalized()
    w = a.cross(u)
    M = Matrix((
        (a.x, u.x, w.x, p[0]),
        (a.y, u.y, w.y, p[1]),
        (a.z, u.z, w.z, p[2]),
        (0, 0, 0, 1)))
    bm = k(m['galv_old'])
    cube(bm, (-0.045, -0.034, -0.034), (0.045, 0.034, 0.034), M, bevel=0.006)
    bm2 = k(m['rust'] if rng.random() < 0.5 else m['steel_dark'], 50)
    # Schraube und Mutter seitlich
    q0 = M @ V((0.0, 0.034, 0.012))
    q1 = M @ V((0.0, 0.066, 0.012))
    cyl(bm2, q0, q1, 0.008, 6)
    blk(k(m['galv_old']), (0.02, 0.016, 0.02), tuple(M @ V((0.0, 0.058, 0.012))), bevel=0.002)


def _base_jack(k, m, x, y, h, rng):
    """Fussspindel: Fussplatte, Gewindespindel, Fluegelmutter. h = Hoehe der Spindel."""
    cube(k(m['rust']), (x - 0.075, y - 0.075, 0.0), (x + 0.075, y + 0.075, 0.007), bevel=0.002)
    # Gewinde: Wechsel von Kern- und Aussendurchmesser
    pts, rs = [], []
    n = int(h / 0.016)
    for i in range(n + 1):
        pts.append((x, y, 0.007 + h * i / n))
        rs.append(0.0165 if i % 2 else 0.0185)
    sweep(k(m['rust'], 50), pts, rs, 8, caps=True)
    zn = 0.007 + h * 0.62
    lathe(k(m['red_worn'], 50), [(0.0, zn), (0.034, zn), (0.036, zn + 0.012), (0.03, zn + 0.03), (0.0, zn + 0.03)], 10,
          tr(x, y, 0))
    for s in (-1, 1):
        blk(k(m['red_worn']), (0.07, 0.012, 0.022), (x + s * 0.05, y, zn + 0.016), bevel=0.003)


def _scaffold(variant):
    rng = random.Random(40 + variant)
    m = mats_site()
    k = Kit()
    X0, X1 = -0.36, 0.36         # Staender quer (Tiefe 0,8 m mit Belag)
    Y0, Y1 = -1.25, 1.25         # Feldlaenge 2,5 m
    R = 0.0242                   # Rohr 48,3 mm
    zJ = 0.20                    # Oberkante Spindel
    D1, D2 = 1.35, 2.60          # Belaghoehen der beiden Etagen
    TOP = 3.60
    tube = lambda: k(m['galv'] if rng.random() < 0.7 else m['galv_old'], 50)  # noqa: E731
    half = variant == 2
    # Fussspindeln
    for x in (X0, X1):
        for y in (Y0, Y1):
            _base_jack(k, m, x, y, zJ + 0.06, rng)
    # Vertikalrahmen an beiden Enden (zwei Staender, Riegel unter jedem Belag, Eckstreben)
    for yi, y in enumerate((Y0, Y1)):
        upper = not (half and yi == 1)
        ztop = TOP if upper else D1 + 0.05
        for x in (X0, X1):
            sweep(tube(), [(x, y, zJ), (x, y, ztop)], R, 10)
            # Rohrverbinder (Stossstelle) zwischen den Rahmen
            cyl(k(m['galv_old'], 50), (x, y, D1 - 0.02), (x, y, D1 + 0.10), R + 0.004, 10)
        for z in ([D1, D2] if upper else [D1]):
            sweep(tube(), [(X0, y, z - 0.03), (X1, y, z - 0.03)], R, 10)
            for sx, x in ((1, X0), (-1, X1)):
                sweep(tube(), [(x, y, z - 0.03 - 0.30), (x + sx * 0.22, y, z - 0.03)], R * 0.7, 8)
            # Belagklaue-Auflage (U-Profil auf dem Riegel)
            cube(k(m['galv_old']), (X0 + 0.03, y - 0.03, z - 0.03 + R * 0.6), (X1 - 0.03, y + 0.03, z - 0.03 + R + 0.004))
        if upper:
            for z in (D2 + 0.5, TOP - 0.02):  # Stirnseitengelaender oben
                sweep(tube(), [(X0, y, z), (X1, y, z)], R, 10)
    # Gelaender und Diagonale auf der Aussenseite (+x)
    xo = X1 + R * 2.0
    rails = [D1 + 0.5]
    if not half:
        rails += [D2 + 0.5, TOP - 0.02]
    for z in rails:
        sweep(tube(), [(xo, Y0 - 0.12, z), (xo, Y1 + 0.12, z)], R, 10)
        for y in (Y0, Y1):
            _coupler(k, m, (xo - R, y, z), (0, 0, 1), rng)
    sweep(tube(), [(xo, Y0 - 0.05, zJ + 0.15), (xo, Y1 + 0.05, D2 + 0.10 if not half else D1 + 0.15)], R, 10)
    _coupler(k, m, (xo - R, Y0, zJ + 0.20), (0, 0, 1), rng)
    _coupler(k, m, (xo - R, Y1, D2 + 0.05 if not half else D1 + 0.12), (0, 0, 1), rng)
    # Laengsriegel unten innen (Laufstabilitaet)
    sweep(tube(), [(X0 - R * 2, Y0 - 0.1, zJ + 0.25), (X0 - R * 2, Y1 + 0.1, zJ + 0.25)], R, 10)
    for y in (Y0, Y1):
        _coupler(k, m, (X0 - R, y, zJ + 0.25), (0, 0, 1), rng)

    # Belaege
    def steel_deck(x0, x1, z, skip_mortar=False):
        g = k(m['galv'])
        cube(g, (x0, Y0 - 0.02, z), (x1, Y1 + 0.02, z + 0.004))
        for xe in (x0, x1 - 0.003):
            cube(g, (xe, Y0 - 0.02, z - 0.045), (xe + 0.003, Y1 + 0.02, z))
        for ye in (Y0 - 0.02, Y1 + 0.02 - 0.012):  # Stirnbleche mit Klauen
            cube(k(m['galv_old']), (x0, ye, z - 0.06), (x1, ye + 0.012, z + 0.002))
        # Rutschhemmung: eingepraegte Sicken (flache Leisten)
        for i in range(5):
            xx = x0 + (x1 - x0) * (i + 0.5) / 5
            cube(g, (xx - 0.006, Y0 + 0.05, z + 0.004), (xx + 0.006, Y1 - 0.05, z + 0.0065))
        if not skip_mortar:
            for _ in range(rng.randint(2, 4)):
                blob(k(m['mortar'], 35), (rng.uniform(x0 + 0.04, x1 - 0.04), rng.uniform(Y0 + 0.2, Y1 - 0.2), z + 0.0065),
                     (0, 0, 1), rng.uniform(0.03, 0.08), rng.uniform(0.004, 0.012), rng)

    def plank_deck(x0, x1, z, n=3):
        w = (x1 - x0 - 0.01 * (n - 1)) / n
        for i in range(n):
            xa = x0 + i * (w + 0.01)
            dz = rng.uniform(0, 0.004)
            mat = m['plank'] if rng.random() < 0.6 else m['plank_old']
            cube(k(mat), (xa, Y0 - 0.12, z + dz), (xa + w, Y1 + 0.12, z + 0.045 + dz), bevel=0.003)
            for ye in (Y0 - 0.12, Y1 + 0.12 - 0.03):  # Bohlenbeschlag (Blechband)
                cube(k(m['galv_old']), (xa - 0.002, ye, z + dz - 0.002), (xa + w + 0.002, ye + 0.03, z + 0.047 + dz))
            for _ in range(rng.randint(1, 2)):
                blob(k(m['mortar'], 35), (xa + w * rng.uniform(0.2, 0.8), rng.uniform(Y0, Y1), z + 0.045 + dz), (0, 0, 1),
                     rng.uniform(0.025, 0.06), rng.uniform(0.003, 0.01), rng)

    def toe_board(z, length_y=True):
        mat = m['plank_old']
        if length_y:
            cube(k(mat), (X1 - 0.005, Y0, z), (X1 + 0.025, Y1, z + 0.15), bevel=0.002)
        for y in (Y0 + 0.02, Y1 - 0.05):
            cube(k(mat), (X0 + 0.02, y, z), (X1 - 0.01, y + 0.03, z + 0.15), bevel=0.002)

    if variant == 0:
        steel_deck(X0 + 0.02, -0.01, D2)
        steel_deck(0.01, X1 - 0.02, D2)
        steel_deck(0.01, X1 - 0.02, D1)
        # Durchstiegsboden innen (Alurahmen, Sperrholz, Klappe offen) und Leiter durch die Luke
        yc, hw = -0.72, 0.32
        ply = k(material('sperrholz', (0.52, 0.36, 0.22), look='wood', tex='wood_walnut_floor', rough=0.8, r=1.2))
        alu = k(m['zinc'])
        x0d, x1d = X0 + 0.02, -0.01
        for (ya, yb) in ((Y0 - 0.02, yc - hw), (yc + hw, Y1 + 0.02)):
            cube(ply, (x0d + 0.012, ya, D1 - 0.012), (x1d - 0.012, yb, D1 + 0.004))
        for xe in (x0d, x1d - 0.012):
            cube(alu, (xe, Y0 - 0.02, D1 - 0.05), (xe + 0.012, Y1 + 0.02, D1 + 0.006))
        for ye in (yc - hw - 0.012, yc + hw):
            cube(alu, (x0d, ye, D1 - 0.03), (x1d, ye + 0.012, D1 + 0.004))
        # Klappe: am wandseitigen Rand angeschlagen, hochgeklappt und an den Staender gelehnt
        hinge = V((x0d + 0.012, yc, D1 + 0.004))
        Mh = tr(*hinge) @ rot(0, -math.radians(100), 0)
        cube(ply, (0.0, -hw + 0.01, -0.016), (x1d - x0d - 0.03, hw - 0.01, 0.0), Mh)
        cube(alu, (x1d - x0d - 0.05, -hw + 0.01, -0.03), (x1d - x0d - 0.03, hw - 0.01, 0.0), Mh)
        al = k(m['zinc'], 50)
        rail = [(1.0, 0.4), (-1.0, 0.4), (-1.0, -0.4), (1.0, -0.4)]
        pb, pt = V((0.08, yc, 0.0)), V((-0.22, yc, D1 + 0.95))
        for sy in (-0.19, 0.19):
            sweep(al, [pb + V((0, sy, 0)), pt + V((0, sy, 0))], 0.022, 4, profile=rail, up=(1, 0, 0))
        for i in range(1, 9):
            p = pb.lerp(pt, (i * 0.28) / (pt - pb).length)
            cyl(al, p + V((0, -0.19, 0)), p + V((0, 0.19, 0)), 0.014, 6)
        for sy in (-0.19, 0.19):  # Leiterfuesse
            blk(k(m['black_rubber']), (0.07, 0.05, 0.03), (pb.x, yc + sy, 0.015), bevel=0.005)
        toe_board(D2 + 0.004)
        toe_board(D1 + 0.004)
        # Eimer mit Moertel auf dem oberen Belag
        _bucket(k, m, 0.15, 0.6, D2 + 0.0065, rng)
    elif variant == 1:
        for z in (D1, D2):
            plank_deck(X0 + 0.02, X1 - 0.02, z)
        toe_board(D2 + 0.045)
        # Geruestplane auf der Aussenseite: zwischen Etage 1 und dem Handlauf, leicht gebaucht, mit Falten
        _sheet(k, m['tarp_white'], xo + 0.035, Y0 - 0.1, Y1 + 0.1, D1 + 0.05, TOP + 0.02, rng, torn=True)
        # Zwei Ziegelstapel und Kanthoelzer auf Etage 1
        for i in range(2):
            y = -0.7 + i * 0.5
            for j in range(3):
                blk(k(m['brick']), (0.24, 0.115, 0.071), (-0.12, y, D1 + 0.045 + 0.036 + j * 0.073), rz=0.02 * j, bevel=0.003)
        blk(k(m['timber']), (0.08, 1.6, 0.08), (0.22, 0.2, D1 + 0.045 + 0.04), rz=0.05, bevel=0.004)
    else:
        # halb abgebaut: unten ein Belag fehlt, oben nur noch ein Rahmen; abgebaute Teile liegen herum
        steel_deck(X0 + 0.02, -0.01, D1)
        toe_board(D1 + 0.004, length_y=False)
        # haengendes Gelaenderrohr (an einer Kupplung um die Achse gekippt)
        sweep(tube(), [(xo, Y0 + 0.05, D2 + 0.45), (xo + 0.05, Y0 + 1.7, D1 + 0.65)], R, 10)
        _coupler(k, m, (xo - R, Y0, D2 + 0.48), (0, 0, 1), rng)
        # abgelegte Rohre am Rahmen angelehnt
        for i, (y, l) in enumerate(((Y1 - 0.25, 3.0), (Y1 - 0.4, 2.5))):
            sweep(k(m['galv_old'], 50), [(X1 + 0.12 + i * 0.07, y, 0.0), (X1 + 0.02, y + 0.1, l * 0.97)], R, 10)
        # abgelegter Stahlboden am Boden hinter dem Feld (innen)
        g = k(m['galv_old'])
        cube(g, (X0 - 0.02, Y0 + 0.1, 0.0), (X0 + 0.30, Y1 - 0.1, 0.05), tr(0, 0, 0))
        # lose Kupplungen auf dem Belag
        for i in range(3):
            _coupler(k, m, (rng.uniform(X0 + 0.1, -0.05), rng.uniform(-0.8, 0.8), D1 + 0.04), (rng.uniform(-1, 1), 1, 0),
                     rng)
    k.finish()


def _bucket(k, m, x, y, z, rng):
    """Moertelkuebel (schwarzer Kunststoff, 12 l) mit Moertelresten."""
    lathe(k(m['black_plastic'], 40), [(0.0, z), (0.12, z), (0.125, z + 0.005), (0.15, z + 0.22), (0.162, z + 0.23),
                                      (0.162, z + 0.24), (0.145, z + 0.24), (0.135, z + 0.03), (0.0, z + 0.03)], 14,
          tr(x, y, 0))
    lathe(k(m['mortar'], 40), [(0.0, z + 0.14), (0.14, z + 0.14), (0.142, z + 0.15), (0.11, z + 0.17), (0.0, z + 0.175)],
          12, tr(x, y, 0))
    # Kelle im Moertel
    st = k(m['steel_dark'])
    blk(st, (0.16, 0.09, 0.003), (x + 0.03, y + 0.02, z + 0.19), ry=-0.9, rz=0.4)
    cyl(k(m['plank'], 50), (x + 0.07, y + 0.04, z + 0.27), (x + 0.12, y + 0.065, z + 0.36), 0.012, 6)


def _sheet(k, mat, x, y0, y1, z0, z1, rng, torn=False):
    """Geruestplane: senkrechtes Tuch mit Falten (zweiseitig), Saum mit Oesen."""
    bm = k(mat, 60, weld=True)
    ny, nz = 22, 9
    grid = []
    for j in range(nz + 1):
        row = []
        for i in range(ny + 1):
            u, v = i / ny, j / nz
            y = y0 + (y1 - y0) * u
            z = z0 + (z1 - z0) * v
            fold = 0.025 * math.sin(u * 23.0 + 0.7 * math.sin(v * 3.0)) * (0.4 + 0.6 * math.sin(math.pi * v))
            belly = 0.05 * math.sin(math.pi * v) * math.sin(math.pi * u) * 0.8
            row.append(V((x + fold + belly, y, z - 0.04 * math.sin(math.pi * u) * (1 - v))))
        grid.append(row)
    for j in range(nz):  # unregelmaessig gezogen (nicht ganz eben gespannt)
        for i in range(ny + 1):
            grid[j][i] += V((jitter(rng, 0.006), jitter(rng, 0.008), jitter(rng, 0.012)))
    cut = [ny - 3 - int(j * 1.6) + rng.randint(-1, 1) for j in range(nz)]
    faces = []
    for j in range(nz):
        for i in range(ny):
            if torn and j < 4 and i >= cut[j]:
                continue  # unten rechts abgerissen, ausgefranster Rand
            faces.append((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
    for f in faces:
        vs = [bm.verts.new(p) for p in f]
        bm.faces.new(vs)  # Normale -x (zum Geruest) ... Rueckseite folgt
        bm.faces.new([bm.verts.new(p + V((0.0012, 0, 0))) for p in reversed(f)])
    # Saum oben und Oesen
    hem = k(m_hem(), 0)
    for i in range(ny):
        a, b = grid[nz][i], grid[nz][i + 1]
        cube(hem, (min(a.x, b.x) - 0.002, a.y, a.z - 0.04), (max(a.x, b.x) + 0.004, b.y, a.z))
    for i in range(1, ny, 3):
        p = grid[nz][i]
        lathe(k(m_zinc(), 50), [(0.006, -0.003), (0.012, -0.003), (0.012, 0.003), (0.006, 0.003), (0.006, -0.003)], 8,
              tr(p.x + 0.003, p.y, p.z - 0.02) @ rot(0, math.pi / 2, 0))


def m_hem():
    return material('plane_saum', (0.85, 0.86, 0.84), look='plastic', rough=0.75, r=1.1)


def m_zinc():
    return material('zink', (0.70, 0.70, 0.68), look='metal', rough=0.45, metal=0.9, r=0.8, m=0.75)


def build_scaffold():
    _scaffold(0)


def build_scaffold__v1():
    _scaffold(1)


def build_scaffold__v2():
    _scaffold(2)


# --- halb gemauerte Wand ---------------------------------------------------------------------------------------------
def _course_units(ci, n, bl, j, L):
    """Steine einer Schicht (y-Bereiche): ungerade Schichten beginnen mit einem halben Stein (Verband)."""
    units = []
    y = -L / 2
    if ci % 2:
        units.append((y, y + bl / 2 - j / 2))
        y += bl / 2 + j / 2
        n -= 1
    for i in range(n):
        ya = y + i * (bl + j)
        yb = min(ya + bl, L / 2)
        if yb - ya > 0.06:
            units.append((ya, yb))
    return units


def _wall_unfinished(variant):
    """3 m lang (entlang y), 0,25 m dick (entlang x). v0: Hochlochziegel, v1: Betonhohlbloecke. Rechts abgetreppt
    (Verzahnung fuer den naechsten Abschnitt), oben offene Steine zeigen ihr Lochbild."""
    rng = random.Random(70 + variant)
    m = mats_site()
    k = Kit()
    L, Tk = 3.0, 0.24
    if variant == 0:
        bl, bh, mats = 0.373, 0.249, (m['clay'], m['clay2'])
        counts = [8, 8, 8, 7, 5, 3]
    else:
        bl, bh, mats = 0.49, 0.238, (m['block'], m['block2'])
        counts = [6, 6, 5, 4, 2]
    j = 0.012   # Fuge
    courses = [_course_units(ci, n, bl, j, L) for ci, n in enumerate(counts)]
    # Sperrbahn unter der ersten Schicht (seitlich etwas ueberstehend)
    cube(k(m['bitumen']), (-0.135, -L / 2 - 0.01, 0.0), (0.135, L / 2 + 0.01, 0.004))
    z = 0.004
    for ci, units in enumerate(courses):
        y0, y1 = units[0][0], units[-1][1]
        cube(k(m['mortar']), (-Tk / 2 + 0.006, y0, z), (Tk / 2 - 0.006, y1, z + j))
        # herausgequollener Moertel an der Lagerfuge (Batzen und Fahnen)
        for _ in range(int((y1 - y0) / 0.35)):
            yy = rng.uniform(y0 + 0.05, y1 - 0.05)
            sx = rng.choice((-1, 1))
            blk(k(m['mortar'], 30), (0.022, rng.uniform(0.05, 0.16), j * rng.uniform(0.7, 1.1)),
                (sx * (Tk / 2 + 0.004), yy, z + j * 0.5), bevel=0.0025)
        z += j
        nxt = courses[ci + 1] if ci + 1 < len(courses) else None
        for i, (ya, yb) in enumerate(units):
            mat = mats[0] if rng.random() < 0.72 else mats[1]
            dx, dz = jitter(rng, 0.004), jitter(rng, 0.0015)
            exposed = nxt is None or ya < nxt[0][0] - 0.01 or yb > nxt[-1][1] + 0.01
            _masonry_unit(k, m, mat, variant, (-Tk / 2 + dx, ya, z + dz), (Tk / 2 + dx, yb, z + bh + dz), rng, exposed)
            if i < len(units) - 1:  # Stossfuge (nicht immer voll vermoertelt)
                cube(k(m['mortar']), (-Tk / 2 + 0.012, yb, z), (Tk / 2 - 0.012, units[i + 1][0], z + bh * rng.uniform(0.55, 1.0)))
        z += bh
    # Moertelkleckse am Boden
    for _ in range(8):
        blob(k(m['mortar'], 30), (rng.choice((-1, 1)) * rng.uniform(0.16, 0.36), rng.uniform(-1.4, 1.4), 0.0), (0, 0, 1),
             rng.uniform(0.03, 0.09), rng.uniform(0.004, 0.018), rng)
    # Moertelbatzen mit Kelle auf der obersten Schicht
    top = courses[-1]
    zt = z
    ym = (top[0][0] + top[-1][1]) / 2
    blob(k(m['mortar'], 30), (0.0, ym, zt), (0, 0, 1), 0.08, 0.04, rng)
    Mk = tr(0.03, ym + 0.06, zt + 0.035) @ rot(0.0, -0.32, 0.6)
    poly_prism(k(m['steel_dark']), [(-0.09, -0.05), (0.08, -0.04), (0.11, 0.0), (0.08, 0.04), (-0.09, 0.05)], -0.0015, 0.0015, Mk)
    cyl(k(m['steel_dark'], 50), tuple(Mk @ V((-0.09, 0.0, 0.0))), tuple(Mk @ V((-0.13, 0.0, 0.05))), 0.004, 5)
    cyl(k(m['plank'], 50), tuple(Mk @ V((-0.13, 0.0, 0.05))), tuple(Mk @ V((-0.24, 0.0, 0.07))), 0.013, 7)
    k.finish()


def _perforated_top(k, mat, dark, x0, x1, y0, y1, z, holes_x, holes_y, depth):
    """Oberseite eines Lochsteins: Stege auf Hoehe z, Loecher als Schaechte (Waende im Steinmaterial, dunkler Grund)."""
    bm = k(mat)
    up = (0, 0, 1)
    ys = [y0] + [v for (a, b) in holes_y for v in (a, b)] + [y1]
    for i in range(0, len(ys), 2):
        if ys[i + 1] - ys[i] > 1e-4:
            quad_n(bm, [(x0, ys[i], z), (x1, ys[i], z), (x1, ys[i + 1], z), (x0, ys[i + 1], z)], up)
    xs = [x0] + [v for (a, b) in holes_x for v in (a, b)] + [x1]
    for (ya, yb) in holes_y:
        for i in range(0, len(xs), 2):
            if xs[i + 1] - xs[i] > 1e-4:
                quad_n(bm, [(xs[i], ya, z), (xs[i + 1], ya, z), (xs[i + 1], yb, z), (xs[i], yb, z)], up)
        for (xa, xb) in holes_x:
            zb = z - depth
            quad_n(k(dark), [(xa, ya, zb), (xb, ya, zb), (xb, yb, zb), (xa, yb, zb)], up)
            quad_n(bm, [(xa, ya, z), (xb, ya, z), (xb, ya, zb), (xa, ya, zb)], (0, 1, 0))
            quad_n(bm, [(xa, yb, z), (xb, yb, z), (xb, yb, zb), (xa, yb, zb)], (0, -1, 0))
            quad_n(bm, [(xa, ya, z), (xa, yb, z), (xa, yb, zb), (xa, ya, zb)], (1, 0, 0))
            quad_n(bm, [(xb, ya, z), (xb, yb, z), (xb, yb, zb), (xb, ya, zb)], (-1, 0, 0))


def _masonry_unit(k, m, mat, variant, lo, hi, rng, exposed):
    """Mauerstein mit Fase; ist die Oberseite frei, zeigt sie Lochbild (Ziegel) bzw. zwei Kammern (Betonblock)."""
    bm = k(mat)
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    if not exposed:
        cube(bm, lo, hi, bevel=0.004)
    else:
        depth = 0.05
        cube(bm, lo, (x1, y1, z1 - depth), bevel=0.003)
        for (a, b, n) in (((x0, y0), (x1, y0), (0, -1, 0)), ((x1, y1), (x0, y1), (0, 1, 0)),
                          ((x0, y1), (x0, y0), (-1, 0, 0)), ((x1, y0), (x1, y1), (1, 0, 0))):
            quad_n(bm, [(a[0], a[1], z1 - depth - 0.003), (b[0], b[1], z1 - depth - 0.003), (b[0], b[1], z1),
                        (a[0], a[1], z1)], n)
        dark = material('kammer', (0.16, 0.13, 0.11), look='concrete', rough=1.0, r=1.5)
        if variant == 0:
            nx, ny = 4, 6
            hx = [(x0 + 0.02 + (x1 - x0 - 0.04) * (a + 0.12) / nx, x0 + 0.02 + (x1 - x0 - 0.04) * (a + 0.88) / nx) for a in range(nx)]
            hy = [(y0 + 0.02 + (y1 - y0 - 0.04) * (b + 0.15) / ny, y0 + 0.02 + (y1 - y0 - 0.04) * (b + 0.85) / ny) for b in range(ny)]
        else:
            hx = [(x0 + 0.032, x1 - 0.032)]
            w = (y1 - y0 - 0.032 * 3) / 2
            hy = [(y0 + 0.032, y0 + 0.032 + w), (y1 - 0.032 - w, y1 - 0.032)] if y1 - y0 > 0.3 else [(y0 + 0.03, y1 - 0.03)]
        _perforated_top(k, mat, dark, x0, x1, y0, y1, z1, hx, hy, depth)
    # Abplatzer an Kanten (Bruchstellen in hellerem Material)
    if rng.random() < 0.3:
        sx = rng.choice((x0, x1))
        blk(k(m['mortar'] if variant else m['clay2']), (0.012, rng.uniform(0.03, 0.07), rng.uniform(0.02, 0.05)),
            (sx, rng.uniform(y0 + 0.05, y1 - 0.05), rng.uniform(z0 + 0.04, z1 - 0.04)), rx=0.3, rz=0.2)


def build_wall_unfinished():
    _wall_unfinished(0)


def build_wall_unfinished__v1():
    _wall_unfinished(1)


# --- Zementsaecke auf Europalette ------------------------------------------------------------------------------------
def _euro_pallet(k, m, rng, z0=0.0, mtx=None):
    """Europalette 1200 x 800 x 144 mm: 3 Bodenbretter, 9 Kloetze, 3 Querbretter, 5 Deckbretter (entlang x)."""
    M = mtx if mtx is not None else Matrix.Identity(4)

    def board(lo, hi, mat=None):
        mt = mat or (m['pallet'] if rng.random() < 0.65 else m['plank_old'])
        cube(k(mt), lo, hi, M @ tr(0, 0, z0), bevel=0.002)
    for y, w in ((-0.35, 0.1), (0.0, 0.145), (0.35, 0.1)):
        board((-0.6, y - w / 2, 0.0), (0.6, y + w / 2, 0.022))
    for x, wx in ((-0.5275, 0.145), (0.0, 0.145), (0.5275, 0.145)):
        for y, wy in ((-0.35, 0.1), (0.0, 0.145), (0.35, 0.1)):
            board((x - wx / 2, y - wy / 2, 0.022), (x + wx / 2, y + wy / 2, 0.1))
        board((x - wx / 2, -0.4, 0.1), (x + wx / 2, 0.4, 0.122))
    for y, w in ((-0.3275, 0.145), (-0.1638, 0.1), (0.0, 0.145), (0.1638, 0.1), (0.3275, 0.145)):
        board((-0.6, y - w / 2, 0.122), (0.6, y + w / 2, 0.144))
    # Nagelkoepfe auf den Deckbrettern
    for x in (-0.5275, 0.0, 0.5275):
        for y in (-0.3275, 0.0, 0.3275):
            for dy in (-0.04, 0.04):
                cyl(k(m['steel_dark']), tuple(M @ V((x, y + dy, z0 + 0.144))), tuple(M @ V((x, y + dy, z0 + 0.1455))), 0.005, 5)


def _bag(k, m, rng, size, mtx, torn=False, dirty=False):
    """Zementsack (Ventilsack, 25 kg): prall, Kanten gerundet, Enden flacher; Aufdruck als eigene Flaechen."""
    sx, sy, sz = size
    src = bmesh.new()
    bmesh.ops.create_cube(src, size=2.0)
    bmesh.ops.subdivide_edges(src, edges=list(src.edges), cuts=2, use_grid_fill=True)
    ph = rng.uniform(0, 6)
    for v in src.verts:
        u = v.co.copy()
        s = u.normalized() * max(abs(u.x), abs(u.y), abs(u.z))
        p = u.lerp(s, 0.32)
        p.z *= (1.0 - 0.16 * abs(u.x) ** 4) * (1.0 - 0.10 * abs(u.y) ** 4)
        if torn and u.z > 0.5 and abs(u.x) < 0.7:
            p.z -= 0.55 * (1.0 - abs(u.x) / 0.7) * (1.0 - abs(u.y))
        p.z += 0.05 * math.sin(3.1 * u.x + ph) * math.cos(2.3 * u.y + ph)
        v.co = V((p.x * sx / 2, p.y * sy / 2, p.z * sz / 2 + sz / 2))
    paper = k(m['paper_dirty'] if dirty else m['paper'], 55, weld=True)
    pr = k(m['print_red'], 55, weld=True)
    pg = k(m['print_grey'], 55, weld=True)

    def pick(f):
        c = f.calc_center_median()
        n = f.normal
        if n.z > 0.6 and abs(c.x) < sx * 0.12 and not torn:
            return pr
        if n.y > 0.7 and -sx * 0.25 < c.x < sx * 0.05 and c.z > sz * 0.35:
            return pg
        return paper
    for f in src.faces:
        f.normal_update()
    emit(src, pick, mtx)


def build_cement_bags():
    rng = random.Random(81)
    m = mats_site()
    k = Kit()
    _euro_pallet(k, m, rng)
    z = 0.144
    S = (0.62, 0.40, 0.115)
    layers = 5
    for li in range(layers):
        if li % 2 == 0:
            spots = [(-0.3, -0.2, 0.0), (0.3, -0.2, 0.0), (-0.3, 0.2, 0.0), (0.3, 0.2, 0.0)]
        else:
            spots = [(-0.4, 0.0, math.pi / 2), (0.0, 0.0, math.pi / 2), (0.4, 0.0, math.pi / 2)]
        if li == layers - 1:
            spots = spots[:2]
        for si, (x, y, a) in enumerate(spots):
            torn = li == layers - 1 and si == 1
            Mb = tr(x + jitter(rng, 0.02), y + jitter(rng, 0.02), z) @ rot(jitter(rng, 0.02), jitter(rng, 0.02), a + jitter(rng, 0.06))
            _bag(k, m, rng, (S[0] + jitter(rng, 0.02), S[1] + jitter(rng, 0.015), S[2]), Mb, torn=torn, dirty=rng.random() < 0.3)
            if torn:  # ausgelaufener Zement im Riss und darunter
                blob(k(m['cement'], 35), (x, y, z + S[2] * 0.62), (0, 0, 1), 0.12, 0.04, rng, nseg=11)
        z += S[2] * 0.88
    # heruntergefallener Sack, an die Palette gelehnt
    _bag(k, m, rng, (0.62, 0.40, 0.11), tr(0.82, 0.05, 0.0) @ rot(0.0, -0.28, 1.35), dirty=True)
    # Zementstaub auf Palette und Boden
    for (x, y, zz, r_) in ((0.55, -0.3, 0.144, 0.08), (-0.62, 0.25, 0.0, 0.16), (0.66, -0.35, 0.0, 0.12), (0.2, 0.46, 0.0, 0.1)):
        blob(k(m['cement'], 35), (x, y, zz), (0, 0, 1), r_, 0.008, rng, nseg=10, rough=0.45)
    k.finish()


# --- Bewehrungsstahl ---------------------------------------------------------------------------------------------------
def _rebar(bm, p0, p1, r, rng, sag=0.0, n=6):
    """Rippenstahl: sternfoermiges Profil (Laengs- und Schraegrippen angedeutet) mit Verdrehung, leichtes Durchhaengen."""
    prof = [(math.cos(TAU * i / 8) * (1.0 if i % 2 == 0 else 0.82), math.sin(TAU * i / 8) * (1.0 if i % 2 == 0 else 0.82))
            for i in range(8)]
    pts = []
    for i in range(n + 1):
        t = i / n
        p = V(p0).lerp(V(p1), t)
        p.z -= sag * math.sin(math.pi * t)
        pts.append(p)
    sweep(bm, pts, r, profile=prof, twist=0.9)


def build_rebar_bundle():
    rng = random.Random(91)
    m = mats_site()
    k = Kit()
    rust = m['rust']
    rust2 = material('rost_dunkel', (0.42, 0.28, 0.20), look='metal', tex='metal_rusty_iron', rough=0.9, metal=0.3, r=1.4, m=0.1)
    timber = material('kantholz_alt', (0.50, 0.44, 0.36), look='wood', tex='wood_birch', rough=0.9, r=1.4)
    # Kanthoelzer quer (entlang y)
    for x in (-1.25, 0.0, 1.25):
        blk(k(timber), (0.10, 1.05, 0.10), (x + jitter(rng, 0.04), jitter(rng, 0.04), 0.05), rz=jitter(rng, 0.08), bevel=0.006)
    # Buendel: hexagonale Packung, Staebe 3,6 m (entlang x), Enden versetzt
    Lb = 3.6
    zc = 0.10 + 0.085
    bars = []
    for ring_r, cnt in ((0.0, 1), (0.032, 6), (0.064, 12), (0.092, 9)):
        for i in range(cnt):
            a = TAU * i / max(1, cnt) + ring_r * 7
            y, z = ring_r * math.cos(a), ring_r * math.sin(a) * 0.82
            if zc + z < 0.10 + 0.008:
                continue
            bars.append((y, zc + z))
    for (y, z) in bars:
        r = rng.choice((0.006, 0.007, 0.008))
        x0 = -Lb / 2 + jitter(rng, 0.12)
        x1 = Lb / 2 + jitter(rng, 0.12)
        spread = 1.0 + 0.6 * (abs(y) + abs(z - zc))
        _rebar(k(rust if rng.random() < 0.65 else rust2, 50), (x0, y * spread, z), (x1, y * spread * 1.05, z), r, rng,
               sag=0.004)
        if rng.random() < 0.3:  # Farbmarkierung am Stabende (Sorte)
            cyl(k(m['orange'], 50), (x1 - 0.03, y * spread * 1.05, z), (x1 + 0.001, y * spread * 1.05, z), r * 1.15, 8)
    # Bindedraht an drei Stellen, verdrillte Enden
    wire = material('bindedraht', (0.30, 0.29, 0.28), look='metal', rough=0.6, metal=0.8, r=1.1, m=0.6)
    for x in (-1.0, 0.1, 1.15):
        pts = [(x + 0.004 * math.sin(a * 3), 0.112 * math.cos(a), zc + 0.096 * math.sin(a)) for a in [TAU * i / 18 for i in range(19)]]
        sweep(k(wire, 60), pts, 0.0018, 4, caps=False)
        tail = [(x, 0.0, zc + 0.096), (x + 0.01, 0.004, zc + 0.115), (x + 0.004, -0.004, zc + 0.13), (x + 0.012, 0.0, zc + 0.145)]
        sweep(k(wire, 60), catmull(tail, 3), 0.002, 4)
    # Lieferschild (gelber Kunststoff) am Draht
    blk(k(material('schild_gelb', (0.86, 0.72, 0.12), look='plastic', rough=0.5, r=0.9)), (0.002, 0.07, 0.10),
        (1.15, 0.0, zc + 0.20), rx=0.15)
    cyl(k(wire, 60), (1.15, 0.0, zc + 0.145), (1.15, 0.0, zc + 0.25), 0.0012, 4)
    # Buegel (Steckbuegel) lose daneben
    for i in range(4):
        cx, cy = -0.6 + i * 0.35 + jitter(rng, 0.05), 0.42 + jitter(rng, 0.04)
        a = rng.uniform(-0.3, 0.3)
        w, d = 0.30, 0.20
        corners = [(-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2), (-w / 2, -d / 2 + 0.03), (-w / 2 + 0.06, -d / 2 + 0.06)]
        pts = []
        for (u, v) in corners:
            pts.append((cx + u * math.cos(a) - v * math.sin(a), cy + u * math.sin(a) + v * math.cos(a), 0.005 + i * 0.011))
        sweep(k(rust2, 50), pts, 0.005, 6)
    k.finish()


# --- Schalung an der Wand (Wandmodell: Wand bei x = 0, Ursprung am Boden) ------------------------------------------------
def _lean(x_foot, height, thick):
    """Neigung einer an die Wand gelehnten Tafel: Fuss bei x_foot, Oberkante an der Wand (x = thick)."""
    a = math.atan2(x_foot - thick, height)
    return a


def build_formwork():
    rng = random.Random(101)
    m = mats_site()
    k = Kit()
    yel = material('dreischicht_gelb', (0.88, 0.70, 0.30), look='wood', tex='wood_birch', rough=0.7, r=1.0)
    yel_old = material('dreischicht_alt', (0.72, 0.60, 0.34), look='wood', tex='wood_birch', rough=0.8, r=1.2)
    edge_red = material('kante_rot', (0.62, 0.16, 0.10), look='paint', rough=0.6, r=1.0)
    frame_c = material('rahmen_rot', (0.70, 0.13, 0.08), look='paint', rough=0.5, r=0.85)
    film = material('schalhaut', (0.30, 0.19, 0.12), look='wood', tex='wood_walnut_floor', rough=0.5, r=0.75)
    crust = m['concrete_crust']
    # Fuenf Dreischichtplatten 0,5 x 2,0 m, gestaffelt angelehnt (Fuss bei x = 0,32 .. 0,48)
    y = -1.45
    for i in range(5):
        w, h, t = 0.5, 2.0, 0.027
        xf = 0.30 + 0.035 * i + jitter(rng, 0.01)
        a = math.asin((xf - 0.03 - t * i * 0.9) / h)
        M = tr(xf, y + i * 0.06, 0.0) @ rot(0.0, -a, jitter(rng, 0.02))
        mat = yel if rng.random() < 0.6 else yel_old
        cube(k(mat), (-t, -w / 2, 0.0), (0.0, w / 2, h), M, bevel=0.002)
        cube(k(edge_red), (-t - 0.0005, -w / 2 - 0.0005, h - 0.004), (0.0005, w / 2 + 0.0005, h + 0.0005), M)
        for _ in range(rng.randint(2, 4)):  # Betonreste auf der Schalseite
            c = M @ V((0.0008, rng.uniform(-0.2, 0.2), rng.uniform(0.1, 1.6)))
            nrm = (M.to_3x3() @ V((1, 0, 0))).normalized()
            blob(k(crust, 30), tuple(c), tuple(nrm), rng.uniform(0.03, 0.09), 0.004, rng, squash=rng.uniform(0.5, 1.4))
    # Zwei Rahmentafeln 0,9 x 2,4 m, Rueckseite (Rahmen, Rippen) zum Raum
    y = 0.25
    for i in range(2):
        w, h, d = 0.9, 2.4, 0.12
        xf = 0.62 - 0.14 * i
        a = math.asin((xf - 0.04 - 0.13 * (1 - i)) / h)
        M = tr(xf, y + i * 0.08, 0.0) @ rot(0.0, -a, jitter(rng, 0.015))
        # Schalhaut zur Wand, Rahmen aus Hohlprofil (rot), Querrippen, Ankerloecher
        cube(k(film), (-0.018, -w / 2 + 0.005, 0.005), (0.0, w / 2 - 0.005, h - 0.005), M)
        fr = k(frame_c)
        cube(fr, (0.0, -w / 2, 0.0), (d, -w / 2 + 0.05, h), M, bevel=0.003)
        cube(fr, (0.0, w / 2 - 0.05, 0.0), (d, w / 2, h), M, bevel=0.003)
        cube(fr, (0.0, -w / 2, 0.0), (d, w / 2, 0.05), M, bevel=0.003)
        cube(fr, (0.0, -w / 2, h - 0.05), (d, w / 2, h), M, bevel=0.003)
        for zz in (0.55, 1.2, 1.85):
            cube(fr, (0.0, -w / 2 + 0.05, zz - 0.03), (d * 0.75, w / 2 - 0.05, zz + 0.03), M, bevel=0.002)
            for yy in (-0.2, 0.2):  # Ankerloch (Huelse)
                cyl(k(m['steel_dark'], 50), tuple(M @ V((d * 0.75, yy, zz + 0.03))), tuple(M @ V((d * 0.75 + 0.004, yy, zz + 0.03))), 0.014, 8)
        cube(k(m['galv_old']), (d * 0.5, -0.06, h - 0.05), (d, 0.06, h + 0.04), M, bevel=0.004)  # Kranoese
        for _ in range(5):  # Betonkruste an Rahmenkanten und Rippen
            c = M @ V((d + 0.0006, rng.choice((-w / 2 + 0.025, w / 2 - 0.025)), rng.uniform(0.1, 2.2)))
            blob(k(crust, 30), tuple(c), tuple((M.to_3x3() @ V((1, 0, 0))).normalized()), rng.uniform(0.015, 0.03), 0.004, rng)
        for _ in range(3):  # Kratzer/Abplatzer im roten Lack
            c = M @ V((d + 0.0006, rng.uniform(-0.4, 0.4), 0.025))
            blob(k(m['rust'], 30), tuple(c), tuple((M.to_3x3() @ V((1, 0, 0))).normalized()), rng.uniform(0.01, 0.025), 0.001, rng)
    # Zwei Deckenstuetzen (Stahlrohrstuetzen) schraeg angelehnt
    for i, (yb, xf) in enumerate(((1.25, 0.55), (1.38, 0.42))):
        top = V((0.05, yb - 0.1, 2.55 - i * 0.2))
        bot = V((xf, yb, 0.012))
        dirv = (top - bot).normalized()
        L = (top - bot).length
        cut = bot + dirv * (L * 0.6)
        cyl(k(m['orange'], 50), bot + dirv * 0.012, cut, 0.030, 10)   # Aussenrohr lackiert
        cyl(k(m['galv'], 50), cut, top - dirv * 0.012, 0.024, 10)     # Innenrohr verzinkt
        lathe(k(m['rust'], 50), [(0.0, -0.06), (0.042, -0.06), (0.044, 0.0), (0.042, 0.06), (0.0, 0.06)], 10,
              tr(*cut) @ dirv.to_track_quat('Z', 'Y').to_matrix().to_4x4())  # Stellmutter
        cyl(k(m['steel_dark'], 50), cut + dirv * 0.09 + V((0, -0.08, 0)), cut + dirv * 0.09 + V((0, 0.08, 0)), 0.007, 6)  # Steckbolzen
        for p in (bot, top):
            Mp = tr(*p) @ dirv.to_track_quat('Z', 'Y').to_matrix().to_4x4()
            cube(k(m['galv_old']), (-0.075, -0.075, -0.012), (0.075, 0.075, 0.0), Mp, bevel=0.002)
    # Kantholz und Keil am Boden
    blk(k(m['timber']), (0.08, 0.10, 2.5), (0.12, 1.02, 1.24), ry=-0.06, bevel=0.005)
    blk(k(m['timber']), (0.12, 0.08, 0.06), (0.62, -0.9, 0.03), rz=0.4, bevel=0.004)
    for _ in range(4):
        blob(k(crust, 30), (rng.uniform(0.35, 0.7), rng.uniform(-1.4, 1.2), 0.0), (0, 0, 1), rng.uniform(0.03, 0.07), 0.01, rng)
    k.finish()


# --- Baustrahler auf Stativ (leuchtend) ---------------------------------------------------------------------------------
def _floodlight(k, m, M, rng, lit=True):
    """Strahlergehaeuse 22 x 18 x 9 cm (Alu-Druckguss schwarz, Kuehlrippen), Glas leuchtend, Schutzgitter, Buegel."""
    body = material('alu_guss_schwarz', (0.05, 0.05, 0.055), look='metal', rough=0.6, metal=0.6, r=1.2, m=0.35)
    glow = material('strahler_licht', (1.0, 0.87, 0.66), look='glow', glow=1.7) if lit else m['glass_dirty']
    w, h, d = 0.22, 0.18, 0.09
    cube(k(body), (-d, -w / 2, -h / 2), (0.0, w / 2, h / 2), M, bevel=0.012)
    for i in range(7):  # Kuehlrippen hinten
        yy = -w / 2 + 0.02 + i * (w - 0.04) / 6
        cube(k(body), (-d - 0.025, yy - 0.003, -h / 2 + 0.015), (-d + 0.002, yy + 0.003, h / 2 - 0.015), M)
    cube(k(body), (-0.004, -w / 2, -h / 2), (0.012, w / 2, h / 2), M, bevel=0.006)  # Rahmen vorn
    cube(k(glow), (0.0, -w / 2 + 0.018, -h / 2 + 0.018), (0.0125, w / 2 - 0.018, h / 2 - 0.018), M)
    for zz in (-0.04, 0.0, 0.04):  # Schutzgitter
        cyl(k(m['steel_dark'], 50), tuple(M @ V((0.022, -w / 2 + 0.01, zz))), tuple(M @ V((0.022, w / 2 - 0.01, zz))), 0.0025, 5)
    for yy in (-w / 2 + 0.012, w / 2 - 0.012):
        cyl(k(m['steel_dark'], 50), tuple(M @ V((0.012, yy, -h / 2 + 0.01))), tuple(M @ V((0.022, yy, -0.04))), 0.0025, 5)
        cyl(k(m['steel_dark'], 50), tuple(M @ V((0.022, yy, 0.04))), tuple(M @ V((0.012, yy, h / 2 - 0.01))), 0.0025, 5)
    # Haltebuegel (U) mit Klemmknauf
    yl = w / 2 + 0.012
    bpts = [M @ V((-d * 0.5, -yl, 0.0)), M @ V((-d * 0.5, -yl, -h / 2 - 0.03)), M @ V((-d * 0.5, yl, -h / 2 - 0.03)), M @ V((-d * 0.5, yl, 0.0))]
    sweep(k(m['steel_dark'], 40), bpts, 0.006, 6, profile=[(1, 0.35), (-1, 0.35), (-1, -0.35), (1, -0.35)])
    for s in (-1, 1):
        lathe(k(m['black_plastic'], 40), [(0.0, 0.0), (0.016, 0.0), (0.02, 0.01), (0.016, 0.022), (0.0, 0.022)], 8,
              M @ tr(-d * 0.5, s * yl, 0.0) @ rot(-s * math.pi / 2, 0, 0))
    # Kabelverschraubung unten
    cyl(k(m['black_plastic'], 40), tuple(M @ V((-d * 0.5, 0.06, -h / 2))), tuple(M @ V((-d * 0.5, 0.06, -h / 2 - 0.03))), 0.01, 8)
    return M @ V((-d * 0.5, 0.06, -h / 2 - 0.03))


def build_work_light():
    rng = random.Random(111)
    m = mats_site()
    k = Kit()
    blackp = material('stativ_schwarz', (0.04, 0.04, 0.045), look='paint', rough=0.55, r=1.05)
    yel = m['yellow']
    zc = 0.62    # Spinne (Beinansatz)
    # drei Beine (Vierkant), Streben zur Mastmitte, Gummifuesse
    for i in range(3):
        a = TAU * i / 3 + 0.4
        foot = V((0.58 * math.cos(a), 0.58 * math.sin(a), 0.012))
        top = V((0.035 * math.cos(a), 0.035 * math.sin(a), zc))
        sweep(k(blackp, 40), [top, foot], 0.012, 4, profile=[(1, 1), (-1, 1), (-1, -1), (1, -1)])
        mid = top.lerp(foot, 0.45)
        sweep(k(blackp, 40), [mid, V((0.02 * math.cos(a), 0.02 * math.sin(a), zc - 0.30))], 0.008, 4,
              profile=[(1, 0.5), (-1, 0.5), (-1, -0.5), (1, -0.5)])
        blk(k(m['black_rubber']), (0.05, 0.035, 0.024), (foot.x, foot.y, 0.012), rz=a, bevel=0.006)
    lathe(k(blackp, 40), [(0.0, zc - 0.06), (0.05, zc - 0.06), (0.05, zc + 0.04), (0.0, zc + 0.04)], 12)   # Spinne
    lathe(k(blackp, 40), [(0.0, zc - 0.34), (0.03, zc - 0.34), (0.03, zc - 0.28), (0.0, zc - 0.28)], 10)   # Strebenring
    # Mast: Unterrohr gelb, Auszug verzinkt, Klemmen
    cyl(k(yel, 50), (0, 0, zc - 0.34), (0, 0, 1.30), 0.019, 12)
    cyl(k(m['galv'], 50), (0, 0, 1.28), (0, 0, 1.86), 0.0145, 12)
    for zz in (1.30, zc + 0.04):
        lathe(k(m['black_plastic'], 40), [(0.0, zz - 0.02), (0.026, zz - 0.02), (0.026, zz + 0.03), (0.0, zz + 0.03)], 10)
        cyl(k(m['black_plastic'], 40), (0.02, 0, zz + 0.005), (0.06, 0, zz + 0.005), 0.005, 6)
        lathe(k(m['black_plastic'], 40), [(0.0, -0.008), (0.018, -0.008), (0.02, 0.0), (0.018, 0.008), (0.0, 0.008)], 6,
              tr(0.065, 0, zz + 0.005) @ rot(0, math.pi / 2, 0))   # Sternknauf (6-eckig)
    # T-Traverse
    cyl(k(blackp, 50), (0, -0.33, 1.88), (0, 0.33, 1.88), 0.014, 8)
    lathe(k(blackp, 40), [(0.0, 1.84), (0.024, 1.84), (0.024, 1.90), (0.0, 1.90)], 10)
    ends = []
    for s in (-1, 1):
        M = tr(0.05, s * 0.21, 1.98) @ rot(0.0, math.radians(18), jitter(rng, 0.08))
        ends.append(_floodlight(k, m, M, rng, lit=True))
        cyl(k(blackp, 50), (0.0, s * 0.21, 1.88), (0.02, s * 0.21, 1.95), 0.008, 6)
    # Kabel: von den Strahlern am Mast herunter (Klebeband), lose Schlaufe am Boden, Stecker
    cab = material('kabel_gummi', (0.06, 0.06, 0.06), look='rubber', rough=0.7, r=1.05)
    tape = material('klebeband_gelb', (0.80, 0.66, 0.10), look='plastic', rough=0.6, r=1.0)
    for i, e in enumerate(ends):
        s = -1 if i == 0 else 1
        pts = [e, e + V((0, 0, -0.10)), V((0.03, s * 0.03, 1.70)), V((0.025, s * 0.012, 1.35)), V((0.024, 0.0, 1.0)),
               V((0.024, 0.0, 0.70))]
        sweep(k(cab, 60), catmull(pts, 4), 0.0045, 6)
    tail = [V((0.024, 0.0, 0.70)), V((0.05, 0.02, 0.45)), V((0.18, 0.12, 0.05)), V((0.40, 0.28, 0.008)), V((0.62, 0.20, 0.008)),
            V((0.70, -0.05, 0.008)), V((0.55, -0.25, 0.008)), V((0.30, -0.30, 0.008)), V((0.12, -0.42, 0.008)), V((-0.10, -0.50, 0.008))]
    sweep(k(cab, 60), catmull(tail, 5), 0.0055, 6)
    for zz in (1.55, 1.12, 0.82):
        cyl(k(tape, 50), (0.0, 0.0, zz - 0.025), (0.0, 0.0, zz + 0.025), 0.024, 10)
    # Schukostecker
    p = V((-0.10, -0.50, 0.008))
    Mp = tr(p.x - 0.03, p.y - 0.01, 0.022) @ rot(0, math.pi / 2, 0.3)
    lathe(k(m['black_plastic'], 40), [(0.0, -0.02), (0.022, -0.02), (0.024, 0.02), (0.016, 0.045), (0.0, 0.05)], 10, Mp)
    for s in (-1, 1):
        cyl(k(m['zinc'], 40), tuple(Mp @ V((0, s * 0.009, -0.02))), tuple(Mp @ V((0, s * 0.009, -0.039))), 0.002, 5)
    k.finish()


# --- Betonmischer ------------------------------------------------------------------------------------------------------
def build_cement_mixer():
    rng = random.Random(121)
    m = mats_site()
    k = Kit()
    drum_c = material('trommel_orange', (0.84, 0.36, 0.06), look='paint', rough=0.45, r=0.8)
    drum_worn = material('trommel_matt', (0.62, 0.30, 0.10), look='paint', rough=0.7, r=1.2)
    frame_c = material('gestell_grau', (0.28, 0.30, 0.31), look='paint', rough=0.6, r=1.05)
    crust = m['concrete_crust']
    # Fahrgestell: zwei Laengsholme, Querrohre, Stuetzbeine vorn, Achse hinten
    zf = 0.30
    for s in (-1, 1):
        sweep(k(frame_c, 40), [(-0.42, s * 0.30, zf), (0.40, s * 0.30, zf)], 0.022, 4, profile=[(1, 1), (-1, 1), (-1, -1), (1, -1)])
        sweep(k(frame_c, 40), [(0.36, s * 0.30, zf), (0.42, s * 0.33, 0.02)], 0.018, 4, profile=[(1, 1), (-1, 1), (-1, -1), (1, -1)])
        blk(k(m['black_rubber']), (0.08, 0.06, 0.02), (0.42, s * 0.33, 0.01), bevel=0.004)
        # Pfosten zum Drehlager
        sweep(k(frame_c, 40), [(-0.05, s * 0.30, zf), (-0.05, s * 0.30, 0.78)], 0.024, 4, profile=[(1, 1), (-1, 1), (-1, -1), (1, -1)])
        sweep(k(frame_c, 40), [(-0.30, s * 0.30, zf), (-0.06, s * 0.30, 0.70)], 0.014, 4, profile=[(1, 1), (-1, 1), (-1, -1), (1, -1)])
    for x in (-0.38, 0.30):
        cyl(k(frame_c, 40), (x, -0.30, zf), (x, 0.30, zf), 0.018, 8)
    # Raeder hinten (Luftreifen, Felge)
    for s in (-1, 1):
        Mw = tr(-0.42, s * 0.40, 0.18) @ rot(math.pi / 2, 0, 0)
        lathe(k(m['black_rubber'], 50), [(0.10, -0.035), (0.14, -0.04), (0.17, -0.03), (0.18, 0.0), (0.17, 0.03), (0.14, 0.04),
                                        (0.10, 0.035)], 18, Mw)
        lathe(k(m['zinc'], 40), [(0.0, -0.03), (0.10, -0.03), (0.10, 0.03), (0.0, 0.03)], 12, Mw)
        lathe(k(m['steel_dark'], 40), [(0.0, -0.05), (0.025, -0.05), (0.025, 0.05), (0.0, 0.05)], 8, Mw)
        cyl(k(frame_c, 40), (-0.42, s * 0.30, zf - 0.02), (-0.42, s * 0.36, 0.18), 0.012, 6)
    cyl(k(m['steel_dark'], 40), (-0.42, -0.36, 0.18), (-0.42, 0.36, 0.18), 0.012, 8)
    # Trommeljoch (Gabel), um y kippbar; Trommel 35 Grad nach vorn geneigt
    piv = V((-0.05, 0.0, 0.80))
    tilt = math.radians(38)
    Md = tr(*piv) @ rot(0.0, tilt, 0.0) @ tr(0, 0, 0.05)
    for s in (-1, 1):
        cyl(k(frame_c, 40), (piv.x, s * 0.30, piv.z), (piv.x, s * 0.40, piv.z), 0.03, 10)  # Lager
        sweep(k(frame_c, 40), [tuple(V((piv.x, s * 0.36, piv.z))), tuple(Md @ V((0, s * 0.36, -0.30))),
                               tuple(Md @ V((0, s * 0.2, -0.40)))], 0.018, 4, profile=[(1, 1), (-1, 1), (-1, -1), (1, -1)])
    # Trommel (Drehkoerper): Boden, Bauch mit Zahnkranz, Konus zur Oeffnung, Lippe, innen zurueck
    outer = [(0.0, -0.40), (0.20, -0.39), (0.30, -0.33), (0.335, -0.22), (0.34, -0.10), (0.335, 0.02), (0.30, 0.12),
             (0.24, 0.26), (0.19, 0.38), (0.185, 0.41), (0.20, 0.425), (0.205, 0.44)]
    inner = [(0.185, 0.445), (0.17, 0.43), (0.175, 0.40), (0.225, 0.27), (0.285, 0.12), (0.315, 0.0), (0.315, -0.12),
             (0.29, -0.28), (0.18, -0.36), (0.0, -0.37)]
    src = bmesh.new()
    lathe(src, outer + inner, 28)
    for f in src.faces:
        f.normal_update()
    ph = rng.uniform(0, 6)

    def pick(f):
        c = f.calc_center_median()
        r = math.hypot(c.x, c.y)
        inside = c.z > -0.36 and f.normal.dot(V((c.x, c.y, 0.0))) < 0
        if inside:
            return k(crust, 40, weld=True)
        if c.z > 0.36:  # Lippe und Rand mit Beton verkrustet
            return k(crust, 40, weld=True)
        a = math.atan2(c.y, c.x)
        return k(drum_worn if (c.z < -0.25 and math.sin(a * 2 + ph) > 0.3) else drum_c, 40, weld=True)
    emit(src, pick, Md)
    # Betonablaeufe unter der Lippe: Ketten kleiner werdender Kleckse auf dem Konus

    def r_at(z):
        for (r1, z1), (r2, z2) in zip(outer, outer[1:]):
            if z1 <= z <= z2 and z2 > z1:
                t = (z - z1) / (z2 - z1)
                return r1 + (r2 - r1) * t, V((z2 - z1, 0.0, -(r2 - r1))).normalized()
        return 0.3, V((1, 0, 0))
    for i in range(7):
        a = rng.uniform(0, TAU)
        n_len = rng.randint(2, 5)
        for s in range(n_len):
            z = 0.37 - s * 0.045 - rng.uniform(0, 0.01)
            r, nl = r_at(z)
            ca, sa = math.cos(a), math.sin(a)
            p = Md @ V((r * ca, r * sa, z))
            nn = Md.to_3x3() @ V((nl.x * ca, nl.x * sa, nl.z))
            blob(k(crust, 30), tuple(p + nn * 0.001), tuple(nn), 0.03 * (1.0 - 0.15 * s) * rng.uniform(0.7, 1.2), 0.004, rng,
                 nseg=8, squash=0.5)
    # Zahnkranz am Bauch
    for i in range(40):
        a = TAU * i / 40
        c = Md @ V((0.352 * math.cos(a), 0.352 * math.sin(a), -0.16))
        tang = (Md.to_3x3() @ V((-math.sin(a), math.cos(a), 0.0)))
        radial = (Md.to_3x3() @ V((math.cos(a), math.sin(a), 0.0)))
        ax = Md.to_3x3() @ V((0, 0, 1))
        Mt = Matrix((
            (radial.x, tang.x, ax.x, c.x),
            (radial.y, tang.y, ax.y, c.y),
            (radial.z, tang.z, ax.z, c.z),
            (0, 0, 0, 1)))
        cube(k(m['steel_dark']), (-0.016, -0.011, -0.03), (0.014, 0.011, 0.03), Mt)
    lathe(k(m['steel_dark'], 40), [(0.335, -0.20), (0.342, -0.20), (0.342, -0.12), (0.335, -0.12)], 28, Md)
    # Mischschaufeln innen (sichtbar durch die Oeffnung)
    for i in range(3):
        a = TAU * i / 3 + 0.3
        p0 = Md @ V((0.30 * math.cos(a), 0.30 * math.sin(a), -0.05))
        p1 = Md @ V((0.15 * math.cos(a + 0.6), 0.15 * math.sin(a + 0.6), 0.15))
        sweep(k(crust, 40), [p0, p1], 0.02, 4, profile=[(1, 0.15), (-1, 0.15), (-1, -0.15), (1, -0.15)])
    # Motor mit Ritzel und Schalter seitlich am Joch
    mot = material('motor_grau', (0.40, 0.42, 0.43), look='paint', rough=0.5, r=0.95)
    Mm = tr(-0.18, -0.36, 0.52) @ rot(math.pi / 2, 0, 0)
    lathe(k(mot, 40), [(0.0, -0.09), (0.075, -0.09), (0.08, -0.07), (0.08, 0.08), (0.06, 0.10), (0.0, 0.10)], 14, Mm)
    for i in range(10):  # Kuehlrippen
        a = TAU * i / 10
        cube(k(mot), (0.074, -0.004, -0.06), (0.09, 0.004, 0.07), Mm @ rot(0, 0, a))
    cube(k(m['black_plastic']), (-0.05, -0.04, -0.03), (0.05, 0.04, 0.05), tr(-0.18, -0.36, 0.64), bevel=0.008)
    cyl(k(m['red'], 40), (-0.18, -0.40, 0.62), (-0.18, -0.415, 0.62), 0.012, 8)
    cyl(k(material('taster_gruen', (0.10, 0.50, 0.18), look='plastic', r=0.9), 40), (-0.16, -0.40, 0.60), (-0.16, -0.41, 0.60), 0.009, 8)
    # Kipp-Handrad auf der +y-Seite
    hw = V((piv.x, 0.47, piv.z))
    Mhw = tr(*hw) @ rot(math.pi / 2, 0, 0)
    ring = [Mhw @ V((0.17 * math.cos(a), 0.17 * math.sin(a), 0.0)) for a in [TAU * i / 20 for i in range(21)]]
    sweep(k(m['black_plastic'], 50), ring, 0.013, 8, caps=False)
    for i in range(3):
        a = TAU * i / 3 + 0.5
        cyl(k(frame_c, 50), tuple(Mhw @ V((0, 0, 0))), tuple(Mhw @ V((0.165 * math.cos(a), 0.165 * math.sin(a), 0.0))), 0.008, 6)
    cyl(k(frame_c, 50), (piv.x, 0.40, piv.z), tuple(hw), 0.025, 10)
    # Kabel zum Boden und Beton am Boden
    cab = material('kabel_gummi', (0.06, 0.06, 0.06), look='rubber', rough=0.7, r=1.05)
    sweep(k(cab, 60), catmull([(-0.18, -0.40, 0.66), (-0.30, -0.42, 0.55), (-0.40, -0.40, 0.3), (-0.50, -0.30, 0.008),
                                (-0.70, -0.10, 0.008), (-0.75, 0.25, 0.008)], 4), 0.005, 6)
    for _ in range(6):
        blob(k(crust, 30), (rng.uniform(0.05, 0.6), rng.uniform(-0.35, 0.35), 0.0), (0, 0, 1), rng.uniform(0.03, 0.08),
             rng.uniform(0.006, 0.02), rng)
    for _ in range(5):
        x = rng.uniform(-0.38, 0.38)
        blob(k(crust, 30), (x, rng.choice((-0.30, 0.30)), zf + 0.022), (0, 0, 1), 0.02, 0.006, rng)
    k.finish()


# --- Schubkarre --------------------------------------------------------------------------------------------------------
def _rrect(cx, cy, w, d, r, n=4):
    """Abgerundetes Rechteck (gegen den Uhrzeigersinn), Ecken mit n Segmenten."""
    pts = []
    for (qx, qy, a0) in ((1, 1, 0.0), (-1, 1, math.pi / 2), (-1, -1, math.pi), (1, -1, 1.5 * math.pi)):
        ccx, ccy = cx + qx * (w / 2 - r), cy + qy * (d / 2 - r)
        for i in range(n + 1):
            a = a0 + (math.pi / 2) * i / n
            pts.append((ccx + r * math.cos(a), ccy + r * math.sin(a)))
    return pts


def build_wheelbarrow():
    rng = random.Random(131)
    m = mats_site()
    k = Kit()
    tub_c = m['galv']
    frame_c = material('karre_gruen', (0.10, 0.24, 0.14), look='paint', rough=0.5, r=0.9)
    crust = m['concrete_crust']
    # Mulde: Ringe von unten (klein) nach oben (gross), Vorderwand flach geneigt
    rings_def = [(0.30, 0.50, 0.38, 0.06, 0.00), (0.36, 0.62, 0.46, 0.08, 0.06), (0.44, 0.76, 0.54, 0.09, 0.14),
                 (0.52, 0.88, 0.60, 0.10, 0.22), (0.58, 0.96, 0.64, 0.10, 0.28)]
    src = bmesh.new()
    rings = []
    for (z, w, d, r, shift) in rings_def:
        pts = _rrect(0.08 + shift, 0.0, w, d, r)
        rings.append([src.verts.new((x, y, z)) for (x, y) in pts])
    n = len(rings[0])
    for i in range(len(rings) - 1):
        for j in range(n):
            j2 = (j + 1) % n
            src.faces.new((rings[i][j], rings[i][j2], rings[i + 1][j2], rings[i + 1][j]))
    src.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(src, faces=list(src.faces))
    # Mulde zweiseitig: aussen verzinkt, innen verzinkt mit Betonkruste am Boden
    out_faces = [[v.co.copy() for v in f.verts] for f in src.faces]
    src.free()
    tub = k(tub_c, 40, weld=True)
    for pts in out_faces:
        tub.faces.new([tub.verts.new(p) for p in pts])
        c = sum(pts, V()) / len(pts)
        inside = k(crust if (c.z < 0.36 or (c.z < 0.45 and rng.random() < 0.4)) else m['galv_old'], 40, weld=True)
        inside.faces.new([inside.verts.new(p + V((0, 0, 0.0)) - (p - V((0.36, 0.0, 0.62))).normalized() * 0.0015)
                          for p in reversed(pts)])
    # Rollrand oben
    top = rings_def[-1]
    rim = [V((x, y, top[0] + 0.004)) for (x, y) in _rrect(0.08 + top[4], 0.0, top[1] + 0.012, top[2] + 0.012, top[3])]
    sweep(k(tub_c, 50), rim + [rim[0]], 0.009, 8, caps=False)
    # Betonreste innen am Boden (Klumpen) und ein paar Spritzer aussen
    for _ in range(5):
        blob(k(crust, 30), (rng.uniform(0.0, 0.3), rng.uniform(-0.12, 0.12), 0.302), (0, 0, 1), rng.uniform(0.04, 0.08),
             rng.uniform(0.01, 0.03), rng)
    # Rahmen: zwei Holme vom Rad zu den Griffen, Beine, Querstreben
    for s in (-1, 1):
        pts = [V((0.62, s * 0.06, 0.20)), V((0.48, s * 0.20, 0.27)), V((0.20, s * 0.24, 0.29)), V((-0.30, s * 0.27, 0.40)),
               V((-0.80, s * 0.29, 0.56)), V((-0.98, s * 0.30, 0.60))]
        sweep(k(frame_c, 55), catmull(pts, 3), 0.016, 8)
        # Griffe (Gummi)
        sweep(k(m['black_rubber'], 55), [V((-0.84, s * 0.292, 0.575)), V((-1.0, s * 0.30, 0.605))], 0.02, 8)
        # Bein mit Fussbuegel
        leg = [V((-0.30, s * 0.27, 0.40)), V((-0.32, s * 0.28, 0.12)), V((-0.36, s * 0.28, 0.012)), V((-0.20, s * 0.28, 0.012))]
        sweep(k(frame_c, 55), catmull(leg, 3), 0.014, 8)
        # Gabel zum Rad
        sweep(k(frame_c, 55), [V((0.48, s * 0.20, 0.27)), V((0.60, s * 0.06, 0.20))], 0.012, 6)
        # Muldenhalter (Flachstahl)
        sweep(k(frame_c, 40), [V((0.20, s * 0.24, 0.29)), V((0.18, s * 0.21, 0.31))], 0.01, 4)
    for x, z in ((0.20, 0.29), (-0.25, 0.39)):
        cyl(k(frame_c, 50), (x, -0.25, z), (x, 0.25, z), 0.012, 8)
    # Rad: Luftreifen, Stahlfelge, Nabe, Achse
    Mw = tr(0.62, 0.0, 0.19) @ rot(math.pi / 2, 0, 0)
    lathe(k(m['black_rubber'], 50), [(0.11, -0.04), (0.15, -0.045), (0.18, -0.035), (0.19, 0.0), (0.18, 0.035), (0.15, 0.045),
                                    (0.11, 0.04)], 20, Mw)
    lathe(k(material('felge_rot', (0.55, 0.10, 0.07), look='paint', r=0.9), 40), [(0.0, -0.035), (0.11, -0.035), (0.11, 0.035),
                                                                                    (0.0, 0.035)], 14, Mw)
    lathe(k(m['zinc'], 40), [(0.0, -0.06), (0.03, -0.06), (0.03, 0.06), (0.0, 0.06)], 8, Mw)
    for i in range(14):  # Profilstollen
        a = TAU * i / 14
        cube(k(m['black_rubber']), (0.185, -0.012, -0.03), (0.197, 0.012, 0.03), Mw @ rot(0, 0, a))
    k.finish()


# --- Saegebock mit Brett -------------------------------------------------------------------------------------------------
def build_sawhorse():
    rng = random.Random(141)
    m = mats_site()
    k = Kit()
    wood = material('bock_holz', (0.66, 0.54, 0.38), look='wood', tex='wood_birch', rough=0.85, r=1.25)
    wood_d = material('bock_holz_alt', (0.50, 0.42, 0.32), look='wood', tex='wood_birch', rough=0.9, r=1.35)
    zb = 0.66
    # Balken (entlang y), vier gespreizte Beine, Stirnbretter, Winkelbeschlaege
    blk(k(wood), (0.09, 1.0, 0.09), (0.0, 0.0, zb - 0.045), bevel=0.004)
    for sy in (-1, 1):
        for sx in (-1, 1):
            top = V((sx * 0.03, sy * 0.40, zb - 0.06))
            foot = V((sx * 0.32, sy * 0.46, 0.0))
            d = (foot - top)
            Ml = tr(*((top + foot) / 2)) @ d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
            cube(k(wood_d if sx > 0 else wood), (-0.022, -0.035, -d.length / 2), (0.022, 0.035, d.length / 2), Ml, bevel=0.003)
        # Stirnbrett (Sperrholz-Dreieck) und Querlatte
        tri = [(-0.13, sy * 0.43, 0.30), (0.13, sy * 0.43, 0.30), (0.04, sy * 0.43, zb - 0.09), (-0.04, sy * 0.43, zb - 0.09)]
        poly = [V(p) for p in tri]
        bm = k(material('sperrholz', (0.52, 0.36, 0.22), look='wood', tex='wood_walnut_floor', rough=0.8, r=1.2))
        for off, nn in ((0.012, (0, sy, 0)), (-0.012, (0, -sy, 0))):
            quad_n(bm, [p + V((0, off * sy, 0)) for p in poly], nn)
        cube(k(wood), (-0.27, sy * 0.43 - 0.012, 0.12), (0.27, sy * 0.43 + 0.012, 0.19), bevel=0.003)
        for sx in (-1, 1):  # Nagelkoepfe
            cyl(k(m['steel_dark']), (sx * 0.22, sy * 0.43 + sy * 0.012, 0.155), (sx * 0.22, sy * 0.43 + sy * 0.0135, 0.155), 0.005, 5)
    # Brett schraeg aufgelegt: ein Ende am Boden, das andere ueber dem Balken
    p_floor = V((-1.05, -0.12, 0.0))
    p_top = V((0.0, 0.06, zb))
    d = p_top - p_floor
    ang = math.atan2(d.y, d.x)
    pitch = math.atan2(zb + 0.0, math.hypot(d.x, d.y))
    Lb = math.hypot(d.x, d.y) / math.cos(pitch) + 0.5
    Mb = tr(*p_floor) @ rot(0, 0, ang) @ rot(0, -pitch, 0)
    cube(k(material('brett_frisch', (0.80, 0.68, 0.50), look='wood', tex='wood_birch', rough=0.8, r=1.2)),
         (0.0, -0.10, 0.0), (Lb, 0.10, 0.028), Mb, bevel=0.002)
    # Saegeschnitt am oberen Ende: Kerbe
    cube(k(m['steel_dark']), (Lb - 0.25, -0.10, 0.012), (Lb - 0.247, -0.02, 0.0285), Mb)
    # Fuchsschwanz auf dem Brett
    Ms = Mb @ tr(Lb * 0.55, 0.0, 0.029) @ rot(0, 0, 0.25)
    blade = [(0.0, -0.06), (0.42, -0.03), (0.42, -0.005), (0.0, 0.05)]
    poly_prism(k(material('saegeblatt', (0.62, 0.63, 0.64), look='metal', rough=0.35, metal=0.9, r=0.7, m=0.8)), blade, 0.0, 0.0012, Ms)
    handle = [(-0.13, -0.05), (0.02, -0.065), (0.02, 0.06), (-0.04, 0.07), (-0.14, 0.04)]
    poly_prism(k(material('griff_rot', (0.60, 0.08, 0.06), look='plastic', rough=0.5, r=0.9)), handle, -0.008, 0.012, Ms)
    poly_prism(k(m['black_plastic']), [(-0.10, -0.02), (-0.03, -0.03), (-0.03, 0.03), (-0.10, 0.025)], -0.0085, 0.0125, Ms)
    # Saegemehl und Abschnitte am Boden
    dust = material('saegemehl', (0.82, 0.70, 0.50), look='fabric', rough=1.0, r=1.5)
    for (x, y, r_) in ((0.30, 0.55, 0.16), (0.20, 0.35, 0.09), (0.45, 0.75, 0.07)):
        blob(k(dust, 35), (x, y, 0.0), (0, 0, 1), r_, 0.012, rng, nseg=11, rough=0.4)
    blk(k(wood), (0.25, 0.10, 0.028), (0.45, 0.15, 0.014), rz=0.7, bevel=0.002)
    blk(k(wood_d), (0.12, 0.09, 0.09), (-0.30, 0.55, 0.045), rz=0.3, bevel=0.003)
    k.finish()


# --- Absperrgitter rot-weiss -----------------------------------------------------------------------------------------
def build_site_barrier():
    rng = random.Random(151)
    m = mats_site()
    k = Kit()
    red, white = m['red'], m['white']
    L, H = 2.0, 1.0
    z0 = 0.10
    # Rahmen (Rohr 40 mm), rot-weiss in Abschnitten von 25 cm
    def banded(p0, p1, r, seg=0.25, start_red=True):
        p0, p1 = V(p0), V(p1)
        n = max(1, round((p1 - p0).length / seg))
        for i in range(n):
            a, b = p0.lerp(p1, i / n), p0.lerp(p1, (i + 1) / n)
            cyl(k(red if (i % 2 == 0) == start_red else white, 50), a, b, r, 10)
    for s in (-1, 1):
        banded((0, s * L / 2, z0 - 0.06), (0, s * L / 2, z0 + H), 0.02, start_red=(s > 0))
    banded((0, -L / 2, z0 + H - 0.02), (0, L / 2, z0 + H - 0.02), 0.02)
    banded((0, -L / 2, z0 + 0.05), (0, L / 2, z0 + 0.05), 0.016, start_red=False)
    # Fuellstaebe 12 mm
    n = 15
    for i in range(1, n):
        y = -L / 2 + L * i / n
        cyl(k(m['galv_old'], 50), (0, y, z0 + 0.065), (0, y, z0 + H - 0.04), 0.006, 6)
    # Fuesse: Recyclinggummi mit Fase, Steckhuelse
    for s in (-1, 1):
        blk(k(m['black_rubber']), (0.62, 0.15, 0.10), (0.0, s * L / 2, 0.05), bevel=0.02)
        cyl(k(m['galv_old'], 50), (0, s * L / 2, 0.09), (0, s * L / 2, 0.16), 0.026, 10)
    # Lackschaeden (Rost) am Handlauf und an den Pfosten
    for _ in range(8):
        y = rng.uniform(-L / 2, L / 2)
        a = rng.uniform(0, TAU)
        c = V((0.02 * math.cos(a), y, z0 + H - 0.02 + 0.02 * math.sin(a)))
        blob(k(m['rust'], 30), tuple(c + V((0.0005 * math.cos(a), 0, 0.0005 * math.sin(a)))), (math.cos(a), 0, math.sin(a)),
             rng.uniform(0.006, 0.014), 0.0008, rng, squash=0.6)
    # Warnleuchte (gelb, Klemmhalter) auf dem linken Pfosten
    top = V((0.0, -L / 2, z0 + H + 0.02))
    blk(k(m['black_plastic']), (0.05, 0.05, 0.06), (top.x, top.y, top.z + 0.03), bevel=0.006)
    lathe(k(m['black_plastic'], 40), [(0.0, 0.0), (0.085, 0.0), (0.085, 0.05), (0.0, 0.05)], 16, tr(top.x, top.y, top.z + 0.06))
    lathe(k(material('warnlicht_gelb', (1.0, 0.62, 0.10), look='glow', glow=0.9), 50),
          [(0.07, 0.0), (0.075, 0.02), (0.072, 0.06), (0.05, 0.085), (0.0, 0.092)], 16, tr(top.x, top.y, top.z + 0.11))
    k.finish()


# --- Haufen aus Abdeckplane ------------------------------------------------------------------------------------------------
def build_tarp_pile():
    rng = random.Random(161)
    m = mats_site()
    k = Kit()
    blue = m['tarp_blue']
    dusty = material('plane_blau_staub', (0.30, 0.38, 0.50), look='plastic', rough=0.8, r=1.2)
    # Unterlage: was die Plane verdeckt (Stapel, Sack, Rolle) als Huellform
    lumps = [((0.0, 0.0), (0.55, 0.34), 0.30, 3.0), ((-0.45, 0.30), (0.25, 0.22), 0.22, 2.0), ((0.45, -0.25), (0.28, 0.2), 0.16, 2.0),
             ((0.15, 0.35), (0.2, 0.2), 0.12, 2.0)]

    def height(x, y):
        h = 0.0
        for (cx, cy), (rx, ry), hh, p in lumps:
            d = ((abs(x - cx) / rx) ** p + (abs(y - cy) / ry) ** p) ** (1.0 / p)
            h = max(h, hh * max(0.0, 1.0 - d ** 2) ** 0.5)
        return h
    NX, NY = 30, 24
    X0, X1, Y0, Y1 = -0.95, 0.95, -0.75, 0.75
    grid = []
    for j in range(NY + 1):
        row = []
        for i in range(NX + 1):
            x = X0 + (X1 - X0) * i / NX
            y = Y0 + (Y1 - Y0) * j / NY
            # weich gezogen: Mittel aus der Umgebung (Plane spannt ueber Kanten)
            h = 0.0
            for dx, dy, w in ((0, 0, 0.4), (0.08, 0, 0.15), (-0.08, 0, 0.15), (0, 0.08, 0.15), (0, -0.08, 0.15)):
                h += w * height(x + dx, y + dy)
            # Falten: Wellen quer zur Zugrichtung, zum Rand hin flach
            edge = min((x - X0), (X1 - x), (y - Y0), (Y1 - y))
            fade = min(1.0, edge / 0.25)
            h += 0.022 * math.sin(x * 17 + 3 * math.sin(y * 5)) * math.sin(y * 11 + 1.3) * fade
            h += 0.012 * fade + 0.002
            # unregelmaessiger Rand (Plane liegt in Lappen)
            xx = x + 0.04 * math.sin(y * 9 + 1) * (1 - fade)
            yy = y + 0.04 * math.sin(x * 7 + 2) * (1 - fade)
            row.append(V((xx, yy, max(0.003, h))))
        grid.append(row)
    # umgeschlagene Ecke (vorn rechts): ein Lappen liegt doppelt
    src = bmesh.new()
    vs = [[src.verts.new(p) for p in row] for row in grid]
    for j in range(NY):
        for i in range(NX):
            src.faces.new((vs[j][i], vs[j][i + 1], vs[j + 1][i + 1], vs[j + 1][i]))
    for f in src.faces:
        f.normal_update()
    ph = rng.uniform(0, 6)

    def pick(f):
        c = f.calc_center_median()
        return k(blue, 45, weld=True)
    faces_pts = [[v.co.copy() for v in f.verts] for f in src.faces]
    emit(src, pick)
    # Unterseite (fuer angehobene Lappen sichtbar)
    under = k(blue, 45, weld=True)
    for pts in faces_pts:
        under.faces.new([under.verts.new(p - V((0, 0, 0.002))) for p in reversed(pts)])
    # Umschlag: zweite Lage auf der +x-Seite, eingerollt
    fold = []
    for j in range(10):
        row = []
        for i in range(7):
            y = -0.55 + 1.1 * j / 9
            t = i / 6
            x = 0.95 - 0.30 * t
            hz = 0.004 + 0.06 * math.sin(math.pi * t) + 0.01 * math.sin(y * 13)
            row.append(V((x + 0.02 * math.sin(y * 7), y, hz + 0.003)))
        fold.append(row)
    fb = k(blue, 45, weld=True)
    fb2 = k(blue, 45, weld=True)
    for j in range(9):
        for i in range(6):
            q = (fold[j][i], fold[j][i + 1], fold[j + 1][i + 1], fold[j + 1][i])
            quad_n(fb, q, (0, 0, 1))
            quad_n(fb2, [p - V((0, 0, 0.002)) for p in q], (0, 0, -1))
    # Saum mit Oesen entlang der Umschlagkante
    hem = [fold[j][6] for j in range(10)]
    sweep(k(m['tarp_hem'], 50), [p + V((0, 0, 0.004)) for p in hem], 0.006, 6)
    for j in range(0, 10, 2):
        p = fold[j][6] + V((0.02, 0, 0.01))
        lathe(k(m['zinc'], 50), [(0.006, -0.002), (0.011, -0.002), (0.011, 0.002), (0.006, 0.002), (0.006, -0.002)], 8,
              tr(p.x, p.y, p.z + 0.004))
    # zwei Ziegel beschweren die Plane
    for (x, y, a) in ((-0.1, -0.05, 0.3), (0.35, 0.25, -0.5)):
        h = 0.0
        for row in grid:
            for p in row:
                if abs(p.x - x) < 0.06 and abs(p.y - y) < 0.06:
                    h = max(h, p.z)
        blk(k(m['brick']), (0.24, 0.115, 0.071), (x, y, h + 0.035), rz=a, rx=jitter(rng, 0.08), bevel=0.004)
    k.finish()
