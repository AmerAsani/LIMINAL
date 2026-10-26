"""V12 Level 36 (LOOK_36.md): Timeless Airport - Gepaeckband, Fundsachen, Check-in-Schalter, Abflugtafel, BTA-Stand,
Laufband, Wegweiser (auch widerspruechlich und leer), Ladenfront, Sitzreihe mit Tischchen, Gate-Schalter, Gate-Schilder,
Gate-Tuer (eine im Boarding), Uhr ohne Zeiger, Trinkbrunnen, Toilettenschild, Habseligkeiten an Gate C32, Ausgangsschild.

Schrift: Inter (SIL OFL 1.1) aus den Blender-Datendateien, als Geometrie (LICENSES.md).
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import common
from models.models_l2 import _cyl
from models.models_l5 import _text


def _m():
    return {
        'steel': common.material('edelstahl', (0.72, 0.73, 0.74), look='metal', tex='metal_aluminum', rough=0.3, metal=0.9),
        'dark': common.material('anthrazit', (0.14, 0.15, 0.16), look='paint', rough=0.5),
        'rubber': common.material('gummi', (0.07, 0.07, 0.08), look='rubber', rough=0.85),
        'white': common.material('weiss_lack', (0.88, 0.88, 0.86), look='paint', rough=0.4),
        'glass': common.material('glas', (0.55, 0.62, 0.64), look='glass', rough=0.05),
        'screen': common.material('bildschirm', (0.06, 0.12, 0.28), look='glow', glow=0.9),
        'amber': common.material('bernstein', (1.0, 0.72, 0.18), look='glow', glow=1.4),
        'white_glow': common.material('schrift_hell', (0.95, 0.97, 1.0), look='glow', glow=1.3),
        'yellow': common.material('wegweiser_gelb', (0.98, 0.80, 0.12), look='glow', glow=0.9),
        'purple': common.material('bta_violett', (0.42, 0.18, 0.62), look='glow', glow=0.9),
        'seat': common.material('sitzschale', (0.20, 0.30, 0.44), look='plastic', rough=0.5),
        'wood': common.material('holz_hell', (0.62, 0.48, 0.32), look='wood', tex='wood_birch', rough=0.5),
        'fabric_r': common.material('stoff_rot', (0.50, 0.12, 0.10), look='fabric', rough=0.9),
        'fabric_g': common.material('stoff_gruen', (0.18, 0.34, 0.24), look='fabric', rough=0.9),
        'fabric_b': common.material('stoff_blau', (0.16, 0.22, 0.42), look='fabric', rough=0.9),
        'card': common.material('pappe', (0.66, 0.52, 0.34), look='paint', rough=0.9),
        'black': common.material('schwarz', (0.04, 0.04, 0.04), look='paint', rough=0.4),
    }


def _textw(name, body, size, mat, x, y, z, depth=0.0):
    """Text auf einer Flaeche, die entlang +x blickt (Mitte bei (x, y, z)); flach, grobe Kurven (Dreiecke sparen)."""
    return _text(name, body, size, mat, Vector((x, y, z)), depth=depth, res=4)


def _suitcase(name, x, y, z, yaw, w, d, h, mat, M):
    ob = common.box(name, (-w / 2, -d / 2, 0.0), (w / 2, d / 2, h), mat, bevel=0.03, segments=2)
    ob.matrix_world = Matrix.Translation((x, y, z)) @ Matrix.Rotation(yaw, 4, 'Z')
    hd = common.box(name + '_griff', (-0.08, -0.015, h), (0.08, 0.015, h + 0.04), M['black'])
    hd.matrix_world = ob.matrix_world


# --- Gepaeckausgabe ----------------------------------------------------------------------------------------------------
def build_carousel_36():
    """Gepaeckband (Boden, 12 x 3,6 m, entlang x): Edelstahlsockel, Gummilamellen, Mittelwand mit Nummernschild
    (Zug-Bildzeichen: dieses Band gehoert zum Eingang aus der Metro), einige Koffer (Wiki: Ankunft ueber Gepaeckbaender)."""
    M = _m()
    L, W = 12.0, 3.6
    r = W / 2
    common.box('sockel_mitte', (-L / 2 + r, -r, 0.0), (L / 2 - r, r, 0.45), M['steel'])
    for s in (-1, 1):
        _cyl(f'sockel_ende{s}', (s * (L / 2 - r), 0, 0.0), (s * (L / 2 - r), 0, 0.45), r, M['steel'], seg=32)
    common.box('band_mitte', (-L / 2 + r, -r + 0.05, 0.45), (L / 2 - r, r - 0.05, 0.5), M['rubber'])
    for s in (-1, 1):
        _cyl(f'band_ende{s}', (s * (L / 2 - r), 0, 0.45), (s * (L / 2 - r), 0, 0.5), r - 0.05, M['rubber'], seg=32)
    common.box('mittelwand', (-L / 2 + r + 0.5, -0.15, 0.5), (L / 2 - r - 0.5, 0.15, 1.3), M['dark'], bevel=0.02)
    common.box('schild', (-0.6, -0.16, 1.3), (0.6, 0.16, 1.9), M['dark'], bevel=0.02)
    for s in (-1, 1):
        x = s * 0.161
        common.box(f'anzeige{s}', (-0.55, min(x, x + s * 0.002), 1.35), (0.55, max(x, x + s * 0.002), 1.85), M['screen'])
    rng = random.Random(36)
    mats = [M['fabric_r'], M['fabric_b'], M['dark'], M['fabric_g']]
    for k in range(5):
        x = rng.uniform(-L / 2 + 1.0, L / 2 - 1.0)
        y = rng.choice((-1, 1)) * rng.uniform(1.0, 1.4)
        _suitcase(f'koffer{k}', x, y, 0.5, rng.uniform(-0.4, 0.4), 0.7, 0.45, 0.28, mats[k % 4], M)


def build_luggage_pile():
    """Fundsachen (Boden, Regal 3 m mit Koffern und Taschen; Wiki: nach zehn Runden wandert Gepaeck zu Lost and Found)."""
    M = _m()
    rng = random.Random(37)
    for z in (0.0, 0.8, 1.6):
        common.box(f'boden{z}', (-1.5, -0.35, z + 0.0), (1.5, 0.35, z + 0.04), M['steel'])
    for (x, y) in ((-1.5, -0.35), (1.5, -0.35), (-1.5, 0.35), (1.5, 0.35)):
        common.box(f'pfosten{x}{y}', (x - 0.02, y - 0.02, 0.0), (x + 0.02, y + 0.02, 2.2), M['steel'])
    mats = [M['fabric_r'], M['fabric_b'], M['dark'], M['fabric_g'], M['card']]
    for z in (0.04, 0.84, 1.64):
        x = -1.35
        while x < 1.2:
            w = rng.uniform(0.35, 0.6)
            _suitcase(f'stueck{z}{x}', x + w / 2, rng.uniform(-0.05, 0.05), z, rng.uniform(-0.1, 0.1), w, rng.uniform(0.35, 0.55), rng.uniform(0.25, 0.6),
                      mats[rng.randrange(5)], M)
            x += w + 0.06


# --- Check-in ----------------------------------------------------------------------------------------------------------
def build_checkin_counter():
    """Check-in-Schalter (Boden, 2,4 m entlang y, Gast steht bei +x): Tresen, Bildschirm, Gepaeckwaage, Trennwand,
    Nummer (Wiki: endlose Reihe von Schaltern mit Informationsbildschirmen)."""
    M = _m()
    common.box('tresen', (-0.3, -0.9, 0.0), (0.3, 0.9, 1.05), M['white'], bevel=0.015)
    common.box('platte', (-0.35, -0.95, 1.05), (0.38, 0.95, 1.09), M['wood'], bevel=0.01)
    common.box('waage', (0.4, -0.95, 0.0), (1.1, -0.3, 0.32), M['steel'])
    common.box('band', (0.42, -0.93, 0.32), (1.08, -0.32, 0.34), M['rubber'])
    common.box('trennwand', (-0.4, 1.0, 0.0), (0.8, 1.15, 1.8), M['dark'], bevel=0.01)
    common.box('monitor', (-0.15, 0.1, 1.09), (-0.1, 0.6, 1.42), M['dark'])
    common.box('monitor_bild', (-0.1, 0.13, 1.12), (-0.098, 0.57, 1.39), M['screen'])
    common.box('nummer_kasten', (-0.4, -0.5, 1.9), (-0.3, 0.5, 2.3), M['dark'])
    _textw('nummer', '36', 0.26, M['white_glow'], -0.299, 0.0, 2.1)
    _cyl('nummer_stange', (-0.35, 0.0, 1.09), (-0.35, 0.0, 1.9), 0.02, M['steel'], seg=8)


def build_departure_board():
    """Abflugtafel (Wandteil, 4 x 2 m): Kopfzeile, Zeilen mit Flugnummer, Ziel, Gate, Status, Countdown (Wiki: Fluege
    werden ab 1000 aufgerufen und bei 0 geschlossen; C32 wartet auf Flug 1784XBA8JLP nach Level 186)."""
    M = _m()
    common.box('rahmen', (0.0, -2.05, -1.05), (0.12, 2.05, 1.05), M['dark'], bevel=0.02)
    common.box('flaeche', (0.12, -1.98, -0.98), (0.125, 1.98, 0.98), common.material('tafel', (0.02, 0.03, 0.06), look='paint', rough=0.4))
    _textw('kopf', 'ABFLUG  DEPARTURES', 0.17, M['white_glow'], 0.126, 0.0, 0.78)
    rows = [('1784XBA8JLP', '186', 'C32', 'WARTEN'), ('QX 0903', '36.1', 'C33', 'BOARDING 1000'), ('BT 0011', '11', 'M14', '0412'),
            ('ZZ 0000', '---', '---', '----'), ('BT 0185', '185', 'A07', 'VERSPAETET'), ('KE 0042', '???', 'B??', '0000')]
    for k, (f, d, g, st) in enumerate(rows):
        z = 0.48 - k * 0.27
        mat = M['amber'] if k != 1 else common.material('boarding_gruen', (0.4, 1.0, 0.5), look='glow', glow=1.4)
        # Spalten von links nach rechts im Spiel (der Import spiegelt y): Flug, Ziel, Gate, Status
        _textw(f'f{k}', f, 0.12, mat, 0.126, -1.25, z)
        _textw(f'd{k}', d, 0.12, mat, 0.126, -0.3, z)
        _textw(f'g{k}', g, 0.12, mat, 0.126, 0.35, z)
        _textw(f's{k}', st, 0.12, mat, 0.126, 1.35, z)


def build_bta_booth():
    """Stand der Backrooms Travel Agency (Boden, 3 x 2 m): Tresen, violette Leuchtschrift BTA (Wiki: Hauptstand im
    Check-in, violette Beschilderung)."""
    M = _m()
    common.box('tresen', (-1.5, -0.4, 0.0), (1.5, 0.4, 1.05), M['white'], bevel=0.015)
    common.box('front', (-1.5, 0.4, 0.1), (1.5, 0.42, 0.95), M['purple'])
    common.box('rueckwand', (-1.5, -1.0, 0.0), (1.5, -0.9, 2.6), M['dark'])
    _cyl('stange_l', (-1.4, -0.95, 2.6), (-1.4, -0.95, 3.0), 0.02, M['steel'], seg=8)
    _cyl('stange_r', (1.4, -0.95, 2.6), (1.4, -0.95, 3.0), 0.02, M['steel'], seg=8)
    common.box('schild', (-1.5, -1.0, 3.0), (1.5, -0.9, 3.5), M['dark'])
    for sg in (-1, 1):  # Schrift auf beiden Seiten des Schilds
        ob = _text(f'bta{sg}', 'BTA', 0.36, M['purple'], Vector((0.0, 0.0, 0.0)), depth=0.0, res=3)
        ob.matrix_world = Matrix.Translation((0.0, -0.95 + sg * 0.051, 3.25)) @ Matrix.Rotation(sg * math.pi / 2, 4, 'Z') @ ob.matrix_world


# --- Wege -----------------------------------------------------------------------------------------------------------------
def _travelator(M, stopped=False):
    L = 12.0
    common.box('band', (-L / 2, -0.6, 0.0), (L / 2, 0.6, 0.12), M['steel'])
    for k in range(int(L / 0.04)):
        x = -L / 2 + k * 0.04
        if k % 3 == 0:
            common.box(f'rille{k}', (x, -0.58, 0.12), (x + 0.012, 0.58, 0.125), M['dark'])
    for s in (-1, 1):
        common.box(f'sockel{s}', (-L / 2, s * 0.6 - (0.1 if s < 0 else 0), 0.0), (L / 2, s * 0.6 + (0.1 if s > 0 else 0), 0.35), M['steel'])
        common.box(f'glas{s}', (-L / 2 + 0.3, s * 0.65 - 0.01, 0.35), (L / 2 - 0.3, s * 0.65 + 0.01, 1.0), M['glass'])
        common.box(f'handlauf{s}', (-L / 2 + 0.2, s * 0.65 - 0.04, 1.0), (L / 2 - 0.2, s * 0.65 + 0.04, 1.06), M['rubber'], bevel=0.02)
    for s in (-1, 1):
        common.box(f'kamm{s}', (s * L / 2 - (0.3 if s > 0 else 0), -0.6, 0.0), (s * L / 2 + (0.0 if s > 0 else 0.3), 0.6, 0.13), M['yellow'] if not stopped else M['dark'])
    if stopped:  # Aufsteller: Laufband steht (Wiki: stotternde oder stehende Laufbaender meiden)
        common.box('aufsteller', (-L / 2 - 0.6, -0.3, 0.0), (-L / 2 - 0.55, 0.3, 1.0), M['yellow'])
        common.box('aufsteller_x', (-L / 2 - 0.551, -0.25, 0.55), (-L / 2 - 0.549, 0.25, 0.6), M['black'])


def build_travelator():
    """Laufband (Boden, 12 m entlang x, 1,2 m breit, Glasbalustraden; Wiki: fuer lange Wege, oft nur in eine Richtung)."""
    _travelator(_m())


def build_travelator_stopped():
    _travelator(_m(), stopped=True)


def _wayfinder(lines, M, empty=False):
    """Haengender Wegweiser (Deckenteil, Decke bei z = 0, 3 m entlang y): Pendel, Kasten, gelbe Schrift auf Anthrazit."""
    for s in (-1, 1):
        _cyl(f'pendel{s}', (0.0, s * 1.2, -0.7), (0.0, s * 1.2, 0.0), 0.012, M['steel'], seg=6)
    common.box('kasten', (-0.08, -1.5, -1.25), (0.08, 1.5, -0.7), M['dark'], bevel=0.01)
    if empty:
        return
    for sg in (-1, 1):
        x = sg * 0.081
        for (body, y) in lines:
            ob = _text(f'z{sg}{y}', body, 0.21, M['yellow'], Vector((x, y, -0.975)), depth=0.0, res=5)
            if sg < 0:
                ob.matrix_world = Matrix.Translation((x, y, -0.975)) @ Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Translation((-x, -y, 0.975)) @ ob.matrix_world


def build_sign_36_a():
    """Wegweiser: Gates links, Gepaeck rechts."""
    _wayfinder([('<  GATES A-M', 0.72), ('GEPAECK  >', -0.78)], _m())


def build_sign_36_b():
    """Widerspruechlicher Wegweiser (Wiki: Schilder widersprechen sich): Gates rechts, Gepaeck links."""
    _wayfinder([('GATES A-M  >', -0.72), ('<  GEPAECK', 0.78)], _m())


def build_sign_36_empty():
    """Leerer Wegweiser (Wiki: leere oder kaputte Schilder markieren Sackgassen)."""
    _wayfinder([], _m(), empty=True)


def build_exit_sign_36():
    """Ausgangsschild mit Pfeil (Wandteil; Wiki: Ausgangsschildern nicht trauen - es gibt keinen echten Ausgang)."""
    M = _m()
    green = common.material('ausgang_gruen', (0.20, 0.85, 0.35), look='glow', glow=1.3)
    common.box('kasten', (0.0, -0.6, -0.2), (0.08, 0.6, 0.2), M['dark'], bevel=0.01)
    common.box('flaeche', (0.08, -0.56, -0.16), (0.083, 0.56, 0.16), green)
    _textw('text', 'EXIT  >', 0.17, M['black'], 0.084, 0.0, 0.0)


# --- Laeden ---------------------------------------------------------------------------------------------------------------
def _storefront(name, M, glow_rgb, seed):
    """Ladenfront (Wandteil, 6 m entlang y): Pfeiler, Schaufenster, Glastuer, Leuchtkasten mit Namen, flache Auslage
    mit Regalen und Waren dahinter (Wiki: Laeden wie in der Frontrooms-Welt, meist wertlose Ware)."""
    rng = random.Random(seed)
    lit = common.material(f'laden_licht{seed}', glow_rgb, look='glow', glow=0.9)
    common.box('rueck', (0.0, -3.0, 0.0), (0.02, 3.0, 3.2), M['white'])
    for y in (-3.0, 0.6, 2.85):
        common.box(f'pfeiler{y}', (0.0, y, 0.0), (0.9, y + 0.15, 3.2), M['dark'])
    common.box('sockel', (0.0, -3.0, 0.0), (0.9, 3.0, 0.35), M['dark'])
    common.box('kasten', (0.0, -3.0, 3.2), (0.95, 3.0, 3.8), M['dark'])
    common.box('leucht', (0.95, -2.9, 3.28), (0.955, 2.9, 3.72), lit)
    _textw('name', name, 0.3, M['black'], 0.956, 0.0, 3.5)
    common.box('scheibe', (0.88, -2.85, 0.35), (0.9, 0.6, 3.2), M['glass'])
    common.box('tuer', (0.85, 0.75, 0.0), (0.87, 2.85, 2.6), M['glass'])
    common.box('tuergriff', (0.87, 0.85, 1.0), (0.9, 0.88, 1.6), M['steel'])
    goods = [M['fabric_r'], M['fabric_b'], M['fabric_g'], M['card'], M['white']]
    for z in (0.5, 1.2, 1.9, 2.6):
        common.box(f'regal{z}', (0.02, -2.85, z), (0.6, 0.55, z + 0.03), M['wood'])
        y = -2.8
        while y < 0.4:
            w = rng.uniform(0.12, 0.3)
            h = rng.uniform(0.15, 0.45)
            common.box(f'ware{z}{y}', (0.08, y, z + 0.03), (0.5, y + w, z + 0.03 + h), goods[rng.randrange(5)], bevel=0.01)
            y += w + rng.uniform(0.03, 0.12)
    common.box('innen_licht', (0.02, -2.85, 3.1), (0.6, 0.55, 3.15), lit)


def build_storefront_a():
    _storefront('ZEITUNGEN', _m(), (1.0, 0.96, 0.86), 1)


def build_storefront_b():
    _storefront('KAFFEE', _m(), (1.0, 0.86, 0.66), 2)


def build_storefront_c():
    _storefront('MODE', _m(), (0.92, 0.96, 1.0), 3)


def build_storefront_closed():
    """Geschlossener Laden: Rollgitter herab."""
    M = _m()
    common.box('rueck', (0.0, -3.0, 0.0), (0.02, 3.0, 3.8), M['dark'])
    common.box('kasten', (0.0, -3.0, 3.2), (0.95, 3.0, 3.8), M['dark'])
    for k in range(32):
        z = 0.1 * k
        common.box(f'lamelle{k}', (0.85, -2.9, z), (0.88, 2.9, z + 0.08), M['steel'])


# --- Gates ------------------------------------------------------------------------------------------------------------------
def build_seats_row():
    """Sitzreihe am Gate (Boden, 4 Sitze auf Traverse, 2,4 m entlang y, Sitzflaeche nach +x) mit Tischchen am Ende."""
    M = _m()
    common.box('traverse', (-0.1, -1.3, 0.3), (0.1, 1.3, 0.38), M['steel'])
    for y in (-1.2, 1.2):
        common.box(f'fuss{y}', (-0.3, y - 0.04, 0.0), (0.3, y + 0.04, 0.38), M['steel'])
    for k in range(4):
        y = -0.9 + k * 0.6
        common.box(f'sitz{k}', (-0.25, y - 0.27, 0.42), (0.25, y + 0.27, 0.48), M['seat'], bevel=0.02)
        ob = common.box(f'lehne{k}', (-0.03, y - 0.27, 0.0), (0.03, y + 0.27, 0.5), M['seat'], bevel=0.02)
        ob.matrix_world = Matrix.Translation((-0.26, 0.0, 0.46)) @ Matrix.Rotation(-0.2, 4, 'Y')
        if k < 3:
            common.box(f'armlehne{k}', (-0.2, y + 0.27, 0.42), (0.15, y + 0.33, 0.68), M['steel'])
    common.box('tisch', (-0.25, 1.35, 0.0), (0.25, 1.85, 0.03), M['steel'])
    common.box('tisch_bein', (-0.03, 1.57, 0.03), (0.03, 1.63, 0.62), M['steel'])
    common.box('tisch_platte', (-0.25, 1.35, 0.62), (0.25, 1.85, 0.65), M['wood'])


def build_gate_counter():
    """Gate-Schalter (Boden, 2 m entlang y) mit Bildschirm."""
    M = _m()
    common.box('tresen', (-0.35, -1.0, 0.0), (0.35, 1.0, 1.05), M['white'], bevel=0.015)
    common.box('platte', (-0.4, -1.05, 1.05), (0.42, 1.05, 1.09), M['dark'])
    common.box('monitor', (0.0, -0.3, 1.09), (0.05, 0.3, 1.4), M['dark'])
    common.box('monitor_bild', (0.05, -0.27, 1.12), (0.052, 0.27, 1.37), M['screen'])


def _gate_sign(number):
    """Gate-Schild (Wandteil, 2 x 0,8 m) mit Nummer."""
    M = _m()
    common.box('kasten', (0.0, -1.0, -0.4), (0.1, 1.0, 0.4), M['dark'], bevel=0.015)
    common.box('feld', (0.1, -0.95, -0.35), (0.104, -0.15, 0.35), M['yellow'])
    _textw('gate', 'GATE', 0.16, M['black'], 0.105, -0.55, 0.0)
    _textw('nummer', number, 0.42, M['yellow'], 0.105, 0.45, 0.0)


def build_gate_sign_c30():
    _gate_sign('C30')


def build_gate_sign_c31():
    _gate_sign('C31')


def build_gate_sign_c32():
    _gate_sign('C32')


def build_gate_sign_c33():
    _gate_sign('C33')


def build_gate_sign_c34():
    _gate_sign('C34')


def _gate_door(boarding):
    """Tuer zur Fluggastbruecke (Wandteil, Boden z = 0): Stahlrahmen, Tuer mit Fenster, Statusleuchte; boarding: offen,
    dahinter das helle Innere der Bruecke (Wiki: jede Gate-Tuer fuehrt in Sublevel 36.1, das Flugzeug)."""
    M = _m()
    W, H = 1.6, 2.4
    for (lo, hi) in (((0.0, W / 2, 0.0), (0.12, W / 2 + 0.15, H + 0.15)), ((0.0, -W / 2 - 0.15, 0.0), (0.12, -W / 2, H + 0.15)),
                     ((0.0, -W / 2 - 0.15, H), (0.12, W / 2 + 0.15, H + 0.15))):
        common.box('rahmen', lo, hi, M['steel'], bevel=0.01)
    if boarding:
        inside = common.material('bruecke_licht', (0.95, 0.97, 1.0), look='glow', glow=1.6)
        common.box('bruecke', (0.0, -W / 2, 0.0), (0.01, W / 2, H), inside)
        ob = common.box('fluegel', (0.0, 0.0, 0.0), (0.05, W - 0.04, H - 0.02), M['dark'])
        ob.matrix_world = Matrix.Translation((0.1, -W / 2 + 0.02, 0.01)) @ Matrix.Rotation(math.radians(70), 4, 'Z')
        lamp = common.material('status_gruen', (0.3, 1.0, 0.45), look='glow', glow=2.0)
    else:
        common.box('fluegel', (0.0, -W / 2, 0.0), (0.06, W / 2, H), M['dark'])
        common.box('fenster', (0.06, -0.25, 1.3), (0.062, 0.25, 1.9), M['black'])
        common.box('stange', (0.06, -0.6, 1.0), (0.12, 0.6, 1.06), M['steel'])
        lamp = common.material('status_rot', (1.0, 0.2, 0.15), look='glow', glow=1.6)
    common.box('statusleuchte', (0.12, -0.25, H + 0.2), (0.16, 0.25, H + 0.32), lamp)


def build_gate_door_closed():
    _gate_door(False)


def build_gate_door_boarding():
    _gate_door(True)


def build_clock_36():
    """Wanduhr ohne Zeiger (Wandteil; Wiki: Uhren zeigen zufaellige Zeiten oder haben keine Zeiger)."""
    M = _m()
    _cyl('gehaeuse', (0.0, 0, 0), (0.08, 0, 0), 0.32, M['dark'], seg=40)
    _cyl('blatt', (0.08, 0, 0), (0.082, 0, 0), 0.29, M['white'], seg=40)
    for k in range(12):
        a = k * math.pi / 6
        common.box(f'strich{k}', (0.082, math.cos(a) * 0.24 - 0.008, math.sin(a) * 0.24 - (0.04 if k % 3 == 0 else 0.02)),
                   (0.084, math.cos(a) * 0.24 + 0.008, math.sin(a) * 0.24 + (0.04 if k % 3 == 0 else 0.02)), M['black'])


def build_clock_36_b():
    """Wanduhr mit stehenden Zeigern auf einer zufaelligen Zeit."""
    build_clock_36()
    M = _m()
    for (ln, a, w) in ((0.16, 2.3, 0.012), (0.24, -0.9, 0.008)):
        ob = common.box('zeiger', (0.0, -w, 0.0), (0.003, w, ln), M['black'])
        ob.matrix_world = Matrix.Translation((0.085, 0.0, 0.0)) @ Matrix.Rotation(a, 4, 'X')


def build_water_fountain():
    """Trinkbrunnen (Wandteil, Edelstahl; Wiki: Trinkbrunnen an den Gates)."""
    M = _m()
    common.box('becken', (0.0, -0.3, 0.75), (0.45, 0.3, 0.95), M['steel'], bevel=0.04, segments=3)
    common.box('saeule', (0.0, -0.15, 0.0), (0.2, 0.15, 0.75), M['steel'])
    _cyl('hahn', (0.3, 0.0, 0.95), (0.3, 0.0, 1.02), 0.015, M['steel'], seg=8)


def build_restroom_sign():
    """Toilettenschild mit Bildzeichen (Wandteil)."""
    M = _m()
    common.box('kasten', (0.0, -0.45, -0.25), (0.06, 0.45, 0.25), M['dark'], bevel=0.01)
    for (y, skirt) in ((-0.2, False), (0.2, True)):
        _cyl(f'kopf{y}', (0.06, y, 0.12), (0.064, y, 0.12), 0.045, M['white_glow'], seg=16)
        pts = [(y - 0.06, -0.18), (y + 0.06, -0.18), (y + 0.06, 0.05), (y - 0.06, 0.05)] if not skirt else \
              [(y - 0.09, -0.08), (y + 0.09, -0.08), (y + 0.05, 0.05), (y - 0.05, 0.05)]
        bm = bmesh.new()
        f = bm.faces.new([bm.verts.new((0.064, a, b)) for (a, b) in pts])
        f.normal_update()
        if f.normal.x < 0:
            f.normal_flip()
        common.obj_from_bmesh(f'koerper{y}', bm, M['white_glow'])
    common.box('trenner', (0.06, -0.005, -0.2), (0.064, 0.005, 0.2), M['white_glow'])


def build_camp_c32():
    """Habseligkeiten der Wartenden an Gate C32 (Boden, 4 x 3 m): Decken, Taschen, Becher, Thermoskanne, ein Pappschild
    mit der Flugnummer (Wiki: rund 150 Fluggaeste warten dort auf Flug 1784XBA8JLP nach Level 186)."""
    M = _m()
    rng = random.Random(32)
    for k in range(4):
        x, y = rng.uniform(-1.6, 1.6), rng.uniform(-1.2, 1.2)
        ob = common.box(f'decke{k}', (-0.8, -0.6, 0.0), (0.8, 0.6, 0.04), [M['fabric_r'], M['fabric_g'], M['fabric_b']][k % 3], bevel=0.02)
        ob.matrix_world = Matrix.Translation((x, y, 0.0)) @ Matrix.Rotation(rng.uniform(0, 3), 4, 'Z')
    for k in range(6):
        _suitcase(f'tasche{k}', rng.uniform(-1.8, 1.8), rng.uniform(-1.4, 1.4), 0.0, rng.uniform(0, 3), 0.45, 0.25, 0.3,
                  [M['dark'], M['fabric_b'], M['card']][k % 3], M)
    for k in range(5):
        x, y = rng.uniform(-1.5, 1.5), rng.uniform(-1.2, 1.2)
        _cyl(f'becher{k}', (x, y, 0.0), (x, y, 0.11), 0.04, M['white'], seg=12)
    _cyl('thermos', (0.4, -0.3, 0.0), (0.4, -0.3, 0.32), 0.05, M['steel'], seg=14)
    ob = common.box('schild', (-0.02, -0.5, 0.0), (0.02, 0.5, 0.5), M['card'])
    ob.matrix_world = Matrix.Translation((-1.9, 0.0, 0.0)) @ Matrix.Rotation(-0.25, 4, 'Y')
    ob2 = _text('schild_text', '1784XBA8JLP', 0.09, M['black'], Vector((0.0, 0.0, 0.0)), depth=0.0, res=2)
    ob2.matrix_world = Matrix.Translation((-1.9, 0.0, 0.0)) @ Matrix.Rotation(-0.25, 4, 'Y') @ Matrix.Translation((0.021, 0.0, 0.32)) @ Matrix.Rotation(math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X')
    ob3 = _text('schild_text2', '-> 186', 0.09, M['black'], Vector((0.0, 0.0, 0.0)), depth=0.0, res=2)
    ob3.matrix_world = Matrix.Translation((-1.9, 0.0, 0.0)) @ Matrix.Rotation(-0.25, 4, 'Y') @ Matrix.Translation((0.021, 0.0, 0.17)) @ Matrix.Rotation(math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X')
