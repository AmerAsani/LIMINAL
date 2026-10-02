"""Hauptschleife, Kommandozeile und Benchmark.

V2: Menue (ESC), Maussteuerung, gespeicherte Einstellungen, Fenster wird beim
Start maximiert, weiche Beleuchtung, Delta-Ausgabe an das Terminal.
V3: winkeltreue Perspektive, Items (E aufnehmen, F benutzen), Inventar (Tab,
Schnellleiste 1-9/Mausrad), Health & Sanity, roter Bildschirmrand, Tod,
Autosave alle 3 Minuten und Fortsetzen des letzten Spielstands.
"""

import argparse
import math
import os
import random
import time

from . import savegame
from .hud import Hud
from .inventory import HOTBAR, Inventory
from .items import ITEMS
from .menu import Menu
from .mouse import MouseLook
from .player import Player
from .projection import Projector, panini_d
from .raycaster import Camera, Raycaster, Sprite
from .renderer import Renderer, Shader
from .rooms import Door
from .settings import Settings
from .snapshot import save_html
from .stats import Stats
from .terminal import Input, Terminal
from .ui import InventoryScreen, death_screen
from .world import World


class _NoInput:
    """Eingabe waehrend Menue/Inventar offen sind: Spieler bleibt stehen."""

    def held(self, k):
        return False

    def sprint(self):
        return False


