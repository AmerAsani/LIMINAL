"""Screenshot eines Frames als HTML-Datei (Farben + Zeichen, im Browser ansehbar)."""

import html

from .renderer import RAMP


def _rgb(ck):
    return (((ck >> 10) & 31) * 255 // 31, ((ck >> 5) & 31) * 255 // 31, (ck & 31) * 255 // 31)


def frame_to_html(fb, W, rows, mode, title="LIMINAL"):
    lines = []
    for r in range(rows):
        parts = []
        prev = None
        run = []
        for c in range(W):
            if mode == "hires":
                t = fb[2 * r * W + c] >> 3
                b = fb[(2 * r + 1) * W + c] >> 3
                style = "color:rgb(%d,%d,%d);background:rgb(%d,%d,%d)" % (_rgb(t) + _rgb(b))
                ch = "▀"
            else:
                code = fb[r * W + c]
                ck = code >> 3
                rr, gg, bb = _rgb(ck)
                fg = (min(255, rr * 6 // 5 + 4), min(255, gg * 6 // 5 + 4), min(255, bb * 6 // 5 + 4))
                # Volle Bloecke fuellen im Terminal die ganze Zelle - im Browser nachbilden
                bg = fg if (code & 7) == 7 else (rr * 2 // 5, gg * 2 // 5, bb * 2 // 5)
                style = "color:rgb(%d,%d,%d);background:rgb(%d,%d,%d)" % (fg + bg)
                ch = RAMP[code & 7]
            if style != prev and run:
                parts.append('<span style="%s">%s</span>' % (prev, html.escape("".join(run))))
                run = []
            prev = style
            run.append(ch)
        if run:
            parts.append('<span style="%s">%s</span>' % (prev, html.escape("".join(run))))
        lines.append("".join(parts))
    body = "\n".join(lines)
    return ("<!doctype html><html><head><meta charset='utf-8'><title>%s</title>"
            "<style>body{background:#000;margin:0;padding:12px}"
            "pre{font:14px/1.0 Consolas,'DejaVu Sans Mono',monospace;margin:0;letter-spacing:0}"
            "</style></head><body><pre>%s</pre></body></html>" % (html.escape(title), body))


def save_html(path, fb, W, rows, mode, title="LIMINAL"):
    with open(path, "w", encoding="utf-8") as f:
        f.write(frame_to_html(fb, W, rows, mode, title))
