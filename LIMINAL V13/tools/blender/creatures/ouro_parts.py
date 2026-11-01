"""V13.3: Ouroboros-Arbeiter (KREATUR_OuroborosArbeiter.md) - gemeinsame Teile der vier Modelle im Skelett 'ent_ouroboros'
(anim/creature.hpp, namespace ob): Arbeiter (schmal: ouroboros.py, schwer: ouroboros_heavy.py), Mauer (ouroboros_wall.py),
Fahrgeruest (ouroboros_scaffold.py).

Wiki (Wikidot level-1, Sektor Ouroboros; backrooms-remodeling-co): schattenhafte Gestalten im Overall; pechschwarze Humanoide,
das Gesicht fast ohne Zuege (kein Mund, keine Nase), grosse weisse Kreisaugen; schwarze Gummistiefel, Guertel mit
"Reality Fresheners", Barett mit Abzeichen aus Kupfer (unterster Rang). Eigene Anatomie (Interpretation): ueberlange Arme mit
einem zweiten Gelenk im Unterarm, langer, vorgeneigter Hals, kleiner eifoermiger Kopf.
"""
import math
import os

import numpy as np

import anat as A
import sdf

X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])

# Gelenke und Knochen (wie anim/creature.hpp, ob)
ROOT, PELVIS, BELLY, CHEST, NECK, HEAD, HEADTOP, ARM0 = 0, 1, 2, 3, 4, 5, 6, 7
LEG0 = ARM0 + 10
TOOL0 = LEG0 + 8
KTOOLS, KWISPS, KSPARKS, KCOURSES, KLAY, KDEBRIS = 6, 4, 6, 7, 6, 4
WISP0 = TOOL0 + KTOOLS * 2
SPARK0 = WISP0 + KWISPS * 3
COURSE0 = SPARK0 + KSPARKS * 2
LAY0 = COURSE0 + KCOURSES * 2
DEB0 = LAY0 + KLAY * 2
TOWER_A = DEB0 + KDEBRIS * 2
TOWER_B = TOWER_A + 1
B_ROOT, B_PELVIS, B_BELLY, B_CHEST, B_NECK, B_HEAD, B_ARM0 = 0, 1, 2, 3, 4, 5, 6
B_LEG0 = B_ARM0 + 8
B_TOOL0 = B_LEG0 + 6
B_WISP0 = B_TOOL0 + KTOOLS
B_SPARK0 = B_WISP0 + KWISPS * 2
B_COURSE0 = B_SPARK0 + KSPARKS
B_LAY0 = B_COURSE0 + KCOURSES
B_DEB0 = B_LAY0 + KLAY
B_TOWER = B_DEB0 + KDEBRIS
T_HAMMER, T_TROWEL, T_BRICK, T_TORCH, T_JACK, T_BAR = range(6)
BLOCK_L, COURSE_H, WALL_T = 0.375, 0.25, 0.24
WALL_L = BLOCK_L * KLAY
TOWER_H, TOWER_D, TOWER_W = 3.4, 0.9, 1.6
# abgesperrte Flaechen (wie presence_ouroboros.cpp)
WALL_ZONE = (-1.4, 1.6, 2.35)
TOW_ZONE = (-0.8, 4.25, 1.05)
PAL = (0.95, 1.85)


def armJ(s, i): return ARM0 + s * 5 + i
def armB(s, i): return B_ARM0 + s * 4 + i
def legJ(s, i): return LEG0 + s * 4 + i
def legB(s, i): return B_LEG0 + s * 3 + i
def toolJ(t, i): return TOOL0 + t * 2 + i
def wispJ(k, i): return WISP0 + k * 3 + i
def wispB(k, i): return B_WISP0 + k * 2 + i
def sparkJ(k, i): return SPARK0 + k * 2 + i
def courseJ(c, i): return COURSE0 + c * 2 + i
def layJ(k, i): return LAY0 + k * 2 + i
def debJ(k, i): return DEB0 + k * 2 + i


def check(bones):
    assert bones[B_TOWER]['a'] == TOWER_A and bones[B_COURSE0]['a'] == COURSE0 and bones[B_TOOL0]['a'] == TOOL0, 'Skelett passt nicht zu ouro_parts.py'


# --- Oberflaechen -----------------------------------------------------------------------------------------------------
def cloth_disp(Q):  # Segeltuch: Koeper, Falten quer (Knie, Saum, Bauch), Knitter
    z = Q[:, 2]
    folds = 0.0016 * np.sin(z * 60.0 + 3.0 * sdf.fbm(Q, 4, 2, seed=901)) * (0.4 + 0.6 * sdf.fbm(Q, 9, 2, seed=902))
    return folds - 0.0011 * sdf.ridged(Q * np.array([1.0, 1.0, 2.5]), 28, 2, seed=903) + 0.0003 * sdf.fbm(Q, 220, 2, seed=904)


def void_disp(Q):  # der Schatten: unruhig, wie geronnener Rauch
    return 0.0022 * sdf.fbm(Q, 18, 3, seed=911) - 0.0012 * sdf.ridged(Q, 45, 2, seed=912)


def rubber_disp(Q):
    return 0.0004 * sdf.fbm(Q, 120, 2, seed=921) - 0.0006 * sdf.ridged(Q * np.array([1, 1, 3.0]), 40, 2, seed=922)


def brick_disp(Q):  # gebrannter Ton: Poren, Kanten abgestossen
    d1, d2 = sdf.worley(Q, 60, seed=931)
    return -0.0012 * np.clip(d2 - d1, 0, 0.3) + 0.0007 * sdf.fbm(Q, 140, 2, seed=932)


def mortar_disp(Q):
    return 0.0018 * sdf.fbm(Q, 70, 3, seed=941) - 0.0012 * sdf.ridged(Q, 110, 2, seed=942)


def steel_disp(Q):
    return 0.00025 * sdf.fbm(Q, 260, 2, seed=951)


def wood_disp(Q):
    return -0.0005 * sdf.ridged(Q * np.array([0.3, 3.0, 3.0]), 90, 2, seed=961)


