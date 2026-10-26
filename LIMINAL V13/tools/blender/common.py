"""V12: Hilfen fuer die Modell-Pipeline in Blender (Hintergrundmodus).

Konventionen wie im Spiel: Meter, z oben, das Modell blickt entlang +x, der Ursprung liegt am Fusspunkt bzw.
an der Befestigung (Wandmodelle: Rueckseite bei x = 0). Materialnamen steuern das Aussehen im Spiel
(props/glb_import.hpp): "look=<art> tex=<textur> glow=<staerke>"; die Farbe ist die Grundfarbe (linear).
Viewmodel-Teile (Hand, Taschenlampe) tragen "fm=<name>" fuer ein festes Material des Renderers.
"""
import math
import os

import bmesh
import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'data', 'models')


EXPORT_EXTRA = {}  # V13: zusaetzliche Exportoptionen eines Modells (z. B. Skelett und Posen der Hand)


def reset():
    global EXPORT_EXTRA
    EXPORT_EXTRA = {}
    bpy.ops.wm.read_factory_settings(use_empty=True)


def srgb_to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(name, rgb=(0.8, 0.8, 0.8), look='paint', tex=None, glow=0.0, rough=0.6, metal=0.0, fm=None, r=None, m=None):
    """Material mit Namen nach Konvention; rgb in sRGB (wie in den Level-Daten). fm: festes Material des
    Renderers (Viewmodel, render/world_renderer.hpp), dann zaehlen look/tex/glow im Spiel nicht.
    V13.3: r = Faktor auf die Rauheit der Textur im Spiel (0,3 glaenzend .. 1,5 stumpf), m = Metall-Zuschlag (0..1);
    ohne Angabe gilt der Look (props/glb_import.hpp). rough/metal wirken nur in Blender."""
    key = (f'fm={fm} ' if fm else '') + f'look={look}' + (f' tex={tex}' if tex else '') + \
        (f' glow={glow:g}' if glow > 0 else '') + (f' r={r:g}' if r is not None else '') + \
        (f' m={m:g}' if m is not None else '') + f' #{name}'
    mat = bpy.data.materials.get(key) or bpy.data.materials.new(key)
    m = mat
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    lin = [srgb_to_lin(c) for c in rgb]
    bsdf.inputs['Base Color'].default_value = (*lin, 1.0)
    bsdf.inputs['Roughness'].default_value = rough
    bsdf.inputs['Metallic'].default_value = metal
    return m


def obj_from_bmesh(name, bm, mat=None):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if mat is not None:
        me.materials.append(mat)
    return ob


def box(name, lo, hi, mat=None, bevel=0.0, segments=2):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x = lo[0] + (v.co.x + 0.5) * (hi[0] - lo[0])
        v.co.y = lo[1] + (v.co.y + 0.5) * (hi[1] - lo[1])
        v.co.z = lo[2] + (v.co.z + 0.5) * (hi[2] - lo[2])
    ob = obj_from_bmesh(name, bm, mat)
    if bevel > 0:
        mod = ob.modifiers.new('bevel', 'BEVEL')
        mod.width = bevel
        mod.segments = segments
        mod.limit_method = 'ANGLE'
        mod.harden_normals = True
    return ob


def profile_extrude(name, profile, length, mat=None, axis='y', smooth_deg=35.0):
    """Profil (Liste von (u, z) in der Ebene senkrecht zur Achse, gegen den Uhrzeigersinn) entlang axis
    von 0 bis length ziehen - Leisten, Rahmen, Gesimse."""
    bm = bmesh.new()
    ring0, ring1 = [], []
    for (u, z) in profile:
        if axis == 'y':
            ring0.append(bm.verts.new((u, 0.0, z)))
            ring1.append(bm.verts.new((u, length, z)))
        else:
            ring0.append(bm.verts.new((0.0, u, z)))
            ring1.append(bm.verts.new((length, u, z)))
    n = len(profile)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ring0[i], ring0[j], ring1[j], ring1[i]))
    bm.faces.new(list(reversed(ring0)))
    bm.faces.new(ring1)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = obj_from_bmesh(name, bm, mat)
    shade_auto(ob, smooth_deg)
    return ob


def shade_auto(ob, deg=35.0):
    ob.data.shade_smooth()
    mod = ob.modifiers.new('weighted', 'WEIGHTED_NORMAL')
    mod.keep_sharp = True
    for p in ob.data.polygons:
        p.use_smooth = True
    try:
        ob.data.set_sharp_from_angle(angle=math.radians(deg))
    except AttributeError:
        pass


def export(name):
    """Alles in der Szene als data/models/<name>.glb (Modifikatoren angewendet, +Y oben wie glTF)."""
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + '.glb')
    opts = dict(filepath=path, export_format='GLB', export_apply=True, export_yup=True,
                export_texcoords=True, export_normals=True, export_materials='EXPORT',
                export_cameras=False, export_lights=False, export_animations=False)
    opts.update(EXPORT_EXTRA)
    bpy.ops.export_scene.gltf(**opts)
    tris = 0
    for ob in bpy.context.scene.objects:
        if ob.type == 'MESH':
            dg = bpy.context.evaluated_depsgraph_get()
            me = ob.evaluated_get(dg).to_mesh()
            me.calc_loop_triangles()
            tris += len(me.loop_triangles)
    print(f'MODELL {name}: {tris} Dreiecke -> {path}', flush=True)
    return path
