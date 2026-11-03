"""V13.3 Teil 4 Level 188: die Wurzelpistole (Entity 39) und die Spuren der Gaben im Hof (KREATUR_Wurzelpistole.md;
Wikidot level-188 Rev. 69, entity-39 Rev. 46 - CC BY-SA 3.0).

Die Seite der Ebene: Moosproben aus dem Hof gingen an einen Kult, der sich mit "Gaben" bedankte - das Wort verweist auf
Entity 39: pflanzliche Wesen in Gestalt von Schusswaffen, die Schale aus Mineralien und Metall des Bodens, am Griff Wurzeln, die
sich um Hand und Unterarm schlingen und Blut ziehen; Kugeln aus verdichtetem Blut (Feuersalz statt Pulver) in einer verborgenen
Kammer zwischen Griff und Magazin; leere Blutkoerperchen-Huelsen sickern aus dem Lauf; ein Schuss kann als Same keimen; ohne
genug Metall im Boden bleibt die Schale schwach; man soll sie in einem Kasten aufbewahren, damit sie die Haut nicht beruehrt.
Gebaut (eigene Gestaltung, keine Waffe aus einem Spiel):
  e39_gun        Ursprung unten am Griff, Lauf entlang +x (Masse wie gameplay/courtyard_l188.hpp: Griff bis 0,105 m, Muendung
                 bei x 0,158 / z 0,142): Griff aus rissiger Rinde, darin vorn die Kammer (eine dunkelrote, nasse Blase unter
                 duenner Rinde); die Schale duenn, stumpf grau-gruen, hier und da mit rostigem Mineralschorf, an anderen Stellen
                 abgeblaettert - darunter bleiches Pflanzenfleisch (zu wenig Eisen im Moos); Abzugbuegel eine Ranke, der Abzug
                 ein Dorn; zwei Blaettchen aus einer Naht; am Griffende der Wurzelkragen.
  e39_root       ein Glied einer Wurzel (entlang +x, 0,075 m), blassbraun mit rotem Stich (Blut), Wurzelhaare
  e39_husk       ein Tropfen leerer Huelsen aus dem Lauf (rotbraun, nass)
  e39_bed_moss   aufgeworfenes Moos um den Griff, dunkle Erde, Oberflaechenwurzeln, eingetrocknete Tropfen
  e39_bed_crack  gesprungener Betonboden des Gangs, Wurzeln in den Rissen, gehobene Splitter, etwas Moos im Riss
  e39_sprout     ein Keimling aus einem Schuss: Stiel, zwei Keimblaetter, oben die winzige Knospe einer Schale
  e39_samples    drei ausgestochene Moosproben mit Staeben und Faehnchen (die Proben, die weggegeben wurden)
  e39_case       der leere Kasten der Gabe: Holz, innen mit Moos ausgeschlagen, die Mulde in Form der Pistole ist leer
  e39_note       der Probenzettel (Fund): Karteikarte mit Bleistiftzeilen, ein Moosfleck, Schnur
Konventionen: Meter, z oben, Blick entlang +x; Bodenmodelle Ursprung am Boden.
"""
import math
import random

from mathutils import Matrix, Vector

from models.models_l1_phenomena import ellipsoid
from models.models_l1_sectors import Kit, blk, cyl, lathe, sweep
from models.models_l3 import mat

V = Vector
TAU = 2.0 * math.pi
GRIP_TOP = 0.105
MUZZLE = V((0.158, 0.0, 0.142))
SEG = 0.075


