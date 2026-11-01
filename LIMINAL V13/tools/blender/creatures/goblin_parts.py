"""V13.3: Plague Goblins (Wikidot entity-25, Rev. 28; KREATUR_PlagueGoblins.md) - gemeinsame Teile der vier Modelle im Skelett
'ent_goblin' (anim/creature.hpp, namespace gb): Henchman (goblin.py), Spaeher (goblin_scout.py), Torwaechterin (goblin_gate.py),
Portal (goblin_portal.py).

Wiki: katzengrosse, halb zweibeinige Mischwesen aus Vogel und Katze; weiche, schwarze, fellartige Federn am ganzen Koerper;
geschuppte Glieder mit kraeftigen, geschickten Krallen; unter der Maske ein Kraehenschnabel voller rasiermesserscharfer
Saegezaehne; riesige, stark spiegelnde Augen mit gelbem Schimmer; zwei lange, duenne Ohren mit Pinseln. Masken und Umhaenge wie
mittelalterliche Pestaerzte. Henchmen: glatte hellbraune oder weisse Masken ohne Merkmale. Spaeher: dunkle Ledermasken mit
Glasaugen. Torwaechterinnen (erwachsene Weibchen, groesser): breitkrempiger Hut, feineres Cape, reich verzierte Maske, Silberstock
mit Sanduhr. Auslegung: senkrechte Schlitzpupillen (die Katze), ein gefiederter Katzenschwanz, zerrissener Umhangsaum.
"""
import math

import numpy as np

import anat as A
import sdf

X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])

# Gelenke und Knochen (wie anim/creature.hpp, gb)
ROOT, PELVIS, SPINE, CHEST, NECK, HEAD, SNOUT, CROWN, JAWP, JAWT, MASKP, MASKT = range(12)
EAR0 = 12
ARM0 = EAR0 + 6
LEG0 = ARM0 + 8
TAIL0 = LEG0 + 10
KSTRANDS, KSWIRLS = 6, 6
CLOAK0 = TAIL0 + 4
CANE_A = CLOAK0 + KSTRANDS * 3
CANE_B, ITEM_A, ITEM_B, PORT_A, PORT_B = CANE_A + 1, CANE_A + 2, CANE_A + 3, CANE_A + 4, CANE_A + 5
SWIRL0 = CANE_A + 6
B_ROOT, B_PELVIS, B_SPINE, B_CHEST, B_NECK, B_HEAD, B_JAW, B_MASK = range(8)
B_EAR0 = 8
B_ARM0 = B_EAR0 + 4
B_LEG0 = B_ARM0 + 6
B_TAIL0 = B_LEG0 + 8
B_CLOAK0 = B_TAIL0 + 3
B_CANE = B_CLOAK0 + KSTRANDS * 2
B_ITEM, B_PORTAL, B_SWIRL0 = B_CANE + 1, B_CANE + 2, B_CANE + 3
STRAND_ANG = (0.62, 1.62, 2.62, -2.62, -1.62, -0.62)
PORTAL_H, PORTAL_W = 1.25, 0.9


def earJ(s, i): return EAR0 + s * 3 + i
def earB(s, i): return B_EAR0 + s * 2 + i
def armJ(s, i): return ARM0 + s * 4 + i
def armB(s, i): return B_ARM0 + s * 3 + i
def legJ(s, i): return LEG0 + s * 5 + i
def legB(s, i): return B_LEG0 + s * 4 + i
def cloakJ(k, i): return CLOAK0 + k * 3 + i
def cloakB(k, i): return B_CLOAK0 + k * 2 + i
def swirlJ(k, i): return SWIRL0 + k * 2 + i


def check(bones):
    assert bones[B_CANE]['a'] == CANE_A and bones[B_PORTAL]['a'] == PORT_A and bones[B_CLOAK0]['a'] == CLOAK0, 'Skelett passt nicht zu goblin_parts.py'


# --- Oberflaechen -----------------------------------------------------------------------------------------------------
def feather_disp(Q):  # Federfell: dachziegelartige Schuppen (Zellen), feine Straehnen, weich
    d1, d2 = sdf.worley(Q * np.array([1.0, 1.0, 1.4]), 95, seed=2501)
    edge = np.exp(-((d2 - d1) / 0.12) ** 2)  # Rand jeder Feder
    streak = sdf.ridged(Q * np.array([0.6, 1.0, 0.35]), 160, 2, seed=2502)
    return -0.0009 * d1 - 0.0005 * edge - 0.0006 * (streak - 0.5) + 0.0004 * sdf.fbm(Q, 40, 2, seed=2503)


def skin_disp(Q):  # kahle Haut: tiefe Runzeln, Schorf
    return -0.0007 * sdf.ridged(Q * np.array([1.0, 0.6, 1.6]), 210, 2, seed=2515) + 0.0003 * sdf.fbm(Q, 500, 2, seed=2516)


def scale_disp(Q):  # Schuppen der Laeufe und Finger: kleine Schilde mit tiefen Fugen
    d1, d2 = sdf.worley(Q, 420, seed=2511)
    return -0.0005 * np.exp(-((d2 - d1) / 0.1) ** 2) - 0.00015 * d1 + 0.00012 * sdf.fbm(Q, 900, 1, seed=2512)


