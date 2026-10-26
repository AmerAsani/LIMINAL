"""V12 Level 3 (LOOK_3.md): weisse rostige Gitter, Rohre mit schwarzer Fluessigkeit, Schlammlachen, Generator,
Foerderband, Fenster mit Ziegel dahinter, Gitterrosttreppe, Engelsstatue auf Sockel mit Zeichen, verwitterte
Frauenstatue, weisse Tuecher, Engelsbilder, Aufzugstuer, Warnschild, Schaedel, Zugteil.

Konventionen wie models_l2: Bodenmodelle Ursprung am Boden; Wandmodelle Wandflaeche x = 0, Blick +x; Modelle im
Durchbruch mit dem Ursprung in der Mitte der Oeffnung (Laibung entlang x, 1 m tief).
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common
from models.models_l2 import _cyl, _flange

WHITE_RUST = (0.84, 0.82, 0.76)


def _m():
    return {
        'bar': common.material('gitter', WHITE_RUST, look='paint', rough=0.55),
        'rust': common.material('rost', (0.42, 0.26, 0.15), look='metal', tex='metal_rusty_iron', rough=0.8, metal=0.4),
        'black': common.material('schlamm', (0.01, 0.01, 0.012), look='water', rough=0.05),
        'pipe': common.material('rohr', (0.10, 0.10, 0.11), look='paint', rough=0.25),
        'steel': common.material('stahl', (0.52, 0.53, 0.53), look='metal', rough=0.4, metal=0.9),
        'grey': common.material('grau', (0.40, 0.42, 0.41), look='paint', rough=0.55),
        'green': common.material('gruen', (0.24, 0.30, 0.26), look='paint', rough=0.6),
        'dark': common.material('dunkel', (0.08, 0.08, 0.08), look='plastic', rough=0.6),
        'stone': common.material('stein', (0.86, 0.84, 0.80), look='concrete', tex='travertine', rough=0.8),
        'stone2': common.material('stein2', (0.78, 0.76, 0.72), look='concrete', tex='travertine', rough=0.85),
        'cloth': common.material('tuch', (0.88, 0.87, 0.84), look='fabric', rough=0.95),
        'brick': common.material('ziegel', (0.90, 0.60, 0.50), look='concrete', tex='bricks_red_factory', rough=0.9),
        'glass': common.material('glas', (0.55, 0.60, 0.60), look='glass', rough=0.08),
    }


# --- Gitter ------------------------------------------------------------------------------------------------------
def _bars(width, H=3.6):
    """Gitter in einer Oeffnung (Laibungsmitte x = 0): Staebe 3 cm alle 12 cm, zwei Querriegel; ragt in Wand und Decke."""
    M = _m()
    rng = random.Random(int(width * 10))
    n = int(width / 0.12)
    for k in range(n + 1):
        y = -width / 2 + k * width / n
        mat = M['rust'] if rng.random() < 0.18 else M['bar']
        _cyl(f'stab{k}', (0.0, y, -0.05), (0.0, y, H), 0.015, mat, seg=8)
    for z in (0.12, 1.1, 2.1):
        common.box(f'riegel{z}', (-0.025, -width / 2 - 0.15, z), (0.025, width / 2 + 0.15, z + 0.06), M['bar'], bevel=0.005)


def build_bars_w1():
    _bars(1.0)


def build_bars_w2():
    _bars(2.0)


def build_bars_w3():
    _bars(3.0)


# --- Schwarze Fluessigkeit ---------------------------------------------------------------------------------------
def build_pipe_black():
    """Zwei Rohre an der Wand (5 m entlang y), glaenzend dunkel; an einer Muffe tritt Schwarzes aus und laeuft herab."""
    M = _m()
    for k, (z, off, r) in enumerate(((2.1, 0.14, 0.09), (1.75, 0.11, 0.06))):
        _cyl(f'rohr{k}', (off, -2.5, z), (off, 2.5, z), r, M['pipe'], seg=18)
        for j in range(3):
            y = -2.2 + j * 2.2
            common.box(f'schelle{k}{j}', (0.0, y - 0.025, z - r - 0.03), (off + r + 0.01, y + 0.025, z - r), M['steel'])
            _flange(f'muffe{k}{j}', (off, y + 1.1, z), (0, 1, 0), r, M['rust'])
    # Leck: schwarzer Streifen an der Wand unter der Muffe, Lache am Boden
    common.box('lauf', (0.002, 1.05, 0.0), (0.006, 1.2, 1.7), M['black'])
    _sludge_blob('lache', (0.45, 1.1), 0.5, 11)


def _sludge_blob(name, c, r, seed):
    M = _m()
    rng = random.Random(seed)
    bm = bmesh.new()
    n = 20
    ctr = bm.verts.new((c[0], c[1], 0.004))
    ring = []
    for k in range(n):
        a = 2 * math.pi * k / n
        rr = r * (0.7 + 0.5 * rng.random())
        ring.append(bm.verts.new((c[0] + math.cos(a) * rr, c[1] + math.sin(a) * rr, 0.003)))
    for k in range(n):
        bm.faces.new((ctr, ring[k], ring[(k + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < 0:
            f.normal_flip()
    ob = common.obj_from_bmesh(name, bm, M['black'])
    ob.data.shade_smooth()


def build_sludge_pool():
    _sludge_blob('lache', (0.0, 0.0), 0.9, 21)
    _sludge_blob('lache2', (0.8, 0.5), 0.35, 22)


# --- Generator, Foerderband ----------------------------------------------------------------------------------------
def build_generator():
    """Liegender Generatorsatz 3,2 x 1,5 x 2,0 m: Gehaeuse mit Lueftungsschlitzen, Kuehler, Auspuff, Bedienfeld."""
    M = _m()
    common.box('rahmen', (-1.6, -0.75, 0.0), (1.6, 0.75, 0.18), M['steel'], bevel=0.01)
    common.box('gehaeuse', (-1.5, -0.7, 0.18), (1.1, 0.7, 1.75), M['green'], bevel=0.03, segments=2)
    for k in range(10):
        x = -1.3 + k * 0.22
        for s in (-1, 1):
            common.box(f'schlitz{k}{s}', (x, s * 0.7 - 0.004, 1.0), (x + 0.12, s * 0.7 + 0.004, 1.5), M['dark'])
    common.box('kuehler', (1.1, -0.65, 0.18), (1.5, 0.65, 1.6), M['grey'], bevel=0.02)
    for k in range(9):
        z = 0.3 + k * 0.14
        common.box(f'lamelle{k}', (1.5, -0.6, z), (1.52, 0.6, z + 0.06), M['dark'])
    _cyl('auspuff', (-1.0, 0.3, 1.75), (-1.0, 0.3, 2.3), 0.09, M['rust'], seg=16)
    common.box('feld', (-1.52, -0.4, 0.8), (-1.5, 0.2, 1.4), M['grey'], bevel=0.005)
    for k in range(3):
        _cyl(f'anzeige{k}', (-1.53, -0.3 + k * 0.18, 1.25), (-1.56, -0.3 + k * 0.18, 1.25), 0.05, M['glass'], seg=16)
    _cyl('kabel', (-0.5, -0.7, 0.4), (-0.5, -1.2, 0.05), 0.05, M['dark'], seg=10)


def build_conveyor():
    """Foerderband 6 m entlang x, 0,9 m breit, Bandoberkante 0,9 m: Rahmen, Rollen, Beine, Seitenleisten, Kisten."""
    M = _m()
    belt = common.material('band', (0.12, 0.12, 0.12), look='rubber', rough=0.8)
    L, W, Z = 6.0, 0.9, 0.9
    common.box('band', (-L / 2, -W / 2 + 0.05, Z - 0.03), (L / 2, W / 2 - 0.05, Z), belt)
    for s in (-1, 1):
        common.box(f'holm{s}', (-L / 2, s * W / 2 - 0.04, Z - 0.16), (L / 2, s * W / 2 + 0.04, Z + 0.05), M['grey'], bevel=0.008)
    for k in range(13):
        x = -L / 2 + 0.25 + k * 0.46
        _cyl(f'rolle{k}', (x, -W / 2 + 0.05, Z - 0.06), (x, W / 2 - 0.05, Z - 0.06), 0.04, M['steel'], seg=12)
    for x in (-L / 2 + 0.3, 0.0, L / 2 - 0.3):
        for s in (-1, 1):
            common.box(f'bein{x}{s}', (x - 0.04, s * W / 2 - 0.04, 0.0), (x + 0.04, s * W / 2 + 0.04, Z - 0.16), M['grey'])
        common.box(f'quer{x}', (x - 0.03, -W / 2, 0.25), (x + 0.03, W / 2, 0.3), M['grey'])
    rng = random.Random(5)
    card = common.material('karton', (0.62, 0.50, 0.34), look='cardboard', rough=0.95)
    for k in range(4):
        x = -2.3 + k * 1.5 + rng.uniform(-0.2, 0.2)
        sz = rng.uniform(0.3, 0.45)
        ob = common.box(f'kiste{k}', (-sz / 2, -sz / 2, 0.0), (sz / 2, sz / 2, sz * 0.8), card, bevel=0.004)
        ob.location = (x, rng.uniform(-0.1, 0.1), Z)
        ob.rotation_euler = (0, 0, rng.uniform(-0.3, 0.3))


# --- Fenster mit Ziegel dahinter, Gitterrosttreppe ------------------------------------------------------------------
def build_window_brick():
    """Fenster in der Wand (Wandflaeche x = 0, Mitte im Ursprung, 1,6 x 1,2 m): Rahmen, Sprossen, Glas; 0,35 m dahinter
    roter Ziegel (Wiki: die Fenster der Fertigungsstrassen zeigen roten Ziegel)."""
    M = _m()
    frame = common.material('rahmen', (0.30, 0.32, 0.32), look='paint', rough=0.5)
    W, H = 1.6, 1.2
    common.box('ziegel', (-0.36, -W / 2, -H / 2), (-0.34, W / 2, H / 2), M['brick'])
    for (y0, y1, z0, z1) in ((-W / 2, W / 2, -H / 2 - 0.06, -H / 2), (-W / 2, W / 2, H / 2, H / 2 + 0.06),
                             (-W / 2 - 0.06, -W / 2, -H / 2 - 0.06, H / 2 + 0.06), (W / 2, W / 2 + 0.06, -H / 2 - 0.06, H / 2 + 0.06)):
        common.box(f'rahmen{y0}{z0}', (-0.34, y0, z0), (0.04, y1, z1), frame, bevel=0.004)
    common.box('sprosse_v', (-0.02, -0.02, -H / 2), (0.02, 0.02, H / 2), frame)
    common.box('sprosse_h', (-0.02, -W / 2, -0.02), (0.02, W / 2, 0.02), frame)
    common.box('glas', (-0.004, -W / 2, -H / 2), (0.0, W / 2, H / 2), M['glass'])
    common.box('bank', (0.0, -W / 2 - 0.08, -H / 2 - 0.1), (0.14, W / 2 + 0.08, -H / 2 - 0.06), frame, bevel=0.004)


def build_grate_stair():
    """Gitterrosttreppe an einer Wand (Wand bei y = +0,7, Treppe steigt entlang +x): 1,2 m breit, 4,0 m hoch ueber 5,6 m,
    oben ein Podest 3 m entlang der Wand mit Gelaender. Ursprung am Fuss der Treppe."""
    M = _m()
    grate = common.material('rost_gitter', (0.40, 0.42, 0.42), look='galvanized', rough=0.5, metal=0.8)
    n, run, rise = 20, 0.28, 0.2
    for k in range(n):
        x, z = k * run, (k + 1) * rise
        common.box(f'stufe{k}', (x, -0.6, z - 0.03), (x + run + 0.02, 0.6, z), grate)
    for s in (-1, 1):
        ob = common.box(f'wange{s}', (0.0, s * 0.62 - 0.02, -0.25), (n * run * 1.08, s * 0.62 + 0.02, 0.05), M['grey'])
        ob.matrix_world = Matrix.Rotation(-math.atan2(rise, run), 4, 'Y') @ ob.matrix_world
    px0 = n * run
    common.box('podest', (px0, -0.6, n * rise - 0.04), (px0 + 3.0, 0.6, n * rise), grate)
    for x in (px0 + 0.05, px0 + 1.5, px0 + 2.95):
        _cyl(f'pfosten{x}', (x, -0.6, n * rise), (x, -0.6, n * rise + 1.05), 0.022, M['grey'], seg=10)
    _cyl('handlauf_p', (px0, -0.6, n * rise + 1.05), (px0 + 3.0, -0.6, n * rise + 1.05), 0.025, M['grey'], seg=10)
    _cyl('handlauf', (0.0, -0.6, 1.05), (px0, -0.6, n * rise + 1.05), 0.025, M['grey'], seg=10)
    for k in range(0, n, 4):
        _cyl(f'stab{k}', (k * run, -0.6, (k + 1) * rise), (k * run, -0.6, (k + 1) * rise + 1.05), 0.018, M['grey'], seg=8)
    for x in (px0 + 0.3, px0 + 2.7):
        _cyl(f'stuetze{x}', (x, 0.0, 0.0), (x, 0.0, n * rise - 0.04), 0.06, M['grey'], seg=12)


# --- Engel und Statuen ----------------------------------------------------------------------------------------------
def _lathe(name, prof, mat, seg=40, center=(0.0, 0.0), scale_y=1.0):
    bm = bmesh.new()
    rings = []
    for (z, r) in prof:
        rings.append([bm.verts.new((center[0] + r * math.cos(2 * math.pi * k / seg),
                                    center[1] + r * scale_y * math.sin(2 * math.pi * k / seg), z)) for k in range(seg)])
    for i in range(len(rings) - 1):
        for k in range(seg):
            j = (k + 1) % seg
            bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
    top = bm.verts.new((center[0], center[1], prof[-1][0]))
    for k in range(seg):
        bm.faces.new((rings[-1][k], rings[-1][(k + 1) % seg], top))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh(name, bm, mat)
    common.shade_auto(ob, 45.0)
    return ob


def _wing(name, side, mat, base):
    """Fluegel: eine gewoelbte Flaeche, die an den Schulterblaettern ansetzt, ueber den Kopf steigt und hinter dem
    Ruecken nach unten faellt; darauf gestaffelte Federn (lange Schwungfedern aussen, kurze Deckfedern oben)."""
    sh = Vector((-0.16, side * 0.1, base + 1.66))  # Ansatz am Schulterblatt

    def arc(t):  # Oberkante des Fluegels
        return sh + Vector((-0.08 - 0.12 * t, side * (0.06 + 0.5 * math.sin(math.pi * t * 0.55)), 0.15 + 0.85 * math.sin(math.pi * t * 0.7)))

    def tip(t):  # Unterkante (Spitzen der Schwungfedern)
        a = arc(t)
        L = 0.55 + 1.15 * t ** 0.8
        return a + Vector((-0.06 - 0.06 * t, side * 0.08 * t, -L))
    n = 12
    bm = bmesh.new()
    top = [bm.verts.new(arc(k / n)) for k in range(n + 1)]
    bot = [bm.verts.new(tip(k / n)) for k in range(n + 1)]
    for k in range(n):
        bm.faces.new((top[k], top[k + 1], bot[k + 1], bot[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh(name, bm, mat)
    mod = ob.modifiers.new('dicke', 'SOLIDIFY')
    mod.thickness = 0.06
    ob.data.shade_smooth()
    # Schwungfedern: entlang der Unterkante, leicht gefaechert, ueber die Flaeche gelegt
    for k in range(1, n + 1):
        t = k / n
        a, b = arc(t), tip(t)
        d = b - a
        L = d.length * (0.55 + 0.1 * math.sin(k))
        c = a + d * (1.0 - 0.5 * L / d.length)
        f = common.box(f'{name}_feder{k}', (-0.03, -0.045, -L / 2), (0.03, 0.045, L / 2), mat, bevel=0.02, segments=2)
        f.matrix_world = Matrix.Translation(c + Vector((-0.04, 0, 0))) @ d.to_track_quat('-Z', 'X').to_matrix().to_4x4()
    # Deckfedern: kurze Reihen unter der Oberkante
    for k in range(n):
        t = (k + 0.5) / n
        a = arc(t)
        f = common.box(f'{name}_deck{k}', (-0.03, -0.06, -0.2), (0.03, 0.06, 0.0), mat, bevel=0.025, segments=2)
        f.matrix_world = Matrix.Translation(a + Vector((-0.05, 0, -0.03))) @ Matrix.Rotation(side * 0.2, 4, 'X')


def build_angel_statue():
    """Engel auf Sockel (Sockel 1,4 x 1,4 x 1,1 m mit Zeichen; Figur 2,6 m, Fluegel bis 3,6 m ueber dem Sockel): Gewand mit
    Taille und Falten, betende Haende, gesenkter Kopf, Fluegel vom Schulterblatt. Verwitterter Stein (travertine)."""
    M = _m()
    S = M['stone']
    common.box('sockel_fuss', (-0.78, -0.78, 0.0), (0.78, 0.78, 0.18), M['stone2'], bevel=0.02)
    common.box('sockel', (-0.7, -0.7, 0.18), (0.7, 0.7, 1.0), M['stone2'], bevel=0.015)
    common.box('sockel_kopf', (-0.76, -0.76, 1.0), (0.76, 0.76, 1.12), M['stone2'], bevel=0.02)
    rng = random.Random(3)
    for side in range(4):
        R = Matrix.Rotation(side * math.pi / 2, 4, 'Z')
        for row in range(3):
            for col in range(5):
                if rng.random() < 0.25:
                    continue
                y, z = -0.42 + col * 0.21, 0.4 + row * 0.2
                kind = rng.randrange(3)
                if kind == 0:
                    ob = common.box('zeichen', (0.7, y - 0.012, z - 0.06), (0.712, y + 0.012, z + 0.06), M['stone'])
                elif kind == 1:
                    ob = common.box('zeichen', (-0.006, -0.06, -0.012), (0.006, 0.06, 0.012), M['stone'])
                    ob.matrix_world = Matrix.Translation((0.706, y, z)) @ Matrix.Rotation(rng.uniform(-0.8, 0.8), 4, 'X')
                else:
                    bpy.ops.mesh.primitive_torus_add(major_radius=0.045, minor_radius=0.01, major_segments=16, minor_segments=5,
                                                     location=(0.705, y, z), rotation=(0, math.pi / 2, 0))
                    ob = bpy.context.active_object
                    ob.data.materials.append(M['stone'])
                ob.matrix_world = R @ ob.matrix_world
    base = 1.12
    # Gewand: weit am Saum, Taille, Brust, Schultern (Drehkoerper, quer etwas breiter), Saum mit Wellen
    prof = [(base, 0.40), (base + 0.06, 0.41), (base + 0.4, 0.34), (base + 0.8, 0.26), (base + 1.12, 0.19), (base + 1.22, 0.165),
            (base + 1.42, 0.19), (base + 1.58, 0.2), (base + 1.68, 0.17), (base + 1.74, 0.09), (base + 1.76, 0.05)]
    _lathe('gewand', prof, S, seg=56, scale_y=1.3)
    for k in range(16):
        a = 2 * math.pi * k / 16
        x, y = math.cos(a), math.sin(a) * 1.3
        _cyl(f'falte{k}', (x * 0.4, y * 0.4, base + 0.02), (x * 0.2, y * 0.2, base + 1.1), 0.03, S, seg=8)
    common.box('guertel', (-0.2, -0.25, base + 1.2), (0.2, 0.25, base + 1.25), M['stone2'], bevel=0.02)
    # Arme: Oberarme am Koerper, Unterarme nach vorn oben, Haende betend vor der Brust
    for s in (-1, 1):
        sh = Vector((0.0, s * 0.24, base + 1.66))
        el = Vector((0.06, s * 0.2, base + 1.32))
        wr = Vector((0.24, s * 0.035, base + 1.5))
        _cyl(f'oberarm{s}', sh, el, 0.065, S, seg=14)
        _cyl(f'unterarm{s}', el, wr, 0.055, S, seg=14)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=10, radius=0.06, location=el)
        bpy.context.active_object.data.materials.append(S)
        hand = common.box(f'hand{s}', (-0.02, -0.018, 0.0), (0.02, 0.018, 0.17), S, bevel=0.015, segments=2)
        hand.matrix_world = Matrix.Translation(wr + Vector((0.0, -s * 0.016, 0.0))) @ Matrix.Rotation(-0.25, 4, 'Y')
        # Aermel: weite Stoffglocke am Unterarm
        _lathe(f'aermel{s}', [(0.0, 0.1), (0.12, 0.08), (0.2, 0.06)], S, seg=20)
        sl = bpy.context.active_object
        d = (wr - el).normalized()
        sl.matrix_world = Matrix.Translation(el + d * 0.02) @ d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    # Hals, Kopf gesenkt, Haar
    _cyl('hals', (0.0, 0.0, base + 1.72), (0.03, 0.0, base + 1.86), 0.055, S, seg=12)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=16, radius=0.115, location=(0.07, 0.0, base + 1.96))
    ob = bpy.context.active_object
    ob.scale = (1.0, 0.85, 1.18)
    ob.rotation_euler = (0, math.radians(25), 0)
    ob.data.materials.append(S)
    ob.data.shade_smooth()
    bpy.ops.mesh.primitive_uv_sphere_add(segments=28, ring_count=14, radius=0.125, location=(0.03, 0.0, base + 2.0))
    ob = bpy.context.active_object
    ob.scale = (1.05, 0.95, 0.95)
    ob.data.materials.append(M['stone2'])
    ob.data.shade_smooth()
    _cyl('haar_hinten', (-0.05, 0.0, base + 1.98), (-0.09, 0.0, base + 1.72), 0.09, M['stone2'], seg=16)
    for s in (-1, 1):
        _wing(f'fluegel{s}', s, S, base)


def build_greek_torso():
    """Verwitterte Frauenstatue ohne Kopf und Arme auf kleinem Sockel (1,7 m): Gewand, Huefte, Torso."""
    M = _m()
    S = M['stone2']
    common.box('sockel', (-0.3, -0.3, 0.0), (0.3, 0.3, 0.3), M['stone'], bevel=0.02)
    prof = [(0.3, 0.26), (0.5, 0.25), (0.9, 0.2), (1.1, 0.19), (1.2, 0.17), (1.35, 0.15), (1.5, 0.17), (1.6, 0.16), (1.68, 0.12),
            (1.72, 0.06)]
    _lathe('figur', prof, S, seg=36, scale_y=1.35)
    for k in range(7):
        a = -0.6 + k * 0.2
        _cyl(f'falte{k}', (math.cos(a) * 0.25, math.sin(a) * 0.32, 0.3), (math.cos(a) * 0.17, math.sin(a) * 0.22, 1.1), 0.02, S, seg=8)
    for s in (-1, 1):  # Armstuempfe
        _cyl(f'stumpf{s}', (0.0, s * 0.2, 1.55), (0.02, s * 0.27, 1.45), 0.05, S, seg=10)


# --- Tuecher ----------------------------------------------------------------------------------------------------------
def _drape(L, W, H, seed):
    """Weisses Tuch ueber einem Gegenstand (L x W x H): unterteilte Flaeche, die ueber Kanten faellt und Falten wirft."""
    M = _m()
    rng = random.Random(seed)
    bm = bmesh.new()
    nx, ny = 28, 22
    ext = 0.45
    verts = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            x = (i / nx - 0.5) * (L + 2 * ext)
            y = (j / ny - 0.5) * (W + 2 * ext)
            dx, dy = max(0.0, abs(x) - L / 2), max(0.0, abs(y) - W / 2)
            d = math.hypot(dx, dy)
            z = H - d * 2.4 if d < H / 2.4 else max(0.01, 0.02 + 0.03 * math.sin(i + j))
            z += 0.012 * math.sin(i * 1.7 + rng.random()) * (1 if d > 0 else 0.3)
            if d > 0:  # ueberhaengend: nach aussen gewellt
                x += math.copysign(0.03 * math.sin(j * 1.3 + i), x)
            verts[(i, j)] = bm.verts.new((x, y, z))
    for i in range(nx):
        for j in range(ny):
            bm.faces.new((verts[(i, j)], verts[(i + 1, j)], verts[(i + 1, j + 1)], verts[(i, j + 1)]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.z < -0.2:
            f.normal_flip()
    ob = common.obj_from_bmesh('tuch', bm, M['cloth'])
    ob.data.shade_smooth()
    mod = ob.modifiers.new('dicke', 'SOLIDIFY')
    mod.thickness = 0.008
    common.box('darunter', (-L / 2 + 0.05, -W / 2 + 0.05, 0.0), (L / 2 - 0.05, W / 2 - 0.05, H - 0.04), M['dark'])


def build_canvas_drape_a():
    _drape(1.4, 0.8, 1.1, 1)


def build_canvas_drape_b():
    _drape(2.2, 1.0, 1.6, 2)


def build_canvas_wall():
    """Weisse Bahn an der Wand (Wandflaeche x = 0, 3 x 2,8 m, unten am Boden), lose mit senkrechten Falten."""
    M = _m()
    bm = bmesh.new()
    nx, nz = 40, 12
    verts = {}
    for i in range(nx + 1):
        for k in range(nz + 1):
            y = (i / nx - 0.5) * 3.0
            z = k / nz * 2.8
            x = 0.02 + 0.05 * (0.5 + 0.5 * math.sin(i * 0.9)) * (1.0 - z / 2.8 * 0.6)
            verts[(i, k)] = bm.verts.new((x, y, z))
    for i in range(nx):
        for k in range(nz):
            bm.faces.new((verts[(i, k)], verts[(i + 1, k)], verts[(i + 1, k + 1)], verts[(i, k + 1)]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        if f.normal.x < 0:
            f.normal_flip()
    ob = common.obj_from_bmesh('bahn', bm, M['cloth'])
    ob.data.shade_smooth()


# --- Engelsbilder ---------------------------------------------------------------------------------------------------------
def _angel_painting(scribble):
    """Bild (1,0 x 1,3 m, Wandmontage): dunkler Grund, heller Engel-Umriss mit Fluegeln; scribble: Gesicht uebermalt."""
    frame = common.material('rahmen', (0.30, 0.22, 0.14), look='wood', rough=0.5)
    ground = common.material('grund', (0.16, 0.14, 0.13), look='paint', rough=0.85)
    figure = common.material('figur', (0.82, 0.78, 0.68), look='paint', rough=0.8)
    halo = common.material('schein', (0.80, 0.66, 0.30), look='paint', rough=0.6)
    black = common.material('kritzel', (0.04, 0.04, 0.04), look='paint', rough=0.9)
    W, H, fw = 1.0, 1.3, 0.06
    for (lo, hi) in (((0.0, -W / 2 - fw, H / 2), (0.04, W / 2 + fw, H / 2 + fw)), ((0.0, -W / 2 - fw, -H / 2 - fw), (0.04, W / 2 + fw, -H / 2)),
                     ((0.0, W / 2, -H / 2), (0.04, W / 2 + fw, H / 2)), ((0.0, -W / 2 - fw, -H / 2), (0.04, -W / 2, H / 2))):
        common.box('rahmen', lo, hi, frame, bevel=0.005)
    common.box('grund', (0.012, -W / 2, -H / 2), (0.013, W / 2, H / 2), ground)

    def poly(name, pts, mat, d):
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((0.013 + d, y, z)) for (y, z) in pts])
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()
        common.obj_from_bmesh(name, bm, mat)
    poly('gewand', [(-0.18, -0.55), (0.18, -0.55), (0.1, 0.15), (-0.1, 0.15)], figure, 0.001)
    for s in (-1, 1):
        poly(f'fluegel{s}', [(s * 0.08, 0.12), (s * 0.42, 0.38), (s * 0.38, 0.0), (s * 0.12, -0.2)], figure, 0.0012)
    pts = [(math.cos(a) * 0.17, 0.33 + math.sin(a) * 0.17) for a in [k * 2 * math.pi / 20 for k in range(20)]]
    poly('schein', pts, halo, 0.0014)
    pts = [(math.cos(a) * 0.08, 0.3 + math.sin(a) * 0.1) for a in [k * 2 * math.pi / 16 for k in range(16)]]
    poly('kopf', pts, figure, 0.0016)
    if scribble:
        rng = random.Random(4)
        for k in range(14):
            ob = common.box(f'kritzel{k}', (0.0156, -0.1, -0.006), (0.0162, 0.1, 0.006), black)
            ob.location = (0, rng.uniform(-0.03, 0.03), 0.3 + rng.uniform(-0.06, 0.06))
            ob.rotation_euler = (rng.uniform(-1.2, 1.2), 0, 0)


def build_painting_angel():
    _angel_painting(False)


def build_painting_angel_scribbled():
    _angel_painting(True)


# --- Aufzug, Schild, Kleinteile ------------------------------------------------------------------------------------------
def build_elevator_door():
    """Aufzugstuer im Durchbruch (Laibung 1 m, Oeffnung 1,4 m, Sturz 2,2 m): Edelstahlrahmen, Schiebetueren offen,
    Ruftaster, Etagenanzeige ueber der Tuer (Seite -x, zum Vorraum)."""
    M = _m()
    inox = common.material('edelstahl', (0.72, 0.73, 0.74), look='metal', rough=0.3, metal=1.0)
    lamp = common.material('anzeige', (1.0, 0.45, 0.18), look='glow', glow=2.0)
    W, H = 1.4, 2.2
    for s in (-1, 1):
        common.box(f'laibung{s}', (-0.5, s * W / 2 - (0.06 if s > 0 else 0), 0.0), (0.5, s * W / 2 + (0.0 if s > 0 else 0.06), H), inox)
        common.box(f'tuer{s}', (0.1, s * (W / 2 + 0.02), 0.0), (0.14, s * (W / 2 + 0.52), H - 0.02), inox)  # in der Wand geparkt
        common.box(f'blende{s}', (-0.52, s * W / 2 - 0.1 if s > 0 else -W / 2 - 0.02, 0.0),
                   (-0.5, s * W / 2 + 0.02 if s > 0 else -W / 2 + 0.1, H + 0.12), inox)
    common.box('sturz', (-0.5, -W / 2, H), (0.5, W / 2, H + 0.06), inox)
    common.box('anzeige_kasten', (-0.53, -0.25, H + 0.18), (-0.5, 0.25, H + 0.36), M['dark'], bevel=0.005)
    common.box('anzeige', (-0.535, -0.08, H + 0.22), (-0.53, 0.08, H + 0.32), lamp)
    common.box('taster_platte', (-0.53, -W / 2 - 0.25, 1.0), (-0.51, -W / 2 - 0.13, 1.3), inox, bevel=0.004)
    for z in (1.1, 1.2):
        _cyl(f'taster{z}', (-0.53, -W / 2 - 0.19, z), (-0.545, -W / 2 - 0.19, z), 0.018, lamp, seg=12)


def build_sign_warning():
    """Warnschild der Paranoia-Zonen (Wandmontage, 40 x 40 cm): gelb, schwarzer Rand, Auge im Dreieck (ohne Schrift)."""
    yel = common.material('gelb', (0.86, 0.70, 0.12), look='paint', rough=0.5)
    blk = common.material('schwarz', (0.04, 0.04, 0.04), look='paint', rough=0.6)
    common.box('platte', (0.0, -0.2, -0.2), (0.012, 0.2, 0.2), yel, bevel=0.004)
    for (y0, z0, y1, z1) in ((-0.2, -0.2, 0.2, -0.18), (-0.2, 0.18, 0.2, 0.2), (-0.2, -0.2, -0.18, 0.2), (0.18, -0.2, 0.2, 0.2)):
        common.box('rand', (0.012, y0, z0), (0.014, y1, z1), blk)
    bm = bmesh.new()
    outer = [bm.verts.new((0.0145, y, z)) for (y, z) in ((0.0, 0.14), (-0.15, -0.12), (0.15, -0.12))]
    f = bm.faces.new(outer)
    f.normal_update()
    if f.normal.x < 0:
        f.normal_flip()
    common.obj_from_bmesh('dreieck', bm, blk)
    bm = bmesh.new()
    eye = [bm.verts.new((0.016, math.cos(a) * 0.07, -0.02 + math.sin(a) * 0.03)) for a in [k * 2 * math.pi / 16 for k in range(16)]]
    f = bm.faces.new(eye)
    f.normal_update()
    if f.normal.x < 0:
        f.normal_flip()
    common.obj_from_bmesh('auge', bm, yel)
    _cyl('pupille', (0.016, 0.0, -0.02), (0.0175, 0.0, -0.02), 0.018, blk, seg=12)


def build_skull():
    bone = common.material('knochen', (0.78, 0.74, 0.64), look='ceramic', rough=0.7)
    dark = common.material('hoehle', (0.05, 0.04, 0.03), look='plastic', rough=0.9)
    bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=12, radius=0.09, location=(0.0, 0.0, 0.1))
    ob = bpy.context.active_object
    ob.scale = (1.15, 0.9, 0.95)
    ob.data.materials.append(bone)
    ob.data.shade_smooth()
    common.box('kiefer', (0.03, -0.05, 0.0), (0.12, 0.05, 0.06), bone, bevel=0.015, segments=2)
    for s in (-1, 1):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=8, radius=0.022, location=(0.09, s * 0.035, 0.1))
        bpy.context.active_object.data.materials.append(dark)


def build_train_bogie():
    """Liegendes Drehgestell eines Zuges (Teile von Zuegen, Wiki): zwei Radsaetze, Rahmen, Federn - rostig."""
    M = _m()
    for x in (-0.9, 0.9):
        _cyl(f'achse{x}', (x, -0.8, 0.42), (x, 0.8, 0.42), 0.06, M['rust'], seg=12)
        for s in (-1, 1):
            _cyl(f'rad{x}{s}', (x, s * 0.72, 0.42), (x, s * 0.8, 0.42), 0.42, M['rust'], seg=28)
            _cyl(f'kranz{x}{s}', (x, s * 0.8, 0.42), (x, s * 0.82, 0.42), 0.45, M['rust'], seg=28)
    for s in (-1, 1):
        common.box(f'rahmen{s}', (-1.4, s * 0.6 - 0.06, 0.5), (1.4, s * 0.6 + 0.06, 0.72), M['rust'], bevel=0.02)
        for x in (-0.9, 0.9):
            for k in range(4):
                _cyl(f'feder{s}{x}{k}', (x - 0.1 + k * 0.07, s * 0.6, 0.72), (x - 0.1 + k * 0.07, s * 0.6, 0.9), 0.03, M['steel'], seg=8)
    common.box('quer', (-0.2, -0.6, 0.55), (0.2, 0.6, 0.7), M['rust'], bevel=0.02)


def build_column_greek():
    """Griechisch-roemische Saeule 7,9 m (Sanktuarium): quadratische Plinthe, Wulste, kannelierter Schaft mit leichter
    Schwellung (24 Kanneluren), Echinus und Abakus."""
    M = _m()
    S = common.material('marmor', (0.92, 0.89, 0.84), look='concrete', tex='marble_beige', rough=0.5)
    H = 7.9
    common.box('plinthe', (-0.62, -0.62, 0.0), (0.62, 0.62, 0.22), S, bevel=0.02)
    for (z, r, h) in ((0.22, 0.56, 0.12), (0.34, 0.5, 0.06)):
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=r, depth=h, location=(0, 0, z + h / 2))
        ob = bpy.context.active_object
        ob.data.materials.append(S)
        common.shade_auto(ob, 40.0)
    bm = bmesh.new()
    flutes, per, rings = 24, 4, 14
    z0, z1 = 0.4, H - 0.75
    vs = []
    for k in range(rings + 1):
        t = k / rings
        z = z0 + (z1 - z0) * t
        r = 0.46 - 0.06 * t + 0.015 * math.sin(math.pi * t)
        ring = []
        for i in range(flutes * per):
            a = 2 * math.pi * i / (flutes * per)
            g = 0.022 * math.sin(math.pi * (i % per) / per) if (i % per) else 0.0
            ring.append(bm.verts.new((math.cos(a) * (r - g), math.sin(a) * (r - g), z)))
        vs.append(ring)
    n = flutes * per
    for k in range(rings):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((vs[k][i], vs[k][j], vs[k + 1][j], vs[k + 1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh('schaft', bm, S)
    common.shade_auto(ob, 50.0)
    for (z, r, h) in ((z1, 0.42, 0.06), (z1 + 0.06, 0.46, 0.08), (z1 + 0.14, 0.54, 0.16)):
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=r, depth=h, location=(0, 0, z + h / 2))
        ob = bpy.context.active_object
        ob.data.materials.append(S)
        common.shade_auto(ob, 40.0)
    common.box('abakus', (-0.66, -0.66, H - 0.45), (0.66, 0.66, H - 0.25), S, bevel=0.015)
    common.box('auflager', (-0.5, -0.5, H - 0.25), (0.5, 0.5, H), S, bevel=0.01)
