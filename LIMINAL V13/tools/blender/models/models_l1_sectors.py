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

    def finish(self, mtx=None):
        """Objekte anlegen; mtx dreht/verschiebt am Ende das ganze Modell (z. B. Achsen an die Karte anpassen)."""
        for i, (mat, bm, smooth, weld) in enumerate(self.parts.values()):
            if not bm.faces:
                bm.free()
                continue
            if mtx is not None:
                bmesh.ops.transform(bm, matrix=mtx, verts=list(bm.verts))
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
        'plank': material('bohle', (0.62, 0.55, 0.44), look='wood', rough=0.85, r=1.25),
        'plank_old': material('bohle_alt', (0.47, 0.45, 0.40), look='wood', rough=0.9, r=1.4),
        'timber': material('kantholz', (0.60, 0.52, 0.40), look='wood', rough=0.85, r=1.3),
        'red': material('rot_lack', (0.66, 0.10, 0.07), look='paint', rough=0.5, r=0.8),
        'red_worn': material('rot_matt', (0.52, 0.12, 0.09), look='paint', rough=0.7, r=1.15),
        'white': material('weiss_lack', (0.86, 0.86, 0.83), look='paint', rough=0.5, r=0.85),
        'yellow': material('gelb_lack', (0.86, 0.66, 0.10), look='paint', rough=0.55, r=0.9),
        'orange': material('orange_lack', (0.86, 0.38, 0.07), look='paint', rough=0.5, r=0.85),
        'black_rubber': material('gummi', (0.045, 0.045, 0.045), look='rubber', rough=0.9, r=1.2),
        'black_plastic': material('kunststoff_schwarz', (0.06, 0.06, 0.065), look='plastic', rough=0.6, r=1.0),
        'paper': material('papiersack', (0.50, 0.42, 0.30), look='cardboard', rough=0.95, r=1.4),
        'paper_dirty': material('papiersack_alt', (0.42, 0.38, 0.32), look='cardboard', rough=0.95, r=1.45),
        'print_red': material('aufdruck_rot', (0.46, 0.13, 0.10), look='paint', rough=0.8, r=1.3),
        'print_grey': material('aufdruck_grau', (0.42, 0.42, 0.42), look='paint', rough=0.8, r=1.3),
        'cement': material('zementstaub', (0.70, 0.70, 0.68), look='concrete', tex='concrete_chalky', rough=1.0, r=1.5),
        'pallet': material('palette', (0.60, 0.53, 0.42), look='wood', rough=0.9, r=1.3),
        'tarp_blue': material('plane_blau', (0.12, 0.23, 0.40), look='plastic', rough=0.7, r=1.05),
        'tarp_green': material('plane_gruen', (0.20, 0.30, 0.20), look='plastic', rough=0.75, r=1.05),
        'tarp_white': material('plane_weiss', (0.58, 0.60, 0.57), look='plastic', rough=0.7, r=1.15),
        'tarp_hem': material('plane_saum', (0.85, 0.86, 0.84), look='plastic', rough=0.75, r=1.1),
        'zinc': material('zink', (0.70, 0.70, 0.68), look='metal', rough=0.45, metal=0.9, r=0.8, m=0.75),
        'brick': material('ziegel', (0.86, 0.56, 0.44), look='concrete', tex='bricks_red_factory', rough=0.9, r=1.3),
        'clay': material('hochlochziegel', (0.60, 0.34, 0.24), look='concrete', tex='concrete_rough', rough=0.9, r=1.4),
        'clay2': material('hochlochziegel_hell', (0.68, 0.42, 0.30), look='concrete', tex='concrete_rough', rough=0.9, r=1.4),
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
    # Gebaut mit Feldlaenge entlang y und Gelaender auf +x; level1.py setzt das Feld mit yaw = Wandnormale + 90 Grad
    # (Feldlaenge entlang x, Wand auf +y). Daher am Ende -90 Grad um z: Laenge -> x, Gelaenderseite -> -y (Raum).
    k.finish(rot(0, 0, -math.pi / 2))


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
            fold = 0.045 * math.sin(u * 23.0 + 0.7 * math.sin(v * 3.0)) * (0.4 + 0.6 * math.sin(math.pi * v))
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
    timber = material('kantholz_alt', (0.42, 0.39, 0.34), look='wood', rough=0.9, r=1.4)
    # Kanthoelzer quer (entlang y)
    for x in (-1.0, 0.0, 1.0):
        blk(k(timber), (0.10, 1.05, 0.10), (x + jitter(rng, 0.04), jitter(rng, 0.04), 0.05), rz=jitter(rng, 0.08), bevel=0.006)
    # Buendel: hexagonale Packung, Staebe 3,6 m (entlang x), Enden versetzt
    Lb = 2.8
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
    for x in (-0.85, 0.1, 0.95):
        pts = [(x + 0.004 * math.sin(a * 3), 0.112 * math.cos(a), zc + 0.096 * math.sin(a)) for a in [TAU * i / 18 for i in range(19)]]
        sweep(k(wire, 60), pts, 0.0018, 4, caps=False)
        tail = [(x, 0.0, zc + 0.096), (x + 0.01, 0.004, zc + 0.115), (x + 0.004, -0.004, zc + 0.13), (x + 0.012, 0.0, zc + 0.145)]
        sweep(k(wire, 60), catmull(tail, 3), 0.002, 4)
    # Lieferschild (gelber Kunststoff) am Draht
    blk(k(material('schild_gelb', (0.86, 0.72, 0.12), look='plastic', rough=0.5, r=0.9)), (0.002, 0.07, 0.10),
        (0.95, 0.0, zc + 0.20), rx=0.15)
    cyl(k(wire, 60), (0.95, 0.0, zc + 0.145), (0.95, 0.0, zc + 0.25), 0.0012, 4)
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
    frame_c = material('rahmen_rot', (0.54, 0.12, 0.08), look='paint', rough=0.55, r=1.0)
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
    drum_c = material('trommel_orange', (0.72, 0.30, 0.06), look='paint', rough=0.5, r=0.95)
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
    tub_c = m['galv_old']
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
    wood = material('bock_holz', (0.58, 0.50, 0.38), look='wood', rough=0.85, r=1.25)
    wood_d = material('bock_holz_alt', (0.44, 0.40, 0.33), look='wood', rough=0.9, r=1.35)
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
    cube(k(material('brett_frisch', (0.70, 0.62, 0.48), look='wood', rough=0.8, r=1.2)),
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
    dust = material('saegemehl', (0.62, 0.53, 0.40), look='fabric', rough=1.0, r=1.5)
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


# =====================================================================================================================
# GARDEN - ueberwuchert (gedaempftes Gruen im Kunstlicht, Moos feucht)
# =====================================================================================================================
def mats_plant():
    return {
        'leaf_dark': material('blatt_dunkel', (0.13, 0.22, 0.09), look='paint', rough=0.5, r=0.95),
        'leaf_mid': material('blatt_mittel', (0.20, 0.31, 0.11), look='paint', rough=0.55, r=0.9),
        'leaf_light': material('blatt_hell', (0.30, 0.41, 0.15), look='paint', rough=0.6, r=1.0),
        'leaf_yellow': material('blatt_gelb', (0.38, 0.38, 0.16), look='paint', rough=0.7, r=1.15),
        'leaf_brown': material('blatt_braun', (0.34, 0.24, 0.13), look='paint', rough=0.85, r=1.35),
        'fern': material('farn', (0.24, 0.36, 0.13), look='paint', rough=0.7, r=1.1),
        'fern_dark': material('farn_dunkel', (0.17, 0.27, 0.10), look='paint', rough=0.7, r=1.1),
        'stem': material('stengel', (0.24, 0.30, 0.13), look='paint', rough=0.7, r=1.1),
        'stem_brown': material('stengel_braun', (0.30, 0.22, 0.14), look='wood', rough=0.8, r=1.25),
        'bark': material('rinde', (0.36, 0.29, 0.22), look='wood', tex='concrete_rough', rough=0.95, r=1.4),
        'bark_dark': material('rinde_dunkel', (0.24, 0.19, 0.15), look='wood', tex='concrete_rough', rough=0.95, r=1.45),
        'birch': material('birkenrinde', (0.86, 0.84, 0.79), look='paint', tex='plaster_white', rough=0.7, r=1.15),
        'birch_mark': material('birke_lentizellen', (0.10, 0.09, 0.09), look='paint', rough=0.8, r=1.3),
        'twig': material('zweig', (0.32, 0.20, 0.15), look='wood', rough=0.75, r=1.2),
        'root': material('wurzel', (0.40, 0.31, 0.22), look='wood', tex='concrete_rough', rough=0.9, r=1.35),
        'soil': material('erde_feucht', (0.40, 0.31, 0.24), look='concrete', tex='concrete_rough', rough=0.9, r=1.15),
        'moss': material('moos', (0.70, 0.62, 0.48), look='fabric', tex='moss', rough=0.9, r=1.05),
        'moss_dry': material('moos_trocken', (0.74, 0.56, 0.38), look='fabric', tex='moss', rough=0.95, r=1.3),
        'wet': material('beton_nass', (0.44, 0.44, 0.42), look='concrete', tex='concrete_floor', rough=0.3, r=0.5),
        'slab': material('estrich', (0.68, 0.68, 0.66), look='concrete', tex='concrete_floor', rough=0.9, r=1.2),
        'slab_edge': material('estrich_bruch', (0.62, 0.61, 0.58), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'grass': material('gras', (0.30, 0.40, 0.15), look='paint', rough=0.7, r=1.1),
        'grass_dry': material('gras_trocken', (0.50, 0.46, 0.28), look='paint', rough=0.85, r=1.3),
    }


def _pick_leaf(P, rng, weights=(('leaf_dark', 0.3), ('leaf_mid', 0.45), ('leaf_light', 0.17), ('leaf_yellow', 0.08))):
    x = rng.random()
    for name, w in weights:
        x -= w
        if x <= 0:
            return P[name]
    return P[weights[-1][0]]


def _jag(rng, cx, cy, rx, ry, n=7, rough=0.3, a0=0.0):
    """Unregelmaessiger Umriss (gegen den Uhrzeigersinn)."""
    return [(cx + rx * math.cos(a0 + TAU * i / n) * (1 + jitter(rng, rough)), cy + ry * math.sin(a0 + TAU * i / n) * (1 + jitter(rng, rough)))
            for i in range(n)]


def _slab(k, P, rng, outline, thick, M):
    """Bruchstueck der Bodenplatte: Oberseite Estrich, Bruchkanten rau."""
    poly_prism(k(P['slab_edge']), outline, -thick, -0.004, M)
    top = [M @ V((x, y, 0.0)) for (x, y) in outline]
    quad_n(k(P['slab']), top, M.to_3x3() @ V((0, 0, 1)))
    # Kantenbruch oben: schmale Fase in Bruchmaterial
    for i in range(len(outline)):
        a, b = outline[i], outline[(i + 1) % len(outline)]
        c = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2)
        pts = [M @ V((a[0], a[1], 0.0)), M @ V((b[0], b[1], 0.0)), M @ V((b[0], b[1], -0.004)), M @ V((a[0], a[1], -0.004))]
        out = M.to_3x3() @ V((c[0], c[1], 0.0))
        quad_n(k(P['slab_edge']), pts, out)


def _crack_base(k, P, rng, rx=0.45, ry=0.10, chunks=4, lift=0.03, soil_r=None):
    """Riss im Betonboden: dunkle Erde im Spalt, angehobene Bruchstuecke an den Raendern, Kruemel."""
    soil_out = _jag(rng, 0.0, 0.0, rx, ry, n=11, rough=0.35)
    quad_n(k(P['soil']), [V((x, y, 0.004)) for (x, y) in soil_out], (0, 0, 1))
    for _ in range(4):
        blob(k(P['soil'], 30), (rng.uniform(-rx * 0.6, rx * 0.6), rng.uniform(-ry * 0.5, ry * 0.5), 0.004), (0, 0, 1),
             rng.uniform(0.03, 0.07), rng.uniform(0.01, 0.025), rng)
    for i in range(chunks):
        side = 1 if i % 2 == 0 else -1
        cx = -rx * 0.7 + (rx * 1.4) * (i + 0.5) / chunks + jitter(rng, 0.04)
        cy = side * (ry + rng.uniform(0.06, 0.12))
        out = _jag(rng, 0.0, 0.0, rng.uniform(0.08, 0.14), rng.uniform(0.06, 0.10), n=6, rough=0.25)
        M = tr(cx, cy, lift * rng.uniform(0.4, 1.0)) @ rot(side * rng.uniform(0.08, 0.2), jitter(rng, 0.08), rng.uniform(0, TAU))
        _slab(k, P, rng, out, 0.07, M)
    for _ in range(8):
        a = rng.uniform(0, TAU)
        blk(k(P['slab_edge']), (rng.uniform(0.015, 0.04), rng.uniform(0.012, 0.03), rng.uniform(0.008, 0.02)),
            (rx * 1.1 * math.cos(a), (ry + 0.15) * math.sin(a), 0.006), rx=jitter(rng, 0.4), ry=jitter(rng, 0.4), rz=a)


# --- haengende Ranken (Deckenmodell) -------------------------------------------------------------------------------------
def _strand(k, P, rng, top, length, r0, leaf_every, leaf_len, sway=0.12, dry=False, curl=True):
    """Ranke, die von der Decke haengt: Stengel duenner werdend, Blaetter wechselstaendig, Spitze leicht eingerollt."""
    n = max(4, int(length / 0.09))
    dx, dy = rng.uniform(-1, 1), rng.uniform(-1, 1)
    ph = rng.uniform(0, 6)
    pts = []
    for i in range(n + 1):
        t = i / n
        z = top[2] - length * t
        off = sway * t * t
        x = top[0] + dx * off + 0.025 * math.sin(t * 7 + ph)
        y = top[1] + dy * off + 0.025 * math.cos(t * 6 + ph)
        pts.append(V((x, y, z)))
    if curl:
        e = pts[-1]
        d = V((dx, dy, 0)).normalized() if abs(dx) + abs(dy) > 0.01 else V((1, 0, 0))
        pts += [e + d * 0.025 + V((0, 0, -0.01)), e + d * 0.04 + V((0, 0, 0.012)), e + d * 0.025 + V((0, 0, 0.03))]
    rs = [r0 * (1.0 - 0.6 * i / (len(pts) - 1)) for i in range(len(pts))]
    sweep(k(P['stem_brown'] if dry else P['stem'], 70), pts, rs, 5)
    # Blaetter
    s = 0.06
    side = 1
    total = sum((pts[i + 1] - pts[i]).length for i in range(len(pts) - 1))
    while s < total - 0.04:
        # Punkt auf der Polylinie bei Bogenlaenge s
        acc = 0.0
        for i in range(len(pts) - 1):
            seg = (pts[i + 1] - pts[i]).length
            if acc + seg >= s:
                p = pts[i].lerp(pts[i + 1], (s - acc) / max(seg, 1e-6))
                tang = (pts[i + 1] - pts[i]).normalized()
                break
            acc += seg
        side = -side
        az = rng.uniform(0, TAU) if not dry else 0.0
        outw = V((math.cos(az), math.sin(az), 0.0))
        outw = (outw - tang * outw.dot(tang)).normalized()
        frac = s / total
        L = leaf_len * (1.15 - 0.5 * frac) * rng.uniform(0.8, 1.15)
        if dry and rng.random() < 0.5:
            s += leaf_every
            continue
        mat = P['leaf_brown'] if (dry and rng.random() < 0.6) else _pick_leaf(P, rng)
        # kurzer Stiel, dann Blatt nach aussen/unten haengend, Oberseite nach aussen
        stalk_end = p + outw * 0.018 + V((0, 0, -0.006))
        cyl(k(P['stem'], 70), p, stalk_end, 0.0015, 3, caps=False)
        fwd = (outw * 0.55 + V((0, 0, -0.85))).normalized()
        leaf(k(mat, 60), stalk_end, fwd, outw, L, L * 0.62, stations=((0.22, 0.95), (0.55, 0.85)), droop=0.35,
             fold=0.12, curl=0.06)
        s += leaf_every * rng.uniform(0.75, 1.25)


def _ceiling_mat(k, P, rng, rx, ry, n_stems=6):
    """Geflecht an der Decke: flach anliegende Triebe mit ein paar Blaettern (nach unten)."""
    for i in range(n_stems):
        a = rng.uniform(0, TAU)
        p = V((rx * math.cos(a) * rng.uniform(0.5, 1.0), ry * math.sin(a) * rng.uniform(0.5, 1.0), -0.008))
        d = (-p).normalized() if p.length > 0.01 else V((1, 0, 0))
        ctrl = [p.copy()]
        for _ in range(5):  # kriechender Trieb: Zufallsweg mit Richtungsgedaechtnis
            d = (Matrix.Rotation(rng.uniform(-0.9, 0.9), 3, 'Z') @ d).normalized()
            p = p + d * rng.uniform(0.12, 0.22)
            p.x = max(-rx, min(rx, p.x))
            p.y = max(-ry, min(ry, p.y))
            p.z = -0.006 - rng.uniform(0.0, 0.01)
            ctrl.append(p.copy())
        pts = catmull(ctrl, 3)
        sweep(k(P['stem_brown'], 70), pts, [0.008 - 0.004 * j / (len(pts) - 1) for j in range(len(pts))], 5)
        for j in range(1, len(pts) - 1, 3):
            p = pts[j]
            d = V((rng.uniform(-1, 1), rng.uniform(-1, 1), 0.0)).normalized()
            leaf(k(_pick_leaf(P, rng), 60), p + V((0, 0, -0.004)), d, (0, 0, -1), rng.uniform(0.05, 0.08), 0.045,
                 stations=((0.3, 1.0),), fold=0.1)


def _vines(variant):
    rng = random.Random(201 + variant)
    P = mats_plant()
    k = Kit()
    if variant == 0:      # dichter Vorhang
        _ceiling_mat(k, P, rng, 0.7, 0.45, 6)
        for i in range(10):
            top = V((rng.uniform(-0.6, 0.6), rng.uniform(-0.38, 0.38), -0.01))
            _strand(k, P, rng, top, rng.uniform(0.45, 1.4), 0.005, 0.105, 0.11)
    elif variant == 1:    # wenige lange, duenne Ranken, teils vertrocknet
        _ceiling_mat(k, P, rng, 0.5, 0.3, 4)
        for i in range(7):
            top = V((rng.uniform(-0.45, 0.45), rng.uniform(-0.25, 0.25), -0.01))
            _strand(k, P, rng, top, rng.uniform(0.8, 1.6), 0.0035, 0.12, 0.06, sway=0.2, dry=(i % 3 == 0))
    else:                 # verholzte Liane als Bogen, daraus haengende Triebe
        _ceiling_mat(k, P, rng, 0.9, 0.35, 5)
        a, b = V((-0.85, -0.1, -0.01)), V((0.85, 0.12, -0.01))
        loop = []
        for i in range(13):
            t = i / 12
            p = a.lerp(b, t)
            p.z -= 0.55 * math.sin(math.pi * t) ** 1.3
            p.y += 0.08 * math.sin(t * 9)
            loop.append(p)
        rs = [0.022 - 0.008 * abs(0.5 - i / 12) for i in range(13)]
        sweep(k(P['bark'], 60), loop, rs, 7)
        # zweiter, duennerer Strang um den ersten gewunden
        twist = [loop[i] + V((0, 0.03 * math.cos(i * 1.4), 0.03 * math.sin(i * 1.4))) for i in range(13)]
        sweep(k(P['bark_dark'], 60), catmull(twist, 2), 0.008, 5)
        for i in range(2, 11, 2):
            _strand(k, P, rng, loop[i] + V((0, 0, -0.02)), rng.uniform(0.3, 0.75), 0.004, 0.08, 0.07, sway=0.08)
        for i in range(1, 12):
            p = loop[i]
            for _ in range(2):
                d = V((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.6, 0.3))).normalized()
                leaf(k(_pick_leaf(P, rng), 60), p, d, (0, 0, 1), rng.uniform(0.07, 0.10), 0.06, droop=0.4)
    k.finish()


