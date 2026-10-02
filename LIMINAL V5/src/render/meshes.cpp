#include "render/meshes.hpp"

#include <cmath>

#include "render/world_renderer.hpp"

namespace lim::gfx {

void MeshLibrary::init(Gpu& gpu) {
    add(gpu, "energy", makeEnergyBar(), FM_ITEM_ENERGY);
    add(gpu, "almond", makeBottle(), FM_ITEM_ALMOND);
}

int MeshLibrary::find(const std::string& name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? -1 : it->second;
}

int MeshLibrary::add(Gpu& gpu, const std::string& name, const MeshData& d, u32 material) {
    GpuMesh m;
    m.vb = gpu.createVertex(d.vertices.data(), (u32)(d.vertices.size() * sizeof(MeshVertex)), sizeof(MeshVertex));
    m.ib = gpu.createIndex(d.indices.data(), (u32)(d.indices.size() * sizeof(u16)));
    m.indexCount = (u32)d.indices.size();
    m.material = material;
    for (const auto& v : d.vertices) m.bounds.add(v.pos);
    meshes_.push_back(std::move(m));
    byName_[name] = (int)meshes_.size() - 1;
    return (int)meshes_.size() - 1;
}

// Energieriegel: flacher Riegel (22 x 6 x 2.4 cm) mit abgerundeten Kanten und
// flachgepressten Folienenden. u laeuft entlang des Riegels, v einmal ringsum.
MeshData makeEnergyBar() {
    MeshData d;
    const int segL = 24, segR = 24;
    const float len = 0.22f, hw = 0.03f, hh = 0.012f;
    for (int i = 0; i <= segL; ++i) {
        float u = (float)i / segL;
        float x = (u - 0.5f) * len;
        float endK = std::min(u, 1.0f - u) / 0.09f;  // 0 am Ende .. 1 innen
        float pinch = endK < 1.0f ? std::sqrt(std::max(endK, 0.0f)) : 1.0f;
        float sealW = hw * (1.0f + 0.12f * (1.0f - pinch));
        for (int j = 0; j <= segR; ++j) {
            float v = (float)j / segR;
            float a = v * 6.2831853f;
            // Superellipse -> abgerundetes Rechteck
            float c = std::cos(a), s = std::sin(a);
            float pc = std::copysign(std::pow(std::fabs(c), 0.35f), c);
            float ps = std::copysign(std::pow(std::fabs(s), 0.35f), s);
            vec3 p{x, pc * sealW, ps * hh * pinch + hh};
            vec3 n = normalize(vec3{0, pc / (sealW * sealW + 1e-6f), ps / (hh * hh + 1e-6f)});
            if (endK < 1.0f) n = normalize(n + vec3{u < 0.5f ? -0.6f : 0.6f, 0, 0} * (1.0f - endK));
            d.vertices.push_back({p, n, {1, 0, 0, 1}, {u, v}});
        }
    }
    for (int i = 0; i < segL; ++i)
        for (int j = 0; j < segR; ++j) {
            u16 a = (u16)(i * (segR + 1) + j), b = (u16)(a + segR + 1);
            d.indices.insert(d.indices.end(), {a, (u16)(a + 1), b, (u16)(a + 1), (u16)(b + 1), b});
        }
    return d;
}

// Flasche als Drehkoerper: Profil (Radius, Hoehe) von unten nach oben.
MeshData makeBottle() {
    MeshData d;
    const float prof[][2] = {{0.0f, 0.0f},    {0.046f, 0.0f},  {0.052f, 0.006f}, {0.054f, 0.02f},
                             {0.054f, 0.17f}, {0.05f, 0.19f},  {0.034f, 0.215f}, {0.02f, 0.226f},
                             {0.02f, 0.228f}, {0.023f, 0.23f}, {0.023f, 0.258f}, {0.02f, 0.262f},
                             {0.0f, 0.262f}};
    const int np = (int)(sizeof(prof) / sizeof(prof[0]));
    const float H = 0.262f;
    const int seg = 32;
    for (int i = 0; i < np; ++i) {
        // Normale aus dem Profil (Mittel der angrenzenden Kanten)
        int i0 = std::max(0, i - 1), i1 = std::min(np - 1, i + 1);
        float dr = prof[i1][0] - prof[i0][0], dz = prof[i1][1] - prof[i0][1];
        float nr = dz, nz = -dr;
        float nl = std::sqrt(nr * nr + nz * nz) + 1e-9f;
        nr /= nl, nz /= nl;
        for (int j = 0; j <= seg; ++j) {
            float u = (float)j / seg;
            float a = u * 6.2831853f;
            float c = std::cos(a), s = std::sin(a);
            vec3 p{c * prof[i][0], s * prof[i][0], prof[i][1]};
            vec3 n = normalize(vec3{c * nr, s * nr, nz});
            vec4 t{-s, c, 0, 1};
            d.vertices.push_back({p, n, t, {u, prof[i][1] / H}});
        }
    }
    for (int i = 0; i + 1 < np; ++i)
        for (int j = 0; j < seg; ++j) {
            u16 a = (u16)(i * (seg + 1) + j), b = (u16)(a + seg + 1);
            d.indices.insert(d.indices.end(), {a, b, (u16)(a + 1), (u16)(a + 1), b, (u16)(b + 1)});
        }
    return d;
}

}  // namespace lim::gfx
