"""V13.3: Plague Goblins, Spaeher (dunkle Ledermaske mit Glasaugen) - Variante 1 von 'ent_goblin' (KREATUR_PlagueGoblins.md, goblin_parts.py)."""
import goblin_parts as G

SKELETON = 'ent_goblin'
LODS = (48000, 15000, 5000)


def build(J, bones):
    return G.body(J, bones, 1)
