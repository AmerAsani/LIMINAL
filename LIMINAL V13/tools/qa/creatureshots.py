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
def _view(name, x, y, yaw, pitch=0.0, lamp=False):
    """V13.3 Level 0: Spielerkamera an eine feste Stelle (Zonen, Halluzinationen), Licht der Ebene oder Lampe."""
    s = f'pos {x} {y} {yaw} {pitch}; wait 60; shot {{out}}/{name}.png; '
    return s + (f'key T; wait 90; shot {{out}}/{name}_lampe.png; key T; wait 30; ' if lamp else '')


# V13.3 Level 0 (Abnahme): die Seite nennt keine bestaetigten Wesen - im Spiel die Gestalt (dunkle Figur am Gangende), die
# Halluzinationen (Gestalt im Raum, Stromausfall des Raums; Schritte, Fluestern, Kratzen und Stimmen sind nur Klang) und
# die Fundstuecke in der Hand (Leuchtstab geknickt und geworfen, Beil geschwungen). Dazu die Zonen der Ebene (LOOK_0).
LEVEL0 = {
    'gestalt': ('Gestalt (dunkle Figur, Level 0)',
        W0 + 'presence gestalt 16; wait 30; face gestalt; wait 3; shot {out}/gestalt_1.png; key T; wait 90; '
        'shot {out}/gestalt_2.png; key T; wait 30; near gestalt 7; wait 40; shot {out}/gestalt_3.png; near gestalt 3; wait 40; '
        'shot {out}/gestalt_4.png; key T; wait 90; shot {out}/gestalt_5.png; quit'),
    # Halluzination 3: bei wenig Verstand faellt das Licht, und im Dunkeln bezieht eine Gestalt Stellung (ausser Sicht, am
    # Ende eines Gangs) - hinsehen, solange es dunkel ist, dann im zurueckgekehrten Licht und mit Lampe (Saeulenfeld)
    'ghost': ('Halluzination: Gestalt im Raum',
        W0 + 'pos 81.5 52 1.5708 0; wait 60; sanity 12; haunt 3; wait 50; face gestalt; wait 3; shot {out}/ghost_1.png; wait 200; '
        'face gestalt; wait 3; shot {out}/ghost_2.png; key T; wait 90; shot {out}/ghost_3.png; key T; wait 30; quit'),
    # Halluzination 2: das Licht des Raums faellt aus (vorher/nachher), dann mit Lampe
    'blackout': ('Halluzination: Raum ohne Licht',
        W0 + 'pos 18 28 1.5708 0; wait 60; shot {out}/blackout_1.png; haunt 2; wait 50; shot {out}/blackout_2.png; key T; wait 90; '
        'shot {out}/blackout_3.png; quit'),
    # Fundstuecke in der Hand im Licht von Level 0: Leuchtstab knicken und werfen (F), Beil schwingen (F)
    'hand': ('Fundstuecke in der Hand (Leuchtstab, Beil)',
        W0 + 'pos 18 28 1.5708 -0.15; wait 30; give glowstick; give hatchet; wait 20; key F; wait 9; shot {out}/hand_1.png; '
        'wait 70; shot {out}/hand_2.png; lurecam 40 25 1.2; wait 4; shot {out}/hand_3.png; cam off; worldlight 0; wait 4; '
        'shot {out}/hand_4.png; worldlight 1; key 2; wait 20; key F; wait 7; shot {out}/hand_5.png; wait 6; shot {out}/hand_6.png; quit'),
    'zones': ('Zonen von Level 0',
        W0 + _view('zones_1', 81.5, 60, 1.571) + _view('zones_2', 62, 85.5, 0.0) + _view('zones_3', 80.5, 21, 3.142) +
        _view('zones_4', 116, 31.5, 0.524, lamp=True) + _view('zones_5', 128, 121.5, 1.92) + _view('zones_6', 37, 40.5, 4.712) +
        _view('zones_7', 154, 64, 0.0) + _view('zones_8', 63.4, 85.5, 0.0, -0.45) +
        _view('zones_9', 169.5, 64, 0.0, 0.0, lamp=True) + 'quit'),  # Naht: die flackernde Wand nach Level 1
}


