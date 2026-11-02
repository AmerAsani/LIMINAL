"""V13.3 (Abnahme Level 2): Atlas "ps_photos" fuer Photoshop (Entity 104) aus den Blender-Fotos zusammensetzen.

Eingang: die Renderbilder von tools/textures/ps_photos_render.py (Ordner build/qa/ps_photos). Jedes Foto wird wie ein Abzug
behandelt: Farbstich des Blitzes oder der Leuchtstoffroehre, Randabfall des Blitzes, Korn, leicht angehobenes Schwarz;
darum der weisse Rand aus Fotopapier, vergilbt, mit blutigen Fingerabdruecken und eingesogenem Blut an manchen Ecken.
Dazu das Bild des Roehrenmonitors (Fleisch mit einem Auge, Bildstoerung) und die Rueckseite des Fotopapiers.

Ausgabe im Format von process.py: data/textures/ps_photos/albedo.jpg, orm.png (R Verdeckung, G Rauheit, B Metall,
A Hoehe), normal.png (DirectX), 2048 px. Die Zellen (Pixel, oben links) stehen in CELLS - gleich in
tools/blender/models/models_l2_rand.py (PS_CELLS).

    python tools/textures/ps_photos_atlas.py [renderordner] [zielordner]
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SIZE = 2048
MEATS = ['meat_steak', 'meat_mince', 'meat_belly', 'meat_liver', 'meat_osso', 'meat_guts', 'meat_tongue', 'meat_skin']
CELLS = {'mouth': (0, 0, 1024, 768), 'eye_a': (1024, 0, 2048, 400), 'eye_b': (1024, 400, 2048, 800),
         'screen': (0, 1824, 272, 2048), 'back': (272, 1824, 528, 2048)}
for _i, _n in enumerate(MEATS):
    CELLS[_n] = ((_i % 4) * 512, 800 + (_i // 4) * 512, (_i % 4) * 512 + 512, 800 + (_i // 4) * 512 + 512)
BORDER = {'mouth': 30, 'eye_a': 20, 'eye_b': 20}


def _font(px):
    for f in ('consola.ttf', 'lucon.ttf', 'cour.ttf', 'DejaVuSansMono.ttf'):
        try:
            return ImageFont.truetype(f, px)
        except OSError:
            continue
    return ImageFont.load_default()


def develop(img, rng, cast):
    """Renderbild -> Abzug: Farbstich, Blitzabfall, Korn, Schwarz angehoben, leicht weich."""
    a = np.asarray(img.convert('RGB'), np.float32) / 255.0
    h, w = a.shape[:2]
    a = a * np.array(cast, np.float32)[None, None, :]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy = w * rng.uniform(0.42, 0.58), h * rng.uniform(0.38, 0.55)
    r2 = ((xx - cx) / (w * 0.62)) ** 2 + ((yy - cy) / (h * 0.62)) ** 2
    a *= (1.0 - 0.42 * np.clip(r2, 0, 1.6))[..., None]
    a = 0.035 + a * 0.95
    grain = np.random.default_rng(rng.randrange(1 << 30)).normal(0.0, 0.022, (h, w)).astype(np.float32)
    a += grain[..., None] * (0.6 + 0.4 * a)
    out = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))
    return out.filter(ImageFilter.GaussianBlur(0.45))


def date_stamp(img, rng):
    """Datumsaufdruck einer alten Kamera (orange Ziffern mit Leuchtsaum, unten rechts)."""
    w, h = img.size
    txt = f"'{rng.choice([91, 94, 97, 98, 99])}  {rng.randint(1, 12)}  {rng.randint(1, 28)}"
    f = _font(max(12, h // 16))
    glow = Image.new('L', img.size, 0)
    d = ImageDraw.Draw(glow)
    tw = d.textlength(txt, font=f)
    pos = (w - tw - w * 0.05, h - h * 0.11)
    d.text(pos, txt, font=f, fill=255)
    blur = glow.filter(ImageFilter.GaussianBlur(2.2))
    base = np.asarray(img, np.float32)
    col = np.array([255, 150, 40], np.float32)
    k = np.asarray(glow, np.float32)[..., None] / 255.0 * 0.95 + np.asarray(blur, np.float32)[..., None] / 255.0 * 0.5
    base = base * (1 - np.clip(k, 0, 1)) + col * np.clip(k, 0, 1)
    return Image.fromarray(np.clip(base, 0, 255).astype(np.uint8))


def fingerprint(size, rng):
    """Maske eines Fingerabdrucks: Rillen (Sinus ueber verzerrtem Abstand), oval, zum Rand ausduennend."""
    s = size
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32) / s - 0.5
    rot = rng.uniform(0, math.pi)
    u = xx * math.cos(rot) - yy * math.sin(rot)
    v = xx * math.sin(rot) + yy * math.cos(rot)
    d = np.sqrt((u / 0.32) ** 2 + (v / 0.44) ** 2)
    warp = 0.03 * np.sin(u * 17 + rng.uniform(0, 6)) + 0.02 * np.sin(v * 23)
    ridge = 0.5 + 0.5 * np.sin((np.sqrt(u * u + (v + 0.08) ** 2) + warp) * 2 * math.pi * 26)
    m = np.clip(1.2 - d, 0, 1) * (ridge > 0.45)
    m *= np.clip(np.random.default_rng(rng.randrange(1 << 30)).random((s, s)) * 1.4, 0, 1) ** 0.5
    return Image.fromarray((np.clip(m, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.7))


def card(name, src, rng, cw, ch, border):
    """Ein Abzug fuer eine Zelle: Foto innen, Papierrand aussen; Rueckgabe (RGB, Rauheit 0..1)."""
    paper = np.array([238, 234, 222], np.float32) * rng.uniform(0.93, 1.0)
    img = np.ones((ch, cw, 3), np.float32) * paper[None, None, :]
    # vergilbt und fleckig
    yy, xx = np.mgrid[0:ch, 0:cw].astype(np.float32)
    stain = 0.5 + 0.5 * np.sin(xx / cw * rng.uniform(3, 7) + rng.uniform(0, 6)) * np.sin(yy / ch * rng.uniform(2, 6) + rng.uniform(0, 6))
    img *= (1.0 - 0.07 * stain)[..., None] * np.array([1.0, 0.985, 0.94], np.float32)[None, None, :]
    rough = np.full((ch, cw), 0.38, np.float32)
    if src is not None:
        cast = rng.choice([(1.05, 0.98, 0.88), (0.94, 1.02, 0.98), (1.02, 0.97, 0.95), (1.0, 1.0, 1.0)])
        if name.startswith('eye'):  # Nahaufnahme mit schwachem Blitz: heller abziehen
            cast = tuple(c * 1.4 for c in cast)
        ph = develop(src.resize((cw - 2 * border, ch - 2 * border), Image.LANCZOS), rng, cast)
        if rng.random() < 0.45:
            ph = date_stamp(ph, rng)
        img[border:ch - border, border:cw - border] = np.asarray(ph, np.float32)
        rough[border:ch - border, border:cw - border] = 0.2
    # Blut: eingesogen an einer Ecke (Papier saugt, auf dem Bild glaenzt es) und Fingerabdruecke am Rand
    blood = np.array([92, 8, 10], np.float32)
    if rng.random() < 0.6:
        cxs, cys = rng.choice([(0, 0), (cw, 0), (0, ch), (cw, ch)])
        rr = rng.uniform(0.18, 0.38) * min(cw, ch)
        g = np.random.default_rng(rng.randrange(1 << 30))
        n = np.zeros((ch, cw), np.float32)
        for octave, amp in ((24, 0.6), (64, 0.3), (160, 0.1)):  # weiches Rauschen in drei Stufen (keine Bildpunkt-Kloetze)
            lo = g.random((max(2, ch // octave + 2), max(2, cw // octave + 2))).astype(np.float32)
            lo = Image.fromarray((lo * 255).astype(np.uint8)).resize((cw, ch), Image.BICUBIC).filter(ImageFilter.GaussianBlur(ch / octave * 0.6))
            n += (np.asarray(lo, np.float32) / 255.0 - 0.5) * amp
        d = np.sqrt((xx - cxs) ** 2 + (yy - cys) ** 2) / rr + n * 1.3
        m = np.clip((1.0 - d) * 3.0, 0, 1)
        rim = np.clip(1.0 - np.abs(1.0 - d - 0.06) * 14.0, 0, 1) * 0.5  # getrocknet dunkler am Rand
        img = img * (1 - m[..., None] * 0.82) + blood[None, None, :] * (m[..., None] * 0.82)
        img *= (1 - rim)[..., None]
        rough = rough * (1 - m) + 0.12 * m
    for _ in range(rng.randint(1, 3)):
        fs = int(min(cw, ch) * rng.uniform(0.16, 0.26))
        fm = np.asarray(fingerprint(fs, rng), np.float32) / 255.0 * rng.uniform(0.55, 0.85)
        side = rng.randrange(4)
        if side == 0:
            x0, y0 = rng.randint(0, cw - fs), rng.randint(-fs // 3, border)
        elif side == 1:
            x0, y0 = rng.randint(0, cw - fs), ch - fs + rng.randint(-border, fs // 3)
        elif side == 2:
            x0, y0 = rng.randint(-fs // 3, border), rng.randint(0, ch - fs)
        else:
            x0, y0 = cw - fs + rng.randint(-border, fs // 3), rng.randint(0, ch - fs)
        xa, ya = max(0, x0), max(0, y0)
        xb, yb = min(cw, x0 + fs), min(ch, y0 + fs)
        sub = fm[ya - y0:yb - y0, xa - x0:xb - x0]
        img[ya:yb, xa:xb] = img[ya:yb, xa:xb] * (1 - sub[..., None]) + np.array([70, 6, 8], np.float32)[None, None, :] * sub[..., None]
        rough[ya:yb, xa:xb] = rough[ya:yb, xa:xb] * (1 - sub) + 0.3 * sub
    return img, rough


def screen(srcs, rng, cw, ch):
    """Bild des Roehrenmonitors: Fleisch, darin ein Auge, rot, mit Zeilen, Versatz und Rauschen (leuchtet im Spiel)."""
    meat = srcs['meat_mince'].resize((cw, ch), Image.LANCZOS) if srcs.get('meat_mince') else Image.new('RGB', (cw, ch), (120, 20, 20))
    a = np.asarray(meat, np.float32) / 255.0
    if srcs.get('eye_b'):
        e = srcs['eye_b']
        ew = int(cw * 0.7)
        e = e.resize((ew, int(ew * e.size[1] / e.size[0])), Image.LANCZOS)
        ea = np.asarray(e, np.float32) / 255.0
        y0, x0 = (ch - ea.shape[0]) // 2 - 6, (cw - ew) // 2
        yy, xx = np.mgrid[0:ea.shape[0], 0:ea.shape[1]].astype(np.float32)
        m = np.clip(1.3 - np.sqrt(((xx / ea.shape[1] - 0.5) / 0.45) ** 2 + ((yy / ea.shape[0] - 0.5) / 0.45) ** 2), 0, 1)
        a[y0:y0 + ea.shape[0], x0:x0 + ew] = a[y0:y0 + ea.shape[0], x0:x0 + ew] * (1 - m[..., None]) + ea * m[..., None]
    lum = a.mean(axis=2)
    out = np.stack([np.clip(lum * 1.6 + 0.12, 0, 1), lum * 0.32, lum * 0.28], axis=2)
    for _ in range(9):  # Zeilenversatz
        y = rng.randrange(ch)
        hgt = rng.randint(2, 10)
        out[y:y + hgt] = np.roll(out[y:y + hgt], rng.randint(-24, 24), axis=1)
    out[::3] *= 0.62  # Zeilen
    out += np.random.default_rng(rng.randrange(1 << 30)).normal(0, 0.05, out.shape[:2])[..., None]
    out[..., 0] = np.roll(out[..., 0], 2, axis=1)
    return np.clip(out, 0, 1) * 255.0, np.full((ch, cw), 0.1, np.float32)


def back(rng, cw, ch):
    """Rueckseite des Fotopapiers: matt, leicht vergilbt, schwaches Rasterwasserzeichen (ohne Marke)."""
    img = np.ones((ch, cw, 3), np.float32) * np.array([232, 228, 214], np.float32)[None, None, :]
    yy, xx = np.mgrid[0:ch, 0:cw].astype(np.float32)
    lines = ((np.abs(((xx + yy) % 46) - 23) < 0.8) | (np.abs(((xx - yy) % 46) - 23) < 0.8)).astype(np.float32)
    img *= (1 - 0.05 * lines)[..., None]
    img *= (1 - 0.05 * np.random.default_rng(7).random((ch, cw)))[..., None]
    return img, np.full((ch, cw), 0.7, np.float32)


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'qa', 'ps_photos')
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'data', 'textures', 'ps_photos')
    build(src_dir, dst)


def build(src_dir, dst):
    rng = random.Random(1040)
    srcs = {}
    for n in MEATS + ['eye_a', 'eye_b', 'mouth']:
        p = os.path.join(src_dir, n + '.png')
        srcs[n] = Image.open(p).convert('RGB') if os.path.exists(p) else None
        if srcs[n] is None:
            print('fehlt', p)
    alb = np.zeros((SIZE, SIZE, 3), np.float32)
    alb[:] = np.array([40, 30, 28], np.float32)
    rough = np.full((SIZE, SIZE), 0.5, np.float32)
    for name, (x0, y0, x1, y1) in CELLS.items():
        cw, ch = x1 - x0, y1 - y0
        if name == 'screen':
            im, rr = screen(srcs, rng, cw, ch)
        elif name == 'back':
            im, rr = back(rng, cw, ch)
        else:
            im, rr = card(name, srcs.get(name), rng, cw, ch, BORDER.get(name, 18))
        alb[y0:y1, x0:x1] = im
        rough[y0:y1, x0:x1] = rr
    os.makedirs(dst, exist_ok=True)
    Image.fromarray(np.clip(alb, 0, 255).astype(np.uint8)).save(os.path.join(dst, 'albedo.jpg'), quality=94)
    # Papier: kaum Relief (feine Faser), Abzug glatt
    rng_np = np.random.default_rng(3)
    h = 0.5 + 0.01 * np.asarray(Image.fromarray((rng_np.random((SIZE, SIZE)) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    gy, gx = np.gradient(h)
    k = 6.0
    nx, ny, nz = -gx * k, gy * k, np.ones_like(h)  # DirectX: Gruen nach unten
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    nrm = np.stack([nx / ln, ny / ln, nz / ln], axis=2) * 0.5 + 0.5
    Image.fromarray((nrm * 255).astype(np.uint8)).save(os.path.join(dst, 'normal.png'), optimize=True)
    orm = np.stack([np.full_like(h, 1.0), np.clip(rough, 0, 1), np.zeros_like(h), h], axis=2)
    Image.fromarray((orm * 255).astype(np.uint8), 'RGBA').save(os.path.join(dst, 'orm.png'), optimize=True)
    print('Atlas', dst)


if __name__ == '__main__':
    main()
