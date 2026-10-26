"""V13.2: haarlose Death Rat (Level 2: an die Hitze angepasst, Wikidot Entity 24). Gleiches Skelett, Haut statt Fell."""
import rat

LODS = rat.LODS
SKELETON = 'ent_rat'
SCALE = rat.SCALE


def build(J, bones):
    return rat.build_rat(J, True)
