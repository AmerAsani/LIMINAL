"""V12 Level 5.3 (LOOK_5.3.md): Promethei Bibliotheca - Regalwand mit drei Geschossen und Galerien, Regale der Gaenge,
Tuffsaeule, Kandelaber, eiserner Kronleuchter (Metall wie die Gitter von Level 3, Wiki), Gemaelde im Stil der
Renaissance, Wappen an der Decke, heruntergefallene Buecher, Inschrift, sehr alte Holztuer, abgeloeste Tapete.

Buchruecken: eigene Textur books_spines (tools/textures/generate.py), aus Weltkoordinaten projiziert. Eine Kachel
(0,75 m) hat zwei Reihen; die Fachboeden liegen deshalb auf Vielfachen von 0,375 m ueber dem Raumboden, und die
Raumboeden der Bibliothek auf Vielfachen von 0,75 m.
Schrift der Inschrift: Inter (SIL OFL 1.1) aus den Blender-Datendateien.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common
from models.models_l2 import _cyl
from models.models_l5 import _text

ROW = 0.375
WHITE_RUST = (0.84, 0.82, 0.76)  # wie die Gitter von Level 3 (models_l3)


def _m():
    return {
        'walnut': common.material('nussbaum', (0.42, 0.26, 0.16), look='wood', tex='wood_walnut_floor', rough=0.45),
        'walnut_d': common.material('nussbaum_dunkel', (0.30, 0.18, 0.11), look='wood', tex='wood_walnut_floor', rough=0.5),
        'books': common.material('buecher', (0.92, 0.92, 0.92), look='fabric', tex='books_spines', rough=0.7),
        'iron': common.material('schmiedeeisen', (0.08, 0.08, 0.08), look='metal', rough=0.55, metal=0.6),
        'brass': common.material('messing', (0.80, 0.62, 0.30), look='metal', rough=0.3, metal=1.0),
        'bronze': common.material('bronze', (0.55, 0.40, 0.20), look='metal', rough=0.4, metal=1.0),
        'gold': common.material('gold', (0.88, 0.70, 0.34), look='metal', rough=0.25, metal=1.0),
        'bar': common.material('gitter', WHITE_RUST, look='paint', rough=0.55),
        'rust': common.material('rost', (0.42, 0.26, 0.15), look='metal', tex='metal_rusty_iron', rough=0.8, metal=0.4),
        'cream': common.material('creme', (0.86, 0.80, 0.66), look='paint', rough=0.6),
        'tuff': common.material('tuff', (0.95, 0.95, 0.95), look='concrete', tex='tuff_stone', rough=0.9),
        'marble': common.material('marmor', (0.95, 0.95, 0.95), look='ceramic', tex='marble_white', rough=0.3),
        'granite': common.material('granit', (0.22, 0.21, 0.21), look='ceramic', rough=0.35),
        'paper': common.material('papier', (0.86, 0.82, 0.70), look='paint', rough=0.85),
        'black': common.material('schwarz', (0.04, 0.04, 0.04), look='paint', rough=0.4),
    }


LEATHER = [(0.36, 0.08, 0.06), (0.30, 0.17, 0.09), (0.12, 0.22, 0.13), (0.10, 0.13, 0.24), (0.07, 0.06, 0.05), (0.48, 0.34, 0.16)]


def _leather(k):
    return common.material(f'leder{k}', LEATHER[k % len(LEATHER)], look='fabric', rough=0.7)


# --- Regale -------------------------------------------------------------------------------------------------------------
def _tier(z0, M, rng, depth=0.42, plinth=True, crown=False):
    """Ein Regalgeschoss (3,0 m) vor der Wand x = 0, 1 m entlang y: Rueckwand, Seiten (halb, Nachbar ergaenzt),
    Fachboeden auf z0 + k * 0,375, Buchbloecke mit Buchruecken-Textur, einzelne lose Buecher."""
    z1 = z0 + 8 * ROW
    common.box('rueck', (0.0, -0.5, z0), (0.02, 0.5, z1), M['walnut_d'])
    common.box('seite_a', (0.0, -0.5, z0), (depth, -0.48, z1), M['walnut'])
    common.box('seite_b', (0.0, 0.48, z0), (depth, 0.5, z1), M['walnut'])
    if plinth:
        common.box('sockel', (0.0, -0.5, z0), (depth + 0.015, 0.5, z0 + ROW), M['walnut_d'], bevel=0.006)
        common.box('sockel_fuell', (depth + 0.015, -0.4, z0 + 0.08), (depth + 0.025, 0.4, z0 + ROW - 0.06), M['walnut'], bevel=0.008)
    for k in range(1, 9):
        z = z0 + k * ROW
        common.box(f'boden{k}', (0.02, -0.48, z - 0.025), (depth, 0.48, z), M['walnut'])
        common.box(f'kante{k}', (depth - 0.012, -0.48, z - 0.04), (depth + 0.006, 0.48, z), M['walnut_d'])
    for k in range(1 if plinth else 0, 8):
        zb = z0 + k * ROW
        front = 0.30 if k == 3 else 0.33  # Fach 3 (1,1 m): Ablage vor den Buechern
        if rng.random() < 0.22:  # Luecke mit schraeg lehnendem Buch
            g = rng.uniform(-0.3, 0.25)
            gw = rng.uniform(0.1, 0.18)
            if g > -0.46:
                common.box(f'buecher{k}a', (0.03, -0.48, zb), (front, g, zb + 0.34), M['books'])
            if g + gw < 0.46:
                common.box(f'buecher{k}b', (0.03, g + gw, zb), (front, 0.48, zb + 0.34), M['books'])
            ob = common.box(f'lehnt{k}', (0.0, 0.0, 0.0), (0.22, 0.035, 0.27), _leather(rng.randrange(6)))
            ob.matrix_world = Matrix.Translation((0.06, g + 0.01, zb)) @ Matrix.Rotation(-0.35, 4, 'X')
        else:
            common.box(f'buecher{k}', (0.03, -0.48, zb), (front, 0.48, zb + 0.34), M['books'])
        if rng.random() < 0.12:  # ein Buch liegt quer obenauf
            y = rng.uniform(-0.3, 0.2)
            common.box(f'quer{k}', (0.06, y, zb + 0.34), (0.28, y + 0.22, zb + 0.37), _leather(rng.randrange(6)))
    if crown:
        common.box('kranz', (0.0, -0.5, z1), (depth + 0.06, 0.5, z1 + 0.08), M['walnut_d'], bevel=0.01, segments=2)
        common.box('kranz_o', (0.0, -0.5, z1 + 0.08), (depth + 0.1, 0.5, z1 + 0.16), M['walnut'], bevel=0.015, segments=2)


def _gallery(zg, M, out=1.2):
    """Galerie vor dem Regal: Boden (Nussbaum), Stirnbrett, Konsole, Gelaender aus Schmiedeeisen mit Messinghandlauf."""
    common.box('galerie', (0.0, -0.5, zg - 0.14), (out, 0.5, zg), M['walnut_d'], bevel=0.005)
    common.box('stirn', (out - 0.03, -0.5, zg - 0.32), (out + 0.01, 0.5, zg - 0.1), M['walnut'], bevel=0.006, segments=2)
    common.box('stirn_gold', (out + 0.01, -0.5, zg - 0.22), (out + 0.016, 0.5, zg - 0.2), M['gold'])
    ob = common.box('konsole', (0.0, -0.03, -0.6), (0.06, 0.03, 0.0), M['walnut_d'], bevel=0.008)
    ob.matrix_world = Matrix.Translation((0.05, 0.0, zg - 0.14)) @ Matrix.Rotation(-0.85, 4, 'Y')
    for k in range(8):
        y = -0.5 + 0.125 * (k + 0.5)
        _cyl(f'stab{k}', (out - 0.06, y, zg), (out - 0.06, y, zg + 0.95), 0.011, M['iron'], seg=5)
    common.box('handlauf', (out - 0.1, -0.5, zg + 0.95), (out - 0.02, 0.5, zg + 1.0), M['brass'], bevel=0.012, segments=2)
    common.box('fussleiste', (out - 0.08, -0.5, zg + 0.04), (out - 0.04, 0.5, zg + 0.08), M['iron'])
    # Zierbogen zwischen den Staeben
    for k in range(4):
        y = -0.5 + 0.25 * (k + 0.5)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.07, minor_radius=0.006, major_segments=10, minor_segments=3,
                                         location=(out - 0.06, y, zg + 0.82), rotation=(0, math.pi / 2, 0))
        bpy.context.active_object.data.materials.append(M['iron'])


def _cornice(z, M, depth=0.42):
    prof = [(0.0, 0.0), (depth + 0.05, 0.0), (depth + 0.05, 0.06)]
    for k in range(7):
        a = math.pi / 2 * k / 6
        prof.append((depth + 0.05 + 0.18 * math.sin(a), 0.06 + 0.18 * (1 - math.cos(a))))
    prof += [(depth + 0.26, 0.24), (depth + 0.26, 0.32), (0.0, 0.32)]
    ob = common.profile_extrude('gesims', [(u, z + w) for (u, w) in prof], 1.0, M['walnut_d'], axis='y')
    ob.location = (0.0, -0.5, 0.0)


def build_library_wall():
    """Regalwand der grossen Halle (Wandteil, Wandflaeche x = 0, 1 m entlang y, Boden z = 0): drei Geschosse Regale
    (je 3,0 m), Galerien bei 3,0 und 6,0 m, Gesims bis 9,3 m (Wiki-Bild: drei Stockwerke Regale)."""
    M = _m()
    rng = random.Random(5301)
    for t in range(3):
        _tier(t * 3.0, M, rng, plinth=(t == 0))
    for zg in (3.0, 6.0):
        _gallery(zg, M)
    _cornice(9.0, M)


def _rail_candles(zg, M, out=1.2):
    """Dreiarmiger Kerzenhalter aus Messing, an den Handlauf der Galerie geklemmt (Licht fuer die Regale)."""
    flame = common.material('flamme', (1.0, 0.80, 0.50), look='glow', glow=3.5)
    wax = common.material('wachs', (0.90, 0.86, 0.74), look='paint', rough=0.7)
    x = out - 0.06
    common.box('klemme', (x - 0.05, -0.04, zg + 0.9), (x + 0.05, 0.04, zg + 1.03), M['brass'], bevel=0.006)
    _cyl('halter', (x, 0.0, zg + 1.03), (x, 0.0, zg + 1.12), 0.012, M['brass'], seg=8)
    for k, y in enumerate((-0.12, 0.0, 0.12)):
        _cyl(f'arm{k}', (x, 0.0, zg + 1.1), (x, y, zg + 1.14), 0.008, M['brass'], seg=6)
        _cyl(f'teller{k}', (x, y, zg + 1.14), (x, y, zg + 1.155), 0.03, M['brass'], seg=10)
        _cyl(f'kerze{k}', (x, y, zg + 1.155), (x, y, zg + 1.3), 0.011, wax, seg=8)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=6, radius=0.013, location=(x, y, zg + 1.325))
        ob = bpy.context.active_object
        ob.scale = (1, 1, 1.9)
        ob.data.materials.append(flame)


def build_library_wall_lamp():
    """Wie library_wall, dazu Kerzenhalter an den Handlaeufen beider Galerien (jedes vierte Wandstueck)."""
    M = _m()
    rng = random.Random(5301)
    for t in range(3):
        _tier(t * 3.0, M, rng, plinth=(t == 0))
    for zg in (3.0, 6.0):
        _gallery(zg, M)
        _rail_candles(zg, M)
    _cornice(9.0, M)


def build_library_wall_upper():
    """Wie library_wall, aber erst ab der ersten Galerie (Ursprung auf der Galerie, z = 0 = 3,0 m ueber dem Boden):
    ueber Durchgaengen in die Gaenge."""
    M = _m()
    rng = random.Random(5302)
    for t in range(2):
        _tier(t * 3.0, M, rng, plinth=False)
    for zg in (0.0, 3.0):
        _gallery(zg, M)
    _cornice(6.0, M)


def build_bookcase_aisle():
    """Regal der Gaenge (Wandteil, 1 m, 3,0 m hoch mit Kranz)."""
    M = _m()
    _tier(0.0, M, random.Random(5303), crown=True)


def build_bookcase_aisle_b():
    """Wie bookcase_aisle, andere Verteilung der Luecken und losen Buecher."""
    M = _m()
    _tier(0.0, M, random.Random(5304), crown=True)


# --- Saeule, Kandelaber, Kronleuchter ----------------------------------------------------------------------------------
def build_column_tuff():
    """Saeule aus Tuff (Wiki: unbekannter Tuffstein), 10,2 m: Plinthe, Wulst, leicht verjuengter Schaft, Hals,
    Echinus und Abakus (toskanisch)."""
    M = _m()
    H = 10.2
    common.box('plinthe', (-0.45, -0.45, 0.0), (0.45, 0.45, 0.3), M['tuff'], bevel=0.02, segments=2)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.34, minor_radius=0.07, major_segments=32, minor_segments=10, location=(0, 0, 0.37))
    bpy.context.active_object.data.materials.append(M['tuff'])
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.34, radius2=0.29, depth=H - 1.2, location=(0, 0, 0.42 + (H - 1.2) / 2))
    ob = bpy.context.active_object
    ob.data.materials.append(M['tuff'])
    common.shade_auto(ob, 40.0)
    z = 0.42 + H - 1.2
    bpy.ops.mesh.primitive_torus_add(major_radius=0.3, minor_radius=0.035, major_segments=32, minor_segments=8, location=(0, 0, z + 0.03))
    bpy.context.active_object.data.materials.append(M['tuff'])
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.3, radius2=0.44, depth=0.25, location=(0, 0, z + 0.2))
    ob = bpy.context.active_object
    ob.data.materials.append(M['tuff'])
    common.shade_auto(ob, 40.0)
    common.box('abakus', (-0.48, -0.48, z + 0.32), (0.48, 0.48, H), M['tuff'], bevel=0.02, segments=2)


def build_candelabrum():
    """Stehender Kandelaber aus Bronze (1,95 m): Dreifuss, Schaft mit Knaeufen, vier Arme und Mitte mit Kerzen
    (Wiki-Bild: Kandelaber um Saeulen und Regale)."""
    M = _m()
    flame = common.material('flamme', (1.0, 0.80, 0.50), look='glow', glow=3.5)
    wax = common.material('wachs', (0.90, 0.86, 0.74), look='paint', rough=0.7)
    for k in range(3):
        a = k * 2 * math.pi / 3
        ob = common.box(f'fuss{k}', (0.0, -0.025, 0.0), (0.3, 0.025, 0.06), M['bronze'], bevel=0.012)
        ob.matrix_world = Matrix.Rotation(a, 4, 'Z') @ Matrix.Translation((0.02, 0, 0.02)) @ Matrix.Rotation(-0.18, 4, 'Y')
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6, radius=0.035, location=(math.cos(a) * 0.3, math.sin(a) * 0.3, 0.035))
        bpy.context.active_object.data.materials.append(M['bronze'])
    _cyl('schaft', (0, 0, 0.05), (0, 0, 1.55), 0.022, M['bronze'], seg=12)
    for (z, r) in ((0.12, 0.07), (0.6, 0.05), (1.05, 0.045), (1.5, 0.06)):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=8, radius=r, location=(0, 0, z))
        ob = bpy.context.active_object
        ob.scale = (1, 1, 0.7)
        ob.data.materials.append(M['bronze'])
    tips = [(0.0, 0.0, 1.62)]
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        x, y = math.cos(a) * 0.26, math.sin(a) * 0.26
        _cyl(f'arm{k}', (0, 0, 1.48), (x * 0.6, y * 0.6, 1.44), 0.012, M['bronze'], seg=6)
        _cyl(f'arm_b{k}', (x * 0.6, y * 0.6, 1.44), (x, y, 1.55), 0.012, M['bronze'], seg=6)
        tips.append((x, y, 1.55))
    for k, (x, y, z) in enumerate(tips):
        _cyl(f'teller{k}', (x, y, z), (x, y, z + 0.02), 0.045, M['bronze'], seg=12)
        _cyl(f'kerze{k}', (x, y, z + 0.02), (x, y, z + 0.22), 0.016, wax, seg=10)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=8, radius=0.016, location=(x, y, z + 0.25))
        ob = bpy.context.active_object
        ob.scale = (1, 1, 1.9)
        ob.data.materials.append(flame)


def build_chandelier_iron():
    """Eiserner Kronleuchter (Deckenebene z = 0, haengt 2,2 m herab), Metall wie die Gitter von Level 3 (Wiki):
    weiss gestrichenes, rostiges Eisen, zwei Reifen mit Kerzen."""
    M = _m()
    flame = common.material('flamme', (1.0, 0.80, 0.50), look='glow', glow=3.5)
    wax = common.material('wachs', (0.90, 0.86, 0.74), look='paint', rough=0.7)
    _cyl('kette', (0, 0, -0.9), (0, 0, 0.0), 0.014, M['bar'], seg=8)
    _cyl('schaft', (0, 0, -1.95), (0, 0, -0.9), 0.04, M['bar'], seg=12)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=0.11, location=(0, 0, -1.95))
    bpy.context.active_object.data.materials.append(M['rust'])
    rng = random.Random(77)
    for (zr, R, n) in ((-1.85, 1.0, 12), (-1.35, 0.6, 8)):
        bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=0.025, major_segments=48, minor_segments=8, location=(0, 0, zr))
        bpy.context.active_object.data.materials.append(M['bar'])
        for k in range(n):
            a = 2 * math.pi * k / n
            x, y = math.cos(a) * R, math.sin(a) * R
            if k % (n // 4) == 0:
                _cyl(f'strebe{zr}{k}', (0, 0, zr + 0.5), (x, y, zr), 0.012, M['rust'] if rng.random() < 0.3 else M['bar'], seg=6)
            _cyl(f'teller{zr}{k}', (x, y, zr + 0.02), (x, y, zr + 0.04), 0.045, M['bar'], seg=10)
            _cyl(f'kerze{zr}{k}', (x, y, zr + 0.04), (x, y, zr + 0.2), 0.015, wax, seg=8)
            bpy.ops.mesh.primitive_uv_sphere_add(segments=8, ring_count=6, radius=0.015, location=(x, y, zr + 0.23))
            ob = bpy.context.active_object
            ob.scale = (1, 1, 1.9)
            ob.data.materials.append(flame)


# --- Bilder ------------------------------------------------------------------------------------------------------------------
def _poly(name, pts, mat, d, x0=0.021):
    """Ebene Flaeche in der yz-Ebene bei x = x0 + d, Blick entlang +x."""
    bm = bmesh.new()
    f = bm.faces.new([bm.verts.new((x0 + d, y, z)) for (y, z) in pts])
    f.normal_update()
    if f.normal.x < 0:
        f.normal_flip()
    common.obj_from_bmesh(name, bm, mat)


def _frame(W, H, M, fw=0.1):
    for (lo, hi) in (((0.0, -W / 2 - fw, H / 2), (0.07, W / 2 + fw, H / 2 + fw)), ((0.0, -W / 2 - fw, -H / 2 - fw), (0.07, W / 2 + fw, -H / 2)),
                     ((0.0, W / 2, -H / 2), (0.07, W / 2 + fw, H / 2)), ((0.0, -W / 2 - fw, -H / 2), (0.07, -W / 2, H / 2))):
        common.box('rahmen', lo, hi, M['gold'], bevel=0.025, segments=2)


def _figure(name, y, z0, h, robe, skin, d, lean=0.0):
    """Stehende Figur im Gewand (Umriss), Kopf, Arm; y Mitte, z0 Fuss, h Hoehe."""
    w = h * 0.24
    pts = [(y - w * 0.55, z0), (y + w * 0.55, z0), (y + w * 0.42 + lean, z0 + h * 0.55), (y + w * 0.36 + lean, z0 + h * 0.8),
           (y + w * 0.12 + lean, z0 + h * 0.86), (y - w * 0.12 + lean, z0 + h * 0.86), (y - w * 0.36 + lean, z0 + h * 0.8),
           (y - w * 0.42 + lean, z0 + h * 0.55)]
    _poly(name, pts, robe, d)
    hc = (y + lean * 1.1, z0 + h * 0.92)
    _poly(name + '_kopf', [(hc[0] + math.cos(a) * h * 0.065, hc[1] + math.sin(a) * h * 0.08) for a in [k * 2 * math.pi / 14 for k in range(14)]], skin, d + 0.0005)
    _poly(name + '_arm', [(y + w * 0.3 + lean, z0 + h * 0.78), (y + w * 0.75 + lean, z0 + h * 0.6), (y + w * 0.68 + lean, z0 + h * 0.55),
                          (y + w * 0.22 + lean, z0 + h * 0.7)], robe, d + 0.0004)


def _sky(W, H, d, top, mid, low):
    for k, (c, z0, z1) in enumerate(((top, H * 0.15, H / 2), (mid, -H * 0.05, H * 0.15), (low, -H * 0.2, -H * 0.05))):
        _poly(f'himmel{k}', [(-W / 2, z0), (W / 2, z0), (W / 2, z1), (-W / 2, z1)],
              common.material(f'himmel{k}', c, look='paint', rough=0.7), d)


def build_painting_renaissance_a():
    """Gemaelde im Stil der Renaissance (Wiki: ueber den Regalen), 1,6 x 1,05 m, Goldrahmen: Landschaft mit Huegeln,
    Tempel mit Saeulen links, fuenf Gestalten in Gewaendern, nachgedunkelter Firnis."""
    M = _m()
    W, H = 1.6, 1.05
    _frame(W, H, M)
    _poly('grund', [(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, H / 2), (-W / 2, H / 2)], common.material('firnis', (0.16, 0.13, 0.09), look='paint', rough=0.6), 0.0)
    _sky(W, H, 0.0004, (0.30, 0.38, 0.44), (0.52, 0.52, 0.44), (0.58, 0.48, 0.32))
    hill = common.material('huegel', (0.24, 0.26, 0.16), look='paint', rough=0.7)
    _poly('huegel', [(-W / 2, -H * 0.2)] + [(-W / 2 + W * t, -H * 0.2 + 0.09 * math.sin(math.pi * t * 2.2) + 0.04) for t in [k / 12 for k in range(13)]] + [(W / 2, -H * 0.2)], hill, 0.0008)
    ground = common.material('boden_bild', (0.30, 0.22, 0.13), look='paint', rough=0.7)
    _poly('boden', [(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, -H * 0.2), (-W / 2, -H * 0.2)], ground, 0.001)
    stone = common.material('tempel', (0.70, 0.64, 0.52), look='paint', rough=0.7)
    _poly('tempel_dach', [(-W / 2 + 0.04, H * 0.18), (-W / 2 + 0.48, H * 0.18), (-W / 2 + 0.26, H * 0.32)], stone, 0.0012)
    _poly('tempel_gebaelk', [(-W / 2 + 0.04, H * 0.12), (-W / 2 + 0.48, H * 0.12), (-W / 2 + 0.48, H * 0.18), (-W / 2 + 0.04, H * 0.18)], stone, 0.0012)
    for k in range(4):
        y = -W / 2 + 0.08 + k * 0.12
        _poly(f'saeule{k}', [(y, -H * 0.3), (y + 0.04, -H * 0.3), (y + 0.04, H * 0.12), (y, H * 0.12)], stone, 0.0013)
    skin = common.material('haut', (0.66, 0.50, 0.38), look='paint', rough=0.7)
    robes = [(0.50, 0.10, 0.08), (0.14, 0.20, 0.42), (0.58, 0.44, 0.18), (0.20, 0.32, 0.20), (0.44, 0.40, 0.36)]
    for k, (y, h, lean) in enumerate(((-0.05, 0.62, 0.02), (0.14, 0.66, -0.01), (0.32, 0.6, 0.03), (0.5, 0.64, -0.02), (0.66, 0.5, 0.0))):
        _figure(f'figur{k}', y, -H * 0.42, h, common.material(f'gewand{k}', robes[k], look='paint', rough=0.7), skin, 0.002 + k * 0.0003, lean)


def build_painting_renaissance_b():
    """Gemaelde im Stil der Renaissance, 1,6 x 1,05 m: Arkade mit drei Boegen, in der Mitte eine sitzende Gestalt mit
    offenem Buch, Zuhoerer zu beiden Seiten, Schachbrettboden."""
    M = _m()
    W, H = 1.6, 1.05
    _frame(W, H, M)
    wall = common.material('arkade', (0.58, 0.50, 0.38), look='paint', rough=0.7)
    _poly('grund', [(-W / 2, -H / 2), (W / 2, -H / 2), (W / 2, H / 2), (-W / 2, H / 2)], wall, 0.0)
    dark = common.material('bogen_innen', (0.10, 0.09, 0.08), look='paint', rough=0.7)
    for k in range(3):
        cy = -0.5 + k * 0.5
        pts = [(cy - 0.17, -H * 0.2), (cy + 0.17, -H * 0.2)]
        pts += [(cy + 0.17 * math.cos(a), H * 0.22 + 0.17 * math.sin(a)) for a in [j * math.pi / 10 for j in range(11)]]
        _poly(f'bogen{k}', pts, dark, 0.0005)
    sky = common.material('himmel_b', (0.36, 0.44, 0.50), look='paint', rough=0.7)
    pts = [(-0.12, H * 0.0), (0.12, H * 0.0)] + [(0.12 * math.cos(a), H * 0.22 + 0.12 * math.sin(a)) for a in [j * math.pi / 10 for j in range(11)]]
    _poly('himmel_mitte', pts, sky, 0.0007)
    w1 = common.material('fliese_hell', (0.74, 0.70, 0.60), look='paint', rough=0.7)
    w2 = common.material('fliese_dunkel', (0.22, 0.18, 0.14), look='paint', rough=0.7)
    for i in range(10):
        for j in range(3):
            y0, z0 = -W / 2 + i * W / 10, -H / 2 + j * 0.1
            _poly(f'fliese{i}{j}', [(y0, z0), (y0 + W / 10, z0), (y0 + W / 10, z0 + 0.1), (y0, z0 + 0.1)], w1 if (i + j) % 2 else w2, 0.0006)
    skin = common.material('haut', (0.66, 0.50, 0.38), look='paint', rough=0.7)
    gold = common.material('thron', (0.62, 0.48, 0.22), look='paint', rough=0.6)
    _poly('thron', [(-0.13, -H * 0.22), (0.13, -H * 0.22), (0.13, H * 0.12), (-0.13, H * 0.12)], gold, 0.0009)
    _figure('mitte', 0.0, -H * 0.22, 0.42, common.material('gewand_mitte', (0.48, 0.10, 0.10), look='paint', rough=0.7), skin, 0.0011)
    _poly('buch', [(-0.07, -H * 0.02), (0.07, -H * 0.02), (0.06, H * 0.03), (-0.06, H * 0.03)], common.material('seiten', (0.88, 0.84, 0.72), look='paint', rough=0.7), 0.0014)
    robes = [(0.14, 0.20, 0.42), (0.20, 0.32, 0.20), (0.58, 0.44, 0.18), (0.30, 0.16, 0.30)]
    for k, y in enumerate((-0.62, -0.42, 0.42, 0.62)):
        _figure(f'zuh{k}', y, -H * 0.45, 0.55, common.material(f'gewand_z{k}', robes[k], look='paint', rough=0.7), skin, 0.0016 + k * 0.0003, 0.02 if y < 0 else -0.02)


def build_ceiling_arms():
    """Gemaltes Wappen an der Decke (Deckenteil, Decke bei z = 0, 1,6 x 1,6 m; Wiki-Bild: Wappen an der Decke der
    alten Gaenge): Stuckfeld, Goldring, Lorbeer, geviertelter Schild mit Sparren, Krone."""
    def poly(name, pts, mat, d):
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((x, y, -0.004 - d)) for (x, y) in pts])
        f.normal_update()
        if f.normal.z > 0:
            f.normal_flip()
        common.obj_from_bmesh(name, bm, mat)
    M = _m()
    poly('feld', [(-0.8, -0.8), (0.8, -0.8), (0.8, 0.8), (-0.8, 0.8)], common.material('stuckfeld', (0.80, 0.74, 0.62), look='paint', rough=0.8), 0.0)
    ring_o = [(math.cos(a) * 0.72, math.sin(a) * 0.72) for a in [k * 2 * math.pi / 48 for k in range(48)]]
    poly('ring', ring_o, common.material('goldring', (0.70, 0.54, 0.24), look='paint', rough=0.5), 0.0005)
    poly('ring_innen', [(math.cos(a) * 0.66, math.sin(a) * 0.66) for a in [k * 2 * math.pi / 48 for k in range(48)]],
         common.material('grund_wappen', (0.20, 0.24, 0.30), look='paint', rough=0.8), 0.001)
    leaf = common.material('lorbeer', (0.24, 0.32, 0.16), look='paint', rough=0.8)
    for k in range(24):
        a = k * 2 * math.pi / 24
        cx, cy = math.cos(a) * 0.58, math.sin(a) * 0.58
        poly(f'blatt{k}', [(cx + math.cos(a + s) * 0.06 * (1 if s == 0 else 0.4), cy + math.sin(a + s) * 0.06 * (1 if s == 0 else 0.4))
                           for s in (0.0, 1.6, math.pi, -1.6)], leaf, 0.0015)
    # Schild: oben gerade, unten zur Spitze gerundet (Bild: x nach rechts, y nach oben)
    red = common.material('rot_wappen', (0.55, 0.10, 0.08), look='paint', rough=0.6)
    gold = common.material('gold_wappen', (0.80, 0.64, 0.28), look='paint', rough=0.5)
    blue = common.material('blau_wappen', (0.12, 0.20, 0.45), look='paint', rough=0.6)
    silver = common.material('silber_wappen', (0.80, 0.80, 0.78), look='paint', rough=0.5)
    outline = [(-0.32, 0.36), (0.32, 0.36), (0.32, 0.0)] + [(0.32 * math.cos(a), -0.42 * math.sin(a) * 0.95) for a in [k * math.pi / 16 for k in range(1, 16)]] + [(-0.32, 0.0)]
    poly('schild_rand', [(x * 1.08, y * 1.06 + 0.005) for (x, y) in outline], gold, 0.002)
    quads = [((-0.3, 0.02), (0.0, 0.34), red), ((0.0, 0.02), (0.3, 0.34), gold), ((-0.3, -0.3), (0.0, 0.02), gold), ((0.0, -0.3), (0.3, 0.02), blue)]
    poly('schild', outline, silver, 0.0025)
    for k, ((x0, y0), (x1, y1), mat) in enumerate(quads):
        pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
        if y0 < 0:  # unten zur Spitze hin beschneiden
            pts = [(x0, y1), (x1, y1)] + [(x, -0.42 * math.sqrt(max(0.0, 1 - (x / 0.32) ** 2)) * 0.95 * 0.75) for x in ([x1, (x0 + x1) / 2, x0])]
        poly(f'feld{k}', pts, mat, 0.003)
    poly('sparren', [(-0.3, -0.18), (0.0, 0.14), (0.3, -0.18), (0.3, -0.08), (0.0, 0.24), (-0.3, -0.08)], silver, 0.0035)
    # Krone ueber dem Schild
    crown = [(-0.2, 0.42), (0.2, 0.42), (0.22, 0.56), (0.12, 0.48), (0.06, 0.6), (0.0, 0.5), (-0.06, 0.6), (-0.12, 0.48), (-0.22, 0.56)]
    poly('krone', crown, gold, 0.003)


# --- Buecher, Inschrift, Tuer, Tapete ---------------------------------------------------------------------------------------
def build_books_fallen():
    """Heruntergefallene Buecher (Boden, Mitte im Ursprung): sieben Baende verstreut, einer aufgeschlagen
    (Wiki: der Library Mind laesst Buecher fallen)."""
    M = _m()
    rng = random.Random(5305)
    for k in range(7):
        w, d, t = rng.uniform(0.15, 0.24), rng.uniform(0.22, 0.32), rng.uniform(0.03, 0.06)
        ob = common.box(f'buch{k}', (-d / 2, -w / 2, 0.0), (d / 2, w / 2, t), _leather(rng.randrange(6)), bevel=0.004)
        x, y = rng.uniform(-0.5, 0.5), rng.uniform(-0.45, 0.45)
        tilt = 0.0 if rng.random() < 0.7 else 0.25
        ob.matrix_world = Matrix.Translation((x, y, 0.0 if tilt == 0.0 else 0.02)) @ Matrix.Rotation(rng.uniform(0, 3.1), 4, 'Z') @ Matrix.Rotation(tilt, 4, 'X')
        common.box(f'seiten{k}', (-d / 2 + 0.01, -w / 2 + 0.008, 0.004), (d / 2 - 0.004, w / 2 - 0.008, t - 0.004), M['paper']).matrix_world = ob.matrix_world
    # aufgeschlagen: zwei Deckel, Seiten leicht gewoelbt
    for s in (-1, 1):
        ob = common.box(f'offen{s}', (0.0, 0.0, 0.0), (0.26, 0.19, 0.006), _leather(0))
        ob.matrix_world = Matrix.Translation((0.15, 0.25 + s * 0.0, 0.0)) @ Matrix.Rotation(0.4, 4, 'Z') @ Matrix.Translation((0.0, -0.19 if s < 0 else 0.0, 0.0))
        pg = common.box(f'blaetter{s}', (0.01, 0.005, 0.006), (0.25, 0.185, 0.022), M['paper'])
        pg.matrix_world = ob.matrix_world


def build_inscription_bibliotheca():
    """Inschrift "PROMETHEI BIBLIOTHECA" (Wiki: an einigen Waenden) auf Marmortafel mit Granitrahmen (Wandteil, Mitte
    im Ursprung, 3,4 x 0,56 m), Buchstaben vergoldet."""
    M = _m()
    common.box('rahmen', (0.0, -1.72, -0.28), (0.05, 1.72, 0.28), M['granite'], bevel=0.015, segments=2)
    common.box('tafel', (0.05, -1.62, -0.2), (0.07, 1.62, 0.2), M['marble'], bevel=0.006)
    _text('inschrift', 'PROMETHEI  BIBLIOTHECA', 0.2, M['gold'], Vector((0.071, 0.0, 0.0)), depth=0.004)


def build_door_old_open():
    """Sehr alte Holztuer im Durchbruch (Laibung x = -0,5 .. 0,5), offen nach +x: gerissene Bretter, Eisenbaender,
    Ringgriff (Wiki: am Ende des Eingangsgangs und als Ausgang in einer Sackgasse)."""
    M = _m()
    old = common.material('altholz', (0.30, 0.22, 0.15), look='wood', tex='wood_walnut_floor', rough=0.8)
    W, H = 1.0, 2.15
    for s in (-1, 1):
        common.box(f'pfosten{s}', (0.42, s * W / 2 - (0.08 if s > 0 else 0), 0.0), (0.5, s * W / 2 + (0.0 if s > 0 else 0.08), H), M['walnut_d'], bevel=0.006)
    common.box('sturz', (0.42, -W / 2, H - 0.08), (0.5, W / 2, H + 0.02), M['walnut_d'], bevel=0.006)
    rng = random.Random(9)
    leaf = Matrix.Translation((0.46, -W / 2 + 0.01, 0.01)) @ Matrix.Rotation(math.radians(78), 4, 'Z')
    for k in range(5):
        y0 = k * (W - 0.03) / 5
        ob = common.box(f'brett{k}', (0.0, y0 + 0.004, rng.uniform(0.0, 0.02)), (0.05, y0 + (W - 0.03) / 5 - 0.004, H - 0.08 - rng.uniform(0.0, 0.05)), old, bevel=0.004)
        ob.matrix_world = leaf @ ob.matrix_world
    for z in (0.3, 1.0, 1.75):
        ob = common.box(f'band{z}', (0.05, 0.0, z), (0.058, W - 0.15, z + 0.06), M['iron'], bevel=0.003)
        ob.matrix_world = leaf @ ob.matrix_world
    bpy.ops.mesh.primitive_torus_add(major_radius=0.06, minor_radius=0.008, major_segments=16, minor_segments=6,
                                     location=(0.07, W - 0.16, 1.0), rotation=(0, math.pi / 2, 0))
    ob = bpy.context.active_object
    ob.data.materials.append(M['iron'])
    ob.matrix_world = leaf @ ob.matrix_world


def build_wallpaper_peel():
    """Abgeloeste Tapete (Wandteil, 1 m; Wiki: im Eingangsgang war die Tapete alt und abgeblaettert): freiliegender
    Putz als unregelmaessiger Fleck, ein eingerollter Tapetenstreifen haengt herab."""
    damask = common.material('damast', (0.70, 0.66, 0.62), look='paint', tex='wallpaper_damask_red', rough=0.8)
    plaster = common.material('putz_alt', (0.62, 0.56, 0.46), look='concrete', rough=0.9)
    rng = random.Random(41)
    pts = []
    for k in range(18):
        a = 2 * math.pi * k / 18
        r = rng.uniform(0.75, 1.0)
        pts.append((math.cos(a) * 0.4 * r, 1.9 + math.sin(a) * 0.55 * r))
    _poly('putz', pts, plaster, 0.0, x0=0.002)
    # Streifen: von der Oberkante des Flecks herab, nach vorn gewoelbt und unten eingerollt
    bm = bmesh.new()
    rows = []
    for k in range(12):
        t = k / 11
        z = 2.4 - 0.9 * t
        x = 0.004 + 0.12 * t ** 2 + (0.05 * math.sin(math.pi * t) if t > 0.8 else 0)
        rows.append([bm.verts.new((x, y, z)) for y in (-0.16, 0.16)])
    for k in range(11):
        bm.faces.new((rows[k][0], rows[k][1], rows[k + 1][1], rows[k + 1][0]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('streifen', bm, damask)
    mod = ob.modifiers.new('dicke', 'SOLIDIFY')
    mod.thickness = 0.002


def build_bookcase_end():
    """Stirnbrett eines frei stehenden Regals (Wandteil, 1 m breit, 3,16 m hoch): Nussbaum mit Fuellung und Kranz."""
    M = _m()
    common.box('brett', (0.0, -0.5, 0.0), (0.03, 0.5, 3.16), M['walnut_d'])
    common.box('fuellung_u', (0.03, -0.36, 0.25), (0.045, 0.36, 1.45), M['walnut'], bevel=0.01, segments=2)
    common.box('fuellung_o', (0.03, -0.36, 1.65), (0.045, 0.36, 2.85), M['walnut'], bevel=0.01, segments=2)
    common.box('kranz', (0.0, -0.52, 3.0), (0.07, 0.52, 3.16), M['walnut'], bevel=0.012, segments=2)
    common.box('sockel', (0.0, -0.5, 0.0), (0.05, 0.5, 0.15), M['walnut'], bevel=0.006)
