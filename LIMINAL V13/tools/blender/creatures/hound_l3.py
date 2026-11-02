"""V13.3 Level 3 (KREATUR_Hound.md, Abschnitt "Rudel"): Hound, der in Level 3 ("Electrical Station") allein lauert.

Wikidot Level 3 (Rev. 18): Hounds verstecken sich dort perfekt, springen von hinten an und jagen in Gruppen, die sich durch
Bellen verstaendigen; sie quaelen ihre Opfer. Aussehen wie Entity 8 (Rev. 56) - die Zeichen sind Auslegung aus der Ebene
(Umspannwerk, blanke Leitungen): ueber Ruecken und Flanken verzweigte, erhabene Brandnarben eines Stromschlags
(Lichtenberg-Figur, glaenzendes Narbengewebe), Haende und Fuesse russgeschwaerzt (er liegt in den Kabelschaechten), ein Auge
milchig trueb. Grundform, Knochen und Gewichte wie hound.py.
"""
import hound
import hound_marks as M

LODS = (60000, 17000, 4800)
SKELETON = 'ent_hound'


def build(J, bones):
    groups = hound.build(J, bones)
    skin = M.part_of(groups, 'haut')
    groups += M.lichtenberg(skin, 3301, roots=((-0.33, 1), (-0.12, -1), (0.10, 1), (0.02, -1)))
    groups += M.overlay(J, skin, 'ent_hd_soot', 'russ', 3302, lift=0.0006)
    eyes = M.part_of(groups, 'augen')
    left = [p for p in eyes['parts'] if p.g.get('c', (0, 0, 0))[1] > 0]
    right = [p for p in eyes['parts'] if p not in left]
    if left and right:  # das linke Auge milchig trueb
        eyes['parts'] = right
        groups.append({'name': 'auge_trueb', 'fm': 'ent_tooth', 'parts': left, 'disp': None, 'h': 0.0008, 'cav_scale': 0.004})
    return groups
