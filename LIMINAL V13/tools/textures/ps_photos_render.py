"""V13.3 (Abnahme Level 2): Fotos fuer Photoshop (Entity 104) in Blender/Cycles rendern - keine gemalten Flaechen mehr.

Die Seite (Wikidot Entity 104 Rev. 43, CC BY-SA 3.0): ein Klotz, auf allen Seiten mit Fotos von Fleisch beklebt, das Gesicht
aus zwei Augenfotos und dem Foto eines weit aufgerissenen Pitbull-Mauls. Hier entstehen diese Fotos als echte Aufnahmen
gebauter Szenen (Fleisch, Haut und Fell mit Lichtstreuung, nass glaenzend, Blitz an der Kamera, geringe Schaerfentiefe).
Alles eigene Geometrie und eigene Shader, keine fremden Bilder.

    blender -b -P tools/textures/ps_photos_render.py -- <ordner> [name ...] [--samples 96] [--scale 1.0]
Namen: meat_steak meat_mince meat_belly meat_liver meat_osso meat_guts meat_tongue meat_skin eye_a eye_b mouth
Ausgabe <ordner>/<name>.png (sRGB). Das Zusammensetzen zum Atlas macht tools/textures/generate.py (ps_photos).
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

V = Vector
TAU = 2.0 * math.pi
SIZES = {  # Bildgroesse je Foto (Pixel bei scale 1)
    'mouth': (1024, 768), 'eye_a': (1024, 400), 'eye_b': (1024, 400),
}
MEATS = ['meat_steak', 'meat_mince', 'meat_belly', 'meat_liver', 'meat_osso', 'meat_guts', 'meat_tongue', 'meat_skin']


# =====================================================================================================================
# Grundlagen: Szene, Kamera mit Blitz, Knoten
# =====================================================================================================================
def reset(samples):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = samples
    sc.cycles.use_adaptive_sampling = True
    sc.cycles.use_denoising = True
    sc.cycles.max_bounces = 8
    sc.cycles.transparent_max_bounces = 16
    try:
        sc.cycles.denoiser = 'OPENIMAGEDENOISE'
    except TypeError:
        pass
    sc.view_settings.view_transform = 'AgX'
    try:
        sc.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGB'
    w = bpy.data.worlds.new('welt')
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.012, 0.010, 0.010, 1.0)
    w.node_tree.nodes['Background'].inputs[1].default_value = 1.0
    return sc


def camera(sc, loc, target, lens=60.0, fstop=5.6, flash=40.0, flash_off=(0.0, 0.05), fill=0.0, roll=0.0):
    """Kamera mit Aufsteckblitz: kleine, harte Flaechenleuchte knapp ueber dem Objektiv (wie ein Handyfoto mit Blitz)."""
    cd = bpy.data.cameras.new('kamera')
    cd.lens = lens
    cd.dof.use_dof = fstop > 0
    cd.dof.aperture_fstop = max(0.5, fstop)
    cd.clip_start = 0.002
    cam = bpy.data.objects.new('kamera', cd)
    sc.collection.objects.link(cam)
    cam.location = V(loc)
    d = (V(target) - V(loc))
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    if roll:
        cam.rotation_euler.rotate_axis('Z', roll)
    cd.dof.focus_distance = d.length
    sc.camera = cam
    bpy.context.view_layer.update()
    R = cam.matrix_world.to_3x3()
    ld = bpy.data.lights.new('blitz', 'AREA')
    ld.energy = flash
    ld.size = 0.012
    ld.color = (1.0, 0.97, 0.92)
    lo = bpy.data.objects.new('blitz', ld)
    sc.collection.objects.link(lo)
    lo.location = V(loc) + R @ V((flash_off[0], flash_off[1], 0.0))
    lo.rotation_euler = cam.rotation_euler
    if fill > 0:  # schwaches Raumlicht von oben (Leuchtstoffroehre)
        fd = bpy.data.lights.new('raum', 'AREA')
        fd.energy = fill
        fd.size = 0.6
        fd.color = (0.95, 1.0, 0.92)
        fo = bpy.data.objects.new('raum', fd)
        sc.collection.objects.link(fo)
        fo.location = V(target) + V((0.15, 0.3, 0.9))
        fo.rotation_euler = (V(target) - fo.location).to_track_quat('-Z', 'Y').to_euler()
    return cam


class N:
    """Knotenbau in einem Material: N(mat).n('ShaderNodeTexNoise', Scale=4) ..."""

    def __init__(self, mat):
        self.t = mat.node_tree
        self.x = 0

    def n(self, kind, **inputs):
        nd = self.t.nodes.new(kind)
        nd.location = (self.x, 0)
        self.x -= 200
        for k, v in inputs.items():
            if k.startswith('_'):
                setattr(nd, k[1:], v)
            else:
                nd.inputs[k.replace('_', ' ')].default_value = v
        return nd

    def link(self, a, b):
        self.t.links.new(a, b)
        return b

    def ramp(self, src, stops, interp='LINEAR'):
        r = self.t.nodes.new('ShaderNodeValToRGB')
        r.color_ramp.interpolation = interp
        el = r.color_ramp.elements
        while len(el) > 1:
            el.remove(el[-1])
        el[0].position, el[0].color = stops[0][0], _rgba(stops[0][1])
        for p, c in stops[1:]:
            e = el.new(p)
            e.color = _rgba(c)
        self.link(src, r.inputs[0])
        return r

    def math(self, op, a, b=None, clamp=False):
        m = self.t.nodes.new('ShaderNodeMath')
        m.operation = op
        m.use_clamp = clamp
        for i, x in enumerate((a, b)):
            if x is None:
                continue
            if isinstance(x, (int, float)):
                m.inputs[i].default_value = x
            else:
                self.link(x, m.inputs[i])
        return m.outputs[0]

    def mix(self, fac, a, b):
        m = self.t.nodes.new('ShaderNodeMix')
        m.data_type = 'RGBA'
        self.link(fac, m.inputs['Factor'])
        for sock, x in ((m.inputs[6], a), (m.inputs[7], b)):
            if isinstance(x, tuple):
                sock.default_value = _rgba(x)
            else:
                self.link(x, sock)
        return m.outputs[2]

    def coord(self, kind='Object', scale=1.0, offset=(0, 0, 0)):
        tc = self.t.nodes.new('ShaderNodeTexCoord')
        mp = self.t.nodes.new('ShaderNodeMapping')
        mp.inputs['Scale'].default_value = (scale, scale, scale) if isinstance(scale, (int, float)) else scale
        mp.inputs['Location'].default_value = offset
        self.link(tc.outputs[kind], mp.inputs['Vector'])
        return mp.outputs[0]


def _rgba(c):
    return (c[0], c[1], c[2], 1.0) if len(c) == 3 else tuple(c)


def material(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    return m, m.node_tree.nodes['Principled BSDF']


def bsdf_set(b, **kw):
    names = {'base': 'Base Color', 'rough': 'Roughness', 'sss': 'Subsurface Weight', 'sss_r': 'Subsurface Radius',
             'sss_s': 'Subsurface Scale', 'spec': 'Specular IOR Level', 'coat': 'Coat Weight', 'coat_r': 'Coat Roughness',
             'trans': 'Transmission Weight', 'ior': 'IOR', 'metal': 'Metallic', 'sheen': 'Sheen Weight', 'sheen_r': 'Sheen Roughness',
             'emit': 'Emission Color', 'emit_s': 'Emission Strength', 'alpha': 'Alpha'}
    for k, v in kw.items():
        b.inputs[names[k]].default_value = _rgba(v) if k in ('base', 'emit') else v


def bump(nb, height, strength, distance, normal=None):
    bp = nb.n('ShaderNodeBump', Strength=strength, Distance=distance)
    nb.link(height, bp.inputs['Height'])
    if normal is not None:
        nb.link(normal, bp.inputs['Normal'])
    return bp.outputs['Normal']


def add_obj(name, bm, mat=None, smooth=True, subsurf=0):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if mat is not None:
        me.materials.append(mat)
    if smooth:
        for p in me.polygons:
            p.use_smooth = True
    if subsurf:
        md = ob.modifiers.new('sub', 'SUBSURF')
        md.levels = subsurf
        md.render_levels = subsurf
    return ob


def grid(nx, ny, sx, sy, fn):
    """Gitter nx x ny ueber [-sx/2, sx/2] x [-sy/2, sy/2]; fn(x, y) -> z oder None (Loch)."""
    bm = bmesh.new()
    vs = {}
    for j in range(ny + 1):
        for i in range(nx + 1):
            x, y = (i / nx - 0.5) * sx, (j / ny - 0.5) * sy
            z = fn(x, y)
            if z is not None:
                vs[i, j] = bm.verts.new((x, y, z))
    for j in range(ny):
        for i in range(nx):
            q = [vs.get(k) for k in ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))]
            if all(q):
                bm.faces.new(q)
    return bm


def blob(center, radii, seed, lump=0.25, res=48, flat_bottom=None):
    """Weiche, unregelmaessige Form (Kugel, verformt mit Rauschen)."""
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=res, v_segments=res // 2, radius=1.0)
    off = V((seed * 1.7, seed * 3.1, seed * 0.7))
    for v in bm.verts:
        d = v.co.normalized()
        f = 1.0 + lump * noise.noise(d * 1.6 + off) + lump * 0.4 * noise.noise(d * 4.0 + off * 2)
        p = V((d.x * radii[0], d.y * radii[1], d.z * radii[2])) * f
        if flat_bottom is not None and p.z < flat_bottom:
            p.z = flat_bottom + (p.z - flat_bottom) * 0.15
        v.co = p + V(center)
    return bm


def tube(points, radius, res=12, rad_fn=None):
    """Schlauch entlang Punkten (fuer Darm, Zaehne, Lippen, Speichel)."""
    bm = bmesh.new()
    rings = []
    n = len(points)
    for i, p in enumerate(points):
        p = V(p)
        t = (V(points[min(i + 1, n - 1)]) - V(points[max(i - 1, 0)])).normalized()
        a = V((0, 0, 1)) if abs(t.z) < 0.9 else V((1, 0, 0))
        u = t.cross(a).normalized()
        w = t.cross(u).normalized()
        r = rad_fn(i / (n - 1)) if rad_fn else radius
        rings.append([bm.verts.new(p + (u * math.cos(k * TAU / res) + w * math.sin(k * TAU / res)) * r) for k in range(res)])
    for i in range(n - 1):
        for k in range(res):
            bm.faces.new([rings[i][k], rings[i][(k + 1) % res], rings[i + 1][(k + 1) % res], rings[i + 1][k]])
    for ring, rev in ((rings[0], True), (rings[-1], False)):
        c = bm.verts.new(sum((v.co for v in ring), V()) / len(ring))
        for k in range(res):
            q = [ring[k], ring[(k + 1) % res], c]
            bm.faces.new(q[::-1] if rev else q)
    return bm


# =====================================================================================================================
# Fleisch
# =====================================================================================================================
def mat_muscle(name, seed, base=((0.20, 0.012, 0.018), (0.42, 0.035, 0.04), (0.62, 0.11, 0.10)), marble=0.6, fiber=1.0,
               fat=(0.86, 0.78, 0.62), membrane=0.0, wet=0.6, glossy=0.28):
    m, b = material(name)
    nb = N(m)
    co = nb.coord('Object', 1.0, (seed * 0.37, seed * 0.11, seed * 0.53))
    n1 = nb.n('ShaderNodeTexNoise', Scale=28.0, Detail=10.0, Roughness=0.62, Distortion=0.6)
    nb.link(co, n1.inputs['Vector'])
    col = nb.ramp(n1.outputs['Fac'], [(0.30, base[0]), (0.52, base[1]), (0.78, base[2])])
    # Fett: duenne, verzweigte Adern (Marmorierung)
    n2 = nb.n('ShaderNodeTexNoise', Scale=20.0, Detail=14.0, Roughness=0.7, Distortion=2.6)
    nb.link(co, n2.inputs['Vector'])
    band = nb.ramp(n2.outputs['Fac'], [(0.475, (0, 0, 0)), (0.495, (1, 1, 1)), (0.505, (1, 1, 1)), (0.525, (0, 0, 0))])
    n3 = nb.n('ShaderNodeTexNoise', Scale=9.0, Detail=3.0)
    nb.link(co, n3.inputs['Vector'])
    fatmask = nb.math('MULTIPLY', band.outputs[0], nb.math('MULTIPLY', n3.outputs['Fac'], marble * 2.2), clamp=True)
    out = nb.mix(fatmask, col.outputs[0], fat)
    hgt = nb.math('MULTIPLY', fatmask, 0.6)
    if membrane > 0:  # Silberhaut zwischen den Muskelstraengen (Voronoi-Kanten)
        vo = nb.n('ShaderNodeTexVoronoi', Scale=26.0, _feature='DISTANCE_TO_EDGE')
        dn = nb.n('ShaderNodeTexNoise', Scale=30.0, Detail=4.0)
        nb.link(co, dn.inputs['Vector'])
        dv = nb.n('ShaderNodeMix', Factor=0.025, _data_type='VECTOR')
        nb.link(co, dv.inputs[4])
        nb.link(dn.outputs['Color'], dv.inputs[5])
        nb.link(dv.outputs[1], vo.inputs['Vector'])
        edge = nb.ramp(vo.outputs['Distance'], [(0.0, (1, 1, 1)), (0.02, (0, 0, 0))])
        mm = nb.math('MULTIPLY', edge.outputs[0], membrane)
        out = nb.mix(mm, out, (0.66, 0.50, 0.50))
        hgt = nb.math('ADD', hgt, nb.math('MULTIPLY', mm, -0.8))
    nb.link(out, b.inputs['Base Color'])
    # Fasern: gestreckte Wellen, quer geschnitten
    wv = nb.n('ShaderNodeTexWave', Scale=170.0, Distortion=7.0, Detail=6.0, Detail_Scale=2.0)
    nb.link(co, wv.inputs['Vector'])
    n4 = nb.n('ShaderNodeTexNoise', Scale=600.0, Detail=4.0)
    nb.link(co, n4.inputs['Vector'])
    h = nb.math('ADD', nb.math('MULTIPLY', wv.outputs['Fac'], 0.35 * fiber), nb.math('MULTIPLY', n4.outputs['Fac'], 0.25))
    h = nb.math('ADD', h, hgt)
    nb.link(bump(nb, h, 0.35, 0.0015), b.inputs['Normal'])
    # nass: Flecken mit sehr glatter Oberflaeche
    n5 = nb.n('ShaderNodeTexNoise', Scale=24.0, Detail=5.0)
    nb.link(co, n5.inputs['Vector'])
    wr = nb.ramp(n5.outputs['Fac'], [(0.45, (glossy + 0.25,) * 3), (0.6, (glossy,) * 3), (0.7, (0.08 + 0.1 * (1 - wet),) * 3)])
    nb.link(wr.outputs[0], b.inputs['Roughness'])
    bsdf_set(b, sss=0.35, sss_r=(1.0, 0.22, 0.12), sss_s=0.006, spec=0.55, coat=0.25 * wet, coat_r=0.08)
    return m


def mat_simple(name, base, rough=0.5, sss=0.0, sss_r=(1, 0.3, 0.2), sss_s=0.005, coat=0.0, bump_scale=0.0, bump_str=0.2,
               spots=None, spot_scale=6.0, trans=0.0, ior=1.45, spec=0.5):
    m, b = material(name)
    nb = N(m)
    co = nb.coord('Object')
    if spots:
        n1 = nb.n('ShaderNodeTexNoise', Scale=spot_scale, Detail=8.0, Distortion=0.4)
        nb.link(co, n1.inputs['Vector'])
        r = nb.ramp(n1.outputs['Fac'], spots)
        nb.link(r.outputs[0], b.inputs['Base Color'])
    else:
        bsdf_set(b, base=base)
    if bump_scale > 0:
        n2 = nb.n('ShaderNodeTexNoise', Scale=bump_scale, Detail=6.0)
        nb.link(co, n2.inputs['Vector'])
        nb.link(bump(nb, n2.outputs['Fac'], bump_str, 0.001), b.inputs['Normal'])
    bsdf_set(b, rough=rough, sss=sss, sss_r=sss_r, sss_s=sss_s, coat=coat, coat_r=0.05, trans=trans, ior=ior, spec=spec)
    return m


def tray(kind, seed):
    """Unterlage im Bild: Styroporschale mit Saugeinlage, Metzgerpapier, Edelstahl oder Brett."""
    rng = random.Random(seed)
    if kind == 'tray':
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = V((v.co.x * 0.42, v.co.y * 0.30, v.co.z * 0.03 - 0.015))
        ob = add_obj('schale', bm, mat_simple('styropor', (0.86, 0.86, 0.84), rough=0.75, sss=0.2, bump_scale=900.0, bump_str=0.15), subsurf=3)
        # Saugeinlage mit Blut
        pad = grid(60, 40, 0.30, 0.20, lambda x, y: 0.0012 + 0.0008 * noise.noise(V((x * 60, y * 60, seed))))
        mt, b = material('einlage')
        nb = N(mt)
        n1 = nb.n('ShaderNodeTexNoise', Scale=22.0, Detail=9.0, Distortion=1.2)
        nb.link(nb.coord('Object', 1.0, (seed, 0, 0)), n1.inputs['Vector'])
        r = nb.ramp(n1.outputs['Fac'], [(0.35, (0.55, 0.02, 0.03)), (0.5, (0.75, 0.12, 0.12)), (0.62, (0.92, 0.9, 0.88))])
        nb.link(r.outputs[0], b.inputs['Base Color'])
        bsdf_set(b, rough=0.55, sss=0.1)
        add_obj('einlage', pad, mt)
        return 0.0028
    if kind == 'paper':
        g = grid(120, 90, 0.9, 0.7, lambda x, y: 0.004 * noise.noise(V((x * 9, y * 9, seed))) + 0.0015 * noise.noise(V((x * 40, y * 40, seed))))
        mt, b = material('papier')
        nb = N(mt)
        n1 = nb.n('ShaderNodeTexNoise', Scale=12.0, Detail=9.0, Distortion=1.6)
        nb.link(nb.coord('Object', 1.0, (seed, 2, 0)), n1.inputs['Vector'])
        r = nb.ramp(n1.outputs['Fac'], [(0.3, (0.38, 0.02, 0.03)), (0.42, (0.62, 0.15, 0.13)), (0.5, (0.86, 0.80, 0.70))])
        nb.link(r.outputs[0], b.inputs['Base Color'])
        rr = nb.ramp(n1.outputs['Fac'], [(0.3, (0.12,) * 3), (0.45, (0.65,) * 3)])
        nb.link(rr.outputs[0], b.inputs['Roughness'])
        add_obj('papier', g, mt)
        return 0.002
    if kind == 'steel':
        g = grid(4, 4, 1.0, 0.8, lambda x, y: 0.0)
        mt, b = material('stahl')
        nb = N(mt)
        n1 = nb.n('ShaderNodeTexNoise', Scale=3.0, Detail=6.0)
        nb.link(nb.coord('Object', (1.0, 30.0, 1.0)), n1.inputs['Vector'])
        rr = nb.ramp(n1.outputs['Fac'], [(0.3, (0.18,) * 3), (0.7, (0.38,) * 3)])
        nb.link(rr.outputs[0], b.inputs['Roughness'])
        bsdf_set(b, base=(0.62, 0.62, 0.6), metal=1.0)
        add_obj('stahl', g, mt)
        # Blutlache
        pts = []
        for k in range(40):
            a = k * TAU / 40
            r = 0.16 * (1 + 0.3 * noise.noise(V((math.cos(a), math.sin(a), seed))) + 0.12 * math.sin(5 * a + rng.random()))
            pts.append((r * math.cos(a) * 1.3 + 0.03, r * math.sin(a)))
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((x, y, 0.0008)) for (x, y) in pts])
        bmesh.ops.triangulate(bm, faces=[f])
        add_obj('lache', bm, mat_simple('blut', (0.07, 0.002, 0.004), rough=0.04, coat=1.0, spec=0.8), smooth=False)
        return 0.0
    # Brett
    g = grid(4, 4, 1.0, 0.8, lambda x, y: 0.0)
    mt, b = material('brett')
    nb = N(mt)
    wv = nb.n('ShaderNodeTexWave', Scale=6.0, Distortion=8.0, Detail=4.0, _wave_profile='SIN')
    nb.link(nb.coord('Object', (1.0, 0.2, 1.0)), wv.inputs['Vector'])
    r = nb.ramp(wv.outputs['Fac'], [(0.0, (0.30, 0.18, 0.10)), (1.0, (0.55, 0.38, 0.24))])
    n1 = nb.n('ShaderNodeTexNoise', Scale=8.0, Detail=8.0, Distortion=1.4)
    nb.link(nb.coord('Object', 1.0, (seed, 0, 0)), n1.inputs['Vector'])
    st = nb.ramp(n1.outputs['Fac'], [(0.4, (1, 1, 1)), (0.55, (0, 0, 0))])
    nb.link(nb.mix(st.outputs[0], r.outputs[0], (0.28, 0.03, 0.03)), b.inputs['Base Color'])
    bsdf_set(b, rough=0.6)
    add_obj('brett', g, mt)
    return 0.0


def scene_meat(kind, seed):
    rng = random.Random(seed)
    if kind == 'meat_steak':
        z0 = tray('tray', seed)
        ob = add_obj('steak', blob((0, 0, z0 + 0.016), (0.15, 0.105, 0.018), seed, 0.18, 64, flat_bottom=z0), mat_muscle('rind', seed), subsurf=2)
        # Fettrand
        pts = [V((0.15 * math.cos(a) * 0.98, 0.105 * math.sin(a) * 1.02, z0 + 0.016)) for a in [0.5 + k * 0.12 for k in range(18)]]
        add_obj('fettrand', tube(pts, 0.014, 14, lambda t: 0.013 * (0.6 + 0.4 * math.sin(math.pi * t))),
                mat_simple('fett', (0.88, 0.82, 0.66), rough=0.45, sss=0.4, sss_r=(1, 0.8, 0.5), bump_scale=200.0, bump_str=0.25), subsurf=1)
        return ((0.0, -0.07, 0.36), (0, 0.0, 0.0), 55, 4.0)
    if kind == 'meat_mince':
        z0 = tray('paper', seed)
        m, b = material('hack')
        nb = N(m)
        co = nb.coord('Object', 1.0, (seed, 0, 0))
        wv = nb.n('ShaderNodeTexWave', Scale=240.0, Distortion=26.0, Detail=8.0, _wave_type='RINGS')
        nb.link(co, wv.inputs['Vector'])
        worm = nb.ramp(wv.outputs['Fac'], [(0.25, (0, 0, 0)), (0.55, (1, 1, 1))])
        n1 = nb.n('ShaderNodeTexNoise', Scale=120.0, Detail=6.0)
        nb.link(co, n1.inputs['Vector'])
        c = nb.ramp(n1.outputs['Fac'], [(0.35, (0.30, 0.02, 0.03)), (0.65, (0.55, 0.10, 0.10))])
        vo = nb.n('ShaderNodeTexVoronoi', Scale=700.0)
        nb.link(co, vo.inputs['Vector'])
        fs = nb.ramp(vo.outputs['Distance'], [(0.0, (1, 1, 1)), (0.12, (0, 0, 0))])
        col = nb.mix(worm.outputs[0], (0.16, 0.01, 0.02), c.outputs[0])
        col = nb.mix(nb.math('MULTIPLY', fs.outputs[0], 0.8), col, (0.9, 0.82, 0.74))
        nb.link(col, b.inputs['Base Color'])
        nb.link(bump(nb, worm.outputs[0], 0.9, 0.002), b.inputs['Normal'])
        bsdf_set(b, rough=0.32, sss=0.3, sss_r=(1, 0.25, 0.15), sss_s=0.004, coat=0.2, coat_r=0.1)
        bm = blob((0, 0, z0), (0.12, 0.10, 0.05), seed, 0.3, 64, flat_bottom=z0)
        add_obj('hack', bm, m, subsurf=2)
        return ((0.0, -0.17, 0.27), (0, 0.0, 0.02), 50, 4.0)
    if kind == 'meat_belly':
        tray('steel', seed)
        m, b = material('bauch')
        nb = N(m)
        co = nb.coord('Object')
        wv = nb.n('ShaderNodeTexWave', Scale=34.0, Distortion=5.0, Detail=5.0, _bands_direction='Z')
        nb.link(nb.coord('Object', 1.0, (seed, 0, 0)), wv.inputs['Vector'])
        lay = nb.ramp(wv.outputs['Fac'], [(0.32, (0.42, 0.07, 0.07)), (0.45, (0.66, 0.30, 0.27)), (0.56, (0.86, 0.80, 0.68))])
        sep = nb.n('ShaderNodeSeparateXYZ')
        nb.link(co, sep.inputs[0])
        top = nb.math('GREATER_THAN', sep.outputs['Z'], 0.043)
        skin = nb.n('ShaderNodeTexNoise', Scale=300.0, Detail=6.0)
        nb.link(co, skin.inputs['Vector'])
        sk = nb.ramp(skin.outputs['Fac'], [(0.3, (0.80, 0.60, 0.50)), (0.7, (0.92, 0.76, 0.64))])
        nb.link(nb.mix(top, lay.outputs[0], sk.outputs[0]), b.inputs['Base Color'])
        vo = nb.n('ShaderNodeTexVoronoi', Scale=1500.0)
        nb.link(co, vo.inputs['Vector'])
        nb.link(bump(nb, nb.math('ADD', vo.outputs['Distance'], nb.math('MULTIPLY', wv.outputs['Fac'], 0.4)), 0.3, 0.001), b.inputs['Normal'])
        bsdf_set(b, rough=0.4, sss=0.4, sss_r=(1, 0.5, 0.35), sss_s=0.005, coat=0.15)
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = V((v.co.x * 0.26, v.co.y * 0.13, (v.co.z + 0.5) * 0.065))
        ob = add_obj('bauch', bm, m, subsurf=0)
        bv = ob.modifiers.new('fase', 'BEVEL')
        bv.width = 0.006
        bv.segments = 3
        sd = ob.modifiers.new('sub', 'SUBSURF')
        sd.levels = sd.render_levels = 4
        sd.subdivision_type = 'SIMPLE'
        ob.rotation_euler = (0, 0, 0.12)
        d = ob.modifiers.new('d', 'DISPLACE')
        tx = bpy.data.textures.new('wolke', 'CLOUDS')
        tx.noise_scale = 0.02
        d.texture = tx
        d.strength = 0.003
        return ((0.05, -0.26, 0.075), (0.0, 0.0, 0.03), 50, 4.5)
    if kind == 'meat_liver':
        tray('board', seed)
        m = mat_simple('leber', None, rough=0.12, sss=0.25, sss_r=(1, 0.2, 0.15), sss_s=0.004, coat=0.6, bump_scale=150.0, bump_str=0.12,
                       spots=[(0.3, (0.07, 0.008, 0.012)), (0.55, (0.14, 0.02, 0.025)), (0.8, (0.22, 0.05, 0.05))], spot_scale=25.0)
        for k, (c, r) in enumerate((((-0.04, 0.0, 0.0), (0.13, 0.08, 0.028)), ((0.08, 0.03, 0.0), (0.09, 0.06, 0.024)),
                                     ((0.02, -0.07, 0.0), (0.08, 0.05, 0.02)))):
            add_obj(f'lappen{k}', blob(V(c) + V((0, 0, r[2] * 0.6)), r, seed + k, 0.12, 48, flat_bottom=0.0), m, subsurf=2)
        # Blut darunter
        pts = [(0.2 * math.cos(a) * (1 + 0.25 * noise.noise(V((math.cos(a), math.sin(a), seed)))), 0.14 * math.sin(a) * (1 + 0.25 * noise.noise(V((math.sin(a), 2, seed))))) for a in [k * TAU / 40 for k in range(40)]]
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((x, y, 0.0008)) for (x, y) in pts])
        bmesh.ops.triangulate(bm, faces=[f])
        add_obj('lache', bm, mat_simple('blut', (0.08, 0.002, 0.004), rough=0.03, coat=1.0, spec=0.8), smooth=False)
        return ((0.03, -0.16, 0.26), (0.02, 0.0, 0.02), 50, 5.0)
    if kind == 'meat_osso':
        tray('paper', seed)
        h = 0.035
        bm = blob((0, 0, h * 0.5), (0.11, 0.095, h * 0.5), seed, 0.12, 64, flat_bottom=0.0)
        for v in bm.verts:  # flache Schnittflaeche oben
            if v.co.z > h * 0.8:
                v.co.z = h * 0.8 + (v.co.z - h * 0.8) * 0.1
        add_obj('scheibe', bm, mat_muscle('beinscheibe', seed, membrane=0.9, marble=0.3), subsurf=2)
        ring = blob((0.01, 0.005, h * 0.8), (0.032, 0.026, 0.004), seed + 3, 0.05, 48)
        add_obj('knochen', ring, mat_simple('knochen', (0.86, 0.80, 0.66), rough=0.5, sss=0.3, bump_scale=300.0, bump_str=0.3), subsurf=1)
        add_obj('mark', blob((0.01, 0.005, h * 0.8 + 0.0025), (0.022, 0.017, 0.003), seed + 4, 0.06, 48),
                mat_simple('mark', None, rough=0.2, sss=0.5, coat=0.4, bump_scale=500.0, bump_str=0.5,
                           spots=[(0.3, (0.55, 0.20, 0.12)), (0.6, (0.80, 0.55, 0.32))], spot_scale=80.0), subsurf=1)
        return ((0.0, -0.1, 0.25), (0.0, 0.0, 0.02), 50, 4.0)
    if kind == 'meat_guts':
        tray('steel', seed)
        m, b = material('darm')
        nb = N(m)
        co = nb.coord('Object', 1.0, (seed, 0, 0))
        n1 = nb.n('ShaderNodeTexNoise', Scale=90.0, Detail=12.0, Distortion=3.5)
        nb.link(co, n1.inputs['Vector'])
        vein = nb.ramp(n1.outputs['Fac'], [(0.46, (0, 0, 0)), (0.5, (1, 1, 1)), (0.54, (0, 0, 0))])
        n2 = nb.n('ShaderNodeTexNoise', Scale=20.0, Detail=4.0)
        nb.link(co, n2.inputs['Vector'])
        base = nb.ramp(n2.outputs['Fac'], [(0.3, (0.42, 0.22, 0.24)), (0.7, (0.58, 0.36, 0.36))])
        nb.link(nb.mix(vein.outputs[0], base.outputs[0], (0.45, 0.05, 0.08)), b.inputs['Base Color'])
        nb.link(bump(nb, n2.outputs['Fac'], 0.2, 0.002), b.inputs['Normal'])
        bsdf_set(b, rough=0.18, sss=0.6, sss_r=(1, 0.4, 0.3), sss_s=0.006, coat=0.6, coat_r=0.05)
        for k in range(9):
            p = V((rng.uniform(-0.12, 0.12), rng.uniform(-0.09, 0.09), 0.02))
            a = rng.uniform(0, TAU)
            pts = []
            for i in range(26):
                a += rng.uniform(-0.6, 0.6)
                p = p + V((math.cos(a), math.sin(a), 0)) * 0.018
                p.x = max(-0.16, min(0.16, p.x))
                p.y = max(-0.12, min(0.12, p.y))
                pts.append(V((p.x, p.y, 0.016 + 0.012 * k / 9 + 0.006 * math.sin(i * 0.7))))
            add_obj(f'darm{k}', tube(pts, 0.016, 16, lambda t: 0.015 * (1 + 0.15 * math.sin(t * 40))), m)
        return ((0.0, -0.17, 0.26), (0.0, 0.0, 0.02), 45, 4.0)
    if kind == 'meat_tongue':
        tray('board', seed)
        bm = blob((0, 0, 0.026), (0.17, 0.055, 0.03), seed, 0.08, 64, flat_bottom=0.0)
        for v in bm.verts:  # Mittelfurche, spitzes Ende
            if v.co.z > 0.03:
                v.co.z -= 0.006 * math.exp(-(v.co.y / 0.01) ** 2)
            if v.co.x > 0.08:
                s = 1.0 - (v.co.x - 0.08) * 3.0
                v.co.y *= max(0.35, s)
        m, b = material('zunge')
        nb = N(m)
        co = nb.coord('Object', 1.0, (seed, 0, 0))
        vo = nb.n('ShaderNodeTexVoronoi', Scale=900.0)
        nb.link(co, vo.inputs['Vector'])
        n1 = nb.n('ShaderNodeTexNoise', Scale=40.0, Detail=8.0, Distortion=0.8)
        nb.link(co, n1.inputs['Vector'])
        c = nb.ramp(n1.outputs['Fac'], [(0.35, (0.12, 0.07, 0.08)), (0.5, (0.36, 0.22, 0.24)), (0.65, (0.50, 0.34, 0.34))])
        nb.link(c.outputs[0], b.inputs['Base Color'])
        nb.link(bump(nb, nb.math('SUBTRACT', 1.0, vo.outputs['Distance']), 0.8, 0.0015), b.inputs['Normal'])
        bsdf_set(b, rough=0.38, sss=0.4, sss_r=(1, 0.4, 0.3), sss_s=0.005, coat=0.2)
        add_obj('zunge', bm, m, subsurf=2)
        return ((0.02, -0.17, 0.2), (0.02, 0.0, 0.02), 45, 4.0)
    # meat_skin: Haut mit Gaensehaut und Federporen, bildfuellend
    g = grid(240, 240, 0.12, 0.12, lambda x, y: 0.004 * noise.noise(V((x * 25, y * 25, seed))))
    m, b = material('haut')
    nb = N(m)
    co = nb.coord('Object', 1.0, (seed, 0, 0))
    vo = nb.n('ShaderNodeTexVoronoi', Scale=500.0, Randomness=0.8)
    nb.link(co, vo.inputs['Vector'])
    pore = nb.ramp(vo.outputs['Distance'], [(0.0, (1, 1, 1)), (0.18, (0, 0, 0))])
    n1 = nb.n('ShaderNodeTexNoise', Scale=60.0, Detail=8.0)
    nb.link(co, n1.inputs['Vector'])
    c = nb.ramp(n1.outputs['Fac'], [(0.3, (0.55, 0.40, 0.30)), (0.55, (0.66, 0.50, 0.38)), (0.75, (0.52, 0.28, 0.25))])
    nb.link(nb.mix(nb.math('MULTIPLY', pore.outputs[0], 0.7), c.outputs[0], (0.42, 0.22, 0.2)), b.inputs['Base Color'])
    nb.link(bump(nb, nb.math('SUBTRACT', 1.0, pore.outputs[0]), 0.7, 0.0012), b.inputs['Normal'])
    bsdf_set(b, rough=0.35, sss=0.6, sss_r=(1, 0.55, 0.4), sss_s=0.004, coat=0.3)
    add_obj('haut', g, m)
    return ((0.0, -0.05, 0.1), (0.0, 0.0, 0.0), 60, 6.0)


# =====================================================================================================================
# Auge
# =====================================================================================================================
def scene_eye(seed, iris_cols, look):
    rng = random.Random(seed)
    A, UP, LO = 0.0135, 0.0058, 0.0042  # halbe Lidspaltenbreite, Oberlid, Unterlid (m)

    def opening(x):
        t = max(0.0, 1.0 - (x / A) ** 2)
        s = math.sqrt(t)
        return UP * s * (1.0 - 0.18 * x / A), -LO * s * (1.0 + 0.1 * x / A)

    margin = {}

    def surf(x, y):
        up, lo = opening(x)
        inside = abs(x) < A and lo < y < up
        # Abstand zur Lidkante (positiv draussen)
        if abs(x) < A:
            d = (y - up) if y >= 0 else (lo - y)
        else:
            d = math.hypot(abs(x) - A, y)
        if inside:
            dd = -d  # wie tief innen
            z = 0.0024 - min(dd, 0.003) * 2.2 - max(0.0, dd - 0.003) * 0.4
            return z
        z = 0.0026 * math.exp(-d / 0.0016) + 0.0005  # Lidrand wulstet
        if y > 0:
            z += -0.0011 * math.exp(-((d - 0.0062) / 0.0022) ** 2)  # Lidfalte
            z += 0.004 * max(0.0, (y - 0.011) / 0.008) ** 1.5  # Brauenbogen
        else:
            z += 0.0008 * math.exp(-((d - 0.004) / 0.002) ** 2)  # Traenensack
        z += 0.00025 * noise.noise(V((x * 400, y * 400, seed)))
        return z

    sx, sy = 0.07, 0.044
    nx, ny = 420, 264
    bm = bmesh.new()
    lay = bm.verts.layers.float.new('rand')
    vs = {}
    for j in range(ny + 1):
        for i in range(nx + 1):
            x, y = (i / nx - 0.5) * sx, (j / ny - 0.5) * sy
            v = bm.verts.new((x, y, surf(x, y)))
            up, lo = opening(x)
            if abs(x) < A:
                d = (y - up) if y >= 0 else (lo - y)
            else:
                d = math.hypot(abs(x) - A, y)
            v[lay] = max(0.0, min(1.0, 1.0 - (d + 0.0004) / 0.0011))
            vs[i, j] = v
    for j in range(ny):
        for i in range(nx):
            bm.faces.new([vs[i, j], vs[i + 1, j], vs[i + 1, j + 1], vs[i, j + 1]])
    # Haut
    m, b = material('haut')
    nb = N(m)
    co = nb.coord('Object', 1.0, (seed, 0, 0))
    at = nb.n('ShaderNodeAttribute', _attribute_name='rand')
    n1 = nb.n('ShaderNodeTexNoise', Scale=90.0, Detail=8.0)
    nb.link(co, n1.inputs['Vector'])
    skin = nb.ramp(n1.outputs['Fac'], [(0.3, (0.60, 0.40, 0.33)), (0.5, (0.70, 0.50, 0.41)), (0.72, (0.62, 0.36, 0.32))])
    n2 = nb.n('ShaderNodeTexNoise', Scale=18.0, Detail=4.0)
    nb.link(co, n2.inputs['Vector'])
    dark = nb.ramp(n2.outputs['Fac'], [(0.35, (0, 0, 0)), (0.7, (1, 1, 1))])
    skin2 = nb.mix(nb.math('MULTIPLY', dark.outputs[0], 0.35), skin.outputs[0], (0.38, 0.22, 0.24))  # Augenringe
    col = nb.mix(nb.ramp(at.outputs['Fac'], [(0.45, (0, 0, 0)), (1.0, (1, 1, 1))]).outputs[0], skin2, (0.62, 0.34, 0.33))
    nb.link(col, b.inputs['Base Color'])
    vo = nb.n('ShaderNodeTexVoronoi', Scale=1500.0)
    nb.link(co, vo.inputs['Vector'])
    wv = nb.n('ShaderNodeTexWave', Scale=300.0, Distortion=4.0, Detail=3.0, _bands_direction='Y')
    nb.link(co, wv.inputs['Vector'])
    hh = nb.math('ADD', vo.outputs['Distance'], nb.math('MULTIPLY', wv.outputs['Fac'], 0.15))
    nb.link(bump(nb, hh, 0.25, 0.0002), b.inputs['Normal'])
    rr = nb.ramp(at.outputs['Fac'], [(0.0, (0.48,) * 3), (0.85, (0.12,) * 3)])
    nb.link(rr.outputs[0], b.inputs['Roughness'])
    bsdf_set(b, sss=0.5, sss_r=(1.0, 0.35, 0.2), sss_s=0.0025, spec=0.5)
    add_obj('haut', bm, m)
    # Augapfel
    R = 0.0122
    eb = bmesh.new()
    bmesh.ops.create_uvsphere(eb, u_segments=96, v_segments=64, radius=R)
    eye = add_obj('augapfel', eb, None)
    eye.location = (0, 0, 0.0015 - R)
    eye.rotation_euler = (math.radians(-look[1]), math.radians(look[0]), 0)
    m, b = material('auge')
    nb = N(m)
    tc = nb.n('ShaderNodeTexCoord')
    sep = nb.n('ShaderNodeSeparateXYZ')
    nb.link(tc.outputs['Object'], sep.inputs[0])
    rxy = nb.math('DIVIDE', nb.math('SQRT', nb.math('ADD', nb.math('MULTIPLY', sep.outputs['X'], sep.outputs['X']),
                                                     nb.math('MULTIPLY', sep.outputs['Y'], sep.outputs['Y']))), R)
    front = nb.math('GREATER_THAN', sep.outputs['Z'], 0.0)
    ang = nb.math('ARCTAN2', sep.outputs['Y'], sep.outputs['X'])
    cx = nb.n('ShaderNodeCombineXYZ')
    nb.link(nb.math('MULTIPLY', ang, 9.0), cx.inputs['X'])
    nb.link(nb.math('MULTIPLY', rxy, 1.5), cx.inputs['Y'])
    fib = nb.n('ShaderNodeTexNoise', Scale=3.0, Detail=10.0, Roughness=0.7)
    nb.link(cx.outputs[0], fib.inputs['Vector'])
    fib2 = nb.n('ShaderNodeTexNoise', Scale=1.0, Detail=4.0)
    cx2 = nb.n('ShaderNodeCombineXYZ')
    nb.link(nb.math('MULTIPLY', ang, 4.0), cx2.inputs['X'])
    nb.link(nb.math('MULTIPLY', rxy, 30.0), cx2.inputs['Y'])
    nb.link(cx2.outputs[0], fib2.inputs['Vector'])
    iris = nb.ramp(nb.math('ADD', nb.math('MULTIPLY', fib.outputs['Fac'], 0.7), nb.math('MULTIPLY', fib2.outputs['Fac'], 0.3)),
                   [(0.3, iris_cols[0]), (0.5, iris_cols[1]), (0.72, iris_cols[2])])
    ring = nb.ramp(rxy, [(0.0, (0, 0, 0)), (0.36, (0, 0, 0)), (0.46, (1, 1, 1))])  # dunkler Rand der Iris
    coll = nb.ramp(rxy, [(0.13, (0, 0, 0)), (0.17, (1, 1, 1)), (0.24, (0, 0, 0))])  # heller Kranz
    ic = nb.mix(nb.math('MULTIPLY', ring.outputs[0], 0.75), iris.outputs[0], (0.04, 0.035, 0.03))
    ic = nb.mix(nb.math('MULTIPLY', coll.outputs[0], 0.45), ic, iris_cols[3])
    # Lederhaut mit Aederchen, zum Rand hin mehr
    vt = nb.n('ShaderNodeTexNoise', Scale=0.0, Detail=14.0, Roughness=0.7, Distortion=4.0)
    nb.link(nb.coord('Object', 160.0, (seed, 0, 0)), vt.inputs['Vector'])
    vein = nb.ramp(vt.outputs['Fac'], [(0.47, (0, 0, 0)), (0.5, (1, 1, 1)), (0.53, (0, 0, 0))])
    vfac = nb.math('MULTIPLY', vein.outputs[0], nb.ramp(rxy, [(0.5, (0.1,) * 3), (0.95, (1, 1, 1))]).outputs[0])
    scl = nb.mix(vfac, (0.86, 0.80, 0.74), (0.55, 0.06, 0.06))
    scl = nb.mix(nb.ramp(rxy, [(0.55, (0, 0, 0)), (1.0, (0.45, 0.45, 0.45))]).outputs[0], scl, (0.75, 0.50, 0.42))
    irisf = nb.math('MULTIPLY', nb.ramp(rxy, [(0.46, (1, 1, 1)), (0.49, (0, 0, 0))]).outputs[0], front)
    col = nb.mix(irisf, scl, ic)
    pup = nb.math('MULTIPLY', nb.ramp(rxy, [(0.15, (1, 1, 1)), (0.165, (0, 0, 0))]).outputs[0], front)
    col = nb.mix(pup, col, (0.008, 0.006, 0.006))
    nb.link(col, b.inputs['Base Color'])
    bsdf_set(b, rough=0.32, sss=0.15, sss_r=(1, 0.5, 0.4), sss_s=0.002, coat=1.0, coat_r=0.02, spec=0.6)
    eye.data.materials.append(m)
    for p in eye.data.polygons:
        p.use_smooth = True
    # Hornhaut: Wulst ueber der Iris (spiegelt den Blitz)
    cb = bmesh.new()
    bmesh.ops.create_uvsphere(cb, u_segments=64, v_segments=32, radius=0.0078)
    for v in list(cb.verts):
        if v.co.z < 0.0042:
            cb.verts.remove(v)
    corn = add_obj('hornhaut', cb, mat_simple('hornhaut', (1, 1, 1), rough=0.0, trans=1.0, ior=1.376, spec=0.6))
    corn.parent = eye
    corn.location = (0, 0, R - 0.0078 - 0.0005 + 0.0021)
    # Wimpern
    lash, lb = material('wimper')
    bsdf_set(lb, base=(0.02, 0.016, 0.014), rough=0.35)
    for k in range(120):
        x = (rng.random() * 2 - 1) * A * 0.96
        upper = k < 85
        up, lo = opening(x)
        y = up if upper else lo
        z = 0.0033
        ln = (0.0085 if upper else 0.004) * (0.6 + 0.4 * math.sqrt(max(0.0, 1 - (x / A) ** 2))) * rng.uniform(0.7, 1.1)
        dy = 1.0 if upper else -1.0
        pts = []
        for i in range(6):
            t = i / 5
            pts.append(V((x + x / A * 0.003 * t, y + dy * ln * (0.25 * t + 0.75 * t * t) * 0.6, z + ln * (math.sin(t * 1.2)) * 0.9)))
        add_obj('wimper', tube(pts, 0.0, 5, lambda t: 0.00008 * (1 - 0.8 * t)), lash)
    return ((0.0, -0.004, 0.11), (0.0, 0.001, 0.0), 100, 4.0)


# =====================================================================================================================
# Maul eines Pitbulls, weit aufgerissen
# =====================================================================================================================
def mat_gum(name, seed, pig=0.45):
    m, b = material(name)
    nb = N(m)
    co = nb.coord('Object', 1.0, (seed, 0, 0))
    n1 = nb.n('ShaderNodeTexNoise', Scale=80.0, Detail=8.0, Distortion=1.2)
    nb.link(co, n1.inputs['Vector'])
    pg = nb.ramp(n1.outputs['Fac'], [(0.62 - pig * 0.2, (0, 0, 0)), (0.66 - pig * 0.2, (1, 1, 1))])
    n2 = nb.n('ShaderNodeTexNoise', Scale=60.0, Detail=6.0)
    nb.link(co, n2.inputs['Vector'])
    c = nb.ramp(n2.outputs['Fac'], [(0.3, (0.62, 0.22, 0.26)), (0.7, (0.82, 0.42, 0.45))])
    nb.link(nb.mix(pg.outputs[0], c.outputs[0], (0.07, 0.05, 0.06)), b.inputs['Base Color'])
    nb.link(bump(nb, n2.outputs['Fac'], 0.25, 0.001), b.inputs['Normal'])
    bsdf_set(b, rough=0.2, sss=0.45, sss_r=(1, 0.35, 0.3), sss_s=0.004, coat=0.7, coat_r=0.04)
    return m


def tooth(bm_parent_mat, base, direction, length, width, curve, kind, rng):
    """Zahn als gebogener Kegel: base = Wurzel am Zahnfleisch, direction = Richtung der Spitze."""
    d = V(direction).normalized()
    side = d.cross(V((0, 0, 1)))
    if side.length < 1e-3:
        side = V((1, 0, 0))
    side.normalize()
    pts = []
    for i in range(9):
        t = i / 8
        pts.append(V(base) + d * length * t + V((0, 0, 1)) * curve * length * t * t)
    if kind == 'canine':
        rf = lambda t: width * (1.0 - t) ** 0.8 * (1.0 + 0.15 * (1 - t))
    elif kind == 'incisor':
        rf = lambda t: width * (0.75 + 0.25 * (1 - t)) * (1.0 if t < 0.8 else (1 - t) * 5)
    else:
        rf = lambda t: width * (1.0 - t) ** 1.2
    return tube(pts, width, 12, rf)


def scene_mouth(seed):
    rng = random.Random(seed)
    # Koordinaten: Kamera auf +y schaut nach -y; x links/rechts, z oben/unten; das Maul oeffnet sich zur Kamera
    gum_u = mat_gum('zahnfleisch_oben', seed)
    gum_l = mat_gum('zahnfleisch_unten', seed + 1, 0.6)
    enamel, eb = material('zahn')
    nb = N(enamel)
    sep = nb.n('ShaderNodeSeparateXYZ')
    tc = nb.n('ShaderNodeTexCoord')
    nb.link(tc.outputs['Object'], sep.inputs[0])
    n1 = nb.n('ShaderNodeTexNoise', Scale=120.0, Detail=6.0)
    nb.link(tc.outputs['Object'], n1.inputs['Vector'])
    ec = nb.ramp(n1.outputs['Fac'], [(0.3, (0.66, 0.56, 0.36)), (0.7, (0.84, 0.78, 0.60))])
    zz = nb.math('ABSOLUTE', sep.outputs['Z'])
    tart = nb.ramp(zz, [(0.028, (0.62, 0.48, 0.25)), (0.036, (0, 0, 0))])  # Zahnstein am Zahnfleisch
    nb.link(nb.mix(nb.math('MULTIPLY', tart.outputs[0], 1.4), ec.outputs[0], (0.55, 0.40, 0.18)), eb.inputs['Base Color'])
    bsdf_set(eb, rough=0.22, sss=0.2, sss_r=(1, 0.9, 0.7), sss_s=0.002, coat=0.6, coat_r=0.05)
    lip, lb = material('lefze')
    nb = N(lip)
    n1 = nb.n('ShaderNodeTexNoise', Scale=400.0, Detail=6.0)
    nb.link(nb.coord('Object'), n1.inputs['Vector'])
    lc = nb.ramp(n1.outputs['Fac'], [(0.4, (0.012, 0.01, 0.01)), (0.7, (0.05, 0.035, 0.035))])
    nb.link(lc.outputs[0], lb.inputs['Base Color'])
    nb.link(bump(nb, n1.outputs['Fac'], 0.5, 0.001), lb.inputs['Normal'])
    bsdf_set(lb, rough=0.5, coat=0.12, coat_r=0.15, sss=0.05, spec=0.25)
    # Rachen und Gaumen
    throat = blob((0, -0.11, 0.0), (0.075, 0.06, 0.07), seed, 0.1, 48)
    add_obj('rachen', throat, mat_simple('rachen', (0.10, 0.012, 0.016), rough=0.15, coat=0.8, sss=0.3))
    pal = grid(80, 80, 0.10, 0.11, lambda x, y: 0.0)
    for v in pal.verts:  # Gaumendach: gewoelbt, mit Querrippen
        x, y = v.co.x, v.co.y
        v.co = V((x, y - 0.055, 0.032 + 0.012 * (1 - (x / 0.05) ** 2) + 0.0018 * max(0.0, math.sin(y * 260)) * (1 - abs(x) / 0.05)))
    add_obj('gaumen', pal, mat_gum('gaumen', seed + 2, 0.55))
    # Zahnboegen (U-Form in x/y, vorn bei y = 0)
    def arch(t, half_w, depth):  # t in [-1, 1]
        return V((half_w * math.sin(t * math.pi * 0.5), -depth * (1 - math.cos(t * math.pi * 0.5)), 0.0))
    teeth_info = []
    for upper in (True, False):
        zc = 0.036 if upper else -0.044
        hw, dp = (0.050, 0.07) if upper else (0.044, 0.065)
        pts = [arch(-1 + 2 * k / 30, hw, dp) + V((0, 0, zc)) for k in range(31)]
        add_obj('kiefer_o' if upper else 'kiefer_u', tube(pts, 0.011, 18), gum_u if upper else gum_l, subsurf=1)
        lpts = [arch(-1.05 + 2.1 * k / 34, hw * 1.25, dp * 1.05) + V((0, 0.006, zc + (0.009 if upper else -0.01))) for k in range(35)]
        add_obj('lippe', tube(lpts, 0.006, 14, lambda t: 0.006 * (1 + (0.35 * max(0, math.sin(t * 90)) if not upper else 0.1 * math.sin(t * 37)))), lip, subsurf=1)
        s = -1.0 if upper else 1.0
        # Schneidezaehne vorn
        for k in range(6):
            t = (k - 2.5) / 2.5 * 0.22
            p = arch(t, hw, dp) + V((0, 0.004, zc + s * 0.006))
            teeth_info.append((p, V((0, 0.12, s)).normalized(), 0.013, 0.0042, 'incisor'))
        for side in (-1, 1):  # Eckzaehne, Backenzaehne
            p = arch(side * 0.42, hw, dp) + V((0, 0.002, zc + s * 0.006))
            teeth_info.append((p, V((side * 0.15, 0.25, s)).normalized(), 0.032 if upper else 0.026, 0.0068, 'canine'))
            for j in range(4):
                p = arch(side * (0.56 + j * 0.12), hw, dp) + V((0, 0, zc + s * 0.005))
                teeth_info.append((p, V((0, 0.05, s)).normalized(), 0.017 - j * 0.002, 0.0055, 'molar'))
    for (p, d, ln, w, kind) in teeth_info:
        bmt = tooth(enamel, p, d, ln * rng.uniform(0.9, 1.08), w, -0.05 if d.z < 0 else 0.05, kind, rng)
        add_obj('zahn', bmt, enamel, subsurf=1)
    # Zunge
    tg = blob((0, -0.045, -0.04), (0.036, 0.07, 0.016), seed + 5, 0.05, 64)
    for v in tg.verts:
        if v.co.z > -0.04:
            v.co.z -= 0.004 * math.exp(-(v.co.x / 0.006) ** 2)
    m, b = material('zunge')
    nb = N(m)
    co = nb.coord('Object', 1.0, (seed, 0, 0))
    vo = nb.n('ShaderNodeTexVoronoi', Scale=700.0)
    nb.link(co, vo.inputs['Vector'])
    n1 = nb.n('ShaderNodeTexNoise', Scale=30.0, Detail=6.0)
    nb.link(co, n1.inputs['Vector'])
    c = nb.ramp(n1.outputs['Fac'], [(0.3, (0.72, 0.30, 0.36)), (0.7, (0.86, 0.46, 0.50))])
    pig = nb.n('ShaderNodeTexNoise', Scale=14.0, Detail=5.0)
    nb.link(co, pig.inputs['Vector'])
    pm = nb.ramp(pig.outputs['Fac'], [(0.66, (0, 0, 0)), (0.7, (1, 1, 1))])
    nb.link(nb.mix(pm.outputs[0], c.outputs[0], (0.12, 0.06, 0.08)), b.inputs['Base Color'])
    nb.link(bump(nb, nb.math('SUBTRACT', 1.0, vo.outputs['Distance']), 0.4, 0.0008), b.inputs['Normal'])
    bsdf_set(b, rough=0.18, sss=0.5, sss_r=(1, 0.35, 0.3), sss_s=0.005, coat=0.8, coat_r=0.04)
    add_obj('zunge', tg, m, subsurf=1)
    # Schnauze und Kinn mit Fell
    furm = bpy.data.materials.new('fell')
    furm.use_nodes = True
    nt = furm.node_tree
    for nd in list(nt.nodes):
        nt.nodes.remove(nd)
    hb = nt.nodes.new('ShaderNodeBsdfHairPrincipled')
    try:
        hb.parametrization = 'MELANIN'
    except (TypeError, AttributeError):
        pass
    hb.inputs['Melanin'].default_value = 0.62
    hb.inputs['Melanin Redness'].default_value = 0.75
    hb.inputs['Roughness'].default_value = 0.35
    hb.inputs['Random Color'].default_value = 0.35
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    nt.links.new(hb.outputs[0], out.inputs['Surface'])
    # Gesicht um das Maul: eine Flaeche (Blick zur Kamera, +y) mit Loch; am Lochrand schwarze, nasse, faltige Lefzen,
    # darueber die Schnauze bis zur Nase, seitlich die Backen (weg von der Kamera gewoelbt); Fell nur ausserhalb der Lefzen
    MA, MB, MZ = 0.070, 0.060, -0.004  # halbe Breite, halbe Hoehe, Mitte des Mauls (z)

    def rim(x, z):  # Abstand vom Rand der Maulöffnung (negativ innen), grob in Metern
        e = math.sqrt((x / MA) ** 2 + ((z - MZ) / MB) ** 2)
        return (e - 1.0) * min(MA, MB)

    # Ringe um die Oeffnung (glatter Rand statt Treppen); die Mundwinkel liegen tief hinten, oben und unten ragen
    # Oberlippe und Kinn zur Kamera
    face = bmesh.new()
    flay = face.verts.layers.float.new('lefze')
    NA, NR = 220, 90
    rings = []
    for j in range(NR + 1):
        t = j / NR
        ring = []
        for i in range(NA):
            a = TAU * i / NA
            ca, sa = math.cos(a), math.sin(a)
            wob = 1.0 + 0.05 * math.sin(3 * a + 0.4) + 0.035 * noise.noise(V((ca * 2, sa * 2, seed)))
            rx, rz = MA * wob * ca, MB * wob * sa * (1.08 if sa > 0 else 0.95)
            grow = t * t * 0.32 + t * 0.012  # nach aussen weit ueber den Bildrand
            x = rx * (1 + grow / MA * 0.9)
            z = MZ + rz * (1 + grow / MB * 0.75)
            side = ca * ca
            y = -0.045 * side * math.exp(-t * 6) + 0.012 * (1 - side) * math.exp(-t * 5)  # Mundwinkel hinten
            y += 0.016 * math.exp(-t * 9) - 1.4 * x * x * (0.4 + 0.6 * min(1.0, abs(x) / 0.06))
            y += 0.03 * math.exp(-((z - 0.1) / 0.045) ** 2) * math.exp(-(x / 0.07) ** 2)  # Nasenruecken
            y += 0.003 * math.sin(a * 22 + t * 30) * math.exp(-t * 8) * side  # Falten am Mundwinkel
            y += 0.0012 * noise.noise(V((x * 60, z * 60, seed)))
            v = face.verts.new((x, y, z))
            v[flay] = 1.0 if t < 0.075 else 0.0
            ring.append(v)
        rings.append(ring)
    for j in range(NR):
        for i in range(NA):
            q = [rings[j][i], rings[j][(i + 1) % NA], rings[j + 1][(i + 1) % NA], rings[j + 1][i]]
            f = face.faces.new(q)
            f.normal_update()
            if f.normal.y < 0:
                f.normal_flip()
            f.material_index = 0 if min(v[flay] for v in q) > 0.5 else 1
    lipm, lb2 = material('lefzenhaut')
    nb = N(lipm)
    n1 = nb.n('ShaderNodeTexNoise', Scale=300.0, Detail=8.0)
    nb.link(nb.coord('Object'), n1.inputs['Vector'])
    wv = nb.n('ShaderNodeTexWave', Scale=60.0, Distortion=6.0, Detail=4.0, _bands_direction='Z')
    nb.link(nb.coord('Object'), wv.inputs['Vector'])
    nb.link(nb.ramp(n1.outputs['Fac'], [(0.4, (0.012, 0.010, 0.010)), (0.7, (0.06, 0.045, 0.045))]).outputs[0], lb2.inputs['Base Color'])
    nb.link(bump(nb, nb.math('ADD', n1.outputs['Fac'], nb.math('MULTIPLY', wv.outputs['Fac'], 0.4)), 0.35, 0.001), lb2.inputs['Normal'])
    bsdf_set(lb2, rough=0.32, coat=0.35, coat_r=0.08, sss=0.05)
    skinm = mat_simple('hautgrund', (0.10, 0.07, 0.05), rough=0.6)
    ob = add_obj('gesicht', face, lipm)
    ob.data.materials.append(skinm)
    ob.data.materials.append(furm)
    vg = ob.vertex_groups.new(name='fell')
    vg.add([v.index for v in ob.data.vertices if v.co.y < 0.03 or True], 1.0, 'REPLACE')
    for p in ob.data.polygons:  # auf den Lefzen kein Fell
        if p.material_index == 0:
            vg.add(list(p.vertices), 0.0, 'REPLACE')
    ps = ob.modifiers.new('fell', 'PARTICLE_SYSTEM').particle_system
    ps.vertex_group_density = 'fell'
    st = ps.settings
    st.type = 'HAIR'
    st.count = 26000
    st.hair_length = 0.007
    st.material = 3
    st.use_advanced_hair = True
    st.normal_factor = 0.0045
    st.object_align_factor = (0.0, 0.0, -0.0042)  # Fell liegt an und faellt nach unten
    st.factor_random = 0.0012
    st.child_type = 'INTERPOLATED'
    for k_, v_ in (('child_nbr', 6), ('child_percent', 6)):
        if hasattr(st, k_):
            setattr(st, k_, v_)
    st.rendered_child_count = 8
    st.root_radius = 1.0
    st.tip_radius = 0.0
    st.radius_scale = 0.00016
    st.roughness_2 = 0.0012
    st.clump_factor = 0.2
    ps.seed = rng.randrange(1000)
    # Nase oben im Bild
    nose = blob((0.0, 0.0, 0.118), (0.032, 0.022, 0.02), seed + 9, 0.06, 48)
    m, b = material('nase')
    nb = N(m)
    vo = nb.n('ShaderNodeTexVoronoi', Scale=900.0)
    nb.link(nb.coord('Object'), vo.inputs['Vector'])
    nb.link(bump(nb, vo.outputs['Distance'], 0.8, 0.0008), b.inputs['Normal'])
    bsdf_set(b, base=(0.035, 0.03, 0.03), rough=0.3, coat=0.4)
    add_obj('nase', nose, m, subsurf=1)
    # Speichelfaeden zwischen den Kiefern
    drool = mat_simple('speichel', (0.95, 0.95, 0.93), rough=0.04, trans=0.6, ior=1.34, spec=0.8, coat=0.5)
    for k in range(5):
        x = rng.uniform(-0.04, 0.04)
        y = -0.01 - rng.uniform(0, 0.03)
        za, zb = 0.03, -0.035
        sag = rng.uniform(0.004, 0.012)
        pts = [V((x + 0.004 * math.sin(i), y - sag * math.sin(math.pi * i / 10) * 0.5, za + (zb - za) * i / 10)) for i in range(11)]
        add_obj('speichel', tube(pts, 0.0, 8, lambda t: 0.0011 * (1.0 + 1.5 * (0.5 - abs(t - 0.5)) ** 2 - 0.55 * math.sin(math.pi * t))), drool)
    return ((0.0, 0.33, 0.02), (0.0, -0.03, 0.0), 55, 6.3)


# =====================================================================================================================
def render(name, out, samples, scale):
    sc = reset(samples)
    seed = sum(map(ord, name))
    if name.startswith('meat_'):
        loc, tgt, lens, fs = scene_meat(name, seed)
        flash = 0.6
    elif name == 'eye_a':
        loc, tgt, lens, fs = scene_eye(seed, ((0.12, 0.20, 0.08), (0.30, 0.38, 0.14), (0.55, 0.50, 0.22), (0.62, 0.48, 0.20)), (14, 5))
        flash = 0.12
    elif name == 'eye_b':
        loc, tgt, lens, fs = scene_eye(seed, ((0.10, 0.05, 0.02), (0.24, 0.12, 0.04), (0.40, 0.24, 0.10), (0.55, 0.36, 0.16)), (-11, -4))
        flash = 0.12
    else:
        loc, tgt, lens, fs = scene_mouth(seed)
        flash = 2.2
    w, h = SIZES.get(name, (512, 512))
    sc.render.resolution_x = int(w * scale)
    sc.render.resolution_y = int(h * scale)
    sc.render.resolution_percentage = 100
    d = (V(tgt) - V(loc)).length
    camera(sc, loc, tgt, lens=lens, fstop=fs, flash=flash * d * d / 0.1, flash_off=(0.0, d * 0.12), fill=0.0)
    sc.render.filepath = os.path.join(out, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('FOTO', name, sc.render.filepath, flush=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    out = argv[0]
    samples, scale = 96, 1.0
    names = []
    i = 1
    while i < len(argv):
        if argv[i] == '--samples':
            samples = int(argv[i + 1])
            i += 2
        elif argv[i] == '--scale':
            scale = float(argv[i + 1])
            i += 2
        else:
            names.append(argv[i])
            i += 1
    names = names or (MEATS + ['eye_a', 'eye_b', 'mouth'])
    os.makedirs(out, exist_ok=True)
    for n in names:
        render(n, out, samples, scale)


main()
