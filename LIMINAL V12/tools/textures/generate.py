"""V12: eigene, prozedural erzeugte Texturen (keine fremde Quelle) im Format von process.py.

damask(dst, size)  - Tapete mit Damastmuster im Ogee-Gitter: grosses, spiegelsymmetrisches Medaillon (Palmette aus
                     Blattfaechern, Kelch, Akanthus-Voluten, Knospe, Spitze) in jeder Gitterzelle, kleine Rosette an
                     den Gitterknoten, die Gitterlinien als S-Ranken mit Blaettchen. Gold auf Mahagonirot, leicht
                     erhaben (Beflockung). Versatzrapport, nahtlos kachelbar.

Aufruf ueber process.py (Eintrag mit "generator" in selection.json); allein: python tools/textures/generate.py <name> <ordner>
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


def _leaf(draw, P, a, b, w, bend=0.0, n=24, round_=False):
    """Blatt (Tropfen) vom Ansatz a zur Spitze b, groesste Breite w, seitlich um bend gebogen (Motivkoordinaten).
    round_: runder Lappen (Ellipse mit schmalem Ansatz) statt spitzem Blatt."""
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    L = math.hypot(dx, dy) or 1e-6
    nx, ny = -dy / L, dx / L
    left, right = [], []
    for k in range(n + 1):
        t = k / n
        cx, cy = ax + dx * t + nx * bend * math.sin(math.pi * t), ay + dy * t + ny * bend * math.sin(math.pi * t)
        if round_:
            q = (t - 0.58) / 0.42
            h = w * 0.5 * math.sqrt(max(0.0, 1 - q * q)) + w * 0.12 * (1 - t)
        else:
            h = w * 0.5 * math.sin(math.pi * t ** 0.75) * (1 - t) ** 0.35
        left.append(P(cx + nx * h, cy + ny * h))
        right.append(P(cx - nx * h, cy - ny * h))
    draw.polygon(left + right[::-1], fill=255)


def _band(draw, P, pts, w0, w1):
    """Ranke: Linienzug pts (Motivkoordinaten) als Band, Breite von w0 nach w1."""
    n = len(pts)
    left, right = [], []
    for k in range(n):
        p0 = pts[max(0, k - 1)]
        p1 = pts[min(n - 1, k + 1)]
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        L = math.hypot(dx, dy) or 1e-6
        nx, ny = -dy / L, dx / L
        h = (w0 + (w1 - w0) * k / (n - 1)) * 0.5
        left.append(P(pts[k][0] + nx * h, pts[k][1] + ny * h))
        right.append(P(pts[k][0] - nx * h, pts[k][1] - ny * h))
    draw.polygon(left + right[::-1], fill=255)


def _spiral(cx, cy, r0, a0, turns, sg, n=40):
    return [(cx + sg * r0 * (1 - 0.8 * t) * math.cos(a0 + sg * turns * 2 * math.pi * t),
             cy + r0 * (1 - 0.8 * t) * math.sin(a0 + sg * turns * 2 * math.pi * t)) for t in (k / (n - 1) for k in range(n))]


def _medallion(draw, P):
    """Hauptmotiv in der Zelle |x|+|y|<=1 (y nach unten), spiegelsymmetrisch."""
    for sg in (1, -1):
        # Palmette: Blattfaecher aus dem Kelch nach oben
        for (ang, ln, w) in ((0.38, 0.46, 0.13), (0.8, 0.38, 0.12), (1.2, 0.28, 0.1)):
            a = (0.0, 0.02)
            b = (sg * math.sin(ang) * ln, 0.02 - math.cos(ang) * ln)
            _leaf(draw, P, a, b, w, bend=sg * 0.07, round_=True)
            # eingerollte Blattspitze
            _band(draw, P, _spiral(b[0] + sg * 0.03, b[1] + 0.02, 0.035, -1.2, 0.7, sg), 0.022, 0.01)
        # Akanthus-Volute: vom Kelchfuss nach aussen, oben einrollend, Blattlappen aussen
        stem = [(sg * (0.06 + 0.5 * math.sin(math.pi * 0.5 * t)), 0.26 - 0.22 * t - 0.18 * t * t) for t in (k / 29 for k in range(30))]
        _band(draw, P, stem, 0.075, 0.045)
        _band(draw, P, _spiral(sg * 0.5, -0.24, 0.1, 0.4, 0.85, sg), 0.045, 0.02)
        for k in range(4):
            t = 0.2 + 0.2 * k
            x, y = stem[int(t * 29)]
            _leaf(draw, P, (x, y), (x + sg * 0.12, y + 0.1 - 0.06 * k), 0.07, bend=-sg * 0.02)
        # Kelch-Seitenblaetter nach unten aussen
        _leaf(draw, P, (sg * 0.05, 0.3), (sg * 0.3, 0.52), 0.1, bend=sg * 0.05)
        _leaf(draw, P, (sg * 0.25, 0.47), (sg * 0.42, 0.42), 0.05)
        # Ranke von der Knospe zur Gitterecke
        _band(draw, P, [(sg * (0.05 + 0.25 * t), 0.78 - 0.1 * math.sin(math.pi * t)) for t in (k / 19 for k in range(20))], 0.03, 0.015)
        # Spitze oben: zwei kleine Voluten
        _band(draw, P, _spiral(sg * 0.12, -0.72, 0.07, 1.2, 0.8, sg), 0.03, 0.012)
        # Punkte
        for (x, y, r) in ((0.62, 0.05, 0.03), (0.3, -0.62, 0.025), (0.36, 0.66, 0.025)):
            draw.ellipse([P(sg * x - r, y - r), P(sg * x + r, y + r)], fill=255)
    # Kelch (Schale) unter der Palmette
    bowl = [(0.24 * math.sin(math.pi * t), 0.06 + 0.26 * t ** 1.5) for t in (k / 24 for k in range(25))]
    draw.polygon([P(x, y) for x, y in bowl] + [P(-x, y) for x, y in bowl[::-1]], fill=255)
    draw.rectangle([P(-0.2, 0.02), P(0.2, 0.07)], fill=255)
    # Stiel und haengende Knospe
    _band(draw, P, [(0.0, 0.3 + 0.3 * t) for t in (k / 9 for k in range(10))], 0.05, 0.04)
    _leaf(draw, P, (0.0, 0.58), (0.0, 0.9), 0.19, round_=True)
    # Mittelblatt der Palmette und Spitze
    _leaf(draw, P, (0.0, 0.0), (0.0, -0.56), 0.15, round_=True)
    _leaf(draw, P, (0.0, -0.55), (0.0, -0.88), 0.11)
    draw.ellipse([P(-0.045, -0.6), P(0.045, -0.51)], fill=255)


def _carve(draw, P):
    """Feine Schnittlinien im Rot (Blattadern, Fugen zwischen den Palmettenlappen) - Zeichnung im Inneren der Flaechen."""
    def cut(pts, w):
        draw.line([P(x, y) for x, y in pts], fill=0, width=max(1, int(w)), joint='curve')
    W = 0.016 * _carve.u
    for sg in (1, -1):
        for ang in (0.17, 0.59, 1.0):
            cut([(sg * math.sin(ang) * r, 0.02 - math.cos(ang) * r) for r in (0.08 + 0.03 * k for k in range(12))], W)
        for (ang, ln) in ((0.38, 0.46), (0.8, 0.38), (1.2, 0.28)):
            cut([(sg * math.sin(ang) * r, 0.02 - math.cos(ang) * r) for r in (0.12 + (ln - 0.2) * k / 6 for k in range(7))], W * 0.6)
    cut([(0.0, -0.1 - 0.06 * k) for k in range(8)], W * 0.6)
    cut([(0.0, 0.64 + 0.03 * k) for k in range(8)], W * 0.6)
    cut([(0.18 * math.sin(math.pi * t) * 0.8, 0.14 + 0.1 * t) for t in (k / 10 for k in range(11))], W * 0.7)
    cut([(-0.18 * math.sin(math.pi * t) * 0.8, 0.14 + 0.1 * t) for t in (k / 10 for k in range(11))], W * 0.7)


def _rosette(draw, P):
    """Kleines Fuellmotiv an den Gitterknoten (zwischen vier Medaillons)."""
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        _leaf(draw, P, (0.0, 0.0), (math.cos(a) * 0.17, math.sin(a) * 0.17), 0.09)
    for k in range(4):
        a = k * math.pi / 2
        _leaf(draw, P, (0.0, 0.0), (math.cos(a) * 0.11, math.sin(a) * 0.11), 0.06)
    draw.ellipse([P(-0.04, -0.04), P(0.04, 0.04)], fill=255)


def _lattice(draw, P):
    """Ogee-Gitter: S-Ranken entlang der Zellraender (punktsymmetrisch zur Kantenmitte, damit Nachbarzellen passen)."""
    for sx in (1, -1):
        for sy in (1, -1):
            pts = []
            for k in range(41):
                t = k / 40
                b = -0.13 * math.sin(2 * math.pi * t)
                x, y = t + b * 0.7071, 1 - t + b * 0.7071
                pts.append((sx * x, sy * y))
            _band(draw, P, pts, 0.03, 0.03)
            for t in (0.22, 0.78):
                b = -0.13 * math.sin(2 * math.pi * t)
                x, y = t + b * 0.7071, 1 - t + b * 0.7071
                ox = 0.09 if t < 0.5 else -0.09
                _leaf(draw, P, (sx * x, sy * y), (sx * (x + ox), sy * (y + ox)), 0.05)


def damask(dst, size=1024, base=(98, 22, 18), gold=(168, 132, 66)):
    ss = 2  # Ueberabtastung gegen Treppen
    N = size * ss
    mask = Image.new('L', (N, N), 0)
    d = ImageDraw.Draw(mask)
    u = N / 2  # Motiveinheit: Zelle |x|+|y|<=1, zwei Zellen je Kachel (Versatzrapport)
    for (cx, cy) in ((0.25 * N, 0.25 * N), (0.75 * N, 0.75 * N), (0.75 * N, 0.25 * N), (0.25 * N, 0.75 * N)):
        main = (cx == cy)
        for ox in (-N, 0, N):
            for oy in (-N, 0, N):
                def P(x, y, ox=ox, oy=oy, cx=cx, cy=cy):
                    return (cx + ox + x * u, cy + oy + y * u)
                if main:
                    _medallion(d, P)
                    _lattice(d, P)
                    _carve.u = u
                    _carve(d, P)
                else:
                    _rosette(d, P)
    mask = mask.resize((size, size), Image.LANCZOS)
    m = np.asarray(mask, dtype=np.float32) / 255.0
    rng = np.random.default_rng(7)
    # Papier: leichte Faser- und Fleckenstruktur; Gold: feines Korn
    noise = rng.normal(0.0, 1.0, (size // 8, size // 8)).astype(np.float32)
    noise = np.asarray(Image.fromarray(((noise * 20 + 128).clip(0, 255)).astype(np.uint8)).resize((size, size), Image.BICUBIC),
                       dtype=np.float32) / 255.0 - 0.5
    fine = rng.normal(0.0, 1.0, (size, size)).astype(np.float32) * 0.03
    paper = np.stack([np.full((size, size), c / 255.0, np.float32) for c in base], -1) * (1.0 + noise[..., None] * 0.25 + fine[..., None])
    gld = np.stack([np.full((size, size), c / 255.0, np.float32) for c in gold], -1) * (1.0 + fine[..., None] * 2.0)
    alb = paper * (1 - m[..., None]) + gld * m[..., None]
    # dunkle Kante um das Ornament (Beflockung wirft feinen Schatten)
    edge = np.asarray(mask.filter(ImageFilter.GaussianBlur(2.5)), dtype=np.float32) / 255.0
    alb *= (1.0 - 0.25 * np.clip(edge - m, 0, 1))[..., None]
    os.makedirs(dst, exist_ok=True)
    Image.fromarray((np.clip(alb, 0, 1) * 255).astype(np.uint8)).save(os.path.join(dst, 'albedo.jpg'), quality=93)
    # Hoehe: Ornament erhaben, weich; Normalen (DirectX: Gruen nach unten) aus der Hoehe, kachelbar (np.roll)
    h = np.asarray(mask.filter(ImageFilter.GaussianBlur(1.6)), dtype=np.float32) / 255.0 + noise * 0.08
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    k = 6.0
    nx, ny, nz = -gx * k, gy * k, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nrm = np.stack([nx / ln, ny / ln, nz / ln], -1) * 0.5 + 0.5
    Image.fromarray((nrm * 255).astype(np.uint8)).save(os.path.join(dst, 'normal.png'), optimize=True)
    ao = 1.0 - 0.3 * np.clip(edge - m, 0, 1)
    rough = 0.82 * (1 - m) + 0.42 * m
    metal = 0.35 * m
    orm = np.stack([ao, rough, metal, 0.5 + (h - 0.5) * 0.4], -1)
    Image.fromarray((np.clip(orm, 0, 1) * 255).astype(np.uint8), 'RGBA').save(os.path.join(dst, 'orm.png'), optimize=True)


GENERATORS = {'damask': damask}

if __name__ == '__main__':
    GENERATORS[sys.argv[1]](sys.argv[2])
