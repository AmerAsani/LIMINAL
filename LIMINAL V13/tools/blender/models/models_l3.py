"""V13.3 Level 3 "Electrical Station" (LEVELSPEC_3.md, LOOK_3.md, PROPS_Level3.md): alles neu gebaut.

Ziegelgaenge mit weissen, rostigen Gittern, Elektrik (Schaltschraenke, Generatoren, Kabeltrassen), Rohre mit schwarzer
Fluessigkeit und Schlammlachen, Kesselraeume, Fertigungsstrassen (Foerderbaender, Fenster mit Ziegel dahinter,
Gitterrosttreppe) und das Sanktuarium (Engelsstatue, Saeulen, Tuecher, Bilder vom Engel).

Konventionen (wie das Spiel): Meter, z oben, Blick entlang +x. Bodenmodelle: Ursprung am Boden; Wandmodelle:
Wandflaeche x = 0; Deckenmodelle: Decke z = 0; Modelle im Durchbruch: Ursprung in der Mitte der Oeffnung auf dem
Boden, Laibung entlang x (1 m). Das Spiel spiegelt y beim Laden (glb_import) - was in Blender bei +y liegt, sieht
der Betrachter vor dem Modell (bei +x) rechts.

Werkzeuge in diesem Modul:
- Kit: sammelt Geometrie je Material in einem bmesh (schnell, wenige Objekte); Grundkoerper als bmesh-Bausteine
  (Kasten mit Fase, Zylinder, Drehkoerper, Rohr entlang eines Pfads, Prisma), Schrauben, Nieten, Schweissnaehte,
  Flecken (Rost, Abplatzer, Staub, Schmutz) als eigene Flaechen knapp ueber der Oberflaeche.
- Skulpturen (Engel, Torso, Schaedel, Schlamm) mit dem Distanzfeld-Werkzeug der Wesen (creatures/sdf.py,
  OpenVDB), danach Reduktion und Materialien nach Hoehlung (Schmutz in Falten, Staub oben).
- Tuecher mit der Stoffsimulation von Blender (Tuch faellt ueber verdeckte Moebel).
"""
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector, noise

import common

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'creatures'))
import sdf  # noqa: E402

TAU = 2.0 * math.pi


# =====================================================================================================================
# Materialien (Rauheit r = Faktor auf die Textur im Spiel, m = Metall-Zuschlag; bewusst je Material gesetzt)
# =====================================================================================================================
def mat(name, rgb, look='paint', tex=None, glow=0.0, r=None, m=None):
    return common.material(name, rgb, look=look, tex=tex, glow=glow, r=r, m=m,
                           rough=0.5 if r is None else min(1.0, 0.45 * r), metal=0.9 if look == 'metal' else 0.0)


def _pal():
    return {
        # Gitter: weiss lackiert (Textur mit Abplatzern), Rost in Stufen
        'bar': mat('gitterlack', (0.92, 0.91, 0.86), 'paint', tex='painted_metal_white', r=1.05),
        'bar_dirty': mat('gitterlack_schmutz', (0.70, 0.67, 0.60), 'paint', tex='painted_metal_white', r=1.25),
        'rust': mat('rost', (0.95, 0.88, 0.82), 'metal', tex='metal_rusty_iron', r=1.3, m=0.15),
        'rust_dark': mat('rost_dunkel', (0.55, 0.45, 0.38), 'metal', tex='metal_rusty_iron', r=1.4, m=0.1),
        'rust_light': mat('rost_hell', (1.0, 0.80, 0.62), 'metal', tex='metal_rusty_iron', r=1.35, m=0.05),
        'steel': mat('stahl', (0.50, 0.51, 0.52), 'metal', r=0.75),
        'steel_dark': mat('stahl_dunkel', (0.22, 0.22, 0.23), 'metal', r=0.9),
        'steel_bright': mat('stahl_blank', (0.70, 0.70, 0.70), 'metal', r=0.45),
        'galv': mat('verzinkt', (0.66, 0.67, 0.66), 'galvanized', r=1.0),
        'galv_dark': mat('verzinkt_alt', (0.46, 0.47, 0.46), 'galvanized', r=1.2),
        'zinc_white': mat('zinkbluete', (0.80, 0.80, 0.76), 'concrete', r=1.4),
        'black_rubber': mat('gummi_schwarz', (0.035, 0.035, 0.037), 'rubber', r=1.0),
        'black_plastic': mat('kunststoff_schwarz', (0.04, 0.04, 0.045), 'plastic', r=1.3),
        'grey_plastic': mat('kunststoff_grau', (0.42, 0.43, 0.44), 'plastic', r=1.4),
        'white_plastic': mat('kunststoff_weiss', (0.82, 0.81, 0.77), 'plastic', r=1.3),
        'dust': mat('staub', (0.52, 0.50, 0.46), 'concrete', r=1.5),
        'dust_light': mat('staub_hell', (0.66, 0.64, 0.60), 'concrete', r=1.5),
        'grime': mat('schmutz', (0.17, 0.14, 0.11), 'paint', r=1.4),
        'oil': mat('oel', (0.03, 0.025, 0.02), 'water', r=0.6),
        'sludge': mat('schlamm_nass', (0.010, 0.009, 0.008), 'water', r=1.0),
        'sludge_skin': mat('schlamm_haut', (0.016, 0.014, 0.012), 'water', r=1.7),
        'crust': mat('schlamm_kruste', (0.040, 0.034, 0.028), 'concrete', r=1.5),
        'hole': mat('loch', (0.012, 0.012, 0.012), 'plastic', r=1.5),
        'glass': mat('glas', (0.42, 0.46, 0.46), 'glass'),
        'glass_dirty': mat('glas_schmutz', (0.30, 0.30, 0.27), 'glass', r=4.0),
    }


# =====================================================================================================================
# Kit: Geometrie je Material sammeln
# =====================================================================================================================
class Kit:
    def __init__(self, deg=40.0):
        self.parts = {}
        self.deg = deg
        self.degs = {}

    def bm(self, m):
        e = self.parts.get(m.name)
        if e is None:
            b = bmesh.new()
            b.loops.layers.uv.new('UVMap')
            e = self.parts[m.name] = (m, b)
        return e[1]

    def add(self, m, src, M=None):
        """bmesh src (wird freigegeben) mit Matrix M zum Material m."""
        if M is not None:
            bmesh.ops.transform(src, matrix=M, verts=src.verts)
            if M.to_3x3().determinant() < 0:
                bmesh.ops.reverse_faces(src, faces=src.faces)
        dst = self.bm(m)
        uvs = src.loops.layers.uv.active
        uvd = dst.loops.layers.uv.active
        vm = {v: dst.verts.new(v.co) for v in src.verts}
        for f in src.faces:
            try:
                nf = dst.faces.new([vm[v] for v in f.verts])
            except ValueError:
                continue
            if uvs is not None:
                for a, b in zip(f.loops, nf.loops):
                    b[uvd].uv = a[uvs].uv
        src.free()
        return self

    def add_split(self, src, pick, M=None):
        """Flaechen von src je nach pick(face) -> Material verteilen (z. B. Staub oben)."""
        if M is not None:
            bmesh.ops.transform(src, matrix=M, verts=src.verts)
        src.normal_update()
        groups = {}
        for f in src.faces:
            m = pick(f)
            if m is not None:
                groups.setdefault(m.name, (m, []))[1].append(f)
        for name, (m, faces) in groups.items():
            dst = self.bm(m)
            vm = {}
            for f in faces:
                vs = []
                for v in f.verts:
                    if v not in vm:
                        vm[v] = dst.verts.new(v.co)
                    vs.append(vm[v])
                try:
                    dst.faces.new(vs)
                except ValueError:
                    pass
        src.free()

    # --- Bequemlichkeit ---
    def box(self, m, lo, hi, bev=0.0, seg=1, M=None):
        return self.add(m, bm_box(lo, hi, bev, seg), M)

    def cyl(self, m, a, b, r, seg=12, r2=None, caps=True):
        M, L = along(a, b)
        return self.add(m, bm_cyl(r, L, seg, r2, caps), M)

    def tube(self, m, pts, r, seg=8, caps=True, prof=None, closed=True, path_closed=False):
        return self.add(m, bm_sweep(pts, r, seg, caps, prof, closed, path_closed))

    def lathe(self, m, prof, seg=24, M=None, cap0=True, cap1=True):
        return self.add(m, bm_lathe(prof, seg, cap0, cap1), M)

    def finish(self):
        obs = []
        for key, (m, b) in self.parts.items():
            if not b.faces:
                b.free()
                continue
            bmesh.ops.dissolve_degenerate(b, dist=1e-6, edges=b.edges)
            ob = common.obj_from_bmesh('teil', b, m)
            for p in ob.data.polygons:
                p.use_smooth = True
            try:
                ob.data.set_sharp_from_angle(angle=math.radians(self.degs.get(key, self.deg)))
            except AttributeError:
                pass
            wn = ob.modifiers.new('wn', 'WEIGHTED_NORMAL')
            wn.keep_sharp = True
            obs.append(ob)
        self.parts = {}
        return obs


# =====================================================================================================================
# Bausteine (bmesh, lokale Koordinaten)
# =====================================================================================================================
def V(*a):
    return Vector(a[0]) if len(a) == 1 else Vector(a)


def along(a, b):
    """Matrix, deren z-Achse von a nach b zeigt (Ursprung a), und die Laenge."""
    a, b = Vector(a), Vector(b)
    d = b - a
    q = d.to_track_quat('Z', 'Y') if abs(d.normalized().y) < 0.99 else d.to_track_quat('Z', 'X')
    return Matrix.Translation(a) @ q.to_matrix().to_4x4(), d.length


def frame(origin, z, xhint=(1, 0, 0)):
    """Matrix mit z-Achse z und x-Achse moeglichst xhint."""
    z = Vector(z).normalized()
    x = Vector(xhint)
    x = (x - z * x.dot(z))
    if x.length < 1e-6:
        x = Vector((0, 1, 0)) - z * z.y
    x.normalize()
    y = z.cross(x)
    M = Matrix((x, y, z)).transposed().to_4x4()
    M.translation = Vector(origin)
    return M


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(angle, axis):
    return Matrix.Rotation(angle, 4, axis)


def bm_box(lo, hi, bev=0.0, seg=1):
    b = bmesh.new()
    bmesh.ops.create_cube(b, size=1.0)
    for v in b.verts:
        v.co = Vector([lo[i] + (v.co[i] + 0.5) * (hi[i] - lo[i]) for i in range(3)])
    if bev > 0:
        bmesh.ops.bevel(b, geom=list(b.edges), offset=bev, offset_type='OFFSET', segments=seg, profile=0.5,
                        affect='EDGES', clamp_overlap=True)
    return b


def bm_cyl(r, h, seg=12, r2=None, caps=True, z0=0.0):
    b = bmesh.new()
    bmesh.ops.create_cone(b, cap_ends=caps, cap_tris=False, segments=seg, radius1=r, radius2=r if r2 is None else r2, depth=h)
    bmesh.ops.translate(b, vec=(0, 0, z0 + h / 2), verts=b.verts)
    return b


def bm_sphere(r, seg=12, rings=8):
    b = bmesh.new()
    bmesh.ops.create_uvsphere(b, u_segments=seg, v_segments=rings, radius=r)
    return b


def bm_lathe(prof, seg=24, cap0=True, cap1=True, a0=0.0, arc=TAU):
    """Drehkoerper um z: prof = [(r, z), ...] von unten nach oben; r = 0 an den Enden schliesst spitz."""
    b = bmesh.new()
    full = arc >= TAU - 1e-6
    ns = seg if full else seg + 1
    rings = []
    for (r, z) in prof:
        if r <= 1e-7:
            rings.append([b.verts.new((0, 0, z))])
        else:
            rings.append([b.verts.new((r * math.cos(a0 + arc * k / seg), r * math.sin(a0 + arc * k / seg), z)) for k in range(ns)])
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        for k in range(seg):
            j = (k + 1) % ns if full else k + 1
            if len(A) == 1 and len(B) == 1:
                continue
            if len(A) == 1:
                b.faces.new((A[0], B[j], B[k]))
            elif len(B) == 1:
                b.faces.new((A[k], A[j], B[0]))
            else:
                b.faces.new((A[k], A[j], B[j], B[k]))
    if full:
        if cap0 and len(rings[0]) > 2:
            b.faces.new(list(reversed(rings[0])))
        if cap1 and len(rings[-1]) > 2:
            b.faces.new(rings[-1])
    return b


def _frames(pts):
    """Rotationsminimierende Rahmen (Doppelspiegelung) entlang eines Polygonzugs."""
    n = len(pts)
    Tn = []
    for i in range(n):
        if i == 0:
            t = pts[1] - pts[0]
        elif i == n - 1:
            t = pts[-1] - pts[-2]
        else:
            t = (pts[i + 1] - pts[i]).normalized() + (pts[i] - pts[i - 1]).normalized()
            if t.length < 1e-9:
                t = pts[i + 1] - pts[i]
        Tn.append(t.normalized())
    up = Vector((0, 0, 1)) if abs(Tn[0].z) < 0.9 else Vector((1, 0, 0))
    N = (up - Tn[0] * up.dot(Tn[0])).normalized()
    out = []
    for i in range(n):
        if i > 0:
            v1 = pts[i] - pts[i - 1]
            c1 = v1.dot(v1)
            if c1 > 1e-14:
                rL = N - (2.0 / c1) * v1.dot(N) * v1
                tL = Tn[i - 1] - (2.0 / c1) * v1.dot(Tn[i - 1]) * v1
                v2 = Tn[i] - tL
                c2 = v2.dot(v2)
                N = rL - (2.0 / c2) * v2.dot(rL) * v2 if c2 > 1e-14 else rL
            N = (N - Tn[i] * N.dot(Tn[i])).normalized()
        out.append((Tn[i], N, Tn[i].cross(N)))
    return out


def bm_sweep(pts, r, seg=8, caps=True, prof=None, closed=True, path_closed=False):
    """Querschnitt entlang eines Pfads. r: Zahl, Liste je Punkt oder (rx, ry); prof: Liste (u, v) (Standard Kreis)."""
    pts = [Vector(p) for p in pts]
    n = len(pts)
    rr = r if isinstance(r, (list, tuple)) and len(r) == n and n != 2 else [r] * n
    if prof is None:
        prof = [(math.cos(TAU * k / seg), math.sin(TAU * k / seg)) for k in range(seg)]
    fr = _frames(pts)
    b = bmesh.new()
    rings = []
    for p, (t, N, B), ri in zip(pts, fr, rr):
        rx, ry = (ri, ri) if not isinstance(ri, (list, tuple)) else ri
        rings.append([b.verts.new(p + N * (u * rx) + B * (v * ry)) for (u, v) in prof])
    m = len(prof)
    last = n if path_closed else n - 1
    for i in range(last):
        A, Bn = rings[i], rings[(i + 1) % n]
        for k in range(m if closed else m - 1):
            j = (k + 1) % m
            b.faces.new((A[k], A[j], Bn[j], Bn[k]))
    if caps and closed and not path_closed:
        b.faces.new(list(reversed(rings[0])))
        b.faces.new(rings[-1])
    return b


def bm_prism(poly, z0, z1):
    """2D-Umriss (x, y) zwischen z0 und z1."""
    area = sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]))
    if area < 0:
        poly = list(reversed(poly))
    b = bmesh.new()
    lo = [b.verts.new((x, y, z0)) for x, y in poly]
    hi = [b.verts.new((x, y, z1)) for x, y in poly]
    b.faces.new(list(reversed(lo)))
    b.faces.new(hi)
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        b.faces.new((lo[i], lo[j], hi[j], hi[i]))
    return b


def bm_poly(pts):
    b = bmesh.new()
    b.faces.new([b.verts.new(p) for p in pts])
    return b


def bm_grid(w, h, nx, ny, fz=None):
    """Flaeche in xy (-w/2..w/2, -h/2..h/2), Normale +z; fz(x, y) -> z."""
    b = bmesh.new()
    vs = [[b.verts.new((-w / 2 + w * i / nx, -h / 2 + h * j / ny, 0.0)) for j in range(ny + 1)] for i in range(nx + 1)]
    if fz:
        for row in vs:
            for v in row:
                v.co.z = fz(v.co.x, v.co.y)
    for i in range(nx):
        for j in range(ny):
            b.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1]))
    return b


def rounded_rect(w, h, r, n=4):
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0.0), (-w / 2 + r, h / 2 - r, 0.5 * math.pi), (-w / 2 + r, -h / 2 + r, math.pi),
                       (w / 2 - r, -h / 2 + r, 1.5 * math.pi)):
        for k in range(n + 1):
            a = a0 + 0.5 * math.pi * k / n
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def blob2d(n, seed, irr=0.35):
    """Unregelmaessiger Fleck (Einheitsradius) als Punktliste."""
    rng = random.Random(seed)
    ph = [rng.uniform(0, TAU) for _ in range(4)]
    out = []
    for k in range(n):
        a = TAU * k / n
        rr = 1.0 + irr * (0.55 * math.sin(2 * a + ph[0]) + 0.35 * math.sin(3 * a + ph[1]) + 0.2 * math.sin(5 * a + ph[2])) \
            + irr * 0.35 * rng.uniform(-1, 1)
        out.append((math.cos(a) * max(0.25, rr), math.sin(a) * max(0.25, rr)))
    return out


def catmull(ctrl, per=4):
    """Glatter Pfad durch Kontrollpunkte (Catmull-Rom, per Punkte je Abschnitt)."""
    P = [Vector(p) for p in ctrl]
    out = []
    for i in range(len(P) - 1):
        p0 = P[i - 1] if i > 0 else P[i] * 2 - P[i + 1]
        p1, p2 = P[i], P[i + 1]
        p3 = P[i + 2] if i + 2 < len(P) else P[i + 1] * 2 - P[i]
        for k in range(per):
            t = k / per
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(P[-1].copy())
    return out


def arc_pts(c, r, a0, a1, n, u=(1, 0, 0), v=(0, 1, 0)):
    c, u, v = Vector(c), Vector(u), Vector(v)
    return [c + (u * math.cos(a0 + (a1 - a0) * k / n) + v * math.sin(a0 + (a1 - a0) * k / n)) * r for k in range(n + 1)]


def bent_path(pts, rad, n=4):
    """Polygonzug mit gerundeten Ecken (Rohrbogen mit Radius rad)."""
    P = [Vector(p) for p in pts]
    out = [P[0]]
    for i in range(1, len(P) - 1):
        a, b, c = P[i - 1], P[i], P[i + 1]
        d1, d2 = (b - a).normalized(), (c - b).normalized()
        ang = d1.angle(d2) if (d1 - d2).length > 1e-6 else 0.0
        if ang < 1e-3:
            out.append(b)
            continue
        t = rad * math.tan(ang / 2)
        t = min(t, (b - a).length * 0.49, (c - b).length * 0.49)
        p0, p1 = b - d1 * t, b + d2 * t
        for k in range(n + 1):
            s = k / n
            q = (1 - s) ** 2 * p0 + 2 * (1 - s) * s * b + s * s * p1  # quadratisch, nah am Kreisbogen
            out.append(q)
    out.append(P[-1])
    return out


# =====================================================================================================================
# Kleinteile: Schrauben, Muttern, Nieten, Schweissnaht, Flecken, Schrift
# =====================================================================================================================
def bolt(K, m, p, n, d=0.012, washer=True, stub=0.0, rot=None, mw=None):
    """Sechskantschraube/Mutter auf einer Flaeche (Punkt p, Normale n), Nenndurchmesser d."""
    M = frame(p, n)
    rot = random.uniform(0, 1) if rot is None else rot
    z = 0.0
    if washer:
        K.add(mw or m, bm_cyl(d * 1.05, d * 0.14, 10), M)
        z = d * 0.14
    K.add(m, bm_cyl(d * 0.88, d * 0.62, 6, z0=z), M @ R(rot, 'Z'))
    if stub > 0:
        K.add(m, bm_cyl(d * 0.42, stub, 6, z0=z + d * 0.62), M)


def rivet(K, m, p, n, d=0.018, seg=6):
    M = frame(p, n)
    K.add(m, bm_lathe([(d * 0.55, 0.0), (d * 0.48, d * 0.22), (d * 0.28, d * 0.36), (0.0, d * 0.40)], seg, cap0=False), M)


def slot_screw(K, m, mslot, p, n, d=0.008, rot=0.0):
    M = frame(p, n)
    K.add(m, bm_lathe([(d * 0.5, 0.0), (d * 0.46, d * 0.12), (d * 0.3, d * 0.24), (0.0, d * 0.28)], 8, cap0=False), M)
    K.add(mslot, bm_box((-d * 0.42, -d * 0.06, d * 0.2), (d * 0.42, d * 0.06, d * 0.3)), M @ R(rot, 'Z'))


def weld(K, m, pts, r=0.004, seed=0, seg=5):
    """Schweissraupe entlang pts (leicht unregelmaessig)."""
    rng = random.Random(seed)
    P = [Vector(p) for p in pts]
    rr = [r * (0.8 + 0.4 * rng.random()) for _ in P]
    K.tube(m, P, rr, seg)


def decal(K, m, c, u, v, su, sv, seed, n=9, lift=0.0012, irr=0.35, deform=None):
    """Fleck auf einer ebenen Flaeche: Mitte c, Richtungen u, v (Normale u x v), Halbachsen su, sv."""
    c, u, v = Vector(c), Vector(u).normalized(), Vector(v).normalized()
    nn = u.cross(v)
    pts = [c + u * (x * su) + v * (y * sv) + nn * lift for (x, y) in blob2d(n, seed, irr)]
    if deform:
        pts = [deform(p) for p in pts]
    K.add(m, bm_poly(pts))


def streak(K, m, top, u, down, w, length, seed, n=6, lift=0.0012):
    """Laufspur (Rost, Fluessigkeit) von top in Richtung down, Breite w, verjuengt."""
    rng = random.Random(seed)
    top, u, down = Vector(top), Vector(u).normalized(), Vector(down).normalized()
    nn = u.cross(down) * -1.0
    L, Rr = [], []
    for k in range(n + 1):
        t = k / n
        ww = w * (1.0 - 0.85 * t) * (0.8 + 0.4 * rng.random())
        cx = (rng.random() - 0.5) * w * 0.4
        p = top + down * (length * t) + u * cx + nn * lift
        L.append(p - u * ww * 0.5)
        Rr.append(p + u * ww * 0.5)
    b = bmesh.new()
    vl = [b.verts.new(p) for p in L]
    vr = [b.verts.new(p) for p in Rr]
    for k in range(n):
        f = b.faces.new((vl[k], vr[k], vr[k + 1], vl[k + 1]))
        f.normal_update()
        if f.normal.dot(nn) < 0:
            f.normal_flip()
    K.add(m, b)


def cdecal(K, m, a, axis, ref, R0, ang, s, wa, ws, seed, n=10, lift=0.0012):
    """Fleck auf einem Zylindermantel (Achse durch a, Richtung axis, Winkel 0 bei ref, Radius R0):
    Mitte bei Winkel ang und Achsposition s, halbe Breite wa (rad) und halbe Laenge ws."""
    a, axis, ref = Vector(a), Vector(axis).normalized(), Vector(ref).normalized()
    side = axis.cross(ref)
    rr = R0 + lift

    def P(th, t):
        return a + axis * t + (ref * math.cos(th) + side * math.sin(th)) * rr
    b = bmesh.new()
    pts = blob2d(n, seed, 0.3)
    ctr = b.verts.new(P(ang, s))
    mid = [b.verts.new(P(ang + x * wa * 0.5, s + y * ws * 0.5)) for (x, y) in pts]
    out = [b.verts.new(P(ang + x * wa, s + y * ws)) for (x, y) in pts]
    for k in range(n):
        j = (k + 1) % n
        b.faces.new((ctr, mid[k], mid[j]))
        b.faces.new((mid[k], out[k], out[j], mid[j]))
    b.normal_update()
    b.faces.ensure_lookup_table()
    radial = (P(ang, s) - (a + axis * s)).normalized()
    if b.faces[0].normal.dot(radial) < 0:
        bmesh.ops.reverse_faces(b, faces=b.faces)
    K.add(m, b)


def glyph_cell(ch):
    if 'A' <= ch <= 'Z':
        return ord(ch) - 65
    if '0' <= ch <= '9':
        return 26 + ord(ch) - 48
    if ch == '-':
        return 36
    return -1


