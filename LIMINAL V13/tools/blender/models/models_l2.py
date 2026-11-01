"""V13.3 Level 2 ("Pipe Dreams", Versorgungsgaenge; LOOK_2.md, LEVELSPEC_2.md): Ausstattung neu gebaut.

Alles aus echter Geometrie: Profile (Drehkoerper, gezogene Querschnitte mit Gehrung), Fasen, Schrauben, Muttern,
Naehte, Flansche, Ventilraeder, Gebrauchsspuren als eigene Flaechen (Rost, Abplatzer, Oel, Staub, Laeufer,
Etiketten) mit leichtem Abstand zur Oberflaeche, Dellen als Verformung. Je Material ein Objekt (wenige
Zeichenaufrufe). Die Texturen des Spiels werden aus Weltkoordinaten projiziert; Variation entsteht daher ueber
mehrere Materialien und Geometrie.

Konventionen wie bisher (V12): Meter, z oben, Blick entlang +x. Wandmaschinen stehen mit der Rueckseite an der Wand
(x = 0), Wandteile (Tueren, Kaesten, Notlicht) haben die Wandflaeche bei x = 0, Deckenleuchten die Deckenebene bei
z = 0. Achtung: das Spiel spiegelt y beim Einlesen (glb_import.hpp) - Grenzen im propdump haben -y.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

import common

TAU = 2.0 * math.pi


# =====================================================================================================================
# Geometrie-Kern
# =====================================================================================================================
class Geo:
    """Sammelt Flaechen je Material; build() erzeugt ein Objekt je Material."""

    def __init__(self):
        self.parts = {}

    def add(self, mat, data, M=None, flat=False):
        vs, fs = data
        p = self.parts.setdefault(mat, ([], [], []))
        base = len(p[0])
        if M is None:
            p[0].extend(Vector(v) for v in vs)
        else:
            p[0].extend(M @ Vector(v) for v in vs)
        p[1].extend(tuple(base + i for i in f) for f in fs)
        p[2].extend([flat] * len(fs))
        return self

    def add_split(self, data, pick, M=None, flat=False):
        """Flaechen einzeln einem Material zuordnen: pick(mittelpunkt, normale) -> Material (Rostinseln u. ae.)."""
        vs, fs = data
        vs = [M @ Vector(v) for v in vs] if M is not None else [Vector(v) for v in vs]
        groups = {}
        for f in fs:
            c = Vector((0.0, 0.0, 0.0))
            for i in f:
                c += vs[i]
            c /= len(f)
            n = (vs[f[1]] - vs[f[0]]).cross(vs[f[-1]] - vs[f[0]])
            if n.length > 1e-12:
                n.normalize()
            groups.setdefault(pick(c, n), []).append(f)
        for mat, ff in groups.items():
            if mat is None:
                continue
            used = sorted({i for f in ff for i in f})
            rm = {o: k for k, o in enumerate(used)}
            self.add(mat, ([vs[i] for i in used], [tuple(rm[i] for i in f) for f in ff]), None, flat)
        return self

    def merge(self, other, M=None):
        for mat, (vs, fs, fl) in other.parts.items():
            p = self.parts.setdefault(mat, ([], [], []))
            base = len(p[0])
            p[0].extend((M @ v) if M is not None else v.copy() for v in vs)
            p[1].extend(tuple(base + i for i in f) for f in fs)
            p[2].extend(fl)
        return self

    def deform(self, fn, mats=None):
        for mat, p in self.parts.items():
            if mats is None or mat in mats:
                p[0][:] = [fn(v) for v in p[0]]
        return self

    def tris(self):
        return sum(sum(len(f) - 2 for f in p[1]) for p in self.parts.values())

    def build(self, name='teil', angle=40.0, weighted=True):
        obs = []
        for i, (mat, (vs, fs, fl)) in enumerate(self.parts.items()):
            if not fs:
                continue
            me = bpy.data.meshes.new(f'{name}{i}')
            me.from_pydata([tuple(v) for v in vs], [], list(fs))
            me.materials.append(mat)
            me.shade_smooth()
            me.set_sharp_from_angle(angle=math.radians(angle))
            if any(fl):
                for poly, f in zip(me.polygons, fl):
                    if f:
                        poly.use_smooth = False
            me.validate()
            ob = bpy.data.objects.new(f'{name}{i}', me)
            bpy.context.scene.collection.objects.link(ob)
            if weighted:
                mod = ob.modifiers.new('wn', 'WEIGHTED_NORMAL')
                mod.keep_sharp = True
            obs.append(ob)
        return obs


def _bm_data(bm):
    bm.verts.index_update()
    vs = [v.co.copy() for v in bm.verts]
    fs = [tuple(v.index for v in f.verts) for f in bm.faces]
    bm.free()
    return vs, fs


def _bm_from(data):
    bm = bmesh.new()
    vv = [bm.verts.new(v) for v in data[0]]
    for f in data[1]:
        bm.faces.new([vv[i] for i in f])
    return bm


def box(lo, hi, bev=0.0, seg=1, cuts=0):
    """Quader von lo nach hi, Kanten gefast (bev), optional unterteilt (cuts) fuer Dellen und Rostinseln."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((lo[0] + (v.co.x + 0.5) * (hi[0] - lo[0]), lo[1] + (v.co.y + 0.5) * (hi[1] - lo[1]),
                       lo[2] + (v.co.z + 0.5) * (hi[2] - lo[2])))
    if cuts:
        bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    if bev > 0:
        edges = [e for e in bm.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > 0.2]
        bmesh.ops.bevel(bm, geom=edges, offset=bev, offset_type='OFFSET', segments=seg, profile=0.5,
                        affect='EDGES', clamp_overlap=True)
    return _bm_data(bm)


def lathe(prof, seg=24, a0=0.0, a1=None, close=False, sx=1.0, sy=1.0):
    """Drehkoerper um z: prof = [(r, z), ...] gegen den Uhrzeigersinn um den Querschnitt (aussen rechts), r = 0 Pol."""
    full = a1 is None
    n = seg if full else seg + 1
    span = TAU if full else (a1 - a0)
    cs = [(math.cos(a0 + span * j / seg), math.sin(a0 + span * j / seg)) for j in range(n)]
    vs, fs, rings = [], [], []
    for r, z in prof:
        if r < 1e-7:
            rings.append([len(vs)])
            vs.append(Vector((0.0, 0.0, z)))
        else:
            rings.append(list(range(len(vs), len(vs) + n)))
            vs.extend(Vector((r * c * sx, r * s * sy, z)) for c, s in cs)
    m = n if full else n - 1
    pairs = list(zip(rings[:-1], rings[1:]))
    if close:
        pairs.append((rings[-1], rings[0]))
    for A, B in pairs:
        for j in range(m):
            j1 = (j + 1) % n
            if len(A) == 1 and len(B) == 1:
                continue
            if len(A) == 1:
                fs.append((A[0], B[j1], B[j]))
            elif len(B) == 1:
                fs.append((A[j], A[j1], B[0]))
            else:
                fs.append((A[j], A[j1], B[j1], B[j]))
    return vs, fs


def frame_z(d, up=None):
    z = Vector(d).normalized()
    h = Vector(up) if up is not None else (Vector((0, 0, 1)) if abs(z.z) < 0.95 else Vector((1, 0, 0)))
    x = h.cross(z)
    if x.length < 1e-6:
        x = Vector((1, 0, 0)).cross(z) if abs(z.x) < 0.9 else Vector((0, 1, 0)).cross(z)
    x.normalize()
    y = z.cross(x)
    return x, y, z


def along(a, b, up=None, roll=0.0):
    """Matrix: lokale z-Achse von a nach b (Laenge = |b - a|), lokale x-Achse senkrecht zu up."""
    a = Vector(a)
    x, y, z = frame_z(Vector(b) - a, up)
    if roll:
        c, s = math.cos(roll), math.sin(roll)
        x, y = x * c + y * s, -x * s + y * c
    M = Matrix((x, y, z)).transposed().to_4x4()
    M.translation = a
    return M


def FX(p, roll=0.0):
    """Lokale xy-Ebene (Schilder, Platten) auf eine Flaeche mit Normale +x legen: lokal x -> +y, y -> +z, z -> +x."""
    return Matrix.Translation(Vector(p)) @ Matrix.Rotation(math.pi / 2, 4, 'Y') @ Matrix.Rotation(math.pi / 2, 4, 'Z') @ \
        Matrix.Rotation(roll, 4, 'Z')


def FY(p, s=1, roll=0.0):
    """Wie FX, aber Flaeche mit Normale s*y (s = +1/-1): lokal z -> s*y, lokal y -> +z."""
    R = Matrix(((-s, 0, 0, 0), (0, 0, s, 0), (0, 1, 0, 0), (0, 0, 0, 1))).transposed()
    return Matrix.Translation(Vector(p)) @ R @ Matrix.Rotation(roll, 4, 'Z')


def mt(loc=(0, 0, 0), rot=(0, 0, 0), s=None):
    from mathutils import Euler
    M = Matrix.Translation(Vector(loc)) @ Euler(rot, 'XYZ').to_matrix().to_4x4()
    if s is not None:
        if isinstance(s, (int, float)):
            s = (s, s, s)
        M = M @ Matrix.Diagonal((s[0], s[1], s[2], 1.0))
    return M


def cyl(r, L, seg=16, r2=None, cap=True, bev=0.0):
    """Zylinder entlang z von 0 bis L (mit along(a, b) platzieren); r2 = Radius oben (Kegelstumpf)."""
    r2 = r if r2 is None else r2
    if bev > 0:
        prof = [(0, 0), (r - bev, 0), (r, bev), (r2, L - bev), (r2 - bev, L), (0, L)]
    else:
        prof = [(0, 0), (r, 0), (r2, L), (0, L)]
    if not cap:
        prof = prof[1:-1]
    return lathe(prof, seg)


def circ(r, seg=12, a0=0.0, sx=1.0, sy=1.0):
    return [(r * sx * math.cos(a0 + TAU * j / seg), r * sy * math.sin(a0 + TAU * j / seg)) for j in range(seg)]


def rrect(w, h, r, k=3):
    """Rechteck mit runden Ecken (Mitte im Ursprung), gegen den Uhrzeigersinn."""
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0.0), (-w / 2 + r, h / 2 - r, 90.0), (-w / 2 + r, -h / 2 + r, 180.0),
                       (w / 2 - r, -h / 2 + r, 270.0)):
        for i in range(k + 1):
            a = math.radians(a0 + 90.0 * i / k)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def _ccw(poly):
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    return poly if a > 0 else list(reversed(poly))


def prism(poly, z0, z1, cap0=True, cap1=True):
    """2D-Umriss (x, y) entlang z von z0 bis z1 ziehen."""
    poly = _ccw(list(poly))
    n = len(poly)
    vs = [Vector((x, y, z0)) for x, y in poly] + [Vector((x, y, z1)) for x, y in poly]
    fs = []
    if cap0:
        fs.append(tuple(range(n))[::-1])
    if cap1:
        fs.append(tuple(range(n, 2 * n)))
    for i in range(n):
        j = (i + 1) % n
        fs.append((i, j, n + j, n + i))
    return vs, fs


def fillet(path, r, seg=4):
    """Ecken eines Polygonzugs durch Boegen (Radius r) ersetzen."""
    P = [Vector(p) for p in path]
    out = [P[0]]
    for k in range(1, len(P) - 1):
        d0 = (P[k] - P[k - 1]).normalized()
        d1 = (P[k + 1] - P[k]).normalized()
        cosang = max(-1.0, min(1.0, d0.dot(d1)))
        th = math.acos(cosang)
        if th < 1e-3:
            out.append(P[k])
            continue
        rr = min(r, 0.45 * min((P[k] - P[k - 1]).length, (P[k + 1] - P[k]).length) / max(math.tan(th / 2), 1e-6))
        t = rr * math.tan(th / 2)
        a = P[k] - d0 * t
        nrm = (d1 - d0 * cosang).normalized()
        c = a + nrm * rr
        for i in range(seg + 1):
            ph = th * i / seg
            out.append(c + (-nrm * math.cos(ph) + d0 * math.sin(ph)) * rr)
    out.append(P[-1])
    return out


def sweep(path, prof, caps=True, up=None, twist=0.0):
    """Querschnitt prof [(x, y)] (gegen den Uhrzeigersinn in der Ebene N, B) entlang path ziehen; Ecken mit Gehrung,
    Rahmen mit Paralleltransport (kein Verdrehen)."""
    prof = _ccw(list(prof))
    P = [Vector(p) for p in path]
    P2 = [P[0]]
    for p in P[1:]:
        if (p - P2[-1]).length > 1e-7:
            P2.append(p)
    P = P2
    nseg = len(P) - 1
    dirs = [(P[i + 1] - P[i]).normalized() for i in range(nseg)]
    T0 = dirs[0]
    h = Vector(up) if up is not None else (Vector((0, 0, 1)) if abs(T0.z) < 0.95 else Vector((1, 0, 0)))
    N = h - T0 * h.dot(T0)
    if N.length < 1e-6:
        N = Vector((1, 0, 0)) - T0 * T0.x
    N.normalize()
    if twist:
        N = Matrix.Rotation(twist, 3, T0) @ N
    Ns = [N]
    for i in range(1, nseg):
        q = dirs[i - 1].rotation_difference(dirs[i])
        Ns.append((q @ Ns[-1]).normalized())
    Bs = [dirs[i].cross(Ns[i]) for i in range(nseg)]
    rings = []
    vs = []
    for k in range(nseg + 1):
        ring = []
        if k == 0 or k == nseg:
            i = 0 if k == 0 else nseg - 1
            for x, y in prof:
                ring.append(P[k] + Ns[i] * x + Bs[i] * y)
        else:
            i = k - 1
            Tm = (dirs[k - 1] + dirs[k])
            Tm = Tm.normalized() if Tm.length > 1e-6 else dirs[k]
            for x, y in prof:
                s = P[k - 1] + Ns[i] * x + Bs[i] * y
                den = dirs[i].dot(Tm)
                t = (P[k] - s).dot(Tm) / den if abs(den) > 1e-6 else (P[k] - P[k - 1]).length
                ring.append(s + dirs[i] * t)
        rings.append(list(range(len(vs), len(vs) + len(ring))))
        vs.extend(ring)
    fs = []
    n = len(prof)
    for A, B in zip(rings[:-1], rings[1:]):
        for j in range(n):
            j1 = (j + 1) % n
            fs.append((A[j], A[j1], B[j1], B[j]))
    if caps:
        fs.append(tuple(reversed(rings[0])))
        fs.append(tuple(rings[-1]))
    return vs, fs


def tube(path, r, seg=10, bend=0.0, bseg=4, caps=True, up=None, sx=1.0, sy=1.0):
    pts = fillet(path, bend, bseg) if bend > 0 else path
    return sweep(pts, circ(r, seg, sx=sx, sy=sy), caps=caps, up=up)


def orient(data, nfun):
    """Flaechen umdrehen, deren Normale nicht zu nfun(mittelpunkt) passt (Aufkleber auf gekruemmten Flaechen)."""
    vs, fs = data
    out = []
    for f in fs:
        a, b, c = vs[f[0]], vs[f[1]], vs[f[-1]]
        n = (b - a).cross(c - a)
        m = sum((vs[i] for i in f), Vector()) / len(f)
        out.append(f if n.dot(nfun(m)) >= 0 else tuple(reversed(f)))
    return vs, out


def _wobble(rng, irr):
    ph = [rng.uniform(0, TAU) for _ in range(4)]
    amp = [irr * 0.55, irr * 0.32, irr * 0.2, irr * 0.12]
    return lambda a: max(0.22, 1.0 + amp[0] * math.sin(2 * a + ph[0]) + amp[1] * math.sin(3 * a + ph[1]) +
                         amp[2] * math.sin(5 * a + ph[2]) + amp[3] * math.sin(9 * a + ph[3]))


def blob_uv(S, u0, v0, ru, rv, rng, seg=12, rings=2, irr=0.35, rot=0.0):
    """Unregelmaessiger Fleck auf einer Flaeche S(u, v) -> (punkt, normale), Mitte (u0, v0), Halbachsen ru, rv."""
    fr = _wobble(rng, irr)
    vs = [S(u0, v0)[0]]
    cr, sr = math.cos(rot), math.sin(rot)
    for k in range(1, rings + 1):
        for j in range(seg):
            a = TAU * j / seg
            f = fr(a) * k / rings
            x, y = ru * f * math.cos(a), rv * f * math.sin(a)
            vs.append(S(u0 + x * cr - y * sr, v0 + x * sr + y * cr)[0])
    fs = []
    for j in range(seg):
        fs.append((0, 1 + j, 1 + (j + 1) % seg))
    for k in range(1, rings):
        A, B = 1 + (k - 1) * seg, 1 + k * seg
        for j in range(seg):
            j1 = (j + 1) % seg
            fs.append((A + j, B + j, B + j1, A + j1))
    return vs, fs


def plane_S(c, U, V, off=0.0012):
    c, U, V = Vector(c), Vector(U).normalized(), Vector(V).normalized()
    N = U.cross(V).normalized()
    return (lambda u, v: (c + N * off + U * u + V * v, N)), N


def blob(c, U, V, rx, ry, rng, seg=12, rings=2, irr=0.35, off=0.0012, rot=0.0):
    S, N = plane_S(c, U, V, off)
    return orient(blob_uv(S, 0.0, 0.0, rx, ry, rng, seg, rings, irr, rot), lambda m: N)


def streak_uv(S, u0, v0, w, L, rng, steps=6, down=-1.0, taper=0.8):
    """Laeufer (Rost, Oel, Wasser) von (u0, v0) in Richtung v*down, Breite w, Laenge L."""
    vs, fs = [], []
    ph = rng.uniform(0, TAU)
    for s in range(steps + 1):
        t = s / steps
        hw = 0.5 * w * (1.0 - taper * t) * (0.85 + 0.3 * math.sin(7 * t + ph) ** 2)
        wob = 0.25 * w * math.sin(5.0 * t + ph)
        v = v0 + down * L * t
        vs.append(S(u0 + wob - hw, v)[0])
        vs.append(S(u0 + wob + hw, v)[0])
    for s in range(steps):
        a = 2 * s
        fs.append((a, a + 1, a + 3, a + 2))
    return vs, fs


def strip_uv(S, u0, u1, v0, h, rng, steps=14, irr=0.6, up=1.0):
    """Band von der Kante v = v0 aus mit unregelmaessiger Oberkante (Hoehe h)."""
    vs, fs = [], []
    ph = [rng.uniform(0, TAU) for _ in range(3)]
    for s in range(steps + 1):
        t = s / steps
        u = u0 + (u1 - u0) * t
        hh = h * max(0.15, 1.0 + irr * (0.5 * math.sin(TAU * 1.3 * t + ph[0]) + 0.3 * math.sin(TAU * 3.1 * t + ph[1]) +
                                         0.2 * math.sin(TAU * 7.3 * t + ph[2])))
        vs.append(S(u, v0)[0])
        vs.append(S(u, v0 + up * hh)[0])
    for s in range(steps):
        a = 2 * s
        fs.append((a, a + 2, a + 3, a + 1))
    return vs, fs


def on_plane(G, mat, c, U, V, fn, *args, off=0.0012, **kw):
    """Fleck/Laeufer/Band auf einer Ebene (Mitte c, Achsen U, V; Normale U x V) mit richtiger Ausrichtung."""
    S, N = plane_S(c, U, V, off)
    G.add(mat, orient(fn(S, *args, **kw), lambda m: N))


def cyl_S(r_of, off=0.0012, M=None):
    """Mantel eines stehenden Zylinders: u = Winkel (rad), v = z; r_of(z) Radius."""
    def S(u, v):
        r = r_of(v) + off
        p = Vector((r * math.cos(u), r * math.sin(u), v))
        n = Vector((math.cos(u), math.sin(u), 0.0))
        if M is not None:
            return M @ p, (M.to_3x3() @ n).normalized()
        return p, n
    return S


def hexhead(s, h, seg=6):
    """Sechskant (Umkreisradius s, Hoehe h) mit Fase oben, auf z = 0 stehend."""
    return lathe([(0, 0), (s, 0), (s, h * 0.72), (s * 0.82, h), (0, h)], seg, a0=TAU / 12)


def nut_shape(s, h):
    if s < 0.012:  # kleine Muttern ohne Fase (sparsam, aus der Naehe kaum sichtbar)
        return lathe([(0, 0), (s, 0), (s, h), (0, h)], 6, a0=TAU / 12)
    return lathe([(0, 0), (s, 0), (s, h * 0.8), (s * 0.86, h), (0, h)], 6, a0=TAU / 12)


def dome(r, h, seg=8):
    return lathe([(0, 0), (r, 0), (r * 0.92, h * 0.45), (r * 0.62, h * 0.85), (0, h)], seg)


def torus(R, r, seg=24, seg2=8, a0=0.0, a1=None, sz=1.0):
    prof = [(R + r * math.cos(TAU * j / seg2), r * sz * math.sin(TAU * j / seg2)) for j in range(seg2)]
    return lathe(prof, seg, a0, a1, close=True)


def nz(p, f=1.0, o=(0.0, 0.0, 0.0)):
    """Glattes Rauschen -1..1 an p (mathutils.noise, Perlin)."""
    return noise.noise(Vector((p[0] * f + o[0], p[1] * f + o[1], p[2] * f + o[2])))


def dent_fn(c, d, R, depth):
    """Delle: Punkte nahe c um depth entlang d verschieben (Gauss, Radius R)."""
    c, d = Vector(c), Vector(d).normalized()

    def fn(v):
        q = (v - c).length_squared / (R * R)
        if q > 9.0:
            return v
        return v + d * depth * math.exp(-q)
    return fn


# =====================================================================================================================
# Materialien
# =====================================================================================================================
def mat(name, rgb, look='paint', tex=None, r=None, m=None, glow=0.0):
    rough = {'metal': 0.45, 'galvanized': 0.5, 'glass': 0.08, 'glow': 0.5}.get(look, 0.75)
    metal = {'metal': 0.85, 'galvanized': 0.7}.get(look, 0.0)
    if r is not None:
        rough = min(1.0, 0.6 * r)
    if m is not None:
        metal = m
    return common.material(name, rgb, look=look, tex=tex, glow=glow, rough=rough, metal=metal, r=r, m=m)


def M_common():
    """Gemeinsame Stoffe der Ebene (Rost, Stahl blank, Oel, Staub, Gummi ...)."""
    return {
        'rust': mat('rost', (0.46, 0.30, 0.20), 'metal', tex='metal_rusty_iron', r=1.35, m=0.15),
        'rust_dark': mat('rost_dunkel', (0.30, 0.19, 0.12), 'metal', tex='metal_rusty_iron', r=1.4, m=0.1),
        'rust_stain': mat('rostfahne', (0.36, 0.22, 0.13), 'paint', r=1.3, m=0.0),
        'steel': mat('stahl_blank', (0.52, 0.52, 0.50), 'metal', r=0.75, m=0.8),
        'steel_dark': mat('stahl_dunkel', (0.30, 0.30, 0.29), 'metal', r=0.9, m=0.65),
        'zinc': mat('verzinkt', (0.50, 0.51, 0.50), 'galvanized', r=1.2, m=0.5),
        'oil': mat('oel', (0.07, 0.06, 0.05), 'paint', r=0.32, m=0.0),
        'grease': mat('fett', (0.12, 0.10, 0.07), 'paint', r=0.45, m=0.0),
        'dust': mat('staub', (0.52, 0.49, 0.44), 'concrete', r=1.5, m=0.0),
        'grime': mat('schmutz', (0.28, 0.26, 0.22), 'paint', r=1.35, m=0.0),
        'rubber': mat('gummi', (0.06, 0.06, 0.06), 'rubber', r=1.25),
        'black': mat('schwarz_matt', (0.03, 0.03, 0.03), 'paint', r=1.2),
        'hole': mat('loch', (0.01, 0.01, 0.01), 'paint', r=1.5),
        'brass': mat('messing', (0.62, 0.50, 0.28), 'metal', r=0.7, m=0.85),
        'alu': mat('alu', (0.70, 0.71, 0.72), 'metal', r=0.8, m=0.8),
        'paper': mat('papier', (0.86, 0.85, 0.80), 'paint', r=1.4),
        'ink': mat('druck', (0.10, 0.10, 0.10), 'paint', r=1.2),
        'warn_yel': mat('warn_gelb', (0.86, 0.68, 0.10), 'paint', r=0.9),
        'red': mat('signal_rot', (0.62, 0.10, 0.08), 'paint', r=0.9),
        'glass': mat('glas', (0.80, 0.82, 0.80), 'glass'),
    }


def chips_on_plane(G, matl, c, U, V, w, h, rng, n=6, smin=0.006, smax=0.02, edge=True):
    """Lackabplatzer bevorzugt an den Raendern einer Rechteckflaeche (Mitte c, Breite w entlang U, Hoehe h entlang V)."""
    U, V = Vector(U).normalized(), Vector(V).normalized()
    for _ in range(n):
        s = rng.uniform(smin, smax)
        if edge:
            side = rng.randrange(4)
            t = rng.uniform(-0.5, 0.5)
            u = (t * w) if side < 2 else ((w / 2 - s * 0.9) * (1 if side == 2 else -1))
            v = ((h / 2 - s * 0.9) * (1 if side == 0 else -1)) if side < 2 else (t * h)
        else:
            u, v = rng.uniform(-w / 2, w / 2), rng.uniform(-h / 2, h / 2)
        G.add(matl, blob(Vector(c) + U * u + V * v, U, V, s * rng.uniform(0.8, 1.6), s, rng, seg=8, rings=1, irr=0.5,
                         rot=rng.uniform(0, TAU)))


# =====================================================================================================================
# Normteile: Schrauben, Flansche, Ventilrad, Ventile
# =====================================================================================================================
def bolt(G, matl, p, axis, s, head_h=None, stub=0.0, washer=True, seg=4):
    """Sechskantschraube/Mutter an p, Achse axis (zeigt aus der Flaeche heraus). s = Umkreisradius."""
    p, axis = Vector(p), Vector(axis).normalized()
    hh = head_h or s * 1.25
    M = along(p, p + axis)
    if washer:
        G.add(matl, lathe([(0, 0), (s * 1.35, 0), (s * 1.35, s * 0.18), (0, s * 0.18)], 10), M)
        M = along(p + axis * s * 0.18, p + axis * (s * 0.18 + 1))
    G.add(matl, nut_shape(s, hh), M)
    if stub > 0:
        G.add(matl, lathe([(0, 0), (s * 0.55, 0), (s * 0.55, stub + hh), (0, stub + hh)], seg), M)


def screw_head(G, matl, p, axis, r, slot_mat=None, kind='pan', seg=8):
    """Linsen-/Senkkopf mit Schlitz oder Kreuz (Schlitz als dunkle Flaeche)."""
    p, axis = Vector(p), Vector(axis).normalized()
    M = along(p, p + axis)
    if kind == 'pan':
        G.add(matl, dome(r, r * 0.55, seg), M)
        top = r * 0.52
    else:
        G.add(matl, lathe([(0, 0), (r, 0), (r * 0.95, r * 0.12), (0, r * 0.14)], seg), M)
        top = r * 0.14
    if slot_mat is not None:
        x, y, z = frame_z(axis)
        G.add(slot_mat, box((-r * 0.75, -r * 0.12, top - 0.0004), (r * 0.75, r * 0.12, top + 0.0005)), M)
        G.add(slot_mat, box((-r * 0.12, -r * 0.75, top - 0.0004), (r * 0.12, r * 0.75, top + 0.0005)), M)


def rivet(G, matl, p, axis, r):
    p = Vector(p)
    G.add(matl, dome(r, r * 0.6, 8), along(p, p + Vector(axis)))


def flange(G, mats, p, axis, rp, rf, t=0.022, nb=8, side=1, hub=True, blind=False, seg=14, bolt_s=None, stub=0.012,
           rng=None, rust=0.0):
    """Vorschweissflansch an p, Achse axis (Rohr laeuft in Richtung -axis weg, Dichtflaeche bei p).
    side: +1 Schraubenkoepfe auf der Rohrseite, -1 Muttern mit Gewindeende auf der Rohrseite, 0 keine Schrauben.
    mats: dict mit 'fl' (Flansch), 'bolt', optional 'gasket', 'rust'."""
    p, ax = Vector(p), Vector(axis).normalized()
    M = along(p, p - ax)  # lokales z zeigt zur Rohrseite
    prof = [(rp * 0.96 if not blind else 0.0, 0.0), (rf - 0.003, 0.0), (rf, 0.003), (rf, t)]
    if hub:
        prof += [(rp + 0.012, t), (rp * 0.999, t + 0.032)]
    else:
        prof += [(rp * 0.999, t)]
    # Profil gegen den Uhrzeigersinn: Dichtflaeche (z = 0) nach aussen, Rand, Rueckseite, Ansatz zum Rohr
    G.add(mats['fl'], lathe(prof, seg), M)
    if side == 0 or nb == 0:
        return
    pcd = (rp + rf) / 2 + 0.004
    bs = bolt_s or max(0.006, (rf - rp) * 0.17)
    x, y, z = frame_z(ax)
    for k in range(nb):
        a = TAU * (k + 0.5) / nb
        q = p + (x * math.cos(a) + y * math.sin(a)) * pcd
        back = q - ax * t
        if side > 0:
            bolt(G, mats['bolt'], back, -ax, bs, washer=False)
        else:
            bolt(G, mats['bolt'], back, -ax, bs, stub=stub, washer=False)


def handwheel(G, M3, c, axis, R, rng, spokes=5, paint=None, steel=None, rust=None, grease=None, rim_r=None, seg=24,
              wobble=0.004):
    """Ventilrad nach Massstab: Nabe mit Mutter, Speichen (oval, verjuengt), Kranz mit Dicke und Griffmulden,
    Lackabplatzer am Kranz, Rost in den Ecken, Fett, leichte Schraeglage. c = Mitte der Nabe, axis zeigt vom Ventil weg."""
    c, ax = Vector(c), Vector(axis).normalized()
    paint = paint or M3['red']
    steel = steel or M3['steel']
    rust = rust or M3['rust']
    grease = grease or M3['grease']
    tilt = Matrix.Rotation(rng.uniform(-0.05, 0.05), 4, frame_z(ax)[0])
    M = Matrix.Translation(c) @ tilt @ along((0, 0, 0), ax)
    rr = rim_r or max(0.006, R * 0.085)
    # Kranz: Torus mit ovalem Querschnitt, Griffmulden (Radius wellig), leicht windschief
    ph = rng.uniform(0, TAU)
    seg2 = 6 if R > 0.1 else 5
    vs, fs = [], []
    nmu = 12 if spokes >= 5 else 8
    for j in range(seg):
        a = TAU * j / seg
        Rj = R * (1.0 - 0.012 * math.cos(nmu * a))
        zj = wobble * math.sin(a + ph)
        for i in range(seg2):
            b = TAU * i / seg2
            rx = Rj + rr * math.cos(b)
            vs.append(Vector((rx * math.cos(a), rx * math.sin(a), zj + rr * 1.25 * math.sin(b))))
    for j in range(seg):
        j1 = (j + 1) % seg
        for i in range(seg2):
            i1 = (i + 1) % seg2
            fs.append((j * seg2 + i, j * seg2 + i1, j1 * seg2 + i1, j1 * seg2 + i))
    rim = orient((vs, fs), lambda m: (m - Vector((math.cos(math.atan2(m.y, m.x)) * R, math.sin(math.atan2(m.y, m.x)) * R, 0))))
    o3 = Vector((rng.uniform(-9, 9), rng.uniform(-9, 9), rng.uniform(-9, 9)))

    def rim_pick(m, n):
        # Griffflaechen (aussen/oben) abgegriffen: blanker Stahl, sonst Lack, Rost in Flecken
        k = nz(m, 40.0, o3)
        if n.z > 0.35 and k > 0.15:
            return steel
        if k < -0.45:
            return rust
        return paint
    G.add_split(rim, rim_pick, M)
    # Nabe
    hub_r = max(0.016, R * 0.2)
    hub_h = hub_r * 1.5
    G.add(paint, lathe([(0, -hub_h * 0.5), (hub_r * 0.8, -hub_h * 0.5), (hub_r, -hub_h * 0.3), (hub_r, hub_h * 0.3),
                        (hub_r * 0.8, hub_h * 0.5), (0, hub_h * 0.5)], 14), M)
    # Mutter und Spindelende
    G.add(steel, nut_shape(hub_r * 0.62, hub_h * 0.45), M @ Matrix.Translation((0, 0, hub_h * 0.5)))
    G.add(grease, lathe([(0, 0), (hub_r * 0.33, 0), (hub_r * 0.33, hub_h * 0.55), (hub_r * 0.25, hub_h * 0.65), (0, hub_h * 0.65)], 8),
          M @ Matrix.Translation((0, 0, hub_h * 0.5 + hub_h * 0.45)))
    # Speichen: ovaler Querschnitt, zur Nabe hin dicker, leicht nach hinten gekroepft
    bent = rng.randrange(spokes)
    for k in range(spokes):
        a = TAU * k / spokes + rng.uniform(-0.03, 0.03)
        d = Vector((math.cos(a), math.sin(a), 0.0))
        p0 = d * hub_r * 0.85
        p1 = d * (R * 0.55) + Vector((0, 0, -R * 0.06))
        p2 = d * (R - rr * 0.6) + Vector((0, 0, (0.012 if k == bent else 0.0)))
        w0, w1 = rr * 1.25, rr * 0.8
        for (q0, q1, ww0, ww1) in ((p0, p1, w0, (w0 + w1) / 2), (p1, p2, (w0 + w1) / 2, w1)):
            L = (q1 - q0).length
            Ms = M @ along(q0, q1, up=Vector((0, 0, 1)).cross(d) if True else None)
            G.add(paint, lathe([(0, 0), (ww0, 0), (ww1, L), (0, L)], 5, sx=1.0, sy=0.65), Ms)
    # Rost an den Speichenansaetzen, Fett am Nabenrand
    for k in range(min(spokes, 3)):
        a = TAU * rng.randrange(spokes) / spokes
        G.add(rust, lathe([(0, 0), (rr * 1.5, 0), (rr * 1.4, rr * 0.8), (0, rr)], 6),
              M @ Matrix.Translation((math.cos(a) * (R - rr * 1.6), math.sin(a) * (R - rr * 1.6), -rr * 0.3)))
    G.add(grease, torus(hub_r * 1.02, hub_r * 0.18, 10, 4), M @ Matrix.Translation((0, 0, hub_h * 0.35)))


def spindle(G, matl, a, b, r, pitch=0.004, seg=10, grease=None, rng=None):
    """Spindel mit Gewinde (gestufte Ringe), optional mit Fettfilm im unteren Teil."""
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    n = max(2, int(L / pitch))
    prof = [(0, 0), (r * 0.9, 0)]
    for i in range(n):
        z = L * (i + 0.25) / n
        prof.append((r if i % 2 == 0 else r * 0.88, z))
    prof += [(r * 0.9, L), (0, L)]
    # Gewinde als Zickzack (Flanken): jeder zweite Ring kleiner
    G.add(matl, lathe(prof, seg), along(a, b))
    if grease is not None:
        G.add(grease, lathe([(r * 1.04, L * 0.1), (r * 1.06, L * 0.25), (r * 1.04, L * 0.45)], seg), along(a, b))


# =====================================================================================================================
# Fass (Stahlfass 200 l, ISO 15750: D 585 mm, H 880 mm, zwei Rollsicken, Falze, zwei Spundloecher)
# =====================================================================================================================
RB = 0.2858  # Mantelradius


def _barrel_profile():
    prof = [(0.0, 0.0165), (0.15, 0.0165), (0.262, 0.0172),
            (0.2700, 0.0105), (0.2760, 0.0035), (0.2840, 0.0000), (0.2920, 0.0030), (0.2955, 0.0100),
            (0.2955, 0.0180), (0.2920, 0.0250), (0.2875, 0.0290), (RB, 0.0360)]
    body = [0.09, 0.15, 0.21, 0.27]

    def hoop(zc):
        return [(RB + 0.0018, zc - 0.018), (RB + 0.0075, zc - 0.010), (RB + 0.0108, zc - 0.003),
                (RB + 0.0108, zc + 0.003), (RB + 0.0075, zc + 0.010), (RB + 0.0018, zc + 0.018), (RB, zc + 0.026)]
    prof += [(RB, z) for z in body]
    prof += hoop(0.296)
    prof += [(RB, z) for z in (0.38, 0.44, 0.50, 0.564)]
    prof += hoop(0.590)
    prof += [(RB, z) for z in (0.67, 0.73, 0.79, 0.852)]
    prof += [(0.2875, 0.8580), (0.2920, 0.8620), (0.2955, 0.8690), (0.2955, 0.8780), (0.2920, 0.8850), (0.2840, 0.8880),
             (0.2770, 0.8860), (0.2720, 0.8800), (0.2680, 0.8730), (0.2620, 0.8700), (0.1400, 0.8708), (0.0, 0.8712)]
    return prof


def _r_barrel(z):
    """Aussenradius des Mantels bei z (fuer Aufkleber/Flecken)."""
    for zc in (0.296, 0.590):
        d = abs(z - zc)
        if d < 0.026:
            return RB + 0.0108 * max(0.0, 1.0 - (d / 0.024) ** 2)
    if z < 0.036 or z > 0.852:
        return RB + 0.008
    return RB


def _barrel(v):
    rng = random.Random(4100 + v)
    C = M_common()
    paint = [mat('fass_lack0', (0.12, 0.20, 0.34), 'paint', r=1.15),
             mat('fass_lack1', (0.42, 0.20, 0.12), 'paint', tex='painted_metal_grey', r=1.3),
             mat('fass_lack2', (0.055, 0.055, 0.055), 'paint', r=0.85)][v]
    G = Geo()
    G.add(paint, lathe(_barrel_profile(), 26))
    S = cyl_S(_r_barrel)
    nrm = lambda m: Vector((m.x, m.y, 0))  # noqa: E731
    # Rost kriecht vom unteren Falz hoch, oberer Falz rostig (Wasser steht auf dem Deckel)
    rust_h = [0.035, 0.09, 0.045][v]
    for k in range(3):
        a0 = TAU * k / 3 + rng.uniform(-0.3, 0.3)
        G.add(C['rust'], orient(strip_uv(cyl_S(lambda z: 0.2955, 0.0012), a0, a0 + TAU / 3 * rng.uniform(0.5, 0.95), 0.006,
                                         rust_h, rng, 14, 0.7), nrm))
        a0 = TAU * k / 3 + rng.uniform(-0.3, 0.3)
        G.add(C['rust_dark'] if v != 2 else C['rust'], orient(strip_uv(cyl_S(lambda z: 0.2955, 0.0012), a0, a0 + TAU / 3 * rng.uniform(0.3, 0.8),
                                                                       0.882, rust_h * 0.6, rng, 12, 0.7, up=-1.0), nrm))
    # Scheitel der Rollsicken beim Rollen blank gescheuert (Stuecke)
    for zc in (0.296, 0.590):
        for k in range(rng.randint(3, 5)):
            a0 = rng.uniform(0, TAU)
            Sc = cyl_S(lambda z: RB + 0.0108, 0.0010)
            G.add(C['steel_dark'] if v != 1 else C['rust_dark'],
                  orient(strip_uv(Sc, a0, a0 + rng.uniform(0.25, 0.9), zc - 0.0035, 0.007, rng, 10, 0.5), nrm))
    # Spunde: 2" und 3/4" gegenueber nahe am Rand
    ab = rng.uniform(0, TAU)
    for k, (rb, rpl) in enumerate(((0.205, 0.031), (0.215, 0.019))):
        a = ab + k * math.pi
        cx, cy = rb * math.cos(a), rb * math.sin(a)
        Mb = Matrix.Translation((cx, cy, 0.8705))
        G.add(paint, lathe([(rpl + 0.004, 0.0), (rpl + 0.012, 0.0), (rpl + 0.011, 0.004), (rpl + 0.007, 0.0065),
                            (rpl + 0.003, 0.0060)], 16), Mb)
        if v == 1 and k == 0:  # Stopfen fehlt: Gewinde und dunkles Loch
            G.add(C['rust'], lathe([(rpl + 0.003, 0.006), (rpl, 0.004), (rpl, -0.01), (0, -0.01)], 16), Mb)
            G.add(C['hole'], lathe([(rpl * 0.98, -0.0095), (0, -0.0095)], 16), Mb)
            continue
        G.add(C['steel'] if v != 2 else C['steel_dark'],
              lathe([(rpl + 0.003, 0.006), (rpl, 0.0065), (rpl, 0.009), (rpl * 0.95, 0.0102), (0, 0.0102)], 16), Mb)
        G.add(C['steel_dark'], box((-rpl * 0.75, -0.0035, 0.0100), (rpl * 0.75, 0.0035, 0.0145), bev=0.0012), Mb @ Matrix.Rotation(a, 4, 'Z'))
        if v == 0 and k == 0:  # Kappe aus Kunststoff ueber dem grossen Spund
            capm = mat('fass_kappe', (0.70, 0.14, 0.10), 'plastic', r=0.9)
            G.add(capm, lathe([(0, 0.0105), (rpl + 0.006, 0.0105), (rpl + 0.006, 0.016), (rpl + 0.002, 0.020), (0, 0.021)], 16), Mb)
    # Deckelsicke (konzentrisch)
    G.add(paint, torus(0.11, 0.004, 28, 5, sz=0.6), Matrix.Translation((0, 0, 0.8712)))
    if v == 0:  # Gefahrgut-Etikett mit Raute und Zeilen, Ecke abgerissen
        a0, z0 = rng.uniform(0, TAU), 0.44
        lab = mat('etikett_weiss', (0.80, 0.80, 0.76), 'paint', r=1.3)
        G.add(lab, orient(_cyl_rect(S, a0 - 0.32, a0 + 0.32, z0 - 0.07, z0 + 0.07, 8, 3), nrm))
        S2 = cyl_S(_r_barrel, 0.0022)
        G.add(C['red'], orient(_cyl_poly(S2, a0 - 0.18, z0, 0.045), nrm))
        S3 = cyl_S(_r_barrel, 0.0028)
        G.add(lab, orient(_cyl_poly(S3, a0 - 0.18, z0, 0.036), nrm))
        for i in range(5):
            w = rng.uniform(0.12, 0.24)
            G.add(C['ink'], orient(_cyl_rect(S2, a0 - 0.05, a0 - 0.05 + w, z0 + 0.04 - i * 0.02, z0 + 0.048 - i * 0.02, 3, 1), nrm))
        G.add(paint, orient(blob_uv(cyl_S(_r_barrel, 0.0032), a0 + 0.30, z0 - 0.055, 0.05, 0.022, rng, 8, 1, 0.4), nrm))
        G.add(C['grime'], orient(blob_uv(cyl_S(_r_barrel, 0.0034), a0 + 0.27, z0 - 0.06, 0.03, 0.012, rng, 8, 1, 0.4), nrm))
    if v == 2:  # gelbes Kennband, abgegriffen (Luecken und Abplatzer)
        band = mat('fass_band', (0.72, 0.58, 0.14), 'paint', r=1.15)
        a = rng.uniform(0, TAU)
        Sb = cyl_S(_r_barrel, 0.0012)
        while a < TAU + 0.5:
            L = rng.uniform(0.5, 1.6)
            G.add(band, orient(_cyl_rect(Sb, a, a + L, 0.43, 0.476, max(3, int(L * 6)), 1), nrm))
            a += L + rng.uniform(0.04, 0.2)
        for _ in range(7):
            on_cyl(G, C['black'], cyl_S(_r_barrel, 0.0018), rng.uniform(0, TAU), rng.choice((0.432, 0.474)), 0.03, 0.006, rng, 8, 1, 0.6)
    # Flecken: Rost, Abplatzer, Laeufer, Oel, Staub
    for _ in range([5, 9, 5][v]):
        z = rng.choice([rng.uniform(0.06, 0.25), rng.uniform(0.34, 0.55), rng.uniform(0.63, 0.83)])
        s = rng.uniform(0.010, 0.03) * (1.7 if v == 1 else 1.0)
        on_cyl(G, C['rust'], S, rng.uniform(0, TAU), z, s / RB, s * rng.uniform(0.6, 1.0), rng, 12, 2, 0.5)
    for _ in range([10, 6, 9][v]):
        z = rng.choice([0.278, 0.314, 0.572, 0.608, 0.045, 0.845]) + rng.uniform(-0.006, 0.006)
        s = rng.uniform(0.004, 0.012)
        on_cyl(G, C['steel_dark'] if v != 1 else C['rust_dark'], cyl_S(_r_barrel, 0.0016), rng.uniform(0, TAU), z, s * 1.8 / RB,
               s * 0.6, rng, 8, 1, 0.6)
    for _ in range([2, 5, 1][v]):
        a = rng.uniform(0, TAU)
        G.add(C['rust_stain'], orient(streak_uv(cyl_S(_r_barrel, 0.0014), a, rng.uniform(0.80, 0.845), 0.018 / RB,
                                                rng.uniform(0.12, 0.3), rng, 6), nrm))
    if v == 2:  # Oel aus dem Spund ueber den Falz nach unten, Lache auf dem Deckel, Staub
        for i in range(3):
            G.add(C['oil'], orient(streak_uv(cyl_S(_r_barrel, 0.0019), ab + rng.uniform(-0.15, 0.15), 0.85, 0.02 / RB,
                                             rng.uniform(0.25, 0.6), rng, 8, taper=0.5), nrm))
        G.add(C['oil'], blob((0.17 * math.cos(ab), 0.17 * math.sin(ab), 0.8712), (1, 0, 0), (0, 1, 0), 0.07, 0.05, rng, 14, 2, 0.5,
                             off=0.0009, rot=ab))
        G.add(C['dust'], blob((-0.06 * math.cos(ab), -0.06 * math.sin(ab), 0.8712), (1, 0, 0), (0, 1, 0), 0.16, 0.13, rng, 16, 2, 0.35,
                              off=0.0005, rot=ab))
    if v == 1:  # Rost auf dem Deckel, Wasserrand
        for _ in range(5):
            r0, a = rng.uniform(0.03, 0.22), rng.uniform(0, TAU)
            G.add(C['rust'], blob((r0 * math.cos(a), r0 * math.sin(a), 0.8712), (1, 0, 0), (0, 1, 0), rng.uniform(0.02, 0.06),
                                  rng.uniform(0.015, 0.04), rng, 12, 2, 0.5, off=0.0008))
        G.add(C['rust_dark'], orient(_ring_flat(0.235, 0.012, 0.8718, rng), lambda m: Vector((0, 0, 1))))
    else:
        G.add(C['dust'] if v == 0 else C['grime'], blob((0.0, 0.0, 0.8712), (1, 0, 0), (0, 1, 0), 0.2, 0.17, rng, 16, 2, 0.3, off=0.0004))
    # Dellen (wirken auf Mantel und Flecken)
    dents = [[(0.6, 0.45, 0.07, 0.010)], [(2.2, 0.80, 0.09, 0.022), (4.0, 0.20, 0.07, 0.012)], [(1.1, 0.16, 0.06, 0.012)]][v]
    for a, z, R, d in dents:
        a += ab
        c = Vector((RB * math.cos(a), RB * math.sin(a), z))
        G.deform(dent_fn(c, -Vector((math.cos(a), math.sin(a), 0.25 if z > 0.75 else 0.0)), R, d))
    G.build('fass', angle=38.0)


def _cyl_rect(S, a0, a1, z0, z1, nu, nv):
    vs, fs = [], []
    for j in range(nv + 1):
        for i in range(nu + 1):
            vs.append(S(a0 + (a1 - a0) * i / nu, z0 + (z1 - z0) * j / nv)[0])
    for j in range(nv):
        for i in range(nu):
            a = j * (nu + 1) + i
            fs.append((a, a + 1, a + nu + 2, a + nu + 1))
    return vs, fs


def _cyl_poly(S, a, z, s):
    """Raute (auf der Spitze stehendes Quadrat) auf dem Mantel."""
    du = s / RB
    pts = [(a, z - s), (a + du, z), (a, z + s), (a - du, z)]
    vs = [S(u, v)[0] for u, v in pts]
    return vs, [(0, 1, 2, 3)]


def on_cyl(G, matl, S, a, z, ra, rz, rng, seg=10, rings=2, irr=0.4):
    G.add(matl, orient(blob_uv(S, a, z, ra, rz, rng, seg, rings, irr), lambda m: Vector((m.x, m.y, 0))))


def _ring_flat(r, w, z, rng, seg=32):
    vs, fs = [], []
    ph = rng.uniform(0, TAU)
    for j in range(seg):
        a = TAU * j / seg
        rr = r + 0.012 * math.sin(3 * a + ph)
        ww = w * (0.5 + 0.5 * abs(math.sin(2 * a + ph)))
        vs.append(Vector(((rr - ww / 2) * math.cos(a), (rr - ww / 2) * math.sin(a), z)))
        vs.append(Vector(((rr + ww / 2) * math.cos(a), (rr + ww / 2) * math.sin(a), z)))
    for j in range(seg):
        a, b = 2 * j, 2 * ((j + 1) % seg)
        fs.append((a, a + 1, b + 1, b))
    return vs, fs


def build_barrel():
    _barrel(0)


def build_barrel__v1():
    _barrel(1)


def build_barrel__v2():
    _barrel(2)


# =====================================================================================================================
# Palette (EUR 1200 x 800 x 144) und Kartons
# =====================================================================================================================
def _wood_mats():
    """Saegeraue Fichte: Look 'concrete' (koernig, keine Furnierfugen der Holztextur), je Brett eigener Ton."""
    return [mat('holz_hell', (0.60, 0.52, 0.40), 'concrete', r=1.3), mat('holz_grau', (0.53, 0.49, 0.43), 'concrete', r=1.35),
            mat('holz_dunkel', (0.43, 0.36, 0.28), 'concrete', r=1.3), mat('holz_klotz', (0.56, 0.47, 0.35), 'concrete', r=1.3)]


def _board(G, matl, lo, hi, rng, cracks=None, C=None):
    """Brett mit leicht unregelmaessiger Lage, gefasten Kanten und Rissen (dunkle Fugen laengs)."""
    G.add(matl, box(lo, hi, bev=0.0025, seg=1), mt((rng.uniform(-0.003, 0.003), rng.uniform(-0.002, 0.002), 0),
                                                    (0, 0, rng.uniform(-0.006, 0.006))))
    if C is not None and cracks:
        for _ in range(cracks):
            L = rng.uniform(0.08, 0.3)
            x0 = rng.uniform(lo[0] + 0.02, hi[0] - L - 0.02) if hi[0] - lo[0] > L + 0.05 else lo[0]
            y0 = rng.uniform(lo[1] + 0.01, hi[1] - 0.01)
            on_plane(G, C['hole'], Vector((x0, y0, hi[2])), (1, 0, 0), (0, 1, 0), streak_uv, 0.0, 0.0, 0.0028, L, rng, 5,
                     down=1.0, taper=0.9, off=0.0006)


def _board_broken(G, matl, x0, x1, y0, y1, z0, xb, rng, side):
    """Brettstueck mit gesplitterter Bruchkante bei xb (side -1: Stueck links vom Bruch, +1 rechts)."""
    n = 7
    zig = [(xb + rng.uniform(-0.035, 0.035) + (0.02 if i % 2 else -0.02), y0 + (y1 - y0) * i / (n - 1)) for i in range(n)]
    if side < 0:
        poly = [(x0, y0)] + [(x, y) for x, y in zig] + [(x0, y1)]
    else:
        poly = [(x1, y0), (x1, y1)] + [(x, y) for x, y in reversed(zig)]
    G.add(matl, prism(poly, z0, z0 + 0.022))
    return zig


def _nails(G, matl, xs, ys, z, rng, r=0.0045):
    for x in xs:
        for y in ys:
            G.add(matl, lathe([(0, 0), (r, 0), (r * 0.8, 0.0012), (0, 0.0015)], 6), Matrix.Translation(
                (x + rng.uniform(-0.006, 0.006), y + rng.uniform(-0.006, 0.006), z - 0.0003)))


def _pallet(G, rng, C, state=0):
    W = _wood_mats()
    grime = mat('holz_schmutz', (0.36, 0.31, 0.25), 'concrete', r=1.4)
    oilw = mat('oel_holz', (0.14, 0.12, 0.09), 'concrete', r=0.8)
    rust = C['rust_dark']
    xs = (-0.5275, 0.0, 0.5275)
    for k, (y0, y1) in enumerate(((-0.40, -0.30), (-0.0725, 0.0725), (0.30, 0.40))):
        _board(G, W[(k + state) % 3], (-0.6, y0, 0.0), (0.6, y1, 0.022), rng)
    for x in xs:
        for y, wy in ((-0.35, 0.05), (0.0, 0.0725), (0.35, 0.05)):
            G.add(W[3] if rng.random() < 0.7 else W[2], box((x - 0.0725, y - wy, 0.022), (x + 0.0725, y + wy, 0.100), bev=0.004),
                  mt((rng.uniform(-0.004, 0.004), rng.uniform(-0.004, 0.004), 0)))
    for k, x in enumerate(xs):
        _board(G, W[(k + 1 + state) % 3], (x - 0.0725, -0.40, 0.100), (x + 0.0725, 0.40, 0.122), rng)
    tops = ((-0.400, -0.255), (-0.21375, -0.11375), (-0.0725, 0.0725), (0.11375, 0.21375), (0.255, 0.400))
    for k, (y0, y1) in enumerate(tops):
        ys = [y0 + (y1 - y0) * f for f in ((0.25, 0.75) if y1 - y0 < 0.12 else (0.2, 0.5, 0.8))]
        if state == 2 and k == 3:  # Brett fehlt: Naegel stehen krumm aus den Querbrettern
            for x in xs:
                for y in ys:
                    a = rng.uniform(0, TAU)
                    p0 = Vector((x + rng.uniform(-0.03, 0.03), y, 0.122))
                    p1 = p0 + Vector((0, 0, 0.022))
                    p2 = p1 + Vector((math.cos(a) * 0.012, math.sin(a) * 0.012, 0.012))
                    G.add(rust, tube([p0, p1, p2], 0.0016, 5, bend=0.006, bseg=2))
                    G.add(rust, lathe([(0, 0), (0.0042, 0), (0.0035, 0.0012), (0, 0.0014)], 6), along(p2, p2 + (p2 - p1)))
            continue
        if state == 2 and k == 1:  # gebrochen: linkes Stueck liegt, rechtes ist verrutscht und hochgedrueckt
            xb = 0.06
            _board_broken(G, W[2], -0.6, 0.6, y0, y1, 0.122, xb, rng, -1)
            Gr = Geo()
            _board_broken(Gr, W[2], -0.6, 0.6, y0, y1, 0.0, xb, rng, 1)
            G.merge(Gr, mt((0.6, 0.0, 0.122), (0, -0.07, 0.05)) @ Matrix.Translation((-0.6, 0.0, 0.0)))
            # helle Bruchflaechen (frisches Holz) als Splitter
            for i in range(5):
                yy = y0 + (y1 - y0) * (i + 0.5) / 5
                L = rng.uniform(0.03, 0.07)
                G.add(W[0], prism([(0, -0.004), (L, 0.0), (0, 0.004)], 0.0, 0.006),
                      mt((xb + rng.uniform(-0.02, 0.02), yy, 0.122 + 0.016), (rng.uniform(-0.2, 0.2), -0.35, rng.uniform(-0.3, 0.3))))
            _nails(G, rust, [x + dx for x in xs[:1] for dx in (-0.035, 0.035)], ys, 0.144, rng)
            continue
        _board(G, W[(k + state) % 3], (-0.6, y0, 0.122), (0.6, y1, 0.144), rng, rng.randrange(0, 3), C)
        _nails(G, rust, [x + dx for x in xs for dx in (-0.035, 0.035)], ys, 0.144, rng)
    # Brandstempel (EPAL/EUR) auf den aeusseren Kloetzen, Kontrollklammer am Mittelklotz
    stamp = mat('brandstempel', (0.20, 0.14, 0.09), 'concrete', r=1.3)
    for x in (-0.5275, 0.5275):
        for sy in (-1, 1):
            c = Vector((x, sy * 0.40, 0.061))
            U = Vector((-sy, 0, 0))
            G.add(stamp, blob(c, U, (0, 0, 1), 0.034, 0.020, rng, 12, 1, 0.12, off=0.0008))
            G.add(W[3], blob(c, U, (0, 0, 1), 0.026, 0.012, rng, 10, 1, 0.1, off=0.0014))
            G.add(stamp, blob(c + Vector((0, 0, 0)), U, (0, 0, 1), 0.016, 0.004, rng, 6, 1, 0.1, off=0.0018))
    G.add(C['steel'], box((-0.012, -0.402, 0.05), (0.012, -0.399, 0.062), bev=0.0006))
    # Schmutz und Abrieb (dezent), ein Oelfleck
    for _ in range(5):
        x, y = rng.uniform(-0.55, 0.55), rng.choice([-0.33, -0.16, 0.0, 0.16, 0.33])
        G.add(grime, blob((x, y, 0.144), (1, 0, 0), (0, 1, 0), rng.uniform(0.03, 0.07), rng.uniform(0.015, 0.035), rng, 12, 2,
                          0.5, off=0.0008))
    if state != 1:
        x, y = rng.uniform(-0.3, 0.3), rng.choice([-0.33, 0.0, 0.33])
        G.add(oilw, blob((x, y, 0.144), (1, 0, 0), (0, 1, 0), 0.05, 0.035, rng, 12, 2, 0.5, off=0.0011))


def _carton(Gp, C, size, state, rng, M):
    """Karton (Wellpappe) nach Massstab: Laschen, Falzlinien, Klebeband, Etikett, Griffloch, Dellen, Kantenschaeden.
    state: 'zu' (zugeklebt), 'offen', 'nass' (durchnaesst, abgesackt). size = (lx, ly, h); Ursprung Boden Mitte."""
    lx, ly, h = size
    G = Geo()
    kart = mat('karton', (0.56, 0.44, 0.31), 'cardboard', r=1.25)
    kart_d = mat('karton_innen', (0.40, 0.30, 0.20), 'cardboard', r=1.4)
    kart_n = mat('karton_nass', (0.40, 0.29, 0.18), 'cardboard', r=0.95)
    kart_rand = mat('karton_rand', (0.30, 0.22, 0.14), 'cardboard', r=1.0)
    tape = mat('klebeband', (0.66, 0.52, 0.32), 'plastic', r=0.55)
    o3 = Vector((rng.uniform(-40, 40), rng.uniform(-40, 40), 0))
    wet_h = h * rng.uniform(0.35, 0.55)

    def pick(c, n):
        if state != 'nass':
            return kart
        k = nz(c, 9.0, o3) * 0.05
        if c.z < wet_h + k:
            return kart_n
        if c.z < wet_h + k + 0.008:
            return kart_rand
        return kart
    if state == 'offen':
        bm = _bm_from(box((-lx / 2, -ly / 2, 0), (lx / 2, ly / 2, h), cuts=3))
        top = [f for f in bm.faces if f.normal.z > 0.9 and min(v.co.z for v in f.verts) > h - 1e-4]
        bmesh.ops.delete(bm, geom=top, context='FACES')
        bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=0.004)
        data = _bm_data(bm)
        G.add_split(data, lambda c, n: kart_d if (abs(c.x) < lx / 2 - 0.003 and abs(c.y) < ly / 2 - 0.003 and n.z < 0.9 and c.z > 0.006
                                                  and (abs(c.x) < lx / 2 - 0.0035 or abs(c.y) < ly / 2 - 0.0035)) else pick(c, n))
        # Laschen: Laengslaschen nach aussen geklappt, Querlaschen halb hoch, eine geknickt
        angs = [rng.uniform(1.9, 2.4), rng.uniform(1.2, 1.7), rng.uniform(0.5, 1.0), rng.uniform(2.6, 3.0)]
        for k, (axis, s) in enumerate((('y', 1), ('y', -1), ('x', 1), ('x', -1))):
            if axis == 'y':
                L, Wd = ly / 2 - 0.004, lx - 0.008
                Mf = Matrix.Translation((0, s * ly / 2, h)) @ Matrix.Rotation(-s * angs[k], 4, 'X')
                G.add(kart, box((-Wd / 2, -0.002, 0), (Wd / 2, 0.002, L), cuts=0), Mf @ Matrix.Rotation(0, 4, 'X') @ mt((0, s * 0.002, 0)))
            else:
                L, Wd = lx / 2 - 0.004, ly - 0.008
                Mf = Matrix.Translation((s * lx / 2, 0, h)) @ Matrix.Rotation(s * angs[k], 4, 'Y')
                G.add(kart_d if k == 3 else kart, box((-0.002, -Wd / 2, 0), (0.002, Wd / 2, L * 0.98)), Mf)
        # Inhalt: zerknuelltes Papier (unregelmaessige Klumpen)
        for i in range(4):
            r0 = rng.uniform(0.04, 0.07)
            bmp = bmesh.new()
            bmesh.ops.create_icosphere(bmp, subdivisions=1, radius=r0)
            for vv in bmp.verts:
                vv.co *= rng.uniform(0.75, 1.25)
            G.add(C['paper'], _bm_data(bmp), Matrix.Translation((rng.uniform(-lx / 3, lx / 3), rng.uniform(-ly / 3, ly / 3), h * 0.75)))
    else:
        G.add_split(box((-lx / 2, -ly / 2, 0), (lx / 2, ly / 2, h), bev=0.003, seg=1, cuts=4), pick)
        # Laschenfuge oben (laengs x), Falz der Deckel an den Laengskanten
        G.add(kart_rand, box((-lx / 2 + 0.004, -0.0015, h - 0.0008), (lx / 2 - 0.004, 0.0015, h + 0.0004)))
        # Klebeband ueber die Fuge und an den Stirnseiten herunter
        tw = 0.048
        G.add(tape, box((-lx / 2 - 0.0006, -tw / 2, h), (lx / 2 + 0.0006, tw / 2, h + 0.0006)))
        for s in (-1, 1):
            Lt = rng.uniform(0.05, 0.08)
            G.add(tape, box((s * lx / 2 - (0.0006 if s > 0 else 0), -tw / 2, h - Lt), (s * lx / 2 + (0.0006 if s < 0 else 0), tw / 2, h)))
        if rng.random() < 0.5:  # Bandende abgeloest, steht ab
            G.add(tape, box((0, -tw / 2, 0), (0.0006, tw / 2, 0.04)), Matrix.Translation((lx / 2, 0, h - 0.07)) @ Matrix.Rotation(0.5, 4, 'Y'))
    # Fabrikkante (geklebte Lasche) an einer Ecke, Griffloecher an den Stirnseiten
    G.add(kart_rand, box((lx / 2 - 0.035, ly / 2, 0.002), (lx / 2 - 0.002, ly / 2 + 0.0012, h - 0.004)))
    for s in (-1, 1):
        if state == 'offen' or h > 0.25:
            G.add(C['hole'], blob((s * lx / 2, 0, h * 0.72), (0, s, 0), (0, 0, 1), 0.045, 0.014, rng, 12, 1, 0.05, off=0.0009))
    # Etikett mit Zeilen und Strichcode auf der Laengsseite (-y)
    if state != 'nass' or rng.random() < 0.5:
        c = Vector((rng.uniform(-lx / 4, lx / 4), -ly / 2, h * 0.55))
        U, V = Vector((1, 0, 0)), Vector((0, 0, 1))
        G.add(C['paper'], box((c.x - 0.06, -ly / 2 - 0.0008, c.z - 0.045), (c.x + 0.06, -ly / 2 + 0.0002, c.z + 0.045)))
        for i in range(4):
            w = rng.uniform(0.04, 0.09)
            G.add(C['ink'], box((c.x - 0.05, -ly / 2 - 0.0012, c.z + 0.03 - i * 0.012), (c.x - 0.05 + w, -ly / 2 - 0.0008, c.z + 0.034 - i * 0.012)))
        x = c.x - 0.05
        while x < c.x + 0.05:
            w = rng.choice((0.0012, 0.0012, 0.0024, 0.0036))
            G.add(C['ink'], box((x, -ly / 2 - 0.0012, c.z - 0.04), (x + w, -ly / 2 - 0.0008, c.z - 0.018)))
            x += w + rng.choice((0.0012, 0.0024))
        if state != 'offen':
            # Pfeile "oben" (zwei Pfeile) auf der Stirnseite
            for k in range(2):
                yy = -0.03 + k * 0.06
                G.add(C['ink'], prism([(-0.008, 0), (0.008, 0), (0.008, 0.03), (0.016, 0.03), (0, 0.05), (-0.016, 0.03), (-0.008, 0.03)],
                                      0, 0.0006), Matrix.Translation((lx / 2 + 0.0004, yy, h * 0.55)) @ Matrix.Rotation(math.pi / 2, 4, 'Y') @
                      Matrix.Rotation(-math.pi / 2, 4, 'Z'))
    # Verformung: Dellen, eingedrueckte Ecke; durchnaesst: abgesackt, Boden gestaucht
    for _ in range(2 if state != 'nass' else 1):
        sx, sy = rng.choice((-1, 1)), rng.choice((-1, 1))
        c = Vector((sx * lx / 2, sy * ly / 2 * rng.uniform(-1, 1), rng.uniform(0.3, 0.9) * h))
        G.deform(dent_fn(c, (-sx, 0, 0), 0.05, 0.012))
    ce = Vector((rng.choice((-1, 1)) * lx / 2, rng.choice((-1, 1)) * ly / 2, h))
    G.deform(dent_fn(ce, (-ce.x, -ce.y, -0.6 * h), 0.05, 0.02))
    if state == 'nass':
        def sag(v):
            t = max(0.0, 1.0 - (v.x / (lx / 2)) ** 2) * max(0.0, 1.0 - (v.y / (ly / 2)) ** 2)
            dz = -0.045 * t * (v.z / h)
            bulge = 0.012 * math.sin(math.pi * min(1.0, v.z / max(h, 1e-6))) * (0.5 + 0.5 * t)
            return Vector((v.x * (1 + bulge / max(lx, 0.1)), v.y * (1 + bulge / max(ly, 0.1)), v.z * 0.94 + dz))
        G.deform(sag)
    Gp.merge(G, M)


def _pallet_model(v):
    rng = random.Random(4200 + v)
    C = M_common()
    G = Geo()
    _pallet(G, rng, C, state=2 if v == 2 else (1 if v in (1, 3) else 0))
    if v == 1:  # drei Kartons nebeneinander: zugeklebt, offen, zugeklebt mit Delle
        _carton(G, C, (0.36, 0.56, 0.40), 'zu', rng, mt((-0.39, 0.04, 0.144), (0, 0, 0.03)))
        _carton(G, C, (0.34, 0.50, 0.30), 'offen', rng, mt((0.0, -0.03, 0.144), (0, 0, -0.05)))
        _carton(G, C, (0.36, 0.56, 0.34), 'zu', rng, mt((0.39, 0.02, 0.144), (0, 0, 0.06)))
    elif v == 3:  # gestapelt: zwei grosse unten, oben ein durchnaesster und ein offener kleiner
        _carton(G, C, (0.58, 0.78, 0.26), 'zu', rng, mt((-0.30, 0.0, 0.144), (0, 0, 0.01)))
        _carton(G, C, (0.58, 0.78, 0.26), 'nass', rng, mt((0.30, 0.0, 0.144), (0, 0, -0.015)))
        _carton(G, C, (0.46, 0.56, 0.24), 'nass', rng, mt((-0.26, 0.05, 0.404), (0, 0, 0.12)))
        _carton(G, C, (0.32, 0.40, 0.22), 'offen', rng, mt((0.30, -0.08, 0.398), (0, 0, -0.2)))
    G.build('palette', angle=40.0)


def build_pallet():
    _pallet_model(0)


def build_pallet__v1():
    _pallet_model(1)


def build_pallet__v2():
    _pallet_model(2)


def build_pallet__v3():
    _pallet_model(3)


# =====================================================================================================================
# Leuchtroehren (T8, 1200 mm) an der Decke - auch in Level 3 und 188 in Gebrauch, Hunderte Stueck: sparsam
# =====================================================================================================================
def _tube(kind):
    """Nackte Lichtleiste: Blechkanal mit Endkappen, zwei G13-Fassungen, Roehre (Glas, Alu-Endkappen mit Stiften,
    geschwaerzte Enden), an zwei Drahtseilen mit Deckenrosette abgehaengt, Kabel mit Verschraubung haengt im Bogen
    zur Decke. Ursprung in der Deckenebene (z = 0)."""
    rng = random.Random({'white': 11, 'orange': 12, 'dead': 13}[kind])
    C = M_common()
    G = Geo()
    body = mat('leiste', (0.74, 0.73, 0.68), 'paint', r=1.05)
    cap = mat('leiste_kappe', (0.62, 0.61, 0.57), 'plastic', r=1.1)
    holder = mat('fassung', (0.80, 0.79, 0.74), 'plastic', r=0.9)
    if kind == 'dead':
        glass = mat('roehre_tot', (0.66, 0.67, 0.64), 'glass', r=3.0)
        endb = mat('roehre_schwarz', (0.07, 0.07, 0.07), 'glass', r=2.0)
    else:
        rgb = (1.0, 0.98, 0.94) if kind == 'white' else (1.0, 0.72, 0.42)
        glass = common.material('roehre', rgb, look='glow', glow=1.6)
        endb = common.material('roehre_ende', tuple(c * 0.55 for c in rgb), look='glow', glow=0.5)
    zt, zb = -0.052, -0.098          # Kanal oben/unten
    zr = -0.124                      # Roehrenachse
    G.add(body, box((-0.615, -0.029, zb), (0.615, 0.029, zt), bev=0.004, seg=1))
    for s in (-1, 1):
        x0, x1 = (0.615, 0.627) if s > 0 else (-0.627, -0.615)
        G.add(cap, box((x0, -0.031, zb - 0.002), (x1, 0.031, zt + 0.002), bev=0.003))
        x = s * 0.598  # Fassung G13 mit Drehteil
        G.add(holder, box((x - 0.011, -0.017, zr - 0.018), (x + 0.011, 0.017, zb + 0.001)))
    L = 0.575  # Roehre: Glas mit geschwaerzten Enden, Alu-Kappen mit zwei Stiften
    for x0, x1, m_ in ((-L, -L + 0.035, endb), (-L + 0.035, L - 0.035, glass), (L - 0.035, L, endb)):
        G.add(m_, lathe([(0.013, 0.0), (0.013, x1 - x0)], 10), along((x0, 0, zr), (x1, 0, zr)))
    for s in (-1, 1):
        G.add(C['alu'], lathe([(0.0, 0.0), (0.0136, 0.0), (0.0136, 0.014), (0.0122, 0.016), (0, 0.016)], 6),
              along((s * L, 0, zr), (s * (L + 0.016), 0, zr)))
        for dy in (-0.006, 0.006):
            G.add(C['steel'], cyl(0.0012, 0.006, 4), along((s * (L + 0.016), dy, zr), (s * (L + 0.022), dy, zr)))
    for x in (-0.42, 0.42):  # Deckenrosette, Haken, Drahtseil, Seilklemme
        G.add(body, lathe([(0, -0.006), (0.014, -0.005), (0.018, 0), (0, 0)], 8), Matrix.Translation((x, 0, 0)))
        G.add(C['steel'], cyl(0.0009, abs(zt + 0.012) - 0.006, 4), along((x, 0, -0.016), (x, 0, zt + 0.004)))
        G.add(C['steel_dark'], cyl(0.0035, 0.012, 4), along((x, 0, zt + 0.014), (x, 0, zt + 0.002)))
    p0 = Vector((0.627, 0.0, -0.075))  # Kabel: Verschraubung an der Endkappe, Bogen zur Decke, Schelle
    G.add(C['black'], lathe([(0, 0), (0.007, 0), (0.007, 0.006), (0.005, 0.012), (0, 0.012)], 8), along(p0 - Vector((0.002, 0, 0)), p0 + Vector((0.01, 0, 0))))
    sag = rng.uniform(0.07, 0.1)
    pts = [p0 + Vector((0.012, 0, 0))]
    for i in range(1, 9):
        t = i / 8
        pts.append(Vector((0.64 + t * 0.36, 0.006 * math.sin(t * 5), min(-0.075 + t * 0.072 - sag * math.sin(math.pi * t) * (1 - t * 0.6), -0.004))))
    pts[-1] = Vector((1.0, 0.004, -0.004))
    G.add(C['black'], tube(pts[::2] + [pts[-1]], 0.0035, 4))
    G.add(C['steel'], box((0.985, -0.008, -0.009), (1.003, 0.008, -0.0)))
    for s in (-1, 1):  # Schmutz an den Kanalseiten
        for _ in range(1):
            G.add(C['grime'] if kind != 'dead' else C['dust'], blob((rng.uniform(-0.55, 0.55), s * 0.0292, (zt + zb) / 2), (-s, 0, 0), (0, 0, 1),
                                                                    rng.uniform(0.04, 0.12), 0.01, rng, 10, 1, 0.5, off=0.0006))
    G.build('roehre', angle=40.0)


def build_tube_white():
    _tube('white')


def build_tube_orange():
    _tube('orange')


def build_tube_dead():
    _tube('dead')


def build_violet_lamp():
    """Notleuchte (Wandmontage, Wandflaeche x = 0): Gehaeuse mit runden Ecken, Prismenscheibe (violett leuchtend),
    Schraubenabdeckungen, gruene Ladeanzeige, Kabel mit Verschraubung oben in die Wand."""
    rng = random.Random(21)
    C = M_common()
    G = Geo()
    hous = mat('notleuchte', (0.70, 0.70, 0.66), 'plastic', r=1.0)
    glow = common.material('violett', (0.42, 0.10, 1.0), look='glow', glow=0.95)
    glow2 = common.material('violett_rippe', (0.30, 0.07, 0.72), look='glow', glow=0.65)
    led = common.material('led_gruen', (0.2, 1.0, 0.3), look='glow', glow=1.5)
    G.add(hous, prism(rrect(0.176, 0.096, 0.014, 3), 0.0, 0.006), FX((0, 0, 0)))
    vs, fs = prism(rrect(0.168, 0.088, 0.012, 3), 0.006, 0.040)
    vs = [Vector((v.x * (1 - 0.06 * (v.z - 0.006) / 0.034), v.y * (1 - 0.1 * (v.z - 0.006) / 0.034), v.z)) for v in vs]
    G.add(hous, (vs, fs), FX((0, 0, 0)))
    G.add(glow, prism(rrect(0.128, 0.056, 0.008, 2), 0.0, 0.006), FX((0.038, 0, 0)))
    for k in range(7):  # Prismenrippen
        z = -0.024 + k * 0.008
        G.add(glow2, lathe([(0, 0), (0.0022, 0), (0.0022, 0.118), (0, 0.118)], 6, sx=0.7), along((0.044, -0.059, z), (0.044, 0.059, z)))
    for y in (-0.074, 0.074):
        G.add(hous, dome(0.007, 0.004, 10), along((0.038, y, 0.0), (0.039, y, 0.0)))
    G.add(led, dome(0.0025, 0.002, 8), along((0.039, 0.07, -0.032), (0.040, 0.07, -0.032)))
    G.add(C['paper'], box((0.0395, -0.075, -0.040), (0.0405, -0.05, -0.030)))
    G.add(C['black'], lathe([(0, 0), (0.0065, 0), (0.0065, 0.005), (0.0045, 0.01), (0, 0.01)], 8), along((0.02, 0.05, 0.043), (0.02, 0.05, 0.053)))
    G.add(C['black'], tube([(0.02, 0.05, 0.052), (0.02, 0.05, 0.07), (0.0, 0.05, 0.075)], 0.0035, 6, bend=0.012, bseg=3))
    G.add(C['dust'], box((0.008, -0.08, 0.0398), (0.034, 0.08, 0.0408)))
    G.add(C['grime'], blob((0.02, 0.0, -0.040), (0, 1, 0), (1, 0, 0), 0.06, 0.008, rng, 10, 1, 0.4, off=0.0006))
    G.build('notlicht', angle=40.0)


# =====================================================================================================================
# Verteilerkasten (Wand, Mitte der Kastenflaeche im Ursprung) und Schaltschrank (Boden, Rueckseite an der Wand)
# =====================================================================================================================
def _warn_triangle(G, C, M, s=0.09, bolt=True):
    """Warnzeichen (W012 Blitz oder W001 Ausrufezeichen) auf der lokalen xy-Ebene (+z vorn), Spitze oben."""
    h = s * 0.866
    G.add(C['black'], prism([(-s / 2, 0), (s / 2, 0), (0, h)], 0.0, 0.0006), M)
    k = 0.12 * s
    G.add(C['warn_yel'], prism([(-s / 2 + k * 1.7, k), (s / 2 - k * 1.7, k), (0, h - k * 2.0)], 0.0006, 0.0010), M)
    if bolt:
        b = s * 0.5
        pts = [(0.05 * b, 0.95 * b), (-0.18 * b, 0.42 * b), (0.0, 0.42 * b), (-0.12 * b, 0.05 * b), (0.2 * b, 0.55 * b), (0.03 * b, 0.55 * b),
               (0.16 * b, 0.95 * b)]
        G.add(C['black'], prism([(x, y + 0.1 * s) for x, y in pts], 0.0010, 0.0013), M)
    else:
        G.add(C['black'], prism([(-0.035 * s, h * 0.34), (0.035 * s, h * 0.34), (0.025 * s, h * 0.72), (-0.025 * s, h * 0.72)], 0.0010, 0.0013), M)
        G.add(C['black'], cyl(0.03 * s, 0.0003, 8), M @ Matrix.Translation((0, h * 0.24, 0.0010)))


def _plate_lines(G, C, M, w, h, rng, n=4, metal=True):
    """Typenschild: Alu-Blech mit Nieten und unleserlichen Zeilen (lokale xy-Ebene, +z vorn)."""
    G.add(C['alu'] if metal else C['paper'], box((-w / 2, -h / 2, 0.0), (w / 2, h / 2, 0.0008), bev=0.0003), M)
    for i in range(n):
        lw = rng.uniform(0.35, 0.85) * (w - 0.012)
        y = h / 2 - 0.008 - i * (h - 0.014) / max(n, 1)
        G.add(C['ink'], box((-w / 2 + 0.006, y - 0.0018, 0.0008), (-w / 2 + 0.006 + lw, y + 0.0018, 0.0011)), M)
    for sx in (-1, 1):
        G.add(C['steel'], dome(0.0022, 0.0012, 6), M @ Matrix.Translation((sx * (w / 2 - 0.004), 0, 0.0008)))


def _saddle(xa, r, t=0.0012, w=0.007):
    """Zweiloch-Rohrschelle: Blechband um ein senkrechtes Rohr (Achse x = xa, y = 0), Fuesse an der Wand x = 0."""
    arc = [Vector((xa + (r + t) * math.cos(a), (r + t) * math.sin(a), 0.0)) for a in [-math.pi / 2 + math.pi * i / 4.0 for i in range(5)]]
    path = [Vector((0.0006, -r - 0.018, 0)), Vector((0.0006, -r - t, 0))] + arc + [Vector((0.0006, r + t, 0)), Vector((0.0006, r + 0.018, 0))]
    return sweep(path, [(-w, -t / 2), (w, -t / 2), (w, t / 2), (-w, t / 2)], up=(0, 0, 1))


def build_junction_box():
    """Stahlblech-Verteiler 600 x 800 x 210 an der Wand: Befestigungslaschen, Tuer mit zwei Scharnieren und zwei
    Vierkantverschluessen, Lueftungskiemen, Warnzeichen, Typenschild, Kreppband-Beschriftung, Kabelverschraubungen
    oben mit Stahlrohren (Versatz, Muffen, Schellen) bis zur Decke; Rost an der Unterkante, Griffspuren, Delle."""
    rng = random.Random(31)
    C = M_common()
    G = Geo()
    paint = mat('verteiler', (0.64, 0.62, 0.55), 'paint', tex='painted_metal_white', r=1.1)
    D, Wy, Hz = 0.205, 0.30, 0.40
    for sy in (-1, 1):  # Befestigungslaschen, Schrauben in die Wand
        for sz in (-1, 1):
            G.add(C['zinc'], box((0.0, sy * Wy - 0.012, sz * (Hz + 0.02) - 0.02), (0.003, sy * Wy + 0.012, sz * (Hz + 0.02) + 0.02)))
            screw_head(G, C['steel'], (0.003, sy * Wy, sz * (Hz + 0.03)), (1, 0, 0), 0.006, kind='flat', seg=6)
    G.add(paint, box((0.0, -Wy, -Hz), (D, Wy, Hz), bev=0.009, seg=2))
    G.add(C['black'], box((D - 0.001, -Wy + 0.012, -Hz + 0.012), (D + 0.002, Wy - 0.012, Hz - 0.012)))  # Dichtung
    xd = D + 0.020
    G.add(paint, box((D + 0.002, -Wy + 0.006, -Hz + 0.006), (xd, Wy - 0.006, Hz - 0.006), bev=0.005, seg=1))
    for z in (-0.28, 0.28):  # Scharniere links
        G.add(C['zinc'], cyl(0.0065, 0.07, 8), along((D + 0.008, -Wy - 0.002, z - 0.035), (D + 0.008, -Wy - 0.002, z + 0.035)))
    for z in (-0.22, 0.22):  # Vierkantverschluesse
        c = Vector((xd, 0.24, z))
        G.add(C['black'], lathe([(0, 0), (0.016, 0), (0.016, 0.004), (0.013, 0.007), (0, 0.007)], 10), along(c, c + Vector((1, 0, 0))))
        G.add(C['steel'], box((0.0, -0.004, -0.004), (0.004, 0.004, 0.004)), Matrix.Translation(c + Vector((0.005, 0, 0))) @ Matrix.Rotation(0.4, 4, 'X'))
        G.add(C['grime'], blob(c, (0, 1, 0), (0, 0, 1), 0.045, 0.06, rng, 12, 2, 0.4, off=0.0009))
    for k in range(4):  # Lueftungskiemen
        z = -Hz + 0.06 + k * 0.02
        G.add(C['hole'], box((xd - 0.0005, -0.12, z), (xd + 0.0004, 0.12, z + 0.008)))
        G.add(paint, box((xd, -0.12, z + 0.008), (xd + 0.007, 0.12, z + 0.0105)))
    _warn_triangle(G, C, FX((xd + 0.0002, -0.04, 0.20)), 0.10)
    _plate_lines(G, C, FX((xd + 0.0002, -0.15, 0.32)), 0.07, 0.035, rng, 3)
    tape = mat('kreppband', (0.80, 0.76, 0.62), 'paint', r=1.35)
    G.add(tape, box((0.0, -0.08, -0.0125), (0.0006, 0.08, 0.0125)), Matrix.Translation((xd, 0.02, -0.05)) @ Matrix.Rotation(0.03, 4, 'X'))
    for i in range(3):
        G.add(C['ink'], box((0.0006, -0.07 + i * 0.05, -0.004), (0.0009, -0.035 + i * 0.05, 0.004)), Matrix.Translation((xd, 0.02, -0.05)) @ Matrix.Rotation(0.03, 4, 'X'))
    for y, r in ((-0.17, 0.016), (0.12, 0.0125)):  # Verschraubungen und Rohre zur Decke
        top = Vector((0.11, y, Hz))
        G.add(C['zinc'], nut_shape(r * 1.55, 0.012), along(top, top + Vector((0, 0, 1))))
        G.add(C['zinc'], lathe([(0, 0), (r * 1.25, 0), (r * 1.2, 0.012), (r * 1.05, 0.018), (0, 0.018)], 8), along(top + Vector((0, 0, 0.012)), top + Vector((0, 0, 1))))
        path = [top + Vector((0, 0, 0.02)), Vector((0.11, y, Hz + 0.12)), Vector((r + 0.012, y, Hz + 0.26)), Vector((r + 0.012, y, 1.3))]
        G.add(C['zinc'], tube(path, r, 7, bend=0.06, bseg=3, caps=False))
        for z in (0.80,):
            G.add(C['zinc'], cyl(r * 1.18, 0.04, 7, cap=False), along((r + 0.012, y, z), (r + 0.012, y, z + 0.04)))
        for z in (1.15,):
            G.add(C['zinc'], _saddle(r + 0.012, r), Matrix.Translation((0, y, z)))
            for sy in (-1, 1):
                screw_head(G, C['steel'], (0.0018, y + sy * (r + 0.011), z), (1, 0, 0), 0.0035, kind='flat', seg=5)
    G.add(C['zinc'], tube([(D - 0.01, -Wy + 0.002, -0.05), (D + 0.012, -Wy - 0.01, -0.02), (D + 0.012, -Wy + 0.004, 0.02)], 0.002, 5, sx=3.0, bend=0.008))
    Sb = lambda u, v: (Vector((xd + 0.0009, u, v)), Vector((1, 0, 0)))  # noqa: E731
    nx = lambda m: Vector((1, 0, 0))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(Sb, -Wy + 0.01, Wy - 0.01, -Hz + 0.006, 0.02, rng, 16, 0.8), nx))
    for y in (-0.25, 0.27, 0.24):
        G.add(C['rust_stain'], orient(streak_uv(Sb, y, -Hz + 0.03 if y != 0.24 else -0.23, 0.011, 0.04 if y != 0.24 else 0.12, rng, 5), nx))
    Sd = lambda u, v: (Vector((D * 0.5 + u, Wy + 0.0009, v)), Vector((0, 1, 0)))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(Sd, -D * 0.45, D * 0.45, -Hz + 0.004, 0.025, rng, 10, 0.8), lambda m: Vector((0, 1, 0))))
    chips_on_plane(G, C['steel_dark'], (xd + 0.0009, 0.0, 0.0), (0, 1, 0), (0, 0, 1), 0.57, 0.77, rng, 7, 0.004, 0.012)
    G.deform(dent_fn((xd, 0.12, -0.25), (-1, 0, 0), 0.05, 0.006))
    G.build('verteiler', angle=40.0)


def build_mach_cabinet():
    """Schaltschrank 1000 x 500 x 2000 auf 100-mm-Sockel: zwei Tueren mit Schwenkhebelgriff, Sichtfenster mit
    Reiheneinbaugeraeten dahinter, Bedienfeld (Messinstrumente, Meldeleuchten, Wahlschalter, Not-Aus), Filterluefter,
    Typenschild, Warnzeichen, Kranoesen, Kabel ueber das Dach in die Wand; Rost am Sockel und an den Tuerunterkanten, Russfahne
    ueber dem Luefter, Abplatzer und Griffspuren, Delle."""
    rng = random.Random(41)
    C = M_common()
    G = Geo()
    paint = mat('schrank_lack', (0.41, 0.44, 0.40), 'paint', r=1.15)
    plinth = mat('sockel', (0.10, 0.10, 0.10), 'paint', r=1.2)
    plast = mat('kunststoff_grau', (0.56, 0.57, 0.55), 'plastic', r=1.0)
    D, W, H0, H1 = 0.50, 0.5, 0.10, 2.10
    for sx in (0, 1):  # Sockel: Eckstuecke und Blende
        for sy in (-1, 1):
            x0 = 0.01 if sx == 0 else D - 0.09
            G.add(plinth, box((x0, sy * W - (0.09 if sy > 0 else 0.0), 0.0), (x0 + 0.09, sy * W + (0.0 if sy > 0 else 0.09), H0)))
    G.add(plinth, box((0.03, -W + 0.09, 0.0), (D - 0.015, W - 0.09, H0 - 0.004)))
    G.add(paint, box((0.0, -W, H0), (D, W, H1), bev=0.006, seg=2))
    G.add(paint, box((0.0, -W - 0.004, H1 - 0.002), (D + 0.008, W + 0.004, H1 + 0.018), bev=0.005, seg=1))
    for sy, x in ((-1, 0.07), (1, D - 0.07)):  # Kranoesen (diagonal)
        G.add(C['steel_dark'], torus(0.022, 0.006, 8, 3), mt((x, sy * (W - 0.07), H1 + 0.045), (math.pi / 2, 0, math.pi / 4 * sy)))
        G.add(C['steel_dark'], cyl(0.014, 0.012, 8), along((x, sy * (W - 0.07), H1 + 0.018), (x, sy * (W - 0.07), H1 + 0.03)))
    xd = D
    G.add(C['black'], box((xd - 0.001, -W + 0.01, H0 + 0.01), (xd + 0.003, W - 0.01, H1 - 0.01)))
    doors = [(-W + 0.004, -0.002), (0.002, W - 0.004)]
    for k, (y0, y1) in enumerate(doors):
        G.add(paint, box((xd + 0.003, y0, H0 + 0.006), (xd + 0.024, y1, H1 - 0.006), bev=0.004, seg=2),
              Matrix.Rotation(0.0 if k == 0 else 0.003, 4, 'X'))
        yh = y0 - 0.001 if k == 0 else y1 + 0.001
        for z in (0.35, 1.1, 1.85):  # Scharnierbolzen
            G.add(C['steel_dark'], cyl(0.006, 0.06, 6), along((xd + 0.012, yh, z), (xd + 0.012, yh, z + 0.06)))
    xf = xd + 0.024
    y = 0.035  # Schwenkhebelgriff mit Schliesszylinder (rechte Tuer an der Mittelfuge)
    G.add(C['black'], prism(rrect(0.04, 0.17, 0.01, 2), 0.0, 0.006), FX((xf, y, 1.05)))
    G.add(C['black'], box((xf + 0.006, y - 0.012, 0.99), (xf + 0.022, y + 0.012, 1.11), bev=0.005, seg=2))
    G.add(C['steel'], lathe([(0, 0), (0.008, 0), (0.008, 0.004), (0, 0.004)], 10), along((xf + 0.022, y, 1.09), (xf + 0.03, y, 1.09)))
    G.add(C['hole'], box((xf + 0.0255, y - 0.001, 1.086), (xf + 0.0262, y + 0.001, 1.094)))
    G.add(C['grime'], blob((xf + 0.0005, y, 1.05), (0, 1, 0), (0, 0, 1), 0.07, 0.12, rng, 14, 2, 0.4, off=0.0009))
    chips_on_plane(G, C['steel_dark'], (xf + 0.0009, y, 1.05), (0, 1, 0), (0, 0, 1), 0.10, 0.22, rng, 4, 0.003, 0.009, edge=False)
    yc, zc, fw, fh = -0.25, 1.45, 0.34, 0.40  # Sichtfenster mit Reiheneinbaugeraeten
    G.add(C['alu'], prism(rrect(fw + 0.03, fh + 0.03, 0.012, 2), 0.0, 0.008), FX((xf, yc, zc)))
    G.add(mat('glas_dunkel', (0.05, 0.06, 0.06), 'glass'), box((xf - 0.004, yc - fw / 2, zc - fh / 2), (xf + 0.0085, yc + fw / 2, zc + fh / 2)))
    face = mat('skala', (0.86, 0.85, 0.80), 'paint', r=0.9)  # Bedienfeld rechte Tuer
    for y in (0.15, 0.32):
        G.add(C['black'], box((xf, y - 0.048, 1.502), (xf + 0.012, y + 0.048, 1.598), bev=0.003))
        G.add(face, box((xf + 0.012, y - 0.04, 1.51), (xf + 0.0125, y + 0.04, 1.59)))
        G.add(C['ink'], lathe([(0.034, 0.0), (0.028, 0.0)], 12, a0=math.radians(45), a1=math.radians(135)), FX((xf + 0.0127, y, 1.52)))
        G.add(C['red'], box((xf + 0.0128, -0.0008, 0.0), (xf + 0.0132, 0.0008, 0.05)), Matrix.Translation((0, y, 1.52)) @ Matrix.Rotation(rng.uniform(-0.5, 0.3), 4, 'X'))
    for k, rgb in enumerate([(0.62, 0.10, 0.08), (0.15, 0.55, 0.15), (0.80, 0.55, 0.10)]):
        y = 0.12 + k * 0.09
        G.add(C['steel'], lathe([(0, 0), (0.016, 0), (0.016, 0.006), (0.013, 0.008), (0, 0.008)], 8), along((xf, y, 1.36), (xf + 1, y, 1.36)))
        G.add(mat(f'meldeleuchte{k}', rgb, 'glass', r=1.5), dome(0.011, 0.012, 8), along((xf + 0.008, y, 1.36), (xf + 1, y, 1.36)))
    G.add(C['steel'], lathe([(0, 0), (0.016, 0), (0.016, 0.006), (0, 0.006)], 8), along((xf, 0.14, 1.22), (xf + 1, 0.14, 1.22)))
    G.add(C['black'], box((0.0, -0.006, -0.02), (0.022, 0.006, 0.02), bev=0.003), Matrix.Translation((xf + 0.006, 0.14, 1.22)) @ Matrix.Rotation(0.6, 4, 'X'))
    G.add(C['warn_yel'], lathe([(0, 0), (0.035, 0), (0.035, 0.004), (0, 0.004)], 12), along((xf, 0.32, 1.22), (xf + 1, 0.32, 1.22)))
    G.add(C['red'], lathe([(0, 0), (0.012, 0), (0.012, 0.018), (0.022, 0.02), (0.022, 0.028), (0.018, 0.034), (0, 0.036)], 10), along((xf + 0.004, 0.32, 1.22), (xf + 1, 0.32, 1.22)))
    _plate_lines(G, C, FX((xf + 0.0002, 0.25, 1.92)), 0.12, 0.06, rng, 5)
    _warn_triangle(G, C, FX((xf + 0.0002, -0.25, 1.76)), 0.10)
    for y in (-0.25, 0.25):  # Filterluefter mit Lamellen
        G.add(plast, box((xf, y - 0.125, 0.25), (xf + 0.014, y + 0.125, 0.50), bev=0.003))
        G.add(C['hole'], box((xf + 0.0135, y - 0.105, 0.27), (xf + 0.0145, y + 0.105, 0.48)))
        for k in range(6):
            z = 0.275 + k * 0.034
            G.add(plast, box((xf + 0.012, y - 0.105, z + 0.012), (xf + 0.022, y + 0.105, z + 0.018)))
    nx = lambda m: Vector((1, 0, 0))  # noqa: E731
    Sf = lambda u, v: (Vector((xf + 0.0009, u, v)), Vector((1, 0, 0)))  # noqa: E731
    G.add(mat('russ', (0.06, 0.055, 0.05), 'paint', r=1.4), orient(streak_uv(Sf, -0.25, 0.51, 0.16, 0.30, rng, 7, down=1.0, taper=0.9), nx))
    G.add(C['zinc'], box((0.10, 0.10, H1 + 0.018), (0.30, 0.40, H1 + 0.023)))  # Kabelverschraubungen, Kabel liegen auf dem Dach in die Wand
    G.add(C['zinc'], box((0.12, 0.10, H1 + 0.023), (0.14, 0.40, H1 + 0.045)))
    for k, y in enumerate((0.16, 0.25, 0.34)):
        G.add(C['black'], nut_shape(0.014, 0.012), along((0.12, y, H1 + 0.033), (1.12, y, H1 + 0.033)))
        G.add(C['black'], cyl(0.008, 0.12, 6), along((0.12, y, H1 + 0.033), (0.0, y, H1 + 0.033)))
    for (y0, y1) in doors:  # Rost an den Tuerunterkanten, Laeufer
        G.add(C['rust'], orient(strip_uv(Sf, y0 + 0.004, y1 - 0.004, H0 + 0.007, 0.035, rng, 14, 0.8), nx))
        for _ in range(2):
            G.add(C['rust_stain'], orient(streak_uv(Sf, rng.uniform(y0 + 0.04, y1 - 0.04), H0 + 0.06, 0.012, 0.05, rng, 4, down=1.0), nx))
    for sy in (-1, 1):
        Ss = (lambda s: (lambda u, v: (Vector((u, s * (W + 0.0009), v)), Vector((0, s, 0)))))(sy)
        G.add(C['rust'], orient(strip_uv(Ss, 0.02, D - 0.02, H0 + 0.001, 0.05, rng, 10, 0.8), lambda m, s=sy: Vector((0, s, 0))))
        chips_on_plane(G, C['steel_dark'], (D / 2, sy * (W + 0.0009), 1.1), (-sy, 0, 0), (0, 0, 1), D, 1.9, rng, 5, 0.004, 0.012)
        for k in range(5):  # Lueftungskiemen in der Seitenwand oben
            z = 1.70 + k * 0.035
            G.add(C['hole'], box((0.12, sy * W - (0.0009 if sy > 0 else -0.0009) * 0 + (0.0 if sy > 0 else -0.001), z), (0.38, sy * W + (0.001 if sy > 0 else 0.0), z + 0.012)))
            G.add(paint, box((0.12, sy * W - 0.002 if sy < 0 else sy * W, z + 0.012), (0.38, sy * W + 0.002 if sy > 0 else sy * W, z + 0.016)))
        G.add(mat('rostfahne_hell', (0.33, 0.31, 0.27), 'paint', r=1.3), orient(streak_uv(Ss, 0.25, 1.69, 0.09, rng.uniform(0.3, 0.6), rng, 7, taper=0.5),
                                                                                  lambda m, s=sy: Vector((0, s, 0))))
    chips_on_plane(G, C['steel_dark'], (xf + 0.0009, 0.0, 1.1), (0, 1, 0), (0, 0, 1), 0.98, 1.95, rng, 10, 0.003, 0.010)
    G.add(C['oil'], blob((D + 0.02, -0.1, H0 + 0.0005), (1, 0, 0), (0, 1, 0), 0.05, 0.07, rng, 12, 2, 0.4, off=0.0006))
    G.deform(dent_fn((xf, -0.32, 0.75), (-1, 0, 0), 0.07, 0.008))
    G.build('schaltschrank', angle=40.0)


# =====================================================================================================================
# Rohrleitungen: Rohr mit Rostuebergaengen, Flanschverbindung, Absperrschieber mit Ventilrad, Manometer, Halterungen
# =====================================================================================================================
class Pipe:
    """Gerades Rohrstueck a -> b (Radius r). S(off) liefert die Mantelflaeche fuer Flecken: u = Winkel, v = Laenge ab a.
    Bei waagrechten Rohren zeigt u = -pi/2 nach unten, u = pi zum Raum (+x) bei Rohren entlang +y."""

    def __init__(self, a, b, r):
        self.a, self.b, self.r = Vector(a), Vector(b), r
        self.d = (self.b - self.a).normalized()
        self.M = along(self.a, self.b)
        self.R3 = self.M.to_3x3()
        self.L = (self.b - self.a).length

    def S(self, off=0.0012, r=None):
        rr = (r or self.r) + off
        return lambda u, v: (self.M @ Vector((rr * math.cos(u), rr * math.sin(u), v)), self.R3 @ Vector((math.cos(u), math.sin(u), 0.0)))

    def nrm(self, m):
        q = m - self.a
        return q - self.d * q.dot(self.d)


def rust_ring(G, C, P, v, rng, w=0.06, dirn=1, seg=None, heavy=False):
    """Rostuebergang am Rohr ab Laenge v in Richtung dirn: braune Fahne (Uebergang) und Rost mit unregelmaessigem Rand."""
    seg = seg or max(12, int(P.r * 200))
    sag = P.r * (1.0 - math.cos(math.pi / seg)) + P.r * (1.0 - math.cos(math.pi / 16))
    G.add(C['rust_stain'], orient(strip_uv(P.S(0.0008 + sag), 0.0, TAU, v, w * 1.6, rng, seg, 0.7, up=dirn), P.nrm))
    G.add(C['rust'], orient(strip_uv(P.S(0.0014 + sag), 0.0, TAU, v, w * (1.0 if heavy else 0.7), rng, seg, 0.8, up=dirn), P.nrm))


def pipe_spots(G, C, P, rng, n=4, v0=0.1, v1=None, under=True, smax=0.03):
    """Rostflecken bevorzugt an der Unterseite waagrechter Rohre."""
    v1 = v1 if v1 is not None else P.L - 0.1
    for _ in range(n):
        u = -math.pi / 2 + rng.uniform(-0.9, 0.9) if under else rng.uniform(0, TAU)
        s = rng.uniform(0.008, smax)
        G.add(C['rust'], orient(blob_uv(P.S(0.0014), u, rng.uniform(v0, v1), s / P.r, s * rng.uniform(1.0, 2.2), rng, 10, 2, 0.5), P.nrm))


def flange_pair(G, Fm, P, v, rp, rf, t, nb, C):
    """Flanschverbindung im Rohr P an Laenge v: zwei Flansche, Dichtung, Schraubenkoepfe auf der einen, Muttern mit
    Gewindeenden auf der anderen Seite."""
    c = P.a + P.d * v
    flange(G, Fm, c + P.d * 0.0015, -P.d, rp, rf, t, nb, side=1)
    flange(G, Fm, c - P.d * 0.0015, P.d, rp, rf, t, 0, side=0)
    G.add(C['black'], lathe([(rf * 0.82, 0.0), (rf * 0.82, 0.003)], 20), along(c - P.d * 0.0015, c + P.d * 0.0015))
    pcd = (rp + rf) / 2 + 0.004
    bs = max(0.006, (rf - rp) * 0.17)
    xx, yy, zz = frame_z(-P.d)
    for k in range(nb):
        a = TAU * (k + 0.5) / nb
        q = c + (xx * math.cos(a) + yy * math.sin(a)) * pcd
        bolt(G, Fm['bolt'], q - P.d * (t + 0.0015), -P.d, bs, stub=0.012, washer=False)


def end_flange(G, Fm, P, at_b, rp, rf, t, nb, heads):
    """Halbe Flanschverbindung am Modulende (bei b oder a), Dichtflaeche geschlossen (Blindflansch: ein Ende ohne
    Gegenstueck ist nicht offen). heads: Schraubenkoepfe (True) oder Muttern mit Gewindeende (False). Die Winkel
    entsprechen frame_z(-axis) wie bei flange(), damit Koepfe und Muttern benachbarter Module fluchten."""
    c, ax = (P.b, P.d) if at_b else (P.a, -P.d)
    flange(G, Fm, c, ax, rp, rf, t, 0, side=0, blind=True)
    xx, yy, zz = frame_z(P.d)
    pcd = (rp + rf) / 2 + 0.004
    bs = max(0.006, (rf - rp) * 0.17)
    for k in range(nb):
        a = TAU * (k + 0.5) / nb
        q = c + (xx * math.cos(a) + yy * math.sin(a)) * pcd - ax * t
        if heads:
            bolt(G, Fm['bolt'], q, -ax, bs, washer=False)
        else:
            bolt(G, Fm['bolt'], q, -ax, bs, stub=0.012, washer=False)


def gauge(G, C, p, nrm, R, rng, stem=0.05):
    """Manometer: Messingstutzen mit Sechskant, Gehaeuse (schwarz), Zifferblatt mit Teilung und Zeiger, Ring.
    p = Anschlusspunkt, nrm = Blickrichtung des Zifferblatts (Stutzen laeuft entlang nrm)."""
    p, n = Vector(p), Vector(nrm).normalized()
    top = p + n * stem
    G.add(C['brass'], cyl(0.006, stem, 8), along(p, top))
    G.add(C['brass'], nut_shape(0.011, 0.012), along(p + n * stem * 0.3, p + n * (stem * 0.3 + 1)))
    back = top
    G.add(C['black'], lathe([(0, 0), (R, 0), (R, 0.026), (R * 0.9, 0.03), (0, 0.03)], 14), along(back, back + n))
    face = mat('zifferblatt', (0.86, 0.85, 0.80), 'paint', r=0.9)
    fz = back + n * 0.0305
    G.add(face, lathe([(0, 0), (R * 0.86, 0), (0, 0.0004)], 14), along(fz, fz + n))
    x, y, z = frame_z(n, up=(0, 0, 1) if abs(n.z) < 0.9 else (1, 0, 0))
    for t in range(7):
        a = math.radians(225 - t * 45)
        d = x * math.cos(a) + y * math.sin(a)
        q0 = fz + n * 0.0006 + d * R * 0.66
        G.add(C['ink'], lathe([(0.0012 if t % 3 else 0.0018, 0.0), (0.0012 if t % 3 else 0.0018, R * 0.14)], 3), along(q0, q0 + d * R * 0.14))
    a = math.radians(225 - rng.uniform(0.1, 0.6) * 270)
    d = x * math.cos(a) + y * math.sin(a)
    G.add(C['red'], cyl(0.0014, R * 0.72, 4), along(fz + n * 0.0016, fz + n * 0.0016 + d * R * 0.72))
    G.add(C['black'], dome(0.004, 0.003, 6), along(fz + n * 0.0016, fz + n * 0.004))
    G.add(C['steel'], torus(R * 0.93, 0.004, 14, 3), along(fz, fz + n))


def gate_valve(G, Vm, C, P, v, rng, R_wheel=0.1, stem_dir=(1, 0, 0), yoke=0.03, rf=None, nb=None, wheel_paint=None, spokes=None, own_flanges=True):
    """Keilflachschieber zwischen zwei Flanschen im Rohr P (Mitte bei Laenge v): Gussgehaeuse, Hals, Deckelflansch mit
    sechs Schrauben, Oberteil, Joch mit zwei Armen und Jochmutter, steigende Spindel mit Gewinde und Fett, Ventilrad.
    Vm: 'body', 'fl', 'bolt'. Liefert die halbe Baulaenge (fuer die Rohrstuecke)."""
    r = P.r
    rf = rf or r * 1.9
    nb = nb or (8 if r > 0.06 else 4)
    Lb = max(0.16, r * 3.0)
    c = P.a + P.d * v
    sd = Vector(stem_dir).normalized()
    for s in (-1, 1):
        fc = c + P.d * s * (Lb / 2)
        if own_flanges:
            flange(G, {'fl': Vm['body'], 'bolt': Vm['bolt']}, fc, P.d * s, r * 1.05, rf, 0.02, 0, side=0, hub=False)
        flange(G, {'fl': Vm['fl'], 'bolt': Vm['bolt']}, fc + P.d * s * 0.002, -P.d * s, r, rf, 0.022, nb, side=-1)
    G.add(Vm['body'], lathe([(r * 1.04, 0), (r * 1.25, Lb * 0.18), (r * 1.32, Lb * 0.5), (r * 1.25, Lb * 0.82), (r * 1.04, Lb)], 14),
          along(c - P.d * (Lb / 2), c + P.d * (Lb / 2)))
    G.add(C['rust_dark'], lathe([(r * 1.335, Lb * 0.32), (r * 1.335, Lb * 0.68)], 8, a0=math.pi * 1.2, a1=math.pi * 1.8),
          along(c - P.d * (Lb / 2), c + P.d * (Lb / 2), up=sd))
    neck0, neck1 = c + sd * r * 0.6, c + sd * r * 1.25
    G.add(Vm['body'], lathe([(0, 0), (r * 1.05, 0), (r * 0.9, (neck1 - neck0).length), (0, (neck1 - neck0).length)], 12, sy=0.62),
          along(neck0, neck1, up=P.d))
    G.add(Vm['body'], lathe([(0, 0), (r * 1.35, 0), (r * 1.35, 0.018), (0, 0.018)], 12, sy=0.7), along(neck1, neck1 + sd, up=P.d))
    xx, yy, zz = frame_z(sd, up=P.d)
    for k in range(4):
        a = TAU * (k + 0.5) / 4
        q = neck1 + sd * 0.018 + (xx * math.cos(a) * r * 1.18 + yy * math.sin(a) * r * 1.18 * 0.7)
        bolt(G, Vm['bolt'], q, sd, max(0.005, r * 0.11), washer=False)
    bon0 = neck1 + sd * 0.018
    bon1 = bon0 + sd * (r * 0.8)
    G.add(Vm['body'], lathe([(0, 0), (r * 0.7, 0), (r * 0.55, r * 0.8), (0, r * 0.8)], 14), along(bon0, bon1, up=P.d))
    top = bon1 + sd * yoke
    for s in (-1, 1):
        a0, a1 = bon1 + xx * s * r * 0.45, top + xx * s * r * 0.2
        G.add(Vm['body'], prism([(-0.010, -0.005), (0.010, -0.005), (0.010, 0.005), (-0.010, 0.005)], 0.0, (a1 - a0).length), along(a0, a1, up=P.d))
    G.add(Vm['body'], lathe([(0, 0), (r * 0.35 + 0.01, 0), (r * 0.35 + 0.01, 0.022), (0, 0.022)], 12), along(top - sd * 0.011, top + sd, up=P.d))
    rs = max(0.007, r * 0.16)
    spindle(G, C['steel'], bon1 + sd * 0.005, top + sd * 0.05, rs, pitch=0.009, seg=8, grease=C['grease'])
    handwheel(G, C, top + sd * 0.03, sd, R_wheel, rng, spokes=spokes or (5 if R_wheel > 0.08 else 4), paint=wheel_paint or C['red'], seg=20 if R_wheel > 0.08 else 16)
    return Lb / 2


def strut_profile(w=0.041, h=0.021, t=0.0025, lip=0.008):
    """C-Profil (Montageschiene 41 x 21): (quer, tief), Oeffnung zur Tiefe +."""
    return [(-w / 2, 0), (w / 2, 0), (w / 2, h), (w / 2 - lip, h), (w / 2 - lip, h - t), (w / 2 - t, h - t), (w / 2 - t, t),
            (-w / 2 + t, t), (-w / 2 + t, h - t), (-w / 2 + lip, h - t), (-w / 2 + lip, h), (-w / 2, h)]


def strut_h(y0, y1, z):
    """Waagrechte Montageschiene an der Wand (x = 0) entlang y in Hoehe z, Oeffnung zum Raum."""
    return sweep([(0, y0, z), (0, y1, z)], strut_profile(), up=(0, 0, 1))


def strut_v(z0, z1):
    """Senkrechte Montageschiene an der Wand bei y = 0, Oeffnung zum Raum."""
    return sweep([(0, 0, z0), (0, 0, z1)], [(v, u) for u, v in strut_profile()])


def ubolt(G, matl, c, axis, r, arm_dir, leg=None):
    """Rundstahlbuegel um ein Rohr (Mitte c, Rohrachse axis), Schenkel in Richtung arm_dir, zwei Muttern am Ende."""
    c, ax, ad = Vector(c), Vector(axis).normalized(), Vector(arm_dir).normalized()
    side = ax.cross(ad).normalized()
    rr = r + 0.005
    leg = leg or (r + 0.03)
    pts = [c + ad * leg + side * rr]
    for i in range(5):
        a = math.pi * i / 4
        pts.append(c + side * math.cos(a) * rr - ad * math.sin(a) * rr)
    pts.append(c + ad * leg - side * rr)
    G.add(matl, tube(pts, 0.0042, 5))


def build_pipe_riser():
    """Steigleitungen an der Wand, aus dem Boden kommend und oben mit Bogen in die Wand (unter den Rohrbuendeln, die
    die Karte oft darueber setzt): DN100 (lackiert, Kennringe Wasser) mit Flanschverbindung, Absperrschieber mit
    Ventilrad zum Raum, Rostuebergaengen; DN50 gedaemmt (Alu-Mantel mit Ueberlappungen, Spannbaendern, Endkappen;
    aufgerissene Stelle mit Mineralwolle und dunkler Kante); zwei Wandkonsolen (C-Schiene, Ausleger, Rundstahlbuegel),
    Bodenmanschetten und Wandrosetten."""
    rng = random.Random(51)
    C = M_common()
    G = Geo()
    paintA = mat('steig_lack', (0.30, 0.38, 0.34), 'paint', tex='painted_metal_grey', r=1.15)
    Fm = {'fl': paintA, 'bolt': C['steel_dark']}
    xA, yA, rp, rf = 0.13, -0.14, 0.057, 0.11
    zt = 1.95  # Beginn des Bogens
    PA = Pipe((xA, yA, 0.0), (xA, yA, zt), rp)
    vz = 1.15
    Lb2 = max(0.16, rp * 3.0) / 2
    for z0, z1 in ((0.0, 0.4485), (0.4515, vz - Lb2 - 0.002), (vz + Lb2 + 0.002, zt)):
        G.add(paintA, cyl(rp, z1 - z0, 12, cap=False), along((xA, yA, z0), (xA, yA, z1)))
    G.add(paintA, torus(0.08, rp, 6, 12, 0.0, math.pi / 2), Matrix.Translation((xA - 0.08, yA, zt)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    G.add(paintA, cyl(rp, xA - 0.08, 12, cap=False), along((xA - 0.08, yA, zt + 0.08), (0.0, yA, zt + 0.08)))
    flange_pair(G, Fm, PA, 0.45, rp, rf, 0.02, 4, C)
    Vm = {'body': mat('guss_schieber', (0.20, 0.22, 0.22), 'paint', r=1.3), 'fl': paintA, 'bolt': C['steel_dark']}
    gate_valve(G, Vm, C, PA, vz, rng, R_wheel=0.095, stem_dir=(1, 0, 0), yoke=0.025, spokes=4, own_flanges=False)
    green = mat('kennring_gruen', (0.15, 0.40, 0.20), 'paint', r=1.0)
    for z, m_ in ((1.58, green), (1.62, C['paper']), (1.66, green)):
        G.add(m_, cyl(rp + 0.0012, 0.04, 12, cap=False), along((xA, yA, z - 0.02), (xA, yA, z + 0.02)))
    rust_ring(G, C, PA, 0.0, rng, 0.09, 1, heavy=True)
    rust_ring(G, C, PA, 0.4285, rng, 0.05, -1)
    for _ in range(3):
        G.add(C['rust_stain'], orient(streak_uv(PA.S(0.0014), rng.uniform(0, TAU), vz - Lb2 - 0.03, 0.25, rng.uniform(0.12, 0.3),
                                                rng, 6, down=-1.0), PA.nrm))
    # DN50 gedaemmt
    xB, yB, rB, rc = 0.095, 0.10, 0.030, 0.062
    PB = Pipe((xB, yB, 0.0), (xB, yB, 2.0), rB)
    G.add(C['steel_dark'], cyl(rB, 0.37, 8, cap=False), along((xB, yB, 0.0), (xB, yB, 0.37)))
    G.add(C['steel_dark'], cyl(rB, 1.94 - 1.86, 8, cap=False), along((xB, yB, 1.86), (xB, yB, 1.94)))
    G.add(C['steel_dark'], torus(0.05, rB, 5, 8, 0.0, math.pi / 2), Matrix.Translation((xB - 0.05, yB, 1.94)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    G.add(C['steel_dark'], cyl(rB, xB - 0.05, 8, cap=False), along((xB - 0.05, yB, 1.99), (0.0, yB, 1.99)))
    rust_ring(G, C, PB, 0.0, rng, 0.12, 1, heavy=True)
    clad = mat('alu_mantel', (0.50, 0.51, 0.50), 'metal', r=1.2, m=0.55)
    wool = mat('mineralwolle', (0.50, 0.44, 0.30), 'fabric', r=1.5)
    Mb = lambda z0, z1: along((xB, yB, z0), (xB, yB, z1))  # noqa: E731
    G.add(clad, lathe([(rB + 0.0015, 0.0), (rc, 0.035)], 10), Mb(0.36, 1.0))
    G.add(clad, lathe([(rc, -0.035), (rB + 0.0015, 0.0)], 10), Mb(1.88, 3.0))
    for z0, z1 in ((0.395, 1.20), (1.18, 1.845)):
        G.add(clad, cyl(rc, z1 - z0, 10, cap=False), Mb(z0, z1))
        G.add(clad, cyl(rc + 0.0012, 0.03, 10, cap=False), Mb(z1 - 0.03, z1))
    # Rissstelle im Mantel: unregelmaessiges Loch (dunkler Rand) mit Mineralwolle, abstehender Blechlappen
    Pc = Pipe((xB, yB, 0.0), (xB, yB, 2.0), rc)
    u0, v0 = math.pi / 2 + 0.35, 1.44
    G.add(C['grime'], orient(blob_uv(Pc.S(0.0012), u0, v0, 0.62, 0.085, rng, 14, 2, 0.55), Pc.nrm))
    G.add(wool, orient(blob_uv(Pc.S(0.0022), u0, v0, 0.50, 0.068, rng, 14, 3, 0.55), Pc.nrm))
    G.add(clad, lathe([(rc + 0.002, 0.0), (rc + 0.002, 0.08)], 3, a0=u0 - 0.2, a1=u0 + 0.25), Mb(v0 + 0.06, v0 + 1) @ Matrix.Rotation(-0.30, 4, 'Y'))
    for z in (0.75, 1.70):
        G.add(C['steel'], cyl(rc + 0.0022, 0.016, 10, cap=False), Mb(z, z + 0.016))
    # Wandkonsolen
    for z in (0.80, 1.78):
        G.add(C['zinc'], strut_h(-0.27, 0.19, z - 0.0205))
        for y in (-0.24, 0.16):
            bolt(G, C['steel'], (0.021, y, z), (1, 0, 0), 0.007, washer=False)
        for (x, y, r) in ((xA, yA, rp), (xB, yB, rc)):
            G.add(C['zinc'], box((0.021, y - 0.0205, z - 0.0205), (x - r - 0.004, y + 0.0205, z + 0.0205)))
            ubolt(G, C['zinc'], (x, y, z), (0, 0, 1), r, (-1, 0, 0), leg=x - 0.03)
    for (x, y, r) in ((xA, yA, rp), (xB, yB, rB)):  # Bodenmanschetten
        G.add(C['zinc'], lathe([(r + 0.002, 0.0), (r + 0.03, 0.0), (r + 0.028, 0.006), (r + 0.002, 0.012)], 10),
              Matrix.Translation((x, y, 0.0)))
    for (y, z, r) in ((yA, 2.03, rp), (yB, 1.99, rB)):  # Wandrosetten am Eintritt
        G.add(C['zinc'], lathe([(r + 0.002, 0.0), (r + 0.03, 0.0), (r + 0.028, 0.006), (r + 0.002, 0.012)], 10), along((0.0, y, z), (1.0, y, z)))
        G.add(C['rust_stain'], orient(streak_uv(lambda u, v: (Vector((0.0015, u, v)), Vector((1, 0, 0))), y, z - r - 0.02, 0.05, rng.uniform(0.2, 0.5), rng, 6),
                                      lambda m: Vector((1, 0, 0))))
    G.add(C['grime'], blob((xA + 0.08, yA, 0.0005), (1, 0, 0), (0, 1, 0), 0.10, 0.08, rng, 12, 2, 0.5, off=0.0))
    G.build('steigleitung', angle=40.0)


# ---------------------------------------------------------------------------------------------------------------------
# Rohrbuendel (5-m-Module, Ende an Ende): gemeinsame Hauptleitungen mit halben Flanschverbindungen an den Modulgrenzen
# ---------------------------------------------------------------------------------------------------------------------
TRUNK = [  # (x, z, r_rohr, r_flansch, Schrauben, Lack)
    (0.150, 2.43, 0.0699, 0.125, 6, 'grau'),
    (0.105, 2.265, 0.0445, 0.100, 4, 'rot'),
    (0.085, 2.17, 0.0302, 0.0825, 4, 'gelb'),
]


def _bundle_mats():
    return {
        'grau': mat('rohr_grau', (0.38, 0.41, 0.40), 'paint', tex='painted_metal_grey', r=1.15),
        'rot': mat('rohr_rot', (0.46, 0.14, 0.10), 'paint', r=1.2),
        'gelb': mat('rohr_gelb', (0.60, 0.48, 0.16), 'paint', r=1.2),
        'guss': mat('rohr_guss', (0.16, 0.14, 0.12), 'paint', r=1.35),
    }


def _saddle_h(x, z, r):
    """Kabelschelle an der Wand fuer ein waagrechtes Kabel (Achse entlang y bei x, z)."""
    t, w = 0.001, 0.006
    arc = [Vector((x + (r + t) * math.cos(a), 0.0, z + (r + t) * math.sin(a))) for a in [-math.pi / 2 + math.pi * i / 3.0 for i in range(4)]]
    path = [Vector((0.0006, 0, z - r - 0.012)), Vector((0.0006, 0, z - r - t))] + arc + [Vector((0.0006, 0, z + r + t)), Vector((0.0006, 0, z + r + 0.012))]
    return sweep(path, [(-w, -t / 2), (w, -t / 2), (w, t / 2), (-w, t / 2)], up=(0, 1, 0))


def _bundle_trunk(G, C, PM, rng, skip=()):
    pipes = []
    for k, (x, z, r, rf, nb, pm) in enumerate(TRUNK):
        P = Pipe((x, -2.5, z), (x, 2.5, z), r)
        pipes.append(P)
        if k not in skip:
            G.add(PM[pm], cyl(r, 5.0, 14 if r > 0.05 else 12, cap=False), P.M)
        Fm = {'fl': PM[pm], 'bolt': C['steel_dark']}
        end_flange(G, Fm, P, True, r, rf, 0.022, nb, heads=True)
        end_flange(G, Fm, P, False, r, rf, 0.022, nb, heads=False)
        rust_ring(G, C, P, 0.056, rng, 0.05, 1)
        rust_ring(G, C, P, 4.944, rng, 0.05, -1)
        pipe_spots(G, C, P, rng, 2 + k)
    for j, (x, z) in enumerate(((0.016, 2.345), (0.040, 2.335))):  # Kabel in Durchhaengen
        rk = 0.0085 if j == 0 else 0.0065
        for s in range(4):
            y0, y1 = -2.5 + s * 1.25, -2.5 + (s + 1) * 1.25
            sag = 0.010 + 0.004 * ((s + j) % 2)
            pts = [Vector((x, y0 + (y1 - y0) * t / 4, z - sag * math.sin(math.pi * t / 4))) for t in range(5)]
            G.add(C['black'], tube(pts, rk, 5, caps=False))
        for y in (-1.25, 0.0, 1.25):
            G.add(C['zinc'], _saddle_h(x, z, rk), Matrix.Translation((0, y, 0)))
    for y in (-1.25, 1.25):  # Konsolen
        G.add(C['zinc'], strut_v(2.10, 2.57), Matrix.Translation((0.0, y, 0.0)))
        for z in (2.13, 2.54):
            bolt(G, C['steel'], (0.021, y, z), (1, 0, 0), 0.007)
        for (x, z, r, rf, nb, pm) in TRUNK:
            zt = z - r - 0.002
            G.add(C['zinc'], box((0.021, y - 0.0205, zt - 0.041), (x + r * 0.7, y + 0.0205, zt)))
            ubolt(G, C['zinc'], (x, y, z), (0, 1, 0), r, (0, 0, -1), leg=r + 0.05)
    return pipes


def _drip_wall(y, z0, rng):
    """Rostfahne auf der Wand (knapp vor x = 0) senkrecht nach unten."""
    S = lambda u, w: (Vector((0.0015, u, w)), Vector((1, 0, 0)))  # noqa: E731
    return orient(streak_uv(S, y, z0, 0.06, 0.8, rng, 8, down=-1.0, taper=0.6), lambda m: Vector((1, 0, 0)))


def build_pipe_bundle_a():
    """Rohrbuendel A (5 m, Wand bei x = 0): Hauptleitungen DN125 grau, DN80 rot, DN50 gelb mit halben Flanschverbindungen
    an den Modulenden (geschlossene Dichtflaechen), Konsolen alle 2,5 m, Kabel in Schellen; dazu Absperrschieber mit
    Ventilrad in der roten, Kompensator mit Zugankern und Manometer in der grauen Leitung; Rostuebergaenge, Laeufer.
    Alle Leitungen liegen ueber 2,13 m: die Karte setzt Schaltschraenke und Steigleitungen unter die Buendel."""
    rng = random.Random(61)
    C = M_common()
    PM = _bundle_mats()
    G = Geo()
    P1, P2, P3 = _bundle_trunk(G, C, PM, rng, skip=(0, 1))
    x1, z1 = P1.a.x, P1.a.z
    yk0, yk1 = -0.62, -0.38  # Kompensator (Faltenbalg) zwischen zwei Flanschen
    G.add(PM['grau'], cyl(P1.r, yk0 - 0.024 + 2.5, 14, cap=False), P1.M)
    G.add(PM['grau'], cyl(P1.r, 2.5 - yk1 - 0.024, 14, cap=False), along((x1, yk1 + 0.024, z1), (x1, 2.5, z1)))
    Fm1 = {'fl': PM['grau'], 'bolt': C['steel_dark']}
    flange(G, Fm1, (x1, yk0, z1), (0, 1, 0), P1.r, 0.125, 0.022, 4, side=1)
    flange(G, Fm1, (x1, yk1, z1), (0, -1, 0), P1.r, 0.125, 0.022, 4, side=1)
    prof = [(P1.r + (0.03 if i % 2 else 0.012), 0.004 + (yk1 - yk0 - 0.008) * i / 12) for i in range(13)]
    G.add(C['steel'], lathe(prof, 14), along((x1, yk0, z1), (x1, yk1, z1)))
    for a in (math.radians(45), math.radians(225)):
        d = Vector((math.cos(a), 0, math.sin(a))) * 0.108
        p0, p1 = Vector((x1, yk0 - 0.05, z1)) + d, Vector((x1, yk1 + 0.05, z1)) + d
        G.add(C['steel'], cyl(0.006, (p1 - p0).length, 6), along(p0, p1))
        for q, s in ((p0 + Vector((0, 0.02, 0)), -1), (p1 - Vector((0, 0.02, 0)), 1)):
            G.add(C['steel_dark'], nut_shape(0.01, 0.01), along(q, q + Vector((0, s, 0))))
    rust_ring(G, C, P1, yk0 + 2.5 - 0.03, rng, 0.06, -1)
    rust_ring(G, C, P1, yk1 + 2.5 + 0.03, rng, 0.05, 1)
    gp = Vector((x1 + P1.r - 0.004, 0.9, z1))  # Manometer auf der grauen Leitung
    G.add(PM['grau'], cyl(0.013, 0.03, 10), along(gp, gp + Vector((0.03, 0, 0))))
    gauge(G, C, gp + Vector((0.03, 0, 0)), (1, 0, 0), 0.05, rng, stem=0.035)
    yv = 0.55  # rote Leitung mit Absperrschieber
    Lb2 = max(0.16, P2.r * 3.0) / 2
    G.add(PM['rot'], cyl(P2.r, yv - Lb2 - 0.004 + 2.5, 14, cap=False), P2.M)
    G.add(PM['rot'], cyl(P2.r, 2.5 - (yv + Lb2 + 0.004), 14, cap=False), along((P2.a.x, yv + Lb2 + 0.004, P2.a.z), (P2.a.x, 2.5, P2.a.z)))
    Vm = {'body': mat('guss_schieber', (0.20, 0.22, 0.22), 'paint', r=1.3), 'fl': PM['rot'], 'bolt': C['steel_dark']}
    gate_valve(G, Vm, C, Pipe((P2.a.x, yv - 1, P2.a.z), (P2.a.x, yv + 1, P2.a.z), P2.r), 1.0, rng, R_wheel=0.075, stem_dir=(1, 0, 0), yoke=0.02, rf=0.1,
               own_flanges=False)
    G.build('rohrbuendel_a', angle=40.0)


def build_pipe_bundle_b():
    """Rohrbuendel B (5 m): dieselben Hauptleitungen wie A (Module passen in jeder Folge); die graue Leitung mit
    Alu-Ummantelung (Ueberlappungen, Spannbaender, Endkappen, aufgerissene Stelle mit Mineralwolle, Delle), die rote
    mit Reparaturschelle an einer Leckstelle und Rostfahne auf der Wand, die gelbe mit Kugelhahn."""
    rng = random.Random(62)
    C = M_common()
    PM = _bundle_mats()
    G = Geo()
    P1, P2, P3 = _bundle_trunk(G, C, PM, rng)
    clad = mat('alu_mantel', (0.50, 0.51, 0.50), 'metal', r=1.2, m=0.55)
    wool = mat('mineralwolle', (0.60, 0.50, 0.27), 'fabric', r=1.5)
    rc = 0.125
    x, z = P1.a.x, P1.a.z
    My = lambda y0, y1: along((x, y0, z), (x, y1, z))  # noqa: E731
    Gc = Geo()
    Gc.add(clad, lathe([(P1.r + 0.001, 0.0), (rc, 0.05)], 16), My(-2.36, 0.0))
    Gc.add(clad, lathe([(rc, -0.05), (P1.r + 0.001, 0.0)], 16), My(2.36, 3.0))
    tear = (0.55, 0.85)
    y = -2.31
    while y < 2.31 - 1e-6:
        y1 = min(y + 1.0, 2.31)
        Gc.add(clad, cyl(rc, y1 - y, 16, cap=False), My(y, y1))
        if y1 < 2.31 - 1e-6:
            Gc.add(clad, cyl(rc + 0.0013, 0.035, 16, cap=False), My(y1 - 0.035, y1))
        y = y1
    for yb in (-1.8, -0.8, 0.2, 1.4):
        Gc.add(C['steel'], cyl(rc + 0.0025, 0.018, 16, cap=False), My(yb, yb + 0.018))
    Pc = Pipe((x, -2.31, z), (x, 2.31, z), rc)
    ut, vt = math.pi * 1.15, 0.5 * (tear[0] + tear[1]) + 2.31  # Riss vorn unten: Loch mit Wolle, Blechlappen
    Gc.add(C['grime'], orient(blob_uv(Pc.S(0.0014), ut, vt, 0.75, 0.20, rng, 16, 2, 0.55), Pc.nrm))
    Gc.add(wool, orient(blob_uv(Pc.S(0.0026), ut, vt, 0.60, 0.16, rng, 16, 3, 0.55), Pc.nrm))
    Gc.add(clad, lathe([(rc + 0.002, 0.0), (rc + 0.002, 0.14)], 4, a0=ut + 0.2, a1=ut + 0.55), My(tear[1] - 0.02, tear[1] + 1) @ Matrix.Rotation(0.25, 4, 'X'))
    for _ in range(6):
        Gc.add(C['grime'], orient(blob_uv(Pc.S(0.0032), rng.uniform(-2.4, 0.6), rng.uniform(0.2, 4.4), rng.uniform(0.15, 0.35), rng.uniform(0.08, 0.3), rng, 10, 2, 0.5), Pc.nrm))
    for _ in range(4):
        Gc.add(C['dust'], orient(blob_uv(Pc.S(0.0036), rng.uniform(1.2, 1.9), rng.uniform(0.2, 4.4), 0.3, rng.uniform(0.2, 0.5), rng, 10, 2, 0.4), Pc.nrm))
    for k in range(3):
        Gc.add(C['dust'], orient(blob_uv(Pc.S(0.0040), math.pi / 2 + rng.uniform(-0.15, 0.15), 0.9 + k * 1.4, 0.35, 0.42, rng, 12, 2, 0.3), Pc.nrm))
    Gc.deform(dent_fn((x + rc * 0.7, -1.2, z - rc * 0.7), (-1, 0, 1), 0.07, 0.012))
    G.merge(Gc)
    yr = -0.7  # rote Leitung: Reparaturschelle, Rostfahne
    G.add(C['steel'], lathe([(P2.r + 0.003, 0.0), (P2.r + 0.006, 0.004), (P2.r + 0.006, 0.096), (P2.r + 0.003, 0.1)], 16), along((P2.a.x, yr, P2.a.z), (P2.a.x, yr + 1, P2.a.z)))
    for k in range(3):
        yy = yr + 0.02 + k * 0.03
        G.add(C['steel'], box((P2.a.x + P2.r, yy - 0.006, P2.a.z - 0.004), (P2.a.x + P2.r + 0.028, yy + 0.006, P2.a.z + 0.004)))
        G.add(C['steel_dark'], cyl(0.004, 0.05, 6), along((P2.a.x + P2.r + 0.022, yy, P2.a.z - 0.025), (P2.a.x + P2.r + 0.022, yy, P2.a.z + 0.025)))
        for s in (-1, 1):
            G.add(C['steel_dark'], nut_shape(0.007, 0.006), along((P2.a.x + P2.r + 0.022, yy, P2.a.z + s * 0.004), (P2.a.x + P2.r + 0.022, yy, P2.a.z + s * 1)))
    G.add(C['rust_stain'], orient(strip_uv(P2.S(0.0014), math.pi * 0.85, math.pi * 2.15, yr + 2.5 + 0.1, 0.12, rng, 12, 0.6), P2.nrm))
    pipe_spots(G, C, P2, rng, 4, v0=yr + 2.45, v1=yr + 2.7, smax=0.02)
    G.add(C['rust_stain'], _drip_wall(yr + 0.05, P2.a.z - 0.02, rng))
    yb = 1.6  # gelbe Leitung: Kugelhahn mit Hebel
    G.add(C['brass'], lathe([(P3.r + 0.002, 0.0), (P3.r + 0.012, 0.01), (P3.r + 0.022, 0.035), (P3.r + 0.022, 0.065), (P3.r + 0.012, 0.09), (P3.r + 0.002, 0.1)], 14),
          along((P3.a.x, yb - 0.05, P3.a.z), (P3.a.x, yb + 0.05, P3.a.z)))
    for s in (-1, 1):
        G.add(C['brass'], nut_shape(P3.r + 0.018, 0.022), along((P3.a.x, yb + s * 0.06, P3.a.z), (P3.a.x, yb + s * 1, P3.a.z)))
    G.add(C['steel'], cyl(0.007, 0.03, 8), along((P3.a.x, yb, P3.a.z + P3.r + 0.015), (P3.a.x, yb, P3.a.z + P3.r + 0.045)))
    lever = mat('hebel_gelb', (0.80, 0.62, 0.10), 'plastic', r=0.9)
    G.add(lever, box((-0.012, -0.007, 0.0), (0.16, 0.007, 0.012), bev=0.003), Matrix.Translation((P3.a.x, yb, P3.a.z + P3.r + 0.045)) @ Matrix.Rotation(0.15, 4, 'Z'))
    G.build('rohrbuendel_b', angle=40.0)


# =====================================================================================================================
# Maschinen an der Wand: Pumpe (Normpumpe mit Motor auf Grundrahmen) und stehender Druckbehaelter
# =====================================================================================================================
PXZ = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))  # Umriss in (x, z), gezogen entlang -y


def _volute(axis_c, r_hub, rng, seg=18, prs=8):
    """Spiralgehaeuse einer Kreiselpumpe (Achse entlang y durch axis_c): Querschnitt waechst von oben (theta = 90 Grad)
    gegen den Uhrzeigersinn (Blick von +y) bis zur Seite +x, dort geht der Druckstutzen senkrecht nach oben."""
    c = Vector(axis_c)
    vs, fs = [], []
    n = seg
    for j in range(n + 1):
        t = j / n
        th = math.radians(90 + 270 * t)
        Rc = r_hub + 0.02 + 0.055 * t
        rt = 0.030 + 0.034 * t
        e = Vector((math.cos(th), 0.0, math.sin(th)))
        cc = c + e * Rc
        for k in range(prs):
            a = TAU * k / prs
            vs.append(cc + e * (rt * math.cos(a)) + Vector((0, 1, 0)) * (rt * 1.15 * math.sin(a)))
    for j in range(n):
        for k in range(prs):
            k1 = (k + 1) % prs
            fs.append((j * prs + k, j * prs + k1, (j + 1) * prs + k1, (j + 1) * prs + k))
    fs.append(tuple(range(prs))[::-1])
    data = orient((vs, fs), lambda m: m - (c + Vector(((m - c).x, 0, (m - c).z)).normalized() * (r_hub + 0.02 + 0.055 * 0.5)))
    end = c + Vector((1, 0, 0)) * (r_hub + 0.075)
    return data, end, 0.064


def build_mach_pump():
    """Liegende Normpumpe mit Drehstrommotor auf geschweisstem Grundrahmen (Wand bei x = 0): Motor mit Laengsrippen,
    Lagerschilden, Lueferhaube mit Gitter, Klemmenkasten mit Kabelverschraubung und Schutzschlauch zur Wand, Kranoese,
    Typenschild; Kupplungsschutz (gelocht), Lagertraeger mit Oelstandsauge, Spiralgehaeuse, Saugstutzen mit Bogen in
    die Wand, Druckleitung mit Rueckschlagklappe, Absperrschieber mit Ventilrad und Bogen in die Wand, zwei Manometer;
    Ankerschrauben, Oellache, Rost, Abplatzer."""
    rng = random.Random(71)
    C = M_common()
    G = Geo()
    motor = mat('motor_lack', (0.30, 0.37, 0.34), 'paint', tex='painted_metal_grey', r=1.15)
    casing = mat('pumpe_lack', (0.20, 0.26, 0.34), 'paint', r=1.2)
    base = mat('grundrahmen', (0.17, 0.18, 0.17), 'paint', r=1.25)
    pipe = mat('druckrohr', (0.42, 0.15, 0.10), 'paint', r=1.2)
    ax_x, ax_z = 0.45, 0.40
    # Grundrahmen: zwei U-Profile laengs (Oeffnung nach aussen), Quertraeger, Deckblech, Ankerschrauben
    for x0, s in ((0.10, -1), (0.70, 1)):
        U = [(0, 0), (0.08, 0), (0.08, 0.008), (0.008, 0.008), (0.008, 0.112), (0.08, 0.112), (0.08, 0.12), (0, 0.12)]
        if s < 0:
            U = [(0.08 - x, z) for x, z in U]
        G.add(base, prism([(x0 + x, z) for x, z in U], -0.74, 0.74), PXZ)
    for y in (-0.70, 0.0, 0.70):
        G.add(base, box((0.18, y - 0.03, 0.0), (0.70, y + 0.03, 0.11), bev=0.003))
    G.add(base, box((0.10, -0.745, 0.12), (0.78, 0.745, 0.128), bev=0.003))
    for x in (0.14, 0.74):
        for y in (-0.66, 0.66):
            G.add(C['steel_dark'], box((x - 0.03, y - 0.04, 0.0), (x + 0.03, y + 0.04, 0.008)), Matrix.Translation((0, 0, 0)))
            bolt(G, C['steel_dark'], (x, y, 0.128), (0, 0, 1), 0.011, stub=0.02)
    # Motor: Rippenkoerper (Laengsrippen), Fuesse, Lagerschilde, Luefterhaube mit Gitter
    star = []
    nf = 22
    for i in range(nf):
        a = TAU * i / nf
        for da, rr in ((-0.05, 0.128), (-0.022, 0.156), (0.022, 0.156), (0.05, 0.128)):
            star.append((math.cos(a + da) * rr, math.sin(a + da) * rr))
    G.add(motor, prism(star, 0.0, 0.40), along((ax_x, -0.61, ax_z), (ax_x, -0.21, ax_z), up=(0, 0, 1)))
    for y0, y1 in ((-0.64, -0.61), (-0.21, -0.18)):
        G.add(motor, lathe([(0, 0), (0.15, 0), (0.158, 0.008), (0.158, y1 - y0 - 0.006), (0.12, y1 - y0), (0, y1 - y0)], 20), along((ax_x, y0, ax_z), (ax_x, y1, ax_z)))
        for k in range(2):
            a = TAU * (k + 0.25) / 2
            bolt(G, C['steel_dark'], (ax_x + math.cos(a) * 0.14, y1 if y1 > -0.3 else y0, ax_z + math.sin(a) * 0.14), (0, 1 if y1 > -0.3 else -1, 0), 0.007, washer=False)
    G.add(motor, lathe([(0, 0), (0.162, 0), (0.168, 0.01), (0.168, 0.10), (0.16, 0.112), (0, 0.112)], 20), along((ax_x, -0.64, ax_z), (ax_x, -0.752, ax_z)))
    for k, rr in enumerate((0.03, 0.06, 0.09, 0.12, 0.145)):
        G.add(C['hole'], lathe([(rr + 0.006, 0.0), (rr - 0.006, 0.0)], 20), along((ax_x, -0.7525, ax_z), (ax_x, -0.80, ax_z)))
    for y in (-0.56, -0.26):
        G.add(motor, box((ax_x - 0.15, y - 0.04, 0.128), (ax_x + 0.15, y + 0.04, 0.165), bev=0.004))
        G.add(motor, box((ax_x - 0.12, y - 0.03, 0.165), (ax_x + 0.12, y + 0.03, ax_z - 0.10), bev=0.004))
        for sx in (-1, 1):
            bolt(G, C['steel_dark'], (ax_x + sx * 0.12, y, 0.165), (0, 0, 1), 0.009, stub=0.008, washer=False)
    # Klemmenkasten oben mit Deckel, Schrauben, Verschraubung, Schutzschlauch zur Wand
    G.add(motor, box((ax_x - 0.075, -0.48, ax_z + 0.15), (ax_x + 0.075, -0.33, ax_z + 0.24), bev=0.006))
    G.add(motor, box((ax_x - 0.08, -0.485, ax_z + 0.24), (ax_x + 0.08, -0.325, ax_z + 0.255), bev=0.004))
    for sx in (-1, 1):
        for sy in (-1, 1):
            screw_head(G, C['steel'], (ax_x + sx * 0.065, -0.405 + sy * 0.07, ax_z + 0.255), (0, 0, 1), 0.005, C['black'])
    gl = Vector((ax_x - 0.075, -0.405, ax_z + 0.19))
    G.add(C['black'], nut_shape(0.016, 0.012), along(gl, gl + Vector((-1, 0, 0))))
    G.add(C['black'], lathe([(0, 0), (0.014, 0), (0.012, 0.02), (0, 0.02)], 8), along(gl + Vector((-0.012, 0, 0)), gl + Vector((-1, 0, 0))))
    hose = [gl + Vector((-0.03, 0, 0)), Vector((0.22, -0.405, ax_z + 0.20)), Vector((0.10, -0.405, ax_z + 0.40)), Vector((0.0, -0.405, ax_z + 0.52))]
    hp = fillet(hose, 0.08, 4)
    G.add(mat('wellschlauch', (0.10, 0.10, 0.11), 'plastic', r=0.9), tube(hp, 0.011, 8))
    for i in range(1, len(hp) - 1, 4):
        G.add(mat('wellschlauch', (0.10, 0.10, 0.11), 'plastic', r=0.9), torus(0.0115, 0.002, 6, 3), along(hp[i], hp[i] + (hp[i + 1] - hp[i - 1]).normalized()))
    G.add(C['steel_dark'], torus(0.03, 0.007, 12, 5), mt((ax_x, -0.22, ax_z + 0.19), (0, 0, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    _plate_lines(G, C, FX((ax_x + 0.157, -0.46, ax_z)), 0.10, 0.055, rng, 5)
    # Welle, Kupplung unter gelochtem Schutz
    G.add(C['steel'], cyl(0.022, 0.08, 10), along((ax_x, -0.18, ax_z), (ax_x, -0.10, ax_z)))
    G.add(C['steel_dark'], lathe([(0, 0), (0.06, 0), (0.06, 0.06), (0, 0.06)], 14), along((ax_x, -0.13, ax_z), (ax_x, -0.07, ax_z)))
    guard = mat('schutzblech', (0.70, 0.56, 0.10), 'paint', r=1.1)
    G.add(guard, lathe([(0.105, 0.0), (0.105, 0.16)], 12, a0=math.radians(-40), a1=math.radians(220)), along((ax_x, -0.185, ax_z), (ax_x, -0.025, ax_z), up=(1, 0, 0)))
    for y in (-0.185, -0.025):
        G.add(guard, box((ax_x - 0.11, y - 0.003, 0.128), (ax_x - 0.095, y + 0.003, ax_z)), Matrix.Translation((0, 0, 0)))
        G.add(guard, box((ax_x + 0.095, y - 0.003, 0.128), (ax_x + 0.11, y + 0.003, ax_z)))
    Sg = (lambda u, v: (Vector((ax_x + 0.1065 * math.cos(u), -0.185 + v, ax_z + 0.1065 * math.sin(u))), Vector((math.cos(u), 0, math.sin(u)))))
    for i in range(4):
        for j in range(3):
            u, v = math.radians(5 + i * 50), 0.035 + j * 0.045
            G.add(C['hole'], orient(blob_uv(Sg, u, v, 0.09, 0.009, rng, 4, 1, 0.0), lambda m: Vector((m.x - ax_x, 0, m.z - ax_z))))
    # Lagertraeger mit Fenstern und Oelstandsauge
    G.add(casing, lathe([(0, 0), (0.085, 0), (0.09, 0.02), (0.12, 0.11), (0.14, 0.13), (0, 0.13)], 18), along((ax_x, -0.03, ax_z), (ax_x, 0.10, ax_z)))
    for a in (math.radians(20), math.radians(160)):
        q = Vector((ax_x + math.cos(a) * 0.105, 0.03, ax_z + math.sin(a) * 0.105))
        G.add(C['hole'], blob(q, (0, 1, 0), Vector((-math.sin(a), 0, math.cos(a))), 0.03, 0.02, rng, 8, 1, 0.05, off=0.003))
    G.add(C['brass'], lathe([(0, 0), (0.016, 0), (0.016, 0.008), (0, 0.008)], 10), along((ax_x + 0.11, 0.06, ax_z - 0.05), (ax_x + 0.2, 0.06, ax_z - 0.05)))
    G.add(mat('oelauge', (0.55, 0.40, 0.10), 'glass'), dome(0.011, 0.006, 8), along((ax_x + 0.118, 0.06, ax_z - 0.05), (ax_x + 0.2, 0.06, ax_z - 0.05)))
    # Spiralgehaeuse (Ruecken + Spirale + Deckel), Gehaeusefuss, Saugstutzen axial
    G.add(casing, lathe([(0, 0), (0.16, 0), (0.175, 0.02), (0.175, 0.11), (0.16, 0.13), (0, 0.13)], 22), along((ax_x, 0.10, ax_z), (ax_x, 0.23, ax_z)))
    vdata, vend, vr = _volute((ax_x, 0.165, ax_z), 0.14, rng)
    G.add(casing, vdata)
    for k in range(6):
        a = TAU * (k + 0.5) / 6
        bolt(G, C['steel_dark'], (ax_x + math.cos(a) * 0.165, 0.23, ax_z + math.sin(a) * 0.165), (0, 1, 0), 0.007, washer=False)
    G.add(casing, box((ax_x - 0.12, 0.12, 0.128), (ax_x + 0.12, 0.21, ax_z - 0.16), bev=0.006))
    G.add(casing, lathe([(0, 0), (0.075, 0), (0.07, 0.07), (0, 0.07)], 16), along((ax_x, 0.23, ax_z), (ax_x, 0.30, ax_z)))
    # Druckstutzen: senkrecht ab Spiralende
    dx = vend.x
    G.add(casing, cyl(vr, 0.16, 14, cap=False), along((dx, 0.165, ax_z), (dx, 0.165, ax_z + 0.16)))
    Fc = {'fl': casing, 'bolt': C['steel_dark']}
    Fp = {'fl': pipe, 'bolt': C['steel_dark']}
    flange(G, Fc, (dx, 0.165, ax_z + 0.18), (0, 0, 1), vr, 0.11, 0.02, 0, side=0)
    # Saugseite: Flansch, Rohr mit Bogen in die Wand, Wandmanschette
    flange(G, Fc, (ax_x, 0.32, ax_z), (0, 1, 0), 0.07, 0.125, 0.02, 0, side=0)
    flange(G, Fp, (ax_x, 0.322, ax_z), (0, -1, 0), 0.07, 0.125, 0.022, 8, side=-1)
    sp = fillet([(ax_x, 0.33, ax_z), (ax_x, 0.60, ax_z), (0.0, 0.60, ax_z)], 0.15, 6)
    G.add(pipe, sweep(sp, circ(0.07, 16), caps=False))
    G.add(C['zinc'], lathe([(0.072, 0.0), (0.11, 0.0), (0.11, 0.012), (0.072, 0.02)], 16), along((0.0, 0.60, ax_z), (1.0, 0.60, ax_z)))
    # Druckleitung: Rueckschlagklappe, Schieber mit Ventilrad (schraeg zum Raum), Bogen in die Wand
    zf = ax_z + 0.18
    flange(G, Fp, (dx, 0.165, zf + 0.002), (0, 0, -1), 0.057, 0.11, 0.022, 4, side=-1)
    G.add(pipe, cyl(0.057, 0.05, 14, cap=False), along((dx, 0.165, zf + 0.002), (dx, 0.165, zf + 0.06)))
    G.add(casing, lathe([(0.057, 0.0), (0.09, 0.03), (0.095, 0.09), (0.07, 0.13), (0.057, 0.14)], 14), along((dx, 0.165, zf + 0.06), (dx, 0.165, zf + 0.20)))
    G.add(C['steel'], cyl(0.012, 0.03, 8), along((dx + 0.09, 0.165, zf + 0.12), (dx + 0.12, 0.165, zf + 0.12)))
    G.add(pipe, cyl(0.057, 0.12, 14, cap=False), along((dx, 0.165, zf + 0.20), (dx, 0.165, zf + 0.32)))
    PV = Pipe((dx, 0.165, zf + 0.32), (dx, 0.165, zf + 0.52), 0.057)
    sd = Vector((0.85, 0.53, 0.0)).normalized()
    Vm = {'body': mat('guss_schieber', (0.20, 0.22, 0.22), 'paint', r=1.3), 'fl': pipe, 'bolt': C['steel_dark']}
    gate_valve(G, Vm, C, PV, 0.10, rng, R_wheel=0.085, stem_dir=sd, yoke=0.02)
    top = zf + 0.42 + 0.09
    dp = fillet([(dx, 0.165, top), (dx, 0.165, 1.60), (0.0, 0.165, 1.60)], 0.12, 6)
    G.add(pipe, sweep(dp, circ(0.057, 14), caps=False))
    G.add(C['zinc'], lathe([(0.059, 0.0), (0.095, 0.0), (0.095, 0.012), (0.059, 0.02)], 14), along((0.0, 0.165, 1.60), (1.0, 0.165, 1.60)))
    gauge(G, C, (dx + 0.057, 0.165, zf + 0.29), (1, 0, 0), 0.045, rng, stem=0.035)
    Pd = Pipe((dx, 0.165, top), (dx, 0.165, 1.48), 0.057)
    rust_ring(G, C, Pd, 0.0, rng, 0.05, 1)
    Ps = Pipe((ax_x, 0.33, ax_z), (ax_x, 0.47, ax_z), 0.07)
    rust_ring(G, C, Ps, 0.0, rng, 0.05, 1)
    # Gebrauchsspuren: Oellache unter dem Lagertraeger, Rost am Deckblech, Laeufer am Gehaeuse, Abplatzer
    G.add(C['oil'], blob((ax_x + 0.02, 0.02, 0.1285), (1, 0, 0), (0, 1, 0), 0.14, 0.09, rng, 16, 2, 0.5, off=0.0006))
    G.add(C['oil'], blob((ax_x + 0.20, 0.10, 0.0006), (1, 0, 0), (0, 1, 0), 0.10, 0.18, rng, 14, 2, 0.5, off=0.0))
    for y0, y1 in ((-0.74, -0.2), (0.25, 0.74)):
        G.add(C['rust'], orient(strip_uv(lambda u, v: (Vector((0.775 - v, u, 0.1287)), Vector((0, 0, 1))), y0, y1, 0.0, 0.04, rng, 12, 0.8),
                                lambda m: Vector((0, 0, 1))))
    Sc = lambda u, v: (Vector((ax_x + 0.1765 * math.cos(u), 0.10 + v, ax_z + 0.1765 * math.sin(u))), Vector((math.cos(u), 0, math.sin(u))))  # noqa: E731
    for _ in range(4):
        G.add(C['rust_stain'], orient(blob_uv(Sc, rng.uniform(-1.4, -0.2), rng.uniform(0.02, 0.11), 0.12, 0.012, rng, 8, 1, 0.5),
                                      lambda m: Vector((m.x - ax_x, 0, m.z - ax_z))))
    G.build('pumpe', angle=40.0)


def build_mach_tank():
    """Stehender Druckbehaelter (D 0,9 m) auf drei Rohrstuetzen vor der Wand: Mantel mit Rund- und Laengsnaehten,
    Kloepperboeden, Handloch mit verschraubtem Deckel und Griff, Stutzen oben mit Flansch und Bogen in die Wand,
    Sicherheitsventil mit Hebel, seitlicher Absperrschieber mit Ventilrad, Manometer auf Wassersackrohr,
    Ablasshahn, Typenschild auf Konsole, Hebeoesen; Rostlaeufer, Abplatzer, Kondenswasserrand."""
    rng = random.Random(81)
    C = M_common()
    G = Geo()
    shell = mat('kessel_lack', (0.42, 0.50, 0.44), 'paint', tex='painted_metal_grey', r=1.15)
    weld = mat('schweissnaht', (0.32, 0.36, 0.32), 'paint', r=1.3)
    cx, R = 0.55, 0.45
    z0, z1 = 0.42, 1.95
    c = Vector((cx, 0.0, 0.0))
    nrm = lambda m: Vector((m.x - cx, m.y, 0))  # noqa: E731
    # Mantel + Kloepperboeden als ein Drehkoerper
    prof = [(0.0, 0.255), (0.20, 0.275), (0.33, 0.315), (0.40, 0.355), (0.435, 0.385), (R, z0)]
    prof += [(R, z) for z in (0.82, 1.18, 1.54)]
    prof += [(R, z1), (0.435, 1.985), (0.40, 2.015), (0.33, 2.055), (0.20, 2.095), (0.0, 2.115)]
    G.add(shell, lathe(prof, 26), Matrix.Translation(c))
    for z in (z0, 1.18, z1):  # Rundnaehte
        G.add(weld, torus(R + 0.001, 0.0045, 20, 3), Matrix.Translation(c + Vector((0, 0, z))))
    a_l = math.radians(-50)
    G.add(weld, cyl(0.0045, z1 - z0, 4), along(c + Vector((math.cos(a_l) * (R + 0.001), math.sin(a_l) * (R + 0.001), z0)),
                                              c + Vector((math.cos(a_l) * (R + 0.001), math.sin(a_l) * (R + 0.001), z1))))
    # Stuetzen (Rohr mit Fussplatte und Ankerschrauben, Verstaerkungsblech)
    for a in (math.radians(60), math.radians(180), math.radians(300)):
        p = c + Vector((math.cos(a) * 0.30, math.sin(a) * 0.30, 0.0))
        G.add(C['steel_dark'], cyl(0.038, 0.33, 12, cap=False), along(p, p + Vector((0, 0, 0.33))))
        G.add(C['steel_dark'], box((p.x - 0.07, p.y - 0.07, 0.0), (p.x + 0.07, p.y + 0.07, 0.012), bev=0.002))
        for k in range(1):
            q = p + Vector((math.cos(a) * 0.05, math.sin(a) * 0.05, 0.012))
            bolt(G, C['steel'], q, (0, 0, 1), 0.009, stub=0.012, washer=False)
        G.add(shell, box((-0.06, -0.05, -0.004), (0.06, 0.05, 0.004)), Matrix.Translation(p + Vector((0, 0, 0.335))) @ Matrix.Rotation(a, 4, 'Z') @ Matrix.Rotation(-0.35, 4, 'Y'))
        G.add(C['rust'], orient(strip_uv(Pipe(p, p + Vector((0, 0, 0.33)), 0.038).S(0.0012), 0, TAU, 0.0, 0.06, rng, 10, 0.8),
                                Pipe(p, p + Vector((0, 0, 0.33)), 0.038).nrm))
    # Handloch vorn (Richtung +x, leicht seitlich) mit Deckel, Schrauben, Griff
    ah = math.radians(15)
    dh = Vector((math.cos(ah), math.sin(ah), 0.0))
    ph = c + Vector((0, 0, 0.82)) + dh * (R - 0.02)
    G.add(shell, cyl(0.12, 0.06, 18, cap=False), along(ph, ph + dh * 0.06))
    Fh = {'fl': shell, 'bolt': C['steel_dark']}
    flange(G, Fh, ph + dh * 0.082, dh, 0.12, 0.17, 0.022, 0, side=0, hub=False, seg=18)
    flange(G, Fh, ph + dh * 0.084, -dh, 0.12, 0.17, 0.024, 6, side=-1, blind=True, hub=False, seg=18)
    G.add(C['steel'], tube([ph + dh * 0.108 + Vector((0, 0, -0.05)), ph + dh * 0.145 + Vector((0, 0, -0.04)), ph + dh * 0.145 + Vector((0, 0, 0.04)),
                            ph + dh * 0.108 + Vector((0, 0, 0.05))], 0.008, 6, bend=0.02))
    # Stutzen oben mit Flanschverbindung und Bogen in die Wand
    top = c + Vector((0, 0, 2.115))
    G.add(shell, cyl(0.06, 0.12, 14, cap=False), along(top - Vector((0, 0, 0.02)), top + Vector((0, 0, 0.12))))
    Fs = {'fl': shell, 'bolt': C['steel_dark']}
    pipe = mat('kessel_rohr', (0.36, 0.40, 0.38), 'paint', r=1.2)
    Fp = {'fl': pipe, 'bolt': C['steel_dark']}
    flange(G, Fs, top + Vector((0, 0, 0.14)), (0, 0, 1), 0.06, 0.11, 0.02, 0, side=0)
    flange(G, Fp, top + Vector((0, 0, 0.142)), (0, 0, -1), 0.06, 0.11, 0.022, 4, side=-1)
    tp = fillet([top + Vector((0, 0, 0.14)), Vector((cx, 0.0, 2.45)), Vector((0.0, 0.0, 2.45))], 0.12, 6)
    G.add(pipe, sweep(tp, circ(0.06, 14), caps=False))
    G.add(C['zinc'], lathe([(0.062, 0.0), (0.1, 0.0), (0.1, 0.012), (0.062, 0.02)], 14), along((0.0, 0.0, 2.45), (1.0, 0.0, 2.45)))
    # Sicherheitsventil (Federbelastet, Hebel) und Hebeoesen auf dem Boden
    sv = c + Vector((-0.12, 0.20, 2.08))
    G.add(C['brass'], lathe([(0, 0), (0.035, 0), (0.04, 0.03), (0.03, 0.06), (0.025, 0.06), (0.022, 0.16), (0.03, 0.17), (0, 0.18)], 12), along(sv, sv + Vector((0, 0, 1))))
    G.add(C['steel'], box((-0.004, -0.004, 0.0), (0.12, 0.004, 0.008)), Matrix.Translation(sv + Vector((0, 0, 0.165))) @ Matrix.Rotation(0.6, 4, 'Z') @ Matrix.Rotation(-0.12, 4, 'Y'))
    for a in (math.radians(100), math.radians(280)):
        q = c + Vector((math.cos(a) * 0.27, math.sin(a) * 0.27, 2.075))
        G.add(shell, prism([(-0.04, 0.0), (0.04, 0.0), (0.03, 0.06), (-0.03, 0.06)], -0.006, 0.006), Matrix.Translation(q) @ Matrix.Rotation(a + math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    # Seitlicher Absperrschieber (Richtung -y) mit Ventilrad
    ps = c + Vector((0, -R + 0.01, 1.10))
    PV = Pipe(ps + Vector((0, -0.04, 0)), ps + Vector((0, -0.24, 0)), 0.045)
    G.add(shell, cyl(0.045, 0.04 + 0.018, 12, cap=False), along(ps, ps + Vector((0, -0.058, 0))))
    Vm = {'body': mat('guss_schieber', (0.20, 0.22, 0.22), 'paint', r=1.3), 'fl': shell, 'bolt': C['steel_dark']}
    gate_valve(G, Vm, C, PV, 0.1, rng, R_wheel=0.075, stem_dir=(0.6, 0, 0.8), yoke=0.02, rf=0.1)
    Pout = Pipe(PV.a + Vector((0, -0.2, 0)), PV.a + Vector((0, -0.24, 0)), 0.045)
    G.add(shell, cyl(0.045, 0.04, 12, cap=False), Pout.M)
    end_flange(G, {'fl': shell, 'bolt': C['steel_dark']}, Pout, True, 0.045, 0.1, 0.02, 4, heads=False)
    # Manometer auf Wassersackrohr (Schlinge), vorn rechts
    am = math.radians(-35)
    dm = Vector((math.cos(am), math.sin(am), 0))
    pm = c + dm * R + Vector((0, 0, 1.52))
    loop = [pm, pm + dm * 0.05]
    for i in range(9):
        a = TAU * i / 8
        loop.append(pm + dm * (0.08 + 0.025 * math.sin(a)) + Vector((0, 0, 0.03 - 0.03 * math.cos(a))) + Vector((-dm.y, dm.x, 0)) * 0.012 * i / 8)
    loop.append(loop[-1] + Vector((0, 0, 0.05)))
    G.add(C['brass'], tube(loop, 0.005, 6))
    gauge(G, C, loop[-1], dm, 0.05, rng, stem=0.03)
    # Ablasshahn unten, Typenschild auf Konsole
    pd = c + Vector((0.0, 0.0, 0.255))
    G.add(C['steel_dark'], cyl(0.015, 0.08, 8), along(pd, pd - Vector((0, 0, 0.08))))
    G.add(C['brass'], lathe([(0, 0), (0.02, 0), (0.022, 0.02), (0.02, 0.04), (0, 0.04)], 8), along(pd - Vector((0, 0, 0.08)), pd - Vector((0, 0, 0.12))))
    G.add(C['red'], box((0.0, -0.005, -0.004), (0.09, 0.005, 0.004), bev=0.002), Matrix.Translation(pd - Vector((0, 0, 0.1))) @ Matrix.Rotation(0.4, 4, 'Z'))
    an = math.radians(40)
    dn = Vector((math.cos(an), math.sin(an), 0))
    pn = c + dn * (R + 0.04) + Vector((0, 0, 1.40))
    G.add(shell, box((-0.04, -0.005, -0.03), (0.0, 0.005, 0.03)), Matrix.Translation(pn) @ Matrix.Rotation(an, 4, 'Z'))
    _plate_lines(G, C, Matrix.Translation(pn + dn * 0.001) @ Matrix.Rotation(an, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'Y') @ Matrix.Rotation(math.pi / 2, 4, 'Z'),
                 0.16, 0.10, rng, 6)
    # Gebrauchsspuren: Rostlaeufer unter Stutzen/Naehten, Abplatzer, Kondenswasser unten
    S = cyl_S(lambda z: R, 0.0014, Matrix.Translation(c))
    for _ in range(9):
        a = rng.uniform(-math.pi * 0.6, math.pi * 0.6)
        G.add(C['rust_stain'], orient(streak_uv(S, a, rng.choice((1.17, 1.93, 1.08, 0.8)), 0.03 / R * 0.8, rng.uniform(0.15, 0.5), rng, 7), lambda m: nrm(m)))
    for _ in range(8):
        a = rng.uniform(-math.pi * 0.7, math.pi * 0.7)
        G.add(C['rust'], orient(blob_uv(S, a, rng.uniform(0.45, 1.9), rng.uniform(0.02, 0.06), rng.uniform(0.02, 0.05), rng, 10, 2, 0.5), lambda m: nrm(m)))
    G.add(C['rust_stain'], orient(strip_uv(cyl_S(lambda z: R, 0.0035, Matrix.Translation(c)), -math.pi, math.pi, z0, 0.10, rng, 60, 0.7), lambda m: nrm(m)))
    G.add(C['grime'], blob((cx, 0.0, 0.0006), (1, 0, 0), (0, 1, 0), 0.3, 0.25, rng, 16, 2, 0.4, off=0.0))
    G.build('kessel', angle=40.0)


# =====================================================================================================================
# Verschlossene Tueren (Wandteile, Wandflaeche x = 0) und die offene Tuer nach Level 3
# =====================================================================================================================
DW, DH, FW = 0.92, 2.05, 0.05   # lichte Breite, Hoehe, Zargenspiegel


def _frame_profile(fw=FW, depth=0.062):
    """Stahl-Eckzarge im Querschnitt (Tiefe n, quer b; +b zur Oeffnung): Spiegel vorn, Falz mit Anschlag hinten."""
    return [(0.0, -fw / 2), (0.0, fw / 2 + 0.014), (0.022, fw / 2 + 0.014), (0.022, fw / 2), (depth - 0.004, fw / 2),
            (depth, fw / 2 - 0.004), (depth, -fw / 2 + 0.004), (depth - 0.004, -fw / 2)]


def _frame(G, matl, x0=0.0, W=DW, H=DH, fw=FW, prof=None):
    """Zarge als gezogenes Profil (Gehrung an den Ecken), Wand bei x = x0, Oeffnung y +-W/2, Hoehe H."""
    yo, zt = W / 2 + fw / 2, H + fw / 2
    path = [(x0, -yo, 0.0), (x0, -yo, zt), (x0, yo, zt), (x0, yo, 0.0)]
    G.add(matl, sweep(path, prof or _frame_profile(fw), caps=True, up=(1, 0, 0)))


def _emboss(G, matl, x, y0, y1, z0, z1, h=0.004, s=0.012, dirx=1):
    """Gepraegtes Feld (Kassette) auf einer Flaeche bei x (Normale dirx * x): Pyramidenstumpf, Hoehe h, Schraege s."""
    b = [Vector((x, y0, z0)), Vector((x, y1, z0)), Vector((x, y1, z1)), Vector((x, y0, z1))]
    xt = x + dirx * h
    t = [Vector((xt, y0 + s, z0 + s)), Vector((xt, y1 - s, z0 + s)), Vector((xt, y1 - s, z1 - s)), Vector((xt, y0 + s, z1 - s))]
    fs = [(4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    G.add(matl, orient((b + t, fs), lambda m: Vector((dirx, 0, 0))))


def _door_relief(y, z, h=0.004, s=0.012):
    """Hoehe der Blattoberflaeche ueber der Grundflaeche an (y, z) wegen der gepraegten Kassetten der Stahltueren."""
    y0, y1 = -DW / 2 + 0.003, DW / 2 - 0.003
    best = 0.0
    for (ya, yb) in ((y0 + 0.09, -0.02), (0.02, y1 - 0.09)):
        for (za, zb) in ((0.20, 0.95), (1.18, 1.93)):
            d = min(y - ya, yb - y, z - za, zb - z)
            if d > 0:
                best = max(best, h * min(1.0, d / s))
    return best


def _lever_set(G, C, x, y, z, rng, finish='alu', side=-1):
    """Drueckergarnitur mit Langschild: Schild (abgerundet), Druecker (Hals + Griff, Ende gerundet), Profilzylinder."""
    fin = {'alu': mat('beschlag_alu', (0.62, 0.62, 0.60), 'metal', r=0.9, m=0.7),
           'schwarz': mat('beschlag_schwarz', (0.05, 0.05, 0.05), 'plastic', r=0.9),
           'messing': mat('beschlag_messing', (0.50, 0.40, 0.22), 'metal', r=1.0, m=0.7)}[finish]
    G.add(fin, prism(rrect(0.042, 0.262, 0.012, 3), 0.0, 0.007), FX((x, y, z - 0.045)))
    p0 = Vector((x + 0.007, y, z))
    G.add(fin, lathe([(0, 0), (0.011, 0), (0.0095, 0.05), (0, 0.05)], 10), along(p0, p0 + Vector((0.05, 0, 0))))
    path = [p0 + Vector((0.042, 0, 0)), p0 + Vector((0.058, side * 0.03, -0.004)), p0 + Vector((0.058, side * 0.125, -0.012))]
    G.add(fin, tube(path, 0.0095, 8, bend=0.025, bseg=3, sx=1.0, sy=0.75, up=(0, 0, 1)))
    G.add(fin, dome(0.0095, 0.008, 8), along(path[-1], path[-1] + Vector((0, side, -0.1)).normalized()))
    zc = z - 0.092  # Profilzylinder
    prof = [(0.0085 * math.cos(a), 0.0085 * math.sin(a)) for a in [-0.94 + i * (math.pi + 1.88) / 10 for i in range(11)]] + [(-0.005, -0.0245), (0.005, -0.0245)]
    G.add(C['brass'], prism(prof, 0.0, 0.0085), FX((x, y, zc)))
    G.add(C['hole'], box((x + 0.0085, y - 0.0012, zc - 0.012), (x + 0.0092, y + 0.0012, zc + 0.004)))
    G.add(mat('griffspur', (0.40, 0.38, 0.34), 'paint', r=1.1), blob((x, y, z - 0.03), (0, 1, 0), (0, 0, 1), 0.04, 0.10, rng, 14, 2, 0.35, off=0.0008))


def _door_wear(G, C, xf, y0, y1, rng, rust=0.05, scuff=True):
    """Gebrauchsspuren am Blatt (Front bei x = xf): Rost von unten, Tritt- und Schleifspuren, Abplatzer an Kanten."""
    nx = lambda m: Vector((1, 0, 0))  # noqa: E731
    S = lambda u, v: (Vector((xf + 0.0009, u, v)), Vector((1, 0, 0)))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(S, y0 + 0.004, y1 - 0.004, 0.012, rust, rng, 16, 0.8), nx))
    G.add(C['rust_stain'], orient(strip_uv(lambda u, v: (Vector((xf + 0.0006, u, v)), Vector((1, 0, 0))), y0 + 0.004, y1 - 0.004, 0.012, rust * 2.2, rng, 16, 0.6), nx))
    if scuff:
        scuffm = mat('trittspur', (0.16, 0.15, 0.14), 'paint', r=1.3)
        for _ in range(6):
            y = rng.uniform(y0 + 0.1, y1 - 0.1)
            G.add(scuffm, blob((xf + 0.0012, y, rng.uniform(0.04, 0.17)), (0, 1, 0), (0, 0, 1), rng.uniform(0.02, 0.07), rng.uniform(0.005, 0.02), rng, 10, 1, 0.6,
                               off=0.0, rot=rng.uniform(-0.4, 0.4)))
    chips_on_plane(G, C['steel_dark'], (xf + 0.0011, (y0 + y1) / 2, DH / 2), (0, 1, 0), (0, 0, 1), (y1 - y0) - 0.01, DH - 0.02, rng, 10, 0.003, 0.012)
    for _ in range(2):
        G.add(C['rust_stain'], orient(streak_uv(S, rng.uniform(y1 - 0.08, y1 - 0.02), rng.uniform(0.80, 0.86), 0.010, rng.uniform(0.15, 0.4), rng, 5), nx))


def _frame_wear(G, C, rng, x=0.062, W=DW, fw=FW):
    """Zargenfuss rostig (Putzwasser), Abplatzer am Spiegel."""
    for s in (-1, 1):
        yc = s * (W / 2 + fw / 2)
        S = lambda u, v, yc=yc: (Vector((x + 0.0009, yc + u, v)), Vector((1, 0, 0)))  # noqa: E731
        G.add(C['rust'], orient(strip_uv(S, -fw / 2 + 0.003, fw / 2 - 0.003, 0.0, 0.06, rng, 6, 0.8), lambda m: Vector((1, 0, 0))))
        chips_on_plane(G, C['steel_dark'], (x + 0.0009, yc, 1.0), (0, 1, 0), (0, 0, 1), fw - 0.008, 1.9, rng, 4, 0.003, 0.008, edge=False)


def _steel_door(kind, seed):
    rng = random.Random(seed)
    C = M_common()
    G = Geo()
    col = {'white': (0.80, 0.80, 0.77), 'grey': (0.46, 0.48, 0.48), 'cream': (0.78, 0.72, 0.58), 'window': (0.70, 0.71, 0.69)}[kind]
    # V13.3 Nachforderung: die graue Tuer in schlichtem grauem Lack (mit den Lacktexturen wirkte sie rostig bzw. weiss)
    leaf = mat(f'tuer_{kind}', col, 'paint', tex='painted_metal_white', r=1.05) if kind != 'grey' else mat('tuer_grey', (0.21, 0.23, 0.24), 'paint', r=1.05)
    framem = mat('zarge_lack', (0.27, 0.28, 0.27), 'paint', r=1.1)
    _frame(G, framem)
    G.add(C['black'], box((0.022, -DW / 2 - 0.002, 0.0), (0.026, DW / 2 + 0.002, DH)))  # Dichtung im Falz
    xb, xf = 0.024, 0.056
    y0, y1 = -DW / 2 + 0.003, DW / 2 - 0.003
    G.add(leaf, box((xb, y0, 0.008), (xf, y1, DH - 0.003), bev=0.003, seg=2))
    # Kassetten (2 x 2), beim Fenster nur unten
    for i, (ya, yb) in enumerate(((y0 + 0.09, -0.02), (0.02, y1 - 0.09))):
        for k, (za, zb) in enumerate(((0.20, 0.95), (1.18, 1.93))):
            if kind == 'window' and k == 1:
                continue
            _emboss(G, leaf, xf, ya, yb, za, zb)
    if kind == 'window':  # Lichtausschnitt mit Glasleisten und Schrauben, dahinter Schwaerze
        wy0, wy1, wz0, wz1 = -0.20, 0.20, 1.30, 1.80
        G.add(framem, prism(rrect(wy1 - wy0 + 0.06, wz1 - wz0 + 0.06, 0.008, 2), 0.0, 0.012), FX((xf, 0.0, (wz0 + wz1) / 2)))
        G.add(mat('glas_schwarz', (0.015, 0.015, 0.017), 'glass'), box((xf + 0.002, wy0, wz0), (xf + 0.014, wy1, wz1)))
        for (a, b) in (((wy0, wz0), (wy1, wz0 + 0.012)), ((wy0, wz1 - 0.012), (wy1, wz1)), ((wy0, wz0), (wy0 + 0.012, wz1)), ((wy1 - 0.012, wz0), (wy1, wz1))):
            G.add(framem, box((xf + 0.012, a[0], a[1]), (xf + 0.02, b[0], b[1]), bev=0.002))
        for (yy, zz) in ((wy0 + 0.006, wz0 + 0.1), (wy0 + 0.006, wz1 - 0.1), (wy1 - 0.006, wz0 + 0.1), (wy1 - 0.006, wz1 - 0.1), (0.0, wz0 + 0.006), (0.0, wz1 - 0.006)):
            screw_head(G, C['steel'], (xf + 0.02, yy, zz), (1, 0, 0), 0.0035, seg=6)
        for _ in range(3):  # Kratzer im Glas (heller), Fingerabdrucke
            G.add(mat('glas_kratzer', (0.25, 0.25, 0.26), 'glass', r=3.0), blob((xf + 0.0145, rng.uniform(-0.15, 0.15), rng.uniform(1.35, 1.75)), (0, 1, 0), (0, 0, 1),
                                                                               rng.uniform(0.03, 0.09), 0.0012, rng, 6, 1, 0.2, off=0.0, rot=rng.uniform(-0.6, 0.6)))
    _lever_set(G, C, xf, y1 - 0.075, 1.05, rng, 'schwarz' if kind in ('grey', 'cream') else 'alu', side=-1)
    if kind in ('grey', 'white'):  # Gleitschienen-Tuerschliesser oben, Bandseite
        cm = mat('schliesser', (0.55, 0.56, 0.56), 'metal', r=1.0, m=0.6)
        G.add(cm, box((xf, y0 + 0.06, DH - 0.13), (xf + 0.055, y0 + 0.34, DH - 0.06), bev=0.008, seg=2))
        G.add(cm, box((0.062, -0.25, DH + 0.008), (0.088, 0.25, DH + 0.032), bev=0.003))
        G.add(C['steel_dark'], box((xf + 0.055, y0 + 0.30, DH - 0.10), (xf + 0.075, y0 + 0.50, DH - 0.088), bev=0.002), Matrix.Rotation(-0.05, 4, 'X'))
        G.add(C['steel_dark'], cyl(0.01, 0.02, 10), along((xf + 0.055, y0 + 0.31, DH - 0.095), (xf + 0.075, y0 + 0.31, DH - 0.095)))
        G.add(C['oil'], blob((xf + 0.056, y0 + 0.2, DH - 0.14), (0, 1, 0), (0, 0, 1), 0.03, 0.012, rng, 8, 1, 0.4, off=0.0))
    if kind == 'grey':  # Trittblech (zerkratzt), Blechschild mit eingeschlagenen Zeilen, Nummernschild
        kp = mat('trittblech', (0.62, 0.62, 0.60), 'metal', r=1.1, m=0.6)
        G.add(kp, box((xf, y0 + 0.02, 0.02), (xf + 0.0015, y1 - 0.02, 0.27), bev=0.0006))
        for _ in range(14):
            G.add(C['steel_dark'], blob((xf + 0.0017, rng.uniform(y0 + 0.05, y1 - 0.05), rng.uniform(0.04, 0.25)), (0, 1, 0), (0, 0, 1), rng.uniform(0.02, 0.09), 0.0012, rng,
                                        6, 1, 0.3, off=0.0, rot=rng.uniform(-0.3, 0.3)))
        for sy in (-1, 1):
            for z in (0.05, 0.24):
                screw_head(G, C['steel'], (xf + 0.0015, sy * (DW / 2 - 0.04), z), (1, 0, 0), 0.004, seg=6, kind='flat')
        _plate_lines(G, C, FX((xf + 0.0042, -0.19, 1.62)), 0.26, 0.11, rng, 3)
        G.add(C['black'], box((xf + 0.004, -0.25, 1.78), (xf + 0.005, -0.13, 1.84)))
        for i in range(3):
            G.add(C['paper'], box((xf + 0.005, -0.235 + i * 0.032, 1.79), (xf + 0.0054, -0.215 + i * 0.032, 1.83)))
    if kind == 'white':  # Warnzeichen Hochspannung und Verbotsschild (passen nicht zum Raum)
        xs, ys = xf + 0.0042, -0.19
        _warn_triangle(G, C, FX((xs, ys, 1.55)), 0.22)
        G.add(C['paper'], box((xs, ys - 0.11, 1.22), (xs + 0.0008, ys + 0.11, 1.44), bev=0.0003))
        G.add(C['red'], lathe([(0.085, 0.0), (0.068, 0.0)], 24), FX((xs + 0.0009, ys, 1.33)))
        G.add(C['red'], box((-0.075, -0.008, 0.0), (0.075, 0.008, 0.0004)), FX((xs + 0.0009, ys, 1.33), roll=-math.pi / 4))
        G.add(C['black'], prism([(-0.012, -0.05), (0.012, -0.05), (0.014, 0.02), (-0.014, 0.02)], 0.0, 0.0003), FX((xs + 0.0008, ys, 1.33)))
        G.add(C['black'], cyl(0.012, 0.0003, 8), FX((xs + 0.0008, ys, 1.37)))
        for sy in (-1, 1):
            for z in (1.235, 1.425):
                screw_head(G, C['steel'], (xs + 0.0008, ys + sy * 0.098, z), (1, 0, 0), 0.003, seg=5)
    if kind == 'cream':  # Spruehfarbe: Kreuz und Pfeil mit Laeufern, angeklebter Zettel
        sp = mat('spruehfarbe', (0.62, 0.12, 0.10), 'paint', r=1.2)
        for (a, b) in (((-0.25, 1.75), (0.22, 1.25)), ((-0.24, 1.26), (0.24, 1.72))):
            pts = [Vector((0.0, a[0] + (b[0] - a[0]) * t / 40 + 0.01 * math.sin(t * 0.3), a[1] + (b[1] - a[1]) * t / 40)) for t in range(41)]
            pts = [Vector((xf + 0.0016 + _door_relief(p.y, p.z), p.y, p.z)) for p in pts]
            G.add(sp, sweep([(p.x, p.y, p.z) for p in pts], [(0.0, -0.016), (0.0006, -0.016), (0.0006, 0.016), (0.0, 0.016)], caps=True, up=(1, 0, 0)))
        S = lambda u, v: (Vector((xf + 0.0016, u, v)), Vector((1, 0, 0)))  # noqa: E731
        for _ in range(6):
            yy, zz = rng.uniform(-0.22, 0.22), rng.uniform(1.27, 1.5)
            Sx = lambda u, v, dx=_door_relief(yy, zz): (Vector((xf + 0.0016 + dx, u, v)), Vector((1, 0, 0)))  # noqa: E731
            G.add(sp, orient(streak_uv(Sx, yy, zz, 0.006, min(rng.uniform(0.05, 0.22), zz - 1.2), rng, 5, taper=0.4), lambda m: Vector((1, 0, 0))))
        G.add(C['paper'], box((xf, -0.05, 0.95), (xf + 0.0005, 0.10, 1.16)), Matrix.Rotation(0.03, 4, 'X'))
        for i in range(6):
            G.add(C['ink'], box((xf + 0.0005, -0.04, 1.11 - i * 0.022), (xf + 0.0008, -0.04 + rng.uniform(0.06, 0.12), 1.116 - i * 0.022)), Matrix.Rotation(0.03, 4, 'X'))
        tape = mat('klebeband_klar', (0.72, 0.68, 0.55), 'plastic', r=0.6)
        for (yy, zz) in ((-0.05, 1.16), (0.10, 1.16), (-0.05, 0.95), (0.10, 0.95)):
            G.add(tape, box((xf + 0.0005, yy - 0.022, zz - 0.008), (xf + 0.0009, yy + 0.022, zz + 0.008)))
    _door_wear(G, C, xf + 0.0, y0, y1, rng, rust={'white': 0.05, 'grey': 0.03, 'cream': 0.07, 'window': 0.05}[kind], scuff=kind != 'grey')
    _frame_wear(G, C, rng)
    if kind != 'window':
        G.deform(dent_fn((xf, rng.uniform(-0.2, 0.2), rng.uniform(0.4, 0.9)), (-1, 0, 0), 0.07, 0.006), mats=[leaf])
    G.build(f'tuer_{kind}', angle=40.0)


def build_door_steel_white():
    _steel_door('white', 1)


def build_door_steel_grey():
    _steel_door('grey', 2)


def build_door_steel_cream():
    _steel_door('cream', 3)


def build_door_window():
    _steel_door('window', 4)


def build_door_plywood():
    """Behelfstuer aus Sperrholz in einer Bohlenzarge: Platte mit sichtbaren Lagen an den Kanten, Schraubenreihen auf das
    verdeckte Rahmenholz, D-Griff, Ueberfalle mit Vorhaengeschloss, Wasserschaden unten (dunkel, Furnier abgeloest),
    gespruehte Markierung."""
    rng = random.Random(5)
    C = M_common()
    G = Geo()
    ply = mat('sperrholz', (0.55, 0.47, 0.36), 'concrete', r=1.35)
    plyw = mat('sperrholz_nass', (0.42, 0.34, 0.24), 'concrete', r=1.2)
    edge_l = mat('sperrholz_lage_hell', (0.72, 0.62, 0.46), 'concrete', r=1.3)
    edge_d = mat('sperrholz_lage_dunkel', (0.36, 0.28, 0.19), 'concrete', r=1.3)
    timber = mat('bohle', (0.44, 0.37, 0.28), 'concrete', r=1.35)
    # Bohlenzarge (sägerau), mit Schrauben an der Wand
    G.add(timber, box((0.0, -DW / 2 - 0.05, 0.0), (0.05, -DW / 2, DH + 0.05), bev=0.003))
    G.add(timber, box((0.0, DW / 2, 0.0), (0.05, DW / 2 + 0.05, DH + 0.05), bev=0.003))
    G.add(timber, box((0.0, -DW / 2 - 0.05, DH), (0.05, DW / 2 + 0.05, DH + 0.05), bev=0.003))
    for s in (-1, 1):
        for z in (0.3, 1.0, 1.7):
            screw_head(G, C['steel_dark'], (0.05, s * (DW / 2 + 0.025), z), (1, 0, 0), 0.006, C['black'], seg=6, kind='flat')
    # Platte 18 mm, Kanten mit Lagen
    xb, xf = 0.02, 0.038
    y0, y1 = -DW / 2 + 0.004, DW / 2 - 0.004
    G.add(C['black'], box((0.0, y0 - 0.003, 0.0), (0.02, y1 + 0.003, DH)))
    wet = 0.22
    G.add(ply, box((xb, y0, wet), (xf, y1, DH - 0.004), bev=0.0015))
    G.add(plyw, box((xb, y0, 0.006), (xf, y1, wet), bev=0.0015))
    for (ya, yb) in ((y0 - 0.0002, y0 + 0.0), (y1, y1 + 0.0002)):
        for i in range(7):
            m_ = edge_l if i % 2 == 0 else edge_d
            G.add(m_, box((xb + i * 0.018 / 7, min(ya, yb) - 0.0004, 0.006), (xb + (i + 1) * 0.018 / 7, max(ya, yb) + 0.0004, DH - 0.004)))
    for i in range(7):
        m_ = edge_l if i % 2 == 0 else edge_d
        G.add(m_, box((xb + i * 0.018 / 7, y0, DH - 0.0044), (xb + (i + 1) * 0.018 / 7, y1, DH - 0.0036)))
    # Wasserrand (Tidenlinie), abgeloestes Furnier unten links
    G.add(mat('wasserrand', (0.30, 0.23, 0.15), 'concrete', r=1.2), orient(strip_uv(lambda u, v: (Vector((xf + 0.0008, u, v)), Vector((1, 0, 0))), y0, y1, wet - 0.01,
                                                                                      0.03, rng, 18, 0.9), lambda m: Vector((1, 0, 0))))
    flap = [(0, 0), (0.16, 0.0), (0.13, 0.05), (0.07, 0.09), (0.0, 0.10)]
    G.add(ply, prism(flap, 0.0, 0.0015), Matrix.Translation((xf, y0 + 0.01, 0.02)) @ Matrix.Rotation(math.pi / 2, 4, 'Y') @ Matrix.Rotation(math.pi / 2, 4, 'Z') @
          Matrix.Rotation(0.5, 4, 'X'))
    G.add(edge_d, box((xf - 0.001, y0 + 0.01, 0.02), (xf + 0.0006, y0 + 0.17, 0.12)))
    # Schraubenreihen (Senkkopf) auf das Rahmenholz dahinter
    for z in (0.05, 1.05, DH - 0.05):
        for k in range(7):
            screw_head(G, C['steel_dark'], (xf, y0 + 0.04 + k * (y1 - y0 - 0.08) / 6, z), (1, 0, 0), 0.0045, seg=6, kind='flat')
    for s in (-1, 1):
        for k in range(6):
            screw_head(G, C['steel_dark'], (xf, s * (DW / 2 - 0.045), 0.20 + k * 0.29), (1, 0, 0), 0.0045, seg=6, kind='flat')
    # D-Griff, Ueberfalle mit Vorhaengeschloss
    for z in (0.93, 1.13):
        G.add(C['steel'], box((xf, y1 - 0.12 - 0.012, z - 0.015), (xf + 0.004, y1 - 0.12 + 0.012, z + 0.015), bev=0.001))
    G.add(C['steel'], tube([(xf + 0.004, y1 - 0.12, 0.94), (xf + 0.045, y1 - 0.12, 0.96), (xf + 0.045, y1 - 0.12, 1.10), (xf + 0.004, y1 - 0.12, 1.12)], 0.007, 8, bend=0.02, bseg=3))
    hasp = mat('ueberfalle', (0.34, 0.34, 0.32), 'metal', r=1.0, m=0.6)
    G.add(hasp, box((xf, y1 - 0.02, 1.25), (xf + 0.004, y1 + 0.09, 1.29), bev=0.001))
    G.add(hasp, box((0.05, y1 + 0.015, 1.24), (0.056, y1 + 0.045, 1.30), bev=0.001))
    G.add(hasp, torus(0.012, 0.0035, 10, 4), mt((0.065, y1 + 0.03, 1.27), (0, 0, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    lock = mat('schloss_messing', (0.55, 0.43, 0.22), 'metal', r=0.9, m=0.75)
    G.add(lock, box((0.058, y1 + 0.007, 1.195), (0.078, y1 + 0.053, 1.245), bev=0.004))
    G.add(C['steel'], torus(0.016, 0.003, 12, 4, a0=0.0, a1=math.pi), mt((0.068, y1 + 0.03, 1.245), (math.pi / 2, 0, 0)))
    # Spruehmarkierung (Nummer als drei Balken) und Schmutz
    sp = mat('spruehfarbe_weiss', (0.80, 0.80, 0.76), 'paint', r=1.2)
    for i in range(3):
        G.add(sp, box((xf + 0.0006, -0.18 + i * 0.07, 1.62), (xf + 0.0012, -0.15 + i * 0.07, 1.80)), Matrix.Rotation(rng.uniform(-0.04, 0.04), 4, 'X'))
    gw = mat('holz_wasser', (0.50, 0.43, 0.33), 'concrete', r=1.35)
    Sx = lambda u, v: (Vector((xf + 0.0009, u, v)), Vector((1, 0, 0)))  # noqa: E731
    for _ in range(5):  # Wasserlaeufer von oben (Tropfwasser), weich und breit
        G.add(gw, orient(streak_uv(Sx, rng.uniform(-0.38, 0.38), DH - 0.01, rng.uniform(0.05, 0.12), rng.uniform(0.4, 1.3), rng, 8, taper=0.5), lambda m: Vector((1, 0, 0))))
    G.add(mat('griffspur_holz', (0.42, 0.36, 0.28), 'concrete', r=1.2), blob((xf + 0.0011, y1 - 0.12, 1.03), (0, 1, 0), (0, 0, 1), 0.035, 0.10, rng, 12, 2, 0.4,
                                                                            off=0.0))
    G.build('tuer_sperrholz', angle=40.0)


def build_door_wood_old():
    """Alte Brettertuer (gespundete Bretter mit V-Fugen, Querleisten und Strebe), dunkelgruener Altanstrich, grossflaechig
    abgeblaettert bis aufs vergraute Holz, faule Unterkante; zwei Langbaender mit Kloben an der Holzzarge, Kastenschloss
    mit Knauf, Ueberfalle mit Vorhaengeschloss, Emailleschild."""
    rng = random.Random(6)
    C = M_common()
    G = Geo()
    paint = mat('tuer_altgruen', (0.15, 0.22, 0.17), 'paint', r=1.25)
    wood = mat('holz_vergraut', (0.36, 0.33, 0.28), 'concrete', r=1.4)
    wood_d = mat('holz_faul', (0.24, 0.20, 0.15), 'concrete', r=1.4)
    framem = mat('zarge_holz', (0.24, 0.27, 0.22), 'paint', r=1.25)
    iron = mat('eisen_alt', (0.16, 0.14, 0.12), 'metal', tex='metal_rusty_iron', r=1.3, m=0.4)
    for s in (-1, 1):
        G.add(framem, box((0.0, s * DW / 2 + (0.0 if s > 0 else -0.06), 0.0), (0.055, s * DW / 2 + (0.06 if s > 0 else 0.0), DH + 0.05), bev=0.004))
    G.add(framem, box((0.0, -DW / 2 - 0.06, DH), (0.055, DW / 2 + 0.06, DH + 0.05), bev=0.004))
    G.add(C['black'], box((0.0, -DW / 2, 0.0), (0.02, DW / 2, DH)))
    xb, xf = 0.02, 0.042
    nb = 6
    bw = DW / nb
    for k in range(nb):  # Bretter mit V-Fuge und leichtem Verzug
        ya, yb = -DW / 2 + k * bw + 0.003, -DW / 2 + (k + 1) * bw - 0.003
        rot = 0.02 + 0.03 * rng.random() if k == 0 else rng.uniform(0.004, 0.025)
        G.add(paint, box((xb, ya, rot), (xf, yb, DH - 0.004), bev=0.003, seg=1), Matrix.Rotation(rng.uniform(-0.002, 0.002), 4, 'X'))
        G.add(C['hole'], box((xb, yb - 0.0005, rot), (xf - 0.004, yb + 0.006, DH - 0.004)))
    lx = xf + 0.022  # Querleisten und Strebe
    for z in (0.18, 1.0, 1.85):
        G.add(paint, box((xf, -DW / 2 + 0.03, z - 0.07), (lx, DW / 2 - 0.03, z + 0.07), bev=0.004))
    for (za, zb) in ((0.25, 0.93), (1.07, 1.78)):
        a, b = Vector((xf + 0.011, DW / 2 - 0.08, za)), Vector((xf + 0.011, -DW / 2 + 0.08, zb))
        G.add(paint, box((-0.011, -0.05, 0.0), (0.011, 0.05, (b - a).length), bev=0.003), along(a, b, up=(1, 0, 0)))
    # Abgeblaetterter Anstrich: vergraute Holzflecken auf Brettern und Leisten (unten mehr), faule Unterkante
    for _ in range(18):
        z = rng.uniform(0.05, 1.9) if rng.random() < 0.4 else rng.uniform(0.05, 0.6)
        y = rng.uniform(-DW / 2 + 0.03, DW / 2 - 0.03)
        on_leiste = any(abs(z - zz) < 0.065 for zz in (0.18, 1.0, 1.85))
        x = (lx if on_leiste else xf) + 0.0008
        G.add(wood, blob((x, y, z), (0, 1, 0), (0, 0, 1), rng.uniform(0.015, 0.06), rng.uniform(0.008, 0.035), rng, 12, 2, 0.6, off=0.0, rot=rng.uniform(-0.3, 0.3)))
    G.add(wood_d, orient(strip_uv(lambda u, v: (Vector((xf + 0.001, u, v)), Vector((1, 0, 0))), -DW / 2 + 0.005, DW / 2 - 0.005, 0.02, 0.10, rng, 18, 0.9),
                         lambda m: Vector((1, 0, 0))))
    G.add(wood_d, orient(strip_uv(lambda u, v: (Vector((lx + 0.001, u, v)), Vector((1, 0, 0))), -DW / 2 + 0.035, DW / 2 - 0.035, 0.11, 0.05, rng, 14, 0.9),
                         lambda m: Vector((1, 0, 0))))
    # Langbaender ueber die Leisten (Bandseite -y), Kloben an der Zarge
    for z in (0.18, 1.85):
        L = 0.62
        G.add(iron, prism([(0, -0.022), (L - 0.03, -0.012), (L, 0.0), (L - 0.03, 0.012), (0, 0.022)], 0.0, 0.005), FX((lx, -DW / 2 + 0.02, z)))
        G.add(iron, cyl(0.011, 0.07, 10), along((lx - 0.004, -DW / 2 + 0.015, z - 0.035), (lx - 0.004, -DW / 2 + 0.015, z + 0.035)))
        G.add(iron, box((0.03, -DW / 2 - 0.05, z - 0.03), (0.06, -DW / 2 + 0.0, z + 0.03), bev=0.003))
        for k in range(4):
            G.add(iron, dome(0.006, 0.004, 6), along((lx + 0.005, -DW / 2 + 0.06 + k * 0.15, z), (lx + 0.01, -DW / 2 + 0.06 + k * 0.15, z)))
        G.add(C['rust_stain'], orient(streak_uv(lambda u, v: (Vector((xf + 0.0012, u, v)), Vector((1, 0, 0))), -DW / 2 + 0.2, z - 0.06, 0.05, min(0.25, z - 0.1), rng, 6),
                                      lambda m: Vector((1, 0, 0))))
    # Kastenschloss mit Knauf und Schluesselloch, Ueberfalle mit Vorhaengeschloss
    G.add(iron, box((lx, DW / 2 - 0.20, 0.93), (lx + 0.025, DW / 2 - 0.04, 1.06), bev=0.004))
    G.add(C['brass'], lathe([(0, 0), (0.012, 0), (0.024, 0.03), (0.022, 0.045), (0, 0.05)], 10), along((lx + 0.025, DW / 2 - 0.15, 1.0), (lx + 1, DW / 2 - 0.15, 1.0)))
    G.add(C['hole'], box((lx + 0.025, DW / 2 - 0.085, 0.955), (lx + 0.0256, DW / 2 - 0.079, 0.985)))
    G.add(iron, box((lx, DW / 2 - 0.10, 1.25), (lx + 0.005, DW / 2 + 0.07, 1.29), bev=0.001))
    G.add(iron, box((0.05, DW / 2 + 0.02, 1.24), (0.058, DW / 2 + 0.05, 1.30), bev=0.001))
    lock = mat('schloss_eisen', (0.22, 0.20, 0.18), 'metal', r=1.1, m=0.6)
    G.add(lock, box((0.06, DW / 2 + 0.012, 1.19), (0.082, DW / 2 + 0.058, 1.245), bev=0.005))
    G.add(C['steel'], torus(0.016, 0.003, 12, 4, a0=0.0, a1=math.pi), mt((0.071, DW / 2 + 0.035, 1.245), (math.pi / 2, 0, 0)))
    # Emailleschild (abgeplatzt): weiss mit blauem Rand, Zeilen
    enam = mat('emaille_weiss', (0.84, 0.84, 0.80), 'ceramic', r=0.6)
    blue = mat('emaille_blau', (0.10, 0.18, 0.42), 'ceramic', r=0.6)
    G.add(blue, box((lx, -0.12, 1.42), (lx + 0.002, 0.12, 1.58), bev=0.0008))
    G.add(enam, box((lx + 0.002, -0.11, 1.43), (lx + 0.0026, 0.11, 1.57)))
    for i in range(2):
        G.add(C['ink'], box((lx + 0.0026, -0.09, 1.52 - i * 0.05), (lx + 0.003, 0.09 - i * 0.05, 1.545 - i * 0.05)))
    for _ in range(4):
        G.add(C['black'], blob((lx + 0.0031, rng.uniform(-0.11, 0.11), rng.choice((1.43, 1.57))), (0, 1, 0), (0, 0, 1), rng.uniform(0.006, 0.014), rng.uniform(0.005, 0.01), rng,
                               8, 1, 0.5, off=0.0))
    G.add(C['grime'], blob((lx + 0.001, DW / 2 - 0.15, 1.0), (0, 1, 0), (0, 0, 1), 0.10, 0.12, rng, 12, 2, 0.4, off=0.0))
    G.build('tuer_holz', angle=40.0)


def build_door_level3():
    """Offene Stahltuer im Durchbruch nach Level 3: Zarge auf der +x-Seite, Blatt um 80 Grad nach -x geoeffnet (beide
    Seiten mit Kassetten, Drueckergarnituren, Baendern, Gebrauchsspuren), oben eine mit dem Pinsel gemalte "3" mit
    Laeufern auf der Wand ueber dem Durchbruch (Flaeche bei x = -0.5)."""
    rng = random.Random(8)
    C = M_common()
    G = Geo()
    framem = mat('zarge_lack', (0.27, 0.28, 0.27), 'paint', r=1.1)
    leafm = mat('tuer_l3', (0.68, 0.69, 0.66), 'paint', tex='painted_metal_white', r=1.05)
    W, H = 0.98, 2.04
    Gf = Geo()
    _frame(Gf, framem, 0.0, W - 0.1, H - 0.05, 0.05)
    G.merge(Gf, Matrix.Translation((0.44, 0.0, 0.0)))
    # Blatt in eigenem System: Bandachse bei y = 0, Blatt nach +y, Dicke entlang x (0..0.04)
    Gl = Geo()
    T = 0.04
    BW = W - 0.12
    Gl.add(leafm, box((0.0, 0.0, 0.008), (T, BW, H - 0.06), bev=0.003, seg=2))
    for xs, sgn in ((T, 1), (0.0, -1)):
        for (ya, yb) in ((0.07, BW / 2 - 0.02), (BW / 2 + 0.02, BW - 0.07)):
            for (za, zb) in ((0.20, 0.95), (1.15, 1.88)):
                _emboss(Gl, leafm, xs, ya, yb, za, zb, dirx=sgn)
    Gs = Geo()
    _lever_set(Gs, C, T, BW - 0.075, 1.05, rng, 'alu', side=-1)
    Gl.merge(Gs)
    Gs2 = Geo()
    _lever_set(Gs2, C, 0.0, 0.0, 0.0, rng, 'alu', side=1)
    Gl.merge(Gs2, Matrix.Translation((0.0, BW - 0.075, 1.05)) @ Matrix.Rotation(math.pi, 4, 'Z'))
    for z in (0.25, 1.05, 1.80):  # Baender
        Gl.add(C['steel_dark'], cyl(0.011, 0.11, 10), along((T / 2, -0.012, z - 0.055), (T / 2, -0.012, z + 0.055)))
        Gl.add(C['grease'], cyl(0.0115, 0.008, 10), along((T / 2, -0.012, z - 0.004), (T / 2, -0.012, z + 0.004)))
    _door_wear(Gl, C, T, 0.0, BW, rng, 0.06)
    chips_on_plane(Gl, C['steel_dark'], (-0.0011, BW / 2, H / 2), (0, -1, 0), (0, 0, 1), BW - 0.02, H - 0.1, rng, 8, 0.003, 0.012)
    Gl.deform(dent_fn((T, BW * 0.6, 0.55), (-1, 0, 0), 0.06, 0.007))
    hinge = Vector((0.445, -W / 2 + 0.055, 0.0))
    G.merge(Gl, Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(80), 4, 'Z') @ Matrix.Translation((-T, 0, 0)))
    # Pinsel-"3" auf der Wand ueber dem Durchbruch (Blick von -x: rechts = -y), mit Laeufern
    paint3 = mat('farbe_drei', (0.86, 0.85, 0.79), 'paint', r=1.2)
    x = -0.5035
    stroke = [(-0.10, 2.49), (0.07, 2.50), (0.075, 2.47), (-0.02, 2.36), (0.05, 2.34), (0.085, 2.28), (0.07, 2.215), (0.0, 2.185), (-0.10, 2.20)]
    pts = [(x, -u, z) for (u, z) in stroke]
    pts = fillet(pts, 0.02, 3)
    G.add(paint3, sweep(pts, [(0.0, -0.016), (0.0012, -0.016), (0.0012, 0.016), (0.0, 0.016)], caps=True, up=(-1, 0, 0)))
    S = lambda u, v: (Vector((x - 0.0006, -u, v)), Vector((-1, 0, 0)))  # noqa: E731
    for (u, z, L) in ((-0.06, 2.475, 0.05), (0.03, 2.34, 0.08), (-0.08, 2.185, 0.12), (0.04, 2.20, 0.06)):
        G.add(paint3, orient(streak_uv(S, u, z, 0.007, L, rng, 5, taper=0.35), lambda m: Vector((-1, 0, 0))))
    G.build('tuer_level3', angle=40.0)


# =====================================================================================================================
# Abgeschraegte Ecke, Gleis, Trichterwagen, Plattformwagen, Werkzeugkasten, Stehleiter, Kabeltrommel, Klappstuhl
# =====================================================================================================================
MYZX = Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))  # Umriss in (y, z), gezogen entlang +x


def build_chamfer_26():
    """Abgeschraegte Tunnelecke (45 Grad, 1 x 1 m, 2,6 m hoch) aus Ortbeton wie die Waende: Schalungsstoss mit Grat,
    Ankerloecher mit Kunststoffkonen, Schmutzrand am Boden, Ausbluehungen, Wasserlaeufer von der Decke, Riss, zwei
    alte Duebel mit Rostfahnen (abgebaute Halterung). Dunkler/heller getoente Flecken mit derselben Betontextur."""
    rng = random.Random(91)
    C = M_common()
    G = Geo()
    H = 2.6
    conc = mat('beton', (0.70, 0.68, 0.64), 'concrete', tex='concrete_chalky')
    dirty = mat('beton_schmutz', (0.42, 0.39, 0.34), 'concrete', tex='concrete_chalky')
    wet = mat('beton_nass', (0.52, 0.49, 0.44), 'concrete', tex='concrete_chalky', r=0.9)
    salt = mat('beton_salz', (0.86, 0.85, 0.81), 'concrete', tex='concrete_chalky', r=1.3)
    G.add(conc, prism([(-0.5, -0.5), (0.5, -0.5), (-0.5, 0.5)], 0.0, H))
    U = Vector((-1, 1, 0)).normalized()
    N = Vector((1, 1, 0)).normalized()
    W = math.sqrt(2.0)
    c0 = Vector((0.0, 0.0, 0.0))
    nf = lambda m: N  # noqa: E731
    S = lambda off: (lambda u, v: (c0 + N * off + U * u + Vector((0, 0, v)), N))  # noqa: E731
    # Schalungsstoss (Grat) waagrecht und senkrecht
    for z in (1.25,):
        pts = [c0 + U * (-W / 2 + 0.01 + (W - 0.02) * i / 8) + Vector((0, 0, z + rng.uniform(-0.002, 0.002))) for i in range(9)]
        G.add(conc, sweep(pts, [(0.0, -0.004), (0.003, -0.002), (0.003, 0.002), (0.0, 0.004)], up=N))
    # Ankerloecher (2 x 3) mit Konus
    plug = mat('konus', (0.45, 0.45, 0.44), 'plastic', r=1.2)
    for u in (-0.35, 0.35):
        for z in (0.45, 1.25, 2.05):
            p = c0 + U * u + Vector((0, 0, z))
            G.add(C['hole'], lathe([(0.016, 0.0), (0.011, 0.0)], 12), along(p + N * 0.0006, p + N))
            G.add(plug, lathe([(0, -0.004), (0.011, -0.004), (0.011, 0.0003), (0, 0.0003)], 12), along(p, p + N))
            G.add(dirty, orient(streak_uv(S(0.0008), u, z - 0.012, 0.012, rng.uniform(0.05, 0.15), rng, 4), nf))
    # Schmutzrand am Boden, Ausbluehungen darueber, Wasserlaeufer von oben
    G.add(dirty, orient(strip_uv(S(0.0010), -W / 2 + 0.002, W / 2 - 0.002, 0.0, 0.16, rng, 18, 0.6), nf))
    G.add(wet, orient(strip_uv(S(0.0007), -W / 2 + 0.002, W / 2 - 0.002, 0.0, 0.30, rng, 18, 0.5), nf))
    for _ in range(5):
        G.add(salt, orient(blob_uv(S(0.0013), rng.uniform(-0.6, 0.6), rng.uniform(0.18, 0.32), rng.uniform(0.04, 0.12), rng.uniform(0.01, 0.03), rng, 10, 2, 0.6), nf))
    for _ in range(3):
        u0 = rng.uniform(-0.55, 0.55)
        G.add(wet, orient(streak_uv(S(0.0009), u0, H - 0.002, rng.uniform(0.10, 0.18), rng.uniform(0.8, 1.6), rng, 9, taper=0.5), nf))
        G.add(dirty, orient(streak_uv(S(0.0011), u0 + rng.uniform(-0.02, 0.02), H - 0.002, 0.025, rng.uniform(0.3, 0.7), rng, 6, taper=0.7), nf))
    # Riss (duenne Zickzacklinie)
    pts = []
    u, z = -0.2, 0.9
    for i in range(12):
        pts.append(c0 + N * 0.0012 + U * u + Vector((0, 0, z)))
        u += rng.uniform(-0.025, 0.04)
        z += rng.uniform(0.03, 0.06)
    G.add(C['hole'], sweep(pts, [(0.0, -0.0012), (0.0004, -0.0012), (0.0004, 0.0012), (0.0, 0.0012)], up=N))
    # Duebel mit Rostfahnen
    for u in (0.12, 0.30):
        p = c0 + U * u + Vector((0, 0, 1.72))
        G.add(C['rust'], lathe([(0, 0), (0.006, 0), (0.005, 0.004), (0, 0.005)], 6), along(p, p + N))
        G.add(C['rust_stain'], orient(streak_uv(S(0.0011), u, 1.715, 0.02, rng.uniform(0.25, 0.5), rng, 6), nf))
    G.build('schraege', angle=30.0)


def build_rail_track():
    """Schmalspurgleis (600 mm) als 5-m-Modul: Vignolschienen (Profil), halbe Laschenstoesse an den Enden (passen Ende
    an Ende), Unterlagsplatten mit Hakennaegeln, verrottende Holzschwellen (verrutscht, eine fehlt, eine gebrochen,
    faule Enden, Risse), Schotter- und Schmutzbett mit losen Steinen; alles rostig."""
    rng = random.Random(95)
    C = M_common()
    G = Geo()
    rail = mat('schiene', (0.42, 0.28, 0.20), 'metal', tex='metal_rusty_iron', r=1.3, m=0.25)
    sleeper = mat('schwelle', (0.30, 0.26, 0.21), 'concrete', r=1.4)
    rot = mat('schwelle_faul', (0.18, 0.15, 0.12), 'concrete', r=1.4)
    bed = mat('schotterbett', (0.36, 0.33, 0.29), 'concrete', r=1.45)
    stone = mat('schotter', (0.44, 0.41, 0.37), 'concrete', r=1.4)
    zt = 0.068  # Schienenfuss
    prof = [(-0.025, 0.0), (0.025, 0.0), (0.025, 0.004), (0.006, 0.012), (0.0045, 0.045), (0.012, 0.050), (0.0125, 0.067), (0.010, 0.070),
            (-0.010, 0.070), (-0.0125, 0.067), (-0.012, 0.050), (-0.0045, 0.045), (-0.006, 0.012), (-0.025, 0.004)]
    for yr in (-0.316, 0.316):
        G.add(rail, prism([(y + yr, z + zt) for y, z in prof], -2.5, 2.5), MYZX)
        for xe, s in ((2.5, 1), (-2.5, -1)):  # halbe Laschen beidseits des Stegs, zwei Schrauben je Haelfte
            for side in (-1, 1):
                y0 = yr + side * 0.0045
                y1 = yr + side * 0.016
                G.add(C['rust_dark'], box((min(xe, xe - s * 0.2), min(y0, y1), zt + 0.014), (max(xe, xe - s * 0.2), max(y0, y1), zt + 0.043), bev=0.002))
            for xb in (xe - s * 0.06, xe - s * 0.15):
                bolt(G, C['rust_dark'], (xb, yr + 0.016, zt + 0.0285), (0, 1, 0), 0.008, stub=0.008, washer=False)
                bolt(G, C['rust_dark'], (xb, yr - 0.016, zt + 0.0285), (0, -1, 0), 0.008, washer=False)
    xs = [-2.4 + 0.6 * k for k in range(9)]
    for k, x in enumerate(xs):
        if k == 6:
            continue  # fehlt
        dx, ry = rng.uniform(-0.04, 0.04), rng.uniform(-0.05, 0.05)
        Ms = mt((x + dx, rng.uniform(-0.02, 0.02), 0.0), (0, 0, ry))
        vs, fs = box((-0.07, -0.5, 0.0), (0.07, 0.5, 0.062), bev=0.006, cuts=2)
        o = Vector((rng.uniform(0, 50), rng.uniform(0, 50), 0))
        vs2 = []
        for v in vs:  # Oberseite verwittert, faule Enden abgesackt
            drop = 0.006 * (0.5 + 0.5 * nz(v, 9.0, o)) + 0.02 * max(0.0, abs(v.y) - 0.38) / 0.12
            vs2.append(Vector((v.x * (1.0 + 0.04 * nz(v, 5.0, o)), v.y, v.z * (1.0 - drop / 0.062))))
        if k == 3:  # gebrochen: zwei Haelften, leicht verdreht
            G.add(sleeper, (vs2, fs), Ms @ Matrix.Diagonal((1.0, 0.48, 1.0, 1.0)) @ Matrix.Translation((0, -0.52, 0)))
            G.add(sleeper, (vs2, fs), Ms @ Matrix.Translation((0.02, 0.01, 0)) @ Matrix.Rotation(0.08, 4, 'Z') @ Matrix.Diagonal((1.0, 0.48, 1.0, 1.0)) @ Matrix.Translation((0, 0.5, 0)))
        else:
            G.add(sleeper, (vs2, fs), Ms)
        for s in (-1, 1):  # faule Enden, Risse
            G.add(rot, blob((0.0, s * 0.44, 0.049), (1, 0, 0), (0, 1, 0), 0.05, 0.06, rng, 10, 2, 0.5, off=0.0015), Ms)
        for _ in range(2):
            G.add(C['hole'], orient(streak_uv(lambda u, v: (Vector((u, v, 0.0575)), Vector((0, 0, 1))), rng.uniform(-0.04, 0.04), rng.uniform(-0.3, 0.3), 0.004,
                                              rng.uniform(0.1, 0.3), rng, 5, down=1.0, taper=0.9), lambda m: Vector((0, 0, 1))), Ms)
        if k == 3:
            continue
        for yr in (-0.316, 0.316):  # Unterlagsplatte und Hakennaegel
            G.add(C['rust_dark'], box((-0.06, yr - 0.08, 0.06), (0.06, yr + 0.08, zt), bev=0.0015), Ms)
            for sy in (-1, 1):
                for sx in (-1, 1) if k % 2 else (1,):
                    q = Vector((sx * 0.03, yr + sy * 0.034, zt))
                    G.add(C['rust'], box((-0.006, -0.006, 0.0), (0.006, 0.006, 0.006)), Ms @ Matrix.Translation(q))
                    G.add(C['rust'], box((-0.006, -0.002, 0.004), (0.006, 0.016, 0.009)), Ms @ Matrix.Translation(q) @ Matrix.Rotation(0.0 if sy < 0 else math.pi, 4, 'Z'))
    # Bett: niedriger Huegel, in x periodisch (Enden passen), zu den Seiten auslaufend
    ph = [(rng.randint(1, 6), rng.uniform(0, TAU), rng.uniform(-6, 6), rng.uniform(0.002, 0.006)) for _ in range(6)]

    def hbed(x, y):
        h = 0.016 + sum(a * math.sin(TAU * k * x / 5.0 + p + c * y) for k, p, c, a in ph)
        edge = 0.40 + 0.06 * math.sin(TAU * 3 * x / 5.0 + 1.3 * (1 if y > 0 else -1)) + 0.03 * math.sin(TAU * 7 * x / 5.0)
        f = max(0.0, 1.0 - max(0.0, abs(y) - edge) / 0.08)
        return max(0.0003, h * f * f)
    nx_, ny_ = 50, 12
    vs, fs = [], []
    for j in range(ny_ + 1):
        y = -0.53 + 1.06 * j / ny_
        for i in range(nx_ + 1):
            x = -2.5 + 5.0 * i / nx_
            vs.append(Vector((x, y, hbed(x, y))))
    for j in range(ny_):
        for i in range(nx_):
            a = j * (nx_ + 1) + i
            fs.append((a, a + 1, a + nx_ + 2, a + nx_ + 1))
    G.add(bed, (vs, fs))
    for _ in range(45):  # lose Steine
        x, y = rng.uniform(-2.45, 2.45), rng.uniform(-0.5, 0.5)
        if abs(abs(y) - 0.316) < 0.03:
            continue
        bms = bmesh.new()
        bmesh.ops.create_icosphere(bms, subdivisions=1, radius=1.0)
        r0 = rng.uniform(0.016, 0.034)
        for vv in bms.verts:
            vv.co = Vector((vv.co.x * r0 * rng.uniform(0.8, 1.3), vv.co.y * r0 * rng.uniform(0.8, 1.3), vv.co.z * r0 * 0.6))
        G.add(stone, _bm_data(bms), mt((x, y, hbed(x, y)), (0, 0, rng.uniform(0, TAU))))
    # loser Hakennagel und Rostfahnen auf dem Bett unter den Stoessen
    G.add(C['rust'], box((-0.05, -0.004, 0.0), (0.05, 0.004, 0.008)), mt((0.7, 0.47, hbed(0.7, 0.47)), (0, 0, 0.7)))
    for x in (-2.45, 2.45):
        for yr in (-0.316, 0.316):
            G.add(C['rust_stain'], blob((x, yr, hbed(x, yr) + 0.0008), (1, 0, 0), (0, 1, 0), 0.08, 0.05, rng, 10, 2, 0.5, off=0.0))
    G.build('gleis', angle=40.0)


def build_hopper_wagon():
    """Entgleister Trichterwagen der Feldbahn (600 mm): Fahrgestell aus U-Profilen mit Pufferbohlen, Radsaetze
    (Scheibenraeder mit Spurkranz, Achse, Achslager), Trichter aus genieteten Blechen mit Winkelrand und Rippen,
    Bodenklappe mit Hebel, Kuppelbolzen mit Kette; Rest von gruenem Anstrich, durchgerostet, verbeult, Schotter
    darin. Liegt schraeg neben dem Gleis."""
    rng = random.Random(97)
    C = M_common()
    G = Geo()
    rustm = mat('wagen_rost', (0.50, 0.32, 0.20), 'metal', tex='metal_rusty_iron', r=1.35, m=0.2)
    frame = mat('wagen_rahmen', (0.24, 0.17, 0.12), 'metal', tex='metal_rusty_iron', r=1.4, m=0.2)
    paint = mat('wagen_farbe', (0.20, 0.28, 0.20), 'paint', r=1.3)
    gravel = mat('wagen_schotter', (0.27, 0.26, 0.24), 'concrete', r=1.45)
    # Fahrgestell
    for s in (-1, 1):
        Uy = [(0, 0), (0.06, 0), (0.06, 0.006), (0.006, 0.006), (0.006, 0.094), (0.06, 0.094), (0.06, 0.1), (0, 0.1)]
        prof = [(s * 0.36 - s * y, z + 0.22) for y, z in Uy]
        G.add(frame, prism(prof, -0.86, 0.86), MYZX)
    for x in (-0.86, 0.86):
        G.add(frame, box((x - 0.04 if x < 0 else x - 0.0, -0.42, 0.20), (x + 0.0 if x < 0 else x + 0.04, 0.42, 0.33), bev=0.004))
    for x in (-0.3, 0.3):
        G.add(frame, box((x - 0.03, -0.36, 0.24), (x + 0.03, 0.36, 0.30), bev=0.003))
    # Radsaetze
    wheel = [(0.0, -0.035), (0.05, -0.035), (0.05, -0.02), (0.13, -0.02), (0.145, -0.035), (0.165, -0.035), (0.168, -0.028), (0.152, -0.018),
             (0.15, 0.035), (0.13, 0.035), (0.125, 0.02), (0.06, 0.02), (0.055, 0.035), (0.0, 0.035)]
    for x in (-0.55, 0.55):
        for s in (-1, 1):
            # Spurkranz innen (zur Gleismitte): lokale -z zeigt nach innen
            Mw = Matrix.Translation((x, s * 0.30, 0.15)) @ (Matrix.Rotation(-s * math.pi / 2, 4, 'X'))
            G.add(C['rust_dark'], lathe(wheel, 20), Mw)
            for k in range(6):
                a = TAU * k / 6
                G.add(C['hole'], cyl(0.018, 0.002, 8), Mw @ Matrix.Translation((math.cos(a) * 0.092, math.sin(a) * 0.092, 0.019)))
            G.add(frame, box((x - 0.06, s * 0.36, 0.10), (x + 0.06, s * 0.36 + s * 0.05, 0.24), bev=0.006) if s > 0 else
                  box((x - 0.06, -0.41, 0.10), (x + 0.06, -0.36, 0.24), bev=0.006))
        G.add(C['rust_dark'], cyl(0.028, 0.84, 10), along((x, -0.42, 0.15), (x, 0.42, 0.15)))
    # Trichter: Blech mit Dicke, Winkelrand, Rippen, Nietreihen
    top, bot = (0.90, 0.45, 1.0), (0.24, 0.12, 0.38)
    bm = bmesh.new()
    vt = [bm.verts.new((sx * top[0], sy * top[1], top[2])) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    vb = [bm.verts.new((sx * bot[0], sy * bot[1], bot[2])) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((vt[i], vt[j], vb[j], vb[i]))
    bm.faces.new(vb[::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=3, use_grid_fill=True)
    bmesh.ops.solidify(bm, geom=bm.faces[:], thickness=0.008)
    hop = _bm_data(bm)
    G.add(rustm, hop)
    rim = [(-top[0], -top[1], top[2]), (top[0], -top[1], top[2]), (top[0], top[1], top[2]), (-top[0], top[1], top[2]), (-top[0], -top[1], top[2])]
    G.add(frame, sweep(rim, [(0.0, 0.0), (0.05, 0.0), (0.05, 0.006), (0.006, 0.006), (0.006, -0.05), (0.0, -0.05)], caps=True, up=(0, 0, 1)))
    for sy in (-1, 1):  # Rippen an den Laengsseiten
        for x in (-0.5, 0.0, 0.5):
            a = Vector((x, sy * top[1], top[2] - 0.05))
            b = Vector((x * bot[0] / top[0], sy * bot[1], bot[2] + 0.04))
            nrm_ = Vector((0, sy, (top[1] - bot[1]) / (top[2] - bot[2]))).normalized()
            G.add(frame, box((-0.006, 0.0, 0.0), (0.006, 0.05, (b - a).length)), along(a, b, up=nrm_) @ Matrix.Translation((0, -0.0, 0)))
    for sy in (-1, 1):  # Niete an den Kanten
        for i in range(9):
            t_ = (i + 0.5) / 9
            p = Vector((-top[0] + 2 * top[0] * t_, sy * (top[1] + 0.006), top[2] - 0.025))
            rivet(G, frame, p, (0, sy, 0), 0.006)
    for sx in (-1, 1):
        for i in range(6):
            t_ = (i + 0.5) / 6
            p = Vector((sx * (top[0] + 0.006), -top[1] + 2 * top[1] * t_, top[2] - 0.025))
            rivet(G, frame, p, (sx, 0, 0), 0.006)
    # Bodenklappe mit Hebel
    G.add(frame, box((-0.26, -0.14, 0.33), (0.26, 0.14, 0.38), bev=0.004))
    G.add(C['rust_dark'], tube([(0.26, 0.10, 0.355), (0.45, 0.16, 0.36), (0.62, 0.20, 0.42)], 0.012, 6, bend=0.05))
    # Kupplung: Kuppelbolzen mit Kette, Puffer
    for x, s in ((-0.90, -1), (0.90, 1)):
        G.add(frame, box((x, -0.08, 0.22), (x + s * 0.06, 0.08, 0.30), bev=0.005))
        G.add(C['rust_dark'], cyl(0.012, 0.12, 8), along((x + s * 0.03, 0, 0.32), (x + s * 0.03, 0, 0.20)))
        for k in range(3):
            G.add(C['rust_dark'], torus(0.025, 0.006, 8, 4), mt((x + s * (0.08 + k * 0.045), 0, 0.24 - k * 0.025), (math.pi / 2 if k % 2 else 0, 0.6, 0)))
    # Schotter im Trichter
    vs, fs = [], []
    nxs, nys = 12, 6
    zg = 0.86
    w_at = lambda z: (bot[0] + (top[0] - bot[0]) * (z - bot[2]) / (top[2] - bot[2]), bot[1] + (top[1] - bot[1]) * (z - bot[2]) / (top[2] - bot[2]))  # noqa: E731
    wx, wy = w_at(zg)
    for j in range(nys + 1):
        for i in range(nxs + 1):
            x = -wx + 2 * wx * i / nxs
            y = -wy + 2 * wy * j / nys
            hump = 0.13 * max(0.0, 1.0 - (x / wx) ** 2) * max(0.0, 1.0 - (y / wy) ** 2) + 0.025 * nz((x, y, 0), 9.0) + 0.012 * nz((x, y, 0), 31.0)
            vs.append(Vector((x * 0.985, y * 0.97, zg + hump)))
    for j in range(nys):
        for i in range(nxs):
            a = j * (nxs + 1) + i
            fs.append((a, a + 1, a + nxs + 2, a + nxs + 1))
    G.add(gravel, (vs, fs))
    for _ in range(26):
        x, y = rng.uniform(-wx * 0.8, wx * 0.8), rng.uniform(-wy * 0.8, wy * 0.8)
        hump = 0.13 * max(0.0, 1.0 - (x / wx) ** 2) * max(0.0, 1.0 - (y / wy) ** 2) + 0.025 * nz((x, y, 0), 9.0) + 0.012 * nz((x, y, 0), 31.0)
        bms = bmesh.new()
        bmesh.ops.create_icosphere(bms, subdivisions=1, radius=1.0)
        r0 = rng.uniform(0.02, 0.045)
        for vv in bms.verts:
            vv.co = Vector((vv.co.x * r0 * rng.uniform(0.8, 1.3), vv.co.y * r0 * rng.uniform(0.8, 1.3), vv.co.z * r0 * 0.7))
        G.add(mat('wagen_stein', (0.38, 0.36, 0.33), 'concrete', r=1.4), _bm_data(bms), mt((x, y, zg + hump), (0, 0, rng.uniform(0, TAU))))
    # Anstrichreste, Rostlaeufer, Delle
    for _ in range(10):
        sy = rng.choice((-1, 1))
        t_ = rng.uniform(0.15, 0.85)
        z = top[2] - (top[2] - bot[2]) * t_
        wxz, wyz = w_at(z)
        p = Vector((rng.uniform(-wxz * 0.8, wxz * 0.8), sy * (wyz + 0.0105), z))
        nrm_ = Vector((0, sy, (top[1] - bot[1]) / (top[2] - bot[2]))).normalized()
        U = Vector((1, 0, 0))
        V = nrm_.cross(U) * (1 if sy < 0 else -1)
        G.add(paint, blob(p, U if sy < 0 else -U, V if sy < 0 else -V, rng.uniform(0.04, 0.14), rng.uniform(0.03, 0.08), rng, 12, 2, 0.6, off=0.0))
    G.deform(dent_fn((0.3, -top[1], 0.8), (0, 1, 0.2), 0.16, 0.035))
    # schraeg neben dem Gleis
    M_ = Matrix.Translation((0.0, 0.35, 0.05)) @ Matrix.Rotation(math.radians(-22), 4, 'X') @ Matrix.Rotation(0.15, 4, 'Z')
    G.deform(lambda v: M_ @ v)
    zmin = min(v.z for p in G.parts.values() for v in p[0])
    G.deform(lambda v: Vector((v.x, v.y, v.z - zmin - 0.08)))
    G.build('trichterwagen', angle=40.0)


def build_trolley_flatbed():
    """Plattformwagen 1,2 x 0,7 m: Winkelrahmen mit Sperrholzboden, zwei Lenk- und zwei Bockrollen (Gabel, Gummirad,
    Achsschraube), Schiebebuegel mit Griffschlauch; Ladung: Stirnrad mit Zaehnen, Nabe, Passfedernut und
    Erleichterungsloechern, Getriebegehaeuse mit Rippen und Flansch, rostiges Rohrstueck mit Flansch, lose Schrauben;
    Oelflecken, Abplatzer."""
    rng = random.Random(101)
    C = M_common()
    G = Geo()
    paint = mat('wagen_blau', (0.20, 0.26, 0.32), 'paint', r=1.2)
    ply = mat('wagen_boden', (0.33, 0.29, 0.23), 'concrete', r=1.35)
    tyre = mat('reifen', (0.07, 0.07, 0.07), 'rubber', r=1.2)
    L, Wd, zd = 0.6, 0.35, 0.165
    rim = [(-L, -Wd, zd), (L, -Wd, zd), (L, Wd, zd), (-L, Wd, zd), (-L, -Wd, zd)]
    G.add(paint, sweep(rim, [(0.0, 0.0), (0.04, 0.0), (0.04, 0.004), (0.004, 0.004), (0.004, 0.035), (0.0, 0.035)], caps=True, up=(0, 0, 1)))
    G.add(ply, box((-L + 0.004, -Wd + 0.004, zd + 0.02), (L - 0.004, Wd - 0.004, zd + 0.035)))
    for x in (-0.35, 0.0, 0.35):
        G.add(paint, box((x - 0.02, -Wd, zd), (x + 0.02, Wd, zd + 0.02)))
    for x in (-0.55, -0.2, 0.2, 0.55):
        for y in (-0.3, 0.3):
            screw_head(G, C['steel_dark'], (x, y, zd + 0.035), (0, 0, 1), 0.006, kind='flat', seg=6)
    # Rollen: Lenkrollen am Buegelende (-x), Bockrollen vorn
    for x in (-0.48, 0.48):
        for y in (-0.26, 0.26):
            swivel = x < 0
            yaw = rng.uniform(-1.2, 1.2) if swivel else 0.0
            M0 = Matrix.Translation((x, y, 0.0))
            G.add(C['zinc'], box((-0.05, -0.04, zd - 0.006), (0.05, 0.04, zd)), M0)
            for sx in (-1, 1):
                for sy in (-1, 1):
                    G.add(C['steel_dark'], nut_shape(0.006, 0.005), M0 @ Matrix.Translation((sx * 0.038, sy * 0.028, zd - 0.011)))
            if swivel:
                G.add(C['zinc'], cyl(0.03, 0.012, 12), M0 @ Matrix.Translation((0, 0, zd - 0.018)))
            Mr = M0 @ Matrix.Rotation(yaw, 4, 'Z')
            off = 0.03 if swivel else 0.0
            for s in (-1, 1):
                G.add(C['zinc'], box((-0.032, s * 0.022, 0.06), (0.032, s * 0.022 + s * 0.004, zd - 0.018)) if s > 0 else
                      box((-0.032, -0.026, 0.06), (0.032, -0.022, zd - 0.018)), Mr @ Matrix.Translation((off, 0, 0)))
            G.add(C['zinc'], box((-0.032, -0.026, zd - 0.024), (0.032, 0.026, zd - 0.018)), Mr @ Matrix.Translation((off, 0, 0)))
            Mw = Mr @ Matrix.Translation((off, 0, 0.063)) @ Matrix.Rotation(math.pi / 2, 4, 'X')
            G.add(tyre, lathe([(0.03, -0.017), (0.058, -0.017), (0.063, -0.01), (0.063, 0.01), (0.058, 0.017), (0.03, 0.017)], 16, close=False), Mw)
            G.add(C['steel'], lathe([(0.0, -0.016), (0.03, -0.016), (0.03, 0.016), (0.0, 0.016)], 12), Mw)
            G.add(C['steel_dark'], nut_shape(0.008, 0.006), Mw @ Matrix.Translation((0, 0, 0.026)))
    # Schiebebuegel mit Fussplatten und Griffschlauch
    hb = [(-0.56, -0.30, zd + 0.035), (-0.58, -0.30, 0.93), (-0.58, 0.30, 0.93), (-0.56, 0.30, zd + 0.035)]
    G.add(paint, tube(hb, 0.0135, 10, bend=0.06, bseg=4))
    G.add(C['black'], cyl(0.017, 0.40, 10), along((-0.58, -0.2, 0.93), (-0.58, 0.2, 0.93)))
    for y in (-0.30, 0.30):
        G.add(paint, box((-0.60, y - 0.03, zd + 0.035), (-0.52, y + 0.03, zd + 0.041), bev=0.002))
        for dx in (-0.025, 0.025):
            bolt(G, C['steel_dark'], (-0.56 + dx, y, zd + 0.041), (0, 0, 1), 0.006, washer=False)
    # Ladung 1: Stirnrad (flach liegend)
    zc = zd + 0.035
    nt, ra, rf_ = 26, 0.19, 0.172
    gear = []
    for i in range(nt):
        a = TAU * i / nt
        for da, rr in ((-0.45, rf_), (-0.25, ra), (0.25, ra), (0.45, rf_)):
            aa = a + da * TAU / nt / 2
            gear.append((rr * math.cos(aa), rr * math.sin(aa)))
    Mg = Matrix.Translation((0.20, -0.05, zc))
    G.add(C['rust'], prism(gear, 0.0, 0.045), Mg)
    G.add(C['rust'], lathe([(0.0, 0.045), (0.065, 0.045), (0.065, 0.075), (0.0, 0.075)], 18), Mg)
    G.add(C['hole'], cyl(0.035, 0.0012, 16), Mg @ Matrix.Translation((0, 0, 0.0752)))
    G.add(C['hole'], box((0.03, -0.006, 0.0752), (0.044, 0.006, 0.0764)), Mg)
    for k in range(4):
        a = TAU * (k + 0.5) / 4
        G.add(C['hole'], cyl(0.03, 0.0012, 12), Mg @ Matrix.Translation((math.cos(a) * 0.11, math.sin(a) * 0.11, 0.0452)))
    G.add(C['grease'], orient(_ring_flat(0.18, 0.02, 0.0458, rng, 26), lambda m: Vector((0, 0, 1))), Mg)
    # Ladung 2: Getriebegehaeuse
    gb = mat('getriebe', (0.30, 0.36, 0.33), 'paint', tex='painted_metal_grey', r=1.2)
    Mb = Matrix.Translation((-0.25, 0.08, zc)) @ Matrix.Rotation(0.3, 4, 'Z')
    G.add(gb, box((-0.15, -0.11, 0.0), (0.15, 0.11, 0.22), bev=0.02, seg=2), Mb)
    for x in (-0.07, 0.0, 0.07):
        G.add(gb, box((x - 0.006, -0.125, 0.02), (x + 0.006, 0.125, 0.2), bev=0.003), Mb)
    G.add(gb, lathe([(0, 0), (0.08, 0), (0.08, 0.02), (0.05, 0.03), (0, 0.03)], 18), Mb @ Matrix.Translation((0.15, 0, 0.11)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    G.add(C['steel'], cyl(0.02, 0.07, 10), Mb @ along((0.18, 0, 0.11), (0.25, 0, 0.11)))
    for k in range(6):
        a = TAU * k / 6
        bolt(G, C['steel_dark'], Mb @ Vector((0.17, math.cos(a) * 0.065, 0.11 + math.sin(a) * 0.065)), (Mb.to_3x3() @ Vector((1, 0, 0))), 0.006, washer=False)
    _plate_lines(G, C, Mb @ Matrix.Translation((0.0, -0.1105, 0.11)) @ Matrix.Rotation(math.pi / 2, 4, 'X'), 0.08, 0.04, rng, 3)
    G.add(C['oil'], blob((0.0, 0.0, 0.0008), (1, 0, 0), (0, 1, 0), 0.12, 0.08, rng, 12, 2, 0.5, off=0.0), Mb)
    # Ladung 3: rostiges Rohrstueck mit Flansch, lose Schrauben
    Pp = Pipe((0.02, 0.24, zc + 0.052), (0.55, 0.27, zc + 0.052), 0.05)
    G.add(C['rust'], cyl(0.05, Pp.L, 14), Pp.M)
    flange(G, {'fl': C['rust'], 'bolt': C['rust_dark']}, Pp.a, -Pp.d, 0.05, 0.1, 0.018, 0, side=0, hub=True)
    for _ in range(5):
        p = Vector((rng.uniform(-0.5, 0.5), rng.uniform(-0.3, 0.3), zc))
        if (p - Vector((0.2, -0.05, zc))).length < 0.22 or abs(p.x + 0.25) < 0.2:
            continue
        G.add(C['steel_dark'], nut_shape(0.01, 0.008), Matrix.Translation(p))
    for _ in range(5):
        G.add(C['oil'], blob((rng.uniform(-0.5, 0.5), rng.uniform(-0.3, 0.3), zc + 0.0008), (1, 0, 0), (0, 1, 0), rng.uniform(0.04, 0.1), rng.uniform(0.03, 0.07), rng,
                             12, 2, 0.5, off=0.0))
    for _ in range(4):
        G.add(C['grime'], blob((rng.uniform(-0.55, 0.55), rng.uniform(-0.3, 0.3), zc + 0.0005), (1, 0, 0), (0, 1, 0), rng.uniform(0.08, 0.2), rng.uniform(0.05, 0.12), rng,
                               12, 2, 0.5, off=0.0))
    chips_on_plane(G, C['steel_dark'], (-0.60, 0.0, zd + 0.02), (0, -1, 0), (0, 0, 1), 0.68, 0.03, rng, 4, 0.004, 0.01, edge=False)
    G.build('plattformwagen', angle=40.0)


def build_toolbox():
    """Werkzeugkasten aus Stahlblech (500 x 220 mm) mit Satteldeckel, Klappgriff mit Griffschlauch, zwei
    Spannverschluessen, Klavierband hinten, Oese fuer ein Vorhaengeschloss, Gummifuesse; roter Lack mit Abplatzern,
    Dellen, Rost an der Unterkante."""
    rng = random.Random(103)
    C = M_common()
    G = Geo()
    red = mat('kasten_rot', (0.46, 0.10, 0.07), 'paint', tex='painted_metal_white', r=1.1)
    L, Wd, H = 0.25, 0.11, 0.17
    G.add(red, box((-L, -Wd, 0.006), (L, Wd, H), bev=0.006, seg=2, cuts=2))
    G.add(red, box((-L - 0.002, -Wd - 0.002, H - 0.012), (L + 0.002, Wd + 0.002, H), bev=0.003))
    # Satteldeckel: Pyramidenstumpf
    b = [Vector((-L - 0.003, -Wd - 0.003, H + 0.002)), Vector((L + 0.003, -Wd - 0.003, H + 0.002)), Vector((L + 0.003, Wd + 0.003, H + 0.002)), Vector((-L - 0.003, Wd + 0.003, H + 0.002))]
    tt = [Vector((-L + 0.06, -0.045, H + 0.062)), Vector((L - 0.06, -0.045, H + 0.062)), Vector((L - 0.06, 0.045, H + 0.062)), Vector((-L + 0.06, 0.045, H + 0.062))]
    bm = _bm_from((b + tt, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.bevel(bm, geom=[e for e in bm.edges], offset=0.004, segments=2, profile=0.5, affect='EDGES', clamp_overlap=True)
    G.add(red, _bm_data(bm))
    # Klappgriff mit Lagerboecken
    for x in (-0.13, 0.13):
        G.add(C['steel'], box((x - 0.012, -0.012, H + 0.062), (x + 0.012, 0.012, H + 0.075), bev=0.002))
    G.add(C['steel'], tube([(-0.13, 0, H + 0.07), (-0.12, 0, H + 0.11), (0.12, 0, H + 0.11), (0.13, 0, H + 0.07)], 0.004, 6, bend=0.015, bseg=3))
    G.add(C['black'], cyl(0.011, 0.17, 10), along((-0.085, 0, H + 0.11), (0.085, 0, H + 0.11)))
    # Spannverschluesse vorn (-y), Schlossoese, Klavierband hinten
    for x in (-0.17, 0.17):
        G.add(C['steel'], box((x - 0.02, -Wd - 0.006, H - 0.06), (x + 0.02, -Wd, H - 0.02), bev=0.002))
        G.add(C['steel'], box((x - 0.012, -Wd - 0.01, H - 0.05), (x + 0.012, -Wd - 0.004, H + 0.015), bev=0.002), Matrix.Rotation(0.0, 4, 'X'))
        G.add(C['steel'], box((x - 0.014, -Wd - 0.008, H + 0.004), (x + 0.014, -Wd - 0.002, H + 0.022), bev=0.002))
        G.add(C['grease'], blob((x, -Wd - 0.0008, H - 0.075), (1, 0, 0), (0, 0, 1), 0.025, 0.012, rng, 8, 1, 0.5, off=0.0))
    G.add(C['steel'], box((-0.012, -Wd - 0.004, H - 0.03), (0.012, -Wd, H + 0.02), bev=0.002))
    G.add(C['hole'], cyl(0.004, 0.0012, 8), along((0, -Wd - 0.0042, H + 0.005), (0, -Wd - 1, H + 0.005)))
    G.add(C['steel_dark'], cyl(0.004, 2 * L - 0.02, 8), along((-L + 0.01, Wd + 0.003, H + 0.002), (L - 0.01, Wd + 0.003, H + 0.002)))
    for x in (-0.2, 0.2):
        for y in (-0.08, 0.08):
            G.add(C['black'], cyl(0.009, 0.006, 8), along((x, y, 0.0), (x, y, 0.006)))
    # Abplatzer an Kanten, Rost unten, Kratzer auf dem Deckel, Dellen
    nx = lambda m: Vector((0, -1, 0))  # noqa: E731
    Sf = lambda u, v: (Vector((-u, -Wd - 0.0009, v)), Vector((0, -1, 0)))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(Sf, -L + 0.01, L - 0.01, 0.008, 0.018, rng, 12, 0.8), nx))
    chips_on_plane(G, C['steel_dark'], (0.0, -Wd - 0.0009, H / 2), (1, 0, 0), (0, 0, 1), 2 * L - 0.01, H - 0.01, rng, 10, 0.003, 0.009)
    chips_on_plane(G, C['steel_dark'], (0.0, Wd + 0.0009, H / 2), (-1, 0, 0), (0, 0, 1), 2 * L - 0.01, H - 0.01, rng, 8, 0.003, 0.009)
    for _ in range(5):
        G.add(C['steel_dark'], blob((rng.uniform(-0.15, 0.15), rng.uniform(-0.03, 0.03), H + 0.0625), (1, 0, 0), (0, 1, 0), rng.uniform(0.02, 0.06), 0.0012, rng, 6, 1, 0.3,
                                    off=0.0, rot=rng.uniform(-0.5, 0.5)))
    G.deform(dent_fn((0.12, -Wd, 0.08), (0, 1, 0), 0.04, 0.006))
    G.build('werkzeugkasten', angle=40.0)


def build_stepladder():
    """Alu-Stehleiter (5 Stufen + Plattform), offen: Kastenholme vorn mit gerillten Stufen (Nieten), Hinterholme mit
    Querstreben, Spreizsicherung (zwei Gelenklaschen), Kunststoff-Plattform mit Ablage, Gummifuesse; Warn- und
    Typaufkleber, Farbspritzer, Abrieb auf den Stufen."""
    rng = random.Random(105)
    C = M_common()
    G = Geo()
    alu = mat('leiter_alu', (0.66, 0.67, 0.68), 'metal', r=1.1, m=0.6)
    plast = mat('leiter_kunststoff', (0.62, 0.26, 0.08), 'plastic', r=1.0)
    top = 1.58
    stile = rrect(0.058, 0.025, 0.004, 1)
    for s in (-1, 1):
        a, b = Vector((0.31, s * 0.21, 0.012)), Vector((0.02, s * 0.18, top))
        G.add(alu, sweep([a, b], [(y, x) for x, y in stile], up=(0, s, 0)))
        ra, rb = Vector((-0.31, s * 0.21, 0.012)), Vector((-0.03, s * 0.18, top - 0.03))
        G.add(alu, sweep([ra, rb], [(y, x) for x, y in rrect(0.04, 0.02, 0.003, 1)], up=(0, s, 0)))
        for p in (a, ra):  # Gummifuesse
            G.add(C['rubber'], box((-0.04, -0.018, 0.0), (0.04, 0.018, 0.035), bev=0.004), Matrix.Translation((p.x, p.y, 0.0)))
            for k in range(3):
                G.add(C['rubber'], box((-0.042, -0.019, 0.0), (0.042, 0.019, 0.004)), Matrix.Translation((p.x, p.y, 0.008 + k * 0.009)))
    tread = [(-0.04, 0.0), (0.04, 0.0), (0.04, -0.025), (0.036, -0.025), (0.036, -0.004), (-0.036, -0.004), (-0.036, -0.012), (-0.04, -0.012)]
    for k in range(5):
        t = (k + 1) / 6
        z = t * top - 0.03
        x = 0.31 + (0.02 - 0.31) * (z - 0.012) / (top - 0.012)
        hw = 0.21 + (0.18 - 0.21) * (z - 0.012) / (top - 0.012) - 0.015
        G.add(alu, prism(tread, -hw, hw), Matrix.Translation((x, 0, z)) @ PXZ)
        for r_ in range(4):  # Rillen
            G.add(alu, box((x - 0.032 + r_ * 0.02, -hw, z), (x - 0.026 + r_ * 0.02, hw, z + 0.003)))
        for s in (-1, 1):
            for dz in (-0.006, -0.018):
                rivet(G, alu, (x, s * (hw + 0.015 + 0.0125), z + dz), (0, s, 0), 0.004)
        G.add(C['grime'], blob((x, 0.0, z + 0.0035), (1, 0, 0), (0, 1, 0), 0.03, 0.10, rng, 10, 1, 0.4, off=0.0))
    # Plattform oben (Kunststoff) mit Ablagen
    G.add(plast, box((-0.10, -0.205, top - 0.01), (0.12, 0.205, top + 0.045), bev=0.008, seg=2))
    for y in (-0.12, 0.0, 0.12):
        G.add(C['hole'], box((-0.06, y - 0.04, top + 0.045), (0.06, y + 0.04, top + 0.0458)))
    G.add(C['hole'], cyl(0.012, 0.0008, 10), along((0.09, 0.15, top + 0.045), (0.09, 0.15, top + 0.05)))
    # Hinterholme: Querstreben, Spreizsicherung
    for z in (0.45, 1.05):
        f = (z - 0.012) / (top - 0.03 - 0.012)
        x = -0.31 + 0.28 * f
        hw = 0.21 - 0.03 * f
        G.add(alu, box((x - 0.012, -hw, z - 0.004), (x + 0.012, hw, z + 0.004), bev=0.0015))
    for s in (-1, 1):
        zz = 0.72
        f1 = (zz - 0.012) / (top - 0.012)
        xa = 0.31 - 0.29 * f1
        f2 = (zz - 0.012) / (top - 0.042)
        xb = -0.31 + 0.28 * f2
        yy = s * (0.21 - 0.03 * f1 - 0.03)
        mid = Vector(((xa + xb) / 2, yy, zz - 0.04))
        G.add(C['steel'], box((-0.008, -0.002, 0.0), (0.008, 0.002, (mid - Vector((xa, yy, zz))).length)), along(Vector((xa, yy, zz)), mid, up=(0, 1, 0)))
        G.add(C['steel'], box((-0.008, -0.002, 0.0), (0.008, 0.002, (mid - Vector((xb, yy, zz))).length)), along(Vector((xb, yy, zz)), mid, up=(0, 1, 0)))
        rivet(G, C['steel'], mid + Vector((0, s * 0.002, 0)), (0, s, 0), 0.005)
    # Aufkleber, Farbspritzer
    pa, pb = Vector((0.31, 0.21, 0.012)), Vector((0.02, 0.18, top))
    d = (pb - pa).normalized()
    nside = Vector((0, 1, 0))
    for (t0, m_) in ((0.45, C['warn_yel']), (0.62, C['paper'])):
        c = pa + d * (t0 * (pb - pa).length) + nside * 0.0135
        G.add(m_, box((-0.024, -0.0004, -0.05), (0.024, 0.0004, 0.05)), Matrix.Translation(c) @ along((0, 0, 0), d, up=(0, 1, 0)).to_3x3().to_4x4() @
              Matrix.Rotation(0, 4, 'Z'))
    paintsp = mat('farbspritzer', (0.82, 0.82, 0.78), 'paint', r=1.2)
    for _ in range(14):
        k = rng.randrange(5)
        t = (k + 1) / 6
        z = t * top - 0.03
        x = 0.31 + (0.02 - 0.31) * (z - 0.012) / (top - 0.012)
        G.add(paintsp, blob((x + rng.uniform(-0.03, 0.03), rng.uniform(-0.15, 0.15), z + 0.0035), (1, 0, 0), (0, 1, 0), rng.uniform(0.003, 0.01), rng.uniform(0.003, 0.01),
                            rng, 6, 1, 0.5, off=0.0))
    G.build('stehleiter', angle=40.0)


def build_cable_reel():
    """Hoelzerne Kabeltrommel (D 600 mm) auf der Kante, mit Keilen gesichert: Flansche aus Brettern (Fugen, Schablonen-
    Pfeil fuer die Abrollrichtung), vier Zuganker mit Unterlegplatten und Muttern, Achsloch mit Stahlbuchse, Wicklung
    (Lagen als Wellen), loses Kabelende mit Stecker am Boden; vergrautes Holz, Schmutz unten."""
    rng = random.Random(107)
    C = M_common()
    G = Geo()
    wood = mat('trommel_holz', (0.40, 0.36, 0.30), 'concrete', r=1.4)
    wood_d = mat('trommel_fuge', (0.20, 0.17, 0.13), 'concrete', r=1.4)
    cable = mat('kabel_orange', (0.55, 0.31, 0.11), 'rubber', r=1.15)
    R, zc = 0.30, 0.30
    hw = 0.16
    for s in (-1, 1):
        y0, y1 = s * (hw - 0.025), s * hw
        Mf = along((0, y0, zc), (0, y1, zc))
        G.add(wood, lathe([(0.0, 0.0), (R - 0.004, 0.0), (R, 0.004), (R, 0.021), (R - 0.004, 0.025), (0.0, 0.025)], 32), Mf)
        for k in (-1, 1):  # Brettfugen auf der Aussenseite
            x = k * 0.10
            hh = math.sqrt(R * R - x * x)
            G.add(wood_d, box((x - 0.002, -0.0006, -hh + 0.005), (x + 0.002, 0.0006, hh - 0.005)), Matrix.Translation((0, y1 + s * 0.0005, zc)))
        G.add(C['hole'], lathe([(0.04, 0.0), (0.0, 0.0)], 14), along((0, y1 + s * 0.0006, zc), (0, y1 + s * 1, zc)))
        G.add(C['steel_dark'], lathe([(0.055, 0.0), (0.04, 0.0)], 14), along((0, y1 + s * 0.0008, zc), (0, y1 + s * 1, zc)))
        for k in range(4):  # Zuganker
            a = TAU * (k + 0.5) / 4
            p = Vector((math.cos(a) * 0.14, y1, zc + math.sin(a) * 0.14))
            G.add(C['rust_dark'], box((-0.022, 0.0, -0.022), (0.022, s * 0.004, 0.022)), Matrix.Translation(p))
            bolt(G, C['rust_dark'], p + Vector((0, s * 0.004, 0)), (0, s, 0), 0.011, head_h=0.008, stub=0.004, washer=False)
            G.add(C['rust_stain'], orient(streak_uv(lambda u, v, s=s: (Vector((u, y1 + s * 0.0009, v)), Vector((0, s, 0))), p.x, p.z - 0.02, 0.012, 0.06, rng, 4),
                                          lambda m, s=s: Vector((0, s, 0))))
        # Schablonen-Pfeil (Abrollrichtung) und Zeilen
        sten = mat('schablone', (0.10, 0.10, 0.10), 'paint', r=1.2)
        arc = [Vector((math.cos(a) * 0.22, y1 + s * 0.0008, zc + math.sin(a) * 0.22)) for a in [math.radians(40 + i * 12) for i in range(8)]]
        G.add(sten, sweep(arc, [(0.0, -0.008), (0.0005, -0.008), (0.0005, 0.008), (0.0, 0.008)], up=(0, s, 0)))
        tip = arc[-1]
        G.add(sten, prism([(-0.025, 0.0), (0.025, 0.0), (0.0, 0.035)], 0.0, 0.0006), Matrix.Translation(tip) @ along((0, 0, 0), (0, s, 0), up=(0, 0, 1)).to_3x3().to_4x4() @
              Matrix.Rotation(math.radians(-124 * s), 4, 'Z'))
        for i in range(3):
            G.add(sten, box((-0.08, -0.0003, -0.008), (0.03 + 0.03 * i, 0.0003, 0.008)), Matrix.Translation((0.0, y1 + s * 0.0008, zc - 0.20 + i * 0.03)))
        G.add(C['grime'], orient(strip_uv(lambda u, v, s=s: (Vector((u, y1 + s * 0.0012, v)), Vector((0, s, 0))), -0.25, 0.25, 0.0, 0.08, rng, 12, 0.7),
                                 lambda m, s=s: Vector((0, s, 0))))
    # Wicklung: Lagen als Wellen
    prof = []
    nturn = 12
    y0, y1 = -(hw - 0.025), hw - 0.025
    rw = 0.235
    prof.append((0.12, y0))
    for i in range(nturn * 2 + 1):
        y = y0 + (y1 - y0) * i / (nturn * 2)
        prof.append((rw + (0.009 if i % 2 else 0.0), y))
    prof.append((0.12, y1))
    vs, fs = lathe([(r_, y) for r_, y in prof], 30)
    G.add(cable, (vs, fs), Matrix.Translation((0, 0, zc)) @ Matrix.Rotation(-math.pi / 2, 4, 'X'))
    # loses Kabelende mit Stecker
    a0 = math.radians(-40)
    p0 = Vector((math.cos(a0) * (rw + 0.008), 0.05, zc + math.sin(a0) * (rw + 0.008)))
    path = [p0, Vector((0.25, 0.06, 0.06)), Vector((0.285, 0.0, 0.012)), Vector((0.24, -0.12, 0.012)), Vector((0.08, -0.14, 0.012))]
    G.add(cable, tube(fillet(path, 0.05, 3), 0.011, 8))
    plug = mat('stecker_blau', (0.12, 0.28, 0.55), 'plastic', r=0.9)
    G.add(plug, lathe([(0.0, 0.0), (0.02, 0.0), (0.024, 0.03), (0.024, 0.07), (0.02, 0.075), (0.0, 0.075)], 12), along((0.08, -0.14, 0.024), (0.0, -0.14, 0.024)))
    # Keile
    for sx in (-1, 1):
        for s in (-1, 1):
            G.add(wood, prism([(0.0, 0.0), (0.06, 0.0), (0.06, 0.04)], -0.02, 0.02),
                  Matrix.Translation((sx * 0.10, s * (hw - 0.012), 0.0)) @ Matrix.Rotation(0.0 if sx > 0 else math.pi, 4, 'Z') @ PXZ)
    G.build('kabeltrommel', angle=40.0)


def build_folding_chair():
    """Stahlrohr-Klappstuhl: Seitenrahmen (Vorderbein und Rueckenholm aus einem gebogenen Rohr), Hinterbeine mit
    Gelenknieten, Querrohre, gepresste Sitzschale mit umgebordeltem Rand, gewoelbte Rueckenlehne, Kunststoffkappen;
    Abplatzer an Sitzkante und Lehne, Rost an den Fuessen, Kratzer, Delle."""
    rng = random.Random(109)
    C = M_common()
    G = Geo()
    frame = mat('stuhl_rahmen', (0.30, 0.28, 0.25), 'paint', r=1.0)
    seat = mat('stuhl_sitz', (0.44, 0.41, 0.35), 'paint', r=1.1)
    r = 0.011
    for s in (-1, 1):
        ys = s * 0.19
        A = [(0.20, ys, 0.012), (-0.12, ys, 0.60), (-0.205, ys * 0.98, 0.86)]
        G.add(frame, tube(A, r, 8, bend=0.08, bseg=4))
        B = [(-0.21, s * 0.172, 0.012), (0.15, s * 0.172, 0.43)]
        G.add(frame, tube(B, r, 8))
        for p in (A[0], B[0]):
            G.add(C['black'], lathe([(0.0, -0.012), (0.0125, -0.012), (0.0125, 0.02), (0.0, 0.02)], 8), Matrix.Translation(p))
            d = (Vector(A[1]) - Vector(A[0])).normalized() if p == A[0] else (Vector(B[1]) - Vector(B[0])).normalized()
            G.add(C['rust'], cyl(r + 0.0012, 0.035, 8, cap=False), along(Vector(p) + d * 0.02, Vector(p) + d * 0.055))
        piv = Vector((-0.005, s * 0.181, 0.20))
        rivet(G, C['steel'], piv + Vector((0, s * 0.011, 0)), (0, s, 0), 0.006)
        rivet(G, C['steel'], Vector((0.15, s * 0.161, 0.43)), (0, -s, 0), 0.006)
    G.add(frame, tube([(0.175, -0.19, 0.06), (0.175, 0.19, 0.06)], 0.008, 8))
    G.add(frame, tube([(-0.18, -0.172, 0.07), (-0.18, 0.172, 0.07)], 0.008, 8))
    # Sitzschale mit Bordrand (leicht nach hinten geneigt)
    Ms = Matrix.Translation((-0.01, 0.0, 0.445)) @ Matrix.Rotation(-0.05, 4, 'Y')
    G.add(seat, prism(rrect(0.38, 0.37, 0.04, 3), 0.0, 0.004), Ms)
    rim = [Vector((x, y, 0.0)) for x, y in rrect(0.38, 0.37, 0.04, 3)]
    rim.append(rim[0])
    G.add(seat, sweep(rim, [(0.0, -0.02), (0.004, -0.02), (0.004, 0.004), (0.0, 0.004)], caps=False, up=(0, 0, 1)), Ms)
    for s in (-1, 1):
        G.add(frame, box((-0.17, s * 0.172 - 0.004, -0.02), (0.17, s * 0.172 + 0.004, 0.0)), Ms)
    # Rueckenlehne: gewoelbtes Blech
    arc = [Vector((-0.19 - 0.6 + 0.6 * math.cos(a), 0.6 * math.sin(a), 0.0)) for a in [math.radians(-17.6 + i * 4.4) for i in range(9)]]
    for s in (-1, 1):
        for zz in (0.68, 0.82):
            G.add(frame, box((-0.012, -0.004, -0.012), (0.012, 0.004, 0.012), bev=0.002), Matrix.Translation((-0.199, s * 0.180, zz)))
            rivet(G, C['steel'], (-0.188, s * 0.180, zz), (1, 0, 0), 0.004)
    G.add(seat, sweep(arc, [(0.0, -0.0015), (0.18, -0.0015), (0.18, 0.0015), (0.0, 0.0015)], up=(0, 0, 1)), Matrix.Translation((0.0, 0.0, 0.66)))
    # Abplatzer an Sitzvorderkante und Lehne, Rost an den Fuessen, Kratzer auf dem Sitz
    for _ in range(7):
        x = rng.uniform(-0.17, 0.17)
        G.add(C['steel_dark'], blob((0.18, x, 0.0045), (0, 1, 0), (-1, 0, 0), rng.uniform(0.006, 0.02), rng.uniform(0.004, 0.01), rng, 8, 1, 0.5, off=0.0005), Ms)
    for _ in range(6):
        G.add(C['steel_dark'], blob((rng.uniform(-0.15, 0.15), rng.uniform(-0.15, 0.15), 0.0045), (1, 0, 0), (0, 1, 0), rng.uniform(0.02, 0.06), 0.0012, rng, 6, 1, 0.3,
                                    off=0.0005, rot=rng.uniform(-1, 1)), Ms)
    for _ in range(5):
        a = math.radians(rng.uniform(-14, 14))
        z = rng.choice((0.002, 0.178, rng.uniform(0.02, 0.16)))
        p = Vector((-0.19 - 0.6 + 0.6 * math.cos(a) + 0.0017 * math.cos(a), 0.6 * math.sin(a), 0.66 + z))
        G.add(C['steel_dark'], blob(p, Vector((-math.sin(a), math.cos(a), 0)), (0, 0, 1), rng.uniform(0.006, 0.018), rng.uniform(0.004, 0.012), rng, 8, 1, 0.5, off=0.0005))
    G.deform(dent_fn((-0.19, 0.08, 0.76), (-1, 0, 0), 0.05, 0.008), mats=[seat])
    G.build('klappstuhl', angle=40.0)


# =====================================================================================================================
# Macchina-Werkstatt (Wikidot Level 2: verlassene Macchina-Bereiche mit Werkzeug und Teilen). Die Karte stellt beide
# Stuecke 0,6 m vor eine Wand und dreht sie so, dass lokal +x zur Wand zeigt: Vorderseite (Arbeitsseite) ist -x.
# =====================================================================================================================
FXN_R = Matrix(((0, 0, -1, 0), (-1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1)))  # lokal x -> -y, y -> +z, z -> -x


def FXN(p, roll=0.0):
    """Lokale xy-Ebene auf eine Flaeche mit Normale -x legen (Vorderseite der Werkstattstuecke)."""
    return Matrix.Translation(Vector(p)) @ FXN_R @ Matrix.Rotation(roll, 4, 'Z')


def _gear_outline(nt, ra, rf_, w0=0.45, w1=0.25):
    pts = []
    for i in range(nt):
        a = TAU * i / nt
        for da, rr in ((-w0, rf_), (-w1, ra), (w1, ra), (w0, rf_)):
            aa = a + da * TAU / nt / 2
            pts.append((rr * math.cos(aa), rr * math.sin(aa)))
    return pts


def _helix(c, axis_len, r, turns, n_per=10):
    pts = []
    n = int(turns * n_per)
    for i in range(n + 1):
        a = TAU * i / n_per
        pts.append(Vector((c[0] + r * math.cos(a), c[1] + r * math.sin(a), c[2] + axis_len * i / n)))
    return pts


def _wrench(L, s):
    """Doppelmaulschluessel als Umriss (Laenge L entlang x, Maulweite s), Mitte im Ursprung."""
    h = s * 0.55
    jaw = s * 1.15
    pts = [(-L / 2 + jaw, -h * 0.5), (L / 2 - jaw, -h * 0.5)]
    # rechtes Maul (offen nach +x, leicht schraeg)
    pts += [(L / 2 - jaw * 0.6, -jaw), (L / 2, -jaw * 0.75), (L / 2, -s * 0.5), (L / 2 - s * 0.7, -s * 0.5), (L / 2 - s * 0.7, s * 0.5),
            (L / 2, s * 0.5), (L / 2, jaw * 0.75), (L / 2 - jaw * 0.6, jaw)]
    pts += [(L / 2 - jaw, h * 0.5), (-L / 2 + jaw, h * 0.5)]
    pts += [(-L / 2 + jaw * 0.6, jaw), (-L / 2, jaw * 0.75), (-L / 2, s * 0.45), (-L / 2 + s * 0.65, s * 0.45), (-L / 2 + s * 0.65, -s * 0.45),
            (-L / 2, -s * 0.45), (-L / 2, -jaw * 0.75), (-L / 2 + jaw * 0.6, -jaw)]
    return pts


def build_macchina_parts():
    """Werkbank der Macchina-Werkstatt (1,5 x 0,7 m, Vorderseite -x): Gussbeine mit Querstreben, dicke Holzplatte mit
    Stahlkante, Ablage mit Teilekiste; darauf ein halb zerlegtes Getriebe (Zahnraeder mit Zaehnen, Welle), eine
    Kurbelwelle, Druckfedern, eine Kupferrohrwendel, ein Zylinder mit Kolben, ein angefangener gelenkiger Messingarm,
    eine Teileschale mit Muttern, eine Oelkanne, ein Lappen und ein Schraubstock; alles verstaubt, Oel auf dem Holz."""
    rng = random.Random(121)
    C = M_common()
    G = Geo()
    iron = mat('guss_werkbank', (0.17, 0.18, 0.18), 'paint', tex='painted_metal_grey', r=1.25)
    top = mat('werkbank_holz', (0.27, 0.21, 0.15), 'wood', r=1.0)
    crate = mat('kiste_holz', (0.34, 0.29, 0.22), 'concrete', r=1.35)
    shelf = mat('ablage_holz', (0.28, 0.24, 0.19), 'concrete', r=1.4)
    brass = mat('messing_alt', (0.55, 0.43, 0.22), 'metal', r=0.9, m=0.7)
    copper = mat('kupfer', (0.58, 0.34, 0.20), 'metal', r=0.9, m=0.7)
    x0, x1, yw, zt = -0.30, 0.40, 0.75, 0.86
    # Gestell: vier Beine (Winkelprofil), Fussplatten, Querstreben, Ablage
    for x in (x0 + 0.04, x1 - 0.04):
        for y in (-yw + 0.06, yw - 0.06):
            G.add(iron, prism([(-0.03, -0.03), (0.03, -0.03), (0.03, -0.024), (-0.024, -0.024), (-0.024, 0.03), (-0.03, 0.03)], 0.0, zt - 0.06),
                  Matrix.Translation((x, y, 0.0)) @ Matrix.Rotation((0 if x < 0 else math.pi / 2) + (0 if y < 0 else -math.pi / 2), 4, 'Z'))
            G.add(iron, box((x - 0.045, y - 0.045, 0.0), (x + 0.045, y + 0.045, 0.008), bev=0.002))
    for y in (-yw + 0.06, yw - 0.06):
        G.add(iron, box((x0 + 0.04, y - 0.015, 0.16), (x1 - 0.04, y + 0.015, 0.20)))
    G.add(iron, box((x1 - 0.06, -yw + 0.06, 0.16), (x1 - 0.03, yw - 0.06, 0.20)))
    G.add(shelf, box((x0 + 0.03, -yw + 0.06, 0.20), (x1 - 0.03, yw - 0.06, 0.218)))
    # Platte mit Stahlkante vorn
    G.add(top, box((x0, -yw, zt - 0.06), (x1, yw, zt), bev=0.004))
    G.add(C['steel_dark'], box((x0 - 0.004, -yw, zt - 0.062), (x0, yw, zt + 0.002), bev=0.001))
    # Teilekiste auf der Ablage mit Zahnraedern/Wellen darin
    G.add(crate, box((-0.15, -0.55, 0.218), (0.25, -0.10, 0.40), bev=0.004))
    G.add(C['hole'], box((-0.13, -0.53, 0.399), (0.23, -0.12, 0.401)))
    for k in range(3):
        G.add(C['rust'], prism(_gear_outline(16, 0.07, 0.062), 0.0, 0.02), mt((0.0 + k * 0.06, -0.33 + k * 0.05, 0.40 + k * 0.005), (0.3 * k, 0.2, 0.0)))
    # Getriebe zerlegt: grosses Zahnrad liegend, Ritzel, Welle, Gehaeusehaelfte
    Mg = Matrix.Translation((0.05, 0.35, zt))
    G.add(brass, prism(_gear_outline(30, 0.15, 0.137), 0.0, 0.03), Mg)
    G.add(brass, lathe([(0.0, 0.03), (0.045, 0.03), (0.045, 0.05), (0.0, 0.05)], 16), Mg)
    G.add(C['hole'], cyl(0.02, 0.0008, 12), Mg @ Matrix.Translation((0, 0, 0.0505)))
    for k in range(5):
        a = TAU * k / 5
        G.add(C['hole'], cyl(0.022, 0.0008, 10), Mg @ Matrix.Translation((math.cos(a) * 0.09, math.sin(a) * 0.09, 0.0302)))
    G.add(brass, prism(_gear_outline(12, 0.05, 0.043), 0.0, 0.03), mt((-0.12, 0.52, zt), (0, 0, 0.3)))
    G.add(C['steel'], cyl(0.016, 0.42, 10), along((-0.18, 0.12, zt + 0.016), (0.20, 0.25, zt + 0.016)))
    housing = mat('getriebe_alt', (0.22, 0.27, 0.25), 'paint', tex='painted_metal_grey', r=1.2)
    G.add(housing, lathe([(0.0, 0.0), (0.12, 0.0), (0.13, 0.02), (0.13, 0.09), (0.12, 0.10), (0.06, 0.10), (0.06, 0.12), (0.0, 0.12)], 20, a0=0.0, a1=math.pi),
          Matrix.Translation((0.20, -0.10, zt)) @ Matrix.Rotation(math.pi / 2, 4, 'Y') @ Matrix.Translation((0, 0, -0.06)))
    for k in range(6):
        a = math.pi * (k + 0.5) / 6
        bolt(G, C['steel_dark'], (0.26, -0.10 + math.cos(a) * 0.115, zt + math.sin(a) * 0.115), (1, 0, 0), 0.006, washer=False)
    # Kurbelwelle (gekroepft) vorn liegend
    cw = [(x0 + 0.06, -0.62, zt + 0.02), (x0 + 0.06, -0.52, zt + 0.02), (x0 + 0.06, -0.50, zt + 0.07), (x0 + 0.06, -0.42, zt + 0.07),
          (x0 + 0.06, -0.40, zt + 0.02), (x0 + 0.06, -0.30, zt + 0.02)]
    G.add(C['steel_dark'], tube(cw, 0.014, 8, bend=0.012, bseg=2))
    for y in (-0.51, -0.41):
        G.add(C['steel_dark'], box((-0.03, -0.012, -0.03), (0.03, 0.012, 0.05)), Matrix.Translation((x0 + 0.06, y, zt + 0.03)))
    # Druckfedern und Kupferwendel
    for k, (x, y) in enumerate(((x0 + 0.12, -0.20), (x0 + 0.20, -0.16))):
        G.add(C['steel'], tube(_helix((0, 0, 0), 0.12, 0.022, 6, 8), 0.0032, 5, caps=True), mt((x, y, zt + 0.025), (0, math.pi / 2, 0.4 * k)))
    G.add(copper, tube(_helix((0, 0, 0), 0.16, 0.05, 4, 10), 0.005, 6), mt((0.28, 0.60, zt + 0.055), (math.pi / 2, 0, 0.0)))
    # Zylinder mit Kolben und Pleuel
    G.add(C['steel_dark'], lathe([(0.0, 0.0), (0.055, 0.0), (0.055, 0.012), (0.045, 0.014), (0.045, 0.13), (0.055, 0.135), (0.055, 0.15), (0.0, 0.15)], 16),
          Matrix.Translation((0.25, 0.08, zt)))
    for k in range(6):
        G.add(C['steel_dark'], lathe([(0.0, 0.0), (0.054, 0.0), (0.054, 0.004), (0.0, 0.004)], 16), Matrix.Translation((0.25, 0.08, zt + 0.025 + k * 0.018)))
    G.add(C['steel'], lathe([(0.0, 0.0), (0.04, 0.0), (0.04, 0.05), (0.0, 0.05)], 14), mt((0.08, -0.25, zt + 0.04), (math.pi / 2, 0, 0.8)))
    G.add(C['steel'], box((-0.008, -0.004, 0.0), (0.008, 0.004, 0.16)), mt((0.10, -0.22, zt + 0.004), (math.pi / 2 - 0.05, 0, 0.8)))
    # angefangener Gelenkarm (Messing): drei Glieder mit Gelenkkugeln, Kabel
    p = Vector((-0.05, 0.05, zt + 0.03))
    d = Vector((0.0, -1.0, 0.0))
    for k in range(3):
        L = [0.22, 0.18, 0.12][k]
        q = p + d * L
        G.add(brass, lathe([(0.0, 0.0), (0.026 - k * 0.004, 0.0), (0.022 - k * 0.004, L), (0.0, L)], 10), along(p, q))
        G.add(brass, dome(0.03 - k * 0.004, 0.06 - k * 0.008, 10), along(p - Vector((0, 0, 0.028 - k * 0.004)), p + Vector((0, 0, 1))))
        a = rng.uniform(-0.5, 0.5)
        d = Matrix.Rotation(a, 3, 'Z') @ d
        p = q
    G.add(C['black'], tube([(-0.05, 0.05, zt + 0.056), (-0.02, -0.12, zt + 0.05), (0.02, -0.30, zt + 0.045), (0.0, -0.45, zt + 0.04)], 0.004, 5, bend=0.05))
    for k in range(3):  # Finger (Greifer)
        a = TAU * k / 3
        G.add(brass, box((-0.004, -0.004, 0.0), (0.004, 0.004, 0.05)), Matrix.Translation(p) @ Matrix.Rotation(a, 4, 'Z') @ Matrix.Rotation(1.1, 4, 'X'))
    # Teileschale mit Muttern
    tray = [(-0.10, -0.06), (0.10, -0.06), (0.10, 0.06), (-0.10, 0.06)]
    G.add(C['zinc'], box((-0.11, 0.48, zt), (0.09, 0.62, zt + 0.004)))
    for (a, b) in (((-0.11, 0.48), (0.09, 0.484)), ((-0.11, 0.616), (0.09, 0.62)), ((-0.11, 0.48), (-0.106, 0.62)), ((0.086, 0.48), (0.09, 0.62))):
        G.add(C['zinc'], box((a[0], a[1], zt), (b[0], b[1], zt + 0.03)))
    for _ in range(14):
        G.add(C['steel_dark'], nut_shape(0.008, 0.007), mt((rng.uniform(-0.09, 0.07), rng.uniform(0.50, 0.60), zt + 0.004), (rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 0)))
    # Oelkanne, Lappen, Schraubstock
    can = mat('oelkanne', (0.48, 0.38, 0.14), 'metal', r=1.0, m=0.5)
    G.add(can, lathe([(0.0, 0.0), (0.07, 0.0), (0.07, 0.035), (0.03, 0.07), (0.012, 0.08), (0.0, 0.08)], 14), Matrix.Translation((x0 + 0.12, 0.62, zt)))
    G.add(can, tube([(x0 + 0.12, 0.62, zt + 0.07), (x0 + 0.08, 0.66, zt + 0.16), (x0 + 0.02, 0.69, zt + 0.21)], 0.004, 6))
    rag = mat('lappen', (0.42, 0.36, 0.30), 'fabric', r=1.5)
    vs, fs = [], []
    for j in range(5):
        for i in range(5):
            x, y = -0.08 + 0.04 * i, -0.08 + 0.04 * j
            vs.append(Vector((x, y, 0.004 + 0.012 * abs(nz((x, y, 0), 25.0)))))
    for j in range(4):
        for i in range(4):
            a = j * 5 + i
            fs.append((a, a + 1, a + 6, a + 5))
    G.add(rag, (vs, fs), mt((x0 + 0.15, 0.30, zt), (0, 0, 0.4)))
    vise = mat('schraubstock', (0.20, 0.24, 0.21), 'paint', r=1.15)
    Mv = Matrix.Translation((x0 + 0.02, -0.66, zt))
    G.add(vise, box((0.0, -0.06, 0.0), (0.20, 0.06, 0.05), bev=0.006), Mv)
    G.add(vise, box((0.14, -0.065, 0.05), (0.20, 0.065, 0.13), bev=0.005), Mv)
    G.add(vise, box((0.0, -0.065, 0.05), (0.06, 0.065, 0.13), bev=0.005), Mv)
    for xx in (0.06, 0.14):
        G.add(C['steel_dark'], box((xx - 0.004 if xx > 0.1 else xx, -0.06, 0.09), (xx if xx > 0.1 else xx + 0.004, 0.06, 0.13)), Mv)
    G.add(C['steel'], cyl(0.009, 0.16, 8), Mv @ along((-0.10, 0, 0.08), (0.06, 0, 0.08)))
    G.add(C['steel'], cyl(0.006, 0.16, 6), Mv @ along((-0.10, -0.08, 0.08), (-0.10, 0.08, 0.08)))
    # Staub und Oel
    dustw = mat('staub_holz', (0.36, 0.32, 0.27), 'concrete', r=1.5)
    for _ in range(6):
        G.add(dustw, blob((rng.uniform(x0 + 0.05, x1 - 0.05), rng.uniform(-0.7, 0.7), zt + 0.0006), (1, 0, 0), (0, 1, 0), rng.uniform(0.04, 0.10),
                          rng.uniform(0.03, 0.08), rng, 12, 2, 0.4, off=0.0))
    for _ in range(4):
        G.add(C['oil'], blob((rng.uniform(x0 + 0.05, x1 - 0.05), rng.uniform(-0.7, 0.7), zt + 0.0009), (1, 0, 0), (0, 1, 0), rng.uniform(0.03, 0.08),
                             rng.uniform(0.02, 0.06), rng, 12, 2, 0.5, off=0.0))
    G.build('macchina_teile', angle=40.0)


def build_macchina_tools():
    """Werkzeugwand der Macchina-Werkstatt (Vorderseite -x): Schubladenschrank (eine Lade offen), Stahlrahmen mit
    Lochplatte, aufgemalte Schattenumrisse, an Haken: Maulschluessel in Groessen, Hammer, Faeustel, Saege mit Zahnung,
    Schraubendreher, Zange, Rohrzange, Kabelrolle; fehlende Werkzeuge nur als Umriss; verstaubt, Rost."""
    rng = random.Random(123)
    C = M_common()
    G = Geo()
    cab = mat('werkzeugschrank', (0.22, 0.27, 0.24), 'paint', r=1.15)
    board = mat('lochplatte', (0.36, 0.32, 0.26), 'concrete', r=1.4)
    shadow = mat('schattenumriss', (0.12, 0.12, 0.11), 'paint', r=1.3)
    wood = mat('stiel_holz', (0.50, 0.38, 0.25), 'wood', r=1.1)
    x0, x1, yw = -0.25, 0.45, 0.60
    # Schubladenschrank
    G.add(cab, box((x0 + 0.02, -yw, 0.0), (x1, yw, 0.82), bev=0.006, seg=2))
    for k in range(4):
        z0, z1 = 0.06 + k * 0.19, 0.06 + k * 0.19 + 0.17
        open_ = (k == 1)
        dx = -0.18 if open_ else 0.0
        G.add(cab, box((x0 + dx, -yw + 0.03, z0), (x0 + 0.02 + dx, yw - 0.03, z1), bev=0.003))
        G.add(C['steel'], box((x0 - 0.012 + dx, -0.18, z1 - 0.05), (x0 + dx, 0.18, z1 - 0.035), bev=0.002))
        G.add(C['paper'], box((x0 - 0.0006 + dx, 0.24, z1 - 0.06), (x0 + dx, 0.34, z1 - 0.03)))
        if open_:  # offene Lade: Seitenwaende, Boden, Werkzeug darin
            for s in (-1, 1):
                G.add(cab, box((x0 - 0.18, s * (yw - 0.03) - (0.01 if s > 0 else 0), z0 + 0.01), (x0 + 0.02, s * (yw - 0.03) + (0.0 if s > 0 else 0.01), z1 - 0.02)))
            G.add(cab, box((x0 - 0.18, -yw + 0.03, z0 + 0.01), (x0 + 0.02, yw - 0.03, z0 + 0.02)))
            for i in range(5):
                G.add(C['steel'], prism(_wrench(0.16 + 0.02 * i, 0.012 + 0.003 * i), 0.0, 0.005), mt((x0 - 0.08, -0.45 + i * 0.18, z0 + 0.021), (0, 0, 1.5 + rng.uniform(-0.2, 0.2))))
            for i in range(3):
                G.add(mat('griff_rot', (0.55, 0.10, 0.07), 'plastic', r=0.9), lathe([(0, 0), (0.012, 0), (0.014, 0.08), (0.01, 0.1), (0, 0.1)], 8),
                      mt((x0 - 0.12, 0.25 + i * 0.07, z0 + 0.035), (0, math.pi / 2, 0.1)))
    G.add(cab, box((x0 + 0.02, -yw - 0.01, 0.82), (x1, yw + 0.01, 0.85), bev=0.004))
    # Rahmen und Lochplatte (Vorderseite bei x = 0.38)
    xb = 0.38
    for s in (-1, 1):
        G.add(C['steel_dark'], box((xb, s * yw - (0.03 if s > 0 else 0), 0.85), (xb + 0.04, s * yw + (0.0 if s > 0 else 0.03), 1.92), bev=0.003))
    G.add(C['steel_dark'], box((xb, -yw, 1.88), (xb + 0.04, yw, 1.92), bev=0.003))
    G.add(board, box((xb + 0.01, -yw + 0.03, 0.87), (xb + 0.016, yw - 0.03, 1.88)))
    hv, hf = [], []
    for j in range(16):
        for i in range(30):
            y = -yw + 0.06 + i * (2 * yw - 0.12) / 29
            z = 0.92 + j * 0.06
            b = len(hv)
            hv += [Vector((xb + 0.0094, y + 0.003, z - 0.003)), Vector((xb + 0.0094, y - 0.003, z - 0.003)), Vector((xb + 0.0094, y - 0.003, z + 0.003)),
                   Vector((xb + 0.0094, y + 0.003, z + 0.003))]
            hf.append((b, b + 1, b + 2, b + 3))
    G.add(C['hole'], orient((hv, hf), lambda m: Vector((-1, 0, 0))))
    xs = xb + 0.0092  # Werkzeug vor der Platte

    def hook(y, z):
        G.add(C['steel'], tube([(xb + 0.01, y, z), (xb - 0.03, y, z), (xb - 0.04, y, z + 0.015)], 0.0025, 4, bend=0.008, bseg=2))
    # Maulschluessel (vier), dazu ein fehlender als Umriss
    for i in range(5):
        L, s = 0.14 + 0.03 * i, 0.010 + 0.003 * i
        y, z = -0.50 + i * 0.075, 1.45
        if i == 2:
            G.add(shadow, prism(_wrench(L * 1.08, s * 1.15), 0.0, 0.0005), FXN((xs, y, z), roll=math.pi / 2))
            continue
        G.add(shadow, prism(_wrench(L * 1.08, s * 1.15), 0.0, 0.0005), FXN((xs, y, z), roll=math.pi / 2))
        G.add(C['steel'], prism(_wrench(L, s), 0.0, 0.005), FXN((xs - 0.0006, y, z), roll=math.pi / 2 + rng.uniform(-0.05, 0.05)))
        hook(y, z + L / 2 - 0.01)
    # Hammer und Faeustel (Faeustel fehlt: nur Umriss)
    for k, (y, hl, hw) in enumerate(((0.05, 0.30, 0.10), (0.25, 0.26, 0.12))):
        z = 1.40
        G.add(shadow, prism([(-0.012, -hl / 2), (0.012, -hl / 2), (0.012, hl / 2 - 0.02), (hw / 2, hl / 2 - 0.02), (hw / 2, hl / 2 + 0.02), (-hw / 2, hl / 2 + 0.02),
                             (-hw / 2, hl / 2 - 0.02), (-0.012, hl / 2 - 0.02)], 0.0, 0.0005), FXN((xs, y, z)))
        if k == 1:
            continue
        G.add(wood, lathe([(0.0, 0.0), (0.012, 0.0), (0.014, hl * 0.6), (0.011, hl), (0.0, hl)], 8), along((xs - 0.016, y, z - hl / 2), (xs - 0.016, y, z + hl / 2 - 0.02)))
        G.add(C['steel_dark'], box((-0.016, -hw / 2, -0.018), (0.016, hw / 2, 0.018), bev=0.004), Matrix.Translation((xs - 0.018, y, z + hl / 2)))
        hook(y + 0.03, z + hl / 2 - 0.05)
        hook(y - 0.03, z + hl / 2 - 0.05)
    # Saege mit Zahnung
    blade = [(-0.0, 0.0)]
    n = 24
    Lb = 0.42
    for i in range(n + 1):
        blade.append((Lb * i / n, -0.004 if i % 2 else 0.0))
    blade = [(0.0, 0.0)] + [(x, y) for x, y in blade[1:]] + [(Lb, 0.06), (0.0, 0.11)]
    G.add(C['steel'], prism(blade, 0.0, 0.0012), FXN((xs - 0.001, -0.08, 1.08)))
    G.add(wood, prism([(-0.13, 0.0), (0.0, 0.0), (0.0, 0.12), (-0.13, 0.12), (-0.15, 0.06)], 0.0, 0.022), FXN((xs - 0.002, -0.08, 1.06)))
    hook(-0.30, 1.17)
    # Schraubendreher (Reihe), Zange, Rohrzange, Kabelrolle
    for i in range(6):
        y = 0.17 + i * 0.05
        G.add(mat(f'griff{i % 3}', [(0.55, 0.10, 0.07), (0.70, 0.55, 0.10), (0.10, 0.25, 0.55)][i % 3], 'plastic', r=0.9),
              lathe([(0, 0), (0.012, 0), (0.014, 0.07), (0.01, 0.1), (0, 0.1)], 8), along((xs - 0.02, y, 1.20), (xs - 0.02, y, 1.30)))
        G.add(C['steel'], cyl(0.003, 0.08 + 0.02 * (i % 2), 5), along((xs - 0.02, y, 1.20), (xs - 0.02, y, 1.12 - 0.02 * (i % 2))))
    G.add(C['steel_dark'], box((xs - 0.015, 0.14, 1.30), (xs - 0.005, 0.46, 1.31)))
    for s in (-1, 1):
        G.add(C['steel_dark'], prism([(-0.008, 0.0), (0.008, 0.0), (0.012, 0.15), (-0.004, 0.15)], 0.0, 0.006), FXN((xs - 0.002 - (0.006 if s > 0 else 0), 0.53, 0.92), roll=s * 0.12))
    G.add(C['steel_dark'], cyl(0.006, 0.02, 8), along((xs - 0.002, 0.53, 1.06), (xs - 0.02, 0.53, 1.06)))
    G.add(C['red'], prism([(-0.01, 0.0), (0.01, 0.0), (0.012, 0.32), (-0.012, 0.32)], 0.0, 0.012), FXN((xs - 0.002, 0.40, 1.48)))
    G.add(C['steel_dark'], prism([(-0.02, 0.32), (0.05, 0.32), (0.05, 0.40), (-0.02, 0.40)], 0.0, 0.016), FXN((xs - 0.002, 0.40, 1.48)))
    G.add(mat('kabel_schwarz', (0.08, 0.08, 0.08), 'rubber', r=1.2), torus(0.09, 0.012, 16, 6, sz=1.6), FXN((xs - 0.03, -0.20, 1.70)))
    hook(-0.20, 1.80)
    # Staub auf dem Schrank, Rost am Sockel, Abplatzer
    dustm = mat('staub_schrank', (0.30, 0.31, 0.28), 'concrete', r=1.5)
    for _ in range(4):
        G.add(dustm, blob((rng.uniform(x0 + 0.05, xb - 0.03), rng.uniform(-0.55, 0.55), 0.8506), (1, 0, 0), (0, 1, 0), rng.uniform(0.05, 0.10), rng.uniform(0.04, 0.08),
                          rng, 12, 2, 0.4, off=0.0))
    chips_on_plane(G, C['steel_dark'], (x0 - 0.0009, 0.0, 0.42), (0, -1, 0), (0, 0, 1), 2 * yw - 0.04, 0.8, rng, 8, 0.004, 0.012)
    Sf = lambda u, v: (Vector((x0 - 0.0009, -u, v)), Vector((-1, 0, 0)))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(Sf, -yw + 0.02, yw - 0.02, 0.004, 0.04, rng, 14, 0.8), lambda m: Vector((-1, 0, 0))))
    G.build('macchina_werkzeug', angle=40.0)


# =====================================================================================================================
# V13.3 Nachforderung: Bauarten aus verschiedenen Epochen (Varianten <name>@N, die Karte waehlt zufaellig) - Wiki:
# Maschinen "verschiedener Bauarten aus frueheren Epochen, willkuerlich gesetzt, nicht aufeinander abgestimmt"
# =====================================================================================================================
ARROW = [(-0.006, 0.045), (0.010, 0.045), (0.002, 0.006), (0.012, 0.006), (-0.008, -0.045), (-0.001, -0.004), (-0.012, -0.004)]
NX = lambda m: Vector((1, 0, 0))  # noqa: E731


def _SX(x):
    """Ebene x = const (Normale +x) fuer Flecken: u = y, v = z."""
    return lambda u, v: (Vector((x, u, v)), Vector((1, 0, 0)))


def rv(G, matl, p, axis, r=0.006):
    """Nietkopf (sparsam: fuenfseitig, ohne Boden)."""
    p = Vector(p)
    G.add(matl, lathe([(r, 0.0), (r * 0.72, r * 0.45), (0.0, r * 0.62)], 5), along(p, p + Vector(axis)))


def rv_row(G, matl, a, b, axis, step=0.08, r=0.006):
    """Nietreihe von a nach b (beide Enden eingeschlossen)."""
    a, b = Vector(a), Vector(b)
    n = max(1, int(round((b - a).length / step)))
    for i in range(n + 1):
        rv(G, matl, a + (b - a) * (i / n), axis, r)


def frame_uvn(o, U, V):
    """Matrix fuer schraege Platten: lokal x -> U, y -> V, z -> U x V (Normale), Ursprung o."""
    U, V = Vector(U).normalized(), Vector(V).normalized()
    N = U.cross(V).normalized()
    V = N.cross(U).normalized()
    M = Matrix((U, V, N)).transposed().to_4x4()
    M.translation = Vector(o)
    return M


def round_poly(pts, rad, k=3):
    """Polygon mit gerundeten Ecken (quadratische Bezierkurve je Ecke); rad = Radius (Zahl oder Liste je Ecke)."""
    n = len(pts)
    rads = list(rad) if isinstance(rad, (list, tuple)) else [rad] * n
    out = []
    for i in range(n):
        p0, p1, p2 = Vector(pts[i - 1]), Vector(pts[i]), Vector(pts[(i + 1) % n])
        r = rads[i]
        if r <= 0:
            out.append((p1.x, p1.y))
            continue
        d0, d2 = (p0 - p1).normalized(), (p2 - p1).normalized()
        ang = math.acos(max(-1.0, min(1.0, d0.dot(d2))))
        t = min(r / max(1e-6, math.tan(ang / 2)), (p0 - p1).length * 0.45, (p2 - p1).length * 0.45)
        a, b = p1 + d0 * t, p1 + d2 * t
        for j in range(k + 1):
            s = j / k
            q = a * ((1 - s) ** 2) + p1 * (2 * s * (1 - s)) + b * (s * s)
            out.append((q.x, q.y))
    return out


def _stroke(G, matl, a, b, w=0.0022, t=0.0003):
    """Strich (Stift, Riss) auf einer Flaeche mit Normale +x von a nach b."""
    A, B = Vector(a), Vector(b)
    if (B - A).length < 1e-5:
        return
    G.add(matl, box((-w / 2, -t / 2, 0.0), (w / 2, t / 2, (B - A).length)), along(A, B, up=(1, 0, 0)))


# ---------------------------------------------------------------------------------------------------------------------
# Schaltschraenke: v0 (1990er, grau, Filterluefter) oben; v1 1950er Schalttafel genietet; v2 1970er Steuerpult beige
# ---------------------------------------------------------------------------------------------------------------------
def build_mach_cabinet__v1():
    """1950er Schalttafel aus genietetem Stahlblech (Wand bei x = 0): Unterschrank mit Tuer (gestanzte
    Lueftungsschlitze, Knebelgriff, Schluesselschild, Emailleschild Hochspannung), schraeges Pult mit offenem
    dreipoligem Messerschalter auf Schieferplatte, zurueckgesetzte Tafel mit drei runden Zeigerinstrumenten
    (Bakelitgehaeuse, Chromring, Messingschild), zwei Paketschaltern und Meldelampen, Pruefkarte, Gesims oben;
    Winkeleisen an den Kanten mit Nietreihen; Rost am Fuss, Laeufer unter den Nieten, Griffspuren, Abplatzer."""
    rng = random.Random(42)
    C = M_common()
    G = Geo()
    X = Vector((1, 0, 0))
    paint = mat('tafel_lack', (0.13, 0.18, 0.15), 'paint', r=1.2)
    iron = mat('winkeleisen', (0.12, 0.13, 0.12), 'paint', r=1.25)
    bake = mat('bakelit_schwarz', (0.035, 0.03, 0.028), 'plastic', r=0.55)
    face = mat('skala_alt', (0.80, 0.76, 0.62), 'paint', r=0.9)
    slate = mat('schiefer', (0.10, 0.11, 0.12), 'ceramic', r=1.0)
    copper = mat('kupfer_blank', (0.56, 0.31, 0.18), 'metal', r=0.8, m=0.85)
    W = 0.42
    D0, Z0, Z1 = 0.52, 0.075, 0.94   # Unterschrank
    D1, Z2, Z3 = 0.32, 1.16, 1.98    # Tafel
    G.add(iron, box((0.02, -W + 0.02, 0.0), (D0 - 0.03, W - 0.02, Z0)))  # Sockel, zurueckgesetzt
    G.add(paint, prism([(0.0, Z0), (D0, Z0), (D0, Z1), (D1, Z2), (D1, Z3), (0.0, Z3)], -W, W), PXZ)
    cor = [(0.0, Z3), (D1 + 0.03, Z3), (D1 + 0.03, Z3 + 0.015), (D1 + 0.012, Z3 + 0.03), (D1 + 0.012, Z3 + 0.05),
           (D1 + 0.042, Z3 + 0.066), (D1 + 0.042, Z3 + 0.09), (0.0, Z3 + 0.09)]
    G.add(paint, prism(cor, -W - 0.022, W + 0.022), PXZ)  # Gesims

    def angle_v(xc, yc, sy, z0, z1, leg=0.035, t=0.004):
        poly = [(xc, yc - sy * leg), (xc + t, yc - sy * leg), (xc + t, yc + sy * t), (xc - leg, yc + sy * t), (xc - leg, yc), (xc, yc)]
        G.add(iron, prism(poly, z0, z1))
    for sy in (-1, 1):  # Winkeleisen an den Vorderkanten, Nietreihen
        angle_v(D0, sy * W, sy, Z0, Z1)
        angle_v(D1, sy * W, sy, Z2, Z3)
        rv_row(G, iron, (D0 + 0.004, sy * (W - 0.018), Z0 + 0.045), (D0 + 0.004, sy * (W - 0.018), Z1 - 0.05), (1, 0, 0), 0.10)
        rv_row(G, iron, (D1 + 0.004, sy * (W - 0.018), Z2 + 0.05), (D1 + 0.004, sy * (W - 0.018), Z3 - 0.045), (1, 0, 0), 0.10)
    for (x, z0, z1) in ((D0, Z0, Z0 + 0.035), (D0, Z1 - 0.035, Z1), (D1, Z2, Z2 + 0.035), (D1, Z3 - 0.035, Z3)):
        G.add(iron, box((x, -W + 0.035, z0), (x + 0.004, W - 0.035, z1)))
        rv_row(G, iron, (x + 0.004, -W + 0.10, (z0 + z1) / 2), (x + 0.004, W - 0.10, (z0 + z1) / 2), (1, 0, 0), 0.13)
    # Tuer des Unterschranks
    xd = D0 + 0.004
    dy0, dy1, dz0, dz1 = -W + 0.045, W - 0.045, Z0 + 0.05, Z1 - 0.05
    G.add(C['black'], box((xd - 0.002, dy0 - 0.004, dz0 - 0.004), (xd + 0.001, dy1 + 0.004, dz1 + 0.004)))
    G.add(paint, box((xd, dy0, dz0), (xd + 0.014, dy1, dz1), bev=0.004, seg=1))
    xf = xd + 0.014
    for z in (dz0 + 0.12, dz1 - 0.12):  # Baender
        G.add(iron, cyl(0.008, 0.07, 8), along((xf - 0.004, dy0 - 0.006, z - 0.035), (xf - 0.004, dy0 - 0.006, z + 0.035)))
    for k in range(5):  # gestanzte Lueftungsschlitze
        z = dz0 + 0.06 + k * 0.032
        G.add(C['hole'], box((xf - 0.0005, -0.22, z), (xf + 0.0004, 0.22, z + 0.010)))
        G.add(paint, box((xf, -0.22, z + 0.010), (xf + 0.008, 0.22, z + 0.0135)))
    hy, hz = dy1 - 0.07, 0.56  # Knebelgriff mit Rosette, Schluesselschild
    G.add(C['steel'], lathe([(0, 0), (0.026, 0), (0.026, 0.004), (0.02, 0.008), (0, 0.008)], 12), along((xf, hy, hz), (xf + 1, hy, hz)))
    G.add(C['steel'], cyl(0.007, 0.03, 8), along((xf + 0.008, hy, hz), (xf + 0.038, hy, hz)))
    G.add(C['steel'], box((-0.011, -0.045, -0.006), (0.011, 0.045, 0.006), bev=0.004), Matrix.Translation((xf + 0.044, hy, hz)) @ Matrix.Rotation(0.5, 4, 'X'))
    G.add(C['brass'], prism(rrect(0.022, 0.04, 0.008, 2), 0.0, 0.002), FX((xf, hy, hz - 0.08)))
    G.add(C['hole'], box((xf + 0.002, hy - 0.002, hz - 0.09), (xf + 0.0025, hy + 0.002, hz - 0.07)))
    enam = mat('emaille_gelb', (0.80, 0.62, 0.12), 'ceramic', r=0.6)  # Emailleschild Hochspannung
    sy_, sz_ = -0.03, 0.74
    G.add(C['black'], box((xf, sy_ - 0.10, sz_ - 0.06), (xf + 0.0015, sy_ + 0.10, sz_ + 0.06), bev=0.0005))
    G.add(enam, box((xf + 0.0015, sy_ - 0.094, sz_ - 0.054), (xf + 0.0022, sy_ + 0.094, sz_ + 0.054)))
    G.add(C['red'], prism([(x - 0.058, y) for x, y in ARROW], 0.0022, 0.0026), FX((xf, sy_, sz_)))
    for i, w in enumerate((0.11, 0.10)):
        G.add(C['ink'], box((xf + 0.0022, sy_ - 0.025, sz_ + 0.012 - i * 0.026), (xf + 0.0026, sy_ - 0.025 + w, sz_ + 0.022 - i * 0.026)))
    for sy in (-1, 1):
        rv(G, C['steel'], (xf + 0.0022, sy_ + sy * 0.087, sz_), (1, 0, 0), 0.003)
    # Pult: Schieferplatte mit offenem Messerschalter
    V = Vector((D1 - D0, 0.0, Z2 - Z1)).normalized()
    Ms = frame_uvn(((D0 + D1) / 2, 0.0, (Z1 + Z2) / 2), (0, 1, 0), V)
    G.add(slate, box((-0.36, -0.12, 0.0), (0.36, 0.12, 0.016), bev=0.003), Ms)
    for u in (-0.33, 0.33):
        for v in (-0.10, 0.10):
            screw_head(G, C['steel'], Ms @ Vector((u, v, 0.016)), Ms.to_3x3() @ Vector((0, 0, 1)), 0.005, kind='flat', seg=6)
    th = math.radians(58)
    for u in (-0.17, 0.0, 0.17):
        for v0 in (-0.075, 0.085):  # Gelenk und Kontaktklemme (zwei Wangen)
            for s in (-1, 1):
                G.add(copper, box((u + s * 0.0065 - 0.0025, v0 - 0.014, 0.016), (u + s * 0.0065 + 0.0025, v0 + 0.014, 0.058)), Ms)
            G.add(C['brass'], cyl(0.007, 0.005, 6), Ms @ Matrix.Translation((u, v0 + (0.032 if v0 > 0 else -0.032), 0.016)))
        G.add(copper, box((-0.0028, -0.01, -0.011), (0.0028, 0.175, 0.011)), Ms @ Matrix.Translation((u, -0.075, 0.045)) @ Matrix.Rotation(th, 4, 'X'))
        G.add(C['steel'], cyl(0.004, 0.022, 6), Ms @ along((u - 0.011, -0.075, 0.045), (u + 0.011, -0.075, 0.045)))
    Mc = Ms @ Matrix.Translation((0.0, -0.075, 0.045)) @ Matrix.Rotation(th, 4, 'X')
    G.add(mat('hartpapier', (0.18, 0.10, 0.06), 'plastic', r=0.9), box((-0.21, 0.125, -0.009), (0.21, 0.150, 0.009), bev=0.003), Mc)
    G.add(mat('griff_holz', (0.05, 0.035, 0.025), 'plastic', r=0.6), lathe([(0, 0), (0.012, 0), (0.014, 0.03), (0.019, 0.07), (0.017, 0.092), (0, 0.098)], 10),
          Mc @ Matrix.Translation((0.0, 0.1375, 0.009)))
    G.add(C['grime'], blob(Ms @ Vector((0.0, 0.0, 0.0165)), Ms.to_3x3() @ Vector((1, 0, 0)), Ms.to_3x3() @ Vector((0, 1, 0)), 0.12, 0.06, rng, 12, 2, 0.5, off=0.0004))
    # Tafel: drei Zeigerinstrumente
    for k, y in enumerate((-0.25, 0.0, 0.25)):
        c = Vector((D1, y, 1.70))
        R = 0.072
        G.add(bake, lathe([(0, 0), (R, 0), (R, 0.032), (R - 0.006, 0.044), (0, 0.044)], 14), along(c, c + X))
        fz = c + X * 0.0442
        G.add(face, lathe([(0, 0), (R * 0.84, 0), (0, 0.0004)], 14), along(fz, fz + X))
        G.add(C['ink'], lathe([(R * 0.70, 0.0), (R * 0.65, 0.0)], 8, a0=math.radians(35), a1=math.radians(145)), along(fz + X * 0.0006, fz + X))
        for t in range(5):
            a = math.radians(35 + t * 27.5)
            d = Vector((0, math.cos(a), math.sin(a)))
            q0 = fz + X * 0.0006 + d * R * 0.70
            G.add(C['ink'], lathe([(0.0011, 0.0), (0.0011, R * (0.12 if t % 2 == 0 else 0.07))], 3), along(q0, q0 + d))
        a = math.radians(rng.uniform(50, 130))
        d = Vector((0, math.cos(a), math.sin(a)))
        p = fz + X * 0.0016
        G.add(C['black'], cyl(0.0012, R * 0.74, 4), along(p, p + d))
        G.add(C['black'], cyl(0.0035, 0.002, 6), along(p, p + X))
        G.add(C['steel'], torus(R * 0.93, 0.0042, 14, 3), along(fz, fz + X))
        G.add(C['brass'], box((D1, y - 0.035, 1.70 - R - 0.03), (D1 + 0.0015, y + 0.035, 1.70 - R - 0.012)))
        G.add(C['ink'], box((D1 + 0.0015, y - 0.026, 1.70 - R - 0.023), (D1 + 0.0018, y + 0.02, 1.70 - R - 0.019)))
    for y in (-0.20, 0.20):  # Paketschalter mit Stellungsschild
        c = Vector((D1, y, 1.45))
        G.add(face, lathe([(0, 0), (0.052, 0), (0.052, 0.002), (0, 0.002)], 14), along(c, c + X))
        for a in (60, 90, 120):
            d = Vector((0, math.cos(math.radians(a)), math.sin(math.radians(a))))
            q0 = c + X * 0.0022 + d * 0.040
            G.add(C['ink'], lathe([(0.0015, 0.0), (0.0015, 0.008)], 3), along(q0, q0 + d))
        G.add(bake, lathe([(0, 0), (0.026, 0), (0.026, 0.012), (0.021, 0.018), (0, 0.018)], 12), along(c + X * 0.002, c + X))
        Mk = Matrix.Translation(c + X * 0.018) @ Matrix.Rotation(rng.choice([-0.52, 0.0, 0.52]), 4, 'X')
        G.add(bake, box((0.0, -0.012, -0.045), (0.022, 0.012, 0.045), bev=0.006, seg=1), Mk)
        G.add(C['paper'], box((0.0222, -0.002, 0.02), (0.0226, 0.002, 0.042)), Mk)
    for y, nm, rgb in ((-0.05, 'lampe_rot', (0.35, 0.05, 0.04)), (0.05, 'lampe_gruen', (0.06, 0.22, 0.08))):
        c = Vector((D1, y, 1.45))
        G.add(C['steel'], lathe([(0, 0), (0.017, 0), (0.017, 0.008), (0.012, 0.011), (0, 0.011)], 8), along(c, c + X))
        G.add(mat(nm, rgb, 'glass', r=1.4), dome(0.011, 0.012, 8), along(c + X * 0.010, c + X))
    G.add(C['brass'], box((D1, -0.14, 1.865), (D1 + 0.002, 0.14, 1.915), bev=0.0006))  # Messingschild
    for i, w in enumerate((0.22, 0.15)):
        G.add(C['ink'], box((D1 + 0.002, -w / 2, 1.897 - i * 0.02), (D1 + 0.0024, w / 2, 1.905 - i * 0.02)))
    for sy in (-1, 1):
        rv(G, C['brass'], (D1 + 0.002, sy * 0.13, 1.89), (1, 0, 0), 0.003)
    Mp = Matrix.Translation((D1, 0.10, 1.255)) @ Matrix.Rotation(-0.06, 4, 'X')  # Pruefkarte mit Klebeband
    G.add(C['paper'], box((0.0, -0.05, -0.035), (0.0005, 0.05, 0.035)), Mp)
    for i in range(4):
        G.add(C['ink'], box((0.0005, -0.042, 0.02 - i * 0.014), (0.0008, -0.042 + rng.uniform(0.04, 0.08), 0.024 - i * 0.014)), Mp)
    tape = mat('klebeband_alt', (0.62, 0.52, 0.30), 'plastic', r=0.8)
    for sy in (-1, 1):
        G.add(tape, box((0.0005, sy * 0.05 - 0.012, 0.024), (0.0009, sy * 0.05 + 0.012, 0.040)), Mp)
    # Spuren
    Sf = _SX(xf + 0.0009)
    G.add(C['rust'], orient(strip_uv(Sf, dy0 + 0.005, dy1 - 0.005, dz0 + 0.002, 0.03, rng, 14, 0.8), NX))
    for _ in range(3):
        G.add(C['rust_stain'], orient(streak_uv(Sf, rng.uniform(dy0 + 0.05, dy1 - 0.05), dz0 + 0.05, 0.012, 0.05, rng, 4, down=1.0), NX))
    Sa = _SX(D1 + 0.0049)
    for sy in (-1, 1):
        for z in (1.62, 1.37):
            G.add(C['rust_stain'], orient(streak_uv(Sa, sy * (W - 0.018), z - 0.008, 0.009, rng.uniform(0.07, 0.17), rng, 5), NX))
    G.add(C['rust_stain'], orient(streak_uv(_SX(D0 + 0.0049), W - 0.018, 0.55, 0.009, 0.14, rng, 5), NX))
    G.add(C['grime'], blob((xf, hy, hz), (0, 1, 0), (0, 0, 1), 0.06, 0.08, rng, 12, 2, 0.4, off=0.0009))
    chips_on_plane(G, C['steel_dark'], (xf + 0.0009, 0.0, (dz0 + dz1) / 2), (0, 1, 0), (0, 0, 1), dy1 - dy0, dz1 - dz0, rng, 8, 0.004, 0.012)
    chips_on_plane(G, C['steel_dark'], (D1 + 0.0009, 0.0, 1.57), (0, 1, 0), (0, 0, 1), 0.74, 0.74, rng, 6, 0.004, 0.012)
    for sy in (-1, 1):
        Ss = (lambda s: (lambda u, v: (Vector((u, s * (W + 0.0009), v)), Vector((0, s, 0)))))(sy)
        G.add(C['rust'], orient(strip_uv(Ss, 0.03, D0 - 0.04, Z0 + 0.001, 0.05, rng, 10, 0.8), lambda m, s=sy: Vector((0, s, 0))))
        chips_on_plane(G, C['steel_dark'], (D0 / 2, sy * (W + 0.0009), 0.5), (-sy, 0, 0), (0, 0, 1), D0 - 0.08, 0.8, rng, 4, 0.004, 0.012)
        ns = (lambda s: (lambda m: Vector((0, s, 0))))(sy)
        for (u, v, ru, rv_) in ((0.12, 1.55, 0.05, 0.08), (0.36, 0.62, 0.06, 0.05), (0.20, 1.86, 0.04, 0.03)):
            G.add(C['rust'], orient(blob_uv(Ss, u, v, ru, rv_, rng, 12, 2, 0.55), ns))
            G.add(C['rust_stain'], orient(streak_uv(Ss, u, v - rv_ * 0.6, ru * 0.7, rng.uniform(0.12, 0.3), rng, 6), ns))
    for (y, z, ru, rv_) in ((-0.33, 1.22, 0.035, 0.03), (0.30, 1.92, 0.03, 0.025), (-0.16, 0.30, 0.04, 0.025)):
        G.add(C['rust'], orient(blob_uv(_SX((D1 if z > 1.1 else xf) + 0.0011), y, z, ru, rv_, rng, 12, 2, 0.55), NX))
    for z in (dz0 + 0.12, dz1 - 0.12):
        G.add(C['rust_stain'], orient(streak_uv(Sf, dy0 + 0.012, z - 0.04, 0.012, rng.uniform(0.08, 0.16), rng, 5), NX))
    G.add(C['oil'], blob((D0 + 0.06, 0.1, 0.0005), (1, 0, 0), (0, 1, 0), 0.06, 0.09, rng, 12, 2, 0.4, off=0.0))
    G.build('schalttafel', angle=40.0)


def build_mach_cabinet__v2():
    """1970er Steuerpult: beiger Kasten mit gerundeten Kanten und braunen Seitenwangen, schraege Bedienplatte mit
    Kippschaltern (Chromhebel, Praegeband-Schildchen), drei Drehknoepfe mit Skala, Schluesselschalter mit steckendem
    Schluessel, senkrechte Rueckwand mit eckigen Leuchtmeldern (einer leuchtet noch) und zwei Profilinstrumenten,
    Firmenschild, beschrifteter Kreppstreifen; Fronttuer mit Griffmulde, Schloss und Lueftungsschlitzen; vergilbter
    Lack, Brandfleck, Staubfilm, Trittspuren, Abplatzer (graue Grundierung), Rost an der Tuerunterkante."""
    rng = random.Random(43)
    C = M_common()
    G = Geo()
    beige = mat('pult_beige', (0.57, 0.51, 0.38), 'paint', r=1.0)
    brown = mat('pult_wange', (0.19, 0.12, 0.07), 'paint', r=0.95)
    plinth = mat('sockel', (0.10, 0.10, 0.10), 'paint', r=1.2)
    plate = mat('bedienplatte', (0.24, 0.24, 0.23), 'paint', r=0.9)
    prim = mat('grundierung', (0.42, 0.42, 0.40), 'paint', r=1.2)
    dymo = mat('praegeband', (0.04, 0.04, 0.05), 'plastic', r=0.7)
    face = mat('skala', (0.86, 0.85, 0.80), 'paint', r=0.9)
    W, Wc = 0.44, 0.022
    D, Zb, Zd, Dt, Zt, Zh = 0.58, 0.08, 0.84, 0.30, 1.02, 1.60
    body = round_poly([(0.0, Zb), (D, Zb), (D, Zd), (Dt, Zt), (Dt, Zh), (0.0, Zh)], [0, 0.015, 0.03, 0.03, 0.04, 0], 3)
    G.add(beige, prism(body, -W, W), PXZ)
    cheek = round_poly([(0.0, Zb - 0.01), (D + 0.012, Zb - 0.01), (D + 0.012, Zd + 0.006), (Dt + 0.012, Zt + 0.012), (Dt + 0.012, Zh + 0.012), (0.0, Zh + 0.012)],
                       [0, 0.012, 0.035, 0.035, 0.045, 0], 3)
    G.add(brown, prism(cheek, W, W + Wc), PXZ)
    G.add(brown, prism(cheek, -W - Wc, -W), PXZ)
    G.add(plinth, box((0.04, -W - Wc + 0.012, 0.0), (D - 0.06, W + Wc - 0.012, Zb)))
    # Bedienplatte (schraeg)
    V = Vector((Dt - D, 0.0, Zt - Zd)).normalized()
    L = math.hypot(Dt - D, Zt - Zd)
    Ms = frame_uvn(((D + Dt) / 2, 0.0, (Zd + Zt) / 2), (0, 1, 0), V)
    U3, V3 = Ms.to_3x3() @ Vector((1, 0, 0)), Ms.to_3x3() @ Vector((0, 1, 0))
    G.add(plate, box((-0.41, -L / 2 + 0.025, 0.0), (0.41, L / 2 - 0.025, 0.004), bev=0.0015), Ms)
    G.add(mat('staubfilm', (0.36, 0.35, 0.32), 'concrete', r=1.4), blob(Ms @ Vector((0.05, 0.12, 0.0042)), U3, V3, 0.26, 0.02, rng, 12, 2, 0.5, off=0.0))

    def toggle(u, v):
        G.add(C['steel'], nut_shape(0.0075, 0.004), Ms @ Matrix.Translation((u, v, 0.004)))
        tilt = 0.42 * rng.choice([-1, 1])
        G.add(C['steel'], lathe([(0.0028, 0.0), (0.0019, 0.020), (0.0026, 0.022), (0.0, 0.024)], 5),
              Ms @ Matrix.Translation((u, v, 0.008)) @ Matrix.Rotation(-tilt, 4, 'X'))
        G.add(dymo, box((u - 0.03, v - 0.042, 0.004), (u + 0.03, v - 0.030, 0.0048)), Ms)
        G.add(C['paper'], box((u - 0.022, v - 0.0375, 0.0048), (u - 0.022 + rng.uniform(0.02, 0.044), v - 0.0345, 0.0051)), Ms)
    for i in range(8):
        toggle(-0.34 + i * 0.097, -0.07)
    for i in range(4):
        toggle(-0.34 + i * 0.097, 0.075)
    knob = mat('drehknopf', (0.06, 0.06, 0.06), 'plastic', r=0.6)
    for u in (0.07, 0.17, 0.27):
        G.add(C['paper'], lathe([(0.027, 0.0), (0.0225, 0.0)], 10, a0=math.radians(-45), a1=math.radians(225)), Ms @ Matrix.Translation((u, 0.06, 0.0045)))
        G.add(knob, lathe([(0.019, 0), (0.019, 0.004), (0.014, 0.007), (0.012, 0.019), (0.009, 0.023), (0, 0.023)], 10), Ms @ Matrix.Translation((u, 0.06, 0.004)))
        G.add(C['paper'], box((-0.0012, 0.002, 0.0272), (0.0012, 0.0085, 0.0276)), Ms @ Matrix.Translation((u, 0.06, 0.0)) @ Matrix.Rotation(rng.uniform(-0.9, 0.9), 4, 'Z'))
    Mk = Ms @ Matrix.Translation((0.355, 0.06, 0.004))  # Schluesselschalter, Schluessel steckt
    G.add(C['steel'], lathe([(0.0, 0.0), (0.016, 0.0), (0.016, 0.005), (0.012, 0.008), (0.0, 0.008)], 10), Mk)
    G.add(C['brass'], box((-0.001, -0.004, 0.008), (0.001, 0.004, 0.028)), Mk @ Matrix.Rotation(0.5, 4, 'Z'))
    G.add(C['brass'], box((-0.0015, -0.012, 0.028), (0.0015, 0.012, 0.050), bev=0.001, seg=1), Mk @ Matrix.Rotation(0.5, 4, 'Z'))
    burn = mat('brandfleck', (0.12, 0.07, 0.04), 'paint', r=1.3)  # Brandfleck einer Zigarette an der Vorderkante
    G.add(burn, blob(Ms @ Vector((-0.385, -0.135, 0.0042)), U3, V3, 0.014, 0.009, rng, 8, 2, 0.4, off=0.0))
    G.add(C['black'], blob(Ms @ Vector((-0.385, -0.135, 0.0044)), U3, V3, 0.006, 0.004, rng, 6, 1, 0.3, off=0.0))
    # Rueckwand: Leuchtmelder, Profilinstrumente, Schilder
    cols = [(0.70, 0.36, 0.06), (0.10, 0.40, 0.12), (0.75, 0.72, 0.62), (0.55, 0.08, 0.06), (0.75, 0.60, 0.08), (0.12, 0.22, 0.45)]
    lmats = [mat(f'leuchtmelder{i}', c, 'plastic', r=0.5) for i, c in enumerate(cols)]
    lit = mat('leuchtmelder_an', (1.0, 0.55, 0.12), 'glow', glow=1.1)
    for row, z in enumerate((1.40, 1.30)):
        for i in range(6):
            y = -0.02 + i * 0.072
            G.add(C['black'], box((Dt, y - 0.018, z - 0.018), (Dt + 0.009, y + 0.018, z + 0.018)))
            m_ = lit if (row, i) == (0, 1) else lmats[(i + row * 2) % 6]
            G.add(m_, box((Dt + 0.009, y - 0.0145, z - 0.0145), (Dt + 0.0135, y + 0.0145, z + 0.0145)))
        G.add(dymo, box((Dt, -0.045, z - 0.034), (Dt + 0.0008, 0.385, z - 0.025)))
        for i in range(6):
            y = -0.02 + i * 0.072
            G.add(C['paper'], box((Dt + 0.0008, y - 0.016, z - 0.031), (Dt + 0.0011, y - 0.016 + rng.uniform(0.012, 0.03), z - 0.028)))
    for y in (-0.33, -0.21):
        G.add(C['black'], box((Dt, y - 0.032, 1.27), (Dt + 0.012, y + 0.032, 1.43)))
        G.add(face, box((Dt + 0.012, y - 0.024, 1.28), (Dt + 0.0125, y + 0.024, 1.42)))
        for i in range(6):
            z = 1.295 + i * 0.022
            G.add(C['ink'], box((Dt + 0.0125, y - 0.022, z), (Dt + 0.0128, y - 0.022 + (0.014 if i % 2 else 0.009), z + 0.0012)))
        zn = rng.uniform(1.31, 1.40)
        G.add(C['red'], box((Dt + 0.0128, y - 0.022, zn), (Dt + 0.0132, y + 0.018, zn + 0.0015)))
    _plate_lines(G, C, FX((Dt + 0.0002, -0.27, 1.50)), 0.14, 0.04, rng, 2)
    tape = mat('kreppband', (0.80, 0.76, 0.62), 'paint', r=1.35)
    Mt = Matrix.Translation((Dt, 0.17, 1.485)) @ Matrix.Rotation(0.02, 4, 'X')
    G.add(tape, box((0.0, -0.10, -0.013), (0.0006, 0.10, 0.013)), Mt)
    pen = mat('kugelschreiber', (0.06, 0.08, 0.22), 'paint', r=1.0)
    for i in range(7):
        y0 = -0.085 + i * 0.024
        _stroke(G, pen, Mt @ Vector((0.0009, y0, -0.006)), Mt @ Vector((0.0009, y0 + rng.uniform(0.008, 0.016), rng.uniform(-0.002, 0.007))), 0.0016)
    # Fronttuer mit Griffmulde, Schloss und Lueftungsschlitzen
    ty0, ty1, tz0, tz1 = -W + 0.03, W - 0.03, Zb + 0.05, Zd - 0.06
    G.add(C['black'], box((D - 0.001, ty0 - 0.003, tz0 - 0.003), (D + 0.002, ty1 + 0.003, tz1 + 0.003)))
    G.add(beige, box((D, ty0, tz0), (D + 0.012, ty1, tz1), bev=0.005, seg=1))
    xf = D + 0.012
    G.add(C['black'], box((xf - 0.0005, 0.24, tz1 - 0.07), (xf + 0.0005, 0.37, tz1 - 0.045)))
    G.add(beige, box((xf, 0.24, tz1 - 0.045), (xf + 0.008, 0.37, tz1 - 0.040)))
    G.add(C['steel'], lathe([(0.0, 0.0), (0.011, 0.0), (0.011, 0.005), (0.0, 0.005)], 10), along((xf, 0.305, tz1 - 0.11), (xf + 1, 0.305, tz1 - 0.11)))
    G.add(C['hole'], box((xf + 0.005, 0.304, tz1 - 0.116), (xf + 0.0055, 0.306, tz1 - 0.104)))
    for k in range(6):
        z = tz0 + 0.04 + k * 0.028
        G.add(C['hole'], box((xf - 0.0005, -0.30, z), (xf + 0.0005, 0.30, z + 0.009)))
        G.add(beige, box((xf, -0.30, z + 0.009), (xf + 0.007, 0.30, z + 0.012)))
    Sf = _SX(xf + 0.0009)
    G.add(C['rust'], orient(strip_uv(Sf, ty0 + 0.006, ty1 - 0.006, tz0 + 0.002, 0.018, rng, 14, 0.8), NX))
    for _ in range(4):  # Trittspuren
        G.add(mat('trittspur', (0.40, 0.37, 0.31), 'paint', r=1.3), blob((xf, rng.uniform(-0.3, 0.3), rng.uniform(0.36, 0.52)), (0, 1, 0), (0, 0, 1), rng.uniform(0.03, 0.06), rng.uniform(0.015, 0.03),
                               rng, 10, 2, 0.5, off=0.0009, rot=rng.uniform(-0.4, 0.4)))
    chips_on_plane(G, prim, (xf + 0.0009, 0.0, (tz0 + tz1) / 2), (0, 1, 0), (0, 0, 1), ty1 - ty0, tz1 - tz0, rng, 7, 0.003, 0.010)
    chips_on_plane(G, prim, (Dt + 0.0009, 0.0, 1.32), (0, 1, 0), (0, 0, 1), 2 * W, 0.5, rng, 5, 0.003, 0.009)
    for sy in (-1, 1):
        yy = sy * (W + Wc + 0.0009)
        chips_on_plane(G, prim, (0.30, yy, 0.6), (-sy, 0, 0), (0, 0, 1), 0.5, 0.9, rng, 4, 0.004, 0.012)
        G.add(C['grime'], blob((0.40, yy, 0.30), (-sy, 0, 0), (0, 0, 1), 0.10, 0.08, rng, 10, 2, 0.5, off=0.0))
    G.build('steuerpult', angle=40.0)


# ---------------------------------------------------------------------------------------------------------------------
# Verteiler: v0 (Stahlblech, gestrichen) oben; v1 Gusseisen mit Porzellankappen; v2 Bakelit; v3 Kunststoff neu
# ---------------------------------------------------------------------------------------------------------------------
def build_junction_box__v1():
    """1930er Hausanschlusskasten aus Gusseisen (Mitte 1,5 m, Wand bei x = 0): Gehaeuse mit angegossenen Laschen,
    Deckel mit Spiegel, Gussrippen und vier Sechskantschrauben, zwei Porzellan-Schraubkappen (Sicherungen, gerippt,
    Kennmelder rot/grau) in Gussbunden, Emailleschildchen mit Blitzpfeil; Stahlpanzerrohr mit Muffe und Schelle zur
    Decke, bleiummanteltes Kabel unten in die Wand; Rost, Laeufer, Kalkfahne."""
    rng = random.Random(32)
    C = M_common()
    G = Geo()
    X, Z = Vector((1, 0, 0)), Vector((0, 0, 1))
    iron = mat('gusseisen', (0.16, 0.17, 0.16), 'paint', tex='painted_metal_grey', r=1.3)
    porc = mat('porzellan', (0.86, 0.84, 0.78), 'ceramic', r=0.5)
    lead = mat('bleimantel', (0.30, 0.31, 0.33), 'metal', r=1.15, m=0.45)
    enam = mat('emaille_weiss', (0.82, 0.82, 0.78), 'ceramic', r=0.6)
    kalk = mat('kalk', (0.66, 0.64, 0.58), 'concrete', r=1.4)
    D, Wy, Hz = 0.11, 0.15, 0.19
    for sz in (-1, 1):
        G.add(iron, prism(rrect(0.075, 0.06, 0.022, 2), 0.0, 0.014), FX((0.0, 0.0, sz * (Hz + 0.018))))
        screw_head(G, C['steel'], (0.014, 0.0, sz * (Hz + 0.03)), (1, 0, 0), 0.007, slot_mat=C['hole'], kind='pan', seg=6)
    G.add(iron, box((0.0, -Wy, -Hz), (D, Wy, Hz), bev=0.012, seg=2))
    xl = D + 0.016
    G.add(C['black'], box((D - 0.001, -Wy + 0.01, -Hz + 0.01), (D + 0.002, Wy - 0.01, Hz - 0.01)))
    G.add(iron, box((D + 0.001, -Wy + 0.006, -Hz + 0.006), (xl, Wy - 0.006, Hz - 0.006), bev=0.005, seg=1))
    G.add(iron, box((xl - 0.002, -Wy + 0.028, -Hz + 0.028), (xl + 0.006, Wy - 0.028, Hz - 0.028), bev=0.003, seg=1))
    xs = xl + 0.006
    for sy in (-1, 1):
        for sz in (-1, 1):
            bolt(G, C['steel_dark'], (xl, sy * (Wy - 0.016), sz * (Hz - 0.016)), (1, 0, 0), 0.0095, washer=False)
    for z in (-0.105, -0.13):
        G.add(iron, box((xs - 0.001, -0.09, z - 0.004), (xs + 0.006, 0.09, z + 0.004), bev=0.002))
    for k, y in enumerate((-0.062, 0.062)):  # Porzellan-Schraubkappen
        c = Vector((xs, y, 0.07))
        G.add(iron, lathe([(0.0, 0.0), (0.036, 0.0), (0.036, 0.010), (0.031, 0.014), (0.0, 0.014)], 14), along(c, c + X))
        star = [((0.027 if j % 2 == 0 else 0.0245) * math.cos(TAU * j / 24), (0.027 if j % 2 == 0 else 0.0245) * math.sin(TAU * j / 24)) for j in range(24)]
        G.add(porc, prism(star, 0.014, 0.040), Matrix.Translation(c) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
        G.add(porc, lathe([(0.0245, 0.0), (0.022, 0.006), (0.012, 0.009), (0.0, 0.010)], 12), along(c + X * 0.040, c + X * 1.04))
        G.add(mat('kennmelder_rot' if k == 0 else 'kennmelder_grau', (0.62, 0.10, 0.08) if k == 0 else (0.45, 0.45, 0.44), 'plastic', r=0.8),
              lathe([(0.0, 0.0), (0.0055, 0.0), (0.0055, 0.0016), (0.0, 0.0016)], 8), along(c + X * 0.0492, c + X * 1.05))
        G.add(C['grime'], blob(c + X * 0.0002, (0, 1, 0), (0, 0, 1), 0.045, 0.042, rng, 12, 2, 0.4, off=0.0))
    G.add(enam, box((xs, -0.05, -0.062), (xs + 0.0015, 0.05, -0.024), bev=0.0005))
    G.add(C['red'], prism([(x * 0.55, y * 0.38) for x, y in ARROW], 0.0015, 0.0019), FX((xs, -0.03, -0.043)))
    for i, w in enumerate((0.05, 0.04)):
        G.add(C['ink'], box((xs + 0.0015, -0.012, -0.040 - i * 0.012), (xs + 0.0019, -0.012 + w, -0.034 - i * 0.012)))
    for sy in (-1, 1):
        rv(G, C['steel'], (xs + 0.0015, sy * 0.044, -0.043), (1, 0, 0), 0.0025)
    # Panzerrohr oben, Bleikabel unten
    pipe = mat('panzerrohr', (0.10, 0.10, 0.10), 'paint', r=1.0)
    top = Vector((0.06, 0.0, Hz))
    G.add(iron, lathe([(0.0, 0.0), (0.026, 0.0), (0.024, 0.02), (0.020, 0.03), (0.0, 0.03)], 10), along(top - Z * 0.01, top + Z))
    G.add(C['steel_dark'], nut_shape(0.020, 0.014), along(top + Z * 0.02, top + Z * 1.02))
    G.add(pipe, cyl(0.0135, 1.3 - Hz - 0.03, 8, cap=False), along(top + Z * 0.03, (0.06, 0.0, 1.3)))
    G.add(pipe, cyl(0.017, 0.045, 8, cap=False), along((0.06, 0.0, 0.62), (0.06, 0.0, 0.665)))
    G.add(C['steel_dark'], _saddle(0.06, 0.0135), Matrix.Translation((0, 0, 0.95)))
    for sy in (-1, 1):
        screw_head(G, C['steel'], (0.0018, sy * (0.0135 + 0.011), 0.95), (1, 0, 0), 0.0035, kind='flat', seg=5)
    bot = Vector((0.06, -0.05, -Hz))
    G.add(iron, lathe([(0.0, 0.0), (0.024, 0.0), (0.022, 0.02), (0.018, 0.028), (0.0, 0.028)], 10), along(bot + Z * 0.01, bot - Z))
    G.add(C['steel_dark'], nut_shape(0.018, 0.012), along(bot - Z * 0.018, bot - Z * 1.018))
    G.add(lead, tube([(0.06, -0.05, -Hz - 0.028), (0.06, -0.05, -0.40), (0.0, -0.05, -0.46)], 0.011, 8, bend=0.05, caps=False))
    # Spuren
    Sm = _SX(xs + 0.0009)
    for y in (-0.062, 0.062):
        G.add(C['rust_stain'], orient(streak_uv(Sm, y + rng.uniform(-0.01, 0.01), 0.036, 0.012, rng.uniform(0.05, 0.09), rng, 5), NX))
    G.add(kalk, orient(streak_uv(Sm, 0.112, Hz - 0.03, 0.010, 0.14, rng, 6, taper=0.5), NX))
    G.add(C['rust'], orient(strip_uv(_SX(xl + 0.0009), -Wy + 0.012, Wy - 0.012, -Hz + 0.007, 0.022, rng, 12, 0.8), NX))
    for sy in (-1, 1):
        Ss = (lambda s: (lambda u, v: (Vector((u, s * (Wy + 0.0009), v)), Vector((0, s, 0)))))(sy)
        for _ in range(2):
            G.add(C['rust'], orient(blob_uv(Ss, rng.uniform(0.02, D - 0.02), rng.uniform(-Hz + 0.03, Hz - 0.03), 0.02, 0.03, rng, 10, 2, 0.5), lambda m, s=sy: Vector((0, s, 0))))
        G.add(C['rust_stain'], orient(streak_uv(Ss, D * 0.5, -Hz + 0.05, 0.03, 0.05, rng, 4), lambda m, s=sy: Vector((0, s, 0))))
    chips_on_plane(G, C['rust_dark'], (xl + 0.0009, 0.0, 0.0), (0, 1, 0), (0, 0, 1), 2 * Wy - 0.02, 2 * Hz - 0.02, rng, 6, 0.004, 0.012)
    G.build('gussverteiler', angle=40.0)


def build_junction_box__v2():
    """1950er Bakelit-Paketschalter auf lackiertem Holzbrett (Mitte 1,5 m): braunes, glaenzendes Pressstoffgehaeuse
    mit gerundeten Kanten, Drehknebel mit Stellungsschild 0-I-II, zwei Deckelschrauben, Haarriss; Bergmannrohr
    (Messingmantel) mit Schellen zur Decke, darunter stoffumsponnene Leitung in loser Schlaufe zur runden
    Porzellan-Abzweigdose und in die Wand; Staubfilm oben, abgegriffener Knebel."""
    rng = random.Random(33)
    C = M_common()
    G = Geo()
    X, Z = Vector((1, 0, 0)), Vector((0, 0, 1))
    bake = mat('bakelit_braun', (0.17, 0.085, 0.04), 'plastic', r=0.45)
    board = mat('brett_lack', (0.32, 0.24, 0.15), 'paint', r=1.0)
    porc = mat('porzellan', (0.86, 0.84, 0.78), 'ceramic', r=0.5)
    cloth = mat('stoffkabel', (0.20, 0.16, 0.11), 'fabric', r=1.4)
    berg = mat('bergmannrohr', (0.42, 0.36, 0.22), 'metal', r=1.1, m=0.6)
    G.add(board, prism(rrect(0.24, 0.34, 0.012, 2), 0.0, 0.018), FX((0.0, 0.0, 0.0)))
    for sy in (-1, 1):
        for sz in (-1, 1):
            screw_head(G, C['steel'], (0.018, sy * 0.10, sz * 0.15), (1, 0, 0), 0.005, slot_mat=C['hole'], kind='flat', seg=6)
    zc = 0.01
    G.add(bake, prism(rrect(0.15, 0.19, 0.028, 3), 0.018, 0.075), FX((0.0, 0.0, zc)))
    G.add(bake, prism(rrect(0.142, 0.182, 0.024, 3), 0.075, 0.083), FX((0.0, 0.0, zc)))
    G.add(bake, prism(rrect(0.13, 0.17, 0.02, 3), 0.083, 0.087), FX((0.0, 0.0, zc)))
    xf = 0.087
    for z in (zc + 0.072, zc - 0.072):
        screw_head(G, C['steel'], (xf, 0.0, z), (1, 0, 0), 0.0045, slot_mat=C['hole'], kind='pan', seg=6)
    c = Vector((xf, 0.0, zc + 0.008))  # Stellungsschild und Knebel
    G.add(porc, lathe([(0.0, 0.0), (0.042, 0.0), (0.042, 0.0015), (0.0, 0.0015)], 14), along(c, c + X))
    for a in (45, 90, 135):
        d = Vector((0.0, math.cos(math.radians(a)), math.sin(math.radians(a))))
        q0 = c + X * 0.0016 + d * 0.027
        G.add(C['ink'], lathe([(0.0016, 0.0), (0.0016, 0.011)], 3), along(q0, q0 + d))
    G.add(bake, lathe([(0.0, 0.0), (0.020, 0.0), (0.020, 0.008), (0.016, 0.014), (0.0, 0.014)], 12), along(c + X * 0.0015, c + X))
    roll = rng.choice([0.0, -0.785])
    lever = prism(round_poly([(-0.011, -0.05), (0.011, -0.05), (0.008, 0.05), (-0.008, 0.05)], 0.006, 2), 0.0, 0.016)
    G.add(bake, lever, FX(c + X * 0.012, roll=roll))
    G.add(C['paper'], box((-0.0015, 0.022, 0.016), (0.0015, 0.044, 0.0164)), FX(c + X * 0.012, roll=roll))
    G.add(mat('bakelit_abgegriffen', (0.30, 0.17, 0.09), 'plastic', r=0.3), blob(c + X * 0.0285, (0, 1, 0), (0, 0, 1), 0.012, 0.03, rng, 8, 1, 0.3, off=0.0, rot=roll))
    for a, b in (((0.0873, 0.032, -0.012), (0.0873, 0.044, -0.03)), ((0.0873, 0.044, -0.03), (0.0873, 0.048, -0.052)),
                 ((0.0873, 0.048, -0.052), (0.0873, 0.06, -0.066))):  # Haarriss
        _stroke(G, C['hole'], a, b, 0.0012)
    G.add(C['dust'], blob((0.05, 0.0, zc + 0.095), (1, 0, 0), (0, 1, 0), 0.03, 0.06, rng, 10, 2, 0.5, off=0.0006))
    # Bergmannrohr zur Decke
    tb = Vector((0.045, 0.0, zc + 0.095))
    G.add(bake, lathe([(0, 0), (0.014, 0), (0.012, 0.015), (0, 0.015)], 10), along(tb, tb + Z))
    G.add(berg, cyl(0.0085, 1.3 - tb.z - 0.012, 8, cap=False), along(tb + Z * 0.012, (0.045, 0.0, 1.3)))
    for z in (0.45, 0.92):
        G.add(berg, _saddle(0.045, 0.0085, t=0.001, w=0.006), Matrix.Translation((0, 0, z)))
        for sy in (-1, 1):
            screw_head(G, C['steel'], (0.0018, sy * (0.0085 + 0.011), z), (1, 0, 0), 0.003, kind='flat', seg=5)
    # Stoffkabel in loser Schlaufe zur Porzellandose, von dort in die Wand
    bb = Vector((0.045, 0.035, zc - 0.095))
    G.add(bake, lathe([(0, 0), (0.010, 0), (0.008, 0.012), (0, 0.012)], 8), along(bb, bb - Z))
    G.add(cloth, tube([(0.045, 0.035, zc - 0.105), (0.05, 0.04, -0.17), (0.064, 0.055, -0.25), (0.058, 0.07, -0.32), (0.03, 0.08, -0.395)], 0.0045, 6, bend=0.04, caps=False))
    dc = Vector((0.0, 0.08, -0.44))
    G.add(porc, lathe([(0.0, 0.0), (0.045, 0.0), (0.045, 0.012), (0.040, 0.022), (0.025, 0.030), (0.0, 0.033)], 14), along(dc, dc + X))
    screw_head(G, C['steel'], dc + X * 0.033, X, 0.004, slot_mat=C['hole'], kind='pan', seg=6)
    G.add(cloth, tube([(0.025, 0.08, -0.48), (0.025, 0.08, -0.52), (0.0, 0.08, -0.55)], 0.0045, 6, bend=0.02, caps=False))
    G.add(C['grime'], blob(dc + X * 0.0332 + Vector((0, 0, 0.012)), (0, 1, 0), (0, 0, 1), 0.02, 0.012, rng, 8, 1, 0.4, off=0.0))
    G.build('bakelitschalter', angle=40.0)


def build_junction_box__v3():
    """1990er/2000er Kleinverteiler aus hellgrauem Kunststoff (Mitte 1,5 m): Unterteil, Klappdeckel mit Woelbung,
    Rauchglas-Sichtfenster, angeformte Scharniere, zwei Viertelumdrehungsverschluesse; Warnaufkleber, Typenschild,
    Filzstift-Beschriftung; drei Kabelverschraubungen unten mit grauen Leitungen in einen Kabelkanal bis zum Boden,
    oben Installationsrohr mit Schnellschellen zur Decke; Schmutzrand, vergilbte Oberseite, Fingerspuren."""
    rng = random.Random(34)
    C = M_common()
    G = Geo()
    X, Z = Vector((1, 0, 0)), Vector((0, 0, 1))
    grey = mat('kunststoff_hell', (0.60, 0.60, 0.57), 'plastic', r=0.9)
    lidm = mat('kunststoff_deckel', (0.63, 0.63, 0.60), 'plastic', r=0.8)
    smoke = mat('rauchglas', (0.07, 0.08, 0.08), 'glass')
    cable = mat('kabel_grau', (0.40, 0.40, 0.39), 'rubber', r=1.1)
    duct = mat('kabelkanal', (0.62, 0.62, 0.59), 'plastic', r=0.95)
    pvc = mat('installationsrohr', (0.55, 0.56, 0.55), 'plastic', r=0.9)
    Hz = 0.20
    G.add(grey, prism(rrect(0.30, 0.40, 0.025, 3), 0.0, 0.095), FX((0.0, 0.0, 0.0)))
    G.add(C['black'], prism(rrect(0.292, 0.392, 0.022, 3), 0.095, 0.097), FX((0.0, 0.0, 0.0)))
    G.add(lidm, prism(rrect(0.296, 0.396, 0.024, 3), 0.097, 0.128), FX((0.0, 0.0, 0.0)))
    G.add(lidm, prism(rrect(0.27, 0.37, 0.02, 3), 0.128, 0.132), FX((0.0, 0.0, 0.0)))
    xf = 0.132
    G.add(lidm, prism(rrect(0.21, 0.095, 0.01, 2), xf, xf + 0.004), FX((0.0, 0.0, 0.075)))
    G.add(smoke, prism(rrect(0.19, 0.075, 0.008, 2), xf + 0.004, xf + 0.0055), FX((0.0, 0.0, 0.075)))
    for z in (0.15, -0.15):  # Viertelumdrehungsverschluesse
        c = Vector((xf, 0.115, z))
        G.add(lidm, lathe([(0, 0), (0.012, 0), (0.012, 0.004), (0.010, 0.006), (0, 0.006)], 10), along(c, c + X))
        G.add(C['hole'], box((0.0, -0.0015, -0.008), (0.0006, 0.0015, 0.008)), Matrix.Translation(c + X * 0.0058) @ Matrix.Rotation(0.3, 4, 'X'))
        G.add(C['grime'], blob(c + X * 0.0004, (0, 1, 0), (0, 0, 1), 0.025, 0.02, rng, 8, 1, 0.4, off=0.0))
    for z in (0.12, -0.12):  # Scharniere links
        G.add(grey, cyl(0.007, 0.05, 8), along((0.11, -0.152, z - 0.025), (0.11, -0.152, z + 0.025)))
    _warn_triangle(G, C, FX((xf + 0.0002, -0.06, -0.06)), 0.08)
    _plate_lines(G, C, FX((xf + 0.0002, 0.05, -0.075)), 0.10, 0.04, rng, 4, metal=False)
    pen = mat('edding', (0.02, 0.02, 0.03), 'paint', r=0.9)
    strokes = [((-0.10, -0.115), (-0.10, -0.140)), ((-0.10, -0.140), (-0.085, -0.140)), ((-0.085, -0.140), (-0.085, -0.115)),
               ((-0.075, -0.115), (-0.067, -0.140)), ((-0.067, -0.140), (-0.059, -0.115)),
               ((-0.045, -0.118), (-0.035, -0.115)), ((-0.035, -0.115), (-0.032, -0.124)), ((-0.032, -0.124), (-0.046, -0.140)),
               ((-0.046, -0.140), (-0.030, -0.140)), ((-0.022, -0.140), (-0.021, -0.138)),
               ((-0.012, -0.116), (-0.002, -0.116)), ((-0.002, -0.116), (-0.009, -0.127)), ((-0.009, -0.127), (-0.002, -0.133)),
               ((-0.002, -0.133), (-0.012, -0.140))]
    for (a, b) in strokes:
        _stroke(G, pen, (xf + 0.0003, a[0], a[1]), (xf + 0.0003, b[0], b[1]))
    # Kabelverschraubungen, Leitungen, Kabelkanal bis zum Boden
    for y in (-0.08, 0.0, 0.08):
        b = Vector((0.05, y, -Hz))
        G.add(grey, nut_shape(0.014, 0.010), along(b + Z * 0.002, b - Z))
        G.add(grey, lathe([(0.012, 0.0), (0.011, 0.012), (0.008, 0.018), (0.0, 0.018)], 8), along(b - Z * 0.008, b - Z))
        G.add(cable, tube([(0.05, y, -Hz - 0.02), (0.05, y, -0.25), (0.024, y * 0.25, -0.31), (0.024, y * 0.25, -0.34)], 0.0055, 6, bend=0.03, caps=False))
    G.add(duct, box((0.0, -0.032, -1.5), (0.044, 0.032, -0.33)))
    G.add(duct, box((0.044, -0.034, -1.5), (0.048, 0.034, -0.33), bev=0.001))
    for z in (-0.6, -1.1):
        G.add(C['black'], box((0.0478, -0.034, z), (0.0483, 0.034, z + 0.0015)))
    G.add(C['grime'], blob((0.0485, 0.0, -1.42), (0, 1, 0), (0, 0, 1), 0.03, 0.08, rng, 10, 2, 0.5, off=0.0))
    # Installationsrohr oben mit Schnellschellen
    G.add(grey, nut_shape(0.016, 0.012), along((0.04, 0.0, Hz - 0.002), (0.04, 0.0, 1.2)))
    G.add(pvc, cyl(0.010, 1.3 - Hz - 0.01, 8, cap=False), along((0.04, 0.0, Hz + 0.01), (0.04, 0.0, 1.3)))
    G.add(pvc, cyl(0.0125, 0.05, 8, cap=False), along((0.04, 0.0, 0.85), (0.04, 0.0, 0.90)))
    for z in (0.55, 1.05):
        G.add(grey, box((0.0, -0.014, z - 0.009), (0.029, 0.014, z + 0.009), bev=0.002))
        G.add(grey, lathe([(0.0105, 0.0), (0.0135, 0.0), (0.0135, 0.018), (0.0105, 0.018)], 8, close=True), along((0.04, 0.0, z - 0.009), (0.04, 0.0, z + 0.009)))
    # Spuren
    G.add(C['grime'], orient(strip_uv(_SX(0.1289), -0.13, 0.13, -0.196, 0.02, rng, 12, 0.8), NX))
    for _ in range(3):
        G.add(C['grime'], orient(streak_uv(_SX(0.1329), rng.uniform(-0.12, 0.12), rng.uniform(0.0, 0.17), 0.012, rng.uniform(0.06, 0.15), rng, 5), NX))
    G.add(C['grime'], blob((0.0485, 0.0, -0.75), (0, 1, 0), (0, 0, 1), 0.03, 0.25, rng, 12, 2, 0.5, off=0.0))
    G.add(mat('vergilbt', (0.62, 0.58, 0.45), 'plastic', r=0.9), blob((0.064, 0.0, Hz + 0.0006), (1, 0, 0), (0, 1, 0), 0.06, 0.14, rng, 12, 2, 0.4, off=0.0))
    G.add(C['dust'], blob((0.112, 0.02, Hz + 0.0008), (1, 0, 0), (0, 1, 0), 0.012, 0.12, rng, 10, 2, 0.5, off=0.0))
    for _ in range(4):
        y0, z0 = rng.uniform(-0.12, 0.12), rng.uniform(-0.17, 0.17)
        _stroke(G, mat('kratzer_hell', (0.82, 0.82, 0.80), 'plastic', r=1.2), (xf + 0.0002, y0, z0), (xf + 0.0002, y0 + rng.uniform(-0.05, 0.05), z0 + rng.uniform(-0.02, 0.02)), 0.0008)
    G.build('kunststoffverteiler', angle=40.0)


# ---------------------------------------------------------------------------------------------------------------------
# Pumpe v1: 1920er Kolbenpumpe mit Schwungrad
# ---------------------------------------------------------------------------------------------------------------------
def build_mach_pump__v1():
    """1920er liegende Kolbenpumpe mit Schwungrad (Wand bei x = 0): gusseisernes Bett mit Profilkante und Zierlinie,
    doppeltwirkender Pumpenzylinder mit gewoelbtem Deckel und Stopfbuchse (Messingbrille), Ventilkasten mit
    verschraubtem Deckel, Windkessel mit Manometer und Entlueftungshahn, Kreuzkopf in Gleitbahnen, Pleuel mit
    Lagerdeckel, gekroepfte Welle in zwei Stehlagern mit Messingoelern, Schwungrad mit sechs ovalen Speichen und
    blankgelaufenem Kranz, abgesprungener Flachriemen bis zum Boden, Saug- und Druckleitung mit Flanschen in Bett und
    Wand; Messingschild, Oellachen, Rost, Staub."""
    rng = random.Random(72)
    C = M_common()
    G = Geo()
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    paint = mat('pumpe_alt', (0.085, 0.09, 0.09), 'paint', r=1.15)
    cast = mat('guss_alt', (0.06, 0.065, 0.065), 'paint', r=1.3)
    pipe = mat('druckrohr_alt', (0.20, 0.20, 0.19), 'paint', r=1.2)
    line = mat('zierlinie', (0.45, 0.12, 0.08), 'paint', r=1.0)
    Fm = {'fl': pipe, 'bolt': C['steel_dark']}
    # Bett
    G.add(paint, box((0.14, -0.74, 0.0), (0.76, 0.62, 0.15), bev=0.012, seg=2))
    G.add(paint, box((0.16, -0.72, 0.15), (0.74, 0.60, 0.185), bev=0.008, seg=1))
    G.add(line, box((0.7602, -0.72, 0.121), (0.7612, 0.60, 0.127)))
    for x in (0.20, 0.70):
        for y in (-0.69, 0.57):
            bolt(G, C['steel_dark'], (x, y, 0.185), Z, 0.012, stub=0.02)
    G.add(C['brass'], prism(circ(0.07, 16, sx=1.0, sy=0.45), 0.0, 0.003), FX((0.76, -0.05, 0.07)))
    for i, w in enumerate((0.08, 0.10, 0.06)):
        G.add(C['ink'], box((0.763, -0.05 - w / 2, 0.082 - i * 0.012), (0.7634, -0.05 + w / 2, 0.086 - i * 0.012)))
    # Pumpenzylinder (Achse y), Deckel, Stopfbuchse
    cx, cz = 0.45, 0.50
    a, b = Vector((cx, -0.70, cz)), Vector((cx, -0.30, cz))
    G.add(paint, lathe([(0.0, 0.0), (0.158, 0.0), (0.158, 0.03), (0.128, 0.036), (0.128, 0.364), (0.158, 0.37), (0.158, 0.40), (0.0, 0.40)], 20), along(a, b))
    G.add(paint, lathe([(0.13, 0.0), (0.11, 0.022), (0.06, 0.04), (0.0, 0.045)], 20), along(a, a - Y))
    Pc = Pipe(a, b, 0.128)
    xx, yy, zz = frame_z(Y)
    for k in range(8):
        t = TAU * (k + 0.5) / 8
        q = xx * math.cos(t) + yy * math.sin(t)
        bolt(G, C['steel_dark'], a + q * 0.143, -Y, 0.010, washer=False)
        bolt(G, C['steel_dark'], b + q * 0.143, Y, 0.010, washer=False)
    G.add(paint, lathe([(0.0, 0.0), (0.05, 0.0), (0.05, 0.04), (0.0, 0.04)], 12), along(b, b + Y))
    G.add(C['brass'], lathe([(0.0, 0.0), (0.064, 0.0), (0.064, 0.014), (0.0, 0.014)], 12), along(b + Y * 0.038, b + Y))
    for s in (-1, 1):
        bolt(G, C['steel_dark'], b + Y * 0.052 + X * s * 0.055, Y, 0.008, stub=0.015, washer=False)
    G.add(cast, box((0.34, -0.64, 0.185), (0.56, -0.36, 0.40), bev=0.01))
    G.add(paint, box((0.35, -0.64, 0.60), (0.55, -0.36, 0.72), bev=0.01, seg=1))  # Ventilkasten
    G.add(paint, box((0.34, -0.65, 0.72), (0.56, -0.35, 0.745), bev=0.004))
    for (x, y) in ((0.355, -0.635), (0.355, -0.50), (0.355, -0.365), (0.545, -0.635), (0.545, -0.50), (0.545, -0.365), (0.45, -0.635), (0.45, -0.365)):
        bolt(G, C['steel_dark'], (x, y, 0.745), Z, 0.009, washer=False)
    wk = [(0.0, 0.745), (0.135, 0.745), (0.135, 0.770), (0.10, 0.776), (0.10, 1.28), (0.092, 1.33), (0.07, 1.37), (0.04, 1.392), (0.0, 1.40)]
    G.add(paint, lathe(wk, 18), Matrix.Translation((cx, -0.50, 0.0)))  # Windkessel
    for k in range(6):
        t = TAU * (k + 0.5) / 6
        bolt(G, C['steel_dark'], (cx + 0.118 * math.cos(t), -0.50 + 0.118 * math.sin(t), 0.770), Z, 0.008, washer=False)
    G.add(C['brass'], cyl(0.008, 0.03, 8), along((cx, -0.50, 1.395), (cx, -0.50, 1.425)))
    G.add(C['brass'], box((-0.003, -0.025, -0.003), (0.003, 0.0, 0.003)), Matrix.Translation((cx, -0.50, 1.418)))
    gauge(G, C, (cx + 0.10, -0.50, 1.08), X, 0.05, rng, stem=0.035)
    # Druckleitung in die Wand, Saugleitung ins Bett
    PD = Pipe((0.35, -0.58, 0.665), (0.0, -0.58, 0.665), 0.045)
    G.add(pipe, cyl(0.045, 0.35, 14, cap=False), along(PD.a, PD.b))
    flange(G, Fm, PD.a, X, 0.045, 0.085, 0.018, 4, side=1)
    flange_pair(G, Fm, PD, 0.14, 0.045, 0.085, 0.018, 4, C)
    G.add(C['steel_dark'], lathe([(0.047, 0.0), (0.075, 0.0), (0.073, 0.008), (0.047, 0.014)], 12), along(PD.b, PD.b + X))
    rust_ring(G, C, PD, PD.L, rng, 0.06, -1)
    pipe_spots(G, C, PD, rng, 3, 0.05, 0.30)
    rs = 0.05
    G.add(pipe, cyl(rs, 0.11, 14, cap=False), along((0.35, -0.42, 0.64), (0.24, -0.42, 0.64)))
    flange(G, Fm, (0.35, -0.42, 0.64), X, rs, 0.092, 0.018, 4, side=1)
    G.add(pipe, torus(0.07, rs, 6, 14, math.pi / 2, math.pi), Matrix.Translation((0.24, -0.42, 0.57)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    G.add(pipe, cyl(rs, 0.57 - 0.205, 14, cap=False), along((0.17, -0.42, 0.57), (0.17, -0.42, 0.205)))
    flange(G, Fm, (0.17, -0.42, 0.187), -Z, rs, 0.092, 0.018, 4, side=1)
    PS = Pipe((0.17, -0.42, 0.57), (0.17, -0.42, 0.205), rs)
    rust_ring(G, C, PS, PS.L - 0.05, rng, 0.05, -1)
    # Kreuzkopf in Gleitbahnen, Kolbenstange
    yg0, yg1 = -0.23, 0.04
    G.add(cast, box((0.37, yg0, 0.185), (0.39, yg1, 0.60), bev=0.004))
    G.add(cast, box((0.39, yg0, 0.185), (0.51, yg1, 0.43), bev=0.004))
    G.add(C['steel'], box((0.39, yg0, 0.430), (0.50, yg1, 0.444)))
    G.add(C['steel'], box((0.39, yg0, 0.556), (0.50, yg1, 0.570)))
    G.add(C['grease'], blob((0.45, -0.05, 0.5705), (1, 0, 0), (0, 1, 0), 0.03, 0.10, rng, 10, 2, 0.5, off=0.0004))
    ys, zs, rc = 0.32, 0.50, 0.10
    th = math.radians(150)
    py, pz = ys + rc * math.cos(th), zs + rc * math.sin(th)
    yc = py - math.sqrt(0.32 ** 2 - (pz - cz) ** 2)
    G.add(C['steel_dark'], box((0.405, yc - 0.045, 0.444), (0.495, yc + 0.045, 0.556), bev=0.005))
    G.add(C['steel'], cyl(0.016, 0.104, 10), along((0.398, yc, cz), (0.502, yc, cz)))
    G.add(C['steel'], cyl(0.021, (yc - 0.045) - (b.y + 0.052), 10, cap=False), along((cx, b.y + 0.052, cz), (cx, yc - 0.045, cz)))
    G.add(C['brass'], lathe([(0.0, 0.0), (0.008, 0.0), (0.008, 0.012), (0.016, 0.022), (0.016, 0.05), (0.012, 0.055), (0.0, 0.056)], 10), Matrix.Translation((0.45, -0.18, 0.570)))
    # Kurbelwelle, Stehlager, Pleuel
    for xb in (0.27, 0.63):
        G.add(cast, box((xb - 0.05, ys - 0.08, 0.185), (xb + 0.05, ys + 0.08, zs - 0.01), bev=0.006))
        G.add(cast, box((xb - 0.045, ys - 0.07, zs - 0.01), (xb + 0.045, ys + 0.07, zs + 0.055), bev=0.008))
        for s in (-1, 1):
            bolt(G, C['steel_dark'], (xb, ys + s * 0.052, zs + 0.055), Z, 0.009, stub=0.012, washer=False)
        G.add(C['brass'], lathe([(0.0, 0.0), (0.008, 0.0), (0.008, 0.012), (0.016, 0.022), (0.016, 0.05), (0.012, 0.055), (0.0, 0.056)], 10), Matrix.Translation((xb, ys, zs + 0.055)))
    for (x0, x1) in ((0.20, 0.41), (0.49, 0.86)):
        G.add(C['steel'], cyl(0.035, x1 - x0, 12), along((x0, ys, zs), (x1, ys, zs)))

    def web_poly(r0, r1, L, t, k=5):
        pts = []
        for j in range(k + 1):
            q = t + math.pi / 2 + math.pi * j / k
            pts.append((r0 * math.cos(q), r0 * math.sin(q)))
        for j in range(k + 1):
            q = t - math.pi / 2 + math.pi * j / k
            pts.append((L * math.cos(t) + r1 * math.cos(q), L * math.sin(t) + r1 * math.sin(q)))
        return pts
    for (x0, x1) in ((0.395, 0.425), (0.475, 0.505)):
        G.add(paint, prism(web_poly(0.06, 0.045, rc, th), x0, x1), Matrix.Translation((0.0, ys, zs)) @ MYZX)
    G.add(C['steel'], cyl(0.03, 0.05, 10), along((0.425, py, pz), (0.475, py, pz)))
    C1, C2 = Vector((0.45, yc, cz)), Vector((0.45, py, pz))
    dr = (C2 - C1).normalized()
    Lr = (C2 - C1).length
    G.add(C['steel_dark'], box((-0.020, -0.011, 0.04), (0.020, 0.011, Lr - 0.04)), along(C1, C2, up=X))
    G.add(C['steel_dark'], box((-0.05, -0.02, -0.045), (0.05, 0.02, 0.045), bev=0.006), along(C2, C2 + dr, up=X))
    Mr = along(C2, C2 + dr, up=X)
    hd = Mr.to_3x3() @ Vector((1, 0, 0))
    for s in (-1, 1):
        bolt(G, C['steel'], C2 + hd * s * 0.036 + dr * 0.045, dr, 0.008, stub=0.008, washer=False)
    G.add(C['oil'], blob((0.45, 0.32, 0.1856), (1, 0, 0), (0, 1, 0), 0.10, 0.14, rng, 12, 2, 0.5, off=0.0))
    # Schwungrad mit blankgelaufenem Kranz, abgesprungener Riemen
    fc = Vector((0.81, ys, zs))
    G.add(paint, lathe([(0.355, -0.03), (0.40, -0.03), (0.40, 0.03), (0.355, 0.03)], 32, close=True), along(fc, fc + X))
    G.add(C['steel'], lathe([(0.4007, -0.027), (0.4007, 0.027)], 32), along(fc, fc + X))
    G.add(paint, lathe([(0.0, -0.05), (0.075, -0.05), (0.075, 0.05), (0.0, 0.05)], 14), along(fc, fc + X))
    G.add(C['steel_dark'], nut_shape(0.03, 0.02), along(fc + X * 0.05, fc + X * 1.05))
    for k in range(6):
        t = math.radians(15 + 60 * k)
        d = Vector((0, math.cos(t), math.sin(t)))
        G.add(paint, lathe([(0.026, 0.0), (0.018, 0.29)], 6, sx=1.0, sy=0.55), along(fc + d * 0.07, fc + d * 0.36, up=X))
    belt = mat('riemen_leder', (0.22, 0.13, 0.07), 'fabric', r=1.2)
    bp = [fc + Vector((0.0, 0.404 * math.cos(math.radians(t)), 0.404 * math.sin(math.radians(t)))) for t in (100, 75, 50, 25, 0)]
    bp += [Vector((0.81, ys + 0.408, zs - 0.15)), Vector((0.81, ys + 0.425, 0.06)), Vector((0.81, ys + 0.47, 0.0085)), Vector((0.80, ys + 0.56, 0.0075))]
    G.add(belt, sweep([tuple(p) for p in bp], [(-0.034, 0.0), (0.034, 0.0), (0.034, 0.006), (-0.034, 0.006)], caps=True, up=(1, 0, 0)))
    # Spuren
    G.add(C['rust'], orient(strip_uv(_SX(0.7609), -0.72, 0.60, 0.004, 0.035, rng, 16, 0.8), NX))
    for _ in range(3):
        G.add(C['rust_stain'], orient(streak_uv(_SX(0.7609), rng.uniform(-0.6, 0.5), 0.12, 0.02, rng.uniform(0.05, 0.09), rng, 5), NX))
    pipe_spots(G, C, Pc, rng, 5, 0.06, 0.34, smax=0.03)
    chips_on_plane(G, C['steel_dark'], (0.7609, -0.06, 0.075), (0, 1, 0), (0, 0, 1), 1.30, 0.12, rng, 7, 0.004, 0.012)
    for (x, y) in ((0.30, 0.50), (0.62, -0.25)):
        G.add(mat('staub_dunkel', (0.34, 0.32, 0.28), 'concrete', r=1.5), blob((x, y, 0.1858), (1, 0, 0), (0, 1, 0), 0.10, 0.08, rng, 10, 2, 0.5, off=0.0))
    Pw = Pipe((cx, -0.50, 0.78), (cx, -0.50, 1.28), 0.10)
    for _ in range(5):
        G.add(C['rust'], orient(blob_uv(Pw.S(0.0014), rng.uniform(0, TAU), rng.uniform(0.05, 0.48), rng.uniform(0.2, 0.5), rng.uniform(0.02, 0.05), rng, 10, 2, 0.5), Pw.nrm))
    for _ in range(3):
        G.add(C['rust_stain'], orient(streak_uv(Pw.S(0.0012), rng.uniform(1.0, 4.0), rng.uniform(0.25, 0.45), 0.1, rng.uniform(0.12, 0.25), rng, 6), Pw.nrm))
    for y in (-0.62, -0.38):
        G.add(C['rust_stain'], orient(streak_uv(_SX(0.5509), y, 0.70, 0.02, 0.06, rng, 4), NX))
    G.add(C['oil'], blob((0.83, 0.10, 0.0005), (1, 0, 0), (0, 1, 0), 0.07, 0.16, rng, 12, 2, 0.5, off=0.0))
    G.build('kolbenpumpe', angle=40.0)


# ---------------------------------------------------------------------------------------------------------------------
# Kessel v1: 1930er liegender genieteter Druckkessel
# ---------------------------------------------------------------------------------------------------------------------
def build_mach_tank__v1():
    """1930er liegender Druckkessel, genietet (Wand bei x = 0): zwei Mantelschuesse mit Ueberlappungsnaehten und
    Nietreihen, Laengsnaht, gewoelbte Boeden, Dom mit verschraubtem Deckel und Hebelsicherheitsventil (Bock, Hebel,
    Gewicht, Fuehrung), ovales Mannloch mit zwei Buegeln, Wasserstandsglas mit Messinghaehnen und Kalkrand, Manometer
    auf Wassersackrohr, Speiseleitung aus der Wand mit Absperrschieber, Ablass mit Ventilrad, zwei gusseiserne Saettel
    mit Ankerschrauben; Mennige-Anstrich mit Rostinseln, Rostlaeufer unter den Nieten, Kalkfahne, Messingschild."""
    rng = random.Random(82)
    C = M_common()
    G = Geo()
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    shell = mat('kessel_mennige', (0.34, 0.17, 0.12), 'paint', tex='painted_metal_grey', r=1.2)
    cast = mat('guss_sattel', (0.12, 0.12, 0.11), 'paint', r=1.3)
    kalk = mat('kalk', (0.66, 0.64, 0.58), 'concrete', r=1.4)
    cx, cz, R = 0.52, 0.80, 0.40
    yl = 0.58
    Lt = 0.30 + 2 * yl
    prof = [(0.0, 0.0), (0.16, 0.012), (0.27, 0.04), (0.34, 0.075), (0.38, 0.11), (R, 0.15)]
    prof += [(R, 0.15 + t * 2 * yl) for t in (0.25, 0.5, 0.75)]
    prof += [(R, Lt - 0.15), (0.38, Lt - 0.11), (0.34, Lt - 0.075), (0.27, Lt - 0.04), (0.16, Lt - 0.012), (0.0, Lt)]
    y0 = -(yl + 0.15)
    G.add(shell, lathe(prof, 28), along((cx, y0, cz), (cx, y0 + 1, cz)))

    def on_shell(a, y, off=0.0):  # Winkel wie lathe entlang +y: 0 = Wand (-x), 90 = oben, 180 = Raum (+x)
        d = Vector((-math.cos(a), 0.0, math.sin(a)))
        return Vector((cx, y, cz)) + d * (R + off), d
    for ys_ in (-yl, 0.0, yl):  # Rundnaehte (Ueberlappung) mit Nietreihe
        G.add(shell, lathe([(R - 0.001, -0.032), (R + 0.0045, -0.029), (R + 0.0045, 0.029), (R - 0.001, 0.032)], 28), along((cx, ys_, cz), (cx, ys_ + 1, cz)))
        n = 26
        for j in range(n):
            p, d = on_shell(math.radians(40 + 280 * j / (n - 1)), ys_, 0.0045)
            rv(G, shell, p, d, 0.0065)
    al = math.radians(125)  # Laengsnaht oben vorn
    G.add(shell, lathe([(R - 0.001, -yl + 0.03), (R + 0.0045, -yl + 0.03), (R + 0.0045, yl - 0.03), (R - 0.001, yl - 0.03)], 3, a0=al - 0.07, a1=al + 0.07),
          along((cx, 0.0, cz), (cx, 1.0, cz)))
    for j in range(15):
        y = -yl + 0.06 + (2 * yl - 0.12) * j / 14
        if abs(y) < 0.04:
            continue
        p, d = on_shell(al, y, 0.0045)
        rv(G, shell, p, d, 0.0065)
    # Saettel
    def saddle_poly():
        pts = [(cx - 0.36, 0.0), (cx + 0.36, 0.0), (cx + 0.36, 0.035), (cx + 0.30, 0.06)]
        for j in range(9):
            q = math.radians(-35 - 110 * j / 8)
            pts.append((cx + (R + 0.004) * math.cos(q), cz + (R + 0.004) * math.sin(q)))
        pts += [(cx - 0.30, 0.06), (cx - 0.36, 0.035)]
        return pts
    for ys_ in (-0.40, 0.40):
        G.add(cast, prism(saddle_poly(), -(ys_ + 0.0125), -(ys_ - 0.0125)), PXZ)
        G.add(cast, box((cx - 0.37, ys_ - 0.09, 0.0), (cx + 0.37, ys_ + 0.09, 0.03), bev=0.004))
        G.add(cast, box((cx - 0.012, ys_ - 0.08, 0.03), (cx + 0.012, ys_ + 0.08, cz - R + 0.01)))
        G.add(cast, lathe([(R + 0.0008, -0.07), (R + 0.010, -0.07), (R + 0.010, 0.07), (R + 0.0008, 0.07)], 10, a0=math.radians(215), a1=math.radians(325), close=True),
              along((cx, ys_, cz), (cx, ys_ + 1, cz)))
        for s in (-1, 1):
            bolt(G, C['steel_dark'], (cx + s * 0.32, ys_, 0.03), Z, 0.011, stub=0.02)
        G.add(C['rust'], orient(strip_uv(_SX(cx + 0.3609), ys_ - 0.09, ys_ + 0.09, 0.0, 0.03, rng, 6, 0.8), NX))
    # Dom mit Hebelsicherheitsventil
    dy = 0.22
    dtop = cz + R + 0.20
    G.add(shell, cyl(0.15, dtop - (cz + R - 0.05), 20, cap=False), along((cx, dy, cz + R - 0.05), (cx, dy, dtop)))
    G.add(shell, lathe([(0.149, -0.025), (0.185, -0.025), (0.185, 0.0), (0.149, 0.0)], 20, close=True), Matrix.Translation((cx, dy, dtop)))
    G.add(shell, lathe([(0.0, 0.0), (0.185, 0.0), (0.185, 0.026), (0.17, 0.034), (0.0, 0.04)], 20), Matrix.Translation((cx, dy, dtop)))
    for k in range(10):
        t = TAU * (k + 0.5) / 10
        bolt(G, C['steel_dark'], (cx + 0.168 * math.cos(t), dy + 0.168 * math.sin(t), dtop + 0.026), Z, 0.008, stub=0.008, washer=False)
    for k in range(14):
        t = TAU * k / 14
        rv(G, shell, (cx + 0.151 * math.cos(t), dy + 0.151 * math.sin(t), cz + R + 0.03), (math.cos(t), math.sin(t), 0.0), 0.006)
    vtop = dtop + 0.04
    vb = Vector((cx, dy + 0.04, vtop))
    G.add(C['brass'], lathe([(0.0, 0.0), (0.045, 0.0), (0.045, 0.012), (0.030, 0.02), (0.028, 0.075), (0.020, 0.085), (0.0, 0.088)], 12), Matrix.Translation(vb))
    G.add(C['steel'], cyl(0.006, 0.02, 6), along(vb + Z * 0.086, vb + Z * 0.106))
    G.add(cast, box((cx - 0.015, dy + 0.115, dtop + 0.034), (cx + 0.015, dy + 0.145, vtop + 0.125), bev=0.004))
    G.add(C['steel_dark'], box((cx - 0.007, dy - 0.40, vtop + 0.095), (cx + 0.007, dy + 0.15, vtop + 0.12), bev=0.002))
    G.add(C['steel'], cyl(0.006, 0.034, 6), along((cx - 0.017, dy + 0.13, vtop + 0.108), (cx + 0.017, dy + 0.13, vtop + 0.108)))
    wy = dy - 0.36
    G.add(C['steel_dark'], cyl(0.004, 0.035, 5), along((cx, wy, vtop + 0.095), (cx, wy, vtop + 0.06)))
    G.add(cast, lathe([(0.0, 0.0), (0.03, 0.006), (0.055, 0.04), (0.05, 0.08), (0.02, 0.10), (0.0, 0.10)], 12), Matrix.Translation((cx, wy, vtop - 0.04)))
    for s in (-1, 1):
        G.add(C['steel_dark'], box((cx + s * 0.016 - 0.004, dy - 0.31, cz + R - 0.012), (cx + s * 0.016 + 0.004, dy - 0.29, vtop + 0.14)))
    G.add(C['steel'], cyl(0.004, 0.04, 6), along((cx - 0.02, dy - 0.30, vtop + 0.132), (cx + 0.02, dy - 0.30, vtop + 0.132)))
    # Mannloch vorn mit zwei Buegeln
    mh = Vector((cx + R, -0.28, cz))
    G.add(shell, prism(circ(0.155, 18, sx=0.80, sy=1.0), -0.05, 0.035), FX(mh))
    G.add(shell, prism(circ(0.13, 18, sx=0.77, sy=1.0), 0.035, 0.05), FX(mh))
    for z in (-0.06, 0.06):
        G.add(C['steel_dark'], box((cx + R + 0.05, -0.28 - 0.14, cz + z - 0.012), (cx + R + 0.07, -0.28 + 0.14, cz + z + 0.012), bev=0.003))
        G.add(C['steel'], cyl(0.009, 0.05, 8), along((cx + R + 0.04, -0.28, cz + z), (cx + R + 0.09, -0.28, cz + z)))
        bolt(G, C['steel_dark'], (cx + R + 0.07, -0.28, cz + z), X, 0.016, washer=True)
    # Wasserstandsglas
    gy = 0.32
    xg = cx + R + 0.075
    for zz in (cz + 0.22, cz - 0.12):
        xs_ = cx + math.sqrt(R * R - (zz - cz) ** 2)
        G.add(C['brass'], cyl(0.013, xg - xs_ + 0.01, 10), along((xs_ - 0.01, gy, zz), (xg, gy, zz)))
        G.add(C['brass'], lathe([(0.0, -0.028), (0.019, -0.028), (0.019, 0.028), (0.0, 0.028)], 10), Matrix.Translation((xg, gy, zz)))
        G.add(C['brass'], cyl(0.005, 0.04, 6), along((xg, gy, zz), (xg + 0.04, gy, zz)))
        G.add(C['black'], box((xg + 0.04, gy - 0.004, zz - 0.03), (xg + 0.05, gy + 0.004, zz + 0.006), bev=0.002))
    G.add(mat('wasserglas', (0.20, 0.26, 0.24), 'glass'), cyl(0.011, 0.284, 10), along((xg, gy, cz - 0.092), (xg, gy, cz + 0.192)))
    G.add(kalk, torus(0.0115, 0.0025, 10, 4), Matrix.Translation((xg, gy, cz + 0.04)))
    for s in (-1, 1):
        G.add(C['steel'], cyl(0.003, 0.284, 5), along((xg + 0.017, gy + s * 0.017, cz - 0.092), (xg + 0.017, gy + s * 0.017, cz + 0.192)))
    G.add(C['brass'], cyl(0.006, 0.05, 6), along((xg, gy, cz - 0.148), (xg, gy, cz - 0.198)))
    # Manometer auf Wassersackrohr
    p0 = Vector((cx + R * 0.5, -0.50, cz + R * 0.866))
    G.add(C['steel_dark'], cyl(0.007, 0.17, 8), along(p0 - Z * 0.01, p0 + Z * 0.16))
    G.add(C['steel_dark'], torus(0.022, 0.006, 12, 5), Matrix.Translation(p0 + Z * 0.08 + Y * 0.022) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    gauge(G, C, p0 + Z * 0.16, X, 0.05, rng, stem=0.035)
    # Speiseleitung aus der Wand mit Schieber, Ablass mit Ventilrad
    zf = cz + 0.28
    xsf = cx - math.sqrt(R * R - 0.28 ** 2)
    PF = Pipe((0.0, -0.30, zf), (xsf + 0.03, -0.30, zf), 0.04)
    Vm = {'body': mat('guss_schieber', (0.20, 0.22, 0.22), 'paint', r=1.3), 'fl': mat('speiserohr', (0.16, 0.16, 0.15), 'paint', r=1.2), 'bolt': C['steel_dark']}
    vz = 0.12
    hl = max(0.16, 0.04 * 3.0) / 2
    for (v0, v1) in ((0.0, vz - hl - 0.002), (vz + hl + 0.002, PF.L)):
        G.add(Vm['fl'], cyl(0.04, v1 - v0, 12, cap=False), along(PF.a + PF.d * v0, PF.a + PF.d * v1))
    gate_valve(G, Vm, C, PF, vz, rng, R_wheel=0.075, stem_dir=(0, 0, 1), yoke=0.02, spokes=4)
    G.add(C['steel_dark'], lathe([(0.042, 0.0), (0.07, 0.0), (0.068, 0.008), (0.042, 0.014)], 12), along(PF.a, PF.a + X))
    xd_, yd_ = cx + 0.05, 0.05
    zb_ = cz - math.sqrt(R * R - 0.05 ** 2)
    G.add(C['steel_dark'], cyl(0.02, zb_ + 0.01 - 0.26, 10, cap=False), along((xd_, yd_, zb_ + 0.01), (xd_, yd_, 0.26)))
    G.add(C['brass'], lathe([(0.0, -0.035), (0.026, -0.03), (0.032, 0.0), (0.026, 0.03), (0.0, 0.035)], 10), Matrix.Translation((xd_, yd_, 0.24)))
    G.add(C['steel'], cyl(0.005, 0.05, 6), along((xd_, yd_, 0.24), (xd_ + 0.05, yd_, 0.24)))
    handwheel(G, C, (xd_ + 0.05, yd_, 0.24), X, 0.045, rng, spokes=4, paint=C['red'], seg=14)
    G.add(C['steel_dark'], cyl(0.016, 0.07, 10), along((xd_, yd_, 0.205), (xd_, yd_, 0.135)))
    # Messingschild
    G.add(C['brass'], box((cx + R + 0.001, 0.04 - 0.07, cz - 0.04), (cx + R + 0.003, 0.04 + 0.07, cz + 0.04), bev=0.0008))
    for i, w in enumerate((0.11, 0.08, 0.10, 0.06)):
        G.add(C['ink'], box((cx + R + 0.003, 0.04 - w / 2, cz + 0.022 - i * 0.015), (cx + R + 0.0034, 0.04 + w / 2, cz + 0.027 - i * 0.015)))
    for s in (-1, 1):
        rv(G, C['steel'], (cx + R + 0.003, 0.04 + s * 0.06, cz), X, 0.003)
    # Spuren: Laeufer unter den Nieten, Rostinseln, Kalk, Boden
    def Ssh(u, v):  # u = y, v = Bogenlaenge vorn nach oben (v = 0 auf Hoehe der Achse)
        a = math.pi - v / R
        d = Vector((-math.cos(a), 0.0, math.sin(a)))
        return Vector((cx, u, cz)) + d * (R + 0.0016), d
    nsh = lambda m: Vector((m.x - cx, 0.0, m.z - cz))  # noqa: E731
    for ys_ in (-yl, 0.0, yl):
        for _ in range(3):
            G.add(C['rust_stain'], orient(streak_uv(Ssh, ys_ + rng.uniform(-0.02, 0.02), rng.uniform(0.15, 0.45), 0.014, rng.uniform(0.12, 0.30), rng, 6), nsh))
    for _ in range(5):
        G.add(C['rust'], orient(blob_uv(Ssh, rng.uniform(-0.5, 0.5), rng.uniform(-0.35, 0.3), rng.uniform(0.02, 0.05), rng.uniform(0.02, 0.04), rng, 10, 2, 0.5), nsh))
    G.add(kalk, orient(streak_uv(Ssh, -0.28 + 0.01, -0.13, 0.03, 0.18, rng, 6, taper=0.4), nsh))
    G.add(C['rust_stain'], orient(streak_uv(Ssh, -0.28 - 0.03, -0.12, 0.02, 0.15, rng, 6), nsh))
    G.add(C['oil'], blob((xd_ + 0.03, yd_, 0.0005), (1, 0, 0), (0, 1, 0), 0.12, 0.10, rng, 12, 2, 0.5, off=0.0))
    G.add(mat('kalk_boden', (0.46, 0.44, 0.40), 'concrete', r=1.4), blob((xd_ + 0.03, yd_, 0.0008), (1, 0, 0), (0, 1, 0), 0.035, 0.028, rng, 10, 2, 0.5, off=0.0))
    G.build('kessel_genietet', angle=40.0)


# ---------------------------------------------------------------------------------------------------------------------
# Steigleitung v1: Gussfallrohr und verzinkte Wasserleitung mit Wasserzaehler
# ---------------------------------------------------------------------------------------------------------------------
def build_pipe_riser__v1():
    """Aeltere Steigleitungen an der Wand (oben mit Bogen in die Wand, unter den Rohrbuendeln): gusseisernes
    Fallrohr DN150 mit Muffen (Bleistemmung), Reinigungsoeffnung mit verschraubtem Deckel, zwei Gussschellen mit
    Stockschrauben, Moertelkragen am Boden; verzinkte Wasserleitung DN25 mit Muffe, Kugelhahn, Wasserzaehler (Messing,
    aufgeklappter Deckel, Zifferblatt, Anhaenger), Tempergussbogen, Rohrschellen; Rostlaeufer und Kalkfahne an der
    undichten Muffe, Weissrost, Rost am Fuss."""
    rng = random.Random(52)
    C = M_common()
    G = Geo()
    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    castm = mat('guss_rohr', (0.09, 0.09, 0.085), 'paint', tex='painted_metal_grey', r=1.3)
    leadm = mat('bleistemmung', (0.34, 0.35, 0.36), 'metal', r=1.0, m=0.5)
    kalk = mat('kalk', (0.66, 0.64, 0.58), 'concrete', r=1.4)
    mortar = mat('moertel', (0.42, 0.41, 0.38), 'concrete', r=1.4)
    wrust = mat('weissrost', (0.72, 0.72, 0.68), 'concrete', r=1.4)
    xA, yA, r = 0.115, -0.09, 0.079
    zt = 1.90
    PA = Pipe((xA, yA, 0.0), (xA, yA, zt), r)
    G.add(castm, cyl(r, zt, 14, cap=False), along(PA.a, PA.b))
    G.add(castm, torus(xA, r, 8, 14, 0.0, math.pi / 2), Matrix.Translation((0.0, yA, zt)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    for z in (0.62, 1.38, zt - 0.11):  # Muffen mit Bleistemmung
        G.add(castm, lathe([(r - 0.001, 0.0), (r + 0.016, 0.012), (r + 0.019, 0.025), (r + 0.019, 0.095), (r + 0.012, 0.105), (r - 0.001, 0.107)], 14), along((xA, yA, z), (xA, yA, z + 1)))
        G.add(leadm, lathe([(r + 0.0115, 0.1058), (r + 0.0005, 0.1076)], 14), along((xA, yA, z), (xA, yA, z + 1)))
    zc = 0.98  # Reinigungsoeffnung
    G.add(castm, box((xA + 0.04, yA - 0.058, zc - 0.10), (xA + r + 0.018, yA + 0.058, zc + 0.10), bev=0.012, seg=2))
    G.add(castm, box((xA + r + 0.018, yA - 0.050, zc - 0.090), (xA + r + 0.032, yA + 0.050, zc + 0.090), bev=0.006, seg=1))
    for sy in (-1, 1):
        for sz in (-1, 1):
            bolt(G, C['steel_dark'], (xA + r + 0.032, yA + sy * 0.036, zc + sz * 0.072), X, 0.008, stub=0.008, washer=False)
    for z in (0.32, 1.72):  # Gussschellen mit Ohren und Stockschraube
        G.add(castm, lathe([(r + 0.001, 0.0), (r + 0.012, 0.0), (r + 0.012, 0.04), (r + 0.001, 0.04)], 14, close=True), along((xA, yA, z), (xA, yA, z + 1)))
        for sy in (-1, 1):
            G.add(castm, box((xA - 0.012, yA + sy * (r + 0.008) - 0.017, z), (xA + 0.012, yA + sy * (r + 0.008) + 0.017, z + 0.04), bev=0.003))
            bolt(G, C['steel_dark'], (xA + 0.012, yA + sy * (r + 0.012), z + 0.02), X, 0.008, stub=0.01, washer=False)
        G.add(C['steel_dark'], cyl(0.008, xA - r - 0.012, 6), along((xA - r - 0.012, yA, z + 0.02), (0.0, yA, z + 0.02)))
        G.add(C['steel_dark'], nut_shape(0.013, 0.010), along((xA - r - 0.012, yA, z + 0.02), (-1.0, yA, z + 0.02)))
    G.add(mortar, lathe([(r + 0.001, 0.0), (r + 0.05, 0.0), (r + 0.035, 0.015), (r + 0.004, 0.03)], 14), Matrix.Translation((xA, yA, 0.0)))
    G.add(mortar, lathe([(r + 0.002, 0.0), (r + 0.035, 0.0), (r + 0.03, 0.01), (r + 0.002, 0.016)], 14), along((0.0, yA, zt + xA), (1.0, yA, zt + xA)))
    # Wasserleitung DN25
    xB, yB, rB = 0.062, 0.13, 0.0168
    zb = 1.80
    zm0, zm1 = 1.02, 1.20
    for (z0, z1) in ((0.0, zm0), (zm1, zb)):
        G.add(C['zinc'], cyl(rB, z1 - z0, 10, cap=False), along((xB, yB, z0), (xB, yB, z1)))
    G.add(C['zinc'], torus(0.032, rB + 0.0045, 6, 10, 0.0, math.pi / 2), Matrix.Translation((xB - 0.032, yB, zb)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
    G.add(C['zinc'], cyl(rB, xB - 0.032, 10, cap=False), along((xB - 0.032, yB, zb + 0.032), (0.0, yB, zb + 0.032)))
    bead = [(rB + 0.0005, 0.0), (rB + 0.0055, 0.002), (rB + 0.0068, 0.006), (rB + 0.0068, 0.018), (rB + 0.0055, 0.022), (rB + 0.0005, 0.024)]
    G.add(C['zinc'], lathe(bead, 10), along((xB, yB, zb - 0.024), (xB, yB, zb + 1)))
    G.add(C['zinc'], lathe(bead, 10), along((0.03, yB, zb + 0.032), (-1.0, yB, zb + 0.032)))
    G.add(C['zinc'], lathe(bead, 10), along((xB, yB, 0.42), (xB, yB, 1.42)))
    zv = 0.80  # Kugelhahn
    G.add(C['brass'], lathe([(rB + 0.001, 0.0), (0.026, 0.012), (0.030, 0.032), (0.026, 0.052), (rB + 0.001, 0.064)], 10), along((xB, yB, zv - 0.032), (xB, yB, zv + 1)))
    for z in (zv - 0.044, zv + 0.032):
        G.add(C['brass'], nut_shape(0.025, 0.012), along((xB, yB, z), (xB, yB, z + 1)))
    G.add(C['steel'], cyl(0.005, 0.016, 6), along((xB + 0.028, yB, zv), (xB + 0.044, yB, zv)))
    lp = Vector((xB + 0.044, yB, zv))
    G.add(C['red'], box((-0.007, -0.0022, 0.0), (0.007, 0.0022, 0.10), bev=0.0015), along(lp, lp + Vector((0.12, 0.0, -1.0)), up=X))
    for (z, s) in ((zm0, 1), (zm1, -1)):  # Wasserzaehler
        G.add(C['brass'], nut_shape(0.024, 0.018), along((xB, yB, z - (0.018 if s > 0 else 0.0)), (xB, yB, z + 1)))
    G.add(C['brass'], lathe([(rB + 0.001, 0.0), (0.024, 0.008), (0.030, 0.03), (0.030, 0.15), (0.024, 0.172), (rB + 0.001, 0.18)], 12), along((xB, yB, zm0), (xB, yB, zm1)))
    zmid = (zm0 + zm1) / 2
    hc = Vector((xB + 0.02, yB, zmid))
    G.add(C['brass'], lathe([(0.0, 0.0), (0.044, 0.0), (0.044, 0.028), (0.040, 0.034), (0.0, 0.034)], 16), along(hc, hc + X))
    fzm = hc + X * 0.0342
    G.add(mat('skala', (0.86, 0.85, 0.80), 'paint', r=0.9), lathe([(0.0, 0.0), (0.036, 0.0), (0.0, 0.0004)], 16), along(fzm, fzm + X))
    G.add(C['black'], box((fzm.x + 0.0004, yB - 0.018, zmid + 0.008), (fzm.x + 0.0008, yB + 0.018, zmid + 0.018)))
    for i in range(5):
        G.add(C['paper'], box((fzm.x + 0.0008, yB - 0.016 + i * 0.0068, zmid + 0.010), (fzm.x + 0.0011, yB - 0.012 + i * 0.0068, zmid + 0.016)))
    for i in range(8):
        a = TAU * i / 8
        q0 = fzm + X * 0.0006 + Vector((0, math.cos(a), math.sin(a))) * 0.008 + Vector((0, 0, -0.014))
        G.add(C['ink'], lathe([(0.0008, 0.0), (0.0008, 0.004)], 3), along(q0, q0 + Vector((0, math.cos(a), math.sin(a)))))
    G.add(C['red'], cyl(0.0011, 0.009, 4), along(fzm + X * 0.0012 + Vector((0, 0, -0.014)), fzm + X * 0.0012 + Vector((0, 0.6, -0.014 + 0.8))))
    hinge = hc + X * 0.034 + Vector((0, 0, 0.045))
    Ml = Matrix.Translation(hinge) @ Matrix.Rotation(-math.radians(110), 4, 'Y') @ Matrix.Translation(-hinge) @ Matrix.Translation(hc + X * 0.034) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
    G.add(C['black'], lathe([(0.0, 0.0), (0.046, 0.0), (0.046, 0.006), (0.0, 0.006)], 16), Ml)
    G.add(C['steel'], cyl(0.003, 0.03, 5), along(hinge - Y * 0.015, hinge + Y * 0.015))
    Mt = Matrix.Translation((xB + 0.036, yB + 0.04, zm0 - 0.06)) @ Matrix.Rotation(0.25, 4, 'X')  # Anhaenger
    G.add(mat('pappe', (0.62, 0.52, 0.36), 'cardboard', r=1.3), box((0.0, -0.016, -0.028), (0.0008, 0.016, 0.028)), Mt)
    for i in range(3):
        G.add(C['ink'], box((0.0008, -0.012, 0.01 - i * 0.01), (0.0011, -0.012 + rng.uniform(0.012, 0.024), 0.013 - i * 0.01)), Mt)
    G.add(C['steel_dark'], cyl(0.0007, 0.075, 3), along((xB + 0.028, yB + 0.02, zm0 + 0.01), (xB + 0.036, yB + 0.04, zm0 - 0.035)))
    for z in (0.30, 1.55):  # Rohrschellen
        G.add(C['zinc'], _saddle(xB, rB), Matrix.Translation((0, yB, z)))
        for sy in (-1, 1):
            screw_head(G, C['steel'], (0.0018, yB + sy * (rB + 0.011), z), X, 0.0035, kind='flat', seg=5)
    # Spuren
    rust_ring(G, C, PA, 0.0, rng, 0.10, 1, heavy=True)
    G.add(C['rust_stain'], orient(streak_uv(PA.S(0.0018), math.pi / 2 + 0.25, 0.618, 0.05, 0.36, rng, 7), PA.nrm))
    G.add(kalk, orient(streak_uv(PA.S(0.0026), math.pi / 2 - 0.05, 0.618, 0.03, 0.30, rng, 7, taper=0.5), PA.nrm))
    G.add(kalk, orient(blob_uv(PA.S(0.0034), math.pi / 2 + 0.1, 0.60, 0.25, 0.012, rng, 10, 2, 0.5), PA.nrm))
    for _ in range(4):
        G.add(C['rust'], orient(blob_uv(PA.S(0.0014), rng.uniform(0.5, 2.6), rng.uniform(0.2, 1.8), rng.uniform(0.15, 0.35), rng.uniform(0.02, 0.05), rng, 10, 2, 0.5), PA.nrm))
    G.add(C['rust_stain'], orient(streak_uv(_SX(xA + r + 0.0329), yA + 0.036, zc - 0.075, 0.008, 0.10, rng, 5), NX))
    PB = Pipe((xB, yB, 0.0), (xB, yB, zb), rB)
    for (u, v) in ((math.pi / 2, 0.40), (math.pi / 2 + 0.6, 0.47), (math.pi / 2 - 0.4, 1.75), (math.pi / 2, 0.06)):
        G.add(wrust, orient(blob_uv(PB.S(0.0012), u, v, 0.5, 0.012, rng, 8, 1, 0.5), PB.nrm))
    rust_ring(G, C, PB, 0.0, rng, 0.05, 1)
    G.add(C['grime'], blob((xA + 0.07, yA, 0.0005), (1, 0, 0), (0, 1, 0), 0.12, 0.10, rng, 12, 2, 0.5, off=0.0))
    G.add(C['rust_stain'], orient(streak_uv(lambda u, v: (Vector((0.0015, u, v)), Vector((1, 0, 0))), yA, zt + xA - r - 0.02, 0.06, 0.35, rng, 6), NX))
    G.build('steigleitung_guss', angle=40.0)


# =====================================================================================================================
# V12-Fassung (noch nicht ersetzt)
# =====================================================================================================================
def _cyl(name, a, b, r, mat, seg=20, caps=True):
    """Zylinder von Punkt a nach Punkt b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=r, depth=d.length, location=(a + b) / 2)
    ob = bpy.context.active_object
    ob.name = name
    ob.rotation_euler = d.to_track_quat('Z', 'Y').to_euler()
    if not caps:
        pass
    ob.data.materials.append(mat)
    common.shade_auto(ob, 40.0)
    return ob


def _flange(name, p, axis, r, mat):
    ax = Vector(axis).normalized()
    return _cyl(name, Vector(p) - ax * 0.015, Vector(p) + ax * 0.015, r * 1.45, mat, seg=20)


STEEL_G = (0.34, 0.37, 0.36)
RUST = (0.40, 0.26, 0.16)


def _mats():
    return {
        'grey': common.material('grau', (0.42, 0.45, 0.44), look='paint', rough=0.55),
        'green': common.material('gruen', (0.26, 0.31, 0.27), look='paint', rough=0.65),
        'beige': common.material('beige', (0.62, 0.58, 0.48), look='paint', rough=0.6),
        'dark': common.material('dunkel', (0.12, 0.12, 0.12), look='plastic', rough=0.6),
        'steel': common.material('stahl', (0.55, 0.56, 0.56), look='metal', rough=0.4, metal=0.9),
        'rust': common.material('rost', RUST, look='metal', tex='metal_rusty_iron', rough=0.8, metal=0.4),
        'pipe': common.material('rohr', (0.36, 0.38, 0.38), look='galvanized', rough=0.55, metal=0.7),
        'pipe_red': common.material('rohr_rot', (0.36, 0.15, 0.12), look='paint', rough=0.7),
        'pipe_yel': common.material('rohr_gelb', (0.52, 0.44, 0.20), look='paint', rough=0.7),
        'glass': common.material('glas', (0.75, 0.78, 0.74), look='glass', rough=0.1),
        'black': common.material('schwarz', (0.02, 0.02, 0.02), look='glass', rough=0.05),
        'red': common.material('rot', (0.46, 0.12, 0.09), look='paint', rough=0.6),
    }


# --- Maschinen ---------------------------------------------------------------------------------------------------
# --- Licht --------------------------------------------------------------------------------------------------------
# --- Verschlossene Tueren (Wandteil) ------------------------------------------------------------------------------
def _locked_door(leaf_rgb, kind, sign=None, H=2.05, W=0.92, seed=1):
    rng = random.Random(seed)
    M = _mats()
    frame = common.material('zarge', (0.30, 0.31, 0.30), look='paint', rough=0.5)
    look = {'steel': 'paint', 'plywood': 'wood', 'wood': 'wood'}[kind]
    leaf = common.material(f'blatt_{kind}', leaf_rgb, look=look, rough=0.6 if kind == 'steel' else 0.8)
    t = 0.05
    common.box('zarge_l', (0.0, W / 2, 0.0), (0.06, W / 2 + t, H + t), frame, bevel=0.004)
    common.box('zarge_r', (0.0, -W / 2 - t, 0.0), (0.06, -W / 2, H + t), frame, bevel=0.004)
    common.box('zarge_o', (0.0, -W / 2 - t, H), (0.06, W / 2 + t, H + t), frame, bevel=0.004)
    common.box('blatt', (0.0, -W / 2, 0.0), (0.035, W / 2, H), leaf, bevel=0.004)
    if kind == 'steel':
        for i in range(2):  # gepraegte Quadrate
            for k in range(2):
                y0, z0 = -W / 2 + 0.1 + i * (W / 2 - 0.05), 0.25 + k * 0.95
                if sign == 'window' and k == 1:
                    continue
                common.box(f'praegung{i}{k}', (0.035, y0, z0), (0.042, y0 + W / 2 - 0.15, z0 + 0.75), leaf, bevel=0.01)
    if kind == 'plywood':
        for k in range(3):
            common.box(f'fuge{k}', (0.035, -W / 2, 0.6 + k * 0.6), (0.037, W / 2, 0.605 + k * 0.6), M['dark'])
    if kind == 'wood':
        for k in range(5):
            y0 = -W / 2 + k * W / 5
            common.box(f'brett{k}', (0.035, y0 + 0.004, 0.02), (0.045, y0 + W / 5 - 0.004, H - 0.02), leaf, bevel=0.003)
    if sign == 'window':
        common.box('fenster_rahmen', (0.035, -0.22, 1.3), (0.05, 0.22, 1.75), frame, bevel=0.004)
        common.box('fenster', (0.05, -0.19, 1.33), (0.052, 0.19, 1.72), M['black'])  # dahinter: reines Schwarz
    # Klinke und Schloss
    _cyl('rosette', (0.035, -W / 2 + 0.1, 1.05), (0.045, -W / 2 + 0.1, 1.05), 0.03, M['steel'], seg=16)
    _cyl('klinke', (0.06, -W / 2 + 0.1, 1.05), (0.06, -W / 2 + 0.24, 1.05), 0.011, M['steel'], seg=10)
    _cyl('klinke_hals', (0.045, -W / 2 + 0.1, 1.05), (0.065, -W / 2 + 0.1, 1.05), 0.011, M['steel'], seg=10)
    _cyl('schloss', (0.035, -W / 2 + 0.1, 0.92), (0.045, -W / 2 + 0.1, 0.92), 0.018, M['steel'], seg=12)
    if sign == 'warn':  # Warnschild ohne Schrift: gelbes Dreieck mit schwarzem Rand und Ausrufezeichen
        yel = common.material('warn_gelb', (0.85, 0.68, 0.10), look='paint', rough=0.5)
        bm = bmesh.new()
        z0, s = 1.45, 0.24
        vs = [bm.verts.new((0.046, 0.0, z0 + s * 0.866)), bm.verts.new((0.046, -s / 2 * 1.0, z0)), bm.verts.new((0.046, s / 2, z0))]
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()
        common.obj_from_bmesh('warn', bm, yel)
        common.box('warn_strich', (0.047, -0.01, z0 + 0.06), (0.048, 0.01, z0 + 0.15), M['dark'])
        common.box('warn_punkt', (0.047, -0.01, z0 + 0.025), (0.048, 0.01, z0 + 0.045), M['dark'])
    if sign == 'plate':  # Blechschild mit eingeschlagenen Strichen (unleserlich)
        common.box('schild', (0.036, -0.15, 1.5), (0.04, 0.15, 1.62), M['steel'], bevel=0.003)
        for k in range(3):
            common.box(f'zeile{k}', (0.04, -0.12, 1.52 + k * 0.03), (0.041, -0.12 + rng.uniform(0.1, 0.22), 1.535 + k * 0.03), M['dark'])
    if sign == 'spray':  # SprÃ¼hfarbe: zwei schraege Striche
        sp = common.material('spray', (0.70, 0.14, 0.12), look='paint', rough=0.9)
        for k in range(2):
            ob = common.box(f'spray{k}', (0.036, -0.3, -0.02), (0.037, 0.3, 0.02), sp)
            ob.location = (0, 0.05 * k, 1.2 + 0.15 * k)
            ob.rotation_euler = (0.5 - k, 0, 0)


def build_door_oversize():
    _locked_door((0.46, 0.48, 0.47), 'steel', 'warn', H=2.5, W=1.8, seed=7)


# --- Gegenstaende -----------------------------------------------------------------------------------------------------
# --- Gleis und Wagen ---------------------------------------------------------------------------------------------------
# --- Abgeschraegte Ecke (45-Grad-Knick) ---------------------------------------------------------------------------------
def _chamfer(H):
    """Dreiecksprisma in einer Kachel (Ursprung Kachelmitte am Boden): fuellt die Ecke -x/-y, schraege Flaeche blickt
    nach +x+y. Beton wie die Tunnelwand."""
    conc = common.material('beton', (0.70, 0.68, 0.64), look='concrete', tex='concrete_chalky')
    bm = bmesh.new()
    tri = [(-0.5, -0.5), (0.5, -0.5), (-0.5, 0.5)]
    lo = [bm.verts.new((x, y, 0.0)) for x, y in tri]
    hi = [bm.verts.new((x, y, H)) for x, y in tri]
    bm.faces.new(lo[::-1])
    bm.faces.new(hi)
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('schraege', bm, conc)
    common.shade_auto(ob, 30.0)


def build_chamfer_32():
    _chamfer(3.2)
