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
    return lathe([(0, 0), (s * 0.86, 0), (s, h * 0.18), (s, h * 0.82), (s * 0.86, h), (0, h)], 6, a0=TAU / 12)


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
        'zinc': mat('verzinkt', (0.62, 0.63, 0.62), 'galvanized', r=1.15, m=0.55),
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
def bolt(G, matl, p, axis, s, head_h=None, stub=0.0, washer=True, seg=6):
    """Sechskantschraube/Mutter an p, Achse axis (zeigt aus der Flaeche heraus). s = Umkreisradius."""
    p, axis = Vector(p), Vector(axis).normalized()
    hh = head_h or s * 1.25
    M = along(p, p + axis)
    if washer:
        G.add(matl, lathe([(0, 0), (s * 1.35, 0), (s * 1.35, s * 0.18), (0, s * 0.18)], 10), M)
        M = along(p + axis * s * 0.18, p + axis * (s * 0.18 + 1))
    G.add(matl, nut_shape(s, hh), M)
    if stub > 0:
        G.add(matl, lathe([(0, 0), (s * 0.55, 0), (s * 0.55, stub + hh - s * 0.1), (s * 0.42, stub + hh), (0, stub + hh)], seg), M)


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


def flange(G, mats, p, axis, rp, rf, t=0.022, nb=8, side=1, hub=True, blind=False, seg=24, bolt_s=None, stub=0.012,
           rng=None, rust=0.0):
    """Vorschweissflansch an p, Achse axis (Rohr laeuft in Richtung -axis weg, Dichtflaeche bei p).
    side: +1 Schraubenkoepfe auf der Rohrseite, -1 Muttern mit Gewindeende auf der Rohrseite, 0 keine Schrauben.
    mats: dict mit 'fl' (Flansch), 'bolt', optional 'gasket', 'rust'."""
    p, ax = Vector(p), Vector(axis).normalized()
    M = along(p, p - ax)  # lokales z zeigt zur Rohrseite
    prof = [(rp * 0.96 if not blind else 0.0, 0.0), (rf - 0.003, 0.0), (rf, 0.003), (rf, t - 0.003), (rf - 0.003, t)]
    if hub:
        prof += [(rp + 0.012, t), (rp + 0.004, t + 0.03), (rp * 0.999, t + 0.034)]
    else:
        prof += [(rp * 0.999, t)]
    if blind:
        prof = prof
    else:
        prof += [(rp * 0.96, t + (0.034 if hub else 0.0))]
    prof = prof[::-1] if False else prof
    # Querschnitt im Uhrzeigersinn umdrehen: Profil laeuft hier von innen unten (z = 0) nach aussen, dann zur Rohrseite
    G.add(mats['fl'], lathe(list(reversed(prof)), seg), M)
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
            bolt(G, mats['bolt'], back, -ax, bs, stub=stub, washer=True)


