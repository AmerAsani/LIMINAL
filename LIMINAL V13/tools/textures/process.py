"""V12: ambientCG-Sets (2K-JPG) zu Spieltexturen aufbereiten.

Fuer jeden Eintrag in tools/textures/selection.json entsteht data/textures/<name>/:
  albedo.jpg  - Farbe (sRGB), 1024 px
  normal.png  - Normalen im DirectX-Format (Gruen nach unten, wie die Engine sie aus Hoehen rechnet), 1024 px
  orm.png     - R = Umgebungsverdeckung, G = Rauheit, B = Metall, A = Hoehe (0,5 = Mitte), 1024 px
Skaliert wird kachelbar: das Bild wird 3x3 aneinandergelegt, verkleinert und die Mitte ausgeschnitten.
Dazu data/textures/textures.json mit Name, Quelle und realer Kantenlaenge (aus den ambientCG-Massen).
Aufruf: python -I tools/textures/process.py   (braucht Pillow)
"""
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SEL = os.path.join(ROOT, 'tools', 'textures', 'selection.json')
LOCK = os.path.join(ROOT, 'tools', 'textures', 'ambientcg_lock.json')
DL = os.path.join(ROOT, '.toolchain', 'assets_dl')
OUT = os.path.join(ROOT, 'data', 'textures')
SIZE = 1024


def find(xdir, aid, key):
    for f in os.listdir(xdir):
        if f.startswith(aid + '_') and f.split('_')[-1].lower().startswith(key.lower()):
            return os.path.join(xdir, f)
    return None


def tile_resize(img, size):
    w, h = img.size
    big = Image.new(img.mode, (w * 3, h * 3))
    for i in range(3):
        for j in range(3):
            big.paste(img, (i * w, j * h))
    big = big.resize((size * 3, size * 3), Image.LANCZOS)
    return big.crop((size, size, size * 2, size * 2))


def main():
    sel = json.load(open(SEL, encoding='utf-8'))['textures']
    lock = json.load(open(LOCK, encoding='utf-8'))['assets']
    os.makedirs(OUT, exist_ok=True)
    manifest = []
    for t in sel:
        if t.get('same_as'):  # V13.3 (Abnahme Level 36): dieselben Bilder einer anderen Textur mit eigenen Werten
            manifest.append({'name': t['name'], 'dir': t['same_as'], 'source': 'wie:' + t['same_as'],
                             'meters': t.get('meters', next((m['meters'] for m in manifest if m['name'] == t['same_as']), 2.0)),
                             'normal_strength': t.get('normal_strength', 1.0), 'rough_bias': t.get('rough_bias', 0.0),
                             'variation': t.get('variation', 0.25), 'antitile': t.get('antitile', 0),
                             'levels': t.get('levels', [])})
            print('ok', t['name'], 'wie', t['same_as'], flush=True)
            continue
        aid, name = t['id'], t['name']
        xdir = os.path.join(DL, aid, 'x')
        dst = os.path.join(OUT, name)
        os.makedirs(dst, exist_ok=True)
        if t.get('generator'):  # V12: eigene, prozedurale Textur (keine fremde Quelle)
            import generate  # noqa: E402 (liegt neben diesem Skript)
            generate.GENERATORS[t['generator']](dst, SIZE)
            manifest.append({'name': name, 'dir': name, 'source': 'eigen:' + t['generator'], 'meters': t.get('meters', 1.0),
                             'normal_strength': t.get('normal_strength', 1.0), 'rough_bias': t.get('rough_bias', 0.0),
                             'variation': t.get('variation', 0.25), 'antitile': t.get('antitile', 0),
                         'levels': t.get('levels', [])})
            print('ok', name, 'eigen', t['generator'], flush=True)
            continue
        color = find(xdir, aid, 'Color')
        normal = find(xdir, aid, 'NormalDX')
        rough = find(xdir, aid, 'Roughness')
        ao = find(xdir, aid, 'AmbientOcclusion')
        metal = find(xdir, aid, 'Metalness')
        disp = find(xdir, aid, 'Displacement')
        if not (color and normal and rough):
            raise SystemExit(f'{aid}: Farbe, Normalen oder Rauheit fehlen')
        tile_resize(Image.open(color).convert('RGB'), SIZE).save(os.path.join(dst, 'albedo.jpg'), quality=93)
        tile_resize(Image.open(normal).convert('RGB'), SIZE).save(os.path.join(dst, 'normal.png'), optimize=True)
        g = tile_resize(Image.open(rough).convert('L'), SIZE)
        r = tile_resize(Image.open(ao).convert('L'), SIZE) if ao else Image.new('L', (SIZE, SIZE), 255)
        b = tile_resize(Image.open(metal).convert('L'), SIZE) if metal else Image.new('L', (SIZE, SIZE), 0)
        a = tile_resize(Image.open(disp).convert('L'), SIZE) if disp else Image.new('L', (SIZE, SIZE), 128)
        Image.merge('RGBA', (r, g, b, a)).save(os.path.join(dst, 'orm.png'), optimize=True)
        dim = lock[aid]['dimensionsCm']
        meters = t.get('meters') or (dim[0] / 100.0 if dim and dim[0] else 2.0)
        manifest.append({'name': name, 'dir': name, 'source': aid, 'meters': round(meters, 3),
                         'normal_strength': t.get('normal_strength', 1.0), 'rough_bias': t.get('rough_bias', 0.0),
                         'variation': t.get('variation', 0.25), 'antitile': t.get('antitile', 0),
                         'levels': t.get('levels', [])})
        print('ok', name, aid, meters, 'm', flush=True)
    with open(os.path.join(OUT, 'textures.json'), 'w', encoding='utf-8') as f:
        f.write('// LIMINAL V12 - erzeugt von tools/textures/process.py; Quellen und Lizenz in LICENSES.md\n')
        json.dump({'textures': manifest}, f, indent=1, ensure_ascii=False)


if __name__ == '__main__':
    main()