# --- Arbeiter ---------------------------------------------------------------------------------------------------------
def worker(J, bones, heavy=False):
    check(bones)
    J = np.asarray(J, dtype=float)
    rng = np.random.default_rng(1601 if heavy else 1600)
    k = 1.38 if heavy else 1.0  # Masse
    groups = []
    void, cloth, dust, rubber, steel, eye, card, copper = [], [], [], [], [], [], [], []

    # Rumpf (der Schatten unter dem Overall; sichtbar an Schultern, Seiten, Achseln)
    void += [
        A.ell(B_CHEST, (0.0, 0, 1.44), X, (0.085 * k, 0.14 * k, 0.15), k=0.0),
        A.ell(B_BELLY, (0.0, 0, 1.23), X, (0.075 * k, 0.11 * k, 0.12), k=0.05),
        A.ell(B_PELVIS, (-0.005, 0, 1.02), X, (0.095 * k, 0.14 * k, 0.1), k=0.05),
    ]
    for s, g in ((0, 1.0), (1, -1.0)):
        sh = J[armJ(s, 0)]
        void += [
            A.ell(B_CHEST, (0.0, g * 0.16 * k, 1.55), X, (0.05 * k, 0.065 * k, 0.04), k=0.05),          # Schulter, abfallend
            A.cone(B_CHEST, (0.0, g * 0.03, 1.63), (0.0, g * 0.17 * k, 1.555), 0.035 * k, 0.03 * k, k=0.05),  # Nacken zur Schulter
            A.cone(B_CHEST, (0.03, g * 0.02, 1.56), (0.01, g * 0.16 * k, 1.585), 0.012, 0.01, k=0.02),  # Schluesselbein
            A.ell(B_CHEST, (-0.075 * k, g * 0.08 * k, 1.5), X, (0.02, 0.06, 0.075), k=0.03),            # Schulterblatt
        ]
    if heavy:  # Buckel: der Nacken ein Wulst zwischen den Schultern
        void += [A.ell(B_CHEST, (-0.06, 0, 1.6), X, (0.13, 0.22, 0.11), k=0.06),
                 A.ell(B_CHEST, (0.0, 0, 1.52), X, (0.12, 0.2, 0.12), k=0.05)]
    # Hals: lang, nach vorn geneigt; beim Schweren kurz und dick, der Kopf sitzt tief
    if heavy:
        void.append(A.cone(B_NECK, (0.02, 0, 1.58), (0.1, 0, 1.75), 0.07, 0.05, k=0.05))
    else:
        void.append(A.cone(B_NECK, (0.02, 0, 1.56), (0.1, 0, 1.765), 0.04, 0.027, k=0.04))
        void.append(A.cone(B_NECK, (0.045, 0.012, 1.6), (0.098, 0.006, 1.75), 0.008, 0.006, k=0.012))   # Sehne
        void.append(A.cone(B_NECK, (0.045, -0.012, 1.6), (0.098, -0.006, 1.75), 0.008, 0.006, k=0.012))
    # Kopf: klein, eifoermig, laenglich; das Gesicht eine leere, leicht hohle Flaeche; zwei flache, blasse Kreisscheiben (die
    # "Augen" der Seite) - ungleich gross, eingesunken, starr
    ax = A.unit(J[HEADTOP] - J[HEAD])
    nf = A.unit(np.cross(Y, ax))  # Gesicht: nach vorn (unten)
    hc = J[HEAD] + ax * 0.09
    hs = 0.88 if heavy else 1.0
    void += [
        A.ell(B_HEAD, hc + ax * 0.012, ax, (0.1 * hs, 0.068 * hs, 0.082 * hs), up=nf, k=0.012),
        A.ell(B_HEAD, hc - nf * 0.035 + ax * 0.04, ax, (0.072 * hs, 0.066 * hs, 0.07 * hs), up=nf, k=0.02),  # Hinterkopf
        A.ell(B_HEAD, hc + nf * 0.04 * hs - ax * 0.085, ax, (0.07, 0.042, 0.035), up=nf, k=0.04),          # Kinn haengt, zaeh
        A.ell(B_HEAD, hc + nf * 0.02 * hs - ax * 0.15, ax, (0.03, 0.022, 0.02), up=nf, k=0.03),            # ... zieht einen Faden
        A.ell(B_HEAD, hc + nf * 0.1 * hs + ax * 0.015, nf, (0.03, 0.058, 0.075), up=ax, op='sub', k=0.03),  # Gesicht hohl
    ]
    for g, r, dz in ((1.0, 0.027, 0.022), (-1.0, 0.0215, 0.01)):  # Runde 3: grosse, runde Augen (Remodeling Co)
        ec = hc + nf * 0.066 * hs + ax * dz + Y * (g * 0.034)
        void.append(A.ell(B_HEAD, ec + nf * 0.01, nf, (0.016, r + 0.007, r + 0.008), up=ax, op='sub', k=0.008))  # Mulde
        eye.append(A.ell(B_HEAD, ec - nf * 0.004, nf, (0.003, r, r * 1.08), up=ax, k=0.0))
    # Barett mit Abzeichen (Remodeling Co: Kupfer = unterster Rang), schlaff zur Seite
    bt = hc + ax * (0.062 * hs) - nf * 0.02
    cloth += [
        A.ell(B_HEAD, bt + Y * -0.03 + ax * 0.012 - nf * 0.012, A.unit(ax - Y * 0.25), (0.036, 0.122, 0.112), up=nf, k=0.0),
        sdf.Part('torus', B_HEAD, k=0.012, c=bt - ax * 0.02, R=sdf.frame_from(nf, ax), major=0.072 * hs, minor=0.011),
        A.cone(B_HEAD, bt + ax * 0.035, bt + ax * 0.05, 0.007, 0.004, k=0.004),
    ]
    copper.append(A.ell(B_HEAD, bt - ax * 0.02 + nf * 0.078 * hs + Y * 0.025, nf, (0.004, 0.012, 0.014), up=ax, k=0.0))

    # Arme: Oberarm, Unterarm 1, Unterarm 2 (das zweite Gelenk), lange Hand mit vier Fingern (gekruemmt) und Daumen
    ra = (0.041, 0.031, 0.025, 0.018) if not heavy else (0.068, 0.062, 0.055, 0.04)  # Runde 3: Unterarme duenner (ausgezehrt)
    for s, g in ((0, 1.0), (1, -1.0)):
        P = [J[armJ(s, i)] for i in range(5)]
        void += [
            A.cone(armB(s, 0), P[0] + Z * 0.01, P[1], ra[0], ra[1], k=0.04),
            A.cone(armB(s, 1), P[1], P[2], ra[1] * 0.97, ra[2] * 0.97, k=0.006),
            A.cone(armB(s, 2), P[2], P[3], ra[2] * 1.0, ra[3], k=0.006),
            # das zweite Gelenk: eine Falte quer ueber den Unterarm und ein Hoecker, wo er falsch herum knickt
            A.ell(armB(s, 2), P[2] + X * (ra[2] * 0.85), A.unit(np.cross(P[3] - P[1], Y)), (0.004, ra[2] * 1.2, 0.006), up=X, op='sub', k=0.004),
            A.ell(armB(s, 2), P[2] - X * (ra[2] * 0.7), P[3] - P[1], (0.024, ra[2] * 0.95, ra[2] * 0.8), k=0.012),  # Runde 3: deutlicher
        ]
        # Hand: Handflaeche zum Koerper (Normale -g*y), Finger von der Wurzel bis ueber die Spitze hinaus
        hd = A.unit(P[4] - P[3])
        side = Y * g
        fw = A.unit(np.cross(side, hd))  # vorn (Daumenseite)
        hw = 1.35 if heavy else 1.0
        palm_c = P[3] + hd * 0.05
        void.append(A.box(armB(s, 3), palm_c, hd, (0.05, 0.034 * hw, 0.011 * hw), 0.008, up=side, k=0.012))
        void.append(A.ell(armB(s, 3), P[3], hd, (0.026, 0.028 * hw, 0.02 * hw), up=side, k=0.012))
        root = P[3] + hd * 0.098
        for f in range(4):
            off = (f - 1.5) * 0.017 * hw
            L0 = (0.05, 0.042, 0.032)[0] * (1.0 - 0.12 * abs(f - 1.2)) * (1.25 if not heavy else 0.9)
            p0 = root + fw * off
            curl = 0.35 + 0.15 * f + rng.uniform(-0.08, 0.08)
            d = hd
            pts = [p0]
            lens = [L0, L0 * 0.8, L0 * 0.65]
            for seg in range(3):
                d = A.unit(d * math.cos(curl) - side * math.sin(curl))
                pts.append(pts[-1] + d * lens[seg])
            rr = [0.0085 * hw, 0.0075 * hw, 0.0062 * hw, 0.0045 * hw]
            void += A.chain(lambda i: armB(s, 3), pts, rr, k=0.004)
            for j in (1, 2):  # Knoechel
                void.append(A.ell(armB(s, 3), pts[j], d, (0.009 * hw, 0.0085 * hw, 0.0085 * hw), k=0.003))
        tp = [P[3] + hd * 0.03 + fw * 0.026 * hw]
        td = A.unit(hd * 0.6 + fw * 0.5 - side * 0.3)
        for seg, ln in enumerate((0.04, 0.032, 0.026)):
            td = A.unit(td * math.cos(0.3) - side * math.sin(0.3))
            tp.append(tp[-1] + td * ln * (1.2 if heavy else 1.0))
        void += A.chain(lambda i: armB(s, 3), tp, [0.011 * hw, 0.009 * hw, 0.0075 * hw, 0.0055 * hw], k=0.005)

    # Overall: gerade, schwer haengende Hosenbeine, Latz und Rueckenteil folgen dem Koerper (der Rumpf etwas groesser, vorn ueber
    # dem Latz, hinten ueber dem Rueckenteil und seitlich ueber dem Bund weggeschnitten), Traeger ueber Kreuz, flache Knieschoner
    for s, g in ((0, 1.0), (1, -1.0)):
        H, Kn, An = J[legJ(s, 0)], J[legJ(s, 1)], J[legJ(s, 2)]
        cloth += [
            A.cone(legB(s, 0), H + Z * 0.04 + Y * (g * 0.012), Kn + Z * 0.02, 0.088 * k, 0.075 * k, k=0.06),
            A.cone(legB(s, 1), Kn + Z * 0.02, An + Z * 0.32, 0.075 * k, 0.08 * k, k=0.01),
        ]
        dust.append(A.ell(legB(s, 0), Kn + X * (0.066 * k) + Z * 0.01, Z, (0.08, 0.056 * k, 0.014), up=X, k=0.0))
        # Gummistiefel: weiter Schaft, Fuss, Sohle mit flachem Profil
        rubber += [
            A.cone(legB(s, 1), An + Z * 0.0, An + Z * 0.3, 0.058 * k, 0.065 * k, k=0.02),
            A.ell(legB(s, 2), An + X * 0.07 - Z * 0.035, X, (0.135, 0.054 * k, 0.044), k=0.04),
            A.box(legB(s, 2), An + X * 0.065 - Z * 0.081, X, (0.138, 0.054 * k, 0.007), 0.005, k=0.01),
        ]
        for q in range(9):  # Profil (Querrillen)
            rubber.append(A.box(legB(s, 2), An + X * (-0.065 + q * 0.034) - Z * 0.089, Y, (0.07 * k, 0.005, 0.0025), 0.001, op='sub'))
    cloth += [
        A.ell(B_PELVIS, (-0.015, 0, 0.99), X, (0.105 * k, 0.142 * k, 0.12), k=0.06),        # Gesaess, Schritt
        A.ell(B_BELLY, (0.0, 0, 1.15), X, (0.09 * k, 0.13 * k, 0.13), k=0.06),              # Bund
        A.ell(B_CHEST, (0.008, 0, 1.36), X, (0.098 * k, 0.152 * k, 0.17), k=0.04),          # wird zu Latz und Ruecken
        A.box(B_CHEST, (-0.2, 0, 1.62), X, (0.2, 0.3, 0.29), 0.0, op='sub', k=0.012),        # hinten ueber 1,33 frei
        A.box(B_CHEST, (0.15, 0, 1.75), X, (0.18, 0.3, 0.26), 0.0, op='sub', k=0.012),       # vorn ueber 1,49 frei
        A.box(B_CHEST, (0.0, 0.29, 1.52), X, (0.3, 0.16, 0.27), 0.0, op='sub', k=0.012),     # seitlich ueber dem Bund frei
        A.box(B_CHEST, (0.0, -0.29, 1.52), X, (0.3, 0.16, 0.27), 0.0, op='sub', k=0.012),
        A.box(B_CHEST, (0.1 * k, 0.045, 1.405), A.unit((0.25, 0, 1.0)), (0.05, 0.048, 0.005), 0.003, up=X, k=0.003),  # Brusttasche
    ]
    for s, g in ((0, 1.0), (1, -1.0)):
        pts = [np.array(p) for p in ((0.098 * k, g * 0.1, 1.485), (0.06 * k, g * 0.118, 1.57), (0.0, g * 0.128, 1.618),
                                     (-0.07 * k, g * 0.1, 1.57), (-0.092 * k, g * 0.03, 1.44), (-0.098 * k, -g * 0.07, 1.33))]
        for i in range(len(pts) - 1):
            nrm = A.unit(pts[i] * np.array([1, 1, 0.3]) - np.array([0, 0, 1.2 * 0.3]))
            cloth.append(A.tube(B_CHEST, pts[i], pts[i + 1], (0.021, 0.0045), up=nrm, k=0.003))
        steel.append(A.box(B_CHEST, pts[0] + X * 0.008, A.unit((0.25, 0, 1.0)), (0.02, 0.024, 0.005), 0.003, up=X))  # Schnalle
        steel.append(A.ell(B_PELVIS, (0.02, g * 0.148 * k, 1.1), Y, (0.005, 0.012, 0.012), k=0.0))  # Knopf an der Seite
    # Hammerschlaufe rechts, Oberschenkeltasche links, Lappen aus der Gesaesstasche
    cloth.append(sdf.Part('torus', legB(1, 0), k=0.004, c=np.array([0.0, -0.198 * k, 0.83]), R=sdf.frame_from(Z, Y), major=0.024, minor=0.0045))
    cloth.append(A.box(legB(0, 0), (0.005, 0.192 * k, 0.76), Z, (0.065, 0.006, 0.07), 0.004, up=Y, k=0.004))
    dust += [A.tube(B_PELVIS, (-0.12 * k, -0.07, 0.96), (-0.13 * k, -0.085, 0.84), (0.035, 0.004), up=-X, k=0.0),
             A.tube(B_PELVIS, (-0.13 * k, -0.085, 0.84), (-0.122 * k, -0.08, 0.71), (0.03, 0.004), up=-X, k=0.004)]
    # drei kleine Anhaenger ("Reality Fresheners", Remodeling Co) an einem Ring am linken Seitenknopf
    ring_c = np.array([0.03, 0.152 * k, 1.085])
    steel.append(sdf.Part('torus', B_PELVIS, k=0.0, c=ring_c, R=sdf.frame_from(X, Y), major=0.012, minor=0.0018))
    for q in range(3):
        top = ring_c + np.array([0.012 * (q - 1), 0.004, -0.012])
        hang = top + np.array([0.006 * (q - 1), 0.006, -0.05 - 0.014 * q])
        card.append(A.cone(B_PELVIS, top, hang, 0.0012, 0.0012))
        nrm = A.unit(np.array([0.3 * (q - 1), 1.0, 0.0]))
        for j, (dz, w) in enumerate(((0.0, 0.012), (-0.017, 0.017), (-0.034, 0.022))):  # Baumform
            card.append(A.ell(B_PELVIS, hang + Z * (dz - 0.01), nrm, (0.0018, w, 0.012), up=Z, k=0.003))
        card.append(A.box(B_PELVIS, hang + Z * -0.053, nrm, (0.0018, 0.005, 0.006), 0.001, up=Z, k=0.002))

    # Rauchfahnen (Schatten, duenn auslaufend)
    wisps = []
    for w in range(KWISPS):  # je Fahne drei feine, gewellte Faeden (der Rauchrand duennt sie zu Rauch aus)
        p = [J[wispJ(w, i)] for i in range(3)]
        side = A.perp(p[2] - p[0], X)
        for th in range(3):
            pts = []
            ph = rng.uniform(0, 6.28)
            for i in range(8):
                t = i / 7
                pts.append(p[0] + (p[2] - p[0]) * t * (0.75 + 0.25 * th / 2) + side * (0.03 * math.sin(t * 6.0 + ph) + (th - 1) * 0.04 * t)
                           + X * (0.02 * math.cos(t * 4.0 + ph)))
            # Runde 4: 6,5 mm duenne Faeden lasen sich als Draehte - eine dicke Fahne (2,6 cm) und zwei Schleier (1,1 cm)
            rr = [(0.026 if th == 1 else 0.011) * (1.0 - 0.6 * i / 7) for i in range(8)]
            wisps += A.chain(lambda i, w=w: wispB(w, 0 if i < 4 else 1), pts, rr, k=0.004)
    groups += [
        {'name': 'schatten', 'fm': 'ent_ob_void', 'parts': void, 'disp': void_disp, 'h': 0.0032, 'cav_scale': 0.01, 'lod_weight': 1.4},
        {'name': 'overall', 'fm': 'ent_ob_overall', 'parts': cloth, 'disp': cloth_disp, 'h': 0.0034, 'cav_scale': 0.012, 'lod_weight': 1.3},
        {'name': 'staub', 'fm': 'ent_ob_dust', 'parts': dust, 'disp': cloth_disp, 'h': 0.003, 'lod_weight': 0.4},
        {'name': 'stiefel', 'fm': 'ent_ob_rubber', 'parts': rubber, 'disp': rubber_disp, 'h': 0.003, 'lod_weight': 0.6},
        {'name': 'beschlag', 'fm': 'ent_ob_steel', 'parts': steel, 'disp': None, 'h': 0.0016, 'lod_weight': 0.3},
        {'name': 'augen', 'fm': 'ent_ob_eye', 'parts': eye, 'disp': None, 'h': 0.0012, 'lod_weight': 0.5},
        {'name': 'abzeichen', 'fm': 'ent_ob_rust', 'parts': copper, 'disp': None, 'h': 0.0012, 'lod_weight': 0.2},
        {'name': 'anhaenger', 'fm': 'ent_ob_card', 'parts': card, 'disp': None, 'h': 0.0011, 'lod_weight': 0.3},
        {'name': 'rauch', 'fm': 'ent_ob_void', 'parts': wisps, 'disp': None, 'h': 0.0018, 'merge': 'rauchfahnen', 'lod_weight': 0.3},
    ]
    if not os.environ.get('OURO_NOTOOLS'):  # Vorschau nur des Koerpers
        groups += tools(J)
        groups += sparks(J)
    return groups


