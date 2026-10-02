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

V2 - weiche Beleuchtung:
  Jede Kachel kennt die Helligkeit ihrer vier Ecken (inkl. Ambient Occlusion).
  Fuer Boden und Decke wird das Licht am Eintritts- und Austrittspunkt des
  Strahls bilinear bestimmt und dazwischen je Bildzeile interpoliert
  (Gouraud entlang des Strahls). Waende interpolieren entlang ihrer Kante.
  So entstehen fliessende Lichtverlaeufe statt kachelgrosser Helligkeitsstufen,
  weiche Schatten hinter Saeulen und dunklere Ecken.
"""

import math

from .materials import PAT_GRID
from .renderer import BSCALE, NB
from .world import CM, WALL_AO

OFF = 16777216.0   # Offset: int() schneidet dann wie floor() ab (Texturkoordinaten)
FACE = (0.80, 0.93, 1.0, 0.70)   # Lichtfaktor je Wandausrichtung (-> plastische Ecken)
QMAX = 52                         # hoechste Lichtstufe (1.6 * 32)


class Camera:
    __slots__ = ("x", "y", "eye", "angle", "pitch", "fov")

    def __init__(self, x=0.0, y=0.0, eye=1.6, angle=0.0, pitch=0.0, fov=math.radians(80)):
        self.x, self.y, self.eye, self.angle, self.pitch, self.fov = x, y, eye, angle, pitch, fov


class Raycaster:
    def __init__(self, world, shader):
        self.world = world
        self.shader = shader
        self.steps = 0          # DDA-Schritte im letzten Frame (Debug)
        self.smooth = True      # weiche Beleuchtung an/aus

    def render(self, fb, W, H, cam, aspect):
        world = self.world
        shader = self.shader
        cells_of = world.cells
        corners_of = world.corners
        load = world.load_chunk_now
        flick = world.flicker
        mats = shader.bank.items
        lut_get = shader.lut
        plane_get = shader.plane
        sqrt = math.sqrt
        smooth = self.smooth

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
        luts = {}
        steps = 0

        def lut(mat, q):
            t = luts.get((mat, q))
            if t is None:
                t = lut_get(mat, q)
                luts[(mat, q)] = t
            return t

        def plane_fill(col, a, b, t, mat, la, lb, d0, d1, rdx, rdy, ceiling):
            """Fuellt die Zeilen [a, b) einer Boden-/Deckenebene.

            la/lb: Helligkeit am Eintritts-/Austrittspunkt des Strahls.
            """
            codes, alt, dist, kind, inv, lw, tex, buck = t
            # texturierte Zeilen (nahe genug fuer Muster)
            if kind:
                if ceiling:
                    ta, tb = a, (tex if tex < b else b)
                else:
                    ta, tb = (tex if tex > a else a), b
                if ta >= tb:
                    kind = 0
            qa = int(la * 32.0)
            qb = int(lb * 32.0)
            if not smooth or qa == qb:
                # gleichmaessiges Licht: schnelle Zeilentabelle
                if kind == 0:
                    fb[a * W + col:b * W + col:W] = codes[a:b]
                    return
                if ceiling:
                    if b > tb:
                        fb[tb * W + col:b * W + col:W] = codes[tb:b]
                else:
                    if ta > a:
                        fb[a * W + col:ta * W + col:W] = codes[a:ta]
                if kind == PAT_GRID:
                    for y in range(ta, tb):
                        dd = dist[y]
                        wx = (pxo + rdx * dd) * inv
                        wy = (pyo + rdy * dd) * inv
                        line = lw[y]
                        fb[y * W + col] = alt[y] if (wx - int(wx) < line or wy - int(wy) < line) else codes[y]
                else:
                    for y in range(ta, tb):
                        dd = dist[y]
                        if (int((pxo + rdx * dd) * inv) + int((pyo + rdy * dd) * inv)) & 1:
                            fb[y * W + col] = alt[y]
                        else:
                            fb[y * W + col] = codes[y]
                return

            # weicher Verlauf: Licht linear ueber die Distanz interpolieren
            if qa > qb:
                qmin, qmax = qb, qa
            else:
                qmin, qmax = qa, qb
            if qmin < 0:
                qmin = 0
            if qmax > QMAX:
                qmax = QMAX
            qr = qmax - qmin
            seg = [lut(mat, q) for q in range(qmin, qmax + 1)]
            span = d1 - d0
            k32 = (lb - la) / span * 32.0 if span > 1e-6 else 0.0
            base = la * 32.0 - k32 * d0 - qmin
            # untexturierte Zeilen
            if ceiling:
                rng_plain = range(tb, b) if kind else range(a, b)
            else:
                rng_plain = range(a, ta) if kind else range(a, b)
            for y in rng_plain:
                q = int(base + k32 * dist[y])
                if q < 0:
                    q = 0
                elif q > qr:
                    q = qr
                fb[y * W + col] = seg[q][buck[y]]
            if not kind:
                return
            sd = mats[mat].seam_dark
            segalt = [lut(mat, int((qmin + i) * sd)) for i in range(qr + 1)] if not mats[mat].emissive else seg
            if kind == PAT_GRID:
                for y in range(ta, tb):
                    dd = dist[y]
                    q = int(base + k32 * dd)
                    if q < 0:
                        q = 0
                    elif q > qr:
                        q = qr
                    wx = (pxo + rdx * dd) * inv
                    wy = (pyo + rdy * dd) * inv
                    line = lw[y]
                    if wx - int(wx) < line or wy - int(wy) < line:
                        fb[y * W + col] = segalt[q][buck[y]]
                    else:
                        fb[y * W + col] = seg[q][buck[y]]
            else:
                for y in range(ta, tb):
                    dd = dist[y]
                    q = int(base + k32 * dd)
                    if q < 0:
                        q = 0
                    elif q > qr:
                        q = qr
                    if (int((pxo + rdx * dd) * inv) + int((pyo + rdy * dd) * inv)) & 1:
                        fb[y * W + col] = segalt[q][buck[y]]
                    else:
                        fb[y * W + col] = seg[q][buck[y]]

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
            cl = corners_of[ck]
            cur = cc[((my & 15) << 4) | (mx & 15)]
            if cur is None:
                fb[col:end:W] = [0] * H
                continue
            tx = mx
            ty = my
            ci = (my & 15) * CM + (mx & 15)
            c00 = cl[ci]
            c10 = cl[ci + 1]
            c01 = cl[ci + CM]
            c11 = cl[ci + CM + 1]
            # Licht am Standpunkt des Spielers (Eintritt in die erste Kachel)
            fx = px - tx
            fy = py - ty
            t0 = c00 + (c10 - c00) * fx
            l_in = t0 + (c01 + (c11 - c01) * fx - t0) * fy
            prev_d = 0.0
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
                own = cur[6]
                fk = flick.get(own, 1.0) if flick else 1.0
                lq = int(cur[5] * fk * 32.0)
                q = pv / d

                # Licht am Austrittspunkt (auf der gemeinsamen Kante)
                if side == 0:
                    fr = py + d * rdy - ty
                    if fr < 0.0:
                        fr = 0.0
                    elif fr > 1.0:
                        fr = 1.0
                    if stx > 0:
                        ea, eb = c10, c11
                    else:
                        ea, eb = c00, c01
                else:
                    fr = px + d * rdx - tx
                    if fr < 0.0:
                        fr = 0.0
                    elif fr > 1.0:
                        fr = 1.0
                    if sty > 0:
                        ea, eb = c01, c11
                    else:
                        ea, eb = c00, c10
                l_out = ea + (eb - ea) * fr
                if smooth:
                    la = l_in * fk
                    lb = l_out * fk
                    lq = int(la * 32.0)
                else:
                    la = lb = cur[5] * fk

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
                        plane_fill(col, s, bot, t, cur[2], la, lb, prev_d, d, rdx, rdy, False)
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
                        plane_fill(col, top, e, t, cur[3], la, lb, prev_d, d, rdx, rdy, True)
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
                    cl = corners_of[ck]
                nxt = cc[((my & 15) << 4) | (mx & 15)]
                if side == 0:
                    face = 0 if stx > 0 else 1
                else:
                    face = 2 if sty > 0 else 3
                bi = int(sqrt(d) * BSCALE)
                if bi >= NB:
                    bi = NB - 1

                if nxt is None:
                    # massive Wand: Licht entlang der Kante, auf gerade Wand normiert
                    wm = cur[4]
                    wl = l_out / WALL_AO if smooth else cur[5]
                    if wl > 1.6:
                        wl = 1.6
                    lqf = int(wl * fk * 32.0 * FACE[face])
                    code = lut(wm, lqf)[bi]
                    m = mats[wm]
                    if m.vseam and d < 16.0:
                        wu = (py + d * rdy) if side == 0 else (px + d * rdx)
                        u = (wu + OFF) / m.vseam
                        foot = d * colw / m.vseam
                        if foot < 0.45 and u - int(u) < 0.035 + foot:
                            code = lut(wm, int(lqf * m.seam_dark))[bi]
                    n = bot - top
                    fb[top * W + col:bot * W + col:W] = [code] * n
                    if n > 3:
                        if m.hseam and m.hseam * q >= 3.0:
                            dcode = lut(wm, int(lqf * m.seam_dark))[bi]
                            k = 1
                            z = fl + m.hseam
                            while z < ce:
                                y = int(hz + 0.5 - (z - eye) * q)
                                if top <= y < bot:
                                    fb[y * W + col] = dcode
                                k += 1
                                z = fl + k * m.hseam
                        # Kontaktschatten: Wandfuss und Deckenkante etwas dunkler
                        if smooth and n > 6:
                            ya = int(hz + 0.5 - (fl + 0.4 - eye) * q)
                            if ya < top:
                                ya = top
                            if bot > ya:
                                sc = lut(wm, int(lqf * 0.8))[bi]
                                fb[ya * W + col:bot * W + col:W] = [sc] * (bot - ya)
                            yb = int(hz + 0.5 - (ce - 0.25 - eye) * q)
                            if yb > bot:
                                yb = bot
                            if yb > top:
                                sc = lut(wm, int(lqf * 0.88))[bi]
                                fb[top * W + col:yb * W + col:W] = [sc] * (yb - top)
                        for z0, z1, bm in m.bands:
                            ya = int(hz + 0.5 - (fl + z1 - eye) * q)
                            yb = int(hz + 0.5 - (fl + z0 - eye) * q)
                            if ya < top:
                                ya = top
                            if yb > bot:
                                yb = bot
                            if yb > ya:
                                bl = lut(bm, lqf)
                                fb[ya * W + col:yb * W + col:W] = [bl[bi]] * (yb - ya)
                    break

                # oberer Absatz (Tuersturz / niedrigere Decke dahinter)
                nce = nxt[1]
                if nce < ce:
                    e = int(hz + 0.5 - (nce - eye) * q)
                    if e > top:
                        if e > bot:
                            e = bot
                        wl = l_out if smooth else cur[5]
                        lqf = int(wl * fk * 32.0 * FACE[face])
                        fb[top * W + col:e * W + col:W] = [lut(cur[4], lqf)[bi]] * (e - top)
                        top = e
                # unterer Absatz (Stufe, Kiste, Podest)
                nfl = nxt[0]
                if nfl > fl:
                    s = int(hz + 0.5 + (eye - nfl) * q)
                    if s < top:
                        s = top
                    if s < bot:
                        nfk = flick.get(nxt[6], 1.0) if flick else 1.0
                        wl = l_out if smooth else nxt[5]
                        lqf = int(wl * nfk * 32.0 * FACE[face])
                        fb[s * W + col:bot * W + col:W] = [lut(nxt[4], lqf)[bi]] * (bot - s)
                        bot = s
                if top >= bot:
                    break
                if d > maxd:
                    fb[top * W + col:bot * W + col:W] = [fogc] * (bot - top)
                    break
                # weiter in die naechste Kachel
                cur = nxt
                tx = mx
                ty = my
                ci = (my & 15) * CM + (mx & 15)
                c00 = cl[ci]
                c10 = cl[ci + 1]
                c01 = cl[ci + CM]
                c11 = cl[ci + CM + 1]
                l_in = l_out
                prev_d = d

        self.steps = steps