def beak_disp(Q):  # Horn: Laengsriefen, kleine Absplitterungen
    return -0.00025 * sdf.ridged(Q * np.array([0.25, 1.0, 1.0]), 300, 2, seed=2521)


def cloak_disp(Q):  # grobes Tuch: Koeper, Knitter, Flicken-Kanten; der Saum zerrissen (Fetzen und Loecher)
    z = Q[:, 2]
    weave = 0.00035 * np.sin(Q[:, 0] * 900.0 + Q[:, 1] * 900.0) * np.sin(z * 700.0)
    crum = -0.0012 * sdf.ridged(Q * np.array([1.0, 1.0, 0.6]), 30, 2, seed=2531)
    rag = sdf.fbm(Q * np.array([1.0, 1.0, 0.35]), 22, 3, seed=2532)
    hem = np.clip((0.215 - z) / 0.07, 0.0, 1.0)
    holes = np.where(rag * hem > 0.22, 0.012 * hem, 0.0)
    return weave + crum + holes


def cape_disp(Q):  # Samt: weiche, schwere Falten, kein Riss
    return -0.0009 * sdf.ridged(Q * np.array([1.0, 1.0, 0.4]), 24, 2, seed=2541) + 0.0002 * sdf.fbm(Q, 300, 1, seed=2542)


def mask_disp(Q):  # glatt, abgegriffen (kaum Struktur)
    return 0.00015 * sdf.fbm(Q, 120, 2, seed=2551)


def leather_disp(Q):  # Leder: Narben, Falten, eine Naht laengs ueber den Schnabel (y = 0)
    seam = np.exp(-(Q[:, 1] / 0.0012) ** 2) * 0.0006 * (0.5 + 0.5 * np.sin(Q[:, 0] * 2400.0))
    return -0.0005 * sdf.ridged(Q, 160, 2, seed=2561) + seam


def ornate_disp(Q):  # Lack mit eingeschnittenem Rankenmuster
    a = np.arctan2(Q[:, 2] - 0.5, Q[:, 1])
    vine = np.sin(Q[:, 0] * 260.0 + 4.0 * np.sin(a * 3.0)) * np.sin(a * 6.0 + Q[:, 0] * 90.0)
    return -0.00045 * (vine > 0.55) + 0.0001 * sdf.fbm(Q, 200, 1, seed=2571)


def hat_disp(Q):  # Filz, eingedrueckt
    return -0.0006 * sdf.fbm(Q, 35, 2, seed=2581)


def void_disp(Q):  # das Innere des Portals: wirbelnde Schlieren
    r = np.sqrt(Q[:, 1] ** 2 + (Q[:, 2] - 0.675) ** 2)
    a = np.arctan2(Q[:, 2] - 0.675, Q[:, 1])
    return 0.004 * np.sin(a * 5.0 + r * 18.0) * (0.5 + 0.5 * sdf.fbm(Q, 6, 2, seed=2591))


def rim_disp(Q):
    return 0.006 * sdf.ridged(Q, 18, 3, seed=2592) - 0.002


