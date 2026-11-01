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
    global FONT
    keep = FONT
    if any(ch.isdigit() for ch in body):
        # Inter (variable) hat bei Ziffern ueberlappende Konturen (die 4 fuellte ihr Loch) - Ziffern mit DejaVu Sans
        # Mono aus den Blender-Datendateien (Bitstream-Vera-/DejaVu-Lizenz, LICENSES.md)
        try:
            FONT = bpy.data.fonts.load(os.path.join(os.path.dirname(bpy.app.binary_path), '4.5', 'datafiles', 'fonts',
                                                    'DejaVuSansMono.woff2'), check_existing=True)
        except Exception:  # noqa: BLE001
            FONT = bpy.data.fonts.get('Bfont Regular') or bpy.data.fonts.load('<builtin>')
    try:
        ob = _text(name, body, size, mat, at, depth=depth, res=res)
    finally:
        FONT = keep
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces)  # (Verschmelzen fuellte das Loch der 4 - nur Nullflaechen loeschen)
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
        q = pos + nrm * 0.004 * k
        if not loop or (q - loop[-1]).length > 0.012:  # zu dichte Punkte an den Ecken knicken den Keder
            loop.append(q)
    while len(loop) > 3 and (loop[0] - loop[-1]).length < 0.012:
        loop.pop()
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
        'walnut': common.material('nussbaum', (0.46, 0.30, 0.20), look='wood', tex=WOOD, rough=0.45, r=0.6),
        'walnut_d': common.material('nussbaum_dunkel', (0.28, 0.18, 0.12), look='wood', tex=WOOD, rough=0.5, r=0.7),
        'walnut_l': common.material('nussbaum_hell', (0.56, 0.38, 0.25), look='wood', tex=WOOD, rough=0.45, r=0.55),
        'veneer': common.material('wurzelfurnier', (0.62, 0.40, 0.25), look='wood', tex=WOOD, rough=0.35, r=0.5),
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
        'soot': common.material('russ', (0.03, 0.028, 0.026), look='concrete', rough=1.0, r=1.5, m=0.0),
        'soot_l': common.material('russ_hauch', (0.40, 0.29, 0.23), look='concrete', rough=1.0, r=1.5, m=0.0),
        'soot_m': common.material('russ_mittel', (0.22, 0.17, 0.14), look='concrete', rough=1.0, r=1.5, m=0.0),
        'grease': common.material('fett', (0.05, 0.045, 0.035), look='paint', rough=0.2, r=0.35, m=0.1),
        'brick': common.material('ziegel', (0.80, 0.55, 0.45), look='concrete', tex='bricks_brown_old', rough=0.9, r=1.3),
        'red': common.material('rot', (0.46, 0.08, 0.07), look='fabric', rough=0.8, r=1.2),
    }


WOOD = 'wood_walnut_floor'  # Fotostruktur (Runde r3): die prozedurale Maserung wirkte gezeichnet, mit Aesten


VELVET = {0: (0.42, 0.07, 0.08), 1: (0.60, 0.46, 0.18)}


