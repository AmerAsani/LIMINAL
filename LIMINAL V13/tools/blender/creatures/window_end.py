"""V13.3 Teil 4: Fensterwesen in Level 2 (KREATUR_Fensterwesen.md; Wikidot Entity 2 Rev. 36, Level 2 Rev. 93).

Level 2 hat keine Fenster. Entity 2: das Wesen bildet sich auf fast jeder Flaeche, lieber an Waenden, und nimmt die Form an,
die zur Ebene passt. In den Versorgungsgaengen ist das ein altes Stahlfenster einer Fabrikhalle in der Betonstirnwand
einer Sackgasse: ein Stahlkasten (9,5 cm aus der Wand), dunkel lackiert, abgeplatzt und rostig, Nieten, eine Bank, oben
ein Kaempfer mit zwei kleinen Feldern (eines mit verschmutztem Drahtglas, gesprungen; eines leer), das grosse untere Feld
ohne Glas (Scherben in den Ecken). Darin wie in Level 188 (window.py): die verschwommene Landschaft (ent_view) und der
dunkle Umriss mit den Armen. Moertelfuge rings um den Kasten (er sitzt in der Wand).

Wurzel: Mitte der Oeffnung auf der Wandflaeche (Spiel: 1,85 m ueber dem Boden), x aus der Wand. Oeffnung 1,10 m breit,
z -0,90 .. +0,52 (Unterkante 0,95 m, Oberkante 2,37 m ueber dem Boden - die Tunnel sind 2,6 m hoch). Skelett ent_window.
"""
import numpy as np

import anat as A
import sdf
import window as WN

LODS = (14000, 5000, 1600)
SKELETON = 'ent_window'
X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])
W2 = 1.10            # Oeffnung (Breite)
Z0, Z1 = -0.90, 0.52  # Unter-, Oberkante der Oeffnung
D = 0.11             # Tiefe des Kastens (Runde 2: tiefer und breiter - vorher ein duenner Bilderrahmen)
FR = 0.10            # Breite des Rahmens
ZT = 0.12            # Kaempfer (Querriegel)


def _box(bone, c, half, rnd=0.003, k=0.0, op='add', up=Z):
    return A.box(bone, np.asarray(c, dtype=np.float64), X, half, rnd, up=up, k=k, op=op)


