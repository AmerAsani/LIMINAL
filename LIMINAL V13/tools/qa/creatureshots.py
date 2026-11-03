"""V13.3: Wesen einer Ebene in ihren Raeumen aufnehmen (Abnahmeschranke d: Kritikrunden "mit allen Wesen").

Je Wesen ein Lauf des Spiels: das Wesen erscheint dort, wo die Ebene es hinstellt (presence/near, rand, pack, Spuren aus
dem Ausstattungs-Abzug), die Spielerkamera steht davor - erst im Licht der Ebene, dann mit Lampe, dazu nah/fern oder ein
zweiter Zustand. Kein Studio: Licht, Nebel und Raum sind die der Ebene.

    python tools/qa/creatureshots.py level2 --round r4 [--only photoshop,jerry] [--seed 3]
Ausgabe: Einzelbilder build/qa/creatures/<level>/<runde>/<wesen>_<n>.png, Boegen docs/screens/<level>/wesen_<runde>_<seite>.jpg
(3 x 4 Bilder je Seite, beschriftet) und die Liste der Aufnahmen wesen_<runde>.json.
"""
import argparse
import json
import math
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from game import ROOT, run  # noqa: E402
from maplib import Grid  # noqa: E402

W0 = 'wait 40; grace 9999; '  # Regie ruhig stellen: nur das erzwungene Wesen


def _lamp(name, k):
    # die Lampe braucht gut eine Sekunde, bis sie voll leuchtet (Anlauf); danach wieder aus
    return f'key T; wait 90; shot {{out}}/{name}_{k}.png; key T; wait 40; '