# --- Koerper ----------------------------------------------------------------------------------------------------------
def body(J, bones, caste):
    """caste: 0 Henchman, 1 Spaeher, 2 Torwaechterin."""
    check(bones)
    J = np.asarray(J, dtype=float)
    rng = np.random.default_rng(2500 + caste)
    feather, scale, talon, beak, tooth, mouth, eye, pupil, hair, skin = [], [], [], [], [], [], [], [], [], []
    groups = []

    # Rumpf: Becken, Bauch, Brust mit Kiel (Vogel), Schultern
    feather += [
        A.ell(B_PELVIS, (-0.03, 0, 0.22), X, (0.06, 0.05, 0.052), k=0.0),
        A.ell(B_SPINE, (-0.005, 0, 0.3), A.unit(J[CHEST] - J[PELVIS]), (0.062, 0.047, 0.05), up=-X, k=0.03),
        A.ell(B_CHEST, (0.035, 0, 0.385), A.unit(J[NECK] - J[SPINE]), (0.055, 0.045, 0.05), up=X, k=0.03),
        A.ell(B_CHEST, (0.062, 0, 0.37), Z, (0.04, 0.026, 0.022), up=X, k=0.02),  # Kiel
    ]
    for s, g in ((0, 1.0), (1, -1.0)):
        feather.append(A.ell(B_CHEST, (0.03, g * 0.055, 0.42), X, (0.03, 0.026, 0.028), k=0.02))
    # Hals mit Halskrause (aufgeplustert)
    feather += [A.cone(B_NECK, (0.05, 0, 0.44), (0.085, 0, 0.51), 0.031, 0.029, k=0.02),
                A.ell(B_NECK, (0.058, 0, 0.462), A.unit(J[HEAD] - J[NECK]), (0.03, 0.046, 0.043), up=X, k=0.02)]
    # Kopf: rund, Stirnwulst ueber den Augen, Wangen; Augenhoehlen gross
    feather += [
        A.ell(B_HEAD, (0.085, 0, 0.522), X, (0.05, 0.043, 0.045), k=0.02),
        A.ell(B_HEAD, (0.112, 0, 0.548), X, (0.024, 0.045, 0.018), k=0.014),
    ]
    for g in (1.0, -1.0):
        feather.append(A.ell(B_HEAD, (0.108, g * 0.03, 0.502), X, (0.03, 0.022, 0.024), k=0.02))
        feather.append(A.ell(B_HEAD, (0.122, g * 0.029, 0.529), A.unit((1.0, g * 0.35, 0.0)), (0.017, 0.024, 0.022), op='sub', k=0.006))
    # Runde 2: das Gesicht unter der Maske ist kahl - grau-rosa, runzlige Haut, wo die Maske die Federn abgerieben hat
    # (Auslegung); schwere obere Lider liegen halb ueber den Augen, die Schnabelwurzel ist wulstig
    skin += [A.ell(B_HEAD, (0.115, 0, 0.522), X, (0.033, 0.046, 0.042), k=0.0),
             A.ell(B_HEAD, (0.128, 0, 0.51), X, (0.016, 0.024, 0.02), k=0.012),   # Schnabelwurzel
             A.ell(B_HEAD, (0.1, 0, 0.483), X, (0.032, 0.03, 0.02), k=0.012)]    # Kehle (Runde 4: kahle Stelle groesser)
    for g in (1.0, -1.0):
        ax = A.unit((1.0, g * 0.35, 0.0))
        ec = np.array([0.117, g * 0.029, 0.528])
        skin.append(A.ell(B_HEAD, ec + ax * 0.004, ax, (0.017, 0.0235, 0.0215), op='sub', k=0.004))
        skin.append(A.ell(B_HEAD, ec + ax * 0.007 + Z * 0.013, ax, (0.0105, 0.0235, 0.0085), up=Z, k=0.005))  # oberes Lid
        skin.append(A.ell(B_HEAD, ec + ax * 0.006 - Z * 0.016, ax, (0.008, 0.019, 0.005), up=Z, k=0.004))     # unteres Lid
    # Augen: riesig (je etwa 4 cm), nach vorn und etwas zur Seite; senkrechte Schlitzpupille (die Katze - Auslegung)
    for g in (1.0, -1.0):
        ax = A.unit((1.0, g * 0.35, 0.0))
        ec = np.array([0.117, g * 0.029, 0.528])
        eye.append(A.ell(B_HEAD, ec, ax, (0.0148, 0.0215, 0.0195), k=0.0))
        pupil.append(A.ell(B_HEAD, ec + ax * 0.013, ax, (0.0016, 0.0016, 0.0135), k=0.0))
    # Ohren: wie beim Karakal - breit am Ansatz, spitz zulaufend, ein langer schwarzer Pinsel; innen nackte Haut (Runde 1: lang
    # und duenn wie Fuehler, unter dem Hut wie erhobene Arme)
    for s, g in ((0, 1.0), (1, -1.0)):
        E = [J[earJ(s, i)] for i in range(3)]
        feather.append(A.tube(earB(s, 0), E[0] - A.unit(E[1] - E[0]) * 0.006, E[1], (0.021, 0.0062), (0.0145, 0.0045), up=X, k=0.014))
        feather.append(A.tube(earB(s, 1), E[1], E[2], (0.0145, 0.0045), (0.0032, 0.002), up=X, k=0.004))
        mid = (E[0] + E[1]) / 2
        feather.append(A.ell(earB(s, 0), mid + X * 0.0045, A.unit(E[1] - E[0]), (0.05, 0.012, 0.0022), up=X, op='sub', k=0.003))
        skin.append(A.ell(earB(s, 0), mid + X * 0.0015 + A.unit(E[1] - E[0]) * 0.01, A.unit(E[1] - E[0]), (0.045, 0.0105, 0.0016), up=X, k=0.0))
        d = A.unit(E[2] - E[1])
        for t in range(7):
            a = (t - 3) * 0.12
            dd = A.unit(d + A.perp(d, Y * g) * math.sin(a) - X * 0.12)
            hair += A.chain(lambda i: earB(s, 1), [E[2] - d * 0.006, E[2] + dd * 0.024, E[2] + dd * 0.046 - X * 0.004], [0.0016, 0.001, 0.0002], k=0.0)
    # Schnabel (Kraehe): seitlich zusammengedrueckt, gebogener First, Hakenspitze; der Unterkiefer am eigenen Knochen
    beak += [
        A.tube(B_HEAD, (0.13, 0, 0.51), (0.2, 0, 0.492), (0.017, 0.022), (0.011, 0.014), k=0.0),
        A.tube(B_HEAD, (0.2, 0, 0.492), (0.245, 0, 0.47), (0.011, 0.014), (0.0032, 0.0045), k=0.01),
        A.cone(B_HEAD, (0.243, 0, 0.471), (0.252, 0, 0.459), 0.0032, 0.0007, k=0.004),
        A.box(B_HEAD, (0.19, 0, 0.463), A.unit((1.0, 0, -0.27)), (0.08, 0.03, 0.017), 0.0, k=0.002, op='sub'),  # Schneide unten flach
        A.tube(B_JAW, J[JAWP] + X * 0.01, J[JAWT], (0.0135, 0.011), (0.003, 0.003), k=0.0),
    ]
    mouth += [A.ell(B_JAW, (0.162, 0, 0.484), A.unit(J[JAWT] - J[JAWP]), (0.04, 0.0075, 0.0035), k=0.0),  # Zunge
              A.ell(B_HEAD, (0.17, 0, 0.491), A.unit((1.0, 0, -0.3)), (0.045, 0.0095, 0.0035), k=0.0)]       # Gaumen
    # Saegezaehne: oben neun, unten acht je Seite - klein, dreieckig, dicht (die Seite: "razor-sharp serrated teeth")
    for g in (1.0, -1.0):
        # Runde 4: aus 2-3 m nicht zu sehen - groesser, und sie stehen schraeg nach aussen ueber den Schnabelrand (Saege wie beim
        # Gaensesaeger, auch bei geschlossenem Schnabel sichtbar)
        up_pts = [A.lerp(np.array([0.142, g * 0.0135, 0.494]), np.array([0.238, g * 0.0034, 0.466]), t) for t in np.linspace(0, 1, 9)]
        tooth += A.teeth_row(B_HEAD, up_pts, 0.0125, 0.0029, [A.unit((-0.2, g * 0.55, -1.0))] * 9, rng, jitter=0.3)
        lo_pts = [A.lerp(np.array([0.13, g * 0.0125, 0.494]), np.array([0.226, g * 0.003, 0.461]), t) for t in np.linspace(0, 1, 8)]
        tooth += A.teeth_row(B_JAW, lo_pts, 0.011, 0.0026, [A.unit((0.15, g * 0.5, 1.0))] * 8, rng, jitter=0.3)

    # Arme: Oberarm befiedert, Unterarm und Hand geschuppt; drei lange Finger und ein Daumen mit gebogenen Krallen
    for s, g in ((0, 1.0), (1, -1.0)):
        P = [J[armJ(s, i)] for i in range(4)]
        feather.append(A.cone(armB(s, 0), P[0], P[1], 0.019, 0.012, k=0.015))
        scale += [A.cone(armB(s, 1), P[1], P[2], 0.0112, 0.0078, k=0.0),
                  A.ell(armB(s, 1), P[2], A.unit(P[2] - P[1]), (0.008, 0.0085, 0.0075), k=0.004)]  # Handgelenk
        hd = A.unit(P[3] - P[2])
        side = A.unit(np.cross(Z, hd)) * g
        palm = P[2] + hd * 0.013
        scale.append(A.ell(armB(s, 2), palm, hd, (0.013, 0.0095, 0.006), up=A.perp(hd, Z), k=0.004))
        for f in range(4):
            if f < 3:
                spr = (f - 1) * 0.32
                d = A.unit(hd + side * math.sin(spr))
                base = palm + d * 0.01
                p1 = base + d * 0.029 - Z * 0.004
                p2 = p1 + A.unit(d - Z * 0.5) * 0.023
                L = (0.0039, 0.0032, 0.0023)
            else:  # Daumen, zur Mitte gedreht
                d = A.unit(hd * 0.5 - side * 0.8)
                base = palm - hd * 0.004
                p1 = base + d * 0.014
                p2 = p1 + A.unit(d + hd * 0.6 - Z * 0.3) * 0.012
                L = (0.0034, 0.0028, 0.002)
            scale += A.chain(lambda i: armB(s, 2), [base, p1, p2], list(L), k=0.003)
            tip = p2 + A.unit(p2 - p1) * 0.001
            dd = A.unit(p2 - p1)
            c1 = tip + dd * 0.012 - Z * 0.002
            c2 = c1 + A.unit(dd * 0.35 - Z) * 0.012
            talon += A.chain(lambda i: armB(s, 2), [tip, c1, c2], [0.0024, 0.0015, 0.0003], k=0.001)

    # Beine: Schenkel befiedert ("Hosen" bis kurz ueber die Ferse), Lauf und Zehen geschuppt; drei Zehen vorn, eine hinten
    for s, g in ((0, 1.0), (1, -1.0)):
        P = [J[legJ(s, i)] for i in range(5)]
        feather += [A.muscle(legB(s, 0), P[0], P[1], 0.027, flat=0.9, k=0.02, t0=0.0, t1=1.0),
                    A.cone(legB(s, 1), P[1], P[1] + (P[2] - P[1]) * 0.8, 0.017, 0.0095, k=0.012)]
        scale += [A.cone(legB(s, 1), P[1] + (P[2] - P[1]) * 0.7, P[2], 0.0085, 0.0068, k=0.003),
                  A.ell(legB(s, 2), P[2], A.unit(P[3] - P[2]), (0.0085, 0.0075, 0.0072), k=0.003),   # Ferse
                  A.cone(legB(s, 2), P[2], P[3], 0.0064, 0.0056, k=0.003),
                  A.ell(legB(s, 3), P[3], X, (0.009, 0.008, 0.0055), k=0.003)]                       # Ballen
        fwd = A.unit(P[4] - P[3])
        for t, (ang, ln) in enumerate(((-0.45, 0.048), (0.0, 0.058), (0.45, 0.048), (math.pi, 0.024))):
            d = A.unit(np.array([math.cos(ang) * fwd[0] - math.sin(ang) * fwd[1], math.sin(ang) * fwd[0] + math.cos(ang) * fwd[1], 0.0]))
            p0 = P[3] + Z * 0.002
            p1 = p0 + d * ln * 0.5 + Z * 0.002
            p2 = p0 + d * ln + Z * 0.0
            scale += A.chain(lambda i: legB(s, 3), [p0, p1, p2], [0.0045, 0.0036, 0.0028], k=0.003)
            c1 = p2 + d * 0.009 - Z * 0.0005
            c2 = c1 + A.unit(d * 0.3 - Z) * 0.0075
            talon += A.chain(lambda i: legB(s, 3), [p2, c1, c2], [0.0026, 0.0017, 0.0004], k=0.001)

    # Schwanz: gefiederter Katzenschwanz, am Ende ein Faecher langer Federn
    T = [J[TAIL0 + i] for i in range(4)]
    feather += A.chain(lambda i: B_TAIL0 + min(i, 2), T, [0.021, 0.016, 0.011, 0.0055], k=0.012)
    td = A.unit(T[3] - T[2])
    for t in range(7):
        a = (t - 3) * 0.2
        dd = A.unit(td + A.perp(td, Y) * 0.0 + np.cross(Z, td) * math.sin(a))
        feather.append(A.ell(B_TAIL0 + 2, T[3] + dd * 0.035, dd, (0.04, 0.009, 0.0022), up=Z, k=0.004))

    groups += cloak_groups(J, caste, rng)
    groups += mask_groups(J, caste, rng)
    if caste == 2:
        groups += hat_groups(J) + cane_groups(J)
    groups += loot_groups(J)
    groups += [
        {'name': 'federn', 'fm': 'ent_pg_feather', 'parts': feather, 'disp': feather_disp, 'h': 0.0016, 'cav_scale': 0.006, 'lod_weight': 1.3},
        {'name': 'pinsel', 'fm': 'ent_pg_feather', 'parts': hair, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.4},
        {'name': 'schuppen', 'fm': 'ent_pg_scale', 'parts': scale, 'disp': scale_disp, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 1.2},
        {'name': 'krallen', 'fm': 'ent_pg_talon', 'parts': talon, 'disp': None, 'h': 0.00045, 'cav_scale': 0.001, 'lod_weight': 0.8},
        {'name': 'schnabel', 'fm': 'ent_pg_beak', 'parts': beak, 'disp': beak_disp, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 0.9},
        {'name': 'zaehne', 'fm': 'ent_pg_tooth', 'parts': tooth, 'disp': None, 'h': 0.00035, 'cav_scale': 0.001, 'lod_weight': 0.9},
        {'name': 'rachen', 'fm': 'ent_pg_mouth', 'parts': mouth, 'disp': None, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 0.3},
        {'name': 'augen', 'fm': 'ent_pg_eye', 'parts': eye, 'disp': None, 'h': 0.0006, 'cav_scale': 0.002, 'lod_weight': 0.9},
        {'name': 'pupillen', 'fm': 'ent_pg_beak', 'parts': pupil, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'lod_weight': 0.4},
        {'name': 'haut', 'fm': 'ent_pg_skin', 'parts': skin, 'disp': skin_disp, 'h': 0.0006, 'cav_scale': 0.002, 'lod_weight': 1.0},
    ]
    return groups


