"""V13.3 Level 36 (LEVELSPEC_36.md, LOOK_36.md): Timeless Airport - neu gebaut mit echter Geometrie.

Gepaeckband (Plattenband mit Gummilamellen, Fuehrungen, Gummischuerze), Fundsachen-Regal, Check-in-Schalter mit
Gepaeckwaage, Abflugtafel, BTA-Stand, Laufband (Paletten, Kammplatten, Glasbalustrade, Handlauf), haengende Wegweiser
(auch widerspruechlich und leer/kaputt), Ausgangsschild, Ladenfronten (Profile, Glas, Rollgitter), Sitzreihe
(Lochblech, Polster, Armlehnen, Traverse, Fussgestell), Gate-Schalter, Gate-Schilder, Gate-Tueren, Uhren, Trinkbrunnen,
Toilettenschild, Habseligkeiten an Gate C32.

Konventionen wie common.py: Meter, z oben, Blick entlang +x, Wandmodelle mit Rueckseite bei x = 0, Deckenmodelle mit
Oberseite bei z = 0. Groesse, Ursprung und Befestigung entsprechen der V12-Fassung (die Karte platziert danach).
Die Teile eines Materials werden in einem Netz gesammelt (Kit); Schrift ist Geometrie aus Inter (SIL OFL 1.1, aus den
Blender-Datendateien, LICENSES.md) und wird von entarteten Dreiecken bereinigt.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

# ======================================================================================================================
# Werkzeuge
# ======================================================================================================================

FONT = None


def _font():
    global FONT
    try:
        FONT.name  # nach common.reset() ist der Datenblock ungueltig
    except (AttributeError, ReferenceError):
        FONT = None
    if FONT is None:
        base = os.path.dirname(bpy.app.binary_path)
        for p in (os.path.join(base, '4.5', 'datafiles', 'fonts', 'Inter.woff2'),):
            try:
                FONT = bpy.data.fonts.load(p)
                break
            except Exception:  # noqa: BLE001
                FONT = None
        if FONT is None:
            FONT = bpy.data.fonts.get('Bfont Regular') or bpy.data.fonts.load('<builtin>')
    return FONT


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(a, axis):
    return Matrix.Rotation(a, 4, axis)


def S(sx, sy=None, sz=None):
    sy = sx if sy is None else sy
    sz = sx if sz is None else sz
    m = Matrix.Identity(4)
    m[0][0], m[1][1], m[2][2] = sx, sy, sz
    return m


# Text in der yz-Ebene, blickt entlang +x, liest entlang +y (so steht er auf einer Wand, die nach +x blickt)
TEXT_ON_WALL = R(math.pi / 2, 'Z') @ R(math.pi / 2, 'X')


def text(name, body, size, mat, tf, depth=0.0, res=3, align='CENTER', spacing=1.0):
    """Schrift als Netz (blickt nach tf @ +x). Bereinigt: doppelte Punkte, entartete und flaechenlose Dreiecke."""
    cu = bpy.data.curves.new(name, 'FONT')
    cu.resolution_u = res
    cu.body = body
    cu.font = _font()
    cu.size = size
    cu.space_character = spacing
    cu.extrude = depth * 0.5
    cu.align_x = align
    cu.align_y = 'CENTER'
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(mat)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    ob.select_set(False)
    me = ob.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-6, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method='BEAUTY', ngon_method='BEAUTY')
    bad = [f for f in bm.faces if f.calc_area() < 1e-9]
    if bad:
        bmesh.ops.delete(bm, geom=bad, context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context='VERTS')
    if depth > 0:
        bm.transform(T(0, 0, depth * 0.5))  # Rueckseite der Buchstaben auf der Flaeche
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = False
    ob.matrix_world = tf @ TEXT_ON_WALL
    return ob


class Kit:
    """Sammelt Teile je Material in einem bmesh; finish() legt je Material ein Objekt an (auto-glatt nach Winkel)."""

    def __init__(self, name):
        self.name = name
        self.bms = {}
        self.mats = {}

    def bm(self, mat):
        k = mat.name
        if k not in self.bms:
            self.bms[k] = bmesh.new()
            self.mats[k] = mat
        return self.bms[k]

    def finish(self, deg=34.0):
        obs = []
        for i, (k, bm) in enumerate(self.bms.items()):
            # Vielecke selbst dreiteilen und flaechenlose Dreiecke entfernen (keine entarteten Dreiecke im Export)
            ng = [f for f in bm.faces if len(f.verts) > 3]
            if ng:
                bmesh.ops.triangulate(bm, faces=ng, quad_method='BEAUTY', ngon_method='EAR_CLIP')
            bad = []
            for f in bm.faces:  # Flaechenlose und Splitter (Hoehe unter 0,02 mm) entfernen
                ar = f.calc_area()
                lmax = max((e.calc_length() for e in f.edges), default=0.0)
                if ar < 1e-8 or (lmax > 0 and 2 * ar / lmax < 2e-5):
                    bad.append(f)
            if bad:
                bmesh.ops.delete(bm, geom=bad, context='FACES')
            if not bm.faces:
                bm.free()
                continue
            ob = common.obj_from_bmesh(f'{self.name}_{i}', bm, self.mats[k])
            common.shade_auto(ob, deg)
            obs.append(ob)
        self.bms = {}
        return obs


def _apply(bm, vs, fs, tf, bev, seg, recalc, bev_angle=30.0, prof=0.5):
    if tf is not None:
        bmesh.ops.transform(bm, matrix=tf, verts=vs)
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=fs)
    if bev > 0:
        lim = math.radians(bev_angle)
        edges = {e for f in fs for e in f.edges if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > lim}
        if edges:
            res = bmesh.ops.bevel(bm, geom=list(edges), offset=bev, offset_type='OFFSET', segments=seg, profile=prof,
                                  affect='EDGES', clamp_overlap=True)
            fs = [f for f in fs if f.is_valid] + list(res.get('faces', []))
    return fs


def poly(K, mat, verts, faces, tf=None, bev=0.0, seg=1, recalc=False):
    bm = K.bm(mat)
    vs = [bm.verts.new(v) for v in verts]
    fs = []
    for f in faces:
        try:
            fs.append(bm.faces.new([vs[i] for i in f]))
        except ValueError:
            pass
    return _apply(bm, vs, fs, tf, bev, seg, recalc)


CUBE_F = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]


def box(K, mat, lo, hi, tf=None, bev=0.0, seg=1):
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    return poly(K, mat, v, CUBE_F, tf, bev, seg)


def prism(K, mat, pts, z0, z1, tf=None, bev=0.0, seg=1, cap0=True, cap1=True):
    """Prisma aus einem Umriss (x, y) gegen den Uhrzeigersinn, von z0 bis z1."""
    if _area(pts) < 0:
        pts = pts[::-1]
    n = len(pts)
    v = [(x, y, z0) for (x, y) in pts] + [(x, y, z1) for (x, y) in pts]
    f = [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    if cap0:
        f.append(tuple(range(n))[::-1])
    if cap1:
        f.append(tuple(range(n, 2 * n)))
    return poly(K, mat, v, f, tf, bev, seg)


def loft(K, mat, rings, tf=None, cap0=True, cap1=True, closed=True, bev=0.0, seg=1, recalc=False):
    """Ringe gleicher Punktzahl (je gegen den Uhrzeigersinn von oben, aufsteigend) zu einer Huelle verbinden."""
    n = len(rings[0])
    v, f = [], []
    for r in rings:
        v += [tuple(p) for p in r]
    m = n if closed else n - 1
    for k in range(len(rings) - 1):
        for i in range(m):
            j = (i + 1) % n
            f.append((k * n + i, k * n + j, (k + 1) * n + j, (k + 1) * n + i))
    if cap0 and closed:
        f.append(tuple(range(n))[::-1])
    if cap1 and closed:
        L = (len(rings) - 1) * n
        f.append(tuple(range(L, L + n)))
    return poly(K, mat, v, f, tf, bev, seg, recalc)


def lathe(K, mat, prof, seg=16, tf=None, a0=0.0, a1=2 * math.pi):
    """Drehkoerper um z: prof = [(r, z), ...] gegen den Uhrzeigersinn in der (r, z)-Ebene (Normalen nach aussen).
    r = 0 wird ein Pol."""
    full = abs(a1 - a0 - 2 * math.pi) < 1e-6
    ns = seg if full else seg + 1
    bm = K.bm(mat)
    rings = []
    allv = []
    for (r, z) in prof:
        if r < 1e-7:
            v = bm.verts.new((0.0, 0.0, z))
            rings.append([v])
            allv.append(v)
        else:
            ring = []
            for i in range(ns):
                a = a0 + (a1 - a0) * i / seg
                ring.append(bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
            rings.append(ring)
            allv += ring
    fs = []
    m = seg
    for k in range(len(rings) - 1):
        A, B = rings[k], rings[k + 1]
        for i in range(m):
            j = (i + 1) % ns
            if not full and i == ns - 1:
                continue
            if len(A) == 1 and len(B) == 1:
                continue
            if len(A) == 1:
                q = (A[0], B[j], B[i])
            elif len(B) == 1:
                q = (A[i], A[j], B[0])
            else:
                q = (A[i], A[j], B[j], B[i])
            try:
                fs.append(bm.faces.new(q))
            except ValueError:
                pass
    return _apply(bm, allv, fs, tf, 0.0, 1, False)


def cyl(K, mat, a, b, r, seg=12, bev=0.0, bseg=1, r2=None):
    """Zylinder (oder Kegelstumpf mit r2) von Punkt a nach Punkt b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    r2 = r if r2 is None else r2
    q = d.normalized().to_track_quat('Z', 'Y')
    tf = T(*a) @ q.to_matrix().to_4x4()
    prof = [(0.0, 0.0), (r, 0.0), (r2, L), (0.0, L)]
    if bev > 0:
        e = min(bev, r * 0.45, L * 0.3)
        prof = [(0.0, 0.0), (r - e, 0.0), (r - e * 0.3, e * 0.3), (r, e), (r2, L - e), (r2 - e * 0.3, L - e * 0.3),
                (r2 - e, L), (0.0, L)]
    return lathe(K, mat, prof, seg, tf)


def _area(pts):
    s = 0.0
    for i in range(len(pts)):
        x0, y0 = pts[i]
        x1, y1 = pts[(i + 1) % len(pts)]
        s += x0 * y1 - x1 * y0
    return s * 0.5


def rrect(w, h, r, n=3, cx=0.0, cy=0.0):
    """Rechteck mit runden Ecken (gegen den Uhrzeigersinn), Punktzahl 4 * (n + 1)."""
    r = min(r, w * 0.5 - 1e-4, h * 0.5 - 1e-4)
    out = []
    for (ccx, ccy, a0) in ((w / 2 - r, -h / 2 + r, -math.pi / 2), (w / 2 - r, h / 2 - r, 0.0),
                           (-w / 2 + r, h / 2 - r, math.pi / 2), (-w / 2 + r, -h / 2 + r, math.pi)):
        for k in range(n + 1):
            a = a0 + (math.pi / 2) * k / n
            out.append((cx + ccx + r * math.cos(a), cy + ccy + r * math.sin(a)))
    return out


def circle(r, n, cx=0.0, cy=0.0, a0=0.0):
    return [(cx + r * math.cos(a0 + 2 * math.pi * k / n), cy + r * math.sin(a0 + 2 * math.pi * k / n)) for k in range(n)]


def fillet(pts, r, n=3):
    """Polylinie (Vektoren) mit runden Ecken."""
    pts = [Vector(p) for p in pts]
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        a, b, c = pts[i - 1], pts[i], pts[i + 1]
        d0, d1 = (a - b), (c - b)
        if d0.length < 1e-6 or d1.length < 1e-6:
            continue
        ang = d0.angle(d1)
        if ang > math.pi - 1e-3:
            out.append(b)
            continue
        t = min(r / math.tan(ang / 2), d0.length * 0.49, d1.length * 0.49)
        p0 = b + d0.normalized() * t
        p1 = b + d1.normalized() * t
        for k in range(n + 1):
            s = k / n
            out.append((p0 * (1 - s) + b * s) * (1 - s) + (b * (1 - s) + p1 * s) * s)
    out.append(pts[-1])
    return out


def sweep(K, mat, path, prof, closed=False, up=None, caps=True, tf=None, scale=None):
    """Profil [(u, v)] entlang einer Polylinie ziehen. u laeuft entlang N, v entlang B = T x N; N folgt 'up'
    (fester Hinweisvektor) oder wird mitgefuehrt. Geschlossene Huellen bekommen nach aussen zeigende Normalen."""
    path = [Vector(p) for p in path]
    n = len(path)
    if _area(prof) < 0:
        prof = prof[::-1]
    Ts = []
    for i in range(n):
        if closed:
            a, b = path[(i - 1) % n], path[(i + 1) % n]
        else:
            a, b = path[max(i - 1, 0)], path[min(i + 1, n - 1)]
        Ts.append((b - a).normalized())
    Ns = []
    if up is not None:
        h = Vector(up)
        for t in Ts:
            nn = h - t * h.dot(t)
            if nn.length < 1e-6:
                nn = t.orthogonal()
            Ns.append(nn.normalized())
    else:
        nn = Ts[0].orthogonal().normalized()
        for t in Ts:
            nn = nn - t * nn.dot(t)
            if nn.length < 1e-6:
                nn = t.orthogonal()
            nn = nn.normalized()
            Ns.append(nn)
    bm = K.bm(mat)
    rings = []
    allv = []
    for i in range(n):
        t, nn = Ts[i], Ns[i]
        bb = t.cross(nn)
        sc = 1.0 if scale is None else scale[i]
        ring = [bm.verts.new(path[i] + nn * (u * sc) + bb * (v * sc)) for (u, v) in prof]
        rings.append(ring)
        allv += ring
    m = len(prof)
    fs = []
    segs = n if closed else n - 1
    for k in range(segs):
        A, B = rings[k], rings[(k + 1) % n]
        for i in range(m):
            j = (i + 1) % m
            try:
                fs.append(bm.faces.new((A[i], A[j], B[j], B[i])))
            except ValueError:
                pass
    if caps and not closed:
        try:
            fs.append(bm.faces.new(rings[0][::-1]))
            fs.append(bm.faces.new(rings[-1]))
        except ValueError:
            pass
    return _apply(bm, allv, fs, tf, 0.0, 1, False)


def tube(K, mat, path, r, seg=8, closed=False, up=None, caps=True):
    return sweep(K, mat, path, circle(r, seg), closed=closed, up=up, caps=caps)


def plate(K, mat, c, u, v, w, h, n_off=0.0007):
    """Flaches Rechteck (Etikett, Fuge) um c in der Ebene (u, v); Normale = u x v; um n_off vor der Flaeche."""
    c, u, v = Vector(c), Vector(u).normalized(), Vector(v).normalized()
    nrm = u.cross(v)
    c = c + nrm * n_off
    p = [c - u * w / 2 - v * h / 2, c + u * w / 2 - v * h / 2, c + u * w / 2 + v * h / 2, c - u * w / 2 + v * h / 2]
    return poly(K, mat, [tuple(x) for x in p], [(0, 1, 2, 3)])


def blob(K, mat, c, u, v, w, h, rng, k=9, n_off=0.0008, jit=0.3):
    """Unregelmaessiger Fleck (Schmutz, Abrieb, Wasserrand) in der Ebene (u, v) um c."""
    c, u, v = Vector(c), Vector(u).normalized(), Vector(v).normalized()
    nrm = u.cross(v)
    c = c + nrm * n_off
    pts = []
    for i in range(k):
        a = 2 * math.pi * i / k + rng.uniform(-0.2, 0.2)
        f = 1.0 - jit * rng.random()
        pts.append(c + u * (math.cos(a) * w / 2 * f) + v * (math.sin(a) * h / 2 * f))
    return poly(K, mat, [tuple(x) for x in pts], [tuple(range(k))])


def screw(K, mat, c, nrm, r=0.004, seg=6, h=None):
    """Linsenkopfschraube auf einer Flaeche (Normale nrm)."""
    h = r * 0.45 if h is None else h
    q = Vector(nrm).normalized().to_track_quat('Z', 'Y')
    tf = T(*c) @ q.to_matrix().to_4x4()
    return lathe(K, mat, [(0.0, 0.0), (r, 0.0), (r * 0.8, h * 0.7), (0.0, h)], seg, tf)


def grid_holes(K, mat, origin, u, v, nu, nv, du, dv, size, n_off=0.0006, stagger=True):
    """Lochraster (Lochblech): kleine dunkle Achtecke in der Ebene (u, v) ab origin."""
    origin, u, v = Vector(origin), Vector(u).normalized(), Vector(v).normalized()
    nrm = u.cross(v)
    bm = K.bm(mat)
    vs, fs = [], []
    for i in range(nu):
        for j in range(nv):
            off = (du * 0.5 if (stagger and j % 2) else 0.0)
            c = origin + u * (i * du + off) + v * (j * dv) + nrm * n_off
            ring = [bm.verts.new(c + u * (math.cos(a) * size) + v * (math.sin(a) * size))
                    for a in [2 * math.pi * k / 6 for k in range(6)]]
            vs += ring
            fs.append(bm.faces.new(ring))
    return fs


def jitter(K, mat, amp, rng, fs=None):
    bm = K.bm(mat)
    verts = {v for f in fs if f.is_valid for v in f.verts} if fs is not None else set(bm.verts)
    for v in verts:
        v.co += Vector((rng.uniform(-amp, amp), rng.uniform(-amp, amp), rng.uniform(-amp, amp)))


# ======================================================================================================================
# Materialien (Rauheit r und Metall m je Material bewusst gesetzt)
# ======================================================================================================================

def _m():
    mat = common.material
    return {
        # Metall
        'steel': mat('edelstahl_gebuerstet', (0.70, 0.71, 0.72), look='metal', tex='metal_aluminum', rough=0.35, metal=0.9, r=0.62, m=0.85),
        'steel_dull': mat('edelstahl_matt', (0.58, 0.59, 0.60), look='metal', tex='metal_aluminum', rough=0.55, metal=0.8, r=1.0, m=0.7),
        'chrome': mat('chrom', (0.80, 0.81, 0.82), look='metal', rough=0.15, metal=1.0, r=0.35, m=0.95),
        'alu': mat('alu_eloxiert', (0.64, 0.65, 0.66), look='metal', tex='metal_aluminum', rough=0.45, metal=0.8, r=0.85, m=0.65),
        'alu_dark': mat('alu_anthrazit', (0.20, 0.21, 0.22), look='metal', tex='metal_aluminum', rough=0.5, metal=0.6, r=0.95, m=0.45),
        'galv': mat('verzinkt', (0.56, 0.58, 0.58), look='galvanized', rough=0.55, metal=0.7, r=1.1, m=0.6),
        'cast': mat('alu_guss', (0.55, 0.56, 0.57), look='metal', rough=0.5, metal=0.8, r=1.05, m=0.7),
        'brass': mat('messing_matt', (0.62, 0.50, 0.28), look='metal', rough=0.4, metal=0.9, r=0.8, m=0.8),
        # Lack, Kunststoff
        'anthr': mat('anthrazit_pulver', (0.13, 0.14, 0.15), look='paint', tex='painted_metal_grey', rough=0.6, r=1.05),
        'anthr_gloss': mat('anthrazit_glatt', (0.09, 0.10, 0.11), look='paint', rough=0.35, r=0.6),
        'white': mat('weiss_schichtstoff', (0.86, 0.86, 0.84), look='paint', tex='painted_metal_white', rough=0.4, r=0.8),
        'white_dull': mat('weiss_lack_matt', (0.80, 0.80, 0.78), look='paint', rough=0.7, r=1.15),
        'grey': mat('grau_lack', (0.45, 0.46, 0.47), look='paint', tex='painted_metal_grey', rough=0.6, r=1.0),
        'black_pl': mat('kunststoff_schwarz', (0.035, 0.036, 0.04), look='plastic', rough=0.6, r=1.1),
        'grey_pl': mat('kunststoff_grau', (0.30, 0.31, 0.32), look='plastic', rough=0.6, r=1.05),
        'white_pl': mat('kunststoff_weiss', (0.82, 0.82, 0.80), look='plastic', rough=0.5, r=0.9),
        'yellow_pl': mat('kunststoff_gelb', (0.92, 0.70, 0.08), look='plastic', rough=0.5, r=0.9),
        'red_pl': mat('kunststoff_rot', (0.72, 0.06, 0.05), look='plastic', rough=0.4, r=0.75),
        'rubber': mat('gummi', (0.045, 0.045, 0.05), look='rubber', rough=0.9, r=1.25, m=0.0),
        'rubber_worn': mat('gummi_abgerieben', (0.10, 0.10, 0.105), look='rubber', rough=0.8, r=1.05, m=0.0),
        'rubber_mark': mat('gummiabrieb', (0.05, 0.05, 0.05), look='paint', rough=0.8, r=1.2),
        'glass': mat('glas_klar', (0.50, 0.58, 0.60), look='glass', rough=0.05, r=0.25),
        'glass_frost': mat('glas_satiniert', (0.72, 0.76, 0.77), look='plastic', rough=0.6, r=0.95),
        'glass_edge': mat('glas_kante', (0.38, 0.55, 0.50), look='glass', rough=0.1, r=0.4),
        'screen_off': mat('bildschirm_aus', (0.03, 0.035, 0.04), look='glass', rough=0.1, r=0.3),
        # Leuchtflaechen
        'screen': mat('bildschirm', (0.06, 0.12, 0.28), look='glow', glow=0.9),
        'screen_dim': mat('bildschirm_dunkel', (0.03, 0.06, 0.16), look='glow', glow=0.35),
        'amber': mat('bernstein', (1.0, 0.72, 0.18), look='glow', glow=1.4),
        'white_glow': mat('schrift_hell', (0.95, 0.97, 1.0), look='glow', glow=1.3),
        'yellow': mat('wegweiser_gelb', (0.98, 0.80, 0.12), look='glow', glow=0.9),
        'purple': mat('bta_violett', (0.42, 0.18, 0.62), look='glow', glow=0.9),
        'green_glow': mat('gruen_leucht', (0.20, 0.85, 0.35), look='glow', glow=1.3),
        'red_glow': mat('rot_leucht', (1.0, 0.16, 0.10), look='glow', glow=1.3),
        # Stoff, Holz, Papier
        'seat': mat('sitz_stoff_blau', (0.13, 0.22, 0.42), look='fabric', rough=0.9, r=1.35),
        'seat_worn': mat('sitz_stoff_blau_abgewetzt', (0.20, 0.27, 0.42), look='fabric', rough=0.9, r=1.4),
        'wood': mat('holz_hell', (0.62, 0.48, 0.32), look='wood', tex='wood_birch', rough=0.5, r=0.85),
        'fabric_r': mat('stoff_rot', (0.50, 0.12, 0.10), look='fabric', rough=0.9, r=1.35),
        'fabric_g': mat('stoff_gruen', (0.18, 0.32, 0.22), look='fabric', rough=0.9, r=1.35),
        'fabric_b': mat('stoff_blau', (0.14, 0.20, 0.40), look='fabric', rough=0.9, r=1.35),
        'fabric_k': mat('stoff_schwarz', (0.05, 0.05, 0.055), look='fabric', rough=0.9, r=1.3),
        'fabric_gr': mat('stoff_grau', (0.33, 0.34, 0.35), look='fabric', rough=0.9, r=1.4),
        'fabric_o': mat('stoff_ocker', (0.55, 0.40, 0.18), look='fabric', rough=0.9, r=1.35),
        'shell_r': mat('hartschale_rot', (0.55, 0.08, 0.07), look='plastic', rough=0.35, r=0.6),
        'shell_s': mat('hartschale_silber', (0.62, 0.63, 0.64), look='plastic', rough=0.3, r=0.55, m=0.35),
        'shell_b': mat('hartschale_petrol', (0.06, 0.25, 0.30), look='plastic', rough=0.35, r=0.6),
        'shell_k': mat('hartschale_schwarz', (0.04, 0.04, 0.045), look='plastic', rough=0.35, r=0.65),
        'card': mat('pappe', (0.62, 0.48, 0.30), look='cardboard', rough=0.9),
        'card_dark': mat('pappe_feucht', (0.45, 0.33, 0.19), look='cardboard', rough=0.9),
        'paper': mat('papier', (0.88, 0.87, 0.82), look='paint', rough=0.9, r=1.4),
        'tape': mat('klebeband', (0.70, 0.58, 0.36), look='plastic', rough=0.4, r=0.7),
        'black': mat('schwarz_matt', (0.03, 0.03, 0.03), look='paint', rough=0.8, r=1.2),
        'dirt': mat('schmutz', (0.20, 0.19, 0.17), look='paint', rough=0.9, r=1.4),
        'scuff': mat('abrieb_hell', (0.40, 0.40, 0.40), look='paint', rough=0.9, r=1.3),
        'gap': mat('fuge_dunkel', (0.02, 0.02, 0.022), look='paint', rough=0.9, r=1.4),
    }