def build_vines_ceiling():
    _vines(0)


def build_vines_ceiling__v1():
    _vines(1)


def build_vines_ceiling__v2():
    _vines(2)


# --- Efeu an der Wand (Wandmodell: Wand bei x = 0, Ursprung am Boden) -----------------------------------------------
IVY_OUTLINE = [(0.0, 1.0), (0.26, 0.48), (0.62, 0.36), (0.32, -0.08), (0.0, 0.04), (-0.32, -0.08), (-0.62, 0.36),
               (-0.26, 0.48)]


def _ivy_leaf(k, P, rng, base, outward, size, mat=None):
    """Efeublatt (fuenflappig) an kurzem Stiel; Blattflaeche schraeg nach aussen/oben gestellt."""
    up = V((jitter(rng, 0.4), jitter(rng, 0.4), 1.0)).normalized()
    nrm = (V(outward) * 0.85 + V((0, 0, 0.25)) + V((0, jitter(rng, 0.3), jitter(rng, 0.2)))).normalized()
    t = (up - nrm * up.dot(nrm)).normalized()     # Blattspitze
    s = nrm.cross(t)
    c = V(base) + V(outward) * rng.uniform(0.025, 0.06)
    cyl(k(P['stem_brown'], 70), V(base), c, 0.0018, 3, caps=False)
    pts = [c + (s * u + t * v) * size for (u, v) in IVY_OUTLINE]
    lift = max(0.0, 0.004 - min(q.x for q in pts)) if outward[0] > 0.5 else 0.0
    if lift > 0:  # Blatt nicht in die Wand stecken lassen
        c = c + V((lift, 0, 0))
        pts = [q + V((lift, 0, 0)) for q in pts]
    leaf_fan(k(mat or _pick_leaf(P, rng, (('leaf_dark', 0.55), ('leaf_mid', 0.32), ('leaf_light', 0.1), ('leaf_yellow', 0.03))), 55, weld=True),
             c + t * size * 0.35, pts, nrm, cup=size * 0.08)


def _ivy_stem(k, P, rng, start, height, width, r0, depth, step=0.11, max_z=2.4, size=(0.075, 0.115), kid_p=0.45):
    """Kriechender Efeutrieb an der Wand (x klein), mit Verzweigungen; Blaetter wechselstaendig, gross und dicht
    (geschindelt) - so wirkt der Bewuchs als Teppich und nicht als Gestruepp."""
    n = max(4, int(height / step))
    pts = []
    ph = rng.uniform(0, 6)
    for i in range(n + 1):
        t = i / n
        y = start[1] + width * math.sin(t * 2.2 + ph) * 0.5 + jitter(rng, 0.025)
        z = min(max_z, start[2] + height * t)
        pts.append(V((r0 + 0.003 + abs(math.sin(t * 9 + ph)) * 0.005, y, z)))
    rs = [r0 * (1.0 - 0.7 * i / n) for i in range(n + 1)]
    sweep(k(P['bark_dark'] if r0 > 0.006 else P['stem_brown'], 70), pts, rs, 5)
    # Haftwurzeln: kurze Buendel zur Wand
    for i in range(1, n, 3):
        p = pts[i]
        cyl(k(P['stem_brown'], 0), p + V((-rs[i] * 0.5, 0, 0)), V((0.001, p.y + jitter(rng, 0.01), p.z - 0.012)), 0.0012, 3, caps=False)
    for i in range(1, n + 1):
        p = pts[i]
        side = 1 if i % 2 else -1
        q = p + V((0, side * rng.uniform(0.03, 0.08), jitter(rng, 0.03)))
        _ivy_leaf(k, P, rng, q, (1, side * rng.uniform(0.1, 0.45), 0), rng.uniform(*size))
    if depth > 0:
        for i in range(2, n - 1, 3):
            if rng.random() < kid_p:
                p = pts[i]
                _ivy_stem(k, P, rng, (0, p.y, p.z), height * rng.uniform(0.25, 0.45), width * rng.uniform(0.5, 1.0) * rng.choice((-1, 1)),
                          r0 * 0.55, depth - 1, step, max_z, size, kid_p)


def _ivy(variant):
    rng = random.Random(221 + variant)
    P = mats_plant()
    k = Kit()
    if variant == 0:
        for (y, h, w) in ((-0.45, 2.3, 0.35), (-0.12, 2.0, 0.4), (0.2, 2.35, 0.4), (0.48, 1.7, 0.3)):
            _ivy_stem(k, P, rng, (0, y, 0.0), h, w * 1.5, 0.011, 1, step=0.15, size=(0.10, 0.145), kid_p=0.4)
    else:
        for (y, h, w) in ((-0.25, 1.6, 0.35), (0.0, 1.2, 0.3), (0.25, 1.4, 0.4)):
            _ivy_stem(k, P, rng, (0, y, 0.0), h, w * 1.5, 0.009, 1, max_z=1.7, step=0.16, size=(0.09, 0.13), kid_p=0.3)
        # Auslaeufer auf dem Boden
        for s in (-1, 1):
            pts = catmull([(0.03, s * 0.2, 0.006), (0.18, s * 0.45, 0.008), (0.30, s * 0.70, 0.006), (0.36, s * 0.95, 0.006)], 3)
            sweep(k(P['stem_brown'], 70), pts, 0.005, 5)
            for p in pts[1::2]:
                _ivy_leaf(k, P, rng, p, (0, 0, 1), rng.uniform(0.06, 0.085))
    # Erde und Laub am Fuss der Wand
    for _ in range(3):
        rr = rng.uniform(0.06, 0.12)
        blob(k(P['soil'], 30), (rr * 1.35 + rng.uniform(0.0, 0.06), rng.uniform(-0.45, 0.45), 0.0), (0, 0, 1), rr,
             0.012, rng, nseg=10)
    k.finish()