def _mats():
    return {
        'bark': mat('e39_rinde', (0.20, 0.15, 0.11), 'wood', r=1.7),
        'bark_d': mat('e39_rinde_dunkel', (0.11, 0.085, 0.065), 'wood', r=1.9),
        'shell': mat('e39_schale', (0.30, 0.32, 0.29), 'metal', r=1.25, m=0.55),
        'shell_d': mat('e39_schale_dunkel', (0.17, 0.18, 0.17), 'metal', r=1.4, m=0.45),
        'crust': mat('e39_mineralschorf', (0.46, 0.28, 0.14), 'concrete', r=1.6),
        'flesh': mat('e39_pflanzenfleisch', (0.62, 0.64, 0.46), 'plastic', r=0.8),
        'chamber': mat('e39_kammer', (0.26, 0.02, 0.025), 'water', r=0.15),
        'bore': mat('e39_lauf', (0.02, 0.015, 0.012), 'plastic', r=0.9),
        'thorn': mat('e39_dorn', (0.72, 0.66, 0.52), 'ceramic', r=0.6),
        'leaf': mat('e39_blatt', (0.20, 0.32, 0.09), 'plastic', r=0.6),
        'root': mat('e39_wurzel', (0.66, 0.48, 0.38), 'wood', r=1.0),  # Runde 1: auf weissem Beton schwarze Spinnenbeine
        'root_d': mat('e39_wurzel_blutig', (0.36, 0.11, 0.08), 'plastic', r=0.5),
        'husk': mat('e39_huelsen', (0.30, 0.05, 0.035), 'water', r=0.2),
        'husk_dry': mat('e39_huelsen_trocken', (0.18, 0.06, 0.04), 'paint', r=1.3),
        'moss': mat('moos_rand', (0.24, 0.32, 0.10), 'fabric', 'moss', r=1.4),
        'moss_dry': mat('e39_moos_trocken', (0.30, 0.30, 0.14), 'fabric', 'moss', r=1.6),
        'soil': mat('e39_erde', (0.26, 0.19, 0.12), 'concrete', r=1.8),  # Runde 1: auf dem Moos ein schwarzes Loch
        'concrete': mat('beton_weiss', (0.93, 0.93, 0.92), 'concrete', 'concrete_white', r=1.2),
        'crack': mat('e39_riss', (0.05, 0.045, 0.04), 'concrete', r=1.5),
        'stick': mat('e39_stab', (0.62, 0.50, 0.32), 'wood', r=1.3),
        'paper': mat('e39_zettel', (0.86, 0.84, 0.76), 'paint', r=1.25),
        'pencil': mat('e39_bleistift', (0.18, 0.18, 0.2), 'paint', r=1.0),
        'string': mat('e39_schnur', (0.70, 0.64, 0.50), 'fabric', r=1.3),
        'case': mat('e39_kasten', (0.30, 0.17, 0.09), 'wood', 'wood_walnut_floor', r=1.5),
        'case_d': mat('e39_kasten_innen', (0.16, 0.09, 0.05), 'wood', 'wood_walnut_floor', r=1.7),
        'brass': mat('messing_angelaufen', (0.45, 0.34, 0.18), 'metal', r=0.75, m=0.6),
    }


def _wiggle(rng, a, b, n, amp, z=None):
    pts = []
    d = (b - a)
    side = V((-d.y, d.x, 0.0)).normalized() if d.length > 1e-6 else V((0, 1, 0))
    for i in range(n + 1):
        t = i / n
        p = a.lerp(b, t) + side * (amp * math.sin(t * math.pi * (1.5 + rng.random())) * (1.0 if i % 2 else 0.6)) * (1 - abs(2 * t - 1) * 0.3)
        if z is not None:
            p.z = z(t)
        pts.append(p)
    return pts


