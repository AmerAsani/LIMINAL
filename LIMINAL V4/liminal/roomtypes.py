"""Katalog der Raumtypen und ihre Auswahlwahrscheinlichkeiten.

Grundverteilung (entspricht grob der Vorgabe):
    ~60 % normale Raeume, ~20 % Korridore (ergeben sich aus der BSP-Teilung),
    ~10 % grosse Hallen, ~7 % ungewoehnliche Raeume, ~3 % extrem seltene Raeume.

Die Wahrscheinlichkeiten fuer Ungewoehnliches steigen mit der Entfernung zum
Startpunkt ("rarity"). Das ist deterministisch - die Welt bleibt reproduzierbar,
wird aber seltsamer, je weiter man sich hinauswagt.
"""

NORMAL, CORRIDOR, LARGE, UNUSUAL, RARE = range(5)
CATEGORY_NAMES = ("normal", "Korridor", "gross", "ungewoehnlich", "selten")


class RoomType:
    def __init__(self, key, label, category, short=(2, 999), long=(2, 999),
                 affinity=(1, 1, 1, 1, 1), height=None, features=(), light=None,
                 sunken=None, extra_doors=None, multi_doors=False, ambient_mul=1.0,
                 fog_mul=1.0, stair_step=0.3):
        self.key = key
        self.label = label
        self.category = category
        self.short = short
        self.long = long
        self.affinity = affinity          # Gewicht je Zone (Buero, Industrie, Tunnel, Hallen, Wartung)
        self.height = height              # None = Zonenwert der Kategorie
        self.features = features
        self.light = light                # None = Lichtstil der Zone
        self.sunken = sunken              # (Wahrscheinlichkeit, min Tiefe, max Tiefe)
        self.extra_doors = extra_doors    # Wahrscheinlichkeit zusaetzlicher Tueren
        self.multi_doors = multi_doors    # mehrere identische Tueren pro Nachbar
        self.ambient_mul = ambient_mul
        self.fog_mul = fog_mul
        self.stair_step = stair_step

    def fits(self, short, long):
        return (self.short[0] <= short <= self.short[1]) and (self.long[0] <= long <= self.long[1])