def cloak_groups(J, caste, rng):
    """Umhang aus sechs Bahnen (je Strang zwei Knochen), vorn offen; Henchmen und Spaeher: grob, Saum zerrissen; Torwaechterin:
    Samtcape bis fast zum Boden, mit Schulterkragen und hohem Kragen."""
    parts, hood = [], []
    long = 1.12 if caste == 2 else 1.0
    for k in range(KSTRANDS):
        a = STRAND_ANG[k]
        d = np.array([math.cos(a), math.sin(a), 0.0])
        Ak, Mk, Hk = J[cloakJ(k, 0)], J[cloakJ(k, 1)], J[cloakJ(k, 2)]
        if caste == 2:
            Hk = Hk + (Hk - Mk) * 0.25 - Z * 0.02
        # Runde 2: jede Bahn war ein flaches Brett (Pappe) - jetzt drei schmale, gegeneinander versetzte Streifen je Bahn: runde
        # Falten, die nach unten tiefer werden
        tg = np.array([-math.sin(a), math.cos(a), 0.0])
        for j, (off, dep) in enumerate(((-1.0, 0.005), (0.0, -0.004), (1.0, 0.006))):
            o0, o1, o2 = tg * (off * 0.021), tg * (off * 0.036), tg * (off * 0.047 * long)
            r0, r1, r2 = d * (dep * 0.4), d * dep, d * (dep * 1.6)
            dd = A.unit(d + tg * off * 0.25)
            parts.append(A.tube(cloakB(k, 0), Ak + o0 + r0, Mk + o1 + r1, (0.014, 0.0042), (0.022, 0.0042), up=dd, k=0.012))
            parts.append(A.tube(cloakB(k, 1), Mk + o1 + r1, Hk + o2 + r2, (0.022, 0.0042), (0.029 * long, 0.0046), up=dd, k=0.012))
    if caste == 2:  # Schulterkragen (Capelet) und hoher Kragen
        for k in range(KSTRANDS):
            a = STRAND_ANG[k]
            d = np.array([math.cos(a), math.sin(a), 0.0])
            Ak, Mk = J[cloakJ(k, 0)], J[cloakJ(k, 1)]
            tg = np.array([-math.sin(a), math.cos(a), 0.0])
            for off in (-0.6, 0.6):
                parts.append(A.tube(cloakB(k, 0), Ak + d * 0.008 + Z * 0.004 + tg * (off * 0.018), Ak + (Mk - Ak) * 0.55 + d * 0.022 + tg * (off * 0.03),
                                    (0.022, 0.0045), (0.032, 0.0045), up=d, k=0.01))
        hood.append(sdf.Part('torus', B_CHEST, k=0.006, c=np.array([0.0, 0, 0.475]), R=sdf.frame_from(Z, X), major=0.045, minor=0.008, arc=1.9))
        hood.append(A.ell(B_CHEST, (-0.035, 0, 0.49), A.unit((-0.3, 0, 1.0)), (0.035, 0.05, 0.006), up=-X, k=0.008))
    else:  # zurueckgeschlagene Kapuze auf den Schultern
        hood.append(A.ell(B_CHEST, (-0.018, 0, 0.452), X, (0.04, 0.06, 0.024), k=0.0))
        hood.append(sdf.Part('torus', B_CHEST, k=0.01, c=np.array([0.015, 0, 0.45]), R=sdf.frame_from(Z, X), major=0.05, minor=0.012, arc=2.3))
    fm = 'ent_pg_cape' if caste == 2 else 'ent_pg_cloak'
    disp = cape_disp if caste == 2 else cloak_disp
    return [{'name': 'umhang', 'fm': fm, 'parts': parts + hood, 'disp': disp, 'h': 0.0017, 'cav_scale': 0.008, 'lod_weight': 1.1, 'wsoft': 0.01}]


