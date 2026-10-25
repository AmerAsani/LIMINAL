"""V12: Pruefmodell fuer die Pipeline - Ausrichtung (+x vorn, z oben), Massstab, Materialien, Fasen."""
import common


def build_pipeline_test():
    red = common.material('vorn', (0.85, 0.15, 0.1), look='paint', rough=0.5)
    concrete = common.material('beton', (0.9, 0.9, 0.9), look='concrete', tex='concrete_cast')
    glow = common.material('licht', (1.0, 0.95, 0.85), look='glow', glow=2.0)
    # Sockel 1 x 1 x 0.5 m mit Beton-Textur, Fase 3 cm
    common.box('sockel', (-0.5, -0.5, 0.0), (0.5, 0.5, 0.5), concrete, bevel=0.03)
    # roter Pfeil nach +x (vorn), 0,6 m lang, auf dem Sockel
    common.box('pfeil', (0.0, -0.05, 0.5), (0.6, 0.05, 0.6), red, bevel=0.01)
    # Leuchtwuerfel oben links (+y)
    common.box('licht', (-0.1, 0.3, 0.5), (0.1, 0.5, 0.7), glow)
