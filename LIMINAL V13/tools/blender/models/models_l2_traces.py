"""V13.3 Level 2: Spuren der Skin-Stealer (KREATUR_L2_Spuren.md; Wikidot Level 2 Rev. 93, Entity 10 Rev. 61).

Laut Level-2-Seite haben die Skin-Stealer die Ebene nach dem grossen Stromausfall verlassen (oder sind verhungert - sie
fressen nur Menschen; die Hitze mancher Gaenge reizte sie und liess die getragene Haut schmelzen). Entity 10: sie
ziehen Menschen die Haut ab und tragen sie, bis sie nach etwa einem Tag ueber die Oberflaeche verdaut ist; ihr Blut ist
klar. Im Spiel kein lebendes Wesen - nur was sie zurueckliessen:

  skin_shed       abgestreifte, vertrocknete Menschenhaut am Boden (v0: flach und zerknittert, das leere Gesicht nach
                  oben, der Ruecken aufgerissen; v1: in der Hitze halb geschmolzen, glasig in einer Lache)
  skin_hung       eine Haut an einem Wandhaken aufgehaengt wie ein Mantel (Gesicht in die Laenge gezogen)
  skin_scratches  Kratzspuren in der kreidigen Wand (vier Krallen und Daumen, Hautfetzen in den Rillen)
  skin_lair       verlassene Lagerstelle: Mulde aus zerrissener Kleidung, gefaltete Haeute, abgenagte Menschenknochen,
                  Habe der Opfer, Flecken aus klarem Blut

Dazu fuer die Death Rats (KREATUR_DeathRat.md; Level 2: sie fressen die Fluessigkeiten in Faessern und Essensreste):
  food_scraps     Essensreste neben einem Fass, das leckt (v0: Konservendose mit ausgelaufenem Inhalt, Verpackung; v1:
                  angenagtes Brot, Kekse, Papier), eine Lache aus dem Fass, Rattenkot

Eine leere Haut ist keine duenne Puppe: plattgedrueckt ist ein Glied so breit wie sein halber Umfang (Oberschenkel
etwa 27 cm, Oberarm 15 cm, Rumpf 45 cm). Darum entstehen die Haeute als zerknitterte Lage: Umriss aus 2D-Kapseln,
darauf ein Hoehenfeld aus Falten, Beulen und Runzeln (Distanzfeld, creatures/sdf.py, Helfer aus models_l3).

Konventionen wie das Spiel: Meter, z oben, Blick entlang +x; Bodenmodelle: Ursprung am Boden; Wandmodelle: Wandflaeche
x = 0, Ursprung auf Bodenhoehe.
"""
import math
import random

import numpy as np
from mathutils import Vector

from models import models_l1 as L1
from models.models_l1_phenomena import blob2, poly_on
from models.models_l1_sectors import Kit, blk, blob
from models.models_l3 import FieldPart, Pcone, Pell, Ptube, mat, rotm, sculpt, sdf

V = Vector
Z3 = (0.0, 0.0, 1.0)


def _mats():
    return {
        # vertrocknete Haut: pergamentartig, graugelb, matt; in Falten dunkler, an duennen Raendern heller
        # Runde 3: dunkler und entsaettigt (vorher cremig-rosa), Flecken nur knapp abgesetzt (vorher Tarnmuster)
        'skin': mat('haut_trocken', (0.40, 0.33, 0.24), 'cardboard', r=1.3),  # Runde 5: noch etwas dunkler (im Licht der Ebene zu hell)
        'skin_dark': mat('haut_falte', (0.23, 0.18, 0.12), 'cardboard', r=1.5),
        'skin_pale': mat('haut_duenn', (0.52, 0.45, 0.33), 'cardboard', r=1.2),
        'skin_spot': mat('haut_fleckig', (0.39, 0.31, 0.22), 'cardboard', r=1.35),
        'skin_bruise': mat('haut_totenfleck', (0.37, 0.30, 0.25), 'cardboard', r=1.35),
        # innen: mit getrocknetem klarem Blut verglast (Entity 10: das Blut ist voellig durchsichtig)
        'skin_in': mat('haut_innen', (0.30, 0.21, 0.13), 'plastic', r=0.3),
        'hair': mat('haar_verfilzt', (0.13, 0.10, 0.07), 'fabric', r=1.35),
        'glaze': mat('blut_klar_getrocknet', (0.30, 0.28, 0.21), 'plastic', r=0.12),
        'glaze_edge': mat('blut_klar_rand', (0.21, 0.18, 0.12), 'plastic', r=0.22),
        'melt': mat('haut_geschmolzen', (0.30, 0.22, 0.14), 'plastic', r=0.22),
        'melt_dark': mat('haut_geschmolzen_dunkel', (0.15, 0.10, 0.06), 'plastic', r=0.26),
        'nail': mat('fingernagel', (0.70, 0.64, 0.52), 'ceramic', r=1.1),
    }


# --- Werkzeug: zerknitterte Lage --------------------------------------------------------------------------------------
def _seg2(u, v, c):
    ax, ay, bx, by, ra, rb = c
    vx, vy = bx - ax, by - ay
    t = np.clip(((u - ax) * vx + (v - ay) * vy) / (vx * vx + vy * vy + 1e-12), 0.0, 1.0)
    return np.hypot(u - (ax + vx * t), v - (ay + vy * t)) - (ra + (rb - ra) * t)


def _smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return b * (1 - h) + a * h - k * h * (1 - h)


class Outline:
    """Umriss in der Ebene (u, v): Vereinigung verjuengter Kapseln (ax, ay, bx, by, ra, rb), Loecher ebenso."""

    def __init__(self, caps, holes=(), k=0.025):
        self.caps, self.holes, self.k = list(caps), list(holes), k

    def d(self, u, v):
        d = None
        for c in self.caps:
            di = _seg2(u, v, c)
            d = di if d is None else _smin(d, di, self.k)
        for h in self.holes:
            d = np.maximum(d, -_seg2(u, v, h))
        return d

    def bounds(self):
        us = [c[0] for c in self.caps] + [c[2] for c in self.caps]
        vs = [c[1] for c in self.caps] + [c[3] for c in self.caps]
        r = max(max(c[4], c[5]) for c in self.caps)
        return min(us) - r, max(us) + r, min(vs) - r, max(vs) + r


def _n2(u, v, freq, seed, oct=3):
    P = np.stack([u, v, np.zeros_like(u)], axis=1)
    return sdf.fbm(P, freq, oct, seed)


def _r2(u, v, freq, seed, oct=2, su=1.0, sv=1.0, warp=0.04):
    """Grate (0..1) in der Ebene - unregelmaessige Falten (Runde 3: statt Sinus-Rippen); su/sv strecken die Richtung."""
    wu = u + warp * _n2(u, v, 5.0, seed + 7, 2)
    wv = v + warp * _n2(u, v, 5.0, seed + 8, 2)
    P = np.stack([wu * su, wv * sv, np.zeros_like(u)], axis=1)
    return sdf.ridged(P, freq, oct, seed)


def sheet(outline, hf, t0=0.0035, w0=0.0, axis='z', hmax=0.08, fade=0.035, k=0.0, ragged=0.0, rseed=5):
    """Lage auf einer Flaeche: axis 'z' (Boden: u = x, v = y, Hoehe z) oder 'x' (Wand: u = y, v = z, Abstand x).
    hf(u, v) >= 0 ist die Oberkante ueber w0; zum Umriss hin laeuft sie flach aus (fade) - der Rand ist duenn.
    ragged > 0: ausgefranster, eingerissener Rand (Runde 4: vorher glatt wie ausgeschnitten)."""
    def fn(P):
        if axis == 'z':
            u, v, w = P[:, 0], P[:, 1], P[:, 2]
        else:
            u, v, w = P[:, 1], P[:, 2], P[:, 0]
        d2 = outline.d(u, v)
        if ragged > 0.0:
            d2 = d2 + ragged * (_n2(u, v, 22.0, rseed, 2) + 0.6 * _r2(u, v, 40.0, rseed + 1, 1) - 0.25)
        e = np.clip(-d2 / fade, 0.0, 1.0)
        h = hf(u, v) * (e * e * (3 - 2 * e))
        return np.maximum(np.abs(w - (w0 + t0 + 0.5 * h)) - (t0 + 0.5 * h), d2)
    u0, u1, v0, v1 = outline.bounds()
    if axis == 'z':
        lo, hi = (u0, v0, w0 - 0.004), (u1, v1, w0 + hmax + 2 * t0)
    else:
        lo, hi = (w0 - 0.004, u0, v0), (w0 + hmax + 2 * t0, u1, v1)
    return FieldPart(fn, lo, hi, k=k)


