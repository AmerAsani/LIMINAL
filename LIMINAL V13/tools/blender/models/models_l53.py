"""V13.3 Level 5.3 (LOOK_5.3.md, LEVELSPEC_5.3.md): Promethei Bibliotheca, neu gebaut (PROPS_Level5.3.md).

Regalwand der Halle mit drei Geschossen und Galerien, Regale der Gaenge, Stirnbrett, Tuffsaeule, Kandelaber, eiserner
Kronleuchter (weiss und rostig wie die Gitter von Level 3, Wiki), Gemaelde im Stil der Renaissance, Wappen an der Decke,
heruntergefallene Buecher, Inschrift, sehr alte Holztuer, abgeloeste Tapete.

Buecher sind einzelne Baende (Ruecken, Goldlinien, Schilder, Bünde, Schnitt, Abnutzung, verblichene Ruecken, Reihen
mehrbaendiger Werke, schief lehnende und liegende Buecher). Dreiecke sparsam: je Buch nur Ruecken, die sichtbaren Teile
der Seiten (Stufen zum Nachbarn) und - nur in Augenhoehe und darunter - der Schnitt oben. Runde Ruecken entstehen ohne
weitere Dreiecke ueber eigene Normalen je Ecke (Geo).
Die Fachboeden liegen auf Vielfachen von 0,375 m (Funde im Fach 3 bei 1,125 m, x = 0,36 m vor den Buechern).
"""
import math
import random

import bpy
from mathutils import Matrix, Vector

import common

ROW = 0.375
CANDLE_Z = 1.85


# ======================================================================================================================
# Hilfen: Flaechensammlung mit Normalen je Ecke, Profile entlang eines Wegs, Drehkoerper, Rohre
# ======================================================================================================================
def _nw(P):
    """Normale nach Newell (auch fuer nicht ebene oder konkave Vielecke)."""
    n = Vector((0.0, 0.0, 0.0))
    for i in range(len(P)):
        a, b = P[i], P[(i + 1) % len(P)]
        n.x += (a.y - b.y) * (a.z + b.z)
        n.y += (a.z - b.z) * (a.x + b.x)
        n.z += (a.x - b.x) * (a.y + b.y)
    return n


class Geo:
    """Flaechen mit eigenen Punkten, Material und Normalen je Ecke (glatt, ohne Verschweissen). M: Lage (Matrix).
    face(..., out=Richtung) dreht den Umlaufsinn so, dass die Flaeche nach aussen zeigt."""

    def __init__(self):
        self.F = []
        self.M = Matrix.Identity(4)

    def face(self, pts, mat, out=None, nrm=None, uv=None):
        M = self.M
        R = M.to_3x3()
        P = [M @ Vector(p) for p in pts]
        n = _nw(P)
        if n.length < 1e-10:
            return
        N = [(R @ Vector(v)).normalized() for v in nrm] if nrm else None
        if out is None and N:
            out = sum(N, Vector((0.0, 0.0, 0.0)))
            want = out
        else:
            want = R @ Vector(out) if out is not None else None
        if want is not None and n.dot(want) < 0.0:
            P.reverse()
            if N:
                N.reverse()
            if uv:
                uv = list(reversed(uv))
        self.F.append((P, mat, N, uv))

    def quad(self, a, b, c, d, mat, out=None, nrm=None):
        self.face([a, b, c, d], mat, out, nrm)

    def box(self, lo, hi, mat, skip=(), mats=None):
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        F = {'+x': ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
             '-x': ([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0)),
             '+y': ([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0)),
             '-y': ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0)),
             '+z': ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
             '-z': ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1))}
        for k, (P, o) in F.items():
            if k in skip:
                continue
            self.face(P, (mats or {}).get(k, mat), out=o, nrm=[o] * 4)

    def cbox(self, lo, hi, mat, c=0.003, skip=(), mats=None):
        """Quader mit gefasten Kanten; die Fasen haben die Normalen der Nachbarflaechen (wirkt gerundet)."""
        lo, hi = Vector(lo), Vector(hi)
        c = min(c, 0.45 * min(hi - lo))
        a, b = lo + Vector((c, c, c)), hi - Vector((c, c, c))

        def P(ax, sx, ay, sy, az, sz):  # Punkt: je Achse aussen (o) oder innen (i)
            v = [0.0, 0.0, 0.0]
            for k, (o, s) in enumerate(((ax, sx), (ay, sy), (az, sz))):
                if o == 'o':
                    v[k] = hi[k] if s > 0 else lo[k]
                else:
                    v[k] = b[k] if s > 0 else a[k]
            return Vector(v)
        E = [Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))]
        mats = mats or {}
        for ax in range(3):
            for s in (-1, 1):
                key = ('+' if s > 0 else '-') + 'xyz'[ax]
                if key in skip:
                    continue
                n = E[ax] * s
                u, w = (ax + 1) % 3, (ax + 2) % 3
                pts = []
                for (su, sw) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                    v = Vector((0.0, 0.0, 0.0))
                    v[ax] = hi[ax] if s > 0 else lo[ax]
                    v[u] = b[u] if su > 0 else a[u]
                    v[w] = b[w] if sw > 0 else a[w]
                    pts.append(v)
                self.face(pts, mats.get(key, mat), out=n, nrm=[n] * 4)
        # Fasen an den Kanten
        for ax in range(3):  # Kante parallel zu ax
            u, w = (ax + 1) % 3, (ax + 2) % 3
            for su in (-1, 1):
                for sw in (-1, 1):
                    nu, nw_ = E[u] * su, E[w] * sw
                    pts, nr = [], []
                    for sa in (-1, 1):
                        for which in (0, 1):
                            v = Vector((0.0, 0.0, 0.0))
                            v[ax] = b[ax] if sa > 0 else a[ax]
                            if which == 0:
                                v[u] = hi[u] if su > 0 else lo[u]
                                v[w] = b[w] if sw > 0 else a[w]
                            else:
                                v[u] = b[u] if su > 0 else a[u]
                                v[w] = hi[w] if sw > 0 else lo[w]
                            pts.append(v)
                            nr.append(nu if which == 0 else nw_)
                    pts = [pts[0], pts[1], pts[3], pts[2]]
                    nr = [nr[0], nr[1], nr[3], nr[2]]
                    self.face(pts, mat, out=(nu + nw_), nrm=nr)
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    s = (sx, sy, sz)
                    pts, nr = [], []
                    for ax in range(3):
                        v = Vector((0.0, 0.0, 0.0))
                        for k in range(3):
                            if k == ax:
                                v[k] = hi[k] if s[k] > 0 else lo[k]
                            else:
                                v[k] = b[k] if s[k] > 0 else a[k]
                        pts.append(v)
                        nr.append(E[ax] * s[ax])
                    self.face(pts, mat, out=Vector(s), nrm=nr)

    def build(self, name='teil'):
        verts, polys, mats, mi, idx, loopn, uvs = [], [], [], {}, [], [], []
        for P, mat, N, UV in self.F:
            k = mi.get(mat.name)
            if k is None:
                k = mi[mat.name] = len(mats)
                mats.append(mat)
            b = len(verts)
            verts.extend(P)
            polys.append(list(range(b, b + len(P))))
            idx.append(k)
            loopn.extend(N if N else [_nw(P).normalized()] * len(P))
            uvs.extend(UV if UV else [(0.0, 0.0)] * len(P))
        me = bpy.data.meshes.new(name)
        me.from_pydata([tuple(v) for v in verts], [], polys)
        for m in mats:
            me.materials.append(m)
        me.polygons.foreach_set('material_index', idx)
        if any(u != (0.0, 0.0) for u in uvs):
            lay = me.uv_layers.new(name='UVMap')
            lay.data.foreach_set('uv', [c for u in uvs for c in u])
        me.update()
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        me.shade_smooth()
        me.normals_split_custom_set([tuple(n) for n in loopn])
        return ob


def _ang(a, b):
    return math.degrees(math.acos(max(-1.0, min(1.0, a[0] * b[0] + a[1] * b[1]))))


def _n2(prof, flip=False):
    """2D-Normalen der Strecken eines Profils (u, v): rechts der Laufrichtung (flip: links)."""
    N = []
    for k in range(len(prof) - 1):
        du, dv = prof[k + 1][0] - prof[k][0], prof[k + 1][1] - prof[k][1]
        ln = math.hypot(du, dv) or 1.0
        N.append((-dv / ln, du / ln) if flip else (dv / ln, -du / ln))
    return N


def sweep(g, prof, path, up, mat, closed=False, smooth=40.0, flip=False, mats=None, caps=False):
    """Profil [(u, v)] entlang eines Wegs (Punkte); u quer (t x up), v entlang up. Gehrung an den Ecken.
    Ausserhalb liegt rechts der Laufrichtung des Profils (flip: links). caps: Profil ist geschlossen -> Deckel."""
    up = Vector(up).normalized()
    P = [Vector(p) for p in path]
    n = len(P)
    nseg = n if closed else n - 1
    T = [(P[(j + 1) % n] - P[j]).normalized() for j in range(nseg)]
    ring = []
    for i in range(n):
        if closed:
            t0, t1 = T[i - 1], T[i % nseg]
        else:
            t0, t1 = T[max(0, i - 1)], T[min(i, nseg - 1)]
        m = (t0 + t1).normalized()
        s = m.cross(up).normalized()
        c = max(0.3, s.dot(t1.cross(up).normalized()))
        ring.append([P[i] + s * (u / c) + up * v for (u, v) in prof])
    N2 = _n2(prof, flip)
    np_ = len(prof)
    for j in range(nseg):
        i0, i1 = j, (j + 1) % n
        sd = T[j].cross(up).normalized()

        def n3(a):
            return (sd * a[0] + up * a[1]).normalized()
        for k in range(np_ - 1):
            nk = n3(N2[k])
            na = nb = nk
            if k > 0 and _ang(N2[k - 1], N2[k]) < smooth:
                na = n3((N2[k - 1][0] + N2[k][0], N2[k - 1][1] + N2[k][1]))
            if k < np_ - 2 and _ang(N2[k], N2[k + 1]) < smooth:
                nb = n3((N2[k][0] + N2[k + 1][0], N2[k][1] + N2[k + 1][1]))
            g.face([ring[i0][k], ring[i1][k], ring[i1][k + 1], ring[i0][k + 1]], mats[k] if mats else mat, out=nk,
                   nrm=[na, na, nb, nb])
    if caps and not closed:
        g.face(ring[0], mat, out=-T[0])
        g.face(ring[-1], mat, out=T[-1])


def lathe(g, prof, mat, seg=12, at=(0.0, 0.0, 0.0), smooth=40.0, mats=None, a0=0.0, sx=1.0, sy=1.0, flip=False, mfn=None):
    """Drehkoerper um z: prof [(r, z)] von unten nach oben, aussen rechts der Laufrichtung."""
    at = Vector(at)
    N2 = _n2(prof, flip)
    A = [a0 + 2 * math.pi * i / seg for i in range(seg + 1)]

    def p(k, i):
        r, z = prof[k]
        return at + Vector((r * math.cos(A[i]) * sx, r * math.sin(A[i]) * sy, z))

    def nn(a2, i):
        v = Vector((a2[0] * math.cos(A[i]) / max(sx, 1e-6), a2[0] * math.sin(A[i]) / max(sy, 1e-6), a2[1]))
        return v.normalized()
    for k in range(len(prof) - 1):
        na2 = nb2 = N2[k]
        if k > 0 and _ang(N2[k - 1], N2[k]) < smooth:
            na2 = (N2[k - 1][0] + N2[k][0], N2[k - 1][1] + N2[k][1])
        if k < len(prof) - 2 and _ang(N2[k], N2[k + 1]) < smooth:
            nb2 = (N2[k][0] + N2[k + 1][0], N2[k][1] + N2[k + 1][1])
        for i in range(seg):
            pts = [p(k, i), p(k, i + 1), p(k + 1, i + 1), p(k + 1, i)]
            nr = [nn(na2, i), nn(na2, i + 1), nn(nb2, i + 1), nn(nb2, i)]
            if prof[k][0] < 1e-6:
                pts, nr = pts[1:], nr[1:]
            elif prof[k + 1][0] < 1e-6:
                pts, nr = pts[:3], nr[:3]
            mid = (A[i] + A[i + 1]) * 0.5
            out = Vector((N2[k][0] * math.cos(mid), N2[k][0] * math.sin(mid), N2[k][1]))
            g.face(pts, mfn(k, i) if mfn else (mats[k] if mats else mat), out=out, nrm=nr)


def tube(g, path, r, mat, sides=6, caps=False, radii=None, closed=False, mats=None, twist=0.0):
    """Rohr (Kreisquerschnitt, sides Ecken) entlang eines Wegs; Rahmen mitgefuehrt (ohne Verdrehen)."""
    P = [Vector(p) for p in path]
    n = len(P)
    T = []
    for i in range(n):
        if closed:
            t = P[(i + 1) % n] - P[i - 1]
        else:
            t = P[min(i + 1, n - 1)] - P[max(i - 1, 0)]
        T.append(t.normalized())
    ref = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
    N = [(ref - T[0] * ref.dot(T[0])).normalized()]
    for i in range(1, n):
        v = N[-1] - T[i] * N[-1].dot(T[i])
        N.append(v.normalized() if v.length > 1e-9 else N[-1])
    rings = []
    for i in range(n):
        B = T[i].cross(N[i])
        ri = radii[i] if radii else r
        ring, nr = [], []
        for k in range(sides):
            a = 2 * math.pi * k / sides + twist * i
            d = N[i] * math.cos(a) + B * math.sin(a)
            ring.append(P[i] + d * ri)
            nr.append(d)
        rings.append((ring, nr))
    segs = n if closed else n - 1
    for i in range(segs):
        (r0, n0), (r1, n1) = rings[i], rings[(i + 1) % n]
        for k in range(sides):
            k1 = (k + 1) % sides
            g.face([r0[k], r0[k1], r1[k1], r1[k]], mats[i] if mats else mat, out=n0[k] + n0[k1], nrm=[n0[k], n0[k1], n1[k1], n1[k]])
    if caps and not closed:
        g.face(rings[0][0], mat, out=-T[0])
        g.face(rings[-1][0], mat, out=T[-1])


def mat(name, rgb, look='paint', tex=None, glow=0.0, r=None, m=None):
    return common.material(name, rgb, look=look, tex=tex, glow=glow, r=r, m=m)


def _mix(c, d, t):
    return tuple(c[k] * (1 - t) + d[k] * t for k in range(3))


# ======================================================================================================================
# Materialien der Bibliothek
# ======================================================================================================================
def _m():
    return {
        'walnut': mat('nussbaum', (0.40, 0.25, 0.15), 'wood', 'wood_walnut_floor', r=2.0),
        'walnut_d': mat('nussbaum_dunkel', (0.26, 0.155, 0.095), 'wood', 'wood_walnut_floor', r=2.2),
        'walnut_w': mat('nussbaum_abgegriffen', (0.50, 0.33, 0.20), 'wood', 'wood_walnut_floor', r=1.6),
        'groove': mat('fuge_dunkel', (0.05, 0.035, 0.025), 'paint', r=1.4),
        'dust': mat('staub', (0.27, 0.23, 0.18), 'paint', r=1.5),
        'iron': mat('schmiedeeisen', (0.07, 0.07, 0.068), 'paint', r=0.75, m=0.45),
        'iron_w': mat('schmiedeeisen_blank', (0.20, 0.19, 0.18), 'metal', r=0.6),
        'brass': mat('messing_alt', (0.74, 0.56, 0.28), 'metal', r=0.5),
        'brass_d': mat('messing_angelaufen', (0.42, 0.32, 0.17), 'metal', r=0.8, m=0.6),
        'bronze': mat('bronze', (0.52, 0.37, 0.21), 'metal', r=0.55),
        'bronze_d': mat('bronze_patina', (0.22, 0.17, 0.11), 'metal', r=0.9, m=0.45),
        'gold': mat('blattgold', (0.85, 0.66, 0.32), 'metal', r=0.45, m=0.85),
        'gold_w': mat('blattgold_abgerieben', (0.58, 0.44, 0.22), 'metal', r=0.8, m=0.5),
        'wax': mat('wachs', (0.90, 0.86, 0.76), 'paint', r=0.55),
        'wax_o': mat('wachs_alt', (0.84, 0.76, 0.60), 'paint', r=0.6),
        'wick': mat('docht', (0.05, 0.04, 0.035), 'paint', r=1.2),
        'flame': mat('flamme', (1.0, 0.76, 0.42), 'glow', glow=3.2),
        'flame_c': mat('flamme_kern', (1.0, 0.93, 0.76), 'glow', glow=5.0),
        'paper': mat('papier', (0.86, 0.82, 0.70), 'paint', r=1.25),
        'paper_o': mat('papier_alt', (0.78, 0.69, 0.52), 'paint', r=1.3),
        'ink': mat('tinte', (0.10, 0.08, 0.07), 'paint', r=1.0),
    }


# Einbaende: Name, Farbe (sRGB), Art (l Leder, c Leinen, v Pergament)
BINDINGS = [
    ('ochsenblut', (0.33, 0.07, 0.05), 'l'), ('rot', (0.45, 0.11, 0.07), 'l'), ('braun', (0.29, 0.165, 0.09), 'l'),
    ('dunkelbraun', (0.16, 0.095, 0.06), 'l'), ('kalb', (0.47, 0.30, 0.15), 'l'), ('ocker', (0.55, 0.39, 0.17), 'l'),
    ('oliv', (0.20, 0.22, 0.10), 'l'), ('flaschengruen', (0.08, 0.19, 0.11), 'l'), ('marine', (0.08, 0.11, 0.23), 'c'),
    ('schwarz', (0.05, 0.045, 0.04), 'l'), ('pergament', (0.74, 0.66, 0.50), 'v'), ('violett', (0.24, 0.08, 0.13), 'l'),
    ('graublau', (0.24, 0.29, 0.34), 'c'), ('leinen_gruen', (0.22, 0.30, 0.22), 'c'), ('leinen_rot', (0.40, 0.14, 0.11), 'c'),
    ('kastanie', (0.36, 0.19, 0.10), 'l'),
]
WEIGHTS = [8, 6, 10, 9, 7, 4, 4, 5, 4, 6, 3, 3, 3, 3, 3, 7]
FADE = (0.60, 0.53, 0.43)


