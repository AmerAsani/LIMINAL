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


def run(args, script, userdir, size='1600x900', quality=2, hud=False, timeout=3600, extra_env=None):
    """Startet das Spiel und wartet. Liefert (Exitcode, Text des Protokolls dieses Laufs)."""
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
    for attempt in range(4):
        try:
            code = subprocess.run(cmd, env=env, timeout=timeout).returncode
            break
        except OSError:
            # Smart App Control prueft neue Programme kurz - nicht umgehen, nur warten
            time.sleep(20)
    text = ''
    if os.path.isdir(logdir):
        new = sorted(set(os.listdir(logdir)) - before)
        if new:
            with open(os.path.join(logdir, new[-1]), encoding='utf-8', errors='replace') as fh:
                text = fh.read()
    return code, text


LEVELS = ['level0', 'level1', 'level2', 'level3', 'level5', 'level5_3', 'level188', 'level903', 'level37', 'level36']
