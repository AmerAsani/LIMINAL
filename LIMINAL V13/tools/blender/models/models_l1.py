"""V12 Level 1 (LOOK_1.md): Holzkisten, Haufen aus Brettern/Verpackung/Blech, farbige Stuetzenbaender, hohe
Betonboegen und runde Saeulen (Gothic), Schwingtueren, Krankenbett, Polsterfelder, Glühbirne, Arbeitstisch.

Bodenmodelle stehen mit dem Ursprung am Boden; Wandmodelle haben die Wandflaeche bei x = 0 und blicken entlang +x;
Tuer- und Bogenmodelle stehen mit dem Ursprung in der Mitte des Durchbruchs am Boden (Laibung entlang x, 1 m tief).
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

PINE = (0.66, 0.53, 0.38)


def _rot_box(name, size, loc, rot, mat, bevel=0.0):
    """Kasten der Groesse size (x, y, z), mittig um loc, gedreht um rot (Euler xyz, rad)."""
    ob = common.box(name, (-size[0] / 2, -size[1] / 2, -size[2] / 2), (size[0] / 2, size[1] / 2, size[2] / 2), mat,
                    bevel=bevel, segments=1)
    ob.location = loc
    ob.rotation_euler = rot
    return ob


# --- Kisten ----------------------------------------------------------------------------------------------------
def _crate(L, W, H, seed):
    """Schlichte Holzkiste aus Brettern (4 mm Fugen) mit Eckleisten und Randleisten; L entlang x, W entlang y."""
    rng = random.Random(seed)
    wood = common.material('kiste', PINE, look='wood', rough=0.8)
    dark = common.material('leiste', (0.55, 0.43, 0.30), look='wood', rough=0.85)
    t, gap, bw = 0.018, 0.004, 0.11  # Brettstaerke, Fuge, Brettbreite (Soll)
    # Seiten mit waagrechten Brettern
    n = max(2, round(H / (bw + gap)))
    h = (H - (n - 1) * gap) / n
    for k in range(n):
        z0 = k * (h + gap)
        common.box(f'bx{k}a', (L / 2 - t, -W / 2 + t, z0), (L / 2, W / 2 - t, z0 + h), wood, bevel=0.0015, segments=1)
        common.box(f'bx{k}b', (-L / 2, -W / 2 + t, z0), (-L / 2 + t, W / 2 - t, z0 + h), wood, bevel=0.0015, segments=1)
        common.box(f'by{k}a', (-L / 2, W / 2 - t, z0), (L / 2, W / 2, z0 + h), wood, bevel=0.0015, segments=1)
        common.box(f'by{k}b', (-L / 2, -W / 2, z0), (L / 2, -W / 2 + t, z0 + h), wood, bevel=0.0015, segments=1)
    # Deckel: Bretter entlang x
    m_ = max(2, round(W / (bw + gap)))
    w = (W - (m_ - 1) * gap) / m_
    for k in range(m_):
        y0 = -W / 2 + k * (w + gap)
        dz = rng.uniform(0.0, 0.002)
        common.box(f'top{k}', (-L / 2, y0, H - t + dz), (L / 2, y0 + w, H + dz), wood, bevel=0.0015, segments=1)
    # Eckleisten (senkrecht) und Randleisten oben/unten, aussen aufgesetzt
    b, bt = 0.05, 0.02
    for sx in (-1, 1):
        for sy in (-1, 1):
            x0, y0 = sx * L / 2, sy * W / 2
            xa, xb = sorted((x0 - sx * b, x0 + sx * bt))
            ya, yb = sorted((y0 - sy * b, y0 + sy * bt))
            common.box(f'ecke{sx}{sy}', (xa, ya, 0.0), (xb, yb, H), dark, bevel=0.002, segments=1)
    for z0 in (0.0, H - b):
        for sx in (-1, 1):
            x0 = sx * L / 2
            common.box(f'randx{sx}{z0}', (min(x0, x0 + sx * bt), -W / 2 + b, z0), (max(x0, x0 + sx * bt), W / 2 - b, z0 + b),
                       dark, bevel=0.002, segments=1)
        for sy in (-1, 1):
            y0 = sy * W / 2
            common.box(f'randy{sy}{z0}', (-L / 2 + b, min(y0, y0 + sy * bt), z0), (L / 2 - b, max(y0, y0 + sy * bt), z0 + b),
                       dark, bevel=0.002, segments=1)


def build_crate_a():
    _crate(0.6, 0.6, 0.6, 1)


def build_crate_b():
    _crate(0.9, 0.6, 0.55, 2)


def build_crate_c():
    _crate(1.1, 0.8, 0.75, 3)


# --- Haufen ----------------------------------------------------------------------------------------------------
def _debris(seed, n_planks, n_card, n_metal, radius):
    """Haufen aus Brettern, Verpackung (Karton) und Blech, flach am Boden liegend und uebereinander geworfen."""
    rng = random.Random(seed)
    wood = common.material('brett', (0.58, 0.47, 0.34), look='wood', rough=0.85)
    wood2 = common.material('brett_alt', (0.42, 0.36, 0.28), look='wood', rough=0.9)
    card = common.material('karton', (0.62, 0.50, 0.34), look='cardboard', rough=0.95)
    metal = common.material('blech', (0.48, 0.49, 0.50), look='galvanized', rough=0.5, metal=0.8)
    z = 0.0
    items = [('p', i) for i in range(n_planks)] + [('c', i) for i in range(n_card)] + [('m', i) for i in range(n_metal)]
    rng.shuffle(items)
    for k, (kind, i) in enumerate(items):
        a = rng.uniform(0, math.pi)
        r = rng.uniform(0, radius) * (1.0 - 0.5 * k / max(1, len(items)))
        cx, cy = r * math.cos(rng.uniform(0, 6.28)), r * math.sin(rng.uniform(0, 6.28))
        lift = 0.02 + min(0.35, k * 0.03)
        if kind == 'p':
            Lp, Wp, Tp = rng.uniform(0.9, 2.1), rng.uniform(0.08, 0.16), rng.uniform(0.018, 0.03)
            tilt = rng.uniform(-0.18, 0.18)
            _rot_box(f'brett{i}', (Lp, Wp, Tp), (cx, cy, lift * 0.6 + Tp / 2 + abs(math.sin(tilt)) * Lp / 2 * 0.5),
                     (rng.uniform(-0.1, 0.1), tilt, a), wood if rng.random() < 0.6 else wood2, bevel=0.002)
        elif kind == 'c':
            Lc, Wc = rng.uniform(0.5, 1.0), rng.uniform(0.4, 0.8)
            _rot_box(f'karton{i}', (Lc, Wc, 0.006), (cx, cy, lift * 0.5 + 0.01), (rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25), a),
                     card)
        else:
            Lm, Wm = rng.uniform(0.5, 1.2), rng.uniform(0.3, 0.6)
            _rot_box(f'blech{i}', (Lm, Wm, 0.003), (cx, cy, lift * 0.5 + 0.01), (rng.uniform(-0.3, 0.3), rng.uniform(-0.2, 0.2), a),
                     metal)
    # ein liegender Kartonquader und ein Brettstapel geben dem Haufen Hoehe
    _rot_box('kasten', (0.55, 0.4, 0.32), (radius * 0.3, -radius * 0.2, 0.16), (0, 0, rng.uniform(0, 3)), card, bevel=0.004)


def build_debris_a():
    _debris(11, 9, 3, 2, 0.8)


def build_debris_b():
    _debris(12, 5, 5, 3, 0.7)


# --- Farbige Stuetzenbaender (Merkpunkte) -----------------------------------------------------------------------
def _band(rgb, size):
    paint = common.material('band', rgb, look='paint', rough=0.55)
    h = size / 2 + 0.003
    z0, z1 = 1.0, 1.8
    common.box('n', (-h, h - 0.006, z0), (h, h, z1), paint)
    common.box('s', (-h, -h, z0), (h, -h + 0.006, z1), paint)
    common.box('o', (h - 0.006, -h, z0), (h, h, z1), paint)
    common.box('w', (-h, -h, z0), (-h + 0.006, h, z1), paint)


COLORS = {'blau': (0.18, 0.32, 0.58), 'gelb': (0.82, 0.64, 0.14), 'rot': (0.62, 0.16, 0.12), 'gruen': (0.18, 0.46, 0.27)}


def build_pillar_band_blau():
    _band(COLORS['blau'], 1.0)


def build_pillar_band_gelb():
    _band(COLORS['gelb'], 1.0)


def build_pillar_band_rot():
    _band(COLORS['rot'], 1.0)


def build_pillar_band_gruen():
    _band(COLORS['gruen'], 1.0)


def build_pillar_band2_blau():
    _band(COLORS['blau'], 2.0)


def build_pillar_band2_gelb():
    _band(COLORS['gelb'], 2.0)


# --- Gothic: hohe Betonboegen und runde Saeulen -----------------------------------------------------------------
def _arch_tall(width, spring, crown, top):
    """Halbkreisnaher Bogen im Durchbruch (Laibung 1 m), fuellt bis zum Sturz 'top'."""
    conc = common.material('beton', (0.86, 0.85, 0.82), look='concrete', tex='concrete_wall_smooth')
    half = width / 2.0
    rise = crown - spring
    radius = (half * half + rise * rise) / (2 * rise)
    cz = crown - radius
    n = 32
    bm = bmesh.new()
    pts = [(-half, top), (-half, spring)]
    a0, a1 = math.atan2(spring - cz, -half), math.atan2(spring - cz, half)
    for i in range(1, n):
        a = a0 + (a1 - a0) * i / n
        pts.append((radius * math.cos(a), cz + radius * math.sin(a)))
    pts += [(half, spring), (half, top)]
    depth = 0.5
    front = [bm.verts.new((depth, y, z)) for (y, z) in pts]
    back = [bm.verts.new((-depth, y, z)) for (y, z) in pts]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        bm.faces.new((front[i], back[i], back[j], front[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('bogen', bm, conc)
    common.shade_auto(ob, 30.0)
    # Bogenlaibung mit umlaufender Kante (Fase aus einem schmalen Ring vor der Wand)
    for s in (-1, 1):
        common.box(f'kaempfer{s}', (-depth - 0.03, min(s * half, s * half - s * 0.05), spring - 0.12),
                   (depth + 0.03, max(s * half, s * half - s * 0.05), spring), conc, bevel=0.01)


def build_arch_tall3():
    _arch_tall(3.0, 3.0, 4.35, 4.62)


def build_arch_tall4():
    _arch_tall(4.0, 2.9, 4.45, 4.62)


def build_column_round():
    """Runde Betonsaeule 5,6 m (Gothic): Sockel, Schaft mit leichter Schwellung, Kragen unter der Decke."""
    conc = common.material('beton', (0.86, 0.85, 0.82), look='concrete', tex='concrete_wall_smooth')
    H, r = 5.6, 0.42
    prof = [(0.0, r + 0.10), (0.10, r + 0.10), (0.14, r + 0.04), (0.20, r)]
    for k in range(1, 8):
        t = k / 8.0
        prof.append((0.20 + (H - 0.9) * t, r * (1.0 + 0.035 * math.sin(math.pi * t))))
    prof += [(H - 0.55, r), (H - 0.35, r + 0.10), (H - 0.18, r + 0.16), (H, r + 0.16)]
    bm = bmesh.new()
    seg = 40
    rings = []
    for (z, rr) in prof:
        rings.append([bm.verts.new((rr * math.cos(2 * math.pi * k / seg), rr * math.sin(2 * math.pi * k / seg), z)) for k in range(seg)])
    for i in range(len(rings) - 1):
        for k in range(seg):
            j = (k + 1) % seg
            bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('saeule', bm, conc)
    common.shade_auto(ob, 40.0)


# --- Schwingtueren --------------------------------------------------------------------------------------------
def _swing(width, leaves):
    """Zarge im Durchbruch (2,2 m Sturz) und Fluegel, die nach +x offen stehen (Schwingtuer, angelehnt)."""
    frame = common.material('zarge', (0.36, 0.37, 0.36), look='paint', rough=0.5)
    leaf = common.material('fluegel', (0.55, 0.56, 0.53), look='paint', rough=0.55)
    kick = common.material('tritt', (0.62, 0.62, 0.60), look='metal', rough=0.4, metal=0.8)
    glass = common.material('fenster', (0.20, 0.22, 0.22), look='glass', rough=0.1)
    half, top, fd = width / 2.0, 2.2, 0.06
    # Zarge: zwei Pfosten und Sturz auf der +x-Seite der Laibung
    for s in (-1, 1):
        common.box(f'pfosten{s}', (0.44, s * half - (0.05 if s > 0 else 0), 0.0), (0.5, s * half + (0.0 if s > 0 else 0.05), top),
                   frame, bevel=0.004)
    common.box('sturz', (0.44, -half, top - 0.05), (0.5, half, top), frame, bevel=0.004)
    lw = (width - 0.02) / leaves
    opens = [math.radians(72), math.radians(-64)] if leaves == 2 else [math.radians(78)]
    for k in range(leaves):
        hinge_y = -half + 0.01 if k == 0 else half - 0.01
        sgn = 1 if k == 0 else -1
        ang = opens[k] * sgn
        parts = []
        # Fluegel im eigenen Rahmen: Scharnier im Ursprung, Fluegel entlang +y*sgn
        def box_rel(name, y0, y1, z0, z1, x0, x1, mat):
            ob = common.box(name, (x0, min(y0, y1), z0), (x1, max(y0, y1), z1), mat, bevel=0.003, segments=1)
            parts.append(ob)
        box_rel(f'blatt{k}', 0.0, sgn * lw, 0.01, top - 0.06, -fd / 2, fd / 2, leaf)
        box_rel(f'tritt{k}a', 0.03 * sgn, sgn * (lw - 0.03), 0.04, 0.30, fd / 2, fd / 2 + 0.002, kick)
        box_rel(f'tritt{k}b', 0.03 * sgn, sgn * (lw - 0.03), 0.04, 0.30, -fd / 2 - 0.002, -fd / 2, kick)
        box_rel(f'fenster{k}a', sgn * (lw * 0.3), sgn * (lw * 0.7), 1.35, 1.75, fd / 2, fd / 2 + 0.003, glass)
        box_rel(f'fenster{k}b', sgn * (lw * 0.3), sgn * (lw * 0.7), 1.35, 1.75, -fd / 2 - 0.003, -fd / 2, glass)
        box_rel(f'platte{k}', sgn * (lw - 0.28), sgn * (lw - 0.06), 1.0, 1.2, fd / 2, fd / 2 + 0.003, kick)
        M = Matrix.Translation((0.47, hinge_y, 0.0)) @ Matrix.Rotation(ang, 4, 'Z')
        for ob in parts:
            ob.matrix_world = M @ ob.matrix_world


def build_swing_door1():
    _swing(1.0, 1)


def build_swing_door2():
    _swing(2.0, 2)


# --- Krankenzimmer, Gummizelle, Buero, Halle --------------------------------------------------------------------
def build_hospital_bed():
    """Krankenbett (2,05 x 0,95 m, Liegeflaeche 0,6 m) mit Rollen, Kopf- und Fussteil, Seitengittern, Matratze."""
    steel = common.material('gestell', (0.78, 0.79, 0.78), look='metal', rough=0.35, metal=0.9)
    board = common.material('kopfteil', (0.86, 0.85, 0.80), look='plastic', rough=0.5)
    mat = common.material('matratze', (0.74, 0.78, 0.76), look='fabric', rough=0.9)
    sheet = common.material('laken', (0.88, 0.88, 0.85), look='fabric', rough=0.95)
    rubber = common.material('rolle', (0.08, 0.08, 0.08), look='rubber', rough=0.9)
    L, W = 2.05, 0.95
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (L / 2 - 0.08), sy * (W / 2 - 0.08)
            common.box(f'bein{sx}{sy}', (x - 0.02, y - 0.02, 0.10), (x + 0.02, y + 0.02, 0.48), steel, bevel=0.006)
            bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.055, depth=0.035, location=(x, y, 0.055),
                                                rotation=(math.pi / 2, 0, 0))
            bpy.context.active_object.data.materials.append(rubber)
    common.box('rahmen', (-L / 2 + 0.04, -W / 2 + 0.03, 0.46), (L / 2 - 0.04, W / 2 - 0.03, 0.52), steel, bevel=0.01)
    common.box('matratze', (-L / 2 + 0.07, -W / 2 + 0.06, 0.52), (L / 2 - 0.07, W / 2 - 0.06, 0.66), mat, bevel=0.03, segments=3)
    common.box('laken', (-L / 2 + 0.25, -W / 2 + 0.05, 0.60), (L / 2 - 0.08, W / 2 - 0.05, 0.672), sheet, bevel=0.03, segments=3)
    common.box('kissen', (-L / 2 + 0.1, -0.3, 0.64), (-L / 2 + 0.48, 0.3, 0.74), sheet, bevel=0.04, segments=3)
    common.box('kopf', (-L / 2 - 0.02, -W / 2, 0.40), (-L / 2 + 0.03, W / 2, 1.05), board, bevel=0.015, segments=2)
    common.box('fuss', (L / 2 - 0.03, -W / 2, 0.40), (L / 2 + 0.02, W / 2, 0.86), board, bevel=0.015, segments=2)
    for sy in (-1, 1):
        y = sy * (W / 2 + 0.01)
        for z in (0.70, 0.86):
            common.box(f'gitter{sy}{z}', (-0.55, y - 0.012, z - 0.012), (0.45, y + 0.012, z + 0.012), steel, bevel=0.008)
        for x in (-0.5, 0.0, 0.4):
            common.box(f'stab{sy}{x}', (x - 0.01, y - 0.01, 0.6), (x + 0.01, y + 0.01, 0.86), steel)


def build_padded_panel():
    """Polsterfeld der Gummizelle (Wandflaeche x = 0, 1,0 x 2,4 m, 9 cm tief): 2 x 4 gesteppte Kissen."""
    pad = common.material('polster', (0.86, 0.84, 0.78), look='fabric', rough=0.85)
    seam = common.material('naht', (0.66, 0.64, 0.58), look='fabric', rough=0.9)
    common.box('grund', (0.0, -0.5, 0.0), (0.03, 0.5, 2.4), seam)
    for i in range(2):
        for k in range(4):
            y0, z0 = -0.49 + i * 0.49, 0.01 + k * 0.6
            ob = common.box(f'kissen{i}{k}', (0.02, y0 + 0.01, z0 + 0.01), (0.09, y0 + 0.47, z0 + 0.57), pad, bevel=0.03, segments=4)


def build_hanging_bulb():
    """Einzelne Gluehbirne am Kabel (Decke bei z = 0, haengt 0,7 m herab): Rosette, Kabel, Fassung, Birne."""
    black = common.material('fassung', (0.05, 0.05, 0.05), look='plastic', rough=0.6)
    bulb = common.material('birne', (1.0, 0.82, 0.55), look='glow', glow=2.5)
    bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=0.045, depth=0.02, location=(0, 0, -0.01))
    bpy.context.active_object.data.materials.append(black)
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.003, depth=0.6, location=(0, 0, -0.32))
    bpy.context.active_object.data.materials.append(black)
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.018, depth=0.06, location=(0, 0, -0.65))
    bpy.context.active_object.data.materials.append(black)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=12, radius=0.032, location=(0, 0, -0.71))
    ob = bpy.context.active_object
    ob.scale = (1.0, 1.0, 1.25)
    ob.data.materials.append(bulb)
    ob.data.shade_smooth()


def build_work_table():
    """Arbeitstisch 1,6 x 0,8 m, 0,75 m hoch: Stahlgestell, Holzplatte mit Kanten, Unterboden."""
    steel = common.material('gestell', (0.30, 0.32, 0.33), look='paint', rough=0.5)
    top = common.material('platte', (0.56, 0.45, 0.33), look='wood', rough=0.7)
    L, W, H = 1.6, 0.8, 0.75
    common.box('platte', (-L / 2, -W / 2, H - 0.035), (L / 2, W / 2, H), top, bevel=0.004)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * (L / 2 - 0.05), sy * (W / 2 - 0.05)
            common.box(f'bein{sx}{sy}', (x - 0.02, y - 0.02, 0.0), (x + 0.02, y + 0.02, H - 0.035), steel, bevel=0.003)
    common.box('zarge_x1', (-L / 2 + 0.05, W / 2 - 0.07, H - 0.11), (L / 2 - 0.05, W / 2 - 0.05, H - 0.035), steel)
    common.box('zarge_x2', (-L / 2 + 0.05, -W / 2 + 0.05, H - 0.11), (L / 2 - 0.05, -W / 2 + 0.07, H - 0.035), steel)
    common.box('boden', (-L / 2 + 0.05, -W / 2 + 0.05, 0.16), (L / 2 - 0.05, W / 2 - 0.05, 0.18), top)


# --- Notausgangsschild (gruen leuchtend, weisses Piktogramm) --------------------------------------------------------
def build_exit_sign_green():
    """Notausgangsschild (Wandmontage, Wandflaeche x = 0, blickt entlang +x; Mitte des Schilds im Ursprung):
    Gehaeuse 40 x 16 cm, gruene Leuchtflaeche, weisses Piktogramm (Laeufer, Tuer, Pfeil) - eigene Zeichnung."""
    housing = common.material('gehaeuse', (0.88, 0.89, 0.87), look='plastic', rough=0.4)
    green = common.material('gruen', (0.10, 0.85, 0.35), look='glow', glow=2.2)
    white = common.material('weiss', (0.95, 1.0, 0.96), look='glow', glow=3.0)
    common.box('gehaeuse', (0.0, -0.20, -0.08), (0.045, 0.20, 0.08), housing, bevel=0.006, segments=2)
    common.box('feld', (0.045, -0.19, -0.07), (0.047, 0.19, 0.07), green)
    x = 0.0475

    def q(name, y0, z0, y1, z1, rot=0.0):
        cy, cz = (y0 + y1) / 2, (z0 + z1) / 2
        ob = common.box(name, (x, -(y1 - y0) / 2, -(z1 - z0) / 2), (x + 0.0015, (y1 - y0) / 2, (z1 - z0) / 2), white)
        ob.location = (0, cy, cz)
        ob.rotation_euler = (rot, 0, 0)
    # Tuer (Rahmen) rechts im Bild = -y (das Schild blickt entlang +x, rechts davon ist -y)
    q('tuer_l', -0.165, -0.055, -0.155, 0.055)
    q('tuer_r', -0.105, -0.055, -0.095, 0.055)
    q('tuer_o', -0.165, 0.045, -0.095, 0.055)
    # Laeufer: Kopf, Rumpf schraeg, Arme und Beine im Schritt
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.011, depth=0.0015, location=(x + 0.0008, -0.03, 0.045),
                                        rotation=(0, math.pi / 2, 0))
    bpy.context.active_object.data.materials.append(white)
    q('rumpf', -0.012, -0.012, 0.002, 0.03, rot=-0.35)
    q('arm_v', -0.06, 0.012, -0.012, 0.022, rot=0.4)
    q('arm_h', -0.002, 0.008, 0.03, 0.018, rot=-0.5)
    q('bein_v', -0.055, -0.03, -0.012, -0.02, rot=-0.6)
    q('bein_h', -0.006, -0.05, 0.024, -0.04, rot=0.7)
    # Pfeil links (+y)
    q('pfeil', 0.06, -0.006, 0.17, 0.006)
    q('pfeil_o', 0.055, 0.005, 0.09, 0.015, rot=0.6)
    q('pfeil_u', 0.055, -0.015, 0.09, -0.005, rot=-0.6)


# --- Bilder (eigene, schlichte Malerei aus Farbflaechen) --------------------------------------------------------------
def _painting(W, H, seed, scene, lean=False):
    """Bild mit Rahmen. Wandmontage: Rueckseite bei x = 0, Mitte im Ursprung. lean: unten auf dem Boden (z = 0)
    an die Wand gelehnt (8 Grad), Unterkante 6 cm vor der Wand."""
    rng = random.Random(seed)
    frame_c = rng.choice([(0.55, 0.42, 0.20), (0.30, 0.20, 0.12), (0.62, 0.52, 0.30), (0.12, 0.10, 0.09)])
    frame = common.material(f'rahmen{seed}', frame_c, look='wood' if frame_c[0] < 0.5 else 'paint', rough=0.45)
    fw = 0.05 if W > 0.8 else 0.04
    parts = []

    def box(name, lo, hi, mat):
        parts.append(common.box(name, lo, hi, mat, bevel=0.004 if 'rahmen' in name else 0.0, segments=1))

    box('rahmen_o', (0.0, -W / 2 - fw, H / 2), (0.035, W / 2 + fw, H / 2 + fw), frame)
    box('rahmen_u', (0.0, -W / 2 - fw, -H / 2 - fw), (0.035, W / 2 + fw, -H / 2), frame)
    box('rahmen_l', (0.0, W / 2, -H / 2), (0.035, W / 2 + fw, H / 2), frame)
    box('rahmen_r', (0.0, -W / 2 - fw, -H / 2), (0.035, -W / 2, H / 2), frame)
    canvas_x = 0.012

    def layer(name, rgb, y0, z0, y1, z1, depth):
        mat = common.material(f'{name}{seed}', rgb, look='paint', rough=0.8)
        box(name, (canvas_x + depth, y0, z0), (canvas_x + depth + 0.001, y1, z1), mat)

    def poly(name, rgb, pts, depth):
        """Flaeche aus Punkten (y, z) in der Bildebene, Vorderseite entlang +x."""
        mat = common.material(f'{name}{seed}', rgb, look='paint', rough=0.8)
        bm = bmesh.new()
        vs = [bm.verts.new((canvas_x + depth, y, z)) for (y, z) in pts]
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()
        parts.append(common.obj_from_bmesh(name, bm, mat))
    hw, hh = W / 2, H / 2
    if scene == 'landschaft':
        sky = rng.choice([(0.55, 0.62, 0.70), (0.70, 0.66, 0.55), (0.48, 0.55, 0.62)])
        layer('himmel', sky, -hw, -hh, hw, hh, 0.0)
        for k in range(3):
            base = -hh + H * (0.45 - 0.12 * k)
            col = [(0.46, 0.46, 0.34), (0.38, 0.42, 0.28), (0.30, 0.36, 0.24)][k]
            pts = [(hw, -hh), (-hw, -hh)]
            n = 7
            for i in range(n + 1):
                y = -hw + W * i / n
                pts.append((y, base + H * 0.10 * math.sin(i * 1.3 + k + seed) * (0.6 + 0.4 * rng.random())))
            poly(f'huegel{k}', col, pts, 0.001 * (k + 1))
        if rng.random() < 0.6:
            layer('see', (0.40, 0.48, 0.55), -hw * 0.6, -hh * 0.85, hw * 0.3, -hh * 0.65, 0.005)
    elif scene == 'meer':
        layer('himmel', (0.62, 0.66, 0.68), -hw, 0.0, hw, hh, 0.0)
        layer('meer', (0.22, 0.32, 0.40), -hw, -hh, hw, 0.0, 0.0)
        layer('horizont', (0.75, 0.74, 0.66), -hw, -0.01, hw, 0.01, 0.002)
        poly('segel', (0.85, 0.82, 0.74), [(0.22 * W, -0.02 * H), (0.18 * W, 0.25 * H), (0.1 * W, -0.02 * H)], 0.003)
    elif scene == 'stillleben':
        layer('grund', (0.24, 0.20, 0.16), -hw, -hh, hw, hh, 0.0)
        layer('tisch', (0.42, 0.30, 0.20), -hw, -hh, hw, -hh * 0.35, 0.001)
        pts = [(math.cos(a) * W * 0.12, -hh * 0.35 + H * 0.22 + math.sin(a) * H * 0.2) for a in [i * 6.283 / 18 for i in range(18)]]
        poly('vase', (0.58, 0.52, 0.38), pts, 0.002)
        for i in range(3):
            cx, cz = -W * 0.2 + i * W * 0.06, -hh * 0.35 + H * 0.06
            pts = [(cx + math.cos(a) * W * 0.04, cz + math.sin(a) * W * 0.04) for a in [i2 * 6.283 / 12 for i2 in range(12)]]
            poly(f'frucht{i}', [(0.62, 0.28, 0.16), (0.70, 0.56, 0.20), (0.40, 0.46, 0.20)][i], pts, 0.003)
    else:  # abstrakt
        layer('grund', (0.78, 0.74, 0.64), -hw, -hh, hw, hh, 0.0)
        for i in range(5):
            y0 = rng.uniform(-hw, hw * 0.4)
            z0 = rng.uniform(-hh, hh * 0.4)
            layer(f'feld{i}', rng.choice([(0.62, 0.22, 0.16), (0.20, 0.30, 0.48), (0.80, 0.66, 0.24), (0.18, 0.18, 0.18)]),
                  y0, z0, min(hw, y0 + rng.uniform(0.1, 0.5) * W), min(hh, z0 + rng.uniform(0.1, 0.5) * H), 0.001 * (i + 1))
    if lean:
        tilt = math.radians(8.0)
        M = Matrix.Translation((0.06, 0.0, 0.0)) @ Matrix.Rotation(-tilt, 4, 'Y') @ Matrix.Translation((0, 0, H / 2 + fw))
        for ob in parts:
            ob.matrix_world = M @ ob.matrix_world


def build_painting_a():
    _painting(1.1, 0.75, 31, 'landschaft')


def build_painting_b():
    _painting(0.9, 0.6, 32, 'meer')


def build_painting_c():
    _painting(0.6, 0.75, 33, 'stillleben')


def build_painting_d():
    _painting(0.8, 0.8, 34, 'abstrakt')


def build_painting_e():
    _painting(1.4, 0.9, 35, 'landschaft')


def build_painting_f():
    _painting(0.5, 0.7, 36, 'abstrakt')


def build_painting_lean_a():
    _painting(1.0, 0.7, 41, 'landschaft', lean=True)


def build_painting_lean_b():
    _painting(0.7, 0.9, 42, 'abstrakt', lean=True)


def build_painting_lean_c():
    _painting(0.8, 0.6, 43, 'meer', lean=True)
