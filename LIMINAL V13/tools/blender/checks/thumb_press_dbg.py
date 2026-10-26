import os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import common
from models import models_hand as H
from models import hand_grip as G
common.reset()
log = []
ob, arm, sw = H.build_final(log)
rig = H.Rig(arm, ob)
tsel = np.where(rig.W[:, rig.idx['th_2']] > 0.9)[0]
c, a = np.zeros(3), np.array([1.0, 0, 0])
dt0, t0 = H.axis_dist(rig.V[tsel], c, a)
pad = tsel[dt0 < dt0.min() + 0.003]
print('pad n', len(pad), 'd', dt0.min() * 1000, 'tip x range', rig.V[tsel][:, 0].min(), rig.V[tsel][:, 0].max())
names = ['th_0', 'th_1', 'th_2']
def test(qs, label):
    S = G.chain(rig, names, qs)
    P = G.skin_sel(rig, S, pad)
    d = H.axis_dist(P, c, a)[0]
    d0 = H.axis_dist(rig.V[pad], c, a)[0]
    print(label, 'min', round(float(d.min() - d0.min()) * 1000, 2), 'mean', round(float(d.mean() - d0.mean()) * 1000, 2))
from mathutils import Quaternion
for deg in (-10, 10):
    test([H.flex(deg), H.flex(0), H.flex(0)], f'th_0 flex {deg}')
    test([H.flex(0, deg), H.flex(0), H.flex(0)], f'th_0 abd {deg}')
    test([H.flex(0, 0, deg), H.flex(0), H.flex(0)], f'th_0 twist {deg}')
    test([H.flex(0), H.flex(deg), H.flex(0)], f'th_1 flex {deg}')
    test([H.flex(0), H.flex(0), H.flex(deg)], f'th_2 flex {deg}')
b = arm.data.bones['th_2']
print('th_2 head', tuple(round(x, 4) for x in b.head_local), 'tail', tuple(round(x, 4) for x in b.tail_local), 'z axis', tuple(round(x, 3) for x in b.matrix_local.to_3x3().col[2]))
