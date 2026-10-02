"""Echte Kameraperspektive (V3.1).

Ein Spalten-Raycaster kann eine nach oben/unten geneigte Kamera nicht exakt
darstellen: Jede Bildspalte ist bei ihm eine senkrechte Ebene in der Welt.
Bisher wurde der Blick nach oben deshalb nur angenaehert - Hoehen stimmten,
Breiten aber nicht: Saeulen und Waende wirkten beim Blick nach oben/unten
in die Breite gezogen (bei 50 Grad fast doppelt so breit).

Loesung in zwei Schritten:
  1. Der Raycaster rendert ein Panorama: Spalte = gleichmaessiger Drehwinkel,
     Zeile = Hoehenwinkel, Entfernungen entlang des Strahls. Dieses Bild ist
     unabhaengig vom Blickwinkel nach oben/unten.
  2. Fuer jeden Bildpunkt der echten, geneigten Kamera wird berechnet, in
     welche Richtung er schaut (inkl. Neigung) - und der passende Punkt im
     Panorama uebernommen. Die Zuordnung wird je Neigung einmal berechnet und
     gecacht; pro Frame ist das nur ein Tabellen-Lookup je Pixel.

Ergebnis: eine physikalisch korrekte Lochkamera. Senkrechte Kanten laufen beim
Blick nach oben zusammen wie bei einem Foto, Proportionen stimmen aus jedem
Blickwinkel.

Weites Sichtfeld: Eine Lochkamera streckt Dinge am linken/rechten Rand stark
(bei 105 Grad fast dreifach). Optional wird deshalb die Panini-Projektion
eingemischt (Parameter d: 0 = Lochkamera, 1 = Panini). Sie haelt senkrechte
Linien gerade und Objekte am Rand in ihrer Form.
"""

import math

OVERSAMPLE = 1.08      # Panorama minimal feiner als das Ausgabebild (sauberere Raender)
PITCH_STEP = 0.006     # Quantisierung der Neigung fuer den Cache (rad, < 1 Bildzeile)


def panini_d(fov, mode):
    """Anteil Panini-Projektion je nach Einstellung und Sichtfeld."""
    if mode == "rect":
        return 0.0
    if mode == "panini":
        return 1.0
    return max(0.0, min(1.0, (math.degrees(fov) - 90.0) / 25.0))   # auto: ab 90 Grad weich zumischen


