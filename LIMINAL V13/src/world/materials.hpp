// Materialien der Welt: Farbe plus Oberflaechenmuster (Port von V4, materials.py).
//
// Die Generierung legt Materialien nur ueber die MaterialBank an; dort werden
// sie dedupliziert und bekommen eine kleine ID, die in den Kacheln steht. Der
// Renderer uebersetzt IDs in GPU-Materialien (Textur-Ebenen + Farbe). Die Bank
// ist threadsicher, weil Sektoren parallel erzeugt werden.
#pragma once

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "core/core.hpp"

namespace lim::world {

// Muster fuer Boeden und Decken.
enum class PlanePattern : u8 { None, Carpet, Slabs, BigTiles, Grate, Tiles, Checker, Lines, Planks, Count };  // V11: Planks
// Muster fuer Waende.
enum class WallPattern : u8 { Plain, Wallpaper, Blocks, Bricks, Panels, Concrete, Boards, Tiles, Count };  // V11: Boards, Tiles

PlanePattern planePatternFromName(const std::string& s);
WallPattern wallPatternFromName(const std::string& s);

// Besondere Oberflaechen fuer Einbauten (steuern Textur und PBR-Werte).
enum class Surface : u8 {
    Default,
    Partition,  // Bueroabtrennungen
    Shelf,      // Regale
    Crate,      // Kisten
    Machine,    // Maschinen
    Stage,      // Podest
    Monolith,
    Counter,    // Theke (Holzdekor)
    CounterTop,
    Vending,    // Automat (leuchtet)
    Trim,       // Sockel, Tuerrahmen
    Metal,
    // V11: Ausstattung (props/prop_library.hpp, Look)
    PropPaint, PropCarPaint, PropMetal, PropGalvanized, PropPlastic, PropRubber, PropGlass, PropWater,
    PropRoadPaint, PropSign, PropStencil, PropWood, PropFabric, PropConcrete, PropCardboard, PropGlow, PropCeramic,
};

struct Band {
    double z0, z1;  // relativ zum Boden (kind 2: relativ zur Decke, negativ)
    u16 mat;
    // V12: 0 = vorstehende Leiste (Sockel), 1 = buendiger Anstrich (Feuchtigkeitsrand), 2 = Leiste unter der Decke
    // (Winkelprofil der Rasterdecke)
    u8 kind = 0;
    bool operator<(const Band& o) const { return std::tie(z0, z1, mat, kind) < std::tie(o.z0, o.z1, o.mat, o.kind); }
    bool operator==(const Band& o) const { return z0 == o.z0 && z1 == o.z1 && mat == o.mat && kind == o.kind; }
};

struct Material {
    u16 id = 0;
    std::array<double, 3> rgb{};
    bool emissive = false;
    PlanePattern plane = PlanePattern::None;
    WallPattern wall = WallPattern::Plain;
    Surface surface = Surface::Default;
    std::vector<Band> bands;
    double seamDark = 0.72;
    // V12: Textur-Ebene aus Bilddateien je Richtung (oben = Boden, unten = Decke, Wand); 255 = nach Muster
    std::array<u8, 3> layer{255, 255, 255};
    // V13.3: Rauheitsfaktor und Metall-Zuschlag aus Blender (< 0: nach Oberflaeche)
    float roughMul = -1.0f, metalAdd = -1.0f;
};

class MaterialBank {
public:
    // 14 Bit im Vertex, davon 64 feste GPU-Materialien (Items, V10: Figuren) abgezogen
    static constexpr std::size_t kCapacity = 16320;

    MaterialBank();
    u16 get(std::array<double, 3> rgb, PlanePattern plane = PlanePattern::None, WallPattern wall = WallPattern::Plain,
            bool emissive = false, std::vector<Band> bands = {}, double seamDark = 0.72,
            Surface surface = Surface::Default, std::array<u8, 3> layer = {255, 255, 255},
            std::array<float, 2> pbr = {-1.0f, -1.0f});
    const Material& operator[](u16 id) const { return *items_[id]; }
    std::size_t size() const { return count_.load(std::memory_order_acquire); }

private:
    using Key = std::tuple<std::array<double, 3>, u8, u8, bool, std::vector<Band>, double, u8, std::array<u8, 3>, std::array<float, 2>>;
    std::mutex mutex_;
    std::map<Key, u16> index_;
    std::vector<std::unique_ptr<Material>> items_;  // feste Kapazitaet -> Zeiger bleiben gueltig
    std::atomic<std::size_t> count_{0};
};

}  // namespace lim::world
