"""V13.3 Teil 4 Level 5: Wesen des Hotels ohne eigene Rolle (gameplay/hotel_l5.cpp) - Kolonien (Entity 89), Woodlins und
Watchers (nur Namen, Auslegung), der Gastgeber (Entity 18) mit seinem Buero, Fleisch fuer die Katze (Samantha, Entity 26).
Quellen: Wikidot level-5 Rev. 184, trimmed:entity-89 Rev. 35, entity-18 Rev. 54, entity-26 Rev. 22 (CC BY-SA 3.0).

Bewegliche Wesen bestehen aus starren Teilen, die das Spiel in einer Kette dreht (Ursprung jedes Teils = sein Gelenk; Glieder
liegen entlang +x). Masse gleichlautend in hotel_l5.hpp:
  ant_ball@0..3   ein Tierchen der Kolonie: flauschige, lichtschluckende Kugel (Halbmesser 2 cm, im Spiel 1-3 cm), unten eine
                  Saugscheibe mit Zahnkranz (beissen wie Blutegel - Auslegung der Seite)
  wl_body         Woodlin: Rumpf aus gedrechseltem Nussbaum (Ursprung Huefte, entlang +z), Kopf als geschnitzte Fratze mit
                  Astloch-Augen; im Holz klaffen Risse, darin bleiches, nasses Fleisch; wl_jaw Unterkiefer (Gelenk kWlJaw)
  wl_leg_a/b      Glieder wie Stuhlbeine (0,15 / 0,16 m), unten ein Klauen-und-Kugel-Fuss (Moebel der 1920er)
  wa_head         Watcher: langer, schmaler Kopf (0,42 m, Ursprung Hals), Saeule aus fuenf Augenhoehlen, wachsbleiche Haut
  wa_eye          ein Auge (Halbmesser 2 cm, Iris nach +x)
  wa_hand@0/@1    Hand um die Kante der Tueroeffnung (links/rechts): Ballen in der Laibung, vier lange Finger um die Ecke
  wa_arm_a/b, wa_claw  der lange Arm beim Hieb (0,55 / 0,62 m) und die gespreizte Hand
  e18_arm_a/b/c   Arm des Gastgebers im Anzugaermel (je 0,42 m, gestreckt im Spiel), e18_hand die Hand zum Handschlag
  e18_eye         Auge, das aus der Leinwand eines Bildnisses tritt (Lid aus Haut)
  e18_desk, e18_wingchair, e18_portrait  Buero (Kartenobjekte): Schreibtisch mit Bankerlampe, Vertrag, Telefon; Ohrensessel;
                  Bildnis eines Mannes im Anzug, das Gesicht abgekratzt
  meat_steak, meat_tin  Fundstuecke: rohes Steak auf Metzgerpapier und silbernem Teller; Dose Pressfleisch ohne Marke
Konventionen: Meter, z oben, Blick entlang +x; Bodenmodelle Ursprung am Boden; Wandmodelle: Wandflaeche x = 0.
"""
import math
import random

from mathutils import Matrix, Vector

from models.models_l1_phenomena import ellipsoid
from models.models_l1_sectors import Kit, blk, cyl, lathe, sweep
from models.models_l3 import mat

V = Vector
TAU = 2.0 * math.pi

# gleichlautend in hotel_l5.hpp
ANT_R = 0.02
WL_SHOULDER, WL_LEG_A, WL_LEG_B, WL_JAW = 0.22, 0.15, 0.16, (0.035, 0.0, 0.305)
WA_ARM_A, WA_ARM_B = 0.55, 0.62
E18_ARM = 0.42


def _rot_to(d):
    """Matrix: lokale z-Achse -> Richtung d."""
    d = V(d).normalized()
    return d.to_track_quat('Z', 'Y').to_matrix().to_4x4()


