#include "world/level_def.hpp"

#include <algorithm>
#include <format>

#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim::world {

const char* categoryName(Category c) {
    switch (c) {
        case Category::Normal: return "normal";
        case Category::Corridor: return "Korridor";
        case Category::Large: return "groß";
        case Category::Unusual: return "ungewöhnlich";
        case Category::Rare: return "selten";
    }
    return "?";
}

namespace {

Rgb rgb(const Json& j, Rgb def = {0, 0, 0}) {
    if (!j.isArray() || j.size() < 3) return def;
    return {j[0].asNumber(), j[1].asNumber(), j[2].asNumber()};
}
DRange drange(const Json& j, DRange def) {
    if (!j.isArray() || j.size() < 2) return def;
    return {j[0].asNumber(), j[1].asNumber()};
}
IRange irange(const Json& j, IRange def) {
    if (!j.isArray() || j.size() < 2) return def;
    return {j[0].asInt(), j[1].asInt()};
}

bool parseCategory(const std::string& s, Category& out) {
    static const std::pair<const char*, Category> names[] = {{"normal", Category::Normal},
                                                             {"corridor", Category::Corridor},
                                                             {"large", Category::Large},
                                                             {"unusual", Category::Unusual},
                                                             {"rare", Category::Rare}};
    for (auto& [n, c] : names)
        if (s == n) {
            out = c;
            return true;
        }
    return false;
}

unsigned parseFeature(const std::string& s) {
    static const std::pair<const char*, unsigned> names[] = {
        {"counter", F_COUNTER},     {"pillars", F_PILLARS},         {"pillars_sparse", F_PILLARS_SPARSE},
        {"thick_pillars", F_THICK_PILLARS}, {"giant_pillars", F_GIANT_PILLARS}, {"colonnade", F_COLONNADE},
        {"cubicles", F_CUBICLES},   {"shelves", F_SHELVES},         {"crates", F_CRATES},
        {"machines", F_MACHINES},   {"podium", F_PODIUM},           {"monolith", F_MONOLITH},
        {"beams", F_BEAMS},         {"vault", F_VAULT},             {"pipes", F_PIPES},
        {"checker", F_CHECKER}};
    for (auto& [n, f] : names)
        if (s == n) return f;
    return 0;
}

StyleOverride parseStyle(const Json& j) {
    StyleOverride s;
    if (!j.isObject()) return s;
    if (j.has("wall")) s.wall = rgb(j["wall"]);
    if (j.has("floor")) s.floor = rgb(j["floor"]);
    if (j.has("ceil")) s.ceil = rgb(j["ceil"]);
    if (j.has("lamp")) s.lamp = rgb(j["lamp"]);
    if (j.has("fog")) s.fog = rgb(j["fog"]);
    if (j.has("wall_pattern")) s.wallPattern = j["wall_pattern"].asString();
    if (j.has("floor_pattern")) s.floorPattern = j["floor_pattern"].asString();
    if (j.has("ceil_pattern")) s.ceilPattern = j["ceil_pattern"].asString();
    return s;
}

// Felder eines Raumtyps (ohne Index und Zonengewichte); false bei unbekannter Kategorie
bool parseRoomType(const Json& t, RoomTypeDef& d) {
    d.key = t["key"].asString();
    d.label = t["label"].asString(d.key);
    if (!parseCategory(t["category"].asString("normal"), d.category)) return false;
    d.shortSide = irange(t["short"], {2, 999});
    d.longSide = irange(t["long"], {2, 999});
    if (t.has("height")) d.height = drange(t["height"], {3.0, 3.0});
    for (const auto& f : t["features"].items()) d.features |= parseFeature(f.asString());
    d.light = t["light"].asString("");
    if (t.has("sunken")) {
        const Json& s = t["sunken"];
        d.sunken = std::array<double, 3>{s[0].asNumber(), s[1].asNumber(), s[2].asNumber()};
    }
    d.extraDoors = t["extra_doors"].asNumber(0.0);
    d.multiDoors = t["multi_doors"].asBool(false);
    d.ambientMul = t["ambient_mul"].asNumber(1.0);
    d.fogMul = t["fog_mul"].asNumber(1.0);
    d.stairStep = t["stair_step"].asNumber(0.3);
    d.biggish = t["biggish"].asBool(false);
    d.supply = t["supply"].asBool(false);
    d.noDarkVariant = t["no_dark_variant"].asBool(false);
    d.style = parseStyle(t["style"]);
    return true;
}

// Felder eines Objekts in ein anderes uebernehmen (vorhandene Schluessel ersetzen)
void mergeInto(Json& dst, const Json& src) {
    for (const auto& [k, v] : src.members()) dst.set(k, v);
}

// V6: Leveldaten mit dem Abschnitt "layout_v6" ueberlagern.
//   zones / room_types:   Schluessel des Eintrags -> Felder, die ersetzt werden
//   add_room_types:       neue Raumtypen (am Ende des Katalogs)
//   light_styles:         Lichtstile ergaenzen/ersetzen
//   alles andere:         ersetzt den gleichnamigen Wert (z. B. sector_size, corridor_types)
Json applyLayoutPatch(const Json& base, const Json& patch) {
    Json out = base;
    for (const auto& [key, val] : patch.members()) {
        if (key == "zones" || key == "room_types") {
            Json list = out[key];
            for (auto& entry : list.items()) {
                const Json* p = val.find(entry["key"].asString(""));
                if (p) mergeInto(entry, *p);
            }
            out.set(key, std::move(list));
        } else if (key == "add_room_types") {
            Json list = out["room_types"];
            for (const auto& t : val.items()) list.push(t);
            out.set("room_types", std::move(list));
        } else if (key == "light_styles") {
            Json styles = out["light_styles"];
            for (const auto& [name, s] : val.members()) {
                Json cur = styles[name].isObject() ? styles[name] : Json::object();
                mergeInto(cur, s);
                styles.set(name, std::move(cur));
            }
            out.set("light_styles", std::move(styles));
        } else if (key != "structure") {
            out.set(key, val);
        }
    }
    return out;
}

}  // namespace

