"""Shading und Terminalausgabe.

Pixelcode (int) im Framebuffer:  (farbe15 << 3) | zeichenindex
    farbe15      = quantisierte Endfarbe, je 5 Bit R/G/B (inkl. Licht und Nebel)
    zeichenindex = Dichtestufe 0..7 aus der Helligkeit (' ' ... '█')

Der Shader berechnet Farben nie pro Pixel. Stattdessen gibt es Lookup-Tabellen:
    lut(material, licht)  -> Code je Distanz-Bucket (sqrt-skaliert)
    plane(...)            -> Code je Bildschirmzeile fuer eine Boden-/Deckenebene
Damit reduziert sich das Fuellen einer Bodenspalte auf ein Slice-Assignment.
"""

import math

from .materials import PAT_GRID

NB = 170                # Anzahl Distanz-Buckets
BSCALE = 20.0            # bucket = sqrt(d) * BSCALE  -> max ~72 m
TEXD = 11.0              # bis zu dieser Distanz werden Boden-/Deckenmuster gezeichnet

RAMP = (" ", "·", ".", ":", "░", "▒", "▓", "█")
_THRESH = (0.035, 0.07, 0.115, 0.17, 0.25, 0.36, 0.5)
MONO_RAMP = (" ", ".", ":", "-", "=", "+", "*", "#", "%", "@")


def _code(r, g, b):
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    chi = 0
    for t in _THRESH:
        if lum >= t:
            chi += 1
        else:
            break
    ri = int(r * 31.0 + 0.5)
    gi = int(g * 31.0 + 0.5)
    bi = int(b * 31.0 + 0.5)
    ri = 31 if ri > 31 else (0 if ri < 0 else ri)
    gi = 31 if gi > 31 else (0 if gi < 0 else gi)
    bi = 31 if bi > 31 else (0 if bi < 0 else bi)
    return (((ri << 10) | (gi << 5) | bi) << 3) | chi