def _mats():
    return {
        # Kolonie: schwaerzer als alles im Hotel, stumpf (schluckt das Licht), die Saugscheibe nass dunkelrot
        # Runde 2: lasen sich grau (Stoffglanz) - fast reines Schwarz, stumpf wie Russ
        'fuzz': mat('kolonie_flaum', (0.003, 0.003, 0.0035), 'concrete', r=1.5),
        'fuzz_tip': mat('kolonie_flaumspitzen', (0.008, 0.008, 0.009), 'concrete', r=1.5),
        'sucker': mat('kolonie_saugscheibe', (0.30, 0.05, 0.06), 'plastic', r=0.25),
        'hook': mat('kolonie_zaehnchen', (0.75, 0.70, 0.60), 'ceramic', r=0.6),
        # Woodlin: polierter, abgegriffener Nussbaum; rohes Holz in den Rissen; Fleisch darin; nasse Astloecher
        'wood': mat('woodlin_nussbaum', (0.20, 0.11, 0.06), 'wood', r=0.55),
        'wood_l': mat('woodlin_nussbaum_hell', (0.34, 0.21, 0.12), 'wood', r=0.7),
        'raw': mat('woodlin_rohholz', (0.62, 0.48, 0.32), 'wood', r=1.3),
        'flesh': mat('woodlin_fleisch', (0.80, 0.66, 0.58), 'plastic', r=0.3),
        'sap': mat('woodlin_saft', (0.28, 0.06, 0.04), 'water', r=0.25),
        'hole': mat('woodlin_astloch', (0.01, 0.008, 0.006), 'plastic', r=0.2),
        'brass': mat('woodlin_messing', (0.62, 0.46, 0.22), 'metal', r=0.6, m=0.9),
        # Watcher: wachsbleiche, graue Haut, rote Raender um die Augen, Adern; nasse Augen; schwarzes, nasses Haar
        # Runde 2: weiss wie ein Teller - grau-gelbliche, wachsige Haut, dunkle Schatten
        'skin': mat('watcher_haut', (0.33, 0.30, 0.26), 'plastic', r=0.75),  # Runde 4: im Lampenlicht noch weiss
        'skin_d': mat('watcher_haut_schatten', (0.24, 0.21, 0.19), 'plastic', r=0.85),
        'rim': mat('watcher_lidrand', (0.62, 0.22, 0.20), 'plastic', r=0.3),
        'vein': mat('watcher_adern', (0.30, 0.32, 0.45), 'plastic', r=0.6),
        'sclera': mat('watcher_augapfel', (0.88, 0.84, 0.76), 'ceramic', r=0.2),
        'iris': mat('watcher_iris', (0.32, 0.36, 0.30), 'ceramic', r=0.2),
        'pupil': mat('watcher_pupille', (0.01, 0.01, 0.01), 'ceramic', r=0.2),
        'nail': mat('watcher_naegel', (0.52, 0.48, 0.40), 'ceramic', r=0.5),
        'hair': mat('watcher_haar', (0.02, 0.02, 0.02), 'water', r=0.4),
        'void': mat('watcher_schlund', (0.03, 0.01, 0.01), 'plastic', r=0.4),
        # Gastgeber: Nadelstreifen-Anzug (dunkles Anthrazit), Hemdmanschette, Manschettenknopf, wachsbleiche Hand
        'suit': mat('gastgeber_anzug', (0.06, 0.06, 0.07), 'fabric', r=1.0),
        'stripe': mat('gastgeber_nadelstreifen', (0.30, 0.30, 0.32), 'fabric', r=1.0),
        'shirt': mat('gastgeber_manschette', (0.88, 0.86, 0.80), 'fabric', r=0.9),
        'gold': mat('gastgeber_gold', (0.85, 0.66, 0.30), 'metal', r=0.4, m=1.0),
        'hand': mat('gastgeber_haut', (0.72, 0.63, 0.57), 'plastic', r=0.65),
        'hand_d': mat('gastgeber_haut_falten', (0.62, 0.50, 0.44), 'plastic', r=0.7),
        'eye_w': mat('bildnis_augapfel', (0.86, 0.80, 0.70), 'ceramic', r=0.15),
        'eye_i': mat('bildnis_iris', (0.25, 0.16, 0.09), 'ceramic', r=0.15),
        'lid': mat('bildnis_lid', (0.70, 0.46, 0.38), 'plastic', r=0.35),
        # Buero
        'desk': mat('buero_mahagoni', (0.30, 0.12, 0.07), 'wood', r=0.45),
        'desk_d': mat('buero_mahagoni_dunkel', (0.16, 0.06, 0.04), 'wood', r=0.6),
        'leather_g': mat('buero_leder_gruen', (0.08, 0.20, 0.12), 'fabric', r=0.6),
        'leather_r': mat('buero_leder_ochsenblut', (0.26, 0.05, 0.04), 'fabric', r=0.5),
        'brass_b': mat('buero_messing', (0.80, 0.62, 0.32), 'metal', r=0.45, m=0.95),
        'shade': mat('buero_lampenschirm', (0.10, 0.45, 0.20), 'glass', r=0.15),
        'bulb': mat('buero_gluehbirne', (1.0, 0.92, 0.70), 'glow', glow=0.8),
        'paper': mat('buero_papier', (0.88, 0.85, 0.76), 'cardboard', r=1.2),
        'ink': mat('buero_tinte', (0.03, 0.03, 0.05), 'plastic', r=0.4),
        'seal': mat('buero_siegel', (0.55, 0.04, 0.04), 'plastic', r=0.35),
        'bakelite': mat('buero_bakelit', (0.025, 0.022, 0.02), 'plastic', r=0.3),
        'gilt': mat('buero_rahmengold', (0.78, 0.60, 0.28), 'metal', r=0.5, m=0.9),
        'canvas': mat('buero_leinwand', (0.62, 0.56, 0.44), 'cardboard', r=1.3),
        'paint_d': mat('buero_firnis_dunkel', (0.07, 0.06, 0.05), 'paint', r=0.5),
        'paint_suit': mat('buero_gemalt_anzug', (0.05, 0.05, 0.06), 'paint', r=0.5),
        'paint_shirt': mat('buero_gemalt_hemd', (0.80, 0.78, 0.70), 'paint', r=0.5),
        'paint_skin': mat('buero_gemalt_haut', (0.62, 0.46, 0.34), 'paint', r=0.5),
        'paint_red': mat('buero_gemalt_krawatte', (0.42, 0.06, 0.05), 'paint', r=0.5),
        # Fleisch
        'meat': mat('fleisch_roh', (0.60, 0.10, 0.10), 'plastic', r=0.28),
        'meat_d': mat('fleisch_roh_dunkel', (0.34, 0.04, 0.05), 'plastic', r=0.25),
        'fat': mat('fleisch_fett', (0.92, 0.86, 0.74), 'plastic', r=0.35),
        'bpaper': mat('fleisch_metzgerpapier', (0.88, 0.80, 0.66), 'cardboard', r=1.2),
        'silver': mat('fleisch_silberteller', (0.80, 0.80, 0.82), 'metal', r=0.35, m=1.0),
        'tin': mat('fleisch_dose', (0.66, 0.66, 0.68), 'metal', r=0.6, m=0.9),
        'label': mat('fleisch_etikett', (0.72, 0.42, 0.28), 'cardboard', r=1.1),
        'label_w': mat('fleisch_etikett_hell', (0.90, 0.84, 0.70), 'cardboard', r=1.1),
        'parsley': mat('fleisch_petersilie', (0.15, 0.35, 0.10), 'plastic', r=0.8),
    }


# --- Kolonie (Entity 89) -----------------------------------------------------------------------------------------------
def _ant(v):
    rng = random.Random(8900 + v)
    K = Kit()
    M = _mats()
    sq = (1.0, 0.92, 0.85, 1.05)[v]
    core = ANT_R * 0.82
    ellipsoid(K(M['fuzz'], 60), V((0, 0, 0)), V((0, 0, 1)), core * sq, core, 10, 7)
    # Flaum: dichte, weiche Haarbuschel rundum, oben laenger (wirkt groesser, als es ist)
    n = 90
    for i in range(n):
        z = 1.0 - 2.0 * (i + 0.5) / n
        a = i * 2.39996 + rng.uniform(-0.2, 0.2)
        r = math.sqrt(max(0.0, 1.0 - z * z))
        d = V((r * math.cos(a), r * math.sin(a), z * sq)).normalized()
        if z < -0.75:
            continue  # unten die Saugscheibe
        p0 = d * core * 0.9
        L = ANT_R * rng.uniform(0.28, 0.5) * (1.2 if z > 0.3 else 1.0)
        bend = V((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))) * 0.35
        p1 = p0 + (d + bend).normalized() * L
        sweep(K(M['fuzz'] if rng.random() < 0.8 else M['fuzz_tip'], 30), [p0, p0.lerp(p1, 0.5) + bend * L * 0.2, p1],
              [ANT_R * 0.09, ANT_R * 0.06, ANT_R * 0.012], 3, caps=False)
    # unten: nasse Saugscheibe mit einem Kranz winziger Haken
    c = V((0, 0, -core * sq * 0.88))
    lathe(K(M['sucker'], 40), [(0.0, -0.0012), (core * 0.38, -0.0006), (core * 0.42, 0.0015)], 12, Matrix.Translation(c))
    for k in range(10):
        a = TAU * k / 10
        p = c + V((math.cos(a), math.sin(a), 0)) * core * 0.3
        sweep(K(M['hook'], 30), [p, p + V((0, 0, -0.0022)) - V((math.cos(a), math.sin(a), 0)) * 0.0008], [0.0006, 0.0001], 3)
    K.finish()


def build_ant_ball():
    _ant(0)


def build_ant_ball__v1():
    _ant(1)


def build_ant_ball__v2():
    _ant(2)


def build_ant_ball__v3():
    _ant(3)


# --- Woodlin (nur Name; Auslegung: Wesen aus gedrechseltem Moebelholz) ---------------------------------------------------
def _turned(bm, prof, segs=12, M=None):
    lathe(bm, prof, segs, M)


