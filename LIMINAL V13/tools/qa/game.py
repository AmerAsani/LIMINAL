"""V13.3: Spiel fuer Pruefungen und Bilder starten (Kontaktboegen, Wegbilder, Objektpruefung).

Jeder Lauf bekommt einen eigenen Benutzerordner (Einstellungen, Protokoll, Spielstaende), damit parallele Laeufe und
das normale Spiel sich nicht stoeren. Das Skript ist die Befehlsfolge von --script (Befehle mit ';' getrennt).

    from game import run
    log = run('--level level1', 'wait 30; shot C:/x/a.png; quit', userdir='C:/tmp/ud1')
"""
import json
import os
import subprocess
import time

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
EXE = os.environ.get('LIM_EXE', os.path.join(ROOT, 'build', 'Release', 'bin', 'LiminalGame.exe'))


def reset_settings(userdir):
    """Ego-Perspektive, keine Debug-Ansicht (beides wird in den Einstellungen gespeichert)."""
    p = os.path.join(userdir, 'settings_v13.json')
    if os.path.exists(p):
        with open(p, encoding='utf-8') as fh:
            j = json.load(fh)
        j['third_person'] = False
        j['debug_view'] = 0
        with open(p, 'w', encoding='utf-8') as fh:
            json.dump(j, fh, indent=1)


def sync_data():
    """Karten und Level-Dateien in die Kopie des Spiels uebernehmen (das Spiel liest build/Release/bin/data; ohne Neubau
    blieben neu erzeugte Karten sonst unsichtbar)."""
    import shutil
    dst_root = os.path.join(os.path.dirname(EXE), 'data')
    for sub in ('maps', 'levels'):
        src = os.path.join(ROOT, 'data', sub)
        dst = os.path.join(dst_root, sub)
        if not os.path.isdir(src) or not os.path.isdir(dst):
            continue
        for f in os.listdir(src):
            a, b = os.path.join(src, f), os.path.join(dst, f)
            if not os.path.exists(b) or os.path.getmtime(a) > os.path.getmtime(b):
                shutil.copy2(a, b)


def run(args, script, userdir, size='1600x900', quality=2, hud=False, timeout=3600, extra_env=None):
    """Startet das Spiel und wartet. Liefert (Exitcode, Text des Protokolls dieses Laufs)."""
    if 'LIM_EXE' not in os.environ:
        sync_data()
    os.makedirs(userdir, exist_ok=True)
    reset_settings(userdir)
    logdir = os.path.join(userdir, 'logs')
    before = set(os.listdir(logdir)) if os.path.isdir(logdir) else set()
    env = dict(os.environ)
    env['LIMINAL_USER_DIR'] = userdir
    if extra_env:
        env.update(extra_env)
    cmd = [EXE, '--windowed', '--size', size, '--fixed-dt', '--quality', str(quality), '--script', script]
    if not hud:
        cmd.append('--no-hud')
    cmd += args.split() if isinstance(args, str) else list(args)
    code = None
    t0 = time.time()
    for attempt in range(4):
        try:
            code = subprocess.run(cmd, env=env, timeout=timeout).returncode
            break
        except OSError:
            # Smart App Control prueft neue Programme kurz - nicht umgehen, nur warten
            time.sleep(20)
    text = ''
    if os.path.isdir(logdir):
        # das Spiel schreibt immer dieselbe Datei: die nach dem Start geaenderte(n) lesen
        files = [os.path.join(logdir, f) for f in os.listdir(logdir)]
        fresh = [f for f in files if os.path.getmtime(f) >= t0 - 1]
        if fresh:
            with open(max(fresh, key=os.path.getmtime), encoding='utf-8', errors='replace') as fh:
                text = fh.read()
    return code, text


LEVELS = ['level0', 'level1', 'level2', 'level3', 'level5', 'level5_3', 'level188', 'level903', 'level37', 'level36']
