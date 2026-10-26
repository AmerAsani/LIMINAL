"""V12 Level 0 (LOOK_0.md): Rasterleuchte, Luftauslass, Boegen.

Deckenmodelle haben den Ursprung in der Deckenebene (z = 0) und haengen nach unten; die Rasterdecke hat ein
0,6-m-Raster (ceiling_office, 3,6 m Textur). Boegen stehen mit dem Ursprung in der Mitte des Durchbruchs am
Boden; die Laibung laeuft entlang x (Wandstaerke 1 m), die Oeffnung entlang y.
"""
import math

import bmesh
import bpy

import common


def _frame(name, outer_x, outer_y, inner_x, inner_y, z0, z1, mat, bevel=0.003):
    """Rechteckiger Rahmen (Blech-Kante um eine Leuchte) als Ring aus vier Kaesten."""
    hx, hy, ix, iy = outer_x / 2, outer_y / 2, inner_x / 2, inner_y / 2
    parts = [((-hx, -hy), (hx, -iy)), ((-hx, iy), (hx, hy)), ((-hx, -iy), (-ix, iy)), ((ix, -iy), (hx, iy))]
    for k, (lo, hi) in enumerate(parts):
        common.box(f'{name}_{k}', (lo[0], lo[1], z0), (hi[0], hi[1], z1), mat, bevel=bevel, segments=1)


def _troffer(lit, tint=(1.0, 0.99, 0.92)):
    frame = common.material('rahmen', (0.86, 0.85, 0.80), look='paint', rough=0.45)
    trim = common.material('innen', (0.70, 0.70, 0.66), look='paint', rough=0.5)
    if lit:
        lens = common.material('scheibe', tint, look='glow', glow=1.0)
    else:
        lens = common.material('scheibe_aus', (0.62, 0.62, 0.60), look='plastic', rough=0.35)
    # 0,6 x 1,2 m (lange Achse x): flacher Blechrahmen unter der Decke, darin die Prismenscheibe leicht vertieft
    _frame('flansch', 1.212, 0.612, 1.17, 0.57, -0.012, 0.0, frame)
    _frame('kehle', 1.17, 0.57, 1.13, 0.53, -0.006, 0.0, trim, bevel=0.002)
    common.box('scheibe', (-0.565, -0.265, -0.004), (0.565, 0.265, -0.002), lens)
    # Prismen: zwei feine Stege quer, wie die Teilung einer Prismenscheibe
    for x in (-0.19, 0.19):
        common.box(f'steg_{x}', (x - 0.004, -0.265, -0.006), (x + 0.004, 0.265, -0.002), trim)


def build_troffer():
    _troffer(True)


def build_troffer_off():
    _troffer(False)


def build_troffer_red():
    """Rote Raeume (Wikidot: roetlicher Schimmer) - die Scheibe gibt rotes Licht."""
    _troffer(True, (1.0, 0.30, 0.22))


def build_ceiling_diffuser():
    """Quadratischer Luftauslass 0,6 x 0,6 m mit vier Lamellenringen (Deckenebene z = 0)."""
    metal = common.material('blech', (0.84, 0.84, 0.82), look='paint', rough=0.4)
    dark = common.material('schlitz', (0.30, 0.30, 0.29), look='paint', rough=0.9)  # nur in den Spalten sichtbar
    _frame('rand', 0.596, 0.596, 0.52, 0.52, -0.02, 0.0, metal, bevel=0.004)
    common.box('dunkel', (-0.26, -0.26, -0.004), (0.26, 0.26, -0.001), dark)
    for k, s in enumerate((0.21, 0.155, 0.10, 0.05)):
        _frame(f'lamelle{k}', 2 * s + 0.03, 2 * s + 0.03, 2 * s, 2 * s, -0.035 + k * 0.004, -0.004, metal, bevel=0.0015)
    common.box('mitte', (-0.025, -0.025, -0.045), (0.025, 0.025, -0.004), metal, bevel=0.004)


