"""V13.3 Teil 4 Level 5.3: Wissen der Bibliothek und das Notizbuch des Hausherrn (KREATUR_Bibliothekswissen.md,
KREATUR_Gentleman.md; Wikidot level-5-3 Rev. 40, entity-141 Rev. 13, entity-189 Rev. 27, entity-18 Rev. 54 - CC BY-SA 3.0).

Die Seite der Bibliothek nennt Buecher ueber die roten Lichter (Entity 141) und das Volk in den Anzuegen (Entity 189); beide
sind dort nicht anwesend. Gebaut ist, was man in einer Bibliothek findet:
  lore_lectern   Lesepult aus dunklem Nussbaum (Bodenmodell, Leser auf +x): schraeges Brett (20 Grad), darauf ein grosses Folio,
                 rechts eine eingeklebte Tafel - ein dunkler Gang, darin ein rot leuchtender Wuerfel und ein Mensch, der auf ihn
                 zugeht; links Text in fremder Schrift mit Bleistiftnotizen am Rand. Daneben ein altes Diktiergeraet (die
                 Seite nennt eine Aufnahme) und ein Bleistift.
  lore_slip      der Zettel mit der Uebersetzung der Randnotizen (Fund): liegt schraeg auf dem Folio (20 Grad wie das Brett)
  lore_catalog   Katalogschrank (Bodenmodell, Front +x): 4 x 5 Schubladen mit Messingrahmen, eine herausgezogen voller Karten,
                 oben ein Kohleabrieb mit Zeichenreihen und ein Stueck Rohrblech mit eingeschlagenen Zeichen
  lore_cards     ein Buendel Karteikarten mit Schnur (Fund): oben ein Reiter mit Zahnrad und Helm
  gm_notebook    das kleine schwarze Notizbuch, das er hinter sich wirft (Fund; gleiche Masse wie im Modell des Hausherrn)
Konventionen: Meter, z oben, Blick entlang +x; Bodenmodelle Ursprung am Boden.
"""
import math
import random

from mathutils import Matrix, Vector

from models.models_l1_phenomena import ellipsoid
from models.models_l1_sectors import Kit, blk, cyl, sweep
from models.models_l3 import mat

V = Vector
TAU = 2.0 * math.pi
SLANT = math.radians(20.0)  # Brett des Lesepults (gleichlautend: Spawnpunkt des Zettels in tools/levels/level5_3.py)


def _mats():
    return {
        'walnut': mat('wissen_nussbaum', (0.30, 0.18, 0.10), 'wood', 'wood_walnut_floor', r=1.6),
        'walnut_d': mat('wissen_nussbaum_dunkel', (0.18, 0.105, 0.06), 'wood', 'wood_walnut_floor', r=1.9),
        'oak': mat('wissen_eiche', (0.46, 0.33, 0.20), 'wood', 'wood_walnut_floor', r=1.7),
        'oak_d': mat('wissen_eiche_dunkel', (0.30, 0.21, 0.12), 'wood', 'wood_walnut_floor', r=2.0),
        'brass': mat('wissen_messing', (0.70, 0.53, 0.27), 'metal', r=0.6),
        'brass_d': mat('wissen_messing_angelaufen', (0.40, 0.30, 0.16), 'metal', r=0.9, m=0.6),
        'paper': mat('wissen_papier', (0.84, 0.79, 0.66), 'paint', r=1.25),
        'paper_o': mat('wissen_papier_alt', (0.74, 0.65, 0.48), 'paint', r=1.3),
        'card': mat('wissen_karton', (0.93, 0.90, 0.82), 'paint', r=1.2),
        'ink': mat('wissen_tinte', (0.10, 0.08, 0.07), 'paint', r=1.0),
        'pencil': mat('wissen_bleistift', (0.30, 0.30, 0.32), 'paint', r=0.7),
        'charcoal': mat('wissen_kohle', (0.06, 0.06, 0.06), 'paint', r=1.5),
        'plate_d': mat('wissen_tafel_dunkel', (0.05, 0.045, 0.04), 'paint', r=1.2),
        'plate_g': mat('wissen_tafel_gang', (0.20, 0.17, 0.13), 'paint', r=1.2),
        'red_glow': mat('wissen_roter_wuerfel', (1.0, 0.12, 0.06), 'glow', glow=2.4),
        'red_halo': mat('wissen_roter_schein', (0.55, 0.10, 0.06), 'paint', r=1.0),
        'leather': mat('wissen_leder_braun', (0.22, 0.10, 0.06), 'fabric', r=1.0),
        'leather_k': mat('wissen_leder_schwarz', (0.035, 0.03, 0.028), 'fabric', r=0.8),
        'plastic': mat('wissen_kunststoff', (0.05, 0.05, 0.055), 'plastic', r=0.6),
        'chrome': mat('wissen_chrom', (0.62, 0.62, 0.64), 'metal', r=0.4),
        'button_r': mat('wissen_knopf_rot', (0.55, 0.05, 0.04), 'plastic', r=0.5),
        'rust': mat('wissen_rost', (0.95, 0.88, 0.82), 'metal', 'metal_rusty_iron', r=1.3, m=0.15),
        'string': mat('wissen_schnur', (0.62, 0.52, 0.38), 'fabric', r=1.4),
        'ribbon': mat('wissen_lesebaendchen', (0.35, 0.05, 0.05), 'fabric', r=1.0),
    }