def mask_groups(J, caste, rng):
    """Pestarztmaske am Maskenknochen (Scharnier an der Stirn): Gesichtsschale (hinten offen) und langer Schnabel, der den
    echten Schnabel ganz umschliesst."""
    m, deco, glass = [], [], []
    shell = [
        A.ell(B_MASK, (0.128, 0, 0.515), X, (0.031, 0.052, 0.061), k=0.0),
        A.ell(B_MASK, (0.142, 0, 0.47), X, (0.03, 0.04, 0.03), k=0.02),  # Kinnschale
    ]
    # Runde 1: dick und knollig wie eine Karotte - jetzt schlank, hoeher als breit, leicht gebogen (der echte Schnabel und die
    # Zaehne bleiben ganz darin)
    tip = np.array([0.33, 0, 0.449])
    bk = [np.array([0.145, 0, 0.505]), np.array([0.235, 0, 0.477]), tip]
    m += shell + [A.tube(B_MASK, bk[0], bk[1], (0.03, 0.04), (0.016, 0.026), k=0.02),
                  A.tube(B_MASK, bk[1], tip, (0.016, 0.026), (0.003, 0.0045), k=0.01),
                  A.cone(B_MASK, tip, tip + np.array([0.008, 0, -0.008]), 0.0035, 0.0008, k=0.004)]
    m.append(A.ell(B_MASK, (0.113, 0, 0.515), X, (0.036, 0.046, 0.054), op='sub', k=0.004))  # hinten offen (Schale)
    if caste == 0:  # glatt, ohne Merkmale - nur zwei schmale Schlitze, durch die die Augen schimmern
        for g in (1.0, -1.0):
            m.append(A.ell(B_MASK, (0.153, g * 0.028, 0.532), A.unit((1.0, g * 0.3, 0.0)), (0.012, 0.0085, 0.0045), op='sub', k=0.002))
        fm, disp = 'ent_pg_mask', mask_disp
    elif caste == 1:  # dunkles Leder, runde Glasaugen in Messingfassung, Nieten
        for g in (1.0, -1.0):
            ax = A.unit((1.0, g * 0.28, 0.05))
            c = np.array([0.151, g * 0.03, 0.535])
            m.append(A.ell(B_MASK, c, ax, (0.014, 0.0155, 0.0155), op='sub', k=0.002))
            glass.append(A.ell(B_MASK, c + ax * 0.002, ax, (0.0035, 0.0152, 0.0152), k=0.0))
            deco.append(sdf.Part('torus', B_MASK, k=0.002, c=c + ax * 0.0035, R=sdf.frame_from(ax, Z), major=0.0158, minor=0.0026))
            for t in range(4):  # Nieten am Schnabelansatz
                p = np.array([0.168 + 0.012 * t, g * (0.031 - 0.004 * t), 0.5 - 0.004 * t])
                deco.append(A.ell(B_MASK, p, Y * g, (0.0022, 0.0022, 0.0022), k=0.0))
        fm, disp = 'ent_pg_leather', leather_disp
    else:  # Torwaechterin: roter Lack, Messing: Ringe um den Schnabel, gefasste Augenoeffnungen, Perlen auf dem First, Ranken
        for g in (1.0, -1.0):
            ax = A.unit((1.0, g * 0.3, 0.05))
            c = np.array([0.151, g * 0.029, 0.534])
            m.append(A.ell(B_MASK, c, ax, (0.012, 0.012, 0.0095), op='sub', k=0.002))
            deco.append(sdf.Part('torus', B_MASK, k=0.002, c=c + ax * 0.004, R=sdf.frame_from(ax, Z), major=0.0128, minor=0.0024))
            for t in range(5):  # Ranke ueber der Wange
                a = 0.6 + t * 0.45
                p = np.array([0.15 - 0.004 * t, g * (0.03 + 0.016 * math.cos(a)), 0.515 - 0.018 * math.sin(a)])
                deco.append(A.ell(B_MASK, p, X, (0.004, 0.0028, 0.0028), k=0.002))
        for t, u in enumerate((0.25, 0.5, 0.72)):
            p = bk[0] + (tip - bk[0]) * u
            r = 0.045 + (0.0065 - 0.045) * u
            deco.append(sdf.Part('torus', B_MASK, k=0.002, c=p, R=sdf.frame_from(A.unit(tip - bk[0]), Z), major=r * 0.98, minor=0.0024))
        for t in range(9):  # Perlen auf dem First
            u = 0.06 + t * 0.1
            p = bk[0] + (tip - bk[0]) * u
            r = 0.045 + (0.0065 - 0.045) * u
            deco.append(A.ell(B_MASK, p + Z * (r * 0.92), X, (0.0028, 0.0028, 0.0028), k=0.0))
        fm, disp = 'ent_pg_ornate', ornate_disp
    g = [{'name': 'maske', 'fm': fm, 'parts': m, 'disp': disp, 'h': 0.0009, 'cav_scale': 0.003, 'lod_weight': 1.2}]
    if deco:
        g.append({'name': 'messing', 'fm': 'ent_pg_brass', 'parts': deco, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.5})
    if glass:
        g.append({'name': 'glas', 'fm': 'ent_pg_glass', 'parts': glass, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.4})
    return g