# V13.3 Level 5 (Abnahme): alle Wesen des Hotels in ihren Raeumen. Die Wesen ohne Rolle (Kolonie, Woodlins, Watchers, Gastgeber)
# stehen an den Plaetzen der Karte (hotel <art> <abstand> <winkel>); Samantha im Hauptgang (Rolle 19). Vierter Eintrag: Anzeige
# an (Untertitel der Stimmen, Meldungen). Kesselgang: Gang der Kesselraeume (120..160, 30..32).
LEVEL5 = {
    'samantha': ('Samantha (Entity 26)',
        W0 + 'pos 70 66 0 0; wait 20; presence samantha 8; wait 30; near samantha 3.0; wait 40; shot {out}/samantha_1.png; ' +
        _lamp('samantha', 2) + 'figclear; figure samantha 1.3; figstate crouch 1; figstate talk 1; figstate tilt 0.55; wait 40; '
        'figcam 12 3 0.6 0.24; wait 5; shot {out}/samantha_3.png; figstate talk 0; figstate tilt 0; figstate growl 1; figstate shiver 1; '
        'figstate jaw 1; figstate wave 1; figcam 0 2 0.42 0.22; wait 30; shot {out}/samantha_4.png; key T; wait 90; '
        'shot {out}/samantha_5.png; quit'),
    'samflow': ('Samantha ohne Fleisch (Warnung, Sprung)',
        W0 + 'pos 70 66 0 0; wait 20; presence samantha 8; wait 20; near samantha 2.7; wait 50; shot {out}/samflow_1.png; '
        'near samantha 1.9; wait 70; shot {out}/samflow_2.png; wait 60; shot {out}/samflow_3.png; wait 160; shot {out}/samflow_4.png; quit',
        '--level level5 --seed 3', True),
    'samtrade': ('Samantha mit Fleisch (Tausch, Lesung)',
        W0 + 'give steak; pos 70 66 0 0; wait 20; presence samantha 8; wait 20; near samantha 2.6; key T; wait 50; '
        'shot {out}/samtrade_1.png; near samantha 1.2; wait 40; shot {out}/samtrade_2.png; look 0 -0.5; wait 120; '
        'shot {out}/samtrade_3.png; wait 150; shot {out}/samtrade_4.png; quit', '--level level5 --seed 3', True),
    'colony': ('Kolonie (Entity 89)',
        W0 + 'hotel colony 2.4 0 0; wait 40; shot {out}/colony_1.png; key T; hotel colony 1.5 0.4 0; wait 90; shot {out}/colony_2.png; '
        'hotelforce colony 0; wait 28; shot {out}/colony_3.png; wait 140; look 0 -1.2; wait 20; shot {out}/colony_4.png; quit'),
    'woodlin': ('Woodlins (Entity 45)',
        W0 + 'hotel woodlin 3.5 0 0; hotelforce woodlins 0; hotelstate hold 1; wait 40; shot {out}/woodlin_1.png; ' +
        _lamp('woodlin', 2) + 'hotel woodlin 1.6 0.5 0; wait 30; shot {out}/woodlin_3.png; key T; wait 90; shot {out}/woodlin_4.png; '
        'key T; wait 30; hotel woodlin 7 0 0; wait 30; shot {out}/woodlin_5.png; hotel woodlin 2.4 0 0; hotelstate hold 0; '
        'look 3.14 0; wait 90; look 3.14 0; wait 3; shot {out}/woodlin_6.png; quit'),
    'watcher': ('Watcher (Entity 38)',
        W0 + 'hotel watcher 7 0.5 0; hotelstate hold 1; hotelstate wa_peek 1; wait 40; shot {out}/watcher_1.png; ' +
        _lamp('watcher', 2) + 'hotel watcher 3.5 0.3 0; wait 30; shot {out}/watcher_3.png; hotel watcher 2.6 1.1 0; wait 30; '
        'shot {out}/watcher_4.png; key T; wait 90; shot {out}/watcher_5.png; key T; wait 30; hotel watcher 1.8 0.2 0; '
        'hotelstate wa_arm 1; wait 20; shot {out}/watcher_6.png; quit'),
    'host': ('Gastgeber (Entity 18): Bildnis und Buero',
        W0 + 'hotel portrait 2.2 0 0; hotelforce host 1; wait 60; shot {out}/host_1.png; hotel portrait 3.6 0 0; wait 20; '
        'shot {out}/host_2.png; hotel office 4.5 0 0; hotelforce host 2; hotelstate host_arm 1; hotelstate host_lamp 1; wait 80; '
        'shot {out}/host_3.png; hotel office 1.4 0 0; look 0 -0.35; wait 30; shot {out}/host_4.png; key T; wait 90; '
        'shot {out}/host_5.png; quit'),
    'hostflow': ('Gastgeber: Ablauf (Stimme, Buero, Hand)',
        W0 + 'sanity 45; hotelstate host_on 300; pos 52 66 3.14 0; wait 200; hotel portrait 1.8 0 0; wait 30; '
        'shot {out}/hostflow_1.png; hotel office 3.2 0 0; wait 220; shot {out}/hostflow_2.png; hotel office 1.3 0 0; '
        'look 0 -0.25; wait 280; shot {out}/hostflow_3.png; key E; wait 40; shot {out}/hostflow_4.png; quit',
        '--level level5 --seed 3', True),
    'moth': ('Deathmoth-Maennchen an den Birnen (Entity 4)',
        W0 + 'pos 130 31 0 0; wait 30; presence moth; wait 120; near moth 3.2; wait 30; shot {out}/moth_1.png; ' + _lamp('moth', 2) +
        'quit'),
    # das Nest entsteht an der Tuer eines dunklen Zimmers (Regie ruhig gestellt: an dieser Tuer erzwingen, 54/68 am Hauptgang)
    'nest': ('Nest und Weibchen (Entity 4)',
        W0 + 'pos 54.5 64.8 1.5708 0; wait 30; hiveat 54 68; wait 60; shot {out}/nest_1.png; ' + _lamp('nest', 2) +
        'pos 54.5 66.6 1.5708 0.15; key T; wait 120; shot {out}/nest_3.png; pos 54.5 67.6 1.5708 0.1; wait 150; '
        'shot {out}/nest_4.png; wait 120; shot {out}/nest_5.png; quit'),  # in der Tuer: die Weibchen fliegen auf
    'hound': ('Hound (Entity 8)',
        W0 + 'pos 70 66 0 0; wait 30; presence hound 9; wait 150; near hound 4; wait 20; shot {out}/hound_1.png; ' +
        'near hound 6; key T; wait 45; shot {out}/hound_2.png; quit'),  # mit Lampe greift er an: das Bild vorher
    'skinstealer': ('Skin-Stealer, wahre Gestalt (Entity 10)',
        W0 + 'presence skinstealer 6; wait 240; near skinstealer 3.2; wait 2; shot {out}/skinstealer_1.png; ' +
        _lamp('skinstealer', 2) + 'near skinstealer 1.8; wait 2; shot {out}/skinstealer_3.png; quit'),
    'rat': ('Death Rats (Entity 24)',
        W0 + 'presence rat; wait 150; near rat 2.8; wait 10; shot {out}/rat_1.png; ' + _lamp('rat', 2) +
        'key T; near rat 1.3; wait 20; shot {out}/rat_3.png; quit'),
    'growler': ('Growler, ausgewachsen (Entity 130)',
        W0 + 'presence growler; wait 150; near growler 3.5; wait 20; shot {out}/growler_1.png; ' + _lamp('growler', 2) + 'quit'),
    'nguithrxurh': ("Nguithr'xurh (Entity 16)",
        W0 + 'presence nguithrxurh; wait 150; near nguithrxurh 3.2; wait 20; shot {out}/nguithrxurh_1.png; ' +
        _lamp('nguithrxurh', 2) + 'quit'),
    'wrangler': ('Wrangler im Kesselgang (Entity 75)',
        W0 + 'pos 126 31 0 0; wait 30; figure wrangler 3.6; wait 60; shot {out}/wrangler_1.png; ' + _lamp('wrangler', 2) +
        'figclear; presence wrangler; wait 200; near wrangler 4; key T; wait 90; shot {out}/wrangler_3.png; quit'),
}