def build_wl_body():
    """Rumpf (Ursprung Huefte, aufrecht entlang +z): gedrechselte Spindel wie ein Treppendocke, Schulterring, darueber der
    geschnitzte Kopf: Fratze mit Astloch-Augen (nass, tief), Brauenwulst, Nasengrat; ein Riss klafft vom Scheitel bis in die
    Brust - darin bleiches Fleisch und dunkler Saft."""
    rng = random.Random(4501)
    K = Kit()
    M = _mats()
    # Spindel: Becken (Kugel), Taille, Bauch (Vase), Hals, Schulterring
    prof = [(0.0, -0.03), (0.035, -0.022), (0.045, 0.0), (0.04, 0.03), (0.022, 0.05), (0.026, 0.07), (0.044, 0.11), (0.05, 0.14),
            (0.04, 0.17), (0.024, 0.19), (0.03, 0.205), (0.052, 0.215), (0.052, 0.232), (0.03, 0.24), (0.022, 0.255)]
    _turned(K(M['wood'], 35), prof, 14)
    for z in (0.055, 0.205, 0.235):  # Ringe (heller, abgegriffen)
        lathe(K(M['wood_l'], 35), [(0.027 if z < 0.1 else 0.05, z - 0.004), (0.031 if z < 0.1 else 0.055, z), (0.027 if z < 0.1 else 0.05, z + 0.004)], 14)
    K.finish()
    # Kopf: Ei mit Gesicht nach +x (Runde 5: 1,4-fach um den Hals - die geschnitzte Fratze muss man sehen)
    K = Kit()
    ellipsoid(K(M['wood'], 40), V((0.0, 0.0, 0.315)), V((0, 0, 1)), 0.06, 0.046, 14, 9)
    ellipsoid(K(M['wood'], 40), V((0.03, 0.0, 0.335)), V((0, 1, 0)), 0.042, 0.012, 10, 5)  # Brauenwulst
    sweep(K(M['wood'], 40), [V((0.042, 0, 0.335)), V((0.05, 0, 0.312)), V((0.052, 0, 0.298))], [0.008, 0.009, 0.006], 6)  # Nase
    for s in (1, -1):  # Astloecher als Augen: dunkler Ring, nass glaenzendes Schwarz darin
        c = V((0.04, s * 0.02, 0.32))
        lathe(K(M['wood_l'], 30), [(0.009, -0.002), (0.012, 0.0), (0.009, 0.003)], 10, Matrix.Translation(c) @ _rot_to((1, s * 0.3, 0)))
        ellipsoid(K(M['hole'], 30), c + V((0.002, 0, 0)), V((1, s * 0.3, 0)), 0.006, 0.0085, 8, 4)
    # Wangen als geschnitzte Wuelste, Mundwinkel (der Kiefer ist eigen)
    for s in (1, -1):
        ellipsoid(K(M['wood'], 40), V((0.03, s * 0.03, 0.3)), V((1, 0, -0.3)), 0.02, 0.012, 8, 5)
    # Riss: vom Scheitel ueber das Gesicht in die Brust - rohes Holz an den Raendern, Fleisch darin, Saft laeuft
    path = [V((-0.01, 0.006, 0.375)), V((0.03, 0.004, 0.36)), V((0.045, -0.008, 0.34)), V((0.048, 0.006, 0.31)), V((0.04, -0.004, 0.285)),
            V((0.035, 0.006, 0.262)), V((0.03, 0.0, 0.25))]
    sweep(K(M['raw'], 30), path, [0.003, 0.005, 0.006, 0.006, 0.006, 0.005, 0.003], 6)
    sweep(K(M['flesh'], 40), [p + V((0.002, 0, 0)) for p in path[1:-1]], [0.0035, 0.0045, 0.004, 0.0045, 0.004], 6)
    for i in range(4):  # Saft
        p = path[2 + i] + V((0.004, rng.uniform(-0.004, 0.004), 0))
        sweep(K(M['sap'], 40), [p, p + V((0.002, 0, -rng.uniform(0.01, 0.03)))], [0.0018, 0.0008], 5)
    # Splitter an den Rissraendern
    for i in range(10):
        p = path[rng.randrange(1, len(path) - 1)]
        d = V((0.4, rng.choice((-1, 1)), rng.uniform(-0.3, 0.3))).normalized()
        sweep(K(M['raw'], 20), [p, p + d * rng.uniform(0.006, 0.012)], [0.0015, 0.0003], 3)
    # Messingnagel im Scheitel (wie ein Moebelbeschlag)
    lathe(K(M['brass'], 30), [(0.0, 0.372), (0.008, 0.372), (0.006, 0.378), (0.0, 0.38)], 10)
    K.finish(Matrix.Translation((0, 0, 0.3)) @ Matrix.Diagonal((1.4, 1.4, 1.4, 1.0)) @ Matrix.Translation((0, 0, -0.3)))


def build_wl_jaw():
    """Unterkiefer (Ursprung Gelenk, entlang +x): grob geschnitzt, Zahnreihe aus Holzsplittern, innen Fleisch."""
    rng = random.Random(4502)
    K = Kit()
    M = _mats()
    ellipsoid(K(M['wood'], 40), V((0.014, 0, -0.012)), V((1, 0, 0)), 0.026, 0.028, 10, 6)
    ellipsoid(K(M['flesh'], 40), V((0.016, 0, -0.004)), V((1, 0, 0)), 0.02, 0.018, 8, 5)
    for k in range(9):
        a = -1.1 + 2.2 * k / 8
        p = V((0.014 + 0.018 * math.cos(a), 0.022 * math.sin(a), -0.002))
        sweep(K(M['raw'], 20), [p, p + V((rng.uniform(-0.002, 0.002), 0, rng.uniform(0.006, 0.011)))], [0.0022, 0.0003], 4)
    K.finish()


def build_wl_leg_a():
    """Oberes Glied (entlang +x, 0,15 m): gedrechselt mit Ring und Kugelgelenk."""
    K = Kit()
    M = _mats()
    prof = [(0.0, 0.0), (0.016, 0.004), (0.017, 0.014), (0.011, 0.024), (0.013, 0.05), (0.016, 0.075), (0.012, 0.11), (0.009, 0.135), (0.012, 0.145), (0.0, 0.152)]
    lathe(K(M['wood'], 35), prof, 10, Matrix.Rotation(math.pi / 2, 4, 'Y'))
    lathe(K(M['wood_l'], 35), [(0.014, 0.068), (0.018, 0.075), (0.014, 0.082)], 10, Matrix.Rotation(math.pi / 2, 4, 'Y'))
    K.finish()


def build_wl_leg_b():
    """Unteres Glied (entlang +x, 0,16 m): schlanker, am Ende ein Klauen-und-Kugel-Fuss - die Klaue greift eine Kugel."""
    K = Kit()
    M = _mats()
    prof = [(0.0, 0.0), (0.012, 0.004), (0.011, 0.03), (0.008, 0.08), (0.007, 0.12), (0.01, 0.138), (0.0, 0.142)]
    lathe(K(M['wood'], 35), prof, 10, Matrix.Rotation(math.pi / 2, 4, 'Y'))
    c = V((0.152, 0, 0))
    ellipsoid(K(M['wood_l'], 40), c, V((1, 0, 0)), 0.013, 0.013, 10, 6)  # Kugel
    for k in range(4):  # Klauen umgreifen die Kugel
        a = TAU * k / 4 + 0.4
        d = V((0, math.cos(a), math.sin(a)))
        pts = [V((0.136, 0, 0)) + d * 0.008, c + d * 0.016 + V((-0.004, 0, 0)), c + d * 0.012 + V((0.009, 0, 0)), c + d * 0.005 + V((0.014, 0, 0))]
        sweep(K(M['wood'], 35), pts, [0.0045, 0.004, 0.003, 0.0012], 5)
    K.finish()


