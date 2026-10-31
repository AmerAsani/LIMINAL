"""V13.3: Ausstattung aller Ebenen aus dem Spiel lesen (Befehl propdump) -> build/qa/props_<level>.json

    python tools/qa/propdump.py [level ...]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import LEVELS, ROOT, run  # noqa: E402

OUT = os.path.join(ROOT, 'build', 'qa')


def dump(level):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f'props_{level}.json').replace('\\', '/')
    ud = os.path.join(OUT, 'ud_dump')
    code, log = run(f'--level {level} --seed 1', f'wait 20; propdump {path}; quit', ud, size='640x360', quality=0)
    ok = os.path.exists(path) and code == 0
    print(f'{level}: {"ok" if ok else "FEHLER"} ({path})', flush=True)
    return ok


if __name__ == '__main__':
    levels = sys.argv[1:] or LEVELS
    bad = [lv for lv in levels if not dump(lv)]
    sys.exit(1 if bad else 0)