def build_ivy_wall():
    _ivy(0)


def build_ivy_wall__v1():
    _ivy(1)


# --- Moospolster (Boden, flach, weiche Raender) ------------------------------------------------------------------------
def _cushion(k, P, rng, cx, cy, rx, ry, h, rings=6, segs=26, a0=0.0, z0=0.0, wet_ring=True, dry=True):
    """Moospolster: Polarnetz mit welligem Rand, Hoehe zum Rand weich auslaufend, kleine Buckel; feuchter Hof.
    z0: Hoehe der Unterlage (Boden 0, Kistendeckel ...)."""
    ph = [rng.uniform(0, TAU) for _ in range(4)]

    def rad(a):
        return 1.0 + 0.18 * math.sin(3 * a + ph[0]) + 0.10 * math.sin(5 * a + ph[1]) + 0.06 * math.sin(9 * a + ph[2])
    bumps = [(rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(0.15, 0.35), rng.uniform(0.3, 1.0)) for _ in range(6)]
    src = bmesh.new()
    center = src.verts.new((cx, cy, z0 + h + 0.002))
    grid = []
    for ri in range(1, rings + 1):
        t = ri / rings
        row = []
        for si in range(segs):
            a = a0 + TAU * si / segs
            rr = rad(a) * t
            u, v = rr * math.cos(a), rr * math.sin(a)
            hh = h * (1.0 - t ** 2.2) ** 0.8
            for (bu, bv, bs, bh) in bumps:
                d2 = ((u - bu) ** 2 + (v - bv) ** 2) / (bs * bs)
                hh += h * 0.35 * bh * math.exp(-d2) * (1.0 - t)
            row.append(src.verts.new((cx + u * rx, cy + v * ry, z0 + max(0.0015, hh + 0.0015))))
        grid.append(row)
    for si in range(segs):
        sj = (si + 1) % segs
        src.faces.new((center, grid[0][si], grid[0][sj]))
    for ri in range(rings - 1):
        for si in range(segs):
            sj = (si + 1) % segs
            src.faces.new((grid[ri][si], grid[ri + 1][si], grid[ri + 1][sj], grid[ri][sj]))
    for f in src.faces:
        f.normal_update()
        if f.normal.z < 0:
            f.normal_flip()
    dry_a = rng.uniform(0, TAU)

    def pick(f):
        c = f.calc_center_median()
        a = math.atan2((c.y - cy) / ry, (c.x - cx) / rx)
        if dry and math.cos(a - dry_a) > 0.88 and math.hypot((c.x - cx) / rx, (c.y - cy) / ry) > 0.62:
            return k(P['moss_dry'], 60, weld=True)
        return k(P['moss'], 60, weld=True)
    emit(src, pick)
    if not wet_ring:
        return
    # feuchter Hof um das Polster (dunkler, glaenzender Beton)
    outer = []
    inner = []
    for si in range(segs):
        a = a0 + TAU * si / segs
        r1 = rad(a)
        inner.append(V((cx + r1 * 0.97 * math.cos(a) * rx, cy + r1 * 0.97 * math.sin(a) * ry, 0.0012)))
        r2 = r1 * (1.05 + 0.035 * math.sin(4 * a + ph[3]))
        outer.append(V((cx + r2 * math.cos(a) * rx, cy + r2 * math.sin(a) * ry, 0.0008)))
    wet = k(P['wet'], 0)
    for si in range(segs):
        sj = (si + 1) % segs
        quad_n(wet, [inner[si], outer[si], outer[sj], inner[sj]], (0, 0, 1))


def _sporophytes(k, P, rng, cx, cy, rx, ry, h, n):
    cap = material('sporenkapsel', (0.42, 0.30, 0.16), look='paint', rough=0.7, r=1.1)
    for _ in range(n):
        a, r = rng.uniform(0, TAU), rng.uniform(0, 0.6)
        x, y = cx + r * math.cos(a) * rx, cy + r * math.sin(a) * ry
        z0 = h * (1.0 - r ** 2) * 0.9
        top = V((x + jitter(rng, 0.004), y + jitter(rng, 0.004), z0 + rng.uniform(0.015, 0.03)))
        cyl(k(P['stem_brown'], 0), (x, y, z0), top, 0.0007, 3, caps=False)
        cyl(k(cap, 60), top, top + V((0.002, 0, 0.004)), 0.0018, 5)


def _moss(variant):
    rng = random.Random(241 + variant)
    P = mats_plant()
    k = Kit()
    if variant == 0:
        _cushion(k, P, rng, 0.0, 0.0, 0.62, 0.48, 0.035)
        _sporophytes(k, P, rng, 0.0, 0.0, 0.62, 0.48, 0.035, 14)
        for _ in range(3):  # Betonkruemel im Polster
            blk(k(P['slab_edge']), (0.03, 0.02, 0.015), (rng.uniform(-0.4, 0.4), rng.uniform(-0.3, 0.3), 0.03), rz=rng.uniform(0, 3),
                rx=0.3, bevel=0.003)
    elif variant == 1:
        # entlang eines Risses: dunkler Spalt, mehrere Polster in einer Reihe
        crack = [(-1.0, -0.02), (-0.6, 0.03), (-0.2, -0.04), (0.2, 0.02), (0.6, -0.03), (1.0, 0.02)]
        pts = catmull([V((x, y, 0.001)) for (x, y) in crack], 3)
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            d = (b - a).normalized()
            nrm = V((-d.y, d.x, 0)) * 0.012
            quad_n(k(P['soil'], 0), [a - nrm, b - nrm, b + nrm, a + nrm], (0, 0, 1))
        for i, x in enumerate((-0.75, -0.3, 0.15, 0.6)):
            _cushion(k, P, rng, x, jitter(rng, 0.03), rng.uniform(0.22, 0.32), rng.uniform(0.12, 0.18), rng.uniform(0.02, 0.03),
                     rings=5, segs=20, a0=i)
    else:
        for (x, y, r1, r2, h) in ((-0.25, 0.1, 0.28, 0.22, 0.03), (0.22, -0.12, 0.2, 0.16, 0.025), (0.05, 0.32, 0.12, 0.1, 0.02)):
            _cushion(k, P, rng, x, y, r1, r2, h, rings=5, segs=20, a0=x)
        _sporophytes(k, P, rng, -0.25, 0.1, 0.28, 0.22, 0.03, 8)
    k.finish()


def build_moss_patch():
    _moss(0)


def build_moss_patch__v1():
    _moss(1)


def build_moss_patch__v2():
    _moss(2)


# --- Farne aus Betonrissen ------------------------------------------------------------------------------------------------
def _frond(k, P, rng, base, out_dir, length, rise, pairs=9, mat=None, dead=False):
    """Farnwedel: gebogene Spindel, Fiederblaetter paarweise, zur Spitze kleiner."""
    out_dir = V(out_dir).normalized()
    pts = []
    n = 6
    for i in range(n + 1):
        t = i / n
        horiz = length * (0.15 + 0.85 * t) * t
        h = rise * math.sin(min(1.0, t * 1.6) * math.pi * 0.5) - length * 0.35 * max(0.0, t - 0.5) ** 2 * 2
        if dead:
            h = 0.01 + 0.02 * math.sin(t * 3)
            horiz = length * t
        pts.append(V(base) + out_dir * horiz + V((0, 0, h)))
    mat = mat or (P['leaf_brown'] if dead else (P['fern'] if rng.random() < 0.6 else P['fern_dark']))
    sweep(k(P['stem_brown'] if dead else P['stem'], 70), pts, [0.004 - 0.0028 * i / n for i in range(n + 1)], 4)
    side = V((0, 0, 1)).cross(out_dir).normalized()
    for j in range(pairs):
        t = 0.18 + 0.8 * j / pairs
        f = t * n
        i = min(n - 1, int(f))
        p = pts[i].lerp(pts[i + 1], f - i)
        tang = (pts[i + 1] - pts[i]).normalized()
        nrm = tang.cross(side).normalized()
        if nrm.z < 0:
            nrm = -nrm
        size = length * 0.16 * math.sin(math.pi * (0.18 + 0.72 * t)) * rng.uniform(0.9, 1.1)
        for s in (-1, 1):
            d = (side * s * 0.85 + tang * 0.45).normalized()
            leaf(k(mat, 0), p, d, nrm, size, size * 0.34, stations=((0.35, 1.0),), droop=0.4, fold=0.2)


def _fiddlehead(k, P, base, h, d):
    pts = []
    for i in range(14):
        t = i / 13
        if t < 0.6:
            pts.append(V(base) + V((d[0] * t * 0.06, d[1] * t * 0.06, h * t / 0.6)))
        else:
            a = (t - 0.6) / 0.4 * TAU * 0.9
            r = 0.025 * (1.0 - (t - 0.6) / 0.4 * 0.7)
            c = V(base) + V((d[0] * 0.06 + d[0] * 0.025, d[1] * 0.06 + d[1] * 0.025, h))
            pts.append(c + V((-d[0] * r * math.cos(a), -d[1] * r * math.cos(a), r * math.sin(a))))
    sweep(k(P['stem'], 70), pts, [0.005 - 0.003 * i / 13 for i in range(14)], 5)


def build_fern_cluster():
    rng = random.Random(261)
    P = mats_plant()
    k = Kit()
    _crack_base(k, P, rng, rx=0.42, ry=0.08, chunks=4)
    for i in range(8):
        a = TAU * i / 8 + jitter(rng, 0.3)
        b = V((0.08 * math.cos(a) + jitter(rng, 0.05), 0.04 * math.sin(a), 0.004))
        _frond(k, P, rng, b, (math.cos(a), math.sin(a), 0), rng.uniform(0.5, 0.85), rng.uniform(0.35, 0.6))
    for a in (0.4,):
        _frond(k, P, rng, (0.05 * math.cos(a), 0.05 * math.sin(a), 0.004), (math.cos(a), math.sin(a), 0), 0.55, 0.0, dead=True)
    _fiddlehead(k, P, (0.02, -0.02, 0.004), 0.12, (0.7, 0.7))
    _fiddlehead(k, P, (-0.04, 0.02, 0.004), 0.09, (-0.8, 0.5))
    k.finish()


