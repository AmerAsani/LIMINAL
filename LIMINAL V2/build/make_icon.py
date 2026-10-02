"""Erzeugt das LIMINAL-Icon (ICO mit 16/24/32/48/64/256 px) und eine PNG-Vorschau.

Motiv: Blick in einen endlosen Flur (Zentralperspektive) - vergilbte Tapete,
Teppich, Deckenleuchten, am Ende eine warm leuchtende Tuer im Dunkel.
Die grosse Variante wird bewusst mit 64x64 "Pixeln" gerendert und dann ohne
Glaettung vergroessert - derselbe Pixel-Look wie die Halbblock-Grafik im Spiel.
Nur Standardbibliothek.

    python build/make_icon.py  ->  assets/liminal.ico, assets/liminal_icon.png
"""

import math
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(os.path.dirname(HERE), "assets")

WALL = (0.86, 0.80, 0.54)
FLOOR = (0.52, 0.43, 0.26)
CEIL = (0.82, 0.80, 0.70)
LAMP = (1.00, 0.97, 0.84)
FOG = (0.10, 0.085, 0.05)
DOOR = (1.00, 0.86, 0.55)
BACK = (0.18, 0.16, 0.10)
BG = (0.055, 0.05, 0.04)
ACCENT = (1.00, 0.84, 0.47)

HALF_W = 1.0      # halbe Flurbreite
EYE = 0.95        # Augenhoehe ueber dem Boden
CEIL_H = 1.15     # Decke ueber Augenhoehe
LENGTH = 9.0      # Flurlaenge bis zur Rueckwand