class Shader:
    """Farbtabellen fuer Material x Licht x Distanz, inklusive Nebel."""

    def __init__(self, bank):
        self.bank = bank
        self.fog = (0.1, 0.1, 0.1)
        self.density = 0.05
        self.max_dist = 48.0
        self.exposure = 1.0
        self.luts = {}
        self.planes = {}
        self.rowcache = {}
        self.fog_code = _code(*self.fog)

    def set_fog(self, rgb, density):
        """Setzt Nebelfarbe/-dichte (quantisiert, damit Caches selten verfallen)."""
        q = (round(rgb[0] * 40) / 40, round(rgb[1] * 40) / 40, round(rgb[2] * 40) / 40)
        dq = round(density * 400) / 400
        if q == self.fog and dq == self.density:
            return
        self.fog = q
        self.density = max(0.004, dq)
        self.max_dist = min(62.0, 3.6 / self.density)
        self.fog_code = _code(*self.fog)
        self.luts.clear()
        self.planes.clear()

    def lut(self, mat, lq):
        key = (mat, lq)
        t = self.luts.get(key)
        if t is not None:
            return t
        m = self.bank[mat]
        r, g, b = m.rgb
        # Tonkurve: dunkle Bereiche bleiben dunkel, aber lesbar (wie ein an die
        # Dunkelheit gewoehntes Auge); helle Bereiche saettigen weich.
        inten = 1.3 if m.emissive else ((lq / 32.0) * self.exposure) ** 0.62
        fr, fg, fb = self.fog
        dens = self.density
        emissive = m.emissive
        t = []
        for i in range(NB):
            d = ((i + 0.5) / BSCALE) ** 2
            # Lichtabfall mit der Entfernung (Blickrichtung) plus exponentieller Nebel
            a = inten if emissive else inten / (1.0 + d * d * 0.0011)
            f = 1.0 - math.exp(-d * dens)
            k = a * (1.0 - f)
            t.append(_code(r * k + fr * f, g * k + fg * f, b * k + fb * f))
        self.luts[key] = t
        if len(self.luts) > 6000:
            self.luts.clear()
            self.luts[key] = t
        return t

    def rows(self, dzq, ceiling, hz, H, pv, colw):
        """Distanz, Bucket und Pixel-Fussabdruck je Bildschirmzeile fuer eine Ebene."""
        key = (dzq, ceiling, int(hz * 8), H, int(pv * 16), int(colw * 100000))
        r = self.rowcache.get(key)
        if r is not None:
            return r
        dz = max(0.02, dzq / 50.0)
        dist = [1e9] * H
        buck = [NB - 1] * H
        foot = [9.0] * H
        for y in range(H):
            den = (hz - (y + 0.5)) if ceiling else ((y + 0.5) - hz)
            if den > 0.01:
                d = dz * pv / den
                dist[y] = d
                bi = int(math.sqrt(d) * BSCALE)
                buck[y] = bi if bi < NB else NB - 1
                # Weltgroesse, die ein Pixel abdeckt (horizontal bzw. perspektivisch verkuerzt)
                foot[y] = max(d * colw, d * d / (dz * pv))
        r = (dist, buck, foot)
        if len(self.rowcache) > 3000:
            self.rowcache.clear()
        self.rowcache[key] = r
        return r

    def plane(self, dz, mat, lq, ceiling, hz, H, pv, colw):
        """Zeilentabelle einer Boden-/Deckenebene (gecacht).

        Musterlinien werden mit dem Pixel-Fussabdruck verbreitert (billiges
        Anti-Aliasing). Wo ein Pixel groesser als eine halbe Rasterzelle ist,
        faellt das Muster weg - so flimmert in der Ferne nichts.
        """
        dzq = int(dz * 50.0 + 0.5)
        key = (dzq, mat, lq, ceiling, int(hz * 8), H)
        t = self.planes.get(key)
        if t is not None:
            return t
        dist, buck, foot = self.rows(dzq, ceiling, hz, H, pv, colw)
        lut = self.lut(mat, lq)
        codes = [lut[b] for b in buck]
        m = self.bank[mat]
        inv = 1.0 / m.scale
        lw = [m.line + f * inv * 0.5 for f in foot]
        tex = H if not ceiling else 0
        if m.kind:
            dark = self.lut(mat, int(lq * m.seam_dark)) if not m.emissive else lut
            alt = [dark[b] for b in buck]
            lim = 0.42 if m.kind == PAT_GRID else 0.9
            for y in range(H):
                if dist[y] < TEXD and lw[y] < lim:
                    if ceiling:
                        tex = y + 1
                    elif tex == H:
                        tex = y
        else:
            alt = codes
        t = (codes, alt, dist, m.kind, inv, lw, tex, buck)
        if len(self.planes) > 5000:
            self.planes.clear()
        self.planes[key] = t
        return t