def hat_groups(J):
    """Breitkrempiger Hut der Torwaechterin (am Kopf), abgegriffener Filz; Schlitze in der Krempe fuer die Ohren."""
    parts = [
        A.ell(B_HEAD, (0.072, 0, 0.592), X, (0.118, 0.112, 0.0055), k=0.0),
        A.cone(B_HEAD, (0.068, 0, 0.588), (0.062, 0, 0.66), 0.038, 0.032, k=0.008),
        A.ell(B_HEAD, (0.062, 0, 0.662), X, (0.02, 0.012, 0.006), op='sub', k=0.004),  # Delle oben
    ]
    for g in (1.0, -1.0):
        parts.append(A.ell(B_HEAD, (0.055, g * 0.052, 0.592), A.unit((-0.2, g * 0.3, 1.0)), (0.02, 0.008, 0.012), op='sub', k=0.002))
    band = [sdf.Part('torus', B_HEAD, k=0.002, c=np.array([0.067, 0, 0.6]), R=sdf.frame_from(Z, X), major=0.0375, minor=0.0035)]
    return [{'name': 'hut', 'fm': 'ent_pg_hat', 'parts': parts, 'disp': hat_disp, 'h': 0.0011, 'cav_scale': 0.004, 'lod_weight': 0.7},
            {'name': 'hutband', 'fm': 'ent_pg_cape', 'parts': band, 'disp': None, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 0.2}]