def _arch(width):
    """Segmentbogen im Durchbruch: Laibung 1 m tief (Wandstaerke), Oeffnung 'width' breit, Kaempfer bei
    1,85 m, Scheitel 2,30 m. Das Modell fuellt die Ecken zwischen Bogen und Sturz (2,35 m) und traegt ein
    schmales Kaempferprofil."""
    plaster = common.material('putz', (0.98, 0.95, 0.88), look='concrete', tex='plaster_white')
    profile = common.material('profil', (0.95, 0.92, 0.85), look='paint', rough=0.5)
    half = width / 2.0
    spring, crown, top = 1.85, 2.30, 2.36
    rise = crown - spring
    # Kreis durch (+-half, spring) und (0, crown)
    radius = (half * half + rise * rise) / (2 * rise)
    cz = crown - radius
    n = 24
    bm = bmesh.new()
    # Profil in der yz-Ebene: von (-half, top) ueber (-half, spring), den Bogen, (half, spring), (half, top)
    pts = [(-half, top), (-half, spring)]
    a0 = math.atan2(spring - cz, -half)
    a1 = math.atan2(spring - cz, half)
    for i in range(1, n):
        t = i / n
        a = a0 + (a1 - a0) * t
        pts.append((radius * math.cos(a), cz + radius * math.sin(a)))
    pts += [(half, spring), (half, top)]
    depth = 0.5
    front = [bm.verts.new((depth, y, z)) for (y, z) in pts]
    back = [bm.verts.new((-depth, y, z)) for (y, z) in pts]
    m = len(pts)
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(m):
        j = (i + 1) % m
        bm.faces.new((front[i], back[i], back[j], front[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('bogen', bm, plaster)
    common.shade_auto(ob, 30.0)
    # Kaempferprofil: schmale Leiste an beiden Seiten in Kaempferhoehe, ueber die ganze Laibungstiefe
    for s in (-1, 1):
        y0 = s * half
        common.box(f'kaempfer{s}', (-depth - 0.02, min(y0, y0 - s * 0.035), spring - 0.06),
                   (depth + 0.02, max(y0, y0 - s * 0.035), spring), profile, bevel=0.008)


def build_arch2():
    _arch(2.0)


def build_arch3():
    _arch(3.0)


def build_arch4():
    _arch(4.0)


def _cyl_x(name, x0, x1, y, z, r, mat, seg=24):
    """Zylinder entlang x (Wandmodelle blicken entlang +x)."""
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=r, depth=x1 - x0, location=((x0 + x1) / 2, y, z),
                                        rotation=(0.0, math.pi / 2, 0.0))
    ob = bpy.context.active_object
    ob.name = name
    ob.data.materials.append(mat)
    common.shade_auto(ob, 30.0)
    return ob


def build_outlet():
    """Doppelsteckdose (Wandmontage ueber dem Sockel, blickt entlang +x, Ursprung Wandflaeche Mitte):
    Rahmen 152 x 82 mm, zwei runde Einsaetze mit Kontaktloechern und Schutzkontakten."""
    plastic = common.material('kunststoff', (0.90, 0.88, 0.80), look='plastic', rough=0.35)
    insert = common.material('einsatz', (0.84, 0.82, 0.75), look='plastic', rough=0.4)
    hole = common.material('loch', (0.03, 0.03, 0.03), look='plastic', rough=0.9)
    metal = common.material('kontakt', (0.70, 0.70, 0.68), look='metal', rough=0.3)
    common.box('rahmen', (0.0, -0.076, -0.041), (0.011, 0.076, 0.041), plastic, bevel=0.004, segments=3)
    for y in (-0.038, 0.038):
        _cyl_x(f'mulde{y}', 0.004, 0.012, y, 0.0, 0.021, insert)
        for dy in (-0.0095, 0.0095):
            _cyl_x(f'loch{y}{dy}', 0.0115, 0.0125, y + dy, 0.0, 0.0026, hole, seg=12)
        for dz in (-0.017, 0.017):
            common.box(f'schutz{y}{dz}', (0.010, y - 0.004, dz - 0.0015), (0.0125, y + 0.004, dz + 0.0015), metal)


def build_vent_grille():
    """Lueftungsgitter (Wandmontage nahe der Decke, blickt entlang +x): Rahmen 540 x 320 mm mit Fase, schraege
    Lamellen, dahinter Dunkel, vier Schrauben."""
    paint = common.material('lack', (0.86, 0.85, 0.80), look='paint', rough=0.45)
    dark = common.material('dunkel', (0.02, 0.02, 0.02), look='paint', rough=0.95)
    screw = common.material('schraube', (0.62, 0.62, 0.60), look='metal', rough=0.35)
    w, h = 0.50, 0.28
    common.box('hinten', (0.0, -w / 2, -h / 2), (0.003, w / 2, h / 2), dark)
    _frame_wall = [((-w / 2 - 0.02, h / 2), (w / 2 + 0.02, h / 2 + 0.02)), ((-w / 2 - 0.02, -h / 2 - 0.02), (w / 2 + 0.02, -h / 2)),
                   ((-w / 2 - 0.02, -h / 2), (-w / 2, h / 2)), ((w / 2, -h / 2), (w / 2 + 0.02, h / 2))]
    for k, ((y0, z0), (y1, z1)) in enumerate(_frame_wall):
        common.box(f'rahmen{k}', (0.0, y0, z0), (0.018, y1, z1), paint, bevel=0.003)
    z = -h / 2 + 0.022
    k = 0
    while z < h / 2 - 0.01:
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co.x *= 0.016
            v.co.y *= w
            v.co.z *= 0.004
        ob = common.obj_from_bmesh(f'lamelle{k}', bm, paint)
        ob.location = (0.010, 0.0, z)
        ob.rotation_euler = (0.0, -0.75, 0.0)
        z += 0.034
        k += 1
    for (y, zz) in ((-w / 2 - 0.01, h / 2 + 0.01), (w / 2 + 0.01, h / 2 + 0.01), (-w / 2 - 0.01, -h / 2 - 0.01), (w / 2 + 0.01, -h / 2 - 0.01)):
        _cyl_x(f'schraube{y}{zz}', 0.018, 0.021, y, zz, 0.004, screw, seg=10)