# --- Wurzeln brechen den Boden auf --------------------------------------------------------------------------------------
def build_roots_floor():
    rng = random.Random(281)
    P = mats_plant()
    k = Kit()
    # Hauptwurzeln: tauchen auf und ab (z < 0 = im Boden), verjuengen sich
    mains = [
        [(-1.25, -0.10, -0.06), (-0.85, -0.05, 0.05), (-0.40, 0.05, 0.08), (0.05, 0.10, 0.03), (0.45, 0.25, -0.03), (0.80, 0.45, 0.03), (1.15, 0.65, -0.05)],
        [(-1.25, 0.05, -0.05), (-0.90, 0.25, 0.06), (-0.55, 0.45, 0.07), (-0.25, 0.70, 0.0), (0.0, 0.85, -0.05)],
        [(-1.25, -0.20, -0.05), (-0.85, -0.40, 0.04), (-0.35, -0.55, 0.06), (0.15, -0.60, 0.0), (0.55, -0.75, -0.05)],
    ]
    humps = []
    for mi, ctrl in enumerate(mains):
        pts = catmull([V(p) for p in ctrl], 5)
        r0 = 0.10 - mi * 0.02
        rs = [r0 * (1.0 - 0.75 * i / (len(pts) - 1)) for i in range(len(pts))]
        prof = [(math.cos(TAU * i / 9) * (1.0 + 0.12 * math.sin(i * 2.3)), math.sin(TAU * i / 9) * 0.8) for i in range(9)]
        sweep(k(P['root'], 60), pts, rs, profile=prof, up=(0, 0, 1))
        for i, p in enumerate(pts):
            if p.z > 0.04 and i % 4 == 0:
                tg = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
                humps.append((p, rs[i], tg))
        # Seitenwurzeln
        for i in range(4, len(pts) - 4, 5):
            p = pts[i]
            d = V((rng.uniform(-0.3, 0.6), rng.choice((-1, 1)) * rng.uniform(0.5, 1.0), 0.0)).normalized()
            sp = [p, p + d * 0.15 + V((0, 0, 0.01)), p + d * 0.32 + V((jitter(rng, 0.06), jitter(rng, 0.06), -0.01)),
                  p + d * 0.5 + V((jitter(rng, 0.08), jitter(rng, 0.08), -0.04))]
            sweep(k(P['root'], 60), catmull(sp, 3), [rs[i] * 0.45 * (1 - 0.7 * j / 9) for j in range(10)], 6)
    # erdige Stellen um die Buckel
    for (p, r, tg) in humps:
        out = _jag(rng, p.x, p.y, r * 2.6, r * 2.2, n=9, rough=0.3)
        quad_n(k(P['soil'], 0), [V((x, y, 0.003)) for (x, y) in out], (0, 0, 1))
    # angehobene Plattenstuecke: liegen mit der Kante auf der Wurzel, die Gegenkante bleibt am Boden (gekippt um eine
    # Achse parallel zur Wurzel) - wie eine Platte, die von unten aufgedrueckt wurde
    for i, (p, r, tg) in enumerate(humps[:8]):
        side = V((-tg.y, tg.x, 0.0)).normalized() * (1 if i % 2 == 0 else -1)
        rad = rng.uniform(0.18, 0.30)
        out = _jag(rng, 0.0, 0.0, rad, rad * rng.uniform(0.7, 0.95), n=6, rough=0.2)
        top_z = p.z + r * 0.9
        ang = math.atan2(top_z, rad * 1.7)
        c = p + side * (rad * 0.85 + r * 0.3)
        axis = V((tg.x, tg.y, 0.0)).normalized()
        R = Matrix.Rotation(ang, 4, axis)
        if (R.to_3x3() @ (-side)).z < 0:  # die zur Wurzel zeigende Kante soll oben liegen
            R = Matrix.Rotation(-ang, 4, axis)
        M = tr(c.x, c.y, top_z * 0.5) @ R @ rot(0, 0, rng.uniform(0, TAU))
        _slab(k, P, rng, out, 0.08, M)
        if rng.random() < 0.6:  # Moos in der Fuge
            blob(k(P['moss'], 60), tuple(p + side * 0.05 + V((0, 0, 0.004 - p.z))), (0, 0, 1), rng.uniform(0.04, 0.08), 0.012, rng)
    # Bruchstuecke und Kruemel
    for _ in range(14):
        x, y = rng.uniform(-1.1, 1.0), rng.uniform(-0.8, 0.8)
        s = rng.uniform(0.02, 0.07)
        blk(k(P['slab_edge']), (s, s * rng.uniform(0.5, 0.9), s * rng.uniform(0.25, 0.45)), (x, y, s * 0.12), rx=jitter(rng, 0.25),
            ry=jitter(rng, 0.25), rz=rng.uniform(0, 3))
    k.finish()


# --- Strauch und junger Baum ------------------------------------------------------------------------------------------------
def _branch(k, P, rng, start, d, length, r0, depth, leaf_fn, bark, twig_mat, gravity=-0.05, kids=2, segs=5):
    """Ast: Abschnitte mit leichter Kruemmung; Kinder seitlich; am Ende Blaetter (leaf_fn(p, tangente))."""
    pts = [V(start)]
    d = V(d).normalized()
    seg = length / segs
    for i in range(segs):
        d = (d + V((jitter(rng, 0.18), jitter(rng, 0.18), jitter(rng, 0.12) + gravity))).normalized()
        pts.append(pts[-1] + d * seg)
    r1 = r0 * 0.35
    rs = [r0 + (r1 - r0) * i / segs for i in range(segs + 1)]
    sweep(k(bark if r0 > 0.008 else twig_mat, 70), pts, rs, 5 if r0 > 0.012 else 3)
    if depth > 0:
        for c in range(kids):
            t = rng.uniform(0.35, 0.8)
            f = t * segs
            i = min(segs - 1, int(f))
            p = pts[i].lerp(pts[i + 1], f - i)
            tang = (pts[i + 1] - pts[i]).normalized()
            side = tang.orthogonal().normalized()
            side = Matrix.Rotation(rng.uniform(0, TAU), 3, tang) @ side
            cd = (tang * 0.65 + side * 0.6 + V((0, 0, 0.15))).normalized()
            _branch(k, P, rng, p, cd, length * rng.uniform(0.45, 0.65), rs[i] * 0.7, depth - 1, leaf_fn, bark, twig_mat,
                    gravity, kids, max(3, segs - 1))
    # Blaetter an den letzten Abschnitten
    for i in range(max(1, segs - 3), segs + 1):
        leaf_fn(pts[i], (pts[i] - pts[i - 1]).normalized())


def build_shrub_concrete():
    rng = random.Random(301)
    P = mats_plant()
    k = Kit()
    _crack_base(k, P, rng, rx=0.32, ry=0.12, chunks=5, lift=0.04)

    def leaves(p, tang):
        for _ in range(rng.randint(4, 6)):
            side = Matrix.Rotation(rng.uniform(0, TAU), 3, tang) @ tang.orthogonal().normalized()
            d = (side * 0.8 + tang * 0.45 + V((0, 0, -0.05))).normalized()
            L = rng.uniform(0.075, 0.11)
            leaf(k(_pick_leaf(P, rng), 0), p + side * 0.01, d, (0, 0, 1), L, L * 0.52, stations=((0.4, 1.0),), droop=0.4, fold=0.15)
    for i in range(7):
        a = TAU * i / 7 + jitter(rng, 0.3)
        tilt = rng.uniform(0.25, 0.65)
        d = V((math.cos(a) * tilt, math.sin(a) * tilt, 1.0))
        _branch(k, P, rng, (0.03 * math.cos(a), 0.03 * math.sin(a), 0.0), d, rng.uniform(0.8, 1.25), 0.016, 1, leaves,
                P['bark'], P['twig'], gravity=-0.04, kids=3, segs=5)
    # Laub am Boden
    for _ in range(16):
        a, r = rng.uniform(0, TAU), rng.uniform(0.15, 0.6)
        p = V((r * math.cos(a), r * math.sin(a), 0.003))
        d = V((math.cos(a * 3), math.sin(a * 3), 0.05)).normalized()
        leaf(k(P['leaf_brown'] if rng.random() < 0.6 else P['leaf_yellow'], 0), p, d, (0, 0, 1), 0.05, 0.026,
             stations=((0.4, 1.0),), fold=0.2, curl=0.25)
    k.finish()


def build_sapling():
    """Junge Birke (2,8 m) aus einem aufgebrochenen Loch im Estrich."""
    rng = random.Random(321)
    P = mats_plant()
    k = Kit()
    # aufgeworfene Plattenstuecke im Kreis, Erde
    out = _jag(rng, 0.0, 0.0, 0.22, 0.2, n=10, rough=0.25)
    quad_n(k(P['soil'], 0), [V((x, y, 0.004)) for (x, y) in out], (0, 0, 1))
    for i in range(5):
        a = TAU * i / 5 + jitter(rng, 0.2)
        o = _jag(rng, 0.0, 0.0, rng.uniform(0.12, 0.18), rng.uniform(0.09, 0.13), n=6, rough=0.2)
        M = tr(0.28 * math.cos(a), 0.28 * math.sin(a), 0.04) @ rot(0, 0, a) @ rot(0, -rng.uniform(0.12, 0.28), 0)
        _slab(k, P, rng, o, 0.07, M)
    # Stamm: leicht geneigt und gebogen, unten dunkle Borke, oben weiss mit Lentizellen
    H = 2.8
    trunk = []
    for i in range(11):
        t = i / 10
        trunk.append(V((0.06 * math.sin(t * 2.2) + 0.05 * t, 0.04 * math.sin(t * 3.1 + 1), H * t)))
    rs = [0.038 * (1 - 0.82 * i / 10) for i in range(11)]
    sweep(k(P['bark_dark'], 60), trunk[:3], rs[:3], 9)
    sweep(k(P['birch'], 60), trunk[2:], rs[2:], 9)
    for _ in range(26):  # Lentizellen: quer liegende dunkle Striche
        t = rng.uniform(0.25, 0.85)
        f = t * 10
        i = min(9, int(f))
        p = trunk[i].lerp(trunk[i + 1], f - i)
        r = rs[i] + (rs[i + 1] - rs[i]) * (f - i)
        a = rng.uniform(0, TAU)
        n = V((math.cos(a), math.sin(a), 0.0))
        c = p + n * (r + 0.0006)
        tg = V((-math.sin(a), math.cos(a), 0.0))
        w = rng.uniform(0.008, 0.025)
        quad_n(k(P['birch_mark'], 0), [c - tg * w + V((0, 0, -0.002)), c + tg * w + V((0, 0, -0.002)), c + tg * w + V((0, 0, 0.002)),
                                       c - tg * w + V((0, 0, 0.002))], n)

    def leaves(p, tang):
        for _ in range(rng.randint(5, 7)):
            side = Matrix.Rotation(rng.uniform(0, TAU), 3, tang) @ tang.orthogonal().normalized()
            d = (side * 0.7 + tang * 0.3 + V((0, 0, -0.45))).normalized()
            L = rng.uniform(0.05, 0.07)
            leaf(k(_pick_leaf(P, rng, (('leaf_mid', 0.45), ('leaf_light', 0.4), ('leaf_yellow', 0.15))), 0), p, d, (0, 0, 1), L,
                 L * 0.7, stations=((0.35, 1.0),), droop=0.3, fold=0.15)
    # Aeste spiralig ab 0,9 m, oben kuerzer; Birkenzweige haengen leicht
    nb = 13
    for j in range(nb):
        t = 0.32 + 0.62 * j / nb
        f = t * 10
        i = min(9, int(f))
        p = trunk[i].lerp(trunk[i + 1], f - i)
        a = j * 2.4
        up = 0.55 + 0.5 * t
        d = V((math.cos(a), math.sin(a), up))
        L = (0.95 - 0.7 * (t - 0.32)) * rng.uniform(0.8, 1.1)
        _branch(k, P, rng, p, d, L, rs[i] * 0.42, 1, leaves, P['twig'], P['twig'], gravity=-0.08, kids=2, segs=4)
    # Spitze
    _branch(k, P, rng, trunk[-1], V((0.05, 0.0, 1.0)), 0.25, rs[-1], 0, leaves, P['twig'], P['twig'], gravity=0.0, kids=0, segs=3)
    k.finish()


