"""V13: Vorschau der Hand (models_hand) aus mehreren Richtungen. Aufruf aus dem V13-Ordner:
  .toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/preview_hand.py -- <ordner> [pose] [stufe]
pose: rest (Standard) | grip | press. Schreibt <ordner>/hand_<pose>_<ansicht>.png.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402
from models import models_hand  # noqa: E402


def cam(name, loc, look_at, ortho):
    c = bpy.data.cameras.new(name)
    c.type = 'ORTHO'
    c.ortho_scale = ortho
    c.clip_start = 0.001
    ob = bpy.data.objects.new(name, c)
    bpy.context.scene.collection.objects.link(ob)
    ob.location = loc
    d = Vector(look_at) - Vector(loc)
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    return ob


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    out = args[0]
    pose = args[1] if len(args) > 1 else 'rest'
    levels = int(args[2]) if len(args) > 2 else 2
    os.makedirs(out, exist_ok=True)
    common.reset()
    eye = None
    if pose == 'rest':
        ob, arm = models_hand.make_hand(levels)
        ctr = Vector((0.045, 0.0, 0.0))
        size = 0.30
    elif pose in ('final', 'press'):
        # fertiges Modell im Rahmen der Lampe (wie im Spiel), Lampe als Zylinder auf der x-Achse
        log = []
        ob, arm, sw = models_hand.build_final(log)
        for line in log:
            print(line, flush=True)
        if pose == 'press':
            act = bpy.data.actions['press']
            arm.animation_data.action = act
            bpy.context.scene.frame_set(1)
        models_hand._lamp_cylinder((-0.06, 0, 0), (1, 0, 0))
        ctr = Vector((0.0, 0.0, 0.0))
        size = 0.20
        eye = (-0.40, -0.15, 0.17)
    else:
        ob, arm = models_hand.build_posed(pose)
        ctr = Vector((0.0, 0.0, 0.0))
        size = 0.26
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'MATERIAL'
    sh.show_cavity = True
    sh.cavity_type = 'BOTH'
    sh.show_shadows = False
    sh.show_object_outline = False
    sc.render.resolution_x, sc.render.resolution_y = 1200, 900
    sc.world = bpy.data.worlds.new('w')
    sh.background_type = 'VIEWPORT'
    sh.background_color = (0.16, 0.16, 0.17)
    for m in bpy.data.materials:
        b = m.node_tree.nodes.get('Principled BSDF') if m.use_nodes else None
        if b:
            m.diffuse_color = b.inputs['Base Color'].default_value
    arm.hide_render = True
    D = 0.5
    views = [('oben', (0, 0, D), (0, 1, 0)), ('unten', (0, 0, -D), (0, 1, 0)), ('seite_daumen', (0, -D, 0), (0, 0, 1)),
             ('seite_klein', (0, D, 0), (0, 0, 1)), ('vorn', (D, 0, 0), (0, 0, 1)), ('schraeg', (-0.25, -0.3, 0.3), (0, 0, 1)),
             ('schraeg2', (0.3, 0.3, -0.25), (0, 0, 1))]
    if eye:
        views.append(('auge', eye, None))
    for n, off, up in views:
        if n == 'auge':
            cd = bpy.data.cameras.new('auge')
            cd.sensor_fit = 'VERTICAL'
            cd.angle_y = math.radians(30.0)
            cd.clip_start = 0.01
            c = bpy.data.objects.new('auge', cd)
            sc.collection.objects.link(c)
            c.location = Vector(off)
            c.rotation_euler = (ctr - Vector(off)).to_track_quat('-Z', 'Y').to_euler()
        else:
            c = cam(n, ctr + Vector(off), ctr, size)
        sc.camera = c
        sc.render.filepath = os.path.join(out, f'hand_{pose}_{n}.png')
        bpy.ops.render.render(write_still=True)
    print('FERTIG', out, flush=True)


if __name__ == '__main__':
    main()
