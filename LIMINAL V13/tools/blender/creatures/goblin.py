"""V13.3: Plague Goblins, Henchman (glatte helle Maske) - Variante 0 von 'ent_goblin' (KREATUR_PlagueGoblins.md, goblin_parts.py)."""
import goblin_parts as G

LODS = (48000, 15000, 5000)


def build(J, bones):
    return G.body(J, bones, 0)
