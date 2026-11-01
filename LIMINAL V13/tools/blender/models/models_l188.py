"""V13.3 Level 188 (LOOK_188.md, LEVELSPEC_188.md): Der Hof der Fenster, neu gebaut (PROPS_Level188.md).

Fenster 1,2 x 1,8 m mit profiliertem Blendrahmen, Pfosten, Kaempfer, Glasleisten, Fugen und Aluminiumbank; Jalousien mit
gewoelbten Lamellen, Leiterkordeln, Unterschiene, Zugschnur mit Eichel. Glas ist im Spiel deckend: die Scheibe ist die
Rueckflaeche (x = 0 .. 0,005, glatt und spiegelnd), Jalousien liegen davor.
Fensterwesen (window_188_c): Rueckflaeche bis x = 0,005, frei zwischen 0,0055 und 0,026 (Ansicht und Umriss des Wesens),
Pfosten und Kaempfer ab 0,026, Rahmen nur ausserhalb der Oeffnung (|y| >= 0,6, |z| >= 0,9) - wie bisher.
Mahagonituer mit Kassetten, Pflaster aus Platten mit Moos in den Fugen, Birkenholzkreis (hohl, Deckel mit Ringzug),
Tisch mit Gegenstaenden, Galerie mit Glasbruestung, weisse Stuetze, Oberlicht.
"""
import math
import random

from mathutils import Matrix, Vector

from models.models_l53 import Geo, sweep, lathe, tube, mat, _mix, _emit_box_book, Books, _pick, WEIGHTS

W, H = 1.2, 1.8  # Fensteroeffnung


def _m():
    return {
        'frame': mat('fensterrahmen', (0.89, 0.89, 0.87), 'paint', r=0.75),
        'frame_d': mat('fensterrahmen_schmutz', (0.70, 0.70, 0.67), 'paint', r=0.9),
        'seal': mat('fuge_silikon', (0.42, 0.42, 0.41), 'rubber', r=0.9),
        'gasket': mat('dichtung', (0.08, 0.08, 0.08), 'rubber', r=0.8),
        'alu': mat('aluminium_bank', (0.74, 0.75, 0.76), 'metal', r=0.8, m=0.45),
        'glass_dark': mat('scheibe_dunkel', (0.02, 0.022, 0.026), 'glass'),
        'glass_room': mat('scheibe_raum', (0.12, 0.13, 0.15), 'glass'),
        'blind': mat('lamelle', (0.86, 0.83, 0.74), 'paint', r=0.7),
        'blind_d': mat('lamelle_staub', (0.66, 0.63, 0.56), 'paint', r=1.0),
        'cord': mat('kordel', (0.80, 0.77, 0.68), 'fabric', r=1.3),
        'mahogany': mat('mahagoni', (0.36, 0.13, 0.08), 'wood', 'wood_walnut_floor', r=1.5),
        'mahogany_d': mat('mahagoni_dunkel', (0.22, 0.075, 0.045), 'wood', 'wood_walnut_floor', r=1.7),
        'mahogany_w': mat('mahagoni_abgegriffen', (0.46, 0.20, 0.12), 'wood', 'wood_walnut_floor', r=1.3),
        'brass': mat('messing', (0.78, 0.60, 0.30), 'metal', r=0.45),
        'brass_d': mat('messing_angelaufen', (0.45, 0.34, 0.18), 'metal', r=0.75, m=0.6),
        'concrete': mat('beton_weiss', (0.93, 0.93, 0.92), 'concrete', 'concrete_white', r=1.2),
        'steel': mat('stahl_gestrichen', (0.62, 0.64, 0.66), 'paint', r=0.7, m=0.3),
        'steel_inox': mat('edelstahl', (0.70, 0.71, 0.72), 'metal', r=0.55, m=0.45),
        'black': mat('schwarz', (0.04, 0.04, 0.04), 'paint', r=0.9),
    }


# ======================================================================================================================
# Fenster
# ======================================================================================================================
def _frame(g, M, dirt=0.0):
    """Blendrahmen (Profil mit Gehrung ausserhalb der Oeffnung), Fuge zur Wand, Pfosten, Kaempfer, Glasleisten, Bank."""
    # u nach aussen ab der Oeffnungskante, v entlang +x
    prof = [(0.0, 0.004), (0.0, 0.026), (0.006, 0.03), (0.012, 0.03), (0.012, 0.05), (0.018, 0.058), (0.05, 0.064), (0.066, 0.06),
            (0.074, 0.05), (0.078, 0.0)]
    F, Fd = M['frame'], M['frame_d']
    mats = [F, Fd, F, F, F, F, F, Fd, F]
    path = [(0.0, -W / 2, -H / 2), (0.0, W / 2, -H / 2), (0.0, W / 2, H / 2), (0.0, -W / 2, H / 2)]
    sweep(g, prof, path, (1, 0, 0), F, closed=True, smooth=50, flip=True, mats=mats)
    # Silikonfuge zur Wand
    sweep(g, [(0.078, 0.0), (0.081, 0.004), (0.083, 0.0)], path, (1, 0, 0), M['seal'], closed=True, smooth=80, flip=True)
    # Dichtung zwischen Rahmen und Scheibe (schwarzer Faden innen)
    sweep(g, [(-0.001, 0.0045), (-0.0005, 0.006), (0.0, 0.0065)], path, (1, 0, 0), M['gasket'], closed=True, smooth=80, flip=True)
    # Pfosten (y = 0) und Kaempfer (z = 0,32): ab x = 0,026 (dahinter Platz fuer das Fensterwesen)
    bar = [(-0.02, 0.026), (-0.02, 0.046), (-0.014, 0.054), (0.014, 0.054), (0.02, 0.046), (0.02, 0.026)]
    sweep(g, [(u, v) for (u, v) in bar], [(0.0, 0.0, -H / 2), (0.0, 0.0, 0.3)], (1, 0, 0), F, smooth=50, flip=True,
          mats=[F, F, F, F, Fd])
    sweep(g, [(u, v) for (u, v) in bar], [(0.0, 0.0, 0.34), (0.0, 0.0, H / 2)], (1, 0, 0), F, smooth=50, flip=True,
          mats=[F, F, F, F, Fd])
    sweep(g, [(-0.02, 0.026), (-0.02, 0.046), (-0.014, 0.054), (0.014, 0.054), (0.02, 0.046), (0.02, 0.026)],
          [(0.0, -W / 2, 0.32), (0.0, W / 2, 0.32)], (1, 0, 0), F, smooth=50, flip=True, mats=[Fd, F, F, F, F])
    # Fluegelrahmen je Scheibe mit Glasleiste (ab x = 0,026 wie Pfosten und Kaempfer)
    sprof = [(-0.032, 0.026), (-0.032, 0.031), (-0.027, 0.039), (-0.012, 0.043), (0.0, 0.04), (0.0, 0.026)]
    for (ya, yb) in ((-W / 2 + 0.002, -0.02), (0.02, W / 2 - 0.002)):
        for (za, zb_) in ((-H / 2 + 0.002, 0.3), (0.34, H / 2 - 0.002)):
            sweep(g, sprof, [(0.0, ya, za), (0.0, yb, za), (0.0, yb, zb_), (0.0, ya, zb_)], (1, 0, 0), F, closed=True, smooth=50, flip=True,
                  mats=[Fd, F, F, F, F])
    # Fensterbank aus Aluminium mit Tropfnase und Endkappen (Oberseite bei z = -0,97: Fundstuecke liegen hier)
    zb = -H / 2 - 0.07
    bprof = [(0.0, zb + 0.004), (0.12, zb + 0.001), (0.195, zb - 0.002), (0.202, zb - 0.006), (0.205, zb - 0.014), (0.205, zb - 0.04),
             (0.199, zb - 0.04), (0.199, zb - 0.016), (0.19, zb - 0.012), (0.0, zb - 0.012)]
    sweep(g, list(reversed(bprof)), [(0.0, -W / 2 - 0.09, 0.0), (0.0, W / 2 + 0.09, 0.0)], (0, 0, 1), M['alu'], smooth=45)
    for s in (-1, 1):
        yy = s * (W / 2 + 0.09)
        g.face([(x, yy, z) for (x, z) in bprof], M['alu'], out=(0, s, 0))
        g.face([(x, yy + 0.002 * s, z) for (x, z) in [(0.0, zb + 0.03), (0.0, zb - 0.012), (0.2, zb - 0.012), (0.2, zb + 0.004)]],
               M['alu'], out=(0, s, 0))
    # Anschluss Rahmen/Bank: Staub und Schmutz auf der Bank, Schmutz unten am Rahmen
    g.face([(0.004, -W / 2, zb + 0.0045), (0.09, -W / 2, zb + 0.0025), (0.09, W / 2, zb + 0.0025), (0.004, W / 2, zb + 0.0045)],
           mat('staub_bank', (0.58, 0.57, 0.54), 'paint', r=1.4), out=(0, 0, 1))
    if dirt > 0:
        for s in (-1, 1):
            g.face([(0.0785, s * (W / 2 + 0.02), -H / 2 - 0.07), (0.0785, s * (W / 2 + 0.06), -H / 2 - 0.07),
                    (0.0785, s * (W / 2 + 0.05), -H / 2 + 0.2 * dirt), (0.0785, s * (W / 2 + 0.03), -H / 2 + 0.3 * dirt)], Fd, out=(1, 0, 0))


