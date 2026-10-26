"""V13.2: Faceling im rosa Kleid (Level 188, Wikidot Rev. 69: in den Gaengen, toetete ein Mitglied einer Expedition mit
Hieben und Stichen). Ein gesichtsloser Mensch mit langem Haar im knielangen, kurzaermeligen rosa Kleid; Arme und
Unterschenkel frei, flache Schuhe. Kein Werkzeug sichtbar (die Seite nennt keins).
"""
import numpy as np

import anat as A
import faceling as F
import human as Hm
import sdf

LODS = (22000, 7600, 2600)
SKELETON = 'ent_faceling'
MODEL = 'ent_faceling_dress'


def build(J, bones):
    rng = np.random.default_rng(188)
    B, groups = F.build_parts(J, slim=0.9, muscle=0.75, shirt=False, hair='long', rng=rng)
    skin = groups[0]['parts']
    # frei: Arme ab dem kurzen Aermel, Unterschenkel unter dem Saum
    for s in (0, 1):
        skin += B.arm(s, upper=True, fore=True, hand=False, sleeve_to=1.0)
        skin += B.leg(s, thigh=False, shin=True)
        skin.append(A.ell(Hm.THIGH + 3 * s, B.j(Hm.jKNEE, s) + Hm.Z * 0.05, Hm.Z, (0.06, 0.055, 0.08), k=0.03))
    skin += B.torso(upper=True, lower=False)[:2]
    # Kleid: Oberteil eng, kurze Aermel, Rock ab der Taille ausgestellt bis knapp unters Knie
    dress = B.torso(t=0.01, upper=True, lower=True)[:-3]
    for s in (0, 1):
        S, E = B.j(Hm.jSH, s), B.j(Hm.jEL, s)
        dress.append(A.cone(Hm.UPPER + 4 * s, S, A.lerp(S, E, 0.42), 0.055, 0.047, k=0.03))
    kneeZ = B.j(Hm.jKNEE, 0)[2]
    waist = np.array([0.005, 0, B.waist - 0.02])
    hem = np.array([0.02, 0, kneeZ - 0.07])
    # Runde 1: runde Kegel haben Kugelkappen - der Rock reichte bis zum Boden. Jetzt unten gerade abgeschnitten.
    dress.append(A.cone(Hm.PELVIS, waist, hem, 0.135, 0.27, k=0.05))
    # Rock innen hohl (die Beine sind darin)
    dress.append(A.cone(Hm.PELVIS, waist - Hm.Z * 0.14, hem - Hm.Z * 0.3, 0.11, 0.3, op='sub', k=0.02))
    dress.append(A.box(Hm.PELVIS, np.array([0.0, 0.0, hem[2] - 0.4]), Hm.X, (0.7, 0.7, 0.4), 0.0, op='sub', k=0.0))

    def dress_disp(Q):
        # Falten vom Bund zum Saum (Rock), sonst feines Gewebe
        a = np.arctan2(Q[:, 1], Q[:, 0] - 0.02)
        low = np.clip((B.waist - Q[:, 2]) / 0.4, 0, 1)
        d = -0.012 * low * (0.5 + 0.5 * np.sin(a * 11 + 2.0 * sdf.fbm(Q, 3, 2, seed=950)))
        d -= 0.0008 * sdf.fbm(Q, 45, 2, seed=951)
        return d

    groups.insert(1, {'name': 'kleid', 'fm': 'ent_cloth_pink', 'parts': dress, 'disp': dress_disp, 'h': 0.0045, 'cav_scale': 0.012})
    groups.insert(2, {'name': 'schuhe', 'fm': 'ent_cloth_b', 'parts': B.foot(0, t=0.01) + B.foot(1, t=0.01), 'disp': None, 'h': 0.0035,
                      'cav_scale': 0.006})
    return groups
