"""V12: Vorschau eines Modells aus Blender (Workbench) aus vier Richtungen. Aufruf (aus dem V12-Ordner):
  .toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/preview_model.py -- <name> <ordner>
Schreibt <ordner>/<name>_front.png, _side.png, _back.png, _top.png (Kamera auf die Huelle des Modells gerichtet).
"""
import importlib
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_models  # noqa: E402
import common  # noqa: E402


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    name, out = args[0], args[1]
    os.makedirs(out, exist_ok=True)
    builders = {}
    for modname in build_models.MODULES:
        mod = importlib.import_module('models.' + modname)
        for k in dir(mod):
            if k.startswith('build_'):
                builders[k[6:]] = getattr(mod, k)
    common.reset()
    builders[name]()
    dg = bpy.context.evaluated_depsgraph_get()
    lo, hi = Vector((1e9, 1e9, 1e9)), Vector((-1e9, -1e9, -1e9))
    for ob in bpy.context.scene.objects:
        if ob.type != 'MESH':
            continue
        for c in ob.evaluated_get(dg).bound_box:
            p = ob.matrix_world @ Vector(c)
            lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
            hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
    ctr, size = (lo + hi) / 2, max((hi - lo).length, 0.2)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light, sh.color_type, sh.show_cavity, sh.show_shadows = 'STUDIO', 'MATERIAL', True, True
    sh.background_type, sh.background_color = 'VIEWPORT', (0.2, 0.19, 0.18)
    sc.render.resolution_x, sc.render.resolution_y = 720, 720
    for m in bpy.data.materials:
        b = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        if b:
            m.diffuse_color = b.inputs['Base Color'].default_value
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    cam.data.angle = math.radians(40)
    sc.camera = cam
    for tag, d in (('front', Vector((1, -0.35, 0.25))), ('side', Vector((0.15, 1, 0.2))), ('back', Vector((-1, 0.3, 0.3))),
                   ('top', Vector((0.4, -0.3, 1)))):
        d.normalize()
        cam.location = ctr + d * size * 1.6
        cam.rotation_euler = (ctr - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(out, f'{name}_{tag}.png')
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    main()
