"""V13: Griff der Hand um die Lampe loesen (gehoert zu models_hand; HAND_ANALYSE.md).

Ablauf: Querwoelbung der Mittelhand setzen -> Achse der Lampe unter die Hohlhand legen (so hoch, dass der
Handteller hoechstens COMPRESS eindringt) -> je Finger Grund-, Mittel- und Endgelenk im Raster suchen (Beugeseite
liegt an, kein Eindringen in Lampe, Handteller oder den Nachbarfinger) -> Daumen per Zufallssuche laengs auf den
Schaft legen, die Kuppe auf dem Schalter.
"""
import math

import numpy as np
from mathutils import kdtree

import models.models_hand as H

RB = H.RB
BETA = 42.0                 # Lampe zur Handlaengsachse (Grad); der Kopf zeigt zur Daumenseite
GRIP_C = (0.056, 0.003)     # die Achse laeuft unter diesem Punkt der Handflaeche
CUP = {'f2_mc': 5.0, 'f3_mc': 12.0}   # Querwoelbung: Ring- und Kleinfinger-Mittelhand beugen sich mit
ABD = {'f0': 3.0, 'f1': 0.0, 'f2': -3.0, 'f3': -7.0}  # Finger im Griff fast parallel (Spreizung der Ruhe weg)
WRIST_ULNAR, WRIST_EXT = 34.0, 14.0   # Haltung des Handgelenks (Grad)
PALM_C = (0.050, 0.004)     # Mitte der Handflaeche: ihr Fusspunkt auf der Achse ist der Ursprung der Lampe
THUMB_AHEAD = 0.055         # Daumenkuppe so weit vor der Griffmitte (m, entlang der Achse)
COMPRESS = 0.0012           # so weit darf Haut am Schaft nachgeben (m)
SWITCH_GAP = 0.0008         # Daumenkuppe ueber dem Schaft (liegt auf dem flachen Schalter)


def lamp_axis():
    b = math.radians(BETA)
    return np.array([math.cos(b), -math.sin(b), 0.0])


def qm(q):
    return np.array(q.to_matrix().to_4x4())


def chain(rig, names, quats):
    """Skinning-Matrizen einer Kette, die an der Hand (Wurzel, in Ruhe) haengt: Name -> 4x4."""
    out = {}
    hand = rig.idx['hand']
    Mp, Rp = rig.rest[hand], rig.rest[hand]
    for n, q in zip(names, quats):
        i = rig.idx[n]
        M = Mp @ (np.linalg.inv(Rp) @ rig.rest[i]) @ qm(q)
        out[n] = M @ rig.rest_inv[i]
        Mp, Rp = M, rig.rest[i]
    return out


def skin_sel(rig, S, sel):
    """Punkte sel mit den Skinning-Matrizen S (Name -> 4x4; fehlende Knochen in Ruhe)."""
    W = rig.W[sel]
    V = rig.V[sel]
    P = np.zeros_like(V)
    for b in np.nonzero(W.sum(0) > 0)[0]:
        M = S.get(rig.names[b])
        w = W[:, b][:, None]
        P += w * (V if M is None else V @ M[:3, :3].T + M[:3, 3])
    return P


def cup_matrices(rig):
    S = {}
    for n, deg in CUP.items():
        S.update(chain(rig, [n], [H.flex(deg)]))
    return S


def place_axis(rig, S_cup, a):
    """Achse so hoch wie moeglich, ohne dass der Handteller mehr als COMPRESS eindringt."""
    palm = rig.W[:, rig.idx['hand']] + sum(rig.W[:, rig.idx[f'f{k}_mc']] for k in range(4))
    sel = np.where((palm > 0.7) & (rig.V[:, 2] < -0.004))[0]
    P = skin_sel(rig, S_cup, sel)
    for zc in np.arange(-0.010, -0.060, -0.0002):
        c = np.array([GRIP_C[0], GRIP_C[1], zc])
        d, t = H.axis_dist(P, c, a)
        if d.min() >= RB - COMPRESS:
            return c
    raise RuntimeError('Achse nicht platzierbar')


def surface(P, N):
    kd = kdtree.KDTree(len(P))
    for i, p in enumerate(P):
        kd.insert(p, i)
    kd.balance()
    return kd, P, N


