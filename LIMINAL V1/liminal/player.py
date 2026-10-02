"""Spieler: First-Person-Bewegung mit Traegheit, Kollision, Stufen und Head-Bobbing."""

import math

RADIUS = 0.24        # Kollisionsradius (m)
BODY = 1.78          # benoetigte Kopffreiheit (m)
EYE = 1.62           # Augenhoehe ueber dem Boden (m)
MAX_STEP = 0.5       # hoechste Stufe, die man hinaufgehen kann (m)
GRAVITY = 18.0


class Player:
    def __init__(self, x, y, z, angle):
        self.x = x
        self.y = y
        self.z = z                 # Fusshoehe
        self.vz = 0.0
        self.angle = angle
        self.pitch = 0.0           # Blick hoch/runter (-1..1, Anteil der Bildhoehe)
        self.vx = 0.0
        self.vy = 0.0
        self.turn_v = 0.0
        self.pitch_v = 0.0
        self.walk_speed = 3.0
        self.sprint_mul = 1.9
        self.turn_speed = 2.3
        self.bob_enabled = True
        self.bob_phase = 0.0
        self.bob_amount = 0.0
        self.distance = 0.0

    # --- Kollision --------------------------------------------------------------
    def _blocked(self, world, x, y, z):
        """True, wenn der Spielerkreis an Position (x, y) mit Fusshoehe z kollidiert."""
        r = RADIUS
        for ty in range(int(math.floor(y - r)), int(math.floor(y + r)) + 1):
            for tx in range(int(math.floor(x - r)), int(math.floor(x + r)) + 1):
                c = world.cell(tx, ty)
                if c is None:
                    return True
                if c[0] - z > MAX_STEP:
                    return True
                if c[1] - max(c[0], z) < BODY:
                    return True
        return False

    def _ground(self, world, x, y):
        """Hoechster Boden unter dem Spielerkreis (fuer fluessiges Treppensteigen)."""
        r = RADIUS * 0.7
        g = -1e9
        for ty in range(int(math.floor(y - r)), int(math.floor(y + r)) + 1):
            for tx in range(int(math.floor(x - r)), int(math.floor(x + r)) + 1):
                c = world.cell(tx, ty)
                if c is not None and c[0] > g and c[0] - self.z <= MAX_STEP + 0.01:
                    g = c[0]
        return g if g > -1e8 else self.z

    # --- Update -------------------------------------------------------------------
    def update(self, dt, inp, world):
        # Drehung mit weicher Beschleunigung
        turn = 0.0
        if inp.held("left") or inp.held("q"):
            turn -= 1.0
        if inp.held("right") or inp.held("e"):
            turn += 1.0
        target = turn * self.turn_speed
        self.turn_v += (target - self.turn_v) * min(1.0, dt * 14.0)
        self.angle = (self.angle + self.turn_v * dt) % (2.0 * math.pi)

        look = 0.0
        if inp.held("up"):
            look += 1.0
        if inp.held("down"):
            look -= 1.0
        self.pitch_v += (look * 1.1 - self.pitch_v) * min(1.0, dt * 14.0)
        self.pitch = max(-0.6, min(0.6, self.pitch + self.pitch_v * dt))

        # Gewuenschte Bewegungsrichtung
        fwd = 0.0
        side = 0.0
        if inp.held("w"):
            fwd += 1.0
        if inp.held("s"):
            fwd -= 1.0
        if inp.held("d"):
            side += 1.0
        if inp.held("a"):
            side -= 1.0
        ca = math.cos(self.angle)
        sa = math.sin(self.angle)
        wx = ca * fwd - sa * side
        wy = sa * fwd + ca * side
        n = math.hypot(wx, wy)
        speed = self.walk_speed * (self.sprint_mul if inp.sprint() else 1.0)
        if n > 0.0:
            wx = wx / n * speed
            wy = wy / n * speed
        accel = 10.0 if n > 0.0 else 12.0
        k = min(1.0, dt * accel)
        self.vx += (wx - self.vx) * k
        self.vy += (wy - self.vy) * k
        if abs(self.vx) < 1e-3 and abs(self.vy) < 1e-3:
            self.vx = self.vy = 0.0

        # Bewegung in Teilschritten, getrennt nach Achsen -> sauberes Sliden an Waenden
        dx = self.vx * dt
        dy = self.vy * dt
        steps = max(1, int(max(abs(dx), abs(dy)) / 0.2) + 1)
        sx = dx / steps
        sy = dy / steps
        ox, oy = self.x, self.y
        for _ in range(steps):
            if sx != 0.0:
                if not self._blocked(world, self.x + sx, self.y, self.z):
                    self.x += sx
                else:
                    self.vx = 0.0
                    sx = 0.0
            if sy != 0.0:
                if not self._blocked(world, self.x, self.y + sy, self.z):
                    self.y += sy
                else:
                    self.vy = 0.0
                    sy = 0.0
        moved = math.hypot(self.x - ox, self.y - oy)
        self.distance += moved

        # Vertikal: Stufen weich hinauf, Absaetze mit Schwerkraft hinunter
        ground = self._ground(world, self.x, self.y)
        if ground > self.z:
            self.z = min(ground, self.z + dt * 5.0)
            self.vz = 0.0
        elif ground < self.z:
            self.vz -= GRAVITY * dt
            self.z += self.vz * dt
            if self.z <= ground:
                self.z = ground
                self.vz = 0.0
        else:
            self.vz = 0.0

        # Head-Bobbing proportional zur tatsaechlichen Geschwindigkeit
        spd = moved / dt if dt > 0 else 0.0
        target_bob = min(1.0, spd / 3.0) if self.bob_enabled else 0.0
        self.bob_amount += (target_bob - self.bob_amount) * min(1.0, dt * 6.0)
        self.bob_phase += dt * (5.5 + spd * 1.3)

    @property
    def eye(self):
        bob = math.sin(self.bob_phase * 2.0) * 0.045 * self.bob_amount
        return self.z + EYE + bob

    @property
    def sway(self):
        """Leichte seitliche Kopfbewegung (in Radiant), passend zum Bobbing."""
        return math.sin(self.bob_phase) * 0.006 * self.bob_amount