class Projector:
    def __init__(self, raycaster):
        self.rc = raycaster
        self._dirs = {}
        self._maps = {}
        self.src = []
        self.zb = []
        self.src_size = (0, 0)

    # --- Kamera: Bildpunkt -> Richtung (ohne Neigung) ---------------------------------
    def _camera_dirs(self, W, H, aspect, fov, d):
        key = (W, H, round(aspect, 4), round(fov, 5), round(d, 3))
        r = self._dirs.get(key)
        if r is not None:
            return r
        half = fov * 0.5
        if d <= 1e-6:
            x_edge = math.tan(half)
        else:
            x_edge = (d + 1.0) * math.sin(half) / (d + math.cos(half))
        f = (W * 0.5) / x_edge            # Pixel pro Einheit Bildkoordinate (= pro Radiant in der Mitte)
        wh = (W + 1) // 2                 # linke Haelfte, rechts wird gespiegelt
        rows = []
        sqrt, acos, sin = math.sqrt, math.acos, math.sin
        for y in range(H):
            ys = (H * 0.5 - y - 0.5) * aspect / f
            row = []
            for x in range(wh):
                xs = (x + 0.5 - W * 0.5) / f
                if d <= 1e-6:
                    X, U, Z = xs, ys, 1.0
                else:
                    # Umkehrung der Panini-Projektion
                    k = xs * xs / ((d + 1.0) * (d + 1.0))
                    disc = k * k * d * d - (k + 1.0) * (k * d * d - 1.0)
                    cphi = (-k * d + sqrt(disc if disc > 0.0 else 0.0)) / (k + 1.0)
                    cphi = -1.0 if cphi < -1.0 else (1.0 if cphi > 1.0 else cphi)
                    phi = acos(cphi)
                    if xs < 0.0:
                        phi = -phi
                    X, U, Z = sin(phi), ys * (d + cphi) / (d + 1.0), cphi
                n = 1.0 / sqrt(X * X + U * U + Z * Z)
                row.append((X * n, U * n, Z * n))
            rows.append(row)
        if len(self._dirs) > 6:
            self._dirs.clear()
        r = (rows, f)
        self._dirs[key] = r
        return r

    # --- Zuordnung Ausgabepixel -> Panoramapixel (je Neigung) ------------------------------
    def _map(self, W, H, aspect, fov, d, pitch, hscale):
        pq = int(round(pitch / PITCH_STEP))
        key = (W, H, round(aspect, 4), round(fov, 5), round(d, 3), pq, hscale)
        m = self._maps.get(key)
        if m is not None:
            return m
        rows, f = self._camera_dirs(W, H, aspect, fov, d)
        p = pq * PITCH_STEP
        cp, sp = math.cos(p), math.sin(p)
        atan2, asin = math.atan2, math.asin
        yaw_rows = []
        el_rows = []
        ymax = 0.0
        emin, emax = 9.0, -9.0
        for row in rows:
            yr = []
            er = []
            for X, U, Z in row:
                u2 = U * cp + Z * sp
                z2 = Z * cp - U * sp
                yw = atan2(X, z2)
                el = asin(u2 if -1.0 <= u2 <= 1.0 else (1.0 if u2 > 0 else -1.0))
                yr.append(yw)
                er.append(el)
                if -yw > ymax:
                    ymax = -yw
                if el > emax:
                    emax = el
                if el < emin:
                    emin = el
            yaw_rows.append(yr)
            el_rows.append(er)
        ymax += 0.01
        emax += 0.01
        emin -= 0.01
        Ksh = f * OVERSAMPLE / hscale
        Ksv = f / aspect * OVERSAMPLE
        Ws = int(2.0 * ymax * Ksh) + 2
        Hs = int((emax - emin) * Ksv) + 2
        mapping = [0] * (W * H)
        wmax = Ws - 1
        hmax = Hs - 1
        for y in range(H):
            base = y * W
            yr = yaw_rows[y]
            er = el_rows[y]
            for x in range(len(yr)):
                sy = int((emax - er[x]) * Ksv)
                sy = hmax if sy > hmax else (0 if sy < 0 else sy)
                rowbase = sy * Ws
                sx = int((yr[x] + ymax) * Ksh)
                mapping[base + x] = rowbase + (wmax if sx > wmax else (0 if sx < 0 else sx))
                sx = int((ymax - yr[x]) * Ksh)
                mapping[base + W - 1 - x] = rowbase + (wmax if sx > wmax else (0 if sx < 0 else sx))
        m = (mapping, Ws, Hs, ymax, emax, Ksh, Ksv)
        if len(self._maps) > 48:
            self._maps.pop(next(iter(self._maps)))
        self._maps[key] = m
        return m

    # --- Rendern ---------------------------------------------------------------------------
    def render(self, cam, W, H, aspect, hscale, sprites, d):
        mapping, Ws, Hs, ymax, emax, Ksh, Ksv = self._map(W, H, aspect, cam.fov, d, cam.pitch, hscale)
        n = Ws * Hs
        if len(self.src) != n:
            self.src = [0] * n
            self.zb = [1e9] * n
        self.src_size = (Ws, Hs)
        self.rc.render(self.src, Ws, Hs, cam, 1.0, self.zb if sprites else None, pano=(ymax, Ksh, Ksv, emax))
        if sprites:
            self.rc.render_sprites(self.src, self.zb, Ws, Hs, sprites)
        src = self.src
        return [src[i] for i in mapping]