def build_e39_gun():
    rng = random.Random(3901)
    K = Kit()
    M = _mats()
    tilt = 0.33  # Griffwinkel: oben weiter vorn
    up = V((math.sin(tilt), 0.0, math.cos(tilt)))
    g0, g1 = V((-0.012, 0, 0.0)), V((-0.012, 0, 0.0)) + up * 0.112
    # Griff: Rinde, oval, unten knollig (der Wurzelkragen)
    for i in range(7):
        t = i / 6
        c = g0.lerp(g1, t)
        ellipsoid(K(M['bark'], 35), c, up, 0.018, 0.0175 - 0.002 * math.sin(t * math.pi), 9, 5)
    blk(K(M['bark'], 30), (0.038, 0.026, 0.1), tuple(g0.lerp(g1, 0.5)), ry=tilt, bevel=0.009)
    for k in range(9):  # Rindenrippen laengs
        a = TAU * k / 9 + rng.uniform(-0.15, 0.15)
        off = V((math.cos(a) * 0.0205, math.sin(a) * 0.0138, 0.0))
        pts = [g0 + off + up * (0.006 + 0.1 * j / 5) + V((rng.uniform(-0.001, 0.001), rng.uniform(-0.001, 0.001), 0)) for j in range(6)]
        sweep(K(M['bark_d'], 30), pts, [0.0028, 0.0034, 0.003, 0.0034, 0.0028, 0.0016], 5)
    # die Kammer vorn im Griff: dunkelrote nasse Blase unter aufgerissener Rinde
    kc = g0.lerp(g1, 0.55) + V((0.017, 0, 0.0))
    ellipsoid(K(M['chamber'], 50), kc, up, 0.03, 0.0105, 12, 7)
    for s in (-1, 1):  # Rindenlappen beiderseits
        sweep(K(M['bark_d'], 30), [kc + up * -0.03 + V((0.002, s * 0.009, 0)), kc + V((0.004, s * 0.012, 0)), kc + up * 0.03 + V((0.001, s * 0.009, 0))],
              [0.002, 0.0035, 0.002], 5)
    # Wurzelkragen am Griffende
    for k in range(10):
        a = TAU * k / 10 + 0.2
        ellipsoid(K(M['root'], 35), g0 + V((math.cos(a) * 0.017, math.sin(a) * 0.013, 0.004)), V((math.cos(a), math.sin(a), -0.4)), 0.011, 0.006, 7, 4)
    # Schale (Schlitten): duenn, stumpf, Kanten gerundet
    blk(K(M['shell'], 25), (0.19, 0.026, 0.036), (0.067, 0.0, 0.131), bevel=0.0065)
    blk(K(M['shell_d'], 25), (0.088, 0.024, 0.018), (0.108, 0.0, 0.106), bevel=0.004)  # unter dem Lauf
    for k in range(7):  # gewachsene Rippen hinten statt Fraesung
        x = -0.022 + k * 0.0055
        sweep(K(M['shell_d'], 25), [V((x, -0.0135, 0.116)), V((x + 0.002, -0.0138, 0.131)), V((x, -0.0135, 0.146))], 0.0011, 4)
        sweep(K(M['shell_d'], 25), [V((x, 0.0135, 0.116)), V((x + 0.002, 0.0138, 0.131)), V((x, 0.0135, 0.146))], 0.0011, 4)
    # Muendung: Lauf, dunkle Bohrung, nasser Rand (Huelsen sickern)
    cyl(K(M['shell_d'], 25), MUZZLE - V((0.008, 0, 0)), MUZZLE + V((0.005, 0, 0)), 0.0082, 10)
    cyl(K(M['bore'], 0), MUZZLE + V((0.0042, 0, 0)), MUZZLE + V((0.0056, 0, 0)), 0.0048, 10)
    ellipsoid(K(M['husk'], 50), MUZZLE + V((0.0045, 0, -0.0072)), V((0, 0, 1)), 0.004, 0.003, 8, 5)
    sweep(K(M['husk'], 50), [MUZZLE + V((0.006, 0.003, -0.004)), MUZZLE + V((0.0058, 0.004, -0.009)), MUZZLE + V((0.005, 0.0042, -0.016))], [0.0016, 0.0019, 0.0009], 5)
    # Mineralschorf (rostig) und abgeblaetterte Stellen (bleiches Fleisch)
    for k in range(12):
        x = rng.uniform(-0.02, 0.15)
        s = rng.choice((-1, 1))
        z = rng.uniform(0.118, 0.146)
        top = rng.random() < 0.3
        p = V((x, 0.0 if top else s * 0.0132, 0.1495 if top else z))
        n = V((0, 0, 1)) if top else V((0, s, 0))
        if k % 3 == 0:
            ellipsoid(K(M['flesh'], 40), p - n * 0.0009, n, 0.0012, rng.uniform(0.004, 0.008), 9, 3)
        else:
            ellipsoid(K(M['crust'], 30), p + n * 0.0002, n, 0.0016, rng.uniform(0.003, 0.009), 7, 3)
    for k in range(4):  # Risse in der Schale
        x0 = rng.uniform(0.0, 0.13)
        s = rng.choice((-1, 1))
        pts = [V((x0 + j * 0.008, s * 0.0134, 0.14 - j * 0.005 + rng.uniform(-0.002, 0.002))) for j in range(4)]
        sweep(K(M['bore'], 0), pts, 0.0007, 4)
    # Abzugbuegel (Ranke), Abzug (Dorn)
    sweep(K(M['bark'], 35), [V((0.018, 0, 0.098)), V((0.03, 0, 0.074)), V((0.058, 0, 0.068)), V((0.078, 0, 0.082)), V((0.084, 0, 0.098))],
          [0.0034, 0.0036, 0.0036, 0.0034, 0.003], 6)
    sweep(K(M['thorn'], 40), [V((0.046, 0, 0.098)), V((0.049, 0, 0.087)), V((0.045, 0, 0.077))], [0.0032, 0.0024, 0.0006], 6)
    # zwei Blaettchen aus einer Naht hinten oben
    for s, a in ((1, 0.5), (-1, -0.4)):
        base = V((0.004, s * 0.006, 0.149))
        tip = base + V((-0.012, s * 0.014, 0.016))
        ellipsoid(K(M['leaf'], 40), base.lerp(tip, 0.55), (tip - base), 0.0125, 0.0055, 8, 4)
        sweep(K(M['leaf'], 40), [base - V((0, 0, 0.002)), base.lerp(tip, 0.3)], 0.0009, 4)
    K.finish()


