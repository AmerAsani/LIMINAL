// Prozedurale Weltgenerierung (Port von V4, generator.py).
//
// Die unendliche Ebene ist in Sektoren von S x S Kacheln aufgeteilt. Jeder Sektor
// entsteht unabhaengig von allen anderen nur aus (Seed, sx, sy):
//   1. Portale auf jeder Sektorkante (von beiden Seiten identisch berechnet)
//   2. BSP-Teilung mit Zonenparametern, Korridorstreifen
//   3. Raumtypen je Blatt
//   4. Tueren (spannender Baum -> alles erreichbar, dazu Schleifen)
//   5. Bodenhoehen (Treppen passen immer), 6. Hoehen, Materialien, Licht
//
// generateSector() ist const und threadsicher; mehrere Sektoren koennen
// parallel entstehen.
#pragma once

#include <memory>

#include "core/rng.hpp"
#include "world/level_def.hpp"
#include "world/materials.hpp"
#include "world/sector.hpp"
#include "world/zone_field.hpp"

namespace lim::world {

class Generator {
public:
    Generator(const LevelDef& level, i64 seed, MaterialBank& bank, double itemOddsNone, double itemOddsOne,
              SupplyRules supplyRules = {});

    std::unique_ptr<Sector> generateSector(i64 sx, i64 sy) const;
    // Portale einer Sektorkante: kind 0 = senkrechte Kante links von (ex, ey), 1 = waagrechte oben.
    std::vector<std::pair<i64, i64>> edgePortals(int kind, i64 ex, i64 ey) const;
    double rarity(double x, double y) const;

    const ZoneField& zones() const { return zones_; }
    const LevelDef& level() const { return level_; }
    i64 seed() const { return seed_; }
    MaterialBank& bank() const { return bank_; }

private:
    struct Ctx;
    struct Leaf;
    int bsp(Ctx& c, i64 x0, i64 y0, i64 x1, i64 y1, int depth) const;
    int corridorSplit(Ctx& c, i64 x0, i64 y0, i64 x1, i64 y1, int depth, i64 t, i64 cw, int axis, i64 minLeaf,
                      bool endless, bool passage = false) const;
    bool wallOk(const Ctx& c, int axis, i64 s, i64 t, i64 x0, i64 y0, i64 x1, i64 y1) const;
    bool pick(Ctx& c, int axis, i64 lo, i64 hi, i64 t, i64 x0, i64 y0, i64 x1, i64 y1, i64& out) const;
    int leaf(Ctx& c, i64 x0, i64 y0, i64 x1, i64 y1, bool corridor = false, bool big = false, bool endless = false) const;
    void makeRooms(Ctx& c) const;
    std::vector<int> collect(Ctx& c, int node) const;
    void doorSpec(lim::Rng& rng, const Room& ra, const Room& rb, i64 overlap, DoorKind& kind, i64& wd) const;
    void addDoor(Sector& sec, int axis, i64 s, i64 t, Room* ra, Room* rb, i64 p, i64 wd, DoorKind kind) const;
    void makeDoors(Ctx& c) const;
    void makePortals(Ctx& c) const;
    void resolveFloors(Sector& sec) const;
    void resolveHeights(Sector& sec) const;
    void style(Room& room) const;
    void doorStyle(Door& door) const;

    const LevelDef& level_;
    i64 seed_;
    MaterialBank& bank_;
    ZoneField zones_;
    double oddsNone_, oddsOne_;
    SupplyRules supplyRules_;  // V6: Seltenheit einzelner Items (leer = V4-Verteilung)
    // Vorberechnete Musterkennungen je Zone
    std::vector<PlanePattern> zoneFloor_, zoneCeil_;
    std::vector<WallPattern> zoneWall_;
};

}  // namespace lim::world
