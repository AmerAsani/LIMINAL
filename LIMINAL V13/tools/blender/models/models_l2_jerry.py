"""V13.3 Teil 4 Level 2: Jerrys Raum (KREATUR_Jerry.md; Wikidot Entity 7 Rev. 14, Level 2 Rev. 93).

Level 2: Jerry ist an seinen Raum in der Basis der Followers gebunden. Entity 7: ein Video aus Level 2 zeigt ihn auf einem
rostigen Rohr; die Followers bemalen ihre Bauten blau und mit Vogelbildern, kleben Bilder von Jerry an die Waende, malen
Anhaenger, die ihn anbeten, und locken Neue mit Schriften, die Sicherheit versprechen. Die Followers selbst sind Menschen
(nicht im Spiel) - nur ihre Spuren:

  jerry_perch   das Sitzrohr: rostige Steigleitung mit einem waagrechten Abzweig in 1,52 m (Jerry sitzt darauf, Wurzel
                ueber dem Ursprung), Absperrschieber; darunter Kot (weisse Laeufe am Rohr, Kleckse am Boden), ausgefallene
                blaue Federn, aufgeknackte Nussschalen
  jerry_shrine  Opfertisch der Followers: zwei Kisten unter blauem Tuch, heruntergebrannte Kerzen, Nuesse und Saaten auf
                einem Blechteller, ein Becher Wasser, ein Glas mit blauen Federn, ein gerahmtes Foto des Vogels, Flugblaetter,
                eine Dose blauer Farbe; davor ein Kniekissen mit Abdruecken
  jerry_mural   Wandbild in blauer Farbe: der Vogel mit ausgebreiteten Fluegeln und Strahlen, darunter kniende Gestalten mit
                erhobenen Armen, blaue Handabdruecke, Gekritzel; Farbe laeuft herab
  jerry_poster  Fotos eines blauen Papageis mit Klebeband an der Wand, darunter ein blauer Pfeil (+y: Richtung des Raums)
                und Gekritzel; v1: nur der gemalte Pfeil mit einem Vogelzeichen

Konventionen: Meter, z oben, Blick entlang +x; Bodenmodelle Ursprung am Boden; Wandmodelle: Wandflaeche x = 0.
"""
import math
import random

from mathutils import Vector

from models import models_l1 as L1
from models.models_l1_phenomena import blob2, poly_on
from models.models_l1_sectors import Kit, blk, blob, cyl, sweep
from models.models_l3 import mat

V = Vector
TAU = 2.0 * math.pi
PERCH_Z = 1.52   # Oberkante des Abzweigs (anim::jy::kPerchZ)
PERCH_R = 0.03


def _mats():
    return {
        'rust': mat('rost_rohr', (0.95, 0.88, 0.82), 'metal', tex='metal_rusty_iron', r=1.35, m=0.12),
        'rust_dark': mat('rost_rohr_dunkel', (0.50, 0.40, 0.34), 'metal', tex='metal_rusty_iron', r=1.45, m=0.08),
        'iron': mat('guss_alt', (0.30, 0.29, 0.28), 'metal', r=1.1),
        'paint_red': mat('handrad_rot', (0.45, 0.10, 0.07), 'paint', r=1.3),
        'dung': mat('kot_kalk', (0.86, 0.85, 0.80), 'plastic', r=1.2),
        'dung_d': mat('kot_dunkel', (0.32, 0.33, 0.26), 'plastic', r=0.9),
        'feather': mat('feder_blau', (0.13, 0.22, 0.62), 'fabric', r=0.9),
        'feather_d': mat('feder_unterseite', (0.07, 0.07, 0.09), 'fabric', r=1.0),
        'down': mat('daune_grau', (0.62, 0.64, 0.70), 'fabric', r=1.4),
        'shell': mat('nussschale', (0.36, 0.22, 0.12), 'wood', r=1.2),
        'shell_in': mat('nuss_innen', (0.72, 0.60, 0.42), 'wood', r=1.3),
        'seed': mat('saat', (0.52, 0.45, 0.30), 'wood', r=1.3),
        'wood': mat('kistenholz', (0.62, 0.50, 0.36), 'wood', r=1.3),
        'cloth': mat('tuch_blau', (0.10, 0.18, 0.48), 'fabric', r=1.35),
        'cloth_d': mat('tuch_blau_falte', (0.06, 0.10, 0.28), 'fabric', r=1.45),
        'wax': mat('wachs', (0.86, 0.82, 0.68), 'plastic', r=0.7),
        'wick': mat('docht', (0.05, 0.05, 0.05), 'fabric', r=1.4),
        'tin': mat('blech', (0.55, 0.55, 0.54), 'metal', r=0.9),
        'water': mat('wasser_truebe', (0.30, 0.32, 0.26), 'water', r=0.2),
        'glass': mat('glas', (0.70, 0.76, 0.74), 'glass', r=0.2),
        'paper': mat('papier', (0.84, 0.82, 0.74), 'cardboard', r=1.3),
        'paper_old': mat('papier_alt', (0.70, 0.64, 0.50), 'cardboard', r=1.35),
        'ink': mat('schrift', (0.10, 0.10, 0.12), 'paint', r=1.2),
        'blue': mat('farbe_blau', (0.10, 0.22, 0.62), 'paint', r=1.15),
        'blue_d': mat('farbe_blau_dick', (0.07, 0.15, 0.46), 'paint', r=0.95),
        'photo_bg': mat('foto_grund', (0.30, 0.34, 0.28), 'plastic', r=0.6),
        'photo_bird': mat('foto_vogel', (0.14, 0.24, 0.66), 'plastic', r=0.6),
        'photo_dark': mat('foto_schwarz', (0.04, 0.04, 0.05), 'plastic', r=0.6),
        'photo_yel': mat('foto_gelb', (0.92, 0.74, 0.12), 'plastic', r=0.6),
        'photo_white': mat('foto_rand', (0.88, 0.87, 0.84), 'plastic', r=0.7),
        'tape': mat('klebeband', (0.70, 0.66, 0.52), 'plastic', r=0.8),
        'frame': mat('rahmen_holz', (0.30, 0.20, 0.12), 'wood', r=1.0),
        'cushion': mat('kissen', (0.20, 0.24, 0.42), 'fabric', r=1.45),
        'flame': mat('kerzenflamme', (1.0, 0.72, 0.36), 'glow', glow=4.0),
    }