# Je Wesen: (Beschriftung, Skript). {out} = Bildordner. Jede Aufnahme: <wesen>_<n>.png
LEVEL2 = {
    'photoshop': ('Photoshop (Entity 104)',
        W0 + 'rand photoshop 4.2; wait 50; shot {out}/photoshop_1.png; ' + _lamp('photoshop', 2) +
        'rand photoshop 3.1 0.55; wait 30; shot {out}/photoshop_3.png; key T; wait 90; shot {out}/photoshop_4.png; key T; wait 30; '
        'rand photoshop 2.4; randstate ps_box 1; wait 40; shot {out}/photoshop_5.png; randstate ps_box 0; quit'),
    'cuisine': ('Cannibal Cuisine (Entity 36)',
        W0 + 'rand cuisine 3.6; wait 40; shot {out}/cuisine_1.png; ' + _lamp('cuisine', 2) +
        'rand cuisine 2.2 0.5; wait 20; shot {out}/cuisine_3.png; randstate cc_arm 1; wait 50; shot {out}/cuisine_4.png; quit'),
    'swindle': ('Swindlebird (Entity 192)',
        W0 + 'rand swindle 9; wait 30; shot {out}/swindle_1.png; rand swindle 5; wait 10; shot {out}/swindle_2.png; quit'),
    'macchina': ('Macchina-Reste (Entity 189)',
        W0 + 'pos 130.4 105.5 3.1416 -0.25; wait 60; shot {out}/macchina_1.png; ' + _lamp('macchina', 2) +
        'pos 175.4 117.5 3.1416 -0.25; wait 60; shot {out}/macchina_3.png; ' + _lamp('macchina', 4) + 'quit'),
    # Jerry und das Fenster entstehen nur in geladenen Gegenden: erst hingehen (Kammer des Vogels; 20 m vor einem Gangende,
    # Blick weg vom Ende - das Fenster bildet sich nur ungesehen)
    'jerry': ('Jerry (Entity 7)',
        W0 + 'pos 178.2 28 0; wait 60; presence jerry; wait 60; near jerry 3.4; wait 30; shot {out}/jerry_1.png; ' +
        _lamp('jerry', 2) + 'near jerry 2.2 0.6; wait 20; shot {out}/jerry_3.png; quit'),
    'window': ('Fensterwesen (Entity 2)',
        W0 + 'pos 44 8 3.1416; wait 60; presence window; wait 240; near window 7; wait 30; shot {out}/window_1.png; ' +
        _lamp('window', 2) + 'near window 3.2; wait 60; shot {out}/window_3.png; quit'),
    'moth': ('Deathmoths (Entity 4)',
        W0 + 'presence moth; wait 120; near moth 3.2; wait 30; shot {out}/moth_1.png; ' + _lamp('moth', 2) +
        'key T; wait 200; shot {out}/moth_3.png; quit'),
    'rat': ('Death Rats (Entity 24)',
        W0 + 'presence rat; wait 150; near rat 2.8; wait 10; shot {out}/rat_1.png; ' + _lamp('rat', 2) + 'quit'),
    'pack': ('Hound-Rudel (Entity 8)',
        W0 + 'pack next 99999; pack spawn 14; wait 90; near hound 9; wait 20; shot {out}/pack_1.png; key T; wait 90; '
        'shot {out}/pack_2.png; wait 110; shot {out}/pack_3.png; quit'),
    # Smiler nur im Stromausfall: erst faellt das Licht, 7-10 s spaeter ist er da
    'smiler': ('Smiler (Entity 3)',
        W0 + 'presence smiler 7; wait 760; near smiler 6; wait 30; shot {out}/smiler_1.png; ' + _lamp('smiler', 2) + 'quit'),
    'clump': ('Clump (Entity 5)',
        W0 + 'presence clump 8; wait 150; near clump 4.5 3.1416; wait 20; shot {out}/clump_1.png; ' + _lamp('clump', 2) +
        'figclear; pos 44 8 3.1416; wait 30; figure clump 3.2; wait 60; shot {out}/clump_3.png; ' + _lamp('clump', 4) + 'quit'),
    'nguithrxurh': ("Nguithr'xurh (Entity 16)",
        W0 + 'presence nguithrxurh; wait 150; near nguithrxurh 3.2; wait 20; shot {out}/nguithrxurh_1.png; ' +
        _lamp('nguithrxurh', 2) + 'quit'),
    'growler': ('Growler (Entity 130)',
        W0 + 'presence growler; wait 150; near growler 3.5; wait 20; shot {out}/growler_1.png; ' + _lamp('growler', 2) + 'quit'),
    # der Wrangler steckt bis zum Ausbruch im Boden: Vorfuehrung im Gang der Ebene (Licht der Ebene, Lampe)
    'wrangler': ('Wrangler (Entity 75)',
        W0 + 'pos 44 8 3.1416; wait 30; figure wrangler 3.6; wait 60; shot {out}/wrangler_1.png; ' + _lamp('wrangler', 2) + 'quit'),
    # die Gruppe geht auf den Spieler zu (die Kamera stak im Koerper): Vorfuehrung im Gang, dazu die Gruppe im Gang von weitem
    'faceling': ('Faceling (Entity 9)',
        W0 + 'pos 44 8 3.1416; wait 30; figure faceling 3.4; wait 60; shot {out}/faceling_1.png; ' + _lamp('faceling', 2) +
        'figclear; presence faceling 14; wait 60; near faceling 12; wait 1; shot {out}/faceling_3.png; quit'),
    'wretch': ('Wretch (Entity 15)',
        W0 + 'presence wretch 9; wait 150; near wretch 4; wait 20; shot {out}/wretch_1.png; ' + _lamp('wretch', 2) + 'quit'),
}


def _purple(name, px, py, tx, ty, tz, lamp=False):
    """V13.3 Level 3: Spielerkamera vor eine purpifizierte Stelle der Karte (KREATUR_L3_Purpur.md)."""
    a = math.atan2(ty - py, tx - px)
    p = math.atan2(tz - 1.62, math.hypot(tx - px, ty - py))
    return f'pos {px} {py} {a:.4f} {p:.4f}; ' + ('key T; ' if lamp else '') + f'wait 90; shot {{out}}/{name}.png; ' + \
        ('key T; wait 30; ' if lamp else '')


def _knight():
    # V13.3 Level 3: erzwungener Lauf des Red Knight; er kommt ausser Sicht, rennt vorbei, haut in die Luft und ist fort.
    # Die Spielerkamera dreht sich jedes Bild zu ihm (face wretch); die zweite Haelfte mit Lampe.
    s = W0 + 'knight run 1; wait 150; '
    for k in range(1, 11):
        if k == 6:
            s += 'key T; '
        s += f'face wretch; wait 2; shot {{out}}/knight_{k}.png; wait 28; '
    return s + 'quit'