class Books:
    """Materialien der Baende (einmal je Modell)."""

    def __init__(self):
        self.spine, self.faded, self.worn = [], [], []
        for (nm, c, kind) in BINDINGS:
            look, r = ('fabric', 1.15) if kind == 'c' else ('paint', 0.8 if kind == 'l' else 0.95)
            self.spine.append(mat('einband_' + nm, c, look, r=r))
            self.faded.append(mat('einband_' + nm + '_verblichen', _mix(c, FADE, 0.38), look, r=r + 0.15))
            self.worn.append(mat('einband_' + nm + '_abgewetzt', _mix(c, (0.62, 0.52, 0.40), 0.5), 'paint', r=1.2))
        self.kind = [b[2] for b in BINDINGS]
        self.marble = [mat('marmorpapier_braun', (0.36, 0.24, 0.17), 'paint', r=1.0),
                       mat('marmorpapier_blau', (0.20, 0.23, 0.30), 'paint', r=1.0),
                       mat('marmorpapier_rot', (0.38, 0.17, 0.13), 'paint', r=1.0)]
        self.gold = mat('goldpraegung', (0.80, 0.62, 0.31), 'metal', r=0.5, m=0.75)
        self.gold_w = mat('goldpraegung_matt', (0.55, 0.43, 0.24), 'metal', r=0.85, m=0.45)
        self.labels = [mat('schild_rot', (0.42, 0.08, 0.06), 'paint', r=0.7), mat('schild_schwarz', (0.06, 0.05, 0.045), 'paint', r=0.7),
                       mat('schild_gruen', (0.10, 0.20, 0.12), 'paint', r=0.7)]
        self.paper_label = mat('schild_papier', (0.80, 0.74, 0.60), 'paint', r=1.2)
        self.ink = mat('tinte_schild', (0.12, 0.09, 0.08), 'paint', r=1.0)
        self.edges = [mat('schnitt', (0.78, 0.72, 0.58), 'paint', r=1.3), mat('schnitt_staub', (0.55, 0.51, 0.45), 'paint', r=1.5),
                      mat('schnitt_rot', (0.52, 0.20, 0.14), 'paint', r=1.2), mat('schnitt_gold', (0.70, 0.55, 0.30), 'metal', r=0.75, m=0.55)]


def _pick(rng, weights):
    t = rng.random() * sum(weights)
    for i, w in enumerate(weights):
        t -= w
        if t <= 0:
            return i
    return len(weights) - 1


SIZES = {  # Hoehe, Dicke (Bereiche) je Format
    'folio': ((0.29, 0.335), (0.04, 0.09)),
    'quarto': ((0.235, 0.295), (0.028, 0.065)),
    'octavo': ((0.175, 0.235), (0.02, 0.048)),
    'duodez': ((0.14, 0.18), (0.017, 0.034)),
}


def _style(rng, BK, col, h):
    """Verzierung eines Rueckens: Liste (z0, z1, Material) relativ zum Fuss."""
    kind = BK.kind[col]
    d = []
    r = rng.random()
    if kind == 'v':  # Pergament: Papierschild mit Signatur
        if r < 0.7:
            z0 = h * rng.uniform(0.62, 0.72)
            d.append((z0, z0 + h * 0.11, BK.paper_label))
            d.append((z0 + h * 0.045, z0 + h * 0.06, BK.ink))
        return d
    if kind == 'c':  # Leinen: Goldtitel oben, Linie unten
        if r < 0.65:
            d.append((h * 0.80, h * 0.86, BK.gold_w if rng.random() < 0.5 else BK.gold))
        if rng.random() < 0.4:
            d.append((h * 0.07, h * 0.085, BK.gold_w))
        return d
    if r < 0.30:  # Buende mit Goldlinien
        nb = 3 if h > 0.22 else 2
        for k in range(nb):
            zc = h * (0.16 + 0.68 * k / (nb - 1))
            d.append((zc - 0.004, zc + 0.004, BK.gold_w if rng.random() < 0.6 else BK.gold))
    elif r < 0.62:  # Schild (Titel) und Linien an Kopf und Fuss
        lab = BK.labels[rng.randrange(3)]
        z0 = h * rng.uniform(0.66, 0.74)
        d.append((z0, z0 + h * 0.10, lab))
        if rng.random() < 0.5:
            d.append((h * 0.92, h * 0.935, BK.gold))
    elif r < 0.82:
        d.append((h * 0.915, h * 0.93, BK.gold_w))
        d.append((h * 0.055, h * 0.07, BK.gold_w))
    return d


class Book:
    __slots__ = ('y0', 'y1', 'h', 'xf', 'd', 'mat', 'side', 'deco', 'edge', 'alpha', 'worn')

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))


def _make_book(rng, BK, y, t, h, xf, col=None, faded=None, deco=None, edge=None, side=None):
    if col is None:
        col = _pick(rng, WEIGHTS)
    if faded is None:
        faded = rng.random() < 0.14
    kind = BK.kind[col]
    if side is None:
        side = BK.marble[rng.randrange(3)] if (kind == 'l' and rng.random() < 0.3) else None
    m = BK.faded[col] if faded else BK.spine[col]
    worn = kind == 'l' and rng.random() < 0.28
    alpha = (0.75 if kind == 'l' else 0.5 if kind == 'c' else 0.6) * min(1.0, 0.35 + t / 0.04)
    return Book(y0=y, y1=y + t, h=h, xf=xf, d=min(h * rng.uniform(0.66, 0.78), xf - 0.025), mat=m, side=side or m,
                deco=_style(rng, BK, col, h) if deco is None else deco,
                edge=edge if edge is not None else BK.edges[_pick(rng, [5, 4, 1.2, 0.8])], alpha=alpha,
                worn=(BK.worn[col] if worn else None))


def _spine_normals(alpha):
    ca, sa = math.cos(alpha), math.sin(alpha)
    return Vector((ca, -sa, 0.0)), Vector((ca, sa, 0.0))


def _emit_upright(g, b, zb, tops, left, right):
    """Ein stehendes Buch: Ruecken (rund ueber Normalen), Verzierung, Schnitt (nur wenn sichtbar), sichtbare Seiten."""
    y0, y1, h, xf = b.y0, b.y1, b.h, b.xf
    nl, nr = _spine_normals(b.alpha)
    g.face([(xf, y0, zb), (xf, y1, zb), (xf, y1, zb + h), (xf, y0, zb + h)], b.mat, out=(1, 0, 0), nrm=[nl, nr, nr, nl])
    e = min(0.0008, (y1 - y0) * 0.05)
    x = xf + 0.0008
    deco = list(b.deco)
    if b.worn is not None:
        deco.append((h - 0.012, h, b.worn))
    for (z0, z1, m) in deco:
        g.face([(x, y0 + e, zb + z0), (x, y1 - e, zb + z0), (x, y1 - e, zb + z1), (x, y0 + e, zb + z1)], m, out=(1, 0, 0),
               nrm=[nl, nr, nr, nl])
    if tops:
        g.face([(xf - b.d, y0, zb + h), (xf, y0, zb + h), (xf, y1, zb + h), (xf - b.d, y1, zb + h)], b.edge, out=(0, 0, 1))
    for s, nb in ((-1, left), (1, right)):
        _emit_side(g, b, nb, zb, s)


def _emit_side(g, b, nb, zb, s):
    y = b.y0 if s < 0 else b.y1
    a0, a1, h = b.xf - b.d, b.xf, b.h
    o = (0, s, 0)
    touching = nb is not None and abs((nb.y1 if s < 0 else nb.y0) - y) < 0.0015
    if not touching:
        rects = [[(a0, 0.0), (a1, 0.0), (a1, h), (a0, h)]]
    else:
        b1, h2 = nb.xf, nb.h
        if h > h2 + 0.001 and a1 > b1 + 0.001:
            rects = [[(a0, h2), (b1, h2), (b1, 0.0), (a1, 0.0), (a1, h), (a0, h)]]
        elif h > h2 + 0.001:
            rects = [[(a0, h2), (a1, h2), (a1, h), (a0, h)]]
        elif a1 > b1 + 0.001:
            rects = [[(b1, 0.0), (a1, 0.0), (a1, h), (b1, h)]]
        else:
            rects = []
    for R in rects:
        g.face([(xx, y, zb + zz) for (xx, zz) in R], b.side, out=o)


def _emit_box_book(g, M, L, D, T, mat_cover, mat_edge, alpha=0.6, deco=(), full=False, side=None):
    """Liegendes Buch in eigener Lage M: Ruecken bei x = 0 (blickt +x), Tiefe D nach -x, Breite L entlang y, Dicke T
    entlang z. Sichtbar: Ruecken, Deckel oben, Schnitt an Kopf und Fuss zwischen den Deckeln; full: auch vorn
    (Vorderschnitt) und unten."""
    old = g.M
    g.M = old @ M
    ct = min(0.003, T * 0.12)
    ca, sa = math.cos(alpha), math.sin(alpha)
    nb, nt = Vector((ca, 0, -sa)), Vector((ca, 0, sa))
    sd = side or mat_cover
    g.face([(0, 0, 0), (0, L, 0), (0, L, T), (0, 0, T)], mat_cover, out=(1, 0, 0), nrm=[nb, nb, nt, nt])
    for (y0, y1, m) in deco:
        g.face([(0.0008, y0, ct), (0.0008, y1, ct), (0.0008, y1, T - ct), (0.0008, y0, T - ct)], m, out=(1, 0, 0), nrm=[nb, nb, nt, nt])
    g.face([(-D, 0, T), (0, 0, T), (0, L, T), (-D, L, T)], sd, out=(0, 0, 1))
    if full:
        g.face([(-D, 0, 0), (0, 0, 0), (0, L, 0), (-D, L, 0)], sd, out=(0, 0, -1))
    for (y, s) in ((0.0, -1), (L, 1)):  # Kopf und Fuss: Schnitt zwischen den Deckeln
        g.face([(-D, y, ct), (-0.004, y, ct), (-0.004, y, T - ct), (-D, y, T - ct)], mat_edge, out=(0, s, 0))
        g.face([(-D, y, 0), (0, y, 0), (0, y, ct), (-D, y, ct)], sd, out=(0, s, 0))
        g.face([(-D, y, T - ct), (0, y, T - ct), (0, y, T), (-D, y, T)], sd, out=(0, s, 0))
        g.face([(-0.004, y, ct), (0, y, ct), (0, y, T - ct), (-0.004, y, T - ct)], mat_cover, out=(0, s, 0))
    if full:
        g.face([(-D, 0, ct), (-D, L, ct), (-D, L, T - ct), (-D, 0, T - ct)], mat_edge, out=(-1, 0, 0))
        g.face([(-D, 0, 0), (-D, L, 0), (-D, L, ct), (-D, 0, ct)], sd, out=(-1, 0, 0))
        g.face([(-D, 0, T - ct), (-D, L, T - ct), (-D, L, T), (-D, 0, T)], sd, out=(-1, 0, 0))
    g.M = old


MIX = {  # Anteile: mehrbaendige Werke, gemischte Baende, liegender Stapel, lehnendes Buch, leere Luecke
    'ordentlich': (0.32, 0.53, 0.06, 0.05, 0.04),
    'unordentlich': (0.20, 0.50, 0.12, 0.10, 0.08),
}


def _row(g, BK, rng, y0, y1, zb, fmt, xf_lo, xf_hi, tops_below, dust_mat=None, mix='ordentlich'):
    """Eine Reihe Buecher auf dem Fachboden zb zwischen y0 und y1. fmt: Format der Reihe (mit Beimischung).
    Ruecken zwischen xf_lo und xf_hi. tops_below: Schnitt oben nur fuer Buecher mit Oberkante darunter (Sichtbarkeit)."""
    order = ['folio', 'quarto', 'octavo', 'duodez']
    fi = order.index(fmt)
    P = MIX[mix]
    acc = [sum(P[:k + 1]) for k in range(5)]
    items = []
    y = y0 + rng.uniform(0.0, 0.004)
    xf_row = rng.uniform(xf_lo, xf_hi)

    def size(f=None):
        f = f or order[max(0, min(3, fi + (0 if rng.random() < 0.72 else rng.choice((-1, 1)))))]
        (h0, h1), (t0, t1) = SIZES[f]
        return rng.uniform(h0, h1), rng.uniform(t0, t1)

    def xf_j(k=0.006):
        return min(xf_hi + 0.01, max(xf_lo - 0.015, xf_row + rng.uniform(-k, k)))
    last_up = None
    while y < y1 - 0.016:
        r = rng.random() * acc[-1]
        room = y1 - y
        if r < acc[0]:  # mehrbaendiges Werk: gleiche Ruecken, gleiche Hoehe
            h, t = size(fmt)
            col = _pick(rng, WEIGHTS)
            faded = rng.random() < 0.15
            deco = _style(rng, BK, col, h)
            edge = BK.edges[_pick(rng, [5, 4, 1.2, 0.8])]
            side = BK.marble[rng.randrange(3)] if (BK.kind[col] == 'l' and rng.random() < 0.3) else None
            for i in range(rng.randint(3, 9)):
                if y + t > y1:
                    break
                if rng.random() < 0.05 and i > 0:  # ein Band fehlt
                    y += t
                    continue
                xf = xf_row + rng.uniform(-0.002, 0.002)
                if rng.random() < 0.06:
                    xf += rng.uniform(0.01, 0.03)  # herausgezogen
                b = _make_book(rng, BK, y, t, h, min(xf, 0.432, xf_hi + 0.012), col, faded, deco, edge, side)
                items.append(('u', b))
                last_up = b
                y += t
            y += rng.choice((0.0, 0.0, 0.0, 0.002, 0.004))
        elif r < acc[1] or room < 0.12:  # gemischte Baende
            for i in range(rng.randint(2, 7)):
                h, t = size()
                if y + t > y1:
                    t = y1 - y
                    if t < 0.014:
                        break
                b = _make_book(rng, BK, y, t, h, xf_j())
                items.append(('u', b))
                last_up = b
                y += t + rng.choice((0.0, 0.0, 0.0, 0.0, 0.002, 0.003))
        elif r < acc[2]:  # liegender Stapel
            stack, zz, L = [], 0.0, 0.0
            for i in range(rng.randint(1, 4)):
                Lb = rng.uniform(0.17, 0.30) * (1.0 - 0.08 * i)
                Tb = rng.uniform(0.022, 0.055)
                if zz + Tb > 0.33:
                    break
                stack.append((Lb, Tb, zz))
                zz += Tb
                L = max(L, Lb)
            if not stack or y + L + 0.012 > y1:
                y += 0.01
                continue
            items.append(('lie', y + 0.004, stack, xf_j(0.012)))
            y += L + 0.012
            last_up = None
        elif r < acc[3]:  # lehnt in einer Luecke an seinem linken Nachbarn
            h, t = size()
            th = math.radians(rng.uniform(9.0, 24.0))
            hl = last_up.h if last_up is not None else h
            yp = y + min(hl, h) * math.tan(th) + 0.002
            if yp + t * math.cos(th) > y1:
                y += 0.02
                continue
            items.append(('lean', yp, h, t, th, xf_j()))
            y = yp + t * math.cos(th) + rng.uniform(0.003, 0.025)
            last_up = None
        else:  # leere Luecke
            y += rng.uniform(0.02, 0.07)
            last_up = None
    ups = [it[1] for it in items if it[0] == 'u']
    for i, b in enumerate(ups):
        left = ups[i - 1] if i > 0 else None
        right = ups[i + 1] if i + 1 < len(ups) else None
        _emit_upright(g, b, zb, zb + b.h < tops_below, left, right)
    for it in items:
        if it[0] == 'lie':
            _, ys, stack, xf = it
            for (Lb, Tb, zz) in stack:
                col = _pick(rng, WEIGHTS)
                D = min(Lb * rng.uniform(0.68, 0.78), xf - 0.03)
                M = Matrix.Translation((xf + rng.uniform(-0.008, 0.008), ys + (stack[0][0] - Lb) * 0.5 + rng.uniform(-0.008, 0.008), zb + zz)) @ \
                    Matrix.Rotation(rng.uniform(-0.06, 0.06), 4, 'Z')
                deco = [] if rng.random() < 0.4 else [(Lb * 0.08, Lb * 0.095, BK.gold), (Lb * 0.9, Lb * 0.915, BK.gold)]
                _emit_box_book(g, M, Lb, D, Tb, BK.spine[col], BK.edges[rng.randrange(2)], 0.6, deco,
                               side=BK.marble[rng.randrange(3)] if rng.random() < 0.3 else None)
        elif it[0] == 'lean':
            _, yp, h, t, th, xf = it
            old = g.M
            g.M = old @ Matrix.Translation((xf, yp, zb)) @ Matrix.Rotation(th, 4, 'X')
            _emit_lean(g, BK, rng, t, h, min(h * 0.72, xf - 0.04))
            g.M = old
    if dust_mat is not None:  # Staub auf der Kante vor den Buechern
        g.face([(xf_lo - 0.012, y0, zb + 0.0006), (0.424, y0, zb + 0.0006), (0.424, y1, zb + 0.0006), (xf_lo - 0.012, y1, zb + 0.0006)],
               dust_mat, out=(0, 0, 1))


def _emit_lean(g, BK, rng, t, h, D):
    """Lehnendes Buch in lokaler Lage: Fuss-Kante links unten im Ursprung, Ruecken bei x = 0, Breite t (+y), Hoehe h (+z)."""
    col = _pick(rng, WEIGHTS)
    m = BK.spine[col]
    nl, nr = _spine_normals(0.7)
    g.face([(0, 0, 0), (0, t, 0), (0, t, h), (0, 0, h)], m, out=(1, 0, 0), nrm=[nl, nr, nr, nl])
    for (z0, z1, dm) in _style(rng, BK, col, h):
        g.face([(0.0008, 0.0008, z0), (0.0008, t - 0.0008, z0), (0.0008, t - 0.0008, z1), (0.0008, 0.0008, z1)], dm, out=(1, 0, 0),
               nrm=[nl, nr, nr, nl])
    g.face([(-D, 0, h), (-0.004, 0, h), (-0.004, t, h), (-D, t, h)], BK.edges[rng.randrange(2)], out=(0, 0, 1))
    g.face([(-0.004, 0, h), (0, 0, h), (0, t, h), (-0.004, t, h)], m, out=(0, 0, 1))
    g.face([(-D, 0, 0), (0, 0, 0), (0, t, 0), (-D, t, 0)], BK.edges[1], out=(0, 0, -1))
    side = BK.marble[rng.randrange(3)] if rng.random() < 0.3 else m
    g.face([(-D, 0, 0), (0, 0, 0), (0, 0, h), (-D, 0, h)], side, out=(0, -1, 0))
    g.face([(-D, t, 0), (0, t, 0), (0, t, h), (-D, t, h)], side, out=(0, 1, 0))

rng_global = random.Random(53)


# ======================================================================================================================
# Regale
# ======================================================================================================================
YI = 0.465  # Innenkante der Pilaster (Buecher und Fachboeden dazwischen)


