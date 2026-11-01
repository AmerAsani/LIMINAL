"""V13: Wesen bauen (KREATUREN_ANALYSE.md). Aufruf aus dem V13-Ordner:
  .toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/creatures/build.py -- <wesen> [--preview <ordner>] [--grob]
<wesen>: hound, smiler, clump, gestalt, moth, nest; V13.2: faceling, faceling_evo, faceling_dress, skinstealer, wretch, rat,
rat_bald, window; V13.3: duller, arms, wrangler, wrangler_f, wrangler_fs, wrangler_brood. Schreibt data/models/ent_<wesen>_l0..l2.glb (Varianten mit SKELETON im Modul).

Je Wesen liefert ein Modul (creatures/<wesen>.py) eine Funktion build(J, bones) -> Liste von Gruppen:
  {'name', 'fm' (Material im Spiel), 'parts' (sdf.Part), 'disp' (Verschiebung oder None), 'h' (Voxel, m)}
und LODS = (Dreiecke Stufe 0, 1, 2). Gewichte kommen aus den formenden Teilen (Knochen je Teil).
"""
import importlib
import math
import os
import sys
import time

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import common  # noqa: E402
import sdf  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.dirname(HERE)), '..', 'data', 'models')
OUT = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'data', 'models'))
MIRROR = np.array([1.0, -1.0, 1.0])  # Spiel -> Blender (der glb-Leser spiegelt zurueck)


def log(*a):
    print(*a, flush=True)


def make_material(fm, rgb):
    m = bpy.data.materials.new(f'fm={fm} look=plastic #{fm}')
    m.use_nodes = True
    b = m.node_tree.nodes.get('Principled BSDF')
    b.inputs['Base Color'].default_value = (*rgb, 1.0)
    m.diffuse_color = (*rgb, 1.0)
    return m


PREVIEW_RGB = {'ent_skin': (0.55, 0.47, 0.40), 'ent_hair': (0.03, 0.03, 0.03), 'ent_tooth': (0.85, 0.82, 0.70),
               'ent_mouth': (0.35, 0.12, 0.12), 'ent_eyeshine': (0.9, 0.9, 0.85), 'ent_glow': (1, 1, 0.95),
               'ent_glow_dim': (0.8, 0.8, 0.7), 'ent_glow_eye': (0.9, 0.95, 1.0), 'ent_shadow': (0.02, 0.02, 0.02),
               'ent_wing': (0.35, 0.30, 0.25), 'ent_chitin': (0.20, 0.15, 0.10), 'ent_fur': (0.45, 0.40, 0.35),
               'ent_motheye': (0.05, 0.05, 0.05), 'ent_nest': (0.45, 0.25, 0.15), 'ent_horn': (0.4, 0.35, 0.3),
               'clump_a': (0.75, 0.58, 0.50), 'clump_b': (0.55, 0.40, 0.32), 'clump_c': (0.36, 0.25, 0.20),
               'clump_core': (0.55, 0.35, 0.40), 'ent_darkskin': (0.06, 0.05, 0.05),
               'ent_silhouette': (0.03, 0.03, 0.03), 'ent_resin': (0.7, 0.42, 0.12), 'ent_nestflesh': (0.30, 0.13, 0.10),
               'ent_nestegg': (0.70, 0.62, 0.42), 'ent_cloth_a': (0.30, 0.29, 0.27), 'ent_cloth_b': (0.16, 0.17, 0.20),
               'ent_cloth_pink': (0.78, 0.42, 0.50), 'ent_paleskin': (0.85, 0.78, 0.52), 'ent_wretchskin': (0.55, 0.30, 0.22),
               'ent_stone': (0.55, 0.53, 0.50), 'ent_ratfur': (0.08, 0.08, 0.08), 'ent_hornglow': (0.9, 0.2, 0.1),
               'ent_view': (0.6, 0.62, 0.55), 'ent_ooze': (0.35, 0.18, 0.10), 'ent_humanskin': (0.62, 0.47, 0.40),
               'ent_ratskin': (0.62, 0.48, 0.48), 'ent_umbra': (0.02, 0.02, 0.02),
               'ent_cloth_c': (0.20, 0.26, 0.36), 'ent_cloth_d': (0.17, 0.20, 0.14), 'ent_humanskin_b': (0.38, 0.26, 0.21),
               'ent_hair_grey': (0.55, 0.53, 0.5),
               'ent_dullerskin': (0.30, 0.30, 0.32), 'ent_rotskin': (0.45, 0.47, 0.40), 'ent_smoke': (0.5, 0.5, 0.5), 'ent_concrete': (0.7, 0.7, 0.68),  # V13.3
               # V13.3 Wrangler
               'ent_wr_membrane': (0.62, 0.60, 0.56), 'ent_wr_membrane_f': (0.80, 0.66, 0.62), 'ent_wr_hide': (0.42, 0.40, 0.38),
               'ent_wr_gut': (0.30, 0.08, 0.10), 'ent_wr_pith': (0.62, 0.64, 0.46), 'ent_wr_bone': (0.80, 0.76, 0.66),
               'ent_wr_slime': (0.55, 0.57, 0.50), 'ent_wr_seed': (0.72, 0.60, 0.36), 'ent_wr_dust': (0.62, 0.60, 0.56),
               'ent_wr_crack': (0.05, 0.05, 0.05), 'ent_wr_pincer': (0.12, 0.08, 0.06), 'ent_wr_debris': (0.55, 0.53, 0.5)}