def paint_vec(ob, parts, disp, mats, pick, cav_scale=0.01):
    """Wie paint_by, aber pick(C, N, cav, edge) bekommt alle Flaechen auf einmal (numpy) und gibt Indizes zurueck."""
    me = ob.data
    nv = len(me.vertices)
    Vv = np.zeros(nv * 3)
    me.vertices.foreach_get('co', Vv)
    Vv = Vv.reshape(-1, 3)
    tri = np.array([p.vertices[:3] for p in me.polygons], dtype=np.int64)
    Nn = sdf.vertex_normals(Vv, tri) if len(tri) else np.zeros_like(Vv)
    cav, edge = sdf.cavity_edge(parts, Vv, Nn, disp, cav_scale)
    C = Vv[tri].mean(axis=1)
    FN = np.cross(Vv[tri[:, 1]] - Vv[tri[:, 0]], Vv[tri[:, 2]] - Vv[tri[:, 0]])
    FN /= np.linalg.norm(FN, axis=1, keepdims=True) + 1e-12
    idx = np.asarray(pick(C, FN, cav[tri].mean(axis=1), edge[tri].mean(axis=1)), dtype=np.int64)
    for m in mats:
        me.materials.append(m)
    me.polygons.foreach_set('material_index', idx.tolist())
    for p in me.polygons:
        p.use_smooth = True


def _mottle(C, seed, freq=11.0):
    return sdf.fbm(np.asarray(C, dtype=np.float64), freq, 3, seed)


# --- Haut eines Menschen, flach ausgelegt -----------------------------------------------------------------------------
def _hand_caps(wx, wy, dx, dy, side, spread=1.0, scale=1.0):
    """Leere Hand, plattgedrueckt: Handteller, vier Finger, Daumen (je eine Kapsel; Breite = halber Umfang)."""
    l = math.hypot(dx, dy)
    dx, dy = dx / l, dy / l
    sx, sy = -dy * side, dx * side
    S = scale
    caps = [(wx, wy, wx + dx * 0.075 * S, wy + dy * 0.075 * S, 0.042 * S, 0.052 * S)]
    tips = []
    for f in range(5):
        if f == 4:
            bx, by = wx + dx * 0.03 * S + sx * 0.045 * S, wy + dy * 0.03 * S + sy * 0.045 * S
            fx, fy = dx * 0.45 + sx * 0.9, dy * 0.45 + sy * 0.9
            L = 0.07 * S
        else:
            o = (f - 1.5) * 0.026 * S
            bx, by = wx + dx * 0.095 * S + sx * o, wy + dy * 0.095 * S + sy * o
            fx, fy = dx + sx * (f - 1.5) * 0.2 * spread, dy + sy * (f - 1.5) * 0.2 * spread
            L = [0.085, 0.098, 0.092, 0.075][f] * S
        fl = math.hypot(fx, fy)
        fx, fy = fx / fl, fy / fl
        caps.append((bx, by, bx + fx * L, by + fy * L, 0.0145 * S, 0.010 * S))
        tips.append((bx + fx * L, by + fy * L, fx, fy))
    return caps, tips


def _person(face=True, slit=True):
    """Umriss und Merkmale einer flach liegenden Haut: Kopf nach +x, Gesicht nach oben (Kopfhaut umgeschlagen)."""
    caps = [(-0.20, 0.0, 0.05, 0.0, 0.205, 0.188), (0.05, 0.0, 0.29, 0.0, 0.188, 0.222), (0.29, 0.0, 0.37, 0.0, 0.222, 0.13),
            (0.37, 0.0, 0.48, 0.012, 0.072, 0.066), (0.53, 0.018, 0.69, 0.03, 0.124, 0.112)]
    feats = {'face_c': (0.625, 0.022), 'nails': []}
    # Ohren
    caps += [(0.625, 0.022 + g * 0.125, 0.645, 0.022 + g * 0.13, 0.024, 0.02) for g in (1, -1)]
    # Arme: links am Koerper entlang, rechts weggestreckt
    armL = [(0.30, 0.20), (0.11, 0.34), (-0.11, 0.38)]
    armR = [(0.30, -0.20), (0.40, -0.45), (0.30, -0.66)]
    for arm in (armL, armR):
        caps += [(arm[0][0], arm[0][1], arm[1][0], arm[1][1], 0.078, 0.062), (arm[1][0], arm[1][1], arm[2][0], arm[2][1], 0.062, 0.042)]
    hc, tl = _hand_caps(armL[2][0], armL[2][1], armL[2][0] - armL[1][0], armL[2][1] - armL[1][1], 1, 1.3)
    caps += hc
    hc, tr = _hand_caps(armR[2][0], armR[2][1], armR[2][0] - armR[1][0], armR[2][1] - armR[1][1], -1, 0.7)
    caps += hc
    feats['nails'] = tl + tr
    # Beine: links gestreckt (mit Fuss), rechts nur der Oberschenkel - die Wade liegt umgeschlagen obenauf (eigene Lage)
    caps += [(-0.24, 0.10, -0.66, 0.145, 0.132, 0.085), (-0.66, 0.145, -1.02, 0.125, 0.085, 0.050),
             (-1.02, 0.125, -1.06, 0.13, 0.050, 0.055), (-1.06, 0.13, -1.25, 0.15, 0.058, 0.046)]
    caps += [(-1.25 - 0.012, 0.15 + (t - 2) * 0.022, -1.29 - 0.006 * abs(t - 2), 0.152 + (t - 2) * 0.026, 0.013, 0.010) for t in range(5)]
    caps += [(-0.24, -0.10, -0.62, -0.165, 0.132, 0.09)]
    holes = []
    fc = feats['face_c']
    if face:
        for g in (1, -1):
            holes.append((fc[0] + 0.030, fc[1] + g * 0.042, fc[0] + 0.033, fc[1] + g * 0.046, 0.0185, 0.0165))  # Augen
            holes.append((fc[0] - 0.022, fc[1] + g * 0.009, fc[0] - 0.022, fc[1] + g * 0.009, 0.005, 0.005))   # Nasenloecher
        holes.append((fc[0] - 0.062, fc[1] - 0.030, fc[0] - 0.060, fc[1] + 0.034, 0.012, 0.014))                # Mund
    if slit:
        holes.append((-0.10, 0.004, 0.36, 0.018, 0.010, 0.014))  # Riss entlang des Rueckgrats
    return caps, holes, feats


def _hf_skin(feats, seed, slit=True, face=True):
    fx, fy = feats['face_c']

    def hf(u, v):
        # Runde 4: mehr Relief (vorher flach wie Pappe): hohe Beulen, gestauchte Falten quer zu den Gliedern
        h = 0.006 + 0.020 * np.clip(_n2(u, v, 5.0, seed) + 0.2, 0.0, 1.3)            # grosse Beulen
        h += 0.017 * _r2(u, v, 8.0, seed + 1, su=1.0, sv=0.45) ** 3                      # Falten quer zum Koerper
        h += 0.008 * _r2(u, v, 15.0, seed + 2, su=0.5, sv=1.0) ** 5                      # feinere, laengs
        h += 0.016 * np.exp(-((u - 0.12) / 0.17) ** 2 - (v / 0.10) ** 2)                 # Luft unter dem Rumpf
        if slit:  # eingerollte Raender des Risses
            sl = 0.004 + (u + 0.10) / 0.46 * 0.014
            m = np.clip((u + 0.12) / 0.04, 0, 1) * np.clip((0.38 - u) / 0.04, 0, 1)
            h += 0.017 * m * np.exp(-((np.abs(v - sl) - 0.026) / 0.010) ** 2)
        if face:  # Nasenruecken, Brauen, Lippenwulst, Kinn
            du, dv = u - fx, v - fy
            h += 0.011 * np.exp(-((du + 0.002) / 0.026) ** 2 - (dv / 0.010) ** 2)
            h += 0.006 * np.exp(-((du - 0.058) / 0.012) ** 2 - ((np.abs(dv) - 0.045) / 0.024) ** 2)
            h += 0.005 * np.exp(-((du + 0.062) / 0.016) ** 2 - (dv / 0.040) ** 2)
            h *= 1.0 - 0.6 * np.exp(-(du / 0.06) ** 2 - (dv / 0.07) ** 2) * (h > 0.02)
        return h
    return hf


def _hair(c, fwd, rng, n=160, length=0.11, spread=0.09, z=0.012, r0=0.0032):
    """Verfilztes Haar in Straehnen, flach nach fwd gestrichen."""
    c = np.asarray(c, dtype=np.float64)
    f = np.asarray(fwd, dtype=np.float64)
    f = f / np.linalg.norm(f)
    s = np.cross(np.array(Z3), f)
    out = []
    for i in range(n):
        o = c + s * rng.uniform(-spread, spread) + f * rng.uniform(-0.02, 0.03)
        d = f + s * rng.uniform(-0.6, 0.6)
        d = d / np.linalg.norm(d)
        L = length * rng.uniform(0.4, 1.2)
        p1 = o + d * L * 0.5 + s * rng.uniform(-0.015, 0.015)
        p2 = o + d * L + s * rng.uniform(-0.03, 0.03)
        out.append(Pcone(o + np.array([0, 0, z]), p1 + np.array([0, 0, z * 0.7]), r0, r0 * 0.75, k=0.003))
        out.append(Pcone(p1 + np.array([0, 0, z * 0.7]), p2 + np.array([0, 0, z * 0.35]), r0 * 0.75, r0 * 0.3, k=0.002))
    return out


