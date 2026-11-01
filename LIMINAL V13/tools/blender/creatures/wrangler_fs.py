"""V13.3: Wrangler, traechtiges Weibchen (Entity 75) - wie wrangler_f, dazu die Eier, die als Samen am Leib wachsen
(Buckel voller Gruebchen, in jedem ein dunkler Same). Gleiches Skelett wie das Maennchen."""
import wrangler

SKELETON = 'ent_wrangler'
LODS = wrangler.LODS


def build(J, bones):
    wrangler.FEMALE = True
    wrangler.SEEDS = True
    return wrangler.build(J, bones)
