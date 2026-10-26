"""V13.2: Skin-Stealer (Entity 10, Wikidot Rev. 61) - KREATUR_SkinStealer.md. Wahre Gestalt: gross (2,05 m - die Tueren der
Ebenen sind 2,2 m hoch, E-031), menschenaehnlich und sehr kraeftig (die Seite: reisst Menschen mit blossen Haenden auseinander), blassgelbe Haut,
ueber und ueber mit winzigen Noppen wie Saugnaepfe eines Kraken, eingesunkene weisse Augen. Mund und Glieder beschreibt
die Seite nicht naeher: hier ein schmaler Mund ohne Lippen, keine Ohren, grosse Haende.

Variante skinstealer_skin: getarnt - er traegt die Haut eines Menschen, die er passend gezogen und geheilt hat; er sieht
dann aus wie ein Mensch (die Seite: nicht zu unterscheiden).
"""
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
    return [
        {'name': 'blass', 'fm': 'ent_paleskin', 'parts': pale, 'disp': pale_disp, 'h': 0.0042, 'cav_scale': 0.012},
        {'name': 'augen', 'fm': 'ent_eyeshine', 'parts': eyes, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003, 'lod_weight': 2.0},
        {'name': 'mund', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.002, 'cav_scale': 0.003},
    ]
