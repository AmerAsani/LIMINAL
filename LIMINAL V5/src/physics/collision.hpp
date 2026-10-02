// Kollision gegen die Kachelwelt (Port von V4, player.py) und Sichtlinien.
//
// Die Welt ist ein Hoehenfeld aus 1-m-Kacheln mit eigener Boden- und
// Deckenhoehe. Figuren sind aufrechte Zylinder (Radius, Koerperhoehe); sie
// koennen Stufen bis MAX_STEP hinaufgehen und fallen von hoeheren Kanten.
// Das Modul kennt nur die Kacheldaten - nicht, woher sie kommen.
#pragma once

#include "core/math.hpp"
#include "world/tile.hpp"

namespace lim::phys {

// Quelle fuer Kacheln (die Welt; in Tests auch eine Attrappe).
class TileSource {
public:
    virtual ~TileSource() = default;
    virtual const world::Tile& at(i64 x, i64 y) = 0;
};

struct Shape {
    double radius = 0.24;   // Kollisionsradius (m)
    double body = 1.78;     // benoetigte Kopffreiheit (m)
    double maxStep = 0.5;   // hoechste Stufe (m)
};

// true, wenn die Figur an (x, y) mit Fusshoehe z kollidiert
bool blocked(TileSource& w, double x, double y, double z, const Shape& s);
// hoechster begehbarer Boden unter der Figur (fuer fluessiges Treppensteigen)
double ground(TileSource& w, double x, double y, double z, const Shape& s);

// Bewegt eine Figur mit Gleiten an Waenden (Teilschritte, getrennt nach Achsen).
struct MoveResult {
    double moved = 0.0;
    bool hitX = false, hitY = false;
};
MoveResult moveAndSlide(TileSource& w, double& x, double& y, double z, double dx, double dy, const Shape& s);

// Freie Sichtlinie in der Ebene (V4 find_target): tastet in 0.2-m-Schritten ab.
bool clearLine(TileSource& w, double x0, double y0, double z, double x1, double y1, double stopBefore, double headroom);

// Naechste freie Stelle, falls eine Figur in Geometrie steckt (V4 unstuck)
bool findFreeSpot(TileSource& w, double& x, double& y, double& z, const Shape& s);

}  // namespace lim::phys