# V13.3 Level 3 (Abnahme): alle Wesen der Ebene in ihren Raeumen. Dritter Eintrag: eigene Startargumente (Saat), wo die
# Kreaturen-Runden einen Ort mit passender Decke/Nische gefunden hatten (KREATUR_Clump.md, KREATUR_Hound.md).
LEVEL3 = {
    'bigclump': ('Grosses Buendel (Entity 5, Level 3)',
        W0 + 'bigclump next 99999; bigclump spawn 16; wait 90; near clump 6 90; wait 20; shot {out}/bigclump_1.png; ' +
        _lamp('bigclump', 2) + 'bigclump state 3; wait 70; shot {out}/bigclump_3.png; key T; wait 90; '
        'shot {out}/bigclump_4.png; bigclump state 5; wait 45; shot {out}/bigclump_5.png; key T; wait 30; '
        'figclear; bigclump next 99999; figure clump 2.6; figstate variant 1; figstate hang 0; wait 90; key T; wait 90; '
        'figcam 60 10 2.8 1.0; wait 5; shot {out}/bigclump_6.png; figcam 130 8 1.5 1.1; wait 5; shot {out}/bigclump_7.png; quit',
        '--level level3 --seed 9'),
    # das kleine Buendel braucht ein Lager (Spawnpunkt der Karte mit Deckenluefter) in der geladenen Gegend: erst hingehen
    'clump': ('Clump, klein (Entity 5)',
        W0 + 'pos 73.6 51.4 -1.5708; wait 60; presence clump 8; wait 150; pos 73.6 45.4 -1.5708 0.42; wait 20; '
        'shot {out}/clump_1.png; ' + _lamp('clump', 2) + 'figclear; figure clump 3.0; figstate variant 0; wait 60; key T; wait 90; '
        'figcam 30 12 2.6 0.5; wait 5; shot {out}/clump_3.png; quit'),
    'pack': ('Hound-Rudel (Entity 8, Level 3)',
        W0 + 'pack next 99999; pack spawn 22; wait 60; near hound 6; wait 20; shot {out}/pack_1.png; key T; wait 90; '
        'shot {out}/pack_2.png; wait 150; shot {out}/pack_3.png; near hound 2.6; wait 2; shot {out}/pack_4.png; quit',
        '--level level3 --seed 7'),
    # die Gruppe geht los, sobald sie einen bemerkt: im Gang aus 5 m, dann die Vorfuehrung des Entwickelten nah mit Lampe
    'faceling': ('Faceling bewaffnet (Entity 9)',
        W0 + 'presence faceling 12; wait 150; near faceling 5; wait 1; shot {out}/faceling_1.png; key T; wait 90; '
        'near faceling 3.2; wait 1; shot {out}/faceling_2.png; figclear; figure faceling 2.2; figstate variant 1; figstate look 1; '
        'wait 60; figcam 10 4 1.2 1.55; wait 5; shot {out}/faceling_3.png; figcam -40 4 2.6 1.0; wait 5; shot {out}/faceling_4.png; quit'),
    'skinstealer': ('Skin-Stealer im Mantel (Entity 10)',
        W0 + 'presence skinstealer 6; wait 240; near skinstealer 3.2; wait 2; shot {out}/skinstealer_1.png; ' +
        _lamp('skinstealer', 2) + 'near skinstealer 1.8; wait 2; shot {out}/skinstealer_3.png; quit'),
    'wretch': ('Wretch bewaffnet (Entity 15)',
        W0 + 'presence wretch 9; wait 150; near wretch 4; wait 20; shot {out}/wretch_1.png; ' + _lamp('wretch', 2) +
        'near wretch 2.0; key T; wait 90; shot {out}/wretch_3.png; quit'),
    'rat': ('Death Rats und Falle (Entity 24)',
        W0 + 'presence rat; wait 90; near rat 6; key T; wait 90; shot {out}/rat_1.png; key T; wait 30; near rat 3; wait 20; '
        'shot {out}/rat_2.png; l3 trap 2.5; wait 10; l3near 2.6; wait 40; shot {out}/rat_3.png; key T; wait 90; '
        'shot {out}/rat_4.png; l3near 1.6; wait 10; shot {out}/rat_5.png; l3near 4.0; wait 10; shot {out}/rat_6.png; quit'),
    'moth': ('Deathmoth-Schwarm (Entity 4)',
        W0 + 'presence moth; wait 90; near moth 5; wait 30; shot {out}/moth_1.png; ' + _lamp('moth', 2) +
        'key T; l3 swarm 1; wait 80; shot {out}/moth_3.png; wait 200; shot {out}/moth_4.png; wait 40; shot {out}/moth_5.png; quit'),
    'prae': ('Praetorianer (Entity 4)',
        W0 + 'presence moth; wait 90; l3prae 4.5; wait 30; shot {out}/prae_1.png; key T; wait 90; shot {out}/prae_2.png; '
        'l3prae 2.2; l3 pstate 3; wait 30; shot {out}/prae_3.png; quit'),
    # Smiler nur im Stromausfall und nur in Bereich 1 (am Start: Bereich 0) - im Hauptgang die Welle erzwingen, dann er
    'smiler': ('Smiler (Entity 3, Level 3)',
        W0 + 'pos 30 73 0; wait 60; presence encounter; wait 700; face smiler; wait 2; shot {out}/smiler_1.png; ' +
        _lamp('smiler', 2) + 'near smiler 5; wait 2; shot {out}/smiler_3.png; quit'),
    'knight': ('Red Knight (Entity 71)', _knight()),
    'purple': ('Purpurne Wesen (Purpification)',
        W0 + 'wait 200; ' + _purple('purple_1', 70.0, 100.55, 72.5, 99.2, 1.3) +
        _purple('purple_2', 70.0, 100.55, 72.5, 99.2, 1.3, lamp=True) +
        _purple('purple_3', 30.55, 52.4, 31.7, 54.5, 0.55) + _purple('purple_4', 30.55, 52.4, 31.7, 54.5, 0.55, lamp=True) +
        _purple('purple_5', 30.25, 54.6, 31.95, 56.3, 1.45) + 'quit'),
}
# Spuren (Ausstattung der Karte): Kamera vor das Modell aus dem Ausstattungs-Abzug
TRACES = {'level2': [('skin_hung', 0, 2.3), ('skin_lair', 0, 2.6), ('skin_shed', 0, 2.2), ('skin_shed', 1, 2.2),
                     ('skin_scratches', 0, 1.8)]}
