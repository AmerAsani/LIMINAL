"""V13.3: Baukasten fuer Level 0 und die Fundstuecke (models_l0.py, models_items.py).

Alles als bmesh-Netze mit mehreren Materialien je Objekt und eigenen UV-Koordinaten:
- Teile mit festem Material des Renderers (fm=..., Etiketten, Atlas der Fundstuecke) bekommen ihre
  Atlas-Koordinaten beim Bauen (setuv: v wie im Spiel, der glTF-Export dreht v um).
- alle anderen Teile bekommen eine Kastenprojektion in Metern (box_uv); das Spiel rechnet sie fuer
  Fundstuecke auf die Kachelgroesse der Textur um (src/render/item_models.cpp). Ausstattung (Level 0)
  braucht keine UV - die Welt projiziert die Texturen selbst.
Koordinaten wie in common.py: Meter, z oben, Blick entlang +x.
"""
import math
import random

import bmesh
import bpy
from mathutils import Vector

import common


class Part:
    """Ein Objekt im Bau: bmesh plus Materialliste (Index je Flaeche)."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.verify()
        self.mats = []
        self.atlas = set()   # Materialindizes mit eigenen (Atlas-)UV
        self.fixed = set()   # Flaechen mit vorgegebener Richtung (nicht neu ausrichten)
        self.uvmaps = {}     # Materialindex -> uvmap(co, normal)
        self.uvface = {}     # einzelne Flaechen -> uvmap (Vorrang vor uvmaps)

    def mat(self, m, atlas=False, uvmap=None):
        """Material anmelden. atlas: UV werden beim Bauen gesetzt; uvmap(co, normal) -> (u, v): UV werden beim
        Abschluss aus Lage und Flaechennormale berechnet (Atlas der Fundstuecke)."""
        if m not in self.mats:
            self.mats.append(m)
        i = self.mats.index(m)
        if atlas or uvmap is not None:
            self.atlas.add(i)
        if uvmap is not None:
            self.uvmaps[i] = uvmap
        return i

    def v(self, co):
        return self.bm.verts.new(co)

    def face(self, verts, mi, uvs=None, want=None):
        """Flaeche; want = Richtung, in die sie zeigen soll (dann wird sie beim Abschluss nicht neu ausgerichtet)."""
        try:
            f = self.bm.faces.new(verts)
        except ValueError:
            return None
        f.material_index = mi
        if want is not None:
            f.normal_update()
            if f.normal.dot(Vector(want)) < 0:
                f.normal_flip()
            self.fixed.add(f)
        if uvs is not None:
            for loop, (u, vv) in zip(f.loops, uvs):
                setuv(loop, self.uv, u, vv)
        return f

    def fix(self, faces, want):
        """Flaechen in eine Richtung drehen und festhalten (Rueckseiten werden im Spiel nicht gezeichnet)."""
        for f in faces:
            if f is None:
                continue
            f.normal_update()
            if f.normal.dot(Vector(want)) < 0:
                f.normal_flip()
            self.fixed.add(f)

    def finish(self, smooth_deg=35.0, box_scale=1.0, weighted=True, recalc=True):
        bm = self.bm
        if recalc:
            free = [f for f in bm.faces if f not in self.fixed]
            if free:
                bmesh.ops.recalc_face_normals(bm, faces=free)
        # Atlas aus Lage und Normale (vor remove_doubles: die Flaechenzuordnung in uvface bleibt gueltig)
        for f in bm.faces:
            fn = self.uvface.get(f) or self.uvmaps.get(f.material_index)
            if fn is None:
                continue
            f.normal_update()
            for loop in f.loops:
                setuv(loop, self.uv, *fn(loop.vert.co, f.normal))
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
        # Kastenprojektion fuer alle Flaechen ohne Atlas
        for f in bm.faces:
            if f.material_index in self.atlas:
                continue
            n = f.normal
            a = (abs(n.x), abs(n.y), abs(n.z))
            for loop in f.loops:
                c = loop.vert.co
                if a[2] >= a[0] and a[2] >= a[1]:
                    u, vv = c.x, c.y * (1 if n.z > 0 else -1)
                elif a[0] >= a[1]:
                    u, vv = -c.y * (1 if n.x > 0 else -1), -c.z
                else:
                    u, vv = c.x * (1 if n.y > 0 else -1), -c.z
                setuv(loop, self.uv, u * box_scale, vv * box_scale)
        me = bpy.data.meshes.new(self.name)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(ob)
        for m in self.mats:
            me.materials.append(m)
        me.shade_smooth()
        for p in me.polygons:
            p.use_smooth = True
        try:
            me.set_sharp_from_angle(angle=math.radians(smooth_deg))
        except AttributeError:
            pass
        if weighted:
            mod = ob.modifiers.new('weighted', 'WEIGHTED_NORMAL')
            mod.keep_sharp = True
        return ob


def setuv(loop, layer, u, v):
    """UV wie im Spiel (v nach unten); Blender/glTF drehen v beim Export um."""
    loop[layer].uv = (u, 1.0 - v)


def fm_material(fm, rgb=(0.8, 0.8, 0.8), name=None):
    """Festes Material des Renderers (Atlas der Fundstuecke); look zaehlt im Spiel nicht."""
    return common.material(name or fm, rgb, look='paint', fm=fm)


# --- Profile ------------------------------------------------------------------------------------------
def rrect(hw, hh, r, seg=4):
    """Umriss eines Rechtecks mit gerundeten Ecken (halbe Breite/Hoehe, Radius), gegen den Uhrzeigersinn,
    Start rechts unten. Immer 4 * (seg + 1) Punkte (auch bei sehr kleinem Radius)."""
    r = max(min(r, hw - 1e-5, hh - 1e-5), 0.0004)
    pts = []
    for cx, cy, a0 in ((hw - r, -hh + r, -90), (hw - r, hh - r, 0), (-hw + r, hh - r, 90), (-hw + r, -hh + r, 180)):
        for k in range(seg + 1):
            a = math.radians(a0 + 90.0 * k / seg)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def ring_sweep(P, path_at, profile, frame, mi, cap0=False, cap1=False, uvfn=None, closed=False):
    """Profil um einen geschlossenen Umriss fuehren: profile = [(einzug, hoehe), ...]; path_at(einzug) liefert den
    Umriss (Liste von (a, b), immer gleich viele Punkte); frame(a, b, h) -> (x, y, z). closed: das Profil ist
    selbst geschlossen (Lamelle, Rohr). Liefert die Ringe."""
    rings = []
    for k, (ins, h) in enumerate(profile):
        rings.append([P.v(frame(a, b, h)) for (a, b) in path_at(ins)])
    n = len(rings[0])
    nb = len(rings) if closed else len(rings) - 1
    for k in range(nb):
        r0, r1 = rings[k], rings[(k + 1) % len(rings)]
        mk = mi[k] if isinstance(mi, (list, tuple)) else mi
        for i in range(n):
            j = (i + 1) % n
            f = P.face((r0[i], r0[j], r1[j], r1[i]), mk)
            if f is not None and uvfn is not None:
                for loop in f.loops:
                    setuv(loop, P.uv, *uvfn(loop.vert.co))
    m0 = mi[0] if isinstance(mi, (list, tuple)) else mi
    m1 = mi[-1] if isinstance(mi, (list, tuple)) else mi
    if cap0:
        P.face(list(reversed(rings[0])), m0)
    if cap1:
        P.face(list(rings[-1]), m1)
    return rings


def sweep_open(P, path, profile, mi, up=(0.0, 0.0, 1.0), caps=True):
    """Profil entlang eines offenen Linienzugs (Gehrung an den Ecken). profile = [(s, t), ...]: s nach aussen
    (rechts der Laufrichtung gegen die Seite 'cross(richtung, up)'), t entlang up. Geschlossenes Profil."""
    upv = Vector(up)
    pts = [Vector(p) for p in path]
    n = len(pts)
    sides = []
    for i in range(n - 1):
        d = (pts[i + 1] - pts[i]).normalized()
        sides.append(-d.cross(upv).normalized())
    rings = []
    for i in range(n):
        if i == 0:
            s = sides[0]
        elif i == n - 1:
            s = sides[-1]
        else:
            a, b = sides[i - 1], sides[i]
            m = (a + b).normalized()
            s = m / max(m.dot(a), 0.2)
        rings.append([P.v(pts[i] + s * so + upv * to) for (so, to) in profile])
    k = len(profile)
    for i in range(n - 1):
        for j in range(k):
            j1 = (j + 1) % k
            P.face((rings[i][j], rings[i][j1], rings[i + 1][j1], rings[i + 1][j]), mi)
    if caps:
        P.face(list(rings[0]), mi)
        P.face(list(reversed(rings[-1])), mi)
    return rings


def frame_wall(a, b, h):
    """Wandmodelle: Umriss in der Wandebene (a = y, b = z), Hoehe = Abstand von der Wand (x)."""
    return (h, a, b)


def frame_ceiling(a, b, h):
    """Deckenmodelle: Umriss in der Deckenebene (a = x, b = y), Hoehe = Abstand unter der Decke (-z)."""
    return (a, b, -h)


def frame_floor(a, b, h):
    return (a, b, h)


def revolve(P, prof, seg, mi, axis='z', origin=(0.0, 0.0, 0.0), rmod=None, uvfn=None, a0=0.0, arc=2 * math.pi,
            close=True):
    """Drehkoerper: prof = [(r, t), ...] (Radius, Lage entlang der Achse). Punkte mit r == 0 werden zu einem
    Pol. rmod(theta, t, r) -> r veraendert den Radius je Winkel (Riffel, Fuesse). uvfn(s, k, t) -> (u, v), s = 0..1
    ringsum (am Saum 1), k = Profilindex. Liefert die Ringe (Listen von Punkten oder ein Pol)."""
    ox, oy, oz = origin

    def place(r, t, th):
        c, s = math.cos(th), math.sin(th)
        if axis == 'z':
            return (ox + r * c, oy + r * s, oz + t)
        if axis == 'x':
            return (ox + t, oy + r * c, oz + r * s)
        return (ox + r * s, oy + t, oz + r * c)  # y

    nseg = seg if close else seg + 1
    rings = []
    for k, (r, t) in enumerate(prof):
        if r <= 1e-9:
            rings.append(P.v(place(0.0, t, 0.0)))
            continue
        ring = []
        for i in range(nseg):
            th = a0 + arc * i / seg
            rr = rmod(th, t, r) if rmod else r
            ring.append(P.v(place(rr, t, th)))
        rings.append(ring)

    def uvs(k, i):
        if uvfn is None:
            return None
        return uvfn(i / seg, k, prof[k][1])

    for k in range(len(prof) - 1):
        A, B = rings[k], rings[k + 1]
        for i in range(seg if close else seg):
            i1 = (i + 1) % nseg if close else i + 1
            if isinstance(A, list) and isinstance(B, list):
                vs = (A[i], A[i1], B[i1], B[i])
                ks = ((k, i), (k, i + 1), (k + 1, i + 1), (k + 1, i))
            elif isinstance(A, list):
                vs = (A[i], A[i1], B)
                ks = ((k, i), (k, i + 1), (k + 1, i + 0.5))
            elif isinstance(B, list):
                vs = (A, B[i1], B[i])
                ks = ((k, i + 0.5), (k + 1, i + 1), (k + 1, i))
            else:
                continue
            f = P.face(vs, mi)
            if f is not None and uvfn is not None:
                for loop, (kk, ii) in zip(f.loops, ks):
                    setuv(loop, P.uv, *uvs(kk, ii))
    return rings


def bevel_box(P, lo, hi, mi, r=0.002, seg=1, uvfn=None):
    """Kasten mit gerundeten/abgefasten Kanten als ein Netz (Profil um die z-Achse gefuehrt)."""
    hx, hy = (hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2
    cx, cy = (hi[0] + lo[0]) / 2, (hi[1] + lo[1]) / 2
    z0, z1 = lo[2], hi[2]
    r = min(r, hx * 0.49, hy * 0.49, (z1 - z0) * 0.49)
    prof = []
    for k in range(seg + 1):  # unten
        a = math.radians(-90 + 90.0 * k / seg)
        prof.append((r - r * math.cos(a), z0 + r + r * math.sin(a)))
    for k in range(seg + 1):  # oben
        a = math.radians(90.0 * k / seg)
        prof.append((r - r * math.cos(a), z1 - r + r * math.sin(a)))
    prof[0] = (r, z0)
    prof[-1] = (r, z1)
    rings = ring_sweep(P, lambda ins: [(cx + a, cy + b) for (a, b) in rrect(hx - ins, hy - ins, r - ins if r - ins > 0 else 0, seg)],
                       prof, frame_floor, mi, cap0=True, cap1=True, uvfn=uvfn)
    return rings


def box(P, lo, hi, mi, uvfn=None):
    """Einfacher Kasten (6 Flaechen)."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    v = [P.v(c) for c in ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                          (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    out = []
    for fi in faces:
        f = P.face([v[i] for i in fi], mi)
        if f is not None and uvfn is not None:
            for loop in f.loops:
                setuv(loop, P.uv, *uvfn(loop.vert.co, f.normal))
        out.append(f)
    return out