int LevelDef::findType(const std::string& key) const {
    for (const auto& t : roomTypes)
        if (t.key == key) return t.index;
    return -1;
}

const LightStyleDef& LevelDef::lightStyle(const std::string& name) const {
    static const LightStyleDef kDefault;
    auto it = lightStyles.find(name);
    return it != lightStyles.end() ? it->second : kDefault;
}

const LightStyleDef& LevelDef::lightStyleFor(int zone, const std::string& name) const {
    if (world()) {
        auto it = lightStyles.find(std::to_string(regionOfZone(zone)) + ":" + name);
        if (it != lightStyles.end()) return it->second;
    }
    return lightStyle(name);
}

std::optional<LevelDef> LevelDef::fromJson(const Json& jIn, std::string* error, int layout) {
    auto fail = [&](std::string msg) -> std::optional<LevelDef> {
        if (error) *error = std::move(msg);
        return std::nullopt;
    };
    // V6: weitlaeufigerer Aufbau als Ueberlagerung; ohne "layout_v6" bleibt das Level wie es ist
    const bool v6 = layout >= kLayoutV6 && jIn["layout_v6"].isObject();
    const Json patched = v6 ? applyLayoutPatch(jIn, jIn["layout_v6"]) : Json();
    const Json& j = v6 ? patched : jIn;
    LevelDef L;
    L.layout = v6 ? kLayoutV6 : kLayoutLegacy;
    L.id = j["id"].asString("level");
    L.name = j["name"].asString(L.id);
    L.title = j["title"].asString("");
    L.generator = j["generator"].asString("sectors");
    L.nextLevel = j["next_level"].asString("");
    {
        const Json& f = j["finds"];
        FindsDef& d = L.finds;
        d.note = f["note"].asNumber(d.note);
        d.battery = f["battery"].asNumber(d.battery);
        d.batteryDark = f["battery_dark"].asNumber(d.batteryDark);
        d.exitStart = f["exit_start"].asNumber(d.exitStart);
        d.exitRange = f["exit_range"].asNumber(d.exitRange);
        d.exitMax = f["exit_max"].asNumber(d.exitMax);
    }
    L.sectorSize = (int)j["sector_size"].asInt(96);
    if (L.sectorSize < 32 || L.sectorSize % 16 != 0) return fail("sector_size muss ein Vielfaches von 16 (>= 32) sein");
    L.rarityDistance = j["rarity"]["distance"].asNumber(700.0);
    L.rarityMaxExtra = j["rarity"]["max_extra"].asNumber(1.6);
    L.zoneScale = j["zone_field"]["scale"].asNumber(190.0);
    L.zoneSharpness = j["zone_field"]["sharpness"].asNumber(11.0);
    L.originRadius = j["zone_field"]["origin_radius"].asNumber(130.0);

    for (const auto& z : j["zones"].items()) {
        ZoneDef d;
        d.key = z["key"].asString("zone");
        d.name = z["name"].asString(d.key);
        d.wall = rgb(z["wall"]);
        d.floor = rgb(z["floor"]);
        d.ceil = rgb(z["ceil"]);
        d.trim = rgb(z["trim"]);
        d.lamp = rgb(z["lamp"], {1, 1, 1});
        d.fog = rgb(z["fog"]);
        d.fogDensity = z["fog_density"].asNumber(0.05);
        d.ambient = z["ambient"].asNumber(0.5);
        d.lightStyle = z["light_style"].asString("panels");
        d.wallPattern = z["wall_pattern"].asString("plain");
        d.floorPattern = z["floor_pattern"].asString("none");
        d.ceilPattern = z["ceil_pattern"].asString("none");
        d.minLeaf = z["min_leaf"].asNumber(3);
        d.maxLeaf = z["max_leaf"].asNumber(13);
        d.bigChance = z["big_chance"].asNumber(0.05);
        d.corridorChance = z["corridor_chance"].asNumber(0.5);
        d.corridorWidth = irange(z["corridor_width"], {3, 4});
        d.wallThickness = irange(z["wall_thickness"], {1, 1});
        d.loopChance = z["loop_chance"].asNumber(0.2);
        d.hRoom = drange(z["h_room"], {2.6, 3.0});
        d.hCorridor = drange(z["h_corridor"], {2.5, 2.8});
        d.hBig = drange(z["h_big"], {4.0, 6.0});
        d.doorHeight = z["door_height"].asNumber(2.2);
        d.sunken = z["sunken"].asNumber(0.0);
        d.sunkenDepth = z["sunken_depth"].asNumber(2.4);
        d.originBias = z["origin_bias"].asNumber(0.0);
        d.weightOffset = z["weight_offset"].asNumber(0.0);
        d.doorClearance = z["door_clearance"].asNumber(2.4);
        d.wideDoors = z["wide_doors"].asBool(true);
        d.baseboard = z["baseboard"].asBool(false);
        d.largeRoomLight = z["large_room_light"].asString("");
        L.zones.push_back(std::move(d));
    }
    if (L.zones.empty() || L.zones.size() > (size_t)kMaxZones)
        return fail(std::format("Ein Level braucht 1 bis {} Zonen", kMaxZones));

    for (const auto& [name, s] : j["light_styles"].members()) {
        LightStyleDef d;
        d.extra = s["extra"].asNumber(0.3);
        d.intensity = s["intensity"].asNumber(0.6);
        if (s.has("ambient")) d.ambient = s["ambient"].asNumber();
        d.darkFog = s["dark_fog"].asBool(false);
        d.centerFallback = s["center_fallback"].asBool(false);
        d.hum = (int)s["hum"].asInt(-1);
        L.lightStyles[name] = d;
    }
    L.darkFog = rgb(j["dark_fog"], L.darkFog);

    for (const auto& t : j["room_types"].items()) {
        RoomTypeDef d;
        d.index = (int)L.roomTypes.size();
        if (!parseRoomType(t, d)) return fail("Unbekannte Kategorie bei Raumtyp " + d.key);
        for (const auto& a : t["affinity"].items()) d.affinity.push_back(a.asNumber());
        d.affinity.resize(L.zones.size(), 1.0);
        L.byCategory[(size_t)d.category].push_back(d.index);
        L.roomTypes.push_back(std::move(d));
    }
    if (L.roomTypes.empty()) return fail("Keine Raumtypen definiert");

    auto typeRef = [&](const char* field, int& out) -> bool {
        std::string key = j[field].asString("");
        out = L.findType(key);
        return out >= 0;
    };
    for (const auto& k : j["corridor_types"].items()) {
        int i = L.findType(k.asString());
        if (i < 0) return fail("Unbekannter Korridortyp " + k.asString());
        L.corridorTypes.push_back(i);
    }
    if (!typeRef("endless_type", L.endlessType)) return fail("endless_type fehlt/unbekannt");
    if (!typeRef("default_corridor_type", L.defaultCorridorType)) return fail("default_corridor_type fehlt/unbekannt");
    if (!typeRef("fallback_type", L.fallbackType)) return fail("fallback_type fehlt/unbekannt");
    if (!typeRef("supply_fallback_type", L.supplyFallbackType)) return fail("supply_fallback_type fehlt/unbekannt");
    for (const auto& k : j["spawn_preferred_types"].items()) {
        int i = L.findType(k.asString());
        if (i >= 0) L.spawnPreferred.push_back(i);
    }
    for (const auto& s : j["supply_items"].items()) L.supplyItems.push_back({s["item"].asString(), s["weight"].asNumber(1.0)});
    for (const auto& s : j["entities"].items()) {
        EntitySpawnDef d;
        d.prefab = s["prefab"].asString("");
        d.chance = s["chance"].asNumber(0.1);
        d.height = s["height"].asNumber(0.0);
        for (const auto& k : s["room_types"].items()) {
            int i = L.findType(k.asString());
            if (i >= 0) d.roomTypes.push_back(i);
            else log::warn("Level {}: entities: unbekannter Raumtyp '{}'", L.id, k.asString());
        }
        if (!d.prefab.empty()) L.entities.push_back(std::move(d));
    }
    L.ambience = j["ambience"].asString("");
    if (v6) {
        const Json& s = jIn["layout_v6"]["structure"];
        StructureDef& st = L.structure;
        st.passageChance = s["passage_chance"].asNumber(0.0);
        st.passageWidth = irange(s["passage_width"], {6, 9});
        st.openJoin = s["open_join"].asNumber(0.0);
        st.openLarge = s["open_large"].asNumber(0.0);
        st.endlessMul = s["endless_mul"].asNumber(1.0);
        for (const auto& k : s["passage_types"].items()) {
            int i = L.findType(k.asString(""));
            if (i < 0) return fail("layout_v6: passage_types: unbekannter Raumtyp " + k.asString(""));
            st.passageTypes.push_back(i);
        }
    }
    return L;
}