def inside_penalty(surf, P, margin=0.0006):
    kd, Q, NQ = surf
    cost = 0.0
    for p in P:
        co, i, dist = kd.find(p)
        if dist < 0.006:
            s = float(np.dot(p - Q[i], NQ[i]))
            if s < margin:
                cost += (margin - s) ** 2
    return cost * 4e6


def rotated_normals(rig, S, sel):
    """Normalen grob mitdrehen (Knochen mit dem groessten Gewicht)."""
    out = np.zeros((len(sel), 3))
    dom = np.argmax(rig.W[sel], axis=1)
    for k, (i, b) in enumerate(zip(sel, dom)):
        M = S.get(rig.names[b])
        n = rig.N[i]
        out[k] = n if M is None else M[:3, :3] @ n
    return out


def solve_fingers(rig, c, a, S_cup, log):
    palm = rig.W[:, rig.idx['hand']] + sum(rig.W[:, rig.idx[f'f{k}_mc']] for k in range(4))
    psel = np.where(palm > 0.8)[0]
    palm_surf = surface(skin_sel(rig, S_cup, psel), rotated_normals(rig, S_cup, psel))
    rot, surfs = {}, []
    for k, (n, mcp, deg, lens, wd, th) in enumerate(H.FING):
        names = [n + '_mc', f'{n}_1', f'{n}_2', f'{n}_3']
        bi = [rig.idx[x] for x in names[1:]]
        own = rig.W[:, bi].sum(1)
        sel_all = np.where(own > 0.5)[0]
        sel = sel_all[::3]
        dom = np.argmax(rig.W[sel][:, bi], axis=1)
        palmar = rig.N[sel][:, 2] < -0.35
        tipsel = sel[dom >= 1]
        cup = CUP.get(n + '_mc', 0.0)

        def quats(a1, a2, f3):
            return [H.flex(cup), H.flex(a1, ABD[n]), H.flex(a2), H.flex(a2 * f3)]

        def evaluate(a1, a2, f3):
            S = chain(rig, names, quats(a1, a2, f3))
            P = skin_sel(rig, S, sel)
            d, t = H.axis_dist(P, c, a)
            pen = np.maximum(0.0, (RB - COMPRESS) - d)
            cost = 4e6 * float(np.sum(pen ** 2))
            for seg, wseg in ((0, 0.4), (1, 1.0), (2, 1.0)):
                m = (dom == seg) & palmar
                if m.any():
                    gap = float(d[m].min()) - RB
                    cost += wseg * 1e6 * max(gap, 0.0) ** 2
            return cost, S

        cands = []
        for a1 in np.arange(0.0, 96.0, 5.0):
            for a2 in np.arange(5.0, 111.0, 5.0):
                for f3 in (0.5, 0.65, 0.8):
                    cands.append((evaluate(a1, a2, f3)[0], a1, a2, f3))
        cands.sort()
        fine = []
        for (_, a1, a2, f3) in cands[:6]:
            for d1 in (-2.5, 0.0, 2.5):
                for d2 in (-2.5, 0.0, 2.5):
                    for f in (f3 - 0.07, f3, f3 + 0.07):
                        fine.append((evaluate(a1 + d1, a2 + d2, f)[0], a1 + d1, a2 + d2, f))
        fine.sort()
        best = None
        for (cost, a1, a2, f3) in fine[:25]:
            S = evaluate(a1, a2, f3)[1]
            extra = inside_penalty(palm_surf, skin_sel(rig, S, tipsel))
            if surfs:
                extra += inside_penalty(surfs[-1], skin_sel(rig, S, sel), margin=0.0)
            if best is None or cost + extra < best[0]:
                best = (cost + extra, a1, a2, f3, cost, extra)
        _, a1, a2, f3, c0, ex = best
        for x, q in zip(names, quats(a1, a2, f3)):
            rot[x] = q
        log.append(f'FINGER {n}: Grund {a1:.1f}, Mittel {a2:.1f}, End {a2 * f3:.1f} Grad; Kosten {c0:.3f} + {ex:.3f}')
        S = chain(rig, names, [rot[x] for x in names])
        surfs.append(surface(skin_sel(rig, S, sel_all), rotated_normals(rig, S, sel_all)))
    return rot, surfs


