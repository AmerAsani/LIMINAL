"""V13.3: Smoke-Test je Ebene mit Bildzeiten und Speicher (Abnahmeschranke f).

Startet die Ebene (Qualitaet 2, 1600x900, ohne feste Zeitschritte), geht den Laufweg von walkway.py ab (ohne Bilder) und
misst an jeder Station mit dem Skriptbefehl perf: Spiel (CPU, ms), Grafik (CPU, ms), GPU (ms), Dreiecke, VRAM und RAM.
Fehler: Protokollzeilen mit ERROR, ein Skriptfehler, GPU-Zeit im Mittel ueber 16,7 ms (60 Bilder/s) oder RAM ueber 4 GB.

    python tools/qa/smoke.py <level> [--stations 24] [--figures 2]
Ergebnis: docs/metrics/smoke_<level>.json und eine Zeile fuer AUFWAND.md.
"""
import argparse
import json
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import ROOT, run  # noqa: E402
from maplib import NAMES, Grid  # noqa: E402
import walkway  # noqa: E402

PERF = re.compile(r'perf Spiel ([\d.]+) ms, Grafik ([\d.]+) ms, (\d+) Dreiecke, (\d+) Netze, VRAM (\d+) MB von (\d+) MB, '
                  r'RAM (\d+) MB \(Spitze (\d+) MB\) \| GPU ([\d.]+) ms')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('--stations', type=int, default=24)
    ap.add_argument('--figures', type=int, default=2)
    ap.add_argument('--seed', type=int, default=1)
    a = ap.parse_args()
    g = Grid(a.level)
    pts, seq, length = walkway.plan(g, 15, 200.0, a.seed)
    stations = walkway.resample(pts, max(4.0, length / a.stations))[:a.stations]
    sc = ['wait 150']
    for i, (x, y) in enumerate(stations):
        j = min(i + 1, len(stations) - 1)
        yaw = math.atan2(stations[j][1] - y, stations[j][0] - x) if j != i else 0.0
        sc += [f'pos {x:.2f} {y:.2f} {yaw:.4f} 0', 'wait 45', 'perf']
    sc.append('quit')
    sp = os.path.join(ROOT, 'build', 'qa', f'smoke_{a.level}.txt')
    with open(sp, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(sc))
    exe_args = ['--level', a.level, '--seed', str(a.seed), '--set', f'figures={a.figures}']
    import subprocess  # noqa: F401  (run() startet das Spiel)
    code, log = run(exe_args, '@' + sp, os.path.join(ROOT, 'build', 'qa', 'ud_smoke'), size='1600x900', quality=2,
                    extra_env=None)
    rows = [tuple(float(v) for v in m.groups()) for m in PERF.finditer(log)]
    errors = [ln for ln in log.splitlines() if ' ERROR ' in ln]
    res = {'level': a.level, 'exit_code': code, 'stations': len(rows), 'errors': errors[:20]}
    if rows:
        col = list(zip(*rows))
        res.update({
            'sim_ms_avg': sum(col[0]) / len(rows), 'render_cpu_ms_avg': sum(col[1]) / len(rows),
            'gpu_ms_avg': sum(col[8]) / len(rows), 'gpu_ms_max': max(col[8]),
            'triangles_max': int(max(col[2])), 'vram_mb_max': int(max(col[4])), 'ram_mb_max': int(max(col[6])),
            'ram_peak_mb': int(max(col[7])),
        })
    ok = code == 0 and not errors and rows and res['gpu_ms_avg'] <= 16.7 and res['ram_peak_mb'] <= 4096
    res['ok'] = bool(ok)
    od = os.path.join(ROOT, 'docs', 'metrics')
    os.makedirs(od, exist_ok=True)
    with open(os.path.join(od, f'smoke_{a.level}.json'), 'w', encoding='utf-8') as fh:
        json.dump(res, fh, indent=1)
    if rows:
        print(f"{NAMES[a.level]}: {'OK' if ok else 'FEHLER'} - {len(rows)} Stationen, GPU {res['gpu_ms_avg']:.2f} ms "
              f"(max {res['gpu_ms_max']:.2f}), Spiel {res['sim_ms_avg']:.2f} ms, Grafik-CPU {res['render_cpu_ms_avg']:.2f} ms, "
              f"bis {res['triangles_max'] / 1e6:.2f} Mio. Dreiecke, VRAM {res['vram_mb_max']} MB, RAM {res['ram_mb_max']} MB "
              f"(Spitze {res['ram_peak_mb']} MB), Fehler im Protokoll {len(errors)}")
    else:
        print(f'{NAMES[a.level]}: FEHLER - keine Messwerte (Exitcode {code})')
    for e in errors[:5]:
        print('   ', e[:200])
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