def tools(J):
    g = []
    # Hammer: Holzstiel (a Griffende -> b Kopf), Stahlkopf quer (Bahn bei -y, Finne bei +y)
    a, b = J[toolJ(T_HAMMER, 0)], J[toolJ(T_HAMMER, 1)]
    g.append({'name': 'hammerstiel', 'fm': 'ent_ob_wood', 'parts': [A.tube(B_TOOL0 + T_HAMMER, a, b + Z * 0.01, (0.014, 0.011), (0.012, 0.009), up=Y)],
              'disp': wood_disp, 'h': 0.0016, 'lod_weight': 0.25})
    head = [A.box(B_TOOL0 + T_HAMMER, b + Y * 0.012, Y, (0.05, 0.015, 0.016), 0.003, up=X),
            A.cone(B_TOOL0 + T_HAMMER, b - Y * 0.035, b - Y * 0.062, 0.017, 0.015, k=0.004),   # Bahn
            A.box(B_TOOL0 + T_HAMMER, b + Y * 0.07, Y, (0.02, 0.012, 0.004), 0.002, up=Z, k=0.006)]  # Finne
    g.append({'name': 'hammerkopf', 'fm': 'ent_ob_rust', 'parts': head, 'disp': steel_disp, 'h': 0.0014, 'lod_weight': 0.25})
    # Kelle: Griff, gekroepfter Hals, Blatt (flach in der y-z-Ebene)
    a, b = J[toolJ(T_TROWEL, 0)], J[toolJ(T_TROWEL, 1)]
    g.append({'name': 'kellengriff', 'fm': 'ent_ob_wood', 'parts': [A.cone(B_TOOL0 + T_TROWEL, a, b, 0.012, 0.014)], 'disp': wood_disp, 'h': 0.0014, 'lod_weight': 0.2})
    blade = [A.cone(B_TOOL0 + T_TROWEL, b, b + Z * 0.02 + X * 0.025, 0.004, 0.004),
             A.box(B_TOOL0 + T_TROWEL, b + Z * 0.12 + X * 0.028, Z, (0.1, 0.055, 0.0015), 0.0008, up=X, k=0.0),
             A.box(B_TOOL0 + T_TROWEL, b + Z * 0.24 + X * 0.028, A.unit(Z + Y), (0.06, 0.06, 0.004), 0.0, up=X, op='sub'),
             A.box(B_TOOL0 + T_TROWEL, b + Z * 0.24 + X * 0.028, A.unit(Z - Y), (0.06, 0.06, 0.004), 0.0, up=X, op='sub')]
    g.append({'name': 'kellenblatt', 'fm': 'ent_ob_steel', 'parts': blade, 'disp': None, 'h': 0.001, 'lod_weight': 0.2})
    # Stein (Hochlochziegel) zum Tragen
    a, b = J[toolJ(T_BRICK, 0)], J[toolJ(T_BRICK, 1)]
    g.append({'name': 'stein', 'fm': 'ent_ob_brick', 'parts': block_parts(B_TOOL0 + T_BRICK, (a + b) / 2, Y), 'disp': brick_disp, 'h': 0.003, 'lod_weight': 0.3})
    # Brenner: Griff mit zwei Ventilen, Rohr zur Duese (abgewinkelt), Schlauchstueck
    a, b = J[toolJ(T_TORCH, 0)], J[toolJ(T_TORCH, 1)]
    mid = a + (b - a) * 0.6
    torch = [A.cone(B_TOOL0 + T_TORCH, a, mid, 0.016, 0.014),
             A.cone(B_TOOL0 + T_TORCH, mid, b - Z * 0.03 + X * 0.03, 0.006, 0.005, k=0.004),
             A.cone(B_TOOL0 + T_TORCH, b - Z * 0.03 + X * 0.03, b, 0.005, 0.004, k=0.003)]
    for q in range(2):
        torch.append(A.ell(B_TOOL0 + T_TORCH, a + Z * (0.03 + q * 0.03) + X * 0.018, X, (0.008, 0.011, 0.011), k=0.003))
    g.append({'name': 'brenner', 'fm': 'ent_ob_steel', 'parts': torch, 'disp': None, 'h': 0.0012, 'lod_weight': 0.25})
    g.append({'name': 'duese', 'fm': 'ent_ob_rust', 'parts': [A.cone(B_TOOL0 + T_TORCH, b - (b - a) * 0.05 + X * 0.004, b + X * 0.004 + Z * 0.004, 0.0055, 0.004)],
              'disp': None, 'h': 0.001, 'lod_weight': 0.1})
    hose = A.chain(lambda i: B_TOOL0 + T_TORCH, A.bezier(a, a - Z * 0.25 - X * 0.05, a - Z * 0.36 - X * 0.2 + Y * 0.05, 6), [0.009] * 7, k=0.004)
    g.append({'name': 'schlauch', 'fm': 'ent_ob_rubber', 'parts': hose, 'disp': None, 'h': 0.002, 'lod_weight': 0.2})
    # Presslufthammer: Meissel (a = Spitze), Gehaeuse mit Rippen (Lack), T-Griff mit Gummi, Luftschlauch
    a, b = J[toolJ(T_JACK, 0)], J[toolJ(T_JACK, 1)]
    t = B_TOOL0 + T_JACK
    chisel = [A.cone(t, a, a + Z * 0.05, 0.004, 0.014), A.cone(t, a + Z * 0.05, a + Z * 0.31, 0.015, 0.016, k=0.004),
              A.cone(t, a + Z * 0.3, a + Z * 0.34, 0.025, 0.025, k=0.004)]
    g.append({'name': 'meissel', 'fm': 'ent_ob_steel', 'parts': chisel, 'disp': steel_disp, 'h': 0.0018, 'lod_weight': 0.3})
    body = [A.cone(t, a + Z * 0.34, a + Z * 0.72, 0.06, 0.055, k=0.0), A.ell(t, a + Z * 0.72, Z, (0.05, 0.065, 0.065), k=0.02)]
    for q in range(6):
        body.append(sdf.Part('torus', t, k=0.004, c=a + Z * (0.4 + q * 0.05), R=sdf.frame_from(X, Z), major=0.06, minor=0.006))
    g.append({'name': 'gehaeuse', 'fm': 'ent_ob_paint', 'parts': body, 'disp': steel_disp, 'h': 0.002, 'lod_weight': 0.4})
    grip = [A.cone(t, b - Z * 0.07, b - Z * 0.02, 0.022, 0.02), A.cone(t, b - Y * 0.24, b + Y * 0.24, 0.014, 0.014, k=0.01)]
    g.append({'name': 'griffrohr', 'fm': 'ent_ob_rust', 'parts': grip, 'disp': None, 'h': 0.0018, 'lod_weight': 0.25})
    rub = [A.cone(t, b - Y * 0.25, b - Y * 0.13, 0.019, 0.019), A.cone(t, b + Y * 0.13, b + Y * 0.25, 0.019, 0.019)]
    rub += A.chain(lambda i: t, A.bezier(a + Z * 0.62 + X * 0.06, a + Z * 0.55 + X * 0.25, a + Z * 0.05 + X * 0.35 + Y * 0.1, 6), [0.012] * 7, k=0.004)
    g.append({'name': 'gummi', 'fm': 'ent_ob_rubber', 'parts': rub, 'disp': rubber_disp, 'h': 0.002, 'lod_weight': 0.25})
    # Brechstange: Sechskant, Spitze abgeflacht und gekruemmt (Klaue)
    a, b = J[toolJ(T_BAR, 0)], J[toolJ(T_BAR, 1)]
    t = B_TOOL0 + T_BAR
    bar = [A.cone(t, a + Z * 0.06 + X * 0.03, b, 0.013, 0.013), A.cone(t, a + Z * 0.06 + X * 0.03, a + Z * 0.015 + X * 0.012, 0.012, 0.009, k=0.006),
           A.box(t, a + Z * 0.008, A.unit(X * 0.4 - Z), (0.018, 0.017, 0.004), 0.002, up=X, k=0.006)]
    g.append({'name': 'stange', 'fm': 'ent_ob_rust', 'parts': bar, 'disp': steel_disp, 'h': 0.0018, 'lod_weight': 0.25})
    return g