def _board(g, M, zb, y0=-YI, y1=YI, front=0.438):
    """Fachboden mit gerundeter Vorderkante (Oberseite bei zb), Profil quer, entlang y gezogen."""
    prof = [(0.02, zb - 0.022), (front - 0.032, zb - 0.022), (front - 0.032, zb - 0.03), (front - 0.01, zb - 0.03),
            (front - 0.003, zb - 0.025), (front, zb - 0.013), (front - 0.003, zb - 0.004), (front - 0.009, zb), (0.02, zb)]
    sweep(g, prof, [(0, y0, 0), (0, y1, 0)], (0, 0, 1), M['walnut'], smooth=55,
          mats=[M['walnut'], M['walnut_d'], M['walnut'], M['walnut'], M['walnut_w'], M['walnut_w'], M['walnut'], M['walnut']])


def _pilaster(g, M, s, z0, z1, front=0.445):
    """Halber Pilaster vorn an der Seitenwand (s = +1: rechts bei y = 0,5); der Nachbar ergaenzt die andere Haelfte."""
    yo = 0.5 * s
    yi = YI * s
    # Innenseite der Seitenwand zwischen Rueckwand und Pilaster
    g.face([(0.02, yi, z0), (0.42, yi, z0), (0.42, yi, z1), (0.02, yi, z1)], M['walnut_d'], out=(0, -s, 0))
    prof = [(0.42, yi), (front - 0.008, yi), (front, yi + 0.008 * s), (front, yo - 0.004 * s), (front - 0.003, yo)]
    P = [(x, y, z0) for (x, y) in prof]
    Q = [(x, y, z1) for (x, y) in prof]
    mats = [M['walnut'], M['walnut_w'], M['walnut'], M['groove']]
    N2 = []
    for k in range(len(prof) - 1):
        dx, dy = prof[k + 1][0] - prof[k][0], prof[k + 1][1] - prof[k][1]
        ln = math.hypot(dx, dy)
        n = Vector((dy / ln, -dx / ln, 0.0)) * s
        N2.append(n)
    for k in range(len(prof) - 1):
        n = N2[k]
        g.face([P[k], P[k + 1], Q[k + 1], Q[k]], mats[k], out=n, nrm=[n] * 4)
    # Aussenseite (an Reihenenden sichtbar)
    g.face([(0.0, yo, z0), (front - 0.003, yo, z0), (front - 0.003, yo, z1), (0.0, yo, z1)], M['walnut_d'], out=(0, s, 0))


def _back(g, M, z0, z1):
    """Rueckwand aus vier senkrechten Brettern mit Fugen."""
    g.face([(0.02, -YI, z0), (0.02, YI, z0), (0.02, YI, z1), (0.02, -YI, z1)], M['walnut_d'], out=(1, 0, 0))
    for k in (-1, 0, 1):
        y = k * 0.235 + 0.01
        g.face([(0.0204, y - 0.0025, z0), (0.0204, y + 0.0025, z0), (0.0204, y + 0.0025, z1), (0.0204, y - 0.0025, z1)], M['groove'],
               out=(1, 0, 0))