# V13.3 Level 5.3 (Abnahme): der Hausherr (Rolle 20) an seinem Platz in der grossen Halle (Spawnpunkt gm/stand 54,5/105,6, Blick
# nach Westen) und im alten Gang (gm/door 63,5/129); der Besuch aus fester Kamera am Ende des Gangs (63,5/98, Anzeige an); die
# Stoerung von hinten; der Zorn der Bibliothek (Buecherhaufen rechts von seinem Platz, 52,9/106,6); das Wissen der Bibliothek auf
# der Galerie (Folio 65,1/41,2 und Karteikarten 67,3/45,7, 4,1 m hoch). Haltungen: gent hold/pose/stand (presence_gentleman.cpp).
GM0 = W0 + 'pos 50 100 0 0; wait 20; presence gentleman; wait 30; gent hold 1; '
LEVEL5_3 = {
    'gentleman': ('Hausherr (Entity 18): fern, mittel, nah',
        GM0 + 'gent pose 1; gent stand 1; wait 30; near gentleman 8 0; wait 40; shot {out}/gentleman_1.png; ' + _lamp('gentleman', 2) +
        'near gentleman 3.6 0.35; wait 30; shot {out}/gentleman_3.png; ' + _lamp('gentleman', 4) +
        'near gentleman 1.5 0.2; wait 30; shot {out}/gentleman_5.png; ' + _lamp('gentleman', 6) + 'quit'),
    'gentpose': ('Hausherr: Verbeugung, Lesen, Wurf, Ansprache',
        GM0 + 'gent pose 3; gent stand 3; wait 30; near gentleman 3.2 1.1; wait 40; shot {out}/gentpose_1.png; gent pose 4; wait 40; '
        'shot {out}/gentpose_2.png; gent pose 5; gent book 4.6; wait 30; shot {out}/gentpose_3.png; gent book -1; gent pose 6; wait 40; '
        'near gentleman 4 0.3; wait 30; shot {out}/gentpose_4.png; key T; wait 90; shot {out}/gentpose_5.png; quit'),
    'gentnotice': ('Hausherr: Wahrnehmen (Kopf bis 166 Grad), Ankuendigung, Griff',
        GM0 + 'gent pose 11; gent stand 11; wait 30; near gentleman 2.6 3.1416; wait 40; shot {out}/gentnotice_1.png; ' +
        _lamp('gentnotice', 2) + 'gent pose 13; near gentleman 2.1 0; wait 40; shot {out}/gentnotice_3.png; gent pose 14; '
        'gent place 0.85; wait 30; shot {out}/gentnotice_4.png; key T; wait 90; shot {out}/gentnotice_5.png; quit'),
    # der Besuch, wie er im Spiel ablaeuft (60 Bilder je Sekunde): Tuer und Gang, an seinem Platz Umsehen, Notizbuch, Wurf,
    # Ansprache mit Stille, Rueckzug; danach der Zorn der Bibliothek (Buecherhaufen) und das Notizbuch als Fund
    'gentflow': ('Hausherr: Besuch (Gang, Szene, Ansprache, Rueckzug), Zorn der Bibliothek',
        # Runde 2: aus 12 m war er im Bild zu klein - Gang aus der Halle (63,5/101), die Szene aus 6 m (59/101,5), beide ueber 3 m
        # von seinem Weg (sonst bemerkt er den Spieler); zuletzt der Fund hinter ihm und die Buecherhaufen
        W0 + 'pos 63.5 101 1.5708 0.04; wait 30; presence gentleman; wait 180; shot {out}/gentflow_1.png; wait 360; '
        'shot {out}/gentflow_2.png; wait 300; shot {out}/gentflow_3.png; pos 59.0 101.5 2.402 0.1; wait 540; shot {out}/gentflow_4.png; '
        'wait 300; shot {out}/gentflow_5.png; wait 150; shot {out}/gentflow_6.png; wait 150; shot {out}/gentflow_7.png; wait 180; '
        'shot {out}/gentflow_8.png; wait 540; shot {out}/gentflow_9.png; wait 360; shot {out}/gentflow_10.png; wait 660; '
        'shot {out}/gentflow_11.png; wait 600; shot {out}/gentflow_12.png; pos 59.1 103.4 1.5708 -0.62; wait 60; '
        'shot {out}/gentflow_13.png; pos 55.8 104.3 2.48 -0.42; wait 60; shot {out}/gentflow_14.png; key T; wait 90; '
        'shot {out}/gentflow_15.png; quit', '--level level5_3 --seed 3', True),
    # Stoerung im Spiel: von hinten naeher als 3 m - er bringt zu Ende, was er tut, dreht den Kopf, kommt, kuendigt an, greift.
    # Runde 2: der Spieler stand beim Erscheinen am Start im Gang (0,4 m) - bemerkt, bevor er an seinem Platz war; jetzt erst fort
    'gentdisturb': ('Hausherr: Stoerung von hinten (Ablauf), mit Untertiteln',
        W0 + 'pos 50 100 0 0; wait 30; presence gentleman; wait 30; gent stand 1; pos 57.4 105.6 3.1416 0.08; wait 90; '
        'shot {out}/gentdisturb_1.png; wait 120; shot {out}/gentdisturb_2.png; wait 120; shot {out}/gentdisturb_3.png; wait 120; '
        'shot {out}/gentdisturb_4.png; wait 120; shot {out}/gentdisturb_5.png; wait 120; shot {out}/gentdisturb_6.png; wait 120; '
        'shot {out}/gentdisturb_7.png; wait 120; shot {out}/gentdisturb_8.png; wait 180; shot {out}/gentdisturb_9.png; quit',
        '--level level5_3 --seed 3', True),
    'lore': ('Wissen der Bibliothek: Folio (Entity 141), Karteikarten (Entity 189)',
        W0 + 'pos 65.0 43.1 -1.5708 -0.55; wait 60; shot {out}/lore_1.png; key T; wait 90; shot {out}/lore_2.png; key T; wait 30; '
        'pos 67.45 47.4 -1.69 -0.5; wait 60; shot {out}/lore_3.png; key T; wait 90; shot {out}/lore_4.png; key T; wait 30; '
        'pos 64.4 44.4 -1.2 -0.12; wait 60; shot {out}/lore_5.png; quit'),
    'loreread': ('Wissen: Folio lesen (Diktiergeraet), Karteikarten lesen',
        W0 + 'pos 65.0 42.3 -1.45 -0.45; wait 60; key E; wait 60; shot {out}/loreread_1.png; wait 240; shot {out}/loreread_2.png; '
        'key E; wait 30; pos 67.4 46.6 -1.69 -0.7; wait 60; key E; wait 60; shot {out}/loreread_3.png; quit',
        '--level level5_3 --seed 3', True),
    # Abnahme Runde 4: Bereiche und Uebergaenge abseits des Laufwegs (Sicherungspunkte der Karte): vor der Leere mit dem weissen
    # Gitter, Ostfluegel zur Tuer nach Level 5, alte Gaenge (+6 m), Treppe, Obergeschoss, Kronleuchter und Wappen der Halle
    'zones': ('Bereiche und Uebergaenge (Leere, Ausgang, alte Gaenge, Treppe, Obergeschoss, Kronleuchter)',
        W0 + _view('zones_1', 14.0, 81.0, 3.1416, 0.0, lamp=True) + _view('zones_2', 123.5, 80.5, 0.0) +
        _view('zones_3', 103.5, 23.5, 1.5708, 0.0, lamp=True) + _view('zones_4', 90.5, 23.5, 0.0) +
        _view('zones_5', 64.0, 43.5, 1.5708) + _view('zones_6', 64.0, 80.0, 1.5708, 0.55) + 'quit'),
}


