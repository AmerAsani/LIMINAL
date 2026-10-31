"""V13.3: Pruefung der Modelle aus Blender (data/models/*.glb) - Teil der Objektvalidierung.

Je Modell und Teil: keine ungueltigen Zahlen, keine entarteten Dreiecke (Anteil), Normalen passen zum Umlaufsinn
(Flaechennormale und Vertexnormalen zeigen in dieselbe Richtung), Groesse plausibel (0,5 cm .. 120 m), und die
Dreieckszahl liegt unter der Obergrenze. Wesen (ent_*) und Viewmodel (vm_*) werden mit geprueft.

    python tools/qa/validate_models.py [name ...]      Exitcode 1 bei Fehlern
"""
import json
import os
import struct
import sys

import numpy as np

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MODELS = os.path.join(ROOT, 'data', 'models')
MAX_TRIS = 120000          # je Modell (grosse Einzelstuecke wie Wagen)
MAX_FLIPPED = 0.01         # hoechstens 1 % der Dreiecke mit umgeklappter Normale
MAX_DEGENERATE = 0.02      # hoechstens 2 % entartete Dreiecke
CTYPE = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


def read_glb(path):
    with open(path, 'rb') as fh:
        data = fh.read()
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
    return js, binc


def accessor(js, binc, i):
    a = js['accessors'][i]
    bv = js['bufferViews'][a['bufferView']]
    dt = np.dtype(CTYPE[a['componentType']])
    n = NCOMP[a['type']]
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    stride = bv.get('byteStride', 0)
    if stride and stride != dt.itemsize * n:
        rows = [np.frombuffer(binc, dt, n, start + k * stride) for k in range(a['count'])]
        return np.array(rows)
    return np.frombuffer(binc, dt, a['count'] * n, start).reshape(a['count'], n) if n > 1 else \
        np.frombuffer(binc, dt, a['count'], start)


def check(path):
    errs, info = [], {}
    js, binc = read_glb(path)
    tris = 0
    lo, hi = np.full(3, 1e9), np.full(3, -1e9)
    for mesh in js.get('meshes', []):
        for prim in mesh['primitives']:
            if prim.get('mode', 4) != 4:
                continue
            P = accessor(js, binc, prim['attributes']['POSITION']).astype(np.float64)
            if 'indices' in prim:
                I = accessor(js, binc, prim['indices']).astype(np.int64).reshape(-1, 3)
            else:
                I = np.arange(len(P)).reshape(-1, 3)
            tris += len(I)
            if not np.isfinite(P).all():
                errs.append(f'{mesh.get("name")}: ungueltige Punkte')
                continue
            lo, hi = np.minimum(lo, P.min(0)), np.maximum(hi, P.max(0))
            a, b, c = P[I[:, 0]], P[I[:, 1]], P[I[:, 2]]
            fn = np.cross(b - a, c - a)
            area = np.linalg.norm(fn, axis=1)
            deg = area < 1e-12
            if deg.mean() > MAX_DEGENERATE:
                errs.append(f'{mesh.get("name")}: {deg.mean() * 100:.1f} % entartete Dreiecke')
            if 'NORMAL' in prim['attributes']:
                N = accessor(js, binc, prim['attributes']['NORMAL']).astype(np.float64)
                vn = N[I[:, 0]] + N[I[:, 1]] + N[I[:, 2]]
                ok = ~deg
                flipped = (np.einsum('ij,ij->i', fn[ok], vn[ok]) < 0).mean() if ok.any() else 0.0
                if flipped > MAX_FLIPPED:
                    errs.append(f'{mesh.get("name")}: {flipped * 100:.1f} % Dreiecke mit Normale gegen den Umlaufsinn')
            else:
                errs.append(f'{mesh.get("name")}: ohne Normalen')
    size = hi - lo
    info['tris'] = tris
    info['size'] = [round(float(v), 3) for v in size]
    if tris == 0:
        errs.append('keine Dreiecke')
    elif tris > MAX_TRIS:
        errs.append(f'{tris} Dreiecke (Grenze {MAX_TRIS})')
    if tris and (size.max() < 0.005 or size.max() > 120.0):
        errs.append(f'Groesse unplausibel {info["size"]}')
    return errs, info


def main():
    names = sys.argv[1:]
    files = sorted(f for f in os.listdir(MODELS) if f.endswith('.glb'))
    if names:
        files = [f for f in files if f[:-4] in names or f[:-4].split('@')[0] in names]
    bad = 0
    for f in files:
        try:
            errs, info = check(os.path.join(MODELS, f))
        except Exception as e:  # noqa: BLE001
            errs, info = [f'nicht lesbar: {e}'], {}
        if errs:
            bad += 1
            print(f'FEHLER {f}: ' + '; '.join(errs))
        elif names:
            print(f'ok {f}: {info}')
    print(f'{len(files)} Modelle geprueft, {bad} mit Fehlern')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
