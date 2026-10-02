// Ein Sektor (S x S Kacheln): Raeume und Tueren als Metadaten.
#pragma once

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