def handwheel(G, M3, c, axis, R, rng, spokes=5, paint=None, steel=None, rust=None, grease=None, rim_r=None, seg=28,
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
    seg2 = 8
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
            G.add(paint, lathe([(0, 0), (ww0, 0), (ww1, L), (0, L)], 8, sx=1.0, sy=0.65), Ms)
    # Rost an den Speichenansaetzen, Fett am Nabenrand
    for k in range(min(spokes, 3)):
        a = TAU * rng.randrange(spokes) / spokes
        G.add(rust, lathe([(0, 0), (rr * 1.5, 0), (rr * 1.4, rr * 0.8), (0, rr)], 6),
              M @ Matrix.Translation((math.cos(a) * (R - rr * 1.6), math.sin(a) * (R - rr * 1.6), -rr * 0.3)))
    G.add(grease, torus(hub_r * 1.02, hub_r * 0.18, 12, 5), M @ Matrix.Translation((0, 0, hub_h * 0.35)))


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
def build_mach_cabinet():
    """Schaltschrank 1,0 x 0,55 x 2,1 m: zwei Tueren mit Sicke, Anzeige, Schalter, Kabelrohr bis zur Decke (2,6 m)."""
    M = _mats()
    D, W, H = 0.55, 1.0, 2.1
    common.box('sockel', (0.02, -W / 2, 0.0), (D - 0.02, W / 2, 0.1), M['dark'], bevel=0.01)
    common.box('korpus', (0.0, -W / 2, 0.1), (D, W / 2, H), M['grey'], bevel=0.012, segments=2)
    for s in (-1, 1):
        y0, y1 = (0.005, W / 2 - 0.02) if s > 0 else (-W / 2 + 0.02, -0.005)
        common.box(f'tuer{s}', (D, y0, 0.15), (D + 0.015, y1, H - 0.05), M['grey'], bevel=0.006)
        common.box(f'sicke{s}', (D + 0.015, y0 + 0.06, 0.4), (D + 0.02, y1 - 0.06, 0.42), M['grey'])
        common.box(f'griff{s}', (D + 0.015, s * 0.03 - 0.008, 1.0), (D + 0.05, s * 0.03 + 0.008, 1.18), M['dark'], bevel=0.004)
    common.box('anzeige', (D + 0.016, 0.12, 1.45), (D + 0.02, 0.36, 1.62), M['glass'])
    for k in range(4):
        _cyl(f'schalter{k}', (D + 0.015, -0.35 + k * 0.08, 1.55), (D + 0.05, -0.35 + k * 0.08, 1.55), 0.015,
             M['red'] if k == 0 else M['dark'], seg=12)
    _cyl('kabelrohr', (0.15, 0.3, H), (0.15, 0.3, 2.6), 0.04, M['pipe'], seg=14)
    _cyl('kabelrohr2', (0.3, -0.25, H), (0.3, -0.25, 2.6), 0.025, M['pipe'], seg=12)


def build_mach_pump():
    """Liegende Pumpe mit Motor auf Grundrahmen (1,5 x 0,8 m), Saug- und Druckrohr in die Wand."""
    M = _mats()
    common.box('rahmen', (0.05, -0.75, 0.0), (0.8, 0.75, 0.14), M['steel'], bevel=0.01)
    _cyl('motor', (0.45, -0.7, 0.42), (0.45, 0.0, 0.42), 0.26, M['green'], seg=28)
    for k in range(8):
        y = -0.65 + k * 0.08
        _cyl(f'rippe{k}', (0.45, y, 0.42), (0.45, y + 0.015, 0.42), 0.285, M['green'], seg=28)
    _cyl('kupplung', (0.45, 0.0, 0.42), (0.45, 0.18, 0.42), 0.11, M['steel'])
    _cyl('pumpe', (0.45, 0.18, 0.42), (0.45, 0.5, 0.42), 0.3, M['green'], seg=28)
    _cyl('druck', (0.45, 0.35, 0.72), (0.45, 0.35, 1.6), 0.09, M['pipe_red'])
    _cyl('druck_wand', (0.45, 0.35, 1.6), (0.0, 0.35, 1.6), 0.09, M['pipe_red'])
    _flange('fl1', (0.45, 0.35, 0.8), (0, 0, 1), 0.09, M['steel'])
    _cyl('saug', (0.45, 0.62, 0.42), (0.0, 0.62, 0.42), 0.11, M['pipe'])
    _flange('fl2', (0.3, 0.62, 0.42), (1, 0, 0), 0.11, M['steel'])
    common.box('klemme', (0.66, -0.45, 0.55), (0.76, -0.25, 0.7), M['grey'], bevel=0.005)


def build_mach_tank():
    """Stehender Kessel (D 0,9 m, 2,2 m) auf Fuessen vor der Wand, Ventil, Manometer, Rohr in die Wand."""
    M = _mats()
    r = 0.45
    c = Vector((0.55, 0.0, 0.0))
    for a in (0.5, 2.6, 4.7):
        p = c + Vector((math.cos(a) * 0.3, math.sin(a) * 0.3, 0))
        _cyl(f'fuss{a}', p, p + Vector((0, 0, 0.32)), 0.035, M['steel'], seg=10)
    _cyl('mantel', c + Vector((0, 0, 0.3)), c + Vector((0, 0, 2.0)), r, M['rust'], seg=36)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=36, ring_count=12, radius=r, location=c + Vector((0, 0, 2.0)))
    ob = bpy.context.active_object
    ob.scale = (1, 1, 0.35)
    ob.data.materials.append(M['rust'])
    ob.data.shade_smooth()
    bpy.ops.mesh.primitive_uv_sphere_add(segments=36, ring_count=12, radius=r, location=c + Vector((0, 0, 0.3)))
    ob = bpy.context.active_object
    ob.scale = (1, 1, 0.3)
    ob.data.materials.append(M['rust'])
    ob.data.shade_smooth()
    for z in (0.7, 1.35, 1.95):
        _cyl(f'ring{z}', c + Vector((0, 0, z - 0.02)), c + Vector((0, 0, z + 0.02)), r + 0.012, M['steel'], seg=36)
    _cyl('stutzen', c + Vector((0, 0, 2.1)), c + Vector((0, 0, 2.45)), 0.07, M['pipe'])
    _cyl('stutzen_wand', c + Vector((0, 0, 2.45)), Vector((0.0, 0.0, 2.45)), 0.07, M['pipe'])
    _cyl('ventil_rohr', c + Vector((0.0, -r, 1.1)), c + Vector((0.0, -r - 0.18, 1.1)), 0.05, M['pipe'])
    _cyl('handrad', c + Vector((0.0, -r - 0.18, 1.1)), c + Vector((0.0, -r - 0.2, 1.1)), 0.12, M['red'], seg=20)
    _cyl('mano', c + Vector((r * 0.7, -r * 0.7, 1.5)), c + Vector((r * 0.75 + 0.05, -r * 0.75 - 0.05, 1.5)), 0.06, M['steel'], seg=18)


