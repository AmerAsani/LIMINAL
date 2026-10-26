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


def _periodic_noise(size, sigma, rng):
    """Kachelbares Rauschen: weisses Rauschen, im Frequenzraum gaussgefiltert (periodisch), auf -0.5..0.5."""
    w = rng.normal(0.0, 1.0, (size, size)).astype(np.float32)
    f = np.fft.fftfreq(size).astype(np.float32)
    g = np.exp(-(f[:, None] ** 2 + f[None, :] ** 2) * (2 * math.pi * sigma) ** 2 / 2)
    n = np.real(np.fft.ifft2(np.fft.fft2(w) * g)).astype(np.float32)
    n -= n.min()
    n /= max(1e-6, n.max())
    return n - 0.5


def _save_set(dst, alb, h, rough, metal, k=6.0, ao=None):
    os.makedirs(dst, exist_ok=True)
    Image.fromarray((np.clip(alb, 0, 1) * 255).astype(np.uint8)).save(os.path.join(dst, 'albedo.jpg'), quality=93)
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
    nx, ny, nz = -gx * k, gy * k, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nrm = np.stack([nx / ln, ny / ln, nz / ln], -1) * 0.5 + 0.5
    Image.fromarray((nrm * 255).astype(np.uint8)).save(os.path.join(dst, 'normal.png'), optimize=True)
    if ao is None:
        ao = np.ones_like(h)
    orm = np.stack([ao, rough, metal, np.clip(h, 0, 1)], -1)
    Image.fromarray((np.clip(orm, 0, 1) * 255).astype(np.uint8), 'RGBA').save(os.path.join(dst, 'orm.png'), optimize=True)


def books(dst, size=1024):
    """Buchruecken in zwei Reihen je Kachel (Kachel 0,75 m -> Reihe 0,375 m = Fachhoehe der Regale): Leder in
    Rot, Braun, Gruen, Blau, Schwarz, Ocker; Goldbaender und Schild auf dem Ruecken, runde Ruecken, dunkle Luecke
    ueber den Buechern. Die Unterkante jeder Bildhaelfte ist der Fachboden (Projektion v = -z)."""
    rng = np.random.default_rng(53)
    H = size // 2
    alb = np.zeros((size, size, 3), np.float32)
    h = np.zeros((size, size), np.float32)
    rough = np.full((size, size), 0.75, np.float32)
    metal = np.zeros((size, size), np.float32)
    back = np.array([0.05, 0.035, 0.025], np.float32)
    alb[:] = back
    pal = [(0.36, 0.08, 0.06), (0.30, 0.17, 0.09), (0.12, 0.22, 0.13), (0.10, 0.13, 0.24), (0.07, 0.06, 0.05),
           (0.48, 0.34, 0.16), (0.42, 0.28, 0.20), (0.22, 0.10, 0.12), (0.55, 0.47, 0.36)]
    gold = np.array([0.72, 0.56, 0.26], np.float32)
    grain = _periodic_noise(size, 1.2, rng)
    for row in range(2):
        bottom = (row + 1) * H  # Bildzeile des Fachbodens
        x = 0
        while x < size - 20:
            w = int(rng.uniform(0.022, 0.055) / 0.75 * size)
            if x + w > size - 6:
                break
            bh = int(rng.uniform(0.2, 0.33) / 0.75 * size)
            if rng.random() < 0.08:  # Luecke im Regal
                x += int(rng.uniform(0.02, 0.06) / 0.75 * size)
                continue
            c = np.array(pal[rng.integers(len(pal))], np.float32) * rng.uniform(0.75, 1.2)
            top = bottom - bh
            xs = np.arange(w, dtype=np.float32)
            prof = np.sin(np.pi * (xs + 0.5) / w) ** 0.5  # runder Ruecken
            shade = 0.55 + 0.45 * prof
            alb[top:bottom, x:x + w] = c[None, None, :] * shade[None, :, None]
            h[top:bottom, x:x + w] = 0.5 + 0.4 * prof[None, :]
            rough[top:bottom, x:x + w] = 0.62
            # Goldbaender oben und unten, Schild
            nb = rng.integers(1, 4)
            for k in range(nb):
                for (y0, y1) in ((top + int(bh * (0.08 + 0.05 * k)), top + int(bh * (0.08 + 0.05 * k)) + 3),
                                 (bottom - int(bh * (0.08 + 0.05 * k)) - 3, bottom - int(bh * (0.08 + 0.05 * k)))):
                    alb[y0:y1, x + 1:x + w - 1] = gold * shade[1:w - 1, None]
                    metal[y0:y1, x + 1:x + w - 1] = 0.6
                    rough[y0:y1, x + 1:x + w - 1] = 0.35
                    h[y0:y1, x + 1:x + w - 1] += 0.05
            if rng.random() < 0.6 and w > 14:
                y0 = top + int(bh * 0.22)
                y1 = y0 + int(bh * rng.uniform(0.08, 0.14))
                lab = np.array(pal[rng.integers(len(pal))], np.float32) * 0.6 if rng.random() < 0.5 else gold * 0.9
                alb[y0:y1, x + 3:x + w - 3] = lab * shade[3:w - 3, None]
                if lab is not None and lab[0] > 0.5:
                    metal[y0:y1, x + 3:x + w - 3] = 0.5
            # Schatten zwischen den Buechern
            alb[top:bottom, x:x + 1] *= 0.4
            alb[top:bottom, x + w - 1:x + w] *= 0.5
            x += w
        # Schatten unter dem darueberliegenden Fachboden (oben in der Reihe dunkler)
        yy = np.arange(H, dtype=np.float32)
        fall = 0.55 + 0.45 * np.clip(yy / (H * 0.25), 0, 1)
        alb[row * H:(row + 1) * H] *= fall[:, None, None]
    alb *= (1.0 + grain[..., None] * 0.25)
    _save_set(dst, alb, h, rough, metal, k=5.0)


