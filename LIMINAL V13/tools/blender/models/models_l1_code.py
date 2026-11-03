"""V13.3 Teil A (Ebene 1): die Modelle, die bisher im Code entstanden (src/props/prop_library.cpp), neu in Blender.

Klassen (Varianten): batten (0-1), ceiling_vent (0-1), chair (0-1), desk_crt (0-2), gauge (0-3), highbay,
pipe_run (0-3), power_box, rack (0-7), safety_light, side_table (0-1); dazu fuer den Rohrverlauf der Karten
pipe_elbow (0-3), pipe_wall (0-1), pipe_drop (0-3). Masse, Ursprung und Befestigung wie die Code-Modelle
(PROPS_Level1.md). Konventionen wie common.py: Meter, z oben, Blick entlang +x.

Werkzeug: Kit sammelt Geometrie je Material in einem bmesh (ein Objekt je Material). Gekruemmte Flaechen bekommen
analytische Normalen (Drehkoerper, Rohre), damit Materialwechsel auf einem Rohr keine Schattierungskante erzeugen;
Knicke im Profil werden hart. Die Ausstattung wird mit Rueckseiten-Culling gezeichnet (renderer.cpp): Abziehbilder
zeigen ueber poly(n=...) zur gewuenschten Seite, duenne Bleche, Laschen und Folien sind zweiseitig (poly(two=True)).
Import: Blender (x, y, z) -> Spiel (x, -y, z) (props/glb_import.cpp).
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

TAU = 2.0 * math.pi


# =====================================================================================================================
# Werkzeugkasten
# =====================================================================================================================
def _axes(axis):
    """(U, V, W) rechtshaendig, W = Achse."""
    if axis == 'x':
        return Vector((0, 1, 0)), Vector((0, 0, 1)), Vector((1, 0, 0))
    if axis == 'y':
        return Vector((0, 0, 1)), Vector((1, 0, 0)), Vector((0, 1, 0))
    return Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(axis, a):
    return Matrix.Rotation(a, 4, axis.upper())


def frame(origin, w, up=(0, 0, 1)):
    """Matrix, deren lokale z-Achse entlang w zeigt (x, y senkrecht dazu)."""
    w = Vector(w).normalized()
    u = Vector(up)
    if abs(u.dot(w)) > 0.95:
        u = Vector((1, 0, 0)) if abs(w.x) < 0.9 else Vector((0, 1, 0))
    x = u.cross(w).normalized()
    y = w.cross(x)
    M = Matrix.Identity(4)
    for i in range(3):
        M[i][0], M[i][1], M[i][2], M[i][3] = x[i], y[i], w[i], origin[i]
    return M


def _cols(prof, closed, sharp_deg):
    """Profilpunkte -> (Punkte, 2D-Normalen, skip): an Knicken (> sharp_deg) wird der Punkt verdoppelt (harte Kante);
    skip enthaelt die Indizes k, deren Viereck (k, k+1) zwischen den Doppelpunkten laege. Normale = rechts der
    Laufrichtung (Profil gegen den Uhrzeigersinn -> nach aussen)."""
    n = len(prof)
    segs = n if closed else n - 1
    en = []
    for s in range(segs):
        a, b = prof[s], prof[(s + 1) % n]
        du, dv = b[0] - a[0], b[1] - a[1]
        L = math.hypot(du, dv) or 1.0
        en.append((dv / L, -du / L))
    cosl = math.cos(math.radians(sharp_deg))
    pts, nrm, skip = [], [], []
    for i in range(n):
        prev = en[i - 1] if (closed or i > 0) else None
        nxt = en[i] if (closed or i < segs) else None
        if prev is None:
            ns = [nxt]
        elif nxt is None:
            ns = [prev]
        elif prev[0] * nxt[0] + prev[1] * nxt[1] < cosl:
            ns = [prev, nxt]
        else:
            x, y = prev[0] + nxt[0], prev[1] + nxt[1]
            L = math.hypot(x, y) or 1.0
            ns = [(x / L, y / L)]
        for j, nn in enumerate(ns):
            if j == 1:
                skip.append(len(pts) - 1)
            pts.append(prof[i])
            nrm.append(nn)
    return pts, nrm, skip


def _xp(M, p):
    return (M @ Vector(p)) if M is not None else Vector(p)


def _xn(M, n):
    v = Vector(n)
    if M is not None:
        v = M.to_3x3() @ v
    return v.normalized() if v.length > 1e-9 else v


class Kit:
    """Geometrie je Material; finish() legt je Material ein Objekt an (mit eigenen Normalen)."""

    def __init__(self):
        self.parts = {}

    def _get(self, mat):
        p = self.parts.get(mat.name)
        if p is None:
            bm = bmesh.new()
            uv = bm.loops.layers.uv.new('UVMap')
            nl = bm.verts.layers.float_vector.new('nrm')
            p = self.parts[mat.name] = (bm, mat, uv, nl)
        return p

    def verts(self, mat, pts, nrms=None):
        bm, _, _, nl = self._get(mat)
        out = []
        for i, p in enumerate(pts):
            v = bm.verts.new(p)
            if nrms is not None and nrms[i] is not None:
                v[nl] = nrms[i]
            out.append(v)
        return out

    def face(self, mat, vs, smooth=False, uvs=None):
        bm, _, uvl, _ = self._get(mat)
        try:
            f = bm.faces.new(vs)
        except ValueError:
            return None
        f.smooth = smooth
        if uvs:
            for loop, uv in zip(f.loops, uvs):
                loop[uvl].uv = uv
        return f

    # --- Grundformen ---------------------------------------------------------------------------------------------
    def grid(self, mat, rows, nrows=None, closed=False, smooth=True, skip_cols=(), skip_rows=()):
        """rows[i][k] (bereits transformiert); Viereck (i,k),(i,k+1),(i+1,k+1),(i+1,k). Zeilen, deren Punkte
        zusammenfallen (Pol), werden ein Punkt (Dreiecke)."""
        V = []
        for i, row in enumerate(rows):
            if all((p - row[0]).length < 1e-7 for p in row):
                v = self.verts(mat, [row[0]], [nrows[i][0]] if nrows else None)[0]
                V.append([v] * len(row))
            else:
                V.append(self.verts(mat, row, nrows[i] if nrows else None))
        n = len(rows[0])
        kmax = n if closed else n - 1
        sc, sr = set(skip_cols), set(skip_rows)
        for i in range(len(rows) - 1):
            if i in sr:
                continue
            for k in range(kmax):
                if k in sc:
                    continue
                k1 = (k + 1) % n
                q = []
                for v in (V[i][k], V[i][k1], V[i + 1][k1], V[i + 1][k]):
                    if v not in q:
                        q.append(v)
                if len(q) >= 3:
                    self.face(mat, q, smooth)

    def poly(self, mat, pts, M=None, uvs=None, smooth=False, n=None, two=False):
        """Vieleck. Die Ausstattung wird mit Rueckseiten-Culling gezeichnet: n = gewuenschte Blickrichtung der
        Vorderseite (dreht die Reihenfolge bei Bedarf), two = auch die Rueckseite (duenne Bleche, Laschen, Folie)."""
        P = [_xp(M, p) for p in pts]
        if n is not None:
            nn = _xn(M, n)
            fn = Vector((0, 0, 0))
            for i in range(len(P)):  # Newell
                a, b = P[i], P[(i + 1) % len(P)]
                fn += Vector(((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)))
            if fn.dot(nn) < 0:
                P = P[::-1]
                uvs = uvs[::-1] if uvs else uvs
        f = self.face(mat, self.verts(mat, P), smooth, uvs)
        if two:
            self.face(mat, self.verts(mat, P[::-1]), smooth, uvs[::-1] if uvs else None)
        return f

    def box(self, mat, lo, hi, M=None, skip=()):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        P = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        vs = self.verts(mat, [_xp(M, p) for p in P])
        F = {'-z': (0, 3, 2, 1), '+z': (4, 5, 6, 7), '-y': (0, 1, 5, 4), '+y': (2, 3, 7, 6), '-x': (0, 4, 7, 3), '+x': (1, 2, 6, 5)}
        for k, idx in F.items():
            if k not in skip:
                self.face(mat, [vs[i] for i in idx])

    def bbox(self, mat, lo, hi, r, M=None, seg=1):
        """Kasten mit gerundeten Kanten (Fase bei seg=1)."""
        bm = self._get(mat)[0]
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        r = min(r, 0.49 * min(x1 - x0, y1 - y0, z1 - z0))
        P = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        vs = self.verts(mat, [_xp(M, p) for p in P])
        fs = []
        for idx in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (2, 3, 7, 6), (0, 4, 7, 3), (1, 2, 6, 5)):
            fs.append(self.face(mat, [vs[i] for i in idx]))
        edges = list({e for f in fs if f for e in f.edges})
        if r > 1e-5:
            res = bmesh.ops.bevel(bm, geom=edges + vs, offset=r, offset_type='OFFSET', segments=seg, profile=0.5,
                                  affect='EDGES', clamp_overlap=True)
            if seg > 1:
                for f in res['faces']:
                    f.smooth = True

    def prism(self, mat, prof, w0, w1, M=None, axis='z', caps=(True, True), smooth=False, sharp=40.0):
        """Profil (u, v) gegen den Uhrzeigersinn (von +W gesehen) von w0 bis w1 entlang axis ziehen."""
        U, V, W = _axes(axis)
        if smooth:
            pts, nrm, skip = _cols(prof, True, sharp)
        else:
            pts, nrm, skip = list(prof), None, []
        rows, nrows = [], []
        for w in (w0, w1):
            rows.append([_xp(M, W * w + U * u + V * v) for (u, v) in pts])
            if nrm:
                nrows.append([_xn(M, U * a + V * b) for (a, b) in nrm])
        self.grid(mat, rows, nrows if nrm else None, closed=True, smooth=smooth, skip_cols=skip)
        if caps[0]:
            self.poly(mat, [W * w0 + U * u + V * v for (u, v) in reversed(prof)], M)
        if caps[1]:
            self.poly(mat, [W * w1 + U * u + V * v for (u, v) in prof], M)

    def lathe(self, mat, prof, seg, M=None, axis='z', a0=0.0, a1=TAU, smooth=True, sharp=40.0, jitter=None):
        """Drehkoerper: prof = [(r, w)] gegen den Uhrzeigersinn (r rechts, w oben) -> Normalen nach aussen."""
        U, V, W = _axes(axis)
        closed = abs(a1 - a0 - TAU) < 1e-6
        if smooth:
            pts, nrm, skip = _cols(prof, False, sharp)
        else:
            pts, nrm, skip = list(prof), None, []
        ns = seg if closed else seg + 1
        rows, nrows = [], []
        for i, (r, w) in enumerate(pts):
            row, nrow = [], []
            for k in range(ns):
                a = a0 + (a1 - a0) * k / seg
                d = U * math.cos(a) + V * math.sin(a)
                rr = r * (jitter(i, k) if jitter else 1.0)
                row.append(_xp(M, W * w + d * rr))
                if nrm:
                    nrow.append(_xn(M, d * nrm[i][0] + W * nrm[i][1]))
            rows.append(row)
            nrows.append(nrow)
        self.grid(mat, rows, nrows if nrm else None, closed=closed, smooth=smooth, skip_rows=skip)

    def sweep(self, mat, path, prof, M=None, scales=None, caps=(False, False), smooth=True, sharp=40.0, up=(0, 0, 1),
              closed=False):
        """Profil (u, v) gegen den Uhrzeigersinn entlang eines Linienzugs (parallel transportierte Rahmen, Gehrung)."""
        P = [Vector(p) for p in path]
        n = len(P)
        if closed:
            P.append(P[0])
            n += 1
        dirs = [(P[i + 1] - P[i]).normalized() for i in range(n - 1)]
        Ts = []
        for i in range(n):
            if i == 0:
                t = (dirs[0] + dirs[-1]) if closed else dirs[0]
            elif i == n - 1:
                t = (dirs[-1] + dirs[0]) if closed else dirs[-1]
            else:
                t = dirs[i - 1] + dirs[i]
            Ts.append(t.normalized() if t.length > 1e-9 else dirs[min(i, n - 2)])
        u0 = Vector(up)
        if abs(u0.dot(Ts[0])) > 0.95:
            u0 = Vector((1, 0, 0)) if abs(Ts[0].x) < 0.9 else Vector((0, 1, 0))
        N = (u0 - Ts[0] * u0.dot(Ts[0])).normalized()
        Ns = [N]
        for i in range(1, n):
            q = Ts[i - 1].rotation_difference(Ts[i])
            N = q @ Ns[-1]
            N = (N - Ts[i] * N.dot(Ts[i])).normalized()
            Ns.append(N)
        if smooth:
            pts, nrm, skip = _cols(prof, True, sharp)
        else:
            pts, nrm, skip = list(prof), None, []
        rows, nrows = [], []
        for i in range(n):
            Ti, Ni = Ts[i], Ns[i]
            Bi = Ti.cross(Ni)
            s = scales[i] if scales else 1.0
            # Gehrung: in der Knickebene um 1/cos(halber Winkel) strecken
            b, st = None, 1.0
            if 0 < i < n - 1 or closed:
                d0 = dirs[i - 1] if i > 0 else dirs[-1]
                d1 = dirs[i] if i < n - 1 else dirs[0]
                dd = d1 - d0
                if dd.length > 1e-6:
                    b = (dd - Ti * dd.dot(Ti)).normalized()
                    c = max(0.2, math.cos(0.5 * d0.angle(d1)))
                    st = 1.0 / c
            row, nrow = [], []
            for (u, v) in pts:
                o = (Ni * u + Bi * v) * s
                if b is not None:
                    o = o + b * (o.dot(b) * (st - 1.0))
                row.append(_xp(M, P[i] + o))
            if nrm:
                for (a, c2) in nrm:
                    nrow.append(_xn(M, Ni * a + Bi * c2))
            rows.append(row)
            nrows.append(nrow)
        self.grid(mat, rows, nrows if nrm else None, closed=True, smooth=smooth, skip_cols=skip)
        if caps[0] and not closed:
            self.poly(mat, [rows[0][k] for k in range(len(pts)) if k - 1 not in skip][::-1] if skip else rows[0][::-1])
        if caps[1] and not closed:
            self.poly(mat, [rows[-1][k] for k in range(len(pts)) if k - 1 not in skip] if skip else rows[-1])

    def cyl(self, mat, a, b, r, seg=8, caps=(True, True), smooth=True):
        prof = [(r * math.cos(TAU * k / seg), r * math.sin(TAU * k / seg)) for k in range(seg)]
        self.sweep(mat, [a, b], prof, caps=caps, smooth=smooth, sharp=89.0 if seg > 4 else 10.0)

    def hexnut(self, mat, c, w, rf, h, M=None, caps=(False, True)):
        """Sechskant (Schluesselweite 2*rf) von c entlang w (Laenge h)."""
        ro = rf / math.cos(math.pi / 6)
        prof = [(ro * math.cos(math.pi / 6 + k * math.pi / 3), ro * math.sin(math.pi / 6 + k * math.pi / 3)) for k in range(6)]
        F = frame(c, w)
        if M is not None:
            F = M @ F
        self.prism(mat, prof, 0.0, h, F, caps=caps)

    def cell(self, mat, c, right, up, cell, M=None):
        """Flaeche mit einer Zelle des Atlas L_SIGNS (right/up: halbe Breite/Hoehe als Vektor)."""
        c, r_, u_ = Vector(c), Vector(right), Vector(up)
        pts = [c - r_ + u_, c - r_ - u_, c + r_ - u_, c + r_ + u_]
        e = 0.004
        u0, v0 = (cell % 8) / 8.0 + e, (cell // 8) / 8.0 + e
        u1, v1 = u0 + 0.125 - 2 * e, v0 + 0.125 - 2 * e
        uvs = [(u0, 1 - v0), (u0, 1 - v1), (u1, 1 - v1), (u1, 1 - v0)]
        return self.poly(mat, pts, M, uvs)

    def text(self, mat, s, c, right, up, size, M=None, gap=0.2):
        """Schablonenschrift (Atlas-Zeichen) zentriert um c; right/up Einheitsvektoren der Schreibebene."""
        r_, u_ = Vector(right).normalized(), Vector(up).normalized()
        w = size * 0.72
        total = len(s) * w + (len(s) - 1) * w * gap
        x = -total / 2 + w / 2
        for ch in s:
            cell = _glyph(ch)
            if cell >= 0:
                self.cell(mat, Vector(c) + r_ * x, r_ * (w / 2), u_ * (size / 2), cell, M)
            x += w * (1 + gap)

    def blob(self, mat, c, right, up, rx, ry, n, rng, M=None, irr=0.35):
        """unregelmaessiger Fleck (Vieleck) in der Ebene right/up um c."""
        c, r_, u_ = Vector(c), Vector(right).normalized(), Vector(up).normalized()
        pts = []
        ph = rng.uniform(0, TAU)
        for k in range(n):
            a = ph + TAU * k / n
            f = 1.0 + rng.uniform(-irr, irr)
            pts.append(c + r_ * (math.cos(a) * rx * f) + u_ * (math.sin(a) * ry * f))
        return self.poly(mat, pts, M)

    def streak(self, mat, top, right, down, w, L, rng, M=None, n=4):
        """Laufspur (Rost, Wasser) von top nach down: oben breit, unten spitz, leicht unregelmaessig."""
        top, r_, d_ = Vector(top), Vector(right).normalized(), Vector(down).normalized()
        left, rightp = [], []
        for k in range(n + 1):
            t = k / n
            ww = w * (1.0 - 0.85 * t) * rng.uniform(0.8, 1.15)
            off = r_ * (rng.uniform(-0.15, 0.15) * w)
            p = top + d_ * (L * t) + off
            left.append(p - r_ * ww / 2)
            rightp.append(p + r_ * ww / 2)
        return self.poly(mat, left + rightp[::-1], M)

    def tube_patch(self, mat, M, r, w0, w1, a0, a1, rng, nw=2, na=2, irr=0.25):
        """Flecken auf einem Rohrmantel (Achse = lokale x von M), leicht ueber der Oberflaeche."""
        rows, nrows = [], []
        for i in range(nw + 1):
            w = w0 + (w1 - w0) * i / nw
            row, nrow = [], []
            for k in range(na + 1):
                a = a0 + (a1 - a0) * k / na
                if 0 < i < nw:
                    a += rng.uniform(-irr, irr) * (a1 - a0) / na
                ww = w + (rng.uniform(-irr, irr) * (w1 - w0) / nw if 0 < k < na else 0.0)
                d = Vector((0, math.cos(a), math.sin(a)))
                row.append(_xp(M, Vector((ww, 0, 0)) + d * r))
                nrow.append(_xn(M, d))
            rows.append(row)
            nrows.append(nrow)
        self.grid(mat, rows, nrows, smooth=True)

    def xform(self, M):
        """alles bisher Gebaute transformieren (z. B. umgekippter Stuhl)."""
        M3 = M.to_3x3()
        for bm, _, _, nl in self.parts.values():
            bmesh.ops.transform(bm, matrix=M, verts=bm.verts)
            for v in bm.verts:
                n = Vector(v[nl])
                if n.length > 1e-6:
                    v[nl] = (M3 @ n).normalized()

    def mirror_y(self):
        """an y = 0 spiegeln (Umlaufsinn und Normalen mit) - Blender +y ist im Spiel -y."""
        for bm, _, _, nl in self.parts.values():
            for v in bm.verts:
                v.co.y = -v.co.y
                n = Vector(v[nl])
                if n.length > 1e-6:
                    v[nl] = (n.x, -n.y, n.z)
            bmesh.ops.reverse_faces(bm, faces=bm.faces[:])

    def bounds(self):
        lo = Vector((1e9, 1e9, 1e9))
        hi = -lo
        for bm, _, _, _ in self.parts.values():
            for v in bm.verts:
                for i in range(3):
                    lo[i] = min(lo[i], v.co[i])
                    hi[i] = max(hi[i], v.co[i])
        return lo, hi

    def finish(self, name='teil'):
        for i, (bm, mat, _, _) in enumerate(self.parts.values()):
            me = bpy.data.meshes.new(f'{name}{i}')
            bm.to_mesh(me)
            bm.free()
            attr = me.attributes.get('nrm')
            if attr is not None:
                nv = [tuple(d.vector) for d in attr.data]
                cn = [tuple(c.vector) for c in me.corner_normals]
                out = []
                for p in me.polygons:
                    for li in p.loop_indices:
                        n = nv[me.loops[li].vertex_index]
                        if not p.use_smooth:
                            out.append(tuple(p.normal))
                        elif n[0] * n[0] + n[1] * n[1] + n[2] * n[2] > 0.5:
                            out.append(n)
                        else:
                            out.append(cn[li])
                me.attributes.remove(me.attributes.get('nrm'))
                me.normals_split_custom_set(out)
            ob = bpy.data.objects.new(f'{name}{i}', me)
            bpy.context.scene.collection.objects.link(ob)
            me.materials.append(mat)
        self.parts = {}


def _glyph(c):
    if 'A' <= c <= 'Z':
        return ord(c) - ord('A')
    if 'a' <= c <= 'z':
        return ord(c) - ord('a')
    if '0' <= c <= '9':
        return 26 + ord(c) - ord('0')
    if c == '-':
        return 36
    return -1


def circle(r, seg, ph=0.0):
    return [(r * math.cos(ph + TAU * k / seg), r * math.sin(ph + TAU * k / seg)) for k in range(seg)]


def rrect(w, h, r, n=2):
    """gerundetes Rechteck (halbe Breite w, halbe Hoehe h), gegen den Uhrzeigersinn."""
    pts = []
    for (cx, cy, a0) in ((w - r, -h + r, -math.pi / 2), (w - r, h - r, 0.0), (-w + r, h - r, math.pi / 2), (-w + r, -h + r, math.pi)):
        for k in range(n + 1):
            a = a0 + (math.pi / 2) * k / n
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def M_(name, rgb, look='paint', tex=None, glow=0.0, r=None, m=None):
    rough = {'metal': 0.35, 'galvanized': 0.5, 'glow': 0.5}.get(look, 0.7)
    metal = {'metal': 0.9, 'galvanized': 0.7}.get(look, 0.0)
    return common.material(name, rgb, look=look, tex=tex, glow=glow, rough=rough, metal=metal, r=r, m=m)


def mats():
    """gemeinsame Materialien (nach common.reset neu anlegen)."""
    return {
        'galv': M_('verzinkt', (0.62, 0.63, 0.62), 'galvanized', r=1.25, m=0.6),
        'galv_dark': M_('verzinkt_alt', (0.46, 0.47, 0.46), 'galvanized', r=1.4, m=0.5),
        'rust': M_('rost', (0.33, 0.24, 0.17), 'metal', tex='metal_rusty_iron', r=1.4, m=0.2),
        'rust_light': M_('flugrost', (0.47, 0.41, 0.34), 'galvanized', r=1.45, m=0.35),
        'rust_stain': M_('rostfahne', (0.36, 0.28, 0.20), 'concrete', r=1.3),
        'steel': M_('stahl', (0.56, 0.56, 0.55), 'metal', r=0.8, m=0.85),
        'steel_dark': M_('stahl_dunkel', (0.28, 0.28, 0.27), 'metal', r=1.0, m=0.7),
        'black': M_('schwarz', (0.05, 0.05, 0.05), 'paint', r=1.1),
        'rubber': M_('gummi', (0.045, 0.045, 0.045), 'rubber'),
        'grime': M_('schmutz', (0.19, 0.16, 0.12), 'paint', r=1.45),
        'water': M_('tropfen', (0.10, 0.10, 0.09), 'water'),
        'paper': M_('papier', (0.94, 0.93, 0.88), 'sign'),
        'ink': M_('druck', (0.20, 0.18, 0.16), 'stencil'),
    }


def dull_screw(K, mat, c, w, r, h=None, seg=6, slot_mat=None):
    """Linsenkopf (Kuppel) bei c, Achse w (zeigt vom Bauteil weg)."""
    h = h if h is not None else r * 0.55
    F = frame(c, w)
    K.lathe(mat, [(r, 0.0), (r * 0.9, h * 0.6), (r * 0.5, h * 0.95), (0.0, h)], seg, F, smooth=True, sharp=60)
    if slot_mat is not None:
        K.box(slot_mat, (-r * 0.75, -r * 0.12, h * 0.55), (r * 0.75, r * 0.12, h * 1.05), F, skip=('-z',))


# =====================================================================================================================
# Leuchten
# =====================================================================================================================
def _batten(dead):
    """Feuchtraum-Wannenleuchte 1,5 m (Ursprung an der Decke, haengt an zwei Seilen 0,35 m tiefer)."""
    rng = random.Random(41 + dead)
    K = Kit()
    C = mats()
    hous = M_('wanne', (0.74, 0.74, 0.70), 'plastic', r=1.15)
    band = M_('wanne_kappe', (0.66, 0.66, 0.62), 'plastic', r=1.25)
    if dead:
        dif = M_('diffusor_tot', (0.52, 0.52, 0.49), 'plastic', r=0.85)
        dif_end = M_('diffusor_ende', (0.30, 0.29, 0.27), 'plastic', r=0.9)
    else:
        dif = M_('diffusor', (0.95, 1.0, 0.93), 'glow', glow=1.0)
        dif_end = dif
    clip = M_('edelstahl', (0.72, 0.72, 0.70), 'metal', tex='metal_aluminum', r=0.7, m=0.85)
    cable = M_('kabel', (0.11, 0.11, 0.11), 'rubber')
    fly = M_('fliegen', (0.05, 0.045, 0.04), 'plastic', r=1.3)
    # Wanne (Profil y,z; gegen den Uhrzeigersinn von +x gesehen)
    prof = [(-0.067, -0.410), (0.067, -0.410), (0.067, -0.404), (0.065, -0.400), (0.063, -0.356), (0.058, -0.350),
            (-0.058, -0.350), (-0.063, -0.356), (-0.065, -0.400), (-0.067, -0.404)]
    K.prism(hous, prof, -0.745, 0.745, axis='x', smooth=True, sharp=30)
    # Endkappen (etwas groesser, mit Fuge)
    big = [(y * 1.018, -0.380 + (z + 0.380) * 1.018) for (y, z) in prof]
    K.prism(band, big, -0.758, -0.737, axis='x', smooth=True, sharp=30, caps=(True, False))
    K.prism(band, big, 0.737, 0.758, axis='x', smooth=True, sharp=30, caps=(False, True))
    # Diffusor (opal), unten gewoelbt
    dprof = [(-0.058, -0.405), (-0.057, -0.422), (-0.050, -0.437), (-0.036, -0.447), (-0.013, -0.452), (0.013, -0.452),
             (0.036, -0.447), (0.050, -0.437), (0.057, -0.422), (0.058, -0.405)]
    if dead:
        K.prism(dif_end, dprof, -0.735, -0.6, axis='x', smooth=True, sharp=50, caps=(True, False))
        K.prism(dif, dprof, -0.6, 0.6, axis='x', smooth=True, sharp=50, caps=(False, False))
        K.prism(dif_end, dprof, 0.6, 0.735, axis='x', smooth=True, sharp=50, caps=(False, True))
    else:
        # Leuchtdichte: unter der Roehre (Mitte) am hellsten, zu den Seiten schwaecher
        side = M_('diffusor_seite', (0.86, 0.90, 0.84), 'glow', glow=0.45)
        mid = M_('diffusor_rand', (0.92, 0.97, 0.90), 'glow', glow=0.72)
        zone = {0: side, 1: mid, 2: mid, 3: dif, 4: dif, 5: dif, 6: mid, 7: mid, 8: side}
        n = len(dprof)
        segn = []
        for s in range(n - 1):
            (y0, z0), (y1, z1) = dprof[s], dprof[s + 1]
            L = math.hypot(y1 - y0, z1 - z0)
            segn.append(Vector((0, (z1 - z0) / L, -(y1 - y0) / L)))
        vn = [segn[0]] + [(segn[i - 1] + segn[i]).normalized() for i in range(1, n - 1)] + [segn[-1]]
        for s in range(n - 1):
            m_ = zone[s]
            pts, ns = [], []
            for (i, x) in ((s, -0.735), (s + 1, -0.735), (s + 1, 0.735), (s, 0.735)):
                pts.append(Vector((x, dprof[i][0], dprof[i][1])))
                ns.append(vn[i])
            K.face(m_, K.verts(m_, pts, ns), True)
        for x, rev in ((-0.735, True), (0.735, False)):
            P = [Vector((x, y, z)) for (y, z) in dprof]
            K.poly(side, P[::-1] if rev else P)
    # Klammern (Edelstahl), je Seite zwei
    cp = [(0.0540, -0.4170), (0.0690, -0.4170), (0.0690, -0.3920), (0.0665, -0.3920), (0.0665, -0.4145), (0.0540, -0.4145)]
    cm = [(-y, z) for (y, z) in reversed(cp)]
    for x in (-0.46, 0.46):
        K.prism(clip, cp, x - 0.011, x + 0.011, axis='x', caps=(False, False))
        K.prism(clip, cm, x - 0.011, x + 0.011, axis='x', caps=(False, False))
    # Montagebuegel, Seilhalter, Seile, Deckenhaken
    for x in (-0.6, 0.6):
        K.box(C['galv'], (x - 0.02, -0.045, -0.3505), (x + 0.02, 0.045, -0.346), skip=('-z',))
        K.cyl(clip, (x, 0, -0.346), (x, 0, -0.318), 0.0055, seg=6, caps=(False, True))
        K.cyl(clip, (x, 0, -0.318), (x + 0.002, 0.001, -0.004), 0.0016, seg=4, caps=(False, False))
        K.lathe(C['galv'], [(0.0, -0.009), (0.015, -0.006), (0.017, 0.0)], 6, T(x, 0, 0), smooth=True)
    # Kabelverschraubung an der Kappe, Anschlusskabel zur Decke
    K.cyl(C['black'], (0.758, 0.0, -0.384), (0.772, 0.0, -0.384), 0.0075, seg=6, caps=(False, True))
    path = [(0.772, 0.0, -0.384), (0.795, 0.004, -0.376), (0.812, 0.010, -0.340), (0.815, 0.016, -0.24), (0.806, 0.02, -0.11),
            (0.798, 0.021, 0.004)]
    K.sweep(cable, path, circle(0.0034, 4), smooth=True, sharp=89)
    # tote Insekten und Staub im Diffusor (dunkle Flecken an der Unterseite)
    nfly = 9 if dead else 5
    for k in range(nfly):
        x = rng.uniform(-0.68, 0.68) if k % 3 else rng.choice((-1, 1)) * rng.uniform(0.55, 0.7)
        y = rng.uniform(-0.02, 0.02)
        K.blob(fly, (x, y, -0.4526), (1, 0, 0), (0, -1, 0), rng.uniform(0.007, 0.013), rng.uniform(0.003, 0.006), 5, rng)
    # Staubschicht auf der Wanne (sieht man von schraeg oben und im Streiflicht an den Kanten)
    K.poly(M_('staub_wanne', (0.50, 0.49, 0.45), 'concrete', r=1.5),
           [(-0.70, -0.052, -0.3496), (0.70, -0.052, -0.3496), (0.68, 0.050, -0.3496), (-0.69, 0.051, -0.3496)], n=(0, 0, 1))
    # Typschild an der Kappe
    K.cell(C['paper'], (0.7585, 0.025, -0.385), (0, 0.012, 0), (0, 0, 0.008), 57)
    K.finish('batten')


def build_batten():
    _batten(0)


def build_batten__v1():
    _batten(1)


def build_highbay():
    """Hallenstrahler (Pendel an der Decke): Deckendose, Kugelgelenk, Pendelrohr mit Muffe, Vorschaltgeraet mit
    Kuehlrippen, Reflektor (aussen lackiert, innen facettiertes Aluminium), Glas mit Klemmring, Fangseil."""
    rng = random.Random(77)
    K = Kit()
    C = mats()
    housing = M_('gehaeuse_guss', (0.20, 0.21, 0.21), 'paint', tex='painted_metal_grey', r=1.25)
    refl_out = M_('reflektor_aussen', (0.25, 0.29, 0.27), 'paint', r=1.1)
    refl_dust = M_('reflektor_staub', (0.36, 0.37, 0.34), 'paint', r=1.45)
    refl_in = M_('reflektor_innen', (0.78, 0.78, 0.76), 'metal', tex='metal_aluminum', r=0.55, m=0.9)
    lens = M_('glas_leuchtend', (1.0, 0.88, 0.70), 'glow', glow=1.4)
    fly = M_('fliegen', (0.05, 0.045, 0.04), 'plastic', r=1.3)
    dust = M_('staub', (0.36, 0.34, 0.31), 'concrete', r=1.5)
    # Deckendose
    K.lathe(C['galv'], [(0.0, -0.034), (0.046, -0.034), (0.054, -0.029), (0.056, -0.006), (0.058, 0.0)], 12, smooth=True, sharp=35)
    for a in (0.6, 0.6 + math.pi):
        dull_screw(K, C['steel_dark'], (0.04 * math.cos(a), 0.04 * math.sin(a), -0.0342), (0, 0, -1), 0.005, seg=6)
    # Kugelgelenk
    ball = [(0.0, -0.066), (0.010, -0.064), (0.016, -0.056), (0.016, -0.046), (0.011, -0.037), (0.008, -0.034)]
    K.lathe(C['steel_dark'], ball, 8, smooth=True, sharp=70)
    # Pendelrohr mit Muffe
    K.cyl(C['galv'], (0, 0, -0.064), (0, 0, -0.590), 0.0105, seg=8, caps=(False, False))
    K.lathe(C['galv_dark'], [(0.0105, -0.330), (0.0138, -0.328), (0.0140, -0.292), (0.0105, -0.290)], 8, smooth=True, sharp=50)
    # Vorschaltgeraet (Guss), Rippen
    body = [(0.0105, -0.588), (0.032, -0.590), (0.066, -0.598), (0.084, -0.610), (0.090, -0.626), (0.090, -0.712),
            (0.098, -0.716), (0.100, -0.728)]
    K.lathe(housing, list(reversed(body)), 16, smooth=True, sharp=35)
    for k in range(10):
        a = TAU * k / 10 + 0.13
        K.prism(housing, [(0.088, -0.0022), (0.110, -0.0018), (0.110, 0.0018), (0.088, 0.0022)], -0.708, -0.630, R('z', a),
                caps=(False, True))
    # Klemmkasten seitlich, Typschild, Kabelverschraubung
    K.bbox(housing, (-0.03, 0.084, -0.70), (0.03, 0.118, -0.645), 0.004)
    K.cell(C['paper'], (0.0, 0.1186, -0.672), (-0.022, 0, 0), (0, 0, 0.016), 58)
    K.lathe(C['black'], [(0.009, 0.0), (0.009, 0.012), (0.0055, 0.016), (0.0, 0.016)], 6,
            frame((0.0, 0.118, -0.69), (0, 1, 0)), smooth=False)
    # Reflektor: aussen (vom Rand zum Hals -> Normale nach aussen), innen (Hals -> Rand, facettiert)
    rp = [(0.112, -0.728), (0.128, -0.758), (0.158, -0.808), (0.196, -0.866), (0.228, -0.918), (0.248, -0.954), (0.256, -0.972)]
    K.lathe(refl_out, [(r + 0.002, z) for (r, z) in reversed(rp[3:])], 18, smooth=True, sharp=60)
    K.lathe(refl_dust, [(r + 0.002, z) for (r, z) in reversed(rp[:4])], 18, smooth=True, sharp=60)
    K.lathe(refl_in, rp, 18, smooth=False)
    # Rollrand und Klemmring
    K.lathe(C['galv'], [(0.262, -0.986), (0.266, -0.981), (0.264, -0.975), (0.256, -0.972)], 20, smooth=True, sharp=60)
    K.lathe(C['galv_dark'], [(0.247, -0.9955), (0.2615, -0.9955), (0.2625, -0.9875), (0.2585, -0.9850)], 20, smooth=True, sharp=50)
    for k in range(3):
        a = TAU * k / 3 + 0.4
        F = frame((0.262 * math.cos(a), 0.262 * math.sin(a), -0.988), (math.cos(a), math.sin(a), 0))
        K.box(C['steel'], (-0.009, -0.004, 0.0), (0.009, 0.004, 0.006), F, skip=('-z',))
    # Glas (leicht gewoelbt), Insekten darin
    K.lathe(lens, [(0.0, -1.004), (0.11, -1.0015), (0.19, -0.998), (0.249, -0.9935)], 20, smooth=True, sharp=60)
    for k in range(6):
        a, d = rng.uniform(0, TAU), rng.uniform(0.02, 0.2)
        z = -1.004 + (d / 0.249) ** 2 * 0.0105 - 0.0004
        K.blob(fly, (d * math.cos(a), d * math.sin(a), z), (1, 0, 0), (0, -1, 0), rng.uniform(0.004, 0.009),
               rng.uniform(0.003, 0.005), 5, rng)
    # Staub auf dem Reflektorhals
    K.lathe(dust, [(0.1235, -0.7535), (0.1105, -0.7300)], 16, smooth=False)
    # Fangseil (Edelstahl) vom Gehaeuse zur Decke, durchhaengend
    path = [(0.07, 0.03, -0.60), (0.09, 0.05, -0.55), (0.10, 0.07, -0.42), (0.095, 0.08, -0.25), (0.08, 0.075, -0.10),
            (0.07, 0.07, -0.004)]
    K.sweep(C['steel'], path, circle(0.0016, 4), smooth=True, sharp=89)
    K.box(C['steel'], (0.062, 0.022, -0.606), (0.078, 0.038, -0.594))
    K.finish('highbay')


def build_safety_light():
    _safety_light(True)


def build_safety_light__v1():
    """V13.3: dieselbe Notleuchte ohne Notausgang-Piktogramm - Sicherungspunkte in Ebenen ohne Notausgangsschilder
    (STRUKTURTYP, signs.py); das gruene Licht bleibt das Zeichen fuer den sicheren Ort."""
    _safety_light(False)


def _safety_light(picto):
    """Notleuchte (Deckenaufbau, eigene Batterie): Gehaeuse, gruener Diffusor nach unten und seitlich, Endstuecke,
    Ladeanzeige, Pruefknopf, Piktogramm (nur picto), Schrauben, Typschild."""
    rng = random.Random(5)
    K = Kit()
    C = mats()
    hous = M_('notleuchte', (0.84, 0.83, 0.77), 'plastic', r=1.15)
    endp = M_('notleuchte_kappe', (0.80, 0.81, 0.79), 'plastic', r=1.2)
    dif = M_('diffusor_gruen', (0.30, 1.0, 0.50), 'glow', glow=1.4)
    led = M_('led_gruen', (0.4, 1.0, 0.5), 'glow', glow=3.0)
    red = M_('knopf_rot', (0.62, 0.07, 0.05), 'plastic', r=0.9)
    pict = M_('piktogramm', (1.0, 1.0, 1.0), 'sign')
    fly = M_('fliegen', (0.05, 0.045, 0.04), 'plastic', r=1.3)
    K.bbox(hous, (-0.19, -0.075, -0.085), (0.19, 0.075, 0.0), 0.012, seg=2)
    dp = [(-0.062, -0.080), (-0.062, -0.110), (-0.057, -0.124), (-0.043, -0.132), (-0.020, -0.1355), (0.020, -0.1355),
          (0.043, -0.132), (0.057, -0.124), (0.062, -0.110), (0.062, -0.080)]
    K.prism(dif, dp, -0.160, 0.160, axis='x', smooth=True, sharp=50)
    ep = [(y * 1.04, -0.108 + (z + 0.108) * 1.05) for (y, z) in dp]
    K.prism(endp, ep, -0.172, -0.158, axis='x', smooth=True, sharp=50)
    K.prism(endp, ep, 0.158, 0.172, axis='x', smooth=True, sharp=50)
    for x in (-0.165, 0.165):
        dull_screw(K, C['steel'], (x, 0.0, -0.1385), (0, 0, -1), 0.0045, seg=6)
    # Ladeanzeige und Pruefknopf an der Laengsseite (-y), Piktogramm auf beiden Laengsseiten
    K.lathe(led, [(0.0032, 0.0), (0.003, 0.002), (0.0, 0.0035)], 6, frame((0.12, -0.0752, -0.040), (0, -1, 0)), smooth=True)
    K.lathe(red, [(0.0055, 0.0), (0.005, 0.0040), (0.0, 0.0045)], 8, frame((0.145, -0.0752, -0.040), (0, -1, 0)), smooth=True)
    for s in ((-1, 1) if picto else ()):
        K.cell(pict, (-0.07, 0.0753 * s, -0.042), (-0.034 * s, 0, 0), (0, 0, 0.022), 38)
    K.cell(C['paper'], (0.1905, 0.0, -0.045), (0, 0.03, 0), (0, 0, 0.015), 59)
    for k in range(3):
        K.blob(fly, (rng.uniform(-0.12, 0.12), rng.uniform(-0.02, 0.02), -0.1360), (1, 0, 0), (0, -1, 0), rng.uniform(0.004, 0.007),
               0.003, 5, rng)
    K.finish('safety_light')


def _merge(K, G, M):
    """Geometrie aus Kit G mit M transformiert in Kit K uebernehmen."""
    G.xform(M)
    for (bm_src, mat, uvl, nl) in G.parts.values():
        bm_dst, _, uvl2, nl2 = K._get(mat)
        mp = {}
        for vv in bm_src.verts:
            nv = bm_dst.verts.new(vv.co)
            nv[nl2] = vv[nl]
            mp[vv] = nv
        for f in bm_src.faces:
            try:
                nf = bm_dst.faces.new([mp[x] for x in f.verts])
            except ValueError:
                continue
            nf.smooth = f.smooth
            for l1, l2 in zip(f.loops, nf.loops):
                l2[uvl2].uv = l1[uvl].uv
        bm_src.free()
    G.parts = {}


def _ceiling_vent(v):
    """Deckenschacht (Lager des Buendels): dunkle Oeffnung 0,6 x 0,6 m im Gehrungsrahmen mit Laibung; das
    Lamellengitter haengt an einer Kante aufgeklappt herab (v 0 weit, v 1 schraeg an einem Scharnier)."""
    rng = random.Random(90 + v)
    K = Kit()
    C = mats()
    frame_m = M_('rahmen', (0.62, 0.62, 0.60), 'paint', tex='painted_metal_white', r=1.2)
    grille_m = M_('gitter', (0.55, 0.55, 0.53), 'paint', tex='painted_metal_white', r=1.3)
    grime_l = M_('gitter_verstaubt', (0.40, 0.38, 0.34), 'paint', r=1.45)
    dark = M_('schacht', (0.008, 0.008, 0.008), 'plastic', r=1.5)
    w = 0.3
    K.poly(dark, [(-w, -w, -0.0042), (-w, w, -0.0042), (w, w, -0.0042), (w, -w, -0.0042)])
    # Rahmen: Querschnitt (Abstand von der Mitte, Hoehe) gegen den Uhrzeigersinn; Ecken auf Gehrung
    prof = [(0.300, -0.030), (0.302, -0.030), (0.302, -0.012), (0.304, -0.010), (0.327, -0.009), (0.330, -0.006),
            (0.330, -0.002), (0.300, -0.002)]
    rows = []
    for (s, z) in prof + prof[:1]:
        rows.append([Vector((s * sx, s * sy, z)) for (sx, sy) in ((1, -1), (1, 1), (-1, 1), (-1, -1))])
    K.grid(frame_m, rows, closed=True, smooth=False)
    # Eckschrauben (eine fehlt: Loch)
    for i, (sx, sy) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
        c = (0.316 * sx, 0.316 * sy, -0.0092)
        if (v == 0 and i == 2) or (v == 1 and i == 1):
            K.lathe(dark, [(0.0035, 0.0), (0.0, 0.0)], 6, frame(c, (0, 0, -1)), smooth=False)
        else:
            dull_screw(K, C['steel'], c, (0, 0, -1), 0.0048, seg=6, slot_mat=C['steel_dark'])
    # Schmutzfahnen am Rahmen (Luftzug)
    for k in range(3):
        a = rng.uniform(0, TAU)
        sx, sy = math.cos(a), math.sin(a)
        m = max(abs(sx), abs(sy))
        K.blob(C['grime'], (0.316 * sx / m, 0.316 * sy / m, -0.0093), (1, 0, 0), (0, -1, 0), rng.uniform(0.03, 0.07), 0.009, 6, rng)
    # Russ- und Staubhof an der Decke um den Rahmen (Luft streicht aus dem Schacht), unregelmaessig
    halo = M_('deckenschmutz', (0.17, 0.16, 0.14), 'concrete', r=1.4)
    for (cx, cy, rx, ry) in ((0.0, -0.345, 0.22, 0.02), (0.345, 0.05, 0.02, 0.18), (-0.1, 0.345, 0.15, 0.015),
                             (-0.345, -0.12, 0.016, 0.12)):
        K.blob(halo, (cx, cy, -0.0006), (1, 0, 0), (0, -1, 0), rx * rng.uniform(0.8, 1.1), ry, 8, rng, irr=0.25)
    # Gitter (eigenes Kit): lokal y 0 .. 2w (Scharnierkante y = 0), schraege Lamellen, Unterseite = -z
    G = Kit()
    t = 0.012
    for (lo, hi) in (((-w, 0.0, -t), (w, 0.02, 0.0)), ((-w, 2 * w - 0.02, -t), (w, 2 * w, 0.0)),
                     ((-w, 0.02, -t), (-w + 0.02, 2 * w - 0.02, 0.0)), ((w - 0.02, 0.02, -t), (w, 2 * w - 0.02, 0.0))):
        G.box(grille_m, lo, hi)
    missing = {7} if v == 1 else set()
    x0, x1 = -w + 0.02, w - 0.02
    for j in range(12):
        if j in missing:
            continue
        y = 0.05 + j * 0.0435
        a = 0.75 + (0.35 if (j == 4 and v == 0) else 0.0)
        p0 = Vector((0, y, -0.002))
        q = p0 + Vector((0, math.cos(a), -math.sin(a))) * 0.016
        e = q + Vector((0, 0.004, -0.003))
        lm = grime_l if j % 3 == 1 else grille_m
        G.poly(lm, [(x0, p0.y, p0.z), (x1, p0.y, p0.z), (x1, q.y, q.z), (x0, q.y, q.z)], two=True)
        G.poly(lm, [(x0, q.y, q.z), (x1, q.y, q.z), (x1, e.y, e.z), (x0, e.y, e.z)], two=True)
    # Staubfahnen auf den Rahmenleisten des Gitters (Unterseite)
    for (y0, y1) in ((0.0, 0.02), (2 * w - 0.02, 2 * w)):
        xa = rng.uniform(-0.25, 0.0)
        G.poly(C['grime'], [(xa, y0 + 0.002, -t - 0.0006), (xa + rng.uniform(0.12, 0.25), y0 + 0.002, -t - 0.0006),
                            (xa + 0.2, y1 - 0.002, -t - 0.0006), (xa + 0.03, y1 - 0.002, -t - 0.0006)], n=(0, 0, -1))
    for x in (-0.2, 0.2):
        G.cyl(C['steel_dark'], (x - 0.025, -0.004, -0.006), (x + 0.025, -0.004, -0.006), 0.0055, seg=6)
    open_ = 1.15 if v == 0 else 0.85
    twist = 0.05 if v == 0 else 0.22
    _merge(K, G, T(0, -w - 0.004, -0.008) @ R('y', twist) @ R('x', -open_))
    # Scharnierlappen am Rahmen
    for x in (-0.2, 0.2):
        K.box(C['steel_dark'], (x - 0.022, -w - 0.006, -0.012), (x + 0.022, -w + 0.004, -0.0095), skip=('+z',))
    K.finish('ceiling_vent')


def build_ceiling_vent():
    _ceiling_vent(0)


def build_ceiling_vent__v1():
    _ceiling_vent(1)


# =====================================================================================================================
# Moebel
# =====================================================================================================================
def bar(K, mat, p0, p1, a, b, up=(0, 0, 1), caps=(False, False)):
    """Rechteckstab (halbe Masse a entlang up, b quer) von p0 nach p1."""
    K.sweep(mat, [p0, p1], [(-a, -b), (a, -b), (a, b), (-a, b)], up=up, smooth=False, caps=caps)


def _chair(v):
    """Holzstuhl (Buche, lackiert): konische Beine mit Fasen, hintere Beine durchgehend und nach hinten geneigt,
    Zarge, Sitz mit abgegriffener Vorderkante, zwei gebogene Lehnenbretter, Streben, Gleiter, schmutzige Fuesse."""
    rng = random.Random(16 + v)
    K = Kit()
    C = mats()
    wood = M_('buche_lack', (0.92, 0.80, 0.66), 'wood', tex='wood_walnut_floor', r=0.8)
    worn = M_('buche_abgegriffen', (1.0, 0.90, 0.76), 'wood', tex='wood_walnut_floor', r=1.15)
    foot = M_('holz_fuss', (0.50, 0.42, 0.34), 'wood', tex='wood_walnut_floor', r=1.25)
    glide = M_('gleiter', (0.08, 0.08, 0.08), 'plastic', r=0.9)
    felt = M_('filz', (0.36, 0.30, 0.24), 'fabric')
    leg = rrect(0.017, 0.017, 0.005, n=1)

    def legsweep(pts, sc):
        # untere 5 cm dunkel (Wischwasser, Schmutz), darueber lackiert
        K.sweep(foot, pts[:2], leg, scales=sc[:2], smooth=True, sharp=50)
        K.sweep(wood, pts[1:], leg, scales=sc[1:], smooth=True, sharp=50, caps=(False, True))

    for y in (-0.19, 0.19):
        legsweep([(0.19, y, 0.011), (0.19, y, 0.058), (0.19, y, 0.432)], [0.84, 0.86, 1.0])
        legsweep([(-0.197, y, 0.011), (-0.191, y, 0.058), (-0.188, y, 0.44), (-0.198, y, 0.62), (-0.221, y, 0.94)],
                 [0.84, 0.86, 1.0, 0.96, 0.84])
    for i, (x, y) in enumerate(((0.19, -0.19), (0.19, 0.19), (-0.197, -0.19), (-0.197, 0.19))):
        prof = [(0.0, 0.0), (0.0135, 0.0005), (0.0145, 0.006), (0.013, 0.0115)]
        K.lathe(felt if i == 1 else glide, prof, 8, T(x, y, 0), smooth=True, sharp=60)
    # Sitz, Zarge, Streben
    K.bbox(wood, (-0.212, -0.228, 0.432), (0.234, 0.228, 0.457), 0.008, seg=2)
    K.blob(worn, (0.15, 0.0, 0.4573), (1, 0, 0), (0, 1, 0), 0.05, 0.17, 9, rng, irr=0.2)
    K.blob(worn, (-0.02, 0.0, 0.4573), (1, 0, 0), (0, 1, 0), 0.12, 0.12, 8, rng, irr=0.3)
    for (lo, hi) in (((0.166, -0.172, 0.372), (0.184, 0.172, 0.432)), ((-0.186, -0.172, 0.372), (-0.168, 0.172, 0.432)),
                     ((-0.172, -0.186, 0.372), (0.172, -0.168, 0.432)), ((-0.172, 0.168, 0.372), (0.172, 0.186, 0.432))):
        K.box(wood, lo, hi, skip=('+z',))
    sr = circle(0.0095, 8)
    for y in (-0.19, 0.19):
        K.sweep(wood, [(0.19, y, 0.15), (-0.19, y, 0.15)], sr, smooth=True, sharp=89)
    K.sweep(wood, [(0.19, -0.19, 0.23), (0.19, 0.19, 0.23)], sr, smooth=True, sharp=89)
    K.sweep(wood, [(-0.19, -0.19, 0.23), (-0.19, 0.19, 0.23)], sr, smooth=True, sharp=89)
    # Lehnenbretter, im Grundriss gebogen (Mitte weiter hinten)
    for (zc, hh, xe) in ((0.865, 0.042, -0.208), (0.685, 0.026, -0.197)):
        path = []
        for k in range(7):
            y = -0.205 + 0.41 * k / 6
            path.append((xe - 0.016 * (1 - (y / 0.205) ** 2), y, zc))
        K.sweep(wood, path, rrect(hh, 0.008, 0.004, n=1), up=(0, 0, 1), smooth=True, sharp=50)
    # Kratzer an der Lehne, Abrieb oben
    K.blob(worn, (-0.2325, 0.0, 0.875), (0, -1, 0), (0, 0, 1), 0.03, 0.003, 6, rng)
    K.blob(worn, (-0.2315, -0.05, 0.85), (0, -1, 0), (0, 0, 1), 0.02, 0.0025, 6, rng)
    if v == 1:  # umgekippt: liegt auf der Seite
        K.xform(R('z', 0.35) @ R('x', math.pi / 2))
        lo, hi = K.bounds()
        K.xform(T(0, 0, -lo.z))
    K.finish('chair')


def build_chair():
    _chair(0)


def build_chair__v1():
    _chair(1)


def loft(K, mat, secs, n=2, caps=(True, True), smooth=True, sharp=50.0):
    """Querschnitte (x, cy, cz, halbe Breite, halbe Hoehe, Rundung) entlang +x verbinden."""
    rows, nrows = [], []
    base = None
    for (x, cy, cz, hw, hh, r) in secs:
        pts = rrect(hw, hh, r, n)
        rows.append([Vector((x, cy + u, cz + w)) for (u, w) in pts])
    K.grid(mat, rows, closed=True, smooth=False)
    if caps[0]:
        K.poly(mat, rows[0][::-1])
    if caps[1]:
        K.poly(mat, rows[-1])
    return base


def _keycap(K, mat, c, hw, hd, h):
    """Taste: Pyramidenstumpf ohne Boden (c = Mitte unten)."""
    x, y, z = c
    b = [(x - hd, y - hw, z), (x + hd, y - hw, z), (x + hd, y + hw, z), (x - hd, y + hw, z)]
    t = [(x - hd * 0.78 - 0.001, y - hw * 0.8, z + h), (x + hd * 0.78 - 0.001, y - hw * 0.8, z + h + 0.0012),
         (x + hd * 0.78 - 0.001, y + hw * 0.8, z + h + 0.0012), (x - hd * 0.78 - 0.001, y + hw * 0.8, z + h)]
    vs = K.verts(mat, [Vector(p) for p in b + t])
    for idx in ((4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
        K.face(mat, [vs[i] for i in idx])


def _swivel_chair(G, rng):
    """Buerodrehstuhl der 80er (Rueckenlehne bei +x): Fuenfstern mit Rollen, Gasfeder, Polster, Lehnenbuegel."""
    C = mats()
    fab = M_('stoff_buero', (0.16, 0.17, 0.20), 'fabric')
    foam = M_('schaumstoff', (0.70, 0.62, 0.40), 'fabric')
    plast = M_('kunststoff_schwarz', (0.06, 0.06, 0.065), 'plastic', r=1.0)
    chrome = M_('chrom', (0.75, 0.75, 0.74), 'metal', r=0.35, m=0.95)
    G.lathe(plast, [(0.0, 0.052), (0.042, 0.052), (0.05, 0.068), (0.046, 0.094), (0.026, 0.102)], 10, smooth=True, sharp=50)
    for k in range(5):
        a = TAU * k / 5 + 0.3
        ca, sa = math.cos(a), math.sin(a)
        G.sweep(plast, [(0.03 * ca, 0.03 * sa, 0.078), (0.262 * ca, 0.262 * sa, 0.058)], rrect(0.014, 0.020, 0.006, n=1),
                scales=[1.0, 0.72], smooth=True, sharp=50, caps=(False, True))
        # Rolle: Gehaeuse, Achse quer, zwei Raeder
        cx, cy = 0.262 * ca, 0.262 * sa
        b = rng.uniform(0, TAU)
        G.bbox(plast, (cx - 0.016, cy - 0.012, 0.03), (cx + 0.016, cy + 0.012, 0.052), 0.004, M=None)
        for s in (-1, 1):
            wc = Vector((cx + 0.009 * math.cos(b), cy + 0.009 * math.sin(b), 0.025)) + Vector((-math.sin(b), math.cos(b), 0)) * (0.011 * s)
            G.lathe(C['rubber'], [(0.0, -0.0055), (0.023, -0.0055), (0.025, 0.0), (0.023, 0.0055), (0.0, 0.0055)], 8,
                    frame(wc, (-math.sin(b), math.cos(b), 0)), smooth=True, sharp=60)
    G.cyl(plast, (0, 0, 0.10), (0, 0, 0.30), 0.027, seg=10, caps=(False, True))
    G.cyl(chrome, (0, 0, 0.30), (0, 0, 0.408), 0.0145, seg=10, caps=(False, False))
    G.box(plast, (-0.10, -0.10, 0.405), (0.10, 0.10, 0.432))
    G.sweep(plast, [(-0.03, 0.1, 0.42), (-0.04, 0.17, 0.418), (-0.05, 0.2, 0.41)], circle(0.005, 6), smooth=True, caps=(False, True))
    G.bbox(fab, (-0.215, -0.22, 0.432), (0.215, 0.22, 0.50), 0.03, seg=2)
    G.blob(foam, (-0.19, 0.13, 0.5004), (1, 0, 0), (0, 1, 0), 0.035, 0.025, 7, rng)
    G.sweep(C['steel_dark'], [(0.09, 0.0, 0.418), (0.20, 0.0, 0.42), (0.222, 0.0, 0.47), (0.226, 0.0, 0.64)],
            [(-0.024, -0.004), (0.024, -0.004), (0.024, 0.004), (-0.024, 0.004)], up=(0, 1, 0), smooth=False)
    G.bbox(fab, (0.205, -0.20, 0.60), (0.248, 0.20, 0.93), 0.022, M=T(0.226, 0, 0.60) @ R('y', 0.10) @ T(-0.226, 0, -0.60), seg=2)


def _desk(v):
    """Bueroschreibtisch mit Roehrenmonitor: Platte mit Umleimer, Vierkantbeine, Zarge, Sichtblende, haengender
    Schubladenblock, Monitor (Rundgehaeuse, gewoelbte Scheibe, Lueftungsschlitze, Schalter, Kabel), Tastatur mit
    einzelnen Tasten, Papiere, Tasse, Kaffeeringe; Drehstuhl (v 0, 1), v 2 ohne Stuhl mit Papier am Boden."""
    rng = random.Random(230 + v)
    K = Kit()
    C = mats()
    top = M_('tischplatte', (0.60, 0.57, 0.50), 'plastic', r=1.1)
    edge = M_('umleimer', (0.22, 0.17, 0.12), 'plastic', r=0.9)
    steel = M_('tischgestell', (0.27, 0.27, 0.28), 'paint', r=1.1)
    drawer = M_('schublade', (0.50, 0.50, 0.48), 'paint', r=1.1)
    crt = M_('roehre_gehaeuse', (0.66, 0.62, 0.52), 'plastic', r=1.15)
    bez = M_('roehre_rahmen', (0.70, 0.66, 0.56), 'plastic', r=1.1)
    scr = M_('bildschirm', (0.07, 0.085, 0.08), 'glass', r=0.35)
    kb = M_('tastatur', (0.68, 0.64, 0.55), 'plastic', r=1.0)
    key = M_('taste', (0.76, 0.73, 0.65), 'plastic', r=0.9)
    keyd = M_('taste_dunkel', (0.44, 0.43, 0.40), 'plastic', r=0.9)
    vent = M_('schlitz', (0.04, 0.04, 0.04), 'plastic', r=1.5)
    sheet = M_('papier_blank', (0.88, 0.87, 0.82), 'plastic', r=1.45)
    mug = M_('tasse', (0.84, 0.83, 0.79), 'ceramic', r=0.8)
    coffee = M_('kaffee', (0.16, 0.09, 0.05), 'plastic', r=0.6)
    ring = M_('kaffeerand', (0.36, 0.27, 0.18), 'plastic', r=1.0)
    folder = M_('ordner', (0.13, 0.20, 0.36), 'plastic', r=1.2)
    # Platte
    K.poly(top, [(-0.35, -0.7, 0.75), (0.35, -0.7, 0.75), (0.35, 0.7, 0.75), (-0.35, 0.7, 0.75)])
    K.box(edge, (-0.35, -0.7, 0.722), (0.35, 0.7, 0.75), skip=('+z',))
    # Beine, Fusskappen, Zarge, Sichtblende
    for x in (-0.31, 0.31):
        for y in (-0.66, 0.66):
            K.prism(steel, rrect(0.02, 0.02, 0.003, n=1), 0.012, 0.722, T(x, y, 0), smooth=True, sharp=50, caps=(False, False))
            K.box(C['black'], (x - 0.021, y - 0.021, 0.0), (x + 0.021, y + 0.021, 0.012), skip=('-z',))
        K.box(steel, (x - 0.01, -0.64, 0.672), (x + 0.01, 0.64, 0.722), skip=('+z',))
    for y in (-0.66, 0.66):
        K.box(steel, (-0.29, y - 0.01, 0.672), (0.29, y + 0.01, 0.722), skip=('+z',))
    K.box(steel, (-0.318, -0.64, 0.36), (-0.314, 0.64, 0.672))
    # Schubladenblock (haengend, rechts), Griffe, Schloss; v 2: obere Schublade 6 cm offen
    K.box(steel, (-0.29, 0.28, 0.42), (0.325, 0.64, 0.722), skip=('+z', '+x'))
    K.poly(C['black'], [(0.325, 0.29, 0.425), (0.325, 0.63, 0.425), (0.325, 0.63, 0.717), (0.325, 0.29, 0.717)])
    out = 0.06 if v == 2 else 0.0
    for (z0, z1, dx) in ((0.575, 0.716, out), (0.426, 0.569, 0.0)):
        K.bbox(drawer, (0.327 + dx, 0.288, z0), (0.345 + dx, 0.632, z1), 0.003)
        zc = (z0 + z1) / 2 + 0.03
        K.sweep(C['steel'], [(0.345 + dx, 0.41, zc), (0.368 + dx, 0.41, zc), (0.368 + dx, 0.51, zc), (0.345 + dx, 0.51, zc)],
                circle(0.0045, 6), smooth=True, sharp=60)
        if dx > 0:
            for y in (0.29, 0.628):
                K.box(drawer, (0.25, y, z0 + 0.01), (0.327 + dx, y + 0.003, z1 - 0.02))
    K.lathe(C['steel'], [(0.007, 0.0), (0.007, 0.003), (0.0, 0.0035)], 8, frame((0.345 + out, 0.6, 0.69), (1, 0, 0)), smooth=False)
    # Monitor (auf Drehfuss)
    mon = Kit()
    mon.lathe(crt, [(0.0, 0.0), (0.11, 0.0), (0.116, 0.006), (0.10, 0.016), (0.05, 0.022), (0.04, 0.03)], 12, smooth=True, sharp=50)
    mon.box(crt, (-0.05, -0.04, 0.02), (0.05, 0.04, 0.045), skip=('-z',))
    loft(mon, crt, [(-0.205, 0.0, 0.195, 0.105, 0.10, 0.02), (-0.12, 0.0, 0.20, 0.15, 0.15, 0.025),
                    (-0.03, 0.0, 0.205, 0.18, 0.165, 0.025), (0.165, 0.0, 0.208, 0.19, 0.172, 0.024)], caps=(True, False))
    outer = rrect(0.19, 0.172, 0.024, n=2)
    inner = rrect(0.152, 0.126, 0.03, n=2)
    rws = [[Vector((0.165, u, 0.208 + w)) for (u, w) in outer], [Vector((0.193, u * 0.99, 0.208 + w * 0.99)) for (u, w) in outer],
           [Vector((0.201, u * 1.03, 0.208 + w * 1.03)) for (u, w) in inner], [Vector((0.189, u, 0.208 + w)) for (u, w) in inner]]
    mon.grid(bez, rws, closed=True, smooth=False)
    srows = [[Vector((0.189 + 0.011 * (1 - s * s), u * s, 0.208 + w * s)) for (u, w) in inner] for s in (1.0, 0.7, 0.35, 0.0)]
    mon.grid(scr, srows, closed=True, smooth=False)
    for j in range(6):
        x = -0.115 + j * 0.016
        zt = 0.20 + 0.15 + (x + 0.12) / 0.09 * 0.02 + 0.0012 if x < -0.03 else 0.37
        mon.poly(vent, [(x, -0.12, zt), (x + 0.006, -0.12, zt + 0.0013), (x + 0.006, 0.12, zt + 0.0013), (x, 0.12, zt)])
    mon.bbox(C['black'], (0.198, 0.115, 0.055), (0.206, 0.14, 0.07), 0.002)
    mon.lathe(M_('led_aus', (0.05, 0.12, 0.06), 'plastic', r=0.5), [(0.003, 0.0), (0.0, 0.002)], 6, frame((0.2012, 0.095, 0.0625), (1, 0, 0)), smooth=False)
    mon.box(C['steel'], (0.199, -0.03, 0.054), (0.2015, 0.03, 0.064))
    mon_rot = {0: 0.0, 1: -0.10, 2: 0.22}[v]
    _merge(K, mon, T(-0.02, 0.0, 0.75) @ R('z', mon_rot))
    K.sweep(C['black'], [(-0.205, 0.05, 0.89), (-0.25, 0.06, 0.865), (-0.31, 0.08, 0.80), (-0.338, 0.10, 0.60), (-0.342, 0.12, 0.30),
                         (-0.33, 0.14, 0.004)], circle(0.004, 5), smooth=True, sharp=89)
    # Tastatur
    kbk = Kit()
    wedge = [(0.0, 0.0), (0.140, 0.0), (0.142, 0.012), (0.136, 0.019), (0.0, 0.033)]
    kbk.prism(kb, [(z, x) for (x, z) in reversed(wedge)], -0.225, 0.225, axis='y', caps=(True, True))
    for r_ in range(5):
        x = 0.026 + r_ * 0.0215
        zr = 0.033 - (0.033 - 0.017) * (x / 0.136)
        nkeys = 13 if r_ < 4 else 0
        for kk in range(nkeys):
            y = -0.205 + kk * 0.0195 + (0.006 if r_ in (1, 3) else 0.0)
            _keycap(kbk, keyd if (kk == 0 or kk >= 12) else key, (x, y, zr), 0.0082, 0.0085, 0.008)
        if r_ == 4:
            _keycap(kbk, key, (x, -0.06, zr), 0.06, 0.0085, 0.007)
            for kk in range(3):
                _keycap(kbk, keyd, (x, -0.2 + kk * 0.03, zr), 0.012, 0.0085, 0.007)
        for kk in range(4):
            _keycap(kbk, keyd if r_ else key, (x, 0.12 + kk * 0.0195, zr), 0.0082, 0.0085, 0.008)
    kb_rot = {0: 0.04, 1: -0.12, 2: 0.25}[v]
    _merge(K, kbk, T(0.205, -0.02 if v != 2 else 0.05, 0.75) @ R('z', kb_rot))
    K.sweep(C['black'], [(0.205, 0.0, 0.765), (0.19, 0.03, 0.7515), (0.16, 0.12, 0.752), (0.12, 0.21, 0.7518)], circle(0.0025, 4),
            smooth=True, sharp=89)
    # Papiere: Stapel (oberstes Blatt beschrieben), bei v 2 auch am Boden
    def paper_stack(cx, cy, z, n, rot0):
        for k in range(n):
            a = rot0 + rng.uniform(-0.12, 0.12)
            ca, sa = math.cos(a), math.sin(a)
            c = Vector((cx + rng.uniform(-0.01, 0.01), cy + rng.uniform(-0.01, 0.01), z + 0.0006 * (k + 1)))
            r_ = Vector((sa, -ca, 0)) * 0.105
            u_ = Vector((ca, sa, 0)) * 0.1485
            if k == n - 1:
                K.cell(C['paper'], c, r_, u_, 56 + rng.randrange(4))
            else:
                K.poly(sheet, [c - r_ + u_, c - r_ - u_, c + r_ - u_, c + r_ + u_])

    paper_stack(-0.12, 0.42, 0.75, 4, 0.1)
    if v == 1:
        paper_stack(0.12, 0.47, 0.75, 2, -0.5)
        K.bbox(folder, (-0.30, -0.62, 0.75), (-0.02, -0.30, 0.79), 0.004)
        K.box(C['paper'], (-0.299, -0.31, 0.752), (-0.021, -0.297, 0.788))
    if v == 2:
        for k in range(3):
            paper_stack(rng.uniform(-0.25, 0.25), rng.uniform(-0.55, 0.0), 0.0005 + 0.0004 * k, 1, rng.uniform(0, 3))
    # Tasse (bei v 2 umgekippt), Kaffeeringe, Stift
    mp = [(0.0, 0.0), (0.036, 0.0), (0.040, 0.006), (0.041, 0.092), (0.0385, 0.095), (0.036, 0.092), (0.035, 0.03), (0.0, 0.03)]
    Mm = T(0.22, -0.42, 0.75) if v != 2 else T(0.18, -0.40, 0.7905) @ R('z', 0.8) @ R('x', math.pi / 2) @ T(0, 0, -0.045)
    K.lathe(mug, mp, 12, Mm, smooth=True, sharp=50)
    K.lathe(coffee, [(0.035, 0.032), (0.0, 0.032)], 12, Mm, smooth=False)
    K.sweep(mug, [(0.040, 0, 0.075), (0.062, 0, 0.07), (0.066, 0, 0.045), (0.058, 0, 0.022), (0.040, 0, 0.02)], circle(0.0055, 6),
            M=Mm, smooth=True, sharp=70)
    rings = [(0.14, -0.50), (0.27, -0.25)] + ([(0.05, -0.45)] if v == 1 else [])
    for (x, y) in rings:
        K.lathe(ring, [(0.041, 0.0), (0.0365, 0.0)], 16, T(x, y, 0.7503), smooth=False)
    if v == 2:
        K.blob(coffee, (0.24, -0.33, 0.7504), (1, 0, 0), (0, 1, 0), 0.07, 0.04, 9, rng)
    K.sweep(M_('stift', (0.12, 0.20, 0.55), 'plastic', r=0.8), [(0.10, 0.27, 0.7545), (0.24, 0.30, 0.7545)], circle(0.0042, 6),
            smooth=True, sharp=89, caps=(True, True))
    # Drehstuhl
    if v != 2:
        G = Kit()
        _swivel_chair(G, rng)
        _merge(K, G, T(0.665, 0.0, 0.0) if v == 0 else T(0.69, -0.07, 0.0) @ R('z', 0.45))
    K.finish('desk')


def build_desk_crt():
    _desk(0)


def build_desk_crt__v1():
    _desk(1)


def build_desk_crt__v2():
    _desk(2)


def _side_table(v):
    """Runder Beistelltisch (Nussbaum, poliert): Platte mit Profilkante, gedrechselte Saeule, drei geschwungene
    Fuesse mit Messingschuhen, Wasserringe und Kratzer; v 1 mit Tischlampe (Messingfuss, Faltenschirm, Kabel)."""
    rng = random.Random(1198 + v)
    K = Kit()
    C = mats()
    wood = M_('nussbaum_poliert', (0.78, 0.62, 0.50), 'wood', tex='wood_walnut_floor', r=0.55)
    wood_d = M_('nussbaum_dunkel', (0.58, 0.46, 0.38), 'wood', tex='wood_walnut_floor', r=0.7)
    worn = M_('nussbaum_abgerieben', (0.95, 0.80, 0.66), 'wood', tex='wood_walnut_floor', r=1.0)
    water = M_('wasserrand', (0.56, 0.50, 0.42), 'wood', r=1.3)
    brass = M_('messing', (0.62, 0.46, 0.22), 'metal', r=0.7, m=0.8)
    # Platte: Unterseite -> Kante -> Oberseite (gegen den Uhrzeigersinn), Kante abgerieben
    K.lathe(wood_d, [(0.055, 0.628), (0.284, 0.628), (0.294, 0.631)], 32, smooth=True, sharp=40)
    K.lathe(worn, [(0.294, 0.631), (0.300, 0.638), (0.300, 0.648), (0.297, 0.655), (0.289, 0.660)], 32, smooth=True, sharp=40)
    K.lathe(wood, [(0.289, 0.660), (0.0, 0.660)], 32, smooth=False)
    for k in range(2 + v):
        a, d = rng.uniform(0, TAU), rng.uniform(0.08, 0.2)
        rr = rng.uniform(0.032, 0.042)
        K.lathe(water, [(rr, 0.0), (rr - 0.004, 0.0)], 18, T(d * math.cos(a), d * math.sin(a), 0.6603), smooth=False)
    for k in range(3):
        a = rng.uniform(0, TAU)
        c = Vector((0.15 * math.cos(a), 0.15 * math.sin(a), 0.6603))
        K.blob(worn, c, (math.cos(a + 1.2), math.sin(a + 1.2), 0), (-math.sin(a + 1.2), math.cos(a + 1.2), 0),
               rng.uniform(0.03, 0.06), 0.0012, 4, rng, irr=0.1)
    # Saeule (gedrechselt)
    col = [(0.0, 0.098), (0.045, 0.10), (0.046, 0.195), (0.051, 0.205), (0.034, 0.245), (0.040, 0.31), (0.048, 0.36),
           (0.030, 0.46), (0.024, 0.50), (0.036, 0.53), (0.026, 0.57), (0.050, 0.60), (0.050, 0.628)]
    K.lathe(wood, col, 12, smooth=True, sharp=60)
    # drei geschwungene Fuesse
    for k in range(3):
        a = TAU * k / 3 + 0.5
        ca, sa = math.cos(a), math.sin(a)
        pts = [(0.03, 0.18), (0.075, 0.15), (0.13, 0.10), (0.18, 0.05), (0.205, 0.024), (0.215, 0.016)]
        K.sweep(wood, [(r * ca, r * sa, z) for (r, z) in pts], rrect(0.022, 0.014, 0.006, n=1),
                scales=[1.0, 0.95, 0.82, 0.7, 0.62, 0.6], smooth=True, sharp=50, caps=(False, True))
        K.lathe(brass, [(0.0, 0.0), (0.012, 0.0), (0.013, 0.010), (0.010, 0.019)], 8, T(0.214 * ca, 0.214 * sa, 0.0),
                smooth=True, sharp=60)
    if v == 1:
        cloth = M_('lampenschirm', (0.90, 0.80, 0.60), 'fabric')
        trim = M_('schirmborte', (0.62, 0.50, 0.32), 'fabric')
        bulb = M_('birne', (0.92, 0.90, 0.84), 'glass', r=0.6)
        K.lathe(brass, [(0.0, 0.660), (0.075, 0.6605), (0.080, 0.668), (0.070, 0.678), (0.040, 0.690), (0.025, 0.71),
                        (0.020, 0.73), (0.012, 0.74)], 16, smooth=True, sharp=50)
        K.cyl(brass, (0, 0, 0.74), (0, 0, 0.95), 0.009, seg=8, caps=(False, False))
        K.lathe(C['black'], [(0.009, 0.95), (0.018, 0.955), (0.018, 0.99), (0.012, 0.995)], 10, smooth=True, sharp=50)
        K.lathe(bulb, [(0.012, 0.995), (0.028, 1.02), (0.030, 1.045), (0.018, 1.065), (0.0, 1.07)], 10, smooth=True, sharp=70)
        rows = []
        for (z, r) in ((0.925, 0.165), (1.125, 0.092)):
            rows.append([Vector(((r * (1.0 + (0.035 if k % 2 else -0.01))) * math.cos(TAU * k / 40),
                                 (r * (1.0 + (0.035 if k % 2 else -0.01))) * math.sin(TAU * k / 40), z)) for k in range(40)])
        K.grid(cloth, rows, closed=True, smooth=False)
        K.grid(M_('lampenschirm_innen', (0.80, 0.70, 0.50), 'fabric'), [[p * 0.995 + Vector((0, 0, p.z * 0.005)) for p in r_[::-1]]
                                                                     for r_ in rows], closed=True, smooth=False)
        K.lathe(trim, [(0.168, 0.920), (0.171, 0.925), (0.168, 0.932)], 20, smooth=True, sharp=80)
        K.lathe(trim, [(0.092, 1.118), (0.096, 1.125), (0.092, 1.131)], 16, smooth=True, sharp=80)
        for k in range(3):
            a = TAU * k / 3
            K.cyl(brass, (0.018 * math.cos(a), 0.018 * math.sin(a), 0.99), (0.09 * math.cos(a), 0.09 * math.sin(a), 1.123),
                  0.0018, seg=4, caps=(False, False))
        K.lathe(brass, [(0.0, 1.122), (0.008, 1.125), (0.006, 1.14), (0.0, 1.148)], 8, smooth=True, sharp=60)
        cord = [(-0.06, 0.03, 0.6645), (-0.2, 0.08, 0.6635), (-0.288, 0.10, 0.663), (-0.303, 0.105, 0.645), (-0.306, 0.11, 0.40),
                (-0.298, 0.12, 0.05), (-0.28, 0.16, 0.004), (-0.2, 0.29, 0.004)]
        K.sweep(C['black'], cord, circle(0.0033, 5), smooth=True, sharp=89)
    K.finish('side_table')


def build_side_table():
    _side_table(0)


def build_side_table__v1():
    _side_table(1)


# =====================================================================================================================
# Wandgeraete
# =====================================================================================================================
def _gauge(v):
    """Manometer 160 mm (Wand, Rueckseite 2 cm vor der Wand): Gehaeuse mit Bajonettring, Zifferblatt mit
    Teilung, Zahlen und rotem Bereich, Zeiger mit Gegengewicht und Nabe, Anschlag, Sechskant, Gewinde, Rosette;
    Lackabplatzer, Rost, Fettfilm, leicht schief. v 0 schwarz, v 1 Edelstahl (roter Bereich), v 2 Messing alt
    (Zeiger am Anschlag), v 3 schwarz mit Wasserrand."""
    rng = random.Random(776 + v)
    K = Kit()
    C = mats()
    black = M_('lack_schwarz', (0.06, 0.06, 0.06), 'paint', r=1.05)
    stainless = M_('edelstahl_gauge', (0.70, 0.70, 0.69), 'metal', tex='metal_aluminum', r=0.55, m=0.9)
    brass = M_('messing_alt', (0.50, 0.38, 0.18), 'metal', r=0.9, m=0.75)
    case = {0: black, 1: stainless, 2: black, 3: black}[v]
    bezel = {0: black, 1: stainless, 2: brass, 3: stainless}[v]
    dial = M_('zifferblatt', (0.88, 0.86, 0.78) if v != 2 else (0.80, 0.76, 0.62), 'paint', r=1.15)
    tick = M_('teilung', (0.04, 0.04, 0.04), 'paint', r=1.0)
    redz = M_('rot_bereich', (0.70, 0.08, 0.05), 'paint', r=1.0)
    ink = M_('ziffern', (0.19, 0.19, 0.19), 'stencil')
    grease = M_('fett', (0.76, 0.72, 0.60), 'plastic', r=0.4)
    # Gehaeuse (Achse x)
    K.lathe(case, [(0.0, 0.004), (0.066, 0.004)], 20, axis='x', smooth=False)
    K.lathe(case, [(0.066, 0.004), (0.0715, 0.010), (0.072, 0.047)], 20, axis='x', smooth=True, sharp=50)
    K.lathe(bezel, [(0.072, 0.047), (0.0762, 0.052), (0.0752, 0.0615), (0.0675, 0.064), (0.066, 0.053)], 20, axis='x',
            smooth=True, sharp=50)
    K.lathe(dial, [(0.066, 0.053), (0.0, 0.053)], 20, axis='x', smooth=False)
    # Teilung: 270 Grad, 0 unten links, im Uhrzeigersinn
    def rad_quad(mat, th, r0, r1, w, x):
        d = Vector((0, math.sin(th), math.cos(th)))
        t = Vector((0, math.cos(th), -math.sin(th)))
        c = Vector((x, 0, 0)) + d * ((r0 + r1) / 2)
        h = d * ((r1 - r0) / 2)
        K.poly(mat, [c - t * w + h, c - t * w - h, c + t * w - h, c + t * w + h])

    th0, th1 = math.radians(-135), math.radians(135)
    for i in range(51):
        th = th0 + (th1 - th0) * i / 50
        major = i % 5 == 0
        rad_quad(tick, th, 0.050 if major else 0.054, 0.0605, 0.0008 if major else 0.00045, 0.0532)
    for i in range(10):
        a0 = th0 + (th1 - th0) * (0.8 + 0.02 * i)
        a1 = th0 + (th1 - th0) * (0.8 + 0.02 * (i + 1))
        pts = []
        for (r, a) in ((0.0545, a0), (0.0545, a1), (0.0600, a1), (0.0600, a0)):
            pts.append((0.0531, r * math.sin(a), r * math.cos(a)))
        K.poly(redz, pts, n=(1, 0, 0))
    for i, s in enumerate(('0', '2', '4', '6', '8', '10')):
        th = th0 + (th1 - th0) * i / 5
        K.text(ink, s, (0.0533, 0.0385 * math.sin(th), 0.0385 * math.cos(th)), (0, 1, 0), (0, 0, 1), 0.0125)
    K.text(ink, 'BAR', (0.0533, 0.0, -0.026), (0, 1, 0), (0, 0, 1), 0.0095)
    # Zeiger, Nabe, Anschlag
    val = {0: 0.38, 1: 0.88, 2: -0.012, 3: 0.55}[v]
    th = th0 + (th1 - th0) * val
    pp = [(0.0, 0.058), (-0.0013, 0.010), (-0.0024, -0.004), (-0.0045, -0.012), (0.0, -0.0165), (0.0045, -0.012),
          (0.0024, -0.004), (0.0013, 0.010)]
    K.prism(C['black'], pp, 0.0548, 0.0560, R('x', -th) @ frame((0, 0, 0), (1, 0, 0), up=(0, 0, 1)), caps=(False, True))
    K.lathe(C['black'], [(0.0055, 0.054), (0.005, 0.058), (0.0025, 0.0605), (0.0, 0.061)], 8, axis='x', smooth=True, sharp=60)
    ths = th0 - math.radians(6)
    K.cyl(C['steel'], (0.0532, 0.051 * math.sin(ths), 0.051 * math.cos(ths)), (0.0575, 0.051 * math.sin(ths), 0.051 * math.cos(ths)),
          0.0013, seg=5, caps=(False, True))
    # Anschluss: Stutzen, Sechskant, Gewinde, Rosette, Rohr in die Wand
    K.cyl(C['steel'], (0.004, 0, 0), (-0.003, 0, 0), 0.010, seg=8, caps=(False, False))
    K.hexnut(C['steel'], (-0.003, 0, 0), (-1, 0, 0), 0.011, 0.010, caps=(True, True))
    K.lathe(C['steel_dark'], [(0.0075, -0.0200), (0.0085, -0.0185), (0.0075, -0.0170), (0.0085, -0.0155), (0.0075, -0.0135)], 8,
            axis='x', smooth=True, sharp=80)
    K.lathe(bezel if v else C['galv'], [(0.030, -0.0200), (0.030, -0.0180), (0.026, -0.0165), (0.0085, -0.0160)], 12, axis='x',
            smooth=True, sharp=50)
    K.cyl(C['steel_dark'], (-0.02, 0, 0), (-0.10, 0, 0), 0.008, seg=6, caps=(False, False))
    # Gebrauchsspuren: Abplatzer (blankes Metall), Rost unten, Fett auf dem Zifferblatt
    Mx = Matrix.Identity(4)
    if case is black:
        for k in range(5):
            a = rng.uniform(0, TAU)
            w0 = rng.uniform(0.014, 0.04)
            K.tube_patch(C['steel'], Mx, 0.0724, w0, w0 + rng.uniform(0.003, 0.008), a, a + rng.uniform(0.06, 0.16), rng, 1, 2)
    for k in range(3 if v != 1 else 1):
        a = -math.pi / 2 + rng.uniform(-0.6, 0.6)
        w0 = rng.uniform(0.012, 0.035)
        K.tube_patch(C['rust'], Mx, 0.0726, w0, w0 + rng.uniform(0.006, 0.014), a, a + rng.uniform(0.15, 0.4), rng, 2, 2)
    K.blob(grease, (0.0534, 0.020, -0.050), (0, 1, 0), (0, 0, 1), 0.010, 0.004, 8, rng)
    if v == 3:
        K.blob(M_('wasserrand_braun', (0.72, 0.64, 0.50), 'paint', r=1.2), (0.05335, -0.012, -0.034), (0, 1, 0), (0, 0, 1),
               0.026, 0.012, 10, rng)
    if v == 2:
        K.blob(M_('zifferblatt_alt', (0.62, 0.56, 0.42), 'paint', r=1.2), (0.05335, -0.024, 0.026), (0, 1, 0), (0, 0, 1),
               0.016, 0.01, 9, rng)
    K.xform(R('x', {0: 0.05, 1: -0.035, 2: 0.09, 3: -0.13}[v]))
    K.finish('gauge')


def build_gauge():
    _gauge(0)


def build_gauge__v1():
    _gauge(1)


def build_gauge__v2():
    _gauge(2)


def build_gauge__v3():
    _gauge(3)


def build_power_box():
    """Stahlblech-Schaltkasten 600 x 900 x 240 mm (Wand, Rueckseite x = 0): Tuer mit Kanten, Scharniere, Vierkant-
    verschluesse, Warnschild, Schildhalter, Kiemen, Kabelverschraubungen, Stahlpanzerrohr mit Muffen und Abstands-
    schelle bis zur Abzweigdose, Leitungen in die Wand (bis 1,6 m wie das Code-Modell), abgeschnittenes Kabel; Rostlaeufer,
    Abplatzer, Staub."""
    rng = random.Random(308)
    K = Kit()
    C = mats()
    paint = M_('ral7035', (0.56, 0.57, 0.55), 'paint', r=1.2)
    door = M_('ral7035_tuer', (0.58, 0.59, 0.57), 'paint', r=1.1)
    warn = M_('warnschild', (1.0, 1.0, 1.0), 'sign')
    copper = M_('kupfer', (0.72, 0.42, 0.22), 'metal', r=0.6, m=0.9)
    dust = M_('staub_kasten', (0.48, 0.46, 0.42), 'concrete', r=1.5)
    K.bbox(paint, (0.0, -0.3, -0.45), (0.21, 0.3, 0.45), 0.006)
    K.bbox(door, (0.208, -0.285, -0.435), (0.232, 0.285, 0.435), 0.005)
    for z in (-0.33, 0.0, 0.33):
        K.cyl(C['steel_dark'], (0.222, -0.291, z - 0.028), (0.222, -0.291, z + 0.028), 0.0065, seg=8)
    for z in (-0.26, 0.26):
        F = frame((0.232, 0.245, z), (1, 0, 0))
        K.lathe(C['steel_dark'], [(0.013, 0.0), (0.013, 0.004), (0.011, 0.0062), (0.0, 0.0066)], 10, F, smooth=True, sharp=50)
        K.box(C['black'], (-0.0022, -0.0022, 0.0064), (0.0022, 0.0022, 0.0098), F, skip=('-z',))
    K.cell(warn, (0.2325, 0.0, 0.335), (0, 0.045, 0), (0, 0, 0.040), 41)
    K.box(C['black'], (0.2322, -0.072, -0.045), (0.2352, 0.072, 0.045))
    K.cell(C['paper'], (0.2356, 0.0, 0.0), (0, 0.064, 0), (0, 0, 0.038), 56)
    for s in (-1, 1):
        for k in range(4):
            z0 = -0.36 + k * 0.035
            tri = [(0.3, z0), (0.307, z0), (0.3, z0 + 0.016)] if s > 0 else [(-0.3, z0), (-0.3, z0 + 0.016), (-0.307, z0)]
            K.prism(paint, tri, 0.05, 0.16, axis='x', caps=(True, True))
            K.poly(C['black'], [(0.05, 0.3 * s + 0.0005 * s, z0 - 0.0005), (0.16, 0.3 * s + 0.0005 * s, z0 - 0.0005),
                                (0.16, 0.3065 * s, z0 - 0.0005), (0.05, 0.3065 * s, z0 - 0.0005)], n=(0, 0, -1))
    # Verschraubung oben, Panzerrohr mit Muffe und Abstandsschelle bis zur Abzweigdose (1,0-1,17 m ueber dem
    # Ursprung), von dort Leitung in die Wand; zweite Leitung an Clips bis 1,6 m, dann in die Wand (Huelse)
    K.hexnut(C['steel'], (0.12, 0.0, 0.45), (0, 0, 1), 0.024, 0.012)
    K.lathe(C['steel'], [(0.024, 0.462), (0.022, 0.47), (0.02, 0.474)], 10, smooth=True, sharp=60)
    K.cyl(C['galv'], (0.12, 0.0, 0.474), (0.12, 0.0, 1.0), 0.020, seg=10, caps=(False, False))
    zc = 0.72
    K.lathe(C['galv_dark'], [(0.02, zc - 0.03), (0.0235, zc - 0.028), (0.0235, zc + 0.028), (0.02, zc + 0.03)], 10,
            T(0.12, 0, 0), smooth=True, sharp=50)
    zc = 0.88
    K.lathe(C['galv'], [(0.0205, zc - 0.011), (0.0228, zc - 0.011), (0.0228, zc + 0.011), (0.0205, zc + 0.011)], 10,
            T(0.12, 0, 0), smooth=True, sharp=50)
    K.box(C['galv'], (0.003, -0.008, zc - 0.009), (0.0975, 0.008, zc + 0.009))
    K.box(C['galv'], (0.0, -0.026, zc - 0.026), (0.004, 0.026, zc + 0.026), skip=('-x',))
    dull_screw(K, C['steel_dark'], (0.004, 0.017, zc + 0.016), (1, 0, 0), 0.005, slot_mat=C['black'])
    K.streak(C['rust_stain'], (0.0008, 0.0, zc - 0.026), (0, 1, 0), (0, 0, -1), 0.03, rng.uniform(0.08, 0.2), rng)
    jb = M_('abzweigdose', (0.50, 0.51, 0.49), 'plastic', r=1.2)
    K.bbox(jb, (0.0, -0.07, 1.0), (0.165, 0.07, 1.17), 0.008, seg=2)
    K.bbox(jb, (0.165, -0.064, 1.006), (0.172, 0.064, 1.164), 0.003)
    for (y, z) in ((-0.052, 1.018), (0.052, 1.018), (-0.052, 1.152), (0.052, 1.152)):
        dull_screw(K, C['steel'], (0.172, y, z), (1, 0, 0), 0.0045, slot_mat=C['black'])
    K.blob(C['grime'], (0.1722, 0.0, 1.03), (0, 1, 0), (0, 0, 1), 0.05, 0.012, 8, rng)
    K.sweep(C['black'], [(0.06, 0.07, 1.12), (0.06, 0.11, 1.125), (0.035, 0.15, 1.13), (0.004, 0.16, 1.13)], circle(0.0065, 6),
            smooth=True, sharp=89)
    K.lathe(C['galv_dark'], [(0.014, 0.0), (0.014, 0.004), (0.0075, 0.006)], 8, frame((0.0, 0.16, 1.13), (1, 0, 0)), smooth=True, sharp=50)
    K.hexnut(C['black'], (0.07, -0.18, 0.45), (0, 0, 1), 0.012, 0.012)
    K.sweep(C['black'], [(0.07, -0.18, 0.462), (0.07, -0.18, 0.52), (0.045, -0.18, 0.585), (0.014, -0.18, 0.66),
                         (0.012, -0.18, 1.52), (0.016, -0.18, 1.565), (0.004, -0.18, 1.585)], circle(0.0075, 6), smooth=True, sharp=89)
    K.lathe(C['galv_dark'], [(0.015, 0.0), (0.015, 0.004), (0.0085, 0.006)], 8, frame((0.0, -0.18, 1.585), (1, 0, 0)), smooth=True,
            sharp=50)
    for zc in (0.9, 1.3):
        K.box(M_('clip', (0.82, 0.82, 0.80), 'plastic', r=1.0), (0.0, -0.192, zc - 0.008), (0.022, -0.168, zc + 0.008), skip=('-x',))
    # unten: abgeschnittenes Kabel mit blanken Adern
    K.hexnut(C['steel'], (0.12, 0.12, -0.45), (0, 0, -1), 0.016, 0.012)
    cp = [(0.12, 0.12, -0.462), (0.12, 0.125, -0.52), (0.112, 0.14, -0.56), (0.10, 0.155, -0.585)]
    K.sweep(C['black'], cp, circle(0.0068, 6), smooth=True, sharp=89, caps=(False, True))
    end = Vector(cp[-1])
    d = (end - Vector(cp[-2])).normalized()
    for k in range(3):
        o = Vector((0.0, 0.0028 * math.cos(TAU * k / 3), 0.0028 * math.sin(TAU * k / 3)))
        K.cyl(copper, end + o, end + o + d * (0.012 + 0.004 * k), 0.0016, seg=5, caps=(False, True))
    # Gebrauchsspuren: Rostlaeufer von Scharnieren und Oberkante, Abplatzer an den Kanten, Staub oben
    for (y, z, L) in ((-0.291, 0.30, 0.18), (-0.291, -0.035, 0.12), (0.1, 0.434, 0.25), (-0.15, 0.434, 0.12)):
        K.streak(C['rust'], (0.2326, y + 0.012, z), (0, 1, 0), (0, 0, -1), 0.018, L, rng)
    for k in range(7):
        y = rng.choice((-0.274, 0.274)) if k % 2 else rng.uniform(-0.26, 0.26)
        z = rng.uniform(-0.41, 0.41) if k % 2 else rng.choice((-0.425, 0.425))
        K.blob(C['steel'], (0.2327, y, z), (0, 1, 0), (0, 0, 1), rng.uniform(0.004, 0.012), rng.uniform(0.003, 0.008), 6, rng)
    K.blob(dust, (0.10, 0.0, 0.4502), (1, 0, 0), (0, 1, 0), 0.09, 0.27, 10, rng, irr=0.12)
    K.blob(C['grime'], (0.2326, 0.0, -0.40), (0, 1, 0), (0, 0, 1), 0.26, 0.03, 10, rng, irr=0.2)
    K.finish('power_box')


# =====================================================================================================================
# Rohre: Achse durch den Ursprung. pipe_run = 1-m-Stueck (x -0,5 .. 0,5), kachelbar; v 0/2 Stahlrohr verzinkt
# (r 0,045), v 1/3 gedaemmt mit Blechmantel (r 0,070). Alle Rohrmaentel: 12 Segmente, gleiche Phase -> die Stuecke
# passen auch nach 90-Grad-Drehungen Kante an Kante.
# =====================================================================================================================
PSEG = 12
R_STEEL, R_INS = 0.045, 0.070
ROD_TOP = 0.75   # Gewindestange der Haengeschelle bis 0,75 m ueber der Rohrachse (verschwindet in der Decke)


def pipe_mats():
    return {
        'alu': M_('alu_mantel', (0.72, 0.73, 0.71), 'galvanized', r=1.3, m=0.25),
        'alu_dirty': M_('alu_mantel_schmutzig', (0.58, 0.57, 0.53), 'galvanized', r=1.45, m=0.15),
        'wool': M_('mineralwolle', (0.60, 0.56, 0.44), 'fabric'),
        'dust': M_('staub_rohr', (0.50, 0.48, 0.44), 'concrete', r=1.5),
        'iron': M_('temperguss', (0.30, 0.30, 0.29), 'metal', r=1.2, m=0.6),
    }


def tube_zones(K, M, r, xs, matf, seg=PSEG, rf=None):
    """Rohrmantel entlang lokaler x von M, Ringe bei xs; matf(i, k) waehlt das Material je Viereck (None = Luecke);
    rf(i, k) -> Radiusfaktor je Ringpunkt (Dellen)."""
    for i in range(len(xs) - 1):
        for k in range(seg):
            mat = matf(i, k)
            if mat is None:
                continue
            pts, ns = [], []
            for (ii, kk) in ((i, k), (i, k + 1), (i + 1, k + 1), (i + 1, k)):
                a = TAU * kk / seg
                d = Vector((0, math.cos(a), math.sin(a)))
                rr = r * (rf(ii, kk % seg) if rf else 1.0)
                pts.append(_xp(M, Vector((xs[ii], 0, 0)) + d * rr))
                ns.append(_xn(M, d))
            vs = K.verts(mat, pts, ns)
            K.face(mat, vs, True)


def rivet(K, mat, c, w, r, h=None, seg=6):
    h = h if h is not None else r * 0.5
    K.lathe(mat, [(r, 0.0), (r * 0.62, h * 0.8), (0.0, h)], seg, frame(c, w), smooth=True, sharp=70)


def drip(K, C, M, x, r):
    """Wassertropfen an der Rohrunterseite."""
    F = T(x, 0.0, -r - 0.0005)
    K.lathe(C['water'], [(0.0, -0.011), (0.0026, -0.0085), (0.003, -0.005), (0.0018, -0.0015), (0.0, 0.0)], 6,
            M @ F if M is not None else F, smooth=True, sharp=80)


def pipe_clamp(K, C, M, r, rod_top=None, rng=None):
    """Rohrschelle um die lokale x-Achse von M bei x = 0, Ohren nach lokal +z, Schraube, Anschlussmutter und
    Gewindestange bis rod_top (lokal z)."""
    band = C['galv_dark']
    K.lathe(band, [(r + 0.0004, -0.0125), (r + 0.0036, -0.0125), (r + 0.0036, 0.0125), (r + 0.0004, 0.0125)], PSEG, M,
            axis='x', smooth=True, sharp=50)
    zt = r + 0.026
    for s in (-1, 1):
        y0, y1 = sorted((0.003 * s, 0.0068 * s))
        K.box(band, (-0.0125, y0, r + 0.001), (0.0125, y1, zt), M)
    zb = r + 0.015
    K.hexnut(C['steel_dark'], _xp(M, (0, -0.0068, zb)), _xn(M, (0, -1, 0)), 0.0065, 0.0045, caps=(False, True))
    K.hexnut(C['steel_dark'], _xp(M, (0, 0.0068, zb)), _xn(M, (0, 1, 0)), 0.0065, 0.0055, caps=(False, True))
    K.hexnut(C['galv'], _xp(M, (0, 0, zt)), _xn(M, (0, 0, 1)), 0.0085, 0.014, caps=(False, False))
    if rod_top is not None:
        K.cyl(C['steel_dark'], _xp(M, (0, 0, zt + 0.014)), _xp(M, (0, 0, rod_top)), 0.0048, seg=6, caps=(False, False))


def _steel_matf(C, xs, rng, rusty_x=(), heavy_x=()):
    """Material je Viereck fuer verzinktes Rohr: Flugrost an der Unterseite, Rost an Schellen/Gewinden, dunklere
    Anlauffarben oben."""
    rnd = {(i, k): rng.random() for i in range(len(xs)) for k in range(PSEG)}

    def f(i, k):
        s = math.sin(TAU * (k + 0.5) / PSEG)
        xm = (xs[i] + xs[i + 1]) / 2
        if any(abs(xm - x) < 0.05 for x in heavy_x):  # Gewinde an Muffen: rostet zuerst
            return C['rust'] if s < 0.0 else C['rust_light']
        if any(abs(xm - x) < 0.09 for x in rusty_x):  # unter der Schelle: Wasser steht
            return C['rust'] if s < -0.8 else (C['rust_light'] if s < -0.3 else C['galv'])
        if s < -0.85:
            return C['galv_dark']  # Tropfkante unten
        if s > 0.5 and rnd[(i, k)] < 0.3:
            return C['galv_dark']
        return C['galv']
    return f


def _ins_matf(C, P, xs, rng, gap=None):
    rnd = {(i, k): rng.random() for i in range(len(xs)) for k in range(PSEG)}

    def f(i, k):
        s = math.sin(TAU * (k + 0.5) / PSEG)
        if gap and gap(i, k):
            return None
        if s > 0.6:
            return P['alu_dirty']
        if s < -0.7 and rnd[(i, k)] < 0.5:
            return P['alu_dirty']
        return P['alu']
    return f


def _pipe_run(v):
    rng = random.Random(1284 + v)
    K = Kit()
    C = mats()
    P = pipe_mats()
    steel = v in (0, 2)
    r = R_STEEL if steel else R_INS
    if v == 0:
        xs = [-0.5, -0.075, 0.075, 0.5]
        tube_zones(K, None, r, xs, _steel_matf(C, xs, rng, rusty_x=(0.0,)))
        pipe_clamp(K, C, None, r, rod_top=ROD_TOP)
    elif v == 2:
        xs = [-0.5, -0.25, -0.06, -0.034, 0.034, 0.06, 0.25, 0.5]
        tube_zones(K, None, r, xs, _steel_matf(C, xs, rng, heavy_x=(-0.047, 0.047)))
        mp = [(r + 0.0002, -0.036), (r + 0.008, -0.035), (r + 0.008, -0.027), (r + 0.0062, -0.025), (r + 0.0062, 0.025),
              (r + 0.008, 0.027), (r + 0.008, 0.035), (r + 0.0002, 0.036)]
        K.lathe(P['iron'], mp, PSEG, axis='x', smooth=True, sharp=50)
        K.tube_patch(C['rust'], None, r + 0.0066, -0.02, 0.022, -2.3, -0.9, rng, 2, 2)
        drip(K, C, None, 0.04, r)
    elif v == 1:
        xs = [-0.5, -0.0255, 0.0255, 0.45]
        dent = {}
        tube_zones(K, None, r, xs, _ins_matf(C, P, xs, rng), rf=lambda i, k: dent.get((i, k), 1.0))
        pipe_clamp(K, C, None, r, rod_top=ROD_TOP)
    else:
        xs = [-0.5, -0.30, 0.12, 0.45]
        gapf = lambda i, k: i == 1 and 1 <= k <= 4  # oben aufgerissen
        tube_zones(K, None, r, xs, _ins_matf(C, P, xs, rng, gap=gapf))
        jit = {}
        K.lathe(P['wool'], [(r - 0.004, -0.30), (r - 0.0025, -0.09), (r - 0.0045, 0.12)], 6, axis='x',
                a0=TAU * 1 / PSEG, a1=TAU * 5 / PSEG, smooth=True, sharp=80,
                jitter=lambda i, k: jit.setdefault((i, k), rng.uniform(0.95, 1.04)))
        # abstehender Mantellappen
        a = TAU * 5 / PSEG
        d0 = Vector((0, math.cos(a), math.sin(a)))
        out = Vector((0, math.cos(a + 0.5), math.sin(a + 0.5)))
        p0, p1 = Vector((-0.28, 0, 0)) + d0 * r, Vector((0.05, 0, 0)) + d0 * r
        K.poly(P['alu'], [p0, p1, p1 + out * 0.06 + Vector((0.01, 0, -0.01)), p0 + out * 0.075], two=True)
    if not steel:
        K.lathe(P['alu'], [(0.0700, 0.4500), (0.0712, 0.4520), (0.0712, 0.5000)], PSEG, axis='x', smooth=True, sharp=50)
        for a in (-1.0, 2.6):
            rivet(K, C['steel'], (0.475, 0.0713 * math.cos(a), 0.0713 * math.sin(a)), (0, math.cos(a), math.sin(a)), 0.0042)
        K.tube_patch(P['alu'], None, 0.0704, -0.5, 0.45, -0.75, -0.55, rng, 1, 1)
    # Staub oben (durchgehend, damit die Stuecke nahtlos aneinanderpassen)
    K.tube_patch(P['dust'], None, r + (0.0002 if steel else 0.0006), -0.5, 0.5 if steel else 0.45, 1.25, 1.89, rng, 1, 2, irr=0.0)
    K.finish('pipe')


def build_pipe_run():
    _pipe_run(0)


def build_pipe_run__v1():
    _pipe_run(1)


def build_pipe_run__v2():
    _pipe_run(2)


def build_pipe_run__v3():
    _pipe_run(3)


def _arc(c, rad, a0, a1, n, plane='xy'):
    pts = []
    for k in range(n + 1):
        a = a0 + (a1 - a0) * k / n
        if plane == 'xy':
            pts.append((c[0] + rad * math.cos(a), c[1] + rad * math.sin(a), c[2]))
    return pts


def _pipe_elbow(v):
    """90-Grad-Bogen: beginnt im Ursprung entlang +x, endet im Spiel bei (0,5; 0,5; 0) entlang +y (v 0/1; in Blender
    -y, weil der Import y spiegelt); v 2/3 biegt nach unten und endet bei (0,5; 0; -0,5) entlang -z."""
    rng = random.Random(1300 + v)
    K = Kit()
    C = mats()
    P = pipe_mats()
    steel = v in (0, 2)
    ph = -math.pi / 2
    if steel:
        r, Rb = R_STEEL, 0.09
        circ = circle(r, PSEG, ph)
        arc = _arc((0.5 - Rb, Rb, 0.0), Rb, -math.pi / 2, 0.0, 6)
        K.sweep(C['galv'], [(0, 0, 0), (0.36, 0, 0)], circ, smooth=True, sharp=80)
        K.sweep(C['rust'], [(0.36, 0, 0), (0.39, 0, 0)], circ, smooth=True, sharp=80)
        K.sweep(C['rust'], [(0.5, 0.11, 0), (0.5, 0.14, 0)], circ, smooth=True, sharp=80)
        K.sweep(C['galv'], [(0.5, 0.14, 0), (0.5, 0.5, 0)], circ, smooth=True, sharp=80)
        K.sweep(P['iron'], [(0.385, 0, 0)] + arc + [(0.5, 0.115, 0)], circle(r + 0.0065, PSEG, ph), smooth=True, sharp=80)
        for (c, w) in (((0.385, 0, 0), (1, 0, 0)), ((0.5, 0.115, 0), (0, -1, 0))):
            K.lathe(P['iron'], [(r + 0.0002, 0.0), (r + 0.0088, 0.0), (r + 0.0088, 0.011), (r + 0.0065, 0.014)], PSEG,
                    frame(c, w), smooth=True, sharp=50)
        # Rostfahne unten am Bogen
        for k in range(3):
            t = 0.2 + 0.3 * k
            a = -math.pi / 2 + (math.pi / 2) * t
            p = Vector((0.5 - Rb + Rb * math.cos(a), Rb + Rb * math.sin(a), -(r + 0.0068)))
            K.blob(C['rust'], p, (1, 0, 0), (0, -1, 0), rng.uniform(0.012, 0.02), rng.uniform(0.008, 0.014), 6, rng)
    else:
        r, Rb = R_INS, 0.2
        circ = circle(r, PSEG, ph)
        arc = _arc((0.5 - Rb, Rb, 0.0), Rb, -math.pi / 2, 0.0, 4)
        K.sweep(P['alu'], [(0, 0, 0), (0.30, 0, 0)] + arc[1:] + [(0.5, 0.45, 0)], circ, smooth=True, sharp=80)
        # Segmentnaehte des Bogens (Ueberlappungen)
        for j in range(5):
            p = Vector(arc[j])
            a = -math.pi / 2 + (math.pi / 2) * j / 4
            tdir = Vector((-math.sin(a), math.cos(a), 0))
            K.lathe(P['alu_dirty'] if j in (1, 3) else P['alu'], [(0.0711, -0.004), (0.0711, 0.004)], PSEG, frame(p, tdir),
                    smooth=True, sharp=80)
        K.lathe(P['alu'], [(0.0700, 0.0), (0.0712, 0.002), (0.0712, 0.05)], PSEG, frame((0.5, 0.45, 0), (0, 1, 0)),
                smooth=True, sharp=50)
        for a in (-1.2, 0.0, 1.2, 2.4):
            c = Vector((0.5 + 0.0713 * math.cos(a), 0.475, 0.0713 * math.sin(a)))
            rivet(K, C['steel'], c, (math.cos(a), 0, math.sin(a)), 0.0042)
        K.tube_patch(P['dust'], None, 0.0706, 0.0, 0.30, 1.25, 1.89, rng, 1, 2, irr=0.0)
    if v >= 2:
        K.xform(R('x', -math.pi / 2))
    else:
        K.mirror_y()  # gebaut nach Blender +y; im Spiel (y gespiegelt) soll der Bogen nach +y fuehren
    K.finish('pipe_elbow')


def build_pipe_elbow():
    _pipe_elbow(0)


def build_pipe_elbow__v1():
    _pipe_elbow(1)


def build_pipe_elbow__v2():
    _pipe_elbow(2)


def build_pipe_elbow__v3():
    _pipe_elbow(3)


def _pipe_wall(v):
    """Wanddurchfuehrung (Rueckseite = Wandflaeche bei x = 0). Enthaelt kein eigenes Rohr: sie sitzt ueber dem ersten
    pipe_run-Stueck (Karte: Ursprung an der Wand, Lauf beginnt an der Wand). v 0 Stahl: Rosette, Flanschpaar DN 80
    mit 4 Schrauben und Dichtung, Rost am Rohr hinter dem Flansch; v 1 gedaemmt: Rosette um den Mantel, Spannband.
    Beide: Feuchte- und Rostfahne an der Wand."""
    rng = random.Random(1400 + v)
    K = Kit()
    C = mats()
    P = pipe_mats()
    if v == 0:
        r = R_STEEL
        K.lathe(C['galv_dark'], [(0.085, 0.0), (0.085, 0.004), (0.080, 0.008), (0.050, 0.010), (0.047, 0.016), (0.0453, 0.020)],
                PSEG, axis='x', smooth=True, sharp=45)
        for a in (0.5, 0.5 + TAU / 3, 0.5 + 2 * TAU / 3):
            dull_screw(K, C['steel_dark'], (0.0088, 0.067 * math.cos(a), 0.067 * math.sin(a)), (1, 0, 0), 0.0055, slot_mat=C['black'])
        fa = [(0.0453, 0.104), (0.0535, 0.107), (0.0575, 0.112), (0.0585, 0.126), (0.097, 0.127), (0.100, 0.130), (0.100, 0.1455),
              (0.098, 0.148), (0.085, 0.148)]
        fb = [(0.085, 0.151), (0.098, 0.151), (0.100, 0.1535), (0.100, 0.169), (0.097, 0.172), (0.0585, 0.173), (0.0575, 0.187),
              (0.0535, 0.192), (0.0453, 0.195)]
        K.lathe(C['rust'], fa, 16, axis='x', smooth=True, sharp=40)
        K.lathe(C['rust'], fb, 16, axis='x', smooth=True, sharp=40)
        K.lathe(C['rubber'], [(0.0855, 0.148), (0.0855, 0.151)], 16, axis='x', smooth=True, sharp=80)
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            y, z = 0.080 * math.cos(a), 0.080 * math.sin(a)
            K.hexnut(C['steel_dark'], (0.127, y, z), (-1, 0, 0), 0.012, 0.010)
            K.hexnut(C['steel_dark'], (0.172, y, z), (1, 0, 0), 0.012, 0.013)
            K.cyl(C['steel_dark'], (0.185, y, z), (0.194, y, z), 0.0075, seg=6, caps=(False, True))
        # Rost am Rohr zwischen Rosette und Flansch und hinter dem Flansch (Unterseite), Tropfen
        tube_zones(K, None, r + 0.0003, [0.021, 0.06, 0.103], lambda i, k: C['rust'] if math.sin(TAU * (k + 0.5) / PSEG) < -0.2 else None)
        tube_zones(K, None, r + 0.0003, [0.196, 0.26], lambda i, k: C['rust_light'] if math.sin(TAU * (k + 0.5) / PSEG) < -0.4 else None)
        drip(K, C, None, 0.20, r + 0.0003)
        wet = (0.15, 0.13)
    else:
        r = R_INS
        K.lathe(C['galv_dark'], [(0.098, 0.0), (0.098, 0.004), (0.093, 0.008), (0.076, 0.010), (0.0725, 0.016), (0.0706, 0.022)],
                PSEG, axis='x', smooth=True, sharp=45)
        for a in (0.3, 0.3 + TAU / 3, 0.3 + 2 * TAU / 3):
            dull_screw(K, C['steel_dark'], (0.0088, 0.086 * math.cos(a), 0.086 * math.sin(a)), (1, 0, 0), 0.0055, slot_mat=C['black'])
        K.lathe(C['steel'], [(0.0702, 0.060), (0.0718, 0.061), (0.0718, 0.074), (0.0702, 0.075)], PSEG, axis='x', smooth=True, sharp=50)
        K.box(C['steel_dark'], (0.058, -0.012, -0.0745), (0.077, 0.012, -0.0712))
        K.tube_patch(P['alu_dirty'], None, r + 0.0004, 0.022, 0.058, -2.6, -0.6, rng, 2, 3)
        wet = (0.17, 0.15)
    # Fahne an der Wand: Feuchte und Rost unter der Durchfuehrung
    K.blob(M_('feuchte_wand', (0.42, 0.39, 0.34), 'concrete', r=1.2), (0.0004, 0.0, -0.05), (0, 1, 0), (0, 0, 1), wet[0] * 1.15,
           wet[1] * 1.3, 14, rng, irr=0.2)
    K.blob(M_('feuchte_wand_kern', (0.33, 0.30, 0.26), 'concrete', r=1.1), (0.0007, 0.0, -0.04), (0, 1, 0), (0, 0, 1), wet[0] * 0.75,
           wet[1] * 0.8, 12, rng, irr=0.25)
    K.streak(C['rust_stain'], (0.0008, 0.01, -0.07), (0, 1, 0), (0, 0, -1), 0.07, 0.38, rng, n=5)
    K.streak(C['rust_stain'], (0.0009, -0.04, -0.06), (0, 1, 0), (0, 0, -1), 0.03, 0.22, rng, n=4)
    K.finish('pipe_wall')


def build_pipe_wall():
    _pipe_wall(0)


def build_pipe_wall__v1():
    _pipe_wall(1)


def _pipe_drop(v):
    """Fallrohr-Stueck 1 m senkrecht (z -0,5 .. 0,5, Achse durch den Ursprung). v 0/1 mit Wandschelle (Wand bei
    x = -0,15), v 2/3 frei. v 0/2 Stahl, v 1/3 gedaemmt (Ueberlappung unten, Wasser laeuft ab)."""
    rng = random.Random(1500 + v)
    K = Kit()
    C = mats()
    P = pipe_mats()
    steel = v in (0, 2)
    Mz = R('y', -math.pi / 2)   # lokale x -> z
    if steel:
        r = R_STEEL
        xs = [-0.5, -0.22, -0.03, 0.03, 0.26, 0.5]
        rnd = {(i, k): rng.random() for i in range(6) for k in range(PSEG)}
        streaks = {k for k in range(PSEG) if rng.random() < 0.3}

        def f(i, k):
            if k in streaks and i >= 2:
                return C['galv_dark'] if rnd[(i, k)] < 0.7 else C['rust_light']
            if i == 2 and v == 0:
                return C['rust']
            return C['galv'] if rnd[(i, k)] < 0.8 else C['galv_dark']
        tube_zones(K, Mz, r, xs, f)
    else:
        r = R_INS
        xs = [-0.45, -0.03, 0.03, 0.5]
        tube_zones(K, Mz, r, xs, lambda i, k: P['alu_dirty'] if (i == 0 and k % 3 == 0) else P['alu'])
        K.lathe(P['alu'], [(0.0701, -0.5), (0.0712, -0.5), (0.0712, -0.45), (0.0700, -0.45)], PSEG, Mz, axis='x', smooth=True, sharp=50)
        for a in (0.3, 1.5, 2.7, 3.9, 5.1):
            rivet(K, C['steel'], _xp(Mz, (-0.475, 0.0713 * math.cos(a), 0.0713 * math.sin(a))), _xn(Mz, (0, math.cos(a), math.sin(a))), 0.0042)
    if v in (0, 1):
        # Schelle, Ohren zur Wand (-x), Gewindestange zur Wandplatte bei x = -0,15
        pipe_clamp(K, C, Mz, r, rod_top=None)  # Mz bildet die Ohren (lokal +z) auf Welt -x ab
        x0 = -(r + 0.026)
        K.hexnut(C['galv_dark'], (x0 - 0.019, 0, 0), (-1, 0, 0), 0.0085, 0.008)
        K.cyl(C['steel_dark'], (x0 - 0.014, 0, 0), (-0.144, 0, 0), 0.0048, seg=6, caps=(False, False))
        K.lathe(C['galv'], [(0.032, -0.150), (0.032, -0.146), (0.026, -0.143), (0.012, -0.142), (0.009, -0.136)], 10, axis='x',
                smooth=True, sharp=45)
        for a in (0.4, 0.4 + TAU / 3, 0.4 + 2 * TAU / 3):
            dull_screw(K, C['steel_dark'], (-0.1422, 0.022 * math.cos(a), 0.022 * math.sin(a)), (1, 0, 0), 0.004)
        K.streak(C['rust_stain'], (-0.1495, 0.0, -0.03), (0, 1, 0), (0, 0, -1), 0.03, 0.2, rng)
    K.finish('pipe_drop')


def build_pipe_drop():
    _pipe_drop(0)


def build_pipe_drop__v1():
    _pipe_drop(1)


def build_pipe_drop__v2():
    _pipe_drop(2)


def build_pipe_drop__v3():
    _pipe_drop(3)


# =====================================================================================================================
# Hochregal (Gild): ein Feld 2,7 m (x) einer Doppelreihe (2 x 1,0 m tief, 0,2 m Spalt), Traversen auf 1,7 / 3,2 /
# 4,7 m, Ladung bis 5,4 m. Rahmen bei x = -1,35 (v & 4: auch bei +1,35, letztes Feld). Gassen bei y = -1,2 und +1,2.
# =====================================================================================================================
def rack_mats():
    return {
        'frame': M_('regal_rahmen', (0.09, 0.20, 0.46), 'paint', r=1.0),
        'beam': M_('regal_traverse', (0.86, 0.36, 0.05), 'paint', r=1.0),
        'slot': M_('lochung', (0.015, 0.015, 0.015), 'plastic', r=1.5),
        'guard': M_('anfahrschutz', (0.78, 0.60, 0.10), 'paint', r=1.25),
        'guard_b': M_('anfahrschutz_streifen', (0.05, 0.05, 0.05), 'paint', r=1.25),
        'pin': M_('sicherung', (0.75, 0.12, 0.08), 'plastic', r=0.8),
        'pal': M_('palette', (0.60, 0.50, 0.36), 'wood', r=1.3),
        'pal_old': M_('palette_alt', (0.46, 0.40, 0.31), 'wood', r=1.4),
        'pal_blk': M_('palette_klotz', (0.52, 0.42, 0.30), 'wood', r=1.35),
        'pal_wet': M_('palette_nass', (0.30, 0.24, 0.17), 'wood', r=0.9),
        'card': M_('karton', (0.60, 0.46, 0.30), 'cardboard'),
        'card_l': M_('karton_hell', (0.68, 0.55, 0.38), 'cardboard'),
        'card_o': M_('karton_alt', (0.52, 0.42, 0.30), 'cardboard'),
        'card_w': M_('karton_nass', (0.33, 0.24, 0.15), 'cardboard', r=0.95),
        'card_in': M_('karton_innen', (0.40, 0.31, 0.20), 'cardboard'),
        'card_seam': M_('karton_naht', (0.50, 0.38, 0.24), 'cardboard'),
        'tape': M_('klebeband', (0.58, 0.40, 0.17), 'plastic', r=0.55),
        'tape_c': M_('klebeband_klar', (0.66, 0.58, 0.44), 'plastic', r=0.4),
        'tape_r': M_('klebeband_rot', (0.60, 0.12, 0.08), 'plastic', r=0.6),
        'film': M_('folie', (0.60, 0.53, 0.44), 'plastic', r=0.45),
        'film_thick': M_('folie_dicht', (0.80, 0.81, 0.80), 'plastic', r=0.5),
        'strap': M_('umreifung', (0.10, 0.10, 0.11), 'plastic', r=0.7),
        'crate': M_('kiste_kiefer', (0.48, 0.41, 0.32), 'wood', r=1.35),
        'crate_d': M_('kiste_leiste', (0.34, 0.28, 0.21), 'wood', r=1.4),
        'crate_in': M_('kiste_innen', (0.16, 0.12, 0.08), 'wood', r=1.5),
        'ink': M_('karton_druck', (0.22, 0.17, 0.12), 'stencil'),
        'ink_w': M_('kiste_schablone', (0.21, 0.21, 0.22), 'stencil'),
    }


def _pallet(K, Rm, xc, yc, z0, rng, loaded=True, under=True, wet=False, face=0, lod=False):
    """Europalette 1,2 m (y) x 0,8 m (x) x 0,144 m: Bodenbretter, Kloetze mit dunklen Gabeloeffnungen, Querbretter,
    Deckbretter (beladen: eine Decklage mit Fugen an den Stirnseiten); verdeckte Flaechen fehlen."""
    wood = Rm['pal_wet'] if wet else (Rm['pal_old'] if rng.random() < 0.35 else Rm['pal'])
    blk = Rm['pal_wet'] if wet else Rm['pal_blk']
    dark = Rm['crate_in']
    j = lambda: rng.uniform(-0.003, 0.003)
    if lod and loaded:
        # oberste Fachboeden (ab 3,2 m): Bodenbretter einzeln (von unten sichtbar), Rest als ein Koerper mit
        # dunklen Gabeloeffnungen und Fugen zur Gasse
        for (bx, bw) in ((-0.35, 0.10), (0.0, 0.145), (0.35, 0.10)):
            K.box(wood, (xc + bx - bw / 2, yc - 0.6, z0), (xc + bx + bw / 2, yc + 0.6, z0 + 0.022), skip=('+z',))
        K.box(blk, (xc - 0.4, yc - 0.6, z0 + 0.022), (xc + 0.4, yc + 0.6, z0 + 0.144), skip=('+z', '-z'))
        K.poly(dark, [(xc - 0.4, yc - 0.6, z0 + 0.022), (xc + 0.4, yc - 0.6, z0 + 0.022), (xc + 0.4, yc + 0.6, z0 + 0.022),
                      (xc - 0.4, yc + 0.6, z0 + 0.022)], n=(0, 0, -1))
        if face:
            y = yc + face * 0.6003
            for (g0, g1) in ((-0.30, -0.0725), (0.0725, 0.30)):
                K.poly(dark, [(xc + g0, y, z0 + 0.023), (xc + g1, y, z0 + 0.023), (xc + g1, y, z0 + 0.099), (xc + g0, y, z0 + 0.099)],
                       n=(0, face, 0))
            K.poly(wood, [(xc - 0.4, y + face * 0.0003, z0 + 0.1), (xc + 0.4, y + face * 0.0003, z0 + 0.1),
                          (xc + 0.4, y + face * 0.0003, z0 + 0.144), (xc - 0.4, y + face * 0.0003, z0 + 0.144)], n=(0, face, 0))
            for gx in (-0.2335, -0.093, 0.093, 0.2335):
                K.poly(dark, [(xc + gx - 0.019, y + face * 0.0006, z0 + 0.122), (xc + gx + 0.019, y + face * 0.0006, z0 + 0.122),
                              (xc + gx + 0.019, y + face * 0.0006, z0 + 0.144), (xc + gx - 0.019, y + face * 0.0006, z0 + 0.144)],
                       n=(0, face, 0))
        return
    for (bx, bw) in ((-0.35, 0.10), (0.0, 0.145), (0.35, 0.10)):
        K.box(wood, (xc + bx - bw / 2 + j(), yc - 0.6, z0), (xc + bx + bw / 2 + j(), yc + 0.6, z0 + 0.022),
              skip=('+z',) if under else ('+z', '-z'))
    for by in (-0.5375, 0.5375):
        # Klotzreihe als ein Koerper, die Gabeloeffnungen als dunkle Flaechen (vorn und hinten)
        K.box(blk, (xc - 0.4, yc + by - 0.0625, z0 + 0.022), (xc + 0.4, yc + by + 0.0625, z0 + 0.1), skip=('+z', '-z'))
        for (g0, g1) in ((-0.30, -0.0725), (0.0725, 0.30)):
            for s in (-1, 1):
                if face and (s != face or by * face < 0):
                    continue  # nur die Oeffnungen zur Gasse (die Rueckseiten stossen an die Nachbarpalette)
                y = yc + by + s * 0.0627
                K.poly(dark, [(xc + g0, y, z0 + 0.023), (xc + g1, y, z0 + 0.023), (xc + g1, y, z0 + 0.099), (xc + g0, y, z0 + 0.099)],
                       n=(0, s, 0))
    for by in (-0.5275, 0.5275):
        K.box(wood, (xc - 0.4, yc + by - 0.0725, z0 + 0.1), (xc + 0.4, yc + by + 0.0725, z0 + 0.122), skip=('+z',))
    if loaded:
        K.box(wood, (xc - 0.4 + j(), yc - 0.6, z0 + 0.122), (xc + 0.4 + j(), yc + 0.6, z0 + 0.144), skip=('+z',))
        for gx in (-0.2335, -0.093, 0.093, 0.2335):
            for s in (-1, 1):
                if face and s != face:
                    continue
                y = yc + s * 0.6003
                K.poly(dark, [(xc + gx - 0.019, y, z0 + 0.122), (xc + gx + 0.019, y, z0 + 0.122), (xc + gx + 0.019, y, z0 + 0.144),
                              (xc + gx - 0.019, y, z0 + 0.144)], n=(0, s, 0))
    else:
        for (bx, bw) in ((-0.3275, 0.145), (-0.164, 0.10), (0.0, 0.145), (0.164, 0.10), (0.3275, 0.145)):
            K.box(wood, (xc + bx - bw / 2 + j(), yc - 0.6, z0 + 0.122), (xc + bx + bw / 2 + j(), yc + 0.6, z0 + 0.144 + abs(j())))


def _carton(K, Rm, c, L, W, H, rot, rng, state='zu', detail=1, body=None, face=(0, -1), tilt=None, cover=False,
            top_only=False):
    """Karton (RSC): c = Mitte unten; L entlang x, W entlang y (lokal, um rot gedreht). state: zu | offen | nass.
    detail 1: Delle, Herstellernaht, Etikett, Druck. cover: Deckel verdeckt (naechste Lage). top_only: nur Deckel
    (in Folie gewickelt)."""
    M = T(*c) @ R('z', rot)
    if tilt is not None:
        M = M @ tilt
    body = body or rng.choice((Rm['card'], Rm['card'], Rm['card_l'], Rm['card_o']))
    jt = lambda s: rng.uniform(-s, s)
    hx, hy = L / 2, W / 2
    sag = rng.uniform(0.02, 0.05) if state == 'nass' else 0.0
    bot = [Vector((-hx, -hy, 0)), Vector((hx, -hy, 0)), Vector((hx, hy, 0)), Vector((-hx, hy, 0))]
    top = [Vector((p.x + jt(0.004), p.y + jt(0.004), H + jt(0.004) - sag * 0.3)) for p in bot]
    if detail and state != 'offen' and rng.random() < 0.4:
        k = rng.randrange(4)
        top[k] = top[k] + Vector((-math.copysign(0.01, top[k].x), -math.copysign(0.01, top[k].y), -rng.uniform(0.015, 0.035)))
    up = Vector((0, 0, 1))
    fdir = Vector((face[0], face[1], 0))
    Ri = R('z', -rot)
    fl = (Ri @ fdir.to_4d()).to_3d() if tilt is None else Vector((0, -1, 0))
    best = max(range(4), key=lambda i: ((bot[(i + 1) % 4] - bot[i]).cross(up)).normalized().dot(fl))
    xp = lambda p: M @ p
    if not top_only:
        dent_side = rng.randrange(4) if (detail and state == 'zu' and rng.random() < 0.45) else -1
        for i in range(4):
            a, b, ta, tb = bot[i], bot[(i + 1) % 4], top[i], top[(i + 1) % 4]
            out = (b - a).cross(up).normalized()
            if state == 'nass':
                hw = rng.uniform(0.3, 0.6) * H
                rows = []
                for (zf, bul) in ((0.0, 0.0), (hw, 0.014), (None, 0.006)):
                    row = []
                    for t in (0.0, 0.5, 1.0):
                        p = ta.lerp(tb, t) if zf is None else a.lerp(b, t) + Vector((0, 0, zf + (jt(0.04) if t == 0.5 else jt(0.012))))
                        row.append(xp(p + out * (bul * (1.0 if t == 0.5 else 0.3))))
                    rows.append(row)
                K.grid(Rm['card_w'], rows[:2], smooth=False)
                K.grid(body, rows[1:], smooth=False)
            elif i == dent_side:
                cpt = (a + b + ta + tb) / 4 - out * rng.uniform(0.012, 0.03) + Vector((0, 0, jt(0.05)))
                for tri in ((a, b, cpt), (b, tb, cpt), (tb, ta, cpt), (ta, a, cpt)):
                    K.poly(body, [xp(p) for p in tri])
            else:
                K.poly(body, [xp(a), xp(b), xp(tb), xp(ta)])
                if state == 'offen':  # Innenseite (Rueckseiten werden nicht gezeichnet)
                    K.poly(Rm['card_in'], [xp(ta), xp(tb), xp(b), xp(a)])
            sp = lambda t, h, o=0.0012: a.lerp(b, t).lerp(ta.lerp(tb, t), h) + out * o
            if detail and i == best and state != 'nass':
                Lside = (b - a).length
                if rng.random() < 0.75:
                    t0 = rng.uniform(0.25, 0.6)
                    cc = sp(t0, rng.uniform(0.4, 0.62), 0.0015)
                    K.cell(Rm['paper_l'] if 'paper_l' in Rm else mats()['paper'], xp(cc), (M.to_3x3() @ (b - a).normalized()) * 0.055,
                           (M.to_3x3() @ up) * 0.04, 56 + rng.randrange(4))
                if rng.random() < 0.45:
                    code = rng.choice(('A7', 'B12', '04', 'K3', '217', 'X', 'L1', '09'))
                    cc = sp(rng.uniform(0.2, 0.35) if Lside > 0.5 else 0.5, rng.uniform(0.72, 0.85), 0.0014)
                    K.text(Rm['ink'], code, xp(cc), M.to_3x3() @ (b - a).normalized(), M.to_3x3() @ up,
                           min(0.08, H * 0.18))
            if detail and i == (best + 1) % 4 and state == 'zu' and i != dent_side:
                w = min(0.4, 0.035 / max(0.05, (b - a).length))
                K.poly(Rm['card_seam'], [xp(sp(0, 0.02)), xp(sp(w, 0.02)), xp(sp(w, 0.98)), xp(sp(0, 0.98))])
        if state == 'offen':
            K.poly(Rm['card_in'], [xp(p + Vector((0, 0, 0.006))) for p in bot])
    if state == 'offen':
        f = min(L, W) / 2 * 0.95
        for i in range(4):
            ta, tb = top[i], top[(i + 1) % 4]
            out = (tb - ta).cross(up).normalized()
            # Laschen bleiben in den Grenzen des Regals (Gasse frei, unter 5,45 m): sonst steiler, weiter nach
            # aussen geklappt oder nach innen gefallen
            for psi in (rng.uniform(0.25, 2.0), 0.25, 1.7, 2.3, -1.3):
                d = (out * math.sin(psi) + up * math.cos(psi)) * f
                ends = (xp(ta + d), xp(tb + d))
                if all(abs(e.y) <= 1.205 and e.z <= 5.42 for e in ends):
                    break
            K.poly(body, [xp(ta), xp(tb), xp(tb + d), xp(ta + d)], two=True)
        return
    if cover:
        return
    # Deckel: zwei Aussenlaschen, Stoss entlang der laengeren Seite, leicht gewoelbt oder eingesunken
    bump = rng.uniform(-0.006, 0.005) - (rng.uniform(0.02, 0.05) if state == 'nass' else 0.0)
    if L >= W:
        s0, s1 = (top[0] + top[3]) / 2, (top[1] + top[2]) / 2
        fl1, fl2 = [top[0], top[1]], [top[3], top[2]]
    else:
        s0, s1 = (top[0] + top[1]) / 2, (top[3] + top[2]) / 2
        fl1, fl2 = [top[0], top[3]], [top[1], top[2]]
    s0, s1 = s0 + Vector((0, 0, bump)), s1 + Vector((0, 0, bump))
    if L >= W:
        K.poly(body, [xp(top[0]), xp(top[1]), xp(s1), xp(s0)])
        K.poly(body, [xp(s0), xp(s1), xp(top[2]), xp(top[3])])
    else:
        K.poly(body, [xp(top[0]), xp(s0), xp(s1), xp(top[3])])
        K.poly(body, [xp(s0), xp(top[1]), xp(top[2]), xp(s1)])
    # Klebeband ueber den Stoss und an den Enden herunter
    tape = Rm['tape_r'] if rng.random() < 0.08 else (Rm['tape_c'] if rng.random() < 0.3 else Rm['tape'])
    half = (fl1[0] - s0).length or 1.0
    tw = min(0.5, 0.025 / half)
    o = Vector((0, 0, 0.0012))
    m0, m1 = s0.lerp(fl1[0], tw) + o, s1.lerp(fl1[1], tw) + o
    p0, p1 = s0.lerp(fl2[0], tw) + o, s1.lerp(fl2[1], tw) + o
    K.poly(tape, [xp(m0), xp(m1), xp(s1 + o), xp(s0 + o)], n=M.to_3x3() @ up)
    K.poly(tape, [xp(s0 + o), xp(s1 + o), xp(p1), xp(p0)], n=M.to_3x3() @ up)
    if not top_only:
        ax = (s1 - s0).normalized()
        for (sa, ma, pa, sg) in ((s0, m0, p0, -1), (s1, m1, p1, 1)):
            if state == 'nass' and sg > 0:
                continue  # Band abgeloest
            dn = Vector((0, 0, -rng.uniform(0.05, 0.08)))
            e = ax * (sg * 0.0015)
            K.poly(tape, [xp(ma + e), xp(pa + e), xp(pa + e + dn), xp(ma + e + dn)], n=M.to_3x3() @ (ax * sg))


def _carton_load(K, Rm, xc, yc, z0, hmax, rng, layout='mix', state='zu', detail=1, face=(0, -1), lean=0.0, top_only=False):
    plan, tot = [], 0.0
    while len(plan) < 4:
        pat = layout if layout != 'mix' else rng.choice(('1', '2', '2', '4'))
        H = rng.uniform(0.30, 0.48) if pat == '1' else rng.uniform(0.24, 0.40)
        if tot + H > hmax:
            if not plan and hmax > 0.15:
                plan.append((pat, hmax - 0.01))
            break
        plan.append((pat, H))
        tot += H
    z = z0
    for layer, (pat, H) in enumerate(plan):
        nxt = layer < len(plan) - 1
        cells = {'1': [(0.0, 0.0, 0.78, 1.16)], '2': [(0.0, -0.29, 0.78, 0.575), (0.0, 0.29, 0.78, 0.575)],
                 '4': [(sx * 0.196, sy * 0.29, 0.385, 0.575) for sx in (-1, 1) for sy in (-1, 1)]}[pat]
        body = rng.choice((Rm['card'], Rm['card'], Rm['card_l'], Rm['card_o']))
        for (dx, dy, L, W) in cells:
            sc = rng.uniform(0.96, 0.995)
            cx = xc + dx + rng.uniform(-0.012, 0.012) + lean * layer
            cy = yc + dy + rng.uniform(-0.015, 0.015)
            st = state
            if state == 'zu' and rng.random() < 0.06 and not nxt:
                st = 'offen'
            _carton(K, Rm, (cx, cy, z), L * sc, W * sc, H + rng.uniform(-0.004, 0.004), rng.uniform(-0.03, 0.03), rng, st,
                    detail if layer < 2 else 0, body=body, face=face, cover=nxt and not top_only,
                    top_only=top_only and not nxt)
        z += H
    return z


def _wrapped(K, Rm, xc, yc, z0, hmax, rng, face):
    """gewickelte Ladung: Folie um Palette und Kartons, oben schauen die Deckel heraus; ein Folienfetzen haengt."""
    Ht = rng.uniform(0.55, max(0.56, min(hmax, 1.15)))
    zt = z0 + Ht
    n = max(1, round(Ht / 0.33))
    h = Ht / n
    pat = rng.choice(('2', '4', '4'))
    _carton_load(K, Rm, xc, yc, z0 + h * (n - 1), h + 0.002, rng, layout=pat, detail=0, face=face, top_only=True)
    zt = z0 + h * n
    rows = []
    for (zz, hx, hy) in ((z0 - 0.085, 0.407, 0.607), (z0 + 0.10, 0.401, 0.597), (z0 + Ht * 0.5, 0.392, 0.587),
                         (zt - 0.12, 0.397, 0.591), (zt - 0.02, 0.398, 0.592), (zt + 0.004, 0.372, 0.566)):
        rows.append([Vector((xc + u, yc + v, zz)) for (u, v) in rrect(hx, hy, 0.035, n=1)])
    K.grid(Rm['film_thick'], rows[:2], closed=True, smooth=True)
    K.grid(Rm['film'], rows[1:4], closed=True, smooth=True)
    K.grid(Rm['film_thick'], rows[3:], closed=True, smooth=True)
    k = rng.randrange(4)
    sx, sy = (1, -1, -1, 1)[k], (-1, -1, 1, 1)[k]
    p = Vector((xc + sx * 0.25, yc + sy * 0.6075, z0 - 0.06))
    d = Vector((rng.uniform(-0.05, 0.05), sy * 0.01, -0.14))
    K.poly(Rm['film_thick'], [p, p + Vector((0.12, 0, 0)), p + d + Vector((0.03, 0, 0))], two=True)


def _crate(K, Rm, xc, yc, z0, L, W, H, rng, face, rot=0.0):
    """Holzkiste aus Brettern mit Fugen, Eckleisten, Deckel, Schablonenschrift; innen dunkel (Fugen)."""
    M = T(xc, yc, z0) @ R('z', rot)
    wood, dark = Rm['crate'], Rm['crate_d']
    t = 0.018
    K.box(wood, (-L / 2, -W / 2, 0.0), (L / 2, W / 2, H), M, skip=('-z',))
    # Brettfugen als dunkle Linien (Seiten waagrecht, Deckel laengs)
    n = max(2, round(H / 0.15))
    for k in range(1, n):
        z = H * k / n + rng.uniform(-0.006, 0.006)
        for (p0, p1, o) in (((-L / 2, -W / 2), (L / 2, -W / 2), (0, -1)), ((L / 2, W / 2), (-L / 2, W / 2), (0, 1)),
                            ((L / 2, -W / 2), (L / 2, W / 2), (1, 0)), ((-L / 2, W / 2), (-L / 2, -W / 2), (-1, 0))):
            e = (o[0] * 0.0008, o[1] * 0.0008)
            K.poly(Rm['crate_in'], [M @ Vector((p0[0] + e[0], p0[1] + e[1], z - 0.003)), M @ Vector((p1[0] + e[0], p1[1] + e[1], z - 0.003)),
                                    M @ Vector((p1[0] + e[0], p1[1] + e[1], z + 0.003)), M @ Vector((p0[0] + e[0], p0[1] + e[1], z + 0.003))])
    m = max(2, round(W / 0.14))
    for k in range(1, m):
        y = -W / 2 + W * k / m
        K.poly(Rm['crate_in'], [M @ Vector((-L / 2, y - 0.003, H + 0.0008)), M @ Vector((L / 2, y - 0.003, H + 0.0008)),
                                M @ Vector((L / 2, y + 0.003, H + 0.0008)), M @ Vector((-L / 2, y + 0.003, H + 0.0008))])
    b = 0.045
    for sx in (-1, 1):
        for sy in (-1, 1):
            x0, y0 = sx * L / 2, sy * W / 2
            xa, xb = sorted((x0 - sx * b, x0 + sx * 0.016))
            ya, yb = sorted((y0, y0 + sy * 0.016))
            K.box(dark, (xa, ya, 0.0), (xb, yb, H - t), M, skip=('-z',))
    # Schablonenschrift zur Gasse
    sgn = 1 if face[1] > 0 else -1
    code = rng.choice(('L1-07', 'G 214', 'NR 3', 'B-12', '0815', 'AQ 9'))
    c = Vector((0.0, sgn * (W / 2 + 0.0012), H * 0.55))
    rgt = Vector((-sgn, 0, 0))
    K.text(Rm['ink_w'], code, M @ c, M.to_3x3() @ rgt, M.to_3x3() @ Vector((0, 0, 1)), min(0.09, H * 0.2))


def _flat_bundle(K, Rm, xc, yc, z0, rng, face):
    H = rng.uniform(0.12, 0.26)
    body = rng.choice((Rm['card'], Rm['card_o']))
    rot = rng.uniform(-0.05, 0.05)
    M = T(xc, yc, z0) @ R('z', rot)
    K.box(body, (-0.385, -0.56, 0.0), (0.385, 0.56, H), M, skip=('-z',))
    for k in range(1, int(H / 0.012)):
        z = k * 0.012 + rng.uniform(-0.002, 0.002)
        for sy in (-1, 1):
            K.poly(Rm['card_seam'], [M @ Vector((-0.38, sy * 0.5612, z)), M @ Vector((0.38, sy * 0.5612, z)),
                                     M @ Vector((0.38, sy * 0.5612, z + 0.002)), M @ Vector((-0.38, sy * 0.5612, z + 0.002))],
                   n=M.to_3x3() @ Vector((0, sy, 0)))
    for x in (-0.2, 0.2):
        K.box(Rm['strap'], (x - 0.008, -0.562, -0.001), (x + 0.008, 0.562, H + 0.0015), M, skip=('-z',))


def _rack(v):
    rng = random.Random(2400 + v)
    K = Kit()
    C = mats()
    Rm = rack_mats()
    Rm['paper_l'] = C['paper']
    Lh = 1.35
    frames = [-Lh] + ([Lh] if v & 4 else [])
    rows_y = [(-1.1, -0.1), (0.1, 1.1)]
    # --- Rahmen: C-Profile (offen zur Aussteifung), Lochung zur Gasse, Aussteifung, Fussplatten, Distanzstuecke
    cprof = [(-0.045, -0.035), (0.045, -0.035), (0.045, 0.035), (0.033, 0.035), (0.033, 0.032), (0.0425, 0.032), (0.0425, -0.0325),
             (-0.0425, -0.0325), (-0.0425, 0.032), (-0.033, 0.032), (-0.033, 0.035), (-0.045, 0.035)]
    for xf in frames:
        for ri, (y0, y1) in enumerate(rows_y):
            for (yc, sgn) in ((y0 + 0.035, 1), (y1 - 0.035, -1)):
                prof = cprof if sgn > 0 else [(x, -y) for (x, y) in reversed(cprof)]
                K.prism(Rm['frame'], prof, 0.006, 5.2, T(xf, yc, 0), caps=(False, True))
                aisle = (ri == 0 and sgn > 0) or (ri == 1 and sgn < 0)
                if aisle:
                    for sx in (-1, 1):
                        for k in range(14):
                            z = 0.16 + k * 0.1
                            yy = yc - sgn * 0.016
                            K.poly(Rm['slot'], [(xf + sx * 0.0453, yy - 0.004, z), (xf + sx * 0.0453, yy + 0.004, z),
                                                (xf + sx * 0.0453, yy + 0.004, z + 0.026), (xf + sx * 0.0453, yy - 0.004, z + 0.026)],
                                   n=(sx, 0, 0))
                K.box(C['galv_dark'], (xf - 0.065, yc - 0.06, 0.0), (xf + 0.065, yc + 0.06, 0.006), skip=('-z',))
                if aisle:  # Anfahrspuren am Pfostenfuss (Lack ab, blankes Blech, etwas Rost)
                    yw = yc - sgn * 0.0358
                    for k in range(3):
                        z = rng.uniform(0.08, 0.55)
                        K.blob(C['steel'] if k else C['rust'], (xf + rng.uniform(-0.025, 0.025), yw, z), (sgn, 0, 0), (0, 0, 1),
                               rng.uniform(0.008, 0.02), rng.uniform(0.003, 0.012), 6, rng)
                K.hexnut(C['steel_dark'], (xf + 0.052, yc, 0.006), (0, 0, 1), 0.0085, 0.008)
            # Aussteifung zwischen den Pfosten (Zickzack)
            ya, yb = y0 + 0.06, y1 - 0.06
            zs = [0.14, 0.95, 1.78, 2.61, 3.44, 4.27, 5.08]
            for z in (zs[0], zs[-1]):
                bar(K, Rm['frame'], (xf, ya, z), (xf, yb, z), 0.012, 0.015, up=(1, 0, 0))
            for k in range(len(zs) - 1):
                pa = (xf, ya if k % 2 == 0 else yb, zs[k] + 0.02)
                pb = (xf, yb if k % 2 == 0 else ya, zs[k + 1] - 0.02)
                bar(K, Rm['frame'], pa, pb, 0.012, 0.015, up=(1, 0, 0))
        for z in (2.45, 4.95):
            K.box(Rm['frame'], (xf - 0.02, -0.1, z - 0.025), (xf + 0.02, 0.1, z + 0.025), skip=('-y', '+y'))
    # Anfahrschutz und Lastschild am Ende der Reihe
    if v & 4:
        xf = Lh
        for (yo, sgn) in ((-1.1, -1), (1.1, 1)):
            K.box(Rm['guard'], (xf - 0.06, yo + sgn * 0.002, 0.0), (xf + 0.07, yo + sgn * 0.008, 0.4))
            ya_, yb_ = sorted((yo + sgn * 0.008, yo - sgn * 0.08))
            K.box(Rm['guard'], (xf + 0.064, ya_, 0.0), (xf + 0.07, yb_, 0.4))
            for k in range(3):
                z = 0.06 + k * 0.12
                K.poly(Rm['guard_b'], [(xf - 0.06, yo + sgn * 0.0085, z), (xf + 0.07, yo + sgn * 0.0085, z + 0.05),
                                       (xf + 0.07, yo + sgn * 0.0085, z + 0.09), (xf - 0.06, yo + sgn * 0.0085, z + 0.04)], n=(0, sgn, 0))
        K.box(C['galv'], (xf + 0.013, 0.80, 1.42), (xf + 0.017, 0.96, 1.64))
        K.cell(C['paper'], (xf + 0.0175, 0.88, 1.53), (0, 0.075, 0), (0, 0, 0.1), 59)
    # --- Traversen (Stufenprofil), Einhaengeplatten, Sicherungen, Fachetiketten, Schablonennummern
    xa, xb = -Lh + 0.045, Lh - 0.045
    for z in (1.7, 3.2, 4.7):
        for (yo, yi) in ((-1.1, -1.05), (-0.1, -0.15), (0.1, 0.15), (1.1, 1.05)):
            if yi > yo:
                bp = [(yo, z - 0.12), (yi, z - 0.12), (yi, z - 0.015), (yi - 0.015, z - 0.015), (yi - 0.015, z), (yo, z)]
            else:
                bp = [(yi, z - 0.12), (yo, z - 0.12), (yo, z), (yi + 0.015, z), (yi + 0.015, z - 0.015), (yi, z - 0.015)]
            K.prism(Rm['beam'], bp, xa, xb, axis='x', caps=(True, True))
            aisle = abs(yo) > 1.0
            for xe, s in ((xa, 1), (xb, -1)):
                if aisle:
                    y_lo, y_hi = min(yo, yi), max(yo, yi)
                    K.box(Rm['beam'], (xe if s > 0 else xe - 0.004, y_lo - 0.003, z - 0.17), (xe + 0.004 if s > 0 else xe, y_hi + 0.003, z + 0.01))
                    yp = yo - math.copysign(0.0035, yi - yo)
                    K.poly(Rm['pin'], [(xe + s * 0.006, yp, z - 0.05), (xe + s * 0.012, yp, z - 0.05), (xe + s * 0.012, yp, z - 0.03),
                                       (xe + s * 0.006, yp, z - 0.03)], n=(0, math.copysign(1, yo), 0))
            if aisle:
                sgn = 1 if yo > 0 else -1
                rgt = (-sgn, 0, 0)
                for xs in (-0.88, 0.0, 0.88):
                    K.cell(C['paper'], (xs, yo + sgn * 0.0012, z - 0.06), (rgt[0] * 0.045, 0, 0), (0, 0, 0.022), 56 + rng.randrange(4))
                K.text(Rm['ink_w'], f'{rng.randrange(1, 40):02d}', (-0.45, yo + sgn * 0.0012, z - 0.06), rgt, (0, 0, 1), 0.06)
                # Kratzer vom Stapler
                for k in range(2):
                    xk = rng.uniform(-1.2, 1.2)
                    K.blob(C['steel'], (xk, yo + sgn * 0.0013, z - rng.uniform(0.09, 0.115)), rgt, (0, 0, 1),
                           rng.uniform(0.03, 0.09), 0.003, 5, rng, irr=0.15)
    # --- Ladung
    recipes = {
        0: (('A', 0.62), ('B', 0.12), ('F', 0.10), ('D', 0.08), ('C', 0.08)),
        1: (('B', 0.50), ('A', 0.22), ('C', 0.12), ('D', 0.16)),
        2: (('D', 0.30), ('E', 0.20), ('F', 0.26), ('A', 0.16), ('H', 0.08)),
        3: (('G', 0.46), ('A', 0.22), ('D', 0.12), ('F', 0.12), ('E', 0.08)),
        4: (('C', 0.48), ('A', 0.28), ('D', 0.10), ('B', 0.14)),
        5: (('A', 0.60), ('B', 0.25), ('H', 0.07), ('D', 0.08)),
        6: (('C', 0.25), ('B', 0.25), ('F', 0.20), ('E', 0.15), ('D', 0.15)),
        7: (('D', 0.50), ('E', 0.22), ('G', 0.13), ('A', 0.15)),
    }[v]

    def pick():
        u, acc = rng.random(), 0.0
        for kind, p in recipes:
            acc += p
            if u < acc:
                return kind
        return recipes[-1][0]

    levels = [0.0, 1.7, 3.2, 4.7]
    for li, zl in enumerate(levels):
        hmax = (levels[li + 1] - 0.20 - zl - 0.144) if li < 3 else (5.4 - zl - 0.144)
        for ri, yc in enumerate((-0.6, 0.6)):
            face = (0, -1) if ri == 0 else (0, 1)
            for xc in (-0.88, 0.0, 0.88):
                kind = pick()
                if kind == 'D':
                    if li == 0 and rng.random() < 0.3:
                        _carton(K, Rm, (xc + rng.uniform(-0.15, 0.15), yc + rng.uniform(-0.2, 0.2), 0.0), 0.5, 0.4, 0.35,
                                rng.uniform(0, 3), rng, 'offen' if rng.random() < 0.5 else 'zu', 1, face=face)
                    continue
                under = li > 0
                z0 = zl + 0.144
                detail = 1 if li <= 1 else 0
                xj = xc + rng.uniform(-0.02, 0.02)
                if kind == 'E':
                    n = rng.choice((1, 1, 2, 3)) if li == 0 else 1
                    for k in range(n):
                        _pallet(K, Rm, xj + rng.uniform(-0.02, 0.02), yc + rng.uniform(-0.02, 0.02), zl + k * 0.144, rng,
                                loaded=k < n - 1, under=under or k > 0, face=face[1])
                    continue
                _pallet(K, Rm, xj, yc, zl, rng, loaded=True, under=under, wet=(kind == 'G' and rng.random() < 0.7), face=face[1],
                        lod=li >= 1)
                if kind == 'A':
                    lay = rng.choice(('mix', 'mix', '1', '2', '4')) if li <= 1 else rng.choice(('1', '2', '2'))
                    _carton_load(K, Rm, xj, yc, z0, hmax * rng.uniform(0.6, 1.0), rng, layout=lay, detail=detail, face=face)
                elif kind == 'B':
                    _wrapped(K, Rm, xj, yc, z0, hmax, rng, face)
                elif kind == 'C':
                    if rng.random() < 0.5 or hmax < 0.7:
                        _crate(K, Rm, xj, yc, z0, 0.78, 1.15, min(hmax, rng.uniform(0.5, 0.8)), rng, face)
                    else:
                        hh = min(hmax / 2 - 0.01, rng.uniform(0.4, 0.55))
                        _crate(K, Rm, xj, yc - 0.29, z0, 0.76, 0.56, hh, rng, face, rng.uniform(-0.03, 0.03))
                        _crate(K, Rm, xj, yc + 0.29, z0, 0.76, 0.56, hh, rng, face, rng.uniform(-0.03, 0.03))
                        if rng.random() < 0.6:
                            _crate(K, Rm, xj + 0.01, yc - 0.29 * (1 if rng.random() < 0.5 else -1), z0 + hh, 0.74, 0.54,
                                   min(hmax - hh - 0.02, hh), rng, face, rng.uniform(-0.05, 0.05))
                elif kind == 'F':
                    zt = _carton_load(K, Rm, xj, yc + 0.29 * (-face[1]), z0, min(hmax, 0.45), rng, layout='2' if rng.random() < 0.5 else '4',
                                      detail=detail, face=face) if rng.random() < 0.6 else z0
                    for k in range(rng.choice((1, 2))):
                        _carton(K, Rm, (xj + rng.uniform(-0.15, 0.15), yc + face[1] * rng.uniform(0.2, 0.3), z0),
                                rng.uniform(0.35, 0.5), rng.uniform(0.3, 0.4), rng.uniform(0.25, min(0.4, hmax)), rng.uniform(-0.4, 0.4),
                                rng, 'offen', detail, face=face)
                    if hmax > 0.5:
                        Lc, Wc, Hc = 0.45, 0.35, 0.3
                        _carton(K, Rm, (xj + rng.uniform(-0.1, 0.1), yc - face[1] * 0.1, z0), Lc, Wc, Hc, rng.uniform(-0.6, 0.6), rng,
                                'zu', detail, face=face, tilt=T(0, Hc / 2, Wc / 2) @ R('x', math.pi / 2))
                elif kind == 'G':
                    _carton_load(K, Rm, xj, yc, z0, hmax * rng.uniform(0.55, 0.9), rng, layout=rng.choice(('1', '2')), state='nass',
                                 detail=detail, face=face, lean=rng.uniform(-0.025, 0.025))
                elif kind == 'H':
                    zz = z0
                    for k in range(rng.choice((1, 2))):
                        _flat_bundle(K, Rm, xj, yc, zz, rng, face)
                        zz += 0.27
    K.finish('rack')


def build_rack():
    _rack(0)


def build_rack__v1():
    _rack(1)


def build_rack__v2():
    _rack(2)


def build_rack__v3():
    _rack(3)


def build_rack__v4():
    _rack(4)


def build_rack__v5():
    _rack(5)


def build_rack__v6():
    _rack(6)


def build_rack__v7():
    _rack(7)