def _bundle(length, specs, seed):
    """Waagrechte Rohre an der Wand (Laenge entlang y, mittig um y = 0): specs = (hoehe, abstand, radius, mat)."""
    M = _mats()
    for k, (z, off, r, mk) in enumerate(specs):
        _cyl(f'rohr{k}', (off, -length / 2, z), (off, length / 2, z), r, M[mk], seg=18)
        for j in range(int(length / 2.5) + 1):
            y = -length / 2 + 0.2 + j * 2.5
            if y > length / 2:
                break
            common.box(f'schelle{k}{j}', (0.0, y - 0.025, z - r - 0.03), (off + r + 0.01, y + 0.025, z - r), M['steel'])
            _flange(f'muffe{k}{j}', (off, y + 1.2, z), (0, 1, 0), r, M['steel'])


def build_pipe_bundle_a():
    _bundle(5.0, [(2.25, 0.16, 0.08, 'pipe'), (2.0, 0.12, 0.05, 'pipe_red'), (0.35, 0.14, 0.07, 'pipe'), (1.75, 0.1, 0.035, 'pipe_yel')], 1)


def build_pipe_bundle_b():
    _bundle(5.0, [(2.3, 0.2, 0.12, 'rust'), (1.95, 0.1, 0.04, 'pipe'), (1.85, 0.1, 0.04, 'pipe')], 2)


def build_pipe_riser():
    """Senkrechte Rohre Boden-Decke (2,6 m) mit Flanschen, Absperrventil mit Handrad."""
    M = _mats()
    for y, r, mk in ((-0.15, 0.07, 'pipe'), (0.12, 0.05, 'pipe_red')):
        _cyl(f'steig{y}', (0.12, y, 0.0), (0.12, y, 2.6), r, M[mk], seg=18)
        for z in (0.4, 1.6, 2.4):
            _flange(f'fl{y}{z}', (0.12, y, z), (0, 0, 1), r, M['steel'])
    common.box('ventil', (0.06, -0.21, 1.05), (0.18, -0.09, 1.2), M['steel'], bevel=0.01)
    _cyl('spindel', (0.18, -0.15, 1.12), (0.32, -0.15, 1.12), 0.012, M['steel'], seg=10)
    _cyl('rad', (0.32, -0.15, 1.12), (0.335, -0.15, 1.12), 0.13, M['red'], seg=22)


def build_junction_box():
    """Verteilerkasten an der Wand (Wandflaeche x = 0, Mitte im Ursprung), Kabelrohre nach oben."""
    M = _mats()
    common.box('kasten', (0.0, -0.3, -0.4), (0.22, 0.3, 0.4), M['beige'], bevel=0.012, segments=2)
    common.box('deckel', (0.22, -0.28, -0.38), (0.235, 0.28, 0.38), M['beige'], bevel=0.005)
    common.box('schild', (0.235, -0.08, 0.2), (0.237, 0.08, 0.3), M['pipe_yel'])
    for y in (-0.18, 0.0, 0.18):
        _cyl(f'kabel{y}', (0.1, y, 0.4), (0.1, y, 1.3), 0.022, M['pipe'], seg=10)


# --- Licht --------------------------------------------------------------------------------------------------------
def _tube(rgb):
    M = _mats()
    lamp = common.material('roehre', rgb, look='glow', glow=1.6)
    common.box('schiene', (-0.62, -0.035, -0.07), (0.62, 0.035, -0.02), M['grey'], bevel=0.006)
    _cyl('roehre', (-0.6, 0.0, -0.09), (0.6, 0.0, -0.09), 0.013, lamp, seg=12)
    for x in (-0.61, 0.61):
        common.box(f'fassung{x}', (x - 0.02, -0.025, -0.11), (x + 0.02, 0.025, -0.06), M['dark'])
    for x in (-0.45, 0.45):
        _cyl(f'seil{x}', (x, 0.0, -0.02), (x, 0.0, 0.0), 0.003, M['dark'], seg=6)
    # offenes Kabel: haengt in einem Bogen zur Decke
    pts = [(0.62, 0.0, -0.05), (0.75, 0.02, -0.16), (0.9, 0.03, -0.12), (1.0, 0.03, 0.0)]
    for i in range(len(pts) - 1):
        _cyl(f'kabel{i}', pts[i], pts[i + 1], 0.005, M['dark'], seg=6)


def build_tube_white():
    _tube((1.0, 0.98, 0.94))


def build_tube_orange():
    _tube((1.0, 0.72, 0.42))


def build_tube_dead():
    _tube((0.35, 0.36, 0.35))


def build_violet_lamp():
    """Notlicht violett (Wandmontage, Wandflaeche x = 0): kleines Gehaeuse mit leuchtender Scheibe."""
    M = _mats()
    glow = common.material('violett', (0.62, 0.32, 1.0), look='glow', glow=2.5)
    common.box('gehaeuse', (0.0, -0.09, -0.05), (0.06, 0.09, 0.05), M['dark'], bevel=0.006)
    common.box('scheibe', (0.06, -0.075, -0.035), (0.065, 0.075, 0.035), glow)


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