def transform_verts(verts, mat):
    for v in verts:
        v.co = mat @ v.co


def new_verts_since(P, n0):
    P.bm.verts.ensure_lookup_table()
    return [P.bm.verts[i] for i in range(n0, len(P.bm.verts))]


def count(P):
    return len(P.bm.verts)


def fcount(P):
    return len(P.bm.faces)


def new_faces_since(P, n0):
    P.bm.faces.ensure_lookup_table()
    return [P.bm.faces[i] for i in range(n0, len(P.bm.faces))]


def fix_toward(P, faces, point):
    """Flaechen zu einem Punkt hin ausrichten (Innenseiten von Toepfen, Mulden, Laibungen)."""
    pt = Vector(point)
    for f in faces:
        f.normal_update()
        if f.normal.dot(pt - f.calc_center_median()) < 0:
            f.normal_flip()
        P.fixed.add(f)


def fix_away(P, faces, point):
    pt = Vector(point)
    for f in faces:
        f.normal_update()
        if f.normal.dot(f.calc_center_median() - pt) < 0:
            f.normal_flip()
        P.fixed.add(f)


def screw(P, c, normal, r, mi_head, mi_slot, seg=8, cross=True, h=None, rot=0.0):
    """Linsenkopfschraube mit Kreuzschlitz (Schlitz als dunkle Einlage). c Mitte auf der Flaeche, normal nach aussen."""
    h = h if h is not None else r * 0.45
    n0 = count(P)
    prof = [(r, 0.0), (r * 0.97, h * 0.45), (r * 0.78, h * 0.85), (r * 0.42, h), (0.0, h)]
    revolve(P, prof, seg, mi_head, axis='z')
    if cross:
        w, L = r * 0.16, r * 0.62
        for ang in (0.0, math.pi / 2):
            ca, sa = math.cos(ang + rot), math.sin(ang + rot)
            pts = [(-L, -w), (L, -w), (L, w), (-L, w)]
            vs = [P.v((x * ca - y * sa, x * sa + y * ca, h * 1.004 + 0.00002 * (ang > 0))) for x, y in pts]
            P.face(vs, mi_slot)
    orient(new_verts_since(P, n0), c, normal)


def orient(verts, c, normal):
    """Teile, die entlang +z gebaut sind, auf eine Flaechennormale drehen und nach c schieben."""
    n = Vector(normal).normalized()
    q = Vector((0, 0, 1)).rotation_difference(n)
    m = q.to_matrix().to_4x4()
    m.translation = Vector(c)
    transform_verts(verts, m)


def jitter(verts, amp, seed, keep=None):
    rnd = random.Random(seed)
    for v in verts:
        if keep and keep(v):
            continue
        v.co += Vector((rnd.uniform(-amp, amp), rnd.uniform(-amp, amp), rnd.uniform(-amp, amp)))


def blob(P, c, rx, ry, mi, seg=10, seed=1, rough=0.35, frame=frame_floor, h=0.0):
    """Unregelmaessiger Fleck (flaches Vieleck) in einer Ebene: Schmutz, Abplatzer, Rost."""
    rnd = random.Random(seed)
    pts = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        k = 1.0 + rnd.uniform(-rough, rough)
        pts.append((c[0] + rx * k * math.cos(a), c[1] + ry * k * math.sin(a)))
    vs = [P.v(frame(a, b, h)) for (a, b) in pts]
    return P.face(vs, mi)
