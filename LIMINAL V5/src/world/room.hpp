// Raeume und Tueren (Port von V4, rooms.py).
//
// Ein Room ist ein achsenparalleles Rechteck (Innenraum; Waende liegen
// ausserhalb) mit Metadaten: Groesse, Boden- und Deckenhoehe, Typ, Zone, Seed,
// Tueren. Alle Details (Saeulen, Regale, Treppen, Lampen) sind reine Funktionen
// des Raum-Seeds - ein Raum sieht nach dem erneuten Laden exakt gleich aus.
//
// Lebenszyklus:  erzeugt (Generator)  ->  prepare() (Treppen, Einbauten, Licht;
// braucht aufgeloeste Portaltueren)  ->  tile() fuer jede Kachel (threadsicher,
// weil danach nichts mehr veraendert wird).
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/core.hpp"
#include "core/rng.hpp"
#include "world/level_def.hpp"
#include "world/materials.hpp"
#include "world/tile.hpp"

namespace lim::world {

constexpr double kCounterHeight = 0.95;  // Hoehe der Theke in Versorgungsraeumen (m)

struct TilePos {
    i64 x = 0, y = 0;
};
inline u64 tileKey(i64 x, i64 y) { return ((u64)(u32)(i32)x << 32) | (u64)(u32)(i32)y; }

enum class DoorKind : u8 { Door, Wide, Gate, Opening };

struct Door {
    i64 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int axis = 0;            // 0: Durchgang in x-Richtung (senkrechte Wand), roomA links
    Room* roomA = nullptr;   // links bzw. oben (bei Portalen ggf. im Nachbarsektor -> nullptr)
    Room* roomB = nullptr;   // rechts bzw. unten
    DoorKind kind = DoorKind::Door;
    double sill = 0.0, top = 2.2;
    bool portal = false;     // liegt auf einer Sektorgrenze
    bool owned = true;       // false: Kacheln gehoeren dem Nachbarsektor
    i64 remoteSx = 0, remoteSy = 0;
    char remoteSide = 0;     // 'a' oder 'b'
    u16 matSill = 0, matFrame = 0;
    double remoteAvgLight = 0.4;  // Helligkeit des Raums auf der anderen Seite (Portale)
    bool remoteResolved = false;

    i64 width() const { return axis == 0 ? (y1 - y0) : (x1 - x0); }
    bool contains(i64 x, i64 y) const { return x0 <= x && x < x1 && y0 <= y && y < y1; }
    std::pair<double, double> center() const { return {(x0 + x1) * 0.5, (y0 + y1) * 0.5}; }
    // Lokaler Raum (bei Portalen der Raum in diesem Sektor).
    Room* localRoom() const { return roomA ? roomA : roomB; }
    // Helligkeit auf der anderen Seite von r (V4: door.other(r).avg_light oder 0.4).
    double otherAvgLight(const Room* r) const;
    // Kacheln direkt vor der Tuer im Raum r und Richtung in den Raum.
    void inner(const Room* r, std::vector<TilePos>& tiles, int& dx, int& dy) const;
    Tile cell() const;
    const char* describe() const;
};

struct Fixture {
    enum Kind : u8 { Ceiling, Wall, Machine };
    double x, y;      // Weltposition (Kachelmitte)
    double intensity;
    double invR2;     // 1 / Radius^2 (V4-Lichtabfall)
    double radius;
    Kind kind;
    int dirX, dirY;   // Wand-/Automatenleuchten: Richtung in den Raum
};

struct ItemSpawn {
    std::string id;    // stabile ID "sx,sy,raum,platz" (wird im Spielstand gespeichert)
    std::string kind;  // Item-Schluessel (data/items.json)
    double x, y, z;
    int dirX, dirY;    // Richtung von der Wand in den Raum (Ausrichtung auf der Theke)
};

class Room {
public:
    Room(i64 sx, i64 sy, int index, i64 x0, i64 y0, i64 x1, i64 y1, const RoomTypeDef* type, const ZoneWeights& zw,
         int zone, u64 seed, bool corridor);

    // --- Identitaet und Geometrie -----------------------------------------------------
    i64 sx, sy;
    int index;
    i64 x0, y0, x1, y1;
    const RoomTypeDef* type;
    ZoneWeights zoneW;
    int zone;
    u64 seed;
    bool isCorridor;
    std::vector<Door*> doors;
    double floor = 0.0;
    bool fixedFloor = false;
    double height = 3.0;
    double stairStep = 0.3;

    i64 w() const { return x1 - x0; }
    i64 h() const { return y1 - y0; }
    double ceil() const { return floor + height; }
    int axis() const { return w() >= h() ? 0 : 1; }
    bool contains(i64 x, i64 y) const { return x0 <= x && x < x1 && y0 <= y && y < y1; }
    std::pair<double, double> center() const { return {(x0 + x1) * 0.5, (y0 + y1) * 0.5}; }
    i64 depthFrom(const Door& d) const { return d.axis == 0 ? w() : h(); }

