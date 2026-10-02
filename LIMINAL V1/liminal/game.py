"""Hauptschleife, Kommandozeile und Benchmark."""

import argparse
import math
import os
import random
import time

from .hud import Hud
from .player import Player
from .raycaster import Camera, Raycaster
from .renderer import Renderer, Shader
from .rooms import Door
from .snapshot import save_html
from .terminal import Input, Terminal
from .world import World


class Game:
    def __init__(self, args):
        self.args = args
        self.world = World(args.seed, load_radius=args.load_radius)
        x, y, z, a = self.world.find_spawn()
        self.player = Player(x, y, z, a)
        self.player.walk_speed = args.speed
        self.player.bob_enabled = not args.no_bob
        self.shader = Shader(self.world.bank)
        self.renderer = Renderer(args.mode, args.colors)
        self.raycaster = Raycaster(self.world, self.shader)
        self.hud = Hud()
        self.hud.debug = args.debug
        self.hscale = 2 if args.fast else 1
        self.cols = self.rows = 0
        self.W = self.H = 0
        self.fb = []
        self.fps = 0.0
        self.t_render = 0.0
        self.t_present = 0.0
        self.fog = None
        self.fog_density = 0.05

    # --- Hilfsfunktionen ------------------------------------------------------------
    def resize(self, cols, rows):
        self.cols = max(20, cols)
        self.rows = max(8, rows)
        self.W = max(10, self.cols // self.hscale)
        self.H = self.renderer.pixel_rows(self.rows)
        self.fb = [0] * (self.W * self.H)

    def fov(self):
        """Horizontales Sichtfeld passend zum Seitenverhaeltnis (vertikal ~58 Grad)."""
        if self.args.fov:
            return math.radians(self.args.fov)
        aspect = self.cols / (self.rows * 2.0)
        h = 2.0 * math.atan(math.tan(math.radians(29.0)) * aspect)
        return max(math.radians(70), min(math.radians(100), h))

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

    def render(self):
        p = self.player
        cam = Camera(p.x, p.y, p.eye, p.angle + p.sway, p.pitch, self.fov())
        t0 = time.perf_counter()
        self.raycaster.render(self.fb, self.W, self.H, cam, self.renderer.pixel_aspect() / self.hscale)
        t1 = time.perf_counter()
        self.t_render += (t1 - t0 - self.t_render) * 0.1
        return self.fb if self.hscale == 1 else self._upscale()

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
        name = "liminal_%d_%s.html" % (self.world.seed, time.strftime("%Y%m%d_%H%M%S"))
        save_html(name, fb, self.cols, self.rows, self.renderer.mode, "LIMINAL - Seed %d" % self.world.seed)
        self.hud.toast("Screenshot gespeichert: " + name, now, 3.0)

    # --- Hauptschleife ------------------------------------------------------------------
    def run(self):
        term = Terminal()
        inp = Input()
        term.start()
        try:
            cols, rows = term.size()
            self.resize(cols, rows)
            msg = "L I M I N A L"
            sub = "erzeuge Welt aus Seed %d ..." % self.world.seed
            term.write("\x1b[2J\x1b[%d;%dH\x1b[38;2;230;220;170m%s\x1b[%d;%dH\x1b[38;2;140;140;130m%s\x1b[0m" % (
                rows // 2, max(1, (cols - len(msg)) // 2), msg, rows // 2 + 2, max(1, (cols - len(sub)) // 2), sub))
            self.world.preload(self.player.x, self.player.y)
            start = time.perf_counter()
            self.hud.help_until = start + 9.0
            last = start
            last_size = start
            moved_once = False
            max_fps = self.args.max_fps
            pilot = _AutoInput(self.world, self.player, self.world.seed) if self.args.demo else None
            if pilot:
                self.hud.help_until = 0.0
                self.hud.toast("Demo: automatische Erkundung - beliebige Taste uebernimmt", start, 5.0)
            while True:
                now = time.perf_counter()
                dt = min(0.1, now - last)
                last = now
                if self.args.quit_after and now - start > self.args.quit_after:
                    return
                term.poll(inp)
                for k in inp.pressed():
                    if k in ("esc", "quit"):
                        return
                    if pilot:
                        pilot = None
                        self.hud.toast("Steuerung uebernommen", now)
                    self._toggle(k, now, term)
                if not moved_once and any(inp.held(k) for k in ("w", "a", "s", "d")):
                    moved_once = True
                    if now - start > 1.5:
                        self.hud.help_until = min(self.hud.help_until, now + 1.0)

                if pilot:
                    pilot.step(dt)
                    self.player.update(dt, pilot, self.world)
                else:
                    self.player.update(dt, inp, self.world)
                self.world.update(self.player.x, self.player.y, budget=2)
                self.world.update_flicker(now)
                room = self.current_room()
                self.update_fog(dt, room)
                self.hud.track_zone(room, now)

                if now - last_size > 0.5:
                    last_size = now
                    c, r = term.size()
                    if (c, r) != (self.cols, self.rows):
                        self.resize(c, r)
                        term.write("\x1b[0m\x1b[2J")

                fb = self.render()
                t1 = time.perf_counter()
                ov = self.hud.build(self, self.cols, self.rows, now)
                term.write(self.renderer.present(fb, self.cols, self.rows, ov))
                t2 = time.perf_counter()
                self.t_present += (t2 - t1 - self.t_present) * 0.1
                self._last_fb = fb

                frame = t2 - now
                if frame > 0:
                    self.fps += (1.0 / max(frame, 1.0 / max_fps) - self.fps) * 0.08
                if max_fps and frame < 1.0 / max_fps:
                    time.sleep(1.0 / max_fps - frame)
        finally:
            term.stop()

    def _toggle(self, k, now, term):
        p = self.player
        if k in ("f3", "i"):
            self.hud.debug = not self.hud.debug
        elif k in ("m", "tab"):
            self.hud.map = not self.hud.map
        elif k in ("h", "f1"):
            self.hud.help_until = 0.0 if now < self.hud.help_until else now + 3600.0
        elif k == "v":
            self.renderer.cycle_mode()
            self.resize(self.cols, self.rows)
            names = {"ascii": "ASCII-Schattierung", "hires": "Halbblock-HiRes", "mono": "Monochrom"}
            self.hud.toast("Darstellung: " + names[self.renderer.mode], now)
        elif k == "f":
            self.hscale = 1 if self.hscale == 2 else 2
            self.resize(self.cols, self.rows)
            self.hud.toast("Leistungsmodus " + ("an" if self.hscale == 2 else "aus"), now)
        elif k == "b":
            p.bob_enabled = not p.bob_enabled
            self.hud.toast("Head-Bobbing " + ("an" if p.bob_enabled else "aus"), now)
        elif k == "+":
            p.walk_speed = min(9.0, p.walk_speed + 0.5)
            self.hud.toast("Laufgeschwindigkeit %.1f m/s" % p.walk_speed, now)
        elif k == "-":
            p.walk_speed = max(0.5, p.walk_speed - 0.5)
            self.hud.toast("Laufgeschwindigkeit %.1f m/s" % p.walk_speed, now)
        elif k == "p" and getattr(self, "_last_fb", None):
            self.screenshot(self._last_fb, now)


# --------------------------------------------------------------------------------------
class _AutoInput:
    """Skriptgesteuerte Eingabe fuer den Benchmark.

    Waehlt regelmaessig die Richtung mit der laengsten freien Strecke und dreht
    sich weich dorthin - ein ziellos wandernder Besucher.
    """

    def __init__(self, world, player, seed):
        self.world = world
        self.player = player
        self.rng = random.Random(seed)
        self.target = player.angle
        self.timer = 0.0

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

    def step(self, dt):
        self.timer -= dt
        p = self.player
        self.stuck = getattr(self, "stuck", 0.0)
        moved = math.hypot(p.vx, p.vy)
        self.stuck = self.stuck + dt if (moved < 0.3 and abs(self._delta()) < 0.5) else 0.0
        if self.stuck > 0.6:
            self.stuck = 0.0
            self.target = p.angle + self.rng.uniform(1.0, 5.3)
            self.timer = 1.5
            return
        aligned = abs(self._delta()) < 0.3
        if self.timer <= 0.0 or (aligned and self._free(self.player.angle, 2.5) < 1.6):
            best = None
            for i in range(16):
                a = self.player.angle + (i / 16.0) * 2.0 * math.pi
                score = self._free(a) * self.rng.uniform(0.6, 1.0)
                if i in (7, 8, 9):
                    score *= 0.3          # nicht staendig umkehren
                if best is None or score > best[0]:
                    best = (score, a)
            self.target = best[1]
            self.timer = self.rng.uniform(2.0, 5.0)

    def _delta(self):
        return (self.target - self.player.angle + math.pi) % (2.0 * math.pi) - math.pi

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
    game = Game(args)
    w, h = (int(v) for v in args.size.lower().split("x"))
    game.resize(w, h)
    game.hud.map = args.debug
    game.world.preload(game.player.x, game.player.y)
    inp = _AutoInput(game.world, game.player, args.seed)
    dt = 1.0 / 30.0
    frames = args.benchmark
    t_r = t_p = t_u = 0.0
    worst = 0.0
    for i in range(frames):
        t0 = time.perf_counter()
        inp.step(dt)
        game.player.update(dt, inp, game.world)
        game.world.update(game.player.x, game.player.y, budget=2)
        game.world.update_flicker(i * dt)
        game.update_fog(dt, game.current_room())
        t1 = time.perf_counter()
        fb = game.render()
        t2 = time.perf_counter()
        game.renderer.present(fb, game.cols, game.rows, game.hud.build(game, game.cols, game.rows, 99.0))
        t3 = time.perf_counter()
        t_u += t1 - t0
        t_r += t2 - t1
        t_p += t3 - t2
        worst = max(worst, t3 - t0)
    total = (t_u + t_r + t_p) / frames
    print("Benchmark %s, Modus %s, %d Frames, Strecke %.0f m" % (args.size, args.mode, frames, game.player.distance))
    print("  Update %.1f ms | Raycast %.1f ms | Ausgabe %.1f ms | gesamt %.1f ms  ->  ~%.0f FPS (langsamster Frame %.0f ms)" % (
        t_u / frames * 1000, t_r / frames * 1000, t_p / frames * 1000, total * 1000, 1.0 / total, worst * 1000))
    print("  Geladen: %d Chunks, %d Sektoren, %d Raeume, erzeugt %d Chunks" % (
        len(game.world.chunks), len(game.world.sectors), game.world.loaded_rooms(), game.world.generated_chunks))
    if args.snapshot:
        save_html(args.snapshot, fb, game.cols, game.rows, game.renderer.mode)
        print("  Snapshot:", args.snapshot)


def parse_args(argv=None):
    ap = argparse.ArgumentParser(prog="liminal", description="LIMINAL - unendliche 3D-Innenraeume im Terminal")
    ap.add_argument("--seed", type=int, default=None, help="Welt-Seed (Standard: zufaellig)")
    ap.add_argument("--mode", choices=Renderer.MODES, default="ascii",
                    help="ascii = Schattierungszeichen, hires = Halbbloecke (doppelte Aufloesung), mono = ohne Farbe")
    ap.add_argument("--colors", choices=("truecolor", "256"), default=None, help="Farbtiefe (Standard: automatisch)")
    ap.add_argument("--fov", type=float, default=None, help="horizontales Sichtfeld in Grad (Standard: automatisch)")
    ap.add_argument("--speed", type=float, default=3.0, help="Laufgeschwindigkeit in m/s")
    ap.add_argument("--max-fps", type=float, default=60.0, help="Obergrenze der Bildrate")
    ap.add_argument("--load-radius", type=int, default=4, help="Laderadius in Chunks (16 Kacheln)")
    ap.add_argument("--debug", action="store_true", help="mit Debug-Anzeige starten")
    ap.add_argument("--no-bob", action="store_true", help="Head-Bobbing ausschalten")
    ap.add_argument("--fast", action="store_true", help="Leistungsmodus: halbe horizontale Aufloesung")
    ap.add_argument("--demo", action="store_true", help="Autopilot erkundet selbststaendig (Taste druecken zum Uebernehmen)")
    ap.add_argument("--quit-after", type=float, default=0.0, metavar="SEK", help="nach SEK Sekunden automatisch beenden")
    ap.add_argument("--benchmark", type=int, default=0, metavar="FRAMES", help="ohne Terminal messen und beenden")
    ap.add_argument("--size", default="160x48", help="Groesse fuer --benchmark (SpaltenxZeilen)")
    ap.add_argument("--snapshot", default=None, help="mit --benchmark: letzten Frame als HTML speichern")
    args = ap.parse_args(argv)
    if args.seed is None:
        args.seed = random.randrange(1, 2 ** 31)
    if args.colors is None:
        ct = os.environ.get("COLORTERM", "").lower()
        modern = "truecolor" in ct or "24bit" in ct or os.name == "nt" or "WT_SESSION" in os.environ
        args.colors = "truecolor" if modern else "256"
    return args


def main(argv=None):
    args = parse_args(argv)
    if args.benchmark:
        benchmark(args)
        return
    game = Game(args)
    try:
        game.run()
    except KeyboardInterrupt:
        pass
    log = os.environ.get("LIMINAL_LOG")
    if log:
        with open(log, "a", encoding="utf-8") as f:
            f.write("seed=%d size=%dx%d mode=%s colors=%s fps=%.1f raycast=%.1fms output=%.1fms dist=%.0fm chunks=%d\n" % (
                args.seed, game.cols, game.rows, game.renderer.mode, game.renderer.colors, game.fps,
                game.t_render * 1000, game.t_present * 1000, game.player.distance, len(game.world.chunks)))
    print("LIMINAL beendet. Seed %d  -  zurueckgelegte Strecke: %.0f m" % (args.seed, game.player.distance))
    print("Gleiche Welt erneut betreten:  python liminal3d.py --seed %d" % args.seed)