def _glaze_pool(rng, cx, cy, rx, ry, n=9, h=0.0018):
    """Lache aus getrocknetem klarem Blut: flache, unregelmaessige Linsen."""
    parts = []
    for i in range(n):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.0, 0.75)
        x, y = cx + math.cos(a) * rx * r, cy + math.sin(a) * ry * r
        parts.append(Pell((x, y, 0.0), (rx * rng.uniform(0.25, 0.5), ry * rng.uniform(0.25, 0.5), h), k=0.03))
    return parts


def _nails(tips, z, scale=1.0):
    out = []
    for (x, y, dx, dy) in tips:
        out.append(Pell((x - dx * 0.008 * scale, y - dy * 0.008 * scale, z), (0.0085 * scale, 0.0072 * scale, 0.0018), k=0.0,
                        rot=rotm(V(Z3), math.atan2(dy, dx))))
    return out


def _wrinkle_disp(seed, amp=0.0011):
    def disp(P):
        return amp * sdf.fbm(P, 95.0, 3, seed) + 0.6 * amp * (1 - np.abs(np.sin(P[:, 0] * 210 + 6 * sdf.fbm(P, 20.0, 2, seed + 1)))) ** 6
    return disp


# =====================================================================================================================
# skin_shed: abgestreifte Haut am Boden
# =====================================================================================================================
def build_skin_shed():
    """Leere Menschenhaut, plattgedrueckt und zerknittert, Kopf nach +x. Entlang des Rueckgrats klafft der Riss, durch
    den das Wesen ausgestiegen ist (Raender eingerollt, innen verglast vom klaren Blut). Die Kopfhaut ist umgeschlagen -
    das leere Gesicht sieht nach oben. Die rechte Wade liegt umgeschlagen auf dem Oberschenkel, eine Hand ist offen."""
    rng = random.Random(1010)
    nrng = np.random.default_rng(1010)
    M = _mats()
    caps, holes, feats = _person()
    body = sheet(Outline(caps, holes, k=0.03), _hf_skin(feats, 3), hmax=0.11, ragged=0.012, rseed=6)
    # umgeschlagene Wade mit Fuss: zweite Lage auf dem Oberschenkel
    calf = Outline([(-0.60, -0.17, -0.36, -0.30, 0.088, 0.052), (-0.36, -0.30, -0.33, -0.31, 0.052, 0.056),
                    (-0.33, -0.31, -0.17, -0.40, 0.058, 0.046)] +
                   [(-0.17 + 0.01, -0.40 + (t - 2) * 0.024 - 0.01, -0.13, -0.42 + (t - 2) * 0.026, 0.013, 0.010) for t in range(5)], k=0.03)
    leg2 = sheet(calf, lambda u, v: 0.008 + 0.012 * np.clip(_n2(u, v, 7.0, 21) + 0.3, 0, 1.2)
                 + 0.014 * _r2(u, v, 9.0, 22, su=1.0, sv=0.6) ** 3, w0=0.016, hmax=0.07, ragged=0.01, rseed=8)
    parts = [body, leg2]
    disp = _wrinkle_disp(5)
    ob = sculpt('haut', parts, 0.0022, disp, target=30000)
    pal = [M['skin'], M['skin_dark'], M['skin_pale'], M['skin_spot'], M['skin_in'], M['skin_bruise']]
    fx, fy = feats['face_c']

    def pick(C, N, cav, edge):
        x, y, z = C[:, 0], C[:, 1], C[:, 2]
        out = np.zeros(len(C), dtype=np.int64)
        mo = _mottle(C, 31, 7.0)  # Runde 3: grosse, weiche Flecken statt Tarnmuster
        out[mo > 0.35] = 3
        out[(mo < -0.38) & (np.hypot(x - 0.1, y) < 0.32)] = 5   # Totenflecken am Rumpf
        out[(edge > 0.6) & (cav < 0.25)] = 2
        out[(cav > 0.5) | (N[:, 2] < -0.3)] = 1
        # Innenseite der eingerollten Raender: nur Flaechen, die zum Riss hin zeigen (Runde 3: vorher Kloetze)
        sl = 0.004 + (x + 0.10) / 0.46 * 0.014
        toward = N[:, 1] * np.sign(sl - y) > 0.25
        out[(np.abs(y - sl) < 0.036) & (x > -0.11) & (x < 0.37) & toward] = 4
        # um Augen und Mund: dunkler (Raender der Loecher)
        out[(np.hypot(x - fx - 0.031, np.abs(y - fy) - 0.044) < 0.026) & (z < 0.02)] = 1
        return out
    paint_vec(ob, parts, disp, pal, pick, cav_scale=0.007)
    nob = sculpt('naegel', _nails(feats['nails'], 0.0085), 0.0012, None, target=1600)
    nob.data.materials.append(M['nail'])
    # Haar auf der umgeschlagenen Kopfhaut (zur Wand hin), verfilzt und kurz
    # Runde 3: feiner, flach und in verklebten Straehnen (vorher ein schwarzer Besen)
    hob = sculpt('haar', _hair((0.715, 0.03, 0.007), (1.0, 0.15, 0.0), nrng, n=260, length=0.075, spread=0.10, z=0.007, r0=0.0019), 0.0011,
                 lambda P: 0.0006 * sdf.fbm(P, 150.0, 2, 4), target=9000)
    hob.data.materials.append(M['hair'])
    # Lache aus klarem Blut, eingetrocknet - glasig um den Riss, unter dem Kopf und den Haenden
    gob = sculpt('lache', _glaze_pool(rng, 0.10, 0.0, 0.48, 0.30, 12) + _glaze_pool(rng, 0.62, 0.02, 0.18, 0.15, 5)
                 + _glaze_pool(rng, -0.15, 0.40, 0.12, 0.10, 3), 0.004, lambda P: 0.0006 * sdf.fbm(P, 40.0, 2, 9), target=3500)
    paint_vec(gob, [], None, [M['glaze'], M['glaze_edge']], lambda C, N, cav, edge: (_mottle(C, 41, 6.0) > 0.3).astype(np.int64))


