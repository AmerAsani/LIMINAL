"""V13.3 Teil 4: Jerry (Wikidot entity-7 Rev. 14, level-2 Rev. 93; KREATUR_Jerry.md) - Modell im Skelett 'ent_jerry'
(anim/creature.hpp, namespace jy).

Wiki: ein blauer Papagei, verglichen mit dem Hyazinth-Ara; spricht und denkt wie ein erwachsener Mensch. Die Followers
raubten Medikamente und pluenderten eine Tierstation - man vermutet, dass er menschliche Krankheiten bekommt.
Umsetzung: echter Hyazinth-Ara (1 m mit Schwanz): kobaltblaues Gefieder, Unterseite der Schwingen und des Schwanzes
schwarzgrau, nackter gelber Augenring und gelbe Hautsichel am Unterschnabel, riesiger schwarzer Hakenschnabel, schwarze
Zunge mit gelbem Streifen, dunkle Wendezehen. Auslegung (Briefing): krank und gerupft - kahle, grau-rosa Haut mit
Gaensehaut und blaeulichen Blutkielen an Brust, Flanken, unter den Fluegeln und am Hals (Federrupfen, wie es kranke
Papageien tun); Augen zu hell und zu menschlich: blasse Iris um eine Pupille, die sich verengt (eigener Knochen).
"""
import math

import numpy as np

import anat as A
import sdf

X = np.array([1.0, 0, 0])
Y = np.array([0, 1.0, 0])
Z = np.array([0, 0, 1.0])

# Gelenke und Knochen (wie anim/creature.hpp, jy)
(ROOT, PELVIS, SPINE, CHEST, NECK0, NECK1, HEAD, CROWN, BEAKP, BEAKT, JAWP, JAWT, TONGA, TONGB,
 EYE_L, PUP_L, EYE_R, PUP_R, RUFF) = range(19)
WING0 = 19
LEG0 = WING0 + 22
TAIL0, TAIL1, TAIL2 = LEG0 + 24, LEG0 + 25, LEG0 + 26
(B_ROOT, B_PELVIS, B_SPINE, B_CHEST, B_NECK0, B_NECK1, B_HEAD, B_BEAK, B_JAW, B_TONGUE, B_PUP_L, B_PUP_R, B_RUFF) = range(13)
B_WING0 = 13
B_LEG0 = B_WING0 + 20
B_TAIL0 = B_LEG0 + 22
KPRIM, KSEC, KTOES = 4, 3, 4

LODS = (60000, 19000, 6500)


def wingJ(s, i): return WING0 + s * 11 + i
def wingB(s, i): return B_WING0 + s * 10 + i
def legJ(s, i): return LEG0 + s * 12 + i
def legB(s, i): return B_LEG0 + s * 11 + i


def check(bones):
    assert bones[B_TAIL0]['a'] == TAIL0 and bones[legB(1, 10)]['b'] == legJ(1, 11) and bones[B_RUFF]['b'] == RUFF, \
        'Skelett passt nicht zu jerry.py'


# --- Oberflaechen -----------------------------------------------------------------------------------------------------
def feather_disp(Q):  # Konturfedern: dachziegelartig nach unten gestreckt, Kanten, feine Aeste
    d1, d2 = sdf.worley(Q * np.array([1.25, 1.25, 0.8]), 80, seed=7001)
    edge = np.exp(-((d2 - d1) / 0.1) ** 2)
    barb = sdf.ridged(Q * np.array([1.0, 1.0, 0.3]), 260, 2, seed=7002)
    return -0.0011 * d1 - 0.0006 * edge - 0.00035 * (barb - 0.5) + 0.0003 * sdf.fbm(Q, 30, 2, seed=7003)