def _blind(g, M, frac, tilt, rng, bent=(), skew=0.0):
    """Jalousie hinter der Scheibenebene des Pfostens: Kasten oben, gewoelbte Lamellen bis frac der Hoehe, Leiterkordeln,
    Unterschiene mit Zugschnur und Eichel. tilt: Neigung der Lamellen (0 offen .. 1,3 fast zu)."""
    y0, y1 = -W / 2 + 0.01, W / 2 - 0.01
    top = H / 2 - 0.005
    g.cbox((0.004, y0 - 0.005, top - 0.05), (0.024, y1 + 0.005, top), M['blind'], c=0.003, skip=('-x',))
    n = int(frac * (H - 0.06) / 0.042)
    zl = top - 0.06
    ca, sa = math.cos(tilt), math.sin(tilt)
    for k in range(n):
        zc = top - 0.07 - k * 0.042
        dz = skew * (k / max(1, n))
        m = M['blind_d'] if rng.random() < 0.15 else M['blind']
        half = 0.0125
        # Querschnitt (gewoelbt) um die Mitte x = 0,014 gekippt
        P = [(-half, 0.0), (0.0, 0.0022), (half, 0.0)]
        Q = [(0.014 + u * ca - v * sa, zc + u * sa + v * ca) for (u, v) in P]
        if k in bent:  # geknickte Lamelle: Mitte haengt durch
            mid = (y0 + y1) / 2 + rng.uniform(-0.2, 0.2)
            for (ya, yb, da, db) in ((y0, mid, 0.0, -0.012), (mid, y1, -0.012, 0.0)):
                for j in range(2):
                    a, b = Q[j], Q[j + 1]
                    g.face([(a[0], ya, a[1] + da + dz * (ya - y0)), (a[0], yb, a[1] + db + dz * (yb - y0)), (b[0], yb, b[1] + db + dz * (yb - y0)),
                            (b[0], ya, b[1] + da + dz * (ya - y0))], m, out=(sa, 0.0, -ca))
            continue
        for j in range(2):
            a, b = Q[j], Q[j + 1]
            nrm = Vector((-(b[1] - a[1]), 0, b[0] - a[0])).normalized()
            if nrm.x < 0:
                nrm = -nrm
            g.face([(a[0], y0, a[1]), (a[0], y1, a[1] + dz), (b[0], y1, b[1] + dz), (b[0], y0, b[1])], m, out=nrm)
        zl = zc
    zr = zl - 0.03
    if n > 0:
        # Unterschiene
        g.cbox((0.006, y0, zr - 0.006), (0.022, y1, zr + 0.008 + skew * 0.5), M['blind'], c=0.003, skip=('-x',))
        # Leiterkordeln vorn (zwei Strippen je Leiter)
        for yc in (-0.42, 0.0, 0.42):
            for xx in (0.0285 - 0.026, 0.0255):
                g.face([(xx, yc - 0.0012, zr), (xx, yc + 0.0012, zr), (xx, yc + 0.0012, top - 0.05), (xx, yc - 0.0012, top - 0.05)], M['cord'],
                       out=(1, 0, 0))
        # Zugschnur mit Eichel
        yp = y1 - 0.06
        tube(g, [(0.023, yp, top - 0.04), (0.0235, yp, zr - 0.25)], 0.0012, M['cord'], sides=3)
        lathe(g, [(0.0, zr - 0.3), (0.006, zr - 0.29), (0.008, zr - 0.272), (0.005, zr - 0.256), (0.0, zr - 0.25)], M['blind'], seg=6,
              at=(0.0175, yp, 0.0))


def _interior(g, M, kind, rng):
    """Rueckflaeche (Scheibe) mit schwachen Formen dahinter: alles bei x <= 0,005."""
    if kind == 'dark':
        base = M['glass_dark']
    elif kind == 'room':
        base = M['glass_room']
    else:
        base = kind
    g.face([(0.003, -W / 2 - 0.004, -H / 2 - 0.004), (0.003, W / 2 + 0.004, -H / 2 - 0.004), (0.003, W / 2 + 0.004, H / 2 + 0.004),
            (0.003, -W / 2 - 0.004, H / 2 + 0.004)], base, out=(1, 0, 0))
    if kind in ('dark', 'room'):  # schwach: Tuerrahmen, Deckenkante, Spiegelung
        c = (0.05, 0.05, 0.06) if kind == 'dark' else (0.20, 0.21, 0.24)
        sh = mat('raum_schatten_' + kind, c, 'glass')
        y = rng.uniform(-0.35, 0.25)
        g.face([(0.0036, y, -H / 2), (0.0036, y + 0.32, -H / 2), (0.0036, y + 0.32, 0.25), (0.0036, y, 0.25)], sh, out=(1, 0, 0))
        g.face([(0.0036, -W / 2, 0.62), (0.0036, W / 2, 0.66), (0.0036, W / 2, 0.68), (0.0036, -W / 2, 0.64)], sh, out=(1, 0, 0))
        ref = mat('spiegelung', (0.30, 0.32, 0.36) if kind == 'dark' else (0.40, 0.42, 0.46), 'glass')
        a = rng.uniform(-0.3, 0.2)
        g.face([(0.0042, a, -H / 2), (0.0042, a + 0.07, -H / 2), (0.0042, a + 0.5, H / 2), (0.0042, a + 0.43, H / 2)], ref, out=(1, 0, 0))


def _glow_room(g, warm=False, strength=0.35):
    """Erleuchteter Raum: Leuchtflaeche mit hellerer Mitte (Lampe) und dunkleren Raendern."""
    c0 = (1.0, 0.72, 0.48) if warm else (0.80, 0.76, 0.66)
    edge = mat('raumlicht_rand' + ('_warm' if warm else ''), _mix(c0, (0.2, 0.15, 0.1), 0.45), 'glow', glow=strength * 0.7)
    g.face([(0.003, -W / 2 - 0.004, -H / 2 - 0.004), (0.003, W / 2 + 0.004, -H / 2 - 0.004), (0.003, W / 2 + 0.004, H / 2 + 0.004),
            (0.003, -W / 2 - 0.004, H / 2 + 0.004)], edge, out=(1, 0, 0))
    # weicher Abfall zur Lampe hin (oben, etwas seitlich): Ringe mit steigender Staerke
    for k in range(8):
        f = 1.0 - k * 0.11
        mk = mat('raumlicht%d' % k + ('_warm' if warm else ''), _mix(c0, (0.2, 0.15, 0.1), 0.38 - 0.05 * k), 'glow', glow=strength * (0.76 + 0.04 * k))
        pts = [(0.0031 + 0.00015 * k, 0.04 + 0.5 * f * math.cos(a), 0.05 + 0.2 * (1 - f) + 0.8 * f * math.sin(a)) for a in [2 * math.pi * i / 16 for i in range(16)]]
        g.face(pts, mk, out=(1, 0, 0))


