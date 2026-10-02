"""Einstellungen (V2): werden sofort angewendet und dauerhaft gespeichert.

Speicherort:
    Windows:      %APPDATA%\\LIMINAL\\settings_v2.json
    Linux/macOS:  ~/.config/liminal/settings_v2.json
"""

import json
import os

DEFAULTS = {
    # Maus
    "mouse_enabled": True,
    "mouse_sensitivity": 1.0,     # Grundempfindlichkeit
    "mouse_x": 1.0,               # Faktor links/rechts
    "mouse_y": 1.0,               # Faktor oben/unten
    "invert_x": False,
    "invert_y": False,
    "mouse_smoothing": 0.35,      # 0 = direkt, 0.9 = sehr weich
    # Grafik & Anzeige
    "render_mode": "hires",
    "smooth_light": True,
    "fast": False,
    "fov": 0,                     # 0 = automatisch
    "max_fps": 60,
    "bob": True,
    "walk_speed": 3.0,
    "maximize": True,
}

MOUSE_KEYS = ("mouse_enabled", "mouse_sensitivity", "mouse_x", "mouse_y",
              "invert_x", "invert_y", "mouse_smoothing")


def default_path():
    if os.name == "nt":
        base = os.environ.get("APPDATA") or os.path.expanduser("~")
        return os.path.join(base, "LIMINAL", "settings_v2.json")
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.join(os.path.expanduser("~"), ".config")
    return os.path.join(base, "liminal", "settings_v2.json")


class Settings:
    def __init__(self, path=None):
        self.path = path or default_path()
        self.values = dict(DEFAULTS)
        self.dirty = False
        self.load()

    def __getitem__(self, key):
        return self.values[key]

    def __setitem__(self, key, value):
        if self.values.get(key) != value:
            self.values[key] = value
            self.dirty = True

    def load(self):
        try:
            with open(self.path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            return
        for k, default in DEFAULTS.items():
            v = data.get(k)
            if v is None:
                continue
            if isinstance(default, bool):
                if isinstance(v, bool):
                    self.values[k] = v
            elif isinstance(default, (int, float)) and isinstance(v, (int, float)) and not isinstance(v, bool):
                self.values[k] = type(default)(v) if isinstance(default, float) else int(v)
            elif isinstance(default, str) and isinstance(v, str):
                self.values[k] = v

    def save(self):
        if not self.dirty:
            return
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            with open(self.path, "w", encoding="utf-8") as f:
                json.dump(self.values, f, indent=2, ensure_ascii=False)
            self.dirty = False
        except OSError:
            pass

    def reset_mouse(self):
        for k in MOUSE_KEYS:
            self[k] = DEFAULTS[k]
