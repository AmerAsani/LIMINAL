"""V13.3: Ouroboros - die Mauer, die die Arbeiter bauen und einreissen (sieben Lagen Hochlochziegel, lose Steine, Brocken,
Palette, Moertelkuebel, Absperrung mit Warnleuchte) - Variante 2 von 'ent_ouroboros' (ouro_parts.py)."""
import ouro_parts as O

SKELETON = 'ent_ouroboros'
LODS = (60000, 20000, 7000)


def build(J, bones):
    return O.wall(J, bones)
