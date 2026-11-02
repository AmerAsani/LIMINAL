"""V13.3: Level 3 - "purpifizierte" tote Wesen in den Ziegelwaenden (KREATUR_L3_Purpur.md).

Wiki (backrooms-wiki.wikidot.com/level-3, Rev. 18, Abschnitt Purpification; CC BY-SA 3.0; eigene Worte): Selten findet man
tote Wesen, die zum Teil in die Wand genoclippt sind. Sie und die Wandstellen direkt um sie herum sind immer ganz violett;
aus dem Koerper stroemt violette Energie ueber die Waende. Ziegel und Kadaver enthalten viel Gammastrahlung und eine
unbekannte, starke Energie, die violettes Licht abgibt; herausgeloeste Ziegel haben violette Schlieren im Inneren.
Bekannt: ein Skin-Stealer in einem dunkleren Bereich, ein Hound mit einer gemalten Botschaft daneben.

Modelle (Wandmodelle: Wandflaeche bei x = 0, Ursprung am Boden, +x in den Gang, y entlang der Wand):
- purple_stealer: Skin-Stealer (gross, lange Finger, schwerer Brauenbogen, tiefe Augenhoehlen, Noppen auf der Haut), der
  halb aus der Wand steigt: Kopf in den Nacken gegen die Ziegel gepresst, Kiefer aufgerissen, der linke Arm zieht sich
  an der Wand entlang, die Finger in die Fugen gekrallt; Knie und Fuss ragen heraus. Die rechte Hand kommt an falscher
  Stelle tief unten aus der Wand (Interpretation: der Noclip hat den Koerper in der Wand verschoben).
- purple_hound: Hound (Mensch auf allen vieren, abgemagert, Rippen und Wirbel unter der Haut), mit Kopf und Schultern
  aus der Wand gebrochen, eine Hand auf dem Beton, das Haar haengt versteinert bis auf den Boden; daneben die gemalte
  Botschaft (eigene Worte; das Wiki gibt sie nur sinngemaess wieder).
Gemeinsam: violette Haut mit glasigen Rissen, aus denen Licht dringt (Augen, Schlund und die Naht zur Wand am hellsten);
um den Koerper sind die Ziegel in drei Stufen violett - mit der Textur der Wand, deckungsgleich, weil die Ausstattung
ihre Texturen wie die Waende aus Weltkoordinaten projiziert; verzweigte Lichtbahnen ueber die Ziegel (wie
Lichtenberg-Figuren, Interpretation von "stroemt ueber die Waende"), herausgedrueckte Ziegelbrocken, am Boden
Bruchstuecke mit violett leuchtenden Schlieren im Inneren.
Aufbau: Distanzfeld (tools/blender/creatures/sdf.py) -> OpenVDB-Netz, hinter der Wand und unter dem Boden abgeschnitten,
ausgeduennt; die Materialien je Dreieck aus denselben Feldern (Risse, Hoehlung, Woelbung, leuchtende Stellen).
"""
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

import common
from common import material
from models.models_l1_sectors import Kit, catmull, sweep

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'creatures'))
import sdf  # noqa: E402

V = Vector
TAU = 2.0 * math.pi
WALL_TEX = 'bricks_grey_black'  # die Gaenge mit den Fundstellen (ziegel_grau, ziegel_dunkel)


# =====================================================================================================================
# Materialien
# =====================================================================================================================
def mats():
    return {
        # Haut: matt in der Flaeche, glasig auf Knochen und Wuelsten (die Purpifizierung als Glasur), tief in den Falten
        'skin': material('purpur_haut', (0.33, 0.13, 0.50), look='rubber', rough=0.6, r=0.85),
        'skin_deep': material('purpur_haut_falte', (0.08, 0.02, 0.14), look='rubber', rough=0.8, r=1.1),
        # Runde 4: Glasur dunkler und satter - der Glanz kommt aus dem Licht, nicht aus heller Farbe (vorher Flecken wie Spielzeug)
        'skin_sheen': material('purpur_glasur', (0.44, 0.20, 0.70), look='plastic', rough=0.15, r=0.26),
        'crack': material('purpur_riss', (0.62, 0.24, 1.0), look='glow', glow=0.8),
        'hot': material('purpur_glut', (0.90, 0.66, 1.0), look='glow', glow=2.6),
        'seam': material('purpur_naht', (0.70, 0.36, 1.0), look='glow', glow=1.5),
        'fleshbrick': material('purpur_fleischziegel', (0.42, 0.20, 0.58), look='concrete', tex='concrete_rough', rough=0.9, r=1.15),
        'pore': material('purpur_noppe', (0.58, 0.22, 1.0), look='glow', glow=0.75),
        'sinew': material('purpur_sehne', (0.22, 0.07, 0.30), look='rubber', rough=0.4, r=0.5),
        'skin_mottle': material('purpur_haut_fleck', (0.27, 0.09, 0.43), look='rubber', rough=0.5, r=0.7),
        'teeth': material('purpur_zahn', (0.74, 0.64, 0.86), look='ceramic', rough=0.3, r=0.5),
        'nail': material('purpur_nagel', (0.13, 0.05, 0.19), look='plastic', rough=0.3, r=0.35),
        'hair': material('purpur_haar', (0.09, 0.03, 0.14), look='plastic', rough=0.35, r=0.45),
        # die Ziegel um den Koerper: Textur der Wand, violett durchgefaerbt; der Kern glimmt selbst (die Energie im Stein)
        # Runde 4: satter (Wiki: die Wand direkt um sie ist immer ganz violett) - im hellen Gang war der Rand nur lavendelgrau
        'brick1': material('purpur_ziegel_kern', (0.46, 0.08, 1.0), look='concrete', tex=WALL_TEX, glow=0.5, rough=0.8, r=0.85),
        'brick2': material('purpur_ziegel_mitte', (0.52, 0.16, 1.0), look='concrete', tex=WALL_TEX, glow=0.22, rough=0.85, r=0.9),
        'brick3': material('purpur_ziegel_rand', (0.58, 0.28, 1.0), look='concrete', tex=WALL_TEX, glow=0.06, rough=0.9, r=0.95),
        'burn': material('purpur_brand', (0.12, 0.04, 0.17), look='concrete', tex='concrete_rough', rough=0.95, r=1.2),
        'vein': material('purpur_bahn', (0.60, 0.22, 1.0), look='glow', glow=0.65),
        # Runde 4: herausgedrueckte Brocken sind Ziegel derselben violetten Wand (vorher grau wie fremder Schutt)
        'chunk': material('purpur_brocken', (0.50, 0.16, 0.92), look='concrete', tex=WALL_TEX, glow=0.1, rough=0.9, r=1.0),
        'chunk_core': material('purpur_schliere', (0.70, 0.34, 1.0), look='glow', glow=0.9),
        'dust': material('purpur_staub', (0.42, 0.30, 0.52), look='concrete', tex='concrete_rough', rough=1.0, r=1.4),
        'paint': material('farbe_kalk', (0.80, 0.76, 0.67), look='paint', rough=0.9, r=1.3),
    }


# =====================================================================================================================
# Distanzfeld-Bausteine
# =====================================================================================================================
def _n(x):
    x = np.asarray(x, dtype=np.float64)
    return x / np.linalg.norm(x)


