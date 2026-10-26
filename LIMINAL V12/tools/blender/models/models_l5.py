"""V12 Level 5 (LOOK_5.md): Hotel der 1930er - Wandvertaefelung, Zimmertueren mit Messingnummer, Schild "The Beverly
Room", Aufzug mit Kupfergitter, Mahjong-Tisch mit Getraenken, grosser Kronleuchter, Grammophon, Kerzenwandleuchter,
Ofen (Kesselraum), Ende eines Metro-Wagens (Halt nach Level 903), helles Fenster (nach Level 188), altes Bildnis.

Schrift: Inter (SIL OFL 1.1) aus den Blender-Datendateien, als Geometrie (LICENSES.md).
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common
from models.models_l2 import _cyl

FONT = None


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


def _text(name, body, size, mat, at, depth=0.002):
    """Text als Mesh in der yz-Ebene, blickt entlang +x, zentriert um at."""
    cu = bpy.data.curves.new(name, 'FONT')
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


def _m():
    return {
        'walnut': common.material('nussbaum', (0.42, 0.26, 0.16), look='wood', tex='wood_walnut_floor', rough=0.45),
        'walnut_d': common.material('nussbaum_dunkel', (0.30, 0.18, 0.11), look='wood', tex='wood_walnut_floor', rough=0.5),
        'brass': common.material('messing', (0.80, 0.62, 0.30), look='metal', rough=0.3, metal=1.0),
        'gold': common.material('gold', (0.88, 0.70, 0.34), look='metal', rough=0.25, metal=1.0),
        'silver': common.material('silber', (0.82, 0.82, 0.84), look='metal', rough=0.25, metal=1.0),
        'copper': common.material('kupfer', (0.72, 0.42, 0.26), look='metal', rough=0.35, metal=1.0),
        'black': common.material('schwarz', (0.04, 0.04, 0.04), look='paint', rough=0.4),
        'cream': common.material('creme', (0.86, 0.80, 0.66), look='paint', rough=0.6),
        'glass': common.material('glas', (0.75, 0.8, 0.8), look='glass', rough=0.05),
        'iron': common.material('eisen', (0.18, 0.17, 0.16), look='metal', rough=0.6, metal=0.8),
        'brick': common.material('ziegel', (0.80, 0.55, 0.45), look='concrete', tex='bricks_brown_old', rough=0.9),
        'red': common.material('rot', (0.46, 0.08, 0.07), look='fabric', rough=0.8),
    }


# --- Vertaefelung -----------------------------------------------------------------------------------------------------
def build_wainscot():
    """Wandvertaefelung (Wandflaeche x = 0, 1 m entlang y, 1,0 m hoch): Sockel, Fuellung mit Rahmen, Brueckungsleiste."""
    M = _m()
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
    M = _m()
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
    M = _m()
    common.box('schild', (0.0, -0.26, -0.06), (0.012, 0.26, 0.06), M['silver'], bevel=0.005, segments=2)
    _text('schrift', 'The Beverly Room', 0.055, M['black'], Vector((0.013, 0.0, 0.0)), depth=0.0012)


# --- Aufzug mit Kupfergitter ------------------------------------------------------------------------------------------------
def build_elevator_cage():
    """Alter Aufzug im Durchbruch (Oeffnung 1,4 m, Laibung 1 m): Kupferrahmen, Scherengitter halb offen, halbrunde
    Stockwerksanzeige mit Zeiger ueber der Tuer (Seite -x)."""
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
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
    M = _m()
    prof = [(0.0, -0.26), (0.03, -0.26), (0.03, -0.235), (0.045, -0.225), (0.05, -0.2)]
    for k in range(9):  # Hohlkehle (konkav)
        a = math.pi / 2 * k / 8
        prof.append((0.06 + 0.12 * (1 - math.cos(a)), -0.19 + 0.13 * math.sin(a)))
    prof += [(0.19, -0.05), (0.2, -0.04), (0.2, 0.0), (0.0, 0.0)]
    ob = common.profile_extrude('gesims', prof, 1.0, M['cream'], axis='y')
    ob.location = (0.0, -0.5, 0.0)
    _cyl('perlstab', (0.052, -0.5, -0.205), (0.052, 0.5, -0.205), 0.009, M['gold'], seg=8)


def build_hotel_bed():
    """Doppelbett der 1930er (Mitte im Ursprung, Kopfteil an der Wand bei x = -1.1, 2,15 x 1,6 m): Nussbaumrahmen mit
    Kopf- und Fussteil, Matratze, Laken, rot-goldene Tagesdecke, zwei Kissen."""
    M = _m()
    sheet = common.material('laken', (0.86, 0.84, 0.78), look='fabric', rough=0.9)
    spread = common.material('tagesdecke', (0.42, 0.07, 0.06), look='fabric', rough=0.85)
    trim = common.material('borte', (0.70, 0.55, 0.28), look='fabric', rough=0.7)
    W = 0.8
    common.box('kopfteil', (-1.1, -W - 0.05, 0.0), (-1.04, W + 0.05, 1.1), M['walnut'], bevel=0.01, segments=2)
    common.box('kopf_kassette', (-1.04, -W + 0.12, 0.62), (-1.025, W - 0.12, 0.98), M['walnut_d'], bevel=0.012, segments=2)
    common.box('kopf_leiste', (-1.12, -W - 0.08, 1.1), (-1.02, W + 0.08, 1.16), M['walnut_d'], bevel=0.015, segments=2)
    common.box('fussteil', (1.0, -W - 0.05, 0.0), (1.05, W + 0.05, 0.62), M['walnut'], bevel=0.01, segments=2)
    common.box('fuss_leiste', (0.99, -W - 0.07, 0.62), (1.06, W + 0.07, 0.67), M['walnut_d'], bevel=0.012, segments=2)
    for (x, z) in ((-1.07, 1.24), (1.025, 0.72)):
        for sg in (-1, 1):
            _cyl(f'pfosten{x}{sg}', (x, sg * (W + 0.04), 0.0), (x, sg * (W + 0.04), z - 0.06), 0.035, M['walnut_d'], seg=12)
            bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=0.045, location=(x, sg * (W + 0.04), z - 0.03))
            bpy.context.active_object.data.materials.append(M['walnut_d'])
    for sg in (-1, 1):
        common.box(f'zarge{sg}', (-1.04, sg * W - 0.03, 0.18), (1.0, sg * W + 0.03, 0.36), M['walnut_d'], bevel=0.008)
    common.box('matratze', (-1.03, -W + 0.03, 0.34), (0.99, W - 0.03, 0.54), sheet, bevel=0.05, segments=3)
    common.box('decke', (-0.42, -W - 0.04, 0.3), (1.0, W + 0.04, 0.57), spread, bevel=0.03, segments=3)
    common.box('umschlag', (-0.5, -W - 0.04, 0.5), (-0.36, W + 0.04, 0.585), sheet, bevel=0.03, segments=3)
    common.box('borte', (0.86, -W - 0.045, 0.3), (0.9, W + 0.045, 0.572), trim)
    common.box('kissen_l', (-1.0, 0.06, 0.53), (-0.64, 0.72, 0.68), sheet, bevel=0.06, segments=3)
    common.box('kissen_r', (-1.0, -0.72, 0.53), (-0.64, -0.06, 0.68), sheet, bevel=0.06, segments=3)


def build_boiler_tank():
    """Alter Flammrohrkessel (Rueckseite an der Wand x = 0, ragt 2,6 m in den Raum): genieteter Mantel auf
    Ziegelsaetteln, gewoelbter Boden mit Feuertuer (Glut), Manometer, Wasserstandsglas, Dampfdom mit Rohr zur Decke,
    Handrad (Wiki: Kesselraeume, alte Maschinen, Ventile)."""
    M = _m()
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
    M = _m()
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