# --- ueberwucherte Holzkiste ------------------------------------------------------------------------------------------
def build_overgrown_crate():
    rng = random.Random(341)
    P = mats_plant()
    k = Kit()
    wood = material('kiste_verwittert', (0.46, 0.45, 0.41), look='wood', rough=0.9, r=1.35)
    wood_g = material('kiste_algen', (0.34, 0.38, 0.30), look='wood', rough=0.9, r=1.4)
    batten = material('kiste_leiste_alt', (0.33, 0.31, 0.27), look='wood', rough=0.9, r=1.4)
    L, W, H = 1.0, 0.7, 0.62
    t, gap = 0.02, 0.006
    n = 5
    h = (H - (n - 1) * gap) / n
    for i in range(n):
        z0 = i * (h + gap)
        mat = wood_g if i < 2 else wood
        cube(k(mat), (L / 2 - t, -W / 2 + t, z0), (L / 2, W / 2 - t, z0 + h), bevel=0.002)
        cube(k(mat), (-L / 2, -W / 2 + t, z0), (-L / 2 + t, W / 2 - t, z0 + h), bevel=0.002)
        cube(k(mat), (-L / 2, W / 2 - t, z0), (L / 2, W / 2, z0 + h), bevel=0.002)
        if i == 3:  # gebrochenes Brett vorn (-y): kuerzer, Splitter
            cube(k(mat), (-L / 2, -W / 2, z0), (0.05, -W / 2 + t, z0 + h), bevel=0.002)
            for s in range(3):
                blk(k(wood), (0.06 + s * 0.02, t * 0.8, h * 0.25), (0.08 + s * 0.02, -W / 2 + t / 2, z0 + h * (0.2 + 0.3 * s)), ry=0.1 * s,
                    bevel=0.001)
        else:
            cube(k(mat), (-L / 2, -W / 2, z0), (L / 2, -W / 2 + t, z0 + h), bevel=0.002)
    # Deckel: Bretter, eines verschoben
    m_ = 5
    w = (W - (m_ - 1) * gap) / m_
    for i in range(m_):
        y0 = -W / 2 + i * (w + gap)
        dx = 0.06 if i == 1 else 0.0
        cube(k(wood), (-L / 2 + dx, y0, H), (L / 2 + dx, y0 + w, H + t), bevel=0.002)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x0, y0 = sx * L / 2, sy * W / 2
            xa, xb = sorted((x0 - sx * 0.05, x0 + sx * 0.02))
            ya, yb = sorted((y0 - sy * 0.05, y0 + sy * 0.02))
            cube(k(batten), (xa, ya, 0.0), (xb, yb, H), bevel=0.003)
    # Moos auf dem Deckel und an den unteren Kanten
    _cushion(k, P, rng, 0.05, 0.02, 0.40, 0.27, 0.04, rings=5, segs=22, z0=H + t, wet_ring=False, dry=False)
    _cushion(k, P, rng, -0.38, -0.2, 0.12, 0.1, 0.025, rings=3, segs=12, z0=H + t, wet_ring=False, dry=False)
    for _ in range(5):
        x = rng.uniform(-L / 2, L / 2)
        blob(k(P['moss'], 60), (x, -W / 2 - 0.004, rng.uniform(0.02, 0.12)), (0, -1, 0), rng.uniform(0.04, 0.08), 0.012, rng, squash=0.5)
    # Efeu klettert an der Vorder- und Seitenwand hoch und ueber die Kante
    # (Stengel laeuft an der Seitenflaeche hoch, ueber die Kante und ein Stueck ueber den Deckel)
    climbs = [((0, -1, 0), -0.32, 0.0), ((0, -1, 0), 0.05, 0.0), ((0, -1, 0), 0.34, 0.0), ((1, 0, 0), -0.15, 0.0),
              ((1, 0, 0), 0.2, 0.0)]
    for (side, u0, _) in climbs:
        side = V(side)
        along = V((-side.y, side.x, 0.0))
        face = V((side.x * (L / 2 + 0.012), side.y * (W / 2 + 0.012), 0.0))
        pts = []
        top = rng.uniform(0.75, 1.0)
        for i in range(8):
            tt = i / 7
            pts.append(face + along * (u0 + 0.10 * math.sin(tt * 4 + u0 * 5)) + V((0, 0, 0.02 + (H + 0.03) * tt * top)))
        if top > 0.85:
            pts += [face - side * 0.05 + along * (u0 + 0.08) + V((0, 0, H + t + 0.01)),
                    face - side * 0.22 + along * (u0 + 0.03) + V((0, 0, H + t + 0.012))]
        sweep(k(P['stem_brown'], 70), pts, 0.005, 5)
        for p in pts[1:]:
            for _ in range(2):
                q = p + along * jitter(rng, 0.05) + V((0, 0, jitter(rng, 0.04)))
                outw = side if p.z < H else V((0, 0, 1))
                _ivy_leaf(k, P, rng, q, outw, rng.uniform(0.06, 0.09))
    # Grasbueschel am Fuss
    for (x, y) in ((-0.55, -0.38), (0.52, -0.40), (0.56, 0.3), (-0.2, -0.42)):
        for b in range(9):
            a = rng.uniform(0, TAU)
            lb = rng.uniform(0.12, 0.28)
            base = V((x + jitter(rng, 0.03), y + jitter(rng, 0.03), 0.0))
            tip = base + V((math.cos(a) * lb * 0.45, math.sin(a) * lb * 0.45, lb))
            mid = base.lerp(tip, 0.5) + V((0, 0, 0.02))
            wv = V((-math.sin(a), math.cos(a), 0.0)) * 0.004
            mat = P['grass'] if rng.random() < 0.7 else P['grass_dry']
            two_sided(k(mat, 0), [[base - wv, base + wv, mid + wv * 0.7, mid - wv * 0.7], [mid - wv * 0.7, mid + wv * 0.7, tip]])
    # kleiner Farn an der Ecke
    for i in range(4):
        a = -0.9 + i * 0.5
        _frond(k, P, rng, (0.6, -0.42, 0.004), (math.cos(a), math.sin(a), 0), rng.uniform(0.25, 0.38), rng.uniform(0.15, 0.25), pairs=8)
    k.finish()


# =====================================================================================================================
# FABLED - altes Holz in Zinnoberrot (abgeplatzt), darueber Neon und farbige Kabel
# =====================================================================================================================
def mats_fabled():
    return {
        'lac': material('zinnober', (0.40, 0.10, 0.06), look='paint', rough=0.45, r=0.8),
        'lac_dark': material('zinnober_dunkel', (0.24, 0.06, 0.04), look='paint', rough=0.6, r=0.95),
        'lac_worn': material('zinnober_matt', (0.36, 0.13, 0.09), look='paint', rough=0.7, r=1.2),
        'lac_black': material('lack_schwarz', (0.045, 0.035, 0.03), look='paint', rough=0.4, r=0.6),
        'wood': material('holz_bloss', (0.36, 0.22, 0.13), look='wood', tex='wood_walnut_floor', rough=0.8, r=1.15),
        'wood_dark': material('holz_dunkel', (0.20, 0.12, 0.08), look='wood', tex='wood_walnut_floor', rough=0.85, r=1.25),
        'ground': material('kreidegrund', (0.70, 0.62, 0.50), look='paint', rough=0.9, r=1.35),
        'grime': material('schmutz', (0.22, 0.16, 0.12), look='paint', rough=0.95, r=1.4),
        'brass': material('messing', (0.72, 0.54, 0.27), look='metal', rough=0.35, metal=0.9, r=0.6, m=0.85),
        'brass_dark': material('messing_angelaufen', (0.40, 0.31, 0.17), look='metal', rough=0.6, metal=0.8, r=0.95, m=0.65),
        'interior': material('innen_dunkel', (0.08, 0.05, 0.035), look='wood', rough=0.9, r=1.3),
        'steel_dark': material('stahl_dunkel', (0.20, 0.20, 0.21), look='metal', rough=0.6, metal=0.8, r=1.15, m=0.55),
        'black_plastic': material('kunststoff_schwarz', (0.06, 0.06, 0.065), look='plastic', rough=0.6, r=1.0),
        'grey_box': material('trafo_grau', (0.30, 0.31, 0.31), look='paint', rough=0.55, r=1.0),
        'glass_dark': material('elektrode_glas', (0.14, 0.13, 0.13), look='glass', rough=0.2, r=0.4),
        'clear': material('halter_klar', (0.62, 0.64, 0.64), look='plastic', rough=0.3, r=0.6),
        'copper': material('kupfer', (0.80, 0.45, 0.25), look='metal', rough=0.35, metal=0.9, r=0.6, m=0.85),
        'galv': material('verzinkt', (0.66, 0.67, 0.66), look='galvanized', rough=0.5, metal=0.8, r=1.05, m=0.6),
        'tape': material('isolierband', (0.04, 0.04, 0.04), look='plastic', rough=0.5, r=0.8),
        'tie': material('kabelbinder', (0.85, 0.85, 0.80), look='plastic', rough=0.5, r=0.9),
    }


def _chips_rect(k, F, rng, M, axis, sign, a0, a1, b0, b1, level, n, size, edge_bias=0.7, mats=('wood', 'ground', 'wood_dark')):
    """Abplatzer auf einer Rechteckflaeche (lokal): Flaeche senkrecht zu axis (0 x, 1 y, 2 z) bei level, Normale sign.
    a/b: Grenzen in den beiden anderen Achsen. Abplatzer liegen bevorzugt an den Kanten (wo Lack abgestossen wird)."""
    other = [i for i in range(3) if i != axis]
    nrm_l = V((0, 0, 0))
    nrm_l[axis] = sign
    nrm = (M.to_3x3() @ nrm_l).normalized()
    for _ in range(n):
        if rng.random() < edge_bias:
            e = rng.randrange(4)
            ca = (a0 + rng.uniform(0, 1) * (a1 - a0)) if e < 2 else (a0 + size * 0.6 if e == 2 else a1 - size * 0.6)
            cb = (b0 + size * 0.6 if e == 0 else b1 - size * 0.6) if e < 2 else (b0 + rng.uniform(0, 1) * (b1 - b0))
        else:
            ca, cb = rng.uniform(a0, a1), rng.uniform(b0, b1)
        s = size * rng.uniform(0.4, 1.0)
        pts = []
        nv = rng.randint(5, 7)
        ph = rng.uniform(0, TAU)
        for i in range(nv):
            ang = ph + TAU * i / nv
            p = V((0, 0, 0))
            p[axis] = level + sign * 0.0005
            p[other[0]] = min(a1, max(a0, ca + math.cos(ang) * s * rng.uniform(0.5, 1.0)))
            p[other[1]] = min(b1, max(b0, cb + math.sin(ang) * s * rng.uniform(0.3, 0.8)))
            pts.append(M @ p)
        quad_n(k(F[rng.choice(mats)]), pts, nrm)


def _box_chips(k, F, rng, lo, hi, M, faces, n, size, **kw):
    """Abplatzer auf ausgewaehlten Seiten eines Quaders lo..hi (faces z. B. '+x -y +z')."""
    for f in faces.split():
        s = 1 if f[0] == '+' else -1
        ax = 'xyz'.index(f[1])
        o = [i for i in range(3) if i != ax]
        level = hi[ax] if s > 0 else lo[ax]
        _chips_rect(k, F, rng, M, ax, s, lo[o[0]], hi[o[0]], lo[o[1]], hi[o[1]], level, n, size, **kw)


def _raised_panel(k, mat, M, y0, y1, z0, z1, x0, h, inset):
    """Erhabene Fuellung: Rechteck y0..y1 / z0..z1 auf x0, Spiegel (inset) um h vorstehend, mit Schraegen."""
    def P(x, y, z):
        return M @ V((x, y, z))
    o = [P(x0, y0, z0), P(x0, y1, z0), P(x0, y1, z1), P(x0, y0, z1)]
    i_ = [P(x0 + h, y0 + inset, z0 + inset), P(x0 + h, y1 - inset, z0 + inset), P(x0 + h, y1 - inset, z1 - inset),
          P(x0 + h, y0 + inset, z1 - inset)]
    n = M.to_3x3() @ V((1, 0, 0))
    bm = k(mat)
    quad_n(bm, i_, n)
    for a in range(4):
        b = (a + 1) % 4
        quad_n(bm, [o[a], o[b], i_[b], i_[a]], n)


def _prism_y(k, mat, prof, y0, y1, M=None, smooth=0.0):
    """Profilleiste: Querschnitt prof [(x, z), ...] entlang y von y0 bis y1 (geschlossener Koerper)."""
    src = bmesh.new()
    a = [src.verts.new((x, y0, z)) for (x, z) in prof]
    b = [src.verts.new((x, y1, z)) for (x, z) in prof]
    n = len(prof)
    for i in range(n):
        j = (i + 1) % n
        src.faces.new((a[i], a[j], b[j], b[i]))
    src.faces.new(a)
    src.faces.new(list(reversed(b)))
    bmesh.ops.recalc_face_normals(src, faces=list(src.faces))
    emit(src, lambda f: k(mat, smooth), M)


