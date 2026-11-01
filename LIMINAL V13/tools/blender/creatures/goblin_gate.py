"""V13.3: Plague Goblins, Torwaechterin (Hut, Samtcape, verzierte Maske, Silberstock mit Sanduhr) - Variante 2 von 'ent_goblin' (KREATUR_PlagueGoblins.md, goblin_parts.py)."""
import goblin_parts as G

SKELETON = 'ent_goblin'
LODS = (60000, 18000, 6000)


def build(J, bones):
    return G.body(J, bones, 2)
