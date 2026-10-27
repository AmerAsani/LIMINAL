"""V13.3: Growler (Entity 130, Wikidot trimmed:entity-130, Rev. 67) - KREATUR_Growler.md.

Eine riesige, grob menschenaehnliche Masse aus bunten Pfeifenreinigern, staendig vornuebergebeugt. Kein Kopf: auf der Brust
ein Knoten, aus dem die Kabel sternfoermig auseinanderlaufen (Level-1-Seite). Kurze Beine, lange Arme; alle Glieder enden in
einem Gewirr aus Draht und Pfeifenreinigern, das nachschleift. Am Rumpf Buendel kurzer, spitzer Drahtrippen (fahren beim
Angriff aus), drei Peitschenranken am Buckel. Die Brust laesst sich oeffnen: dahinter dunkles Gewirr und das einzige Organ,
ein blasser Nervenstrang.

Bau: ein dunkler Kern (verfilzter Draht, ent_hair) traegt die Form; darueber Hunderte einzelner Pfeifenreiniger (Rohrketten),
die der Oberflaeche folgen (je Strang ein Gang ueber die Flaeche: quer, schraeg, laengs - durcheinander wie gewickelt), in vier
Farben, die Enden stehen ab und zeigen den blanken Draht (ent_wire). Gruppen je Koerperteil und Farbe (kleine Gitter), danach
je Farbe ein Netz.

Knochen (ent_growler, gw in src/anim/creature.hpp): 1 Becken, 2 Ruecken, 3 Buckel, 4 Knoten, 5/6 Brustklappen, 7..12
Drahtrippen, 13 + 6s + i Arm (Oberarm, Unterarm, Gewirr 1..4), 25 + 5s + i Bein (Oberschenkel, Unterschenkel, Fuss,
Gewirr 1, 2), 35 + 4k + i Ranken.
"""
import math

import numpy as np

import anat as A
import sdf

LODS = (100000, 30000, 9000)
try:  # Vorschaufarben (build.py laeuft als __main__)
    import __main__
    __main__.PREVIEW_RGB.update({'ent_pipe_a': (0.85, 0.15, 0.5), 'ent_pipe_b': (0.12, 0.62, 0.8), 'ent_pipe_c': (0.88, 0.75, 0.12),
                                 'ent_pipe_d': (0.4, 0.8, 0.15), 'ent_wire': (0.7, 0.7, 0.68), 'ent_solifuge': (0.55, 0.42, 0.28),
                                 'ent_silk': (0.85, 0.84, 0.8), 'ent_sedative': (0.85, 0.82, 0.62)})
except Exception:
    pass
COLORS = ('ent_pipe_a', 'ent_pipe_b', 'ent_pipe_c', 'ent_pipe_d')  # Magenta, Tuerkis (beide leuchten bei den Jungen), Gelb, Gruen
H = 0.005       # Voxel der Straenge
RS = 0.021      # Radius eines Pfeifenreinigers (Runde 1: 0,016 - zu duenn, der Kern schaute ueberall durch)
# Gelenke und Knochen (siehe Kopf)
ROOT, PELVIS, SPINE, CHEST, HUMP, KNOT = 0, 1, 2, 3, 4, 5
DOORH = (6, 8)
SPK0 = 10
ARM0, LEG0, TEND0 = 22, 36, 48
B_PELVIS, B_SPINE, B_HUMP, B_KNOT = 1, 2, 3, 4
B_DOOR = (5, 6)
B_SPK0, B_ARM0, B_LEG0, B_TEND0 = 7, 13, 25, 35
SPK = [((0.50, 0.26, 1.48), (0.7, 0.7, 0.12)), ((0.50, -0.26, 1.48), (0.7, -0.7, 0.12)), ((0.36, 0.34, 1.12), (0.5, 0.86, 0.0)),
       ((0.36, -0.34, 1.12), (0.5, -0.86, 0.0)), ((0.32, 0.22, 0.92), (0.62, 0.6, -0.3)), ((0.32, -0.22, 0.92), (0.62, -0.6, -0.3))]
Z = np.array([0.0, 0.0, 1.0])


def armJ(s, i):
    return ARM0 + s * 7 + i


def armB(s, i):
    return B_ARM0 + s * 6 + i


def legJ(s, i):
    return LEG0 + s * 6 + i