class Game:
    AUTOSAVE = 180.0          # Sekunden Spielzeit zwischen automatischen Speicherungen

    def __init__(self, args, settings=None):
        self.args = args
        self.settings = settings or Settings()
        s = self.settings
        # Kommandozeile ueberschreibt Einstellungen nur fuer diese Sitzung
        if args.mode:
            s.values["render_mode"] = args.mode
        if args.fast:
            s.values["fast"] = True
        if args.no_bob:
            s.values["bob"] = False
        if args.speed:
            s.values["walk_speed"] = args.speed
        if args.max_fps:
            s.values["max_fps"] = args.max_fps
        if args.no_mouse:
            s.values["mouse_enabled"] = False
        if args.no_maximize:
            s.values["maximize"] = False

        # Letzten Spielstand fortsetzen (ausser bei --seed, --new oder Benchmark)
        self.save_path = args.save_file or savegame.default_path()
        data = None
        if not args.benchmark and not args.new and args.seed is None:
            data = savegame.load(self.save_path)
        self.has_save = savegame.exists(self.save_path) if not args.benchmark else False
        if args.seed is None:
            args.seed = data["seed"] if data else random.randrange(1, 2 ** 31)

        self.renderer = Renderer(s["render_mode"], args.colors)
        self.hud = Hud()
        self.hud.debug = args.debug
        self.inp = Input()
        self.mouse = MouseLook(s, self.inp)
        self.menu = Menu(self)
        self.inv_screen = InventoryScreen(self)
        self.hscale = 2 if s["fast"] else 1
        self.cols = self.rows = 0
        self.W = self.H = 0
        self.fb = []
        self.zb = []
        self.fps = 0.0
        self.t_render = 0.0
        self.t_present = 0.0
        self.need_clear = False
        self.max_fps = s["max_fps"]
        self._last_fb = None
        self.autosave_t = 0.0
        self.saved_until = 0.0
        self.target_item = None
        self.loaded_message = None
        self._init_world(args.seed)
        self.stats = Stats()
        self.inventory = Inventory()
        if data:
            self.apply_save(data)
            self.loaded_message = "Spielstand geladen (%s)" % data.get("saved_at", "")
        self.apply_settings()

    # --- Welt / Spielstand ------------------------------------------------------------
    def _init_world(self, seed):
        self.world = World(seed, load_radius=self.args.load_radius)
        x, y, z, a = self.world.find_spawn()
        old = getattr(self, "player", None)
        self.player = Player(x, y, z, a)
        if old is not None:
            self.player.walk_speed = old.walk_speed
            self.player.bob_enabled = old.bob_enabled
        self.shader = Shader(self.world.bank)
        smooth = self.raycaster.smooth if getattr(self, "raycaster", None) else True
        self.raycaster = Raycaster(self.world, self.shader)
        self.raycaster.smooth = smooth
        self.projector = Projector(self.raycaster)
        self.fog = None
        self.fog_density = 0.05
        self._item_mats = {}
        self.target_item = None
        self._hinted = set()
        self._hint_t = 0.0

    def unstuck(self):
        """Falls der Spieler in Geometrie steckt (z. B. alter Spielstand): naechste freie Stelle."""
        p = self.player
        w = self.world
        w.preload(p.x, p.y)
        if not p._blocked(w, p.x, p.y, p.z):
            return
        bx, by = math.floor(p.x), math.floor(p.y)
        for rad in range(1, 10):
            for dy in range(-rad, rad + 1):
                for dx in range(-rad, rad + 1):
                    if max(abs(dx), abs(dy)) != rad:
                        continue
                    c = w.cell(bx + dx, by + dy)
                    if c is None or c[1] - c[0] < 1.9:
                        continue
                    x, y = bx + dx + 0.5, by + dy + 0.5
                    if not p._blocked(w, x, y, c[0]):
                        p.x, p.y, p.z = x, y, c[0]
                        return

    def apply_save(self, data):
        p = self.player
        pd = data["player"]
        p.x, p.y, p.z = float(pd["x"]), float(pd["y"]), float(pd["z"])
        p.angle = float(pd.get("angle", 0.0))
        p.pitch = float(pd.get("pitch", 0.0))
        p.distance = float(pd.get("distance", 0.0))
        self.stats.from_dict(data.get("stats", {}))
        self.inventory.from_list(data.get("inventory", []))
        self.inventory.selected = int(data.get("selected", 0)) % HOTBAR
        self.world.picked = set(data.get("picked", []))
        self.unstuck()

    def do_save(self, now):
        if self.stats.dead or self.args.benchmark:
            return False
        ok = savegame.save(self, self.save_path)
        if ok:
            self.has_save = True
            self.saved_until = now + 2.5
        else:
            self.hud.toast("Speichern fehlgeschlagen!", now, 3.0)
        return ok

    def new_game(self, now=None):
        now = time.perf_counter() if now is None else now
        self._init_world(random.randrange(1, 2 ** 31))
        self.args.seed = self.world.seed
        self.stats = Stats()
        self.inventory = Inventory()
        self.autosave_t = 0.0
        self.renderer.invalidate()
        self.do_save(now)
        self.hud.toast("Neue Welt  ·  Seed %d" % self.world.seed, now, 3.0)

    def load_last(self, now):
        data = savegame.load(self.save_path)
        if not data:
            self.new_game(now)
            return
        if data["seed"] != self.world.seed:
            self._init_world(data["seed"])
            self.args.seed = data["seed"]
        self.stats = Stats()
        self.inventory = Inventory()
        self.apply_save(data)
        # Nach dem Tod: mit voller Health und etwas Sanity weiterspielen
        self.stats.health = 100.0
        self.stats.sanity = max(30.0, self.stats.sanity)
        self.stats.zero_time = 0.0
        self.autosave_t = 0.0
        self.renderer.invalidate()
        self.world.preload(self.player.x, self.player.y)
        self.hud.toast("Letzter Spielstand geladen (%s)" % data.get("saved_at", ""), now, 3.0)

    # --- Items ---------------------------------------------------------------------------
    def _mats_for(self, kind):
        m = self._item_mats.get(kind)
        if m is None:
            m = {ch: self.world.bank.get(rgb) for ch, rgb in ITEMS[kind].palette.items()}
            self._item_mats[kind] = m
        return m

    def find_target(self):
        """Item, das mit E aufgenommen werden kann (nah, vor dem Spieler, frei sichtbar)."""
        p = self.player
        best = None
        ca, sa = math.cos(p.angle), math.sin(p.angle)
        for it in self.world.items_near(p.x, p.y, 2.3):
            dx, dy = it.x - p.x, it.y - p.y
            d = math.hypot(dx, dy)
            if d > 2.1 or d < 1e-6:
                continue
            if (dx * ca + dy * sa) / d < math.cos(math.radians(42)):
                continue
            if it.z - p.z > 1.9 or p.z - it.z > 1.2:
                continue
            clear = True
            steps = int(max(0.0, d - 0.55) / 0.2)
            for k in range(1, steps + 1):
                t = k * 0.2 / d
                c = self.world.cell(math.floor(p.x + dx * t), math.floor(p.y + dy * t))
                if c is None or c[0] > p.z + 1.2:
                    clear = False
                    break
            if clear and (best is None or d < best[0]):
                best = (d, it)
        return best[1] if best else None

    def pickup(self, now):
        it = self.target_item
        if it is None:
            return
        if self.inventory.add(it.kind):
            self.world.picked.add(it.id)
            self.target_item = None
            self.hud.toast("%s aufgenommen" % it.type.name, now)
        else:
            self.hud.toast("Inventar voll", now)

    def consume_slot(self, idx, now):
        kind = self.inventory.take(idx)
        if kind is None:
            self.hud.toast("Kein Item auf diesem Platz", now, 1.5)
            return
        self.hud.toast(self.stats.consume(kind), now, 3.0)

    def sprites(self, now):
        p = self.player
        out = []
        target = self.target_item
        for it in self.world.items_near(p.x, p.y, min(40.0, self.shader.max_dist + 2.0)):
            t = ITEMS[it.kind]
            c = self.world.cell(math.floor(it.x), math.floor(it.y))
            light = c[5] if c else 0.5
            if c is not None and c[6] in self.world.flicker:
                light *= self.world.flicker[c[6]]
            glow = 0.12
            if it is target:
                glow = 0.35 + 0.15 * math.sin(now * 5.0)
            out.append(Sprite(it.x, it.y, it.z, t.size[0], t.size[1], t.sprite, self._mats_for(it.kind), light, glow))
        return out

    # --- Einstellungen ------------------------------------------------------------------
    def apply_settings(self):
        """Uebernimmt alle Einstellungen sofort (vom Menue aufgerufen)."""
        s = self.settings
        p = self.player
        p.walk_speed = s["walk_speed"]
        p.bob_enabled = s["bob"]
        self.raycaster.smooth = s["smooth_light"]
        self.max_fps = s["max_fps"]
        hs = 2 if s["fast"] else 1
        if s["render_mode"] != self.renderer.mode or hs != self.hscale:
            self.renderer.mode = s["render_mode"]
            self.hscale = hs
            if self.cols:
                self.resize(self.cols, self.rows)

    # --- Hilfsfunktionen ------------------------------------------------------------
    def resize(self, cols, rows):
        self.cols = max(20, cols)
        self.rows = max(8, rows)
        self.W = self.cols
        self.H = self.renderer.pixel_rows(self.rows)
        self.fb = [0] * (self.W * self.H)
        self.zb = [1e9] * (self.W * self.H)
        self.renderer.invalidate()
        self.need_clear = True

    def fov(self):
        """Horizontales Sichtfeld: Einstellung, Kommandozeile oder automatisch (vertikal ~58 Grad)."""
        if self.settings["fov"]:
            return math.radians(self.settings["fov"])
        if self.args.fov:
            return math.radians(self.args.fov)
        aspect = self.cols / (self.rows * 2.0)
        h = 2.0 * math.atan(math.tan(math.radians(29.0)) * aspect)
        return max(math.radians(70), min(math.radians(90), h))

    def current_room(self):
        p = self.player
        c = self.world.cell(int(math.floor(p.x)), int(math.floor(p.y)))
        if c is None:
            return None
        o = c[6]
        if isinstance(o, Door):
            return o.room_a or o.room_b
        return o

    def update_fog(self, dt, room):
        """Nebel gleitet weich zur Stimmung des aktuellen Raums."""
        if room is None:
            return
        tgt, dens = room.fog, room.fog_density
        if self.fog is None:
            self.fog, self.fog_density = tgt, dens
        else:
            k = min(1.0, dt * 1.2)
            self.fog = tuple(a + (b - a) * k for a, b in zip(self.fog, tgt))
            self.fog_density += (dens - self.fog_density) * k
        self.shader.set_fog(self.fog, self.fog_density)

    def render(self, now=0.0):
        p = self.player
        s = self.settings
        cam = Camera(p.x, p.y, p.eye, p.angle + p.sway, p.pitch, self.fov())
        t0 = time.perf_counter()
        sprites = self.sprites(now)
        # V3.1: echte Kamera (Panorama + Umprojektion), siehe projection.py
        aspect = self.renderer.pixel_aspect() * s["aspect_fix"]
        fb = self.projector.render(cam, self.cols, self.H, aspect, self.hscale, sprites,
                                   panini_d(cam.fov, s["projection"]))
        t1 = time.perf_counter()
        self.t_render += (t1 - t0 - self.t_render) * 0.1
        # V3: roter Bildschirmrand bei 0 % Sanity (mit leichtem Herzschlag)
        red = self.stats.red
        if red > 0.02:
            pulse = 2 if (self.stats.zero_time > 0 and math.sin(now * 6.0) > 0.75) else 0
            fb = self.renderer.red_edge(fb, self.cols, self.H, red, pulse)
        return fb

    def _upscale(self):
        """Leistungsmodus: halbe horizontale Aufloesung, Spalten verdoppelt."""
        W, H, cols = self.W, self.H, self.cols
        src = self.fb
        out = [0] * (cols * H)
        for r in range(H):
            row = src[r * W:(r + 1) * W]
            base = r * cols
            out[base:base + 2 * W:2] = row
            out[base + 1:base + 2 * W:2] = row
            if cols > 2 * W:
                out[base + 2 * W:base + cols] = [row[-1]] * (cols - 2 * W)
        return out

    def screenshot(self, fb, now):
        os.makedirs("screenshots", exist_ok=True)
        name = os.path.join("screenshots", "liminal_%d_%s.html" % (self.world.seed, time.strftime("%Y%m%d_%H%M%S")))
        save_html(name, fb, self.cols, self.rows, self.renderer.mode, "LIMINAL - Seed %d" % self.world.seed)
        self.hud.toast("Screenshot gespeichert: " + name, now, 3.0)

    # --- Hauptschleife ------------------------------------------------------------------
    def run(self):
        term = Terminal()
        inp = self.inp
        s = self.settings
        term.start()
        try:
            cols, rows = term.size()
            self.resize(cols, rows)
            msg = "L I M I N A L"
            sub = "erzeuge Welt aus Seed %d ..." % self.world.seed
            term.write("\x1b[2J\x1b[%d;%dH\x1b[38;2;230;214;150m%s\x1b[%d;%dH\x1b[38;2;140;134;112m%s\x1b[0m" % (
                rows // 2, max(1, (cols - len(msg)) // 2), msg, rows // 2 + 2, max(1, (cols - len(sub)) // 2), sub))
            self.world.preload(self.player.x, self.player.y)
            start = time.perf_counter()
            self.hud.help_until = start + 8.0
            if self.loaded_message:
                self.hud.help_until = start + 4.0
                self.hud.toast(self.loaded_message, start + 4.0, 4.0)
                self.hud.message_until = start + 8.0
            last = start
            last_size = start
            last_find = 0.0
            maximized = False
            pilot = _AutoInput(self.world, self.player, self.world.seed) if self.args.demo else None
            if pilot:
                self.hud.help_until = 0.0
                self.hud.toast("Demo: automatische Erkundung - beliebige Taste übernimmt", start, 5.0)
            idle = _NoInput()
            while True:
                now = time.perf_counter()
                dt = min(0.1, now - last)
                last = now
                if self.args.quit_after and now - start > self.args.quit_after:
                    self.do_save(now)
                    return
                term.poll(inp)

                # Terminalfenster finden (fuer Maus) und beim Start maximieren
                if not self.mouse.window_found and now - last_find > (0.2 if now - start < 6 else 1.0):
                    last_find = now
                    if self.mouse.find_window() and s["maximize"] and not maximized and now - start < 6:
                        self.mouse.maximize()
                        maximized = True
                        last_size = 0.0

                dead = self.stats.dead
                for k in inp.pressed():
                    if k == "quit":
                        self.do_save(now)
                        return
                    if pilot and not self.menu.open:
                        pilot = None
                        self.hud.toast("Steuerung übernommen", now)
                    if dead:
                        if k in ("enter", "space"):
                            self.load_last(now)
                        elif k == "n":
                            self.new_game(now)
                        elif k == "esc":
                            return
                        continue
                    if self.menu.open:
                        if self.menu.key(k) == "quit":
                            self.do_save(now)
                            return
                        continue
                    if self.inv_screen.open:
                        self.inv_screen.key(k, now)
                        continue
                    if k == "esc":
                        self.menu.show("main")
                    elif k in ("h", "f1"):
                        self.menu.show("help")
                    elif k == "tab":
                        self.inv_screen.show()
                    elif k == "e":
                        self.pickup(now)
                    elif k == "f":
                        self.consume_slot(self.inventory.selected, now)
                    elif k in "123456789" and len(k) == 1:
                        self._select_slot(int(k) - 1, now)
                    else:
                        self._toggle(k, now)
                wheel = inp.take_wheel()
                if wheel and not (self.menu.open or self.inv_screen.open or dead):
                    self._select_slot((self.inventory.selected - wheel) % HOTBAR, now)

                paused = self.menu.open or self.inv_screen.open or self.stats.dead
                dyaw, dpitch = self.mouse.update(dt, not paused and pilot is None)
                self.player.look(dyaw, dpitch)
                if pilot and not paused:
                    pilot.step(dt)
                self.step(dt, now, idle if paused else (pilot or inp), paused)

                if now - last_size > 0.25:
                    last_size = now
                    c, r = term.size()
                    if (c, r) != (self.cols, self.rows):
                        self.resize(c, r)

                fb = self.render(now)
                self._last_fb = fb
                t1 = time.perf_counter()
                if self.stats.dead:
                    fb = self.renderer.dim(self.renderer.red_edge(fb, self.cols, self.H, 1.0))
                    ov = death_screen(self, self.cols, self.rows)
                elif self.menu.open:
                    fb = self.renderer.dim(fb)
                    ov = self.menu.build(self.cols, self.rows)
                elif self.inv_screen.open:
                    fb = self.renderer.dim(fb)
                    ov = self.inv_screen.build(self.cols, self.rows)
                else:
                    ov = self.hud.build(self, self.cols, self.rows, now)
                out = self.renderer.present(fb, self.cols, self.rows, ov)
                if self.need_clear:
                    out = "\x1b[0m\x1b[2J" + out
                    self.need_clear = False
                if out:
                    term.write(out)
                t2 = time.perf_counter()
                self.t_present += (t2 - t1 - self.t_present) * 0.1

                frame = t2 - now
                max_fps = self.max_fps or 60
                if frame > 0:
                    self.fps += (1.0 / max(frame, 1.0 / max_fps) - self.fps) * 0.08
                if frame < 1.0 / max_fps:
                    time.sleep(1.0 / max_fps - frame)
        finally:
            self.mouse.release()
            s.save()
            term.stop()

    def step(self, dt, now, control, paused=False):
        """Spiellogik eines Frames: Werte, Autosave, Bewegung, Welt, Items."""
        if not paused:
            self.stats.update(dt)
            self.autosave_t += dt
            if self.autosave_t >= self.AUTOSAVE:
                self.autosave_t = 0.0
                self.do_save(now)
        self.player.speed_mul = self.stats.speed_mul
        self.player.update(dt, control, self.world)
        self.world.update(self.player.x, self.player.y, budget_ms=4.0)
        self.world.update_flicker(now)
        room = self.current_room()
        self.update_fog(dt, room)
        self.hud.track_zone(room, now)
        self.target_item = None if paused else self.find_target()
        if not paused:
            self._hint_t -= dt
            if self._hint_t <= 0.0:
                self._hint_t = 0.5
                self._machine_hint(now)

    def _machine_hint(self, now):
        """Dezenter Hinweis, wenn ein Versorgungsraum mit Items in der Naehe ist."""
        p = self.player
        for sec in self.world.sectors.values():
            for r in sec.supply_rooms:
                if r.rid in self._hinted:
                    continue
                if not any(it.id not in self.world.picked for it in r.items):
                    continue
                cx, cy = r.center()
                if (cx - p.x) ** 2 + (cy - p.y) ** 2 < 22.0 * 22.0:
                    self._hinted.add(r.rid)
                    self.hud.toast("Ganz in der Nähe summt ein Automat ...", now, 3.5)
                    return

    def _select_slot(self, idx, now):
        self.inventory.selected = idx
        it = self.inventory.selected_item()
        if it:
            self.hud.toast(it.name, now, 1.2)

    def _toggle(self, k, now):
        s = self.settings
        if k in ("f3", "i"):
            self.hud.debug = not self.hud.debug
        elif k == "m":
            self.hud.map = not self.hud.map
        elif k == "v":
            modes = Renderer.MODES
            s["render_mode"] = modes[(modes.index(s["render_mode"]) + 1) % len(modes)]
            self.apply_settings()
            names = {"ascii": "ASCII-Schattierung", "hires": "Halbblock-HiRes", "mono": "Monochrom"}
            self.hud.toast("Darstellung: " + names[self.renderer.mode], now)
        elif k == "l":
            s["fast"] = not s["fast"]
            self.apply_settings()
            self.hud.toast("Leistungsmodus " + ("an" if s["fast"] else "aus"), now)
        elif k == "b":
            s["bob"] = not s["bob"]
            self.apply_settings()
            self.hud.toast("Head-Bobbing " + ("an" if s["bob"] else "aus"), now)
        elif k in ("+", "-"):
            v = s["walk_speed"] + (0.5 if k == "+" else -0.5)
            s["walk_speed"] = max(1.0, min(8.0, v))
            self.apply_settings()
            self.hud.toast("Laufgeschwindigkeit %.1f m/s" % s["walk_speed"], now)
        elif k == "p" and self._last_fb:
            self.screenshot(self._last_fb, now)


# --------------------------------------------------------------------------------------
class _AutoInput:
    """Skriptgesteuerte Eingabe fuer Demo und Benchmark.

    Waehlt regelmaessig die Richtung mit der laengsten freien Strecke und dreht
    sich weich dorthin - ein ziellos wandernder Besucher.
    """

    def __init__(self, world, player, seed):
        self.world = world
        self.player = player
        self.rng = random.Random(seed)
        self.target = player.angle
        self.timer = 0.0
        self.stuck = 0.0

    def _free(self, ang, limit=24.0):
        p = self.player
        dx, dy = math.cos(ang), math.sin(ang)
        z = p.z
        d = 0.0
        while d < limit:
            d += 0.3
            c = self.world.cell(int(math.floor(p.x + dx * d)), int(math.floor(p.y + dy * d)))
            if c is None or c[0] - z > 0.5 or c[1] - c[0] < 1.8:
                return d
            z = c[0]
        return limit

    def _delta(self):
        return (self.target - self.player.angle + math.pi) % (2.0 * math.pi) - math.pi

    def step(self, dt):
        self.timer -= dt
        p = self.player
        moved = math.hypot(p.vx, p.vy)
        self.stuck = self.stuck + dt if (moved < 0.3 and abs(self._delta()) < 0.5) else 0.0
        if self.stuck > 0.6:
            self.stuck = 0.0
            self.target = p.angle + self.rng.uniform(1.0, 5.3)
            self.timer = 1.5
            return
        aligned = abs(self._delta()) < 0.3
        if self.timer <= 0.0 or (aligned and self._free(p.angle, 2.5) < 1.6):
            best = None
            for i in range(16):
                a = p.angle + (i / 16.0) * 2.0 * math.pi
                score = self._free(a) * self.rng.uniform(0.6, 1.0)
                if i in (7, 8, 9):
                    score *= 0.3          # nicht staendig umkehren
                if best is None or score > best[0]:
                    best = (score, a)
            self.target = best[1]
            self.timer = self.rng.uniform(2.0, 5.0)

    def held(self, k):
        d = self._delta()
        if k == "w":
            return abs(d) < 0.5
        if k == "right":
            return d > 0.08
        if k == "left":
            return d < -0.08
        return False

    def sprint(self):
        return True


def benchmark(args):
    game = Game(args, Settings(os.devnull))
    w, h = (int(v) for v in args.size.lower().split("x"))
    game.resize(w, h)
    game.hud.map = args.debug
    game.world.preload(game.player.x, game.player.y)
    inp = _AutoInput(game.world, game.player, args.seed)
    dt = 1.0 / 30.0
    frames = args.benchmark
    t_r = t_p = t_u = 0.0
    worst = 0.0
    bytes_out = 0
    for i in range(frames):
        t0 = time.perf_counter()
        inp.step(dt)
        game.player.update(dt, inp, game.world)
        game.world.update(game.player.x, game.player.y, budget_ms=4.0)
        game.world.update_flicker(i * dt)
        game.update_fog(dt, game.current_room())
        game.stats.update(dt)
        game.target_item = game.find_target()
        t1 = time.perf_counter()
        fb = game.render(i * dt)
        t2 = time.perf_counter()
        out = game.renderer.present(fb, game.cols, game.rows, game.hud.build(game, game.cols, game.rows, 99.0))
        t3 = time.perf_counter()
        bytes_out += len(out.encode("utf-8"))
        t_u += t1 - t0
        t_r += t2 - t1
        t_p += t3 - t2
        worst = max(worst, t3 - t0)
    total = (t_u + t_r + t_p) / frames
    print("Benchmark %s, Modus %s, weiche Beleuchtung %s, %d Frames, Strecke %.0f m" % (
        args.size, game.renderer.mode, "an" if game.raycaster.smooth else "aus", frames, game.player.distance))
    print("  Update %.1f ms | Raycast %.1f ms | Ausgabe %.1f ms | gesamt %.1f ms  ->  ~%.0f FPS (langsamster Frame %.0f ms)" % (
        t_u / frames * 1000, t_r / frames * 1000, t_p / frames * 1000, total * 1000, 1.0 / total, worst * 1000))
    print("  Terminal-Daten: %.0f KB pro Frame (Delta-Ausgabe)" % (bytes_out / frames / 1024))
    print("  Geladen: %d Chunks, %d Sektoren, %d Raeume, erzeugt %d Chunks" % (
        len(game.world.chunks), len(game.world.sectors), game.world.loaded_rooms(), game.world.generated_chunks))
    if args.snapshot:
        save_html(args.snapshot, fb, game.cols, game.rows, game.renderer.mode)
        print("  Snapshot:", args.snapshot)


def parse_args(argv=None):
    ap = argparse.ArgumentParser(prog="LIMINAL", description="LIMINAL V3 - unendliche 3D-Innenräume im Terminal")
    ap.add_argument("--seed", type=int, default=None, help="Welt-Seed (startet eine neue Welt mit diesem Seed)")
    ap.add_argument("--new", action="store_true", help="neue Welt statt den letzten Spielstand fortzusetzen")
    ap.add_argument("--save-file", default=None, help="eigene Datei für den Spielstand")
    ap.add_argument("--mode", choices=Renderer.MODES, default=None,
                    help="hires = Halbblöcke (Standard), ascii = Schattierungszeichen, mono = ohne Farbe")
    ap.add_argument("--colors", choices=("truecolor", "256"), default=None, help="Farbtiefe (Standard: automatisch)")
    ap.add_argument("--fov", type=float, default=None, help="horizontales Sichtfeld in Grad")
    ap.add_argument("--speed", type=float, default=None, help="Laufgeschwindigkeit in m/s")
    ap.add_argument("--max-fps", type=int, default=None, help="Obergrenze der Bildrate")
    ap.add_argument("--load-radius", type=int, default=4, help="Laderadius in Chunks (16 Kacheln)")
    ap.add_argument("--debug", action="store_true", help="mit Debug-Anzeige starten")
    ap.add_argument("--no-bob", action="store_true", help="Head-Bobbing ausschalten")
    ap.add_argument("--no-mouse", action="store_true", help="Maussteuerung für diese Sitzung aus")
    ap.add_argument("--no-maximize", action="store_true", help="Fenster beim Start nicht maximieren")
    ap.add_argument("--fast", action="store_true", help="Leistungsmodus: halbe horizontale Auflösung")
    ap.add_argument("--demo", action="store_true", help="Autopilot erkundet selbstständig (Taste drücken zum Übernehmen)")
    ap.add_argument("--quit-after", type=float, default=0.0, metavar="SEK", help="nach SEK Sekunden automatisch beenden")
    ap.add_argument("--benchmark", type=int, default=0, metavar="FRAMES", help="ohne Terminal messen und beenden")
    ap.add_argument("--size", default="160x48", help="Größe für --benchmark (SpaltenxZeilen)")
    ap.add_argument("--snapshot", default=None, help="mit --benchmark: letzten Frame als HTML speichern")
    args = ap.parse_args(argv)
    if args.colors is None:
        ct = os.environ.get("COLORTERM", "").lower()
        modern = "truecolor" in ct or "24bit" in ct or os.name == "nt" or "WT_SESSION" in os.environ
        args.colors = "truecolor" if modern else "256"
    return args


def main(argv=None):
    args = parse_args(argv)
    if args.benchmark:
        if args.seed is None:
            args.seed = random.randrange(1, 2 ** 31)
        benchmark(args)
        return
    game = Game(args)
    try:
        game.run()
    except KeyboardInterrupt:
        game.do_save(time.perf_counter())
    log = os.environ.get("LIMINAL_LOG")
    if log:
        with open(log, "a", encoding="utf-8") as f:
            f.write("seed=%d size=%dx%d mode=%s fps=%.1f raycast=%.1fms output=%.1fms dist=%.0fm "
                    "health=%.0f sanity=%.1f items=%d saved=%s window=%s\n" % (
                        args.seed, game.cols, game.rows, game.renderer.mode, game.fps,
                        game.t_render * 1000, game.t_present * 1000, game.player.distance,
                        game.stats.health, game.stats.sanity, game.inventory.count(),
                        game.has_save, game.mouse.window_found))
    starter = "LIMINAL.exe" if os.environ.get("LIMINAL_LAUNCHER") else "python liminal3d.py"
    print("LIMINAL beendet. Seed %d  -  zurückgelegte Strecke: %.0f m" % (args.seed, game.player.distance))
    if game.has_save and not game.stats.dead:
        print("Spielstand gespeichert - beim nächsten Start geht es genau hier weiter.")
    print("Neue Welt mit diesem Seed:  %s --seed %d" % (starter, args.seed))
