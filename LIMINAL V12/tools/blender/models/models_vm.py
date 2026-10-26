"""V12 Viewmodel: Taschenlampe und linke Hand im Faustgriff (eigene Modelle, keine fremden Assets).

Rahmen der Lampe wie im Spiel (Body::flashGrip): Ursprung in der Griffmitte auf der Achse, +x zum Glas, z oben,
+y links (Blender; das Spiel spiegelt y beim Laden). Schaft r = 16,5 mm von x = -0,050 bis 0,080, Glas bei
x = 0,170 - wie das Code-Modell aus V7, damit Lichtquelle und Halt gleich bleiben.

Die Hand wird in ihrem eigenen Rahmen gebaut (Handgelenk im Ursprung, Finger +x, Handruecken +z, Daumen -y),
um die schraeg in der Hohlhand liegende Lampe geschlossen (Kraftgriff: die Lampe laeuft vom Kleinfingerballen zur
Daumen-Zeigefinger-Falte, der Daumen liegt oben am Schaft) und dann in den Rahmen der Lampe gedreht. Die
Fingerwinkel sucht ein kleiner Loeser so, dass Mittel- und Endglieder am Schaft anliegen, ohne ihn zu
durchdringen. Die Form entsteht aus Ellipsoiden, die ein Voxel-Remesh vereinigt und eine Glaettung verschmilzt.

Im Spiel haengt die Hand am Rahmen der Lampe; der Unterarm des Koerpers fuehrt zum Handgelenk (HAND_WRIST,
HAND_FOREARM werden beim Bau ausgegeben und stehen in gameplay/body.cpp).
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

RB = 0.0165       # Schaftradius
LENS_X = 0.170    # Glas
SKIN = (0.80, 0.62, 0.52)

# --- Taschenlampe -----------------------------------------------------------------------------------------


def _lathe(name, prof, mat, seg=48, smooth_deg=40.0):
    """Drehkoerper um x aus (x, r)-Punkten (r = 0 schliesst die Achse)."""
    bm = bmesh.new()
    rings = []
    for (x, r) in prof:
        ring = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            ring.append(bm.verts.new((x, r * math.cos(a), r * math.sin(a))))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for k in range(seg):
            j = (k + 1) % seg
            try:
                bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
            except ValueError:
                pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh(name, bm, mat)
    common.shade_auto(ob, smooth_deg)
    return ob


def _flashlight(lit):
    body = common.material('gehaeuse', (0.10, 0.10, 0.11), look='metal', rough=0.45, fm='flash_body')
    bezel = common.material('ring', (0.62, 0.62, 0.60), look='metal', rough=0.3, metal=1.0, fm='flash_bezel')
    rubber = common.material('gummi', (0.05, 0.05, 0.05), look='rubber', rough=0.9, fm='flash_rubber')
    if lit:
        glass = common.material('glas_an', (1.0, 0.95, 0.85), look='glow', glow=7.0, fm='flash_lens')
    else:
        glass = common.material('glas', (0.30, 0.31, 0.32), look='glass', rough=0.1, fm='flash_glass')
    # Endkappe mit Gummiring, Schaft, Hals, Kopf mit Kuehlrillen; Ring um das Glas
    _lathe('endkappe', [(-0.0615, 0.0), (-0.0615, 0.0120), (-0.0605, 0.0158), (-0.0585, 0.0172), (-0.0505, 0.0172),
                        (-0.0500, 0.0166)], body)
    _lathe('gummiring', [(-0.0500, 0.0150), (-0.0495, 0.0170), (-0.0440, 0.0170), (-0.0435, 0.0150)], rubber)
    prof = [(-0.0440, 0.0160), (-0.0430, RB)]
    # Griffzone mit flachen Rillen (Riffelung angedeutet), dann glatt bis zum Hals
    x = -0.040
    while x < 0.050:
        prof += [(x, RB), (x + 0.0035, RB), (x + 0.0045, RB - 0.0006), (x + 0.0055, RB)]
        x += 0.0060
    prof += [(0.0800, RB), (0.0880, 0.0172), (0.1000, 0.0205), (0.1100, 0.0232), (0.1140, 0.0236)]
    x = 0.1150
    for k in range(5):  # Kuehlrillen am Kopf
        prof += [(x, 0.0236), (x + 0.0040, 0.0236), (x + 0.0048, 0.0224), (x + 0.0068, 0.0224), (x + 0.0076, 0.0236)]
        x += 0.0076
    prof += [(0.1600, 0.0236), (0.1605, 0.0230)]
    _lathe('schaft', prof, body, seg=48, smooth_deg=30.0)
    _lathe('ring', [(0.1600, 0.0228), (0.1604, 0.0246), (0.1690, 0.0248), (0.1720, 0.0240), (0.1725, 0.0212),
                    (0.1712, 0.0205)], bezel, smooth_deg=30.0)
    _lathe('glas', [(0.1700, 0.0206), (0.1702, 0.0)], glass, seg=48)
    # Schalter: Gummikappe oben auf dem Schaft vor der Griffzone (unter dem Daumen)
    sw = bmesh.new()
    bmesh.ops.create_uvsphere(sw, u_segments=24, v_segments=12, radius=1.0)
    for v in sw.verts:
        v.co = Vector((0.062 + v.co.x * 0.0075, v.co.y * 0.0060, RB - 0.0008 + max(v.co.z, -0.2) * 0.0032))
    ob = common.obj_from_bmesh('schalter', sw, rubber)
    common.shade_auto(ob, 60.0)
    _lathe('schalterring', [(0.0530, RB), (0.0530, RB + 0.0004), (0.0710, RB + 0.0004), (0.0710, RB)], bezel)


def build_vm_flashlight():
    _flashlight(False)


def build_vm_flashlight_on():
    _flashlight(True)


# --- Hand ----------------------------------------------------------------------------------------------------
# Masse einer erwachsenen Hand (~19 cm lang). Handrahmen: Handgelenk im Ursprung, +x Finger, +z Handruecken,
# -y Daumenseite (linke Hand, Handflaeche unten).
FINGERS = [
    # Name, Grundgelenk, Richtung in der Handebene, Gliedlaengen, Breite (Halbachse) am Grund, Massstab
    # (die Hohlhand ist zur Kleinfingerseite hin gewoelbt: Ring- und Kleinfinger-Grundgelenk tiefer)
    ('zeige', (0.094, -0.0245, 0.000), (1.0, -0.07), (0.044, 0.025, 0.020), 1.00),
    ('mittel', (0.097, -0.0035, -0.001), (1.0, 0.0), (0.049, 0.030, 0.022), 1.03),
    ('ring', (0.091, 0.0160, -0.006), (1.0, 0.06), (0.046, 0.029, 0.021), 0.96),
    ('klein', (0.080, 0.0320, -0.013), (1.0, 0.15), (0.036, 0.021, 0.019), 0.85),
]
BETA = math.radians(56.0)        # Lampe zur Handlaengsachse (Kraftgriff, schraeg in der Hohlhand)
DEV, EXT = math.radians(33.0), math.radians(15.0)  # Elle-Abspreizung und Streckung im Handgelenk
THETA_F = math.radians(178.0)    # Richtung, aus der der Unterarm kommt (um die Lampenachse; 180 = von rechts)


def _barrel():
    a = Vector((math.cos(BETA), -math.sin(BETA), 0.0))
    # unter der Hohlhand: Mitte der Handflaeche minus halbe Handdicke minus Schaftradius (Haut gibt 1,5 mm nach)
    c = Vector((0.064, 0.008, -(0.0125 + RB - 0.0015)))
    return c, a


def _dist_axis(p, c, a):
    d = p - c
    return (d - a * d.dot(a)).length


def _finger_chain(base, fdir, lens, ang, spread=0.0):
    """Gelenkpunkte (Grund, Mittel, End, Spitze) bei Beugewinkeln ang (Grund-, Mittel-, Endgelenk); spread dreht
    den Finger in der Handebene (Spreizen, rad)."""
    u = Vector((fdir[0], fdir[1], 0.0)).normalized()
    u = Vector((u.x * math.cos(spread) - u.y * math.sin(spread), u.x * math.sin(spread) + u.y * math.cos(spread), 0.0))
    n = Vector((0.0, 0.0, 1.0))
    pts = [Vector(base)]
    t = 0.0
    for L, a in zip(lens, ang):
        t += a
        d = u * math.cos(t) - n * math.sin(t)
        pts.append(pts[-1] + d * L)
    return pts


def _radius(k, s, scale):
    """Halbachsen (Breite, Hoehe) eines Fingers an der Stelle s (0 Grund .. 3 Spitze)."""
    w = [0.0100, 0.0092, 0.0082, 0.0071]
    h = [0.0090, 0.0080, 0.0070, 0.0060]
    i = min(int(s), 2)
    f = s - i
    return ((w[i] * (1 - f) + w[i + 1] * f) * scale, (h[i] * (1 - f) + h[i + 1] * f) * scale)


def _solve_finger(base, fdir, lens, scale, c, a):
    """Beugewinkel, bei denen Mittel- und Endglied am Schaft anliegen (Endgelenk = 0,7 x Mittelgelenk)."""
    best = None
    for sp in range(-12, 13, 4):
      for a1d in range(10, 100, 3):
        for a2d in range(30, 112, 3):
            ang = (math.radians(a1d), math.radians(a2d), math.radians(a2d) * 0.7)
            pts = _finger_chain(base, fdir, lens, ang, math.radians(sp))
            cost = 0.0
            for seg in range(3):
                for k in range(1, 6):
                    f = k / 5.0
                    p = pts[seg].lerp(pts[seg + 1], f)
                    w, h = _radius(None, seg + f, scale)
                    want = RB + h
                    d = _dist_axis(p, c, a)
                    pen = want - d
                    if pen > 0:
                        cost += 60.0 * pen * pen
                    elif seg >= 1 or f > 0.3:
                        cost += pen * pen
            # Spitze nicht in die Hohlhand (z > -0.005 waere im Handteller)
            tip = pts[3]
            if tip.z > -0.004:
                cost += 30.0 * (tip.z + 0.004) ** 2
            cost += 2e-6 * abs(sp)  # Spreizen nur, wenn es das Anliegen verbessert
            if best is None or cost < best[0]:
                best = (cost, ang, pts)
    return best


def _ellipsoid(bm, center, ax, ay, az, seg=14, rings=9):
    """Ellipsoid mit Halbachsen-Vektoren ax, ay, az (Spalten einer Matrix)."""
    res = bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=1.0)
    m = Matrix((ax, ay, az)).transposed()
    for v in res['verts']:
        v.co = center + m @ v.co


def _frame_along(t, up):
    side = t.cross(up)
    if side.length < 1e-6:
        side = t.orthogonal()
    side.normalize()
    return side, side.cross(t).normalized()


def _sweep(bm, pts, radii, up, step=0.0015, ring=18):
    """Rundes Glied als durchgehende Roehre mit elliptischem Querschnitt (Breite, Hoehe) entlang der Gelenkpunkte,
    an den Gelenken und Enden je ein Ellipsoid (Gelenkkopf, Kuppe). Ueberschneidungen loest das Remesh."""
    for i in range(len(pts) - 1):
        p0, p1 = pts[i], pts[i + 1]
        d = p1 - p0
        L = d.length
        t = d / L
        side, hgt = _frame_along(t, up)
        n = max(2, int(L / step))
        rings = []
        for k in range(n + 1):
            f = k / n
            w, h = radii(i + f)
            c = p0.lerp(p1, f)
            rings.append([bm.verts.new(c + side * (w * math.cos(2 * math.pi * j / ring)) +
                                       hgt * (h * math.sin(2 * math.pi * j / ring))) for j in range(ring)])
        for k in range(n):
            for j in range(ring):
                jj = (j + 1) % ring
                bm.faces.new((rings[k][j], rings[k][jj], rings[k + 1][jj], rings[k + 1][j]))
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side, hgt = _frame_along(t, up)
        w, h = radii(float(i))
        _ellipsoid(bm, p, t * min(w, h), side * w, hgt * h)


def _hand_parts():
    """Alle Teile der Hand im Handrahmen; Rueckgabe: bmesh (Haut), Liste der Naegel (Mitte, Laenge, Breite,
    Normale, Richtung), Daumenpunkte, Gelenkwinkel."""
    c, a = _barrel()
    bm = bmesh.new()
    Z = Vector((0, 0, 1))
    nails = []
    report = []
    # Handteller: flaches, an den Kanten gerundetes Polster aus Ellipsoiden, darunter die Ballen
    for (cx, cy, rx, ry, rz) in ((0.050, 0.004, 0.046, 0.037, 0.0125), (0.078, 0.004, 0.024, 0.040, 0.012),
                                 (0.022, 0.002, 0.026, 0.031, 0.0135)):
        _ellipsoid(bm, Vector((cx, cy, 0.0)), Vector((rx, 0, 0)), Vector((0, ry, 0)), Vector((0, 0, rz)), seg=20, rings=12)
    # Kleinfingerballen (wickelt sich um den Schaft) und Daumenballen
    _ellipsoid(bm, Vector((0.046, 0.028, -0.009)), Vector((0.038, 0, 0)), Vector((0, 0.013, 0)), Vector((0, 0, 0.013)))
    _ellipsoid(bm, Vector((0.074, 0.026, -0.008)), Vector((0.016, 0, 0)), Vector((0, 0.012, 0)), Vector((0, 0, 0.011)))
    # Handgelenk und Unterarmansatz (steckt im Buendchen des Aermels)
    fore = Vector((math.cos(DEV) * math.cos(EXT), -math.sin(DEV) * math.cos(EXT), -math.sin(EXT)))
    # nur so lang, wie das Buendchen sicher deckt (es endet 1 cm vor dem Handgelenk)
    _sweep(bm, [-fore * 0.024, Vector((0.0, 0.0, 0.0))],
           lambda s: (0.0265 - 0.001 * s, 0.0172 - 0.0002 * s), Z)
    _ellipsoid(bm, Vector((0.004, 0.0, 0.0)), Vector((0.016, 0, 0)), Vector((0, 0.027, 0)), Vector((0, 0, 0.0165)))
    # Fingerknoechel (Koepfe der Mittelhandknochen) und Sehnen auf dem Handruecken
    for (name, base, fdir, lens, sc) in FINGERS:
        b = Vector(base)
        _ellipsoid(bm, b + Vector((-0.002, 0, 0.0035)), Vector((0.0085, 0, 0)), Vector((0, 0.0085 * sc, 0)),
                   Vector((0, 0, 0.0085)))
        _sweep(bm, [Vector((0.012, b.y * 0.55, 0.008)), b + Vector((-0.010, 0, 0.0075))], lambda s: (0.0022, 0.0012), Z)
    # Finger: Winkel loesen, dann als Glieder aufbauen
    for (name, base, fdir, lens, sc) in FINGERS:
        cost, ang, pts = _solve_finger(base, fdir, lens, sc, c, a)
        report.append((name, [round(math.degrees(x)) for x in ang], round(cost * 1e6, 2)))
        _sweep(bm, pts, lambda s, sc=sc: _radius(None, s, sc), Z)
        # Gelenkwuelste (Mittel- und Endgelenk), Nagel auf dem Endglied (Handruecken-Seite des Glieds)
        u = Vector((fdir[0], fdir[1], 0)).normalized()
        side = u.cross(Z).normalized()
        for j in (1, 2):
            t = (pts[j + 1] - pts[j - 1]).normalized()
            hgt = side.cross(t).normalized()
            w, h = _radius(None, j, sc)
            _ellipsoid(bm, pts[j] + hgt * (h * 0.15), t * w * 0.8, side * w * 1.0, hgt * h * 0.9)
        t = (pts[3] - pts[2]).normalized()
        hgt = side.cross(t).normalized()
        w, h = _radius(None, 2.6, sc)
        nails.append((pts[2].lerp(pts[3], 0.60) + hgt * (h * 0.80), t, side, hgt, 0.0115 * sc, w * 0.80))
    # Daumen: liegt oben am Schaft und zeigt zum Glas. Spitze und Endgelenk auf der Schaftflaeche, Grundgelenk
    # ueber dem Schaft am Rand des Handtellers, Sattelgelenk im Daumenballen.
    tside = a.cross(Z).normalized()           # quer zur Lampe, zur Daumenseite (proximal-radial)
    rad = (tside * 0.55 + Z * 0.835).normalized()  # oben auf dem Schaft, zur Hand hin
    s_tip = 0.060
    tip = c + a * s_tip + rad * (RB + 0.0080)
    ip = c + a * (s_tip - 0.027) + rad * (RB + 0.0092)
    mcp = ip - (a * 0.80 + rad * 0.25 - tside * 0.1).normalized() * 0.033
    cmc = Vector((0.020, -0.020, -0.006))
    thumb = [cmc, mcp, ip, tip]
    tr = [(0.0125, 0.0115), (0.0105, 0.0090), (0.0098, 0.0082), (0.0080, 0.0062)]

    def trad(s):
        i = min(int(s), 2)
        f = s - i
        return (tr[i][0] * (1 - f) + tr[i + 1][0] * f, tr[i][1] * (1 - f) + tr[i + 1][1] * f)

    _sweep(bm, thumb, trad, rad)
    # Daumenballen: vom Sattelgelenk zum Grundgelenk, zur Hohlhand hin
    mid = cmc.lerp(mcp, 0.45)
    td = (mcp - cmc).normalized()
    s2 = td.cross(Z).normalized()
    _ellipsoid(bm, mid + Vector((0.004, 0.003, -0.004)), td * 0.030, s2 * 0.016, s2.cross(td).normalized() * 0.015)
    t = (tip - ip).normalized()
    nails.append((ip.lerp(tip, 0.62) + rad * 0.0075, t, t.cross(rad).normalized(), rad, 0.0125, 0.0068))
    report.append(('daumen', [round((mcp - cmc).length * 1000), round((ip - mcp).length * 1000),
                              round((tip - ip).length * 1000)], 0))
    return bm, nails, report, fore


def _hand_to_lamp(fore):
    """Drehung und Verschiebung Handrahmen -> Lampenrahmen: Lampenachse -> +x, Unterarm aus Richtung THETA_F."""
    c, a = _barrel()
    Z = Vector((0, 0, 1))
    T = a.cross(Z)
    best = None
    for k in range(720):
        th = 2 * math.pi * k / 720

        def d(t):
            return Vector((0.0, math.cos(t), math.sin(t)))
        R = Matrix((Vector((1, 0, 0)), d(th), d(th + math.pi / 2))).transposed() @ Matrix((a, Z, T))
        f = R @ fore
        perp = Vector((0.0, f.y, f.z))
        ang = math.atan2(perp.z, perp.y)
        err = abs(math.remainder(ang - THETA_F, 2 * math.pi))
        if best is None or err < best[0]:
            best = (err, R, th)
    R = best[1]
    # Griffmitte: Punkt der Achse unter der Mitte des Handtellers
    p = Vector((0.050, 0.004, 0.0))
    g = c + a * (p - c).dot(a)
    M = Matrix.Translation(-(R @ g)) @ R.to_4x4()
    return M, math.degrees(best[2])


def build_vm_hand_l():
    skin = common.material('haut', SKIN, look='plastic', rough=0.6, fm='skin_player')
    nailm = common.material('nagel', (0.86, 0.72, 0.66), look='plastic', rough=0.35, fm='nail')
    bm, nails, report, fore = _hand_parts()
    M, th = _hand_to_lamp(fore)
    bm.transform(M)
    ob = common.obj_from_bmesh('hand', bm, skin)
    rm = ob.modifiers.new('vereinigen', 'REMESH')
    rm.mode = 'VOXEL'
    rm.voxel_size = 0.0006
    rm.adaptivity = 0.0
    rm.use_smooth_shade = True
    sm = ob.modifiers.new('glaetten', 'SMOOTH')
    sm.factor = 0.6
    sm.iterations = 10
    dec = ob.modifiers.new('reduzieren', 'DECIMATE')
    dec.ratio = 0.05
    ob.data.shade_smooth()
    # Naegel: flache, leicht gewoelbte Schalen auf den Endgliedern
    nb = bmesh.new()
    for (ctr, t, side, nrm, L, W) in nails:
        res = bmesh.ops.create_uvsphere(nb, u_segments=16, v_segments=8, radius=1.0)
        m = Matrix((t * (L / 2), side * W, nrm * 0.0012)).transposed()
        for v in res['verts']:
            v.co = M @ (ctr + m @ v.co)
    # die Achsen (Laenge, Breite, Normale) sind teils linkshaendig - Flaechen nach aussen richten
    bmesh.ops.recalc_face_normals(nb, faces=nb.faces)
    nob = common.obj_from_bmesh('naegel', nb, nailm)
    nob.data.shade_smooth()
    wrist = M @ Vector((0, 0, 0))
    fl = (M.to_3x3() @ fore).normalized()
    hd = (M.to_3x3() @ Vector((1, 0, 0))).normalized()
    palm = (M.to_3x3() @ Vector((0, 0, -1))).normalized()
    # Werte fuer gameplay/body.cpp (kVmWrist, kVmForearm, kVmHandDir, kVmPalm): im Spiel ist y gespiegelt
    print('HAND_WRIST', tuple(round(x, 4) for x in wrist), 'HAND_FOREARM', tuple(round(x, 4) for x in fl),
          'HAND_DIR', tuple(round(x, 4) for x in hd), 'HAND_PALM', tuple(round(x, 4) for x in palm),
          'ROLL', round(th, 1), flush=True)
    for r in report:
        print('FINGER', r, flush=True)