# V13.3 Level 188 (Abnahme): alle Wesen des Hofs und der Gaenge in ihren Raeumen. Fensterwesen (drei dunkle Fenster im
# Erdgeschoss, einmal je Ebene gesetzt), Gestalt im Hof, Faceling im rosa Kleid und Skin-Stealer nur in den Gaengen; Horcher,
# Auftritt aus der Zelle und Wurzelpistole aus shots_l188.py (gleiche Ablaeufe wie in den Kritikrunden der Briefings).
# Dazu die Bereiche (LOOK_188) und was seit E-048 auf allen Ebenen wirkt: Stromausfall (Leuchtflaechen), Spiegelung, Beil.
def _l188():
    from shots_l188 import jobs as l188_jobs
    J = l188_jobs()
    G0 = 'pos 58.5 52.5 -1.5708 0; ' + W0  # Suedende des weissen Gangs, Blick nach Norden
    return {
        'window': ('Fensterwesen (Entity 2): fern, Lampe, nah, Arm',
            W0 + 'pos 30 33 0 0; wait 60; near window 7; wait 40; shot {out}/window_1.png; ' + _lamp('window', 2) +
            'near window 3.4; wait 60; shot {out}/window_3.png; key T; wait 90; shot {out}/window_4.png; key T; wait 20; '
            'near window 1.7; wait 50; shot {out}/window_5.png; quit'),
        'gestalt': ('Umriss/Gestalt im Hof',
            W0 + 'pos 33 46 -1.5708 0; wait 30; presence gestalt 16; wait 30; face gestalt; wait 3; shot {out}/gestalt_1.png; '
            'near gestalt 7; wait 30; shot {out}/gestalt_2.png; ' + _lamp('gestalt', 3) + 'quit'),
        # Runde 3: "near" stellte die Kamera im 3 m breiten Gang an die Wand (Kopf abgeschnitten), dann toetete sie und die
        # Vorfuehrung lag unter dem roten Schleier - jetzt erst die Vorfuehrung im Gang (Licht der Ebene, Lampe, nah), dann der
        # echte Auftritt, die Kamera folgt ihr (face), bis sie zuschlaegt
        'dress': ('Faceling im rosa Kleid (Entity 9)',
            G0 + 'figure faceling 3.4; figstate variant 2; wait 60; shot {out}/dress_1.png; key T; wait 90; shot {out}/dress_2.png; '
            'key T; wait 20; figcam 12 4 1.3 1.5; wait 5; shot {out}/dress_3.png; figcam -40 4 2.8 1.0; wait 5; shot {out}/dress_4.png; '
            'figclear; cam off; wait 10; presence faceling 14; wait 120; near faceling 4.5 0 1; face faceling; wait 2; shot {out}/dress_5.png; wait 50; '
            'shot {out}/dress_6.png; wait 50; shot {out}/dress_7.png; wait 40; shot {out}/dress_8.png; quit'),  # Runde 4: in ihrer Zelle
        'skinstealer': ('Skin-Stealer, wahre Gestalt (Entity 10)',
            G0 + 'figure skinstealer 3.4; wait 60; shot {out}/skinstealer_1.png; ' + _lamp('skinstealer', 2) +
            'figcam 20 6 1.6 1.45; wait 5; shot {out}/skinstealer_3.png; figclear; cam off; wait 10; presence skinstealer 10; wait 200; '
            'near skinstealer 4.5 0 1; face skinstealer; wait 2; shot {out}/skinstealer_4.png; key T; wait 60; shot {out}/skinstealer_5.png; wait 60; '
            'shot {out}/skinstealer_6.png; quit'),
        'horcher': J['horcher'],
        'auftritt': J['auftritt'],
        'pistole': J['pistole'],
        # E-048: Leuchtflaechen erloeschen im Stromausfall (Halluzination 2 im Gang), Beil in der Faust, Spiegelung im Glas
        'blackout': ('Stromausfall im Gang (Leuchtflaechen)',
            'pos 58.5 47 -1.5708 0.12; ' + W0 + 'wait 30; shot {out}/blackout_1.png; haunt 2; wait 50; shot {out}/blackout_2.png; '
            'key T; wait 90; shot {out}/blackout_3.png; quit'),
        'hand': ('Fundstuecke in der Hand (Beil) im Gang',
            'pos 58.5 40 -1.5708 -0.15; ' + W0 + 'wait 20; give glowstick; give hatchet; wait 20; key 2; wait 30; '
            'shot {out}/hand_1.png; key F; wait 7; shot {out}/hand_2.png; wait 6; shot {out}/hand_3.png; quit'),
        'zones': ('Bereiche (LOOK_188): Fenster, Moos, Kreis, Tisch, Galerie, Gang, Treppe, Zelle',
            W0 + _view('zones_1', 15.0, 33.5, 0.0, 0.05) + _view('zones_2', 17.5, 34.5, 3.0, 0.0) +
            _view('zones_3', 24.0, 40.0, 0.6, -0.75, lamp=True) + _view('zones_4', 37.5, 35.0, 3.6, -0.3) +
            _view('zones_5', 43.4, 43.2, 4.0, -0.35, lamp=True) + _view('zones_6', 30.0, 22.0, -1.5708, 0.55) +
            _view('zones_7', 33.0, 33.0, 0.0, 1.35) + _view('zones_8', 50.0, 38.5, 0.0, 0.0) +
            _view('zones_9', 58.5, 31.2, 1.5708, 0.3) + _view('zones_10', 58.5, 55.5, 1.5708, 0.25) +
            _view('zones_11', 59.4, 68.0, 3.1416, 0.0) + _view('zones_12', 64.5, 33.0, 3.1416, 0.0, lamp=True) + 'quit'),
    }


