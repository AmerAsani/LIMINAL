"""V13.3: Plague Goblins, das Portal der Bande (Oval, Sandstroeme) - Variante 3 von 'ent_goblin' (KREATUR_PlagueGoblins.md, goblin_parts.py)."""
import goblin_parts as G

SKELETON = 'ent_goblin'
LODS = (14000, 6000, 2400)


def build(J, bones):
    return G.portal(J, bones)
