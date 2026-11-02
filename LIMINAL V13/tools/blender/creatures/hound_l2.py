"""V13.3 Level 2 (KREATUR_Hound.md, Abschnitt "Rudel"): Hound eines Rudels in Level 2 ("Pipe Dreams").

Wikidot Level 2 (Rev. 93): Hounds ziehen dort in Rudeln langsam umher und werden gestoert schlagartig zu schnellen Jaegern;
die Ebene ist heiss, eng, voller Rohre, aus denen eine schwarze Fluessigkeit leckt. Aussehen wie Entity 8 (Rev. 56) - die
Zeichen sind Auslegung aus der Ebene: die Glieder bis ueber die Handgelenke in der schwarzen Fluessigkeit gebadet (glaenzend,
abtropfend), das Haar davon verklebt und nass glaenzend, offene, nasse Stellen auf den Flanken (Hitze, Reibung an den
Rohren), noch magerer (tiefere Rippen). Grundform, Knochen und Gewichte wie hound.py.
"""
import numpy as np

import hound
import hound_marks as M
import sdf

LODS = (56000, 16000, 4500)
SKELETON = 'ent_hound'


def build(J, bones):
    groups = hound.build(J, bones)
    skin = M.part_of(groups, 'haut')
    base = skin['disp']
    rib_c = np.array([0.125, 0, 0.752])

    def gaunt(Q):  # Rippen und Flanken noch tiefer eingefallen
        d = base(Q)
        rq = (Q - rib_c) / np.array([0.205, 0.124, 0.104])
        on = np.clip(1.25 - np.linalg.norm(rq, axis=1), 0, 1) * np.clip((np.abs(Q[:, 1]) - 0.045) / 0.04, 0, 1)
        on *= np.clip((0.83 - Q[:, 2]) / 0.03, 0, 1) * np.clip((Q[:, 0] + 0.07) / 0.05, 0, 1) * np.clip((0.30 - Q[:, 0]) / 0.04, 0, 1)
        ph = (Q[:, 0] + 0.45 * (0.83 - Q[:, 2])) / 0.027 * 2 * np.pi
        d += 0.0016 * np.maximum(-np.cos(ph), 0) * on  # Zwischenrippenraeume sinken ein
        return d

    skin['disp'] = gaunt
    raw, rims = M.sores(skin, 2207, 7)
    skin['parts'] = skin['parts'] + rims
    groups += M.overlay(J, skin, 'ent_hd_oil', 'fluessigkeit', 2201, lift=0.0009, drips=5)
    groups.append({'name': 'wunden', 'fm': 'ent_cl_raw', 'parts': raw, 'disp': lambda Q: -0.0004 * sdf.fbm(Q, 160, 2, seed=97),
                   'h': 0.0011, 'cav_scale': 0.004})
    hair = M.part_of(groups, 'haar')
    hair['fm'] = 'ent_hd_oil'  # verklebt, nass glaenzend
    return groups