# V13.3 Level 903 (Abnahme): die Metro (Entity 756) ist der fahrende Wagen am linken Gleis (Ziele 37/36), dazu friedliche
# Facelings auf dem Steig (winken, wen sie wiedererkennen). Wagen: am Steig, Seite im Dunkeln nur mit Lampe (E-041: Modelle
# auf unsichtbaren Bloecken nicht im Schatten ihres Blocks), Tuer, innen mit beiden Zielanzeigen, vom Gleis. Ruhender Zug,
# bewohnter Wagen, Kupplung, Glas (Ersatzspiegelung mit Wandpruefung, E-048); Stromausfall, Beil; Bereiche (LOOK_903).
# Die Wagenenden (x < 94, x > 107) sind Uebergaenge nach 37/36 - die Kamera bleibt dazwischen.
def _l903():
    def dark(name, x, y, yaw, pitch=0.0):  # Licht der Ebene aus, nur die Lampe (Schatten unsichtbarer Bloecke)
        return (f'pos {x} {y} {yaw} {pitch}; worldlight 0; key T; wait 90; shot {{out}}/{name}.png; key T; worldlight 1; '
                'wait 30; ')
    return {
        'metro': ('Metro-Wagen (Entity 756): Front, Lampe, Tuer, innen 37/36, vom Gleis',
            W0 + _view('metro_1', 84.0, 9.2, -0.62, -0.02) + dark('metro_2', 96.5, 7.8, -2.85, -0.12) +
            _view('metro_3', 100.5, 8.6, -1.5708, -0.06) + _view('metro_4', 101.5, 5.0, 3.1416, 0.04) +
            _view('metro_5', 99.5, 5.0, 0.0, 0.04) + _view('metro_6', 98.0, 5.6, 2.2, -0.2, lamp=True) +
            _view('metro_7', 80.0, 4.6, 0.0, -0.02, lamp=True) +
            # Runde 3: unter dem Wagen (Stirn vom Gleis, Spalt an der Tuer, Wandseite vom schmalen Streifen am Gleis)
            _view('metro_8', 86.0, 5.0, 0.0, -0.15, lamp=True) + _view('metro_9', 97.0, 7.35, -1.5708, -0.95) +
            _view('metro_10', 100.0, 2.5, 1.5708, -0.35, lamp=True) + 'quit'),
        # friedlich: die Gruppe geht ihres Wegs; wer laenger nah ist, dem winkt eines (nach etwa 4 s mit Sicht)
        'faceling': ('Facelings, friedlich (Entity 9): Gruppe, Lampe, Winken, Varianten',
            W0 + 'pos 140 11.5 0 0; wait 20; presence faceling 12; wait 120; near faceling 6 0 1; face faceling; wait 2; '
            'shot {out}/faceling_1.png; key T; wait 90; shot {out}/faceling_2.png; key T; wait 20; near faceling 3.5; '
            'face faceling; wait 240; shot {out}/faceling_3.png; figclear; pos 120 11.5 3.1416 0; wait 10; '
            'figure faceling 3.2; figstate variant 0; wait 60; shot {out}/faceling_4.png; figstate variant 3; wait 30; '
            'shot {out}/faceling_5.png; figstate variant 4; wait 30; shot {out}/faceling_6.png; key T; wait 90; '
            'shot {out}/faceling_7.png; quit'),
        'zug': ('Ruhender Zug, bewohnter Wagen, Kupplung, Glas',
            W0 + _view('zug_1', 58.0, 13.0, 0.17, 0.0) + _view('zug_2', 88.0, 12.4, 1.5708, 0.05) +
            dark('zug_3', 88.0, 12.4, 1.5708, 0.05) + dark('zug_4', 96.6, 13.4, 1.0, 0.0) +
            _view('zug_5', 70.0, 14.0, 1.25, 0.12) + _view('zug_6', 132.0, 13.6, 0.9, 0.05, lamp=True) + 'quit'),
        'blackout': ('Stromausfall am Steig (Leuchtflaechen)',
            'pos 112 11.5 3.1416 0.1; ' + W0 + 'wait 30; shot {out}/blackout_1.png; haunt 2; wait 50; '
            'shot {out}/blackout_2.png; key T; wait 90; shot {out}/blackout_3.png; quit'),
        'hand': ('Fundstuecke in der Hand (Beil) am Steig',
            'pos 80 11.5 3.1416 -0.15; ' + W0 + 'wait 20; give glowstick; give hatchet; wait 20; key 2; wait 30; '
            'shot {out}/hand_1.png; key F; wait 7; shot {out}/hand_2.png; wait 6; shot {out}/hand_3.png; quit'),
        'zones': ('Bereiche (LOOK_903): Steig, Gleis, Treppe vorn/hinten, Schilder, Enden',
            W0 + _view('zones_1', 118.0, 11.5, 3.1416, 0.04) + _view('zones_2', 75.0, 7.7, -1.5708, -0.55, lamp=True) +
            _view('zones_3', 54.0, 11.5, 0.0, 0.1, lamp=True) + _view('zones_4', 68.5, 11.5, 3.1416, 0.1, lamp=True) +
            _view('zones_5', 76.0, 11.5, 0.0, 0.35) + _view('zones_6', 76.5, 7.8, -1.5708, 0.12) +
            _view('zones_7', 14.0, 11.5, 3.1416, 0.0, lamp=True) + _view('zones_8', 186.0, 11.5, 0.0, 0.0, lamp=True) +
            _view('zones_9', 6.0, 11.5, 0.0, 0.0) +
            # Runde 3: Rueckseite der Treppe nah, Ende im Westen nah, letzter Zugabschnitt im Osten
            _view('zones_10', 66.2, 11.0, 3.1416, -0.05, lamp=True) + _view('zones_11', 5.5, 11.5, 3.1416, -0.05, lamp=True) +
            _view('zones_12', 190.0, 12.5, 0.6, 0.0, lamp=True) + 'quit'),
    }


