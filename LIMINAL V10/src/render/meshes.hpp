// Netze frei platzierbarer Objekte (Items, Requisiten). Werden prozedural
// erzeugt; neue Modelle lassen sich hier registrieren und ueber ihren Namen
// in Daten (data/items.json, Prefabs) verwenden.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "render/gpu.hpp"
#include "render/gpu_types.hpp"

namespace lim::gfx {
struct SkinPart;
}

namespace lim::gfx {

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<u16> indices;
};

struct GpuMesh {
    Buffer vb, ib;
    u32 indexCount = 0;
    u32 material = 0;
    Aabb bounds;
    int next = -1;  // V6: weiteres Teil desselben Modells (anderes Material), -1 = Ende
    bool skinned = false;  // V10: Figur (SkinVertex, 32-Bit-Indizes)
};

class MeshLibrary {
public:
    void init(Gpu& gpu);
    int find(const std::string& name) const;  // -1 = unbekannt
    const GpuMesh& get(int id) const { return meshes_[(size_t)id]; }
    int add(Gpu& gpu, const std::string& name, const MeshData& data, u32 material);
    // V6: Modell aus mehreren Teilen mit eigenen Materialien (werden gemeinsam gezeichnet)
    int addGroup(Gpu& gpu, const std::string& name, const std::vector<std::pair<MeshData, u32>>& parts);
    // V10: Figur aus Teilen mit Skinning (render/figure_models.hpp)
    int addSkinned(Gpu& gpu, const std::string& name, const std::vector<SkinPart>& parts);

private:
    std::vector<GpuMesh> meshes_;
    std::unordered_map<std::string, int> byName_;
};

// Bausteine
MeshData makeEnergyBar();
MeshData makeBottle();
// V6: Strichmaennchen
MeshData makeCylinder(int segments);
MeshData makeSphere(int segments, int rings);
MeshData makeSmile();
// V6: Getraenke- und Snackautomat: Gehaeuse, Leuchtteile (Schild, Anzeige), Glasfront
struct VendingParts {
    MeshData body, glow, glass;
};
VendingParts makeVending();
// V7: Taschenlampe (Griff/Kopf und Glas getrennt), Batterie, Notizzettel, Notausgang
struct FlashlightParts {
    MeshData body, lens;
};
FlashlightParts makeFlashlight();
MeshData makeBattery();
MeshData makeNote();
struct ExitParts {
    MeshData frame, door, sign;
};
ExitParts makeExitDoor();
// V8: Aufzug. Lokal: Ursprung am Boden in der Mitte der Tuerebene, +x in den Raum (aus der Kabine
// hinaus), Kabine bei x -2..0, y -1..1. Rahmen und Kabine (Stahl), ein Tuerfluegel (1 m breit,
// Mitte bei y = 0), Deckenlicht, Anzeige (aussen und innen, Feld des Atlas)
struct ElevatorParts {
    MeshData frame, door, light;
};
ElevatorParts makeElevator();
MeshData makeElevatorDisplay(int cell);
// V9: Sicherungskasten an der Wand (lokal: Wand bei x = -0.04, +x in den Raum; Tuer offen), mit oder
// ohne Sicherung; Sicherung als Item (liegend, Mitte im Ursprung)
struct FuseBoxParts {
    MeshData body, fuse, lamp;
};
FuseBoxParts makeFuseBox(bool withFuse);
MeshData makeFuseItem();

}  // namespace lim::gfx
