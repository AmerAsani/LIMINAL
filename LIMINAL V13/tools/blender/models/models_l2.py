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
        'rust_stain': mat('rostfahne', (0.42, 0.22, 0.11), 'paint', r=1.3, m=0.0),
        'steel': mat('stahl_blank', (0.52, 0.52, 0.50), 'metal', r=0.75, m=0.8),
        'steel_dark': mat('stahl_dunkel', (0.30, 0.30, 0.29), 'metal', r=0.9, m=0.65),
        'zinc': mat('verzinkt', (0.50, 0.51, 0.50), 'galvanized', r=1.2, m=0.5),
        'oil': mat('oel', (0.07, 0.06, 0.05), 'paint', r=0.32, m=0.0),
        'grease': mat('fett', (0.12, 0.10, 0.07), 'paint', r=0.45, m=0.0),
        'dust': mat('staub', (0.52, 0.49, 0.44), 'concrete', r=1.5, m=0.0),
        'grime': mat('schmutz', (0.20, 0.18, 0.15), 'paint', r=1.35, m=0.0),
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


def flange(G, mats, p, axis, rp, rf, t=0.022, nb=8, side=1, hub=True, blind=False, seg=18, bolt_s=None, stub=0.012,
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
    seg2 = 6
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
            G.add(paint, lathe([(0, 0), (ww0, 0), (ww1, L), (0, L)], 6, sx=1.0, sy=0.65), Ms)
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
        G.add(holder, box((x - 0.011, -0.017, zr - 0.018), (x + 0.011, 0.017, zb + 0.001), bev=0.004))
    L = 0.575  # Roehre: Glas mit geschwaerzten Enden, Alu-Kappen mit zwei Stiften
    for x0, x1, m_ in ((-L, -L + 0.035, endb), (-L + 0.035, L - 0.035, glass), (L - 0.035, L, endb)):
        G.add(m_, lathe([(0.013, 0.0), (0.013, x1 - x0)], 10), along((x0, 0, zr), (x1, 0, zr)))
    for s in (-1, 1):
        G.add(C['alu'], lathe([(0.0, 0.0), (0.0136, 0.0), (0.0136, 0.014), (0.0122, 0.016), (0, 0.016)], 8),
              along((s * L, 0, zr), (s * (L + 0.016), 0, zr)))
        for dy in (-0.006, 0.006):
            G.add(C['steel'], cyl(0.0012, 0.006, 4), along((s * (L + 0.016), dy, zr), (s * (L + 0.022), dy, zr)))
    for x in (-0.42, 0.42):  # Deckenrosette, Haken, Drahtseil, Seilklemme
        G.add(body, lathe([(0, -0.006), (0.014, -0.005), (0.018, 0), (0, 0)], 8), Matrix.Translation((x, 0, 0)))
        G.add(C['steel'], torus(0.005, 0.0011, 6, 3), mt((x, 0, -0.012), (math.pi / 2, 0, 0)))
        G.add(C['steel'], cyl(0.0009, abs(zt + 0.012) - 0.006, 4), along((x, 0, -0.016), (x, 0, zt + 0.004)))
        G.add(C['steel_dark'], cyl(0.0035, 0.012, 6), along((x, 0, zt + 0.014), (x, 0, zt + 0.002)))
    p0 = Vector((0.627, 0.0, -0.075))  # Kabel: Verschraubung an der Endkappe, Bogen zur Decke, Schelle
    G.add(C['black'], lathe([(0, 0), (0.007, 0), (0.007, 0.006), (0.005, 0.012), (0, 0.012)], 8), along(p0 - Vector((0.002, 0, 0)), p0 + Vector((0.01, 0, 0))))
    sag = rng.uniform(0.07, 0.1)
    pts = [p0 + Vector((0.012, 0, 0))]
    for i in range(1, 9):
        t = i / 8
        pts.append(Vector((0.64 + t * 0.36, 0.006 * math.sin(t * 5), min(-0.075 + t * 0.072 - sag * math.sin(math.pi * t) * (1 - t * 0.6), -0.004))))
    pts[-1] = Vector((1.0, 0.004, -0.004))
    G.add(C['black'], tube(pts[::2] + [pts[-1]], 0.0035, 5))
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
            G.add(C['zinc'], box((0.0, sy * Wy - 0.012, sz * (Hz + 0.02) - 0.02), (0.003, sy * Wy + 0.012, sz * (Hz + 0.02) + 0.02), bev=0.001))
            screw_head(G, C['steel'], (0.003, sy * Wy, sz * (Hz + 0.03)), (1, 0, 0), 0.006, C['black'])
    G.add(paint, box((0.0, -Wy, -Hz), (D, Wy, Hz), bev=0.009, seg=2))
    G.add(C['black'], box((D - 0.001, -Wy + 0.012, -Hz + 0.012), (D + 0.002, Wy - 0.012, Hz - 0.012)))  # Dichtung
    xd = D + 0.020
    G.add(paint, box((D + 0.002, -Wy + 0.006, -Hz + 0.006), (xd, Wy - 0.006, Hz - 0.006), bev=0.006, seg=2))
    for z in (-0.28, 0.28):  # Scharniere links
        G.add(C['zinc'], cyl(0.0065, 0.07, 10), along((D + 0.008, -Wy - 0.002, z - 0.035), (D + 0.008, -Wy - 0.002, z + 0.035)))
        G.add(C['zinc'], cyl(0.0035, 0.08, 6), along((D + 0.008, -Wy - 0.002, z - 0.04), (D + 0.008, -Wy - 0.002, z + 0.04)))
    for z in (-0.22, 0.22):  # Vierkantverschluesse
        c = Vector((xd, 0.24, z))
        G.add(C['black'], lathe([(0, 0), (0.016, 0), (0.016, 0.004), (0.013, 0.007), (0, 0.007)], 14), along(c, c + Vector((1, 0, 0))))
        G.add(C['steel'], box((0.0, -0.004, -0.004), (0.004, 0.004, 0.004)), Matrix.Translation(c + Vector((0.005, 0, 0))) @ Matrix.Rotation(0.4, 4, 'X'))
        G.add(C['grime'], blob(c, (0, 1, 0), (0, 0, 1), 0.045, 0.06, rng, 12, 2, 0.4, off=0.0009))
    for k in range(6):  # Lueftungskiemen
        z = -Hz + 0.06 + k * 0.016
        G.add(C['hole'], box((xd - 0.0005, -0.12, z), (xd + 0.0004, 0.12, z + 0.006)))
        G.add(paint, box((xd, -0.12, z + 0.006), (xd + 0.007, 0.12, z + 0.0085), bev=0.0008))
    _warn_triangle(G, C, FX((xd + 0.0002, -0.04, 0.20)), 0.10)
    _plate_lines(G, C, FX((xd + 0.0002, -0.15, 0.32)), 0.07, 0.035, rng, 3)
    tape = mat('kreppband', (0.80, 0.76, 0.62), 'paint', r=1.35)
    G.add(tape, box((0.0, -0.08, -0.0125), (0.0006, 0.08, 0.0125)), Matrix.Translation((xd, 0.02, -0.05)) @ Matrix.Rotation(0.03, 4, 'X'))
    for i in range(3):
        G.add(C['ink'], box((0.0006, -0.07 + i * 0.05, -0.004), (0.0009, -0.035 + i * 0.05, 0.004)), Matrix.Translation((xd, 0.02, -0.05)) @ Matrix.Rotation(0.03, 4, 'X'))
    for y, r in ((-0.17, 0.016), (0.0, 0.0125), (0.15, 0.0125)):  # Verschraubungen und Rohre zur Decke
        top = Vector((0.11, y, Hz))
        G.add(C['zinc'], nut_shape(r * 1.55, 0.012), along(top, top + Vector((0, 0, 1))))
        G.add(C['zinc'], lathe([(0, 0), (r * 1.25, 0), (r * 1.2, 0.012), (r * 1.05, 0.018), (0, 0.018)], 10), along(top + Vector((0, 0, 0.012)), top + Vector((0, 0, 1))))
        path = [top + Vector((0, 0, 0.02)), Vector((0.11, y, Hz + 0.12)), Vector((r + 0.012, y, Hz + 0.26)), Vector((r + 0.012, y, 1.3))]
        G.add(C['zinc'], tube(path, r, 8, bend=0.06, bseg=3, caps=False))
        for z in (0.62, 1.1):
            G.add(C['zinc'], cyl(r * 1.18, 0.04, 10), along((r + 0.012, y, z), (r + 0.012, y, z + 0.04)))
        for z in (0.85, 1.22):
            G.add(C['zinc'], _saddle(r + 0.012, r), Matrix.Translation((0, y, z)))
            for sy in (-1, 1):
                screw_head(G, C['steel'], (0.0018, y + sy * (r + 0.011), z), (1, 0, 0), 0.0035, seg=5)
    G.add(C['zinc'], tube([(D - 0.01, -Wy + 0.002, -0.05), (D + 0.012, -Wy - 0.01, -0.02), (D + 0.012, -Wy + 0.004, 0.02)], 0.002, 5, sx=3.0, bend=0.008))
    Sb = lambda u, v: (Vector((xd + 0.0009, u, v)), Vector((1, 0, 0)))  # noqa: E731
    nx = lambda m: Vector((1, 0, 0))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(Sb, -Wy + 0.01, Wy - 0.01, -Hz + 0.006, 0.02, rng, 16, 0.8), nx))
    for y in (-0.25, 0.27, 0.24):
        G.add(C['rust_stain'], orient(streak_uv(Sb, y, -Hz + 0.03 if y != 0.24 else -0.23, 0.011, 0.04 if y != 0.24 else 0.12, rng, 5), nx))
    Sd = lambda u, v: (Vector((D * 0.5 + u, Wy + 0.0009, v)), Vector((0, 1, 0)))  # noqa: E731
    G.add(C['rust'], orient(strip_uv(Sd, -D * 0.45, D * 0.45, -Hz + 0.004, 0.025, rng, 10, 0.8), lambda m: Vector((0, 1, 0))))
    chips_on_plane(G, C['steel_dark'], (xd + 0.0009, 0.0, 0.0), (0, 1, 0), (0, 0, 1), 0.57, 0.77, rng, 9, 0.004, 0.012)
    G.deform(dent_fn((xd, 0.12, -0.25), (-1, 0, 0), 0.05, 0.006))
    G.build('verteiler', angle=40.0)