def build_e39_root():
    rng = random.Random(3902)
    K = Kit()
    M = _mats()
    pts = [V((SEG * t / 6, 0.0012 * math.sin(t * 1.7), 0.0009 * math.sin(t * 2.3))) for t in range(7)]
    sweep(K(M['root'], 35), pts, [0.0063, 0.0059, 0.0055, 0.0056, 0.0052, 0.0049, 0.0047], 7)
    ellipsoid(K(M['root'], 35), V((0.0, 0, 0)), V((1, 0, 0)), 0.0075, 0.0068, 8, 5)  # Knoten (Gelenk)
    for k in range(3):  # blutige Stellen (die Wurzel fuellt sich)
        x = rng.uniform(0.012, 0.065)
        a = rng.uniform(0, TAU)
        ellipsoid(K(M['root_d'], 40), V((x, math.cos(a) * 0.0052, math.sin(a) * 0.0052)), V((1, 0, 0)), 0.006, 0.0022, 7, 4)
    for k in range(6):  # Wurzelhaare
        x = rng.uniform(0.008, 0.07)
        a = rng.uniform(0, TAU)
        d = V((0.3, math.cos(a), math.sin(a))).normalized()
        p0 = V((x, math.cos(a) * 0.005, math.sin(a) * 0.005))
        sweep(K(M['root'], 30), [p0, p0 + d * 0.008 + V((0.003, 0, 0)), p0 + d * 0.014 + V((0.006, 0, -0.002))], [0.0011, 0.0007, 0.0002], 3, caps=False)
    K.finish()


def build_e39_husk():
    K = Kit()
    M = _mats()
    ellipsoid(K(M['husk'], 60), V((0, 0, -0.004)), V((0, 0, 1)), 0.0058, 0.0042, 10, 6)
    sweep(K(M['husk'], 60), [V((0, 0, 0.0)), V((0, 0, 0.004)), V((0.0004, 0, 0.0075))], [0.003, 0.0016, 0.0003], 6)
    K.finish()


def _surface_roots(K, M, rng, n, r0, r1, z=0.002):
    for k in range(n):
        a = TAU * k / n + rng.uniform(-0.25, 0.25)
        L = rng.uniform(r0, r1)
        b = V((math.cos(a) * L, math.sin(a) * L, z))
        pts = _wiggle(rng, V((math.cos(a) * 0.02, math.sin(a) * 0.02, z + 0.004)), b, 6, 0.012, z=lambda t: z + 0.004 * (1 - t))
        sweep(K(M['root'], 35), pts, [0.0048 - 0.0032 * i / 6 for i in range(7)], 6)


