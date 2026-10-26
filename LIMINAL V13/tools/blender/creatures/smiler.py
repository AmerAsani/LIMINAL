"""V13: Smiler (Entity 3) - KREATUR_Smiler.md. Kein Comic-Gesicht: ein breiter, dunkler Schaedel mit Brauenwulst und
Jochbeinen, zwei ungleiche, feuchte Augaepfel tief in den Hoehlen (leuchtend weiss), ein Maul mit zurueckgezogenen
Lippen und dunklem Zahnfleisch, darin zwei Reihen ungleicher, spitzer, teils abgebrochener Zaehne entlang einer
schiefen Linie (links hoeher). Darunter die dunkle Masse (Rauch) mit der Andeutung langer, falsch geknickter Arme
(das unbestaetigte Geruecht der Wikidot-Seite), oben und hinten zuengelnde Zungen.

Knochen (ent_smiler): 1 unten, 2 Mitte, 3 Kopf, 4 Gesicht, 5 Kiefer, 6-9 Lider (oben/unten je Auge),
10-27 sechs Zungen zu je drei Knochen. Augen und Lider nach sm::smilerEye (anim/creature.cpp): das Lid-Scharnier liegt
3 cm hinter der Augenflaeche - die Augen sitzen deshalb an derselben Stelle wie bisher.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (42000, 13000, 3800)

HEAD_C = np.array([-0.02, 0.0, 1.84])
HEAD_R = np.array([0.218, 0.37, 0.28])  # Kopfmasse von V11 (nur fuer die Lage der Augen und Lider)


def eye(side):
    """Wie sm::smilerEye: Mitte auf der Haut, Normale, Achse t (laengs), u, Scharnier, halbe Laenge, halbe Hoehe."""
    y, z = (0.118, 1.915) if side == 0 else (-0.112, 1.915)
    roll = 0.12 if side == 0 else -0.17
    dy, dz = y / HEAD_R[1], (z - HEAD_C[2]) / HEAD_R[2]
    x = HEAD_C[0] + HEAD_R[0] * math.sqrt(max(0.05, 1 - dy * dy - dz * dz))
    surf = np.array([x, y, z])
    n = A.unit([(x - HEAD_C[0]) / HEAD_R[0] ** 2, y / HEAD_R[1] ** 2, (z - HEAD_C[2]) / HEAD_R[2] ** 2])
    t = A.unit(np.cross([0, 0, 1], n))
    u = np.cross(n, t)
    tt = t * math.cos(roll) + u * math.sin(roll)
    uu = u * math.cos(roll) - t * math.sin(roll)
    hinge = surf - n * 0.03
    return surf, n, tt, uu, hinge, (0.052 if side == 0 else 0.047), (0.0088 if side == 0 else 0.0078)


def smile_line(t):
    """Mundlinie, t von -1 (Seite 1, y < 0) bis +1 (Seite 0): Punkt auf der Gesichtsflaeche (Lippenspalt)."""
    y = 0.255 * t if t > 0 else 0.232 * t
    # schief: links (Seite 0) steigt der Winkel hoeher, die Mitte haengt leicht
    z = 1.768 + 0.040 * abs(t) ** 1.6 + (0.034 * t ** 3 if t > 0 else 0.012 * t)  # links deutlich hoeher, rechts tiefer
    # auf einer Rundung: vorn weit vor, zu den Winkeln zurueck
    x = 0.205 - 0.17 * abs(t) ** 1.9
    return np.array([x, y, z])


def build(J, bones):
    rng = np.random.default_rng(3)
    body, head, mouth, eyes, lids, lips = [], [], [], [], [], []
    teeth_hi, teeth_lo = [], []
    # --- Masse (Rauch) mit Armen -------------------------------------------------------------------------
    body += [
        A.cone(1, (0, 0, 0.30), (0, 0, 0.98), 0.10, 0.20, k=0.0),
        A.cone(2, (0, 0, 0.98), (0.02, 0, 1.50), 0.20, 0.215, k=0.06),
        A.ell(2, (0.0, 0, 1.43), (0, 1, 0), (0.29, 0.13, 0.12), up=(0, 0, 1), k=0.10),       # Schulterwulst
        A.cone(3, (0.02, 0, 1.50), (-0.01, 0, 1.70), 0.17, 0.14, k=0.06),                      # Hals
    ]
    # V13 Runde 2: keine Arme - im Licht lasen sie sich als Strichmaennchen-Arme; der Koerper bleibt unbekannt (Wikidot)
    # Zungen (Rauch), aus dem Hinterkopf aufsteigend
    for w in range(6):
        pts = [np.asarray(J[12 + 4 * w + i]) for i in range(4)]
        body += A.chain(lambda k, w=w: 10 + 3 * w + k, pts, [0.040, 0.028, 0.016, 0.004], k=0.03)  # Runde 5: duenner
    # --- Kopf (dunkle Haut) ----------------------------------------------------------------------------------
    head += [
        A.ell(3, (0.0, 0, 1.865), (1, 0, 0), (0.200, 0.270, 0.235), k=0.0),                    # Schaedel, breit
        A.ell(4, (0.150, 0, 1.950), (0.4, 0, -1), (0.030, 0.190, 0.026), up=(1, 0, 0.3), k=0.03),  # Brauenwulst
        A.ell(4, (0.105, 0, 1.790), (1, 0, -0.2), (0.120, 0.250, 0.075), k=0.05),               # Oberkiefer, breit
        A.ell(5, (0.050, 0, 1.712), (1, 0, -0.35), (0.140, 0.225, 0.060), k=0.04),              # Unterkiefer
        A.ell(5, (0.165, 0, 1.690), (1, 0, -0.5), (0.050, 0.100, 0.030), k=0.03),               # Kinn
    ]
    for s in (1, -1):
        head.append(A.ell(4, (0.125, 0.165 * s, 1.865), (0.6, 0.6 * s, -0.3), (0.06, 0.03, 0.022), k=0.03))  # Jochbein
    # Augenhoehlen, Augen, Lider
    for side in (0, 1):
        surf, n, t, u, hinge, L, r = eye(side)
        head.append(A.Part('ell', 4, k=0.012, op='sub', group='skin', c=surf + n * 0.004,
                           R=np.stack([t, u, n], axis=1), r=np.array([L * 1.25, 0.024, 0.020])))
        # Runde 5: Mandelform mit spitzen Winkeln (Schnitt zweier gegeneinander verschobener Scheiben) statt einer
        # ovalen Scheibe - die Ellipse las sich als Comic-Auge. Beide Teile auf den Kasten des Auges begrenzt, damit der
        # nur oertlich wirkende Schnitt ('int') nichts ausserhalb stehen laesst.
        hl, hh = L * 0.88, r * 1.15
        Rr = (hl * hl + hh * hh) / (2 * hh)
        Re = np.stack([t, u, n], axis=1)
        c0 = surf - n * 0.006
        ext = np.abs(Re) @ np.array([hl, hh, 0.012]) + 0.003
        for sgn, op in ((1, 'add'), (-1, 'int')):
            pe = A.Part('ell', 4, k=0.0, op=op, group='eye', c=c0 + u * sgn * (Rr - hh), R=Re, r=np.array([Rr, Rr, 0.012]))
            pe.lo, pe.hi = c0 - ext, c0 + ext
            eyes.append(pe)
        for up in (0, 1):
            th = (1 if up == 0 else -1) * (0.2 + 0.95)
            Rl = 0.03 + 0.0105
            bone = (6 if up == 0 else 7) if side == 0 else (8 if up == 0 else 9)
            for kk in (-1, 0, 1):
                a = th + kk * 0.2
                rr = n * math.cos(a) + u * math.sin(a)
                lids.append(A.Part('ell', bone, k=0.002, c=hinge + rr * Rl, R=np.stack([rr, t, np.cross(rr, t)], axis=1),
                                   r=np.array([0.0024, L * (1.1 if kk == 0 else 0.95), 0.0088])))
    # Maul: Spalt entlang der schiefen Linie, Lippenwuelste darueber und darunter, Zahnfleisch innen
    # Runde 5: durchgehende Wuelste (tube-Ketten mit gemeinsamen Enden) statt einzelner Ellipsoide - die Kette las sich
    # im Licht als Perlenschnur, und der Spalt hatte Zacken zwischen den Stuecken
    ts = np.linspace(-0.98, 0.98, 41)
    open_of = lambda t: 0.020 * (1 - 0.55 * abs(t)) + 0.005
    taper = lambda t: 1.0 - 0.45 * abs(t) ** 3  # Lippen und Zahnfleisch laufen zu den Winkeln aus
    X = np.array([1.0, 0, 0])
    Z = np.array([0, 0, 1.0])
    for i in range(len(ts) - 1):
        ta, tb = ts[i], ts[i + 1]
        pa, pb = smile_line(ta), smile_line(tb)
        oa, ob = open_of(ta), open_of(tb)
        wa, wb = taper(ta), taper(tb)
        head.append(A.tube(4, pa + X * 0.01, pb + X * 0.01, (0.05, oa), (0.05, ob), op='sub', k=0.004))
        lips.append(A.tube(4, pa + Z * (oa + 0.005) - X * 0.002, pb + Z * (ob + 0.005) - X * 0.002,
                           (0.011 * wa, 0.0065 * wa), (0.011 * wb, 0.0065 * wb), k=0.004))  # Oberlippe
        lips.append(A.tube(5, pa - Z * (oa + 0.006) - X * 0.004, pb - Z * (ob + 0.006) - X * 0.004,
                           (0.011 * wa, 0.0065 * wa), (0.011 * wb, 0.0065 * wb), k=0.004))  # Unterlippe
        mouth.append(A.tube(4, pa - X * 0.022 + Z * 0.004, pb - X * 0.022 + Z * 0.004,
                            (0.020 * wa, 0.009 * wa), (0.020 * wb, 0.009 * wb), k=0.004))
        mouth.append(A.tube(5, pa - X * 0.024 - Z * 0.010, pb - X * 0.024 - Z * 0.010,
                            (0.020 * wa, 0.008 * wa), (0.020 * wb, 0.008 * wb), k=0.004))
    # Zaehne: zwei Reihen oben und unten (die hintere kleiner), ungleich lang, schief, einzelne Nadeln, abgebrochene
    for row, (n_t, sg, tlist) in enumerate(((46, 1.0, teeth_hi), (40, -1.0, teeth_lo))):
        for back in (0, 1):
            cnt = n_t if back == 0 else n_t // 2
            for i in range(cnt):
                if rng.random() < 0.13:  # Luecke
                    continue
                t = -0.95 + 1.9 * (i + 0.5 + 0.45 * (rng.random() - 0.5)) / cnt
                base = smile_line(t) + np.array([-0.012 - 0.016 * back, 0, sg * (0.010 + 0.003 * rng.random())])
                big = 1.0 - 0.45 * abs(t)
                L = (0.026 + 0.030 * rng.random() ** 2) * big * (0.7 if back else 1.0)
                if rng.random() < 0.13:
                    L = (0.050 + 0.025 * rng.random()) * big  # lange Nadel
                if rng.random() < 0.14:
                    L *= 0.4  # abgebrochen
                d = A.unit(np.array([0.22 + 0.25 * rng.random(), -0.10 * t + 0.30 * (rng.random() - 0.5), -sg]))
                r0 = (0.0042 + 0.0022 * rng.random()) * (1 - 0.3 * abs(t))
                tlist.append(A.cone(4 if sg > 0 else 5, base, base + d * L, r0, 0.0004, group='tooth'))
    # --- Verschiebungen --------------------------------------------------------------------------------------
    def head_disp(Q):
        d = -0.0012 * sdf.fbm(Q, 40, 3, seed=5)
        # Runde 5: der Hinterkopf ist kein glattes Ei - grobe Rauchbeulen, das Gesicht (vorn) bleibt glatt
        back = np.clip((0.09 - Q[:, 0]) / 0.10, 0.0, 1.0)
        d -= 0.007 * back * sdf.fbm(Q, 11, 2, seed=8)
        # feine Hautfalten um Augen und Mundwinkel
        d -= 0.0007 * (sdf.ridged(Q, 90, 2, seed=6) - 0.6)
        return d

    def body_disp(Q):
        return -0.008 * sdf.fbm(Q * np.array([1, 1, 0.5]), 9, 3, seed=7)

    groups = [
        {'name': 'masse', 'fm': 'ent_shadow', 'parts': body, 'disp': body_disp, 'h': 0.0045, 'cav_scale': 0.02},
        {'name': 'kopf', 'fm': 'ent_shadow', 'parts': head, 'disp': head_disp, 'h': 0.0022, 'cav_scale': 0.012},
        {'name': 'lippen', 'fm': 'ent_darkskin', 'parts': lips, 'disp': None, 'h': 0.0012, 'cav_scale': 0.006},
        {'name': 'zahnfleisch', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.0015, 'cav_scale': 0.006},
        {'name': 'augen', 'fm': 'ent_glow_eye', 'parts': eyes, 'disp': None, 'h': 0.0009, 'cav_scale': 0.004},
        {'name': 'lider', 'fm': 'ent_darkskin', 'parts': lids, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003},
    ]
    # Zaehne in zwei Helligkeiten (leuchten ungleich)
    for tl, name in ((teeth_hi, 'oben'), (teeth_lo, 'unten')):
        bright = [p for i, p in enumerate(tl) if (i * 7919) % 10 < 6]
        dim = [p for i, p in enumerate(tl) if (i * 7919) % 10 >= 6]
        groups.append({'name': f'zaehne_{name}_hell', 'fm': 'ent_glow', 'parts': bright, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003})
        groups.append({'name': f'zaehne_{name}_matt', 'fm': 'ent_glow_dim', 'parts': dim, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003})
    return groups