def text(K, m, s, origin, right, up, h, w=None, gap=0.12, lift=0.0):
    """Schrift aus dem Zeichenatlas des Spiels (look=sign: Strich in Materialfarbe auf Schwarz,
    look=stencil: nur der Strich). origin = linke untere Ecke; Blick auf die Flaeche entlang -(right x up)."""
    o, r, u = Vector(origin), Vector(right).normalized(), Vector(up).normalized()
    nn = r.cross(u)
    w = w or h * 0.72
    for i, ch in enumerate(s):
        c = glyph_cell(ch)
        if c < 0:
            continue
        u0, v0 = (c % 8) / 8.0, (c // 8) / 8.0
        u1, v1 = u0 + 0.125, v0 + 0.125
        p = o + r * (i * w * (1 + gap)) + nn * lift
        corners = [p, p + r * w, p + r * w + u * h, p + u * h]
        uvs = [(u0, 1 - v1), (u1, 1 - v1), (u1, 1 - v0), (u0, 1 - v0)]
        b = bmesh.new()
        uvl = b.loops.layers.uv.new('UVMap')
        f = b.faces.new([b.verts.new(q) for q in corners])
        for lp, uv in zip(f.loops, uvs):
            lp[uvl].uv = uv
        K.add(m, b)


def sign_quad(K, m, cell, center, right, up, w, h):
    """Bildzeichen aus dem Atlas (z. B. 41 Warndreieck)."""
    c, r, u = Vector(center), Vector(right).normalized(), Vector(up).normalized()
    u0, v0 = (cell % 8) / 8.0, (cell // 8) / 8.0
    u1, v1 = u0 + 0.125, v0 + 0.125
    corners = [c - r * w / 2 - u * h / 2, c + r * w / 2 - u * h / 2, c + r * w / 2 + u * h / 2, c - r * w / 2 + u * h / 2]
    uvs = [(u0, 1 - v1), (u1, 1 - v1), (u1, 1 - v0), (u0, 1 - v0)]
    b = bmesh.new()
    uvl = b.loops.layers.uv.new('UVMap')
    f = b.faces.new([b.verts.new(q) for q in corners])
    for lp, uv in zip(f.loops, uvs):
        lp[uvl].uv = uv
    K.add(m, b)


def jitter(b, amp, scale, seed=0):
    off = Vector((seed * 3.17, seed * 7.31, seed * 1.93))
    for v in b.verts:
        v.co += Vector(noise.noise_vector(v.co * scale + off)) * amp
    return b


# =====================================================================================================================
# Skulptur (Distanzfeld) und Reduktion
# =====================================================================================================================
def decimate(ob, target):
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if tris <= target:
        return tris
    mod = ob.modifiers.new('red', 'DECIMATE')
    mod.ratio = target / tris
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = ob
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob]):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def sculpt(name, parts, h, disp=None, target=None, adapt=0.0):
    """Netz der Nullflaeche eines Distanzfelds (Teile aus creatures/sdf.py), reduziert auf target Dreiecke."""
    F, lo = sdf.evaluate(parts, h, disp)
    Vv, Tt = sdf.polygonize(F, lo, h, adapt)
    del F
    me = bpy.data.meshes.new(name)
    me.from_pydata(Vv.tolist(), [], Tt.tolist())
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if target:
        decimate(ob, target)
    return ob


def paint_by(ob, parts, disp, mats, pick, cav_scale=0.012):
    """Materialien je Flaeche nach Hoehlung, Kante, Lage: pick(center, normal, cav, edge) -> Index in mats."""
    me = ob.data
    nv = len(me.vertices)
    Vv = np.zeros(nv * 3)
    me.vertices.foreach_get('co', Vv)
    Vv = Vv.reshape(-1, 3)
    tri = np.array([p.vertices[:] for p in me.polygons if len(p.vertices) == 3], dtype=np.int64)
    Nn = sdf.vertex_normals(Vv, tri) if len(tri) else np.zeros_like(Vv)
    cav, edge = sdf.cavity_edge(parts, Vv, Nn, disp, cav_scale)
    for m in mats:
        me.materials.append(m)
    for p in me.polygons:
        idx = list(p.vertices)
        c = Vector(p.center)
        p.material_index = pick(c, Vector(p.normal), float(cav[idx].mean()), float(edge[idx].mean()))
    for p in me.polygons:
        p.use_smooth = True


def Pcone(a, b, ra, rb, k=0.0, op='add'):
    return sdf.Part('cone', -1, k=k, op=op, a=a, b=b, ra=ra, rb=rb)


def Pell(c, r, k=0.0, op='add', rot=None):
    Rm = np.eye(3) if rot is None else np.asarray(rot, dtype=np.float64)
    return sdf.Part('ell', -1, k=k, op=op, c=c, R=Rm, r=r)


def Pbox(c, half, rnd=0.0, k=0.0, op='add', rot=None):
    Rm = np.eye(3) if rot is None else np.asarray(rot, dtype=np.float64)
    return sdf.Part('box', -1, k=k, op=op, c=c, R=Rm, half=half, round=rnd)


def Ptube(a, b, r0, r1, up=(0, 0, 1), k=0.0, op='add'):
    return sdf.Part('tube', -1, k=k, op=op, a=a, b=b, up=up, r0=r0, r1=r1)


def rotm(ax, ang):
    """3x3 Drehmatrix (Spalten = lokale Achsen) um Achse ax."""
    return np.array(Matrix.Rotation(ang, 3, ax))


class FieldPart:
    """Eigenes Distanzfeld (Funktion P -> Abstand) mit Grenzen, passend zu sdf.evaluate."""

    def __init__(self, fn, lo, hi, k=0.0, op='add'):
        self.fn, self.k, self.op, self.bone, self.kind = fn, k, op, -1, 'fn'
        self.lo, self.hi = np.asarray(lo, dtype=np.float64) - k - 0.004, np.asarray(hi, dtype=np.float64) + k + 0.004

    def dist(self, P):
        return self.fn(P)


# =====================================================================================================================
# Gitter (bars_w1/2/3): Rundstaebe 30 mm alle 12 cm durch gelochte Flachstahl-Riegel, verschweisst, weiss lackiert,
# Rost an Naehten, Fuessen und Laufspuren; ein Stab ist ausgebeult. Ursprung in der Mitte der Oeffnung am Boden,
# Gitterebene x = 0; Staebe reichen in Boden und Decke, Riegel 15 cm in die Laibungen.
# =====================================================================================================================
def _bars(width, seed):
    P = _pal()
    K = Kit(deg=50.0)
    rng = random.Random(seed)
    n = max(2, int(round(width / 0.12)))
    sp = width / n
    straps = [0.14, 1.02, 1.96, 2.86]
    ztop = 4.05
    bentk = rng.randrange(2, n - 2) if n > 5 else -1
    for s in straps:  # Flachstahl 60 x 12, liegend (Loecher fuer die Staebe)
        K.box(P['bar'], (-0.03, -width / 2 - 0.15, s), (0.03, width / 2 + 0.15, s + 0.012), bev=0.002)
        # Staub und Rost auf der Oberseite
        for j in range(int(width * 3)):
            y = rng.uniform(-width / 2, width / 2)
            m = P['dust'] if rng.random() < 0.55 else P['rust']
            decal(K, m, (rng.uniform(-0.015, 0.015), y, s + 0.012), (1, 0, 0), (0, 1, 0), 0.012, rng.uniform(0.03, 0.12),
                  seed * 100 + j, n=8, lift=0.0006)
        for j in range(int(width * 2)):  # Abplatzer an der Vorderkante
            y = rng.uniform(-width / 2, width / 2)
            sx = rng.choice((-1, 1))
            decal(K, P['rust_dark'], (sx * 0.03, y, s + 0.006), (0, sx, 0), (0, 0, 1), rng.uniform(0.01, 0.04), 0.005,
                  seed * 200 + j, n=7, lift=0.0006)
    for k in range(n):
        y0 = -width / 2 + sp * (k + 0.5)
        dy = rng.uniform(-0.002, 0.002)
        zs = [-0.05, 0.14, 0.6, 1.02, 1.5, 1.96, 2.4, 2.86, 3.45, ztop]
        pts = []
        for z in zs:
            x, y = rng.uniform(-0.0015, 0.0015), y0 + dy
            pts.append(Vector((x, y, z)))
        if k == bentk:  # ausgebeult zwischen den mittleren Riegeln
            pts = [p for p in pts if not (1.02 < p.z < 1.96)]
            ins = [Vector((0.045 * math.sin(math.pi * t), y0 + 0.012 * math.sin(math.pi * t), 1.02 + 0.94 * t)) for t in (0.2, 0.4, 0.6, 0.8)]
            pts = pts[:4] + ins + pts[4:]
        K.tube(P['bar'], pts, 0.015, 8)
        # Rost am Fuss: Manschette mit zackigem Rand
        hgt = rng.uniform(0.06, 0.32)
        b = bmesh.new()
        lo_r = [b.verts.new((y0 * 0 + 0.0159 * math.cos(TAU * i / 8), y0 + dy + 0.0159 * math.sin(TAU * i / 8), -0.05)) for i in range(8)]
        hi_r = [b.verts.new((0.0159 * math.cos(TAU * i / 8), y0 + dy + 0.0159 * math.sin(TAU * i / 8), hgt * rng.uniform(0.55, 1.0)))
                for i in range(8)]
        for i in range(8):
            j = (i + 1) % 8
            b.faces.new((lo_r[i], lo_r[j], hi_r[j], hi_r[i]))
        K.add(P['rust'], b)
        # Schweissraupen oben und unten an jedem Riegel, oft rostig
        for s in straps:
            for zz in (s + 0.012, s):
                mm = P['rust'] if rng.random() < 0.35 else P['bar_dirty']
                ring = arc_pts((0, y0 + dy, zz), 0.0185, 0, TAU, 6)[:-1]
                K.tube(mm, ring, 0.004, 3, caps=False, path_closed=True)
            # Laufspur unter der Naht
            if rng.random() < 0.8:
                side = rng.choice((0.0, math.pi))
                ln = rng.uniform(0.05, 0.3)
                cdecal(K, P['rust'], (0, y0 + dy, 0), (0, 0, 1), (1, 0, 0), 0.015, side + rng.uniform(-0.4, 0.4),
                       s - ln * 0.5, 0.45, ln * 0.5, seed * 1000 + k * 7 + int(s * 10), n=8, lift=0.0007)
        # Rostflecken und Abplatzer am Stab
        for j in range(rng.randrange(2, 5)):
            cdecal(K, P['rust'] if j % 2 else P['rust_dark'], (0, y0 + dy, 0), (0, 0, 1), (1, 0, 0), 0.015, rng.uniform(0, TAU),
                   rng.uniform(0.3, 2.8), rng.uniform(0.4, 0.9), rng.uniform(0.01, 0.06), seed * 3000 + k * 11 + j, n=7, lift=0.0007)
    return K.finish()


def build_bars_w1():
    _bars(1.0, 31)


def build_bars_w2():
    _bars(2.0, 32)


def build_bars_w3():
    _bars(3.0, 33)


# =====================================================================================================================
# Kabeltrasse (cable_tray, 4 Varianten): 1 m Kabelleiter (verzinkt: zwei C-Holme mit Langloechern, Sprossen alle 25 cm)
# mit Kabeln in festen Lagen - an beiden Enden in allen Varianten gleich, damit Abschnitte beliebig aneinanderpassen.
# Die Kabel liegen auf den Sprossen und haengen dazwischen leicht durch (von unten sichtbar), oben quillt die dritte
# Lage ueber die Holme. Haenger als Traverse (Gewindestangen, Profilschiene, Muttern), Stossverbinder bei x = +0,5.
# Decke z = 0.
# =====================================================================================================================
_LANES = [(-0.150, 0.009, 'k_red', 1), (-0.120, 0.016, 'k_black', 1), (-0.080, 0.021, 'k_black', 1),
          (-0.036, 0.013, 'k_grey', 1), (0.002, 0.019, 'k_black', 1), (0.044, 0.015, 'k_cream', 1),
          (0.086, 0.022, 'k_black', 1), (0.132, 0.012, 'k_grey', 1),
          (-0.058, 0.013, 'k_black', 2), (0.022, 0.012, 'k_grey', 2), (0.066, 0.011, 'k_cream', 2), (0.108, 0.014, 'k_black', 2),
          (-0.020, 0.024, 'k_black', 3), (0.074, 0.017, 'k_black', 3)]
_RUNG_TOP = -0.280
_RUNGS = (-0.375, -0.125, 0.125, 0.375)


def _sag(x, r):
    """Durchhang zwischen den Sprossen (an den Enden x = +-0,5 in jeder Variante gleich)."""
    f = ((x - 0.125) / 0.25) % 1.0
    return (0.002 + 0.10 * r) * math.sin(math.pi * f)


def _lane_z(r, layer):
    return _RUNG_TOP + r + {1: 0.0, 2: 0.028, 3: 0.050}[layer]


def _cable_mats():
    return {
        'k_black': mat('kabel_schwarz', (0.045, 0.045, 0.048), 'rubber', r=0.9),
        'k_grey': mat('kabel_grau', (0.36, 0.37, 0.38), 'plastic', r=1.35),
        'k_cream': mat('kabel_creme', (0.66, 0.62, 0.52), 'plastic', r=1.35),
        'k_red': mat('kabel_rot', (0.40, 0.11, 0.07), 'rubber', r=1.0),
        'k_dust': mat('kabel_staub', (0.40, 0.38, 0.35), 'concrete', r=1.5),
    }


def _cable_tray(v):
    P = _pal()
    C = _cable_mats()
    K = Kit(deg=42.0)
    rng = random.Random(500 + v)
    galv = P['galv']
    # Holme: C-Profil 60 x 15 x 2 mm, offen nach innen (Querschnitt in y, z), entlang x
    for s in (-1, 1):
        prof = [(0.180, -0.240), (0.163, -0.240), (0.163, -0.242), (0.178, -0.242), (0.178, -0.298), (0.163, -0.298),
                (0.163, -0.300), (0.180, -0.300)]
        prof = [(s * y, z) for (y, z) in prof]
        K.add(galv, bm_prism(prof, -0.5, 0.5), Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
        # Langloecher im Steg (aussen dunkel)
        for i in range(8):
            x = -0.4375 + i * 0.125
            yy = s * 0.1803
            pts = [(x - 0.016, yy, -0.274), (x + 0.016, yy, -0.274), (x + 0.016, yy, -0.266), (x - 0.016, yy, -0.266)]
            K.add(P['hole'], bm_poly(pts if s < 0 else list(reversed(pts))))
    # Sprossen: Hutprofil 40 x 20, unten offen
    for x in _RUNGS:
        K.box(galv, (x - 0.020, -0.178, -0.300), (x + 0.020, 0.178, _RUNG_TOP), bev=0.0015)
        K.add(P['hole'], bm_poly([(x - 0.012, -0.176, -0.3003), (x - 0.012, 0.176, -0.3003), (x + 0.012, 0.176, -0.3003),
                                  (x + 0.012, -0.176, -0.3003)]))
        for s in (-1, 1):
            bolt(K, P['steel'], (x, s * 0.1802, -0.290), (0, s, 0), d=0.007, washer=False, rot=rng.random())
    # Stossverbinder bei x = 0,5 (Lasche aussen an beiden Holmen, 4 Schrauben)
    for s in (-1, 1):
        y0, y1 = (0.1802, 0.1824) if s > 0 else (-0.1824, -0.1802)
        K.box(P['galv_dark'], (0.44, y0, -0.296), (0.56, y1, -0.246), bev=0.0008)
        for xx in (0.462, 0.538):
            for zz in (-0.285, -0.258):
                bolt(K, P['steel'], (xx, s * 0.1824, zz), (0, s, 0), d=0.006, washer=False, rot=rng.random())
    # Rost und Zinkbluete an Holmen und Sprossen
    for j in range(rng.randrange(3, 6)):
        s = rng.choice((-1, 1))
        decal(K, P['rust'] if rng.random() < 0.6 else P['zinc_white'], (rng.uniform(-0.45, 0.45), s * 0.1806, rng.uniform(-0.296, -0.246)),
              (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), rng.uniform(0.004, 0.012), rng.uniform(0.01, 0.05), 700 + v * 31 + j,
              n=8, lift=0.0004)
    for j in range(rng.randrange(1, 3)):
        x = rng.choice(_RUNGS)
        decal(K, P['rust'], (x + rng.uniform(-0.01, 0.01), rng.uniform(-0.15, 0.15), -0.3006), (0, 1, 0), (1, 0, 0),
              rng.uniform(0.02, 0.06), 0.012, 800 + v * 17 + j, n=8, lift=0.0002)

    # Kabel (Oberseiten mit Staub)
    def dusty(cm):
        def pick(f):
            c = f.calc_center_median()
            if f.normal.z > 0.5 and noise.noise(c * 9.0 + Vector((v, 0, 0))) > -0.3:
                return C['k_dust']
            return cm
        return pick

    def cable(pts, r, cm, seg=None):
        seg = seg or (8 if r >= 0.018 else 6)
        K.add_split(bm_sweep(pts, r, seg, caps=False), dusty(cm))

    xs = [-0.5 + 0.125 * i for i in range(9)]
    for li, (y, r, ck, layer) in enumerate(_LANES):
        if v == 2 and li == 3:  # duennes Kabel ist zwischen den Sprossen durchgerutscht und haengt unten in einer Schlaufe
            ctrl = [(x, y, _lane_z(r, 1) - _sag(x, r)) for x in (-0.5, -0.4375, -0.375, -0.3125)]
            ctrl += [(-0.24, y + 0.004, -0.284), (-0.19, y + 0.01, -0.316), (-0.10, y + 0.016, -0.352), (0.0, y + 0.02, -0.37),
                     (0.10, y + 0.016, -0.36), (0.19, y + 0.01, -0.33), (0.25, y + 0.005, -0.287)]
            ctrl += [(x, y, _lane_z(r, 1) - _sag(x, r)) for x in (0.3125, 0.375, 0.4375, 0.5)]
            cable(catmull(ctrl, 2), r, C[ck])
            continue
        if v == 3 and li == 7:  # durch die Abzweigdose an der Decke geschleift
            z0 = _lane_z(r, 1)
            cable(catmull([(-0.5, y, z0 - _sag(-0.5, r)), (-0.4375, y, z0 - _sag(-0.4375, r)), (-0.375, y, z0), (-0.33, y - 0.004, z0 + 0.03),
                           (-0.28, y - 0.01, -0.15), (-0.235, y - 0.012, -0.075)], 3), r, C[ck])
            cable(catmull([(-0.115, y - 0.012, -0.075), (-0.07, y - 0.01, -0.15), (-0.01, y - 0.004, z0 + 0.03), (0.125, y, z0),
                           (0.25, y, z0 - _sag(0.25, r)), (0.375, y, z0), (0.4375, y, z0 - _sag(0.4375, r)), (0.5, y, z0 - _sag(0.5, r))], 3),
                  r, C[ck])
            continue
        z0 = _lane_z(r, layer)
        amp = 0.005 if layer == 1 else 0.012
        ph = rng.uniform(0, TAU)
        pts = []
        for x in xs:
            inner = math.sin(math.pi * (x + 0.5)) ** 2 if abs(x) < 0.42 else 0.0
            dy = inner * amp * math.sin(ph + x * 7.0)
            pts.append((x, y + dy, z0 - _sag(x, r) * (1.0 if layer == 1 else 0.6) + inner * (0.002 * math.sin(ph * 2 + x * 9) if layer > 1 else 0.0)))
        cable(pts, r, C[ck], 7 if r >= 0.02 else 5)

    # Kabelbinder an Sprossen (um ein dickes Kabel und die Sprosse)
    def strap(x, y, r, m):
        zc = _RUNG_TOP + r
        ring = [(x, y + (r + 0.0015) * math.cos(a), zc + (r + 0.0015) * math.sin(a)) for a in [math.pi * k / 6 for k in range(7)]]
        ring += [(x, y - r - 0.0015, -0.301), (x, y + r + 0.0015, -0.301)]
        K.tube(m, ring, (0.0012, 0.0035), 4, caps=False, path_closed=True)
    if v == 0:
        strap(-0.125, -0.080, 0.021, P['black_plastic'])
        strap(0.375, 0.086, 0.022, P['black_plastic'])
        zt = _lane_z(0.024, 3) + 0.024
        K.box(P['white_plastic'], (0.20, -0.04, zt - 0.001), (0.25, -0.004, zt + 0.002), bev=0.0008)
        K.box(P['black_plastic'], (0.207, -0.032, zt + 0.002), (0.243, -0.014, zt + 0.0024))
    if v == 1:
        strap(-0.375, 0.002, 0.019, P['white_plastic'])
        strap(0.125, -0.080, 0.021, P['white_plastic'])
        strap(0.125, 0.086, 0.022, P['white_plastic'])
        y, r = 0.044, 0.015  # Isolierung aufgeschnitten: Kupfer sichtbar
        K.cyl(mat('kupfer', (0.62, 0.36, 0.20), 'metal', r=0.7), (-0.03, y, _lane_z(r, 1) - _sag(-0.03, r)),
              (0.01, y, _lane_z(r, 1) - _sag(0.01, r)), r * 1.05, 8)
    if v == 3:  # Abzweigdose an der Decke
        K.box(P['grey_plastic'], (-0.27, 0.055, -0.068), (-0.08, 0.175, -0.002), bev=0.004, seg=2)
        K.box(P['grey_plastic'], (-0.265, 0.06, -0.074), (-0.085, 0.17, -0.068), bev=0.002)
        for xx in (-0.255, -0.095):
            for yy in (0.068, 0.162):
                slot_screw(K, P['steel'], P['hole'], (xx, yy, -0.074), (0, 0, -1), d=0.006, rot=rng.random())
        for xx in (-0.235, -0.115):
            K.cyl(P['black_plastic'], (xx, 0.12, -0.068), (xx, 0.12, -0.082), 0.011, 8)
        decal(K, P['dust'], (-0.175, 0.115, -0.0745), (1, 0, 0), (0, -1, 0), 0.07, 0.04, 77, lift=0.0004)
    # Traverse: Gewindestangen, Deckenanker, Profilschiene, Muttern, Niederhalter
    hx = {0: 0.0, 1: -0.25, 3: 0.25}.get(v)
    if hx is not None:
        for s in (-1, 1):
            y = s * 0.2
            K.cyl(galv, (hx, y, -0.352), (hx, y, -0.02), 0.005, 6)
            K.add(P['steel'], bm_cyl(0.0085, 0.028, 6, z0=-0.03), T(hx, y, 0) @ R(rng.random(), 'Z'))
            K.add(P['steel'], bm_cyl(0.016, 0.003, 10, z0=-0.003), T(hx, y, 0))
            bolt(K, P['steel'], (hx, y, -0.3405), (0, 0, -1), d=0.010)
            K.add(P['steel'], bm_cyl(0.0088, 0.008, 6, z0=0.0), T(hx, y, -0.2995) @ R(rng.random(), 'Z'))
            ya, yb = (0.160, 0.186) if s > 0 else (-0.186, -0.160)
            K.box(P['galv_dark'], (hx - 0.015, ya, -0.2425), (hx + 0.015, yb, -0.2395))
        K.box(P['galv_dark'], (hx - 0.0205, -0.215, -0.3405), (hx + 0.0205, 0.215, -0.3005), bev=0.002)
        K.add(P['hole'], bm_poly([(hx - 0.011, -0.213, -0.3408), (hx - 0.011, 0.213, -0.3408), (hx + 0.011, 0.213, -0.3408),
                                  (hx + 0.011, -0.213, -0.3408)]))
        decal(K, P['rust'], (hx, rng.uniform(-0.1, 0.1), -0.3409), (0, 1, 0), (1, 0, 0), 0.05, 0.015, 900 + v, lift=0.0002)
    return K.finish()


def build_cable_tray():
    _cable_tray(0)


def build_cable_tray__v1():
    _cable_tray(1)


def build_cable_tray__v2():
    _cable_tray(2)


def build_cable_tray__v3():
    _cable_tray(3)


# =====================================================================================================================
# Schaltschrank (switch_cabinet, 3 Varianten): Standschrank an der Wand (Rueckseite x = 0), 1,2 x 0,46 x 2,0 m, zwei
# Tueren mit Spalt und Dichtung, Scharniere, Schwenkhebelverschluss, Messinstrumente, Hauptschalter, Meldeleuchten,
# Beschriftung und Warnzeichen aus dem Atlas, Lueftungskiemen, Erdungsleitung; Abplatzer, Rost unten, Griffspuren.
# =====================================================================================================================
def _flat_layer(K, m, pts2d, x0, deform=None, cuts=0):
    """Ebene Flaeche (Umriss in y, z) bei x = x0, Normale +x; optional unterteilt und verformt (dx = deform(y, z))."""
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(pts2d, pts2d[1:] + pts2d[:1]))
    if area < 0:
        pts2d = list(reversed(pts2d))
    b = bmesh.new()
    b.faces.new([b.verts.new((x0, y, z)) for (y, z) in pts2d])
    if cuts:
        bmesh.ops.triangulate(b, faces=b.faces)
        bmesh.ops.subdivide_edges(b, edges=b.edges, cuts=cuts, use_grid_fill=True)
    if deform:
        for v in b.verts:
            v.co.x += deform(v.co.y, v.co.z)
    K.add(m, b)