def note_job(level, seed):
    """V13.3 Level 0: Notizen dort aufnehmen, wo das Spiel sie hinlegt (Protokoll "Notiz ... bei (x, y)"), Lampe aus/an."""
    if level != 'level0':
        return None
    import re
    code, log = run(f'--level {level} --seed {seed}', 'wait 30; quit',
                    userdir=os.path.join(ROOT, 'build', 'qa', 'ud_creatures', 'notes_probe'), size='640x360')
    spots = [(float(a), float(b)) for a, b in re.findall(r'Notiz "[^"]*" bei \((-?[\d.]+), (-?[\d.]+)\)', log)]
    g = Grid(level)
    parts = []
    for k, (x, y) in enumerate(spots[:3]):
        best = None
        for a in [i * math.pi / 8 for i in range(16)]:
            cx, cy = x + math.cos(a) * 1.3, y + math.sin(a) * 1.3
            if g.ok(cx, cy) and g.ok((x + cx) / 2, (y + cy) / 2):
                best = (cx, cy)
                break
        if not best:
            continue
        cx, cy = best
        yaw = math.atan2(y - cy, x - cx)
        pitch = math.atan2(-1.6, 1.3)
        parts.append(f'pos {cx:.2f} {cy:.2f} {yaw:.3f} {pitch:.3f}; wait 50; shot {{out}}/notes_{2 * k + 1}.png; '
                     f'key T; wait 90; shot {{out}}/notes_{2 * k + 2}.png; key T; wait 30; ')
    if not parts:
        return None
    return ('Notizen (Fundort im Spiel)', W0 + ''.join(parts) + 'quit')