def build_mach_cabinet():
    """Schaltschrank 1000 x 500 x 2000 auf 100-mm-Sockel: zwei Tueren mit Schwenkhebelgriff, Sichtfenster mit
    Reiheneinbaugeraeten dahinter, Bedienfeld (Messinstrumente, Meldeleuchten, Wahlschalter, Not-Aus), Filterluefter,
    Typenschild, Warnzeichen, Kranoesen, Kabelrohre zur Decke; Rost am Sockel und an den Tuerunterkanten, Russfahne
    ueber dem Luefter, Abplatzer und Griffspuren, Delle."""
    rng = random.Random(41)
    C = M_common()
    G = Geo()
    paint = mat('schrank_lack', (0.50, 0.54, 0.49), 'paint', r=1.1)
    plinth = mat('sockel', (0.10, 0.10, 0.10), 'paint', r=1.2)
    plast = mat('kunststoff_grau', (0.56, 0.57, 0.55), 'plastic', r=1.0)
    D, W, H0, H1 = 0.50, 0.5, 0.10, 2.10
    for sx in (0, 1):  # Sockel: Eckstuecke und Blende
        for sy in (-1, 1):
            x0 = 0.01 if sx == 0 else D - 0.09
            G.add(plinth, box((x0, sy * W - (0.09 if sy > 0 else 0.0), 0.0), (x0 + 0.09, sy * W + (0.0 if sy > 0 else 0.09), H0), bev=0.004))
    G.add(plinth, box((0.03, -W + 0.09, 0.0), (D - 0.015, W - 0.09, H0 - 0.004), bev=0.002))
    G.add(paint, box((0.0, -W, H0), (D, W, H1), bev=0.006, seg=2))
    G.add(paint, box((0.0, -W - 0.004, H1 - 0.002), (D + 0.008, W + 0.004, H1 + 0.018), bev=0.005, seg=2))
    for sy in (-1, 1):  # Kranoesen
        for x in (0.07, D - 0.07):
            G.add(C['steel_dark'], torus(0.022, 0.006, 10, 4), mt((x, sy * (W - 0.07), H1 + 0.045), (math.pi / 2, 0, math.pi / 4 * sy)))
            G.add(C['steel_dark'], cyl(0.014, 0.012, 10), along((x, sy * (W - 0.07), H1 + 0.018), (x, sy * (W - 0.07), H1 + 0.03)))
    xd = D
    G.add(C['black'], box((xd - 0.001, -W + 0.01, H0 + 0.01), (xd + 0.003, W - 0.01, H1 - 0.01)))
    doors = [(-W + 0.004, -0.002), (0.002, W - 0.004)]
    for k, (y0, y1) in enumerate(doors):
        G.add(paint, box((xd + 0.003, y0, H0 + 0.006), (xd + 0.024, y1, H1 - 0.006), bev=0.004, seg=2),
              Matrix.Rotation(0.0 if k == 0 else 0.003, 4, 'X'))
        yh = y0 - 0.001 if k == 0 else y1 + 0.001
        for z in (0.35, 1.1, 1.85):  # Scharnierbolzen
            G.add(C['steel_dark'], cyl(0.006, 0.06, 8), along((xd + 0.012, yh, z), (xd + 0.012, yh, z + 0.06)))
    xf = xd + 0.024
    y = 0.035  # Schwenkhebelgriff mit Schliesszylinder (rechte Tuer an der Mittelfuge)
    G.add(C['black'], prism(rrect(0.04, 0.17, 0.01, 2), 0.0, 0.006), FX((xf, y, 1.05)))
    G.add(C['black'], box((xf + 0.006, y - 0.012, 0.99), (xf + 0.022, y + 0.012, 1.11), bev=0.005, seg=2))
    G.add(C['steel'], lathe([(0, 0), (0.008, 0), (0.008, 0.004), (0, 0.004)], 10), along((xf + 0.022, y, 1.09), (xf + 0.03, y, 1.09)))
    G.add(C['hole'], box((xf + 0.0255, y - 0.001, 1.086), (xf + 0.0262, y + 0.001, 1.094)))
    G.add(C['grime'], blob((xf + 0.0005, y, 1.05), (0, 1, 0), (0, 0, 1), 0.07, 0.12, rng, 14, 2, 0.4, off=0.0009))
    chips_on_plane(G, C['steel_dark'], (xf + 0.0009, y, 1.05), (0, 1, 0), (0, 0, 1), 0.10, 0.22, rng, 7, 0.003, 0.009, edge=False)
    yc, zc, fw, fh = -0.25, 1.45, 0.34, 0.40  # Sichtfenster mit Reiheneinbaugeraeten
    G.add(C['alu'], prism(rrect(fw + 0.03, fh + 0.03, 0.012, 2), 0.0, 0.008), FX((xf, yc, zc)))
    G.add(mat('glas_dunkel', (0.05, 0.06, 0.06), 'glass'), box((xf - 0.004, yc - fw / 2, zc - fh / 2), (xf + 0.0085, yc + fw / 2, zc + fh / 2)))
    face = mat('skala', (0.86, 0.85, 0.80), 'paint', r=0.9)  # Bedienfeld rechte Tuer
    for y in (0.15, 0.32):
        G.add(C['black'], box((xf, y - 0.048, 1.502), (xf + 0.012, y + 0.048, 1.598), bev=0.003))
        G.add(face, box((xf + 0.012, y - 0.04, 1.51), (xf + 0.0125, y + 0.04, 1.59)))
        for t in range(9):
            G.add(C['ink'], box((xf + 0.0125, -0.0006, 0.028), (xf + 0.0128, 0.0006, 0.036)), Matrix.Translation((0, y, 1.52)) @ Matrix.Rotation(math.radians(-45 + t * 11.25), 4, 'X'))
        G.add(C['red'], box((xf + 0.0128, -0.0008, 0.0), (xf + 0.0132, 0.0008, 0.05)), Matrix.Translation((0, y, 1.52)) @ Matrix.Rotation(rng.uniform(-0.5, 0.3), 4, 'X'))
    for k, rgb in enumerate([(0.62, 0.10, 0.08), (0.15, 0.55, 0.15), (0.80, 0.55, 0.10), (0.85, 0.85, 0.80)]):
        y = 0.10 + k * 0.08
        G.add(C['steel'], lathe([(0, 0), (0.016, 0), (0.016, 0.006), (0.013, 0.008), (0, 0.008)], 12), along((xf, y, 1.36), (xf + 1, y, 1.36)))
        G.add(mat(f'meldeleuchte{k}', rgb, 'glass', r=1.5), dome(0.011, 0.012, 10), along((xf + 0.008, y, 1.36), (xf + 1, y, 1.36)))
    G.add(C['steel'], lathe([(0, 0), (0.016, 0), (0.016, 0.006), (0, 0.006)], 12), along((xf, 0.14, 1.22), (xf + 1, 0.14, 1.22)))
    G.add(C['black'], box((0.0, -0.006, -0.02), (0.022, 0.006, 0.02), bev=0.003), Matrix.Translation((xf + 0.006, 0.14, 1.22)) @ Matrix.Rotation(0.6, 4, 'X'))
    G.add(C['warn_yel'], lathe([(0, 0), (0.035, 0), (0.035, 0.004), (0, 0.004)], 16), along((xf, 0.32, 1.22), (xf + 1, 0.32, 1.22)))
    G.add(C['red'], lathe([(0, 0), (0.012, 0), (0.012, 0.018), (0.022, 0.02), (0.022, 0.028), (0.018, 0.034), (0, 0.036)], 14), along((xf + 0.004, 0.32, 1.22), (xf + 1, 0.32, 1.22)))
    _plate_lines(G, C, FX((xf + 0.0002, 0.25, 1.92)), 0.12, 0.06, rng, 5)
    _warn_triangle(G, C, FX((xf + 0.0002, -0.25, 1.76)), 0.10)
    for y in (-0.25, 0.25):  # Filterluefter mit Lamellen
        G.add(plast, box((xf, y - 0.125, 0.25), (xf + 0.014, y + 0.125, 0.50), bev=0.004))
        for k in range(9):
            z = 0.272 + k * 0.024
            G.add(C['hole'], box((xf + 0.0135, y - 0.105, z), (xf + 0.0145, y + 0.105, z + 0.014)))
            G.add(plast, box((xf + 0.012, y - 0.105, z + 0.012), (xf + 0.022, y + 0.105, z + 0.016)))
    nx = lambda m: Vector((1, 0, 0))  # noqa: E731
    Sf = lambda u, v: (Vector((xf + 0.0009, u, v)), Vector((1, 0, 0)))  # noqa: E731
    G.add(mat('russ', (0.06, 0.055, 0.05), 'paint', r=1.4), orient(streak_uv(Sf, -0.25, 0.51, 0.16, 0.30, rng, 7, down=1.0, taper=0.9), nx))
    G.add(C['zinc'], box((0.08, 0.17, H1 + 0.018), (0.26, 0.40, H1 + 0.024), bev=0.002))  # Kabelrohre zur Decke
    for (x, y, r) in ((0.15, 0.30, 0.04), (0.3, -0.25, 0.025), (0.2, 0.22, 0.016)):
        G.add(C['zinc'], cyl(r * 1.3, 0.03, 12), along((x, y, H1 + 0.018), (x, y, H1 + 0.048)))
        G.add(C['zinc'], cyl(r, 2.6 - H1 - 0.04, 14, cap=False), along((x, y, H1 + 0.04), (x, y, 2.6)))
        G.add(C['zinc'], cyl(r * 1.15, 0.035, 14), along((x, y, 2.38), (x, y, 2.415)))
    for (y0, y1) in doors:  # Rost an den Tuerunterkanten, Laeufer
        G.add(C['rust'], orient(strip_uv(Sf, y0 + 0.004, y1 - 0.004, H0 + 0.007, 0.035, rng, 14, 0.8), nx))
        for _ in range(2):
            G.add(C['rust_stain'], orient(streak_uv(Sf, rng.uniform(y0 + 0.04, y1 - 0.04), H0 + 0.06, 0.012, 0.05, rng, 4, down=1.0), nx))
    for sy in (-1, 1):
        Ss = (lambda s: (lambda u, v: (Vector((u, s * (W + 0.0009), v)), Vector((0, s, 0)))))(sy)
        G.add(C['rust'], orient(strip_uv(Ss, 0.02, D - 0.02, H0 + 0.001, 0.05, rng, 10, 0.8), lambda m, s=sy: Vector((0, s, 0))))
        chips_on_plane(G, C['steel_dark'], (D / 2, sy * (W + 0.0009), 1.1), (-sy, 0, 0), (0, 0, 1), D, 1.9, rng, 8, 0.004, 0.012)
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