def sparks(J):
    parts = []
    for q in range(KSPARKS):
        a, b = J[sparkJ(q, 0)], J[sparkJ(q, 1)]
        parts.append(A.cone(B_SPARK0 + q, a, b, 0.0022, 0.004))
    return [{'name': f'funke', 'fm': 'ent_ob_spark', 'parts': parts, 'disp': None, 'h': 0.0012, 'lod_weight': 0.15}]


# --- Steine, Mauer --------------------------------------------------------------------------------------------------------
def block_parts(bone, c, along, holes=True, rng=None):
    """Hochlochziegel 36,5 x 24 x 24,9 cm um die Mitte c (lange Seite entlang 'along'); oben ein Lochbild (flache Gruben)."""
    c = np.asarray(c, dtype=float)
    al = A.unit(along)
    cr = A.unit(np.cross(Z, al))
    parts = [A.box(bone, c, al, (0.1825, 0.12, 0.1245), 0.004, up=Z)]
    if holes:
        for i in range(6):
            for j in range(4):
                p = c + al * (-0.14 + i * 0.056) + cr * (-0.075 + j * 0.05) + Z * 0.124
                parts.append(A.box(bone, p, al, (0.018, 0.016, 0.02), 0.002, up=Z, op='sub'))
        for sgn in (-1.0, 1.0):  # Griffloecher an den Stirnseiten? - hier: Nut und Feder an den Stossseiten
            parts.append(A.box(bone, c + al * (sgn * 0.183), cr, (0.035, 0.008, 0.1), 0.002, up=Z, op='sub'))
    if rng is not None:  # abgestossene Kanten
        for _ in range(3):
            e = c + al * rng.choice([-0.18, 0.18]) + cr * rng.choice([-0.118, 0.118]) + Z * rng.uniform(-0.1, 0.12)
            parts.append(A.ell(bone, e, X, (0.02, 0.02, 0.025), op='sub', k=0.004))
    return parts