def build_e39_bed_moss():
    """Aufgeworfenes Moos um den Griff (Ursprung am Boden, Mitte = Griff)."""
    rng = random.Random(3903)
    K = Kit()
    M = _mats()
    # Runde 1: eine flache schwarze Scheibe - jetzt ein aufgeworfener Moosbuckel, Erde nur in einem schmalen Kranz am Griff,
    # ringsum ausgerissene Moosbatzen
    ellipsoid(K(M['moss'], 40), V((0, 0, -0.012)), V((0, 0, 1)), 0.042, 0.13, 14, 6)
    ellipsoid(K(M['moss_dry'], 40), V((0, 0, 0.006)), V((0, 0, 1)), 0.03, 0.07, 12, 5)
    ellipsoid(K(M['soil'], 30), V((0, 0, 0.024)), V((0, 0, 1)), 0.012, 0.03, 12, 5)
    for k in range(7):
        a = TAU * k / 7 + rng.uniform(-0.3, 0.3)
        r = rng.uniform(0.07, 0.12)
        ellipsoid(K(M['moss'] if k % 2 else M['moss_dry'], 40), V((math.cos(a) * r, math.sin(a) * r, 0.012)), V((math.cos(a), math.sin(a), 1.2)),
                  0.014, rng.uniform(0.016, 0.026), 8, 4)
    for k in range(9):  # Erdkrumen
        a = rng.uniform(0, TAU)
        r = rng.uniform(0.03, 0.08)
        ellipsoid(K(M['soil'], 30), V((math.cos(a) * r, math.sin(a) * r, 0.006)), V((0, 0, 1)), 0.004, rng.uniform(0.004, 0.009), 6, 3)
    _surface_roots(K, M, rng, 8, 0.1, 0.19, z=0.008)
    for k in range(4):  # eingetrocknete Tropfen (Huelsen)
        a = rng.uniform(0, TAU)
        r = rng.uniform(0.05, 0.12)
        ellipsoid(K(M['husk_dry'], 30), V((math.cos(a) * r, math.sin(a) * r, 0.0145)), V((0, 0, 1)), 0.0012, rng.uniform(0.006, 0.012), 8, 3)
    K.finish()


def build_e39_bed_crack():
    """Gesprungener Betonboden (Ursprung am Boden): Risse, gehobene Splitter, Wurzeln darin, etwas Moos im Riss."""
    rng = random.Random(3904)
    K = Kit()
    M = _mats()
    for k in range(6):  # Risse strahlenfoermig
        a = TAU * k / 6 + rng.uniform(-0.3, 0.3)
        L = rng.uniform(0.16, 0.3)
        pts = _wiggle(rng, V((0, 0, 0.0008)), V((math.cos(a) * L, math.sin(a) * L, 0.0008)), 7, 0.02, z=lambda t: 0.0008)
        sweep(K(M['crack'], 0), pts, [0.006 - 0.005 * i / 7 for i in range(8)], 4)
        if k % 2 == 0:
            pts2 = [p + V((0, 0, 0.0025)) for p in pts[:5]]
            sweep(K(M['root'], 35), pts2, [0.005 - 0.0006 * i for i in range(5)], 6)
    for k in range(4):  # gehobene Splitter
        a = TAU * k / 4 + rng.uniform(-0.4, 0.4)
        r = rng.uniform(0.03, 0.06)
        blk(K(M['concrete'], 0), (rng.uniform(0.03, 0.06), rng.uniform(0.025, 0.045), 0.012), (math.cos(a) * r, math.sin(a) * r, 0.004),
            rx=rng.uniform(-0.25, 0.25), ry=rng.uniform(-0.25, 0.25), rz=a, bevel=0.0015)
    ellipsoid(K(M['soil'], 30), V((0, 0, -0.002)), V((0, 0, 1)), 0.008, 0.032, 10, 4)
    for k in range(5):  # Moos in den Rissen
        a = rng.uniform(0, TAU)
        r = rng.uniform(0.04, 0.12)
        ellipsoid(K(M['moss'], 40), V((math.cos(a) * r, math.sin(a) * r, 0.001)), V((0, 0, 1)), 0.003, rng.uniform(0.006, 0.014), 8, 3)
    K.finish()


