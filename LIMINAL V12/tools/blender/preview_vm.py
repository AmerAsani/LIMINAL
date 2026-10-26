"""V12: Vorschau des Viewmodels (Hand + Lampe) aus Blender, ohne das Spiel. Aufruf (aus dem V12-Ordner):
  .toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/preview_vm.py -- <ordner>
Schreibt <ordner>/vm_eye.png (aus dem Auge wie im Spiel: Griff 0,40 m vor, 0,15 m links, 0,25 m unter dem
Auge, senkrechtes Sichtfeld 62 Grad), vm_side.png, vm_top.png und vm_front.png.
"""
import math
import os
import sys

import bpy
from mathutils import Euler, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402
from models import models_vm  # noqa: E402


def cam(name, loc, look_at, fov_y=None, ortho=None):
    c = bpy.data.cameras.new(name)
    if ortho:
        c.type = 'ORTHO'
        c.ortho_scale = ortho
    else:
        c.sensor_fit = 'VERTICAL'
        c.angle_y = math.radians(fov_y)
    c.clip_start = 0.01
    ob = bpy.data.objects.new(name, c)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = loc
    d = Vector(look_at) - Vector(loc)
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    return ob


def main():
    out = sys.argv[sys.argv.index('--') + 1]
    os.makedirs(out, exist_ok=True)
    common.reset()
    models_vm.build_vm_flashlight_on()
    models_vm.build_vm_hand_l()
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'MATERIAL'
    sh.show_cavity = True
    sh.show_shadows = True
    sc.render.resolution_x, sc.render.resolution_y = 960, 540
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new('w')
    sh.background_type = 'VIEWPORT'
    sh.background_color = (0.18, 0.17, 0.15)
    for m in bpy.data.materials:
        b = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        if b:
            m.diffuse_color = b.inputs['Base Color'].default_value
    eye = (-0.40, -0.15, 0.25)
    views = [('vm_eye', cam('eye', eye, (0.6, -0.15, 0.20), fov_y=62.0)),
             ('vm_eyezoom', cam('eyezoom', eye, (0.0, 0.0, 0.0), fov_y=22.0)),
             ('vm_side', cam('side', (0.03, 0.45, 0.02), (0.03, 0.0, 0.0), ortho=0.30)),
             ('vm_top', cam('top', (0.03, 0.0, 0.45), (0.03, 0.0, 0.0), ortho=0.30)),
             ('vm_front', cam('front', (0.45, -0.12, 0.10), (0.0, 0.0, 0.0), ortho=0.26))]
    for n, c in views:
        sc.camera = c
        sc.render.filepath = os.path.join(out, n + '.png')
        bpy.ops.render.render(write_still=True)
        print('BILD', sc.render.filepath, flush=True)


if __name__ == '__main__':
    main()