# --- Bausteine ----------------------------------------------------------------------------------------------------------
def _bird_side(cx, cy, s, face=1.0):
    """Sitzender Papagei von der Seite (2D, Blick nach +u bei face=1): Koerper mit langem Schwanz; Kopf, Schnabel, Auge."""
    body = [(-0.05, 0.42), (0.02, 0.5), (0.1, 0.46), (0.14, 0.3), (0.1, 0.1), (0.04, -0.05), (-0.04, -0.3), (-0.1, -0.62),
            (-0.14, -0.6), (-0.1, -0.25), (-0.12, 0.05), (-0.12, 0.25)]
    head = [(0.02 + 0.11 * math.cos(a), 0.55 + 0.1 * math.sin(a)) for a in [i * TAU / 14 for i in range(14)]]
    beak = [(0.1, 0.62), (0.2, 0.6), (0.24, 0.5), (0.2, 0.42), (0.16, 0.47), (0.11, 0.48)]
    eye = [(0.06 + 0.028 * math.cos(a), 0.58 + 0.028 * math.sin(a)) for a in [i * TAU / 10 for i in range(10)]]
    f = lambda pts: [(cx + face * u * s, cy + v * s) for (u, v) in pts]  # noqa: E731
    return f(body), f(head), f(beak), f(eye)


def _photo(k, M, o, u, v, w, h, rng, face=1.0, lift=0.0):
    """Ausgedrucktes Foto eines blauen Papageis (Wandflaeche: o Mitte, u/v Achsen in der Flaeche)."""
    n = u.cross(v).normalized()
    base = o + n * (0.0008 + lift)
    poly_on(k(M['photo_white']), [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)], base, u, v)
    m_ = 0.008
    poly_on(k(M['photo_bg']), [(-w / 2 + m_, -h / 2 + m_), (w / 2 - m_, -h / 2 + m_), (w / 2 - m_, h / 2 - m_), (-w / 2 + m_, h / 2 - m_)],
            base + n * 0.0004, u, v)
    body, head, beak, eye = _bird_side(rng.uniform(-0.1, 0.1) * w, -0.05 * h, h * 0.62, face)
    poly_on(k(M['photo_bird']), body, base + n * 0.0008, u, v)
    poly_on(k(M['photo_bird']), head, base + n * 0.0009, u, v)
    poly_on(k(M['photo_dark']), beak, base + n * 0.0011, u, v)
    poly_on(k(M['photo_yel']), eye, base + n * 0.0011, u, v)
    # Ast unter dem Vogel
    poly_on(k(M['photo_dark']), L1.stroke([(-w / 2 + m_, -0.3 * h), (w / 2 - m_, -0.26 * h)], 0.006), base + n * 0.0007, u, v)
    # Klebeband an zwei Ecken
    for sx, sy in ((-1, 1), (1, 1)) if rng.random() < 0.6 else ((-1, 1), (1, -1)):
        c = (sx * (w / 2 - 0.004), sy * (h / 2 - 0.004))
        a = rng.uniform(-0.6, 0.6) + (0.78 if sx * sy > 0 else -0.78)
        ca, sa = math.cos(a), math.sin(a)
        pts = [(c[0] + ca * x - sa * y, c[1] + sa * x + ca * y) for (x, y) in ((-0.022, -0.008), (0.022, -0.008), (0.022, 0.008), (-0.022, 0.008))]
        poly_on(k(M['tape']), pts, base + n * 0.0014, u, v)


