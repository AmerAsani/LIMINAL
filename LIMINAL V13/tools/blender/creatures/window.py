"""V13.2: Fensterwesen (Entity 2, Wikidot Rev. 36) - KREATUR_Fensterwesen.md. Es sitzt in einem gewoehnlichen Fenster
(Level 188: Oeffnung 1,2 x 1,8 m; Rahmen, Sprosse und Bank kommen vom Fenster der Ebene, models_l188.py). Darin eine
verschwommene, verzerrte Landschaft, die nicht zur Wand passt (Material ent_view), und ein dunkler Umriss eines Menschen
ohne Zuege - ab der Huefte sichtbar, flach wie ein Schatten hinter Glas. Faengt es jemanden, bildet es einen
koerperlichen Arm aus (rund, dunkel), der aus dem Fenster greift; der andere Arm winkt (lockt).

Wurzel: Mitte der Oeffnung auf der Wandflaeche, x aus der Wand. Knochen (wn): 0 Wurzel, 1 Hintergrund, 2 Rumpf, 3 Brust,
4 Kopf, 5-7 linker Arm, 8-10 rechter Arm. Ebenen (x): Rueckwand des Fensters 0..0,005, Ansicht -0,006..0,0075 (vor der
Rueckwand), Umriss um 0,016 (+-0,007), Sprosse ab 0,026.
"""
import numpy as np

import anat as A
import sdf

LODS = (9000, 3500, 1200)
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])
W, H = 1.2, 1.8
T = 0.007  # halbe Dicke des Umrisses


def build(J, bones):
    J = np.asarray(J)
    view = [A.box(1, np.array([0.00075, 0, 0]), X, (0.00675, W / 2 - 0.004, H / 2 - 0.004), 0.0, k=0.0)]
    base, chest, neck, top = J[2], J[3], J[4], J[5]
    y0 = base[1]

    def P(y, z):
        return np.array([base[0], y0 + y, z])

    # Umriss: Huefte und Bauch, Brust, Schulterlinie, Hals, Kopf - flache Ellipsen ineinander verschmolzen
    fig = [A.ell(2, P(0, -0.76), Z, (0.2, 0.145, T), up=X, k=0.0),
           A.ell(3, P(0, -0.57), Z, (0.15, 0.165, T), up=X, k=0.08),
           A.ell(4, P(0, -0.355), Z, (0.06, 0.047, T), up=X, k=0.035),
           A.ell(4, P(0.004, -0.215), Z, (0.118, 0.084, T * 1.15), up=X, k=0.03)]
    # Schulterlinie: vom Hals schraeg zu den Schultern (Kapuzenmuskel), runde Schulterkappen - kein waagrechter Balken
    for g in (1.0, -1.0):
        a, b = P(g * 0.04, -0.375), P(g * 0.185, -0.445)
        fig.append(A.ell(3, A.lerp(a, b, 0.5), A.unit(b - a), (np.linalg.norm(b - a) * 0.5 + 0.03, 0.04, T), up=X, k=0.05))
        fig.append(A.ell(3, b + np.array([0, -g * 0.005, -0.03]), Z, (0.06, 0.045, T), up=X, k=0.05))
    # Arme: koerperlich und rund (die Seite: es bildet einen Arm aus, um zu greifen); lange, duenne Finger
    arms = []
    for (sh, b0) in ((6, 5), (10, 8)):
        S, E, Wr, Hn = J[sh], J[sh + 1], J[sh + 2], J[sh + 3]
        arms.append(A.cone(b0, S, E, 0.04, 0.031, k=0.02))
        arms.append(A.cone(b0 + 1, E, Wr, 0.031, 0.023, k=0.015))
        hd = A.unit(Hn - Wr)
        across = A.unit(np.cross(X, hd))
        arms.append(A.ell(b0 + 2, Wr + hd * 0.05, hd, (0.055, 0.036, 0.013), up=X, k=0.015))
        for f in range(4):
            p0 = Wr + hd * 0.085 + across * (f - 1.5) * 0.017
            fl = 0.11 - abs(f - 1.4) * 0.012
            arms += A.chain(lambda i, b=b0 + 2: b, [p0, p0 + hd * fl * 0.5, p0 + hd * fl], [0.0075, 0.006, 0.003], k=0.005)
        tb = Wr + hd * 0.03 + across * 0.032
        arms += A.chain(lambda i, b=b0 + 2: b, [tb, tb + A.unit(hd + across) * 0.04, tb + A.unit(hd + across * 0.4) * 0.075],
                        [0.009, 0.007, 0.0035], k=0.005)

    def fig_disp(Q):  # unscharfer, rauchiger Rand (kein glatter Koerper)
        return -0.0022 * sdf.fbm(Q * np.array([0.4, 1.0, 1.0]), 16, 3, seed=201)

    def arm_disp(Q):  # Sehnen und Falten, matt
        return -0.0012 * sdf.fbm(Q * np.array([1.0, 1.0, 0.4]), 40, 2, seed=202)

    return [
        {'name': 'ansicht', 'fm': 'ent_view', 'parts': view, 'disp': None, 'h': 0.004, 'cav_scale': 0.002, 'lod_weight': 0.15},
        {'name': 'umriss', 'fm': 'ent_umbra', 'parts': fig, 'disp': fig_disp, 'h': 0.0035, 'cav_scale': 0.01},
        {'name': 'arme', 'fm': 'ent_umbra', 'parts': arms, 'disp': arm_disp, 'h': 0.003, 'cav_scale': 0.008, 'lod_weight': 1.4},
    ]