std::optional<LevelDef> LevelDef::loadFile(const std::string& path, std::string* error, int layout) {
    auto j = loadJsonFile(path);
    if (!j) {
        if (error) *error = "Datei fehlt oder ist kein gueltiges JSON: " + path;
        return std::nullopt;
    }
    return fromJson(*j, error, layout);
}

// --- V8: die zusammenhaengende Welt --------------------------------------------------------
// Alle Ebenen werden zu EINEM Katalog vereinigt: Zonen hintereinander (jede kennt ihre Ebene),
// Raumtypen mit Ebenen-Praefix ("level1/tunnel") und auf alle Zonen erweiterten Gewichten,
// Lichtstile je Ebene ("1:wall"). Dazu Uebergangsraeume aus world.json, die an den Nahtstellen
// zweier Ebenen erscheinen (Gewichte je Zone als "ebene/zone": wert).
std::optional<LevelDef> LevelDef::buildWorld(const Json& wj, const std::vector<const LevelDef*>& levels,
                                             std::string* error) {
    auto fail = [&](std::string msg) -> std::optional<LevelDef> {
        if (error) *error = std::move(msg);
        return std::nullopt;
    };
    LevelDef W;
    W.layout = kLayoutWorld;
    W.id = wj["id"].asString("welt");
    W.name = wj["name"].asString("LIMINAL");
    W.title = wj["title"].asString("");
    W.generator = "sectors";
    // Ebenen in der angegebenen Reihenfolge
    std::vector<const LevelDef*> src;
    std::vector<const Json*> regionJson;
    for (const auto& r : wj["regions"].items()) {
        std::string lid = r["level"].asString("");
        const LevelDef* found = nullptr;
        for (const LevelDef* L : levels)
            if (L && L->id == lid) found = L;
        if (!found) return fail("world.json: unbekannte Ebene " + lid);
        if (!src.empty() && found->sectorSize != src.front()->sectorSize)
            return fail("world.json: alle Ebenen brauchen dieselbe Sektorgroesse");
        src.push_back(found);
        regionJson.push_back(&r);
    }
    if (src.empty()) return fail("world.json: keine Ebenen");
    W.sectorSize = src.front()->sectorSize;
    W.rarityDistance = wj["rarity"]["distance"].asNumber(900.0);
    W.rarityMaxExtra = wj["rarity"]["max_extra"].asNumber(1.8);
    W.zoneScale = wj["zone_field"]["scale"].asNumber(src.front()->zoneScale);
    W.zoneSharpness = wj["zone_field"]["sharpness"].asNumber(src.front()->zoneSharpness);
    W.originRadius = wj["zone_field"]["origin_radius"].asNumber(src.front()->originRadius);

    // 1. Zonen und Lichtstile
    for (size_t r = 0; r < src.size(); ++r) {
        const LevelDef& L = *src[r];
        RegionDef R;
        R.id = L.id;
        R.name = (*regionJson[r])["name"].asString(L.name);
        R.title = (*regionJson[r])["title"].asString(L.title);
        R.firstZone = (int)W.zones.size();
        R.zoneCount = (int)L.zones.size();
        for (ZoneDef z : L.zones) {
            z.region = (int)r;
            if (r != 0) z.originBias = 0.0;  // nur die erste Ebene umgibt den Start
            W.zones.push_back(std::move(z));
        }
        for (const auto& [name, st] : L.lightStyles) {
            W.lightStyles[std::to_string(r) + ":" + name] = st;
            if (r == 0) W.lightStyles[name] = st;
        }
        R.finds = L.finds;
        R.supplyItems = L.supplyItems;
        R.darkFog = L.darkFog;
        W.regions.push_back(std::move(R));
    }
    if (W.zones.size() > (size_t)kMaxZones)
        return fail(std::format("world.json: {} Zonen, hoechstens {}", W.zones.size(), kMaxZones));
    const size_t nz = W.zones.size();
    auto zoneIndex = [&](const std::string& ref) -> int {  // "level1/tunnel"
        auto slash = ref.find('/');
        if (slash == std::string::npos) return -1;
        std::string lid = ref.substr(0, slash), key = ref.substr(slash + 1);
        for (const RegionDef& R : W.regions)
            if (R.id == lid)
                for (int k = 0; k < R.zoneCount; ++k)
                    if (W.zones[(size_t)(R.firstZone + k)].key == key) return R.firstZone + k;
        return -1;
    };

    // 2. Raumtypen (Indizes je Ebene verschoben, Gewichte auf alle Zonen erweitert)
    for (size_t r = 0; r < src.size(); ++r) {
        const LevelDef& L = *src[r];
        RegionDef& R = W.regions[r];
        const int off = (int)W.roomTypes.size();
        for (const RoomTypeDef& t : L.roomTypes) {
            RoomTypeDef c = t;
            c.index = (int)W.roomTypes.size();
            c.key = L.id + "/" + t.key;
            c.region = (int)r;
            c.affinity.assign(nz, 0.0);
            for (size_t k = 0; k < t.affinity.size() && k < (size_t)R.zoneCount; ++k)
                c.affinity[(size_t)R.firstZone + k] = t.affinity[k];
            W.roomTypes.push_back(std::move(c));
        }
        auto map = [&](int i) { return i < 0 ? -1 : off + i; };
        R.endlessType = map(L.endlessType);
        R.defaultCorridorType = map(L.defaultCorridorType);
        R.fallbackType = map(L.fallbackType);
        R.supplyFallbackType = map(L.supplyFallbackType);
        R.structure = L.structure;
        for (int& p : R.structure.passageTypes) p = map(p);
        for (int k : L.corridorTypes) W.corridorTypes.push_back(map(k));
        if (r == 0)
            for (int k : L.spawnPreferred) W.spawnPreferred.push_back(map(k));
        for (EntitySpawnDef e : L.entities) {
            if (e.roomTypes.empty())
                for (size_t k = 0; k < L.roomTypes.size(); ++k) e.roomTypes.push_back(off + (int)k);
            else
                for (int& k : e.roomTypes) k = map(k);
            W.entities.push_back(std::move(e));
        }
    }

    // 3. Uebergangsraeume zwischen den Ebenen
    for (const auto& t : wj["connectors"].items()) {
        RoomTypeDef d;
        d.index = (int)W.roomTypes.size();
        if (!parseRoomType(t, d)) return fail("world.json: unbekannte Kategorie bei " + d.key);
        d.key = W.id + "/" + d.key;
        d.region = -1;
        d.seam = t["seam"].asNumber(1.0);
        d.affinity.assign(nz, 0.0);
        for (const auto& [ref, v] : t["affinity"].members()) {
            int z = zoneIndex(ref);
            if (z < 0) return fail("world.json: " + d.key + ": unbekannte Zone " + ref);
            d.affinity[(size_t)z] = v.asNumber(1.0);
            d.seamMask |= 1u << W.zones[(size_t)z].region;
        }
        if (d.category == Category::Corridor) W.corridorTypes.push_back(d.index);
        W.roomTypes.push_back(std::move(d));
    }
    for (const RoomTypeDef& t : W.roomTypes) W.byCategory[(size_t)t.category].push_back(t.index);

    // Werte der ersten Ebene gelten dort, wo Code noch nach "dem" Level fragt
    const RegionDef& R0 = W.regions.front();
    W.endlessType = R0.endlessType;
    W.defaultCorridorType = R0.defaultCorridorType;
    W.fallbackType = R0.fallbackType;
    W.supplyFallbackType = R0.supplyFallbackType;
    W.structure = R0.structure;
    W.finds = R0.finds;
    W.supplyItems = R0.supplyItems;
    W.darkFog = R0.darkFog;
    W.ambience = src.front()->ambience;

    // 4. Schichten
    const Json& sj = wj["strata"];
    StrataDef& S = W.strata;
    for (const auto& o : sj["order"].items()) {
        std::string id = o.asString("");
        int found = -1;
        for (size_t r = 0; r < W.regions.size(); ++r)
            if (W.regions[r].id == id) found = (int)r;
        if (found < 0) return fail("world.json: strata.order: unbekannte Ebene " + id);
        S.order.push_back(found);
    }
    if (S.order.empty())
        for (size_t r = 0; r < W.regions.size(); ++r) S.order.push_back((int)r);
    if (S.order.front() != 0) return fail("world.json: die Schichtfolge muss mit der ersten Ebene beginnen");
    S.first = sj["first"].asNumber(S.first);
    S.width = std::max(60.0, sj["width"].asNumber(S.width));
    S.warpScale = sj["warp_scale"].asNumber(S.warpScale);
    S.warp = sj["warp"].asNumber(S.warp);
    S.islandScale = sj["island_scale"].asNumber(S.islandScale);
    S.island = sj["island"].asNumber(S.island);
    S.calm = sj["calm"].asNumber(S.calm);
    S.seam = std::clamp(sj["seam"].asNumber(S.seam), 0.01, 0.45);
    S.bias = sj["bias"].asNumber(S.bias);
    W.elevators.chance = wj["elevators"]["chance"].asNumber(0.0);
    W.elevators.start = wj["elevators"]["start"].asNumber(200.0);
    return W;
}

}  // namespace lim::world