class Renderer:
    """Wandelt den Framebuffer in einen einzigen Terminal-String um."""

    MODES = ("ascii", "hires", "mono")

    def __init__(self, mode="hires", colors="truecolor"):
        self.mode = mode
        self.colors = colors
        self._esc = {}
        self._fg = {}
        self._bg = {}
        self._dim = {}
        self._prev_rows = None     # V2: zuletzt gesendete Zeilen (Delta-Ausgabe)
        self.rows_sent = 0

    def pixel_rows(self, rows):
        return rows * 2 if self.mode == "hires" else rows

    def pixel_aspect(self):
        """Hoehe eines Pixels relativ zur Breite (Terminalzellen sind ~2:1)."""
        return 1.0 if self.mode == "hires" else 2.0

    def cycle_mode(self):
        i = self.MODES.index(self.mode)
        self.mode = self.MODES[(i + 1) % len(self.MODES)]

    # --- Farb-Escapes --------------------------------------------------------------
    @staticmethod
    def _rgb(ck):
        return (((ck >> 10) & 31) * 255 // 31, ((ck >> 5) & 31) * 255 // 31, (ck & 31) * 255 // 31)

    @staticmethod
    def _x256(r, g, b):
        def c(v):
            return 0 if v < 48 else (1 if v < 115 else (v - 35) // 40)
        return 16 + 36 * c(r) + 6 * c(g) + c(b)

    def _make_esc(self, ck):
        r, g, b = self._rgb(ck)
        fr, fg, fb = min(255, r * 6 // 5 + 4), min(255, g * 6 // 5 + 4), min(255, b * 6 // 5 + 4)
        br, bg, bb = r * 2 // 5, g * 2 // 5, b * 2 // 5
        if self.colors == "256":
            e = "\x1b[38;5;%d;48;5;%dm" % (self._x256(fr, fg, fb), self._x256(br, bg, bb))
        else:
            e = "\x1b[38;2;%d;%d;%d;48;2;%d;%d;%dm" % (fr, fg, fb, br, bg, bb)
        self._esc[ck] = e
        return e

    def _make_fg(self, ck):
        r, g, b = self._rgb(ck)
        e = ("\x1b[38;5;%dm" % self._x256(r, g, b)) if self.colors == "256" else ("\x1b[38;2;%d;%d;%dm" % (r, g, b))
        self._fg[ck] = e
        return e

    def _make_bg(self, ck):
        r, g, b = self._rgb(ck)
        e = ("\x1b[48;5;%dm" % self._x256(r, g, b)) if self.colors == "256" else ("\x1b[48;2;%d;%d;%dm" % (r, g, b))
        self._bg[ck] = e
        return e

    def reset_cache(self):
        self._esc.clear()
        self._fg.clear()
        self._bg.clear()
        self.invalidate()

    def invalidate(self):
        """Erzwingt beim naechsten Frame eine komplette Neuausgabe."""
        self._prev_rows = None

    def dim(self, fb):
        """Abgedunkelte Kopie des Bildes (Hintergrund fuer das Menue)."""
        cache = self._dim
        out = []
        for code in fb:
            v = cache.get(code)
            if v is None:
                ck = code >> 3
                r = ((ck >> 10) & 31) * 3 // 10
                g = ((ck >> 5) & 31) * 3 // 10
                b = (ck & 31) * 3 // 10
                v = (((r << 10) | (g << 5) | b) << 3) | max(0, (code & 7) - 3)
                cache[code] = v
            out.append(v)
        return out

    # --- Zeilen -------------------------------------------------------------------------
    def _cells(self, fb, W, row, c0, c1, out):
        if self.mode == "ascii":
            esc = self._esc
            prev = -1
            base = row * W
            for i in range(base + c0, base + c1):
                code = fb[i]
                ck = code >> 3
                if ck != prev:
                    e = esc.get(ck)
                    out.append(e if e is not None else self._make_esc(ck))
                    prev = ck
                out.append(RAMP[code & 7])
        elif self.mode == "hires":
            fgc = self._fg
            bgc = self._bg
            pt = pb = -1
            top = 2 * row * W
            bot = top + W
            for c in range(c0, c1):
                ct = fb[top + c] >> 3
                cb = fb[bot + c] >> 3
                if ct != pt:
                    e = fgc.get(ct)
                    out.append(e if e is not None else self._make_fg(ct))
                    pt = ct
                if cb != pb:
                    e = bgc.get(cb)
                    out.append(e if e is not None else self._make_bg(cb))
                    pb = cb
                out.append("▀")
        else:
            base = row * W
            mr = MONO_RAMP
            for i in range(base + c0, base + c1):
                ck = fb[i] >> 3
                lum = (((ck >> 10) & 31) * 3 + ((ck >> 5) & 31) * 6 + (ck & 31)) / 310.0
                out.append(mr[min(9, int(lum * 13.0))])

    def present(self, fb, W, rows, overlays=None):
        """Baut den Frame. overlays: {zeile: [(spalte, text, sichtbare_laenge)]}.

        V2: Jede Bildschirmzeile wird zuerst als String erzeugt und nur dann
        gesendet, wenn sie sich seit dem letzten Frame veraendert hat. Das
        entlastet das Terminal deutlich (Menue, Stillstand, ruhige Bereiche).
        """
        prev = self._prev_rows
        if prev is None or len(prev) != rows:
            prev = [None] * rows
        out = []
        sent = 0
        for r in range(rows):
            parts = []
            segs = overlays.get(r) if overlays else None
            if not segs:
                self._cells(fb, W, r, 0, W, parts)
            else:
                c = 0
                for col, text, vis in sorted(segs):
                    if col > c:
                        self._cells(fb, W, r, c, min(col, W), parts)
                    parts.append("\x1b[0m")
                    parts.append(text)
                    c = max(c, col + vis)
                if c < W:
                    self._cells(fb, W, r, c, W, parts)
            line = "".join(parts)
            if line != prev[r]:
                prev[r] = line
                out.append("\x1b[%d;1H" % (r + 1))
                out.append(line)
                out.append("\x1b[0m")
                sent += 1
        self._prev_rows = prev
        self.rows_sent = sent
        return "".join(out)