def course_y(k, c):
    return -WALL_L * 0.5 + (k + 0.5) * BLOCK_L + (BLOCK_L * 0.5 if c % 2 else 0.0)


def wall(J, bones):
    check(bones)
    rng = np.random.default_rng(1610)
    groups = []
    for c in range(KCOURSES):
        b = B_COURSE0 + c
        z0 = c * COURSE_H
        blocks = []
        for k in range(KLAY):
            blocks += block_parts(b, (0.0, course_y(k, c), z0 + 0.01 + 0.1245), Y, rng=rng)
        groups.append({'name': f'lage{c}', 'fm': 'ent_ob_brick', 'parts': blocks, 'disp': brick_disp, 'h': 0.0045, 'merge': 'ziegel', 'lod_weight': 1.0})
        # Lagerfuge: Moertelbett, seitlich herausgequollen; Stossfugen
        mor = [A.box(b, (0.0, (course_y(0, c) + course_y(KLAY - 1, c)) / 2, z0 + 0.005), Y, (WALL_L * 0.5 - 0.01, 0.118, 0.006), 0.002, up=Z)]
        for k in range(KLAY):
            y = course_y(k, c)
            for sgn in (-1.0, 1.0):
                if rng.random() < 0.55:
                    mor.append(A.ell(b, (sgn * 0.118, y + rng.uniform(-0.15, 0.15), z0 + 0.008), Y, (rng.uniform(0.04, 0.12), 0.012, 0.006), k=0.004))
            if k > 0:
                mor.append(A.box(b, (0.0, y - BLOCK_L * 0.5, z0 + 0.13), Z, (0.115, 0.004, 0.11), 0.002, up=X, k=0.0))
        groups.append({'name': f'fuge{c}', 'fm': 'ent_concrete', 'parts': mor, 'disp': mortar_disp, 'h': 0.003, 'merge': 'moertel', 'lod_weight': 0.4})
    for k in range(KLAY):
        a, b2 = J[layJ(k, 0)], J[layJ(k, 1)]
        parts = block_parts(B_LAY0 + k, (a + b2) / 2 + Z * 0.005, Y, rng=rng)
        parts.append(A.box(B_LAY0 + k, a + Z * 0.004, Y, (0.18, 0.118, 0.005), 0.002, up=Z, k=0.003))  # Moertel darunter
        groups.append({'name': f'los{k}', 'fm': 'ent_ob_brick', 'parts': parts, 'disp': brick_disp, 'h': 0.0045, 'merge': 'ziegel', 'lod_weight': 0.5})
    # Brocken: gebrochene Stuecke (Teile eines Steins mit schraegen Bruchflaechen)
    for k in range(KDEBRIS):
        a = J[debJ(k, 0)]
        c = a + Z * 0.06
        sz = rng.uniform(0.07, 0.11)
        parts = [A.box(B_DEB0 + k, c, A.unit(rng.normal(size=3)), (sz, sz * 0.75, sz * 0.6), 0.004, up=Z)]
        for _ in range(3):
            n = A.unit(rng.normal(size=3))
            parts.append(A.box(B_DEB0 + k, c + n * sz * rng.uniform(0.7, 1.0), n, (sz * 0.5, sz * 1.5, sz * 1.5), 0.0, up=A.perp(n), op='sub'))
        for i in range(3):
            parts.append(A.box(B_DEB0 + k, c + rng.uniform(-0.03, 0.03, 3), Z, (0.015, 0.013, 0.05), 0.001, op='sub'))
        groups.append({'name': f'brocken{k}', 'fm': 'ent_ob_brick', 'parts': parts, 'disp': brick_disp, 'h': 0.003, 'merge': 'ziegel', 'lod_weight': 0.3})
    groups += pallet_groups(rng)
    groups += tub_groups(rng)
    groups += zone_groups(WALL_ZONE[0], WALL_ZONE[1], -WALL_ZONE[2], WALL_ZONE[2], beacon=(WALL_ZONE[1], -WALL_ZONE[2]), mid_x=False, rng=rng)
    return groups


