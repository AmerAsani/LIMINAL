"""Raycaster mit variablen Boden- und Deckenhoehen ("2.5D", Doom-artig).

Pro Bildschirmspalte wird ein Strahl per DDA durch das Kachelraster geschickt.
Jede Kachel hat eigene Boden- und Deckenhoehe. Die Spalte wird von aussen nach
innen gefuellt: ein offenes Fenster [top, bot) schrumpft, waehrend der Strahl
Kacheln durchlaeuft.

Beim Uebergang von Kachel A nach Kachel B (Abstand d):
  * Boden und Decke von A zwischen vorheriger Distanz und d zeichnen
  * B massiv          -> Wand ueber das restliche Fenster, fertig
  * Decke B < Decke A -> oberer Absatz (Tuersturz, Traeger, niedrige Decke)
  * Boden B > Boden A -> unterer Absatz (Treppenstufe, Kiste, Podest)
Dadurch entstehen Tueroeffnungen mit Sturz, Treppen, Hallen mit 30 m hohen
Decken und halbhohe Trennwaende, ueber die man hinwegsehen kann.
"""

import math

from .materials import PAT_GRID
from .renderer import BSCALE, NB

OFF = 16777216.0   # Offset: int() schneidet dann wie floor() ab (Texturkoordinaten)
FACE = (0.80, 0.93, 1.0, 0.70)   # Lichtfaktor je Wandausrichtung (-> plastische Ecken)


class Camera:
    __slots__ = ("x", "y", "eye", "angle", "pitch", "fov")

    def __init__(self, x=0.0, y=0.0, eye=1.6, angle=0.0, pitch=0.0, fov=math.radians(80)):
        self.x, self.y, self.eye, self.angle, self.pitch, self.fov = x, y, eye, angle, pitch, fov