def _scrawl(k, m_, o, u, v, x0, y0, lines, width, rng, lh=0.045, wd=0.004):
    """Unlesbares Gekritzel (Handschrift) in Zeilen."""
    n = u.cross(v).normalized()
    for li in range(lines):
        x, pts = x0, []
        L = width * rng.uniform(0.6, 1.0)
        while x < x0 + L:
            pts.append((x, y0 - li * lh + rng.uniform(-0.008, 0.008) + 0.01 * math.sin(x * 90.0)))
            x += rng.uniform(0.01, 0.022)
            if rng.random() < 0.12:  # Wortluecke
                if len(pts) > 1:
                    poly_on(k(m_), L1.stroke(pts, wd), o + n * 0.0012, u, v)
                pts = []
                x += 0.025
        if len(pts) > 1:
            poly_on(k(m_), L1.stroke(pts, wd), o + n * 0.0012, u, v)


def _feather(k, M, c, yaw, L, rng, flat=True):
    """Ausgefallene Feder am Boden: Fahne (blau, Unterseite schwarz), Kiel; zerzaust."""
    ca, sa = math.cos(yaw), math.sin(yaw)
    u, v = V((ca, sa, 0)), V((-sa, ca, 0))
    w = L * 0.11
    pts = []
    for i in range(9):
        t = i / 8
        pts.append((t * L, w * math.sin(math.pi * min(1.0, t * 1.15)) * (1.0 + 0.25 * (rng.random() - 0.5))))
    for i in range(8, -1, -1):
        t = i / 8
        pts.append((t * L, -0.8 * w * math.sin(math.pi * min(1.0, t * 1.15)) * (1.0 + 0.25 * (rng.random() - 0.5))))
    z = rng.uniform(0.0015, 0.004)
    m_ = M['feather'] if rng.random() < 0.75 else M['feather_d']
    poly_on(k(m_), pts, V(c) + V((0, 0, z)), u, v)
    poly_on(k(M['feather_d']), [(x, -y * 0.9) for (x, y) in pts], V(c) + V((0, 0, z - 0.0008)), u, v)
    cyl(k(M['down'], 30), V(c) + u * (-0.02) + V((0, 0, z + 0.0006)), V(c) + u * (L * 0.98) + V((0, 0, z + 0.0006)), 0.0011, 5)


def _nutshell(k, M, c, rng):
    r = rng.uniform(0.012, 0.018)
    bm = k(M['shell'], 40)
    cyl(bm, V(c) + V((0, 0, 0.0)), V(c) + V((0, 0, r * 0.9)), r, 9)
    poly_on(k(M['shell_in']), [(r * 0.85 * math.cos(a), r * 0.85 * math.sin(a)) for a in [i * TAU / 9 for i in range(9)]],
            V(c) + V((0, 0, r * 0.92)), V((1, 0, 0)), V((0, 1, 0)))


