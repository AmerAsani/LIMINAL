// V11: feste, entworfene Karte eines Levels (semi-prozeduraler Aufbau, WORLD_GENERATION.md).
//
// Fest in jedem Lauf: Raeume (Rechteck, Zone, Raumtyp, Boden- und Deckenhoehe, Stuetzenraster,
// Leuchten und Stromkreise), Tueren und Oeffnungen, feste Objekte, Uebergaenge in andere Level.
// Variabel je Lauf: was an den Spawnpunkten erscheint (gameplay/spawn_plan).
//
// Die Karten entstehen mit den Entwurfsskripten tools/levels/<level>.py und liegen als
// data/maps/<level>.json im Projekt. Geprueft werden sie mit tests/map_check.
//
// Koordinaten: Kacheln (1 m), die Karte liegt bei (0, 0) .. (w, h). Rand und Zwischenraeume sind
// massiv; Raeume sind Innenflaechen (Waende liegen ausserhalb), Tueren fuellen eine Wandluecke.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/json.hpp"

namespace lim::world {

struct MapLight {
    double x = 0, y = 0;   // Kachelmitte (Weltkoordinaten)
    int kind = 0;          // 0 Decke, 1 Wand
    int dirX = 0, dirY = 0;
    double intensity = 0.8, radius = 4.5;
    double z = -999.0;        // Hoehe der Leuchtflaeche (Welt); <= -900: unter der Decke
    bool panel = true;        // Leuchtfeld in der Decke zeichnen (false: Leuchtenkoerper ist ein Modell)
    double r = -1.0, g = 0.0, b = 0.0;  // eigene Lichtfarbe (r < 0: Lampenfarbe der Zone)
};

struct MapRoom {
    i64 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    std::string type, zone;
    double floor = 0.0, height = 3.0;
    u64 seed = 0;                 // Details des Raums (fest)
    std::string light;            // Lichtstil (leer: wie Raumtyp/Zone)
    std::vector<MapLight> lights; // feste Leuchten (ersetzen den Lichtstil)
    int circuit = -1;             // Stromkreis 0..31 (Stromausfall trifft alle Raeume eines Kreises)
    int flicker = -1;             // 0 ruhig, 1 pulsierend, 2 alte Roehre (-1: nach Seed)
    double ambient = -1.0;        // Umgebungslicht (-1: nach Zone/Stil)
    // Stuetzenraster in Weltkoordinaten (sp = Abstand, th = Staerke, ox/oy = erste Stuetze)
    i64 pillarSp = 0, pillarSpY = 0, pillarTh = 1, pillarOx = 0, pillarOy = 0;
    double beamDrop = 0.0;        // Unterzuege ueber den Stuetzenlinien (m unter der Decke, 0: keine)
    // V11: Bloecke (Weltkoordinaten): Hoehe ueber dem Raumboden; hidden = unsichtbar, ein Modell der
    // Ausstattung steht darauf (Wagen, Hochregal) - Kollision, Schatten und Wege bleiben
    struct Block {
        i64 x0, y0, x1, y1;
        double h;
        bool hidden;
    };
    std::vector<Block> blocks;
    double water = -999.0;        // V11: Wasserspiegel (absolute Hoehe; < -900 kein Wasser)
    std::string label;            // Anzeige/Pruefung (z. B. "Deck C")
    std::vector<std::string> tags;
    bool hasTag(const std::string& t) const;
};

enum class MapDoorKind : u8 { Door, Wide, Gate, Opening };

struct MapDoor {
    i64 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int axis = 0;     // 0: Durchgang in x-Richtung (senkrechte Wand)
    int a = -1, b = -1;  // Raumindex links/oben und rechts/unten
    MapDoorKind kind = MapDoorKind::Door;
    double top = -1.0;   // Sturzhoehe ueber dem Boden der Welt (-1: nach Zone)
    std::string style;   // Tuerart fuer die Ausstattung (z. B. "steel", "wood_pink", "black")
};

struct MapSpawn {
    std::string kind;            // item, note, battery, fusebox, crate, deco, entity, sound, light_group
    double x = 0, y = 0, z = 0;
    double yaw = 0;              // Blickrichtung (rad), bei Wandobjekten in den Raum
    std::vector<std::string> tags;
    std::string role, set;       // entity: Kreatur; deco: Ausstattungssatz
    int group = -1;              // sich ausschliessende Punkte (hoechstens einer je Gruppe)
    double weight = 1.0;
    int room = -1;               // Raumindex (vom Compiler gesetzt)
    bool hasTag(const std::string& t) const;
};

struct MapObject {
    std::string model;           // Ausstattungsmodell (render/props)
    double x = 0, y = 0, z = 0, yaw = 0;
    double sx = 1, sy = 1, sz = 1;
    int variant = 0;
    std::string tag;
};

// V11: Sicherungspunkt - wer vorbeigeht, speichert; nach dem Tod geht es hier weiter
struct MapCheckpoint {
    std::string id, label;
    double x = 0, y = 0, yaw = 0;
};

struct MapTransition {
    std::string id;              // Anker (das Gegenstueck im Ziel heisst gleich)
    std::string kind;            // elevator, stairs, hole, door, dive, seam
    std::string target;          // Ziel-Level ("" = kommt hier nur an)
    double x = 0, y = 0, yaw = 0;  // Ankunftspunkt und Blick
    i64 x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // Ausloesebereich
    std::string needs;           // V11: Item, ohne das der Uebergang nicht ausloest (Aufzug: Sicherung)
};

struct MapDef {
    std::string id;
    i64 w = 0, h = 0;
    u64 seed = 1;
    double startX = 0, startY = 0, startYaw = 0;  // neuer Lauf / Ankunft ohne Uebergang
    bool wrap = false;                             // Raender schliessen an sich selbst an (Level 0, 6)
    std::vector<MapRoom> rooms;
    std::vector<MapDoor> doors;
    std::vector<MapSpawn> spawns;
    std::vector<MapObject> objects;
    std::vector<MapTransition> transitions;
    std::vector<MapCheckpoint> checkpoints;  // V11

    const MapTransition* transition(const std::string& id) const;
    int roomAt(double x, double y) const;  // -1 = keiner

    static std::optional<MapDef> fromJson(const Json& j, std::string* error);
    static std::optional<MapDef> loadFile(const std::string& path, std::string* error);
};

}  // namespace lim::world