def _ring_layer(K, m, outer, inner, x0, deform=None):
    """Ring zwischen zwei Umrissen gleicher Punktzahl (beide gegen den Uhrzeigersinn in y, z), Normale +x."""
    b = bmesh.new()
    o = [b.verts.new((x0, y, z)) for (y, z) in outer]
    i = [b.verts.new((x0, y, z)) for (y, z) in inner]
    n = len(o)
    for k in range(n):
        j = (k + 1) % n
        b.faces.new((o[k], o[j], i[j], i[k]))
    b.normal_update()
    b.faces.ensure_lookup_table()
    if b.faces[0].normal.x < 0:
        bmesh.ops.reverse_faces(b, faces=b.faces)
    if deform:
        for v in b.verts:
            v.co.x += deform(v.co.y, v.co.z)
    K.add(m, b)


def _switch_cabinet(v):
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(1200 + v)
    col = [(0.64, 0.65, 0.61), (0.62, 0.50, 0.22), (0.30, 0.35, 0.42)][v]
    paint = mat(f'schranklack{v}', col, 'paint', r=0.85)
    paint_worn = mat(f'schranklack_alt{v}', tuple(c * 0.86 for c in col), 'paint', r=1.2)
    base = mat('sockel_schwarz', (0.07, 0.07, 0.07), 'paint', tex='painted_metal_grey', r=1.2)
    D = 0.46
    # Sockel, Korpus, Dach mit Tropfkante
    K.box(base, (0.02, -0.59, 0.0), (D - 0.01, 0.59, 0.1), bev=0.003)
    K.box(paint, (0.0, -0.6, 0.1), (D, 0.6, 1.96), bev=0.004, seg=2)
    K.box(paint, (0.0, -0.612, 1.96), (D + 0.026, 0.612, 1.995), bev=0.003, seg=2)
    K.box(paint, (D + 0.020, -0.612, 1.944), (D + 0.026, 0.612, 1.962), bev=0.0015)
    # Tueren: links fest, rechts minimal verzogen
    for s in (-1, 1):
        y0, y1 = (0.003, 0.586) if s > 0 else (-0.586, -0.003)
        Md = Matrix.Identity(4)
        if s > 0:
            Md = T(D + 0.012, 0.592, 0) @ R(math.radians(-0.6), 'Z') @ T(-(D + 0.012), -0.592, 0)
        K.add(paint, bm_box((D, y0, 0.13), (D + 0.019, y1, 1.93), 0.003, 2), Md)
        # Kiemen unten (Schlitz dunkel, Lamelle schraeg)
        for i in range(5):
            z = 0.24 + i * 0.042
            yc = (y0 + y1) / 2
            K.add(P['hole'], bm_poly([(D + 0.0193, yc - 0.1, z), (D + 0.0193, yc + 0.1, z), (D + 0.0193, yc + 0.1, z + 0.014),
                                      (D + 0.0193, yc - 0.1, z + 0.014)]), Md)
            lam = bm_box((-0.0015, -0.1, -0.0015), (0.0015, 0.1, 0.019))
            K.add(paint, lam, Md @ T(D + 0.0205, yc, z) @ R(math.radians(-35), 'Y'))
        # Scharniere aussen
        for z in (0.35, 1.03, 1.71):
            K.cyl(P['steel_dark'], (D + 0.012, s * 0.593, z - 0.035), (D + 0.012, s * 0.593, z + 0.035), 0.0075, 8)
            K.cyl(P['steel_dark'], (D + 0.012, s * 0.593, z + 0.035), (D + 0.012, s * 0.593, z + 0.041), 0.004, 6)
    # Dichtung im Mittelspalt
    K.box(P['black_rubber'], (D - 0.004, -0.0035, 0.135), (D + 0.012, 0.0035, 1.925))
    # Verschluss rechts: Mulde, Schwenkhebel, Schliesszylinder
    K.box(P['black_plastic'], (D + 0.019, 0.032, 0.92), (D + 0.025, 0.074, 1.12), bev=0.002)
    K.box(P['black_plastic'], (D + 0.025, 0.043, 0.95), (D + 0.037, 0.063, 1.085), bev=0.003, seg=2)
    K.cyl(P['steel_bright'], (D + 0.025, 0.053, 1.103), (D + 0.031, 0.053, 1.103), 0.0075, 10)
    K.box(P['hole'], (D + 0.0312, 0.0515, 1.098), (D + 0.0315, 0.0545, 1.108))
    # Instrumente links oben
    meters = [(-0.43, 'A'), (-0.31, 'V'), (-0.19, 'KW')] if v != 2 else [(-0.40, 'A'), (-0.24, 'V')]
    stencil_b = mat('schrift_schwarz', (0.03, 0.03, 0.03), 'stencil')
    K.box(P['black_plastic'], (D + 0.019, meters[0][0] - 0.06, 1.52), (D + 0.023, meters[-1][0] + 0.06, 1.70), bev=0.002)
    for k, (yc, lab) in enumerate(meters):
        missing = (v == 1 and k == 1)
        if missing:
            K.add(P['hole'], bm_poly([(D + 0.0232, yc - 0.044, 1.566), (D + 0.0232, yc + 0.044, 1.566), (D + 0.0232, yc + 0.044, 1.654),
                                      (D + 0.0232, yc - 0.044, 1.654)]))
            continue
        K.box(P['black_plastic'], (D + 0.023, yc - 0.048, 1.562), (D + 0.03, yc + 0.048, 1.658), bev=0.002)
        K.add(P['white_plastic'], bm_poly([(D + 0.0302, yc - 0.042, 1.568), (D + 0.0302, yc + 0.042, 1.568),
                                            (D + 0.0302, yc + 0.042, 1.652), (D + 0.0302, yc - 0.042, 1.652)]))
        # Skalenbogen und Zeiger
        cz = 1.578
        arc_o = [(yc + 0.062 * math.cos(a), cz + 0.062 * math.sin(a)) for a in [math.radians(130 - 80 * t / 10) for t in range(11)]]
        arc_i = [(yc + 0.056 * math.cos(a), cz + 0.056 * math.sin(a)) for a in [math.radians(130 - 80 * t / 10) for t in range(11)]]
        b = bmesh.new()
        vo = [b.verts.new((D + 0.0305, y, z)) for (y, z) in arc_o]
        vi = [b.verts.new((D + 0.0305, y, z)) for (y, z) in arc_i]
        for i in range(10):
            b.faces.new((vi[i], vi[i + 1], vo[i + 1], vo[i]))
        b.normal_update()
        b.faces.ensure_lookup_table()
        if b.faces[0].normal.x < 0:
            bmesh.ops.reverse_faces(b, faces=b.faces)
        K.add(P['black_plastic'], b)
        for t in range(0, 11, 2):
            a = math.radians(130 - 80 * t / 10)
            p0 = (yc + 0.062 * math.cos(a), cz + 0.062 * math.sin(a))
            p1 = (yc + 0.068 * math.cos(a), cz + 0.068 * math.sin(a))
            dy, dz = -math.sin(a) * 0.0008, math.cos(a) * 0.0008
            K.add(P['black_plastic'], bm_poly([(D + 0.0305, p0[0] - dy, p0[1] - dz), (D + 0.0305, p0[0] + dy, p0[1] + dz),
                                               (D + 0.0305, p1[0] + dy, p1[1] + dz), (D + 0.0305, p1[0] - dy, p1[1] - dz)]))
        a = math.radians(rng.uniform(95, 125) if k != 2 else 129)
        tip = (yc + 0.064 * math.cos(a), cz + 0.064 * math.sin(a))
        nrm = (-math.sin(a) * 0.0009, math.cos(a) * 0.0009)
        K.add(mat('zeiger', (0.55, 0.06, 0.04), 'paint', r=0.8),
              bm_poly([(D + 0.0308, yc - nrm[0], cz - nrm[1]), (D + 0.0308, yc + nrm[0], cz + nrm[1]), (D + 0.0308, tip[0], tip[1])]))
        text(K, stencil_b, lab, (D + 0.0306, yc - 0.006 * len(lab), 1.6), (0, 1, 0), (0, 0, 1), 0.012)
    # Beschriftung, Warnzeichen, Hauptschalter, Meldeleuchten rechts oben
    label = ['UV-3', 'NV-1', 'UV-7'][v]
    K.box(P['black_plastic'], (D + 0.019, 0.17, 1.755), (D + 0.022, 0.43, 1.815), bev=0.001)
    text(K, mat('schild_weiss', (0.92, 0.92, 0.88), 'sign'), label, (D + 0.0222, 0.19, 1.765), (0, 1, 0), (0, 0, 1), 0.04, w=0.05, gap=0.1)
    sign_quad(K, mat('warnzeichen', (1.0, 1.0, 1.0), 'sign'), 41, (D + 0.0195, 0.30, 1.64), (0, 1, 0), (0, 0, 1), 0.11, 0.11)
    yellow = mat('schalter_gelb', (0.85, 0.68, 0.10), 'plastic', r=0.9)
    red = mat('schalter_rot', (0.62, 0.06, 0.04), 'plastic', r=0.7)
    K.box(yellow, (D + 0.019, 0.255, 1.255), (D + 0.024, 0.345, 1.345), bev=0.002)
    K.cyl(red, (D + 0.024, 0.30, 1.30), (D + 0.042, 0.30, 1.30), 0.024, 16)
    K.add(red, bm_box((0.0, -0.011, -0.055), (0.022, 0.011, 0.035), 0.004, 2), T(D + 0.04, 0.30, 1.30) @ R(math.radians(-40), 'X'))
    text(K, stencil_b, '0', (D + 0.0242, 0.262, 1.327), (0, 1, 0), (0, 0, 1), 0.012)
    text(K, stencil_b, 'I', (D + 0.0242, 0.330, 1.327), (0, 1, 0), (0, 0, 1), 0.012)
    lamps = [('gruen', (0.10, 0.45, 0.12)), ('rot', (0.55, 0.06, 0.04)), ('weiss', (0.85, 0.84, 0.78))]
    for k, (nm, c) in enumerate(lamps):
        yc = 0.20 + k * 0.05
        K.cyl(P['steel'], (D + 0.019, yc, 1.46), (D + 0.026, yc, 1.46), 0.016, 12)
        lit = (k == 0 and v == 0)
        lm = mat(f'meldeleuchte_{nm}{"_an" if lit else ""}', c, 'glow' if lit else 'plastic', glow=0.7 if lit else 0.0, r=0.35)
        K.add(lm, bm_lathe([(0.0115, 0.0), (0.011, 0.006), (0.007, 0.011), (0.0, 0.0125)], 12, cap0=False), frame((D + 0.026, yc, 1.46), (1, 0, 0)))
    # Seitliche Kiemen
    for s in (-1, 1):
        for i in range(4):
            z = 1.72 + i * 0.036
            yy = s * 0.6005
            pts = [(0.30, yy, z), (0.42, yy, z), (0.42, yy, z + 0.012), (0.30, yy, z + 0.012)]
            if s > 0:
                pts = list(reversed(pts))
            K.add(P['hole'], bm_poly(pts))
            K.add(paint, bm_box((-0.06, -0.0015, -0.0015), (0.06, 0.0015, 0.016)), T(0.36, s * 0.602, z) @ R(math.radians(35 * s), 'X'))
    # Erdungsleitung gruen-gelb an der linken Seite, in die Wand
    path = catmull([(0.09, -0.606, 0.19), (0.09, -0.618, 0.13), (0.075, -0.63, 0.03), (0.04, -0.65, 0.008), (-0.03, -0.66, 0.008)], 4)
    half = [(math.cos(a), math.sin(a)) for a in [math.pi * k / 4 for k in range(5)]]
    K.tube(mat('ader_gruen', (0.10, 0.42, 0.12), 'plastic', r=1.0), path, 0.0055, prof=half, closed=False)
    K.tube(mat('ader_gelb', (0.80, 0.66, 0.08), 'plastic', r=1.0), path, 0.0055, prof=[(-x, -y) for x, y in half], closed=False)
    bolt(K, P['steel'], (0.09, -0.6, 0.19), (0, -1, 0), d=0.008)
    # Gebrauchsspuren: Abplatzer an Kanten, Rost unten, Griffspuren, Kratzer, Staub auf dem Dach
    fx = D + 0.0195
    for j in range(30 + 10 * (v == 1)):
        s = rng.choice((-1, 1))
        edge = rng.choice(('mid', 'out', 'bot', 'bot', 'top'))
        if edge == 'mid':
            y, z = s * rng.uniform(0.004, 0.02), rng.uniform(0.2, 1.9)
        elif edge == 'out':
            y, z = s * rng.uniform(0.565, 0.584), rng.uniform(0.2, 1.9)
        elif edge == 'bot':
            y, z = s * rng.uniform(0.02, 0.58), rng.uniform(0.133, 0.15)
        else:
            y, z = s * rng.uniform(0.02, 0.58), rng.uniform(1.91, 1.927)
        m = P['rust'] if rng.random() < 0.45 else P['steel_dark']
        decal(K, m, (fx, y, z), (0, 1, 0), (0, 0, 1), rng.uniform(0.004, 0.014), rng.uniform(0.004, 0.02), 1300 + v * 50 + j, n=7, lift=0.0004)
    for s in (-1, 1):
        for j in range(3 + 2 * (v == 1)):
            y = s * rng.uniform(0.05, 0.55)
            decal(K, P['rust'], (fx, y, 0.15), (0, 1, 0), (0, 0, 1), rng.uniform(0.05, 0.14), rng.uniform(0.018, 0.05), 1400 + v * 30 + j + 9 * s,
                  n=9, lift=0.0006)
            streak(K, P['rust_dark'], (fx + 0.0007, y, 0.17), (0, 1, 0), (0, 0, -1), rng.uniform(0.008, 0.02), rng.uniform(0.02, 0.05), j * 7 + s)
    decal(K, P['grime'], (fx, 0.06, 1.0), (0, 1, 0), (0, 0, 1), 0.05, 0.11, 1500 + v, n=10, lift=0.0003)
    decal(K, paint_worn, (fx, 0.085, 1.02), (0, 1, 0), (0, 0, 1), 0.035, 0.09, 1501 + v, n=10, lift=0.0002)
    for j in range(6):
        a = rng.uniform(-0.6, 0.6)
        streak(K, P['steel_bright'], (fx + 0.0009, 0.05 + rng.uniform(-0.03, 0.03), 1.0 + rng.uniform(-0.08, 0.08)),
               (0, math.cos(a), math.sin(a)), (0, math.sin(a), -math.cos(a)), 0.0016, rng.uniform(0.03, 0.09), 1600 + j, n=3)
    for j in range(3):
        decal(K, P['dust_light'], (rng.uniform(0.18, 0.3), -0.32 + 0.32 * j + rng.uniform(-0.05, 0.05), 1.9955), (1, 0, 0), (0, 1, 0),
              rng.uniform(0.08, 0.12), rng.uniform(0.1, 0.16), 1700 + v * 5 + j, n=11, lift=0.0004, irr=0.25)
    for s in (-1, 1):  # Rostlaeufer an den Seiten vom Dach
        for j in range(2):
            x = rng.uniform(0.1, 0.4)
            streak(K, P['rust'], (x, s * 0.6008, 1.955), (-s, 0, 0), (0, 0, -1), rng.uniform(0.01, 0.025),
                   rng.uniform(0.1, 0.35), 1800 + j + 5 * s + v)
    return K.finish()


def build_switch_cabinet():
    _switch_cabinet(0)


def build_switch_cabinet__v1():
    _switch_cabinet(1)


def build_switch_cabinet__v2():
    _switch_cabinet(2)


# =====================================================================================================================
# Warnschild (sign_warning): emailliertes Blech 40 x 40 cm mit gebordeltem Rand, vier Schrauben, schwarzer Rahmen,
# Auge im Dreieck (Paranoia-Zone). Eine Ecke ist umgebogen, Email an Kanten und Schrauben abgeplatzt, Rostlaeufer.
# =====================================================================================================================
def build_sign_warning():
    P = _pal()
    K = Kit(deg=35.0)
    rng = random.Random(77)
    yel = mat('email_gelb', (0.92, 0.72, 0.08), 'paint', r=0.45)
    yel_dirty = mat('email_gelb_schmutz', (0.60, 0.46, 0.10), 'paint', r=0.95)
    blk = mat('email_schwarz', (0.025, 0.025, 0.028), 'paint', r=0.5)
    steel = mat('schild_blech', (0.20, 0.20, 0.21), 'metal', r=0.9)

    def Dd(y, z):  # untere linke Ecke nach vorn gebogen
        t = (-y - z - 0.25) / 0.15
        return 0.014 * max(0.0, t) ** 1.6 if t > 0 else 0.0

    def pt(y, z, x):
        return Vector((x + Dd(y, z), y, z))
    plate = rounded_rect(0.4, 0.4, 0.02, 4)
    # Platte: Vorder- und Rueckseite unterteilt (die Biegung braucht innere Punkte), Kante als Wulst
    front = [(y, z) for (y, z) in plate]
    _flat_layer(K, yel, front, 0.0042, Dd, cuts=3)
    back = bmesh.new()
    back.faces.new([back.verts.new((0.0018, y, z)) for (y, z) in reversed(front)])
    bmesh.ops.triangulate(back, faces=back.faces)
    bmesh.ops.subdivide_edges(back, edges=back.edges, cuts=3, use_grid_fill=True)
    for v in back.verts:
        v.co.x += Dd(v.co.y, v.co.z)
    K.add(steel, back)
    # gebordelter Rand
    rim = [pt(y * 0.995, z * 0.995, 0.0032) for (y, z) in plate]
    K.tube(yel, rim + [rim[0]], 0.0021, 6, caps=False)
    # schwarzer Rahmen
    ro = rounded_rect(0.37, 0.37, 0.012, 4)
    ri = rounded_rect(0.34, 0.34, 0.006, 4)
    _ring_layer(K, blk, ro, ri, 0.0046, Dd)
    # Dreieck (Umriss), Auge mit Wimpern
    def tri(rad):
        c = (0.0, -0.01)
        out = []
        for a0 in (90, 210, 330):
            for k in range(3):
                a = math.radians(a0 - 20 + 20 * k)
                out.append((c[0] + rad * math.cos(math.radians(a0)) + 0.012 * math.cos(a), c[1] + rad * math.sin(math.radians(a0)) + 0.012 * math.sin(a)))
        return out
    _ring_layer(K, blk, tri(0.150), tri(0.118), 0.0046, Dd)
    eo, ei = [], []
    for k in range(24):
        t = -1 + 2 * k / 12 if k <= 12 else 1 - 2 * (k - 12) / 12
        up = k <= 12
        y = 0.075 * t
        h = 0.040 * (1 - t * t) ** 0.9
        eo.append((y * 1.0, -0.028 + (h if up else -h * 0.85)))
        ei.append((y * 0.86, -0.028 + (h * 0.72 if up else -h * 0.62)))
    eo2 = eo[:12] + eo[12:]
    _ring_layer(K, blk, list(reversed(eo2)), list(reversed(ei)), 0.0047, Dd)
    iris = [(0.024 * math.cos(a), -0.028 + 0.024 * math.sin(a)) for a in [TAU * k / 16 for k in range(16)]]
    _flat_layer(K, blk, iris, 0.0048, Dd)
    glint = [(0.008 + 0.006 * math.cos(a), -0.018 + 0.006 * math.sin(a)) for a in [TAU * k / 8 for k in range(8)]]
    _flat_layer(K, yel, glint, 0.0050, Dd)
    for k in range(5):
        a = math.radians(60 + 15 * k)
        y0, z0 = 0.062 * math.cos(a), -0.028 + 0.046 * math.sin(a)
        y1, z1 = 0.080 * math.cos(a), -0.028 + 0.064 * math.sin(a)
        dy, dz = -math.sin(a) * 0.0035, math.cos(a) * 0.0035
        _flat_layer(K, blk, [(y0 - dy, z0 - dz), (y1 - dy, z1 - dz), (y1 + dy, z1 + dz), (y0 + dy, z0 + dz)], 0.0046, Dd)
    # Schmutz unten (unter den schwarzen Flaechen)
    dirt = [(-0.19, -0.19)] + [(-0.19 + 0.38 * k / 10, -0.135 + 0.02 * math.sin(k * 1.7) + rng.uniform(-0.01, 0.01)) for k in range(11)] + [(0.19, -0.19)]
    _flat_layer(K, yel_dirty, list(reversed(dirt)), 0.0043, Dd, cuts=1)
    # Schrauben mit Abplatzern und Rostlaeufern
    for (y, z) in ((-0.172, 0.172), (0.172, 0.172), (-0.172, -0.172), (0.172, -0.172)):
        p = pt(y, z, 0.0042)
        cp = [(y + 0.013 * u, z + 0.013 * w) for (u, w) in blob2d(9, int(y * 100 + z * 1000), 0.3)]
        _flat_layer(K, P['rust'], cp, 0.0049, Dd)
        cp2 = [(y + 0.008 * u, z + 0.008 * w) for (u, w) in blob2d(8, int(y * 300 + z * 100), 0.3)]
        _flat_layer(K, steel, cp2, 0.0050, Dd)
        slot_screw(K, P['steel_dark'], P['hole'], p + Vector((0.0009, 0, 0)), (1, 0, 0), d=0.011, rot=rng.uniform(0, 3))
        if z < 0:
            streak(K, P['rust'], (p.x + 0.0011, y, z - 0.004), (0, 1, 0), (0, 0, -1), 0.006, rng.uniform(0.02, 0.04), int(y * 50 + 9))
    # Abplatzer an den Kanten
    for j in range(9):
        side = rng.randrange(4)
        u = rng.uniform(-0.18, 0.18)
        y, z = [(u, 0.196), (u, -0.196), (0.196, u), (-0.196, u)][side]
        r0 = rng.uniform(0.006, 0.016)
        cp = [(y + r0 * a, z + r0 * c) for (a, c) in blob2d(8, 1900 + j, 0.35)]
        _flat_layer(K, P['rust'] if j % 3 else steel, cp, 0.0051, Dd)
    # Rostlauf auf der Wand unter dem Schild
    for y in (-0.15, 0.15, 0.04):
        streak(K, P['rust_dark'], (0.0008, y, -0.2), (0, 1, 0), (0, 0, -1), rng.uniform(0.006, 0.014), rng.uniform(0.04, 0.11), int(y * 77 + 3))
    return K.finish()


