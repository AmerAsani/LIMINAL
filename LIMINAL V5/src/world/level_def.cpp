#include "world/level_def.hpp"

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

std::optional<LevelDef> LevelDef::fromJson(const Json& j, std::string* error) {
    auto fail = [&](std::string msg) -> std::optional<LevelDef> {
        if (error) *error = std::move(msg);
        return std::nullopt;
    };
    LevelDef L;
    L.id = j["id"].asString("level");
    L.name = j["name"].asString(L.id);
    L.title = j["title"].asString("");
    L.generator = j["generator"].asString("sectors");
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
        L.lightStyles[name] = d;
    }
    L.darkFog = rgb(j["dark_fog"], L.darkFog);

    for (const auto& t : j["room_types"].items()) {
        RoomTypeDef d;
        d.index = (int)L.roomTypes.size();
        d.key = t["key"].asString();
        d.label = t["label"].asString(d.key);
        if (!parseCategory(t["category"].asString("normal"), d.category))
            return fail("Unbekannte Kategorie bei Raumtyp " + d.key);
        d.shortSide = irange(t["short"], {2, 999});
        d.longSide = irange(t["long"], {2, 999});
        for (const auto& a : t["affinity"].items()) d.affinity.push_back(a.asNumber());
        d.affinity.resize(L.zones.size(), 1.0);
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
    return L;
}

std::optional<LevelDef> LevelDef::loadFile(const std::string& path, std::string* error) {
    auto j = loadJsonFile(path);
    if (!j) {
        if (error) *error = "Datei fehlt oder ist kein gueltiges JSON: " + path;
        return std::nullopt;
    }
    return fromJson(*j, error);
}

}  // namespace lim::world