def _fab(v):
    """Samt (Wiki/LOOK_5: Weinrot oder Gold); abgesessene Stellen heller mit flachem Flor, Keder dunkler."""
    c = VELVET[v]
    return {
        'fab': common.material(f'samt{v}', c, look='fabric', rough=0.9, r=1.25),
        'worn': common.material(f'samt_abgesessen{v}', tuple(min(1.0, x * 1.1 + 0.018) for x in c), look='fabric', rough=0.8, r=1.0),
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
        wrinkle = 0.0012 * math.sin(pos.y * 38.0 + ph + pos.x * 9.0) * k
        front = -0.009 * max(0.0, p[0]) ** 6 * (1 - p[1] ** 2)
        side = 0.007 * (1 - p[2] ** 2) * (1 - abs(nrm.z))
        return top * (crown - sag * k + wrinkle) + front * top + side

    bm = bmesh.new()
    n = (14, 14, 4)
    Sw = g_cushion(bm, ctr, half, (0.045, 0.05, min(0.042, hz * 0.7)), n=n, disp=disp)
    _obj('sitzkissen', bm, F['fab'], smooth=70)
    # abgesessene Stellen: Kuhle oben und Vorderkante (Flor flachgedrueckt, heller)
    bm = bmesh.new()
    g_decal(bm, Sw, 2, 1, sag_at[0] / hx, sag_at[1] / hy, 0.6, 0.65, seed, rough=0.4)
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



# --- Kronleuchter, Laterne -----------------------------------------------------------------------------------------------------
def _chain(bm, z0, z1, link=0.05, r=0.0055, w=0.022):
    """Kette aus ovalen Gliedern, abwechselnd um 90 Grad gedreht (z0 oben, z1 unten)."""
    n = max(2, int((z0 - z1) / (link * 0.8)))
    for k in range(n):
        zc = z0 - (k + 0.5) * (z0 - z1) / n
        pts = []
        for j in range(12):
            a = TAU * j / 12
            pts.append(Vector((w / 2 * math.cos(a), 0.0, zc + link / 2 * math.sin(a))))
        if k % 2:
            pts = [Vector((0.0, p.x, p.z)) for p in pts]
        g_tube(bm, pts, r, seg=5, closed=True)


def _bead(bm, c, r):
    g_lathe(bm, [(0.0, -r), (r * 0.8, 0.0), (0.0, r)], seg=4, M=T(*c) @ R(0.4, 'Z'))


def _chandelier(v):
    """Grosser Kronleuchter (Decke z = 0, haengt 1,6 m herab; Beverly Room und Aufzugshalle): Baldachin, Kette,
    gedrechselter Schaft mit Vase und Schale, zehn geschwungene Arme mit Zierschnecken und Ring, sechs obere Arme,
    Kerzen mit Tropftellern und Flammenbirnen, Kristall: Perlgirlanden zwischen den Armen, Behangtropfen, Prismen
    an der Schale, Kugel unten."""
    M = _m()
    flame = common.material('flamme', (1.0, 0.82, 0.55), look='glow', glow=3.2)
    B = _B(['brass', 'brass_d', 'ivory', 'crystal'])
    bf = bmesh.new()
    g_lathe(B['brass'], [(0.0, -0.11), (0.02, -0.105), (0.03, -0.09), (0.08, -0.075), (0.12, -0.05), (0.14, -0.025), (0.14, 0.0), (0.0, 0.0)],
            seg=24)
    _chain(B['brass_d'], -0.10, -0.56)
    stem = [(0.0, -1.62), (0.02, -1.60), (0.034, -1.565), (0.03, -1.53), (0.06, -1.50), (0.12, -1.45), (0.152, -1.39), (0.145, -1.33),
            (0.10, -1.285), (0.05, -1.255), (0.04, -1.21), (0.075, -1.18), (0.075, -1.155), (0.036, -1.125), (0.03, -1.0), (0.05, -0.965),
            (0.088, -0.91), (0.10, -0.86), (0.085, -0.81), (0.035, -0.77), (0.03, -0.67), (0.052, -0.64), (0.052, -0.61), (0.022, -0.585),
            (0.022, -0.555), (0.0, -0.555)]
    g_lathe(B['brass'], stem, seg=20)
    na = 10 if v == 0 else 8
    tips = []
    for k in range(na):
        a = TAU * k / na
        Rz = R(a, 'Z')
        p = _bez((0.07, 0.0, -1.168), (0.36, 0.0, -1.33), (0.64, 0.0, -1.27), (0.70, 0.0, -1.115), 12)
        p = [Rz @ q for q in p]
        g_tube(B['brass'], p, 0.0095, seg=7, scales=[1.3 - 0.45 * j / 12 for j in range(13)])
        sp = [Rz @ Vector((0.42 + 0.05 * math.cos(t) * (1 - t / 10), 0.0, -1.30 + 0.05 * math.sin(t) * (1 - t / 10))) for t in [j * 0.55 for j in range(14)]]
        g_tube(B['brass'], sp, 0.0045, seg=5)
        tips.append(Rz @ Vector((0.70, 0.0, -1.115)))
    g_tube(B['brass_d'], [Vector((0.45 * math.cos(TAU * j / 30), 0.45 * math.sin(TAU * j / 30), -1.305)) for j in range(30)], 0.006, seg=6, closed=True)
    for k, t in enumerate(tips):
        _candle(B['ivory'], bf, B['brass'], t.x, t.y, t.z, 0.11, 30 + k)
    utips = []
    for k in range(6 if v == 0 else 0):
        a = TAU * (k + 0.5) / 6
        Rz = R(a, 'Z')
        p = [Rz @ q for q in _bez((0.06, 0.0, -0.93), (0.22, 0.0, -1.0), (0.38, 0.0, -0.95), (0.40, 0.0, -0.80), 10)]
        g_tube(B['brass'], p, 0.0085, seg=7, scales=[1.25 - 0.4 * j / 10 for j in range(11)])
        utips.append(Rz @ Vector((0.40, 0.0, -0.80)))
    for k, t in enumerate(utips):
        _candle(B['ivory'], bf, B['brass'], t.x, t.y, t.z, 0.09, 60 + k)
    # Kristall: Girlanden, Behang, Prismen, Kugel
    for k in range(na):
        a0, a1 = TAU * k / na, TAU * (k + 1) / na
        for j in range(1, 8):
            t = j / 8
            a = a0 + (a1 - a0) * t
            rr = 0.67
            _bead(B['crystal'], (rr * math.cos(a), rr * math.sin(a), -1.14 - 0.11 * math.sin(math.pi * t)), 0.0085)
        tp = tips[k]
        for j in range(2):
            _bead(B['crystal'], (tp.x * 0.985, tp.y * 0.985, tp.z - 0.035 - 0.02 * j), 0.007)
        g_lathe(B['crystal'], [(0.0, -0.055), (0.012, -0.035), (0.016, -0.018), (0.01, -0.004), (0.0, 0.0)], seg=6, M=T(tp.x * 0.985, tp.y * 0.985, tp.z - 0.08))
    for k in range(12):
        a = TAU * k / 12
        c = Vector((0.15 * math.cos(a), 0.15 * math.sin(a), -1.40))
        g_lathe(B['crystal'], [(0.0, -0.09), (0.008, -0.08), (0.009, -0.01), (0.0, 0.0)], seg=6, M=T(c.x, c.y, c.z))
    for k in range(len(utips)):
        t = utips[k]
        g_lathe(B['crystal'], [(0.0, -0.045), (0.01, -0.03), (0.013, -0.015), (0.008, -0.003), (0.0, 0.0)], seg=6, M=T(t.x * 0.98, t.y * 0.98, t.z - 0.03))
    g_lathe(B['crystal'], [(0.0, -0.06), (0.025, -0.052), (0.035, -0.03), (0.025, -0.008), (0.0, 0.0)], seg=10, M=T(0, 0, -1.565))
    if v == 1:  # Korb aus Prismenkraenzen unter den Armen
        for (rr, zz, n_) in ((0.56, -1.22, 28), (0.42, -1.33, 22), (0.28, -1.43, 16)):
            g_tube(B['brass_d'], [Vector((rr * math.cos(TAU * j / 24), rr * math.sin(TAU * j / 24), zz)) for j in range(24)], 0.004, seg=5, closed=True)
            for j in range(n_):
                a = TAU * (j + 0.5) / n_
                g_lathe(B['crystal'], [(0.0, -0.075), (0.007, -0.066), (0.008, -0.01), (0.0, 0.0)], seg=4, M=T(rr * math.cos(a), rr * math.sin(a), zz - 0.004) @ R(a, 'Z'))
    for k, b_ in B.items():
        _obj(k, b_, M[k], smooth=45, recalc=True)
    _obj('flammen', bf, flame, smooth=60, recalc=True)


def build_chandelier_grand():
    _chandelier(0)


def build_chandelier_grand__v1():
    _chandelier(1)


def build_lantern_green():
    """Haengelaterne mit gruenem Glas (Decke z = 0): Rosette, Kette, sechseckige Haube mit Rippen, Lueftungskranz und
    Knauf, Rahmen mit sechs Stegen, gruene Scheiben (leuchten), Gluehbirne, Bodenschale mit Zapfen - im Hotel das
    Zeichen des Sicherungspunkts (statt eines Notlichtschilds)."""
    M = _m()
    glass = common.material('gruenglas', (0.35, 0.95, 0.55), look='glow', glow=2.2)
    bulb = common.material('birne_gruen', (0.8, 1.0, 0.85), look='glow', glow=3.0)
    B = _B(['brass', 'brass_d'])
    g_lathe(B['brass'], [(0.0, -0.03), (0.02, -0.028), (0.05, -0.02), (0.08, -0.01), (0.08, 0.0), (0.0, 0.0)], seg=16)
    _chain(B['brass_d'], -0.03, -0.40, link=0.045, r=0.0045, w=0.02)
    g_lathe(B['brass'], [(0.0, -0.455), (0.012, -0.45), (0.012, -0.43), (0.02, -0.418), (0.012, -0.405), (0.0, -0.40)], seg=10)
    g_lathe(B['brass'], [(0.0, -0.545), (0.152, -0.545), (0.152, -0.53), (0.06, -0.47), (0.03, -0.455), (0.0, -0.455)], seg=6)
    for k in range(6):
        a = TAU * k / 6
        g_tube(B['brass_d'], [(0.03 * math.cos(a), 0.03 * math.sin(a), -0.457), (0.15 * math.cos(a), 0.15 * math.sin(a), -0.533)], 0.005, seg=5)
    g_lathe(B['brass_d'], [(0.10, -0.50), (0.112, -0.50), (0.112, -0.49), (0.10, -0.49)], seg=6)
    for zz in (-0.555, -0.89):
        g_lathe(B['brass'], [(0.105, zz - 0.012), (0.142, zz - 0.012), (0.142, zz + 0.012), (0.105, zz + 0.012)], seg=6)
    for k in range(6):
        a = TAU * k / 6
        c = Vector((0.13 * math.cos(a), 0.13 * math.sin(a), 0.0))
        g_box(B['brass'], (-0.008, -0.008, -0.885), (0.008, 0.008, -0.56), M=T(c.x, c.y, 0) @ R(a, 'Z'), ch=0.002)
    g_lathe(B['brass'], [(0.0, -0.95), (0.03, -0.945), (0.09, -0.925), (0.145, -0.905), (0.145, -0.895), (0.0, -0.895)], seg=6)
    g_lathe(B['brass'], [(0.0, -1.01), (0.008, -1.0), (0.02, -0.98), (0.016, -0.962), (0.01, -0.95), (0.0, -0.948)], seg=10)
    g_lathe(B['brass_d'], [(0.0, -0.66), (0.02, -0.66), (0.02, -0.60), (0.0, -0.585)], seg=10)
    _flush(B, M)
    bm = bmesh.new()
    for k in range(6):
        a0, a1 = TAU * k / 6, TAU * (k + 1) / 6
        r_ = 0.122
        p0 = (r_ * math.cos(a0), r_ * math.sin(a0))
        p1 = (r_ * math.cos(a1), r_ * math.sin(a1))
        g_quad(bm, (p0[0], p0[1], -0.878), (p1[0], p1[1], -0.878), (p1[0], p1[1], -0.567), (p0[0], p0[1], -0.567))
    _obj('scheiben', bm, glass, smooth=None)
    bm = bmesh.new()
    g_lathe(bm, [(0.0, -0.76), (0.022, -0.745), (0.032, -0.715), (0.028, -0.685), (0.014, -0.665), (0.0, -0.66)], seg=10)
    _obj('birne', bm, bulb, smooth=60)


# --- Aufzug mit Kupfergitter ---------------------------------------------------------------------------------------------------
def build_elevator_cage():
    """Alter Aufzug im Durchbruch (Seite -x zur Halle; die Karte streckt das Modell in y um 1,35 - Rundes ist in y
    vorab gestaucht): kannelierte Kupferpilaster mit Kapitell und Sockel, Sturz mit Rosettenfries und Gesims,
    halbrunde Stockwerksanzeige (Skala, Zeiger, Rand), geschlossenes Scherengitter (U-Staebe, Kreuzlaschen mit
    Nieten, Laufschienen, Griff), Messingschwelle, Rufknoepfe; Gruenspan in den Kehlen."""
    M = _m()
    B = _B(['copper', 'verdigris', 'brass', 'brass_d', 'black', 'cream', 'ivory'])
    sy = 1.0 / 1.35
    W2, H = 0.70, 2.25
    xf = -0.545
    for s in (-1, 1):
        y0, y1 = sorted((s * (W2 - 0.08), s * W2))
        g_box(B['copper'], (xf, y0, 0.10), (xf + 0.09, y1, H - 0.08), ch=0.004)
        g_box(B['copper'], (xf - 0.012, y0 - 0.008, 0.0), (xf + 0.09, y1 + 0.008, 0.10), ch=0.006)
        g_box(B['copper'], (xf - 0.012, y0 - 0.008, H - 0.12), (xf + 0.09, y1 + 0.008, H - 0.08), ch=0.006)
        for j in range(3):
            yy = y0 + 0.02 + j * 0.02
            g_box(B['verdigris'], (xf - 0.0006, yy - 0.0035, 0.16), (xf + 0.001, yy + 0.0035, H - 0.16))
    g_box(B['copper'], (xf - 0.01, -W2, H - 0.08), (xf + 0.09, W2, H + 0.06), ch=0.005)
    g_mold_u(B['copper'], xf + 0.09, xf - 0.01, W2, H + 0.06, [(-0.02, 0.0), (0.004, 0.0), (0.012, 0.012), (0.014, 0.022), (0.0, 0.026), (-0.02, 0.026)])
    for k in range(5):
        yy = -0.48 + k * 0.24
        g_lathe(B['brass'], [(0.0, 0.0), (0.028, 0.0), (0.026, 0.006), (0.014, 0.01), (0.0, 0.012)], seg=12, M=T(xf - 0.01, yy, H - 0.01) @ R(-math.pi / 2, 'Y') @ S(1.0, sy, 1.0))
    # Anzeige: Halbkreis mit Skala und Zeiger (Blick -x)
    zc, rr = H + 0.08, 0.22
    arc = [(0.0, zc)] + [(-math.cos(math.pi * k / 16) * rr * sy, zc + math.sin(math.pi * k / 16) * rr) for k in range(17)]
    del arc
    g_plate(B['brass_d'], [(math.cos(math.pi * k / 16) * rr * sy, zc + math.sin(math.pi * k / 16) * rr) for k in range(17)], xf - 0.012, xf)
    face = [(math.cos(math.pi * k / 16) * (rr - 0.025) * sy, zc + 0.012 + math.sin(math.pi * k / 16) * (rr - 0.03)) for k in range(17)]
    g_plate(B['cream'], face, xf - 0.0135, xf - 0.012)
    g_tube(B['brass'], [(xf - 0.014, math.cos(math.pi * k / 24) * rr * sy, zc + math.sin(math.pi * k / 24) * rr) for k in range(25)], 0.007, seg=6)
    for k in range(11):
        a = math.pi * (0.1 + 0.8 * k / 10)
        d = Vector((0.0, math.cos(a) * sy, math.sin(a)))
        side = Vector((0.0, -math.sin(a) * sy, math.cos(a))) * 0.003
        c0 = Vector((xf - 0.0137, 0.0, zc + 0.012)) + d * (rr - 0.07)
        c1 = Vector((xf - 0.0137, 0.0, zc + 0.012)) + d * (rr - 0.045 - (0.0 if k % 5 else 0.01))
        g_quad(B['black'], c0 + side, c1 + side, c1 - side, c0 - side)
    a = math.pi * 0.62
    d = Vector((0.0, math.cos(a) * sy, math.sin(a)))
    side = Vector((0.0, -math.sin(a) * sy, math.cos(a)))
    c = Vector((xf - 0.016, 0.0, zc + 0.012))
    g_prism(B['black'], [(0.0, -0.005), (0.15, -0.0015), (0.17, 0.0), (0.15, 0.0015), (0.0, 0.005)], -0.002, 0.0,
            M=Matrix(((0, 0, 1, c.x), (d.y, side.y, 0, c.y), (d.z, side.z, 0, c.z), (0, 0, 0, 1))))
    g_lathe(B['brass'], [(0.0, 0.0), (0.014, 0.0), (0.012, 0.006), (0.0, 0.008)], seg=10, M=T(xf - 0.016, 0.0, zc + 0.012) @ R(-math.pi / 2, 'Y') @ S(1.0, sy, 1.0))
    # Scherengitter (geschlossen)
    xg = -0.44
    n = 11
    ys = [-0.60 + 1.2 * k / (n - 1) for k in range(n)]
    for y in ys:
        g_box(B['brass_d'], (xg - 0.008, y - 0.007, 0.03), (xg + 0.008, y + 0.007, H - 0.12), ch=0.0015)
    for tier in range(3):
        z0 = 0.06 + tier * 0.68
        z1 = z0 + 0.66
        for k in range(n - 1):
            for (za, zb) in ((z0, z1), (z1, z0)):
                pa, pb = Vector((xg - 0.012, ys[k], za)), Vector((xg - 0.012, ys[k + 1], zb))
                dd = (pb - pa).normalized()
                nn = Vector((1, 0, 0)).cross(dd).normalized() * 0.008
                off = Vector((-0.003 if za < zb else 0.003, 0, 0))
                g_quad(B['brass'], pa - nn + off, pb - nn + off, pb + nn + off, pa + nn + off)
                g_quad(B['brass'], pa + nn + off + Vector((0.003, 0, 0)), pb + nn + off + Vector((0.003, 0, 0)), pb - nn + off + Vector((0.003, 0, 0)),
                       pa - nn + off + Vector((0.003, 0, 0)))
            g_domes(B['brass'], [(((ys[k] + ys[k + 1]) / 2 * 0 + xg - 0.018, (ys[k] + ys[k + 1]) / 2, (z0 + z1) / 2), (-1, 0, 0))], 0.006, 0.004, seg=6)
    for z in (0.02, H - 0.13):
        g_box(B['copper'], (xg - 0.02, -W2 + 0.08, z), (xg + 0.02, W2 - 0.08, z + 0.02), ch=0.003)
    g_tube(B['brass'], [(xg - 0.03, 0.55, 0.9), (xg - 0.045, 0.55, 0.92), (xg - 0.045, 0.55, 1.18), (xg - 0.03, 0.55, 1.2)], 0.007, seg=6)
    g_box(B['brass'], (xf - 0.015, -W2, 0.0), (xg + 0.05, W2, 0.012), ch=0.002)
    for k in range(6):
        g_box(B['brass_d'], (xf - 0.012 + k * 0.022, -W2 + 0.02, 0.0121), (xf - 0.004 + k * 0.022, W2 - 0.02, 0.0124))
    # Rufknoepfe am rechten Pilaster
    g_box(B['brass'], (xf - 0.006, W2 - 0.075, 1.08), (xf, W2 - 0.015, 1.30), ch=0.002)
    for z in (1.15, 1.23):
        g_lathe(B['ivory'], [(0.0, 0.0), (0.011, 0.0), (0.011, 0.004), (0.008, 0.008), (0.0, 0.009)], seg=10,
                M=T(xf - 0.006, W2 - 0.045, z) @ R(-math.pi / 2, 'Y') @ S(1.0, sy, 1.0))
    for k, b_ in B.items():
        if len(b_.faces):
            _obj(k, b_, M[k], smooth=40, recalc=k in ('copper', 'cream', 'ivory'))
        else:
            b_.free()


# --- Metro-Wagen (Halt nach Level 903) -----------------------------------------------------------------------------------------
def build_metro_car_end():
    """Metro-Wagen in der Nische (Boden z = 0, Wagen entlang x, Seitentuer bei y = 0 offen): Edelstahlkasten mit
    Sicken, blauer Zierstreifen, Fenster mit Gummirahmen und gerundeten Ecken, gewoelbtes Dach mit Luefterhauben und
    Regenrinne, Stirnwaende mit Uebergangstuer, Griffstangen und Nummer, offene Schiebetuer mit Gummikanten und
    Trittblech; innen Laengsbaenke mit Polstern, Haltestangen, Handlaeufe mit Halteschlaufen, Lichtbaender,
    Werberahmen, Rillengummiboden."""
    M = _m()
    body = common.material('wagen', (0.62, 0.66, 0.68), look='metal', rough=0.45, metal=0.7, r=0.75, m=0.6)
    stripe = common.material('streifen', (0.18, 0.30, 0.52), look='paint', rough=0.5, r=0.85)
    inner = common.material('innenwand', (0.82, 0.80, 0.72), look='paint', rough=0.6, r=1.0)
    floor_m = common.material('gummiboden', (0.16, 0.17, 0.18), look='rubber', rough=0.8, r=1.2)
    seat = common.material('sitz', (0.26, 0.30, 0.42), look='fabric', rough=0.9, r=1.25)
    rubber = common.material('gummi', (0.04, 0.04, 0.04), look='rubber', rough=0.8, r=1.1)
    light = common.material('innenlicht', (1.0, 0.96, 0.84), look='glow', glow=1.2)
    ad = [common.material(f'werbung{k}', c, look='paint', rough=0.5, r=0.8) for k, c in enumerate(((0.70, 0.55, 0.20), (0.25, 0.45, 0.55), (0.62, 0.22, 0.18)))]
    L, W, H = 6.0, 2.6, 2.7
    Bm = {'body': bmesh.new(), 'stripe': bmesh.new(), 'inner': bmesh.new(), 'floor': bmesh.new(), 'seat': bmesh.new(), 'rubber': bmesh.new(),
          'light': bmesh.new(), 'glass': bmesh.new(), 'steel': bmesh.new(), 'ad0': bmesh.new(), 'ad1': bmesh.new(), 'ad2': bmesh.new()}
    mats = {'body': body, 'stripe': stripe, 'inner': inner, 'floor': floor_m, 'seat': seat, 'rubber': rubber, 'light': light, 'glass': common.material('fensterlicht', (0.80, 0.78, 0.68), look='glow', glow=0.15),
            'steel': M['silver'], 'ad0': ad[0], 'ad1': ad[1], 'ad2': ad[2]}
    zf, ze = 0.08, 2.32
    g_box(Bm['body'], (-L / 2, 0.0, 0.0), (L / 2, W, zf), ch=0.01)
    g_box(Bm['floor'], (-L / 2 + 0.06, 0.06, zf), (L / 2 - 0.06, W - 0.06, zf + 0.004))
    for k in range(40):
        x = -L / 2 + 0.1 + k * 0.146
        g_box(Bm['floor'], (x, 0.08, zf + 0.004), (x + 0.012, W - 0.08, zf + 0.007))
    # Seitenwaende (y = 0 mit Tuer, y = W geschlossen): Brüstung mit Sicken, Streifen, Fensterband, Oberteil
    win_x = []
    for side in (0, 1):
        yo = 0.0 if side == 0 else W
        yi = 0.06 if side == 0 else W - 0.06
        y0, y1 = min(yo, yi), max(yo, yi)
        spans = [(-L / 2, -0.75), (0.75, L / 2)] if side == 0 else [(-L / 2, L / 2)]
        for (xa, xb) in spans:
            g_box(Bm['body'], (xa, y0, zf), (xb, y1, 0.95))
            g_box(Bm['stripe'], (xa, y0 - (0.003 if side == 0 else -0.0), 0.95), (xb, y1 + (0.003 if side == 1 else 0.0), 1.05))
            g_box(Bm['body'], (xa, y0, 1.95), (xb, y1, ze))
            for k in range(5):  # Sicken
                z = 0.2 + k * 0.14
                yy = yo - 0.004 if side == 0 else yo
                g_box(Bm['body'], (xa + 0.02, min(yy, yy + 0.004), z), (xb - 0.02, max(yy, yy + 0.004), z + 0.03), ch=0.0015)
            # Fenster: Pfeiler und Glas mit Gummirahmen
            nwin = max(1, int((xb - xa - 0.2) / 1.0))
            wpitch = (xb - xa) / nwin
            for j in range(nwin):
                wa, wb = xa + j * wpitch + 0.12, xa + (j + 1) * wpitch - 0.12
                g_box(Bm['body'], (xa + j * wpitch, y0, 1.05), (wa, y1, 1.95))
                g_box(Bm['body'], (wb, y0, 1.05), (xa + (j + 1) * wpitch, y1, 1.95))
                g_quad(Bm['glass'], (wa, (y0 + y1) / 2, 1.05), (wb, (y0 + y1) / 2, 1.05), (wb, (y0 + y1) / 2, 1.95), (wa, (y0 + y1) / 2, 1.95))
                g_quad(Bm['glass'], (wb, (y0 + y1) / 2, 1.05), (wa, (y0 + y1) / 2, 1.05), (wa, (y0 + y1) / 2, 1.95), (wb, (y0 + y1) / 2, 1.95))
                rr = _rrect((wa + wb) / 2, 1.5, (wb - wa) / 2, 0.45, 0.07, 3)
                ypl = yo - 0.002 if side == 0 else yo + 0.002
                g_sweep(Bm['rubber'], [(-0.012, 0.0), (-0.006, 0.006), (0.006, 0.006), (0.012, 0.0)], [(x, ypl, z) for (x, z) in rr],
                        closed=True, up=(0, -1 if side == 0 else 1, 0), pclosed=False)
                win_x.append(((wa + wb) / 2, side))
            # Innenverkleidung
            g_box(Bm['inner'], (xa, min(yi, yi + (0.01 if side == 0 else -0.01)), zf), (xb, max(yi, yi + (0.01 if side == 0 else -0.01)), 1.0))
    g_box(Bm['body'], (-0.75, 0.0, 2.05), (0.75, 0.06, ze))
    # Tuerrahmen und offene Schiebetuer (Fluegel in der Tasche, Gummikanten sichtbar), Trittblech
    for s in (-1, 1):
        g_box(Bm['steel'], (s * 0.75 - 0.03, -0.01, zf), (s * 0.75 + 0.03, 0.07, 2.05), ch=0.006)
        g_box(Bm['rubber'], (s * 0.70 - 0.015, 0.012, zf + 0.02), (s * 0.70 + 0.015, 0.05, 2.0), ch=0.006)
        g_box(Bm['body'], (s * 0.715 if s > 0 else -0.86, 0.015, zf + 0.02), (0.86 if s > 0 else -0.715, 0.045, 2.0))
    g_box(Bm['steel'], (-0.75, -0.02, 2.05), (0.75, 0.07, 2.10), ch=0.006)
    g_box(Bm['steel'], (-0.72, -0.03, zf - 0.004), (0.72, 0.10, zf + 0.004), ch=0.002)
    for k in range(10):
        g_box(Bm['rubber'], (-0.68 + k * 0.15, -0.02, zf + 0.004), (-0.62 + k * 0.15, 0.09, zf + 0.006))
    # Dach: Bogen ueber die Breite (Schale), Regenrinne, Luefterhauben
    roof = []
    for k in range(13):
        t = k / 12
        roof.append((W * t, ze + 0.32 * math.sin(math.pi * t)))
    prof = [(y, z) for (y, z) in roof] + [(y, z - 0.05) for (y, z) in reversed(roof)]
    g_sweep(Bm['body'], [(-(y - W / 2), z) for (y, z) in prof], [(-L / 2, W / 2, 0.0), (L / 2, W / 2, 0.0)], up=(0, 0, 1))
    for s in (0, 1):
        yy = -0.02 if s == 0 else W + 0.02
        g_box(Bm['body'], (-L / 2, min(yy, yy + (0.03 if s == 0 else -0.03)), ze - 0.01), (L / 2, max(yy, yy + (0.03 if s == 0 else -0.03)), ze + 0.03), ch=0.006)
    for x in (-1.8, 0.0, 1.8):
        g_box(Bm['body'], (x - 0.4, W / 2 - 0.3, ze + 0.30), (x + 0.4, W / 2 + 0.3, ze + 0.38), ch=0.03)
        for j in range(6):
            g_box(Bm['rubber'], (x - 0.36 + j * 0.13, W / 2 - 0.31, ze + 0.31), (x - 0.30 + j * 0.13, W / 2 - 0.30, ze + 0.36))
    g_box(Bm['inner'], (-L / 2 + 0.06, 0.06, ze - 0.02), (L / 2 - 0.06, W - 0.06, ze))
    # Stirnwaende mit Uebergangstuer, Fenster, Griffstangen, Nummer
    for s in (-1, 1):
        x0 = s * L / 2
        xa, xb = sorted((x0, x0 - s * 0.06))
        for (ya, yb, za, zb) in ((0.0, 0.85, 0.0, ze), (1.75, W, 0.0, ze), (0.85, 1.75, 2.05, ze)):
            g_box(Bm['body'], (xa, ya, za), (xb, yb, zb))
        g_box(Bm['body'], (min(x0, x0 + s * 0.03), 0.85, zf), (max(x0, x0 + s * 0.03), 1.75, 2.05), ch=0.008)
        g_quad(Bm['glass'], (x0 + s * 0.031, 1.0 if s > 0 else 1.6, 1.25), (x0 + s * 0.031, 1.6 if s > 0 else 1.0, 1.25),
               (x0 + s * 0.031, 1.6 if s > 0 else 1.0, 1.85), (x0 + s * 0.031, 1.0 if s > 0 else 1.6, 1.85))
        g_tube(Bm['steel'], [(x0 + s * 0.05, 0.75, 0.9), (x0 + s * 0.09, 0.75, 0.92), (x0 + s * 0.09, 0.75, 1.68), (x0 + s * 0.05, 0.75, 1.7)], 0.014, seg=8)
        g_tube(Bm['steel'], [(x0 + s * 0.05, 1.85, 0.9), (x0 + s * 0.09, 1.85, 0.92), (x0 + s * 0.09, 1.85, 1.68), (x0 + s * 0.05, 1.85, 1.7)], 0.014, seg=8)
        g_box(Bm['stripe'], (min(x0, x0 + s * 0.004), 0.0, 0.95), (max(x0, x0 + s * 0.004), W, 1.05))
    # Innenraum: Laengsbaenke, Haltestangen, Handlaeufe mit Schlaufen, Lichtbaender, Werberahmen
    for (xa, xb, side) in ((-2.9, 2.9, 1), (-2.9, -0.85, 0), (0.85, 2.9, 0)):
        y_wall = W - 0.06 if side else 0.06
        sgn = -1 if side else 1
        nseg = max(1, int(round((xb - xa) / 1.4)))
        for j in range(nseg):
            a, b = xa + (xb - xa) * j / nseg + 0.01, xa + (xb - xa) * (j + 1) / nseg - 0.01
            g_cushion(Bm['seat'], ((a + b) / 2, y_wall + sgn * 0.27, 0.46), ((b - a) / 2, 0.22, 0.05), (0.02, 0.04, 0.03), n=(6, 3, 2))
            g_cushion(Bm['seat'], ((a + b) / 2, y_wall + sgn * 0.06, 0.78), ((b - a) / 2, 0.04, 0.22), (0.02, 0.03, 0.04), n=(6, 2, 4),
                      M=T((a + b) / 2, y_wall, 0.56) @ R(-sgn * 0.12, 'X') @ T(-(a + b) / 2, -y_wall, -0.56))
        g_box(Bm['steel'], (xa, min(y_wall, y_wall + sgn * 0.48), 0.36), (xb, max(y_wall, y_wall + sgn * 0.48), 0.41), ch=0.01)
        for x in (xa + 0.05, xb - 0.05):
            g_box(Bm['steel'], (x - 0.02, min(y_wall, y_wall + sgn * 0.45), zf), (x + 0.02, max(y_wall, y_wall + sgn * 0.45), 0.36))
        g_tube(Bm['steel'], [(xa, y_wall + sgn * 0.3, 1.92), (xb, y_wall + sgn * 0.3, 1.92)], 0.016, seg=8)
        for k in range(int((xb - xa) / 0.5)):
            x = xa + 0.25 + k * 0.5
            g_tube(Bm['rubber'], [Vector((x, y_wall + sgn * 0.3, 1.92)), Vector((x, y_wall + sgn * 0.3, 1.80))], 0.008, seg=5)
            g_tube(Bm['steel'], [Vector((x + 0.045 * math.cos(TAU * j / 10), y_wall + sgn * 0.3, 1.76 + 0.045 * math.sin(TAU * j / 10))) for j in range(10)],
                   0.007, seg=5, closed=True)
        for k in range(int((xb - xa) / 1.0)):
            x = xa + 0.2 + k * 1.0
            g_box(Bm['steel'], (x, min(y_wall, y_wall + sgn * 0.004), 2.0), (x + 0.6, max(y_wall, y_wall + sgn * 0.004), 2.25), ch=0.004)
            g_box(Bm[f'ad{k % 3}'], (x + 0.02, min(y_wall + sgn * 0.004, y_wall + sgn * 0.006), 2.02), (x + 0.58, max(y_wall + sgn * 0.004, y_wall + sgn * 0.006), 2.23))
    for x in (-0.75, 0.75):
        g_tube(Bm['steel'], [(x, 0.32, zf), (x, 0.32, ze)], 0.018, seg=10)
    g_tube(Bm['steel'], [(0.0, W / 2, zf), (0.0, W / 2, ze)], 0.018, seg=10)
    for y in (0.75, W - 0.75):
        g_box(Bm['light'], (-L / 2 + 0.3, y - 0.08, ze - 0.03), (L / 2 - 0.3, y + 0.08, ze - 0.02))
        g_box(Bm['steel'], (-L / 2 + 0.28, y - 0.1, ze - 0.02), (L / 2 - 0.28, y + 0.1, ze - 0.005))
    for k, b_ in Bm.items():
        if len(b_.faces):
            _obj(f'wagen_{k}', b_, mats[k], smooth=35, recalc=k not in ('glass', 'light', 'rubber'))
        else:
            b_.free()
    _text_clean('nummer1', '903', 0.12, M['black'], Vector((L / 2 + 0.032, 1.3, 2.15)), depth=0.002, res=3)
    for s, x in ((-1, -2.2), (1, 2.2)):
        ob = _text_clean(f'nummer_s{s}', '903', 0.10, M['black'], Vector((x, -0.004, 2.12)), depth=0.002, res=3)
        ob.matrix_world = Matrix.Translation((x, -0.004, 2.12)) @ Matrix.Rotation(-math.pi / 2, 4, 'Z') @ Matrix.Translation((-x, 0.004, -2.12)) @ ob.matrix_world



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
    wood = common.material(f'schrank{v}', tuple(c * tone for c in (0.46, 0.30, 0.20)), look='wood', tex=WOOD, rough=0.45, r=0.6)
    wood_d = common.material(f'schrank_dunkel{v}', tuple(c * tone for c in (0.28, 0.18, 0.12)), look='wood', tex=WOOD, rough=0.5, r=0.7)
    wood_l = common.material(f'schrank_hell{v}', tuple(min(1.0, c * tone * 1.15) for c in (0.52, 0.35, 0.23)), look='wood', tex=WOOD, rough=0.4, r=0.55)
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
    gs = [[v.co.copy() for v in row] for row in gs]
    _obj('tagesdecke', bm, spread, smooth=60)
    ob = bpy.context.scene.objects['tagesdecke']
    sol = ob.modifiers.new('dicke', 'SOLIDIFY')
    sol.thickness, sol.offset = 0.008, -1.0
    bm = bmesh.new()  # Borte am Saum (folgt der Decke)
    for row_side in (0, -1):
        for i in range(len(gs) - 1):
            a0, a1 = gs[i][row_side], gs[i + 1][row_side]
            b0, b1 = gs[i][row_side + (1 if row_side == 0 else -1)], gs[i + 1][row_side + (1 if row_side == 0 else -1)]
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



# --- Kesselraum: Normteile (Ventilrad, Flansch, Rohr, Flecken auf Zylindern) ------------------------------------------------
def _basis(axis):
    w = Vector(axis).normalized()
    t = Vector((0, 0, 1)) if abs(w.z) < 0.9 else Vector((1, 0, 0))
    u = w.cross(t).normalized()
    v = w.cross(u).normalized()
    return u, v, w


def _cyl_patch(bm, c0, axis, R_, a0, a1, s0, s1, seed, lift=0.0008, n=6, jag=0.25, taper=0.0):
    """Fleck auf einem Zylinder (Achse axis durch c0, Radius R_): Winkel a0..a1, Laenge s0..s1, ausgefranster Rand."""
    rng = random.Random(seed)
    u, v, w = _basis(axis)
    c0 = Vector(c0)
    grid = []
    for i in range(n + 1):
        row = []
        for j in range(n + 1):
            ti, tj = i / n, j / n
            edge = min(ti, 1 - ti, tj, 1 - tj)
            sh = 1.0 if edge > 0.01 else 1.0 - jag * rng.random() * 1.6 / n  # hoechstens eine Zelle nach innen
            a = a0 + (a1 - a0) * (0.5 + (ti - 0.5) * sh)
            s = s0 + (s1 - s0) * (0.5 + (tj - 0.5) * sh * (1.0 - taper * ti))
            row.append(bm.verts.new(c0 + w * s + (u * math.cos(a) + v * math.sin(a)) * (R_ + lift)))
        grid.append(row)
    fs = []
    for i in range(n):
        for j in range(n):
            fs.append(bm.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1])))
    fm = fs[len(fs) // 2]  # Umlaufsinn einmal fuer den ganzen Fleck festlegen
    fm.normal_update()
    cc = fm.calc_center_median()
    radial = (cc - c0) - w * (cc - c0).dot(w)
    if fm.normal.dot(radial) < 0:
        for f in fs:
            f.normal_flip()


def _flat_patch(bm, c, nrm, r, seed, n=9, lift=0.0006, stretch=(1.0, 1.0)):
    """Unregelmaessiger ebener Fleck (Russ, Abplatzer, Fett) auf einer ebenen Flaeche."""
    rng = random.Random(seed)
    u, v, w = _basis(nrm)
    c = Vector(c) + w * lift
    vc = bm.verts.new(c)
    ring = []
    for k in range(n):
        a = TAU * k / n
        rr = r * rng.uniform(0.82, 1.12)
        ring.append(bm.verts.new(c + u * math.cos(a) * rr * stretch[0] + v * math.sin(a) * rr * stretch[1]))
    for k in range(n):
        f = bm.faces.new((vc, ring[k], ring[(k + 1) % n]))
        f.normal_update()
        if f.normal.dot(w) < 0:
            f.normal_flip()


def _flange(bm_f, bm_b, c, axis, r_pipe, nb=6, seed=0):
    """Flanschpaar mit Dichtung und Schrauben (Rohr-Massstab)."""
    u, v, w = _basis(axis)
    M = _aim(Vector(c), w)
    R_ = r_pipe * 1.75 + 0.02
    g_lathe(bm_f, [(r_pipe * 0.98, -0.022), (R_, -0.022), (R_, -0.004), (r_pipe * 1.2, -0.002), (r_pipe * 1.2, 0.002), (R_, 0.004),
                   (R_, 0.022), (r_pipe * 0.98, 0.022)], seg=16, M=M)
    rb = (R_ + r_pipe) / 2 + 0.004
    for k in range(nb):
        a = TAU * (k + 0.5) / nb
        p = Vector(c) + (u * math.cos(a) + v * math.sin(a)) * rb
        g_tube(bm_b, [p - w * 0.034, p + w * 0.034], 0.005, seg=6)
        g_hex(bm_b, p + w * 0.022, w, 0.016, 0.009, washer=False, rot=seed + k)
        g_hex(bm_b, p - w * 0.022, -w, 0.016, 0.009, washer=False, rot=seed - k)


def _valve_wheel(B, c, axis, R_, seed, spokes=5, wheel_mat='wheel'):
    """Handrad nach dem Ventilrad-Massstab: Sockel (Oberteil mit Stopfbuchse und Schrauben), Spindel mit Gewinde,
    Nabe mit Mutter, geschwungene Speichen, Kranz mit Dicke und Griffmulden, leichte Unrundheit; Lackabplatzer,
    Rost am Sockel, Fett an der Spindel. B: dict Material -> bmesh."""
    rng = random.Random(seed)
    u, v, w = _basis(axis)
    c = Vector(c)
    M = _aim(c, w)
    # Oberteil (Sockel) und Stopfbuchse
    g_lathe(B['iron'], [(0.0, 0.0), (0.045, 0.0), (0.047, 0.012), (0.040, 0.016), (0.032, 0.024), (0.030, 0.07), (0.024, 0.075),
                        (0.024, 0.085), (0.0, 0.085)], seg=14, M=M)
    g_box(B['iron'], (-0.006, -0.05, 0.06), (0.006, 0.05, 0.072), M=M, ch=0.002)  # Stopfbuchsbruecke
    for s in (-1, 1):
        g_tube(B['steel'], [c + w * 0.02 + v * (s * 0.042), c + w * 0.082 + v * (s * 0.042)], 0.0045, seg=6)
        g_hex(B['steel'], c + w * 0.072 + v * (s * 0.042), w, 0.014, 0.008, washer=False, rot=s)
    # Spindel mit Gewinde (Rillen), Fett in den Gaengen
    thr = [(0.0, 0.085)]
    for k in range(10):
        z = 0.085 + k * 0.0055
        thr += [(0.0082, z + 0.0005), (0.0098, z + 0.0028)]
    thr += [(0.0082, 0.143), (0.0, 0.143)]
    g_lathe(B['steel'], thr, seg=8, M=M)
    for k in range(3):
        a = rng.uniform(0, TAU)
        p = c + w * rng.uniform(0.09, 0.13) + (u * math.cos(a) + v * math.sin(a)) * 0.0095
        g_domes(B['grease'], [(p, (u * math.cos(a) + v * math.sin(a)))], 0.006, 0.002, seg=5)
    # Nabe mit Mutter
    zh = 0.15
    g_lathe(B[wheel_mat], [(0.0, zh - 0.018), (0.024, zh - 0.018), (0.026, zh - 0.008), (0.024, zh + 0.008), (0.016, zh + 0.012),
                           (0.0, zh + 0.012)], seg=12, M=M)
    g_hex(B['steel'], c + w * (zh + 0.012), w, 0.02, 0.012, washer=True, rot=seed)
    # Kranz (ovaler Querschnitt, leicht unrund) mit Griffmulden
    dish = -0.025
    pts = []
    n = 28
    wob = [1.0 + 0.012 * math.sin(3 * TAU * k / n + seed) + 0.006 * rng.uniform(-1, 1) for k in range(n)]
    for k in range(n):
        a = TAU * k / n
        pts.append(c + w * (zh + dish) + (u * math.cos(a) + v * math.sin(a)) * R_ * wob[k])
    prof = _cw([(0.011 * math.cos(TAU * k / 8), 0.0085 * math.sin(TAU * k / 8)) for k in range(8)])
    g_sweep(B[wheel_mat], prof, pts, closed=True, up=tuple(w))
    for k in range(0, n, 4):  # Griffbuckel
        a = TAU * (k + 2) / n
        p = c + w * (zh + dish) + (u * math.cos(a) + v * math.sin(a)) * (R_ * wob[(k + 2) % n] + 0.009)
        g_domes(B[wheel_mat], [(p - (u * math.cos(a) + v * math.sin(a)) * 0.004, u * math.cos(a) + v * math.sin(a))], 0.008, 0.005, seg=6)
    # Speichen (geschwungen, zur Nabe hin dicker)
    for k in range(spokes):
        a = TAU * k / spokes + seed * 0.3
        dr = u * math.cos(a) + v * math.sin(a)
        side = u * math.cos(a + 1.2) + v * math.sin(a + 1.2)
        p0 = c + w * zh + dr * 0.02
        p3 = c + w * (zh + dish) + dr * (R_ - 0.004)
        sp = _bez(p0, p0 + dr * 0.04 + side * 0.012, p3 - dr * 0.05 + side * 0.016 + w * 0.006, p3, 6)
        g_tube(B[wheel_mat], sp, 0.0075, seg=6, scales=[1.25, 1.1, 1.0, 0.92, 0.88, 0.9, 1.0])
    # Lackabplatzer auf dem Kranz (blankes Metall) und Rost am Sockel
    for k in range(5):
        a0 = rng.uniform(0, TAU)
        b0 = rng.uniform(-0.6, 1.4)
        cc = c + w * (zh + dish)
        for i in range(2):
            for j in range(2):
                pass
        pa = []
        for i in range(4):
            for j in range(3):
                aa = a0 + 0.12 * i / 3 * rng.uniform(0.7, 1.2)
                bb = b0 + 0.9 * j / 2
                pa.append(cc + (u * math.cos(aa) + v * math.sin(aa)) * (R_ + 0.0115 * math.cos(bb)) + w * (0.009 * math.sin(bb)))
        vs = [B['chip'].verts.new(p) for p in pa]
        fs = [B['chip'].faces.new((vs[i * 3 + j], vs[(i + 1) * 3 + j], vs[(i + 1) * 3 + j + 1], vs[i * 3 + j + 1])) for i in range(3) for j in range(2)]
        fm = fs[2]
        fm.normal_update()
        ctr = fm.calc_center_median()
        rad = (ctr - cc) - w * (ctr - cc).dot(w)
        tube_c = cc + rad.normalized() * R_
        if fm.normal.dot(ctr - tube_c) < 0:
            for f in fs:
                f.normal_flip()
    for k in range(3):
        a0 = rng.uniform(0, TAU)
        _cyl_patch(B['rust'], c, w, 0.0305, a0, a0 + rng.uniform(0.6, 1.4), 0.026, 0.026 + rng.uniform(0.02, 0.04), seed + k, n=3)


def _B(names):
    return {k: bmesh.new() for k in names}


def _flush(B, M, extra=None, smooth=40):
    mats = dict(M)
    if extra:
        mats.update(extra)
    for k, b_ in B.items():
        if len(b_.faces):
            _obj(k, b_, mats[k], smooth=smooth)
        else:
            b_.free()


def _mats_machine():
    return {
        'wheel': common.material('rad_rot', (0.48, 0.07, 0.05), look='paint', rough=0.6, r=0.95),
        'chip': common.material('abplatzer', (0.42, 0.42, 0.40), look='metal', rough=0.4, metal=0.8, r=0.7, m=0.75),
        'shell': common.material('kesselmantel', (0.15, 0.17, 0.15), look='paint', rough=0.8, r=1.15, m=0.1),
        'lag': common.material('daemmung', (0.50, 0.46, 0.39), look='fabric', rough=0.9, r=1.4),
        'band': common.material('band', (0.36, 0.36, 0.34), look='galvanized', rough=0.6, r=1.0, m=0.55),
        'dial': common.material('skala', (0.88, 0.85, 0.76), look='paint', rough=0.5, r=0.8),
        'water': common.material('wasser', (0.55, 0.62, 0.58), look='water', rough=0.05),
        'glow': common.material('glut', (1.0, 0.42, 0.12), look='glow', glow=3.0),
        'ash': common.material('asche', (0.30, 0.29, 0.27), look='concrete', rough=1.0, r=1.5),
        'coal': common.material('kohle', (0.04, 0.04, 0.045), look='ceramic', rough=0.4, r=0.6),
        'redline': common.material('rote_marke', (0.6, 0.06, 0.05), look='paint', rough=0.5, r=0.8),
    }


def _gauge(B, c, nrm, r, seed):
    """Manometer: Messinggehaeuse mit Ring, Skala mit Strichen, rote Marke, Zeiger, Glas."""
    u, v, w = _basis(nrm)
    M = _aim(Vector(c), w)
    g_lathe(B['brass'], [(0.0, 0.0), (r, 0.0), (r * 1.04, 0.01), (r * 1.06, 0.03), (r * 0.98, 0.036), (r * 0.9, 0.033), (r * 0.9, 0.03),
                         (0.0, 0.03)], seg=24, M=M)
    g_lathe(B['dial'], [(0.0, 0.0305), (r * 0.9, 0.0305), (r * 0.9, 0.031), (0.0, 0.031)], seg=24, M=M)
    for k in range(13):
        a = math.radians(225 - 270 * k / 12)
        d = u * math.cos(a) + v * math.sin(a)
        p0 = Vector(c) + w * 0.0312 + d * r * 0.68
        p1 = Vector(c) + w * 0.0312 + d * r * (0.82 if k % 3 else 0.76)
        side = w.cross(d).normalized() * 0.0018
        q = [p0 - side, p1 - side, p1 + side, p0 + side]
        f = B['black'].faces.new([B['black'].verts.new(p) for p in q])
        f.normal_update()
        if f.normal.dot(w) < 0:
            f.normal_flip()
    a = math.radians(225 - 270 * 0.78)
    d = u * math.cos(a) + v * math.sin(a)
    side = w.cross(d).normalized()
    for (t0, t1, mat) in ((0.68, 0.84, 'redline'),):
        q = [Vector(c) + w * 0.0313 + d * r * t0 - side * 0.004, Vector(c) + w * 0.0313 + d * r * t1 - side * 0.006,
             Vector(c) + w * 0.0313 + d * r * t1 + side * 0.006, Vector(c) + w * 0.0313 + d * r * t0 + side * 0.004]
        f = B[mat].faces.new([B[mat].verts.new(p) for p in q])
        f.normal_update()
        if f.normal.dot(w) < 0:
            f.normal_flip()
    a = math.radians(225 - 270 * (0.55 + 0.1 * random.Random(seed).random()))
    d = u * math.cos(a) + v * math.sin(a)
    side = w.cross(d).normalized()
    q = [Vector(c) + w * 0.0316 - d * r * 0.12 - side * 0.002, Vector(c) + w * 0.0316 + d * r * 0.78 - side * 0.0006,
         Vector(c) + w * 0.0316 + d * r * 0.78 + side * 0.0006, Vector(c) + w * 0.0316 - d * r * 0.12 + side * 0.002]
    f = B['black'].faces.new([B['black'].verts.new(p) for p in q])
    f.normal_update()
    if f.normal.dot(w) < 0:
        f.normal_flip()
    g_lathe(B['glass'], [(0.0, 0.034), (r * 0.92, 0.034), (r * 0.92, 0.0345), (0.0, 0.0352)], seg=20, M=M)


def build_boiler_tank():
    """Flammrohrkessel (Rueckseite an der Wand x = 0, 2,6 m in den Raum, Kesselraum - Wiki: alte Maschinen, Ventile):
    drei Schuesse mit Ueberlappung und Nietnaehten, gewoelbter Stirnboden mit Feuer- und Aschentuer (Glut im Spalt,
    Luftschieber, Scharniere, Riegel), Mannloch, Dampfdom mit Absperrventil und Handrad, gedaemmte Dampfleitung mit
    Flanschen bis zur Decke, Hebelsicherheitsventil mit Gewicht, Manometer am Siphon, Wasserstandsglas mit Haehnen
    und Schutzstaeben, Speiseleitung mit Handrad in den Boden, Abschlammventil, gemauerte Saettel, Typenschild;
    Russ um die Feuertuer, Rostlaeufer unter Naehten und Ventilen, Abplatzer, Fett."""
    M = _m()
    MM = _mats_machine()
    B = _B(['shell', 'iron', 'steel', 'brass', 'rust', 'rust_l', 'soot', 'soot_l', 'grease', 'wheel', 'chip', 'lag', 'band', 'dial', 'black',
            'redline', 'glass', 'water', 'glow', 'brick'])
    zc, Rr = 1.2, 0.75
    ax = Vector((1, 0, 0))
    # Saettel
    for x0 in (0.35, 1.75):
        g_box(B['brick'], (x0, -0.62, 0.0), (x0 + 0.45, 0.62, zc - 0.55), ch=0.01)
        g_box(B['iron'], (x0 - 0.01, -0.64, zc - 0.57), (x0 + 0.46, 0.64, zc - 0.55))
    # Mantel: drei Schuesse, abwechselnd innen/aussen
    xs = [0.12, 0.86, 1.62, 2.36]
    for k in range(3):
        r_ = Rr + (0.008 if k % 2 else 0.0)
        g_lathe(B['shell'], [(r_, xs[k] - (0.03 if k % 2 else 0.0)), (r_, xs[k + 1] + (0.03 if k % 2 else 0.0))], seg=40,
                M=T(0, 0, zc) @ R(math.pi / 2, 'Y') @ R(0, 'Z'))
    # Rueckwand und Stirnboden (gewoelbt)
    g_lathe(B['shell'], [(Rr + 0.004, -0.04), (Rr + 0.004, 0.0), (Rr * 0.85, 0.03), (Rr * 0.5, 0.05), (0.0, 0.06)], seg=40,
            M=T(0.12, 0, zc) @ R(-math.pi / 2, 'Y'))
    front = [(Rr + 0.004, 0.0), (Rr + 0.004, 0.05), (Rr * 0.92, 0.10), (Rr * 0.7, 0.15), (Rr * 0.4, 0.18), (0.0, 0.19)]
    g_lathe(B['shell'], front, seg=40, M=T(2.33, 0, zc) @ R(math.pi / 2, 'Y'))
    # Nietnaehte (rund) und Laengsnaht oben
    riv = []
    for x in (0.88, 1.60):
        riv += [(p, n) for (p, n) in g_rivet_ring((x, 0, zc), ax, Rr + 0.0085, 56)]
    riv += g_rivet_ring((0.15, 0, zc), ax, Rr + 0.002, 48) + g_rivet_ring((2.38, 0, zc), ax, Rr + 0.002, 48)
    for k in range(3):
        for j in range(9):
            x = xs[k] + 0.06 + (xs[k + 1] - xs[k] - 0.12) * j / 8
            for da in (-0.035, 0.035):
                a = math.pi / 2 + da + (0.5 if k == 1 else 0.0)
                riv.append(((x, math.cos(a) * (Rr + 0.004), zc + math.sin(a) * (Rr + 0.004)), (0, math.cos(a), math.sin(a))))
    g_domes(B['shell'], riv, 0.0085, 0.006, seg=5)
    # Laengslasche
    for k in range(3):
        a = math.pi / 2 + (0.5 if k == 1 else 0.0)
        _cyl_patch(B['shell'], (0, 0, zc), ax, Rr + 0.003, a - 0.06, a + 0.06, xs[k] + 0.03, xs[k + 1] - 0.03, 77 + k, lift=0.0, n=2, jag=0.0)
    # Feuertuer mit Rahmen, Luftschieber, Scharnieren, Riegel; Glutspalt; Aschentuer
    xf = 2.47
    g_box(B['iron'], (xf - 0.02, -0.34, 0.66), (xf + 0.02, 0.34, 1.14), ch=0.006)
    g_box(B['glow'], (xf + 0.02, -0.26, 0.73), (xf + 0.0205, 0.26, 1.07))
    g_box(B['iron'], (xf + 0.02, -0.25, 0.74), (xf + 0.06, 0.25, 1.06), ch=0.008)
    for k in range(5):
        g_box(B['black'], (xf + 0.06, -0.12 + k * 0.05, 0.98), (xf + 0.0605, -0.10 + k * 0.05, 1.03))
    g_box(B['steel'], (xf + 0.061, -0.14, 0.975), (xf + 0.065, 0.06, 1.035), ch=0.002)
    for z in (0.80, 1.00):
        g_lathe(B['iron'], [(0.0, 0.0), (0.016, 0.0), (0.016, 0.07), (0.0, 0.07)], seg=10, M=T(xf + 0.04, 0.27, z - 0.035))
        g_box(B['iron'], (xf + 0.05, 0.12, z - 0.02), (xf + 0.062, 0.27, z + 0.02))
    g_tube(B['steel'], [(xf + 0.07, -0.22, 0.86), (xf + 0.10, -0.28, 0.86), (xf + 0.10, -0.36, 0.86)], 0.009, seg=6)
    g_box(B['iron'], (xf - 0.03, -0.30, 0.30), (xf + 0.03, 0.30, 0.55), ch=0.006)
    g_box(B['glow'], (xf + 0.03, -0.22, 0.34), (xf + 0.0305, 0.22, 0.36))
    g_box(B['brick'], (2.18, -0.52, 0.0), (2.44, 0.52, 0.62), ch=0.01)   # Aschenfall (Mauerwerk)
    g_box(B['iron'], (2.17, -0.54, 0.62), (2.445, 0.54, 0.64))
    for k in range(9):  # Russ ueber der Feuertuer (auf dem gewoelbten Boden)
        y, z = rng_u(300 + k, -0.32, 0.32), rng_u(320 + k, 1.15, 1.42)
        p, n_ = _head_pt(front, 2.33, zc, y, z)
        _flat_patch(B['soot'], p, n_, 0.06 + 0.012 * (k % 4), 340 + k, lift=0.002)
    # Mannloch oben
    g_lathe(B['iron'], [(0.0, 0.0), (0.21, 0.0), (0.21, 0.035), (0.19, 0.045), (0.0, 0.05)], seg=24,
            M=T(1.95, 0, zc + Rr - 0.01) @ S(1.0, 0.72, 1.0))
    for k in range(10):
        a = TAU * k / 10
        g_hex(B['steel'], (1.95 + math.cos(a) * 0.17, math.sin(a) * 0.12, zc + Rr + 0.042), (0, 0, 1), 0.022, 0.014, rot=k)
    # Dampfdom mit Flansch, Absperrventil (Handrad), gedaemmte Leitung bis zur Decke
    xd = 1.3
    g_lathe(B['shell'], [(0.25, zc + Rr - 0.06), (0.25, zc + Rr + 0.33), (0.30, zc + Rr + 0.33), (0.30, zc + Rr + 0.40), (0.0, zc + Rr + 0.40)],
            seg=28, M=T(xd, 0, 0))
    for k in range(12):
        a = TAU * k / 12
        g_hex(B['steel'], (xd + math.cos(a) * 0.275, math.sin(a) * 0.275, zc + Rr + 0.40), (0, 0, 1), 0.02, 0.012, rot=k)
    zv = zc + Rr + 0.40
    g_lathe(B['iron'], [(0.0, zv), (0.13, zv), (0.13, zv + 0.03), (0.09, zv + 0.05), (0.11, zv + 0.12), (0.10, zv + 0.2), (0.07, zv + 0.24),
                        (0.07, zv + 0.28), (0.13, zv + 0.28), (0.13, zv + 0.31), (0.0, zv + 0.31)], seg=18, M=T(xd, 0, 0))
    _valve_wheel(B, (xd + 0.10, 0.0, zv + 0.16), (1, 0, 0), 0.13, 3)
    zp = zv + 0.31
    g_lathe(B['iron'], [(0.07, zp), (0.07, zp + 0.06)], seg=16, M=T(xd, 0, 0))
    g_lathe(B['lag'], [(0.0, zp + 0.06), (0.13, zp + 0.06), (0.13, 3.72), (0.0, 3.72)], seg=18, M=T(xd, 0, 0))
    for z in [zp + 0.2 + k * 0.32 for k in range(int((3.6 - zp) / 0.32))]:
        g_lathe(B['band'], [(0.132, z), (0.134, z + 0.003), (0.134, z + 0.022), (0.132, z + 0.025)], seg=18, M=T(xd, 0, 0))
    g_lathe(B['iron'], [(0.0, 3.72), (0.16, 3.72), (0.16, 3.8), (0.0, 3.8)], seg=18, M=T(xd, 0, 0))
    for k in range(4):  # Russ und Wasserraender auf der Daemmung
        _cyl_patch(B['soot_l'], (xd, 0, 0), (0, 0, 1), 0.1315, rng_u(450 + k, 0, TAU), rng_u(450 + k, 0, TAU) + 0.8, 2.6 + 0.25 * k, 2.75 + 0.25 * k, 460 + k, n=4, jag=0.5)
    for k in range(3):
        _cyl_patch(B['rust'], (xd, 0, 0), (0, 0, 1), 0.2505, 0.4 + k * 2.1, 0.9 + k * 2.1, zc + Rr - 0.04, zc + Rr + 0.3, 400 + k, n=4)
    # Hebelsicherheitsventil mit Gewicht
    xs_ = 0.75
    g_lathe(B['brass'], [(0.0, zc + Rr - 0.02), (0.07, zc + Rr - 0.02), (0.07, zc + Rr + 0.03), (0.045, zc + Rr + 0.05), (0.045, zc + Rr + 0.20),
                         (0.06, zc + Rr + 0.22), (0.0, zc + Rr + 0.24)], seg=14, M=T(xs_, 0, 0))
    g_box(B['iron'], (xs_ - 0.18, -0.012, zc + Rr + 0.23), (xs_ + 0.42, 0.012, zc + Rr + 0.26), ch=0.003)
    g_lathe(B['iron'], [(0.0, 0.0), (0.07, 0.0), (0.075, 0.03), (0.07, 0.12), (0.0, 0.125)], seg=14, M=T(xs_ + 0.38, 0, zc + Rr + 0.10))
    g_tube(B['steel'], [(xs_ + 0.38, 0, zc + Rr + 0.225), (xs_ + 0.38, 0, zc + Rr + 0.255)], 0.008, seg=6)
    # Manometer am Siphon und Wasserstandsglas am Stirnboden
    xg = 2.42
    sip = [Vector((xg - 0.05, 0.32, 1.62))]
    for k in range(17):
        a = TAU * k / 16
        sip.append(Vector((xg + 0.03 + 0.035 * math.cos(a), 0.32 + 0.035 * math.sin(a), 1.68 + 0.004 * k)))
    sip.append(Vector((xg + 0.065, 0.32, 1.78)))
    g_tube(B['brass'], sip, 0.006, seg=6)
    g_lathe(B['brass'], [(0.0, 0.0), (0.016, 0.0), (0.016, 0.04), (0.0, 0.04)], seg=8, M=T(xg + 0.065, 0.32, 1.76))
    _gauge(B, (xg + 0.065, 0.32, 1.92), (1, 0, 0), 0.12, 5)
    for z in (1.22, 1.66):
        g_tube(B['brass'], [(xg - 0.06, -0.40, z), (xg + 0.08, -0.40, z)], 0.02, seg=10)
        g_lathe(B['brass'], [(0.0, 0.0), (0.03, 0.0), (0.03, 0.06), (0.0, 0.06)], seg=10, M=T(xg + 0.11, -0.40, z - 0.03))
        g_box(B['iron'], (xg + 0.10, -0.40, z - 0.006), (xg + 0.20, -0.388, z + 0.006), ch=0.002)
    g_lathe(B['glass'], [(0.0, 1.24), (0.016, 1.24), (0.016, 1.64), (0.0, 1.64)], seg=10, M=T(xg + 0.11, -0.40, 0))
    g_lathe(B['water'], [(0.0, 1.245), (0.0135, 1.245), (0.0135, 1.47), (0.0, 1.47)], seg=10, M=T(xg + 0.11, -0.40, 0))
    for k in range(4):
        a = TAU * k / 4 + 0.4
        g_tube(B['steel'], [(xg + 0.11 + math.cos(a) * 0.028, -0.40 + math.sin(a) * 0.028, 1.25), (xg + 0.11 + math.cos(a) * 0.028, -0.40 + math.sin(a) * 0.028, 1.63)], 0.003, seg=5)
    g_tube(B['brass'], [(xg + 0.11, -0.40, 1.19), (xg + 0.11, -0.40, 1.10), (xg + 0.16, -0.40, 1.06)], 0.008, seg=6)
    for k in range(4):  # Rostlaeufer unter dem Wasserstand
        p, n_ = _head_pt(front, 2.33, zc, -0.40 + rng_u(k + 20, -0.05, 0.05), 1.12 - 0.07 * k)
        _flat_patch(B['rust_l'], p, n_, 0.025, 500 + k, lift=0.002, stretch=(0.5, 2.2))
    # Speiseleitung (seitlich +y) mit Rueckschlag- und Absperrventil, in den Boden; Abschlammventil
    pr = 0.045
    path = [Vector((2.0, Rr - 0.05, 0.95)), Vector((2.0, 0.93, 0.95)), Vector((2.0, 0.98, 0.90)), Vector((2.0, 0.98, 0.0))]
    g_tube(B['steel'], path, pr, seg=12, caps=False)
    _flange(B['iron'], B['steel'], (2.0, 0.80, 0.95), (0, 1, 0), pr, 4, 1)
    _flange(B['iron'], B['steel'], (2.0, 0.98, 0.55), (0, 0, 1), pr, 4, 2)
    g_lathe(B['iron'], [(0.0, 0.62), (0.07, 0.62), (0.09, 0.66), (0.09, 0.78), (0.07, 0.82), (0.0, 0.82)], seg=14, M=T(2.0, 0.98, 0))
    _valve_wheel(B, (2.0, 0.98, 0.72), (0, 1, 0), 0.10, 7, spokes=4)
    g_lathe(B['iron'], [(0.0, 0.0), (0.11, 0.0), (0.11, 0.02), (0.06, 0.03), (0.0, 0.03)], seg=14, M=T(2.0, 0.98, 0.0))
    g_tube(B['steel'], [(1.4, -0.40, 0.60), (1.4, -0.40, 0.2), (1.4, -0.70, 0.2)], 0.03, seg=10, caps=False)
    g_lathe(B['iron'], [(0.0, 0.0), (0.045, 0.0), (0.05, 0.03), (0.045, 0.09), (0.0, 0.09)], seg=12, M=T(1.4, -0.70, 0.2) @ R(math.pi / 2, 'X'))
    _valve_wheel(B, (1.4, -0.745, 0.24), (0, 0, 1), 0.07, 9, spokes=4)
    for k in range(7):  # Rostlaeufer unter den Naehten (schmal, nach unten auslaufend)
        xs0 = (0.88 if k % 2 else 1.60) + rng_u(590 + k, -0.02, 0.02)
        a0 = rng_u(600 + k, -0.3, 0.6)
        a1 = a0 + rng_u(610 + k, 0.25, 0.6)
        if k % 3 == 1:  # andere Seite (+y): nach unten heisst zu kleineren Winkeln
            a0, a1 = math.pi - a0, math.pi - a1
        _cyl_patch(B['rust_l'], (0, 0, zc), ax, Rr + 0.0105, a0, a1, xs0 - 0.02, xs0 + 0.02, 620 + k, n=5, jag=0.6, taper=0.8)
    for k in range(6):  # Abplatzer am Mantel
        a = rng_u(700 + k, -0.4, 2.6)
        x = rng_u(710 + k, 0.3, 2.2)
        _cyl_patch(B['chip'], (0, 0, zc), ax, Rr + 0.0095, a, a + 0.06, x, x + 0.08, 720 + k, n=3)
    # Typenschild
    g_box(B['brass'], (2.43, -0.15, 1.36), (2.436, 0.15, 1.46), ch=0.002)
    _flush(B, M, MM)
    _text_clean('typenschild', 'KESSEL 3  12 ATU  1928', 0.016, M['black'], Vector((2.437, 0.0, 1.41)), depth=0.0005, res=2)


def _head_pt(prof, x0, zc, y, z):
    """Punkt und Normale auf einem gewoelbten Stirnboden (Drehprofil (r, dx) um die x-Achse bei x0)."""
    r = math.hypot(y, z - zc)
    for i in range(len(prof) - 1):
        (r1, x1), (r2, x2) = prof[i], prof[i + 1]
        if min(r1, r2) <= r <= max(r1, r2) and abs(r1 - r2) > 1e-6:
            t = (r - r1) / (r2 - r1)
            dxdr = (x2 - x1) / (r2 - r1)
            rd = Vector((0.0, y, z - zc)) / max(r, 1e-6)
            n_ = (Vector((1.0, 0.0, 0.0)) - rd * dxdr).normalized()
            return Vector((x0 + x1 + t * (x2 - x1), y, z)), n_
    return Vector((x0 + prof[-1][1], y, z)), Vector((1.0, 0.0, 0.0))


def rng_u(seed, a, b):
    return random.Random(seed).uniform(a, b)


def build_furnace():
    """Gemauerter Ofen (Rueckseite an der Wand x = 0, 1,6 x 2,0 x 2,2 m): Ziegelkoerper mit Eisenankern an den Ecken
    und Zugstangen, gusseiserne Deckplatte, Feuertuer mit Rahmen, Scharnieren, Riegel und Schauloch (Glut im Spalt),
    Aschentuer, genietetes Rauchrohr aus drei Schuessen mit Flanschen und Drosselklappe bis zur Decke; Russ ueber
    der Tuer und auf der Platte, Ascheberg mit Kohlestuecken, angelehnte Kohlenschaufel."""
    M = _m()
    MM = _mats_machine()
    B = _B(['brick', 'iron', 'steel', 'rust', 'soot', 'soot_l', 'soot_m', 'glow', 'black', 'ash', 'coal', 'walnut_d', 'grease'])
    D, HW, Hh = 1.6, 1.0, 2.2
    g_box(B['brick'], (0.0, -HW, 0.0), (D, HW, Hh), ch=0.012)
    g_box(B['iron'], (-0.02, -HW - 0.03, Hh), (D + 0.03, HW + 0.03, Hh + 0.08), ch=0.008)
    for (x, y) in ((D, -HW), (D, HW), (0.02, -HW), (0.02, HW)):  # Winkeleisen an den Ecken
        sx = 1 if x > 0.5 else -1
        sy = 1 if y > 0 else -1
        g_box(B['iron'], (x - 0.004 if sx > 0 else x - 0.03, y - 0.06 if sy > 0 else y - 0.004, 0.0),
              (x + 0.008 if sx > 0 else x + 0.03, y + 0.004 if sy > 0 else y + 0.06, Hh))
        g_box(B['iron'], (x - 0.06 if sx > 0 else x - 0.004, y - 0.004 if sy > 0 else y - 0.008, 0.0),
              (x + 0.004 if sx > 0 else x + 0.06, y + 0.008 if sy > 0 else y + 0.004, Hh))
    for z in (0.35, 1.85):
        g_tube(B['steel'], [(D + 0.01, -HW - 0.05, z), (D + 0.01, HW + 0.05, z)], 0.012, seg=8)
        g_tube(B['steel'], [(0.05, -HW - 0.05, z), (D + 0.05, -HW - 0.05, z)], 0.012, seg=8)
        g_tube(B['steel'], [(0.05, HW + 0.05, z), (D + 0.05, HW + 0.05, z)], 0.012, seg=8)
        for (p, n) in (((D + 0.01, -HW - 0.06, z), (0, -1, 0)), ((D + 0.01, HW + 0.06, z), (0, 1, 0)), ((D + 0.06, -HW - 0.05, z), (1, 0, 0)),
                       ((D + 0.06, HW + 0.05, z), (1, 0, 0))):
            g_hex(B['steel'], p, n, 0.03, 0.02, rot=z)
    # Feuertuer
    g_box(B['iron'], (D, -0.48, 0.42), (D + 0.04, 0.48, 1.20), ch=0.008)
    g_box(B['glow'], (D + 0.04, -0.40, 0.50), (D + 0.0405, 0.40, 1.12))
    g_box(B['iron'], (D + 0.04, -0.39, 0.51), (D + 0.09, 0.39, 1.11), ch=0.01)
    riv = []
    for k in range(9):
        for z in (0.53, 1.09):
            riv.append(((D + 0.09, -0.36 + k * 0.09, z), (1, 0, 0)))
    for k in range(6):
        for y in (-0.37, 0.37):
            riv.append(((D + 0.09, y, 0.6 + k * 0.09), (1, 0, 0)))
    g_domes(B['iron'], riv, 0.009, 0.006, seg=6)
    for z in (0.62, 1.0):
        g_lathe(B['iron'], [(0.0, 0.0), (0.02, 0.0), (0.02, 0.09), (0.0, 0.09)], seg=10, M=T(D + 0.065, 0.42, z - 0.045))
        g_box(B['iron'], (D + 0.085, 0.24, z - 0.025), (D + 0.1, 0.42, z + 0.025))
    g_lathe(B['iron'], [(0.0, 0.0), (0.06, 0.0), (0.06, 0.012), (0.0, 0.015)], seg=14, M=T(D + 0.09, 0.0, 0.86) @ R(math.pi / 2, 'Y'))
    g_tube(B['steel'], [(D + 0.10, -0.30, 0.80), (D + 0.15, -0.36, 0.80), (D + 0.15, -0.46, 0.80), (D + 0.12, -0.50, 0.80)], 0.012, seg=6)
    g_box(B['iron'], (D, -0.36, 0.08), (D + 0.05, 0.36, 0.34), ch=0.006)
    g_box(B['glow'], (D + 0.05, -0.30, 0.12), (D + 0.0505, 0.30, 0.135))
    g_box(B['iron'], (D - 0.01, -0.55, 1.22), (D + 0.03, 0.55, 1.30), ch=0.004)   # Sturzeisen
    # Russ ueber der Tuer, auf der Platte
    for k in range(9):  # Russfahne ueber der Tuer: drei Toene, aussen ein Hauch, innen dunkel
        _flat_patch(B['soot_l'], (D + 0.012, rng_u(800 + k, -0.4, 0.4), 1.34 + 0.09 * k), (1, 0, 0), 0.16 + 0.01 * k, 810 + k, n=14)
    for k in range(6):
        _flat_patch(B['soot_m'], (D + 0.0128, rng_u(850 + k, -0.3, 0.3), 1.33 + 0.07 * k), (1, 0, 0), 0.09 + 0.008 * k, 860 + k, n=12)
    for k in range(4):
        _flat_patch(B['soot'], (D + 0.0136, rng_u(870 + k, -0.2, 0.2), 1.32 + 0.04 * k), (1, 0, 0), 0.045, 880 + k, n=10)
    for k in range(5):
        _flat_patch(B['soot_l'], (rng_u(820 + k, 0.3, 1.4), rng_u(830 + k, -0.8, 0.8), Hh + 0.081), (0, 0, 1), 0.15, 840 + k)
    # Rauchrohr: drei genietete Schuesse mit Flanschen, Drosselklappe, Deckenkragen
    xr, rr = 0.6, 0.2
    zs = [Hh + 0.08, 2.75, 3.3, 3.8]
    for k in range(3):
        g_lathe(B['iron'], [(rr, zs[k]), (rr, zs[k + 1])], seg=20, M=T(xr, 0, 0))
        if k:
            _flange(B['iron'], B['steel'], (xr, 0, zs[k]), (0, 0, 1), rr, 8, k)
        for zz in (zs[k] + 0.05, zs[k + 1] - 0.05):
            g_domes(B['iron'], g_rivet_ring((xr, 0, zz), (0, 0, 1), rr + 0.001, 14), 0.008, 0.005, seg=5)
        for j in range(2):
            a0 = rng_u(900 + 3 * k + j, 0, TAU)
            _cyl_patch(B['rust'], (xr, 0, 0), (0, 0, 1), rr + 0.0015, a0, a0 + rng_u(910 + k, 0.5, 1.2), zs[k] + 0.1, zs[k + 1] - 0.1, 920 + k * 3 + j, n=4)
    g_lathe(B['iron'], [(0.0, 3.72), (0.32, 3.72), (0.32, 3.8), (0.0, 3.8)], seg=20, M=T(xr, 0, 0))
    g_tube(B['steel'], [(xr, -rr - 0.06, 2.55), (xr, rr + 0.06, 2.55)], 0.01, seg=6)
    g_box(B['iron'], (xr - 0.012, rr + 0.06, 2.53), (xr + 0.012, rr + 0.09, 2.70), ch=0.003)
    g_lathe(B['iron'], [(0.0, 0.0), (0.02, 0.0), (0.02, 0.05), (0.0, 0.05)], seg=8, M=T(xr, rr + 0.075, 2.70))
    # Ascheberg mit Kohle, Kohlenschaufel
    rng = random.Random(77)
    bm = B['ash']
    nr, ns = 6, 14
    rings = [[bm.verts.new((D + 0.12 + 0.3, 0.0, 0.11))]]
    for i in range(1, nr + 1):
        t = i / nr
        ring = []
        for k in range(ns):
            a = TAU * k / ns
            rr_ = t * rng.uniform(0.85, 1.1)
            ring.append(bm.verts.new((D + 0.42 + math.cos(a) * 0.32 * rr_, math.sin(a) * 0.48 * rr_, 0.11 * (1 - t ** 1.6) + rng.uniform(0, 0.008) * (1 - t) + 0.001)))
        rings.append(ring)
    for k in range(ns):
        bm.faces.new((rings[0][0], rings[1][k], rings[1][(k + 1) % ns]))
    for i in range(1, nr):
        for k in range(ns):
            kk = (k + 1) % ns
            bm.faces.new((rings[i][k], rings[i + 1][k], rings[i + 1][kk], rings[i][kk]))
    for k in range(14):
        a = rng.uniform(0, TAU)
        t = rng.uniform(0, 0.9)
        p = Vector((D + 0.42 + math.cos(a) * 0.32 * t, math.sin(a) * 0.48 * t, 0.11 * (1 - t ** 1.6) + 0.01))
        g_lathe(B['coal'], [(0.0, -0.012), (0.02, -0.006), (0.024, 0.004), (0.012, 0.014), (0.0, 0.016)], seg=5, M=T(*p) @ R(rng.uniform(0, 3), 'Z') @ S(1.0, 0.7, 0.9))
    # Schaufel an der Seite angelehnt
    sh = T(0.9, HW + 0.45, 0.0) @ R(0.36, 'X')
    g_box(B['steel'], (-0.13, -0.004, 0.02), (0.13, 0.004, 0.36), M=sh, ch=0.002)
    g_tube(B['walnut_d'], [sh @ Vector((0.0, 0.0, 0.34)), sh @ Vector((0.0, 0.0, 1.20))], 0.016, seg=8)
    g_tube(B['steel'], [sh @ Vector((0.0, 0.0, 1.18)), sh @ Vector((0.0, 0.0, 1.30))], 0.018, seg=8)
    _flush(B, M, MM)



# --- Beverly Room: Mahjong-Tisch, Grammophon --------------------------------------------------------------------------------
def _tile(B, x, y, z, yaw, face_up=False, standing=False, sym=None, rng=None):
    """Mahjong-Stein (24 x 17 x 32 mm): Bein-/Elfenbeinseite und gruener Bambusruecken, Kanten gefast."""
    tw, td, th = 0.024, 0.017, 0.032
    if standing:
        Mx = T(x, y, z) @ R(yaw, 'Z')
        g_box(B['tile'], (-tw / 2, -td / 2, 0.0), (tw / 2, td * 0.15, th), M=Mx, ch=0.0015)
        g_box(B['tile_b'], (-tw / 2, td * 0.15, 0.0), (tw / 2, td / 2, th), M=Mx, ch=0.0015)
        return
    Mx = T(x, y, z) @ R(yaw, 'Z')
    if face_up:
        g_box(B['tile_b'], (-tw / 2, -th / 2, 0.0), (tw / 2, th / 2, td * 0.4), M=Mx, ch=0.0012)
        g_box(B['tile'], (-tw / 2, -th / 2, td * 0.4), (tw / 2, th / 2, td), M=Mx, ch=0.0012)
        if sym is not None:  # Zeichen auf der Oberseite
            kind = sym % 3
            matk = ('sym_r', 'sym_g', 'sym_b')[kind]
            n = 1 + sym % 4
            for k in range(n):
                cx = (k - (n - 1) / 2) * 0.006 if kind != 1 else 0.0
                cy = 0.0 if kind != 1 else (k - (n - 1) / 2) * 0.007
                hx, hy = (0.0022, 0.0022) if kind == 0 else ((0.007, 0.0016) if kind == 1 else (0.0018, 0.008))
                q = [Mx @ Vector((cx - hx, cy - hy, td + 0.0002)), Mx @ Vector((cx + hx, cy - hy, td + 0.0002)),
                     Mx @ Vector((cx + hx, cy + hy, td + 0.0002)), Mx @ Vector((cx - hx, cy + hy, td + 0.0002))]
                g_quad(B[matk], *q)
    else:
        g_box(B['tile'], (-tw / 2, -td / 2, 0.0), (tw / 2, td / 2, th * 0.6), M=Mx)
        g_box(B['tile_b'], (-tw / 2 + 0.0004, -td / 2 + 0.0004, th * 0.6), (tw / 2 - 0.0004, td / 2 - 0.0004, th), M=Mx)


def build_mahjong_table():
    """Kleiner, verzierter Spieltisch (0,9 m, Wiki: Beverly Room) mit unfertiger Mahjong-Partie und Getraenken: Platte
    mit Randprofil und Filz, Zarge mit vier Schubkaestchen (Messingknoepfe), gedrechselte Beine mit Stegkreuz;
    vier Mauern aus doppelt gestapelten Steinen (angebrochen), zwei Haende auf Bänkchen, offene Steine mit Zeichen,
    Wuerfel; Whiskyglaeser mit Eis, Cocktailglas mit Olive, Karaffe mit Stopfen, Sektflasche, Aschenbecher mit
    Zigarette."""
    M = _m()
    rng = random.Random(8)
    B = _B(['walnut', 'walnut_d', 'felt', 'brass', 'tile', 'tile_b', 'sym_r', 'sym_g', 'sym_b', 'glass', 'whisky', 'ice', 'bottle',
            'foil', 'olive', 'paper', 'ember', 'ash_'])
    MX = {'felt': common.material('filz', (0.12, 0.28, 0.18), look='fabric', rough=0.95, r=1.4),
          'tile': common.material('stein', (0.90, 0.87, 0.78), look='ceramic', rough=0.35, r=0.55),
          'tile_b': common.material('stein_ruecken', (0.16, 0.40, 0.28), look='ceramic', rough=0.35, r=0.55),
          'sym_r': common.material('zeichen_rot', (0.62, 0.08, 0.06), look='paint', rough=0.4, r=0.6),
          'sym_g': common.material('zeichen_gruen', (0.08, 0.36, 0.16), look='paint', rough=0.4, r=0.6),
          'sym_b': common.material('zeichen_blau', (0.06, 0.12, 0.40), look='paint', rough=0.4, r=0.6),
          'whisky': common.material('whisky', (0.62, 0.34, 0.10), look='glass', rough=0.05, r=0.15),
          'ice': common.material('eis', (0.86, 0.90, 0.92), look='glass', rough=0.15, r=0.3),
          'bottle': common.material('flaschenglas', (0.10, 0.24, 0.12), look='glass', rough=0.05, r=0.15),
          'foil': common.material('folie', (0.78, 0.62, 0.26), look='metal', rough=0.35, metal=1.0, r=0.6, m=0.9),
          'olive': common.material('olive', (0.30, 0.38, 0.10), look='ceramic', rough=0.4, r=0.6),
          'paper': common.material('zigarette', (0.90, 0.88, 0.84), look='paint', rough=0.8, r=1.2),
          'ember': common.material('glut_zig', (0.9, 0.3, 0.08), look='glow', glow=1.2),
          'ash_': common.material('zig_asche', (0.42, 0.41, 0.40), look='concrete', rough=1.0, r=1.5)}
    Hz = 0.74
    # Platte mit Randprofil, Filzeinlage
    g_box(B['walnut'], (-0.43, -0.43, Hz - 0.03), (0.43, 0.43, Hz), ch=0.004)
    edge = [(-0.01, -0.03), (0.012, -0.03), (0.016, -0.024), (0.016, -0.014), (0.02, -0.008), (0.02, 0.004), (0.014, 0.014), (0.004, 0.016),
            (-0.01, 0.016)]
    sq = [(-0.43, -0.43, Hz), (0.43, -0.43, Hz), (0.43, 0.43, Hz), (-0.43, 0.43, Hz)]
    g_sweep(B['walnut_d'], edge, sq, closed=True, up=(0, 0, 1))
    g_box(B['felt'], (-0.405, -0.405, Hz), (0.405, 0.405, Hz + 0.004))
    # Zarge mit Schubkaestchen, Beine, Stegkreuz
    for k in range(4):
        Rk = R(k * math.pi / 2, 'Z')
        g_box(B['walnut'], (0.33, -0.36, Hz - 0.13), (0.36, 0.36, Hz - 0.03), M=Rk)
        g_box(B['walnut_d'], (0.36, -0.12, Hz - 0.115), (0.372, 0.12, Hz - 0.045), M=Rk, ch=0.003)
        g_lathe(B['brass'], [(0.0, 0.0), (0.009, 0.0), (0.006, 0.006), (0.009, 0.012), (0.0, 0.016)], seg=10,
                M=Rk @ T(0.372, 0.0, Hz - 0.08) @ R(math.pi / 2, 'Y'))
        c = Rk @ Vector((0.34, 0.34, 0.0))
        leg = [(0.0, 0.0), (0.022, 0.0), (0.026, 0.02), (0.02, 0.05), (0.024, 0.09), (0.03, 0.12), (0.028, 0.3), (0.022, 0.42), (0.026, 0.46),
               (0.03, 0.48), (0.022, 0.5), (0.028, 0.53), (0.032, Hz - 0.13), (0.032, Hz - 0.03), (0.0, Hz - 0.03)]
        g_lathe(B['walnut_d'], leg, seg=12, M=T(c.x, c.y, 0.0))
    for s in (-1, 1):
        g_tube(B['walnut_d'], [(-0.32 * s, -0.32, 0.14), (0.32 * s, 0.32, 0.14)], 0.014, seg=8)
    g_lathe(B['walnut_d'], [(0.0, 0.12), (0.04, 0.12), (0.045, 0.14), (0.04, 0.16), (0.0, 0.16)], seg=12)
    zt = Hz + 0.004
    # vier Mauern (17 Paare je Seite), eine angebrochen; zwei Haende auf Baenkchen; offene Steine
    for side in range(4):
        Rs = R(side * math.pi / 2, 'Z')
        broken = side == 1
        for k in range(17):
            if broken and 6 <= k <= 9:
                continue
            p = Rs @ Vector((-0.204 + k * 0.0255, -0.26, zt))
            for lvl in range(2):
                if lvl == 1 and (broken and k in (5, 10)):
                    continue
                _tile(B, p.x, p.y, zt + lvl * 0.0192, side * math.pi / 2)
    for side in (0, 2):
        Rs = R(side * math.pi / 2, 'Z')
        rk = Rs @ Vector((0.0, -0.36, zt))
        g_box(B['walnut'], (-0.19, -0.012, 0.0), (0.19, 0.012, 0.012), M=T(rk.x, rk.y, zt) @ R(side * math.pi / 2, 'Z'), ch=0.003)
        for k in range(13):
            p = Rs @ Vector((-0.156 + k * 0.026, -0.355, zt))
            _tile(B, p.x, p.y, zt + 0.012, side * math.pi / 2 + rng.uniform(-0.04, 0.04), standing=True)
    for k in range(22):
        x, y = rng.uniform(-0.13, 0.13), rng.uniform(-0.13, 0.13)
        _tile(B, x, y, zt, rng.uniform(0, TAU), face_up=True, sym=rng.randrange(12), rng=rng)
    for k in range(2):  # Wuerfel
        g_box(B['tile'], (-0.0065, -0.0065, 0.0), (0.0065, 0.0065, 0.013), M=T(0.17 + 0.02 * k, -0.12 + 0.03 * k, zt) @ R(0.4 + k, 'Z'), ch=0.002)
    # Getraenke
    for k, (x, y) in enumerate(((0.33, 0.30), (-0.30, 0.34), (0.31, -0.33))):
        g_lathe(B['glass'], [(0.0, 0.0), (0.032, 0.0), (0.034, 0.004), (0.036, 0.09), (0.033, 0.09), (0.031, 0.014), (0.0, 0.014)], seg=12,
                M=T(x, y, zt))
        lvl = (0.04, 0.025, 0.055)[k]
        g_lathe(B['whisky'], [(0.0, 0.0145), (0.031, 0.0145), (0.032, lvl), (0.0, lvl)], seg=12, M=T(x, y, zt))
        g_box(B['ice'], (-0.011, -0.011, 0.0), (0.011, 0.011, 0.02), M=T(x + 0.006, y - 0.004, zt + lvl - 0.012) @ R(0.5 + k, 'Z') @ R(0.3, 'X'), ch=0.004)
    xc, yc = -0.34, -0.06  # Cocktailglas mit Olive
    g_lathe(B['glass'], [(0.0, 0.0), (0.035, 0.0), (0.036, 0.003), (0.004, 0.006), (0.0035, 0.09), (0.006, 0.095), (0.055, 0.15), (0.052, 0.151),
                         (0.004, 0.10), (0.0, 0.10)], seg=14, M=T(xc, yc, zt))
    g_lathe(B['olive'], [(0.0, -0.008), (0.007, -0.005), (0.0075, 0.003), (0.0, 0.009)], seg=8, M=T(xc + 0.01, yc, zt + 0.118))
    g_tube(B['walnut_d'], [(xc + 0.01, yc, zt + 0.112), (xc + 0.035, yc, zt + 0.165)], 0.0012, seg=4)
    xd, yd = -0.33, -0.31  # Karaffe (achtkantig) mit Stopfen
    g_lathe(B['glass'], [(0.0, 0.0), (0.05, 0.0), (0.055, 0.01), (0.055, 0.13), (0.045, 0.16), (0.016, 0.19), (0.014, 0.23), (0.02, 0.235),
                         (0.0, 0.235)], seg=8, M=T(xd, yd, zt))
    g_lathe(B['whisky'], [(0.0, 0.006), (0.05, 0.006), (0.05, 0.085), (0.0, 0.085)], seg=8, M=T(xd, yd, zt))
    g_lathe(B['glass'], [(0.0, 0.235), (0.01, 0.235), (0.012, 0.25), (0.025, 0.27), (0.02, 0.29), (0.0, 0.295)], seg=8, M=T(xd, yd, zt))
    xb, yb = 0.02, 0.36  # Sektflasche
    g_lathe(B['bottle'], [(0.0, 0.0), (0.042, 0.0), (0.044, 0.02), (0.044, 0.19), (0.036, 0.23), (0.016, 0.27), (0.015, 0.29), (0.0, 0.29)],
            seg=14, M=T(xb, yb, zt))
    g_lathe(B['foil'], [(0.0, 0.26), (0.0175, 0.26), (0.0185, 0.30), (0.012, 0.31), (0.0, 0.312)], seg=12, M=T(xb, yb, zt))
    g_lathe(B['glass'], [(0.0, 0.0), (0.045, 0.0), (0.05, 0.02), (0.046, 0.024), (0.03, 0.012), (0.0, 0.012)], seg=14, M=T(0.18, 0.16, zt))
    g_tube(B['paper'], [(0.16, 0.15, zt + 0.02), (0.235, 0.18, zt + 0.026)], 0.0042, seg=6)
    g_lathe(B['ash_'], [(0.0, 0.0), (0.0043, 0.0), (0.0043, 0.012), (0.0, 0.013)], seg=6, M=T(0.235, 0.18, zt + 0.026) @ R(math.pi / 2 - 0.08, 'Y') @ R(0.38, 'X'))
    _flush(B, M, MX)


def _horn(bm, path, r0, r1, petals=8, ring=24):
    """Trichter (Blume) entlang eines Pfads: Radius waechst nach aussen, Naehte zwischen den Blaettern."""
    path = [Vector(p) for p in path]
    d, fr = _frames(path, False, None)
    rings = []
    n = len(path)
    for i in range(n):
        t = i / (n - 1)
        u, v = fr[min(i, n - 2)]
        r = r0 + (r1 - r0) * t ** 2.4
        rw = []
        for k in range(ring):
            a = TAU * k / ring
            seam = math.cos(petals * a)
            rr = r * (1.0 + 0.05 * t * t * seam)
            rw.append(bm.verts.new(path[i] + (u * math.cos(a) + v * math.sin(a)) * rr))
        rings.append(rw)
    for i in range(n - 1):
        for k in range(ring):
            kk = (k + 1) % ring
            bm.faces.new((rings[i][k], rings[i][kk], rings[i + 1][kk], rings[i + 1][k]))
    return [v.co.copy() for v in rings[-1]]


def build_gramophone():
    """Trichtergrammophon auf einem Schraenkchen (Boden z = 0; Wiki: Jazz von alten Plattenspielern): Schrank mit
    Fuellungstueren und Stoffbespannung, Laufwerkskasten mit Profil, Plattenteller mit Filz und Schallplatte (Rillen,
    Etikett), Tonarm mit Schalldose und Nadel, Blumentrichter mit Blattnaehten und gerolltem Rand, Kurbel mit
    Holzgriff, Bremshebel und Drehzahlknopf."""
    M = _m()
    B = _B(['walnut', 'walnut_d', 'walnut_l', 'brass', 'brass_d', 'black', 'felt', 'label', 'grille', 'glass', 'steel', 'ebony'])
    MX = {'felt': common.material('tellerfilz', (0.30, 0.06, 0.06), look='fabric', rough=0.95, r=1.4),
          'label': common.material('etikett', (0.55, 0.10, 0.08), look='paint', rough=0.6, r=0.9),
          'grille': common.material('bespannung', (0.32, 0.24, 0.14), look='fabric', rough=0.9, r=1.3)}
    record = common.material('schellack', (0.025, 0.025, 0.028), look='plastic', rough=0.25, r=0.45)
    # Schraenkchen
    g_box(B['walnut'], (-0.29, -0.24, 0.07), (0.29, 0.24, 0.74), ch=0.006)
    g_mold_u(B['walnut_d'], -0.29, 0.29, 0.24, 0.0, PLINTH)
    g_box(B['walnut'], (-0.30, -0.255, 0.74), (0.305, 0.255, 0.762), ch=0.006)
    for s in (-1, 1):
        ya, yb = sorted((s * 0.006, s * 0.22))
        g_box(B['walnut_l'], (0.29, ya, 0.12), (0.305, yb, 0.70), ch=0.003)
        g_box(B['grille'], (0.305, ya + 0.04, 0.30), (0.306, yb - 0.04, 0.64))
        g_mold_rect(B['walnut_d'], 0.3055, ya + 0.04, 0.30, yb - 0.04, 0.64, BEAD)
        g_lathe(B['brass'], [(0.0, 0.0), (0.009, 0.0), (0.007, 0.008), (0.009, 0.014), (0.0, 0.017)], seg=10, M=T(0.305, s * 0.03, 0.40) @ R(math.pi / 2, 'Y'))
    # Laufwerkskasten
    z0 = 0.762
    g_box(B['walnut'], (-0.22, -0.20, z0), (0.22, 0.20, z0 + 0.13), ch=0.006)
    g_mold_u(B['walnut_d'], -0.22, 0.22, 0.20, z0, [(-0.02, 0.0), (0.012, 0.0), (0.012, 0.006), (0.006, 0.012), (0.004, 0.02), (0.0, 0.024), (-0.02, 0.024)])
    g_box(B['walnut_d'], (-0.222, -0.202, z0 + 0.12), (0.222, 0.202, z0 + 0.135), ch=0.004)
    zt = z0 + 0.135
    g_lathe(B['steel'], [(0.0, zt), (0.155, zt), (0.157, zt + 0.006), (0.155, zt + 0.014), (0.0, zt + 0.014)], seg=32, M=T(0.02, 0.0, 0))
    g_lathe(B['felt'], [(0.0, zt + 0.014), (0.150, zt + 0.014), (0.150, zt + 0.0165), (0.0, zt + 0.0165)], seg=32, M=T(0.02, 0.0, 0))
    bm = bmesh.new()
    rp = [(0.0, zt + 0.0165), (0.148, zt + 0.0165), (0.149, zt + 0.018), (0.147, zt + 0.0193)]
    for k in range(12):  # Rillen (unter streifendem Licht sichtbar)
        rp.append((0.143 - k * 0.0075, zt + (0.0192 if k % 2 == 0 else 0.0187)))
    rp += [(0.048, zt + 0.019), (0.0, zt + 0.019)]
    g_lathe(bm, rp, seg=32, M=T(0.02, 0.0, 0))
    _obj('platte', bm, record, smooth=None, recalc=True)
    g_lathe(B['label'], [(0.0, zt + 0.0194), (0.045, zt + 0.0194), (0.045, zt + 0.0197), (0.0, zt + 0.0197)], seg=24, M=T(0.02, 0.0, 0))
    g_lathe(B['steel'], [(0.0, zt + 0.019), (0.0035, zt + 0.019), (0.0035, zt + 0.032), (0.0, zt + 0.034)], seg=8, M=T(0.02, 0.0, 0))
    # Tonarm: Sockel hinten rechts, Schwanenhals zur Schalldose
    bx, by = -0.16, -0.14
    g_lathe(B['brass'], [(0.0, zt), (0.03, zt), (0.03, zt + 0.012), (0.018, zt + 0.02), (0.016, zt + 0.06), (0.0, zt + 0.062)], seg=14, M=T(bx, by, 0))
    arm = _bez((bx, by, zt + 0.055), (bx + 0.08, by + 0.03, zt + 0.07), (0.0, 0.05, zt + 0.08), (0.06, 0.08, zt + 0.06), 10)
    g_tube(B['brass'], arm, 0.009, seg=8, scales=[1.4, 1.3, 1.2, 1.1, 1.0, 0.95, 0.9, 0.85, 0.82, 0.8, 0.8])
    sd = T(0.075, 0.085, zt + 0.045) @ R(math.pi / 2, 'X') @ R(0.0, 'Z')
    g_lathe(B['brass'], [(0.0, -0.01), (0.034, -0.01), (0.036, -0.006), (0.036, 0.006), (0.034, 0.01), (0.0, 0.01)], seg=20, M=sd)
    g_lathe(B['glass'], [(0.0, 0.0102), (0.03, 0.0102), (0.03, 0.0105), (0.0, 0.0106)], seg=20, M=sd)
    g_tube(B['steel'], [(0.075, 0.085, zt + 0.012), (0.075, 0.085, zt + 0.0195)], 0.0012, seg=4)
    # Trichter: aus dem Tonarmsockel nach hinten oben, Muendung nach vorn oben
    hp = _bez((bx - 0.02, by - 0.02, zt + 0.06), (bx - 0.2, by - 0.02, zt + 0.10), (-0.50, -0.08, zt + 0.30), (-0.30, -0.06, zt + 0.40), 14)
    bm = bmesh.new()
    rim = _horn(bm, hp, 0.022, 0.26)
    ob = _obj('trichter', bm, M['brass'], smooth=50)
    sol = ob.modifiers.new('dicke', 'SOLIDIFY')
    sol.thickness = 0.0025
    g_tube(B['brass_d'], rim, 0.006, seg=6, closed=True)
    # Kurbel rechts, Bremse und Drehzahlknopf
    g_lathe(B['brass'], [(0.0, 0.0), (0.016, 0.0), (0.016, 0.012), (0.0, 0.012)], seg=12, M=T(0.0, 0.20, z0 + 0.07) @ R(-math.pi / 2, 'X'))
    g_tube(B['steel'], [(0.0, 0.212, z0 + 0.07), (0.0, 0.26, z0 + 0.07), (0.0, 0.27, z0 + 0.06), (0.0, 0.27, z0 - 0.04)], 0.0055, seg=6)
    g_lathe(B['ebony'], [(0.0, 0.0), (0.011, 0.002), (0.013, 0.03), (0.011, 0.06), (0.0, 0.065)], seg=10, M=T(0.0, 0.27, z0 - 0.04) @ R(math.pi / 2, 'X'))
    g_box(B['brass'], (0.14, -0.17, zt), (0.17, -0.12, zt + 0.004), ch=0.001)
    g_tube(B['brass'], [(0.155, -0.145, zt + 0.004), (0.155, -0.12, zt + 0.02)], 0.003, seg=5)
    g_lathe(B['ebony'], [(0.0, 0.0), (0.012, 0.0), (0.012, 0.012), (0.0, 0.014)], seg=10, M=T(0.17, 0.15, zt))
    _flush(B, M, MX)


# --- Beistelltisch (4 Bauarten; v1/v3 mit Lampe) ---------------------------------------------------------------------------
def _side_table(v):
    """Beistelltisch, Platte genau in 0,66 m (Funde liegen darauf). Varianten 0/1 (rund, mit Lampe) baut das Modul der
    Level-1-Codemodelle (models_l1_code); hier v2 eckiger Teetisch mit Ablage und v3 dazu Messing-Leuchter mit
    Seidenschirm (Lampe hinten, -x, damit Funde vorn Platz haben)."""
    M = _m()
    B = _B(['walnut', 'walnut_d', 'brass', 'brass_d', 'ivory'])
    H = 0.66
    g_box(B['walnut'], (-0.27, -0.27, H - 0.024), (0.27, 0.27, H), ch=0.005)
    g_sweep(B['walnut_d'], [(-0.01, -0.024), (0.006, -0.024), (0.009, -0.018), (0.009, -0.008), (0.004, 0.0), (-0.01, 0.0)],
            [(-0.27, -0.27, H), (0.27, -0.27, H), (0.27, 0.27, H), (-0.27, 0.27, H)], closed=True, up=(0, 0, 1))
    for k in range(4):
        Rk = R(k * math.pi / 2, 'Z')
        g_box(B['walnut_d'], (0.22, -0.20, H - 0.10), (0.24, 0.20, H - 0.024), M=Rk)
        c = Rk @ Vector((0.215, 0.215, 0.0))
        g_prism(B['walnut_d'], [(-0.02, -0.02), (0.02, -0.02), (0.02, 0.02), (-0.02, 0.02)], 0.03, H - 0.024, M=T(c.x, c.y, 0))
        g_lathe(B['walnut_d'], [(0.0, 0.0), (0.016, 0.0), (0.022, 0.012), (0.02, 0.03), (0.0, 0.03)], seg=10, M=T(c.x, c.y, 0))
    g_box(B['walnut'], (-0.22, -0.22, 0.16), (0.22, 0.22, 0.178), ch=0.003)
    g_box(B['walnut'], (0.238, -0.15, H - 0.092), (0.246, 0.15, H - 0.032), ch=0.002)   # Schublade
    g_mold_rect(B['walnut_d'], 0.246, -0.14, H - 0.085, 0.14, H - 0.039, [(-0.002, 0.0), (0.0, 0.0015), (0.002, 0.0)])
    g_lathe(B['brass'], [(0.0, 0.0), (0.008, 0.0), (0.006, 0.006), (0.009, 0.012), (0.0, 0.015)], seg=10,
            M=T(0.246, 0.0, H - 0.062) @ R(math.pi / 2, 'Y'))
    g_quad(B['brass_d'], (0.2462, -0.012, H - 0.082), (0.2462, 0.012, H - 0.082), (0.2462, 0.012, H - 0.042), (0.2462, -0.012, H - 0.042))
    if v == 3:  # Leuchter mit Seidenschirm
        x0 = -0.12
        g_lathe(B['brass'], [(0.0, H), (0.06, H), (0.064, H + 0.012), (0.04, H + 0.02), (0.02, H + 0.05), (0.03, H + 0.08), (0.016, H + 0.1),
                             (0.012, H + 0.32), (0.024, H + 0.33), (0.02, H + 0.35), (0.0, H + 0.35)], seg=16, M=T(x0, 0, 0))
        _shade(x0, 0.0, H + 0.28, 0.13, 0.11, 0.17, 18, 'schirm_seide', pleat=0.0)
    _flush(B, M)


def _shade(x, y, z0, r0, r1, h, n, name, pleat=0.07):
    shade = common.material(name, (1.0, 0.88, 0.70), look='glow', glow=0.9)
    trim = common.material('schirmborte', (0.62, 0.48, 0.24), look='fabric', rough=0.7, r=0.9)
    bm = bmesh.new()
    rings = []
    for j in range(3):
        t = j / 2
        rr = r0 * (1 - t) + r1 * t
        rings.append([bm.verts.new((x + rr * (1 + pleat * (k % 2)) * math.cos(TAU * k / n), y + rr * (1 + pleat * (k % 2)) * math.sin(TAU * k / n), z0 + h * t))
                      for k in range(n)])
    for j in range(2):
        for k in range(n):
            kk = (k + 1) % n
            bm.faces.new((rings[j][k], rings[j][kk], rings[j + 1][kk], rings[j + 1][k]))
    ob = _obj(name, bm, shade, smooth=40)
    sol = ob.modifiers.new('dicke', 'SOLIDIFY')
    sol.thickness = 0.002
    bm = bmesh.new()
    for (zz, rr) in ((z0, r0 * (1 + pleat * 0.5)), (z0 + h, r1 * (1 + pleat * 0.5))):
        g_lathe(bm, [(rr - 0.002, zz - 0.004), (rr + 0.003, zz - 0.004), (rr + 0.003, zz + 0.004), (rr - 0.002, zz + 0.004)], seg=n, M=T(x, y, 0))
    _obj(name + '_rand', bm, trim, smooth=40, recalc=True)


def build_side_table__v2():
    _side_table(2)


def build_side_table__v3():
    _side_table(3)



# --- Vertaefelung und Gesims (hundertfach: sparsam, aber echte Profile) ------------------------------------------------------
def build_wainscot():
    """Wandvertaefelung (Wandflaeche x = 0, 1 m entlang y, 1,02 m hoch): profilierte Sockelleiste, Rahmen,
    abgeplattete Fuellung mit Kehlleiste, Bruestungsleiste mit Wulst und Hohlkehle. Etwa 180 Dreiecke."""
    M = _m()
    base = [(0.0, 0.0), (0.024, 0.0), (0.024, 0.105), (0.021, 0.112), (0.019, 0.124), (0.015, 0.134), (0.0125, 0.145),
            (0.0125, 0.152), (0.0, 0.152)]
    ob = common.profile_extrude('sockel', base, 1.0, M['walnut_d'], axis='y')
    ob.location = (0.0, -0.5, 0.0)
    rail = [(0.0, 0.858), (0.018, 0.858), (0.021, 0.862), (0.021, 0.870), (0.026, 0.879), (0.031, 0.891), (0.036, 0.903),
            (0.040, 0.914), (0.043, 0.926), (0.044, 0.940), (0.043, 0.953), (0.040, 0.963), (0.034, 0.970), (0.020, 0.974),
            (0.016, 0.984), (0.014, 1.0), (0.012, 1.02), (0.0, 1.02)]
    ob = common.profile_extrude('bruestung', rail, 1.0, M['walnut_d'], axis='y')
    ob.location = (0.0, -0.5, 0.0)
    bm = bmesh.new()
    g_box(bm, (0.0, -0.5, 0.152), (0.016, 0.5, 0.858))
    _obj('rahmen', bm, M['walnut'], smooth=None)
    bp, bmo = bmesh.new(), bmesh.new()
    _raised_panel(bp, bmo, 0.016, -0.40, 0.25, 0.40, 0.79, bevel=0.03)
    _obj('fuellung', bp, M['walnut_l'], smooth=25)
    _obj('kehlleiste', bmo, M['walnut_d'], smooth=45)


def build_cornice():
    """Deckengesims (Wandflaeche x = 0, 1 m entlang y; Decke z = 0, haengt 0,26 m herab): Stuck mit Karnies, Hängeplatte,
    Zahnschnitt, Viertelstab; vergoldeter Rundstab unter der Hängeplatte. Etwa 300 Dreiecke."""
    M = _m()
    prof = [(0.0, 0.0), (0.0, -0.26), (0.040, -0.26), (0.042, -0.252), (0.046, -0.240), (0.058, -0.236), (0.070, -0.229),
            (0.081, -0.220), (0.089, -0.209), (0.094, -0.195), (0.096, -0.192), (0.100, -0.190), (0.100, -0.155), (0.104, -0.153),
            (0.122, -0.153), (0.124, -0.150), (0.124, -0.100), (0.128, -0.098), (0.136, -0.096), (0.138, -0.092), (0.142, -0.084),
            (0.150, -0.072), (0.162, -0.062), (0.176, -0.055), (0.186, -0.046), (0.193, -0.034), (0.196, -0.022), (0.204, -0.018),
            (0.204, 0.0)]
    ob = common.profile_extrude('gesims', list(reversed(prof)), 1.0, M['plaster'], axis='y')
    ob.location = (0.0, -0.5, 0.0)
    bm = bmesh.new()
    g_tube(bm, [(0.125, -0.5, -0.146), (0.125, 0.5, -0.146)], 0.0055, seg=6)
    _obj('rundstab', bm, M['gilt'], smooth=60)
    bm = bmesh.new()  # Zahnschnitt: nur sichtbare Flaechen (vorn, unten, Seiten)
    for k in range(20):
        y0 = -0.5 + 0.014 + k * 0.05
        y1 = y0 + 0.022
        x0, x1, z0, z1 = 0.100, 0.117, -0.188, -0.157
        g_quad(bm, (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1))
        g_quad(bm, (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0))
        g_quad(bm, (x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1))
        g_quad(bm, (x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1))
    for f in bm.faces:  # Normalen nach aussen (vom Block weg)
        f.normal_update()
        c = f.calc_center_median()
        k = round((c.y + 0.5 - 0.014 - 0.011) / 0.05)
        cb = Vector((0.1085, -0.5 + 0.025 + k * 0.05, -0.1725))
        if f.normal.dot(c - cb) < 0:
            f.normal_flip()
    _obj('zahnschnitt', bm, M['plaster'], smooth=None)


# --- Kerzenwandleuchter (3 Bauarten) -------------------------------------------------------------------------------------
def _candle(bm_c, bm_f, bm_p, x, y, z, h, seed, socket=True):
    """Tropfteller mit Tuelle (bm_p), Kerzenhuelse mit Wachsnasen (bm_c), Flammenbirne (bm_f) ab Hoehe z."""
    rng = random.Random(seed)
    g_lathe(bm_p, [(0.0, -0.004), (0.020, -0.003), (0.035, 0.005), (0.033, 0.010), (0.013, 0.008), (0.013, 0.027), (0.0, 0.027)],
            seg=10, M=T(x, y, z))
    pr = [(0.0, 0.026), (0.0105, 0.026)]
    for k in range(1, 3):
        pr.append((0.0105 + rng.uniform(-0.0006, 0.0008), 0.026 + h * k / 2))
    pr += [(0.0085, 0.026 + h + 0.003), (0.0, 0.026 + h + 0.003)]
    g_lathe(bm_c, pr, seg=8, M=T(x, y, z), phase=rng.uniform(0, 1))
    for k in range(1):  # Wachsnase
        a = rng.uniform(0, TAU)
        L = rng.uniform(0.02, 0.045)
        cx, cy = x + math.cos(a) * 0.0108, y + math.sin(a) * 0.0108
        top = z + 0.026 + h
        g_lathe(bm_c, [(0.0, -L), (0.0032, -L * 0.5), (0.0026, -0.002), (0.0, 0.002)], seg=5,
                M=T(cx, cy, top))
    zf = z + 0.026 + h + 0.003
    g_lathe(bm_f, [(0.0, 0.0), (0.007, 0.004), (0.0105, 0.016), (0.0095, 0.031), (0.005, 0.042), (0.0, 0.050)], seg=8, M=T(x, y, zf))


def _sconce(v):
    M = _m()
    flame = common.material('flamme', (1.0, 0.80, 0.50), look='glow', glow=3.5)
    bm_b, bm_d, bm_c, bm_f, bm_p = bmesh.new(), bmesh.new(), bmesh.new(), bmesh.new(), bmesh.new()
    # Wandschild: ovale Rosette (Drehkoerper um x, in z gestreckt)
    sz = 2.2 if v != 1 else 2.6
    g_lathe(bm_b, [(0.0, 0.0), (0.048, 0.0), (0.050, 0.006), (0.042, 0.012), (0.030, 0.014), (0.018, 0.021), (0.0, 0.026)], seg=16, M=T(0, 0, 0.0) @ R(math.pi / 2, 'Y') @ S(sz, 1.0, 1.0))
    g_lathe(bm_d, [(0.040, 0.0006), (0.044, 0.004), (0.041, 0.009), (0.034, 0.0105)], seg=16,
            M=T(0.0006, 0, 0.0) @ R(math.pi / 2, 'Y') @ S(sz, 1.0, 1.0))
    # Zapfen unten (Eichel) und Knauf in der Mitte
    g_lathe(bm_b, [(0.0, -0.125), (0.008, -0.116), (0.013, -0.100), (0.008, -0.086), (0.0105, -0.076), (0.006, -0.068), (0.0, -0.066)],
            seg=8, M=T(0.026, 0, 0.0))
    g_lathe(bm_b, [(0.0, 0.0), (0.016, 0.0), (0.016, 0.009), (0.010, 0.022), (0.0, 0.025)], seg=10,
            M=T(0.02, 0, -0.03) @ R(math.pi / 2, 'Y'))
    arms = [(-1, 0.13), (1, 0.13)] if v != 2 else [(-1, 0.112), (1, 0.112)]
    tops = []
    for (s, yy) in arms:
        p = _bez((0.035, s * 0.006, -0.032), (0.11, s * 0.02, -0.075), (0.135, s * yy, -0.06), (0.13, s * yy, -0.004), 9)
        g_tube(bm_b, p, 0.0068, seg=6, scales=[1.0 - 0.25 * k / 9 for k in range(10)])
        # Zierschnecke unter dem Arm
        c0 = Vector((0.085, s * 0.04, -0.07))
        sp = [c0 + Vector((0.0, s * 0.02 * math.cos(a) * (1 - a / 9), 0.018 * math.sin(a) * (1 - a / 9))) for a in [k * 0.6 for k in range(12)]]
        g_tube(bm_b, sp, 0.0035, seg=5)
        g_lathe(bm_b, [(0.0, -0.006), (0.0095, -0.006), (0.011, 0.0), (0.0095, 0.006), (0.0, 0.006)], seg=8, M=T(0.13, s * yy, -0.012))
        tops.append((0.13, s * yy, -0.004))
    if v == 1:  # dritter Arm: gerade nach oben, Kerze hoeher
        g_tube(bm_b, [(0.03, 0.0, -0.01), (0.06, 0.0, 0.0), (0.07, 0.0, 0.03), (0.07, 0.0, 0.05)], 0.0065, seg=8)
        tops.append((0.07, 0.0, 0.05))
    for k, (x, y, z) in enumerate(tops):
        h = 0.085 if (v == 1 and k == 2) else 0.10
        _candle(bm_c, bm_f, bm_p, x, y, z, h if v != 1 or k == 2 else 0.075, 5 + k + 7 * v)
    _obj('schild', bm_b, M['brass'], smooth=40)
    _obj('schild_tief', bm_d, M['brass_d'], smooth=40)
    _obj('teller', bm_p, M['brass'], smooth=40)
    _obj('kerzen', bm_c, M['ivory'], smooth=50)
    _obj('flammen', bm_f, flame, smooth=60)
    if v == 2:  # gefaltete Stoffschirmchen (leuchten durch)
        shade = common.material('schirm', (1.0, 0.86, 0.66), look='glow', glow=1.1)
        rim = common.material('schirm_rand', (0.70, 0.55, 0.30), look='fabric', rough=0.7, r=0.9)
        bm, br = bmesh.new(), bmesh.new()
        for (x, y, z) in tops:
            zc = z + 0.095
            ring = []
            for j in range(3):
                t = j / 2
                rr = 0.054 * (1 - t) + 0.033 * t
                ring.append([bm.verts.new((x + (rr * (1.0 + 0.06 * (k % 2))) * math.cos(TAU * k / 20),
                                           y + (rr * (1.0 + 0.06 * (k % 2))) * math.sin(TAU * k / 20), zc + 0.085 * t)) for k in range(20)])
            for j in range(2):
                for k in range(20):
                    kk = (k + 1) % 20
                    bm.faces.new((ring[j][k], ring[j][kk], ring[j + 1][kk], ring[j + 1][k]))
            for (zz, rr) in ((zc, 0.057), (zc + 0.085, 0.035)):
                g_lathe(br, [(rr - 0.002, zz - 0.002), (rr + 0.002, zz - 0.002), (rr + 0.002, zz + 0.003), (rr - 0.002, zz + 0.003)],
                        seg=14, M=T(x, y, 0))
        ob = _obj('schirme', bm, shade, smooth=40)
        sol = ob.modifiers.new('dicke', 'SOLIDIFY')
        sol.thickness = 0.0015
        _obj('schirmrand', br, rim, smooth=40, recalc=True)


def build_candle_sconce():
    _sconce(0)


def build_candle_sconce__v1():
    _sconce(1)


def build_candle_sconce__v2():
    _sconce(2)


# --- Zimmertueren: 6 Bauarten (a-f) x 4 Varianten, Nummern ohne Muster (Wiki) ---------------------------------------------------
DOOR_NUMBERS = {'a': (117, 1213, 52, 386), 'b': (2041, 7, 469, 3318), 'c': (31, 812, 1607, 94), 'd': (608, 2275, 13, 541),
                'e': (9, 730, 4102, 68), 'f': (343, 1089, 26, 957)}
CASING = [(-0.006, 0.0), (-0.006, 0.016), (-0.002, 0.021), (0.004, 0.022), (0.008, 0.019), (0.010, 0.016), (0.013, 0.018),
          (0.050, 0.020), (0.056, 0.022), (0.062, 0.026), (0.070, 0.031), (0.078, 0.034), (0.084, 0.034), (0.088, 0.030),
          (0.090, 0.026), (0.093, 0.012), (0.094, 0.0)]
DOOR_PANELS = {  # (y0, z0, y1, z1) der Fuellungen auf dem Blatt (y -0.46..0.46, z 0..2.12)
    'a': [(-0.35, 0.24, -0.04, 0.90), (0.04, 0.24, 0.35, 0.90), (-0.35, 1.12, -0.04, 1.98), (0.04, 1.12, 0.35, 1.98)],
    'b': [(-0.35, 0.24, -0.04, 0.70), (0.04, 0.24, 0.35, 0.70), (-0.35, 0.86, -0.04, 1.56), (0.04, 0.86, 0.35, 1.56),
          (-0.35, 1.70, -0.04, 1.98), (0.04, 1.70, 0.35, 1.98)],
    'c': [(-0.33, 0.24, 0.33, 0.86), (-0.33, 1.08, 0.33, 1.96)],
    'd': [(-0.33, 0.22 + k * 0.36, 0.33, 0.22 + k * 0.36 + 0.28) for k in range(5)],
    'e': [(-0.35, 0.24, -0.04, 1.98), (0.04, 0.24, 0.35, 1.98)],
    'f': [(-0.35, 0.24, -0.04, 0.66), (0.04, 0.24, 0.35, 0.66), (-0.35, 0.86, -0.04, 1.98), (0.04, 0.86, 0.35, 1.98)],
}


def _hotel_door(style, v):
    """Verschlossene Zimmertuer (Wandteil, Wandflaeche x = 0, Boden z = 0): Bekleidung mit Profil und Sockelkloetzen,
    Schattenfuge, Blatt mit abgeplatteten Fuellungen und Kehlleisten (Bauart je Buchstabe), Langschild mit Druecker
    und Schluesselloch, drei Baender, Nummernschild (Messing, Ziffern schwarz) - Nummern ohne Muster.
    Varianten: v1 Trittblech, v2 Schild "DO NOT DISTURB" am Druecker, v3 Trittblech und Schluessel im Schloss."""
    M = _m()
    number = DOOR_NUMBERS[style][v]
    W, H, XL = 0.92, 2.12, 0.040
    wood = M['walnut'] if style not in ('b', 'e') else common.material('mahagoni', (0.52, 0.25, 0.17), look='wood', tex=WOOD, rough=0.45, r=0.6)
    # Bekleidung (U-Pfad gegen den Uhrzeigersinn von +x) und Sockelkloetze
    bm = bmesh.new()
    g_sweep(bm, CASING, [(0.0, W / 2 + 0.006, 0.20), (0.0, W / 2 + 0.006, H + 0.006), (0.0, -W / 2 - 0.006, H + 0.006),
                         (0.0, -W / 2 - 0.006, 0.20)], up=(1, 0, 0), pclosed=False)
    for s in (-1, 1):
        g_box(bm, (0.0, min(s * (W / 2), s * (W / 2 + 0.105)), 0.0), (0.040, max(s * (W / 2), s * (W / 2 + 0.105)), 0.215), ch=0.004)
    _obj('bekleidung', bm, M['walnut_d'], smooth=40)
    if style in ('c', 'f'):  # Verdachung ueber der Tuer
        bm = bmesh.new()
        g_box(bm, (0.0, -W / 2 - 0.12, H + 0.10), (0.034, W / 2 + 0.12, H + 0.13), ch=0.004)
        g_sweep(bm, CROWN, [(0.0, -W / 2 - 0.12, H + 0.13), (0.034, -W / 2 - 0.12, H + 0.13), (0.034, W / 2 + 0.12, H + 0.13),
                            (0.0, W / 2 + 0.12, H + 0.13)], up=(0, 0, 1))
        _obj('verdachung', bm, M['walnut_d'], smooth=40, recalc=True)
    bm = bmesh.new()
    g_box(bm, (0.0, -W / 2 - 0.006, 0.0), (0.030, W / 2 + 0.006, H + 0.006))
    _obj('fuge', bm, M['black'], smooth=None)
    bm = bmesh.new()
    g_box(bm, (0.0, -W / 2, 0.003), (XL, W / 2, H), ch=0.003)
    _obj('blatt', bm, wood, smooth=30)
    bp, bmo = bmesh.new(), bmesh.new()
    for (y0, z0, y1, z1) in DOOR_PANELS[style]:
        _raised_panel(bp, bmo, XL, y0, z0, y1, z1, bevel=0.035 if (y1 - y0) > 0.4 else 0.03)
    _obj('fuellungen', bp, M['walnut_l'] if wood is M['walnut'] else common.material('mahagoni_hell', (0.62, 0.32, 0.22), look='wood', tex=WOOD, rough=0.4, r=0.55), smooth=25)
    _obj('kehlleisten', bmo, M['walnut_d'], smooth=45)
    # Beschlaege: Langschild, Druecker, Schluesselloch (Schlossseite -y), Baender (+y)
    yk = -W / 2 + 0.065
    bm = bmesh.new()
    g_plate(bm, _rrect(yk, 0.98, 0.024, 0.13, 0.022, 4), XL, XL + 0.004)
    g_lathe(bm, [(0.0, 0.0), (0.021, 0.0), (0.021, 0.004), (0.017, 0.008), (0.011, 0.010), (0.009, 0.020), (0.0, 0.022)], seg=16,
            M=T(XL + 0.004, yk, 1.05) @ R(math.pi / 2, 'Y'))
    lever = [Vector((XL + 0.024, yk, 1.05)), Vector((XL + 0.052, yk + 0.006, 1.05)), Vector((XL + 0.062, yk + 0.03, 1.048)),
             Vector((XL + 0.064, yk + 0.07, 1.044)), Vector((XL + 0.060, yk + 0.105, 1.04)), Vector((XL + 0.054, yk + 0.122, 1.038))]
    bl = bmesh.new()
    g_tube(bl, lever, 0.0085, seg=10, scales=[1.0, 1.0, 0.95, 0.92, 0.98, 1.08])
    _obj('druecker', bl, M['brass_w'], smooth=50)
    for z in (0.32, 1.06, 1.86):
        g_lathe(bm, [(0.0, 0.0), (0.0055, 0.002), (0.0075, 0.006), (0.0075, 0.094), (0.0055, 0.098), (0.0, 0.100)], seg=10,
                M=T(XL + 0.004, W / 2 + 0.004, z))
        g_box(bm, (XL - 0.001, W / 2 - 0.034, z + 0.006), (XL + 0.001, W / 2, z + 0.094))
    if v in (1, 3):  # Trittblech
        g_box(bm, (XL, -W / 2 + 0.04, 0.02), (XL + 0.0025, W / 2 - 0.04, 0.24), ch=0.001)
    _obj('beschlag', bm, M['brass'], smooth=40)
    bm = bmesh.new()  # Schluesselloch (dunkel) und angelaufene Kanten des Schilds
    g_lathe(bm, [(0.0, 0.0), (0.0045, 0.0), (0.0045, 0.0007), (0.0, 0.0007)], seg=12, M=T(XL + 0.004, yk, 0.92) @ R(math.pi / 2, 'Y'))
    g_prism(bm, [(-0.0016, 0.0), (0.0016, 0.0), (0.0026, 0.016), (-0.0026, 0.016)], 0.0, 0.0007,
            M=T(XL + 0.004, yk, 0.918) @ R(math.pi / 2, 'Y') @ R(-math.pi / 2, 'Z'))
    _obj('schluesselloch', bm, M['black'], smooth=None)
    # abgegriffen: Holz um das Schild heller
    bm = bmesh.new()
    ring = [bm.verts.new((XL + 0.0004, yk + math.cos(TAU * i / 14) * 0.055, 1.0 + math.sin(TAU * i / 14) * 0.2)) for i in range(14)]
    bm.faces.new(ring)
    _obj('abrieb', bm, common.material('holz_abgegriffen', (0.52, 0.35, 0.24), look='wood', tex=WOOD, rough=0.35, r=0.45), smooth=None)
    # Nummernschild: Form je Bauart (oval / Rechteck mit Fase / Schild mit Spitzen)
    zc = 1.66
    XP = XL + 0.0115  # vor der Fuellung (Plateau XL + 0,010)
    nd = len(str(number))
    hw = 0.034 + 0.018 * nd
    bm = bmesh.new()
    if style in ('a', 'd'):
        g_lathe(bm, [(0.0, 0.0), (0.050, 0.0), (0.052, 0.002), (0.049, 0.005), (0.0, 0.0055)], seg=28,
                M=T(XP, 0, zc) @ R(math.pi / 2, 'Y') @ S(1.0, hw / 0.05, 1.0))
    elif style in ('b', 'e'):
        g_plate(bm, _rrect(0.0, zc, hw, 0.050, 0.008, 2), XP, XP + 0.004)
    else:
        outline = [(-hw, zc - 0.04), (hw, zc - 0.04), (hw + 0.018, zc), (hw, zc + 0.04), (-hw, zc + 0.04), (-hw - 0.018, zc)]
        g_plate(bm, outline, XP, XP + 0.004)
    for s in (-1, 1):  # zwei Schraeubchen
        g_lathe(bm, [(0.0, 0.0), (0.0035, 0.0), (0.0032, 0.0012), (0.002, 0.0018), (0.0, 0.002)], seg=8,
                M=T(XP + 0.0045, s * (hw - 0.008 + (0.012 if style in ('c', 'f') else 0.0)), zc) @ R(math.pi / 2, 'Y'))
    _obj('nummernschild', bm, M['brass'], smooth=40, recalc=True)
    _text_clean('nummer', str(number), 0.058, M['black'], Vector((XP + (0.0055 if style in ('a', 'd') else 0.004), 0.0, zc - 0.002)), depth=0.0008, res=6)
    if v == 2:  # Haengeschild am Druecker
        card = common.material('karte', (0.86, 0.82, 0.70), look='paint', rough=0.8, r=1.2)
        red = common.material('karte_rot', (0.50, 0.08, 0.06), look='paint', rough=0.7, r=1.0)
        bm = bmesh.new()
        g_box(bm, (XL + 0.074, yk + 0.06, 0.82), (XL + 0.076, yk + 0.14, 1.0), M=None)
        _obj('karte', bm, card, smooth=None)
        bm = bmesh.new()
        g_tube(bm, [(XL + 0.066, yk + 0.08, 1.04), (XL + 0.075, yk + 0.08, 1.0)], 0.0008, seg=4)
        g_tube(bm, [(XL + 0.066, yk + 0.12, 1.04), (XL + 0.075, yk + 0.12, 1.0)], 0.0008, seg=4)
        _obj('schnur', bm, red, smooth=None)
        _text_clean('dnd1', 'DO NOT', 0.016, red, Vector((XL + 0.0765, yk + 0.10, 0.94)), depth=0.0004, res=2)
        _text_clean('dnd2', 'DISTURB', 0.014, red, Vector((XL + 0.0765, yk + 0.10, 0.915)), depth=0.0004, res=2)
    if v == 3:  # Schluessel im Schloss
        bm = bmesh.new()
        g_tube(bm, [(XL + 0.004, yk, 0.912), (XL + 0.028, yk, 0.912)], 0.0022, seg=6)
        g_lathe(bm, [(0.0, 0.0), (0.011, 0.0), (0.012, 0.003), (0.011, 0.006), (0.0, 0.006)], seg=12,
                M=T(XL + 0.032, yk, 0.912) @ R(math.pi / 2, 'X'))
        _obj('schluessel', bm, M['brass_d'], smooth=40, recalc=True)


def build_hotel_door_a():
    _hotel_door('a', 0)


def build_hotel_door_b():
    _hotel_door('b', 0)


def build_hotel_door_c():
    _hotel_door('c', 0)


def build_hotel_door_d():
    _hotel_door('d', 0)


def build_hotel_door_e():
    _hotel_door('e', 0)


def build_hotel_door_f():
    _hotel_door('f', 0)


def _make_door_builders():
    g = globals()
    for st in 'abcdef':
        for v in range(1, 4):
            name = f'build_hotel_door_{st}__v{v}'
            g[name] = (lambda s_, v_: (lambda: _hotel_door(s_, v_)))(st, v)


_make_door_builders()


def build_placard_beverly():
    """Schild "The Beverly Room" aus Neusilber neben den Eingaengen (Wandteil, Mitte im Ursprung, 0,52 x 0,12 m):
    Platte mit Fase, erhabener Randlinie, gravierter und schwarz ausgelegter Schrift, vier Linsenkopfschrauben."""
    M = _m()
    bm = bmesh.new()
    g_box(bm, (0.0, -0.26, -0.06), (0.006, 0.26, 0.06), ch=0.0025)
    _obj('platte', bm, M['silver'], smooth=30)
    bm = bmesh.new()
    g_mold_rect(bm, 0.006, -0.245, -0.046, 0.245, 0.046, [(-0.0015, 0.0), (0.0, 0.0012), (0.0015, 0.0)], r=0.006)
    for (y, z) in ((-0.235, -0.036), (0.235, -0.036), (-0.235, 0.036), (0.235, 0.036)):
        g_lathe(bm, [(0.0, 0.0), (0.0042, 0.0), (0.004, 0.0012), (0.0026, 0.002), (0.0, 0.0023)], seg=10, M=T(0.006, y, z) @ R(math.pi / 2, 'Y'))
    _obj('rand_schrauben', bm, M['silver'], smooth=40)
    bm = bmesh.new()
    for (y, z) in ((-0.235, -0.036), (0.235, -0.036), (-0.235, 0.036), (0.235, 0.036)):
        g_box(bm, (0.0082, y - 0.0032, z - 0.0005), (0.0084, y + 0.0032, z + 0.0005), M=None)
    _obj('schlitze', bm, M['black'], smooth=None)
    _text_clean('schrift', 'The Beverly Room', 0.050, M['black'], Vector((0.0062, 0.0, 0.0)), depth=0.0006, res=3)


# --- Bildnisse (3 Varianten: Herr, Dame, alter Mann) -----------------------------------------------------------------------
FRAME_GILT = [(-0.003, 0.004), (0.0, 0.012), (0.004, 0.017), (0.008, 0.017), (0.011, 0.021), (0.016, 0.028), (0.024, 0.035),
              (0.034, 0.041), (0.044, 0.046), (0.052, 0.050), (0.060, 0.051), (0.067, 0.048), (0.071, 0.043), (0.075, 0.044),
              (0.080, 0.045), (0.085, 0.041), (0.088, 0.034), (0.090, 0.022), (0.090, 0.0)]
FRAME_WOOD = [(-0.003, 0.004), (0.0, 0.010), (0.006, 0.014), (0.012, 0.016), (0.030, 0.020), (0.046, 0.026), (0.056, 0.032),
              (0.064, 0.034), (0.070, 0.030), (0.072, 0.024), (0.074, 0.0)]


def _poly(bm, pts, layer, x0=0.0142):
    """Gemalte Flaeche auf der Leinwand (Umriss gegen den Uhrzeigersinn von +x; Faecher aus der Mitte fuer konkave)."""
    c = Vector((x0 + layer * 0.00025, sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)))
    vc = bm.verts.new(c)
    vs = [bm.verts.new((c.x, y, z)) for (y, z) in pts]
    n = len(vs)
    for i in range(n):
        f = bm.faces.new((vc, vs[i], vs[(i + 1) % n]))
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()