def cone(a, b, ra, rb=None, k=0.0, op='add'):
    return sdf.Part('cone', 0, k=k, op=op, a=a, b=b, ra=ra, rb=ra if rb is None else rb)


def ell(c, r, x=(1, 0, 0), up=(0, 0, 1), k=0.0, op='add'):
    return sdf.Part('ell', 0, k=k, op=op, c=c, R=sdf.frame_from(x, up), r=r)


def chain(pts, radii, k=0.0):
    return [cone(pts[i], pts[i + 1], radii[i], radii[i + 1], k=k) for i in range(len(pts) - 1)]


def rot_about(v, axis, ang):
    """Rodrigues: v um axis (Einheitsvektor) drehen."""
    v, axis = np.asarray(v, dtype=np.float64), _n(axis)
    return v * math.cos(ang) + np.cross(axis, v) * math.sin(ang) + axis * float(axis @ v) * (1 - math.cos(ang))


def hand(wrist, fwd, back, s=1.0, curl=(0.4, 0.5, 0.5), spread=0.16, flen=1.0, k=0.008, thumb_curl=0.4):
    """Hand mit langen Fingern. fwd: vom Handgelenk zu den Knoecheln, back: Handruecken. Rueckgabe (Teile, Spitzen)."""
    wrist, fwd = np.asarray(wrist, dtype=np.float64), _n(fwd)
    back = _n(np.asarray(back, dtype=np.float64) - fwd * float(np.asarray(back) @ fwd))
    side = np.cross(back, fwd)
    parts = [ell(wrist + fwd * 0.05 * s, (0.055 * s, 0.043 * s, 0.017 * s), x=fwd, up=back, k=k),
             cone(wrist - fwd * 0.03 * s, wrist + fwd * 0.02 * s, 0.026 * s, 0.03 * s, k=k)]
    tips = []
    lens = [(0.058, 0.040, 0.030), (0.064, 0.044, 0.032), (0.060, 0.042, 0.030), (0.050, 0.034, 0.026)]
    for i, off in enumerate((-1.5, -0.5, 0.5, 1.5)):
        base = wrist + fwd * 0.098 * s + side * off * 0.019 * s + back * 0.004 * s
        d = _n(rot_about(fwd, back, off * spread * 0.5))
        p = base
        r = 0.0105 * s
        for j, L in enumerate(lens[i]):
            d = _n(rot_about(d, side, curl[j]))  # zur Handflaeche hin (side x fwd = -back)
            q = p + d * L * s * flen * 1.25
            parts.append(cone(p, q, r, r * 0.86, k=k * 0.6))
            parts.append(ell(p, (r * 1.15, r * 1.15, r * 1.05), x=d, up=back, k=k * 0.5))  # Knoechel
            p, r = q, r * 0.86
        tips.append((p, d))
    tb = wrist + fwd * 0.03 * s - side * 0.032 * s
    td = _n(fwd * 0.6 - side * 0.8)
    p = tb
    for j, L in enumerate((0.045, 0.034, 0.028)):
        td = _n(rot_about(td, fwd, thumb_curl * 0.5))
        q = p + td * L * s * flen * 1.15
        parts.append(cone(p, q, 0.013 * s * (0.9 ** j), 0.012 * s * (0.9 ** j), k=k * 0.6))
        p = q
    tips.append((p, td))
    return parts, tips


# =====================================================================================================================
# Netz aus dem Feld
# =====================================================================================================================
def _decimate(Vt, T, target):
    me = bpy.data.meshes.new('tmp')
    me.from_pydata(Vt.tolist(), [], T.tolist())
    me.update()
    ob = bpy.data.objects.new('tmp', me)
    bpy.context.scene.collection.objects.link(ob)
    mod = ob.modifiers.new('dec', 'DECIMATE')
    mod.ratio = min(1.0, target / max(1, len(T)))
    mod.use_collapse_triangulate = True
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    m2 = ev.to_mesh()
    m2.calc_loop_triangles()
    V2 = np.zeros(len(m2.vertices) * 3)
    m2.vertices.foreach_get('co', V2)
    T2 = np.zeros(len(m2.loop_triangles) * 3, dtype=np.int64)
    m2.loop_triangles.foreach_get('vertices', T2)
    ev.to_mesh_clear()
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.meshes.remove(me)
    return V2.reshape(-1, 3), T2.reshape(-1, 3)


def field_mesh(parts, h, disp=None, xcut=-0.02, zcut=0.0, tris=None):
    """Netz der Teile: hinter der Wand (x < xcut) und unter dem Boden (z < zcut) abgeschnitten."""
    F, lo = sdf.evaluate(parts, h, disp)
    xs = (lo[0] + h * np.arange(F.shape[0])).astype(np.float32)
    zs = (lo[2] + h * np.arange(F.shape[2])).astype(np.float32)
    F = np.maximum(F, (xcut - xs)[:, None, None])
    F = np.maximum(F, (zcut - zs)[None, None, :])
    Vt, T = sdf.polygonize(F, lo, h)
    del F
    # was ganz in der Wand liegt, sieht niemand (Schnittdeckel, Leib hinter der Wandflaeche)
    keep = Vt[T][:, :, 0].max(axis=1) > -0.007
    T = T[keep]
    if tris and len(T) > tris:
        Vt, T = _decimate(Vt, T, tris)
    return Vt, T


def mesh_object(name, Vt, T, mlist, mi):
    me = bpy.data.meshes.new(name)
    me.from_pydata(Vt.tolist(), [], T.tolist())
    me.update()
    for m in mlist:
        me.materials.append(m)
    me.polygons.foreach_set('material_index', np.asarray(mi, dtype=np.int32))
    me.polygons.foreach_set('use_smooth', np.ones(len(T), dtype=bool))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


class Glaze:
    """Glasur-Risse der purpifizierten Haut: Worley-Zellkanten (Rinne + Leuchten), dichter nahe der Wand."""

    def __init__(self, seed, freq=11.0, width=0.07, depth=0.0026, wall_bias=0.55):
        self.seed, self.freq, self.width, self.depth, self.wall_bias = seed, freq, width, depth, wall_bias

    def mask(self, P):
        return sdf.fbm(P, 3.2, 3, self.seed + 5) + self.wall_bias * np.clip((0.16 - P[:, 0]) / 0.16, 0, 1) - 0.05

    def edge(self, P):
        d1, d2 = sdf.worley(P, self.freq, self.seed)
        return d2 - d1

    def groove(self, P):
        return smoothstep(self.width, 0.0, self.edge(P)) * smoothstep(0.0, 0.25, self.mask(P))


class Fusion:
    """Wo der Koerper in der Wand steckt, wird das Fleisch zu Mauerwerk: Ziegelplatten im Laeuferverband mit Fugen,
    nach aussen auslaufend (Interpretation von "genoclippt": Wand und Leib sind ineinander geraten)."""

    def __init__(self, reach=0.13, bw=0.225, bh=0.072, mortar=0.011):
        self.reach, self.bw, self.bh, self.mortar = reach, bw, bh, mortar

    def weight(self, P):
        return smoothstep(self.reach, 0.015, P[:, 0])

    def mortar_dist(self, P):
        row = np.floor(P[:, 2] / self.bh)
        off = (row % 2) * 0.5 * self.bw
        u = (P[:, 1] + off) / self.bw
        v = P[:, 2] / self.bh
        du = np.abs(u - np.round(u)) * self.bw
        dv = np.abs(v - np.round(v)) * self.bh
        return np.minimum(du, dv)

    def disp(self, P):
        w = self.weight(P)
        md = self.mortar_dist(P)
        groove = smoothstep(self.mortar, 0.002, md)
        return w * (0.0065 * groove - 0.004 * (1.0 - groove))