def _slant_matrix(center):
    """Ebene des Pultbretts: um y gedreht (Normale nach +x geneigt), Mitte center."""
    return Matrix.Translation(center) @ Matrix.Rotation(SLANT, 4, 'Y')


def _strokes(bm, rng, x0, x1, y0, y1, z, rows, thick=0.0012, gap=0.6, w=0.0016):
    """Schriftzeilen als feine Striche (Laengs x), zufaellige Wortlaengen."""
    for i in range(rows):
        y = y0 + (y1 - y0) * (i + 0.5) / rows
        x = x0
        while x < x1 - 0.01:
            L = rng.uniform(0.008, 0.03)
            L = min(L, x1 - x)
            blk(bm, (L, w, thick), (x + L / 2, y, z))
            x += L + rng.uniform(0.003, 0.006) * (1.0 + gap)


def build_lore_lectern():
    rng = random.Random(14101)
    K = Kit()
    M = _mats()
    # Fuss (Wissen Runde 2: zu duenn, ein Notenstaender): Sockel, Kreuzfuesse mit Klauen, kraeftige gedrechselte Saeule
    blk(K(M['walnut_d'], 25), (0.36, 0.36, 0.07), (0, 0, 0.035), bevel=0.012)
    for a in (0.0, math.pi / 2):
        blk(K(M['walnut_d'], 25), (0.56, 0.08, 0.05), (0, 0, 0.025), rz=a, bevel=0.01)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        ellipsoid(K(M['walnut'], 30), V((0.27 * math.cos(a - math.pi / 4), 0.27 * math.sin(a - math.pi / 4), 0.03)), V((0, 0, 1)), 0.03, 0.035, 10, 6)
    cyl(K(M['walnut'], 30), V((0, 0, 0.07)), V((0, 0, 0.95)), 0.062, 16)
    for z, rr in ((0.11, 0.085), (0.3, 0.075), (0.52, 0.082), (0.74, 0.075), (0.9, 0.088)):
        cyl(K(M['walnut_d'], 30), V((0, 0, z - 0.02)), V((0, 0, z + 0.02)), rr, 16)
    blk(K(M['walnut_d'], 25), (0.3, 0.46, 0.07), (0.0, 0, 0.98), bevel=0.012)
    # schraeges Brett mit Buchleiste
    c = V((0.0, 0.0, 1.07))
    Mt = _slant_matrix(c)
    _tf(K(M['walnut'], 25), Mt, blk, (0.46, 0.64, 0.025), (0, 0, 0), bevel=0.006)
    _tf(K(M['walnut_d'], 25), Mt, blk, (0.03, 0.62, 0.05), (0.235, 0, 0.02), bevel=0.005)
    # Folio: Einband, zwei Seitenbloecke, leicht gewoelbt zur Mitte
    _tf(K(M['leather'], 25), Mt, blk, (0.40, 0.6, 0.008), (0.0, 0, 0.0165), bevel=0.003)
    for s in (1, -1):
        for k in range(4):  # Seitenblock in Stufen (Woelbung zur Mitte)
            _tf(K(M['paper_o'], 20), Mt, blk, (0.37, 0.065, 0.014 - 0.002 * k), (0.0, s * (0.035 + 0.06 * k), 0.026 - 0.001 * k))
    # links Text in fremder Schrift, am Rand Bleistiftnotizen
    _tf(K(M['ink'], 0), Mt, _strokes, rng, -0.16, 0.16, 0.05, 0.26, 0.0335, 18, gap=0.3, w=0.0022)
    _tf(K(M['pencil'], 0), Mt, _strokes, rng, 0.165, 0.185, 0.04, 0.27, 0.0335, 22, w=0.001)
    # rechts die eingeklebte Tafel: dunkler Gang (Fluchtpunkt), roter Wuerfel auf Augenhoehe, ein Mensch geht darauf zu
    _tf(K(M['plate_d'], 0), Mt, blk, (0.24, 0.2, 0.0012), (0.0, -0.15, 0.0335))
    for k in range(5):  # Waende und Boden des Gangs, zur Mitte enger
        t = k / 5.0
        for sg in (1, -1):
            _tf(K(M['plate_g'], 0), Mt, blk, (0.004, 0.18 * (1.0 - t) * 0.5 + 0.01, 0.0008), (-0.10 + 0.04 * k, -0.15 + sg * 0.07 * (1 - t), 0.0343))
    _tf(K(M['red_halo'], 0), Mt, blk, (0.07, 0.07, 0.0008), (0.035, -0.15, 0.0345))
    _tf(K(M['red_glow'], 0), Mt, blk, (0.032, 0.032, 0.0014), (0.035, -0.15, 0.0352))
    _tf(K(M['ink'], 0), Mt, blk, (0.03, 0.009, 0.0008), (-0.05, -0.148, 0.0352))  # Silhouette eines Menschen (Ruecken)
    _tf(K(M['ink'], 0), Mt, blk, (0.009, 0.009, 0.0008), (-0.03, -0.148, 0.0352))
    # Diktiergeraet (schwarz, Lautsprechergitter, roter Knopf) auf der Ecke des Bretts, Bleistift in der Falz
    # Wissen Runde 2: das Diktiergeraet war ein schwarzer Strich - groesser, Chromgitter, Kassettenfenster, rote Leuchte
    Md = Matrix.Translation(V((0.085, -0.255, 1.04))) @ Matrix.Rotation(SLANT, 4, 'Y') @ Matrix.Rotation(0.25, 4, 'Z')
    _tf(K(M['plastic'], 30), Md, blk, (0.07, 0.16, 0.032), (0.0, 0.0, 0.0), bevel=0.007)
    for k in range(7):
        _tf(K(M['chrome'], 20), Md, blk, (0.045, 0.0025, 0.0012), (0.0, 0.03 + 0.0065 * k, 0.0162))
    _tf(K(M['chrome'], 20), Md, blk, (0.05, 0.05, 0.001), (0.0, -0.035, 0.0161))
    _tf(K(M['plate_d'], 0), Md, blk, (0.04, 0.04, 0.0012), (0.0, -0.035, 0.0168))   # Kassettenfenster
    _tf(K(M['red_glow'], 0), Md, cyl, V((0.026, -0.068, 0.0155)), V((0.026, -0.068, 0.0175)), 0.0028, 8)  # rote Leuchte
    _tf(K(M['button_r'], 20), Md, cyl, V((-0.018, -0.068, 0.016)), V((-0.018, -0.068, 0.02)), 0.0065, 10)
    _tf(K(M['oak'], 25), _slant_matrix(c) @ Matrix.Translation(V((0.0, 0.29, 0.034))), cyl, V((-0.15, 0.0, 0.003)), V((0.02, 0.0, 0.003)), 0.0035, 6)
    K.finish()


