"""V13.3 Level 903 ("The Metro", LEVELSPEC_903.md / LOOK_903.md) und der Metro-Wagen in Level 37 (LOOK_37.md) - neu gebaut.

Alles echte Geometrie: Wagenkasten aus Profil (Schuerze, Seitenwand mit Einzug, Dachrundung), Fenster mit gerundeten Ecken,
Gummidichtung, Kippfenster und Rahmen, Schiebetueren mit Fenster, Gummilippe und Griff, Drehgestelle (Radprofil mit Spurkranz,
Achslager, Federn, Rahmen, Luftfedern, Motoren, Bremsen, Schleifschuh an der Stromschiene), Kupplung, Innenraum (Laengsbaenke
mit Polstern, Haltestangen, Decke mit Lichtbaendern, Zielanzeigen mit Bildzeichen), Lackabnutzung und Schmutz ohne Graffiti.
Gleis mit Schotterbett, Spannbetonschwellen, Schienenprofil, Befestigungen, Stromschiene und Kabelkanal; Bahnsteigkante mit
Kantenstein und Noppenstreifen; ruhender Zug als Fassade (6-m-Abschnitte, Faltenbalg), ein bewohnter Wagen (Vorhaenge, Kisten,
Kabel, Pflanze, Schild); Leuchtroehren, Schilder in erfundener Schrift, Treppe zugemauert und mit Brettern vernagelt.

Konventionen (common.py): Meter, z oben, +x vorn. Im Spiel wird y gespiegelt (Blender +y = Spiel -y, links).
Texturen werden im Spiel aus Weltkoordinaten projiziert - UVs sind unnoetig; Abwechslung entsteht aus Geometrie und
mehreren Materialien (r = Rauheitsfaktor im Spiel, m = Metallzuschlag).
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

TAU = 2.0 * math.pi


# =====================================================================================================================
# Hilfen
# =====================================================================================================================
def _link(name, bm, mats):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for m in mats:
        me.materials.append(m)
    return ob


def _smooth(ob, deg=40.0):
    common.shade_auto(ob, deg)
    return ob


def _box(name, lo, hi, mat, bevel=0.0, seg=1):
    """Quader; die Ecken werden je Achse sortiert (gespiegelte Angaben ergeben keine umgestuelpten Normalen)."""
    a = tuple(min(lo[i], hi[i]) for i in range(3))
    b = tuple(max(lo[i], hi[i]) for i in range(3))
    return common.box(name, a, b, mat, bevel=bevel, segments=seg)


def _cyl(name, p0, p1, r, mat, seg=12, r1=None, caps=True, smooth=True):
    """Zylinder/Kegel von p0 nach p1."""
    p0, p1 = Vector(p0), Vector(p1)
    d = p1 - p0
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=caps, cap_tris=False, segments=seg, radius1=r, radius2=r if r1 is None else r1,
                          depth=d.length)
    q = Vector((0, 0, 1)).rotation_difference(d.normalized())
    bmesh.ops.transform(bm, matrix=Matrix.Translation((p0 + p1) / 2) @ q.to_matrix().to_4x4(), verts=bm.verts)
    ob = _link(name, bm, [mat])
    if smooth:
        _smooth(ob, max(40.0, 360.0 / seg + 6))
    return ob


def _tube(name, pts, r, mat, seg=8, caps=True, smooth=True):
    """Rohr entlang eines Polygonzugs (Gehrung an den Knicken)."""
    P = [Vector(p) for p in pts]
    n = len(P)
    bm = bmesh.new()
    rings, nrm = [], None
    for i in range(n):
        if i == 0:
            t = (P[1] - P[0]).normalized()
        elif i == n - 1:
            t = (P[-1] - P[-2]).normalized()
        else:
            t = ((P[i] - P[i - 1]).normalized() + (P[i + 1] - P[i]).normalized()).normalized()
        if nrm is None:
            a = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            nrm = t.cross(a).normalized()
        else:
            nrm = (nrm - t * nrm.dot(t)).normalized()
        b = t.cross(nrm)
        k, bd = 1.0, None
        if 0 < i < n - 1:
            d0, d1 = (P[i] - P[i - 1]).normalized(), (P[i + 1] - P[i]).normalized()
            k = 1.0 / max(0.45, d0.dot(t))
            bd = d1 - d0
            bd = bd - t * bd.dot(t)
            bd = bd.normalized() if bd.length > 1e-6 else None
        ring = []
        for j in range(seg):
            a = TAU * j / seg
            off = nrm * math.cos(a) + b * math.sin(a)
            if bd is not None:
                off = off + bd * off.dot(bd) * (k - 1.0)
            ring.append(bm.verts.new(P[i] + off * r))
        rings.append(ring)
    for i in range(n - 1):
        for j in range(seg):
            j2 = (j + 1) % seg
            bm.faces.new((rings[i][j], rings[i][j2], rings[i + 1][j2], rings[i + 1][j]))
    if caps:
        bm.faces.new(rings[0])
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = _link(name, bm, [mat])
    if smooth:
        _smooth(ob, 60)
    return ob


def _lathe(name, prof, mats, seg=16, center=(0, 0, 0), axis='z', seg_mat=None, smooth=True, closed=False, sharp=35.0):
    """Drehkoerper: prof = [(r, h)] entlang der Achse. Sichtbar ist die Seite rechts der Laufrichtung des Profils
    (Normale (dh, -dr)). closed: Profil ist eine geschlossene Schleife."""
    C = Vector(center)

    def P(r, h, a):
        u, v = r * math.cos(a), r * math.sin(a)
        if axis == 'z':
            return C + Vector((u, v, h))
        if axis == 'y':
            return C + Vector((u, h, v))
        return C + Vector((h, u, v))

    def A(h):
        return Vector((0, 0, 1)) if axis == 'z' else (Vector((0, 1, 0)) if axis == 'y' else Vector((1, 0, 0)))

    bm = bmesh.new()
    rings = []
    for (r, h) in prof:
        if r < 1e-6:
            rings.append([bm.verts.new(P(0, h, 0))])
        else:
            rings.append([bm.verts.new(P(r, h, TAU * j / seg)) for j in range(seg)])
    ax = A(0)
    m = len(prof) if closed else len(prof) - 1
    for i in range(m):
        i2 = (i + 1) % len(prof)
        (r0, h0), (r1, h1) = prof[i], prof[i2]
        nr, nh = (h1 - h0), -(r1 - r0)
        for j in range(seg):
            j2 = (j + 1) % seg
            ra, rb = rings[i], rings[i2]
            vs = []
            for v in (ra[j % len(ra)], ra[j2 % len(ra)], rb[j2 % len(rb)], rb[j % len(rb)]):
                if v not in vs:
                    vs.append(v)
            if len(vs) < 3:
                continue
            f = bm.faces.new(vs)
            f.normal_update()
            mid = sum((v.co for v in vs), Vector()) / len(vs)
            radial = mid - C
            radial = radial - ax * radial.dot(ax)
            radial = radial.normalized() if radial.length > 1e-9 else Vector()
            want = radial * nr + ax * nh
            if f.normal.dot(want) < 0:
                f.normal_flip()
            if seg_mat:
                f.material_index = seg_mat[i]
    ob = _link(name, bm, mats if isinstance(mats, list) else [mats])
    if smooth:
        _smooth(ob, sharp)
    return ob


def _prism(name, poly, a0, a1, mats, axis='x', edge_mat=None, cap_mat=(0, 0), smooth=None):
    """Polygon (u, v) entlang einer Achse von a0 nach a1 ziehen. axis 'x': (u, v) = (y, z); 'y': (x, z); 'z': (x, y)."""
    def P(u, v, a):
        if axis == 'x':
            return (a, u, v)
        if axis == 'y':
            return (u, a, v)
        return (u, v, a)
    clean = []
    for p in poly:
        if not clean or (abs(p[0] - clean[-1][0]) > 1e-7 or abs(p[1] - clean[-1][1]) > 1e-7):
            clean.append(p)
    if len(clean) > 2 and abs(clean[0][0] - clean[-1][0]) < 1e-7 and abs(clean[0][1] - clean[-1][1]) < 1e-7:
        clean.pop()
    poly = clean
    if len(poly) < 3:
        return None
    bm = bmesh.new()
    r0 = [bm.verts.new(P(u, v, a0)) for (u, v) in poly]
    r1 = [bm.verts.new(P(u, v, a1)) for (u, v) in poly]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        f = bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
        f.material_index = edge_mat[i] if edge_mat else 0
    f0 = bm.faces.new(r0)
    f0.material_index = cap_mat[0]
    f1 = bm.faces.new(list(reversed(r1)))
    f1.material_index = cap_mat[1]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = _link(name, bm, mats if isinstance(mats, list) else [mats])
    if smooth:
        _smooth(ob, smooth)
    return ob


def _loft(name, rings, mats, caps=True, smooth=None, ring_mat=None):
    """Ringe gleicher Punktzahl (Listen von 3D-Punkten) verbinden."""
    bm = bmesh.new()
    R = [[bm.verts.new(Vector(p)) for p in ring] for ring in rings]
    n = len(R[0])
    for i in range(len(R) - 1):
        for j in range(n):
            j2 = (j + 1) % n
            f = bm.faces.new((R[i][j], R[i][j2], R[i + 1][j2], R[i + 1][j]))
            if ring_mat:
                f.material_index = ring_mat[i]
    if caps:
        bm.faces.new(R[0])
        bm.faces.new(list(reversed(R[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = _link(name, bm, mats if isinstance(mats, list) else [mats])
    if smooth:
        _smooth(ob, smooth)
    return ob


def _area(pts):
    return 0.5 * sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))


def _decal(name, origin, u, v, pts, mat, smooth=False):
    """Flaches Polygon (Fleck, Aufkleber, Zeichen) in der Ebene origin + a*u + b*v; Normale u x v."""
    if _area(pts) < 0:
        pts = list(reversed(pts))
    o, U, V = Vector(origin), Vector(u), Vector(v)
    bm = bmesh.new()
    vs = [bm.verts.new(o + U * a + V * b) for (a, b) in pts]
    f = bm.faces.new(vs)
    f.normal_update()
    if f.normal.dot(U.cross(V)) < 0:
        f.normal_flip()
    return _link(name, bm, [mat])


def _blob(rng, rx, ry, n=9, irr=0.35, cx=0.0, cy=0.0):
    pts = []
    for k in range(n):
        a = TAU * (k + rng.uniform(-0.25, 0.25)) / n
        f = 1.0 + rng.uniform(-irr, irr)
        pts.append((cx + math.cos(a) * rx * f, cy + math.sin(a) * ry * f))
    return pts


def _ribbon(pts, w):
    """Polygon eines Strichs der Breite w entlang eines 2D-Polygonzugs (Gehrung)."""
    n = len(pts)
    left, right = [], []
    for i in range(n):
        if i == 0:
            dx, dy = pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]
        elif i == n - 1:
            dx, dy = pts[-1][0] - pts[-2][0], pts[-1][1] - pts[-2][1]
        else:
            ax, ay = pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]
            bx, by = pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]
            la, lb = math.hypot(ax, ay) or 1, math.hypot(bx, by) or 1
            dx, dy = ax / la + bx / lb, ay / la + by / lb
        L = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / L, dx / L
        k = 1.0
        if 0 < i < n - 1:
            ax, ay = pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]
            la = math.hypot(ax, ay) or 1
            k = 1.0 / max(0.5, (ax / la) * (dx / L) + (ay / la) * (dy / L))
        left.append((pts[i][0] + nx * w * 0.5 * k, pts[i][1] + ny * w * 0.5 * k))
        right.append((pts[i][0] - nx * w * 0.5 * k, pts[i][1] - ny * w * 0.5 * k))
    return left + list(reversed(right))


def _jitter(ob, amp, seed, axes=(1, 1, 1)):
    rng = random.Random(seed)
    for v in ob.data.vertices:
        v.co.x += rng.uniform(-amp, amp) * axes[0]
        v.co.y += rng.uniform(-amp, amp) * axes[1]
        v.co.z += rng.uniform(-amp, amp) * axes[2]


def _xform(objs, M):
    for ob in objs:
        ob.matrix_world = M @ ob.matrix_world


def _objs_since(before):
    return [ob for ob in bpy.context.scene.objects if ob.name not in before]


def _names():
    return {ob.name for ob in bpy.context.scene.objects}


def _clip_rect(poly, x0, x1, y0, y1):
    """Sutherland-Hodgman: Polygon (u, v) auf das Rechteck [x0, x1] x [y0, y1] beschneiden."""
    def clip(pts, inside, inter):
        out = []
        for i in range(len(pts)):
            a, b = pts[i - 1], pts[i]
            ia, ib = inside(a), inside(b)
            if ib:
                if not ia:
                    out.append(inter(a, b))
                out.append(b)
            elif ia:
                out.append(inter(a, b))
        return out

    def ix(c):
        return lambda a, b: (c, a[1] + (b[1] - a[1]) * (c - a[0]) / (b[0] - a[0]))

    def iy(c):
        return lambda a, b: (a[0] + (b[0] - a[0]) * (c - a[1]) / (b[1] - a[1]), c)
    p = poly
    for inside, inter in ((lambda q: q[0] >= x0, ix(x0)), (lambda q: q[0] <= x1, ix(x1)),
                          (lambda q: q[1] >= y0, iy(y0)), (lambda q: q[1] <= y1, iy(y1))):
        if not p:
            return []
        p = clip(p, inside, inter)
    return p


def _rrect(x0, x1, y0, y1, r, n=4):
    """Rechteck mit gerundeten Ecken (gegen den Uhrzeigersinn)."""
    pts = []
    for (cx, cy, a0) in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for k in range(n + 1):
            a = math.radians(a0 + 90.0 * k / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


# =====================================================================================================================
# Materialien
# =====================================================================================================================
def _M():
    mt = common.material
    return {
        # Wagenkasten
        'alu': mt('wagen_alu', (0.74, 0.76, 0.78), look='metal', tex='metal_aluminum', rough=0.35, metal=0.9, r=0.75, m=0.75),
        'alu_dull': mt('wagen_alu_matt', (0.55, 0.57, 0.58), look='metal', tex='metal_aluminum', rough=0.55, metal=0.8, r=1.2, m=0.55),
        'alu_old': mt('wagen_alt', (0.50, 0.53, 0.55), look='metal', tex='painted_metal_grey', rough=0.6, metal=0.3, r=1.0, m=0.25),
        'paint_old': mt('wagen_alt_lack', (0.34, 0.355, 0.365), look='paint', rough=0.6, r=1.0),
        'red': mt('wagen_rot', (0.62, 0.08, 0.06), look='carpaint', rough=0.45, r=0.8),
        'blue': mt('streifen_blau', (0.10, 0.22, 0.52), look='paint', tex='painted_metal_grey', rough=0.55, r=1.0),
        'blue_faded': mt('streifen_blau_blass', (0.16, 0.27, 0.50), look='paint', rough=0.7, r=1.1),
        'rubber': mt('dichtung', (0.035, 0.035, 0.037), look='rubber', rough=0.85, r=1.0),
        'glass': mt('scheibe', (0.045, 0.05, 0.055), look='glass', rough=0.05, r=0.06),
        'glass_dirty': mt('scheibe_staub', (0.12, 0.12, 0.115), look='glass', rough=0.3, r=0.45),
        'inner': mt('innen_creme', (0.83, 0.79, 0.69), look='paint', rough=0.6, r=0.85),
        'inner_grey': mt('innen_grau', (0.50, 0.51, 0.51), look='plastic', rough=0.5, r=0.9),
        'floor': mt('wagenboden', (0.16, 0.16, 0.165), look='rubber', rough=0.8, r=1.1),
        'floor_worn': mt('wagenboden_abgelaufen', (0.24, 0.235, 0.23), look='rubber', rough=0.7, r=0.9),
        'seat': mt('sitz_blau', (0.17, 0.24, 0.46), look='fabric', rough=0.95, r=1.15),
        'seat_worn': mt('sitz_blau_glatt', (0.21, 0.26, 0.40), look='fabric', rough=0.8, r=0.85),
        'shell': mt('sitzschale', (0.30, 0.32, 0.35), look='plastic', rough=0.6, r=0.9),
        'steel': mt('edelstahl', (0.80, 0.80, 0.80), look='metal', rough=0.25, metal=1.0, r=0.5, m=0.9),
        'chrome_worn': mt('edelstahl_matt', (0.62, 0.62, 0.62), look='metal', rough=0.45, metal=1.0, r=0.9, m=0.8),
        'light': mt('lichtband', (1.0, 0.97, 0.90), look='glow', glow=0.55),
        'disp_bg': mt('anzeige_schwarz', (0.018, 0.018, 0.02), look='plastic', rough=0.3, r=0.4),
        'disp': mt('anzeige_weiss', (0.95, 0.98, 1.0), look='glow', glow=1.6),
        'disp_amber': mt('anzeige_bernstein', (1.0, 0.62, 0.18), look='glow', glow=1.2),
        'under': mt('unterboden', (0.10, 0.10, 0.10), look='paint', tex='painted_metal_grey', rough=0.8, r=1.25),
        'under_brown': mt('unterboden_rost', (0.16, 0.12, 0.09), look='metal', tex='metal_rusty_iron', rough=0.8, metal=0.3, r=1.2, m=0.2),
        'bogie': mt('drehgestell', (0.17, 0.155, 0.14), look='metal', tex='metal_rusty_iron', rough=0.7, metal=0.4, r=1.15, m=0.3),
        'wheel': mt('rad', (0.36, 0.33, 0.30), look='metal', tex='metal_rusty_iron', rough=0.6, metal=0.6, r=1.0, m=0.5),
        'tread': mt('laufflaeche', (0.78, 0.78, 0.77), look='metal', rough=0.2, metal=1.0, r=0.4, m=0.9),
        'spring': mt('feder', (0.22, 0.20, 0.18), look='metal', tex='metal_rusty_iron', rough=0.6, metal=0.5, r=1.0, m=0.4),
        'grime': mt('schmutz_braun', (0.21, 0.17, 0.13), look='paint', rough=0.9, r=1.35),
        'grime_dark': mt('russ', (0.075, 0.068, 0.062), look='paint', rough=0.9, r=1.3),
        'dust': mt('staub', (0.42, 0.40, 0.37), look='concrete', rough=0.95, r=1.4),
        'yellow': mt('gelb', (0.86, 0.66, 0.08), look='paint', rough=0.6, r=0.9),
        'yellow_worn': mt('gelb_abgenutzt', (0.62, 0.52, 0.22), look='paint', rough=0.8, r=1.2),
        'tail': mt('schlusslicht', (0.95, 0.10, 0.06), look='glow', glow=0.9),
        'head': mt('scheinwerfer', (1.0, 0.94, 0.80), look='glow', glow=0.35),
        'shoe_wood': mt('isolierholz', (0.32, 0.20, 0.11), look='wood', rough=0.8, r=1.0),
        'red_handle': mt('notgriff_rot', (0.70, 0.06, 0.04), look='plastic', rough=0.5, r=0.7),
        'sticker_w': mt('aufkleber_weiss', (0.86, 0.85, 0.80), look='paint', rough=0.6, r=0.9),
        'sticker_y': mt('aufkleber_gelb', (0.90, 0.72, 0.12), look='paint', rough=0.6, r=0.9),
        'sticker_b': mt('aufkleber_blau', (0.16, 0.34, 0.62), look='paint', rough=0.6, r=0.9),
        'sticker_r': mt('aufkleber_rot', (0.66, 0.12, 0.10), look='paint', rough=0.6, r=0.9),
        'ink': mt('schrift_dunkel', (0.05, 0.05, 0.055), look='paint', rough=0.6, r=0.9),
        # Gleis
        'ballast': mt('schotter', (0.46, 0.43, 0.40), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'ballast_dark': mt('schotter_dunkel', (0.30, 0.28, 0.26), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'ballast_rust': mt('schotter_rost', (0.40, 0.30, 0.22), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'stone_a': mt('stein_grau', (0.50, 0.49, 0.47), look='concrete', tex='concrete_chalky', rough=0.9, r=1.3),
        'stone_b': mt('stein_dunkel', (0.33, 0.32, 0.31), look='concrete', tex='concrete_rough', rough=0.9, r=1.3),
        'sleeper': mt('schwelle', (0.58, 0.57, 0.54), look='concrete', tex='concrete_cast', rough=0.9, r=1.25),
        'rail': mt('schiene', (0.42, 0.33, 0.27), look='metal', tex='metal_rusty_iron', rough=0.7, metal=0.6, r=1.1, m=0.4),
        'railhead': mt('schienenkopf', (0.80, 0.80, 0.79), look='metal', rough=0.2, metal=1.0, r=0.35, m=0.95),
        'clip': mt('spannklemme', (0.20, 0.19, 0.18), look='metal', tex='metal_rusty_iron', rough=0.6, metal=0.6, r=1.0, m=0.5),
        'plate': mt('rippenplatte', (0.30, 0.25, 0.21), look='metal', tex='metal_rusty_iron', rough=0.7, metal=0.5, r=1.1, m=0.4),
        'conductor': mt('stromschiene', (0.55, 0.55, 0.56), look='metal', tex='metal_aluminum', rough=0.4, metal=0.9, r=0.8, m=0.7),
        'cover': mt('stromschiene_abdeckung', (0.56, 0.48, 0.22), look='plastic', rough=0.8, r=1.3),
        'insulator': mt('isolator', (0.88, 0.86, 0.80), look='ceramic', rough=0.3, r=0.5),
        'trough': mt('kabelkanal', (0.55, 0.54, 0.51), look='concrete', tex='concrete_wall_old', rough=0.9, r=1.3),
        'cable': mt('kabel', (0.05, 0.05, 0.05), look='rubber', rough=0.7, r=0.9),
        # Bahnsteig, Licht, Schilder, Treppe
        'coping': mt('kantenstein', (0.70, 0.69, 0.66), look='concrete', tex='concrete_cast', rough=0.85, r=1.1),
        'tactile': mt('noppenplatte', (0.86, 0.67, 0.10), look='plastic', rough=0.7, r=1.15),
        'tactile_worn': mt('noppenplatte_matt', (0.68, 0.55, 0.20), look='plastic', rough=0.8, r=1.3),
        'joint': mt('fuge', (0.12, 0.115, 0.11), look='concrete', rough=0.95, r=1.4),
        'tube': mt('roehre', (0.95, 1.0, 1.0), look='glow', glow=2.2),
        'tube_end': mt('roehre_ende', (0.30, 0.30, 0.29), look='glass', rough=0.3, r=0.5),
        'housing': mt('leuchte_alu', (0.82, 0.83, 0.84), look='metal', tex='painted_metal_white', rough=0.4, metal=0.3, r=0.9, m=0.2),
        'socket': mt('fassung', (0.86, 0.85, 0.82), look='plastic', rough=0.5, r=0.8),
        'sign_glow': mt('schild_licht', (0.98, 0.95, 0.78), look='glow', glow=1.1),
        'sign_case': mt('schild_gehaeuse', (0.16, 0.17, 0.18), look='metal', tex='painted_metal_grey', rough=0.5, metal=0.3, r=0.9, m=0.25),
        'enamel': mt('emaille_blau', (0.07, 0.15, 0.36), look='paint', rough=0.25, r=0.45),
        'enamel_w': mt('emaille_weiss', (0.93, 0.93, 0.90), look='paint', rough=0.25, r=0.45),
        'enamel_chip': mt('emaille_abplatzer', (0.09, 0.08, 0.075), look='metal', tex='metal_rusty_iron', rough=0.7, metal=0.5, r=1.1),
        'rust': mt('rost', (0.36, 0.20, 0.10), look='metal', tex='metal_rusty_iron', rough=0.8, metal=0.3, r=1.3, m=0.1),
        'rust_streak': mt('roststreifen', (0.33, 0.21, 0.13), look='paint', rough=0.9, r=1.3),
        'tile_w': mt('treppe_fliese', (0.88, 0.87, 0.84), look='ceramic', tex='tiles_white', rough=0.3, r=0.5),
        'step': mt('stufe', (0.56, 0.55, 0.53), look='concrete', tex='terrazzo_antique', rough=0.7, r=0.9),
        'step_dirty': mt('stufe_staub', (0.40, 0.38, 0.35), look='concrete', tex='concrete_rough', rough=0.95, r=1.4),
        'brick': mt('ziegel_grau', (0.82, 0.80, 0.78), look='concrete', tex='bricks_grey_black', rough=0.9, r=1.2),
        'mortar': mt('moertel', (0.62, 0.60, 0.56), look='concrete', tex='concrete_chalky', rough=0.95, r=1.4),
        'wood_a': mt('brett_hell', (0.56, 0.50, 0.42), look='wood', tex='wood_birch', rough=0.8, r=1.2),
        'wood_b': mt('brett_alt', (0.42, 0.38, 0.33), look='wood', tex='wood_birch', rough=0.85, r=1.3),
        'wood_c': mt('brett_dunkel', (0.34, 0.28, 0.22), look='wood', tex='wood_walnut_floor', rough=0.85, r=1.3),
        'brake_dust': mt('bremsstaub', (0.19, 0.18, 0.17), look='paint', rough=0.9, r=1.35),
        'wood_end': mt('hirnholz', (0.55, 0.42, 0.28), look='wood', rough=0.9, r=1.3),
        'nail': mt('nagel', (0.30, 0.29, 0.28), look='metal', rough=0.5, metal=0.8, r=0.9, m=0.6),
        'paper': mt('papier', (0.86, 0.84, 0.76), look='paint', rough=0.9, r=1.3),
        'tape': mt('klebeband_rotweiss', (0.80, 0.18, 0.12), look='plastic', rough=0.5, r=0.8),
    }


# =====================================================================================================================
# Erfundene Schrift (keine echte Schrift, LOOK_903 "Nicht") und Bildzeichen
# =====================================================================================================================
def _glyph_table(n=18, seed=9031):
    """Zeichensatz aus Strichen: ein Stamm (senkrecht, Bogen oder Haken) und ein bis drei Anhaenge (Querstrich, Schraege,
    Punkt, Haken). Koordinaten im Einheitskasten (0..1, 0..1)."""
    rng = random.Random(seed)
    table = []
    for _ in range(n):
        strokes = []
        kind = rng.choice(('stem', 'arc', 'hook', 'stem', 'bow'))
        sx = rng.choice((0.3, 0.5, 0.7))
        if kind == 'stem':
            strokes.append([(sx, 0.05), (sx, 0.95)])
        elif kind == 'arc':
            c, r = (0.5, 0.55), 0.38
            a0 = rng.choice((200, 160, -20))
            strokes.append([(c[0] + r * math.cos(math.radians(a0 + 25 * k)), c[1] + r * math.sin(math.radians(a0 + 25 * k)))
                            for k in range(8)])
        elif kind == 'bow':
            strokes.append([(0.15, 0.9), (0.45, 0.95), (0.7, 0.75), (0.65, 0.45), (0.35, 0.3), (0.3, 0.05)])
        else:
            strokes.append([(sx, 0.95), (sx, 0.2), (sx + 0.12, 0.05), (sx + 0.35, 0.1)])
        for _ in range(rng.randint(1, 3)):
            a = rng.choice(('bar', 'diag', 'dot', 'tick', 'hook2'))
            h = rng.uniform(0.25, 0.85)
            if a == 'bar':
                strokes.append([(rng.uniform(0.05, 0.3), h), (rng.uniform(0.6, 0.95), h + rng.uniform(-0.08, 0.08))])
            elif a == 'diag':
                strokes.append([(rng.uniform(0.1, 0.4), rng.uniform(0.05, 0.4)), (rng.uniform(0.6, 0.9), rng.uniform(0.6, 0.95))])
            elif a == 'dot':
                cx, cy = rng.uniform(0.15, 0.85), rng.choice((0.1, 0.92))
                strokes.append([(cx - 0.045, cy), (cx + 0.045, cy)])
            elif a == 'tick':
                x = rng.uniform(0.15, 0.85)
                strokes.append([(x, h), (x + rng.choice((-0.2, 0.2)), h + 0.18)])
            else:
                x = rng.uniform(0.2, 0.6)
                strokes.append([(x, h), (x + 0.3, h), (x + 0.3, h - 0.25)])
        table.append(strokes)
    return table


GLYPHS = _glyph_table()


def _text(name, word, origin, u, v, height, mat, width=None, stroke=0.13, gap=0.25, seed=0):
    """Zeichenfolge (Liste von Zeichen-Indizes) als flache Striche ab origin, Lauf entlang u, Hoehe entlang v."""
    o, U, V = Vector(origin), Vector(u).normalized(), Vector(v).normalized()
    w = height * 0.82
    n = 0
    for k, g in enumerate(word):
        x0 = k * (w + w * gap)
        for s, st in enumerate(GLYPHS[g % len(GLYPHS)]):
            pts = [(x0 + px * w, py * height) for (px, py) in st]
            _decal(f'{name}_{k}_{s}', o, U, V, _ribbon(pts, stroke * height), mat)
            n += 1
    return n


def _word(rng, a, b):
    return [rng.randrange(len(GLYPHS)) for _ in range(rng.randint(a, b))]


def _pictogram(name, kind, origin, u, v, size, mat, dark):
    """Bildzeichen (sprachfrei), Mitte in origin, Breite ~ size: 'wave' Wellen ueber einem Becken (Level 37),
    'plane' Flugzeug (Level 36), 'train' Wagen von vorn (zurueck zur Metro)."""
    o, U, V = Vector(origin), Vector(u).normalized(), Vector(v).normalized()
    s = size

    def poly(pts, m=mat, lift=0.0, nm=''):
        return _decal(f'{name}{nm}', o + U.cross(V) * lift, U, V, [(x * s, y * s) for (x, y) in pts], m)
    if kind == 'wave':
        for r in range(2):
            y0 = 0.10 - r * 0.17
            top = [(-0.42 + 0.84 * t, y0 + 0.055 * math.sin(t * 3.0 * TAU)) for t in [k / 30 for k in range(31)]]
            poly(top + [(x, y - 0.065) for (x, y) in reversed(top)], nm=f'_welle{r}')
        poly([(-0.46, -0.36), (0.46, -0.36), (0.46, -0.30), (-0.46, -0.30)], nm='_becken')
        poly([(-0.46, -0.36), (-0.40, -0.36), (-0.40, -0.12), (-0.46, -0.12)], nm='_rand_l')
        poly([(0.40, -0.36), (0.46, -0.36), (0.46, -0.12), (0.40, -0.12)], nm='_rand_r')
        for k in range(3):  # Leiter
            poly([(0.28, -0.28 + k * 0.08), (0.36, -0.28 + k * 0.08), (0.36, -0.25 + k * 0.08), (0.28, -0.25 + k * 0.08)], nm=f'_sprosse{k}')
    elif kind == 'plane':
        body = [(-0.46, -0.02), (-0.30, -0.05), (0.32, -0.05), (0.42, -0.035), (0.48, 0.0), (0.42, 0.035), (0.32, 0.05),
                (-0.30, 0.05), (-0.40, 0.04)]
        poly(body, nm='_rumpf')
        poly([(0.02, 0.04), (0.14, 0.04), (-0.08, 0.36), (-0.16, 0.36)], nm='_fluegel_o')
        poly([(-0.16, -0.36), (-0.08, -0.36), (0.14, -0.04), (0.02, -0.04)], nm='_fluegel_u')
        poly([(-0.44, 0.03), (-0.36, 0.03), (-0.42, 0.20), (-0.48, 0.20)], nm='_leitwerk')
        poly([(-0.46, -0.02), (-0.38, -0.02), (-0.44, -0.11), (-0.49, -0.11)], nm='_hoehe')
    else:  # train: Wagen von vorn
        poly(_rrect(-0.30, 0.30, -0.30, 0.38, 0.08), nm='_kasten')
        for wx in (-0.24, 0.04):
            poly([(wx, 0.06), (wx + 0.20, 0.06), (wx + 0.20, 0.28), (wx, 0.28)], m=dark, lift=0.002, nm=f'_fenster{wx}')
        for wx in (-0.18, 0.18):
            pts = [(wx + 0.05 * math.cos(TAU * k / 12), -0.16 + 0.05 * math.sin(TAU * k / 12)) for k in range(12)]
            poly(pts, m=dark, lift=0.002, nm=f'_licht{wx}')
        poly([(-0.36, -0.42), (0.36, -0.42), (0.36, -0.37), (-0.36, -0.37)], nm='_schiene')


# =====================================================================================================================
# Gleis (track_metro und unter dem Wagen)
# =====================================================================================================================
GAUGE = 1.435
RAIL_PROFILE = [  # UIC-54-aehnlich (u quer, z hoch), Fuss 0.14, Steg 0.016, Kopf 0.07, Hoehe 0.159
    (-0.070, 0.000), (0.070, 0.000), (0.070, 0.010), (0.030, 0.022), (0.009, 0.030), (0.008, 0.110), (0.020, 0.118),
    (0.035, 0.124), (0.036, 0.152), (0.030, 0.159), (-0.030, 0.159), (-0.036, 0.152), (-0.035, 0.124), (-0.020, 0.118),
    (-0.008, 0.110), (-0.009, 0.030), (-0.030, 0.022), (-0.070, 0.010)]
Z_BALLAST, Z_SLEEPER, Z_RAIL0 = 0.115, 0.215, 0.225  # Weltnull = Gleisbett-Boden; Schienenoberkante 0.384


def _rail(name, x0, x1, yc, z0, M):
    """Schiene von x0 nach x1 (Kopf blank, Steg und Fuss rostig)."""
    poly = [(yc + u, z0 + v) for (u, v) in RAIL_PROFILE]
    edge = [0] * len(poly)
    for i in (8, 9, 10):  # Kopfflaechen oben
        edge[i] = 1
    _prism(name, poly, x0, x1, [M['rail'], M['railhead']], axis='x', edge_mat=edge, smooth=30)


def _sleeper(name, xc, yc, zb, M, rng):
    """Spannbetonschwelle (B70-aehnlich): unter den Schienen breiter und hoeher, in der Mitte eingeschnuert."""
    rings = []
    for (y, w, h) in ((-1.30, 0.24, 0.165), (-1.0, 0.26, 0.18), (-0.42, 0.24, 0.18), (-0.30, 0.20, 0.14), (0.30, 0.20, 0.14),
                      (0.42, 0.24, 0.18), (1.0, 0.26, 0.18), (1.30, 0.24, 0.165)):
        wt = w - 0.06
        rings.append([(xc - w / 2, yc + y, zb), (xc + w / 2, yc + y, zb), (xc + wt / 2, yc + y, zb + h), (xc - wt / 2, yc + y, zb + h)])
    ob = _loft(name, rings, [M['sleeper']])
    _jitter(ob, 0.004, rng.randrange(10 ** 6))
    return ob


def _fastening(name, xc, yr, z, M, side, simple=False):
    """Schienenbefestigung an einem Schienenauflager: Rippenplatte, zwei Spannklemmen (W-Form), zwei Schwellenschrauben."""
    _box(f'{name}_platte', (xc - 0.09, yr - 0.11, z - 0.012), (xc + 0.09, yr + 0.11, z), M['plate'])
    if simple:
        return
    for s in (-1, 1):
        yb = yr + s * 0.085
        # W-Klemme: Federbuegel als Rohr (zwei Schlaufen auf den Schienenfuss)
        pts = [(xc - 0.05, yb + s * 0.02, z + 0.012), (xc - 0.045, yb - s * 0.03, z + 0.03), (xc - 0.02, yb - s * 0.05, z + 0.028),
               (xc, yb - s * 0.02, z + 0.04), (xc + 0.02, yb - s * 0.05, z + 0.028), (xc + 0.045, yb - s * 0.03, z + 0.03),
               (xc + 0.05, yb + s * 0.02, z + 0.012)]
        _tube(f'{name}_klemme{s}', pts, 0.0065, M['clip'], seg=4, caps=False)
        _cyl(f'{name}_schraube{s}', (xc, yb + s * 0.005, z), (xc, yb + s * 0.005, z + 0.05), 0.016, M['clip'], seg=6, caps=False)
        _cyl(f'{name}_kopf{s}', (xc, yb + s * 0.005, z + 0.05), (xc, yb + s * 0.005, z + 0.052), 0.016, M['clip'], seg=6)


def _track(prefix, x0, x1, yc, ylo, yhi, zoff, M, seed, sleeper_x0, detail=True, bed=True, xs_simple=None, cell=0.14, stones=4.2,
           hidden=None):
    """Gleis von x0 bis x1 (Gleismitte yc), Bett von ylo bis yhi (Wand der Halle bei yhi, Bahnsteigwand bei ylo);
    zoff: Hoehe des Hallenbodens. sleeper_x0: erste Schwelle (Raster 0.6 m, Stoss auf Stoss mit den Nachbarstuecken).
    xs_simple(x): Schwellen unter dem Wagen einfacher; hidden(x, y): dort keine Einzelsteine (unsichtbar)."""
    rng = random.Random(seed)
    z = zoff
    # --- Schotterbett: Hoehenfeld mit Steinbuckeln, zur Bahnsteigwand und zum Kabelkanal abfallend
    if bed:
        trough_y = yhi - 0.45  # Kabelkanal an der Wand
        nx = max(2, int(round((x1 - x0) / cell)))
        ys = []
        y = ylo
        while y < trough_y - 0.05:
            ys.append(y)
            y += cell * 0.93
        ys.append(trough_y)
        bm = bmesh.new()
        grid = []
        noise = random.Random(seed + 1)
        xs_ = []
        x = x0
        while x < x1 - 1e-6:  # unter dem Wagen (hidden) grobe Spalten
            xs_.append(x)
            x += cell * (2.5 if hidden and hidden(x, yc) else 1.0)
        xs_.append(x1)
        nx = len(xs_) - 1
        ph = (seed % 97) * 0.37

        def patch(x, y):  # grossflaechige Flecken: Oelspur zwischen den Schienen, Rost an den Schienen, dunkle Inseln
            if abs(y - yc) < 0.32 and math.sin(x * 1.7 + ph) + 0.6 * math.sin(x * 4.1 + ph * 2) > -0.5:
                return 1
            if abs(abs(y - yc) - (GAUGE / 2 + 0.0375)) < 0.16 and math.sin(x * 2.3 + y * 3.1 + ph) > 0.35:
                return 2
            return 1 if math.sin(x * 0.9 + ph) * math.sin(y * 2.2 + ph) > 0.55 else 0
        for i in range(nx + 1):
            x = xs_[i]
            row = []
            for j, yy in enumerate(ys):
                d_rail = abs(yy - yc)
                base = Z_BALLAST if d_rail < 1.55 else Z_BALLAST - (d_rail - 1.55) * 0.18
                base = max(0.035, base)
                if j == 0 or j == len(ys) - 1:
                    base = 0.03 if j == 0 else 0.06
                h = base + (noise.uniform(-0.014, 0.022) if 0 < j < len(ys) - 1 and 0 < i < nx else 0.0)
                row.append(bm.verts.new((x, yy, z + h)))
            grid.append(row)
        mats_idx = []
        for i in range(nx):
            for j in range(len(ys) - 1):
                f = bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
                f.normal_update()
                if f.normal.z < 0:
                    f.normal_flip()
                f.material_index = patch((xs_[i] + xs_[i + 1]) * 0.5, (ys[j] + ys[j + 1]) * 0.5)
                mats_idx.append(f.material_index)
        ob = _link(f'{prefix}_schotter', bm, [M['ballast'], M['ballast_dark'], M['ballast_rust']])
        _smooth(ob, 70)
        # einzelne groessere Steine auf dem Bett (unregelmaessige Achtflaechner)
        for k in range(int((x1 - x0) * (yhi - ylo) * stones)):
            sx = rng.uniform(x0 + 0.05, x1 - 0.05)
            sy = rng.uniform(ylo + 0.05, trough_y - 0.05)
            if abs(sy - yc) < 1.35 and ((sx - sleeper_x0) % 0.6 < 0.17 or (sx - sleeper_x0) % 0.6 > 0.43):
                continue  # nicht in die Schwellen
            if hidden and hidden(sx, sy):
                continue
            if abs(abs(sy - yc) - (GAUGE / 2 + 0.0375)) < 0.09:
                continue  # nicht in die Schienen
            d_rail = abs(sy - yc)
            base = Z_BALLAST if d_rail < 1.55 else max(0.035, Z_BALLAST - (d_rail - 1.55) * 0.18)
            r = rng.uniform(0.022, 0.045)
            bmr = bmesh.new()
            bmesh.ops.create_icosphere(bmr, subdivisions=1, radius=r)
            for v in bmr.verts:
                v.co.x *= rng.uniform(0.7, 1.3)
                v.co.y *= rng.uniform(0.7, 1.3)
                v.co.z *= rng.uniform(0.45, 0.8)
            bmesh.ops.transform(bmr, verts=bmr.verts, matrix=Matrix.Translation((sx, sy, z + base + 0.004)) @
                                Matrix.Rotation(rng.uniform(0, TAU), 4, 'Z'))
            _link(f'{prefix}_stein{k}', bmr, [M['stone_a'] if rng.random() < 0.6 else M['stone_b']])
        # Kabelkanal an der Wand: Trog aus Beton, Deckel mit Griffloechern, einer verschoben
        _box(f'{prefix}_trog', (x0, trough_y, z), (x1, yhi, z + 0.24), M['trough'])
        L = 1.0
        xk = x0
        k = 0
        while xk < x1 - 1e-6:
            xe = min(x1, xk + L)
            off = 0.03 if (k + seed) % 5 == 2 else 0.0
            lid = _box(f'{prefix}_deckel{k}', (xk + 0.004 + off, trough_y + 0.03, z + 0.24), (xe - 0.004 + off, yhi - 0.02, z + 0.275),
                       M['trough'])
            if (k + seed) % 7 == 3:
                lid.matrix_world = Matrix.Translation((0, 0, 0.012)) @ Matrix.Rotation(0.03, 4, 'X')
            for gx in (xk + 0.25, xe - 0.25):
                _decal(f'{prefix}_griff{k}{gx:.2f}', (gx + off, (trough_y + yhi) / 2, z + 0.2755), (1, 0, 0), (0, 1, 0),
                       [(-0.05, -0.018), (0.05, -0.018), (0.05, 0.018), (-0.05, 0.018)], M['joint'])
            xk = xe
            k += 1
    # --- Schwellen und Befestigungen
    x = sleeper_x0
    k = 0
    while x < x1 - 0.1:
        if x > x0 + 0.1:
            simple = xs_simple(x) if xs_simple else False
            _sleeper(f'{prefix}_schwelle{k}', x + rng.uniform(-0.012, 0.012), yc, z + 0.035, M, rng)
            for s in (-1, 1):
                _fastening(f'{prefix}_bef{k}{s}', x, yc + s * (GAUGE / 2 + 0.0375), z + Z_RAIL0, M, s, simple=simple or not detail)
        x += 0.6
        k += 1
    # --- Schienen
    for s in (-1, 1):
        _rail(f'{prefix}_schiene{s}', x0, x1, yc + s * (GAUGE / 2 + 0.0375), z + Z_RAIL0, M)
    # --- Stromschiene (von unten bestrichen, Abdeckung oben) auf Isolatorboecken an jeder vierten Schwelle
    yr = yc + 1.72
    zr = z + 0.43
    poly = [(yr - 0.04, zr - 0.012), (yr + 0.04, zr - 0.012), (yr + 0.04, zr), (yr + 0.012, zr + 0.01), (yr + 0.012, zr + 0.05),
            (yr - 0.012, zr + 0.05), (yr - 0.012, zr + 0.01), (yr - 0.04, zr)]
    _prism(f'{prefix}_stromschiene', poly, x0, x1, [M['conductor']], axis='x', smooth=25)
    cov = [(yr - 0.075, zr + 0.06), (yr + 0.075, zr + 0.06), (yr + 0.085, zr + 0.045), (yr + 0.09, zr + 0.065), (yr + 0.07, zr + 0.08),
           (yr - 0.07, zr + 0.08), (yr - 0.09, zr + 0.065), (yr - 0.085, zr + 0.045)]
    _prism(f'{prefix}_abdeckung', cov, x0, x1, [M['cover']], axis='x', smooth=30)
    x = sleeper_x0 + 0.6
    k = 0
    while x < x1 - 0.1:
        if x > x0 + 0.1 and k % 4 == 0:
            # Bock: Arm vom Schwellenkopf nach aussen, Isolator, Halter ueber der Schiene
            _box(f'{prefix}_bock{k}', (x - 0.03, yc + 1.25, z + 0.20), (x + 0.03, yr + 0.12, z + 0.235), M['clip'])
            _box(f'{prefix}_bocksteg{k}', (x - 0.025, yr + 0.10, z + 0.235), (x + 0.025, yr + 0.13, zr + 0.09), M['clip'])
            _cyl(f'{prefix}_isolator{k}', (x, yr + 0.115, zr + 0.09), (x, yr + 0.115, zr + 0.12), 0.03, M['insulator'], seg=8)
            _box(f'{prefix}_halter{k}', (x - 0.03, yr - 0.05, zr + 0.085), (x + 0.03, yr + 0.13, zr + 0.095), M['clip'])
        x += 0.6
        k += 1


def _place(objs, M):
    """Teile in ihre Lage bringen (Matrix in die Netze einrechnen; bei Spiegelung den Umlaufsinn umdrehen)."""
    neg = M.determinant() < 0
    for ob in objs:
        me = ob.data
        me.transform(M @ ob.matrix_world)
        ob.matrix_world = Matrix.Identity(4)
        if neg:
            bm = bmesh.new()
            bm.from_mesh(me)
            bmesh.ops.reverse_faces(bm, faces=bm.faces)
            bm.to_mesh(me)
            bm.free()
        me.update()


def _mirror(sx, sy, sz=1.0, t=(0, 0, 0)):
    return Matrix.Translation(t) @ Matrix.Diagonal((sx, sy, sz, 1.0))


def _intervals(a, b, holes):
    """[a, b] ohne die Loecher (Liste von (h0, h1))."""
    out, x = [], a
    for (h0, h1) in sorted(holes):
        if h1 <= x or h0 >= b:
            continue
        if h0 > x:
            out.append((x, h0))
        x = max(x, h1)
    if x < b:
        out.append((x, b))
    return out


# =====================================================================================================================
# Fahrender Metro-Wagen (metro_car_903, metro_car_37)
# =====================================================================================================================
CAR_HL = 9.0
END_T = 0.08
DOORS = (-6.0, 0.0, 6.0)
DOOR_HW, DOOR_H = 0.70, 1.95
WIN_Z0, WIN_Z1 = 0.95, 1.85
WINDOWS = ((-8.45, -7.60), (-4.40, -3.15), (-2.85, -1.60), (1.60, 2.85), (3.15, 4.40), (7.60, 8.45))
WALL_T = 0.06
SIDE = [(1.42, -0.46), (1.47, -0.40), (1.49, -0.26), (1.50, -0.14), (1.50, 0.95), (1.478, 1.90), (1.468, 2.04), (1.445, 2.18)]
ROOF = [(1.445, 2.18), (1.40, 2.29), (1.32, 2.39), (1.20, 2.47), (1.02, 2.54), (0.78, 2.595), (0.50, 2.63), (0.20, 2.648),
        (0.0, 2.652)]
CAR_Z = 1.11                                  # Wagenboden ueber dem Gleisbett (Karte: PLAT + 0.01)
Z_RAILTOP = Z_RAIL0 + 0.159 - CAR_Z           # Schienenoberkante im Wagen (-0.726)
WHEEL_R = 0.30
Z_WHEEL = Z_RAILTOP + WHEEL_R                 # Radmitte
BOGIES = (-6.3, 6.3)


def _side_y(z):
    if z <= SIDE[0][1]:
        return SIDE[0][0]
    for (y0, z0), (y1, z1) in zip(SIDE, SIDE[1:]):
        if z0 <= z <= z1:
            return y0 + (y1 - y0) * ((z - z0) / (z1 - z0) if z1 > z0 else 0.0)
    return SIDE[-1][0]


def _cross_section():
    right = SIDE + ROOF[1:]
    left = [(-y, z) for (y, z) in reversed(right)]
    return right + left[1:]


def _side_piece(name, x0, x1, z0, z1, s, M, inner=None, outer=None):
    """Stueck der Seitenwand zwischen z0 und z1 (Aussenkontur mit Einzug, innen um WALL_T versetzt); Laibungen alu matt."""
    zs = [z0] + [z for (_, z) in SIDE if z0 + 1e-6 < z < z1 - 1e-6] + [z1]
    outer_pts = [(_side_y(z), z) for z in zs]
    inner_pts = [(y - WALL_T, z) for (y, z) in reversed(outer_pts)]
    poly = outer_pts + inner_pts
    no = len(outer_pts)
    edge = []
    for i in range(len(poly)):
        if i < no - 1:
            edge.append(0)
        elif i == no - 1 or i == len(poly) - 1:
            edge.append(2)
        else:
            edge.append(1)
    poly = [(s * y, z) for (y, z) in poly]
    return _prism(name, poly, x0, x1, [outer or M['alu'], inner or M['inner'], M['alu_dull']], axis='x', edge_mat=edge,
                  cap_mat=(2, 2))


def _profile_strip(name, x0, x1, ztop, s, mat, dx=0.2, eps=0.004, zbot=-0.46):
    """Schicht auf der Aussenhaut (Schmutz): von zbot bis ztop(x), folgt dem Profil, eps vor der Oberflaeche."""
    nx = max(1, int(math.ceil((x1 - x0) / dx)))
    bm = bmesh.new()
    cols = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        zt = ztop(x)
        zs = [zbot, -0.40, -0.26, -0.14, zt]  # Stuetzstellen der Kontur (zt > -0.14)
        cols.append([bm.verts.new((x, s * (_side_y(z) + eps), z)) for z in zs])
    for i in range(nx):
        for k in range(4):
            f = bm.faces.new((cols[i][k], cols[i + 1][k], cols[i + 1][k + 1], cols[i][k + 1]))
            f.normal_update()
            if f.normal.y * s < 0:
                f.normal_flip()
    return _link(name, bm, [mat])


def _window(prefix, x0, x1, s, M, rng):
    """Seitenfenster: gerundete Ecken (Fuellstuecke durch die Wand), Gummidichtung aussen, Kippfenster oben (Glas, Rahmen,
    nach innen geneigt), darunter offen (Scheibe heruntergelassen - die Engine kennt keine Durchsicht), Oberkante der
    abgesenkten Scheibe an der Bruestung, Staubkante am Kippfenster."""
    z0, z1, rc = WIN_Z0, WIN_Z1, 0.085
    for (cx, cz, a0) in ((x0 + rc, z0 + rc, 180), (x1 - rc, z0 + rc, 270), (x1 - rc, z1 - rc, 0), (x0 + rc, z1 - rc, 90)):
        corner = (cx + rc * math.cos(math.radians(a0 + 45)) * 1.4142, cz + rc * math.sin(math.radians(a0 + 45)) * 1.4142)
        arc = [(cx + rc * math.cos(math.radians(a0 + 90 - 22.5 * k)), cz + rc * math.sin(math.radians(a0 + 90 - 22.5 * k)))
               for k in range(5)]
        poly = [corner] + arc
        yo = _side_y(cz)
        _prism(f'{prefix}_ecke{a0}', poly, s * (yo - WALL_T - 0.001), s * (yo + 0.001), [M['alu_dull']], axis='y')
    path = _rrect(x0, x1, z0, z1, rc, n=3)
    path = path + [path[0]]
    _tube(f'{prefix}_dichtung', [(x, s * (_side_y(z) + 0.002), z) for (x, z) in path], 0.011, M['rubber'], seg=5, caps=False)
    # Kippfenster: Rahmen und Glas, um die Unterkante (z = 1.58) 9 Grad nach innen gekippt
    zt0, zt1 = 1.575, z1 - 0.012
    yh = s * (_side_y(1.6) - 0.025)
    before = _names()
    _box(f'{prefix}_kipp_u', (x0 + 0.02, -0.012, zt0), (x1 - 0.02, 0.012, zt0 + 0.03), M['alu_dull'])
    _box(f'{prefix}_kipp_o', (x0 + 0.06, -0.012, zt1 - 0.028), (x1 - 0.06, 0.012, zt1), M['alu_dull'])
    for xx in (x0 + 0.02, x1 - 0.05):
        _box(f'{prefix}_kipp_s{xx:.2f}', (xx, -0.012, zt0), (xx + 0.03, 0.012, zt1 - 0.04), M['alu_dull'])
    _decal(f'{prefix}_kipp_glas', (x0 + 0.05, 0.0, zt0 + 0.03), (1, 0, 0), (0, 0, 1),
           _rrect(0.0, x1 - x0 - 0.10, 0.0, zt1 - zt0 - 0.058, 0.03, n=2), M['glass'])
    _decal(f'{prefix}_kipp_staub', (x0 + 0.05, 0.001 * s, zt0 + 0.03), (1, 0, 0), (0, 0, 1),
           [(0.01, 0.0), (x1 - x0 - 0.11, 0.0), (x1 - x0 - 0.11, 0.018 + rng.uniform(0, 0.01)), (0.5 * (x1 - x0), 0.03),
            (0.01, 0.014)], M['glass_dirty'])
    _place(_objs_since(before), Matrix.Translation((0, yh, zt0)) @ Matrix.Rotation(math.radians(9) * s, 4, 'X') @
           Matrix.Translation((0, 0, -zt0)))
    # Kaempfer unter dem Kippfenster und Oberkante der abgesenkten Scheibe
    _box(f'{prefix}_kaempfer', (x0, s * (_side_y(1.56) - 0.045) - 0.012, 1.548), (x1, s * (_side_y(1.56) - 0.045) + 0.012, 1.575),
         M['alu_dull'])
    yb = _side_y(z0) - 0.045
    _box(f'{prefix}_scheibenkante', (x0 + 0.03, s * yb - 0.01, z0), (x1 - 0.03, s * yb + 0.01, z0 + 0.035), M['alu_dull'])
    _box(f'{prefix}_scheibenkante_glas', (x0 + 0.03, s * yb - 0.004, z0 - 0.002), (x1 - 0.03, s * yb + 0.004, z0 + 0.03),
         M['glass'])


def _door_leaf(prefix, M, rng, smudge=True):
    """Tuerblatt im eigenen System: x 0 (Fuehrungskante) .. 0.70, y 0 (innen) .. 0.04 (aussen), z 0 .. DOOR_H.
    Rahmen aus Aluminium, Fenster mit gerundeten Ecken, Glas, Dichtung, Gummilippe vorn, Trittschutzblech, Griff innen,
    Warnaufkleber, Griffspuren."""
    W, T = 0.70, 0.04
    wx0, wx1, wz0, wz1, rc = 0.13, 0.57, 0.98, 1.76, 0.06
    _box(f'{prefix}_unten', (0.0, 0.0, 0.0), (W, T, wz0), M['alu'])
    _box(f'{prefix}_oben', (0.0, 0.0, wz1), (W, T, DOOR_H), M['alu'])
    _box(f'{prefix}_links', (0.0, 0.0, wz0), (wx0, T, wz1), M['alu'])
    _box(f'{prefix}_rechts', (wx1, 0.0, wz0), (W, T, wz1), M['alu'])
    for (cx, cz, a0) in ((wx0 + rc, wz0 + rc, 180), (wx1 - rc, wz0 + rc, 270), (wx1 - rc, wz1 - rc, 0), (wx0 + rc, wz1 - rc, 90)):
        corner = (cx + rc * math.cos(math.radians(a0 + 45)) * 1.4142, cz + rc * math.sin(math.radians(a0 + 45)) * 1.4142)
        arc = [(cx + rc * math.cos(math.radians(a0 + 90 - 22.5 * k)), cz + rc * math.sin(math.radians(a0 + 90 - 22.5 * k)))
               for k in range(5)]
        _prism(f'{prefix}_ecke{a0}', [corner] + arc, 0.0, T, [M['alu']], axis='y')
    _decal(f'{prefix}_glas', (0.0, T * 0.5, 0.0), (1, 0, 0), (0, 0, 1), _rrect(wx0, wx1, wz0, wz1, rc, n=3), M['glass'])
    _decal(f'{prefix}_glas_staub', (0.0, T * 0.5 + 0.001, 0.0), (1, 0, 0), (0, 0, 1),
           [(wx0 + 0.02, wz0 + 0.01), (wx1 - 0.02, wz0 + 0.01), (wx1 - 0.02, wz0 + 0.05), (0.5 * (wx0 + wx1), wz0 + 0.07),
            (wx0 + 0.02, wz0 + 0.04)], M['glass_dirty'])
    path = _rrect(wx0, wx1, wz0, wz1, rc, n=3)
    _tube(f'{prefix}_dichtung', [(x, T + 0.002, z) for (x, z) in path + [path[0]]], 0.008, M['rubber'], seg=5, caps=False)
    # Gummilippe an der Fuehrungskante (Wulst)
    lip = [(0.0, 0.004), (-0.012, 0.006), (-0.022, 0.014), (-0.026, 0.02), (-0.022, 0.026), (-0.012, 0.034), (0.0, 0.036)]
    _prism(f'{prefix}_lippe', lip, 0.01, DOOR_H - 0.01, [M['rubber']], axis='z', smooth=50)
    # Trittschutzblech und Schmutz
    _box(f'{prefix}_blech', (0.02, T, 0.03), (W - 0.02, T + 0.002, 0.24), M['chrome_worn'])
    _decal(f'{prefix}_schmutz', (0.0, T + 0.0025, 0.0), (1, 0, 0), (0, 0, 1),
           [(0.02, 0.03), (W - 0.02, 0.03), (W - 0.02, 0.05 + rng.uniform(0, 0.03)), (0.45, 0.07), (0.2, 0.05 + rng.uniform(0, 0.03)),
            (0.02, 0.06)], M['grime'])
    if smudge:  # Griffspuren an der Fuehrungskante (weich, unregelmaessig)
        _decal(f'{prefix}_griffspur', (0.0, T + 0.001, 0.0), (1, 0, 0), (0, 0, 1), _blob(rng, 0.035, 0.20, 13, 0.45, 0.05, 1.22),
               M['grime'])
    # Warnaufkleber (gelbes Dreieck mit Hand) aussen am Fenster
    tri = [(0.30, 1.03), (0.40, 1.03), (0.35, 1.115)]
    _decal(f'{prefix}_warn', (0.0, T * 0.5 + 0.003, 0.0), (1, 0, 0), (0, 0, 1), tri, M['sticker_y'])
    _decal(f'{prefix}_warn_hand', (0.0, T * 0.5 + 0.0035, 0.0), (1, 0, 0), (0, 0, 1), _blob(rng, 0.016, 0.02, 7, 0.2, 0.35, 1.06),
           M['ink'])
    # Griff innen
    _tube(f'{prefix}_griff', [(0.08, -0.002, 0.95), (0.08, -0.04, 0.98), (0.08, -0.04, 1.32), (0.08, -0.002, 1.35)], 0.012,
          M['steel'], seg=6)


def _door(prefix, d, s, M, rng, open_):
    """Tuerportal (Dichtung, Fuehrungsschiene oben und unten, Schwelle) und zwei Tuerblaetter (aussen schiebend)."""
    yo = _side_y(1.0)
    for k in (-1, 1):
        before = _names()
        _door_leaf(f'{prefix}_blatt{k}', M, rng)
        if open_:
            M_ = Matrix.Translation((d + k * 0.685, s * (yo + 0.004), 0.0)) @ Matrix.Diagonal((k, s, 1.0, 1.0))
        else:
            M_ = Matrix.Translation((d + k * 0.001, s * (yo - 0.042), 0.0)) @ Matrix.Diagonal((k, s, 1.0, 1.0))
        _place(_objs_since(before), M_)
    # Portal: Dichtungsstreifen an den Kanten der Oeffnung
    for k in (-1, 1):
        x = d + k * DOOR_HW
        _box(f'{prefix}_portal{k}', (min(x, x - k * 0.025), s * yo - 0.012, 0.0), (max(x, x - k * 0.025), s * yo + 0.012, DOOR_H),
             M['rubber'])
    _box(f'{prefix}_portal_o', (d - DOOR_HW, s * _side_y(1.95) - 0.012, DOOR_H - 0.025), (d + DOOR_HW, s * _side_y(1.95) + 0.012, DOOR_H),
         M['rubber'])
    # Fuehrungsschiene oben (Abdeckung) und unten
    y1 = _side_y(2.0)
    _box(f'{prefix}_schiene_o', (d - 1.44, s * y1, 1.955), (d + 1.44, s * (y1 + 0.085), 2.035),
         M['alu_dull'], bevel=0.008)
    _box(f'{prefix}_schiene_u', (d - 1.44, min(s * 1.48, s * 1.55), -0.07), (d + 1.44, max(s * 1.48, s * 1.55), -0.035), M['under'])
    # Schwelle mit gelber Kante
    _box(f'{prefix}_schwelle', (d - DOOR_HW, min(s * 1.25, s * 1.505), -0.004), (d + DOOR_HW, max(s * 1.25, s * 1.505), 0.006),
         M['chrome_worn'])
    _box(f'{prefix}_schwelle_gelb', (d - DOOR_HW, min(s * 1.46, s * 1.505), 0.0061), (d + DOOR_HW, max(s * 1.46, s * 1.505), 0.0066),
         M['yellow'])
    # Tuerleuchte innen ueber der Tuer (aus)
    _box(f'{prefix}_leuchte', (d - 0.08, s * (yo - WALL_T) - 0.03 * s, 1.97), (d + 0.08, s * (yo - WALL_T), 2.0), M['shell'])


def _bench(prefix, x0, x1, s, M, rng):
    """Laengsbank: Sitzschale mit Heizungsblende, Polster je Sitz (0.44 m), Rueckenpolster, einzelne Sitze abgewetzt."""
    shell = [(1.44, 0.08), (1.02, 0.08), (1.02, 0.36), (0.975, 0.40), (0.975, 0.43), (1.44, 0.43)]
    _prism(f'{prefix}_schale', [(s * y, z) for (y, z) in shell], x0, x1, [M['shell']], axis='x')
    # Blende mit Luftschlitzen
    n = int((x1 - x0 - 0.1) / 0.07)
    for k in range(n):
        x = x0 + 0.08 + k * 0.07
        _decal(f'{prefix}_schlitz{k}', (x, s * (1.02 - 0.0015), 0.13), (s, 0, 0), (0, 0, 1), [(0, 0), (0.035, 0), (0.035, 0.17), (0, 0.17)],
               M['ink'])
    seat = [(1.42, 0.43), (0.99, 0.43), (0.972, 0.448), (0.978, 0.474), (1.0, 0.488), (1.10, 0.494), (1.42, 0.49)]
    back = [(1.435, 0.50), (1.405, 0.50), (1.36, 0.56), (1.335, 0.70), (1.345, 0.88), (1.39, 0.935), (1.435, 0.935)]
    nseat = max(1, int((x1 - x0) / 0.465))
    w = (x1 - x0) / nseat
    for k in range(nseat):
        a, b = x0 + k * w + 0.012, x0 + (k + 1) * w - 0.012
        worn = rng.random() < 0.3
        _prism(f'{prefix}_sitz{k}', [(s * y, z) for (y, z) in seat], a, b, [M['seat_worn'] if worn else M['seat']], axis='x', smooth=45)
        _prism(f'{prefix}_lehne{k}', [(s * y, z) for (y, z) in back], a, b, [M['seat']], axis='x', smooth=45)
        if rng.random() < 0.12:  # geflickter Riss (Klebeband)
            _decal(f'{prefix}_flick{k}', (a + w * 0.4, s * 1.18, 0.4945), (1, 0, 0), (0, 1, 0),
                   [(0, 0), (0.12, 0.01), (0.11, 0.05), (0.0, 0.045)], M['grime_dark'])


def _partition(prefix, x, s, M, facing):
    """Trennwand am Bankende (zur Tuer hin) mit Haltestange bis zur Decke."""
    pts = [(1.44, 0.06), (1.0, 0.06)] + [(1.0 + 0.14 - 0.14 * math.cos(math.radians(a)), 0.92 + 0.14 * math.sin(math.radians(a)))
                                          for a in (0, 30, 60, 90)] + [(1.44, 1.06)]
    _prism(f'{prefix}_wand', [(s * y, z) for (y, z) in pts], x - 0.015, x + 0.015, [M['inner_grey']], axis='x')
    xp = x + facing * 0.03
    _cyl(f'{prefix}_stange', (xp, s * 0.97, 0.0), (xp, s * 0.97, 2.25), 0.019, M['steel'], seg=10)
    _cyl(f'{prefix}_fuss', (xp, s * 0.97, 0.0), (xp, s * 0.97, 0.03), 0.04, M['chrome_worn'], seg=10)
    _cyl(f'{prefix}_kopf', (xp, s * 0.97, 2.21), (xp, s * 0.97, 2.25), 0.04, M['chrome_worn'], seg=10)
    _tube(f'{prefix}_buegel', [(xp, s * 0.97, 1.06), (xp, s * 1.2, 1.06), (xp, s * 1.40, 1.10)], 0.014, M['steel'], seg=6)


def _grab_rail(prefix, x0, x1, s, M):
    y, z, zc = s * 0.93, 1.87, 2.24
    _tube(f'{prefix}_stange', [(x0, y, zc), (x0, y, z + 0.06), (x0 + 0.06, y, z), (x1 - 0.06, y, z), (x1, y, z + 0.06), (x1, y, zc)],
          0.016, M['steel'], seg=7)
    n = max(1, int((x1 - x0) / 1.4))
    for k in range(1, n):
        x = x0 + (x1 - x0) * k / n
        _cyl(f'{prefix}_halter{k}', (x, y, z), (x, y, zc), 0.011, M['chrome_worn'], seg=6)


def _ceiling(M, L):
    side = [(1.44, 1.995), (1.38, 2.10), (1.25, 2.19), (1.05, 2.252), (0.80, 2.285), (0.62, 2.298)]
    for s in (-1, 1):
        poly = side + [(y, z + 0.015) for (y, z) in reversed(side)]
        _prism(f'decke_seite{s}', [(s * y, z) for (y, z) in poly], -L, L, [M['inner']], axis='x', smooth=35)
        # Lichtband: Kanal mit Streuscheibe
        _box(f'licht_kanal{s}', (-L, min(s * 0.40, s * 0.62), 2.298), (L, max(s * 0.40, s * 0.62), 2.36), M['inner_grey'])
        _box(f'licht{s}', (-L + 0.25, min(s * 0.43, s * 0.59), 2.292), (L - 0.25, max(s * 0.43, s * 0.59), 2.300), M['light'])
    _box('decke_mitte', (-L, -0.40, 2.36), (L, 0.40, 2.375), M['inner'])
    x = -L + 0.6
    k = 0
    while x < L - 0.6:  # Lueftungsgitter in der Mitte
        for j in range(5):
            _decal(f'luefter{k}_{j}', (x + j * 0.05, -0.18, 2.3595), (0, 1, 0), (1, 0, 0), [(0, 0), (0.36, 0), (0.36, 0.025), (0, 0.025)],
                   M['ink'])
        x += 1.5
        k += 1


PICTO_SIZE = {'wave': 0.56, 'plane': 0.60, 'train': 0.37}


def _display(prefix, x, sgn, M, kind, size=1.0, w=0.56, z0=1.93, z1=2.30):
    """Zielanzeige an der Stirnwand innen (Gehaeuse, schwarze Front, Bildzeichen oder dunkel). sgn: Blick in -sgn*x."""
    xf = x - sgn * 0.06
    _box(f'{prefix}_gehaeuse', (min(x, xf), -w - 0.03, z0 - 0.03), (max(x, xf), w + 0.03, z1 + 0.03), M['shell'], bevel=0.01)
    U = (0, -sgn, 0)
    _decal(f'{prefix}_front', (xf - sgn * 0.001, 0.0, 0.0), U, (0, 0, 1), [(-w, z0), (w, z0), (w, z1), (-w, z1)], M['disp_bg'])
    if kind:
        _pictogram(f'{prefix}_zeichen', kind, (xf - sgn * 0.002, 0.0, (z0 + z1) / 2 + 0.02), U, (0, 0, 1), size * PICTO_SIZE[kind],
                   M['disp'], M['disp_bg'])
        # Statuszeile (Leuchtpunkte) unter dem Bildzeichen
        for k in range(9):
            _decal(f'{prefix}_punkt{k}', (xf - sgn * 0.002, 0.0, 0.0), U, (0, 0, 1),
                   [(-0.20 + k * 0.05, z0 + 0.018), (-0.18 + k * 0.05, z0 + 0.018), (-0.18 + k * 0.05, z0 + 0.034),
                    (-0.20 + k * 0.05, z0 + 0.034)], M['disp_amber'])
    else:
        # dunkle Anzeige: nur ein schwacher Punktrest
        _decal(f'{prefix}_rest', (xf - sgn * 0.002, 0.0, 0.0), U, (0, 0, 1),
               [(-0.30, z0 + 0.14), (-0.27, z0 + 0.14), (-0.27, z0 + 0.17), (-0.30, z0 + 0.17)], M['disp_amber'])


def _car_end(prefix, sgn, M, rng, kind, front):
    """Stirnseite bei x = sgn*9: Wand aus dem Querschnitt (Tuer, zwei Fenster), Eckpfosten, Rammschutz, Kupplung,
    Scheinwerfer/Schlusslichter, Anzeige aussen und innen."""
    xo = sgn * CAR_HL
    xi = sgn * (CAR_HL - END_T)
    a0, a1 = min(xo, xi), max(xo, xi)
    CS = _cross_section()
    rects = [(-2, 2, -0.46, 0.0), (-2, -0.36, 0.0, 1.0), (0.36, 2, 0.0, 1.0), (-2, -1.26, 1.0, 1.84), (-0.56, -0.36, 1.0, 1.84),
             (0.36, 0.56, 1.0, 1.84), (1.26, 2, 1.0, 1.84), (-2, -0.36, 1.84, 1.90), (0.36, 2, 1.84, 1.90), (-2, 2, 1.90, 3.0)]
    for k, (y0, y1, z0, z1) in enumerate(rects):
        poly = _clip_rect(CS, y0, y1, z0, z1)
        if len(poly) >= 3:
            _prism(f'{prefix}_wand{k}', poly, a0, a1, [M['alu_dull'], M['inner'], M['alu']], axis='x', edge_mat=[0] * len(poly),
                   cap_mat=(1, 2) if sgn > 0 else (2, 1))
    # Fenster der Stirn: Glas, Dichtung; Scheibenwischer am rechten
    for (y0, y1) in ((0.56, 1.26), (-1.26, -0.56)):
        _decal(f'{prefix}_glas{y0}', (sgn * (CAR_HL - END_T * 0.5), 0, 0), (0, 1, 0), (0, 0, 1), [(y0, 1.0), (y1, 1.0), (y1, 1.84), (y0, 1.84)],
               M['glass'])
        path = _rrect(y0, y1, 1.0, 1.84, 0.05, n=2)
        _tube(f'{prefix}_dichtung{y0}', [(xo + sgn * 0.002, y, z) for (y, z) in path + [path[0]]], 0.01, M['rubber'], seg=5, caps=False)
    _box(f'{prefix}_wischer', (xo + sgn * 0.004, 0.70, 1.05), (xo + sgn * 0.012, 0.72, 1.55), M['ink'])
    ob = _box(f'{prefix}_wischblatt', (-0.006, -0.006, -0.22), (0.006, 0.006, 0.22), M['rubber'])
    ob.matrix_world = Matrix.Translation((xo + sgn * 0.012, 0.86, 1.30)) @ Matrix.Rotation(math.radians(28), 4, 'X')
    # Stirntuer (geschlossen) mit Fenster, Griff innen und aussen
    dx0, dx1 = min(xo - sgn * 0.02, xo - sgn * 0.06), max(xo - sgn * 0.02, xo - sgn * 0.06)
    for (y0, y1, z0, z1) in ((-0.36, 0.36, 0.0, 1.05), (-0.36, 0.36, 1.75, 1.90), (-0.36, -0.22, 1.05, 1.75), (0.22, 0.36, 1.05, 1.75)):
        _box(f'{prefix}_tuer{y0}{z0}', (dx0, y0, z0), (dx1, y1, z1), M['alu'])
    _decal(f'{prefix}_tuerglas', (sgn * (CAR_HL - 0.04), 0, 0), (0, 1, 0), (0, 0, 1), [(-0.22, 1.05), (0.22, 1.05), (0.22, 1.75), (-0.22, 1.75)],
           M['glass'])
    for side in (-1, 1):
        xg = (dx1 + 0.03) if side * sgn > 0 else (dx0 - 0.03)
        xg0 = dx1 if side * sgn > 0 else dx0
        for yy in (0.26, 0.12):
            _cyl(f'{prefix}_tuergriff{side}_{yy}', (xg0, yy, 0.98), (xg, yy, 0.98), 0.009, M['steel'], seg=6)
        _cyl(f'{prefix}_tuergriff{side}', (xg, 0.27, 0.98), (xg, 0.11, 0.98), 0.012, M['steel'], seg=8)
    _box(f'{prefix}_tuerblech', (min(dx0, dx1) - 0.002, -0.33, 0.03), (max(dx0, dx1) + 0.002, 0.33, 0.25), M['chrome_worn'])
    # Eckpfosten (gefast) entlang des Profils
    for s in (-1, 1):
        rings = []
        for (y, z) in SIDE:
            rings.append([(xo + sgn * 0.006, s * (y + 0.006), z), (xo - sgn * 0.05, s * (y + 0.006), z), (xo - sgn * 0.05, s * (y - 0.004), z),
                          (xo + sgn * 0.006, s * (y - 0.05), z)])
        _loft(f'{prefix}_eckpfosten{s}', rings, [M['alu_dull']])
    # Rammschutz mit Rippen, Kupplung (Scharfenberg-Art), Luftschlaeuche
    ax = (xo, xo + sgn * 0.09)
    _box(f'{prefix}_ramm', (min(ax), -0.95, -0.26), (max(ax), 0.95, -0.02), M['under'])
    for k in range(4):
        z = -0.23 + k * 0.06
        _box(f'{prefix}_rippe{k}', (min(xo, xo + sgn * 0.11), -0.92, z), (max(xo, xo + sgn * 0.11), 0.92, z + 0.025), M['under'], bevel=0.006)
    _box(f'{prefix}_zugeinr', (min(xo, xo + sgn * 0.16), -0.15, -0.44), (max(xo, xo + sgn * 0.16), 0.15, -0.20), M['bogie'])
    _cyl(f'{prefix}_kuppl_schaft', (xo + sgn * 0.16, 0, -0.32), (xo + sgn * 0.48, 0, -0.32), 0.06, M['bogie'], seg=10)
    _box(f'{prefix}_kuppl_kopf', (min(xo + sgn * 0.48, xo + sgn * 0.62), -0.17, -0.44), (max(xo + sgn * 0.48, xo + sgn * 0.62), 0.17, -0.20),
         M['under_brown'], bevel=0.02, seg=2)
    _cyl(f'{prefix}_kuppl_kegel', (xo + sgn * 0.62, 0.08, -0.32), (xo + sgn * 0.72, 0.08, -0.32), 0.045, M['wheel'], seg=10, r1=0.012)
    _cyl(f'{prefix}_kuppl_trichter', (xo + sgn * 0.615, -0.08, -0.32), (xo + sgn * 0.625, -0.08, -0.32), 0.05, M['grime_dark'], seg=10)
    _box(f'{prefix}_kuppl_kontakt', (min(xo + sgn * 0.46, xo + sgn * 0.60), -0.12, -0.53), (max(xo + sgn * 0.46, xo + sgn * 0.60), 0.12, -0.44),
         M['under'])
    for s in (-1, 1):
        _tube(f'{prefix}_luftschlauch{s}', [(xo, s * 0.45, -0.20), (xo + sgn * 0.08, s * 0.47, -0.30), (xo + sgn * 0.10, s * 0.50, -0.44),
                                            (xo + sgn * 0.05, s * 0.55, -0.50)], 0.022, M['rubber'], seg=6)
        _box(f'{prefix}_hahn{s}', (min(xo + sgn * 0.03, xo + sgn * 0.08), s * 0.55 - 0.03, -0.53), (max(xo + sgn * 0.03, xo + sgn * 0.08), s * 0.55 + 0.03, -0.49),
             M['red_handle'])
    # Scheinwerfer und Schlusslichter (vorn weiss an, hinten rot an)
    for s in (-1, 1):
        for (yy, r, lens) in ((1.00, 0.085, 'head'), (0.74, 0.05, 'tail')):
            on = (front and lens == 'head') or (not front and lens == 'tail')
            for nm, prof, mat in (('ring', [(r + 0.025, 0.0), (r + 0.02, 0.03), (r, 0.035), (r - 0.005, 0.03)], M['alu_dull']),
                                  ('linse', [(r - 0.004, 0.028), (r * 0.6, 0.036), (0.0, 0.039)], M[lens] if on else M['glass'])):
                p = [(rr, sgn * h) for (rr, h) in prof]
                if sgn < 0:
                    p = list(reversed(p))
                _lathe(f'{prefix}_{nm}{s}{yy}', p, [mat], seg=16, center=(xo, s * yy, 0.32), axis='x')
    # Wagennummer (erfundene Zeichen) unten links aussen
    rngw = random.Random(77 + sgn)
    _text(f'{prefix}_nummer', _word(rngw, 3, 4), (xo + sgn * 0.003, -1.25 if sgn > 0 else 1.25, 0.62), (0, 1 if sgn > 0 else -1, 0), (0, 0, 1),
          0.07, M['ink'])
    # Anzeigen: innen ueber der Stirntuer, aussen ueber der Tuer
    _display(f'{prefix}_anzeige_innen', xi, sgn, M, kind, w=0.56)
    _box(f'{prefix}_anzeige_aussen', (min(xo, xo + sgn * 0.03), -0.42, 1.93), (max(xo, xo + sgn * 0.03), 0.42, 2.17), M['shell'], bevel=0.008)
    _decal(f'{prefix}_anzeige_aussen_front', (xo + sgn * 0.031, 0, 0), (0, sgn, 0), (0, 0, 1), [(-0.39, 1.95), (0.39, 1.95), (0.39, 2.15), (-0.39, 2.15)],
           M['disp_bg'])
    if kind:
        _pictogram(f'{prefix}_anzeige_aussen_zeichen', kind, (xo + sgn * 0.032, 0.0, 2.05), (0, sgn, 0), (0, 0, 1), 0.45 * PICTO_SIZE[kind],
                   M['disp'], M['disp_bg'])
    # Notbremsgriff und Feuerloescher innen
    _box(f'{prefix}_notbremse', (min(xi, xi - sgn * 0.05), 0.48, 1.45), (max(xi, xi - sgn * 0.05), 0.60, 1.62), M['red_handle'], bevel=0.008)
    if sgn > 0:
        _cyl(f'{prefix}_loescher', (xi - 0.12, -1.2, 0.06), (xi - 0.12, -1.2, 0.52), 0.07, M['red_handle'], seg=12)
        _cyl(f'{prefix}_loescher_kopf', (xi - 0.12, -1.2, 0.52), (xi - 0.12, -1.2, 0.60), 0.025, M['ink'], seg=8)
        _box(f'{prefix}_loescher_halter', (xi - 0.06, -1.29, 0.20), (xi, -1.11, 0.24), M['ink'])


def _bogie(prefix, bx, dz, M, rng):
    """Drehgestell: Radsaetze (Radprofil mit Spurkranz, blanke Laufflaeche), Achslager mit Deckel, Lenker und
    Schraubenfedern, Rahmen mit Hornfuehrungen, Querträger, Luftfedern, Fahrmotoren, Getriebe, Klotzbremsen,
    Schleifschuh am Stromabnehmerbalken, Kabel."""
    zw = Z_WHEEL + dz
    wheel = [(0.06, 0.58), (0.13, 0.58), (0.16, 0.655), (0.25, 0.664), (0.270, 0.645), (0.300, 0.645), (0.322, 0.650), (0.327, 0.660),
             (0.320, 0.672), (0.302, 0.688), (0.300, 0.735), (0.294, 0.790), (0.282, 0.800), (0.25, 0.778), (0.16, 0.762),
             (0.13, 0.82), (0.06, 0.82)]
    for ax in (bx - 1.0, bx + 1.0):
        for s in (-1, 1):
            prof = [(r, s * h) for (r, h) in wheel]
            segm = [1 if 7 <= i <= 11 else 0 for i in range(len(prof))]
            if _area(prof) < 0:
                prof = list(reversed(prof))
                segm = list(reversed(segm[:-1])) + [segm[-1]]
                segm = [segm[(i - 1) % len(segm)] for i in range(len(segm))]
            _lathe(f'{prefix}_rad{ax:.1f}{s}', prof, [M['wheel'], M['tread']], seg=16, center=(ax, 0, zw), axis='y', seg_mat=segm,
                   closed=True, sharp=30)
            # Achslager, Deckel, Schrauben
            _cyl(f'{prefix}_lager{ax:.1f}{s}', (ax, s * 0.83, zw), (ax, s * 1.0, zw), 0.105, M['bogie'], seg=14)
            _cyl(f'{prefix}_deckel{ax:.1f}{s}', (ax, s * 1.0, zw), (ax, s * 1.03, zw), 0.085, M['under_brown'], seg=14)
            for b in range(4):
                a = TAU * b / 4 + 0.4
                _cyl(f'{prefix}_bolzen{ax:.1f}{s}{b}', (ax + 0.06 * math.cos(a), s * 1.03, zw + 0.06 * math.sin(a)),
                     (ax + 0.06 * math.cos(a), s * 1.045, zw + 0.06 * math.sin(a)), 0.011, M['wheel'], seg=6)
            # Lenker (Achslager-Arme) und zwei Schraubenfedern
            _box(f'{prefix}_lenker{ax:.1f}{s}', (ax - 0.30, s * 0.86 - 0.05, zw - 0.10), (ax + 0.30, s * 0.86 + 0.05 if s > 0 else s * 0.86 + 0.05,
                                                                                         zw - 0.045), M['bogie'], bevel=0.01)
            for fx in (ax - 0.22, ax + 0.22):
                pts = []
                turns, n_t = 2.5, 7
                for k in range(int(turns * n_t) + 1):
                    t = k / n_t
                    a = TAU * t
                    pts.append((fx + 0.052 * math.cos(a), s * 0.86 + 0.052 * math.sin(a), zw - 0.04 + 0.19 * t / turns))
                _tube(f'{prefix}_feder{fx:.2f}{s}', pts, 0.012, M['spring'], seg=4, caps=False)
            # Klotzbremse innen am Rad
            bxk = ax - math.copysign(0.315, ax - bx)
            _box(f'{prefix}_klotz{ax:.1f}{s}', (bxk - 0.03, s * 0.70 - 0.065, zw - 0.09), (bxk + 0.03, s * 0.70 + 0.065, zw + 0.09), M['under_brown'])
            bxa = bxk - math.copysign(0.09, ax - bx)
            _box(f'{prefix}_bremse{ax:.1f}{s}', (bxa - 0.06, s * 0.74 - 0.08, zw - 0.02), (bxa + 0.06, s * 0.74 + 0.08, zw + 0.12), M['bogie'], bevel=0.012)
        _cyl(f'{prefix}_achse{ax:.1f}', (ax, -1.0, zw), (ax, 1.0, zw), 0.065, M['wheel'], seg=10)
    # Rahmen: Langtraeger mit Hornfuehrungen und Mittelabsenkung (Luftfedern)
    frame = [(-1.45, 0.16), (-1.22, 0.16), (-1.22, -0.03), (-1.13, -0.03), (-1.13, 0.16), (-0.87, 0.16), (-0.87, -0.03), (-0.78, -0.03),
             (-0.78, 0.16), (-0.55, 0.04), (0.55, 0.04), (0.78, 0.16), (0.78, -0.03), (0.87, -0.03), (0.87, 0.16), (1.13, 0.16),
             (1.13, -0.03), (1.22, -0.03), (1.22, 0.16), (1.45, 0.16), (1.45, 0.29), (0.62, 0.29), (0.42, 0.14), (-0.42, 0.14),
             (-0.62, 0.29), (-1.45, 0.29)]
    for s in (-1, 1):
        _prism(f'{prefix}_rahmen{s}', [(bx + x, zw + z) for (x, z) in frame], s * 0.86 - 0.07, s * 0.86 + 0.07, [M['bogie']], axis='y')
        # Gurtbleche oben (etwas breiter) als Kante
        _box(f'{prefix}_gurt{s}', (bx - 1.45, s * 0.86 - 0.085, zw + 0.27), (bx - 0.62, s * 0.86 + 0.085, zw + 0.29), M['bogie'])
        _box(f'{prefix}_gurt2{s}', (bx + 0.62, s * 0.86 - 0.085, zw + 0.27), (bx + 1.45, s * 0.86 + 0.085, zw + 0.29), M['bogie'])
        # Luftfeder (Balg) zwischen Rahmenmitte und Wagenkasten
        h0, h1 = zw + 0.14, -0.13
        prof = [(0.11, h0), (0.15, h0 + 0.02), (0.17, (h0 + h1) / 2), (0.15, h1 - 0.02), (0.11, h1)]
        _lathe(f'{prefix}_luftfeder{s}', [(r, h - h0) for (r, h) in prof], [M['rubber']], seg=14, center=(bx, s * 0.86, h0), axis='z')
        _cyl(f'{prefix}_federteller{s}', (bx, s * 0.86, h1), (bx, s * 0.86, -0.12), 0.13, M['under'], seg=14)
        # Stromabnehmerbalken (Holz) und Schleifschuh (unter die Stromschiene greifend)
        _box(f'{prefix}_schuhbalken{s}', (bx - 1.12, s * 1.06 - 0.025, zw - 0.07), (bx + 1.12, s * 1.06 + 0.025, zw + 0.01), M['shoe_wood'])
        for xx in (bx - 1.0, bx + 1.0):
            _box(f'{prefix}_balkenhalter{xx:.1f}{s}', (xx - 0.04, s * 1.03 - 0.02, zw - 0.08), (xx + 0.04, s * 1.03 + 0.03, zw + 0.02), M['bogie'])
        _tube(f'{prefix}_schuharm{s}', [(bx, s * 1.08, zw - 0.03), (bx, s * 1.40, zw - 0.10), (bx, s * 1.60, Z_RAILTOP + 0.025 + dz)], 0.022,
              M['bogie'], seg=6)
        _box(f'{prefix}_schuh{s}', (bx - 0.16, s * 1.71 - 0.09, Z_RAILTOP + 0.018 + dz), (bx + 0.16, s * 1.71 + 0.09, Z_RAILTOP + 0.034 + dz),
             M['tread'])
        _tube(f'{prefix}_schuhkabel{s}', [(bx + 0.1, s * 1.55, Z_RAILTOP + 0.06 + dz), (bx + 0.3, s * 1.35, zw - 0.05), (bx + 0.45, s * 1.22, -0.14)],
              0.012, M['cable'], seg=5)
    # Quertraeger, Drehzapfen, Fahrmotoren mit Getriebe
    for qx in (bx - 0.30, bx + 0.30):
        _cyl(f'{prefix}_quer{qx:.2f}', (qx, -0.86, zw + 0.08), (qx, 0.86, zw + 0.08), 0.065, M['bogie'], seg=10)
    _box(f'{prefix}_zapfen', (bx - 0.22, -0.55, zw + 0.12), (bx + 0.22, 0.55, -0.12), M['under'], bevel=0.02)
    for mx in (-1, 1):
        cx = bx + mx * 0.53
        _cyl(f'{prefix}_motor{mx}', (cx, -0.42, zw + 0.02), (cx, 0.36, zw + 0.02), 0.15, M['bogie'], seg=14)
        for k in range(2):
            _cyl(f'{prefix}_motorrippe{mx}{k}', (cx, -0.25 + k * 0.45, zw + 0.02), (cx, -0.22 + k * 0.45, zw + 0.02), 0.158, M['under'],
                 seg=12, smooth=True)
        _box(f'{prefix}_getriebe{mx}', (min(cx, bx + mx * 1.0) - 0.02, -0.56, zw - 0.12), (max(cx, bx + mx * 1.0) + 0.02, -0.44, zw + 0.12),
             M['under_brown'], bevel=0.02)
        _tube(f'{prefix}_motorkabel{mx}', [(cx, 0.36, zw + 0.10), (cx + mx * 0.05, 0.50, zw + 0.25), (cx + mx * 0.1, 0.62, -0.13)], 0.018,
              M['cable'], seg=5)


def _underframe(M, rng):
    """Unterboden: Langtraeger hinter der Schuerze, Geraetekaesten zwischen den Drehgestellen, Luftbehaelter, Kabelkanaele."""
    _box('bodenplatte_unten', (-CAR_HL + END_T, -1.40, -0.15), (CAR_HL - END_T, 1.40, -0.12), M['under'])
    for s in (-1, 1):
        _box(f'langtraeger{s}', (-CAR_HL + END_T, min(s * 1.28, s * 1.38), -0.40), (CAR_HL - END_T, max(s * 1.28, s * 1.38), -0.12), M['under'])
    # Stromrichter mit Kuehlrippen zur Bahnsteigseite
    _box('stromrichter', (-4.45, -1.18, -0.60), (-2.45, 0.25, -0.13), M['under'], bevel=0.015)
    for k in range(12):
        x = -4.35 + k * 0.165
        _box(f'kuehlrippe{k}', (x, -1.24, -0.56), (x + 0.012, -1.18, -0.20), M['under'])
    _box('batterie', (-2.20, 0.30, -0.52), (-1.0, 1.22, -0.13), M['under_brown'], bevel=0.012)
    for k in range(3):
        _box(f'batterie_griff{k}', (-2.1 + k * 0.4, 1.22, -0.36), (-1.95 + k * 0.4, 1.245, -0.33), M['wheel'])
    for (y, r) in ((-0.78, 0.16), (-0.38, 0.16)):
        _cyl(f'luftbehaelter{y}', (-0.85, y, -0.38), (1.35, y, -0.38), r, M['under'], seg=16)
        for xx in (-0.95, 1.45):
            _lathe(f'luftbehaelter_boden{y}{xx}', [(r, 0.0), (r * 0.8, 0.05 * (1 if xx > 0 else -1)), (0.0, 0.07 * (1 if xx > 0 else -1))],
                   [M['under']], seg=16, center=(1.35 if xx > 0 else -0.85, y, -0.38), axis='x')
        for xx in (-0.45, 0.95):
            _lathe(f'luftbehaelter_band{y}{xx}', [(r + 0.008, 0.0), (r + 0.008, 0.04)], [M['under_brown']], seg=16, center=(xx, y, -0.38), axis='x')
    _box('bremswiderstand', (1.70, -1.15, -0.52), (3.40, -0.05, -0.13), M['under'], bevel=0.012)
    for k in range(9):
        z = -0.48 + k * 0.035
        ob = _box(f'lamelle{k}', (1.75, -0.012, -0.012), (3.35, 0.012, 0.012), M['under_brown'])
        ob.matrix_world = Matrix.Translation((0, -1.16, z)) @ Matrix.Rotation(0.6, 4, 'X')
    _box('kompressor', (3.60, 0.25, -0.56), (4.50, 1.15, -0.13), M['under'], bevel=0.015)
    for k in range(2):
        _cyl(f'zylinderkopf{k}', (3.85 + k * 0.4, 0.70, -0.56), (3.85 + k * 0.4, 0.70, -0.64), 0.09, M['bogie'], seg=12)
    for s in (-1, 1):
        _cyl(f'kabelkanal{s}', (-8.6, s * 0.62, -0.19), (8.6, s * 0.62, -0.19), 0.035, M['under'], seg=8)
    _cyl('hauptluftleitung', (-8.8, 0.95, -0.21), (8.8, 0.95, -0.21), 0.018, M['under_brown'], seg=6)


def _build_car(minus, plus, track, dz, seed):
    """Fahrender Metro-Wagen (Boden; Ursprung Mitte Wagenboden, 18 x 3 m entlang x; Bahnsteigseite -y mit offenen
    Tueren, Wandseite +y geschlossen; die Karte sperrt die Wandseite ohnehin). Wiki: der Wagen nimmt Ziele an - die
    Zielanzeigen innen an beiden Enden zeigen Bildzeichen (-x: minus, +x: plus)."""
    M = _M()
    rng = random.Random(seed)
    L = CAR_HL - END_T
    # --- Wagenkasten: Seitenwaende aus Baendern um Tueren und Fenster
    for s in (-1, 1):
        doors = [(d - DOOR_HW, d + DOOR_HW) for d in DOORS]
        _side_piece(f'schuerze{s}', -L, L, -0.46, 0.0, s, M, inner=M['under'])
        for k, (a, b) in enumerate(_intervals(-L, L, doors)):
            _side_piece(f'unten{s}_{k}', a, b, 0.0, WIN_Z0, s, M)
            _side_piece(f'ueber{s}_{k}', a, b, WIN_Z1, DOOR_H, s, M)
            for j, (c, e) in enumerate(_intervals(a, b, WINDOWS)):
                _side_piece(f'pfeiler{s}_{k}_{j}', c, e, WIN_Z0, WIN_Z1, s, M)
        _side_piece(f'oben{s}', -L, L, DOOR_H, SIDE[-1][1], s, M)
        # Regenrinne, Zierstreifen (rot) und Linie, Riffelung unten
        _cyl(f'rinne{s}', (-CAR_HL, s * (SIDE[-1][0] + 0.004), 2.18), (CAR_HL, s * (SIDE[-1][0] + 0.004), 2.18), 0.012, M['alu_dull'], seg=6)
        for k, (a, b) in enumerate(_intervals(-L, L, doors)):
            _box(f'streifen{s}_{k}', (a, min(s * 1.5, s * 1.505), 0.55), (b, max(s * 1.5, s * 1.505), 0.68), M['red'])
            _box(f'linie{s}_{k}', (a, min(s * 1.5, s * 1.503), 0.715), (b, max(s * 1.5, s * 1.503), 0.727), M['ink'])
            for r in range(5):
                z = 0.10 + r * 0.085
                poly = [(s * 1.5, z), (s * 1.509, z + 0.008), (s * 1.509, z + 0.03), (s * 1.5, z + 0.038)]
                _prism(f'riffel{s}_{k}_{r}', poly, a + 0.02, b - 0.02, [M['alu']], axis='x')
            # Abplatzer im roten Streifen an den Tuerkanten
            for e, edge in enumerate((a, b)):
                if rng.random() < 0.7:
                    cx = edge + (0.06 if e == 0 else -0.06)
                    _decal(f'abplatzer{s}_{k}_{e}', (cx, s * 1.5062, 0.6), (0, 0, 1), (1, 0, 0) if s > 0 else (-1, 0, 0),
                           _blob(rng, 0.025, 0.04, 7, 0.45), M['alu_dull'])
        # Fenster
        for (a, b) in WINDOWS:
            _window(f'fenster{s}_{a:.2f}', a, b, s, M, rng)
            # Schmutzfahnen unter den Fensterecken
            for xc in (a + 0.06, b - 0.06):
                if rng.random() < 0.75:
                    ln = rng.uniform(0.15, 0.32)
                    _decal(f'fahne{s}_{xc:.2f}', (xc, s * (_side_y(0.9) + 0.003), WIN_Z0 - 0.02), (0, 0, -1), (1, 0, 0) if s < 0 else (-1, 0, 0),
                           [(0, -0.012), (ln * 0.5, -0.008), (ln, 0.0), (ln * 0.5, 0.009), (0, 0.013)], M['grime'])
        # Tueren: Bahnsteigseite (-y) offen, Wandseite geschlossen
        for d in DOORS:
            _door(f'tuer{s}_{d:.0f}', d, s, M, rng, open_=(s < 0))
        # Schmutzrand unten (Bremsstaub), an den Tueren unter der Schwelle
        def ztop(x, s=s):
            v = -0.10 + 0.025 * math.sin(x * 1.37 + s) + 0.02 * math.sin(x * 3.91 + 2 * s) + 0.015 * math.sin(x * 8.3) + \
                rng.uniform(-0.02, 0.02)
            for d in DOORS:
                if abs(x - d) < DOOR_HW + 0.05:
                    v = min(v, -0.06)
            return max(-0.125, v)
        _profile_strip(f'bremsstaub{s}', -L, L, ztop, s, M['brake_dust'], dx=0.3)
        # Wasserlaeufe von der Regenrinne
        for k in range(7):
            xc = rng.uniform(-8.5, 8.5)
            if any(abs(xc - d) < 0.8 for d in DOORS):
                continue
            ln = rng.uniform(0.12, 0.30)
            zb = 2.17 - ln
            u = Vector((0, s * (_side_y(zb) - _side_y(2.17)), zb - 2.17)).normalized()
            _decal(f'lauf{s}_{k}', (xc, s * (_side_y(2.17) + 0.004), 2.17), u, (1, 0, 0) if s < 0 else (-1, 0, 0),
                   [(0, -0.01), (ln, -0.003), (ln, 0.003), (0, 0.012)], M['grime_dark'])
    # Dach (Schale ueber die ganze Laenge)
    outer = ROOF + [(-y, z) for (y, z) in reversed(ROOF[:-1])]
    inner = [(y * 0.965, z - 0.05) for (y, z) in reversed(outer)]
    _prism('dach', outer + inner, -CAR_HL, CAR_HL, [M['alu']], axis='x', smooth=30)
    # --- Stirnseiten
    _car_end('stirn_m', -1, M, rng, minus, front=True)
    _car_end('stirn_p', 1, M, rng, plus, front=False)
    # --- Innenraum
    _box('boden', (-L, -1.44, -0.12), (L, 1.44, 0.0), M['floor'])
    for d in DOORS:  # abgelaufene Flaechen an den Tueren (Bahnsteigseite staerker)
        _decal(f'lauf_tuer{d}', (d, -0.95, 0.001), (1, 0, 0), (0, 1, 0), _blob(rng, 0.75, 0.42, 11, 0.25), M['floor_worn'])
        _decal(f'lauf_tuer2{d}', (d, 0.0, 0.0012), (1, 0, 0), (0, 1, 0), _blob(rng, 0.5, 0.3, 9, 0.3), M['floor_worn'])
    _decal('lauf_mitte', (0, 0, 0.0008), (1, 0, 0), (0, 1, 0), [(-8.4, -0.18), (8.4, -0.2), (8.5, 0.0), (8.4, 0.2), (-8.4, 0.18), (-8.5, 0)],
           M['floor_worn'])
    benches = [(-L + 0.06, -6.0 - 0.88), (-6.0 + 0.88, -0.88), (0.88, 6.0 - 0.88), (6.0 + 0.88, L - 0.06)]
    for s in (-1, 1):
        for k, (a, b) in enumerate(benches):
            _bench(f'bank{s}_{k}', a, b, s, M, rng)
            _grab_rail(f'griff{s}_{k}', a + 0.08, b - 0.08, s, M)
            for (x, facing) in ((a, -1), (b, 1)):
                if abs(abs(x) - L) > 0.2:
                    _partition(f'trenn{s}_{k}_{facing}', x + facing * 0.015, s, M, facing)
    for d in DOORS:
        _cyl(f'mittelstange{d}', (d, 0.0, 0.0), (d, 0.0, 2.36), 0.019, M['steel'], seg=10)
        _cyl(f'mittelstange_fuss{d}', (d, 0.0, 0.0), (d, 0.0, 0.03), 0.045, M['chrome_worn'], seg=10)
    _ceiling(M, L)
    # Innen: Bruestungsleiste unter den Fenstern
    for s in (-1, 1):
        for k, (a, b) in enumerate(_intervals(-L, L, [(d - DOOR_HW, d + DOOR_HW) for d in DOORS])):
            _box(f'leiste{s}_{k}', (a, min(s * (1.5 - WALL_T), s * (1.5 - WALL_T - 0.03)), WIN_Z0 - 0.03),
                 (b, max(s * (1.5 - WALL_T), s * (1.5 - WALL_T - 0.03)), WIN_Z0), M['inner_grey'])
    # --- Unterboden, Drehgestelle
    _underframe(M, rng)
    for bx in BOGIES:
        _bogie(f'dg{bx:.0f}', bx, dz, M, rng)
    if track:  # Gleis unter dem Wagen (die Karte laesst unter dem Wagen 86..116 aus): Weltnull = -CAR_Z
        _track('gleis', -14.5, 15.5, 0.0, -2.0, 3.0, -CAR_Z, M, seed + 5, -14.2, detail=True, xs_simple=lambda x: abs(x) < 9.4,
               cell=0.2, stones=2.0, hidden=lambda x, y: abs(x) < 8.9 and abs(y) < 1.45)


def build_metro_car_903():
    """Fahrender Metro-Wagen in Level 903 (vorn Wellen = Level 37, hinten Flugzeug = Level 36), mit Gleis darunter."""
    _build_car('wave', 'plane', track=True, dz=0.0, seed=903)


def build_metro_car_37():
    """Derselbe Wagen, wie er in Level 37 haelt: vorn (in der Fliesenwand) dunkle Anzeige, hinten ein Wagen-Bildzeichen
    (zurueck zur Metro). Kein Gleis; die Raeder stehen knapp im Wasser (Drehgestelle 12 cm tiefer)."""
    _build_car(None, 'train', track=False, dz=-0.12, seed=37)


# =====================================================================================================================
# Gleis, Bahnsteigkante
# =====================================================================================================================
def build_track_metro():
    """Gleis (Boden, 6 m entlang x; 5 m quer von der Hallenwand bei +y bis zur Bahnsteigwand bei -y, Gleismitte unter
    der Wagenmitte bei y = -0.5): Schotterbett mit Einzelsteinen, Spannbetonschwellen (0.6 m), Rippenplatten,
    Spannklemmen und Schrauben, Schienen (blanker Kopf, rostiger Steg), Stromschiene mit Abdeckung auf Isolatorboecken,
    Kabelkanal mit Deckeln an der Wand."""
    _track('gleis', -3.0, 3.0, -0.5, -2.5, 2.5, 0.0, _M(), 1903, -2.7)


def build_track_metro__v1():
    """Gleis, Variante 1: andere Steinlage, staerkere Oelspur, Abfall im Schotter (Papier, Dose), verschobener Deckel."""
    M = _M()
    _track('gleis', -3.0, 3.0, -0.5, -2.5, 2.5, 0.0, M, 2903, -2.7, stones=5.0)
    rng = random.Random(29)
    for k in range(3):  # zerknuellte Papiere
        bmr = bmesh.new()
        bmesh.ops.create_icosphere(bmr, subdivisions=1, radius=0.035)
        for v in bmr.verts:
            v.co *= rng.uniform(0.6, 1.3)
            v.co.z *= 0.6
        x, y = rng.uniform(-2.6, 2.6), rng.choice((-2.2, -1.9, 1.0, 1.6))
        bmesh.ops.translate(bmr, verts=bmr.verts, vec=(x, y, Z_BALLAST + 0.02))
        _link(f'papier{k}', bmr, [M['paper']])
    _cyl('dose', (0.9, -2.15, Z_BALLAST - 0.005), (1.02, -2.10, Z_BALLAST + 0.02), 0.033, M['sticker_r'], seg=10)
    _decal('oelfleck', (-0.8, -0.5, Z_BALLAST + 0.028), (1, 0, 0), (0, 1, 0), _blob(rng, 0.45, 0.22, 11, 0.35), M['grime_dark'])


# --- Bahnsteigkante (392 Stueck je Ebene: sparsam) -----------------------------------------------------------------------------
def _studs(name, x0, x1, y0, y1, nx, ny, h, mat):
    """Noppenfeld als 'Eierkarton': je Noppe vier Dreiecke um eine erhabene Spitze (glatt schattiert = Kuppe)."""
    bm = bmesh.new()
    dx, dy = (x1 - x0) / nx, (y1 - y0) / ny
    low = [[bm.verts.new((x0 + i * dx, y0 + j * dy, 0.006)) for j in range(ny + 1)] for i in range(nx + 1)]
    for i in range(nx):
        for j in range(ny):
            p = bm.verts.new((x0 + (i + 0.5) * dx, y0 + (j + 0.5) * dy, 0.006 + h))
            for (a, b) in ((low[i][j], low[i + 1][j]), (low[i + 1][j], low[i + 1][j + 1]), (low[i + 1][j + 1], low[i][j + 1]),
                           (low[i][j + 1], low[i][j])):
                f = bm.faces.new((a, b, p))
                f.normal_update()
                if f.normal.z < 0:
                    f.normal_flip()
    # Kanten der Platte
    for (a, b) in ((low[0][0], low[nx][0]), (low[nx][0], low[nx][ny]), (low[nx][ny], low[0][ny]), (low[0][ny], low[0][0])):
        va, vb = bm.verts.new((a.co.x, a.co.y, 0.0)), bm.verts.new((b.co.x, b.co.y, 0.0))
        bm.faces.new((a, b, vb, va))
    bm.normal_update()
    for f in bm.faces:  # Seitenflaechen nach aussen
        if abs(f.normal.z) < 0.5:
            c = f.calc_center_median()
            out = Vector((c.x - (x0 + x1) / 2, c.y - (y0 + y1) / 2, 0))
            if f.normal.dot(out) < 0:
                f.normal_flip()
    return _link(name, bm, [mat])  # flach schattiert: die Facetten zeichnen die Noppen im Streiflicht


def _platform_edge(v):
    """Bahnsteigkante (Boden, 1 m entlang x; Bahnsteigkante bei y = -0.5, Bahnsteig nach +y; im Spiel steht die Kante so
    an der Bahnsteigwand): Kantenstein mit gerundeter Nase und 3 cm Ueberstand, Fuge, zwei gelbe Noppenplatten
    (Sicherheitsstreifen), Abnutzung. v: 0 gepflegt, 1 abgetreten (Riss, Kaugummi, matte Platte), 2 ausgebessert
    (neue Platte, Abplatzer an der Nase)."""
    M = _M()
    rng = random.Random(4100 + v)
    prof = [(-0.18, -0.03), (-0.18, 0.006), (-0.505, 0.006), (-0.518, 0.002), (-0.527, -0.007), (-0.530, -0.02), (-0.530, -0.14),
            (-0.50, -0.14)]
    if v == 2:  # Abplatzer an der Nase am rechten Ende
        _prism('kantenstein', prof, -0.497, 0.36, [M['coping']], axis='x', smooth=40)
        chip = [(-0.18, -0.03), (-0.18, 0.006), (-0.49, 0.006), (-0.505, -0.012), (-0.512, -0.05), (-0.530, -0.07), (-0.530, -0.14),
                (-0.50, -0.14)]
        _prism('kantenstein_abplatzer', chip, 0.36, 0.497, [M['coping']], axis='x', smooth=40)
    else:
        _prism('kantenstein', prof, -0.497, 0.497, [M['coping']], axis='x', smooth=40)
    # Abrieb an der Kante: dunkle, glattgetretene Zone; Schmutzlinie an der Nase
    _decal('kante_abrieb', (0, 0, 0.0062), (1, 0, 0), (0, 1, 0),
           [(-0.497, -0.505), (0.497, -0.505), (0.497, -0.47 + 0.01 * v), (0.2, -0.46), (-0.2, -0.475), (-0.497, -0.465)], M['grime'])
    # Noppenplatten (zwei je Meter), Fuge dazwischen
    for k, (a, b) in enumerate(((-0.497, -0.003), (0.003, 0.497))):
        mat = M['tactile']
        if v == 1 and k == 0:
            mat = M['tactile_worn']
        if v == 2 and k == 1:
            mat = common.material('noppenplatte_neu', (0.95, 0.76, 0.12), look='plastic', rough=0.6, r=1.0)
        _studs(f'noppen{k}', a, b, -0.165, 0.125, 6, 3, 0.0075, mat)
    if v == 1:
        crack = [(-0.12, -0.22), (-0.10, -0.30), (-0.13, -0.38), (-0.11, -0.47), (-0.115, -0.505)]
        _decal('riss', (0, 0, 0.0063), (1, 0, 0), (0, 1, 0), _ribbon(crack, 0.004), M['joint'])
        for k in range(3):
            _decal(f'kaugummi{k}', (rng.uniform(-0.4, 0.4), rng.uniform(-0.43, -0.24), 0.0064), (1, 0, 0), (0, 1, 0),
                   _blob(rng, 0.012, 0.011, 7, 0.25), M['grime_dark'])


def build_platform_edge():
    _platform_edge(0)


def build_platform_edge__v1():
    _platform_edge(1)


def build_platform_edge__v2():
    _platform_edge(2)


# --- Leuchtstoffroehre (76 Stueck) ---------------------------------------------------------------------------------------------
def _tube_long(v):
    """Lange, duenne Leuchte (Decke, Oberseite z = 0, 3 m entlang x; Wiki: lange duenne Leuchtstoffroehren entlang der Wege):
    Deckenrosetten, Seile, schmales Aluprofil mit Endkappen und Vorschaltgeraet, Fassungen, zwei T8-Roehren mit
    Endkappen. v: 0 neu, 1 gealtert (geschwaerzte Enden, waermeres Licht), 2 eine Roehre tot."""
    M = _M()
    tube_a = M['tube']
    tube_b = M['tube']
    if v == 1:
        tube_b = common.material('roehre_alt', (1.0, 0.93, 0.80), look='glow', glow=1.7)
    if v == 2:
        tube_b = common.material('roehre_tot', (0.55, 0.56, 0.55), look='glass', rough=0.35, r=0.6)
    for x in (-1.0, 1.0):
        _lathe(f'rosette{x}', [(0.035, -0.012), (0.034, -0.004), (0.03, 0.0)], [M['housing']], seg=10, center=(x, 0, 0))
        _cyl(f'seil{x}', (x, 0, -0.012), (x, 0, -0.05), 0.0022, M['steel'], seg=4, caps=False)
        _box(f'seilhalter{x}', (x - 0.012, -0.008, -0.056), (x + 0.012, 0.008, -0.048), M['steel'])
    prof = [(-0.025, -0.050), (0.025, -0.050), (0.025, -0.085), (0.0215, -0.085), (0.0215, -0.0535), (-0.0215, -0.0535),
            (-0.0215, -0.085), (-0.025, -0.085)]
    _prism('profil', prof, -1.47, 1.47, [M['housing']], axis='x')
    _box('vorschaltgeraet', (-0.32, -0.018, -0.068), (0.32, 0.018, -0.0535), M['inner_grey'])
    for s in (-1, 1):
        _box(f'endkappe{s}', (s * 1.47, -0.026, -0.086), (s * 1.50, 0.026, -0.049), M['socket'], bevel=0.006)
    for x in (-1.455, -0.02, 0.02, 1.455):
        _box(f'fassung{x}', (x - 0.012, -0.014, -0.102), (x + 0.012, 0.014, -0.085), M['socket'])
    for k, (a, b, mat) in enumerate(((-1.443, -0.032, tube_a), (0.032, 1.443, tube_b))):
        _cyl(f'roehre{k}', (a + 0.018, 0, -0.098), (b - 0.018, 0, -0.098), 0.013, mat, seg=10, caps=False)
        for (e, d) in ((a, 1), (b, -1)):
            _cyl(f'roehre_kappe{k}{e:.2f}', (e, 0, -0.098), (e + d * 0.018, 0, -0.098), 0.0135, M['chrome_worn'], seg=10)
            dark = 0.035 if (v == 1 and k == 1) else (0.012 if v == 0 else 0.022)
            _cyl(f'roehre_schwarz{k}{e:.2f}', (e + d * 0.018, 0, -0.098), (e + d * (0.018 + dark), 0, -0.098), 0.01305,
                 M['tube_end'], seg=10, caps=False)


def build_tube_long():
    _tube_long(0)


def build_tube_long__v1():
    _tube_long(1)


def build_tube_long__v2():
    _tube_long(2)


# --- Haengende Leuchtschilder -----------------------------------------------------------------------------------------------
def _hanging_sign(seed, arrow, picto):
    """Haengendes Leuchtschild (Decke bei z = 0, beidseitig, 2.2 m entlang y): Gehaeuse aus gefastem Aluprofil, Leuchtflaeche
    beidseitig, erfundene Zeichen (keine echte Schrift), Linienfeld, Pfeil (a) oder Bildzeichen Wagen (b), zwei Gewindestangen
    mit Deckenplatten, Kabelschlauch, tote Insekten unten in der Leuchtflaeche."""
    M = _M()
    rng = random.Random(seed)
    prof = [(-0.068, -0.985), (0.068, -0.985), (0.075, -0.978), (0.075, -0.582), (0.068, -0.575), (-0.068, -0.575), (-0.075, -0.582),
            (-0.075, -0.978)]
    _prism('gehaeuse', [(x, z) for (x, z) in prof], -1.10, 1.10, [M['sign_case']], axis='y', smooth=30)
    word = _word(rng, 5, 6)
    for sg in (-1, 1):
        x = sg * 0.0756
        U = (0, sg, 0)  # Leserichtung (nach rechts) fuer den Betrachter vor dieser Seite; Normale nach aussen
        _decal(f'licht{sg}', (x, 0, 0), U, (0, 0, 1), [(-1.06, -0.95), (1.06, -0.95), (1.06, -0.61), (-1.06, -0.61)], M['sign_glow'])
        xi = x + sg * 0.0008
        # Linienfeld links (blau mit hellem Ring), Zeichen, rechts Pfeil (a) oder Bildzeichen Wagen (b)
        _decal(f'linie{sg}', (xi, 0, -0.78), U, (0, 0, 1), [(-0.98, -0.13), (-0.72, -0.13), (-0.72, 0.13), (-0.98, 0.13)], M['sticker_b'])
        for k in range(14):
            a0, a1 = TAU * k / 14, TAU * (k + 1) / 14
            _decal(f'ring{sg}_{k}', (xi + sg * 0.0005, 0, -0.78), U, (0, 0, 1),
                   [(-0.85 + 0.064 * math.cos(a0), 0.064 * math.sin(a0)), (-0.85 + 0.086 * math.cos(a0), 0.086 * math.sin(a0)),
                    (-0.85 + 0.086 * math.cos(a1), 0.086 * math.sin(a1)), (-0.85 + 0.064 * math.cos(a1), 0.064 * math.sin(a1))], M['enamel_w'])
        _text(f'zeichen{sg}', word, (xi, -0.62 * sg, -0.855), U, (0, 0, 1), 0.15, M['ink'])
        if arrow:
            pts = [(0.72, -0.022), (0.86, -0.022), (0.86, -0.065), (0.95, 0.0), (0.86, 0.065), (0.86, 0.022), (0.72, 0.022)]
            _decal(f'pfeil{sg}', (xi, 0, -0.78), U, (0, 0, 1), pts, M['ink'])
        if picto:
            _pictogram(f'wagen{sg}', 'train', (xi, 0.84 * sg, -0.78), U, (0, 0, 1), 0.27, M['ink'], M['sign_glow'])
        for k in range(rng.randint(2, 4)):  # tote Insekten am unteren Rand der Streuscheibe
            _decal(f'insekt{sg}_{k}', (xi, rng.uniform(-1.0, 1.0), -0.94), U, (0, 0, 1), _blob(rng, 0.008, 0.005, 6, 0.3), M['grime_dark'])
    for s in (-1, 1):
        y = s * 0.85
        _cyl(f'platte{s}', (0, y, 0.0), (0, y, -0.008), 0.045, M['sign_case'], seg=12)
        _cyl(f'stange{s}', (0, y, -0.008), (0, y, -0.575), 0.0055, M['steel'], seg=6, caps=False)
        _box(f'lasche{s}', (-0.02, y - 0.025, -0.59), (0.02, y + 0.025, -0.565), M['sign_case'])
        for zz in (-0.05, -0.56):
            _cyl(f'mutter{s}{zz}', (0, y, zz + 0.006), (0, y, zz - 0.006), 0.011, M['steel'], seg=6)
    _tube('kabel', [(0.0, 0.30, 0.0), (0.02, 0.32, -0.15), (0.035, 0.28, -0.35), (0.02, 0.18, -0.52), (0.0, 0.12, -0.575)], 0.008, M['cable'],
          seg=5)


def build_sign_903_a():
    """Haengendes Leuchtschild mit erfundenen Zeichen (Wiki: leuchtende Schilder in unbekannter Sprache) und Pfeil."""
    _hanging_sign(903, True, False)


def build_sign_903_b():
    """Haengendes Leuchtschild mit erfundenen Zeichen und Bildzeichen Wagen."""
    _hanging_sign(1903, False, True)


# --- Stationstafel an der Wand (Emaille) ------------------------------------------------------------------------------------
def _wall_sign(v):
    """Stationstafel (Wand, Rueckseite x = 0, 4.1 x 0.76 m, Mitte im Ursprung): Emailleschild dunkelblau mit weissem Rand und
    weissen erfundenen Zeichen (Stationsname - auf allen Tafeln gleich), Aluminiumrahmen, sechs Abstandsbolzen,
    Abplatzer an Kanten und Bolzen mit Rostfahnen. v1: staerker verwittert (Rostlauf, Schmutzfilm, mehr Abplatzer)."""
    M = _M()
    rng = random.Random(9031 + v * 7)
    W, H = 2.0, 0.33
    _box('tafel', (0.022, -W, -H), (0.030, W, H), M['enamel'])
    # Rahmen: L-Profil rundum
    for (a0, a1, z0, z1) in ((-W - 0.03, W + 0.03, H, H + 0.03), (-W - 0.03, W + 0.03, -H - 0.03, -H)):
        _prism(f'rahmen{z0:.2f}', [(0.0, z0), (0.034, z0), (0.034, z1), (0.0, z1)], a0, a1, [M['chrome_worn']], axis='y')
    for (y0, y1) in ((-W - 0.03, -W), (W, W + 0.03)):
        _prism(f'rahmen_s{y0:.2f}', [(0.0, y0), (0.034, y0), (0.034, y1), (0.0, y1)], -H, H, [M['chrome_worn']], axis='z')
    # weisse Randlinie
    for (pts) in (((-W + 0.04, H - 0.05), (W - 0.04, H - 0.05), (W - 0.04, H - 0.035), (-W + 0.04, H - 0.035)),
                  ((-W + 0.04, -H + 0.035), (W - 0.04, -H + 0.035), (W - 0.04, -H + 0.05), (-W + 0.04, -H + 0.05)),
                  ((-W + 0.035, -H + 0.035), (-W + 0.05, -H + 0.035), (-W + 0.05, H - 0.035), (-W + 0.035, H - 0.035)),
                  ((W - 0.05, -H + 0.035), (W - 0.035, -H + 0.035), (W - 0.035, H - 0.035), (W - 0.05, H - 0.035))):
        _decal(f'rand{pts[0][0]:.2f}{pts[0][1]:.2f}', (0.0302, 0, 0), (0, 1, 0), (0, 0, 1), list(pts), M['enamel_w'])
    word = _word(random.Random(9031), 7, 7)
    n = len(word)
    h = 0.34
    width = n * h * 0.82 * 1.25 - h * 0.82 * 0.25
    _text('name', word, (0.0303, -width / 2, -h / 2), (0, 1, 0), (0, 0, 1), h, M['enamel_w'])
    # Bolzen
    for y in (-W + 0.1, 0.0, W - 0.1):
        for z in (-H + 0.06, H - 0.06):
            _cyl(f'bolzen{y:.1f}{z:.1f}', (0.0, y, z), (0.0335, y, z), 0.011, M['steel'], seg=6)
            if rng.random() < (0.5 + 0.3 * v):
                _decal(f'bolzenrost{y:.1f}{z:.1f}', (0.0304, y, z - 0.012), (0, 0, -1), (0, 1, 0),
                       [(0, -0.008), (0.05 + 0.08 * rng.random(), -0.003), (0.05 + 0.08 * rng.random(), 0.003), (0, 0.009)], M['rust_streak'])
    # Abplatzer (Emaille weg, schwarzer Stahl/Rost) an Kanten und Ecken
    for k in range(5 + 6 * v):
        edge = rng.choice(('o', 'u', 'l', 'r'))
        if edge in 'ou':
            y, z = rng.uniform(-W + 0.05, W - 0.05), (H - 0.012) if edge == 'o' else (-H + 0.012)
        else:
            y, z = (-W + 0.012) if edge == 'l' else (W - 0.012), rng.uniform(-H + 0.05, H - 0.05)
        _decal(f'abplatzer{k}', (0.0304, y, z), (0, 1, 0), (0, 0, 1), _blob(rng, rng.uniform(0.01, 0.03), rng.uniform(0.008, 0.02), 8, 0.4),
               M['enamel_chip'])
    if v == 1:
        _decal('rostlauf', (0.0305, 0.7, H - 0.01), (0, 0, -1), (0, 1, 0), [(0, -0.02), (0.35, -0.006), (0.45, 0.0), (0.35, 0.008), (0, 0.025)],
               M['rust_streak'])
        _decal('schmutzfilm', (0.0303, 0, 0), (0, 1, 0), (0, 0, 1),
               [(-W + 0.06, -H + 0.055), (W - 0.06, -H + 0.055), (W - 0.06, -H + 0.09), (0.4, -H + 0.11), (-0.8, -H + 0.085),
                (-W + 0.06, -H + 0.10)], M['grime'])


def build_sign_903_wall():
    _wall_sign(0)


def build_sign_903_wall__v1():
    _wall_sign(1)


# --- Treppe, zugemauert und vernagelt ---------------------------------------------------------------------------------------
def build_stairs_barricaded():
    """Treppe nach oben (Boden, Ursprung am Fuss vor den Brettern, Treppe entlang +x, 6.1 x 2.6 m; Wiki: Treppen tauchen
    auf, sind aber immer versperrt oder zugemauert): Brüstungswaende mit Fliesen und Abdeckstein bis zur Decke, Stufen aus
    Terrazzo mit Kantenschutz, Handlaeufe auf Haltern, oben eine eilig hochgezogene Ziegelwand (einzelne Steine, Moertel
    quillt), unten zwei Pfosten mit genagelten Brettern (Naegel, krumme Naegel), Zettel, Absperrband, Schutt."""
    M = _M()
    rng = random.Random(4)
    yw = 1.18
    RISE, RUN, X0 = 0.17, 0.30, 0.40
    # Brüstungswaende
    wall = [(0.10, 0.0), (6.10, 0.0), (6.10, 4.20), (5.15, 4.20), (X0, 1.03), (0.10, 1.03)]
    for s in (-1, 1):
        _prism(f'wand{s}', wall, s * yw, s * 1.30, [M['tile_w']], axis='y')
        cap = [(0.07, 1.03), (X0, 1.03), (5.15, 4.20), (5.08, 4.20), (X0 - 0.01, 1.075), (0.07, 1.075)]
        _prism(f'abdeckung{s}', cap, s * (yw - 0.02), s * 1.32, [M['coping']], axis='y')
        _decal(f'sockel{s}', (0.0, s * (yw - 0.0015), 0.0), (1, 0, 0), (0, 0, 1),
               [(0.10, 0.0), (6.1, 0.0), (6.1, 0.12), (0.10, 0.12)], M['grime'])
    # Stufen (Saegezahnprofil) bis unter die Ziegelwand
    XB = 3.55
    pts = [(X0, 0.0)]
    k = 0
    while X0 + k * RUN < XB + 0.3:
        x = X0 + k * RUN
        pts += [(x, (k + 1) * RISE), (x + RUN, (k + 1) * RISE)]
        k += 1
    pts.append((pts[-1][0], 0.0))
    _prism('stufen', pts, -yw, yw, [M['step']], axis='y')
    nsteps = k
    for j in range(nsteps):
        x = X0 + j * RUN
        z = (j + 1) * RISE
        _box(f'kante{j}', (x - 0.012, -yw + 0.03, z - 0.012), (x + 0.045, yw - 0.03, z + 0.003), M['chrome_worn'])
        _box(f'kante_gummi{j}', (x + 0.008, -yw + 0.05, z + 0.003), (x + 0.03, yw - 0.05, z + 0.005), M['rubber'])
        for s in (-1, 1):  # Staub in den Ecken
            _decal(f'staub{j}{s}', (x + 0.15, s * (yw - 0.12), z + 0.0055), (1, 0, 0), (0, 1, 0), _blob(rng, 0.14, 0.09, 9, 0.35),
                   M['step_dirty'])
    # Handlaeufe
    for s in (-1, 1):
        y = s * (yw - 0.075)
        path = [(0.15, y, 0.90 + RISE), (X0, y, 0.90 + RISE), (XB - 0.05, y, (XB - 0.05 - X0) / RUN * RISE + 0.90 + RISE)]
        _tube(f'handlauf{s}', path, 0.021, M['steel'], seg=8)
        for x in (0.25, 1.4, 2.6):
            z = 0.90 + RISE if x < X0 else (x - X0) / RUN * RISE + 0.90 + RISE
            _tube(f'halter{s}{x}', [(x, y, z - 0.02), (x, y, z - 0.07), (x, s * (yw - 0.002), z - 0.07)], 0.009, M['steel'], seg=6)
            _cyl(f'rosette{s}{x}', (x, s * yw, z - 0.07), (x, s * (yw - 0.008), z - 0.07), 0.03, M['steel'], seg=10)
    # Ziegelwand: einzelne Steine im Laeuferverband, Moertel dahinter, oben unregelmaessig
    _box('moertel', (XB + 0.03, -yw, 1.7), (XB + 0.25, yw, 4.2), M['mortar'])
    bricks = [M['brick'], common.material('ziegel_rot', (0.52, 0.30, 0.22), look='concrete', tex='bricks_red_factory', rough=0.9, r=1.2),
              common.material('ziegel_dunkel', (0.36, 0.27, 0.22), look='concrete', tex='bricks_dirty_brown', rough=0.9, r=1.25)]
    z = (int((XB - X0) / RUN) + 1) * RISE
    row = 0
    while z < 4.18:
        off = 0.0 if row % 2 == 0 else 0.125
        y = -yw - off
        while y < yw - 0.01:
            a, b = max(y, -yw), min(y + 0.24, yw)
            if b - a > 0.04:
                dx = rng.uniform(-0.008, 0.006)
                ob = _box(f'ziegel{row}_{y:.2f}', (XB + dx, a + 0.005, z + 0.005), (XB + 0.115 + dx, b - 0.005, min(4.2, z + 0.072)),
                          bricks[rng.choice((0, 0, 0, 1, 2))])
                _jitter(ob, 0.003, rng.randrange(10 ** 6))
                if rng.random() < 0.18:  # herausgequollener Moertel
                    _box(f'quill{row}_{y:.2f}', (XB - 0.006, a + rng.uniform(0.02, 0.1), z - 0.004), (XB + 0.01, a + rng.uniform(0.12, 0.22), z + 0.012),
                         M['mortar'])
            y += 0.25
        z += 0.0775
        row += 1
    # Barrikade am Fuss: Pfosten, Bretter, Naegel
    for s in (-1, 1):
        _box(f'pfosten{s}', (-0.12, s * (yw - 0.10), 0.0), (-0.02, s * (yw - 0.002), 1.95), M['wood_c'])
        for z in (0.3, 1.5):
            _box(f'winkel{s}{z}', (-0.06, s * (yw - 0.004), z), (0.10, s * (yw + 0.002), z + 0.06), M['nail'])
    boards = []
    z = 0.18
    while z < 1.85:
        h = rng.uniform(0.14, 0.20)
        tilt = rng.uniform(-0.05, 0.05)
        ext = rng.uniform(0.0, 0.10)
        boards.append((z + h / 2, h, tilt, ext))
        z += h + rng.uniform(0.02, 0.09)
    woods = [M['wood_a'], M['wood_b'], M['wood_c']]
    for i, (zc, h, tilt, ext) in enumerate(boards):
        ob = _box(f'brett{i}', (-0.012, -yw - ext + rng.uniform(0, 0.05), -h / 2), (0.012, yw + 0.04 - rng.uniform(0, 0.06), h / 2),
                  woods[i % 3])
        _jitter(ob, 0.004, i * 31)
        ob.matrix_world = Matrix.Translation((-0.132, 0, zc)) @ Matrix.Rotation(tilt, 4, 'X')
        for s in (-1, 1):  # zwei Naegel je Pfosten
            for dz in (-h * 0.25, h * 0.25):
                yy = s * (yw - 0.05)
                zz = zc + dz + yy * math.tan(tilt) * 0.0
                zz = zc + dz + yy * tilt
                _cyl(f'nagel{i}{s}{dz:.2f}', (-0.149, yy, zz), (-0.144, yy, zz), 0.0065, M['nail'], seg=6)
    # zwei Diagonalbretter (X), ein krummer Nagel
    for i, sgn in enumerate((1, -1)):
        ang = math.atan2(1.4, 2.2) * sgn
        ob = _box(f'diagonal{i}', (-0.012, -1.30, -0.085), (0.012, 1.30, 0.085), woods[(i + 1) % 3])
        _jitter(ob, 0.004, 77 + i)
        ob.matrix_world = Matrix.Translation((-0.157 - i * 0.026, 0, 0.98)) @ Matrix.Rotation(ang, 4, 'X')
    _tube('krummer_nagel', [(-0.144, 0.42, 1.12), (-0.184, 0.42, 1.12), (-0.199, 0.43, 1.10), (-0.201, 0.45, 1.08)], 0.0025, M['nail'], seg=4)
    # Zettel mit erfundenen Zeichen, Absperrband
    _decal('zettel', (-0.1446, 0.10, 1.18), (0, -1, 0), (0, 0, 1), [(0, 0), (0.21, 0.01), (0.215, 0.30), (0.0, 0.29)], M['paper'])
    _text('zettel_text', _word(rng, 3, 4), (-0.1449, 0.08, 1.38), (0, -1, 0), (0, 0, 1), 0.045, M['ink'])
    _text('zettel_text2', _word(rng, 4, 5), (-0.1449, 0.08, 1.30), (0, -1, 0), (0, 0, 1), 0.03, M['ink'])
    tape = []
    for k in range(9):
        t = k / 8
        tape.append((-0.20, -1.10 + 2.2 * t, 1.62 - 0.16 * math.sin(math.pi * t)))
    _tube('band', tape, 0.008, M['tape'], seg=4)
    # Schutt hinter den Brettern
    for k in range(9):
        ob = _box(f'schutt{k}', (0, 0, 0), (rng.uniform(0.05, 0.12), rng.uniform(0.04, 0.11), rng.uniform(0.03, 0.06)), bricks[k % 3])
        _jitter(ob, 0.01, k)
        xs = rng.uniform(0.30, 0.68)
        ob.matrix_world = Matrix.Translation((xs, rng.uniform(-1.0, 1.0), 0.0 if xs < X0 - 0.06 else RISE)) @ \
            Matrix.Rotation(rng.uniform(0, TAU), 4, 'Z')
    _decal('staub_fuss', (0.33, 0.0, 0.002), (1, 0, 0), (0, 1, 0), _blob(rng, 0.065, 1.0, 12, 0.2), M['step_dirty'])


# --- Ruhender Zug (Fassade) ---------------------------------------------------------------------------------------------------
# Profil der Wagenseite: x = Abstand von der Rueckseite (Blockkante) zum Bahnsteig, z = Hoehe ueber dem Bahnsteig
DSIDE = [(0.07, -0.50), (0.105, -0.44), (0.118, -0.25), (0.12, -0.05), (0.12, 0.92), (0.108, 1.95), (0.096, 2.22), (0.075, 2.36),
         (0.045, 2.50), (0.015, 2.62), (0.0, 2.68)]
D_WIN = (0.92, 1.86)


def _dx(z):
    if z <= DSIDE[0][1]:
        return DSIDE[0][0]
    for (x0, z0), (x1, z1) in zip(DSIDE, DSIDE[1:]):
        if z0 <= z <= z1:
            return x0 + (x1 - x0) * ((z - z0) / (z1 - z0) if z1 > z0 else 0.0)
    return DSIDE[-1][0]


def _dpiece(name, y0, y1, z0, z1, mat, M, t=0.04):
    zs = [z0] + [z for (_, z) in DSIDE if z0 + 1e-6 < z < z1 - 1e-6] + [z1]
    outer = [(_dx(z), z) for z in zs]
    inner = [(max(0.0, x - t), z) for (x, z) in reversed(outer)]
    poly = [(x, z) for (x, z) in outer + inner]
    edge = [0] * len(poly)
    no = len(outer)
    edge[no - 1] = 1
    edge[-1] = 1
    _prism(name, poly, y0, y1, [mat, M['alu_dull']], axis='y', edge_mat=edge, smooth=25)


def _dormant(kind, seed, worn=False):
    """6-m-Abschnitt der Seitenwand des ruhenden Zugs (Wand, Rueckseite x = 0, Blick +x, z = 0 = Bahnsteig): aelterer Wagen,
    grauer Lack mit blauem Streifen, dunkle Fenster mit Spiegelung, eine geschlossene Doppeltuer, Regenrinne, verblasste
    Aufkleber, Rostfahnen. kind: 'a' Fenster mit Kippfluegel, 'b' Fenster mit Mittelsprosse und gesprungener Tuerscheibe,
    'home' bewohnt (Vorhaenge, Licht durch den Spalt, Tuer einen Spalt offen, Kisten, Kabel, Pflanze, Schild)."""
    M = _M()
    rng = random.Random(seed)
    home = kind == 'home'
    paint = M['paint_old']
    wins = ((-2.70, -1.05), (1.05, 2.70))
    door = (-0.65, 0.65)
    # Wand in Baendern um Fenster und Tuer
    _dpiece('schuerze', -3.0, 3.0, -0.50, 0.0, paint, M)
    for (a, b) in _intervals(-3.0, 3.0, [door]):
        _dpiece(f'unten{a:.2f}', a, b, 0.0, D_WIN[0], paint, M)
        _dpiece(f'ueber{a:.2f}', a, b, D_WIN[1], 1.98, paint, M)
        for (c, e) in _intervals(a, b, wins):
            _dpiece(f'pfeiler{c:.2f}', c, e, D_WIN[0], D_WIN[1], paint, M)
    _dpiece('oben', -3.0, 3.0, 1.98, 2.68, paint, M)
    # Streifen (auf allen Abschnitten gleich: der Zug laeuft durch), Zierlinie, Regenrinne, Wagenkasten-Naehte
    _box('streifen', (_dx(0.6) - 0.001, -3.0, 0.52), (_dx(0.6) + 0.004, 3.0, 0.70), M['blue_faded'])
    _box('linie', (_dx(0.75) - 0.001, -3.0, 0.74), (_dx(0.75) + 0.003, 3.0, 0.755), M['blue_faded'])
    _cyl('rinne', (_dx(2.22) + 0.012, -3.0, 2.22), (_dx(2.22) + 0.012, 3.0, 2.22), 0.014, M['alu_dull'], seg=6)
    for y in (-2.9, 2.9):
        _box(f'naht{y}', (_dx(1.0), y - 0.004, -0.45), (_dx(1.0) + 0.003, y + 0.004, 2.2), M['grime_dark'])
    # Zierleiste unter den Fenstern und Blechstoesse mit Nietreihen (flache Koepfe) im unteren Band
    for (a, b) in _intervals(-3.0, 3.0, [door]):
        _prism(f'zierleiste{a:.2f}', [(_dx(0.88) - 0.002, 0.875), (_dx(0.88) + 0.008, 0.88), (_dx(0.88) + 0.008, 0.90), (_dx(0.88) - 0.002, 0.905)],
               a, b, [M['alu_dull']], axis='y')
    for y in (-2.25, -1.45, 1.45, 2.25):
        _box(f'stoss{y}', (_dx(0.4), y - 0.002, 0.02), (_dx(0.4) + 0.0025, y + 0.002, 0.50), M['grime_dark'])
        for k in range(6):
            z = 0.06 + k * 0.085
            _cyl(f'niet{y}{k}', (_dx(z), y + 0.018, z), (_dx(z) + 0.004, y + 0.018, z), 0.006, paint, seg=5)
    if worn:  # staerker verschmutzt: Staub auf allen Scheiben, Laufspuren unter dem Streifen, Aufkleberreste
        for k in range(5):
            yc = rng.uniform(-2.8, 2.8)
            ln = rng.uniform(0.2, 0.45)
            _decal(f'laufspur{k}', (_dx(0.3) + 0.0045, yc, 0.50), (0, 0, -1), (0, 1, 0),
                   [(0, -0.012), (ln * 0.6, -0.007), (ln, 0.0), (ln * 0.6, 0.006), (0, 0.01)], M['grime'])
        for k in range(2):
            y, z = rng.uniform(-2.6, 2.4), rng.uniform(0.2, 0.42)
            _decal(f'aufkleberrest{k}', (_dx(z) + 0.0035, y, z), (0, 1, 0), (0, 0, 1), _blob(rng, 0.07, 0.04, 9, 0.4), M['sticker_w'])
    # Fenster
    for (a, b) in wins:
        rc = 0.08
        for (cx, cz, a0) in ((a + rc, D_WIN[0] + rc, 180), (b - rc, D_WIN[0] + rc, 270), (b - rc, D_WIN[1] - rc, 0), (a + rc, D_WIN[1] - rc, 90)):
            corner = (cx + rc * math.cos(math.radians(a0 + 45)) * 1.4142, cz + rc * math.sin(math.radians(a0 + 45)) * 1.4142)
            arc = [(cx + rc * math.cos(math.radians(a0 + 90 - 22.5 * k)), cz + rc * math.sin(math.radians(a0 + 90 - 22.5 * k)))
                   for k in range(5)]
            xo = _dx(cz)
            _prism(f'ecke{a:.1f}{a0}', [(y, z) for (y, z) in [corner] + arc], xo - 0.04, xo + 0.001, [paint], axis='x')
        path = _rrect(a, b, D_WIN[0], D_WIN[1], rc, n=3)
        _tube(f'dichtung{a:.1f}', [(_dx(z) + 0.003, y, z) for (y, z) in path + [path[0]]], 0.012, M['rubber'], seg=5, caps=False)
        xg = _dx(1.4) - 0.025
        if home:
            # Hauptscheibe offen; Vorhaenge mit Falten innen, warmer Spalt dahinter; oben Kippfluegel
            gap = rng.uniform(0.06, 0.14)
            mid = (a + b) / 2 + rng.uniform(-0.2, 0.2)
            _box(f'innenlicht{a:.1f}', (-0.32, a, D_WIN[0]), (-0.30, b, D_WIN[1]), common.material('spalt', (1.0, 0.70, 0.42), look='glow', glow=1.1))
            cloth = common.material('vorhang', (0.50, 0.22, 0.13), look='fabric', rough=0.9, r=1.2)
            for (c0, c1) in ((a + 0.03, mid - gap / 2), (mid + gap / 2, b - 0.03)):
                bm = bmesh.new()
                ny = max(4, int((c1 - c0) / 0.035))
                cols = []
                for i in range(ny + 1):
                    y = c0 + (c1 - c0) * i / ny
                    xx = 0.035 + 0.018 * math.sin(i * math.pi * 0.5)
                    cols.append([bm.verts.new((xx, y, D_WIN[0] + 0.02)), bm.verts.new((0.04 + 0.006 * math.sin(i * math.pi * 0.5), y, D_WIN[1] - 0.06))])
                for i in range(ny):
                    bm.faces.new((cols[i][0], cols[i + 1][0], cols[i + 1][1], cols[i][1]))
                bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
                for f in bm.faces:
                    f.normal_update()
                    if f.normal.x < 0:
                        f.normal_flip()
                ob = _link(f'vorhang{a:.1f}{c0:.2f}', bm, [cloth])
                _smooth(ob, 80)
            _box(f'blende{a:.1f}', (0.055, a - 0.01, D_WIN[1] - 0.09), (0.075, b + 0.01, D_WIN[1] + 0.01), cloth)
            for k in range(int((b - a) / 0.08)):
                _box(f'blende_falte{a:.1f}{k}', (0.075, a + 0.03 + k * 0.08, D_WIN[1] - 0.09), (0.079, a + 0.05 + k * 0.08, D_WIN[1] - 0.005),
                     common.material('vorhang_falte', (0.36, 0.15, 0.09), look='fabric', rough=0.9, r=1.2))
        else:
            _decal(f'glas{a:.1f}', (xg, 0, 0), (0, 1, 0), (0, 0, 1), _rrect(a, b, D_WIN[0], D_WIN[1], rc, n=3), M['glass'])
            hd = 0.25 if worn else 0.0
            _decal(f'glas_staub{a:.1f}', (xg + 0.001, 0, 0), (0, 1, 0), (0, 0, 1),
                   [(a + 0.05, D_WIN[0] + 0.02), (b - 0.05, D_WIN[0] + 0.02), (b - 0.05, D_WIN[0] + 0.07 + hd + rng.uniform(0, 0.05)),
                    ((a + b) / 2, D_WIN[0] + 0.12 + hd * 1.2), (a + 0.05, D_WIN[0] + 0.06 + hd * 0.8)], M['glass_dirty'])
        if kind == 'b':
            _box(f'sprosse{a:.1f}', (xg - 0.01, (a + b) / 2 - 0.025, D_WIN[0]), (xg + 0.012, (a + b) / 2 + 0.025, D_WIN[1]), M['alu_dull'])
        if kind == 'a' or home:  # Kippfluegel oben
            zt = D_WIN[1] - 0.30
            _box(f'kaempfer{a:.1f}', (xg - 0.012, a, zt - 0.02), (xg + 0.012, b, zt), M['alu_dull'])
            if home:
                ob = _decal(f'kipp{a:.1f}', (0, 0, 0), (0, 1, 0), (0, 0, 1), [(a + 0.04, 0.0), (b - 0.04, 0.0), (b - 0.04, 0.26), (a + 0.04, 0.26)],
                            M['glass'])
                ob.matrix_world = Matrix.Translation((xg, 0, zt)) @ Matrix.Rotation(math.radians(-14), 4, 'Y')
        # Rostfahnen unter den Fensterecken
        for yc in (a + 0.07, b - 0.07):
            if rng.random() < 0.8:
                ln = rng.uniform(0.18, 0.40)
                _decal(f'rost{yc:.2f}', (_dx(0.5) + 0.0045, yc, D_WIN[0] - 0.02), (0, 0, -1), (0, 1, 0),
                       [(0, -0.01), (ln * 0.6, -0.006), (ln, 0.0), (ln * 0.6, 0.007), (0, 0.012)], M['rust_streak'])
    # Doppeltuer (geschlossen bzw. bei 'home' einen Spalt offen)
    xd = _dx(1.0)
    for k, (a, b) in enumerate(((door[0] + 0.01, -0.004), (0.004, door[1] - 0.01))):
        shift = 0.16 if (home and k == 1) else 0.0
        a, b = a + shift, b + shift
        _box(f'tuer{k}', (xd - 0.035, a, 0.0), (xd - 0.005, b, 1.95), paint)
        wy0, wy1 = a + 0.12, b - 0.12
        _decal(f'tuerglas{k}', (xd - 0.0045, 0, 0), (0, 1, 0), (0, 0, 1), _rrect(wy0, wy1, 1.0, 1.75, 0.05, n=2), M['glass'])
        if kind == 'b' and k == 0:  # gesprungene Scheibe
            for j in range(4):
                ang = rng.uniform(0, TAU)
                ln = rng.uniform(0.1, 0.25)
                c = ((wy0 + wy1) / 2 - 0.05, 1.45)
                _decal(f'sprung{j}', (xd - 0.004, 0, 0), (0, 1, 0), (0, 0, 1),
                       _ribbon([c, (c[0] + math.cos(ang) * ln * 0.5 + 0.01, c[1] + math.sin(ang) * ln * 0.5),
                                (c[0] + math.cos(ang) * ln, c[1] + math.sin(ang) * ln)], 0.0022), M['glass_dirty'])
        _box(f'tuerblech{k}', (xd - 0.005, a + 0.02, 0.04), (xd - 0.003, b - 0.02, 0.26), M['chrome_worn'])
        _box(f'tuerdichtung{k}', (xd - 0.03, (b if k == 0 else a) - 0.008, 0.0), (xd + 0.002, (b if k == 0 else a) + 0.008, 1.95), M['rubber'])
    _box('tuerfuehrung', (xd - 0.002, door[0] - 0.05, 1.97), (xd + 0.04, door[1] + 0.05, 2.02), M['alu_dull'])
    if home:
        warm = common.material('spalt', (1.0, 0.70, 0.42), look='glow', glow=1.1)
        _box('tuerspalt_licht', (-0.30, -0.004, 0.0), (-0.28, 0.16, 1.95), warm)
        blanket = common.material('decke_wolle', (0.38, 0.32, 0.22), look='fabric', rough=0.95, r=1.3)
        _box('tuerdecke', (-0.10, 0.075, 0.1), (-0.08, 0.17, 1.85), blanket)
    # Aufkleber (verblasst) und Schmutz
    stickers = [M['sticker_w'], M['sticker_y'], M['sticker_b']]
    for k in range(rng.randint(2, 3)):
        y = rng.uniform(-2.8, 2.4)
        if door[0] - 0.4 < y < door[1]:
            continue
        z = rng.uniform(0.15, 0.40)
        w, h = rng.uniform(0.12, 0.30), rng.uniform(0.08, 0.22)
        _decal(f'aufkleber{k}', (_dx(0.3) + 0.0035, y, z), (0, 1, 0), (0, 0, 1), [(0, 0), (w, 0.005), (w - 0.01, h), (0.004, h - 0.01)],
               stickers[(k + seed) % 3])
        _text(f'aufkleber_text{k}', _word(rng, 2, 3), (_dx(0.3) + 0.004, y + 0.02, z + h * 0.35), (0, 1, 0), (0, 0, 1), h * 0.35, M['ink'])
    _text('wagennummer', _word(random.Random(seed * 3), 4, 4), (_dx(0.3) + 0.0035, -2.85, 0.08), (0, 1, 0), (0, 0, 1), 0.06, M['ink'])
    nx = 24
    bm = bmesh.new()
    cols = []
    for i in range(nx + 1):
        y = -3.0 + 6.0 * i / nx
        zt = max(-0.02, 0.01 + 0.03 * math.sin(y * 1.3 + seed) + 0.025 * math.sin(y * 3.7 + 2 * seed) + rng.uniform(-0.025, 0.025))
        cols.append([bm.verts.new((_dx(z) + 0.004, y, z)) for z in (-0.50, -0.44, -0.25, -0.05, zt)])
    for i in range(nx):
        for k in range(4):
            f = bm.faces.new((cols[i][k], cols[i + 1][k], cols[i + 1][k + 1], cols[i][k + 1]))
            f.normal_update()
            if f.normal.x < 0:
                f.normal_flip()
    _link('bremsstaub', bm, [M['brake_dust']])
    # Ablaufschmutz unter der Regenrinne (unregelmaessige Unterkante, Laeufer)
    bm = bmesh.new()
    cols = []
    for i in range(nx + 1):
        y = -3.0 + 6.0 * i / nx
        zb = 2.05 - abs(math.sin(y * 2.7 + seed)) * 0.08 - (0.12 if rng.random() < 0.18 else 0.0)
        cols.append([bm.verts.new((_dx(z) + 0.0045, y, z)) for z in (zb, 2.15, 2.21)])
    for i in range(nx):
        for k in range(2):
            f = bm.faces.new((cols[i][k], cols[i + 1][k], cols[i + 1][k + 1], cols[i][k + 1]))
            f.normal_update()
            if f.normal.x < 0:
                f.normal_flip()
    _link('ablaufschmutz', bm, [common.material('ablaufschmutz', (0.17, 0.165, 0.16), look='paint', rough=0.9, r=1.3)])
    # Rostinseln (wenige, an Kanten und Stoessen)
    for k in range(3 + (3 if worn else 0)):
        y, z = rng.uniform(-2.9, 2.9), rng.choice((rng.uniform(-0.4, 0.0), rng.uniform(1.95, 2.15)))
        _decal(f'rostinsel{k}', (_dx(z) + 0.0056, y, z), (0, 1, 0), (0, 0, 1), _blob(rng, rng.uniform(0.03, 0.09), rng.uniform(0.02, 0.05), 9, 0.45),
               M['rust'])
    if home:
        _home_props(M, rng)


def _home_props(M, rng):
    """Spuren der Bewohner vor dem Wagen (Wiki 903: Wagen als Unterkunft, Handelsposten): Pflanze im Topf, Holzkisten und
    ein Karton, Fussmatte, handgemaltes Schild an Draehten unter der Rinne, Kabel aus dem Tuerspalt am Wagen entlang,
    Gluehbirne im Drahtkorb unter der Rinne."""
    # Pflanze (Bogenhanf-artig: aufrechte, gebogene Blaetter) im Tontopf
    pot = common.material('topf', (0.55, 0.30, 0.18), look='ceramic', rough=0.7, r=1.1)
    leaf = common.material('blatt', (0.20, 0.36, 0.15), look='plastic', rough=0.6, r=1.0)
    leaf2 = common.material('blatt_hell', (0.36, 0.46, 0.18), look='plastic', rough=0.6, r=1.0)
    py, px = -2.55, 0.42
    _lathe('topf', [(0.0, 0.0), (0.10, 0.0), (0.125, 0.22), (0.14, 0.235), (0.14, 0.26), (0.12, 0.26), (0.11, 0.24)], [pot], seg=14,
           center=(px, py, 0.0))
    _lathe('erde', [(0.11, 0.235), (0.0, 0.23)], [M['grime_dark']], seg=14, center=(px, py, 0.0))
    for k in range(9):
        a = TAU * k / 9 + rng.uniform(-0.2, 0.2)
        r0 = rng.uniform(0.0, 0.05)
        h = rng.uniform(0.38, 0.62)
        lean = rng.uniform(0.05, 0.18)
        bm = bmesh.new()
        n = 5
        rows = []
        for j in range(n + 1):
            t = j / n
            w = 0.035 * math.sin(math.pi * min(0.98, t * 0.9 + 0.08))
            c = Vector((px + math.cos(a) * (r0 + lean * t * t), py + math.sin(a) * (r0 + lean * t * t), 0.24 + h * t))
            side = Vector((-math.sin(a), math.cos(a), 0))
            rows.append([bm.verts.new(c - side * w), bm.verts.new(c + side * w)])
        for j in range(n):
            bm.faces.new((rows[j][0], rows[j][1], rows[j + 1][1], rows[j + 1][0]))
        _link(f'blatt{k}', bm, [leaf if k % 3 else leaf2])
    # Holzkisten (Latten) und ein Karton
    for i, (y, z, w, d, h) in enumerate(((1.10, 0.0, 0.50, 0.38, 0.32), (1.18, 0.32, 0.42, 0.34, 0.28))):
        x0 = 0.16
        for j in range(3):
            zz = z + j * (h / 3)
            for (xa, xb, ya, yb) in ((x0, x0 + d, y - 0.01, y + 0.012), (x0, x0 + d, y + w - 0.012, y + w + 0.01),
                                     (x0 + d - 0.012, x0 + d + 0.01, y, y + w), (x0 - 0.01, x0 + 0.012, y, y + w)):
                _box(f'latte{i}{j}{xa:.2f}{ya:.2f}', (xa, ya, zz + 0.01), (xb, yb, zz + h / 3 - 0.012), M['wood_a' if (i + j) % 2 else 'wood_b'])
        _box(f'kistenboden{i}', (x0, y, z), (x0 + d, y + w, z + 0.012), M['wood_b'])
        for (yy) in (y + 0.02, y + w - 0.06):
            _box(f'eckleiste{i}{yy:.2f}', (x0 + d, yy, z), (x0 + d + 0.015, yy + 0.04, z + h), M['wood_c'])
    card = common.material('karton', (0.55, 0.42, 0.28), look='cardboard', rough=0.9)
    ob = _box('karton', (0.15, 1.75, 0.0), (0.55, 2.15, 0.30), card)
    _jitter(ob, 0.008, 5)
    _box('karton_band', (0.149, 1.93, 0.0), (0.551, 1.97, 0.302), common.material('klebeband', (0.62, 0.52, 0.36), look='plastic', rough=0.4))
    _box('fussmatte', (0.14, -0.55, 0.0), (0.62, 0.55, 0.012), common.material('fussmatte', (0.30, 0.22, 0.15), look='fabric', rough=1.0, r=1.4))
    # Schild an Draehten unter der Rinne
    board = common.material('schild_holz', (0.50, 0.40, 0.28), look='wood', tex='wood_birch', rough=0.85, r=1.2)
    paint = common.material('schild_farbe', (0.85, 0.82, 0.70), look='paint', rough=0.8, r=1.1)
    _box('schild', (0.17, 0.95, 1.99), (0.19, 2.25, 2.17), board)
    _text('schild_text', _word(random.Random(51), 5, 5), (0.191, 1.02, 2.03), (0, 1, 0), (0, 0, 1), 0.10, paint)
    for y in (1.05, 2.15):
        _cyl(f'draht{y}', (_dx(2.22) + 0.012, y, 2.21), (0.18, y, 2.17), 0.0025, M['nail'], seg=4, caps=False)
    # Kabel aus dem Tuerspalt am Wagen entlang (Strom fuer Licht und Kocher)
    _tube('kabel', [(_dx(0.5) - 0.02, 0.16, 0.9), (_dx(0.5) + 0.02, 0.17, 0.6), (_dx(0.1) + 0.02, 0.2, 0.05), (_dx(0.1) + 0.03, 0.4, 0.012),
                    (0.13, 1.0, 0.012), (0.13, 1.9, 0.014), (0.14, 2.6, 0.012), (0.13, 3.0, 0.012)], 0.007, M['cable'], seg=5)
    _tube('kabel_lampe', [(_dx(1.9) + 0.01, -0.75, 1.95), (_dx(1.95) + 0.02, -0.75, 2.10), (_dx(2.22) + 0.03, -0.74, 2.21),
                          (0.30, -0.70, 2.20), (0.32, -0.70, 2.08)], 0.005, M['cable'], seg=5)
    bulb = common.material('birne', (1.0, 0.78, 0.50), look='glow', glow=1.6)
    _lathe('birne', [(0.0, -0.06), (0.025, -0.05), (0.03, -0.03), (0.018, -0.005), (0.012, 0.0)], [bulb], seg=10, center=(0.32, -0.70, 2.08))
    for k in range(4):
        a = TAU * k / 4
        _tube(f'korb{k}', [(0.32, -0.70, 2.08), (0.32 + 0.04 * math.cos(a), -0.70 + 0.04 * math.sin(a), 2.04),
                           (0.32 + 0.035 * math.cos(a), -0.70 + 0.035 * math.sin(a), 1.995), (0.32, -0.70, 1.99)], 0.0018, M['nail'], seg=3,
              caps=False)


def build_metro_dormant_a():
    """Ruhender Zug (Wiki: auf dem rechten Gleis ein ruhender Wagen ueber die ganze Laenge), 6-m-Abschnitt, Fenster mit Kippfluegel."""
    _dormant('a', 3)


def build_metro_dormant_b():
    """Ruhender Zug, 6-m-Abschnitt, Fenster mit Mittelsprosse, Tuerscheibe gesprungen."""
    _dormant('b', 11)


def build_metro_dormant_a__v1():
    """Wie a, staerker verschmutzt (Staub auf den Scheiben, Laufspuren, Aufkleberreste), andere Aufkleber."""
    _dormant('a', 23, worn=True)


def build_metro_dormant_b__v1():
    """Wie b, staerker verschmutzt, andere Aufkleber und Spruenge."""
    _dormant('b', 31, worn=True)


def build_metro_dormant_home():
    """Ein Abschnitt des ruhenden Zugs als Unterkunft (Wiki: Wagen als Stuetzpunkte, Motels, Handelsposten)."""
    _dormant('home', 5)


def build_metro_dormant_coupling():
    """Wagenuebergang des ruhenden Zugs (Wand, 1 m breit, Rueckseite x = 0): gerundete Wagenenden beider Wagen mit
    Stirnwaenden in die Tiefe, Faltenbalg mit Rahmen, dunkler Durchgang, Kurzkupplung mit Luft- und Steuerleitungen."""
    M = _M()
    paint = M['paint_old']
    for s in (-1, 1):  # Wagenecken: von der Seitenwand (x = _dx) um 90 Grad in die Stirnwand (y = s*0.42)
        rings = []
        for (xo, z) in DSIDE[:-2]:
            ring = []
            R = 0.10
            for k in range(5):
                a = math.radians(90 * k / 4)
                ring.append((xo - R + R * math.cos(a), s * (0.40 + R - R * math.sin(a)), z))
            ring += [(-0.60, s * 0.40, z), (-0.60, s * 0.50, z)]
            rings.append(ring)
        _loft(f'ecke{s}', rings, [paint], smooth=50)
    # Balg: Falten zwischen den Stirnwaenden, etwas zurueckgesetzt
    bellow = common.material('faltenbalg', (0.13, 0.13, 0.135), look='rubber', rough=0.8, r=1.1)
    zig = []  # Balg als Zickzack-Flaeche (Falten 4 cm), oben und unten Rahmen
    nf = 18
    for k in range(nf + 1):
        y = -0.38 + 0.76 * k / nf
        zig.append((-0.03 if k % 2 else -0.075, y))
    poly = zig + [(-0.14, 0.38), (-0.14, -0.38)]
    _prism('balg', poly, 0.02, 2.12, [bellow], axis='z')
    _box('balgrahmen_o', (-0.14, -0.40, 2.12), (-0.02, 0.40, 2.18), M['under'])
    _box('balgrahmen_u', (-0.14, -0.40, -0.04), (-0.02, 0.40, 0.02), M['under'])
    _box('durchgang_dunkel', (-0.62, -0.40, -0.45), (-0.60, 0.40, 2.62), M['grime_dark'])
    # Kurzkupplung, Leitungen
    _box('kupplung', (-0.40, -0.45, -0.34), (-0.20, 0.45, -0.22), M['bogie'], bevel=0.015)
    _cyl('kupplung_bolzen', (-0.30, 0.0, -0.36), (-0.30, 0.0, -0.20), 0.04, M['wheel'], seg=10)
    for k, (xx, r, mat) in enumerate(((-0.06, 0.022, M['rubber']), (-0.13, 0.022, M['rubber']), (-0.20, 0.016, M['cable']))):
        pts = [(xx, -0.45, -0.16), (xx + 0.02, -0.2, -0.30 - 0.03 * k), (xx + 0.02, 0.2, -0.30 - 0.03 * k), (xx, 0.45, -0.16)]
        _tube(f'leitung{k}', pts, r, mat, seg=6)
