"""V13.3 Teil 4 Level 2: Randwesen ohne Rolle (gameplay/apparitions_l2.cpp; Wikidot Level 2 Rev. 93, Entity 36 Rev. 13,
Entity 192 Rev. 17, Entity 104 Rev. 43, Entity 189 Rev. 27 - alle CC BY-SA 3.0).

Bewegliche Wesen bestehen aus starren Teilen, die das Spiel in einer Kette dreht (Ursprung jedes Teils = sein Gelenk,
das Teil liegt entlang +x). Die Gelenkpunkte stehen hier als Konstanten und gleichlautend in apparitions_l2.cpp.

Cannibal Cuisine (Entity 36: anomaler Verkaufsautomat, etwa 1,83 x 1,22 x 0,76 m, zwei Tastenfelder - Ziffern und
Buchstaben -, statt der Spiralen schiebt je Fach eine abgetrennte, skelettierte Menschenhand die Ware vor; Waren aus
Menschenteilen in Snackverpackung; zwei Klappen oben und unten, aus denen metallisch ueberzogene Skelettarme bis 1,37 m
kommen, die unteren dienen als Beine; drinnen ein Lager mit armlosen Leichen, an einer eine Karteikarte, Nachname Iris;
hinten ein Schild des Herstellers "Cannibal Cuisine Productions", Iris Family, 2019, Arizona; Level 2: er leuchtet):
  cc_body@0..2   Gehaeuse (drei Lackierungen wie Automaten verschiedener Marken, keine echten Marken), Fachwerk mit Ware
                 und Knochenhaenden, Tastenfelder, Ausgabefach, Hohlraeume hinter den Klappen (unten: das Gesicht einer
                 der Leichen mit Karteikarte), Herstellerschild hinten
  cc_flap_top@0..2  Kopfschild (leuchtet; Gelenk an der Oberkante), cc_flap_bot  Sockelklappe (Gelenk an der Unterkante)
  cc_humerus, cc_forearm, cc_hand@0 (Kralle) / @1 (gespreizt)  Arm aus Knochen mit Metallueberzug (0,55 + 0,52 + 0,30 m)
  cc_ware        die Ware (Fleischblock in Schokoriegelpapier) - Fundstueck "Ware aus dem Automaten"
Swindlebird (Entity 192: Gattung taubenaehnlicher Voegel; der blaue sieht aus wie ein gewoehnlicher Papagei, die aeusseren
Federn tragen ein halluzinogenes Mittel; die Gattung teilt den Knochenbau - die rote Art hat drei Beingelenke):
  sb_body, sb_head, sb_wing_in@0/1, sb_wing_out@0/1 (links/rechts), sb_tail, sb_shin, sb_tars, sb_foot
Photoshop (Entity 104: Klotz neben einem Desktop-Rechner, etwa 2,36 m; Gesicht aus zwei Augenfotos und dem Foto eines
weit aufgerissenen Pitbull-Mauls, voellig flach; Koerper ein Block mit Fleischfotos auf allen Seiten, Glitch; aus dem
Bildschirm greift ein grosser, weisser, blutiger Arm; wer zu nahe kommt, den umschliesst eine durchsichtige Box mit
schwarzen Kanten):
  ps_block (@1..@4: waagrechte Scheiben fuer den Glitch), ps_screen (leuchtendes Bild auf dem Roehrenmonitor),
  ps_arm_a, ps_arm_b, ps_hand@0 (offen) / @1 (greift), ps_edge (Kante der Box, 1 m), ps_handle (Anfasser)
Macchina (Entity 189: seit etwa 1850 verschwunden; lebenslang in grossen, klobigen Metallanzuegen aus dem Material von
Level 2, auf die Visiere geschweisste menschenaehnliche Muster; ohne Anzug galt man nicht als lebendig, jedes dritte Kind
starb ohne Anzug; Speere und Piken, eine Roehre fuer brennende Geschosse; Bilder von Gestalten, die aus Lichtkugeln
herabsteigen; Schrift aus Zeichen):
  macchina_suit      leerer Anzug, an die Wand gesunken, Brustpanzer offen (hohl), Visier mit geschweisstem Gesicht
  macchina_cradle    unfertiges Kindergestell (nie fertig geworden), Tuch daruebergelegt
  macchina_tapestry  Wandtuch "die grosse Antwort": Lichtkugeln, herabsteigende Gestalten, kniende Anzugtraeger, Zeichen
  macchina_spear     Pike mit Macchina-Klinge an der Wand, davor die Brandroehre

Konventionen: Meter, z oben, Blick entlang +x; Bodenmodelle Ursprung am Boden; Wandmodelle: Wandflaeche x = 0.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

from models import models_l1 as L1
from models.models_l1_phenomena import blob2, ellipsoid, poly_on
from models.models_l1_sectors import Kit, blk, cyl, lathe, sweep
from models.models_l3 import mat

V = Vector
TAU = 2.0 * math.pi

# --- Gelenke (gleich in apparitions_l2.cpp) ---------------------------------------------------------------------------
CC_W, CC_D, CC_H = 1.22, 0.76, 1.83
CC_FLAP_TOP = (0.36, 1.81)        # Gelenk der Kopfklappe (x, z), klappt nach oben
CC_FLAP_BOT = (0.36, 0.08)        # Gelenk der Sockelklappe (x, z), klappt nach unten
CC_ARM_LEN = (0.55, 0.52, 0.30)   # Oberarm, Unterarm, Hand
SB_NECK = (0.085, 0.0, 0.055)     # am Koerper (Ursprung Huefte)
SB_SHOULDER = (0.045, 0.052, 0.04)
SB_WRIST = 0.11                   # Laenge des inneren Fluegels
SB_TAIL = (-0.115, 0.0, 0.012)
SB_LEG = (0.075, 0.07, 0.06)      # Unterschenkel, Lauf 1, Lauf 2 (drei Beingelenke)
PS_W, PS_H = 0.9, 2.36
PS_ARM = (0.62, 0.56)


def _mats():
    return {
        # Automat
        'lack0': mat('cc_lack_rot', (0.52, 0.06, 0.05), 'paint', r=0.75),
        'lack1': mat('cc_lack_blau', (0.07, 0.16, 0.42), 'paint', r=0.75),
        'lack2': mat('cc_lack_schwarz', (0.06, 0.06, 0.065), 'paint', r=0.7),
        'trim': mat('cc_chrom', (0.78, 0.78, 0.80), 'metal', r=0.45),
        'dark': mat('cc_dunkel', (0.03, 0.03, 0.035), 'plastic', r=1.2),
        'cavity': mat('cc_hohlraum', (0.015, 0.012, 0.012), 'plastic', r=1.4),
        'rubber': mat('cc_gummi', (0.05, 0.05, 0.05), 'rubber', r=1.2),
        'backlit': mat('cc_rueckwand', (1.0, 0.95, 0.86), 'glow', glow=0.55),
        'tube': mat('cc_roehre', (1.0, 0.97, 0.92), 'glow', glow=2.2),
        'shelf': mat('cc_fachblech', (0.55, 0.56, 0.58), 'metal', r=0.8),
        'label': mat('cc_fachschild', (0.92, 0.91, 0.86), 'plastic', r=1.0),
        'ink': mat('cc_schrift', (0.04, 0.04, 0.04), 'plastic', r=1.0),
        'key': mat('cc_taste', (0.80, 0.80, 0.78), 'plastic', r=0.6),
        'lcd': mat('cc_anzeige', (0.04, 0.08, 0.04), 'glass', r=0.3),
        'lcdglow': mat('cc_anzeige_ziffer', (0.45, 1.0, 0.4), 'glow', glow=1.4),
        'plate': mat('cc_schild_blech', (0.62, 0.60, 0.55), 'metal', r=0.9),
        'bone': mat('cc_knochen', (0.80, 0.72, 0.56), 'ceramic', r=1.2),
        'bone_d': mat('cc_knochen_alt', (0.55, 0.45, 0.30), 'ceramic', r=1.35),
        'tissue': mat('cc_gewebe_trocken', (0.30, 0.12, 0.08), 'plastic', r=0.9),
        'coat': mat('cc_ueberzug', (0.42, 0.44, 0.47), 'metal', r=0.6, m=0.3),
        'coat_d': mat('cc_ueberzug_dunkel', (0.18, 0.18, 0.20), 'metal', r=0.8),
        'meat': mat('cc_fleisch', (0.52, 0.10, 0.09), 'plastic', r=0.5),
        'meat_d': mat('cc_fleisch_dunkel', (0.25, 0.04, 0.04), 'plastic', r=0.45),
        'fat': mat('cc_fett', (0.86, 0.78, 0.62), 'plastic', r=0.6),
        'blood': mat('cc_blut', (0.22, 0.01, 0.01), 'plastic', r=0.25),
        'glass': mat('cc_flaschenglas', (0.62, 0.70, 0.66), 'glass', r=0.2),
        'wrapb': mat('cc_papier_braun', (0.30, 0.15, 0.06), 'plastic', r=0.5),
        'wrapg': mat('cc_papier_gold', (0.85, 0.66, 0.22), 'metal', r=0.5),
        'wrapr': mat('cc_tuete_rot', (0.70, 0.12, 0.08), 'plastic', r=0.45),
        'wrapy': mat('cc_tuete_gelb', (0.86, 0.66, 0.12), 'plastic', r=0.45),
        'wrapw': mat('cc_folie_weiss', (0.88, 0.86, 0.82), 'plastic', r=0.4),
        'foil': mat('cc_alufolie', (0.75, 0.75, 0.76), 'metal', r=0.5),
        'can': mat('cc_dose_rot', (0.62, 0.05, 0.08), 'metal', r=0.5),
        'card': mat('cc_karton', (0.70, 0.62, 0.48), 'cardboard', r=1.3),
        'chips': mat('cc_markchips', (0.82, 0.70, 0.52), 'plastic', r=0.9),
        'tooth': mat('cc_zahn', (0.88, 0.84, 0.70), 'ceramic', r=0.7),
        'skin': mat('cc_haut', (0.78, 0.60, 0.50), 'plastic', r=0.9),
        'sugar': mat('cc_zucker', (0.95, 0.94, 0.90), 'plastic', r=1.3),
        'corpse': mat('cc_leiche', (0.48, 0.44, 0.34), 'plastic', r=1.0),
        'corpse_d': mat('cc_leiche_falte', (0.24, 0.20, 0.15), 'plastic', r=1.2),
        'gum': mat('cc_zahnfleisch', (0.36, 0.14, 0.13), 'plastic', r=0.7),
        'indexcard': mat('cc_karteikarte', (0.90, 0.88, 0.80), 'cardboard', r=1.3),
        'tape': mat('cc_klebeband', (0.72, 0.70, 0.60), 'plastic', r=0.8),
        'hlw': mat('cc_kopfschild_weiss', (1.0, 0.98, 0.94), 'glow', glow=1.1),
        'hlr': mat('cc_kopfschild_rot', (1.0, 0.18, 0.12), 'glow', glow=1.0),
        'hlb': mat('cc_kopfschild_blau', (0.25, 0.55, 1.0), 'glow', glow=1.0),
        'hlo': mat('cc_kopfschild_orange', (1.0, 0.55, 0.12), 'glow', glow=1.0),
        'hlk': mat('cc_kopfschild_schrift', (0.05, 0.03, 0.03), 'plastic', r=1.0),
    }


def _bm_mesh(bm, me, M):
    for p in me.polygons:
        bm.faces.new([bm.verts.new(M @ me.vertices[i].co) for i in p.vertices])


def text_on(bm, s, size, o, u, v, depth=0.0, align='CENTER', bold=False):
    """Schrift (Blender-Standardschrift) als Flaechen auf der Ebene o + u x + v y, Normale u x v; depth > 0: erhaben."""
    cu = bpy.data.curves.new('schrift', 'FONT')
    cu.body = s
    cu.size = size
    cu.align_x = align
    cu.align_y = 'CENTER'
    cu.extrude = depth / 2.0
    ob = bpy.data.objects.new('schrift', cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    me = ev.to_mesh()
    u, v = V(u).normalized(), V(v).normalized()
    n = u.cross(v)
    M = Matrix(((u.x, v.x, n.x, o[0] + n.x * depth / 2), (u.y, v.y, n.y, o[1] + n.y * depth / 2),
                (u.z, v.z, n.z, o[2] + n.z * depth / 2), (0, 0, 0, 1)))
    _bm_mesh(bm, me, M)
    ev.to_mesh_clear()
    bpy.data.objects.remove(ob)
    bpy.data.curves.remove(cu)


def quad(bm, a, b, c, d):
    bm.faces.new([bm.verts.new(V(p)) for p in (a, b, c, d)])


def box(bm, lo, hi):
    blk(bm, (hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]), ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2))


def bone_shaft(bm, a, b, r0, r1, knob0=1.6, knob1=1.5, segs=8):
    """Roehrenknochen: schlanker Schaft, verdickte Enden (Gelenkkoepfe)."""
    a, b = V(a), V(b)
    pts, rs = [], []
    for i in range(9):
        t = i / 8
        pts.append(a.lerp(b, t))
        w = 1.0 + (knob0 - 1.0) * max(0.0, 1.0 - t / 0.16) ** 2 + (knob1 - 1.0) * max(0.0, 1.0 - (1.0 - t) / 0.16) ** 2
        rs.append((r0 + (r1 - r0) * t) * w * (0.92 + 0.08 * math.sin(t * 9.0)))
    sweep(bm, pts, rs, segs)


def bone_hand(K, M, base, fwd, side, up, scale=1.0, curl=0.5, spread=0.0, mat_b='bone', mat_j='bone_d', coat=None):
    """Skelettierte Menschenhand: Handwurzel, fuenf Mittelhandknochen, Fingerglieder (gekruemmt um curl)."""
    base, fwd, side, up = V(base), V(fwd).normalized(), V(side).normalized(), V(up).normalized()
    bm = K(M[mat_b], 30)
    bj = K(M[mat_j], 30)
    s = scale
    # Handwurzel: kleine Knoechelchen
    rng = random.Random(int(abs(base.x * 1000 + base.y * 777 + base.z * 333)) + 5)
    for i in range(8):
        p = base + fwd * (0.012 + 0.012 * (i // 4)) * s + side * ((i % 4) - 1.5) * 0.011 * s + up * rng.uniform(-0.003, 0.003) * s
        ellipsoid(bj, p, fwd, 0.006 * s, 0.0055 * s, 6, 3)
    lens = [(0.045, 0.030, 0.022, 0.0), (0.068, 0.040, 0.026, 0.019), (0.066, 0.045, 0.028, 0.021),
            (0.062, 0.042, 0.026, 0.019), (0.058, 0.032, 0.022, 0.017)]  # Daumen, Zeige- .. Kleinfinger
    for f, (mc, p1, p2, p3) in enumerate(lens):
        if f == 0:
            root = base + fwd * 0.012 * s + side * -0.022 * s
            d = (fwd * 0.55 + side * -0.75 + up * 0.25).normalized()
        else:
            root = base + fwd * 0.03 * s + side * (f - 2.5) * 0.0115 * s
            d = (fwd + side * (f - 2.5) * (0.06 + spread * 0.25)).normalized()
        tip = root + d * mc * s
        bone_shaft(bm, root, tip, 0.0042 * s, 0.0036 * s, 1.4, 1.5, 6)
        ax = d.cross(up).normalized() if abs(d.dot(up)) < 0.95 else side
        for k, ln in enumerate((p1, p2, p3)):
            if ln <= 0:
                continue
            ang = curl * (0.55 + 0.25 * k)
            d = (d * math.cos(ang) - up * math.sin(ang)).normalized()
            up = (up * math.cos(ang) + (d * 0 + up.cross(ax).cross(ax) * 0) * 0 + d * 0).normalized() if False else up
            nt = tip + d * ln * s
            bone_shaft(bm, tip, nt, 0.0034 * s * (1 - 0.15 * k), 0.0028 * s * (1 - 0.18 * k), 1.35, 1.3 if k < 2 else 1.0, 6)
            ellipsoid(bj, tip, d, 0.0042 * s, 0.0038 * s, 6, 3)
            if coat is not None and k < 2:
                sweep(K(M[coat], 40), [tip + d * ln * s * 0.25, tip + d * ln * s * 0.75], 0.0045 * s, 6)
            tip = nt
        # Fingerkuppe: Krallenspitze (Endglied spitz)
        sweep(bm, [tip, tip + d * 0.008 * s], [0.0026 * s, 0.0004 * s], 5)


# --- Cannibal Cuisine ---------------------------------------------------------------------------------------------------
LIVERY = (('lack0', 'hlr', 'SNACKS'), ('lack1', 'hlb', 'FRESH BITES'), ('lack2', 'hlo', 'DELI'))


def _ware(K, M, kind, x, y, z, rng):
    """Eine Ware im Fach (Vorderkante bei x)."""
    k = K
    if kind == 'bar':  # Fleischblock in Schokoriegelpapier, am Ende aufgerissen
        box(k(M['wrapb']), (x - 0.11, y - 0.055, z), (x - 0.02, y + 0.055, z + 0.03))
        box(k(M['wrapg']), (x - 0.06, y - 0.056, z + 0.004), (x - 0.045, y + 0.056, z + 0.031))
        box(k(M['meat'], 20), (x - 0.02, y - 0.05, z + 0.002), (x - 0.002, y + 0.05, z + 0.028))
        for i in range(4):
            box(k(M['fat']), (x - 0.0025, y - 0.045 + i * 0.025, z + 0.006 + (i % 2) * 0.008), (x, y - 0.035 + i * 0.025, z + 0.012 + (i % 2) * 0.008))
        for i in range(5):  # Papierfetzen
            a = rng.uniform(-0.05, 0.05)
            quad(k(M['wrapb']), (x - 0.02, y + a, z + 0.03), (x - 0.006, y + a + 0.008, z + 0.036), (x - 0.006, y + a + 0.016, z + 0.03), (x - 0.02, y + a + 0.01, z + 0.028))
    elif kind == 'chips':  # Tuete mit Knochenmark-Chips
        c = V((x - 0.06, y, z + 0.085))
        bm = k(M['wrapy' if rng.random() < 0.5 else 'wrapr'], 40)
        ellipsoid(bm, c, (0, 0, 1), 0.085, 0.055, 10, 6)
        for i in range(3):
            box(k(M['wrapw']), (x - 0.015, y - 0.03 + i * 0.022, z + 0.09), (x - 0.008, y - 0.022 + i * 0.022, z + 0.11))
        box(k(M['chips'], 20), (x - 0.012, y - 0.03, z + 0.06), (x - 0.006, y + 0.03, z + 0.085))
        box(k(M['wrapw']), (x - 0.10, y - 0.05, z + 0.16), (x - 0.02, y + 0.05, z + 0.17))
    elif kind == 'bottle':  # halber Liter Blut
        bm = k(M['glass'], 40)
        lathe(bm, [(0.0, 0.0), (0.032, 0.0), (0.034, 0.012), (0.034, 0.12), (0.016, 0.16), (0.013, 0.19), (0.0, 0.19)], 12,
              Matrix.Translation((x - 0.05, y, z)))
        lathe(k(M['blood'], 40), [(0.0, 0.003), (0.0335, 0.003), (0.0335, 0.105), (0.0, 0.105)], 12, Matrix.Translation((x - 0.05, y, z)))
        cyl(k(M['wrapw']), (x - 0.05, y, z + 0.19), (x - 0.05, y, z + 0.205), 0.014, 10)
        box(k(M['label']), (x - 0.0175, y - 0.025, z + 0.04), (x - 0.0155, y + 0.025, z + 0.08))
    elif kind == 'can':  # Limo mit Blut, Spritzer am Rand
        cyl(k(M['can'], 40), (x - 0.04, y, z), (x - 0.04, y, z + 0.12), 0.033, 12)
        for i in range(4):
            a = rng.uniform(-0.6, 0.6)
            p = V((x - 0.04 + 0.034 * math.cos(a), y + 0.034 * math.sin(a), z + 0.12))
            sweep(k(M['blood']), [p, p - V((0, 0, rng.uniform(0.02, 0.06)))], [0.004, 0.002], 5)
    elif kind == 'heart':  # ganzes Herz in einer Klarsichtschale
        box(k(M['wrapw']), (x - 0.13, y - 0.07, z), (x - 0.01, y + 0.07, z + 0.012))
        bm = k(M['meat_d'], 40)
        ellipsoid(bm, (x - 0.07, y, z + 0.055), (0.3, 0.1, 1.0), 0.05, 0.04, 10, 6)
        ellipsoid(k(M['fat'], 40), (x - 0.08, y - 0.01, z + 0.095), (0, 0, 1), 0.014, 0.022, 8, 4)
        for d in (-1, 1):
            sweep(k(M['meat'], 40), [(x - 0.07, y + d * 0.015, z + 0.09), (x - 0.075, y + d * 0.025, z + 0.115)], [0.009, 0.008], 6)
        sweep(k(M['blood']), [(x - 0.04, y + 0.03, z + 0.03), (x - 0.03, y + 0.045, z + 0.013)], [0.004, 0.006], 5)
    elif kind == 'teeth':  # Tuete mit etwa zwanzig Zaehnen
        box(k(M['wrapw']), (x - 0.10, y - 0.05, z), (x - 0.02, y + 0.05, z + 0.11))
        for i in range(20):
            p = V((x - 0.02 + 0.002, y + rng.uniform(-0.042, 0.042), z + rng.uniform(0.01, 0.09)))
            ellipsoid(k(M['tooth'], 30), p, (rng.uniform(-1, 1), rng.uniform(-1, 1), 1), 0.007, 0.004, 5, 3)
    elif kind == 'ears':  # Schachtel mit zwei Ohren (Sichtfenster)
        box(k(M['card']), (x - 0.12, y - 0.06, z), (x - 0.02, y + 0.06, z + 0.09))
        box(k(M['dark']), (x - 0.0198, y - 0.045, z + 0.015), (x - 0.0195, y + 0.045, z + 0.075))
        for d in (-1, 1):
            bm = k(M['skin'], 40)
            ellipsoid(bm, (x - 0.019, y + d * 0.022, z + 0.045), (0, 0.2 * d, 1), 0.026, 0.014, 8, 4)
            ellipsoid(k(M['meat'], 30), (x - 0.0185, y + d * 0.02, z + 0.044), (0, 0, 1), 0.012, 0.005, 6, 3)
    elif kind == 'drum':  # Fleisch in Haehnchenkeulen-Form, in Folie
        bm = k(M['foil'], 30)
        ellipsoid(bm, (x - 0.07, y, z + 0.035), (0, 1, 0), 0.05, 0.034, 10, 5)
        cyl(k(M['bone'], 30), (x - 0.07, y + 0.04, z + 0.03), (x - 0.07, y + 0.075, z + 0.03), 0.008, 6)
        ellipsoid(k(M['bone'], 30), (x - 0.07, y + 0.078, z + 0.03), (0, 1, 0), 0.008, 0.011, 6, 3)
        ellipsoid(k(M['meat'], 30), (x - 0.037, y - 0.005, z + 0.04), (1, 0, 0), 0.005, 0.022, 6, 3)
    elif kind == 'skin':  # gezuckerte Hautstreifen im Klarsichtbeutel
        box(k(M['wrapw']), (x - 0.10, y - 0.055, z), (x - 0.02, y + 0.055, z + 0.13))
        for i in range(6):
            yy = y - 0.04 + i * 0.016
            box(k(M['skin']), (x - 0.0195, yy, z + 0.01), (x - 0.0185, yy + 0.011, z + 0.12))
            for j in range(5):
                box(k(M['sugar']), (x - 0.0186, yy + 0.002, z + 0.02 + j * 0.022), (x - 0.0182, yy + 0.005, z + 0.023 + j * 0.022))


def build_cc_body(v=0):
    rng = random.Random(3600 + v)
    K = Kit()
    M = _mats()
    lack, _, _ = LIVERY[v]
    L = M[lack]
    hw, hd = CC_W / 2, CC_D / 2
    bL = K(L, 30)
    # Gehaeuse: Rueckwand, Seiten, Deckel, Sockel
    box(bL, (-hd, -hw, 0.06), (-hd + 0.02, hw, CC_H))
    for s in (-1, 1):
        box(bL, (-hd, s * hw - (0.02 if s > 0 else 0), 0.06), (hd, s * hw + (0.0 if s > 0 else 0.02), CC_H))
    box(bL, (-hd, -hw, CC_H - 0.02), (hd, hw, CC_H))
    box(K(M['rubber']), (-hd + 0.03, -hw + 0.03, 0.0), (hd - 0.03, hw - 0.03, 0.06))
    for s in (-1, 1):  # Stellfuesse
        for xx in (-hd + 0.08, hd - 0.08):
            cyl(K(M['trim']), (xx, s * (hw - 0.08), 0.0), (xx, s * (hw - 0.08), 0.03), 0.03, 10)
    # Front: Rahmen um Fach, Ausgabe, Klappen; rechte Saeule mit Tastenfeldern
    fx = hd
    gy0, gy1, gz0, gz1 = -hw + 0.04, 0.21, 0.58, 1.60
    box(bL, (fx - 0.02, -hw, 0.32), (fx, hw, 0.34))          # Leiste ueber der Sockelklappe
    box(bL, (fx - 0.02, -hw, 0.56), (fx, gy1, gz0))           # unter dem Fach
    box(bL, (fx - 0.02, -hw, gz1), (fx, hw, 1.64))            # ueber dem Fach
    box(bL, (fx - 0.02, -hw, gz0), (fx, gy0, gz1))            # links
    box(bL, (fx - 0.02, gy1, 0.34), (fx, hw, gz1))            # rechte Saeule
    for (a, b) in (((fx, gy0 - 0.01, gz0 - 0.01), (fx + 0.008, gy1 + 0.01, gz0)), ((fx, gy0 - 0.01, gz1), (fx + 0.008, gy1 + 0.01, gz1 + 0.01)),
                   ((fx, gy0 - 0.01, gz0), (fx + 0.008, gy0, gz1)), ((fx, gy1, gz0), (fx + 0.008, gy1 + 0.01, gz1))):
        box(K(M['trim']), a, b)  # Chromrahmen ums Fenster (Scheibe fehlt - nur die Nut)
    box(K(M['dark']), (fx - 0.012, gy0, gz0 - 0.004), (fx - 0.006, gy1, gz0))
    # Ausgabefach unter dem Fenster: dunkle Mulde mit Schwenkklappe "PUSH"
    box(K(M['cavity']), (fx - 0.16, -hw + 0.06, 0.37), (fx - 0.15, 0.17, 0.53))
    box(K(M['dark']), (fx - 0.16, -hw + 0.06, 0.365), (fx, 0.17, 0.37))
    box(K(M['dark']), (fx - 0.16, -hw + 0.06, 0.53), (fx, 0.17, 0.535))
    for yy in (-hw + 0.06, 0.17):
        box(K(M['dark']), (fx - 0.16, yy - 0.003, 0.365), (fx, yy + 0.003, 0.535))
    quad(K(M['dark']), (fx - 0.01, -hw + 0.07, 0.535), (fx - 0.01, 0.16, 0.535), (fx - 0.07, 0.16, 0.44), (fx - 0.07, -hw + 0.07, 0.44))
    text_on(K(M['trim']), 'PUSH', 0.022, (fx - 0.039, -0.2, 0.49), (0, 1, 0), (-0.84, 0, 0.55), 0.002)
    # Hohlraeume hinter den Klappen (sieht man nur, wenn sie offen sind)
    box(K(M['cavity']), (fx - 0.36, -hw + 0.03, 0.08), (fx - 0.34, hw - 0.03, 0.32))
    box(K(M['cavity']), (fx - 0.36, -hw + 0.03, 0.06), (fx, hw - 0.03, 0.08))
    box(K(M['cavity']), (fx - 0.36, -hw + 0.03, 0.315), (fx - 0.02, hw - 0.03, 0.32))
    box(K(M['cavity']), (fx - 0.34, -hw + 0.03, 1.64), (fx - 0.32, hw - 0.03, 1.81))
    box(K(M['cavity']), (fx - 0.34, -hw + 0.03, 1.635), (fx - 0.02, hw - 0.03, 1.64))
    for s in (-1, 1):
        box(K(M['cavity']), (fx - 0.36, s * (hw - 0.03) - 0.005, 0.06), (fx, s * (hw - 0.03) + 0.005, 0.32))
        box(K(M['cavity']), (fx - 0.34, s * (hw - 0.03) - 0.005, 1.635), (fx, s * (hw - 0.03) + 0.005, 1.81))
        for (z0, xx) in ((0.2, fx - 0.18), (1.72, fx - 0.16)):  # Schultergelenke der Arme (Metallpfannen)
            lathe(K(M['coat_d'], 40), [(0.0, -0.03), (0.04, -0.03), (0.045, 0.0), (0.03, 0.02), (0.0, 0.02)], 10,
                  Matrix.Translation((xx, s * 0.36, z0)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
    # unten im Hohlraum: das Gesicht einer der armlosen Leichen (groesserer Kiefer), an der Brust eine Karteikarte
    c = V((fx - 0.27, -0.05, 0.17))
    ellipsoid(K(M['corpse'], 40), c, (0.25, 0.0, 1.0), 0.085, 0.066, 12, 7)
    ellipsoid(K(M['corpse'], 40), c + V((0.035, 0.0, -0.065)), (0.6, 0.0, -0.3), 0.06, 0.05, 10, 5)  # Kiefer
    for d in (-1, 1):
        ellipsoid(K(M['cavity'], 30), c + V((0.058, d * 0.026, 0.012)), (1, 0, 0), 0.012, 0.016, 8, 4)  # leere Augen
        ellipsoid(K(M['corpse_d'], 30), c + V((0.05, d * 0.03, 0.03)), (0, d, 0.2), 0.02, 0.008, 6, 3)
    box(K(M['cavity']), (fx - 0.21, -0.075, 0.098), (fx - 0.2, -0.025, 0.112))  # offener Mund
    for i in range(7):
        box(K(M['tooth']), (fx - 0.2045, -0.072 + i * 0.0068, 0.106), (fx - 0.199, -0.067 + i * 0.0068, 0.113))
    box(K(M['corpse'], 30), (fx - 0.33, -0.25, 0.08), (fx - 0.22, 0.16, 0.30))  # Rumpf (armlos, Stumpf)
    ellipsoid(K(M['meat_d'], 30), (fx - 0.24, 0.155, 0.24), (0, 1, 0), 0.02, 0.03, 8, 4)
    quad(K(M['indexcard']), (fx - 0.218, 0.0, 0.11), (fx - 0.218, 0.12, 0.11), (fx - 0.218, 0.12, 0.185), (fx - 0.218, 0.0, 0.185))
    text_on(K(M['ink']), 'HENRY IRIS', 0.014, (fx - 0.217, 0.06, 0.162), (0, 1, 0), (0, 0, 1))
    text_on(K(M['ink']), 'NR. 1', 0.01, (fx - 0.217, 0.06, 0.135), (0, 1, 0), (0, 0, 1))
    for d in (0.005, 0.115):
        quad(K(M['tape']), (fx - 0.2175, d - 0.01, 0.175), (fx - 0.2175, d + 0.01, 0.175), (fx - 0.2175, d + 0.01, 0.195), (fx - 0.2175, d - 0.01, 0.195))
    # Fach: Rueckwand leuchtet, fuenf Reihen Ware je vier Plaetze, hinter jeder Ware eine Knochenhand
    bx = -0.20
    box(K(M['backlit']), (bx - 0.01, gy0, gz0), (bx, gy1, gz1))
    for s_, yy in ((-1, gy0), (1, gy1)):
        box(K(M['shelf']), (bx, yy - 0.005, gz0), (fx - 0.02, yy + 0.005, gz1))
    box(K(M['shelf']), (bx, gy0, gz1 - 0.005), (fx - 0.02, gy1, gz1))
    cyl(K(M['tube']), (fx - 0.05, gy0 + 0.02, gz1 - 0.025), (fx - 0.05, gy1 - 0.02, gz1 - 0.025), 0.012, 8)
    kinds = ['bar', 'chips', 'bottle', 'can', 'heart', 'teeth', 'ears', 'drum', 'skin']
    rows = 5
    for r in range(rows):
        z = gz0 + 0.005 + r * 0.2
        box(K(M['shelf']), (bx, gy0, z - 0.005), (fx - 0.03, gy1, z))
        for c_ in range(4):
            yy = gy0 + 0.1 + c_ * 0.19
            box(K(M['label']), (fx - 0.035, yy - 0.04, z - 0.03), (fx - 0.03, yy + 0.04, z - 0.006))
            text_on(K(M['ink']), f'{"ABCDE"[rows - 1 - r]}{c_ + 1}', 0.016, (fx - 0.0295, yy, z - 0.018), (0, 1, 0), (0, 0, 1))
            kind = kinds[(r * 4 + c_ * 3 + v) % len(kinds)]
            if kind == 'heart' and r != 2:
                kind = 'bar'  # lebenswichtige Organe nur ein- oder zweimal (Wiki)
            if (r * 7 + c_ * 3 + v) % 11 == 4:
                kind = None   # leer: schon herausgefallen (die Hand ist vorgerueckt)
            xf = fx - 0.06
            if kind:
                _ware(K, M, kind, xf, yy, z, rng)
            hx = (xf - 0.15) if kind else (fx - 0.08)
            bone_hand(K, M, (hx - 0.075, yy, z + 0.03), (1, 0, 0), (0, 1, 0), (0, 0, 1), 0.9, curl=0.15, spread=0.4)
    # rechte Saeule: Anzeige, Ziffernfeld, Buchstabenfeld, statt des Muenzschlitzes ein glattes Blech
    sy0, sy1 = gy1 + 0.04, hw - 0.04
    yc = (sy0 + sy1) / 2
    box(K(M['lcd']), (fx, sy0 + 0.02, 1.42), (fx + 0.006, sy1 - 0.02, 1.50))
    text_on(K(M['lcdglow']), '- -', 0.04, (fx + 0.0065, yc, 1.46), (0, 1, 0), (0, 0, 1))
    keys = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#']
    for i, kk in enumerate(keys):
        yy, zz = sy0 + 0.05 + (i % 3) * 0.075, 1.36 - (i // 3) * 0.06
        box(K(M['key']), (fx, yy - 0.026, zz - 0.02), (fx + 0.012, yy + 0.026, zz + 0.02))
        text_on(K(M['ink']), kk, 0.022, (fx + 0.0122, yy, zz), (0, 1, 0), (0, 0, 1))
    for i, kk in enumerate('ABCDEF'):
        yy, zz = sy0 + 0.085 + (i % 2) * 0.1, 1.06 - (i // 2) * 0.062
        box(K(M['key']), (fx, yy - 0.036, zz - 0.022), (fx + 0.012, yy + 0.036, zz + 0.022))
        text_on(K(M['ink']), kk, 0.024, (fx + 0.0122, yy, zz), (0, 1, 0), (0, 0, 1))
    box(K(M['trim']), (fx, sy0 + 0.03, 0.66), (fx + 0.004, sy1 - 0.03, 0.80))
    text_on(K(M['ink']), 'NO COINS', 0.018, (fx + 0.0042, yc, 0.745), (0, 1, 0), (0, 0, 1))
    text_on(K(M['ink']), 'NO CARDS', 0.018, (fx + 0.0042, yc, 0.715), (0, 1, 0), (0, 0, 1))
    box(K(M['dark']), (fx, sy0 + 0.05, 0.42), (fx + 0.004, sy1 - 0.05, 0.56))  # Rueckgabe (zugeklebt)
    # Herstellerschild auf der Rueckseite: Hersteller, Familie, Jahr und Ort, ein Auge (Iris) als Zeichen
    o = V((-hd - 0.004, 0.0, 1.22))
    U, W = V((0, -1, 0)), V((0, 0, 1))
    box(K(M['plate']), (-hd - 0.004, -0.27, 1.04), (-hd, 0.27, 1.40))
    for d in (-1, 1):
        for zz in (1.06, 1.38):
            lathe(K(M['trim']), [(0.006, 0.0), (0.004, 0.003), (0.0, 0.0035)], 8,
                  Matrix.Translation((-hd - 0.004, d * 0.25, zz)) @ Matrix.Rotation(-math.pi / 2, 4, 'Y'))
    text_on(K(M['ink']), 'CANNIBAL CUISINE', 0.04, o + W * 0.12, U, W, 0.0015)
    text_on(K(M['ink']), 'PRODUCTIONS', 0.03, o + W * 0.07, U, W, 0.0015)
    text_on(K(M['ink']), 'IRIS FAMILY', 0.032, o + W * -0.02, U, W, 0.0015)
    text_on(K(M['ink']), 'ARIZONA  2019', 0.022, o + W * -0.075, U, W, 0.0015)
    text_on(K(M['ink']), 'SER. 36-0417-IR', 0.014, o + W * -0.135, U, W, 0.0012)
    eye = o + V((-0.0016, 0, 0)) + W * 0.025
    poly_on(K(M['ink']), [(0.22 + 0.04 * math.cos(a), 0.02 * math.sin(a)) for a in [i * TAU / 16 for i in range(16)]], eye, U, W)
    poly_on(K(M['plate']), [(0.22 + 0.016 * math.cos(a), 0.016 * math.sin(a)) for a in [i * TAU / 12 for i in range(12)]], eye - V((0.0004, 0, 0)), U, W)
    poly_on(K(M['ink']), [(0.22 + 0.007 * math.cos(a), 0.007 * math.sin(a)) for a in [i * TAU / 10 for i in range(10)]], eye - V((0.0008, 0, 0)), U, W)
    # Gebrauchsspuren: Kratzer und Schmutz unten, ein Blutfaden aus dem Ausgabefach
    for i in range(10):
        y0 = rng.uniform(-hw + 0.05, hw - 0.05)
        z0 = rng.uniform(0.35, 0.6)
        poly_on(K(M['dark']), L1.stroke([(y0, z0), (y0 + rng.uniform(-0.03, 0.03), z0 + rng.uniform(0.01, 0.05))], 0.0016), V((fx + 0.0005, 0, 0)), (0, 1, 0), (0, 0, 1))
    sweep(K(M['blood']), [(fx - 0.004, -0.2, 0.37), (fx + 0.001, -0.205, 0.33), (fx + 0.002, -0.21, 0.30)], [0.004, 0.0035, 0.002], 5)
    K.finish()


def build_cc_body__v1():
    build_cc_body(1)


def build_cc_body__v2():
    build_cc_body(2)


def build_cc_flap_top(v=0):
    """Kopfklappe: leuchtendes Kopfschild mit Werbeschrift; Ursprung = Gelenk (Oberkante, Vorderseite)."""
    K = Kit()
    M = _mats()
    lack, glow, word = LIVERY[v]
    hw = CC_W / 2
    # lokales System: Gelenk bei (0, 0, 0); die Klappe haengt nach unten (z < 0), Vorderseite bei x = +0.02
    h = CC_FLAP_TOP[1] - 1.64
    box(K(M[lack]), (-0.0, -hw, -h), (0.02, hw, 0.0))
    box(K(M[glow]), (0.02, -hw + 0.03, -h + 0.02), (0.026, hw - 0.03, -0.02))
    text_on(K(M['hlk']), word, 0.085, (0.0262, 0.0, -h / 2), (0, 1, 0), (0, 0, 1), 0.003)
    box(K(M['trim']), (0.02, -hw, -h), (0.03, hw, -h + 0.012))
    box(K(M['trim']), (0.02, -hw, -0.012), (0.03, hw, 0.0))
    for d in (-1, 1):  # Scharniere
        cyl(K(M['trim']), (0.01, d * (hw - 0.15), 0.0), (0.01, d * (hw - 0.05), 0.0), 0.012, 8)
    K.finish()


def build_cc_flap_top__v1():
    build_cc_flap_top(1)


def build_cc_flap_top__v2():
    build_cc_flap_top(2)


def build_cc_flap_bot():
    """Sockelklappe mit Lueftungsschlitzen; Ursprung = Gelenk (Unterkante, Vorderseite), Klappe steht nach oben."""
    K = Kit()
    M = _mats()
    hw = CC_W / 2
    h = 0.32 - CC_FLAP_BOT[1]
    box(K(M['dark']), (0.0, -hw + 0.01, 0.0), (0.02, hw - 0.01, h))
    for i in range(9):
        z = 0.03 + i * 0.022
        box(K(M['cavity']), (0.02, -hw + 0.12, z), (0.0205, hw - 0.12, z + 0.008))
    box(K(M['trim']), (0.02, -hw + 0.01, h - 0.01), (0.026, hw - 0.01, h))
    for d in (-1, 1):
        cyl(K(M['trim']), (0.01, d * (hw - 0.15), 0.0), (0.01, d * (hw - 0.05), 0.0), 0.012, 8)
    K.finish()


def _coated_bone(K, M, a, b, r0, r1, rng, worn=0.25):
    """Knochen mit einem unbekannten metallischen Ueberzug, an den Gelenken abgeplatzt (gelber Knochen darunter)."""
    a, b = V(a), V(b)
    bone_shaft(K(M['bone'], 30), a, b, r0 * 0.92, r1 * 0.92, 1.75, 1.6, 9)
    d = (b - a)
    ln = d.length
    t0, t1 = 0.12 + rng.uniform(0, worn * 0.3), 0.88 - rng.uniform(0, worn * 0.3)
    pts = [a + d * (t0 + (t1 - t0) * i / 6) for i in range(7)]
    rs = [(r0 + (r1 - r0) * (t0 + (t1 - t0) * i / 6)) * (1.02 + 0.06 * math.sin(i * 2.3)) for i in range(7)]
    sweep(K(M['coat'], 35), pts, rs, 9)
    # Naehte und Wuelste des Ueberzugs (wie geschweisst), Drahtbuendel
    for t in (0.3, 0.55, 0.78):
        p = a + d * t
        r = (r0 + (r1 - r0) * t) * 1.12
        sweep(K(M['coat_d'], 35), [p - d.normalized() * 0.006, p + d.normalized() * 0.006], r, 9)
    side = d.cross(V((0, 0, 1))).normalized() if abs(d.normalized().z) < 0.9 else V((0, 1, 0))
    sweep(K(M['coat_d'], 35), [a + d * 0.15 + side * r0 * 1.05, a + d * 0.5 + side * r0 * 1.25, a + d * 0.85 + side * r1 * 1.05], 0.0035, 5)
    # Fetzen getrockneten Gewebes am Gelenk
    for _ in range(2):
        p = a + d * rng.uniform(0.02, 0.1) + side * rng.uniform(-r0, r0)
        ellipsoid(K(M['tissue'], 30), p, d, ln * 0.03, r0 * 0.5, 6, 3)


def build_cc_humerus():
    rng = random.Random(3611)
    K = Kit()
    M = _mats()
    L = CC_ARM_LEN[0]
    _coated_bone(K, M, (0, 0, 0), (L, 0, 0), 0.016, 0.014, rng)
    ellipsoid(K(M['bone'], 30), (0.0, 0.0, 0.0), (1, 0, 0), 0.03, 0.028, 10, 5)  # Gelenkkopf
    for d in (-1, 1):
        ellipsoid(K(M['bone_d'], 30), (L, d * 0.016, 0.0), (0, 1, 0), 0.016, 0.018, 8, 4)  # Kondylen
    K.finish()


def build_cc_forearm():
    rng = random.Random(3612)
    K = Kit()
    M = _mats()
    L = CC_ARM_LEN[1]
    _coated_bone(K, M, (0.0, 0.012, 0.0), (L, 0.014, 0.0), 0.011, 0.012, rng)   # Elle
    _coated_bone(K, M, (0.01, -0.012, 0.004), (L, -0.018, -0.002), 0.009, 0.013, rng)  # Speiche
    ellipsoid(K(M['bone'], 30), (-0.01, 0.012, 0.0), (1, 0, 0), 0.02, 0.016, 8, 4)  # Ellenhaken
    K.finish()


def build_cc_hand(v=0):
    K = Kit()
    M = _mats()
    bone_hand(K, M, (0.0, 0.0, 0.0), (1, 0, 0), (0, 1, 0), (0, 0, 1), scale=2.25, curl=0.75 if v == 0 else 0.12,
              spread=0.15 if v == 0 else 0.9, mat_b='bone', mat_j='coat_d', coat='coat')
    K.finish()


def build_cc_hand__v1():
    build_cc_hand(1)


def build_cc_ware():
    """Fundstueck: Fleischblock in Schokoriegelpapier, ein Ende aufgerissen (Ursprung Mitte unten, 12 cm lang)."""
    rng = random.Random(3620)
    K = Kit()
    M = _mats()
    _ware(K, M, 'bar', 0.065, 0.0, 0.0, rng)
    text_on(K(M['wrapg']), 'CC', 0.02, (-0.04, 0.0, 0.0302), (1, 0, 0), (0, 1, 0), 0.001)
    K.finish()


# --- Swindlebird ---------------------------------------------------------------------------------------------------------
def _sb_mats():
    return {
        'blue': mat('sb_feder_blau', (0.06, 0.20, 0.72), 'plastic', r=0.55),
        'blue_d': mat('sb_feder_tief', (0.03, 0.08, 0.34), 'plastic', r=0.6),
        'blue_l': mat('sb_feder_hell', (0.24, 0.48, 0.92), 'plastic', r=0.5),
        'under': mat('sb_feder_unten', (0.10, 0.11, 0.16), 'fabric', r=1.1),
        'coat': mat('sb_belag', (0.62, 0.66, 0.95), 'plastic', r=0.12),
        'beak': mat('sb_schnabel', (0.10, 0.10, 0.11), 'plastic', r=0.35),
        'beak_l': mat('sb_schnabel_rand', (0.36, 0.34, 0.30), 'plastic', r=0.5),
        'ring': mat('sb_augenring', (0.86, 0.84, 0.74), 'plastic', r=0.8),
        'eye': mat('sb_auge', (0.02, 0.02, 0.02), 'glass', r=0.1),
        'iris': mat('sb_iris', (0.85, 0.72, 0.30), 'plastic', r=0.4),
        'leg': mat('sb_lauf', (0.42, 0.40, 0.44), 'plastic', r=0.9),
        'leg_d': mat('sb_lauf_schuppe', (0.22, 0.21, 0.24), 'plastic', r=1.0),
        'claw': mat('sb_kralle', (0.05, 0.05, 0.05), 'plastic', r=0.4),
        'tongue': mat('sb_zunge', (0.12, 0.10, 0.12), 'plastic', r=0.4),
    }


def _feather(bm, root, d, side, ln, w, curl=0.0, segs=4):
    """Eine Feder als gewoelbtes, spitz zulaufendes Blatt (zweiseitig sichtbar ueber zwei Flaechen)."""
    root, d, side = V(root), V(d).normalized(), V(side).normalized()
    n = d.cross(side).normalized()
    left, right = [], []
    for i in range(segs + 1):
        t = i / segs
        ww = w * (math.sin(math.pi * min(1.0, t * 1.15)) * 0.8 + 0.2) * (1.0 - t * 0.35)
        c = root + d * ln * t + n * (curl * ln * t * t)
        left.append(c + side * ww / 2 + n * 0.0012)
        right.append(c - side * ww / 2 + n * 0.0012)
    for i in range(segs):
        bm.faces.new([bm.verts.new(p) for p in (left[i], left[i + 1], right[i + 1], right[i])])
        bm.faces.new([bm.verts.new(p - n * 0.0024) for p in (right[i], right[i + 1], left[i + 1], left[i])])


def build_sb_body():
    """Koerper (Ursprung Huefte): plump wie eine Taube, Federschuppen, nass glaenzender Belag mit Perlen."""
    rng = random.Random(1920)
    K = Kit()
    M = _sb_mats()
    c = V((0.015, 0.0, 0.035))
    ellipsoid(K(M['blue_d'], 50), c, (1, 0, 0.18), 0.135, 0.068, 16, 10)
    ellipsoid(K(M['blue'], 50), c + V((0.045, 0.0, -0.005)), (0.4, 0, 1), 0.07, 0.062, 14, 8)  # Brust
    # Federschuppen in Reihen ueber Ruecken, Flanken und Brust
    for row in range(9):
        t = row / 8
        x = 0.11 - t * 0.21
        rr = 0.068 * math.sqrt(max(0.05, 1.0 - ((x - 0.015) / 0.14) ** 2))
        for k in range(14):
            a = -math.pi * 0.95 + TAU * 0.95 * k / 13 + (row % 2) * 0.12
            nrm = V((0.0, math.cos(a), math.sin(a)))
            p = V((x, 0.0, 0.035)) + nrm * rr * 1.01
            mat_ = 'blue_l' if (nrm.z > 0.4 and rng.random() < 0.35) else ('blue' if nrm.z > -0.3 else 'blue_d')
            _feather(K(M[mat_], 40), p, V((-1.0, 0.0, 0.0)) + nrm * 0.25, nrm.cross(V((1, 0, 0))), 0.035 + 0.01 * rng.random(), 0.022, curl=-0.4)
    # der Belag: nasse, glaenzende Perlen und Laeufe (das halluzinogene Mittel der aeusseren Federn)
    for _ in range(26):
        a = rng.uniform(-math.pi, math.pi * 0.2)
        x = rng.uniform(-0.09, 0.1)
        rr = 0.07 * math.sqrt(max(0.05, 1.0 - ((x - 0.015) / 0.14) ** 2))
        p = V((x, rr * math.cos(a) * 1.05, 0.035 + rr * math.sin(a) * 1.05))
        ellipsoid(K(M['coat'], 40), p, (0, 0, 1), rng.uniform(0.002, 0.0045), rng.uniform(0.0025, 0.004), 6, 3)
        if rng.random() < 0.4:
            sweep(K(M['coat'], 40), [p, p - V((rng.uniform(0, 0.01), 0, rng.uniform(0.008, 0.02)))], [0.002, 0.0008], 5)
    K.finish()


def build_sb_head():
    """Kopf (Ursprung Halsansatz): Hals mit Krause, Papageienkopf, Hakenschnabel, nackter heller Augenring."""
    rng = random.Random(1921)
    K = Kit()
    M = _sb_mats()
    sweep(K(M['blue'], 40), [(0, 0, 0), (0.02, 0, 0.03), (0.03, 0, 0.055)], [0.036, 0.03, 0.03], 12)
    hc = V((0.04, 0.0, 0.075))
    ellipsoid(K(M['blue'], 50), hc, (1, 0, 0.3), 0.042, 0.036, 14, 8)
    for k in range(18):  # Halskrause, aufgestellt
        a = TAU * k / 18
        nrm = V((0.0, math.cos(a), math.sin(a)))
        _feather(K(M['blue_l' if k % 3 == 0 else 'blue'], 40), V((0.01, 0, 0.025)) + nrm * 0.03, nrm * 0.8 + V((-0.4, 0, 0.2)),
                 nrm.cross(V((1, 0, 0))), 0.03, 0.016, curl=0.3)
    for k in range(7):  # Schopf (kurz, steif)
        _feather(K(M['blue_d'], 40), hc + V((-0.01 - k * 0.006, 0, 0.032)), V((-0.6, 0, 1.0)), V((0, 1, 0)), 0.03 + 0.004 * k, 0.012, curl=-0.5)
    # Schnabel: Oberschnabel als Haken, Unterschnabel kurz, dazwischen eine dunkle Zunge
    up = [V((0.065, 0, 0.09)), V((0.09, 0, 0.085)), V((0.105, 0, 0.068)), V((0.104, 0, 0.048)), V((0.095, 0, 0.04))]
    sweep(K(M['beak'], 40), up, [0.019, 0.016, 0.012, 0.007, 0.002], 10)
    sweep(K(M['beak'], 40), [(0.066, 0, 0.062), (0.085, 0, 0.055), (0.094, 0, 0.052)], [0.014, 0.01, 0.004], 9)
    ellipsoid(K(M['tongue'], 40), (0.083, 0, 0.062), (1, 0, 0), 0.01, 0.006, 6, 3)
    sweep(K(M['beak_l'], 40), [(0.063, 0, 0.098), (0.07, 0, 0.1)], 0.012, 8)  # Wachshaut
    for d in (-1, 1):
        e = hc + V((0.016, d * 0.03, 0.008))
        lathe(K(M['ring'], 40), [(0.0, 0.0), (0.012, 0.0), (0.0125, 0.002), (0.006, 0.004), (0.0, 0.004)], 12,
              Matrix.Translation(e) @ Matrix.Rotation(-d * math.pi / 2, 4, 'X'))
        ellipsoid(K(M['iris'], 40), e + V((0, d * 0.0035, 0)), (0, d, 0), 0.003, 0.0058, 10, 5)
        ellipsoid(K(M['eye'], 40), e + V((0, d * 0.006, 0)), (0, d, 0), 0.0022, 0.0034, 8, 4)
        for i in range(5):  # Federchen um den Ring
            a = TAU * i / 5 + rng.uniform(-0.2, 0.2)
            _feather(K(M['blue_d'], 40), e + V((0.014 * math.cos(a), d * 0.0, 0.014 * math.sin(a))), V((math.cos(a), 0, math.sin(a))),
                     V((0, 1, 0)), 0.008, 0.005)
    for _ in range(9):
        p = hc + V((rng.uniform(-0.03, 0.02), rng.uniform(-0.034, 0.034), rng.uniform(0.0, 0.035)))
        ellipsoid(K(M['coat'], 40), p, (0, 0, 1), 0.0022, 0.0028, 6, 3)
    K.finish()


def build_sb_wing_in(v=0):
    """Innerer Fluegel (Ursprung Schulter, angelegt: entlang -x), Armschwingen haengen nach hinten."""
    rng = random.Random(1922 + v)
    K = Kit()
    M = _sb_mats()
    s = 1 if v == 0 else -1
    L = SB_WRIST
    sweep(K(M['blue_d'], 40), [(0, 0, 0), (-L * 0.5, s * 0.008, -0.004), (-L, s * 0.01, -0.006)], [0.016, 0.013, 0.01], 8)
    for i in range(9):  # Armschwingen
        t = i / 8
        root = V((-L * t, s * 0.012, -0.006))
        _feather(K(M['blue' if i % 2 else 'blue_l'], 40), root, V((-0.75, s * 0.12, -0.55)), V((1, 0, 0)), 0.075 + 0.01 * t, 0.022, curl=0.1)
        _feather(K(M['under'], 40), root - V((0, s * 0.003, 0)), V((-0.75, s * 0.05, -0.55)), V((1, 0, 0)), 0.06, 0.018)
    for i in range(6):  # Deckfedern
        root = V((-L * i / 5, s * 0.016, 0.004))
        _feather(K(M['blue'], 40), root, V((-1.0, s * 0.1, -0.2)), V((0, 0, 1)), 0.035, 0.02, curl=-0.3)
        if rng.random() < 0.5:
            ellipsoid(K(M['coat'], 40), root + V((-0.015, s * 0.004, -0.004)), (0, 0, 1), 0.002, 0.003, 6, 3)
    K.finish()


def build_sb_wing_in__v1():
    build_sb_wing_in(1)


def build_sb_wing_out(v=0):
    """Aeusserer Fluegel (Ursprung Handgelenk): lange Handschwingen entlang -x, ueber den Schwanz hinaus."""
    K = Kit()
    M = _sb_mats()
    s = 1 if v == 0 else -1
    sweep(K(M['blue_d'], 40), [(0, 0, 0), (-0.05, s * 0.004, -0.002)], [0.009, 0.006], 8)
    for i in range(10):
        t = i / 9
        root = V((-0.05 * t, s * 0.006, -0.002))
        ln = 0.13 - 0.045 * abs(t - 0.6)
        _feather(K(M['blue_d' if i > 6 else 'blue'], 40), root, V((-1.0, s * (0.05 + 0.04 * t), -0.12 - 0.15 * (1 - t))), V((0.05, 0, 1)), ln, 0.02, curl=0.05)
    K.finish()


def build_sb_wing_out__v1():
    build_sb_wing_out(1)


def build_sb_tail():
    K = Kit()
    M = _sb_mats()
    for i in range(10):
        a = (i - 4.5) * 0.09
        _feather(K(M['blue_d' if i in (0, 9) else 'blue'], 40), (0, 0, 0), (-math.cos(a), math.sin(a), -0.08), (math.sin(a), math.cos(a), 0), 0.13, 0.026, curl=0.05)
    _feather(K(M['coat'], 40), (-0.02, 0.0, 0.002), (-1, 0, -0.06), (0, 1, 0), 0.08, 0.008)
    K.finish()


def _scaly(K, M, a, b, r0, r1):
    a, b = V(a), V(b)
    sweep(K(M['leg'], 30), [a, b], [r0, r1], 7)
    for i in range(1, 6):
        p = a.lerp(b, i / 6)
        sweep(K(M['leg_d'], 30), [p - (b - a).normalized() * 0.002, p + (b - a).normalized() * 0.002], (r0 + (r1 - r0) * i / 6) * 1.08, 7)


def build_sb_shin():
    K = Kit()
    M = _sb_mats()
    sweep(K(M['blue_d'], 40), [(0, 0, 0), (SB_LEG[0] * 0.45, 0, 0)], [0.016, 0.009], 8)  # Hosenfedern
    _scaly(K, M, (0, 0, 0), (SB_LEG[0], 0, 0), 0.006, 0.0045)
    ellipsoid(K(M['leg_d'], 30), (SB_LEG[0], 0, 0), (1, 0, 0), 0.0055, 0.0055, 7, 4)
    K.finish()


def build_sb_tars():
    K = Kit()
    M = _sb_mats()
    _scaly(K, M, (0, 0, 0), (SB_LEG[1], 0, 0), 0.0045, 0.004)
    ellipsoid(K(M['leg_d'], 30), (SB_LEG[1], 0, 0), (1, 0, 0), 0.005, 0.005, 7, 4)
    K.finish()


def build_sb_foot():
    """Letztes Laufglied und Fuss (Ursprung drittes Beingelenk; Lauf entlang +x, die Zehen in der Ebene x = Ende)."""
    K = Kit()
    M = _sb_mats()
    L = SB_LEG[2]
    _scaly(K, M, (0, 0, 0), (L, 0, 0), 0.004, 0.0038)
    # Zehen: zwei nach vorn, zwei nach hinten (Papageienfuss); "vorn" = lokale -z (im Spiel dreht der Lauf senkrecht)
    for (dy, dz, ln) in ((0.25, -1.0, 0.032), (-0.25, -1.0, 0.03), (0.35, 1.0, 0.024), (-0.35, 1.0, 0.022)):
        d = V((0.12, dy, dz)).normalized()
        p0 = V((L, 0, 0))
        p1 = p0 + d * ln
        sweep(K(M['leg'], 30), [p0, p0.lerp(p1, 0.5) + V((0.004, 0, 0)), p1], [0.0035, 0.003, 0.0026], 6)
        sweep(K(M['claw'], 30), [p1, p1 + d * 0.007 + V((0.006, 0, 0))], [0.0024, 0.0003], 5)
    K.finish()


# --- Photoshop -----------------------------------------------------------------------------------------------------------
def _ps_mats():
    return {
        'block': mat('ps_klotz', (0.28, 0.12, 0.11), 'plastic', r=1.0),
        'paper': mat('ps_fotorand', (0.94, 0.93, 0.90), 'plastic', r=0.6),
        'flesh': mat('ps_fleisch', (0.66, 0.20, 0.20), 'plastic', r=0.3),
        'flesh_d': mat('ps_fleisch_dunkel', (0.36, 0.06, 0.07), 'plastic', r=0.35),
        'flesh_l': mat('ps_fleisch_hell', (0.88, 0.46, 0.44), 'plastic', r=0.25),
        'fat': mat('ps_fett', (0.92, 0.84, 0.66), 'plastic', r=0.35),
        'vein': mat('ps_ader', (0.28, 0.12, 0.30), 'plastic', r=0.4),
        'shine': mat('ps_glanz', (0.98, 0.86, 0.84), 'plastic', r=0.15),
        'skin': mat('ps_haut', (0.86, 0.68, 0.58), 'plastic', r=0.5),
        'skin_d': mat('ps_haut_falte', (0.55, 0.38, 0.32), 'plastic', r=0.55),
        'sclera': mat('ps_augapfel', (0.95, 0.92, 0.88), 'plastic', r=0.25),
        'redvein': mat('ps_aederchen', (0.75, 0.15, 0.14), 'plastic', r=0.3),
        'iris_a': mat('ps_iris_gruen', (0.30, 0.42, 0.20), 'plastic', r=0.3),
        'iris_b': mat('ps_iris_braun', (0.36, 0.22, 0.10), 'plastic', r=0.3),
        'iris_r': mat('ps_iris_rand', (0.10, 0.10, 0.08), 'plastic', r=0.3),
        'pupil': mat('ps_pupille', (0.01, 0.01, 0.01), 'plastic', r=0.2),
        'catch': mat('ps_lichtpunkt', (1.0, 1.0, 1.0), 'plastic', r=0.1),
        'lash': mat('ps_wimper', (0.05, 0.035, 0.03), 'plastic', r=0.9),
        'lid': mat('ps_lidrand', (0.85, 0.45, 0.42), 'plastic', r=0.3),
        'fur': mat('ps_fell', (0.62, 0.48, 0.34), 'plastic', r=1.1),
        'fur_d': mat('ps_fell_dunkel', (0.30, 0.22, 0.16), 'plastic', r=1.1),
        'fur_w': mat('ps_fell_weiss', (0.86, 0.84, 0.80), 'plastic', r=1.1),
        'lip': mat('ps_lefze', (0.05, 0.04, 0.04), 'plastic', r=0.5),
        'gum': mat('ps_zahnfleisch', (0.80, 0.40, 0.44), 'plastic', r=0.3),
        'gum_d': mat('ps_zahnfleisch_dunkel', (0.25, 0.12, 0.14), 'plastic', r=0.4),
        'tooth': mat('ps_zahn', (0.95, 0.92, 0.82), 'plastic', r=0.3),
        'tongue': mat('ps_zunge', (0.86, 0.42, 0.48), 'plastic', r=0.25),
        'throat': mat('ps_schlund', (0.12, 0.02, 0.03), 'plastic', r=0.5),
        'nose': mat('ps_nase', (0.04, 0.035, 0.035), 'plastic', r=0.3),
        'drool': mat('ps_geifer', (0.92, 0.90, 0.88), 'plastic', r=0.1),
        'armskin': mat('ps_armhaut', (0.90, 0.89, 0.86), 'plastic', r=0.75),
        'armskin_d': mat('ps_armhaut_falte', (0.70, 0.68, 0.66), 'plastic', r=0.8),
        'blood': mat('ps_blut', (0.40, 0.02, 0.02), 'plastic', r=0.2),
        'blood_d': mat('ps_blut_alt', (0.16, 0.01, 0.01), 'plastic', r=0.35),
        'nail': mat('ps_nagel', (0.80, 0.76, 0.70), 'plastic', r=0.35),
        'edge': mat('ps_kante', (0.01, 0.01, 0.01), 'plastic', r=1.3),
        'handle': mat('ps_anfasser', (0.96, 0.96, 0.96), 'plastic', r=0.9),
        'sg_red': mat('ps_schirm_rot', (1.0, 0.16, 0.12), 'glow', glow=1.4),
        'sg_pink': mat('ps_schirm_rosa', (1.0, 0.55, 0.5), 'glow', glow=1.2),
        'sg_dark': mat('ps_schirm_dunkel', (0.35, 0.02, 0.03), 'glow', glow=0.6),
        'sg_white': mat('ps_schirm_weiss', (1.0, 0.95, 0.92), 'glow', glow=1.6),
        'sg_black': mat('ps_schirm_schwarz', (0.02, 0.0, 0.0), 'plastic', r=0.6),
    }


def _flesh_photo(K, M, o, U, W, w, h, rng, border=0.018):
    """Fleischfoto (Rand aus Fotopapier): marmoriertes Muskelfleisch, Fettadern, feuchte Lichter, Aederchen."""
    n = V(U).cross(V(W)).normalized()
    poly_on(K(M['paper']), [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)], o, U, W)
    o1 = o + n * 0.0008
    iw, ih = w / 2 - border, h / 2 - border
    poly_on(K(M['flesh']), [(-iw, -ih), (iw, -ih), (iw, ih), (-iw, ih)], o1, U, W)
    o2 = o + n * 0.0016

    def inside(pts):
        return [(max(-iw, min(iw, a)), max(-ih, min(ih, b))) for (a, b) in pts]
    for _ in range(int(w * h * 70) + 6):
        cx, cy = rng.uniform(-iw, iw), rng.uniform(-ih, ih)
        poly_on(K(M['flesh_d']), inside(blob2(rng, cx, cy, rng.uniform(0.03, 0.12) * w, rng.uniform(0.02, 0.06) * h, 12, 0.45)), o2, U, W)
    o3 = o + n * 0.0024
    for _ in range(int(w * h * 55) + 5):  # Fettadern: lange, gewundene Striche
        cx, cy = rng.uniform(-iw, iw), rng.uniform(-ih, ih)
        pts = [(cx, cy)]
        a = rng.uniform(0, TAU)
        for k in range(rng.randint(3, 7)):
            a += rng.uniform(-0.7, 0.7)
            cx, cy = cx + math.cos(a) * 0.04 * w, cy + math.sin(a) * 0.03 * h
            pts.append((max(-iw, min(iw, cx)), max(-ih, min(ih, cy))))
        poly_on(K(M['fat']), L1.stroke(pts, rng.uniform(0.004, 0.012)), o3, U, W)
    o4 = o + n * 0.0032
    for _ in range(int(w * h * 30) + 3):
        cx, cy = rng.uniform(-iw, iw), rng.uniform(-ih, ih)
        poly_on(K(M['vein']), L1.stroke([(cx, cy), (cx + rng.uniform(-0.05, 0.05), cy + rng.uniform(-0.05, 0.05)),
                                         (cx + rng.uniform(-0.08, 0.08), cy + rng.uniform(-0.08, 0.08))], 0.0025), o4, U, W)
        cx, cy = rng.uniform(-iw, iw), rng.uniform(-ih, ih)
        poly_on(K(M['shine' if rng.random() < 0.6 else 'flesh_l']), inside(blob2(rng, cx, cy, 0.012 + 0.02 * rng.random(), 0.006 + 0.01 * rng.random(), 9, 0.3)), o4, U, W)


def _eye_photo(K, M, o, U, W, w, h, rng, iris='iris_a', look=(0.0, 0.0)):
    """Augenfoto: Haut, Lidspalte, Augapfel mit Aederchen, Iris mit Strahlen, Pupille, Lichtpunkt, Wimpern, Braue."""
    n = V(U).cross(V(W)).normalized()
    d = lambda k: o + n * (0.0008 * k)
    poly_on(K(M['paper']), [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)], d(0), U, W)
    iw, ih = w / 2 - 0.012, h / 2 - 0.012
    poly_on(K(M['skin']), [(-iw, -ih), (iw, -ih), (iw, ih), (-iw, ih)], d(1), U, W)
    for i in range(5):
        y = -ih + ih * 0.3 + i * ih * 0.09
        poly_on(K(M['skin_d']), L1.stroke([(-iw * 0.85, y), (0, y - 0.006), (iw * 0.85, y)], 0.0018), d(2), U, W)
    poly_on(K(M['skin_d']), L1.stroke([(-iw * 0.8, ih * 0.52), (0, ih * 0.66), (iw * 0.8, ih * 0.45)], 0.003), d(2), U, W)  # Lidfalte
    poly_on(K(M['lash']), L1.stroke([(-iw * 0.9, ih * 0.85), (-iw * 0.2, ih * 0.98), (iw * 0.7, ih * 0.9)], 0.012), d(2), U, W)  # Braue
    ew, eh = iw * 0.78, ih * 0.42
    lid = [(ew * math.cos(a), eh * math.sin(a) * (1.0 if math.sin(a) > 0 else 0.75)) for a in [k * TAU / 28 for k in range(28)]]
    poly_on(K(M['lid']), [(x * 1.05, y * 1.1) for (x, y) in lid], d(3), U, W)
    poly_on(K(M['sclera']), lid, d(4), U, W)
    for _ in range(9):
        a = rng.uniform(0, TAU)
        sx, sy = math.cos(a) * ew * 0.95, math.sin(a) * eh * 0.7
        poly_on(K(M['redvein']), L1.stroke([(sx, sy), (sx * 0.75 + rng.uniform(-0.004, 0.004), sy * 0.7), (sx * 0.5, sy * 0.45 + rng.uniform(-0.004, 0.004))], 0.0012), d(5), U, W)
    ic = (look[0] * ew * 0.3, look[1] * eh * 0.2)
    ir = eh * 0.95
    clip = lambda pts: [(x, max(-eh * 0.75, min(eh * 0.98, y))) for (x, y) in pts]
    poly_on(K(M['iris_r']), clip([(ic[0] + ir * math.cos(a), ic[1] + ir * math.sin(a)) for a in [k * TAU / 24 for k in range(24)]]), d(6), U, W)
    poly_on(K(M[iris]), clip([(ic[0] + ir * 0.88 * math.cos(a), ic[1] + ir * 0.88 * math.sin(a)) for a in [k * TAU / 24 for k in range(24)]]), d(7), U, W)
    for k in range(22):
        a = k * TAU / 22 + rng.uniform(-0.08, 0.08)
        p0, p1 = (ic[0] + ir * 0.35 * math.cos(a), ic[1] + ir * 0.35 * math.sin(a)), (ic[0] + ir * 0.85 * math.cos(a), ic[1] + ir * 0.85 * math.sin(a))
        if -eh * 0.75 < p1[1] < eh * 0.98:
            poly_on(K(M['iris_b' if iris == 'iris_a' else 'iris_r']), L1.stroke([p0, p1], 0.0016), d(8), U, W)
    poly_on(K(M['pupil']), [(ic[0] + ir * 0.34 * math.cos(a), ic[1] + ir * 0.34 * math.sin(a)) for a in [k * TAU / 16 for k in range(16)]], d(9), U, W)
    poly_on(K(M['catch']), [(ic[0] + ir * 0.3 + 0.006 * math.cos(a), ic[1] + ir * 0.3 + 0.005 * math.sin(a)) for a in [k * TAU / 8 for k in range(8)]], d(10), U, W)
    poly_on(K(M['skin']), [(-ew * 1.2, eh * 0.98), (ew * 1.2, eh * 0.98), (ew * 1.2, ih), (-ew * 1.2, ih)], d(5), U, W)  # Oberlid deckt
    for k in range(16):  # Wimpern oben
        t = (k + 0.5) / 16
        x = -ew + 2 * ew * t
        y = eh * math.sqrt(max(0.0, 1.0 - (x / ew) ** 2)) * 0.98
        poly_on(K(M['lash']), L1.stroke([(x, y), (x * 1.08, y + 0.012 + 0.004 * math.sin(k)), (x * 1.15, y + 0.017)], 0.0013), d(11), U, W)


def _mouth_photo(K, M, o, U, W, w, h, rng):
    """Foto eines weit aufgerissenen Pitbull-Mauls: Fell, schwarze Lefzen, Zahnfleisch, Eck- und Schneidezaehne, Zunge."""
    n = V(U).cross(V(W)).normalized()
    d = lambda k: o + n * (0.0008 * k)
    poly_on(K(M['paper']), [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)], d(0), U, W)
    iw, ih = w / 2 - 0.016, h / 2 - 0.016
    poly_on(K(M['fur']), [(-iw, -ih), (iw, -ih), (iw, ih), (-iw, ih)], d(1), U, W)
    for _ in range(140):  # Fellstriche
        x, y = rng.uniform(-iw, iw), rng.uniform(-ih, ih)
        a = math.atan2(y, x) + rng.uniform(-0.3, 0.3)
        m_ = 'fur_d' if rng.random() < 0.45 else ('fur_w' if (abs(x) < iw * 0.25 and y < -ih * 0.4) else 'fur')
        poly_on(K(M[m_]), L1.stroke([(x, y), (max(-iw, min(iw, x + math.cos(a) * 0.02)), max(-ih, min(ih, y + math.sin(a) * 0.02)))], 0.0035), d(2), U, W)
    mw, mh = iw * 0.82, ih * 0.72
    outline = [(mw * math.cos(a) * (1.0 - 0.12 * math.cos(2 * a)), mh * math.sin(a) * (1.1 if math.sin(a) > 0 else 0.95) - ih * 0.05)
               for a in [k * TAU / 36 for k in range(36)]]
    poly_on(K(M['lip']), [(x * 1.08, y * 1.1) for (x, y) in outline], d(3), U, W)
    poly_on(K(M['gum_d']), outline, d(4), U, W)
    poly_on(K(M['gum']), [(x * 0.92, y * 0.9) for (x, y) in outline], d(5), U, W)
    poly_on(K(M['throat']), [(x * 0.62, y * 0.55 + ih * 0.02) for (x, y) in outline], d(6), U, W)
    poly_on(K(M['tongue']), [(mw * 0.55 * math.cos(a), -mh * 0.35 + mh * 0.42 * math.sin(a) * (0.6 if math.sin(a) > 0 else 1.0)) for a in [k * TAU / 24 for k in range(24)]], d(7), U, W)
    poly_on(K(M['gum_d']), L1.stroke([(0, -mh * 0.05), (0, -mh * 0.7)], 0.005), d(8), U, W)  # Mittelfurche
    for (yy, s) in ((mh * 0.86, -1), (-mh * 0.9, 1)):  # Zaehne oben (zeigen nach unten) und unten
        for k in range(6):  # Schneidezaehne
            x = (k - 2.5) * mw * 0.085
            y0 = yy - ih * 0.05 * (1 if s < 0 else -1)
            poly_on(K(M['tooth']), [(x - 0.008, y0), (x + 0.008, y0), (x + 0.006, y0 + s * 0.024), (x - 0.006, y0 + s * 0.026)], d(9), U, W)
        for side in (-1, 1):  # Eckzaehne, gross
            x = side * mw * 0.4
            y0 = yy * 0.94 - ih * 0.05 * (1 if s < 0 else -1)
            poly_on(K(M['tooth']), [(x - 0.018, y0), (x + 0.018, y0), (x + 0.003 * side, y0 + s * 0.07)], d(9), U, W)
            for j in range(3):  # Backenzaehne
                xx = side * mw * (0.56 + j * 0.12)
                yb = y0 * (0.9 - 0.12 * j)
                poly_on(K(M['tooth']), [(xx - 0.013, yb), (xx + 0.013, yb), (xx + 0.006, yb + s * 0.028), (xx - 0.006, yb + s * 0.03)], d(9), U, W)
    for _ in range(5):  # Geiferfaeden zwischen den Kiefern
        x = rng.uniform(-mw * 0.7, mw * 0.7)
        poly_on(K(M['drool']), L1.stroke([(x, mh * 0.7), (x + rng.uniform(-0.01, 0.01), 0.0), (x + rng.uniform(-0.02, 0.02), -mh * 0.6)], 0.0016), d(10), U, W)
    poly_on(K(M['nose']), [(iw * 0.3 * math.cos(a), ih * 0.9 + ih * 0.12 * math.sin(a)) for a in [k * TAU / 16 for k in range(16)]], d(4), U, W)


def _ps_block_geo(K, M, rng):
    hw = PS_W / 2
    box(K(M['block']), (-hw, -hw, 0.0), (hw, hw, PS_H))
    # Fleischfotos auf allen Seiten (je Seite drei uebereinander, schief geklebt, Ecken stehen ab)
    for (n, u) in (((1, 0, 0), (0, 1, 0)), ((-1, 0, 0), (0, -1, 0)), ((0, 1, 0), (-1, 0, 0)), ((0, -1, 0), (1, 0, 0))):
        n, u = V(n), V(u)
        for k in range(3):
            if n.x > 0.5 and k > 0:
                continue  # die Vorderseite traegt oben das Gesicht
            zc = 0.42 + k * 0.78
            tilt = rng.uniform(-0.05, 0.05)
            U = (u * math.cos(tilt) + V((0, 0, 1)) * math.sin(tilt))
            W = (V((0, 0, 1)) * math.cos(tilt) - u * math.sin(tilt))
            _flesh_photo(K, M, n * (hw + 0.003) + V((0, 0, zc)), U, W, PS_W * 0.96, 0.74, rng)
    _flesh_photo(K, M, V((0, 0, PS_H + 0.003)), V((1, 0, 0)), V((0, 1, 0)), PS_W * 0.95, PS_W * 0.95, rng)
    o = V((hw + 0.003, 0, 0))
    U, W = V((0, 1, 0)), V((0, 0, 1))
    _flesh_photo(K, M, o + V((0, 0, 1.6)), U, W, PS_W * 0.96, 1.5, rng)
    # Gesicht: zwei Augenfotos, darunter das Maul (flach, leicht schief, etwas zu weit auseinander)
    for side, iris, lk in ((-1, 'iris_a', (0.3, 0.1)), (1, 'iris_b', (-0.2, 0.0))):
        tilt = 0.06 * side + rng.uniform(-0.02, 0.02)
        Ue = V((0, math.cos(tilt), math.sin(tilt)))
        We = V((0, -math.sin(tilt), math.cos(tilt)))
        _eye_photo(K, M, o + V((0.006, side * 0.205, 2.03 + 0.02 * side)), Ue, We, 0.34, 0.21, rng, iris, lk)
    _mouth_photo(K, M, o + V((0.007, 0.01, 1.5)), V((0, math.cos(0.03), math.sin(0.03))), V((0, -math.sin(0.03), math.cos(0.03))), 0.74, 0.5, rng)


def build_ps_block(v=0):
    rng = random.Random(1040)
    K = Kit()
    M = _ps_mats()
    _ps_block_geo(K, M, rng)
    K.finish()
    if v > 0:  # Scheibe v (1..4) von unten: waagrechte Baender fuer den Glitch
        z0, z1 = PS_H * (v - 1) / 4, PS_H * v / 4
        for ob in list(bpy.context.scene.objects):
            if ob.type != 'MESH':
                continue
            bm = bmesh.new()
            bm.from_mesh(ob.data)
            geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
            r = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z0), plane_no=(0, 0, 1), clear_inner=True)
            geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
            bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z1), plane_no=(0, 0, 1), clear_outer=True)
            bm.to_mesh(ob.data)
            bm.free()
            if not ob.data.polygons:
                bpy.data.objects.remove(ob)


def build_ps_block__v1():
    build_ps_block(1)


def build_ps_block__v2():
    build_ps_block(2)


def build_ps_block__v3():
    build_ps_block(3)


def build_ps_block__v4():
    build_ps_block(4)


def build_ps_screen():
    """Leuchtendes Bild fuer den Roehrenmonitor (Ebene x = 0, Blick +x, 0,30 x 0,25 m): Fleisch, darin ein Auge."""
    rng = random.Random(1041)
    K = Kit()
    M = _ps_mats()
    U, W = V((0, 1, 0)), V((0, 0, 1))
    o = V((0, 0, 0))
    w, h = 0.30, 0.25
    poly_on(K(M['sg_red']), [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)], o, U, W)
    for _ in range(26):
        cx, cy = rng.uniform(-w / 2, w / 2), rng.uniform(-h / 2, h / 2)
        pts = [(max(-w / 2, min(w / 2, a)), max(-h / 2, min(h / 2, b))) for (a, b) in blob2(rng, cx, cy, rng.uniform(0.01, 0.04), rng.uniform(0.006, 0.02), 10, 0.4)]
        poly_on(K(M['sg_dark' if rng.random() < 0.6 else 'sg_pink']), pts, o + V((0.0006, 0, 0)), U, W)
    for k in range(9):  # Zeilenstoerung: dunkle Streifen
        y = -h / 2 + rng.uniform(0, h)
        poly_on(K(M['sg_black']), [(-w / 2, y), (w / 2, y), (w / 2, y + 0.003), (-w / 2, y + 0.003)], o + V((0.0012, 0, 0)), U, W)
    lid = [(0.07 * math.cos(a), 0.035 * math.sin(a)) for a in [k * TAU / 24 for k in range(24)]]
    poly_on(K(M['sg_white']), lid, o + V((0.0018, 0, 0.01)), U, W)
    poly_on(K(M['sg_black']), [(0.028 * math.cos(a), 0.01 + 0.028 * math.sin(a)) for a in [k * TAU / 16 for k in range(16)]], o + V((0.0024, 0, 0)), U, W)
    K.finish()


def _arm_seg(K, M, L, r0, r1, rng, elbow=True):
    pts = [V((L * i / 8, 0, 0.004 * math.sin(i * 0.9))) for i in range(9)]
    rs = [(r0 + (r1 - r0) * i / 8) * (1.0 + 0.1 * math.sin(i * 1.7 + 0.5)) for i in range(9)]
    sweep(K(M['armskin'], 45), pts, rs, 12)
    ellipsoid(K(M['armskin'], 45), (0, 0, 0), (1, 0, 0), r0 * 1.1, r0 * 1.02, 12, 6)
    for i in range(3):  # Falten
        x = L * rng.uniform(0.1, 0.9)
        sweep(K(M['armskin_d'], 45), [(x - 0.004, 0, 0), (x + 0.004, 0, 0)], (r0 + (r1 - r0) * x / L) * 1.03, 12)
    for _ in range(9):  # Blut: verschmiert und in Laeufen
        x = L * rng.uniform(0.05, 0.95)
        a = rng.uniform(0, TAU)
        r = (r0 + (r1 - r0) * x / L) * 1.04
        p = V((x, r * math.cos(a), r * math.sin(a)))
        ellipsoid(K(M['blood' if rng.random() < 0.6 else 'blood_d'], 40), p, (math.cos(a) * 0.3, math.sin(a) * 0.3, 1), r * rng.uniform(0.15, 0.4), r * rng.uniform(0.2, 0.55), 8, 3)
        if math.sin(a) < 0.2:
            sweep(K(M['blood'], 40), [p, p + V((rng.uniform(-0.02, 0.02), 0, -rng.uniform(0.02, 0.06)))], [r * 0.12, r * 0.04], 5)


def build_ps_arm_a():
    _K = Kit()
    _arm_seg(_K, _ps_mats(), PS_ARM[0], 0.075, 0.06, random.Random(1042))
    _K.finish()


def build_ps_arm_b():
    _K = Kit()
    _arm_seg(_K, _ps_mats(), PS_ARM[1], 0.058, 0.045, random.Random(1043))
    _K.finish()


def build_ps_hand(v=0):
    """Grosse weisse Hand (Ursprung Handgelenk, Finger entlang +x); v0 offen, v1 greift."""
    rng = random.Random(1044 + v)
    K = Kit()
    M = _ps_mats()
    palm = [V((0.0, 0, 0)), V((0.06, 0, 0)), V((0.13, 0, 0))]
    sweep(K(M['armskin'], 45), palm, [0.045, 0.058, 0.055], 12, profile=[(math.cos(a), 0.45 * math.sin(a)) for a in [k * TAU / 12 for k in range(12)]])
    curl = 0.25 if v == 0 else 1.15
    for f in range(5):
        if f == 0:
            root, dd = V((0.04, -0.05, -0.005)), V((0.6, -0.8, -0.15)).normalized()
        else:
            root, dd = V((0.13, (f - 2.5) * 0.026, 0.0)), V((1.0, (f - 2.5) * 0.12, 0.0)).normalized()
        lens = (0.05, 0.035, 0.028) if f == 0 else (0.07, 0.045, 0.032)
        p = root
        for k, ln in enumerate(lens):
            a = curl * (0.5 + 0.3 * k) if f else curl * 0.4
            dd = (dd * math.cos(a) - V((0, 0, 1)) * math.sin(a)).normalized()
            q = p + dd * ln
            sweep(K(M['armskin'], 45), [p, q], [0.013 - 0.002 * k, 0.012 - 0.002 * k], 8)
            ellipsoid(K(M['armskin_d'], 45), p, dd, 0.014 - 0.002 * k, 0.013 - 0.002 * k, 8, 4)
            p = q
        lathe(K(M['nail'], 40), [(0.0, 0.0), (0.008, 0.0), (0.007, 0.002), (0.0, 0.0025)], 8,
              Matrix.Translation(p - dd * 0.008 + V((0, 0, 0.008))) @ Matrix.Rotation(0.0, 4, 'X'))
        if rng.random() < 0.7:
            ellipsoid(K(M['blood'], 40), p - dd * 0.01, dd, 0.014, 0.011, 6, 3)
    for _ in range(6):
        p = V((rng.uniform(0.0, 0.13), rng.uniform(-0.05, 0.05), rng.choice((-1, 1)) * 0.024))
        ellipsoid(K(M['blood_d'], 40), p, (0, 0, 1), 0.006, rng.uniform(0.01, 0.025), 6, 3)
    K.finish()


def build_ps_hand__v1():
    build_ps_hand(1)


def build_ps_edge():
    K = Kit()
    M = _ps_mats()
    box(K(M['edge']), (0.0, -0.0125, -0.0125), (1.0, 0.0125, 0.0125))
    K.finish()


def build_ps_handle():
    K = Kit()
    M = _ps_mats()
    box(K(M['edge']), (-0.04, -0.04, -0.04), (0.04, 0.04, 0.04))
    for (n, u, w) in (((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)), ((0, 1, 0), (-1, 0, 0), (0, 0, 1)),
                      ((0, -1, 0), (1, 0, 0), (0, 0, 1)), ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0))):
        poly_on(K(M['handle']), [(-0.03, -0.03), (0.03, -0.03), (0.03, 0.03), (-0.03, 0.03)], V(n) * 0.0405, u, w)
    K.finish()


# --- Macchina -------------------------------------------------------------------------------------------------------------
def _mc_mats():
    return {
        'pipe': mat('mc_rohrblech', (0.95, 0.88, 0.82), 'metal', tex='metal_rusty_iron', r=1.25, m=0.1),
        'brass': mat('mc_messing', (0.62, 0.48, 0.24), 'metal', r=0.75, m=0.2),
        'steel': mat('mc_stahl_alt', (0.36, 0.36, 0.37), 'metal', r=0.95),
        'dark': mat('mc_innen', (0.04, 0.035, 0.03), 'plastic', r=1.3),
        'pad': mat('mc_polster', (0.30, 0.22, 0.16), 'fabric', r=1.4),
        'weld': mat('mc_schweissnaht', (0.25, 0.22, 0.20), 'metal', r=1.2),
        'glass': mat('mc_messglas', (0.55, 0.62, 0.58), 'glass', r=0.2),
        'cable': mat('mc_kabel', (0.08, 0.07, 0.06), 'rubber', r=1.1),
        'red': mat('mc_handrad_rot', (0.42, 0.09, 0.06), 'paint', r=1.3),
        'dust': mat('mc_staub', (0.42, 0.40, 0.36), 'concrete', r=1.4),
        'cloth': mat('mc_tuch', (0.52, 0.44, 0.34), 'fabric', r=1.4),
        'cloth_d': mat('mc_tuch_falte', (0.30, 0.25, 0.19), 'fabric', r=1.45),
        'ink_b': mat('mc_farbe_blau', (0.12, 0.22, 0.45), 'fabric', r=1.3),
        'ink_w': mat('mc_farbe_hell', (0.92, 0.86, 0.62), 'fabric', r=1.2),
        'ink_k': mat('mc_farbe_russ', (0.06, 0.05, 0.05), 'fabric', r=1.3),
        'ink_r': mat('mc_farbe_rost', (0.48, 0.20, 0.10), 'fabric', r=1.3),
        'wood': mat('mc_schaft', (0.36, 0.26, 0.16), 'wood', r=1.2),
        'soot': mat('mc_russ', (0.05, 0.045, 0.04), 'metal', r=1.3),
    }


def _pipe_limb(K, M, a, b, r, rng, valve=False):
    a, b = V(a), V(b)
    cyl(K(M['pipe'], 40), a, b, r, 12)
    for t in (0.0, 1.0):  # Flansche
        p = a.lerp(b, t)
        dd = (b - a).normalized()
        cyl(K(M['steel'], 40), p - dd * 0.008, p + dd * 0.008, r * 1.35, 12)
        for k in range(6):
            ang = k * TAU / 6
            side = dd.orthogonal().normalized()
            side2 = dd.cross(side)
            q = p + (side * math.cos(ang) + side2 * math.sin(ang)) * r * 1.2
            cyl(K(M['brass'], 40), q - dd * 0.012, q + dd * 0.012, 0.004, 6)
    if valve:
        mid = a.lerp(b, 0.5)
        dd = (b - a).normalized()
        side = dd.orthogonal().normalized()
        c = mid + side * r * 1.6
        lathe(K(M['red'], 40), [(0.03, -0.006), (0.036, 0.0), (0.03, 0.006), (0.026, 0.0)], 14, Matrix.Translation(c) @ side.to_track_quat('Z', 'Y').to_matrix().to_4x4())
        cyl(K(M['steel'], 40), mid, c, 0.006, 6)


def build_macchina_suit():
    """Leerer Anzug, an die Wand gesunken (Ruecken bei x = 0): Rumpf aus Rohrblech, Brust wie eine Muschel offen und hohl,
    Arme und Beine aus Rohrstuecken mit Flanschen und Handraedern, Kolben an den Gelenken, Helm mit Visier; auf das Visier
    ein Gesicht geschweisst (Brauen, Augen, Nase, Mund aus Schweissraupen) - damit sie fuer The Lost nicht fremd wirkten."""
    rng = random.Random(1890)
    K = Kit()
    M = _mc_mats()
    # Rumpf: Tonne, schraeg an die Wand gelehnt; vorne offen (zwei Schalen), innen Polster und Kabel, leer
    tc = V((0.32, 0.0, 0.62))
    lean = V((0.42, 0.0, 1.0)).normalized()
    q = V((0, 0, 1)).rotation_difference(lean).to_matrix().to_4x4()
    lathe(K(M['pipe'], 45), [(0.0, -0.3), (0.2, -0.3), (0.25, -0.2), (0.27, 0.05), (0.25, 0.25), (0.18, 0.33), (0.0, 0.34)], 18,
          Matrix.Translation(tc) @ q, a0=math.pi * 0.65, a1=math.pi * 2.0 + math.pi * 0.35)
    lathe(K(M['dark'], 45), [(0.0, -0.28), (0.18, -0.28), (0.235, -0.19), (0.25, 0.05), (0.23, 0.24), (0.0, 0.31)], 18,
          Matrix.Translation(tc) @ q)
    for s in (-1, 1):  # die zwei Brustschalen, aufgeklappt zur Seite
        c = tc + V((0.22, s * 0.2, 0.0))
        lathe(K(M['pipe'], 45), [(0.0, -0.28), (0.18, -0.28), (0.22, -0.1), (0.23, 0.12), (0.19, 0.27), (0.0, 0.3)], 10,
              Matrix.Translation(c) @ q @ Matrix.Rotation(s * 1.2, 4, 'Z'), a0=-0.6, a1=0.6)
        for k in range(4):  # Scharniere
            z = -0.2 + k * 0.14
            p = tc + q.to_3x3() @ V((0.24 * math.cos(s * 0.95), 0.24 * math.sin(s * 0.95), z))
            cyl(K(M['brass'], 40), p - V((0, 0, 0.02)), p + V((0, 0, 0.02)), 0.012, 8)
    for _ in range(9):  # Polsterreste und Kabel im Hohlraum
        p = tc + q.to_3x3() @ V((rng.uniform(-0.12, 0.08), rng.uniform(-0.15, 0.15), rng.uniform(-0.2, 0.25)))
        ellipsoid(K(M['pad'], 40), p, (rng.uniform(-1, 1), rng.uniform(-1, 1), 1), rng.uniform(0.03, 0.06), rng.uniform(0.02, 0.04), 7, 4)
    for _ in range(6):
        p0 = tc + q.to_3x3() @ V((rng.uniform(-0.15, 0.0), rng.uniform(-0.18, 0.18), rng.uniform(0.0, 0.25)))
        p1 = p0 + V((rng.uniform(0.05, 0.2), rng.uniform(-0.1, 0.1), -rng.uniform(0.15, 0.4)))
        sweep(K(M['cable'], 40), [p0, p0.lerp(p1, 0.5) + V((0.03, 0, -0.04)), p1], 0.008, 6)
    # Schultern: grosse Kugelpfannen mit Zahnraedern
    for s in (-1, 1):
        sh = tc + q.to_3x3() @ V((0.0, s * 0.3, 0.22))
        ellipsoid(K(M['steel'], 45), sh, (0, s, 0), 0.1, 0.11, 12, 6)
        for k in range(10):
            a = k * TAU / 10
            p = sh + V((0.11 * math.cos(a), s * 0.06, 0.11 * math.sin(a)))
            box(K(M['brass']), (p.x - 0.012, p.y - 0.01, p.z - 0.012), (p.x + 0.012, p.y + 0.01, p.z + 0.012))
        # Arm: Oberarm, Ellbogenkolben, Unterarm, Greifer - schlaff zum Boden
        el = sh + V((0.16, s * 0.12, -0.32))
        wr = el + V((0.28, s * 0.05, -0.22))
        _pipe_limb(K, M, sh, el, 0.055, rng, valve=(s > 0))
        ellipsoid(K(M['steel'], 40), el, (0, 1, 0), 0.07, 0.07, 10, 5)
        cyl(K(M['brass'], 40), sh + V((0.05, 0, -0.08)), el + V((0.05, 0, 0.0)), 0.012, 8)  # Kolben
        _pipe_limb(K, M, el, wr, 0.045, rng)
        for f in range(3):
            a = (f - 1) * 0.5
            p1 = wr + V((0.08 * math.cos(a), s * 0.02 + 0.08 * math.sin(a), -0.03))
            p2 = p1 + V((0.05, 0, -0.06))
            sweep(K(M['steel'], 40), [wr, p1, p2], [0.016, 0.014, 0.008], 7)
        # Bein: ausgestreckt
        hp = tc + q.to_3x3() @ V((0.0, s * 0.13, -0.3))
        kn = V((hp.x + 0.42, s * 0.18, 0.2))
        ft = V((kn.x + 0.4, s * 0.2, 0.08))
        _pipe_limb(K, M, hp, kn, 0.07, rng, valve=(s < 0))
        ellipsoid(K(M['steel'], 40), kn, (0, 1, 0), 0.085, 0.08, 10, 5)
        _pipe_limb(K, M, kn, ft, 0.06, rng)
        box(K(M['steel']), (ft.x - 0.04, ft.y - 0.08, 0.0), (ft.x + 0.18, ft.y + 0.08, 0.1))
        box(K(M['brass']), (ft.x + 0.12, ft.y - 0.08, 0.0), (ft.x + 0.2, ft.y + 0.08, 0.04))
    # Helm: Kuppel, nach vorn gesunken; Visier mit geschweisstem Gesicht
    hc = tc + q.to_3x3() @ V((0.06, 0.0, 0.5))
    hq = V((0, 0, 1)).rotation_difference(V((0.75, 0.05, 0.66)).normalized()).to_matrix().to_4x4()
    lathe(K(M['pipe'], 45), [(0.0, -0.17), (0.17, -0.16), (0.2, 0.0), (0.18, 0.12), (0.1, 0.2), (0.0, 0.22)], 16, Matrix.Translation(hc) @ hq)
    vis_c = hc + hq.to_3x3() @ V((0.0, 0.0, 0.205))
    vn = (hq.to_3x3() @ V((0, 0, 1))).normalized()
    vu = vn.cross(V((0, 0, 1))).normalized() if abs(vn.z) < 0.95 else V((0, 1, 0))
    vw = vu.cross(vn).normalized()
    lathe(K(M['steel'], 45), [(0.0, 0.0), (0.15, 0.0), (0.155, 0.01), (0.0, 0.012)], 16, Matrix.Translation(vis_c) @ hq)
    fo = vis_c + vn * 0.013
    for side in (-1, 1):
        pts = [(side * 0.03, 0.05), (side * 0.07, 0.065), (side * 0.11, 0.05)]
        sweep(K(M['weld'], 40), [fo + vu * a + vw * b for (a, b) in pts], 0.006, 6)                # Braue
        sweep(K(M['weld'], 40), [fo + vu * (side * 0.07 + 0.025 * math.cos(t)) + vw * (0.02 + 0.014 * math.sin(t)) for t in [k * TAU / 10 for k in range(11)]], 0.005, 6)  # Auge
        ellipsoid(K(M['dark'], 30), fo + vu * side * 0.07 + vw * 0.02, vn, 0.004, 0.009, 6, 3)
    sweep(K(M['weld'], 40), [fo + vw * 0.035, fo + vu * 0.012 + vw * -0.025, fo + vw * -0.03], 0.006, 6)  # Nase
    sweep(K(M['weld'], 40), [fo + vu * a + vw * (-0.075 + 0.012 * math.cos(a * 18)) for a in [-0.06 + k * 0.02 for k in range(7)]], 0.006, 6)  # Mund
    # Staub auf allem, was oben liegt
    for _ in range(18):
        p = V((rng.uniform(0.15, 1.1), rng.uniform(-0.35, 0.35), 0.002))
        ellipsoid(K(M['dust'], 40), p, (0, 0, 1), 0.002, rng.uniform(0.02, 0.06), 8, 2)
    K.finish()


def build_macchina_cradle():
    """Unfertiges Kindergestell (etwa 0,7 m): kleine Rippen aus Rohrringen, ein Aermchen, kein Helm; ein Tuch halb darueber
    (die Macchina bildeten Tote nicht ab), daneben Zange und Blechzuschnitte - nie fertig geworden."""
    rng = random.Random(1891)
    K = Kit()
    M = _mc_mats()
    c = V((0.25, 0.0, 0.0))
    for k in range(6):
        z = 0.1 + k * 0.07
        r = 0.11 + 0.03 * math.sin(k * 0.6)
        lathe(K(M['pipe'], 40), [(r, -0.006), (r + 0.012, 0.0), (r, 0.006), (r - 0.006, 0.0)], 14, Matrix.Translation(c + V((0, 0, z))),
              a0=0.6, a1=TAU - 0.6)
    for a in (math.pi, math.pi - 0.5, math.pi + 0.5):
        p0 = c + V((0.12 * math.cos(a), 0.12 * math.sin(a), 0.08))
        cyl(K(M['steel'], 40), p0, p0 + V((0, 0, 0.36)), 0.009, 6)
    _pipe_limb(K, M, c + V((0.0, 0.14, 0.4)), c + V((0.12, 0.2, 0.22)), 0.025, rng)
    ellipsoid(K(M['steel'], 40), c + V((0.0, 0.14, 0.42)), (0, 1, 0), 0.04, 0.04, 8, 4)
    box(K(M['steel']), (c.x - 0.15, c.y - 0.15, 0.0), (c.x + 0.15, c.y + 0.15, 0.06))
    # Tuch: halb ueber das Gestell gezogen, faellt zur Wand
    for i in range(10):
        t = i / 9
        p0 = c + V((-0.16 + 0.05 * math.sin(i), -0.18 + 0.36 * t, 0.5 - 0.06 * math.sin(t * math.pi)))
        p1 = p0 + V((-0.08, 0.0, -0.42))
        sweep(K(M['cloth' if i % 2 else 'cloth_d'], 40), [p0, p0.lerp(p1, 0.5) + V((-0.04, 0, 0)), p1], [0.025, 0.028, 0.03], 5)
    for _ in range(4):  # Blechzuschnitte, Zange
        x, y = rng.uniform(0.45, 0.65), rng.uniform(-0.25, 0.25)
        a = rng.uniform(0, TAU)
        poly_on(K(M['steel']), [(x + 0.06 * math.cos(a + k * TAU / 5), y + 0.04 * math.sin(a + k * TAU / 5)) for k in range(5)], V((0, 0, 0.004)), V((1, 0, 0)), V((0, 1, 0)))
    for s in (-1, 1):
        sweep(K(M['steel'], 40), [(0.5, 0.2, 0.01), (0.62, 0.22 + s * 0.02, 0.012), (0.68, 0.22 + s * 0.035, 0.012)], 0.006, 6)
    K.finish()


def build_macchina_tapestry():
    """Wandtuch (Wand bei x = 0): oben Lichtkugeln mit Strahlen, aus denen lange Gestalten herabsteigen und die Haende
    ausstrecken; unten kniende Anzugtraeger mit erhobenen Armen; am Rand Zeichen wie Hieroglyphen; verblasst, ausgefranst."""
    rng = random.Random(1892)
    K = Kit()
    M = _mc_mats()
    U, W = V((0, 1, 0)), V((0, 0, 1))
    o = V((0.012, 0, 0))
    w, z0, z1 = 0.8, 0.85, 2.2
    for i in range(12):  # Tuch mit Falten (senkrechte Bahnen, unten ausgefranst)
        y0 = -w + 2 * w * i / 12
        y1 = y0 + 2 * w / 12
        d = 0.006 * math.sin(i * 1.7)
        poly_on(K(M['cloth' if i % 2 else 'cloth_d']), [(y0, z0 + rng.uniform(-0.03, 0.0)), (y1, z0 + rng.uniform(-0.03, 0.0)), (y1, z1), (y0, z1)], o + V((d, 0, 0)), U, W)
    sweep(K(M['wood'], 30), [(0.03, -w - 0.06, z1 + 0.02), (0.03, w + 0.06, z1 + 0.02)], 0.018, 8)
    o2 = o + V((0.009, 0, 0))
    # Lichtkugeln
    for (cy, cz, r) in ((-0.42, 2.0, 0.09), (0.0, 2.06, 0.12), (0.4, 1.98, 0.08)):
        poly_on(K(M['ink_w']), [(cy + r * math.cos(a), cz + r * math.sin(a)) for a in [k * TAU / 20 for k in range(20)]], o2, U, W)
        for k in range(12):
            a = k * TAU / 12
            poly_on(K(M['ink_w']), L1.stroke([(cy + r * 1.25 * math.cos(a), cz + r * 1.25 * math.sin(a)), (cy + r * 1.7 * math.cos(a), cz + r * 1.7 * math.sin(a))], 0.006), o2, U, W)
        # herabsteigende Gestalt: lang, schmal, Arme nach unten ausgestreckt
        hz = cz - r - 0.06
        poly_on(K(M['ink_k']), [(cy + 0.025 * math.cos(a), hz + 0.03 * math.sin(a)) for a in [k * TAU / 12 for k in range(12)]], o2 + V((0.001, 0, 0)), U, W)
        poly_on(K(M['ink_k']), L1.stroke([(cy, hz - 0.03), (cy + 0.01, hz - 0.36)], 0.022), o2 + V((0.001, 0, 0)), U, W)
        for s in (-1, 1):
            poly_on(K(M['ink_k']), L1.stroke([(cy, hz - 0.06), (cy + s * 0.08, hz - 0.2), (cy + s * 0.1, hz - 0.3)], 0.01), o2 + V((0.001, 0, 0)), U, W)
            poly_on(K(M['ink_k']), L1.stroke([(cy + 0.01, hz - 0.36), (cy + s * 0.03, hz - 0.5)], 0.012), o2 + V((0.001, 0, 0)), U, W)
    # kniende Anzugtraeger (klobig: Tonne, Kugelhelm), Arme hoch
    for i, cy in enumerate((-0.62, -0.38, -0.14, 0.12, 0.36, 0.6)):
        bz = 1.02
        poly_on(K(M['ink_b']), [(cy - 0.06, bz), (cy + 0.06, bz), (cy + 0.07, bz + 0.16), (cy - 0.07, bz + 0.16)], o2, U, W)
        poly_on(K(M['ink_b']), [(cy + 0.045 * math.cos(a), bz + 0.21 + 0.045 * math.sin(a)) for a in [k * TAU / 12 for k in range(12)]], o2, U, W)
        poly_on(K(M['ink_w']), [(cy - 0.02, bz + 0.21), (cy + 0.02, bz + 0.21), (cy + 0.02, bz + 0.225), (cy - 0.02, bz + 0.225)], o2 + V((0.001, 0, 0)), U, W)
        for s in (-1, 1):
            poly_on(K(M['ink_b']), L1.stroke([(cy + s * 0.06, bz + 0.14), (cy + s * 0.1, bz + 0.26), (cy + s * 0.09, bz + 0.34)], 0.018), o2, U, W)
            poly_on(K(M['ink_b']), L1.stroke([(cy + s * 0.04, bz), (cy + s * 0.08, bz - 0.06), (cy + s * 0.03, bz - 0.09)], 0.022), o2, U, W)
    # Zeichen am Rand (wie Hieroglyphen: Augen, Kreise, Wellen, Stufen)
    for k in range(18):
        for side in (-1, 1):
            cy, cz = side * (w - 0.06), z0 + 0.08 + k * 0.072
            kind = rng.randrange(5)
            m_ = M['ink_r']
            if kind == 0:
                poly_on(K(m_), L1.stroke([(cy - 0.025, cz), (cy - 0.01, cz + 0.015), (cy + 0.01, cz - 0.015), (cy + 0.025, cz)], 0.005), o2, U, W)
            elif kind == 1:
                poly_on(K(m_), [(cy + 0.02 * math.cos(a), cz + 0.012 * math.sin(a)) for a in [j * TAU / 12 for j in range(12)]], o2, U, W)
                poly_on(K(M['cloth']), [(cy + 0.007 * math.cos(a), cz + 0.007 * math.sin(a)) for a in [j * TAU / 8 for j in range(8)]], o2 + V((0.001, 0, 0)), U, W)
            elif kind == 2:
                poly_on(K(m_), L1.stroke([(cy - 0.02, cz - 0.015), (cy - 0.02, cz), (cy, cz), (cy, cz + 0.015), (cy + 0.02, cz + 0.015)], 0.005), o2, U, W)
            elif kind == 3:
                poly_on(K(m_), L1.stroke([(cy, cz - 0.02), (cy, cz + 0.02)], 0.005), o2, U, W)
                poly_on(K(m_), L1.stroke([(cy - 0.015, cz + 0.008), (cy + 0.015, cz + 0.008)], 0.005), o2, U, W)
            else:
                poly_on(K(m_), [(cy - 0.018, cz - 0.015), (cy + 0.018, cz - 0.015), (cy, cz + 0.018)], o2, U, W)
    for _ in range(14):  # Flecken, Stockflecken
        cy, cz = rng.uniform(-w, w), rng.uniform(z0, z1)
        poly_on(K(M['cloth_d']), blob2(rng, cy, cz, rng.uniform(0.02, 0.06), rng.uniform(0.02, 0.05), 10, 0.4), o2 + V((0.002, 0, 0)), U, W)
    K.finish()


def build_macchina_spear():
    """Pike an der Wand (Wand bei x = 0, angelehnt): Schaft aus Rohr mit Lederwicklung, Klinge aus Blech mit Zeichen;
    davor am Boden die Brandroehre - ein Rohr mit Kammer und Zuender, innen verrusst (zu teuer, nie benutzt)."""
    rng = random.Random(1893)
    K = Kit()
    M = _mc_mats()
    foot = V((0.42, 0.0, 0.0))
    top = V((0.06, 0.05, 2.15))
    dd = (top - foot).normalized()
    cyl(K(M['pipe'], 40), foot, top, 0.016, 10)
    for k in range(9):  # Wicklung
        p = foot.lerp(top, 0.45 + k * 0.012)
        cyl(K(M['pad'], 40), p, p + dd * 0.01, 0.019, 10)
    side = dd.cross(V((1, 0, 0))).normalized()
    blade = [top + dd * 0.0 + side * 0.035, top + dd * 0.12 + side * 0.04, top + dd * 0.34, top + dd * 0.12 - side * 0.04, top - side * 0.035]
    bm = K(M['steel'], 30)
    nrm = dd.cross(side).normalized()
    bm.faces.new([bm.verts.new(p + nrm * 0.004) for p in blade])
    bm.faces.new([bm.verts.new(p - nrm * 0.004) for p in reversed(blade)])
    for i in range(len(blade)):
        a, b = blade[i], blade[(i + 1) % len(blade)]
        bm.faces.new([bm.verts.new(p) for p in (a + nrm * 0.004, a - nrm * 0.004, b - nrm * 0.004, b + nrm * 0.004)])
    for k in range(4):  # eingeschlagene Zeichen auf der Klinge
        c = top + dd * (0.06 + k * 0.06)
        poly_on(K(M['soot']), [(0.008 * math.cos(a), 0.008 * math.sin(a)) for a in [j * TAU / 8 for j in range(8)]], c + nrm * 0.0045, side, dd)
    cyl(K(M['brass'], 40), top - dd * 0.03, top + dd * 0.02, 0.022, 10)
    # Brandroehre am Boden
    p0, p1 = V((0.55, -0.35, 0.05)), V((0.95, -0.2, 0.05))
    cyl(K(M['steel'], 40), p0, p1, 0.035, 12)
    cyl(K(M['soot'], 40), p1 - (p1 - p0).normalized() * 0.002, p1 + (p1 - p0).normalized() * 0.001, 0.026, 12)
    ellipsoid(K(M['brass'], 40), p0.lerp(p1, 0.2) + V((0, 0, 0.03)), (1, 0.3, 0), 0.06, 0.045, 10, 5)
    cyl(K(M['brass'], 40), p0.lerp(p1, 0.15) + V((0, 0, 0.06)), p0.lerp(p1, 0.15) + V((0, 0, 0.11)), 0.006, 6)
    for k in range(5):
        p = p1 + V((rng.uniform(0.02, 0.15), rng.uniform(-0.08, 0.08), 0.001))
        ellipsoid(K(M['soot'], 40), p, (0, 0, 1), 0.001, rng.uniform(0.02, 0.05), 8, 2)
    K.finish()
