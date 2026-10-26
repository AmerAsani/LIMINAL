"""V12 Level 2 (LOOK_2.md): Maschinen an den Waenden, Rohrbuendel, verschlossene Tueren, nackte Roehren, violettes
Notlicht, Wagen und Werkzeug, verrottendes Schmalspurgleis mit entgleistem Trichterwagen, abgeschraegte Ecke (45 Grad).

Wandmaschinen stehen auf dem Boden mit der Rueckseite an der Wand: Ursprung am Wandfuss (x = 0 Wandflaeche, z = 0
Boden), sie ragen entlang +x in den Raum, Breite entlang y. Verschlossene Tueren ebenso (Wandflaeche x = 0, Boden z = 0).
Deckenleuchten: Ursprung in der Deckenebene (z = 0), haengen nach unten.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common


def _cyl(name, a, b, r, mat, seg=20, caps=True):
    """Zylinder von Punkt a nach Punkt b."""
    a, b = Vector(a), Vector(b)
    d = b - a
    bpy.ops.mesh.primitive_cylinder_add(vertices=seg, radius=r, depth=d.length, location=(a + b) / 2)
    ob = bpy.context.active_object
    ob.name = name
    ob.rotation_euler = d.to_track_quat('Z', 'Y').to_euler()
    if not caps:
        pass
    ob.data.materials.append(mat)
    common.shade_auto(ob, 40.0)
    return ob


def _flange(name, p, axis, r, mat):
    ax = Vector(axis).normalized()
    return _cyl(name, Vector(p) - ax * 0.015, Vector(p) + ax * 0.015, r * 1.45, mat, seg=20)


STEEL_G = (0.34, 0.37, 0.36)
RUST = (0.40, 0.26, 0.16)


def _mats():
    return {
        'grey': common.material('grau', (0.42, 0.45, 0.44), look='paint', rough=0.55),
        'green': common.material('gruen', (0.26, 0.31, 0.27), look='paint', rough=0.65),
        'beige': common.material('beige', (0.62, 0.58, 0.48), look='paint', rough=0.6),
        'dark': common.material('dunkel', (0.12, 0.12, 0.12), look='plastic', rough=0.6),
        'steel': common.material('stahl', (0.55, 0.56, 0.56), look='metal', rough=0.4, metal=0.9),
        'rust': common.material('rost', RUST, look='metal', tex='metal_rusty_iron', rough=0.8, metal=0.4),
        'pipe': common.material('rohr', (0.36, 0.38, 0.38), look='galvanized', rough=0.55, metal=0.7),
        'pipe_red': common.material('rohr_rot', (0.36, 0.15, 0.12), look='paint', rough=0.7),
        'pipe_yel': common.material('rohr_gelb', (0.52, 0.44, 0.20), look='paint', rough=0.7),
        'glass': common.material('glas', (0.75, 0.78, 0.74), look='glass', rough=0.1),
        'black': common.material('schwarz', (0.02, 0.02, 0.02), look='glass', rough=0.05),
        'red': common.material('rot', (0.46, 0.12, 0.09), look='paint', rough=0.6),
    }


# --- Maschinen ---------------------------------------------------------------------------------------------------
def build_mach_cabinet():
    """Schaltschrank 1,0 x 0,55 x 2,1 m: zwei Tueren mit Sicke, Anzeige, Schalter, Kabelrohr bis zur Decke (2,6 m)."""
    M = _mats()
    D, W, H = 0.55, 1.0, 2.1
    common.box('sockel', (0.02, -W / 2, 0.0), (D - 0.02, W / 2, 0.1), M['dark'], bevel=0.01)
    common.box('korpus', (0.0, -W / 2, 0.1), (D, W / 2, H), M['grey'], bevel=0.012, segments=2)
    for s in (-1, 1):
        y0, y1 = (0.005, W / 2 - 0.02) if s > 0 else (-W / 2 + 0.02, -0.005)
        common.box(f'tuer{s}', (D, y0, 0.15), (D + 0.015, y1, H - 0.05), M['grey'], bevel=0.006)
        common.box(f'sicke{s}', (D + 0.015, y0 + 0.06, 0.4), (D + 0.02, y1 - 0.06, 0.42), M['grey'])
        common.box(f'griff{s}', (D + 0.015, s * 0.03 - 0.008, 1.0), (D + 0.05, s * 0.03 + 0.008, 1.18), M['dark'], bevel=0.004)
    common.box('anzeige', (D + 0.016, 0.12, 1.45), (D + 0.02, 0.36, 1.62), M['glass'])
    for k in range(4):
        _cyl(f'schalter{k}', (D + 0.015, -0.35 + k * 0.08, 1.55), (D + 0.05, -0.35 + k * 0.08, 1.55), 0.015,
             M['red'] if k == 0 else M['dark'], seg=12)
    _cyl('kabelrohr', (0.15, 0.3, H), (0.15, 0.3, 2.6), 0.04, M['pipe'], seg=14)
    _cyl('kabelrohr2', (0.3, -0.25, H), (0.3, -0.25, 2.6), 0.025, M['pipe'], seg=12)


def build_mach_pump():
    """Liegende Pumpe mit Motor auf Grundrahmen (1,5 x 0,8 m), Saug- und Druckrohr in die Wand."""
    M = _mats()
    common.box('rahmen', (0.05, -0.75, 0.0), (0.8, 0.75, 0.14), M['steel'], bevel=0.01)
    _cyl('motor', (0.45, -0.7, 0.42), (0.45, 0.0, 0.42), 0.26, M['green'], seg=28)
    for k in range(8):
        y = -0.65 + k * 0.08
        _cyl(f'rippe{k}', (0.45, y, 0.42), (0.45, y + 0.015, 0.42), 0.285, M['green'], seg=28)
    _cyl('kupplung', (0.45, 0.0, 0.42), (0.45, 0.18, 0.42), 0.11, M['steel'])
    _cyl('pumpe', (0.45, 0.18, 0.42), (0.45, 0.5, 0.42), 0.3, M['green'], seg=28)
    _cyl('druck', (0.45, 0.35, 0.72), (0.45, 0.35, 1.6), 0.09, M['pipe_red'])
    _cyl('druck_wand', (0.45, 0.35, 1.6), (0.0, 0.35, 1.6), 0.09, M['pipe_red'])
    _flange('fl1', (0.45, 0.35, 0.8), (0, 0, 1), 0.09, M['steel'])
    _cyl('saug', (0.45, 0.62, 0.42), (0.0, 0.62, 0.42), 0.11, M['pipe'])
    _flange('fl2', (0.3, 0.62, 0.42), (1, 0, 0), 0.11, M['steel'])
    common.box('klemme', (0.66, -0.45, 0.55), (0.76, -0.25, 0.7), M['grey'], bevel=0.005)


def build_mach_tank():
    """Stehender Kessel (D 0,9 m, 2,2 m) auf Fuessen vor der Wand, Ventil, Manometer, Rohr in die Wand."""
    M = _mats()
    r = 0.45
    c = Vector((0.55, 0.0, 0.0))
    for a in (0.5, 2.6, 4.7):
        p = c + Vector((math.cos(a) * 0.3, math.sin(a) * 0.3, 0))
        _cyl(f'fuss{a}', p, p + Vector((0, 0, 0.32)), 0.035, M['steel'], seg=10)
    _cyl('mantel', c + Vector((0, 0, 0.3)), c + Vector((0, 0, 2.0)), r, M['rust'], seg=36)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=36, ring_count=12, radius=r, location=c + Vector((0, 0, 2.0)))
    ob = bpy.context.active_object
    ob.scale = (1, 1, 0.35)
    ob.data.materials.append(M['rust'])
    ob.data.shade_smooth()
    bpy.ops.mesh.primitive_uv_sphere_add(segments=36, ring_count=12, radius=r, location=c + Vector((0, 0, 0.3)))
    ob = bpy.context.active_object
    ob.scale = (1, 1, 0.3)
    ob.data.materials.append(M['rust'])
    ob.data.shade_smooth()
    for z in (0.7, 1.35, 1.95):
        _cyl(f'ring{z}', c + Vector((0, 0, z - 0.02)), c + Vector((0, 0, z + 0.02)), r + 0.012, M['steel'], seg=36)
    _cyl('stutzen', c + Vector((0, 0, 2.1)), c + Vector((0, 0, 2.45)), 0.07, M['pipe'])
    _cyl('stutzen_wand', c + Vector((0, 0, 2.45)), Vector((0.0, 0.0, 2.45)), 0.07, M['pipe'])
    _cyl('ventil_rohr', c + Vector((0.0, -r, 1.1)), c + Vector((0.0, -r - 0.18, 1.1)), 0.05, M['pipe'])
    _cyl('handrad', c + Vector((0.0, -r - 0.18, 1.1)), c + Vector((0.0, -r - 0.2, 1.1)), 0.12, M['red'], seg=20)
    _cyl('mano', c + Vector((r * 0.7, -r * 0.7, 1.5)), c + Vector((r * 0.75 + 0.05, -r * 0.75 - 0.05, 1.5)), 0.06, M['steel'], seg=18)


def _bundle(length, specs, seed):
    """Waagrechte Rohre an der Wand (Laenge entlang y, mittig um y = 0): specs = (hoehe, abstand, radius, mat)."""
    M = _mats()
    for k, (z, off, r, mk) in enumerate(specs):
        _cyl(f'rohr{k}', (off, -length / 2, z), (off, length / 2, z), r, M[mk], seg=18)
        for j in range(int(length / 2.5) + 1):
            y = -length / 2 + 0.2 + j * 2.5
            if y > length / 2:
                break
            common.box(f'schelle{k}{j}', (0.0, y - 0.025, z - r - 0.03), (off + r + 0.01, y + 0.025, z - r), M['steel'])
            _flange(f'muffe{k}{j}', (off, y + 1.2, z), (0, 1, 0), r, M['steel'])


def build_pipe_bundle_a():
    _bundle(5.0, [(2.25, 0.16, 0.08, 'pipe'), (2.0, 0.12, 0.05, 'pipe_red'), (0.35, 0.14, 0.07, 'pipe'), (1.75, 0.1, 0.035, 'pipe_yel')], 1)


def build_pipe_bundle_b():
    _bundle(5.0, [(2.3, 0.2, 0.12, 'rust'), (1.95, 0.1, 0.04, 'pipe'), (1.85, 0.1, 0.04, 'pipe')], 2)


def build_pipe_riser():
    """Senkrechte Rohre Boden-Decke (2,6 m) mit Flanschen, Absperrventil mit Handrad."""
    M = _mats()
    for y, r, mk in ((-0.15, 0.07, 'pipe'), (0.12, 0.05, 'pipe_red')):
        _cyl(f'steig{y}', (0.12, y, 0.0), (0.12, y, 2.6), r, M[mk], seg=18)
        for z in (0.4, 1.6, 2.4):
            _flange(f'fl{y}{z}', (0.12, y, z), (0, 0, 1), r, M['steel'])
    common.box('ventil', (0.06, -0.21, 1.05), (0.18, -0.09, 1.2), M['steel'], bevel=0.01)
    _cyl('spindel', (0.18, -0.15, 1.12), (0.32, -0.15, 1.12), 0.012, M['steel'], seg=10)
    _cyl('rad', (0.32, -0.15, 1.12), (0.335, -0.15, 1.12), 0.13, M['red'], seg=22)


def build_junction_box():
    """Verteilerkasten an der Wand (Wandflaeche x = 0, Mitte im Ursprung), Kabelrohre nach oben."""
    M = _mats()
    common.box('kasten', (0.0, -0.3, -0.4), (0.22, 0.3, 0.4), M['beige'], bevel=0.012, segments=2)
    common.box('deckel', (0.22, -0.28, -0.38), (0.235, 0.28, 0.38), M['beige'], bevel=0.005)
    common.box('schild', (0.235, -0.08, 0.2), (0.237, 0.08, 0.3), M['pipe_yel'])
    for y in (-0.18, 0.0, 0.18):
        _cyl(f'kabel{y}', (0.1, y, 0.4), (0.1, y, 1.3), 0.022, M['pipe'], seg=10)


# --- Licht --------------------------------------------------------------------------------------------------------
def _tube(rgb):
    M = _mats()
    lamp = common.material('roehre', rgb, look='glow', glow=1.6)
    common.box('schiene', (-0.62, -0.035, -0.07), (0.62, 0.035, -0.02), M['grey'], bevel=0.006)
    _cyl('roehre', (-0.6, 0.0, -0.09), (0.6, 0.0, -0.09), 0.013, lamp, seg=12)
    for x in (-0.61, 0.61):
        common.box(f'fassung{x}', (x - 0.02, -0.025, -0.11), (x + 0.02, 0.025, -0.06), M['dark'])
    for x in (-0.45, 0.45):
        _cyl(f'seil{x}', (x, 0.0, -0.02), (x, 0.0, 0.0), 0.003, M['dark'], seg=6)
    # offenes Kabel: haengt in einem Bogen zur Decke
    pts = [(0.62, 0.0, -0.05), (0.75, 0.02, -0.16), (0.9, 0.03, -0.12), (1.0, 0.03, 0.0)]
    for i in range(len(pts) - 1):
        _cyl(f'kabel{i}', pts[i], pts[i + 1], 0.005, M['dark'], seg=6)


def build_tube_white():
    _tube((1.0, 0.98, 0.94))


def build_tube_orange():
    _tube((1.0, 0.72, 0.42))


def build_tube_dead():
    _tube((0.35, 0.36, 0.35))


def build_violet_lamp():
    """Notlicht violett (Wandmontage, Wandflaeche x = 0): kleines Gehaeuse mit leuchtender Scheibe."""
    M = _mats()
    glow = common.material('violett', (0.62, 0.32, 1.0), look='glow', glow=2.5)
    common.box('gehaeuse', (0.0, -0.09, -0.05), (0.06, 0.09, 0.05), M['dark'], bevel=0.006)
    common.box('scheibe', (0.06, -0.075, -0.035), (0.065, 0.075, 0.035), glow)


# --- Verschlossene Tueren (Wandteil) ------------------------------------------------------------------------------
def _locked_door(leaf_rgb, kind, sign=None, H=2.05, W=0.92, seed=1):
    rng = random.Random(seed)
    M = _mats()
    frame = common.material('zarge', (0.30, 0.31, 0.30), look='paint', rough=0.5)
    look = {'steel': 'paint', 'plywood': 'wood', 'wood': 'wood'}[kind]
    leaf = common.material(f'blatt_{kind}', leaf_rgb, look=look, rough=0.6 if kind == 'steel' else 0.8)
    t = 0.05
    common.box('zarge_l', (0.0, W / 2, 0.0), (0.06, W / 2 + t, H + t), frame, bevel=0.004)
    common.box('zarge_r', (0.0, -W / 2 - t, 0.0), (0.06, -W / 2, H + t), frame, bevel=0.004)
    common.box('zarge_o', (0.0, -W / 2 - t, H), (0.06, W / 2 + t, H + t), frame, bevel=0.004)
    common.box('blatt', (0.0, -W / 2, 0.0), (0.035, W / 2, H), leaf, bevel=0.004)
    if kind == 'steel':
        for i in range(2):  # gepraegte Quadrate
            for k in range(2):
                y0, z0 = -W / 2 + 0.1 + i * (W / 2 - 0.05), 0.25 + k * 0.95
                if sign == 'window' and k == 1:
                    continue
                common.box(f'praegung{i}{k}', (0.035, y0, z0), (0.042, y0 + W / 2 - 0.15, z0 + 0.75), leaf, bevel=0.01)
    if kind == 'plywood':
        for k in range(3):
            common.box(f'fuge{k}', (0.035, -W / 2, 0.6 + k * 0.6), (0.037, W / 2, 0.605 + k * 0.6), M['dark'])
    if kind == 'wood':
        for k in range(5):
            y0 = -W / 2 + k * W / 5
            common.box(f'brett{k}', (0.035, y0 + 0.004, 0.02), (0.045, y0 + W / 5 - 0.004, H - 0.02), leaf, bevel=0.003)
    if sign == 'window':
        common.box('fenster_rahmen', (0.035, -0.22, 1.3), (0.05, 0.22, 1.75), frame, bevel=0.004)
        common.box('fenster', (0.05, -0.19, 1.33), (0.052, 0.19, 1.72), M['black'])  # dahinter: reines Schwarz
    # Klinke und Schloss
    _cyl('rosette', (0.035, -W / 2 + 0.1, 1.05), (0.045, -W / 2 + 0.1, 1.05), 0.03, M['steel'], seg=16)
    _cyl('klinke', (0.06, -W / 2 + 0.1, 1.05), (0.06, -W / 2 + 0.24, 1.05), 0.011, M['steel'], seg=10)
    _cyl('klinke_hals', (0.045, -W / 2 + 0.1, 1.05), (0.065, -W / 2 + 0.1, 1.05), 0.011, M['steel'], seg=10)
    _cyl('schloss', (0.035, -W / 2 + 0.1, 0.92), (0.045, -W / 2 + 0.1, 0.92), 0.018, M['steel'], seg=12)
    if sign == 'warn':  # Warnschild ohne Schrift: gelbes Dreieck mit schwarzem Rand und Ausrufezeichen
        yel = common.material('warn_gelb', (0.85, 0.68, 0.10), look='paint', rough=0.5)
        bm = bmesh.new()
        z0, s = 1.45, 0.24
        vs = [bm.verts.new((0.046, 0.0, z0 + s * 0.866)), bm.verts.new((0.046, -s / 2 * 1.0, z0)), bm.verts.new((0.046, s / 2, z0))]
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()
        common.obj_from_bmesh('warn', bm, yel)
        common.box('warn_strich', (0.047, -0.01, z0 + 0.06), (0.048, 0.01, z0 + 0.15), M['dark'])
        common.box('warn_punkt', (0.047, -0.01, z0 + 0.025), (0.048, 0.01, z0 + 0.045), M['dark'])
    if sign == 'plate':  # Blechschild mit eingeschlagenen Strichen (unleserlich)
        common.box('schild', (0.036, -0.15, 1.5), (0.04, 0.15, 1.62), M['steel'], bevel=0.003)
        for k in range(3):
            common.box(f'zeile{k}', (0.04, -0.12, 1.52 + k * 0.03), (0.041, -0.12 + rng.uniform(0.1, 0.22), 1.535 + k * 0.03), M['dark'])
    if sign == 'spray':  # Sprühfarbe: zwei schraege Striche
        sp = common.material('spray', (0.70, 0.14, 0.12), look='paint', rough=0.9)
        for k in range(2):
            ob = common.box(f'spray{k}', (0.036, -0.3, -0.02), (0.037, 0.3, 0.02), sp)
            ob.location = (0, 0.05 * k, 1.2 + 0.15 * k)
            ob.rotation_euler = (0.5 - k, 0, 0)


def build_door_steel_white():
    _locked_door((0.82, 0.82, 0.79), 'steel', 'warn', seed=1)


def build_door_steel_grey():
    _locked_door((0.50, 0.52, 0.52), 'steel', 'plate', seed=2)


def build_door_steel_cream():
    _locked_door((0.80, 0.74, 0.60), 'steel', 'spray', seed=3)


def build_door_window():
    _locked_door((0.70, 0.71, 0.69), 'steel', 'window', seed=4)


def build_door_plywood():
    _locked_door((0.66, 0.54, 0.38), 'plywood', None, seed=5)


def build_door_wood_old():
    _locked_door((0.36, 0.26, 0.18), 'wood', 'plate', seed=6)


def build_door_oversize():
    _locked_door((0.46, 0.48, 0.47), 'steel', 'warn', H=2.5, W=1.8, seed=7)


def build_door_level3():
    """Stahltuer nach Level 3 (im Durchbruch, offen stehend nach +x) mit aufgemalter 3 auf dem Sturz."""
    M = _mats()
    frame = common.material('zarge', (0.30, 0.31, 0.30), look='paint', rough=0.5)
    leaf = common.material('blatt', (0.70, 0.71, 0.68), look='paint', rough=0.55)
    paint = common.material('farbe', (0.88, 0.86, 0.80), look='paint', rough=0.8)
    W, H = 1.0, 2.05
    for s in (-1, 1):
        common.box(f'pfosten{s}', (0.44, s * W / 2 - (0.05 if s > 0 else 0), 0.0), (0.5, s * W / 2 + (0.0 if s > 0 else 0.05), H), frame, bevel=0.004)
    common.box('sturz', (0.44, -W / 2, H - 0.05), (0.5, W / 2, H), frame)
    ob = common.box('blatt', (0.0, 0.0, 0.0), (0.04, W - 0.02, H - 0.06), leaf, bevel=0.004)
    ob.matrix_world = Matrix.Translation((0.47, -W / 2 + 0.01, 0.01)) @ Matrix.Rotation(math.radians(80), 4, 'Z') @ Matrix.Translation((0, 0, 0)) @ ob.matrix_world
    # "3" aus fuenf Strichen (Segmentziffer), auf die Wand ueber der Tuer (Flaeche der Laibung bei x = -0.5)
    x = -0.505
    seg = [((-0.1, 2.48), (0.1, 2.52)), ((0.06, 2.35), (0.1, 2.52)), ((-0.02, 2.33), (0.1, 2.37)),
           ((0.06, 2.18), (0.1, 2.35)), ((-0.1, 2.18), (0.1, 2.22))]
    for k, ((y0, z0), (y1, z1)) in enumerate(seg):
        common.box(f'ziffer{k}', (x - 0.002, -y1, z0), (x, -y0, z1), paint)  # im Spiel geprueft: so lesbar


# --- Gegenstaende -----------------------------------------------------------------------------------------------------
def build_trolley_flatbed():
    """Plattformwagen 1,2 x 0,7 m mit Buegel, Rollen; darauf Maschinenteile (Zahnrad, Gehaeuse, Rohrstueck)."""
    M = _mats()
    common.box('platte', (-0.6, -0.35, 0.16), (0.6, 0.35, 0.2), M['green'], bevel=0.008)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 0.5, sy * 0.27
            common.box(f'gabel{sx}{sy}', (x - 0.02, y - 0.02, 0.1), (x + 0.02, y + 0.02, 0.16), M['steel'])
            _cyl(f'rad{sx}{sy}', (x, y - 0.018, 0.065), (x, y + 0.018, 0.065), 0.065, M['dark'], seg=16)
    for sy in (-1, 1):
        _cyl(f'buegel{sy}', (-0.58, sy * 0.3, 0.2), (-0.58, sy * 0.3, 0.95), 0.014, M['steel'], seg=10)
    _cyl('buegel_q', (-0.58, -0.3, 0.95), (-0.58, 0.3, 0.95), 0.014, M['steel'], seg=10)
    _cyl('zahnrad', (0.15, 0.0, 0.2), (0.15, 0.0, 0.26), 0.2, M['rust'], seg=24)
    common.box('gehaeuse', (-0.4, -0.25, 0.2), (-0.05, 0.1, 0.45), M['grey'], bevel=0.01)
    _cyl('rohrstueck', (0.0, 0.2, 0.26), (0.55, 0.25, 0.26), 0.05, M['rust'], seg=14)


def build_toolbox():
    M = _mats()
    common.box('kasten', (-0.25, -0.11, 0.0), (0.25, 0.11, 0.2), M['red'], bevel=0.008)
    common.box('deckel', (-0.25, -0.11, 0.2), (0.25, 0.11, 0.24), M['red'], bevel=0.008)
    _cyl('griff', (-0.12, 0.0, 0.27), (0.12, 0.0, 0.27), 0.012, M['dark'], seg=10)
    for x in (-0.12, 0.12):
        _cyl(f'stuetze{x}', (x, 0.0, 0.24), (x, 0.0, 0.27), 0.01, M['dark'], seg=8)


def build_stepladder():
    """Stehleiter (offen, 1,6 m): zwei Holme vorn mit Stufen, zwei hinten, Spreizgurt."""
    M = _mats()
    alu = common.material('alu', (0.70, 0.71, 0.72), look='metal', rough=0.35, metal=0.9)
    top = 1.6
    for s in (-1, 1):
        _cyl(f'vorn{s}', (0.32, s * 0.22, 0.0), (0.0, s * 0.18, top), 0.018, alu, seg=10)
        _cyl(f'hinten{s}', (-0.32, s * 0.22, 0.0), (0.0, s * 0.18, top), 0.016, alu, seg=10)
    for k in range(5):
        t = (k + 1) / 6
        x = 0.32 * (1 - t)
        common.box(f'stufe{k}', (x - 0.05, -0.2, t * top - 0.015), (x + 0.03, 0.2, t * top + 0.01), alu)
    common.box('kopf', (-0.06, -0.21, top - 0.02), (0.06, 0.21, top + 0.03), M['dark'], bevel=0.008)


def build_cable_reel():
    M = _mats()
    for y in (-0.15, 0.15):
        _cyl(f'scheibe{y}', (0.0, y - 0.01, 0.3), (0.0, y + 0.01, 0.3), 0.3, M['dark'], seg=28)
    _cyl('kern', (0.0, -0.15, 0.3), (0.0, 0.15, 0.3), 0.2, common.material('kabel', (0.75, 0.48, 0.10), look='rubber', rough=0.7), seg=24)
    for y in (-0.15, 0.15):
        _cyl(f'fuss{y}', (-0.2, y, 0.0), (0.0, y, 0.3), 0.012, M['steel'], seg=8)
        _cyl(f'fuss2{y}', (0.2, y, 0.0), (0.0, y, 0.3), 0.012, M['steel'], seg=8)


def build_folding_chair():
    M = _mats()
    seat = common.material('sitz', (0.32, 0.34, 0.36), look='paint', rough=0.6)
    common.box('sitz', (-0.2, -0.2, 0.44), (0.2, 0.2, 0.47), seat, bevel=0.01)
    common.box('lehne', (-0.22, -0.19, 0.62), (-0.2, 0.19, 0.84), seat, bevel=0.008)
    for sy in (-1, 1):
        _cyl(f'vorn{sy}', (0.22, sy * 0.19, 0.0), (-0.21, sy * 0.19, 0.86), 0.011, M['steel'], seg=8)
        _cyl(f'hinten{sy}', (-0.22, sy * 0.19, 0.0), (0.15, sy * 0.19, 0.45), 0.011, M['steel'], seg=8)


# --- Gleis und Wagen ---------------------------------------------------------------------------------------------------
def build_rail_track():
    """Schmalspurgleis (Spur 0,6 m), 5 m entlang x: rostige Schienen, verrottende Schwellen, einige fehlen."""
    rng = random.Random(9)
    M = _mats()
    sleeper = common.material('schwelle', (0.30, 0.24, 0.18), look='wood', rough=0.95)
    for k in range(9):
        if rng.random() < 0.15:
            continue
        x = -2.4 + k * 0.6
        ob = common.box(f'schwelle{k}', (-0.07, -0.5, 0.0), (0.07, 0.5, 0.06), sleeper, bevel=0.005)
        ob.location = (x, rng.uniform(-0.03, 0.03), 0)
        ob.rotation_euler = (0, 0, rng.uniform(-0.08, 0.08))
    for y in (-0.3, 0.3):
        common.box(f'fuss{y}', (-2.5, y - 0.035, 0.06), (2.5, y + 0.035, 0.07), M['rust'])
        common.box(f'steg{y}', (-2.5, y - 0.008, 0.07), (2.5, y + 0.008, 0.13), M['rust'])
        common.box(f'kopf{y}', (-2.5, y - 0.02, 0.13), (2.5, y + 0.02, 0.15), M['rust'])


def build_hopper_wagon():
    """Entgleister Trichterwagen (1,8 x 0,9 m), schraeg liegend auf einer Seite des Gleises."""
    M = _mats()
    parts = []

    def add(ob):
        parts.append(ob)
        return ob
    # Trichter: Pyramidenstumpf aus vier Flaechen
    bm = bmesh.new()
    top = [(-0.9, -0.45, 1.0), (0.9, -0.45, 1.0), (0.9, 0.45, 1.0), (-0.9, 0.45, 1.0)]
    bot = [(-0.4, -0.2, 0.35), (0.4, -0.2, 0.35), (0.4, 0.2, 0.35), (-0.4, 0.2, 0.35)]
    vt = [bm.verts.new(p) for p in top]
    vb = [bm.verts.new(p) for p in bot]
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((vt[i], vt[j], vb[j], vb[i]))
    bm.faces.new(vb[::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    sol = add(common.obj_from_bmesh('trichter', bm, M['rust']))
    mod = sol.modifiers.new('dicke', 'SOLIDIFY')
    mod.thickness = 0.02
    add(common.box('rahmen', (-0.85, -0.38, 0.22), (0.85, 0.38, 0.32), M['steel']))
    for x in (-0.55, 0.55):
        for y in (-0.3, 0.3):
            add(_cyl(f'rad{x}{y}', (x, y - 0.03, 0.15), (x, y + 0.03, 0.15), 0.15, M['dark'], seg=18))
    add(_cyl('kupplung', (0.85, 0.0, 0.27), (1.05, 0.0, 0.27), 0.03, M['steel'], seg=10))
    M_ = Matrix.Translation((0.0, 0.35, 0.05)) @ Matrix.Rotation(math.radians(-22), 4, 'X') @ Matrix.Rotation(0.15, 4, 'Z')
    for ob in parts:
        ob.matrix_world = M_ @ ob.matrix_world


# --- Abgeschraegte Ecke (45-Grad-Knick) ---------------------------------------------------------------------------------
def _chamfer(H):
    """Dreiecksprisma in einer Kachel (Ursprung Kachelmitte am Boden): fuellt die Ecke -x/-y, schraege Flaeche blickt
    nach +x+y. Beton wie die Tunnelwand."""
    conc = common.material('beton', (0.70, 0.68, 0.64), look='concrete', tex='concrete_chalky')
    bm = bmesh.new()
    tri = [(-0.5, -0.5), (0.5, -0.5), (-0.5, 0.5)]
    lo = [bm.verts.new((x, y, 0.0)) for x, y in tri]
    hi = [bm.verts.new((x, y, H)) for x, y in tri]
    bm.faces.new(lo[::-1])
    bm.faces.new(hi)
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('schraege', bm, conc)
    common.shade_auto(ob, 30.0)


def build_chamfer_26():
    _chamfer(2.6)


def build_chamfer_32():
    _chamfer(3.2)
