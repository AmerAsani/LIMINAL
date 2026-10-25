#include "props/glb_import.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <map>

#include "core/json.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim::props {

namespace {

u32 rd32(const std::string& s, size_t o) {
    u32 v;
    std::memcpy(&v, s.data() + o, 4);
    return v;
}

float linToSrgb(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

Look lookFromName(const std::string& s) {
    static const std::map<std::string, Look> m = {
        {"paint", Look::Paint},       {"carpaint", Look::CarPaint}, {"metal", Look::Metal},       {"galvanized", Look::Galvanized},
        {"plastic", Look::Plastic},   {"rubber", Look::Rubber},     {"glass", Look::Glass},       {"water", Look::Water},
        {"roadpaint", Look::RoadPaint}, {"sign", Look::Sign},       {"stencil", Look::Stencil},   {"wood", Look::Wood},
        {"fabric", Look::Fabric},     {"concrete", Look::Concrete}, {"cardboard", Look::Cardboard}, {"glow", Look::Glow},
        {"ceramic", Look::Ceramic}};
    auto it = m.find(s);
    return it == m.end() ? Look::Paint : it->second;
}

// Materialname "look=wood tex=wood_walnut_floor glow=0" auswerten
void parseMaterialName(const std::string& name, MatDesc& d) {
    std::string tok;
    auto flush = [&] {
        const auto eq = tok.find('=');
        if (eq != std::string::npos) {
            const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
            if (k == "look") d.look = lookFromName(v);
            else if (k == "tex") d.tex = v;
            else if (k == "glow") d.glow = (float)std::atof(v.c_str());
        }
        tok.clear();
    };
    for (char c : name) {
        if (c == ' ' || c == ';' || c == ',') flush();
        else tok += c;
    }
    flush();
    // Blender haengt bei doppelten Namen ".001" an
    if (const auto dot = d.tex.find('.'); dot != std::string::npos) d.tex.resize(dot);
}

struct Mat4 {
    float m[16];  // spaltenweise wie glTF
};

Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}

Mat4 nodeMatrix(const Json& n) {
    if (n.has("matrix")) {
        Mat4 r{};
        const auto& a = n["matrix"].items();
        for (int i = 0; i < 16 && i < (int)a.size(); ++i) r.m[i] = (float)a[(size_t)i].asNumber(0.0);
        return r;
    }
    float t[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};
    if (n.has("translation"))
        for (int i = 0; i < 3; ++i) t[i] = (float)n["translation"].items()[(size_t)i].asNumber(0.0);
    if (n.has("rotation"))
        for (int i = 0; i < 4; ++i) q[i] = (float)n["rotation"].items()[(size_t)i].asNumber(i == 3 ? 1.0 : 0.0);
    if (n.has("scale"))
        for (int i = 0; i < 3; ++i) s[i] = (float)n["scale"].items()[(size_t)i].asNumber(1.0);
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    Mat4 r{};
    r.m[0] = (1 - 2 * (y * y + z * z)) * s[0], r.m[1] = (2 * (x * y + z * w)) * s[0], r.m[2] = (2 * (x * z - y * w)) * s[0];
    r.m[4] = (2 * (x * y - z * w)) * s[1], r.m[5] = (1 - 2 * (x * x + z * z)) * s[1], r.m[6] = (2 * (y * z + x * w)) * s[1];
    r.m[8] = (2 * (x * z + y * w)) * s[2], r.m[9] = (2 * (y * z - x * w)) * s[2], r.m[10] = (1 - 2 * (x * x + y * y)) * s[2];
    r.m[12] = t[0], r.m[13] = t[1], r.m[14] = t[2], r.m[15] = 1.0f;
    return r;
}

struct Reader {
    const Json& j;
    const std::string& bin;
    // Zeiger und Schrittweite eines Accessors; false bei ungueltigen Angaben
    bool view(int acc, int wantComps, std::vector<float>& out, std::string& err) const {
        const Json& a = j["accessors"].items()[(size_t)acc];
        const int count = (int)a["count"].asNumber(0), ct = (int)a["componentType"].asNumber(0);
        const std::string type = a["type"].asString("");
        const int comps = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : 0;
        if (comps != wantComps) return err = "Accessor-Typ", false;
        const Json& bv = j["bufferViews"].items()[(size_t)a["bufferView"].asNumber(0)];
        const size_t off = (size_t)bv["byteOffset"].asNumber(0) + (size_t)a["byteOffset"].asNumber(0);
        const int csize = ct == 5126 ? 4 : ct == 5125 ? 4 : ct == 5123 ? 2 : ct == 5121 ? 1 : 0;
        if (!csize) return err = "Komponententyp", false;
        const size_t stride = bv.has("byteStride") ? (size_t)bv["byteStride"].asNumber(0) : (size_t)(csize * comps);
        if (count <= 0 || off + stride * (size_t)(count - 1) + (size_t)(csize * comps) > bin.size())
            return err = "Accessor ausserhalb der Daten", false;
        out.resize((size_t)count * comps);
        for (int i = 0; i < count; ++i)
            for (int c = 0; c < comps; ++c) {
                const char* p = bin.data() + off + stride * (size_t)i + (size_t)(csize * c);
                float v = 0.0f;
                if (ct == 5126) std::memcpy(&v, p, 4);
                else if (ct == 5125) {
                    u32 u;
                    std::memcpy(&u, p, 4);
                    v = (float)u;
                } else if (ct == 5123) {
                    u16 u;
                    std::memcpy(&u, p, 2);
                    v = (float)u;
                } else v = (float)(u8)*p;
                out[(size_t)i * comps + c] = v;
            }
        return true;
    }
};

}  // namespace

