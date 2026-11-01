"""V13.3: Ouroboros-Arbeiter, Geselle (schmal) - Variante 0 von 'ent_ouroboros' (KREATUR_OuroborosArbeiter.md, ouro_parts.py)."""
import ouro_parts as O

LODS = (70000, 22000, 7000)


def build(J, bones):
    return O.worker(J, bones, heavy=False)