def classify_skin(parts, disp, Vt, T, glaze, hot_spots, seam_x=0.012, sheen=0.42, deep=0.42, fusion=None, pores=None):
    """Materialindex je Dreieck: 0 Haut, 1 Falte, 2 Glasur, 3 Riss, 4 Glut, 5 Naht zur Wand, 6 Fleischziegel, 7 Noppe,
    8 dunkler Fleck (die Haut ist ungleich durchgefaerbt)."""
    C = Vt[T].mean(axis=1)
    N = sdf.vertex_normals(Vt, T)
    cav, edge = sdf.cavity_edge(parts, Vt, N, disp, 0.014)
    cavf, edgef = cav[T].mean(axis=1), edge[T].mean(axis=1)
    mi = np.zeros(len(T), dtype=np.int32)
    mott = sdf.fbm(C, 9.0, 3, 401) + 0.6 * sdf.fbm(C, 26.0, 2, 402)
    mi[mott > 0.2] = 8
    mi[edgef > sheen] = 2
    mi[cavf > deep] = 1
    if pores is not None:
        mi[pores(C)] = 7
    e = glaze.edge(C)
    m = glaze.mask(C)
    crack = (e < glaze.width * 0.62) & (m > 0.12)
    mi[crack] = 3
    if fusion is not None:
        w = fusion.weight(C)
        md = fusion.mortar_dist(C)
        mi[(w > 0.45) & (md > fusion.mortar * 0.7)] = 6
        mi[(w > 0.45) & (md <= fusion.mortar * 0.7)] = 1
    mi[C[:, 0] < seam_x] = 5
    for (c, r) in hot_spots:
        mi[np.linalg.norm(C - np.asarray(c), axis=1) < r] = 4
    print('Materialien:', np.bincount(mi, minlength=9).tolist(), flush=True)
    return mi


# =====================================================================================================================
# Wand: violette Ziegel in drei Stufen, Lichtbahnen, herausgedrueckte Brocken, Bruchstuecke am Boden
# =====================================================================================================================
def contact(parts, disp, y0, y1, z0, z1, x=0.004, step=0.012):
    """Wo der Koerper die Wandebene schneidet: Maske auf einem Gitter (y, z) und die Randpunkte."""
    ys = np.arange(y0, y1, step)
    zs = np.arange(max(z0, 0.0), z1, step)
    Y, Z = np.meshgrid(ys, zs, indexing='ij')
    P = np.stack([np.full(Y.size, x), Y.ravel(), Z.ravel()], axis=1)
    inside = (sdf.sample(parts, P, disp) < 0).reshape(Y.shape)
    edge = inside.copy()
    edge[1:-1, 1:-1] = inside[1:-1, 1:-1] & ~(inside[:-2, 1:-1] & inside[2:, 1:-1] & inside[1:-1, :-2] & inside[1:-1, 2:])
    pts = np.stack([Y[edge], Z[edge]], axis=1)
    return pts


def _dist_to(pts, Q):
    """kleinster Abstand der Punkte Q (n x 2) zu pts (m x 2), in Bloecken."""
    out = np.full(len(Q), 9.0)
    for i in range(0, len(pts), 256):
        d = np.linalg.norm(Q[:, None, :] - pts[None, i:i + 256, :], axis=2).min(axis=1)
        out = np.minimum(out, d)
    return out


def stain(K, M, cpts, rings, rng, seed, zmax, step=0.03):
    """Violette Ziegel um die Beruehrungslinie: Stufen (Radius, Material, Tiefe) als ebene Flaechen vor der Wand,
    Rand mit Rauschen, an die Isolinie gezogen und zu wenigen Dreiecken zusammengefasst."""
    rmax = max(r for r, _, _ in rings) + 0.25
    y0, y1 = cpts[:, 0].min() - rmax, cpts[:, 0].max() + rmax
    z0, z1 = 0.0, min(zmax, cpts[:, 1].max() + rmax)
    ys = np.arange(y0, y1 + step, step)
    zs = np.arange(z0, z1 + step, step)
    Y, Z = np.meshgrid(ys, zs, indexing='ij')
    Q = np.stack([Y.ravel(), Z.ravel()], axis=1)
    d = _dist_to(cpts, Q)
    P3 = np.stack([np.zeros(len(Q)), Q[:, 0], Q[:, 1]], axis=1)
    L = d + 0.10 * sdf.fbm(P3, 2.2, 3, seed) + 0.05 * sdf.fbm(P3, 7.0, 2, seed + 3)
    L = L.reshape(Y.shape)
    for (rad, mat, depth) in rings:
        bm = bmesh.new()
        ny, nz = L.shape
        vid = {}
        inside = L < rad

        def vert(i, j):
            key = (i, j)
            if key not in vid:
                vid[key] = bm.verts.new((depth, ys[i], zs[j]))
            return vid[key]
        for i in range(ny - 1):
            for j in range(nz - 1):
                if inside[i, j] or inside[i + 1, j] or inside[i, j + 1] or inside[i + 1, j + 1]:
                    bm.faces.new([vert(i, j), vert(i + 1, j), vert(i + 1, j + 1), vert(i, j + 1)])
        # aeussere Punkte an die Isolinie ziehen: Mittel der Schnittpunkte zu allen inneren Nachbarn (auch schraeg)
        moved = {}
        for (i, j), v in vid.items():
            if inside[i, j]:
                continue
            acc, cnt = [0.0, 0.0], 0
            for di in (-1, 0, 1):
                for dj in (-1, 0, 1):
                    a, b = i + di, j + dj
                    if (di or dj) and 0 <= a < ny and 0 <= b < nz and inside[a, b]:
                        t = (rad - L[a, b]) / max(L[i, j] - L[a, b], 1e-6)
                        acc[0] += ys[a] + (ys[i] - ys[a]) * t
                        acc[1] += zs[b] + (zs[j] - zs[b]) * t
                        cnt += 1
            if cnt:
                moved[v] = (acc[0] / cnt, max(0.0, acc[1] / cnt))
        for v, (yy, zz) in moved.items():
            v.co.y, v.co.z = yy, zz
        # Rand glaetten (in der Wandebene; der Boden bleibt Boden)
        for _ in range(3):
            edge_vs = [v for v in bm.verts if v.is_boundary and v.co.z > 0.002]
            bmesh.ops.smooth_vert(bm, verts=edge_vs, factor=0.5, use_axis_x=False, use_axis_y=True, use_axis_z=True)
        bmesh.ops.dissolve_limit(bm, angle_limit=0.01, verts=list(bm.verts), edges=list(bm.edges))
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        # Normale nach +x (in den Gang)
        for f in bm.faces:
            f.normal_update()
            if f.normal.x < 0:
                f.normal_flip()
        bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-5)
        dst = K(mat)
        mapv = {}
        for f in bm.faces:
            vs = []
            for v in f.verts:
                if v not in mapv:
                    mapv[v] = dst.verts.new(v.co)
                vs.append(mapv[v])
            try:
                dst.faces.new(vs)
            except ValueError:
                pass
        bm.free()
    return Y, Z, L