def _tf(bm, mtx, fn, *a, **kw):
    """Teil mit fn(bm, ...) bauen und nur die neuen Punkte mit mtx bewegen (ein Kit-bmesh sammelt mehrere Teile)."""
    # Wissen Runde 3: mit v.tag markiert loeschte die Fase (bevel) die Marken - der Fuss landete oben auf dem Pult
    import bmesh
    old = set(bm.verts)
    fn(bm, *a, **kw)
    new = [v for v in bm.verts if v not in old]
    bmesh.ops.transform(bm, matrix=mtx, verts=new)


def build_lore_slip():
    """Zettel auf dem Folio (Ursprung: seine Mitte auf der Seite), 20 Grad geneigt wie das Pultbrett."""
    rng = random.Random(14102)
    K = Kit()
    M = _mats()
    Ms = Matrix.Rotation(SLANT, 4, 'Y') @ Matrix.Rotation(-0.12, 4, 'Z')
    # Wissen Runde 2: verschwand auf dem gleichfarbigen Folio - groesser, heller, eine Ecke umgeknickt
    _tf(K(M['card'], 20), Ms, blk, (0.13, 0.17, 0.0014), (0, 0, 0.0007))
    _tf(K(M['card'], 20), Ms, blk, (0.13, 0.004, 0.0026), (0, 0.0, 0.0013))  # Knick
    _tf(K(M['card'], 20), Ms, blk, (0.03, 0.03, 0.0014), (0.052, 0.072, 0.0028), rz=0.785)  # umgeknickte Ecke
    _tf(K(M['pencil'], 0), Ms, _strokes, rng, -0.045, 0.045, -0.065, 0.065, 0.0013, 16, w=0.0011)
    K.finish()