JOBS = {'level2': LEVEL2, 'level3': LEVEL3}


def trace_job(level):
    dump = os.path.join(ROOT, 'build', 'qa', f'props_{level}.json')
    if not os.path.exists(dump):
        return None
    props = json.load(open(dump, encoding='utf-8'))['props']
    g = Grid(level)
    parts = []
    k = 0
    for name, idx, dist in TRACES.get(level, []):
        hits = [p for p in props if p['name'] == name]
        if idx >= len(hits):
            continue
        p = hits[idx]
        x, y, z = p['p']
        best = None
        for da in (0, 0.4, -0.4, 0.8, -0.8, 1.2, -1.2, 1.6, -1.6):
            a = p['yaw'] + da
            for d in (dist, dist - 0.4, dist - 0.8, dist + 0.5):
                cx, cy = x + math.cos(a) * d, y + math.sin(a) * d
                if g.ok(cx, cy):
                    best = (cx, cy)
                    break
            if best:
                break
        if not best:
            continue
        cx, cy = best
        tz = z + max(0.15, min(1.3, (p['hi'][2] + p['lo'][2]) * 0.5))
        # Spielerkamera (Augenhoehe 1,62 m) mit Blick auf die Mitte des Modells; die Lampe ist die des Spielers
        yaw = math.atan2(y - cy, x - cx)
        pitch = math.atan2(tz - (z + 1.62), math.hypot(x - cx, y - cy))
        k += 1
        parts.append(f'pos {cx:.2f} {cy:.2f} {yaw:.3f} {pitch:.3f}; wait 50; shot {{out}}/traces_{k}.png; ')
        k += 1
        parts.append(f'key T; wait 90; shot {{out}}/traces_{k}.png; key T; wait 30; ')
    if not parts:
        return None
    return ('Skin-Stealer-Spuren (Entity 10)', W0 + ''.join(parts) + 'quit')