def build_group(g, nb):
    """Netz einer Gruppe: Punkte, Dreiecke (Spielkoordinaten), Gewichte, Hoehlung/Kante."""
    t0 = time.time()
    F, lo = sdf.evaluate(g['parts'], g['h'], g.get('disp'))
    V, T = sdf.polygonize(F, lo, g['h'], g.get('adapt', 0.0))
    del F
    if g.get('tris') and len(T) > g['tris']:  # V13.3: vor dem Weiterrechnen ausduennen (Teile, die vielfach geklont werden)
        V, T = reduce_mesh(V, T, g['tris'])
    N = sdf.vertex_normals(V, T)
    # Hoehlung auch aus Nachbargruppen (cav_extra): wo zwei Netze ineinanderwachsen, liegt eine Falte
    cav, edge = sdf.cavity_edge(g['parts'] + g.get('cav_extra', []), V, N, g.get('disp'), g.get('cav_scale', 0.012))
    W = sdf.weights(g['parts'], V, nb, s=g.get('wsoft', 0.004))
    log(f'  Gruppe {g["name"]}: {len(V)} Punkte, {len(T)} Dreiecke, {time.time() - t0:.1f} s')
    return V, T, W, cav, edge


def reduce_mesh(V, T, target):
    """V13.3: Netz (Spielkoordinaten) auf etwa target Dreiecke ausduennen (Blender, Kantenkollaps)."""
    me = bpy.data.meshes.new('tmp')
    me.from_pydata(V.tolist(), [], T.tolist())
    me.update()
    ob = bpy.data.objects.new('tmp', me)
    bpy.context.scene.collection.objects.link(ob)
    decimate(ob, target)
    me = ob.data
    V2 = np.zeros(len(me.vertices) * 3)
    me.vertices.foreach_get('co', V2)
    T2 = np.array([list(p.vertices) for p in me.polygons], dtype=np.int64)
    bpy.data.objects.remove(ob, do_unlink=True)
    return V2.reshape(-1, 3), T2


