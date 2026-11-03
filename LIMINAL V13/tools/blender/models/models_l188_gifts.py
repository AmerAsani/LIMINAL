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
from models.models_l1_sectors import Kit, blk, cube, cyl, lathe, sweep
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
        'leaf_pale': mat('e39_keimblatt', (0.62, 0.66, 0.30), 'plastic', r=0.6),
        'root': mat('e39_wurzel', (0.70, 0.52, 0.42), 'plastic', r=0.9),  # Runde 1/4: auf weissem Beton schwarze Spinnenbeine - fleischig, blass
        'root_d': mat('e39_wurzel_blutig', (0.36, 0.11, 0.08), 'plastic', r=0.5),
        'husk': mat('e39_huelsen', (0.30, 0.05, 0.035), 'water', r=0.2),
        'husk_dry': mat('e39_huelsen_trocken', (0.18, 0.06, 0.04), 'paint', r=1.3),
        'moss': mat('e39_moos', (0.78, 0.84, 0.72), 'fabric', 'moss', r=1.3),  # Runde 2: wie der Boden des Hofs (Zone hof), sonst schwarz
        'moss_dry': mat('e39_moos_trocken', (0.95, 0.86, 0.62), 'fabric', 'moss', r=1.5),
        'moss_dead': mat('e39_moos_tot', (0.98, 0.66, 0.36), 'fabric', 'moss', r=1.7),  # Abnahme 188: ausgezehrt, braun
        'velvet': mat('e39_kasten_samt', (0.20, 0.035, 0.04), 'fabric', r=1.6),  # Abnahme 188: Mulde dunkel (Blut im Samt)
        'soil': mat('e39_erde', (0.26, 0.19, 0.12), 'concrete', r=1.8),  # Runde 1: auf dem Moos ein schwarzes Loch
        'concrete': mat('beton_weiss', (0.93, 0.93, 0.92), 'concrete', 'concrete_white', r=1.2),
        'crack': mat('e39_riss', (0.30, 0.29, 0.27), 'concrete', r=1.5),  # Runde 2: schwarze Strahlen wie Spinnenbeine
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


def _withered_patch(K, M, rng, R0=0.62):
    """V13.3 Abnahme Level 188 (Auslegung, KREATUR_Wurzelpistole.md): die Pistole zieht Eisen und Blut aus dem Boden - um sie
    ist das Moos ausgezehrt. Ein unregelmaessiger Fleck (Radius um 0,6 m) aus totem, braunem Moos, nach aussen trockenes,
    dann wieder gruenes Moos in Zungen; der Rand sinkt in den Boden. Darin ein Geflecht blasser Wurzeln, das vom Griff
    ausgeht, sich verzweigt und stellenweise unter das Moos taucht. Liest sich aus 5-8 m als falscher Fleck im Rasen."""
    # Runde 4: als eine Flaeche aus Ringen mit Material je Viereck las sich der Fleck wie ein ausgeschnittenes Papier (harte,
    # eckige Kanten, flache Farben). Jetzt aus vielen flachen Moospolstern wie der Buckel am Griff: innen tot und braun, nach
    # aussen trocken, am Rand einzelne gruene und trockene Polster, die in den Rasen auslaufen (kein Rand).
    ph = [rng.uniform(0, TAU) for _ in range(4)]

    def rad(a):
        return R0 * (1.0 + 0.2 * math.sin(3 * a + ph[0]) + 0.11 * math.sin(5 * a + ph[1]) + 0.07 * math.sin(9 * a + ph[2]))

    for k in range(230):
        a = rng.uniform(0, TAU)
        t = math.sqrt(rng.uniform(0.0, 1.0)) * rng.uniform(0.92, 1.08)  # gleichmaessig ueber die Flaeche, Rand ausgefranst
        tongue = 0.5 + 0.5 * math.sin(4 * a + ph[3])
        if t < 0.45 + 0.12 * tongue:
            mk = 'moss_dead'
        elif t < 0.75 + 0.12 * tongue:
            mk = 'moss_dead' if rng.random() < 0.4 else 'moss_dry'
        else:
            mk = 'moss_dry' if rng.random() < 0.6 else 'moss'
        r = rad(a) * t
        c = V((math.cos(a) * r, math.sin(a) * r, rng.uniform(-0.004, 0.0) - 0.004 * t))
        h = rng.uniform(0.006, 0.012) * (1.0 - 0.4 * t)
        rr = rng.uniform(0.035, 0.075) * (1.0 - 0.3 * t)
        ax = V((rng.uniform(-0.15, 0.15), rng.uniform(-0.15, 0.15), 1.0))
        ellipsoid(K(M[mk], 40), c, ax, h, rr, 9, 4)
    # Wurzelgeflecht: Hauptwurzeln vom Griff, je ein bis zwei Seitenwurzeln; mal ueber, mal unter dem Moos (z)
    n = 11
    for k in range(n):
        a = TAU * k / n + rng.uniform(-0.22, 0.22)
        L = rad(a) * rng.uniform(0.55, 0.92)
        b = V((math.cos(a) * L, math.sin(a) * L, 0.0))
        dip = rng.uniform(0.3, 0.7)
        main = _wiggle(rng, V((math.cos(a) * 0.05, math.sin(a) * 0.05, 0.0)), b, 9, 0.03,
                       z=lambda t, d=dip: 0.0075 - 0.009 * max(0.0, 1.0 - abs(t - d) / 0.12) - 0.004 * t)
        sweep(K(M['root'], 35), main, [0.0042 - 0.0032 * i / 9 for i in range(10)], 6)
        for s in range(rng.randint(1, 2)):
            i0 = rng.randint(3, 6)
            p0 = main[i0]
            sa = a + rng.choice((-1, 1)) * rng.uniform(0.5, 1.0)
            sl = rng.uniform(0.08, 0.2)
            pe = p0 + V((math.cos(sa) * sl, math.sin(sa) * sl, 0.0))
            br = _wiggle(rng, p0, pe, 5, 0.012, z=lambda t, z0=p0.z: z0 * (1 - t) + 0.003 * t)
            sweep(K(M['root'], 35), br, [0.0022 - 0.0016 * i / 5 for i in range(6)], 5)


