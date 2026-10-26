"""V13.2: Faceling, dritte Grundvariante (Entity 9: verschieden an Haar, Haut, Kleidung, Koerperbau, Alter). Ein
aelterer, schwerer Mann: graues kurzes Haar, olivgruene Jacke mit langen Aermeln, dunkle Hose."""
import numpy as np

import faceling as F

LODS = (20000, 7000, 2400)
SKELETON = 'ent_faceling'


def build(J, bones):
    rng = np.random.default_rng(92)
    _, groups = F.build_parts(J, slim=1.12, muscle=0.95, sleeves=1.0, hair='short', rng=rng)
    for g in groups:
        g['fm'] = {'ent_cloth_a': 'ent_cloth_d', 'ent_hair': 'ent_hair_grey'}.get(g['fm'], g['fm'])
    return groups