    // --- Stil (vom Generator gesetzt) --------------------------------------------------
    u16 fmat = 0, cmat = 0, wmat = 0, lampmat = 0, walllampmat = 0, stairmat = 0, stairtop = 0;
    u16 mPart = 0, mPartTop = 0, mShelf = 0, mShelfTop = 0, mCrate = 0, mCrateTop = 0;
    u16 mMachine = 0, mMachineTop = 0, mStage = 0, mStageTop = 0, mMono = 0;
    u16 mCounter = 0, mCounterTop = 0, mVend = 0, mVendTop = 0;
    u16 mFixture = 0;  // V5: Gehaeuse von Leuchten und Automaten
    double ambient = 0.5, lampInt = 0.8;
    std::string lightStyle = "panels";
    bool lightFallback = false;  // siehe LightStyleDef::centerFallback
    double avgLight = 0.5;
    Rgb fog{0.1, 0.1, 0.1};
    double fogDensity = 0.05;
    int flicker = 0;          // 0 ruhig, 1 leises Pulsieren, 2 alternde Roehre
    Rgb lampColor{1, 1, 1};
    Rgb wallColor{0.5, 0.5, 0.5};
    bool baseboard = false;

    // --- Versorgungsraum (V3) ----------------------------------------------------------
    std::vector<ItemSpawn> items;
    bool hasMachine() const { return machine_.has_value(); }
    std::optional<TilePos> machineTile() const;  // Weltkoordinaten
    int machineDirX = 0, machineDirY = 0;
    void planSupply(const LevelDef& level, double oddsNone, double oddsOne);

    // --- Vorbereitung und Kacheln -------------------------------------------------------
    void prepare();
    bool prepared() const { return prepared_; }
    // Kachel an (x, y) im Raum (nur nach prepare()).
    Tile tile(i64 x, i64 y) const;
    double lightAt(i64 x, i64 y) const;
    const std::vector<Fixture>& fixtures() const { return fixtures_; }
    bool isLamp(i64 x, i64 y) const { return bitAt(lamps_, x, y); }

private:
    enum class FeatKind : u8 { Counter, Pillars, Colonnade, Cubicles, Shelves, Crates, Machines, Podium, Monolith };
    struct Feat {
        FeatKind kind;
        i64 sp = 0, th = 0, margin = 0, ox = 0, oy = 0;  // Saeulenraster
    };
    enum class CeilKind : u8 { Beams, Vault, Pipes };
    struct FeatResult {
        enum Kind : u8 { None, Solid, Block } kind = None;
        double h = 0.0;
        u16 side = 0, top = 0;
    };

    void planStairs(const Door& d);
    void planClearance(const Door& d);
    void planFeatures(lim::Rng& rng);
    void gridPillars(i64 sp, i64 th, i64 margin);
    void planLights(lim::Rng& rng);
    FeatResult featureAt(i64 lx, i64 ly) const;
    FeatResult counterAt(i64 lx, i64 ly) const;
    void acrossAlong(i64 lx, i64 ly, i64& a, i64& b, i64& wa, i64& la) const;
    double ceilingAt(i64 lx, i64 ly, double fl) const;
    bool occludes(i64 x, i64 y) const;
    bool shadowed(double fx, double fy, double cx, double cy) const;

    bool bitAt(const std::vector<u8>& bits, i64 x, i64 y) const {
        if (!contains(x, y)) return false;
        return bits[(size_t)((y - y0) * w() + (x - x0))] != 0;
    }
    void setBit(std::vector<u8>& bits, i64 x, i64 y) {
        if (contains(x, y)) bits[(size_t)((y - y0) * w() + (x - x0))] = 1;
    }

    bool prepared_ = false;
    // Versorgungsraum: lokale Koordinaten
    std::vector<std::pair<i64, i64>> counter_;
    std::optional<std::pair<i64, i64>> machine_;
    // Treppen: Hoehe je Kachel (in Einfuegereihenfolge wie das dict in V4)
    std::vector<double> stairH_;         // NaN = keine Stufe, Index (y-y0)*w+(x-x0)
    std::vector<TilePos> stairOrder_;
    double stairLight_ = 0.0;
    std::vector<u8> clear_, lamps_, wallLamps_, occ_;
    std::vector<Feat> feats_;
    std::vector<CeilKind> ceilFns_;
    i64 colStep_ = 2, shelfSeg_ = 6, beamStep_ = 3;
    double shelfH_ = 2.2, monoH_ = 1.0, beamDrop_ = 0.5;
    std::vector<Fixture> fixtures_;
    // Suchraster fuer Leuchten (8 x 8 Kacheln je Zelle)
    i64 bx0_ = 0, by0_ = 0, bw_ = 0, bh_ = 0;
    std::vector<std::vector<int>> buckets_;
    bool hasOccluders_ = false;
};

}  // namespace lim::world
