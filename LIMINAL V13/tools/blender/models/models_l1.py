"""V13.3 Level 1 (LOOK_1.md, LEVELSPEC_1.md): Blender-Modelle der Lagerdecks - Holzkisten, Haufen aus Holz, Verpackung
und Metall, farbige Stuetzenbaender, hohe Betonboegen und runde Saeulen (Gothic), Schwingtueren mit Sichtfenstern,
Notausgangsschild, Krankenbett, Polsterfelder der Gummizelle, Gluehbirne am Kabel, Arbeitstisch, Bilder.

Bodenmodelle stehen mit dem Ursprung am Boden; Wandmodelle haben die Wandflaeche bei x = 0 und blicken entlang +x;
Tuer- und Bogenmodelle stehen mit dem Ursprung in der Mitte des Durchbruchs am Boden (Laibung entlang x, 1 m tief);
Deckenmodelle haengen von z = 0 nach unten. Alle Teile entstehen als bmesh in lokalen Koordinaten und werden mit einer
Matrix in ein Netz mit mehreren Materialien (MB) uebernommen; das Spiel projiziert Texturen aus Weltkoordinaten.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

TAU = 2.0 * math.pi


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(a, axis):
    return Matrix.Rotation(a, 4, axis)


def frame(o, u, v, n):
    """Matrix mit den Spalten u, v, n (lokal x, y, z) und dem Ursprung o."""
    return Matrix(((u[0], v[0], n[0], o[0]), (u[1], v[1], n[1], o[1]), (u[2], v[2], n[2], o[2]), (0.0, 0.0, 0.0, 1.0)))


class MB:
    """Netz mit mehreren Materialien (Materialindex je Flaeche)."""

    def __init__(self):
        self.bm = bmesh.new()
        self.mats = []

    def idx(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def add(self, src, mat, M=None, smooth=None):
        """src (bmesh) uebernehmen; mat ein Material oder eine Liste (dann waehlt material_index der Quelle)."""
        mis = [self.idx(x) for x in mat] if isinstance(mat, (list, tuple)) else None
        mi = None if mis else self.idx(mat)
        flip = M is not None and M.to_3x3().determinant() < 0
        vmap = {}
        for v in src.verts:
            vmap[v] = self.bm.verts.new(M @ v.co if M is not None else v.co.copy())
        for f in src.faces:
            vs = [vmap[v] for v in f.verts]
            if flip:
                vs.reverse()
            try:
                nf = self.bm.faces.new(vs)
            except ValueError:
                continue
            nf.material_index = mis[min(f.material_index, len(mis) - 1)] if mis else mi
            nf.smooth = f.smooth if smooth is None else smooth
        src.free()

    def obj(self, name, sharp_deg=None, location=None):
        me = bpy.data.meshes.new(name)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(m)
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        if sharp_deg is not None:
            try:
                me.set_sharp_from_angle(angle=math.radians(sharp_deg))
            except AttributeError:
                pass
        if location is not None:
            ob.location = location
        return ob


# --- Grundkoerper (bmesh) ---------------------------------------------------------------------------------------------
def b_box(lo, hi):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(tuple(lo[i] + (v.co[i] + 0.5) * (hi[i] - lo[i]) for i in range(3)))
    return bm


def chamfer(bm, w, edges=None, seg=1):
    es = list(edges) if edges is not None else bm.edges[:]
    if w > 0 and es:
        bmesh.ops.bevel(bm, geom=es, offset=w, offset_type='OFFSET', segments=seg, profile=0.5, affect='EDGES',
                        clamp_overlap=True)
    return bm


def area2(poly):
    return 0.5 * sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1] for i in range(len(poly)))


def ccw(poly):
    return list(poly) if area2(poly) >= 0 else list(reversed(poly))


def b_prism(poly, z0, z1, ch=0.0):
    """2D-Polygon [(x, y)] von z0 bis z1 extrudieren (geschlossen); ch: Fase an der Oberseite."""
    poly = ccw(poly)
    bm = bmesh.new()
    a = [bm.verts.new((x, y, z0)) for x, y in poly]
    b = [bm.verts.new((x, y, z1)) for x, y in poly]
    bm.faces.new(list(reversed(a)))
    top = bm.faces.new(b)
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((a[i], a[j], b[j], b[i]))
    if ch > 0:
        chamfer(bm, ch, top.edges[:])
    return bm


def b_plate(poly, z=0.0):
    """Einseitige Flaeche (Normale +z) aus einem 2D-Polygon."""
    poly = ccw(clean_poly(poly))
    bm = bmesh.new()
    if len(poly) >= 3:
        bm.faces.new([bm.verts.new((x, y, z)) for x, y in poly])
    return bm


def b_lathe(prof, seg, a0=0.0, a1=TAU, smooth=True):
    """Drehkoerper um z: prof = [(r, z), ...] so durchlaufen, dass das Material links liegt
    (von der Achse unten ueber aussen nach oben). r = 0 ergibt einen Pol (Dreiecksfaecher)."""
    full = abs((a1 - a0) - TAU) < 1e-6
    nv = seg if full else seg + 1
    angs = [a0 + (a1 - a0) * k / seg for k in range(nv)]
    bm = bmesh.new()
    rings = []
    for (r, z) in prof:
        if r < 1e-7:
            rings.append(bm.verts.new((0.0, 0.0, z)))
        else:
            rings.append([bm.verts.new((r * math.cos(a), r * math.sin(a), z)) for a in angs])
    for i in range(len(prof) - 1):
        A, B = rings[i], rings[i + 1]
        for k in range(seg):
            k2 = (k + 1) % nv
            if isinstance(A, list) and isinstance(B, list):
                vs = (A[k], A[k2], B[k2], B[k])
            elif isinstance(B, list):
                vs = (A, B[k2], B[k])
            elif isinstance(A, list):
                vs = (A[k], A[k2], B)
            else:
                continue
            f = bm.faces.new(vs)
            f.smooth = smooth
    return bm


def b_cyl(r, z0, z1, seg=12, caps=True, r1=None, smooth=True):
    r1 = r if r1 is None else r1
    prof = ([(0.0, z0)] if caps else []) + [(r, z0), (r1, z1)] + ([(0.0, z1)] if caps else [])
    bm = b_lathe(prof, seg, smooth=smooth)
    if caps:  # Deckel flach schattieren
        for f in bm.faces:
            if abs(f.normal.z) > 0.99 or len(f.verts) == 3:
                f.smooth = False
    return bm


def b_tube(pts, r, seg=8, caps=True, closed=False, smooth=True, twist=0.0):
    """Rohr entlang einer Polylinie (paralleler Transport des Rahmens); r Zahl oder Liste je Punkt."""
    pts = [Vector(p) for p in pts]
    n = len(pts)
    tans = []
    for i in range(n):
        if closed:
            t = pts[(i + 1) % n] - pts[i - 1]
        else:
            t = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
        tans.append(t.normalized())
    t0 = tans[0]
    ref = Vector((0, 0, 1)) if abs(t0.z) < 0.9 else Vector((1, 0, 0))
    nrm = (ref - t0 * ref.dot(t0)).normalized()
    bm = bmesh.new()
    rings = []
    for i in range(n):
        if i > 0:
            ax = tans[i - 1].cross(tans[i])
            if ax.length > 1e-9:
                nrm = Matrix.Rotation(tans[i - 1].angle(tans[i]), 3, ax.normalized()) @ nrm
        b = tans[i].cross(nrm)
        rr = r[i] if isinstance(r, (list, tuple)) else r
        # Spitze Knicke: Ring um 1/cos(halber Knickwinkel) aufweiten wuerde verzerren - Punkte dicht genug legen
        rings.append([bm.verts.new(pts[i] + (nrm * math.cos(a + twist * i) + b * math.sin(a + twist * i)) * rr)
                      for a in [TAU * k / seg for k in range(seg)]])
    m = n if closed else n - 1
    for i in range(m):
        A, B = rings[i], rings[(i + 1) % n]
        for k in range(seg):
            k2 = (k + 1) % seg
            f = bm.faces.new((A[k], A[k2], B[k2], B[k]))
            f.smooth = smooth
    if caps and not closed:
        f = bm.faces.new(list(reversed(rings[0])))
        f.smooth = False
        f = bm.faces.new(rings[-1])
        f.smooth = False
    return bm


def b_grid(P, smooth=True):
    """Flaeche aus einem Punktraster P[i][j]; Normale = du x dv."""
    bm = bmesh.new()
    V = [[bm.verts.new(p) for p in row] for row in P]
    for i in range(len(P) - 1):
        for j in range(len(P[0]) - 1):
            f = bm.faces.new((V[i][j], V[i + 1][j], V[i + 1][j + 1], V[i][j + 1]))
            f.smooth = smooth
    return bm


def b_nail(r=0.0042, h=0.0013, seg=6):
    """Nagelkopf: flache Kuppe, unten offen (steckt im Holz)."""
    return b_lathe([(r, -0.0004), (0.0, h)], seg, smooth=True)


def jitter(bm, amt, rng, axes=(1, 1, 1)):
    for v in bm.verts:
        v.co.x += rng.uniform(-amt, amt) * axes[0]
        v.co.y += rng.uniform(-amt, amt) * axes[1]
        v.co.z += rng.uniform(-amt, amt) * axes[2]
    return bm


def clip_poly(poly, a, b, c):
    """Konvexes/beliebiges Polygon an der Halbebene a*x + b*y <= c abschneiden (Sutherland-Hodgman)."""
    out = []
    n = len(poly)
    for i in range(n):
        P, Q = poly[i], poly[(i + 1) % n]
        dp, dq = a * P[0] + b * P[1] - c, a * Q[0] + b * Q[1] - c
        if dp <= 0:
            out.append(P)
        if (dp < 0 < dq) or (dq < 0 < dp):
            t = dp / (dp - dq)
            out.append((P[0] + (Q[0] - P[0]) * t, P[1] + (Q[1] - P[1]) * t))
    return out


def clean_poly(poly, eps=1e-5):
    """Doppelte und kollineare Punkte entfernen (sonst entartete Dreiecke)."""
    pts = [tuple(p) for p in poly]
    changed = True
    while changed and len(pts) >= 3:
        changed = False
        out = []
        n = len(pts)
        for i in range(n):
            P, Q, S = pts[i - 1], pts[i], pts[(i + 1) % n]
            if math.hypot(Q[0] - P[0], Q[1] - P[1]) < eps:
                changed = True
                continue
            cr = (Q[0] - P[0]) * (S[1] - Q[1]) - (Q[1] - P[1]) * (S[0] - Q[0])
            if abs(cr) < 1e-9:
                changed = True
                continue
            out.append(Q)
        pts = out
    return pts


def clip_rect(poly, x0, y0, x1, y1):
    for (a, b, c) in ((-1, 0, -x0), (1, 0, x1), (0, -1, -y0), (0, 1, y1)):
        poly = clip_poly(poly, a, b, c)
        if len(poly) < 3:
            return []
    return poly


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def rot2(poly, a, cx=0.0, cy=0.0):
    c, s = math.cos(a), math.sin(a)
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for (x, y) in poly]


def circle(cx, cy, r, n=12, rx=None, a0=0.0):
    rx = r if rx is None else rx
    return [(cx + rx * math.cos(a0 + TAU * k / n), cy + r * math.sin(a0 + TAU * k / n)) for k in range(n)]


def stroke(pts, w):
    """Strich (Polylinie) der Breite w als Polygon (fuer Piktogramme, Malerei)."""
    left, right = [], []
    n = len(pts)
    for i in range(n):
        p = Vector((pts[i][0], pts[i][1]))
        if i == 0:
            d = Vector(pts[1]) - Vector(pts[0])
        elif i == n - 1:
            d = Vector(pts[-1]) - Vector(pts[-2])
        else:
            d = (Vector(pts[i + 1]) - Vector(pts[i])).normalized() + (Vector(pts[i]) - Vector(pts[i - 1])).normalized()
        d = d.normalized()
        nn = Vector((-d.y, d.x))
        left.append(tuple(p + nn * w / 2))
        right.append(tuple(p - nn * w / 2))
    return left + list(reversed(right))


SEG7 = {'0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc', '7': 'abc',
        '8': 'abcdefg', '9': 'abcdfg'}


def digit_polys(ch, x, y, h):
    """Schablonenziffer (Sieben-Segment-Art, Stege an den Enden offen) mit linker unterer Ecke (x, y)."""
    w, s, g = 0.56 * h, 0.15 * h, 0.035 * h
    out = []
    segs = {'a': rect(x + s * 0.6 + g, y + h - s, x + w - s * 0.6 - g, y + h),
            'g': rect(x + s * 0.6 + g, y + h / 2 - s / 2, x + w - s * 0.6 - g, y + h / 2 + s / 2),
            'd': rect(x + s * 0.6 + g, y, x + w - s * 0.6 - g, y + s),
            'f': rect(x, y + h / 2 + g, x + s, y + h - g),
            'b': rect(x + w - s, y + h / 2 + g, x + w, y + h - g),
            'e': rect(x, y + g, x + s, y + h / 2 - g),
            'c': rect(x + w - s, y + g, x + w, y + h / 2 - g)}
    for k in SEG7.get(ch, ''):
        out.append(segs[k])
    return out


# --- Physik: Teile fallen lassen, bis sie liegen (Haufen) ----------------------------------------------------------
def settle(objs, passive=(), frames=110, ground=0.0, masses=None, friction=0.95):
    """Starrkoerper: objs fallen auf den Boden und auf passive (bereits liegende Teile); danach sind die Lagen fest."""
    scn = bpy.context.scene
    bpy.ops.rigidbody.world_add()
    rbw = scn.rigidbody_world
    rbw.substeps_per_frame = 40
    rbw.solver_iterations = 50
    rbw.point_cache.frame_start = 1
    rbw.point_cache.frame_end = frames + 1
    me = bpy.data.meshes.new('boden')
    bm = b_box((-6, -6, -0.25), (6, 6, 0.25))
    bm.to_mesh(me)
    bm.free()
    gob = bpy.data.objects.new('boden', me)
    gob.location = (0, 0, ground - 0.25)
    scn.collection.objects.link(gob)
    for ob in list(objs) + list(passive) + [gob]:
        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        bpy.ops.rigidbody.object_add()
        rb = ob.rigid_body
        act = ob in objs
        rb.type = 'ACTIVE' if act else 'PASSIVE'
        rb.collision_shape = 'BOX' if ob is gob else 'CONVEX_HULL'
        rb.friction = friction
        rb.restitution = 0.0
        rb.use_margin = True
        rb.collision_margin = 0.0006
        rb.linear_damping = 0.6
        rb.angular_damping = 0.7
        if act and masses:
            rb.mass = masses.get(ob.name, 1.0)
    for f in range(1, frames + 1):
        scn.frame_set(f)
    dg = bpy.context.evaluated_depsgraph_get()
    mws = {ob.name: ob.evaluated_get(dg).matrix_world.copy() for ob in objs}
    bpy.ops.object.select_all(action='DESELECT')
    for ob in list(objs) + list(passive) + [gob]:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = gob
    bpy.ops.rigidbody.objects_remove()
    bpy.ops.rigidbody.world_remove()
    scn.frame_set(1)
    for ob in objs:
        ob.matrix_world = mws[ob.name]
    bpy.data.objects.remove(gob, do_unlink=True)


def obj_bounds(obs):
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for ob in obs:
        for v in ob.data.vertices:
            p = ob.matrix_world @ v.co
            lo = Vector(map(min, lo, p))
            hi = Vector(map(max, hi, p))
    return lo, hi


def scene_bounds():
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    dg = bpy.context.evaluated_depsgraph_get()
    for ob in bpy.context.scene.objects:
        if ob.type != 'MESH':
            continue
        me = ob.evaluated_get(dg).to_mesh()
        for v in me.vertices:
            p = ob.matrix_world @ v.co
            lo = Vector(map(min, lo, p))
            hi = Vector(map(max, hi, p))
    return lo, hi


def rng(seed):
    return random.Random(seed)



# =====================================================================================================================
# Kisten (Wiki Level 1: schlichte Holzkisten, am Boden oder auf Regalen)
# =====================================================================================================================
def _crate_mats():
    m = common.material
    return dict(
        hell=m('kiste_hell', (0.72, 0.62, 0.48), look='wood', r=1.15),
        mittel=m('kiste_mittel', (0.63, 0.54, 0.42), look='wood', r=1.2),
        grau=m('kiste_grau', (0.56, 0.53, 0.47), look='wood', r=1.35),
        dunkel=m('kiste_schmutz', (0.43, 0.38, 0.31), look='wood', r=1.4),
        leiste=m('kiste_leiste', (0.60, 0.50, 0.38), look='wood', r=1.2),
        leiste_grau=m('kiste_leiste_grau', (0.50, 0.47, 0.42), look='wood', r=1.35),
        innen=m('kiste_innen', (0.10, 0.085, 0.07), look='wood', r=1.5),
        ast=m('kiste_ast', (0.30, 0.20, 0.12), look='wood', r=1.0),
        nagel=m('nagel', (0.33, 0.32, 0.31), look='metal', r=0.9, m=0.7),
        ecke=m('kantenschutz', (0.57, 0.58, 0.57), look='galvanized', r=1.0, m=0.6),
        rost=m('rost', (0.38, 0.22, 0.12), look='metal', r=1.45, m=0.1),
        farbe=m('schablone', (0.06, 0.06, 0.06), look='paint', r=1.3),
        papier=m('etikett', (0.86, 0.83, 0.74), look='cardboard', r=1.2),
        druck=m('druck', (0.15, 0.15, 0.17), look='paint', r=1.0),
        band=m('stahlband', (0.11, 0.12, 0.14), look='metal', r=0.85, m=0.55),
    )


def _crate(L, W, H, seed, style):
    """Kiste L (x) x W (y) x H aus 16-mm-Brettern (waagrecht, 3 mm Fugen), aussen ein Rahmen aus Leisten 20 x 65 mm,
    Deckel aus Laengsbrettern (oben flach, damit Kisten gestapelt werden koennen), Naegel an jeder Kreuzung.
    style: 'a' hell, ohne Beschlag | 'b' verwittert, Eckbeschlaege, Etikett, Nummer | 'c' gross, Kufen, Diagonalen,
    Stahlbaender, Eckbeschlaege, Pfeile."""
    rg = rng(seed)
    M = _crate_mats()
    mb = MB()
    t, gap, bt, bw = 0.016, 0.003, 0.02, 0.065
    skid = 0.075 if style == 'c' else 0.0
    z0 = skid
    tones = {'a': ['hell', 'hell', 'mittel'], 'b': ['grau', 'grau', 'mittel', 'dunkel'], 'c': ['mittel', 'hell', 'grau']}[style]
    lmat = M['leiste_grau'] if style == 'b' else M['leiste']
    sides = [  # (Normale, u-Achse, Ursprung, Breite der Beplankung, Rahmenbreite)
        (Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((L / 2, 0, 0)), W - 2 * t, W),
        (Vector((-1, 0, 0)), Vector((0, -1, 0)), Vector((-L / 2, 0, 0)), W - 2 * t, W),
        (Vector((0, 1, 0)), Vector((-1, 0, 0)), Vector((0, W / 2, 0)), L, L + 2 * bt),
        (Vector((0, -1, 0)), Vector((1, 0, 0)), Vector((0, -W / 2, 0)), L, L + 2 * bt),
    ]
    z = Vector((0, 0, 1))
    face_boards = []
    for si, (n, u, o, fw, frw) in enumerate(sides):
        F = frame(o, u, z, n)
        # Bretter der Beplankung (v = Hoehe), zufaellige Breiten
        boards = []
        v = z0
        top = H - t
        while v < top - 0.02:
            h = rg.uniform(0.095, 0.145)
            if top - (v + h) < 0.07:
                h = top - v
            boards.append((v, v + h))
            v += h + gap
        rows = []
        for bi, (v0, v1) in enumerate(boards):
            v1 = min(v1, top)
            dn = rg.uniform(-0.0007, 0.0007)
            poly = rect(-fw / 2 + rg.uniform(0, 0.002), v0, fw / 2 - rg.uniform(0, 0.002), v1)
            if rg.random() < 0.22:  # abgeplatzte Brettecke
                k = rg.randrange(4)
                cx, cy = poly[k]
                d1, d2 = rg.uniform(0.012, 0.03), rg.uniform(0.006, 0.02)
                sx = 1 if k in (0, 3) else -1
                sy = 1 if k in (0, 1) else -1
                poly = poly[:k] + [(cx, cy + sy * d2), (cx + sx * d1, cy)] + poly[k + 1:] if k in (0, 2) else \
                    poly[:k] + [(cx + sx * d1, cy), (cx, cy + sy * d2)] + poly[k + 1:]
            poly = rot2(poly, rg.uniform(-0.003, 0.003), 0.0, (v0 + v1) / 2)
            mat = M['dunkel'] if (bi == 0 and style != 'a') else M[rg.choice(tones)]
            mb.add(b_prism(poly, -t + dn, dn, ch=0.0016), mat, F)
            rows.append((v0, v1, dn))
        face_boards.append(rows)
        # dunkles Inneres hinter den Fugen
        mb.add(b_plate(rect(-fw / 2, z0, fw / 2, H - t), -t - 0.001), M['innen'], F)
        # Rahmen: senkrechte Leisten an den Enden, waagrechte oben/unten dazwischen
        e = frw / 2
        frame_rects = [rect(-e, z0, -e + bw, H), rect(e - bw, z0, e, H),
                       rect(-e + bw, z0, e - bw, z0 + bw), rect(-e + bw, H - bw, e - bw, H)]
        if style == 'c' and si >= 2:  # Diagonale im Feld
            x0, x1, y0, y1 = -e + bw, e - bw, z0 + bw, H - bw
            P0, P1 = Vector((x0, y0)), Vector((x1, y1))
            d = (P1 - P0).normalized()
            nn = Vector((-d.y, d.x))
            band = [(x0, y0), (x1, y1)]
            poly = rect(x0, y0, x1, y1)
            c0 = nn.dot(P0)
            poly = clip_poly(poly, nn.x, nn.y, c0 + bw * 0.45)
            poly = clip_poly(poly, -nn.x, -nn.y, -(c0 - bw * 0.45))
            frame_rects.append(poly)
            del band
        if style == 'c' and si < 2:  # Griffleiste an den Stirnseiten (steht weiter vor, unten gefast)
            zc = H - 0.2
            gl = [(-e + bw, zc - 0.03), (e - bw, zc - 0.03), (e - bw, zc + 0.035), (-e + bw, zc + 0.035)]
            mb.add(b_prism(gl, 0.0, bt + 0.014, ch=0.006), lmat, F)
            for uc in (-e + bw + 0.05, 0.0, e - bw - 0.05):
                mb.add(b_nail(), M['nagel'], F @ T(uc, zc + 0.01, bt + 0.014))
        for k, pr in enumerate(frame_rects):
            mat = M['dunkel'] if (k == 2 and style != 'a') else lmat
            pr = rot2(pr, rg.uniform(-0.002, 0.002), sum(p[0] for p in pr) / len(pr), sum(p[1] for p in pr) / len(pr))
            mb.add(b_prism(pr, 0.0, bt + rg.uniform(-0.0006, 0.0006), ch=0.0025), mat, F)
        # Naegel: senkrechte Leisten je Brett, waagrechte alle ~15 cm
        nails = []
        for (v0, v1, dn) in rows:
            vc = (v0 + v1) / 2
            for uc in (-e + bw / 2, e - bw / 2):
                nails.append((uc + rg.uniform(-0.006, 0.006), vc + rg.uniform(-0.012, 0.012)))
        for vc in (z0 + bw / 2, H - bw / 2):
            cnt = max(2, int((2 * e - 2 * bw) / 0.16))
            for i in range(cnt):
                uc = -e + bw + (2 * e - 2 * bw) * (i + 0.5) / cnt
                nails.append((uc + rg.uniform(-0.01, 0.01), vc + rg.uniform(-0.007, 0.007)))
        if style == 'c' and si >= 2:
            for (v0, v1, dn) in rows:
                vc = (v0 + v1) / 2
                if z0 + bw < vc < H - bw:
                    uc = (-e + bw) + (vc - (z0 + bw)) / ((H - bw) - (z0 + bw)) * (2 * e - 2 * bw)
                    nails.append((uc, vc))
        for (uc, vc) in nails:
            nm = M['rost'] if rg.random() < 0.12 else M['nagel']
            mb.add(b_nail(), nm, F @ T(uc, vc, bt) @ R(rg.uniform(0, 1), 'Z'))
            if nm is M['rost'] and rg.random() < 0.6:  # Rostfahne unter dem Nagel
                ln = min(rg.uniform(0.02, 0.06), vc - z0 - 0.004)
                mb.add(b_plate([(uc - 0.003, vc - 0.004), (uc + 0.003, vc - 0.004), (uc + 0.0012, vc - ln), (uc - 0.0012, vc - ln)],
                               bt + 0.0004), M['rost'], F)
        # Aeste (dunkle Flecken) auf einzelnen Brettern
        for _ in range(rg.randint(1, 3)):
            v0, v1, dn = rg.choice(rows)
            if v1 - v0 < 0.05:
                continue
            uc = rg.uniform(-e + bw + 0.04, e - bw - 0.04)
            r_ = rg.uniform(0.007, 0.014)
            mb.add(b_plate(circle(uc, (v0 + v1) / 2 + rg.uniform(-0.01, 0.01), r_, 8, rx=r_ * rg.uniform(1.2, 2.0)), dn + 0.0003),
                   M['ast'], F)
    # Deckel: Laengsbretter, oben bei H
    v = -W / 2
    lid = []
    while v < W / 2 - 0.02:
        h = rg.uniform(0.095, 0.15)
        if W / 2 - (v + h) < 0.07:
            h = W / 2 - v
        lid.append((v, v + h))
        v += h + gap
    for (y0, y1) in lid:
        dz = rg.uniform(-0.0012, 0.0)
        poly = rot2(rect(-L / 2 + rg.uniform(0, 0.002), y0, L / 2 - rg.uniform(0, 0.002), min(y1, W / 2)), rg.uniform(-0.002, 0.002),
                    0, (y0 + y1) / 2)
        mb.add(b_prism(poly, H - t + dz, H + dz, ch=0.0016), M[rg.choice(tones)], None)
        for sx in (-1, 1):  # Deckelnaegel in die Stirnbretter
            for yy in (y0 + (y1 - y0) * 0.28, y0 + (y1 - y0) * 0.72):
                mb.add(b_nail(), M['nagel'], T(sx * (L / 2 - 0.009), yy, H + dz) @ R(rg.uniform(0, 1), 'Z'))
    for _ in range(rg.randint(1, 2)):
        y0, y1 = rg.choice(lid)
        r_ = rg.uniform(0.008, 0.014)
        mb.add(b_plate(circle(rg.uniform(-L / 3, L / 3), (y0 + y1) / 2, r_, 8, rx=r_ * rg.uniform(1.3, 2.2)), H + 0.0003), M['ast'], None)
    # Boden (dunkel, von unten kaum sichtbar)
    mb.add(b_box((-L / 2, -W / 2, z0), (L / 2, W / 2, z0 + t)), M['dunkel'], None)
    # Kufen (nur c): mit angeschraegten Enden
    if skid > 0:
        for sy in (-1, 1):
            yc = sy * (W / 2 - 0.11)
            prof = [(-L / 2, 0.035), (-L / 2 + 0.045, 0.0), (L / 2 - 0.045, 0.0), (L / 2, 0.035), (L / 2, skid), (-L / 2, skid)]
            bm = b_prism(prof, -0.04, 0.04, ch=0.002)
            mb.add(bm, M['dunkel'], T(0, yc, 0) @ R(math.pi / 2, 'X'))
    # Eckbeschlaege (b, c): drei gekantete Bleche je Ecke, je ein Nagel
    if style in ('b', 'c'):
        X, Y = L / 2 + bt, W / 2 + bt
        s = 0.072
        for sx in (-1, 1):
            for sy in (-1, 1):
                for top in (False, True):
                    zc = H if top else z0
                    sz = 1 if top else -1
                    shape = [(0, 0), (s, 0), (s, s * 0.55), (s * 0.55, s), (0, s)]
                    # Bleche auf den beiden Seitenflaechen und oben
                    Fx = frame(Vector((sx * X, sy * Y, zc)), Vector((0, -sy, 0)), Vector((0, 0, -sz)), Vector((sx, 0, 0)))
                    Fy = frame(Vector((sx * X, sy * Y, zc)), Vector((-sx, 0, 0)), Vector((0, 0, -sz)), Vector((0, sy, 0)))
                    plates = [Fx, Fy]
                    if top:
                        plates.append(frame(Vector((sx * X, sy * Y, zc)), Vector((-sx, 0, 0)), Vector((0, -sy, 0)), Vector((0, 0, 1))))
                    for Fp in plates:
                        mb.add(b_prism(shape, 0.0002, 0.0014), M['ecke'], Fp)
                        mb.add(b_nail(0.0035, 0.0011), M['nagel'], Fp @ T(s * 0.32, s * 0.32, 0.0014))
                        if rg.random() < 0.35:
                            mb.add(b_plate(circle(s * rg.uniform(0.3, 0.6), s * rg.uniform(0.15, 0.4), s * 0.12, 7,
                                                  rx=s * 0.2), 0.0015), M['rost'], Fp)
    # Stahlbaender (c): zwei Baender um die Kiste (quer), Plombe oben
    if style == 'c':
        Y, Ht = W / 2 + bt + 0.0006, H + 0.0008
        for xc in (-L * 0.3, L * 0.3):
            w2 = 0.0095
            mb.add(b_box((xc - w2, -Y, Ht - 0.0008), (xc + w2, Y, Ht)), M['band'], None)
            for sy in (-1, 1):
                mb.add(b_box((xc - w2, min(sy * Y, sy * (Y - 0.0008)), z0 + 0.01), (xc + w2, max(sy * Y, sy * (Y - 0.0008)), Ht)),
                       M['band'], None)
            mb.add(b_box((xc - 0.016, -0.03, Ht), (xc + 0.016, 0.01, Ht + 0.0035)), M['band'], None)
            mb.add(b_box((xc - 0.012, 0.012, Ht), (xc + 0.012, 0.03, Ht + 0.002)), M['rost'], None)
    # Kennzeichnung: Pfeile "oben" (a, c), Nummer und Etikett (b)
    def on_boards(si, polys, mat, lift=0.0004):
        n, u, o, fw, frw = sides[si]
        F = frame(o, u, z, n)
        for (v0, v1, dn) in face_boards[si]:
            for p in polys:
                q = clip_rect(p, -fw / 2, v0, fw / 2, v1)
                if len(q) >= 3:
                    mb.add(b_plate(q, dn + lift), mat, F)

    def arrow(cx, cy, h):
        w = h * 0.36
        return [rect(cx - w * 0.16, cy - h / 2, cx + w * 0.16, cy + h * 0.1),
                [(cx - w / 2, cy + h * 0.14), (cx + w / 2, cy + h * 0.14), (cx, cy + h / 2)]]
    if style == 'a':
        h = 0.11
        on_boards(0, arrow(-0.06, z0 + H * 0.5, h) + arrow(0.06, z0 + H * 0.5, h), M['farbe'])
    if style == 'c':
        h = 0.12
        on_boards(0, arrow(-0.07, z0 + (H - z0) * 0.55, h) + arrow(0.07, z0 + (H - z0) * 0.55, h), M['farbe'])
        polys = []
        for i, ch in enumerate('27'):
            polys += digit_polys(ch, -L / 2 + 0.13 + i * 0.075, H - 0.24, 0.11)
        on_boards(3, polys, M['farbe'])
    if style == 'b':
        polys = []
        for i, ch in enumerate('07'):
            polys += digit_polys(ch, -0.11 + i * 0.075, z0 + 0.2, 0.12)
        on_boards(3, polys, M['farbe'])
        # Etikett auf einem Brett der +y-Seite
        n, u, o, fw, frw = sides[2]
        F = frame(o, u, z, n)
        rows = face_boards[2]
        v0, v1, dn = max(rows[1:-1] or rows, key=lambda r_: r_[1] - r_[0])
        vc = (v0 + v1) / 2
        lw, lh = 0.13, min(0.085, (v1 - v0) - 0.012)
        uc = 0.12
        lab = [(uc - lw / 2, vc - lh / 2), (uc + lw / 2 - 0.025, vc - lh / 2), (uc + lw / 2, vc - lh / 2 + 0.018),
               (uc + lw / 2, vc + lh / 2), (uc - lw / 2, vc + lh / 2)]
        mb.add(b_plate(lab, dn + 0.0004), M['papier'], F)
        for i, frac in enumerate((0.8, 0.55, 0.7, 0.4)):
            yy = vc + lh / 2 - 0.012 - i * 0.011
            mb.add(b_plate(rect(uc - lw / 2 + 0.008, yy - 0.0022, uc - lw / 2 + 0.008 + (lw - 0.016) * frac, yy + 0.0022),
                           dn + 0.0007), M['druck'], F)
        bx = uc - lw / 2 + 0.008
        for k in range(14):
            wbar = 0.0012 if k % 3 else 0.0024
            mb.add(b_plate(rect(bx, vc - lh / 2 + 0.006, bx + wbar, vc - lh / 2 + 0.022), dn + 0.0007), M['druck'], F)
            bx += wbar + 0.0016
    mb.obj('kiste')


def build_crate_a():
    _crate(0.6, 0.6, 0.6, 1, 'a')


def build_crate_b():
    _crate(0.9, 0.6, 0.55, 2, 'b')


def build_crate_c():
    _crate(1.1, 0.8, 0.75, 3, 'c')


# =====================================================================================================================
# Haufen aus Holz, Verpackung und Metall (Wiki: Hallen); die Teile fallen in Blender (Starrkoerper) aufeinander
# =====================================================================================================================
def _debris_mats():
    m = common.material
    return dict(
        brett=m('brett', (0.60, 0.51, 0.40), look='wood', r=1.2),
        brett_alt=m('brett_alt', (0.46, 0.41, 0.35), look='wood', r=1.35),
        brett_grau=m('brett_grau', (0.52, 0.50, 0.46), look='wood', r=1.4),
        bruch=m('bruch', (0.78, 0.66, 0.48), look='wood', r=1.3),
        karton=m('karton', (0.62, 0.49, 0.33), look='cardboard', r=1.3),
        karton_nass=m('karton_nass', (0.45, 0.35, 0.24), look='cardboard', r=1.1),
        klebeband=m('klebeband', (0.66, 0.53, 0.32), look='plastic', r=0.5),
        blech=m('blech', (0.55, 0.56, 0.56), look='galvanized', r=1.0, m=0.6),
        rost=m('rost', (0.38, 0.22, 0.12), look='metal', r=1.45, m=0.1),
        rost_hell=m('rost_hell', (0.50, 0.30, 0.16), look='metal', r=1.5, m=0.05),
        stahl=m('stahl_dunkel', (0.25, 0.25, 0.26), look='metal', r=0.9, m=0.6),
        nagel=m('nagel', (0.33, 0.32, 0.31), look='metal', r=0.9, m=0.7),
        folie=m('folie', (0.78, 0.80, 0.80), look='plastic', r=0.45),
        band=m('pp_band', (0.18, 0.30, 0.55), look='plastic', r=0.6),
    )


def _plank_bm(L, W, Tk, rg, broken=(False, True)):
    """Brett entlang x (0..L), y quer (-W/2..W/2), z 0..Tk; gebrochene Enden als Splitter (Zickzack)."""
    K = 5
    ys = [-W / 2 + W * k / K for k in range(K + 1)]

    def ends(b, base):
        if not b:
            return [base] * (K + 1), [base] * (K + 1)
        top = [base + (rg.uniform(0.0, 0.07) if k % 2 else rg.uniform(-0.03, 0.01)) for k in range(K + 1)]
        bot = [x + rg.uniform(-0.025, 0.015) for x in top]
        return top, bot
    s = -1.0
    t0, b0 = ends(broken[0], 0.0)
    t1, b1 = ends(broken[1], L)
    t0 = [-x for x in t0] if broken[0] else t0
    b0 = [-x for x in b0] if broken[0] else b0
    t0 = [min(x, 0.0) if broken[0] else x for x in t0]
    del s
    bm = bmesh.new()
    T0 = [bm.verts.new((t0[k], ys[k], Tk)) for k in range(K + 1)]
    T1 = [bm.verts.new((t1[k], ys[k], Tk)) for k in range(K + 1)]
    B0 = [bm.verts.new((b0[k], ys[k], 0.0)) for k in range(K + 1)]
    B1 = [bm.verts.new((b1[k], ys[k], 0.0)) for k in range(K + 1)]
    for k in range(K):
        bm.faces.new((T0[k], T1[k], T1[k + 1], T0[k + 1]))   # oben
        bm.faces.new((B0[k + 1], B1[k + 1], B1[k], B0[k]))   # unten
        bm.faces.new((T1[k], B1[k], B1[k + 1], T1[k + 1]))   # Stirn bei L
        bm.faces.new((T0[k + 1], B0[k + 1], B0[k], T0[k]))   # Stirn bei 0
    bm.faces.new((T0[0], B0[0], B1[0], T1[0]))               # Seite -y
    bm.faces.new((T1[K], B1[K], B0[K], T0[K]))               # Seite +y
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm, (min(t0 + b0), max(t1 + b1))


class _Pile:
    def __init__(self, seed):
        self.rg = rng(seed)
        self.M = _debris_mats()
        self.items = []
        self.batch = []
        self.want = {}
        self.masses = {}

    def item(self, name, mb, mass, pos, rot):
        c = Vector((0, 0, 0))
        for v in mb.bm.verts:
            c += v.co
        c /= max(1, len(mb.bm.verts))
        for v in mb.bm.verts:
            v.co -= c
        ob = mb.obj(name)
        ob.rotation_euler = rot
        ob.location = (0, 0, 0)
        bpy.context.view_layer.update()
        lo, hi = obj_bounds([ob])
        # auf den Haufen legen: Unterkante knapp ueber der hoechsten Stelle unter dem Teil
        x, y = pos[0], pos[1]
        top = 0.0
        for i in range(7):
            for k in range(7):
                dx = lo.x - 0.03 + (hi.x - lo.x + 0.06) * i / 6
                dy = lo.y - 0.03 + (hi.y - lo.y + 0.06) * k / 6
                top = max(top, _surface_z(x + dx, y + dy, exclude=ob))
        ob.location = (x, y, top - lo.z + 0.02 + pos[2])
        self.want[ob.name] = (x, y)
        bpy.context.view_layer.update()
        self.batch.append(ob)
        self.masses[ob.name] = mass
        return ob

    def drop(self):
        """Die Teile des aktuellen Durchgangs fallen lassen; sie liegen danach fest (passiv fuer die naechsten)."""
        if self.batch:
            settle(self.batch, passive=self.items, masses=self.masses)
            for ob in list(self.batch):
                wx, wy = self.want[ob.name]
                d = math.hypot(ob.matrix_world.translation.x - wx, ob.matrix_world.translation.y - wy)
                print(f'HAUFEN {ob.name}: {d:.2f} m verschoben, z {ob.matrix_world.translation.z:.2f}', flush=True)
                if d > 0.7 or ob.matrix_world.translation.z < -0.01:  # weggeschleudert: Teil entfernen
                    self.batch.remove(ob)
                    bpy.data.objects.remove(ob, do_unlink=True)
            self.items += self.batch
            self.batch = []

    def plank(self, L, W, Tk, pos, rot, mat=None, broken=(False, True), nails=0):
        rg, M = self.rg, self.M
        mb = MB()
        bm, (xa, xb) = _plank_bm(L, W, Tk, rg, broken)
        mat = mat or M[rg.choice(['brett', 'brett', 'brett_alt', 'brett_grau'])]
        # Bruchflaechen heller (frisches Holz): Stirnflaechen an gebrochenen Enden
        for f in bm.faces:
            c = f.calc_center_median()
            end = (broken[1] and c.x > L - 0.12) or (broken[0] and c.x < 0.12)
            f.material_index = 1 if end and abs(f.normal.x) > 0.3 else 0
        mb.add(bm, [mat, M['bruch']], T(-L / 2, 0, -Tk / 2))
        for i in range(nails):  # krumme Naegel, die herausstehen
            x = rg.uniform(0.05, L - 0.05) - L / 2
            ln = rg.uniform(0.03, 0.06)
            tilt = rg.uniform(-0.5, 0.5)
            mb.add(b_tube([(0, 0, 0), (0, 0, ln * 0.6), (rg.uniform(-0.01, 0.01), rg.uniform(-0.01, 0.01), ln)], 0.0016, 4, caps=True),
                   M['rost'], T(x, rg.uniform(-W / 3, W / 3), Tk / 2) @ R(tilt, 'X'))
            mb.add(b_nail(0.0035, 0.001), M['nagel'], T(x, rg.uniform(-W / 3, W / 3), Tk / 2))
        return self.item('brett', mb, 2.0 * L * W / 0.2, pos, rot)

    def pallet_piece(self, pos, rot):
        """Bruchstueck einer Palette: drei Deckbretter auf einem Querbrett mit Kloetzen."""
        rg, M = self.rg, self.M
        mb = MB()
        for k, yy in enumerate((-0.25, 0.0, 0.25)):
            L = rg.uniform(0.55, 0.8)
            bm, _ = _plank_bm(L, 0.10, 0.022, rg, (False, rg.random() < 0.7))
            mb.add(bm, M['brett_alt'] if k != 1 else M['brett'], T(-0.2, yy, 0.1))
        mb.add(chamfer(b_box((-0.18, -0.4, 0.078), (-0.08, 0.4, 0.1)), 0.003), M['brett_alt'])
        for yy in (-0.33, 0.0, 0.33):
            mb.add(chamfer(b_box((-0.2, yy - 0.072, 0.0), (-0.06, yy + 0.072, 0.078)), 0.004), M['brett_grau'])
        for yy in (-0.25, 0.0, 0.25):
            for xx in (-0.15, -0.11):
                mb.add(b_nail(0.004, 0.0012), M['rost'], T(xx, yy + rg.uniform(-0.02, 0.02), 0.122))
        return self.item('palette', mb, 8.0, pos, rot)

    def card_flat(self, L, W, pos, rot, wet=False):
        """Flachgedrueckter Karton: zwei Lagen mit Falz, Laschen an den Enden, Klebeband ueber der Naht."""
        rg, M = self.rg, self.M
        mb = MB()
        mat = M['karton_nass'] if wet else M['karton']
        d = 0.007
        slit = 0.004
        fl = W * 0.42
        body = rect(-L / 2 + fl, -W / 2, L / 2 - fl, W / 2)
        mb.add(b_prism(body, 0, d), mat)
        # Laschen mit Schlitzen
        for sx in (-1, 1):
            for k in range(2):
                y0 = -W / 2 + k * (W / 2 + slit / 2)
                y1 = y0 + W / 2 - slit / 2
                x0, x1 = sorted((sx * (L / 2 - fl + 0.002), sx * (L / 2 - rg.uniform(0, 0.02))))
                mb.add(b_prism(rect(x0, y0, x1, y1), 0, d * rg.uniform(0.7, 1.0)), mat)
        # Falz (Rille) und Klebeband
        mb.add(b_box((-L / 2 + fl + 0.004, -W / 2 + 0.03, d), (L / 2 - fl - 0.004, -W / 2 + 0.034, d + 0.0004)), M['karton_nass'])
        tw = 0.048
        mb.add(b_box((-L / 2 + fl - 0.05, -tw / 2, d), (L / 2 - fl + 0.04, tw / 2, d + 0.0008)), M['klebeband'])
        # Etikett
        mb.add(b_box((0.05, W / 2 - 0.12, d), (0.15, W / 2 - 0.05, d + 0.0005)), M['folie'])
        return self.item('karton', mb, 0.6, pos, rot)

    def card_box(self, L, W, H, pos, rot):
        """Offener, eingedrueckter Karton: Waende, Boden, Laschen in verschiedenen Winkeln."""
        rg, M = self.rg, self.M
        mb = MB()
        d = 0.0045
        mat = M['karton']
        dent = rg.uniform(0.02, 0.05)
        # Waende als Raster mit Delle
        for (n, u, o, fw) in ((Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((L / 2, 0, 0)), W),
                              (Vector((-1, 0, 0)), Vector((0, -1, 0)), Vector((-L / 2, 0, 0)), W),
                              (Vector((0, 1, 0)), Vector((-1, 0, 0)), Vector((0, W / 2, 0)), L),
                              (Vector((0, -1, 0)), Vector((1, 0, 0)), Vector((0, -W / 2, 0)), L)):
            P = []
            for i in range(5):
                row = []
                for j in range(4):
                    uu = -fw / 2 + fw * i / 4
                    vv = H * j / 3
                    dd = -dent * math.sin(math.pi * i / 4) * math.sin(math.pi * j / 3) if n.y > 0 else 0.0
                    row.append(o + u * uu + Vector((0, 0, vv)) + n * dd)
                P.append(row)
            mb.add(b_grid(P, smooth=False), mat)
            # Lasche oben, aufgeklappt
            ang = rg.uniform(0.3, 1.9)
            F = frame(o + Vector((0, 0, H)), u, Vector((0, 0, 1)), n)
            mb.add(b_plate(rect(-fw / 2 + 0.003, 0, fw / 2 - 0.003, W / 2 * rg.uniform(0.8, 1.0))), mat, F @ R(ang, 'X'))
        mb.add(b_box((-L / 2, -W / 2, 0), (L / 2, W / 2, d)), mat)
        mb.add(b_box((-L / 2 - 0.001, -0.024, 0.0), (-L / 2 + 0.0005, 0.024, H * 0.4)), M['klebeband'])
        return self.item('kiste_karton', mb, 0.8, pos, rot)

    def corrugated(self, L, W, pos, rot):
        """Wellblech (76 mm Welle, 18 mm tief), 1 mm stark, mit Rostflecken; Ecke umgebogen."""
        rg, M = self.rg, self.M
        mb = MB()
        nu, nv = 3, int(W / 0.0095)
        bend = rg.uniform(0.2, 0.5)

        def pt(i, j, off):
            x = -L / 2 + L * i / nu
            y = -W / 2 + W * j / nv
            z = 0.009 * math.sin(TAU * y / 0.076) + off
            if x > L / 2 - 0.12:  # umgebogene Kante
                z += (x - (L / 2 - 0.12)) * bend
            return Vector((x, y, z))
        for off, flip in ((0.0006, False), (-0.0006, True)):
            P = [[pt(i, j, off) for j in range(nv + 1)] for i in range(nu + 1)]
            if flip:
                P = [list(reversed(row)) for row in P]
            bm = b_grid(P, smooth=True)
            # Rost: Flaechen nahe einer Ecke und in Flecken
            mb2 = MB()
            mb2.bm.free()
            mb.add(bm, M['blech'])
        # Rostflecken als eigene Inseln
        for _ in range(4):
            cx, cy = rg.uniform(-L / 2, L / 2), rg.uniform(-W / 2, W / 2)
            r_ = rg.uniform(0.04, 0.1)
            P = [[Vector((cx + r_ * (i / 3 - 0.5) * 2, cy + r_ * (j / 6 - 0.5) * 2, 0)) for j in range(7)] for i in range(4)]
            for row in P:
                for p in row:
                    p.z = 0.009 * math.sin(TAU * p.y / 0.076) + 0.0013
                    if p.x > L / 2 - 0.12:
                        p.z += (p.x - (L / 2 - 0.12)) * bend
            pts = [p for row in P for p in row if abs(p.x) <= L / 2 and abs(p.y) <= W / 2]
            if len(pts) == len(P) * len(P[0]):
                mb.add(b_grid(P, smooth=True), M['rost'])
        return self.item('wellblech', mb, 4.0, pos, rot)

    def angle_iron(self, L, pos, rot):
        rg, M = self.rg, self.M
        mb = MB()
        a, s = 0.04, 0.004
        prof = [(0, 0), (a, 0), (a, s), (s, s), (s, a), (0, a)]
        mb.add(b_prism(prof, -L / 2, L / 2, ch=0.0006), M['rost'], R(math.pi / 2, 'Y') @ T(-a / 2, -a / 2, 0))
        # Bohrungen als dunkle Ringe
        for k in range(3):
            x = -L / 2 + 0.08 + k * (L - 0.16) / 2
            mb.add(b_plate(circle(x, a * 0.55, 0.006, 8), 0), M['stahl'], T(0, 0, 0) @ R(math.pi / 2, 'X') @ T(0, -a / 2 + 0.0, a / 2 + 0.0002))
        return self.item('winkel', mb, 3.0, pos, rot)

    def pipe(self, L, r, pos, rot):
        rg, M = self.rg, self.M
        mb = MB()
        prof = [(r - 0.003, -L / 2), (r, -L / 2), (r, L / 2), (r - 0.003, L / 2), (r - 0.003, -L / 2)]
        bm = b_lathe([(r - 0.003, L / 2), (r - 0.003, -L / 2)], 12)  # innen (Normale nach innen)
        mb.add(bm, M['rost'])
        mb.add(b_lathe([(r - 0.003, -L / 2), (r, -L / 2), (r, L / 2), (r - 0.003, L / 2)], 12), M['stahl'])
        mb.add(b_lathe([(r + 0.0006, -L / 2 + 0.1), (r + 0.0006, -L / 2 + 0.1 + rg.uniform(0.08, 0.2))], 12), M['rost_hell'])
        del prof
        ob = self.item('rohr', mb, 3.0, pos, rot)
        return ob

    def strap(self, pts, mat=None):
        """PP-Umreifungsband (12 mm) als flaches Band entlang einer Linie - wird nach dem Fallen aufgelegt."""
        M = self.M
        mb = MB()
        w = 0.006
        P = []
        for i in range(len(pts)):
            p = Vector(pts[i])
            d = (Vector(pts[min(i + 1, len(pts) - 1)]) - Vector(pts[max(i - 1, 0)])).normalized()
            side = Vector((-d.y, d.x, 0)).normalized()
            P.append([p - side * w, p + side * w])
        mb.add(b_grid(P, smooth=True), mat or M['band'])
        mb.obj('band')


def _debris(seed, kind):
    """kind 'a' (rund 2 x 1,8 m): Palettenstueck, Wellblech, Karton, Bretter, Winkel, Rohr, Abschnitte;
    kind 'b' (rund 2,6 x 1,7 m): lange Bretter, mehr Verpackung (flach, nass, offen), Blech, Winkel, Rohr.
    Die Teile fallen in vier Durchgaengen aufeinander (Starrkoerper), so liegt jedes auf etwas auf."""
    P = _Pile(seed)
    rg = P.rg
    j = lambda a: rg.uniform(-a, a)  # noqa: E731
    ang = lambda: rg.uniform(0, math.pi)  # noqa: E731
    if kind == 'a':
        P.pallet_piece((0.0, 0.0, 0.0), (0, 0, ang()))
        P.card_box(0.42, 0.32, 0.26, (0.55, 0.42, 0.0), (0, 0, ang()))
        P.drop()
        P.corrugated(0.95, 0.62, (-0.3 + j(0.05), -0.2 + j(0.05), 0.0), (0, 0, ang()))
        P.drop()
        for k in range(3):
            a0 = k * 2.1 + j(0.4)
            P.plank(rg.uniform(1.1, 1.6), rg.uniform(0.09, 0.15), rg.uniform(0.018, 0.026),
                    (0.15 * math.cos(a0), 0.15 * math.sin(a0), 0.0), (j(0.15), j(0.1), a0 + 1.4 + j(0.3)),
                    broken=(rg.random() < 0.3, rg.random() < 0.7), nails=rg.randint(0, 2))
        P.drop()
        P.card_flat(0.9, 0.55, (-0.05, 0.2, 0.0), (j(0.1), j(0.1), ang()))
        P.angle_iron(1.1, (0.2, -0.25, 0.0), (0, 0, ang()))
        P.pipe(0.75, 0.024, (0.45, 0.05, 0.0), (0, math.pi / 2, ang()))
        P.drop()
        for k in range(3):
            a0 = k * 2.0 + 0.7 + j(0.4)
            P.plank(rg.uniform(0.6, 1.2), rg.uniform(0.08, 0.13), 0.022, (0.12 * math.cos(a0), 0.12 * math.sin(a0), 0.0),
                    (j(0.2), j(0.15), a0 + j(0.5)), broken=(rg.random() < 0.5, True), nails=rg.randint(0, 2))
        P.drop()
        for k in range(3):
            a0 = rg.uniform(0, TAU)
            P.plank(rg.uniform(0.18, 0.35), rg.uniform(0.06, 0.1), 0.022, (0.55 * math.cos(a0), 0.5 * math.sin(a0), 0.0),
                    (0, 0, ang()), broken=(True, True))
        P.drop()
    else:
        for k in range(3):
            P.plank(rg.uniform(1.7, 2.2), rg.uniform(0.1, 0.16), 0.024, (-0.3 + j(0.1), -0.3 + k * 0.28 + j(0.04), 0.0),
                    (0, 0, j(0.12)), broken=(False, rg.random() < 0.5), nails=rg.randint(0, 2))
        P.card_box(0.5, 0.36, 0.3, (0.75, -0.35, 0.0), (0, 0, ang()))
        P.drop()
        P.card_flat(1.0, 0.6, (0.0, 0.0, 0.0), (j(0.05), j(0.05), ang()))
        P.corrugated(0.8, 0.5, (-0.75, 0.2, 0.0), (0, 0, ang()))
        P.drop()
        # zwei Bretter schraeg an den Karton gelehnt
        P.plank(1.5, 0.12, 0.022, (0.35, -0.3, 0.12), (0, -0.32, 0.15 + j(0.1)), broken=(False, True), nails=1)
        P.plank(1.2, 0.1, 0.022, (0.4, -0.05, 0.08), (0, -0.25, -0.3 + j(0.1)), broken=(True, False))
        P.card_flat(0.8, 0.5, (-0.45, -0.2, 0.0), (j(0.1), j(0.1), ang()), wet=True)
        P.drop()
        P.plank(rg.uniform(1.6, 2.0), 0.14, 0.024, (-0.15, 0.1, 0.0), (j(0.1), j(0.1), 0.25 + j(0.2)), broken=(False, True), nails=2)
        P.card_flat(0.7, 0.45, (-0.2, 0.25, 0.0), (j(0.1), j(0.1), ang()))
        P.angle_iron(0.9, (0.25, 0.25, 0.0), (0, 0, ang()))
        P.drop()
        P.pipe(0.6, 0.017, (-0.25, -0.1, 0.0), (0, math.pi / 2, ang()))
        for k in range(3):
            a0 = rg.uniform(0, TAU)
            P.plank(rg.uniform(0.4, 0.9), rg.uniform(0.07, 0.12), 0.02, (0.3 * math.cos(a0), 0.25 * math.sin(a0), 0.0),
                    (j(0.3), j(0.3), ang()), broken=(rg.random() < 0.5, True))
        P.drop()
    # Umreifungsband lose obenauf (folgt der Oberflaeche)
    lo, hi = obj_bounds(P.items)
    cx, cy = (lo.x + hi.x) / 2, (lo.y + hi.y) / 2
    pts = []
    for k in range(16):
        a = k / 15.0
        x = cx - 0.6 + 1.2 * a
        y = cy + 0.22 * math.sin(a * 5.0 + seed)
        pts.append((x, y, _surface_z(x, y) + 0.0025))
    P.strap(pts)
    # Spaene, Splitter und ein paar Naegel rund um den Haufen auf dem Boden
    M = P.M
    mb = MB()
    for k in range(18):
        ang = rg.uniform(0, TAU)
        rr = rg.uniform(0.35, 1.0)
        x, y = cx + math.cos(ang) * rr * (hi.x - lo.x) * 0.55, cy + math.sin(ang) * rr * (hi.y - lo.y) * 0.55
        if _surface_z(x, y) > 0.004:
            continue
        ln, w = rg.uniform(0.02, 0.09), rg.uniform(0.006, 0.02)
        poly = [(-ln / 2, -w / 2), (ln / 2 - rg.uniform(0, ln * 0.4), -w / 2), (ln / 2, rg.uniform(-w / 2, w / 2)),
                (ln / 2 - rg.uniform(0, ln * 0.3), w / 2), (-ln / 2 + rg.uniform(0, ln * 0.3), w / 2)]
        mb.add(b_prism(poly, 0.0, rg.uniform(0.002, 0.006)), rg.choice([M['bruch'], M['brett'], M['brett_alt']]), T(x, y, 0.0) @ R(rg.uniform(0, TAU), 'Z'))
    for k in range(5):
        ang = rg.uniform(0, TAU)
        x, y = cx + math.cos(ang) * (hi.x - lo.x) * 0.45, cy + math.sin(ang) * (hi.y - lo.y) * 0.45
        if _surface_z(x, y) > 0.004:
            continue
        mb.add(b_tube([(0, 0, 0.0016), (rg.uniform(0.035, 0.06), 0, 0.0016)], 0.0016, 4, caps=True), M['rost'],
               T(x, y, 0.0) @ R(rg.uniform(0, TAU), 'Z'))
    mb.obj('spaene')


def _surface_z(x, y, exclude=None):
    """Hoehe der obersten Flaeche der Szene ueber (x, y) (Strahl von oben, ein Teil ausgenommen)."""
    dg = bpy.context.evaluated_depsgraph_get()
    org = Vector((x, y, 5.0))
    for _ in range(8):
        hit, loc, nrm, idx, ob, mw = bpy.context.scene.ray_cast(dg, org, Vector((0, 0, -1)))
        if not hit:
            return 0.0
        if ob is not exclude:
            return max(0.0, loc.z)
        org = loc - Vector((0, 0, 0.0005))
    return 0.0


def build_debris_a():
    _debris(11, 'a')


def build_debris_b():
    _debris(12, 'b')


# =====================================================================================================================
# Farbige Stuetzenbaender (Merkpunkte): Anstrich 1,0 - 1,8 m um eine Betonstuetze (1 x 1 m oder 2 x 2 m)
# =====================================================================================================================
COLORS = {'blau': (0.16, 0.30, 0.56), 'gelb': (0.84, 0.64, 0.12), 'rot': (0.62, 0.15, 0.11), 'gruen': (0.17, 0.45, 0.26)}


def _blob(cx, cy, r, rg, n=9, squash=1.0, spike=0.45):
    """Unregelmaessiger Fleck (Abplatzer, Fleck): Sternpolygon mit zufaelligen Radien."""
    return [(cx + r * (1 - spike * rg.random()) * math.cos(TAU * k / n), cy + squash * r * (1 - spike * rg.random()) * math.sin(TAU * k / n))
            for k in range(n)]


def _band(color, size):
    """Gerollter Anstrich (0,6 mm ueber der Stuetzenflaeche) mit welligen Raendern, Laeufern unter der Unterkante,
    Abplatzern (Beton sichtbar, an den Kanten gehaeuft), abgegriffenen Stellen und Resten von Abdeckband.
    Innen ein Betonkern knapp unter der Stuetzenflaeche: im Spiel verdeckt, im Studio (ohne Stuetze) sieht man
    durch die Abplatzer Beton statt Leere."""
    rg = rng(sum(map(ord, color)) * 7 + int(size * 10))
    col = COLORS[color]
    paint = common.material(f'band_{color}', col, look='paint', r=0.8)
    worn = common.material(f'band_{color}_matt', tuple(c * 0.78 + 0.08 for c in col), look='paint', r=1.25)
    conc = common.material('stuetze_beton', (0.80, 0.80, 0.78), look='concrete', tex='concrete_wall_smooth')
    tape = common.material('abdeckband', (0.80, 0.74, 0.58), look='cardboard', r=1.2)
    mb = MB()
    h = size / 2
    z0, z1 = 1.0, 1.8
    nu = int(round(size / 0.04))
    off = h + 0.0006
    mb.add(b_box((-h + 0.004, -h + 0.004, z0 - 0.08), (h - 0.004, h - 0.004, z1 + 0.08)), conc)
    for side in range(4):
        a = side * math.pi / 2
        n = Vector((math.cos(a), math.sin(a), 0))
        u = Vector((-math.sin(a), math.cos(a), 0))
        F = frame(n * off, u, Vector((0, 0, 1)), n)
        ph = rg.uniform(0, 6)
        top = [z1 + 0.006 * math.sin(i * 0.7 + ph) + 0.003 * math.sin(i * 2.3) + rg.uniform(-0.002, 0.002) for i in range(nu + 1)]
        bot = [z0 + 0.005 * math.sin(i * 1.1 + ph) + 0.003 * math.sin(i * 2.9) + rg.uniform(-0.002, 0.002) for i in range(nu + 1)]
        us = [-off + 2 * off * i / nu for i in range(nu + 1)]
        P = [[Vector((us[i], bot[i] + (top[i] - bot[i]) * j / 2, 0.0)) for j in range(3)] for i in range(nu + 1)]
        mb.add(b_grid(P, smooth=False), paint, F)
        # abgegriffene, stumpfere Stellen (Haende, Schultern, Wagen)
        for _ in range(int(3 * size)):
            uc, vc = rg.uniform(-h + 0.05, h - 0.05), rg.uniform(z0 + 0.06, z1 - 0.1)
            mb.add(b_plate(_blob(uc, vc, rg.uniform(0.03, 0.08), rg, 11, rg.uniform(0.5, 1.2), 0.3), 0.0002), worn, F)
        # Abplatzer: Beton sichtbar; an den Kanten der Stuetze gehaeuft
        for _ in range(int(9 * size)):
            if rg.random() < 0.55:
                uc = (h + 0.0006) * rg.choice((-1, 1)) - rg.uniform(-0.004, 0.008) * (1 if rg.random() < 0.5 else -1)
            else:
                uc = rg.uniform(-h, h)
            vc = rg.uniform(z0, z1)
            r_ = rg.uniform(0.004, 0.018)
            pol = clip_rect(_blob(uc, vc, r_, rg, 8, rg.uniform(0.6, 1.4)), -off, z0 - 0.01, off, z1 + 0.01)
            if len(pol) >= 3:
                mb.add(b_plate(pol, 0.0004), conc, F)
        # Laeufer unter der Unterkante
        for _ in range(int(3 * size)):
            uc = rg.uniform(-h + 0.05, h - 0.05)
            i = min(nu - 1, int((uc + off) / (2 * off) * nu))
            ln = rg.uniform(0.012, 0.06)
            w = rg.uniform(0.004, 0.009)
            vb = bot[i] + 0.003
            mb.add(b_plate([(uc - w / 2, vb), (uc - w * 0.35, vb - ln * 0.75), (uc - w * 0.15, vb - ln), (uc + w * 0.15, vb - ln),
                            (uc + w * 0.35, vb - ln * 0.75), (uc + w / 2, vb)], 0.0), paint, F)
        # Reste von Abdeckband an einer Kante
        if rg.random() < 0.6:
            uc = rg.uniform(-h + 0.1, h - 0.1)
            edge = rg.choice((z0, z1))
            lw = rg.uniform(0.03, 0.09)
            sgn = 1 if edge == z1 else -1
            mb.add(b_plate([(uc, edge), (uc + lw, edge + rg.uniform(-0.002, 0.002)), (uc + lw - 0.004, edge + sgn * 0.019),
                            (uc + 0.006, edge + sgn * 0.017)], 0.0005), tape, F)
    mb.obj('band')


def build_pillar_band_blau():
    _band('blau', 1.0)


def build_pillar_band_gelb():
    _band('gelb', 1.0)


def build_pillar_band_rot():
    _band('rot', 1.0)


def build_pillar_band_gruen():
    _band('gruen', 1.0)


def build_pillar_band2_blau():
    _band('blau', 2.0)


def build_pillar_band2_gelb():
    _band('gelb', 2.0)


# =====================================================================================================================
# Gluehbirne am Kabel (Bueros der Gaenge)
# =====================================================================================================================
def build_hanging_bulb():
    """Decke bei z = 0: Baldachin (Kunststoff) mit 2 Schrauben, Zugentlastung, Textilkabel (leicht verdrallt, kleine
    Knicke), E27-Fassung aus Bakelit mit Gewindering, Birne A60 (leuchtend) mit Hals; Staub auf dem Baldachin."""
    rg = rng(5)
    black = common.material('bakelit', (0.06, 0.055, 0.05), look='plastic', r=0.75)
    cable = common.material('textilkabel', (0.16, 0.15, 0.13), look='fabric', r=1.3)
    rose = common.material('baldachin', (0.80, 0.78, 0.72), look='plastic', r=0.8)
    dust = common.material('staub', (0.55, 0.53, 0.49), look='fabric', r=1.5)
    brass = common.material('messing', (0.62, 0.50, 0.30), look='metal', r=0.7, m=0.8)
    screw = common.material('schraube', (0.55, 0.55, 0.53), look='metal', r=0.8, m=0.7)
    bulb = common.material('birne', (1.0, 0.82, 0.55), look='glow', glow=2.5)
    neck = common.material('birnenhals', (0.92, 0.86, 0.74), look='glow', glow=1.2)
    mb = MB()
    # Baldachin: flache Glocke (85 mm), Unterkante gerundet
    mb.add(b_lathe([(0.0, -0.026), (0.008, -0.026), (0.012, -0.024), (0.03, -0.018), (0.04, -0.008), (0.0425, -0.002),
                    (0.0425, 0.0)], 16), rose)
    for a in (0.3, 0.3 + math.pi):
        mb.add(b_cyl(0.0035, -0.0105, -0.0095, 8), screw, T(0.031 * math.cos(a), 0.031 * math.sin(a), -0.0) @ R(0.38 if a < 1 else -0.38, 'X' if False else 'Y') @ T(0, 0, 0))
    mb.add(b_lathe([(0.0, -0.0247), (0.009, -0.0247), (0.0, -0.0243)], 10), dust)
    # Zugentlastung (Kegel) unter dem Baldachin
    mb.add(b_lathe([(0.0, -0.046), (0.0045, -0.046), (0.0075, -0.03), (0.009, -0.026), (0.0, -0.026)], 10), black)
    # Kabel: 6,5 mm, von -0.046 bis zur Fassung, leicht geschwungen
    pts = []
    zt, zb = -0.046, -0.598
    for k in range(13):
        f = k / 12
        pts.append((0.0025 * math.sin(f * 7.0) * (1 - f), 0.002 * math.cos(f * 5.0) * (1 - f), zt + (zb - zt) * f))
    mb.add(b_tube(pts, 0.0033, 8, caps=False), cable)
    # Fassung E27: Kabeltuelle, Mantel mit Rillen, Gewindering
    prof = [(0.0, -0.594), (0.006, -0.594), (0.008, -0.6), (0.016, -0.606), (0.0175, -0.612)]
    for k in range(3):
        zz = -0.616 - k * 0.0095
        prof += [(0.0185, zz), (0.0176, zz - 0.0045)]
    prof += [(0.0185, -0.648), (0.0215, -0.65), (0.0215, -0.656), (0.0185, -0.658), (0.0172, -0.66), (0.0, -0.66)]
    mb.add(b_lathe(prof, 14), black)
    mb.add(b_lathe([(0.0135, -0.66), (0.0135, -0.664), (0.0, -0.664)], 14), brass)  # Gewinde der Birne (sichtbar)
    # Birne A60: Hals, Kolben (60 mm), unten leicht abgeplattet
    bp = [(0.0, -0.664), (0.0135, -0.664), (0.0142, -0.672), (0.017, -0.682)]
    for k in range(1, 9):
        a = math.pi * k / 9
        bp.append((0.0305 * math.sin(a) ** 0.95, -0.716 + 0.033 * math.cos(a) * (1.0 if k < 5 else 0.98)))
    bp.append((0.0, -0.75))
    mb.add(b_lathe(bp[:4], 14), neck)
    mb.add(b_lathe(bp[3:], 14), bulb)
    mb.obj('gluehbirne')
    del rg


# =====================================================================================================================
# Gothic: hohe Betonboegen (Brettschalung) und runde Saeulen
# =====================================================================================================================
def _conc_mats():
    m = common.material
    return [m('beton_a', (0.88, 0.86, 0.82), look='concrete', tex='concrete_wall_smooth'),
            m('beton_b', (0.84, 0.82, 0.78), look='concrete', tex='concrete_wall_smooth'),
            m('beton_c', (0.90, 0.88, 0.85), look='concrete', tex='concrete_wall_smooth'),
            m('beton_dunkel', (0.70, 0.68, 0.64), look='concrete', tex='concrete_wall_smooth', r=1.2)]


def b_ring(outer, inner, z0, z1):
    """Ring-Prisma zwischen zwei geschlossenen Linien gleicher Punktzahl (beide gegen den Uhrzeigersinn)."""
    bm = bmesh.new()
    n = len(outer)
    oa = [bm.verts.new((x, y, z0)) for x, y in outer]
    ob = [bm.verts.new((x, y, z1)) for x, y in outer]
    ia = [bm.verts.new((x, y, z0)) for x, y in inner]
    ib = [bm.verts.new((x, y, z1)) for x, y in inner]
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ob[i], ob[j], ib[j], ib[i]))     # oben
        bm.faces.new((oa[j], oa[i], ia[i], ia[j]))     # unten
        bm.faces.new((oa[i], oa[j], ob[j], ob[i]))     # aussen
        bm.faces.new((ia[j], ia[i], ib[i], ib[j]))     # innen
    return bm


def rrect(x0, y0, x1, y1, r, seg=4):
    """Rechteck mit gerundeten Ecken (gegen den Uhrzeigersinn), 4 * (seg + 1) Punkte."""
    pts = []
    for (cx, cy, a0) in ((x1 - r, y0 + r, -math.pi / 2), (x1 - r, y1 - r, 0.0), (x0 + r, y1 - r, math.pi / 2), (x0 + r, y0 + r, math.pi)):
        for k in range(seg + 1):
            a = a0 + (math.pi / 2) * k / seg
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def cut_corners(poly, sizes):
    """Ecken eines Polygons abschneiden (je Ecke (a, b) entlang der beiden Kanten)."""
    out = []
    n = len(poly)
    for i in range(n):
        P, Q, S = Vector(poly[i]), Vector(poly[i - 1]), Vector(poly[(i + 1) % n])
        a, b = sizes[i]
        out.append(tuple(P + (Q - P).normalized() * a))
        out.append(tuple(P + (S - P).normalized() * b))
    return out


# Rahmen: lokale y-Achse -> Welt z, lokale z -> Welt x (Profile in der y-z-Ebene, entlang x gezogen)
FYZ = frame(Vector((0, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)))
# lokale x -> Welt x, lokale y -> Welt z, lokale z -> Welt -y (Profile in der x-z-Ebene, entlang y gezogen)
FXZ = frame(Vector((0, 0, 0)), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, -1, 0)))
# y und z tauschen (Querschnitt (u, n) entlang v ziehen)
SWAP = frame(Vector((0, 0, 0)), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, 1, 0)))


def _arch_tall(width, spring, crown, top, D=0.5, impost=True):
    """Segmentbogen im 1 m tiefen Durchbruch (Ursprung Mitte am Boden, Laibung x -0.5..0.5, Oeffnung y +-width/2).
    Laibung aus Brettschalung (Fugen laufen durch die Wandtiefe, jedes Brett leicht anders im Ton und in der Lage),
    2-cm-Fasen an beiden Wandseiten, Bogenzwickel, profilierte Archivolte, Schlussstein (auch in der Laibung),
    Kaempfer mit Karnies an beiden Laibungen mit Rueckspruengen auf der Wand; unterste Bretter dunkler."""
    rg = rng(int(width * 100))
    C = _conc_mats()
    mb = MB()
    half = width / 2.0
    rise = crown - spring
    R0 = (half * half + rise * rise) / (2 * rise)
    cz = crown - R0
    a_l, a_r = math.atan2(spring - cz, -half), math.atan2(spring - cz, half)
    c = 0.02
    nb = max(8, int(round(R0 * (a_l - a_r) / 0.105)))
    g = 0.0035 / R0

    def P(a, r):
        return (r * math.cos(a), cz + r * math.sin(a))
    for k in range(nb):
        b0 = a_r + (a_l - a_r) * k / nb
        b1 = a_r + (a_l - a_r) * (k + 1) / nb
        dr = rg.uniform(-0.0015, 0.0015)
        low = k < 3 or k >= nb - 3
        mat = C[3] if low else C[rg.randrange(3)]
        angs = [b0 + g, (b0 + b1) / 2, b1 - g]
        pts = [P(a, R0 + dr) for a in angs]
        mb.add(b_grid([[Vector((x, y, z)) for (y, z) in pts] for x in (-D + c, D - c)], smooth=False), mat)
        for sx in (-1, 1):
            inner = [Vector((sx * (D - c), y, z)) for (y, z) in pts]
            outer = [Vector((sx * (D + 0.0005), *P(a, R0 + dr + c + 0.0005))) for a in angs]
            mb.add(b_grid([inner, outer] if sx > 0 else [outer, inner], smooth=False), mat)
        if k < nb - 1:
            gp = [P(b1 - g, R0 + dr), P(b1, R0 + 0.004), P(b1 + g, R0)]
            mb.add(b_grid([[Vector((x, y, z)) for (y, z) in gp] for x in (-D + c, D - c)], smooth=False), C[3])
    for sx in (-1, 1):
        n = 40
        rowA = [Vector((sx * (D + 0.001), *P(a_r + (a_l - a_r) * i / n, R0 + c + 0.0005))) for i in range(n + 1)]
        rowB = [Vector((p.x, p.y, top)) for p in rowA]
        mb.add(b_grid([rowA, rowB] if sx > 0 else [rowB, rowA], smooth=False), C[0])
        F = frame(Vector((sx * (D + 0.001), 0, cz)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((sx, 0, 0)))
        r0 = R0 + c
        prof = [(r0 + 0.17, 0.0), (r0 + 0.17, 0.010), (r0 + 0.155, 0.022), (r0 + 0.06, 0.026), (r0 + 0.035, 0.034),
                (r0 + 0.012, 0.034), (r0, 0.024), (r0, 0.0)]
        mb.add(b_lathe(prof, 48, a_r, a_l, smooth=False), C[1], F)
        kw0, kw1, kh = 0.16, 0.25, 0.36
        mb.add(b_prism([(-kw0 / 2, R0 - 0.025), (kw0 / 2, R0 - 0.025), (kw1 / 2, R0 + kh), (-kw1 / 2, R0 + kh)], 0.0, 0.042, ch=0.004),
               C[2], F)
    # Schlussstein in der Laibung (2 cm tiefer als die Bretter)
    da = 0.08 / R0
    kpoly = [P(math.pi / 2 - da, R0 - 0.022), P(math.pi / 2 + da, R0 - 0.022), P(math.pi / 2 + da, R0 + 0.004), P(math.pi / 2 - da, R0 + 0.004)]
    mb.add(chamfer(b_prism(kpoly, -D, D), 0.003), C[2], FYZ)
    # Kaempfer: Karnies (u nach innen, v nach oben) ueber die Laibung, Rueckspruenge auf beiden Wandflaechen
    kp = [(0.0, 0.0), (0.012, 0.0), (0.016, 0.03), (0.03, 0.06), (0.05, 0.085), (0.055, 0.12), (0.055, 0.16), (0.0, 0.16)]
    for sy in ((-1, 1) if impost else ()):
        poly = [(sy * (half - u), spring - 0.16 + v) for (u, v) in kp]
        mb.add(b_prism(poly, -D - 0.035, D + 0.035), C[1], FYZ)
        for sx in (-1, 1):
            poly = [(sx * (D + u * 0.035 / 0.055), spring - 0.16 + v) for (u, v) in kp]
            y0, y1 = sorted((sy * half, sy * (half + 0.11)))
            mb.add(b_prism(poly, -y1, -y0), C[1], FXZ)
    mb.obj('bogen')


def build_arch_tall3():
    _arch_tall(3.0, 3.0, 4.35, 4.62)


def build_arch_tall4():
    _arch_tall(4.0, 2.9, 4.45, 4.62)


def build_arcade_gothic():
    """V13.3 (Kritikrunde 1 Level 1): Arkadenbogen zwischen zwei runden Saeulen der Gothic-Hallen (Achsabstand 7 m).
    Ursprung an der Decke (5,6 m ueber dem Boden), Bogen spannt entlang y: Kaempfer 4,5 m auf Halsring-Hoehe der
    Saeulen (Schaftradius 0,425 m), Scheitel 5,45 m, Zwickel bis zur Decke; 0,8 m tief, ohne eigene Kaempfer (der
    Bogen waechst aus dem Kapitell). Aufbau wie die hohen Durchgangsboegen: Brettschalung, Archivolte, Schlussstein."""
    _arch_tall(7.0 - 2 * 0.425, 4.5 - 5.6, 5.45 - 5.6, 0.0, D=0.4, impost=False)


def build_column_round():
    """Runde Betonsaeule 5,6 m (Gothic): quadratische Plinthe mit abgestossenen Ecken (unten feucht), Wulst-Kehle-
    Wulst-Basis, Schaft mit leichter Schwellung, zwei Arbeitsfugen und zwei Schalungsnaehten, Halsring, ausladendes
    Kapitell und quadratische Deckplatte unter der Decke; jede Betonierlage im eigenen Ton."""
    rg = rng(77)
    C = _conc_mats()
    mb = MB()
    H = 5.6
    seg = 40
    pl = cut_corners(rect(-0.55, -0.55, 0.55, 0.55), [(rg.uniform(0.008, 0.04), rg.uniform(0.008, 0.04)) for _ in range(4)])
    mb.add(b_prism(pl, 0.0, 0.08), C[3])
    mb.add(b_prism(pl, 0.08, 0.28, ch=0.012), C[1])
    z_a, z_b = 0.5, 4.45
    joints = (1.85, 3.25)

    def shaft_r(z):
        t = (z - z_a) / (z_b - z_a)
        return 0.425 * (1.0 + 0.03 * math.sin(math.pi * t * 0.9)) - 0.012 * t
    prof = [(0.0, 0.28), (0.50, 0.28), (0.515, 0.30), (0.52, 0.33), (0.505, 0.36), (0.47, 0.375), (0.455, 0.40),
            (0.46, 0.43), (0.475, 0.45), (0.47, 0.475), (0.445, 0.49), (0.425, 0.50)]
    zs = [z_a + (z_b - z_a) * k / 15 for k in range(1, 16)]
    for zj in joints:
        zs = [z for z in zs if abs(z - zj) > 0.06] + [zj - 0.012, zj - 0.004, zj + 0.004, zj + 0.012]
    for z in sorted(zs):
        r = shaft_r(z)
        if any(abs(z - zj) < 0.005 for zj in joints):
            r -= 0.006
        prof.append((r, z))
    rt = shaft_r(z_b)
    prof += [(rt + 0.018, 4.47), (rt + 0.022, 4.50), (rt + 0.018, 4.53), (rt, 4.55), (rt, 4.62)]
    for k in range(1, 10):
        t = k / 9.0
        prof.append((rt + (0.545 - rt) * (t ** 2.2), 4.62 + (5.38 - 4.62) * t))
    cuts = [0.0, joints[0] - 0.004, joints[1] - 0.004, 4.55, 9.9]
    mats = [C[1], C[0], C[2], C[1]]
    for i in range(4):
        part = [p for p in prof if cuts[i] - 1e-6 <= p[1] <= cuts[i + 1] + 1e-6 and p[0] > 0]
        if i == 0:
            part = [(0.0, 0.28)] + part
            # dichtere Ringe am Fuss fuer den Schmutzsaum
            extra = [(shaft_r(z), z) for z in (0.56, 0.62, 0.68, 0.74, 0.80) if z not in [p[1] for p in part]]
            part = sorted(part[1:] + extra, key=lambda p: p[1])
            part = [(0.0, 0.28)] + part
        if i == 3:
            part = part + [(0.0, 5.38)]
        bm = b_lathe(part, seg, smooth=True)
        if i == 0:  # Schmutz und Feuchte unten: Oberkante unregelmaessig (je Flaeche)
            for f in bm.faces:
                cz_ = f.calc_center_median()
                a = math.atan2(cz_.y, cz_.x)
                edge = 0.62 + 0.08 * math.sin(a * 3 + 1.0) + 0.05 * math.sin(a * 7)
                f.material_index = 1 if cz_.z < edge else 0
            mb.add(bm, [mats[i], C[3]])
        else:
            mb.add(bm, mats[i])
    mb.add(b_prism(rect(-0.575, -0.575, 0.575, 0.575), 5.38, H, ch=0.01), C[1])
    # Schalungsnaehte (Stahlschalung aus zwei Halbschalen): zwei senkrechte Grate
    for a in (0.3, 0.3 + math.pi):
        for (z0, z1) in ((0.52, joints[0] - 0.02), (joints[0] + 0.02, joints[1] - 0.02), (joints[1] + 0.02, 4.44)):
            for k in range(6):
                za, zb = z0 + (z1 - z0) * k / 6, z0 + (z1 - z0) * (k + 1) / 6
                r = shaft_r((za + zb) / 2)
                q = [(r - 0.002, -0.003), (r + 0.0022, -0.0012), (r + 0.0022, 0.0012), (r - 0.002, 0.003)]
                mb.add(b_prism(q, za, zb), C[0], R(a, 'Z'))
    # Abplatzer an der Plinthe (dunkle Bruchflaechen)
    for _ in range(6):
        side = rg.randrange(4)
        ca, sa = math.cos(side * math.pi / 2), math.sin(side * math.pi / 2)
        F = frame(Vector((0.5506 * ca, 0.5506 * sa, 0)), Vector((-sa, ca, 0)), Vector((0, 0, 1)), Vector((ca, sa, 0)))
        u = rg.choice((rg.uniform(-0.5, -0.4), rg.uniform(0.4, 0.5), rg.uniform(-0.4, 0.4)))
        mb.add(b_plate(clip_rect(_blob(u, rg.uniform(0.1, 0.26), rg.uniform(0.012, 0.03), rg, 8), -0.53, 0.0, 0.53, 0.27), 0.0), C[3], F)
    mb.obj('saeule')


# =====================================================================================================================
# Schwingtueren mit Sichtfenstern (Gaenge: einfache oder doppelte Schwingtuer)
# =====================================================================================================================
def _door_mats():
    m = common.material
    return dict(
        zarge=m('zarge', (0.32, 0.34, 0.33), look='paint', r=0.75),
        blatt=m('tuerblatt', (0.53, 0.57, 0.51), look='paint', r=0.7),
        grund=m('grundierung', (0.62, 0.58, 0.50), look='paint', r=1.2),
        edel=m('edelstahl', (0.66, 0.66, 0.64), look='metal', r=0.55, m=0.85),
        edel_matt=m('edelstahl_matt', (0.52, 0.52, 0.50), look='metal', r=1.0, m=0.7),
        gummi=m('gummi', (0.07, 0.07, 0.07), look='rubber', r=1.0),
        glas=m('drahtglas', (0.24, 0.28, 0.27), look='glass'),
        schmutz=m('griffschmutz', (0.43, 0.46, 0.40), look='paint', r=1.25),
        abrieb=m('abrieb', (0.10, 0.10, 0.10), look='rubber', r=1.2),
        schraube=m('schraube', (0.55, 0.55, 0.53), look='metal', r=0.8, m=0.7),
    )


def _leaf(mb, M, lw, Hl, rg, F):
    """Pendelfluegel im eigenen Rahmen F: u = 0 (Drehachse) .. lw, v = Hoehe, n = Dicke (+-22,5 mm). Senkrechte Kanten
    gerundet, Sichtfenster (Durchbruch mit Glas, Glasleisten und Dichtung), je Seite Trittblech mit Abrieb,
    Schiebeblech mit Griffschmutz, abgestossene Lackstellen; Fingerschutz an der freien Kante, Drehzapfen."""
    th, rr = 0.0225, 0.012
    wu0, wu1, wv0, wv1 = lw * 0.27, lw * 0.73, 1.22, 1.76

    def section(u0, u1, round0, round1):
        pts = []
        if round1:
            pts += [(u1 - rr + rr * math.cos(a), -th + rr + rr * math.sin(a)) for a in [-math.pi / 2 + math.pi / 6 * k for k in range(4)]]
            pts += [(u1 - rr + rr * math.cos(a), th - rr + rr * math.sin(a)) for a in [math.pi / 6 * k for k in range(4)]]
        else:
            pts += [(u1, -th), (u1, th)]
        if round0:
            pts += [(u0 + rr + rr * math.cos(a), th - rr + rr * math.sin(a)) for a in [math.pi / 2 + math.pi / 6 * k for k in range(4)]]
            pts += [(u0 + rr + rr * math.cos(a), -th + rr + rr * math.sin(a)) for a in [math.pi + math.pi / 6 * k for k in range(4)]]
        else:
            pts += [(u0, th), (u0, -th)]
        return pts

    def slab(u0, u1, v0, v1, r0, r1):
        mb.add(b_prism(section(u0, u1, r0, r1), v0, v1), M['blatt'], F @ SWAP)
    slab(0.0, lw, 0.012, wv0, True, True)
    slab(0.0, lw, wv1, Hl, True, True)
    slab(0.0, wu0, wv0, wv1, True, False)
    slab(wu1, lw, wv0, wv1, False, True)
    mb.add(b_box((wu0 - 0.004, wv0 - 0.004, -0.003), (wu1 + 0.004, wv1 + 0.004, 0.003)), M['glas'], F)
    outer = rrect(wu0 - 0.028, wv0 - 0.028, wu1 + 0.028, wv1 + 0.028, 0.03, 4)
    inner = rrect(wu0 + 0.006, wv0 + 0.006, wu1 - 0.006, wv1 - 0.006, 0.006, 4)
    gas_i = rrect(wu0 + 0.012, wv0 + 0.012, wu1 - 0.012, wv1 - 0.012, 0.004, 4)
    for s in (-1, 1):
        Fs = F @ frame(Vector((0, 0, s * th)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, s)))
        mb.add(b_ring(outer, inner, -0.0005, 0.007), M['edel_matt'], Fs)
        mb.add(b_ring(inner, gas_i, -0.012, 0.002), M['gummi'], Fs)
        mb.add(b_prism(rect(0.025, 0.03, lw - 0.025, 0.31), 0.0, 0.0015), M['edel'], Fs)
        for (ku, kv) in ((0.04, 0.045), (lw - 0.04, 0.045), (0.04, 0.295), (lw - 0.04, 0.295)):
            mb.add(b_cyl(0.004, 0.0015, 0.0022, 6), M['schraube'], Fs @ T(ku, kv, 0))
        for _ in range(rg.randint(3, 6)):
            ku, kv = rg.uniform(0.08, lw - 0.15), rg.uniform(0.06, 0.26)
            mb.add(b_plate(ccw(stroke([(ku, kv), (ku + rg.uniform(0.04, 0.12), kv + rg.uniform(-0.01, 0.01))], rg.uniform(0.003, 0.009))),
                           0.0018), M['abrieb'], Fs)
        pu0, pu1 = lw - 0.17, lw - 0.06
        mb.add(b_plate(_blob((pu0 + pu1) / 2 + rg.uniform(-0.01, 0.01), 1.15, 0.1, rg, 16, 1.9, 0.5), 0.0004), M['schmutz'], Fs)
        mb.add(b_prism(rect(pu0, 1.0, pu1, 1.32), 0.0, 0.0015), M['edel'], Fs)
        for (ku, kv) in ((pu0 + 0.015, 1.015), (pu1 - 0.015, 1.015), (pu0 + 0.015, 1.305), (pu1 - 0.015, 1.305)):
            mb.add(b_cyl(0.0035, 0.0015, 0.0021, 6), M['schraube'], Fs @ T(ku, kv, 0))
        for _ in range(rg.randint(2, 4)):
            ku = rg.choice((rg.uniform(0.006, 0.03), lw - rg.uniform(0.006, 0.03)))
            mb.add(b_plate(_blob(ku, rg.uniform(0.35, 1.0), rg.uniform(0.004, 0.012), rg, 7, 1.6), 0.0003), M['grund'], Fs)
    mb.add(b_box((lw - 0.002, 0.03, -0.016), (lw + 0.007, Hl - 0.02, 0.016)), M['gummi'], F)
    for (v0, v1) in ((0.0, 0.06), (Hl - 0.05, Hl + 0.04)):
        mb.add(chamfer(b_box((-0.012, v0, -0.012), (0.05, v1, 0.012)), 0.002), M['edel_matt'], F)


def _swing(width, leaves):
    """Zarge (Stahlprofil, Spiegel 3 cm auf der Wand) an der +x-Seite des Durchbruchs (2,2 m Sturz) und Pendelfluegel
    (45 mm), die angelehnt offen stehen; Abdeckungen der Bodenschliesser."""
    rg = rng(int(width * 10 + leaves))
    M = _door_mats()
    mb = MB()
    half, top = width / 2.0, 2.2
    xf = 0.45
    zp = [(-0.03, 0.5), (-0.03, 0.512), (0.04, 0.512), (0.045, 0.507), (0.045, 0.405), (0.04, 0.40), (0.0, 0.40), (0.0, 0.5)]
    for sy in (-1, 1):
        mb.add(b_prism([(x, sy * (half - w)) for (w, x) in zp], 0.0, top + 0.03), M['zarge'])
    mb.add(b_prism([(x, top - w) for (w, x) in zp], -half - 0.03, half + 0.03), M['zarge'], FXZ)
    gap = 0.004
    lw = (width - 2 * 0.045 - gap * (leaves + 1)) / leaves
    Hl = top - 0.045 - 0.004
    opens = [math.radians(72), math.radians(64)] if leaves == 2 else [math.radians(78)]
    for k in range(leaves):
        sgn = 1 if k == 0 else -1
        hinge_y = sgn * (-half + 0.045 + gap)
        u = Vector((0, sgn, 0))
        F = T(xf, hinge_y, 0.0) @ R(opens[k], 'Z') @ frame(Vector((0, 0, 0)), u, Vector((0, 0, 1)), u.cross(Vector((0, 0, 1))))
        _leaf(mb, M, lw, Hl, rg, F)
        y0, y1 = sorted((hinge_y - sgn * 0.03, hinge_y + sgn * 0.27))
        mb.add(chamfer(b_box((xf - 0.05, y0, 0.0), (xf + 0.05, y1, 0.004)), 0.0015), M['edel_matt'])
    mb.obj('schwingtuer')


def build_swing_door1():
    _swing(1.0, 1)


def build_swing_door2():
    _swing(2.0, 2)


# =====================================================================================================================
# Notausgangsschild (Leuchte an der Wand ueber der Hallentuer; im Dunkeln das einzige Licht)
# =====================================================================================================================
def build_exit_sign_green():
    """Rettungszeichenleuchte (Wand x = 0, blickt entlang +x, Mitte im Ursprung): Gehaeuse 40 x 16 x 4,5 cm mit
    gerundeten Kanten (vergilbt), Rahmen um die eingelassene Leuchtscheibe, eigenes Piktogramm (Laeufer, Tuer,
    Pfeil) weiss auf gruen mit Saum, Ladeanzeige (LED), Kabelverschraubung oben mit kurzem Kabel in die Wand,
    Staub auf der Oberseite, eine tote Fliege hinter der Scheibe."""
    rg = rng(9)
    m = common.material
    housing = m('gehaeuse', (0.84, 0.83, 0.76), look='plastic', r=0.85)
    bezel = m('rahmen_schild', (0.80, 0.79, 0.72), look='plastic', r=0.7)
    green = m('gruen', (0.10, 0.85, 0.35), look='glow', glow=2.2)
    white = m('weiss', (0.95, 1.0, 0.96), look='glow', glow=3.0)
    led = m('led', (0.2, 1.0, 0.3), look='glow', glow=4.0)
    dust = m('staub', (0.55, 0.53, 0.49), look='fabric', r=1.5)
    dark = m('fliege', (0.04, 0.04, 0.04), look='paint', r=1.0)
    cable = m('kabel_weiss', (0.86, 0.86, 0.84), look='plastic', r=0.8)
    mb = MB()
    W2, H2, D = 0.20, 0.08, 0.045
    mb.add(b_prism(rrect(-W2, -H2, W2, H2, 0.012, 3), 0.0, D - 0.004), housing, FYZ)
    Ff = frame(Vector((D - 0.004, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)))
    inner = rrect(-W2 + 0.016, -H2 + 0.016, W2 - 0.016, H2 - 0.022, 0.004, 3)
    mb.add(b_ring(rrect(-W2 + 0.002, -H2 + 0.002, W2 - 0.002, H2 - 0.002, 0.01, 3), inner, 0.0, 0.004), bezel, Ff)
    Fs = frame(Vector((D - 0.002, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)))
    mb.add(b_plate(inner, 0.0), green, Fs)
    lift = 0.0003
    sx0, sx1, sz0, sz1 = -W2 + 0.022, W2 - 0.022, -H2 + 0.028, H2 - 0.022
    for q in (rect(sx0, sz0, sx1, sz0 + 0.002), rect(sx0, sz1 - 0.002, sx1, sz1), rect(sx0, sz0, sx0 + 0.002, sz1),
              rect(sx1 - 0.002, sz0, sx1, sz1)):
        mb.add(b_plate(q, lift), white, Fs)
    # Tuer links im Bild (-y), Laeufer laeuft hinein, Pfeil rechts zeigt zur Tuer
    for q in (rect(-0.162, -0.048, -0.153, 0.048), rect(-0.112, -0.048, -0.103, 0.048), rect(-0.162, 0.040, -0.103, 0.048),
              rect(-0.168, -0.052, -0.097, -0.046)):
        mb.add(b_plate(q, lift), white, Fs)
    mb.add(b_plate(circle(-0.052, 0.036, 0.0095, 14), lift), white, Fs)
    sw = 0.0085
    for pts in ([(-0.044, 0.021), (-0.056, -0.004)],
                [(-0.044, 0.019), (-0.064, 0.012), (-0.078, 0.021)],
                [(-0.045, 0.018), (-0.030, 0.008), (-0.024, -0.004)],
                [(-0.056, -0.004), (-0.072, -0.016), (-0.084, -0.010)],
                [(-0.056, -0.004), (-0.046, -0.024), (-0.034, -0.040)],
                [(-0.034, -0.040), (-0.026, -0.042)]):
        mb.add(b_plate(ccw(stroke(pts, sw)), lift), white, Fs)
    mb.add(b_plate(rect(0.03, -0.006, 0.15, 0.006), lift), white, Fs)
    mb.add(b_plate([(0.06, 0.022), (0.06, -0.022), (0.022, 0.0)], lift), white, Fs)
    mb.add(b_plate(_blob(0.12, -0.04, 0.003, rg, 7, 0.6), 0.0001), dark, Fs)
    for a in (0.6, 2.5):
        mb.add(b_plate(ccw(stroke([(0.12, -0.04), (0.12 + 0.005 * math.cos(a), -0.04 + 0.004 * math.sin(a))], 0.0012)), 0.0001), dark, Fs)
    mb.add(b_cyl(0.0022, 0.0, 0.0012, 8), led, Ff @ T(-W2 + 0.03, -H2 + 0.009, 0.004))
    mb.add(b_plate(rect(0.004, -W2 + 0.02, D - 0.01, W2 - 0.03), 0.0), dust, T(0, 0, H2 + 0.0004))
    gx, gy = 0.018, W2 - 0.05
    mb.add(b_cyl(0.007, H2 - 0.001, H2 + 0.009, 6), housing, T(gx, gy, 0))
    mb.add(b_cyl(0.0055, H2 + 0.009, H2 + 0.015, 10), housing, T(gx, gy, 0))
    mb.add(b_tube([(gx, gy, H2 + 0.012), (gx, gy, H2 + 0.022), (gx - 0.004, gy, H2 + 0.03), (gx - 0.012, gy, H2 + 0.034),
                   (-0.003, gy, H2 + 0.035)], 0.0035, 8, caps=True), cable)
    mb.obj('notausgang')



# =====================================================================================================================
# Arbeitstisch (Hallen; Wiki: Tische als Merkpunkte, manchmal Werkzeug wie Brecheisen)
# =====================================================================================================================
def _sq_tube(a, z0, z1, r=0.004):
    """Vierkantrohr a x a mit gerundeten Kanten, senkrecht von z0 bis z1, mittig um (0, 0)."""
    return b_prism(rrect(-a / 2, -a / 2, a / 2, a / 2, r, 1), z0, z1)


def build_work_table():
    """Werkbank 1,6 x 0,8 x 0,75 m: geschweisstes Gestell aus Vierkantrohr 40 x 40 (Hammerschlaglack, Abplatzer,
    Rost an den Fuessen), Stellfuesse, Schweissnaehte, Platte aus verleimten Buchenstaeben (40 mm) mit Oelflecken,
    Brandring, Farbklecksen, Schnitten und einer ausgebrochenen Kante, Zwischenboden aus Sperrholz mit Dose und Lappen,
    Schraubstock an der vorderen rechten Ecke, ein Brecheisen auf der Platte."""
    rg = rng(21)
    m = common.material
    steel = m('werkbank_gestell', (0.30, 0.37, 0.35), look='paint', r=0.85)
    bare = m('stahl_blank', (0.50, 0.50, 0.49), look='metal', r=0.6, m=0.85)
    rust = m('rost', (0.38, 0.22, 0.12), look='metal', r=1.45, m=0.1)
    zinc = m('verzinkt', (0.62, 0.63, 0.62), look='galvanized', r=0.9, m=0.6)
    rubber = m('gummi', (0.07, 0.07, 0.07), look='rubber', r=1.0)
    beech = [m('buche_a', (0.67, 0.51, 0.35), look='wood', r=0.95), m('buche_b', (0.60, 0.45, 0.30), look='wood', r=1.0),
             m('buche_c', (0.71, 0.56, 0.39), look='wood', r=0.95)]
    oil = m('oelfleck', (0.34, 0.25, 0.16), look='wood', r=0.5)
    burn = m('brandfleck', (0.12, 0.09, 0.06), look='wood', r=1.3)
    cut = m('schnitt', (0.36, 0.27, 0.18), look='wood', r=1.2)
    paints = [m('farbklecks_blau', (0.20, 0.32, 0.55), look='paint', r=0.6), m('farbklecks_weiss', (0.86, 0.85, 0.80), look='paint', r=0.6)]
    ply = m('sperrholz', (0.63, 0.53, 0.39), look='wood', r=1.25)
    vise_c = m('schraubstock', (0.20, 0.31, 0.46), look='paint', r=0.7)
    bar_red = m('brecheisen', (0.55, 0.10, 0.08), look='paint', r=0.6)
    rag = m('lappen', (0.36, 0.40, 0.48), look='fabric', r=1.4)
    mb = MB()
    L, W, H, tt = 1.6, 0.8, 0.75, 0.04
    xl, yl = L / 2 - 0.06, W / 2 - 0.06
    # Platte: 20 Buchenstaebe entlang x, leicht ungleich hoch
    n = 20
    for i in range(n):
        y0 = -W / 2 + W * i / n
        y1 = -W / 2 + W * (i + 1) / n
        dz = rg.uniform(-0.00015, 0.00015)
        poly = rect(-L / 2, y0, L / 2, y1)
        if i == 0:  # ausgebrochene Stelle an der Vorderkante
            poly = [(-L / 2, y0), (0.18, y0), (0.2, y0 + 0.012), (0.27, y0 + 0.015), (0.29, y0), (L / 2, y0), (L / 2, y1), (-L / 2, y1)]
        mb.add(b_prism(poly, H - tt, H + dz, ch=0.0005 if 0 < i < n - 1 else 0.003), beech[rg.randrange(3)])
    top = H + 0.0004
    # Gebrauchsspuren auf der Platte
    for _ in range(3):
        mb.add(b_plate(_blob(rg.uniform(-0.6, 0.6), rg.uniform(-0.3, 0.3), rg.uniform(0.03, 0.09), rg, 12, rg.uniform(0.6, 1.3), 0.3), top), oil)
    cx, cy = rg.uniform(-0.5, -0.1), rg.uniform(-0.1, 0.25)
    mb.add(b_ring(circle(cx, cy, 0.075, 24), circle(cx, cy, 0.062, 24), top - 0.0002, top), burn)
    for _ in range(7):
        mb.add(b_plate(_blob(rg.uniform(-0.7, 0.7), rg.uniform(-0.35, 0.35), rg.uniform(0.004, 0.015), rg, 7), top + 0.0001), rg.choice(paints))
    for _ in range(9):
        x0, y0 = rg.uniform(-0.7, 0.5), rg.uniform(-0.35, 0.3)
        a = rg.uniform(-0.6, 0.6)
        ln = rg.uniform(0.05, 0.25)
        mb.add(b_plate(ccw(stroke([(x0, y0), (x0 + ln * math.cos(a), y0 + ln * math.sin(a))], 0.0015)), top + 0.0002), cut)
    # Gestell: Beine, Zargen, untere Rahmen, Fussplatten, Stellfuesse
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * xl, sy * yl
            mb.add(_sq_tube(0.04, 0.035, H - tt), steel, T(x, y, 0))
            mb.add(chamfer(b_box((x - 0.03, y - 0.03, 0.03), (x + 0.03, y + 0.03, 0.035)), 0.001), steel)
            mb.add(b_cyl(0.006, 0.012, 0.03, 8), zinc, T(x, y, 0))
            mb.add(b_cyl(0.011, 0.02, 0.028, 6, smooth=False), zinc, T(x, y, 0))
            mb.add(b_cyl(0.025, 0.004, 0.012, 14), zinc, T(x, y, 0))
            mb.add(b_cyl(0.024, 0.0, 0.004, 14), rubber, T(x, y, 0))
            # Rost am Fuss
            mb.add(b_box((x - 0.0205, y - 0.0205, 0.035), (x + 0.0205, y + 0.0205, 0.035 + rg.uniform(0.02, 0.06))), rust)
    for sy in (-1, 1):
        for (z0, a) in ((H - tt - 0.04, 0.04), (0.15, 0.03)):
            mb.add(_sq_tube(a, -xl + 0.02, xl - 0.02), steel, T(0, sy * yl, z0 + a / 2) @ R(math.pi / 2, 'Y'))
    for sx in (-1, 1):
        for (z0, a) in ((H - tt - 0.04, 0.04), (0.15, 0.03)):
            mb.add(_sq_tube(a, -yl + 0.02, yl - 0.02), steel, T(sx * xl, 0, z0 + a / 2) @ R(-math.pi / 2, 'X'))
    # Schweissnaehte: Raupe rund um jedes Bein an den Stossstellen
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * xl, sy * yl
            for z in (H - tt - 0.04, 0.15, 0.18):
                ring = [(x + 0.0215 * cx_, y + 0.0215 * cy_, z + rg.uniform(-0.0008, 0.0008))
                        for (cx_, cy_) in ((1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1))]
                mb.add(b_tube(ring, 0.0028, 4, closed=True), steel)
    # Abplatzer am Gestell
    for _ in range(14):
        sx, sy = rg.choice((-1, 1)), rg.choice((-1, 1))
        z = rg.uniform(0.05, 0.65)
        face = rg.randrange(2)
        if face == 0:
            Fp = frame(Vector((sx * (xl + 0.0202), sy * yl, 0)), Vector((0, sx, 0)), Vector((0, 0, 1)), Vector((sx, 0, 0)))
        else:
            Fp = frame(Vector((sx * xl, sy * (yl + 0.0202), 0)), Vector((-sy, 0, 0)), Vector((0, 0, 1)), Vector((0, sy, 0)))
        mb.add(b_plate(clip_rect(_blob(rg.uniform(-0.015, 0.015), z, rg.uniform(0.004, 0.012), rg, 7), -0.019, 0.04, 0.019, 0.7), 0.0),
               rust if z < 0.2 else bare, Fp)
    # Zwischenboden (Sperrholz) mit abgebrochener Ecke
    shelf = [(-xl + 0.02, -yl + 0.02), (xl - 0.06, -yl + 0.02), (xl - 0.02, -yl + 0.05), (xl - 0.02, yl - 0.02), (-xl + 0.02, yl - 0.02)]
    mb.add(b_prism(shelf, 0.18, 0.198, ch=0.001), ply)
    # Dose mit Schrauben und ein Lappen auf dem Zwischenboden
    mb.add(b_lathe([(0.0, 0.198), (0.05, 0.198), (0.05, 0.31), (0.052, 0.312), (0.048, 0.312), (0.047, 0.205), (0.0, 0.205)], 16), zinc, T(-0.45, 0.12, 0))
    mb.add(b_plate(_blob(0.0, 0.0, 0.03, rg, 9), 0.0), rust, T(-0.45, 0.12, 0.29))
    for k in range(5):
        mb.add(b_cyl(0.003, 0.0, 0.035, 5), bare, T(-0.45 + rg.uniform(-0.02, 0.02), 0.12 + rg.uniform(-0.02, 0.02), 0.275) @ R(rg.uniform(-0.5, 0.5), 'X'))
    P = []
    for i in range(7):
        row = []
        for j in range(7):
            u, v = (i / 6 - 0.5), (j / 6 - 0.5)
            h = 0.045 * max(0.0, 1 - 3 * (u * u + v * v)) * (0.6 + 0.4 * math.sin(u * 17 + v * 11)) + rg.uniform(0, 0.012)
            row.append(Vector((0.2 + u * 0.32 + rg.uniform(-0.01, 0.01), -0.05 + v * 0.26, 0.199 + h)))
        P.append(row)
    mb.add(b_grid(P, smooth=True), rag)
    # Schraubstock an der rechten Stirnseite (Backen buendig mit der Kante, Spindel und Knebel nach aussen)
    V = T(L / 2 - 0.07, -0.22, H) @ R(math.pi / 2, 'Z')
    mb.add(b_cyl(0.06, 0.0, 0.012, 20), vise_c, V)
    for a in (0.5, 2.6, 4.7):
        mb.add(b_cyl(0.008, 0.012, 0.018, 6, smooth=False), bare, V @ T(0.045 * math.cos(a), 0.045 * math.sin(a), 0))
    mb.add(chamfer(b_box((-0.045, -0.05, 0.012), (0.045, 0.09, 0.058)), 0.006), vise_c, V)
    mb.add(chamfer(b_box((-0.06, -0.07, 0.045), (0.06, -0.035, 0.095)), 0.004), vise_c, V)
    mb.add(b_box((-0.055, -0.0735, 0.068), (0.055, -0.07, 0.094)), bare, V)
    mb.add(chamfer(b_box((-0.06, -0.12, 0.045), (0.06, -0.085, 0.095)), 0.004), vise_c, V)
    mb.add(b_box((-0.055, -0.085, 0.068), (0.055, -0.0815, 0.094)), bare, V)
    mb.add(chamfer(b_box((-0.02, -0.085, 0.03), (0.02, 0.07, 0.05)), 0.002), bare, V)
    mb.add(b_cyl(0.008, 0.0, 0.07, 10), bare, V @ T(0, -0.12, 0.04) @ R(math.pi / 2, 'X'))
    mb.add(b_cyl(0.015, 0.0, 0.022, 12), vise_c, V @ T(0, -0.12, 0.04) @ R(math.pi / 2, 'X'))
    mb.add(b_cyl(0.0055, -0.09, 0.09, 8), bare, V @ T(-0.03, -0.18, 0.04) @ R(math.pi / 2, 'Y'))
    for s_ in (-1, 1):
        mb.add(b_lathe([(0.0, -0.01), (0.007, -0.008), (0.01, 0.0), (0.007, 0.008), (0.0, 0.01)], 10), bare,
               V @ T(-0.03 + s_ * 0.095, -0.18, 0.04))
    # Brecheisen (Sechskant 22 mm, Gaensehals mit Klaue, anderes Ende Meissel), rot mit Abplatzern
    pts = []
    for k in range(9):
        pts.append((-0.32 + 0.6 * k / 8, 0.0, 0.0))
    for k in range(1, 9):
        a = math.pi * 0.8 * k / 8
        pts.append((0.28 + 0.055 * math.sin(a), 0.0, 0.055 - 0.055 * math.cos(a)))
    radii = [0.011] * len(pts)
    radii[0] = 0.006
    radii[1] = 0.009
    radii[-1] = 0.006
    radii[-2] = 0.009
    B = T(-0.2, 0.12, top + 0.011) @ R(0.35, 'Z') @ R(1.45, 'X')
    mb.add(b_tube(pts, radii, 6, caps=True), bar_red, B)
    for k in range(4):
        t = rg.uniform(-0.25, 0.2)
        mb.add(b_tube([(t, 0, 0), (t + rg.uniform(0.01, 0.03), 0, 0)], 0.0113, 6, caps=False), rg.choice((bare, rust)), B)
    mb.obj('werkbank')


# =====================================================================================================================
# Krankenbett (Krankenzimmer der Gaenge: grosses Zimmer, Bett in der Mitte)
# =====================================================================================================================
def _castor(mb, M, brake, F):
    """Lenkrolle 125 mm: Drehlager, Gabel, Rad mit Gummilauf und Nabe; Bremspedal."""
    mb.add(b_cyl(0.022, 0.148, 0.176, 12), M['chrom'], F)
    mb.add(b_box((-0.03, -0.024, 0.142), (0.045, 0.024, 0.148)), M['chrom'], F)
    for s in (-1, 1):
        mb.add(b_box((0.0, s * 0.021 - 0.002, 0.05), (0.045, s * 0.021 + 0.002, 0.146)), M['chrom'], F)
    tire = [(0.012, -0.016), (0.046, -0.016), (0.056, -0.014), (0.0625, -0.008), (0.0625, 0.008), (0.056, 0.014),
            (0.046, 0.016), (0.012, 0.016)]
    W = F @ T(0.028, 0, 0.0625) @ R(math.pi / 2, 'X')
    mb.add(b_lathe(tire, 18), M['reifen'], W)
    mb.add(b_cyl(0.013, -0.018, 0.018, 10), M['nabe'], W)
    if brake:
        mb.add(chamfer(b_box((-0.075, -0.012, 0.12), (-0.02, 0.012, 0.128)), 0.002), M['pedal'], F @ R(0.0, 'Z'))


def build_hospital_bed():
    """Altes Krankenbett 2,05 x 0,95 m: Kopf- und Fussteil aus gebogenem Rohr mit Kunststofftafel, vier Lenkrollen
    (zwei mit Bremse), Winkelrahmen, Liegeflaeche mit Federbaendern, Rueckenteil 20 Grad hochgestellt, Matratze mit
    Bezug und Naht, Laken mit Falten und Ueberhang, Kopfkissen mit Mulde, gefaltete Decke am Fussende, Seitengitter
    (eines hochgeklappt, eines unten), Handkurbel am Fussende, Rammrollen, Infusionshalter, Rost am Gestell."""
    rg = rng(31)
    m = common.material
    M = dict(
        lack=m('bett_lack', (0.84, 0.83, 0.77), look='paint', r=0.65),
        chrom=m('chrom', (0.70, 0.71, 0.70), look='metal', r=0.4, m=0.9),
        rost=m('rost', (0.38, 0.22, 0.12), look='metal', r=1.45, m=0.1),
        tafel=m('bett_tafel', (0.80, 0.76, 0.66), look='plastic', r=0.75),
        kante=m('bett_kante', (0.55, 0.52, 0.46), look='plastic', r=0.8),
        reifen=m('reifen', (0.08, 0.08, 0.08), look='rubber', r=1.0),
        nabe=m('nabe', (0.45, 0.46, 0.47), look='plastic', r=0.7),
        pedal=m('pedal', (0.62, 0.14, 0.10), look='plastic', r=0.8),
        matratze=m('matratze', (0.38, 0.49, 0.50), look='plastic', r=0.85),
        naht=m('matratze_naht', (0.25, 0.32, 0.33), look='plastic', r=1.0),
        laken=m('laken', (0.86, 0.86, 0.82), look='fabric', r=1.3),
        fleck=m('laken_fleck', (0.66, 0.60, 0.46), look='fabric', r=1.3),
        kissen=m('kissen', (0.88, 0.87, 0.83), look='fabric', r=1.3),
        decke=m('wolldecke', (0.50, 0.52, 0.54), look='fabric', r=1.5),
        decke_rand=m('wolldecke_rand', (0.30, 0.33, 0.38), look='fabric', r=1.5),
        gummi=m('gummi', (0.07, 0.07, 0.07), look='rubber', r=1.0),
    )
    mb = MB()
    L, Wd = 2.05, 0.95
    xe = L / 2 - 0.045          # Kopf (-) und Fuss (+), Achse der Pfosten
    yp = 0.40                   # Pfosten / Rollen
    zr = 0.46                   # Winkelrahmen
    tr = 0.019                  # Rohrradius
    for sx in (-1, 1):
        for sy in (-1, 1):
            _castor(mb, M, sx < 0, T(sx * xe, sy * yp, 0) @ R(rg.uniform(-0.6, 0.6) + (math.pi if sx > 0 else 0), 'Z'))
    # Kopf- und Fussteil: durchgehendes Rohr (Pfosten, Bogen, Querrohr), Tafel, Querstrebe unten
    for sx, ztop in ((-1, 1.02), (1, 0.86)):
        x = sx * xe
        pts = [(x, -yp, 0.176), (x, -yp, ztop - 0.07)]
        for k in range(1, 6):
            a = math.pi / 2 * k / 6
            pts.append((x, -yp + 0.07 - 0.07 * math.cos(a), ztop - 0.07 + 0.07 * math.sin(a)))
        pts += [(x, -yp + 0.07, ztop), (x, yp - 0.07, ztop)]
        for k in range(1, 6):
            a = math.pi / 2 * k / 6
            pts.append((x, yp - 0.07 + 0.07 * math.sin(a), ztop - 0.07 + 0.07 * math.cos(a)))
        pts += [(x, yp, ztop - 0.07), (x, yp, 0.176)]
        mb.add(b_tube(pts, tr, 10, caps=True), M['lack'])
        mb.add(b_tube([(x, -yp, 0.30), (x, yp, 0.30)], 0.013, 8, caps=True), M['lack'])
        # Rost unten an den Pfosten
        for sy in (-1, 1):
            mb.add(b_cyl(tr + 0.0006, 0.176, 0.176 + rg.uniform(0.03, 0.09), 10), M['rost'], T(x, sy * yp, 0))
        # Tafel (gerundet), Kante dunkler
        z0, z1 = 0.52, ztop - 0.045
        sec = rrect(-yp + 0.03, z0, yp - 0.03, z1, 0.035, 3)
        Fp = frame(Vector((x - 0.007, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)))
        mb.add(b_prism(sec, 0.0, 0.014, ch=0.003), M['tafel'], Fp)
        for _ in range(3):  # Kratzer
            u, v = rg.uniform(-0.3, 0.2), rg.uniform(z0 + 0.05, z1 - 0.05)
            st = ccw(stroke([(u, v), (u + rg.uniform(0.05, 0.15), v + rg.uniform(-0.03, 0.03))], 0.002))
            for s in (-1, 1):
                Fq = frame(Vector((x + s * 0.0071, 0, 0)), Vector((0, s, 0)), Vector((0, 0, 1)), Vector((s, 0, 0)))
                mb.add(b_plate([(p[0] * s, p[1]) for p in st] if s < 0 else st, 0.0), M['kante'], Fq)
        # Infusionshalter am Kopfteil
        if sx < 0:
            for sy in (-1, 1):
                mb.add(b_cyl(0.012, ztop - 0.03, ztop + 0.03, 10), M['chrom'], T(x - 0.03, sy * (yp - 0.02), 0))
                mb.add(b_box((x - 0.03, sy * (yp - 0.02) - 0.004, ztop - 0.02), (x, sy * (yp - 0.02) + 0.004, ztop)), M['chrom'])
    # Rammrollen an den Kopfecken
    for sy in (-1, 1):
        mb.add(b_cyl(0.033, 0.42, 0.45, 14), M['gummi'], T(-xe - 0.005, sy * 0.44, 0))
        mb.add(b_cyl(0.008, 0.40, 0.47, 8), M['chrom'], T(-xe - 0.005, sy * 0.44, 0))
    # Winkelrahmen (L 40 x 40 x 4) laengs, Federbaender quer
    for sy in (-1, 1):
        yo = sy * (yp + 0.012)
        prof = [(0.0, 0.0), (0.004, 0.0), (0.004, 0.036), (0.04, 0.036), (0.04, 0.04), (0.0, 0.04)]
        mb.add(b_prism([(yo - sy * w, zr - 0.015 + v) for (w, v) in prof], -xe, xe), M['lack'], FYZ)
        for k in range(4):
            x = rg.uniform(-0.6, 0.8)
            y0, y1 = sorted((yo, yo + sy * 0.0006))
            mb.add(b_box((x, y0, zr - 0.012), (x + rg.uniform(0.02, 0.06), y1, zr + rg.uniform(0.0, 0.02))), M['rost'])
    xh = -0.22                  # Knick des Rueckenteils
    phi = math.radians(20)
    zp = zr + 0.025             # Liegeflaeche

    def bend(p):
        """Punkt der flachen Liegeflaeche auf das hochgestellte Rueckenteil abbilden."""
        if p.x >= xh:
            return p
        d = Vector((p.x - xh, p.y, p.z - zp))
        c, s = math.cos(phi), math.sin(phi)
        return Vector((xh + d.x * c + d.z * s, d.y, zp - d.x * s + d.z * c))
    for k in range(12):
        x = -xe + 0.08 + (2 * xe - 0.16) * k / 11
        a = bend(Vector((x - 0.015, -yp + 0.02, zp - 0.004)))
        b = bend(Vector((x + 0.015, yp - 0.02, zp)))
        if x < xh:
            bm = b_box((x - 0.015, -yp + 0.02, zp - 0.004), (x + 0.015, yp - 0.02, zp))
            for v in bm.verts:
                v.co = bend(v.co)
            mb.add(bm, M['chrom'])
        else:
            mb.add(b_box((x - 0.015, -yp + 0.02, zp - 0.004), (x + 0.015, yp - 0.02, zp)), M['chrom'])
        del a, b
    # Stuetze des Rueckenteils (Rastbuegel)
    for sy in (-1, 1):
        mb.add(b_tube([(xh - 0.05, sy * (yp - 0.05), zr), tuple(bend(Vector((-xe + 0.12, sy * (yp - 0.05), zp - 0.01))))], 0.009, 8), M['chrom'])
    # Matratze: gerundeter Querschnitt entlang x, ueber dem Knick mitgebogen, Mulde in der Mitte, Naht als Wulst
    my, mz, mr = 0.44, 0.14, 0.035
    sec = []
    for ci, (cy, cz, a0) in enumerate(((my - mr, mr, -math.pi / 2), (my - mr, mz - mr, 0.0), (-my + mr, mz - mr, math.pi / 2), (-my + mr, mr, math.pi))):
        for k in range(4):
            a = a0 + math.pi / 2 * k / 3
            sec.append((cy + mr * math.cos(a), cz + mr * math.sin(a)))
        if ci == 1:  # Oberseite quer unterteilen (Mulde)
            sec += [((my - mr) * (1 - 2 * k / 6), mz) for k in range(1, 6)]
    nx = 26
    xs = [-xe + 0.02 + (2 * xe - 0.04) * i / nx for i in range(nx + 1)]

    def mattress_pt(x, y, z):
        sag = 0.018 * max(0.0, math.cos((x - 0.25) / 0.7 * math.pi / 2)) * min(1.0, z / mz) * max(0.0, 1 - (y / my) ** 2)
        return bend(Vector((x, y, zp + z - sag)))
    bm = bmesh.new()
    V = [[bm.verts.new(mattress_pt(x, y, z)) for (y, z) in sec] for x in xs]
    ns = len(sec)
    for i in range(nx):
        for j in range(ns):
            j2 = (j + 1) % ns
            f = bm.faces.new((V[i][j], V[i][j2], V[i + 1][j2], V[i + 1][j]))
            f.smooth = True
    bm.faces.new(list(reversed(V[0])))
    bm.faces.new(V[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mb.add(bm, M['matratze'])
    for sy in (-1, 1):
        mb.add(b_tube([tuple(mattress_pt(x, sy * (my + 0.001), mz / 2)) for x in xs], 0.004, 5, caps=False), M['naht'])
    # Laken: liegt auf der Matratze, haengt seitlich ueber, Falten
    ns_, nxs = 18, 30
    P = []
    for i in range(nxs + 1):
        x = -0.75 + (xe - 0.02 + 0.75) * i / nxs
        row = []
        for j in range(ns_ + 1):
            s = -0.6 + 1.2 * j / ns_
            if abs(s) <= my - 0.02:
                y, z = s, mz + 0.006 + 0.003 * (1 + math.sin(x * 9 + s * 7) * math.sin(x * 4.1))
            else:
                over = abs(s) - (my - 0.02)
                ang = min(1.0, over / 0.05) * math.pi / 2
                arc = min(over, 0.05)
                hang = max(0.0, over - 0.05)
                y = math.copysign(my - 0.02 + 0.026 * math.sin(ang) + 0.006, s)
                z = mz + 0.004 - 0.026 * (1 - math.cos(ang)) - hang
                y += math.copysign(0.01 * hang / 0.1 * (0.5 + 0.5 * math.sin(x * 23 + s)), s)
            row.append(mattress_pt(x, y, z))
        P.append(row)
    mb.add(b_grid(P, smooth=True), M['laken'])
    for _ in range(2):  # Flecken
        x, y = rg.uniform(-0.1, 0.6), rg.uniform(-0.25, 0.25)
        bl = _blob(x, y, rg.uniform(0.04, 0.09), rg, 11, rg.uniform(0.6, 1.2), 0.35)
        bm = b_plate(bl, 0.0)
        for v in bm.verts:
            v.co = mattress_pt(v.co.x, v.co.y, mz + 0.0068 + 0.003 * (1 + math.sin(v.co.x * 9 + v.co.y * 7) * math.sin(v.co.x * 4.1)))
        mb.add(bm, M['fleck'])
    # Kopfkissen (auf dem Rueckenteil), Mulde
    P = []
    for i in range(13):
        u = -1 + 2 * i / 12
        row = []
        for j in range(11):
            v = -1 + 2 * j / 10
            row.append((u, v))
        P.append(row)

    def pillow(u, v, top_side):
        x = 0.21 * math.copysign(abs(u) ** 0.6, u)
        y = 0.31 * math.copysign(abs(v) ** 0.6, v)
        th = 0.065 * (1 - abs(u) ** 2.2) ** 0.7 * (1 - abs(v) ** 2.2) ** 0.7
        if top_side:
            th -= 0.03 * math.exp(-((u + 0.1) ** 2 + v ** 2) / 0.18)
        z = th if top_side else -th * 0.6
        return mattress_pt(-xe + 0.27 + x, y, mz + 0.06 + z)
    for top_side in (True, False):
        G = [[pillow(u, v, top_side) for (u, v) in row] for row in P]
        if not top_side:
            G = [list(reversed(r)) for r in G]
        mb.add(b_grid(G, smooth=True), M['kissen'])
    # Gefaltete Wolldecke am Fussende: drei Lagen (je 2 cm) mit runder Bruchkante zum Kopf hin, zwei Webstreifen
    zb_ = zp + mz + 0.009
    for k in range(3):
        d0 = 0.30 - k * 0.012
        sec = rrect(-d0 / 2, 0.0, d0 / 2, 0.021, 0.0095, 3)
        bmD = b_prism(sec, -0.41 + k * 0.004, 0.41 - k * 0.004)
        for v in bmD.verts:
            v.co.y += 0.003 * math.sin(v.co.z * 13 + k) * (v.co.y / 0.021)
            v.co.x += 0.006 * math.sin(v.co.z * 7 + k * 2) * (1 if v.co.x < 0 else 0.2)
        mb.add(bmD, M['decke'], frame(Vector((xe - 0.23 - k * 0.006, 0, zb_ + k * 0.02)), Vector((1, 0, 0)), Vector((0, 0, 1)), Vector((0, -1, 0))))
    for xs_ in (xe - 0.33, xe - 0.14):
        mb.add(b_plate(rect(xs_ - 0.012, -0.405, xs_ + 0.012, 0.405), 0.0), M['decke_rand'], T(0, 0, zb_ + 0.0605))
    # Seitengitter: +y hochgeklappt, -y unten
    for sy, up in ((1, True), (-1, False)):
        y = sy * 0.468
        z0, z1 = (0.64, 0.82) if up else (0.38, 0.50)
        x0, x1 = -0.82, 0.05
        for z in (z0, z1):
            mb.add(b_tube([(x0, y, z), (x1, y, z)], 0.011, 8, caps=True), M['chrom'])
        for x in (x0 + 0.02, (x0 + x1) / 2, x1 - 0.02):
            mb.add(b_tube([(x, y, z0), (x, y, z1)], 0.007, 6, caps=False), M['chrom'])
        for x in (x0 + 0.12, x1 - 0.12):
            mb.add(b_tube([(x, y, z0), (x, sy * (yp + 0.02), zr + 0.02)], 0.008, 6, caps=True), M['chrom'])
        if up:
            mb.add(b_box((x0 + 0.3, y - 0.012, z1 - 0.012), (x0 + 0.34, y + 0.012, z1 + 0.012)), M['rost'])
    # Handkurbel am Fussende (eingeklappt)
    K = T(xe + 0.02, 0.0, 0.36)
    mb.add(b_cyl(0.008, 0.0, 0.05, 8), M['chrom'], K @ R(math.pi / 2, 'Y'))
    mb.add(chamfer(b_box((0.05, -0.008, -0.12), (0.062, 0.008, 0.01)), 0.002), M['chrom'], K)
    mb.add(b_cyl(0.012, 0.0, 0.05, 10), M['kante'], K @ T(0.056, 0, -0.11) @ R(math.pi / 2, 'Y'))
    mb.obj('krankenbett')


# =====================================================================================================================
# Polsterfeld der Gummizelle (259 Stueck auf der Ebene: sparsam, aber echte Polster)
# =====================================================================================================================
def build_padded_panel():
    """Wandfeld 1,0 x 2,4 m (Wand bei x = 0, bis 9 cm tief): 2 x 4 gesteppte Kissen aus beschichtetem Segeltuch,
    Naehte in den Tiefen, Knoepfe an den Kreuzungen (Mitte und linker Rand), jedes Kissen etwas anders gewoelbt und
    unten bauchiger (Schwerkraft); Abschlussleiste oben, Sockelleiste unten, seitlich geschlossen. Nachbarfelder
    schliessen nahtlos an (Rand = Naht)."""
    rg = rng(41)
    m = common.material
    pad = m('polster', (0.84, 0.82, 0.75), look='plastic', r=0.7)  # V13.3 r4: Kunstleder glaenzt seidig (Woelbung im Streiflicht)
    pad_low = m('polster_unten', (0.70, 0.66, 0.57), look='plastic', r=0.85)  # unten abgegriffen, Flecken
    seam = m('polster_naht', (0.76, 0.74, 0.67), look='plastic', r=1.3)
    button = m('polster_knopf', (0.76, 0.74, 0.67), look='plastic', r=1.0)
    trim = m('polster_leiste', (0.55, 0.53, 0.48), look='plastic', r=0.9)
    mb = MB()
    zb, zt = 0.07, 2.36
    cols, rows = 2, 4
    ts = [0.0, 0.04, 0.11, 0.2, 0.31, 0.43, 0.57, 0.69, 0.8, 0.89, 0.96]  # V13.3 r4: feiner - runde Kissen statt Facetten
    su = sv = len(ts)
    nu, nv = cols * su, rows * sv
    us = [(ci + t) / cols for ci in range(cols) for t in ts] + [1.0]
    vs = [(cj + t) / rows for cj in range(rows) for t in ts] + [1.0]
    dmin, dmax = 0.018, 0.092  # V13.3 r4: tiefere Naehte, prallere Kissen
    amp = [[rg.uniform(0.85, 1.0) for _ in range(rows)] for _ in range(cols)]
    shift = [[rg.uniform(0.04, 0.12) for _ in range(rows)] for _ in range(cols)]

    def depth(i, j):
        ci, cj = min(cols - 1, i // su), min(rows - 1, j // sv)
        tu, tv = us[i] * cols - ci, vs[j] * rows - cj
        if tu <= 1e-6 or tu >= 1 - 1e-6 or tv <= 1e-6 or tv >= 1 - 1e-6:
            return dmin
        tv2 = min(1.0, max(0.0, tv - shift[ci][cj] * math.sin(math.pi * tv)))
        f = (math.sin(math.pi * tu) ** 0.75) * (math.sin(math.pi * tv2) ** 0.75)
        return dmin + (dmax - dmin) * f * amp[ci][cj]
    G = [[Vector((depth(i, j), -0.5 + us[i], zb + (zt - zb) * vs[j])) for j in range(nv + 1)] for i in range(nu + 1)]
    bm = bmesh.new()
    VV = [[bm.verts.new(p) for p in row] for row in G]
    for i in range(nu):
        for j in range(nv):
            f = bm.faces.new((VV[i][j], VV[i + 1][j], VV[i + 1][j + 1], VV[i][j + 1]))  # Normale +x
            f.smooth = True
            on_seam = (i % su == 0) or ((i + 1) % su == 0) or (j % sv == 0) or ((j + 1) % sv == 0)
            low = j < sv  # unterste Reihe abgegriffen (r4: Einzelflecken je Flaeche sahen wie Pixel aus - verworfen)
            f.material_index = 2 if on_seam else (1 if low else 0)
    # Seiten schliessen (Rand = Naht, 2,6 cm tief); Normalen -y bzw. +y
    for i in (0, nu):
        for j in range(nv):
            a, b = VV[i][j], VV[i][j + 1]
            a0 = bm.verts.new((0.0, a.co.y, a.co.z))
            b0 = bm.verts.new((0.0, b.co.y, b.co.z))
            f = bm.faces.new((a0, a, b, b0) if i == 0 else (a0, b0, b, a))
            f.material_index = 2
    mats = [pad, pad_low, seam]
    mi = [mb.idx(x) for x in mats]
    vm = {v: mb.bm.verts.new(v.co) for v in bm.verts}
    for f in bm.faces:
        nf = mb.bm.faces.new([vm[v] for v in f.verts])
        nf.material_index = mi[f.material_index]
        nf.smooth = f.smooth
    bm.free()
    # Knoepfe an den inneren Kreuzungen und am linken Rand
    for i in (0, su):
        for j in range(sv, nv, sv):
            y, z = -0.5 + us[i], zb + (zt - zb) * vs[j]
            mb.add(b_lathe([(0.012, -0.002), (0.0105, 0.004), (0.0, 0.008)], 7), button,
                   frame(Vector((dmin, y, z)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0))))
    # Leisten oben und unten (gerundet), seitlich buendig mit dem Feld
    for (z0, z1, d) in ((0.0, zb, 0.07), (zt, 2.4, 0.05)):
        sec = rrect(0.0, z0, d, z1, 0.008, 2)
        mb.add(b_prism([(z, x) for (x, z) in sec], -0.5, 0.5), trim,
               frame(Vector((0, 0, 0)), Vector((0, 0, 1)), Vector((1, 0, 0)), Vector((0, 1, 0))))
    mb.obj('polsterfeld')


# =====================================================================================================================
# Bilder (Wiki: weite Raeume mit Bildern an den Waenden und auf dem Boden) - eigene Malerei, Rahmen mit Profil
# =====================================================================================================================
FRAMES = {
    # Profil (w von der Aussenkante nach innen, d = Tiefe ab Rueckseite); innen Lippe und Falz fuer die Leinwand
    'gold': lambda fw: [(0.0, 0.0), (0.0, 0.026), (0.005, 0.033), (0.012, 0.037), (0.019, 0.034), (0.025, 0.029), (0.032, 0.032),
                        (0.039, 0.029), (fw - 0.005, 0.025), (fw, 0.022), (fw, 0.018), (fw - 0.012, 0.018), (fw - 0.012, 0.0)],
    'dunkel': lambda fw: [(0.0, 0.0), (0.0, 0.029), (0.004, 0.033), (fw - 0.012, 0.033), (fw - 0.004, 0.029), (fw, 0.023),
                          (fw, 0.018), (fw - 0.012, 0.018), (fw - 0.012, 0.0)],
    'schwarz': lambda fw: [(0.0, 0.0), (0.0, 0.031), (0.002, 0.033), (fw, 0.033), (fw, 0.018), (fw - 0.01, 0.018), (fw - 0.01, 0.0)],
    'eiche': lambda fw: [(0.0, 0.0), (0.0, 0.023), (0.008, 0.03), (fw - 0.007, 0.027), (fw, 0.022), (fw, 0.018), (fw - 0.012, 0.018),
                         (fw - 0.012, 0.0)],
}
FRAME_MAT = {'gold': ((0.66, 0.52, 0.28), 'metal', 0.75, 0.45), 'dunkel': ((0.25, 0.16, 0.10), 'wood', 0.8, None),
             'schwarz': ((0.07, 0.07, 0.07), 'paint', 0.7, None), 'eiche': ((0.62, 0.49, 0.32), 'wood', 0.9, None)}


def _aged(rgb):
    """Vergilbter Firnis: etwas waermer und dunkler."""
    return (min(1.0, rgb[0] * 0.95 + 0.02), min(1.0, rgb[1] * 0.92 + 0.015), min(1.0, rgb[2] * 0.80 + 0.01))


class _Canvas:
    """Malgrund: Flaechen in (y, z) der Bildebene, jede Schicht 0,4 mm ueber der vorigen; Raender pinselig."""

    def __init__(self, mb, W, H, x0, rg, tag):
        self.mb, self.W, self.H, self.x0, self.rg, self.tag = mb, W, H, x0, rg, tag
        self.layer = 0
        self.mats = {}

    def mat(self, rgb):
        rgb = _aged(rgb)
        key = tuple(round(c, 2) for c in rgb)
        if key not in self.mats:
            self.mats[key] = common.material('farbe_%02x%02x%02x' % tuple(int(c * 255) for c in key), key, look='paint', r=0.8)
        return self.mats[key]

    def brushy(self, pts, amt, step=0.025):
        out = []
        n = len(pts)
        for i in range(n):
            P, Q = Vector(pts[i]), Vector(pts[(i + 1) % n])
            d = Q - P
            k = max(1, int(d.length / step))
            nrm = Vector((-d.y, d.x)).normalized() if d.length > 1e-9 else Vector((0, 0))
            for j in range(k):
                p = P + d * (j / k)
                off = self.rg.uniform(-amt, amt) if j > 0 else 0.0
                out.append(tuple(p + nrm * off))
        return out

    def poly(self, pts, rgb, amt=0.003, clip=True):
        pts = self.brushy(pts, amt) if amt > 0 else list(pts)
        if clip:
            pts = clip_rect(pts, -self.W / 2, -self.H / 2, self.W / 2, self.H / 2)
        pts = clean_poly(pts)
        if len(pts) < 3 or abs(area2(pts)) < 1e-7:
            return
        self.mb.add(b_plate(ccw(pts), 0.0), self.mat(rgb),
                    frame(Vector((self.x0 + 0.0004 * self.layer, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0))))
        self.layer += 1

    def stroke(self, pts, w, rgb, amt=0.0015):
        self.poly(stroke(pts, w), rgb, min(amt, w * 0.12))

    def blob(self, cx, cy, r, rgb, squash=1.0, n=12, spike=0.25):
        self.poly(_blob(cx, cy, r, self.rg, n, squash, spike), rgb, 0.0)

    def bands(self, y0, y1, colors, wave=0.01, strokes=True):
        """Verlauf aus waagrechten Farbbaendern von oben (y1) nach unten (y0): Stuetzfarben werden je zweimal
        unterteilt, Grenzen wellig, darueber kurze Pinselstriche in Nachbartoenen."""
        keys = colors
        cols = []
        for i in range(len(keys) - 1):
            for t in (0.0, 0.34, 0.67):
                cols.append(tuple(keys[i][k] * (1 - t) + keys[i + 1][k] * t for k in range(3)))
        cols.append(keys[-1])
        n = len(cols)
        hw = self.W / 2 + 0.02
        for i, c in enumerate(cols):
            top = y1 - (y1 - y0) * i / n
            bot = y1 - (y1 - y0) * (i + 1) / n - (0.012 if i < n - 1 else 0)
            ph = self.rg.uniform(0, 6)
            pts = [(-hw, bot)] + [(-hw + 2 * hw * k / 10, bot + wave * math.sin(k * 1.1 + ph)) for k in range(1, 10)] + [(hw, bot), (hw, top), (-hw, top)]
            self.poly(pts, c, 0.0)
        if strokes:
            for _ in range(int(14 * self.W * (y1 - y0) / max(self.H, 0.1) + 6)):
                z = self.rg.uniform(y0, y1)
                i = min(n - 1, int((y1 - z) / (y1 - y0) * n))
                c = tuple(min(1.0, v * self.rg.uniform(0.94, 1.07)) for v in cols[i])
                y = self.rg.uniform(-self.W / 2, self.W / 2)
                ln = self.rg.uniform(0.04, 0.14) * self.W
                self.stroke([(y, z), (y + ln, z + self.rg.uniform(-0.006, 0.006))], self.rg.uniform(0.006, 0.016), c)

    def texture(self, x0, y0, x1, y1, rgb, count, w=(0.005, 0.014), ang=0.0):
        """Pinselstruktur: kurze Striche in Nachbartoenen ueber einer Flaeche."""
        for _ in range(count):
            y, z = self.rg.uniform(x0, x1), self.rg.uniform(y0, y1)
            ln = self.rg.uniform(0.02, 0.08)
            a = ang + self.rg.uniform(-0.3, 0.3)
            c = tuple(min(1.0, v * self.rg.uniform(0.9, 1.1)) for v in rgb)
            self.stroke([(y, z), (y + ln * math.cos(a), z + ln * math.sin(a))], self.rg.uniform(*w), c)


def _scene_landschaft(c, variant):
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    zh = -hh + H * (0.42 if variant != 'berge' else 0.5)
    c.bands(zh - 0.02, hh + 0.02, [(0.50, 0.58, 0.68), (0.58, 0.64, 0.70), (0.68, 0.70, 0.70), (0.78, 0.75, 0.66), (0.84, 0.78, 0.62)], 0.008)
    for _ in range(rg.randint(3, 5)):
        cx, cy = rg.uniform(-hw * 0.9, hw * 0.9), rg.uniform(zh + H * 0.15, hh - H * 0.08)
        for k in range(3):
            c.blob(cx + rg.uniform(-0.04, 0.04) * W, cy + rg.uniform(-0.01, 0.01), rg.uniform(0.03, 0.07) * W, (0.88, 0.86, 0.80), 0.45)
    ridges = [(0.46, 0.52, 0.60), (0.40, 0.48, 0.42), (0.36, 0.44, 0.27)] if variant == 'berge' else [(0.50, 0.55, 0.60), (0.42, 0.48, 0.32)]
    for k, col in enumerate(ridges):
        base = zh - k * H * 0.06
        amp = H * (0.22 if (variant == 'berge' and k == 0) else 0.07)
        ph = rg.uniform(0, 6)
        ridge = []
        for i in range(25):
            y = -hw - 0.02 + (W + 0.04) * i / 24
            top = base + amp * (0.5 + 0.5 * math.sin(i * 0.47 + ph)) * (0.6 + 0.4 * math.sin(i * 1.27 + ph * 2))
            ridge.append((y, top))
        c.poly([(-hw - 0.02, -hh - 0.02)] + ridge + [(hw + 0.02, -hh - 0.02)], col, 0.0)
        if variant == 'berge' and k == 0:  # Schnee auf den beiden hoechsten Gipfeln, folgt der Kammlinie
            peaks = [i for i in range(1, 24) if ridge[i][1] >= ridge[i - 1][1] and ridge[i][1] >= ridge[i + 1][1]]
            for i in sorted(peaks, key=lambda i: -ridge[i][1])[:2]:
                zc = ridge[i][1]
                cap = [p for p in ridge[max(0, i - 3):i + 4] if p[1] > zc - H * 0.07]
                if len(cap) < 2:
                    continue
                low = zc - H * 0.07
                bottom = [(cap[-1][0] - (cap[-1][0] - cap[0][0]) * t, low + H * 0.02 * math.sin(t * 17 + i)) for t in (0.2, 0.4, 0.6, 0.8)]
                c.poly([(cap[0][0], max(low, cap[0][1] - 0.002))] + cap[1:-1] + [(cap[-1][0], max(low, cap[-1][1] - 0.002))] + bottom,
                       (0.86, 0.86, 0.84), 0.0)
    # Feld mit Furchen zum Fluchtpunkt
    fz = zh - H * 0.12
    c.poly([(-hw - 0.02, -hh - 0.02), (hw + 0.02, -hh - 0.02), (hw + 0.02, fz), (-hw - 0.02, fz + 0.01)], (0.56, 0.52, 0.30), 0.004)
    c.texture(-hw, -hh, hw, fz, (0.56, 0.52, 0.30), int(30 * W))
    vp = (W * 0.1, fz + 0.02)
    for i in range(-8, 9):
        c.stroke([(vp[0] + i * 0.01 * W, vp[1] - 0.005), (vp[0] + i * 0.16 * W, -hh - 0.02)], 0.004 + 0.0004 * abs(i),
                 (0.48, 0.44, 0.24) if i % 2 else (0.62, 0.58, 0.36))
    if variant == 'see':
        c.blob(-W * 0.18, zh - H * 0.08, W * 0.2, (0.46, 0.56, 0.62), 0.18, 16, 0.1)
        c.stroke([(-W * 0.3, zh - H * 0.08), (-W * 0.08, zh - H * 0.085)], 0.003, (0.78, 0.80, 0.78))
    # Weg
    c.poly([(-W * 0.05, -hh - 0.02), (W * 0.12, -hh - 0.02), (W * 0.08, fz - H * 0.1), (W * 0.09, fz), (W * 0.07, fz),
            (W * 0.03, fz - H * 0.12)], (0.70, 0.62, 0.46), 0.003)
    # Baeume
    for k in range({'haus': 3, 'berge': 4, 'see': 2, 'baum': 1}[variant]):
        x = rg.uniform(-hw * 0.85, hw * 0.85) if variant != 'baum' else -W * 0.22
        s = rg.uniform(0.7, 1.2) * (1.8 if variant == 'baum' else 1.0)
        zb = fz - rg.uniform(0.0, 0.08) * H
        c.stroke([(x, zb), (x + 0.004, zb + 0.09 * H * s)], 0.008 * s, (0.25, 0.18, 0.12))
        for j in range(4):
            c.blob(x + rg.uniform(-0.025, 0.025) * s, zb + (0.1 + rg.uniform(0, 0.08)) * H * s, rg.uniform(0.025, 0.04) * s,
                   rg.choice([(0.24, 0.34, 0.18), (0.32, 0.42, 0.22), (0.20, 0.28, 0.16)]), 1.0)
    if variant == 'haus':
        x, z = W * 0.26, zh - H * 0.03
        c.poly(rect(x, z, x + 0.06, z + 0.04), (0.84, 0.80, 0.70), 0.001)
        c.poly([(x - 0.008, z + 0.04), (x + 0.068, z + 0.04), (x + 0.03, z + 0.07)], (0.55, 0.22, 0.15), 0.001)
        c.poly(rect(x + 0.024, z, x + 0.036, z + 0.022), (0.30, 0.22, 0.16), 0.0)


def _scene_meer(c, variant):
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    zh = -hh + H * 0.45
    sky = [(0.56, 0.58, 0.62), (0.66, 0.64, 0.62), (0.78, 0.70, 0.60), (0.88, 0.76, 0.58)] if variant == 'abend' else \
        [(0.42, 0.45, 0.48), (0.50, 0.52, 0.54), (0.58, 0.60, 0.60), (0.64, 0.66, 0.64)]
    c.bands(zh, hh + 0.02, sky, 0.006)
    if variant == 'abend':
        c.blob(W * 0.18, zh + H * 0.12, H * 0.07, (0.96, 0.84, 0.58), 1.0, 18, 0.03)
    else:
        for _ in range(5):
            c.blob(rg.uniform(-hw, hw), rg.uniform(zh + H * 0.2, hh), rg.uniform(0.06, 0.12) * W, (0.36, 0.38, 0.42), 0.35)
    c.poly([(-hw - 0.02, zh), (-hw * 0.3, zh), (-hw * 0.45, zh + H * 0.05), (-hw * 0.7, zh + H * 0.07), (-hw - 0.02, zh + H * 0.04)],
           (0.38, 0.42, 0.46), 0.002)
    sea = [(0.30, 0.40, 0.46), (0.26, 0.36, 0.42), (0.22, 0.31, 0.37), (0.18, 0.26, 0.31)] if variant == 'abend' else \
        [(0.26, 0.32, 0.34), (0.22, 0.28, 0.30), (0.18, 0.24, 0.26), (0.15, 0.20, 0.22)]
    c.bands(-hh - 0.02, zh, sea, 0.004)
    c.texture(-hw, -hh, hw, zh - 0.01, sea[1], int(25 * W), (0.004, 0.01))
    for _ in range(int(40 * W)):
        y, z = rg.uniform(-hw, hw), rg.uniform(-hh, zh - 0.01)
        ln = (0.01 + 0.04 * (zh - z) / H) * W
        c.stroke([(y, z), (y + ln, z + rg.uniform(-0.002, 0.002))], 0.002 + 0.004 * (zh - z) / H, (0.62, 0.68, 0.70) if variant == 'abend' else (0.52, 0.58, 0.58))
    if variant == 'abend':
        for k in range(10):
            z = zh - 0.01 - k * H * 0.04
            c.stroke([(W * 0.18 - 0.02 * W * (1 + k * 0.2), z), (W * 0.18 + 0.02 * W * (1 + k * 0.2), z)], 0.004, (0.92, 0.80, 0.56))
    # Segelboot
    x, z = -W * 0.12, zh - H * 0.08
    c.poly([(x - 0.05 * W, z), (x + 0.06 * W, z), (x + 0.045 * W, z - 0.02 * H), (x - 0.04 * W, z - 0.02 * H)], (0.20, 0.16, 0.14), 0.001)
    c.stroke([(x, z), (x, z + 0.3 * H)], 0.003, (0.28, 0.22, 0.18))
    c.poly([(x + 0.003, z + 0.02 * H), (x + 0.003, z + 0.29 * H), (x + 0.07 * W, z + 0.03 * H)], (0.88, 0.85, 0.76), 0.001)
    c.poly([(x - 0.003, z + 0.04 * H), (x - 0.003, z + 0.22 * H), (x - 0.04 * W, z + 0.05 * H)], (0.80, 0.77, 0.68), 0.001)


def _scene_stillleben(c):
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    c.poly(rect(-hw - 0.02, -hh - 0.02, hw + 0.02, hh + 0.02), (0.20, 0.17, 0.13), 0.0)
    c.texture(-hw, -hh * 0.3, hw, hh, (0.22, 0.18, 0.14), 30, (0.008, 0.02), 0.8)
    c.blob(-W * 0.05, H * 0.08, W * 0.32, (0.30, 0.25, 0.18), 1.2, 16, 0.15)
    zt = -hh + H * 0.32
    c.poly(rect(-hw - 0.02, -hh - 0.02, hw + 0.02, zt), (0.40, 0.28, 0.18), 0.002)
    c.poly(rect(-hw - 0.02, zt - 0.012, hw + 0.02, zt), (0.50, 0.36, 0.24), 0.001)
    c.poly([(-hw * 0.9, zt + 0.005), (hw * 0.2, zt + 0.01), (hw * 0.35, zt - 0.08), (hw * 0.25, -hh - 0.02), (-hw * 0.6, -hh - 0.02),
            (-hw * 0.95, zt - 0.1)], (0.84, 0.82, 0.76), 0.004)
    for k in range(5):
        y = -hw * 0.7 + k * hw * 0.22
        c.stroke([(y, zt - 0.005), (y + rg.uniform(-0.03, 0.03), -hh * 0.75)], 0.004, (0.66, 0.64, 0.60))
    # Vase (Umriss eines Drehkoerpers)
    vx, vz, vh = -W * 0.12, zt, H * 0.38
    prof = [(0.0, 0.0), (0.09, 0.0), (0.13, 0.12), (0.15, 0.35), (0.11, 0.6), (0.06, 0.78), (0.07, 0.95), (0.09, 1.0)]
    pts = [(vx + p[0] * W, vz + p[1] * vh) for p in prof] + [(vx - p[0] * W, vz + p[1] * vh) for p in reversed(prof)]
    c.poly(pts, (0.30, 0.38, 0.52), 0.001)
    c.stroke([(vx + 0.05 * W, vz + 0.18 * vh), (vx + 0.07 * W, vz + 0.5 * vh)], 0.008, (0.60, 0.66, 0.74))
    for k in range(7):  # Blumen
        a = -0.9 + 1.8 * k / 6
        tip = (vx + math.sin(a) * W * 0.18, vz + vh + H * rg.uniform(0.08, 0.18))
        c.stroke([(vx, vz + vh * 0.95), tip], 0.003, (0.28, 0.36, 0.18))
        c.blob(tip[0], tip[1], W * 0.035, rg.choice([(0.76, 0.30, 0.22), (0.86, 0.74, 0.40), (0.82, 0.80, 0.74)]), 1.0, 9, 0.35)
    for k, col in enumerate([(0.70, 0.30, 0.16), (0.78, 0.62, 0.22), (0.44, 0.50, 0.20)]):
        fx, fz = W * (0.12 + 0.11 * k), zt + W * 0.045
        c.blob(fx, fz, W * 0.05, col, 1.0, 14, 0.04)
        c.poly([(fx - W * 0.05, fz), (fx + W * 0.05, fz), (fx + W * 0.03, fz - W * 0.035), (fx - W * 0.03, fz - W * 0.035)],
               tuple(v * 0.6 for v in col), 0.001)
        c.blob(fx - W * 0.015, fz + W * 0.02, W * 0.012, (0.92, 0.88, 0.76), 1.0, 7, 0.1)


def _scene_abstrakt(c):
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    c.poly(rect(-hw - 0.02, -hh - 0.02, hw + 0.02, hh + 0.02), (0.50, 0.20, 0.16), 0.0)
    fields = [((0.78, 0.46, 0.20), 0.56, 0.95), ((0.30, 0.10, 0.10), 0.26, 0.5), ((0.72, 0.58, 0.30), 0.05, 0.2)]
    for col, z0, z1 in fields:
        pts = rect(-hw * 0.82, -hh + H * z0, hw * 0.82, -hh + H * z1)
        c.poly(pts, col, 0.008)
        for _ in range(6):
            y = rg.uniform(-hw * 0.8, hw * 0.8)
            zz = -hh + H * rg.uniform(z0, z1)
            c.stroke([(y, zz), (y + rg.uniform(0.05, 0.2) * W, zz + rg.uniform(-0.01, 0.01))], rg.uniform(0.01, 0.03),
                     tuple(min(1, v * rg.uniform(0.9, 1.12)) for v in col))
    for _ in range(4):
        y = rg.uniform(-hw * 0.7, hw * 0.7)
        z = -hh + H * 0.56
        c.stroke([(y, z), (y + 0.002, z - rg.uniform(0.02, 0.08))], 0.004, fields[0][0])


def _scene_konstrukt(c):
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    c.poly(rect(-hw - 0.02, -hh - 0.02, hw + 0.02, hh + 0.02), (0.84, 0.80, 0.70), 0.0)
    c.texture(-hw, -hh, hw, hh, (0.84, 0.80, 0.70), 40, (0.008, 0.02))
    c.poly(rot2(rect(-hw * 0.9, -0.02, hw * 0.6, 0.03), 0.35), (0.12, 0.12, 0.12), 0.003)
    c.poly(rot2(rect(-0.015, -hh * 0.8, 0.02, hh * 0.5), -0.15), (0.12, 0.12, 0.12), 0.003)
    c.blob(W * 0.18, H * 0.2, W * 0.16, (0.72, 0.18, 0.14), 1.0, 24, 0.02)
    c.poly([(-hw * 0.7, -hh * 0.75), (-hw * 0.1, -hh * 0.75), (-hw * 0.45, -hh * 0.15)], (0.18, 0.28, 0.52), 0.003)
    c.poly(rect(hw * 0.35, -hh * 0.7, hw * 0.75, -hh * 0.3), (0.86, 0.68, 0.18), 0.003)
    c.poly(rot2(rect(-hw * 0.8, hh * 0.55, -hw * 0.2, hh * 0.62), -0.1), (0.72, 0.18, 0.14), 0.002)


def _scene_interieur(c):
    """Gang in Zentralperspektive mit Tuer am Ende - ein Bild im Bild der Lagerdecks."""
    rg, W, H = c.rg, c.W, c.H
    hw, hh = W / 2, H / 2
    vp = (W * 0.04, H * 0.06)
    ex0, ex1, ez0, ez1 = vp[0] - W * 0.12, vp[0] + W * 0.12, vp[1] - H * 0.12, vp[1] + H * 0.1
    c.poly([(-hw - 0.02, hh + 0.02), (hw + 0.02, hh + 0.02), (ex1, ez1), (ex0, ez1)], (0.70, 0.68, 0.60), 0.002)       # Decke
    c.poly([(-hw - 0.02, -hh - 0.02), (ex0, ez0), (ex1, ez0), (hw + 0.02, -hh - 0.02)], (0.46, 0.38, 0.24), 0.002)     # Boden
    c.poly([(-hw - 0.02, hh + 0.02), (ex0, ez1), (ex0, ez0), (-hw - 0.02, -hh - 0.02)], (0.66, 0.58, 0.36), 0.002)     # links
    c.poly([(hw + 0.02, hh + 0.02), (hw + 0.02, -hh - 0.02), (ex1, ez0), (ex1, ez1)], (0.74, 0.66, 0.42), 0.002)      # rechts
    c.texture(-hw, ez0, ex0, ez1, (0.66, 0.58, 0.36), 18, (0.004, 0.01), 1.5)
    c.texture(ex1, ez0, hw, ez1, (0.74, 0.66, 0.42), 18, (0.004, 0.01), 1.5)
    c.poly(rect(ex0, ez0, ex1, ez1), (0.62, 0.56, 0.38), 0.001)                                                       # Stirnwand
    c.poly(rect(vp[0] - W * 0.035, ez0, vp[0] + W * 0.035, ez0 + H * 0.13), (0.16, 0.13, 0.10), 0.001)              # Tuer
    c.stroke([(vp[0] + W * 0.03, ez0 + 0.002), (vp[0] + W * 0.03, ez0 + H * 0.125)], 0.003, (0.95, 0.90, 0.70))
    for t in (0.3, 0.62):
        cy = hh + (ez1 - hh) * t
        w = (hw - (ex1 - ex0) / 2) * (1 - t) + (ex1 - ex0) / 2
        c.poly(rect(vp[0] - w * 0.25, cy - 0.006 * (1 - t) - 0.002, vp[0] + w * 0.25, cy + 0.006 * (1 - t) + 0.002), (0.96, 0.94, 0.82), 0.0)
    for k in range(1, 5):  # Fugen im Boden
        t = k / 5
        c.stroke([(-hw + (ex0 + hw) * t, -hh + (ez0 + hh) * t), (hw - (hw - ex1) * t, -hh + (ez0 + hh) * t)], 0.002, (0.36, 0.30, 0.18))
    # Riss in der Leinwand (aufgebogene Ecke)
    tx, tz = -W * 0.22, -H * 0.22
    c.poly([(tx, tz), (tx + 0.05, tz + 0.004), (tx + 0.05, tz + 0.007), (tx + 0.003, tz + 0.004), (tx + 0.004, tz + 0.04), (tx, tz + 0.04)],
           (0.05, 0.04, 0.03), 0.0)


def _painting(W, H, seed, scene, style, lean=False, flat=False, extra=None):
    """Bild W x H (Sicht) im Rahmen mit Profil und Gehrungen, Leinwand im Falz, Malerei aus vielen Schichten mit
    pinseligen Raendern, vergilbter Firnis. Wandbild: Rueckseite bei x = 0, Mitte im Ursprung, haengt an einem Draht
    an einem Haken und kippt oben 1,5 Grad von der Wand; Staub auf der Oberkante, Abrieb an den Ecken, eine
    Gehrungsfuge offen. lean: unten auf dem Boden an die Wand gelehnt (8 Grad), Unterkante 6 cm vor der Wand.
    V13.3: scene darf eine Funktion f(canvas) sein; flat: liegt flach auf dem Boden (Bild nach oben, Oberkante des Bildes
    zeigt nach -x, Mitte im Ursprung, ohne Aufhaengung - KREATUR_GameMaster.md); extra(mb, x0) ergaenzt Teile auf der
    Bildebene vor dem Umlegen (x0 = Ebene der Leinwand)."""
    rg = rng(seed)
    mb = MB()
    fw = (0.055 if W > 0.8 else 0.045) if style != 'schwarz' else 0.035
    prof = FRAMES[style](fw)
    col, look, r_, m_ = FRAME_MAT[style]
    fmat = common.material(f'rahmen_{style}', col, look=look, r=r_, m=m_)
    worn = common.material(f'rahmen_{style}_ab', tuple(c * 0.55 for c in col), look='wood', r=1.1)
    dust = common.material('staub', (0.55, 0.53, 0.49), look='fabric', r=1.5)
    canvas_m = common.material('leinwand', (0.80, 0.76, 0.66), look='fabric', r=1.3)
    Wo, Ho = W / 2 + fw, H / 2 + fw
    X = Vector((1, 0, 0))
    open_corner = rg.randrange(4)
    sides = [(Vector((0, 0, 1)), Vector((0, 1, 0)), Ho, Wo), (Vector((0, 0, -1)), Vector((0, -1, 0)), Ho, Wo),
             (Vector((0, 1, 0)), Vector((0, 0, -1)), Wo, Ho), (Vector((0, -1, 0)), Vector((0, 0, 1)), Wo, Ho)]
    for si, (o, a, E, Lh) in enumerate(sides):
        bm = bmesh.new()
        g0 = 0.0012 if si == open_corner else 0.0
        A = [bm.verts.new(o * (E - w) + a * (-(Lh - w) + g0) + X * d) for (w, d) in prof]
        B = [bm.verts.new(o * (E - w) + a * (Lh - w) + X * d) for (w, d) in prof]
        n = len(prof)
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((A[i], A[j], B[j], B[i]))
        bm.faces.new(list(reversed(A)))
        bm.faces.new(B)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        mb.add(bm, fmat)
        # Abrieb an den Ecken (vorn, auf der hoechsten Stelle des Profils)
        wmax, dmax = max(prof, key=lambda p: p[1])
        for sgn in (-1, 1):
            if rg.random() < 0.6:
                Fc = frame(o * (E - wmax) + a * (sgn * (Lh - wmax - rg.uniform(0.0, 0.02))) + X * (dmax + 0.0004), a, o, X)
                mb.add(b_plate(_blob(0.0, 0.0, rg.uniform(0.004, 0.009), rg, 7, 0.7), 0.0), worn, Fc)
    # Staub auf der Oberkante
    mb.add(b_plate(rect(0.004, -Wo + 0.01, max(p[1] for p in prof) - 0.004, Wo - 0.01), 0.0), dust,
           frame(Vector((0, 0, Ho + 0.0004)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))))
    # Leinwand im Falz
    x0 = 0.014
    mb.add(b_plate(rect(-W / 2 - 0.008, -H / 2 - 0.008, W / 2 + 0.008, H / 2 + 0.008), 0.0), canvas_m,
           frame(Vector((x0, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0))))
    c = _Canvas(mb, W, H, x0 + 0.0004, rg, seed)
    if callable(scene):
        scene(c)
    elif scene.startswith('landschaft'):
        _scene_landschaft(c, scene.split(':')[1])
    elif scene.startswith('meer'):
        _scene_meer(c, scene.split(':')[1])
    elif scene == 'stillleben':
        _scene_stillleben(c)
    elif scene == 'abstrakt':
        _scene_abstrakt(c)
    elif scene == 'konstrukt':
        _scene_konstrukt(c)
    else:
        _scene_interieur(c)
    if extra is not None:
        extra(mb, x0 + 0.0004 * (c.layer + 2))
    if flat:
        for v in mb.bm.verts:
            v.co = R(-math.pi / 2, 'Y') @ v.co
        mb.obj('bild')
        return
    if lean:
        th = math.radians(8.0)
        for v in mb.bm.verts:
            v.co = T(2 * Ho * math.sin(th) + 0.001, 0, 0) @ R(-th, 'Y') @ T(0, 0, Ho) @ v.co
        mb.obj('bild')
        return
    # Wandbild kippt oben 1,5 Grad von der Wand (Unterkante liegt an); Aufhaengung: Oesen hinten an den Seiten,
    # Draht zum Haken ueber dem Bild
    Mt = T(0, 0, -Ho) @ R(math.radians(1.5), 'Y') @ T(0, 0, Ho)
    for v in mb.bm.verts:
        v.co = Mt @ v.co
    mb.obj('bild')
    brass = common.material('messing', (0.62, 0.50, 0.30), look='metal', r=0.7, m=0.8)
    wire = common.material('bilderdraht', (0.55, 0.55, 0.52), look='metal', r=0.8, m=0.7)
    hk = MB()
    hz = Ho + 0.07
    hk.add(b_box((0.0, -0.008, hz - 0.012), (0.0015, 0.008, hz + 0.012)), brass)
    hk.add(b_cyl(0.003, 0.0015, 0.004, 6), brass, T(0, 0, hz + 0.006) @ R(math.pi / 2 - 0.3, 'Y'))
    hk.add(b_tube([(0.0015, 0.0, hz - 0.012), (0.006, 0.0, hz - 0.006), (0.004, 0.0, hz - 0.001)], 0.0014, 5), brass)
    for sgn in (-1, 1):
        eye = Mt @ Vector((0.006, sgn * (Wo - 0.04), Ho * 0.45))
        hk.add(b_tube([tuple(eye), (0.004, sgn * 0.003, hz - 0.011)], 0.0009, 4, caps=False), wire)
    hk.obj('aufhaengung')


def build_painting_a():
    _painting(1.1, 0.75, 31, 'landschaft:haus', 'gold')


def build_painting_b():
    _painting(0.9, 0.6, 32, 'meer:abend', 'dunkel')


def build_painting_c():
    _painting(0.6, 0.75, 33, 'stillleben', 'gold')


def build_painting_d():
    _painting(0.8, 0.8, 34, 'konstrukt', 'schwarz')


def build_painting_e():
    _painting(1.4, 0.9, 35, 'landschaft:berge', 'eiche')


def build_painting_f():
    _painting(0.5, 0.7, 36, 'interieur', 'dunkel')


def build_painting_lean_a():
    _painting(1.0, 0.7, 41, 'landschaft:baum', 'dunkel', lean=True)


def build_painting_lean_b():
    _painting(0.7, 0.9, 42, 'abstrakt', 'schwarz', lean=True)


def build_painting_lean_c():
    _painting(0.8, 0.6, 43, 'meer:grau', 'eiche', lean=True)