def vane_disp(Q):  # Schwungfedern: feine Aeste, ausgefranst, ein paar Luecken
    barb = sdf.ridged(Q * np.array([1.0, 1.0, 1.0]), 420, 2, seed=7011)
    gap = sdf.fbm(Q, 26, 2, seed=7012)
    return -0.0003 * (barb - 0.5) - np.where(gap > 0.42, 0.0014, 0.0)


def skin_disp(Q):  # gerupfte Haut: Gaensehaut aus Federfollikeln, Runzeln, Schorf
    d1, d2 = sdf.worley(Q, 240, seed=7021)
    bump = 0.0009 * np.exp(-(d1 / 0.22) ** 2)
    wr = -0.0006 * sdf.ridged(Q * np.array([1.0, 1.0, 0.5]), 90, 2, seed=7022)
    scab = 0.0007 * np.clip(sdf.fbm(Q, 60, 2, seed=7023) - 0.25, 0.0, 1.0)
    return bump + wr + scab


def beak_disp(Q):  # Horn: Laengsriefen, Kratzer, abgesplitterte Schneide
    return -0.00028 * sdf.ridged(Q * np.array([1.0, 0.3, 1.0]), 340, 2, seed=7031) + 0.00012 * sdf.fbm(Q, 900, 1, seed=7032)


def foot_disp(Q):  # Laeufe und Zehen: kleine Schuppen mit tiefen Fugen, Polster
    d1, d2 = sdf.worley(Q, 520, seed=7041)
    return -0.00045 * np.exp(-((d2 - d1) / 0.12) ** 2) - 0.00012 * d1


def yellow_disp(Q):  # nackte gelbe Haut am Auge und Schnabel: feine Falten
    return -0.00025 * sdf.ridged(Q, 400, 2, seed=7051)


def tongue_disp(Q):  # dicke, trockene Zunge: Papillen
    d1, _ = sdf.worley(Q, 700, seed=7061)
    return 0.00025 * np.exp(-(d1 / 0.3) ** 2)


# --- Werkzeug ---------------------------------------------------------------------------------------------------------
def ell_surface(c, r, d):
    """Punkt auf einem achsparallelen Ellipsoid (Mitte c, Radien r) in Richtung d, und die Normale dort."""
    d = A.unit(d)
    t = 1.0 / math.sqrt(sum((d[i] / r[i]) ** 2 for i in range(3)))
    p = np.asarray(c, float) + d * t
    n = A.unit((p - c) / (np.asarray(r, float) ** 2))
    return p, n