def build_e39_sprout():
    """Ein Keimling aus einem Schuss (Ursprung am Boden, 0,065 m): Stiel, Keimblaetter, oben die Knospe einer Schale."""
    rng = random.Random(3905)
    K = Kit()
    M = _mats()
    stem = [V((0, 0, 0)), V((0.002, 0.001, 0.02)), V((0.006, 0.0, 0.038)), V((0.008, -0.001, 0.05))]
    sweep(K(M['root'], 35), stem, [0.0034, 0.003, 0.0026, 0.0024], 6)
    for s in (1, -1):
        base = V((0.006, 0, 0.04))
        tip = base + V((-0.006, s * 0.022, 0.006))
        ellipsoid(K(M['leaf'], 40), base.lerp(tip, 0.55), tip - base, 0.0125, 0.0058, 8, 4)
    top = V((0.009, 0, 0.052))
    blk(K(M['shell'], 25), (0.024, 0.0068, 0.0085), tuple(top + V((0.006, 0, 0.004))), bevel=0.0018)
    blk(K(M['bark'], 30), (0.0075, 0.0062, 0.012), tuple(top + V((-0.002, 0, -0.003))), ry=0.3, bevel=0.0016)
    ellipsoid(K(M['flesh'], 40), top + V((0.0, 0, 0.0085)), V((1, 0, 0)), 0.006, 0.0022, 7, 3)
    ellipsoid(K(M['soil'], 30), V((0, 0, -0.002)), V((0, 0, 1)), 0.006, 0.022, 10, 4)
    for k in range(4):
        a = TAU * k / 4 + rng.uniform(-0.3, 0.3)
        sweep(K(M['root'], 35), [V((0, 0, 0.003)), V((math.cos(a) * 0.012, math.sin(a) * 0.012, 0.002)), V((math.cos(a) * 0.022, math.sin(a) * 0.024, 0.0012))],
              [0.0018, 0.0012, 0.0005], 4)
    K.finish()


def build_e39_samples():
    """Drei ausgestochene Moosproben mit Staeben und Faehnchen (Ursprung Mitte, am Boden, Reihe entlang +x)."""
    rng = random.Random(3906)
    K = Kit()
    M = _mats()
    for k in range(3):
        c = V(((k - 1) * 0.26 + rng.uniform(-0.02, 0.02), rng.uniform(-0.03, 0.03), 0.0))
        # Loch: dunkle Erde, darum ausgefranster Moosrand
        lathe(K(M['soil'], 30), [(0.0, 0.0008), (0.034, 0.0008), (0.036, 0.0035)], 16, Matrix.Translation(c))
        for j in range(14):
            a = TAU * j / 14 + rng.uniform(-0.1, 0.1)
            ellipsoid(K(M['moss_dry'] if j % 3 else M['moss'], 40), c + V((math.cos(a) * 0.039, math.sin(a) * 0.039, 0.004)), V((math.cos(a), math.sin(a), 0.6)),
                      0.006, rng.uniform(0.006, 0.01), 6, 3)
        # Stab mit Faehnchen
        s0 = c + V((0.05, 0.02, 0.0))
        s1 = s0 + V((rng.uniform(-0.01, 0.01), rng.uniform(-0.01, 0.01), 0.17))
        cyl(K(M['stick'], 20), s0 - V((0, 0, 0.03)), s1, 0.0028, 6)
        blk(K(M['paper'], 0), (0.044, 0.0015, 0.026), tuple(s1 + V((0.023, 0, -0.016))), rz=rng.uniform(-0.3, 0.3), bevel=0.0004)
        for ln in range(2):  # Bleistift: Nummer und Datum
            blk(K(M['pencil'], 0), (0.026 - ln * 0.008, 0.0017, 0.0016), tuple(s1 + V((0.02, 0, -0.01 - ln * 0.007))), bevel=0.0)
    K.finish()