# ======================================================================================================================
# Gepaeck (Hartschale, Stoffkoffer, Reisetasche, Rucksack, Karton) - im lokalen Rahmen: Boden z = 0, Front +x
# ======================================================================================================================

def _tag(K, M, at, rng, down=Vector((0, 0, -1))):
    """Gepaeckanhaenger: Schlaufe, Kunststoffhuelle mit Papierkarte, haengt an 'at'."""
    at = Vector(at)
    a = rng.uniform(-0.6, 0.6)
    d = Vector((math.cos(a) * 0.35, math.sin(a) * 0.35, 0.0)) + down
    d.normalize()
    p1 = at + d * 0.05
    tube(K, M['black_pl'], [at, at + d * 0.025 + Vector((0, 0.006, 0)), p1], 0.0012, seg=4)
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-3:
        side = Vector((0, 1, 0))
    side.normalize()
    nrm = side.cross(d).normalized()
    c = p1 + d * 0.05
    q = Matrix((side, d, nrm)).transposed().to_4x4()
    tf = T(*c) @ q
    box(K, M[rng.choice(['yellow_pl', 'red_pl', 'white_pl'])], (-0.026, -0.05, -0.0015), (0.026, 0.05, 0.0015), tf=tf)
    plate(K, M['paper'], c + nrm * 0.0016, side, d, 0.04, 0.07, n_off=0.0)


def suitcase_hard(K, M, tf, W=0.42, D=0.26, H=0.68, shell='shell_r', rng=None, tag=True, scuffs=3):
    """Hartschalenkoffer mit vier Doppelrollen (stehend): Schale mit Rillen, Reissverschluss rundum, Griff, Teleskopgriff."""
    rng = rng or random.Random(1)
    k0 = len(K.bms)  # noqa: F841
    sub = Kit('tmp')
    hw = 0.07
    sh = M[shell]
    box(sub, sh, (-D / 2, -W / 2, hw), (D / 2, W / 2, H), bev=min(0.045, D * 0.17), seg=3)
    for sx in (-1, 1):  # Rillen vorn und hinten
        for k in range(4):
            y = -W * 0.3 + k * W * 0.2
            box(sub, sh, (sx * D / 2 - 0.004, y - 0.011, hw + 0.07), (sx * D / 2 + 0.005, y + 0.011, H - 0.07), bev=0.003)
    # Reissverschluss rundum (yz-Ebene bei x = 0)
    hh, ww = (H - hw), W
    path = [Vector((0, p[0], hw + hh / 2 + p[1])) for p in rrect(ww + 0.006, hh + 0.006, 0.045, 3)]
    sweep(sub, M['rubber'], path, [(-0.009, -0.004), (0.009, -0.004), (0.009, 0.004), (-0.009, 0.004)], closed=True, up=(1, 0, 0))
    box(sub, M['chrome'], (-0.004, W * 0.18, H - 0.01), (0.004, W * 0.18 + 0.03, H + 0.012), bev=0.002)
    # Tragegriff oben
    gp = fillet([(0, -0.085, H), (0, -0.075, H + 0.032), (0, 0.075, H + 0.032), (0, 0.085, H)], 0.014, 3)
    sweep(sub, M['black_pl'], gp, rrect(0.026, 0.013, 0.005, 1), up=(1, 0, 0))
    for s in (-1, 1):
        box(sub, M['black_pl'], (-0.02, s * 0.095 - 0.018, H - 0.004), (0.02, s * 0.095 + 0.018, H + 0.012), bev=0.004)
    # Teleskopgriff hinten (eingefahren)
    for s in (-1, 1):
        box(sub, M['alu'], (-D / 2 - 0.012, s * 0.11 - 0.012, hw + 0.06), (-D / 2 + 0.002, s * 0.11 + 0.012, H - 0.02), bev=0.003)
    box(sub, M['black_pl'], (-D / 2 - 0.022, -0.14, H - 0.01), (-D / 2 + 0.035, 0.14, H + 0.03), bev=0.009, seg=2)
    # Doppelrollen
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * (D / 2 - 0.045), sy * (W / 2 - 0.05)
            box(sub, M['black_pl'], (cx - 0.025, cy - 0.025, hw - 0.022), (cx + 0.025, cy + 0.025, hw + 0.004), bev=0.006)
            box(sub, M['black_pl'], (cx - 0.008, cy - 0.022, 0.03), (cx + 0.008, cy + 0.022, hw - 0.02))
            for dy in (-0.013, 0.013):
                cyl(sub, M['rubber'], (cx, cy + dy - 0.008, 0.03), (cx, cy + dy + 0.008, 0.03), 0.029, seg=8)
    # Abrieb an Kanten und Flaechen
    for i in range(scuffs):
        sx = rng.choice((-1, 1))
        blob(sub, M[rng.choice(['scuff', 'dirt'])], (sx * (D / 2 + 0.0005), rng.uniform(-W / 3, W / 3), rng.uniform(hw + 0.1, H - 0.1)),
             (0, sx, 0), (0, 0, 1), rng.uniform(0.03, 0.08), rng.uniform(0.02, 0.05), rng)
    _merge(K, sub, tf)
    if tag:
        sub2 = Kit('tmp2')
        _tag(sub2, M, (0.0, 0.07, H + 0.032), rng)
        _merge(K, sub2, tf)