def pallet_groups(rng):
    """Europalette (1,2 x 0,8 m) bei PAL mit zwei Lagen Steinen und ein paar oben drauf."""
    px, py = PAL
    wood, bricks = [], []
    for i in range(3):  # Bodenbretter
        wood.append(A.box(B_ROOT, (px - 0.52 + i * 0.52, py, 0.011), Y, (0.4, 0.05, 0.011), 0.002, up=Z))
    for i in range(3):
        for j in range(3):  # Kloetze
            wood.append(A.box(B_ROOT, (px - 0.52 + i * 0.52, py - 0.335 + j * 0.335, 0.061), X, (0.05, 0.05, 0.039), 0.003, up=Z))
    for j in range(3):
        wood.append(A.box(B_ROOT, (px, py - 0.335 + j * 0.335, 0.111), X, (0.6, 0.05, 0.011), 0.002, up=Z))
    for i in range(5):
        wood.append(A.box(B_ROOT, (px - 0.5 + i * 0.25, py, 0.133), Y, (0.4, 0.05, 0.011), 0.002, up=Z))
    for lay in range(2):
        for i in range(3):
            for j in range(3):
                c = (px - 0.38 + i * 0.38, py - 0.25 + j * 0.25, 0.144 + 0.1245 + lay * 0.249)
                bricks += block_parts(B_ROOT, c, X, holes=(lay == 1), rng=rng if lay == 1 else None)
    for i, j in ((0, 0), (1, 1), (0, 2), (2, 1)):
        c = (px - 0.38 + i * 0.38, py - 0.25 + j * 0.25, 0.144 + 0.1245 + 2 * 0.249)
        bricks += block_parts(B_ROOT, c, X, rng=rng)
    return [{'name': 'palette', 'fm': 'ent_ob_wood', 'parts': wood, 'disp': wood_disp, 'h': 0.004, 'lod_weight': 0.4},
            {'name': 'stapel', 'fm': 'ent_ob_brick', 'parts': bricks, 'disp': brick_disp, 'h': 0.005, 'merge': 'ziegel', 'lod_weight': 0.6}]