def gate_valve(G, Vm, C, P, v, rng, R_wheel=0.1, stem_dir=(1, 0, 0), yoke=0.03, rf=None, nb=None, wheel_paint=None):
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
        flange(G, {'fl': Vm['body'], 'bolt': Vm['bolt']}, fc, P.d * s, r * 1.05, rf, 0.02, 0, side=0, hub=False)
        flange(G, {'fl': Vm['fl'], 'bolt': Vm['bolt']}, fc + P.d * s * 0.002, -P.d * s, r, rf, 0.022, nb, side=-1)
    G.add(Vm['body'], lathe([(r * 1.04, 0), (r * 1.25, Lb * 0.18), (r * 1.32, Lb * 0.5), (r * 1.25, Lb * 0.82), (r * 1.04, Lb)], 18),
          along(c - P.d * (Lb / 2), c + P.d * (Lb / 2)))
    G.add(C['rust_dark'], lathe([(r * 1.335, Lb * 0.32), (r * 1.335, Lb * 0.68)], 8, a0=math.pi * 1.2, a1=math.pi * 1.8),
          along(c - P.d * (Lb / 2), c + P.d * (Lb / 2), up=sd))
    neck0, neck1 = c + sd * r * 0.6, c + sd * r * 1.25
    G.add(Vm['body'], lathe([(0, 0), (r * 1.05, 0), (r * 0.9, (neck1 - neck0).length), (0, (neck1 - neck0).length)], 16, sy=0.62),
          along(neck0, neck1, up=P.d))
    G.add(Vm['body'], lathe([(0, 0), (r * 1.35, 0), (r * 1.35, 0.018), (0, 0.018)], 18, sy=0.7), along(neck1, neck1 + sd, up=P.d))
    xx, yy, zz = frame_z(sd, up=P.d)
    for k in range(6):
        a = TAU * (k + 0.5) / 6
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
    spindle(G, C['steel'], bon1 + sd * 0.005, top + sd * 0.05, rs, pitch=0.007, seg=8, grease=C['grease'])
    handwheel(G, C, top + sd * 0.03, sd, R_wheel, rng, spokes=5 if R_wheel > 0.08 else 4, paint=wheel_paint or C['red'])
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
    for s in (1, -1):
        q = c + ad * (leg - 0.012) + side * rr * s
        G.add(matl, nut_shape(0.0085, 0.008), along(q, q + ad))