def vane(bone, a, b, w0, w1, up, t0=0.0032, t1=0.0016, off=None, k=0.0):
    """Flache Feder von a nach b: Breite w0..w1 (halb), Dicke t0..t1 (halb) in Richtung up; off verschiebt die Fahne."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    if off is not None:
        a, b = a + off, b + off
    return A.tube(bone, a, b, (w0, t0), (w1, t1), up=up, k=k)


# --- Koerper ----------------------------------------------------------------------------------------------------------
def build(J, bones):
    check(bones)
    J = np.asarray(J, dtype=float)
    rng = np.random.default_rng(7000)
    feather, dark, skin, quill, beak, yellow, eye, pupil, tongue, mouth, foot, claw = ([] for _ in range(12))

    # Rumpf: Bauch, breite Brust, Schultern; Hals; der Kopf gross und rund
    ax_b = A.unit(J[CHEST] - J[PELVIS])
    body_c = [(-0.032, 0, 0.135), (-0.006, 0, 0.232), (0.018, 0, 0.300)]
    body_r = [(0.07, 0.058, 0.062), (0.082, 0.066, 0.07), (0.058, 0.056, 0.052)]
    feather += [A.ell(B_PELVIS, body_c[0], ax_b, body_r[0], up=-X, k=0.0),
                A.ell(B_SPINE, body_c[1], A.unit(J[NECK0] - J[SPINE]), body_r[1], up=X, k=0.035),
                A.ell(B_CHEST, body_c[2], A.unit(J[NECK0] - J[CHEST]), body_r[2], up=X, k=0.03)]
    # Bauchgefieder ("Hosen") um die Knie, Steiss ueber dem Schwanz
    for g in (1.0, -1.0):
        feather.append(A.ell(B_PELVIS, (0.012, g * 0.038, 0.085), Z, (0.03, 0.026, 0.035), k=0.02))
    feather.append(A.ell(B_PELVIS, (-0.075, 0, 0.11), A.unit((-0.5, 0, -0.8)), (0.05, 0.035, 0.022), up=X, k=0.025))
    # Hals mit Halskrause (eigener Knochen - straeubt sich): die Federn im Nacken und an den Seiten haengen an B_RUFF
    feather += [A.tube(B_NECK0, J[NECK0], J[NECK1], (0.036, 0.034), (0.032, 0.031), up=X, k=0.025),
                A.tube(B_NECK1, J[NECK1], J[HEAD] - X * 0.01, (0.032, 0.031), (0.034, 0.033), up=X, k=0.02),
                A.ell(B_RUFF, (-0.012, 0, 0.385), A.unit((0.3, 0, 1.0)), (0.035, 0.038, 0.022), up=-X, k=0.02)]
    for g in (1.0, -1.0):
        feather.append(A.ell(B_RUFF, (0.005, g * 0.026, 0.375), A.unit((0.2, 0, 1.0)), (0.03, 0.016, 0.02), up=g * Y, k=0.018))
    # Kopf: rund, flache Stirn zum Schnabel, Wangen; Augenhoehlen
    feather += [A.ell(B_HEAD, (0.052, 0, 0.44), X, (0.044, 0.039, 0.041), k=0.02),
                A.ell(B_HEAD, (0.075, 0, 0.452), A.unit((1.0, 0, -0.35)), (0.03, 0.03, 0.022), k=0.015)]
    for g in (1.0, -1.0):
        feather.append(A.ell(B_HEAD, (0.07, g * 0.022, 0.425), X, (0.028, 0.02, 0.022), k=0.015))  # Wange
    eye_c = []
    for s_, g in ((0, 1.0), (1, -1.0)):
        e = J[EYE_L if s_ == 0 else EYE_R]
        ed = A.unit(J[PUP_L if s_ == 0 else PUP_R] - e)
        c = e - ed * 0.0085  # Mitte des Augapfels
        eye_c.append((c, ed))
        feather.append(A.ell(B_HEAD, c + ed * 0.004, ed, (0.012, 0.0125, 0.0125), op='sub', k=0.004))

    # --- gerupft (Auslegung): kahle Stellen an Brust, Flanken, Hals; darunter die Haut, Blutkiele, abgebrochene Kiele ------
    skin += [A.ell(B_SPINE, body_c[1], A.unit(J[NECK0] - J[SPINE]), np.array(body_r[1]) - 0.0045, up=X, k=0.0),
             A.ell(B_PELVIS, body_c[0], ax_b, np.array(body_r[0]) - 0.0045, up=-X, k=0.03),
             A.ell(B_CHEST, body_c[2], A.unit(J[NECK0] - J[CHEST]), np.array(body_r[2]) - 0.0045, up=X, k=0.03),
             A.tube(B_NECK0, J[NECK0], J[NECK1], (0.031, 0.029), (0.028, 0.027), up=X, k=0.02)]
    patches = [  # (Knochen, Mitte, Radien) - aus dem Gefieder geschnitten
        (B_SPINE, (0.07, 0.006, 0.215), (0.03, 0.036, 0.05)),     # Brust, vorn (aus der Ferne sichtbar)
        (B_PELVIS, (0.04, -0.03, 0.15), (0.026, 0.025, 0.03)),    # Bauch, rechts
        (B_SPINE, (0.0, 0.07, 0.235), (0.035, 0.022, 0.045)),     # Flanke links (unter dem Fluegel)
        (B_SPINE, (0.005, -0.07, 0.245), (0.03, 0.022, 0.04)),    # Flanke rechts
        (B_NECK0, (0.045, -0.025, 0.37), (0.016, 0.016, 0.018)),  # Halsseite
    ]
    for b, c, r in patches:
        rr = np.array(r) * (1.0 + 0.25 * (rng.random(3) - 0.5))
        feather.append(A.ell(b, c, A.unit(X + 0.3 * (rng.random(3) - 0.5)), rr, op='sub', k=0.012))
        # Blutkiele (blaeulich, wachsig) und abgebrochene Kiele in und am Rand der kahlen Stelle
        for _ in range(16):
            d = A.unit(np.array(c) - np.array(body_c[1] if b != B_PELVIS else body_c[0]) + rng.normal(0, 0.012, 3))
            ce, re_ = (body_c[1], np.array(body_r[1]) - 0.0045) if b != B_PELVIS else (body_c[0], np.array(body_r[0]) - 0.0045)
            if b == B_NECK0:
                p = np.array(c) + rng.normal(0, 0.008, 3)
                n = A.unit(p - (J[NECK0] + J[NECK1]) / 2)
            else:
                p, n = ell_surface(ce, re_, d)
            L = rng.uniform(0.004, 0.013)
            dn = A.unit(n - Z * 0.6 + rng.normal(0, 0.25, 3))  # Kiele zeigen nach unten
            quill.append(A.cone(b, p - n * 0.001, p + dn * L, rng.uniform(0.0012, 0.0019), 0.0005))
    # Fluegelunterseite: kahl (sichtbar, wenn er die Fluegel ausbreitet)
    for s_, g in ((0, 1.0), (1, -1.0)):
        a, b = J[wingJ(s_, 0)], J[wingJ(s_, 1)]
        skin.append(A.tube(wingB(s_, 0), a, b, (0.022, 0.016), (0.018, 0.013), up=g * Y, k=0.0))

    # --- Fluegel: Oberarm- und Handdecken (blau), Hand- und Armschwingen in Lagen (oben blau, unten schwarzgrau) -------------
    for s_, g in ((0, 1.0), (1, -1.0)):
        n = g * Y
        SH, EL, WR, HT = (J[wingJ(s_, i)] for i in range(4))
        feather += [A.tube(wingB(s_, 0), SH, EL, (0.03, 0.017), (0.026, 0.015), up=n, k=0.012),
                    A.tube(wingB(s_, 1), EL, WR, (0.026, 0.015), (0.021, 0.013), up=n, k=0.012),
                    A.tube(wingB(s_, 2), WR, HT, (0.021, 0.013), (0.014, 0.009), up=n, k=0.01)]
        # Decken: grosse, flache Masse ueber Unterarm und Hand (die Schulter des angelegten Fluegels)
        mid = (EL + WR + HT) / 3
        feather.append(A.ell(wingB(s_, 1), mid + n * 0.004, A.unit(HT - EL + (WR - EL) * 0.3), (0.06, 0.04, 0.014), up=n, k=0.02))
        feather.append(A.ell(wingB(s_, 2), (WR + HT) / 2 + n * 0.003, A.unit(HT - WR), (0.045, 0.03, 0.011), up=n, k=0.015))
        # Armschwingen (am Ellbogen): breit; innen (zum Koerper) die tiefste Lage
        for k in range(KSEC):
            tip = J[wingJ(s_, 4 + KPRIM + k)]
            off = n * (0.0035 * (KSEC - k) - 0.008)
            vane_parts = [vane(wingB(s_, 3 + KPRIM + k), EL, tip, 0.024, 0.02, n, off=off)]
            feather += vane_parts
            dark.append(vane(wingB(s_, 3 + KPRIM + k), EL + (tip - EL) * 0.2, tip, 0.021, 0.017, n, t0=0.0024, t1=0.0012, off=off - n * 0.0016))
        # Handschwingen (an der Handspitze): lang, schmal, die aeusserste oben; ausgefranste Spitzen
        for k in range(KPRIM):
            tip = J[wingJ(s_, 4 + k)]
            off = n * (0.004 * (KPRIM - k))
            feather.append(vane(wingB(s_, 3 + k), HT, tip, 0.017, 0.009, n, off=off))
            dark.append(vane(wingB(s_, 3 + k), HT + (tip - HT) * 0.15, tip - A.unit(tip - HT) * 0.004, 0.0145, 0.0075, n, t0=0.0022, t1=0.001,
                             off=off - n * 0.0017))
            # Kiel (Schaft) als feine Linie auf der Oberseite
            feather.append(A.cone(wingB(s_, 3 + k), HT + off + n * 0.0028, tip + off + n * 0.0016, 0.0016, 0.0006, k=0.0))
        # ein paar ausgerissene Deckfedern auf dem Fluegel: Luecken, darin Kiele
        for _ in range(3):
            c = EL + (WR - EL) * rng.uniform(0.2, 0.9) + n * 0.012 + Z * rng.uniform(-0.02, 0.0)
            feather.append(A.ell(wingB(s_, 1), c, X, (0.009, 0.007, 0.006), op='sub', k=0.004))
            quill.append(A.cone(wingB(s_, 1), c - n * 0.004, c + n * 0.003 + A.unit(-Z + X * 0.2) * 0.008, 0.0013, 0.0004))

    # --- Schwanz: lang, gestuft, oben blau, unten schwarzgrau ------------------------------------------------------------
    T0, T1, T2 = J[TAIL0], J[TAIL1], J[TAIL2]
    for (a, b, w0, w1, bone) in ((T0, T1, 0.04, 0.036, B_TAIL0), (T1, T2, 0.036, 0.009, B_TAIL0 + 1)):
        up = A.unit(np.cross(Y, A.unit(b - a)))  # Dicke quer zur Fahne
        feather.append(A.tube(bone, a, b, (w0, 0.008), (w1, 0.0035), up=up, k=0.01))
        dark.append(A.tube(bone, a + up * 0.0045, b + up * 0.002, (w0 * 0.92, 0.0045), (w1 * 0.9, 0.002), up=up, k=0.006))
    for k, g in ((0, 1.0), (1, -1.0)):  # aeussere, kuerzere Steuerfedern seitlich versetzt
        a, b = T0 + g * Y * 0.02, T1 + (T2 - T1) * 0.6 + g * Y * 0.028
        up = A.unit(np.cross(Y, A.unit(b - a)))
        feather.append(vane(B_TAIL0, a, b, 0.016, 0.006, up, t0=0.003, t1=0.0015, off=up * 0.006))

    # --- Beine und Fuesse: kurzer, dicker Lauf; Wendezehen (zwei vorn, zwei hinten) mit Polstern und schwarzen Krallen -------
    for s_, g in ((0, 1.0), (1, -1.0)):
        H, K, An, Ft = (J[legJ(s_, i)] for i in range(4))
        foot.append(A.cone(legB(s_, 2), An, Ft, 0.0105, 0.0095, k=0.0))
        foot.append(A.ell(legB(s_, 2), An + Z * 0.004, X, (0.012, 0.011, 0.011), k=0.006))  # Ferse
        foot.append(A.ell(legB(s_, 2), Ft, X, (0.013, 0.012, 0.008), k=0.006))
        for t in range(KTOES):
            m_, tp = J[legJ(s_, 4 + t * 2)], J[legJ(s_, 5 + t * 2)]
            foot += A.chain(lambda i, t=t: legB(s_, 3 + t * 2 + i), [Ft, m_, tp], [0.0072, 0.0062, 0.0048], k=0.004)
            for pp in (m_, tp):  # Polster unten
                foot.append(A.ell(legB(s_, 4 + t * 2), pp - Z * 0.002, X, (0.006, 0.0055, 0.004), k=0.003))
            d = A.unit(tp - m_)
            cd = A.unit(d - Z * 0.8)
            claw += A.chain(lambda i, t=t: legB(s_, 4 + t * 2), [tp, tp + d * 0.008 - Z * 0.002, tp + cd * 0.016], [0.0036, 0.0026, 0.0006], k=0.002)

    # --- Gesicht: nackter gelber Augenring, gelbe Hautsichel am Unterschnabel ---------------------------------------------
    for (c, ed) in eye_c:
        R = sdf.frame_from(ed, Z)
        yellow.append(sdf.Part('torus', B_HEAD, k=0.0, c=c + ed * 0.0045, R=np.stack([R[:, 1], R[:, 2], R[:, 0]], axis=1),
                               major=0.0108, minor=0.0034))
        eye.append(A.ell(B_HEAD, c, ed, (0.0088, 0.0088, 0.0088), k=0.0))
        # Pupille: flache Scheibe auf der Hornhaut, am Pupillenknochen (verengt sich um die Mitte)
        e0 = c + ed * 0.0085
        pupil.append(A.ell(B_PUP_L if ed[1] > 0 else B_PUP_R, e0 + ed * 0.0004, ed, (0.0009, 0.0042, 0.0042), k=0.0))
    for g in (1.0, -1.0):  # Hautsichel: halbmondfoermig am Ansatz des Unterschnabels
        yellow.append(A.tube(B_HEAD, (0.078, g * 0.016, 0.43), (0.088, g * 0.021, 0.414), (0.006, 0.0032), (0.0055, 0.003), up=g * Y, k=0.0))
        yellow.append(A.tube(B_HEAD, (0.088, g * 0.021, 0.414), (0.092, g * 0.017, 0.4), (0.0055, 0.003), (0.004, 0.0025), up=g * Y, k=0.004))

    # --- Schnabel: riesiger Hakenschnabel (Oberschnabel ueber den unteren gezogen), Unterschnabel als tiefe Schaufel -----------
    up_pts = [np.array(p) for p in ((0.086, 0, 0.452), (0.115, 0, 0.449), (0.137, 0, 0.431), (0.147, 0, 0.405), (0.146, 0, 0.38), (0.137, 0, 0.362))]
    up_r = [(0.021, 0.019), (0.019, 0.018), (0.015, 0.015), (0.011, 0.011), (0.0065, 0.0065), (0.0018, 0.0018)]
    for i in range(len(up_pts) - 1):
        beak.append(A.tube(B_BEAK, up_pts[i], up_pts[i + 1], up_r[i], up_r[i + 1], up=A.perp(up_pts[i + 1] - up_pts[i], (-1, 0, 1)), k=0.004 if i else 0.0))
    beak.append(A.ell(B_BEAK, (0.112, 0, 0.418), A.unit((1.0, 0, -0.5)), (0.03, 0.0135, 0.014), op='sub', k=0.004))  # Hohlraum unten
    # Zahn des Oberschnabels (Kerbe an der Schneide) und Nasenloecher in der Wachshaut
    for g in (1.0, -1.0):
        beak.append(A.ell(B_BEAK, (0.128, g * 0.012, 0.42), A.unit((1, 0, -1)), (0.006, 0.004, 0.003), op='sub', k=0.002))
        feather.append(A.ell(B_HEAD, (0.086, g * 0.008, 0.452), X, (0.0025, 0.002, 0.002), op='sub', k=0.0))
    lo_pts = [np.array(p) for p in ((0.08, 0, 0.42), (0.104, 0, 0.408), (0.122, 0, 0.395), (0.132, 0, 0.386))]
    lo_r = [(0.02, 0.016), (0.017, 0.013), (0.012, 0.009), (0.006, 0.005)]
    for i in range(len(lo_pts) - 1):
        beak.append(A.tube(B_JAW, lo_pts[i], lo_pts[i + 1], lo_r[i], lo_r[i + 1], up=Z, k=0.004 if i else 0.0))
    beak.append(A.ell(B_JAW, (0.107, 0, 0.414), A.unit((1, 0, -0.3)), (0.03, 0.012, 0.009), op='sub', k=0.003))  # Schaufel oben offen
    mouth.append(A.ell(B_JAW, (0.1, 0, 0.408), A.unit((1, 0, -0.3)), (0.024, 0.0105, 0.004), k=0.0))
    mouth.append(A.ell(B_HEAD, (0.095, 0, 0.428), A.unit((1, 0, -0.4)), (0.022, 0.012, 0.005), k=0.0))  # Gaumen
    # Zunge: dick, schwarz, rund wie ein Daumen - mit gelbem Streifen (wie beim Hyazinth-Ara)
    TA, TB = J[TONGA], J[TONGB]
    tongue.append(A.tube(B_TONGUE, TA - A.unit(TB - TA) * 0.01, TB, (0.0085, 0.0075), (0.0075, 0.0068), up=Z, k=0.0))
    tongue.append(A.ell(B_TONGUE, TB + A.unit(TB - TA) * 0.002, A.unit(TB - TA), (0.0065, 0.0072, 0.0062), k=0.003))
    yellow.append(A.tube(B_TONGUE, TA + Z * 0.0068, TB + Z * 0.0062 + A.unit(TB - TA) * 0.002, (0.0024, 0.0012), (0.0016, 0.001), up=Z, k=0.0))

    return [
        {'name': 'federn', 'fm': 'ent_jy_feather', 'parts': feather, 'disp': feather_disp, 'h': 0.0014, 'cav_scale': 0.006, 'lod_weight': 1.4},
        {'name': 'unterseite', 'fm': 'ent_jy_feather_dark', 'parts': dark, 'disp': vane_disp, 'h': 0.0009, 'cav_scale': 0.003, 'lod_weight': 0.7},
        {'name': 'haut', 'fm': 'ent_jy_skin', 'parts': skin, 'disp': skin_disp, 'h': 0.0011, 'cav_scale': 0.003, 'lod_weight': 1.0},
        {'name': 'kiele', 'fm': 'ent_jy_quill', 'parts': quill, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'lod_weight': 0.6},
        {'name': 'schnabel', 'fm': 'ent_jy_beak', 'parts': beak, 'disp': beak_disp, 'h': 0.0006, 'cav_scale': 0.002, 'lod_weight': 1.0},
        {'name': 'gelb', 'fm': 'ent_jy_yellow', 'parts': yellow, 'disp': yellow_disp, 'h': 0.00045, 'cav_scale': 0.001, 'lod_weight': 0.7},
        {'name': 'augen', 'fm': 'ent_jy_eye', 'parts': eye, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'lod_weight': 0.8},
        {'name': 'pupillen', 'fm': 'ent_jy_pupil', 'parts': pupil, 'disp': None, 'h': 0.0003, 'cav_scale': 0.001, 'lod_weight': 0.5},
        {'name': 'zunge', 'fm': 'ent_jy_tongue', 'parts': tongue, 'disp': tongue_disp, 'h': 0.0005, 'cav_scale': 0.001, 'lod_weight': 0.4},
        {'name': 'rachen', 'fm': 'ent_pg_mouth', 'parts': mouth, 'disp': None, 'h': 0.0007, 'cav_scale': 0.002, 'lod_weight': 0.2},
        {'name': 'fuesse', 'fm': 'ent_jy_foot', 'parts': foot, 'disp': foot_disp, 'h': 0.0006, 'cav_scale': 0.002, 'lod_weight': 0.8},
        {'name': 'krallen', 'fm': 'ent_jy_beak', 'parts': claw, 'disp': None, 'h': 0.0004, 'cav_scale': 0.001, 'lod_weight': 0.4},
    ]