def legB(s, i):
    return B_LEG0 + s * 5 + i


def tendJ(k, i):
    return TEND0 + k * 5 + i


def tendB(k, i):
    return B_TEND0 + k * 4 + i


def seg_dist(p, a, b):
    ab = b - a
    t = np.clip(float((p - a) @ ab) / max(float(ab @ ab), 1e-12), 0.0, 1.0)
    return float(np.linalg.norm(p - (a + ab * t)))


class Body:
    """Kern (Form) und Werkzeuge, um Straenge ueber seine Oberflaeche zu legen."""

    def __init__(self, J, rng):
        self.J = np.asarray(J, dtype=np.float64)
        self.rng = rng
        J = self.J
        c = []
        # Rumpf: Huefte, Bauch, Brust, Buckel - gebeugt, die Brust vorn ueber dem Bauch
        c.append(A.ell(B_PELVIS, (-0.05, 0, 0.84), (1, 0, 0), (0.29, 0.31, 0.22), k=0.0))
        c.append(A.ell(B_PELVIS, (0.10, 0, 1.12), (1, 0, 0), (0.34, 0.34, 0.27), k=0.08))
        c.append(A.ell(B_SPINE, (0.22, 0, 1.46), A.unit((0.4, 0, 1)), (0.40, 0.42, 0.32), k=0.10))
        c.append(A.ell(B_HUMP, (0.08, 0, 1.80), A.unit((1, 0, 0.3)), (0.38, 0.44, 0.27), k=0.10))
        c.append(A.ell(B_HUMP, (0.00, 0, 1.94), (1, 0, 0), (0.22, 0.30, 0.14), k=0.08))
        for s in (0, 1):
            sg = 1 if s == 0 else -1
            c.append(A.ell(B_HUMP, (0.18, sg * 0.40, 1.74), (1, 0, 0), (0.18, 0.17, 0.17), k=0.08))  # Schulter
            S, E, W = J[armJ(s, 0)], J[armJ(s, 1)], J[armJ(s, 2)]
            c.append(A.cone(armB(s, 0), S, E, 0.13, 0.095, k=0.05))
            c.append(A.cone(armB(s, 1), E, W, 0.095, 0.065, k=0.03))
            Hh, K, An, T = J[legJ(s, 0)], J[legJ(s, 1)], J[legJ(s, 2)], J[legJ(s, 3)]
            c.append(A.cone(legB(s, 0), Hh, K, 0.16, 0.12, k=0.06))
            c.append(A.cone(legB(s, 1), K, An, 0.12, 0.085, k=0.03))
            c.append(A.ell(legB(s, 2), (An + T) / 2 + Z * 0.0, T - An, (0.13, 0.08, 0.06), k=0.03))
        # Hoehle hinter den Brustklappen
        self.closed = list(c)  # ohne Hoehle (fuer die Brustklappen)
        self.cavity = A.ell(B_SPINE, (0.44, 0, 1.17), (1, 0, 0), (0.16, 0.20, 0.20), op='sub', k=0.03)
        c.append(self.cavity)
        self.core = c
        self.use_closed = False

    def dist(self, P):
        return sdf.sample(self.closed if self.use_closed else self.core, np.atleast_2d(P))

    def grad(self, p):
        e = 0.004
        P = np.array([p + [e, 0, 0], p - [e, 0, 0], p + [0, e, 0], p - [0, e, 0], p + [0, 0, e], p - [0, 0, e]])
        d = self.dist(P)
        g = np.array([d[0] - d[1], d[2] - d[3], d[4] - d[5]])
        return A.unit(g if np.linalg.norm(g) > 1e-9 else [0, 0, 1])

    def project(self, p, off, iters=4):
        for _ in range(iters):
            d = float(self.dist(p)[0])
            p = p - self.grad(p) * (d - off)
        return p

    def walk(self, p0, axis, pitch, length, off, step=0.035, wobble=0.25, limit=None):
        """Strang ueber die Oberflaeche: Richtung = um die Achse herum (quer), um pitch zur Achse gekippt."""
        rng = self.rng
        p = self.project(np.asarray(p0, dtype=np.float64), off)
        pts = [p]
        ang = pitch
        n_steps = max(2, int(length / step))
        for i in range(n_steps):
            n = self.grad(p)
            around = np.cross(axis, n)
            if np.linalg.norm(around) < 1e-6:
                around = A.perp(n)
            around = A.unit(around)
            along = A.unit(np.cross(n, around))
            ang += rng.normal(0, wobble * 0.35)
            d = A.unit(around * math.cos(ang) + along * math.sin(ang))
            q = self.project(p + d * step, off + 0.004 * math.sin(i * 1.7 + rng.random() * 6.0))
            if limit is not None and not limit(q):
                break
            pts.append(q)
            p = q
        return pts


