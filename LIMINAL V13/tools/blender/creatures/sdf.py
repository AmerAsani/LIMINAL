"""V13: Formwerkzeug fuer die Wesen (KREATUREN_ANALYSE.md, E-021) - Distanzfeld-Anatomie in numpy, Vernetzung mit
OpenVDB, Gewichte nach den formenden Teilen, Hoehlung/Kante je Punkt, drei Stufen, glb mit den Knochen des Spiels.

Koordinaten: Modellraum des Spiels (x vorn, y Seite 0, z oben; skeletons.json aus `LiminalGame --dump-skeletons`).
Beim Anlegen in Blender wird y gespiegelt - der glb-Leser des Spiels spiegelt beim Laden zurueck.

Ein Wesen ist eine Liste von Teilen (Part). Jedes Teil hat eine Form (runder Kegel zwischen zwei Punkten, Ellipsoid,
abgerundeter Kasten, Torus-Stueck), gehoert zu einem Knochen (fuer die Gewichte) und zu einer Materialgruppe, und
verbindet sich weich (k) mit dem, was vorher kam - oder schneidet heraus (sub). Je Materialgruppe entsteht ein Netz.
Verschiebungen (Falten, Adern, Poren, Raeude) sind Funktionen p -> Meter, nur nahe der Oberflaeche ausgewertet.
"""
import json
import math
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))


def skeleton(model):
    """Ruhepose aus skeletons.json: Gelenke (n x 3), Knochen (Liste mit a, b, parent, frame 4x4)."""
    with open(os.path.join(HERE, 'skeletons.json'), encoding='utf-8') as fh:
        j = json.load(fh)[model]
    J = np.array(j['joints'], dtype=np.float64)
    bones = []
    for b in j['bones']:
        f = np.array(b['frame'], dtype=np.float64).reshape(4, 3).T  # Spalten x, y, z, Ursprung
        M = np.eye(4)
        M[:3, :4] = f
        bones.append({'a': int(b['a']), 'b': int(b['b']), 'parent': int(b['parent']), 'radius': b['radius'], 'frame': M})
    return J, bones


# --- Rauschen (numpy, kachelfrei) ---------------------------------------------------------------------------
def _hash(ix, iy, iz, seed):
    h = (ix.astype(np.int64) * 73856093) ^ (iy.astype(np.int64) * 19349663) ^ (iz.astype(np.int64) * 83492791) ^ (seed * 2654435761)
    h = h & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float64) / float(0x1000000)


def vnoise(P, freq, seed=1):
    """Wertrauschen 0..1 an Punkten P (n x 3), Frequenz in 1/m."""
    q = P * freq
    i = np.floor(q)
    f = q - i
    u = f * f * (3 - 2 * f)
    ix, iy, iz = i[:, 0].astype(np.int64), i[:, 1].astype(np.int64), i[:, 2].astype(np.int64)
    out = 0.0
    for dx in (0, 1):
        wx = u[:, 0] if dx else 1 - u[:, 0]
        for dy in (0, 1):
            wy = u[:, 1] if dy else 1 - u[:, 1]
            for dz in (0, 1):
                wz = u[:, 2] if dz else 1 - u[:, 2]
                out = out + wx * wy * wz * _hash(ix + dx, iy + dy, iz + dz, seed)
    return out


def fbm(P, freq, octaves=4, seed=1, gain=0.5):
    s, a, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        s = s + a * (vnoise(P, freq * (2.0 ** o), seed + 17 * o) * 2 - 1)
        tot += a
        a *= gain
    return s / tot


def ridged(P, freq, octaves=3, seed=1):
    """Grate (0..1, 1 auf dem Grat) - Falten, Adern, Risse."""
    s, a, tot = 0.0, 1.0, 0.0
    for o in range(octaves):
        n = 1.0 - np.abs(vnoise(P, freq * (2.0 ** o), seed + 31 * o) * 2 - 1)
        s = s + a * n * n
        tot += a
        a *= 0.5
    return s / tot


def worley(P, freq, seed=1):
    """Abstand zum naechsten und zweitnaechsten Zellpunkt (0..~1) - Schuppen, Poren, Krusten."""
    q = P * freq
    i = np.floor(q).astype(np.int64)
    d1 = np.full(len(P), 9.0)
    d2 = np.full(len(P), 9.0)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                cx, cy, cz = i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz
                fx = cx + _hash(cx, cy, cz, seed)
                fy = cy + _hash(cx, cy, cz, seed + 1)
                fz = cz + _hash(cx, cy, cz, seed + 2)
                d = np.sqrt((q[:, 0] - fx) ** 2 + (q[:, 1] - fy) ** 2 + (q[:, 2] - fz) ** 2)
                d2 = np.where(d < d1, d1, np.minimum(d2, d))
                d1 = np.minimum(d1, d)
    return d1, d2


