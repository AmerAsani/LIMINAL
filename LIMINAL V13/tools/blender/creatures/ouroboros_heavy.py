"""V13.3: Ouroboros-Arbeiter, Brecher (schwer, geht auf den Knoecheln) - Variante 1 von 'ent_ouroboros' (ouro_parts.py)."""
import ouro_parts as O

SKELETON = 'ent_ouroboros'
LODS = (70000, 22000, 7000)


def build(J, bones):
    return O.worker(J, bones, heavy=True)