def ribbon(bm, pts, widths, x, normal_x=True):
    """Flaches Band auf der Wandebene (x fest) entlang 2D-Punkten (y, z)."""
    n = len(pts)
    if n < 2:
        return
    L, R = [], []
    for i in range(n):
        a = pts[max(0, i - 1)]
        b = pts[min(n - 1, i + 1)]
        dy, dz = b[0] - a[0], b[1] - a[1]
        ln = math.hypot(dy, dz) or 1.0
        py, pz = -dz / ln, dy / ln
        w = widths[i] * 0.5
        L.append(bm.verts.new((x, pts[i][0] + py * w, max(0.0005, pts[i][1] + pz * w))))
        R.append(bm.verts.new((x, pts[i][0] - py * w, max(0.0005, pts[i][1] - pz * w))))
    for i in range(n - 1):
        f = bm.faces.new([L[i], R[i], R[i + 1], L[i + 1]])
        f.normal_update()
        if (f.normal.x < 0) == normal_x:
            f.normal_flip()


def lichtenberg(K, M, cpts, center, rng, n_roots=12, reach=(0.35, 0.95), zmax=2.5, avoid=None):
    """Verzweigte Lichtbahnen von der Beruehrungslinie nach aussen; darunter ein dunkler Brandhof."""
    glow, burn = K(M['vein']), K(M['burn'])
    idx = rng.sample(range(len(cpts)), min(n_roots, len(cpts)))
    cy, cz = center

    def grow(p, d, w, depth, length):
        pts, ws = [p], [w]
        steps = max(3, int(length / 0.018))
        for _ in range(steps):
            away = np.array([p[0] - cy, (p[1] - cz) * 0.8])
            na = np.linalg.norm(away) or 1.0
            ang = math.atan2(d[1], d[0]) + rng.gauss(0.0, 0.42)
            d = np.array([math.cos(ang), math.sin(ang)]) * 0.8 + away / na * 0.2
            d = d / np.linalg.norm(d)
            p = p + d * 0.018 * rng.uniform(0.7, 1.3)
            if p[1] < 0.004 or p[1] > zmax:
                break
            if avoid is not None and avoid(p):
                break
            w *= 0.983
            pts.append(p.copy())
            ws.append(w)
            if depth < 4 and rng.random() < 0.085 - depth * 0.012:
                a2 = math.atan2(d[1], d[0]) + rng.choice((-1, 1)) * rng.uniform(0.45, 1.05)
                grow(p.copy(), np.array([math.cos(a2), math.sin(a2)]), w * 0.62, depth + 1, length * rng.uniform(0.35, 0.6))
        if len(pts) >= 2:
            ws = [ww * (1.0 - 0.75 * (i / (len(ws) - 1)) ** 2) for i, ww in enumerate(ws)]
            ribbon(glow, pts, ws, 0.0046)
            ribbon(burn, pts, [ww * 2.6 + 0.003 for ww in ws], 0.0039)
    for i in idx:
        p = np.array(cpts[i], dtype=np.float64)
        d = np.array([p[0] - cy, (p[1] - cz) * 0.8])
        d = d / (np.linalg.norm(d) or 1.0)
        grow(p, d, rng.uniform(0.0055, 0.0095), 0, rng.uniform(*reach))


def brick_chunk(bm, c, size, rng, mtx_rot=None):
    """Unregelmaessiger Ziegelbrocken (Kasten mit verschobenen Ecken), Mitte c, Groesse (x, y, z)."""
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r['verts']:
        v.co.x = v.co.x * size[0] * rng.uniform(0.8, 1.15)
        v.co.y = v.co.y * size[1] * rng.uniform(0.8, 1.15)
        v.co.z = v.co.z * size[2] * rng.uniform(0.8, 1.15)
        if mtx_rot is not None:
            v.co = mtx_rot @ v.co
        v.co += V(c)
    return r


def pushed_bricks(K, M, cpts, center, rng, n=22):
    """Ziegel, die der Koerper beim Hindurchtreten aus der Wand gedrueckt hat (gekippt, gebrochen)."""
    from mathutils import Euler
    cy, cz = center
    for i in rng.sample(range(len(cpts)), min(n, len(cpts))):
        p = cpts[i]
        d = np.array([p[0] - cy, p[1] - cz])
        d = d / (np.linalg.norm(d) or 1.0)
        q = p + d * rng.uniform(0.02, 0.07)
        if q[1] < 0.03:
            continue
        rot = Euler((rng.uniform(-0.25, 0.25), rng.uniform(-0.35, 0.35), rng.uniform(-0.4, 0.4)), 'XYZ').to_matrix()
        bm = K(M['chunk'], 30.0)
        brick_chunk(bm, (rng.uniform(0.0, 0.025), q[0], q[1]), (0.05, 0.12, 0.055), rng, rot)
        if rng.random() < 0.45:  # aufgebrochene Stirnseite mit leuchtender Schliere
            g = K(M['chunk_core'])
            yy, zz = q[0] + rng.uniform(-0.03, 0.03), q[1] + rng.uniform(-0.015, 0.015)
            ribbon(g, [(yy - 0.03, zz), (yy, zz + 0.006), (yy + 0.03, zz - 0.004)], [0.006, 0.01, 0.005], 0.052)


def floor_debris(K, M, y0, y1, rng, n=9, xmax=0.42):
    """Bruchstuecke am Boden vor der Wand: aussen Ziegel, innen violette Schlieren (Wiki); violetter Staub."""
    from mathutils import Euler
    for _ in range(n):
        s = rng.uniform(0.5, 1.0)
        size = (0.05 * s, rng.uniform(0.06, 0.12) * s, 0.032 * s)
        c = (rng.uniform(0.04, xmax), rng.uniform(y0, y1), size[2] * 0.5)
        rot = Euler((rng.uniform(-0.15, 0.15), rng.uniform(-0.15, 0.15), rng.uniform(0, TAU)), 'XYZ').to_matrix()
        brick_chunk(K(M['chunk'], 30.0), c, size, rng, rot)
        if rng.random() < 0.6:  # Bruchflaeche oben: violette Schlieren
            g = K(M['chunk_core'])
            a = rng.uniform(0, TAU)
            for k in range(2):
                pts = []
                for t in range(4):
                    u = (t / 3.0 - 0.5) * size[1] * 0.9
                    pts.append(V((c[0] + math.cos(a) * u + rng.uniform(-0.006, 0.006), c[1] + math.sin(a) * u + rng.uniform(-0.006, 0.006),
                                  size[2] + 0.002)))
                for i in range(len(pts) - 1):
                    p, q = pts[i], pts[i + 1]
                    side = V((-(q.y - p.y), q.x - p.x, 0)).normalized() * 0.003
                    fc = g.faces.new([g.verts.new(p - side), g.verts.new(q - side), g.verts.new(q + side), g.verts.new(p + side)])
                    fc.normal_update()
                    if fc.normal.z < 0:
                        fc.normal_flip()
    # Staub: flacher, unregelmaessiger Fleck
    bm = K(M['dust'])
    ring = []
    cy = 0.5 * (y0 + y1)
    ry = 0.5 * (y1 - y0) + 0.15
    ph = [rng.uniform(0, TAU) for _ in range(3)]
    for k in range(32):
        a = TAU * k / 32
        rr = 1.0 + 0.14 * math.sin(2 * a + ph[0]) + 0.09 * math.sin(3 * a + ph[1]) + 0.05 * math.sin(5 * a + ph[2])
        ring.append(bm.verts.new((0.02 + 0.22 * (1 + math.cos(a)) * rr, cy + ry * math.sin(a) * rr, 0.0018)))
    f = bm.faces.new(ring)
    f.normal_update()
    if f.normal.z < 0:
        f.normal_flip()