# =====================================================================================================================
# Deckenleuchte (ceiling_globe): Bronzerosette mit Kette, Galerie mit drei Raendelschrauben, Opalglaskugel mit Hals
# (leuchtet), unten Staub und Insekten in der Kugel. Decke z = 0, haengt bis 0,65 m.
# =====================================================================================================================
def build_ceiling_globe():
    K = Kit(deg=40.0)
    bronze = mat('bronze', (0.42, 0.30, 0.16), 'metal', r=0.85)
    patina = mat('bronze_patina', (0.17, 0.14, 0.10), 'metal', r=1.2)
    glass = mat('opalglas', (1.0, 0.86, 0.64), 'glow', glow=1.0)
    glass_dirty = mat('opalglas_staub', (0.55, 0.44, 0.30), 'glow', glow=0.7)
    # Rosette
    K.lathe(bronze, list(reversed([(0.078, -0.002), (0.078, -0.010), (0.072, -0.016), (0.074, -0.020), (0.066, -0.026), (0.048, -0.036),
                                   (0.026, -0.044), (0.012, -0.048), (0.0, -0.049)])), 32)
    K.add(bronze, bm_cyl(0.079, 0.002, 32, z0=-0.002))
    # Oese und Kette
    K.tube(bronze, arc_pts((0, 0, -0.058), 0.010, 0, TAU, 10, u=(1, 0, 0), v=(0, 0, 1))[:-1], 0.0028, 6, caps=False, path_closed=True)
    z = -0.066
    for i in range(7):
        L = 0.027
        rot = (1, 0, 0) if i % 2 else (0, 1, 0)
        c = Vector((0, 0, z - L / 2 + 0.004))
        pts = []
        for k in range(12):
            a = TAU * k / 12
            pts.append(c + Vector(rot) * (0.0075 * math.cos(a)) + Vector((0, 0, 1)) * ((L / 2 - 0.0035) * math.sin(a)))
        K.tube(bronze if i % 3 else patina, pts, 0.0024, 5, caps=False, path_closed=True)
        z -= L - 0.007
    # Haken und Galerie
    gz = z - 0.004
    K.tube(bronze, arc_pts((0, 0, gz - 0.008), 0.009, 0, TAU, 10, u=(0, 1, 0), v=(0, 0, 1))[:-1], 0.003, 6, caps=False, path_closed=True)
    K.lathe(bronze, list(reversed([(0.0, gz - 0.016), (0.016, gz - 0.017), (0.018, gz - 0.034), (0.030, gz - 0.046), (0.050, gz - 0.058),
                                   (0.064, gz - 0.064), (0.067, gz - 0.072), (0.066, gz - 0.088), (0.062, gz - 0.090), (0.060, gz - 0.088),
                                   (0.060, gz - 0.072)])), 32, cap0=False)
    for k in range(3):
        a = TAU * k / 3 + 0.4
        d = Vector((math.cos(a), math.sin(a), 0))
        p0 = Vector((0, 0, gz - 0.081)) + d * 0.066
        K.cyl(bronze, p0, p0 + d * 0.012, 0.0028, 6)
        K.add(bronze, bm_cyl(0.0065, 0.006, 10, z0=0.0), frame(p0 + d * 0.012, d))
    # Glaskugel mit Hals (Hals steckt in der Galerie)
    top = gz - 0.074
    R0 = 0.148
    cz = top - 0.042 - R0 * 0.93
    prof = [(0.0, cz - R0)]
    for k in range(1, 15):
        a = -math.pi / 2 + math.pi * 0.89 * k / 15
        prof.append((R0 * math.cos(a), cz + R0 * math.sin(a)))
    prof += [(0.058, top - 0.030), (0.054, top - 0.020), (0.054, top + 0.004)]
    gl = bm_lathe(prof, 28, cap1=True)
    K.add_split(gl, lambda f: glass_dirty if f.calc_center_median().z < cz - R0 * 0.82 else glass)
    # Fliegen und Staub als dunkle Flecken innen unten (auf der Kugel aussen sichtbar)
    rng = random.Random(9)
    for j in range(5):
        a = rng.uniform(0, TAU)
        el = rng.uniform(-1.35, -1.05)
        p = Vector((math.cos(a) * math.cos(el), math.sin(a) * math.cos(el), math.sin(el))) * (R0 + 0.0008) + Vector((0, 0, cz))
        nrm = (p - Vector((0, 0, cz))).normalized()
        u = nrm.cross(Vector((0, 0, 1))).normalized()
        decal(K, mat('insekt', (0.06, 0.05, 0.04), 'glow', glow=0.25), p, u, nrm.cross(u), rng.uniform(0.004, 0.009),
              rng.uniform(0.003, 0.006), 2000 + j, n=7, lift=0.0)
    return K.finish()


# =====================================================================================================================
# Hilfen fuer Rohre, Flansche, Ventile, Lachen
# =====================================================================================================================
def cpoly(K, m, a, axis, ref, R0, pts, lift=0.0012, cuts=2):
    """Umriss in (Winkel, Achsposition) auf einen Zylindermantel legen (unterteilt, folgt der Woelbung)."""
    a, axis, ref = Vector(a), Vector(axis).normalized(), Vector(ref).normalized()
    side = axis.cross(ref)
    b = bmesh.new()
    b.faces.new([b.verts.new((th * R0, t, 0.0)) for th, t in pts])
    bmesh.ops.triangulate(b, faces=b.faces)
    if cuts:
        bmesh.ops.subdivide_edges(b, edges=b.edges, cuts=cuts, use_grid_fill=True)
    rr = R0 + lift
    for v in b.verts:
        th, t = v.co.x / R0, v.co.y
        v.co = a + axis * t + (ref * math.cos(th) + side * math.sin(th)) * rr
    b.normal_update()
    b.faces.ensure_lookup_table()
    f = b.faces[0]
    c = f.calc_center_median()
    radial = c - (a + axis * axis.dot(c - a))
    if f.normal.dot(radial) < 0:
        bmesh.ops.reverse_faces(b, faces=b.faces)
    K.add(m, b)


def flange_pair(K, mf, mb, mg, p, axis, rp, rng, nb=8, rf=None, t=0.02):
    """Flanschverbindung an p (Rohrachse axis, Rohrradius rp): zwei Flansche mit Dichtung, nb Schrauben mit Muttern."""
    p, ax = Vector(p), Vector(axis).normalized()
    rf = rf or rp * 1.7 + 0.03
    M = frame(p, ax)
    K.add(mf, bm_lathe([(rp * 1.02, -t - 0.0015), (rf, -t - 0.0015), (rf, -0.0015), (rp * 1.25, -0.0015)], 24, cap0=False, cap1=False), M)
    K.add(mf, bm_lathe([(rp * 1.25, 0.0015), (rf, 0.0015), (rf, t + 0.0015), (rp * 1.02, t + 0.0015)], 24, cap0=False, cap1=False), M)
    for z0, z1 in ((-t - 0.0015, -t - 0.035), (t + 0.0015, t + 0.035)):  # Schweisshals
        K.add(mf, bm_lathe([(rp * 1.30, z0), (rp * 1.02, z1)] if z1 > 0 else [(rp * 1.02, z1), (rp * 1.30, z0)], 24, cap0=False, cap1=False), M)
    K.add(mg, bm_lathe([(rp * 1.25, -0.0015), (rp * 1.45, -0.0015), (rp * 1.45, 0.0015), (rp * 1.25, 0.0015)], 24, cap0=False, cap1=False), M)
    rb = (rp * 1.25 + rf) / 2 + 0.004
    db = min(0.02, (rf - rp) * 0.32)
    a0 = rng.uniform(0, 0.3)
    for k in range(nb):
        a = a0 + TAU * (k + 0.5) / nb
        q = M @ Vector((rb * math.cos(a), rb * math.sin(a), 0.0))
        bolt(K, mb, q - ax * (t + 0.0015), -ax, d=db, rot=rng.random(), washer=False)
        bolt(K, mb, q + ax * (t + 0.0015), ax, d=db, rot=rng.random(), stub=db * 0.7, washer=False)


def handwheel(K, m, mh, c, ax, R0, spokes=4, rng=None):
    """Handrad: Kranz (Wulst), Speichen, Nabe."""
    c, ax = Vector(c), Vector(ax).normalized()
    M = frame(c, ax)
    rim = [M @ Vector((R0 * math.cos(a), R0 * math.sin(a), 0.004 * math.sin(3 * a))) for a in [TAU * k / 24 for k in range(24)]]
    K.tube(m, rim, (0.011, 0.008), 6, caps=False, path_closed=True)
    a0 = rng.uniform(0, 1) if rng else 0.3
    for k in range(spokes):
        a = a0 + TAU * k / spokes
        K.tube(m, [M @ Vector((0.02 * math.cos(a), 0.02 * math.sin(a), -0.012)), M @ Vector((R0 * 0.55 * math.cos(a), R0 * 0.55 * math.sin(a), -0.006)),
                   M @ Vector((R0 * 0.95 * math.cos(a), R0 * 0.95 * math.sin(a), 0.0))], (0.007, 0.0045), 6, caps=True)
    K.add(mh, bm_lathe([(0.028, -0.03), (0.03, -0.012), (0.024, 0.004), (0.012, 0.012), (0.0, 0.013)], 12), M)


def sludge(K, P, c, R0, seed, crust=True, clip_x=None, h=0.006, bubbles=4):
    """Lache aus schwarzem Schlamm: unregelmaessiger Rand, gewoelbte Oberflaeche mit Blasen und Haut, Kruste am Rand,
    Spritzer. c = Mitte (x, y), R0 = Radius."""
    rng = random.Random(seed)
    NS, NR = 40, 7
    ph = [rng.uniform(0, TAU) for _ in range(5)]

    def rim(th):
        r = R0 * (1 + 0.22 * math.sin(2 * th + ph[0]) + 0.14 * math.sin(3 * th + ph[1]) + 0.08 * math.sin(5 * th + ph[2])
                  + 0.05 * math.sin(9 * th + ph[3]))
        return r
    bub = [(rng.uniform(0, TAU), rng.uniform(0.1, 0.7), rng.uniform(0.012, 0.03)) for _ in range(bubbles)]
    b = bmesh.new()
    rings = []
    ctr = b.verts.new((c[0], c[1], h + 0.001))
    for k in range(1, NR + 1):
        rho = k / NR
        ring = []
        for i in range(NS):
            th = TAU * i / NS
            r = rim(th) * rho
            x, y = c[0] + r * math.cos(th), c[1] + r * math.sin(th)
            if clip_x is not None and x < clip_x:
                x = clip_x
            z = 0.0014 + h * (1 - rho ** 6) ** 0.5
            z += 0.0012 * noise.noise(Vector((x * 14, y * 14, seed))) * (1 - rho ** 4)
            for (bt, br, bs) in bub:
                bx, by = c[0] + rim(bt) * br * math.cos(bt), c[1] + rim(bt) * br * math.sin(bt)
                d2 = ((x - bx) ** 2 + (y - by) ** 2) / (bs * bs)
                z += 0.7 * bs * math.exp(-d2 * 2.5) * (1 - rho ** 4)
            ring.append(b.verts.new((x, y, z)))
        rings.append(ring)
    for i in range(NS):
        j = (i + 1) % NS
        b.faces.new((ctr, rings[0][i], rings[0][j]))
    for k in range(NR - 1):
        for i in range(NS):
            j = (i + 1) % NS
            b.faces.new((rings[k][i], rings[k + 1][i], rings[k + 1][j], rings[k][j]))
    # Rand nach unten
    low = [b.verts.new((v.co.x, v.co.y, 0.0004)) for v in rings[-1]]
    for i in range(NS):
        j = (i + 1) % NS
        b.faces.new((rings[-1][i], low[i], low[j], rings[-1][j]))
    b.normal_update()
    for f in b.faces:
        if f.normal.z < -0.3:
            f.normal_flip()

    def pick(f):
        cc = f.calc_center_median()
        if noise.noise(Vector((cc.x * 6, cc.y * 6, seed * 0.37))) > 0.25 and f.normal.z > 0.8:
            return P['sludge_skin']
        return P['sludge']
    K.add_split(b, pick)
    if crust:  # Kruste: Band ausserhalb des Randes, matt, mit Rissen (Luecken)
        for i in range(NS):
            if rng.random() < 0.12:
                continue
            th0, th1 = TAU * i / NS, TAU * (i + 1) / NS - 0.01
            w0, w1 = rng.uniform(0.006, 0.022) * R0 / 0.5, rng.uniform(0.006, 0.022) * R0 / 0.5
            pts = []
            for th, w in ((th0, w0), (th1, w1)):
                r = rim(th)
                pts.append((c[0] + (r - 0.004) * math.cos(th), c[1] + (r - 0.004) * math.sin(th)))
            for th, w in ((th1, w1), (th0, w0)):
                r = rim(th) + w
                pts.append((c[0] + r * math.cos(th), c[1] + r * math.sin(th)))
            if clip_x is not None and min(x for x, y in pts) < clip_x:
                continue
            q = [Vector((x, y, 0.0012 + rng.uniform(0, 0.0012))) for x, y in pts]
            bb = bmesh.new()
            f = bb.faces.new([bb.verts.new(v) for v in q])
            f.normal_update()
            if f.normal.z < 0:
                f.normal_flip()
            K.add(P['crust'], bb)
    for j in range(rng.randrange(4, 9)):  # Spritzer
        th = rng.uniform(0, TAU)
        r = rim(th) * rng.uniform(1.15, 1.6)
        x, y = c[0] + r * math.cos(th), c[1] + r * math.sin(th)
        if clip_x is not None and x < clip_x + 0.02:
            continue
        s = rng.uniform(0.006, 0.02)
        K.add(P['sludge'], bm_lathe([(s, 0.0004), (s * 0.8, 0.0018), (0.0, 0.0024)], 8, cap0=False), T(x, y, 0))