def to_object(name, V, T, W, cav, edge, mat, nb):
    """Blender-Objekt (gespiegelt), Gewichte als Gruppen B00.., uv = (Hoehlung, Kante)."""
    me = bpy.data.meshes.new(name)
    Vb = V * MIRROR
    Tb = T[:, [0, 2, 1]]  # Spiegelung kehrt den Umlaufsinn um
    me.from_pydata(Vb.tolist(), [], Tb.tolist())
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(mat)
    uv = me.uv_layers.new(name='UVMap')
    loops = np.zeros(len(me.loops) * 2)
    vi = np.zeros(len(me.loops), dtype=np.int64)
    me.loops.foreach_get('vertex_index', vi)
    loops[0::2] = cav[vi]
    loops[1::2] = edge[vi]
    uv.data.foreach_set('uv', loops)
    # Gewichte: hoechstens 4 je Punkt (Export normiert), Gruppen nur fuer benutzte Knochen
    top = np.argsort(-W, axis=1)[:, :4]
    groups = {}
    for i in range(len(V)):
        ws = W[i, top[i]]
        s = ws.sum()
        for b, w in zip(top[i], ws):
            if w / s < 0.01:
                continue
            if b not in groups:
                groups[b] = ob.vertex_groups.new(name=f'B{b:02d}')
            groups[b].add([i], float(w / s), 'REPLACE')
    for poly in me.polygons:
        poly.use_smooth = True
    return ob


def make_armature(J, bones):
    arm = bpy.data.armatures.new('rig')
    ob = bpy.data.objects.new('rig', arm)
    bpy.context.scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode='EDIT')
    ebs = []
    for i, b in enumerate(bones):
        M = b['frame']
        o, x, z = M[:3, 3], M[:3, 0], M[:3, 2]
        ln = float(np.linalg.norm(J[b['b']] - J[b['a']])) if b['a'] != b['b'] else 0.08
        ln = max(ln, 0.01)
        e = arm.edit_bones.new(f'B{i:02d}')
        e.head = Vector(o * MIRROR)
        e.tail = Vector((o + x * ln) * MIRROR)
        e.align_roll(Vector(z * MIRROR))
        e.use_deform = True
        ebs.append(e)
    for i, b in enumerate(bones):
        if b['parent'] >= 0:
            ebs[i].parent = ebs[b['parent']]
    bpy.ops.object.mode_set(mode='OBJECT')
    return ob