def cane_groups(J):
    """Silberstock mit Sanduhr oben (die Seite: "silver cane tipped with an hourglass")."""
    a, b = J[CANE_A], J[CANE_B]
    u = A.unit(b - a)
    q = A.perp(u, X)
    w = np.cross(u, q)
    silver = [A.cone(B_CANE, a, a + u * 0.014, 0.0038, 0.006, k=0.0),
              A.cone(B_CANE, a + u * 0.012, b - u * 0.05, 0.0052, 0.0056, k=0.003),
              sdf.Part('torus', B_CANE, k=0.002, c=b - u * 0.15, R=sdf.frame_from(u, q), major=0.006, minor=0.002),
              sdf.Part('torus', B_CANE, k=0.002, c=b - u * 0.053, R=sdf.frame_from(u, q), major=0.007, minor=0.0025)]
    hc = b - u * 0.024
    glass, sand = [], []
    for sgn in (1.0, -1.0):
        silver.append(A.ell(B_CANE, hc + u * (sgn * 0.024), u, (0.0028, 0.0135, 0.0135), up=q, k=0.0))  # Deckel
        glass.append(A.ell(B_CANE, hc + u * (sgn * 0.0115), u, (0.0115, 0.0098, 0.0098), up=q, k=0.0))
    glass.append(A.cone(B_CANE, hc - u * 0.004, hc + u * 0.004, 0.003, 0.003, k=0.004))
    for t in range(3):  # drei Saeulen zwischen den Deckeln
        ang = t * 2.094
        off = (q * math.cos(ang) + w * math.sin(ang)) * 0.0118
        silver.append(A.cone(B_CANE, hc - u * 0.023 + off, hc + u * 0.023 + off, 0.0013, 0.0013, k=0.0))
    sand.append(A.ell(B_CANE, hc - u * 0.015, u, (0.006, 0.0078, 0.0078), up=q, k=0.0))
    sand.append(A.cone(B_CANE, hc - u * 0.001, hc - u * 0.012, 0.0006, 0.002, k=0.002))
    return [{'name': 'stock', 'fm': 'ent_pg_silver', 'parts': silver, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.5},
            {'name': 'sanduhrglas', 'fm': 'ent_pg_glass', 'parts': glass, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.3},
            {'name': 'sand', 'fm': 'ent_pg_sand', 'parts': sand, 'disp': None, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.2}]