def _oval(cy, cz, ry, rz, n=28, squeeze=0.0, a0=0.0):
    out = []
    for k in range(n):
        a = a0 + TAU * k / n
        r = 1.0 - squeeze * max(0.0, -math.sin(a))
        out.append((cy + math.cos(a) * ry * r, cz + math.sin(a) * rz))
    return out


def _portrait(v):
    """Altes Bildnis (Wandteil, Mitte im Ursprung, 0,88 x 1,08 m): Rahmen mit Profil und Gehrung (v0/v1 Blattgold mit
    Eckornamenten, v2 Nussbaum), Leinwand auf Keilrahmen, Bildnis in Schichten (Grund mit Schein, Gewand mit Falten,
    Gesicht mit Schatten, Augen mit Glanzlicht - man fuehlt sich beobachtet, Wiki), nachgedunkelter Firnis am Rand,
    Messingschildchen."""
    M = _m()
    W, H = 0.70, 0.90
    gilt = v in (0, 1)
    bm = bmesh.new()
    g_sweep(bm, FRAME_GILT if gilt else FRAME_WOOD, [(0.006, y, z) for (y, z) in [(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, H / 2), (-W / 2, H / 2)]],
            closed=True, up=(1, 0, 0), pclosed=False)
    fw = 0.09 if gilt else 0.074
    g_box(bm, (0.0, -W / 2 - fw, -H / 2 - fw), (0.006, W / 2 + fw, H / 2 + fw))
    _obj('rahmen', bm, M['gilt'] if gilt else M['walnut_d'], smooth=35)
    if gilt:
        bm = bmesh.new()  # Eckornamente (Rosette mit Akanthus) und Muschel oben
        for (sy, sz) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            c = Vector((0.046, sy * (W / 2 + 0.05), sz * (H / 2 + 0.05)))
            g_lathe(bm, [(0.0, 0.0), (0.030, 0.0), (0.028, 0.006), (0.020, 0.012), (0.010, 0.016), (0.0, 0.017)], seg=10,
                    M=T(*c) @ R(math.pi / 2, 'Y') @ S(1.0, 1.0, 1.0))
            for k in range(5):  # Akanthusblaetter
                a = math.atan2(-sz, -sy) + (k - 2) * 0.42
                tip = c + Vector((0.004, math.cos(a) * 0.055, math.sin(a) * 0.055))
                g_tube(bm, [c + Vector((0.008, 0, 0)), c.lerp(tip, 0.55) + Vector((0.012, 0, 0)), tip], 0.006, seg=5, scales=[1.0, 0.8, 0.25])
        g_lathe(bm, [(0.0, 0.0), (0.05, 0.0), (0.045, 0.008), (0.03, 0.016), (0.0, 0.019)], seg=14,
                M=T(0.046, 0.0, H / 2 + 0.05) @ R(math.pi / 2, 'Y') @ S(0.7, 1.0, 1.0))
        _obj('ornamente', bm, M['gilt'], smooth=45)
    bm = bmesh.new()
    g_box(bm, (0.006, -W / 2 - 0.004, -H / 2 - 0.004), (0.014, W / 2 + 0.004, H / 2 + 0.004))
    grounds = [(0.09, 0.07, 0.05), (0.10, 0.09, 0.07), (0.06, 0.06, 0.05)]
    _obj('leinwand', bm, common.material(f'grund{v}', grounds[v], look='paint', rough=0.35, r=0.55), smooth=None)
    P = {}

    def mat(name, rgb, r=0.6):
        if name not in P:
            P[name] = (common.material(f'{name}{v}', rgb, look='paint', rough=0.5, r=r), bmesh.new())
        return P[name][1]
    # Firnis am Rand nachgedunkelt (Ring) und Schein hinter dem Kopf
    dark = mat('firnis', tuple(c * 0.55 for c in grounds[v]))
    ring_o = [(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, H / 2), (-W / 2, H / 2)]
    for i in range(4):
        a, b = ring_o[i], ring_o[(i + 1) % 4]
        ia = (a[0] * 0.86, a[1] * 0.88)
        ib = (b[0] * 0.86, b[1] * 0.88)
        _poly(dark, [a, b, ib, ia], 0)
    halo_c = [(0.22, 0.16, 0.10), (0.24, 0.20, 0.14), (0.16, 0.14, 0.11)][v]
    g0 = grounds[v]
    for k in range(6):  # weicher Schein hinter dem Kopf (sechs flache Stufen)
        t = (k + 1) / 6
        col = tuple(g0[i] + (halo_c[i] * 0.85 - g0[i]) * t for i in range(3))
        _poly(mat(f'schein{k}', col), _oval(0.0, 0.10 + 0.02 * t, 0.30 - 0.15 * t, 0.36 - 0.17 * t, n=32), 1 + k * 0.15)
    skin = (0.62, 0.46, 0.34) if v != 2 else (0.58, 0.44, 0.36)
    shade = tuple(c * 0.72 for c in skin)
    if v == 1:  # Dame: dunkles Kleid mit V-Ausschnitt, Perlenkette, Bubikopf
        dress = mat('kleid', (0.10, 0.12, 0.16))
        sh = [(-0.33, -0.45)] + [(-0.33 + 0.2 * k / 8, -0.24 + 0.1 * math.sin(math.pi / 2 * k / 8)) for k in range(9)] + \
             [(-0.05, -0.12), (0.0, -0.22), (0.05, -0.12)] + [(0.33 - 0.2 * (1 - k / 8), -0.24 + 0.1 * math.sin(math.pi / 2 * (1 - k / 8))) for k in range(9)] + [(0.33, -0.45)]
        _poly(dress, sh, 3)
        _poly(mat('haut', skin), [(-0.05, -0.12), (-0.04, 0.03), (0.04, 0.03), (0.05, -0.12), (0.0, -0.21)], 4)
        pearl = mat('perlen', (0.85, 0.83, 0.76), 0.3)
        for k in range(15):
            a = math.pi * (0.15 + 0.7 * k / 14)
            _poly(pearl, _oval(math.cos(a) * 0.06, -0.075 - math.sin(a) * 0.045, 0.006, 0.006, n=8), 5)
        _poly(mat('gesicht', skin), _oval(0.0, 0.12, 0.078, 0.112, squeeze=0.3), 6)
        hair = mat('haar', (0.07, 0.05, 0.04))
        _poly(hair, [(-0.10, 0.06), (-0.11, 0.14), (-0.09, 0.21), (-0.04, 0.245), (0.03, 0.247), (0.09, 0.22), (0.11, 0.15), (0.10, 0.06),
                     (0.08, 0.10), (0.06, 0.18), (0.0, 0.19), (-0.06, 0.18), (-0.08, 0.10)], 7)
        lip = mat('lippe', (0.55, 0.14, 0.13))
        _poly(lip, _oval(0.0, 0.058, 0.02, 0.007, n=12), 8)
    else:  # Herr (v0) bzw. alter Mann mit Bart (v2): Rock mit Revers, Kragen, Krawatte
        coat = mat('rock', (0.05, 0.05, 0.06) if v == 0 else (0.08, 0.07, 0.06))
        sh = [(-0.33, -0.45)] + [(-0.33 + 0.2 * k / 8, -0.2 + 0.12 * math.sin(math.pi / 2 * k / 8)) for k in range(9)] + \
             [(-0.06, -0.06), (0.06, -0.06)] + [(0.33 - 0.2 * (1 - k / 8), -0.2 + 0.12 * math.sin(math.pi / 2 * (1 - k / 8))) for k in range(9)] + [(0.33, -0.45)]
        _poly(coat, sh, 3)
        lap = mat('revers', (0.09, 0.09, 0.10) if v == 0 else (0.12, 0.10, 0.08))
        for s in (-1, 1):
            _poly(lap, [(s * 0.07, -0.05), (s * 0.13, -0.10), (s * 0.06, -0.30), (s * 0.02, -0.22)][::(1 if s < 0 else -1)], 4)
        _poly(mat('hals', skin), [(-0.045, -0.06), (0.045, -0.06), (0.05, 0.03), (-0.05, 0.03)], 4)
        _poly(mat('kragen', (0.76, 0.72, 0.64)), [(-0.07, -0.05), (0.07, -0.05), (0.0, -0.17)], 5)
        _poly(mat('krawatte', (0.30, 0.06, 0.06) if v == 0 else (0.10, 0.14, 0.22)),
              [(-0.016, -0.07), (0.016, -0.07), (0.011, -0.18), (0.0, -0.20), (-0.011, -0.18)], 6)
        _poly(mat('gesicht', skin), _oval(0.0, 0.12, 0.085, 0.12, squeeze=0.25), 6)
        hair = mat('haar', (0.12, 0.08, 0.05) if v == 0 else (0.62, 0.60, 0.56))
        if v == 0:
            _poly(hair, [(-0.09, 0.13), (-0.088, 0.19), (-0.06, 0.235), (0.0, 0.25), (0.06, 0.235), (0.088, 0.19), (0.09, 0.13),
                         (0.07, 0.17), (0.0, 0.2), (-0.07, 0.17)], 7)
        else:
            for s in (-1, 1):
                _poly(hair, [(s * 0.088, 0.10), (s * 0.095, 0.17), (s * 0.07, 0.19), (s * 0.075, 0.12)][::(1 if s > 0 else -1)], 7)
            _poly(hair, [(-0.075, 0.08), (-0.06, 0.02), (-0.03, -0.02), (0.0, -0.035), (0.03, -0.02), (0.06, 0.02), (0.075, 0.08),
                         (0.04, 0.05), (0.0, 0.045), (-0.04, 0.05)], 8)
        _poly(mat('mund', (0.42, 0.24, 0.20)), [(-0.022, 0.06), (0.022, 0.06), (0.022, 0.054), (-0.022, 0.054)], 9)
    # Gesichtsschatten (Wange, Nase, Augenhoehlen), Augen mit Iris und Glanzlicht
    sb = mat('schatten', shade)
    _poly(sb, [(0.035, 0.18), (0.07, 0.14), (0.075, 0.07), (0.05, 0.03), (0.06, 0.10)], 9)
    _poly(sb, [(0.004, 0.115), (0.016, 0.082), (0.004, 0.076)], 9)
    for sg in (-1, 1):
        _poly(sb, _oval(sg * 0.032, 0.127, 0.022, 0.011, n=12), 9)
        _poly(mat('weiss', (0.78, 0.74, 0.66)), _oval(sg * 0.032, 0.126, 0.013, 0.0055, n=12), 10)
        _poly(mat('iris', (0.20, 0.14, 0.08) if v != 1 else (0.18, 0.24, 0.30)), _oval(sg * 0.032 - 0.002, 0.126, 0.0055, 0.0055, n=10), 11)
        _poly(mat('pupille', (0.02, 0.02, 0.02)), _oval(sg * 0.032 - 0.002, 0.126, 0.0026, 0.0026, n=8), 12)
        _poly(mat('glanz', (0.95, 0.93, 0.88), 0.3), _oval(sg * 0.032 - 0.0005, 0.1275, 0.0012, 0.0012, n=6), 13)
        _poly(mat('braue', (0.12, 0.08, 0.05) if v != 2 else (0.55, 0.53, 0.50)),
              [(sg * 0.014, 0.143), (sg * 0.050, 0.147), (sg * 0.050, 0.141), (sg * 0.014, 0.139)][::(1 if sg < 0 else -1)], 10)
    # Lichter: Stirn, Nasenruecken, Schulter; Ohren; Faltenschatten im Gewand
    hl = mat('licht', tuple(min(1.0, c * 1.08) for c in skin))
    _poly(hl, _oval(-0.016, 0.163, 0.028, 0.012, n=12), 6.4)
    _poly(hl, [(-0.006, 0.12), (0.0, 0.12), (0.002, 0.085), (-0.006, 0.088)], 6.4)
    for sg in (-1, 1):
        _poly(mat('haut', skin), _oval(sg * 0.087, 0.118, 0.010, 0.022, n=10), 5.5)
    cloth_d = mat('falte', (0.02, 0.02, 0.025))
    for (y0, z0, y1, z1) in ((-0.22, -0.25, -0.16, -0.42), (0.20, -0.24, 0.15, -0.43), (-0.12, -0.30, -0.10, -0.44), (0.27, -0.32, 0.30, -0.44)):
        _poly(cloth_d, [(y0, z0), (y0 + 0.012, z0), (y1 + 0.008, z1), (y1 - 0.006, z1)], 7)
    _poly(mat('schulterlicht', (0.16, 0.15, 0.15) if v != 1 else (0.20, 0.22, 0.28)),
          [(-0.30, -0.19), (-0.22, -0.13), (-0.16, -0.115), (-0.22, -0.15)], 7)
    # Craquelé: feine dunklere Risslinien im Firnis
    rng = random.Random(70 + v)
    crack = mat('craquele', tuple(c * 0.6 for c in grounds[v]))
    for k in range(26):
        y, z = rng.uniform(-W / 2 + 0.02, W / 2 - 0.02), rng.uniform(-H / 2 + 0.02, H / 2 - 0.02)
        a = rng.uniform(0, math.pi)
        L = rng.uniform(0.02, 0.06)
        dy, dz = math.cos(a) * L, math.sin(a) * L
        ny_, nz_ = -math.sin(a) * 0.0006, math.cos(a) * 0.0006
        _poly(crack, [(y - ny_, z - nz_), (y + dy - ny_, z + dz - nz_), (y + dy + ny_, z + dz + nz_), (y + ny_, z + nz_)], 14)
    for name, (m_, b_) in P.items():
        _obj(name, b_, m_, smooth=None)
    # Messingschildchen unten am Rahmen
    bm = bmesh.new()
    g_box(bm, (0.050 if gilt else 0.034, -0.06, -H / 2 - 0.068), (0.054 if gilt else 0.038, 0.06, -H / 2 - 0.042), ch=0.0012)
    _obj('schildchen', bm, M['brass_d'], smooth=None)


