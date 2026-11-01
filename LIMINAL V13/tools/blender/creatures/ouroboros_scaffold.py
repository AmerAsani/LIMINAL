"""V13.3: Ouroboros - das Fahrgeruest, das immer wieder umkippt (Stirnrahmen, Diagonalen, Belaege, Lenkrollen;
Gasflaschenwagen, Absperrung mit Warnleuchte) - Variante 3 von 'ent_ouroboros' (ouro_parts.py)."""
import ouro_parts as O

SKELETON = 'ent_ouroboros'
LODS = (50000, 16000, 5000)


def build(J, bones):
    return O.scaffold(J, bones)