def build_skin_shed__v1():
    """In der Hitze halb geschmolzene Haut (Wikidot Level 2: die Hitze liess die getragene, verwesende Haut schmelzen):
    der Rumpf ist zu einer glasigen, braunen Lache zerlaufen, mit Blasen und Laeufern; aus ihr ragen noch das Gesicht
    (verzogen, ein Auge zugelaufen), die Schultern, ein Arm mit der Hand und ein Fuss."""
    rng = random.Random(1011)
    M = _mats()
    caps, holes, feats = _person(slit=False)
    # nur Teile bleiben Haut: Kopf, Schultern, linker Arm mit Hand, linker Fuss
    keep = [c for c in caps if c[0] > 0.25 or c[1] > 0.15 or c[0] < -0.95]
    holes = [h for h in holes if not (h[1] < feats['face_c'][1] - 0.02 and h[0] > 0.6)]  # ein Auge zugelaufen
    skin = sheet(Outline(keep, holes, k=0.03), _hf_skin(feats, 13, slit=False), hmax=0.1, ragged=0.014, rseed=14)
    # die Lache: Linsen um den Rumpf, Zungen in alle Richtungen
    blobs = []
    for i in range(14):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.0, 0.55)
        x, y = -0.12 + math.cos(a) * 0.62 * r, math.sin(a) * 0.36 * r
        rr = rng.uniform(0.14, 0.26)
        blobs.append((x, y, x + rng.uniform(-0.05, 0.05), y + rng.uniform(-0.05, 0.05), rr, rr * rng.uniform(0.7, 1.0)))
    for i in range(5):  # Runde 4: wenige, breite, ungleiche Zungen (vorher ein Stern wie ein Comic-Klecks)
        a = rng.uniform(0, math.tau)
        bend = rng.uniform(-0.4, 0.4)
        blobs.append((-0.12 + math.cos(a) * 0.3, math.sin(a) * 0.2, -0.12 + math.cos(a + bend) * rng.uniform(0.5, 0.72),
                      math.sin(a + bend) * rng.uniform(0.3, 0.46), rng.uniform(0.07, 0.12), rng.uniform(0.025, 0.05)))
    pool_o = Outline(blobs, k=0.06)

    def pool_h(u, v):
        d = -pool_o.d(u, v)
        # Runde 3: Fliessspuren unregelmaessig (vorher Ringe wie eine Zielscheibe)
        return 0.004 + 0.022 * np.clip(d / 0.25, 0, 1) ** 0.7 + 0.004 * _r2(u, v, 7.0, 7, su=1.0, sv=1.0, warp=0.08) ** 3
    pool = sheet(pool_o, pool_h, t0=0.002, hmax=0.04, fade=0.06, ragged=0.02, rseed=17)
    bubbles = []
    for i in range(44):
        a = rng.uniform(0, math.tau)
        r = rng.uniform(0.0, 0.5)
        x, y = -0.12 + math.cos(a) * 0.55 * r, math.sin(a) * 0.32 * r
        rb = rng.uniform(0.005, 0.02)
        zt = float(pool_h(np.array([x]), np.array([y]))[0]) + 0.004
        bubbles.append(Pell((x, y, zt), (rb, rb, rb * 0.65), k=0.006))
        if rng.random() < 0.4:  # geplatzte Blase: Krater
            bubbles.append(Pell((x, y, zt + rb * 0.75), (rb * 0.72, rb * 0.72, rb * 0.6), k=0.003, op='sub'))
    parts = [pool, skin] + bubbles
    disp = _wrinkle_disp(15, amp=0.0008)
    ob = sculpt('geschmolzen', parts, 0.0022, disp, target=28000)
    pal = [M['melt'], M['melt_dark'], M['skin'], M['skin_dark'], M['glaze'], M['glaze_edge'], M['skin_spot']]

    def pick(C, N, cav, edge):
        x, y, z = C[:, 0], C[:, 1], C[:, 2]
        ph = pool_h(x, y) * (pool_o.d(x, y) < 0)
        out = np.where(cav > 0.5, 1, 0)
        above = z > ph + 0.005
        near = z < ph + 0.014
        out[above & ~near] = 6  # Runde 3: was noch Haut ist, zusammenhaengend; Runde 5: angeschmort (fleckig statt bleich)
        out[above & ~near & (_mottle(C, 51, 6.0) > 0.45)] = 6
        out[above & ~near & (cav > 0.55)] = 3
        out[(pool_o.d(x, y) > -0.05) & (z < 0.006)] = 5
        out[(pool_o.d(x, y) > -0.01) & (z < 0.003)] = 4
        return out
    paint_vec(ob, parts, disp, pal, pick, cav_scale=0.006)
    nob = sculpt('naegel', _nails([t for t in feats['nails'] if t[1] > 0], 0.0085), 0.0012, None, target=800)
    nob.data.materials.append(M['nail'])


# =====================================================================================================================
# skin_hung: aufgehaengte Haut (Wand)
# =====================================================================================================================
def build_skin_hung():
    """Eine Haut an einem Haken in der Wand, wie ein Mantel aufgehaengt - an der Kopfhaut. Durch das Gewicht ist das
    Gesicht lang gezogen (Augen und Mund zu Schlitzen), der leere Leib haengt in tiefen senkrechten Falten und staucht
    sich unten, die Aermel haengen, die Beine schleifen fast am Boden. Wandflaeche x = 0, Ursprung am Boden."""
    rng = random.Random(1020)
    nrng = np.random.default_rng(1020)
    M = _mats()
    hz = 1.84
    # Umriss in (y, z): Kopf am Haken gestreckt, Schultern haengen, Leib, Aermel, Beine
    caps = [(0.0, hz - 0.03, 0.0, hz - 0.12, 0.035, 0.085), (0.0, hz - 0.12, 0.0, hz - 0.36, 0.085, 0.075),
            (0.0, hz - 0.36, 0.0, hz - 0.46, 0.06, 0.07),
            (0.0, 1.24, 0.0, 1.24, 0.14, 0.15),
            (0.0, 1.24, 0.0, 0.95, 0.155, 0.18), (0.0, 0.95, 0.0, 0.74, 0.18, 0.14)]
    # Runde 5: ohne Knochen fallen die Schultern ab - vom Hals schraeg nach unten wie ein Mantel am Haken (vorher gerade
    # Schultern auf 1,31 m: der Umriss eines stehenden Menschen, las sich wie ein Pappaufsteller)
    for g in (1, -1):
        caps += [(0.0, 1.40, g * 0.15, 1.22, 0.07, 0.075)]
    for g in (1, -1):  # Runde 4: Aermel frei neben dem Leib (vorher verschmolzen)
        caps += [(g * 0.15, 1.22, g * 0.26, 0.96, 0.058, 0.05), (g * 0.26, 0.96, g * 0.27, 0.72, 0.05, 0.038)]
        caps += [(g * 0.27, 0.72, g * 0.275, 0.66, 0.038, 0.045)]
        for f in range(5):  # Finger haengen
            y = g * (0.275 + (f - 1.5) * 0.022) if f < 4 else g * 0.315
            z0 = 0.62 if f < 4 else 0.66
            caps.append((y, z0, y + g * 0.004, z0 - [0.09, 0.10, 0.095, 0.08, 0.06][f], 0.013, 0.009))
        caps += [(g * 0.09, 0.78, g * 0.105, 0.44, 0.13, 0.09), (g * 0.105, 0.44, g * 0.10, 0.16, 0.09, 0.055),
                 (g * 0.10, 0.16, g * 0.10, 0.06, 0.055, 0.05)]
    fz = hz - 0.22
    holes = []
    for g in (1, -1):
        holes.append((g * 0.036, fz + 0.08, g * 0.034, fz + 0.01, 0.014, 0.011))  # Augen, lang gezogen
    holes.append((0.0, fz - 0.08, 0.0, fz - 0.13, 0.016, 0.022))                  # Mund, aufgezogen
    holes.append((0.006, fz - 0.025, 0.006, fz - 0.03, 0.004, 0.004))
    holes.append((-0.006, fz - 0.025, -0.006, fz - 0.03, 0.004, 0.004))

    def hf(u, v):
        h = 0.010 + 0.012 * np.clip(_n2(u, v, 5.0, 23) + 0.3, 0, 1.2)
        # Runde 3: Falten aus gestreckten Graten (vorher gleichmaessige Wellen wie ein Vorhang)
        h += 0.024 * _r2(u, v, 7.0, 24, su=1.0, sv=0.18, warp=0.05) ** 3 * np.clip((1.45 - v) / 0.3, 0, 1)
        h += 0.012 * _r2(u, v, 9.0, 25, su=0.35, sv=1.0, warp=0.04) ** 5 * np.clip((1.0 - v) / 0.3, 0, 1)  # unten gestaucht
        h += 0.05 * np.exp(-(u / 0.13) ** 2 - ((v - 1.05) / 0.24) ** 2)                                              # Bauch haengt vor
        h += 0.022 * np.exp(-((np.abs(u) - 0.265) / 0.035) ** 2) * (v > 0.66) * (v < 1.2)                              # Aermel rund
        # Runde 5: schraege Zugfalten vom Hals zu den abfallenden Schultern
        h += 0.016 * np.exp(-((v - (1.40 - 1.2 * np.abs(u))) / 0.025) ** 2) * (np.abs(u) < 0.17) * (np.abs(u) > 0.04)
        dz = v - fz
        h += 0.012 * np.exp(-(u / 0.010) ** 2 - ((dz + 0.0) / 0.05) ** 2)  # Nasenruecken
        h += 0.006 * np.exp(-((np.abs(u) - 0.04) / 0.02) ** 2 - ((dz - 0.11) / 0.012) ** 2)
        return h
    body = sheet(Outline(caps, holes, k=0.02), hf, t0=0.004, w0=0.004, axis='x', hmax=0.13, ragged=0.012, rseed=28)
    parts = [body]
    disp = _wrinkle_disp(27)
    ob = sculpt('haut_haengend', parts, 0.0024, disp, target=28000)
    pal = [M['skin'], M['skin_dark'], M['skin_pale'], M['skin_spot'], M['skin_in'], M['skin_bruise']]

    def pick(C, N, cav, edge):
        out = np.zeros(len(C), dtype=np.int64)
        mo = _mottle(C, 61, 6.0)
        out[mo > 0.35] = 3
        out[(mo < -0.3) & (C[:, 2] < 0.95) & (C[:, 2] > 0.5)] = 5  # Totenflecken unten (das Blut sackte)
        out[(edge > 0.6) & (cav < 0.25)] = 2
        out[cav > 0.5] = 1
        out[N[:, 0] < -0.4] = 4
        return out
    paint_vec(ob, parts, disp, pal, pick, cav_scale=0.008)
    # Haken: Flachstahl aus der Wand, nach oben gebogen, rostig; das Haar ist darum gewickelt
    k = Kit()
    rust = mat('haken_rost', (0.36, 0.22, 0.13), 'metal', tex='metal_rusty_iron', r=1.4, m=0.2)
    plate = mat('haken_platte', (0.22, 0.21, 0.20), 'metal', r=1.0)
    blk(k(plate, 30), (0.008, 0.06, 0.09), (0.004, 0.0, hz + 0.03), bevel=0.002)
    blk(k(rust, 30), (0.10, 0.016, 0.012), (0.05, 0.0, hz + 0.01), bevel=0.002)
    blk(k(rust, 30), (0.014, 0.016, 0.05), (0.096, 0.0, hz + 0.032), bevel=0.002)
    for g in (1, -1):
        blk(k(plate, 30), (0.006, 0.010, 0.010), (0.009, g * 0.02, hz + 0.06), bevel=0.002)
    k.finish()
    hair = _hair((0.05, 0.0, hz + 0.02), (0.0, 1.0, 0.0), nrng, n=50, length=0.05, spread=0.03, z=0.0)
    hair += [Ptube((0.035, -0.06, hz - 0.01), (0.035, 0.06, hz - 0.01), (0.02, 0.026), (0.02, 0.026), up=(1, 0, 0), k=0.012)]
    for i in range(40):  # Straehnen haengen ueber die Stirn herab
        y = rng.uniform(-0.07, 0.07)
        a = np.array([0.03, y, hz - 0.02])
        b = np.array([0.03 + rng.uniform(0.0, 0.02), y + rng.uniform(-0.02, 0.02), hz - rng.uniform(0.06, 0.13)])
        hair.append(Pcone(a, b, 0.003, 0.0012, k=0.002))
    hob = sculpt('haar', hair, 0.0015, lambda P: 0.0012 * sdf.fbm(P, 120.0, 2, 3), target=6000)
    hob.data.materials.append(M['hair'])
    # Tropfspur aus klarem Blut an der Wand und eine Lache darunter
    gl = [Pell((0.003, rng.uniform(-0.14, 0.14), z), (0.0028, rng.uniform(0.004, 0.009), rng.uniform(0.05, 0.14)), k=0.01)
          for z in np.linspace(0.18, 1.05, 10)]
    gl += _glaze_pool(rng, 0.13, 0.0, 0.17, 0.24, 6)
    gob = sculpt('blut', gl, 0.003, None, target=2200)
    gob.data.materials.append(M['glaze'])