# =====================================================================================================================
# Gemalte Botschaft (eigene Strichschrift; Wortlaut in eigenen Worten, siehe KREATUR_L3_Purpur.md)
# =====================================================================================================================
GLYPHS = {
    'A': (4, [[(0, 0), (2, 6), (4, 0)], [(0.8, 2.4), (3.2, 2.4)]]),
    'D': (4, [[(0, 0), (0, 6), (2, 6), (3.4, 5), (4, 3), (3.4, 1), (2, 0), (0, 0)]]),
    'E': (3.4, [[(3.4, 6), (0, 6), (0, 0), (3.4, 0)], [(0, 3.1), (2.8, 3.1)]]),
    'F': (3.4, [[(3.4, 6), (0, 6), (0, 0)], [(0, 3.1), (2.8, 3.1)]]),
    'G': (4, [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0), (3, 0), (4, 1), (4, 2.8), (2.2, 2.8)]]),
    'H': (4, [[(0, 0), (0, 6)], [(4, 0), (4, 6)], [(0, 3), (4, 3)]]),
    'I': (0.6, [[(0.3, 0), (0.3, 6)]]),
    'L': (3.4, [[(0, 6), (0, 0), (3.4, 0)]]),
    'M': (4.6, [[(0, 0), (0, 6), (2.3, 2.4), (4.6, 6), (4.6, 0)]]),
    'N': (4, [[(0, 0), (0, 6), (4, 0), (4, 6)]]),
    'O': (4.2, [[(2.1, 6), (0.6, 5.4), (0, 3), (0.6, 0.6), (2.1, 0), (3.6, 0.6), (4.2, 3), (3.6, 5.4), (2.1, 6)]]),
    'R': (4, [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3.1), (0, 3.1)], [(2, 3.1), (4, 0)]]),
    'S': (4, [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 4), (1, 3.1), (3, 2.9), (4, 2), (4, 1), (3, 0), (1, 0), (0, 1)]]),
    'T': (4, [[(0, 6), (4, 6)], [(2, 6), (2, 0)]]),
    'U': (4, [[(0, 6), (0, 1), (1, 0), (3, 0), (4, 1), (4, 6)]]),
    'V': (4, [[(0, 6), (2, 0), (4, 6)]]),
    'W': (5.4, [[(0, 6), (1.2, 0), (2.7, 4.2), (4.2, 0), (5.4, 6)]]),
    'Y': (4, [[(0, 6), (2, 3), (4, 6)], [(2, 3), (2, 0)]]),
    ',': (0.8, [[(0.6, 0.5), (0.1, -1.2)]]),
    '.': (0.8, [[(0.3, 0.0), (0.45, 0.35)]]),
    ' ': (2.6, []),
}
# Eigene Worte nach dem Sinn der Seite (ein Schoepfer klagt ueber sein entstelltes Geschoepf, will es wieder mit echten
# Augen sehen und mit echter Hand beruehren, Leib und Geist heilen). Englisch wie die anderen Spuren der Ebene; die
# Uebersetzung blendet das Spiel beim Lesen ein (gameplay/purpification.cpp).
MESSAGE = ['MY LOVELY ONE', 'YOU WERE NEVER MEANT FOR THIS', 'ONE DAY I WILL SEE YOU AGAIN', 'WITH TRUE EYES, TRUE HANDS',
           'AND MEND YOU. FLESH AND MIND.']


def paint_message(K, M, y0, z_top, width, rng, line_h=0.115):
    """Pinselschrift auf der Wand: je Strich ein Band mit Druckverlauf, Tropfnasen nach unten."""
    bm = K(M['paint'])
    cap = line_h * 0.62
    sx = cap / 6.0
    zline = z_top
    for li, line in enumerate(MESSAGE):
        adv = sum(GLYPHS[c][0] + 1.6 for c in line) * sx
        scale = min(1.0, width / adv)
        gs = sx * scale
        y = y0 + rng.uniform(0.0, 0.06) + li * 0.012  # jede Zeile etwas weiter eingerueckt (hastig)
        base = zline - cap * scale
        slope = rng.uniform(-0.025, 0.01)
        for c in line:
            w, strokes = GLYPHS[c]
            jit = rng.uniform(-0.004, 0.004)
            for st in strokes:
                pts = []
                for (gx, gy) in st:
                    pts.append(V((0.0, y + gx * gs + gy * gs * 0.12, base + jit + gy * gs + (y - y0) * slope)))
                pts = catmull(pts, 3) if len(pts) > 2 else [pts[0].lerp(pts[-1], t / 5.0) for t in range(6)]
                p2 = [(p.y + rng.uniform(-0.0012, 0.0012), p.z + rng.uniform(-0.0012, 0.0012)) for p in pts]
                n = len(p2)
                wbase = 0.0115 * (0.85 + 0.3 * scale)
                ws = [wbase * (1.15 - 0.45 * (i / max(1, n - 1)) + rng.uniform(-0.08, 0.08)) for i in range(n)]
                ribbon(bm, p2, ws, 0.0052)
                # Tropfnase: am Strichanfang oder an einem tiefen Punkt, wenn der Pinsel voll war
                if rng.random() < 0.32:
                    k = 0 if rng.random() < 0.5 else min(range(n), key=lambda i: p2[i][1])
                    dy, dz = p2[k]
                    ln = rng.uniform(0.02, 0.13)
                    drip = [(dy + rng.uniform(-0.001, 0.001), dz - ln * t / 6.0) for t in range(7)]
                    ribbon(bm, drip, [0.0042 * (1.0 - 0.5 * t / 6.0) for t in range(7)], 0.0053)
                    ribbon(bm, [(dy, dz - ln - 0.002), (dy, dz - ln - 0.008)], [0.0062, 0.0035], 0.0054)
            y += (w + 1.6) * gs
        zline -= line_h * (0.92 + 0.12 * rng.random())



# =====================================================================================================================
# Skin-Stealer
# =====================================================================================================================
def head_frame(f, up_hint):
    f = _n(f)
    up_hint = np.asarray(up_hint, dtype=np.float64)
    u = _n(up_hint - f * float(up_hint @ f))
    return f, u, np.cross(u, f)