def _plinth(g, M, z1=ROW):
    prof = [(0.02, z1 - 0.03), (0.445, z1 - 0.03), (0.445, 0.05), (0.452, 0.04), (0.458, 0.028), (0.458, 0.0), (0.02, 0.0)]
    # Laufrichtung: vom oberen hinteren Punkt nach vorn und herunter; aussen = rechts
    sweep(g, list(reversed(prof)), [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['walnut_d'], smooth=50,
          mats=[M['walnut_d'], M['walnut_d'], M['walnut_w'], M['walnut_d'], M['walnut_d'], M['walnut_d']])
    for s in (-1, 1):
        g.face([(x, 0.5 * s, z) for (x, z) in prof], M['walnut_d'], out=(0, s, 0))


def _crown(g, M, z0, top=None):
    """Kranz: Fries, Zahnleiste, Platte, Karnies (bis x = 0,52, Oberkante z0 + 0,16)."""
    prof = [(0.0, z0 + 0.16), (0.52, z0 + 0.16), (0.52, z0 + 0.15), (0.505, z0 + 0.142), (0.492, z0 + 0.128), (0.488, z0 + 0.118),
            (0.49, z0 + 0.105), (0.49, z0 + 0.085), (0.47, z0 + 0.085), (0.462, z0 + 0.075), (0.452, z0 + 0.06), (0.448, z0 + 0.05),
            (0.448, z0 - 0.025), (0.02, z0 - 0.025)]
    sweep(g, prof, [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['walnut'], smooth=50, flip=True,
          mats=[M['walnut_d'], M['walnut'], M['walnut_w'], M['walnut'], M['walnut'], M['walnut_w'], M['walnut_d'], M['walnut'],
                M['walnut'], M['walnut_w'], M['walnut'], M['walnut_d'], M['walnut_d']])
    for s in (-1, 1):
        g.face([(x, 0.5 * s, z) for (x, z) in prof], M['walnut_d'], out=(0, s, 0))
    # Zahnschnitt unter der Platte: Band mit dunklen Fugen
    g.face([(0.4705, -0.5, z0 + 0.06), (0.4705, 0.5, z0 + 0.06), (0.4705, 0.5, z0 + 0.082), (0.4705, -0.5, z0 + 0.082)], M['walnut'],
           out=(1, 0, 0))
    g.face([(0.448, -0.5, z0 + 0.06), (0.4705, -0.5, z0 + 0.06), (0.4705, 0.5, z0 + 0.06), (0.448, 0.5, z0 + 0.06)], M['walnut_d'],
           out=(0, 0, -1))
    for k in range(10):
        y = -0.5 + 0.1 * k
        g.face([(0.471, y - 0.006, z0 + 0.06), (0.471, y + 0.006, z0 + 0.06), (0.471, y + 0.006, z0 + 0.082), (0.471, y - 0.006, z0 + 0.082)],
               M['groove'], out=(1, 0, 0))


def _shelf_label(g, M, z, y=0.0):
    """Messingrahmen mit Signaturkarte an der Front (Standort im Regal)."""
    g.box((0.448, y - 0.035, z - 0.018), (0.452, y + 0.035, z + 0.018), M['brass_d'], skip=('-x',))
    g.face([(0.4524, y - 0.029, z - 0.013), (0.4524, y + 0.029, z - 0.013), (0.4524, y + 0.029, z + 0.013), (0.4524, y - 0.029, z + 0.013)],
           M['paper_o'], out=(1, 0, 0))
    g.face([(0.4528, y - 0.016, z - 0.003), (0.4528, y + 0.012, z - 0.003), (0.4528, y + 0.012, z + 0.004), (0.4528, y - 0.016, z + 0.004)],
           M['ink'], out=(1, 0, 0))


FMT_ROWS = ['folio', 'folio', 'quarto', 'quarto', 'octavo', 'octavo', 'duodez', 'octavo']


def _case(g, M, BK, rng, z0, rows, plinth, tops_limit, mix='ordentlich', xf=(0.385, 0.415), shelf3=True, top_board=True):
    """Ein Regalgeschoss (3,0 m) vor der Wand x = 0, 1 m entlang y: Rueckwand, Pilaster, Fachboeden, Buecher.
    rows: Fachnummern mit Buechern (k: Boden bei z0 + k * 0,375)."""
    z1 = z0 + 8 * ROW
    _back(g, M, z0, z1)
    for s in (-1, 1):
        _pilaster(g, M, s, z0, z1)
    for k in range(1, 9 if top_board else 8):
        _board(g, M, z0 + k * ROW)
    if plinth:
        _plinth(g, M)
    for k in rows:
        zb = z0 + k * ROW
        lo, hi = xf
        if shelf3 and abs(zb - 3 * ROW) < 1e-6:  # Fach 3: Ablage vor den Buechern (Funde bei x = 0,36)
            lo, hi = 0.265, 0.29
        fmt = FMT_ROWS[min(k, 7)] if k > 0 else 'folio'
        _row(g, BK, rng, -YI + 0.002, YI - 0.002, zb, fmt, lo, hi, z0 + tops_limit, M['dust'], mix)


def build_bookcase_aisle():
    """Regal der Gaenge (Wandteil, 1 m breit, 3,16 m hoch): Sockel, sieben Reihen Buecher, Kranz mit Zahnschnitt."""
    _bookcase(5303, 'ordentlich')


def build_bookcase_aisle_b():
    """Wie bookcase_aisle, unordentlicher: mehr Luecken, lehnende und liegende Buecher, andere Werke."""
    _bookcase(5304, 'unordentlich')


def _bookcase(seed, mix):
    M = _m()
    BK = Books()
    rng = random.Random(seed)
    rng_global.seed(seed + 7)
    g = Geo()
    _case(g, M, BK, rng, 0.0, range(1, 8), True, 2.2, mix, top_board=False)
    _crown(g, M, 3.0)
    _shelf_label(g, M, 3.012, rng.uniform(-0.2, 0.2))
    g.build('regal')


# --- Regalwand der Halle mit Galerien ------------------------------------------------------------------------------------
def _gallery(g, M, zg, out=1.2, lamp=False):
    """Galerie vor dem Regal: Boden (Nussbaum) mit Stirnbrett und Goldleiste, Konsole, Gelaender aus Schmiedeeisen
    (Staebe, Zierbogen), Messinghandlauf."""
    x0 = 0.445
    # Boden und Untersicht
    g.face([(x0, -0.5, zg), (out - 0.03, -0.5, zg), (out - 0.03, 0.5, zg), (x0, 0.5, zg)], M['walnut'], out=(0, 0, 1))
    g.face([(x0, -0.5, zg - 0.14), (out - 0.03, -0.5, zg - 0.14), (out - 0.03, 0.5, zg - 0.14), (x0, 0.5, zg - 0.14)], M['walnut_d'],
           out=(0, 0, -1))
    # Stirnbrett: Profil (x, z) - Nase oben, Fries, Goldleiste, Unterkante
    prof = [(out - 0.03, zg - 0.14), (out - 0.03, zg - 0.32), (out - 0.005, zg - 0.32), (out + 0.004, zg - 0.31), (out + 0.004, zg - 0.235),
            (out + 0.012, zg - 0.228), (out + 0.016, zg - 0.215), (out + 0.012, zg - 0.202), (out + 0.004, zg - 0.196),
            (out + 0.004, zg - 0.03), (out + 0.012, zg - 0.024), (out + 0.016, zg - 0.012), (out + 0.012, zg - 0.002), (out, zg),
            (out - 0.03, zg)]
    mats = [M['walnut_d'], M['walnut_d'], M['walnut_d'], M['walnut'], M['gold_w'], M['gold'], M['gold'], M['gold_w'], M['walnut'],
            M['walnut_d'], M['walnut_w'], M['walnut_w'], M['walnut'], M['walnut']]
    sweep(g, prof, [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['walnut'], smooth=55, mats=mats)
    # Konsole unter der Galerie (mittig im Modul): S-foermige Unterkante (Karnies), Schnecke an den Seiten
    outline = [(x0, zg - 0.14), (x0 + 0.6, zg - 0.14), (x0 + 0.6, zg - 0.185)]
    for k in range(1, 10):
        t = k / 9
        outline.append((x0 + 0.6 * (1 - t) ** 1.1, zg - 0.185 - 0.345 * (3 * t * t - 2 * t * t * t)))
    for s in (-1, 1):
        g.face([(x, 0.035 * s, z) for (x, z) in outline], M['walnut_d'], out=(0, s, 0))
    for k in range(1, len(outline) - 1):
        (xa, za), (xb, zb) = outline[k], outline[k + 1]
        n = Vector((-(zb - za), 0, (xb - xa))).normalized()
        if n.x < 0:
            n = -n
        g.face([(xa, -0.035, za), (xb, -0.035, zb), (xb, 0.035, zb), (xa, 0.035, za)], M['walnut'], out=n, nrm=[n] * 4)
    for s in (-1, 1):  # geschnitzte Schnecke (helleres Holz, abgegriffen)
        cx, cz = x0 + 0.085, zg - 0.45
        sp = [(cx + 0.06 * (1 - t / 16) * math.cos(t * 0.7 + 1.5), 0.0352 * s, cz + 0.06 * (1 - t / 16) * math.sin(t * 0.7 + 1.5))
              for t in range(14)]
        for k in range(len(sp) - 1):
            pa, pb = Vector(sp[k]), Vector(sp[k + 1])
            d = (pb - pa).normalized()
            w = Vector((-d.z, 0, d.x)) * 0.007
            g.face([pa - w, pb - w, pb + w, pa + w], M['walnut_w'], out=(0, s, 0))
    # Gelaender
    xr = out - 0.06
    g.box((xr - 0.02, -0.5, zg), (xr + 0.02, 0.5, zg + 0.035), M['iron'], skip=('-z', '-y', '+y'))  # Fussleiste
    g.box((xr - 0.008, -0.5, zg + 0.88), (xr + 0.008, 0.5, zg + 0.9), M['iron'], skip=('-y', '+y'))  # obere Schiene
    g.box((xr - 0.008, -0.5, zg + 0.12), (xr + 0.008, 0.5, zg + 0.135), M['iron'], skip=('-y', '+y'))  # untere Schiene
    g.box((xr - 0.006, -0.5, zg + 0.74), (xr + 0.006, 0.5, zg + 0.752), M['iron'], skip=('-y', '+y'))  # Zwischenschiene
    sq = 0.0065
    for k in range(8):
        y = -0.5 + 0.125 * (k + 0.5)
        g.box((xr - sq, y - sq, zg + 0.035), (xr + sq, y + sq, zg + 0.88), M['iron'], skip=('-z', '+z'))
        if k % 2 == 0:  # Bund auf halber Hoehe
            g.box((xr - 0.011, y - 0.011, zg + 0.46), (xr + 0.011, y + 0.011, zg + 0.49), M['iron_w'])
    # C-Schnecken zwischen Zwischenschiene und oberer Schiene, Enden eingerollt
    zc, R, rc = zg + 0.777, 0.055, 0.02
    for k in range(4):  # alles im Uhrzeigersinn: linke Schnecke -> Bogen -> rechte Schnecke
        yc = -0.375 + 0.25 * k
        left = [(xr, yc - R + rc + rc * (0.4 + 0.6 * sv) * math.cos(math.pi + 1.25 * math.pi * (1 - sv)),
                 zc + rc * (0.4 + 0.6 * sv) * math.sin(math.pi + 1.25 * math.pi * (1 - sv))) for sv in (0.0, 0.4, 0.75)]
        arc = [(xr, yc + R * math.cos(a), zc + R * math.sin(a)) for a in [math.pi * (1 - i / 6) for i in range(7)]]
        right = [(xr, yc + R - rc + rc * (1 - 0.6 * sv) * math.cos(-1.25 * math.pi * sv),
                  zc + rc * (1 - 0.6 * sv) * math.sin(-1.25 * math.pi * sv)) for sv in (0.25, 0.6, 1.0)]
        tube(g, left + arc + right, 0.0042, M['iron'], sides=4)
    # Handlauf aus Messing (Pilzprofil)
    prof = [(-0.022, zg + 0.9), (-0.03, zg + 0.912), (-0.032, zg + 0.926), (-0.026, zg + 0.94), (-0.012, zg + 0.948), (0.0, zg + 0.95),
            (0.012, zg + 0.948), (0.026, zg + 0.94), (0.032, zg + 0.926), (0.03, zg + 0.912), (0.022, zg + 0.9)]
    prof = [(xr + u, z) for (u, z) in prof]
    sweep(g, [(u, z) for (u, z) in prof], [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['brass'], smooth=60, flip=True,
          mats=[M['brass_d'], M['brass_d'], M['brass'], M['brass'], M['brass'], M['brass'], M['brass'], M['brass'], M['brass_d'], M['brass_d']])
    if lamp:
        _rail_candles(g, M, xr, zg)


def _rail_candles(g, M, xr, zg):
    """Dreiarmiger Kerzenhalter aus Messing, an den Handlauf geklemmt; Kerzen unterschiedlich weit herabgebrannt."""
    zt = zg + 0.95
    g.cbox((xr - 0.04, -0.035, zt - 0.06), (xr + 0.04, 0.035, zt + 0.012), M['brass_d'], c=0.006)
    lathe(g, [(0.0, zt + 0.012), (0.016, zt + 0.012), (0.012, zt + 0.04), (0.018, zt + 0.055), (0.009, zt + 0.07), (0.009, zt + 0.1),
              (0.0, zt + 0.1)], M['brass'], seg=8, at=(xr, 0.0, 0.0))
    for k, (y, burn) in enumerate(((-0.13, 0.06), (0.0, 0.0), (0.13, 0.09))):
        za = zt + 0.1
        P = [(xr, 0.0, za - 0.01), (xr, y * 0.35, za - 0.005), (xr, y * 0.75, za + 0.01), (xr, y, za + 0.04)]
        if y != 0.0:
            tube(g, P, 0.0055, M['brass'], sides=5)
        zc = za + 0.04 if y != 0.0 else za
        _candle(g, M, (xr, y, zc), 0.011, 0.15 - burn, cup=0.032)


def _candle(g, M, base, r, h, cup=0.04, seg=7, old=False, drips=2, flame=True, rng=None):
    """Tropfschale, Tuelle, Kerze mit Wachsnasen, Docht und Flamme. base: Oberkante des Halters."""
    rng = rng or rng_global
    x, y, z = base
    lathe(g, [(0.0, z - 0.004), (cup, z - 0.002), (cup + 0.003, z + 0.004), (cup * 0.92, z + 0.006), (r * 1.45, z + 0.006),
              (r * 1.35, z + 0.03), (r * 1.15, z + 0.032)], M['brass'], seg=seg + 3, at=(x, y, 0.0))
    wax = M['wax_o'] if old else M['wax']
    zt = z + 0.032 + h
    lathe(g, [(r, z + 0.03), (r, zt - 0.006), (r * 0.82, zt), (r * 0.3, zt - 0.003), (0.0, zt - 0.002)], wax, seg=seg, at=(x, y, 0.0))
    for k in range(drips):  # Wachsnasen
        a = rng.uniform(0, 2 * math.pi)
        L = rng.uniform(0.02, 0.07)
        P = [(x + math.cos(a) * r * 0.98, y + math.sin(a) * r * 0.98, zt - 0.004 - t * L) for t in (0.0, 0.35, 0.7, 1.0)]
        tube(g, P, 0.0028, wax, sides=4, radii=[0.0035, 0.0032, 0.003, 0.0038])
    tube(g, [(x, y, zt - 0.003), (x + 0.001, y, zt + 0.006), (x + 0.002, y, zt + 0.011)], 0.0011, M['wick'], sides=3)
    if flame:
        fl = [(0.0, zt + 0.007), (0.0045, zt + 0.011), (0.0062, zt + 0.018), (0.005, zt + 0.028), (0.0022, zt + 0.038), (0.0, zt + 0.046)]
        lathe(g, fl, M['flame'], seg=6, at=(x + 0.001, y, 0.0))
        lathe(g, [(0.0, zt + 0.009), (0.0026, zt + 0.014), (0.0026, zt + 0.021), (0.0, zt + 0.027)], M['flame_c'], seg=5,
              at=(x + 0.001, y, 0.0))


def _cornice(g, M, z, depth=0.68):
    """Abschlussgesims der Regalwand (z bis z + 0,32): Fries, Zahnschnitt, Platte, Kehle, Abdeckung."""
    d = depth
    prof = [(0.0, z + 0.32), (d, z + 0.32), (d, z + 0.29), (d - 0.02, z + 0.282), (d - 0.05, z + 0.262), (d - 0.08, z + 0.235),
            (d - 0.1, z + 0.2), (d - 0.1, z + 0.15), (d - 0.12, z + 0.15), (d - 0.18, z + 0.12), (d - 0.2, z + 0.1), (0.47, z + 0.1),
            (0.47, z - 0.0), (0.0, z)]
    mats = [M['walnut_d'], M['walnut'], M['walnut_w'], M['walnut'], M['walnut'], M['walnut_d'], M['walnut'], M['walnut_d'], M['walnut'],
            M['walnut_w'], M['walnut_d'], M['walnut'], M['walnut_d']]
    sweep(g, prof, [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['walnut'], smooth=50, flip=True, mats=mats)
    g.face([(0.47, -0.5, z + 0.04), (0.4706, -0.5, z + 0.04), (0.4706, 0.5, z + 0.04), (0.47, 0.5, z + 0.04)], M['gold_w'], out=(1, 0, 0))
    g.face([(0.4705, -0.5, z + 0.035), (0.4705, 0.5, z + 0.035), (0.4705, 0.5, z + 0.045), (0.4705, -0.5, z + 0.045)], M['gold_w'],
           out=(1, 0, 0))
    for k in range(10):
        y = -0.5 + 0.05 + k * 0.1
        g.box((d - 0.2, y - 0.025, z + 0.1), (d - 0.165, y + 0.025, z + 0.14), M['walnut'], skip=('-x', '+z'))


def _library(seed, tiers, galleries, cornice_z, lamp=False):
    M = _m()
    BK = Books()
    rng = random.Random(seed)
    rng_global.seed(seed + 3)
    g = Geo()
    for (z0, plinth) in tiers:
        rows = range(1, 8) if plinth else range(0, 8)
        _case(g, M, BK, rng, z0, rows, plinth, 2.2, xf=(0.39, 0.415), shelf3=plinth)
    for zg in galleries:
        _gallery(g, M, zg, lamp=lamp)
    _cornice(g, M, cornice_z)
    g.build('regalwand')


def build_library_wall():
    """Regalwand der grossen Halle (Wandteil, Wandflaeche x = 0, 1 m entlang y, Boden z = 0): drei Geschosse Regale
    (je 3,0 m), Galerien bei 3,0 und 6,0 m, Gesims bis 9,32 m (Wiki-Bild: drei Stockwerke Regale)."""
    _library(5301, [(0.0, True), (3.0, False), (6.0, False)], (3.0, 6.0), 9.0)


def build_library_wall_lamp():
    """Wie library_wall, dazu Kerzenhalter an den Handlaeufen beider Galerien (jedes vierte Wandstueck)."""
    _library(5305, [(0.0, True), (3.0, False), (6.0, False)], (3.0, 6.0), 9.0, lamp=True)


def build_library_wall_upper():
    """Wie library_wall, aber erst ab der ersten Galerie (Ursprung auf der Galerie, z = 0 = 3,0 m ueber dem Boden):
    ueber Durchgaengen in die Gaenge."""
    _library(5302, [(0.0, False), (3.0, False)], (0.0, 3.0), 6.0)


# --- Varianten der Regale (zufaellig in der Karte) ------------------------------------------------------------------------
def build_bookcase_aisle__v1():
    """Regal der Gaenge, Variante 1 (andere Werke und Luecken)."""
    _bookcase(5311, 'ordentlich')


def build_bookcase_aisle__v2():
    """Regal der Gaenge, Variante 2."""
    _bookcase(5312, 'ordentlich')


def build_bookcase_aisle_b__v1():
    """Unordentliches Regal, Variante 1."""
    _bookcase(5313, 'unordentlich')


def build_bookcase_aisle_b__v2():
    """Unordentliches Regal, Variante 2."""
    _bookcase(5314, 'unordentlich')


def build_library_wall__v1():
    """Regalwand der Halle, Variante 1 (andere Buecher)."""
    _library(5321, [(0.0, True), (3.0, False), (6.0, False)], (3.0, 6.0), 9.0)


def build_library_wall__v2():
    """Regalwand der Halle, Variante 2."""
    _library(5322, [(0.0, True), (3.0, False), (6.0, False)], (3.0, 6.0), 9.0)


# --- Stirnbrett -----------------------------------------------------------------------------------------------------------
def _raised_panel(g, M, y0, y1, z0, z1, x=0.03):
    """Kassette mit erhabener Fuellung (Fase) und Profilleiste auf der Flaeche x."""
    b = 0.055
    fx = x + 0.018
    g.face([(fx, y0 + b, z0 + b), (fx, y1 - b, z0 + b), (fx, y1 - b, z1 - b), (fx, y0 + b, z1 - b)], M['walnut'], out=(1, 0, 0))
    pe = [(y0, z0), (y1, z0), (y1, z1), (y0, z1)]
    pi = [(y0 + b, z0 + b), (y1 - b, z0 + b), (y1 - b, z1 - b), (y0 + b, z1 - b)]
    for k in range(4):
        a, c = pe[k], pe[(k + 1) % 4]
        d, e = pi[k], pi[(k + 1) % 4]
        n = Vector((0.018, 0, 0)).cross(Vector((0, c[0] - a[0], c[1] - a[1])))
        o = Vector((1.0, 0, 0)) * 0.3 + Vector((0, (a[0] + c[0]) * 0.5 - (y0 + y1) * 0.5, (a[1] + c[1]) * 0.5 - (z0 + z1) * 0.5)).normalized()
        g.face([(x + 0.004, a[0], a[1]), (x + 0.004, c[0], c[1]), (fx, e[0], e[1]), (fx, d[0], d[1])], M['walnut_d'] if k in (2, 3) else M['walnut'],
               out=o)
    # Profilleiste rund um die Kassette (u nach aussen, v entlang +x)
    prof = [(0.0, 0.004), (0.003, 0.012), (0.008, 0.015), (0.013, 0.011), (0.016, 0.0)]
    path = [(x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1)]
    sweep(g, prof, path, (1, 0, 0), M['walnut'], closed=True, smooth=60, flip=True,
          mats=[M['walnut_w'], M['walnut_w'], M['walnut'], M['walnut_d']])


def build_bookcase_end():
    """Stirnseite eines frei stehenden Regals (Wandteil, 1,04 m breit, 3,16 m hoch): Rahmen mit zwei erhabenen Kassetten,
    Sockel und Kranz wie die Regale, Messingschild mit Signatur, abgegriffene Kanten in Griffhoehe."""
    M = _m()
    g = Geo()
    W = 0.52
    g.face([(0.03, -W, 0.0), (0.03, W, 0.0), (0.03, W, 3.0), (0.03, -W, 3.0)], M['walnut_d'], out=(1, 0, 0))
    for s in (-1, 1):
        g.face([(0.0, W * s, 0.0), (0.03, W * s, 0.0), (0.03, W * s, 3.0), (0.0, W * s, 3.0)], M['walnut_d'], out=(0, s, 0))
    _raised_panel(g, M, -0.4, 0.4, 0.42, 1.48)
    _raised_panel(g, M, -0.4, 0.4, 1.66, 2.86)
    # Sockel und Kranz (Profile wie beim Regal, auf 0,07 m Tiefe)
    prof = [(0.03, 0.36), (0.045, 0.36), (0.05, 0.35), (0.05, 0.05), (0.058, 0.04), (0.064, 0.028), (0.064, 0.0), (0.0, 0.0)]
    sweep(g, prof, [(0, -W, 0), (0, W, 0)], (0, 0, 1), M['walnut_d'], smooth=50, flip=True)
    prof = [(0.0, 3.16), (0.07, 3.16), (0.07, 3.15), (0.06, 3.14), (0.052, 3.125), (0.05, 3.11), (0.05, 3.06), (0.042, 3.05),
            (0.036, 3.03), (0.036, 2.98), (0.03, 2.98)]
    sweep(g, prof, [(0, -W, 0), (0, W, 0)], (0, 0, 1), M['walnut'], smooth=50, flip=True)
    for s in (-1, 1):
        g.face([(x, W * s, z) for (x, z) in [(0.0, 3.16), (0.07, 3.16), (0.07, 3.15), (0.06, 3.14), (0.052, 3.125), (0.05, 3.11),
                                              (0.05, 3.06), (0.042, 3.05), (0.036, 3.03), (0.036, 2.98), (0.0, 2.98)]], M['walnut_d'], out=(0, s, 0))
    # abgegriffene Kanten in Griffhoehe
    for s in (-1, 1):
        g.face([(0.0301, W * s - 0.05 * s, 0.9), (0.0301, W * s - 0.002 * s, 0.9), (0.0301, W * s - 0.002 * s, 1.5),
                (0.0301, W * s - 0.05 * s, 1.5)], M['walnut_w'], out=(1, 0, 0))
    # Messingschild mit Signatur (Zeichenatlas)
    g.cbox((0.03, -0.11, 1.53), (0.034, 0.11, 1.61), M['brass'], c=0.0015, skip=('-x',))
    stencil = mat('praegung_schwarz', (0.05, 0.04, 0.03), 'stencil')
    _atlas_text(g, stencil, 'C-VII', (0.0345, -0.075, 1.548), 0.045)
    g.build('stirnbrett')


def _glyph(ch):
    if 'A' <= ch <= 'Z':
        return ord(ch) - 65
    if '0' <= ch <= '9':
        return 26 + ord(ch) - 48
    return 36 if ch == '-' else -1


def _atlas_text(g, m, s, origin, h, w=None, gap=0.1):
    """Schrift aus dem Zeichenatlas des Spiels (8 x 8 Zellen) auf einer Flaeche, die nach +x blickt; origin links unten."""
    w = w or h * 0.7
    x, y, z = origin
    for i, ch in enumerate(s):
        c = _glyph(ch)
        if c < 0:
            continue
        u0, v0 = (c % 8) / 8.0, (c // 8) / 8.0
        yy = y + i * w * (1 + gap)
        g.face([(x, yy, z), (x, yy + w, z), (x, yy + w, z + h), (x, yy, z + h)], m, out=(1, 0, 0),
               uv=[(u0, 1 - v0 - 0.125), (u0 + 0.125, 1 - v0 - 0.125), (u0 + 0.125, 1 - v0), (u0, 1 - v0)])


# --- Heruntergefallene Buecher ----------------------------------------------------------------------------------------------
def _open_book(g, BK, M, at, rot, L=0.26, W=0.19, col=0, face_up=True):
    """Aufgeschlagenes Buch: zwei Deckel, gewoelbte Seitenbloecke mit Satzspiegel (face_up) oder als Dach umgekehrt."""
    old = g.M
    g.M = old @ Matrix.Translation(at) @ Matrix.Rotation(rot, 4, 'Z')
    cov = BK.spine[col]
    T = 0.016
    if face_up:
        for s in (-1, 1):
            y0, y1 = (0.004, W) if s > 0 else (-W, -0.004)
            g.box((-L / 2, y0, 0.0), (L / 2, y1, 0.004), cov, skip=('-z',))
            # Seitenblock: Oberseite steigt vom Ruecken an und faellt zum Schnitt hin leicht ab
            prof = [(0.006, 0.006), (0.03, T * 0.85), (0.07, T), (W * 0.7, T * 0.98), (W - 0.006, T * 0.9)]
            pts = [(abs(yy) * s, zz) for (yy, zz) in prof]
            for k in range(len(pts) - 1):
                (ya, za), (yb, zb) = pts[k], pts[k + 1]
                g.face([(-L / 2 + 0.006, ya, za), (L / 2 - 0.006, ya, za), (L / 2 - 0.006, yb, zb), (-L / 2 + 0.006, yb, zb)], M['paper'],
                       out=(0, 0, 1))
            for xx, o in ((-L / 2 + 0.006, -1), (L / 2 - 0.006, 1)):
                poly = [(xx, pts[0][0], 0.004)] + [(xx, yy, zz) for (yy, zz) in pts] + [(xx, pts[-1][0], 0.004)]
                g.face(poly, M['paper_o'], out=(o, 0, 0))
            g.face([(-L / 2 + 0.006, pts[-1][0], 0.004), (L / 2 - 0.006, pts[-1][0], 0.004), (L / 2 - 0.006, pts[-1][0], pts[-1][1]),
                    (-L / 2 + 0.006, pts[-1][0], pts[-1][1])], M['paper_o'], out=(0, s, 0))
            # Satzspiegel (zwei Spalten grauer Text)
            yt0, yt1 = sorted((0.035 * s, (W - 0.025) * s))
            g.face([(-L / 2 + 0.03, yt0, T + 0.0006), (L / 2 - 0.035, yt0, T + 0.0006), (L / 2 - 0.035, yt1, T + 0.0006),
                    (-L / 2 + 0.03, yt1, T + 0.0006)], mat('satzspiegel', (0.52, 0.48, 0.42), 'paint', r=1.2), out=(0, 0, 1))
    else:  # mit dem Gesicht nach unten, Ruecken oben: Dach
        a = math.radians(24)
        for s in (-1, 1):
            p0 = Vector((0, 0, W * math.sin(a)))
            p1 = Vector((0, s * W * math.cos(a), 0.0))
            g.face([(-L / 2, p0.y, p0.z), (L / 2, p0.y, p0.z), (L / 2, p1.y, p1.z), (-L / 2, p1.y, p1.z)], cov, out=(0, s * math.sin(a), math.cos(a)))
            q0 = p0 + Vector((0, 0, -0.004))
            q1 = p1 + Vector((0, -s * 0.004, 0.0))
            g.face([(-L / 2, q0.y, q0.z), (L / 2, q0.y, q0.z), (L / 2, q1.y, q1.z), (-L / 2, q1.y, q1.z)], M['paper'],
                   out=(0, -s * math.sin(a), -math.cos(a)))
            for xx, o in ((-L / 2, -1), (L / 2, 1)):  # gefaecherte Seiten am Kopf und Fuss
                g.face([(xx + o * 0.004, 0.0, W * math.sin(a) - 0.006), (xx + o * 0.004, s * (W * math.cos(a) - 0.01), 0.002),
                        (xx + o * 0.004, s * 0.01, 0.002)], M['paper_o'], out=(o, 0, 0))
        g.face([(-L / 2, -0.006, W * math.sin(a) + 0.004), (L / 2, -0.006, W * math.sin(a) + 0.004), (L / 2, 0.006, W * math.sin(a) + 0.004),
                (-L / 2, 0.006, W * math.sin(a) + 0.004)], cov, out=(0, 0, 1))
    g.M = old


def _books_fallen(seed):
    M = _m()
    BK = Books()
    rng = random.Random(seed)
    g = Geo()
    placed = []
    for k in range(rng.randint(5, 7)):
        L = rng.uniform(0.2, 0.32)
        W = L * rng.uniform(0.68, 0.78)
        T = rng.uniform(0.025, 0.06)
        for tries in range(30):
            x, y = rng.uniform(-0.3, 0.3), rng.uniform(-0.36, 0.36)
            if all(math.hypot(x - a, y - b) > 0.2 for (a, b) in placed):
                break
        placed.append((x, y))
        col = _pick(rng, WEIGHTS)
        rot = rng.uniform(0, 2 * math.pi)
        tilt = rng.random() < 0.25
        Mt = Matrix.Translation((x, y, 0.0)) @ Matrix.Rotation(rot, 4, 'Z')
        if tilt:  # liegt schraeg auf einem anderen Buch
            Mt = Mt @ Matrix.Translation((0, 0, 0.0)) @ Matrix.Rotation(math.radians(rng.uniform(6, 12)), 4, 'Y')
        deco = [(L * 0.08, L * 0.095, BK.gold), (L * 0.9, L * 0.915, BK.gold)] if rng.random() < 0.6 else []
        _emit_box_book(g, Mt @ Matrix.Translation((W / 2, -L / 2, 0.0)), L, W, T, BK.spine[col], BK.edges[rng.randrange(3)], 0.6, deco, full=True,
                       side=BK.marble[rng.randrange(3)] if rng.random() < 0.3 else None)
        # Staubabdruck/Kante: nichts weiter
    _open_book(g, BK, M, (0.25 if seed % 2 else -0.2, -0.32, 0.0), rng.uniform(0, 6.28), col=_pick(rng, WEIGHTS), face_up=True)
    _open_book(g, BK, M, (-0.22 if seed % 2 else 0.24, 0.3, 0.0), rng.uniform(0, 6.28), col=_pick(rng, WEIGHTS), face_up=False)
    # lose Blaetter, leicht gewellt
    for k in range(rng.randint(2, 3)):
        cx, cy, a = rng.uniform(-0.35, 0.35), rng.uniform(-0.45, 0.45), rng.uniform(0, 6.28)
        old = g.M
        g.M = old @ Matrix.Translation((cx, cy, 0.0015)) @ Matrix.Rotation(a, 4, 'Z')
        w, l = 0.16, 0.23
        zs = [0.0, 0.006, 0.002, 0.009]
        for i in range(3):
            g.face([(-l / 2 + l * i / 3, -w / 2, zs[i]), (-l / 2 + l * (i + 1) / 3, -w / 2, zs[i + 1]),
                    (-l / 2 + l * (i + 1) / 3, w / 2, zs[i + 1]), (-l / 2 + l * i / 3, w / 2, zs[i])], M['paper_o'], out=(0, 0, 1))
        g.face([(-l / 2 + 0.03, -w / 2 + 0.02, 0.0012), (-l / 2 + 0.07, -w / 2 + 0.02, 0.0062), (-l / 2 + 0.07, w / 2 - 0.02, 0.0062),
                (-l / 2 + 0.03, w / 2 - 0.02, 0.0012)], mat('satzspiegel', (0.52, 0.48, 0.42), 'paint', r=1.2), out=(0, 0, 1))
        g.M = old
    g.build('buecher')


def build_books_fallen():
    """Heruntergefallene Buecher (Boden, Mitte im Ursprung; Wiki: der Library Mind laesst Buecher fallen): Baende mit
    Kapital und Schnitt, eines aufgeschlagen mit Satzspiegel, eines mit dem Gesicht nach unten (Dach), lose Blaetter."""
    _books_fallen(5305)


def build_books_fallen__v1():
    """Heruntergefallene Buecher, Variante 1."""
    _books_fallen(5306)


def build_books_fallen__v2():
    """Heruntergefallene Buecher, Variante 2."""
    _books_fallen(5307)


# --- Kandelaber -------------------------------------------------------------------------------------------------------------
def _paw_leg(g, M, a, r0=0.06, r1=0.27, z0=0.24):
    """Geschwungenes Bein mit Volute und Ballenfuss."""
    c, s = math.cos(a), math.sin(a)
    P = []
    for i in range(9):
        t = i / 8
        rr = r0 + (r1 - r0) * t
        zz = z0 * (1 - t) ** 1.6 + 0.045 * t + 0.04 * math.sin(math.pi * t)
        P.append((c * rr, s * rr, zz))
    radii = [0.022 - 0.008 * i / 8 for i in range(9)]
    tube(g, P, 0.02, M['bronze'], sides=6, radii=radii)
    lathe(g, [(0.0, 0.0), (0.028, 0.004), (0.032, 0.022), (0.022, 0.042), (0.0, 0.05)], M['bronze_d'], seg=8, at=(c * r1, s * r1, 0.0))
    # Volute oben am Bein
    V = [(c * (r0 + 0.04 + 0.03 * math.cos(t)), s * (r0 + 0.04 + 0.03 * math.cos(t)), z0 - 0.02 + 0.03 * math.sin(t)) for t in
         [math.pi * 0.2 + i * 0.6 for i in range(7)]]
    tube(g, V, 0.008, M['bronze'], sides=5)


def _candelabrum(arms, seed, h_top=1.55):
    M = _m()
    rng = random.Random(seed)
    rng_global.seed(seed)
    g = Geo()
    for k in range(3):
        _paw_leg(g, M, k * 2 * math.pi / 3 + 0.3)
    # Nabe und Baluster (gedrehter Schaft); Kehlen dunkler (Patina)
    B, D = M['bronze'], M['bronze_d']
    prof = [(0.0, 0.17), (0.075, 0.18), (0.08, 0.2), (0.065, 0.24), (0.04, 0.27), (0.05, 0.3), (0.03, 0.34), (0.022, 0.4),
            (0.019, 0.62), (0.034, 0.64), (0.042, 0.665), (0.03, 0.69), (0.018, 0.72), (0.016, 0.95), (0.032, 0.98), (0.05, 1.04),
            (0.036, 1.1), (0.02, 1.13), (0.016, 1.38), (0.03, 1.4), (0.026, 1.43), (0.04, 1.47), (0.05, 1.5), (0.03, 1.52),
            (0.016, h_top)]
    mats = [B, B, D, D, B, D, D, B, B, B, D, D, B, B, B, D, D, B, B, D, B, B, D, D]
    lathe(g, prof, B, seg=10, mats=mats)
    tips = [(0.0, 0.0, h_top + 0.02)]
    lathe(g, [(0.0, h_top - 0.005), (0.045, h_top), (0.05, h_top + 0.012), (0.012, h_top + 0.02)], B, seg=10)
    for k in range(arms):
        a = k * 2 * math.pi / arms + math.pi / 4
        c, s = math.cos(a), math.sin(a)
        R = 0.24 if arms > 3 else 0.2
        P = []
        for i in range(8):
            t = i / 7
            rr = 0.03 + (R - 0.03) * t
            zz = 1.47 - 0.07 * math.sin(math.pi * t * 1.1) + 0.12 * t * t
            P.append((c * rr, s * rr, zz))
        tube(g, P, 0.011, B, sides=5, radii=[0.013 - 0.004 * i / 7 for i in range(8)])
        # Zierranke unter dem Arm
        Q = [(c * (0.05 + 0.1 * t), s * (0.05 + 0.1 * t), 1.43 - 0.06 * math.sin(math.pi * t)) for t in (0.0, 0.33, 0.66, 1.0)]
        tube(g, Q, 0.005, D, sides=4)
        tips.append((c * R, s * R, P[-1][2] + 0.005))
    burns = [rng.uniform(0.0, 0.12) for _ in tips]
    for k, (x, y, z) in enumerate(tips):
        _candle(g, M, (x, y, z), 0.0135, 0.22 - burns[k], cup=0.045, seg=7, old=rng.random() < 0.4, drips=rng.randint(1, 3), rng=rng)
    g.build('kandelaber')


def build_candelabrum():
    """Stehender Kandelaber aus Bronze (1,9 m; Wiki-Bild: Kandelaber um Saeulen und Regale): drei geschwungene Beine mit
    Volute und Ballenfuss, gedrehter Baluster mit Kehlen (Patina), vier Arme und Mitte, Tropfschalen, Kerzen
    unterschiedlich weit herabgebrannt mit Wachsnasen, Docht und Flamme."""
    _candelabrum(4, 5308)


def build_candelabrum__v1():
    """Kandelaber, Variante 1: drei Kerzen (zwei Arme und Mitte), hoeherer Schaft."""
    _candelabrum(2, 5309, h_top=1.62)


# --- Eiserner Kronleuchter ----------------------------------------------------------------------------------------------------
def build_chandelier_iron():
    """Eiserner Kronleuchter (Deckenebene z = 0, haengt 2,06 m herab), Metall wie die Gitter von Level 3 (Wiki): weiss
    gestrichenes Eisen mit Rost, Baldachin, Kette mit Gliedern, gedrehter Schaft, zwei Reifen mit Kerzen, S-Arme."""
    M = _m()
    rng = random.Random(5310)
    rng_global.seed(5310)
    g = Geo()
    W = mat('eisen_weiss', (0.82, 0.80, 0.74), 'paint', r=0.85)
    Ru = mat('rost_eisen', (0.62, 0.45, 0.32), 'metal', 'metal_rusty_iron', r=1.25, m=0.25)
    Rd = mat('rost_dunkel', (0.28, 0.16, 0.09), 'paint', r=1.3)
    # Baldachin
    lathe(g, [(0.0, -0.06), (0.04, -0.058), (0.1, -0.03), (0.12, -0.01), (0.12, 0.0)], W, seg=14, mats=[W, W, Rd, W])
    # Kette (abwechselnd gedrehte Glieder)
    z = -0.07
    for k in range(10):
        a = (k % 2) * math.pi / 2
        c, s = math.cos(a), math.sin(a)
        P = [(c * 0.016 * math.cos(t), s * 0.016 * math.cos(t), z - 0.03 + 0.03 * math.sin(t)) for t in [2 * math.pi * i / 8 for i in range(8)]]
        tube(g, P, 0.0045, Ru if rng.random() < 0.3 else W, sides=4, closed=True)
        z -= 0.05
    # Schaft (gedreht) mit Kugel und Zapfen
    zt = z + 0.01
    prof = [(0.0, -1.98), (0.02, -1.96), (0.05, -1.92), (0.08, -1.86), (0.085, -1.8), (0.06, -1.76), (0.03, -1.72), (0.022, -1.62),
            (0.04, -1.58), (0.03, -1.52), (0.02, -1.45), (0.018, -1.2), (0.035, -1.16), (0.025, -1.1), (0.016, -1.0), (0.014, zt - 0.03),
            (0.02, zt - 0.01), (0.0, zt)]
    mats = [W if rng.random() < 0.75 else Ru for _ in range(len(prof) - 1)]
    lathe(g, prof, W, seg=10, mats=mats)
    lathe(g, [(0.0, -2.06), (0.012, -2.04), (0.02, -2.0), (0.0, -1.97)], Rd, seg=8)
    for (zr, R, n) in ((-1.85, 1.0, 12), (-1.35, 0.6, 8)):
        P = [(R * math.cos(2 * math.pi * i / 36), R * math.sin(2 * math.pi * i / 36), zr) for i in range(36)]
        hm, run_ = [], 0  # Rost in unregelmaessigen Flecken (abgeplatzter Lack), mal kurz, mal laenger
        for i in range(36):
            if run_ <= 0 and rng.random() < 0.12:
                run_ = rng.choice((1, 1, 2, 3))
            hm.append(Ru if run_ > 0 else (Rd if rng.random() < 0.06 else W))
            run_ -= 1
        tube(g, P, 0.022, W, sides=6, closed=True, mats=hm)
        for k in range(4):  # S-Arme vom Schaft zum Reifen
            a = k * math.pi / 2 + (0.0 if R > 0.8 else math.pi / 4)
            c, s = math.cos(a), math.sin(a)
            Q = []
            for i in range(9):
                t = i / 8
                rr = 0.04 + (R - 0.04) * t
                Q.append((c * rr, s * rr, zr + 0.45 * (1 - t) ** 2 - 0.08 * math.sin(math.pi * t)))
            tube(g, Q, 0.012, W, sides=5, mats=[W if rng.random() < 0.7 else Ru for _ in range(9)])
        for k in range(n):
            a = 2 * math.pi * (k + 0.5) / n
            x, y = math.cos(a) * R, math.sin(a) * R
            lathe(g, [(0.0, zr + 0.015), (0.012, zr + 0.022), (0.012, zr + 0.04)], Rd, seg=6, at=(x, y, 0.0))
            _candle(g, M, (x, y, zr + 0.035), 0.0125, rng.uniform(0.1, 0.18), cup=0.04, seg=6, old=rng.random() < 0.5, drips=rng.randint(1, 2),
                    rng=rng)
    g.build('kronleuchter')


# --- Tuffsaeule -----------------------------------------------------------------------------------------------------------
def build_column_tuff():
    """Saeule aus Tuff (Wiki: unbekannter Tuffstein), 10,2 m, toskanisch: Plinthe, Wulst, Schaft aus Trommeln mit
    Schwellung und Fugen, Hals mit Astragal, Echinus, Abakus. Poroeser Stein, Schmutzsaum unten, Ausbrueche."""
    M = _m()
    rng = random.Random(5320)
    g = Geo()
    T = mat('tuff', (0.95, 0.93, 0.86), 'concrete', 'tuff_stone', r=1.3)
    Td = mat('tuff_dunkel', (0.66, 0.61, 0.52), 'concrete', 'tuff_stone', r=1.45)
    Tj = mat('tuff_fuge', (0.42, 0.38, 0.32), 'concrete', 'tuff_stone', r=1.5)
    H = 10.2
    g.cbox((-0.46, -0.46, 0.0), (0.46, 0.46, 0.3), Td, c=0.02, skip=('-z',))
    # Ausbrueche an der Plinthe (helle Bruchflaechen)
    for (x, y) in ((0.46, 0.3), (-0.2, 0.46), (-0.46, -0.38)):
        nx, ny = (1 if abs(x) > 0.45 else 0) * (1 if x > 0 else -1), (1 if abs(y) > 0.45 else 0) * (1 if y > 0 else -1)
        o = Vector((nx, ny, 0))
        t = Vector((-ny, nx, 0)) * 0.05
        p = Vector((x, y, 0.3)) + o * 0.0008
        g.face([p - t, p + t, p + t * 0.5 + Vector((0, 0, -0.06)), p - t * 0.7 + Vector((0, 0, -0.04))], T, out=o)
    zb = 0.3
    base = [(0.4, zb), (0.415, zb + 0.02), (0.42, zb + 0.06), (0.41, zb + 0.1), (0.385, zb + 0.13), (0.36, zb + 0.14),
            (0.35, zb + 0.16), (0.345, zb + 0.18)]
    lathe(g, base, T, seg=28, mats=[Td, Td, Td, T, T, Tj, T])
    # Schaft aus Trommeln (Fugen als Kerben), Schwellung (Entasis), leichte Unregelmaessigkeit je Trommel
    z0, z1 = zb + 0.18, H - 0.9
    prof, kinds = [], []
    nd = 8
    for d in range(nd):
        za, zc = z0 + (z1 - z0) * d / nd, z0 + (z1 - z0) * (d + 1) / nd
        jit = rng.uniform(-0.004, 0.004)
        for (t, inset) in (((0.0, 0.006),) if d == 0 else ()) + ((0.014, 0.0), (0.25, 0.0), (0.5, 0.0), (0.75, 0.0), (0.986, 0.0), (1.0, 0.006)):
            zz = za + (zc - za) * t
            u = (zz - z0) / (z1 - z0)
            prof.append((0.345 - 0.06 * u ** 1.5 + 0.012 * math.sin(math.pi * u) + jit - inset, zz))
            kinds.append(inset > 0)
    mats = []
    for k in range(len(prof) - 1):
        zm = (prof[k][1] + prof[k + 1][1]) * 0.5
        mats.append(Tj if (kinds[k] or kinds[k + 1]) else (Td if zm < 1.0 else T))
    lathe(g, prof, T, seg=28, mats=mats)
    zc_ = z1
    cap = [(0.285, zc_), (0.3, zc_ + 0.02), (0.31, zc_ + 0.04), (0.3, zc_ + 0.06), (0.288, zc_ + 0.07), (0.285, zc_ + 0.3),
           (0.3, zc_ + 0.32), (0.3, zc_ + 0.34), (0.33, zc_ + 0.38), (0.39, zc_ + 0.44), (0.43, zc_ + 0.5), (0.44, zc_ + 0.53), (0.0, zc_ + 0.53)]
    lathe(g, cap, T, seg=28, mats=[T, T, T, Tj, T, Tj, T, T, T, T, T, T])
    g.cbox((-0.48, -0.48, zc_ + 0.53), (0.48, 0.48, H), T, c=0.015, skip=('+z',))
    # Poren und Ausbrueche am Schaft (dunkle Flecken)
    for k in range(14):
        a = rng.uniform(0, 2 * math.pi)
        zz = rng.uniform(0.7, 8.8)
        u = (zz - z0) / (z1 - z0)
        r = 0.345 - 0.06 * u ** 1.5 + 0.012 * math.sin(math.pi * u) + 0.0012
        c, s = math.cos(a), math.sin(a)
        n = Vector((c, s, 0))
        t = Vector((-s, c, 0))
        w, h = rng.uniform(0.015, 0.05), rng.uniform(0.012, 0.04)
        pts = []
        for i in range(6):
            b = 2 * math.pi * i / 6
            q = n * r + t * (math.cos(b) * w * rng.uniform(0.7, 1.1)) + Vector((0, 0, zz + math.sin(b) * h))
            pts.append(q)
        g.face(pts, Tj, out=n)
    g.build('saeule')


# --- Sehr alte Holztuer -----------------------------------------------------------------------------------------------------
def build_door_old_open():
    """Sehr alte Holztuer im Durchbruch (Laibung x = -0,5 .. 0,5), offen: Bohlen mit Fugen, Rissen und morscher
    Unterkante, Querriegel und Strebe auf der Rueckseite, geschmiedete Baender mit Lilienenden und Nagelkoepfen, Ringzug
    mit Rosette, Schluesselschild; Zarge aus Eiche mit Fase (Wiki: am Ende des Eingangsgangs und als Ausgang)."""
    M = _m()
    rng = random.Random(9)
    g = Geo()
    oak = mat('eiche_alt', (0.34, 0.26, 0.18), 'wood', 'wood_walnut_floor', r=2.6)
    oak_d = mat('eiche_dunkel', (0.20, 0.15, 0.10), 'wood', 'wood_walnut_floor', r=2.6)
    rot = mat('holz_morsch', (0.16, 0.12, 0.09), 'wood', 'wood_walnut_floor', r=3.0)
    iron = mat('eisen_geschmiedet', (0.10, 0.09, 0.085), 'metal', r=1.1, m=0.35)
    rust = mat('rost_band', (0.36, 0.20, 0.11), 'paint', r=1.35)
    W, H = 1.0, 2.15
    for s in (-1, 1):  # Zarge
        g.cbox((0.38, s * W / 2 - (0.09 if s > 0 else 0.0), 0.0), (0.5, s * W / 2 + (0.0 if s > 0 else 0.09), H + 0.02), oak_d, c=0.012)
    g.cbox((0.38, -W / 2, H - 0.06), (0.5, W / 2, H + 0.1), oak_d, c=0.012)
    g.face([(0.5005, -0.3, H + 0.04), (0.5005, 0.3, H + 0.04), (0.5005, 0.3, H + 0.045), (0.5005, -0.3, H + 0.045)], oak, out=(1, 0, 0))
    leaf = Matrix.Translation((0.44, -W / 2 + 0.09, 0.012)) @ Matrix.Rotation(math.radians(78), 4, 'Z')
    old = g.M
    g.M = leaf
    LW, LH, T = W - 0.18, H - 0.07, 0.05
    nb = 5
    for k in range(nb):
        y0 = k * LW / nb
        y1 = (k + 1) * LW / nb - 0.004
        top = LH - rng.uniform(0.0, 0.03)
        g.cbox((0.0, y0 + 0.002, rng.uniform(0.0, 0.015)), (T, y1, top), oak, c=0.005, mats={'-z': rot})
        # morsche Unterkante und Risse (beide Seiten)
        for (x, o) in ((T + 0.0006, 1), (-0.0006, -1)):
            g.face([(x, y0 + 0.004, 0.02), (x, y1 - 0.003, 0.02), (x, y1 - 0.003, 0.02 + rng.uniform(0.05, 0.14)),
                    (x, y0 + 0.004, 0.02 + rng.uniform(0.04, 0.1))], rot, out=(o, 0, 0))
            for c in range(rng.randint(1, 2)):
                yc = rng.uniform(y0 + 0.02, y1 - 0.02)
                za = rng.uniform(0.3, 1.6)
                zl = rng.uniform(0.2, 0.5)
                g.face([(x, yc - 0.0015, za), (x, yc + 0.0015, za), (x, yc + 0.003, za + zl), (x, yc + 0.0005, za + zl)], oak_d, out=(o, 0, 0))
    # Rueckseite: Querriegel und Strebe
    for zz in (0.35, LH - 0.4):
        g.cbox((-0.03, 0.03, zz - 0.07), (0.0, LW - 0.03, zz + 0.07), oak_d, c=0.006, skip=('+x',))
    D = Vector((0, LW - 0.12, LH - 0.4 - 0.35 - 0.1))
    ln = D.length
    old2 = g.M
    g.M = g.M @ Matrix.Translation((0, 0.08, 0.42)) @ Matrix.Rotation(math.atan2(D.z, D.y), 4, 'X')
    g.cbox((-0.028, 0.0, -0.055), (0.0, ln, 0.055), oak_d, c=0.006, skip=('+x',))
    g.M = old2
    # Baender mit Lilienende und Nagelkoepfen, Rost am Rand
    for zz in (0.32, LH - 0.35):
        g.cbox((T, -0.02, zz - 0.03), (T + 0.007, LW - 0.16, zz + 0.03), iron, c=0.002)
        e = LW - 0.16
        g.face([(T + 0.0072, e, zz - 0.03), (T + 0.0072, e + 0.06, zz - 0.045), (T + 0.0072, e + 0.11, zz), (T + 0.0072, e + 0.06, zz + 0.045),
                (T + 0.0072, e, zz + 0.03)], iron, out=(1, 0, 0))
        for i in range(7):
            yy = 0.04 + i * (LW - 0.24) / 6
            p = Vector((T + 0.007, yy, zz))
            g.face([p + Vector((0, -0.008, -0.008)), p + Vector((0, 0.008, -0.008)), p + Vector((0.006, 0, 0))], iron, out=(0.3, 0, -1))
            g.face([p + Vector((0, 0.008, -0.008)), p + Vector((0, 0.008, 0.008)), p + Vector((0.006, 0, 0))], iron, out=(0.3, 1, 0))
            g.face([p + Vector((0, 0.008, 0.008)), p + Vector((0, -0.008, 0.008)), p + Vector((0.006, 0, 0))], iron, out=(0.3, 0, 1))
            g.face([p + Vector((0, -0.008, 0.008)), p + Vector((0, -0.008, -0.008)), p + Vector((0.006, 0, 0))], iron, out=(0.3, -1, 0))
        g.face([(T + 0.0074, 0.0, zz - 0.034), (T + 0.0074, LW - 0.2, zz - 0.034), (T + 0.0074, LW - 0.2, zz - 0.028),
                (T + 0.0074, 0.0, zz - 0.028)], rust, out=(1, 0, 0))
    # Ringzug mit Rosette und Schluesselschild
    yr, zr = LW - 0.12, 1.0
    old3 = g.M
    g.M = g.M @ Matrix.Translation((T, yr, zr)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
    lathe(g, [(0.0, 0.0), (0.045, 0.0), (0.042, 0.004), (0.018, 0.01), (0.012, 0.018), (0.0, 0.02)], iron, seg=10)
    g.M = old3
    ring = [(T + 0.022, yr + 0.05 * math.sin(t), zr - 0.06 + 0.05 * math.cos(t)) for t in [2 * math.pi * i / 12 for i in range(12)]]
    tube(g, ring, 0.007, iron, sides=5, closed=True)
    g.face([(T + 0.0005, yr - 0.022, zr - 0.2), (T + 0.0005, yr + 0.022, zr - 0.2), (T + 0.0005, yr + 0.022, zr - 0.12),
            (T + 0.0005, yr - 0.022, zr - 0.12)], iron, out=(1, 0, 0))
    g.face([(T + 0.0009, yr - 0.005, zr - 0.18), (T + 0.0009, yr + 0.005, zr - 0.18), (T + 0.0009, yr + 0.005, zr - 0.15),
            (T + 0.0009, yr - 0.005, zr - 0.15)], mat('schluesselloch', (0.01, 0.01, 0.01), 'paint', r=1.0), out=(1, 0, 0))
    g.M = old
    # Kloben an der Zarge
    for zz in (0.32 + 0.012, LH - 0.35 + 0.012):
        tube(g, [(0.43, -W / 2 + 0.09, zz - 0.04), (0.43, -W / 2 + 0.09, zz + 0.04)], 0.012, iron, sides=6, caps=True)
    g.build('tuer_alt')


# --- Inschrift ----------------------------------------------------------------------------------------------------------------
# Roemische Kapitalis aus Strichen (eigene Zeichnung): Einheit = Versalhoehe; dick 0,13, duenn 0,05; Serifen.
TK, TN = 0.13, 0.05
LETTER_W = {'P': 0.6, 'R': 0.68, 'O': 0.8, 'M': 0.92, 'E': 0.54, 'T': 0.62, 'H': 0.74, 'I': 0.26, 'B': 0.62, 'L': 0.52, 'C': 0.7, 'A': 0.74}


def _arc(cx, cy, rx, ry, a0, a1, n, wa, wb, tilt=0.35):
    """Bogen als Punkte mit Strichstaerke (dick bei 0/180 Grad gegen die Achse geneigt)."""
    out = []
    for i in range(n + 1):
        a = a0 + (a1 - a0) * i / n
        w = wb + (wa - wb) * abs(math.cos(a - tilt)) ** 1.5
        out.append(((cx + rx * math.cos(a), cy + ry * math.sin(a)), w))
    return out


def _letter_strokes(ch):
    """Striche: Liste von Polylinien [((x, y), Breite), ...] in Versalhoehe 1, Ursprung links unten."""
    S = []
    w = LETTER_W[ch]

    def line(a, b, wd):
        S.append([(a, wd), (b, wd)])

    def serif(x, y, up=True, ln=0.13):
        yy = y if up else y
        line((x - ln, yy), (x + ln, yy), 0.035)
    if ch == 'I':
        line((w / 2, 0.0), (w / 2, 1.0), TK)
        serif(w / 2, 0.018)
        serif(w / 2, 0.982)
    elif ch == 'L':
        line((0.08, 0.0), (0.08, 1.0), TK)
        line((0.08, 0.03), (w, 0.03), 0.06)
        serif(0.08, 0.982)
        line((w - 0.01, 0.0), (w - 0.01, 0.1), 0.03)
    elif ch == 'E':
        line((0.08, 0.0), (0.08, 1.0), TK)
        for (yy, ln) in ((0.97, w), (0.5, w - 0.08), (0.03, w)):
            line((0.08, yy), (ln, yy), 0.06)
            line((ln - 0.01, yy - (0.08 if yy > 0.9 else 0.04)), (ln - 0.01, yy + (0.0 if yy > 0.9 else 0.06 if yy < 0.1 else 0.04)), 0.03)
    elif ch == 'T':
        line((0.0, 0.97), (w, 0.97), 0.06)
        line((w / 2, 0.0), (w / 2, 0.97), TK)
        serif(w / 2, 0.018)
        for xx in (0.01, w - 0.01):
            line((xx, 0.88), (xx, 1.0), 0.03)
    elif ch == 'H':
        for xx in (0.08, w - 0.08):
            line((xx, 0.0), (xx, 1.0), TK)
            serif(xx, 0.018)
            serif(xx, 0.982)
        line((0.08, 0.52), (w - 0.08, 0.52), 0.05)
    elif ch in 'PRB':
        line((0.08, 0.0), (0.08, 1.0), TK)
        serif(0.08, 0.018)
        if ch == 'B':
            S.append([((0.08, 0.97), TN), ((0.32, 0.97), TN)] + _arc(0.32, 0.755, 0.2, 0.215, math.pi / 2, -math.pi / 2, 8, 0.12, 0.045)[1:] +
                     [((0.08, 0.54), TN)])
            S.append([((0.08, 0.54), TN), ((0.35, 0.54), TN)] + _arc(0.35, 0.285, 0.24, 0.255, math.pi / 2, -math.pi / 2, 8, 0.13, 0.05)[1:] +
                     [((0.08, 0.03), TN)])
        else:
            S.append([((0.08, 0.97), TN), ((0.33, 0.97), TN)] + _arc(0.33, 0.72, 0.24, 0.25, math.pi / 2, -math.pi / 2, 8, 0.13, 0.05)[1:] +
                     [((0.08, 0.47), TN)])
        if ch == 'R':
            line((0.3, 0.47), (w - 0.04, 0.0), 0.11)
            serif(w - 0.05, 0.018, ln=0.09)
    elif ch == 'O':
        S.append(_arc(w / 2, 0.5, w / 2 - 0.05, 0.49, 0.0, 2 * math.pi, 20, TK + 0.01, TN))
    elif ch == 'C':
        arc = _arc(w / 2 + 0.03, 0.5, w / 2 - 0.02, 0.49, math.radians(48), math.radians(318), 16, TK, TN)
        S.append(arc)
        (ex, ey), _ = arc[0]
        line((ex, ey - 0.06), (ex, ey + 0.08), 0.035)
    elif ch == 'M':
        line((0.07, 0.0), (0.1, 1.0), 0.05)
        line((0.1, 1.0), (w / 2, 0.02), TK)
        line((w / 2, 0.02), (w - 0.1, 1.0), 0.05)
        line((w - 0.1, 1.0), (w - 0.07, 0.0), TK)
        serif(0.07, 0.018, ln=0.09)
        serif(w - 0.07, 0.018, ln=0.1)
    elif ch == 'A':
        line((0.05, 0.0), (w / 2, 1.0), 0.05)
        line((w / 2, 1.0), (w - 0.05, 0.0), TK)
        line((0.18, 0.32), (w - 0.18, 0.32), 0.05)
        serif(0.05, 0.018, ln=0.08)
        serif(w - 0.05, 0.018, ln=0.1)
    return S


def _ridge(g, m, pts, scale, origin, hgt):
    """Strich als Grat (zwei Flanken) auf einer Flaeche, die nach +x blickt. pts [((y, z), Breite)] in Versalhoehe."""
    x0, y0, z0 = origin
    P = [Vector((0.0, y0 + p[0] * scale, z0 + p[1] * scale)) for (p, w) in pts]
    Wd = [w * scale for (p, w) in pts]
    n = len(P)
    L, R, C = [], [], []
    for i in range(n):
        if i == 0:
            t = (P[1] - P[0]).normalized()
        elif i == n - 1:
            t = (P[-1] - P[-2]).normalized()
        else:
            t = (P[i + 1] - P[i - 1]).normalized()
        nn = Vector((0.0, -t.z, t.y))
        L.append(P[i] + nn * Wd[i] / 2)
        R.append(P[i] - nn * Wd[i] / 2)
        C.append(P[i] + Vector((hgt, 0, 0)))
    up = Vector((1.0, 0, 0))
    for i in range(n - 1):
        for (A, B) in ((L, C), (C, R)):
            q = [A[i], A[i + 1], B[i + 1], B[i]]
            q = [v + Vector((x0, 0, 0)) for v in q]
            g.face(q, m, out=up)
    for (i, j) in ((0, 1), (n - 1, n - 2)):  # Enden
        tdir = (P[i] - P[j]).normalized()
        g.face([v + Vector((x0, 0, 0)) for v in (L[i], C[i], R[i])], m, out=up * 0.5 + tdir)


def _inscription(g, m, text, center, cap, hgt=0.0035, track=0.12):
    widths = [LETTER_W.get(ch, 0.45) if ch != ' ' else 0.25 for ch in text]
    total = sum(widths) + track * (len(text) - 1)
    y = center[1] - total * cap / 2
    for ch, wd in zip(text, widths):
        if ch != ' ':
            for st in _letter_strokes(ch):
                _ridge(g, m, st, cap, (center[0], y, center[2] - cap / 2), hgt)
        y += (wd + track) * cap


def build_inscription_bibliotheca():
    """Inschrift "PROMETHEI BIBLIOTHECA" (Wiki: an einigen Waenden): Marmortafel mit gefaster Kante in profiliertem
    Granitrahmen, Rosetten, erhabene vergoldete Buchstaben in roemischer Kapitalis (eigene Zeichnung), Haarrisse und
    Patina unten (Wandteil, Mitte im Ursprung, 3,44 x 0,56 m)."""
    M = _m()
    g = Geo()
    granite = mat('granit', (0.23, 0.22, 0.22), 'ceramic', 'terrazzo_antique', r=0.9)
    marble = mat('marmor_tafel', (0.96, 0.95, 0.92), 'ceramic', 'marble_white', r=2.5)
    patina = mat('marmor_patina', (0.72, 0.68, 0.60), 'ceramic', 'marble_white', r=3.5)
    # Rahmen: Profil mit Gehrung um das Rechteck (u nach aussen, v entlang +x)
    Yh, Zh = 1.64, 0.205
    prof = [(0.0, 0.05), (0.012, 0.058), (0.03, 0.062), (0.05, 0.055), (0.062, 0.042), (0.075, 0.02), (0.08, 0.0)]
    sweep(g, prof, [(0.0, -Yh, -Zh), (0.0, Yh, -Zh), (0.0, Yh, Zh), (0.0, -Yh, Zh)], (1, 0, 0), granite, closed=True, smooth=55, flip=True)
    g.face([(0.05, -Yh, -Zh), (0.05, Yh, -Zh), (0.05, Yh, Zh), (0.05, -Yh, Zh)], granite, out=(1, 0, 0))
    # Tafel mit Fase
    g.cbox((0.05, -1.6, -0.18), (0.068, 1.6, 0.18), marble, c=0.006, skip=('-x',), mats={'-z': patina})
    g.face([(0.0682, -1.6 + 0.01, -0.178), (0.0682, 1.6 - 0.01, -0.178), (0.0682, 1.6 - 0.01, -0.15), (0.0682, -1.6 + 0.01, -0.15)], patina,
           out=(1, 0, 0))
    for (y, z, ln) in ((1.1, 0.12, 0.13), (-0.7, -0.1, 0.09), (0.35, 0.16, 0.06)):  # Haarrisse
        g.face([(0.0683, y, z), (0.0683, y + 0.002, z), (0.0683, y + ln * 0.4 + 0.001, z - ln), (0.0683, y + ln * 0.4, z - ln)],
               mat('riss', (0.35, 0.33, 0.30), 'ceramic', r=1.0), out=(1, 0, 0))
    for s in (-1, 1):  # Rosetten
        lathe_m = Matrix.Translation((0.068, s * 1.5, 0.0)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
        old = g.M
        g.M = lathe_m
        lathe(g, [(0.0, 0.0), (0.05, 0.0), (0.048, 0.006), (0.035, 0.01), (0.028, 0.006), (0.02, 0.012), (0.0, 0.016)], M['gold_w'], seg=12)
        g.M = old
    _inscription(g, M['gold'], 'PROMETHEI BIBLIOTHECA', (0.068, 0.0, 0.0), 0.2)
    g.build('inschrift')


# --- Gemaelde -----------------------------------------------------------------------------------------------------------------
class Canvas:
    """Malschichten auf der Leinwand (yz-Ebene, Blick +x): jede Schicht 0,25 mm vor der vorigen."""

    def __init__(self, g, x0, tone=0.0):
        self.g, self.x, self.tone, self.cache = g, x0, tone, {}

    def m(self, c, r=0.62):
        c = _mix(c, (0.20, 0.14, 0.07), self.tone)  # nachgedunkelter Firnis
        key = tuple(round(v, 3) for v in c)
        if key not in self.cache:
            self.cache[key] = mat('farbe_%02x%02x%02x' % tuple(int(v * 255) for v in key), key, 'paint', r=r)
        return self.cache[key]

    def poly(self, pts, c, r=0.62):
        self.x += 0.00025
        self.g.face([(self.x, y, z) for (y, z) in pts], self.m(c, r), out=(1, 0, 0))

    def ell(self, cy, cz, ry, rz, c, n=12, a=0.0):
        ca, sa = math.cos(a), math.sin(a)
        self.poly([(cy + ry * math.cos(t) * ca - rz * math.sin(t) * sa, cz + ry * math.cos(t) * sa + rz * math.sin(t) * ca)
                   for t in [2 * math.pi * i / n for i in range(n)]], c)


def _frame(g, M, W, H, gilt=True, d=0.07, fw=0.1):
    """Bilderrahmen mit Profil und Gehrung; Blattgold mit Poliment (rot in den Kehlen) oder Nussbaum."""
    if gilt:
        A, B, C = M['gold'], M['gold_w'], mat('poliment', (0.45, 0.16, 0.10), 'paint', r=0.8)
    else:
        A, B, C = M['walnut'], M['walnut_d'], M['gold_w']
    # u nach aussen ab der Bildkante, v entlang +x
    prof = [(-0.006, 0.022), (0.0, 0.03), (0.012, 0.034), (0.02, 0.028), (0.03, 0.036), (0.045, 0.05), (0.058, 0.06), (0.07, d),
            (0.082, d - 0.004), (0.092, d - 0.02), (fw, 0.035), (fw, 0.0)]
    mats = [B, A, C, A, B, A, A, A, B, B, B]
    sweep(g, prof, [(0.0, -W / 2, -H / 2), (0.0, W / 2, -H / 2), (0.0, W / 2, H / 2), (0.0, -W / 2, H / 2)], (1, 0, 0), A, closed=True,
          smooth=55, flip=True, mats=mats)
    if gilt:  # Eckornamente (Palmetten)
        for (sy, sz) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            cy, cz = sy * (W / 2 + 0.06), sz * (H / 2 + 0.06)
            old = g.M
            g.M = Matrix.Translation((d - 0.004, cy, cz)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
            lathe(g, [(0.0, 0.0), (0.04, 0.0), (0.034, 0.008), (0.018, 0.014), (0.0, 0.018)], A, seg=8, mats=[B, A, A, A])
            g.M = old


def _painting(scene, gilt, tone, seed):
    M = _m()
    g = Geo()
    W, H = 1.6, 1.05
    _frame(g, M, W, H, gilt)
    rng = random.Random(seed)
    cv = Canvas(g, 0.021, tone)
    cv.poly([(-W / 2 - 0.01, -H / 2 - 0.01), (W / 2 + 0.01, -H / 2 - 0.01), (W / 2 + 0.01, H / 2 + 0.01), (-W / 2 - 0.01, H / 2 + 0.01)],
            (0.16, 0.12, 0.08))
    scene(cv, W, H, rng)
    # Craquele, Fehlstellen und nachgedunkelter Rand
    for k in range(10):
        y, z = rng.uniform(-W / 2 + 0.05, W / 2 - 0.05), rng.uniform(-H / 2 + 0.05, H / 2 - 0.05)
        a = rng.uniform(0, math.pi)
        ln = rng.uniform(0.03, 0.09)
        cv.poly([(y, z), (y + 0.0012, z + 0.0012), (y + ln * math.cos(a) + 0.0012, z + ln * math.sin(a)), (y + ln * math.cos(a), z + ln * math.sin(a) - 0.0012)],
                (0.10, 0.08, 0.06))
    for (y, z) in ((W / 2 - 0.12, -H / 2 + 0.08), (-W / 2 + 0.3, H / 2 - 0.06)):
        cv.ell(y, z, 0.012, 0.008, (0.62, 0.56, 0.46), n=7)
    edge = (0.10, 0.08, 0.05)
    b = 0.035
    cv.poly([(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, -H / 2 + b), (-W / 2, -H / 2 + b)], edge)
    cv.poly([(-W / 2, H / 2 - b), (W / 2, H / 2 - b), (W / 2, H / 2), (-W / 2, H / 2)], edge)
    g.build('gemaelde')


def _figure(cv, y, z0, h, robe, skin, hair, lean=0.0, arm=1, cloak=None, beard=False, hold=None):
    """Stehende Gestalt im Gewand, Licht von links oben: Schuhe, Tunika mit welligem Saum, Schattenseite, Falten mit
    Lichtkanten, Guertel, Mantel ueber der linken Schulter, Arm mit Aermel und Hand, Hals, Kopf mit Haar, Gesichtsschatten,
    Augen, Nase; beard: grauer Bart; hold: 'buch' oder 'rolle' in der Hand."""
    w = h * 0.25
    L = lean
    sh = _mix(robe, (0.02, 0.01, 0.0), 0.5)
    hi = _mix(robe, (1.0, 0.95, 0.82), 0.3)

    def P(u, v):  # u quer (in w), v hoch (in h); oben mit Neigung
        return (y + u * w + L * v / 0.85, z0 + v * h)
    for s in (-1, 1):  # Schuhe
        cv.ell(y + s * w * 0.22, z0 + h * 0.006, w * 0.16, h * 0.012, (0.16, 0.11, 0.07), n=8)
    hem = [P(-0.62, 0.02), P(-0.3, 0.0), P(0.0, 0.015), P(0.3, 0.0), P(0.62, 0.02)]
    body = hem + [P(0.5, 0.3), P(0.4, 0.5), P(0.46, 0.68), P(0.5, 0.76), P(0.38, 0.82), P(0.15, 0.84), P(-0.15, 0.84), P(-0.38, 0.82),
                  P(-0.5, 0.76), P(-0.46, 0.68), P(-0.4, 0.5), P(-0.5, 0.3)]
    cv.poly(body, robe)
    cv.poly([P(0.12, 0.012), P(0.3, 0.0), P(0.62, 0.02), P(0.5, 0.3), P(0.4, 0.5), P(0.46, 0.68), P(0.5, 0.76), P(0.38, 0.82), P(0.28, 0.6), P(0.2, 0.3)],
            sh)
    for (u0, u1) in ((-0.38, -0.3), (-0.12, -0.02), (0.12, 0.2)):  # Falten: Schatten und Lichtkante
        cv.poly([P(u0, 0.02), P(u0 + 0.07, 0.02), P((u0 + u1) / 2 + 0.02, 0.48), P((u0 + u1) / 2, 0.48)], sh)
        cv.poly([P(u0 - 0.06, 0.02), P(u0 - 0.01, 0.02), P((u0 + u1) / 2 - 0.02, 0.46), P((u0 + u1) / 2 - 0.035, 0.46)], hi)
    cv.poly([P(-0.42, 0.49), P(0.42, 0.49), P(0.41, 0.52), P(-0.41, 0.52)], _mix(robe, (0.6, 0.45, 0.15), 0.5))  # Guertel
    cv.poly([P(-0.44, 0.56), P(-0.3, 0.57), P(-0.32, 0.78), P(-0.46, 0.75)], hi)
    if cloak:  # Mantel ueber der linken Schulter, faellt rechts herab
        csh = _mix(cloak, (0.02, 0.01, 0.0), 0.45)
        cv.poly([P(-0.52, 0.8), P(-0.2, 0.84), P(0.35, 0.5), P(0.62, 0.22), P(0.66, 0.04), P(0.36, 0.06), P(0.3, 0.3), P(-0.1, 0.55), P(-0.5, 0.62)], cloak)
        cv.poly([P(0.35, 0.5), P(0.62, 0.22), P(0.66, 0.04), P(0.5, 0.05), P(0.46, 0.3)], csh)
        cv.poly([P(-0.5, 0.62), P(-0.1, 0.55), P(-0.08, 0.58), P(-0.48, 0.66)], csh)
    # Arm mit Aermel und Hand
    sx = 0.42 * arm
    elbow = P(sx + 0.12 * arm, 0.6)
    hand = P(sx + 0.5 * arm, 0.62) if hold else P(sx + 0.1 * arm, 0.44)
    a0, a1 = P(sx - 0.1 * arm, 0.8), P(sx + 0.1 * arm, 0.78)
    cv.poly([a0, a1, (elbow[0] + 0.03 * w * arm, elbow[1]), (elbow[0] - 0.12 * w * arm, elbow[1] + 0.02 * h)], robe)
    cv.poly([(elbow[0] - 0.12 * w * arm, elbow[1] + 0.02 * h), (elbow[0] + 0.05 * w * arm, elbow[1] - 0.01 * h), (hand[0], hand[1] - 0.012 * h),
             (hand[0] - 0.02 * w * arm, hand[1] + 0.02 * h)], _mix(robe, (0.02, 0.01, 0.0), 0.2))
    cv.ell(hand[0], hand[1], w * 0.09, h * 0.022, skin, n=8)
    if hold == 'buch':
        cv.poly([(hand[0] - 0.08 * w, hand[1] + 0.01 * h), (hand[0] + 0.25 * w * arm, hand[1] + 0.03 * h), (hand[0] + 0.24 * w * arm, hand[1] + 0.08 * h),
                 (hand[0] - 0.09 * w, hand[1] + 0.06 * h)], (0.42, 0.14, 0.08))
    elif hold == 'rolle':
        cv.poly([(hand[0], hand[1] - 0.05 * h), (hand[0] + 0.05 * w, hand[1] - 0.05 * h), (hand[0] + 0.05 * w, hand[1] + 0.09 * h),
                 (hand[0], hand[1] + 0.09 * h)], (0.86, 0.80, 0.64))
    # Hals und Kopf
    hx, hz = P(0.0, 0.915)
    cv.poly([(hx - w * 0.1, z0 + h * 0.83), (hx + w * 0.1, z0 + h * 0.83), (hx + w * 0.09, z0 + h * 0.89), (hx - w * 0.09, z0 + h * 0.89)],
            _mix(skin, (0, 0, 0), 0.25))
    hw, hh = h * 0.055, h * 0.07
    cv.ell(hx, hz, hw, hh, skin, n=14)
    cv.poly([(hx + hw * 0.15, hz + hh * 0.9), (hx + hw * 0.95, hz + hh * 0.3), (hx + hw * 0.85, hz - hh * 0.5), (hx + hw * 0.3, hz - hh * 0.95),
             (hx + hw * 0.1, hz - hh * 0.3)], _mix(skin, (0.1, 0.02, 0.0), 0.35))  # Schattenseite des Gesichts
    hair_pts = [(hx - hw * 1.05, hz - hh * 0.1), (hx - hw * 1.0, hz + hh * 0.6), (hx - hw * 0.5, hz + hh * 1.08), (hx + hw * 0.3, hz + hh * 1.1),
                (hx + hw * 1.0, hz + hh * 0.7), (hx + hw * 1.05, hz + hh * 0.1), (hx + hw * 0.7, hz + hh * 0.45), (hx, hz + hh * 0.62),
                (hx - hw * 0.7, hz + hh * 0.4)]
    cv.poly(hair_pts, hair)
    for s in (-1, 1):  # Augen
        cv.poly([(hx + s * hw * 0.38 - hw * 0.13, hz + hh * 0.1), (hx + s * hw * 0.38 + hw * 0.13, hz + hh * 0.1), (hx + s * hw * 0.38 + hw * 0.1, hz + hh * 0.2),
                 (hx + s * hw * 0.38 - hw * 0.1, hz + hh * 0.2)], (0.10, 0.07, 0.05))
    cv.poly([(hx + hw * 0.02, hz + hh * 0.05), (hx + hw * 0.12, hz - hh * 0.32), (hx - hw * 0.05, hz - hh * 0.3)], _mix(skin, (0.1, 0.02, 0.0), 0.45))
    if beard:
        cv.poly([(hx - hw * 0.8, hz - hh * 0.2), (hx + hw * 0.8, hz - hh * 0.2), (hx + hw * 0.5, hz - hh * 1.3), (hx, hz - hh * 1.55), (hx - hw * 0.5, hz - hh * 1.3)],
                (0.74, 0.72, 0.68))


def _sky(cv, W, H, top, bot, z_lo, n=8):
    """Himmel mit weichem Verlauf (n Baender, ueberlappend)."""
    for k in range(n):
        t0, t1 = k / n, (k + 1) / n
        c = _mix(top, bot, (t0 + t1) / 2)
        za = H / 2 - (H / 2 - z_lo) * t0
        zb = H / 2 - (H / 2 - z_lo) * t1 - 0.01
        cv.poly([(-W / 2, zb), (W / 2, zb), (W / 2, za), (-W / 2, za)], c)


def _cloud(cv, y, z, s, c):
    for (dy, dz, ry, rz, f) in ((0.0, 0.0, 1.0, 0.24, 0.0), (-0.45, 0.06, 0.55, 0.2, 0.08), (0.35, 0.09, 0.6, 0.26, 0.12), (0.05, 0.14, 0.45, 0.2, 0.2)):
        cv.ell(y + dy * s, z + dz * s, ry * s, rz * s, _mix(c, (1.0, 0.98, 0.92), f), n=12)


def _scene_temple(cv, W, H, rng):
    """Landschaft mit Rundtempel, Zypressen, Pinien, ferner Stadt und fuenf Gestalten (eigenes Motiv)."""
    _sky(cv, W, H, (0.26, 0.36, 0.46), (0.76, 0.68, 0.50), -0.02)
    for (y, z, s) in ((-0.42, 0.37, 0.14), (0.22, 0.41, 0.18), (0.6, 0.3, 0.1)):
        _cloud(cv, y, z, s, (0.78, 0.76, 0.68))
    hills = [((0.46, 0.52, 0.56), 0.02, 0.07), ((0.38, 0.44, 0.42), -0.05, 0.06), ((0.30, 0.34, 0.22), -0.12, 0.05)]
    for k, (c, zb, amp) in enumerate(hills):  # Ferne in Luftperspektive
        pts = [(-W / 2, -H / 2 + 0.2)] + [(-W / 2 + W * t, zb + amp * math.sin(math.pi * t * 2.3 + zb * 9) + 0.015 * math.sin(t * 17 + k))
                                         for t in [i / 18 for i in range(19)]] + [(W / 2, -H / 2 + 0.2)]
        cv.poly(pts, c)
        if k == 0:  # ferne Stadt mit Tuermen
            for i in range(7):
                yy = 0.05 + i * 0.045
                th = 0.03 + 0.02 * ((i * 7) % 3)
                cv.poly([(yy, 0.02), (yy + 0.03, 0.02), (yy + 0.03, 0.02 + th), (yy + 0.015, 0.03 + th), (yy, 0.02 + th)], (0.50, 0.54, 0.56))
    cv.poly([(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, -0.17), (-W / 2, -0.2)], (0.33, 0.27, 0.15))
    cv.poly([(-W / 2, -0.2), (W / 2, -0.17), (W / 2, -0.21), (-W / 2, -0.24)], (0.38, 0.33, 0.18))
    cv.poly([(-0.1, -H / 2), (0.25, -H / 2), (0.08, -0.19), (0.03, -0.19)], (0.50, 0.42, 0.28))  # Weg
    for (y, z, h) in ((-0.72, -0.2, 0.42), (0.62, -0.18, 0.5), (0.7, -0.2, 0.36), (-0.12, -0.12, 0.2)):  # Zypressen
        cv.poly([(y - 0.035, z), (y + 0.035, z), (y + 0.024, z + h * 0.55), (y + 0.012, z + h * 0.85), (y, z + h), (y - 0.012, z + h * 0.85),
                 (y - 0.024, z + h * 0.55)], (0.12, 0.16, 0.08))
        cv.poly([(y + 0.004, z), (y + 0.035, z), (y + 0.024, z + h * 0.55), (y + 0.012, z + h * 0.85), (y + 0.003, z + h * 0.95)], (0.07, 0.09, 0.05))
    for (y, z) in ((0.36, -0.13), (0.47, -0.11)):  # Pinien
        cv.poly([(y - 0.006, z), (y + 0.006, z), (y + 0.004, z + 0.16), (y - 0.004, z + 0.16)], (0.22, 0.15, 0.09))
        cv.ell(y, z + 0.18, 0.09, 0.03, (0.16, 0.22, 0.10), n=12)
        cv.ell(y + 0.02, z + 0.19, 0.06, 0.02, (0.24, 0.30, 0.14), n=10)
    # Rundtempel: Stufen, Saeulen (Licht und Schatten), Gebaelk, Kuppel
    st, sd = (0.74, 0.68, 0.56), (0.48, 0.42, 0.34)
    tx = -0.42
    for k in range(3):
        cv.poly([(tx - 0.24 + k * 0.02, -0.24 + k * 0.025), (tx + 0.24 - k * 0.02, -0.24 + k * 0.025), (tx + 0.24 - k * 0.02, -0.215 + k * 0.025),
                 (tx - 0.24 + k * 0.02, -0.215 + k * 0.025)], st if k % 2 else sd)
    cv.poly([(tx - 0.19, -0.165), (tx + 0.19, -0.165), (tx + 0.19, 0.18), (tx - 0.19, 0.18)], (0.20, 0.17, 0.13))  # Cella dunkel
    cv.poly([(tx - 0.04, -0.165), (tx + 0.04, -0.165), (tx + 0.04, 0.06), (tx - 0.04, 0.06)], (0.10, 0.08, 0.06))  # Tuer
    for k in range(6):
        y = tx - 0.18 + k * 0.072
        cv.poly([(y, -0.165), (y + 0.032, -0.165), (y + 0.03, 0.18), (y + 0.002, 0.18)], st)
        cv.poly([(y + 0.02, -0.165), (y + 0.032, -0.165), (y + 0.03, 0.18), (y + 0.02, 0.18)], sd)
        cv.poly([(y - 0.004, 0.165), (y + 0.036, 0.165), (y + 0.036, 0.18), (y - 0.004, 0.18)], st)  # Kapitell
    cv.poly([(tx - 0.21, 0.18), (tx + 0.21, 0.18), (tx + 0.21, 0.23), (tx - 0.21, 0.23)], st)
    cv.poly([(tx - 0.21, 0.18), (tx + 0.21, 0.18), (tx + 0.21, 0.195), (tx - 0.21, 0.195)], sd)
    dome = [(tx + 0.19 * math.cos(a), 0.23 + 0.13 * math.sin(a)) for a in [math.pi * (1 - i / 10) for i in range(11)]]
    cv.poly(dome, (0.56, 0.52, 0.44))
    cv.poly([(tx + 0.05, 0.23), (tx + 0.19, 0.23)] + [(tx + 0.19 * math.cos(a), 0.23 + 0.13 * math.sin(a)) for a in [i * math.pi / 2 / 5 for i in range(1, 5)]] +
            [(tx + 0.05, 0.36)], (0.40, 0.36, 0.30))
    cv.poly([(tx - 0.012, 0.358), (tx + 0.012, 0.358), (tx + 0.008, 0.39), (tx - 0.008, 0.39)], st)  # Laterne
    skin, hair = (0.70, 0.52, 0.38), (0.24, 0.16, 0.10)
    robes = [((0.55, 0.12, 0.09), (0.18, 0.24, 0.44)), ((0.16, 0.22, 0.44), None), ((0.62, 0.46, 0.20), (0.50, 0.12, 0.10)),
             ((0.22, 0.34, 0.22), None), ((0.48, 0.42, 0.38), (0.30, 0.20, 0.30))]
    for k, (y, h, lean, arm, beard, hold) in enumerate(((-0.02, 0.6, 0.02, 1, True, 'rolle'), (0.16, 0.64, -0.01, -1, False, None),
                                                       (0.34, 0.58, 0.03, 1, False, 'buch'), (0.5, 0.62, -0.02, -1, True, None),
                                                       (0.68, 0.5, 0.0, -1, False, None))):
        _figure(cv, y, -H * 0.43, h, robes[k][0], skin, (0.70, 0.68, 0.64) if beard else hair, lean, arm, robes[k][1], beard, hold)


def _scene_arcade(cv, W, H, rng):
    """Arkade mit drei Boegen, in der Mitte eine Gestalt mit offenem Buch auf einem Thron, Zuhoerer (eigenes Motiv)."""
    wall, wsh = (0.62, 0.54, 0.40), (0.44, 0.37, 0.27)
    cv.poly([(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, H / 2), (-W / 2, H / 2)], wall)
    for k in range(3):
        cy = -0.52 + k * 0.52
        pts = [(cy - 0.18, -0.2), (cy + 0.18, -0.2)] + [(cy + 0.18 * math.cos(a), 0.2 + 0.18 * math.sin(a)) for a in [j * math.pi / 10 for j in range(11)]]
        cv.poly(pts, (0.12, 0.10, 0.09) if k != 1 else (0.36, 0.45, 0.52))
        if k == 1:  # Landschaft im mittleren Bogen
            cv.poly([(cy - 0.18, -0.2), (cy + 0.18, -0.2), (cy + 0.18, 0.02), (cy + 0.05, 0.06), (cy - 0.08, 0.03), (cy - 0.18, 0.05)], (0.34, 0.38, 0.30))
        cv.poly([(cy + 0.14, -0.2), (cy + 0.18, -0.2), (cy + 0.18, 0.2), (cy + 0.14, 0.2)], wsh)  # Laibung im Schatten
        for s in (-1, 1):  # Pilaster
            py = cy + s * 0.22
            cv.poly([(py - 0.035, -0.2), (py + 0.035, -0.2), (py + 0.035, 0.42), (py - 0.035, 0.42)], (0.70, 0.62, 0.48))
            cv.poly([(py + 0.012, -0.2), (py + 0.035, -0.2), (py + 0.035, 0.42), (py + 0.012, 0.42)], wsh)
    cv.poly([(-W / 2, 0.42), (W / 2, 0.42), (W / 2, 0.46), (-W / 2, 0.46)], (0.70, 0.62, 0.48))
    # Schachbrettboden in Perspektive (Fluchtpunkt in der Mitte)
    vz = 0.05
    rows = [-H / 2, -0.43, -0.37, -0.32, -0.28, -0.25, -0.2]
    for j in range(len(rows) - 1):
        za, zb = rows[j], rows[j + 1]
        for i in range(-6, 6):
            def yy(y0, z):
                return y0 * (vz - z) / (vz + H / 2)
            ya0, ya1 = yy(i * 0.16, za), yy((i + 1) * 0.16, za)
            yb0, yb1 = yy(i * 0.16, zb), yy((i + 1) * 0.16, zb)
            c = (0.76, 0.72, 0.62) if (i + j) % 2 else (0.20, 0.16, 0.12)
            cv.poly([(max(-W / 2, ya0), za), (min(W / 2, ya1), za), (min(W / 2, yb1), zb), (max(-W / 2, yb0), zb)], c)
    skin, hair = (0.70, 0.52, 0.38), (0.22, 0.15, 0.10)
    cv.poly([(-0.14, -0.26), (0.14, -0.26), (0.14, 0.14), (0.1, 0.18), (-0.1, 0.18), (-0.14, 0.14)], (0.60, 0.46, 0.20))  # Thron
    cv.poly([(0.08, -0.26), (0.14, -0.26), (0.14, 0.14), (0.08, 0.16)], (0.40, 0.30, 0.12))
    _figure(cv, 0.0, -0.24, 0.42, (0.50, 0.10, 0.10), skin, (0.75, 0.72, 0.68), 0.0, 1, (0.18, 0.22, 0.42), beard=True)
    cv.poly([(-0.075, -0.03), (0.0, -0.045), (0.0, 0.01), (-0.07, 0.025)], (0.88, 0.84, 0.72))  # Buch
    cv.poly([(0.0, -0.045), (0.075, -0.03), (0.07, 0.025), (0.0, 0.01)], (0.80, 0.76, 0.64))
    robes = [(0.16, 0.22, 0.44), (0.22, 0.34, 0.22), (0.62, 0.46, 0.20), (0.32, 0.18, 0.32)]
    for k, y in enumerate((-0.64, -0.42, 0.42, 0.64)):
        _figure(cv, y, -H * 0.46, 0.56, robes[k], skin, hair, 0.02 if y < 0 else -0.02, 1 if y < 0 else -1)


def build_painting_renaissance_a():
    """Gemaelde im Stil der Renaissance (Wiki: ueber den Regalen), 1,6 x 1,05 m: Rundtempel in Landschaft mit Zypressen,
    fuenf Gestalten in Gewaendern mit Faltenschatten; Rahmen mit Blattgold auf Poliment, Eckpalmetten; Firnis, Craquele."""
    _painting(_scene_temple, True, 0.12, 1)


def build_painting_renaissance_a__v1():
    """Wie painting_renaissance_a, Rahmen aus Nussbaum, staerker nachgedunkelt."""
    _painting(_scene_temple, False, 0.3, 2)


def build_painting_renaissance_b():
    """Gemaelde im Stil der Renaissance: Arkade mit drei Boegen, Gestalt mit offenem Buch auf dem Thron, Zuhoerer,
    Schachbrettboden in Perspektive; Goldrahmen."""
    _painting(_scene_arcade, True, 0.12, 3)


def build_painting_renaissance_b__v1():
    """Wie painting_renaissance_b, Rahmen aus Nussbaum, staerker nachgedunkelt."""
    _painting(_scene_arcade, False, 0.3, 4)


# --- Wappen an der Decke ------------------------------------------------------------------------------------------------------
def _ceiling_arms(variant):
    M = _m()
    rng = random.Random(5330 + variant)
    g = Geo()
    stuck = mat('stuck', (0.84, 0.79, 0.68), 'paint', r=1.2)
    stuck_d = mat('stuck_schatten', (0.62, 0.57, 0.48), 'paint', r=1.3)

    def poly(pts, m, layer):
        g.face([(x, y, -0.008 - layer * 0.0003) for (x, y) in pts], m, out=(0, 0, -1))
    poly([(-0.8, -0.8), (0.8, -0.8), (0.8, 0.8), (-0.8, 0.8)], stuck, 0)
    # Stuckring (Profil, haengt bis 3 cm herab), innen Kehle
    lathe(g, [(0.62, -0.008), (0.63, -0.02), (0.66, -0.03), (0.7, -0.028), (0.73, -0.018), (0.76, -0.008)], stuck, seg=40,
          mats=[stuck_d, stuck, stuck, stuck, stuck_d])
    lathe(g, [(0.58, -0.008), (0.59, -0.014), (0.605, -0.012), (0.615, -0.008)], M['gold_w'], seg=40)
    field = [(0.0, 0.2, 0.30), (0.12, 0.10, 0.08)][variant]
    poly([(math.cos(a) * 0.62, math.sin(a) * 0.62) for a in [k * 2 * math.pi / 40 for k in range(40)]], mat('wappen_feld', field, 'paint', r=1.0), 1)
    leaf = mat('lorbeer', (0.22, 0.30, 0.15), 'paint', r=1.0)
    leaf2 = mat('lorbeer_hell', (0.34, 0.42, 0.22), 'paint', r=1.0)
    stem = mat('lorbeer_zweig', (0.30, 0.24, 0.12), 'paint', r=1.0)
    for k in range(40):  # Zweig des Kranzes (oben offen)
        a0, a1 = k * 2 * math.pi / 40, (k + 1) * 2 * math.pi / 40
        if math.sin((a0 + a1) / 2) > 0.92:
            continue
        poly([(math.cos(a0) * 0.527, math.sin(a0) * 0.527), (math.cos(a1) * 0.527, math.sin(a1) * 0.527), (math.cos(a1) * 0.534, math.sin(a1) * 0.534),
              (math.cos(a0) * 0.534, math.sin(a0) * 0.534)], stem, 2)
    for k in range(34):  # Lorbeerblaetter paarweise schraeg zum Zweig, je Seite zur Mitte oben wachsend
        a = k * 2 * math.pi / 34 + 0.05
        if math.sin(a) > 0.88:
            continue
        grow = 1.0 if math.cos(a) >= 0 else -1.0
        for sd in (-1, 1):
            cx, cy = math.cos(a) * (0.53 + sd * 0.024), math.sin(a) * (0.53 + sd * 0.024)
            d = a + grow * math.pi / 2 + sd * grow * 0.6
            tx, ty = math.cos(d), math.sin(d)
            nx, ny = -ty, tx
            L, Wl = 0.034, 0.012
            p = [(cx - tx * L, cy - ty * L), (cx - tx * L * 0.3 + nx * Wl, cy - ty * L * 0.3 + ny * Wl), (cx + tx * L, cy + ty * L),
                 (cx - tx * L * 0.3 - nx * Wl, cy - ty * L * 0.3 - ny * Wl)]
            poly(p, leaf if (k + (sd > 0)) % 2 else leaf2, 3)
    red = mat('wappen_rot', (0.55, 0.10, 0.08), 'paint', r=0.9)
    gold = mat('wappen_gold', (0.80, 0.63, 0.28), 'metal', r=0.7, m=0.5)
    blue = mat('wappen_blau', (0.12, 0.20, 0.45), 'paint', r=0.9)
    silver = mat('wappen_silber', (0.82, 0.82, 0.80), 'paint', r=0.8)
    black = mat('wappen_schwarz', (0.05, 0.05, 0.05), 'paint', r=0.9)
    outline = [(-0.3, 0.34), (0.3, 0.34), (0.3, 0.02)] + [(0.3 * math.cos(a), -0.4 * math.sin(a)) for a in [k * math.pi / 14 for k in range(1, 14)]] + [(-0.3, 0.02)]
    poly([(x * 1.08, y * 1.06 + 0.004) for (x, y) in outline], gold, 3)
    poly(outline, silver, 4)

    def clip_bottom(x0, x1, y0, y1):
        pts = [(x0, y1), (x0, max(y0, -0.4 * math.sqrt(max(0.0, 1 - (x0 / 0.3) ** 2))))]
        for i in range(1, 6):
            x = x0 + (x1 - x0) * i / 6
            pts.append((x, max(y0, -0.4 * math.sqrt(max(0.0, 1 - (x / 0.3) ** 2)))))
        pts += [(x1, max(y0, -0.4 * math.sqrt(max(0.0, 1 - (x1 / 0.3) ** 2)))), (x1, y1)]
        return pts
    if variant == 0:  # geviert mit Sparren
        poly(clip_bottom(-0.3, 0.0, 0.02, 0.34), red, 5)
        poly(clip_bottom(0.0, 0.3, -0.4, 0.02), blue, 5)
        poly([(-0.3, -0.16), (0.0, 0.14), (0.3, -0.16), (0.3, -0.06), (0.0, 0.24), (-0.3, -0.06)], gold, 6)
        for (x, y) in ((0.15, 0.2), (-0.15, -0.2)):  # Sterne
            poly([(x + 0.04 * math.cos(a) * (1 if i % 2 == 0 else 0.45), y + 0.04 * math.sin(a) * (1 if i % 2 == 0 else 0.45))
                  for i, a in enumerate([math.pi / 2 + j * math.pi / 5 for j in range(10)])], black if x > 0 else silver, 7)
    else:  # gespalten mit Turm
        poly(clip_bottom(-0.3, 0.0, -0.4, 0.34), (blue), 5)
        poly([(-0.12, -0.22), (0.12, -0.22), (0.12, 0.12), (0.08, 0.12), (0.08, 0.17), (0.04, 0.17), (0.04, 0.12), (-0.04, 0.12), (-0.04, 0.17),
              (-0.08, 0.17), (-0.08, 0.12), (-0.12, 0.12)], gold, 6)
        poly([(-0.03, -0.22), (0.03, -0.22), (0.03, -0.12), (0.0, -0.09), (-0.03, -0.12)], black, 7)
    crown = [(-0.2, 0.4), (0.2, 0.4), (0.22, 0.54), (0.12, 0.46), (0.06, 0.58), (0.0, 0.48), (-0.06, 0.58), (-0.12, 0.46), (-0.22, 0.54)]
    poly(crown, gold, 6)
    for x in (-0.12, 0.0, 0.12):
        g.face([(x - 0.012, 0.42, -0.0102), (x + 0.012, 0.42, -0.0102), (x + 0.012, 0.444, -0.0102), (x - 0.012, 0.444, -0.0102)], red, out=(0, 0, -1))
    # Schriftband unter dem Schild
    band = [(-0.26, -0.44), (-0.13, -0.415), (0.0, -0.43), (0.13, -0.415), (0.26, -0.44), (0.24, -0.48), (0.13, -0.455), (0.0, -0.47), (-0.13, -0.455),
            (-0.24, -0.48)]
    poly(band, mat('band_pergament', (0.72, 0.64, 0.48), 'paint', r=1.1), 6)
    # Eckrosetten des Stuckfelds, abgeplatzte Farbe, Wasserrand
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        old = g.M
        g.M = Matrix.Translation((sx * 0.7, sy * 0.7, -0.008)) @ Matrix.Rotation(math.pi, 4, 'X')
        lathe(g, [(0.0, 0.0), (0.06, 0.0), (0.05, 0.008), (0.03, 0.014), (0.02, 0.01), (0.0, 0.018)], stuck, seg=10, mats=[stuck_d, stuck, stuck, stuck_d, stuck])
        g.M = old
    for k in range(5):
        cx, cy = rng.uniform(-0.5, 0.5), rng.uniform(-0.5, 0.5)
        r0 = rng.uniform(0.015, 0.04)
        poly([(cx + r0 * rng.uniform(0.6, 1.2) * math.cos(a), cy + r0 * rng.uniform(0.6, 1.2) * math.sin(a)) for a in [i * 2 * math.pi / 7 for i in range(7)]],
             stuck, 9)
    g.build('wappen')


def build_ceiling_arms():
    """Gemaltes Wappen an der Decke (Deckenteil, 1,6 x 1,6 m; Wiki-Bild: Wappen an der Decke der alten Gaenge):
    Stuckfeld mit profiliertem Ring und Goldkehle, Lorbeerkranz, geviertelter Schild mit Sparren und Sternen, Krone,
    Schriftband, Eckrosetten, abgeplatzte Farbe."""
    _ceiling_arms(0)


def build_ceiling_arms__v1():
    """Wappen, Variante 1: gespaltener Schild mit Turm auf dunkelrotem Grund."""
    _ceiling_arms(1)


# --- Abgeloeste Tapete ----------------------------------------------------------------------------------------------------------
def _strip(g, rows, front, back, th=0.0005):
    """Gewoelbter Streifen aus Punktreihen (je Reihe zwei Punkte): Vorderseite (Tapete) und Rueckseite (Papier); der
    Umlaufsinn wird an der ersten Reihe festgelegt (Vorderseite blickt dort von der Wand weg)."""
    sg = None
    for k in range(len(rows) - 1):
        a, b = rows[k]
        c, d = rows[k + 1]
        n = (Vector(b) - Vector(a)).cross(Vector(c) - Vector(a)).normalized()
        if sg is None:
            sg = 1.0 if n.x >= 0 else -1.0
        n = n * sg
        o = n * th
        g.face([a, b, d, c], front, out=n)
        g.face([Vector(a) - o, Vector(b) - o, Vector(d) - o, Vector(c) - o], back, out=-n)


def build_wallpaper_peel():
    """Abgeloeste Tapete (Wandteil, 1 m; Wiki: im Eingangsgang war die Tapete alt und abgeblaettert): Fehlstelle mit
    altem, grauem Putz, Rissen und freiliegendem Lattenwerk, weisse Reisskante, Wasserrand mit Laeufen; die Bahn hat sich
    oben geloest und haengt nach vorn ueber (Rueckseite mit Kleisterflecken sichtbar, unten eingerollt), kleine Locken
    an den Raendern."""
    rng = random.Random(41)
    g = Geo()
    damask = mat('damast', (0.70, 0.66, 0.62), 'paint', 'wallpaper_damask_red', r=0.85)
    back = mat('tapete_rueckseite', (0.78, 0.71, 0.56), 'paint', r=1.3)
    paste = mat('kleisterfleck', (0.62, 0.53, 0.38), 'paint', r=1.1)
    edge = mat('reisskante', (0.86, 0.82, 0.72), 'paint', r=1.3)
    plaster = mat('putz_alt', (0.55, 0.50, 0.42), 'concrete', 'plaster_white', r=1.4)
    lath = mat('latte', (0.34, 0.25, 0.16), 'wood', 'wood_walnut_floor', r=1.4)
    dark = mat('putz_loch', (0.06, 0.05, 0.04), 'paint', r=1.4)
    stain = mat('wasserrand', (0.36, 0.20, 0.11), 'paint', r=1.0)
    zc = 1.92
    pts = []
    for k in range(22):
        a = 2 * math.pi * k / 22
        r = rng.uniform(0.8, 1.0)
        pts.append((math.cos(a) * 0.28 * r, zc + math.sin(a) * 0.36 * r))
    # Wasserrand mit Laeufen nach unten (auf der Tapete)
    st = [(y * 1.22, zc + (z - zc) * 1.18 - (0.06 if z < zc else 0.0)) for (y, z) in pts]
    g.face([(0.0010, y, z) for (y, z) in st], stain, out=(1, 0, 0))
    g.face([(0.0012, y * 0.97, zc + (z - zc) * 0.97 + 0.01) for (y, z) in st], damask, out=(1, 0, 0))
    for (y, ln) in ((-0.12, 0.16), (0.05, 0.22), (0.17, 0.1)):  # Laeufe
        g.face([(0.0011, y - 0.012, zc - 0.36), (0.0011, y + 0.012, zc - 0.36), (0.0011, y + 0.006, zc - 0.36 - ln), (0.0011, y - 0.004, zc - 0.36 - ln)],
               stain, out=(1, 0, 0))
    g.face([(0.0016, y * 1.045, zc + (z - zc) * 1.04) for (y, z) in pts], edge, out=(1, 0, 0))  # Reisskante
    g.face([(0.0020, y, z) for (y, z) in pts], plaster, out=(1, 0, 0))
    hole = [(0.1 * math.cos(a) * rng.uniform(0.75, 1.0) + 0.03, zc + 0.05 + 0.13 * math.sin(a) * rng.uniform(0.75, 1.0)) for a in [2 * math.pi * k / 10 for k in range(10)]]
    g.face([(0.0024, y * 1.12, zc + 0.05 + (z - zc - 0.05) * 1.12) for (y, z) in hole], mat('putz_bruch', (0.70, 0.66, 0.58), 'concrete', 'plaster_white', r=1.5),
           out=(1, 0, 0))
    g.face([(0.0026, y, z) for (y, z) in hole], dark, out=(1, 0, 0))
    for k in range(4):  # Latten im Loch
        z = zc - 0.07 + k * 0.055
        g.face([(0.0029, -0.06, z), (0.0029, 0.12, z), (0.0029, 0.12, z + 0.034), (0.0029, -0.06, z + 0.034)], lath, out=(1, 0, 0))
    for (y, z, ln, a) in ((0.14, 2.12, 0.16, -0.6), (-0.17, 1.82, 0.13, 0.8), (0.06, 2.2, 0.09, 0.2), (-0.05, 1.72, 0.12, -0.3)):  # Risse
        g.face([(0.0027, y, z), (0.0027, y + 0.0022, z), (0.0027, y + ln * math.sin(a) + 0.0018, z - ln), (0.0027, y + ln * math.sin(a), z - ln)],
               mat('putz_riss', (0.22, 0.19, 0.15), 'paint', r=1.3), out=(1, 0, 0))
    # Bahn: unten an der Fehlstelle noch fest (Gelenk bei z = 1,60), oben geloest und nach vorn uebergekippt -> haengt
    # vor der Wand herab, Rueckseite nach aussen, das Ende rollt sich ein
    hinge = zc - 0.32
    rows = []
    for k in range(15):
        t = k / 14
        a = math.pi * 0.92 * min(1.0, t * 1.6)  # Umschlag ueber das Gelenk
        rr = 0.035
        if t < 0.625:  # Falz ueber dem Gelenk
            x, z = 0.002 + rr * (1 - math.cos(a)), hinge + rr * math.sin(a)
        else:
            u = (t - 0.625) / 0.375
            x = 0.002 + 2 * rr + 0.012 * u
            z = hinge - 0.24 * u
            if u > 0.7:  # Rolle am Ende
                b = (u - 0.7) / 0.3 * math.pi * 1.3
                x = 0.002 + 2 * rr + 0.0084 + 0.022 * math.sin(b)
                z = hinge - 0.168 - 0.022 * (1 - math.cos(b))
        w0, w1 = -0.15 + 0.01 * t, 0.17 - 0.02 * t
        rows.append(((x, w0, z), (x, w1, z)))
    _strip(g, rows, damask, back)
    # Kleisterflecken auf der sichtbaren Rueckseite (vor der Bahn)
    for (y, z, r0) in ((-0.05, hinge - 0.08, 0.035), (0.08, hinge - 0.15, 0.025), (0.0, hinge - 0.03, 0.02)):
        g.face([(0.002 + 2 * 0.035 + 0.012 * 0.5 + 0.0012, y + r0 * math.cos(a) * rng.uniform(0.7, 1.1), z + r0 * math.sin(a) * rng.uniform(0.7, 1.1))
                for a in [2 * math.pi * i / 8 for i in range(8)]], paste, out=(1, 0, 0))
    for (yc, zz, s) in ((-0.24, zc + 0.12, -1), (0.25, zc + 0.02, 1)):  # Locken an den Raendern (Rueckseite sichtbar)
        rows = []
        for k in range(6):
            t = k / 5
            a = t * math.pi * 1.1
            x = 0.002 + 0.016 * math.sin(a)
            yy = yc + s * 0.016 * (1 - math.cos(a))
            rows.append(((x, yy, zz - 0.09), (x, yy, zz + 0.09)))
        _strip(g, rows, damask, back)
    g.build('tapete')
