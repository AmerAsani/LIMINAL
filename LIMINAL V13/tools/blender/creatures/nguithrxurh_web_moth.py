"""V13.3: Nguithr'xurh - Netzkugeln mit zwei eingesponnenen Deathmoths (Level 2: dort fressen sie vor allem Insekten und
Deathmoths). Siehe nguithrxurh_web.py."""
import nguithrxurh_web as W

SKELETON = 'ent_nguithrxurh'
LODS = W.LODS


def build(J, bones):
    return W.build_web(J, True)
