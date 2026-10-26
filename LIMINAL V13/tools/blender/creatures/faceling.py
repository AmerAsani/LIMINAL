"""V13.2: Faceling (Entity 9, Wikidot Rev. 39) - KREATUR_Faceling.md. Ein Mensch, dem das Gesicht fehlt: Haar ja, Augen,
Nase, Mund und Ohren nicht; die Stirn geht glatt in Wangen und Kinn ueber. Etwas schmaler als ein Mensch (die Seite:
koerperlich schwaecher). Gewoehnliche Kleidung (graues Hemd, dunkle Hose, Schuhe) - sie ahmen menschliches Leben nach.

Varianten (gleiches Skelett ent_faceling): faceling_evo (Level 3), faceling_dress (Level 188).
"""
import numpy as np

import human as Hm
import sdf

LODS = (20000, 7000, 2400)
SKELETON = 'ent_faceling'


def cloth_disp(seed):
    def f(Q):
        d = -0.0035 * sdf.fbm(Q * np.array([3.0, 3.0, 1.2]), 1.0, 3, seed=seed)       # weite Falten (laengs)
        d -= 0.0009 * sdf.fbm(Q, 40, 2, seed=seed + 1)                                  # Gewebe
        return d
    return f


def skin_disp(Q):
    return -0.0006 * sdf.fbm(Q, 60, 2, seed=901)


def hair_disp(Q):
    return -0.004 * (sdf.ridged(Q * np.array([1.0, 1.0, 0.35]), 70, 2, seed=902) - 0.5) - 0.002 * sdf.fbm(Q, 20, 2, seed=903)


def build_parts(J, slim=1.0, muscle=0.9, shirt=True, sleeves=0.95, hair='short', head_extra=None, rng=None):
    rng = rng or np.random.default_rng(9)
    B = Hm.Body(J, slim=slim, muscle=muscle)
    skin = B.head() + B.neck() + B.hand(0) + B.hand(1)
    if head_extra:
        skin += head_extra(B)
    # Handgelenk unter dem Aermel (kein Spalt)
    for s in (0, 1):
        skin += B.arm(s, upper=False, fore=True, hand=False, sleeve_to=1.0)[-2:] if sleeves < 1.0 else []
    groups = []
    if shirt:
        sh = B.torso(t=0.012, upper=True, lower=True)[:-3] + B.arm(0, t=0.012, hand=False, sleeve_to=sleeves) + B.arm(1, t=0.012, hand=False, sleeve_to=sleeves)
        sh += B.neck(t=0.008)[:0]
        groups.append({'name': 'hemd', 'fm': 'ent_cloth_a', 'parts': sh, 'disp': cloth_disp(910), 'h': 0.0045, 'cav_scale': 0.012})
        pants = B.torso(t=0.014, upper=False, lower=True)[1:] + B.leg(0, t=0.013) + B.leg(1, t=0.013)
        groups.append({'name': 'hose', 'fm': 'ent_cloth_b', 'parts': pants, 'disp': cloth_disp(920), 'h': 0.0045, 'cav_scale': 0.012})
        shoes = B.foot(0, t=0.012) + B.foot(1, t=0.012)
        groups.append({'name': 'schuhe', 'fm': 'ent_cloth_b', 'parts': shoes, 'disp': None, 'h': 0.0035, 'cav_scale': 0.006})
    groups.insert(0, {'name': 'haut', 'fm': 'ent_humanskin', 'parts': skin, 'disp': skin_disp, 'h': 0.0035, 'cav_scale': 0.01})
    if hair == 'short':
        groups.append({'name': 'haar', 'fm': 'ent_hair', 'parts': B.hair_short(), 'disp': hair_disp, 'h': 0.0035, 'cav_scale': 0.006})
    elif hair == 'long':
        groups.append({'name': 'haar', 'fm': 'ent_hair', 'parts': B.hair_long(rng), 'disp': hair_disp, 'h': 0.004, 'cav_scale': 0.006})
    return B, groups


def build(J, bones):
    _, groups = build_parts(J)
    return groups