def _window(kind='dark', blind=0.0, tilt=0.9, seed=1, figure=False, glow=None, bent=(), skew=0.0, dirt=0.0):
    M = _m()
    rng = random.Random(seed)
    g = Geo()
    if glow is not None:
        _glow_room(g, *glow)
    else:
        _interior(g, M, kind, rng)
    if figure:  # dunkler Umriss im Raum (Wiki: Schattengestalten in den Fenstern)
        um = mat('umriss', (0.012, 0.011, 0.012), 'paint', r=1.0)
        yc = 0.14
        pts = []
        # Gestalt ab der Huefte (Fensterbank 1,0 m ueber dem Boden): Schultern bei -0,42, Kopf bis -0,1 (wie das Fensterwesen)
        for (y, z) in [(-0.17, -H / 2), (0.17, -H / 2), (0.16, -0.7), (0.18, -0.55), (0.2, -0.47), (0.19, -0.43), (0.14, -0.4), (0.07, -0.385),
                       (0.05, -0.36), (0.045, -0.33), (0.065, -0.29), (0.07, -0.22), (0.06, -0.15), (0.035, -0.11), (0.0, -0.1), (-0.035, -0.11),
                       (-0.06, -0.15), (-0.07, -0.22), (-0.065, -0.29), (-0.045, -0.33), (-0.05, -0.36), (-0.07, -0.385), (-0.14, -0.4),
                       (-0.19, -0.43), (-0.2, -0.47), (-0.18, -0.55), (-0.16, -0.7)]:
            pts.append((0.0045, yc + y, z))
        g.face(pts, um, out=(1, 0, 0))
    if blind > 0:
        _blind(g, M, blind, tilt, rng, bent, skew)
    _frame(g, M, dirt)
    g.build('fenster')


def build_window_188_a():
    """Fenster mit fast geschlossener Jalousie (Wiki: die meisten Fenster haben Jalousien)."""
    _window('room', blind=0.86, tilt=1.15, seed=11)


def build_window_188_a__v1():
    """Variante 1: eine Lamelle geknickt, Unterschiene schief, Lamellen etwas offener."""
    _window('room', blind=0.82, tilt=0.95, seed=12, bent=(14,), skew=0.02, dirt=0.5)


def build_window_188_b():
    """Fenster mit halb herabgelassener Jalousie, darunter dunkle Scheibe."""
    _window('dark', blind=0.46, tilt=0.6, seed=21)


def build_window_188_b__v1():
    """Variante 1: Jalousie zu zwei Dritteln, Lamellen fast waagrecht, Schmutz am Rahmen."""
    _window('room', blind=0.62, tilt=0.25, seed=22, dirt=1.0)


def build_window_188_c():
    """Fenster ohne Jalousie, dahinter Dunkel (hier sitzen Fensterwesen: Rueckflaeche bis x = 0,005, frei bis 0,026)."""
    _window('dark', seed=31)


def build_window_188_c__v1():
    """Variante 1: andere schwache Formen im dunklen Raum, Schmutzlaeufer am Rahmen (Freiraum fuer das Wesen gleich)."""
    _window('dark', seed=32, dirt=1.0)


def build_window_188_fig():
    """Fenster ohne Jalousie, im schwachen Licht dahinter ein dunkler Umriss (Wiki: Schattengestalten in den Fenstern)."""
    _window(glow=(False, 0.35), figure=True, seed=41)


def build_window_188_lit():
    """Fenster mit halber Jalousie, dahinter schwaches Licht (ein Raum, in dem jemand sein koennte)."""
    _window(glow=(False, 0.35), blind=0.5, tilt=0.7, seed=51)


def build_window_entry_188():
    """Das Eingangsfenster: dahinter warmes Licht des Hotels (Rueckweg nach Level 5), Jalousie oben."""
    _window(glow=(True, 1.6), blind=0.15, tilt=0.4, seed=61)


# ======================================================================================================================
# Tueren aus Mahagoni
# ======================================================================================================================
def _panel(g, M, y0, y1, z0, z1, x, o=1):
    """Erhabene Fuellung mit Fase und Profilleiste, Blick in Richtung o (+1: +x, -1: -x), Flaeche bei x."""
    b = 0.05
    fx = x + o * 0.014
    g.face([(fx, y0 + b, z0 + b), (fx, y1 - b, z0 + b), (fx, y1 - b, z1 - b), (fx, y0 + b, z1 - b)], M['mahogany'], out=(o, 0, 0))
    pe = [(y0, z0), (y1, z0), (y1, z1), (y0, z1)]
    pi = [(y0 + b, z0 + b), (y1 - b, z0 + b), (y1 - b, z1 - b), (y0 + b, z1 - b)]
    cy, cz = (y0 + y1) / 2, (z0 + z1) / 2
    for k in range(4):
        a, c = pe[k], pe[(k + 1) % 4]
        d, e = pi[k], pi[(k + 1) % 4]
        out = Vector((o * 0.4, (a[0] + c[0]) / 2 - cy, (a[1] + c[1]) / 2 - cz))
        out = Vector((out.x, 1.0 if out.y > 0.01 else -1.0 if out.y < -0.01 else 0.0, 1.0 if out.z > 0.01 else -1.0 if out.z < -0.01 else 0.0))
        out.x = o * 0.5
        g.face([(x + o * 0.003, a[0], a[1]), (x + o * 0.003, c[0], c[1]), (fx, e[0], e[1]), (fx, d[0], d[1])],
               M['mahogany_d'] if k in (0, 1) else M['mahogany'], out=out)
    prof = [(0.0, 0.003), (0.003, 0.01), (0.008, 0.013), (0.012, 0.009), (0.014, 0.0)]
    path = [(x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1)]
    if o < 0:
        path = list(reversed(path))
    sweep(g, prof, path, (o, 0, 0), M['mahogany'], closed=True, smooth=60, flip=True,
          mats=[M['mahogany_w'], M['mahogany'], M['mahogany'], M['mahogany_d']])


def _leaf(g, M, LW, LH, T, both=False, knob_side=1):
    """Tuerblatt (lokal: Flaeche x = 0 .. T, y = 0 .. LW, z = 0 .. LH): Rahmen, vier Kassetten, Knauf mit Rosette,
    Schluesselschild, Griffspuren."""
    g.cbox((0.0, 0.0, 0.0), (T, LW, LH), M['mahogany'], c=0.003, mats={'-z': M['mahogany_d']})
    rails = [(0.2, 0.92), (1.1, LH - 0.12)]
    cols = [(0.11, LW / 2 - 0.04), (LW / 2 + 0.04, LW - 0.11)]
    for (z0, z1) in rails:
        for (y0, y1) in cols:
            _panel(g, M, y0, y1, z0, z1, T, 1)
            if both:
                _panel(g, M, y0, y1, z0, z1, 0.0, -1)
    yk = LW - 0.07 if knob_side > 0 else 0.07
    for (x, o) in ((T, 1),) + (((0.0, -1),) if both else ()):
        old = g.M
        g.M = old @ Matrix.Translation((x, yk, 1.0)) @ Matrix.Rotation(o * math.pi / 2, 4, 'Y')
        lathe(g, [(0.0, 0.0), (0.03, 0.0), (0.029, 0.004), (0.012, 0.008), (0.009, 0.03), (0.022, 0.042), (0.027, 0.055), (0.022, 0.068),
                  (0.0, 0.072)], M['brass'], seg=12, mats=[M['brass_d'], M['brass'], M['brass_d'], M['brass'], M['brass'], M['brass'], M['brass'],
                                                         M['brass']])
        g.M = old @ Matrix.Translation((x, yk, 0.88)) @ Matrix.Rotation(o * math.pi / 2, 4, 'Y')
        lathe(g, [(0.0, 0.0), (0.018, 0.0), (0.016, 0.003), (0.0, 0.004)], M['brass_d'], seg=10)
        g.M = old
        xx = x + o * 0.0045
        g.face([(xx, yk - 0.0035, 0.874), (xx, yk + 0.0035, 0.874), (xx, yk + 0.0035, 0.888), (xx, yk - 0.0035, 0.888)], M['black'], out=(o, 0, 0))
        xx = x + o * 0.0004
        g.face([(xx, yk - 0.09 * knob_side, 0.94), (xx, yk + 0.02 * knob_side, 0.94), (xx, yk + 0.02 * knob_side, 1.08), (xx, yk - 0.09 * knob_side, 1.08)],
               M['mahogany_w'], out=(o, 0, 0))