# --- Watcher (nur Name; Level 43: feindselig - Auslegung) ---------------------------------------------------------------------
# Runde 3: aus Kugeln und Rohren las er sich wie ein Spielzeug (Pille mit Ringen, Fischgraeten) - Kopf, Haende, Leib und Arme
# jetzt als Distanzfeld (wie die Wesen mit Skelett): eingefallene Wangen, tiefe Hoehlen, Falten und Poren in der Haut.
WA_EYES = [(0.085, 0.0, 0.36), (0.098, 0.008, 0.30), (0.104, -0.006, 0.235), (0.1, 0.01, 0.17), (0.088, -0.004, 0.11)]
WA_EYE_S = [0.7, 1.0, 1.25, 0.85, 0.6]


def _sdf():
    import sys as _s
    import os as _o
    p = _o.path.join(_o.path.dirname(_o.path.dirname(_o.path.abspath(__file__))), 'creatures')
    if p not in _s.path:
        _s.path.insert(0, p)
    import anat
    import sdf
    return anat, sdf


def _sdf_obj(name, parts, h, disp, material, target):
    """Distanzfeld -> Netz (Blender-Objekt) mit weichen Normalen, ausgeduennt auf etwa target Dreiecke beim Export."""
    import bmesh
    import bpy
    import common
    _, sdf = _sdf()
    F, lo = sdf.evaluate(parts, h, disp)
    Vv, Tt = sdf.polygonize(F, lo, h, 0.0)
    bm = bmesh.new()
    vs = [bm.verts.new((float(p[0]), float(p[1]), float(p[2]))) for p in Vv]
    for t in Tt:
        try:
            bm.faces.new((vs[int(t[0])], vs[int(t[1])], vs[int(t[2])]))
        except ValueError:
            pass
    ob = common.obj_from_bmesh(name, bm, material)
    for p in ob.data.polygons:
        p.use_smooth = True
    if target and len(Tt) > target:
        m = ob.modifiers.new('duenn', 'DECIMATE')
        m.ratio = target / len(Tt)
    return ob


def _skin_disp(scale=1.0, seed=3810):
    _, sdf = _sdf()
    import numpy as np

    def d(Q):  # Falten quer, Poren, feine Runzeln
        folds = sdf.ridged(Q * np.array([0.6, 0.6, 1.8]), 55, 2, seed=seed)
        return scale * (-0.0011 * (folds - 0.5) - 0.0005 * sdf.fbm(Q, 260, 2, seed=seed + 1))
    return d


def build_wa_head():
    """Kopf (Ursprung Hals, entlang +z, 0,42 m): ein ausgezogenes, ausgezehrtes Menschengesicht - Stirn, Brauenwulst,
    Wangenknochen, eingefallene Wangen, langer Kiefer, ein lippenloser Mund mit gelben Zaehnen -, nur liegen statt Nase und
    zwei Augen fuenf Hoehlen untereinander in der Mitte des Gesichts (die Augen sind eigene Teile). Runde 4: als Pille ohne
    Gesicht las er sich wie ein Spielzeug."""
    A, _ = _sdf()
    rng = random.Random(3801)
    M = _mats()
    X = (1, 0, 0)
    Z = (0, 0, 1)
    parts = [A.ell(0, (-0.01, 0, 0.3), Z, (0.13, 0.075, 0.085), up=X, k=0.0),             # Schaedeldach
             A.ell(0, (0.03, 0, 0.19), Z, (0.2, 0.058, 0.068), up=X, k=0.04),              # langes Gesicht
             A.ell(0, (0.085, 0, 0.375), (0, 1, 0), (0.06, 0.02, 0.016), k=0.02),          # Brauenwulst
             A.ell(0, (0.04, 0, 0.045), Z, (0.055, 0.052, 0.058), up=X, k=0.03),            # Kiefer
             A.ell(0, (0.075, 0, 0.0), Z, (0.03, 0.03, 0.03), up=X, k=0.02),                # Kinn
             A.cone(0, (-0.01, 0, -0.08), (0.0, 0, 0.06), 0.042, 0.05, k=0.03)]            # Hals
    for sd in (1, -1):
        parts.append(A.ell(0, (0.07, sd * 0.052, 0.215), X, (0.028, 0.018, 0.024), k=0.015))   # Wangenknochen
        parts.append(A.ell(0, (0.06, sd * 0.058, 0.12), X, (0.04, 0.022, 0.05), op='sub', k=0.02))  # eingefallene Wange
        parts.append(A.ell(0, (0.0, sd * 0.07, 0.31), X, (0.05, 0.02, 0.05), op='sub', k=0.02))   # Schlaefe
        parts.append(A.ell(0, (-0.03, sd * 0.072, 0.24), (0, sd, 0.3), (0.03, 0.012, 0.022), k=0.006))  # Ohr, angelegt
    for (x, y, z), s in zip(WA_EYES, WA_EYE_S):
        parts.append(A.ell(0, (x + 0.004, y, z), X, (0.03 * s, 0.024 * s, 0.021 * s), op='sub', k=0.008))
    parts.append(A.ell(0, (0.1, 0, 0.055), (0, 1, 0), (0.05, 0.03, 0.006), op='sub', k=0.004))  # Mund, breit, lippenlos
    _sdf_obj('kopf', parts, 0.0021, _skin_disp(), M['skin'], 10000)


def build_wa_face():
    """Was im Gesicht nicht Haut ist (Runde 5: die Haut bekommt im Spiel das feste Hautmaterial der Wesen): wunde, nasse
    Raender und Schwaerze in den Hoehlen, gelbe, schiefe Zaehne im lippenlosen Mund, nasse Straehnen (Ursprung wie wa_head)."""
    rng = random.Random(3801)
    M = _mats()
    K = Kit()
    for (x, y, z), s in zip(WA_EYES, WA_EYE_S):  # nasse, wunde Raender in den Hoehlen
        c = V((x - 0.006, y, z))
        lathe(K(M['rim'], 30), [(0.019 * s, -0.004), (0.022 * s, 0.0), (0.02 * s, 0.006)], 16, Matrix.Translation(c) @ _rot_to((1, 0, 0)))
        ellipsoid(K(M['void'], 30), c - V((0.008, 0, 0)), V((1, 0, 0)), 0.008 * s, 0.019 * s, 10, 5)
    ellipsoid(K(M['void'], 30), V((0.085, 0, 0.055)), V((0, 1, 0)), 0.045, 0.004, 10, 4)  # Rachen hinter den Zaehnen
    for k in range(14):  # Zaehne, gelb, schief, zu viele
        y = -0.042 + 0.084 * k / 13
        for up_ in (1, -1):
            p = V((0.094 - 0.012 * (y / 0.042) ** 2, y, 0.055 + up_ * 0.0045))
            sweep(K(M['nail'], 30), [p, p + V((rng.uniform(0.0, 0.003), rng.uniform(-0.002, 0.002), -up_ * rng.uniform(0.006, 0.011)))], [0.0035, 0.0012], 4)
    for i in range(18):  # duenne, nasse Straehnen vom Scheitel
        a = rng.uniform(0.6, TAU - 0.6)
        p0 = V((0.05 * math.cos(a) - 0.02, 0.05 * math.sin(a), 0.42))
        pts = [p0, p0 + V((-0.03, 0.012 * math.sin(a), -0.05)), p0 + V((-0.05, 0.02 * math.sin(a), -rng.uniform(0.12, 0.3)))]
        sweep(K(M['hair'], 30), pts, [0.003, 0.002, 0.0005], 4)
    K.finish()


