// V13.3: Ausstattungsmodelle als frei bewegliche Netze (KREATUR_GameMaster.md: Gemaelde, die unbeobachtet verschwinden).
//
// Die festen Teile der Karte backt der Chunk-Bau in ein Netz je Chunk (world/chunk_builder.cpp). Was sich bewegt oder
// verschwindet, zeichnet die Sitzung als VisibleMesh mit dem Namen "prop:<modell>[@variante]". Die Materialien kommen
// aus derselben Bank wie die der Karte (world::propMaterial) - ein Gemaelde am Boden sieht so aus wie eines an der Wand.
#include <cmath>
#include <string>

#include "core/log.hpp"
#include "props/prop_library.hpp"
#include "render/renderer.hpp"
#include "world/generator.hpp"

namespace lim::gfx {

int Renderer::propMesh(const std::string& name, world::MaterialBank& bank) {
    if (auto it = propMeshes_.find(name); it != propMeshes_.end()) return it->second;
    std::string base = name;
    int variant = 0;
    if (const size_t at = name.find('@'); at != std::string::npos) {
        base = name.substr(0, at);
        variant = std::atoi(name.c_str() + at + 1);
    }
    int id = -1;
    const props::Model* m = props::model(base, variant);
    if (m && !m->skin && !m->parts.empty()) {
        // gleiche Materialien zusammenfassen (ein Gemaelde hat viele Farbflecken; jede Gruppe ist ein Zeichenaufruf)
        std::vector<std::pair<MeshData, u32>> parts;
        for (const props::Part& p : m->parts) {
            if (p.mesh.v.empty() || p.mesh.i.empty()) continue;
            const u32 mat = (u32)world::propMaterial(bank, p.mat) + kFixedMaterials;
            MeshData* d = nullptr;
            for (auto& [md, mm] : parts)
                if (mm == mat && md.vertices.size() + p.mesh.v.size() <= 65535) d = &md;
            if (!d) {
                if (p.mesh.v.size() > 65535) {
                    log::warn("prop:{}: Teil mit {} Punkten (hoechstens 65535) - uebersprungen", name, p.mesh.v.size());
                    continue;
                }
                parts.emplace_back(MeshData{}, mat);
                d = &parts.back().first;
            }
            const u16 base0 = (u16)d->vertices.size();
            for (const props::Vtx& v : p.mesh.v) {
                const vec3 a = std::fabs(v.n.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0};
                d->vertices.push_back({v.p, v.n, vec4(normalize(cross(a, v.n)), 1.0f), v.uv});
            }
            for (u32 i : p.mesh.i) d->indices.push_back((u16)(base0 + i));
        }
        if (!parts.empty()) {
            id = meshes_.addGroup(gpu_, "prop#" + std::to_string(propGen_) + ":" + name, parts);
            world_.syncMaterials(gpu_, bank);  // neue Eintraege der Bank sofort auf die GPU
            log::info("Bewegliches Modell prop:{} ({} Materialgruppen)", name, parts.size());
        }
    }
    if (id < 0) log::warn("Bewegliches Modell prop:{} unbekannt", name);
    propMeshes_[name] = id;
    return id;
}

}  // namespace lim::gfx