# --- Wandvertaefelung (Wandmodell 1 m breit, Ursprung am Boden, Wand bei x = 0) ----------------------------------------
def build_panel_vermilion():
    rng = random.Random(401)
    F = mats_fabled()
    k = Kit()
    H = 2.2
    # Blindboden (Rueckwand) und Rahmen: halbe Aussenstiele (stossen an das Nachbarmodul), Mittelstiel, drei Riegel
    cube(k(F['lac_dark']), (0.0, -0.5, 0.0), (0.012, 0.5, H))
    frame = [((-0.5, 0.12), (-0.455, 2.08)), ((0.455, 0.12), (0.5, 2.08)), ((-0.035, 0.12), (0.035, 2.08))]
    for (y0, z0), (y1, z1) in frame:
        cube(k(F['lac']), (0.012, y0, z0), (0.034, y1, z1), bevel=0.003)
    for (z0, z1) in ((0.12, 0.24), (0.70, 0.79), (1.84, 1.92), (2.02, 2.08)):
        cube(k(F['lac']), (0.012, -0.455, z0), (0.034, 0.455, z1), bevel=0.003)
    # sechs erhabene Fuellungen: Sockelfeld, hohes Feld, schmaler Fries
    for (y0, y1) in ((-0.455, -0.035), (0.035, 0.455)):
        for (z0, z1, ins) in ((0.24, 0.70, 0.045), (0.79, 1.84, 0.05), (1.92, 2.02, 0.02)):
            _raised_panel(k, F['lac'], Matrix.Identity(4), y0 + 0.004, y1 - 0.004, z0 + 0.004, z1 - 0.004, 0.018, 0.010, ins)
    # Sockelleiste (dunkler, verschmutzt) und Abdeckleiste oben (Profil)
    _prism_y(k, F['lac_dark'], [(0.0, 0.0), (0.040, 0.0), (0.040, 0.10), (0.032, 0.115), (0.032, 0.125), (0.0, 0.125)], -0.5, 0.5)
    _prism_y(k, F['lac'], [(0.0, 2.08), (0.040, 2.08), (0.040, 2.11), (0.056, 2.13), (0.056, 2.17), (0.036, 2.20), (0.0, 2.20)],
             -0.5, 0.5)
    # Abplatzer an Kanten (Riegel, Stiele, Sockel), Schmutz unten, Kratzer
    for (z0, z1) in ((0.12, 0.24), (0.70, 0.79)):
        _box_chips(k, F, rng, (0.012, -0.455, z0), (0.034, 0.455, z1), Matrix.Identity(4), '+x', 2, 0.02)
    _box_chips(k, F, rng, (0.0, -0.5, 0.0), (0.040, 0.5, 0.10), Matrix.Identity(4), '+x', 4, 0.025, mats=('wood_dark', 'wood', 'grime'))
    _box_chips(k, F, rng, (0.012, -0.5, 0.12), (0.034, -0.455, 2.08), Matrix.Identity(4), '+x', 2, 0.015)
    _box_chips(k, F, rng, (0.012, -0.035, 0.12), (0.034, 0.035, 2.08), Matrix.Identity(4), '+x', 2, 0.015)
    for (y0, y1) in ((-0.41, -0.08), (0.08, 0.41)):
        _chips_rect(k, F, rng, Matrix.Identity(4), 0, 1, y0, y1, 0.29, 0.65, 0.0285, 2, 0.07, edge_bias=0.3,
                    mats=('lac_worn', 'lac_worn', 'grime'))
        _chips_rect(k, F, rng, Matrix.Identity(4), 0, 1, y0, y1, 0.84, 1.79, 0.0285, 2, 0.09, edge_bias=0.2,
                    mats=('lac_worn', 'lac_dark'))
    for _ in range(3):  # Kratzer: helle, duenne Striche in der unteren Fuellung
        y, z = rng.uniform(-0.4, 0.4), rng.uniform(0.3, 0.62)
        a = rng.uniform(-0.6, 0.6)
        L, w = rng.uniform(0.05, 0.14), 0.0016
        d = V((0, math.cos(a), math.sin(a)))
        c = V((0.0285, y, z))
        nn = V((0, -math.sin(a), math.cos(a))) * w
        quad_n(k(F['ground']), [c - d * L / 2 - nn, c + d * L / 2 - nn, c + d * L / 2 + nn, c - d * L / 2 + nn], (1, 0, 0))
    k.finish()


# --- Neonroehre an der Wand (Ursprung: Wand, Mitte der Roehre; Karte setzt z = Montagehoehe) ----------------------------
NEON = [(1.0, 0.16, 0.10), (0.22, 1.0, 0.36), (0.22, 0.46, 1.0), (0.64, 0.22, 1.0)]


def _boot(k, F, p, d):
    """Durchfuehrung (Elektrodenhuelse) an der Wand: schwarzer Gummischuh, in den die Roehre fuehrt."""
    lathe(k(F['black_plastic'], 50), [(0.0, 0.0), (0.016, 0.0), (0.015, 0.02), (0.011, 0.034), (0.0, 0.034)], 10,
          tr(*p) @ V(d).to_track_quat('Z', 'Y').to_matrix().to_4x4())


def _neon_path(k, F, glow, pts, r=0.0075, dark_ends=0.045):
    """Leuchtroehre entlang pts (Glas leuchtet), Enden als dunkle Elektrodenstuecke zur Wand."""
    pts = [V(p) for p in pts]
    # Bogenlaenge, um die Elektrodenenden abzutrennen
    L = [0.0]
    for a, b in zip(pts, pts[1:]):
        L.append(L[-1] + (b - a).length)
    total = L[-1]

    def at(s):
        for i in range(len(pts) - 1):
            if L[i + 1] >= s:
                return pts[i].lerp(pts[i + 1], (s - L[i]) / max(1e-9, L[i + 1] - L[i]))
        return pts[-1]
    mid = [at(dark_ends)] + [p for p, s in zip(pts, L) if dark_ends < s < total - dark_ends] + [at(total - dark_ends)]
    sweep(k(glow, 60), mid, r, 10, caps=False)
    head = [p for p, s in zip(pts, L) if s <= dark_ends] + [at(dark_ends)]
    tail = [at(total - dark_ends)] + [p for p, s in zip(pts, L) if s >= total - dark_ends]
    for seg in (head, tail):
        if len(seg) >= 2:
            sweep(k(F['glass_dark'], 60), seg, r * 1.02, 10, caps=True)


def _bend(a, corner, b, n=4, rad=0.03):
    """Abgerundete Ecke von a ueber corner nach b (Punkte)."""
    a, corner, b = V(a), V(corner), V(b)
    da, db = (a - corner).normalized(), (b - corner).normalized()
    p0, p1 = corner + da * rad, corner + db * rad
    out = []
    for i in range(n + 1):
        t = i / n
        q = p0.lerp(corner, t).lerp(corner.lerp(p1, t), t)
        out.append(q)
    return out


def _support(k, F, p, xw=0.0):
    """Roehrenhalter: Abstandshalter aus klarem Kunststoff mit Klammer."""
    cyl(k(F['clear'], 50), (xw, p[1], p[2]), (p[0] - 0.008, p[1], p[2]), 0.005, 6)
    lathe(k(F['clear'], 50), [(0.0085, -0.004), (0.0105, -0.004), (0.0105, 0.004), (0.0085, 0.004), (0.0085, -0.004)], 10,
          tr(*p) @ rot(math.pi / 2, 0, 0))


def _neon_tube(variant):
    rng = random.Random(421 + variant)
    F = mats_fabled()
    k = Kit()
    glow = material(f'neon_{variant}', NEON[variant], look='glow', glow=1.5)
    X = 0.055
    yl = 0.60
    pts = [V((0.03, -yl - 0.03, 0.0))] + _bend((0.03, -yl - 0.03, 0.0), (X, -yl - 0.03, 0.0), (X, 0.0, 0.0))[1:] + \
        _bend((X, 0.0, 0.0), (X, yl + 0.03, 0.0), (0.03, yl + 0.03, 0.0)) + [V((0.03, yl + 0.03, 0.0))]
    _neon_path(k, F, glow, pts)
    for s in (-1, 1):
        _boot(k, F, (0.0, s * (yl + 0.03), 0.0), (1, 0, 0))
    for y in (-0.35, 0.05, 0.42):
        _support(k, F, (X, y + jitter(rng, 0.03), 0.0))
    # Trafo unter der Roehre, Hochspannungskabel zu beiden Enden, Netzkabel nach oben
    ty = 0.18 + jitter(rng, 0.05)
    cube(k(F['grey_box']), (0.0, ty - 0.10, -0.24), (0.065, ty + 0.10, -0.12), bevel=0.004)
    cube(k(material('typenschild', (0.80, 0.78, 0.70), look='paint', rough=0.6, r=1.0)), (0.065, ty - 0.05, -0.20), (0.0656, ty + 0.03, -0.16))
    for s in (-1, 1):
        cyl(k(F['steel_dark'], 50), (0.0, ty + s * 0.095, -0.13), (0.07, ty + s * 0.095, -0.13), 0.004, 6)
    cab = k(F['black_plastic'], 60)
    for s in (-1, 1):
        ye = s * (yl + 0.03)
        sweep(cab, catmull([(0.006, ty + s * 0.10, -0.18), (0.006, ty + s * 0.16, -0.17), (0.006, (ty + ye) / 2, -0.10),
                            (0.006, ye, -0.03), (0.012, ye, -0.012)], 3), 0.0035, 5)
    sweep(cab, catmull([(0.006, ty - 0.02, -0.12), (0.006, ty - 0.03, 0.05), (0.006, ty - 0.02, 0.30), (0.006, ty - 0.03, 0.62)], 3),
          0.004, 5)
    for z in (0.18, 0.45):  # Kabelschellen
        cube(k(F['clear']), (0.0, ty - 0.04, z - 0.006), (0.012, ty - 0.01, z + 0.006))
    k.finish()


def build_neon_tube():
    _neon_tube(0)


def build_neon_tube__v1():
    _neon_tube(1)


def build_neon_tube__v2():
    _neon_tube(2)


def build_neon_tube__v3():
    _neon_tube(3)


# --- Neonform ohne Schrift (Wandmodell, Ursprung Mitte) ---------------------------------------------------------------
def _neon_sign(variant):
    rng = random.Random(441 + variant)
    F = mats_fabled()
    k = Kit()
    X = 0.07
    # Traeger (Kabelkanal, schwarz lackiert) unten, darin der Trafo
    cube(k(F['lac_black']), (0.0, -0.45, -0.36), (0.07, 0.45, -0.28), bevel=0.005)
    for y in (-0.38, 0.38):
        cube(k(F['steel_dark']), (0.0, y - 0.02, -0.37), (0.005, y + 0.02, -0.27))
    supports = []
    if variant == 0:
        pink = material('neon_pink', (1.0, 0.22, 0.45), look='glow', glow=1.5)
        cyan = material('neon_cyan', (0.20, 0.90, 1.0), look='glow', glow=1.4)
        for (mat, zc, ph, amp) in ((pink, 0.06, 0.0, 0.10), (cyan, -0.10, 0.6, 0.08)):
            wave = []
            for i in range(29):
                t = i / 28
                y = -0.52 + 1.04 * t
                wave.append(V((X, y, zc + amp * math.sin(t * TAU * 1.5 + ph) * (0.6 + 0.4 * math.sin(math.pi * t)))))
            ends = [V((0.02, wave[0].y - 0.02, wave[0].z))] + wave + [V((0.02, wave[-1].y + 0.02, wave[-1].z))]
            _neon_path(k, F, mat, ends)
            for p in (wave[0], wave[-1]):
                _boot(k, F, (0.0, p.y + (-0.02 if p.y < 0 else 0.02), p.z), (1, 0, 0))
            supports += [wave[5], wave[14], wave[23]]
        # Raute (gelb) rechts oben
        yel = material('neon_gelb', (1.0, 0.80, 0.18), look='glow', glow=1.3)
        c = V((X, 0.36, 0.26))
        dia = [c + V((0, 0, 0.09)), c + V((0, 0.07, 0)), c + V((0, 0, -0.09)), c + V((0, -0.07, 0)), c + V((0, 0, 0.088))]
        _neon_path(k, F, yel, [V((0.02, c.y, c.z + 0.11))] + dia, dark_ends=0.03)
        _boot(k, F, (0.0, c.y, c.z + 0.11), (1, 0, 0))
        supports.append(c + V((0, 0.07, 0)))
    else:
        orange = material('neon_orange', (1.0, 0.45, 0.10), look='glow', glow=1.5)
        violet = material('neon_violett', (0.66, 0.24, 1.0), look='glow', glow=1.5)
        ring = []
        for i in range(37):
            a = math.pi / 2 + TAU * i / 36 * 0.96
            ring.append(V((X, 0.30 * math.cos(a), 0.30 * math.sin(a) + 0.0)))
        _neon_path(k, F, orange, [V((0.02, ring[0].y, ring[0].z + 0.03))] + ring + [V((0.02, ring[-1].y, ring[-1].z + 0.03))])
        for p in (ring[0], ring[-1]):
            _boot(k, F, (0.0, p.y, p.z + 0.03), (1, 0, 0))
        zig = []
        for i in range(9):
            t = i / 8
            zig.append(V((X + 0.012, -0.20 + 0.40 * t, 0.07 * (1 if i % 2 else -1) * (1.0 - abs(t - 0.5)))))
        zz = catmull(zig, 3)
        _neon_path(k, F, violet, [V((0.02, zz[0].y, zz[0].z))] + zz + [V((0.02, zz[-1].y, zz[-1].z))])
        for p in (zz[0], zz[-1]):
            _boot(k, F, (0.0, p.y, p.z), (1, 0, 0))
        supports += [ring[6], ring[18], ring[30], zz[6], zz[18]]
    for p in supports:
        _support(k, F, p)
    # Hochspannungskabel vom Traeger zu den Durchfuehrungen
    for y in (-0.3, 0.3):
        sweep(k(F['black_plastic'], 60), [(0.006, y, -0.28), (0.006, y * 1.2, -0.10), (0.006, y * 1.3, 0.05)], 0.003, 5)
    k.finish()


def build_neon_sign():
    _neon_sign(0)


def build_neon_sign__v1():
    _neon_sign(1)


# --- farbige Kabel von der Decke (Deckenmodell, Oberseite z = 0) -----------------------------------------------------------
CABLE_COLORS = [(0.62, 0.08, 0.06), (0.80, 0.62, 0.08), (0.10, 0.22, 0.55), (0.10, 0.40, 0.18), (0.80, 0.80, 0.76),
                (0.05, 0.05, 0.05), (0.85, 0.36, 0.06), (0.42, 0.42, 0.43), (0.45, 0.20, 0.45)]