T = RoomType
CATALOG = {t.key: t for t in [
    # --- normale Raeume -------------------------------------------------------
    T("zimmer", "Leeres Zimmer", NORMAL, short=(3, 9), long=(3, 14), affinity=(1.0, 0.5, 0.4, 0.2, 0.6)),
    T("buero", "Grossraumbuero", NORMAL, short=(7, 26), long=(8, 40), affinity=(1.6, 0.2, 0.0, 0.3, 0.0),
      features=("cubicles",)),
    T("betonraum", "Betonraum", NORMAL, short=(2, 999), affinity=(0.08, 1.4, 0.5, 0.2, 0.5),
      features=("beams",)),
    T("lager", "Lagerraum", NORMAL, short=(8, 30), long=(9, 60), affinity=(0.2, 1.0, 0.2, 0.5, 0.1),
      features=("shelves",)),
    T("verteiler", "Verteilerraum", NORMAL, short=(3, 7), long=(3, 8), affinity=(0.5, 0.5, 0.6, 0.3, 0.7),
      extra_doors=0.9),
    # V3: kleine Teekueche / Versorgungsraum - hier liegen (selten) Items auf der Theke
    T("versorgung", "Versorgungsraum", NORMAL, short=(3, 6), long=(4, 9), affinity=(0.30, 0.25, 0.21, 0.16, 0.27),
      features=("counter",), ambient_mul=1.15),
    T("kammer", "Offene Kammer", NORMAL, short=(10, 40), affinity=(0.2, 0.7, 0.4, 1.2, 0.0),
      height=(5.0, 9.0), features=("pillars_sparse",)),
    T("gewoelbe", "Gewoelbekeller", NORMAL, short=(4, 12), affinity=(0.0, 0.2, 1.8, 0.0, 0.1),
      features=("vault", "pillars")),
    T("technikraum", "Technikraum", NORMAL, short=(3, 12), long=(3, 16), affinity=(0.1, 0.6, 0.3, 0.0, 1.8),
      features=("machines", "pipes")),
    # --- Korridore (entstehen als BSP-Streifen) -------------------------------
    T("korridor", "Korridor", CORRIDOR, affinity=(1.0, 0.4, 0.2, 0.2, 0.2)),
    T("breiter_korridor", "Breiter Korridor", CORRIDOR, short=(3, 999), affinity=(0.3, 1.0, 0.2, 0.8, 0.0),
      features=("beams",)),
    T("kolonnade", "Saeulengang", CORRIDOR, short=(4, 999), affinity=(0.1, 0.4, 0.2, 1.2, 0.0),
      features=("colonnade",)),
    T("tunnel", "Tunnel", CORRIDOR, short=(2, 5), affinity=(0.0, 0.2, 1.6, 0.0, 0.2),
      features=("vault",)),
    T("wartungsgang", "Wartungstunnel", CORRIDOR, short=(1, 3), affinity=(0.0, 0.2, 0.2, 0.0, 1.6),
      features=("pipes",)),
    T("endlos", "Endloser Gang", CORRIDOR, affinity=(1, 1, 1, 1, 1), extra_doors=0.85, multi_doors=True,
      light="panels_long"),
    # --- grosse Hallen ---------------------------------------------------------
    T("halle", "Grosse Halle", LARGE, short=(12, 999), affinity=(0.4, 0.6, 0.2, 1.5, 0.0),
      features=("thick_pillars",)),
    T("industriehalle", "Industriehalle", LARGE, short=(12, 999), affinity=(0.0, 1.6, 0.2, 0.5, 0.2),
      features=("beams", "crates")),
    T("saeulenhalle", "Saeulenhalle", LARGE, short=(10, 999), affinity=(0.3, 0.6, 0.8, 1.0, 0.0),
      features=("pillars",)),
    T("versenkte_halle", "Abgesenkte Halle", LARGE, short=(12, 999), affinity=(0.3, 0.8, 0.8, 0.8, 0.0),
      features=("pillars_sparse",), sunken=(1.0, 1.2, 2.4)),
    T("lagerhalle", "Lagerhalle", LARGE, short=(12, 999), affinity=(0.1, 1.2, 0.1, 0.6, 0.0),
      features=("shelves", "beams")),
    # --- ungewoehnliche Raeume ------------------------------------------------
    T("treppenhaus", "Treppenhaus", UNUSUAL, short=(8, 40), long=(10, 60), affinity=(0.8, 1.0, 1.0, 0.8, 0.4),
      height=(9.0, 15.0), sunken=(1.0, 3.2, 6.0), stair_step=0.4, light="wall"),
    T("hoher_raum", "Ungewoehnlich hoher Raum", UNUSUAL, short=(3, 10), long=(3, 16),
      affinity=(0.35, 0.35, 0.35, 0.35, 0.35), height=(14.0, 26.0)),
    T("niedriger_raum", "Niedrige Weite", UNUSUAL, short=(9, 999), affinity=(1.0, 0.8, 0.6, 0.6, 0.6),
      height=(2.05, 2.15), features=("pillars",), ambient_mul=0.8),
    T("tuerenraum", "Raum der Tueren", UNUSUAL, short=(4, 14), long=(5, 22), affinity=(1.2, 0.6, 0.6, 0.6, 0.6),
      extra_doors=1.0, multi_doors=True),
    T("podium", "Halle mit Podest", UNUSUAL, short=(10, 999), affinity=(0.6, 0.8, 0.3, 1.0, 0.0),
      height=(5.0, 9.0), features=("podium",)),
    T("schachbrett", "Schachbrettraum", UNUSUAL, short=(5, 20), affinity=(1.0, 0.4, 0.3, 0.8, 0.2),
      height=(3.0, 4.2), features=("checker",)),
    # --- extrem seltene Raeume --------------------------------------------------
    T("leere", "Die Leere", RARE, short=(14, 999), affinity=(1, 1, 1, 1.5, 0.5),
      height=(26.0, 40.0), features=("giant_pillars",), light="void", fog_mul=0.55),
    T("monolith", "Monolith", RARE, short=(12, 999), affinity=(1, 1, 1, 1, 0.5),
      height=(14.0, 22.0), features=("monolith",), light="pools"),
    T("lichtraum", "Ueberbelichteter Raum", RARE, short=(4, 30), affinity=(1, 1, 1, 1, 1),
      light="bright", ambient_mul=1.0, fog_mul=0.6),
    T("dunkelraum", "Dunkelkammer", RARE, short=(5, 40), affinity=(1, 1, 1, 1, 1),
      light="dark"),
]}

_BY_CATEGORY = {}
for _t in CATALOG.values():
    _BY_CATEGORY.setdefault(_t.category, []).append(_t)

# Korridortyp je Zone (bei schmalen Streifen)
_CORRIDOR_KEYS = ("korridor", "breiter_korridor", "kolonnade", "tunnel", "wartungsgang")


def _affinity(t, zone_w):
    return sum(a * w for a, w in zip(t.affinity, zone_w)) + 1e-4


def choose_type(rng, w, h, corridor, big, endless, zone_w, rarity):
    """Waehlt einen Raumtyp fuer ein BSP-Blatt der Groesse w x h."""
    short, long = min(w, h), max(w, h)
    if corridor or (short <= 2 and long >= 6):
        if endless:
            return CATALOG["endlos"]
        cands = [CATALOG[k] for k in _CORRIDOR_KEYS if CATALOG[k].fits(short, long)]
        if not cands:
            cands = [CATALOG["korridor"]]
        return rng.weighted(cands, [_affinity(t, zone_w) for t in cands])

    r = rng.random()
    p_rare = 0.03 * rarity
    p_unusual = 0.07 * rarity
    if r < p_rare:
        order = (RARE, UNUSUAL, LARGE, NORMAL)
    elif r < p_rare + p_unusual:
        order = (UNUSUAL, LARGE, NORMAL)
    elif big or short >= 18:
        order = (LARGE, NORMAL)
    else:
        order = (NORMAL,)
    for cat in order:
        cands = [t for t in _BY_CATEGORY[cat] if t.fits(short, long)]
        if cands:
            return rng.weighted(cands, [_affinity(t, zone_w) for t in cands])
    return CATALOG["betonraum"]