# =====================================================================================================================
# Kessel (boiler): liegender Flammrohrkessel, genietet. Drei Schuesse mit Ueberlappung und Nietreihen, gewoelbte
# Boeden, Stirnplatte mit Feuer- und Aschentuer, Wasserstandsglas, Manometer am Siphon, Dampfdom mit Absperrventil und
# isolierter Dampfleitung bis zur Decke (3,2 m), Sicherheitsventil mit Gewichtshebel, Mannloch, Speise- und
# Abschlammleitung; auf zwei gemauerten Saetteln. Schwarzer Hitzelack, Rost an Naehten, Russ an der Feuertuer.
# =====================================================================================================================
def build_boiler():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(4040)
    lack = mat('kessellack', (0.075, 0.068, 0.062), 'paint', r=1.25)
    lack2 = mat('kessellack_fleckig', (0.13, 0.11, 0.095), 'paint', r=1.35)
    soot = mat('russ', (0.012, 0.011, 0.010), 'concrete', r=1.5)
    cast = mat('guss', (0.10, 0.10, 0.10), 'metal', r=1.3, m=0.4)
    brick = mat('ziegel_sattel', (0.92, 0.80, 0.72), 'concrete', tex='bricks_brown_old', r=1.2)
    capst = mat('sattel_stein', (0.52, 0.50, 0.46), 'concrete', tex='concrete_rough', r=1.3)
    lag = mat('isolierung', (0.72, 0.70, 0.64), 'fabric', r=1.4)
    white = mat('zifferblatt', (0.86, 0.85, 0.80), 'plastic', r=0.8)
    zc, Rs = 1.15, 0.82
    X = Vector((1, 0, 0))
    # Schuesse (Mantelringe) mit Ueberlappung
    for (x0, x1, r) in ((-1.42, -0.47, Rs), (-0.49, 0.49, Rs + 0.012), (0.47, 1.42, Rs)):
        K.add(lack, bm_cyl(r, x1 - x0, 48, caps=False), frame((x0, 0, zc), X))
    for xs, rr in ((-0.49, Rs + 0.012), (0.49, Rs + 0.012)):  # Stufe am Ueberlappungsrand
        K.add(lack, bm_lathe([(rr, 0.0), (Rs, 0.0)] if xs > 0 else [(Rs, 0.0), (rr, 0.0)], 48, cap0=False, cap1=False), frame((xs, 0, zc), X))
    # Boeden (gewoelbt) vorn und hinten
    dish = [(0.0, 0.135), (0.30, 0.125), (0.55, 0.10), (0.72, 0.065), (0.80, 0.03), (Rs, 0.0)]
    K.add(lack, bm_lathe(list(reversed([(r, z) for r, z in dish])), 48, cap0=False), frame((1.42, 0, zc), X))
    K.add(lack, bm_lathe(list(reversed([(r, z) for r, z in dish])), 48, cap0=False), frame((-1.42, 0, zc), -X))
    # Nietreihen: Rundnaehte an den Ueberlappungen und Boeden, Laengsnaht oben
    for xr, rr in ((-0.475, Rs + 0.012), (0.475, Rs + 0.012), (-0.46, Rs), (0.46, Rs), (1.40, Rs), (-1.40, Rs)):
        n = 52
        for k in range(n):
            a = TAU * k / n
            nrm = Vector((0, math.cos(a), math.sin(a)))
            rivet(K, lack, Vector((xr, 0, zc)) + nrm * rr, nrm, d=0.022)
    for (x0, x1, r) in ((-1.38, -0.53, Rs), (-0.45, 0.45, Rs + 0.012), (0.53, 1.38, Rs)):
        a = math.radians(70)
        nrm = Vector((0, math.cos(a), math.sin(a)))
        K.add(lack, bm_lathe([(r, 0.0), (r + 0.008, 0.0), (r + 0.008, x1 - x0), (r, x1 - x0)], 6, cap0=False, cap1=False, a0=a - 0.075, arc=0.15),
              frame((x0, 0, zc), X))
        for k in range(int((x1 - x0) / 0.075)):
            for da in (-0.045, 0.045):
                nn = Vector((0, math.cos(a + da), math.sin(a + da)))
                rivet(K, lack, Vector((x0 + 0.04 + k * 0.075, 0, zc)) + nn * (r + 0.008), nn, d=0.02)
    # Stirnplatte mit Feuertuer und Aschentuer
    fx = 1.555
    K.box(cast, (1.50, -0.52, 0.52), (fx, 0.52, 1.46), bev=0.008, seg=2)
    for (z0, z1, hw) in ((0.92, 1.30, 0.27), (0.58, 0.80, 0.25)):
        K.box(P['hole'], (fx, -hw, z0), (fx + 0.001, hw, z1))
        K.box(cast, (fx + 0.001, -hw - 0.02, z0 - 0.02), (fx + 0.035, hw + 0.02, z1 + 0.02), bev=0.006, seg=2)
        K.box(cast, (fx + 0.035, -hw + 0.03, z0 + 0.02), (fx + 0.05, hw - 0.03, z1 - 0.02), bev=0.008, seg=2)
        for zz in (z0 + 0.05, z1 - 0.05):  # Scharniere links
            K.cyl(cast, (fx + 0.02, -hw - 0.045, zz - 0.035), (fx + 0.02, -hw - 0.045, zz + 0.035), 0.016, 10)
            K.box(cast, (fx + 0.008, -hw - 0.045, zz - 0.02), (fx + 0.032, -hw - 0.01, zz + 0.02))
        # Riegel rechts
        K.box(P['steel_dark'], (fx + 0.05, hw - 0.09, (z0 + z1) / 2 - 0.012), (fx + 0.07, hw + 0.05, (z0 + z1) / 2 + 0.012), bev=0.003)
        K.box(cast, (fx + 0.005, hw + 0.04, (z0 + z1) / 2 - 0.03), (fx + 0.06, hw + 0.075, (z0 + z1) / 2 + 0.03), bev=0.004)
    # Luftschieber (Drehscheibe mit Schlitzen) auf der Feuertuer
    K.cyl(cast, (fx + 0.05, 0.0, 1.11), (fx + 0.062, 0.0, 1.11), 0.085, 24)
    for k in range(6):
        a = TAU * k / 6
        p0 = Vector((fx + 0.0625, 0.03 * math.cos(a), 1.11 + 0.03 * math.sin(a)))
        p1 = Vector((fx + 0.0625, 0.075 * math.cos(a), 1.11 + 0.075 * math.sin(a)))
        tg = Vector((0, -math.sin(a), math.cos(a))) * 0.012
        K.add(P['hole'], bm_poly([p0 - tg, p1 - tg * 1.6, p1 + tg * 1.6, p0 + tg]))
    K.cyl(P['steel_dark'], (fx + 0.062, 0, 1.11), (fx + 0.09, 0, 1.11), 0.012, 8)
    # Russ ueber der Feuertuer, Rost an der Stirnplatte
    decal(K, soot, (fx + 0.0012, 0.0, 1.38), (0, 1, 0), (0, 0, 1), 0.3, 0.07, 41, n=12, lift=0.0)
    for j in range(4):
        streak(K, P['rust'], (fx + 0.0012, rng.uniform(-0.45, 0.45), 0.53), (0, 1, 0), (0, 0, -1), rng.uniform(0.02, 0.05),
               rng.uniform(0.05, 0.15), 42 + j)
    # Wasserstandsglas
    gy = 0.42
    for zz in (1.52, 1.86):
        K.cyl(P['steel'], (1.50, gy, zz), (1.58, gy, zz), 0.022, 10)
        K.add(P['steel'], bm_box((-0.03, -0.028, -0.03), (0.03, 0.028, 0.03), 0.006), T(1.60, gy, zz))
        K.cyl(P['steel_dark'], (1.60, gy - 0.028, zz), (1.60, gy - 0.07, zz), 0.008, 6)
        K.cyl(P['steel_dark'], (1.60, gy - 0.07, zz - 0.03), (1.60, gy - 0.07, zz + 0.03), 0.006, 6)
    K.cyl(P['glass'], (1.60, gy, 1.55), (1.60, gy, 1.83), 0.012, 10)
    K.cyl(mat('wasser_glas', (0.20, 0.17, 0.12), 'glass'), (1.60, gy, 1.552), (1.60, gy, 1.66), 0.0125, 10)
    for dy in (-0.022, 0.022):
        K.cyl(P['steel'], (1.625, gy + dy, 1.55), (1.625, gy + dy, 1.83), 0.003, 5)
    K.cyl(P['steel'], (1.60, gy, 1.49), (1.60, gy, 1.40), 0.008, 6)
    streak(K, mat('kalk', (0.70, 0.68, 0.62), 'concrete', r=1.5), (1.5205, gy, 1.50), (0, 1, 0), (0, 0, -1), 0.03, 0.4, 51)
    # Manometer am Siphon
    sp = [Vector((1.30, -0.36, zc + 0.73)), Vector((1.30, -0.36, 2.05)), Vector((1.36, -0.36, 2.10))]
    sp += [Vector((1.40, -0.36 + 0.035 * math.sin(a), 2.13 + 0.035 * math.cos(a))) for a in [TAU * k / 10 for k in range(10)]]
    sp += [Vector((1.44, -0.36, 2.10)), Vector((1.46, -0.36, 2.20))]
    K.tube(P['steel'], bent_path(sp, 0.02, 2), 0.0075, 6)
    gc = Vector((1.46, -0.36, 2.29))
    K.add(P['steel_dark'], bm_lathe([(0.0, -0.025), (0.088, -0.024), (0.095, -0.016), (0.095, 0.012), (0.088, 0.018), (0.082, 0.014)], 28),
          frame(gc, X))
    K.cyl(P['steel_dark'], gc + Vector((0, 0, -0.09)), gc + Vector((0, 0, -0.07)), 0.014, 8)
    K.add(white, bm_cyl(0.081, 0.002, 28, z0=0.010), frame(gc, X))
    for k in range(9):
        a = math.radians(225 - 270 * k / 8)
        p0 = gc + Vector((0.0145, 0.068 * math.cos(a), 0.068 * math.sin(a)))
        p1 = gc + Vector((0.0145, 0.078 * math.cos(a), 0.078 * math.sin(a)))
        tg = Vector((0, -math.sin(a), math.cos(a))) * 0.0015
        K.add(P['black_plastic'], bm_poly([p0 - tg, p1 - tg, p1 + tg, p0 + tg]) if True else None)
    red = mat('marke_rot', (0.6, 0.05, 0.03), 'paint', r=0.8)
    a = math.radians(-20)
    K.add(red, bm_poly([gc + Vector((0.0146, 0.07 * math.cos(a - 0.05), 0.07 * math.sin(a - 0.05))),
                        gc + Vector((0.0146, 0.08 * math.cos(a - 0.05), 0.08 * math.sin(a - 0.05))),
                        gc + Vector((0.0146, 0.08 * math.cos(a + 0.05), 0.08 * math.sin(a + 0.05))),
                        gc + Vector((0.0146, 0.07 * math.cos(a + 0.05), 0.07 * math.sin(a + 0.05)))]))
    a = math.radians(150)
    K.add(P['black_plastic'], bm_poly([gc + Vector((0.015, -0.004 * math.sin(a), 0.004 * math.cos(a))),
                                       gc + Vector((0.015, 0.066 * math.cos(a), 0.066 * math.sin(a))),
                                       gc + Vector((0.015, 0.004 * math.sin(a), -0.004 * math.cos(a)))]))
    K.add(P['glass'], bm_cyl(0.083, 0.002, 28, z0=0.016), frame(gc, X))
    # Dampfdom, Absperrventil, isolierte Dampfleitung bis zur Decke
    dx = 0.25
    K.add(lack, bm_cyl(0.26, 0.40, 36, caps=False, z0=zc + 0.62), T(dx, 0, 0))
    K.add(lack, bm_lathe([(0.26, 0.0), (0.27, 0.0), (0.27, 0.03), (0.20, 0.06), (0.10, 0.08), (0.0, 0.085)], 36, cap0=False), T(dx, 0, zc + 1.02))
    for k in range(20):
        a = TAU * k / 20
        rivet(K, lack, Vector((dx + 0.262 * math.cos(a), 0.262 * math.sin(a), zc + 0.70)), Vector((math.cos(a), math.sin(a), 0)), d=0.02)
    vz = zc + 1.10
    K.add(cast, bm_cyl(0.11, 0.022, 20, z0=vz), T(dx, 0, 0))
    K.add(cast, bm_lathe([(0.0, 0.0), (0.075, 0.01), (0.10, 0.07), (0.09, 0.13), (0.06, 0.17), (0.0, 0.18)], 16), T(dx, 0, vz + 0.02))
    K.add(cast, bm_cyl(0.06, 0.12, 6, z0=vz + 0.17), T(dx, 0, 0))
    K.cyl(P['steel_bright'], (dx, 0, vz + 0.29), (dx, 0, vz + 0.40), 0.012, 8)
    handwheel(K, mat('handrad_rot', (0.42, 0.07, 0.05), 'paint', r=1.1), cast, (dx, 0, vz + 0.40), (0, 0, 1), 0.16, 5, rng)
    # Abgang zur Seite, dann hoch zur Decke (isoliert, Blechbaender)
    K.cyl(cast, (dx, 0, vz + 0.10), (dx - 0.24, 0, vz + 0.10), 0.06, 16)
    flange_pair(K, cast, P['steel_dark'], P['black_rubber'], (dx - 0.1, 0, vz + 0.10), (1, 0, 0), 0.06, rng, nb=6)
    path = bent_path([(dx - 0.24, 0, vz + 0.10), (-0.45, 0, vz + 0.10), (-0.45, 0, 3.25)], 0.18, 5)
    K.tube(P['steel_dark'], path, 0.062, 12, caps=False)
    lagp = [p for p in path if p.z > vz + 0.25 or p.x < -0.36]
    K.tube(lag, [Vector((-0.45, 0, vz + 0.30)), Vector((-0.45, 0, 3.2))], 0.105, 16, caps=True)
    for zz in (vz + 0.45, vz + 0.85, vz + 1.25):
        if zz < 3.15:
            K.add(P['galv_dark'], bm_cyl(0.108, 0.025, 16, caps=False, z0=zz), T(-0.45, 0, 0))
    decal(K, mat('isolierung_fleck', (0.45, 0.40, 0.33), 'fabric', r=1.5), (-0.45, -0.1055, vz + 0.6), (1, 0, 0), (0, 0, 1),
          0.05, 0.12, 61, lift=0.0)
    K.add(P['galv_dark'], bm_cyl(0.16, 0.012, 20, z0=-0.012), T(-0.45, 0, 3.2))
    # Sicherheitsventil mit Gewichtshebel
    sx = -0.55
    K.add(cast, bm_cyl(0.07, 0.03, 16, z0=zc + Rs - 0.01), T(sx, 0, 0))
    K.add(cast, bm_lathe([(0.0, 0.0), (0.06, 0.0), (0.065, 0.06), (0.045, 0.14), (0.03, 0.2), (0.0, 0.21)], 14), T(sx, 0, zc + Rs + 0.02))
    K.cyl(P['steel_dark'], (sx - 0.06, 0, zc + Rs + 0.20), (sx + 0.55, 0, zc + Rs + 0.24), 0.012, 8)
    K.box(P['steel_dark'], (sx - 0.08, -0.02, zc + Rs + 0.02), (sx - 0.05, 0.02, zc + Rs + 0.23))
    K.add(cast, bm_cyl(0.065, 0.12, 16, z0=-0.06), frame((sx + 0.48, 0, zc + Rs + 0.235), (0, 1, 0)))
    K.cyl(P['steel'], (sx + 0.06, 0.07, zc + Rs + 0.15), (sx + 0.06, 0.07, zc + Rs + 0.45), 0.02, 10)  # Abblaserohr
    streak(K, mat('kalk', (0.70, 0.68, 0.62), 'concrete', r=1.5), (sx + 0.08, 0.055, zc + Rs), (1, 0, 0), (0, -0.3, -1), 0.05, 0.4, 71)
    # Mannloch oben
    mx = -1.05
    K.add(lack, bm_cyl(0.20, 0.06, 28, z0=zc + Rs - 0.03), T(mx, 0, 0) @ Matrix.Diagonal((1.0, 1.35, 1.0, 1.0)))
    K.add(cast, bm_cyl(0.18, 0.025, 28, z0=zc + Rs + 0.03), T(mx, 0, 0) @ Matrix.Diagonal((1.0, 1.35, 1.0, 1.0)))
    for yy in (-0.12, 0.12):
        K.box(P['steel_dark'], (mx - 0.25, yy - 0.02, zc + Rs + 0.055), (mx + 0.25, yy + 0.02, zc + Rs + 0.085), bev=0.004)
        bolt(K, P['steel_dark'], (mx, yy, zc + Rs + 0.085), (0, 0, 1), d=0.03, stub=0.03)
    # Speiseleitung an der Seite mit Rueckschlagventil, Abschlammventil hinten unten
    fy = -(Rs + 0.06)
    sp = bent_path([(1.48, fy, 0.0), (1.48, fy, 1.30), (-0.2, fy, 1.30), (-0.2, -Rs + 0.02, 1.30)], 0.08, 4)
    K.tube(P['steel'], sp, 0.03, 10, caps=False)
    K.add(cast, bm_lathe([(0.0, -0.09), (0.045, -0.07), (0.06, 0.0), (0.045, 0.07), (0.0, 0.09)], 12), frame((0.6, fy, 1.30), X))
    K.add(cast, bm_cyl(0.035, 0.07, 6, z0=0.0), T(0.6, fy, 1.30))
    for xx in (1.0, -0.05):
        K.add(P['galv_dark'], bm_box((-0.012, -0.004, -0.05), (0.012, 0.07, 0.035)), T(xx, fy, 1.30))
    bp = bent_path([(-1.28, 0, zc - Rs + 0.06), (-1.28, 0, 0.30), (-1.28, -0.55, 0.30), (-1.28, -0.55, 0.0)], 0.06, 3)
    K.tube(P['steel_dark'], bp, 0.025, 8, caps=False)
    K.add(cast, bm_lathe([(0.0, -0.06), (0.04, -0.05), (0.05, 0.0), (0.04, 0.05), (0.0, 0.06)], 12), frame((-1.28, -0.3, 0.30), (0, 1, 0)))
    K.cyl(P['steel_bright'], (-1.28, -0.3, 0.35), (-1.28, -0.3, 0.45), 0.008, 6)
    K.box(mat('hebel_rot', (0.45, 0.08, 0.05), 'paint', r=1.1), (-1.46, -0.315, 0.44), (-1.28, -0.285, 0.46))
    # Saettel: gemauert, oben ausgerundet, Deckstein
    for sxp in (-0.95, 0.95):
        poly = [(-0.70, 0.0), (0.70, 0.0), (0.70, 0.62)]
        for k in range(13):
            y = 0.62 - 1.24 * k / 12
            poly.append((y, zc - math.sqrt(max(0.0, (Rs + 0.01) ** 2 - y * y))))
        poly.append((-0.70, 0.62))
        b = bm_prism(poly, -0.24, 0.24)
        K.add(brick, b, Matrix(((0, 0, 1, sxp), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
        K.box(capst, (sxp - 0.27, -0.73, 0.0), (sxp + 0.27, 0.73, 0.06), bev=0.01)
    # Gebrauchsspuren: Rost an Nietreihen und unten, helle Flecken, Laufspuren
    for j in range(26):
        xr = rng.choice((-0.475, 0.475, -1.40, 1.40, rng.uniform(-1.3, 1.3)))
        a = rng.uniform(-math.pi * 0.9, math.pi * 0.1)
        cdecal(K, P['rust'] if j % 3 else P['rust_dark'], (0, 0, zc), X, (0, 1, 0), Rs + (0.012 if abs(xr) < 0.5 else 0.0) + 0.002, a, xr,
               rng.uniform(0.05, 0.16), rng.uniform(0.03, 0.12), 3000 + j, n=10, lift=0.0)
    for j in range(10):
        a = rng.uniform(-0.6, 0.6) + (math.pi if j % 2 else 0)
        x = rng.uniform(-1.3, 1.3)
        cdecal(K, lack2, (0, 0, zc), X, (0, 1, 0), Rs + (0.012 if abs(x) < 0.47 else 0.0) + 0.0015, a, x, rng.uniform(0.15, 0.35),
               rng.uniform(0.1, 0.3), 3100 + j, n=10, lift=0.0)
    return K.finish()


# =====================================================================================================================
# Rohre mit schwarzer Fluessigkeit (pipe_black): zwei lackierte Stahlrohre DN150/DN100 an der Wand (Wandflaeche x = 0)
# auf Wandkonsolen mit Buegelschellen, Flanschverbindungen mit acht Schrauben, Absperrschieber mit Handrad, an beiden
# Enden 90-Grad-Boegen in die Wand (Mauerkragen). An einem Flansch leckt es: glaenzende Laeufe, Tropfen, Lache mit
# Kruste; an einem Mauerdurchgang ist Schwarzes an der Wand heruntergelaufen. Fliessrichtungspfeil.
# =====================================================================================================================
def build_pipe_black():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(606)
    lack = mat('rohrlack_schwarz', (0.045, 0.042, 0.040), 'paint', r=0.75)
    lack_alt = mat('rohrlack_matt', (0.09, 0.08, 0.07), 'paint', r=1.3)
    flg = mat('flansch_lack', (0.06, 0.055, 0.05), 'paint', r=1.0)
    mortar = mat('moertel', (0.50, 0.47, 0.42), 'concrete', tex='concrete_rough', r=1.4)
    X, Y = Vector((1, 0, 0)), Vector((0, 1, 0))
    pipes = [(0.084, 0.185, 2.10, 2.78, 0.26), (0.057, 0.150, 1.72, 2.56, 0.18)]
    for (r, px, pz, L, br) in pipes:
        path = bent_path([(-0.06, -L, pz), (px, -L, pz), (px, L, pz), (-0.06, L, pz)], br, 6)
        K.tube(lack, path, r, 20, caps=False)
        for s in (-1, 1):  # Mauerkragen
            K.add(mortar, bm_lathe([(r + 0.045, 0.0), (r + 0.03, 0.012), (r + 0.004, 0.016)], 20, cap0=False, cap1=False), frame((0.0, s * L, pz), X))
            K.add(P['galv_dark'], bm_lathe([(r + 0.004, 0.0), (r + 0.004, 0.026)], 20, cap0=False, cap1=False), frame((0.0, s * L, pz), X))
    # Flansche
    flange_pair(K, flg, P['steel_dark'], P['black_rubber'], (0.185, -1.20, 2.10), Y, 0.084, rng, nb=8)
    flange_pair(K, P['rust'], P['rust_dark'], P['black_rubber'], (0.185, 1.30, 2.10), Y, 0.084, rng, nb=8)
    flange_pair(K, flg, P['steel_dark'], P['black_rubber'], (0.150, -0.55, 1.72), Y, 0.057, rng, nb=8)
    flange_pair(K, P['rust_dark'], P['rust_dark'], P['black_rubber'], (0.150, 2.00, 1.72), Y, 0.057, rng, nb=8)
    # Absperrschieber auf dem unteren Rohr (Spindel zum Gang, Handrad)
    vy = 0.62
    for s in (-1, 1):
        flange_pair(K, flg, P['steel_dark'], P['black_rubber'], (0.150, vy + s * 0.11, 1.72), Y, 0.057, rng, nb=8, rf=0.115)
    K.add(flg, bm_lathe([(0.0, -0.09), (0.07, -0.085), (0.095, -0.03), (0.095, 0.03), (0.07, 0.085), (0.0, 0.09)], 20), frame((0.15, vy, 1.72), Y))
    K.add(flg, bm_box((-0.05, -0.075, -0.06), (0.15, 0.075, 0.06), 0.02, 2), T(0.15, vy, 1.72))
    K.add(flg, bm_lathe([(0.0, 0.0), (0.07, 0.0), (0.07, 0.03), (0.04, 0.05), (0.035, 0.16), (0.03, 0.20), (0.0, 0.205)], 16),
          frame((0.30, vy, 1.72), X))
    K.cyl(P['steel_bright'], (0.50, vy, 1.72), (0.56, vy, 1.72), 0.012, 8)
    handwheel(K, mat('handrad_rot', (0.42, 0.07, 0.05), 'paint', r=1.1), P['steel_dark'], (0.53, vy, 1.72), X, 0.13, 4, rng)
    # Wandkonsolen: senkrechte Profilschiene, Ausleger, Buegelschellen
    for cy in (-2.05, 0.05, 1.95):
        K.box(P['galv'], (0.0, cy - 0.021, 1.52), (0.041, cy + 0.021, 2.30), bev=0.002)
        K.add(P['hole'], bm_poly([(0.0412, cy - 0.011, 1.53), (0.0412, cy + 0.011, 1.53), (0.0412, cy + 0.011, 2.29), (0.0412, cy - 0.011, 2.29)]))
        for zz in (1.58, 2.24):
            bolt(K, P['steel'], (0.041, cy, zz), X, d=0.012, stub=0.01)
        for (r, px, pz, L, br) in pipes:
            zt = pz - r
            K.box(P['galv'], (0.041, cy - 0.021, zt - 0.041), (px + r + 0.05, cy + 0.021, zt), bev=0.002)
            K.add(P['hole'], bm_poly([(0.05, cy - 0.011, zt - 0.0412), (0.05, cy + 0.011, zt - 0.0412), (px + r + 0.04, cy + 0.011, zt - 0.0412),
                                      (px + r + 0.04, cy - 0.011, zt - 0.0412)]))
            ub = [Vector((px - r - 0.006, cy, zt - 0.05)), Vector((px - r - 0.006, cy, pz))]
            ub += [Vector((px - (r + 0.006) * math.cos(a), cy, pz + (r + 0.006) * math.sin(a))) for a in [math.pi * k / 10 for k in range(1, 10)]]
            ub += [Vector((px + r + 0.006, cy, pz)), Vector((px + r + 0.006, cy, zt - 0.05))]
            K.tube(P['steel'], ub, 0.006, 6)
            for xx in (px - r - 0.006, px + r + 0.006):
                bolt(K, P['steel'], (xx, cy, zt - 0.041), (0, 0, -1), d=0.012)
            cdecal(K, P['rust'], (px, 0, pz), Y, X, r + 0.0005, rng.uniform(-0.5, 0.5) + math.pi / 2, cy, 0.6, 0.03, 4000 + int(cy * 10 + pz * 3))
    # Rost an Flanschen und Rohrunterseiten, Abplatzer, Farblaeufer
    for (r, px, pz, L, br) in pipes:
        for j in range(9):
            y = rng.uniform(-L + 0.3, L - 0.3)
            a = rng.uniform(-0.9, 0.9) + math.pi / 2
            cdecal(K, P['rust'] if j % 2 else P['rust_dark'], (px, 0, pz), Y, X, r, a, y, rng.uniform(0.2, 0.5), rng.uniform(0.04, 0.15),
                   4100 + j + int(pz * 10), n=10, lift=0.0012)
        for j in range(6):
            y = rng.uniform(-L + 0.3, L - 0.3)
            cdecal(K, lack_alt, (px, 0, pz), Y, X, r, rng.uniform(-1.5, 1.5), y, rng.uniform(0.3, 0.8), rng.uniform(0.1, 0.3),
                   4200 + j + int(pz * 10), n=10, lift=0.0009)
    # Leck am rechten oberen Flansch: nasse Laeufe, Tropfen, Lache
    ly = 1.30
    for dy in (-0.035, 0.03):
        cpoly(K, P['sludge'], (0.185, 0, 2.10), Y, X, 0.0845, [(math.pi / 2 + 0.25, ly + dy - 0.02), (math.pi / 2 - 0.25, ly + dy - 0.02),
                                                                (math.pi / 2 - 0.15, ly + dy + 0.10 * (1 if dy > 0 else -1)), (math.pi / 2 + 0.1, ly + dy + 0.12 * (1 if dy > 0 else -1))],
              lift=0.0015)
    K.add(P['sludge'], bm_lathe([(0.0, -0.016), (0.006, -0.012), (0.0075, -0.004), (0.004, 0.003), (0.0, 0.004)], 10), T(0.185, ly + 0.012, 2.10 - 0.142))
    K.add(P['sludge'], bm_lathe([(0.0, -0.009), (0.004, -0.006), (0.005, -0.002), (0.0, 0.0)], 8), T(0.185, ly - 0.02, 1.20))
    sludge(K, P, (0.24, ly + 0.03), 0.30, 61, clip_x=0.006)
    # Durchgang links unten: Schwarzes an der Wand heruntergelaufen, Lache an der Wand
    wy = -2.56
    for j in range(4):
        streak(K, P['sludge'], (0.0012, wy + rng.uniform(-0.05, 0.05), 1.66 - rng.uniform(0, 0.05)), (0, 1, 0), (0, 0, -1),
               rng.uniform(0.02, 0.05), 1.62, 4300 + j, n=10)
    streak(K, P['crust'], (0.0008, wy, 1.70), (0, 1, 0), (0, 0, -1), 0.14, 1.66, 4310, n=10)
    sludge(K, P, (0.10, wy + 0.02), 0.20, 62, clip_x=0.006)
    # Fliessrichtung: gelbe Binde mit schwarzem Pfeil auf dem oberen Rohr
    yel = mat('binde_gelb', (0.80, 0.62, 0.10), 'paint', r=0.9)
    by = -0.35
    cpoly(K, yel, (0.185, 0, 2.10), Y, X, 0.084, [(-1.2, by - 0.05), (1.2, by - 0.05), (1.2, by + 0.05), (-1.2, by + 0.05)], lift=0.0012, cuts=3)
    arrow = [(-0.25, by + 0.10), (0.25, by + 0.10), (0.25, by + 0.20), (0.5, by + 0.20), (0.0, by + 0.32), (-0.5, by + 0.20), (-0.25, by + 0.20)]
    cpoly(K, mat('pfeil_weiss', (0.82, 0.80, 0.74), 'paint', r=1.0), (0.185, 0, 2.10), Y, X, 0.084, [(a, t) for a, t in arrow], lift=0.0012, cuts=2)
    return K.finish()


# =====================================================================================================================
# Schlammlache (sludge_pool): zwei Lachen aus schwarzem Schlamm (glaenzend, Haut, Blasen), Kruste am Rand, Spritzer
# =====================================================================================================================
def build_sludge_pool():
    P = _pal()
    K = Kit(deg=60.0)
    sludge(K, P, (0.05, 0.0), 0.72, 21, bubbles=6, h=0.008)
    sludge(K, P, (0.78, -0.48), 0.26, 22, bubbles=2, h=0.005)
    sludge(K, P, (-0.45, 0.62), 0.14, 23, bubbles=1, h=0.004)
    return K.finish()


# =====================================================================================================================
# Fenster mit Ziegel dahinter (window_brick): Stahlsprossenfenster 1,6 x 1,2 m (4 x 3 Felder, T-Sprossen) vor einer
# von aussen zugemauerten Oeffnung - roter Ziegel im Verband, Moertelfugen, Ausbluehungen. Scheiben teils heil
# (verschmutzt, mit Kitt), teils gesprungen, zerbrochen mit Scherben oder fehlend. Betonsims, Rost am Rahmen.
# Wandflaeche x = 0, Mitte im Ursprung.
# =====================================================================================================================
def build_window_brick():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(1313)
    frame_m = mat('fensterlack', (0.17, 0.20, 0.19), 'paint', r=1.1)
    putty = mat('kitt', (0.55, 0.53, 0.48), 'concrete', r=1.4)
    mortar = mat('fuge', (0.46, 0.43, 0.38), 'concrete', r=1.5)
    bricks = [mat('ziegel_rot', (0.52, 0.21, 0.13), 'concrete', r=1.3), mat('ziegel_dunkel', (0.37, 0.15, 0.10), 'concrete', r=1.3),
              mat('ziegel_braun', (0.42, 0.22, 0.15), 'concrete', r=1.35), mat('ziegel_hell', (0.60, 0.30, 0.20), 'concrete', r=1.3)]
    sill = mat('sims', (0.55, 0.53, 0.49), 'concrete', tex='concrete_rough', r=1.3)
    W, H = 1.6, 1.2
    # Ziegelwand hinter dem Fenster (Laeufer, halbversetzt), Fugen 1 cm vertieft
    K.box(mortar, (-0.02, -W / 2 - 0.02, -H / 2 - 0.02), (-0.012, W / 2 + 0.02, H / 2 + 0.02))
    z = -H / 2 - 0.02
    row = 0
    while z < H / 2 + 0.02:
        y = -W / 2 - 0.02 - (0.125 if row % 2 else 0.0)
        while y < W / 2 + 0.02:
            y0, y1 = max(y + 0.005, -W / 2 - 0.02), min(y + 0.245, W / 2 + 0.02)
            z0, z1 = z + 0.005, min(z + 0.076, H / 2 + 0.02)
            if y1 - y0 > 0.02 and z1 - z0 > 0.01:
                d = rng.uniform(-0.004, 0.003)
                b = bm_box((-0.012, y0, z0), (-0.001 + d, y1, z1))
                jitter(b, 0.0012, 40.0, rng.randrange(1000))
                K.add(bricks[rng.randrange(4)] if rng.random() < 0.9 else bricks[1], b)
            y += 0.25
        z += 0.081
        row += 1
    for j in range(5):  # Ausbluehungen, Russ
        decal(K, P['zinc_white'] if j < 3 else P['grime'], (0.0002, rng.uniform(-0.7, 0.7), rng.uniform(-0.5, 0.5)), (0, 1, 0), (0, 0, 1),
              rng.uniform(0.05, 0.15), rng.uniform(0.04, 0.12), 5000 + j, n=10, lift=0.0)
    # Rahmen: Winkelprofil aussen, T-Sprossen (3 senkrecht, 2 waagerecht)
    fw, fd = 0.05, 0.06
    for (lo, hi) in (((0.0, -W / 2 - fw, -H / 2 - fw), (fd, W / 2 + fw, -H / 2)), ((0.0, -W / 2 - fw, H / 2), (fd, W / 2 + fw, H / 2 + fw)),
                     ((0.0, -W / 2 - fw, -H / 2), (fd, -W / 2, H / 2)), ((0.0, W / 2, -H / 2), (fd, W / 2 + fw, H / 2))):
        K.box(frame_m, lo, hi, bev=0.003)
    cols, rows = 4, 3
    tw = 0.03
    pw = (W - (cols - 1) * tw) / cols
    ph = (H - (rows - 1) * tw) / rows
    for i in range(1, cols):
        y = -W / 2 + i * pw + (i - 0.5) * tw
        K.box(frame_m, (fd - 0.006, y - tw / 2, -H / 2), (fd, y + tw / 2, H / 2), bev=0.0015)
        K.box(frame_m, (0.0, y - 0.003, -H / 2), (fd - 0.006, y + 0.003, H / 2))
    for j in range(1, rows):
        z = -H / 2 + j * ph + (j - 0.5) * tw
        K.box(frame_m, (fd - 0.006, -W / 2, z - tw / 2), (fd, W / 2, z + tw / 2), bev=0.0015)
        K.box(frame_m, (0.0, -W / 2, z - 0.003), (fd - 0.006, W / 2, z + 0.003))
    # Scheiben
    states = ['heil', 'heil', 'sprung', 'fehlt', 'scherben', 'heil', 'sprung', 'scherben', 'heil', 'fehlt', 'sprung', 'scherben']
    rng.shuffle(states)
    gx = fd - 0.012
    for i in range(cols):
        for j in range(rows):
            y0 = -W / 2 + i * (pw + tw)
            z0 = -H / 2 + j * (ph + tw)
            y1, z1 = y0 + pw, z0 + ph
            st = states[i * rows + j]
            if st in ('heil', 'sprung'):
                for (a0, a1, b0, b1) in ((y0, y1, z0, z0 + 0.009), (y0, y1, z1 - 0.009, z1), (y0, y0 + 0.009, z0, z1), (y1 - 0.009, y1, z0, z1)):
                    K.box(putty, (gx, a0, b0), (gx + 0.008, a1, b1))
            if st == 'heil':
                K.add(P['glass'] if rng.random() < 0.6 else P['glass_dirty'], bm_poly([(gx, y0, z0), (gx, y1, z0), (gx, y1, z1), (gx, y0, z1)]))
            elif st == 'sprung':  # Sprung: Scheibe in Stuecke mit feinen Luecken
                cy, cz = rng.uniform(y0 + 0.08, y1 - 0.08), rng.uniform(z0 + 0.08, z1 - 0.08)
                corners = [(y0, z0), (y1, z0), (y1, z1), (y0, z1)]
                for k in range(4):
                    a, b2 = corners[k], corners[(k + 1) % 4]
                    sh = 0.0025
                    pa = (a[0] + (cy - a[0]) * 0.0 + sh * (1 if cy > a[0] else -1), a[1] + sh * (1 if cz > a[1] else -1))
                    pb = (b2[0] + sh * (1 if cy > b2[0] else -1), b2[1] + sh * (1 if cz > b2[1] else -1))
                    K.add(P['glass'], bm_poly([(gx, pa[0], pa[1]), (gx, pb[0], pb[1]), (gx, cy + rng.uniform(-0.004, 0.004), cz + rng.uniform(-0.004, 0.004))]))
            elif st == 'scherben':  # Scherben in Ecken und am Rand
                for k in range(rng.randrange(1, 4)):
                    corner = rng.randrange(4)
                    cyc = [y0, y1][corner % 2]
                    czc = [z0, z1][corner // 2]
                    sy = 1 if corner % 2 == 0 else -1
                    sz = 1 if corner // 2 == 0 else -1
                    p1 = (cyc + sy * rng.uniform(0.05, 0.2), czc)
                    p2 = (cyc, czc + sz * rng.uniform(0.05, 0.22))
                    p3 = (cyc + sy * rng.uniform(0.01, 0.06), czc + sz * rng.uniform(0.01, 0.06))
                    pts = [(gx, cyc, czc), (gx, p1[0], p1[1]), (gx, p3[0], p3[1]), (gx, p2[0], p2[1])]
                    bb = bm_poly(pts)
                    bb.normal_update()
                    for f in bb.faces:
                        if f.normal.x < 0:
                            f.normal_flip()
                    K.add(P['glass'], bb)
            # Rost und Schmutz in der unteren Ecke des Felds
            if rng.random() < 0.6:
                decal(K, P['rust'], (fd + 0.0004, y0 + rng.uniform(0.0, pw), z0 - tw / 2), (0, 1, 0), (0, 0, 1), rng.uniform(0.02, 0.06),
                      0.012, 5200 + i * 7 + j, n=8, lift=0.0)
    # Griff am mittleren Pfosten
    K.box(P['steel_dark'], (fd, -0.03, -0.05), (fd + 0.015, -0.0, 0.03), bev=0.003)
    K.add(P['steel_dark'], bm_box((0.0, -0.008, -0.09), (0.014, 0.008, 0.0), 0.004), T(fd + 0.015, -0.015, -0.01) @ R(0.3, 'X'))
    # Rost und Abplatzer am Rahmen
    for j in range(16):
        side = rng.randrange(4)
        if side == 0:
            p, u, v = (fd + 0.0005, rng.uniform(-W / 2, W / 2), -H / 2 - rng.uniform(0.0, fw)), (0, 1, 0), (0, 0, 1)
        elif side == 1:
            p, u, v = (fd + 0.0005, rng.uniform(-W / 2, W / 2), H / 2 + rng.uniform(0.0, fw)), (0, 1, 0), (0, 0, 1)
        elif side == 2:
            p, u, v = (fd + 0.0005, -W / 2 - rng.uniform(0, fw), rng.uniform(-H / 2, H / 2)), (0, 1, 0), (0, 0, 1)
        else:
            p, u, v = (fd + 0.0005, W / 2 + rng.uniform(0, fw), rng.uniform(-H / 2, H / 2)), (0, 1, 0), (0, 0, 1)
        decal(K, P['rust'] if j % 3 else P['steel_dark'], p, u, v, rng.uniform(0.01, 0.04), rng.uniform(0.008, 0.02), 5300 + j, n=8, lift=0.0)
    # Sims (Beton, leicht geneigt), Rostlaeufer darunter
    sb = bm_prism([(0.0, 0.0), (0.14, -0.015), (0.14, -0.045), (0.0, -0.05)], -W / 2 - 0.1, W / 2 + 0.1)
    for v in sb.verts:
        v.co.x, v.co.y, v.co.z = v.co.x, v.co.z, v.co.y
    for v in sb.verts:
        v.co.z += -H / 2 - fw
    bmesh.ops.recalc_face_normals(sb, faces=sb.faces)
    K.add(sill, sb)
    decal(K, P['dust'], (0.07, 0.0, -H / 2 - fw - 0.0075), (1, 0, -0.107), (0, 1, 0), 0.05, 0.6, 5400, n=12, lift=0.0008)
    for j in range(5):
        decal(K, P['rust'], (0.02, rng.uniform(-0.75, 0.75), -H / 2 - fw - 0.002), (1, 0, -0.107), (0, 1, 0), 0.02, rng.uniform(0.01, 0.03),
              5500 + j, n=8, lift=0.0012)
    return K.finish()


# =====================================================================================================================
# Aufzugstuer (elevator_door): Schachttuer im Durchbruch (Laibung 1 m, Oeffnung 1,4 m, lichte Hoehe 2,2 m), Ursprung in
# der Mitte der Oeffnung auf dem Boden, Vorraum bei -x. Edelstahl-Zarge mit Profil, ausgekleidete Laibung, Schwellen
# aus Aluminium mit Fuehrungsnut, Teleskop-Tuerblaetter offen in den Taschen (Kanten mit Gummi sichtbar), Zeigerwerk
# ueber der Tuer (Halbkreis mit Etagenziffern, Zeiger), Richtungspfeile, Ruftableau mit Leuchtringen, Zettel.
# =====================================================================================================================
def build_elevator_door():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(2525)
    inox = mat('edelstahl', (0.82, 0.82, 0.83), 'metal', tex='metal_aluminum', r=1.0, m=0.45)
    inox_dark = mat('edelstahl_matt', (0.60, 0.60, 0.61), 'metal', tex='metal_aluminum', r=1.2, m=0.4)
    alu = mat('alu_schwelle', (0.62, 0.62, 0.60), 'metal', r=0.9)
    lamp = mat('anzeige_bernstein', (1.0, 0.55, 0.20), 'glow', glow=2.0)
    lamp_off = mat('anzeige_aus', (0.30, 0.20, 0.12), 'plastic', r=0.4)
    W, H = 1.4, 2.2
    hw = W / 2
    # Laibung ausgekleidet
    for s in (-1, 1):
        y0, y1 = (hw, hw + 0.002) if s > 0 else (-hw - 0.002, -hw)
        K.box(inox, (-0.5, y0, 0.0), (0.5, y1, H))
    K.box(inox, (-0.5, -hw, H), (0.5, hw, H + 0.002))
    # Schwellen
    for (x0, x1) in ((-0.56, -0.40), (0.40, 0.50)):
        K.box(alu, (x0, -hw - 0.05, 0.0), (x1, hw + 0.05, 0.012), bev=0.002)
        for gxp in (x0 + 0.045, x0 + 0.095):
            K.add(P['hole'], bm_poly([(gxp - 0.006, -hw - 0.04, 0.0121), (gxp + 0.006, -hw - 0.04, 0.0121), (gxp + 0.006, hw + 0.04, 0.0121),
                                      (gxp - 0.006, hw + 0.04, 0.0121)]))
        for k in range(14):
            yy = -hw + 0.05 + k * 0.1
            K.add(P['grime'], bm_poly([(x0 + 0.01, yy, 0.0122), (x0 + 0.03, yy, 0.0122), (x0 + 0.03, yy + 0.004, 0.0122), (x0 + 0.01, yy + 0.004, 0.0122)]))
    # Tuerblaetter (je Seite zwei, Teleskop) offen in den Taschen, Kante mit Gummi sichtbar
    for xs in (-0.44, 0.40):
        for s in (-1, 1):
            for k, (yy0, yy1, dx) in enumerate(((hw - 0.02, hw + 0.34, 0.0), (hw + 0.0, hw + 0.36, 0.035))):
                y0, y1 = (yy0, yy1) if s > 0 else (-yy1, -yy0)
                K.box(inox_dark, (xs + dx, y0, 0.012), (xs + dx + 0.03, y1, H - 0.005), bev=0.003)
            ye = s * (hw - 0.02)
            K.box(P['black_rubber'], (xs + 0.004, ye - 0.006, 0.02), (xs + 0.026, ye + 0.006, H - 0.02), bev=0.002)
    # Zarge auf der Vorraumseite (-x): Profilleisten seitlich und oben
    fx = -0.5
    prof = [(0.0, 0.0), (0.0, 0.10), (0.012, 0.10), (0.016, 0.085), (0.026, 0.075), (0.026, 0.03), (0.018, 0.012), (0.018, 0.0)]

    def frame_bar(a, b, outward):
        """Profil (Tiefe nach -x, Breite nach outward) entlang a -> b."""
        a, b, o = Vector(a), Vector(b), Vector(outward)
        ax = (b - a).normalized()
        bb = bmesh.new()
        r0 = [bb.verts.new(a + Vector((-d, 0, 0)) + o * w) for (d, w) in prof]
        r1 = [bb.verts.new(b + Vector((-d, 0, 0)) + o * w) for (d, w) in prof]
        n = len(prof)
        for i in range(n):
            j = (i + 1) % n
            bb.faces.new((r0[i], r0[j], r1[j], r1[i]))
        bb.faces.new(list(reversed(r0)))
        bb.faces.new(r1)
        bmesh.ops.recalc_face_normals(bb, faces=bb.faces)
        K.add(inox, bb)
    for s in (-1, 1):
        frame_bar((fx, s * hw, 0.0), (fx, s * hw, H + 0.1), (0, s, 0))
    frame_bar((fx, -hw - 0.1, H), (fx, hw + 0.1, H), (0, 0, 1))
    # Zeigerwerk ueber der Tuer: Halbkreis mit Ziffern, Zeiger, Richtungspfeile
    cz = H + 0.13
    K.box(inox_dark, (fx - 0.03, -0.26, H + 0.105), (fx, 0.26, H + 0.34), bev=0.004, seg=2)
    K.box(P['black_plastic'], (fx - 0.033, -0.22, H + 0.12), (fx - 0.03, 0.22, H + 0.33))
    dial = mat('ziffern', (0.95, 0.88, 0.70), 'sign')
    for k, d in enumerate('12345'):
        a = math.radians(20 + 140 * k / 4)
        c = Vector((fx - 0.0335, 0.15 * math.cos(a), cz + 0.15 * math.sin(a)))
        text(K, dial, d, c + Vector((0, 0.012, -0.016)), (0, -1, 0), (0, 0, 1), 0.032, w=0.024)
    a = math.radians(20 + 140 * 2.3 / 4)
    brass = mat('messing', (0.62, 0.46, 0.20), 'metal', r=0.7)
    tip = Vector((fx - 0.036, 0.12 * math.cos(a), cz + 0.12 * math.sin(a)))
    nrm = Vector((0, -math.sin(a), math.cos(a))) * 0.005
    K.add(brass, bm_poly([Vector((fx - 0.036, 0, cz)) - nrm, tip, Vector((fx - 0.036, 0, cz)) + nrm]))
    K.cyl(brass, (fx - 0.033, 0, cz), (fx - 0.042, 0, cz), 0.014, 12)
    for s, m in ((-1, lamp), (1, lamp_off)):  # Pfeile: abwaerts leuchtet
        yc = s * 0.205
        pts = [(fx - 0.034, yc - 0.022, cz + 0.02), (fx - 0.034, yc + 0.022, cz + 0.02), (fx - 0.034, yc, cz - 0.02)] if s < 0 else \
              [(fx - 0.034, yc, cz + 0.02), (fx - 0.034, yc + 0.022, cz - 0.02), (fx - 0.034, yc - 0.022, cz - 0.02)]
        bb = bm_poly(pts)
        bb.normal_update()
        for f in bb.faces:
            if f.normal.x > 0:
                f.normal_flip()
        K.add(m, bb)
    # Ruftableau links neben der Tuer (Vorraumseite)
    ty = -hw - 0.24
    K.box(inox, (fx - 0.008, ty - 0.055, 0.98), (fx, ty + 0.055, 1.30), bev=0.003, seg=2)
    for k, zz in enumerate((1.18, 1.08)):
        K.add(lamp if k == 1 else lamp_off, bm_lathe([(0.0, 0.0), (0.024, 0.0), (0.024, 0.004), (0.0, 0.004)], 20), frame((fx - 0.008, ty, zz), (-1, 0, 0)))
        K.add(P['black_plastic'], bm_lathe([(0.0, 0.0), (0.019, 0.0), (0.019, 0.009), (0.012, 0.012), (0.0, 0.013)], 16), frame((fx - 0.010, ty, zz), (-1, 0, 0)))
    K.cyl(P['steel_bright'], (fx - 0.008, ty, 1.255), (fx - 0.012, ty, 1.255), 0.009, 10)
    K.box(P['hole'], (fx - 0.0125, ty - 0.001, 1.25), (fx - 0.012, ty + 0.001, 1.26))
    for xx, zz in ((ty - 0.04, 1.0), (ty + 0.04, 1.0), (ty - 0.04, 1.28), (ty + 0.04, 1.28)):
        slot_screw(K, P['steel'], P['hole'], (fx - 0.008, xx, zz), (-1, 0, 0), d=0.007, rot=rng.random())
    decal(K, P['grime'], (fx - 0.0085, ty, 1.13), (0, -1, 0), (0, 0, 1), 0.04, 0.07, 6001, n=10, lift=0.0)
    decal(K, P['grime'], (fx - 0.0004, ty + 0.1, 1.15), (0, -1, 0), (0, 0, 1), 0.05, 0.12, 6002, n=10, lift=0.0)
    # Zettel "ausser Betrieb" (Atlas-Zettel) mit Klebeband an der Zarge
    paper = mat('zettel', (0.95, 0.94, 0.90), 'sign')
    sign_quad(K, paper, 57, (fx - 0.0275, -hw - 0.052, 1.52), (0, -1, 0), (0, 0, 1), 0.075, 0.10)
    tape = mat('klebeband', (0.80, 0.76, 0.62), 'plastic', r=1.1)
    for zz in (1.565, 1.475):
        K.add(tape, bm_poly([(fx - 0.0278, -hw - 0.035, zz - 0.007), (fx - 0.0278, -hw - 0.035, zz + 0.007), (fx - 0.0278, -hw - 0.07, zz + 0.009),
                             (fx - 0.0278, -hw - 0.07, zz - 0.006)]))
    # Gebrauchsspuren: Kratzer in Wagenhoehe, Schmutz unten, Dellen als dunkle Flecken
    for s in (-1, 1):
        for j in range(5):
            x = rng.uniform(-0.4, 0.4)
            z = rng.uniform(0.15, 1.1)
            a = rng.uniform(-0.3, 0.3)
            yy = s * (hw - 0.0005)
            streak(K, P['steel_bright'], (x, yy, z), (math.cos(a), 0, math.sin(a)) if s > 0 else (-math.cos(a), 0, -math.sin(a)),
                   (math.sin(a), 0, -math.cos(a)) if s > 0 else (math.sin(a), 0, -math.cos(a)), 0.002, rng.uniform(0.1, 0.4), 6100 + j * 2 + s, n=3)
        decal(K, P['grime'], (rng.uniform(-0.3, 0.3), s * (hw - 0.0006), 0.06), (s, 0, 0), (0, 0, 1),
              rng.uniform(0.15, 0.3), 0.05, 6200 + s, n=10, lift=0.0)
    for s in (-1, 1):
        decal(K, P['grime'], (fx - 0.0265, s * (hw + 0.06), 0.08), (0, -1, 0), (0, 0, 1), 0.03, 0.08, 6300 + s, n=8, lift=0.0)
    return K.finish()


# =====================================================================================================================
# Kartons (Foerderband): Laschen, Falzlinien, Klebeband, Etikett, Dellen, Kantenschaeden; offen oder zugeklebt
# =====================================================================================================================
def _carton(K, P, c, size, rot, seed, state='zu'):
    rng = random.Random(seed)
    card = mat('karton', (0.56, 0.43, 0.29), 'cardboard', r=1.4)
    card_d = mat('karton_dunkel', (0.42, 0.31, 0.20), 'cardboard', r=1.5)
    tape = mat('packband', (0.60, 0.45, 0.24), 'plastic', r=0.6)
    label = mat('etikett', (0.88, 0.87, 0.82), 'paint', r=1.2)
    ink = mat('druck', (0.05, 0.05, 0.05), 'paint', r=1.2)
    lx, ly, lz = size
    M = T(*c) @ R(rot, 'Z')
    b = bm_box((-lx / 2, -ly / 2, 0.0), (lx / 2, ly / 2, lz), 0.004, 1)
    # Dellen: Ecken und eine Seite eingedrueckt
    for v in b.verts:
        k = noise.noise(Vector((v.co.x * 9 + seed, v.co.y * 9, v.co.z * 9)))
        if v.co.z > lz * 0.9 and abs(v.co.x) > lx * 0.45 and abs(v.co.y) > ly * 0.45:
            v.co.z -= 0.006 + 0.006 * k
        v.co += Vector((0.002 * k, 0.002 * k, 0.0))
    if state == 'offen':  # Laschen schraeg nach aussen
        b.free()
        b = bm_box((-lx / 2, -ly / 2, 0.0), (lx / 2, ly / 2, lz), 0.003, 1)
        b.normal_update()
        bmesh.ops.delete(b, geom=[f for f in b.faces if f.normal.z > 0.9], context='FACES')
        K.add(card, b, M)
        K.add(card_d, bm_box((-lx / 2 + 0.004, -ly / 2 + 0.004, 0.004), (lx / 2 - 0.004, ly / 2 - 0.004, 0.006)), M)
        for (axis, sgn, w, flap_l) in (('x', 1, ly, ly * 0.5), ('x', -1, ly, ly * 0.5), ('y', 1, lx, ly * 0.5), ('y', -1, lx, ly * 0.5)):
            alpha = math.radians(rng.uniform(25, 75))
            fl = bm_box((0.0, -w / 2 + 0.002, -0.0015), (flap_l * 0.95, w / 2 - 0.002, 0.0015))
            base = T(sgn * lx / 2, 0, lz) if axis == 'x' else T(0, sgn * ly / 2, lz)
            spin = R(0.0 if (axis == 'x' and sgn > 0) else (math.pi if axis == 'x' else (math.pi / 2 if sgn > 0 else -math.pi / 2)), 'Z')
            K.add(card, fl, M @ base @ spin @ R(-alpha, 'Y'))
        return
    K.add(card, b, M)
    # Falz/Stoss der Deckellaschen
    K.add(card_d, bm_poly([(-lx / 2 + 0.01, -0.0015, lz + 0.0006), (lx / 2 - 0.01, -0.0015, lz + 0.0006), (lx / 2 - 0.01, 0.0015, lz + 0.0006),
                           (-lx / 2 + 0.01, 0.0015, lz + 0.0006)]), M)
    # Klebeband ueber die Naht und an den Stirnseiten herunter
    K.add(tape, bm_poly([(-lx / 2 - 0.0008, -0.025, lz + 0.0012), (lx / 2 + 0.0008, -0.025, lz + 0.0012), (lx / 2 + 0.0008, 0.025, lz + 0.0012),
                         (-lx / 2 - 0.0008, 0.025, lz + 0.0012)]), M)
    for s in (-1, 1):
        pts = [(s * (lx / 2 + 0.0012), -0.025, lz), (s * (lx / 2 + 0.0012), 0.025, lz), (s * (lx / 2 + 0.0012), 0.025, lz - 0.06 - rng.uniform(0, 0.02)),
               (s * (lx / 2 + 0.0012), -0.025, lz - 0.065)]
        bb = bm_poly(list(reversed(pts)) if s > 0 else pts)
        K.add(tape, bb, M)
    # Etikett mit Strichcode an einer Seite
    yy = -ly / 2 - 0.0012
    K.add(label, bm_poly([(-0.06, yy, lz * 0.45), (0.05, yy, lz * 0.45), (0.05, yy, lz * 0.45 + 0.07), (-0.06, yy, lz * 0.45 + 0.07)]), M)
    for k in range(14):
        x0 = -0.05 + k * 0.0065
        wdt = 0.0015 + 0.002 * ((k * 7 + seed) % 3) / 2
        K.add(ink, bm_poly([(x0, yy - 0.0004, lz * 0.45 + 0.012), (x0 + wdt, yy - 0.0004, lz * 0.45 + 0.012), (x0 + wdt, yy - 0.0004, lz * 0.45 + 0.04),
                            (x0, yy - 0.0004, lz * 0.45 + 0.04)]), M)
    # Kantenschaden (helle Pappe) und Schmutz
    decal(K, card_d, (lx / 2 + 0.0012, rng.uniform(-ly / 3, ly / 3), 0.02), (0, 1, 0), (0, 0, 1), 0.04, 0.015, seed + 7, n=8, lift=0.0)


# =====================================================================================================================
# Foerderband (conveyor): 6 m Gurtbandfoerderer, Gurtoberkante 0,9 m. C-Profil-Holme, Kopf- und Endtrommel mit
# Stehlagern, Tragrollen alle 0,5 m, Untertrum mit Rueckfuehrrollen, Beine mit Fussplatten und Verbaenden,
# Getriebemotor unter dem Kopf mit Kettenschutz, Seitenfuehrung, Reissleine mit Not-Halt-Schalter, Kartons.
# =====================================================================================================================
def build_conveyor():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(3030)
    paint = mat('foerder_lack', (0.30, 0.37, 0.31), 'paint', r=1.05)
    belt = mat('foerdergurt', (0.06, 0.06, 0.06), 'rubber', r=1.1)
    belt_w = mat('foerdergurt_abrieb', (0.15, 0.145, 0.14), 'rubber', r=1.3)
    yellow = mat('schutz_gelb', (0.82, 0.62, 0.08), 'paint', r=1.0)
    red = mat('notaus_rot', (0.55, 0.06, 0.04), 'plastic', r=0.9)
    Zt = 0.90
    xd = 2.78
    rd = 0.11
    zd = Zt - 0.006 - rd
    # Holme (C-Profil, Oeffnung nach innen)
    for s in (-1, 1):
        prof = [(0.362, 0.748), (0.362, 0.882), (0.31, 0.882), (0.31, 0.876), (0.356, 0.876), (0.356, 0.754), (0.31, 0.754), (0.31, 0.748)]
        prof = [(s * y, z) for (y, z) in prof]
        K.add(paint, bm_prism(prof, -2.86, 2.86), Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
    # Gurt als geschlossene Schleife (Rechteckquerschnitt)
    path = [Vector((x, 0, Zt - 0.003)) for x in (-2.0, 0.0, 2.0)]
    path += [Vector((xd + (rd + 0.003) * math.sin(a), 0, zd + (rd + 0.003) * math.cos(a))) for a in [math.pi * k / 10 for k in range(11)]]
    path += [Vector((x, 0, zd - rd - 0.003 - 0.012 * math.sin(math.pi * (x + 2.0) / 4.0) ** 2)) for x in (2.0, 0.0, -2.0)]
    path += [Vector((-xd - (rd + 0.003) * math.sin(a), 0, zd - (rd + 0.003) * math.cos(a))) for a in [math.pi * k / 10 for k in range(11)]]
    rect = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    K.add(belt, bm_sweep(path, (0.0035, 0.30), 4, caps=False, prof=rect, path_closed=True))
    # Abriebspur in der Mitte, Verbinder (Klammern) quer
    pts = [(x, -0.11 + 0.02 * math.sin(x * 2.3 + 1), Zt + 0.0011) for x in [-2.7 + 5.4 * k / 14 for k in range(15)]]
    pts += [(x, 0.10 + 0.02 * math.sin(x * 3.1), Zt + 0.0011) for x in [2.7 - 5.4 * k / 14 for k in range(15)]]
    K.add(belt_w, bm_poly(pts))
    for k in range(18):
        y = -0.27 + k * 0.032
        K.box(P['steel'], (1.18, y - 0.006, Zt), (1.205, y + 0.006, Zt + 0.003))
    # Trommeln mit Welle und Stehlagern
    for xs in (-xd, xd):
        K.add(P['steel_dark'], bm_cyl(rd, 0.66, 28, caps=True, z0=-0.33), frame((xs, 0, zd), (0, 1, 0)))
        K.add(P['steel'], bm_cyl(0.028, 0.92, 10, z0=-0.46), frame((xs, 0, zd), (0, 1, 0)))
        for s in (-1, 1):
            yb = s * 0.40
            K.box(P['steel_dark'], (xs - 0.10, yb - 0.035, zd - 0.075), (xs + 0.10, yb + 0.035, zd - 0.05), bev=0.004)
            K.add(P['steel_dark'], bm_lathe([(0.0, -0.035), (0.055, -0.035), (0.062, -0.02), (0.062, 0.02), (0.055, 0.035), (0.0, 0.035)], 16),
                  frame((xs, yb, zd), (0, 1, 0)))
            K.box(P['steel_dark'], (xs - 0.045, yb - 0.03, zd - 0.05), (xs + 0.045, yb + 0.03, zd - 0.01))
            K.cyl(P['steel_bright'], (xs, yb, zd + 0.062), (xs, yb, zd + 0.08), 0.005, 6)
            for xx in (xs - 0.08, xs + 0.08):
                bolt(K, P['steel'], (xx, yb, zd - 0.05), (0, 0, 1), d=0.012)
            K.box(paint, (xs - 0.12, yb - 0.04, zd - 0.09), (xs + 0.12, yb + 0.04, zd - 0.075))
            K.box(paint, (xs - 0.02, min(s * 0.36, s * 0.44), zd - 0.09), (xs + 0.02, max(s * 0.36, s * 0.44), 0.882))
    # Tragrollen (oben) und Rueckfuehrrollen (unten)
    for k in range(11):
        x = -2.5 + 0.5 * k
        K.add(P['steel'], bm_cyl(0.044, 0.62, 14, z0=-0.31), frame((x, 0, Zt - 0.0065 - 0.044), (0, 1, 0)))
        for s in (-1, 1):
            K.box(paint, (x - 0.02, s * 0.31 - 0.004, 0.80), (x + 0.02, s * 0.31 + 0.004, 0.87) if s > 0 else (x + 0.02, s * 0.31 + 0.004, 0.87))
    for x in (-1.9, -0.6, 0.7, 2.0):
        zr = zd - rd - 0.0065 - 0.04
        K.add(P['steel'], bm_cyl(0.040, 0.64, 12, z0=-0.32), frame((x, 0, zr), (0, 1, 0)))
        for s in (-1, 1):
            K.box(paint, (x - 0.02, s * 0.335, zr - 0.02) if s > 0 else (x - 0.02, s * 0.345, zr - 0.02),
                  (x + 0.02, s * 0.345, 0.76) if s > 0 else (x + 0.02, s * 0.335, 0.76))
    # Beine, Fussplatten, Verbaende
    legs = (-2.45, 0.0, 2.45)
    for x in legs:
        for s in (-1, 1):
            y = s * 0.33
            K.box(paint, (x - 0.03, y - 0.03, 0.012), (x + 0.03, y + 0.03, 0.748), bev=0.003)
            K.box(P['steel_dark'], (x - 0.075, y - 0.075, 0.0), (x + 0.075, y + 0.075, 0.012), bev=0.002)
            for dx, dy in ((-0.05, -0.05), (0.05, 0.05)):
                bolt(K, P['steel'], (x + dx, y + dy, 0.012), (0, 0, 1), d=0.012, stub=0.012)
            decal(K, P['rust'], (x, y + s * 0.0302, 0.06), (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), 0.05, 0.025, int(x * 10 + s * 3 + 50), n=8,
                  lift=0.0)
        K.box(paint, (x - 0.02, -0.30, 0.22), (x + 0.02, 0.30, 0.26))
    for s in (-1, 1):
        y = s * 0.33
        for xa, xb in ((-2.45, 0.0), (0.0, 2.45)):
            K.box(paint, (xa, y - 0.015, 0.20), (xb, y + 0.015, 0.23))
            K.cyl(paint, (xa + 0.03, y, 0.25), (xb - 0.03, y, 0.72), 0.012, 6)
    # Getriebemotor unter dem Kopf, Kettenschutz aussen
    mx, my, mz = 2.15, 0.12, 0.40
    K.add(paint, bm_lathe([(0.0, -0.19), (0.10, -0.19), (0.115, -0.17), (0.115, 0.14), (0.10, 0.16), (0.0, 0.16)], 20), frame((mx - 0.25, my, mz), (0, 1, 0)))
    for k in range(9):
        y = my - 0.15 + k * 0.034
        K.add(paint, bm_cyl(0.122, 0.008, 20, z0=-0.004), frame((mx - 0.25, y, mz), (0, 1, 0)))
    K.box(paint, (mx - 0.13, my - 0.12, mz - 0.13), (mx + 0.10, my + 0.24, mz + 0.14), bev=0.012, seg=2)
    K.box(P['grey_plastic'], (mx - 0.32, my - 0.05, mz + 0.115), (mx - 0.19, my + 0.06, mz + 0.18), bev=0.006)
    K.tube(P['black_rubber'], catmull([(mx - 0.25, my, mz + 0.18), (mx - 0.25, my - 0.05, mz + 0.3), (mx - 0.35, my - 0.2, 0.35), (mx - 0.45, my - 0.25, 0.05),
                                       (mx - 0.55, my - 0.25, 0.012)], 3), 0.012, 6)
    K.box(paint, (mx - 0.40, -0.25, mz - 0.20), (mx + 0.25, 0.30, mz - 0.13))
    K.box(paint, (2.43, -0.30, mz - 0.20), (2.47, 0.30, mz - 0.13))
    for yy in (-0.2, 0.25):
        K.box(paint, (mx - 0.39, yy - 0.02, 0.0), (mx - 0.35, yy + 0.02, mz - 0.20))
    gpts = []
    for k in range(10):
        a = math.pi / 2 + math.pi * k / 9
        gpts.append((mx + 0.09 * math.cos(a), mz + 0.09 * math.sin(a)))
    for k in range(10):
        a = -math.pi / 2 + math.pi * k / 9
        gpts.append((xd + 0.13 * math.cos(a), zd + 0.13 * math.sin(a)))
    gp = [(x, z) for x, z in gpts]
    guard = bm_prism(gp, 0.405, 0.455)
    K.add(yellow, guard, Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
    # Seitenfuehrung (Winkel) auf beiden Holmen
    for s in (-1, 1):
        K.box(paint, (-2.7, s * 0.31 - 0.003, 0.882), (2.7, s * 0.31 + 0.003, 0.96))
        K.box(paint, (-2.7, min(s * 0.31, s * 0.36), 0.882), (2.7, max(s * 0.31, s * 0.36), 0.888))
    # Reissleine mit Not-Halt-Schalter (Seite -y)
    cy = -0.40
    posts = [-2.6, -1.3, 0.0, 1.3, 2.6]
    for x in posts:
        K.box(paint, (x - 0.015, -0.365, 0.80), (x + 0.015, -0.355, 0.84))
        K.cyl(paint, (x, -0.38, 0.82), (x, cy, 1.0), 0.008, 6)
        K.tube(P['steel'], arc_pts((x, cy, 1.0), 0.012, 0, TAU, 8, u=(1, 0, 0), v=(0, 0, 1))[:-1], 0.003, 4, caps=False, path_closed=True)
    cord = []
    for i in range(len(posts) - 1):
        for k in range(6):
            t = k / 6
            x = posts[i] + (posts[i + 1] - posts[i]) * t
            cord.append((x, cy, 1.0 - 0.03 * math.sin(math.pi * t)))
    cord.append((posts[-1], cy, 1.0))
    K.tube(mat('reissleine', (0.62, 0.10, 0.06), 'plastic', r=0.9), cord, 0.0035, 5)
    K.box(red, (-2.86, cy - 0.06, 0.92), (-2.70, cy + 0.04, 1.08), bev=0.008, seg=2)
    K.box(yellow, (-2.865, cy - 0.05, 0.94), (-2.86, cy + 0.03, 1.06))
    K.cyl(P['steel'], (-2.70, cy, 1.0), (-2.62, cy, 1.0), 0.008, 6)
    # Gebrauchsspuren: Abplatzer und Rost an Holmen und Beinen, Oelfleck unter dem Getriebe
    for j in range(24):
        s = rng.choice((-1, 1))
        decal(K, P['rust'] if j % 3 else P['steel_dark'], (rng.uniform(-2.8, 2.8), s * 0.3622, rng.uniform(0.755, 0.875)),
              (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), rng.uniform(0.006, 0.025), rng.uniform(0.01, 0.08), 3100 + j, n=8, lift=0.0)
    decal(K, P['oil'], (mx - 0.1, my, 0.0009), (1, 0, 0), (0, 1, 0), 0.25, 0.18, 3200, n=12, lift=0.0)
    # Kartons auf dem Band
    _carton(K, P, (-1.55, 0.04, Zt + 0.0005), (0.40, 0.30, 0.28), 0.12, 31)
    _carton(K, P, (0.35, -0.05, Zt + 0.0005), (0.45, 0.34, 0.33), -0.08, 32)
    _carton(K, P, (1.85, 0.06, Zt + 0.0005), (0.32, 0.26, 0.22), 0.3, 33, state='offen')
    return K.finish()


# =====================================================================================================================
# Generator (generator): Dieselstromaggregat 3,2 x 1,3 m auf Grundrahmen - Kuehler mit Lamellen und Ventilatorschutz,
# Motorblock mit Zylinderkopf, Ventildeckel, Einspritzleitungen, Abgaskrümmer, Kompensator, Schalldaempfer und
# Auspuff mit Regenklappe (2,3 m), Luftfilter, Schwungradgehaeuse, Generator mit Kuehlrippen und Lueftungsgitter,
# Klemmenkasten mit schweren Kabeln zum Boden, Schalttafel (ein Instrument ausgebaut, Draehte haengen heraus).
# Teils ausgeschlachtet; verblasster gruener Lack, Oel, Rost.
# =====================================================================================================================
def build_generator():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(7070)
    green = mat('aggregat_gruen', (0.22, 0.31, 0.23), 'paint', r=1.15)
    green_d = mat('aggregat_gruen_alt', (0.17, 0.22, 0.17), 'paint', r=1.35)
    black = mat('aggregat_schwarz', (0.05, 0.05, 0.05), 'paint', r=1.1)
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    # Grundrahmen: zwei U-Profile laengs, Quertraeger, Gummipuffer
    for s in (-1, 1):
        prof = [(0.0, 0.0), (0.075, 0.0), (0.075, 0.012), (0.009, 0.012), (0.009, 0.168), (0.075, 0.168), (0.075, 0.18), (0.0, 0.18)]
        prof = [(s * 0.62 - s * y, z) for (y, z) in prof]
        K.add(black, bm_prism(prof, -1.58, 1.58), Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
    for x in (-1.45, -0.35, 0.6, 1.45):
        K.box(black, (x - 0.05, -0.61, 0.02), (x + 0.05, 0.61, 0.17))
    for x in (-1.0, -0.2, 0.6, 1.3):
        for s in (-1, 1):
            K.add(P['black_rubber'], bm_cyl(0.045, 0.05, 12, z0=0.18), T(x, s * 0.42, 0))
            K.box(green, (x - 0.07, s * 0.42 - 0.07, 0.23), (x + 0.07, s * 0.42 + 0.07, 0.25))
            bolt(K, P['steel'], (x, s * 0.42, 0.25), Z, d=0.016, stub=0.02)
    # Kuehler (x -1.6 .. -1.3): Netz, Lamellen, Wasserkasten, Einfuellstutzen, Ventilatorschutz
    K.box(green, (-1.58, -0.52, 0.25), (-1.36, 0.52, 1.50), bev=0.008)
    K.box(P['hole'], (-1.585, -0.46, 0.32), (-1.58, 0.46, 1.36))
    for k in range(26):
        z = 0.335 + k * 0.04
        K.box(P['steel_dark'], (-1.595, -0.46, z), (-1.58, 0.46, z + 0.006))
    for y in (-0.3, 0.0, 0.3):
        K.box(green, (-1.60, y - 0.012, 0.32), (-1.58, y + 0.012, 1.36))
    K.cyl(P['steel'], (-1.47, 0.0, 1.50), (-1.47, 0.0, 1.58), 0.05, 14)
    K.add(P['steel_dark'], bm_lathe([(0.0, 0.0), (0.065, 0.0), (0.065, 0.02), (0.03, 0.035), (0.0, 0.037)], 14), T(-1.47, 0, 1.58))
    fan = Vector((-1.30, 0, 0.82))
    K.add(P['steel_dark'], bm_lathe([(0.40, 0.0), (0.40, 0.08), (0.36, 0.10)], 28, cap0=False, cap1=False), frame(fan, X))
    for k in range(6):
        a = TAU * k / 6
        K.tube(P['steel_dark'], [fan + Vector((0.09, 0.05 * math.cos(a), 0.05 * math.sin(a))), fan + Vector((0.09, 0.36 * math.cos(a), 0.36 * math.sin(a)))],
               0.004, 4)
    for rr in (0.12, 0.22, 0.32):
        K.tube(P['steel_dark'], arc_pts(fan + Vector((0.09, 0, 0)), rr, 0, TAU, 20, u=(0, 1, 0), v=(0, 0, 1))[:-1], 0.003, 4, caps=False,
               path_closed=True)
    # Motor (x -1.2 .. 0.25): Oelwanne, Block, Kopf, Ventildeckel
    K.box(black, (-1.15, -0.25, 0.26), (0.20, 0.25, 0.42), bev=0.02, seg=2)
    K.box(green, (-1.20, -0.30, 0.40), (0.25, 0.30, 0.92), bev=0.02, seg=2)
    for k in range(7):
        x = -1.1 + k * 0.2
        for s in (-1, 1):
            K.box(green, (x - 0.015, s * 0.30 - 0.012, 0.42), (x + 0.015, s * 0.30 + 0.012, 0.90))
    K.box(green, (-1.17, -0.27, 0.92), (0.22, 0.27, 1.06), bev=0.01)
    K.box(green_d, (-1.12, -0.21, 1.06), (0.17, 0.21, 1.17), bev=0.035, seg=3)
    for k in range(6):
        x = -1.0 + k * 0.215
        K.add(P['steel_dark'], bm_lathe([(0.0, 0.0), (0.025, 0.0), (0.025, 0.02), (0.0, 0.022)], 10), T(x, 0, 1.17))
        for s in (-1, 1):
            bolt(K, P['steel'], (x + 0.1, s * 0.20, 1.155), Z, d=0.01)
    # Einspritzpumpe und Leitungen (Seite -y)
    K.box(green, (-0.95, -0.42, 0.62), (-0.35, -0.30, 0.80), bev=0.015, seg=2)
    for k in range(6):
        x = -1.0 + k * 0.215
        pa = Vector((-0.90 + k * 0.1, -0.36, 0.80))
        pb = Vector((x, -0.24, 1.07))
        K.tube(P['steel'], catmull([pa, pa + Vector((0, 0, 0.12)), Vector((x, -0.33, 1.02)), pb], 3), 0.0045, 5)
    # Abgaskruemmer (Seite +y), Kompensator, Schalldaempfer, Auspuff
    for k in range(6):
        x = -1.0 + k * 0.215
        K.cyl(P['rust'], (x, 0.27, 0.99), (x, 0.36, 0.99), 0.035, 8)
    exh = bent_path([(-1.08, 0.38, 0.99), (0.18, 0.38, 0.99), (0.18, 0.38, 1.40)], 0.12, 5)
    K.tube(P['rust'], exh, 0.055, 12, caps=True)
    for k in range(7):  # Wellenbalg
        K.add(P['steel'], bm_lathe([(0.058, 0.0), (0.068, 0.012), (0.058, 0.024)], 14, cap0=False, cap1=False), T(0.18, 0.38, 1.40 + k * 0.024))
    K.cyl(P['rust'], (0.18, 0.38, 1.568), (0.18, 0.38, 1.66), 0.055, 12)
    K.add(P['rust_dark'], bm_cyl(0.17, 0.95, 24, z0=-0.475), frame((-0.25, 0.38, 1.80), X))
    for xs in (-0.725, 0.225):
        K.add(P['rust'], bm_lathe([(0.17, 0.0), (0.18, 0.0), (0.18, 0.02), (0.0, 0.025)], 24), frame((xs, 0.38, 1.80), X if xs > 0 else -X))
    for xs in (-0.6, 0.1):
        K.cyl(black, (xs, 0.38, 1.63), (xs, 0.38, 1.17), 0.015, 6)
        K.box(black, (xs - 0.03, 0.22, 1.15), (xs + 0.03, 0.40, 1.17))
    K.cyl(P['rust'], (-0.62, 0.38, 1.94), (-0.62, 0.38, 2.24), 0.05, 12)
    K.add(P['rust_dark'], bm_box((-0.06, -0.06, 0.0), (0.06, 0.06, 0.004)), T(-0.62, 0.38, 2.24) @ R(-0.5, 'Y'))
    K.cyl(P['steel_dark'], (-0.68, 0.36, 2.25), (-0.68, 0.40, 2.25), 0.008, 6)
    # Luftfilter mit Schlauch (Seite -y)
    K.add(P['steel_dark'], bm_cyl(0.13, 0.32, 20, z0=-0.16), frame((-0.25, -0.50, 1.28), X))
    for xs in (-0.41, -0.09):
        K.add(P['steel_dark'], bm_lathe([(0.0, 0.0), (0.135, 0.0), (0.135, 0.012), (0.0, 0.014)], 20), frame((xs, -0.50, 1.28), X if xs > 0 else -X))
    K.tube(P['black_rubber'], catmull([(-0.25, -0.50, 1.15), (-0.25, -0.42, 1.05), (-0.25, -0.30, 1.0)], 3), 0.045, 10)
    K.box(P['steel_dark'], (-0.3, -0.62, 1.12), (-0.2, -0.40, 1.15))
    # Schlaeuche Kuehler - Motor
    K.tube(P['black_rubber'], catmull([(-1.36, 0.25, 1.38), (-1.28, 0.24, 1.30), (-1.20, 0.20, 1.10)], 3), 0.035, 8)
    K.tube(P['black_rubber'], catmull([(-1.36, -0.25, 0.42), (-1.28, -0.24, 0.48), (-1.20, -0.20, 0.55)], 3), 0.035, 8)
    # Schwungradgehaeuse und Generator (x 0.25 .. 1.5)
    gz = 0.68
    K.add(black, bm_lathe([(0.30, 0.0), (0.46, 0.06), (0.46, 0.18), (0.40, 0.22)], 32, cap0=True, cap1=False), frame((0.25, 0, gz), X))
    prof = [(0.40, 0.0)]
    for k in range(16):
        x0 = 0.04 + k * 0.055
        prof += [(0.40, x0), (0.43, x0 + 0.008), (0.43, x0 + 0.022), (0.40, x0 + 0.03)]
    prof += [(0.40, 0.95), (0.36, 1.0), (0.0, 1.02)]
    K.add(green, bm_lathe(prof, 36, cap0=False), frame((0.47, 0, gz), X))
    K.add(P['hole'], bm_lathe([(0.33, 0.0), (0.28, 0.0)], 24, cap0=False, cap1=False), frame((1.4905, 0, gz), X))
    for k in range(12):
        a = TAU * k / 12
        K.tube(green, [(1.488, 0.27 * math.cos(a), gz + 0.27 * math.sin(a)), (1.488, 0.34 * math.cos(a), gz + 0.34 * math.sin(a))], 0.008, 4)
    for s in (-1, 1):  # Fuesse des Generators
        K.box(green, (0.55, s * 0.42 - 0.06, 0.25), (1.40, s * 0.42 + 0.06, 0.33), bev=0.005)
    # Klemmenkasten oben, schwere Kabel zum Boden und weg (Seite -y)
    K.box(green, (0.85, -0.18, 1.08), (1.25, 0.18, 1.34), bev=0.012, seg=2)
    for xx in (0.87, 1.23):
        for yy in (-0.16, 0.16):
            bolt(K, P['steel'], (xx, yy, 1.34), Z, d=0.008)
    for k in range(3):
        x = 0.92 + k * 0.12
        K.cyl(P['steel_dark'], (x, -0.18, 1.15), (x, -0.215, 1.15), 0.03, 8)
        cab = catmull([(x, -0.21, 1.15), (x, -0.32, 1.13), (x - 0.02, -0.48, 0.85), (x - 0.04, -0.62, 0.30), (x - 0.05, -0.70, 0.04),
                       (x - 0.08, -0.85, 0.025), (x - 0.12, -1.20, 0.025)], 3)
        K.tube(P['black_rubber'], cab, 0.024, 8)
    # Schalttafel an der Generatorseite (+x), ein Instrument fehlt, Draehte haengen heraus
    K.box(green, (1.52, -0.32, 0.25), (1.56, -0.26, 1.0))
    K.box(green, (1.52, 0.26, 0.25), (1.56, 0.32, 1.0))
    K.box(green, (1.45, -0.34, 1.0), (1.58, 0.34, 1.55), bev=0.01, seg=2)
    K.box(black, (1.58, -0.30, 1.04), (1.585, 0.30, 1.51))
    white = mat('skala', (0.85, 0.84, 0.78), 'plastic', r=0.9)
    for k, (yy, zz) in enumerate(((-0.17, 1.38), (0.0, 1.38), (0.17, 1.38))):
        if k == 1:
            K.add(P['hole'], bm_lathe([(0.05, 0.0), (0.0, 0.0)], 16, cap0=False, cap1=False), frame((1.5855, yy, zz), X))
            for j in range(4):
                c0 = Vector((1.586, yy + rng.uniform(-0.02, 0.02), zz + rng.uniform(-0.02, 0.02)))
                col = [mat('ader_rot', (0.5, 0.06, 0.04), 'plastic', r=1.0), mat('ader_blau', (0.06, 0.14, 0.42), 'plastic', r=1.0),
                       P['black_plastic'], mat('ader_grau', (0.4, 0.4, 0.4), 'plastic', r=1.0)][j]
                K.tube(col, catmull([c0 - Vector((0.03, 0, 0)), c0 + Vector((0.03, rng.uniform(-0.03, 0.03), -0.04)),
                                     c0 + Vector((0.05, rng.uniform(-0.05, 0.05), -0.14 - 0.05 * j))], 3), 0.0025, 4)
            continue
        K.add(P['steel_dark'], bm_lathe([(0.0, 0.0), (0.052, 0.0), (0.052, 0.018), (0.046, 0.022), (0.0, 0.022)], 18), frame((1.585, yy, zz), X))
        K.add(white, bm_cyl(0.044, 0.002, 18, z0=0.0225), frame((1.585, yy, zz), X))
        a = math.radians(rng.uniform(140, 200))
        tip = Vector((1.6098, yy + 0.038 * math.cos(a), zz + 0.038 * math.sin(a)))
        nn = Vector((0, -math.sin(a), math.cos(a))) * 0.0018
        K.add(P['black_plastic'], bm_poly([Vector((1.6098, yy, zz)) - nn, tip, Vector((1.6098, yy, zz)) + nn]))
    for k in range(4):
        yy = -0.22 + k * 0.145
        K.cyl(P['black_plastic'], (1.585, yy, 1.15), (1.61, yy, 1.15), 0.018, 10)
        K.add(P['steel'], bm_box((0.0, -0.004, -0.004), (0.035, 0.004, 0.004)), T(1.61, yy, 1.15) @ R(0.5, 'X'))
    K.box(P['white_plastic'], (1.585, -0.12, 1.47), (1.587, 0.12, 1.50))
    text(K, mat('schrift_schwarz', (0.03, 0.03, 0.03), 'stencil'), 'AGGREGAT 2', (1.5874, -0.11, 1.476), (0, 1, 0), (0, 0, 1), 0.018, w=0.017,
         gap=0.15)
    # Lack: Oelflecken, Rost, Abplatzer
    for j in range(14):
        s = rng.choice((-1, 1))
        decal(K, P['oil'] if j % 3 == 0 else (P['rust'] if j % 3 == 1 else green_d), (rng.uniform(-1.15, 0.2), s * 0.302, rng.uniform(0.45, 0.88)),
              (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), rng.uniform(0.03, 0.1), rng.uniform(0.03, 0.15), 7100 + j, n=10, lift=0.0)
    for j in range(10):
        cdecal(K, P['rust'] if j % 2 else green_d, (0.47, 0, gz), X, Y, 0.432, rng.uniform(-math.pi, 0.4), rng.uniform(0.1, 0.9),
               rng.uniform(0.15, 0.4), rng.uniform(0.04, 0.12), 7200 + j, n=10, lift=0.0004)
    for j in range(6):
        streak(K, P['oil'], (rng.uniform(-1.1, 0.1), -0.3015, 0.75), (1, 0, 0), (0, 0, -1), rng.uniform(0.02, 0.05),
               rng.uniform(0.15, 0.3), 7300 + j)
    decal(K, P['oil'], (-0.5, 0.0, 0.0009), (1, 0, 0), (0, 1, 0), 0.6, 0.3, 7400, n=14, lift=0.0)
    decal(K, P['oil'], (0.8, -0.3, 0.0010), (1, 0, 0), (0, 1, 0), 0.25, 0.2, 7401, n=12, lift=0.0)
    return K.finish()


# =====================================================================================================================
# Gitterrosttreppe (grate_stair): Stahltreppe an der Wand (+y), steigt entlang +x um 4,0 m ueber 5,6 m, oben ein Podest
# 3 m. Wangen aus U-Profil, Gitterroststufen (Tragstaebe, Querstaebe, Rahmen, gelbe Trittkante), Podest aus Gitterrost
# auf Traegern und vier Stuetzen mit Fussplatten, Gelaender (Handlauf, Knieleiste, Pfosten, Fussleiste) auf der
# offenen Seite, Wandhandlauf auf Konsolen. Ursprung am Fuss der Treppe.
# =====================================================================================================================
def build_grate_stair():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(8080)
    galv = P['galv']
    paint = mat('treppe_lack', (0.36, 0.38, 0.37), 'paint', r=1.1)
    rail = mat('gelaender_gelb', (0.78, 0.58, 0.06), 'paint', r=1.0)
    nose = mat('trittkante_gelb', (0.80, 0.62, 0.08), 'paint', r=1.1)
    n, run, rise = 20, 0.28, 0.2
    top = n * rise
    W = 0.6
    ang = math.atan2(rise, run)
    ca, sa = math.cos(ang), math.sin(ang)
    # Wangen (U-Profil 200 x 75, Flansche nach aussen), schraeg
    Lw = (n * run + 0.2) / ca
    for s in (-1, 1):
        y0 = s * (W + 0.02)
        M = T(-0.08, 0, -0.08) @ R(-ang, 'Y')
        K.add(paint, bm_box((0.0, y0 - 0.004, -0.10), (Lw, y0 + 0.004, 0.10)), M)
        for zz in (-0.10, 0.092):
            K.add(paint, bm_box((0.0, min(y0, y0 + s * 0.075), zz), (Lw, max(y0, y0 + s * 0.075), zz + 0.008)), M)
        # Fussplatte, Anker
        K.box(P['steel_dark'], (-0.12, y0 - 0.08, 0.0), (0.18, y0 + 0.08, 0.012))
        for dx in (-0.06, 0.12):
            bolt(K, P['steel'], (dx, y0 + s * 0.05, 0.012), (0, 0, 1), d=0.014, stub=0.012)
        decal(K, P['rust'], (0.0, y0 + s * 0.0042, 0.06), (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), 0.05, 0.12, 8000 + s, lift=0.0)
    # Stufen: Rahmen, Tragstaebe quer (y), Querstaebe, Trittkante
    for k in range(1, n):
        x0 = run * k
        z = rise * k
        x1 = x0 + run + 0.02
        for s in (-1, 1):  # Stirnbleche mit Schrauben
            K.box(galv, (x0, s * W - 0.003, z - 0.07), (x1, s * W + 0.003, z), bev=0.001)
            for xx in (x0 + 0.07, x1 - 0.07):
                bolt(K, P['steel'], (xx, s * (W + 0.024), z - 0.035), (0, s, 0), d=0.01)
        for i in range(9):
            x = x0 + 0.02 + i * (x1 - x0 - 0.04) / 8
            K.box(galv, (x - 0.0016, -W, z - 0.03), (x + 0.0016, W, z))
        for j in range(11):
            y = -W + 0.1 + j * (2 * W - 0.2) / 10
            K.box(galv, (x0, y - 0.003, z - 0.006), (x1, y + 0.003, z))
        K.box(nose, (x0 - 0.005, -W, z - 0.03), (x0 + 0.03, W, z + 0.003), bev=0.002)
        K.box(galv, (x1 - 0.004, -W, z - 0.03), (x1, W, z))
        if rng.random() < 0.5:
            decal(K, P['grime'], (x0 + 0.14, rng.uniform(-0.3, 0.3), z + 0.0008), (1, 0, 0), (0, 1, 0), 0.09, 0.2, 8100 + k, n=10, lift=0.0)
        decal(K, P['steel_dark'], (x0 + 0.012, rng.uniform(-0.3, 0.3), z + 0.0034), (1, 0, 0), (0, 1, 0), 0.012, rng.uniform(0.1, 0.25), 8200 + k,
              n=8, lift=0.0)
    # Podest
    px0, px1 = n * run, n * run + 3.0
    for i in range(int((px1 - px0) / 0.034)):
        x = px0 + 0.017 + i * 0.034
        K.box(galv, (x - 0.0016, -W, top - 0.03), (x + 0.0016, W, top))
    for j in range(13):
        y = -W + 0.05 + j * (2 * W - 0.1) / 12
        K.box(galv, (px0, y - 0.003, top - 0.006), (px1, y + 0.003, top))
    for (lo, hi) in (((px0, -W - 0.005, top - 0.06), (px1, -W + 0.005, top)), ((px0, W - 0.005, top - 0.06), (px1, W + 0.005, top)),
                     ((px1 - 0.01, -W, top - 0.06), (px1, W, top)), ((px0, -W, top - 0.06), (px0 + 0.03, W, top + 0.003))):
        K.box(galv, lo, hi)
    K.box(nose, (px0 - 0.005, -W, top - 0.03), (px0 + 0.03, W, top + 0.004), bev=0.002)
    for s in (-1, 1):
        K.box(paint, (px0, s * W - 0.04, top - 0.22), (px1, s * W + 0.04, top - 0.06))
    K.box(paint, (px0 + 1.45, -W, top - 0.20), (px0 + 1.55, W, top - 0.06))
    for x in (px0 + 0.15, px1 - 0.1):
        for s in (-1, 1):
            K.box(paint, (x - 0.05, s * 0.55 - 0.05, 0.012), (x + 0.05, s * 0.55 + 0.05, top - 0.22), bev=0.004)
            K.box(P['steel_dark'], (x - 0.1, s * 0.55 - 0.1, 0.0), (x + 0.1, s * 0.55 + 0.1, 0.012))
            for dx, dy in ((-0.07, -0.07), (0.07, 0.07)):
                bolt(K, P['steel'], (x + dx, s * 0.55 + dy, 0.012), (0, 0, 1), d=0.014, stub=0.012)
            decal(K, P['rust'], (x, s * 0.55 + s * 0.0502, 0.07), (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), 0.07, 0.04, int(8300 + x * 10 + s), lift=0.0)
    # Fussleiste am Podest (offene Seite und Ende)
    K.box(galv, (px0, -W - 0.012, top), (px1, -W - 0.006, top + 0.10))
    K.box(galv, (px1 - 0.006, -W, top), (px1, W, top + 0.10))
    # Gelaender offene Seite (-y): Pfosten, Handlauf, Knieleiste
    yr = -W - 0.04
    hz, kz = 1.0, 0.5

    def stair_z(x):
        return min(top, max(0.0, x / run * rise))
    for x in (0.30, 1.70, 3.10, 4.50, px0 + 0.05, px0 + 1.5, px1 - 0.05):
        zb = stair_z(x) if x < px0 else top
        K.cyl(rail, (x, yr, zb - 0.06), (x, yr, zb + hz), 0.021, 10)
        K.box(rail, (x - 0.03, yr - 0.008, zb - 0.10), (x + 0.03, yr + 0.02, zb - 0.02))
    for zz in (hz, kz):
        pts = [(0.30, yr, stair_z(0.30) + zz), (px0 + 0.05, yr, top + zz), (px1 - 0.05, yr, top + zz)]
        K.tube(rail, bent_path(pts, 0.15, 4), 0.021 if zz == hz else 0.016, 10)
    K.tube(rail, [(px1 - 0.05, yr, top + hz), (px1 - 0.05, W - 0.05, top + hz)], 0.021, 10)
    K.tube(rail, [(px1 - 0.05, yr, top + kz), (px1 - 0.05, W - 0.05, top + kz)], 0.016, 10)
    K.cyl(rail, (px1 - 0.05, W - 0.05, top - 0.06), (px1 - 0.05, W - 0.05, top + hz), 0.021, 10)
    # Wandhandlauf (+y) auf Konsolen
    yw = W + 0.05
    K.tube(rail, [(0.25, yw, stair_z(0.25) + 0.9), (px0, yw, top + 0.9)], 0.02, 10)
    for x in (0.6, 2.2, 3.8, 5.2):
        zc = stair_z(x) + 0.9
        K.tube(P['steel_dark'], [(x, yw, zc - 0.02), (x, yw + 0.06, zc - 0.08), (x, yw + 0.10, zc - 0.08)], 0.007, 6)
        K.add(P['steel_dark'], bm_cyl(0.03, 0.008, 10), frame((x, yw + 0.10, zc - 0.08), (0, 1, 0)))
    # Abgegriffener Lack am Handlauf (oben)
    for j in range(8):
        x = rng.uniform(0.5, 5.0)
        p0 = Vector((0.30, yr, stair_z(0.30) + hz))
        d = Vector((ca, 0, sa))
        tt = (x - 0.30) / ca
        cdecal(K, P['steel_dark'], p0, d, (0, 0, 1), 0.021, rng.uniform(-0.5, 0.5), tt, 0.7, rng.uniform(0.02, 0.07), 8400 + j, n=8, lift=0.0005)
    return K.finish()


# =====================================================================================================================
# Drehgestell (train_bogie): liegengebliebenes Drehgestell eines alten Zuges (Meterspur), 2,8 x 1,6 m. Zwei Radsaetze
# (Raeder mit Spurkranz und Lauffläche, Speichenscheibe, Achse), Achslager mit Deckel, Seitenrahmen mit Achshaltern,
# Wiege mit Drehpfanne und Gleitstuecken, Schraubenfedern, Bremsbalken mit Bremskloetzen und Gestaenge. Durchgerostet,
# Reste von schwarzem Lack. Ursprung am Boden (die Raeder sinken 3 cm in den Schmutz).
# =====================================================================================================================
def build_train_bogie():
    P = _pal()
    K = Kit(deg=40.0)
    rng = random.Random(9090)
    black = mat('drehgestell_lack', (0.06, 0.055, 0.05), 'paint', r=1.3)
    rust, rustd = P['rust'], P['rust_dark']
    zc = 0.35
    rw = 0.38
    Y = Vector((0, 1, 0))
    for xw in (-0.85, 0.85):
        # Achse mit Lagerzapfen
        K.add(rustd, bm_cyl(0.07, 1.24, 16, z0=-0.62), frame((xw, 0, zc), Y))
        for s in (-1, 1):
            # Rad: Laufflaeche (Kegel 1:20), Spurkranz innen, Scheibe gewoelbt, Nabe
            prof = [(0.0, 0.0), (0.11, 0.0), (0.11, 0.035), (0.30, 0.045), (0.33, 0.05), (rw - 0.004, 0.05), (rw, 0.12), (rw + 0.002, 0.128),
                    (rw + 0.010, 0.135), (rw + 0.027, 0.142), (rw + 0.030, 0.150), (rw + 0.02, 0.158), (0.335, 0.160), (0.31, 0.150),
                    (0.13, 0.130), (0.11, 0.155), (0.0, 0.155)]
            M = frame((xw, s * 0.62, zc), (0, -s, 0))
            K.add(rust, bm_lathe(prof, 36), M)
            # blanke, aber angerostete Laufflaeche
            K.add(P['steel_dark'], bm_lathe([(rw - 0.0033, 0.055), (rw + 0.0006, 0.118)], 36, cap0=False, cap1=False), M)
            # Achslager aussen mit Deckel und Schrauben
            K.add(rustd, bm_cyl(0.11, 0.15, 18, z0=0.0), frame((xw, s * 0.64, zc), (0, s, 0)))
            K.add(rust, bm_lathe([(0.0, 0.0), (0.10, 0.0), (0.10, 0.02), (0.06, 0.035), (0.0, 0.04)], 18), frame((xw, s * 0.79, zc), (0, s, 0)))
            for k in range(6):
                a = TAU * k / 6
                bolt(K, rustd, (xw + 0.085 * math.cos(a), s * 0.79, zc + 0.085 * math.sin(a)), (0, s, 0), d=0.014)
            K.box(rustd, (xw - 0.12, s * 0.64, zc + 0.08), (xw + 0.12, s * 0.79, zc + 0.14), bev=0.01)
    # Seitenrahmen mit Achshaltern (Blech 30 mm, Oberkante 0.62 .. 0.78)
    for s in (-1, 1):
        y0, y1 = s * 0.66, s * 0.71
        poly = [(-1.38, 0.60), (-1.30, 0.78), (1.30, 0.78), (1.38, 0.60), (1.05, 0.60), (1.02, 0.50), (0.68, 0.50), (0.65, 0.60), (0.32, 0.60),
                (0.28, 0.40), (-0.28, 0.40), (-0.32, 0.60), (-0.65, 0.60), (-0.68, 0.50), (-1.02, 0.50), (-1.05, 0.60)]
        b = bm_prism(poly, min(y0, y1), max(y0, y1))
        K.add(black, b, Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, 1, 0, 0), (0, 0, 0, 1))))
        for xw in (-0.85, 0.85):  # Achshalter (Fuehrungen)
            for dx in (-0.15, 0.15):
                K.box(rustd, (xw + dx - 0.025, min(y0, y1) - 0.01, zc - 0.12), (xw + dx + 0.025, max(y0, y1) + 0.01, 0.62), bev=0.005)
            K.box(rustd, (xw - 0.18, min(y0, y1) - 0.01, zc - 0.15), (xw + 0.18, max(y0, y1) + 0.01, zc - 0.12))
        # Schraubenfedern unter der Wiege (Federn auf dem Rahmen)
        for dx in (-0.12, 0.12):
            pts = []
            for k in range(61):
                t = k / 60
                a = TAU * 5 * t
                pts.append((dx + 0.055 * math.cos(a), s * 0.555 + 0.055 * math.sin(a), 0.42 + 0.19 * t))
            K.tube(P['steel_dark'], pts, 0.013, 5)
        K.box(rustd, (-0.22, s * 0.555 - 0.09, 0.40), (0.22, s * 0.555 + 0.09, 0.42))
        # Rost- und Lackreste
        for j in range(8):
            decal(K, rust if j % 2 else rustd, (rng.uniform(-1.2, 1.2), s * 0.7105 if s > 0 else s * 0.7105, rng.uniform(0.62, 0.76)),
                  (0, 0, 1) if s > 0 else (0, 0, -1), (1, 0, 0), rng.uniform(0.03, 0.08), rng.uniform(0.05, 0.25), 9100 + j + 20 * s, n=10, lift=0.0)
    # Wiege mit Drehpfanne und Gleitstuecken
    K.box(black, (-0.18, -0.66, 0.61), (0.18, 0.66, 0.76), bev=0.015, seg=2)
    K.add(rust, bm_lathe([(0.0, 0.0), (0.17, 0.0), (0.17, 0.04), (0.15, 0.045), (0.15, 0.015), (0.0, 0.012)], 24, cap0=False), T(0, 0, 0.76))
    for s in (-1, 1):
        K.box(rustd, (-0.08, s * 0.46 - 0.05, 0.76), (0.08, s * 0.46 + 0.05, 0.80), bev=0.005)
    # Bremsbalken, Bremskloetze, Gestaenge
    for xw, sx in ((-0.85, 1), (0.85, -1)):
        xb = xw + sx * 0.43
        K.cyl(rustd, (xb, -0.58, 0.30), (xb, 0.58, 0.30), 0.03, 8)
        for s in (-1, 1):
            a0 = math.atan2(-(0.30 - zc), xb - xw)
            K.add(rust, bm_lathe([(rw + 0.006, -0.035), (rw + 0.05, -0.035), (rw + 0.05, 0.035), (rw + 0.006, 0.035)], 6, a0=a0 - 0.28, arc=0.56,
                                 cap0=False, cap1=False), frame((xw, s * 0.535, zc), (0, 1, 0), xhint=(1, 0, 0)))
            K.cyl(rustd, (xb, s * 0.53, 0.30), (xb + sx * 0.02, s * 0.50, 0.62), 0.012, 6)
    K.cyl(rustd, (-0.42, 0.0, 0.31), (0.42, 0.0, 0.31), 0.018, 6)
    K.box(rustd, (-0.03, -0.06, 0.25), (0.03, 0.06, 0.55))
    # Schmutz am Boden um die Raeder
    for xw in (-0.85, 0.85):
        for s in (-1, 1):
            decal(K, P['grime'], (xw, s * 0.6, 0.0008), (1, 0, 0), (0, 1, 0), 0.25, 0.12, int(9200 + xw * 10 + s), n=10, lift=0.0)
    return K.finish()


# =====================================================================================================================
# Saeule (column_greek): ionische Saeule 7,9 m (Sanktuarium). Plinthe, attische Basis (Wulst, Kehle, Wulst), Schaft mit
# Schwellung, 24 Kanneluren mit Stegen, Trommelfugen, Anlauf oben und unten; Kapitell mit Perlstab, Eierstab,
# Polster mit Gurt, Voluten (Schnecken mit Auge) vorn und hinten, Abakus, Kaempferblock bis zur Decke. Marmor,
# unten verschmutzt, Abplatzer an Kanten, Laufspuren vom Kapitell.
# =====================================================================================================================
def build_column_greek():
    K = Kit(deg=35.0)
    rng = random.Random(1717)
    S = mat('marmor', (0.95, 0.92, 0.86), 'concrete', tex='marble_white', r=0.8)
    Sd = mat('marmor_schmutz', (0.70, 0.66, 0.60), 'concrete', tex='marble_white', r=1.1)
    Sdk = mat('marmor_dunkel', (0.55, 0.51, 0.46), 'concrete', tex='marble_white', r=1.2)
    H = 7.9
    # Plinthe (leicht beschaedigt)
    b = bm_box((-0.62, -0.62, 0.0), (0.62, 0.62, 0.18), 0.012, 2)
    jitter(b, 0.003, 6.0, 3)
    K.add(Sd, b)
    # attische Basis
    base = [(0.58, 0.18), (0.585, 0.20), (0.58, 0.24), (0.56, 0.27), (0.53, 0.282), (0.52, 0.29), (0.49, 0.30), (0.475, 0.32), (0.48, 0.345),
            (0.50, 0.36), (0.51, 0.375), (0.505, 0.392), (0.49, 0.405), (0.465, 0.41), (0.462, 0.42)]
    K.lathe(Sd, base, 48, cap0=False, cap1=False)
    # Schaft: 24 Kanneluren mit Stegen, Schwellung, Anlauf, Trommelfugen
    z0, z1 = 0.42, H - 0.80
    nfl, per = 24, 4
    zs = [z0, z0 + 0.03, z0 + 0.08, z0 + 0.16]
    zz = z0 + 0.5
    while zz < z1 - 0.3:
        zs.append(zz)
        zz += 0.9
    for j in (1.55, 2.95, 4.35, 5.75):
        zs += [j - 0.004, j, j + 0.004]
    zs += [z1 - 0.16, z1 - 0.08, z1 - 0.03, z1]
    zs = sorted(set(round(z, 4) for z in zs))
    bmS = bmesh.new()
    rings = []
    for z in zs:
        t = (z - z0) / (z1 - z0)
        r = 0.44 - 0.07 * t ** 1.4 + 0.012 * math.sin(math.pi * t)
        if z < z0 + 0.08:
            r += 0.02 * (1 - (z - z0) / 0.08) ** 2
        if z > z1 - 0.08:
            r += 0.016 * (1 - (z1 - z) / 0.08) ** 2
        fade = min(1.0, max(0.0, (z - z0 - 0.04) / 0.12), max(0.0, (z1 - z - 0.04) / 0.12))
        joint = any(abs(z - j) < 0.001 for j in (1.55, 2.95, 4.35, 5.75))
        ring = []
        for i in range(nfl):
            for k in range(per):
                a = TAU * (i + k / per) / nfl
                if k == 0:
                    d = 0.0
                else:
                    u = (k - 0.5) / (per - 1)
                    d = 0.024 * math.sin(math.pi * u) * fade
                rr = (r - d) * (0.994 if joint else 1.0)
                ring.append(bmS.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        rings.append(ring)
    nn = nfl * per
    for i in range(len(rings) - 1):
        for k in range(nn):
            j = (k + 1) % nn
            bmS.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
    # Verschmutzung unten, Laufspuren und Flecken: Flaechen nach Hoehe/Rauschen aufteilen
    def pick(f):
        c = f.calc_center_median()
        nz = noise.noise(Vector((c.x * 3, c.y * 3, c.z * 0.8)))
        if c.z < 0.9 + 0.35 * nz:
            return Sd
        if c.z > z1 - 1.6 and noise.noise(Vector((math.atan2(c.y, c.x) * 3.0, 0.0, 1.7))) > 0.35 and c.z > z1 - 0.3 - 1.4 * (0.5 + 0.5 * nz):
            return Sd
        return S
    K.add_split(bmS, pick)
    # Kapitell: Perlstab, Eierstab, Polster mit Voluten, Abakus
    zc = z1
    K.lathe(S, [(0.38, zc), (0.395, zc + 0.012), (0.40, zc + 0.03), (0.39, zc + 0.045), (0.375, zc + 0.05), (0.38, zc + 0.06)], 48, cap0=False, cap1=False)
    ech = [(0.38, zc + 0.06), (0.43, zc + 0.10), (0.47, zc + 0.15), (0.48, zc + 0.19), (0.46, zc + 0.22), (0.0, zc + 0.22)]
    K.lathe(S, ech, 48, cap0=False)
    for k in range(16):  # Eier und Pfeile
        a = TAU * k / 16
        c = Vector((0.462 * math.cos(a), 0.462 * math.sin(a), zc + 0.155))
        e = bm_sphere(1.0, 8, 6)
        Me = frame(c, (math.cos(a), math.sin(a), 0), xhint=(0, 0, 1)) @ Matrix.Diagonal((0.05, 0.034, 0.022, 1.0))
        K.add(S, e, Me)
        a2 = a + TAU / 32
        tip = Vector((0.47 * math.cos(a2), 0.47 * math.sin(a2), zc + 0.10))
        K.add(Sd, bm_cyl(0.008, 0.07, 5, r2=0.0), frame(tip, (0, 0, 1)))
    # Polster (entlang x) mit Gurt, Voluten an beiden Stirnseiten
    vz = zc + 0.14
    for s in (-1, 1):
        yv = s * 0.36
        prof = [(0.13, -0.40), (0.125, -0.25), (0.105, -0.08), (0.10, 0.0), (0.105, 0.08), (0.125, 0.25), (0.13, 0.40)]
        K.add(S, bm_lathe([(r, z) for r, z in prof], 28, cap0=True, cap1=True), frame((0, yv, vz), (1, 0, 0), xhint=(0, 0, 1)))
        for dx in (-0.035, 0.0, 0.035):
            K.add(S, bm_lathe([(0.101, -0.008), (0.112, 0.0), (0.101, 0.008)], 28, cap0=False, cap1=False), frame((dx, yv, vz), (1, 0, 0)))
        for fx in (-1, 1):
            # Spirale (log.) auf der Stirnflaeche, erhabenes Band
            pts = []
            for k in range(48):
                ph = 2.6 * TAU * k / 47
                rr = 0.125 * math.exp(-0.17 * ph)
                a = math.pi / 2 + s * ph if True else 0
                pts.append(Vector((fx * 0.405, yv + s * rr * math.cos(a) * -1, vz + rr * math.sin(a))))
            K.tube(S, pts, (0.008, 0.008), 4)
            K.add(S, bm_lathe([(0.0, 0.0), (0.022, 0.0), (0.02, 0.008), (0.0, 0.012)], 12), frame((fx * 0.402, yv, vz), (fx, 0, 0)))
    # Kanal zwischen den Voluten (vorn und hinten) und Abakus
    for fx in (-1, 1):
        K.box(S, (fx * 0.385 - 0.012, -0.36, vz + 0.085), (fx * 0.385 + 0.012, 0.36, vz + 0.125))
    ab = bm_box((-0.60, -0.60, vz + 0.125), (0.60, 0.60, vz + 0.205), 0.02, 3)
    jitter(ab, 0.002, 5.0, 7)
    K.add(S, ab)
    K.box(Sd, (-0.5, -0.5, vz + 0.205), (0.5, 0.5, H), bev=0.01)
    # Abplatzer an Plinthe und Abakus
    for j in range(10):
        side = rng.randrange(4)
        u = rng.uniform(-0.55, 0.55)
        if j < 6:
            p = [(0.6205, u, 0.17), (-0.6205, u, 0.17), (u, 0.6205, 0.17), (u, -0.6205, 0.17)][side]
        else:
            p = [(0.6005, u, vz + 0.19), (-0.6005, u, vz + 0.19), (u, 0.6005, vz + 0.19), (u, -0.6005, vz + 0.19)][side]
        nrm = [(1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0)][side]
        uu = Vector((0, 0, 1)).cross(Vector(nrm))
        decal(K, Sdk, p, uu, (0, 0, 1), rng.uniform(0.02, 0.06), rng.uniform(0.008, 0.02), 1720 + j, n=8, lift=0.0008)
    return K.finish()


# --- Ende der Builder ---
