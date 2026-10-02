#include "render/meshes.hpp"

#include <cmath>

#include "render/vending_layout.hpp"
#include "render/world_renderer.hpp"

namespace lim::gfx {

void MeshLibrary::init(Gpu& gpu) {
    add(gpu, "energy", makeEnergyBar(), FM_ITEM_ENERGY);
    add(gpu, "almond", makeBottle(), FM_ITEM_ALMOND);
    // V6: Bausteine des Strichmaennchens (werden je Bild passend skaliert)
    add(gpu, "stick_limb", makeCylinder(12), FM_BODY);
    add(gpu, "stick_joint", makeSphere(12, 8), FM_BODY);
    add(gpu, "stick_head", makeSphere(28, 18), FM_BODY);
    add(gpu, "stick_eye", makeSphere(10, 6), FM_BODY_FACE);
    add(gpu, "stick_smile", makeSmile(), FM_BODY_FACE);
    // V6: Automat (ersetzt den leuchtenden Block der Weltgeometrie)
    VendingParts v = makeVending();
    addGroup(gpu, "vending", {{v.body, FM_VEND_BODY}, {v.glow, FM_VEND_GLOW}, {v.glass, FM_VEND_GLASS}});
}

// --- V6: Automat -----------------------------------------------------------------------------------
namespace {

// Viereck p0..p3 (gegen den Uhrzeigersinn um n) mit Atlas-Koordinaten; Tangente zeigt in Richtung +u
void quad(MeshData& d, vec3 p0, vec3 p1, vec3 p2, vec3 p3, vec3 n, vec4 t, vec2 t0, vec2 t1, vec2 t2, vec2 t3) {
    u16 b = (u16)d.vertices.size();
    d.vertices.push_back({p0, n, t, t0});
    d.vertices.push_back({p1, n, t, t1});
    d.vertices.push_back({p2, n, t, t2});
    d.vertices.push_back({p3, n, t, t3});
    d.indices.insert(d.indices.end(), {b, (u16)(b + 1), (u16)(b + 2), b, (u16)(b + 2), (u16)(b + 3)});
}

// Flaeche parallel zur Front (Normale +x) auf Hoehe xPlane, Bereich in Frontkoordinaten (X rechts, Z unten)
void frontQuad(MeshData& d, float xPlane, float X0, float Z0, float X1, float Z1) {
    using namespace vend;
    auto P = [&](float X, float Z) { return vec3{xPlane, kHalfW - X, kHeight - Z}; };
    auto T = [&](float X, float Z) { return vec2{frontU(X), atlasV(Z)}; };
    quad(d, P(X0, Z0), P(X1, Z0), P(X1, Z1), P(X0, Z1), {1, 0, 0}, {0, -1, 0, 1}, T(X0, Z0), T(X1, Z0), T(X1, Z1), T(X0, Z1));
}

// Seitenflaechen eines vorstehenden Kastens (Rahmen um eine Frontflaeche) von xa (hinten) bis xb (vorn)
void frontBoxSides(MeshData& d, float xa, float xb, float X0, float Z0, float X1, float Z1) {
    using namespace vend;
    auto T = [&](float X, float Z) { return vec2{frontU(X), atlasV(Z)}; };
    float y0 = kHalfW - X0, y1 = kHalfW - X1, z0 = kHeight - Z0, z1 = kHeight - Z1;
    // oben (Normale +z), unten (-z), links (+y), rechts (-y)
    quad(d, {xa, y0, z0}, {xa, y1, z0}, {xb, y1, z0}, {xb, y0, z0}, {0, 0, 1}, {0, -1, 0, 1}, T(X0, Z0), T(X1, Z0), T(X1, Z0 + 0.004f), T(X0, Z0 + 0.004f));
    quad(d, {xb, y0, z1}, {xb, y1, z1}, {xa, y1, z1}, {xa, y0, z1}, {0, 0, -1}, {0, -1, 0, 1}, T(X0, Z1 - 0.004f), T(X1, Z1 - 0.004f), T(X1, Z1), T(X0, Z1));
    quad(d, {xa, y0, z1}, {xa, y0, z0}, {xb, y0, z0}, {xb, y0, z1}, {0, 1, 0}, {1, 0, 0, 1}, T(X0, Z1), T(X0, Z0), T(X0 + 0.004f, Z0), T(X0 + 0.004f, Z1));
    quad(d, {xb, y1, z1}, {xb, y1, z0}, {xa, y1, z0}, {xa, y1, z1}, {0, -1, 0}, {-1, 0, 0, 1}, T(X1 - 0.004f, Z1), T(X1 - 0.004f, Z0), T(X1, Z0), T(X1, Z1));
}

}  // namespace

VendingParts makeVending() {
    using namespace vend;
    VendingParts out;
    MeshData& b = out.body;
    const float c = kChamfer, X1 = 2.0f * kHalfW;
    const float zTop = kHeight, zBase = kBase;
    auto TF = [&](float X, float Z) { return vec2{frontU(X), atlasV(Z)}; };
    auto TS = [&](float x, float Z) { return vec2{sideU(kFront - x), atlasV(Z)}; };
    // --- Sockel (zurueckgesetzt, schwarz) ---
    {
        const float xf = kFront - 0.04f, hw = kHalfW - 0.02f, Zt = kHeight - kBase;
        quad(b, {xf, hw, zBase}, {xf, -hw, zBase}, {xf, -hw, 0}, {xf, hw, 0}, {1, 0, 0}, {0, -1, 0, 1}, TF(0.02f, Zt), TF(X1 - 0.02f, Zt), TF(X1 - 0.02f, kHeight), TF(0.02f, kHeight));
        quad(b, {kBack, hw, zBase}, {xf, hw, zBase}, {xf, hw, 0}, {kBack, hw, 0}, {0, 1, 0}, {-1, 0, 0, -1}, TS(kBack, Zt), TS(xf, Zt), TS(xf, kHeight), TS(kBack, kHeight));
        quad(b, {xf, -hw, zBase}, {kBack, -hw, zBase}, {kBack, -hw, 0}, {xf, -hw, 0}, {0, -1, 0}, {-1, 0, 0, 1}, TS(xf, Zt), TS(kBack, Zt), TS(kBack, kHeight), TS(xf, kHeight));
    }
    // --- Front mit Fensteroeffnung ---
    const Rect& w = kWindow;
    frontQuad(b, kFront, c, 0.0f, X1 - c, w.z0);                     // oben
    frontQuad(b, kFront, c, w.z1, X1 - c, kHeight - kBase);          // unten
    frontQuad(b, kFront, c, w.z0, w.x0, w.z1);                       // links
    frontQuad(b, kFront, w.x1, w.z0, X1 - c, w.z1);                  // rechts
    // Laibung des Fensters (Glas sitzt etwas zurueck)
    {
        const float xa = kFront - kGlassIn, xb = kFront;
        float y0 = kHalfW - w.x0, y1 = kHalfW - w.x1, z0 = kHeight - w.z0, z1 = kHeight - w.z1;
        quad(b, {xa, y0, z0}, {xb, y0, z0}, {xb, y1, z0}, {xa, y1, z0}, {0, 0, -1}, {0, -1, 0, 1}, TF(w.x0, w.z0 - 0.01f), TF(w.x0, w.z0 - 0.01f), TF(w.x1, w.z0 - 0.01f), TF(w.x1, w.z0 - 0.01f));
        quad(b, {xb, y0, z1}, {xa, y0, z1}, {xa, y1, z1}, {xb, y1, z1}, {0, 0, 1}, {0, -1, 0, 1}, TF(w.x0, w.z1 + 0.01f), TF(w.x0, w.z1 + 0.01f), TF(w.x1, w.z1 + 0.01f), TF(w.x1, w.z1 + 0.01f));
        quad(b, {xa, y0, z1}, {xb, y0, z1}, {xb, y0, z0}, {xa, y0, z0}, {0, -1, 0}, {1, 0, 0, 1}, TF(w.x0 - 0.01f, w.z1), TF(w.x0 - 0.01f, w.z1), TF(w.x0 - 0.01f, w.z0), TF(w.x0 - 0.01f, w.z0));
        quad(b, {xb, y1, z1}, {xa, y1, z1}, {xa, y1, z0}, {xb, y1, z0}, {0, 1, 0}, {-1, 0, 0, 1}, TF(w.x1 + 0.01f, w.z1), TF(w.x1 + 0.01f, w.z1), TF(w.x1 + 0.01f, w.z0), TF(w.x1 + 0.01f, w.z0));
    }
    // Fasen der senkrechten Frontkanten
    {
        const float s = 0.70710678f, Zb = kHeight - kBase;
        quad(b, {kFront - c, kHalfW, zTop}, {kFront, kHalfW - c, zTop}, {kFront, kHalfW - c, zBase}, {kFront - c, kHalfW, zBase}, {s, s, 0}, {s, -s, 0, 1}, TF(0, 0), TF(c, 0), TF(c, Zb), TF(0, Zb));
        quad(b, {kFront, -kHalfW + c, zTop}, {kFront - c, -kHalfW, zTop}, {kFront - c, -kHalfW, zBase}, {kFront, -kHalfW + c, zBase}, {s, -s, 0}, {-s, -s, 0, 1}, TF(X1 - c, 0), TF(X1, 0), TF(X1, Zb), TF(X1 - c, Zb));
    }
    // Seiten, Oberseite, Rueckseite, Unterseite des Korpus
    {
        const float Zb = kHeight - kBase, xs = kFront - c;
        quad(b, {kBack, kHalfW, zTop}, {xs, kHalfW, zTop}, {xs, kHalfW, zBase}, {kBack, kHalfW, zBase}, {0, 1, 0}, {-1, 0, 0, -1}, TS(kBack, 0), TS(xs, 0), TS(xs, Zb), TS(kBack, Zb));
        quad(b, {xs, -kHalfW, zTop}, {kBack, -kHalfW, zTop}, {kBack, -kHalfW, zBase}, {xs, -kHalfW, zBase}, {0, -1, 0}, {-1, 0, 0, 1}, TS(xs, 0), TS(kBack, 0), TS(kBack, Zb), TS(xs, Zb));
        vec2 dusty0 = TS(kBack, 0.004f), dusty1 = TS(kFront, 0.004f);
        quad(b, {kBack, kHalfW, zTop}, {kBack, -kHalfW, zTop}, {kFront - c, -kHalfW, zTop}, {kFront - c, kHalfW, zTop}, {0, 0, 1}, {-1, 0, 0, 1}, dusty0, dusty0, dusty1, dusty1);
        quad(b, {kFront - c, kHalfW, zTop}, {kFront - c, -kHalfW, zTop}, {kFront, -kHalfW + c, zTop}, {kFront, kHalfW - c, zTop}, {0, 0, 1}, {-1, 0, 0, 1}, dusty1, dusty1, dusty1, dusty1);
        quad(b, {kBack, -kHalfW, zTop}, {kBack, kHalfW, zTop}, {kBack, kHalfW, zBase}, {kBack, -kHalfW, zBase}, {-1, 0, 0}, {0, 1, 0, 1}, TS(kBack, 0), TS(kBack, 0), TS(kBack, Zb), TS(kBack, Zb));
        quad(b, {kBack, kHalfW, zBase}, {kFront, kHalfW, zBase}, {kFront, -kHalfW, zBase}, {kBack, -kHalfW, zBase}, {0, 0, -1}, {-1, 0, 0, 1}, TS(kBack, Zb), TS(kFront, Zb), TS(kFront, Zb), TS(kBack, Zb));
    }
    // Bedienfeld (steht vor) und Leuchtschild (Kasten; die Frontscheibe leuchtet als eigenes Teil)
    frontQuad(b, kFront + kPanelOut, kPanel.x0, kPanel.z0, kPanel.x1, kPanel.z1);
    frontBoxSides(b, kFront, kFront + kPanelOut, kPanel.x0, kPanel.z0, kPanel.x1, kPanel.z1);
    frontBoxSides(b, kFront, kFront + kHeaderOut, kHeader.x0, kHeader.z0, kHeader.x1, kHeader.z1);
    frontQuad(out.glow, kFront + kHeaderOut, kHeader.x0, kHeader.z0, kHeader.x1, kHeader.z1);
    frontQuad(out.glow, kFront + kPanelOut + 0.0015f, kDisplay.x0, kDisplay.z0, kDisplay.x1, kDisplay.z1);
    // Glasfront: UV 0..1 ueber das Fenster (Interior Mapping im Shader)
    {
        const float xg = kFront - kGlassIn;
        float y0 = kHalfW - w.x0, y1 = kHalfW - w.x1, z0 = kHeight - w.z0, z1 = kHeight - w.z1;
        quad(out.glass, {xg, y0, z0}, {xg, y1, z0}, {xg, y1, z1}, {xg, y0, z1}, {1, 0, 0}, {0, -1, 0, 1}, {0, 0}, {1, 0}, {1, 1}, {0, 1});
    }
    return out;
}

int MeshLibrary::find(const std::string& name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? -1 : it->second;
}

int MeshLibrary::addGroup(Gpu& gpu, const std::string& name, const std::vector<std::pair<MeshData, u32>>& parts) {
    int first = -1, prev = -1;
    for (size_t i = 0; i < parts.size(); ++i) {
        int id = add(gpu, name + "#" + std::to_string(i), parts[i].first, parts[i].second);
        if (prev >= 0) meshes_[(size_t)prev].next = id;
        if (first < 0) first = id;
        prev = id;
    }
    byName_[name] = first;
    return first;
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

// Einheitszylinder entlang +x (0..1), Radius 1, mit Deckeln. Der Koerper skaliert ihn je
// Glied auf Laenge und Strichstaerke (Querschnitt bleibt rund -> Normalen bleiben korrekt).
MeshData makeCylinder(int seg) {
    MeshData d;
    for (int ring = 0; ring <= 1; ++ring)
        for (int j = 0; j <= seg; ++j) {
            float a = (float)j / seg * 6.2831853f;
            float c = std::cos(a), s = std::sin(a);
            d.vertices.push_back({{(float)ring, c, s}, {0, c, s}, {1, 0, 0, 1}, {(float)ring * 0.25f, (float)j / seg * 0.05f}});
        }
    for (int j = 0; j < seg; ++j) {
        u16 a = (u16)j, b = (u16)(j + seg + 1);
        d.indices.insert(d.indices.end(), {a, (u16)(a + 1), b, (u16)(a + 1), (u16)(b + 1), b});
    }
    for (int end = 0; end <= 1; ++end) {
        u16 center = (u16)d.vertices.size();
        float nx = end ? 1.0f : -1.0f;
        d.vertices.push_back({{(float)end, 0, 0}, {nx, 0, 0}, {0, 1, 0, 1}, {0, 0}});
        u16 first = (u16)d.vertices.size();
        for (int j = 0; j <= seg; ++j) {
            float a = (float)j / seg * 6.2831853f;
            d.vertices.push_back({{(float)end, std::cos(a), std::sin(a)}, {nx, 0, 0}, {0, 1, 0, 1}, {0, 0}});
        }
        for (int j = 0; j < seg; ++j) {
            if (end) d.indices.insert(d.indices.end(), {center, (u16)(first + j), (u16)(first + j + 1)});
            else d.indices.insert(d.indices.end(), {center, (u16)(first + j + 1), (u16)(first + j)});
        }
    }
    return d;
}

// Einheitskugel (Radius 1) fuer Gelenke, Haende, Kopf und Augen
MeshData makeSphere(int seg, int rings) {
    MeshData d;
    for (int i = 0; i <= rings; ++i) {
        float v = (float)i / rings;
        float th = v * 3.14159265f;
        float st = std::sin(th), ct = std::cos(th);
        for (int j = 0; j <= seg; ++j) {
            float u = (float)j / seg;
            float ph = u * 6.2831853f;
            vec3 n{st * std::cos(ph), st * std::sin(ph), ct};
            d.vertices.push_back({n, n, {-std::sin(ph), std::cos(ph), 0, 1}, {u * 0.1f, v * 0.05f}});
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < seg; ++j) {
            u16 a = (u16)(i * (seg + 1) + j), b = (u16)(a + seg + 1);
            d.indices.insert(d.indices.end(), {a, b, (u16)(a + 1), (u16)(a + 1), b, (u16)(b + 1)});
        }
    return d;
}

// Grinsen: duenner, gebogener Strich in der x/z-Ebene (Mitte unten bei z = 0)
MeshData makeSmile() {
    MeshData d;
    const int segA = 18, segB = 8;
    const float R0 = 0.075f, r = 0.011f, span = 0.95f;
    for (int i = 0; i <= segA; ++i) {
        float a = -span + 2.0f * span * (float)i / segA;
        vec3 radial{std::sin(a), 0, -std::cos(a)};
        vec3 c = radial * R0 + vec3{0, 0, R0};
        vec3 tangent{std::cos(a), 0, std::sin(a)};
        for (int j = 0; j <= segB; ++j) {
            float b = (float)j / segB * 6.2831853f;
            vec3 n = radial * std::cos(b) + vec3{0, 1, 0} * std::sin(b);
            d.vertices.push_back({c + n * r, n, {tangent.x, tangent.y, tangent.z, 1}, {(float)i / segA * 0.05f, (float)j / segB * 0.02f}});
        }
    }
    for (int i = 0; i < segA; ++i)
        for (int j = 0; j < segB; ++j) {
            u16 a = (u16)(i * (segB + 1) + j), b = (u16)(a + segB + 1);
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
