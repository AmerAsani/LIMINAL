"""V13.3: Notausgangs- und Hinweisschilder je Ebene zaehlen und pruefen (PROMPT_V13_3 Teil 2).

Ein Notausgangsschild ist nur dort sinnvoll, wo es baulich einen Ausgang gibt: ueber oder neben einer Tuer bzw. einem
Durchgang (hoechstens 1,5 m waagrecht von einer Tuerkachel, Hoehe ueber 1,8 m) oder an einem Uebergang der Karte.
Jede Ebene hat eine Obergrenze (tools/qa/metric_limits.json: signs_max) - im Wiki stehen Schilder nur in Level 1
(Schwingtueren mit Notausgangsschild darueber), Level 36 (Flughafen) und Level 903 (beleuchtete Schilder).

    python tools/qa/signs.py [--check] [level ...]
Quelle: build/qa/props_<level>.json (tools/qa/propdump.py) und die Karte.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from maplib import LEVELS, NAMES, ROOT, Grid  # noqa: E402

EXIT = ('exit_sign', 'exit_sign_green', 'exit_sign_36')


def count(level):
    g = Grid(level)
    with open(os.path.join(ROOT, 'build', 'qa', f'props_{level}.json'), encoding='utf-8') as fh:
        d = json.load(fh)
    signs = [p for p in d['props'] if p['name'] in EXIT]
    doors = [dd['rect'] for dd in g.m['doors']]
    trans = [t['pos'][:2] for t in g.m.get('transitions', [])]
    bad = []
    for p in signs:
        x, y, z = p['p']
        near_door = any(r[0] - 1.5 <= x <= r[2] + 1.5 and r[1] - 1.5 <= y <= r[3] + 1.5 for r in doors)
        near_exit = any(math.hypot(t[0] - x, t[1] - y) < 4.0 for t in trans)
        high = z - p['floor'] >= 1.8 or p['back'] > 0.5
        if not (near_door or near_exit) or not high:
            bad.append({'p': [round(x, 2), round(y, 2), round(z, 2)], 'tuer': near_door, 'hoch': high})
    return {'level': level, 'exit_signs': len(signs), 'exit_doors': len(d['exits']), 'misplaced': bad}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    do_check = '--check' in sys.argv
    with open(os.path.join(ROOT, 'tools', 'qa', 'metric_limits.json'), encoding='utf-8') as fh:
        limits = json.load(fh)
    fails = 0
    for lv in args or LEVELS:
        c = count(lv)
        cap = limits.get(lv, {}).get('signs_max', 0)
        line = f"{NAMES[lv]:10} Notausgangsschilder {c['exit_signs']:3d} (Grenze {cap}), Notausgangstueren {c['exit_doors']}, falsch platziert {len(c['misplaced'])}"
        print(line)
        if do_check and (c['exit_signs'] > cap or c['misplaced']):
            fails += 1
            for b in c['misplaced'][:5]:
                print('   FEHLER: Schild ohne Tuer/Ausgang oder zu tief bei', b['p'])
            if c['exit_signs'] > cap:
                print(f"   FEHLER: {c['exit_signs']} Schilder ueber der Grenze {cap}")
    if do_check:
        print('Schilder:', 'OK' if not fails else f'{fails} Ebenen mit Fehlern')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