def _cable_mat(i):
    names = ['rot', 'gelb', 'blau', 'gruen', 'weiss', 'schwarz', 'orange', 'grau', 'violett']
    return material(f'kabel_{names[i % 9]}', CABLE_COLORS[i % 9], look='rubber', rough=0.55, r=0.85)


def _stripped_end(k, F, p, d, r):
    """Abisoliertes Kabelende: Kupferlitze."""
    d = V(d).normalized()
    cyl(k(F['copper'], 50), p, V(p) + d * 0.025, r * 0.55, 5)


def _cable_hang(variant):
    rng = random.Random(461 + variant)
    F = mats_fabled()
    k = Kit()
    if variant == 0:
        # Abzweigdose an der Decke, Haken auf der anderen Seite; Kabel haengen in Boegen dazwischen
        A, B = V((0.0, -0.95, -0.06)), V((0.0, 0.95, -0.04))
        cube(k(F['grey_box']), (-0.08, -1.03, -0.09), (0.08, -0.87, 0.0), bevel=0.006)
        cyl(k(F['steel_dark'], 50), (0.0, 0.95, 0.0), (0.0, 0.95, -0.05), 0.006, 6)
        lathe(k(F['steel_dark'], 50), [(0.015, -0.004), (0.022, -0.004), (0.022, 0.004), (0.015, 0.004), (0.015, -0.004)], 8,
              tr(0.0, 0.95, -0.065) @ rot(math.pi / 2, 0, 0))
        for i in range(8):
            sag = rng.uniform(0.35, 0.95)
            off = V((jitter(rng, 0.035), 0.0, jitter(rng, 0.01)))
            pts = []
            for j in range(15):
                t = j / 14
                p = A.lerp(B, t) + off * (1 - abs(2 * t - 1))
                p.z -= sag * (1 - (2 * t - 1) ** 2) ** 1.1
                p.x += 0.06 * math.sin(t * math.pi * 2 + i) * (1 - abs(2 * t - 1))
                pts.append(p)
            sweep(k(_cable_mat(i), 60), pts, rng.uniform(0.004, 0.0065), 5)
        # zwei Kabel haengen vom Haken weiter herunter, offene Enden
        for i in range(2):
            pts = [B + V((0, 0, -0.01)), B + V((0.02 * i, 0.03, -0.25)), B + V((0.04 - 0.08 * i, 0.06, -0.55)),
                   B + V((0.03 - 0.07 * i, 0.04, -0.75 - 0.2 * i))]
            pp = catmull(pts, 4)
            sweep(k(_cable_mat(i + 6), 60), pp, 0.0045, 5)
            _stripped_end(k, F, pp[-1], pp[-1] - pp[-2], 0.0045)
        for p in (A + V((0, 0.05, -0.01)), B + V((0, -0.05, -0.02))):  # Kabelbinder am Buendel
            lathe(k(F['tie'], 50), [(0.018, -0.003), (0.021, -0.003), (0.021, 0.003), (0.018, 0.003), (0.018, -0.003)], 8,
                  tr(*p) @ rot(math.pi / 2, 0, 0))
    else:
        # abgerissene Kabeltrasse (Stueck Gitterrinne), Buendel faellt heraus, Enden lose, Isolierband
        g = k(F['galv'], 0)
        for x in (-0.1, 0.1):
            cyl(g, (x, -0.35, -0.06), (x, 0.15, -0.06), 0.0025, 4)
        for y in [-0.35 + 0.1 * i for i in range(6)]:
            sweep(g, [(-0.1, y, -0.0), (-0.1, y, -0.06), (0.1, y, -0.06), (0.1, y, 0.0)], 0.0022, 4)
        cyl(k(F['steel_dark'], 50), (0.0, -0.1, 0.0), (0.0, -0.1, -0.06), 0.005, 6)
        for i in range(9):
            L = rng.uniform(0.45, 1.05)
            x0, y0 = jitter(rng, 0.06), rng.uniform(-0.3, 0.05)
            pts = [V((x0, y0, -0.05)), V((x0 * 0.6, 0.12 + jitter(rng, 0.02), -0.07)), V((x0 * 0.4 + jitter(rng, 0.03), 0.2, -0.2)),
                   V((jitter(rng, 0.05), 0.22 + jitter(rng, 0.04), -0.2 - L * 0.5)), V((jitter(rng, 0.08), 0.2 + jitter(rng, 0.06), -0.2 - L))]
            if i % 4 == 0:  # Schlaufe am Ende
                e = pts[-1]
                pts += [e + V((0.04, 0.0, 0.05)), e + V((0.06, 0.03, 0.12))]
            pp = catmull(pts, 4)
            r = rng.uniform(0.0035, 0.006)
            sweep(k(_cable_mat(i), 60), pp, r, 5)
            if i % 3 != 2:
                _stripped_end(k, F, pp[-1], pp[-1] - pp[-2], r)
        for z in (-0.32, -0.55):  # Isolierband ums Buendel
            lathe(k(F['tape'], 50), [(0.0, z - 0.02), (0.03, z - 0.02), (0.031, z + 0.02), (0.0, z + 0.02)], 10,
                  tr(0.0, 0.21, 0.0))
        # Verbindungsklemme an einem Kabel
        cube(k(material('klemme_orange', (0.85, 0.40, 0.08), look='plastic', rough=0.5, r=0.8)), (-0.012, 0.19, -0.72),
             (0.012, 0.21, -0.68), bevel=0.002)
    k.finish()


def build_cable_hang():
    _cable_hang(0)


def build_cable_hang__v1():
    _cable_hang(1)


# --- Schrank, Tisch, Bank, Paravent (Bodenmodelle, Blick +x) --------------------------------------------------------------
def _pull(k, F, M, p, r=0.032):
    """Runde Messingrosette mit Buegelgriff (chinesischer Beschlag)."""
    Mp = M @ tr(*p) @ rot(0, math.pi / 2, 0)
    lathe(k(F['brass_dark'], 40), [(0.0, 0.0), (r, 0.0), (r * 0.95, 0.003), (r * 0.3, 0.005), (0.0, 0.005)], 14, Mp)
    arc = [Mp @ V((-0.002, r * 0.7 * math.cos(a), r * 0.7 * math.sin(a) * 0.0 + 0.006 + 0.014 * math.sin(a))) for a in
           [math.pi * i / 8 for i in range(9)]]
    sweep(k(F['brass'], 60), arc, 0.003, 5)


def build_cabinet_vermilion():
    rng = random.Random(481)
    F = mats_fabled()
    k = Kit()
    W, D, H = 1.0, 0.48, 1.86
    x0, x1 = -D / 2, D / 2
    I = Matrix.Identity(4)
    # Sockel mit ausgeschnittenen Fuessen (Vorderseite), dunkel verschmutzt
    for (y0, y1) in ((-W / 2, -W / 2 + 0.09), (W / 2 - 0.09, W / 2)):
        cube(k(F['lac_dark']), (x0, y0, 0.0), (x1, y1, 0.11), bevel=0.004)
    cube(k(F['lac_dark']), (x1 - 0.03, -W / 2, 0.06), (x1, W / 2, 0.11), bevel=0.004)
    cube(k(F['lac_dark']), (x0, -W / 2, 0.06), (x0 + 0.03, W / 2, 0.11), bevel=0.004)
    # Korpus: Seiten, Rueckwand, Boden, Deckel mit Ueberstand und Kranzprofil
    lac = F['lac']
    cube(k(lac), (x0, -W / 2, 0.11), (x1, -W / 2 + 0.025, H - 0.06), bevel=0.003)
    cube(k(lac), (x0, W / 2 - 0.025, 0.11), (x1, W / 2, H - 0.06), bevel=0.003)
    cube(k(F['lac_dark']), (x0, -W / 2 + 0.025, 0.11), (x0 + 0.015, W / 2 - 0.025, H - 0.06))
    cube(k(F['interior']), (x0 + 0.015, -W / 2 + 0.025, 0.11), (x1 - 0.02, W / 2 - 0.025, 0.13))
    cube(k(lac), (x0 - 0.02, -W / 2 - 0.03, H - 0.06), (x1 + 0.035, W / 2 + 0.03, H - 0.03), bevel=0.004)
    cube(k(lac), (x0 - 0.01, -W / 2 - 0.015, H - 0.03), (x1 + 0.02, W / 2 + 0.015, H), bevel=0.006)
    # Front: Rahmen (Stiele, Riegel), zwei Schubladen, zwei Tueren (eine einen Spalt offen)
    xf = x1
    cube(k(lac), (xf - 0.02, -W / 2 + 0.025, 0.11), (xf, W / 2 - 0.025, 0.16), bevel=0.003)
    cube(k(lac), (xf - 0.02, -W / 2 + 0.025, 0.36), (xf, W / 2 - 0.025, 0.40), bevel=0.003)
    cube(k(lac), (xf - 0.02, -W / 2 + 0.025, H - 0.11), (xf, W / 2 - 0.025, H - 0.06), bevel=0.003)
    cube(k(lac), (xf - 0.02, -0.02, 0.16), (xf, 0.02, H - 0.11), bevel=0.003)
    cube(k(F['interior']), (x0 + 0.015, -W / 2 + 0.025, 0.40), (xf - 0.03, W / 2 - 0.025, H - 0.11))
    for (y0, y1) in ((-W / 2 + 0.03, -0.025), (0.025, W / 2 - 0.03)):
        cube(k(lac), (xf - 0.018, y0, 0.165), (xf + 0.004, y1, 0.355), bevel=0.004)
        _raised_panel(k, lac, I, y0 + 0.02, y1 - 0.02, 0.185, 0.335, xf + 0.004, 0.006, 0.025)
        _pull(k, F, I, (xf + 0.01, (y0 + y1) / 2, 0.26), 0.026)
    hinge_y = {0: -W / 2 + 0.03, 1: W / 2 - 0.03}
    for d in (0, 1):
        y0, y1 = ((-W / 2 + 0.03, -0.022) if d == 0 else (0.022, W / 2 - 0.03))
        ajar = math.radians(7.0) if d == 1 else 0.0
        hy = hinge_y[d]
        Md = tr(xf, hy, 0.0) @ rot(0, 0, ajar) @ tr(-xf, -hy, 0.0)
        z0, z1 = 0.405, H - 0.115
        cube(k(lac), (xf - 0.02, y0, z0), (xf + 0.004, y1, z1), Md, bevel=0.004)
        for (za, zb) in ((z0 + 0.06, z0 + 0.62), (z0 + 0.70, z1 - 0.06)):
            _raised_panel(k, lac, Md, y0 + 0.05, y1 - 0.05, za, zb, xf + 0.004, 0.008, 0.035)
        for zh in (z0 + 0.12, (z0 + z1) / 2, z1 - 0.12):  # Messingbaender
            cube(k(F['brass_dark']), (xf + 0.004, hy - 0.004 if d == 0 else hy - 0.05, zh - 0.035),
                 (xf + 0.0065, hy + 0.05 if d == 0 else hy + 0.004, zh + 0.035), Md)
        yy = y1 - 0.06 if d == 0 else y0 + 0.06
        _pull(k, F, Md, (xf + 0.006, yy, (z0 + z1) / 2 + 0.05), 0.03)
        # abgegriffene Stelle um den Griff (Lack weg, Holz dunkel)
        _chips_rect(k, F, rng, Md, 0, 1, yy - 0.06, yy + 0.06, (z0 + z1) / 2 - 0.05, (z0 + z1) / 2 + 0.15, xf + 0.004, 4, 0.03,
                    edge_bias=0.0, mats=('wood_dark', 'wood'))
        _box_chips(k, F, rng, (xf - 0.02, y0, z0), (xf + 0.004, y1, z1), Md, '+x', 6, 0.025)
        _box_chips(k, F, rng, (xf - 0.02, y0, z0), (xf + 0.0045, y1, z1), Md, '+x', 2, 0.09, edge_bias=0.1,
                   mats=('lac_worn', 'lac_dark'))
    # Schloss mit Rosette in der Mitte
    Ml = tr(xf + 0.005, 0.0, 1.05) @ rot(0, math.pi / 2, 0)
    lathe(k(F['brass_dark'], 40), [(0.0, 0.0), (0.03, 0.0), (0.028, 0.003), (0.0, 0.004)], 12, Ml)
    cube(k(F['interior']), (xf + 0.009, -0.003, 1.035), (xf + 0.0095, 0.003, 1.06))
    # Abplatzer: Seiten, Kranz, Sockel; Staub auf dem Deckel
    _box_chips(k, F, rng, (x0, -W / 2, 0.11), (x1, -W / 2 + 0.025, H - 0.06), I, '-y', 8, 0.03)
    for sgn in (-1, 1):
        lo_, hi_ = ((x0, -W / 2, 0.15), (x1, -W / 2 + 0.025, H - 0.1)) if sgn < 0 else ((x0, W / 2 - 0.025, 0.15), (x1, W / 2, H - 0.1))
        _box_chips(k, F, rng, lo_, hi_, I, '-y' if sgn < 0 else '+y', 4, 0.11, edge_bias=0.2, mats=('lac_worn', 'lac_worn', 'lac_dark'))
    _box_chips(k, F, rng, (x0, W / 2 - 0.025, 0.11), (x1, W / 2, H - 0.06), I, '+y', 8, 0.03)
    _box_chips(k, F, rng, (x0 - 0.02, -W / 2 - 0.03, H - 0.06), (x1 + 0.035, W / 2 + 0.03, H - 0.03), I, '+x', 5, 0.02)
    _box_chips(k, F, rng, (x0, -W / 2, 0.0), (x1, W / 2, 0.11), I, '+x -y +y', 3, 0.03, mats=('wood_dark', 'grime'))
    dust = material('staub', (0.46, 0.40, 0.36), look='fabric', rough=1.0, r=1.5)
    blob(k(dust, 30), (0.0, 0.1, H), (0, 0, 1), 0.16, 0.0015, rng, nseg=12, squash=0.7)
    k.finish()