def _stealer_parts():
    S = []
    # Becken in der Wand, Darmbeinkaemme stehen vor; der Bauch ist eingefallen, der Brustkorb kippt heraus
    S.append(ell((-0.10, 0.0, 0.98), (0.12, 0.18, 0.11), x=(0, 1, 0), up=(1, 0, 0)))
    for s in (-1, 1):
        S.append(ell((-0.005, s * 0.13, 1.03), (0.05, 0.03, 0.028), x=(0.2, s * 0.6, 0.6), up=(1, 0, 0), k=0.025))
    S.append(cone((-0.08, 0.0, 1.0), (-0.02, 0.0, 1.22), 0.105, 0.12, k=0.05))
    S.append(ell((0.0, 0.0, 1.37), (0.25, 0.19, 0.125), x=(0.28, 0, 1), up=(1, 0, 0), k=0.05))
    S.append(ell((0.085, 0.0, 1.13), (0.085, 0.11, 0.075), x=(0, 0, 1), up=(1, 0, 0), k=0.035, op='sub'))  # eingefallener Bauch
    S.append(cone((0.115, 0.0, 1.24), (0.112, 0.0, 1.52), 0.016, 0.018, k=0.02))  # Brustbein
    for s in (-1, 1):
        S.append(ell((0.03 if s > 0 else -0.05, s * 0.24, 1.56), (0.065, 0.06, 0.065), k=0.03))  # Schulter
        S.append(cone((0.11, s * 0.02, 1.61), (0.045, s * 0.21, 1.6), 0.017, 0.019, k=0.015))   # Schluesselbein
    # Hals mager, Sehnen gespannt; der Kopf in den Nacken gerissen, zum Gang gedreht
    S.append(cone((0.05, 0.0, 1.58), (0.10, 0.02, 1.73), 0.045, 0.04, k=0.03))
    f, u, side = head_frame((0.60, 0.32, 0.73), (-0.75, 0.0, 0.66))
    H = np.array([0.10, 0.035, 1.865])
    for s in (-1, 1):
        S.append(cone(H - u * 0.03 + side * s * 0.05 - f * 0.03, np.array([0.11, s * 0.035, 1.6]), 0.013, 0.016, k=0.012))
    S.append(ell(H, (0.105, 0.085, 0.12), x=f, up=u, k=0.03))
    S.append(cone(H + f * 0.075 + u * 0.05 - side * 0.058, H + f * 0.075 + u * 0.05 + side * 0.058, 0.026, k=0.016))  # Brauen
    for s in (-1, 1):
        S.append(ell(H + f * 0.065 - u * 0.012 + side * s * 0.062, (0.026, 0.018, 0.016), x=f, up=u, k=0.012))  # Jochbein
        S.append(cone(H - u * 0.04 + side * s * 0.064, H + f * 0.03 - u * 0.26 + side * s * 0.032, 0.021, 0.016, k=0.016))  # Kiefer
    chin = H + f * 0.035 - u * 0.275
    S.append(ell(chin, (0.024, 0.034, 0.02), x=f, up=u, k=0.016))
    S.append(ell(H + f * 0.105 - u * 0.025, (0.012, 0.016, 0.02), x=f, up=u, k=0.01))  # flache Nase
    mouth_c = H + f * 0.07 - u * 0.15
    S.append(ell(mouth_c, (0.052, 0.042, 0.115), x=f, up=u, k=0.012, op='sub'))  # ausgerenkter, aufgerissener Kiefer
    eyes = []
    for s in (-1, 1):
        ec = H + f * 0.095 + u * 0.022 + side * s * 0.036
        S.append(ell(ec, (0.032, 0.024, 0.02), x=f, up=u, k=0.008, op='sub'))  # tiefe Hoehle
        S.append(ell(ec - f * 0.02, (0.011, 0.011, 0.011), k=0.002))  # Auge weit hinten
        eyes.append(ec - f * 0.014)
    # linker Arm zieht sich an der Wand entlang, die Finger in die Fugen gekrallt (mager, Ellbogen spitz)
    Ls, Le, Lw = np.array([0.03, 0.24, 1.56]), np.array([0.12, 0.50, 1.66]), np.array([0.07, 0.78, 1.78])
    S += chain([Ls, Le, Lw], [0.042, 0.03, 0.024], k=0.025)
    S.append(ell((Ls + Le) / 2 + np.array([0.02, 0, 0]), (0.1, 0.034, 0.03), x=Le - Ls, up=(1, 0, 0), k=0.025))
    S.append(ell(Le + np.array([0.012, 0, -0.01]), (0.022, 0.022, 0.024), k=0.01))  # Ellbogen
    S.append(ell((Le + Lw) / 2 + np.array([0.01, 0, 0.004]), (0.1, 0.026, 0.022), x=Lw - Le, up=(1, 0, 0), k=0.018))
    hp, tips_l = hand(Lw, (0.05, 1.0, 0.3), (1, 0, 0), s=1.22, curl=(0.42, 0.65, 0.6), spread=0.34, flen=1.18)
    S += hp
    S += chain([np.array([-0.05, -0.245, 1.55]), np.array([-0.16, -0.33, 1.3])], [0.045, 0.036], k=0.03)  # rechte Schulter: in die Wand
    # die rechte Hand kommt an falscher Stelle tief unten aus der Wand (Noclip), greift ins Leere
    Rf0, Rw = np.array([-0.14, -0.70, 0.27]), np.array([0.15, -0.66, 0.50])
    S += chain([Rf0, Rw], [0.032, 0.025], k=0.02)
    S.append(ell((Rf0 + Rw) / 2 + np.array([0.0, 0.0, 0.008]), (0.12, 0.03, 0.026), x=Rw - Rf0, up=(0, 0, 1), k=0.018))
    hp, tips_r = hand(Rw, (0.45, 0.08, 1.0), (0.9, 0.0, -0.4), s=1.25, curl=(0.55, 0.8, 0.75), spread=0.48, flen=1.2)
    S += hp
    # linkes Bein: Knie und Fuss ragen heraus (mager)
    Lh, Lk, La = np.array([-0.07, 0.11, 0.95]), np.array([0.11, 0.155, 0.55]), np.array([0.0, 0.19, 0.09])
    S += chain([Lh, Lk, La], [0.065, 0.044, 0.03], k=0.025)
    S.append(ell(Lk + np.array([0.03, 0, 0]), (0.03, 0.032, 0.036), k=0.012))
    S.append(ell((Lk + La) / 2 + np.array([-0.01, 0, 0.04]), (0.12, 0.032, 0.036), x=La - Lk, up=(-1, 0, 0), k=0.025))
    S.append(cone(La, np.array([0.17, 0.215, 0.03]), 0.036, 0.028, k=0.022))
    for t in range(4):
        toe0 = np.array([0.165, 0.19 + t * 0.017, 0.028])
        S.append(cone(toe0, toe0 + np.array([0.045 - t * 0.005, 0.004, -0.014]), 0.01, 0.007, k=0.007))
    hot = [(e, 0.016) for e in eyes] + [(mouth_c - f * 0.05 - u * 0.02, 0.034)]
    return S, hot, tips_l + tips_r, (H, f, u, side, mouth_c)


def _stealer_disp(glaze, fusion):
    def disp(P):
        d1, d2 = sdf.worley(P, 30.0, 913)  # Noppen der Aussenhaut (entity-10: wie Saugnaepfe), erstarrt
        cup = 0.0011 * smoothstep(0.28, 0.05, d1) - 0.0006 * np.exp(-((d1 - 0.34) / 0.08) ** 2)
        # Rippen vorn: Boegen, die vom Brustbein nach aussen abfallen
        zr = P[:, 2] + 0.5 * np.abs(P[:, 1])
        m = smoothstep(0.025, 0.05, np.abs(P[:, 1])) * smoothstep(1.2, 1.27, P[:, 2]) * smoothstep(1.56, 1.48, zr) \
            * smoothstep(0.0, 0.06, P[:, 0])
        rib = -0.0055 * m * np.clip(np.cos(zr * TAU / 0.042), 0, 1) ** 1.5
        return cup + rib + glaze.depth * glaze.groove(P) + fusion.disp(P) + 0.0008 * sdf.fbm(P, 30.0, 2, 77)
    return disp