def build_e39_bed_moss():
    """Aufgeworfenes Moos um den Griff (Ursprung am Boden, Mitte = Griff)."""
    rng = random.Random(3903)
    K = Kit()
    M = _mats()
    _withered_patch(K, M, random.Random(3913))
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
    # Runde 2: sechs gleich lange schwarze Strahlen lasen sich wie Spinnenbeine - jetzt zwei lange, verzweigte Risse quer
    # durch die Platte (der Boden ist gesprungen), die Wurzeln folgen ihnen ein Stueck
    for k, (a, L) in enumerate(((0.3, 0.42), (0.3 + math.pi, 0.36), (2.0, 0.18))):
        pts = _wiggle(rng, V((0, 0, 0.0008)), V((math.cos(a) * L, math.sin(a) * L, 0.0008)), 9, 0.025, z=lambda t: 0.0008)
        sweep(K(M['crack'], 0), pts, [0.0055 - 0.0045 * i / 9 for i in range(10)], 4)
        if k < 2:
            for j in (3, 6):  # Verzweigungen
                b = pts[j] + V((math.cos(a + 1.1) * 0.08, math.sin(a + 1.1) * 0.08, 0.0))
                sweep(K(M['crack'], 0), _wiggle(rng, pts[j], b, 4, 0.01, z=lambda t: 0.0008), [0.003, 0.0026, 0.002, 0.0014, 0.0008], 4)
            sweep(K(M['root'], 35), [p + V((0, 0, 0.0028)) for p in pts[:6]], [0.0055 - 0.0006 * i for i in range(6)], 6)
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
        ellipsoid(K(M['leaf_pale'], 40), base.lerp(tip, 0.55), tip - base, 0.0125, 0.0058, 8, 4)
    top = V((0.009, 0, 0.052))
    blk(K(M['shell'], 25), (0.024, 0.0068, 0.0085), tuple(top + V((0.006, 0, 0.004))), bevel=0.0018)
    blk(K(M['bark'], 30), (0.0075, 0.0062, 0.012), tuple(top + V((-0.002, 0, -0.003))), ry=0.3, bevel=0.0016)
    ellipsoid(K(M['flesh'], 40), top + V((0.0, 0, 0.0085)), V((1, 0, 0)), 0.006, 0.0022, 7, 3)
    ellipsoid(K(M['soil'], 30), V((0, 0, -0.002)), V((0, 0, 1)), 0.006, 0.022, 10, 4)
    # Runde 5: im Moos unsichtbar (gruen auf gruen) - aufgeworfener, trockener Moosring um das Einschussloch, blasse Keimblaetter
    for k in range(9):
        a = TAU * k / 9 + rng.uniform(-0.2, 0.2)
        ellipsoid(K(M['moss_dry'], 40), V((math.cos(a) * 0.03, math.sin(a) * 0.03, 0.003)), V((math.cos(a), math.sin(a), 0.8)), 0.007, 0.011, 6, 3)
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
    # V13.3 Abnahme Level 188: von oben las sich der alte Kasten (7 cm hoch, 12 mm Wand, Deckel um 109 Grad flach nach hinten,
    # helles Moos bis fast an den Rand) als leerer Bilderrahmen. Jetzt ein tiefer Kasten (11 cm, Wand 18 mm) mit dunkler
    # Innenwand, das Moosbett 4 cm unter dem Rand, die Mulde als dunkler, blutfleckiger Samt in Pistolenform mit Moosrand; der
    # Deckel steht offen (98 Grad), von einer Messingstrebe gehalten, innen mit dem Abdruck der Pistole; vorn der offene Riegel.
    rng = random.Random(3907)
    K = Kit()
    M = _mats()
    L, W, H, T = 0.32, 0.22, 0.11, 0.018
    blk(K(M['case'], 0), (L, W, 0.014), (0, 0, 0.007), bevel=0.003)
    for s in (-1, 1):
        blk(K(M['case'], 0), (L, T, H), (0, s * (W / 2 - T / 2), H / 2), bevel=0.003)
        blk(K(M['case'], 0), (T, W - 2 * T + 0.002, H), (s * (L / 2 - T / 2), 0, H / 2), bevel=0.003)
    # Innenwand dunkel (Schatten, alter Leim), ein Fingerbreit unter dem Rand eine Leiste
    IL, IW = L - 2 * T, W - 2 * T
    for s in (-1, 1):
        blk(K(M['case_d'], 0), (IL, 0.002, H - 0.03), (0, s * (IW / 2 - 0.001), 0.014 + (H - 0.03) / 2))
        blk(K(M['case_d'], 0), (0.002, IW, H - 0.03), (s * (IL / 2 - 0.001), 0, 0.014 + (H - 0.03) / 2))
    ZB = H - 0.04  # Moosbett 4 cm unter dem Rand
    blk(K(M['moss_dry'], 0), (IL - 0.004, IW - 0.004, ZB - 0.014), (0, 0, 0.014 + (ZB - 0.014) / 2), bevel=0.005)
    for k in range(18):  # Polster des Mooses (Buckel), nicht flach wie ein Brett
        p = V((rng.uniform(-IL / 2 + 0.02, IL / 2 - 0.02), rng.uniform(-IW / 2 + 0.02, IW / 2 - 0.02), ZB - 0.002))
        if abs(p.y) < 0.03 and -0.08 < p.x < 0.12:
            continue
        ellipsoid(K(M['moss'] if k % 3 else M['moss_dry'], 40), p, V((0, 0, 1)), 0.006, rng.uniform(0.014, 0.024), 8, 3)
    # Mulde in Pistolenform: Lauf entlang +x, Griff schraeg nach hinten unten (wie e39_gun)
    shape = [((0.035, 0.0), (0.17, 0.036), 0.0), ((-0.055, -0.04), (0.05, 0.10), 0.33)]
    for (c, sz, rz) in shape:
        blk(K(M['velvet'], 0), (sz[0], sz[1], 0.006), (c[0], c[1], ZB - 0.0005), rz=rz, bevel=0.001)
        for k in range(12):  # Moosrand: aufgeworfene Lippe um die Mulde
            a = TAU * k / 12
            q = V((math.cos(a) * sz[0] * 0.55, math.sin(a) * sz[1] * 0.58, 0.0))
            q.rotate(Matrix.Rotation(rz, 3, 'Z'))
            ellipsoid(K(M['moss'], 40), V((c[0], c[1], ZB + 0.003)) + q, V((0, 0, 1)), 0.005, 0.008, 6, 3)
    for k in range(6):  # eingetrocknete Blutflecken im Samt und am Moos
        p = V((rng.uniform(-0.08, 0.1), rng.uniform(-0.06, 0.02), ZB + 0.003))
        ellipsoid(K(M['husk_dry'], 30), p, V((0, 0, 1)), 0.0015, rng.uniform(0.005, 0.01), 8, 3)
    for k in range(6):  # abgerissene Wurzelfasern, die ueber den Rand der Mulde haengen
        p0 = V((-0.07 + rng.uniform(-0.012, 0.012), -0.075 + rng.uniform(-0.01, 0.01), ZB + 0.004))
        p1 = p0 + V((rng.uniform(-0.04, 0.04), rng.uniform(-0.035, 0.01), 0.003))
        sweep(K(M['root'], 35), [p0, p0.lerp(p1, 0.5) + V((0, 0, 0.005)), p1], [0.0019, 0.0013, 0.0004], 4)
    # Deckel: Scharnier an der hinteren Oberkante (-y), 98 Grad offen - steht fast senkrecht, leicht nach hinten
    ang = 1.71  # Drehung um x positiv: die Deckelflaeche richtet sich auf (der alte Kasten: -1,9 - Deckel im Boden)
    lid = Matrix.Translation(V((0, -W / 2, H))) @ Matrix.Rotation(ang, 4, 'X')
    LT = 0.016
    blk(K(M['case'], 0), (L, W, LT), tuple(lid @ V((0, W / 2, LT / 2))), rx=ang, bevel=0.003)
    for s in (-1, 1):  # Zarge des Deckels (Rand, 2 cm)
        blk(K(M['case'], 0), (L, T * 0.8, 0.02), tuple(lid @ V((0, W / 2 + s * (W / 2 - T * 0.4), -0.01))), rx=ang, bevel=0.002)
        blk(K(M['case'], 0), (T * 0.8, W, 0.02), tuple(lid @ V((s * (L / 2 - T * 0.4), W / 2, -0.01))), rx=ang, bevel=0.002)
    blk(K(M['moss_dry'], 0), (L - 0.04, W - 0.04, 0.01), tuple(lid @ V((0, W / 2, -0.004))), rx=ang, bevel=0.004)
    # innen am Deckel der Abdruck der Pistole (dunkler, gedrueckter Samt)
    for (c, sz, rz) in shape:
        m3 = lid @ Matrix.Translation(V((c[0], W / 2 + c[1], -0.0095))) @ Matrix.Rotation(rz, 4, 'Z')
        cube(K(M['velvet'], 0), (-sz[0] * 0.47, -sz[1] * 0.47, -0.002), (sz[0] * 0.47, sz[1] * 0.47, 0.002), m3, 0.0006)
    # Messing: Ecken, zwei Scharniere, Strebe (Kasten rechts -> Deckel), offener Riegel vorn
    for sx in (-1, 1):
        for sy in (-1, 1):
            blk(K(M['brass'], 0), (0.026, 0.026, 0.026), (sx * (L / 2 - 0.01), sy * (W / 2 - 0.01), H - 0.011), bevel=0.002)
            blk(K(M['brass'], 0), (0.026, 0.026, 0.02), (sx * (L / 2 - 0.01), sy * (W / 2 - 0.01), 0.012), bevel=0.002)
        cyl(K(M['brass'], 0), V((sx * 0.09 - 0.016, -W / 2 - 0.004, H)), V((sx * 0.09 + 0.016, -W / 2 - 0.004, H)), 0.005, 10)
    s0 = V((L / 2 - T - 0.004, -W / 2 + 0.05, H - 0.012))
    s1 = lid @ V((L / 2 - T - 0.004, W / 2 - 0.07, -0.012))
    cyl(K(M['brass'], 0), s0, s0.lerp(s1, 0.5) + V((0.0, 0.0, 0.004)), 0.0022, 6)
    cyl(K(M['brass'], 0), s0.lerp(s1, 0.5) + V((0.0, 0.0, 0.004)), s1, 0.0022, 6)
    blk(K(M['brass'], 0), (0.03, 0.004, 0.026), (0, W / 2 + 0.002, H - 0.016), bevel=0.001)  # Schliessblech
    hasp = lid @ V((0, W - 0.002, 0.004))
    blk(K(M['brass'], 0), (0.026, 0.004, 0.05), tuple(hasp + V((0, 0, -0.02))), rx=ang + 0.2, bevel=0.001)  # Riegel am Deckel
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