def build_door_mahogany():
    """Verschlossene Tuer aus Mahagoni (Wandteil, Boden z = 0; Wiki: die meisten Tueren sind von innen verschlossen):
    weisse Zarge mit profilierter Bekleidung, Blatt mit vier erhabenen Kassetten, Messingknauf mit Rosette,
    Schluesselschild, drei Baender, Schattenfuge am Boden."""
    M = _m()
    g = Geo()
    LW, LH = 0.94, 2.2
    prof = [(0.0, 0.03), (0.0, 0.062), (0.008, 0.08), (0.03, 0.088), (0.06, 0.082), (0.075, 0.07), (0.08, 0.05), (0.08, 0.0)]
    # Weg: linker Pfosten hoch, Sturz, rechter Pfosten runter (u nach aussen)
    path = [(0.0, LW / 2, 0.0), (0.0, LW / 2, LH), (0.0, -LW / 2, LH), (0.0, -LW / 2, 0.0)]
    sweep(g, prof, path, (1, 0, 0), M['frame'], smooth=50, flip=False,
          mats=[M['frame'], M['frame'], M['frame'], M['frame'], M['frame_d'], M['frame'], M['frame']])
    for s in (-1, 1):  # Enden der Bekleidung am Boden
        g.face([(0.0, s * LW / 2, 0.0), (0.03, s * LW / 2, 0.0), (0.088, s * (LW / 2 + 0.03), 0.0), (0.0, s * (LW / 2 + 0.08), 0.0)],
               M['frame_d'], out=(0, 0, -1))
    old = g.M
    g.M = Matrix.Translation((0.0, -LW / 2 + 0.003, 0.008))
    _leaf(g, M, LW - 0.006, LH - 0.011, 0.04, knob_side=1)
    g.M = old
    g.face([(0.0005, -LW / 2, 0.0), (0.0005, LW / 2, 0.0), (0.0005, LW / 2, 0.008), (0.0005, -LW / 2, 0.008)], M['black'], out=(1, 0, 0))
    for zz in (0.25, 1.1, 1.95):  # Baender (Huelse sichtbar)
        tube(g, [(0.042, -LW / 2 + 0.002, zz - 0.05), (0.042, -LW / 2 + 0.002, zz + 0.05)], 0.008, M['brass_d'], sides=8, caps=True)
    g.build('tuer')


def build_door_mahogany_open():
    """Offene Mahagonituer im Durchbruch (Laibung x = -0,5 .. 0,5): weisses Futter durch die ganze Wand, Bekleidungen
    beidseits, Blatt mit Kassetten auf beiden Seiten um 75 Grad aufgeschwenkt."""
    M = _m()
    g = Geo()
    LW, LH = 0.94, 2.2
    Wd = 1.0
    for s in (-1, 1):  # Futter (Laibung)
        y = s * LW / 2 + s * 0.01
        g.face([(-0.5, y, 0.0), (0.5, y, 0.0), (0.5, y, LH + 0.01), (-0.5, y, LH + 0.01)], M['frame'], out=(0, -s, 0))
    g.face([(-0.5, -LW / 2 - 0.01, LH + 0.01), (0.5, -LW / 2 - 0.01, LH + 0.01), (0.5, LW / 2 + 0.01, LH + 0.01), (-0.5, LW / 2 + 0.01, LH + 0.01)],
           M['frame'], out=(0, 0, -1))
    prof = [(0.0, 0.0), (0.0, 0.012), (0.008, 0.03), (0.03, 0.038), (0.06, 0.032), (0.075, 0.02), (0.08, 0.0)]
    for (xf, o) in ((0.5, 1), (-0.5, -1)):  # Bekleidungen auf beiden Wandseiten
        path = [(xf, LW / 2 + 0.01, 0.0), (xf, LW / 2 + 0.01, LH + 0.01), (xf, -LW / 2 - 0.01, LH + 0.01), (xf, -LW / 2 - 0.01, 0.0)]
        if o < 0:
            path = list(reversed(path))
        sweep(g, prof, path, (o, 0, 0), M['frame'], smooth=50)
    old = g.M
    g.M = Matrix.Translation((0.46, -LW / 2 + 0.006, 0.008)) @ Matrix.Rotation(math.radians(75), 4, 'Z') @ Matrix.Translation((-0.04, 0.0, 0.0))
    _leaf(g, M, LW - 0.012, LH - 0.012, 0.04, both=True, knob_side=1)
    g.M = old
    for zz in (0.25, 1.1, 1.95):
        tube(g, [(0.462, -LW / 2 + 0.004, zz - 0.05), (0.462, -LW / 2 + 0.004, zz + 0.05)], 0.008, M['brass_d'], sides=8, caps=True)
    g.build('tuer_offen')


# ======================================================================================================================
# Hof
# ======================================================================================================================
def _slab(g, lo, hi, m, rng, c=0.006, chip=False):
    """Platte mit Fase, leicht verkantet; ohne Unterseite."""
    old = g.M
    cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
    g.M = old @ Matrix.Translation((cx, cy, 0.0)) @ Matrix.Rotation(rng.uniform(-0.012, 0.012), 4, 'X') @ \
        Matrix.Rotation(rng.uniform(-0.012, 0.012), 4, 'Y') @ Matrix.Translation((-cx, -cy, rng.uniform(-0.003, 0.002)))
    g.cbox(lo, hi, m, c=c, skip=('-z',))
    if chip:  # abgeplatzte Ecke (helle Bruchflaeche)
        x, y = hi[0], hi[1]
        g.face([(x - 0.05, y + 0.0004, hi[2] + 0.0004), (x + 0.0004, y - 0.04, hi[2] + 0.0004), (x + 0.0004, y - 0.01, hi[2] - 0.012),
                (x - 0.015, y + 0.0004, hi[2] - 0.012)], mat('bruch_hell', (0.82, 0.81, 0.78), 'concrete', 'concrete_cast', r=1.4),
               out=(0.5, 0.5, 0.7))
    g.M = old