def tub_groups(rng):
    c = np.array([1.2, -1.15, 0.0])
    tub = [A.cone(B_ROOT, c + Z * 0.01, c + Z * 0.3, 0.24, 0.29), A.cone(B_ROOT, c + Z * 0.04, c + Z * 0.32, 0.22, 0.27, op='sub', k=0.01)]
    tub.append(sdf.Part('torus', B_ROOT, k=0.01, c=c + Z * 0.305, R=sdf.frame_from(X, Z), major=0.285, minor=0.016))
    mort = [A.cone(B_ROOT, c + Z * 0.05, c + Z * 0.2, 0.235, 0.255)]
    for _ in range(5):
        mort.append(A.ell(B_ROOT, c + np.array([rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), 0.2]), X, (0.07, 0.06, 0.03), k=0.03))
    return [{'name': 'kuebel', 'fm': 'ent_ob_rubber', 'parts': tub, 'disp': rubber_disp, 'h': 0.004, 'lod_weight': 0.3},
            {'name': 'moertel_kuebel', 'fm': 'ent_concrete', 'parts': mort, 'disp': mortar_disp, 'h': 0.004, 'merge': 'moertel', 'lod_weight': 0.2}]


def zone_groups(x0, x1, y0, y1, beacon, mid_x, rng, bone=B_ROOT):
    """Absperrung: Fusspfosten (Gummifuss, rot-weisse Stange) an den Ecken und Laengsseiten, dazwischen Flatterband (rot/weiss,
    leicht durchhaengend, ein loses Ende); Warnleuchte auf einem Pfosten. Band und Pfosten haengen an der Wurzel."""
    posts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    ring = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    long_y = abs(y1 - y0) > abs(x1 - x0)
    pts = []
    for i in range(4):
        a, b = ring[i], ring[(i + 1) % 4]
        pts.append(a)
        ln = math.hypot(b[0] - a[0], b[1] - a[1])
        if ln > 3.2:
            pts.append(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2))
    groups = []
    post_w, post_r, feet = [], [], []
    for (x, y) in pts:
        feet.append(A.box(bone, (x, y, 0.022), X, (0.13, 0.13, 0.02), 0.012))
        feet.append(A.ell(bone, (x, y, 0.045), X, (0.1, 0.1, 0.03), k=0.02))
        feet.append(A.cone(bone, (x, y, 0.05), (x, y, 0.12), 0.034, 0.024, k=0.02))
        for q in range(4):  # rot-weisse Abschnitte (25 cm)
            seg = A.cone(bone, (x, y, 0.1 + q * 0.25), (x, y, 0.1 + (q + 1) * 0.25), 0.02, 0.02)
            (post_r if q % 2 == 0 else post_w).append(seg)
    groups += [{'name': 'pfostenfuss', 'fm': 'ent_ob_rubber', 'parts': feet, 'disp': rubber_disp, 'h': 0.004, 'merge': 'fuesse', 'lod_weight': 0.2},
               {'name': 'pfosten_r', 'fm': 'ent_ob_tape_r', 'parts': post_r, 'disp': None, 'h': 0.003, 'merge': 'band_r', 'lod_weight': 0.15},
               {'name': 'pfosten_w', 'fm': 'ent_ob_tape_w', 'parts': post_w, 'disp': None, 'h': 0.003, 'merge': 'band_w', 'lod_weight': 0.15}]
    # Flatterband je Spanne: Abschnitte 0,3 m im Wechsel, durchhaengend
    for i in range(len(pts)):
        a, b = np.array([*pts[i], 0.0]), np.array([*pts[(i + 1) % len(pts)], 0.0])
        ln = float(np.linalg.norm(b - a))
        d = (b - a) / ln
        nseg = max(2, int(ln / 0.3))
        red, white = [], []
        torn = rng.random() < 0.18
        for q in range(nseg):
            if torn and q > nseg // 2:
                break
            t0, t1 = q / nseg, (q + 1) / nseg
            z0 = 0.93 - 0.07 * math.sin(math.pi * t0)
            z1 = 0.93 - 0.07 * math.sin(math.pi * t1)
            p0, p1 = a + d * (ln * t0) + Z * z0, a + d * (ln * t1) + Z * z1
            tw = 0.25 * math.sin(t0 * 7.0 + i)  # verdreht
            up = A.unit(Z * math.cos(tw) + np.cross(Z, d) * math.sin(tw))
            seg = A.box(bone, (p0 + p1) / 2, p1 - p0, (np.linalg.norm(p1 - p0) / 2 + 0.002, 0.0018, 0.036), 0.0005, up=up)
            (red if q % 2 == 0 else white).append(seg)
        if torn:  # loses Ende haengt herab
            q = nseg // 2 + 1
            p0 = a + d * (ln * q / nseg) + Z * (0.93 - 0.07 * math.sin(math.pi * q / nseg))
            for j in range(4):
                p1 = p0 - Z * 0.1 + d * (0.025 * math.sin(j * 1.7))
                (red if j % 2 == 0 else white).append(A.box(bone, (p0 + p1) / 2, p1 - p0, (0.052, 0.0018, 0.036), 0.0005, up=np.cross(Z, d)))
                p0 = p1
        if red:
            groups.append({'name': f'band_r{i}', 'fm': 'ent_ob_tape_r', 'parts': red, 'disp': None, 'h': 0.0016, 'merge': 'band_r', 'lod_weight': 0.25})
        if white:
            groups.append({'name': f'band_w{i}', 'fm': 'ent_ob_tape_w', 'parts': white, 'disp': None, 'h': 0.0016, 'merge': 'band_w', 'lod_weight': 0.25})
    # Warnleuchte (Batterieleuchte, gelbe Kuppel) auf dem Pfosten bei 'beacon'
    bx, by = beacon
    groups.append({'name': 'leuchtgehaeuse', 'fm': 'ent_ob_rubber', 'parts': [A.box(bone, (bx, by, 1.02), X, (0.06, 0.07, 0.035), 0.01, up=Z)],
                   'disp': None, 'h': 0.003, 'lod_weight': 0.1})
    groups.append({'name': 'leuchte', 'fm': 'ent_ob_beacon', 'parts': [A.ell(bone, (bx, by, 1.075), X, (0.06, 0.06, 0.05), k=0.0),
                                                                       A.box(bone, (bx, by, 1.04), X, (0.07, 0.07, 0.03), 0.0, op='sub')],
                   'disp': None, 'h': 0.002, 'lod_weight': 0.15})
    return groups