# --- jerry_perch ----------------------------------------------------------------------------------------------------------
def build_jerry_perch():
    rng = random.Random(7101)
    k = Kit()
    M = _mats()
    rx, ry = -0.04, -0.5       # Steigleitung rechts hinter dem Sitzplatz
    R1, R2 = 0.055, PERCH_R
    zc = PERCH_Z - R2
    # Steigleitung bis unter die Decke, Flansche mit Schrauben
    sweep(k(M['rust'], 40), [V((rx, ry, 0.0)), V((rx, ry, 2.6))], R1, 16, caps=True)
    for z in (0.22, 2.25):
        cyl(k(M['rust_dark'], 30), V((rx, ry, z)), V((rx, ry, z + 0.035)), R1 + 0.03, 16)
        for i in range(6):
            a = i * TAU / 6 + 0.3
            cyl(k(M['iron'], 30), V((rx + math.cos(a) * (R1 + 0.02), ry + math.sin(a) * (R1 + 0.02), z - 0.012)),
                V((rx + math.cos(a) * (R1 + 0.02), ry + math.sin(a) * (R1 + 0.02), z + 0.047)), 0.007, 6)
    # T-Stueck und Abzweig nach links (quer unter Jerrys Fuessen), am Ende ein Absperrschieber mit Handrad
    cyl(k(M['rust_dark'], 30), V((rx, ry, zc - 0.08)), V((rx, ry, zc + 0.08)), R1 + 0.012, 16)
    sweep(k(M['rust'], 40), [V((rx, ry + R1, zc)), V((rx, 0.36, zc))], R2, 14, caps=False)
    cyl(k(M['rust_dark'], 30), V((rx, 0.36, zc)), V((rx, 0.4, zc)), R2 + 0.016, 12)
    blk(k(M['iron'], 0), (0.07, 0.07, 0.09), (rx, 0.44, zc), bevel=0.006)
    cyl(k(M['iron'], 30), V((rx, 0.44, zc + 0.045)), V((rx, 0.44, zc + 0.12)), 0.008, 6)
    wheel = k(M['paint_red'], 30)
    for i in range(18):
        a0, a1 = i * TAU / 18, (i + 1) * TAU / 18
        cyl(wheel, V((rx + 0.06 * math.cos(a0), 0.44 + 0.06 * math.sin(a0), zc + 0.125)),
            V((rx + 0.06 * math.cos(a1), 0.44 + 0.06 * math.sin(a1), zc + 0.125)), 0.006, 6, caps=False)
    for i in range(3):
        a = i * TAU / 3
        cyl(wheel, V((rx, 0.44, zc + 0.125)), V((rx + 0.058 * math.cos(a), 0.44 + 0.058 * math.sin(a), zc + 0.125)), 0.005, 5)
    # abgewetzt, wo die Zehen greifen: blanker Rost oben auf dem Abzweig (dunkler Streifen)
    sweep(k(M['iron'], 30), [V((rx + 0.002, -0.1, zc + R2 - 0.002)), V((rx + 0.002, 0.12, zc + R2 - 0.002))], 0.006, 6)
    # Kot: Krusten auf dem Abzweig hinter den Fuessen, kurze Laeufe am Rohr herab, Kleckse am Boden darunter (der Vogel sitzt
    # hier seit Jahren). Runde 1: vierzehn gleich lange Faeden unter dem Abzweig lasen sich wie ein Kamm
    for i in range(5):
        y = rng.uniform(-0.25, 0.25)
        a = rng.uniform(2.2, 3.6)  # an der Rueckseite des Rohrs
        c = V((rx + math.cos(a) * R2, y, zc + math.sin(a) * R2))
        blob(k(M['dung'], 40), c, (math.cos(a), 0, math.sin(a)), rng.uniform(0.008, 0.016), 0.003, rng, 9, 1.0, 0.5)
        if rng.random() < 0.5:
            sweep(k(M['dung'], 30), [c, c + V((-0.004, rng.uniform(-0.004, 0.004), -rng.uniform(0.012, 0.035)))], [0.0035, 0.002], 6)
    for i in range(10):
        z0 = rng.uniform(0.6, 1.4)
        a = rng.uniform(1.2, 2.4)
        p0 = V((rx + math.cos(a) * R1, ry + math.sin(a) * R1, z0))
        sweep(k(M['dung'], 30), [p0, p0 + V((0, 0, -rng.uniform(0.05, 0.25)))], [0.006, 0.003], 6)
    for i in range(26):  # Kleckse am Boden (Kalk weiss, Mitte dunkel), vor allem unter dem Sitzplatz
        c = V((rng.gauss(-0.12, 0.14), rng.gauss(0.0, 0.18), 0.0))
        rr = rng.uniform(0.012, 0.045)
        blob(k(M['dung'], 40), c, (0, 0, 1), rr, rr * 0.18, rng, 11, 1.0, 0.45)
        if rng.random() < 0.6:
            blob(k(M['dung_d'], 40), c + V((rng.uniform(-0.3, 0.3) * rr, rng.uniform(-0.3, 0.3) * rr, rr * 0.12)), (0, 0, 1), rr * 0.35, rr * 0.12, rng, 9, 1.0, 0.4)
    # ausgefallene Federn, Daunen, Nussschalen und Saaten
    for i in range(12):
        c = (rng.gauss(0.05, 0.4), rng.gauss(0.0, 0.45), 0.0)
        _feather(k, M, c, rng.uniform(0, TAU), rng.choice((0.06, 0.08, 0.12, 0.18, 0.3, 0.42)), rng)
    for i in range(9):
        c = V((rng.gauss(0.0, 0.3), rng.gauss(0.0, 0.3), 0.004))
        blob(k(M['down'], 40), c, (0, 0, 1), rng.uniform(0.008, 0.016), 0.006, rng, 9, 1.0, 0.6)
    for i in range(18):
        _nutshell(k, M, (rng.gauss(0.08, 0.22), rng.gauss(0.0, 0.25), 0.0), rng)
    for i in range(70):
        s = rng.uniform(0.003, 0.006)
        blk(k(M['seed'], 0), (s * 1.6, s, s * 0.6), (rng.gauss(0.05, 0.3), rng.gauss(0.0, 0.3), s * 0.3), rz=rng.uniform(0, 3))
    k.finish()


