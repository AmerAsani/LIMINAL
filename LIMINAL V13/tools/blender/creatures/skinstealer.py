"""V13.2: Skin-Stealer (Entity 10, Wikidot Rev. 61) - KREATUR_SkinStealer.md. Wahre Gestalt: gross (2,05 m - die Tueren der
Ebenen sind 2,2 m hoch, E-031), menschenaehnlich und sehr kraeftig (die Seite: reisst Menschen mit blossen Haenden auseinander), blassgelbe Haut,
ueber und ueber mit winzigen Noppen wie Saugnaepfe eines Kraken, eingesunkene weisse Augen. Mund und Glieder beschreibt
die Seite nicht naeher: hier ein schmaler Mund ohne Lippen, keine Ohren, grosse Haende.

Variante skinstealer_skin: getarnt - er traegt die Haut eines Menschen, die er passend gezogen und geheilt hat; er sieht
dann aus wie ein Mensch (die Seite: nicht zu unterscheiden).
"""
import math

import numpy as np

import anat as A
import human as Hm
import sdf

LODS = (24000, 8000, 2600)


def pale_disp(Q):
    """Noppen wie Saugnaepfchen (kleine Mulden mit Rand, dicht), dazu feine Falten an den Gelenken."""
    d1, d2 = sdf.worley(Q, 160, seed=1001)
    cup = np.clip(1.0 - d1 / 0.32, 0, 1)
    rim = np.clip(1.0 - np.abs(d1 - 0.42) / 0.12, 0, 1)
    return -0.0011 * cup * cup + 0.00045 * rim - 0.0007 * sdf.fbm(Q, 22, 2, seed=1002)


def rot_disp(Q):  # faulige Stellen: aufgequollen, rissig
    return -0.0008 * sdf.fbm(Q, 90, 2, seed=1003) - 0.0005 * np.clip(sdf.ridged(Q, 60, 2, seed=1004) - 0.6, 0, 1)