# Spuren (Ausstattung der Karte): Kamera vor das Modell aus dem Ausstattungs-Abzug
TRACES = {'level2': [('skin_hung', 0, 2.3), ('skin_lair', 0, 2.6), ('skin_shed', 0, 2.2), ('skin_shed', 1, 2.2),
                     ('skin_scratches', 0, 1.8)]}
JOBS = {'level0': LEVEL0, 'level2': LEVEL2, 'level3': LEVEL3, 'level5': LEVEL5, 'level5_3': LEVEL5_3, 'level188': _l188(),
        'level903': _l903()}


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


def _order(rest):
    """Reihenfolge im Bogen: zones_2 vor zones_2_lampe vor zones_3 (V13.3 Abnahme Level 903: "_lampe" stand vorher vorn)."""
    num, _, suffix = rest.partition('_')
    return (int(num) if num.isdigit() else 0, suffix)


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
    if not a.sheet_only and (not a.only or 'notes' in a.only.split(',')):
        nj = note_job(a.level, a.seed)
        if nj:
            jobs['notes'] = nj
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
            # dritter Eintrag: eigene Startargumente; vierter: Anzeige an (Untertitel, Meldungen)
            code, log = run(args[0] if args else f'--level {a.level} --seed {a.seed}', script.format(out=raw.replace('\\', '/')),
                            userdir=os.path.join(ROOT, 'build', 'qa', 'ud_creatures', job), size=a.size,
                            hud=len(args) > 1 and bool(args[1]))
            warn = [ln.strip() for ln in log.splitlines() if 'Skript' in ln and ('kein' in ln or 'unbekannt' in ln or 'nicht' in ln)]
            report[job] = {'exit': code, 'warn': warn[:6]}
            print(f'{job}: exit {code}', warn[:3], flush=True)
    # alle vorhandenen Bilder dieser Runde (auch aus frueheren Teil-Laeufen mit --only), Reihenfolge der Tabelle
    labels = {k: v[0] for k, v in dict(JOBS.get(a.level, {}), traces=('Skin-Stealer-Spuren (Entity 10)', ''),
                                       notes=('Notizen (Fundort im Spiel)', '')).items()}
    shots = []
    for job in labels:
        fs = sorted((f for f in os.listdir(raw) if f.startswith(job + '_') and f.endswith('.png')),
                    key=lambda f: _order(f[len(job) + 1:-4]))
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
