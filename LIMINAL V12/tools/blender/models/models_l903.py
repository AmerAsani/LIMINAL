"""V12 Level 903 (LOOK_903.md): The Metro - fahrender Wagen mit offenen Tueren und Zielanzeigen (Bildzeichen), ruhender
Zug als Wand (Abschnitte, Kupplung, ein bewohnter Wagen), Gleis mit Schotter, lange duenne Leuchtstoffroehren,
leuchtende Schilder in unbekannter Schrift (erfundene Zeichen), Bahnsteigkante, zugemauerte Treppe.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix

import common
from models.models_l2 import _cyl


def _m():
    return {
        'body': common.material('wagenkasten', (0.70, 0.72, 0.74), look='metal', tex='metal_aluminum', rough=0.4, metal=0.7),
        'body_old': common.material('wagen_alt', (0.52, 0.54, 0.55), look='metal', tex='metal_aluminum', rough=0.55, metal=0.6),
        'stripe_r': common.material('streifen_rot', (0.62, 0.10, 0.08), look='paint', rough=0.5),
        'stripe_b': common.material('streifen_blau', (0.12, 0.24, 0.50), look='paint', rough=0.5),
        'win': common.material('wagenfenster', (0.06, 0.07, 0.08), look='glass', rough=0.05),
        'inner': common.material('innenwand', (0.86, 0.84, 0.78), look='paint', rough=0.5),
        'floor': common.material('wagenboden', (0.30, 0.31, 0.30), look='paint', rough=0.8),
        'seat': common.material('sitzbank', (0.22, 0.30, 0.48), look='fabric', rough=0.9),
        'steel': common.material('stahl', (0.70, 0.71, 0.72), look='metal', rough=0.3, metal=1.0),
        'dark': common.material('dunkel', (0.05, 0.05, 0.05), look='paint', rough=0.8),
        'rubber': common.material('gummi', (0.08, 0.08, 0.08), look='rubber', rough=0.9),
        'light': common.material('innenlicht', (1.0, 0.98, 0.92), look='glow', glow=1.4),
        'tube': common.material('roehre', (0.95, 1.0, 1.0), look='glow', glow=2.2),
        'ballast': common.material('schotter', (0.36, 0.34, 0.32), look='concrete', tex='concrete_rough', rough=0.95),
        'sleeper': common.material('schwelle', (0.30, 0.29, 0.27), look='concrete', rough=0.9),
        'rail': common.material('schiene', (0.45, 0.40, 0.36), look='metal', tex='metal_rusty_iron', rough=0.5, metal=0.8),
        'yellow': common.material('gelb', (0.86, 0.68, 0.10), look='paint', rough=0.6),
        'brick': common.material('ziegel_grau', (0.85, 0.85, 0.85), look='concrete', tex='bricks_dark_grey', rough=0.9),
        'board': common.material('bretter', (0.42, 0.32, 0.22), look='wood', rough=0.85),
    }


L, WID, H = 18.0, 3.0, 2.5  # Wagen: Laenge, Breite, Innenhoehe


def _glyphs(name, y0, z0, w, h, mat, x, seed, n=6, sign=1):
    """Erfundene Schriftzeichen (keine echte Schrift): je Zeichen zwei bis vier Striche in einem Kasten."""
    rng = random.Random(seed)
    cw = w / n
    for k in range(n):
        cy = y0 + (k + 0.5) * cw
        for j in range(rng.randint(2, 4)):
            horiz = rng.random() < 0.5
            a = rng.uniform(0.15, 0.85)
            if horiz:
                ya, yb = cy - cw * 0.35, cy + cw * rng.uniform(-0.1, 0.35)
                za = zb = z0 + h * a
                common.box(f'{name}{k}{j}', (x, min(ya, yb), za - h * 0.05), (x + 0.004 * sign, max(ya, yb), za + h * 0.05), mat)
            else:
                zc0, zc1 = z0 + h * rng.uniform(0.1, 0.4), z0 + h * rng.uniform(0.6, 0.95)
                yc = cy + cw * (a - 0.5) * 0.6
                common.box(f'{name}{k}{j}', (x, yc - cw * 0.06, zc0), (x + 0.004 * sign, yc + cw * 0.06, zc1), mat)


def _pictogram(name, kind, x, cz, M, sign=1):
    """Bildzeichen auf der Zielanzeige (sprachfrei): 'wave' Wellen ueber einem Becken (Level 37), 'plane' Flugzeug (36)."""
    white = common.material('anzeige_weiss', (0.95, 0.98, 1.0), look='glow', glow=1.6)

    def poly(pts, k=1.7):
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((x, y * k, cz + (z - cz) * k)) for (y, z) in pts])
        f.normal_update()
        if f.normal.x * sign < 0:
            f.normal_flip()
        common.obj_from_bmesh(name, bm, white)
    if kind == 'wave':
        for r in range(2):
            z0 = cz + 0.04 - r * 0.09
            pts = [(-0.22 + 0.44 * t, z0 + 0.03 * math.sin(t * 4 * math.pi)) for t in [k / 24 for k in range(25)]]
            pts += [(y, z - 0.025) for (y, z) in reversed(pts)]
            poly(pts)
        poly([(-0.26, cz - 0.17), (0.26, cz - 0.17), (0.26, cz - 0.14), (-0.26, cz - 0.14)])
    else:
        poly([(-0.25, cz - 0.02), (0.22, cz - 0.02), (0.26, cz), (0.22, cz + 0.02), (-0.25, cz + 0.02)])  # Rumpf
        poly([(-0.02, cz), (0.06, cz), (-0.08, cz + 0.16), (-0.14, cz + 0.16)])  # Fluegel
        poly([(-0.02, cz), (0.06, cz), (-0.08, cz - 0.16), (-0.14, cz - 0.16)])
        poly([(-0.25, cz), (-0.2, cz), (-0.26, cz + 0.08), (-0.29, cz + 0.08)])  # Leitwerk


def build_metro_car_903():
    """Fahrender Metro-Wagen (Boden, Ursprung Mitte Wagenboden, 18 x 3 m, entlang x): Kasten mit Streifen, Fenster,
    je Seite drei offene Tueren, Innenraum mit Baenken, Stangen, Licht; an den Stirnseiten innen Zielanzeigen mit
    Bildzeichen (-x: Wellen = Level 37, +x: Flugzeug = Level 36). Wiki: der Wagen nimmt Ziele an."""
    M = _m()
    doors = (-6.0, 0.0, 6.0)
    common.box('boden', (-L / 2, -WID / 2, -0.12), (L / 2, WID / 2, 0.0), M['floor'])
    common.box('dach', (-L / 2, -WID / 2, H), (L / 2, WID / 2, H + 0.12), M['body'], bevel=0.05, segments=3)
    common.box('decke_innen', (-L / 2 + 0.06, -WID / 2 + 0.06, H - 0.02), (L / 2 - 0.06, WID / 2 - 0.06, H), M['inner'])
    for s in (-1, 1):
        y0, y1 = (s * WID / 2 - 0.06, s * WID / 2) if s > 0 else (-WID / 2, -WID / 2 + 0.06)
        xs = [-L / 2] + [v for d in doors for v in (d - 0.7, d + 0.7)] + [L / 2]
        for k in range(0, len(xs), 2):
            a, b = xs[k], xs[k + 1]
            common.box(f'seite{s}{k}', (a, y0, -0.6), (b, y1, H), M['body'])
            yo = y1 + 0.004 if s > 0 else y0 - 0.004
            common.box(f'streifen{s}{k}', (a, min(y1, yo) if s > 0 else yo, 0.55), (b, max(y1, yo) if s > 0 else y0, 0.7), M['stripe_r'])
            # Fenster: Leuchtflaeche innen sichtbar, dunkle Scheibe aussen
            for wx in [a + 0.5 + 1.25 * j for j in range(int((b - a - 0.6) / 1.25))]:
                common.box(f'fenster{s}{k}{wx}', (wx, min(y0, yo) - 0.001, 1.0), (wx + 1.0, max(y1, yo) + 0.001, 1.9), M['win'])
        for d in doors:  # Sturz ueber der Tuer, Tuerblaetter in die Wand geschoben
            common.box(f'sturz{s}{d}', (d - 0.7, y0, 2.05), (d + 0.7, y1, H), M['body'])
            for side in (-1, 1):  # Tuerblaetter in die Wandtaschen geschoben
                xa, xb = (d + 0.7, d + 1.35) if side > 0 else (d - 1.35, d - 0.7)
                common.box(f'blatt{s}{d}{side}', (xa, min(y0, y1) - s * 0.03, 0.0), (xb, max(y0, y1) - s * 0.03, 2.05), M['body'])
    for s in (-1, 1):  # Stirnwaende mit Fenster und Zielanzeige
        x0, x1 = (L / 2 - 0.08, L / 2) if s > 0 else (-L / 2, -L / 2 + 0.08)
        common.box(f'stirn{s}', (x0, -WID / 2, -0.6), (x1, WID / 2, H), M['body'])
        xi = x0 - 0.002 if s > 0 else x1 + 0.002
        common.box(f'anzeige{s}', (min(xi, xi - s * 0.03), -0.6, 1.72), (max(xi, xi - s * 0.03), 0.6, 2.42), M['dark'])
        _pictogram(f'zeichen{s}', 'plane' if s > 0 else 'wave', xi - s * 0.032, 2.07, M, sign=-s)
    # Innenraum: Laengsbaenke, Stangen, Lichtband
    for s in (-1, 1):
        for (a, b) in ((-L / 2 + 0.3, -6.8), (-5.2, -0.8), (0.8, 5.2), (6.8, L / 2 - 0.3)):
            ya, yb = (WID / 2 - 0.5, WID / 2 - 0.06) if s > 0 else (-WID / 2 + 0.06, -WID / 2 + 0.5)
            common.box(f'bank{s}{a}', (a, ya, 0.0), (b, yb, 0.45), M['seat'], bevel=0.03)
            common.box(f'lehne{s}{a}', (a, WID / 2 - 0.16 if s > 0 else -WID / 2 + 0.06, 0.45), (b, WID / 2 - 0.06 if s > 0 else -WID / 2 + 0.16, 0.95), M['seat'], bevel=0.03)
    for x in (-7.5, -3.0, 3.0, 7.5):
        _cyl(f'stange{x}', (x, 0.0, 0.0), (x, 0.0, H), 0.02, M['steel'], seg=10)
    _cyl('haltestange_l', (-L / 2 + 0.5, -0.6, 1.95), (L / 2 - 0.5, -0.6, 1.95), 0.016, M['steel'], seg=8)
    _cyl('haltestange_r', (-L / 2 + 0.5, 0.6, 1.95), (L / 2 - 0.5, 0.6, 1.95), 0.016, M['steel'], seg=8)
    common.box('lichtband', (-L / 2 + 0.5, -0.12, H - 0.06), (L / 2 - 0.5, 0.12, H - 0.02), M['light'])
    # Fahrwerk unter dem Boden
    for x in (-6.5, 6.5):
        common.box(f'drehgestell{x}', (x - 1.2, -1.1, -0.95), (x + 1.2, 1.1, -0.15), M['dark'])
        for wx in (x - 0.8, x + 0.8):
            for s in (-1, 1):
                _cyl(f'rad{x}{wx}{s}', (wx, s * 0.72, -0.65), (wx, s * 0.82, -0.65), 0.38, M['rail'], seg=16)


def _dormant_side(M, home=False, seed=1):
    """6 m Abschnitt der Seitenwand des ruhenden Zugs (Wandteil, Flaeche x = 0, Blick +x, z = 0 = Wagenboden):
    Kasten, Streifen, zwei Fenster, eine geschlossene Tuer, Dachkante; dunkel innen - oder bewohnt."""
    rng = random.Random(seed)
    common.box('kasten', (0.0, -3.0, -0.5), (0.08, 3.0, 2.5), M['body_old'])
    common.box('dachkante', (0.0, -3.0, 2.5), (0.25, 3.0, 2.75), M['body_old'], bevel=0.06, segments=3)
    common.box('streifen', (0.08, -3.0, 0.55), (0.085, 3.0, 0.7), M['stripe_b'])
    inside = M['win']
    for wy in (-2.3, 0.9):
        common.box(f'fensterrahmen{wy}', (0.08, wy - 0.04, 0.96), (0.09, wy + 1.44, 1.0), M['rubber'])
        if not home:
            common.box(f'fenster{wy}', (0.08, wy, 1.0), (0.086, wy + 1.4, 1.9), inside)
            continue
        # bewohnt: zugezogene Vorhaenge mit Falten, in der Mitte ein schmaler, warm leuchtender Spalt
        cloth = common.material('vorhang', (0.46, 0.20, 0.12), look='fabric', rough=0.9)
        fold = common.material('vorhang_falte', (0.30, 0.12, 0.07), look='fabric', rough=0.9)
        slit = common.material('spalt', (1.0, 0.70, 0.42), look='glow', glow=1.2)
        common.box(f'spalt{wy}', (0.08, wy + 0.64, 1.0), (0.084, wy + 0.76, 1.9), slit)
        for (a, b) in ((wy, wy + 0.64), (wy + 0.76, wy + 1.4)):
            common.box(f'vorhang{wy}{a}', (0.084, a, 1.0), (0.09, b, 1.9), cloth)
            for k in range(4):
                fy = a + (k + 0.5) * (b - a) / 4
                common.box(f'falte{wy}{a}{k}', (0.09, fy - 0.025, 1.0), (0.094, fy + 0.025, 1.9), fold)
    if home:
        inside = common.material('spalt', (1.0, 0.70, 0.42), look='glow', glow=1.2)
    common.box('tuer', (0.08, -0.7, 0.0), (0.09, 0.7, 2.05), M['body_old'])
    common.box('tuerfuge', (0.09, -0.01, 0.0), (0.092, 0.01, 2.05), M['rubber'])
    for s in (-1, 1):
        common.box(f'tuerfenster{s}', (0.09, s * 0.35 - 0.2, 1.1), (0.094, s * 0.35 + 0.2, 1.85), inside)
    if home:  # Wiki: Wagen als Unterkunft und Handelsposten
        common.box('schild', (0.09, 0.95, 2.05), (0.1, 2.25, 2.4), M['board'])
        _glyphs('schild_z', 1.0, 2.1, 1.2, 0.25, M['dark'], 0.1, seed + 7, n=5)
        _cyl('topf', (0.35, -2.6, 0.0), (0.35, -2.6, 0.32), 0.15, common.material('topf', (0.55, 0.30, 0.18), look='ceramic', rough=0.7), seg=14)
        for k in range(5):
            a = k * 1.25
            _cyl(f'blatt{k}', (0.35, -2.6, 0.3), (0.35 + math.cos(a) * 0.22, -2.6 + math.sin(a) * 0.22, 0.65 + 0.04 * k), 0.02,
                 common.material('pflanze', (0.20, 0.36, 0.16), look='paint', rough=0.8), seg=5)
    else:
        for k in range(rng.randint(1, 3)):  # Aufkleber, verblasst
            y = rng.uniform(-2.6, 2.2)
            common.box(f'aufkleber{k}', (0.086, y, rng.uniform(0.8, 2.1)), (0.088, y + rng.uniform(0.15, 0.4), rng.uniform(0.9, 2.3)),
                       common.material(f'aufkleber{k}', [(0.8, 0.7, 0.2), (0.2, 0.5, 0.7), (0.8, 0.8, 0.8)][k % 3], look='paint', rough=0.7))


def build_metro_dormant_a():
    """Ruhender Zug (Wiki: auf dem rechten Gleis ein ruhender Wagen ueber die ganze Laenge), 6 m Abschnitt."""
    _dormant_side(_m(), seed=3)


def build_metro_dormant_b():
    _dormant_side(_m(), seed=11)


def build_metro_dormant_home():
    """Ein Abschnitt des ruhenden Zugs als Unterkunft (Wiki: Wagen als Stuetzpunkte, Motels, Handelsposten)."""
    _dormant_side(_m(), home=True, seed=5)


def build_metro_dormant_coupling():
    """Wagenuebergang des ruhenden Zugs (Wandteil, 1 m): Faltenbalg, Kupplung."""
    M = _m()
    for k in range(7):
        common.box(f'falte{k}', (0.0, -0.45 + k * 0.13, 0.0), (0.05 + 0.03 * (k % 2), -0.38 + k * 0.13, 2.3), M['rubber'])
    common.box('kupplung', (0.0, -0.4, -0.3), (0.15, 0.4, -0.1), M['dark'])


# --- Gleis, Bahnsteig, Licht, Schilder, Treppe -----------------------------------------------------------------------------
def build_track_metro():
    """Gleis (Boden, 6 m entlang x, 5 m breit): Schotterbett, Betonschwellen, zwei Schienen, Stromschiene."""
    M = _m()
    common.box('schotter', (-3.0, -2.5, 0.0), (3.0, 2.5, 0.12), M['ballast'])
    for k in range(10):
        x = -2.7 + k * 0.6
        common.box(f'schwelle{k}', (x - 0.12, -1.25, 0.12), (x + 0.12, 1.25, 0.24), M['sleeper'])
    for s in (-1, 1):
        common.box(f'fuss{s}', (-3.0, s * 0.7175 - 0.06, 0.24), (3.0, s * 0.7175 + 0.06, 0.26), M['rail'])
        common.box(f'steg{s}', (-3.0, s * 0.7175 - 0.01, 0.26), (3.0, s * 0.7175 + 0.01, 0.36), M['rail'])
        common.box(f'kopf{s}', (-3.0, s * 0.7175 - 0.035, 0.36), (3.0, s * 0.7175 + 0.035, 0.4), M['rail'])
    common.box('stromschiene', (-3.0, 1.7, 0.3), (3.0, 1.78, 0.38), M['steel'])
    for x in (-2.0, 1.0):
        common.box(f'isolator{x}', (x - 0.05, 1.68, 0.12), (x + 0.05, 1.8, 0.3), M['yellow'])


def build_platform_edge():
    """Bahnsteigkante (Boden, 1 m entlang x, Kante bei y = +0.5): Kantenstein, gelbe Linie mit Noppen."""
    M = _m()
    common.box('stein', (-0.5, 0.2, -0.02), (0.5, 0.52, 0.005), common.material('kantenstein', (0.62, 0.62, 0.60), look='concrete', tex='concrete_rough', rough=0.8))
    common.box('linie', (-0.5, -0.15, 0.0), (0.5, 0.1, 0.008), M['yellow'])
    for i in range(5):
        for j in range(2):
            _cyl(f'noppe{i}{j}', (-0.4 + i * 0.2, -0.08 + j * 0.12, 0.008), (-0.4 + i * 0.2, -0.08 + j * 0.12, 0.016), 0.025, M['yellow'], seg=8)


def build_tube_long():
    """Lange, duenne Leuchtstoffroehre (Deckenteil, 3 m entlang x; Wiki: lange duenne Leuchten entlang der Wege)."""
    M = _m()
    common.box('schiene', (-1.5, -0.04, -0.06), (1.5, 0.04, 0.0), M['steel'])
    _cyl('roehre', (-1.45, 0.0, -0.09), (1.45, 0.0, -0.09), 0.018, M['tube'], seg=10)
    for x in (-1.0, 1.0):
        _cyl(f'pendel{x}', (x, 0.0, -0.06), (x, 0.0, 0.0), 0.004, M['steel'], seg=4)


def _hanging_sign(seed, arrow):
    M = _m()
    panel = common.material('schild_licht', (0.98, 0.94, 0.70), look='glow', glow=1.1)
    ink = common.material('schild_schrift', (0.06, 0.06, 0.07), look='paint', rough=0.6)
    for s in (-1, 1):
        _cyl(f'pendel{s}', (0.0, s * 0.9, -0.6), (0.0, s * 0.9, 0.0), 0.008, M['steel'], seg=6)
    common.box('gehaeuse', (-0.07, -1.1, -1.0), (0.07, 1.1, -0.6), M['dark'], bevel=0.01)
    for sg in (-1, 1):
        x = sg * 0.071
        common.box(f'licht{sg}', (min(x, x + sg * 0.002), -1.05, -0.97), (max(x, x + sg * 0.002), 1.05, -0.63), panel)
        _glyphs(f'z{sg}', -0.95, -0.94, 1.5, 0.28, ink, x + sg * 0.002, seed, n=6, sign=sg)
        if arrow:
            ya = 0.75
            common.box(f'pfeil{sg}', (min(x, x + sg * 0.004), 0.62, -0.81), (max(x, x + sg * 0.004), 0.95, -0.77), ink)
            common.box(f'pfeil_k{sg}', (min(x, x + sg * 0.004), 0.86, -0.87), (max(x, x + sg * 0.004), 0.92, -0.71), ink)


def build_sign_903_a():
    """Haengendes Leuchtschild (Deckenteil, Decke bei z = 0) mit Zeichen in unbekannter Schrift (Wiki) und Pfeil."""
    _hanging_sign(903, True)


def build_sign_903_b():
    _hanging_sign(1903, False)


def build_sign_903_wall():
    """Stationsschild an der Wand (Wandteil, 4 m lang, Mitte im Ursprung): dunkelblaue Tafel, helle Zeichen."""
    M = _m()
    board = common.material('stationstafel', (0.10, 0.16, 0.34), look='paint', rough=0.5)
    ink = common.material('stationsschrift', (0.92, 0.92, 0.90), look='paint', rough=0.5)
    common.box('rahmen', (0.0, -2.05, -0.38), (0.05, 2.05, 0.38), M['steel'], bevel=0.01)
    common.box('tafel', (0.05, -2.0, -0.33), (0.055, 2.0, 0.33), board)
    _glyphs('z', -1.7, -0.22, 3.4, 0.44, ink, 0.055, 9031, n=7)


def build_stairs_barricaded():
    """Treppe nach oben, zugemauert und vernagelt (Boden, Treppe entlang +x, 2,4 x 6 m; Wiki: Treppen tauchen auf, sind
    aber immer versperrt oder zugemauert): Stufen, Gelaender, oben Ziegelwand, davor Bretter."""
    M = _m()
    W_ = 2.4
    for k in range(12):
        x = -3.0 + k * 0.35
        common.box(f'stufe{k}', (x, -W_ / 2, -1.2), (x + 0.35, W_ / 2, k * 0.2), M['brick'])
    for s in (-1, 1):
        common.box(f'wange{s}', (-3.0, s * W_ / 2 - (0.12 if s > 0 else 0), 0.0), (3.0, s * W_ / 2 + (0.0 if s > 0 else 0.12), 1.0), M['brick'])
        _cyl(f'handlauf{s}', (-3.0, s * (W_ / 2 - 0.06), 1.0), (1.2, s * (W_ / 2 - 0.06), 3.0), 0.025, M['steel'], seg=8)
    common.box('mauer', (1.2, -W_ / 2, 0.0), (1.45, W_ / 2, 4.2), M['brick'])
    rng = random.Random(4)
    for k in range(5):
        ob = common.box(f'brett{k}', (0.0, -W_ / 2 - 0.1, -0.08), (0.03, W_ / 2 + 0.1, 0.08), M['board'])
        ob.matrix_world = Matrix.Translation((-3.05, 0.0, 0.4 + k * 0.32)) @ Matrix.Rotation(rng.uniform(-0.12, 0.12), 4, 'X')
    for s in (-1, 1):
        common.box(f'pfosten{s}', (-3.1, s * W_ / 2 - 0.05, 0.0), (-3.02, s * W_ / 2 + 0.05, 1.8), M['board'])
    # Ursprung an den Fuss der Treppe (vor den Brettern) legen: dort ist Bahnsteig, dahinter sperrt ein Block
    for ob in bpy.context.scene.objects:
        ob.matrix_world = Matrix.Translation((3.15, 0.0, 0.0)) @ ob.matrix_world
