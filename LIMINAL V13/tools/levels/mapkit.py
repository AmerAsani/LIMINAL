"""LIMINAL V11 - Werkzeug fuer die festen Karten der Level (WORLD_GENERATION.md).

Ein Entwurfsskript (tools/levels/<level>.py) beschreibt die Gestalt eines Levels mit diesem
Werkzeug: Raeume (Rechtecke mit Zone, Raumtyp, Boden- und Deckenhoehe), Tueren auf gemeinsamen
Waenden, Leuchten und Stromkreise, Spawnpunkte, feste Objekte und Uebergaenge. Das Ergebnis wird als
data/maps/<level>.json gespeichert und aendert sich zwischen den Laeufen nicht.

Regeln, die save() prueft (dieselben prueft tests/map_check im Spiel):
  - Raeume ueberlappen sich nicht und haben mindestens 1 m Wand zwischen sich
  - jede Tuer liegt in der Wand zwischen genau den zwei Raeumen, die sie verbindet
  - alle Raeume sind vom Startraum aus erreichbar
  - Spawnpunkte, Objekte und Uebergaenge liegen in Raeumen
  - Sicherungspunkte (Wiedereinstieg nach dem Tod) liegen sicher: siehe checkpoint()

Aufruf:  python tools/levels/<level>.py   (schreibt data/maps/<level>.json und eine Draufsicht)
"""
import json
import math
import os
import random

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..'))


class Room:
    def __init__(self, idx, x0, y0, x1, y1, rtype, zone, floor, height, kw):
        self.idx, self.x0, self.y0, self.x1, self.y1 = idx, x0, y0, x1, y1
        self.type, self.zone, self.floor, self.height = rtype, zone, floor, height
        self.kw = kw
        self.lights = []
        self.blocks = []

    @property
    def w(self):
        return self.x1 - self.x0

    @property
    def h(self):
        return self.y1 - self.y0

    @property
    def cx(self):
        return (self.x0 + self.x1) / 2

    @property
    def cy(self):
        return (self.y0 + self.y1) / 2

    def contains(self, x, y):
        return self.x0 <= x < self.x1 and self.y0 <= y < self.y1


# V11 (Premium-Ueberarbeitung): wie ein Ausstattungsteil haelt. Wandteile brauchen Wand dahinter (keine Tuer,
# keine Oeffnung, kein Nachbarraum) und Luft davor; Bodenteile stehen auf dem Raumboden (oder einem Block);
# Deckenteile haengen an der Decke. validate() prueft es; tests/gameplay_test prueft es in der echten Welt.
WALL_MOUNT = {'outlet', 'vent_grille', 'painting', 'power_box', 'wall_sconce', 'gauge', 'window', 'speaker',
              'exit_sign', 'locker_shutter', 'steel_door', 'hotel_door', 'damaged_door', 'padded_panel',
              'painting_a', 'painting_b', 'painting_c', 'painting_d', 'painting_e', 'painting_f', 'painting_lean_a', 'painting_lean_b', 'painting_lean_c', 'exit_sign_green',  # V12 L1
              'junction_box', 'violet_lamp', 'door_steel_white', 'door_steel_grey', 'door_steel_cream', 'door_window', 'door_plywood', 'door_wood_old', 'door_oversize',  # V12 L2
              'window_brick', 'painting_angel', 'painting_angel_scribbled', 'sign_warning', 'canvas_wall',  # V12 L3
              'wainscot', 'hotel_door_a', 'hotel_door_b', 'hotel_door_c', 'hotel_door_d', 'hotel_door_e', 'hotel_door_f', 'placard_beverly', 'candle_sconce', 'painting_portrait', 'window_bright', 'cornice',  # V12 L5
              'library_wall', 'library_wall_lamp', 'library_wall_upper', 'bookcase_end', 'bookcase_aisle', 'bookcase_aisle_b', 'painting_renaissance_a', 'painting_renaissance_b', 'inscription_bibliotheca', 'wallpaper_peel',  # V12 L5.3
              'window_188_a', 'window_188_b', 'window_188_c', 'window_188_fig', 'window_188_lit', 'window_entry_188', 'door_mahogany', 'walkway_glass',  # V12 L188
              'metro_dormant_a', 'metro_dormant_b', 'metro_dormant_home', 'metro_dormant_coupling', 'sign_903_wall',  # V12 L903
              'formwork', 'ivy_wall', 'neon_tube', 'neon_sign', 'panel_vermilion', 'pipe_wall',  # V13.3 L1-Sektoren
              'fungus_wall', 'claw_marks',  # V13.3 L1-Erscheinungen (Crawler-Pilz, Tuerspuren)
              'skin_hung', 'skin_scratches',  # V13.3 L2: Spuren der Skin-Stealer (KREATUR_L2_Spuren.md)
              'jerry_mural', 'jerry_poster',  # V13.3 Teil 4 L2: Jerrys Raum, Wegweiser der Followers (KREATUR_Jerry.md)
              'macchina_tapestry',  # V13.3 Teil 4 L2: Wandtuch der Macchina (KREATUR_Macchina.md)
              'e18_portrait',  # V13.3 Teil 4 L5: Bildnis im Buero des Gastgebers (KREATUR_Gastgeber.md)
              'departure_board', 'exit_sign_36', 'storefront_a', 'storefront_b', 'storefront_c', 'storefront_closed', 'gate_sign_c30', 'gate_sign_c31', 'gate_sign_c32', 'gate_sign_c33', 'gate_sign_c34', 'gate_door_closed', 'gate_door_boarding', 'clock_36', 'clock_36_b', 'water_fountain', 'restroom_sign'}  # V12 L36
