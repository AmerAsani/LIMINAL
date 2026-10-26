"""V13.2: Faceling, zweite Grundvariante (Entity 9: Haar, Hautton, Kleidung, Koerperbau und scheinbares Alter sind bei
Facelings sehr verschieden). Eine Frau: dunklere Haut, langes Haar, Bluse in gedecktem Blau, Jeans."""
import numpy as np

import faceling as F

LODS = (20000, 7000, 2400)
SKELETON = 'ent_faceling'


def build(J, bones):
    rng = np.random.default_rng(91)
    _, groups = F.build_parts(J, slim=0.94, muscle=0.8, sleeves=0.75, hair='long', rng=rng)
    for g in groups:
        g['fm'] = {'ent_humanskin': 'ent_humanskin_b', 'ent_cloth_a': 'ent_cloth_c'}.get(g['fm'], g['fm'])
    return groups