# --- jerry_shrine ---------------------------------------------------------------------------------------------------------
def build_jerry_shrine():
    rng = random.Random(7102)
    k = Kit()
    M = _mats()
    # zwei Kisten uebereinander (Ruecken zur Wand bei x = -0,28), blaues Tuch darueber mit Falten bis fast zum Boden
    blk(k(M['wood'], 0), (0.5, 0.8, 0.42), (0.0, 0.0, 0.21), bevel=0.006)
    blk(k(M['wood'], 0), (0.42, 0.6, 0.32), (-0.02, 0.0, 0.58), bevel=0.006)
    cloth = k(M['cloth'], 35)
    blk(cloth, (0.47, 0.66, 0.012), (-0.02, 0.0, 0.746), bevel=0.003)
    for side in (-1, 1):  # Tuch faellt seitlich in Falten
        for i in range(7):
            x = -0.22 + i * 0.07
            pts = [V((x, side * 0.33, 0.74)), V((x + rng.uniform(-0.01, 0.01), side * (0.335 + 0.01 * rng.random()), 0.5)),
                   V((x + rng.uniform(-0.02, 0.02), side * (0.41 + 0.02 * rng.random()), 0.3)), V((x, side * (0.43 + 0.02 * rng.random()), 0.04 + 0.05 * rng.random()))]
            sweep(k(M['cloth'] if i % 2 else M['cloth_d'], 35), pts, [0.022, 0.026, 0.03, 0.03], 7)
    for i in range(6):  # vorn
        y = -0.25 + i * 0.1
        pts = [V((0.215, y, 0.74)), V((0.235, y + rng.uniform(-0.01, 0.01), 0.55)), V((0.27, y + rng.uniform(-0.015, 0.015), 0.3)),
               V((0.28, y, 0.06 + 0.06 * rng.random()))]
        sweep(k(M['cloth'] if i % 2 else M['cloth_d'], 35), pts, [0.024, 0.028, 0.032, 0.034], 7)
    top = 0.753
    # heruntergebrannte Kerzen mit Wachslaeufen und Docht
    for (x, y, h) in ((-0.12, -0.24, 0.07), (-0.16, -0.17, 0.12), (-0.14, 0.2, 0.05), (-0.1, 0.25, 0.1), (0.1, 0.24, 0.035)):
        r = rng.uniform(0.014, 0.02)
        cyl(k(M['wax'], 35), V((x, y, top)), V((x, y, top + h)), r, 10)
        blob(k(M['wax'], 40), V((x, y, top)), (0, 0, 1), r * 2.2, 0.006, rng, 11, 1.0, 0.5)
        for j in range(3):
            a = rng.uniform(0, TAU)
            sweep(k(M['wax'], 35), [V((x + math.cos(a) * r, y + math.sin(a) * r, top + h - 0.004)),
                                    V((x + math.cos(a) * r * 1.1, y + math.sin(a) * r * 1.1, top + h * rng.uniform(0.2, 0.7)))], [0.004, 0.003], 6)
        cyl(k(M['wick'], 30), V((x, y, top + h)), V((x + 0.002, y, top + h + 0.012)), 0.0016, 5)
        if h > 0.04:  # Runde 1: der Raum war schwarz - die Followers halten die Kerzen am Brennen (Auslegung)
            sweep(k(M['flame'], 40), [V((x + 0.002, y, top + h + 0.008)), V((x + 0.002, y, top + h + 0.02)), V((x + 0.003, y, top + h + 0.04))],
                  [0.004, 0.0065, 0.0005], 8)
    # Blechteller mit Nuessen und Saaten, Becher mit truebem Wasser
    cyl(k(M['tin'], 35), V((0.06, -0.08, top)), V((0.06, -0.08, top + 0.012)), 0.11, 18)
    for i in range(14):
        _nutshell(k, M, (0.06 + rng.uniform(-0.07, 0.07), -0.08 + rng.uniform(-0.07, 0.07), top + 0.012), rng)
    for i in range(60):
        s = rng.uniform(0.003, 0.005)
        blk(k(M['seed'], 0), (s * 1.6, s, s * 0.6), (0.06 + rng.uniform(-0.08, 0.08), -0.08 + rng.uniform(-0.08, 0.08), top + 0.014),
            rz=rng.uniform(0, 3))
    cyl(k(M['tin'], 35), V((0.08, 0.12, top)), V((0.08, 0.12, top + 0.09)), 0.04, 14, caps=True)
    cyl(k(M['water'], 30), V((0.08, 0.12, top + 0.06)), V((0.08, 0.12, top + 0.083)), 0.037, 14)
    # Glas mit blauen Federn
    cyl(k(M['glass'], 35), V((-0.12, 0.05, top)), V((-0.12, 0.05, top + 0.14)), 0.035, 14, caps=True)
    for i in range(7):
        a = rng.uniform(-0.35, 0.35)
        L = rng.uniform(0.22, 0.38)
        base = V((-0.12, 0.05, top + 0.02))
        d = V((math.sin(a) * 0.4, math.cos(a) * 0.35 * rng.uniform(-1, 1), 1.0)).normalized()
        sweep(k(M['feather'] if i % 3 else M['feather_d'], 35), [base, base + d * L * 0.5, base + d * L],
              [0.004, 0.016, 0.005], 6, profile=None)
    # gerahmtes Foto, an die Wand gelehnt
    fu, fv = V((0, 1, 0)), V((-0.32, 0, 1)).normalized()
    fo = V((-0.19, -0.02, top + 0.15))
    poly_on(k(M['frame']), [(-0.11, -0.14), (0.11, -0.14), (0.11, 0.14), (-0.11, 0.14)], fo - fu.cross(fv) * 0.004, fu, fv)
    blk(k(M['frame'], 0), (0.03, 0.24, 0.02), (-0.15, -0.02, top + 0.01), bevel=0.003)
    _photo(k, M, fo, fu, fv, 0.17, 0.22, rng)
    # Flugblaetter (gefaltet, gestapelt), Dose blauer Farbe mit Pinsel
    for i in range(6):
        a = rng.uniform(-0.3, 0.3)
        blk(k(M['paper'] if i % 2 else M['paper_old'], 0), (0.15, 0.1, 0.0025), (0.11, -0.25 + rng.uniform(-0.01, 0.01), top + 0.002 + i * 0.003), rz=a)
    cyl(k(M['tin'], 35), V((0.32, 0.36, 0.0)), V((0.32, 0.36, 0.16)), 0.075, 16)
    cyl(k(M['blue_d'], 30), V((0.32, 0.36, 0.13)), V((0.32, 0.36, 0.162)), 0.071, 16)
    for j in range(5):
        a = rng.uniform(0, TAU)
        sweep(k(M['blue'], 30), [V((0.32 + math.cos(a) * 0.076, 0.36 + math.sin(a) * 0.076, 0.16)),
                                 V((0.32 + math.cos(a) * 0.077, 0.36 + math.sin(a) * 0.077, rng.uniform(0.04, 0.12)))], [0.006, 0.004], 6)
    cyl(k(M['wood'], 35), V((0.3, 0.36, 0.15)), V((0.42, 0.42, 0.3)), 0.007, 6)
    blob(k(M['blue'], 40), V((0.36, 0.25, 0.0)), (0, 0, 1), 0.07, 0.004, rng, 13, 1.0, 0.5)
    # Kniekissen vor dem Tisch, mit zwei Mulden (Knie)
    blk(k(M['cushion'], 0), (0.32, 0.5, 0.07), (0.48, 0.0, 0.035), bevel=0.025)
    for y in (-0.11, 0.11):
        blob(k(M['cloth_d'], 40), V((0.48, y, 0.071)), (0, 0, 1), 0.06, 0.003, rng, 11, 1.0, 0.3)
    k.finish()