WALL_MOUNT |= {'purple_stealer', 'purple_hound'}  # V13.3 L3: purpifizierte Wesen in der Wand (KREATUR_L3_Purpur.md)
DOOR_MODELS = {'steel_door', 'hotel_door', 'damaged_door'}
CEILING_MOUNT = {'batten', 'highbay', 'chandelier', 'safety_light', 'ceiling_vent',
                 'troffer', 'troffer_off', 'troffer_red', 'ceiling_diffuser', 'hanging_bulb',  # V12: Blender-Modelle
                 'vines_ceiling', 'cable_hang', 'arcade_gothic',  # V13.3 L1-Sektoren, Gothic-Arkaden
                 'tube_white', 'tube_orange', 'tube_dead', 'chandelier_grand', 'lantern_green', 'chandelier_iron', 'ceiling_arms', 'skylight_188', 'tube_long', 'sign_903_a', 'sign_903_b', 'sign_36_a', 'sign_36_b', 'sign_36_empty'}
# V12: Modelle, die in einem Durchbruch stehen (Bogen mit Laibung): Mitte muss in einer Tuer/Oeffnung liegen
IN_OPENING = {'arch2', 'arch3', 'arch4', 'arch_tall3', 'arch_tall4', 'swing_door1', 'swing_door2', 'door_level3', 'elevator_door', 'elevator_cage', 'door_old_open', 'door_mahogany_open'}
FLOOR_MOUNT = {'chair', 'rack', 'bed', 'barrel', 'cart', 'pump', 'wardrobe', 'trunk', 'water_cooler', 'armchair', 'sofa',
               'side_table', 'game_table', 'boiler', 'statue', 'transformer', 'fence_panel', 'rubble', 'barrier', 'camp',
               'desk_crt', 'switch_cabinet', 'bookcase', 'coffee_table', 'desk', 'shelf_unit', 'cabinet',
               'crate_a', 'crate_b', 'crate_c', 'debris_a', 'debris_b', 'hospital_bed', 'work_table', 'column_round',  # V12 L1
               'mach_cabinet', 'mach_pump', 'mach_tank', 'pipe_riser', 'pipe_bundle_a', 'pipe_bundle_b', 'trolley_flatbed', 'toolbox', 'stepladder', 'cable_reel', 'folding_chair', 'hopper_wagon', 'rail_track', 'chamfer_26', 'chamfer_32',  # V12 L2
               'generator', 'conveyor', 'sludge_pool', 'skull', 'train_bogie', 'greek_torso', 'angel_statue', 'canvas_drape_a', 'canvas_drape_b', 'grate_stair', 'pipe_black', 'column_greek',  # V12 L3
               'mahjong_table', 'gramophone', 'furnace', 'metro_car_end', 'hotel_bed', 'boiler_tank',  # V12 L5
               'column_tuff', 'candelabrum', 'books_fallen',  # V12 L5.3
               'paving_strip', 'birch_circle', 'table_188', 'pillar_188',  # V12 L188
               'metro_car_903', 'metro_car_37', 'track_metro', 'platform_edge', 'stairs_barricaded',  # V12 L903
               'scaffold', 'wall_unfinished', 'cement_bags', 'rebar_bundle', 'work_light', 'cement_mixer', 'wheelbarrow', 'sawhorse', 'site_barrier', 'tarp_pile',  # V13.3 L1 Ouroboros
               'moss_patch', 'fern_cluster', 'roots_floor', 'shrub_concrete', 'sapling', 'overgrown_crate',  # V13.3 L1 Garden
               'cabinet_vermilion', 'table_vermilion', 'bench_vermilion', 'screen_vermilion',  # V13.3 L1 Fabled
               'fungus_floor', 'fungus_camp', 'goo_trail', 'plant_wanderer',  # V13.3 L1-Erscheinungen
               'xray_scanner', 'security_gate', 'tray_stack', 'lounge_chair', 'lounge_table',  # V13.3 L36
               'macchina_parts', 'macchina_tools',  # V13.3 L2
               'skin_shed', 'skin_lair', 'food_scraps',  # V13.3 L2: Spuren der Skin-Stealer (KREATUR_L2_Spuren.md), Futter der Death Rats
               'jerry_perch', 'jerry_shrine',  # V13.3 Teil 4 L2: Jerrys Sitzrohr, Opfertisch der Followers
               'macchina_suit', 'macchina_cradle', 'macchina_spear',  # V13.3 Teil 4 L2: Reste der Macchina (KREATUR_Macchina.md)
               'e18_desk', 'e18_wingchair',  # V13.3 Teil 4 L5: Buero des Gastgebers (KREATUR_Gastgeber.md)
               'carousel_36', 'luggage_pile', 'checkin_counter', 'bta_booth', 'travelator', 'travelator_stopped', 'seats_row', 'gate_counter', 'camp_c32'}  # V12 L36