def _paving(seed, across=3, along=2):
    rng = random.Random(seed)
    g = Geo()
    tints = [mat('platte_%d' % k, c, 'concrete', 'concrete_cast', r=1.25) for k, c in
             enumerate(((0.74, 0.73, 0.70), (0.68, 0.67, 0.64), (0.79, 0.78, 0.74)))]
    kerb = mat('randstein', (0.56, 0.56, 0.54), 'concrete', 'concrete_rough', r=1.3)
    moss = mat('moos_fuge', (0.24, 0.32, 0.10), 'fabric', 'moss', r=1.4)
    dirt = mat('fuge_erde', (0.20, 0.17, 0.12), 'concrete', r=1.5)
    z = 0.04
    gap = 0.008
    wy = 1.4 / across
    wx = 1.0 / along
    for i in range(along):
        for j in range(across):
            x0, x1 = -0.5 + i * wx + gap / 2, -0.5 + (i + 1) * wx - gap / 2
            y0, y1 = -0.7 + j * wy + gap / 2, -0.7 + (j + 1) * wy - gap / 2
            _slab(g, (x0, y0, 0.0), (x1, y1, z), tints[rng.randrange(3)], rng, chip=rng.random() < 0.12)
    for s in (-1, 1):  # Randsteine
        for i in range(2):
            x0, x1 = -0.5 + i * 0.5 + 0.004, -0.5 + (i + 1) * 0.5 - 0.004
            y0, y1 = (0.704, 0.8) if s > 0 else (-0.8, -0.704)
            _slab(g, (x0, y0, 0.0), (x1, y1, z + 0.006), kerb, rng, c=0.01)
    # Fugen: Erde und Moos
    # Flecken und ein Haarriss auf einzelnen Platten
    stain = mat('platte_fleck', (0.61, 0.59, 0.55), 'concrete', 'concrete_cast', r=1.4)
    crack = mat('platte_riss', (0.18, 0.17, 0.15), 'paint', r=1.4)
    for k in range(rng.randint(1, 3)):
        cx, cy = rng.uniform(-0.4, 0.4), rng.uniform(-0.6, 0.6)
        r0 = rng.uniform(0.04, 0.1)
        g.face([(cx + r0 * rng.uniform(0.6, 1.1) * math.cos(a), cy + r0 * rng.uniform(0.6, 1.1) * math.sin(a), z + 0.0012) for a in
                [2 * math.pi * i / 9 for i in range(9)]], stain, out=(0, 0, 1))
    if rng.random() < 0.6:
        cx, cy = rng.uniform(-0.35, 0.2), rng.uniform(-0.5, 0.4)
        P = [(cx + 0.06 * i, cy + 0.025 * math.sin(i * 1.7) + 0.01 * i) for i in range(5)]
        for i in range(4):
            (xa, ya), (xb, yb) = P[i], P[i + 1]
            g.face([(xa, ya - 0.0012, z + 0.0014), (xb, yb - 0.0012, z + 0.0014), (xb, yb + 0.0012, z + 0.0014), (xa, ya + 0.0012, z + 0.0014)], crack,
                   out=(0, 0, 1))
    for j in range(1, across):
        y = -0.7 + j * wy
        g.face([(-0.5, y - gap / 2, 0.026), (0.5, y - gap / 2, 0.026), (0.5, y + gap / 2, 0.026), (-0.5, y + gap / 2, 0.026)], dirt, out=(0, 0, 1))
        for k in range(rng.randint(1, 3)):
            xa = rng.uniform(-0.5, 0.35)
            g.face([(xa, y - gap / 2 - 0.002, 0.03), (xa + rng.uniform(0.06, 0.18), y - gap / 2 - 0.002, 0.03),
                    (xa + 0.1, y + gap / 2 + 0.002, 0.031), (xa, y + gap / 2 + 0.002, 0.031)], moss, out=(0, 0, 1))
    for i in range(1, along):
        x = -0.5 + i * wx
        g.face([(x - gap / 2, -0.7, 0.026), (x + gap / 2, -0.7, 0.026), (x + gap / 2, 0.7, 0.026), (x - gap / 2, 0.7, 0.026)], dirt, out=(0, 0, 1))
    for s in (-1, 1):  # Moos am Rand ueber den Randstein
        yy = s * 0.8
        g.face([(-0.5, yy, 0.0), (0.5, yy, 0.0), (0.5, yy + s * 0.0005, 0.018), (-0.5, yy + s * 0.0005, 0.012)], moss, out=(0, s, 0))
    g.build('pflaster')


def build_paving_strip():
    """Weg aus Betonplatten (Boden, Mitte im Ursprung, 1 m entlang x, 1,6 m breit, 4 cm): sechs Platten mit Fase, leicht
    verkantet, Randsteine, Fugen mit Erde und Moos, abgeplatzte Ecken."""
    _paving(1881)


def build_paving_strip__v1():
    """Pflaster, Variante 1: zwei breite Platten quer, andere Fugen."""
    _paving(1882, across=2, along=2)


def build_paving_strip__v2():
    """Pflaster, Variante 2: drei Platten quer, eine Reihe (laengere Platten)."""
    _paving(1883, across=3, along=1)


def build_birch_circle():
    """Kreis aus hellem Birkenholz in der Mitte (Boden, Durchmesser 7 m, 12 cm hoch; Wiki: hohl, laesst sich kaum oeffnen):
    Dielen mit Fugen und Schrauben ueber den Lagern, gebogener Randstreifen aus Schichtholz, Deckel mit Fuge und
    eingelassenem Ringzug, Vergrauung, Moos am Rand."""
    rng = random.Random(188)
    g = Geo()
    R, Hc = 3.5, 0.12
    birch = [mat('birke_%d' % k, c, 'wood', 'wood_birch', r=1.8) for k, c in enumerate(((0.80, 0.86, 0.98), (0.74, 0.80, 0.92), (0.86, 0.90, 1.0)))]
    grey = mat('birke_vergraut', (0.62, 0.62, 0.62), 'wood', 'wood_birch', r=2.0)
    gap_m = mat('fuge_dunkel', (0.06, 0.05, 0.04), 'paint', r=1.3)
    screw = mat('schraube', (0.30, 0.30, 0.30), 'metal', r=0.7)
    moss = mat('moos_rand', (0.24, 0.32, 0.10), 'fabric', 'moss', r=1.4)
    # dunkler Grund unter den Fugen
    n = 48
    g.face([(R * math.cos(2 * math.pi * i / n) * 0.985, R * math.sin(2 * math.pi * i / n) * 0.985, Hc - 0.02) for i in range(n)], gap_m, out=(0, 0, 1))
    bw = 0.145
    nb = int(2 * R / bw)
    for k in range(nb):
        y0 = -R + k * bw + 0.003
        y1 = y0 + bw - 0.006
        ym = (y0 + y1) / 2
        if abs(ym) > R - 0.05:
            continue
        half = math.sqrt(max(0.0, (R - 0.06) ** 2 - max(y0 * y0, y1 * y1)))
        m = birch[rng.randrange(3)] if rng.random() > 0.12 else grey
        segs = [(-half, half)]
        if abs(ym) < 0.9:  # Deckel: Dielen dort mit Fuge getrennt
            hx = math.sqrt(0.81 - ym * ym)
            segs = [(-half, -hx - 0.004), (-hx + 0.004, hx - 0.004), (hx + 0.004, half)]
        Rr = R - 0.05

        def xr(y):
            return math.sqrt(max(0.0, Rr * Rr - y * y))
        for (xa, xb) in segs:
            if xb - xa < 0.05:
                continue
            ys = (y0, ym, y1)
            left = [(-xr(y), y) for y in ys] if xa <= -half + 1e-6 else [(xa, y) for y in (y0, y1)]
            right = [(xr(y), y) for y in ys] if xb >= half - 1e-6 else [(xb, y) for y in (y0, y1)]
            top = [right[0]] + right[1:] + list(reversed(left))
            g.face([(x, y, Hc) for (x, y) in top], m, out=(0, 0, 1))
            for (yy, s, a_, b_) in ((y0, -1, left[0][0], right[0][0]), (y1, 1, left[-1][0], right[-1][0])):
                g.face([(a_, yy, Hc - 0.02), (b_, yy, Hc - 0.02), (b_, yy, Hc), (a_, yy, Hc)], m, out=(0, s, 0))
            for xx in [x for x in (-2.4, -1.2, 0.0, 1.2, 2.4) if xa + 0.03 < x < xb - 0.03]:  # Schrauben ueber den Lagern
                for yy in (y0 + 0.025, y1 - 0.025):
                    g.face([(xx - 0.004, yy - 0.004, Hc + 0.0005), (xx + 0.004, yy - 0.004, Hc + 0.0005), (xx + 0.004, yy + 0.004, Hc + 0.0005),
                            (xx - 0.004, yy + 0.004, Hc + 0.0005)], screw, out=(0, 0, 1))
    # Randstreifen (gebogen) mit Schichten an der Oberkante
    prof = [(R - 0.06, Hc + 0.004), (R - 0.01, Hc + 0.004), (R, Hc - 0.006), (R, 0.0)]
    P = [(math.cos(2 * math.pi * i / 64), math.sin(2 * math.pi * i / 64)) for i in range(64)]
    for i in range(64):
        a, b = P[i], P[(i + 1) % 64]
        for k in range(len(prof) - 1):
            (ra, za), (rb, zb) = prof[k], prof[k + 1]
            n2 = Vector((-(zb - za), 0, (rb - ra))).normalized()
            nA = Vector((a[0] * n2.x, a[1] * n2.x, n2.z))
            nB = Vector((b[0] * n2.x, b[1] * n2.x, n2.z))
            g.face([(a[0] * ra, a[1] * ra, za), (b[0] * ra, b[1] * ra, za), (b[0] * rb, b[1] * rb, zb), (a[0] * rb, a[1] * rb, zb)],
                   birch[0] if k != 1 else birch[1], out=(nA + nB), nrm=[nA, nB, nB, nA])
    for i in range(0, 64, 2):  # Moos und Vergrauung unten am Rand
        a, b = P[i], P[(i + 1) % 64]
        hm = rng.uniform(0.01, 0.04)
        if rng.random() < 0.5:
            g.face([(a[0] * (R + 0.0008), a[1] * (R + 0.0008), 0.0), (b[0] * (R + 0.0008), b[1] * (R + 0.0008), 0.0),
                    (b[0] * (R + 0.0008), b[1] * (R + 0.0008), hm), (a[0] * (R + 0.0008), a[1] * (R + 0.0008), hm * 0.6)], moss,
                   out=(a[0] + b[0], a[1] + b[1], 0))
    # Deckel: Fuge und Ringzug
    ring = [(0.9 * math.cos(2 * math.pi * i / 40), 0.9 * math.sin(2 * math.pi * i / 40)) for i in range(40)]
    for i in range(40):
        a, b = ring[i], ring[(i + 1) % 40]
        g.face([(a[0] * 0.996, a[1] * 0.996, Hc + 0.0006), (b[0] * 0.996, b[1] * 0.996, Hc + 0.0006), (b[0] * 1.004, b[1] * 1.004, Hc + 0.0006),
                (a[0] * 1.004, a[1] * 1.004, Hc + 0.0006)], gap_m, out=(0, 0, 1))
    M = _m()
    lathe(g, [(0.0, Hc + 0.0006), (0.05, Hc + 0.0006), (0.05, Hc + 0.0012), (0.0, Hc + 0.0012)], M['black'], seg=12, at=(0.55, 0.0, 0.0))
    tube(g, [(0.55 + 0.035 * math.cos(2 * math.pi * i / 12), 0.035 * math.sin(2 * math.pi * i / 12), Hc + 0.004) for i in range(12)], 0.004,
         M['brass_d'], sides=5, closed=True)
    g.build('birkenkreis')


