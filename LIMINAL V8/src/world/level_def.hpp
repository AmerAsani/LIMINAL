// Datenbeschreibung eines Levels (data/levels/*.json): Zonen, Raumtypen,
// Lichtstile, Item-Tabellen. Die Weltgenerierung liest nur diese Daten; neue
// Level oder Raumtypen brauchen keine Aenderung am Engine-Code.
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace lim::world {

constexpr int kMaxZones = 16;  // V8: die zusammenhaengende Welt traegt die Zonen aller Ebenen
using Rgb = std::array<double, 3>;
using ZoneWeights = std::array<double, kMaxZones>;

struct IRange {
    long long lo = 0, hi = 0;
};
struct DRange {
    double lo = 0, hi = 0;
};

enum class Category { Normal, Corridor, Large, Unusual, Rare };
const char* categoryName(Category c);

// Einbauten eines Raums (Bitmaske). Neue Einbauten brauchen Code in room.cpp.
enum Feature : unsigned {
    F_COUNTER = 1u << 0,
    F_PILLARS = 1u << 1,
    F_PILLARS_SPARSE = 1u << 2,
    F_THICK_PILLARS = 1u << 3,
    F_GIANT_PILLARS = 1u << 4,
    F_COLONNADE = 1u << 5,
    F_CUBICLES = 1u << 6,
    F_SHELVES = 1u << 7,
    F_CRATES = 1u << 8,
    F_MACHINES = 1u << 9,
    F_PODIUM = 1u << 10,
    F_MONOLITH = 1u << 11,
    F_BEAMS = 1u << 12,
    F_VAULT = 1u << 13,
    F_PIPES = 1u << 14,
    F_CHECKER = 1u << 15,
};

struct ZoneDef {
    std::string key, name;
    Rgb wall{}, floor{}, ceil{}, trim{}, lamp{}, fog{};
    double fogDensity = 0.05, ambient = 0.5;
    std::string lightStyle = "panels";
    std::string wallPattern = "plain", floorPattern = "none", ceilPattern = "none";
    double minLeaf = 3, maxLeaf = 13, bigChance = 0.05, corridorChance = 0.5;
    IRange corridorWidth{3, 4}, wallThickness{1, 1};
    double loopChance = 0.2;
    DRange hRoom{2.6, 3.0}, hCorridor{2.5, 2.8}, hBig{4.0, 6.0};
    double doorHeight = 2.2;
    double sunken = 0.0, sunkenDepth = 2.4;  // Wahrscheinlichkeit/Tiefe abgesenkter Raeume
    double originBias = 0.0;                  // Bevorzugung rund um den Startpunkt
    double weightOffset = 0.0;
    double doorClearance = 2.4;               // Kopffreiheit ueber Tuerschwellen
    bool wideDoors = true;
    bool baseboard = false;                   // Sockelleisten in dieser Zone
    std::string largeRoomLight;               // Lichtstil fuer grosse Hallen (statt "panels")
    int region = 0;                           // V8: Ebene (Region) dieser Zone in der zusammenhaengenden Welt
};

struct StyleOverride {
    std::optional<Rgb> wall, floor, ceil, lamp, fog;
    std::optional<std::string> wallPattern, floorPattern, ceilPattern;
};

struct RoomTypeDef {
    int index = 0;
    std::string key, label;
    Category category = Category::Normal;
    IRange shortSide{2, 999}, longSide{2, 999};
    std::vector<double> affinity;
    std::optional<DRange> height;
    unsigned features = 0;
    std::string light;
    std::optional<std::array<double, 3>> sunken;  // (Wahrscheinlichkeit, min, max)
    double extraDoors = 0.0;
    bool multiDoors = false;
    double ambientMul = 1.0, fogMul = 1.0, stairStep = 0.3;
    bool biggish = false, supply = false, noDarkVariant = false;
    StyleOverride style;
    // V8: Ebene des Raumtyps (-1 = Uebergangsraum zwischen Ebenen) und Bevorzugung an den Nahtstellen
    int region = 0;
    double seam = 0.0;  // > 0: erscheint fast nur dort, wo zwei Ebenen aneinanderstossen
    unsigned seamMask = 0;  // Ebenen, die ein Uebergangsraum verbindet (alle muessen dort liegen)

    bool fits(long long s, long long l) const {
        return shortSide.lo <= s && s <= shortSide.hi && longSide.lo <= l && l <= longSide.hi;
    }
    bool has(Feature f) const { return (features & f) != 0; }
};

struct LightStyleDef {
    double extra = 0.3, intensity = 0.6;
    std::optional<double> ambient;
    bool darkFog = false;
    bool centerFallback = false;  // Raeume ohne Lampe im Raster bekommen eine Lampe in der Mitte
    int hum = -1;                 // V7: Klang der Leuchten (-1 nach Lichtstil, 0 still, 1..4 siehe ChunkLight::hum)
};

struct SupplyItem {
    std::string item;
    double weight = 1.0;
};

// Datengesteuerte Entities: ein Prefab erscheint mit einer Wahrscheinlichkeit in
// Raeumen bestimmter Typen (Mitte des Raums). Deterministisch je Raum und Seed;
// beeinflusst die Weltgenerierung nicht.
struct EntitySpawnDef {
    std::string prefab;
    std::vector<int> roomTypes;  // leer = alle Raumtypen
    double chance = 0.1;
    double height = 0.0;         // Hoehe ueber dem Boden (m)
};

// V6: Weltstruktur-Version. 4 = Aufbau aus V4/V5 (bitgenau, fuer alte Spielstaende),
// 6 = weitlaeufigerer V6-Aufbau (Abschnitt "layout_v6" der Leveldatei ueberschreibt Werte).
constexpr int kLayoutLegacy = 4;
constexpr int kLayoutV6 = 6;

// V6: Strukturregeln des weitlaeufigeren Aufbaus ("layout_v6" -> "structure").
struct StructureDef {
    double passageChance = 0.0;     // Hauptachse eines Sektors wird zur breiten Passage (Zwischenraum)
    IRange passageWidth{6, 9};
    std::vector<int> passageTypes;  // Raumtypen der Passagen (eine wird je Passage gewaehlt)
    double openJoin = 0.0;          // Flur trifft Flur/Passage: offener Uebergang statt Tuer
    double openLarge = 0.0;         // Flur trifft grossen Raum: breite Oeffnung statt Tuer
    double endlessMul = 1.0;        // Faktor fuer die Chance auf einen endlosen Gang
};

// V7: Fundstuecke und Notausgaenge (beeinflussen die Weltgenerierung nicht)
struct FindsDef {
    double note = 0.05;         // Notizzettel je Raum
    double battery = 0.03;      // Batterien je Raum
    double batteryDark = 0.3;   // ... in dunklen Raeumen (Lichtstile dark/void)
    double exitStart = 120.0;   // Notausgaenge erst ab dieser Entfernung vom Start (m)
    double exitRange = 600.0;   // ... und bis zur vollen Haeufigkeit nach weiteren Metern
    double exitMax = 0.035;     // hoechste Chance je Raum
};

// V8: Die zusammenhaengende Welt ("data/world.json"). Alle Ebenen liegen in EINER Welt: um den
// Start die Bueros (Level 0), weiter draussen das Tiefgeschoss (Level 1), dahinter das Hotel
// (Level 2) - als unregelmaessige, verwirbelte Schichten mit Ausbuchtungen und Inseln. Danach
// wiederholt sich die Folge. Jede Ebene bringt ihre Zonen, Raumtypen, Lichtstile, Vorraete und
// Fundstuecke mit; an den Nahtstellen mischen sich die Zonen beider Ebenen.
constexpr int kLayoutWorld = 8;

struct RegionDef {
    std::string id, name, title;  // "level1", "Level 1", "Das Tiefgeschoss"
    int firstZone = 0, zoneCount = 0;
    StructureDef structure;       // Passagen, Fluchten (aus dem V6-Aufbau der Ebene)
    FindsDef finds;
    std::vector<SupplyItem> supplyItems;
    int endlessType = -1, defaultCorridorType = -1, fallbackType = -1, supplyFallbackType = -1;
    Rgb darkFog{0.012, 0.013, 0.02};
};

struct StrataDef {
    std::vector<int> order;         // Abfolge der Ebenen von innen nach aussen (wiederholt sich)
    double first = 380.0;           // Radius der innersten Schicht (m)
    double width = 520.0;           // Breite jeder weiteren Schicht (m)
    double warpScale = 480.0, warp = 240.0;     // Verwirbelung der Schichtgrenzen (Massstab, Weite in m)
    double islandScale = 230.0, island = 120.0; // Ausbuchtungen und Inseln (m)
    double calm = 160.0;            // so weit um den Start bleibt die erste Schicht ungestoert (m)
    double seam = 0.16;             // Breite der Nahtstelle (Anteil einer Schicht)
    double bias = 1.6;              // wie stark die Schicht die Zonenwahl bestimmt
};

// V8: Aufzuege der Welt - seltene Kerne in Raeumen, die eine Schicht tiefer fuehren (und zurueck)
struct ElevatorDef {
    double chance = 0.0;   // je geeignetem Raum
    double start = 200.0;  // erst ab dieser Entfernung vom Start (m)
};

struct LevelDef {
    std::string id, name, title, generator;
    std::string nextLevel;  // V7: wohin die Notausgaenge fuehren (leer = nirgendwohin)
    FindsDef finds;         // V7
    int layout = kLayoutLegacy;  // tatsaechlich angewendete Version (6 nur, wenn die Datei "layout_v6" hat)
    int sectorSize = 96;
    double rarityDistance = 700.0, rarityMaxExtra = 1.6;
    double zoneScale = 190.0, zoneSharpness = 11.0, originRadius = 130.0;
    std::vector<ZoneDef> zones;
    std::map<std::string, LightStyleDef> lightStyles;
    Rgb darkFog{0.012, 0.013, 0.02};
    std::vector<RoomTypeDef> roomTypes;
    std::vector<int> corridorTypes;
    int endlessType = -1, defaultCorridorType = -1, fallbackType = -1, supplyFallbackType = -1;
    std::vector<int> spawnPreferred;
    std::vector<SupplyItem> supplyItems;
    std::vector<EntitySpawnDef> entities;
    std::string ambience;
    StructureDef structure;  // V6 (nur bei layout >= kLayoutV6 belegt)

    std::array<std::vector<int>, 5> byCategory;  // Typindizes je Kategorie (Katalogreihenfolge)
    // V8: zusammenhaengende Welt (leer = ein einzelnes Level wie bisher)
    std::vector<RegionDef> regions;
    StrataDef strata;
    ElevatorDef elevators;

    int zoneCount() const { return (int)zones.size(); }
    const RoomTypeDef& type(int i) const { return roomTypes[(size_t)i]; }
    int findType(const std::string& key) const;
    const LightStyleDef& lightStyle(const std::string& name) const;
    // V8
    bool world() const { return !regions.empty(); }
    int regionOfZone(int zone) const { return world() ? zones[(size_t)zone].region : 0; }
    // Lichtstil, wie ihn die Ebene dieser Zone definiert (in der Welt tragen gleichnamige Stile
    // je Ebene eigene Werte)
    const LightStyleDef& lightStyleFor(int zone, const std::string& name) const;
    const FindsDef& findsFor(int zone) const { return world() ? regions[(size_t)regionOfZone(zone)].finds : finds; }
    const std::vector<SupplyItem>& supplyItemsFor(int zone) const {
        return world() ? regions[(size_t)regionOfZone(zone)].supplyItems : supplyItems;
    }
    int supplyFallbackFor(int zone) const {
        return world() ? regions[(size_t)regionOfZone(zone)].supplyFallbackType : supplyFallbackType;
    }
    const StructureDef& structureFor(int zone) const {
        return world() ? regions[(size_t)regionOfZone(zone)].structure : structure;
    }
    // Ebene als Text ("level1"); einzelne Level liefern ihre eigene Kennung
    const std::string& regionId(int zone) const { return world() ? regions[(size_t)regionOfZone(zone)].id : id; }

    // layout: gewuenschte Weltstruktur (kLayoutLegacy oder kLayoutV6)
    static std::optional<LevelDef> fromJson(const Json& j, std::string* error, int layout = kLayoutLegacy);
    static std::optional<LevelDef> loadFile(const std::string& path, std::string* error, int layout = kLayoutLegacy);
    // V8: die zusammenhaengende Welt aus data/world.json und den (V6-)Leveln ihrer Ebenen
    static std::optional<LevelDef> buildWorld(const Json& world, const std::vector<const LevelDef*>& levels,
                                              std::string* error);
};

}  // namespace lim::world
