"""V13.3 Level 3 (KREATUR_Clump.md): das grosse Buendel, entwaffnet - das Rohr ist zerbrochen, an Arm 3 hat es sich die Hand
abgerissen und Speiche und Elle als frischen Knochenspiess freigelegt (Wikidot Level 3 Rev. 18: entwaffnet verstuemmelt es
sich und nutzt blanken Knochen als Waffe). Sonst wie clump_l3.py (Variante 2 der Rolle Clump).
"""
import clump_l3

LODS = clump_l3.LODS
SKELETON = 'ent_clump'


def build(J, bones):
    return clump_l3.build_big(J, bones, bare=True)
