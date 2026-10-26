"""V12 Viewmodel: Taschenlampe und linke Hand im Faustgriff (eigene Modelle, keine fremden Assets).

Rahmen der Lampe wie im Spiel (Body::flashGrip): Ursprung in der Griffmitte auf der Achse, +x zum Glas, z oben,
+y links (Blender; das Spiel spiegelt y beim Laden). Schaft r = 16,5 mm von x = -0,050 bis 0,080, Glas bei
x = 0,170 - wie das Code-Modell aus V7, damit Lichtquelle und Halt gleich bleiben.

Die Hand wird in ihrem eigenen Rahmen gebaut (Handgelenk im Ursprung, Finger +x, Handruecken +z, Daumen -y),
um die schraeg in der Hohlhand liegende Lampe geschlossen (Kraftgriff: die Lampe laeuft vom Kleinfingerballen zur
Daumen-Zeigefinger-Falte, der Daumen liegt oben am Schaft) und dann in den Rahmen der Lampe gedreht. Die
Fingerwinkel sucht ein kleiner Loeser so, dass Mittel- und Endglieder am Schaft anliegen, ohne ihn zu
durchdringen. Die Form entsteht aus Ellipsoiden, die ein Voxel-Remesh vereinigt und eine Glaettung verschmilzt.

Im Spiel haengt die Hand am Rahmen der Lampe; der Unterarm des Koerpers fuehrt zum Handgelenk (HAND_WRIST,
HAND_FOREARM werden beim Bau ausgegeben und stehen in gameplay/body.cpp).
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

import common

RB = 0.0165       # Schaftradius
LENS_X = 0.170    # Glas
SKIN = (0.80, 0.62, 0.52)

# --- Taschenlampe -----------------------------------------------------------------------------------------


def _lathe(name, prof, mat, seg=48, smooth_deg=40.0):
    """Drehkoerper um x aus (x, r)-Punkten (r = 0 schliesst die Achse)."""
    bm = bmesh.new()
    rings = []
    for (x, r) in prof:
        ring = []
        for k in range(seg):
            a = 2.0 * math.pi * k / seg
            ring.append(bm.verts.new((x, r * math.cos(a), r * math.sin(a))))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for k in range(seg):
            j = (k + 1) % seg
            try:
                bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
            except ValueError:
                pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = common.obj_from_bmesh(name, bm, mat)
    common.shade_auto(ob, smooth_deg)
    return ob


def _flashlight(lit):
    body = common.material('gehaeuse', (0.10, 0.10, 0.11), look='metal', rough=0.45, fm='flash_body')
    bezel = common.material('ring', (0.62, 0.62, 0.60), look='metal', rough=0.3, metal=1.0, fm='flash_bezel')
    rubber = common.material('gummi', (0.05, 0.05, 0.05), look='rubber', rough=0.9, fm='flash_rubber')
    if lit:
        glass = common.material('glas_an', (1.0, 0.95, 0.85), look='glow', glow=7.0, fm='flash_lens')
    else:
        glass = common.material('glas', (0.30, 0.31, 0.32), look='glass', rough=0.1, fm='flash_glass')
    # Endkappe mit Gummiring, Schaft, Hals, Kopf mit Kuehlrillen; Ring um das Glas
    _lathe('endkappe', [(-0.0615, 0.0), (-0.0615, 0.0120), (-0.0605, 0.0158), (-0.0585, 0.0172), (-0.0505, 0.0172),
                        (-0.0500, 0.0166)], body)
    _lathe('gummiring', [(-0.0500, 0.0150), (-0.0495, 0.0170), (-0.0440, 0.0170), (-0.0435, 0.0150)], rubber)
    prof = [(-0.0440, 0.0160), (-0.0430, RB)]
    # Griffzone mit flachen Rillen (Riffelung angedeutet), dann glatt bis zum Hals
    x = -0.040
    while x < 0.050:
        prof += [(x, RB), (x + 0.0035, RB), (x + 0.0045, RB - 0.0006), (x + 0.0055, RB)]
        x += 0.0060
    prof += [(0.0800, RB), (0.0880, 0.0172), (0.1000, 0.0205), (0.1100, 0.0232), (0.1140, 0.0236)]
    x = 0.1150
    for k in range(5):  # Kuehlrillen am Kopf
        prof += [(x, 0.0236), (x + 0.0040, 0.0236), (x + 0.0048, 0.0224), (x + 0.0068, 0.0224), (x + 0.0076, 0.0236)]
        x += 0.0076
    prof += [(0.1600, 0.0236), (0.1605, 0.0230)]
    _lathe('schaft', prof, body, seg=48, smooth_deg=30.0)
    _lathe('ring', [(0.1600, 0.0228), (0.1604, 0.0246), (0.1690, 0.0248), (0.1720, 0.0240), (0.1725, 0.0212),
                    (0.1712, 0.0205)], bezel, smooth_deg=30.0)
    _lathe('glas', [(0.1700, 0.0206), (0.1702, 0.0)], glass, seg=48)
    # Schalter: Gummikappe oben auf dem Schaft vor der Griffzone (unter dem Daumen)
    sw = bmesh.new()
    bmesh.ops.create_uvsphere(sw, u_segments=24, v_segments=12, radius=1.0)
    for v in sw.verts:
        v.co = Vector((0.062 + v.co.x * 0.0075, v.co.y * 0.0060, RB - 0.0008 + max(v.co.z, -0.2) * 0.0032))
    ob = common.obj_from_bmesh('schalter', sw, rubber)
    common.shade_auto(ob, 60.0)
    _lathe('schalterring', [(0.0530, RB), (0.0530, RB + 0.0004), (0.0710, RB + 0.0004), (0.0710, RB)], bezel)


def build_vm_flashlight():
    _flashlight(False)


def build_vm_flashlight_on():
    _flashlight(True)


# V13: die Hand steht in models_hand.py (Kaefig mit Subdivision, Skelett, Posen).
