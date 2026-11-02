#include "render/item_models.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "core/log.hpp"
#include "props/prop_library.hpp"
#include "render/meshes.hpp"
#include "render/texture_gen.hpp"
#include "render/world_renderer.hpp"
#include "world/imported_textures.hpp"
#include "world/materials.hpp"

namespace lim::gfx {

namespace {

struct ItemModel {
    const char* mesh;  // Name in der Netzbibliothek (Daten, Prefabs, Spielcode)
    const char* file;  // Modell aus Blender (data/models/<file>.glb)
};
constexpr ItemModel kItems[] = {
    {"energy", "item_energy"},         {"almond", "item_almond"},       {"battery", "item_battery"},
    {"note", "item_note"},             {"fuse", "item_fuse"},           {"fusebox", "item_fusebox"},
    {"fusebox_empty", "item_fusebox_empty"}, {"vending", "item_vending"}, {"exit_door", "item_exit_door"},
    {"elev_frame", "item_elev_frame"}, {"elev_door", "item_elev_door"},
};

// feste Materialien mit Atlas (Etiketten, Front, Schilder) - Name hinter "fm=" in Blender
const std::pair<const char*, u32> kFm[] = {
    {"item_energy", FM_ITEM_ENERGY}, {"item_almond", FM_ITEM_ALMOND}, {"item_battery", FM_ITEM_BATTERY},
    {"item_note", FM_ITEM_NOTE},     {"item_fuse", FM_ITEM_FUSE},     {"fusebox", FM_FUSEBOX},
    {"fuse_glow", FM_FUSE_GLOW},     {"vend_body", FM_VEND_BODY},     {"vend_glow", FM_VEND_GLOW},
    {"vend_glass", FM_VEND_GLASS},   {"exit_frame", FM_EXIT_FRAME},   {"exit_door", FM_EXIT_DOOR},
    {"exit_sign", FM_EXIT_SIGN},     {"flash_lens", FM_FLASH_LENS},   {"elev_display", FM_ELEV_DISPLAY},
};

world::Surface surfaceOf(props::Look l) {
    using props::Look;
    using world::Surface;
    switch (l) {
        case Look::Paint: return Surface::PropPaint;
        case Look::CarPaint: return Surface::PropCarPaint;
        case Look::Metal: return Surface::PropMetal;
        case Look::Galvanized: return Surface::PropGalvanized;
        case Look::Plastic: return Surface::PropPlastic;
        case Look::Rubber: return Surface::PropRubber;
        case Look::Glass: return Surface::PropGlass;
        case Look::Water: return Surface::PropWater;
        case Look::RoadPaint: return Surface::PropRoadPaint;
        case Look::Sign: return Surface::PropSign;
        case Look::Stencil: return Surface::PropStencil;
        case Look::Wood: return Surface::PropWood;
        case Look::Fabric: return Surface::PropFabric;
        case Look::Concrete: return Surface::PropConcrete;
        case Look::Cardboard: return Surface::PropCardboard;
        case Look::Glow: return Surface::PropGlow;
        case Look::Ceramic: return Surface::PropCeramic;
    }
    return Surface::PropPaint;
}

struct Table {
    std::vector<GpuMaterial> mats;  // Platz FM_FIG_END + i
    std::vector<std::string> keys;
    std::vector<float> meters;      // Kachelgroesse der Textur je Platz (UV in Metern -> Textureinheiten)
    bool scanned = false;
};

Table& table() {
    static Table t;
    return t;
}

float layerMeters(u32 layer) {
    if (layer >= (u32)L_IMP0) {
        const auto& imp = world::importedTextures();
        const size_t i = layer - (u32)L_IMP0;
        return i < imp.size() ? imp[i].meters : 2.0f;
    }
    return layer < (u32)L_COUNT ? kLayers[layer].meters : 1.0f;
}

// Material eines Teils: festes Atlas-Material oder ein freier Platz mit einem Material wie die Ausstattung.
// meters: Kachelgroesse fuer die UV (Atlas: 1). -1: kein Material (Teil wird uebersprungen)
int materialFor(const props::MatDesc& d, float& meters) {
    meters = 1.0f;
    if (!d.fm.empty()) {
        for (const auto& [k, v] : kFm)
            if (d.fm == k) return (int)v;
        log::warn("Fundstueck: unbekanntes festes Material fm='{}'", d.fm);
        return -1;
    }
    Table& t = table();
    const std::string key = std::format("{}|{:.4f},{:.4f},{:.4f}|{}|{:.3f}|{:.3f}|{:.3f}", (int)d.look, d.r, d.g, d.b, d.tex,
                                        d.glow, d.rough, d.metal);
    for (size_t i = 0; i < t.keys.size(); ++i)
        if (t.keys[i] == key) {
            meters = t.meters[i];
            return (int)(FM_FIG_END + i);
        }
    if (FM_FIG_END + t.mats.size() >= kFixedMaterials) {
        log::warn("Fundstueck: keine freien festen Materialien mehr ({} belegt)", t.mats.size());
        return -1;
    }
    world::Material m;
    m.rgb = {d.r, d.g, d.b};
    m.emissive = d.glow > 0.0f;
    m.surface = surfaceOf(d.look);
    const int tl = d.tex.empty() ? -1 : world::importedLayer(d.tex);
    if (!d.tex.empty() && tl < 0) log::warn("Fundstueck: unbekannte Textur '{}'", d.tex);
    const u8 l = tl < 0 ? (u8)255 : (u8)tl;
    m.layer = {l, l, l};
    m.roughMul = d.rough, m.metalAdd = d.metal;
    GpuMaterial g = toGpuMaterial(m);
    if (d.glow > 0.0f) g.emissive *= d.glow;
    t.mats.push_back(g);
    t.keys.push_back(key);
    t.meters.push_back(std::max(0.05f, layerMeters(g.layers & 0xFFu)));
    meters = t.meters.back();
    return (int)(FM_FIG_END + t.mats.size() - 1);
}

void scan() {
    Table& t = table();
    if (t.scanned) return;
    t.scanned = true;
    for (const ItemModel& it : kItems) {
        const props::Model* m = props::model(it.file);
        if (!m || m->file.empty()) continue;
        for (const props::Part& p : m->parts) {
            float meters;
            materialFor(p.mat, meters);
        }
    }
}

// Tangenten aus den UV (Richtung +u; w: Vorzeichen der Bitangente in Richtung +v)
void tangents(MeshData& d) {
    std::vector<vec3> tu(d.vertices.size(), vec3{0, 0, 0}), tv(d.vertices.size(), vec3{0, 0, 0});
    for (size_t i = 0; i + 2 < d.indices.size(); i += 3) {
        const u16 a = d.indices[i], b = d.indices[i + 1], c = d.indices[i + 2];
        const MeshVertex &A = d.vertices[a], &B = d.vertices[b], &C = d.vertices[c];
        const vec3 e1 = B.pos - A.pos, e2 = C.pos - A.pos;
        const vec2 d1{B.uv.x - A.uv.x, B.uv.y - A.uv.y}, d2{C.uv.x - A.uv.x, C.uv.y - A.uv.y};
        const float det = d1.x * d2.y - d2.x * d1.y;
        if (std::fabs(det) < 1e-14f) continue;
        const float r = 1.0f / det;
        const vec3 su = (e1 * d2.y - e2 * d1.y) * r, sv = (e2 * d1.x - e1 * d2.x) * r;
        for (u16 k : {a, b, c}) tu[k] = tu[k] + su, tv[k] = tv[k] + sv;
    }
    for (size_t i = 0; i < d.vertices.size(); ++i) {
        MeshVertex& v = d.vertices[i];
        vec3 t = tu[i] - v.normal * dot(v.normal, tu[i]);
        if (dot(t, t) < 1e-20f) {
            const vec3 a = std::fabs(v.normal.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0};
            t = cross(a, v.normal);
        }
        t = normalize(t);
        const float w = dot(cross(v.normal, t), tv[i]) < 0.0f ? -1.0f : 1.0f;
        v.tangent = vec4(t, w);
    }
}

}  // namespace

void itemModelMaterials(std::vector<GpuMaterial>& out) {
    scan();
    const Table& t = table();
    for (size_t i = 0; i < t.mats.size(); ++i)
        if (FM_FIG_END + i < out.size()) out[FM_FIG_END + i] = t.mats[i];
    if (!t.mats.empty()) log::info("Fundstuecke: {} Materialien auf den Plaetzen {}..{}", t.mats.size(), (int)FM_FIG_END,
                                   (int)(FM_FIG_END + t.mats.size() - 1));
}

void addItemModels(MeshLibrary& lib, Gpu& gpu) {
    scan();
    for (const ItemModel& it : kItems) {
        const props::Model* m = props::model(it.file);
        if (!m || m->parts.empty() || m->file.empty()) continue;
        std::vector<std::pair<MeshData, u32>> parts;
        size_t tris = 0;
        for (const props::Part& p : m->parts) {
            float meters = 1.0f;
            const int mat = materialFor(p.mat, meters);
            if (mat < 0) continue;
            if (p.mesh.v.size() > 65535) {
                log::warn("Fundstueck {}: Teil mit {} Punkten (hoechstens 65535) - uebersprungen", it.file, p.mesh.v.size());
                continue;
            }
            MeshData d;
            d.vertices.reserve(p.mesh.v.size());
            const float s = 1.0f / meters;
            for (const props::Vtx& v : p.mesh.v) d.vertices.push_back({v.p, v.n, vec4(1, 0, 0, 1), vec2{v.uv.x * s, v.uv.y * s}});
            d.indices.assign(p.mesh.i.begin(), p.mesh.i.end());
            tangents(d);
            tris += d.indices.size() / 3;
            parts.emplace_back(std::move(d), (u32)mat);
        }
        if (parts.empty()) continue;
        lib.addGroup(gpu, it.mesh, parts);
        log::info("Fundstueck: {} aus {}.glb ({} Teile, {} Dreiecke)", it.mesh, it.file, parts.size(), tris);
    }
}

}  // namespace lim::gfx