def build_wa_eye():
    """Auge (Halbmesser 2 cm, Iris nach +x): nasser, gelblicher Augapfel mit roten Aederchen, graugruene Iris, grosse Pupille."""
    rng = random.Random(3802)
    K = Kit()
    M = _mats()
    ellipsoid(K(M['sclera'], 60), V((0, 0, 0)), V((1, 0, 0)), 0.02, 0.02, 14, 10)
    lathe(K(M['iris'], 60), [(0.0, 0.0198), (0.0095, 0.0175), (0.0105, 0.0168)], 16, _rot_to((1, 0, 0)))
    lathe(K(M['pupil'], 60), [(0.0, 0.0201), (0.0058, 0.0186)], 14, _rot_to((1, 0, 0)))
    for i in range(12):
        a = rng.uniform(0, TAU)
        p0 = V((-0.006, 0.019 * math.cos(a), 0.019 * math.sin(a)))
        p1 = V((0.011, 0.017 * math.cos(a + 0.2), 0.017 * math.sin(a + 0.2)))
        sweep(K(M['rim'], 40), [p0, p0.lerp(p1, 0.5) * 1.03, p1], [0.0005, 0.0004, 0.0002], 3, caps=False)
    K.finish()


def _wa_hand(side):
    """Hand um die Kante (Ursprung Kante der Oeffnung auf der Gangwand; x in den Gang, y = side: Richtung der Wand neben der
    Oeffnung): der Ballen liegt in der Laibung, vier lange, knotige Finger greifen um die Ecke und liegen flach auf der
    Gangwand; gelbe, rissige Naegel."""
    A, _ = _sdf()
    M = _mats()
    s = 1.0 if side > 0 else -1.0
    X = (1, 0, 0)
    parts = [A.ell(0, (-0.08, -s * 0.016, 0.0), X, (0.075, 0.016, 0.042), k=0.0),
             A.cone(0, (-0.34, -s * 0.03, -0.02), (-0.1, -s * 0.02, 0.0), 0.026, 0.03, k=0.02)]
    tips = []
    for f in range(4):
        z = 0.048 - 0.032 * f
        L = (0.07, 0.075, 0.07, 0.055)[f] * 1.45
        pts = [(-0.03, -s * 0.013, z), (0.006, -s * 0.004, z), (0.014, s * 0.02, z + 0.002), (0.014, s * (0.02 + L * 0.55), z), (0.013, s * (0.02 + L), z - 0.003)]
        r = [0.0105, 0.0098, 0.009, 0.0078, 0.0058]
        parts += A.chain(lambda i: 0, pts, r, k=0.004)
        for k in (1, 2, 3):
            parts.append(A.ell(0, (pts[k][0] + 0.003, pts[k][1], pts[k][2]), (0, 1, 0), (0.0085, 0.0115, 0.0105), k=0.004))
        tips.append(V(pts[-1]))
    parts += A.chain(lambda i: 0, [(-0.09, -s * 0.03, -0.035), (-0.04, -s * 0.03, -0.055), (-0.01, -s * 0.021, -0.062)], [0.011, 0.009, 0.007], k=0.006)
    _sdf_obj('hand', parts, 0.0012, _skin_disp(0.5, 3811), M['skin'], 7000)
    K = Kit()
    for tip in tips:
        sweep(K(M['nail'], 30), [tip + V((0.004, -s * 0.008, 0)), tip + V((0.005, s * 0.012, -0.002))], [0.0055, 0.0018], 5)
    K.finish()


def build_wa_hand():
    _wa_hand(1)


def build_wa_hand__v1():
    _wa_hand(-1)


def build_wa_body():
    """Leib (Ursprung Halsansatz, haengt nach -z): ausgezehrt, viel zu lang - Rippen zeichnen sich unter der Haut ab,
    eingefallener Bauch, spitze Schultern; dunkler, damit er im Dunkel der Oeffnung nur als Umriss steht."""
    A, _ = _sdf()
    M = _mats()
    Z = (0, 0, 1)
    parts = [A.cone(0, (0.0, 0, 0.06), (0.0, 0, -0.08), 0.042, 0.05, k=0.0),
             A.ell(0, (-0.02, 0, -0.3), Z, (0.25, 0.115, 0.085), up=(1, 0, 0), k=0.04),
             A.ell(0, (-0.05, 0, -0.62), Z, (0.18, 0.07, 0.055), up=(1, 0, 0), k=0.06),
             A.ell(0, (-0.05, 0, -0.86), (0, 1, 0), (0.15, 0.08, 0.08), k=0.05)]
    for s in (1, -1):
        parts.append(A.cone(0, (0.02, s * 0.02, -0.07), (0.0, s * 0.2, -0.1), 0.014, 0.022, k=0.02))  # Schluesselbein
        parts.append(A.ell(0, (-0.03, s * 0.21, -0.13), Z, (0.05, 0.045, 0.045), k=0.03))  # Schulter
        for k in range(8):
            z = -0.15 - 0.052 * k
            pts = [(0.092 - 0.004 * k, s * 0.01, z), (0.075, s * 0.08, z - 0.02), (0.0, s * 0.112, z - 0.035), (-0.07, s * 0.075, z - 0.04)]
            parts += A.chain(lambda i: 0, pts, [0.0085, 0.0095, 0.009, 0.007], k=0.012)
    _sdf_obj('leib', parts, 0.004, _skin_disp(1.3, 3812), M['skin_d'], 8000)


def _limb(name, L, r0, r1, seed):
    A, _ = _sdf()
    M = _mats()
    rng = random.Random(seed)
    pts = [(L * t, 0.006 * math.sin(t * 7.0), 0.004 * math.sin(t * 5.0)) for t in (0.0, 0.25, 0.5, 0.75, 1.0)]
    parts = A.chain(lambda i: 0, pts, [r0, r0 * 1.12, (r0 + r1) / 2, r1 * 1.05, r1], k=0.01)
    parts.append(A.ell(0, (L, 0, 0), (1, 0, 0), (r1 * 1.4, r1 * 1.2, r1 * 1.25), k=0.01))  # Gelenk, knotig
    for i in range(3):  # Sehnen
        y = (i - 1) * r0 * 0.5
        parts.append(A.cone(0, (L * 0.15, y, r0 * 0.75), (L * 0.85, y * 0.8, r1 * 0.8), 0.004, 0.003, k=0.006))
    _sdf_obj(name, parts, 0.0025, _skin_disp(0.8, seed), M['skin'], 4000)


def build_wa_arm_a():
    _limb('oberarm', WA_ARM_A, 0.03, 0.022, 3804)