def build_lore_catalog():
    rng = random.Random(18901)
    K = Kit()
    M = _mats()
    W, D, H = 0.76, 0.42, 1.02
    # Korpus auf kurzen Fuessen, Deckplatte mit Profil
    for sx in (1, -1):
        for sy in (1, -1):
            blk(K(M['oak_d'], 25), (0.05, 0.05, 0.08), (sx * (D / 2 - 0.04), sy * (W / 2 - 0.04), 0.04), bevel=0.006)
    blk(K(M['oak'], 25), (D, W, H - 0.1), (0, 0, 0.08 + (H - 0.1) / 2), bevel=0.006)
    blk(K(M['oak_d'], 25), (D + 0.03, W + 0.03, 0.025), (0, 0, H - 0.0125), bevel=0.008)
    # Schubladen 4 x 5 mit Messingrahmen fuer das Schild und Griff; eine (Reihe 1, Spalte 2) herausgezogen
    cols, rows = 4, 5
    cw, rh = (W - 0.06) / cols, (H - 0.18) / rows
    for i in range(cols):
        for j in range(rows):
            y = -(W - 0.06) / 2 + cw * (i + 0.5)
            z = 0.12 + rh * (j + 0.5)
            out = 0.25 if (i == 2 and j == 3) else 0.0
            x = D / 2 + out
            blk(K(M['oak'], 25), (0.012, cw - 0.012, rh - 0.012), (x + 0.004, y, z), bevel=0.003)
            blk(K(M['brass_d'] if rng.random() < 0.5 else M['brass'], 25), (0.004, 0.06, 0.03), (x + 0.012, y, z + 0.03))
            blk(K(M['card'], 0), (0.002, 0.05, 0.022), (x + 0.0135, y, z + 0.03))
            cyl(K(M['brass'], 30), V((x + 0.01, y, z - 0.015)), V((x + 0.03, y, z - 0.015)), 0.008, 10)
            if out > 0:  # der Kasten der Schublade mit Karten, ein Reiter ragt heraus
                blk(K(M['oak'], 25), (out + 0.02, 0.008, rh - 0.03), (D / 2 + out / 2 - 0.005, y + cw / 2 - 0.012, z - 0.004))
                blk(K(M['oak'], 25), (out + 0.02, 0.008, rh - 0.03), (D / 2 + out / 2 - 0.005, y - cw / 2 + 0.012, z - 0.004))
                blk(K(M['oak'], 25), (out + 0.02, cw - 0.02, 0.008), (D / 2 + out / 2 - 0.005, y, z - rh / 2 + 0.015))
                for k in range(26):
                    xx = D / 2 + 0.015 + out * k / 27.0
                    tilt = rng.uniform(-0.08, 0.08)
                    blk(K(M['card'] if k % 7 else M['paper_o'], 20), (0.0012, cw - 0.03, rh - 0.05), (xx, y, z - 0.006), ry=tilt)
                blk(K(M['paper_o'], 20), (0.0015, 0.03, 0.03), (D / 2 + out * 0.5, y + 0.03, z + rh / 2 - 0.01))  # Reiter
    # oben: Kohleabrieb (Zeichen in Reihen, in der Mitte eine Kugel aus Strahlen) und ein Stueck Rohrblech
    blk(K(M['paper'], 20), (0.3, 0.22, 0.0012), (-0.02, 0.17, H + 0.0006), rz=0.15)
    ch = K(M['charcoal'], 0)
    for r_ in range(6):
        for k in range(9):
            if 2 <= r_ <= 3 and 3 <= k <= 5:
                continue
            blk(ch, (0.012, 0.012, 0.0006), (-0.11 + 0.024 * r_ + 0.003, 0.08 + 0.022 * k, H + 0.0015), rz=0.15 + rng.uniform(-0.4, 0.4))
    for a in range(10):  # Kugel aus Licht mit Strahlen
        ang = TAU * a / 10
        blk(ch, (0.024, 0.0025, 0.0006), (-0.02 + 0.016 * math.cos(ang), 0.17 + 0.016 * math.sin(ang), H + 0.0016), rz=ang)
    sw = K(M['rust'], 20)
    blk(sw, (0.09, 0.07, 0.006), (-0.08, 0.28, H + 0.004), rz=-0.4, bevel=0.002)
    K.finish()


