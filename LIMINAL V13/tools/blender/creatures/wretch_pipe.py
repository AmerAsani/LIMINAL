"""V13.3 Level 3: Wretch mit Rohrstueck mit gusseisernem Winkel (wretch_l3.py, KREATUR_Wretch.md)."""
import wretch_l3 as L3

LODS = (30000, 10000, 3400)
SKELETON = 'ent_wretch'


def build(J, bones):
    return L3.build_armed(J, bones, 'pipe')
