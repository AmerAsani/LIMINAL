"""V13.3 Level 5 (LEVELSPEC_5.md, LOOK_5.md, Wikidot Level 5): ein Grandhotel der 1920er-1930er, das niemand mehr
betritt, aber jemand noch putzt. Alles in Blender neu gebaut (V13.3, PROPS_Level5.md): echte Profile (Sweep mit Gehrung),
Drehkoerper, Polster als verformte Kissen (durchgesessen, Naehte mit Keder, Knoepfe), Beschlaege mit Schluesselloch,
Nieten und Schrauben, Ventilraeder nach dem Ventilrad-Massstab; Gebrauchsspuren als eigene Flaechen und Materialien
(abgegriffenes Messing, abgesessener Samt, Russ und Rost im Kesselraum - die Hotelbereiche bleiben sauber, Wiki).

Konventionen: Meter, z oben, Blick entlang +x. Wandmodelle: Wandflaeche x = 0. Deckenmodelle: Decke z = 0.
Bodenmodelle: Boden z = 0. Groesse, Ursprung und Befestigung wie in V12 (Karten).
Schrift: Inter (SIL OFL 1.1) aus den Blender-Datendateien, als Geometrie (LICENSES.md).
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

FONT = None
TAU = 2.0 * math.pi


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


def _text(name, body, size, mat, at, depth=0.002, res=None):
    """Text als Mesh in der yz-Ebene, blickt entlang +x, zentriert um at. res: Kurvenaufloesung (weniger Dreiecke).
    (Auch von models_l36 und models_l53 benutzt - Signatur nicht aendern.)"""
    cu = bpy.data.curves.new(name, 'FONT')
    if res:
        cu.resolution_u = res
    cu.body = body
    cu.font = _font()
    cu.size = size
    cu.extrude = depth
    cu.align_x = 'CENTER'
    cu.align_y = 'CENTER'
    ob = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(ob)
    ob.data.materials.append(mat)
    # Text liegt in xy und blickt entlang +z: auf die Wand drehen (blickt entlang +x)
    ob.matrix_world = Matrix.Translation(at) @ Matrix.Rotation(math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X')
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    ob.select_set(False)
    return ob


def _text_clean(name, body, size, mat, at, depth=0.0012, res=3):
    """Wie _text, danach doppelte Punkte verschmolzen und entartete Dreiecke aufgeloest (die alten Plaketten
    hatten entartete Dreiecke)."""
    ob = _text(name, body, size, mat, at, depth=depth, res=res)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-5, edges=bm.edges)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bad = [f for f in bm.faces if f.calc_area() < 1e-10]
    if bad:
        bmesh.ops.delete(bm, geom=bad, context='FACES')
    bm.to_mesh(ob.data)
    bm.free()
    return ob


# --- Geometrie in ein bmesh -------------------------------------------------------------------------------------------
def _obj(name, bm, mat, smooth=40.0, recalc=False):
    """bmesh -> Objekt. mat: Material oder Liste (Flaechen tragen material_index). smooth: Grenzwinkel fuer weiche
    Schattierung (None = flach). recalc: Normalen nach aussen richten (nur fuer geschlossene Koerper)."""
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    for m in (mat if isinstance(mat, (list, tuple)) else [mat]):
        me.materials.append(m)
    if smooth is not None:
        common.shade_auto(ob, smooth)
    return ob


def _xf(bm, verts, M):
    if M is not None:
        bmesh.ops.transform(bm, matrix=M, verts=verts)
    return verts


def T(x=0.0, y=0.0, z=0.0):
    return Matrix.Translation((x, y, z))


def R(a, ax):
    return Matrix.Rotation(a, 4, ax)


def S(sx, sy, sz):
    return Matrix.Diagonal((sx, sy, sz, 1.0))


def _aim(o, d):
    """Matrix: lokale z-Achse -> Richtung d, Ursprung o."""
    d = Vector(d).normalized()
    return Matrix.Translation(o) @ d.to_track_quat('Z', 'Y').to_matrix().to_4x4()


def g_box(bm, lo, hi, M=None, ch=0.0, mi=0):
    """Quader (lo, hi), optional mit Fase ch (eine Stufe), dann Matrix M."""
    r = bmesh.ops.create_cube(bm, size=1.0)
    vs = r['verts']
    for v in vs:
        v.co = Vector([lo[i] + (v.co[i] + 0.5) * (hi[i] - lo[i]) for i in range(3)])
    _xf(bm, vs, M)
    faces = list({f for v in vs for f in v.link_faces})
    for f in faces:
        f.material_index = mi
    if ch > 0.0:
        c = min(ch, 0.4 * min(hi[i] - lo[i] for i in range(3)))
        edges = list({e for v in vs for e in v.link_edges})
        res = bmesh.ops.bevel(bm, geom=edges + vs, offset=c, segments=1, profile=0.5, affect='EDGES', clamp_overlap=True)
        for f in res['faces']:
            f.material_index = mi
    return vs


def g_lathe(bm, prof, seg=16, M=None, phase=0.0, sy=1.0, mi=0):
    """Drehkoerper um z aus (r, z)-Punkten (aufsteigend = Normalen aussen); r <= 0 schliesst mit einem Pol."""
    rings = []
    for (r, z) in prof:
        if r <= 1e-7:
            rings.append([bm.verts.new((0.0, 0.0, z))])
        else:
            rings.append([bm.verts.new((r * math.cos(TAU * k / seg + phase), sy * r * math.sin(TAU * k / seg + phase), z))
                          for k in range(seg)])
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        if len(A) == 1 and len(B) == 1:
            continue
        for k in range(seg):
            j = (k + 1) % seg
            if len(A) == 1:
                f = bm.faces.new((A[0], B[j], B[k]))
            elif len(B) == 1:
                f = bm.faces.new((A[k], A[j], B[0]))
            else:
                f = bm.faces.new((A[k], A[j], B[j], B[k]))
            f.material_index = mi
    vs = [v for r in rings for v in r]
    _xf(bm, vs, M)
    return vs


def _frames(path, closed, up):
    n = len(path)
    segs = n if closed else n - 1
    d = []
    for i in range(segs):
        e = path[(i + 1) % n] - path[i]
        d.append(e.normalized() if e.length > 1e-9 else (d[-1] if d else Vector((1, 0, 0))))
    fr = []
    if up is not None:
        U = Vector(up)
        for di in d:
            v = U - di * U.dot(di)
            v = v.normalized() if v.length > 1e-6 else Vector((0, 0, 1))
            fr.append((di.cross(v), v))
    else:
        a = Vector((0, 0, 1)) if abs(d[0].z) < 0.9 else Vector((1, 0, 0))
        v = (a - d[0] * a.dot(d[0])).normalized()
        fr.append((d[0].cross(v), v))
        for i in range(1, segs):
            q = d[i - 1].rotation_difference(d[i])
            v = q @ fr[-1][1]
            v = (v - d[i] * v.dot(d[i])).normalized()
            fr.append((d[i].cross(v), v))
    return d, fr


def g_sweep(bm, prof, path, closed=False, up=None, caps=True, pclosed=True, scales=None, mi=0):
    """Profil (u, v) entlang eines Pfads; u = t x v (bei einem Pfad gegen den Uhrzeigersinn: nach aussen), v = up.
    Gehrungen an den Ecken. Offene Profile (pclosed=False) von kleinem zu grossem u ueber die Oberseite: Normalen +v.
    scales: Faktor je Pfadpunkt (verjuengte Arme)."""
    path = [Vector(p) for p in path]
    n = len(path)
    d, fr = _frames(path, closed, up)
    rings = []
    for i in range(n):
        if closed:
            si, so = (i - 1) % n, i
        else:
            si, so = max(i - 1, 0), min(i, n - 2)
        din, dout = d[si], d[so]
        u, v = fr[si]
        nm = din + dout
        nm = nm.normalized() if nm.length > 1e-6 else dout
        k = scales[i] if scales else 1.0
        ring = []
        for (a, b) in prof:
            off = u * (a * k) + v * (b * k)
            if closed or 0 < i < n - 1:
                off = off - din * (off.dot(nm) / din.dot(nm))
            elif i == 0 and not closed:
                off = off - din * off.dot(din)
            ring.append(bm.verts.new(path[i] + off))
        rings.append(ring)
    m = len(prof)
    pc = m if pclosed else m - 1
    segs = n if closed else n - 1
    for i in range(segs):
        A, B = rings[i], rings[(i + 1) % n]
        for kk in range(pc):
            j = (kk + 1) % m
            f = bm.faces.new((A[kk], A[j], B[j], B[kk]))
            f.material_index = mi
    if caps and not closed and pclosed:
        f0 = bm.faces.new(list(reversed(rings[0])))
        f1 = bm.faces.new(rings[-1])
        f0.material_index = f1.material_index = mi
        # Kappen nach aussen (gegen die Pfadrichtung bzw. mit ihr)
        f0.normal_update()
        f1.normal_update()
        if f0.normal.dot(d[0]) > 0:
            f0.normal_flip()
        if f1.normal.dot(d[-1]) < 0:
            f1.normal_flip()
    return [v for r in rings for v in r]


def _circle(r, n, a0=0.0):
    return [(r * math.cos(a0 + TAU * k / n), r * math.sin(a0 + TAU * k / n)) for k in range(n)]


def _cw(pts):
    """Profilpunkte im Uhrzeigersinn (Normalen nach aussen bei g_sweep)."""
    a = sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    return list(reversed(pts)) if a > 0 else list(pts)


def g_tube(bm, path, r, seg=8, closed=False, caps=True, scales=None, mi=0):
    return g_sweep(bm, _cw(_circle(r, seg)), path, closed=closed, caps=caps, scales=scales, mi=mi)


def _bez(p0, p1, p2, p3, n):
    p0, p1, p2, p3 = (Vector(p) for p in (p0, p1, p2, p3))
    out = []
    for k in range(n + 1):
        t = k / n
        out.append(p0 * (1 - t) ** 3 + p1 * 3 * (1 - t) ** 2 * t + p2 * 3 * (1 - t) * t * t + p3 * t ** 3)
    return out


def _rrect(cy, cz, hy, hz, r, n=3):
    """Abgerundetes Rechteck in (y, z), gegen den Uhrzeigersinn (von +x gesehen)."""
    pts = []
    for (sy, sz, a0) in ((1, -1, -90), (1, 1, 0), (-1, 1, 90), (-1, -1, 180)):
        c = (cy + sy * (hy - r), cz + sz * (hz - r))
        for k in range(n + 1):
            a = math.radians(a0 + 90.0 * k / n)
            pts.append((c[0] + r * math.cos(a), c[1] + r * math.sin(a)))
    return pts


def g_plate(bm, outline, x0, x1, M=None, mi=0):
    """Ebene Platte in der yz-Ebene (Umriss gegen den Uhrzeigersinn von +x), von x0 bis x1 dick."""
    a = [bm.verts.new((x0, y, z)) for (y, z) in outline]
    b = [bm.verts.new((x1, y, z)) for (y, z) in outline]
    n = len(outline)
    fs = [bm.faces.new(b), bm.faces.new(list(reversed(a)))]
    for i in range(n):
        j = (i + 1) % n
        fs.append(bm.faces.new((a[i], a[j], b[j], b[i])))
    for f in fs:
        f.material_index = mi
    _xf(bm, a + b, M)
    return a + b


def g_prism(bm, pts, z0, z1, M=None, mi=0):
    """Prisma: Umriss pts (x, y) gegen den Uhrzeigersinn (von +z), von z0 bis z1, dann Matrix M."""
    a = [bm.verts.new((x, y, z0)) for (x, y) in pts]
    b = [bm.verts.new((x, y, z1)) for (x, y) in pts]
    n = len(pts)
    fs = [bm.faces.new(b), bm.faces.new(list(reversed(a)))]
    for i in range(n):
        j = (i + 1) % n
        fs.append(bm.faces.new((a[i], a[j], b[j], b[i])))
    for f in fs:
        f.material_index = mi
    _xf(bm, a + b, M)
    return a + b


def g_quad(bm, p0, p1, p2, p3, mi=0):
    """Einzelne Flaeche (Reihenfolge gegen den Uhrzeigersinn von der Sichtseite)."""
    f = bm.faces.new([bm.verts.new(p) for p in (p0, p1, p2, p3)])
    f.material_index = mi
    return f


def g_mold_rect(bm, x, y0, z0, y1, z1, prof, r=0.0, mi=0):
    """Profilleiste rund um ein Rechteck in der Wandebene x (Sichtseite +x); prof (u, v): u nach aussen, v aus der
    Flaeche (+x). r > 0: abgerundete Ecken."""
    if r > 0:
        pts = _rrect((y0 + y1) / 2, (z0 + z1) / 2, (y1 - y0) / 2, (z1 - z0) / 2, r, 3)
    else:
        pts = [(y0, z0), (y1, z0), (y1, z1), (y0, z1)]
    return g_sweep(bm, prof, [(x, y, z) for (y, z) in pts], closed=True, up=(1, 0, 0), pclosed=False, mi=mi)


def g_mold_u(bm, x0, x1, hy, z, prof, mi=0):
    """Profil um Front und Seiten eines Korpus (Sockel, Kranz): Pfad hinten links -> vorn -> hinten rechts in Hoehe z,
    Korpus x0..x1, +-hy; prof (u nach aussen, v nach oben)."""
    path = [(x0, -hy, z), (x1, -hy, z), (x1, hy, z), (x0, hy, z)]
    return g_sweep(bm, prof, path, closed=False, up=(0, 0, 1), caps=True, mi=mi)


def g_domes(bm, pts, r, h, seg=6, mi=0):
    """Viele kleine Kuppeln (Nietkoepfe, Knoepfe): pts = [(Mitte, Normale)]."""
    for (c, nrm) in pts:
        M = _aim(Vector(c), Vector(nrm))
        g_lathe(bm, [(r * 1.02, -0.001), (r, 0.0), (r * 0.8, h * 0.6), (r * 0.42, h * 0.95), (0.0, h)], seg=seg, M=M, mi=mi)


def g_hex(bm, c, nrm, s, h, mi=0, washer=True, rot=0.0):
    """Sechskantmutter/Schraubenkopf (Schluesselweite s, Hoehe h) mit Unterlegscheibe."""
    M = _aim(Vector(c), Vector(nrm)) @ R(rot, 'Z')
    if washer:
        g_lathe(bm, [(0.0, -0.0005), (s * 0.95, -0.0005), (s * 0.95, h * 0.25), (0.0, h * 0.25)], seg=10, M=M, mi=mi)
    rr = s / math.sqrt(3.0)
    g_lathe(bm, [(0.0, h * 0.2), (rr, h * 0.2), (rr, h * 0.95), (rr * 0.85, h * 1.1), (0.0, h * 1.1)], seg=6, M=M, mi=mi)


def g_rivet_ring(c0, axis, R_, n, a0=0.0):
    """Nietpunkte auf einem Kreis (Mitte c0, Achse axis, Radius R_) -> [(Punkt, Normale)]."""
    ax = Vector(axis).normalized()
    t = Vector((0, 0, 1)) if abs(ax.z) < 0.9 else Vector((1, 0, 0))
    u = ax.cross(t).normalized()
    v = ax.cross(u).normalized()
    out = []
    for k in range(n):
        a = a0 + TAU * k / n
        dn = u * math.cos(a) + v * math.sin(a)
        out.append((Vector(c0) + dn * R_, dn))
    return out


# --- Polster ------------------------------------------------------------------------------------------------------------
def _rbox_map(half, rad):
    hx, hy, hz = half
    rx, ry, rz = rad
    ix, iy, iz = hx - rx, hy - ry, hz - rz

    def f(p):
        P = Vector((p[0] * hx, p[1] * hy, p[2] * hz))
        c = Vector((max(-ix, min(ix, P.x)), max(-iy, min(iy, P.y)), max(-iz, min(iz, P.z))))
        e = Vector(((P.x - c.x) / rx, (P.y - c.y) / ry, (P.z - c.z) / rz))
        L = e.length
        if L < 1e-9:
            return P, Vector(p).normalized()
        e /= L
        pos = c + Vector((e.x * rx, e.y * ry, e.z * rz))
        nrm = Vector((e.x / rx, e.y / ry, e.z / rz)).normalized()
        return pos, nrm
    return f


def _warp(i, n):
    return math.sin((-1.0 + 2.0 * i / n) * math.pi / 2.0)


def g_cushion(bm, ctr, half, rad, n=(8, 8, 6), disp=None, skip=(), M=None, matfn=None):
    """Polster: abgerundeter Quader (Halbachsen half, Kantenradien rad), Gitter n je Achse (an den Kanten dichter),
    verformt durch disp(pos, nrm, p) entlang der Normalen (Bausch, Kuhle, Knopfheftung). skip: ausgelassene Seiten
    ((Achse, Vorzeichen)). matfn(pos, nrm) -> Materialindex je Flaeche. Liefert S(p) -> (Punkt, Normale) in Welt."""
    f = _rbox_map(half, rad)
    C = Vector(ctr)
    Mx = M if M is not None else Matrix.Identity(4)
    Mr = Mx.to_3x3()

    def Sl(p):
        pos, nrm = f(p)
        if disp:
            pos = pos + nrm * disp(pos, nrm, p)
        return pos, nrm

    def Sw(p):
        pos, nrm = Sl(p)
        return Mx @ (pos + C), (Mr @ nrm).normalized()

    cache = {}

    def V(p):
        key = (round(p[0], 6), round(p[1], 6), round(p[2], 6))
        v = cache.get(key)
        if v is None:
            v = bm.verts.new(Sw(p)[0])
            cache[key] = v
        return v
    ns = n
    for ax in range(3):
        a, b = ((1, 2), (2, 0), (0, 1))[ax]
        for s in (-1, 1):
            if (ax, s) in skip:
                continue
            na, nb = ns[a], ns[b]
            grid = []
            for i in range(na + 1):
                row = []
                for j in range(nb + 1):
                    p = [0.0, 0.0, 0.0]
                    p[ax] = float(s)
                    p[a] = _warp(i, na)
                    p[b] = _warp(j, nb)
                    row.append(V(p))
                grid.append(row)
            for i in range(na):
                for j in range(nb):
                    q = (grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1])
                    if s < 0:
                        q = q[::-1]
                    fc = bm.faces.new(q)
                    if matfn:
                        pc = [0.0, 0.0, 0.0]
                        pc[ax] = float(s)
                        pc[a] = (_warp(i, na) + _warp(i + 1, na)) / 2
                        pc[b] = (_warp(j, nb) + _warp(j + 1, nb)) / 2
                        pos, nrm = Sl(pc)
                        fc.material_index = matfn(pos, nrm)
    return Sw


def _edge_loop(Sw, n, face_ax, s, k=1.06):
    """Naht (Keder) entlang der Kante der Seite (face_ax, s): Punkte auf der Rundung, leicht nach aussen."""
    a, b = ((1, 2), (2, 0), (0, 1))[face_ax]
    na, nb = n[a], n[b]
    loop = []
    # Umlauf: a von -1..1 bei b=-1, b von -1..1 bei a=1, a zurueck bei b=1, b zurueck bei a=-1
    seq = [(_warp(i, na), -1.0) for i in range(na)] + [(1.0, _warp(j, nb)) for j in range(nb)] + \
          [(_warp(na - i, na), 1.0) for i in range(na)] + [(-1.0, _warp(nb - j, nb)) for j in range(nb)]
    for (ua, ub) in seq:
        p = [0.0, 0.0, 0.0]
        p[face_ax] = float(s)
        p[a], p[b] = ua, ub
        pos, nrm = Sw(p)
        loop.append(pos + nrm * 0.004 * k)
    return loop


def g_decal(bm, Sw, ax, s, cu, cv, ru, rv, seed, lift=0.0015, rings=4, seg=20, rough=0.18, mi=0):
    """Flecken auf einer Polsterseite (Abrieb, Glanzstelle): unregelmaessiger Umriss, folgt der Flaeche (Sw) mit
    kleinem Abstand. (cu, cv): Mitte in den Seitenkoordinaten der Seite (ax, s), ru/rv: Halbachsen (-1..1)."""
    rng = random.Random(seed)
    a, b = ((1, 2), (2, 0), (0, 1))[ax]
    jit = [1.0 + rough * (rng.random() * 2 - 1) for _ in range(seg)]
    jit = [(jit[k - 1] + 2 * jit[k] + jit[(k + 1) % seg]) / 4 for k in range(seg)]

    def P(t, k):
        ang = TAU * k / seg
        r = t * (jit[k] if t > 0.5 else 1.0 + (jit[k] - 1.0) * t)
        p = [0.0, 0.0, 0.0]
        p[ax] = float(s)
        p[a] = max(-0.999, min(0.999, cu + math.cos(ang) * ru * r))
        p[b] = max(-0.999, min(0.999, cv + math.sin(ang) * rv * r))
        pos, nrm = Sw(p)
        return pos + nrm * lift, nrm
    c, cn = P(0.0, 0)
    vc = bm.verts.new(c)
    prev = None
    for i in range(1, rings + 1):
        t = i / rings
        ring = [bm.verts.new(P(t, k)[0]) for k in range(seg)]
        for k in range(seg):
            j = (k + 1) % seg
            f = bm.faces.new((vc, ring[k], ring[j]) if prev is None else (prev[k], ring[k], ring[j], prev[j]))
            f.material_index = mi
            f.normal_update()
            if f.normal.dot(cn) < 0:
                f.normal_flip()
        prev = ring


def _tufts(cy, cz, w, h, cols, rows):
    """Rautenheftung: Knopfpunkte (y, z) und Faltenlinien zwischen Nachbarn."""
    pts, idx = [], {}
    for r in range(rows):
        nc = cols if r % 2 == 0 else cols - 1
        for c in range(nc):
            y = cy - w / 2 + (c + (0.5 if r % 2 else 0.0)) * (w / (cols - 1))
            z = cz - h / 2 + r * (h / (rows - 1))
            idx[(r, c)] = len(pts)
            pts.append((y, z))
    links = []
    for (r, c), i in idx.items():
        for (dr, dc) in ((1, 0), (1, -1)) if r % 2 == 0 else ((1, 0), (1, 1)):
            j = idx.get((r + dr, c + dc))
            if j is not None:
                links.append((i, j))
    return pts, links


def _tuft_disp(pts, links, depth=0.040, sig=0.022, crease=0.014, cw=0.007, puff=0.020):
    def d(y, z):
        v = puff
        for (ty, tz) in pts:
            r2 = (y - ty) ** 2 + (z - tz) ** 2
            if r2 < 0.02:
                v -= depth * math.exp(-r2 / (sig * sig))
        for (i, j) in links:
            (ay, az), (by, bz) = pts[i], pts[j]
            dy, dz = by - ay, bz - az
            L2 = dy * dy + dz * dz
            t = max(0.0, min(1.0, ((y - ay) * dy + (z - az) * dz) / L2))
            qy, qz = ay + t * dy - y, az + t * dz - z
            r2 = qy * qy + qz * qz
            if r2 < 0.004:
                v -= crease * math.exp(-r2 / (cw * cw)) * (0.4 + 0.6 * math.sin(math.pi * t))
        return v
    return d



# --- Materialien --------------------------------------------------------------------------------------------------------
def _m():
    return {
        # Nussbaum, schellackpoliert (seidenmatt); dunkle Beize in Kehlen; abgegriffene Kanten heller
        'walnut': common.material('nussbaum', (0.44, 0.27, 0.16), look='wood', rough=0.45, r=0.7),
        'walnut_d': common.material('nussbaum_dunkel', (0.29, 0.17, 0.10), look='wood', rough=0.5, r=0.8),
        'walnut_l': common.material('nussbaum_hell', (0.52, 0.34, 0.20), look='wood', rough=0.45, r=0.65),
        'veneer': common.material('wurzelfurnier', (0.50, 0.30, 0.16), look='wood', rough=0.35, r=0.55),
        'ebony': common.material('ebenholz', (0.07, 0.05, 0.04), look='wood', rough=0.4, r=0.6),
        # Messing: poliert, angelaufen (in Vertiefungen), abgegriffen (hell, glatt)
        'brass': common.material('messing', (0.80, 0.62, 0.32), look='metal', rough=0.35, metal=1.0, r=0.55, m=0.95),
        'brass_d': common.material('messing_alt', (0.48, 0.36, 0.18), look='metal', rough=0.6, metal=0.9, r=0.95, m=0.8),
        'brass_w': common.material('messing_blank', (0.92, 0.76, 0.46), look='metal', rough=0.2, metal=1.0, r=0.35, m=1.0),
        'gilt': common.material('blattgold', (0.82, 0.64, 0.30), look='metal', rough=0.4, metal=0.9, r=0.6, m=0.9),
        'gilt_d': common.material('gold_bolus', (0.36, 0.24, 0.12), look='paint', rough=0.8, r=1.0, m=0.25),
        'silver': common.material('neusilber', (0.80, 0.80, 0.78), look='metal', rough=0.3, metal=1.0, r=0.5, m=0.95),
        'copper': common.material('kupfer', (0.70, 0.42, 0.27), look='metal', rough=0.4, metal=1.0, r=0.6, m=0.95),
        'verdigris': common.material('gruenspan', (0.36, 0.48, 0.40), look='paint', rough=0.9, r=1.2, m=0.1),
        'black': common.material('schwarz', (0.035, 0.035, 0.035), look='paint', rough=0.4, r=0.8),
        'cream': common.material('creme', (0.86, 0.80, 0.66), look='paint', rough=0.6, r=1.0),
        'plaster': common.material('stuck', (0.88, 0.83, 0.70), look='paint', rough=0.8, r=1.25),
        'ivory': common.material('elfenbein', (0.88, 0.84, 0.72), look='ceramic', rough=0.35, r=0.7),
        'glass': common.material('glas', (0.75, 0.8, 0.8), look='glass', rough=0.05),
        'crystal': common.material('kristall', (0.92, 0.94, 0.96), look='glass', rough=0.02, r=0.15),
        'iron': common.material('gusseisen', (0.11, 0.105, 0.10), look='metal', rough=0.7, metal=0.6, r=1.2, m=0.45),
        'steel': common.material('stahl', (0.30, 0.30, 0.29), look='metal', rough=0.55, metal=0.8, r=1.0, m=0.7),
        'rust': common.material('rost', (0.45, 0.27, 0.15), look='metal', tex='metal_rusty_iron', rough=0.9, metal=0.3, r=1.4, m=0.15),
        'rust_l': common.material('rost_hell', (0.58, 0.36, 0.18), look='concrete', rough=1.0, r=1.5, m=0.0),
        'soot': common.material('russ', (0.025, 0.024, 0.022), look='concrete', rough=1.0, r=1.5, m=0.0),
        'grease': common.material('fett', (0.05, 0.045, 0.035), look='paint', rough=0.2, r=0.35, m=0.1),
        'brick': common.material('ziegel', (0.80, 0.55, 0.45), look='concrete', tex='bricks_brown_old', rough=0.9, r=1.3),
        'red': common.material('rot', (0.46, 0.08, 0.07), look='fabric', rough=0.8, r=1.2),
    }


VELVET = {0: (0.42, 0.07, 0.08), 1: (0.60, 0.46, 0.18)}


def _fab(v):
    """Samt (Wiki/LOOK_5: Weinrot oder Gold); abgesessene Stellen heller mit flachem Flor, Keder dunkler."""
    c = VELVET[v]
    return {
        'fab': common.material(f'samt{v}', c, look='fabric', rough=0.9, r=1.25),
        'worn': common.material(f'samt_abgesessen{v}', tuple(min(1.0, x * 1.22 + 0.035) for x in c), look='fabric', rough=0.8, r=0.9),
        'pipe': common.material(f'keder{v}', tuple(x * 0.7 for x in c), look='fabric', rough=0.85, r=1.05),
        'btn': common.material(f'knopf{v}', tuple(x * 0.78 for x in c), look='fabric', rough=0.7, r=0.85),
    }


# --- Polstermoebel ------------------------------------------------------------------------------------------------------
def _foot(bm, x, y, h=0.10, r=0.033):
    """Gedrechselter Kugelfuss (Nussbaum)."""
    k = h / 0.10
    g_lathe(bm, [(0.0, 0.0), (r * 0.62, 0.0), (r * 0.74, 0.006 * k), (r * 0.92, 0.022 * k), (r, 0.042 * k), (r * 0.94, 0.062 * k),
                 (r * 0.78, 0.074 * k), (r * 0.84, 0.080 * k), (r * 1.08, 0.086 * k), (r * 1.08, h), (0.0, h)], seg=14, M=T(x, y, 0.0))


def _ferrule(bm, x, y, r=0.033):
    g_lathe(bm, [(0.0, -0.0005), (r * 0.70, -0.0005), (r * 0.79, 0.002), (r * 0.84, 0.012), (r * 0.72, 0.0135), (0.0, 0.0135)], seg=14, M=T(x, y, 0.0))


def _arm(F, s, x0, x1, yi, yo, z0, ztop, seed):
    """Rollarmlehne: Koerper, Rolle (ueberstehend nach aussen), Stirnplatte mit Keder."""
    yc, hy = s * (yi + yo) / 2, (yo - yi) / 2
    rr_y, rr_z = hy * 1.2, 0.07
    zc = ztop - rr_z
    bm = bmesh.new()
    g_cushion(bm, ((x0 + x1) / 2, yc, (z0 + zc) / 2), ((x1 - x0) / 2, hy, (zc - z0) / 2), (0.035, min(0.03, hy * 0.45), 0.04),
              n=(10, 4, 8), disp=lambda pos, nrm, p: 0.006 * (1 - p[2] ** 2) * abs(nrm.y))
    rc_y = yc + s * hy * 0.25
    Sr = g_cushion(bm, ((x0 + x1) / 2 + 0.004, rc_y, zc), ((x1 - x0) / 2 + 0.004, rr_y, rr_z), (0.03, rr_y * 0.98, rr_z * 0.98),
                   n=(10, 6, 6), disp=lambda pos, nrm, p: -0.004 * math.exp(-((pos.x - 0.05) ** 2) / 0.03) * max(0.0, nrm.z))
    _obj('armlehne', bm, F['fab'], smooth=70)
    bm = bmesh.new()  # abgegriffen, wo die Haende liegen
    g_decal(bm, Sr, 2, 1, 0.55, -0.1 * s, 0.3, 0.75, seed, rings=3, seg=16)
    _obj('arm_abrieb', bm, F['worn'], smooth=70)
    # Stirnplatte (Umriss gegen den Uhrzeigersinn von +x)
    zi = zc - 0.01
    arc = []
    for k in range(13):
        a = math.radians(180 - 210 * k / 12)
        arc.append((abs(rc_y) + math.cos(a) * rr_y * 0.98, zc + math.sin(a) * rr_z * 0.98))
    # unten innen -> unten aussen -> Bogen von aussen unten ueber oben nach innen -> innen oben
    pts = [(yi, z0 + 0.01), (yo - 0.004, z0 + 0.01)] + [arc[12 - k] for k in range(13)] + [(yi, zi)]
    if s < 0:
        pts = [(-y, z) for (y, z) in reversed(pts)]
    bm = bmesh.new()
    g_plate(bm, pts, x1 - 0.004, x1 + 0.010)
    _obj('stirn', bm, F['fab'], smooth=30, recalc=True)
    bm = bmesh.new()
    g_tube(bm, [(x1 + 0.011, y, z) for (y, z) in pts], 0.0055, seg=6, closed=True)
    _obj('stirn_keder', bm, F['pipe'], smooth=70)


def _tufted_back(F, ctr, half, rad, tilt, pivot, cols, rows, ny, nz, seed):
    """Rueckenpolster mit Rautenheftung und Knoepfen, nach hinten geneigt; Rueckseite als eigenes flaches Polster."""
    hx, hy, hz = half
    pts, links = _tufts(0.0, 0.02, 2 * hy - 0.16, 2 * hz - 0.18, cols, rows)
    field = _tuft_disp(pts, links)

    def disp(pos, nrm, p):
        w = max(0.0, nrm.x) ** 2
        top = max(0.0, nrm.z)
        return w * field(pos.y, pos.z) + 0.012 * top * (1 - p[1] ** 2)
    M = T(*pivot) @ R(-tilt, 'Y') @ T(-pivot[0], -pivot[1], -pivot[2])
    bm = bmesh.new()
    Sw = g_cushion(bm, ctr, half, rad, n=(4, ny, nz), disp=disp, skip=((0, -1),), M=M)
    _obj('ruecken', bm, F['fab'], smooth=70)
    bm = bmesh.new()
    g_cushion(bm, (ctr[0] - hx - 0.012, ctr[1], ctr[2] - 0.03), (0.022, hy + 0.02, hz + 0.03), (0.018, 0.035, 0.045), n=(2, 8, 8),
              disp=lambda pos, nrm, p: 0.008 * (1 - p[1] ** 2) * (1 - p[2] ** 2) * max(0.0, -nrm.x), M=M)
    _obj('ruecken_aussen', bm, F['fab'], smooth=70)
    # Knoepfe in den Kuhlen
    bm = bmesh.new()
    spots = []
    for (y, z) in pts:
        pos, nrm = Sw((1.0, y / hy, z / hz))
        spots.append((pos - nrm * 0.002, nrm))
    g_domes(bm, spots, 0.0105, 0.008, seg=7)
    _obj('knoepfe', bm, F['btn'], smooth=80)
    # Keder rund um die Vorderseite
    bm = bmesh.new()
    g_tube(bm, _edge_loop(Sw, (4, ny, nz), 0, 1), 0.0055, seg=5, closed=True)
    _obj('ruecken_keder', bm, F['pipe'], smooth=70)


def _seat_cushion(F, ctr, half, sag, sag_at, seed):
    """Sitzkissen, durchgesessen (Kuhle vorn-mittig), mit Keder oben und unten; die Kuhle ist abgesessen (heller)."""
    hx, hy, hz = half
    rng = random.Random(seed)
    ph = rng.uniform(0, TAU)

    def disp(pos, nrm, p):
        top = max(0.0, nrm.z)
        crown = 0.012 * (1 - p[0] ** 2) * (1 - p[1] ** 2)
        k = math.exp(-(((pos.x - sag_at[0]) ** 2) / (hx * hx * 0.32) + ((pos.y - sag_at[1]) ** 2) / (hy * hy * 0.38)))
        wrinkle = 0.0025 * math.sin(pos.y * 38.0 + ph + pos.x * 9.0) * k
        front = -0.009 * max(0.0, p[0]) ** 6 * (1 - p[1] ** 2)
        side = 0.007 * (1 - p[2] ** 2) * (1 - abs(nrm.z))
        return top * (crown - sag * k + wrinkle) + front * top + side

    bm = bmesh.new()
    n = (14, 14, 4)
    Sw = g_cushion(bm, ctr, half, (0.045, 0.05, min(0.042, hz * 0.7)), n=n, disp=disp)
    _obj('sitzkissen', bm, F['fab'], smooth=70)
    # abgesessene Stellen: Kuhle oben und Vorderkante (Flor flachgedrueckt, heller)
    bm = bmesh.new()
    g_decal(bm, Sw, 2, 1, sag_at[0] / hx, sag_at[1] / hy, 0.55, 0.6, seed)
    g_decal(bm, Sw, 0, 1, sag_at[1] / hy, 0.25, 0.55, 0.45, seed + 1, rings=3)
    _obj('abrieb', bm, F['worn'], smooth=70)
    bm = bmesh.new()
    g_tube(bm, _edge_loop(Sw, n, 2, 1), 0.0055, seg=5, closed=True)
    g_tube(bm, _edge_loop(Sw, n, 2, -1), 0.0055, seg=5, closed=True)
    _obj('sitz_keder', bm, F['pipe'], smooth=70)


def _armchair(v):
    """Clubsessel der 1930er (Ursprung Mitte am Boden, Blick +x, 0,80 x 1,0 x 0,98 m): Rollarmlehnen mit Keder an den
    Stirnplatten, Rautenheftung im Ruecken, durchgesessenes Sitzkissen, gedrechselte Kugelfuesse mit Messinghuelsen."""
    M = _m()
    F = _fab(v)
    bm = bmesh.new()
    feet = [(-0.30, -0.40), (-0.30, 0.40), (0.33, -0.40), (0.33, 0.40)]
    for (x, y) in feet:
        _foot(bm, x, y)
    _obj('fuesse', bm, M['walnut_d'], smooth=50)
    bm = bmesh.new()
    for (x, y) in feet:
        _ferrule(bm, x, y)
    _obj('huelsen', bm, M['brass_d'], smooth=50)
    bm = bmesh.new()
    g_cushion(bm, (0.01, 0.0, 0.255), (0.39, 0.455, 0.152), (0.03, 0.03, 0.035), n=(10, 12, 4),
              disp=lambda pos, nrm, p: 0.010 * (1 - p[2] ** 2) * (1 - p[1] ** 2) * max(0.0, nrm.x))
    _obj('unterbau', bm, F['fab'], smooth=60)
    bm = bmesh.new()
    g_tube(bm, [(0.40 + 0.002, y, 0.395) for y in (-0.34, -0.2, 0.0, 0.2, 0.34)], 0.0055, seg=5)
    _obj('unterbau_keder', bm, F['pipe'], smooth=70)
    for s in (-1, 1):
        _arm(F, s, -0.385, 0.395, 0.345, 0.475, 0.11, 0.69, 7 + s)
    _tufted_back(F, (-0.29, 0.0, 0.675), (0.075, 0.40, 0.29), (0.045, 0.05, 0.07), 0.09, (-0.29, 0.0, 0.40), 5, 4, 34, 20, 3)
    _seat_cushion(F, (0.075, 0.0, 0.468), (0.322, 0.338, 0.066), 0.030, (0.07, 0.03), 11)


def build_armchair():
    _armchair(0)


def build_armchair__v1():
    _armchair(1)


def _sofa(v):
    """Chesterfield-Sofa der 1930er (Ursprung Mitte am Boden, Blick +x, 0,9 x 2,16 x 0,9 m): Rollarmlehnen, Rautenheftung
    mit Knoepfen, drei unterschiedlich durchgesessene Sitzkissen mit Keder, gedrechselte Fuesse mit Messingrollen vorn."""
    M = _m()
    F = _fab(v)
    bm = bmesh.new()
    legs = [(-0.36, -1.0), (-0.36, 1.0), (0.37, -1.0), (0.37, 1.0), (-0.36, 0.0), (0.37, 0.0)]
    for (x, y) in legs:
        _foot(bm, x, y, h=0.12, r=0.036)
    _obj('fuesse', bm, M['walnut_d'], smooth=50)
    bm = bmesh.new()
    for (x, y) in legs:
        _ferrule(bm, x, y, r=0.036)
    _obj('huelsen', bm, M['brass_d'], smooth=50)
    bm = bmesh.new()
    g_cushion(bm, (0.0, 0.0, 0.275), (0.44, 1.02, 0.155), (0.03, 0.03, 0.035), n=(10, 20, 4),
              disp=lambda pos, nrm, p: 0.010 * (1 - p[2] ** 2) * max(0.0, nrm.x))
    _obj('unterbau', bm, F['fab'], smooth=60)
    bm = bmesh.new()
    g_tube(bm, [(0.442, y, 0.42) for y in (-0.86, -0.4, 0.0, 0.4, 0.86)], 0.0055, seg=5)
    _obj('unterbau_keder', bm, F['pipe'], smooth=70)
    for s in (-1, 1):
        _arm(F, s, -0.43, 0.44, 0.86, 1.035, 0.12, 0.74, 5 + s)
    _tufted_back(F, (-0.335, 0.0, 0.655), (0.085, 0.875, 0.245), (0.05, 0.05, 0.07), 0.07, (-0.335, 0.0, 0.42), 11, 4, 56, 18, 9)
    for k, (yc, sag, at) in enumerate(((-0.575, 0.034, (0.06, 0.05)), (0.0, 0.018, (0.10, -0.03)), (0.575, 0.026, (0.08, -0.04)))):
        _seat_cushion(F, (0.09, yc, 0.487), (0.345, 0.282, 0.066), sag, at, 20 + k)


def build_sofa():
    _sofa(0)


def build_sofa__v1():
    _sofa(1)



# --- Kastenmoebel: Profile -------------------------------------------------------------------------------------------------
PLINTH = [(-0.02, 0.0), (0.016, 0.0), (0.016, 0.010), (0.013, 0.014), (0.012, 0.03), (0.010, 0.042), (0.006, 0.052),
          (0.004, 0.062), (0.006, 0.068), (0.006, 0.074), (0.0, 0.078), (-0.02, 0.078)]
CROWN = [(-0.02, 0.0), (0.0, 0.0), (0.006, 0.004), (0.008, 0.010), (0.006, 0.016), (0.008, 0.020), (0.016, 0.026),
         (0.022, 0.034), (0.026, 0.046), (0.028, 0.058), (0.032, 0.064), (0.034, 0.070), (0.034, 0.082), (-0.02, 0.082)]
BEAD = [(-0.006, 0.0), (-0.005, 0.003), (-0.002, 0.0055), (0.002, 0.0055), (0.005, 0.003), (0.006, 0.0)]
OGEE = [(-0.004, 0.0), (-0.002, 0.006), (0.003, 0.009), (0.008, 0.0085), (0.012, 0.006), (0.016, 0.0025), (0.02, 0.0)]


def _raised_panel(bm_p, bm_m, x, y0, z0, y1, z1, mold=True, bevel=0.022):
    """Abgeplattete Fuellung (Fase zum Rand) mit Kehlleiste ringsum (Sichtseite +x)."""
    inner = [(y0 + bevel, z0 + bevel), (y1 - bevel, z0 + bevel), (y1 - bevel, z1 - bevel), (y0 + bevel, z1 - bevel)]
    outer = [(y0, z0), (y1, z0), (y1, z1), (y0, z1)]
    vo = [bm_p.verts.new((x, y, z)) for (y, z) in outer]
    vi = [bm_p.verts.new((x + 0.008, y, z)) for (y, z) in inner]
    vt = [bm_p.verts.new((x + 0.010, y + (0.004 if k in (0, 3) else -0.004), z + (0.004 if k in (0, 1) else -0.004)))
          for k, (y, z) in enumerate(inner)]
    for i in range(4):
        j = (i + 1) % 4
        bm_p.faces.new((vo[i], vo[j], vi[j], vi[i]))
        bm_p.faces.new((vi[i], vi[j], vt[j], vt[i]))
    bm_p.faces.new(vt)
    if mold:
        g_mold_rect(bm_m, x + 0.0005, y0, z0, y1, z1, OGEE)


def _wardrobe(v):
    """Kleiderschrank der 1930er (Ursprung Mitte am Boden, Blick +x, 0,67 x 1,16 x 2,03 m): Sockel und Kranz mit Profil,
    Seiten mit Fuellungen, zwei Tueren mit abgeplatteten Fuellungen und Kehlleisten, Schublade mit zwei Buegelgriffen,
    Schluesselschild mit Schluesselloch, Knoepfe, Baender; Variante 1: dunkler, linke Tuer mit Facettenspiegel
    (blinde Stellen am Rand)."""
    M = _m()
    tone = 0.78 if v == 1 else 1.0
    wood = common.material(f'schrank{v}', tuple(c * tone for c in (0.44, 0.27, 0.16)), look='wood', rough=0.45, r=0.7)
    wood_d = common.material(f'schrank_dunkel{v}', tuple(c * tone for c in (0.29, 0.17, 0.10)), look='wood', rough=0.5, r=0.8)
    wood_l = common.material(f'schrank_hell{v}', tuple(min(1.0, c * tone * 1.25) for c in (0.46, 0.30, 0.18)), look='wood', rough=0.4, r=0.6)
    X0, X1, HY = -0.30, 0.30, 0.55
    bm = bmesh.new()
    g_box(bm, (X0, -HY, 0.078), (X1, HY, 1.92), ch=0.003)                  # Korpus
    g_box(bm, (X0 - 0.01, -HY - 0.01, 1.92), (X1 + 0.012, HY + 0.01, 1.935), ch=0.002)
    _obj('korpus', bm, wood, smooth=30)
    bm = bmesh.new()
    g_mold_u(bm, X0 + 0.01, X1, HY, 0.0, PLINTH)
    g_mold_u(bm, X0 + 0.01, X1 + 0.012, HY + 0.01, 1.935, CROWN)
    _obj('sockel_kranz', bm, wood_d, smooth=35, recalc=True)
    bm = bmesh.new()
    g_box(bm, (X0, -HY - 0.03, 2.012), (X1 + 0.046, HY + 0.03, 2.03), ch=0.004)   # Deckplatte
    _obj('deckplatte', bm, wood, smooth=30)
    # Seitenfuellungen (aussen)
    bmp, bmm = bmesh.new(), bmesh.new()
    for s in (-1, 1):
        Mside = T(0, s * HY, 0) @ R(s * math.pi / 2, 'Z')
        bp, bmo = bmesh.new(), bmesh.new()
        _raised_panel(bp, bmo, 0.0, -0.22, 0.30, 0.22, 1.00)
        _raised_panel(bp, bmo, 0.0, -0.22, 1.08, 0.22, 1.82)
        for src, dst in ((bp, bmp), (bmo, bmm)):
            me = bpy.data.meshes.new('tmp')
            src.transform(Mside)
            src.to_mesh(me)
            src.free()
            dst.from_mesh(me)
            bpy.data.meshes.remove(me)
    # Tueren: Blatt, Fuellungen, Kehlleisten
    bmd = bmesh.new()
    mirror = None
    for s in (-1, 1):
        y_in, y_out = s * 0.004, s * 0.535
        ya, yb = min(y_in, y_out), max(y_in, y_out)
        g_box(bmd, (X1, ya, 0.42), (X1 + 0.022, yb, 1.86), ch=0.003)
        if v == 1 and s < 0:
            mirror = (ya + 0.07, 1.02, yb - 0.07, 1.78)
            g_mold_rect(bmm, X1 + 0.0225, ya + 0.07, 1.02, yb - 0.07, 1.78, OGEE)
        else:
            _raised_panel(bmp, bmm, X1 + 0.022, ya + 0.07, 1.02, yb - 0.07, 1.78)
        _raised_panel(bmp, bmm, X1 + 0.022, ya + 0.07, 0.50, yb - 0.07, 0.94)
    # Schublade
    g_box(bmd, (X1, -0.535, 0.10), (X1 + 0.024, 0.535, 0.40), ch=0.004)
    g_mold_rect(bmm, X1 + 0.0245, -0.50, 0.13, 0.50, 0.37, BEAD)
    _obj('tueren', bmd, wood, smooth=30)
    _obj('fuellungen', bmp, wood_l, smooth=25)
    _obj('leisten', bmm, wood_d, smooth=45)
    if mirror:
        y0, z0, y1, z1 = mirror
        glass = common.material('spiegel', (0.62, 0.64, 0.64), look='glass', rough=0.04, r=0.12, m=0.9)
        blind = common.material('spiegel_blind', (0.30, 0.29, 0.26), look='paint', rough=0.6, r=1.0)
        bm = bmesh.new()
        g_box(bm, (X1 + 0.016, y0, z0), (X1 + 0.021, y1, z1), ch=0.003)
        _obj('spiegel', bm, glass, smooth=30)
        bm = bmesh.new()  # blinde Stellen (abgeloeste Silberschicht) am Rand
        rng = random.Random(41)
        for k in range(9):
            yy = rng.choice((y0 + 0.012, y1 - 0.012, rng.uniform(y0 + 0.02, y1 - 0.02)))
            zz = rng.choice((z0 + 0.015, z1 - 0.015)) if yy not in (y0 + 0.012, y1 - 0.012) else rng.uniform(z0 + 0.05, z1 - 0.05)
            r_ = rng.uniform(0.006, 0.018)
            ring = [bm.verts.new((X1 + 0.0213, yy + math.cos(TAU * i / 9) * r_ * rng.uniform(0.6, 1.3),
                                  zz + math.sin(TAU * i / 9) * r_ * rng.uniform(0.6, 1.3))) for i in range(9)]
            bm.faces.new(ring)
        _obj('blind', bm, blind, smooth=None)
    # Beschlaege: Knoepfe, Schluesselschild, Baender, Buegelgriffe
    bm = bmesh.new()
    for s in (-1, 1):
        g_lathe(bm, [(0.0, 0.0), (0.013, 0.0), (0.013, 0.003), (0.006, 0.008), (0.005, 0.016), (0.012, 0.022), (0.014, 0.028),
                     (0.011, 0.034), (0.0, 0.036)], seg=14, M=T(X1 + 0.022, s * 0.045, 1.12) @ R(math.pi / 2, 'Y'))
        for z in (0.55, 1.20, 1.74):  # Baender (Kloben) an der Aussenkante
            g_lathe(bm, [(0.0, 0.0), (0.0065, 0.0), (0.0065, 0.07), (0.0045, 0.075), (0.0, 0.078)], seg=8,
                    M=T(X1 + 0.012, s * 0.542, z - 0.035))
    for yy in (-0.27, 0.27):  # Buegelgriffe der Schublade
        g_box(bm, (X1 + 0.024, yy - 0.055, 0.225), (X1 + 0.028, yy + 0.055, 0.275), ch=0.0015)
        arc = [(X1 + 0.03 + 0.022 * math.sin(math.pi * k / 10), yy - 0.045 + 0.09 * k / 10, 0.24 - 0.018 * math.sin(math.pi * k / 10))
               for k in range(11)]
        g_tube(bm, arc, 0.0035, seg=6)
    g_box(bm, (X1 + 0.022, 0.012, 1.04), (X1 + 0.025, 0.040, 1.10), ch=0.0012)   # Schluesselschild
    _obj('beschlaege', bm, M['brass'], smooth=40)
    bm = bmesh.new()  # Schluesselloch: Kreis und Schlitz, dunkel
    g_lathe(bm, [(0.0, 0.0), (0.0035, 0.0), (0.0035, 0.0006), (0.0, 0.0006)], seg=10, M=T(X1 + 0.0252, 0.026, 1.078) @ R(math.pi / 2, 'Y'))
    g_box(bm, (X1 + 0.0252, 0.0245, 1.060), (X1 + 0.0258, 0.0275, 1.078))
    _obj('schluesselloch', bm, M['black'], smooth=None)
    bm = bmesh.new()  # abgegriffenes Holz um Knoepfe und Schluessel (heller, glatter)
    for (yy, zz, ry, rz) in ((-0.045, 1.12, 0.03, 0.06), (0.045, 1.12, 0.03, 0.06), (0.026, 1.07, 0.025, 0.05), (0.0, 0.25, 0.25, 0.05)):
        ring = [bm.verts.new((X1 + 0.0225 if zz > 0.5 else X1 + 0.0245, yy + math.cos(TAU * i / 12) * ry, zz + math.sin(TAU * i / 12) * rz))
                for i in range(12)]
        bm.faces.new(ring)
    _obj('abrieb', bm, wood_l, smooth=None)


def build_wardrobe():
    _wardrobe(0)


def build_wardrobe__v1():
    _wardrobe(1)


# --- Buecherregal -------------------------------------------------------------------------------------------------------
LEATHER = [(0.36, 0.08, 0.06), (0.30, 0.17, 0.09), (0.12, 0.22, 0.13), (0.10, 0.13, 0.24), (0.08, 0.06, 0.05),
           (0.48, 0.34, 0.16), (0.55, 0.50, 0.40), (0.24, 0.10, 0.16)]


def _book(bm, x, y, z, d, t, h, lean=0.0, lie=False):
    """Buch: Ruecken gerundet (vorn, +x), Deckel; stehend (Dicke entlang y) oder liegend."""
    pts = [(0.0, -t / 2), (d - t * 0.22, -t / 2)]
    for k in range(1, 6):
        a = -math.pi / 2 + math.pi * k / 6
        pts.append((d - t * 0.22 + t * 0.22 * math.cos(a), t / 2 * math.sin(a)))
    pts += [(d - t * 0.22, t / 2), (0.0, t / 2)]
    if lie:
        Mx = T(x, y, z) @ R(math.pi / 2, 'X')
        g_prism(bm, pts, -h / 2, h / 2, M=Mx)
    else:
        Mx = T(x, y, z) @ R(lean, 'X')
        g_prism(bm, pts, 0.0, h, M=Mx)
    return Mx


def _bookcase(v):
    """Buecherregal aus Nussbaum (Ursprung hinten unten Mitte, Front +x, 0,35 x 0,9 x 2,0 m): Sockel und Kranz mit
    Profil, Perlstab an den Vorderkanten, Rueckwand mit Nut und Feder, Boeden mit Vorleimer; Buecher in Leder und
    Leinen mit Buenden und Goldschild, Luecken, angelehnte und liegende Stapel, Buchstuetze. Variante = andere Belegung."""
    M = _m()
    rng = random.Random(4100 + v * 37)
    D, W, H, t = 0.33, 0.45, 2.0, 0.022
    boards = [0.078, 0.45, 0.84, 1.23, 1.62]
    bm = bmesh.new()
    for s in (-1, 1):
        g_box(bm, (0.008, min(s * W, s * (W - t)), 0.0), (D, max(s * W, s * (W - t)), 1.92), ch=0.002)
    for z in boards[1:]:
        g_box(bm, (0.008, -W + t, z), (D - 0.004, W - t, z + 0.02), ch=0.0015)
        g_box(bm, (D - 0.022, -W + t, z - 0.012), (D, W - t, z + 0.02), ch=0.002)   # Vorleimer
    g_box(bm, (0.008, -W + t, boards[0]), (D, W - t, boards[0] + 0.02), ch=0.0015)
    g_box(bm, (0.0, -W - 0.004, 1.90), (D + 0.004, W + 0.004, 1.92), ch=0.002)
    _obj('korpus', bm, M['walnut'], smooth=30)
    bm = bmesh.new()
    g_box(bm, (0.0, -W, 0.0), (0.008, W, 1.92))
    _obj('rueckwand', bm, M['walnut_d'], smooth=None)
    bm = bmesh.new()  # Fugen der Rueckwand (Nut und Feder)
    for k in range(1, 8):
        yy = -W + k * (2 * W / 8)
        g_box(bm, (0.0081, yy - 0.0012, 0.08), (0.0086, yy + 0.0012, 1.90))
    _obj('fugen', bm, M['black'], smooth=None)
    bm = bmesh.new()
    g_mold_u(bm, 0.0, D, W, 0.0, PLINTH)
    g_mold_u(bm, 0.0, D + 0.004, W + 0.004, 1.92, CROWN)
    for s in (-1, 1):  # Perlstab an den Vorderkanten der Seiten
        g_tube(bm, [(D + 0.001, s * (W - 0.011), 0.08), (D + 0.001, s * (W - 0.011), 1.90)], 0.006, seg=8)
    _obj('profile', bm, M['walnut_d'], smooth=40, recalc=True)
    # Buecher
    mats = [common.material(f'buch{k}', c, look='fabric', rough=0.6, r=0.85 if k % 2 else 1.1) for k, c in enumerate(LEATHER)]
    page = common.material('buchschnitt', (0.80, 0.74, 0.60), look='paint', rough=0.9, r=1.2)
    gilt = M['gilt']
    bms = [bmesh.new() for _ in mats]
    bpage, bgold = bmesh.new(), bmesh.new()
    fill = (0.92, 0.85, 0.75, 0.6)[v % 4]
    tops = boards[1:] + [1.90]
    for b in range(5):
        z0 = boards[b] + 0.02
        room = tops[b] - z0
        y = -W + t + 0.006
        style = rng.random()
        while y < W - t - 0.03:
            if rng.random() > fill:  # Luecke, manchmal ein liegender Stapel
                gap = rng.uniform(0.06, 0.2)
                if gap > 0.12 and rng.random() < 0.5 and y + gap < W - t:
                    zz = z0
                    for k in range(rng.randint(2, 5)):
                        bt, bd, bw = rng.uniform(0.02, 0.04), rng.uniform(0.15, 0.22), min(gap - 0.01, rng.uniform(0.11, 0.2))
                        mi = rng.randrange(len(mats))
                        _book(bms[mi], 0.06 + rng.uniform(0, 0.03), y + gap / 2 + rng.uniform(-0.01, 0.01), zz + bt / 2, bd, bt, bw, lie=True)
                        zz += bt
                y += gap
                continue
            big = b == 0 or style < 0.25
            bt = rng.uniform(0.025, 0.06) if not big else rng.uniform(0.04, 0.075)
            bh = min(room - 0.025, rng.uniform(0.21, 0.30) if not big else rng.uniform(0.28, 0.36))
            bd = min(D - 0.03, rng.uniform(0.16, 0.23) if not big else rng.uniform(0.22, 0.28))
            if y + bt > W - t:
                break
            lean = 0.0
            if rng.random() < 0.05 and room > bh + 0.03:
                lean = -0.28
            x0 = D - bd - rng.uniform(0.012, 0.04)
            mi = rng.randrange(len(mats))
            yc = y + bt / 2
            _book(bms[mi], x0, yc, z0, bd, bt, bh, lean=lean)
            # Schnitt oben (Papier), leicht unter den Deckeln
            Mx = T(x0, yc, z0) @ R(lean, 'X')
            g_box(bpage, (0.004, -bt / 2 + 0.0025, bh - 0.0035), (bd - t * 0.0 - bt * 0.25, bt / 2 - 0.0025, bh + 0.0004), M=Mx)
            # Buende und Titelschild auf dem Ruecken
            xs = bd + bt * 0.0 + 0.0005
            if rng.random() < 0.6:
                for zz in (0.18, 0.32, 0.46, 0.60)[:rng.randint(2, 4)]:
                    g_box(bms[mi], (xs - 0.004, -bt * 0.42, bh * zz), (xs + 0.0025, bt * 0.42, bh * zz + 0.007), M=Mx)
            if rng.random() < 0.55:
                g_box(bgold, (xs - 0.002, -bt * 0.34, bh * 0.66), (xs + 0.001, bt * 0.34, bh * 0.66 + min(0.04, bh * 0.14)), M=Mx)
                g_box(bgold, (xs - 0.002, -bt * 0.36, bh * 0.08), (xs + 0.0008, bt * 0.36, bh * 0.08 + 0.003), M=Mx)
            y += bt + rng.uniform(0.0, 0.003)
            if lean:
                y += bh * 0.28
    for k, b_ in enumerate(bms):
        if len(b_.verts):
            _obj(f'buecher{k}', b_, mats[k], smooth=40, recalc=True)
        else:
            b_.free()
    _obj('buchschnitt', bpage, page, smooth=None)
    _obj('goldpraegung', bgold, gilt, smooth=None)
    # Buchstuetze aus Messing auf dem obersten Boden
    bm = bmesh.new()
    g_box(bm, (0.14, 0.20, boards[4] + 0.02), (0.29, 0.203, boards[4] + 0.17), ch=0.001)
    g_box(bm, (0.14, 0.15, boards[4] + 0.02), (0.29, 0.203, boards[4] + 0.023), ch=0.001)
    _obj('buchstuetze', bm, M['brass_d'], smooth=None)


def build_bookcase():
    _bookcase(0)


def build_bookcase__v1():
    _bookcase(1)


def build_bookcase__v2():
    _bookcase(2)


def build_bookcase__v3():
    _bookcase(3)


# --- Hotelbett -------------------------------------------------------------------------------------------------------------
def _drape(bm, x0, x1, top, hw, hang, r_edge, nx, seed, amp=0.022, lam=0.26, crown=0.012, ns_top=10, ns_side=6, sides=(-1, 1)):
    """Decke ueber dem Bett: oben flach (leicht gewoelbt), ueber die Kante (Radius r_edge), dann haengend mit Falten,
    die nach unten groesser werden. Gitter entlang x (nx) und quer (oben ns_top, je Seite ns_side). Normalen aussen."""
    rng = random.Random(seed)
    ph = [rng.uniform(0, TAU) for _ in range(3)]
    cols = []
    # Querschnitt: Parameter s von -1 (unten links) bis +1 (unten rechts)
    prof = []
    for k in range(ns_side, 0, -1):  # linke Seite, von unten nach oben
        prof.append(('side', -1, k / ns_side))
    for k in range(4, -1, -1):
        prof.append(('edge', -1, k / 4))
    for k in range(1, ns_top):
        prof.append(('top', 0, -1 + 2 * k / ns_top))
    for k in range(5):
        prof.append(('edge', 1, k / 4))
    for k in range(1, ns_side + 1):
        prof.append(('side', 1, k / ns_side))
    prof = [p for p in prof if p[0] != 'side' or p[1] in sides] if len(sides) < 2 else prof
    grid = []
    for i in range(nx + 1):
        x = x0 + (x1 - x0) * i / nx
        row = []
        for (kind, sgn, t) in prof:
            if kind == 'top':
                y = t * (hw - r_edge)
                z = top + crown * (1 - t * t)
            elif kind == 'edge':
                a = math.pi / 2 * t  # t=0 oben, t=1 seitlich
                y = sgn * (hw - r_edge + math.sin(a) * r_edge)
                z = top - r_edge + math.cos(a) * r_edge
            else:
                dz = hang * t
                f = (dz / hang) ** 1.3
                fold = amp * f * (0.55 + 0.45 * math.sin(TAU * x / lam + ph[0] + sgn) + 0.2 * math.sin(TAU * x / (lam * 0.47) + ph[1]))
                y = sgn * (hw + 0.012 * f + max(0.0, fold))
                z = top - r_edge - dz
                x += 0.0
                row.append(bm.verts.new((x + 0.006 * f * math.sin(TAU * x / lam + ph[2]), y, z)))
                continue
            row.append(bm.verts.new((x, y, z)))
        grid.append(row)
    for i in range(nx):
        for j in range(len(prof) - 1):
            bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    return grid


def build_hotel_bed():
    """Doppelbett der 1930er (Mitte im Ursprung, Kopfteil an der Wand bei x = -1,1, 2,15 x 1,6 m): Nussbaum mit
    gedrechselten Pfosten und Knaeufen, Kopfteil mit Bogen, Abdeckleiste und Wurzelfurnier-Fuellung, Fussteil, Zargen
    mit Profil; Matratze, Laken mit Umschlag, rot-goldene Tagesdecke mit Faltenwurf und Borte, zwei Kissen."""
    M = _m()
    sheet = common.material('laken', (0.86, 0.84, 0.78), look='fabric', rough=0.9, r=1.15)
    spread = common.material('tagesdecke', (0.42, 0.07, 0.06), look='fabric', tex='carpet_red_pattern', rough=0.85, r=1.1)
    trim = common.material('borte', (0.72, 0.56, 0.26), look='fabric', rough=0.6, r=0.8, m=0.2)
    W = 0.80
    # Kopfteil mit Bogen
    out = [(-W, 0.28), (W, 0.28), (W, 1.02)]
    arch = []
    for k in range(1, 16):
        u = -1 + 2 * k / 16
        arch.append((-u * W, 1.02 + 0.13 * math.cos(u * math.pi / 2) ** 1.5))
    out += arch + [(-W, 1.02)]
    bm = bmesh.new()
    g_plate(bm, out, -1.10, -1.055)
    _obj('kopfteil', bm, M['walnut'], smooth=30, recalc=True)
    bm = bmesh.new()
    cap = [(-0.004, -0.030), (0.010, -0.032), (0.022, -0.027), (0.030, -0.016), (0.033, 0.0), (0.030, 0.016), (0.022, 0.027),
           (0.010, 0.032), (-0.004, 0.030)]
    top_path = [(-1.0775, y, z) for (y, z) in [(W, 1.02)] + arch + [(-W, 1.02)]]
    g_sweep(bm, cap, top_path, up=(1, 0, 0))
    _obj('abdeckleiste', bm, M['walnut_d'], smooth=40, recalc=True)
    bm = bmesh.new()  # Wurzelfurnier-Fuellung mit Perlstab
    inner = [(-0.62, 0.62), (0.62, 0.62), (0.62, 0.96)]
    for k in range(1, 12):
        u = -1 + 2 * k / 12
        inner.append((-u * 0.62, 0.96 + 0.08 * math.cos(u * math.pi / 2) ** 1.5))
    inner.append((-0.62, 0.96))
    g_plate(bm, inner, -1.055, -1.048)
    _obj('furnier', bm, M['veneer'], smooth=30, recalc=True)
    bm = bmesh.new()
    g_sweep(bm, BEAD, [(-1.048, y, z) for (y, z) in inner], closed=True, up=(1, 0, 0), pclosed=False)
    _obj('perlstab', bm, M['walnut_d'], smooth=50)
    # Fussteil
    bm = bmesh.new()
    g_box(bm, (1.0, -W, 0.24), (1.045, W, 0.60), ch=0.004)
    _raised_panel(bm, bm, 1.045, -0.66, 0.31, 0.66, 0.54, mold=False)
    _obj('fussteil', bm, M['walnut'], smooth=30)
    bm = bmesh.new()
    g_mold_rect(bm, 1.0455, -0.66, 0.31, 0.66, 0.54, OGEE)
    g_sweep(bm, cap, [(1.0225, W + 0.01, 0.60), (1.0225, -W - 0.01, 0.60)], up=(1, 0, 0))
    _obj('fuss_leisten', bm, M['walnut_d'], smooth=40)
    # Pfosten (gedrechselt) mit Knaeufen
    post = [(0.0, 0.0), (0.030, 0.0), (0.034, 0.012), (0.036, 0.05), (0.030, 0.07), (0.026, 0.10), (0.030, 0.13), (0.034, 0.15),
            (0.034, 0.24), (0.031, 0.26), (0.034, 0.28)]
    bm = bmesh.new()
    for (x, ztop) in ((-1.075, 1.19), (1.03, 0.70)):
        for s in (-1, 1):
            pr = post + [(0.034, ztop - 0.22), (0.028, ztop - 0.19), (0.024, ztop - 0.12), (0.03, ztop - 0.08), (0.036, ztop - 0.06),
                         (0.026, ztop - 0.045), (0.018, ztop - 0.04), (0.034, ztop - 0.02), (0.04, ztop + 0.0), (0.034, ztop + 0.03),
                         (0.02, ztop + 0.05), (0.0, ztop + 0.062)]
            g_lathe(bm, pr, seg=16, M=T(x, s * (W + 0.04), 0.0))
    _obj('pfosten', bm, M['walnut_d'], smooth=40)
    # Zargen
    bm = bmesh.new()
    for s in (-1, 1):
        g_box(bm, (-1.055, s * W - 0.028, 0.20), (1.0, s * W + 0.028, 0.36), ch=0.006)
        g_sweep(bm, [(-0.01, 0.0), (0.0, 0.006), (0.01, 0.0)], [(-1.055, s * (W + 0.029), 0.33), (1.0, s * (W + 0.029), 0.33)],
                up=(0, s, 0), pclosed=False)
    _obj('zargen', bm, M['walnut'], smooth=35)
    # Matratze (fast ganz bedeckt)
    bm = bmesh.new()
    g_cushion(bm, (-0.03, 0.0, 0.45), (1.0, W - 0.03, 0.105), (0.05, 0.05, 0.05), n=(10, 8, 3))
    _obj('matratze', bm, sheet, smooth=60)
    # Laken am Kopfende, Tagesdecke, Umschlag
    bm = bmesh.new()
    _drape(bm, -1.035, -0.40, 0.555, W - 0.02, 0.10, 0.05, 6, 3, amp=0.006)
    _obj('laken', bm, sheet, smooth=60)
    bm = bmesh.new()
    gs = _drape(bm, -0.44, 1.0, 0.565, W - 0.02, 0.30, 0.055, 22, 7)
    _obj('tagesdecke', bm, spread, smooth=60)
    ob = bpy.context.scene.objects['tagesdecke']
    sol = ob.modifiers.new('dicke', 'SOLIDIFY')
    sol.thickness, sol.offset = 0.008, -1.0
    bm = bmesh.new()  # Borte am Saum (folgt der Decke)
    for row_side in (0, -1):
        for i in range(len(gs) - 1):
            a0, a1 = gs[i][row_side].co, gs[i + 1][row_side].co
            b0, b1 = gs[i][row_side + (1 if row_side == 0 else -1)].co, gs[i + 1][row_side + (1 if row_side == 0 else -1)].co
            out_dir = Vector((0, 1 if row_side == -1 else -1, 0))
            q = [a0 + out_dir * 0.0025, a1 + out_dir * 0.0025, a1.lerp(b1, 0.55) + out_dir * 0.0025, a0.lerp(b0, 0.55) + out_dir * 0.0025]
            f = bm.faces.new([bm.verts.new(p) for p in q])
            f.normal_update()
            if f.normal.dot(out_dir) < 0:
                f.normal_flip()
    _obj('borte', bm, trim, smooth=60)
    bm = bmesh.new()
    _drape(bm, -0.52, -0.38, 0.585, W - 0.005, 0.16, 0.06, 2, 5, amp=0.01, crown=0.016)
    _obj('umschlag', bm, sheet, smooth=60)
    ob = bpy.context.scene.objects['umschlag']
    sol = ob.modifiers.new('dicke', 'SOLIDIFY')
    sol.thickness, sol.offset = 0.006, -1.0
    # Kissen, leicht an das Kopfteil gelehnt
    for k, s in enumerate((-1, 1)):
        bm = bmesh.new()
        piv = (-0.69, s * 0.38, 0.565)
        Mk = T(*piv) @ R(0.30 + 0.05 * k, 'Y') @ T(-piv[0], -piv[1], -piv[2])
        g_cushion(bm, (-0.86, s * 0.38, 0.635), (0.17, 0.33, 0.075), (0.07, 0.10, 0.07), n=(8, 12, 4),
                  disp=lambda pos, nrm, p: 0.025 * (1 - p[0] ** 2) * (1 - p[1] ** 2) * abs(nrm.z) - 0.012 * math.exp(-(pos.y ** 2 + pos.x ** 2) / 0.02) * max(0.0, nrm.z),
                  M=Mk)
        _obj(f'kissen{k}', bm, sheet, smooth=70)


# ======== V12 (noch nicht neu gebaut) ========
from models.models_l2 import _cyl  # noqa: E402


def _m_old():
    M = _m()
    M["gold"] = M["gilt"]
    return M


# --- Vertaefelung -----------------------------------------------------------------------------------------------------
def build_wainscot():
    """Wandvertaefelung (Wandflaeche x = 0, 1 m entlang y, 1,0 m hoch): Sockel, Fuellung mit Rahmen, Brueckungsleiste."""
    M = _m_old()
    common.box('grund', (0.0, -0.5, 0.0), (0.018, 0.5, 0.98), M['walnut_d'])
    common.box('sockel', (0.0, -0.5, 0.0), (0.03, 0.5, 0.14), M['walnut_d'], bevel=0.004)
    common.box('leiste', (0.0, -0.5, 0.96), (0.04, 0.5, 1.02), M['walnut'], bevel=0.008, segments=2)
    common.box('rahmen_o', (0.018, -0.42, 0.82), (0.03, 0.42, 0.86), M['walnut'], bevel=0.004)
    common.box('rahmen_u', (0.018, -0.42, 0.22), (0.03, 0.42, 0.26), M['walnut'], bevel=0.004)
    common.box('rahmen_l', (0.018, 0.38, 0.22), (0.03, 0.42, 0.86), M['walnut'], bevel=0.004)
    common.box('rahmen_r', (0.018, -0.42, 0.22), (0.03, -0.38, 0.86), M['walnut'], bevel=0.004)
    common.box('fuellung', (0.018, -0.36, 0.28), (0.026, 0.36, 0.8), M['walnut'], bevel=0.01, segments=2)


# --- Zimmertueren mit Nummer ----------------------------------------------------------------------------------------------
def _hotel_door(number):
    """Verschlossene Zimmertuer (Wandteil, Wandflaeche x = 0, Boden z = 0): Nussbaum, Kassetten, Messingknauf, goldene
    Nummernplakette (die Nummern folgen keinem Muster, Wiki)."""
    M = _m_old()
    W, H = 0.94, 2.15
    for (lo, hi) in (((0.0, W / 2, 0.0), (0.07, W / 2 + 0.08, H + 0.08)), ((0.0, -W / 2 - 0.08, 0.0), (0.07, -W / 2, H + 0.08)),
                     ((0.0, -W / 2 - 0.08, H), (0.07, W / 2 + 0.08, H + 0.08))):
        common.box('zarge', lo, hi, M['walnut_d'], bevel=0.008, segments=2)
    common.box('blatt', (0.0, -W / 2, 0.0), (0.04, W / 2, H), M['walnut'], bevel=0.004)
    for (z0, z1) in ((0.15, 0.95), (1.1, 1.95)):
        for (y0, y1) in ((-W / 2 + 0.1, -0.03), (0.03, W / 2 - 0.1)):
            common.box('kassette', (0.04, y0, z0), (0.05, y1, z1), M['walnut_d'], bevel=0.012, segments=2)
    _cyl('knauf_hals', (0.04, -W / 2 + 0.1, 1.0), (0.09, -W / 2 + 0.1, 1.0), 0.012, M['brass'], seg=12)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=0.03, location=(0.1, -W / 2 + 0.1, 1.0))
    bpy.context.active_object.data.materials.append(M['brass'])
    common.box('schloss', (0.04, -W / 2 + 0.07, 0.9), (0.046, -W / 2 + 0.13, 0.96), M['brass'], bevel=0.004)
    hw = 0.05 + 0.03 * len(str(number))
    common.box('plakette', (0.04, -hw, 1.6), (0.048, hw, 1.74), M['gold'], bevel=0.006, segments=2)
    _text('nummer', str(number), 0.09, M['black'], Vector((0.049, 0.0, 1.67)), depth=0.0015)


def build_hotel_door_a():
    _hotel_door(117)


def build_hotel_door_b():
    _hotel_door(2041)


def build_hotel_door_c():
    _hotel_door(31)


def build_hotel_door_d():
    _hotel_door(608)


def build_hotel_door_e():
    _hotel_door(9)


def build_hotel_door_f():
    _hotel_door(343)


def build_placard_beverly():
    """Kleines Silberschild "The Beverly Room" (Wiki) neben dem Eingang (Wandteil, Mitte im Ursprung)."""
    M = _m_old()
    common.box('schild', (0.0, -0.26, -0.06), (0.012, 0.26, 0.06), M['silver'], bevel=0.005, segments=2)
    _text('schrift', 'The Beverly Room', 0.055, M['black'], Vector((0.013, 0.0, 0.0)), depth=0.0012)


# --- Aufzug mit Kupfergitter ------------------------------------------------------------------------------------------------
def build_elevator_cage():
    """Alter Aufzug im Durchbruch (Oeffnung 1,4 m, Laibung 1 m): Kupferrahmen, Scherengitter halb offen, halbrunde
    Stockwerksanzeige mit Zeiger ueber der Tuer (Seite -x)."""
    M = _m_old()
    W, H = 1.4, 2.25
    for s in (-1, 1):
        common.box(f'pfosten{s}', (-0.52, s * W / 2 - (0.08 if s > 0 else 0), 0.0), (-0.44, s * W / 2 + (0.0 if s > 0 else 0.08), H), M['copper'], bevel=0.008)
    common.box('sturz', (-0.52, -W / 2, H - 0.08), (-0.44, W / 2, H + 0.04), M['copper'], bevel=0.008)
    # Scherengitter: Rauten aus Staeben, zur Haelfte zusammengeschoben (y > 0 frei)
    for k in range(7):
        y = -W / 2 + 0.05 + k * 0.09
        for z0 in (0.05, 0.75, 1.45):
            ob = common.box(f'schere{k}{z0}', (-0.006, -0.008, 0.0), (0.006, 0.008, 0.78), M['brass'])
            ob.matrix_world = Matrix.Translation((-0.4, y, z0)) @ Matrix.Rotation(0.12 if k % 2 else -0.12, 4, 'X')
    for z in (0.05, 1.1, 2.15):
        common.box(f'schiene{z}', (-0.41, -W / 2, z), (-0.39, -W / 2 + 0.66, z + 0.025), M['brass'])
    # Anzeige: Halbkreis mit Zeiger
    bm = bmesh.new()
    pts = [bm.verts.new((-0.53, 0.0, H + 0.12))]
    for k in range(17):
        a = math.pi * k / 16
        pts.append(bm.verts.new((-0.53, math.cos(a) * 0.22, H + 0.12 + math.sin(a) * 0.22)))
    for k in range(1, 17):
        f = bm.faces.new((pts[0], pts[k], pts[k + 1]))
        f.normal_update()
        if f.normal.x > 0:
            f.normal_flip()
    common.obj_from_bmesh('zifferblatt', bm, M['cream'])
    ob = common.box('zeiger', (-0.004, -0.005, 0.0), (0.0, 0.005, 0.18), M['black'])
    ob.matrix_world = Matrix.Translation((-0.535, 0.0, H + 0.12)) @ Matrix.Rotation(0.6, 4, 'X')
    common.box('anzeige_rahmen', (-0.54, -0.25, H + 0.09), (-0.52, 0.25, H + 0.12), M['brass'])


# --- Beverly Room: Mahjong-Tisch, Kronleuchter, Grammophon ---------------------------------------------------------------
def build_mahjong_table():
    """Kleiner, verzierter Tisch (0,9 m) mit einer unfertigen Mahjong-Partie (Mauern aus Steinen, abgelegte Steine) und
    Getraenken (Glaeser, Flaschen) - Wiki: Beverly Room."""
    M = _m_old()
    felt = common.material('filz', (0.14, 0.30, 0.20), look='fabric', rough=0.9)
    tile = common.material('stein', (0.90, 0.88, 0.80), look='ceramic', rough=0.35)
    tile_b = common.material('stein_ruecken', (0.18, 0.42, 0.30), look='ceramic', rough=0.35)
    common.box('platte', (-0.45, -0.45, 0.72), (0.45, 0.45, 0.76), M['walnut'], bevel=0.015, segments=2)
    common.box('filz', (-0.4, -0.4, 0.76), (0.4, 0.4, 0.765), felt)
    _cyl('saeule', (0, 0, 0.1), (0, 0, 0.72), 0.06, M['walnut_d'], seg=16)
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        ob = common.box(f'fuss{k}', (0.0, -0.03, 0.0), (0.38, 0.03, 0.08), M['walnut_d'], bevel=0.015)
        ob.matrix_world = Matrix.Rotation(a, 4, 'Z')
    rng = random.Random(8)
    tw, td, th = 0.024, 0.017, 0.032
    for side in range(4):
        R = Matrix.Rotation(side * math.pi / 2, 4, 'Z')
        n = rng.randint(9, 15)
        for k in range(n):
            for lvl in range(2):
                if lvl == 1 and rng.random() < 0.3:
                    continue
                ob = common.box('mauer', (-tw / 2, -td / 2, 0.0), (tw / 2, td / 2, th / 2), tile_b if lvl else tile)
                ob.matrix_world = R @ Matrix.Translation((-0.18 + k * tw, -0.26, 0.765 + lvl * th / 2))
    for k in range(14):
        ob = common.box('ablage', (-tw / 2, -th / 2, 0.0), (tw / 2, th / 2, td / 2), tile)
        ob.matrix_world = Matrix.Translation((rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), 0.765)) @ Matrix.Rotation(rng.uniform(0, 3), 4, 'Z')
    for k, (x, y) in enumerate(((0.32, 0.3), (-0.3, 0.33), (0.3, -0.32))):
        _cyl(f'glas{k}', (x, y, 0.765), (x, y, 0.86), 0.03, M['glass'], seg=16)
        _cyl(f'inhalt{k}', (x, y, 0.767), (x, y, 0.82), 0.026, common.material('whisky', (0.62, 0.36, 0.12), look='glass', rough=0.1), seg=16)
    for k, (x, y) in enumerate(((-0.33, -0.3), (0.0, 0.36))):
        _cyl(f'flasche{k}', (x, y, 0.765), (x, y, 0.98), 0.04, common.material('flasche', (0.12, 0.25, 0.12), look='glass', rough=0.1), seg=16)
        _cyl(f'hals{k}', (x, y, 0.98), (x, y, 1.06), 0.014, common.material('flasche', (0.12, 0.25, 0.12), look='glass', rough=0.1), seg=12)


def build_chandelier_grand():
    """Grosser Kronleuchter (Deckenebene z = 0, haengt 1,8 m herab): Kette, Mittelschaft, zwei Arm-Kraenze mit
    Kerzenbirnen, Kristallbehang."""
    M = _m_old()
    bulb = common.material('kerze', (1.0, 0.86, 0.62), look='glow', glow=3.0)
    crystal = common.material('kristall', (0.92, 0.94, 0.96), look='glass', rough=0.02)
    _cyl('kette', (0, 0, 0), (0, 0, -0.6), 0.015, M['brass'], seg=8)
    _cyl('schaft', (0, 0, -0.6), (0, 0, -1.55), 0.035, M['brass'], seg=14)
    for (z, r) in ((-0.75, 0.09), (-1.2, 0.14), (-1.55, 0.1)):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=10, radius=r, location=(0, 0, z))
        ob = bpy.context.active_object
        ob.scale = (1, 1, 0.6)
        ob.data.materials.append(M['brass'])
        ob.data.shade_smooth()
    for (zr, R, n) in ((-1.2, 0.75, 10), (-0.85, 0.45, 6)):
        bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=0.012, major_segments=40, minor_segments=6, location=(0, 0, zr))
        bpy.context.active_object.data.materials.append(M['brass'])
        for k in range(n):
            a = 2 * math.pi * k / n
            x, y = math.cos(a) * R, math.sin(a) * R
            _cyl(f'arm{zr}{k}', (0, 0, zr - 0.05), (x, y, zr), 0.01, M['brass'], seg=6)
            _cyl(f'teller{zr}{k}', (x, y, zr), (x, y, zr + 0.015), 0.04, M['brass'], seg=12)
            _cyl(f'kerze{zr}{k}', (x, y, zr + 0.015), (x, y, zr + 0.11), 0.012, M['cream'], seg=8)
            bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=8, radius=0.02, location=(x, y, zr + 0.14))
            ob = bpy.context.active_object
            ob.scale = (1, 1, 1.5)
            ob.data.materials.append(bulb)
            for j in range(2):
                bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.018, location=(x * (0.9 - 0.15 * j), y * (0.9 - 0.15 * j), zr - 0.08 - 0.06 * j))
                ob = bpy.context.active_object
                ob.scale = (0.7, 0.7, 1.6)
                ob.data.materials.append(crystal)


def build_gramophone():
    """Grammophon auf kleinem Schrank (Boden z = 0): Schrank, Plattenteller, Tonarm, Trichter (Wiki: Jazz von alten
    Plattenspielern)."""
    M = _m_old()
    common.box('schrank', (-0.3, -0.25, 0.0), (0.3, 0.25, 0.75), M['walnut'], bevel=0.015, segments=2)
    common.box('front', (0.3, -0.21, 0.08), (0.31, 0.21, 0.66), M['walnut_d'], bevel=0.01)
    common.box('kasten', (-0.22, -0.2, 0.75), (0.22, 0.2, 0.9), M['walnut_d'], bevel=0.01)
    _cyl('teller', (0.0, 0.0, 0.9), (0.0, 0.0, 0.915), 0.16, M['black'], seg=32)
    _cyl('platte', (0.0, 0.0, 0.915), (0.0, 0.0, 0.92), 0.15, M['black'], seg=32)
    _cyl('arm', (-0.15, 0.15, 0.93), (0.02, 0.05, 0.94), 0.008, M['brass'], seg=8)
    # Trichter: Drehkoerper, schraeg nach oben
    bm = bmesh.new()
    rings = []
    for k in range(13):
        t = k / 12
        z, r = t * 0.55, 0.02 + 0.28 * t ** 2.2
        rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / 28), r * math.sin(2 * math.pi * i / 28), z)) for i in range(28)])
    for k in range(12):
        for i in range(28):
            j = (i + 1) % 28
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('trichter', bm, M['brass'])
    mod = ob.modifiers.new('dicke', 'SOLIDIFY')
    mod.thickness = 0.004
    ob.data.shade_smooth()
    ob.matrix_world = Matrix.Translation((-0.15, 0.15, 0.94)) @ Matrix.Rotation(-0.9, 4, 'Y') @ Matrix.Rotation(0.6, 4, 'X')


def build_candle_sconce():
    """Kerzenwandleuchter (Wandteil, Mitte im Ursprung): Messingplatte, zwei Arme, Kerzen mit Flammenbirne (elektrisch)."""
    M = _m_old()
    flame = common.material('flamme', (1.0, 0.80, 0.50), look='glow', glow=3.5)
    common.box('platte', (0.0, -0.06, -0.12), (0.02, 0.06, 0.12), M['brass'], bevel=0.01, segments=2)
    for s in (-1, 1):
        _cyl(f'arm{s}', (0.02, 0.0, -0.04), (0.14, s * 0.13, 0.02), 0.009, M['brass'], seg=8)
        _cyl(f'teller{s}', (0.14, s * 0.13, 0.02), (0.14, s * 0.13, 0.035), 0.035, M['brass'], seg=12)
        _cyl(f'kerze{s}', (0.14, s * 0.13, 0.035), (0.14, s * 0.13, 0.15), 0.011, M['cream'], seg=8)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=8, radius=0.014, location=(0.14, s * 0.13, 0.175))
        ob = bpy.context.active_object
        ob.scale = (1, 1, 1.8)
        ob.data.materials.append(flame)


# --- Kesselraum: Ofen ------------------------------------------------------------------------------------------------------
def build_furnace():
    """Grosser Ofen (2,0 x 1,6 x 2,2 m) mit Ziegelmantel, Eisentuer, gluehendem Schlitz, Rauchrohr nach oben."""
    M = _m_old()
    glow = common.material('glut', (1.0, 0.42, 0.12), look='glow', glow=4.0)
    common.box('mantel', (0.0, -1.0, 0.0), (1.6, 1.0, 2.2), M['brick'], bevel=0.02)
    common.box('platte', (-0.02, -1.02, 2.2), (1.62, 1.02, 2.3), M['iron'], bevel=0.01)
    common.box('tuer', (1.6, -0.4, 0.4), (1.66, 0.4, 1.1), M['iron'], bevel=0.01)
    common.box('schlitz', (1.66, -0.3, 0.55), (1.665, 0.3, 0.62), glow)
    for y in (-0.3, 0.3):
        _cyl(f'band{y}', (1.66, y, 0.75), (1.7, y, 0.75), 0.03, M['iron'], seg=10)
    _cyl('griff', (1.7, -0.3, 0.75), (1.7, 0.3, 0.75), 0.012, M['iron'], seg=8)
    _cyl('rohr', (0.6, 0.0, 2.3), (0.6, 0.0, 3.6), 0.2, M['iron'], seg=20)
    common.box('asche', (1.66, -0.5, 0.0), (1.9, 0.5, 0.03), M['iron'])


# --- Uebergaenge: Metro-Wagen (nach 903) und helles Fenster (nach 188) -----------------------------------------------------
def build_metro_car_end():
    """Stirnseite eines Metro-Wagens in einer Nische (Boden z = 0, Wagen steht entlang x hinter der Nischenwand bei
    x = 0, die offene Seitentuer liegt in der Nische): Wagenkasten, Fenster mit Licht, Schiebetuer offen, Innenraum."""
    M = _m_old()
    body = common.material('wagen', (0.62, 0.66, 0.68), look='metal', rough=0.4, metal=0.7)
    stripe = common.material('streifen', (0.22, 0.34, 0.52), look='paint', rough=0.5)
    light = common.material('innenlicht', (1.0, 0.96, 0.84), look='glow', glow=0.9)
    seat = common.material('sitz', (0.28, 0.32, 0.42), look='fabric', rough=0.9)
    L, W, H = 6.0, 2.6, 2.7
    # Kasten (Seite zur Nische bei y = 0 offen, damit die Tuer hineinfuehrt): Boden, Dach, Rueckwand, Stirnwaende
    common.box('boden', (-L / 2, 0.0, 0.0), (L / 2, W, 0.08), body)
    common.box('dach', (-L / 2, 0.0, H - 0.08), (L / 2, W, H), body, bevel=0.05, segments=3)
    common.box('rueck', (-L / 2, W - 0.06, 0.0), (L / 2, W, H), body)
    for s in (-1, 1):
        common.box(f'stirn{s}', (s * L / 2 - 0.06 if s > 0 else -L / 2, 0.0, 0.0), (s * L / 2 if s > 0 else -L / 2 + 0.06, W, H), body)
    for (x0, x1) in ((-L / 2, -0.7), (0.7, L / 2)):
        common.box(f'seite{x0}', (x0, 0.0, 0.0), (x1, 0.06, H), body)
        common.box(f'streifen{x0}', (x0, -0.004, 0.9), (x1, 0.0, 1.0), stripe)
        for k in range(2):
            wx = x0 + 0.4 + k * 0.9
            if wx + 0.7 < x1:
                common.box(f'fenster{x0}{k}', (wx, -0.005, 1.2), (wx + 0.7, 0.0, 1.9), light)
    common.box('tuer_sturz', (-0.7, 0.0, 2.05), (0.7, 0.06, H), body)
    common.box('tuerfluegel', (0.7, -0.02, 0.08), (1.4, 0.0, 2.05), body)  # aufgeschoben
    common.box('lichtband', (-L / 2 + 0.2, W / 2 - 0.1, H - 0.1), (L / 2 - 0.2, W / 2 + 0.1, H - 0.08), light)
    for x in (-2.2, -1.4, 1.4, 2.2):
        common.box(f'bank{x}', (x - 0.35, W - 0.6, 0.08), (x + 0.35, W - 0.06, 0.48), seat, bevel=0.02)
        common.box(f'lehne{x}', (x - 0.35, W - 0.18, 0.48), (x + 0.35, W - 0.06, 1.0), seat, bevel=0.02)
    for x in (-1.0, 0.0, 1.0):
        _cyl(f'stange{x}', (x, W / 2, 0.08), (x, W / 2, H - 0.08), 0.02, M['silver'], seg=10)


def build_window_bright():
    """Fenster (Wandteil, Mitte im Ursprung, 1,2 x 1,6 m) mit hellem, weissem Licht dahinter - der Weg nach Level 188."""
    M = _m_old()
    light = common.material('aussen', (1.0, 1.0, 0.98), look='glow', glow=2.2)
    frame = M['cream']
    W, H = 1.2, 1.6
    common.box('licht', (-0.3, -W / 2, -H / 2), (-0.28, W / 2, H / 2), light)
    for (y0, y1, z0, z1) in ((-W / 2, W / 2, -H / 2 - 0.07, -H / 2), (-W / 2, W / 2, H / 2, H / 2 + 0.07),
                             (-W / 2 - 0.07, -W / 2, -H / 2 - 0.07, H / 2 + 0.07), (W / 2, W / 2 + 0.07, -H / 2 - 0.07, H / 2 + 0.07)):
        common.box('rahmen', (-0.28, y0, z0), (0.05, y1, z1), frame, bevel=0.005)
    common.box('sprosse_v', (-0.02, -0.02, -H / 2), (0.02, 0.02, H / 2), frame)
    for z in (-0.27, 0.27):
        common.box(f'sprosse_h{z}', (-0.02, -W / 2, z - 0.02), (0.02, W / 2, z + 0.02), frame)
    common.box('bank', (0.0, -W / 2 - 0.1, -H / 2 - 0.12), (0.18, W / 2 + 0.1, -H / 2 - 0.07), M['walnut'], bevel=0.006)


# --- Bildnis -----------------------------------------------------------------------------------------------------------------
def build_painting_portrait():
    """Altes Bildnis (0,7 x 0,9 m, Wandteil): breiter Goldrahmen, nachgedunkelter Grund mit hellerem Schein hinter dem
    Kopf, Brustbild eines Herrn (Haar, Gesicht, Augen, Hemd, Krawatte, Rock) - Wiki: man fuehlt sich von alten Gemaelden
    beobachtet."""
    M = _m_old()
    ground = common.material('grund', (0.09, 0.07, 0.05), look='paint', rough=0.6)
    halo = common.material('schein', (0.22, 0.16, 0.10), look='paint', rough=0.6)
    coat = common.material('rock', (0.05, 0.05, 0.06), look='paint', rough=0.6)
    skin = common.material('haut', (0.62, 0.46, 0.34), look='paint', rough=0.6)
    hair = common.material('haar', (0.12, 0.08, 0.05), look='paint', rough=0.6)
    shirt = common.material('kragen', (0.74, 0.70, 0.62), look='paint', rough=0.6)
    lip = common.material('lippe', (0.42, 0.24, 0.2), look='paint', rough=0.6)
    W, H, fw = 0.7, 0.9, 0.09
    for (lo, hi) in (((0.0, -W / 2 - fw, H / 2), (0.06, W / 2 + fw, H / 2 + fw)), ((0.0, -W / 2 - fw, -H / 2 - fw), (0.06, W / 2 + fw, -H / 2)),
                     ((0.0, W / 2, -H / 2), (0.06, W / 2 + fw, H / 2)), ((0.0, -W / 2 - fw, -H / 2), (0.06, -W / 2, H / 2))):
        common.box('rahmen', lo, hi, M['gold'], bevel=0.02, segments=2)
    common.box('grund', (0.02, -W / 2, -H / 2), (0.021, W / 2, H / 2), ground)

    def poly(name, pts, mat, d):
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((0.021 + d, y, z)) for (y, z) in pts])
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()
        common.obj_from_bmesh(name, bm, mat)

    def oval(cy, cz, ry, rz, n=28, squeeze=0.0):
        out = []
        for k in range(n):
            a = 2 * math.pi * k / n
            r = 1.0 - squeeze * max(0.0, -math.sin(a))  # Kinn schmaler
            out.append((cy + math.cos(a) * ry * r, cz + math.sin(a) * rz))
        return out
    poly('schein', oval(0.0, 0.1, 0.27, 0.32), halo, 0.0005)
    sh = [(-0.33, -0.45)]  # Rock mit runden Schultern
    for k in range(9):
        t = k / 8
        sh.append((-0.33 + 0.2 * t, -0.2 + 0.12 * math.sin(math.pi / 2 * t)))
    sh += [(-0.06, -0.06), (0.06, -0.06)]
    for k in range(9):
        t = 1 - k / 8
        sh.append((0.33 - 0.2 * t, -0.2 + 0.12 * math.sin(math.pi / 2 * t)))
    sh.append((0.33, -0.45))
    poly('rock', sh, coat, 0.001)
    poly('hals', [(-0.045, -0.06), (0.045, -0.06), (0.05, 0.03), (-0.05, 0.03)], skin, 0.0011)
    poly('hemd', [(-0.07, -0.05), (0.07, -0.05), (0.0, -0.2)], shirt, 0.0013)
    poly('krawatte', [(-0.018, -0.07), (0.018, -0.07), (0.012, -0.19), (0.0, -0.21), (-0.012, -0.19)], coat, 0.0015)
    poly('gesicht', oval(0.0, 0.12, 0.085, 0.12, squeeze=0.25), skin, 0.0012)
    poly('haar', [(-0.09, 0.13), (-0.088, 0.19), (-0.06, 0.235), (0.0, 0.25), (0.06, 0.235), (0.088, 0.19), (0.09, 0.13),
                  (0.07, 0.17), (0.0, 0.2), (-0.07, 0.17)], hair, 0.0014)
    for sg in (-1, 1):
        poly(f'auge{sg}', oval(sg * 0.032, 0.125, 0.014, 0.006, n=12), coat, 0.0016)
        poly(f'braue{sg}', [(sg * 0.015, 0.142), (sg * 0.05, 0.145), (sg * 0.05, 0.139), (sg * 0.015, 0.137)], hair, 0.0016)
    poly('mund', [(-0.025, 0.06), (0.025, 0.06), (0.025, 0.054), (-0.025, 0.054)], lip, 0.0016)


# --- Runde 2 (QA Level 5): Gesims, Hotelbett, Kessel, gruene Laterne -----------------------------------------------------
def build_cornice():
    """Deckengesims (Wandteil, Wandflaeche x = 0, 1 m entlang y; Oberkante z = 0 an der Decke, haengt 0,26 m herab):
    Stuckprofil mit Hohlkehle, cremefarben, vergoldeter Perlstab."""
    M = _m_old()
    prof = [(0.0, -0.26), (0.03, -0.26), (0.03, -0.235), (0.045, -0.225), (0.05, -0.2)]
    for k in range(9):  # Hohlkehle (konkav)
        a = math.pi / 2 * k / 8
        prof.append((0.06 + 0.12 * (1 - math.cos(a)), -0.19 + 0.13 * math.sin(a)))
    prof += [(0.19, -0.05), (0.2, -0.04), (0.2, 0.0), (0.0, 0.0)]
    ob = common.profile_extrude('gesims', prof, 1.0, M['cream'], axis='y')
    ob.location = (0.0, -0.5, 0.0)
    _cyl('perlstab', (0.052, -0.5, -0.205), (0.052, 0.5, -0.205), 0.009, M['gold'], seg=8)


def build_boiler_tank():
    """Alter Flammrohrkessel (Rueckseite an der Wand x = 0, ragt 2,6 m in den Raum): genieteter Mantel auf
    Ziegelsaetteln, gewoelbter Boden mit Feuertuer (Glut), Manometer, Wasserstandsglas, Dampfdom mit Rohr zur Decke,
    Handrad (Wiki: Kesselraeume, alte Maschinen, Ventile)."""
    M = _m_old()
    shell = common.material('kesselmantel', (0.13, 0.13, 0.12), look='metal', rough=0.8, metal=0.25)
    glow = common.material('glut', (1.0, 0.42, 0.12), look='glow', glow=3.0)
    wheel = common.material('rad_rot', (0.5, 0.08, 0.06), look='paint', rough=0.5)
    zc, R = 1.2, 0.75
    for x0 in (0.35, 1.75):
        common.box(f'sattel{x0}', (x0, -0.62, 0.0), (x0 + 0.45, 0.62, zc - 0.45), M['brick'], bevel=0.01)
    _cyl('mantel', (0.12, 0, zc), (2.35, 0, zc), R, shell, seg=40)
    for x in (0.3, 1.0, 1.7, 2.3):  # Nietnaehte als Baender
        _cyl(f'naht{x}', (x - 0.025, 0, zc), (x + 0.025, 0, zc), R + 0.012, M['iron'], seg=40)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=40, ring_count=20, radius=R, location=(2.35, 0, zc))
    ob = bpy.context.active_object
    for v in ob.data.vertices:  # gewoelbter Boden: Kugel flach gedrueckt (in den Ecken, damit die Normalen stimmen)
        v.co.x *= 0.25
    ob.data.materials.append(shell)
    ob.data.shade_smooth()
    common.box('feuertuer', (2.45, -0.28, 0.72), (2.53, 0.28, 1.08), M['iron'], bevel=0.01)
    common.box('glutschlitz', (2.53, -0.2, 0.8), (2.535, 0.2, 0.85), glow)
    _cyl('riegel', (2.54, -0.22, 0.98), (2.54, 0.22, 0.98), 0.012, M['iron'], seg=8)
    _cyl('mano_rohr', (2.45, 0.3, 1.55), (2.45, 0.3, 1.8), 0.012, M['brass'], seg=8)
    _cyl('mano_gehaeuse', (2.43, 0.3, 1.92), (2.5, 0.3, 1.92), 0.11, M['brass'], seg=24)
    _cyl('mano_blatt', (2.5, 0.3, 1.92), (2.505, 0.3, 1.92), 0.095, M['cream'], seg=24)
    ob = common.box('mano_zeiger', (0.0, -0.004, 0.0), (0.003, 0.004, 0.08), M['black'])
    ob.matrix_world = Matrix.Translation((2.506, 0.3, 1.92)) @ Matrix.Rotation(-0.9, 4, 'X')
    for z in (1.25, 1.65):
        _cyl(f'wsg_arm{z}', (2.42, -0.42, z), (2.55, -0.42, z), 0.02, M['brass'], seg=10)
    _cyl('wsg_glas', (2.55, -0.42, 1.25), (2.55, -0.42, 1.65), 0.018, M['glass'], seg=10)
    _cyl('dom', (1.3, 0, zc + R - 0.05), (1.3, 0, zc + R + 0.35), 0.24, shell, seg=28)
    _cyl('dom_flansch', (1.3, 0, zc + R + 0.35), (1.3, 0, zc + R + 0.41), 0.29, M['iron'], seg=28)
    _cyl('dampfrohr', (1.3, 0, zc + R + 0.41), (1.3, 0, 3.8), 0.09, M['iron'], seg=16)
    _cyl('sv', (0.7, 0, zc + R - 0.05), (0.7, 0, zc + R + 0.3), 0.06, M['brass'], seg=12)
    _cyl('sv_hebel', (0.5, 0, zc + R + 0.32), (0.95, 0, zc + R + 0.32), 0.012, M['iron'], seg=8)
    _cyl('speise', (2.0, 0.78, 0.9), (2.0, 0.95, 0.9), 0.05, M['iron'], seg=12)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.16, minor_radius=0.014, major_segments=24, minor_segments=6,
                                     location=(2.0, 1.0, 0.9), rotation=(math.pi / 2, 0, 0))
    bpy.context.active_object.data.materials.append(wheel)
    for k in range(3):
        a = k * math.pi / 3
        _cyl(f'speiche{k}', (2.0 - math.cos(a) * 0.16, 1.0, 0.9 - math.sin(a) * 0.16),
             (2.0 + math.cos(a) * 0.16, 1.0, 0.9 + math.sin(a) * 0.16), 0.008, M['iron'], seg=6)


def build_lantern_green():
    """Haengelaterne mit gruenem Glas (Deckenteil, Decke bei z = 0): Rosette, Kette, Messinghaube, sechseckiger
    Glaskoerper - im Hotel das Zeichen des Sicherungspunkts (statt eines Notlichtschilds)."""
    M = _m_old()
    glass = common.material('gruenglas', (0.35, 0.95, 0.55), look='glow', glow=2.2)
    _cyl('rosette', (0, 0, -0.025), (0, 0, 0.0), 0.08, M['brass'], seg=20)
    _cyl('kette', (0, 0, -0.45), (0, 0, -0.025), 0.008, M['brass'], seg=6)
    bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.15, radius2=0.03, depth=0.12, location=(0, 0, -0.51))
    bpy.context.active_object.data.materials.append(M['brass'])
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.12, depth=0.34, location=(0, 0, -0.74))
    bpy.context.active_object.data.materials.append(glass)
    for k in range(6):
        a = k * math.pi / 3
        _cyl(f'stab{k}', (math.cos(a) * 0.125, math.sin(a) * 0.125, -0.92), (math.cos(a) * 0.125, math.sin(a) * 0.125, -0.56), 0.01, M['brass'], seg=6)
    _cyl('boden', (0, 0, -0.95), (0, 0, -0.91), 0.14, M['brass'], seg=6)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6, radius=0.03, location=(0, 0, -0.98))
    bpy.context.active_object.data.materials.append(M['brass'])