def build_pipe_riser():
    """Steigleitungen Boden-Decke an der Wand: DN100 (lackiert, Kennringe Wasser) mit zwei Flanschverbindungen,
    Absperrschieber mit Ventilrad zum Raum, Rostuebergaengen an Flanschen und am Boden; DN50 gedaemmt (Alu-Mantel mit
    Ueberlappungen, Spannbaendern, Endkappen; aufgerissene Stelle mit Mineralwolle und abstehendem Blech); zwei
    Wandkonsolen (C-Schiene, Ausleger, Rundstahlbuegel), Manschetten an Boden und Decke."""
    rng = random.Random(51)
    C = M_common()
    G = Geo()
    paintA = mat('steig_lack', (0.30, 0.38, 0.34), 'paint', tex='painted_metal_grey', r=1.15)
    Fm = {'fl': paintA, 'bolt': C['steel_dark']}
    xA, yA, rp, rf = 0.13, -0.14, 0.057, 0.11
    PA = Pipe((xA, yA, 0.0), (xA, yA, 2.6), rp)
    vz = 1.15
    Lb2 = max(0.16, rp * 3.0) / 2
    for z0, z1 in ((0.0, 0.4485), (0.4515, vz - Lb2 - 0.002), (vz + Lb2 + 0.002, 2.6)):
        G.add(paintA, cyl(rp, z1 - z0, 16, cap=False), along((xA, yA, z0), (xA, yA, z1)))
    flange_pair(G, Fm, PA, 0.45, rp, rf, 0.02, 8, C)
    Vm = {'body': mat('guss_schieber', (0.20, 0.22, 0.22), 'paint', r=1.3), 'fl': paintA, 'bolt': C['steel_dark']}
    gate_valve(G, Vm, C, PA, vz, rng, R_wheel=0.095, stem_dir=(1, 0, 0), yoke=0.025)
    green = mat('kennring_gruen', (0.15, 0.40, 0.20), 'paint', r=1.0)
    for z, m_ in ((1.58, green), (1.62, C['paper']), (1.66, green)):
        G.add(m_, cyl(rp + 0.0012, 0.04, 16, cap=False), along((xA, yA, z - 0.02), (xA, yA, z + 0.02)))
    rust_ring(G, C, PA, 0.0, rng, 0.09, 1, heavy=True)
    rust_ring(G, C, PA, 0.4285, rng, 0.05, -1)
    rust_ring(G, C, PA, 0.4715, rng, 0.04, 1)
    rust_ring(G, C, PA, 2.6, rng, 0.06, -1)
    for _ in range(3):
        G.add(C['rust_stain'], orient(streak_uv(PA.S(0.0014), rng.uniform(0, TAU), vz - Lb2 - 0.03, 0.25, rng.uniform(0.12, 0.3),
                                                rng, 6, down=-1.0), PA.nrm))
    # DN50 gedaemmt
    xB, yB, rB, rc = 0.095, 0.10, 0.030, 0.062
    PB = Pipe((xB, yB, 0.0), (xB, yB, 2.6), rB)
    G.add(C['steel_dark'], cyl(rB, 0.37, 12, cap=False), along((xB, yB, 0.0), (xB, yB, 0.37)))
    G.add(C['steel_dark'], cyl(rB, 0.21, 12, cap=False), along((xB, yB, 2.39), (xB, yB, 2.6)))
    rust_ring(G, C, PB, 0.0, rng, 0.12, 1, heavy=True)
    clad = mat('alu_mantel', (0.50, 0.51, 0.50), 'metal', r=1.2, m=0.55)
    wool = mat('mineralwolle', (0.60, 0.50, 0.27), 'fabric', r=1.5)
    Mb = lambda z0, z1: along((xB, yB, z0), (xB, yB, z1))  # noqa: E731
    G.add(clad, lathe([(rB + 0.0015, 0.0), (rc, 0.035)], 14), Mb(0.36, 1.0))
    G.add(clad, lathe([(rc, -0.035), (rB + 0.0015, 0.0)], 14), Mb(2.4, 3.0))
    for z0, z1 in ((0.395, 1.40), (1.38, 1.52), (1.80, 2.365)):
        G.add(clad, cyl(rc, z1 - z0, 14, cap=False), Mb(z0, z1))
        G.add(clad, cyl(rc + 0.0012, 0.03, 14, cap=False), Mb(z1 - 0.03, z1))
    G.add(clad, lathe([(rc, 0.0), (rc, 0.28)], 8, a0=math.radians(100), a1=math.radians(260)), Mb(1.52, 1.80))
    vs, fs = lathe([(0.050, 0.0), (0.058, 0.035), (0.056, 0.07), (0.059, 0.105), (0.055, 0.14), (0.060, 0.175), (0.056, 0.21), (0.058, 0.245), (0.050, 0.28)], 14,
                   a0=math.radians(-100), a1=math.radians(100))
    vs = [v + Vector((v.x, v.y, 0)).normalized() * 0.008 * nz(v, 70.0) for v in vs]
    G.add(wool, (vs, fs), Mb(1.52, 1.80))
    G.add(clad, lathe([(rc + 0.002, 0.0), (rc + 0.002, 0.10)], 4, a0=math.radians(-60), a1=math.radians(-20)), Mb(1.80, 2.0) @ Matrix.Rotation(-0.35, 4, 'Y'))
    for z in (0.75, 1.25, 1.95, 2.25):
        G.add(C['steel'], cyl(rc + 0.0022, 0.016, 14, cap=False), Mb(z, z + 0.016))
    # Wandkonsolen
    for z in (0.80, 1.95):
        G.add(C['zinc'], strut_h(-0.27, 0.19, z - 0.0205))
        for y in (-0.24, 0.16):
            bolt(G, C['steel'], (0.021, y, z), (1, 0, 0), 0.007, washer=True)
        for (x, y, r) in ((xA, yA, rp), (xB, yB, rc)):
            G.add(C['zinc'], box((0.021, y - 0.0205, z - 0.0205), (x - r - 0.004, y + 0.0205, z + 0.0205), bev=0.002))
            ubolt(G, C['zinc'], (x, y, z), (0, 0, 1), r, (-1, 0, 0), leg=x - 0.03)
    for (x, y, r) in ((xA, yA, rp), (xB, yB, rB)):  # Manschetten
        G.add(C['zinc'], lathe([(r + 0.002, 0.0), (r + 0.03, 0.0), (r + 0.028, 0.006), (r + 0.002, 0.012)], 16),
              Matrix.Translation((x, y, 0.0)))
        G.add(C['zinc'], lathe([(r + 0.002, -0.012), (r + 0.028, -0.006), (r + 0.03, 0.0), (r + 0.002, 0.0)], 16), Matrix.Translation((x, y, 2.6)))
    G.add(C['grime'], blob((xA + 0.08, yA, 0.0005), (1, 0, 0), (0, 1, 0), 0.10, 0.08, rng, 12, 2, 0.5, off=0.0))
    G.build('steigleitung', angle=40.0)