def loot_groups(J):
    """Beute in der linken Klaue: eine Flasche (meist Mandelwasser) - verschwindet im Umhang."""
    a, b = J[ITEM_A], J[ITEM_B]
    u = A.unit(b - a)
    body = [A.cone(B_ITEM, a, a + u * 0.075, 0.019, 0.019, k=0.0), A.cone(B_ITEM, a + u * 0.075, a + u * 0.098, 0.019, 0.0085, k=0.006)]
    cap = [A.cone(B_ITEM, a + u * 0.096, a + u * 0.11, 0.009, 0.009, k=0.0)]
    return [{'name': 'beute', 'fm': 'ent_pg_loot', 'parts': body, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002, 'lod_weight': 0.2},
            {'name': 'beutedeckel', 'fm': 'ent_pg_beak', 'parts': cap, 'disp': None, 'h': 0.0006, 'cav_scale': 0.001, 'lod_weight': 0.1}]


# --- Portal -----------------------------------------------------------------------------------------------------------
def portal(J, bones):
    """Aufrechtes Oval ueber dem Boden (Mitte PORT_A), schwarz und glaenzend, Rand ausgefranst und gluehend wie rieselnder Sand;
    sechs Sandstroeme kreisen am Rand (eigene Knochen)."""
    check(bones)
    J = np.asarray(J, dtype=float)
    rng = np.random.default_rng(2599)
    c = J[PORT_A]
    # Runde 1: ein regelmaessiger Feuerring - jetzt ein Riss: mandelfoermig (oben und unten spitz), Rand unregelmaessig
    def edge(a):
        w = PORTAL_W * 0.5 * (1.0 - 0.42 * abs(math.sin(a)) ** 2.2) * (1.0 + 0.06 * math.sin(a * 5.0 + 1.3))
        return c + np.array([0.0, math.cos(a) * w, math.sin(a) * PORTAL_H * 0.5 * (1.0 + 0.03 * math.sin(a * 7.0))])
    void = [A.ell(B_PORTAL, c + Z * (PORTAL_H * 0.25 * t), X, (0.022, PORTAL_W * 0.5 * (0.92 - 0.42 * abs(t) ** 1.6), PORTAL_H * 0.3), k=0.06)
            for t in (-0.6, 0.0, 0.6)]
    rim = []
    n = 44
    pts = [edge(2 * math.pi * i / n) for i in range(n + 1)]
    for i in range(n):
        rim.append(A.cone(B_PORTAL, pts[i], pts[i + 1], 0.005 + 0.009 * rng.random(), 0.005 + 0.009 * rng.random(), k=0.006))
        if i % 2 == 0:  # Splitter, die nach aussen stehen
            o = A.unit(pts[i] - c)
            rim.append(A.cone(B_PORTAL, pts[i], pts[i] + A.unit(o + rng.normal(0, 0.4, 3) * np.array([0.3, 1, 1])) * (0.02 + 0.05 * rng.random()), 0.006, 0.0008, k=0.004))
    sand = []
    for k in range(KSWIRLS):
        p0, p1 = J[swirlJ(k, 0)], J[swirlJ(k, 1)]
        for t in range(12):
            u = t / 11.0
            p = p0 + (p1 - p0) * u + rng.normal(0, 0.012, 3) * np.array([0.6, 1, 1])
            sand.append(A.ell(B_SWIRL0 + k, p, A.unit(p1 - p0), (0.006 + 0.008 * math.sin(math.pi * u), 0.0035, 0.0035), k=0.0))
    return [{'name': 'leere', 'fm': 'ent_pg_void', 'parts': void, 'disp': void_disp, 'h': 0.004, 'cav_scale': 0.01, 'lod_weight': 0.8},
            {'name': 'rand', 'fm': 'ent_pg_rim', 'parts': rim, 'disp': rim_disp, 'h': 0.0022, 'cav_scale': 0.005, 'lod_weight': 1.0},
            {'name': 'sandstrom', 'fm': 'ent_pg_sand', 'parts': sand, 'disp': None, 'h': 0.0012, 'cav_scale': 0.002, 'lod_weight': 0.5}]