def build_e39_case():
    """Der leere Kasten einer Gabe (Ursprung am Boden, Laenge entlang +x): Holz mit Messingecken, Deckel offen nach hinten,
    innen Moos; die Mulde in Form der Pistole ist leer, darin ein paar abgerissene Wurzelfasern."""
    rng = random.Random(3907)
    K = Kit()
    M = _mats()
    L, W, H = 0.30, 0.2, 0.07
    blk(K(M['case'], 0), (L, W, 0.01), (0, 0, 0.005), bevel=0.002)
    for s in (-1, 1):
        blk(K(M['case'], 0), (L, 0.012, H), (0, s * (W / 2 - 0.006), H / 2), bevel=0.0018)
        blk(K(M['case'], 0), (0.012, W, H), (s * (L / 2 - 0.006), 0, H / 2), bevel=0.0018)
    blk(K(M['moss'], 0), (L - 0.026, W - 0.026, 0.035), (0, 0, 0.028), bevel=0.004)
    # Mulde: dunkles Holz in Pistolenform (Griff und Lauf)
    blk(K(M['case_d'], 0), (0.2, 0.034, 0.006), (0.02, 0.0, 0.046), bevel=0.003)
    blk(K(M['case_d'], 0), (0.045, 0.11, 0.006), (-0.06, -0.045, 0.046), rz=0.33, bevel=0.003)
    for k in range(5):  # abgerissene Wurzelfasern
        p0 = V((-0.07 + rng.uniform(-0.01, 0.01), -0.09 + rng.uniform(-0.01, 0.01), 0.05))
        p1 = p0 + V((rng.uniform(-0.04, 0.04), rng.uniform(-0.03, 0.01), 0.002))
        sweep(K(M['root'], 35), [p0, p0.lerp(p1, 0.5) + V((0, 0, 0.004)), p1], [0.0018, 0.0013, 0.0004], 4)
    # Deckel offen nach hinten (gegen -y gekippt), Messingecken, Scharniere
    lid = Matrix.Translation(V((0, -W / 2, H))) @ Matrix.Rotation(-1.9, 4, 'X')
    for (sx, sy, sz, cx, cy, cz) in ((L, W, 0.012, 0, W / 2, 0.006),):
        cnt = lid @ V((cx, cy, cz))
        blk(K(M['case'], 0), (sx, sy, sz), tuple(cnt), rx=-1.9, bevel=0.002)
    lin = lid @ V((0, W / 2, -0.004))
    blk(K(M['moss_dry'], 0), (L - 0.03, W - 0.03, 0.008), tuple(lin), rx=-1.9, bevel=0.003)
    for sx in (-1, 1):
        for sy in (-1, 1):
            blk(K(M['brass'], 0), (0.022, 0.022, 0.022), (sx * (L / 2 - 0.008), sy * (W / 2 - 0.008), H - 0.008), bevel=0.002)
        cyl(K(M['brass'], 0), V((sx * 0.09 - 0.012, -W / 2 - 0.003, H)), V((sx * 0.09 + 0.012, -W / 2 - 0.003, H)), 0.004, 8)
    K.finish()


def build_e39_note():
    """Der Probenzettel (Fund, Ursprung am Boden): Karteikarte mit Bleistiftzeilen, ein Moosfleck, eine Schnur."""
    rng = random.Random(3908)
    K = Kit()
    M = _mats()
    blk(K(M['paper'], 0), (0.1, 0.064, 0.0014), (0, 0, 0.0007), rz=0.0, bevel=0.0003)
    for ln in range(6):
        w = 0.07 - rng.uniform(0.0, 0.03) if ln else 0.05
        blk(K(M['pencil'], 0), (w, 0.0012, 0.0003), (-0.04 + w / 2, 0.022 - ln * 0.0085, 0.0016), bevel=0.0)
    ellipsoid(K(M['moss'], 40), V((0.03, -0.018, 0.0017)), V((0, 0, 1)), 0.0008, 0.011, 9, 3)
    cyl(K(M['brass'], 0), V((-0.044, 0.024, 0.0014)), V((-0.044, 0.024, 0.0026)), 0.0035, 10)
    sweep(K(M['string'], 30), [V((-0.044, 0.024, 0.003)), V((-0.06, 0.04, 0.0015)), V((-0.075, 0.03, 0.0012)), V((-0.085, 0.05, 0.0012))], 0.0008, 4)
    K.finish()
