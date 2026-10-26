"""V12: alle Detailmodelle bauen. Aufruf (aus dem V12-Ordner):
  .toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/build_models.py -- [name ...]
Ohne Namen werden alle Modelle gebaut. Jedes Modell ist eine Funktion build_<name>() in einem Modul unter
tools/blender/models/ und schreibt data/models/<name>.glb.
"""
import importlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common  # noqa: E402

MODULES = ['models_test', 'models_l0', 'models_vm', 'models_l1', 'models_l2']


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    builders = {}
    for modname in MODULES:
        mod = importlib.import_module('models.' + modname)
        for k in dir(mod):
            if k.startswith('build_'):
                builders[k[6:]] = getattr(mod, k)
    names = argv or sorted(builders)
    for n in names:
        if n not in builders:
            raise SystemExit(f'unbekanntes Modell: {n}')
        common.reset()
        builders[n]()
        common.export(n)


if __name__ == '__main__':
    main()