def sheets(level, rnd, shots, labels):
    d = os.path.join(ROOT, 'docs', 'screens', level)
    tw, th, cols, rows = 640, 360, 3, 4
    per = cols * rows
    out = []
    for s in range(0, len(shots), per):
        page = shots[s:s + per]
        S = Image.new('RGB', (cols * tw, ((len(page) + cols - 1) // cols) * th), (16, 16, 16))
        dr = ImageDraw.Draw(S)
        for i, (job, f) in enumerate(page):
            x, y = (i % cols) * tw, (i // cols) * th
            S.paste(Image.open(f).convert('RGB').resize((tw, th), Image.LANCZOS), (x, y))
            t = f'{labels[job]} - {os.path.basename(f)[:-4]}'
            dr.rectangle((x, y, x + 8 + 7 * len(t), y + 16), fill=(0, 0, 0))
            dr.text((x + 4, y + 2), t, fill=(255, 230, 120))
        p = os.path.join(d, f'wesen_{rnd}_{s // per + 1}.jpg')
        S.save(p, quality=82)
        out.append(p)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('level')
    ap.add_argument('--round', default='r1')
    ap.add_argument('--seed', type=int, default=3)
    ap.add_argument('--only', default='')
    ap.add_argument('--size', default='1280x720')
    ap.add_argument('--sheet-only', action='store_true', help='nur die Boegen aus vorhandenen Bildern')
    a = ap.parse_args()
    jobs = dict(JOBS.get(a.level, {}))
    tj = trace_job(a.level)
    if tj:
        jobs['traces'] = tj
    if a.only:
        jobs = {k: v for k, v in jobs.items() if k in a.only.split(',')}
    raw = os.path.join(ROOT, 'build', 'qa', 'creatures', a.level, a.round)
    os.makedirs(raw, exist_ok=True)
    report = {}
    for job, (label, script, *args) in jobs.items():
        if not a.sheet_only:
            for f in os.listdir(raw):
                if f.startswith(job + '_'):
                    os.remove(os.path.join(raw, f))
            code, log = run(args[0] if args else f'--level {a.level} --seed {a.seed}', script.format(out=raw.replace('\\', '/')),
                            userdir=os.path.join(ROOT, 'build', 'qa', 'ud_creatures', job), size=a.size)
            warn = [ln.strip() for ln in log.splitlines() if 'Skript' in ln and ('kein' in ln or 'unbekannt' in ln or 'nicht' in ln)]
            report[job] = {'exit': code, 'warn': warn[:6]}
            print(f'{job}: exit {code}', warn[:3], flush=True)
    # alle vorhandenen Bilder dieser Runde (auch aus frueheren Teil-Laeufen mit --only), Reihenfolge der Tabelle
    labels = {k: v[0] for k, v in dict(JOBS.get(a.level, {}), traces=('Skin-Stealer-Spuren (Entity 10)', '')).items()}
    shots = []
    for job in labels:
        fs = sorted((f for f in os.listdir(raw) if f.startswith(job + '_') and f.endswith('.png')),
                    key=lambda f: int(f[len(job) + 1:-4]) if f[len(job) + 1:-4].isdigit() else 0)
        shots += [(job, os.path.join(raw, f)) for f in fs]
    pages = sheets(a.level, a.round, shots, labels)
    meta = os.path.join(ROOT, 'docs', 'screens', a.level, f'wesen_{a.round}.json')
    old = json.load(open(meta, encoding='utf-8')) if os.path.exists(meta) else {}
    old.update(report)
    with open(meta, 'w', encoding='utf-8') as fh:
        json.dump({'shots': [os.path.basename(f) for _, f in shots], **{k: v for k, v in old.items() if k != 'shots'}}, fh,
                  indent=1, ensure_ascii=False)
    print('Boegen:', *pages, sep='\n  ')


if __name__ == '__main__':
    main()
