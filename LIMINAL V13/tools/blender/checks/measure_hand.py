"""V13: Masse der subdividierten Hand (Ruhepose) gegen die Zielwerte pruefen (je Finger nur seine Punkte)."""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import common  # noqa: E402
from models import models_hand as H  # noqa: E402

common.reset()
ob, arm = H.make_hand(2)
rig = H.Rig(arm, ob)
V = rig.V
print('Punkte', len(V), 'Flaechen', len(ob.data.polygons))
for (n, mcp, deg, lens, wd, th) in H.FING:
    m = np.array(mcp)
    a = math.radians(deg)
    u = np.array([math.cos(a), math.sin(a), 0.0])
    s = np.array([-u[1], u[0], 0.0])
    own = sum(rig.W[:, rig.idx[f'{n}_{k}']] for k in (1, 2, 3)) + rig.W[:, rig.idx[n + '_mc']]
    t = (V - m) @ u
    out = []
    for label, tt in (('P1', lens[0] * 0.55), ('PIP', lens[0]), ('P2', lens[0] + lens[1] * 0.5), ('DIP', lens[0] + lens[1]), ('P3', lens[0] + lens[1] + lens[2] * 0.55)):
        sel = (np.abs(t - tt) < 0.0018) & (own > 0.95)
        lat = (V[sel] - m) @ s
        z = V[sel][:, 2]
        out.append(f'{label} {1000 * (lat.max() - lat.min()):.1f}x{1000 * (z.max() - z.min()):.1f}')
    sel = own > 0.95
    print(f'{n}: ' + ', '.join(out) + f'  | Ziel PIP {wd * 1000:.1f}x{th * 1000:.1f} | Spitze {((V[sel] - m) @ u).max() * 1000:.1f} (Ziel {sum(lens) * 1000:.1f})')
th = sum(rig.W[:, rig.idx[f'th_{k}']] for k in (1, 2))
sel = th > 0.95
print('Daumen Punkte', sel.sum())
palm = rig.W[:, rig.idx['hand']] + sum(rig.W[:, rig.idx[f'f{k}_mc']] for k in range(4))
for x in (0.02, 0.04, 0.06, 0.075):
    s2 = (np.abs(V[:, 0] - x) < 0.001) & (palm > 0.9)
    print(f'Handteller x={x * 1000:.0f}: y {1000 * V[s2][:, 1].min():.1f}..{1000 * V[s2][:, 1].max():.1f}, z {1000 * V[s2][:, 2].min():.1f}..{1000 * V[s2][:, 2].max():.1f}')