def suitcase_soft(K, M, tf, W=0.44, D=0.27, H=0.66, cloth='fabric_k', rng=None, tag=True):
    """Stoffkoffer: Korpus mit Paspel, Vortasche mit Reissverschluss, Griffe, zwei Rollen hinten, Fuesse vorn."""
    rng = rng or random.Random(2)
    sub = Kit('tmp')
    hw = 0.045
    c = M[cloth]
    box(sub, c, (-D / 2, -W / 2, hw), (D / 2, W / 2, H), bev=0.05, seg=3)
    hh = H - hw
    for sx in (-1, 1):  # Paspel vorn/hinten
        path = [Vector((sx * (D / 2 - 0.012), p[0], hw + hh / 2 + p[1])) for p in rrect(W - 0.004, hh - 0.004, 0.05, 3)]
        sweep(sub, M['fabric_k' if cloth != 'fabric_k' else 'fabric_gr'], path, circle(0.0055, 5), closed=True, up=(1, 0, 0))
    # Vortasche
    pw, ph = W - 0.1, hh * 0.62
    box(sub, c, (D / 2 - 0.01, -pw / 2, hw + 0.06), (D / 2 + 0.035, pw / 2, hw + 0.06 + ph), bev=0.018, seg=2)
    zp = [Vector((D / 2 + 0.035, p[0], hw + 0.06 + ph / 2 + p[1])) for p in rrect(pw - 0.05, ph - 0.05, 0.03, 2)]
    zp = zp[len(zp) // 4: len(zp) * 3 // 4 + 1]  # oberer Bogen (Reissverschluss)
    sweep(sub, M['rubber'], zp, [(-0.002, -0.006), (0.003, -0.006), (0.003, 0.006), (-0.002, 0.006)], up=(1, 0, 0))
    for p in (zp[2], zp[-3]):
        box(sub, M['chrome'], (p.x - 0.002, p.y - 0.006, p.z - 0.032), (p.x + 0.006, p.y + 0.006, p.z), bev=0.002)
    # Griffe (Gurtband)
    gp = fillet([(0, -0.09, H - 0.01), (0, -0.075, H + 0.03), (0, 0.075, H + 0.03), (0, 0.09, H - 0.01)], 0.015, 3)
    sweep(sub, M['fabric_k'], gp, rrect(0.032, 0.009, 0.003, 1), up=(1, 0, 0))
    sp = fillet([(-0.06, W / 2 - 0.01, H * 0.55), (-0.045, W / 2 + 0.03, H * 0.55), (0.045, W / 2 + 0.03, H * 0.55), (0.06, W / 2 - 0.01, H * 0.55)], 0.014, 3)
    sweep(sub, M['fabric_k'], sp, rrect(0.03, 0.009, 0.003, 1), up=(0, 0, 1))
    # Teleskopgriff, Rollen hinten, Fuesse vorn
    for s in (-1, 1):
        box(sub, M['alu_dark'], (-D / 2 - 0.012, s * 0.1 - 0.012, hw + 0.08), (-D / 2 + 0.004, s * 0.1 + 0.012, H - 0.02), bev=0.003)
    box(sub, M['black_pl'], (-D / 2 - 0.02, -0.13, H - 0.012), (-D / 2 + 0.03, 0.13, H + 0.025), bev=0.008, seg=2)
    for s in (-1, 1):
        box(sub, M['black_pl'], (-D / 2 - 0.01, s * (W / 2 - 0.04) - 0.03, 0.02), (-D / 2 + 0.07, s * (W / 2 - 0.04) + 0.03, hw + 0.05), bev=0.008)
        cyl(sub, M['rubber'], (-D / 2 + 0.01, s * (W / 2 - 0.04) - 0.012, 0.037), (-D / 2 + 0.01, s * (W / 2 - 0.04) + 0.012, 0.037), 0.037, seg=10)
        box(sub, M['black_pl'], (D / 2 - 0.07, s * (W / 2 - 0.06) - 0.02, 0.0), (D / 2 - 0.02, s * (W / 2 - 0.06) + 0.02, hw + 0.01), bev=0.006)
    for i in range(2):
        blob(sub, M['dirt'], (D / 2 + 0.036, rng.uniform(-pw / 3, pw / 3), rng.uniform(hw + 0.1, hw + ph)), (0, 1, 0), (0, 0, 1),
             rng.uniform(0.04, 0.09), rng.uniform(0.03, 0.06), rng)
    _merge(K, sub, tf)
    if tag:
        sub2 = Kit('tmp2')
        _tag(sub2, M, (0.0, 0.06, H + 0.03), rng)
        _merge(K, sub2, tf)


def duffel(K, M, tf, L=0.62, Dm=0.30, cloth='fabric_g', rng=None, strap=True):
    """Reisetasche (liegend, Achse y): Koerper mit flachem Boden, Endkappen mit Paspel, Reissverschluss, Gurte, Griffe."""
    rng = rng or random.Random(3)
    sub = Kit('tmp')
    r = Dm / 2
    prof = [(0.0, -L / 2), (r * 0.55, -L / 2), (r * 0.86, -L / 2 + 0.012), (r, -L / 2 + 0.05), (r * 1.02, -L / 4), (r * 1.03, 0.0),
            (r * 1.02, L / 4), (r, L / 2 - 0.05), (r * 0.86, L / 2 - 0.012), (r * 0.55, L / 2), (0.0, L / 2)]
    fs = lathe(sub, M[cloth], prof, 18, T(0, 0, r * 0.86) @ R(-math.pi / 2, 'X') @ S(1.0, 0.86, 1.0))
    bm = sub.bm(M[cloth])
    for f in fs:  # flacher Boden und leichte Unregelmaessigkeit
        for v in f.verts:
            if v.co.z < 0.0:
                v.co.z = 0.0
    for v in {v for f in fs for v in f.verts}:
        v.co += Vector((rng.uniform(-0.004, 0.004), rng.uniform(-0.003, 0.003), rng.uniform(-0.004, 0.004)))
    del bm
    for s in (-1, 1):  # Paspel an den Enden
        ring = [Vector((math.cos(a) * r * 0.9, s * (L / 2 - 0.02), r * 0.86 + math.sin(a) * r * 0.9 * 0.86)) for a in
                [2 * math.pi * k / 16 for k in range(16)]]
        ring = [Vector((p.x, p.y, max(p.z, 0.008))) for p in ring]
        sweep(sub, M['fabric_k'], ring, circle(0.006, 4), closed=True, up=(0, s, 0))
    top = r * 0.86 * 2 - 0.003
    box(sub, M['rubber'], (-0.008, -L / 2 + 0.07, top - 0.004), (0.008, L / 2 - 0.07, top + 0.005))
    box(sub, M['chrome'], (0.004, L / 2 - 0.12, top - 0.004), (0.012, L / 2 - 0.09, top + 0.004))
    box(sub, M['chrome'], (0.008, L / 2 - 0.12, top - 0.04), (0.012, L / 2 - 0.105, top - 0.004))
    for s in (-1, 1):  # Gurte um den Koerper und Griffe
        y = s * L * 0.2
        ring = []
        for k in range(15):
            a = -math.pi * 0.05 + math.pi * 1.1 * k / 14
            ring.append(Vector((math.cos(a) * (r + 0.006), y, r * 0.86 + math.sin(a) * (r + 0.006) * 0.86)))
        ring = [Vector((-p.x, p.y, p.z)) for p in ring[::-1]] + ring
        ring = [Vector((p.x, p.y, max(p.z, 0.003))) for p in ring]
        sweep(sub, M['fabric_k'], ring, [(-0.002, -0.019), (0.002, -0.019), (0.002, 0.019), (-0.002, 0.019)], up=(0, 1, 0))
    hp = fillet([(-0.05, -L * 0.2, top - 0.02), (-0.03, -L * 0.13, top + 0.09), (-0.03, L * 0.13, top + 0.09), (-0.05, L * 0.2, top - 0.02)], 0.05, 4)
    sweep(sub, M['fabric_k'], hp, rrect(0.034, 0.008, 0.003, 1), up=(1, 0, 0))
    hp2 = [Vector((-p.x * 0.2 + 0.05, p.y, p.z - 0.01)) for p in hp]
    sweep(sub, M['fabric_k'], hp2, rrect(0.034, 0.008, 0.003, 1), up=(-1, 0, 0))
    box(sub, M['fabric_k'], (-0.03, -0.07, top + 0.06), (0.06, 0.07, top + 0.1), bev=0.012, seg=2)
    if strap:  # Schultergurt lose daneben
        p0 = Vector((r * 0.7, -L / 2 + 0.02, r * 0.86))
        pts = [p0, p0 + Vector((0.12, 0.05, -0.18)), Vector((r + 0.22, -0.1, 0.006)), Vector((r + 0.25, 0.2, 0.006)),
               Vector((r + 0.1, L / 2 + 0.02, 0.05)), Vector((r * 0.7, L / 2 - 0.02, r * 0.86))]
        pts = fillet(pts, 0.08, 4)
        sweep(sub, M['fabric_k'], pts, [(-0.0018, -0.019), (0.0018, -0.019), (0.0018, 0.019), (-0.0018, 0.019)], up=(0, 0, 1))
    blob(sub, M['dirt'], (0.0, 0.0, 0.004), (1, 0, 0), (0, 1, 0), Dm * 1.1, L * 0.9, rng, n_off=0.0)
    _merge(K, sub, tf)


def backpack(K, M, tf, W=0.30, D=0.19, H=0.46, cloth='fabric_b', rng=None):
    """Rucksack (stehend, Ruecken bei -x): Koerper, Fronttasche mit Reissverschluss, Traeger, Tragschlaufe."""
    rng = rng or random.Random(4)
    sub = Kit('tmp')
    c = M[cloth]
    fs = box(sub, c, (-D / 2, -W / 2, 0.0), (D / 2, W / 2, H), bev=0.07, seg=3)
    bm = sub.bm(c)
    for v in {v for f in bm.faces for v in f.verts}:
        if v.co.z > H * 0.6:  # oben schmaler
            k = (v.co.z - H * 0.6) / (H * 0.4)
            v.co.y *= 1.0 - 0.12 * k
            v.co.x *= 1.0 - 0.15 * k
        v.co += Vector((rng.uniform(-0.003, 0.003), rng.uniform(-0.003, 0.003), rng.uniform(-0.002, 0.002)))
    box(sub, c, (D / 2 - 0.02, -W * 0.38, 0.05), (D / 2 + 0.05, W * 0.38, H * 0.48), bev=0.03, seg=2)
    zp = fillet([(D / 2 + 0.05, -W * 0.33, 0.07), (D / 2 + 0.052, -W * 0.33, H * 0.44), (D / 2 + 0.052, W * 0.33, H * 0.44),
                 (D / 2 + 0.05, W * 0.33, 0.07)], 0.04, 3)
    sweep(sub, M['rubber'], zp, [(-0.002, -0.005), (0.003, -0.005), (0.003, 0.005), (-0.002, 0.005)], up=(1, 0, 0))
    zp2 = fillet([(D * 0.3, -W / 2 + 0.01, 0.12), (D * 0.42, -W * 0.4, H - 0.01), (D * 0.42, W * 0.4, H - 0.01), (D * 0.3, W / 2 - 0.01, 0.12)], 0.08, 4)
    sweep(sub, M['rubber'], zp2, [(-0.002, -0.005), (0.003, -0.005), (0.003, 0.005), (-0.002, 0.005)], up=(1, 0.0, 0.6))
    for p in (zp[3], zp2[5]):
        box(sub, M['black_pl'], (p.x, p.y - 0.006, p.z - 0.04), (p.x + 0.008, p.y + 0.006, p.z), bev=0.002)
    for s in (-1, 1):  # Traeger
        pts = fillet([(-D / 2 + 0.01, s * 0.06, H - 0.04), (-D / 2 - 0.03, s * 0.08, H - 0.12), (-D / 2 - 0.035, s * 0.1, H * 0.45),
                      (-D / 2 - 0.02, s * 0.11, 0.1), (-D / 2 + 0.01, s * 0.12, 0.04)], 0.06, 3)
        sweep(sub, M['fabric_k'], pts, rrect(0.016, 0.06, 0.006, 2), up=(1, 0, 0))
    gp = fillet([(-D / 2 + 0.03, -0.035, H - 0.02), (-D / 2 + 0.02, -0.025, H + 0.04), (-D / 2 + 0.02, 0.025, H + 0.04), (-D / 2 + 0.03, 0.035, H - 0.02)], 0.02, 3)
    sweep(sub, M['fabric_k'], gp, rrect(0.026, 0.006, 0.002, 1), up=(1, 0, 0))
    _merge(K, sub, tf)


def carton(K, M, tf, W=0.4, D=0.3, H=0.3, rng=None, wet=False):
    """Karton, zugeklebt und verschnuert, mit Etikett."""
    rng = rng or random.Random(5)
    sub = Kit('tmp')
    cm = M['card_dark' if wet else 'card']
    fs = box(sub, cm, (-D / 2, -W / 2, 0.0), (D / 2, W / 2, H), bev=0.004)
    jitter(sub, cm, 0.003, rng, fs)
    box(sub, M['gap'], (-D / 2 + 0.01, -0.001, H - 0.0005), (D / 2 - 0.01, 0.001, H + 0.0008))  # Laschenfuge
    box(sub, M['tape'], (-D / 2 - 0.0015, -0.025, H - 0.06), (D / 2 + 0.0015, 0.025, H + 0.0015))
    for s in (-1, 1):  # Schnur
        p = [Vector((s * 0.0, -W / 2 - 0.003, 0.0))]
        tube(sub, M['white_dull'], [(-D / 2 - 0.004, s * W * 0.22, 0.003), (-D / 2 - 0.004, s * W * 0.22, H + 0.004),
                                     (D / 2 + 0.004, s * W * 0.22, H + 0.004), (D / 2 + 0.004, s * W * 0.22, 0.003)], 0.0025, seg=4)
        del p
    plate(sub, M['paper'], (D / 2 + 0.003, -W * 0.2, H * 0.45), (0, 1, 0), (0, 0, 1), 0.12, 0.08)
    plate(sub, M['black'], (D / 2 + 0.0035, -W * 0.2, H * 0.47), (0, 1, 0), (0, 0, 1), 0.08, 0.008)
    _merge(K, sub, tf)


def _merge(K, sub, tf=None):
    """Teil-Kit in K uebernehmen (mit Transformation)."""
    for k, bm in sub.bms.items():
        if tf is not None:
            bm.transform(tf)
        me = bpy.data.meshes.new('tmp')
        bm.to_mesh(me)
        bm.free()
        K.bm(sub.mats[k]).from_mesh(me)
        bpy.data.meshes.remove(me)
    sub.bms = {}


def _lug_any(K, M, tf, rng, kind=None, size=1.0, lying=False):
    """Zufaelliges Gepaeckstueck; liefert (Breite entlang y, Tiefe entlang x, Hoehe)."""
    kind = kind or rng.choice(['hard', 'soft', 'duffel', 'backpack', 'hard', 'soft'])
    if kind == 'hard':
        W, D, H = 0.40 * size, 0.25 * size, 0.62 * size
        sh = rng.choice(['shell_r', 'shell_s', 'shell_b', 'shell_k'])
        if lying:
            suitcase_hard(K, M, tf @ T(0, 0, D / 2 + 0.014) @ R(-math.pi / 2, 'Y') @ T(0, 0, -H / 2), W, D, H, sh, rng)
            return W, H, D
        suitcase_hard(K, M, tf, W, D, H, sh, rng)
        return W, D, H
    if kind == 'soft':
        W, D, H = 0.42 * size, 0.26 * size, 0.6 * size
        cl = rng.choice(['fabric_k', 'fabric_b', 'fabric_gr', 'fabric_r'])
        if lying:
            suitcase_soft(K, M, tf @ T(0, 0, D / 2 + 0.014) @ R(-math.pi / 2, 'Y') @ T(0, 0, -H / 2), W, D, H, cl, rng)
            return W, H, D
        suitcase_soft(K, M, tf, W, D, H, cl, rng)
        return W, D, H
    if kind == 'duffel':
        L, Dm = 0.6 * size, 0.3 * size
        duffel(K, M, tf, L, Dm, rng.choice(['fabric_g', 'fabric_o', 'fabric_k', 'fabric_r']), rng, strap=rng.random() < 0.5)
        return L, Dm, Dm * 0.86
    if kind == 'backpack':
        W, D, H = 0.3 * size, 0.19 * size, 0.46 * size
        if lying:
            backpack(K, M, tf @ T(0, 0, D / 2 + 0.03) @ R(-math.pi / 2, 'Y') @ T(0, 0, -H / 2), W, D, H, rng.choice(['fabric_b', 'fabric_r', 'fabric_gr']), rng)
            return W, H, D
        backpack(K, M, tf, W, D, H, rng.choice(['fabric_b', 'fabric_r', 'fabric_gr']), rng)
        return W, D, H
    W, D, H = 0.4 * size, 0.3 * size, 0.28 * size
    carton(K, M, tf, W, D, H, rng, wet=rng.random() < 0.3)
    return W, D, H


# ======================================================================================================================
# Gepaeckausgabe
# ======================================================================================================================

def stadium(R, xc, n_arc=16, n_str=1):
    """Stadionform (gegen den Uhrzeigersinn von oben): Geraden bei y = -R/+R von -xc bis xc, Halbkreise um (+-xc, 0)."""
    pts = []
    for k in range(n_str):
        pts.append((-xc + 2 * xc * k / n_str, -R))
    for k in range(n_arc + 1):
        a = -math.pi / 2 + math.pi * k / n_arc
        pts.append((xc + R * math.cos(a), R * math.sin(a)))
    for k in range(1, n_str):
        pts.append((xc - 2 * xc * k / n_str, R))
    for k in range(n_arc):
        a = math.pi / 2 + math.pi * k / n_arc
        pts.append((-xc + R * math.cos(a), R * math.sin(a)))
    return pts


def _ring_sweep(K, mat, xc, R0, prof_zo, n_arc=16):
    """Querschnitt (z, Versatz nach aussen) rund um die Stadionform mit Radius R0 ziehen."""
    path = [Vector((x, y, 0.0)) for (x, y) in stadium(R0, xc, n_arc, 2)]
    return sweep(K, mat, path, [(z, o) for (z, o) in prof_zo], closed=True, up=(0, 0, 1))


def _stadium_frame(xc, r, s):
    """Punkt und Richtungen auf der Stadionform: s = Bogenlaenge ab (-xc, -r) gegen den Uhrzeigersinn."""
    Ls, La = 2 * xc, math.pi * r
    per = 2 * Ls + 2 * La
    s %= per
    if s < Ls:
        return Vector((-xc + s, -r, 0)), Vector((1, 0, 0)), Vector((0, -1, 0))
    s -= Ls
    if s < La:
        a = -math.pi / 2 + s / r
        n = Vector((math.cos(a), math.sin(a), 0))
        return Vector((xc, 0, 0)) + n * r, Vector((-math.sin(a), math.cos(a), 0)), n
    s -= La
    if s < Ls:
        return Vector((xc - s, r, 0)), Vector((-1, 0, 0)), Vector((0, 1, 0))
    s -= Ls
    a = math.pi / 2 + s / r
    n = Vector((math.cos(a), math.sin(a), 0))
    return Vector((-xc, 0, 0)) + n * r, Vector((-math.sin(a), math.cos(a), 0)), n


def build_carousel_36():
    """Gepaeckband (Boden, 12 x 3,6 m, Stadionform entlang x): Plattenband aus ueberlappenden Gummilamellen auf dunklem
    Unterbau, Edelstahl-Schuerze mit Sockel, Fugen und Schrauben, Gummi-Prallleiste aussen, Mittelinsel mit
    Gummifuehrung, Einlauf mit Gummischuerze (Streifenvorhang), Not-Aus, Anzeige am Mast (Metro 903) und Koffer
    (Wiki: Ankunft ueber Gepaeckbaender)."""
    M = _m()
    K = Kit('band')
    rng = random.Random(36)
    xc, R0 = 4.2, 1.0
    # Aussenschuerze (Edelstahl), Sockel, Prallleiste, Unterbau
    _ring_sweep(K, M['steel'], xc, R0, [(0.08, 0.60), (0.08, 0.775), (0.40, 0.775), (0.43, 0.768), (0.455, 0.74),
                                        (0.47, 0.66), (0.47, 0.60)], n_arc=24)
    _ring_sweep(K, M['black_pl'], xc, R0, [(0.0, 0.62), (0.0, 0.755), (0.085, 0.755), (0.085, 0.62)], n_arc=24)
    _ring_sweep(K, M['rubber'], xc, R0, [(0.405, 0.765), (0.408, 0.786), (0.425, 0.796), (0.445, 0.786), (0.452, 0.765)], n_arc=24)
    _ring_sweep(K, M['gap'], xc, R0, [(0.395, -0.62), (0.395, 0.64), (0.41, 0.64), (0.41, -0.62)], n_arc=20)
    # Mittelinsel (Edelstahl) mit Gummifuehrung am Fuss
    isl = stadium(0.40, xc, 16, 1)
    prism(K, M['steel_dull'], isl, 0.0, 0.66, bev=0.02, seg=2)
    _ring_sweep(K, M['rubber'], xc, R0, [(0.445, -0.61), (0.452, -0.582), (0.47, -0.574), (0.49, -0.582), (0.495, -0.61)], n_arc=16)
    # Lamellen (Gummi, leicht schraeg, ueberlappend); Geraden und Boegen
    r_in, r_out = 0.42, 1.615
    plates = []
    n_str = 52
    pitch = 2 * xc / n_str
    for side in (-1, 1):
        for k in range(n_str):
            x0 = -xc + k * pitch
            plates.append(('s', side, x0, x0 + pitch))
    n_arc = 20
    for side in (-1, 1):
        for k in range(n_arc):
            plates.append(('a', side, k * math.pi / n_arc, (k + 1) * math.pi / n_arc))
    for i, (kind, side, a0, a1) in enumerate(plates):
        mat = M['rubber_worn'] if rng.random() < 0.12 else M['rubber']
        lo_z, hi_z = 0.432, 0.452  # Vorderkante tiefer: Schuppen
        if kind == 's':
            ys = (side * r_in, side * r_out)
            # Fahrtrichtung: unten (+x), oben (-x)
            xa, xb = (a0 - 0.006, a1 + 0.014) if side < 0 else (a1 + 0.006, a0 - 0.014)
            pts = [(xa, ys[0], lo_z), (xb, ys[0], hi_z), (xb, ys[1], hi_z), (xa, ys[1], lo_z)]
        else:
            cx = xc if side > 0 else -xc
            base = -math.pi / 2 if side > 0 else math.pi / 2
            t0, t1 = base + a0 - 0.012, base + a1 + 0.02
            pts = [(cx + r_in * math.cos(t0), r_in * math.sin(t0), lo_z), (cx + r_in * math.cos(t1), r_in * math.sin(t1), hi_z),
                   (cx + r_out * math.cos(t1), r_out * math.sin(t1), hi_z), (cx + r_out * math.cos(t0), r_out * math.sin(t0), lo_z)]
        top = [Vector(p) for p in pts]
        bot = [p - Vector((0, 0, 0.012)) for p in top]
        vv = [tuple(p) for p in bot + top]
        fs = poly(K, mat, vv, CUBE_F, recalc=True)
        _apply(K.bm(mat), [], fs, None, 0.004, 1, False)
        if rng.random() < 0.18:  # Abrieb auf der Lamelle
            c = sum(top, Vector()) / 4 + Vector((0, 0, 0.0008))
            blob(K, M['scuff'], c, (1, 0, 0), (0, 1, 0), 0.08, rng.uniform(0.15, 0.4), rng, n_off=0.0)
    # Fugen und Schrauben in der Schuerze, Abrieb am Sockel
    per = 4 * xc + 2 * math.pi * 1.775
    nseg = 20
    for k in range(nseg):
        p, t, n = _stadium_frame(xc, 1.775, k * per / nseg + 0.3)
        c = p + Vector((0, 0, 0.24)) + n * 0.0005
        plate(K, M['gap'], c, t, (0, 0, 1), 0.003, 0.31, n_off=0.0)
        for dz in (0.11, 0.37):
            for s in (-1, 1):
                screw(K, M['steel'], p + t * (s * 0.03) + Vector((0, 0, dz)), n, r=0.0045)
        if rng.random() < 0.5:
            pp, tt, nn = _stadium_frame(xc, 1.756, k * per / nseg + rng.uniform(0.6, 1.4))
            blob(K, M['rubber_mark'], pp + Vector((0, 0, 0.05)), tt, (0, 0, 1), rng.uniform(0.2, 0.5), 0.05, rng, n_off=0.0008)
    # Not-Aus an beiden Laengsseiten
    for side in (-1, 1):
        y = side * 1.778
        box(K, M['yellow_pl'], (-0.045, y - (0.0 if side > 0 else 0.05), 0.24), (0.045, y + (0.05 if side > 0 else 0.0), 0.34), bev=0.006)
        q = R(-side * math.pi / 2, 'X')
        lathe(K, M['red_pl'], [(0.0, 0.0), (0.016, 0.0), (0.016, 0.012), (0.026, 0.016), (0.027, 0.024), (0.02, 0.03), (0.0, 0.032)], 14,
              T(0.0, y + side * 0.05, 0.29) @ q)
    # Einlauf aus der Insel: Haube, Wangen, Gummischuerze (Streifen) zu beiden Seiten
    xa, xb = -2.95, -1.55
    box(K, M['steel_dull'], (xa, -0.42, 0.66), (xb, 0.42, 1.02), bev=0.015, seg=2)
    for side in (-1, 1):
        y0, y1 = sorted((side * 0.40, side * 0.66))
        box(K, M['steel'], (xa - 0.01, y0, 0.96), (xb + 0.01, y1, 1.0), bev=0.006)
        for x in (xa - 0.01, xb - 0.002):
            box(K, M['steel'], (x, min(side * 0.40, side * 0.66), 0.47), (x + 0.012, max(side * 0.40, side * 0.66), 0.96), bev=0.003)
        box(K, M['gap'], (xa, min(side * 0.401, side * 0.404), 0.48), (xb, max(side * 0.401, side * 0.404), 0.95))
        n = int((xb - xa) / 0.09)
        for k in range(n):
            x0 = xa + 0.01 + k * (xb - xa - 0.02) / n
            ang = rng.uniform(-0.05, 0.08)
            tfm = T(0, side * 0.645, 0.955) @ R(side * ang, 'X')
            box(K, M['rubber'], (x0, -0.003, -0.48 + rng.uniform(0.0, 0.02)), (x0 + (xb - xa - 0.02) / n - 0.004, 0.003, 0.0), tf=tfm)
    # Mast mit Anzeige (beidseitig): Zug-Bildzeichen, METRO 903, GEPAECK
    box(K, M['steel'], (-0.06, -0.04, 0.66), (0.06, 0.04, 1.32), bev=0.01, seg=2)
    box(K, M['anthr'], (-0.62, -0.16, 1.30), (0.62, 0.16, 1.92), bev=0.025, seg=2)
    for side in (-1, 1):
        y = side * 0.161
        box(K, M['anthr_gloss'], (-0.57, min(y, y + side * 0.004), 1.35), (0.57, max(y, y + side * 0.004), 1.87))
        ys = y + side * 0.0045
        tf_face = T(0, ys, 0) @ R(side * math.pi / 2, 'Z')
        _train_icon(K, M['white_glow'], tf_face @ T(0, -0.42, 1.64), 0.3)
        text(f'metro{side}', 'METRO 903', 0.1, M['white_glow'], tf_face @ T(0, 0.12, 1.69))
        text(f'gep{side}', 'GEPAECK', 0.07, M['amber'], tf_face @ T(0, 0.12, 1.5))
    # Koffer auf dem Band
    spots = [(-3.0, -1.0, 0.3), (-0.6, -1.05, -0.4), (1.9, -0.95, 0.2), (2.4, 1.0, 2.8), (-1.0, 1.02, 3.3), (4.9, 0.6, 1.1)]
    kinds = ['hard', 'soft', 'duffel', 'hard', 'backpack', 'soft']
    for (x, y, yaw), kind in zip(spots, kinds):
        _lug_any(K, M, T(x, y, 0.452) @ R(yaw + rng.uniform(-0.2, 0.2), 'Z'), rng, kind=kind, lying=kind != 'duffel')
    K.finish()


def _train_icon(K, mat, tf, s):
    """Bildzeichen Zug (Vorderansicht) in der yz-Ebene, Groesse s, blickt nach +x."""
    out = rrect(0.62 * s, 0.78 * s, 0.14 * s, 3)
    hole_w = [(0.0, -0.18 * s, 0.22 * s, 0.24 * s), (0.0, 0.18 * s, 0.22 * s, 0.24 * s)]
    pts = [Vector((0.0, x, z + 0.06 * s)) for (x, z) in out]
    fs = poly(K, mat, [tuple(tf @ p) for p in pts], [tuple(range(len(pts)))])
    del fs, hole_w
    # Fenster und Lampen als dunkle Inseln davor (leicht versetzt)
    for (y, z, w, h) in ((-0.13 * s, 0.22 * s, 0.2 * s, 0.22 * s), (0.13 * s, 0.22 * s, 0.2 * s, 0.22 * s),
                         (-0.18 * s, -0.16 * s, 0.07 * s, 0.07 * s), (0.18 * s, -0.16 * s, 0.07 * s, 0.07 * s)):
        q = [Vector((0.0015, y + a, z + b)) for (a, b) in rrect(w, h, 0.02 * s, 1)]
        poly(K, _m()['anthr_gloss'], [tuple(tf @ p) for p in q], [tuple(range(len(q)))])
    for sgn in (-1, 1):  # Schienen
        q = [Vector((0.0, sgn * 0.16 * s + a, -0.42 * s + b)) for (a, b) in ((-0.03 * s, -0.1 * s), (0.03 * s, -0.1 * s), (0.03 * s, 0.06 * s), (-0.03 * s, 0.06 * s))]
        poly(K, mat, [tuple(tf @ p) for p in q], [(0, 1, 2, 3)])


def build_luggage_pile():
    _luggage_pile(0)


def build_luggage_pile__v1():
    _luggage_pile(1)


def build_luggage_pile__v2():
    _luggage_pile(2)


def _luggage_pile(v=0):
    """Fundsachen-Regal (Boden, 3 x 0,7 x 2,2 m, beidseitig offen): verzinkte Winkelstuetzen mit Lochung, Fachboeden mit
    gekanteten Raendern, Kreuzverbaende an den Enden, Fussplatten, Schild FUNDSACHEN; darauf Koffer, Taschen, Rucksaecke
    und Kartons mit Gepaeckanhaengern (Wiki: Gepaeck, das zehn Runden liegen bleibt, kommt ins Fundbuero).
    Varianten: andere Belegung; v1 mit Luecken, v2 mehr Kartons."""
    M = _m()
    K = Kit('fund')
    rng = random.Random(37 + v * 11)
    X, Y = 1.5, 0.35
    for x in (-X, 0.0, X):
        for y in (-Y, Y):
            sx = 1 if x < 0 else -1
            sy = 1 if y < 0 else -1
            if x == 0.0:
                sx = 1
            # Winkelprofil 40 x 40 x 2 mm
            L = [(0, 0), (0.04, 0), (0.04, 0.002), (0.002, 0.002), (0.002, 0.04), (0, 0.04)]
            pts = [(x + sx * a if x != 0.0 else x - 0.02 + a, y + sy * b) for (a, b) in L]
            prism(K, M['galv'], pts, 0.0, 2.2)
            for k in range(42):  # Lochung (Schlitze)
                z = 0.1 + k * 0.05
                xx = x + sx * 0.02 if x != 0.0 else x
                plate(K, M['gap'], (xx, y + sy * 0.0023, z), (-sy, 0, 0), (0, 0, 1), 0.007, 0.022, n_off=0.0)
                plate(K, M['gap'], (xx, y - sy * 0.0003, z), (sy, 0, 0), (0, 0, 1), 0.007, 0.022, n_off=0.0)
            box(K, M['galv'], (x - 0.05, y - 0.05, 0.0), (x + 0.05, y + 0.05, 0.004), bev=0.002)
            screw(K, M['steel_dull'], (x + sx * 0.03 if x != 0.0 else x + 0.03, y + sy * 0.03, 0.004), (0, 0, 1), r=0.006)
    levels = [0.12, 0.84, 1.56]
    for z in levels + [2.16]:
        prof = [(z - 0.03, -Y + 0.002), (z - 0.03, -Y - 0.001), (z, -Y - 0.001), (z, Y + 0.001), (z - 0.03, Y + 0.001),
                (z - 0.03, Y - 0.002), (z - 0.002, Y - 0.002), (z - 0.002, -Y + 0.002)]
        if z > 2.0:
            for y in (-Y, Y):
                box(K, M['galv'], (-X, y - 0.02, z - 0.04), (X, y + 0.02, z), bev=0.002)
            continue
        sweep(K, M['galv'], [(-X - 0.002, 0, 0), (X + 0.002, 0, 0)], [(u, v) for (u, v) in prof], up=(0, 0, 1))
        for x in (-X, 0.0, X):
            for y in (-Y, Y):
                screw(K, M['steel_dull'], (x + (0.02 if x < 0.5 else -0.02), y + (0.0012 if y > 0 else -0.0012), z - 0.015), (0, 1 if y > 0 else -1, 0), r=0.005)
        # Etikettenleiste mit Karten
        for k in range(3):
            plate(K, M['paper'], (-1.1 + k * 1.1 + rng.uniform(-0.2, 0.2), Y + 0.0015, z - 0.015), (-1, 0, 0), (0, 0, 1), 0.07, 0.022, n_off=0.0)
    for x in (-X, X):  # Kreuzverband an den Enden
        sx = 1 if x < 0 else -1
        for (z0, z1) in ((0.15, 1.5), (0.8, 2.15)):
            for s in (-1, 1):
                p0 = Vector((x + sx * 0.003, s * (Y - 0.03), z0))
                p1 = Vector((x + sx * 0.003, -s * (Y - 0.03), z1))
                sweep(K, M['galv'], [p0, p1], [(-0.0015, -0.015), (0.0015, -0.015), (0.0015, 0.015), (-0.0015, 0.015)], up=(1, 0, 0))
                screw(K, M['steel_dull'], p0 + Vector((sx * 0.002, 0, 0)), (sx, 0, 0), r=0.005)
    # Schild oben
    box(K, M['white_pl'], (-0.4, Y + 0.001, 2.0), (0.4, Y + 0.005, 2.13), bev=0.004)
    sign_tf = T(0, Y + 0.0058, 2.065) @ R(-math.pi / 2, 'Z') @ R(math.pi, 'Z')
    text('fund_txt', 'FUNDSACHEN', 0.075, M['black'], sign_tf)
    # Fundsachen je Fach (Breite entlang x; oben nur Liegendes und Flaches)
    for li, z in enumerate(levels):
        x = -X + 0.06
        cap = 0.66 if li < 2 else 0.5
        while True:
            kinds = ['hard', 'soft', 'duffel', 'backpack', 'box', 'hard', 'soft'] + (['box', 'box', 'box'] if v == 2 else [])
            kind = rng.choice(kinds)
            size = rng.uniform(0.84, 0.98)
            if v == 1 and rng.random() < 0.25:  # Luecke
                x += 0.35
                if x > X - 0.3:
                    break
                continue
            lying = kind in ('hard', 'soft', 'backpack') and (rng.random() < 0.4 or cap < 0.6)
            est = {'hard': 0.40, 'soft': 0.42, 'duffel': 0.30, 'backpack': 0.30, 'box': 0.40}[kind] * size
            if x + est > X - 0.04:
                break
            if kind == 'duffel':
                yaw = rng.uniform(-0.08, 0.08)
            else:
                yaw = math.pi / 2 + rng.uniform(-0.08, 0.08) + (math.pi if rng.random() < 0.3 else 0.0)
            tf = T(x + est / 2, rng.uniform(-0.03, 0.03), z) @ R(yaw, 'Z')
            _lug_any(K, M, tf, rng, kind=kind, size=size, lying=lying)
            x += est + rng.uniform(0.015, 0.06)
    K.finish()


# Flaechenrahmen fuer Schrift/Bildzeichen: Ebene blickt nach +x, -x, +y, -y (liest jeweils von links nach rechts)
FACE = {'+x': Matrix.Identity(4), '-x': R(math.pi, 'Z'), '+y': R(math.pi / 2, 'Z'), '-y': R(-math.pi / 2, 'Z')}


def _keyboard(K, M, tf):
    """Tastatur (liegend, Tasten zur Person bei -x): Gehaeuse und Tastenreihen."""
    box(K, M['black_pl'], (-0.075, -0.22, 0.0), (0.075, 0.22, 0.018), tf=tf, bev=0.004)
    for r in range(5):
        x = -0.055 + r * 0.026
        for c in range(4):
            y = -0.2 + c * 0.1
            box(K, M['grey_pl'], (x - 0.01, y + 0.003, 0.018), (x + 0.01, y + 0.097, 0.024), tf=tf)


def _monitor(K, M, tf, w=0.52, h=0.32, face='screen'):
    """Flachbildschirm auf Fuss (Bild nach +x), Fuss bei z = 0."""
    lathe(K, M['black_pl'], [(0.0, 0.0), (0.11, 0.0), (0.105, 0.012), (0.0, 0.016)], 16, tf=tf @ S(0.8, 1.0, 1.0))
    box(K, M['black_pl'], (-0.035, -0.025, 0.0), (-0.015, 0.025, 0.22), tf=tf, bev=0.006)
    box(K, M['black_pl'], (-0.03, -w / 2, 0.12), (0.0, w / 2, 0.12 + h), tf=tf, bev=0.008, seg=2)
    box(K, M['grey_pl'], (-0.06, -w * 0.3, 0.16), (-0.03, w * 0.3, 0.12 + h * 0.8), tf=tf, bev=0.012, seg=2)
    box(K, M[face], (0.0, -w / 2 + 0.012, 0.132), (0.0012, w / 2 - 0.012, 0.12 + h - 0.012), tf=tf)
    for k in range(6):  # Lueftungsschlitze hinten
        plate(K, M['gap'], tf @ Vector((-0.0601, -0.1 + k * 0.04, 0.12 + h * 0.7)), tf.to_3x3() @ Vector((0, -1, 0)),
              tf.to_3x3() @ Vector((0, 0, 1)), 0.006, 0.08, n_off=0.0)


def build_checkin_counter():
    _checkin(0)


def build_checkin_counter__v1():
    _checkin(1)


def build_checkin_counter__v2():
    _checkin(2)


def _checkin(v=0):
    """Check-in-Schalter (Boden, Gast bei +x): Tresen mit Ablage, Arbeitsplatte, Bildschirm, Drucker, Tastatur;
    Gepaeckwaage mit Gummiband (Querrippen), Rollen, Fuehrung und Gummivorhang zum Sammelband, Gewichtsanzeige;
    Trennwand aus Schichtstoff und satiniertem Glas; Nummernschild am Mast (Wiki: endlose Schalterreihe mit
    Bildschirmen)."""
    M = _m()
    K = Kit('checkin')
    rng = random.Random(3601)
    y0, y1 = -0.25, 0.95
    # Korpus: Front, Seiten, Arbeitsplatte, Ablage
    box(K, M['white'], (0.255, y0 + 0.02, 0.1), (0.29, y1 - 0.02, 1.06), bev=0.004)
    box(K, M['steel'], (0.24, y0 + 0.02, 0.0), (0.27, y1 - 0.02, 0.1), bev=0.003)
    for y in (y0, y1 - 0.025):
        box(K, M['white'], (-0.38, y, 0.0), (0.30, y + 0.025, 1.06), bev=0.004)
    box(K, M['white_pl'], (-0.38, y0 + 0.02, 0.735), (0.255, y1 - 0.02, 0.765), bev=0.006, seg=2)
    box(K, M['white_pl'], (0.10, y0 - 0.01, 1.06), (0.37, y1 + 0.01, 1.095), bev=0.014, seg=3)
    box(K, M['alu'], (0.29, y0, 0.86), (0.296, y1, 0.885), bev=0.002)
    for y in (y0 + 0.1, y1 - 0.1):
        box(K, M['white'], (0.10, y - 0.012, 0.765), (0.255, y + 0.012, 1.06))
    # Unterbau: Rechner, Kabelkanal
    box(K, M['black_pl'], (-0.30, 0.6, 0.02), (0.05, 0.8, 0.45), bev=0.008)
    box(K, M['grey_pl'], (-0.36, y0 + 0.03, 0.62), (0.2, y0 + 0.08, 0.7), bev=0.004)
    # Arbeitsplatz
    _monitor(K, M, T(-0.1, 0.35, 0.765) @ R(math.pi, 'Z'))
    _keyboard(K, M, T(-0.25, 0.35, 0.765))
    box(K, M['grey_pl'], (0.0, 0.66, 0.765), (0.16, 0.88, 0.89), bev=0.012, seg=2)  # Bordkartendrucker
    box(K, M['gap'], (-0.002, 0.7, 0.85), (0.002, 0.84, 0.856))
    box(K, M['white_pl'], (-0.22, -0.18, 0.765), (-0.04, -0.02, 0.86), bev=0.01, seg=2)  # Anhaengerdrucker
    box(K, M['paper'], (-0.06, -0.15, 0.80), (-0.03, -0.05, 0.801))
    box(K, M['paper'], (-0.34, 0.0, 0.765), (-0.13, 0.15, 0.773))
    # Kundenanzeige auf der Ablage (Bild nach +x)
    box(K, M['black_pl'], (0.12, 0.05, 1.095), (0.16, 0.45, 1.33), bev=0.008)
    box(K, M['screen' if v != 1 else 'screen_off'], (0.16, 0.07, 1.115), (0.1612, 0.43, 1.31))
    if v == 0:
        text('kunde1', 'CHECK-IN', 0.045, M['white_glow'], T(0.1615, 0.25, 1.25))
        text('kunde2', 'QX 0903', 0.035, M['amber'], T(0.1615, 0.25, 1.17))
    elif v == 2:
        text('kunde1', '- - - -', 0.06, M['amber'], T(0.1615, 0.25, 1.21))
    else:  # geschlossen: Aufsteller auf der Ablage, Gurt vor der Waage
        tfs = T(0.25, 0.7, 1.095) @ R(-0.25, 'Y')
        box(K, M['white_pl'], (-0.003, -0.11, 0.0), (0.003, 0.11, 0.3), tf=tfs, bev=0.002)
        text('zu', 'GESCHLOSSEN', 0.03, M['black'], tfs @ T(0.0035, 0.0, 0.16))
        text('zu2', 'CLOSED', 0.03, M['black'], tfs @ T(0.0035, 0.0, 0.11))
        box(K, M['black_pl'], (0.18, 0.62, 1.095), (0.3, 0.78, 1.105), bev=0.003)
        for y in (-0.95, -0.27):
            cyl(K, M['chrome'], (1.08, y, 0.0), (1.08, y, 0.95), 0.025, seg=10)
            lathe(K, M['chrome'], [(0.0, 0.0), (0.15, 0.0), (0.14, 0.02), (0.03, 0.03), (0.0, 0.03)], 16, T(1.08, y, 0.0))
        box(K, M['red_pl'], (1.075, -0.93, 0.86), (1.085, -0.29, 0.91))
    # Gepaeckwaage mit Band
    wy0, wy1 = -0.92, -0.30
    for (a, b) in ((wy0, wy0 + 0.03), (wy1 - 0.03, wy1)):
        box(K, M['steel'], (-0.38, a, 0.0), (1.05, b, 0.33), bev=0.008, seg=2)
    box(K, M['steel_dull'], (-0.38, wy0 - 0.02, 0.0), (1.05, wy0 + 0.01, 0.06), bev=0.004)
    box(K, M['rubber'], (-0.36, wy0 + 0.03, 0.285), (1.04, wy1 - 0.03, 0.305))
    for k in range(17):
        x = -0.33 + k * 0.08
        box(K, M['rubber_worn'], (x, wy0 + 0.035, 0.305), (x + 0.012, wy1 - 0.035, 0.31))
    cyl(K, M['steel_dull'], (1.0, wy0 + 0.03, 0.27), (1.0, wy1 - 0.03, 0.27), 0.036, seg=14)
    box(K, M['black_pl'], (1.04, wy0, 0.0), (1.06, wy1, 0.27), bev=0.004)
    # Fuehrung aussen (schraeg), Gummivorhang hinten
    gp = [(0.33, 0.0), (0.33, 0.03), (0.56, -0.05), (0.56, -0.07)]
    sweep(K, M['steel'], [(-0.38, wy0 + 0.015, 0.0), (0.95, wy0 + 0.015, 0.0)],
          [(z, -v) for (z, v) in gp], up=(0, 0, 1))
    box(K, M['steel'], (-0.40, wy0 - 0.06, 0.62), (-0.30, wy1, 0.66), bev=0.006)
    for y in (wy0 - 0.06, wy1 - 0.02):
        box(K, M['steel'], (-0.40, y, 0.0), (-0.36, y + 0.02, 0.62), bev=0.004)
    n = 7
    for k in range(n):
        yy = wy0 - 0.04 + k * (wy1 - wy0 + 0.02) / n
        box(K, M['rubber'], (-0.335, yy, 0.315), (-0.33, yy + (wy1 - wy0 + 0.02) / n - 0.006, 0.62), tf=None)
    # Gewichtsanzeige
    cyl(K, M['alu'], (1.0, wy0 - 0.03, 0.0), (1.0, wy0 - 0.03, 1.02), 0.02, seg=10)
    box(K, M['black_pl'], (0.97, wy0 - 0.13, 1.0), (1.03, wy0 + 0.07, 1.12), bev=0.01, seg=2)
    box(K, M['anthr_gloss'], (1.03, wy0 - 0.11, 1.02), (1.031, wy0 + 0.05, 1.1))
    text('waage', '0.0 kg', 0.04, M['red_glow'], T(1.0315, wy0 - 0.03, 1.06))
    plate(K, M['yellow_pl'], (0.85, wy0 - 0.0006, 0.2), (1, 0, 0), (0, 0, 1), 0.12, 0.05, n_off=0.0)
    text('max', 'MAX 32 kg', 0.018, M['black'], T(0.85, wy0 - 0.0012, 0.2) @ FACE['-y'])
    # Trennwand: Pfosten, Schichtstoffpaneel, satiniertes Glas
    for x in (-0.38, 0.76):
        box(K, M['alu'], (x - 0.02, 1.0, 0.0), (x + 0.02, 1.1, 1.8), bev=0.004)
    box(K, M['alu'], (-0.4, 1.0, 1.78), (0.78, 1.1, 1.82), bev=0.004)
    box(K, M['white'], (-0.36, 1.03, 0.02), (0.74, 1.07, 1.06), bev=0.003)
    box(K, M['alu'], (-0.36, 1.025, 1.06), (0.74, 1.075, 1.08))
    box(K, M['glass_frost'], (-0.36, 1.045, 1.08), (0.74, 1.055, 1.78))
    # Nummernschild
    cyl(K, M['alu'], (-0.33, 0.86, 0.0), (-0.33, 0.86, 1.93), 0.025, seg=12)
    box(K, M['anthr'], (-0.40, -0.15, 1.92), (-0.30, 0.85, 2.3), bev=0.012, seg=2)
    box(K, M['screen_dim' if v != 1 else 'screen_off'], (-0.30, -0.12, 1.95), (-0.299, 0.82, 2.27))
    text('nummer', '36', 0.24, M['white_glow'], T(-0.2985, 0.18, 2.115))
    text('ci', 'CHECK-IN', 0.05, M['white_glow'], T(-0.2985, 0.6, 2.115))
    box(K, M['anthr'], (-0.40, 0.70, 1.88), (-0.26, 0.95, 1.92), bev=0.006)
    # Sammelband hinter den Schaltern (laeuft entlang der Reihe; deckt den Block der Karte)
    bx0, bx1 = -1.5, -0.42
    box(K, M['black_pl'], (bx0 + 0.03, -1.5, 0.0), (bx1 - 0.03, 1.5, 0.08))
    for (a, b, h) in ((bx0, bx0 + 0.03, 1.1), (bx1 - 0.03, bx1, 0.62)):
        box(K, M['steel'], (a, -1.5, 0.0), (b, 1.5, h), bev=0.004)
    box(K, M['steel_dull'], (bx0 + 0.03, -1.5, 0.08), (bx1 - 0.03, 1.5, 0.5))
    box(K, M['rubber'], (bx0 + 0.04, -1.5, 0.5), (bx1 - 0.04, 1.5, 0.53))
    for k in range(6):
        y = -1.25 + k * 0.5
        box(K, M['rubber_worn'], (bx0 + 0.04, y, 0.53), (bx1 - 0.04, y + 0.01, 0.533))
    for y in (-1.499, 1.499):  # Stossfugen der Verkleidung zum Nachbarschalter
        box(K, M['gap'], (bx0 - 0.001, y - 0.002, 0.0), (bx0 + 0.031, y + 0.002, 1.1))
    for k in range(5):
        y = -1.2 + k * 0.6
        for z in (0.2, 0.9):
            screw(K, M['steel_dull'], (bx0, y, z), (-1, 0, 0), r=0.005)
        plate(K, M['gap'], (bx1 + 0.0005, y + 0.3, 0.3), (0, -1, 0), (0, 0, 1), 0.003, 0.5, n_off=0.0)
    if v == 2:  # vergessenes Gepaeckstueck auf dem Sammelband
        suitcase_soft(K, M, T(-0.95, -0.6, 0.533) @ R(1.3, 'Z') @ T(0, 0, 0.27 / 2 + 0.014) @ R(-math.pi / 2, 'Y') @ T(0, 0, -0.3),
                      0.42, 0.27, 0.6, 'fabric_b', random.Random(5))
    # Gebrauchsspuren
    for i in range(5):
        blob(K, M['scuff'], (0.2905, rng.uniform(y0 + 0.1, y1 - 0.1), rng.uniform(0.12, 0.3)), (0, 1, 0), (0, 0, 1),
             rng.uniform(0.08, 0.2), rng.uniform(0.03, 0.06), rng, n_off=0.0005)
    for i in range(3):
        blob(K, M['scuff'], (rng.uniform(0.15, 0.33), rng.uniform(y0, y1), 1.0955), (1, 0, 0), (0, 1, 0), rng.uniform(0.03, 0.08), rng.uniform(0.05, 0.15), rng, n_off=0.0)
    K.finish()


def build_departure_board():
    """Abflugtafel (Wandteil, 4,1 x 2,1 m): Gehaeuse mit Lueftungsschlitzen und Wandschienen, Rahmen, vier Bildschirme
    mit Stossfugen, Kopfband, Spaltenkoepfe, Zeilen mit Flug, Ziel, Gate, Status (Wiki: Einstieg ab Countdown 1000,
    C32 wartet auf 1784XBA8JLP nach Level 186), stehende Uhrzeit, Statusleuchte."""
    M = _m()
    K = Kit('tafel')
    for z in (-0.6, 0.6):
        box(K, M['alu'], (0.0, -1.85, z - 0.04), (0.03, 1.85, z + 0.04), bev=0.003)
    box(K, M['anthr'], (0.03, -2.05, -1.05), (0.118, 2.05, 1.05), bev=0.014, seg=2)
    # Rahmen
    fw = 0.045
    for (lo, hi) in (((0.112, -2.04, 0.995), (0.126, 2.04, 1.04)), ((0.112, -2.04, -1.04), (0.126, 2.04, -0.995)),
                     ((0.112, -2.04, -0.995), (0.126, -2.04 + fw, 0.995)), ((0.112, 2.04 - fw, -0.995), (0.126, 2.04, 0.995))):
        box(K, M['alu_dark'], lo, hi, bev=0.004)
    # Bildschirmflaeche (glaenzend schwarz), Stossfugen
    box(K, M['anthr_gloss'], (0.112, -1.995, -0.995), (0.121, 1.995, 0.995))
    box(K, M['gap'], (0.121, -0.004, -0.995), (0.1225, 0.004, 0.995))
    box(K, M['gap'], (0.121, -1.995, -0.004), (0.1225, 1.995, 0.004))
    # Lueftungsschlitze oben und unten
    for zs in (-1, 1):
        for k in range(24):
            y = -1.8 + k * 0.155
            plate(K, M['gap'], (0.075, y, zs * 1.0501), (1, 0, 0) if zs > 0 else (0, 1, 0), (0, 1, 0) if zs > 0 else (1, 0, 0), 0.045, 0.08, n_off=0.0)
    # Kopfband und Zeilenbaender
    box(K, M['screen_dim'], (0.1212, -1.99, 0.70), (0.1222, 1.99, 0.93))
    for k in range(6):
        z = 0.42 - k * 0.25
        if k % 2 == 0:
            box(K, common.material('zeile_dunkelblau', (0.03, 0.05, 0.11), look='paint', rough=0.3, r=0.5),
                (0.1212, -1.99, z - 0.11), (0.1218, 1.99, z + 0.11))
    # Statusleuchte, Sensor, Kabel
    box(K, M['green_glow'], (0.126, 1.96, -1.03), (0.127, 1.975, -1.015))
    box(K, M['black_pl'], (0.126, 1.90, -1.032), (0.127, 1.93, -1.012))
    cyl(K, M['grey'], (0.06, 1.75, 1.05), (0.06, 1.75, 1.12), 0.018, seg=10)
    box(K, M['grey'], (0.04, 1.72, 1.04), (0.08, 1.78, 1.07), bev=0.004)
    for y in (-1.9, 1.9):
        for z in (-0.9, 0.9):
            screw(K, M['alu'], (0.126, y, z), (1, 0, 0), r=0.006)
    tx = 0.1225
    text('kopf', 'ABFLUG  DEPARTURES', 0.15, M['white_glow'], T(tx, -0.55, 0.815))
    text('uhr', '04:00', 0.15, M['amber'], T(tx, 1.62, 0.815))
    for (body, y) in (('FLUG', -1.25), ('ZIEL', -0.3), ('GATE', 0.35), ('STATUS', 1.35)):
        text(f'sp_{body}', body, 0.06, M['white_glow'], T(tx, y, 0.62))
    rows = [('1784XBA8JLP', '186', 'C32', 'WARTEN'), ('QX 0903', '36.1', 'C33', 'BOARDING 1000'), ('BT 0011', '11', 'M14', '0412'),
            ('ZZ 0000', '---', '---', '----'), ('BT 0185', '185', 'A07', 'VERSPAETET'), ('KE 0042', '???', 'B??', '0000')]
    green = common.material('boarding_gruen', (0.4, 1.0, 0.5), look='glow', glow=1.4)
    for k, (f, d, g, st) in enumerate(rows):
        z = 0.42 - k * 0.25
        mat = M['amber'] if k != 1 else green
        text(f'f{k}', f, 0.11, mat, T(tx, -1.25, z))
        text(f'd{k}', d, 0.11, mat, T(tx, -0.3, z))
        text(f'g{k}', g, 0.11, mat, T(tx, 0.35, z))
        text(f's{k}', st, 0.11, mat, T(tx, 1.35, z))
    K.finish()


def build_bta_booth():
    """Stand der Backrooms Travel Agency (Boden, 3 x 1,4 m, Front bei +y): Tresen mit runden Enden, violett
    hinterleuchtete Front mit Schriftzug, Sockel, Platte mit Kante, Klingel, Prospektstaender, Bildschirm, Formulare;
    Rueckwand mit Profilen, Zielliste und Fuessen; Leuchtkasten BTA beidseitig auf Masten (Wiki: Hauptstand im
    Check-in, violette Beschilderung; nur die Buchstaben, kein Logo)."""
    M = _m()
    K = Kit('bta')
    rng = random.Random(3602)
    xc = 1.07
    prism(K, M['anthr'], stadium(0.33, xc, 10, 1), 0.0, 0.1)
    prism(K, M['white'], stadium(0.38, xc, 12, 1), 0.1, 1.0, bev=0.006)
    prism(K, M['purple'], stadium(0.386, xc, 12, 1), 0.86, 0.885)
    prism(K, M['white_pl'], stadium(0.41, xc, 14, 1), 1.0, 1.04, bev=0.012, seg=2)
    # Front: violettes Leuchtfeld mit Schriftzug (Buchstaben als Silhouette davor)
    box(K, M['purple'], (-1.0, 0.380, 0.28), (1.0, 0.383, 0.78))
    box(K, M['alu'], (-1.02, 0.378, 0.26), (1.02, 0.388, 0.28), bev=0.002)
    box(K, M['alu'], (-1.02, 0.378, 0.78), (1.02, 0.388, 0.80), bev=0.002)
    text('front', 'BACKROOMS TRAVEL AGENCY', 0.085, M['anthr'], T(0, 0.384, 0.53) @ FACE['+y'], depth=0.006)
    # Auf der Platte
    lathe(K, M['chrome'], [(0.0, 0.0), (0.04, 0.0), (0.04, 0.006), (0.034, 0.03), (0.02, 0.045), (0.0, 0.05)], 16, T(0.6, 0.18, 1.04))
    cyl(K, M['chrome'], (0.6, 0.18, 1.09), (0.6, 0.18, 1.1), 0.006, seg=8)
    for k in range(3):  # Prospektstaender (Acryl) mit Heften
        z = 1.04 + k * 0.09
        y = 0.12 - k * 0.07
        box(K, M['glass_frost'], (-1.25, y - 0.03, z), (-0.85, y + 0.03, z + 0.004))
        for j in range(3):
            c = rng.choice(['fabric_b', 'fabric_r', 'paper', 'purple', 'fabric_o'])
            tfp = T(-1.2 + j * 0.13, y, z + 0.004) @ R(-0.25, 'X')
            box(K, M[c], (-0.05, -0.004, 0.0), (0.05, 0.004, 0.2), tf=tfp)
        box(K, M['glass_frost'], (-1.25, y + 0.03, z), (-0.85, y + 0.034, z + 0.08))
    _monitor(K, M, T(0.0, -0.2, 1.04) @ R(-math.pi / 2, 'Z'), w=0.46, h=0.29)
    box(K, M['paper'], (0.3, -0.25, 1.04), (0.51, -0.1, 1.06))
    cyl(K, M['black_pl'], (-0.5, -0.2, 1.04), (-0.5, -0.2, 1.14), 0.03, seg=12)
    for k in range(3):
        cyl(K, M['grey_pl'], (-0.5 + k * 0.01 - 0.01, -0.2, 1.1), (-0.48 + k * 0.012, -0.21, 1.2), 0.004, seg=5)
    # Sideboard vor der Rueckwand und Lagerschrank dahinter (decken den Block der Karte)
    box(K, M['white'], (-1.4, -0.92, 0.06), (1.4, -0.45, 0.98), bev=0.006)
    box(K, M['anthr'], (-1.38, -0.9, 0.0), (1.38, -0.47, 0.06))
    box(K, M['white_pl'], (-1.42, -0.92, 0.98), (1.42, -0.43, 1.01), bev=0.006)
    for k in range(4):
        x = -1.4 + k * 0.7
        box(K, M['gap'], (x + 0.698, -0.451, 0.08), (x + 0.702, -0.449, 0.96))
        box(K, M['alu'], (x + 0.3, -0.449, 0.85), (x + 0.4, -0.44, 0.87), bev=0.003)
    box(K, M['anthr'], (-1.45, -1.6, 0.0), (1.45, -1.0, 1.12), bev=0.008)
    for k in range(3):
        x = -1.45 + k * 0.9667
        box(K, M['gap'], (x - 0.002, -1.601, 0.02), (x + 0.002, -1.599, 1.1))
        box(K, M['alu'], (x + 0.4, -1.61, 0.9), (x + 0.55, -1.6, 0.92), bev=0.003)
    # Rueckwand mit Profilen, Zielliste und Fuessen
    box(K, M['anthr'], (-1.5, -1.0, 0.04), (1.5, -0.92, 2.6), bev=0.006)
    for x in (-1.5, 1.5):
        box(K, M['alu'], (x - 0.02, -1.005, 0.04), (x + 0.02, -0.915, 2.62), bev=0.004)
        box(K, M['alu'], (x - 0.05, -1.25, 0.0), (x + 0.05, -0.67, 0.04), bev=0.008)
    box(K, M['alu'], (-1.52, -1.005, 2.6), (1.52, -0.915, 2.64), bev=0.004)
    box(K, M['purple'], (-1.45, -0.919, 1.95), (1.45, -0.917, 2.25))
    text('rw1', 'BACKROOMS TRAVEL AGENCY', 0.13, M['white_glow'], T(0, -0.916, 2.1) @ FACE['+y'])
    text('rw2', 'ZIELE', 0.08, M['purple'], T(-0.9, -0.917, 1.7) @ FACE['+y'])
    for k, ln in enumerate(('LEVEL 11', 'LEVEL 186', 'LEVEL 903', 'LEVEL 36.1')):
        text(f'rwz{k}', ln, 0.065, M['white_glow'], T(-0.9, -0.917, 1.55 - k * 0.12) @ FACE['+y'])
        text(f'rwt{k}', ('---', '1784XBA8JLP', '0903', 'C33')[k], 0.065, M['amber'], T(0.3, -0.917, 1.55 - k * 0.12) @ FACE['+y'])
    # Masten und Leuchtkasten
    for x in (-1.3, 1.3):
        cyl(K, M['alu'], (x, -0.96, 2.64), (x, -0.96, 3.0), 0.025, seg=12)
    box(K, M['anthr'], (-1.5, -1.0, 3.0), (1.5, -0.9, 3.5), bev=0.015, seg=2)
    for sg in (-1, 1):
        box(K, M['anthr_gloss'], (-1.45, -0.95 + sg * 0.05 - (0.0 if sg > 0 else 0.001), 3.05), (1.45, -0.95 + sg * 0.05 + (0.001 if sg > 0 else 0.0), 3.45))
        f = FACE['+y'] if sg > 0 else FACE['-y']
        text(f'bta{sg}', 'BTA', 0.30, M['purple'], T(0.0, -0.95 + sg * 0.0515, 3.29) @ f, depth=0.01)
        text(f'bta_s{sg}', 'TRAVEL AGENCY', 0.055, M['white_glow'], T(0.0, -0.95 + sg * 0.0515, 3.1) @ f)
    for i in range(4):
        blob(K, M['rubber_mark'], (rng.uniform(-1.0, 1.0), 0.3305, rng.uniform(0.0, 0.09)), (-1, 0, 0), (0, 0, 1),
             rng.uniform(0.05, 0.12), 0.03, rng, n_off=0.0005)
    K.finish()


# ======================================================================================================================
# Wege: Laufband, Wegweiser, Ausgangsschild
# ======================================================================================================================

def _face_poly(K, mat, tf, pts, off=0.0):
    """Ebenes Vieleck [(u, z)] in der Flaeche (blickt nach tf @ +x), um off vor der Flaeche."""
    if _area(pts) < 0:
        pts = pts[::-1]
    if _area(pts) < 0:
        pts = pts[::-1]
    vv = [tuple(tf @ Vector((off, u, z))) for (u, z) in pts]
    return poly(K, mat, vv, [tuple(range(len(vv)))])


ARROW = [(-0.09, -0.024), (0.015, -0.024), (0.015, -0.068), (0.1, 0.0), (0.015, 0.068), (0.015, 0.024), (-0.09, 0.024)]
PLANE = [(0.0, 0.11), (0.012, 0.095), (0.014, 0.03), (0.1, -0.015), (0.1, -0.035), (0.014, -0.012), (0.01, -0.07), (0.035, -0.09),
         (0.035, -0.105), (0.0, -0.095), (-0.035, -0.105), (-0.035, -0.09), (-0.01, -0.07), (-0.014, -0.012), (-0.1, -0.035),
         (-0.1, -0.015), (-0.014, 0.03), (-0.012, 0.095)]


def _arrow(K, mat, tf, u, z, d, s=1.0, off=0.0):
    _face_poly(K, mat, tf, [(u + d * a * s, z + b * s) for (a, b) in ARROW], off)


def _plane_icon(K, mat, tf, u, z, s=1.0, off=0.0, rot=0.0):
    c, sn = math.cos(rot), math.sin(rot)
    _face_poly(K, mat, tf, [(u + (a * c - b * sn) * s, z + (a * sn + b * c) * s) for (a, b) in PLANE], off)


def _case_icon(K, mat, tf, u, z, s=1.0, off=0.0):
    _face_poly(K, mat, tf, [(u + a * s, z - 0.02 * s + b * s) for (a, b) in rrect(0.17, 0.12, 0.018, 2)], off)
    _face_poly(K, mat, tf, [(u + a * s, z + b * s) for (a, b) in
                            ((-0.04, 0.04), (-0.025, 0.04), (-0.025, 0.063), (0.025, 0.063), (0.025, 0.04), (0.04, 0.04),
                             (0.04, 0.078), (-0.04, 0.078))], off)


def _travelator(stopped=False):
    """Laufband (Boden, 12 m entlang x, Paletten 1 m breit): Palettenband mit Laengsrillen, Palettenfugen und gelben
    Randmarken, Kammplatten mit Zaehnen, Flurplatten mit Rillen und Schrauben, Sockelblech mit Buerstenleiste,
    schraeges Innendeck, Aussenverkleidung, Glasbalustrade mit Klemmen, Gummihandlauf mit Umlenkung an beiden Enden,
    Not-Aus, Fahrtrichtungsanzeige. stopped: Paletten fehlen an einer Stelle, Warnaufsteller, rote Anzeigen
    (Wiki: stehende oder stotternde Laufbaender meiden)."""
    M = _m()
    K = Kit('laufband')
    rng = random.Random(3610 + stopped)
    XB = 5.25  # Palettenband bis Kammplatte
    # Palettenband (Laengsrillen, Profil quer)
    p = 0.02
    prof = [(0.09, -0.5)]
    for i in range(50):
        y = -0.5 + i * p
        prof += [(0.12, y + 0.003), (0.12, y + p - 0.003), (0.108, y + p - 0.002), (0.108, y + p)]
    prof += [(0.09, 0.5)]
    segs = [(-XB, XB)] if not stopped else [(-XB, -1.25), (-0.45, XB)]
    for (a, b) in segs:
        sweep(K, M['alu'], [(a, 0, 0), (b, 0, 0)], [(z, -y) for (z, y) in prof], up=(0, 0, 1), caps=True)
    for k in range(int(2 * XB / 0.4) + 1):
        x = -XB + k * 0.4
        if stopped and -1.25 < x < -0.45:
            continue
        box(K, M['gap'], (x - 0.0025, -0.5, 0.119), (x + 0.0025, 0.5, 0.1212))
        for s in (-1, 1):
            if k < int(2 * XB / 0.4):
                box(K, M['yellow_pl'], (x + 0.004, s * 0.5 - (0.05 if s > 0 else 0), 0.1195), (x + 0.396, s * 0.5 + (0.05 if s < 0 else 0), 0.1212))
    if stopped:  # Loch: fehlende Paletten, darunter Kette und Rollen
        box(K, M['gap'], (-1.25, -0.5, 0.02), (-0.45, 0.5, 0.03))
        for s in (-1, 1):
            box(K, M['steel_dull'], (-1.3, s * 0.45 - 0.01, 0.03), (-0.4, s * 0.45 + 0.01, 0.07))
            for k in range(9):
                cyl(K, M['black_pl'], (-1.2 + k * 0.1, s * 0.47, 0.06), (-1.2 + k * 0.1, s * 0.42, 0.06), 0.03, seg=10)
        cyl(K, M['steel_dull'], (-0.85, -0.5, 0.06), (-0.85, 0.5, 0.06), 0.012, seg=8)
        box(K, M['alu'], (-1.25, -0.5, 0.09), (-1.2, 0.5, 0.12), bev=0.004)
    # Kammplatten und Flurplatten an beiden Enden
    for e in (-1, 1):
        x0 = e * XB
        box(K, M['alu'], (min(x0, x0 + e * 0.3), -0.505, 0.105), (max(x0, x0 + e * 0.3), 0.505, 0.135), bev=0.004)
        for i in range(50):
            y = -0.5 + i * p + p / 2
            tri = [(x0 + e * 0.01, y - 0.0055), (x0 - e * 0.045, y - 0.003), (x0 - e * 0.045, y + 0.003), (x0 + e * 0.01, y + 0.0055)]
            prism(K, M['yellow_pl'], tri, 0.113, 0.128)
        box(K, M['yellow_pl'], (min(x0 + e * 0.01, x0 + e * 0.04), -0.5, 0.135), (max(x0 + e * 0.01, x0 + e * 0.04), 0.5, 0.137))
        xa, xb = sorted((x0 + e * 0.3, x0 + e * 0.70))
        box(K, M['alu'], (xa, -0.505, 0.0), (xb, 0.505, 0.135), bev=0.003)
        for k in range(15):
            xx = xa + 0.02 + k * 0.025
            box(K, M['alu'], (xx, -0.47, 0.135), (xx + 0.008, 0.47, 0.139))
        for xx in (xa + 0.02, xb - 0.02):
            for yy in (-0.48, 0.48):
                screw(K, M['steel_dull'], (xx, yy, 0.135), (0, 0, 1), r=0.006, seg=6)
        box(K, M['gap'], (min(x0 + e * 0.3, x0 + e * 0.302), -0.505, 0.134), (max(x0 + e * 0.3, x0 + e * 0.302), 0.505, 0.1355))
    # Balustrade je Seite: Verkleidung (Sweep), Buerste, Glas, Klemmen, Handlauf
    XE = 5.95
    for s in (-1, 1):
        prof = [(0.0, 0.505), (0.0, 0.70), (0.30, 0.70), (0.335, 0.665), (0.335, 0.60), (0.225, 0.52), (0.22, 0.505)]
        sweep(K, M['steel'], [(-XE, 0, 0), (XE, 0, 0)], [(z, -s * y) for (z, y) in prof], up=(0, 0, 1))
        box(K, M['rubber'], (-XB, s * 0.505 - (0.022 if s > 0 else 0.0), 0.165), (XB, s * 0.505 + (0.022 if s < 0 else 0.0), 0.18))
        # Fugen der Verkleidung aussen
        for k in range(1, 8):
            x = -XE + k * (2 * XE / 8)
            box(K, M['gap'], (x - 0.0015, s * 0.70 - 0.001, 0.01), (x + 0.0015, s * 0.70 + 0.001, 0.29))
        # Glas (Felder; Endfelder mit Rundung)
        xs = [-5.55, -3.7, -1.85, 0.0, 1.85, 3.7, 5.55]
        for k in range(6):
            a, b = xs[k] + 0.004, xs[k + 1] - 0.004
            pts = [(a, 0.335), (b, 0.335), (b, 0.985), (a, 0.985)]
            if k == 5:
                pts = [(a, 0.335), (b, 0.335)] + [(5.55 + 0.32 * math.cos(t), 0.66 + 0.325 * math.sin(t)) for t in
                                                  [-math.pi / 2 + math.pi * j / 10 for j in range(1, 10)]] + [(b, 0.985), (a, 0.985)]
            if k == 0:
                pts = [(a, 0.335), (b, 0.335), (b, 0.985), (a, 0.985)] + [(-5.55 - 0.32 * math.cos(t), 0.66 + 0.325 * math.sin(t)) for t in
                                                                         [math.pi / 2 - math.pi * j / 10 for j in range(1, 10)]]
            prism(K, M['glass'], pts, -0.006, 0.006, tf=T(0, s * 0.625, 0) @ R(math.pi / 2, 'X'))
        for k in range(13):
            x = -5.3 + k * 0.885
            box(K, M['steel'], (x - 0.04, s * 0.625 - 0.022, 0.335), (x + 0.04, s * 0.625 + 0.022, 0.375), bev=0.005)
        # Handlauf: Umlenkung um beide Enden
        path = []
        for j in range(15):
            t = -math.pi / 2 + math.pi * j / 14
            path.append((5.55 + 0.335 * math.cos(t), s * 0.625, 0.66 + 0.335 * math.sin(t)))
        for j in range(15):
            t = math.pi / 2 + math.pi * j / 14
            path.append((-5.55 + 0.335 * math.cos(t), s * 0.625, 0.66 + 0.335 * math.sin(t)))
        hr = [(u, v) for (u, v) in rrect(0.078, 0.05, 0.018, 3, 0.0, -0.022)]
        sweep(K, M['rubber'], path, hr, up=(0, 1, 0))
        # Not-Aus am Innendeck nahe den Enden
        for e in (-1, 1):
            c = Vector((e * 5.0, s * 0.56, 0.28))
            box(K, M['yellow_pl'], (c.x - 0.04, c.y - 0.03, c.z - 0.02), (c.x + 0.04, c.y + 0.03, c.z + 0.02), bev=0.005)
            lathe(K, M['red_pl'], [(0.0, 0.0), (0.012, 0.0), (0.012, 0.01), (0.022, 0.013), (0.02, 0.022), (0.0, 0.024)], 12,
                  T(c.x, c.y, c.z + 0.02))
    # Fahrtrichtungsanzeige an den Stirnen (Verkleidung, nach aussen)
    for e in (-1, 1):
        for s in (-1, 1):
            c = (e * (XE + 0.0005), s * 0.6, 0.2)
            box(K, M['black_pl'], (e * XE - (0.0 if e > 0 else 0.01), s * 0.6 - 0.05, 0.15), (e * XE + (0.01 if e > 0 else 0.0), s * 0.6 + 0.05, 0.25), bev=0.004)
            f = FACE['+x'] if e > 0 else FACE['-x']
            tf = T(e * (XE + 0.0105), s * 0.6, 0.2) @ f
            if stopped or e > 0:
                _face_poly(K, M['red_glow'], tf, circle(0.035, 12))
                _face_poly(K, M['white_glow'], tf, [(a, b) for (a, b) in rrect(0.05, 0.012, 0.004, 1)], off=0.0006)
            else:
                _face_poly(K, M['green_glow'], tf, [(-b * 0.4, a * 0.4) for (a, b) in ARROW])
    if stopped:  # Warnaufsteller
        for sg in (-1, 1):
            tfa = T(-6.42, 0.0, 0.94) @ R(sg * 0.2, 'Y') @ T(0, 0, -0.94)
            box(K, M['yellow_pl'], (-0.008, -0.28, 0.02), (0.008, 0.28, 0.93), tf=tfa, bev=0.004)
        tfw = T(-6.42 - 0.47 * math.sin(0.2) - 0.0095 * math.cos(0.2), 0.0, 0.47 + 0.0095 * math.sin(0.2)) @ R(0.2, 'Y') @ FACE['-x']
        _face_poly(K, M['black'], tfw, [(-0.22, 0.12), (-0.17, 0.17), (0.0, 0.05), (0.17, 0.17), (0.22, 0.12), (0.05, 0.0),
                                        (0.22, -0.12), (0.17, -0.17), (0.0, -0.05), (-0.17, -0.17), (-0.22, -0.12), (-0.05, 0.0)])
        text('ab', 'AUSSER BETRIEB', 0.05, M['black'], tfw @ T(0.0, 0.0, -0.3))
        box(K, M['black_pl'], (-6.62, -0.3, 0.0), (-6.22, -0.26, 0.03), bev=0.005)
        box(K, M['black_pl'], (-6.62, 0.26, 0.0), (-6.22, 0.3, 0.03), bev=0.005)
    # Abrieb auf den Paletten (Laufspur), Schmutz an den Kammplatten
    for i in range(10):
        blob(K, M['scuff'], (rng.uniform(-5.0, 5.0), rng.uniform(-0.25, 0.25), 0.1212), (1, 0, 0), (0, 1, 0), rng.uniform(0.3, 0.9),
             rng.uniform(0.1, 0.3), rng, n_off=0.0003)
    for e in (-1, 1):
        blob(K, M['dirt'], (e * (XB + 0.15), 0.0, 0.1352), (1, 0, 0), (0, 1, 0), 0.25, 0.8, rng, n_off=0.0003)
    K.finish()


def build_travelator():
    _travelator(False)


def build_travelator_stopped():
    _travelator(True)


def _wayfinder(gates_world=-1, empty=False):
    """Haengender Wegweiser (Deckenteil, Decke bei z = 0, 3 m entlang y): Alu-Strangprofil mit runden Kanten,
    Endkappen, beidseitige Leuchtfelder (gelb auf Anthrazit), Pfeile und Bildzeichen, Gewindestangen mit Rosetten,
    Abhaengern und Spannschloss, Zuleitung. gates_world: Richtung (Welt-y) der Gates auf beiden Seiten gleich."""
    M = _m()
    K = Kit('wegweiser')
    rng = random.Random(3620 + gates_world)
    zc, H, Wd, L = -0.975, 0.55, 0.16, 1.48
    tilt = R(math.radians(-3.0), 'X') if empty else Matrix.Identity(4)
    piv = T(0, 1.2, -0.7) @ tilt @ T(0, -1.2, 0.7)
    hk = Kit('gehaeuse')
    sweep(hk, M['alu_dark'], [(0, -L, 0), (0, L, 0)], [(u + zc, v) for (u, v) in rrect(H, Wd, 0.03, 3)], up=(0, 0, 1))
    for e in (-1, 1):
        sweep(hk, M['anthr'], [(0, e * L, 0), (0, e * (L + 0.02), 0)], [(u + zc, v) for (u, v) in rrect(H + 0.012, Wd + 0.012, 0.035, 3)], up=(0, 0, 1))
        for z in (zc - 0.2, zc + 0.2):
            screw(hk, M['steel_dull'], (0, e * (L + 0.02), z), (0, e, 0), r=0.005)
    faces = ('+x', '-x')
    for fname in faces:
        sx = 1 if fname == '+x' else -1
        tf = T(sx * (Wd / 2 + 0.0004), 0, zc) @ FACE[fname]
        broken = empty and fname == '+x'
        if broken:
            _face_poly(hk, M['anthr_gloss'], tf, [(-1.44, -0.235), (-0.1, -0.235), (0.05, -0.1), (-0.12, 0.02), (0.08, 0.12),
                                                  (0.0, 0.235), (-1.44, 0.235)])
            # Inneres: dunkel, zwei tote Leuchtstoffroehren mit Fassungen
            tfi = T(sx * 0.03, 0, zc) @ FACE[fname]
            _face_poly(hk, M['gap'], tfi, [(-0.2, -0.24), (1.44, -0.24), (1.44, 0.24), (-0.2, 0.24)])
            for z in (-0.1, 0.1):
                cyl(hk, M['white_dull'], (sx * 0.045, -0.15, zc + z), (sx * 0.045, 1.4, zc + z), 0.013, seg=8)
                for y in (-0.17, 1.42):
                    box(hk, M['white_pl'], (sx * 0.03, y - 0.015, zc + z - 0.02), (sx * 0.06, y + 0.015, zc + z + 0.02))
            for i in range(5):  # Splitter am Rand
                u0 = rng.uniform(0.0, 1.3)
                _face_poly(hk, M['anthr_gloss'], tf, [(u0, -0.235), (u0 + rng.uniform(0.05, 0.15), -0.235), (u0 + 0.03, -0.235 + rng.uniform(0.03, 0.09))])
            continue
        _face_poly(hk, M['anthr_gloss'], tf, rrect(2.9, 0.47, 0.02, 2))
        if empty:
            continue
        gs = gates_world * sx  # Seite (in Leserichtung) der Gates auf dieser Flaeche
        o = 0.0012
        _arrow(hk, M['yellow'], tf, gs * 1.27, 0.0, gs, 1.3, o)
        _plane_icon(hk, M['yellow'], tf, gs * 1.0, 0.0, 1.05, o, rot=-gs * math.pi / 2)
        text(f'gates{fname}', 'GATES A-M', 0.15, M['yellow'], tf @ T(o, gs * 0.47, 0.0))
        _arrow(hk, M['yellow'], tf, -gs * 1.27, 0.0, -gs, 1.3, o)
        _case_icon(hk, M['yellow'], tf, -gs * 1.0, 0.0, 1.0, o)
        text(f'gep{fname}', 'GEPAECK', 0.15, M['yellow'], tf @ T(o, -gs * 0.47, 0.0))
        _face_poly(hk, M['yellow'], tf, [(-0.004, -0.2), (0.004, -0.2), (0.004, 0.2), (-0.004, 0.2)], o)
    _merge(K, hk, piv)
    # Abhaengung
    for e in (-1, 1):
        y = e * 1.2
        lathe(K, M['white_dull'], [(0.0, -0.02), (0.012, -0.02), (0.045, -0.006), (0.045, 0.0), (0.0, 0.0)], 16, T(0, y, 0))
        if empty and e < 0:  # abgerissene Stange haengt lose
            cyl(K, M['steel_dull'], (0, y, -0.02), (0.02, y + 0.03, -0.42), 0.006, seg=6)
            continue
        top = Vector((0, y, -0.02))
        bot = piv @ Vector((0, y, -0.69))
        cyl(K, M['steel_dull'], top, bot, 0.006, seg=6)
        mid = (top + bot) / 2
        cyl(K, M['steel'], mid - Vector((0, 0, 0.05)), mid + Vector((0, 0, 0.05)), 0.011, seg=8)
        box(K, M['steel'], (-0.015, -0.01, -0.03), (0.015, 0.01, 0.0), tf=T(*bot), bev=0.003)
    # Zuleitung
    pts = [piv @ Vector((0.0, 0.85, -0.70)), Vector((0.03, 0.8, -0.45)), Vector((0.02, 0.72, -0.2)), Vector((0.0, 0.7, -0.01))]
    if empty:
        pts = [piv @ Vector((0.0, 0.85, -0.70)), Vector((0.06, 0.6, -0.55)), Vector((0.1, 0.4, -0.6)), Vector((0.12, 0.3, -0.45))]
    tube(K, M['black_pl'], fillet(pts, 0.1, 3), 0.0045, seg=6)
    box(K, M['white_dull'], (-0.05, 0.62, -0.03), (0.05, 0.78, 0.0), bev=0.004)
    K.finish()


def build_sign_36_a():
    """Wegweiser: Gates und Gepaeck in Gegenrichtungen, auf beiden Seiten gleich (Welt-Richtung)."""
    _wayfinder(-1)


def build_sign_36_b():
    """Widerspruechlicher Wegweiser (Wiki: Schilder widersprechen sich): alle Richtungen umgekehrt."""
    _wayfinder(1)


def build_sign_36_empty():
    """Leerer, kaputter Wegweiser (Wiki: leere oder kaputte Schilder markieren Sackgassen): Leuchtfeld leer, eine
    Abdeckung herausgebrochen (tote Roehren sichtbar), eine Stange abgerissen, Kasten haengt schief, Kabel lose."""
    _wayfinder(-1, empty=True)


RUNNER = [(-0.02, 0.07), (0.02, 0.07), (0.035, 0.035), (0.075, 0.01), (0.07, -0.005), (0.03, 0.015), (0.02, -0.03), (0.06, -0.06),
          (0.05, -0.1), (0.035, -0.1), (0.04, -0.065), (0.0, -0.045), (-0.03, -0.085), (-0.07, -0.08), (-0.065, -0.068),
          (-0.04, -0.07), (-0.015, -0.02), (-0.02, 0.025), (-0.05, 0.0), (-0.065, 0.01), (-0.03, 0.05)]


def build_exit_sign_36():
    """Ausgangsschild (Wandteil, 1,2 x 0,4 m): Alu-Gehaeuse mit runden Kanten auf Wandwinkeln, gruenes Leuchtfeld,
    weisses Bildzeichen (Laeufer, Tuer, Pfeil) und EXIT, Leitungsrohr mit Abzweigdose (Wiki: Ausgangsschildern nicht
    trauen - es gibt keinen echten Ausgang). Schrift ohne entartete Dreiecke."""
    M = _m()
    K = Kit('exit')
    for y in (-0.45, 0.45):
        box(K, M['alu'], (0.0, y - 0.03, -0.15), (0.012, y + 0.03, 0.15), bev=0.002)
        for z in (-0.12, 0.12):
            screw(K, M['steel_dull'], (0.012, y, z), (1, 0, 0), r=0.004)
    sweep(K, M['alu'], [(0.012, 0, 0), (0.072, 0, 0)], [(a, b) for (a, b) in rrect(1.2, 0.4, 0.03, 3)], up=(0, 1, 0))
    tf = T(0.0725, 0, 0)
    _face_poly(K, M['green_glow'], tf, rrect(1.13, 0.33, 0.015, 2))
    o = 0.0008
    _face_poly(K, M['white_glow'], tf, [(-0.42 + a * 1.25, b * 1.25 - 0.01) for (a, b) in RUNNER], o)
    _face_poly(K, M['white_glow'], tf, circle(0.022, 10, -0.42 + 0.01, 0.125), o)
    for (a, b, c, d) in ((-0.31, -0.14, -0.295, 0.14), (-0.31, 0.125, -0.19, 0.14), (-0.205, -0.14, -0.19, 0.14)):
        _face_poly(K, M['white_glow'], tf, [(a, b), (c, b), (c, d), (a, d)], o)
    text('exit', 'EXIT', 0.15, M['white_glow'], tf @ T(o, 0.08, 0.0), res=3)
    _arrow(K, M['white_glow'], tf, 0.42, 0.0, 1, 1.0, o)
    cyl(K, M['grey'], (0.04, 0.5, 0.2), (0.04, 0.5, 0.235), 0.012, seg=10)
    box(K, M['grey'], (0.015, 0.45, 0.2), (0.065, 0.55, 0.215), bev=0.004)
    K.finish()


# ======================================================================================================================
# Ladenzeile
# ======================================================================================================================

def _mullion(K, M, y, z0, z1, x=0.86, w=0.06, d=0.12, mat='alu_dark'):
    """Senkrechter Pfosten aus Strangprofil mit Glasnut (Querschnitt in xy)."""
    pr = [(x - d / 2, y - w / 2), (x + d / 2, y - w / 2), (x + d / 2, y - w / 2 + 0.008), (x + d / 2 - 0.004, y - w / 2 + 0.012),
          (x + d / 2 - 0.004, y + w / 2 - 0.012), (x + d / 2, y + w / 2 - 0.008), (x + d / 2, y + w / 2), (x - d / 2, y + w / 2)]
    prism(K, M[mat], pr, z0, z1)


def _transom(K, M, z, y0, y1, x=0.86, h=0.06, d=0.12, mat='alu_dark'):
    box(K, M[mat], (x - d / 2, y0, z - h / 2), (x + d / 2, y1, z + h / 2), bev=0.004)


def _storefront_base(K, M, name, glow_rgb, seed, closed=False):
    """Ladenfront (Wandteil, 6 m entlang y, Rueckwand bei x = 0): Pfeiler mit Fuge, Sockel mit Edelstahl-Trittblech,
    Pfosten-Riegel-Fassade aus Alu-Strangprofil mit Dichtungen, Schaufenster, Tuer mit Stossgriff und Bodenschiene,
    Rollladenkasten mit Fuehrungsschienen, Leuchtkasten mit erhabenen Buchstaben; innen Rueckwand, Deckenlicht."""
    lit = common.material(f'laden_licht{seed}', glow_rgb, look='glow', glow=0.9)
    gx = 0.86  # Glasebene
    box(K, M['white_dull'] if not closed else M['grey'], (0.0, -3.0, 0.0), (0.02, 3.0, 3.2))  # Rueckwand innen
    for y0 in (-3.0, 2.85):  # Pfeiler
        box(K, M['anthr'], (0.0, y0, 0.0), (0.95, y0 + 0.15, 3.2), bev=0.01)
        box(K, M['gap'], (0.9505, y0 + 0.07, 0.05), (0.9515, y0 + 0.08, 3.15))
    box(K, M['anthr'], (0.6, -2.85, 0.0), (0.92, 0.62, 0.32), bev=0.008)  # Sockel unter dem Schaufenster
    box(K, M['steel'], (0.92, -2.85, 0.0), (0.925, 0.62, 0.3), bev=0.002)
    # Fassade: Pfosten, Riegel, Glas, Dichtungen
    for y in (-2.82, -1.1, 0.62, 2.82):
        _mullion(K, M, y, 0.0 if y > 0.6 else 0.32, 3.12)
    _transom(K, M, 3.09, -2.85, 2.85)
    _transom(K, M, 0.35, -2.85, 0.62)
    _transom(K, M, 2.62, 0.62, 2.82)
    for (a, b, z0, z1) in ((-2.79, -1.13, 0.38, 3.06), (-1.07, 0.59, 0.38, 3.06), (0.65, 2.79, 2.65, 3.06)):
        box(K, M['glass'], (gx - 0.004, a, z0), (gx + 0.004, b, z1))
        for (pa, pb) in (((gx + 0.004, a, z0), (gx + 0.012, b, z0 + 0.012)), ((gx + 0.004, a, z1 - 0.012), (gx + 0.012, b, z1)),
                         ((gx + 0.004, a, z0), (gx + 0.012, a + 0.012, z1)), ((gx + 0.004, b - 0.012, z0), (gx + 0.012, b, z1))):
            box(K, M['rubber'], pa, pb)
    # Tuer: Rahmen, Glas, Stossgriff, Trittblech, Bodenschiene
    dy0, dy1 = 0.66, 2.78
    for (a, b) in ((dy0, dy0 + 0.07), (dy1 - 0.07, dy1)):
        box(K, M['alu_dark'], (gx - 0.03, a, 0.0), (gx + 0.03, b, 2.58), bev=0.004)
    box(K, M['alu_dark'], (gx - 0.03, dy0, 2.5), (gx + 0.03, dy1, 2.58), bev=0.004)
    box(K, M['alu_dark'], (gx - 0.03, dy0, 0.0), (gx + 0.03, dy1, 0.22), bev=0.004)
    box(K, M['steel'], (gx + 0.03, dy0 + 0.07, 0.01), (gx + 0.033, dy1 - 0.07, 0.2), bev=0.001)
    box(K, M['glass'], (gx - 0.004, dy0 + 0.07, 0.22), (gx + 0.004, dy1 - 0.07, 2.5))
    for sx in (-1, 1):  # Stossgriff beidseitig
        xx = gx + sx * 0.03
        cyl(K, M['steel'], (xx + sx * 0.06, dy0 + 0.16, 0.85), (xx + sx * 0.06, dy0 + 0.16, 1.55), 0.016, seg=10)
        for z in (0.9, 1.5):
            cyl(K, M['steel'], (xx, dy0 + 0.16, z), (xx + sx * 0.06, dy0 + 0.16, z), 0.01, seg=8)
    box(K, M['steel'], (gx - 0.05, dy0, -0.002), (gx + 0.12, dy1, 0.006), bev=0.002)
    box(K, M['steel_dull'], (gx - 0.02, dy0 + 0.1, 2.58), (gx + 0.05, dy0 + 0.45, 2.62), bev=0.005)  # Tuerschliesser
    # Rollladenkasten und Fuehrungsschienen
    box(K, M['grey'], (0.86, -2.85, 3.12), (0.95, 2.85, 3.2), bev=0.004)
    for y in (-2.85, 2.79):
        box(K, M['steel_dull'], (0.905, y, 0.0), (0.945, y + 0.06, 3.12), bev=0.002)
        box(K, M['gap'], (0.92, y + 0.02, 0.01), (0.946, y + 0.04, 3.1))
    # Leuchtkasten mit Buchstaben
    box(K, M['anthr'], (0.0, -3.0, 3.2), (0.95, 3.0, 3.8), bev=0.012, seg=2)
    box(K, lit, (0.95, -2.9, 3.27), (0.952, 2.9, 3.73))
    box(K, M['alu'], (0.948, -2.92, 3.25), (0.958, 2.92, 3.27), bev=0.002)
    box(K, M['alu'], (0.948, -2.92, 3.73), (0.958, 2.92, 3.75), bev=0.002)
    if name:
        text('name', name, 0.3, M['anthr_gloss'], T(0.952, 0.0, 3.5), depth=0.02)
    for y in (-2.0, 2.0):  # Wandhalter des Kastens
        screw(K, M['steel_dull'], (0.952, y, 3.26), (1, 0, 0), r=0.005)
    if not closed:
        box(K, lit, (0.02, -2.85, 3.08), (0.7, -2.8, 3.11))
        box(K, lit, (0.02, -2.85, 3.08), (0.06, 2.8, 3.11))
    # Gebrauchsspuren: Schmutzkante am Sockel, Fingerabdruecke am Griff, Aufkleber
    rng = random.Random(seed)
    blob(K, M['dirt'], (0.9256, -1.0, 0.04), (0, 1, 0), (0, 0, 1), 3.0, 0.06, rng, n_off=0.0005, jit=0.1)
    plate(K, M['white_pl'], (gx + 0.0045, dy0 + 0.6, 1.5), (0, 1, 0), (0, 0, 1), 0.16, 0.1, n_off=0.0)
    text('oeffnung', '00:00 - 00:00', 0.018, M['black'], T(gx + 0.0047, dy0 + 0.6, 1.5))
    return gx


def _shelf_unit(K, M, y0, y1, levels, x0=0.02, x1=0.42, tilt=0.0):
    for z in levels:
        box(K, M['white_pl'], (x0, y0, z), (x1, y1, z + 0.02), bev=0.003)
        box(K, M['alu'], (x1 - 0.005, y0, z - 0.02), (x1, y1, z + 0.03))
    for y in (y0, y1 - 0.02):
        box(K, M['alu'], (x0, y, levels[0] - 0.3 if levels[0] > 0.3 else 0.0), (x0 + 0.03, y + 0.02, levels[-1] + 0.02))


def build_storefront_a():
    """Zeitungsladen: Regalwand mit schraegen Zeitschriftenfaechern, Zeitschriften (Titelband, Farben), Zeitungsstapel
    mit Band, Kasse (Wiki: Laeden wie in der Frontrooms-Welt, meist wertlose Ware)."""
    M = _m()
    K = Kit('laden_a')
    rng = random.Random(1)
    _storefront_base(K, M, 'ZEITUNGEN', (1.0, 0.96, 0.86), 1)
    covers = ['fabric_r', 'fabric_b', 'paper', 'yellow_pl', 'fabric_g', 'white_pl', 'red_pl', 'fabric_o']
    for r_, z in enumerate((0.55, 0.95, 1.35, 1.75, 2.15)):
        box(K, M['wood'], (0.02, -2.75, z - 0.02), (0.36, 0.5, z), bev=0.003)
        box(K, M['glass_frost'], (0.33, -2.75, z), (0.36, 0.5, z + 0.07))
        y = -2.72
        while y < 0.42:
            w = rng.uniform(0.2, 0.24)
            tfm = T(0.27, y + w / 2, z) @ R(-0.28, 'Y')
            c = M[rng.choice(covers)]
            box(K, c, (-0.006, -w / 2, 0.0), (0.006, w / 2, 0.29), tf=tfm)
            plate(K, M[rng.choice(['white_pl', 'black', 'red_pl'])], tfm @ Vector((0.0065, 0, 0.25)), tfm.to_3x3() @ Vector((0, 1, 0)),
                  tfm.to_3x3() @ Vector((0, 0, 1)), w * 0.9, 0.045, n_off=0.0)
            plate(K, M['paper'], tfm @ Vector((0.0065, -w * 0.15, 0.12)), tfm.to_3x3() @ Vector((0, 1, 0)), tfm.to_3x3() @ Vector((0, 0, 1)),
                  w * 0.5, 0.08, n_off=0.0) if rng.random() < 0.5 else None
            y += w + 0.015
    for k in range(4):  # Zeitungsstapel am Boden
        y = -2.5 + k * 0.7
        h = rng.uniform(0.15, 0.32)
        box(K, M['paper'], (0.08, y, 0.0), (0.48, y + 0.32, h), bev=0.004)
        jitter(K, M['paper'], 0.0, rng, [])
        for d in (0.1, 0.22):
            box(K, M['white_dull'], (0.075, y + d, 0.0), (0.485, y + d + 0.012, h + 0.003))
        for j in range(int(h / 0.02)):
            plate(K, M['gap'], (0.4805, y + 0.16, 0.01 + j * 0.02), (0, 1, 0), (0, 0, 1), 0.3, 0.002, n_off=0.0)
    # Kasse hinter der Tuer
    box(K, M['wood'], (0.1, 1.1, 0.0), (0.6, 2.2, 0.95), bev=0.008)
    box(K, M['white_pl'], (0.08, 1.08, 0.95), (0.62, 2.22, 0.98), bev=0.006)
    box(K, M['black_pl'], (0.2, 1.4, 0.98), (0.4, 1.7, 1.08), bev=0.01)
    box(K, M['screen_off'], (0.4, 1.45, 1.0), (0.401, 1.65, 1.07))
    K.finish()


def build_storefront_b():
    """Kaffee: Theke mit Glasvitrine (Gebaeck aus Pappe), Siebtraegermaschine, Becherstapel, Mahlwerk, Tafel mit
    Preisen (--,--)."""
    M = _m()
    K = Kit('laden_b')
    rng = random.Random(2)
    _storefront_base(K, M, 'KAFFEE', (1.0, 0.86, 0.66), 2)
    box(K, M['wood'], (0.02, -2.8, 0.0), (0.62, 0.5, 0.95), bev=0.01)
    box(K, M['anthr'], (0.0, -2.8, 0.95), (0.64, 0.5, 0.99), bev=0.008)
    box(K, M['glass'], (0.4, -2.6, 0.99), (0.405, -0.9, 1.35))
    box(K, M['glass'], (0.1, -2.6, 1.35), (0.405, -0.9, 1.355))
    box(K, M['white_pl'], (0.1, -2.6, 1.15), (0.4, -0.9, 1.16))
    for k in range(10):  # Gebaeck
        y = -2.5 + k * 0.16
        lathe(K, M['card'], [(0.0, 0.0), (0.05, 0.0), (0.055, 0.02), (0.035, 0.04), (0.0, 0.045)], 10,
              T(0.25 + rng.uniform(-0.05, 0.05), y, 0.99) @ S(1.0, 1.0, rng.uniform(0.8, 1.2)))
        lathe(K, M['card_dark'], [(0.0, 0.0), (0.04, 0.0), (0.045, 0.015), (0.0, 0.035)], 8, T(0.22, y + 0.05, 1.16))
    # Maschine
    box(K, M['chrome'], (0.05, -0.6, 0.99), (0.5, 0.2, 1.42), bev=0.02, seg=2)
    box(K, M['black_pl'], (0.05, -0.62, 1.42), (0.52, 0.22, 1.46), bev=0.008)
    for y in (-0.4, -0.1):
        cyl(K, M['chrome'], (0.5, y, 1.25), (0.56, y, 1.25), 0.03, seg=12)
        cyl(K, M['black_pl'], (0.56, y, 1.23), (0.62, y, 1.21), 0.012, seg=8)
        lathe(K, M['white_pl'], [(0.0, 0.0), (0.025, 0.0), (0.032, 0.06), (0.028, 0.062), (0.0, 0.004)], 10, T(0.53, y, 1.03))
    box(K, M['steel'], (0.45, -0.55, 1.0), (0.6, 0.15, 1.02))
    for k in range(4):  # Becherstapel und Muehle
        lathe(K, M['white_pl'], [(0.0, 0.0), (0.03, 0.0), (0.042, 0.25 + k * 0.03), (0.0, 0.25 + k * 0.03)], 12, T(0.3, 0.3 + k * 0.05, 0.99))
    box(K, M['anthr'], (0.25, 0.42, 0.99), (0.4, 0.48, 1.3), bev=0.01)
    lathe(K, M['glass_frost'], [(0.0, 0.0), (0.05, 0.0), (0.07, 0.15), (0.0, 0.16)], 12, T(0.32, 0.45, 1.3))
    # Preistafel an der Rueckwand
    box(K, M['anthr'], (0.02, -2.6, 1.7), (0.04, -0.6, 2.6), bev=0.006)
    box(K, M['wood'], (0.02, -2.63, 1.67), (0.045, -0.57, 1.7))
    for k, (a, b) in enumerate((('KAFFEE', '--,--'), ('ESPRESSO', '--,--'), ('TEE', '--,--'), ('KAKAO', '--,--'))):
        text(f'tk{k}', a, 0.07, M['white_dull'], T(0.041, -2.1, 2.42 - k * 0.17), align='CENTER')
        text(f'tp{k}', b, 0.07, M['white_dull'], T(0.041, -1.0, 2.42 - k * 0.17))
    # Stehtisch im Laden
    cyl(K, M['steel'], (0.6, 1.8, 0.0), (0.6, 1.8, 1.05), 0.03, seg=12)
    lathe(K, M['steel'], [(0.0, 0.0), (0.22, 0.0), (0.2, 0.015), (0.0, 0.02)], 16, T(0.6, 1.8, 0.0))
    cyl(K, M['wood'], (0.6, 1.8, 1.05), (0.6, 1.8, 1.08), 0.3, seg=20, bev=0.006)
    K.finish()


def _garment(K, M, tf, mat, rng, kind='shirt'):
    """Kleidungsstueck auf Buegel (Buegel oben bei z = 0, haengt nach unten, flach in der yz-Ebene)."""
    hk = [Vector((0, 0, 0.0)), Vector((0, -0.01, 0.035)), Vector((0, 0.012, 0.05))]
    tube(K, M['chrome'], [tf @ p for p in hk], 0.0025, seg=4)
    hb = [Vector((0, -0.2, -0.07)), Vector((0, 0, 0.0)), Vector((0, 0.2, -0.07))]
    tube(K, M['wood'] if rng.random() < 0.5 else M['black_pl'], [tf @ p for p in hb], 0.006, seg=5)
    L = 0.75 if kind == 'shirt' else (1.0 if kind == 'coat' else 0.6)
    w = 0.22
    rings = []
    for k in range(6):
        t = k / 5
        z = -0.06 - t * L
        hw = (0.2 - 0.02 * t) if k > 0 else 0.15
        if kind == 'dress':
            hw = 0.14 + 0.12 * t
        th = 0.03 + 0.02 * math.sin(t * 3)
        rings.append([tf @ Vector((x * th, y * hw, z + (0.012 * math.sin(y * 9 + k) if k else 0))) for (x, y) in
                      [(-1, -1), (1, -1), (1, 1), (-1, 1)]])
    del w
    loft(K, mat, rings[::-1], recalc=True)
    if kind != 'dress':  # Aermel
        for s in (-1, 1):
            p0 = tf @ Vector((0, s * 0.19, -0.1))
            p1 = tf @ Vector((0.01, s * 0.24, -0.1 - L * 0.55))
            tube(K, mat, [p0, (p0 + p1) / 2 + Vector((0, 0, 0)), p1], 0.035, seg=6)


def build_storefront_c():
    """Mode: Kleiderstangen mit Buegeln und Kleidungsstuecken, Regal mit gefalteten Stapeln, Schuhe, Spiegel."""
    M = _m()
    K = Kit('laden_c')
    rng = random.Random(3)
    _storefront_base(K, M, 'MODE', (0.92, 0.96, 1.0), 3)
    cols = ['fabric_r', 'fabric_b', 'fabric_g', 'fabric_k', 'fabric_gr', 'fabric_o', 'white_dull']
    for (y0, y1) in ((-2.7, -1.3),):
        tube(K, M['chrome'], [(0.3, y0, 1.75), (0.3, y1, 1.75)], 0.014, seg=10)
        for y in (y0, y1):
            cyl(K, M['chrome'], (0.02, y, 1.75), (0.3, y, 1.75), 0.012, seg=8)
            box(K, M['chrome'], (0.02, y - 0.03, 1.72), (0.03, y + 0.03, 1.78))
        y = y0 + 0.1
        while y < y1 - 0.05:
            _garment(K, M, T(0.3, y, 1.75) @ R(math.pi / 2 + rng.uniform(-0.25, 0.25), 'Z'), M[rng.choice(cols)], rng,
                     rng.choice(['shirt', 'coat', 'dress']))
            y += rng.uniform(0.1, 0.16)
    _shelf_unit(K, M, -1.15, 0.45, [0.4, 0.8, 1.2, 1.6], x1=0.4)
    for z in (0.42, 0.82, 1.22, 1.62):
        y = -1.1
        while y < 0.35:
            h = rng.uniform(0.06, 0.18)
            c = M[rng.choice(cols)]
            for j in range(int(h / 0.03)):
                box(K, c, (0.06, y, z + j * 0.03), (0.36, y + 0.26, z + j * 0.03 + 0.028), bev=0.008)
            y += 0.3
    for k in range(3):  # Schuhe
        y = -0.9 + k * 0.45
        for s in (0, 0.11):
            box(K, M[rng.choice(['fabric_k', 'black_pl', 'card_dark'])], (0.08, y + s, 0.02), (0.36, y + s + 0.09, 0.1), bev=0.03, seg=2)
    box(K, M['wood'], (0.02, 1.0, 0.1), (0.05, 1.6, 2.1), bev=0.006)
    box(K, M['chrome'], (0.05, 1.04, 0.15), (0.052, 1.56, 2.05))
    K.finish()


def build_storefront_closed():
    """Geschlossener Laden: Rollgitter herab (Lamellen mit Lochreihen, Endleiste mit Griffmulden und Vorhaengeschloss),
    Zettel GESCHLOSSEN mit Klebeband, dahinter dunkel."""
    M = _m()
    K = Kit('laden_zu')
    rng = random.Random(4)
    _storefront_base(K, M, '', (0.6, 0.6, 0.6), 4, closed=True)
    sx = 0.925
    n = 38
    zt = 3.12
    for k in range(n):
        z0 = k * (zt - 0.12) / n + 0.12
        z1 = z0 + (zt - 0.12) / n - 0.006
        pr = [(z0, sx - 0.006), (z0, sx + 0.004), ((z0 + z1) / 2, sx + 0.009), (z1, sx + 0.004), (z1, sx - 0.006)]
        sweep(K, M['galv'], [(sx, -2.81, 0), (sx, 2.81, 0)], [(z, x - sx) for (z, x) in pr], up=(0, 0, 1))
        if k % 2 == 0:
            for j in range(28):
                y = -2.7 + j * 0.2
                plate(K, M['gap'], (sx + 0.0095, y, (z0 + z1) / 2), (0, 1, 0), (0, 0, 1), 0.08, 0.012, n_off=0.0)
    box(K, M['alu_dark'], (sx - 0.012, -2.81, 0.0), (sx + 0.02, 2.81, 0.12), bev=0.006)
    for y in (-1.6, 1.6):
        box(K, M['gap'], (sx + 0.0205, y - 0.08, 0.04), (sx + 0.021, y + 0.08, 0.08))
    lathe(K, M['brass'], [(0.0, 0.0), (0.03, 0.0), (0.03, 0.05), (0.0, 0.05)], 10, T(sx + 0.035, 0.0, 0.03) @ R(math.pi / 2, 'Y') @ S(1, 0.6, 1))
    tube(K, M['chrome'], [(sx + 0.03, -0.02, 0.07), (sx + 0.03, -0.02, 0.11), (sx + 0.03, 0.02, 0.11), (sx + 0.03, 0.02, 0.07)], 0.004, seg=5)
    tfp = T(sx + 0.0125, 0.9, 1.45) @ R(0.03, 'X')
    box(K, M['paper'], (0.0, -0.15, -0.21), (0.001, 0.15, 0.21), tf=tfp)
    text('zu', 'GESCHLOSSEN', 0.035, M['black'], tfp @ T(0.0012, 0.0, 0.06))
    text('zu2', 'BIS AUF WEITERES', 0.02, M['black'], tfp @ T(0.0012, 0.0, -0.02))
    for (a, b) in ((-0.15, 0.21), (0.15, 0.21), (-0.15, -0.21), (0.15, -0.21)):
        plate(K, M['tape'], tfp @ Vector((0.0015, a, b)), tfp.to_3x3() @ Vector((0, 1, 0)), tfp.to_3x3() @ Vector((0, 0.7, 0.7)), 0.07, 0.02, n_off=0.0)
    for i in range(6):
        blob(K, M['dirt'], (sx + 0.0098, rng.uniform(-2.6, 2.6), rng.uniform(0.15, 0.6)), (0, 1, 0), (0, 0, 1), rng.uniform(0.2, 0.6),
             rng.uniform(0.05, 0.2), rng, n_off=0.0)
    K.finish()


# ======================================================================================================================
# Gates
# ======================================================================================================================

def _seat_shell(K, M, y, rng, pad, worn=False, torn=False):
    """Sitzschale aus Lochblech (Seitenprofil gebogen), Polster optional. Sitz blickt nach +x, Mitte bei y."""
    hw = 0.245
    prof = [(0.25, 0.405), (0.235, 0.44), (0.18, 0.452), (-0.12, 0.45), (-0.2, 0.47), (-0.255, 0.55), (-0.3, 0.75), (-0.34, 0.93)]
    n = len(prof)
    t = 0.004
    pts = [Vector((x, 0, z)) for (x, z) in prof]
    rings = []
    for (x, z) in prof:
        rings.append((x, z))
    # Blech als duennes Band: oben und unten
    vv, ff = [], []
    for s in (-1, 1):
        for (x, z) in prof:
            vv.append((x, y + s * hw, z))
    for (x, z) in prof:
        pass
    top = [(x, y - hw, z) for (x, z) in prof] + [(x, y + hw, z) for (x, z) in prof]
    bot = [(x - 0.0, y - hw, z - t) for (x, z) in prof] + [(x, y + hw, z - t) for (x, z) in prof]
    allv = top + bot
    N2 = 2 * n
    f = []
    for i in range(n - 1):
        f.append((i, i + 1, n + i + 1, n + i)[::-1])
        f.append((N2 + i, N2 + n + i, N2 + n + i + 1, N2 + i + 1)[::-1])
        f.append((i, N2 + i, N2 + i + 1, i + 1)[::-1])
        f.append((n + i, n + i + 1, N2 + n + i + 1, N2 + n + i)[::-1])
    f.append((0, n, N2 + n, N2)[::-1])
    f.append((n - 1, N2 + n - 1, N2 + n + n - 1, n + n - 1)[::-1])
    poly(K, M['steel_dull'], allv, f, recalc=True)
    del pts, rings, vv, ff
    # Lochung auf Sitz und Lehne
    for (a, b, cols) in ((2, 3, 9), (5, 6, 6), (6, 7, 5)):
        p0, p1 = Vector((prof[a][0], 0, prof[a][1])), Vector((prof[b][0], 0, prof[b][1]))
        d = (p1 - p0)
        L = d.length
        dn = d.normalized()
        nrm = Vector((-dn.z, 0, dn.x))
        if nrm.z < 0 and a < 4:
            nrm = -nrm
        if a >= 4 and nrm.x < 0:
            nrm = -nrm
        rows = max(2, int(L / 0.03))
        for i in range(rows):
            c = p0 + dn * (L * (i + 0.5) / rows) + nrm * 0.0007
            for j in range(-cols, cols + 1):
                cc = c + Vector((0, y + j * 0.026 + (0.013 if i % 2 else 0.0), 0))
                if abs(j * 0.026) > hw - 0.03:
                    continue
                ring = [cc + Vector((0, math.cos(q) * 0.0055, 0)) + dn * (math.sin(q) * 0.0055) for q in [k * math.pi / 3 for k in range(6)]]
                fs = poly(K, M['gap'], [tuple(v) for v in ring], [tuple(range(6))])
                if fs and fs[0].normal.dot(nrm) < 0:
                    fs[0].normal_flip()
    # Halter zur Traverse
    box(K, M['cast'], (-0.05, y - 0.12, 0.33), (0.05, y + 0.12, 0.4), bev=0.01)
    box(K, M['cast'], (-0.27, y - 0.03, 0.4), (-0.2, y + 0.03, 0.62), bev=0.008)
    if pad:
        c = M['seat_worn'] if worn else M['seat']
        box(K, c, (-0.17, y - hw + 0.015, 0.452), (0.22, y + hw - 0.015, 0.49), bev=0.016, seg=2)
        tfb = T(-0.255, y, 0.55) @ R(math.atan2(-0.085, 0.38), 'Y')
        box(K, c, (-0.012, -hw + 0.015, 0.02), (0.022, hw - 0.015, 0.4), tf=tfb, bev=0.014, seg=2)
        if torn:
            blob(K, M['white_dull'], (0.05, y + 0.05, 0.4905), (1, 0, 0), (0, 1, 0), 0.08, 0.05, rng, n_off=0.0)
        if worn:
            blob(K, M['seat_worn'], (0.02, y, 0.4908), (1, 0, 0), (0, 1, 0), 0.2, 0.25, rng, n_off=0.0)


def _seats_row(v=0):
    """Sitzreihe am Gate (Boden, 4 Sitze auf Traverse, Sitzflaeche nach +x, Tischchen am Ende bei -y): Traverse aus
    Rechteckrohr mit Endkappen, zwei Alu-Guss-Fuesse mit Gleitern, Sitzschalen aus Lochblech, Polster (Stoff blau,
    teils abgewetzt), Armlehnen aus Guss mit PU-Auflage, Tischplatte mit Kante auf Kragarm (Platte genau 0,65 m)."""
    M = _m()
    K = Kit('sitze')
    rng = random.Random(3630 + v)
    box(K, M['steel_dull'], (-0.05, -1.88, 0.30), (0.05, 1.28, 0.38), bev=0.006)
    for y in (-1.88, 1.28):
        box(K, M['black_pl'], (-0.052, y - 0.006 if y > 0 else y - 0.002, 0.298), (0.052, y + 0.002 if y < 0 else y + 0.006, 0.382), bev=0.004)
    for y in (-1.05, 1.0):  # Fuesse
        sweep(K, M['cast'], [(0, y, 0.30), (0, y, 0.06)], rrect(0.07, 0.05, 0.012, 2), up=(1, 0, 0))
        box(K, M['cast'], (-0.30, y - 0.03, 0.02), (0.30, y + 0.03, 0.08), bev=0.012, seg=2)
        for x in (-0.28, 0.28):
            cyl(K, M['black_pl'], (x, y, 0.0), (x, y, 0.02), 0.025, seg=10)
    pads = {0: [1, 1, 1, 1], 1: [1, 0, 1, 1], 2: [1, 1, 0, 0]}[v]
    worn = {0: [0, 1, 0, 0], 1: [1, 0, 0, 1], 2: [0, 1, 0, 0]}[v]
    for k in range(4):
        y = -0.9 + k * 0.6
        _seat_shell(K, M, y, rng, pads[k], worn[k], torn=(v == 2 and k == 1))
    for k, y in enumerate((-1.2, -0.6, 0.0, 0.6, 1.2)):  # Armlehnen
        sweep(K, M['cast'], fillet([(-0.02, y, 0.38), (-0.06, y, 0.58), (-0.2, y, 0.63)], 0.06, 3), rrect(0.035, 0.04, 0.01, 2), up=(0, 1, 0))
        box(K, M['black_pl'], (-0.24, y - 0.028, 0.62), (0.16, y + 0.028, 0.655), bev=0.012, seg=2)
        if rng.random() < 0.5:
            blob(K, M['scuff'], (0.0, y, 0.6555), (1, 0, 0), (0, 1, 0), 0.12, 0.03, rng, n_off=0.0)
    # Tisch auf Kragarm
    box(K, M['cast'], (-0.05, -1.75, 0.38), (0.05, -1.45, 0.6), bev=0.01)
    box(K, M['anthr'], (-0.25, -1.85, 0.615), (0.25, -1.35, 0.635), bev=0.004)
    box(K, M['wood'], (-0.255, -1.855, 0.635), (0.255, -1.345, 0.65), bev=0.004)
    if v == 1:  # liegengebliebene Zeitung auf einem Sitz
        box(K, M['paper'], (-0.1, -0.45, 0.455), (0.2, -0.12, 0.468), tf=None, bev=0.003)
    if v == 2:  # Jacke ueber der Lehne
        tube(K, M['fabric_gr'], fillet([(0.2, 0.15, 0.47), (-0.1, 0.25, 0.48), (-0.3, 0.3, 0.8), (-0.38, 0.32, 0.92), (-0.42, 0.35, 0.7)], 0.08, 3), 0.05, seg=7)
    K.finish()


def build_seats_row():
    _seats_row(0)


def build_seats_row__v1():
    _seats_row(1)


def build_seats_row__v2():
    _seats_row(2)


def build_gate_counter():
    """Gate-Schalter (Boden, 2 m entlang y, Fluggast bei -x): Pult mit schraeger Front und Alu-Band, Sockel, Ablage,
    Arbeitsplatte, Bildschirm (zum Personal), Bordkartenleser mit rotem Fenster, Mikrofon am Schwanenhals fuer die
    Durchsagen, Telefon, Papierstapel."""
    M = _m()
    K = Kit('gate')
    rng = random.Random(3640)
    pts = rrect(0.66, 2.0, 0.12, 3)
    prism(K, M['anthr'], [(x * 0.95, y * 0.98) for (x, y) in pts], 0.0, 0.08)
    prism(K, M['white'], pts, 0.08, 0.98, bev=0.005)
    box(K, M['alu'], (-0.335, -0.9, 0.6), (-0.329, 0.9, 0.63), bev=0.002)
    box(K, M['screen_dim'], (-0.336, -0.6, 0.66), (-0.33, 0.6, 0.86))
    text('gate_front', 'BOARDING', 0.08, M['white_glow'], T(-0.337, 0, 0.76) @ FACE['-x'])
    prism(K, M['white_pl'], rrect(0.82, 2.1, 0.14, 3), 0.98, 1.02, bev=0.01, seg=2)
    box(K, M['white_pl'], (-0.41, -1.0, 1.02), (-0.2, 1.0, 1.1), bev=0.01, seg=2)
    _monitor(K, M, T(0.05, 0.25, 1.02), w=0.5, h=0.3)
    _keyboard(K, M, T(0.2, 0.25, 1.02) @ R(math.pi, 'Z'))
    box(K, M['black_pl'], (-0.15, -0.75, 1.02), (0.05, -0.5, 1.1), bev=0.012, seg=2)  # Bordkartenleser
    box(K, M['red_glow'], (-0.12, -0.7, 1.1), (0.02, -0.55, 1.101))
    lathe(K, M['black_pl'], [(0.0, 0.0), (0.06, 0.0), (0.055, 0.012), (0.0, 0.016)], 12, T(0.1, -0.35, 1.02))
    tube(K, M['black_pl'], fillet([(0.1, -0.35, 1.03), (0.1, -0.35, 1.2), (0.0, -0.33, 1.3), (-0.08, -0.3, 1.33)], 0.06, 4), 0.006, seg=6)
    lathe(K, M['black_pl'], [(0.0, 0.0), (0.012, 0.0), (0.014, 0.04), (0.0, 0.05)], 8, T(-0.08, -0.3, 1.33) @ R(-1.9, 'Y'))
    box(K, M['grey_pl'], (0.1, 0.65, 1.02), (0.3, 0.85, 1.07), bev=0.01)  # Telefon
    box(K, M['grey_pl'], (0.12, 0.66, 1.07), (0.18, 0.84, 1.1), bev=0.012, seg=2)
    box(K, M['paper'], (0.05, -0.95, 1.02), (0.26, -0.8, 1.04))
    for i in range(3):
        blob(K, M['rubber_mark'], (-0.3305, rng.uniform(-0.8, 0.8), rng.uniform(0.1, 0.3)), (0, -1, 0), (0, 0, 1), rng.uniform(0.05, 0.12),
             0.03, rng, n_off=0.0005)
    K.finish()


def _gate_sign(number):
    """Gate-Schild (Wandteil, 2 x 0,8 m): Gehaeuse mit runden Kanten auf Wandwinkeln, Alu-Rahmen, gelbes Feld mit
    Flugzeug und GATE, Nummer gelb auf Anthrazit, Schrauben."""
    M = _m()
    K = Kit('gateschild')
    for y in (-0.8, 0.8):
        box(K, M['alu'], (0.0, y - 0.03, -0.3), (0.015, y + 0.03, 0.3), bev=0.002)
    sweep(K, M['anthr'], [(0.015, 0, 0), (0.095, 0, 0)], rrect(2.0, 0.8, 0.035, 3), up=(0, 1, 0))
    sweep(K, M['alu'], [(0.095, 0, 0), (0.105, 0, 0)], rrect(2.0, 0.8, 0.035, 3), up=(0, 1, 0))
    tf = T(0.1052, 0, 0)
    _face_poly(K, M['anthr_gloss'], tf, rrect(1.92, 0.72, 0.02, 2))
    _face_poly(K, M['yellow'], tf, [(-0.95, -0.35), (-0.15, -0.35), (-0.15, 0.35), (-0.95, 0.35)], 0.0006)
    _plane_icon(K, M['black'], tf, -0.55, 0.13, 1.3, 0.0012, rot=-math.pi / 2)
    text('gate', 'GATE', 0.15, M['black'], tf @ T(0.0012, -0.55, -0.15))
    text('nummer', number, 0.42, M['yellow'], tf @ T(0.0012, 0.45, 0.0))
    for (y, z) in ((-0.96, -0.36), (0.96, -0.36), (-0.96, 0.36), (0.96, 0.36)):
        screw(K, M['steel_dull'], (0.105, y, z), (1, 0, 0), r=0.006)
    K.finish()


def build_gate_sign_c30():
    _gate_sign('C30')


def build_gate_sign_c31():
    _gate_sign('C31')


def build_gate_sign_c32():
    _gate_sign('C32')


def build_gate_sign_c33():
    _gate_sign('C33')


def build_gate_sign_c34():
    _gate_sign('C34')


def _gate_door(boarding):
    """Tuer zur Fluggastbruecke (Wandteil, Boden z = 0): Stahlzarge mit Falz und Dichtung, Tuerblatt mit
    Drahtglasfenster, Panikstange, Obentuerschliesser, Baender, Trittblech, Kartenleser, Statusleuchte mit Gehaeuse,
    Schild. boarding: Tuer offen (zur Bruecke), dahinter das helle Innere, gruene Leuchte (Wiki: jede Gate-Tuer fuehrt
    in Sublevel 36.1, das Flugzeug)."""
    M = _m()
    K = Kit('gatetuer')
    W, H = 1.6, 2.4
    zp = [(0.0, -W / 2 - 0.15), (0.12, -W / 2 - 0.15), (0.12, -W / 2 - 0.02), (0.07, -W / 2 - 0.02), (0.07, -W / 2), (0.0, -W / 2)]
    for s in (-1, 1):
        prism(K, M['grey'], [(x, s * y) for (x, y) in zp], 0.0, H + 0.15)
    box(K, M['grey'], (0.0, -W / 2 - 0.15, H), (0.12, W / 2 + 0.15, H + 0.15), bev=0.004)
    box(K, M['grey'], (0.07, -W / 2 - 0.02, H - 0.02), (0.12, W / 2 + 0.02, H))
    for s in (-1, 1):
        box(K, M['rubber'], (0.065, s * (W / 2) - 0.004, 0.0), (0.07, s * (W / 2) + 0.004, H))
    if boarding:
        inside = common.material('bruecke_licht', (0.95, 0.97, 1.0), look='glow', glow=1.6)
        box(K, inside, (0.0, -W / 2, 0.0), (0.01, W / 2, H))
        box(K, common.material('bruecke_boden', (0.55, 0.57, 0.6), look='glow', glow=0.5), (0.0105, -W / 2, 0.0), (0.011, W / 2, 0.35))
        box(K, M['steel'], (0.0, -W / 2, -0.002), (0.13, W / 2, 0.012), bev=0.003)
        for s in (-1, 1):  # Kanten der offenen Fluegel in der Laibung
            box(K, M['anthr'], (0.0, s * (W / 2 - 0.02) - 0.02, 0.02), (0.06, s * (W / 2 - 0.02) + 0.02, H - 0.03), bev=0.004)
        lamp = common.material('status_gruen', (0.3, 1.0, 0.45), look='glow', glow=2.0)
    else:
        box(K, M['anthr'], (0.02, -W / 2 + 0.005, 0.01), (0.068, W / 2 - 0.005, H - 0.02), bev=0.004)
        box(K, M['gap'], (0.068, -0.002, 0.02), (0.0685, 0.002, H - 0.03))
        for s in (-1, 1):
            yc = s * 0.4
            box(K, M['grey'], (0.068, yc - 0.17, 1.25), (0.075, yc + 0.17, 1.95), bev=0.004)
            box(K, M['glass'], (0.07, yc - 0.15, 1.27), (0.0755, yc + 0.15, 1.93))
            for j in range(6):
                box(K, M['steel_dull'], (0.0756, yc - 0.15, 1.3 + j * 0.11), (0.0758, yc + 0.15, 1.302 + j * 0.11))
                box(K, M['steel_dull'], (0.0756, yc - 0.15 + j * 0.06, 1.27), (0.0758, yc - 0.148 + j * 0.06, 1.93))
            box(K, M['steel'], (0.068, yc - 0.35, 0.01), (0.071, yc + 0.35, 0.3), bev=0.001)
        for s in (-1, 1):  # Panikstange
            yc = s * 0.4
            for yy in (yc - 0.3, yc + 0.3):
                box(K, M['steel'], (0.068, yy - 0.03, 0.97), (0.12, yy + 0.03, 1.07), bev=0.008)
            cyl(K, M['steel'], (0.11, yc - 0.3, 1.0), (0.11, yc + 0.3, 1.0), 0.018, seg=10)
        for s in (-1, 1):  # Baender
            for z in (0.25, 1.2, 2.1):
                cyl(K, M['steel_dull'], (0.075, s * (W / 2 - 0.01), z), (0.075, s * (W / 2 - 0.01), z + 0.12), 0.012, seg=8)
        box(K, M['steel_dull'], (0.07, -0.55, H - 0.14), (0.13, -0.05, H - 0.06), bev=0.012, seg=2)
        box(K, M['steel_dull'], (0.12, -0.3, H - 0.1), (0.135, 0.15, H - 0.08), bev=0.004)
        lamp = common.material('status_rot', (1.0, 0.2, 0.15), look='glow', glow=1.6)
    # Kartenleser, Statusleuchte, Schild
    box(K, M['black_pl'], (0.12, W / 2 + 0.03, 1.1), (0.15, W / 2 + 0.12, 1.25), bev=0.008)
    box(K, M['red_glow'] if not boarding else M['green_glow'], (0.15, W / 2 + 0.07, 1.22), (0.151, W / 2 + 0.09, 1.235))
    box(K, M['grey_pl'], (0.12, W / 2 + 0.045, 1.13), (0.1505, W / 2 + 0.105, 1.18))
    box(K, M['anthr'], (0.12, -0.3, H + 0.18), (0.16, 0.3, H + 0.34), bev=0.01, seg=2)
    box(K, lamp, (0.16, -0.26, H + 0.2), (0.1605, 0.26, H + 0.32))
    text('tuer', 'NUR FUER FLUGGAESTE MIT BORDKARTE', 0.022, M['white_dull'], T(0.1206, 0.0, H + 0.075))
    K.finish()


def build_gate_door_closed():
    _gate_door(False)


def build_gate_door_boarding():
    _gate_door(True)


def _clock(hands):
    """Bahnhofs-Wanduhr (Wandteil, 64 cm): Gehaeuse als Drehkoerper mit Wandteller und Wulst, Zifferblatt mit Stunden-
    und Minutenstrichen, Mittelachse; hands: Stunden- und Minutenzeiger, roter Sekundenzeiger mit Kelle (stehen still)
    - sonst fehlen die Zeiger (Wiki: Uhren stehen still, zeigen zufaellige Zeiten oder haben keine Zeiger)."""
    M = _m()
    K = Kit('uhr')
    ax = R(math.pi / 2, 'Y')  # Drehachse z -> x
    lathe(K, M['anthr'], [(0.0, 0.0), (0.30, 0.0), (0.315, 0.012), (0.32, 0.04), (0.318, 0.068), (0.305, 0.082), (0.288, 0.084),
                          (0.285, 0.07), (0.0, 0.07)], 48, ax)
    lathe(K, M['white_dull'], [(0.0, 0.0), (0.286, 0.0), (0.286, 0.002), (0.0, 0.002)], 48, T(0.07, 0, 0) @ ax)
    o = 0.0725
    for k in range(60):
        a = k * math.pi / 30
        big = k % 5 == 0
        L, w = (0.06, 0.012) if big else (0.018, 0.0035)
        c, s = math.cos(a), math.sin(a)
        r0 = 0.268 - L
        pts = [(c * r0 - s * w, s * r0 + c * w), (c * r0 + s * w, s * r0 - c * w), (c * 0.268 + s * w, s * 0.268 - c * w),
               (c * 0.268 - s * w, s * 0.268 + c * w)]
        _face_poly(K, M['black'], Matrix.Identity(4), [(y, z) for (y, z) in pts], o)
    lathe(K, M['black'], [(0.0, 0.0), (0.012, 0.0), (0.012, 0.006), (0.004, 0.012), (0.0, 0.012)], 12, T(o, 0, 0) @ ax)
    if hands:
        for (ln, back, w0, w1, a, mat, dz) in ((0.17, 0.05, 0.016, 0.011, 2.3, 'black', 0.003), (0.25, 0.06, 0.012, 0.007, -0.9, 'black', 0.006)):
            c, s = math.cos(a), math.sin(a)
            loc = [(-w0, -back), (w0, -back), (w1, ln), (-w1, ln)]
            pts = [(c * u - s * v, s * u + c * v) for (u, v) in loc]
            _face_poly(K, M[mat], Matrix.Identity(4), pts, o + dz)
        a = 0.6
        c, s = math.cos(a), math.sin(a)
        loc = [(-0.0018, -0.07), (0.0018, -0.07), (0.0018, 0.17), (-0.0018, 0.17)]
        _face_poly(K, M['red_pl'], Matrix.Identity(4), [(c * u - s * v, s * u + c * v) for (u, v) in loc], o + 0.009)
        _face_poly(K, M['red_pl'], Matrix.Identity(4), [(c * u - s * v, s * u + c * v) for (u, v) in circle(0.022, 14, 0.0, 0.17)], o + 0.009)
    K.finish()


def build_clock_36():
    _clock(False)


def build_clock_36_b():
    _clock(True)


def build_water_fountain():
    """Trinkbrunnen (Wandteil, Edelstahl): Rueckplatte, Becken mit gerundetem Rand und Mulde, Ablaufsieb mit Schlitzen,
    Sprudler mit Schutzbuegel, Druckknopf vorn, Verkleidung mit Lueftungsschlitzen bis zum Boden, Kalkrand, Tropfen."""
    M = _m()
    K = Kit('brunnen')
    rng = random.Random(3650)
    box(K, M['steel'], (0.0, -0.26, 0.55), (0.012, 0.26, 1.0), bev=0.003)
    for (y, z) in ((-0.22, 0.6), (0.22, 0.6), (-0.22, 0.95), (0.22, 0.95)):
        screw(K, M['steel_dull'], (0.012, y, z), (1, 0, 0), r=0.005)
    # Becken: Ringe nach Umriss (abgerundetes Rechteck), aussen hoch, Rand, innen die Mulde
    def ring(w, d, r, z, xo=0.012):
        return [(xo + d / 2 + a, b, z) for (a, b) in rrect(d, w, r, 3)]
    rings = [ring(0.56, 0.42, 0.1, 0.76), ring(0.58, 0.43, 0.11, 0.9), ring(0.585, 0.435, 0.112, 0.93), ring(0.57, 0.425, 0.106, 0.945),
             ring(0.5, 0.36, 0.08, 0.94), ring(0.42, 0.3, 0.06, 0.9), ring(0.16, 0.12, 0.05, 0.875)]
    loft(K, M['steel'], rings, cap0=True, cap1=True, recalc=False)
    lathe(K, M['steel_dull'], [(0.0, 0.0), (0.035, 0.0), (0.035, 0.003), (0.0, 0.003)], 16, T(0.072, 0, 0.874))
    for k in range(5):
        plate(K, M['gap'], (0.072, -0.02 + k * 0.01, 0.8775), (1, 0, 0), (0, 1, 0), 0.04, 0.003, n_off=0.0)
    lathe(K, M['chrome'], [(0.0, 0.0), (0.02, 0.0), (0.016, 0.04), (0.01, 0.07), (0.006, 0.075), (0.0, 0.075)], 12, T(0.12, 0.08, 0.9))
    tube(K, M['chrome'], fillet([(0.08, 0.04, 0.92), (0.08, 0.04, 0.99), (0.17, 0.08, 0.99), (0.17, 0.12, 0.92)], 0.03, 3), 0.005, seg=6)
    lathe(K, M['steel'], [(0.0, 0.0), (0.03, 0.0), (0.03, 0.012), (0.022, 0.02), (0.0, 0.02)], 16, T(0.443, 0.0, 0.85) @ R(math.pi / 2, 'Y'))
    # Verkleidung
    box(K, M['steel_dull'], (0.0, -0.17, 0.0), (0.22, 0.17, 0.77), bev=0.006)
    for k in range(10):
        plate(K, M['gap'], (0.2201, 0.0, 0.2 + k * 0.03), (0, 1, 0), (0, 0, 1), 0.2, 0.008, n_off=0.0)
    box(K, M['black_pl'], (0.0, -0.165, 0.0), (0.215, 0.165, 0.05))
    # Kalk, Wasser
    blob(K, M['white_dull'], (0.075, 0.0, 0.8766), (1, 0, 0), (0, 1, 0), 0.12, 0.14, rng, n_off=0.0003)
    blob(K, common.material('wasser', (0.4, 0.45, 0.5), look='water', rough=0.02), (0.09, 0.03, 0.8772), (1, 0, 0), (0, 1, 0), 0.05, 0.07, rng, n_off=0.0006)
    K.finish()


MAN = [(-0.035, -0.18), (-0.012, -0.18), (-0.01, -0.06), (0.01, -0.06), (0.012, -0.18), (0.035, -0.18), (0.035, -0.04), (0.06, -0.04),
       (0.06, 0.055), (0.04, 0.075), (-0.04, 0.075), (-0.06, 0.055), (-0.06, -0.04), (-0.035, -0.04)]
WOMAN = [(-0.03, -0.18), (-0.01, -0.18), (-0.01, -0.09), (0.01, -0.09), (0.01, -0.18), (0.03, -0.18), (0.03, -0.09), (0.075, -0.09),
         (0.045, 0.055), (0.03, 0.075), (-0.03, 0.075), (-0.045, 0.055), (-0.075, -0.09), (-0.03, -0.09)]


def build_restroom_sign():
    """Toilettenschild (Wandteil): Platte mit runden Kanten auf Abstandshaltern, weisse Bildzeichen Mann und Frau,
    Trennstrich, WC und Pfeil."""
    M = _m()
    K = Kit('wc')
    for (y, z) in ((-0.4, -0.2), (0.4, -0.2), (-0.4, 0.2), (0.4, 0.2)):
        cyl(K, M['steel'], (0.0, y, z), (0.014, y, z), 0.012, seg=10)
    sweep(K, M['anthr'], [(0.014, 0, 0), (0.05, 0, 0)], rrect(0.9, 0.5, 0.04, 3), up=(0, 1, 0))
    for (y, z) in ((-0.4, -0.2), (0.4, -0.2), (-0.4, 0.2), (0.4, 0.2)):
        lathe(K, M['steel'], [(0.0, 0.0), (0.014, 0.0), (0.014, 0.006), (0.0, 0.009)], 10, T(0.05, y, z) @ R(math.pi / 2, 'Y'))
    tf = T(0.0502, 0, 0.0)
    _face_poly(K, M['white_glow'], tf, [(-0.25 + a, 0.03 + b) for (a, b) in MAN])
    _face_poly(K, M['white_glow'], tf, circle(0.036, 14, -0.25, 0.155))
    _face_poly(K, M['white_glow'], tf, [(-0.05 + a, 0.03 + b) for (a, b) in WOMAN])
    _face_poly(K, M['white_glow'], tf, circle(0.036, 14, -0.05, 0.155))
    _face_poly(K, M['white_glow'], tf, [(-0.153, -0.17), (-0.147, -0.17), (-0.147, 0.2), (-0.153, 0.2)])
    text('wc', 'WC', 0.12, M['white_glow'], tf @ T(0.0, 0.17, 0.05))
    _arrow(K, M['white_glow'], tf, 0.17, -0.12, 1, 0.8)
    K.finish()


# ======================================================================================================================
# Gate C32: Habseligkeiten der Wartenden
# ======================================================================================================================

def blanket(K, mat, tf, w, d, rng, n=(16, 12), amp=0.07, folds=4, lift=None):
    """Decke auf dem Boden: Hoehenfeld mit Falten und Knautschungen, duenn aufgedickt."""
    nx, ny = n
    vv, ff = [], []
    waves = [(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(4, 9), rng.uniform(0, 6.28), rng.uniform(0.3, 1.0)) for _ in range(folds)]
    for j in range(ny + 1):
        for i in range(nx + 1):
            x = -w / 2 + w * i / nx
            y = -d / 2 + d * j / ny
            z = 0.006
            for (a, b, k, ph, A) in waves:
                L = math.hypot(a, b) or 1.0
                t = (x * a + y * b) / L * k + ph
                z += amp * A * max(0.0, math.cos(t)) ** 3 / folds * 2.0
            if lift:
                z += lift(x, y)
            z += rng.uniform(-0.004, 0.004)
            vv.append(tf @ Vector((x + rng.uniform(-0.01, 0.01), y + rng.uniform(-0.01, 0.01), max(0.004, z))))
    # Oberseite, Unterseite (8 mm tiefer, gegenlaeufig) und Rand
    nv = (nx + 1) * (ny + 1)
    vv = vv + [v - Vector((0, 0, 0.008)) if v.z > 0.012 else Vector((v.x, v.y, 0.0005)) for v in vv]
    for j in range(ny):
        for i in range(nx):
            a = j * (nx + 1) + i
            ff.append((a, a + 1, a + nx + 2, a + nx + 1))
            ff.append((nv + a + nx + 1, nv + a + nx + 2, nv + a + 1, nv + a))
    ring = [i for i in range(nx)] + [i * (nx + 1) + nx for i in range(ny)] + \
           [ny * (nx + 1) + nx - i for i in range(nx)] + [(ny - i) * (nx + 1) for i in range(ny)]
    for k in range(len(ring)):
        a, b = ring[k], ring[(k + 1) % len(ring)]
        ff.append((a, nv + a, nv + b, b))
    poly(K, mat, [tuple(v) for v in vv], ff)


def _cup(K, M, tf, lid=True, sleeve=True):
    lathe(K, M['white_pl'], [(0.0, 0.0), (0.028, 0.0), (0.04, 0.115), (0.042, 0.118), (0.0, 0.118)], 14, tf)
    if sleeve:
        lathe(K, M['card'], [(0.0, 0.03), (0.0335, 0.03), (0.0385, 0.088), (0.0, 0.088)], 14, tf)
    if lid:
        lathe(K, M['black_pl'], [(0.0, 0.118), (0.044, 0.118), (0.044, 0.126), (0.036, 0.132), (0.0, 0.134)], 14, tf)


def build_camp_c32():
    """Habseligkeiten der Wartenden an Gate C32 (Boden, 4,5 x 3,8 m): Decken mit Falten, Isomatten (gerollt und
    ausgebreitet mit Rillen), Kissen, Reisetasche, Rucksaecke, Koffer, Pappbecher mit Deckel und Manschette, umgefallene
    Becher, Thermoskanne, Wasserflasche, Verpackungen, Zeitung, Pappschild mit der Flugnummer (Wiki: rund 150
    Fluggaeste warten an C32 auf Flug 1784XBA8JLP nach Level 186)."""
    M = _m()
    K = Kit('camp')
    rng = random.Random(32)
    for (x, y, a, c, w, d) in ((-1.1, -0.9, 0.3, 'fabric_r', 1.7, 1.3), (0.9, -1.0, -0.4, 'fabric_g', 1.6, 1.2),
                               (0.3, 0.9, 1.2, 'fabric_b', 1.8, 1.4), (-1.3, 0.8, 2.6, 'fabric_gr', 1.5, 1.1)):
        blanket(K, M[c], T(x, y, 0.0) @ R(a, 'Z'), w, d, rng)
    # Isomatten: ausgebreitet (Rillen) und gerollt
    tfm = T(1.6, 0.3, 0.0) @ R(1.4, 'Z')
    box(K, M['fabric_o'], (-0.9, -0.27, 0.0), (0.9, 0.27, 0.012), tf=tfm, bev=0.004)
    for k in range(12):
        box(K, M['fabric_o'], (-0.85 + k * 0.15, -0.26, 0.012), (-0.8 + k * 0.15, 0.26, 0.018), tf=tfm)
    for (x, y, a) in ((-1.95, -0.2, 1.5), (2.0, -1.4, 0.2)):
        tfr = T(x, y, 0.09) @ R(a, 'Z') @ R(math.pi / 2, 'X')
        lathe(K, M['fabric_b'], [(0.0, -0.3), (0.085, -0.3), (0.09, -0.29), (0.09, 0.29), (0.085, 0.3), (0.0, 0.3)], 16, tfr)
        for s in (-1, 1):
            tube(K, M['fabric_k'], [tfr @ Vector((math.cos(q) * 0.093, math.sin(q) * 0.093, s * 0.18)) for q in [2 * math.pi * k / 12 for k in range(12)]],
                 0.006, seg=4, closed=True)
            _face_poly(K, M['gap'], tfr @ T(0, 0, s * 0.301) @ R(-s * math.pi / 2, 'Y'), circle(0.06, 10), 0.0)
    # Kissen
    fs = box(K, M['white_dull'], (-0.3, -0.2, 0.0), (0.3, 0.2, 0.12), tf=T(-1.5, 1.1, 0.04) @ R(0.4, 'Z'), bev=0.05, seg=3)
    jitter(K, M['white_dull'], 0.008, rng, fs)
    # Gepaeck
    _lug_any(K, M, T(-0.2, -1.55, 0.0) @ R(0.1, 'Z'), rng, kind='duffel')
    _lug_any(K, M, T(1.75, 1.3, 0.0) @ R(-2.0, 'Z'), rng, kind='backpack')
    _lug_any(K, M, T(-2.0, 0.6, 0.0) @ R(0.3, 'Z'), rng, kind='backpack', lying=True)
    _lug_any(K, M, T(0.6, -0.1, 0.0) @ R(0.5, 'Z'), rng, kind='soft', lying=True, size=0.9)
    _lug_any(K, M, T(-0.5, 1.6, 0.0) @ R(1.7, 'Z'), rng, kind='hard', size=0.85)
    # Becher, Thermoskanne, Flasche, Verpackungen, Zeitung
    for (x, y, up) in ((0.1, -0.5, True), (0.25, -0.45, True), (-0.7, 0.25, True), (1.2, -0.3, False), (-0.3, -0.2, False)):
        if up:
            _cup(K, M, T(x, y, 0.0))
        else:
            _cup(K, M, T(x, y, 0.042) @ R(rng.uniform(0, 6), 'Z') @ R(math.pi / 2, 'Y') @ T(0, 0, -0.06), lid=False)
            blob(K, M['card_dark'], (x + 0.12, y, 0.0015), (1, 0, 0), (0, 1, 0), 0.25, 0.15, rng, n_off=0.0)
    tft = T(0.45, -0.75, 0.0)
    lathe(K, M['steel'], [(0.0, 0.0), (0.045, 0.0), (0.048, 0.01), (0.048, 0.25), (0.04, 0.27), (0.025, 0.28), (0.0, 0.28)], 16, tft)
    lathe(K, M['black_pl'], [(0.0, 0.27), (0.03, 0.27), (0.034, 0.33), (0.0, 0.335)], 14, tft)
    box(K, M['black_pl'], (0.045, -0.01, 0.06), (0.07, 0.01, 0.22), tf=tft, bev=0.006)
    lathe(K, M['glass_frost'], [(0.0, 0.0), (0.032, 0.0), (0.034, 0.18), (0.014, 0.22), (0.014, 0.24), (0.0, 0.24)], 12,
          T(-0.9, -0.4, 0.034) @ R(1.2, 'Z') @ R(math.pi / 2, 'Y') @ T(0, 0, -0.12))
    for k in range(4):
        fs = box(K, M[rng.choice(['red_pl', 'yellow_pl', 'white_pl'])], (-0.05, -0.03, 0.0), (0.05, 0.03, 0.02),
                 tf=T(rng.uniform(-1.5, 1.5), rng.uniform(-1.2, 1.2), 0.0) @ R(rng.uniform(0, 6), 'Z'), bev=0.008)
        jitter(K, M['red_pl'], 0.0, rng, [])
    tfz = T(-0.6, -0.95, 0.008) @ R(0.7, 'Z')
    box(K, M['paper'], (-0.2, -0.15, 0.0), (0.2, 0.15, 0.012), tf=tfz, bev=0.002)
    for k in range(6):
        plate(K, M['gap'], tfz @ Vector((-0.12 + k * 0.05, 0.0, 0.0125)), tfz.to_3x3() @ Vector((1, 0, 0)), tfz.to_3x3() @ Vector((0, 1, 0)),
              0.03, 0.25, n_off=0.0)
    # Pappschild an einem Rucksack angelehnt
    tfs = T(-1.9, 0.0, 0.0) @ R(-0.25, 'Y')
    fs = box(K, M['card'], (-0.004, -0.5, 0.0), (0.004, 0.5, 0.5), tf=tfs)
    jitter(K, M['card'], 0.004, rng, fs)
    text('schild_text', '1784XBA8JLP', 0.09, M['black'], tfs @ T(0.0045, 0.0, 0.32))
    text('schild_text2', '-> 186', 0.09, M['black'], tfs @ T(0.0045, 0.0, 0.17))
    K.finish()


# ======================================================================================================================
# Sicherheitskontrolle (ausser Betrieb) und Lounge
# ======================================================================================================================

def _rollers(K, M, x0, x1, y0, y1, z, pitch=0.1, r=0.024):
    for side in (y0, y1):
        box(K, M['steel_dull'], (x0, min(side, side + (0.03 if side == y1 else -0.03)), z - 0.07),
            (x1, max(side, side + (0.03 if side == y1 else -0.03)), z + 0.01), bev=0.004)
    n = int((x1 - x0) / pitch)
    for k in range(n):
        x = x0 + pitch / 2 + k * (x1 - x0) / n
        cyl(K, M['steel'], (x, y0, z - r), (x, y1, z - r), r, seg=10)


def _xray(oy=0.5):
    """Durchleuchtungsband (Boden, 5 m entlang x, ausser Betrieb): Rollenbahn vorn, Geraet mit gerundetem Gehaeuse,
    Tunnel mit Bleivorhang an beiden Enden, Gurtband, Bedienpult mit Not-Aus und Schluesselschalter, Warnzeichen,
    Lueftungsschlitze, Absperrband AUSSER BETRIEB, Rollenbahn hinten mit Endanschlag, Ablagetisch mit Wannen,
    Monitorplatz. oy: Lage quer, damit das Modell den Kollisionsblock der Karte genau deckt."""
    M = _m()
    K = Kit('roentgen')
    rng = random.Random(3660)
    zb = 0.72
    W = 0.5
    # Rollenbahn vorn mit Beinen
    _rollers(K, M, -2.0, -0.85, oy - 0.42, oy + 0.42, zb)
    for x in (-1.95, -0.95):
        for y in (oy - 0.4, oy + 0.4):
            box(K, M['steel_dull'], (x - 0.02, y - 0.02, 0.0), (x + 0.02, y + 0.02, zb - 0.07), bev=0.003)
            cyl(K, M['black_pl'], (x, y, 0.0), (x, y, 0.015), 0.03, seg=8)
    # Geraet
    box(K, M['white_dull'], (-0.85, oy - W, 0.06), (1.2, oy + W, 1.38), bev=0.05, seg=3)
    box(K, M['black_pl'], (-0.82, oy - W + 0.03, 0.0), (1.17, oy + W - 0.03, 0.06))
    box(K, M['grey'], (-0.86, oy - W - 0.002, 0.3), (1.21, oy + W + 0.002, 0.36))
    for xe in (-0.851, 1.201):
        sgn = -1 if xe < 0 else 1
        x0, x1 = sorted((xe, xe + sgn * 0.002))
        box(K, M['gap'], (x0, oy - 0.33, zb - 0.02), (x1, oy + 0.33, zb + 0.44))
        for k in range(10):
            y = oy - 0.32 + k * 0.064
            box(K, M['rubber'], (min(xe + sgn * 0.004, xe + sgn * 0.01), y, zb + 0.005), (max(xe + sgn * 0.004, xe + sgn * 0.01), y + 0.06, zb + 0.43))
        box(K, M['rubber'], (min(xe, xe + sgn * 0.6), oy - 0.31, zb - 0.015), (max(xe, xe + sgn * 0.06), oy + 0.31, zb))
        # Absperrband diagonal
        for j in range(8):
            t0, t1 = j / 8, (j + 1) / 8
            p0 = Vector((xe + sgn * 0.012, oy - 0.36 + 0.72 * t0, zb + 0.46 - 0.44 * t0))
            p1 = Vector((xe + sgn * 0.012, oy - 0.36 + 0.72 * t1, zb + 0.46 - 0.44 * t1))
            sweep(K, M['red_pl'] if j % 2 == 0 else M['white_pl'], [p0, p1], [(-0.001, -0.025), (0.001, -0.025), (0.001, 0.025), (-0.001, 0.025)], up=(1, 0, 0))
    for k in range(14):  # Lueftungsschlitze an der Seite
        for sy in (-1, 1):
            plate(K, M['gap'], (-0.5 + k * 0.08, oy + sy * (W + 0.0005), 0.5), (sy * -1, 0, 0) if sy > 0 else (1, 0, 0), (0, 0, 1), 0.03, 0.14, n_off=0.0)
    # Warnzeichen und Typenschild
    tfw = T(0.2, oy - W - 0.0006, 1.05) @ FACE['-y']
    _face_poly(K, M['yellow_pl'], tfw, [(-0.07, -0.06), (0.07, -0.06), (0.0, 0.065)])
    _face_poly(K, M['black'], tfw, circle(0.012, 10, 0.0, -0.01), 0.0005)
    text('xr1', 'ROENTGEN', 0.04, M['black'], tfw @ T(0.0005, 0.25, 1.05 - 1.05 + 0.0))
    plate(K, M['paper'], (0.9, oy - W - 0.0006, 0.9), (1, 0, 0), (0, 0, 1), 0.3, 0.21, n_off=0.0)
    text('xr2', 'AUSSER BETRIEB', 0.03, M['red_pl'], T(0.9, oy - W - 0.0012, 0.93) @ FACE['-y'])
    # Bedienpult oben, Monitorplatz seitlich
    tfp = T(0.7, oy - W + 0.12, 1.38) @ R(0.35, 'X')
    box(K, M['grey'], (-0.2, -0.1, 0.0), (0.2, 0.1, 0.04), tf=tfp, bev=0.01)
    lathe(K, M['red_pl'], [(0.0, 0.0), (0.015, 0.0), (0.02, 0.012), (0.018, 0.02), (0.0, 0.022)], 12, tfp @ T(0.12, 0.0, 0.04))
    cyl(K, M['chrome'], tfp @ Vector((-0.1, 0.0, 0.04)), tfp @ Vector((-0.1, 0.0, 0.055)), 0.012, seg=8)
    for k in range(4):
        box(K, M['black_pl'], (-0.05 + k * 0.03, -0.03, 0.04), (-0.03 + k * 0.03, 0.03, 0.047), tf=tfp)
    box(K, M['grey'], (0.0, oy - 1.0, 0.0), (0.8, oy - 0.55, 0.74), bev=0.008)
    box(K, M['white_pl'], (-0.02, oy - 1.0, 0.74), (0.82, oy - 0.53, 0.77), bev=0.006)
    _monitor(K, M, T(0.4, oy - 0.8, 0.77) @ R(math.pi / 2, 'Z'), w=0.48, h=0.3, face='screen_off')
    _keyboard(K, M, T(0.4, oy - 0.65, 0.77) @ R(math.pi / 2, 'Z'))
    # Rollenbahn hinten mit Endanschlag, Ablagetisch mit Wannen
    _rollers(K, M, 1.2, 2.95, oy - 0.42, oy + 0.42, zb - 0.02)
    for x in (1.3, 2.9):
        for y in (oy - 0.4, oy + 0.4):
            box(K, M['steel_dull'], (x - 0.02, y - 0.02, 0.0), (x + 0.02, y + 0.02, zb - 0.09), bev=0.003)
    box(K, M['black_pl'], (2.95, oy - 0.45, zb - 0.1), (3.0, oy + 0.45, zb + 0.08), bev=0.01)
    box(K, M['steel'], (-2.0, oy + 0.5, 0.0), (-0.9, oy + 0.95, 0.04), bev=0.004)
    box(K, M['steel'], (-2.0, oy + 0.5, 0.7), (-0.9, oy + 0.95, 0.74), bev=0.004)
    for x in (-1.97, -0.93):
        for y in (oy + 0.53, oy + 0.92):
            box(K, M['steel_dull'], (x - 0.015, y - 0.015, 0.04), (x + 0.015, y + 0.015, 0.7))
    box(K, M['steel'], (1.4, oy + 0.5, 0.7), (2.95, oy + 0.95, 0.74), bev=0.004)
    for x in (1.43, 2.92):
        for y in (oy + 0.53, oy + 0.92):
            box(K, M['steel_dull'], (x - 0.015, y - 0.015, 0.0), (x + 0.015, y + 0.015, 0.7))
    for k, x in enumerate((-1.6, 1.9)):
        _tray(K, M, T(x, oy + 0.72, 0.74) @ R(rng.uniform(-0.1, 0.1), 'Z'))
    _tray(K, M, T(-1.45, oy, zb) @ R(0.1, 'Z'))
    for i in range(4):
        blob(K, M['dirt'], (rng.uniform(-0.8, 1.1), oy + rng.choice((-1, 1)) * (W + 0.0007), rng.uniform(0.07, 0.25)), (1, 0, 0), (0, 0, 1),
             rng.uniform(0.1, 0.3), 0.05, rng, n_off=0.0)
    K.finish()


def build_xray_scanner():
    _xray(0.5)


def build_xray_scanner__v1():
    _xray(0.0)


def _tray(K, M, tf, mat='grey_pl'):
    """Kunststoffwanne der Kontrolle (60 x 40 cm), offene Schale mit Rand."""
    def ring(w, d, z, r):
        return [tuple(tf @ Vector((a, b, z))) for (a, b) in rrect(w, d, r, 2)]
    rings = [ring(0.56, 0.36, 0.0, 0.04), ring(0.62, 0.42, 0.075, 0.05), ring(0.645, 0.445, 0.075, 0.055), ring(0.645, 0.445, 0.08, 0.055),
             ring(0.6, 0.4, 0.08, 0.045), ring(0.545, 0.345, 0.006, 0.035)]
    loft(K, M[mat], rings, cap0=True, cap1=True, recalc=False)


def build_tray_stack():
    """Wannenstapel auf Rollwagen (Boden): 12 ineinandergesetzte Wannen, leicht versetzt, Rollwagen mit vier Lenkrollen;
    in der obersten ein vergessener Guertel."""
    M = _m()
    K = Kit('wannen')
    rng = random.Random(3661)
    box(K, M['black_pl'], (-0.34, -0.24, 0.07), (0.34, 0.24, 0.11), bev=0.012, seg=2)
    for (x, y) in ((-0.28, -0.18), (0.28, -0.18), (-0.28, 0.18), (0.28, 0.18)):
        box(K, M['black_pl'], (x - 0.025, y - 0.025, 0.05), (x + 0.025, y + 0.025, 0.07), bev=0.005)
        cyl(K, M['rubber'], (x - 0.01, y - 0.012, 0.025), (x - 0.01, y + 0.012, 0.025), 0.025, seg=10)
    z = 0.11
    for k in range(12):
        _tray(K, M, T(rng.uniform(-0.008, 0.008), rng.uniform(-0.008, 0.008), z) @ R(rng.uniform(-0.03, 0.03), 'Z'))
        z += 0.034
    zt = z - 0.034 + 0.01
    pts = [Vector((math.cos(a) * (0.13 - 0.006 * a), math.sin(a) * (0.09 - 0.004 * a), zt + 0.003 * a)) for a in [k * 0.4 for k in range(30)]]
    sweep(K, M['fabric_k'], pts, [(-0.0015, -0.016), (0.0015, -0.016), (0.0015, 0.016), (-0.0015, 0.016)], up=(0, 0, 1))
    box(K, M['chrome'], (0.1, -0.02, zt), (0.14, 0.02, zt + 0.008), bev=0.002)
    K.finish()


def build_security_gate():
    """Torsonde (Boden, Durchgang entlang x, ausser Betrieb): zwei Saeulen mit gerundeten Kanten und LED-Leisten (aus),
    Querbalken mit Anzeige und Statusleuchte, Fussplatten, Kabelbruecke, Absperrband AUSSER BETRIEB, Zettel."""
    M = _m()
    K = Kit('sonde')
    rng = random.Random(3662)
    for s in (-1, 1):
        y0, y1 = sorted((s * 0.38, s * 0.47))
        box(K, M['grey_pl'], (-0.3, y0, 0.02), (0.3, y1, 2.06), bev=0.03, seg=3)
        box(K, M['steel'], (-0.33, y0 - 0.02, 0.0), (0.33, y1 + 0.02, 0.02), bev=0.004)
        yi = s * 0.379
        for k in range(14):
            box(K, M['black_pl'], (-0.29 + 0.0, min(yi, yi - s * 0.003), 0.2 + k * 0.12), (-0.27, max(yi, yi - s * 0.003), 0.28 + k * 0.12))
        box(K, M['gap'], (-0.305, y0 + 0.01, 0.1), (-0.3, y1 - 0.01, 1.95))
    box(K, M['grey_pl'], (-0.32, -0.48, 2.04), (0.32, 0.48, 2.22), bev=0.04, seg=3)
    box(K, M['screen_off'], (-0.3205, -0.15, 2.08), (-0.32, 0.15, 2.18))
    box(K, M['anthr'], (-0.3205, 0.25, 2.1), (-0.32, 0.35, 2.16))
    lathe(K, M['red_pl'], [(0.0, 0.0), (0.03, 0.0), (0.03, 0.02), (0.0, 0.04)], 12, T(0.0, 0.0, 2.22))
    # Absperrband zwischen den Saeulen
    for j in range(10):
        y0, y1 = -0.38 + 0.076 * j, -0.38 + 0.076 * (j + 1)
        sag = lambda t: 1.05 - 0.06 * math.sin(math.pi * (t + 0.38) / 0.76)  # noqa: E731
        sweep(K, M['red_pl'] if j % 2 == 0 else M['white_pl'], [(-0.31, y0, sag(y0)), (-0.31, y1, sag(y1))],
              [(-0.025, -0.0008), (0.025, -0.0008), (0.025, 0.0008), (-0.025, 0.0008)], up=(0, 0, 1))
    tfz = T(-0.312, -0.05, 1.04) @ R(0.05, 'X') @ FACE['-x']
    box(K, M['paper'], (-0.001, -0.1, -0.14), (0.0, 0.1, 0.0), tf=tfz)
    text('sonde_zettel', 'AUSSER BETRIEB', 0.022, M['black'], tfz @ T(0.0015, 0.0, -0.06))
    # Kabelbruecke zur Wand
    sweep(K, M['yellow_pl'], [(0.0, 0.47, 0.0), (0.0, 1.2, 0.0)], [(0.0, -0.05), (0.0, 0.05), (0.015, 0.03), (0.015, -0.03)], up=(0, 0, 1))
    blob(K, M['scuff'], (0.0, -0.46, 0.0205), (1, 0, 0), (0, 1, 0), 0.4, 0.06, rng, n_off=0.0)
    K.finish()


def _lounge_chair(v=0):
    """Loungesessel (Boden, Blick nach +x): Kufengestell aus Chromrohr, Sitzkoerper mit Keder, Sitzkissen, Rueckenkissen,
    Armlehnen, Abnutzung an den Armlehnen. v0 Leder braun, v1 Stoff blau."""
    M = _m()
    K = Kit('lounge')
    rng = random.Random(3670 + v)
    c = common.material('leder_braun', (0.30, 0.17, 0.10), look='fabric', rough=0.45, r=0.75) if v == 0 else M['seat']
    for s in (-1, 1):
        path = fillet([(0.3, s * 0.3, 0.27), (0.36, s * 0.3, 0.02), (-0.36, s * 0.3, 0.02), (-0.3, s * 0.3, 0.27)], 0.06, 4)
        tube(K, M['chrome'], path, 0.012, seg=8)
    for x in (-0.3, 0.3):
        cyl(K, M['chrome'], (x, -0.3, 0.27), (x, 0.3, 0.27), 0.01, seg=8)
    box(K, c, (-0.38, -0.41, 0.27), (0.38, 0.41, 0.4), bev=0.03, seg=2)
    fs = box(K, c, (-0.25, -0.3, 0.39), (0.37, 0.3, 0.5), bev=0.045, seg=3)
    bm = K.bm(c)
    for f in fs:
        for vv in f.verts:
            if vv.co.z > 0.47:
                vv.co.z += 0.012 * math.cos(vv.co.x * 4) * math.cos(vv.co.y * 4) - (0.01 if abs(vv.co.y) < 0.15 and abs(vv.co.x) < 0.15 else 0)
    del bm
    tfb = T(-0.3, 0.0, 0.39) @ R(-0.2, 'Y')
    box(K, c, (-0.08, -0.38, 0.0), (0.08, 0.38, 0.45), tf=tfb, bev=0.05, seg=3)
    for s in (-1, 1):
        y0, y1 = sorted((s * 0.3, s * 0.41))
        box(K, c, (-0.37, y0, 0.38), (0.37, y1, 0.62), bev=0.04, seg=3)
        blob(K, M['seat_worn'] if v else common.material('leder_abgewetzt', (0.42, 0.27, 0.16), look='fabric', rough=0.4, r=0.6),
             (0.1, s * 0.355, 0.6205), (1, 0, 0), (0, 1, 0), 0.3, 0.06, rng, n_off=0.0)
    # Keder an der Kante des Sitzkissens
    kp = [Vector((0.06 + a, b, 0.497)) for (a, b) in rrect(0.6, 0.58, 0.05, 3)]
    sweep(K, c, kp, circle(0.006, 5), closed=True, up=(0, 0, 1))
    K.finish()


def build_lounge_chair():
    _lounge_chair(0)


def build_lounge_chair__v1():
    _lounge_chair(1)


def build_lounge_table():
    """Lounge-Beistelltisch rund (Boden): Teller- Fuss und Saeule aus Chrom, Platte Holz mit Kante, Zeitschrift, Becher."""
    M = _m()
    K = Kit('ltisch')
    lathe(K, M['chrome'], [(0.0, 0.0), (0.22, 0.0), (0.22, 0.008), (0.05, 0.02), (0.0, 0.022)], 24)
    cyl(K, M['chrome'], (0, 0, 0.02), (0, 0, 0.43), 0.03, seg=12)
    lathe(K, M['anthr'], [(0.0, 0.43), (0.12, 0.43), (0.12, 0.44), (0.0, 0.44)], 16)
    lathe(K, M['wood'], [(0.0, 0.44), (0.315, 0.44), (0.32, 0.447), (0.32, 0.458), (0.314, 0.465), (0.0, 0.465)], 32)
    tfm = T(-0.08, 0.05, 0.465) @ R(0.4, 'Z')
    box(K, M['fabric_r'], (-0.11, -0.15, 0.0), (0.11, 0.15, 0.006), tf=tfm)
    plate(K, M['white_pl'], tfm @ Vector((0, 0.1, 0.0065)), tfm.to_3x3() @ Vector((1, 0, 0)), tfm.to_3x3() @ Vector((0, 1, 0)), 0.2, 0.05, n_off=0.0)
    _cup(K, M, T(0.15, -0.1, 0.465))
    K.finish()