def _leg(k, mat, x, y, z0, z1, w0, w1, splay=(0.0, 0.0), foot=False):
    """Vierkantbein, nach unten schlanker; foot: nach innen eingerollter Fuss (Pferdehuf)."""
    src = bmesh.new()
    ring0 = [src.verts.new((x + sx * w1 / 2 + splay[0], y + sy * w1 / 2 + splay[1], z0)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    ring1 = [src.verts.new((x + sx * w0 / 2, y + sy * w0 / 2, z1)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for i in range(4):
        j = (i + 1) % 4
        src.faces.new((ring0[i], ring0[j], ring1[j], ring1[i]))
    src.faces.new(list(reversed(ring0)))
    src.faces.new(ring1)
    bmesh.ops.recalc_face_normals(src, faces=list(src.faces))
    emit(src, lambda f: k(mat))
    if foot:
        dx = -1 if x > 0 else 1
        dy = -1 if y > 0 else 1
        blk(k(mat), (w1 * 1.25, w1 * 1.25, z0), (x + splay[0] + dx * w1 * 0.15, y + splay[1] + dy * w1 * 0.15, z0 * 0.5), bevel=0.006)


def build_table_vermilion():
    rng = random.Random(501)
    F = mats_fabled()
    k = Kit()
    L, W, H = 1.4, 0.8, 0.78
    I = Matrix.Identity(4)
    t = 0.04
    # Platte: Rahmen mit Fase, Mitte abgenutzt (matter), Ringe und Kratzer
    cube(k(F['lac']), (-L / 2, -W / 2, H - t), (L / 2, W / 2, H), bevel=0.008)
    for (cx, cy, rx, ry) in ((-0.32, 0.12, 0.16, 0.10), (0.25, -0.1, 0.2, 0.12), (0.05, 0.2, 0.1, 0.07), (0.48, 0.22, 0.08, 0.06)):
        worn = _jag(rng, cx, cy, rx, ry, n=9, rough=0.3)
        quad_n(k(F['lac_worn']), [V((x, y, H + 0.0004)) for (x, y) in worn], (0, 0, 1))
    for _ in range(3):
        c = V((rng.uniform(-0.5, 0.5), rng.uniform(-0.28, 0.28), H + 0.0008))
        rr = rng.uniform(0.032, 0.042)
        lathe(k(F['lac_dark'], 0), [(rr, 0.0), (rr + 0.003, 0.0)], 20, tr(*c))
    for _ in range(6):
        a = rng.uniform(0, math.pi)
        c = V((rng.uniform(-0.6, 0.6), rng.uniform(-0.33, 0.33), H + 0.0009))
        d = V((math.cos(a), math.sin(a), 0)) * rng.uniform(0.04, 0.12)
        nn = V((-math.sin(a), math.cos(a), 0)) * 0.0012
        quad_n(k(F['ground']), [c - d - nn, c + d - nn, c + d + nn, c - d + nn], (0, 0, 1))
    _box_chips(k, F, rng, (-L / 2, -W / 2, H - t), (L / 2, W / 2, H), I, '+z', 10, 0.025)
    _box_chips(k, F, rng, (-L / 2, -W / 2, H - t), (L / 2, W / 2, H), I, '+x -x +y -y', 2, 0.015)
    # Zarge (zurueckgesetzt), Beine mit Pferdehuf-Fuessen, Eckbaender (Spandrels)
    zg = 0.10
    for s in (-1, 1):
        cube(k(F['lac']), (-L / 2 + 0.06, s * (W / 2 - 0.06) - 0.012, H - t - zg), (L / 2 - 0.06, s * (W / 2 - 0.06) + 0.012, H - t),
             bevel=0.003)
        cube(k(F['lac']), (s * (L / 2 - 0.06) - 0.012, -W / 2 + 0.06, H - t - zg), (s * (L / 2 - 0.06) + 0.012, W / 2 - 0.06, H - t),
             bevel=0.003)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (L / 2 - 0.08), sy * (W / 2 - 0.08)
            _leg(k, F['lac'], x, y, 0.03, H - t, 0.065, 0.05, foot=True)
            _box_chips(k, F, rng, (x - 0.03, y - 0.03, 0.03), (x + 0.03, y + 0.03, 0.25), I, '+x -x +y -y', 1, 0.02,
                       mats=('wood_dark', 'wood'))
            # Eckband: Dreieck unter der Zarge, zur Mitte hin
            for (ax, ay) in ((1, 0), (0, 1)):
                p0 = V((x, y, H - t - zg))
                p1 = p0 + V((-sx * ax * 0.12, -sy * ay * 0.12, 0))
                p2 = p0 + V((0, 0, -0.10))
                nrm = V((ay * sy, ax * sx, 0))
                if nrm.length < 0.5:
                    continue
                for off in (0.006, -0.006):
                    q = [p + nrm * off for p in (p0, p1, p2)]
                    quad_n(k(F['lac']), q, nrm * (1 if off > 0 else -1))
    # Fussleiste (Steg) zwischen den Beinen, schwarz abgesetzt
    for sy in (-1, 1):
        cube(k(F['lac_black']), (-L / 2 + 0.1, sy * (W / 2 - 0.08) - 0.012, 0.12), (L / 2 - 0.1, sy * (W / 2 - 0.08) + 0.012, 0.15),
             bevel=0.003)
    k.finish()


def build_bench_vermilion():
    rng = random.Random(521)
    F = mats_fabled()
    k = Kit()
    L, W, H = 1.6, 0.38, 0.47
    I = Matrix.Identity(4)
    t = 0.055
    # Sitzbrett entlang y (die Bank steht an der Wand, Blick +x)
    cube(k(F['lac']), (-W / 2, -L / 2, H - t), (W / 2, L / 2, H), bevel=0.008)
    for (cy, ry) in ((-0.42, 0.18), (0.05, 0.14), (0.45, 0.16)):  # Sitzflaechen abgewetzt
        worn = _jag(rng, 0.01, cy, 0.09, ry, n=9, rough=0.25)
        quad_n(k(F['lac_worn']), [V((x, y, H + 0.0004)) for (x, y) in worn], (0, 0, 1))
    _box_chips(k, F, rng, (-W / 2, -L / 2, H - t), (W / 2, L / 2, H), I, '+z', 8, 0.025)
    _box_chips(k, F, rng, (-W / 2, -L / 2, H - t), (W / 2, L / 2, H), I, '+x -x', 3, 0.02)
    # gespreizte Beine, Zarge, Seitenstege
    for sy in (-1, 1):
        for sx in (-1, 1):
            x, y = sx * (W / 2 - 0.06), sy * (L / 2 - 0.12)
            _leg(k, F['lac'], x, y, 0.0, H - t, 0.055, 0.05, splay=(sx * 0.03, sy * 0.025))
            _box_chips(k, F, rng, (x - 0.03, y - 0.03, 0.0), (x + 0.03, y + 0.03, 0.2), I, '+x -x', 1, 0.018,
                       mats=('wood_dark', 'grime'))
        cube(k(F['lac']), (-W / 2 + 0.07, sy * (L / 2 - 0.12) - 0.012, 0.16), (W / 2 - 0.07, sy * (L / 2 - 0.12) + 0.012, 0.19),
             bevel=0.003)
    for sx in (-1, 1):
        cube(k(F['lac']), (sx * (W / 2 - 0.06) - 0.012, -L / 2 + 0.14, H - t - 0.07), (sx * (W / 2 - 0.06) + 0.012, L / 2 - 0.14, H - t),
             bevel=0.003)
    k.finish()


def build_screen_vermilion():
    """Paravent aus vier Fluegeln (je 0,5 x 1,8 m) im Zickzack; oben Gitterwerk, unten Fuellung, Messingscharniere."""
    rng = random.Random(541)
    F = mats_fabled()
    k = Kit()
    w, h = 0.5, 1.8
    ang = math.radians(28)
    # Fluegel i: Kante an Kante, abwechselnd gedreht (Draufsicht Zickzack entlang y)
    p = V((0.0, -2 * w * math.cos(ang), 0.0))
    for i in range(4):
        a = ang if i % 2 == 0 else -ang
        d = V((math.sin(a), math.cos(a), 0.0))
        M = Matrix((
            (d.y, d.x, 0, p.x),
            (-d.x, d.y, 0, p.y),
            (0, 0, 1, 0.0),
            (0, 0, 0, 1)))
        # lokale y von 0..w entlang d, lokale x = Dicke
        fr = k(F['lac'])
        tk = 0.028
        cube(fr, (-tk / 2, 0.0, 0.03), (tk / 2, 0.04, h), M, bevel=0.003)
        cube(fr, (-tk / 2, w - 0.04, 0.03), (tk / 2, w, h), M, bevel=0.003)
        for (z0, z1) in ((0.08, 0.13), (0.62, 0.67), (h - 0.05, h)):
            cube(fr, (-tk / 2, 0.04, z0), (tk / 2, w - 0.04, z1), M, bevel=0.003)
        for s in (0.04, w - 0.04):  # Fuesse
            cube(k(F['lac_dark']), (-0.03, s - 0.03, 0.0), (0.03, s + 0.03, 0.03), M, bevel=0.004)
        # Fuellung unten
        cube(k(F['lac_dark']), (-0.006, 0.04, 0.13), (0.006, w - 0.04, 0.62), M)
        for sgn in (1, -1):
            Mr = M if sgn > 0 else M @ rot(0, 0, math.pi) @ tr(0, -w, 0)
            _raised_panel(k, F['lac'], Mr, 0.06, w - 0.06, 0.15, 0.60, 0.006, 0.006, 0.03)
        # Gitterwerk oben: senkrechte und waagrechte Staebe, versetzt; im dritten Fluegel fehlen Staebe
        sl = k(F['lac'])
        zs = [0.67 + (h - 0.05 - 0.67) * j / 7 for j in range(1, 7)]
        ys = [0.04 + (w - 0.08) * j / 4 for j in range(1, 4)]
        for j, z in enumerate(zs):
            if i == 2 and j in (2, 3):
                continue
            cube(sl, (-0.008, 0.04, z - 0.007), (0.008, w - 0.04, z + 0.007), M)
        for j, y in enumerate(ys):
            if i == 2 and j == 1:
                # abgebrochener Stab: haengt schraeg
                cube(sl, (-0.008, y - 0.007, h - 0.25), (0.008, y + 0.007, h - 0.05), M)
                Mb = M @ tr(0.0, y, h - 0.25) @ rot(0.35, 0, 0)
                cube(sl, (-0.008, -0.007, -0.3), (0.008, 0.007, 0.0), Mb)
                continue
            cube(sl, (-0.008, y - 0.007, 0.67), (0.008, y + 0.007, h - 0.05), M)
        _box_chips(k, F, rng, (-tk / 2, 0.0, 0.03), (tk / 2, w, h), M, '+x -x', 4, 0.02)
        _box_chips(k, F, rng, (-0.0065, 0.04, 0.13), (0.0065, w - 0.04, 0.62), M, '+x -x', 1, 0.08, edge_bias=0.2,
                   mats=('lac_worn', 'grime'))
        # Scharniere zum naechsten Fluegel
        if i < 3:
            for zh in (0.3, 1.0, 1.6):
                cube(k(F['brass_dark']), (-tk / 2 - 0.002, w - 0.02, zh - 0.03), (tk / 2 + 0.002, w + 0.004, zh + 0.03), M)
        p = p + d * w
    k.finish()
