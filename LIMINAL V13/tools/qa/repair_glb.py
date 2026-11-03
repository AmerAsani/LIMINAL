"""V13.3 (Abnahme Level 2): Dreieckslisten fertiger Modelle bereinigen - ohne Neubau in Blender.

Was validate_models.py bemaengelt und hier behoben wird:
  - entartete Dreiecke (Flaeche ~0; entstehen beim Vereinfachen der Wesen-Stufen l1/l2 und an Nahtstellen) -> entfernt
  - Dreiecke, deren Umlaufsinn gegen ihre Vertexnormalen zeigt (duenne Teile wie Haar, Seide, Schnurrhaare nach dem
    Vereinfachen) -> Umlaufsinn gedreht, so dass er zur Normale passt (glTF: Vorderseite gegen den Uhrzeigersinn)
Punkte, Normalen, UV und Gewichte bleiben unveraendert; nur die Indexliste wird neu geschrieben (gleicher Platz im Puffer,
kuerzer, wenn Dreiecke wegfallen). Wesen werden ohne Rueckseitenkappung gezeichnet - im Bild aendert sich nichts, die
Daten sind danach aber stimmig (Schatten, kuenftige Kappung, Pruefung).

    python tools/qa/repair_glb.py <name> [name ...]     (Namen wie in data/models ohne .glb; nur <name> = alle Varianten)
    python tools/qa/repair_glb.py --dry <name> ...      nur zaehlen
"""
import json
import os
import struct
import sys

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MODELS = os.path.join(ROOT, 'data', 'models')
CTYPE = {5121: np.uint8, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}


def read(path):
    data = open(path, 'rb').read()
    magic, ver, length = struct.unpack_from('<4sII', data, 0)
    if magic != b'glTF':
        raise ValueError('kein GLB')
    off = 12
    js, binc = None, b''
    while off < length:
        clen, ctype = struct.unpack_from('<II', data, off)
        chunk = data[off + 8: off + 8 + clen]
        if ctype == 0x4E4F534A:
            js = json.loads(chunk.decode('utf-8'))
        elif ctype == 0x004E4942:
            binc = chunk
        off += 8 + clen
    return js, bytearray(binc)


def write(path, js, binc):
    jb = json.dumps(js, separators=(',', ':')).encode('utf-8')
    jb += b' ' * ((4 - len(jb) % 4) % 4)
    bb = bytes(binc) + b'\0' * ((4 - len(binc) % 4) % 4)
    out = struct.pack('<4sII', b'glTF', 2, 12 + 8 + len(jb) + 8 + len(bb))
    out += struct.pack('<II', len(jb), 0x4E4F534A) + jb + struct.pack('<II', len(bb), 0x004E4942) + bb
    open(path, 'wb').write(out)


def view(js, binc, i):
    a = js['accessors'][i]
    bv = js['bufferViews'][a['bufferView']]
    dt = np.dtype(CTYPE[a['componentType']])
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', 0)
    if stride and stride != dt.itemsize * n:
        raise ValueError('verschraenkte Puffer werden nicht unterstuetzt')
    arr = np.frombuffer(bytes(binc[start:start + a['count'] * n * dt.itemsize]), dt).reshape(a['count'], n) if n > 1 else \
        np.frombuffer(bytes(binc[start:start + a['count'] * dt.itemsize]), dt)
    return arr, start, dt


def repair(path, dry=False):
    js, binc = read(path)
    removed = flipped = total = 0
    done = set()
    for mesh in js.get('meshes', []):
        for prim in mesh['primitives']:
            if prim.get('mode', 4) != 4 or 'indices' not in prim or 'NORMAL' not in prim['attributes']:
                continue
            ia = prim['indices']
            if ia in done:
                continue
            done.add(ia)
            P = view(js, binc, prim['attributes']['POSITION'])[0].astype(np.float64)
            N = view(js, binc, prim['attributes']['NORMAL'])[0].astype(np.float64)
            I, start, dt = view(js, binc, ia)
            I = I.astype(np.int64).reshape(-1, 3)
            total += len(I)
            a, b, c = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
            fn = np.cross(b - a, c - a)
            deg = np.linalg.norm(fn, axis=1) < 1e-12
            vn = N[I[:, 0]] + N[I[:, 1]] + N[I[:, 2]]
            flip = (~deg) & (np.einsum('ij,ij->i', fn, vn) < 0)
            removed += int(deg.sum())
            flipped += int(flip.sum())
            if dry or not (deg.any() or flip.any()):
                continue
            J = I.copy()
            J[flip, 1], J[flip, 2] = I[flip, 2], I[flip, 1]
            J = J[~deg]
            raw = J.astype(dt).tobytes()
            binc[start:start + len(raw)] = raw
            acc = js['accessors'][ia]
            acc['count'] = int(J.size)
            if 'max' in acc:
                acc['max'] = [int(J.max())]
                acc['min'] = [int(J.min())]
    if not dry and (removed or flipped):
        write(path, js, binc)
    return total, removed, flipped


def main():
    args = sys.argv[1:]
    dry = '--dry' in args
    names = [a for a in args if a != '--dry']
    files = sorted(f for f in os.listdir(MODELS) if f.endswith('.glb') and (f[:-4] in names or f[:-4].split('@')[0] in names))
    for f in files:
        t, r, fl = repair(os.path.join(MODELS, f), dry)
        if r or fl:
            print(f'{f}: {t} Dreiecke, {r} entartet entfernt, {fl} Umlaufsinn gedreht' + (' (nur gezaehlt)' if dry else ''))
    print(f'{len(files)} Modelle')


if __name__ == '__main__':
    main()