def build_table_188():
    """Kleiner runder Tisch aus Mahagoni (Wiki: Tisch, alte Fotografien, Buecher, Vasen, Wecker - nicht beruehren):
    Platte mit Profilkante, Zarge, gedrehte Saeule, drei geschwungene Beine mit Messingschuhen; zwei Fotografien im
    Rahmen mit Stuetze, drei Buecher, Porzellanvase mit Band und trockenen Zweigen, Wecker mit zwei Glocken."""
    M = _m()
    rng = random.Random(188)
    g = Geo()
    Ma, Md = M['mahogany'], M['mahogany_d']
    lathe(g, [(0.0, 0.725), (0.44, 0.725), (0.452, 0.73), (0.455, 0.738), (0.45, 0.745), (0.44, 0.748), (0.0, 0.748)], Ma, seg=40,
          mats=[Md, Md, Ma, M['mahogany_w'], Ma, Ma])
    lathe(g, [(0.38, 0.66), (0.385, 0.67), (0.385, 0.725)], Md, seg=32)
    prof = [(0.06, 0.12), (0.075, 0.14), (0.05, 0.18), (0.035, 0.22), (0.04, 0.32), (0.055, 0.4), (0.06, 0.44), (0.04, 0.5), (0.03, 0.56),
            (0.045, 0.6), (0.05, 0.63), (0.07, 0.66), (0.0, 0.67)]
    lathe(g, prof, Ma, seg=14, mats=[Md, Ma, Ma, Ma, Ma, Md, Ma, Ma, Md, Ma, Ma, Md])
    for k in range(3):
        a = k * 2 * math.pi / 3 + 0.4
        c, s = math.cos(a), math.sin(a)
        P = [(c * (0.05 + 0.3 * t), s * (0.05 + 0.3 * t), 0.17 * (1 - t) ** 1.4 + 0.035 + 0.05 * math.sin(math.pi * t) * (1 - t)) for t in [i / 8 for i in range(9)]]
        tube(g, P, 0.02, Ma, sides=6, radii=[0.026 - 0.012 * i / 8 for i in range(9)])
        lathe(g, [(0.0, 0.0), (0.017, 0.002), (0.016, 0.03), (0.0, 0.035)], M['brass_d'], seg=8, at=(c * 0.35, s * 0.35, 0.0))
    zt = 0.748
    # Fotografien im Rahmen mit Stuetze
    sepia = [mat('foto_hell', (0.66, 0.56, 0.42), 'paint', r=0.6), mat('foto_mittel', (0.46, 0.38, 0.28), 'paint', r=0.6),
             mat('foto_dunkel', (0.24, 0.19, 0.14), 'paint', r=0.6)]
    for k, (x, y, yaw, wd, ht) in enumerate(((-0.17, 0.12, 0.3, 0.13, 0.18), (0.06, 0.27, -0.45, 0.16, 0.12))):
        old = g.M
        g.M = Matrix.Translation((x, y, zt)) @ Matrix.Rotation(yaw, 4, 'Z') @ Matrix.Rotation(-0.22, 4, 'Y')
        fw = 0.014
        g.cbox((-0.01, -wd / 2 - fw, 0.0), (0.0, wd / 2 + fw, ht + 2 * fw), M['brass'] if k == 0 else Md, c=0.003)
        g.face([(0.0004, -wd / 2, fw), (0.0004, wd / 2, fw), (0.0004, wd / 2, ht + fw), (0.0004, -wd / 2, ht + fw)], sepia[0], out=(1, 0, 0))
        g.face([(0.0007, -wd / 2, fw), (0.0007, wd / 2, fw), (0.0007, wd / 2, fw + ht * 0.35), (0.0007, -wd / 2, fw + ht * 0.3)], sepia[1], out=(1, 0, 0))
        # Person: Kopf und Schultern
        cy = rng.uniform(-0.01, 0.01)
        g.face([(0.001, cy - wd * 0.28, fw), (0.001, cy + wd * 0.28, fw), (0.001, cy + wd * 0.22, fw + ht * 0.5), (0.001, cy + wd * 0.1, fw + ht * 0.58),
                (0.001, cy - wd * 0.1, fw + ht * 0.58), (0.001, cy - wd * 0.22, fw + ht * 0.5)], sepia[2], out=(1, 0, 0))
        g.face([(0.0013, cy + wd * 0.1 * math.cos(t), fw + ht * 0.7 + ht * 0.12 * math.sin(t)) for t in [2 * math.pi * i / 10 for i in range(10)]],
               sepia[2], out=(1, 0, 0))
        g.cbox((-0.075, -0.01, 0.0), (-0.01, 0.01, 0.006), M['black'], c=0.002)  # Stuetze
        g.M = old
    # Buecherstapel
    BK = Books()
    zz = zt
    for k in range(3):
        L, T = 0.22 - 0.02 * k, 0.03 + 0.008 * (k % 2)
        D = L * 0.72
        Mt = Matrix.Translation((0.24, -0.13, zz)) @ Matrix.Rotation(rng.uniform(-0.3, 0.3) + 1.2, 4, 'Z') @ Matrix.Translation((D / 2, -L / 2, 0.0))
        _emit_box_book(g, Mt, L, D, T, BK.spine[[0, 7, 4][k]], BK.edges[0], 0.6, [(L * 0.1, L * 0.115, BK.gold)], full=True)
        zz += T
    # Vase mit Band und trockenen Zweigen
    por = mat('porzellan', (0.86, 0.87, 0.90), 'ceramic', r=0.45)
    blue = mat('porzellan_blau', (0.16, 0.24, 0.50), 'ceramic', r=0.45)
    vx, vy = -0.15, -0.2
    vp = [(0.0, zt), (0.04, zt), (0.05, zt + 0.02), (0.075, zt + 0.08), (0.08, zt + 0.12), (0.07, zt + 0.17), (0.04, zt + 0.22), (0.03, zt + 0.25),
          (0.036, zt + 0.27), (0.03, zt + 0.272), (0.022, zt + 0.26)]
    lathe(g, vp, por, seg=18, at=(vx, vy, 0.0), mats=[por, por, por, blue, por, por, blue, por, por, por])
    twig = mat('zweig', (0.30, 0.23, 0.15), 'wood', r=1.2)
    for k in range(6):
        a = k * 1.1 + rng.uniform(-0.2, 0.2)
        L = rng.uniform(0.35, 0.55)
        base = Vector((vx, vy, zt + 0.25))
        dirv = Vector((math.cos(a) * 0.32, math.sin(a) * 0.32, 1.0)).normalized()
        P = [base + dirv * L * t + Vector((math.cos(a + 1.3), math.sin(a + 1.3), 0)) * 0.02 * math.sin(t * 5) for t in (0.0, 0.33, 0.66, 1.0)]
        tube(g, P, 0.003, twig, sides=4, radii=[0.0035, 0.003, 0.0022, 0.0012])
        for j in (1, 2):
            q = P[j]
            d2 = (dirv + Vector((math.cos(a + 2.0 * (j - 1.5)), math.sin(a + 2.0 * (j - 1.5)), 0.2))).normalized()
            tube(g, [q, q + d2 * 0.07, q + d2 * 0.12 + Vector((0, 0, 0.02))], 0.0015, twig, sides=3, radii=[0.0018, 0.0013, 0.0007])
    # Wecker mit zwei Glocken
    wx, wy = 0.2, 0.1
    old = g.M
    g.M = Matrix.Translation((wx, wy, zt + 0.065)) @ Matrix.Rotation(-0.5, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'Y')
    lathe(g, [(0.0, -0.02), (0.05, -0.02), (0.055, -0.012), (0.055, 0.012), (0.05, 0.02), (0.044, 0.022)], M['brass'], seg=20)
    face = mat('zifferblatt', (0.92, 0.89, 0.80), 'paint', r=0.6)
    g.face([(0.044 * math.cos(t), 0.044 * math.sin(t), 0.0215) for t in [2 * math.pi * i / 20 for i in range(20)]], face, out=(0, 0, 1))
    for i in range(12):
        t = 2 * math.pi * i / 12
        c, s = math.cos(t), math.sin(t)
        g.face([(c * 0.034 - s * 0.0015, s * 0.034 + c * 0.0015, 0.022), (c * 0.04 - s * 0.0015, s * 0.04 + c * 0.0015, 0.022),
                (c * 0.04 + s * 0.0015, s * 0.04 - c * 0.0015, 0.022), (c * 0.034 + s * 0.0015, s * 0.034 - c * 0.0015, 0.022)], M['black'], out=(0, 0, 1))
    for (ang, ln) in ((1.9, 0.024), (0.5, 0.034)):
        c, s = math.cos(ang), math.sin(ang)
        g.face([(-s * 0.0015, c * 0.0015, 0.0225), (c * ln - s * 0.0007, s * ln + c * 0.0007, 0.0225), (c * ln + s * 0.0007, s * ln - c * 0.0007, 0.0225),
                (s * 0.0015, -c * 0.0015, 0.0225)], M['black'], out=(0, 0, 1))
    g.M = old
    for s in (-1, 1):
        bell = Vector((wx, wy, zt + 0.13)) + Vector((math.sin(0.5), math.cos(0.5), 0)) * 0.0 + Vector((-math.sin(-0.5) * 0, 0, 0))
        off = Vector((math.cos(-0.5 + math.pi / 2), math.sin(-0.5 + math.pi / 2), 0.0)) * 0.03 * s
        lathe(g, [(0.0, zt + 0.112), (0.026, zt + 0.112), (0.025, zt + 0.124), (0.018, zt + 0.136), (0.0, zt + 0.14)], M['brass'], seg=12,
              at=(wx + off.x, wy + off.y, 0.0))
        tube(g, [(wx + off.x * 0.6, wy + off.y * 0.6, zt + 0.02), (wx + off.x * 1.4, wy + off.y * 1.4, zt)], 0.004, M['brass_d'], sides=5)
    tube(g, [(wx, wy, zt + 0.118), (wx, wy, zt + 0.145)], 0.003, M['brass_d'], sides=5)
    g.build('tisch')