def build_wa_arm_b():
    _limb('unterarm', WA_ARM_B, 0.022, 0.016, 3805)


def build_wa_claw():
    """Gespreizte Hand (Ursprung Handgelenk, entlang +x): fuenf viel zu lange, knotige Finger, gekruemmt, gelbe Naegel."""
    A, _ = _sdf()
    M = _mats()
    parts = [A.ell(0, (0.05, 0, 0), (1, 0, 0), (0.058, 0.032, 0.016), k=0.0)]
    tips = []
    for f in range(5):
        a = -0.8 + 1.6 * f / 4
        d = (math.cos(a), math.sin(a), 0)
        L = 0.18 if f not in (0, 4) else 0.13
        p0 = (0.08 + d[0] * 0.015, d[1] * 0.015, 0)
        pts = [p0, (p0[0] + d[0] * L * 0.4, p0[1] + d[1] * L * 0.4, -0.01), (p0[0] + d[0] * L * 0.75, p0[1] + d[1] * L * 0.75, -0.03),
               (p0[0] + d[0] * L, p0[1] + d[1] * L, -0.06)]
        parts += A.chain(lambda i: 0, pts, [0.0085, 0.0078, 0.0068, 0.0048], k=0.004)
        tips.append((V(pts[-1]), V(d)))
    _sdf_obj('klaue', parts, 0.0013, _skin_disp(0.5, 3806), M['skin'], 6000)
    K = Kit()
    for tip, d in tips:
        sweep(K(M['nail'], 30), [tip, tip + d * 0.009 + V((0, 0, -0.013))], [0.0045, 0.0009], 4)
    K.finish()


# --- der Gastgeber (Entity 18) ----------------------------------------------------------------------------------------------
def _sleeve(K, M, rng, cuff=False):
    L = E18_ARM
    pts = [V((L * t, 0.0, 0.0)) for t in (0.0, 0.3, 0.6, 1.0)]
    sweep(K(M['suit'], 30), pts, [0.05, 0.047, 0.044, 0.042], 12)
    ellipsoid(K(M['suit'], 30), V((0.0, 0, 0)), V((1, 0, 0)), 0.06, 0.055, 10, 6)  # Runde 2: Ellbogen (kein Rohr)
    for k in range(7):  # Nadelstreifen
        a = TAU * k / 7
        o = V((0, math.cos(a), math.sin(a)))
        sweep(K(M['stripe'], 20), [p + o * 0.0455 for p in pts], [0.0012] * 4, 3, caps=False)
    for t in (0.35, 0.55, 0.75):  # Falten
        lathe(K(M['suit'], 30), [(0.046, -0.006), (0.049, 0.0), (0.046, 0.006)], 12, Matrix.Translation((L * t, 0, 0)) @ _rot_to((1, 0, rng.uniform(-0.2, 0.2))))
    if cuff:
        lathe(K(M['shirt'], 30), [(0.0, L - 0.002), (0.033, L - 0.002), (0.034, L + 0.03), (0.0, L + 0.03)], 12, _rot_to((1, 0, 0)))
        lathe(K(M['gold'], 30), [(0.0, 0.0), (0.006, 0.0), (0.005, 0.003), (0.0, 0.004)], 8,
              Matrix.Translation((L + 0.015, 0.0, 0.034)) @ _rot_to((0, 0, 1)))


def build_e18_arm_a():
    rng = random.Random(1801)
    K = Kit()
    _sleeve(K, _mats(), rng)
    K.finish()


def build_e18_arm_b():
    rng = random.Random(1802)
    K = Kit()
    _sleeve(K, _mats(), rng)
    K.finish()


def build_e18_arm_c():
    rng = random.Random(1803)
    K = Kit()
    _sleeve(K, _mats(), rng, cuff=True)
    K.finish()


def build_e18_hand():
    """Hand zum Handschlag (Ursprung Handgelenk, entlang +x; Daumen oben, Handflaeche zur Seite): gepflegt, wachsbleich, die
    Finger um ein Glied zu lang, Knoechel; ein Siegelring (Runde 3: als Distanzfeld - vorher Plastik)."""
    A, _ = _sdf()
    M = _mats()
    parts = [A.ell(0, (0.055, 0, 0), (1, 0, 0), (0.06, 0.021, 0.04), k=0.0),
             A.cone(0, (-0.03, 0, 0), (0.02, 0, 0), 0.028, 0.032, k=0.012)]
    tips = []
    for f in range(4):
        z = 0.026 - 0.0175 * f
        L = (0.11, 0.12, 0.115, 0.09)[f] * 1.18
        pts = [(0.105, 0, z), (0.105 + L * 0.35, -0.004, z - 0.002), (0.105 + L * 0.7, -0.012, z - 0.004), (0.105 + L, -0.022, z - 0.006)]
        parts += A.chain(lambda i: 0, pts, [0.0088, 0.0082, 0.0072, 0.0052], k=0.004)
        for k in (1, 2):
            parts.append(A.ell(0, pts[k], (1, 0, 0), (0.006, 0.009, 0.009), k=0.004))
        tips.append(V(pts[-1]))
    parts += A.chain(lambda i: 0, [(0.04, 0.012, 0.03), (0.08, 0.016, 0.05), (0.112, 0.01, 0.063)], [0.011, 0.009, 0.0068], k=0.006)
    _sdf_obj('hand', parts, 0.0011, _skin_disp(0.35, 1806), M['hand'], 7000)
    K = Kit()
    for tip in tips:
        sweep(K(M['nail'], 30), [tip + V((-0.006, 0.0, 0.004)), tip + V((0.001, -0.002, 0.005))], [0.004, 0.003], 4)
    lathe(K(M['gold'], 30), [(0.0098, -0.003), (0.0105, 0.0), (0.0098, 0.003)], 10, Matrix.Translation((0.152, -0.006, -0.027)) @ _rot_to((1, 0, 0)))
    K.finish()


def build_e18_eye():
    """Auge im Bildnis (Halbmesser 9 mm, Iris nach +x): tritt aus der Leinwand, nasses Lid aus Haut darum."""
    K = Kit()
    M = _mats()
    ellipsoid(K(M['eye_w'], 60), V((0, 0, 0)), V((1, 0, 0)), 0.009, 0.009, 12, 8)
    lathe(K(M['eye_i'], 60), [(0.0, 0.0089), (0.0042, 0.0079), (0.0046, 0.0075)], 12, _rot_to((1, 0, 0)))
    lathe(K(M['paint_d'], 60), [(0.0, 0.009), (0.0022, 0.0084)], 10, _rot_to((1, 0, 0)))
    lathe(K(M['lid'], 40), [(0.0105, -0.004), (0.011, 0.0), (0.0098, 0.0035), (0.0085, 0.0045)], 14, _rot_to((1, 0, 0)))
    K.finish()