def build(J, bones):
    B = Hm.Body(J, slim=1.12, muscle=1.35, head=0.98, neck=1.2, hand=1.4, chest=1.18, hips=1.0)
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    c, r = B.headC, B.headR
    pale = B.head(featureless=True) + B.neck() + B.torso() + B.leg(0) + B.leg(1) + B.foot(0, bare=True) + B.foot(1, bare=True)
    for s in (0, 1):
        pale += B.arm(s, hand=False)
        pale += B.hand(s, curl=0.3, claw=0.25)
    # Kraft: Kapuzenmuskel, breiter Ruecken, Unterarme
    for s in (0, 1):
        g = Hm.side(s)
        pale.append(A.ell(Hm.CHEST, (-0.03, g * 0.1, B.shZ + 0.02), (0.1, g * 1.0, 0.4), (0.11, 0.05, 0.045), up=X, k=0.05))
        pale.append(A.ell(Hm.CHEST, (-0.07, g * 0.12, B.chestZ - 0.02), (0.1, g * 0.3, 1.0), (0.16, 0.06, 0.04), up=-X, k=0.05))
        E, W = B.j(Hm.jEL, s), B.j(Hm.jWR, s)
        pale.append(A.ell(Hm.FORE + 4 * s, A.lerp(E, W, 0.3) + X * 0.012, A.unit(W - E), (0.1, 0.042, 0.038), up=X, k=0.03))
    # eingesunkene Augen: tiefe Hoehlen unter einem schweren Brauenbogen, die Augaepfel weit hinten
    eyes = []
    for s in (0, 1):
        g = Hm.side(s)
        e = c + X * r[0] * 0.78 + Y * g * r[1] * 0.40 + Z * r[2] * 0.02
        pale.append(A.ell(Hm.HEAD, e + X * 0.022, X, (0.036, 0.025, 0.02), op='sub', k=0.014))
        pale.append(A.ell(Hm.HEAD, e + X * 0.02 + Z * 0.03, X, (0.016, 0.028, 0.01), k=0.014))
        eyes.append(A.ell(Hm.HEAD, e - X * 0.012, X, (0.0125, 0.0125, 0.0105), k=0.0))
    # flache Nase (nur ein Grat), schmaler Mund ohne Lippen
    nose = c + X * r[0] * 0.96 - Z * r[2] * 0.2
    pale.append(A.ell(Hm.HEAD, nose, X - Z * 0.6, (0.012, 0.01, 0.03), k=0.016))
    mouthC = c + X * r[0] * 0.9 - Z * r[2] * 0.47
    pale.append(A.ell(Hm.HEAD, mouthC + X * 0.01, X, (0.012, 0.033, 0.0025), op='sub', k=0.004))
    mouth = [A.ell(Hm.JAW, mouthC - X * 0.004, X, (0.008, 0.028, 0.006), k=0.0)]
    # V13.3 Wow-Check (KREATUR_SkinStealer.md, Abschnitt "Wow-Check Level 3"): aus der Naehe glatt wie eine Puppe. Jetzt:
    # eingesunkene, faulige Stellen (nass, olivbraun), hervortretende Adern an Hals, Schlaefen und Unterarmen, Schleim in den
    # Mundwinkeln, am Kinn und in den Augenhoehlen
    rng = np.random.default_rng(1010)
    rot, slime = [], []
    q = B.sz
    rx, ry, rz = 0.105 * 1.12 * 1.18, 0.150 * 1.12, 0.175 * q

    def chest_pt(th, z, out=0.0):
        f = math.sqrt(max(0.05, 1.0 - ((z - B.chestZ) / rz) ** 2))
        return np.array([(rx + out) * f * math.cos(th), (ry + out) * f * math.sin(th), z])

    lesions = [(Hm.CHEST, chest_pt(th, z), A.unit(chest_pt(th, z) - np.array([0, 0, z]))) for th, z in
               ((0.5, B.chestZ + 0.05), (-0.9, B.chestZ - 0.06), (2.6, B.chestZ + 0.02), (-2.3, B.chestZ - 0.1), (1.4, B.chestZ - 0.12))]
    for s_ in (0, 1):
        g = Hm.side(s_)
        S, E, W = B.j(Hm.jSH, s_), B.j(Hm.jEL, s_), B.j(Hm.jWR, s_)
        du = A.unit(E - S)
        nrm = A.unit(np.cross(du, Y * g) + X * 0.3)
        lesions.append((Hm.UPPER + 4 * s_, A.lerp(S, E, 0.55) + nrm * 0.048, nrm))
        Hh, K = B.j(Hm.jHIP, s_), B.j(Hm.jKNEE, s_)
        lesions.append((Hm.THIGH + 3 * s_, A.lerp(Hh, K, 0.4) + X * 0.085 + Y * g * 0.03, A.unit(X + Y * g * 0.4)))
    lesions.append((Hm.HEAD, c + X * r[0] * 0.62 - Y * r[1] * 0.62 - Z * r[2] * 0.25, A.unit(X * 0.5 - Y)))  # Wange
    for bone, p, nrm in lesions:
        rr = (0.006, 0.022 + 0.014 * rng.random(), 0.018 + 0.012 * rng.random())
        pale.append(A.ell(bone, p + nrm * 0.002, nrm, rr, op='sub', k=0.006))
        rot.append(A.ell(bone, p - nrm * 0.0035, nrm, (0.003, rr[1] * 0.92, rr[2] * 0.92), k=0.0))
        for k in range(2 if rng.random() < 0.6 else 1):  # Schleim laeuft aus
            a0 = p + nrm * 0.001 - Z * rr[2] * 0.6 + Y * (rng.random() - 0.5) * 0.02
            ln = 0.02 + 0.05 * rng.random()
            slime += A.chain(lambda i, b=bone: b, [a0, a0 - Z * ln * 0.5 + nrm * 0.001, a0 - Z * ln], [0.0028, 0.0024, 0.0016], k=0.002)
            slime.append(A.ell(bone, a0 - Z * ln, Z, (0.003, 0.003, 0.0042)))
    # Adern: Hals, Schlaefen, Unterarme (erhaben, verzweigt)
    nr = 0.052 * 1.2 * 1.12 + 0.002
    for s_ in (0, 1):
        g = Hm.side(s_)
        for k in range(2):
            th = g * (0.9 + 0.5 * k)
            pts = [np.array([nr * math.cos(th + 0.08 * math.sin(z * 60)), nr * math.sin(th + 0.08 * math.sin(z * 60)), z]) for z in np.linspace(B.shZ - 0.0, B.neckTop + 0.0, 6)]
            pale += A.chain(lambda i: Hm.NECK, pts, [0.0045, 0.0042, 0.004, 0.0038, 0.0035, 0.003], k=0.003)
        tp = c + X * r[0] * 0.55 + Y * g * r[1] * 0.85 + Z * r[2] * 0.25
        pale += A.chain(lambda i: Hm.HEAD, [tp - Z * 0.04 - X * 0.01, tp, tp + Z * 0.03 - X * 0.025, tp + Z * 0.05 - X * 0.05], [0.0035, 0.0035, 0.003, 0.0022], k=0.002)
        E, W = B.j(Hm.jEL, s_), B.j(Hm.jWR, s_)
        dw = A.unit(W - E)
        for k in range(2):
            off = A.unit(X * (0.8 - 1.6 * k) + np.cross(dw, X) * 0.3)
            pts = [A.lerp(E, W, t) + off * (0.042 - 0.012 * t) + np.cross(dw, off) * 0.008 * math.sin(t * 9 + k) for t in np.linspace(0.1, 0.9, 6)]
            pale += A.chain(lambda i: Hm.FORE + 4 * s_, pts, [0.0042, 0.004, 0.0038, 0.0034, 0.003, 0.0026], k=0.003)
    # Schleim: Faeden in den Mundwinkeln, Tropfen am Kinn, nasser Film in den Augenhoehlen
    for s_ in (0, 1):
        g = Hm.side(s_)
        mc = mouthC + Y * g * 0.03 + X * 0.012
        slime += A.chain(lambda i: Hm.JAW, [mc, mc - Z * 0.02 + X * 0.004, mc - Z * 0.045 + X * 0.002], [0.0032, 0.0026, 0.0018], k=0.002)
        e = c + X * r[0] * 0.78 + Y * g * r[1] * 0.40 + Z * r[2] * 0.02
        slime.append(A.ell(Hm.HEAD, e + X * 0.004, X, (0.012, 0.022, 0.017), k=0.0))
    ch = c + X * r[0] * 0.75 - Z * r[2] * 0.95
    slime += A.chain(lambda i: Hm.JAW, [ch, ch - Z * 0.03, ch - Z * 0.06], [0.004, 0.003, 0.002], k=0.002)
    slime.append(A.ell(Hm.JAW, ch - Z * 0.062, Z, (0.0035, 0.0035, 0.005)))
    return [
        {'name': 'blass', 'fm': 'ent_paleskin', 'parts': pale, 'disp': pale_disp, 'h': 0.0042, 'cav_scale': 0.012},
        {'name': 'augen', 'fm': 'ent_eyeshine', 'parts': eyes, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003, 'lod_weight': 2.0},
        {'name': 'mund', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.002, 'cav_scale': 0.003},
        {'name': 'faeulnis', 'fm': 'ent_ss_rot', 'parts': rot, 'disp': rot_disp, 'h': 0.0018, 'cav_scale': 0.004},
        {'name': 'schleim', 'fm': 'ent_wr_slime', 'parts': slime, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002},
    ]
