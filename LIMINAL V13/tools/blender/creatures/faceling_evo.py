"""V13.2: entwickelter Faceling (Entity 9; Level 3: feindselig, mit Steinwerkzeugen). Die Seite: einzelne, fehlgebildete
Zuege - ein Auge oder ein Ohr, halb verwachsene Nasenloecher, kleine kurzsichtige Augen, ein Mund ohne Lippen, der oft
offen haengt, platte Ohren voller Schmalz, mehr Haar an Koerper und Gesicht. Das Werkzeug: ein geschlagener Stein in der
rechten Hand. Die Kleidung ist zerrissen.
"""
import math

import numpy as np

import anat as A
import faceling as F
import human as Hm
import sdf

LODS = (22000, 7600, 2600)
SKELETON = 'ent_faceling'
MODEL = 'ent_faceling_evo'


def build(J, bones):
    rng = np.random.default_rng(31)
    B, groups = F.build_parts(J, slim=0.96, muscle=1.0, sleeves=0.62, hair='short', rng=rng)
    c, r = B.headC, B.headR
    X, Y, Z = Hm.X, Hm.Y, Hm.Z
    skin = groups[0]['parts']
    front = c + X * r[0] * 0.96
    # ein einzelnes Auge (links, zu klein), tief in einer flachen Hoehle
    eyeC = c + X * r[0] * 0.80 + Y * r[1] * 0.38 + Z * r[2] * 0.05
    skin.append(A.ell(Hm.HEAD, eyeC + X * 0.012, X, (0.018, 0.022, 0.014), op='sub', k=0.012))
    # halb verwachsene Nase: ein Hoecker mit einem Loch
    nose = c + X * r[0] * 0.98 - Z * r[2] * 0.18
    skin.append(A.ell(Hm.HEAD, nose, X - Z * 0.5, (0.018, 0.016, 0.022), k=0.015))
    skin.append(A.ell(Hm.HEAD, nose + X * 0.012 - Z * 0.012 + Y * 0.004, X, (0.006, 0.004, 0.003), op='sub', k=0.002))
    # Mund ohne Lippen, offen haengend (Kiefer)
    mouthC = c + X * r[0] * 0.86 - Z * r[2] * 0.48
    skin.append(A.ell(Hm.JAW, mouthC + X * 0.02, X, (0.03, 0.03, 0.016), op='sub', k=0.008))
    # platte Ohren
    for s in (0, 1):
        g = Hm.side(s)
        skin.append(A.ell(Hm.HEAD, c + Y * g * r[1] * 0.98 - Z * r[2] * 0.08 - X * 0.01, Y * g, (0.008, 0.028, 0.034), up=Z, k=0.01))
    # Unterarme frei (zerrissene, kurze Aermel) - Haut bis zum Ellbogen
    for s in (0, 1):
        skin += B.arm(s, upper=False, fore=True, hand=False, sleeve_to=1.0)
        skin.append(A.cone(Hm.UPPER + 4 * s, A.lerp(B.j(Hm.jSH, s), B.j(Hm.jEL, s), 0.5), B.j(Hm.jEL, s), 0.036, 0.034, k=0.02))
    # Risse im Hemd (Haut darunter): Brust und Ruecken
    skin += B.torso(upper=True, lower=False)[:2]
    shirt = groups[1]['parts']
    for k in range(5):
        p = np.array([0.1 if k % 2 else -0.1, (rng.random() - 0.5) * 0.24, B.chestZ - 0.1 + 0.18 * rng.random()])
        shirt.append(A.ell(Hm.CHEST, p, A.unit(np.array([0.2, rng.random() - 0.5, 1.0])), (0.05 + 0.03 * rng.random(), 0.012, 0.03), up=X, op='sub', k=0.008))
    # das Auge selbst: klein, dunkel, feucht
    eye = [A.ell(Hm.HEAD, eyeC + X * 0.004, X, (0.012, 0.012, 0.011), k=0.0)]
    # Zaehne im offenen Mund, Gaumen dunkel
    teeth = A.teeth_row(Hm.HEAD, [mouthC + X * 0.012 + Y * (k - 2.5) * 0.008 + Z * 0.012 for k in range(6)], 0.012, 0.004,
                        [-Z] * 6, rng, jitter=0.4)
    mouth = [A.ell(Hm.JAW, mouthC + X * 0.004, X, (0.024, 0.025, 0.012), k=0.0)]
    # mehr Haar: Brauen, Backen, Kinn, Unterarme (Faeden)
    # V13.3 Abnahme Level 3: der Bart war drei glatte Ellipsoide (ein glaenzender Klotz wie ein Maulkorb) - jetzt einzelne,
    # verfilzte Straehnen an Backen und Kinn, ungleich lang, um den offenen Mund frei, dazwischen Haut
    fur = []
    for k in range(150):
        phi = rng.uniform(-1.25, 1.25)
        el = rng.uniform(-1.25, -0.42)
        ce, se = math.cos(el), math.sin(el)
        p = c + X * (r[0] * ce * math.cos(phi)) + Y * (r[1] * ce * math.sin(phi)) + Z * (r[2] * se)
        if np.linalg.norm(p - (mouthC + X * 0.02)) < 0.034 or rng.random() < 0.18:  # Mund frei, kahle Stellen
            continue
        out = A.unit((p - c) * np.array([1.0, 1.0, 0.5]))
        d = A.unit(out * 0.55 - Z * 0.8 + np.array([0.0, rng.uniform(-0.25, 0.25), 0.0]))
        ln = 0.012 + 0.022 * rng.random() ** 1.5
        p0 = p - out * 0.002
        mid = p0 + d * ln * 0.5 + out * 0.003 * rng.random()
        fur += A.chain(lambda i: Hm.JAW, [p0, mid, p0 + d * ln], [0.0017, 0.0011, 0.0004], k=0.0)
    for s in (0, 1):
        E, W = B.j(Hm.jEL, s), B.j(Hm.jWR, s)
        for k in range(16):
            p = A.lerp(E, W, 0.1 + 0.8 * rng.random()) + np.array([0.02 * (rng.random() - 0.3), 0.02 * (rng.random() - 0.5), 0.0])
            d = A.unit(W - E) + np.array([rng.random() - 0.5, rng.random() - 0.5, 0.2]) * 0.6
            fur += A.chain(lambda i: Hm.FORE + 4 * s, [p, p + A.unit(d) * 0.018], [0.0014, 0.0005], k=0.0)
    # Steinwerkzeug in der rechten Hand: ein geschlagener Faustkeil, mit der Spitze nach vorn
    W1, H1 = B.j(Hm.jWR, 1), B.j(Hm.jHAND, 1)
    hd = A.unit(H1 - W1)
    grip = W1 + hd * 0.07
    tip = grip + A.unit(hd + X * 1.2) * 0.17
    stone = [A.ell(Hm.HAND + 4, A.lerp(grip, tip, 0.45), tip - grip, (0.085, 0.034, 0.022), up=Z, k=0.0),
             A.ell(Hm.HAND + 4, grip, tip - grip, (0.04, 0.032, 0.03), up=Z, k=0.02)]

    def stone_disp(Q):  # Schlagnarben: muschelige Flaechen
        d1, d2 = sdf.worley(Q, 55, seed=940)
        return -0.004 * np.clip(d2 - d1, 0, 1) - 0.001 * sdf.fbm(Q, 120, 2, seed=941)

    groups += [{'name': 'auge', 'fm': 'ent_motheye', 'parts': eye, 'disp': None, 'h': 0.0015, 'cav_scale': 0.003, 'lod_weight': 2.0},
               {'name': 'zaehne', 'fm': 'ent_tooth', 'parts': teeth, 'disp': None, 'h': 0.0012, 'cav_scale': 0.003, 'lod_weight': 1.5},
               {'name': 'gaumen', 'fm': 'ent_mouth', 'parts': mouth, 'disp': None, 'h': 0.002, 'cav_scale': 0.004},
               {'name': 'koerperhaar', 'fm': 'ent_hair', 'parts': fur, 'disp': None, 'h': 0.0009, 'cav_scale': 0.002, 'merge': 'ent_hair_fur'},
               {'name': 'stein', 'fm': 'ent_stone', 'parts': stone, 'disp': stone_disp, 'h': 0.0025, 'cav_scale': 0.01}]
    # V13.3 Abnahme Level 3 (KRITIK_Level3.md, Runde 3): im Lampenlicht glatt wie eine Schaufensterpuppe, der Bart ein
    # glaenzender schwarzer Klotz (das geteilte Haar ist nass und glaenzt). Eigene Haut (stumpf, staubig, fleckig) und eigenes
    # Haar (stumpf, verfilzt, graubraun) - nur fuer den Entwickelten (Level 3); die anderen Facelings bleiben, wie sie sind
    groups[0]['fm'] = 'ent_fc_skin'
    for g in groups:
        if g['fm'] == 'ent_hair':
            g['fm'] = 'ent_fc_hair'
    return groups