def build_e18_desk():
    """Schreibtisch (Bodenmodell, Ursprung Mitte unten, Besucherseite +x): Mahagoni mit zwei Schubladenkaesten, gruene
    Lederauflage, Bankerlampe mit gruenem Glasschirm (links), offener Vertrag mit rotem Siegel und Fuellfeder, Tintenfass,
    Kerzenstaender-Telefon, ein Kontobuch."""
    rng = random.Random(1804)
    K = Kit()
    M = _mats()
    W, D, H = 1.5, 0.8, 0.78
    blk(K(M['desk'], 25), (D, W, 0.05), (0, 0, H - 0.025), bevel=0.01)
    blk(K(M['leather_g'], 25), (D - 0.16, W - 0.3, 0.004), (0, 0, H + 0.002))
    for s in (1, -1):  # Schubladenkaesten
        blk(K(M['desk'], 25), (D - 0.04, 0.42, H - 0.08), (0, s * (W / 2 - 0.23), (H - 0.08) / 2 + 0.03), bevel=0.008)
        blk(K(M['desk_d'], 25), (D - 0.08, 0.38, 0.03), (0, s * (W / 2 - 0.23), 0.015))
        for k in range(3):  # Schubladen zur Sitzseite (-x) mit Messinggriffen
            z = 0.14 + 0.2 * k
            blk(K(M['desk_d'], 25), (0.012, 0.36, 0.17), (-D / 2 + 0.014, s * (W / 2 - 0.23), z), bevel=0.003)
            cyl(K(M['brass_b'], 30), V((-D / 2 + 0.0, s * (W / 2 - 0.23) - 0.04, z)), V((-D / 2 + 0.0, s * (W / 2 - 0.23) + 0.04, z)), 0.006, 6)
    blk(K(M['desk'], 25), (0.02, W - 0.9, H - 0.12), (D / 2 - 0.05, 0, (H - 0.12) / 2 + 0.07))  # Blende zum Besucher
    # Bankerlampe (links vom Sitzenden: +y), Licht im Spiel bei (-0,05, +0,38, 1,02)
    o = V((-0.05, 0.38, H))
    lathe(K(M['brass_b'], 30), [(0.0, 0.0), (0.075, 0.0), (0.07, 0.012), (0.02, 0.018), (0.0, 0.02)], 14, Matrix.Translation(o))
    cyl(K(M['brass_b'], 30), o + V((0, 0, 0.015)), o + V((0, 0, 0.2)), 0.008, 8)
    sweep(K(M['brass_b'], 30), [o + V((0, 0, 0.2)), o + V((0.0, 0, 0.23)), o + V((0.04, 0, 0.235))], [0.007, 0.007, 0.006], 8)
    shade_c = o + V((0.04, 0, 0.235))
    lathe(K(M['shade'], 40), [(0.0, 0.035), (0.05, 0.03), (0.13, -0.01), (0.135, -0.025), (0.125, -0.025)], 16,
          Matrix.Translation(shade_c) @ Matrix.Rotation(math.pi / 2, 4, 'Z') @ Matrix.Diagonal((0.5, 1.0, 1.0, 1.0)))
    ellipsoid(K(M['bulb'], 40), shade_c + V((0, 0, -0.01)), V((0, 1, 0)), 0.04, 0.012, 8, 4)
    # Vertrag: zwei Bogen, Schrift als Zeilen, rotes Siegel, Fuellfeder quer
    for k in range(2):
        c = V((0.08 + 0.01 * k, -0.05 + 0.03 * k, H + 0.006 + 0.001 * k))
        blk(K(M['paper'], 20), (0.3, 0.21, 0.0015), c, rz=0.08 * k - 0.04)
    for li in range(12):
        x = 0.2 - 0.02 * li
        blk(K(M['ink'], 20), (0.0025, rng.uniform(0.09, 0.17), 0.0006), (x, -0.02, H + 0.0086), rz=-0.03)
    lathe(K(M['seal'], 30), [(0.0, 0.0), (0.016, 0.0), (0.015, 0.003), (0.0, 0.004)], 12, Matrix.Translation((0.0, 0.03, H + 0.008)))
    cyl(K(M['bakelite'], 30), V((0.06, -0.16, H + 0.012)), V((0.16, -0.04, H + 0.012)), 0.0055, 8)
    cyl(K(M['gold'], 30), V((0.16, -0.04, H + 0.012)), V((0.18, -0.016, H + 0.012)), 0.003, 6)
    # Tintenfass, Kontobuch, Telefon
    lathe(K(M['ink'], 30), [(0.0, 0.0), (0.03, 0.0), (0.032, 0.03), (0.012, 0.045), (0.01, 0.055), (0.0, 0.055)], 12, Matrix.Translation((-0.2, -0.45, H)))
    blk(K(M['leather_r'], 25), (0.24, 0.32, 0.035), (-0.2, -0.2, H + 0.018), rz=0.1, bevel=0.004)
    t = V((-0.25, 0.55, H))
    lathe(K(M['bakelite'], 30), [(0.0, 0.0), (0.06, 0.0), (0.055, 0.02), (0.016, 0.03), (0.012, 0.25), (0.022, 0.27), (0.024, 0.3), (0.0, 0.31)], 12, Matrix.Translation(t))
    sweep(K(M['bakelite'], 30), [t + V((0.03, 0.02, 0.2)), t + V((0.05, 0.04, 0.17)), t + V((0.05, 0.05, 0.12))], [0.006, 0.006, 0.006], 6)
    lathe(K(M['bakelite'], 30), [(0.0, 0.0), (0.03, 0.0), (0.025, 0.05), (0.0, 0.055)], 10, Matrix.Translation(t + V((0.05, 0.05, 0.07))))
    K.finish()


def build_e18_wingchair():
    """Ohrensessel (Bodenmodell, Ursprung Mitte unten, Sitzseite +x): hohe Rueckenlehne mit Ohren, Ochsenblutleder mit
    Messingnaegeln, gedrechselte Fuesse - im Buero von der Tuer abgewandt."""
    K = Kit()
    M = _mats()
    blk(K(M['leather_r'], 30), (0.6, 0.72, 0.16), (0.03, 0, 0.36), bevel=0.04)  # Sitz
    blk(K(M['leather_r'], 30), (0.18, 0.78, 0.95), (-0.27, 0, 0.72), bevel=0.05)  # Ruecken
    for s in (1, -1):
        blk(K(M['leather_r'], 30), (0.55, 0.12, 0.3), (0.02, s * 0.36, 0.52), bevel=0.04)  # Armlehnen
        blk(K(M['leather_r'], 30), (0.3, 0.1, 0.5), (-0.17, s * 0.36, 0.98), rx=s * 0.25, bevel=0.04)  # Ohren
        for k in range(10):
            z = 0.4 + 0.07 * k
            lathe(K(M['brass_b'], 30), [(0.0, 0.0), (0.006, 0.0), (0.004, 0.004), (0.0, 0.005)], 6,
                  Matrix.Translation((0.2 - 0.04 * k, s * 0.42, z if z < 0.7 else 0.7)) @ _rot_to((0, s, 0)))
    blk(K(M['leather_r'], 30), (0.6, 0.78, 0.22), (0.0, 0, 0.18), bevel=0.03)  # Unterbau
    for (x, y) in ((0.25, 0.32), (0.25, -0.32), (-0.3, 0.32), (-0.3, -0.32)):
        lathe(K(M['desk_d'], 30), [(0.0, 0.0), (0.03, 0.0), (0.025, 0.04), (0.032, 0.07), (0.02, 0.09), (0.0, 0.09)], 10, Matrix.Translation((x, y, 0.0)))
    K.finish()