# --- jerry_mural ---------------------------------------------------------------------------------------------------------
def build_jerry_mural():
    rng = random.Random(7103)
    k = Kit()
    M = _mats()
    U, W = V((0, 1, 0)), V((0, 0, 1))
    o = V((0.0012, 0, 0))
    blue, blue_d = M['blue'], M['blue_d']
    # Strahlen hinter dem Vogel
    for i in range(15):
        a = math.pi * (0.08 + 0.84 * i / 14)
        r0, r1 = 0.45, 0.95 + 0.12 * rng.random()
        poly_on(k(blue), L1.stroke([(math.cos(a) * r0, 1.75 + math.sin(a) * r0 * 0.8), (math.cos(a) * r1, 1.75 + math.sin(a) * r1 * 0.8)], 0.03),
                o, U, W)
    # der Vogel: Koerper, Kopf mit Hakenschnabel, weit ausgebreitete Fluegel mit Schwungfedern, langer Schwanz
    body = [(0.0, 2.05), (0.1, 1.95), (0.13, 1.7), (0.09, 1.45), (0.04, 1.3), (-0.04, 1.3), (-0.09, 1.45), (-0.13, 1.7), (-0.1, 1.95)]
    poly_on(k(blue_d), body, o + V((0.0004, 0, 0)), U, W)
    head = [(0.11 * math.cos(a), 2.12 + 0.1 * math.sin(a)) for a in [i * TAU / 16 for i in range(16)]]
    poly_on(k(blue_d), head, o + V((0.0004, 0, 0)), U, W)
    poly_on(k(blue_d), [(0.06, 2.16), (0.17, 2.13), (0.19, 2.02), (0.13, 1.98), (0.1, 2.06)], o + V((0.0006, 0, 0)), U, W)
    poly_on(k(M['photo_yel']), [(0.04 + 0.025 * math.cos(a), 2.14 + 0.025 * math.sin(a)) for a in [i * TAU / 10 for i in range(10)]],
            o + V((0.0009, 0, 0)), U, W)  # gelbes Auge (Farbe aus einer anderen Dose)
    for g in (1, -1):
        wing = [(g * 0.08, 1.95), (g * 0.35, 2.15), (g * 0.62, 2.28), (g * 0.86, 2.3), (g * 0.8, 2.18), (g * 0.92, 2.1), (g * 0.82, 2.0),
                (g * 0.9, 1.9), (g * 0.76, 1.84), (g * 0.82, 1.73), (g * 0.62, 1.72), (g * 0.4, 1.68), (g * 0.12, 1.65)]
        poly_on(k(blue), wing if g > 0 else list(reversed(wing)), o + V((0.0003, 0, 0)), U, W)
    poly_on(k(blue), [(-0.06, 1.33), (0.06, 1.33), (0.1, 0.95), (0.0, 0.85), (-0.1, 0.95)], o + V((0.0003, 0, 0)), U, W)
    # kniende Gestalten mit erhobenen Armen (Strichmaennchen, ungelenk), zu beiden Seiten
    for i, y in enumerate((-0.95, -0.62, -0.3, 0.32, 0.64, 0.98)):
        s = 0.9 + 0.2 * rng.random()
        face = 1 if y < 0 else -1
        hx, hz = y + face * 0.04 * s, 0.72 * s
        poly_on(k(blue), [(hx + 0.045 * s * math.cos(a), hz + 0.05 * s * math.sin(a)) for a in [j * TAU / 10 for j in range(10)]], o, U, W)
        spine = [(hx - face * 0.01, hz - 0.05 * s), (y, 0.42 * s)]
        poly_on(k(blue), L1.stroke(spine, 0.022), o, U, W)
        poly_on(k(blue), L1.stroke([(y, 0.42 * s), (y + face * 0.12 * s, 0.38 * s), (y + face * 0.1 * s, 0.2 * s)], 0.02), o, U, W)  # Knie
        poly_on(k(blue), L1.stroke([(y, 0.42 * s), (y - face * 0.1 * s, 0.24 * s), (y - face * 0.02 * s, 0.2 * s)], 0.02), o, U, W)
        for arm in (0.12, 0.2):  # Arme zum Vogel erhoben
            poly_on(k(blue), L1.stroke([(hx - face * 0.005, hz - 0.08 * s), (hx + face * arm * s, hz + 0.12 * s), (hx + face * (arm + 0.05) * s, hz + 0.2 * s)], 0.016), o, U, W)
    # blaue Handabdruecke und Gekritzel, Farbe laeuft herab
    for i in range(7):
        cy, cz = rng.uniform(-1.1, 1.1), rng.uniform(0.95, 2.2)
        if abs(cy) < 0.95 and cz > 1.2:
            continue
        a = rng.uniform(-0.4, 0.4)
        palm = [(cy + 0.045 * math.cos(t) * math.cos(a) - 0.055 * math.sin(t) * math.sin(a), cz + 0.045 * math.cos(t) * math.sin(a) + 0.055 * math.sin(t) * math.cos(a))
                for t in [j * TAU / 12 for j in range(12)]]
        poly_on(k(blue_d), palm, o + V((0.0005, 0, 0)), U, W)
        for f in range(5):
            fa = a + (f - 2) * 0.32 + (0.9 if f == 0 else 0.0)
            poly_on(k(blue_d), L1.stroke([(cy + math.sin(-fa) * 0.05, cz + math.cos(fa) * 0.05), (cy + math.sin(-fa) * 0.11, cz + math.cos(fa) * 0.11)], 0.018),
                    o + V((0.0005, 0, 0)), U, W)
    _scrawl(k, blue_d, o, U, W, -1.15, 2.55, 2, 0.8, rng, lh=0.07, wd=0.012)
    _scrawl(k, blue_d, o, U, W, 0.35, 2.55, 2, 0.75, rng, lh=0.07, wd=0.012)
    for i in range(26):
        y = rng.uniform(-1.0, 1.0)
        zt = rng.uniform(0.85, 2.2)
        L = rng.uniform(0.05, 0.4)
        poly_on(k(blue), L1.stroke([(y, zt), (y + rng.uniform(-0.004, 0.004), zt - L)], rng.uniform(0.005, 0.01)), o + V((0.0002, 0, 0)), U, W)
    # zwei Fotos des Vogels in die Fluegel geklebt
    for (y, z) in ((-0.55, 2.42), (0.6, 2.42)):
        _photo(k, M, V((0.0, y, z)), U, W, 0.12, 0.16, rng, face=1.0 if y < 0 else -1.0, lift=0.002)
    k.finish()


