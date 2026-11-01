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

    def face(self, pts, mat, out=None, nrm=None):
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
        self.F.append((P, mat, N))

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
        verts, polys, mats, mi, idx, loopn = [], [], [], {}, [], []
        for P, mat, N in self.F:
            k = mi.get(mat.name)
            if k is None:
                k = mi[mat.name] = len(mats)
                mats.append(mat)
            b = len(verts)
            verts.extend(P)
            polys.append(list(range(b, b + len(P))))
            idx.append(k)
            loopn.extend(N if N else [_nw(P).normalized()] * len(P))
        me = bpy.data.meshes.new(name)
        me.from_pydata([tuple(v) for v in verts], [], polys)
        for m in mats:
            me.materials.append(m)
        me.polygons.foreach_set('material_index', idx)
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


def lathe(g, prof, mat, seg=12, at=(0.0, 0.0, 0.0), smooth=40.0, mats=None, a0=0.0, sx=1.0, sy=1.0, flip=False):
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
            g.face(pts, mats[k] if mats else mat, out=out, nrm=nr)


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
        'walnut': mat('nussbaum', (0.40, 0.25, 0.15), 'wood', 'wood_walnut_floor', r=0.85),
        'walnut_d': mat('nussbaum_dunkel', (0.26, 0.155, 0.095), 'wood', 'wood_walnut_floor', r=0.9),
        'walnut_w': mat('nussbaum_abgegriffen', (0.50, 0.33, 0.20), 'wood', 'wood_walnut_floor', r=0.7),
        'groove': mat('fuge_dunkel', (0.05, 0.035, 0.025), 'paint', r=1.4),
        'dust': mat('staub', (0.44, 0.41, 0.37), 'paint', r=1.5),
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
    'folio': ((0.29, 0.335), (0.035, 0.085)),
    'quarto': ((0.235, 0.295), (0.025, 0.06)),
    'octavo': ((0.175, 0.235), (0.018, 0.045)),
    'duodez': ((0.14, 0.18), (0.015, 0.032)),
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
        nb = 4 if h > 0.22 else 3
        for k in range(nb):
            zc = h * (0.16 + 0.68 * k / (nb - 1))
            d.append((zc - 0.004, zc + 0.004, BK.gold_w if rng.random() < 0.6 else BK.gold))
    elif r < 0.62:  # Schild (Titel) und Linien an Kopf und Fuss
        lab = BK.labels[rng.randrange(3)]
        z0 = h * rng.uniform(0.66, 0.74)
        d.append((z0, z0 + h * 0.10, lab))
        if rng.random() < 0.55:
            d.append((h * 0.92, h * 0.935, BK.gold))
            d.append((h * 0.05, h * 0.065, BK.gold))
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
    worn = kind == 'l' and rng.random() < 0.35
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
        deco += [(h - 0.012, h, b.worn), (0.0, 0.008, b.worn)]
    for (z0, z1, m) in deco:
        g.face([(x, y0 + e, zb + z0), (x, y1 - e, zb + z0), (x, y1 - e, zb + z1), (x, y0 + e, zb + z1)], m, out=(1, 0, 0),
               nrm=[nl, nr, nr, nl])
    if tops:
        a0 = xf - b.d
        cap = min(0.005, b.d * 0.1)
        g.face([(a0, y0, zb + h), (xf - cap, y0, zb + h), (xf - cap, y1, zb + h), (a0, y1, zb + h)], b.edge, out=(0, 0, 1))
        g.face([(xf - cap, y0, zb + h), (xf, y0, zb + h), (xf, y1, zb + h), (xf - cap, y1, zb + h)], b.mat, out=(0, 0, 1))
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
                b = _make_book(rng, BK, y, t, h, min(xf, 0.432), col, faded, deco, edge, side)
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
        g.face([(xf_lo - 0.02, y0, zb + 0.0006), (0.428, y0, zb + 0.0006), (0.428, y1, zb + 0.0006), (xf_lo - 0.02, y1, zb + 0.0006)],
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
    # Zahnschnitt unter der Platte
    for k in range(10):
        y = -0.5 + 0.05 + k * 0.1
        g.box((0.448, y - 0.022, z0 + 0.06), (0.47, y + 0.022, z0 + 0.082), M['walnut'], skip=('-x', '+z'))


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
                 zc + rc * (0.4 + 0.6 * sv) * math.sin(math.pi + 1.25 * math.pi * (1 - sv))) for sv in (0.0, 0.25, 0.5, 0.75)]
        arc = [(xr, yc + R * math.cos(a), zc + R * math.sin(a)) for a in [math.pi * (1 - i / 8) for i in range(9)]]
        right = [(xr, yc + R - rc + rc * (1 - 0.6 * sv) * math.cos(-1.25 * math.pi * sv),
                  zc + rc * (1 - 0.6 * sv) * math.sin(-1.25 * math.pi * sv)) for sv in (0.25, 0.5, 0.75, 1.0)]
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