# ---------------------------------------------------------------------------------------------------------------------
# Rohrbuendel (5-m-Module, Ende an Ende): gemeinsame Hauptleitungen mit halben Flanschverbindungen an den Modulgrenzen
# ---------------------------------------------------------------------------------------------------------------------
TRUNK = [  # (x, z, r_rohr, r_flansch, Schrauben, Lack)
    (0.150, 2.28, 0.0699, 0.125, 8, 'grau'),
    (0.105, 2.02, 0.0445, 0.100, 8, 'rot'),
    (0.085, 1.86, 0.0302, 0.0825, 4, 'gelb'),
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
    arc = [Vector((x + (r + t) * math.cos(a), 0.0, z + (r + t) * math.sin(a))) for a in [-math.pi / 2 + math.pi * i / 6.0 for i in range(7)]]
    path = [Vector((0.0006, 0, z - r - 0.012)), Vector((0.0006, 0, z - r - t))] + arc + [Vector((0.0006, 0, z + r + t)), Vector((0.0006, 0, z + r + 0.012))]
    return sweep(path, [(-w, -t / 2), (w, -t / 2), (w, t / 2), (-w, t / 2)], up=(0, 1, 0))


def _bundle_trunk(G, C, PM, rng, skip=()):
    pipes = []
    for k, (x, z, r, rf, nb, pm) in enumerate(TRUNK):
        P = Pipe((x, -2.5, z), (x, 2.5, z), r)
        pipes.append(P)
        if k not in skip:
            G.add(PM[pm], cyl(r, 5.0, 18 if r > 0.05 else 14, cap=False), P.M)
        Fm = {'fl': PM[pm], 'bolt': C['steel_dark']}
        end_flange(G, Fm, P, True, r, rf, 0.022, nb, heads=True)
        end_flange(G, Fm, P, False, r, rf, 0.022, nb, heads=False)
        rust_ring(G, C, P, 0.056, rng, 0.05, 1)
        rust_ring(G, C, P, 4.944, rng, 0.05, -1)
        pipe_spots(G, C, P, rng, 3 + k)
    for j, (x, z) in enumerate(((0.016, 1.74), (0.040, 1.73))):  # Kabel in Durchhaengen
        rk = 0.0085 if j == 0 else 0.0065
        for s in range(4):
            y0, y1 = -2.5 + s * 1.25, -2.5 + (s + 1) * 1.25
            sag = 0.018 + 0.006 * ((s + j) % 2)
            pts = [Vector((x, y0 + (y1 - y0) * t / 6, z - sag * math.sin(math.pi * t / 6))) for t in range(7)]
            G.add(C['black'], tube(pts, rk, 6, caps=False))
        for y in (-1.25, 0.0, 1.25):
            G.add(C['zinc'], _saddle_h(x, z, rk), Matrix.Translation((0, y, 0)))
    for y in (-1.25, 1.25):  # Konsolen
        G.add(C['zinc'], strut_v(1.66, 2.42), Matrix.Translation((0.0, y, 0.0)))
        for z in (1.70, 2.38):
            bolt(G, C['steel'], (0.021, y, z), (1, 0, 0), 0.007)
        for (x, z, r, rf, nb, pm) in TRUNK:
            zt = z - r - 0.002
            G.add(C['zinc'], box((0.021, y - 0.0205, zt - 0.041), (x + r * 0.7, y + 0.0205, zt), bev=0.002))
            ubolt(G, C['zinc'], (x, y, z), (0, 1, 0), r, (0, 0, -1), leg=r + 0.05)
    return pipes


def _drip_wall(y, z0, rng):
    """Rostfahne auf der Wand (knapp vor x = 0) senkrecht nach unten."""
    S = lambda u, w: (Vector((0.0015, u, w)), Vector((1, 0, 0)))  # noqa: E731
    return orient(streak_uv(S, y, z0, 0.06, 0.8, rng, 8, down=-1.0, taper=0.6), lambda m: Vector((1, 0, 0)))


def build_pipe_bundle_a():
    """Rohrbuendel A (5 m, Wand bei x = 0): Hauptleitungen DN125 grau, DN80 rot, DN50 gelb mit halben Flanschverbindungen
    an den Modulenden (geschlossene Dichtflaechen), Konsolen alle 2,5 m, Kabel in Schellen; dazu Absperrschieber mit
    Ventilrad in der roten, Kompensator mit Zugankern und Manometer in der grauen Leitung, tiefe Gussleitung DN100 mit
    Muffen auf Bodenstuetzen; Rostuebergaenge, Laeufer."""
    rng = random.Random(61)
    C = M_common()
    PM = _bundle_mats()
    G = Geo()
    P1, P2, P3 = _bundle_trunk(G, C, PM, rng, skip=(0, 1))
    x1, z1 = P1.a.x, P1.a.z
    yk0, yk1 = -0.62, -0.38  # Kompensator (Faltenbalg) zwischen zwei Flanschen
    G.add(PM['grau'], cyl(P1.r, yk0 - 0.024 + 2.5, 18, cap=False), P1.M)
    G.add(PM['grau'], cyl(P1.r, 2.5 - yk1 - 0.024, 18, cap=False), along((x1, yk1 + 0.024, z1), (x1, 2.5, z1)))
    Fm1 = {'fl': PM['grau'], 'bolt': C['steel_dark']}
    flange(G, Fm1, (x1, yk0, z1), (0, 1, 0), P1.r, 0.125, 0.022, 8, side=1)
    flange(G, Fm1, (x1, yk1, z1), (0, -1, 0), P1.r, 0.125, 0.022, 8, side=1)
    prof = [(P1.r + (0.03 if i % 2 else 0.012), 0.004 + (yk1 - yk0 - 0.008) * i / 18) for i in range(19)]
    G.add(C['steel'], lathe(prof, 20), along((x1, yk0, z1), (x1, yk1, z1)))
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
    gate_valve(G, Vm, C, Pipe((P2.a.x, yv - 1, P2.a.z), (P2.a.x, yv + 1, P2.a.z), P2.r), 1.0, rng, R_wheel=0.075, stem_dir=(1, 0, 0), yoke=0.02, rf=0.1)
    P4 = Pipe((0.13, -2.5, 0.35), (0.13, 2.5, 0.35), 0.057)  # tiefe Gussleitung mit Muffen
    G.add(PM['guss'], cyl(P4.r, 5.0, 16, cap=False), P4.M)
    Fm4 = {'fl': PM['guss'], 'bolt': C['steel_dark']}
    end_flange(G, Fm4, P4, True, P4.r, 0.1, 0.02, 8, heads=True)
    end_flange(G, Fm4, P4, False, P4.r, 0.1, 0.02, 8, heads=False)
    for y in (-1.5, 1.0):
        G.add(PM['guss'], lathe([(P4.r, -0.01), (P4.r + 0.014, 0.0), (P4.r + 0.016, 0.012), (P4.r + 0.016, 0.085), (P4.r + 0.012, 0.095), (P4.r, 0.098)], 16),
              along((0.13, y, 0.35), (0.13, y + 1, 0.35)))
        rust_ring(G, C, P4, y + 2.5 + 0.1, rng, 0.05, 1, heavy=True)
    for y in (-2.0, -0.25, 1.75):  # Bodenstuetzen: Fussplatte, Stiel, Sattel, Buegel
        G.add(C['steel_dark'], box((0.06, y - 0.06, 0.0), (0.20, y + 0.06, 0.008), bev=0.002))
        for sx in (-1, 1):
            bolt(G, C['steel'], (0.13 + sx * 0.05, y, 0.008), (0, 0, 1), 0.006)
        G.add(C['steel_dark'], box((0.115, y - 0.015, 0.008), (0.145, y + 0.015, 0.35 - P4.r - 0.006), bev=0.002))
        G.add(C['steel_dark'], box((0.07, y - 0.02, 0.35 - P4.r - 0.012), (0.19, y + 0.02, 0.35 - P4.r - 0.002), bev=0.002))
        ubolt(G, C['zinc'], (0.13, y, 0.35), (0, 1, 0), P4.r, (0, 0, -1), leg=P4.r + 0.02)
        G.add(C['rust_stain'], blob((0.13, y, 0.0006), (1, 0, 0), (0, 1, 0), 0.09, 0.07, rng, 12, 2, 0.5, off=0.0))
    pipe_spots(G, C, P4, rng, 6, smax=0.05)
    for P in (P2, P3):
        pipe_spots(G, C, P, rng, 3, smax=0.02)
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
    Gc.add(clad, lathe([(P1.r + 0.001, 0.0), (rc, 0.05)], 20), My(-2.36, 0.0))
    Gc.add(clad, lathe([(rc, -0.05), (P1.r + 0.001, 0.0)], 20), My(2.36, 3.0))
    tear = (0.55, 0.85)
    y = -2.31
    while y < 2.31 - 1e-6:
        y1 = min(y + 1.0, 2.31)
        if y < tear[0] < y1:
            Gc.add(clad, cyl(rc, tear[0] - y, 20, cap=False), My(y, tear[0]))
            Gc.add(clad, lathe([(rc, 0.0), (rc, tear[1] - tear[0])], 12, a0=math.radians(250), a1=math.radians(510)), My(tear[0], tear[1]))
            vs, fs = lathe([(rc - 0.012, 0.0), (rc - 0.004, 0.05), (rc - 0.008, 0.10), (rc - 0.002, 0.15), (rc - 0.007, 0.2), (rc - 0.003, 0.25), (rc - 0.012, 0.30)], 10,
                           a0=math.radians(150), a1=math.radians(250))
            vs = [v + Vector((v.x, v.y, 0)).normalized() * 0.008 * nz(v, 45.0) for v in vs]
            G.add(wool, (vs, fs), My(tear[0], tear[1]))
            Gc.add(clad, cyl(rc, y1 - tear[1], 20, cap=False), My(tear[1], y1))
            Gc.add(clad, lathe([(rc + 0.002, 0.0), (rc + 0.002, 0.16)], 4, a0=math.radians(250), a1=math.radians(285)),
                   My(tear[0] + 0.05, tear[0] + 1) @ Matrix.Rotation(0.25, 4, 'X'))
        else:
            Gc.add(clad, cyl(rc, y1 - y, 20, cap=False), My(y, y1))
        if y1 < 2.31 - 1e-6:
            Gc.add(clad, cyl(rc + 0.0013, 0.035, 20, cap=False), My(y1 - 0.035, y1))
        y = y1
    for yb in (-1.9, -1.4, -0.9, -0.4, 0.1, 1.1, 1.6, 2.1):
        Gc.add(C['steel'], cyl(rc + 0.0025, 0.018, 20, cap=False), My(yb, yb + 0.018))
    Pc = Pipe((x, -2.31, z), (x, 2.31, z), rc)
    for _ in range(9):
        Gc.add(C['grime'], orient(blob_uv(Pc.S(0.0032), rng.uniform(-2.4, 0.6), rng.uniform(0.2, 4.4), rng.uniform(0.15, 0.35), rng.uniform(0.08, 0.3), rng, 10, 2, 0.5), Pc.nrm))
    for _ in range(6):
        Gc.add(C['dust'], orient(blob_uv(Pc.S(0.0036), rng.uniform(1.2, 1.9), rng.uniform(0.2, 4.4), 0.3, rng.uniform(0.2, 0.5), rng, 10, 2, 0.4), Pc.nrm))
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


def _volute(axis_c, r_hub, rng, seg=22, prs=8):
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
        for k in range(4):
            a = TAU * (k + 0.5) / 4
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
    for i in range(1, len(hp) - 1, 3):
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
    for k in range(8):
        a = TAU * (k + 0.5) / 8
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
    gauge(G, C, (ax_x + 0.07, 0.45, ax_z + 0.02), (1, 0, 0), 0.045, rng, stem=0.04)
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
    G.add(shell, lathe(prof, 30), Matrix.Translation(c))
    for z in (z0, 1.18, z1):  # Rundnaehte
        G.add(weld, torus(R + 0.001, 0.0045, 30, 3), Matrix.Translation(c + Vector((0, 0, z))))
    a_l = math.radians(-50)
    G.add(weld, cyl(0.0045, z1 - z0, 4), along(c + Vector((math.cos(a_l) * (R + 0.001), math.sin(a_l) * (R + 0.001), z0)),
                                              c + Vector((math.cos(a_l) * (R + 0.001), math.sin(a_l) * (R + 0.001), z1))))
    # Stuetzen (Rohr mit Fussplatte und Ankerschrauben, Verstaerkungsblech)
    for a in (math.radians(60), math.radians(180), math.radians(300)):
        p = c + Vector((math.cos(a) * 0.30, math.sin(a) * 0.30, 0.0))
        G.add(C['steel_dark'], cyl(0.038, 0.33, 12, cap=False), along(p, p + Vector((0, 0, 0.33))))
        G.add(C['steel_dark'], box((p.x - 0.07, p.y - 0.07, 0.0), (p.x + 0.07, p.y + 0.07, 0.012), bev=0.002))
        for k in range(2):
            q = p + Vector((math.cos(a + k * math.pi) * 0.05, math.sin(a + k * math.pi) * 0.05, 0.012))
            bolt(G, C['steel'], q, (0, 0, 1), 0.009, stub=0.012)
        G.add(shell, box((-0.06, -0.05, -0.004), (0.06, 0.05, 0.004)), Matrix.Translation(p + Vector((0, 0, 0.335))) @ Matrix.Rotation(a, 4, 'Z') @ Matrix.Rotation(-0.35, 4, 'Y'))
        G.add(C['rust'], orient(strip_uv(Pipe(p, p + Vector((0, 0, 0.33)), 0.038).S(0.0012), 0, TAU, 0.0, 0.06, rng, 10, 0.8),
                                Pipe(p, p + Vector((0, 0, 0.33)), 0.038).nrm))
    # Handloch vorn (Richtung +x, leicht seitlich) mit Deckel, Schrauben, Griff
    ah = math.radians(15)
    dh = Vector((math.cos(ah), math.sin(ah), 0.0))
    ph = c + Vector((0, 0, 0.82)) + dh * (R - 0.02)
    G.add(shell, cyl(0.12, 0.09, 20, cap=False), along(ph, ph + dh * 0.09))
    Fh = {'fl': shell, 'bolt': C['steel_dark']}
    flange(G, Fh, ph + dh * 0.112, dh, 0.12, 0.17, 0.022, 0, side=0, hub=False)
    flange(G, Fh, ph + dh * 0.114, -dh, 0.12, 0.17, 0.024, 8, side=-1, blind=True, hub=False)
    G.add(C['steel'], tube([ph + dh * 0.138 + Vector((0, 0, -0.05)), ph + dh * 0.19 + Vector((0, 0, -0.04)), ph + dh * 0.19 + Vector((0, 0, 0.04)),
                            ph + dh * 0.138 + Vector((0, 0, 0.05))], 0.008, 8, bend=0.02))
    # Stutzen oben mit Flanschverbindung und Bogen in die Wand
    top = c + Vector((0, 0, 2.115))
    G.add(shell, cyl(0.06, 0.12, 14, cap=False), along(top - Vector((0, 0, 0.02)), top + Vector((0, 0, 0.12))))
    Fs = {'fl': shell, 'bolt': C['steel_dark']}
    pipe = mat('kessel_rohr', (0.36, 0.40, 0.38), 'paint', r=1.2)
    Fp = {'fl': pipe, 'bolt': C['steel_dark']}
    flange(G, Fs, top + Vector((0, 0, 0.14)), (0, 0, 1), 0.06, 0.11, 0.02, 0, side=0)
    flange(G, Fp, top + Vector((0, 0, 0.142)), (0, 0, -1), 0.06, 0.11, 0.022, 8, side=-1)
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


def build_door_steel_white():
    _locked_door((0.82, 0.82, 0.79), 'steel', 'warn', seed=1)


def build_door_steel_grey():
    _locked_door((0.50, 0.52, 0.52), 'steel', 'plate', seed=2)


def build_door_steel_cream():
    _locked_door((0.80, 0.74, 0.60), 'steel', 'spray', seed=3)


def build_door_window():
    _locked_door((0.70, 0.71, 0.69), 'steel', 'window', seed=4)


def build_door_plywood():
    _locked_door((0.66, 0.54, 0.38), 'plywood', None, seed=5)


def build_door_wood_old():
    _locked_door((0.36, 0.26, 0.18), 'wood', 'plate', seed=6)


def build_door_oversize():
    _locked_door((0.46, 0.48, 0.47), 'steel', 'warn', H=2.5, W=1.8, seed=7)


def build_door_level3():
    """Stahltuer nach Level 3 (im Durchbruch, offen stehend nach +x) mit aufgemalter 3 auf dem Sturz."""
    M = _mats()
    frame = common.material('zarge', (0.30, 0.31, 0.30), look='paint', rough=0.5)
    leaf = common.material('blatt', (0.70, 0.71, 0.68), look='paint', rough=0.55)
    paint = common.material('farbe', (0.88, 0.86, 0.80), look='paint', rough=0.8)
    W, H = 1.0, 2.05
    for s in (-1, 1):
        common.box(f'pfosten{s}', (0.44, s * W / 2 - (0.05 if s > 0 else 0), 0.0), (0.5, s * W / 2 + (0.0 if s > 0 else 0.05), H), frame, bevel=0.004)
    common.box('sturz', (0.44, -W / 2, H - 0.05), (0.5, W / 2, H), frame)
    ob = common.box('blatt', (0.0, 0.0, 0.0), (0.04, W - 0.02, H - 0.06), leaf, bevel=0.004)
    ob.matrix_world = Matrix.Translation((0.47, -W / 2 + 0.01, 0.01)) @ Matrix.Rotation(math.radians(80), 4, 'Z') @ Matrix.Translation((0, 0, 0)) @ ob.matrix_world
    # "3" aus fuenf Strichen (Segmentziffer), auf die Wand ueber der Tuer (Flaeche der Laibung bei x = -0.5)
    x = -0.505
    seg = [((-0.1, 2.48), (0.1, 2.52)), ((0.06, 2.35), (0.1, 2.52)), ((-0.02, 2.33), (0.1, 2.37)),
           ((0.06, 2.18), (0.1, 2.35)), ((-0.1, 2.18), (0.1, 2.22))]
    for k, ((y0, z0), (y1, z1)) in enumerate(seg):
        common.box(f'ziffer{k}', (x - 0.002, -y1, z0), (x, -y0, z1), paint)  # im Spiel geprueft: so lesbar


# --- Gegenstaende -----------------------------------------------------------------------------------------------------
def build_trolley_flatbed():
    """Plattformwagen 1,2 x 0,7 m mit Buegel, Rollen; darauf Maschinenteile (Zahnrad, Gehaeuse, Rohrstueck)."""
    M = _mats()
    common.box('platte', (-0.6, -0.35, 0.16), (0.6, 0.35, 0.2), M['green'], bevel=0.008)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 0.5, sy * 0.27
            common.box(f'gabel{sx}{sy}', (x - 0.02, y - 0.02, 0.1), (x + 0.02, y + 0.02, 0.16), M['steel'])
            _cyl(f'rad{sx}{sy}', (x, y - 0.018, 0.065), (x, y + 0.018, 0.065), 0.065, M['dark'], seg=16)
    for sy in (-1, 1):
        _cyl(f'buegel{sy}', (-0.58, sy * 0.3, 0.2), (-0.58, sy * 0.3, 0.95), 0.014, M['steel'], seg=10)
    _cyl('buegel_q', (-0.58, -0.3, 0.95), (-0.58, 0.3, 0.95), 0.014, M['steel'], seg=10)
    _cyl('zahnrad', (0.15, 0.0, 0.2), (0.15, 0.0, 0.26), 0.2, M['rust'], seg=24)
    common.box('gehaeuse', (-0.4, -0.25, 0.2), (-0.05, 0.1, 0.45), M['grey'], bevel=0.01)
    _cyl('rohrstueck', (0.0, 0.2, 0.26), (0.55, 0.25, 0.26), 0.05, M['rust'], seg=14)


def build_toolbox():
    M = _mats()
    common.box('kasten', (-0.25, -0.11, 0.0), (0.25, 0.11, 0.2), M['red'], bevel=0.008)
    common.box('deckel', (-0.25, -0.11, 0.2), (0.25, 0.11, 0.24), M['red'], bevel=0.008)
    _cyl('griff', (-0.12, 0.0, 0.27), (0.12, 0.0, 0.27), 0.012, M['dark'], seg=10)
    for x in (-0.12, 0.12):
        _cyl(f'stuetze{x}', (x, 0.0, 0.24), (x, 0.0, 0.27), 0.01, M['dark'], seg=8)


def build_stepladder():
    """Stehleiter (offen, 1,6 m): zwei Holme vorn mit Stufen, zwei hinten, Spreizgurt."""
    M = _mats()
    alu = common.material('alu', (0.70, 0.71, 0.72), look='metal', rough=0.35, metal=0.9)
    top = 1.6
    for s in (-1, 1):
        _cyl(f'vorn{s}', (0.32, s * 0.22, 0.0), (0.0, s * 0.18, top), 0.018, alu, seg=10)
        _cyl(f'hinten{s}', (-0.32, s * 0.22, 0.0), (0.0, s * 0.18, top), 0.016, alu, seg=10)
    for k in range(5):
        t = (k + 1) / 6
        x = 0.32 * (1 - t)
        common.box(f'stufe{k}', (x - 0.05, -0.2, t * top - 0.015), (x + 0.03, 0.2, t * top + 0.01), alu)
    common.box('kopf', (-0.06, -0.21, top - 0.02), (0.06, 0.21, top + 0.03), M['dark'], bevel=0.008)


def build_cable_reel():
    M = _mats()
    for y in (-0.15, 0.15):
        _cyl(f'scheibe{y}', (0.0, y - 0.01, 0.3), (0.0, y + 0.01, 0.3), 0.3, M['dark'], seg=28)
    _cyl('kern', (0.0, -0.15, 0.3), (0.0, 0.15, 0.3), 0.2, common.material('kabel', (0.75, 0.48, 0.10), look='rubber', rough=0.7), seg=24)
    for y in (-0.15, 0.15):
        _cyl(f'fuss{y}', (-0.2, y, 0.0), (0.0, y, 0.3), 0.012, M['steel'], seg=8)
        _cyl(f'fuss2{y}', (0.2, y, 0.0), (0.0, y, 0.3), 0.012, M['steel'], seg=8)


def build_folding_chair():
    M = _mats()
    seat = common.material('sitz', (0.32, 0.34, 0.36), look='paint', rough=0.6)
    common.box('sitz', (-0.2, -0.2, 0.44), (0.2, 0.2, 0.47), seat, bevel=0.01)
    common.box('lehne', (-0.22, -0.19, 0.62), (-0.2, 0.19, 0.84), seat, bevel=0.008)
    for sy in (-1, 1):
        _cyl(f'vorn{sy}', (0.22, sy * 0.19, 0.0), (-0.21, sy * 0.19, 0.86), 0.011, M['steel'], seg=8)
        _cyl(f'hinten{sy}', (-0.22, sy * 0.19, 0.0), (0.15, sy * 0.19, 0.45), 0.011, M['steel'], seg=8)


# --- Gleis und Wagen ---------------------------------------------------------------------------------------------------
def build_rail_track():
    """Schmalspurgleis (Spur 0,6 m), 5 m entlang x: rostige Schienen, verrottende Schwellen, einige fehlen."""
    rng = random.Random(9)
    M = _mats()
    sleeper = common.material('schwelle', (0.30, 0.24, 0.18), look='wood', rough=0.95)
    for k in range(9):
        if rng.random() < 0.15:
            continue
        x = -2.4 + k * 0.6
        ob = common.box(f'schwelle{k}', (-0.07, -0.5, 0.0), (0.07, 0.5, 0.06), sleeper, bevel=0.005)
        ob.location = (x, rng.uniform(-0.03, 0.03), 0)
        ob.rotation_euler = (0, 0, rng.uniform(-0.08, 0.08))
    for y in (-0.3, 0.3):
        common.box(f'fuss{y}', (-2.5, y - 0.035, 0.06), (2.5, y + 0.035, 0.07), M['rust'])
        common.box(f'steg{y}', (-2.5, y - 0.008, 0.07), (2.5, y + 0.008, 0.13), M['rust'])
        common.box(f'kopf{y}', (-2.5, y - 0.02, 0.13), (2.5, y + 0.02, 0.15), M['rust'])


def build_hopper_wagon():
    """Entgleister Trichterwagen (1,8 x 0,9 m), schraeg liegend auf einer Seite des Gleises."""
    M = _mats()
    parts = []

    def add(ob):
        parts.append(ob)
        return ob
    # Trichter: Pyramidenstumpf aus vier Flaechen
    bm = bmesh.new()
    top = [(-0.9, -0.45, 1.0), (0.9, -0.45, 1.0), (0.9, 0.45, 1.0), (-0.9, 0.45, 1.0)]
    bot = [(-0.4, -0.2, 0.35), (0.4, -0.2, 0.35), (0.4, 0.2, 0.35), (-0.4, 0.2, 0.35)]
    vt = [bm.verts.new(p) for p in top]
    vb = [bm.verts.new(p) for p in bot]
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((vt[i], vt[j], vb[j], vb[i]))
    bm.faces.new(vb[::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    sol = add(common.obj_from_bmesh('trichter', bm, M['rust']))
    mod = sol.modifiers.new('dicke', 'SOLIDIFY')
    mod.thickness = 0.02
    add(common.box('rahmen', (-0.85, -0.38, 0.22), (0.85, 0.38, 0.32), M['steel']))
    for x in (-0.55, 0.55):
        for y in (-0.3, 0.3):
            add(_cyl(f'rad{x}{y}', (x, y - 0.03, 0.15), (x, y + 0.03, 0.15), 0.15, M['dark'], seg=18))
    add(_cyl('kupplung', (0.85, 0.0, 0.27), (1.05, 0.0, 0.27), 0.03, M['steel'], seg=10))
    M_ = Matrix.Translation((0.0, 0.35, 0.05)) @ Matrix.Rotation(math.radians(-22), 4, 'X') @ Matrix.Rotation(0.15, 4, 'Z')
    for ob in parts:
        ob.matrix_world = M_ @ ob.matrix_world


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


def build_chamfer_26():
    _chamfer(2.6)


def build_chamfer_32():
    _chamfer(3.2)