def solve_thumb(rig, c, a, surfs, log, t0, seed=7):
    names = ['th_0', 'th_1', 'th_2']
    bi = [rig.idx[x] for x in names]
    own = rig.W[:, bi].sum(1)
    sel = np.where(own > 0.5)[0][::2]
    dom = np.argmax(rig.W[sel][:, bi], axis=1)
    B = H.bone_layout()
    nail = np.array([list(B[names[d]][2]) for d in dom])
    palmar = np.einsum('ij,ij->i', rig.N[sel], nail) < -0.35
    rng = np.random.default_rng(seed)
    y_rest = rig.rest[rig.idx['th_2']][:3, 1]

    def quats(x):
        return [H.flex(x[0], x[1], x[2]), H.flex(x[3]), H.flex(x[4])]

    def evaluate(x):
        S = chain(rig, names, quats(x))
        P = skin_sel(rig, S, sel)
        d, t = H.axis_dist(P, c, a)
        pen = np.maximum(0.0, (RB - COMPRESS) - d)
        cost = 4e6 * float(np.sum(pen ** 2))
        for seg, wseg, want in ((2, 1.0, SWITCH_GAP), (1, 0.35, 0.0)):
            m = (dom == seg) & palmar
            gap = float(d[m].min()) - RB
            cost += wseg * 1e6 * max(gap - want, 0.0) ** 2
        y2 = S['th_2'][:3, :3] @ y_rest
        cost += 1.0 * (1.0 - float(np.dot(y2 / np.linalg.norm(y2), a)))
        m = (dom == 2) & palmar
        k = np.argmin(d[m])
        cost += ((float(t[m][k]) - t0 - THUMB_AHEAD) / 0.006) ** 2
        cost += 1e-4 * (x[1] ** 2 + x[2] ** 2) + 2e-4 * max(0.0, x[4] - 40.0) ** 2
        return cost, S, P

    lo = np.array([-20.0, -45.0, -70.0, -5.0, -15.0])
    hi = np.array([55.0, 45.0, 70.0, 55.0, 60.0])
    X = lo + (hi - lo) * rng.random((2500, 5))
    scored = sorted(((evaluate(x)[0], tuple(x)) for x in X))
    best = scored[0]
    sig = (hi - lo) * 0.08
    starts = [s[1] for s in scored[:5]]
    for rnd in range(5):
        cand = [best]
        for x in starts:
            for _ in range(160):
                y = np.clip(np.array(x) + rng.normal(0, 1, 5) * sig, lo, hi)
                cand.append((evaluate(y)[0], tuple(y)))
        # Ueberschneidung mit Zeige- und Mittelfinger fuer die besten pruefen
        cand.sort()
        rescored = []
        for (cst, x) in cand[:12]:
            P = evaluate(np.array(x))[2]
            over = sum(inside_penalty(s, P, margin=0.0) for s in surfs[:2])
            rescored.append((cst + over, x))
        rescored.sort()
        best = rescored[0]
        starts = [best[1]]
        sig *= 0.55
    x = best[1]
    cost, S, P = evaluate(np.array(x))
    over = sum(inside_penalty(s, P, margin=0.0) for s in surfs[:2])
    log.append(f'DAUMEN: Mittelhand Beugen {x[0]:.1f} Abspreizen {x[1]:.1f} Drehen {x[2]:.1f}, Grund {x[3]:.1f}, '
               f'End {x[4]:.1f} Grad; Kosten {cost:.3f}, Ueberschneidung Finger {over:.3f}')
    return dict(zip(names, quats(x))), x


def solve(rig, log):
    a = lamp_axis()
    S_cup = cup_matrices(rig)
    c = place_axis(rig, S_cup, a)
    log.append(f'ACHSE durch ({c[0] * 1000:.1f}, {c[1] * 1000:.1f}, {c[2] * 1000:.1f}) mm, Richtung {np.round(a, 3)}')
    rot, surfs = solve_fingers(rig, c, a, S_cup, log)
    t0 = float((np.array([PALM_C[0], PALM_C[1], 0.0]) - c) @ a)
    trot, tx = solve_thumb(rig, c, a, surfs, log, t0)
    rot.update(trot)
    return rot, c, a, tx