# =====================================================================================================================
# skin_scratches: Kratzspuren (Wand)
# =====================================================================================================================
def build_skin_scratches():
    """Kratzspuren eines Skin-Stealers in der kreidigen Betonwand: Hand mit vier Krallen und Daumen, von Schulterhoehe
    herab gezogen, mehrfach; tiefe Rillen (dunkler Grund, ausgebrochene helle Kanten, Kreidepuder), in den Rillen
    bleiche Hautfetzen (die getragene Haut riss beim Kratzen - Auslegung), darunter Kreidestaub, ein glasiger Streifen
    aus klarem Blut."""
    rng = random.Random(1030)
    k = Kit()
    M = _mats()
    chalk = mat('kreide_frei', (0.78, 0.76, 0.70), 'concrete', tex='concrete_chalky', r=1.5)
    chalk2 = mat('kreide_rand', (0.66, 0.64, 0.58), 'concrete', tex='concrete_chalky', r=1.5)
    deep = mat('rille_tief', (0.13, 0.12, 0.11), 'concrete', tex='concrete_rough', r=1.5)
    mid = mat('rille_flanke', (0.34, 0.33, 0.30), 'concrete', tex='concrete_rough', r=1.5)
    dust = mat('kreidestaub', (0.84, 0.82, 0.77), 'concrete', tex='concrete_chalky', r=1.6)
    flake = M['skin_pale']
    flake_d = M['skin']
    U, Wv = V((0, 1, 0)), V((0, 0, 1))
    # drei Haende: (y, z oben, z unten, Neigung, Seite des Daumens)
    sets = [(0.00, 1.58, 0.82, 0.10, 1), (0.36, 1.44, 0.98, -0.07, 1), (-0.32, 1.72, 1.18, 0.16, -1)]
    for si, (y0, z0, z1, lean, hand) in enumerate(sets):
        claws = [(-0.058, 0.0), (-0.019, 0.016), (0.020, 0.020), (0.058, 0.006), (0.105 * hand, -0.22)]
        for ci, (dy, dz) in enumerate(claws):
            n = 11
            top, bot = z0 + dz, z1 + dz * 0.5 + (0.28 if ci == 4 else 0.0)
            pts = [(y0 + dy * hand + lean * (i / (n - 1)) + 0.012 * math.sin(i * 0.9 + ci + si), top - (top - bot) * i / (n - 1))
                   for i in range(n)]
            wdt = [0.026 + 0.008 * math.sin(math.pi * i / (n - 1)) - 0.016 * (i / (n - 1)) ** 1.5 for i in range(n)]
            L, Rr = [], []
            for i, (y, z) in enumerate(pts):
                d = V((pts[min(i + 1, n - 1)][0] - pts[max(i - 1, 0)][0], pts[min(i + 1, n - 1)][1] - pts[max(i - 1, 0)][1], 0))
                s = V((-d.y, d.x, 0)).normalized() * wdt[i] * 0.5
                L.append((y + s.x, z + s.y))
                Rr.append((y - s.x, z - s.y))
            poly_on(k(chalk2), L + list(reversed(Rr)), (0.0010, 0, 0), U, Wv)
            inner = [(y + rng.uniform(-0.0015, 0.0015), z) for (y, z) in pts]
            # Runde 4: drei Toene (Rand, Flanke, Grund) statt schwarzer Linie mit weissem Saum
            poly_on(k(mid), L1.stroke(inner, 0.015), (0.0012, 0, 0), U, Wv)
            poly_on(k(deep), L1.stroke(inner[1:-1], 0.007), (0.0014, 0, 0), U, Wv)
            poly_on(k(chalk), L1.stroke([(y + 0.006, z) for (y, z) in inner[1:-2]], 0.003), (0.0017, 0, 0), U, Wv)  # Lichtkante
            for side in (L, Rr):  # ausgebrochene Kanten
                jag = [(y + rng.uniform(-0.005, 0.005), z + rng.uniform(-0.004, 0.004)) for (y, z) in side]
                poly_on(k(chalk), L1.stroke(jag, 0.0045), (0.0019, 0, 0), U, Wv)
            if rng.random() < 0.55:  # Hautfetzen in der Rille
                i = rng.randint(2, n - 3)
                y, z = pts[i]
                poly_on(k(flake if rng.random() < 0.6 else flake_d), blob2(rng, y, z, rng.uniform(0.01, 0.018), rng.uniform(0.014, 0.03), 9, 0.5),
                        (0.0024, 0, 0), U, Wv)
        for _ in range(8):  # am Ende der Rillen: Kreidebroeckchen
            b = rng.uniform(0.005, 0.014)
            blk(k(chalk, 0), (b, b * 1.2, b), (0.005, y0 + rng.uniform(-0.07, 0.08), z1 + rng.uniform(-0.03, 0.06)), rx=rng.uniform(0, 3), rz=rng.uniform(0, 3))
    glaze = M['glaze']  # glasiger Streifen aus klarem Blut, von der mittleren Hand herab
    for j in range(3):
        y = 0.33 + j * 0.035 + rng.uniform(-0.01, 0.01)
        zt = 1.05 - j * 0.05
        zb = zt - rng.uniform(0.18, 0.38)
        pts = [(y + 0.006 * math.sin(t * 7.0), zt - (zt - zb) * t) for t in np.linspace(0, 1, 8)]
        poly_on(k(glaze), L1.stroke(pts, 0.011 - 0.002 * j), (0.0022, 0, 0), U, Wv)
    for _ in range(4):  # Kreidestaub und Fetzen am Boden vor der Wand
        blob(k(dust, 40), (rng.uniform(0.03, 0.14), rng.uniform(-0.35, 0.4), 0.0), (0, 0, 1), rng.uniform(0.06, 0.13), 0.004, rng, 11, 1.0, 0.45)
    for _ in range(22):
        b = rng.uniform(0.004, 0.016)
        blk(k(chalk, 0), (b, b * 1.3, b * 0.6), (rng.uniform(0.02, 0.28), rng.uniform(-0.4, 0.45), b * 0.3), rz=rng.uniform(0, 3), bevel=0.001)
    for _ in range(6):
        poly_on(k(flake), blob2(rng, rng.uniform(0.04, 0.24), rng.uniform(-0.35, 0.35), rng.uniform(0.012, 0.024), rng.uniform(0.01, 0.018), 9, 0.5),
                (0, 0, 0.0015), V((1, 0, 0)), V((0, 1, 0)))
    k.finish()


