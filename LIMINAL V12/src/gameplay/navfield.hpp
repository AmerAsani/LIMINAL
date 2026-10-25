// V10: Wege und Wahrnehmung fuer die Figuren.
//
// NavField: Distanzfeld (Dijkstra) ueber die geladenen Kacheln um einen Ursprung - wer dem
// Gefaelle folgt, kommt zum Ursprung (Spieler, Ziel eines Wanderers). Stufen bis 0,65 m, schraeg
// nur an freien Ecken vorbei.
// Wahrnehmung: Sieht der Spieler einen Punkt? Blickfeld (mit zentralem Sehen), Sichtlinie,
// Licht (Leuchten, Taschenlampe, Stromausfall) und Nebel.
#pragma once

#include <cmath>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"
#include "world/room.hpp"
#include "world/tile.hpp"

namespace lim::world {
class World;
struct Tile;
}

namespace lim::game {

bool figureWalkable(const world::Tile* t, float clearance = 1.9f);
// freie Sicht ueber geladene Kacheln (ohne Chunks nachzuladen)
bool clearSight(world::World& w, vec3 a, vec3 b);

class NavField {
public:
    static constexpr int kR = 40, kN = 2 * kR + 1;
    void build(world::World& w, i64 cx, i64 cy, float clearance = 1.9f);
    int at(i64 x, i64 y) const;  // Kosten (10 je Kachel, 14 schraeg), -1 unerreichbar
    bool valid() const { return !f_.empty(); }
    i64 cx() const { return cx_; }
    i64 cy() const { return cy_; }
    // naechster Wegpunkt (Kachelmitte) Richtung Ursprung; false: kein Weg
    bool next(double x, double y, double& tx, double& ty) const;
    // alle Kacheln mit Kosten in [lo, hi]
    void ring(int lo, int hi, std::vector<std::pair<i64, i64>>& out) const;

private:
    std::vector<int> f_;
    i64 cx_ = 0, cy_ = 0;
};

// Was der Spieler gerade sieht
struct Viewer {
    vec3 eye, look;
    bool flashOn = false;
    vec3 flashPos, flashDir;
    int blackoutSeed = -1;
    float blackoutAmount = 0.0f;
    // V11: der echte Stromausfall kommt als Welle - dunkel ist nur, was die Front schon erreicht hat
    vec3 blackoutOrigin;
    float blackoutFront = -1.0f;  // m ab Ursprung; < 0: keine Welle (alles zugleich)
    float fog = 0.05f;            // Nebeldichte (Sichtweite)
};

// V11: Ist die Kachel bei (x, y) vom Stromausfall erfasst (Stromkreis und Welle)?
inline bool blackoutCovers(const Viewer& v, const world::Tile* t, float x, float y) {
    if (v.blackoutSeed < 0 || v.blackoutAmount <= 0.5f || !t || !t->room || (int)t->room->lightCircuit() != v.blackoutSeed) return false;
    return v.blackoutFront < 0.0f || std::hypot(x - v.blackoutOrigin.x, y - v.blackoutOrigin.y) < v.blackoutFront;
}

struct Sight {
    float level = 0.0f;   // 0 ungesehen .. 1 deutlich gesehen
    bool inView = false;  // im Blickfeld und frei (auch im Dunkeln)
    bool direct = false;  // zentrales Sehen (wird angesehen)
    bool lit = false;
    float angle = 3.14f;  // kleinster Winkel zur Blickrichtung (rad)
    float dist = 1e9f;
};

// Wahrnehmung einer stehenden Figur (Fuesse bei feet, Hoehe height)
Sight perceive(const Viewer& v, world::World& w, vec3 feet, float height);
// Ist ein Punkt beleuchtet (Kachellicht, Taschenlampe, Stromausfall)?
bool litPoint(const Viewer& v, world::World& w, vec3 p);

}  // namespace lim::game