def build_walkway_glass():
    """Galerie mit Glasbruestung an der Wand (Wandteil, 1 m entlang y, Boden der Galerie bei z = 0, ragt 1,6 m vor;
    Wiki: Glasgelaender und Stege): Betonplatte mit Fase und Tropfkante, Stahlkonsole (Doppel-T, zur Spitze schlanker)
    unter jedem Modul, Randprofil aus Stahl, Bodenschiene mit Deckblech und Schrauben, Glasscheibe mit Spiegelung und
    gruener Kante, U-Kappe und Handlauf aus Edelstahl, Fugen zwischen den Scheiben."""
    M = _m()
    g = Geo()
    C = M['concrete']
    St = M['steel']
    g.face([(0.0, -0.5, 0.0), (1.48, -0.5, 0.0), (1.48, 0.5, 0.0), (0.0, 0.5, 0.0)], C, out=(0, 0, 1))
    g.face([(0.0, -0.5, -0.2), (1.55, -0.5, -0.2), (1.55, 0.5, -0.2), (0.0, 0.5, -0.2)], C, out=(0, 0, -1))
    for s_ in (-1, 1):
        g.face([(0.0, s_ * 0.5, -0.2), (1.55, s_ * 0.5, -0.2), (1.55, s_ * 0.5, 0.0), (0.0, s_ * 0.5, 0.0)], C, out=(0, s_, 0))
    # Randprofil (C) aus Stahl an der Stirn, mit Tropfkante
    sweep(g, [(1.55, 0.0), (1.605, 0.0), (1.605, -0.25), (1.6, -0.25), (1.6, -0.2), (1.55, -0.2)], [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), St,
          smooth=20, flip=True)
    # Konsole unter der Platte (Doppel-T, schlanker zur Spitze)
    for (y0, y1) in ((-0.004, 0.004),):
        web = [(0.0, -0.2), (1.5, -0.2), (1.5, -0.25), (0.0, -0.37)]
        for yy, s_ in ((y0, -1), (y1, 1)):
            g.face([(x, yy, z) for (x, z) in web], St, out=(0, s_, 0))
        g.box((0.0, -0.05, -0.385), (0.02, 0.05, -0.2), St, skip=('-x',))  # Wandplatte
        g.face([(0.0, -0.04, -0.37), (1.5, -0.04, -0.25), (1.5, 0.04, -0.25), (0.0, 0.04, -0.37)], St, out=(0.08, 0, -1))
        g.face([(1.5, -0.04, -0.25), (1.5, 0.04, -0.25), (1.5, 0.04, -0.2), (1.5, -0.04, -0.2)], St, out=(1, 0, 0))
        for zz in (-0.24, -0.34):
            for yy in (-0.03, 0.03):
                g.face([(0.0205, yy - 0.006, zz - 0.006), (0.0205, yy + 0.006, zz - 0.006), (0.0205, yy + 0.006, zz + 0.006), (0.0205, yy - 0.006, zz + 0.006)],
                       M['black'], out=(1, 0, 0))
    # Bodenschiene (U) mit Deckblech und Schrauben
    prof = [(1.48, 0.0), (1.48, 0.07), (1.5, 0.075), (1.505, 0.06), (1.565, 0.06), (1.57, 0.075), (1.6, 0.07), (1.6, 0.0)]
    sweep(g, prof, [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['steel_inox'], smooth=40, flip=True)
    for yy in (-0.3, 0.0, 0.3):
        old = g.M
        g.M = Matrix.Translation((1.6, yy, 0.035)) @ Matrix.Rotation(math.pi / 2, 4, 'Y')
        lathe(g, [(0.0, 0.0), (0.008, 0.0), (0.0065, 0.003), (0.0, 0.0035)], M['steel_inox'], seg=6)
        g.M = old
    # Glasscheibe (im Spiel deckend): glatte, leicht getoente Flaechen, Spiegelstreifen, gruene Kanten
    glass = mat('glas_bruestung', (0.30, 0.38, 0.38), 'glass')
    refl = mat('glas_spiegelung', (0.55, 0.62, 0.62), 'glass')
    edge = mat('glas_kante', (0.25, 0.45, 0.38), 'glass')
    g.box((1.524, -0.494, 0.06), (1.546, 0.494, 1.0), glass, skip=('-z',), mats={'+z': edge, '-y': edge, '+y': edge})
    for (ya, w) in ((-0.3, 0.08), (0.1, 0.03)):
        g.face([(1.5465, ya, 0.07), (1.5465, ya + w, 0.07), (1.5465, ya + w + 0.35, 0.99), (1.5465, ya + 0.35, 0.99)], refl, out=(1, 0, 0))
    # Handlauf: U-Kappe und Rundrohr
    sweep(g, [(1.518, 0.98), (1.518, 1.01), (1.552, 1.01), (1.552, 0.98)], [(0, -0.5, 0), (0, 0.5, 0)], (0, 0, 1), M['steel_inox'], smooth=20, flip=True)
    tube(g, [(1.535, -0.5, 1.05), (1.535, 0.5, 1.05)], 0.024, M['steel_inox'], sides=10)
    tube(g, [(1.535, 0.0, 1.01), (1.535, 0.0, 1.03)], 0.008, M['steel_inox'], sides=6)
    g.build('galerie')


def build_pillar_188():
    """Weisse runde Stuetze (Boden, 30 m hoch; Wiki: Saeulen erscheinen): Sichtbeton mit Brettschalung (Bretter als
    Facetten in zwei Toenen), Arbeitsfugen alle 2,5 m mit Ankerloechern, gefaster Sockel, Schmutz und Moos am Fuss,
    Stahlplatte oben."""
    M = _m()
    rng = random.Random(1880)
    g = Geo()
    c1 = mat('beton_brett_a', (0.93, 0.93, 0.92), 'concrete', 'concrete_white', r=1.2)
    c2 = mat('beton_brett_b', (0.83, 0.83, 0.82), 'concrete', 'concrete_white', r=1.3)
    cj = mat('beton_fuge', (0.55, 0.55, 0.54), 'concrete', 'concrete_white', r=1.4)
    dirty = mat('beton_fuss', (0.52, 0.54, 0.46), 'concrete', 'concrete_white', r=1.4)
    seg = 24
    lathe(g, [(0.55, 0.0), (0.55, 0.12), (0.53, 0.15), (0.45, 0.15)], dirty, seg=seg, mats=[dirty, dirty, c2])
    prof = []
    kinds = []
    z = 0.15
    while z < 29.9:
        z1 = min(29.9, z + 2.5)
        for (t, ins) in ((0.0, 0.0), (0.004, 0.0), (0.5, 0.0), (0.996, 0.0), (1.0, 0.003)):
            if prof and t == 0.0:
                continue
            prof.append((0.45 - ins, z + (z1 - z) * t))
            kinds.append(ins > 0)
        z = z1
    mats = []
    for k in range(len(prof) - 1):
        mats.append(cj if (kinds[k] or kinds[k + 1]) else None)

    def mfn(k, i):
        if mats[k] is not None:
            return mats[k]
        if prof[k][1] < 0.5:
            return dirty
        return c1 if (i + k // 4) % 2 else c2
    lathe(g, prof, c1, seg=seg, mfn=mfn)
    g.box((-0.5, -0.5, 29.9), (0.5, 0.5, 30.0), M['steel'], skip=('+z',))
    # Ankerloecher (Konen) an jeder Arbeitsfuge
    zz = 0.15 + 1.25
    while zz < 29.5:
        for a in (0.4, 0.4 + math.pi):
            c, s = math.cos(a), math.sin(a)
            n = Vector((c, s, 0))
            t = Vector((-s, c, 0))
            p = n * 0.4512 + Vector((0, 0, zz))
            g.face([p + t * 0.012 * math.cos(b) + Vector((0, 0, 0.012 * math.sin(b))) for b in [2 * math.pi * i / 8 for i in range(8)]], cj, out=n)
        zz += 2.5
    g.build('stuetze')


def build_skylight_188():
    """Oberlicht (Deckenteil, Decke bei z = 0, 8 x 8 m): Mattglas mit kaltem Tageslicht in Stahl-T-Sprossen alle 1 m,
    Hauptprofile alle 4 m, Knotenbleche mit Schrauben, einzelne Scheiben verschmutzt, Laub und Schmutz auf dem Glas
    (eigene Ergaenzung: die Seite nennt keine Lichtquelle des Hofs)."""
    M = _m()
    rng = random.Random(1888)
    g = Geo()
    sky = mat('oberlicht', (0.92, 0.96, 1.0), 'glow', glow=1.1)
    sky_d = mat('oberlicht_schmutz', (0.78, 0.82, 0.86), 'glow', glow=0.85)
    leaf = mat('laub', (0.10, 0.09, 0.07), 'paint', r=1.2)
    for i in range(8):
        for j in range(8):
            x0, y0 = -4.0 + i + 0.03, -4.0 + j + 0.03
            m = sky_d if rng.random() < 0.15 else sky
            g.face([(x0, y0, -0.03), (x0 + 0.94, y0, -0.03), (x0 + 0.94, y0 + 0.94, -0.03), (x0, y0 + 0.94, -0.03)], m, out=(0, 0, -1))
            if rng.random() < 0.1:  # Laub auf dem Glas (von unten als Schatten)
                cx, cy = x0 + rng.uniform(0.2, 0.7), y0 + rng.uniform(0.2, 0.7)
                g.face([(cx + rng.uniform(0.02, 0.06) * math.cos(t), cy + rng.uniform(0.02, 0.05) * math.sin(t), -0.0298) for t in
                        [2 * math.pi * k / 7 for k in range(7)]], leaf, out=(0, 0, -1))
    S = M['steel']
    Sd = mat('stahl_rost', (0.40, 0.30, 0.24), 'paint', r=1.1, m=0.2)
    for k in range(9):
        t = -4.0 + k
        main = k % 4 == 0
        d = 0.1 if main else 0.075
        w = 0.035 if main else 0.025
        prof = [(-0.004, -0.03), (-0.004, -d + 0.006), (-w, -d + 0.006), (-w, -d), (w, -d), (w, -d + 0.006), (0.004, -d + 0.006), (0.004, -0.03)]
        sweep(g, prof, [(t, -4.03, 0.0), (t, 4.03, 0.0)], (0, 0, 1), S, smooth=20, flip=True)
        sweep(g, prof, [(-4.03, t, 0.0), (4.03, t, 0.0)], (0, 0, 1), S, smooth=20, flip=True)
    for i in range(9):
        for j in range(9):
            x, y = -4.0 + i, -4.0 + j
            main = i % 4 == 0 or j % 4 == 0
            d = 0.1 if (i % 4 == 0 and j % 4 == 0) else (0.077 if main else 0.077)
            g.box((x - 0.05, y - 0.05, -d - 0.006), (x + 0.05, y + 0.05, -d), Sd if rng.random() < 0.2 else S, skip=('+z',))
            for (dx, dy) in ((-0.03, -0.03), (0.03, 0.03)):
                g.face([(x + dx - 0.006, y + dy - 0.006, -d - 0.0066), (x + dx + 0.006, y + dy - 0.006, -d - 0.0066),
                        (x + dx + 0.006, y + dy + 0.006, -d - 0.0066), (x + dx - 0.006, y + dy + 0.006, -d - 0.0066)], M['black'], out=(0, 0, -1))
    g.build('oberlicht')