def build_purple_stealer():
    rng = random.Random(3301)
    M = mats()
    K = Kit()
    glaze = Glaze(41, freq=9.5, width=0.085, wall_bias=0.7)
    fusion = Fusion(reach=0.12)
    parts, hot, nails, (H, f, u, side, mouth_c) = _stealer_parts()
    disp = _stealer_disp(glaze, fusion)
    Vt, T = field_mesh(parts, 0.005, disp, tris=52000)

    def pores(C):  # die Noppen glimmen (Interpretation: die Energie sitzt in der Haut)
        d1, _ = sdf.worley(C, 30.0, 913)
        return (d1 < 0.2) & (C[:, 0] > 0.03) & (np.linalg.norm(C - H, axis=1) > 0.16)
    mi = classify_skin(parts, disp, Vt, T, glaze, hot, fusion=fusion, pores=pores)
    mesh_object('koerper', Vt, T, [M['skin'], M['skin_deep'], M['skin_sheen'], M['crack'], M['hot'], M['seam'], M['fleshbrick'],
                                   M['pore'], M['skin_mottle']], mi)
    # Zaehne im Schlund, Sehnenfaeden zwischen den Kiefern, Naegel an den langen Fingern
    tb = K(M['teeth'], 40.0)
    for jaw in (1, -1):
        for i in range(10):
            a = (i / 9.0 - 0.5) * 2.3
            base = mouth_c + u * (0.105 if jaw > 0 else -0.11) + side * math.sin(a) * 0.036 + f * (math.cos(a) * 0.04 - 0.014)
            tip = base - u * jaw * rng.uniform(0.014, 0.024) + f * 0.004
            sweep(tb, [V(base.tolist()), V(tip.tolist())], [0.0048, 0.0012], 6)
    sb = K(M['sinew'], 50.0)
    for k in range(4):
        sy = rng.uniform(-0.03, 0.03)
        top = mouth_c + u * 0.1 + side * sy + f * rng.uniform(0.0, 0.03)
        bot = mouth_c - u * 0.1 + side * (sy + rng.uniform(-0.01, 0.01)) + f * rng.uniform(0.0, 0.03)
        mid = (top + bot) / 2 + f * rng.uniform(0.01, 0.03) - u * 0.02
        sweep(sb, [V(top.tolist()), V(mid.tolist()), V(bot.tolist())], [0.0028, 0.0016, 0.0026], 5)
    nb = K(M['nail'], 40.0)
    for (p, d) in nails:
        p, d = np.asarray(p), _n(d)
        sweep(nb, [V((p - d * 0.012).tolist()), V((p + d * 0.006).tolist()), V((p + d * 0.022).tolist())], [0.0075, 0.0066, 0.002], 7)
    # Wand: Beruehrungslinie, violette Ziegel, Lichtbahnen, Brocken
    cpts = contact(parts, disp, -1.0, 1.15, 0.0, 2.3)
    center = (0.08, 1.15)
    stain(K, M, cpts, [(0.62, M['brick3'], 0.0014), (0.36, M['brick2'], 0.0022), (0.14, M['brick1'], 0.0030)], rng, 51, 2.55)
    lichtenberg(K, M, cpts, center, rng, n_roots=11, reach=(0.25, 0.7), zmax=2.5)
    pushed_bricks(K, M, cpts, center, rng, n=22)
    floor_debris(K, M, -0.75, 0.55, rng, n=9, xmax=0.36)
    K.finish()


# =====================================================================================================================
# Hound mit der gemalten Botschaft
# =====================================================================================================================
def _hound_parts():
    S = []
    # Brustkorb kriecht aus der Wand; abgemagert: Wirbel mit Dornfortsaetzen, Schulterblaetter, Rippen (Verschiebung)
    S.append(ell((0.0, 0.0, 0.58), (0.30, 0.165, 0.14), x=(1, 0, -0.2), up=(0, 0, 1)))
    S.append(ell((0.15, 0.0, 0.56), (0.15, 0.145, 0.125), x=(1, 0, -0.35), up=(0, 0, 1), k=0.05))
    for i in range(10):
        x = 0.25 - i * 0.042
        z = 0.735 + 0.025 * math.sin((x + 0.1) * 5.0)
        S.append(ell((x, 0.0, z), (0.019, 0.023, 0.022), k=0.012))
        S.append(cone((x, 0.0, z + 0.01), (x - 0.008, 0.0, z + 0.03), 0.009, 0.005, k=0.006))
    for s in (-1, 1):
        S.append(ell((0.08, s * 0.115, 0.71), (0.09, 0.055, 0.024), x=(1, s * 0.25, 0.25), up=(0.1, s * 0.45, 1), k=0.018))
        S.append(ell((0.16, s * 0.16, 0.63), (0.06, 0.055, 0.06), k=0.03))
    # langer, gestreckter Hals; der Kopf hebt sich und dreht sich zum Gang (zur Schrift), der Kiefer weit offen
    S.append(cone((0.20, 0.0, 0.67), (0.33, 0.07, 0.64), 0.052, 0.044, k=0.04))
    S.append(cone((0.33, 0.07, 0.64), (0.40, 0.11, 0.62), 0.044, 0.04, k=0.03))
    f, u, side = head_frame((0.62, 0.38, 0.40), (-0.2, -0.25, 1.0))
    H = np.array([0.45, 0.12, 0.63])
    S.append(ell(H, (0.092, 0.075, 0.1), x=f, up=u, k=0.03))
    S.append(cone(H + f * 0.068 + u * 0.042 - side * 0.05, H + f * 0.068 + u * 0.042 + side * 0.05, 0.02, k=0.014))
    for s in (-1, 1):
        S.append(ell(H + f * 0.05 - u * 0.01 + side * s * 0.056, (0.022, 0.019, 0.019), x=f, up=u, k=0.012))
        S.append(cone(H - u * 0.03 + side * s * 0.058, H + f * 0.05 - u * 0.19 + side * s * 0.036, 0.021, 0.016, k=0.016))
    chin = H + f * 0.055 - u * 0.2
    S.append(ell(chin, (0.022, 0.04, 0.02), x=f, up=u, k=0.016))
    mouth_c = H + f * 0.08 - u * 0.1
    S.append(ell(mouth_c, (0.05, 0.052, 0.085), x=f, up=u, k=0.01, op='sub'))  # Maul bis unter die Wangen
    eyes = []
    for s in (-1, 1):
        ec = H + f * 0.083 + u * 0.024 + side * s * 0.033
        S.append(ell(ec, (0.024, 0.02, 0.017), x=f, up=u, k=0.007, op='sub'))
        S.append(ell(ec - f * 0.013, (0.0105, 0.0105, 0.0105), k=0.002))
        eyes.append(ec - f * 0.009)
    # linker Arm vorn aufgestemmt (Ellbogen nach aussen wie ein kriechender Mensch), Hand flach, Finger gespreizt
    Ls, Le, Lw = np.array([0.17, 0.16, 0.62]), np.array([0.25, 0.33, 0.38]), np.array([0.44, 0.30, 0.065])
    S += chain([Ls, Le, Lw], [0.042, 0.03, 0.023], k=0.026)
    S.append(ell((Ls + Le) / 2, (0.11, 0.036, 0.032), x=Le - Ls, up=(0, 1, 0), k=0.022))
    S.append(ell(Le + np.array([0.0, 0.012, 0.0]), (0.02, 0.02, 0.02), k=0.01))
    S.append(ell((Le + Lw) / 2, (0.12, 0.028, 0.024), x=Lw - Le, up=(0, 1, 0), k=0.018))
    hp, tips = hand(Lw, (0.95, 0.12, -0.5), (0.3, 0, 1), s=1.1, curl=(0.25, 0.7, 0.7), spread=0.55, flen=1.12)
    S += hp
    # rechter Arm kommt tiefer aus der Wand und greift ueber den Boden nach vorn
    Rs, Re, Rw = np.array([0.08, -0.17, 0.58]), np.array([0.16, -0.33, 0.30]), np.array([0.36, -0.40, 0.06])
    S += chain([Rs, Re, Rw], [0.04, 0.03, 0.023], k=0.026)
    S.append(ell(Re + np.array([0.0, -0.012, 0.0]), (0.02, 0.02, 0.02), k=0.01))
    S.append(ell((Re + Rw) / 2, (0.11, 0.027, 0.023), x=Rw - Re, up=(0, -1, 0), k=0.018))
    hp, tips2 = hand(Rw, (0.9, -0.2, -0.45), (0.3, 0, 1), s=1.1, curl=(0.35, 0.85, 0.8), spread=0.45, flen=1.12)
    S += hp
    hot = [(e, 0.016) for e in eyes] + [(mouth_c - f * 0.045, 0.034)]
    return S, hot, tips + tips2, (H, f, u, side, mouth_c)


