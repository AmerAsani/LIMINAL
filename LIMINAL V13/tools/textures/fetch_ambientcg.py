"""V12: CC0-Texturen von ambientCG holen (2K-JPG) und mit Pruefsumme festhalten.

Liest tools/textures/selection.json, fragt je Asset die offizielle API nach Download-Datei und Masse, laedt
die ZIP-Datei in einen eigenen Ordner unter .toolchain/assets_dl/<id>/ (nicht im Repository), entpackt sie
dort und schreibt tools/textures/ambientcg_lock.json (ID, URL, Groesse, SHA-256, Masse, Abrufdatum).
Lizenz: CC0 1.0 (docs.ambientcg.com/license, abgerufen 25.10.2026) - siehe LICENSES.md.
Aufruf: python -I tools/textures/fetch_ambientcg.py
"""
import datetime
import hashlib
import json
import os
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SEL = os.path.join(ROOT, 'tools', 'textures', 'selection.json')
LOCK = os.path.join(ROOT, 'tools', 'textures', 'ambientcg_lock.json')
DL = os.path.join(ROOT, '.toolchain', 'assets_dl')
API = 'https://ambientcg.com/api/v2/full_json?id={}&include=downloadData'
UA = {'User-Agent': 'LIMINAL-V12-asset-fetch/1.0 (single-user, CC0 textures)'}


def get_json(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        return json.loads(r.read().decode('utf-8'))


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    sel = json.load(open(SEL, encoding='utf-8'))['textures']
    lock = json.load(open(LOCK, encoding='utf-8')) if os.path.exists(LOCK) else {'assets': {}}
    today = datetime.date.today().isoformat()
    for t in sel:
        if t.get('generator'):  # V12: eigene Textur (tools/textures/generate.py), nichts herunterzuladen
            continue
        aid = t['id']
        folder = os.path.join(DL, aid)
        os.makedirs(folder, exist_ok=True)
        meta = get_json(API.format(aid))['foundAssets'][0]
        dl = [d for d in meta['downloadFolders']['default']['downloadFiletypeCategories']['zip']['downloads']
              if d.get('attribute') == '2K-JPG'][0]
        zpath = os.path.join(folder, dl['fileName'])
        if not os.path.exists(zpath) or os.path.getsize(zpath) != dl['size']:
            print('lade', dl['fileName'], dl['size'], 'Byte', flush=True)
            req = urllib.request.Request(dl['fullDownloadPath'], headers=UA)
            with urllib.request.urlopen(req, timeout=300) as r, open(zpath, 'wb') as f:
                while True:
                    b = r.read(1 << 20)
                    if not b:
                        break
                    f.write(b)
        if os.path.getsize(zpath) != dl['size']:
            sys.exit(f'{aid}: Groesse passt nicht')
        digest = sha256(zpath)
        old = lock['assets'].get(aid)
        if old and old['sha256'] != digest:
            sys.exit(f'{aid}: Pruefsumme weicht vom Lock ab')
        xdir = os.path.join(folder, 'x')
        if not os.path.isdir(xdir):
            with zipfile.ZipFile(zpath) as z:
                for n in z.namelist():  # nur flache Bilddateien, keine Pfade aus dem Archiv
                    base = os.path.basename(n)
                    if base and base.lower().endswith(('.jpg', '.png')) and '..' not in n:
                        os.makedirs(xdir, exist_ok=True)
                        with z.open(n) as src, open(os.path.join(xdir, base), 'wb') as dst:
                            dst.write(src.read())
        lock['assets'][aid] = {
            'url': dl['fullDownloadPath'], 'file': dl['fileName'], 'size': dl['size'], 'sha256': digest,
            'displayName': meta.get('displayName'), 'releaseDate': meta.get('releaseDate'),
            'dimensionsCm': [meta.get('dimensionX'), meta.get('dimensionY')],
            'license': 'CC0-1.0', 'licenseSource': 'https://docs.ambientcg.com/license/',
            'retrieved': old['retrieved'] if old else today,
        }
        json.dump(lock, open(LOCK, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
        print('ok', aid, digest[:16], flush=True)


if __name__ == '__main__':
    main()