class Map:
    def __init__(self, mid, w, h, seed=1):
        self.id, self.w, self.h, self.seed = mid, w, h, seed
        self.rooms, self.doors, self.spawns, self.objects, self.transitions = [], [], [], [], []
        self.checkpoints = []  # V11: Wiedereinstieg nach dem Tod
        self.blackout_circuits = set()  # V11: Stromkreise, die ausfallen koennen (Sicherungspunkte nie daran)
        self.styled_lit = set()
        self.start = (2.5, 2.5, 0.0)
        self.wrap = False
        self.rng = random.Random(seed)

    # --- Raeume -------------------------------------------------------------------------------
    def room(self, x0, y0, x1, y1, rtype, zone, floor=0.0, height=3.0, **kw):
        assert x1 > x0 and y1 > y0, (x0, y0, x1, y1)
        r = Room(len(self.rooms), int(x0), int(y0), int(x1), int(y1), rtype, zone, floor, height, kw)
        self.rooms.append(r)
        return r.idx

    def R(self, i):
        return self.rooms[i]

    def at(self, x, y):
        for r in self.rooms:
            if r.contains(x, y):
                return r.idx
        return -1

    # --- Tueren -------------------------------------------------------------------------------
    def _wall(self, a, b):
        """Gemeinsame Wand zwischen a und b: (axis, s, t, lo, hi) oder None."""
        A, B = self.rooms[a], self.rooms[b]
        if A.x1 <= B.x0 and B.x0 - A.x1 <= 4:
            lo, hi = max(A.y0, B.y0), min(A.y1, B.y1)
            if hi > lo:
                return 0, A.x1, B.x0 - A.x1, lo, hi, False
        if B.x1 <= A.x0 and A.x0 - B.x1 <= 4:
            lo, hi = max(A.y0, B.y0), min(A.y1, B.y1)
            if hi > lo:
                return 0, B.x1, A.x0 - B.x1, lo, hi, True
        if A.y1 <= B.y0 and B.y0 - A.y1 <= 4:
            lo, hi = max(A.x0, B.x0), min(A.x1, B.x1)
            if hi > lo:
                return 1, A.y1, B.y0 - A.y1, lo, hi, False
        if B.y1 <= A.y0 and A.y0 - B.y1 <= 4:
            lo, hi = max(A.x0, B.x0), min(A.x1, B.x1)
            if hi > lo:
                return 1, B.y1, A.y0 - B.y1, lo, hi, True
        return None

    def door(self, a, b, kind='door', width=1, at=None, top=None, style=''):
        w = self._wall(a, b)
        if not w:
            raise ValueError(f'{self.id}: Raeume {a} und {b} haben keine gemeinsame Wand')
        axis, s, t, lo, hi, swapped = w
        width = min(width, hi - lo)
        if at is None:
            p = (lo + hi - width) // 2
        else:
            p = int(round(at - width / 2))
        p = max(lo, min(hi - width, p))
        if axis == 0:
            rect = [s, p, s + t, p + width]
        else:
            rect = [p, s, p + width, s + t]
        ra, rb = (b, a) if swapped else (a, b)
        d = {'rect': rect, 'axis': axis, 'a': ra, 'b': rb, 'kind': kind}
        if top is not None:
            d['top'] = top
        if style:
            d['style'] = style
        self.doors.append(d)
        return len(self.doors) - 1

    def opening(self, a, b, width=None, at=None):
        w = self._wall(a, b)
        if width is None:
            width = w[4] - w[3]
        return self.door(a, b, 'opening', width, at)

    # --- Licht --------------------------------------------------------------------------------
    def light(self, ri, x, y, kind=0, intensity=0.8, radius=4.5, dx=0, dy=0, z=None, panel=True, color=None):
        l = [round(x, 2), round(y, 2), kind, intensity, radius, dx, dy]
        if z is not None or not panel or color:
            l += [round(z, 3) if z is not None else -999.0, 1 if panel else 0]
        if color:
            l += [round(c, 3) for c in color]
        self.rooms[ri].lights.append(l)

    @staticmethod
    def pillar_at(R, tx, ty):
        """V12: Stuetze des Rasters an der Kachel? Wie world/room.cpp (FeatKind::Pillars, Rand 1)."""
        sp, spy, th, px, py = (list(R.kw['pillars']) + [0, 0, 0, 0, 0])[:5]
        spy = spy or sp
        lx, ly = tx - R.x0, ty - R.y0
        if not (1 <= lx < R.w - 1 and 1 <= ly < R.h - 1):
            return False
        ox, oy = (px - R.x0) % sp, (py - R.y0) % spy
        return lx >= ox and ly >= oy and (lx - ox) % sp < th and (ly - oy) % spy < th

    # --- Bloecke --------------------------------------------------------------------------------
    def block(self, ri, x0, y0, x1, y1, h, hidden=True):
        """Block auf dem Raumboden (Weltkoordinaten, ganze Kacheln). hidden: unsichtbar, ein Modell der
        Ausstattung steht darauf (Hochregal) - Kollision, Schatten und Wege bleiben."""
        R = self.rooms[ri]
        assert R.x0 <= x0 < x1 <= R.x1 and R.y0 <= y0 < y1 <= R.y1, (ri, x0, y0, x1, y1)
        R.blocks.append([int(x0), int(y0), int(x1), int(y1), round(h, 3), 1 if hidden else 0])

    def blocked(self, x, y):
        """In einem unbetretbaren Block: unsichtbar (Wagen, Regal) oder eine Grube; sichtbare Podeste,
        Beckenraender und Stufen sind begehbarer Boden. V12: auch in einer Stuetze des Rasters (kw pillars)."""
        for r in self.rooms:
            for b in r.blocks:
                if b[0] <= x < b[2] and b[1] <= y < b[3] and (b[5] == 1 or b[4] < 0):
                    return True
            if 'pillars' in r.kw and r.x0 <= x < r.x1 and r.y0 <= y < r.y1 and self.pillar_at(r, math.floor(x), math.floor(y)):
                return True
        return False

    # --- Wandflaechen (V11) ----------------------------------------------------------------------
    def in_door(self, x, y):
        for d in self.doors:
            x0, y0, x1, y1 = d['rect']
            if x0 <= x < x1 and y0 <= y < y1:
                return d
        return None

    def wall_faces(self, ri, inset=0.005, margin=0):
        """Kacheln am Rand von Raum ri, hinter denen feste Wand liegt: Liste (x, y, yaw) - Punkt auf der
        Wandflaeche (inset in den Raum), Blick in den Raum. Keine Tueren, Oeffnungen, Nachbarraeume oder Bloecke;
        margin haelt Abstand zu Wandenden (Kacheln neben einer Oeffnung zaehlen nicht)."""
        R = self.rooms[ri]
        out = []

        def solid(x, y):
            return self.at(x + 0.5, y + 0.5) < 0 and self.in_door(x, y) is None

        def blocked_tile(x, y):
            for b in R.blocks:
                if b[0] <= x < b[2] and b[1] <= y < b[3]:
                    return True
            return False

        sides = [  # (Kacheln innen, Kachel dahinter, Punkt, Blick)
            ([(R.x0, y) for y in range(R.y0, R.y1)], lambda x, y: (x - 1, y), lambda x, y: (x + inset, y + 0.5), 0.0),
            ([(R.x1 - 1, y) for y in range(R.y0, R.y1)], lambda x, y: (x + 1, y), lambda x, y: (x + 1 - inset, y + 0.5), math.pi),
            ([(x, R.y0) for x in range(R.x0, R.x1)], lambda x, y: (x, y - 1), lambda x, y: (x + 0.5, y + inset), math.pi / 2),
            ([(x, R.y1 - 1) for x in range(R.x0, R.x1)], lambda x, y: (x, y + 1), lambda x, y: (x + 0.5, y + 1 - inset), -math.pi / 2),
        ]
        for tiles, behind, point, yaw in sides:
            ok = [solid(*behind(x, y)) and not blocked_tile(x, y) for x, y in tiles]
            for k, (x, y) in enumerate(tiles):
                if all(0 <= k + d < len(tiles) and ok[k + d] for d in range(-margin, margin + 1)):
                    px, py = point(x, y)
                    out.append((px, py, yaw))
        return out

    @staticmethod
    def lintel(d):
        """Hoehe des Sturzes einer Tuer ueber der Schwelle (None: Oeffnung bis zur Decke, kein Sturz).
        Wie die Engine (generator.cpp): Oeffnungen reichen immer bis zur Decke; Tueren 2,2 m, sonst 'top'."""
        if d['kind'] == 'opening':
            return None
        return d.get('top', 2.2 if d['kind'] == 'door' else 2.5)

    def block_wall(self, x, y, z):
        """V12: Steht bei (x, y) ein Block, der bis mindestens 0,3 m ueber z reicht? Dann traegt er Wandteile
        (frei stehende Regale: Block mit Regalen auf beiden Seiten)."""
        ri = self.at(x, y)
        if ri < 0:
            return False
        R = self.rooms[ri]
        return any(b[0] <= x < b[2] and b[1] <= y < b[3] and R.floor + b[4] >= z + 0.3 for b in R.blocks)

    def wall_ok(self, x, y, yaw, rel_z=1.0):
        """Steht hinter (x, y) - Blick yaw - feste Wand? Kein Nachbarraum; vor einer Tuer nur ueber ihrem Sturz
        (Oeffnungen haben keinen)."""
        c, s = math.cos(yaw), math.sin(yaw)
        bx, by = x - c * 0.25, y - s * 0.25
        if self.at(bx, by) >= 0:
            return False
        d = self.in_door(math.floor(bx), math.floor(by))
        if d is None:
            return True
        top = self.lintel(d)
        return top is not None and rel_z > top + 0.08

    # --- Belegung -------------------------------------------------------------------------------
    def spawn(self, kind, x, y, z=0.0, yaw=0.0, tags=(), role='', set='', group=-1, weight=1.0):
        s = {'kind': kind, 'pos': [round(x, 2), round(y, 2), round(z, 2)]}
        if yaw:
            s['yaw'] = round(yaw, 4)
        if tags:
            s['tags'] = list(tags)
        if role:
            s['role'] = role
        if set:
            s['set'] = set
        if group >= 0:
            s['group'] = group
        if weight != 1.0:
            s['weight'] = weight
        self.spawns.append(s)

    def obj(self, model, x, y, z=0.0, yaw=0.0, scale=None, variant=0, tag=''):
        o = {'model': model, 'pos': [round(x, 3), round(y, 3), round(z, 3)]}
        if yaw:
            o['yaw'] = round(yaw, 4)
        if scale:
            o['scale'] = list(scale)
        if variant:
            o['variant'] = variant
        if tag:
            o['tag'] = tag
        self.objects.append(o)

    def transition(self, tid, kind, target, x, y, yaw=0.0, rect=None, requires=''):
        t = {'id': tid, 'kind': kind, 'target': target, 'pos': [round(x, 2), round(y, 2), round(yaw, 4)]}
        if rect:
            t['rect'] = list(rect)
        if requires:
            t['requires'] = requires
        self.transitions.append(t)

    # --- Sicherungspunkte (V11) ------------------------------------------------------------------
    def checkpoint(self, cid, x, y, yaw=0.0, label='', lit_by_style=False):
        """Sicherungspunkt: wer vorbeigeht, speichert hier; nach dem Tod geht es hier weiter.

        Regeln (prueft validate()): in einem beleuchteten Raum, der nicht am Stromkreis eines offenen
        Decks haengt (ein Stromausfall dort laesst ihn hell), nicht in einem Block oder Uebergang,
        mindestens 15 m von Spawnpunkten umherstreifender Kreaturen (Hund, Buendel).
        """
        self.checkpoints.append({'id': cid, 'pos': [round(x, 2), round(y, 2), round(yaw, 4)], 'label': label})
        if lit_by_style:  # Leuchten kommen aus dem Lichtstil der Zone (z. B. Deckenfelder), nicht aus der Karte
            self.styled_lit.add(cid)

    # --- Pruefen und Speichern ------------------------------------------------------------------
    def validate(self):
        errs = []
        occ = {}
        for r in self.rooms:
            if r.x0 < 1 or r.y0 < 1 or r.x1 > self.w - 1 or r.y1 > self.h - 1:
                errs.append(f'Raum {r.idx} ragt aus der Karte')
            for y in range(r.y0 - 1, r.y1 + 1):
                for x in range(r.x0 - 1, r.x1 + 1):
                    inner = r.x0 <= x < r.x1 and r.y0 <= y < r.y1
                    o = occ.get((x, y))
                    if inner:
                        if o is not None and o[1]:
                            errs.append(f'Raeume {o[0]} und {r.idx} ueberlappen bei {x},{y}')
                            break
                        occ[(x, y)] = (r.idx, True)
            if len(errs) > 20:
                break
        # Wand dazwischen: kein Raum darf direkt an einen anderen grenzen
        for r in self.rooms:
            for x in range(r.x0, r.x1):
                for y in (r.y0 - 1, r.y1):
                    o = occ.get((x, y))
                    if o and o[1] and o[0] != r.idx:
                        errs.append(f'Raeume {r.idx} und {o[0]} ohne Wand bei {x},{y}')
            for y in range(r.y0, r.y1):
                for x in (r.x0 - 1, r.x1):
                    o = occ.get((x, y))
                    if o and o[1] and o[0] != r.idx:
                        errs.append(f'Raeume {r.idx} und {o[0]} ohne Wand bei {x},{y}')
        # Erreichbarkeit
        adj = {i: set() for i in range(len(self.rooms))}
        for d in self.doors:
            adj[d['a']].add(d['b'])
            adj[d['b']].add(d['a'])
        s = self.at(self.start[0], self.start[1])
        if s < 0:
            errs.append('Startpunkt liegt in keinem Raum')
        else:
            seen, stack = {s}, [s]
            while stack:
                n = stack.pop()
                for m in adj[n]:
                    if m not in seen:
                        seen.add(m)
                        stack.append(m)
            for r in self.rooms:
                if r.idx not in seen and not r.kw.get('sealed'):
                    errs.append(f'Raum {r.idx} ({r.type}, {r.kw.get("label", "")}) nicht erreichbar')
        for s_ in self.spawns:
            if self.at(s_['pos'][0], s_['pos'][1]) < 0:
                errs.append(f'Spawnpunkt {s_["kind"]} bei {s_["pos"]} liegt in keinem Raum')
            elif self.blocked(s_['pos'][0], s_['pos'][1]):
                errs.append(f'Spawnpunkt {s_["kind"]} bei {s_["pos"]} liegt in einem Block')
        for t in self.transitions:
            if self.at(t['pos'][0], t['pos'][1]) < 0:
                errs.append(f'Uebergang {t["id"]} liegt in keinem Raum')
        # Halt der Ausstattung (V11): Wandteile an Wand, Bodenteile am Boden, Deckenteile an der Decke
        for o in self.objects:
            md, (x, y, z) = o['model'], o['pos']
            yaw = o.get('yaw', 0.0)
            c, s = math.cos(yaw), math.sin(yaw)
            where = f'{md} bei ({x:.2f}, {y:.2f}, {z:.2f})'
            if md in WALL_MOUNT:
                bx, by = x - c * 0.25, y - s * 0.25  # sicher in der (1 m dicken) Wand, auch bei Teilen mit Tiefe
                fx, fy = x + c * 0.15, y + s * 0.15
                d = self.in_door(math.floor(bx), math.floor(by))
                if md in DOOR_MODELS and (d or self.in_door(math.floor(x), math.floor(y))):
                    continue
                own = self.at(fx, fy)
                rel = z - (self.rooms[own].floor if own >= 0 else 0.0)
                top = self.lintel(d) if d else None
                if self.at(bx, by) >= 0 and not self.block_wall(bx, by, z):
                    errs.append(f'{where}: hinter dem Wandteil ist Raum, keine Wand')
                elif d and (top is None or rel <= top + 0.08):
                    errs.append(f'{where}: haengt in einer Tuer/Oeffnung (kein Sturz dahinter)')
                elif self.at(fx, fy) < 0 and not self.in_door(math.floor(fx), math.floor(fy)):
                    errs.append(f'{where}: steckt in der Wand (davor kein Raum)')
            elif md in IN_OPENING:
                if not self.in_door(math.floor(x), math.floor(y)):
                    errs.append(f'{where}: steht nicht in einem Durchbruch')
            elif md in FLOOR_MOUNT or md in CEILING_MOUNT:
                ri = self.at(x, y)
                if ri < 0:
                    errs.append(f'{where}: in keinem Raum')
                    continue
                R = self.rooms[ri]
                if md in FLOOR_MOUNT:
                    tops = [R.floor] + [R.floor + b[4] for b in R.blocks if b[0] <= x < b[2] and b[1] <= y < b[3]]
                    if min(abs(z - t) for t in tops) > 0.06:
                        errs.append(f'{where}: steht nicht auf dem Boden (Boden {R.floor:.2f})')
                elif abs(z - (R.floor + R.height)) > 0.06:
                    errs.append(f'{where}: haengt nicht an der Decke (Decke {R.floor + R.height:.2f})')
        # Sicherungspunkte (V11)
        deck_circuits = {r.kw.get('circuit') for r in self.rooms if r.type == 'deck'} | self.blackout_circuits
        roamers = [s_['pos'] for s_ in self.spawns if s_['kind'] == 'entity' and s_.get('role') in ('hound', 'clump')]
        ids = set()
        for c in self.checkpoints:
            x, y = c['pos'][0], c['pos'][1]
            ri = self.at(x, y)
            where = f'Sicherungspunkt {c["id"]}'
            if c['id'] in ids:
                errs.append(f'{where}: id doppelt')
            ids.add(c['id'])
            if ri < 0:
                errs.append(f'{where} liegt in keinem Raum')
                continue
            R = self.rooms[ri]
            if R.type == 'deck' or (not R.lights and c['id'] not in self.styled_lit):
                errs.append(f'{where}: im offenen Deck oder ohne Licht')
            if R.kw.get('circuit') in deck_circuits:
                errs.append(f'{where}: am Stromkreis eines Decks (wuerde im Stromausfall dunkel)')
            if self.blocked(x, y):
                errs.append(f'{where}: in einem Block')
            for t in self.transitions:
                rc = t.get('rect')
                if rc and rc[0] <= x < rc[2] and rc[1] <= y < rc[3]:
                    errs.append(f'{where}: im Uebergang {t["id"]}')
            for p in roamers:
                if math.hypot(p[0] - x, p[1] - y) < 15.0:
                    errs.append(f'{where}: naeher als 15 m an einem Spawnpunkt (Hund/Buendel) bei {p[:2]}')
        # jede Ankunft (Uebergang, ausser Loechern) hat einen Sicherungspunkt in der Naehe
        if self.checkpoints:
            for t in self.transitions:
                if t['kind'] in ('hole', 'dive', 'end'):  # Einbahnwege hinab und das Ende
                    continue
                if not t['target']:  # V12: nur Ankunft - ohne Sicherungspunkt beginnt man wieder am Ankunftsort
                    continue
                d = min(math.hypot(c['pos'][0] - t['pos'][0], c['pos'][1] - t['pos'][1]) for c in self.checkpoints)
                if d > 16.0:
                    errs.append(f'Uebergang {t["id"]}: kein Sicherungspunkt in 16 m ({d:.0f} m)')
        return errs

    def to_json(self):
        rooms = []
        for r in self.rooms:
            o = {'rect': [r.x0, r.y0, r.x1, r.y1], 'type': r.type, 'zone': r.zone,
                 'floor': round(r.floor, 3), 'height': round(r.height, 3)}
            kw = dict(r.kw)
            kw.pop('sealed', None)
            if 'pillars' in kw:
                o['pillars'] = list(kw.pop('pillars'))
            for k in ('light', 'circuit', 'flicker', 'ambient', 'label', 'tags', 'seed', 'beams', 'water', 'water_look'):
                if k in kw:
                    o[k] = kw.pop(k)
            if r.lights:
                o['lights'] = r.lights
            if r.blocks:
                o['blocks'] = r.blocks
            rooms.append(o)
        return {'id': self.id, 'size': [self.w, self.h], 'seed': self.seed,
                'start': [self.start[0], self.start[1], self.start[2]], 'wrap': self.wrap,
                'rooms': rooms, 'doors': self.doors, 'spawns': self.spawns, 'objects': self.objects,
                'transitions': self.transitions, 'checkpoints': self.checkpoints}

    def save(self, preview=True):
        errs = self.validate()
        if errs:
            for e in errs[:40]:
                print('FEHLER:', e)
            raise SystemExit(f'{self.id}: {len(errs)} Fehler')
        path = os.path.join(ROOT, 'data', 'maps', self.id + '.json')
        with open(path, 'w', encoding='utf-8') as f:
            f.write('// LIMINAL V13 - feste Karte. Erzeugt von tools/levels/' + self.id + '.py, nicht von Hand aendern.\n')
            json.dump(self.to_json(), f, ensure_ascii=False, separators=(',', ':'))
        print(f'{self.id}: {len(self.rooms)} Raeume, {len(self.doors)} Tueren, {len(self.spawns)} Spawnpunkte, '
              f'{len(self.objects)} Objekte, {len(self.transitions)} Uebergaenge, {len(self.checkpoints)} Sicherungspunkte -> {path}')
        if preview:
            self.preview(os.path.join(ROOT, 'tools', 'levels', 'preview', self.id + '.png'))

    def preview(self, path, colors=None):
        try:
            import matplotlib
            matplotlib.use('Agg')
            import matplotlib.pyplot as plt
            import matplotlib.patches as pa
        except ImportError:
            return
        os.makedirs(os.path.dirname(path), exist_ok=True)
        zones = sorted({r.zone for r in self.rooms})
        pal = plt.get_cmap('tab20')
        zc = {z: pal(i % 20) for i, z in enumerate(zones)}
        if colors:
            zc.update(colors)
        fig, ax = plt.subplots(figsize=(14, 14 * self.h / self.w))
        ax.set_facecolor('#151515')
        for r in self.rooms:
            ax.add_patch(pa.Rectangle((r.x0, r.y0), r.w, r.h, color=zc[r.zone], lw=0))
            if r.kw.get('label') and r.w * r.h > 60:
                ax.text(r.cx, r.cy, r.kw['label'], ha='center', va='center', fontsize=6, color='black')
            sp = r.kw.get('pillars')
            if sp:
                sx, sy, th, ox, oy = sp
                x = ox - ((ox - r.x0) // sx) * sx
                while x < r.x1:
                    y = oy - ((oy - r.y0) // sy) * sy
                    while y < r.y1:
                        if r.x0 + 1 <= x < r.x1 - 1 and r.y0 + 1 <= y < r.y1 - 1:
                            ax.add_patch(pa.Rectangle((x, y), th, th, color='#333333', lw=0))
                        y += sy
                    x += sx
            for b in r.blocks:
                ax.add_patch(pa.Rectangle((b[0], b[1]), b[2] - b[0], b[3] - b[1], color='#24407a', lw=0))
            for l in r.lights:
                ax.plot(l[0], l[1], marker='.', color='#ffffaa', ms=2)
        for c in self.checkpoints:
            ax.plot(c['pos'][0], c['pos'][1], marker='*', color='#33ff99', ms=7)
        for d in self.doors:
            x0, y0, x1, y1 = d['rect']
            ax.add_patch(pa.Rectangle((x0, y0), x1 - x0, y1 - y0,
                                      color='#ff9933' if d['kind'] != 'opening' else '#ffe680', lw=0))
        mk = {'item': ('o', '#66ccff'), 'note': ('s', '#ffffff'), 'entity': ('^', '#ff3333'),
              'deco': ('x', '#aaaaaa'), 'battery': ('o', '#33ff66'), 'fusebox': ('D', '#ff8800'),
              'crate': ('s', '#aa7744')}
        for s in self.spawns:
            m, c = mk.get(s['kind'], ('.', '#ff00ff'))
            ax.plot(s['pos'][0], s['pos'][1], marker=m, color=c, ms=3)
        for t in self.transitions:
            ax.plot(t['pos'][0], t['pos'][1], marker='*', color='#00ffff', ms=12)
            ax.text(t['pos'][0], t['pos'][1] + 2, t['id'], color='#00ffff', fontsize=7)
        ax.plot(self.start[0], self.start[1], marker='P', color='#00ff00', ms=12)
        ax.set_xlim(0, self.w)
        ax.set_ylim(0, self.h)
        ax.set_aspect('equal')
        handles = [pa.Patch(color=zc[z], label=z) for z in zones]
        ax.legend(handles=handles, loc='upper left', fontsize=6, ncol=2, framealpha=0.7)
        ax.set_title(self.id)
        plt.tight_layout()
        plt.savefig(path, dpi=90)
        plt.close(fig)


# --- Bausteine fuer wiederkehrende Strukturen -----------------------------------------------------
def corridor_maze(m, x0, y0, x1, y1, cell, cw, rtype, zone, room_types, room_zone, height=2.7, floor=0.0,
                  density=0.75, room_chance=0.7, rng=None, circuit=-1, light_every=6):
    """Gaenge als Gitter mit Kreuzungen; in den Zellen kleine Raeume mit Tuer zum Gang.

    Gibt (junctions, segments, cellrooms) zurueck: dicts (i, j) -> Raumindex.
    Ein Teil der Gangstuecke faellt weg (density), der Rest bleibt zusammenhaengend (Spannbaum
    plus Schleifen). Alles wird einmal beim Entwurf erzeugt und dann als feste Karte gespeichert.
    """
    rng = rng or m.rng
    nx = (x1 - x0 - cw) // cell
    ny = (y1 - y0 - cw) // cell
    J, S, C = {}, {}, {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            jx, jy = x0 + i * cell, y0 + j * cell
            J[(i, j)] = None
    # Spannbaum ueber die Kreuzungen + zusaetzliche Kanten
    edges = []
    for i in range(nx + 1):
        for j in range(ny + 1):
            if i < nx:
                edges.append(((i, j), (i + 1, j)))
            if j < ny:
                edges.append(((i, j), (i, j + 1)))
    rng.shuffle(edges)
    parent = {k: k for k in J}

    def find(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k
    keep = []
    for a, b in edges:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb
            keep.append((a, b))
        elif rng.random() < density - 0.5:
            keep.append((a, b))
    used = set()
    for a, b in keep:
        used.add(a)
        used.add(b)
    for k in used:
        i, j = k
        jx, jy = x0 + i * cell, y0 + j * cell
        J[k] = m.room(jx, jy, jx + cw, jy + cw, rtype, zone, floor, height, label='', circuit=circuit,
                      tags=['junction'])
    for a, b in keep:
        (i0, j0), (i1, j1) = a, b
        if j0 == j1:  # waagrecht
            sx0 = x0 + i0 * cell + cw + 1
            sx1 = x0 + i1 * cell - 1
            sy = y0 + j0 * cell
            r = m.room(sx0, sy, sx1, sy + cw, rtype, zone, floor, height, circuit=circuit, tags=['segment'])
        else:
            sy0 = y0 + j0 * cell + cw + 1
            sy1 = y0 + j1 * cell - 1
            sx = x0 + i0 * cell
            r = m.room(sx, sy0, sx + cw, sy1, rtype, zone, floor, height, circuit=circuit, tags=['segment'])
        S[(a, b)] = r
        m.opening(J[a], r)
        m.opening(r, J[b])
        # Leuchten im Gang (schwach, unregelmaessig)
        R = m.R(r)
        if R.w > R.h:
            for x in range(R.x0 + 2, R.x1 - 1, light_every):
                if rng.random() < 0.8:
                    m.light(r, x + 0.5, R.cy, 0, 0.55, 3.6)
        else:
            for y in range(R.y0 + 2, R.y1 - 1, light_every):
                if rng.random() < 0.8:
                    m.light(r, R.cx, y + 0.5, 0, 0.55, 3.6)
    # Zellen: kleine Raeume, Tuer zu einem angrenzenden Gangstueck
    for i in range(nx):
        for j in range(ny):
            if rng.random() > room_chance:
                continue
            cx0, cy0 = x0 + i * cell + cw + 1, y0 + j * cell + cw + 1
            cx1, cy1 = x0 + (i + 1) * cell - 1, y0 + (j + 1) * cell - 1
            if cx1 - cx0 < 3 or cy1 - cy0 < 3:
                continue
            nbrs = []
            for a, b in keep:
                (i0, j0), (i1, j1) = a, b
                if j0 == j1 and min(i0, i1) == i and j0 in (j, j + 1):
                    nbrs.append(S[(a, b)])
                if i0 == i1 and min(j0, j1) == j and i0 in (i, i + 1):
                    nbrs.append(S[(a, b)])
            if not nbrs:
                continue
            rt = rng.choice(room_types)
            rr = m.room(cx0, cy0, cx1, cy1, rt, room_zone(rt) if callable(room_zone) else room_zone, floor,
                        height + 0.1, circuit=circuit, label='')
            m.door(rr, rng.choice(nbrs), 'door', 1)
            C[(i, j)] = rr
    return J, S, C


# --- Raumteilung (V11, Level 0 und andere Innenraum-Ebenen) --------------------------------------------
def partition_rooms(m, x0, y0, x1, y1, zone, pick_type, min_leaf=3, max_leaf=12, height=(2.6, 3.0), floor=0.0,
                    stop_chance=0.3, loop_chance=0.25, width=(1, 3), rng=None, **room_kw):
    """Teilt das Rechteck rekursiv (BSP) in Raeume; zwischen zwei Teilen liegt 1 m Wand.

    pick_type(w, h, rng) -> Raumtyp. Jede Teilung bekommt mindestens eine Oeffnung zwischen ihren
    Haelften (alles bleibt zusammenhaengend), weitere mit loop_chance (Schleifen statt Sackgassen).
    Gibt die Liste der Raumindizes zurueck. Einmal beim Entwurf erzeugt, danach feste Karte.
    """
    rng = rng or m.rng

    def make(rx0, ry0, rx1, ry1):
        w, h = rx1 - rx0, ry1 - ry0
        hh = height[0] + (height[1] - height[0]) * rng.random() if isinstance(height, tuple) else height
        return [m.room(rx0, ry0, rx1, ry1, pick_type(w, h, rng), zone, floor, round(hh, 2), **room_kw)]

    def pairs(A, B):
        out = []
        for a in A:
            for b in B:
                wl = m._wall(a, b)
                if wl and wl[2] == 1 and wl[4] - wl[3] >= 1:
                    out.append((a, b, wl[4] - wl[3]))
        return out

    def link(A, B):
        ps = pairs(A, B)
        if not ps:
            return False
        rng.shuffle(ps)
        a, b, span = ps[0]
        wmax = min(span, width[1])
        m.opening(a, b, width=rng.randint(min(width[0], wmax), wmax), at=None if span <= wmax else
                  m._wall(a, b)[3] + rng.randint(wmax // 2, span - (wmax + 1) // 2))
        for a, b, span in ps[1:]:
            if rng.random() < loop_chance / max(1.0, len(ps) ** 0.5):
                wmax = min(span, width[1])
                m.opening(a, b, width=rng.randint(min(width[0], wmax), wmax))
        return True

    def split(rx0, ry0, rx1, ry1, depth):
        w, h = rx1 - rx0, ry1 - ry0
        can_x = w >= 2 * min_leaf + 1
        can_y = h >= 2 * min_leaf + 1
        if not can_x and not can_y:
            return make(rx0, ry0, rx1, ry1)
        if max(w, h) <= max_leaf and depth > 0 and rng.random() < stop_chance:
            return make(rx0, ry0, rx1, ry1)
        along_x = can_x and (not can_y or w > h * 1.2 or (h <= w * 1.2 and rng.random() < 0.5))
        for _ in range(4):
            if along_x:
                s = rng.randint(rx0 + min_leaf, rx1 - min_leaf - 1)
                A, B = split(rx0, ry0, s, ry1, depth + 1), split(s + 1, ry0, rx1, ry1, depth + 1)
            else:
                s = rng.randint(ry0 + min_leaf, ry1 - min_leaf - 1)
                A, B = split(rx0, ry0, rx1, s, depth + 1), split(rx0, s + 1, rx1, ry1, depth + 1)
            if link(A, B):
                return A + B
            raise SystemExit(f'partition_rooms: keine Oeffnung zwischen den Haelften bei {rx0},{ry0}')
        return []

    return split(x0, y0, x1, y1, 0)


def link_regions(m, A, B, count, width=(1, 2), rng=None):
    """count Oeffnungen zwischen zwei benachbarten Bereichen (Raumlisten), moeglichst verteilt."""
    rng = rng or m.rng
    ps = []
    for a in A:
        for b in B:
            wl = m._wall(a, b)
            if wl and wl[2] == 1 and wl[4] - wl[3] >= max(1, width[0]):
                ps.append((a, b, wl))
    if not ps:
        raise SystemExit('link_regions: Bereiche grenzen nicht aneinander')
    rng.shuffle(ps)
    done, used = 0, set()
    for a, b, wl in ps:
        if done >= count:
            break
        if a in used or b in used:
            continue
        span = wl[4] - wl[3]
        m.opening(a, b, width=rng.randint(min(width[0], span), min(width[1], span)))
        used.add(a)
        used.add(b)
        done += 1
    return done
