"""Spielstaende (V4): mehrere Slots mit Name, Schwierigkeit und Vorschaubild.

Ablage: %APPDATA%\\LIMINAL\\saves_v4\\slot_<id>.json (bzw. ~/.local/share/liminal).
Die Welt selbst entsteht aus dem Seed jedes Mal identisch und wird nicht
gespeichert. Schreiben erfolgt atomar (temporaere Datei + Umbenennen), der
vorige Stand bleibt als .bak erhalten und wird bei Beschaedigung geladen.
Ein vorhandener V3-Spielstand (save_v3.json) erscheint automatisch in der Liste.
"""

import glob
import json
import os
import time

VERSION = 4


def _base():
    if os.environ.get("LIMINAL_SAVE_DIR"):
        return os.environ["LIMINAL_SAVE_DIR"]
    if os.name == "nt":
        return os.path.join(os.environ.get("APPDATA") or os.path.expanduser("~"), "LIMINAL")
    return os.path.join(os.environ.get("XDG_DATA_HOME") or os.path.join(os.path.expanduser("~"), ".local", "share"),
                        "liminal")


def save_dir():
    return os.path.join(_base(), "saves_v4")


def slot_path(slot):
    return os.path.join(save_dir(), "slot_%s.json" % slot)


def new_slot():
    return time.strftime("%Y%m%d_%H%M%S")


def thumbnail(fb, W, H, tw=32, th=16):
    """Verkleinertes Bild (15-Bit-Farben) fuer die Spielstandliste."""
    return [fb[(y * H // th) * W + (x * W // tw)] >> 3 for y in range(th) for x in range(tw)]


def save(game, slot, path=None):
    """Schreibt den Spielstand. Liefert True bei Erfolg."""
    path = path or slot_path(slot)
    p = game.player
    data = {
        "version": VERSION, "slot": slot, "name": game.name, "difficulty": game.difficulty,
        "saved_at": time.strftime("%Y-%m-%d %H:%M:%S"), "seed": game.world.seed,
        "player": {"x": p.x, "y": p.y, "z": p.z, "angle": p.angle, "pitch": p.pitch, "distance": p.distance},
        "stats": game.stats.to_dict(), "inventory": game.inventory.to_list(),
        "selected": game.inventory.selected, "picked": sorted(game.world.picked),
        "thumb": game.thumb or [],
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


def load(slot=None, path=None):
    """Liest einen Spielstand (bei Fehler die Sicherung). None, wenn keiner da ist."""
    if path is None:
        path = slot_path(slot)
        if slot == "v3" and not os.path.exists(path):
            path = _legacy_path()
    for candidate in (path, path + ".bak"):
        try:
            with open(candidate, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("version") in (3, VERSION) and "seed" in data and "player" in data:
                data.setdefault("slot", slot or "v3")
                data.setdefault("name", "Spielstand aus V3")
                data.setdefault("difficulty", "medium")
                return data
        except (OSError, ValueError):
            continue
    return None


def _legacy_path():
    return os.path.join(_base(), "save_v3.json")


def list_saves():
    """Alle Spielstaende, neueste zuerst."""
    out = []
    for fn in glob.glob(os.path.join(save_dir(), "slot_*.json")):
        d = load(os.path.basename(fn)[5:-5])
        if d:
            out.append(d)
    if os.path.exists(_legacy_path()) and not any(d["slot"] == "v3" for d in out):
        d = load("v3")
        if d:
            out.append(d)
    return sorted(out, key=lambda d: d.get("saved_at", ""), reverse=True)


def delete(slot):
    for p in ((_legacy_path(), slot_path(slot)) if slot == "v3" else (slot_path(slot),)):
        for q in (p, p + ".bak"):
            if os.path.exists(q):
                os.remove(q)
