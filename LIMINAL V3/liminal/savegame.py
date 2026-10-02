"""Spielstand (V3): Autosave alle 3 Minuten, beim Beenden und nach "Neues Spiel".

Gespeichert werden Seed, Position, Blickrichtung, Health, Sanity, aktive
Effekte, Inventar, aufgenommene Items, zurueckgelegte Strecke und Spielzeit.
Die Welt selbst muss nicht gespeichert werden - sie entsteht aus dem Seed
jedes Mal identisch.

Zuverlaessigkeit: Es wird zuerst in eine temporaere Datei geschrieben und diese
dann atomar umbenannt; der vorherige Stand bleibt als .bak erhalten. Ist die
Hauptdatei beschaedigt, wird automatisch die Sicherung geladen.
"""

import json
import os
import time

VERSION = 3


def default_path():
    if os.name == "nt":
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
        return os.path.join(base, "LIMINAL", "save_v3.json")
    base = os.environ.get("XDG_DATA_HOME") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return os.path.join(base, "liminal", "save_v3.json")


def save(game, path=None):
    """Schreibt den Spielstand. Liefert True bei Erfolg."""
    path = path or default_path()
    p = game.player
    data = {
        "version": VERSION,
        "saved_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "seed": game.world.seed,
        "player": {"x": p.x, "y": p.y, "z": p.z, "angle": p.angle, "pitch": p.pitch,
                   "distance": p.distance},
        "stats": game.stats.to_dict(),
        "inventory": game.inventory.to_list(),
        "selected": game.inventory.selected,
        "picked": sorted(game.world.picked),
    }
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        tmp = path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False)
            f.flush()
            os.fsync(f.fileno())
        if os.path.exists(path):
            os.replace(path, path + ".bak")
        os.replace(tmp, path)
        return True
    except OSError:
        return False


def load(path=None):
    """Liest den Spielstand (bei Fehler die Sicherung). None, wenn keiner da ist."""
    path = path or default_path()
    for candidate in (path, path + ".bak"):
        try:
            with open(candidate, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("version") == VERSION and "seed" in data and "player" in data:
                return data
        except (OSError, ValueError):
            continue
    return None


def exists(path=None):
    return load(path) is not None