bool loadGlb(const std::string& path, Model& out, std::string& err) {
    auto file = paths::readFile(path);
    if (!file) return err = "nicht lesbar", false;
    const std::string& s = *file;
    if (s.size() < 20 || rd32(s, 0) != 0x46546C67u || rd32(s, 4) != 2) return err = "kein glTF 2.0 (.glb)", false;
    size_t o = 12;
    std::string json, bin;
    while (o + 8 <= s.size()) {
        const u32 len = rd32(s, o), type = rd32(s, o + 4);
        if (o + 8 + len > s.size()) return err = "Abschnitt zu lang", false;
        if (type == 0x4E4F534Au) json = s.substr(o + 8, len);
        else if (type == 0x004E4942u) bin = s.substr(o + 8, len);
        o += 8 + len;
    }
    auto j = Json::parse(json, &err);
    if (!j) return false;
    Reader rd{*j, bin};
    // Materialien
    std::vector<MatDesc> mats;
    for (const Json& m : (*j)["materials"].items()) {
        MatDesc d;
        parseMaterialName(m["name"].asString(""), d);
        const Json& pbr = m["pbrMetallicRoughness"];
        if (pbr.has("baseColorFactor")) {
            const auto& c = pbr["baseColorFactor"].items();
            d.r = linToSrgb((float)c[0].asNumber(1.0)), d.g = linToSrgb((float)c[1].asNumber(1.0)), d.b = linToSrgb((float)c[2].asNumber(1.0));
        } else {
            d.r = d.g = d.b = 1.0f;
        }
        mats.push_back(d);
    }
    if (mats.empty()) mats.push_back(MatDesc{});
    std::map<int, Mesh> byMat;
    // Knoten rekursiv: Netze mit Weltmatrix einsammeln
    const auto& nodes = (*j)["nodes"].items();
    std::function<bool(int, const Mat4&)> walk = [&](int ni, const Mat4& parent) -> bool {
        if (ni < 0 || ni >= (int)nodes.size()) return err = "Knotenindex", false;
        const Json& n = nodes[(size_t)ni];
        const Mat4 m = mul(parent, nodeMatrix(n));
        if (n.has("mesh")) {
            const Json& mesh = (*j)["meshes"].items()[(size_t)n["mesh"].asNumber(0)];
            for (const Json& p : mesh["primitives"].items()) {
                if ((int)p["mode"].asNumber(4) != 4) continue;  // nur Dreiecke
                const Json& at = p["attributes"];
                std::vector<float> pos, nor, uv, idx;
                if (!rd.view((int)at["POSITION"].asNumber(-1), 3, pos, err)) return false;
                if (at.has("NORMAL") && !rd.view((int)at["NORMAL"].asNumber(-1), 3, nor, err)) return false;
                if (at.has("TEXCOORD_0") && !rd.view((int)at["TEXCOORD_0"].asNumber(-1), 2, uv, err)) return false;
                if (p.has("indices") && !rd.view((int)p["indices"].asNumber(-1), 1, idx, err)) return false;
                const int mi = std::clamp((int)p["material"].asNumber(0), 0, (int)mats.size() - 1);
                Mesh& dst = byMat[mi];
                const u32 base = (u32)dst.v.size();
                const size_t nv = pos.size() / 3;
                for (size_t i = 0; i < nv; ++i) {
                    const float x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
                    const float wx = m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12];
                    const float wy = m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13];
                    const float wz = m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14];
                    Vtx v;
                    // glTF ist Y-oben und rechtshaendig; das Spiel ist Z-oben und linkshaendig (x vorn, y rechts). Aus
                    // Blender (x, y, z) wird glTF (x, z, -y), hier (x, -y, z): was in Blender links liegt (+y), liegt im
                    // Spiel links (-y). Gespiegelt -> der Umlaufsinn der Dreiecke wird unten umgedreht
                    v.p = vec3{wx, wz, wy};
                    vec3 nn{0, 0, 1};
                    if (!nor.empty()) {
                        const float a = nor[i * 3], b = nor[i * 3 + 1], c = nor[i * 3 + 2];
                        // Normalen mit der linearen Teilmatrix (gleichmaessige Skalierung angenommen)
                        const float nx = m.m[0] * a + m.m[4] * b + m.m[8] * c, ny = m.m[1] * a + m.m[5] * b + m.m[9] * c,
                                    nz = m.m[2] * a + m.m[6] * b + m.m[10] * c;
                        nn = normalize(vec3{nx, nz, ny});
                    }
                    v.n = nn;
                    v.uv = uv.empty() ? vec2{0, 0} : vec2{uv[i * 2], uv[i * 2 + 1]};
                    dst.v.push_back(v);
                }
                std::vector<u32> tri;
                if (idx.empty())
                    for (size_t i = 0; i < nv; ++i) tri.push_back((u32)i);
                else
                    for (float f : idx) {
                        const u32 k = (u32)f;
                        if (k >= nv) return err = "Index ausserhalb", false;
                        tri.push_back(k);
                    }
                for (size_t t = 0; t + 2 < tri.size(); t += 3)  // Spiegelung: Umlaufsinn umdrehen
                    dst.i.insert(dst.i.end(), {base + tri[t], base + tri[t + 2], base + tri[t + 1]});
            }
        }
        if (n.has("children"))
            for (const Json& c : n["children"].items())
                if (!walk((int)c.asNumber(-1), m)) return false;
        return true;
    };
    const int scene = (int)(*j)["scene"].asNumber(0);
    const Json& sc = (*j)["scenes"].items()[(size_t)scene];
    for (const Json& r : sc["nodes"].items())
        if (!walk((int)r.asNumber(-1), identity())) return false;
    out.parts.clear();
    out.bounds = Aabb{};
    for (auto& [mi, mesh] : byMat) {
        if (mesh.v.empty()) continue;
        out.bounds.add(mesh.bounds().lo), out.bounds.add(mesh.bounds().hi);
        out.parts.push_back(Part{std::move(mesh), mats[(size_t)mi]});
    }
    if (out.parts.empty()) return err = "keine Dreiecke", false;
    return true;
}

int loadModelDirectory(const std::string& dir) {
    int n = 0;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(paths::widen(dir), ec)) {
        if (!e.is_regular_file() || e.path().extension() != L".glb") continue;
        const std::string name = e.path().stem().string(), file = paths::join(dir, e.path().filename().string());
        Model m;
        std::string err;
        if (!loadGlb(file, m, err)) {
            log::warn("Modell {}: {}", name, err);
            continue;
        }
        size_t tris = 0;
        for (const auto& p : m.parts) tris += p.mesh.tris();
        const int parts = (int)m.parts.size();
        registerModel(name, std::move(m));
        log::info("Modell {}: {} Teile, {} Dreiecke", name, parts, tris);
        ++n;
    }
    return n;
}

}  // namespace lim::props