def build_painting_portrait():
    _portrait(0)


def build_painting_portrait__v1():
    _portrait(1)


def build_painting_portrait__v2():
    _portrait(2)


# --- Helles Fenster (nach Level 188) -------------------------------------------------------------------------------------
def build_window_bright():
    """Schiebefenster (Wandteil, Mitte im Ursprung, Lichtoeffnung 1,2 x 1,6 m) in tiefer Laibung mit hellem, weissem
    Licht dahinter - der Weg nach Level 188: Blendrahmen, zwei Fluegel mit Sprossen (je 6 Scheiben, leicht
    unterschiedlich spiegelnd), Kaempfer mit Riegel, Hebegriffe, Fensterbank mit Profil, Bekleidung, Samtvorhaenge
    seitlich gerafft."""
    M = _m()
    light = common.material('aussen', (1.0, 1.0, 0.98), look='glow', glow=2.2)
    frame = common.material('fensterlack', (0.84, 0.80, 0.70), look='paint', rough=0.5, r=0.85)
    W, H = 1.2, 1.6
    bm = bmesh.new()
    g_box(bm, (-0.31, -W / 2, -H / 2), (-0.30, W / 2, H / 2))
    _obj('licht', bm, light, smooth=None)
    # Laibung (Wandputz) und Blendrahmen
    bm = bmesh.new()
    for (lo, hi) in (((-0.30, -W / 2 - 0.02, -H / 2 - 0.02), (0.0, -W / 2, H / 2 + 0.02)), ((-0.30, W / 2, -H / 2 - 0.02), (0.0, W / 2 + 0.02, H / 2 + 0.02)),
                     ((-0.30, -W / 2, H / 2), (0.0, W / 2, H / 2 + 0.02)), ((-0.30, -W / 2, -H / 2 - 0.02), (0.0, W / 2, -H / 2))):
        g_box(bm, lo, hi)
    _obj('laibung', bm, M['plaster'], smooth=None)
    bm = bmesh.new()
    t = 0.05
    for (lo, hi) in (((-0.20, -W / 2, -H / 2), (-0.14, -W / 2 + t, H / 2)), ((-0.20, W / 2 - t, -H / 2), (-0.14, W / 2, H / 2)),
                     ((-0.20, -W / 2, H / 2 - t), (-0.14, W / 2, H / 2)), ((-0.20, -W / 2, -H / 2), (-0.14, W / 2, -H / 2 + t))):
        g_box(bm, lo, hi, ch=0.004)
    # zwei Fluegel (oberer aussen), je 3 x 2 Scheiben
    glass_l = []
    for k, (x0, z0, z1) in enumerate(((-0.19, 0.0, H / 2 - t), (-0.16, -H / 2 + t, 0.03))):
        y0, y1 = -W / 2 + t, W / 2 - t
        st = 0.045
        for (lo, hi) in (((x0, y0, z0), (x0 + 0.035, y0 + st, z1)), ((x0, y1 - st, z0), (x0 + 0.035, y1, z1)),
                         ((x0, y0, z1 - st), (x0 + 0.035, y1, z1)), ((x0, y0, z0), (x0 + 0.035, y1, z0 + st))):
            g_box(bm, lo, hi, ch=0.003)
        for j in (1, 2):
            yy = y0 + st + (y1 - y0 - 2 * st) * j / 3
            g_box(bm, (x0 + 0.006, yy - 0.011, z0 + st), (x0 + 0.03, yy + 0.011, z1 - st), ch=0.004)
        zm = (z0 + z1) / 2
        g_box(bm, (x0 + 0.006, y0 + st, zm - 0.011), (x0 + 0.03, y1 - st, zm + 0.011), ch=0.004)
        glass_l.append((x0 + 0.017, y0 + st, z0 + st, y1 - st, z1 - st))
    _obj('rahmen', bm, frame, smooth=30)
    bm = bmesh.new()
    rng = random.Random(188)
    gm = [common.material(f'scheibe{k}', (0.72, 0.76, 0.76), look='glass', rough=0.04 + 0.03 * k, r=0.1 + 0.08 * k) for k in range(3)]
    bms = [bmesh.new() for _ in gm]
    for (x, y0, z0, y1, z1) in glass_l:
        for j in range(3):
            for i in range(2):
                ya, yb = y0 + (y1 - y0) * j / 3, y0 + (y1 - y0) * (j + 1) / 3
                za, zb = z0 + (z1 - z0) * i / 2, z0 + (z1 - z0) * (i + 1) / 2
                g_quad(bms[rng.randrange(3)], (x, ya, za), (x, yb, za), (x, yb, zb), (x, ya, zb))
    bm.free()
    for k, b_ in enumerate(bms):
        _obj(f'glas{k}', b_, gm[k], smooth=None)
    # Riegel und Hebegriffe (Messing)
    bm = bmesh.new()
    g_box(bm, (-0.125, -0.05, 0.03), (-0.115, 0.05, 0.045), ch=0.002)
    g_lathe(bm, [(0.0, 0.0), (0.012, 0.0), (0.012, 0.01), (0.0, 0.012)], seg=12, M=T(-0.115, 0.0, 0.045))
    for s in (-1, 1):
        g_tube(bm, [(-0.125, s * 0.30, -0.62), (-0.105, s * 0.30, -0.62), (-0.105, s * 0.30, -0.56), (-0.125, s * 0.30, -0.56)], 0.004, seg=6)
    _obj('beschlag', bm, M['brass'], smooth=40)
    # Fensterbank, Schuerze, Bekleidung
    bm = bmesh.new()
    sill = [(-0.02, 0.0), (0.0, 0.0), (0.0, -0.004), (0.12, -0.004), (0.13, 0.0), (0.135, 0.008), (0.13, 0.016), (0.12, 0.02),
            (0.0, 0.022), (-0.02, 0.022)]
    g_sweep(bm, [(x, z) for (x, z) in sill], [(0.0, -W / 2 - 0.1, -H / 2 - 0.022), (0.0, W / 2 + 0.1, -H / 2 - 0.022)], up=(0, 0, 1))
    _obj('fensterbank', bm, M['walnut'], smooth=40, recalc=True)
    bm = bmesh.new()
    g_sweep(bm, CASING, [(0.0, W / 2 + 0.006, -H / 2 - 0.02), (0.0, W / 2 + 0.006, H / 2 + 0.006), (0.0, -W / 2 - 0.006, H / 2 + 0.006),
                         (0.0, -W / 2 - 0.006, -H / 2 - 0.02)], up=(1, 0, 0), pclosed=False)
    g_box(bm, (0.0, -W / 2 - 0.05, -H / 2 - 0.13), (0.02, W / 2 + 0.05, -H / 2 - 0.022), ch=0.003)
    _obj('bekleidung', bm, frame, smooth=40)
    # Vorhaenge: Samt, oben in Falten, in der Mitte gerafft (Raffhalter)
    F = _fab(0)
    for s in (-1, 1):
        bm = bmesh.new()
        nz, ny = 18, 14
        grid = []
        for i in range(nz + 1):
            z = H / 2 + 0.08 - (H + 0.06) * i / nz
            u = (z - (-0.15)) / 0.9
            pinch = math.exp(-((z + 0.15) / 0.22) ** 2)
            width = 0.17 * (1 - 0.55 * pinch)
            row = []
            for j in range(ny + 1):
                tj = j / ny
                y = s * (W / 2 + 0.12 - width * tj)
                x = 0.04 + 0.022 * (0.5 + 0.5 * math.sin(tj * TAU * 2.5 + s)) * (1 - 0.6 * pinch) + 0.01 * pinch
                row.append(bm.verts.new((x, y, z)))
            grid.append(row)
            del u
        for i in range(nz):
            for j in range(ny):
                q = (grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j])
                f = bm.faces.new(q if s > 0 else q[::-1])
                f.normal_update()
                if f.normal.x < 0:
                    f.normal_flip()
        ob = _obj(f'vorhang{s}', bm, F['fab'], smooth=60)
        sol = ob.modifiers.new('dicke', 'SOLIDIFY')
        sol.thickness = 0.006
        bm = bmesh.new()
        g_tube(bm, [(0.05, s * (W / 2 + 0.03), -0.15), (0.075, s * (W / 2 + 0.07), -0.17), (0.06, s * (W / 2 + 0.14), -0.16),
                    (0.02, s * (W / 2 + 0.15), -0.15)], 0.009, seg=6)
        _obj(f'raffhalter{s}', bm, common.material('kordel', (0.70, 0.54, 0.24), look='fabric', rough=0.6, r=0.8), smooth=60)
    bm = bmesh.new()  # Vorhangstange mit Endknaeufen
    g_tube(bm, [(0.06, -W / 2 - 0.13, H / 2 + 0.10), (0.06, W / 2 + 0.13, H / 2 + 0.10)], 0.012, seg=10)
    for s in (-1, 1):
        g_lathe(bm, [(0.0, 0.0), (0.016, 0.0), (0.022, 0.02), (0.016, 0.04), (0.0, 0.045)], seg=12,
                M=T(0.06, s * (W / 2 + 0.13), H / 2 + 0.10) @ R(-s * math.pi / 2, 'X'))
        g_tube(bm, [(0.0, s * (W / 2 + 0.11), H / 2 + 0.10), (0.06, s * (W / 2 + 0.11), H / 2 + 0.10)], 0.006, seg=6)
    _obj('stange', bm, M['brass_d'], smooth=40)
