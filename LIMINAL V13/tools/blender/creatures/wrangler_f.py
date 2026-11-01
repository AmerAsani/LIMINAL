"""V13.3: Wrangler, Weibchen (Entity 75) - wurmartig, blass und klarer durchscheinend, statt des Mauls zwei Zangen
(wrangler.py, KREATUR_Wrangler.md). Gleiches Skelett wie das Maennchen."""
import wrangler

SKELETON = 'ent_wrangler'
LODS = wrangler.LODS


def build(J, bones):
    wrangler.FEMALE = True
    wrangler.SEEDS = False
    return wrangler.build(J, bones)