def tube_chain(bone_of, pts, r, k=0.0, taper=0.0):
    parts = []
    n = len(pts)
    for i in range(n - 1):
        ra = r * (1.0 - taper * i / max(1, n - 1))
        rb = r * (1.0 - taper * (i + 1) / max(1, n - 1))
        parts.append(A.tube(bone_of((pts[i] + pts[i + 1]) * 0.5), pts[i], pts[i + 1], (ra, ra), (rb, rb), k=k))
    return parts


def frayed_end(pts, n_out, rng, r, bone):
    """Ende eines Strangs: steht ab; der blanke Draht schaut 1-3 cm heraus. Rueckgabe: (Teile Strang, Teile Draht)."""
    a, b = pts[-2], pts[-1]
    d = A.unit(b - a)
    out = A.unit(d * 0.6 + n_out * 0.8 + rng.normal(0, 0.25, 3))
    q = b + out * rng.uniform(0.03, 0.08)
    wire_tip = q + A.unit(out + rng.normal(0, 0.2, 3)) * rng.uniform(0.012, 0.03)
    return ([A.tube(bone, b, q, (r, r), (r * 0.85, r * 0.85))],
            [A.cone(bone, q - out * 0.004, wire_tip, r * 0.22, r * 0.06)])


def nearest_bone(segs):
    def f(p):
        best, bb = 1e9, segs[0][0]
        for b, a, c in segs:
            d = seg_dist(p, a, c)
            if d < best:
                best, bb = d, b
        return bb
    return f