# --- Formen ---------------------------------------------------------------------------------------------------
def _v(x):
    return np.asarray(x, dtype=np.float64)


class Part:
    """Ein formendes Teil. kind: cone (a, b, ra, rb), ell (c, R 3x3 Spalten = Achsen, r 3), box (c, R, half, round),
    torus (c, R, major, minor, arc), tube (a, b, up, r0, r1: Strecke mit ellipsenfoermigem Querschnitt, r = (quer, hoch)
    an beiden Enden - Ketten aus tube-Teilen mit gemeinsamen Enden ergeben glatte Wuelste ohne Perlen)."""

    def __init__(self, kind, bone, k=0.0, op='add', group='skin', **g):
        self.kind, self.bone, self.k, self.op, self.group, self.g = kind, bone, k, op, group, g
        self._bounds()

    def _bounds(self):
        g = self.g
        if self.kind == 'cone':
            a, b, r = _v(g['a']), _v(g['b']), max(g['ra'], g['rb'])
            lo, hi = np.minimum(a, b) - r, np.maximum(a, b) + r
        elif self.kind == 'ell':
            c, R, r = _v(g['c']), _v(g['R']), _v(g['r'])
            ext = np.abs(R) @ r
            lo, hi = c - ext, c + ext
        elif self.kind == 'box':
            c, R, h = _v(g['c']), _v(g['R']), _v(g['half']) + g.get('round', 0.0)
            ext = np.abs(R) @ h
            lo, hi = c - ext, c + ext
        elif self.kind == 'tube':
            a, b, r = _v(g['a']), _v(g['b']), max(max(g['r0']), max(g['r1']))
            lo, hi = np.minimum(a, b) - r, np.maximum(a, b) + r
        else:  # torus
            c, ext = _v(g['c']), g['major'] + g['minor']
            lo, hi = c - ext, c + ext
        m = self.k + 0.004
        self.lo, self.hi = lo - m, hi + m

    def dist(self, P):
        g = self.g
        if self.kind == 'cone':  # runder Kegel (Kapsel mit zwei Radien), geschlossene Form nach I. Quilez
            a, b, ra, rb = _v(g['a']), _v(g['b']), g['ra'], g['rb']
            ba = b - a
            l2 = float(ba @ ba)
            pa = P - a
            rr = ra - rb
            a2 = l2 - rr * rr
            if l2 < 1e-12 or a2 <= 1e-12:  # eine Kugel enthaelt die andere
                c, r = (a, ra) if ra >= rb else (b, rb)
                return np.linalg.norm(P - c, axis=1) - r
            il2 = 1.0 / l2
            y = pa @ ba
            z = y - l2
            w = pa * l2 - np.outer(y, ba)
            x2 = np.einsum('ij,ij->i', w, w)
            y2 = y * y * l2
            z2 = z * z * l2
            k = math.copysign(1.0, rr) * rr * rr * x2
            out = (np.sqrt(np.maximum(x2 * a2 * il2, 0.0)) + y * rr) * il2 - ra
            c2 = np.sign(y) * a2 * y2 < k
            out = np.where(c2, np.sqrt(x2 + y2) * il2 - ra, out)
            c1 = np.sign(z) * a2 * z2 > k
            out = np.where(c1, np.sqrt(x2 + z2) * il2 - rb, out)
            return out
        if self.kind == 'ell':  # Ellipsoid (Naeherung nach Iquilez)
            c, R, r = _v(g['c']), _v(g['R']), _v(g['r'])
            q = (P - c) @ R  # lokale Koordinaten
            k0 = np.linalg.norm(q / r, axis=1)
            k1 = np.linalg.norm(q / (r * r), axis=1)
            return k0 * (k0 - 1.0) / np.maximum(k1, 1e-9)
        if self.kind == 'tube':  # naechster Punkt auf der Strecke, dort Ellipsoid-Naeherung mit den Radien an dieser Stelle
            a, b = _v(g['a']), _v(g['b'])
            F = frame_from(b - a, g.get('up', (0, 0, 1)))
            ln = float(np.linalg.norm(b - a))
            q = (P - a) @ F
            t = np.clip(q[:, 0] / max(ln, 1e-9), 0.0, 1.0)
            r0, r1 = _v(g['r0']), _v(g['r1'])
            ry = r0[0] + (r1[0] - r0[0]) * t
            rz = r0[1] + (r1[1] - r0[1]) * t
            rx = np.minimum(ry, rz)
            loc = np.stack([q[:, 0] - t * ln, q[:, 1], q[:, 2]], axis=1)
            rr = np.stack([rx, ry, rz], axis=1)
            k0 = np.linalg.norm(loc / rr, axis=1)
            k1 = np.linalg.norm(loc / (rr * rr), axis=1)
            # auf der Achse selbst versagt die Naeherung (0/0) - dort der kleinste Radius nach innen
            return np.where(k1 < 1e-6, -rx, k0 * (k0 - 1.0) / np.maximum(k1, 1e-9))
        if self.kind == 'box':
            c, R, h, rd = _v(g['c']), _v(g['R']), _v(g['half']), g.get('round', 0.0)
            q = np.abs((P - c) @ R) - h
            return np.linalg.norm(np.maximum(q, 0), axis=1) + np.minimum(np.max(q, axis=1), 0) - rd
        # Torus (lokale Achse z), optional nur ein Bogen (arc in rad um die lokale x-Achse symmetrisch)
        c, R = _v(g['c']), _v(g['R'])
        q = (P - c) @ R
        rq = np.sqrt(q[:, 0] ** 2 + q[:, 1] ** 2)
        d = np.sqrt((rq - g['major']) ** 2 + q[:, 2] ** 2) - g['minor']
        if 'arc' in g:
            ang = np.abs(np.arctan2(q[:, 1], q[:, 0]))
            over = np.maximum(ang - g['arc'], 0) * g['major']
            d = np.maximum(d, over - g['minor'] * 0.5)
        return d