def tuff(dst, size=1024):
    """Tuffstein: porig, gelblich-grau, fleckig, mit kleinen dunklen Poren und feinem Korn (kachelbar)."""
    rng = np.random.default_rng(31)
    big = _periodic_noise(size, 40.0, rng)
    mid = _periodic_noise(size, 10.0, rng)
    fine = _periodic_noise(size, 1.5, rng)
    base = np.array([0.62, 0.58, 0.49], np.float32)
    alb = base[None, None, :] * (1.0 + big[..., None] * 0.35 + mid[..., None] * 0.18 + fine[..., None] * 0.12)
    alb[..., 2] *= (1.0 - big * 0.15)  # gelblicher in den hellen Flecken
    h = 0.5 + big * 0.2 + mid * 0.15 + fine * 0.1
    pores = np.zeros((size, size), np.float32)
    img = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(img)
    for _ in range(1400):
        x, y = rng.uniform(0, size), rng.uniform(0, size)
        r = rng.uniform(0.8, 3.6) * (1.0 if rng.random() > 0.06 else 2.0)
        e = rng.uniform(0.6, 1.0)
        for ox in (-size, 0, size):
            for oy in (-size, 0, size):
                d.ellipse([x + ox - r, y + oy - r * e, x + ox + r, y + oy + r * e], fill=255)
    pores = np.asarray(img.filter(ImageFilter.GaussianBlur(0.8)), dtype=np.float32) / 255.0
    alb *= (1.0 - 0.32 * pores)[..., None]
    h -= pores * 0.3
    rough = np.full((size, size), 0.92, np.float32)
    metal = np.zeros((size, size), np.float32)
    _save_set(dst, alb, h, rough, metal, k=7.0, ao=1.0 - 0.4 * pores)


GENERATORS = {'damask': damask, 'books': books, 'tuff': tuff}

if __name__ == '__main__':
    GENERATORS[sys.argv[1]](sys.argv[2])