def build(J, bones):
    rng = np.random.default_rng(130)
    J = np.asarray(J, dtype=np.float64)
    body = Body(J, rng)
    torso_segs = [(B_PELVIS, J[PELVIS] - Z * 0.3, J[SPINE]), (B_SPINE, J[SPINE], J[CHEST]), (B_HUMP, J[CHEST], J[HUMP])]
    for s in (0, 1):
        torso_segs.append((B_HUMP, J[CHEST], J[armJ(s, 0)]))
    torso_bone = nearest_bone(torso_segs)
    groups = []
    # Sammelbehaelter: (Bereich, Farbe) -> Teile; Draht je Bereich
    bins = {}
    wires = {}

    def add(region, color, parts):
        bins.setdefault((region, color), []).extend(parts)

    def add_wire(region, parts):
        wires.setdefault(region, []).extend(parts)

    def pick_color(bias=None):
        w = np.array([1.0, 1.0, 1.0, 1.0]) if bias is None else np.asarray(bias, dtype=float)
        return COLORS[int(rng.choice(4, p=w / w.sum()))]

    # --- Rumpf: drei Lagen Straenge, quer und schraeg gewickelt ---------------------------------------------------
    # nicht ueber die Brustklappen (die liegen als eigenes Geflecht ueber der Hoehle)
    in_torso = lambda q: q[2] > 0.62 and q[2] < 2.08 and not (q[0] > 0.3 and abs(q[1]) < 0.27 and 0.93 < q[2] < 1.43)
    for layer, (count, off) in enumerate(((230, RS * 0.7), (170, RS * 1.8), (90, RS * 2.9))):  # Runde 1: 95/60/30 - zu duenn
        for _ in range(count):
            z = rng.uniform(0.68, 2.0)
            az = rng.uniform(-math.pi, math.pi)
            p0 = np.array([0.1 + 0.5 * math.cos(az), 0.55 * math.sin(az), z])
            # Achse des Rumpfs: gebeugt (unten senkrecht, oben nach vorn)
            axis = A.unit(np.array([0.35 if z > 1.3 else 0.12, 0.0, 1.0]))
            pitch = rng.uniform(-1.1, 1.1) if layer < 2 else rng.uniform(-1.5, 1.5)
            pts = body.walk(p0, axis, pitch, rng.uniform(0.45, 1.0), off, limit=in_torso)
            if len(pts) < 3:
                continue
            col = pick_color()
            add('rumpf', col, tube_chain(torso_bone, pts, RS * rng.uniform(0.85, 1.1)))
            if rng.random() < 0.45:  # abstehendes Ende mit blankem Draht
                n = body.grad(pts[-1])
                sp, wp = frayed_end(pts, n, rng, RS, torso_bone(pts[-1]))
                add('rumpf', col, sp)
                add_wire('rumpf', wp)
    # --- Arme: Straenge laengs, verdreht, buendelartig ----------------------------------------------------------
    for s in (0, 1):
        S, E, W = J[armJ(s, 0)], J[armJ(s, 1)], J[armJ(s, 2)]
        arm_bone = nearest_bone([(armB(s, 0), S, E), (armB(s, 1), E, W)])
        for _ in range(80):
            t0 = rng.uniform(-0.15, 0.7)
            a0 = S + (W - S) * max(0.0, t0) + (S - J[CHEST]) * (0.25 if t0 < 0 else 0.0)
            ax = A.unit(W - S)
            off = RS * rng.choice([0.8, 1.9])
            # ueber den Arm (Achse = Armrichtung), fast laengs (pitch nahe 90 Grad)
            pitch = math.pi / 2 + rng.uniform(-0.55, 0.55)
            pts = body.walk(a0 + A.perp(ax) * 0.1, ax, pitch, rng.uniform(0.35, 0.8), off,
                            limit=lambda q, S=S, W=W: seg_dist(q, S, W) < 0.2)
            if len(pts) < 3:
                continue
            col = pick_color([1.6, 1.6, 0.8, 0.8])
            add(f'arm{s}', col, tube_chain(arm_bone, pts, RS * rng.uniform(0.8, 1.05)))
            if rng.random() < 0.3:
                sp, wp = frayed_end(pts, body.grad(pts[-1]), rng, RS, arm_bone(pts[-1]))
                add(f'arm{s}', col, sp)
                add_wire(f'arm{s}', wp)
        # Gewirr an der Hand: Kern (Filz) und viele verdrehte Straenge entlang der Kette bis zum Boden und nach hinten
        chain = [J[armJ(s, i)] for i in range(2, 7)]
        cb = nearest_bone([(armB(s, i + 1), chain[i], chain[i + 1]) for i in range(4)])
        for i in range(4):
            body.core.append(A.cone(armB(s, i + 2), chain[i], chain[i + 1], 0.06 - 0.008 * i, 0.05 - 0.008 * i, k=0.02))
        for _ in range(46):
            pts = []
            ph, rad = rng.uniform(0, 2 * math.pi), rng.uniform(0.03, 0.095)
            twist = rng.uniform(4.0, 11.0) * rng.choice([-1, 1])
            n = 14
            t0, t1 = rng.uniform(0.0, 0.3), rng.uniform(0.7, 1.0)
            for j in range(n + 1):
                t = t0 + (t1 - t0) * j / n
                f = t * 4
                i = min(int(f), 3)
                u = f - i
                c = chain[i] + (chain[i + 1] - chain[i]) * u
                d = A.unit(chain[i + 1] - chain[i])
                e1 = A.perp(d)
                e2 = np.cross(d, e1)
                a = ph + twist * t
                rr = rad * (1.0 - 0.35 * t) + 0.02 * math.sin(t * 17 + ph)
                pts.append(c + (e1 * math.cos(a) + e2 * math.sin(a)) * rr)
            for p in pts:  # nicht unter den Boden
                p[2] = max(p[2], RS * 0.9)
            col = pick_color([1.8, 1.8, 0.7, 0.7])
            add(f'gewirr{s}', col, tube_chain(cb, pts, RS * rng.uniform(0.6, 0.9)))
            if rng.random() < 0.6:
                sp, wp = frayed_end(pts, A.unit(rng.normal(0, 1, 3)), rng, RS * 0.75, cb(pts[-1]))
                add(f'gewirr{s}', col, sp)
                add_wire(f'gewirr{s}', wp)
        # einzelne blanke Draehte im Gewirr
        for _ in range(16):
            i = int(rng.integers(0, 4))
            c = chain[i] + (chain[i + 1] - chain[i]) * rng.random()
            tip = c + A.unit(rng.normal(0, 1, 3) + (chain[3] - chain[0]) * 0.5) * rng.uniform(0.08, 0.2)
            tip[2] = max(tip[2], 0.01)
            add_wire(f'gewirr{s}', [A.cone(cb(c), c, tip, 0.0035, 0.0008)])
    # --- Beine: kurz, dick umwickelt; Gewirr am Knoechel -----------------------------------------------------------
    for s in (0, 1):
        Hh, K, An = J[legJ(s, 0)], J[legJ(s, 1)], J[legJ(s, 2)]
        leg_bone = nearest_bone([(legB(s, 0), Hh, K), (legB(s, 1), K, An), (legB(s, 2), An, J[legJ(s, 3)])])
        for _ in range(70):
            a0 = Hh + (An - Hh) * rng.uniform(0, 0.9)
            ax = A.unit(An - Hh)
            pts = body.walk(a0 + A.perp(ax) * 0.1, ax, rng.uniform(-0.9, 0.9) + (math.pi / 2 if rng.random() < 0.5 else 0.0),
                            rng.uniform(0.3, 0.6), RS * rng.choice([0.8, 1.8]),
                            limit=lambda q, Hh=Hh, An=An: seg_dist(q, Hh, An) < 0.22 and q[2] > 0.03)
            if len(pts) < 3:
                continue
            add(f'bein{s}', pick_color(), tube_chain(leg_bone, pts, RS * rng.uniform(0.85, 1.05)))
        trail = [J[legJ(s, 2)], J[legJ(s, 4)], J[legJ(s, 5)]]
        tb = nearest_bone([(legB(s, 3), trail[0], trail[1]), (legB(s, 4), trail[1], trail[2])])
        for i in range(2):
            body.core.append(A.cone(legB(s, 3 + i), trail[i], trail[i + 1], 0.05 - 0.01 * i, 0.04 - 0.01 * i, k=0.02))
        for _ in range(26):
            ph, tw = rng.uniform(0, 2 * math.pi), rng.uniform(5, 12) * rng.choice([-1, 1])
            pts = []
            for j in range(11):
                t = j / 10
                c = trail[0] + (trail[1] - trail[0]) * min(1, t * 2) if t < 0.5 else trail[1] + (trail[2] - trail[1]) * (t * 2 - 1)
                d = A.unit(trail[2] - trail[0])
                e1 = A.perp(d)
                e2 = np.cross(d, e1)
                a = ph + tw * t
                p = c + (e1 * math.cos(a) + e2 * math.sin(a)) * rng.uniform(0.03, 0.06) * (1 - 0.3 * t)
                p[2] = max(p[2], RS * 0.9)
                pts.append(p)
            add(f'schleppe{s}', pick_color([1.6, 1.6, 0.8, 0.8]), tube_chain(tb, pts, RS * 0.7))
            if rng.random() < 0.5:
                _, wp = frayed_end(pts, Z, rng, RS * 0.7, tb(pts[-1]))
                add_wire(f'schleppe{s}', wp)
    # --- Ranken: drei geflochtene Peitschen, auslaufend, zerfranst --------------------------------------------------
    for k in range(3):
        pl = [J[tendJ(k, i)] for i in range(5)]
        rb = nearest_bone([(tendB(k, i), pl[i], pl[i + 1]) for i in range(4)])
        cols = [COLORS[(k + j) % 4] for j in range(3)]
        for st in range(3):
            pts = []
            for j in range(41):
                t = j / 40
                f = t * 4
                i = min(int(f), 3)
                u = f - i
                c = pl[i] + (pl[i + 1] - pl[i]) * u
                d = A.unit(pl[min(i + 1, 4)] - pl[i])
                e1 = A.perp(d, (1, 0, 0))
                e2 = np.cross(d, e1)
                a = st * 2.094 + t * 26.0
                pts.append(c + (e1 * math.cos(a) + e2 * math.sin(a)) * (0.024 * (1 - 0.6 * t)))
            add(f'ranke{k}', cols[st], tube_chain(rb, pts, 0.016, taper=0.5))
        tip = pl[4]
        for _ in range(7):  # zerfranstes Ende
            q = tip + A.unit(rng.normal(0, 1, 3) + (pl[4] - pl[3]) * 3) * rng.uniform(0.05, 0.12)
            add_wire(f'ranke{k}', [A.cone(tendB(k, 3), tip, q, 0.003, 0.0006)])
    # --- Knoten: Sternfoermig laufen die Kabel auseinander; in der Mitte ein verdrehter Wulst um ein dunkles Loch ----
    K0 = J[KNOT]
    nK = body.grad(body.project(K0, 0.0))
    for _ in range(70):
        ang = rng.uniform(0, 2 * math.pi)
        e1 = A.perp(nK)
        e2 = np.cross(nK, e1)
        d = e1 * math.cos(ang) + e2 * math.sin(ang)
        pts = [body.project(K0 + nK * 0.02, RS * 1.2)]
        p = pts[0]
        L = rng.uniform(0.14, 0.38)
        for j in range(int(L / 0.03)):
            p = body.project(p + d * 0.03 + rng.normal(0, 0.006, 3), RS * (1.2 + 0.4 * rng.random()))
            pts.append(p)
        if len(pts) < 3:
            continue
        col = pick_color()
        add('knoten', col, tube_chain(lambda q: B_KNOT, pts, RS * rng.uniform(0.7, 1.0)))
        if rng.random() < 0.5:
            _, wp = frayed_end(pts, nK, rng, RS * 0.8, B_KNOT)
            add_wire('knoten', wp)
    for j in range(16):  # verdrehter Knaeuel in der Mitte (Runde 1: Ringe - sahen aus wie ein Auge)
        n0 = A.unit(rng.normal(0, 1, 3) + nK * 0.8)
        e1 = A.perp(n0)
        e2 = np.cross(n0, e1)
        a0, span, rr = rng.uniform(0, 6.3), rng.uniform(2.5, 5.0), rng.uniform(0.045, 0.085)
        knot = [K0 + nK * 0.02 + (e1 * math.cos(a0 + span * t) + e2 * math.sin(a0 + span * t)) * rr * (1 - 0.3 * t) for t in np.linspace(0, 1, 14)]
        add('knoten', pick_color(), tube_chain(lambda q: B_KNOT, knot, RS * rng.uniform(0.7, 0.95)))
    knot_hole = [A.ell(B_KNOT, K0 + nK * 0.03, nK, (0.04, 0.03, 0.03), k=0.0)]
    # --- Brustklappen: Geflecht ueber der Hoehle ---------------------------------------------------------------------
    doors = {}
    body.use_closed = True
    for s in (0, 1):
        h0 = J[DOORH[s]]
        t0 = J[DOORH[s] + 1]
        parts = []
        for row in range(9):  # Runde 1: zu regelmaessig (wie ein Grill) - jetzt schief, verschieden dick, ausgefranst
            z = 0.97 + row * 0.054 + rng.uniform(-0.015, 0.015)
            tilt = rng.uniform(-0.06, 0.06)
            pts = [body.project(np.array([h0[0] + (t0[0] - h0[0]) * u, h0[1] + (t0[1] - h0[1]) * u, z + tilt * u + 0.01 * math.sin(u * 9 + row)]), RS * (1.2 + 0.4 * (row % 2)))
                   for u in np.linspace(-0.05, 1.05, 8)]
            add(f'klappe{s}', pick_color(), tube_chain(lambda q, s=s: B_DOOR[s], pts, RS * rng.uniform(0.75, 1.05)))
            parts.append(pts)
        for colm in range(8):
            u = rng.uniform(0.0, 1.0)
            du = rng.uniform(-0.25, 0.25)
            pts = [body.project(np.array([h0[0] + (t0[0] - h0[0]) * (u + du * t), h0[1] + (t0[1] - h0[1]) * (u + du * t), z]), RS * (1.6 + 0.6 * rng.random()))
                   for t, z in zip(np.linspace(0, 1, 9), np.linspace(0.93, 1.44, 9))]
            add(f'klappe{s}', pick_color(), tube_chain(lambda q, s=s: B_DOOR[s], pts, RS * rng.uniform(0.7, 0.95)))
            if rng.random() < 0.6:
                _, wp = frayed_end(pts, np.array([1.0, 0, 0]), rng, RS * 0.7, B_DOOR[s])
                add_wire(f'klappe{s}', wp)
        doors[s] = parts
    body.use_closed = False
    # Nervenstrang in der Hoehle (das einzige Organ) und dunkle Draehte darin
    nerve = []
    cpts = [np.array([0.34, 0.01 * math.sin(i), 0.98 + i * 0.05]) for i in range(9)]
    nerve += tube_chain(lambda q: B_SPINE if q[2] > 1.2 else B_PELVIS, cpts, 0.022)
    nerve.append(A.ell(B_SPINE, (0.35, 0, 1.24), (0, 0, 1), (0.05, 0.045, 0.04), k=0.02))  # Knoten des Nervenstrangs
    for s in (-1, 1):
        for i in range(4):
            a = np.array([0.34, 0, 1.0 + 0.09 * i])
            nerve += tube_chain(lambda q: B_SPINE, [a, a + np.array([0.04, s * 0.09, 0.03]), a + np.array([0.06, s * 0.15, -0.02])], 0.006, taper=0.5)
    # --- Drahtrippen: je Buendel 11 Spitzen, Huelle bunt, die Spitze blank ----------------------------------------
    for q, (at, dr) in enumerate(SPK):
        at, dr = np.asarray(at), A.unit(dr)
        e1 = A.perp(dr)
        e2 = np.cross(dr, e1)
        for i in range(11):
            a, r = rng.uniform(0, 2 * math.pi), rng.uniform(0.0, 0.07)
            base = at + (e1 * math.cos(a) + e2 * math.sin(a)) * r - dr * 0.2
            d = A.unit(dr + rng.normal(0, 0.18, 3))
            L = rng.uniform(0.27, 0.36)
            mid = base + d * (L * 0.72)
            tip = base + d * L
            add(f'rippe{q}', COLORS[int(rng.integers(0, 4))], [A.cone(B_SPK0 + q, base, mid, RS * 0.8, RS * 0.62)])
            add_wire(f'rippe{q}', [A.cone(B_SPK0 + q, mid - d * 0.01, tip, RS * 0.28, 0.0007)])
    # --- Gruppen -------------------------------------------------------------------------------------------------
    def strand_disp(P):  # flauschig: feine Buckel (Chenille), etwas groeber verdreht
        return -0.0016 * (sdf.vnoise(P, 260, seed=1301) * 2 - 1) - 0.0009 * (sdf.vnoise(P, 610, seed=1302) * 2 - 1)

    def core_disp(P):  # verfilzter Draht
        return -0.007 * sdf.ridged(P, 34, 3, seed=1303) - 0.003 * sdf.fbm(P, 90, 2, seed=1304)

    groups.append({'name': 'kern', 'fm': 'ent_hair', 'parts': body.core, 'disp': core_disp, 'h': 0.008, 'cav_scale': 0.03, 'lod_weight': 0.5})
    for (region, col), parts in bins.items():
        if region.startswith('rumpf') and len(parts) > 260:  # grosse Bereiche teilen (kleinere Gitter, schnellere Gewichte)
            parts = sorted(parts, key=lambda p: float(p.g['a'][2]) if 'a' in p.g else float(p.g['c'][2]))
            chunks = [parts[i:i + 260] for i in range(0, len(parts), 260)]
        else:
            chunks = [parts]
        for i, ch in enumerate(chunks):
            groups.append({'name': f'{region}_{col}_{i}', 'fm': col, 'parts': ch, 'disp': strand_disp, 'h': H, 'cav_scale': 0.012,
                           'merge': col, 'cav_extra': body.core, 'lod_weight': 1.3})
    for region, parts in wires.items():  # verstreut: jeder Draht auf eigenem kleinem Gitter, danach ein Netz
        for i, wp in enumerate(parts):
            groups.append({'name': f'draht_{region}_{i}', 'fm': 'ent_wire', 'parts': [wp], 'disp': None, 'h': 0.0015, 'cav_scale': 0.003,
                           'merge': 'draht', 'lod_weight': 1.2})
    groups.append({'name': 'nerv', 'fm': 'ent_mouth', 'parts': nerve, 'disp': lambda P: -0.0015 * sdf.ridged(P, 120, 2, seed=1305),
                   'h': 0.003, 'cav_scale': 0.008})
    groups.append({'name': 'loch', 'fm': 'ent_umbra', 'parts': knot_hole, 'disp': None, 'h': 0.004, 'cav_scale': 0.01})
    return groups