# =====================================================================================================================
# skin_lair: verlassene Lagerstelle (Boden)
# =====================================================================================================================
def _bone_femur(a, b, r=0.013):
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = (b - a) / np.linalg.norm(b - a)
    s = np.cross(d, np.array(Z3))
    s = s / (np.linalg.norm(s) + 1e-9)
    parts = [Pcone(a + d * 0.03, b - d * 0.03, r, r * 0.9, k=0.0)]
    parts.append(Pell(a + d * 0.012 + s * 0.012, (0.024, 0.022, 0.022), k=0.012))  # Kopf
    parts.append(Pell(a + d * 0.035 - s * 0.012, (0.018, 0.015, 0.015), k=0.012))
    for g in (1, -1):  # Gelenkrollen unten
        parts.append(Pell(b - d * 0.012 + s * g * 0.016, (0.020, 0.017, 0.018), k=0.012))
    return parts


def _bone_gnaw(a, b, n, rng):
    """Nagespuren: kleine Kerben (sub) nahe den Enden."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    out = []
    for _ in range(n):
        t = rng.choice([rng.uniform(0.05, 0.22), rng.uniform(0.78, 0.95)])
        p = a + (b - a) * t
        out.append(Pell(p + np.array([rng.uniform(-0.006, 0.006), rng.uniform(-0.006, 0.006), 0.014]), (0.006, 0.0025, 0.006), k=0.002, op='sub',
                        rot=rotm(V(Z3), rng.uniform(0, 3))))
    return out


def _rib(c, yaw, span):
    out, pts = [], []
    for t in np.linspace(0, 1, 7):
        a = math.pi * (0.15 + 0.7 * t)
        x, y = math.cos(a) * span * 0.5, math.sin(a) * span * 0.35
        pts.append(np.asarray(c, float) + np.array([x * math.cos(yaw) - y * math.sin(yaw), x * math.sin(yaw) + y * math.cos(yaw), 0.006 + 0.004 * math.sin(math.pi * t)]))
    for p, q in zip(pts[:-1], pts[1:]):
        out.append(Ptube(p, q, (0.0075, 0.0045), (0.0075, 0.0045), k=0.003))
    return out


def _garment(kind, cx, cy, yaw, sc=1.0):
    """Umriss eines Kleidungsstuecks (flach hingeworfen), gedreht um yaw: Jacke, Hose, Hemd, Decke."""
    c, s = math.cos(yaw), math.sin(yaw)

    def T(x, y):
        return cx + (x * c - y * s) * sc, cy + (x * s + y * c) * sc
    if kind == 'jacket':
        raw = [(-0.25, 0, 0.2, 0, 0.24, 0.26), (0.2, 0.2, -0.05, 0.52, 0.09, 0.07), (0.2, -0.2, 0.32, -0.55, 0.09, 0.07), (0.28, 0, 0.32, 0, 0.12, 0.10)]
    elif kind == 'trousers':
        raw = [(0.25, 0, 0.12, 0, 0.2, 0.2), (0.12, 0.09, -0.62, 0.17, 0.12, 0.09), (0.12, -0.09, -0.55, -0.22, 0.12, 0.09)]
    elif kind == 'shirt':
        raw = [(-0.2, 0, 0.18, 0, 0.2, 0.21), (0.16, 0.18, 0.05, 0.42, 0.07, 0.055), (0.16, -0.18, 0.05, -0.42, 0.07, 0.055)]
    else:  # Decke, zerrissen
        raw = [(-0.4, -0.1, 0.35, 0.12, 0.36, 0.30), (0.2, -0.3, 0.5, -0.45, 0.12, 0.05)]
    caps = []
    for (ax, ay, bx, by, ra, rb) in raw:
        a, b = T(ax, ay), T(bx, by)
        caps.append((a[0], a[1], b[0], b[1], ra * sc, rb * sc))
    return Outline(caps, k=0.04)


def build_skin_lair():
    """Verlassene Lagerstelle in einer Ecke (Ursprung am Boden; die Ecke liegt hinten links, x < 0, y > 0): eine Mulde aus
    hingeworfener, zerrissener Kleidung der Opfer, darin ein Stapel gefalteter Haeute (die oberste mit Gesicht), ringsum
    abgenagte Menschenknochen (sie fressen nur Menschen), Habe der Opfer - ein Turnschuh, ein aufgerissener Rucksack, eine
    leere Flasche, eine Taschenlampe - und Flecken aus getrocknetem klarem Blut. Alles verstaubt."""
    rng = random.Random(1040)
    nrng = np.random.default_rng(1040)
    M = _mats()
    cloth = [mat('stoff_jeans', (0.18, 0.21, 0.27), 'fabric', r=1.35), mat('stoff_grau', (0.30, 0.29, 0.27), 'fabric', r=1.4),
             mat('stoff_rot', (0.30, 0.10, 0.08), 'fabric', r=1.35), mat('stoff_khaki', (0.36, 0.32, 0.23), 'fabric', r=1.4),
             mat('stoff_schwarz', (0.06, 0.06, 0.065), 'fabric', r=1.35), mat('stoff_decke', (0.27, 0.23, 0.19), 'fabric', r=1.5),
             mat('stoff_staub', (0.38, 0.36, 0.32), 'fabric', r=1.6)]
    # --- Mulde: Kleidungsstuecke im Kreis, die Raender aufgeworfen (Hoehe waechst nach aussen), innen eine Decke ------
    nest_c = (-0.10, 0.08)
    G = [('blanket', -0.10, 0.08, 0.3, 1.0, 5, 0.0), ('jacket', -0.55, 0.30, 2.4, 1.0, 2, 0.012), ('trousers', 0.25, 0.42, -2.6, 1.0, 0, 0.01),
         ('shirt', -0.45, -0.32, 0.9, 1.0, 1, 0.014), ('jacket', 0.30, -0.26, -0.8, 0.95, 4, 0.016), ('trousers', -0.25, 0.55, 0.2, 0.9, 3, 0.02),
         ('shirt', 0.40, 0.10, 1.6, 0.9, 2, 0.022)]
    sheets, owners = [], []
    for gi, (kind, cx, cy, yaw, sc, mi, z0) in enumerate(G):
        o = _garment(kind, cx, cy, yaw, sc)

        def hf(u, v, gi=gi):
            r = np.hypot(u - nest_c[0], (v - nest_c[1]) * 1.3)
            rim = 0.10 * np.exp(-((r - 0.5) / 0.17) ** 2)  # aufgeworfener Rand der Mulde
            # Runde 4: mehr Volumen (vorher flache Umrisse wie Schnittmuster), Falten aus Graten, zerrissene Raender
            h = 0.012 + rim + 0.036 * np.clip(_n2(u, v, 4.5, 70 + gi) + 0.2, 0, 1.3)
            h += 0.022 * _r2(u, v, 6.0, 80 + gi, su=1.0, sv=0.6, warp=0.05) ** 3
            return h
        sheets.append(sheet(o, hf, t0=0.004, w0=z0, hmax=0.17, fade=0.035, ragged=0.012, rseed=90 + gi))
        owners.append(mi)

    def cloth_disp(P):
        return 0.0016 * sdf.fbm(P, 45.0, 3, 41) + 0.0012 * (1 - np.abs(np.sin(P[:, 0] * 90 + 5 * sdf.fbm(P, 9.0, 2, 42)))) ** 5
    ob = sculpt('kleidung', sheets, 0.0045, cloth_disp, target=24000)

    def pick_cloth(C, N, cav, edge):
        d = np.stack([s.dist(C) for s in sheets], axis=1)
        top = np.argmin(np.abs(d), axis=1)
        out = np.array(owners)[top]
        out[(N[:, 2] > 0.7) & (_mottle(C, 91, 7.0) > 0.4)] = 6  # Staub auf der Oberseite (Runde 3: weniger, dunkler)
        return out
    paint_vec(ob, sheets, cloth_disp, cloth, pick_cloth, cav_scale=0.02)
    # --- gefaltete Haeute in der Mulde (die oberste mit Gesicht) ----------------------------------------------------
    skins = []
    z = 0.035
    for i in range(3):
        yaw = rng.uniform(-0.3, 0.3)
        cx, cy = -0.10 + rng.uniform(-0.02, 0.02), 0.08 + rng.uniform(-0.02, 0.02)
        c, s = math.cos(yaw), math.sin(yaw)
        o = Outline([(cx - 0.15 * c, cy - 0.15 * s, cx + 0.15 * c, cy + 0.15 * s, 0.13, 0.125)], k=0.02)
        skins.append(sheet(o, lambda u, v, i=i: 0.010 + 0.008 * np.clip(_n2(u, v, 9.0, 100 + i) + 0.3, 0, 1)
                           + 0.006 * (1 - np.abs(np.sin(u * 40 + v * 21))) ** 5, t0=0.003, w0=z, hmax=0.04, fade=0.02))
        z += 0.022
    fo = Outline([(-0.17, 0.08, -0.03, 0.09, 0.105, 0.095)],
                 [(-0.075 + 0.03, 0.09 + g * 0.04, -0.072 + 0.03, 0.094 + g * 0.04, 0.016, 0.015) for g in (1, -1)]
                 + [(-0.075 - 0.058, 0.06, -0.075 - 0.056, 0.12, 0.011, 0.012)], k=0.02)

    def fhf(u, v):
        du, dv = u + 0.075, v - 0.09
        h = 0.008 + 0.006 * np.clip(_n2(u, v, 11.0, 111) + 0.3, 0, 1)
        h += 0.010 * np.exp(-((du + 0.002) / 0.024) ** 2 - (dv / 0.009) ** 2)
        h += 0.005 * np.exp(-((du - 0.055) / 0.012) ** 2 - ((np.abs(dv) - 0.042) / 0.022) ** 2)
        return h
    skins.append(sheet(fo, fhf, t0=0.003, w0=z, hmax=0.035, fade=0.02))
    sdisp = _wrinkle_disp(113, amp=0.0008)
    sob = sculpt('haeute', skins, 0.0022, sdisp, target=14000)
    paint_vec(sob, skins, sdisp, [M['skin'], M['skin_dark'], M['skin_pale'], M['skin_spot']],
              lambda C, N, cav, edge: np.where(cav > 0.55, 1, np.where(edge > 0.5, 2, np.where(_mottle(C, 121) > 0.3, 3, 0))), cav_scale=0.006)
    hob = sculpt('haar', _hair((0.0, 0.09, z + 0.004), (1.0, 0.0, 0.0), nrng, n=80, length=0.07, spread=0.07, z=0.004, r0=0.0024), 0.0014, None, target=4000)
    hob.data.materials.append(M['hair'])
    # --- Knochen: zwei Oberschenkel, Rippen, Wirbel, ein Unterkiefer, Fingerknochen - abgenagt -------------------------
    bones = []
    f1 = ((0.62, -0.20, 0.014), (0.28, -0.40, 0.014))
    f2 = ((0.46, 0.52, 0.014), (0.80, 0.36, 0.016))
    bones += _bone_femur(*f1) + _bone_femur(*f2)
    bones += _bone_gnaw(*f1, 5, rng) + _bone_gnaw(*f2, 4, rng)
    for i in range(5):
        bones += _rib((0.66 + rng.uniform(-0.1, 0.1), 0.08 + rng.uniform(-0.15, 0.15), 0.0), rng.uniform(0, math.tau), rng.uniform(0.2, 0.28))
    for i in range(6):
        c = np.array([0.42 + rng.uniform(-0.05, 0.25), -0.02 + rng.uniform(-0.3, 0.2), 0.012])
        bones.append(Pell(c, (0.018, 0.016, 0.012), k=0.0))
        bones.append(Pcone(c, c + np.array([rng.uniform(-0.03, 0.03), rng.uniform(-0.03, 0.03), 0.004]), 0.006, 0.003, k=0.006))
    jaw = np.array([0.70, -0.48, 0.012])
    for t0, t1 in zip(np.linspace(-1.2, 1.2, 7)[:-1], np.linspace(-1.2, 1.2, 7)[1:]):
        bones.append(Ptube(jaw + np.array([math.cos(t0) * 0.045, math.sin(t0) * 0.05, 0.0]), jaw + np.array([math.cos(t1) * 0.045, math.sin(t1) * 0.05, 0.0]),
                           (0.008, 0.012), (0.008, 0.012), k=0.004))
    for i in range(12):
        c = np.array([0.55 + rng.uniform(-0.2, 0.3), 0.15 + rng.uniform(-0.45, 0.3), 0.005])
        d = np.array([math.cos(rng.uniform(0, 6.3)), math.sin(rng.uniform(0, 6.3)), 0.0]) * rng.uniform(0.02, 0.04)
        bones.append(Pcone(c, c + d, 0.005, 0.004, k=0.003))
    bob = sculpt('knochen', bones, 0.0016, lambda P: 0.0004 * sdf.fbm(P, 120.0, 2, 51), target=14000)
    bone_m = [mat('knochen_alt', (0.66, 0.60, 0.48), 'ceramic', r=1.55), mat('knochen_dunkel', (0.42, 0.36, 0.27), 'ceramic', r=1.6),
              mat('knochen_blank', (0.74, 0.69, 0.57), 'ceramic', r=1.2)]
    paint_vec(bob, bones, None, bone_m, lambda C, N, cav, edge: np.where((cav > 0.55) | (N[:, 2] < -0.3), 1, np.where(edge > 0.6, 2, 0)), cav_scale=0.004)
    # --- Habe der Opfer: Turnschuh, Rucksack (aufgerissen), Flasche, Taschenlampe ------------------------------------------
    gear = []
    sh = np.array([-0.78, -0.42, 0.0])
    R = rotm(V(Z3), 0.7)
    gear += [Pell(sh + R @ np.array([0.0, 0.0, 0.045]), (0.13, 0.045, 0.04), k=0.02, rot=R),
             Pell(sh + R @ np.array([0.0, 0.0, 0.012]), (0.135, 0.05, 0.012), k=0.01, rot=R),
             Pell(sh + R @ np.array([-0.07, 0.0, 0.07]), (0.045, 0.04, 0.035), k=0.02, rot=R)]
    rp = np.array([-0.80, 0.55, 0.0])
    R2 = rotm(V(Z3), -0.4)
    gear += [Pell(rp + R2 @ np.array([0, 0, 0.07]), (0.15, 0.19, 0.08), k=0.03, rot=R2),
             Pell(rp + R2 @ np.array([0.04, 0, 0.14]), (0.09, 0.14, 0.05), k=0.04, rot=R2),
             Pell(rp + R2 @ np.array([0.13, 0.02, 0.10]), (0.05, 0.1, 0.06), k=0.03, op='sub', rot=R2)]
    for g in (1, -1):
        a = rp + R2 @ np.array([-0.1, g * 0.1, 0.12])
        gear.append(Ptube(a, a + R2 @ np.array([0.25, g * 0.12, -0.11]), (0.022, 0.006), (0.022, 0.006), k=0.01))
    bt = np.array([0.25, 0.70, 0.0])
    gear += [Pcone(bt + np.array([0, 0, 0.035]), bt + np.array([0.18, 0.05, 0.035]), 0.034, 0.034, k=0.0),
             Pcone(bt + np.array([0.18, 0.05, 0.035]), bt + np.array([0.25, 0.07, 0.035]), 0.032, 0.013, k=0.02)]
    fl = np.array([-0.30, -0.66, 0.0])
    gear += [Pcone(fl + np.array([0, 0, 0.018]), fl + np.array([0.16, -0.04, 0.018]), 0.016, 0.016, k=0.0),
             Pcone(fl + np.array([0.16, -0.04, 0.018]), fl + np.array([0.20, -0.05, 0.022]), 0.019, 0.023, k=0.006)]
    gob = sculpt('habe', gear, 0.003, lambda P: 0.0012 * sdf.fbm(P, 50.0, 2, 61), target=9000)
    gm = [mat('schuh_stoff', (0.42, 0.40, 0.37), 'fabric', r=1.45), mat('schuh_sohle', (0.62, 0.60, 0.55), 'rubber', r=1.2),
          mat('rucksack', (0.12, 0.18, 0.15), 'fabric', r=1.35), mat('flasche', (0.34, 0.38, 0.38), 'plastic', r=0.7),
          mat('lampe', (0.09, 0.09, 0.10), 'plastic', r=0.8), mat('rucksack_innen', (0.04, 0.04, 0.04), 'fabric', r=1.4)]

    def pick_gear(C, N, cav, edge):
        x, y, z = C[:, 0], C[:, 1], C[:, 2]
        out = np.full(len(C), 4)
        shoe = np.hypot(x + 0.78, y + 0.42) < 0.17
        out[shoe] = np.where(z[shoe] < 0.025, 1, 0)
        bag = np.hypot(x + 0.80, y - 0.55) < 0.34
        out[bag] = np.where(cav[bag] > 0.6, 5, 2)
        out[np.hypot(x - 0.35, y - 0.73) < 0.2] = 3
        return out
    paint_vec(gob, gear, None, gm, pick_gear, cav_scale=0.01)
    # --- Flecken aus klarem Blut, eingetrocknet ---------------------------------------------------------------------
    pools = _glaze_pool(rng, 0.55, -0.1, 0.3, 0.22, 7) + _glaze_pool(rng, 0.0, 0.0, 0.6, 0.45, 9) + _glaze_pool(rng, 0.7, 0.4, 0.14, 0.1, 3)
    pob = sculpt('blut', pools, 0.004, lambda P: 0.0006 * sdf.fbm(P, 40.0, 2, 71), target=3000)
    paint_vec(pob, [], None, [M['glaze'], M['glaze_edge']], lambda C, N, cav, edge: (_mottle(C, 131, 6.0) > 0.3).astype(np.int64))


# =====================================================================================================================
# food_scraps: Essensreste neben einem leckenden Fass (Futter der Death Rats)
# =====================================================================================================================
def _food_common(rng, k_pool, cx, cy, rx, ry):
    """Lache aus dem Fass (oelig-dunkel, glaenzend, mit Laufspur zur Wand) und Rattenkot."""
    pool_o = Outline([(cx, cy, cx + rng.uniform(-0.1, 0.1), cy + rng.uniform(-0.08, 0.08), rx, ry * 0.8)]
                     + [(cx, cy, cx - 0.25, cy + rng.uniform(-0.05, 0.05), 0.05, 0.03)], k=0.06)
    pool = sheet(pool_o, lambda u, v: 0.0015 + 0.0015 * np.clip(-pool_o.d(u, v) / 0.08, 0, 1), t0=0.0008, hmax=0.006, fade=0.04, ragged=0.012, rseed=k_pool)
    pob = sculpt('lache', [pool], 0.003, None, target=2500)
    pob.data.materials.append(mat('fass_lache', (0.07, 0.06, 0.045), 'water', r=0.25))
    pel = []
    for i in range(14):  # Rattenkot: kleine, dunkle, laengliche Koerner
        x, y = cx + rng.uniform(-0.35, 0.35), cy + rng.uniform(-0.3, 0.3)
        a = rng.uniform(0, math.tau)
        pel.append(Pell((x, y, 0.003), (0.006, 0.0028, 0.0028), k=0.0, rot=rotm(V(Z3), a)))
    kob = sculpt('kot', pel, 0.0009, None, target=1500)
    kob.data.materials.append(mat('rattenkot', (0.05, 0.04, 0.035), 'rubber', r=1.2))


def build_food_scraps():
    """v0: umgekippte Konservendose (Deckel aufgerissen) mit ausgelaufenem, eingetrocknetem Inhalt, eine zerrissene
    Verpackung (Folie) mit Kruemeln, eine Lache aus dem Fass, Rattenkot. Ursprung am Boden, das Fass steht bei -x."""
    rng = random.Random(1050)
    _food_common(rng, 51, -0.05, 0.0, 0.26, 0.20)
    can = []
    c0 = np.array([0.12, 0.10, 0.042])
    d = np.array([math.cos(0.5), math.sin(0.5), 0.0])
    can.append(Pcone(c0 - d * 0.055, c0 + d * 0.055, 0.042, 0.042, k=0.0))
    for t in (-0.045, -0.015, 0.015, 0.045):  # Sicken
        can.append(Pcone(c0 + d * (t - 0.003), c0 + d * (t + 0.003), 0.0435, 0.0435, k=0.002))
    can.append(Pcone(c0 + d * 0.05, c0 + d * 0.06, 0.038, 0.038, k=0.0, op='sub'))  # offen
    cob = sculpt('dose', can, 0.0015, lambda P: 0.0002 * sdf.fbm(P, 120.0, 2, 52), target=4000)
    paint_vec(cob, can, None, [mat('dose_blech', (0.55, 0.55, 0.53), 'metal', r=0.9), mat('dose_rost', (0.36, 0.22, 0.13), 'metal', tex='metal_rusty_iron', r=1.4),
                               mat('dose_etikett', (0.38, 0.20, 0.13), 'paint', r=1.3)],  # Runde 5: ausgeblichen (vorher grell)
              lambda C, N, cav, edge: np.where(cav > 0.5, 1, np.where(np.abs((C - c0) @ d) < 0.035, 2, 0)), cav_scale=0.004)
    # ausgelaufener Inhalt (Bohnen in Sosse, eingetrocknet) und Nagespuren
    goo = [Pell((0.2 + 0.03 * i * math.cos(0.5) + rng.uniform(-0.02, 0.02), 0.15 + 0.03 * i * math.sin(0.5) + rng.uniform(-0.02, 0.02), 0.004),
                (rng.uniform(0.02, 0.04), rng.uniform(0.015, 0.03), 0.004), k=0.02) for i in range(5)]
    goo += [Pell((0.2 + rng.uniform(-0.03, 0.12), 0.15 + rng.uniform(-0.04, 0.08), 0.008), (0.006, 0.004, 0.0035), k=0.003) for _ in range(16)]
    gob = sculpt('inhalt', goo, 0.0012, None, target=3000)
    paint_vec(gob, goo, None, [mat('bohnen_sosse', (0.20, 0.09, 0.05), 'plastic', r=0.7), mat('bohnen', (0.30, 0.18, 0.09), 'plastic', r=0.8)],
              lambda C, N, cav, edge: np.where(C[:, 2] > 0.0065, 1, 0))
    # zerrissene Folienverpackung mit Kruemeln
    wo = Outline([(-0.18, -0.20, -0.04, -0.27, 0.07, 0.06)], k=0.02)
    wrap = sheet(wo, lambda u, v: 0.004 + 0.012 * _r2(u, v, 18.0, 53, warp=0.02) ** 2 + 0.006 * np.clip(_n2(u, v, 14.0, 54) + 0.3, 0, 1), t0=0.0012,
                 hmax=0.025, fade=0.01, ragged=0.008, rseed=55)
    crumbs = [Pell((rng.uniform(-0.25, 0.05), rng.uniform(-0.35, -0.12), 0.003), (0.004, 0.003, 0.0025), k=0.0) for _ in range(20)]
    wob = sculpt('folie', [wrap] + crumbs, 0.0014, None, target=4000)
    paint_vec(wob, [wrap] + crumbs, None, [mat('folie_silber', (0.62, 0.62, 0.60), 'metal', r=0.5), mat('folie_druck', (0.12, 0.30, 0.55), 'plastic', r=0.6),
                                           mat('kruemel', (0.62, 0.48, 0.30), 'concrete', r=1.4)],
              lambda C, N, cav, edge: np.where(C[:, 2] < 0.0045, 2, np.where(_mottle(C, 56, 20.0) > 0.1, 1, 0)))


def build_food_scraps__v1():
    """v1: angenagter Brotkanten (hart, schimmelfleckig), zerbrochene Kekse, zerrissenes Papier, Lache, Rattenkot."""
    rng = random.Random(1051)
    _food_common(rng, 61, -0.08, 0.02, 0.22, 0.17)
    bread = [Pell((0.12, -0.05, 0.03), (0.09, 0.05, 0.032), k=0.0, rot=rotm(V(Z3), 0.4)),
             Pell((0.16, -0.03, 0.045), (0.06, 0.04, 0.022), k=0.02, rot=rotm(V(Z3), 0.4))]
    for i in range(7):  # Nagekerben (Ratten)
        bread.append(Pell((0.05 + rng.uniform(-0.01, 0.02), -0.09 + i * 0.012, 0.03 + rng.uniform(-0.01, 0.015)), (0.012, 0.004, 0.01), k=0.002, op='sub'))
    bob = sculpt('brot', bread, 0.0015, lambda P: 0.0008 * sdf.fbm(P, 90.0, 3, 62), target=4000)
    paint_vec(bob, bread, None, [mat('brot_kruste', (0.34, 0.23, 0.12), 'concrete', r=1.4), mat('brot_innen', (0.55, 0.49, 0.38), 'concrete', r=1.5),
                                 mat('schimmel', (0.38, 0.42, 0.34), 'fabric', r=1.5)],
              lambda C, N, cav, edge: np.where(cav > 0.5, 1, np.where(_mottle(C, 63, 30.0) > 0.35, 2, 0)), cav_scale=0.004)
    bisc = []
    for i in range(5):  # Kekse, zerbrochen
        x, y = rng.uniform(-0.2, 0.25), rng.uniform(-0.25, 0.25)
        bisc.append(Pell((x, y, 0.004), (rng.uniform(0.015, 0.025), rng.uniform(0.012, 0.022), 0.004), k=0.0, rot=rotm(V(Z3), rng.uniform(0, 3))))
    kob = sculpt('kekse', bisc, 0.0012, None, target=1500)
    kob.data.materials.append(mat('keks', (0.66, 0.50, 0.30), 'concrete', r=1.3))
    po = Outline([(0.0, 0.18, 0.12, 0.24, 0.07, 0.06)], k=0.02)
    pap = sheet(po, lambda u, v: 0.003 + 0.012 * _r2(u, v, 14.0, 64, warp=0.02) ** 2, t0=0.001, hmax=0.02, fade=0.01, ragged=0.01, rseed=65)
    pob = sculpt('papier', [pap], 0.0014, None, target=2500)
    pob.data.materials.append(mat('papier_fettig', (0.44, 0.41, 0.34), 'cardboard', r=1.3))