def frame_from(x, up):
    """Orthonormale Achsen (Spalten): x entlang, z nach up."""
    x = _v(x)
    x = x / np.linalg.norm(x)
    z = _v(up) - x * float(_v(up) @ x)
    if np.linalg.norm(z) < 1e-6:
        z = np.cross(x, [0, 1, 0])
    z = z / np.linalg.norm(z)
    y = np.cross(z, x)
    return np.stack([x, y, z], axis=1)


def smin(a, b, k):
    if k <= 0:
        return np.minimum(a, b)
    h = np.maximum(k - np.abs(a - b), 0.0) / k
    return np.minimum(a, b) - h * h * k * 0.25


def smax(a, b, k):
    return -smin(-a, -b, k)


# --- Auswertung und Vernetzung ---------------------------------------------------------------------------------
def evaluate(parts, h, disp=None, band=None):
    """Distanzfeld aller Teile einer Gruppe auf einem Gitter (Voxel h). Rueckgabe: Feld (nx, ny, nz), Ursprung."""
    lo = np.min([p.lo for p in parts], axis=0) - 4 * h
    hi = np.max([p.hi for p in parts], axis=0) + 4 * h
    n = np.ceil((hi - lo) / h).astype(int) + 1
    F = np.full(tuple(n), 1.0, dtype=np.float32)
    for p in parts:
        i0 = np.clip(np.floor((p.lo - lo) / h).astype(int), 0, n - 1)
        i1 = np.clip(np.ceil((p.hi - lo) / h).astype(int) + 1, 1, n)
        ys = lo[1] + h * np.arange(i0[1], i1[1])
        zs = lo[2] + h * np.arange(i0[2], i1[2])
        # in Scheiben entlang x (lange, schraege Glieder haben riesige Kaesten - sonst Gigabytes an Punkten)
        step = max(1, int(3_000_000 // max(1, len(ys) * len(zs))))
        for x0 in range(i0[0], i1[0], step):
            x1 = min(i1[0], x0 + step)
            xs = lo[0] + h * np.arange(x0, x1)
            X, Y, Z = np.meshgrid(xs, ys, zs, indexing='ij')
            P = np.stack([X.ravel(), Y.ravel(), Z.ravel()], axis=1)
            d = p.dist(P).reshape(X.shape).astype(np.float32)
            blk = F[x0:x1, i0[1]:i1[1], i0[2]:i1[2]]
            if p.op == 'add':
                blk[...] = smin(blk, d, p.k)
            elif p.op == 'sub':
                blk[...] = smax(blk, -d, p.k)
            else:  # 'int': nur in diesem Bereich, sonst leer - selten gebraucht
                blk[...] = smax(blk, d, p.k)
    if disp is not None:
        bw = band if band is not None else 6 * h
        idx = np.nonzero(np.abs(F) < bw)
        P = np.stack([lo[0] + h * idx[0], lo[1] + h * idx[1], lo[2] + h * idx[2]], axis=1)
        F[idx] += disp(P).astype(np.float32)
    return F, lo


def polygonize(F, lo, h, adaptivity=0.0):
    """Netz der Nullflaeche mit OpenVDB: Punkte (n x 3), Dreiecke (m x 3)."""
    import openvdb as vdb
    bg = float(3 * h)
    A = np.clip(F, -bg, bg).astype(np.float32)
    g = vdb.FloatGrid(bg)
    # nur Werte gleich dem Hintergrund (weit aussen) werden inaktiv - sonst verschiebt sich die Nullflaeche (Terrassen)
    g.copyFromArray(A, ijk=(0, 0, 0), tolerance=bg * 1e-6)
    g.transform = vdb.createLinearTransform(voxelSize=h)
    g.gridClass = vdb.GridClass.LEVEL_SET
    pts, tris, quads = g.convertToPolygons(isovalue=0.0, adaptivity=adaptivity)
    pts = np.asarray(pts, dtype=np.float64) + lo
    tris = np.asarray(tris, dtype=np.int64).reshape(-1, 3)
    quads = np.asarray(quads, dtype=np.int64).reshape(-1, 4)
    T = np.concatenate([tris, quads[:, [0, 1, 2]], quads[:, [0, 2, 3]]]) if len(quads) else tris
    # OpenVDB liefert die Dreiecke nach innen gewunden (geprueft an einer Kugel) - nach aussen drehen
    return pts, T[:, [0, 2, 1]]


def sample(parts, P, disp=None):
    """Distanzfeld an beliebigen Punkten (fuer Hoehlung)."""
    d = np.full(len(P), 1.0)
    for p in parts:
        m = np.all((P >= p.lo) & (P <= p.hi), axis=1)
        if not m.any():
            continue
        dd = np.full(len(P), 1.0)
        dd[m] = p.dist(P[m])
        if p.op == 'add':
            d = smin(d, dd, p.k)
        elif p.op == 'sub':
            d = smax(d, -dd, p.k)
        else:
            d = smax(d, dd, p.k)
    if disp is not None:
        near = np.abs(d) < 0.02
        d[near] += disp(P[near])
    return d


def weights(parts, P, nbones, s=0.004, cut=4.0):
    """Gewichte je Punkt aus den formenden Teilen (nur 'add'): je Knochen der kleinste Abstand seiner Teile,
    w_b ~ exp(-(d_b - d_min)/s). Speicher n x Knochen (nicht n x Teile - die Motte hat Hunderte Haarbueschel)."""
    adds = [p for p in parts if p.op == 'add' and p.bone >= 0]
    D = np.full((len(P), nbones), 9.0)
    for p in adds:
        m = np.all((P >= p.lo - 0.02) & (P <= p.hi + 0.02), axis=1)
        if m.any():
            D[m, p.bone] = np.minimum(D[m, p.bone], p.dist(P[m]))
    dmin = D.min(axis=1, keepdims=True)
    W = np.exp(-np.minimum((D - dmin) / s, cut * 3))
    W[(D - dmin) > cut * s] = 0.0
    W /= np.maximum(W.sum(axis=1, keepdims=True), 1e-12)
    return W


def vertex_normals(V, T):
    fn = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
    N = np.zeros_like(V)
    for k in range(3):
        np.add.at(N, T[:, k], fn)
    return N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)


def cavity_edge(parts, V, N, disp=None, scale=0.01):
    """Hoehlung (0..1, Falten und Ritzen) aus der Verdeckung entlang der Normalen; Kante (0..1, Woelbung) aus dem
    Abstand des Felds ausserhalb im Vergleich zur Ebene."""
    occ = np.zeros(len(V))
    tot = 0.0
    for i, t in enumerate((0.25, 0.5, 1.0, 2.0)):
        dt = t * scale
        d = sample(parts, V + N * dt, disp)
        occ += np.clip((dt - d) / dt, 0, 1) / (2 ** i)
        tot += 1.0 / (2 ** i)
    cav = np.clip(occ / tot * 1.6, 0, 1)
    # Kante: an Kanten ist das Feld nach aussen weiter als dt (konvex) - Unterschied normiert
    dt = scale * 0.6
    d = sample(parts, V + N * dt, disp)
    edge = np.clip((d - dt) / dt * 2.5, 0, 1)
    return cav, edge


def smooth_weights(W, T, iters=2):
    n = len(W)
    nb = [set() for _ in range(n)]
    for a, b, c in T:
        nb[a].update((b, c))
        nb[b].update((a, c))
        nb[c].update((a, b))
    for _ in range(iters):
        W2 = W.copy()
        for i in range(n):
            if nb[i]:
                W2[i] = 0.5 * W[i] + 0.5 * W[list(nb[i])].mean(axis=0)
        W = W2
    return W / np.maximum(W.sum(axis=1, keepdims=True), 1e-12)