def _hound_disp(glaze, fusion):
    def disp(P):
        # Rippen: Wuelste quer an den Flanken des Brustkorbs (abgemagert, straffe Haut)
        flank = smoothstep(0.06, 0.12, np.abs(P[:, 1])) * smoothstep(0.72, 0.62, P[:, 2]) * smoothstep(0.40, 0.47, P[:, 2]) \
            * smoothstep(0.30, 0.22, P[:, 0]) * smoothstep(-0.10, -0.02, P[:, 0])
        rib = -0.007 * flank * np.clip(np.cos((P[:, 0] + 0.08 * np.abs(P[:, 1])) * TAU / 0.038), 0, 1) ** 1.5
        return rib + glaze.depth * glaze.groove(P) + fusion.disp(P) + 0.001 * sdf.fbm(P, 24.0, 3, 19)
    return disp


def hair(K, M, H, f, u, side, rng, clumps=18, per=(2, 4)):
    """Langes, schweres Haar, versteinert: in nassen Straehnen vom Hinterkopf, faellt bis auf den Beton und liegt dort
    ausgebreitet; die Straehnen loesen sich zu den Spitzen hin; das Gesicht bleibt frei."""
    bm = K(M['hair'], 50.0)
    for c in range(clumps):
        a = rng.uniform(-1.45, 1.45)
        b = rng.uniform(0.1, 1.35)  # 0 = Scheitel, > 0 nach hinten (weg vom Gesicht)
        dirv = _n(u * math.cos(b) - f * math.sin(b) + side * math.sin(a) * 0.9)
        root = H + dirv * 0.098
        n = rng.randint(*per)
        for s_ in range(n):
            p = root + np.array([rng.uniform(-0.01, 0.01) for _ in range(3)])
            vel = dirv * rng.uniform(0.02, 0.035)
            sway_ph, sway_a = rng.uniform(0, TAU), rng.uniform(0.0008, 0.0022)
            pool = np.array([math.cos(rng.uniform(0, TAU)), math.sin(rng.uniform(0, TAU)), 0.0]) * rng.uniform(0.008, 0.016)
            pts = [p.copy()]
            steps = rng.randint(24, 34)
            for k in range(steps):
                vel = vel * 0.72 + np.array([0, 0, -0.02]) + (side * math.sin(k * 0.45 + sway_ph) + f * math.cos(k * 0.3 + sway_ph)) * sway_a
                q = p + vel
                rel = q - H
                if np.linalg.norm(rel) < 0.104:
                    q = H + _n(rel) * 0.104
                if q[2] < 0.005:
                    q[2] = 0.005
                    vel = np.array([vel[0] * 0.3, vel[1] * 0.3, 0.0]) + pool
                q[0] = max(q[0], 0.02)
                pts.append(q)
                p = q
            r0 = rng.uniform(0.0034, 0.0056)
            rr = [r0 * (1.0 - 0.72 * (k / len(pts)) ** 1.4) for k in range(len(pts))]
            sweep(bm, [V(q.tolist()) for q in pts], rr, 6)


def build_purple_hound():
    rng = random.Random(3302)
    M = mats()
    K = Kit()
    glaze = Glaze(57, freq=10.5, width=0.085, wall_bias=0.75)
    fusion = Fusion(reach=0.12)
    parts, hot, tips, (H, f, u, side, mouth_c) = _hound_parts()
    disp = _hound_disp(glaze, fusion)
    Vt, T = field_mesh(parts, 0.0045, disp, tris=46000)
    mi = classify_skin(parts, disp, Vt, T, glaze, hot, fusion=fusion)
    mesh_object('koerper', Vt, T, [M['skin'], M['skin_deep'], M['skin_sheen'], M['crack'], M['hot'], M['seam'], M['fleshbrick'],
                                   M['pore'], M['skin_mottle']], mi)
    tb = K(M['teeth'], 40.0)
    for jaw in (1, -1):
        for i in range(13):
            a = (i / 12.0 - 0.5) * 2.7
            base = mouth_c + u * (0.072 if jaw > 0 else -0.078) + side * math.sin(a) * 0.048 + f * (math.cos(a) * 0.042 - 0.02)
            tip = base - u * jaw * rng.uniform(0.015, 0.026) + f * 0.003
            sweep(tb, [V(base.tolist()), V(tip.tolist())], [0.0054, 0.0012], 6)
    nb = K(M['nail'], 40.0)
    for (p, d) in tips:
        p, d = np.asarray(p), _n(d)
        sweep(nb, [V((p - d * 0.01).tolist()), V((p + d * 0.008).tolist()), V((p + d * 0.024 + np.array([0, 0, -0.006])).tolist())],
              [0.0075, 0.0065, 0.0018], 7)
    hair(K, M, H, f, u, side, rng)
    cpts = contact(parts, disp, -0.7, 0.6, 0.0, 1.0)
    center = (-0.02, 0.5)
    stain(K, M, cpts, [(0.58, M['brick3'], 0.0014), (0.34, M['brick2'], 0.0022), (0.13, M['brick1'], 0.0030)], rng, 63, 1.6)

    def avoid(p):  # keine Bahnen ueber die Schrift
        return p[0] > 0.72 and 0.95 < p[1] < 1.85
    lichtenberg(K, M, cpts, center, rng, n_roots=10, reach=(0.25, 0.65), zmax=1.9, avoid=avoid)
    pushed_bricks(K, M, cpts, center, rng, n=16)
    floor_debris(K, M, -0.5, 0.6, rng, n=8, xmax=0.5)
    paint_message(K, M, 0.82, 1.78, 2.05, rng)
    K.finish()
