"""V12: alle Detailmodelle bauen. Aufruf (aus dem V12-Ordner):
  .toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/build_models.py -- [name ...]
Ohne Namen werden alle Modelle gebaut. Jedes Modell ist eine Funktion build_<name>() in einem Modul unter
tools/blender/models/ und schreibt data/models/<name>.glb.
V13.3: build_<name>__v<N>() ist Variante N von <name> und schreibt data/models/<name>@<N>.glb (Name auf der
Kommandozeile: <name>__v<N> oder <name>@<N>; nur <name> baut alle Varianten).
"""
import importlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

MODULES = ['models_test', 'models_l0', 'models_vm', 'models_hand', 'models_l1', 'models_l2', 'models_l3', 'models_l5', 'models_l53', 'models_l188', 'models_l903', 'models_l36', 'models_l1_code', 'models_l37', 'models_l1_sectors']


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    builders = {}
    for modname in MODULES:
        mod = importlib.import_module('models.' + modname)
        for k in dir(mod):
            if k.startswith('build_'):
                builders[k[6:]] = getattr(mod, k)
    names = []
    for n in argv or sorted(builders):
        n = n.replace('@', '__v')
        if n not in builders:
            raise SystemExit(f'unbekanntes Modell: {n}')
        names.append(n)
        if '__v' not in n:  # alle Varianten mitbauen
            names += sorted(k for k in builders if k.startswith(n + '__v') and k not in argv)
    for n in dict.fromkeys(names):
        common.reset()
        builders[n]()
        common.export(n.replace('__v', '@'))


if __name__ == '__main__':
    main()