# --- Fahrgeruest -----------------------------------------------------------------------------------------------------------
def scaffold(J, bones):
    check(bones)
    rng = np.random.default_rng(1620)
    t = B_TOWER
    hx, hy = 0.4, 0.75  # Stiele an den Ecken (x = +-0,4, y = +-0,75)
    tubes, wood, wheels, steel2, rust = [], [], [], [], []
    R = 0.024
    for sx in (-1, 1):
        for sy in (-1, 1):
            p = np.array([sx * hx, sy * hy, 0.0])
            tubes.append(A.cone(t, p + Z * 0.2, p + Z * TOWER_H, R, R))
            # Lenkrolle: Gabel, Rad, Feststeller
            steel2.append(A.box(t, p + Z * 0.15, X, (0.04, 0.035, 0.05), 0.006, up=Z))
            wheels.append(sdf.Part('torus', t, k=0.0, c=p + Z * 0.075 + X * 0.03, R=sdf.frame_from(X, Y), major=0.055, minor=0.022))
            steel2.append(A.cone(t, p + Z * 0.075 + X * 0.03 - Y * 0.03, p + Z * 0.075 + X * 0.03 + Y * 0.03, 0.03, 0.03))
            rust.append(A.box(t, p + Z * 0.16 + X * 0.06, X, (0.03, 0.012, 0.006), 0.002, up=Z))
            for z in (1.7,):  # Zapfenverbindung
                steel2.append(A.cone(t, p + Z * (z - 0.05), p + Z * (z + 0.05), R * 1.25, R * 1.25))
    for sy in (-1, 1):  # Stirnrahmen: Sprossen alle 0,28 m
        z = 0.42
        while z < TOWER_H - 0.05:
            tubes.append(A.cone(t, (-hx, sy * hy, z), (hx, sy * hy, z), R * 0.8, R * 0.8, k=0.008))
            z += 0.28
    for sx in (-1, 1):  # Laengsseiten: Diagonalen und Holme
        for z0 in (0.3, 1.25, 2.2):
            a = np.array([sx * hx, -hy, z0])
            b = np.array([sx * hx, hy, z0 + 0.95])
            tubes.append(A.cone(t, a, b, R * 0.7, R * 0.7, k=0.006))
        tubes.append(A.cone(t, (sx * hx, -hy, 0.3), (sx * hx, hy, 0.3), R * 0.8, R * 0.8, k=0.006))
        for z in (TOWER_H - 0.5, TOWER_H - 0.02):  # Gelaender oben
            tubes.append(A.cone(t, (sx * hx, -hy, z), (sx * hx, hy, z), R * 0.8, R * 0.8, k=0.006))
    # Beläge: unten (0,3) und oben (TOWER_H - 1,0) mit Durchstiegsklappe, Bordbretter
    for zb in (0.32, TOWER_H - 1.0):
        for q in range(3):
            wood.append(A.box(t, (-hx * 0.66 + q * hx * 0.66, 0.0, zb + 0.02), Y, (hy - 0.02, hx * 0.31, 0.012), 0.003, up=Z))
        steel2.append(A.box(t, (0.0, 0.0, zb), Y, (hy, hx, 0.012), 0.002, up=Z, k=0.0))
    zb = TOWER_H - 1.0
    for sx in (-1, 1):
        wood.append(A.box(t, (sx * (hx - 0.01), 0.0, zb + 0.09), Y, (hy, 0.012, 0.075), 0.002, up=Z))
    for sy in (-1, 1):
        wood.append(A.box(t, (0.0, sy * (hy - 0.01), zb + 0.09), X, (hx, 0.012, 0.075), 0.002, up=Z))
    wood.append(A.box(t, (0.12, 0.35, zb + 0.04), Y, (0.25, 0.2, 0.012), 0.002, up=Z, op='sub'))
    groups = [
        {'name': 'rohre', 'fm': 'ent_ob_steel', 'parts': tubes, 'disp': steel_disp, 'h': 0.004, 'lod_weight': 1.2},
        {'name': 'belag', 'fm': 'ent_ob_wood', 'parts': wood, 'disp': wood_disp, 'h': 0.004, 'lod_weight': 0.6},
        {'name': 'rollen', 'fm': 'ent_ob_rubber', 'parts': wheels, 'disp': None, 'h': 0.003, 'lod_weight': 0.3},
        {'name': 'gabeln', 'fm': 'ent_ob_rust', 'parts': steel2 + rust, 'disp': None, 'h': 0.003, 'lod_weight': 0.4},
    ]
    # Gasflaschenwagen ausserhalb des Felds (Brenner): zwei Flaschen, Kappen rot und weiss, Schlauchrolle
    gc = np.array([-1.15, 0.72, 0.0])
    cart = [A.box(B_ROOT, gc + Z * 0.02, X, (0.18, 0.28, 0.015), 0.004, up=Z),
            A.cone(B_ROOT, gc + X * -0.16 + Z * 0.02, gc + X * -0.2 + Z * 1.1, 0.012, 0.012),
            sdf.Part('torus', B_ROOT, k=0.0, c=gc + X * -0.12 + Z * 0.12 + Y * 0.3, R=sdf.frame_from(X, Y), major=0.09, minor=0.025)]
    bottles = []
    caps_r, caps_w = [], []
    for q, yy in enumerate((-0.12, 0.12)):
        c = gc + Y * yy + X * 0.02
        bottles.append(A.cone(B_ROOT, c + Z * 0.04, c + Z * 1.05, 0.11, 0.11))
        bottles.append(A.ell(B_ROOT, c + Z * 1.05, Z, (0.11, 0.11, 0.06), k=0.02))
        (caps_r if q == 0 else caps_w).append(A.cone(B_ROOT, c + Z * 1.06, c + Z * 1.2, 0.045, 0.04, k=0.01))
        (caps_r if q == 0 else caps_w).append(A.ell(B_ROOT, c + Z * 1.0, Z, (0.112, 0.112, 0.05), k=0.0))
    groups += [{'name': 'wagen', 'fm': 'ent_ob_rubber', 'parts': cart, 'disp': None, 'h': 0.004, 'lod_weight': 0.2},
               {'name': 'flaschen', 'fm': 'ent_ob_rust', 'parts': bottles, 'disp': steel_disp, 'h': 0.005, 'lod_weight': 0.3},
               {'name': 'kappe_r', 'fm': 'ent_ob_tape_r', 'parts': caps_r, 'disp': None, 'h': 0.004, 'merge': 'band_r', 'lod_weight': 0.1},
               {'name': 'kappe_w', 'fm': 'ent_ob_tape_w', 'parts': caps_w, 'disp': None, 'h': 0.004, 'merge': 'band_w', 'lod_weight': 0.1}]
    groups += zone_groups(TOW_ZONE[0], TOW_ZONE[1], -TOW_ZONE[2], TOW_ZONE[2], beacon=(TOW_ZONE[0], TOW_ZONE[2]), mid_x=True, rng=rng)
    return groups