def _mix(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def scene(u, v):
    """Farbe fuer Bildkoordinate u, v in [-1, 1] (v nach unten)."""
    fov = 1.05
    dx, dy = u * fov, v * fov
    hits = []
    if abs(dx) > 1e-6:
        hits.append((HALF_W / abs(dx), "wall"))
    if dy > 1e-6:
        hits.append((EYE / dy, "floor"))
    if dy < -1e-6:
        hits.append((CEIL_H / -dy, "ceil"))
    t, kind = min(hits) if hits else (1e9, "back")
    if t > LENGTH:
        t, kind = LENGTH, "back"
    x, y, z = dx * t, dy * t, t
    shade = 1.0
    if kind == "wall":
        col = WALL
        if (z * 0.9) % 1.0 < 0.06:
            shade *= 0.72           # Tapetenbahnen
        if y > EYE - 0.1:
            col = (0.40, 0.29, 0.17)   # Sockelleiste
        shade *= 0.78 if dx < 0 else 0.9
        # Tueren entlang des Flurs
        if 2.0 < z % 3.4 < 2.7 and y > -0.55:
            col, shade = (0.24, 0.17, 0.10), 0.9
    elif kind == "floor":
        col = FLOOR
        if (z * 0.8) % 1.0 < 0.05 or abs(x) < 0.03:
            shade *= 0.8
    elif kind == "ceil":
        col = CEIL
        if (z % 1.7) < 0.55 and abs(x) < 0.42:
            return _mix(LAMP, FOG, 1.0 - math.exp(-z * 0.12))
        if (z % 1.7) < 0.04 or abs(abs(x) - 0.5) < 0.03:
            shade *= 0.8
    else:
        # Rueckwand mit leuchtender Tuer
        if abs(x) < 0.34 and y > -0.45:
            glow = 1.0 - min(1.0, abs(x) / 0.34) * 0.15
            return tuple(c * glow for c in DOOR)
        col = BACK
        d = math.hypot(x / 0.34, (y - 0.3) / 0.9)
        shade *= 1.0 + max(0.0, 1.9 - d) * 1.6   # Lichthof um die Tuer
    # Licht der Deckenleuchten (periodisch) und Nebel
    lamp = 0.55 + 0.45 * math.cos(((z % 1.7) - 0.27) / 1.7 * 2 * math.pi) ** 2
    c = tuple(ch * shade * lamp for ch in col)
    f = 1.0 - math.exp(-z * 0.36)
    return _mix(c, FOG, f)


def render(size, ss=4, pixel=None):
    """Rendert das Motiv in size x size (RGBA 0..255), abgerundete Ecken."""
    base = pixel or size
    img = []
    for py in range(base):
        row = []
        for px in range(base):
            acc = [0.0, 0.0, 0.0]
            for sy in range(ss):
                for sx in range(ss):
                    u = ((px + (sx + 0.5) / ss) / base) * 2.0 - 1.0
                    v = ((py + (sy + 0.5) / ss) / base) * 2.0 - 1.0 - 0.06
                    c = scene(u, v)
                    acc[0] += c[0]
                    acc[1] += c[1]
                    acc[2] += c[2]
            n = ss * ss
            row.append((acc[0] / n, acc[1] / n, acc[2] / n))
        img.append(row)
    # Vergroessern (Pixel-Look) falls noetig
    if pixel and pixel != size:
        k = size // pixel
        img = [[img[y // k][x // k] for x in range(size)] for y in range(size)]
    # Rahmen, Vignette, abgerundete Ecken
    out = []
    r = size * 0.18
    border = max(1.0, size / 64.0)
    for y in range(size):
        for x in range(size):
            cx = min(x + 0.5, size - x - 0.5)
            cy = min(y + 0.5, size - y - 0.5)
            # Abstand zur abgerundeten Kante
            if cx < r and cy < r:
                edge = r - math.hypot(r - cx, r - cy)
            else:
                edge = min(cx, cy)
            alpha = max(0.0, min(1.0, edge + 0.5))
            c = img[y][x]
            nx = (x + 0.5) / size * 2 - 1
            ny = (y + 0.5) / size * 2 - 1
            vig = 1.0 - 0.55 * min(1.0, (nx * nx + ny * ny) * 0.6)
            c = tuple(ch * vig for ch in c)
            if edge < border * 1.6:
                t = max(0.0, min(1.0, edge / (border * 1.6)))
                c = _mix(_mix(ACCENT, BG, 0.35), c, t)
            out.append(tuple(int(max(0.0, min(1.0, ch)) ** (1 / 1.1) * 255 + 0.5) for ch in c) + (int(alpha * 255),))
    return out


def png_bytes(size, px):
    raw = b"".join(b"\x00" + bytes(v for p in px[y * size:(y + 1) * size] for v in p) for y in range(size))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def dib_bytes(size, px):
    """32-Bit-BMP (BGRA) inkl. AND-Maske, wie Windows es fuer kleine Icons erwartet."""
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    rows = []
    for y in range(size - 1, -1, -1):
        rows.append(b"".join(bytes((p[2], p[1], p[0], p[3])) for p in px[y * size:(y + 1) * size]))
    mask_row = ((size + 31) // 32) * 4
    mask = b""
    for y in range(size - 1, -1, -1):
        bits = 0
        row = bytearray(mask_row)
        for x in range(size):
            if px[y * size + x][3] < 128:
                row[x // 8] |= 0x80 >> (x % 8)
        mask += bytes(row)
        del bits
    return header + b"".join(rows) + mask


def main():
    os.makedirs(ASSETS, exist_ok=True)
    images = []
    for size in (16, 24, 32, 48, 64, 256):
        if size == 256:
            px = render(256, ss=5, pixel=64)
            data = png_bytes(size, px)
            with open(os.path.join(ASSETS, "liminal_icon.png"), "wb") as f:
                f.write(data)
        else:
            px = render(size, ss=6)
            data = dib_bytes(size, px)
        images.append((size, data))
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    blobs = b""
    for size, data in images:
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset + len(blobs))
        blobs += data
    with open(os.path.join(ASSETS, "liminal.ico"), "wb") as f:
        f.write(header + entries + blobs)
    print("Icon geschrieben:", os.path.join(ASSETS, "liminal.ico"))


if __name__ == "__main__":
    main()