def build_e18_portrait():
    """Bildnis (Wandmodell, Mitte im Ursprung, 0,7 x 0,9 m): ein Mann im Nadelstreifenanzug, weisses Hemd, rote Krawatte -
    das Gesicht ist abgekratzt bis auf die Leinwand. Goldrahmen."""
    K = Kit()
    M = _mats()
    Wd, Hd = 0.7, 0.9
    for (y0, y1, z0, z1) in ((-Wd / 2 - 0.08, Wd / 2 + 0.08, Hd / 2, Hd / 2 + 0.08), (-Wd / 2 - 0.08, Wd / 2 + 0.08, -Hd / 2 - 0.08, -Hd / 2),
                             (-Wd / 2 - 0.08, -Wd / 2, -Hd / 2, Hd / 2), (Wd / 2, Wd / 2 + 0.08, -Hd / 2, Hd / 2)):
        blk(K(M['gilt'], 30), (0.05, y1 - y0, z1 - z0), (0.025, (y0 + y1) / 2, (z0 + z1) / 2), bevel=0.01)
    blk(K(M['paint_d'], 20), (0.01, Wd, Hd), (0.01, 0, 0))
    # Anzug (Schultern, Revers), Hemd, Krawatte
    ellipsoid(K(M['paint_suit'], 20), V((0.016, 0, -0.32)), V((0, 1, 0)), 0.3, 0.004, 14, 3)
    blk(K(M['paint_suit'], 20), (0.004, 0.55, 0.3), (0.017, 0, -0.33))
    for s in (1, -1):
        blk(K(M['paint_shirt'], 20), (0.004, 0.06, 0.22), (0.019, s * 0.035, -0.26), rx=s * 0.35)
    blk(K(M['paint_red'], 20), (0.004, 0.035, 0.2), (0.0205, 0, -0.28))
    # Hals und Kopf: der Kopf ist abgekratzt - helle, rohe Leinwand mit Kratzspuren
    blk(K(M['paint_skin'], 20), (0.004, 0.08, 0.08), (0.019, 0, -0.14))
    ellipsoid(K(M['canvas'], 20), V((0.019, 0, 0.02)), V((0, 0, 1)), 0.15, 0.11, 14, 3)
    rng = random.Random(1805)
    for i in range(18):
        y = rng.uniform(-0.1, 0.1)
        z = rng.uniform(-0.1, 0.14)
        blk(K(M['paint_d'], 20), (0.002, 0.004, rng.uniform(0.03, 0.08)), (0.0215, y, z), rx=rng.uniform(-0.4, 0.4))
    K.finish()


# --- Fleisch -------------------------------------------------------------------------------------------------------------
def build_meat_steak():
    """Fundstueck (Ursprung Mitte unten): silberner Teller vom Zimmerservice, darauf rohes Steak auf Metzgerpapier, Fettrand,
    Marmorierung, ein Zweig Petersilie."""
    rng = random.Random(2601)
    K = Kit()
    M = _mats()
    lathe(K(M['silver'], 40), [(0.0, 0.0), (0.1, 0.0), (0.12, 0.012), (0.13, 0.016), (0.128, 0.018), (0.1, 0.006), (0.0, 0.006)], 24)
    for k in range(5):  # Papier, unregelmaessig
        a = rng.uniform(0, TAU)
        blk(K(M['bpaper'], 20), (0.17, 0.15, 0.0015), (0.0, 0.0, 0.0075 + 0.0003 * k), rz=a)
    # Steak: flacher, unregelmaessiger Laib
    prof = []
    n = 16
    for i in range(n):
        a = TAU * i / n
        r = 0.075 * (1.0 + 0.18 * math.sin(2 * a + 0.4) + 0.08 * math.sin(5 * a))
        prof.append(V((r * math.cos(a) * 1.15, r * math.sin(a) * 0.8, 0.0)))
    for z0, z1, mm in ((0.009, 0.034, 'meat'),):
        for i in range(n):
            p, q = prof[i], prof[(i + 1) % n]
            bm = K(M[mm], 30)
            vs = [bm.verts.new(p + V((0, 0, z0))), bm.verts.new(q + V((0, 0, z0))), bm.verts.new(q * 0.98 + V((0, 0, z1))), bm.verts.new(p * 0.98 + V((0, 0, z1)))]
            bm.faces.new(vs)
        bm = K(M['meat'], 30)
        bm.faces.new([bm.verts.new(p * 0.98 + V((0, 0, 0.034 + 0.003 * math.sin(i)))) for i, p in enumerate(prof)])
    sweep(K(M['fat'], 30), [prof[i] * 1.02 + V((0, 0, 0.02)) for i in range(3, 9)], [0.009, 0.011, 0.012, 0.012, 0.01, 0.007], 6)
    for i in range(14):  # Marmorierung
        p = V((rng.uniform(-0.06, 0.06), rng.uniform(-0.04, 0.04), 0.0352))
        q = p + V((rng.uniform(-0.02, 0.02), rng.uniform(-0.015, 0.015), 0))
        sweep(K(M['fat'], 20), [p, q], [0.0014, 0.0006], 3, caps=False)
    for i in range(4):
        p = V((rng.uniform(-0.05, 0.05), rng.uniform(-0.03, 0.03), 0.0355))
        ellipsoid(K(M['meat_d'], 30), p, V((1, 0, 0)), rng.uniform(0.008, 0.015), 0.006, 6, 3)
    sweep(K(M['parsley'], 30), [V((0.08, 0.05, 0.012)), V((0.1, 0.07, 0.02)), V((0.11, 0.09, 0.022))], [0.002, 0.0015, 0.001], 4)
    for k in range(4):
        ellipsoid(K(M['parsley'], 30), V((0.1 + 0.01 * k, 0.07 + 0.008 * k, 0.022)), V((1, 1, 0)), 0.008, 0.005, 6, 3)
    K.finish()


def build_meat_tin():
    """Fundstueck (Ursprung Mitte unten): Dose Pressfleisch ohne Marke, Etikett verblichen, eine Delle, Schluessel oben."""
    K = Kit()
    M = _mats()
    lathe(K(M['tin'], 40), [(0.0, 0.0), (0.042, 0.0), (0.045, 0.004), (0.045, 0.012), (0.044, 0.014)], 20)
    lathe(K(M['label'], 40), [(0.0447, 0.014), (0.0447, 0.05)], 20)
    lathe(K(M['label_w'], 40), [(0.0449, 0.026), (0.0449, 0.038)], 20, a0=-0.6, a1=0.9)
    lathe(K(M['tin'], 40), [(0.044, 0.05), (0.045, 0.052), (0.045, 0.058), (0.042, 0.06), (0.0, 0.06)], 20)
    ellipsoid(K(M['tin'], 30), V((0.038, 0.02, 0.03)), V((1, 0.5, 0)), 0.004, 0.016, 8, 4)  # Delle (Wulst)
    sweep(K(M['tin'], 30), [V((0.02, 0, 0.061)), V((0.06, 0, 0.061)), V((0.075, 0, 0.066))], [0.002, 0.002, 0.004], 4)
    K.finish()