class Raycaster:
    def __init__(self, world, shader):
        self.world = world
        self.shader = shader
        self.steps = 0          # DDA-Schritte im letzten Frame (Debug)

    def render(self, fb, W, H, cam, aspect):
        world = self.world
        shader = self.shader
        cells_of = world.cells
        load = world.load_chunk_now
        flick = world.flicker
        mats = shader.bank.items
        lut_get = shader.lut
        plane_get = shader.plane
        sqrt = math.sqrt

        px, py, eye = cam.x, cam.y, cam.eye
        dirx = math.cos(cam.angle)
        diry = math.sin(cam.angle)
        th = math.tan(cam.fov * 0.5)
        plx = -diry * th
        ply = dirx * th
        ph = (W * 0.5) / th
        pv = ph / aspect
        hz = H * 0.5 + cam.pitch * H
        maxd = shader.max_dist
        fogc = shader.fog_code
        pxo = px + OFF
        pyo = py + OFF
        mx0 = int(math.floor(px))
        my0 = int(math.floor(py))
        colw = 2.0 * th / W          # Weltbreite einer Spalte in 1 m Abstand
        end = H * W

        fplanes = {}
        cplanes = {}
        walls = {}
        steps = 0

        for col in range(W):
            camx = (2.0 * col + 1.0) / W - 1.0
            rdx = dirx + plx * camx
            rdy = diry + ply * camx
            if rdx == 0.0:
                rdx = 1e-9
            if rdy == 0.0:
                rdy = 1e-9
            ddx = abs(1.0 / rdx)
            ddy = abs(1.0 / rdy)
            mx = mx0
            my = my0
            if rdx < 0.0:
                stx = -1
                sdx = (px - mx) * ddx
            else:
                stx = 1
                sdx = (mx + 1.0 - px) * ddx
            if rdy < 0.0:
                sty = -1
                sdy = (py - my) * ddy
            else:
                sty = 1
                sdy = (my + 1.0 - py) * ddy

            ck = (mx >> 4, my >> 4)
            cc = cells_of.get(ck)
            if cc is None:
                cc = load(ck)
            cur = cc[((my & 15) << 4) | (mx & 15)]
            if cur is None:
                fb[col:end:W] = [0] * H
                continue
            top = 0
            bot = H

            while True:
                steps += 1
                if sdx < sdy:
                    d = sdx
                    sdx += ddx
                    mx += stx
                    side = 0
                else:
                    d = sdy
                    sdy += ddy
                    my += sty
                    side = 1
                if d < 0.05:
                    d = 0.05
                fl = cur[0]
                ce = cur[1]
                li = cur[5]
                own = cur[6]
                if own in flick:
                    li *= flick[own]
                lq = int(li * 32.0)
                q = pv / d

                # --- Boden der aktuellen Kachel ---------------------------------
                dz = eye - fl
                if dz > 0.0:
                    s = int(hz + 0.5 + dz * q)
                    if s < bot:
                        if s < top:
                            s = top
                        key = (int(dz * 50.0 + 0.5), cur[2], lq)
                        t = fplanes.get(key)
                        if t is None:
                            t = plane_get(dz, cur[2], lq, False, hz, H, pv, colw)
                            fplanes[key] = t
                        codes = t[0]
                        tex = t[6]
                        if t[3] == 0 or tex >= bot:
                            fb[s * W + col:bot * W + col:W] = codes[s:bot]
                        else:
                            a = tex if tex > s else s
                            if a > s:
                                fb[s * W + col:a * W + col:W] = codes[s:a]
                            alt = t[1]
                            dist = t[2]
                            inv = t[4]
                            if t[3] == PAT_GRID:
                                lw = t[5]
                                for y in range(a, bot):
                                    dd = dist[y]
                                    wx = (pxo + rdx * dd) * inv
                                    wy = (pyo + rdy * dd) * inv
                                    line = lw[y]
                                    if wx - int(wx) < line or wy - int(wy) < line:
                                        fb[y * W + col] = alt[y]
                                    else:
                                        fb[y * W + col] = codes[y]
                            else:
                                for y in range(a, bot):
                                    dd = dist[y]
                                    if (int((pxo + rdx * dd) * inv) + int((pyo + rdy * dd) * inv)) & 1:
                                        fb[y * W + col] = alt[y]
                                    else:
                                        fb[y * W + col] = codes[y]
                        bot = s

                # --- Decke der aktuellen Kachel --------------------------------
                cz = ce - eye
                if cz > 0.0:
                    e = int(hz + 0.5 - cz * q)
                    if e > top:
                        if e > bot:
                            e = bot
                        key = (int(cz * 50.0 + 0.5), cur[3], lq)
                        t = cplanes.get(key)
                        if t is None:
                            t = plane_get(cz, cur[3], lq, True, hz, H, pv, colw)
                            cplanes[key] = t
                        codes = t[0]
                        tex = t[6]
                        if t[3] == 0 or tex <= top:
                            fb[top * W + col:e * W + col:W] = codes[top:e]
                        else:
                            b2 = tex if tex < e else e
                            alt = t[1]
                            dist = t[2]
                            inv = t[4]
                            lw = t[5]
                            for y in range(top, b2):
                                dd = dist[y]
                                wx = (pxo + rdx * dd) * inv
                                wy = (pyo + rdy * dd) * inv
                                line = lw[y]
                                if wx - int(wx) < line or wy - int(wy) < line:
                                    fb[y * W + col] = alt[y]
                                else:
                                    fb[y * W + col] = codes[y]
                            if e > b2:
                                fb[b2 * W + col:e * W + col:W] = codes[b2:e]
                        top = e
                if top >= bot:
                    break

                # --- naechste Kachel ------------------------------------------------
                k2 = (mx >> 4, my >> 4)
                if k2 != ck:
                    ck = k2
                    cc = cells_of.get(ck)
                    if cc is None:
                        cc = load(ck)
                nxt = cc[((my & 15) << 4) | (mx & 15)]
                if side == 0:
                    face = 0 if stx > 0 else 1
                else:
                    face = 2 if sty > 0 else 3

                if nxt is None:
                    # massive Wand
                    wm = cur[4]
                    lqf = int(lq * FACE[face])
                    lut = walls.get((wm, lqf))
                    if lut is None:
                        lut = lut_get(wm, lqf)
                        walls[(wm, lqf)] = lut
                    bi = int(sqrt(d) * BSCALE)
                    if bi >= NB:
                        bi = NB - 1
                    code = lut[bi]
                    m = mats[wm]
                    if m.vseam and d < 16.0:
                        wu = (py + d * rdy) if side == 0 else (px + d * rdx)
                        u = (wu + OFF) / m.vseam
                        foot = d * colw / m.vseam
                        if foot < 0.45 and u - int(u) < 0.035 + foot:
                            dl = lut_get(wm, int(lqf * m.seam_dark))
                            code = dl[bi]
                    fb[top * W + col:bot * W + col:W] = [code] * (bot - top)
                    if bot - top > 3:
                        if m.hseam and m.hseam * q >= 3.0:
                            dl = lut_get(wm, int(lqf * m.seam_dark))
                            dcode = dl[bi]
                            k = 1
                            z = fl + m.hseam
                            while z < ce:
                                y = int(hz + 0.5 - (z - eye) * q)
                                if top <= y < bot:
                                    fb[y * W + col] = dcode
                                k += 1
                                z = fl + k * m.hseam
                        for z0, z1, bm in m.bands:
                            ya = int(hz + 0.5 - (fl + z1 - eye) * q)
                            yb = int(hz + 0.5 - (fl + z0 - eye) * q)
                            if ya < top:
                                ya = top
                            if yb > bot:
                                yb = bot
                            if yb > ya:
                                bl = lut_get(bm, lqf)
                                fb[ya * W + col:yb * W + col:W] = [bl[bi]] * (yb - ya)
                    break

                # oberer Absatz (Tuersturz / niedrigere Decke dahinter)
                nce = nxt[1]
                if nce < ce:
                    e = int(hz + 0.5 - (nce - eye) * q)
                    if e > top:
                        if e > bot:
                            e = bot
                        wm = cur[4]
                        lqf = int(lq * FACE[face])
                        lut = walls.get((wm, lqf))
                        if lut is None:
                            lut = lut_get(wm, lqf)
                            walls[(wm, lqf)] = lut
                        bi = int(sqrt(d) * BSCALE)
                        fb[top * W + col:e * W + col:W] = [lut[bi if bi < NB else NB - 1]] * (e - top)
                        top = e
                # unterer Absatz (Stufe, Kiste, Podest)
                nfl = nxt[0]
                if nfl > fl:
                    s = int(hz + 0.5 + (eye - nfl) * q)
                    if s < top:
                        s = top
                    if s < bot:
                        wm = nxt[4]
                        nli = nxt[5]
                        if nxt[6] in flick:
                            nli *= flick[nxt[6]]
                        lqf = int(nli * 32.0 * FACE[face])
                        lut = walls.get((wm, lqf))
                        if lut is None:
                            lut = lut_get(wm, lqf)
                            walls[(wm, lqf)] = lut
                        bi = int(sqrt(d) * BSCALE)
                        fb[s * W + col:bot * W + col:W] = [lut[bi if bi < NB else NB - 1]] * (bot - s)
                        bot = s
                if top >= bot:
                    break
                if d > maxd:
                    fb[top * W + col:bot * W + col:W] = [fogc] * (bot - top)
                    break
                cur = nxt

        self.steps = steps
