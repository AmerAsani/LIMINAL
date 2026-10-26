"""V12 Level 188 (LOOK_188.md): Courtyard of Windows - Fenster in Varianten (Jalousie zu, halb, dunkel, mit Umriss),
das Eingangsfenster mit warmem Licht aus dem Hotel, Tueren aus Mahagoni (zu und offen), Pflasterstreifen, Kreis aus
Birkenholz, Tisch mit Gegenstaenden, Galerie mit Glasgelaender, weisse Stuetze.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix

import common
from models.models_l2 import _cyl


def _m():
    return {
        'frame': common.material('fensterrahmen', (0.90, 0.90, 0.88), look='paint', rough=0.4),
        'glass': common.material('scheibe', (0.08, 0.09, 0.10), look='glass', rough=0.05),
        'blind': common.material('jalousie', (0.86, 0.84, 0.78), look='paint', rough=0.6),
        'room': common.material('raum_dahinter', (0.20, 0.21, 0.24), look='glass', rough=0.15),
        'dark': common.material('dunkel', (0.02, 0.02, 0.025), look='glass', rough=0.1),
        'mahogany': common.material('mahagoni', (0.36, 0.13, 0.08), look='wood', tex='wood_walnut_floor', rough=0.4),
        'mahogany_d': common.material('mahagoni_dunkel', (0.24, 0.08, 0.05), look='wood', tex='wood_walnut_floor', rough=0.45),
        'brass': common.material('messing', (0.80, 0.62, 0.30), look='metal', rough=0.3, metal=1.0),
        'paving': common.material('pflaster', (0.92, 0.92, 0.92), look='concrete', tex='paving_grey', rough=0.8),
        'birch': common.material('birke', (0.70, 0.78, 0.95), look='wood', tex='wood_birch', rough=0.55),
        'concrete': common.material('beton_weiss', (0.95, 0.95, 0.95), look='concrete', tex='concrete_white', rough=0.8),
        'clear': common.material('glasgelaender', (0.75, 0.82, 0.82), look='glass', rough=0.02),
        'steel': common.material('stahl', (0.70, 0.71, 0.72), look='metal', rough=0.3, metal=1.0),
        'paper': common.material('papier', (0.86, 0.82, 0.70), look='paint', rough=0.85),
        'black': common.material('schwarz', (0.04, 0.04, 0.04), look='paint', rough=0.4),
    }


W, H = 1.2, 1.8  # Fensteroeffnung


def _poly(name, pts, mat, x):
    bm = bmesh.new()
    f = bm.faces.new([bm.verts.new((x, y, z)) for (y, z) in pts])
    f.normal_update()
    if f.normal.x < 0:
        f.normal_flip()
    common.obj_from_bmesh(name, bm, mat)


def _window(M, blinds=0.0, inside='room', figure=False, glow=None):
    """Fenster vor der Wand (x = 0, Blick +x), Mitte im Ursprung: Rahmen, Sprosse, Bank, Hintergrund (Raum dahinter),
    Jalousie bis zum Anteil blinds herab, Scheibe davor. figure: dunkler Umriss im Raum dahinter."""
    common.box('laibung_hinten', (0.0, -W / 2, -H / 2), (0.005, W / 2, H / 2), glow if glow else M[inside])
    if figure:
        pts = [(-0.16, -H / 2), (0.16, -H / 2), (0.15, 0.05), (0.2, 0.2), (0.17, 0.42), (0.09, 0.5), (0.05, 0.53),
               (0.06, 0.62), (0.0, 0.72), (-0.06, 0.62), (-0.05, 0.53), (-0.09, 0.5), (-0.17, 0.42), (-0.2, 0.2), (-0.15, 0.05)]
        _poly('umriss', [(y + 0.12, z) for (y, z) in pts], common.material('umriss', (0.01, 0.01, 0.012), look='paint', rough=0.9), 0.007)
    if blinds > 0.0:
        n = int(blinds * H / 0.05)
        for k in range(n):
            z = H / 2 - 0.04 - k * 0.05
            ob = common.box(f'lamelle{k}', (0.0, -W / 2 + 0.02, -0.018), (0.004, W / 2 - 0.02, 0.018), M['blind'])
            ob.matrix_world = Matrix.Translation((0.012, 0.0, z)) @ Matrix.Rotation(0.5, 4, 'Y')
        common.box('jal_leiste', (0.006, -W / 2 + 0.02, H / 2 - 0.04 - n * 0.05 - 0.02), (0.02, W / 2 - 0.02, H / 2 - 0.04 - n * 0.05 + 0.01), M['blind'])
        common.box('jal_kasten', (0.0, -W / 2, H / 2 - 0.06), (0.03, W / 2, H / 2), M['blind'])
    t = 0.07
    for (lo, hi) in (((0.0, -W / 2 - t, H / 2), (0.06, W / 2 + t, H / 2 + t)), ((0.0, -W / 2 - t, -H / 2 - t), (0.06, W / 2 + t, -H / 2)),
                     ((0.0, W / 2, -H / 2), (0.06, W / 2 + t, H / 2)), ((0.0, -W / 2 - t, -H / 2), (0.06, -W / 2, H / 2))):
        common.box('rahmen', lo, hi, M['frame'], bevel=0.004)
    common.box('sprosse', (0.026, -0.02, -H / 2), (0.05, 0.02, H / 2), M['frame'])
    common.box('kaempfer', (0.026, -W / 2, 0.3), (0.05, W / 2, 0.34), M['frame'])
    common.box('bank', (0.0, -W / 2 - 0.1, -H / 2 - t - 0.04), (0.2, W / 2 + 0.1, -H / 2 - t), M['frame'], bevel=0.006)


def build_window_188_a():
    """Fenster mit fast geschlossener Jalousie (Wiki: die meisten Fenster haben Jalousien)."""
    _window(_m(), blinds=0.85)


def build_window_188_b():
    """Fenster mit halb herabgelassener Jalousie."""
    _window(_m(), blinds=0.45)


def build_window_188_c():
    """Fenster ohne Jalousie, dahinter Dunkel."""
    _window(_m(), inside='dark')


def build_window_188_fig():
    """Fenster ohne Jalousie, im schwachen Licht dahinter ein dunkler Umriss (Wiki: Schattengestalten in den Fenstern)."""
    dim = common.material('raum_licht', (0.80, 0.76, 0.66), look='glow', glow=0.35)
    _window(_m(), figure=True, glow=dim)


def build_window_188_lit():
    """Fenster mit halber Jalousie, dahinter schwaches Licht (ein Raum, in dem jemand sein koennte)."""
    dim = common.material('raum_licht', (0.80, 0.76, 0.66), look='glow', glow=0.35)
    _window(_m(), blinds=0.5, glow=dim)


def build_window_entry_188():
    """Das Eingangsfenster: dahinter warmes Licht des Hotels (Rueckweg nach Level 5), Jalousie oben."""
    M = _m()
    warm = common.material('hotellicht', (1.0, 0.72, 0.48), look='glow', glow=1.6)
    _window(M, blinds=0.15, glow=warm)


# --- Tueren ---------------------------------------------------------------------------------------------------------------
def _door_leaf(M, W_=0.94, H_=2.2):
    common.box('blatt', (0.0, -W_ / 2, 0.0), (0.045, W_ / 2, H_), M['mahogany'], bevel=0.004)
    for (z0, z1) in ((0.15, 0.95), (1.1, 2.05)):
        for (y0, y1) in ((-W_ / 2 + 0.1, -0.03), (0.03, W_ / 2 - 0.1)):
            common.box('kassette', (0.045, y0, z0), (0.055, y1, z1), M['mahogany_d'], bevel=0.012, segments=2)
    _cyl('knauf', (0.045, -W_ / 2 + 0.1, 1.0), (0.1, -W_ / 2 + 0.1, 1.0), 0.025, M['brass'], seg=14)


def build_door_mahogany():
    """Verschlossene Tuer aus Mahagoni (Wandteil, Boden z = 0; Wiki: die meisten Tueren sind von innen verschlossen)."""
    M = _m()
    W_, H_ = 0.94, 2.2
    for (lo, hi) in (((0.0, W_ / 2, 0.0), (0.08, W_ / 2 + 0.08, H_ + 0.08)), ((0.0, -W_ / 2 - 0.08, 0.0), (0.08, -W_ / 2, H_ + 0.08)),
                     ((0.0, -W_ / 2 - 0.08, H_), (0.08, W_ / 2 + 0.08, H_ + 0.08))):
        common.box('zarge', lo, hi, M['frame'], bevel=0.006)
    _door_leaf(M, W_, H_)


def build_door_mahogany_open():
    """Offene Mahagonituer im Durchbruch (Laibung x = -0,5 .. 0,5), Blatt nach +x aufgeschwenkt."""
    M = _m()
    W_, H_ = 1.0, 2.2
    for s in (-1, 1):
        common.box(f'pfosten{s}', (0.42, s * W_ / 2 - (0.08 if s > 0 else 0), 0.0), (0.5, s * W_ / 2 + (0.0 if s > 0 else 0.08), H_), M['frame'], bevel=0.005)
    common.box('sturz', (0.42, -W_ / 2, H_ - 0.06), (0.5, W_ / 2, H_ + 0.02), M['frame'], bevel=0.005)
    before = set(bpy.data.objects)
    _door_leaf(M, 0.96, H_ - 0.08)
    leaf = Matrix.Translation((0.46, -W_ / 2 + 0.49, 0.01)) @ Matrix.Rotation(math.radians(75), 4, 'Z') @ Matrix.Translation((0.0, 0.48, 0.0))
    for ob in set(bpy.data.objects) - before:
        ob.matrix_world = leaf @ ob.matrix_world


# --- Hof ------------------------------------------------------------------------------------------------------------------
def build_paving_strip():
    """Pflasterstreifen des Wegs (Boden, Mitte im Ursprung, 1 m entlang x, 1,6 m breit, 4 cm), Kantensteine."""
    M = _m()
    common.box('pflaster', (-0.5, -0.8, 0.0), (0.5, 0.8, 0.04), M['paving'])


def build_birch_circle():
    """Kreis aus hellem Birkenholz in der Mitte (Boden, Durchmesser 7 m, 12 cm hoch; Wiki: hohl): Dielen, Rand."""
    M = _m()
    R = 3.5
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=R, depth=0.12, location=(0, 0, 0.06))
    ob = bpy.context.active_object
    ob.data.materials.append(M['birch'])
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=0.05, major_segments=64, minor_segments=8, location=(0, 0, 0.09))
    bpy.context.active_object.data.materials.append(M['birch'])
    # Fugen der Dielen als dunkle Linien (leicht erhaben)
    seam = common.material('fuge', (0.42, 0.38, 0.30), look='paint', rough=0.8)
    for k in range(-6, 7):
        y = k * 0.5
        half = math.sqrt(max(0.0, R * R - y * y)) - 0.06
        if half > 0.1:
            common.box(f'fuge{k}', (-half, y - 0.004, 0.12), (half, y + 0.004, 0.122), seam)


def build_table_188():
    """Kleiner runder Tisch aus Mahagoni mit Gegenstaenden (Wiki: Tisch, alte Fotografien, Buecher, Vasen, Wecker -
    nicht beruehren)."""
    M = _m()
    rng = random.Random(188)
    _cyl('platte', (0, 0, 0.7), (0, 0, 0.74), 0.45, M['mahogany'], seg=32)
    _cyl('saeule', (0, 0, 0.06), (0, 0, 0.7), 0.05, M['mahogany_d'], seg=14)
    for k in range(3):
        a = k * 2 * math.pi / 3
        ob = common.box(f'fuss{k}', (0.0, -0.03, 0.0), (0.32, 0.03, 0.06), M['mahogany_d'], bevel=0.012)
        ob.matrix_world = Matrix.Rotation(a, 4, 'Z')
    # zwei Bilderrahmen mit alten Fotografien (sepia, eine Person als Umriss)
    sepia = common.material('foto', (0.62, 0.52, 0.38), look='paint', rough=0.6)
    for k, (x, y, yaw) in enumerate(((-0.18, 0.1, 0.3), (0.05, 0.26, -0.4))):
        R = Matrix.Translation((x, y, 0.74)) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(-0.25, 4, 'Y')
        ob = common.box(f'rahmen{k}', (-0.01, -0.08, 0.0), (0.0, 0.08, 0.2), M['brass'])
        ob.matrix_world = R @ ob.matrix_world
        ob = common.box(f'foto{k}', (0.0, -0.065, 0.015), (0.002, 0.065, 0.185), sepia)
        ob.matrix_world = R @ ob.matrix_world
        ob = common.box(f'person{k}', (0.002, -0.025, 0.02), (0.003, 0.025, 0.14), common.material('foto_dunkel', (0.28, 0.22, 0.16), look='paint', rough=0.6))
        ob.matrix_world = R @ ob.matrix_world
    # Buecherstapel
    for k in range(3):
        ob = common.box(f'buch{k}', (-0.1, -0.07, 0.0), (0.1, 0.07, 0.035), common.material(f'einband{k}', [(0.3, 0.1, 0.08), (0.12, 0.2, 0.14), (0.2, 0.16, 0.1)][k], look='fabric', rough=0.7), bevel=0.004)
        ob.matrix_world = Matrix.Translation((0.2, -0.15, 0.74 + k * 0.036)) @ Matrix.Rotation(rng.uniform(-0.3, 0.3), 4, 'Z')
    # Vase mit trockenen Zweigen
    bm = bmesh.new()
    rings = []
    for k in range(10):
        t = k / 9
        r = 0.045 + 0.04 * math.sin(math.pi * t * 0.9) - 0.02 * t
        rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / 20), r * math.sin(2 * math.pi * i / 20), t * 0.26)) for i in range(20)])
    for k in range(9):
        for i in range(20):
            j = (i + 1) % 20
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]))
    bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('vase', bm, common.material('porzellan', (0.80, 0.82, 0.86), look='ceramic', rough=0.2))
    ob.data.shade_smooth()
    ob.matrix_world = Matrix.Translation((-0.15, -0.2, 0.74))
    twig = common.material('zweig', (0.30, 0.24, 0.16), look='wood', rough=0.8)
    for k in range(5):
        a = k * 1.3
        _cyl(f'zweig{k}', (-0.15, -0.2, 0.95), (-0.15 + math.cos(a) * 0.12, -0.2 + math.sin(a) * 0.12, 1.25 + 0.05 * k), 0.004, twig, seg=5)
    # Wecker
    _cyl('wecker', (0.22, 0.12, 0.8), (0.26, 0.12, 0.8), 0.055, M['brass'], seg=24)
    _cyl('zifferblatt', (0.26, 0.12, 0.8), (0.262, 0.12, 0.8), 0.048, common.material('blatt_weiss', (0.92, 0.9, 0.84), look='paint', rough=0.5), seg=24)
    for s in (-1, 1):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6, radius=0.022, location=(0.24, 0.12 + s * 0.035, 0.86))
        bpy.context.active_object.data.materials.append(M['brass'])
        _cyl(f'wecker_fuss{s}', (0.24, 0.12 + s * 0.03, 0.74), (0.24, 0.12 + s * 0.035, 0.75), 0.008, M['brass'], seg=6)


def build_walkway_glass():
    """Galerie mit Glasgelaender an der Wand (Wandteil, 1 m entlang y, Boden der Galerie bei z = 0, ragt 1,6 m vor;
    Wiki: Glasgelaender und Stege erscheinen)."""
    M = _m()
    common.box('steg', (0.0, -0.5, -0.25), (1.6, 0.5, 0.0), M['concrete'])
    common.box('glas', (1.52, -0.49, 0.0), (1.55, 0.49, 1.05), M['clear'])
    common.box('handlauf', (1.5, -0.5, 1.05), (1.57, 0.5, 1.09), M['steel'], bevel=0.01)
    _cyl('halter', (1.54, 0.0, 0.0), (1.54, 0.0, 1.05), 0.012, M['steel'], seg=8)


def build_pillar_188():
    """Weisse runde Stuetze (Boden, 30 m hoch; Wiki: Saeulen erscheinen)."""
    M = _m()
    _cyl('schaft', (0, 0, 0), (0, 0, 30.0), 0.45, M['concrete'], seg=32)
    _cyl('fuss', (0, 0, 0), (0, 0, 0.15), 0.55, M['concrete'], seg=32)


def build_skylight_188():
    """Oberlicht (Deckenteil, Decke bei z = 0, 8 x 8 m): mattes Glas mit kaltem Tageslicht, Stahlsprossen alle 1 m
    (eigene Ergaenzung: die Seite nennt keine Lichtquelle des Hofs)."""
    M = _m()
    sky = common.material('oberlicht', (0.92, 0.96, 1.0), look='glow', glow=1.1)
    common.box('glas', (-3.9, -3.9, -0.04), (3.9, 3.9, -0.02), sky)
    for k in range(9):
        t = -4.0 + k * 1.0
        common.box(f'sprosse_x{k}', (t - 0.03, -4.0, -0.1), (t + 0.03, 4.0, 0.0), M['steel'])
        common.box(f'sprosse_y{k}', (-4.0, t - 0.03, -0.1), (4.0, t + 0.03, 0.0), M['steel'])
