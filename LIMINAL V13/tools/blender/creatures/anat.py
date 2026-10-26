"""V13: Anatomie-Bausteine fuer die Wesen (sdf.Part-Listen). Alle Masse in Metern, Spielkoordinaten."""
import math

import numpy as np

from sdf import Part, frame_from


def v(x):
    return np.asarray(x, dtype=np.float64)


def lerp(a, b, t):
    return v(a) + (v(b) - v(a)) * t


def unit(x):
    x = v(x)
    return x / max(np.linalg.norm(x), 1e-12)


def perp(d, hint=(0, 0, 1)):
    """Einheitsvektor senkrecht zu d, moeglichst nahe an hint."""
    d = unit(d)
    h = v(hint) - d * float(v(hint) @ d)
    if np.linalg.norm(h) < 1e-6:
        h = np.cross(d, [0, 1, 0])
    return unit(h)


def cone(bone, a, b, ra, rb, k=0.0, group='skin', op='add'):
    return Part('cone', bone, k=k, op=op, group=group, a=v(a), b=v(b), ra=ra, rb=rb)


def ell(bone, c, axis, r, up=(0, 0, 1), k=0.0, group='skin', op='add'):
    """Ellipsoid um c: r[0] entlang axis, r[2] nach up, r[1] quer."""
    return Part('ell', bone, k=k, op=op, group=group, c=v(c), R=frame_from(axis, up), r=v(r))


def tube(bone, a, b, r0, r1=None, up=(0, 0, 1), k=0.0, group='skin', op='add'):
    """Strecke a-b mit ellipsenfoermigem Querschnitt r = (quer, hoch), hoch zeigt nach up."""
    return Part('tube', bone, k=k, op=op, group=group, a=v(a), b=v(b), up=v(up), r0=v(r0), r1=v(r0 if r1 is None else r1))


def box(bone, c, axis, half, rnd, up=(0, 0, 1), k=0.0, group='skin', op='add'):
    return Part('box', bone, k=k, op=op, group=group, c=v(c), R=frame_from(axis, up), half=v(half), round=rnd)


def muscle(bone, a, b, r_mid, offset=(0, 0, 0), flat=1.0, up=(0, 0, 1), k=0.012, t0=0.15, t1=0.85, group='skin'):
    """Muskelbauch zwischen zwei Punkten (dick in der Mitte, Sehnen an den Enden): Ellipsoid entlang a-b."""
    a, b = v(a), v(b)
    p0, p1 = lerp(a, b, t0), lerp(a, b, t1)
    c = (p0 + p1) / 2 + v(offset)
    L = np.linalg.norm(p1 - p0) / 2
    return ell(bone, c, p1 - p0, (L, r_mid, r_mid * flat), up=up, k=k, group=group)


def chain(bone_of, pts, radii, k=0.006, group='skin'):
    """Kette runder Kegel durch Punkte (Finger, Zehen, Straehnen). bone_of(i) gibt den Knochen des Abschnitts i."""
    out = []
    for i in range(len(pts) - 1):
        out.append(cone(bone_of(i), pts[i], pts[i + 1], radii[i], radii[i + 1], k=k if i else 0.0, group=group))
    return out


def bezier(p0, p1, p2, n):
    """Punkte einer quadratischen Kurve (fuer gebogene Glieder, Straehnen)."""
    out = []
    for i in range(n + 1):
        t = i / n
        out.append(v(p0) * (1 - t) ** 2 + v(p1) * 2 * t * (1 - t) + v(p2) * t * t)
    return out


def teeth_row(bone, pts, length, base_r, dirs, rng, group='tooth', jitter=0.25):
    """Zahnreihe: an jedem Punkt ein spitzer Zahn (Kegel) in Richtung dirs[i], Laenge und Neigung leicht ungleich."""
    out = []
    for i, p in enumerate(pts):
        L = length * (1 + jitter * (rng.random() * 2 - 1))
        d = unit(v(dirs[i]) + rng.normal(0, 0.12 + 0.2 * jitter, 3))
        if rng.random() < 0.12:  # abgebrochen
            L *= 0.45
        out.append(cone(bone, p, v(p) + d * L, base_r * (0.8 + 0.4 * rng.random()), base_r * 0.08, group=group))
    return out
