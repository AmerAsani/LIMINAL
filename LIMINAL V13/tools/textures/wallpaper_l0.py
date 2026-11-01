"""V13.3 (PROPS_Level0.md): Raufaser von Level 0 als geklebte Tapete - Bahnen 53 cm mit Stossfugen.

Wikidot Level 0: "mono-yellow wallpaper". Die Wand war auf 3-4 m glatt, weil die Raufaser (ambientCG Wallpaper001A, CC0)
nur feines Relief hat, das mit dem Abstand verschwindet. Echte Raufaser wird in Bahnen von 53 cm geklebt: an jeder
Stossfuge eine feine Schattenlinie und eine leicht aufstehende Kante, jede Bahn hat einen etwas anderen Ton (andere
Rolle, anders gealtert), dazu grossflaechige Vergilbung. Das bleibt einfarbig (kein Muster, LOOK_0.md), ist aber
auf Distanz als Tapete lesbar.

Liest die Originaldaten aus .toolchain/assets_dl/Wallpaper001A (wie tools/textures/process.py) und schreibt
data/textures/wallpaper_threshold/ neu (Kachel 2,12 m = 4 Bahnen; meters in textures.json). Nach process.py ausfuehren.
    python tools/textures/wallpaper_l0.py   (braucht numpy, Pillow)
"""
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, '.toolchain', 'assets_dl', 'Wallpaper001A', 'x')
DST = os.path.join(ROOT, 'data', 'textures', 'wallpaper_threshold')
SIZE = 1024
STRIPS = 4          # Bahnen je Kachel (Kachel 2,12 m)
SEAM_PX = 0.9       # halbe Breite der Schattenlinie in Pixeln (1 px = 2,07 mm)
TONES = [1.000, 0.982, 1.013, 0.991]   # Ton je Bahn (Helligkeit)
WARM = [0.000, 0.008, -0.004, 0.005]   # Vergilbung je Bahn (Blau runter, Rot rauf)


def tile_resize(img, size):
    w, h = img.size
    big = Image.new(img.mode, (w * 3, h * 3))
    for i in range(3):
        for j in range(3):
            big.paste(img, (i * w, j * h))
    big = big.resize((size * 3, size * 3), Image.LANCZOS)
    return big.crop((size, size, size * 2, size * 2))


def tile_noise(size, cells, seed):
    """Kachelbares Wertrauschen (bikubisch geglaettet) 0..1."""
    rng = np.random.default_rng(seed)
    g = rng.random((cells, cells))
    x = np.arange(size) * cells / size
    i0 = np.floor(x).astype(int)
    f = x - i0
    f = f * f * (3 - 2 * f)
    i1 = (i0 + 1) % cells
    a = g[i0][:, i0] * (1 - f)[None, :] + g[i0][:, i1] * f[None, :]
    b = g[i1][:, i0] * (1 - f)[None, :] + g[i1][:, i1] * f[None, :]
    return a * (1 - f)[:, None] + b * f[:, None]


def main():
    find = lambda key: os.path.join(SRC, f'Wallpaper001A_2K-JPG_{key}.jpg')
    col = np.asarray(tile_resize(Image.open(find('Color')).convert('RGB'), SIZE), np.float32) / 255.0
    nrm = np.asarray(tile_resize(Image.open(find('NormalDX')).convert('RGB'), SIZE), np.float32) / 255.0
    rough = np.asarray(tile_resize(Image.open(find('Roughness')).convert('L'), SIZE), np.float32) / 255.0
    disp = np.asarray(tile_resize(Image.open(find('Displacement')).convert('L'), SIZE), np.float32) / 255.0
    u = (np.arange(SIZE) + 0.5) / SIZE * STRIPS            # Bahnkoordinate entlang u (waagrecht)
    strip = np.floor(u).astype(int) % STRIPS
    d = (u - np.floor(u)) * SIZE / STRIPS                   # Pixel ab der linken Bahnkante
    dist = np.minimum(d, SIZE / STRIPS - d)                 # Abstand zur naechsten Fuge (px)
    side = np.where(d < SIZE / STRIPS / 2, 1.0, -1.0)       # links der Fuge: Kante der rechten Bahn liegt oben
    seam = np.exp(-(dist / SEAM_PX) ** 2)                   # Schattenlinie
    lip = np.exp(-((dist - 2.2) / 1.1) ** 2) * (side > 0)   # aufstehende Kante (eine Seite)
    tone = np.array(TONES, np.float32)[strip]
    warm = np.array(WARM, np.float32)[strip]
    # grossflaechige Vergilbung (0,5-1 m) und leichte Streifigkeit der Rolle (senkrecht)
    blot = tile_noise(SIZE, 4, 11) * 0.6 + tile_noise(SIZE, 9, 12) * 0.4
    roll = tile_noise(SIZE, 64, 13).mean(axis=0)[None, :]  # nur waagrecht veraenderlich -> senkrechte Schlieren
    k = (tone[None, :] * (1.0 - 0.035 * (blot - 0.5)) * (1.0 - 0.02 * (roll - 0.5)))
    k = k * (1.0 - 0.13 * seam[None, :]) * (1.0 + 0.035 * lip[None, :])
    out = col * k[..., None]
    out[..., 0] *= 1.0 + warm[None, :] * 0.6 + 0.02 * (blot - 0.5)
    out[..., 2] *= 1.0 - warm[None, :] * 1.4 - 0.05 * (blot - 0.5)
    Image.fromarray(np.clip(out * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGB').save(os.path.join(DST, 'albedo.jpg'), quality=93)
    # Normalen (DirectX): Fuge als Kerbe, Kante als kleiner Grat; x = waagrechte Neigung
    nx = nrm[..., 0] * 2 - 1
    grad = np.gradient(-0.35 * seam + 0.2 * lip)[None, :]
    nx = np.clip(nx + grad * 0.6, -1, 1)
    nrm2 = nrm.copy()
    nrm2[..., 0] = (nx + 1) * 0.5
    Image.fromarray(np.clip(nrm2 * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGB').save(os.path.join(DST, 'normal.png'), optimize=True)
    # ORM: Verdeckung in der Fuge, Rauheit etwas geringer auf der Kante (Kleisterglanz), Hoehe mit Kerbe
    ao = np.clip(1.0 - 0.10 * seam[None, :].repeat(SIZE, 0), 0, 1)
    r2 = np.clip(rough - 0.06 * lip[None, :], 0, 1)
    metal = np.zeros_like(rough)
    h = np.clip(disp - 0.025 * seam[None, :] + 0.012 * lip[None, :], 0, 1)
    orm = np.stack([ao, r2, metal, h], axis=-1)
    Image.fromarray(np.clip(orm * 255.0 + 0.5, 0, 255).astype(np.uint8), 'RGBA').save(os.path.join(DST, 'orm.png'), optimize=True)
    print('ok', DST)


if __name__ == '__main__':
    main()
