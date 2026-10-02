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
};

class MeshLibrary {
public:
    void init(Gpu& gpu);
    int find(const std::string& name) const;  // -1 = unbekannt
    const GpuMesh& get(int id) const { return meshes_[(size_t)id]; }
    int add(Gpu& gpu, const std::string& name, const MeshData& data, u32 material);
    // V6: Modell aus mehreren Teilen mit eigenen Materialien (werden gemeinsam gezeichnet)
    int addGroup(Gpu& gpu, const std::string& name, const std::vector<std::pair<MeshData, u32>>& parts);

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

}  // namespace lim::gfx