# --- jerry_poster ---------------------------------------------------------------------------------------------------------
def build_jerry_poster():
    rng = random.Random(7104)
    k = Kit()
    M = _mats()
    U, W = V((0, 1, 0)), V((0, 0, 1))
    for (y, z, w, h) in ((-0.12, 1.62, 0.13, 0.17), (0.05, 1.68, 0.12, 0.16), (0.02, 1.47, 0.15, 0.11), (-0.16, 1.43, 0.11, 0.14)):
        _photo(k, M, V((0.0, y, z)), U, W, w, h, rng, face=1.0 if rng.random() < 0.5 else -1.0, lift=rng.uniform(0, 0.001))
    _arrow(k, M, rng, 1.2)
    _scrawl(k, M['blue_d'], V((0.0012, 0, 0)), U, W, -0.22, 1.87, 2, 0.4, rng, lh=0.05, wd=0.008)
    k.finish()


def build_jerry_poster__v1():
    rng = random.Random(7105)
    k = Kit()
    M = _mats()
    U, W = V((0, 1, 0)), V((0, 0, 1))
    _arrow(k, M, rng, 1.35, big=True)
    body, head, beak, eye = _bird_side(-0.25, 1.55, 0.32, 1.0)
    o = V((0.0012, 0, 0))
    poly_on(k(M['blue']), body, o, U, W)
    poly_on(k(M['blue']), head, o, U, W)
    poly_on(k(M['blue_d']), beak, o + V((0.0003, 0, 0)), U, W)
    _scrawl(k, M['blue_d'], o, U, W, -0.1, 1.82, 2, 0.45, rng, lh=0.06, wd=0.01)
    k.finish()


def _arrow(k, M, rng, z, big=False):
    """Gemalter Pfeil in +y (zum Raum), ungleich, Farbe laeuft."""
    U, W = V((0, 1, 0)), V((0, 0, 1))
    o = V((0.0012, 0, 0))
    s = 1.5 if big else 1.0
    shaft = [(-0.22 * s, z + rng.uniform(-0.01, 0.01)), (0.0, z + rng.uniform(-0.015, 0.015)), (0.2 * s, z)]
    poly_on(k(M['blue']), L1.stroke(shaft, 0.035 * s), o, U, W)
    poly_on(k(M['blue']), [(0.14 * s, z + 0.09 * s), (0.3 * s, z), (0.14 * s, z - 0.09 * s), (0.17 * s, z)], o + V((0.0002, 0, 0)), U, W)
    for i in range(6):
        y = rng.uniform(-0.2, 0.25) * s
        L = rng.uniform(0.04, 0.2)
        poly_on(k(M['blue']), L1.stroke([(y, z - 0.012), (y + rng.uniform(-0.003, 0.003), z - 0.012 - L)], rng.uniform(0.005, 0.009)), o, U, W)
