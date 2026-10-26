"""V13.2: getarnter Skin-Stealer (Entity 10). Er traegt die Haut eines Menschen - die Noppen halten sie fest, ziehen sie
passend und heilen Schnitte und Naehte; er sieht dann aus wie ein Mensch. Hier: ein sehr grosser, hagerer Mann in
abgetragener Kleidung (Hemd, Hose, Schuhe), kurzes Haar, ein gewoehnliches Gesicht. Was ihn verraet, ist das Verhalten
(spricht kaum, wiederholt Worte) - nicht das Aussehen. Gleiches Skelett wie die wahre Gestalt.
"""
import numpy as np

import anat as A
import faceling as F
import human as Hm
import sdf

LODS = (26000, 8600, 2800)
SKELETON = 'ent_skinstealer'


def skin_disp(Q):  # Poren und feine Falten (Menschenhaut)
    return -0.0005 * sdf.fbm(Q, 70, 2, seed=1011) - 0.0003 * sdf.fbm(Q, 160, 2, seed=1012)


def build(J, bones):
    B = Hm.Body(J, slim=1.08, muscle=1.2, head=0.98, neck=1.15, hand=1.25, chest=1.12, hips=1.0)
    X, Z = Hm.X, Hm.Z
    skin = B.head(featureless=False) + B.neck() + B.hand(0, curl=0.3) + B.hand(1, curl=0.3)
    fskin, eyes, brows, mouth = B.face()
    skin += fskin
    for s in (0, 1):  # Handgelenk unter dem Aermel
        skin += B.arm(s, upper=False, fore=True, hand=False, sleeve_to=1.0)[-2:]
    shirt = B.torso(t=0.013, upper=True, lower=True)[:-3] + B.arm(0, t=0.013, hand=False, sleeve_to=0.93) + \
        B.arm(1, t=0.013, hand=False, sleeve_to=0.93)
    # Kragen (offen) und Knopfleiste
    shirt.append(A.cone(Hm.CHEST, np.array([-0.02, 0, B.shZ + 0.02]), np.array([0.03, 0, B.shZ + 0.05]), 0.072, 0.066, k=0.02))
    shirt.append(A.ell(Hm.CHEST, (0.07, 0, B.shZ + 0.04), X, (0.05, 0.04, 0.06), op='sub', k=0.02))
    pants = B.torso(t=0.015, upper=False, lower=True)[1:] + B.leg(0, t=0.014) + B.leg(1, t=0.014)
    shoes = B.foot(0, t=0.013) + B.foot(1, t=0.013)
    eyes_g = [{'name': 'augen', 'fm': 'ent_tooth', 'parts': eyes, 'disp': None, 'h': 0.0012, 'cav_scale': 0.003, 'lod_weight': 2.0}]
    return [
        {'name': 'haut', 'fm': 'ent_humanskin', 'parts': skin, 'disp': skin_disp, 'h': 0.0032, 'cav_scale': 0.01, 'lod_weight': 1.3},
        {'name': 'hemd', 'fm': 'ent_cloth_a', 'parts': shirt, 'disp': F.cloth_disp(1020), 'h': 0.0048, 'cav_scale': 0.012},
        {'name': 'hose', 'fm': 'ent_cloth_b', 'parts': pants, 'disp': F.cloth_disp(1030), 'h': 0.0048, 'cav_scale': 0.012},
        {'name': 'schuhe', 'fm': 'ent_cloth_b', 'parts': shoes, 'disp': None, 'h': 0.0038, 'cav_scale': 0.006},
        {'name': 'haar', 'fm': 'ent_hair', 'parts': B.hair_short(t=0.009) + brows, 'disp': F.hair_disp, 'h': 0.0032, 'cav_scale': 0.006},
        {'name': 'mund', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.002, 'cav_scale': 0.003},
    ] + eyes_g