def build_lore_cards():
    """Buendel Karteikarten mit Schnur (Ursprung unten Mitte), oben ein Reiter mit Zahnrad und Helm."""
    rng = random.Random(18902)
    K = Kit()
    M = _mats()
    for k in range(7):
        blk(K(M['card'] if k % 3 else M['paper_o'], 20), (0.075, 0.125, 0.0011), (rng.uniform(-0.004, 0.004), rng.uniform(-0.004, 0.004), 0.0006 + 0.0012 * k),
            rz=rng.uniform(-0.06, 0.06))
    top = 0.0006 + 0.0012 * 7
    ink = K(M['ink'], 0)
    _strokes(ink, rng, -0.03, 0.035, -0.055, 0.02, top + 0.0002, 7, w=0.001)
    for a in range(10):  # Zahnrad
        ang = TAU * a / 10
        blk(ink, (0.006, 0.0018, 0.0004), (0.0, 0.04 + 0.009 * math.cos(ang), top + 0.0003 + 0.0 * a), rz=ang)
    cyl(ink, V((0.0, 0.04, top)), V((0.0, 0.04, top + 0.0006)), 0.007, 14)
    cyl(K(M['card'], 0), V((0.0, 0.04, top + 0.0005)), V((0.0, 0.04, top + 0.0009)), 0.004, 10)
    blk(ink, (0.012, 0.01, 0.0005), (0.022, 0.04, top + 0.0003))  # Helm daneben
    st = K(M['string'], 30)
    for y in (-0.03, 0.03):
        sweep(st, [V((-0.04, y, -0.0005)), V((-0.039, y, top + 0.001)), V((0.039, y, top + 0.001)), V((0.04, y, -0.0005))], 0.0011, 5)
    K.finish()


def build_gm_notebook():
    """Das kleine schwarze Notizbuch (13 x 19 x 2,2 cm, Ursprung unten Mitte, Ruecken bei -x) - wie im Modell des Hausherrn."""
    K = Kit()
    M = _mats()
    lk = K(M['leather_k'], 30)
    blk(lk, (0.13, 0.19, 0.003), (0, 0, 0.0015), bevel=0.002)
    blk(lk, (0.13, 0.19, 0.003), (0, 0, 0.0205), bevel=0.002)
    blk(lk, (0.006, 0.19, 0.022), (-0.062, 0, 0.011), bevel=0.003)
    blk(K(M['paper_o'], 20), (0.124, 0.182, 0.0165), (0.002, 0, 0.011))
    for y in (0.07, -0.08):  # abgestossene Ecken
        blk(K(M['leather'], 20), (0.012, 0.012, 0.0032), (0.058, y, 0.021), rz=0.7)
    sweep(K(M['ribbon'], 0), [V((0.02, 0.0, 0.011)), V((0.03, -0.1, 0.006)), V((0.045, -0.13, 0.0008))], 0.0025, 4, profile=None)
    K.finish()