def build(J, bones):
    rng = np.random.default_rng(2402)
    groups = WN.build(J, bones)
    # Ansicht: auf die Oeffnung dieses Fensters (die Platte liegt an der Wand, hinter dem Kasten - wirkt tief)
    zc, hz = (Z0 + Z1) * 0.5, (Z1 - Z0) * 0.5
    groups[0]['parts'] = [_box(1, (0.00075, 0, zc), (0.00675, W2 / 2 - 0.004, hz - 0.004), 0.0)]
    # --- Stahlkasten: Pfosten, Sturz, Bank, Stirnblech, Kaempfer, Sprosse ------------------------------------------------
    case = []
    for s in (1, -1):
        case.append(_box(0, (D / 2, s * (W2 / 2 + FR / 2), zc), (D / 2, FR / 2, hz + FR)))
        case.append(_box(0, (D - 0.007, s * (W2 / 2 + FR / 2 + 0.008), zc), (0.007, FR / 2 + 0.012, hz + FR + 0.012)))  # Stirnblech
    case.append(_box(0, (D / 2, 0, Z1 + FR / 2), (D / 2, W2 / 2 + FR, FR / 2)))
    case.append(_box(0, (D - 0.007, 0, Z1 + FR / 2 + 0.006), (0.007, W2 / 2 + FR + 0.02, FR / 2 + 0.012)))
    # Bank: ragt vor, faellt leicht nach vorn ab
    case.append(_box(0, (D / 2 + 0.035, 0, Z0 - FR / 2), (D / 2 + 0.035, W2 / 2 + FR + 0.025, FR / 2 - 0.004),
                     up=A.unit(np.array([0.12, 0, 1.0]))))
    # Kaempfer und Sprosse im oberen Teil (tiefer im Kasten - die Ansicht liegt dahinter)
    case.append(_box(0, (D * 0.55, 0, ZT), (D * 0.32, W2 / 2, 0.019)))
    case.append(_box(0, (D * 0.55, 0, (ZT + Z1) * 0.5), (D * 0.32, 0.017, (Z1 - ZT) * 0.5)))
    # Kittfalz: Winkel an der Innenkante der Oeffnung
    for s in (1, -1):
        case.append(_box(0, (D * 0.5, s * (W2 / 2 - 0.006), zc), (0.012, 0.006, hz)))
    case.append(_box(0, (D * 0.5, 0, Z0 + 0.006), (0.012, W2 / 2, 0.006)))
    # Beulen und Kanten: der Kasten ist alt (eine Delle im Sturz, ein verbogener Pfosten)
    case.append(_box(0, (D + 0.004, 0.22, Z1 + FR / 2), (0.012, 0.06, 0.02), rnd=0.01, k=0.01, op='sub'))

    def case_disp(Q):  # gewalzter Stahl: leichte Wellen, abgeplatzter Lack (Krater)
        d = 0.00035 * sdf.fbm(Q, 25.0, 2, 71)
        d1, d2 = sdf.worley(Q, 18.0, seed=72)
        return d + 0.0009 * np.clip((0.18 - d1) / 0.18, 0, 1) ** 2
    # --- Nieten an Stirnblech und Bank ---------------------------------------------------------------------------------
    rivets = []
    for s in (1, -1):
        for z in np.arange(Z0 - FR * 0.5, Z1 + FR * 0.6, 0.11):
            rivets.append(A.ell(0, np.array([D + 0.001, s * (W2 / 2 + FR / 2 + 0.01), z]), X, (0.004, 0.0055, 0.0055), k=0.0))
    for y in np.arange(-W2 / 2 - FR * 0.5, W2 / 2 + FR * 0.6, 0.11):
        rivets.append(A.ell(0, np.array([D + 0.001, y, Z1 + FR / 2 + 0.006]), X, (0.004, 0.0055, 0.0055), k=0.0))
    # --- Glas: oben links verschmutztes Drahtglas, gesprungen; Scherben in den Ecken des unteren Felds ---------------------
    glass = []
    pane_c = np.array([D * 0.42, -W2 / 4 - 0.004, (ZT + Z1) * 0.5])
    glass.append(_box(0, pane_c, (0.0025, W2 / 4 - 0.02, (Z1 - ZT) * 0.5 - 0.02), 0.0))
    # Runde 2: ein Einschlag statt Kritzelei; Runde 3: die Spruenge gezackt und ungleich lang (vorher ein Stern wie ein Symbol)
    hit = pane_c + np.array([0.0, 0.07, -0.03])
    for k in range(5):
        a = k * 2 * np.pi / 5 + rng.uniform(-0.5, 0.5)
        p = hit.copy()
        for seg in range(rng.integers(2, 5)):
            a += rng.uniform(-0.6, 0.6)
            L_ = rng.uniform(0.02, 0.05)
            dv = np.array([0.0, np.cos(a), np.sin(a)])
            up = np.array([0.0, -np.sin(a), np.cos(a)])
            glass.append(_box(0, p + dv * L_ * 0.5, (0.006, L_ * 0.5 + 0.001, 0.0009), 0.0, op='sub', up=up))
            p = p + dv * L_
    glass.append(_box(0, hit, (0.006, 0.005, 0.004), 0.002, op='sub'))
    for k in range(6):  # Schmutz auf dem Glas (unten dichter)
        glass.append(A.ell(0, pane_c + np.array([0.003, rng.uniform(-0.2, 0.2), rng.uniform(-0.17, -0.08)]), X, (0.0015, rng.uniform(0.02, 0.05), rng.uniform(0.01, 0.025)), k=0.01))
    for (cy, cz, sy, sz) in ((W2 / 2 - 0.02, Z0 + 0.02, -1, 1), (-W2 / 2 + 0.02, Z0 + 0.02, 1, 1), (W2 / 2 - 0.02, ZT - 0.03, -1, -1),
                             (-W2 / 2 + 0.02, ZT - 0.04, 1, -1)):
        for k in range(rng.integers(1, 3) + 1):  # spitze Scherben, schraeg im Kittfalz
            a = rng.uniform(0.3, 1.2)
            up = np.array([0.0, sy * np.sin(a), sz * np.cos(a)])
            L_ = rng.uniform(0.04, 0.12)
            glass.append(_box(0, (D * 0.42, cy + sy * L_ * 0.3, cz + sz * L_ * 0.3), (0.002, rng.uniform(0.006, 0.02), L_), 0.0, up=up))
    # --- Rost: Laeufer unter der Bank und in den Ecken, auf der Bank eine Kruste ------------------------------------------
    rust = []
    for k in range(9):
        y = rng.uniform(-W2 / 2 - FR * 0.6, W2 / 2 + FR * 0.6)
        L_ = rng.uniform(0.06, 0.22)
        rust.append(_box(0, (D + 0.0015, y, Z0 - FR - L_ * 0.5 + 0.01), (0.0015, rng.uniform(0.006, 0.016), L_ * 0.5), 0.002))
    for s in (1, -1):
        for k in range(3):
            z = rng.uniform(Z0, Z1)
            rust.append(_box(0, (D + 0.0012, s * (W2 / 2 + 0.012), z), (0.0012, rng.uniform(0.006, 0.014), rng.uniform(0.03, 0.09)), 0.002))
    rust.append(_box(0, (D / 2 + 0.035, 0, Z0 - 0.004), (D / 2 + 0.03, W2 / 2 + FR, 0.0022), 0.002, up=A.unit(np.array([0.12, 0, 1.0]))))
    # --- Moertelfuge rings um den Kasten (er sitzt in der Wand), Staub und tote Insekten auf der Bank ------------------------
    mortar = []
    for s in (1, -1):
        mortar.append(_box(0, (0.006, s * (W2 / 2 + FR + 0.02), zc), (0.006, 0.022, hz + FR + 0.03), 0.004))
    mortar.append(_box(0, (0.006, 0, Z1 + FR + 0.03), (0.006, W2 / 2 + FR + 0.04, 0.022), 0.004))
    mortar.append(_box(0, (0.006, 0, Z0 - FR - 0.02), (0.006, W2 / 2 + FR + 0.05, 0.022), 0.004))
    for k in range(18):  # Staubkoerner, Mottenleichen (dunkle Kluempchen)
        y = rng.uniform(-W2 / 2, W2 / 2)
        x = rng.uniform(D * 0.6, D + 0.05)
        r = rng.uniform(0.004, 0.011)
        mortar.append(A.ell(0, np.array([x, y, Z0 + 0.002 - (x - D / 2) * 0.12 + r * 0.3]), X, (r, r * 1.6, r * 0.6), k=0.002))

    def mortar_disp(Q):
        return 0.0012 * sdf.fbm(Q, 60.0, 3, 73)

    # Runde 3: der Kasten ist durchgerostet (vorher schwarz lackiert - im Dunkel unsichtbar); Reste des Lacks als Platten
    paint = []
    for s in (1, -1):
        for k in range(5):
            z = rng.uniform(Z0 - FR * 0.4, Z1 + FR * 0.4)
            paint.append(_box(0, (D + 0.0012, s * (W2 / 2 + FR / 2 + 0.008) + rng.uniform(-0.02, 0.02), z),
                              (0.0012, rng.uniform(0.015, FR / 2), rng.uniform(0.03, 0.12)), 0.004))
    for k in range(5):
        paint.append(_box(0, (D + 0.0012, rng.uniform(-W2 / 2, W2 / 2), Z1 + FR / 2 + 0.006), (0.0012, rng.uniform(0.04, 0.14), rng.uniform(0.015, FR / 2)), 0.004))
    groups += [
        {'name': 'kasten', 'fm': 'ent_ob_rust', 'parts': case, 'disp': case_disp, 'h': 0.0028, 'cav_scale': 0.006, 'lod_weight': 0.8},
        {'name': 'lack', 'fm': 'ent_ob_paint', 'parts': paint, 'disp': lambda Q: 0.0004 * sdf.fbm(Q, 60.0, 2, 75), 'h': 0.0016, 'cav_scale': 0.003,
         'lod_weight': 0.3},
        {'name': 'nieten', 'fm': 'ent_ob_steel', 'parts': rivets, 'disp': None, 'h': 0.0016, 'cav_scale': 0.003, 'lod_weight': 0.3},
        {'name': 'glas', 'fm': 'ent_pg_glass', 'parts': glass, 'disp': None, 'h': 0.0016, 'cav_scale': 0.003, 'lod_weight': 0.5},
        {'name': 'schmutz', 'fm': 'ent_ob_paint', 'parts': rust, 'disp': lambda Q: 0.0005 * sdf.fbm(Q, 80.0, 2, 74), 'h': 0.0016,
         'cav_scale': 0.003, 'lod_weight': 0.4},
        {'name': 'moertel', 'fm': 'ent_concrete', 'parts': mortar, 'disp': mortar_disp, 'h': 0.0028, 'cav_scale': 0.006, 'lod_weight': 0.4},
    ]
    return groups