def decimate(ob, target):
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if tris <= target:
        return tris
    mod = ob.modifiers.new('red', 'DECIMATE')
    mod.ratio = target / tris
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = ob
    with bpy.context.temp_override(object=ob, active_object=ob, selected_objects=[ob]):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def export(path, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[-1]
    bpy.ops.export_scene.gltf(filepath=path, export_format='GLB', use_selection=True, export_apply=False,
                              export_yup=True, export_texcoords=True, export_normals=True, export_materials='EXPORT',
                              export_skins=True, export_animations=False, export_def_bones=False,
                              export_cameras=False, export_lights=False)


def preview(objs, arm, out, name, views=None):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'MATERIAL'
    sh.show_cavity = True
    sh.cavity_type = 'BOTH'
    sc.render.resolution_x, sc.render.resolution_y = 1100, 800
    sc.world = bpy.data.worlds.new('w')
    sh.background_type = 'VIEWPORT'
    sh.background_color = (0.16, 0.16, 0.17)
    for o in bpy.context.scene.objects:
        o.hide_render = o not in objs
    lo = np.min([np.array([o.matrix_world @ Vector(c) for c in o.bound_box]) for o in objs], axis=(0, 1))
    hi = np.max([np.array([o.matrix_world @ Vector(c) for c in o.bound_box]) for o in objs], axis=(0, 1))
    ctr = Vector(((lo + hi) / 2).tolist())
    size = float(np.max(hi - lo)) * 1.15
    D = size * 3
    views = views or [('vorn', (1, 0, 0.15)), ('seite', (0.05, -1, 0.1)), ('schraeg', (0.7, -0.6, 0.45)),
                      ('hinten', (-0.8, 0.4, 0.3)), ('oben', (0.01, 0, 1)), ('unten', (0.3, 0.2, -1))]
    for vn, d in views:
        cd = bpy.data.cameras.new(vn)
        cd.type = 'ORTHO'
        cd.ortho_scale = size * 1.4  # Bild ist breiter als hoch - sonst schneidet es hohe Wesen ab
        cd.clip_end = 100
        c = bpy.data.objects.new(vn, cd)
        sc.collection.objects.link(c)
        dv = Vector(d).normalized()
        c.location = ctr + dv * D
        c.rotation_euler = (-dv).to_track_quat('-Z', 'Y').to_euler()
        sc.camera = c
        sc.render.filepath = os.path.join(out, f'{name}_{vn}.png')
        bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    name = argv[0]
    prev = argv[argv.index('--preview') + 1] if '--preview' in argv else None
    coarse = '--grob' in argv
    common.reset()
    mod = importlib.import_module(name)
    model = 'ent_' + name
    J, bones = sdf.skeleton(getattr(mod, 'SKELETON', model))  # V13.2: Varianten nutzen das Skelett des Wesens
    nb = len(bones)
    t0 = time.time()
    # V13.2: SCALE - das Modul baut in eigenem Massstab (Ratte: entworfen in 1/SCALE der Spielgroesse), das Netz wird danach
    # auf die Spielgroesse gebracht
    S = float(getattr(mod, 'SCALE', 1.0))
    groups = mod.build(np.asarray(J, dtype=float) / S if S != 1.0 else J, bones)
    # V13.3: fertige Netze je Stufe ('lod_mesh': je Stufe (V, T, W oder None[, Hoehlung, Kante])) werden nicht ausgeduennt -
    # duenne Glieder und Haare zerfallen beim Kantenkollaps zu flachen Dreiecken (Nguithr'xurh: Beine, Taster, Haare)
    fixed = [g for g in groups if 'lod_mesh' in g]
    groups = [g for g in groups if 'lod_mesh' not in g]
    if coarse:
        for g in groups:
            g['h'] *= 2.0
    # Gruppen mit gleichem 'merge' werden nach dem Vernetzen zu einem Netz (ein Zeichenaufruf) - so koennen weit
    # verstreute Teile (Naegel an zwoelf Haenden) je fuer sich auf kleinem Gitter entstehen
    built = []
    for g in groups:
        V, T, W, cav, edge = build_group(g, nb)
        # V13.3: Abzuege desselben Netzes an anderen Stellen (Ringe des Wranglers): je Abzug Versatz und Knochen - alle
        # Gewichte des Abzugs gehen an diesen Knochen. Ein Gitter statt vieler gleicher.
        if g.get('clones'):
            Vs, Ts, Ws, Cs, Es = [V], [T], [W], [cav], [edge]
            for off, bone in g['clones']:
                W2 = np.zeros_like(W)
                W2[:, bone] = 1.0
                Ts.append(T + sum(len(x) for x in Vs))
                Vs.append(V + np.asarray(off, dtype=float))
                Ws.append(W2)
                Cs.append(cav)
                Es.append(edge)
            V, T, W, cav, edge = np.vstack(Vs), np.vstack(Ts), np.vstack(Ws), np.concatenate(Cs), np.concatenate(Es)
        V = V * S
        key = g.get('merge')
        tgt = next((b for b in built if key and b['key'] == key), None)
        if tgt is None:
            built.append({'key': key, 'name': key or g['name'], 'fm': g['fm'], 'V': V, 'T': T, 'W': W, 'cav': cav, 'edge': edge})
            continue
        assert tgt['fm'] == g['fm'], f'{g["name"]}: anderes Material als {key}'
        tgt['T'] = np.vstack([tgt['T'], T + len(tgt['V'])])
        tgt['V'] = np.vstack([tgt['V'], V])
        tgt['W'] = np.vstack([tgt['W'], W])
        tgt['cav'] = np.concatenate([tgt['cav'], cav])
        tgt['edge'] = np.concatenate([tgt['edge'], edge])
    objs = []
    for b in built:
        mat = make_material(b['fm'], PREVIEW_RGB.get(b['fm'], (0.5, 0.5, 0.5)))
        objs.append(to_object(b['name'], b['V'], b['T'], b['W'], b['cav'], b['edge'], mat, nb))
    arm = make_armature(J, bones)
    for o in objs:
        o.parent = arm
        m = o.modifiers.new('skelett', 'ARMATURE')
        m.object = arm
    log(f'{model}: Form fertig in {time.time() - t0:.1f} s')

    def fixed_objs(lod):  # V13.3
        out = []
        for g in fixed:
            m = g['lod_mesh'][min(lod, len(g['lod_mesh']) - 1)]
            V, T = np.asarray(m[0], dtype=float), np.asarray(m[1], dtype=np.int64)
            if len(T) == 0:
                continue
            W = m[2] if len(m) > 2 and m[2] is not None else sdf.weights(g['parts'], V, nb, s=g.get('wsoft', 0.004))
            if len(m) > 4:
                cav, edge = np.asarray(m[3], dtype=float), np.asarray(m[4], dtype=float)
            else:
                cav, edge = sdf.cavity_edge(g['parts'], V, sdf.vertex_normals(V, T), None, g.get('cav_scale', 0.003))
            o = to_object(f'{g["name"]}_l{lod}', V * S, T, W, cav, edge, make_material(g['fm'], PREVIEW_RGB.get(g['fm'], (0.5, 0.5, 0.5))), nb)
            o.parent = arm
            o.modifiers.new('skelett', 'ARMATURE').object = arm
            out.append(o)
        return out
    if prev:
        os.makedirs(prev, exist_ok=True)
        fx = fixed_objs(0)
        hide = getattr(mod, 'PREVIEW_HIDE', ())  # V13.3: z. B. der 3 m lange Faden - sonst ist das Wesen im Bild winzig
        preview([o for o in objs + fx if not any(o.name.startswith(h) for h in hide)], arm, prev, model)
        for o in fx:
            bpy.data.objects.remove(o, do_unlink=True)
    # Stufen: Anteil der Dreiecke je Gruppe nach ihrem Anteil am Ganzen, gewichtet mit 'lod_weight' (flache Teile wie
    # Fluegel brauchen wenig, feine wie Augen, Haar und Beine mehr)
    lw = {}
    for g in groups:
        k = g.get('merge') or g['name']
        lw[k] = max(lw.get(k, 0.0), g.get('lod_weight', 1.0))
    tri_of = lambda o: sum(len(p.vertices) - 2 for p in o.data.polygons) * lw.get(o.name, 1.0)
    total = sum(tri_of(o) for o in objs)
    # V13.2: jede Stufe aus der vorigen (vorher jede aus dem vollen Netz - beim Nest ueber 4 Mio. Dreiecke je Stufe)
    prev_copies = None
    for lod, budget in enumerate(mod.LODS):
        copies = []
        tri = 0
        for i, o in enumerate(objs):
            src = prev_copies[i] if prev_copies else o
            c = src.copy()
            c.data = src.data.copy()
            bpy.context.scene.collection.objects.link(c)
            share = tri_of(o) / total
            tri += decimate(c, max(64, int(budget * share)))
            c.name = f'{o.name}_l{lod}'
            copies.append(c)
        fx = fixed_objs(lod)  # V13.3
        tri += sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in fx)
        path = os.path.join(OUT, f'{model}_l{lod}.glb')
        export(path, copies + fx + [arm])
        log(f'{model}_l{lod}: {tri} Dreiecke -> {path}')
        for o in fx:
            bpy.data.objects.remove(o, do_unlink=True)
        if prev_copies:
            for c in prev_copies:
                bpy.data.objects.remove(c, do_unlink=True)
        prev_copies = copies
    for c in prev_copies or []:
        bpy.data.objects.remove(c, do_unlink=True)


if __name__ == '__main__':
    main()
