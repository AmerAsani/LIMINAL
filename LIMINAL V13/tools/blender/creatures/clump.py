"""V13: Clump / Buendel (Entity 5) - KREATUR_Clump.md. Kein Spinnenrumpf aus Rohren: ein Knaeuel aus verwachsenen
menschlichen Gliedern. Sieben lebende Arme (einer 2,6 m lang, ausgezehrt, mit verlaengerten Fingern) und zwei
nachschleifende Beine wachsen aus einer Masse, die selbst aus toten, halb versunkenen Armen und Beinen besteht -
Ellbogen, Knie, Haende, die sich in die Masse krallen, ein Fuss, der herausragt. Drei Hauttoene (verschiedene
Menschen). In der Masse: drei schiefe Augen mit Lidern, ein Ohr; unten eine quer liegende Spalte mit Lippenwuelsten
und zwei Reihen rasiermesserscharfer Zaehne.

Knochen (ent_clump): 1 Kern, Arm a: 2+3a Oberarm, 3+3a Unterarm, 4+3a Hand; Bein l: 23+2l Oberschenkel,
24+2l Unterschenkel (der Fuss haengt am Unterschenkel). Gelenke: Arm a 4+4a Schulter .. 7+4a Fingerspitzen,
Bein l 32+3l Huefte, 33+3l Knie, 34+3l Knoechel, 3 Maul.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (80000, 22000, 6500)

CORE = 1
C = np.array([0.0, 0.0, 0.44])
CORE_R = np.array([0.25, 0.23, 0.20])  # Runde 1: kleiner, die Glieder tragen das Volumen  # Fuellmasse zwischen den Gliedern: flacher, breiter Haufen
TONES = ('clump_a', 'clump_b', 'clump_c')
Z = np.array([0.0, 0.0, 1.0])


def norm(x):
    return float(np.linalg.norm(x))


def orth(v, axis):
    """v ohne Anteil entlang axis, normiert."""
    v = np.asarray(v, dtype=np.float64)
    w = v - axis * float(v @ axis)
    return A.unit(w) if np.linalg.norm(w) > 1e-6 else A.perp(axis)


def rot(v, axis, ang):
    axis = A.unit(axis)
    return v * math.cos(ang) + np.cross(axis, v) * math.sin(ang) + axis * float(axis @ v) * (1 - math.cos(ang))


def surf(u, off=0.0):
    """Punkt auf der Fuellmasse in Richtung u (vom Kern aus), um off nach aussen versetzt."""
    u = A.unit(u)
    r = 1.0 / math.sqrt(float(np.sum((u / CORE_R) ** 2)))
    return C + u * (r + off)


def sph(az, el):
    return np.array([math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el)])


# --- Hand, Arm, Fuss, Bein ---------------------------------------------------------------------------------------
def hand(W, hd, back, hs, bone, curl, floor=None, sc=1.0, flen=1.0):
    """Menschliche Hand am Handgelenk W: Finger nach hd, Handruecken nach back, Daumen auf der Seite hs (+1/-1).
    curl: je Finger Beugung an Grund-, Mittel- und Endgelenk (Grad). floor: Kuppen nicht unter diese Hoehe."""
    side = A.unit(np.cross(back, hd))
    skin, nails = [], []
    skin.append(A.box(bone, W + hd * 0.050 * sc, hd, (0.040 * sc, 0.034 * sc, 0.0092 * sc), 0.008 * sc, up=back, k=0.012))
    skin.append(A.ell(bone, W + hd * 0.046 * sc - side * hs * 0.024 * sc - back * 0.004 * sc, hd,
                      (0.040 * sc, 0.012 * sc, 0.011 * sc), up=back, k=0.012))                          # Kleinfingerballen
    skin.append(A.ell(bone, W + hd * 0.030 * sc + side * hs * 0.022 * sc - back * 0.007 * sc, hd + side * hs * 0.6,
                      (0.030 * sc, 0.015 * sc, 0.012 * sc), up=back, k=0.012))                          # Daumenballen
    offs = np.array([0.026, 0.009, -0.009, -0.025]) * hs * sc
    fan = np.array([0.10, 0.03, -0.04, -0.13]) * hs
    lens = np.array([0.95, 1.0, 0.95, 0.78])
    for f in range(4):
        K = W + hd * 0.093 * sc + side * offs[f] + back * 0.001 * sc
        skin.append(A.cone(bone, W + hd * 0.014 * sc + side * offs[f] * 0.45 + back * 0.0062 * sc, K + back * 0.003 * sc,
                           0.0046 * sc, 0.0060 * sc, k=0.006))                                         # Mittelhandknochen
        skin.append(A.ell(bone, K + back * 0.0025 * sc, hd, (0.0105 * sc, 0.0094 * sc, 0.0085 * sc), up=back, k=0.006))  # Knoechel
        fd = A.unit(hd + side * fan[f])
        L = np.array([0.044, 0.027, 0.021]) * lens[f] * sc * flen
        R = np.array([0.0092, 0.0084, 0.0075, 0.0063]) * (0.9 if f == 3 else 1.0) * sc
        ang = np.array(curl[f], dtype=np.float64)
        for _ in range(10):
            pts, dirs, a = [K], [], 0.0
            for i in range(3):
                a += math.radians(ang[i])
                d = fd * math.cos(a) - back * math.sin(a)
                dirs.append(d)
                pts.append(pts[-1] + d * L[i])
            if floor is None or min(p[2] for p in pts[1:]) - R[-1] >= floor:
                break
            ang *= 0.75  # die Kuppe stuetzt sich auf, statt in den Boden zu greifen
        skin += A.chain(lambda k: bone, pts, R, k=0.004)
        for i in (1, 2):  # Gelenkknoten
            skin.append(A.ell(bone, pts[i], dirs[i - 1] + dirs[i], (R[i] * 1.15, R[i] * 1.12, R[i] * 1.06), up=back, k=0.003))
        d3 = dirs[2]
        bk = A.unit(np.cross(d3, side))
        nails.append(A.ell(bone, A.lerp(pts[2], pts[3], 0.60) + bk * R[3] * 0.92, d3,
                           (0.0088 * lens[f] * sc, R[3] * 0.86, 0.0016), up=bk, group='nail'))
    # Daumen
    c0 = W + hd * 0.020 * sc + side * hs * 0.021 * sc - back * 0.006 * sc
    td = A.unit(hd * 0.55 + side * hs * 0.72 - back * 0.40)
    tL = np.array([0.040, 0.031, 0.025]) * sc * (1 + (flen - 1) * 0.5)
    tR = np.array([0.0125, 0.0105, 0.0095, 0.0080]) * sc
    for lift in np.linspace(0, 0.6, 7):
        d1 = A.unit(td + back * lift)
        d2 = A.unit(d1 + hd * 0.25 - back * (0.15 - lift * 0.4))
        d3 = A.unit(d2 + hd * 0.25 - back * (0.20 - lift * 0.4))
        pts = [c0, c0 + d1 * tL[0]]
        pts.append(pts[-1] + d2 * tL[1])
        pts.append(pts[-1] + d3 * tL[2])
        if floor is None or min(p[2] for p in pts[1:]) - tR[-1] >= floor:
            break
    skin += A.chain(lambda k: bone, pts, tR, k=0.005)
    nb = orth(side * hs * 0.75 + back * 0.65, d3)
    nails.append(A.ell(bone, A.lerp(pts[2], pts[3], 0.6) + nb * tR[3] * 0.92, d3, (0.0095 * sc, tR[3] * 0.9, 0.0017), up=nb, group='nail'))
    return skin, nails


def arm(S, E, W, T, bones, th, hs, back_hint, floor=None, gaunt=False, root=None, sc=1.0, flen=1.0, curl=None):
    """Lebender Arm: Schulter (Deltamuskel), Oberarm mit Bizeps und Trizeps, Ellbogen mit Spitze und Knorren,
    Unterarm mit Muskelbauch nahe dem Ellbogen und Sehnen zum Handgelenk, flaches Handgelenk, Hand."""
    bu, bf, bh = bones
    skin = []
    du, df = A.unit(E - S), A.unit(W - E)
    lu, lf = norm(E - S), norm(W - E)
    nfl = orth(df, du)                     # Beugeseite am Oberarm (zum Unterarm hin)
    lat = A.unit(np.cross(du, nfl))
    m = 0.72 if gaunt else 1.0
    if root is not None:                   # in die Masse verwachsen: weich zwischen Kern und Oberarm
        skin.append(A.cone(root, S - A.unit(S - C) * 0.16, S, 0.050 * th, 0.042 * th, k=0.05))
    if root is not None:
        # Runde 6: flacher und laenger - als runde Kugeln lasen sich zwei Schultern nebeneinander wie ein Gesaess
        skin.append(A.ell(bu, S + du * 0.11 - nfl * 0.004, du, (0.14, 0.043 * th, 0.039 * th), up=nfl, k=0.06))   # Deltamuskel
    skin.append(A.cone(bu, S, E, 0.044 * th, 0.038 * th))
    cb, ct = min(lu * 0.55, 0.32), min(lu * 0.40, 0.25)
    skin.append(A.ell(bu, S + du * cb + nfl * 0.010 * th, du, (min(lu * 0.32, 0.20), 0.034 * th * m, 0.036 * th * m), up=nfl, k=0.05))  # Bizeps
    skin.append(A.ell(bu, S + du * ct - nfl * 0.010 * th, du, (min(lu * 0.36, 0.23), 0.037 * th * m, 0.033 * th * m), up=nfl, k=0.05))  # Trizeps
    skin.append(A.ell(bf, E + du * 0.006 - nfl * 0.024 * th, du, (0.026, 0.022 * th, 0.020 * th), up=nfl, k=0.03))  # Ellbogenspitze
    for s_ in (1, -1):
        skin.append(A.ell(bu, E + lat * s_ * 0.030 * th - du * 0.012, du, (0.020, 0.013, 0.013), up=nfl, k=0.025))  # Knorren
    hd = A.unit(T - W)
    back = orth(back_hint, hd)
    pal = orth(-back, df)                  # Beugeseite am Unterarm (Handflaeche)
    lat2 = A.unit(np.cross(df, pal))
    skin.append(A.cone(bf, E, W, 0.040 * th, 0.024 * th))
    hl = min(lf * 0.30, 0.18)
    skin.append(A.ell(bf, E + df * min(lf * 0.22, 0.14) + pal * 0.008 * th, df, (hl, 0.036 * th * m, 0.031 * th * m), up=pal, k=0.05))  # Beuger
    skin.append(A.ell(bf, E + df * min(lf * 0.19, 0.12) - pal * 0.007 * th + lat2 * hs * 0.010 * th, df,
                      (hl * 0.9, 0.030 * th * m, 0.027 * th * m), up=pal, k=0.05))                      # Strecker
    skin.append(A.ell(bf, E + df * min(lf * 0.12, 0.08) + lat2 * hs * 0.024 * th - du * 0.01, df,
                      (min(lf * 0.16, 0.10), 0.020 * th * m, 0.026 * th * m), up=pal, k=0.04))   # Oberarmspeichenmuskel
    skin.append(A.cone(bf, E + df * 0.03 - pal * 0.024 * th, W - pal * 0.012 - lat2 * hs * 0.014, 0.008, 0.006, k=0.012))  # Elle
    skin.append(A.ell(bf, W - df * 0.012 - lat2 * hs * 0.020 - pal * 0.006, df, (0.010, 0.009, 0.008), up=pal, k=0.006))  # Griffelfortsatz
    for o in (-0.008, 0.0, 0.008):         # Sehnen zum Handgelenk
        skin.append(A.cone(bf, E + df * (lf * 0.55) + pal * 0.012 * th + lat2 * o, W - df * 0.005 + pal * 0.008 + lat2 * o * 0.8,
                           0.0042, 0.0032, k=0.008))
    if gaunt:                              # ausgezehrt: Strangsehnen entlang der ueberlangen Knochen
        for s_ in (1, -1):
            skin.append(A.cone(bu, S + du * 0.25 + lat * s_ * 0.014, E - du * 0.06 + lat * s_ * 0.012, 0.0075, 0.0065, k=0.01))
            skin.append(A.cone(bf, E + df * 0.15 + pal * 0.010 + lat2 * s_ * 0.011, W - df * 0.04 + lat2 * s_ * 0.009, 0.0065, 0.005, k=0.01))
    skin.append(A.ell(bh, W + hd * 0.004, hd, (0.022, 0.030 * sc, 0.015 * sc), up=back, k=0.016))   # Handgelenk
    curl = curl or [(14, 34, 26), (12, 30, 24), (16, 36, 28), (20, 42, 32)]
    hs_, nails = hand(W, hd, back, hs, bh, curl, floor=floor, sc=sc, flen=flen)
    return skin + hs_, nails


def foot(Fo, fd, sole, bone, sm, sc=1.0):
    """Menschlicher Fuss ab dem Knoechel Fo: Zehen nach fd, Sohle nach sole, grosser Zeh auf der Seite sm."""
    fl = A.unit(np.cross(sole, fd))
    skin = []
    for s_ in (1, -1):
        skin.append(A.ell(bone, Fo + fl * s_ * 0.030 * sc + sole * 0.004, fd, (0.013, 0.010, 0.013), up=sole, k=0.01))  # Knoechel
    skin.append(A.ell(bone, Fo + sole * 0.022 * sc - fd * 0.012 * sc, fd, (0.034 * sc, 0.027 * sc, 0.028 * sc), up=sole, k=0.02))  # Ferse
    skin.append(A.ell(bone, Fo + fd * 0.075 * sc + sole * 0.003, fd, (0.090 * sc, 0.036 * sc, 0.020 * sc), up=sole, k=0.025))  # Mittelfuss
    skin.append(A.ell(bone, Fo + fd * 0.150 * sc + sole * 0.006, fd, (0.026 * sc, 0.045 * sc, 0.019 * sc), up=sole, k=0.018))  # Ballen
    offs = np.array([0.030, 0.012, -0.003, -0.017, -0.029]) * sm * sc
    tl = np.array([0.034, 0.030, 0.027, 0.023, 0.019]) * sc
    tr = np.array([0.0125, 0.0085, 0.0080, 0.0075, 0.0068]) * sc
    for i in range(5):
        b = Fo + fd * 0.168 * sc + fl * offs[i] + sole * 0.004
        d1 = A.unit(fd * math.cos(0.17) + sole * math.sin(0.17))
        d2 = A.unit(fd * math.cos(0.6) + sole * math.sin(0.6))
        p1 = b + d1 * tl[i] * 0.55
        skin += A.chain(lambda k: bone, [b, p1, p1 + d2 * tl[i] * 0.45], [tr[i], tr[i] * 0.92, tr[i] * 0.85], k=0.004)
    return skin


def leg(H, K, Fo, bones, th, sgn, root=None):
    """Nachschleifendes Bein: Gesaess, Oberschenkel mit Strecker und Beuger, Knie mit Kniescheibe, Wade,
    Schienbeinkante; der Fuss liegt auf dem Ruecken, die Sohle nach oben (er wird geschleift)."""
    bt, bs = bones
    skin = []
    dt, ds = A.unit(K - H), A.unit(Fo - K)
    lt, ls = norm(K - H), norm(Fo - K)
    nk = orth(ds, dt)
    lat = A.unit(np.cross(dt, nk))
    if root is not None:
        skin.append(A.ell(root, H - dt * 0.02, dt, (0.10, 0.09, 0.09), up=nk, k=0.05))
    skin.append(A.ell(bt, H + dt * 0.06, dt, (0.12, 0.085 * th, 0.080 * th), up=nk, k=0.05))       # Gesaess
    skin.append(A.cone(bt, H, K, 0.046 * th, 0.033 * th))
    skin.append(A.ell(bt, H + dt * lt * 0.52 - nk * 0.016 * th, dt, (lt * 0.36, 0.044 * th, 0.040 * th), up=nk, k=0.03))  # Strecker
    skin.append(A.ell(bt, H + dt * lt * 0.45 + nk * 0.014 * th, dt, (lt * 0.34, 0.040 * th, 0.034 * th), up=nk, k=0.03))  # Beuger
    skin.append(A.ell(bs, K - nk * 0.034 * th + dt * 0.004, dt, (0.026, 0.024, 0.014), up=-nk, k=0.012))  # Kniescheibe
    for s_ in (1, -1):
        skin.append(A.ell(bt, K + lat * s_ * 0.030 * th - dt * 0.012, dt, (0.026, 0.016, 0.020), up=nk, k=0.014))
    ns = orth(-dt, ds)
    skin.append(A.cone(bs, K, Fo, 0.034 * th, 0.021 * th))
    skin.append(A.ell(bs, K + ds * ls * 0.30 + ns * 0.016 * th, ds, (ls * 0.24, 0.036 * th, 0.032 * th), up=ns, k=0.025))  # Wade
    skin.append(A.cone(bs, K + ds * 0.04 - ns * 0.022, Fo - ds * 0.03 - ns * 0.016, 0.009, 0.007, k=0.012))  # Schienbein
    fd = A.unit(np.array([ds[0], ds[1], 0.0]))
    sm = 1.0 if float(np.cross(Z, fd) @ np.array([0, -sgn, 0])) > 0 else -1.0
    skin += foot(Fo, fd, Z, bs, sm)
    return skin


# --- tote Glieder auf der Masse -----------------------------------------------------------------------------------
def walk(u0, h0, lengths, turns, off, off0=None):
    """Weg auf der Fuellmasse: Start in Richtung u0, Kurs h0 (rad, 0 = Osten, pi/2 = Norden), Abschnitte mit
    Knick am Anfang (turns, rad). Rueckgabe: Gelenke, dichte Punkte je Abschnitt, letzte Richtung."""
    u0 = A.unit(u0)
    east = A.unit(np.cross(Z, u0)) if abs(u0[2]) < 0.97 else np.array([0.0, 1.0, 0.0])
    north = np.cross(u0, east)
    t = east * math.cos(h0) + north * math.sin(h0)
    off0 = off if off0 is None else off0
    p = surf(u0, off0)
    joints, dense = [p], []
    for si, (L, turn) in enumerate(zip(lengths, turns)):
        n = A.unit(p - C)
        t = orth(rot(t, n, turn), n)
        seg = [p]
        for st in range(5):
            q = p + t * (L / 5)
            u = A.unit(q - C)
            if u[2] < -0.22:  # nicht unter die Masse (dort liegt das Maul, darunter der Boden)
                u[2] = -0.22
                u = A.unit(u)
            q = surf(u, off if si or st >= 1 else off0 + (off - off0) * 0.6)  # aus der Masse auftauchen
            t = orth(q - p, A.unit(q - C))
            p = q
            seg.append(p)
        joints.append(p)
        dense.append(seg)
    return joints, dense, t


def tangent_at(u, h):
    u = A.unit(u)
    east = A.unit(np.cross(Z, u)) if abs(u[2]) < 0.97 else np.array([0.0, 1.0, 0.0])
    north = np.cross(u, east)
    return east * math.cos(h) + north * math.sin(h)


def dead_arm(az, el, head, Lu, Lf, turn, r, hs, child=False):
    """Toter Arm: die Schulter steckt in der Masse, der Ellbogen liegt auf ihr, der Unterarm legt sich ueber sie,
    die Hand krallt sich hinein. Gerade Knochen mit derselben Anatomie wie die lebenden Arme (Runde 3: vorher
    Kegelketten - sie lasen sich als Wuerste)."""
    u0 = A.unit(sph(az, el))
    t0 = tangent_at(u0, math.radians(head))
    S = surf(u0, -0.07)
    E = surf(u0 + t0 * (Lu / 0.24), r * 0.55)
    nE = A.unit(E - C)
    t1 = orth(rot(orth(E - S, nE), nE, math.radians(turn)), nE)
    W = surf(A.unit(E - C) + t1 * (Lf / 0.24), r * 0.45)
    nW = A.unit((W - C) / CORE_R ** 2)
    hd = orth(W - E, nW)
    sc = 0.62 if child else 1.05
    T = W + hd * 0.19 * sc
    curl = [(28, 52, 40), (24, 50, 38), (30, 55, 42), (34, 58, 44)]  # die Hand krallt sich in die Masse
    # Runde 7: tote Glieder ausgezehrt (sehnig, flache Muskeln) - zwei pralle Oberarme nebeneinander lasen sich als Gesaess
    return arm(S, E, W, T, (CORE, CORE, CORE), r / 0.052, hs, nW, floor=None, gaunt=True, root=None, sc=sc, curl=curl)


def dead_leg(az, el, head, Lt, Ls, turn, r, sm):
    """Totes Bein: Huefte in der Masse, Knie auf ihr, der Unterschenkel ragt schraeg heraus, der Fuss steht ab."""
    u0 = A.unit(sph(az, el))
    t0 = tangent_at(u0, math.radians(head))
    H = surf(u0, -0.08)
    K = surf(u0 + t0 * (Lt / 0.24), r * 0.5)
    nK = A.unit(K - C)
    t1 = orth(rot(orth(K - H, nK), nK, math.radians(turn)), nK)
    Fo = K + A.unit(t1 * 0.75 + nK * 0.65) * Ls     # der Unterschenkel hebt sich von der Masse ab
    th = r / 0.060  # Runde 7: schmaler
    skin = leg_parts(H, K, Fo, CORE, CORE, th)
    fd = orth(nK * 0.6 + t1 * 0.4, A.unit(Fo - K))
    skin += foot(Fo, fd, A.unit(Fo - K), CORE, sm, sc=0.95)  # Sohle in Richtung des Schienbeins (wie im Stand)
    return skin


def leg_parts(H, K, Fo, bt, bs, th):
    """Oberschenkel, Knie, Unterschenkel (ohne Fuss)."""
    skin = []
    dt, ds = A.unit(K - H), A.unit(Fo - K)
    lt, ls = norm(K - H), norm(Fo - K)
    nk = orth(ds, dt)
    lat = A.unit(np.cross(dt, nk))
    skin.append(A.cone(bt, H, K, 0.050 * th, 0.038 * th))
    skin.append(A.ell(bt, H + dt * lt * 0.50 - nk * 0.012 * th, dt, (lt * 0.40, 0.044 * th, 0.040 * th), up=nk, k=0.05))  # Strecker
    skin.append(A.ell(bt, H + dt * lt * 0.45 + nk * 0.010 * th, dt, (lt * 0.36, 0.040 * th, 0.034 * th), up=nk, k=0.05))  # Beuger
    skin.append(A.ell(bs, K - nk * 0.030 * th + dt * 0.004, dt, (0.026, 0.024, 0.014), up=-nk, k=0.02))  # Kniescheibe
    for s_ in (1, -1):
        skin.append(A.ell(bt, K + lat * s_ * 0.028 * th - dt * 0.012, dt, (0.026, 0.016, 0.020), up=nk, k=0.025))
    ns = orth(-dt, ds)
    skin.append(A.cone(bs, K, Fo, 0.036 * th, 0.022 * th))
    skin.append(A.ell(bs, K + ds * ls * 0.28 + ns * 0.012 * th, ds, (ls * 0.28, 0.034 * th, 0.030 * th), up=ns, k=0.05))  # Wade
    skin.append(A.cone(bs, K + ds * 0.04 - ns * 0.020 * th, Fo - ds * 0.03 - ns * 0.014 * th, 0.009, 0.007, k=0.02))  # Schienbein
    return skin


# --- Wesen ----------------------------------------------------------------------------------------------------------
def build(J, bones):
    tone_parts = {t: [] for t in TONES}
    nails_by_hand = []
    core, teeth, throat, eyeballs, irises = [], [], [], [], []

    # Fuellmasse: flacher Haufen, unten vorn ein Lappen fuer das Maul
    core += [
        A.ell(CORE, C, (1, 0, 0), CORE_R, k=0.0),
        A.ell(CORE, (0.06, 0.0, 0.19), (1, 0, 0), (0.14, 0.14, 0.12), k=0.12),   # Unterseite mit dem Maul
    ]

    # lebende Arme
    tone_of_arm = (0, 1, 2, 0, 2, 1, 0)
    for a in range(7):
        S, E, W, T = (np.asarray(J[4 + 4 * a + i]) for i in range(4))
        bb = (2 + 3 * a, 3 + 3 * a, 4 + 3 * a)
        back_hint = bones[bb[2]]['frame'][:3, 2]
        lure = a == 0
        th = 0.86 if lure else (1.0 + 0.14 * ((a * 37) % 5) / 4)
        skin, nails = arm(S, E, W, T, bb, th, 1 if a % 2 == 0 else -1, back_hint, floor=0.004, gaunt=lure, root=CORE,
                          sc=1.3, flen=1.35 if lure else 1.0)
        tone_parts[TONES[tone_of_arm[a]]].append((f'arm{a}', skin))
        nails_by_hand.append(nails)

    # Beine (schleifen nach)
    for l in range(2):
        H, K, Fo = (np.asarray(J[32 + 3 * l + i]) for i in range(3))
        tone_parts['clump_b'].append((f'bein{l}', leg(H, K, Fo, (23 + 2 * l, 24 + 2 * l), 1.0, 1 if l == 0 else -1, root=CORE)))

    # tote, verwachsene Glieder: (Azimut, Hoehe, Kurs in Grad, Ober-, Unterlaenge, Knick in Grad, Radius, Ton, Art)
    # Runde 1: doppelt so viele und dicker - die Masse soll aus Gliedern bestehen, nicht aus einer Kugel
    DEAD = [
        (0.00, 1.20, 200, 0.30, 0.27, 80, 0.052, 0, 'arm'),
        (1.80, 0.85, 330, 0.31, 0.26, -95, 0.056, 1, 'arm'),
        (-2.60, 0.70, 60, 0.36, 0.30, 100, 0.066, 2, 'leg'),
        (3.10, 0.20, 100, 0.28, 0.25, -70, 0.054, 0, 'arm'),
        (-1.30, 0.95, 170, 0.30, 0.26, 85, 0.050, 2, 'arm'),
        (1.45, 0.15, 20, 0.26, 0.24, 90, 0.052, 0, 'arm'),
        (-0.30, 0.75, 260, 0.34, 0.30, -110, 0.064, 1, 'leg'),
        (2.40, 0.30, 290, 0.29, 0.25, 75, 0.050, 2, 'arm'),
        (-2.00, 0.40, 140, 0.30, 0.26, -90, 0.054, 1, 'arm'),
        (0.95, 1.30, 120, 0.17, 0.15, 60, 0.032, 0, 'child'),
        (-0.90, 0.25, 10, 0.30, 0.27, 95, 0.055, 0, 'arm'),
        (2.00, 1.25, 250, 0.29, 0.25, -85, 0.050, 2, 'arm'),
        (-1.70, 1.35, 30, 0.33, 0.28, 105, 0.060, 1, 'leg'),
        (0.50, 0.60, 300, 0.27, 0.24, -80, 0.048, 1, 'arm'),
        (2.90, 0.95, 160, 0.30, 0.26, 90, 0.054, 0, 'arm'),
        (-2.90, 0.15, 290, 0.28, 0.24, -75, 0.050, 2, 'arm'),
        # Runde 2: unten herum, damit kein nackter Bauch unter dem Knaeuel bleibt
        (0.70, -0.20, 180, 0.28, 0.24, 70, 0.052, 2, 'arm'),
        (-0.75, -0.20, 0, 0.28, 0.24, -70, 0.052, 0, 'arm'),
        (2.30, -0.15, 200, 0.30, 0.26, 80, 0.058, 1, 'leg'),
        (-2.20, -0.15, 340, 0.27, 0.24, -80, 0.050, 2, 'arm'),
    ]
    for i, (az, el, hd_, lu, lf, turn, r, tone, kind) in enumerate(DEAD):
        if kind == 'leg':
            skin = dead_leg(az, el, hd_, lu, lf, turn, r, 1 if i % 2 else -1)
            nails = []
        else:
            skin, nails = dead_arm(az, el, hd_, lu, lf, turn, r, 1 if i % 2 else -1, child=kind == 'child')
        tone_parts[TONES[tone]].append((f'tot{i}', skin))
        if nails:
            nails_by_hand.append(nails)

    # Augen (drei, schief) mit Lidern, Iris dunkel - Runde 4: auf der aeussersten Flaeche (Masse und Glieder) in einem
    # fleischigen Wulst, der zwischen den Gliedern hervortritt (vorher lagen sie unter toten Armen begraben)
    solid = [p for p in core if p.op == 'add'] + [p for t in TONES for _, ps in tone_parts[t] for p in ps if p.op == 'add']

    def outer(u):
        ts = np.arange(0.8, 0.0, -0.002)
        d = sdf.sample(solid, C + np.outer(ts, u))
        return C + u * ts[int(np.argmax(d < 0))]

    for u, size, roll, shut in (((0.735, 0.522, 0.435), 1.0, 0.25, 0.30), ((0.709, -0.66, 0.247), 1.12, -0.70, 0.15),
                                ((-0.462, 0.278, 0.841), 0.78, 1.10, 0.55)):
        u = A.unit(u)
        n = u
        core.append(A.ell(CORE, outer(u) - n * 0.004, orth(Z, n), (0.040 * size, 0.034 * size, 0.022), up=n, k=0.02))  # Wulst
        p = outer(u) + n * 0.014
        e0 = orth(Z, n)
        up = rot(e0, n, roll)
        sd = np.cross(up, n)
        r = 0.0125 * size
        core.append(A.ell(CORE, p + n * 0.004, sd, (r * 1.9, r * 1.25, r * 1.1), up=n, op='sub', k=0.006))   # Hoehle
        eyeballs.append([A.ell(CORE, p - n * r * 0.25, sd, (r, r, r), up=n)])
        irises.append([A.ell(CORE, p + n * r * 0.70, sd, (r * 0.46, r * 0.46, r * 0.12), up=n)])
        core.append(A.ell(CORE, p + n * r * 0.55 + up * r * (0.95 - shut * 0.9), sd, (r * 1.55, r * 0.75, r * 0.42), up=n, k=0.004))  # Oberlid
        core.append(A.ell(CORE, p + n * r * 0.45 - up * r * 0.95, sd, (r * 1.45, r * 0.45, r * 0.32), up=n, k=0.004))  # Unterlid

    # ein Ohr, schief in der Masse
    u = A.unit((-0.028, -0.955, 0.296))
    n = u
    core.append(A.ell(CORE, outer(u) - n * 0.006, orth(Z, n), (0.045, 0.035, 0.020), up=n, k=0.02))  # Wulst
    p = outer(u) + n * 0.010
    upe = rot(orth(Z, n), n, 0.5)
    fwe = np.cross(n, upe)
    c = p + n * 0.006
    Rf = np.stack([-fwe, upe, n], axis=1)  # Torus-Achse = Flaechennormale, Bogen nach hinten offen nach vorn
    core.append(A.Part('torus', CORE, k=0.006, c=c, R=Rf, major=0.022, minor=0.0055, arc=2.3))      # Helix
    core.append(A.ell(CORE, c - n * 0.002, upe, (0.026, 0.018, 0.006), up=n, k=0.008))              # Ohrmuschel
    core.append(A.ell(CORE, c + n * 0.005 + fwe * 0.004 - upe * 0.004, upe, (0.011, 0.009, 0.006), up=n, op='sub', k=0.004))
    core.append(A.ell(CORE, c - upe * 0.026 + fwe * 0.002, upe, (0.009, 0.008, 0.005), up=n, k=0.006))  # Ohrlaeppchen
    core.append(A.ell(CORE, c + fwe * 0.016 - upe * 0.006 + n * 0.002, upe, (0.006, 0.004, 0.004), up=n, k=0.004))  # Tragus

    # Maul: quer liegende Spalte unten vorn, Lippenwuelste, zwei Reihen Zaehne, dunkler Schlund
    ys = np.linspace(-0.12, 0.12, 25)
    op_of = lambda y: 0.004 + 0.016 * (1 - (y / 0.12) ** 2)
    X = np.array([1.0, 0, 0])
    for i in range(len(ys) - 1):
        ya, yb = ys[i], ys[i + 1]
        pa, pb = np.array([0.07, ya, 0.07]), np.array([0.07, yb, 0.07])
        oa, ob = op_of(ya), op_of(yb)
        core.append(A.tube(CORE, pa, pb, (oa, 0.055), (ob, 0.055), op='sub', k=0.004))
        for sx in (1, -1):
            core.append(A.tube(CORE, pa + X * sx * (oa + 0.008) + Z * 0.006, pb + X * sx * (ob + 0.008) + Z * 0.006,
                               (0.010, 0.011), (0.010, 0.011), k=0.006))                                # Lippenwulst
    rng = np.random.default_rng(5)
    for sx in (1, -1):
        for row in (0, 1):
            cnt = 22 if row == 0 else 16
            for i in range(cnt):
                y = -0.105 + 0.21 * (i + 0.5 + 0.3 * (rng.random() - 0.5)) / cnt
                o = op_of(y)
                base = np.array([0.07 + sx * (o + 0.001 + 0.006 * row), y, 0.080 + 0.016 * row])
                d = A.unit(np.array([-sx * 0.75, 0.15 * (rng.random() - 0.5), 0.65]))
                L = (0.020 + 0.014 * rng.random()) * (0.75 if row else 1.0) * (1 - 0.5 * (y / 0.12) ** 2)
                if rng.random() < 0.1:
                    L *= 0.45
                teeth.append(A.cone(CORE, base, base + d * L, 0.0034, 0.0003, group='tooth'))
    throat.append(A.ell(CORE, (0.07, 0.0, 0.135), (1, 0, 0), (0.035, 0.115, 0.05)))

    # --- Verschiebungen ---------------------------------------------------------------------------------------
    def core_disp(Q):
        # Runde 1: keine Zellnaehte mehr (lasen sich als Fussballmuster) - Falten entstehen an den Gliedern
        d = -0.0016 * (sdf.ridged(Q * np.array([1.0, 1.0, 0.6]), 24, 2, seed=72) - 0.5)  # gespannte, runzlige Haut
        d -= 0.006 * sdf.fbm(Q, 6, 2, seed=73)                            # Beulen
        mq = Q - np.array([0.07, 0.0, 0.07])
        rr = np.sqrt(mq[:, 0] ** 2 + (mq[:, 1] * 0.6) ** 2 + mq[:, 2] ** 2)
        ang = np.arctan2(mq[:, 1] * 0.6, mq[:, 0])
        d -= 0.0022 * np.cos(ang * 15 + 3 * sdf.fbm(Q, 20, 1, seed=75)) * np.exp(-((rr - 0.07) / 0.04) ** 2)  # Falten um das Maul
        d -= 0.0004 * sdf.fbm(Q, 90, 2, seed=74)                          # Poren
        return d

    def limb_disp(Q):
        d = -0.0004 * sdf.fbm(Q, 90, 2, seed=81)                          # Poren
        d -= 0.0010 * np.clip((sdf.ridged(Q, 28, 2, seed=82) - 0.78) * 5, 0, 1)  # Adern
        d -= 0.0004 * (sdf.ridged(Q, 60, 1, seed=83) - 0.5)               # Hautfalten
        return d

    core_add = [p for p in core if p.op == 'add']
    limb_add = [p for t in TONES for _, ps in tone_parts[t] for p in ps if p.op == 'add']
    groups = [{'name': 'kern', 'fm': 'clump_core', 'parts': core, 'disp': core_disp, 'h': 0.0030, 'cav_scale': 0.016,
               'cav_extra': limb_add}]
    for t in TONES:
        for name, ps in tone_parts[t]:
            groups.append({'name': name, 'fm': t, 'parts': ps, 'disp': limb_disp, 'h': 0.0021, 'cav_scale': 0.010,
                           'cav_extra': core_add, 'merge': 'glieder_' + t[-1]})
    for i, ns in enumerate(nails_by_hand):
        groups.append({'name': f'naegel{i}', 'fm': 'ent_horn', 'parts': ns, 'disp': None, 'h': 0.0006, 'cav_scale': 0.003,
                       'merge': 'naegel'})
    groups += [
        {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.0007, 'cav_scale': 0.003},
        {'name': 'schlund', 'fm': 'ent_mouth', 'parts': throat, 'disp': None, 'h': 0.0030, 'cav_scale': 0.01},
    ]
    for i, (eb, ir) in enumerate(zip(eyeballs, irises)):  # je Auge ein kleines Gitter, danach ein Netz
        groups.append({'name': f'auge{i}', 'fm': 'ent_tooth', 'parts': eb, 'disp': None, 'h': 0.0008, 'cav_scale': 0.004,
                       'merge': 'augen'})
        groups.append({'name': f'iris{i}', 'fm': 'ent_darkskin', 'parts': ir, 'disp': None, 'h': 0.0005, 'cav_scale': 0.002,
                       'merge': 'iris'})
    return groups
