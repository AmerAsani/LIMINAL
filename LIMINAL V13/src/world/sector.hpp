// Ein Sektor (S x S Kacheln): Raeume und Tueren als Metadaten.
#pragma once

#include "props/prop_library.hpp"

#include <memory>
#include <vector>

#include "world/room.hpp"

namespace lim::world {

class Sector {
public:
    Sector(i64 sx_, i64 sy_, int size) : sx(sx_), sy(sy_), x0(sx_ * size), y0(sy_ * size), S(size) {}

    i64 sx, sy, x0, y0;
    int S;
    std::vector<std::unique_ptr<Room>> rooms;
    std::vector<std::unique_ptr<Door>> doors;
    std::vector<Room*> supplyRooms;  // Versorgungsraeume mit Automat
    std::vector<Room*> itemRooms;    // Raeume mit Items

    // V11: Ausstattung (feste Objekte der Karte und je Lauf gewaehlte Dekoration). Materialien sind
    // schon in der Bank angelegt (je Teil eine ID), Flackern/Stromkreis vom Raum am Standort.
    struct Prop {
        const props::Model* model = nullptr;
        float x = 0, y = 0, z = 0, yaw = 0, sx = 1, sy = 1, sz = 1;
        std::vector<u16> mats;
        std::vector<float> glow;
        u8 flickMode = 0, flickSeed = 0;
    };
    std::vector<Prop> props;
    // V11: Stellen der Ausstattung, die klingen oder den Untergrund aendern (Pfuetze, Wagen, Rohre,
    // Schaltkasten, Glut im Bodenloch) - fuer Klanglandschaft und Schritte
    struct Spot {
        std::string kind;
        float x = 0, y = 0, z = 0;
        float yaw = 0;      // V13.2: Ausrichtung des Objekts (Fenster: x aus der Wand)
        std::string model;  // V13.2: Modell (Fenster: welche Art - Fensterwesen sitzen in dunklen Fenstern)
    };
    std::vector<Spot> spots;
    // V11: Notizen an den Spawnpunkten der Karte (vom Lauf gewaehlt); welche Notiz, entscheidet die Sitzung
    struct NoteSpot {
        std::string id;
        float x = 0, y = 0, z = 0, yaw = 0;
        std::string item;  // Sicherungskasten: welche Sicherung
    };
    std::vector<NoteSpot> notes;
    std::vector<NoteSpot> fuseboxes;  // V11: Sicherungskaesten der Karte (Spawnpunkte "fusebox")
    std::vector<NoteSpot> entities;   // V11: Lager der Entities (Spawnpunkte "entity"; item = Rolle, z = Decke)

    // Nach der Erzeugung: Kachelindex fuer schnelle Abfragen.
    void buildIndex();
    Room* roomAt(i64 x, i64 y) const;
    // Tuer (nur eigene Durchgangskacheln) an einer Kachel.
    const Door* doorAt(i64 x, i64 y) const;
    bool contains(i64 x, i64 y) const { return x >= x0 && x < x0 + S && y >= y0 && y < y0 + S; }

    // Stufe 2: Portale mit den Nachbarsektoren verbunden und Raeume vorbereitet.
    bool linked = false;

private:
    std::vector<i32> index_;  // >= 0 Raum, -1 massiv, <= -2 Tuer (-2 - i)
};

}  // namespace lim::world
