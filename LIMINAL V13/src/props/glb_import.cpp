#include "props/glb_import.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>

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
            else if (k == "fm") d.fm = v;
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

void trsFromNode(const Json& n, float t[3], float r[4], float s[3]) {
    if (n.has("translation"))
        for (int i = 0; i < 3; ++i) t[i] = (float)n["translation"].items()[(size_t)i].asNumber(0.0);
    if (n.has("rotation"))
        for (int i = 0; i < 4; ++i) r[i] = (float)n["rotation"].items()[(size_t)i].asNumber(i == 3 ? 1.0 : 0.0);
    if (n.has("scale"))
        for (int i = 0; i < 3; ++i) s[i] = (float)n["scale"].items()[(size_t)i].asNumber(1.0);
}

Mat4 trsMatrix(const float* t, const float* q, const float* s) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    Mat4 r{};
    r.m[0] = (1 - 2 * (y * y + z * z)) * s[0], r.m[1] = (2 * (x * y + z * w)) * s[0], r.m[2] = (2 * (x * z - y * w)) * s[0];
    r.m[4] = (2 * (x * y - z * w)) * s[1], r.m[5] = (1 - 2 * (x * x + z * z)) * s[1], r.m[6] = (2 * (y * z + x * w)) * s[1];
    r.m[8] = (2 * (x * z + y * w)) * s[2], r.m[9] = (2 * (y * z - x * w)) * s[2], r.m[10] = (1 - 2 * (x * x + y * y)) * s[2];
    r.m[12] = t[0], r.m[13] = t[1], r.m[14] = t[2], r.m[15] = 1.0f;
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
    trsFromNode(n, t, q, s);
    return trsMatrix(t, q, s);
}

struct Reader {
    const Json& j;
    const std::string& bin;
    // Zeiger und Schrittweite eines Accessors; false bei ungueltigen Angaben
    bool view(int acc, int wantComps, std::vector<float>& out, std::string& err) const {
        const Json& a = j["accessors"].items()[(size_t)acc];
        const int count = (int)a["count"].asNumber(0), ct = (int)a["componentType"].asNumber(0);
        const std::string type = a["type"].asString("");
        const int comps = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : type == "MAT4" ? 16 : 0;
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
        // V13: normierte Ganzzahlen (Gewichte) auf 0..1
        if (a["normalized"].asBool(false) && ct != 5126)
            for (float& v : out) v /= ct == 5121 ? 255.0f : (ct == 5123 ? 65535.0f : 4294967295.0f);
        return true;
    }
};

// V13: Teil im Bau (Netz plus Skinning-Daten)
struct Acc {
    Mesh mesh;
    std::vector<std::array<u8, 4>> joints;
    std::vector<std::array<float, 4>> weights;
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
    std::map<std::pair<int, bool>, Acc> byMat;  // (Material, mit Skin)
    const auto& nodes = (*j)["nodes"].items();
    std::vector<Mat4> global(nodes.size(), identity());
    std::vector<int> parentOf(nodes.size(), -1);
    int skinIndex = -1;
    // Knoten rekursiv: Netze mit Weltmatrix einsammeln. V13: Netze mit Skin bleiben im eigenen Raum (glTF: die
    // Lage des Netzknotens zaehlt dann nicht, die Gelenke bewegen die Punkte)
    std::function<bool(int, const Mat4&)> walk = [&](int ni, const Mat4& parent) -> bool {
        if (ni < 0 || ni >= (int)nodes.size()) return err = "Knotenindex", false;
        const Json& n = nodes[(size_t)ni];
        const Mat4 m = mul(parent, nodeMatrix(n));
        global[(size_t)ni] = m;
        if (n.has("mesh")) {
            const bool skinned = n.has("skin");
            if (skinned) {
                const int si = (int)n["skin"].asNumber(-1);
                if (skinIndex >= 0 && si != skinIndex) return err = "mehrere Skins", false;
                skinIndex = si;
            }
            const Mat4 mm = skinned ? identity() : m;
            const Json& mesh = (*j)["meshes"].items()[(size_t)n["mesh"].asNumber(0)];
            for (const Json& p : mesh["primitives"].items()) {
                if ((int)p["mode"].asNumber(4) != 4) continue;  // nur Dreiecke
                const Json& at = p["attributes"];
                std::vector<float> pos, nor, uv, idx, jo, we;
                if (!rd.view((int)at["POSITION"].asNumber(-1), 3, pos, err)) return false;
                if (at.has("NORMAL") && !rd.view((int)at["NORMAL"].asNumber(-1), 3, nor, err)) return false;
                if (at.has("TEXCOORD_0") && !rd.view((int)at["TEXCOORD_0"].asNumber(-1), 2, uv, err)) return false;
                if (p.has("indices") && !rd.view((int)p["indices"].asNumber(-1), 1, idx, err)) return false;
                if (skinned) {
                    if (!at.has("JOINTS_0") || !at.has("WEIGHTS_0")) return err = "Skin ohne JOINTS_0/WEIGHTS_0", false;
                    if (!rd.view((int)at["JOINTS_0"].asNumber(-1), 4, jo, err)) return false;
                    if (!rd.view((int)at["WEIGHTS_0"].asNumber(-1), 4, we, err)) return false;
                }
                const int mi = std::clamp((int)p["material"].asNumber(0), 0, (int)mats.size() - 1);
                Acc& acc = byMat[{mi, skinned}];
                Mesh& dst = acc.mesh;
                const u32 base = (u32)dst.v.size();
                const size_t nv = pos.size() / 3;
                if (skinned && (jo.size() != nv * 4 || we.size() != nv * 4)) return err = "Skin-Daten passen nicht", false;
                for (size_t i = 0; i < nv; ++i) {
                    const float x = pos[i * 3], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
                    const float wx = mm.m[0] * x + mm.m[4] * y + mm.m[8] * z + mm.m[12];
                    const float wy = mm.m[1] * x + mm.m[5] * y + mm.m[9] * z + mm.m[13];
                    const float wz = mm.m[2] * x + mm.m[6] * y + mm.m[10] * z + mm.m[14];
                    Vtx v;
                    // glTF ist Y-oben und rechtshaendig; das Spiel ist Z-oben und linkshaendig (x vorn, y rechts). Aus
                    // Blender (x, y, z) wird glTF (x, z, -y), hier (x, -y, z): was in Blender links liegt (+y), liegt im
                    // Spiel links (-y). Gespiegelt -> der Umlaufsinn der Dreiecke wird unten umgedreht
                    v.p = vec3{wx, wz, wy};
                    vec3 nn{0, 0, 1};
                    if (!nor.empty()) {
                        const float a = nor[i * 3], b = nor[i * 3 + 1], c = nor[i * 3 + 2];
                        // Normalen mit der linearen Teilmatrix (gleichmaessige Skalierung angenommen)
                        const float nx = mm.m[0] * a + mm.m[4] * b + mm.m[8] * c, ny = mm.m[1] * a + mm.m[5] * b + mm.m[9] * c,
                                    nz = mm.m[2] * a + mm.m[6] * b + mm.m[10] * c;
                        nn = normalize(vec3{nx, nz, ny});
                    }
                    v.n = nn;
                    v.uv = uv.empty() ? vec2{0, 0} : vec2{uv[i * 2], uv[i * 2 + 1]};
                    dst.v.push_back(v);
                    if (skinned) {
                        std::array<u8, 4> jj{};
                        std::array<float, 4> ww{};
                        float sum = 0.0f;
                        for (int k = 0; k < 4; ++k) {
                            jj[(size_t)k] = (u8)std::clamp((int)jo[i * 4 + (size_t)k], 0, 255);
                            ww[(size_t)k] = std::max(0.0f, we[i * 4 + (size_t)k]);
                            sum += ww[(size_t)k];
                        }
                        if (sum <= 0.0f) ww = {1, 0, 0, 0}, sum = 1.0f;
                        for (float& w : ww) w /= sum;
                        acc.joints.push_back(jj);
                        acc.weights.push_back(ww);
                    }
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
            for (const Json& c : n["children"].items()) {
                const int ci = (int)c.asNumber(-1);
                if (ci >= 0 && ci < (int)nodes.size()) parentOf[(size_t)ci] = ni;
                if (!walk(ci, m)) return false;
            }
        return true;
    };
    const int scene = (int)(*j)["scene"].asNumber(0);
    const Json& sc = (*j)["scenes"].items()[(size_t)scene];
    for (const Json& r : sc["nodes"].items())
        if (!walk((int)r.asNumber(-1), identity())) return false;
    out.parts.clear();
    out.bounds = Aabb{};
    out.skin.reset();
    for (auto& [key, acc] : byMat) {
        if (acc.mesh.v.empty()) continue;
        out.bounds.add(acc.mesh.bounds().lo), out.bounds.add(acc.mesh.bounds().hi);
        Part part{std::move(acc.mesh), mats[(size_t)key.first], std::move(acc.joints), std::move(acc.weights)};
        out.parts.push_back(std::move(part));
    }
    if (out.parts.empty()) return err = "keine Dreiecke", false;
    // V13: Skelett und Posen
    if (skinIndex >= 0) {
        const Json& sk = (*j)["skins"].items()[(size_t)skinIndex];
        auto sp = std::make_shared<Skin>();
        std::map<int, int> jointOf;  // Knoten -> Gelenk
        const auto& jl = sk["joints"].items();
        for (size_t k = 0; k < jl.size(); ++k) jointOf[(int)jl[k].asNumber(-1)] = (int)k;
        std::vector<float> ibm;
        if (sk.has("inverseBindMatrices") && !rd.view((int)sk["inverseBindMatrices"].asNumber(-1), 16, ibm, err)) return false;
        if (!ibm.empty() && ibm.size() != jl.size() * 16) return err = "inverse Bindematrizen passen nicht", false;
        bool rootSet = false;
        for (size_t k = 0; k < jl.size(); ++k) {
            const int ni = (int)jl[k].asNumber(-1);
            if (ni < 0 || ni >= (int)nodes.size()) return err = "Gelenkknoten", false;
            const Json& n = nodes[(size_t)ni];
            if (n.has("matrix")) return err = "Gelenk mit Matrix (nur TRS)", false;
            SkinJoint sj;
            sj.name = n["name"].asString("");
            trsFromNode(n, sj.t, sj.r, sj.s);
            const int pn = parentOf[(size_t)ni];
            auto it = jointOf.find(pn);
            sj.parent = it != jointOf.end() ? it->second : -1;
            if (sj.parent < 0) {
                const Mat4 pre = pn >= 0 ? global[(size_t)pn] : identity();
                if (rootSet && std::memcmp(pre.m, sp->rootPre, sizeof(pre.m)) != 0)
                    return err = "Wurzelgelenke mit verschiedenen Eltern", false;
                std::memcpy(sp->rootPre, pre.m, sizeof(pre.m));
                rootSet = true;
            }
            if (!ibm.empty()) std::memcpy(sj.ibm, ibm.data() + k * 16, sizeof(sj.ibm));
            else sj.ibm[0] = sj.ibm[5] = sj.ibm[10] = sj.ibm[15] = 1.0f;
            sp->joints.push_back(std::move(sj));
        }
        for (size_t k = 0; k < sp->joints.size(); ++k)
            if (sp->joints[k].parent >= (int)k) {
                // Eltern muessen vor den Kindern stehen (Blender ordnet so); sonst nicht unterstuetzt
                return err = "Gelenkreihenfolge (Eltern nach Kind)", false;
            }
        // Posen: Animationen, der erste Schluessel zaehlt
        for (const Json& an : (*j)["animations"].items()) {
            SkinPose pose;
            pose.name = an["name"].asString("");
            for (const SkinJoint& sj : sp->joints)
                pose.trs.push_back({sj.t[0], sj.t[1], sj.t[2], sj.r[0], sj.r[1], sj.r[2], sj.r[3], sj.s[0], sj.s[1], sj.s[2]});
            const auto& samplers = an["samplers"].items();
            for (const Json& ch : an["channels"].items()) {
                const Json& tg = ch["target"];
                auto it = jointOf.find((int)tg["node"].asNumber(-1));
                if (it == jointOf.end()) continue;
                const std::string path = tg["path"].asString("");
                const int comps = path == "rotation" ? 4 : (path == "translation" || path == "scale") ? 3 : 0;
                if (!comps) continue;
                const size_t si = (size_t)ch["sampler"].asNumber(-1);
                if (si >= samplers.size()) return err = "Animation: Sampler", false;
                const Json& smp = samplers[si];
                std::vector<float> val;
                if (!rd.view((int)smp["output"].asNumber(-1), comps, val, err)) return false;
                const size_t off = smp["interpolation"].asString("LINEAR") == "CUBICSPLINE" ? (size_t)comps : 0;
                if (val.size() < off + (size_t)comps) return err = "Animation: Werte", false;
                auto& trs = pose.trs[(size_t)it->second];
                const int at0 = path == "translation" ? 0 : (path == "rotation" ? 3 : 7);
                for (int c = 0; c < comps; ++c) trs[(size_t)(at0 + c)] = val[off + (size_t)c];
            }
            sp->poses.push_back(std::move(pose));
        }
        for (const Part& part : out.parts)
            for (const auto& jj : part.joints)
                for (u8 x : jj)
                    if (x >= sp->joints.size()) return err = "Gelenkindex ausserhalb", false;
        out.skin = std::move(sp);
    }
    return true;
}

bool skinJointRest(const Skin& skin, const std::string& name, vec3& out) {
    int k = -1;
    for (size_t i = 0; i < skin.joints.size(); ++i)
        if (skin.joints[i].name == name) k = (int)i;
    if (k < 0) return false;
    Mat4 m = identity();
    for (int i = k; i >= 0; i = skin.joints[(size_t)i].parent) {
        const SkinJoint& sj = skin.joints[(size_t)i];
        m = mul(trsMatrix(sj.t, sj.r, sj.s), m);
    }
    Mat4 pre;
    std::memcpy(pre.m, skin.rootPre, sizeof(pre.m));
    m = mul(pre, m);
    out = vec3{m.m[12], m.m[14], m.m[13]};  // glTF -> Spiel (y und z tauschen)
    return true;
}

int Skin::pose(const std::string& name) const {
    for (size_t i = 0; i < poses.size(); ++i)
        if (poses[i].name == name) return (int)i;
    return -1;
}

void skinMatrices(const Skin& skin, const std::vector<float>& weights, std::vector<mat4>& out) {
    const size_t nj = skin.joints.size();
    std::vector<Mat4> glob(nj);
    out.resize(nj);
    float wsum = 0.0f;
    for (size_t p = 0; p < skin.poses.size() && p < weights.size(); ++p) wsum += std::max(0.0f, weights[p]);
    for (size_t k = 0; k < nj; ++k) {
        const SkinJoint& sj = skin.joints[k];
        float t[3] = {sj.t[0], sj.t[1], sj.t[2]}, r[4] = {sj.r[0], sj.r[1], sj.r[2], sj.r[3]}, sc[3] = {sj.s[0], sj.s[1], sj.s[2]};
        if (wsum > 0.0f) {
            float tt[3] = {}, rr[4] = {}, ss[3] = {};
            const float* r0 = nullptr;
            for (size_t p = 0; p < skin.poses.size() && p < weights.size(); ++p) {
                const float w = std::max(0.0f, weights[p]) / wsum;
                if (w <= 0.0f) continue;
                const auto& v = skin.poses[p].trs[k];
                for (int c = 0; c < 3; ++c) tt[c] += v[(size_t)c] * w, ss[c] += v[(size_t)(7 + c)] * w;
                // Rotationen gleichsinnig ausrichten (q und -q sind dieselbe Drehung), dann normiert mischen
                float sgn = 1.0f;
                if (!r0) r0 = &v[3];
                else if (r0[0] * v[3] + r0[1] * v[4] + r0[2] * v[5] + r0[3] * v[6] < 0.0f) sgn = -1.0f;
                for (int c = 0; c < 4; ++c) rr[c] += v[(size_t)(3 + c)] * w * sgn;
            }
            const float rl = std::sqrt(rr[0] * rr[0] + rr[1] * rr[1] + rr[2] * rr[2] + rr[3] * rr[3]);
            if (rl > 1e-6f) {
                for (int c = 0; c < 3; ++c) t[c] = tt[c], sc[c] = ss[c];
                for (int c = 0; c < 4; ++c) r[c] = rr[c] / rl;
            }
        }
        const Mat4 local = trsMatrix(t, r, sc);
        Mat4 pre;
        std::memcpy(pre.m, skin.rootPre, sizeof(pre.m));
        glob[k] = mul(sj.parent >= 0 ? glob[(size_t)sj.parent] : pre, local);
        Mat4 ibm;
        std::memcpy(ibm.m, sj.ibm, sizeof(ibm.m));
        const Mat4 m = mul(glob[k], ibm);
        // glTF -> Spiel: Achsen y und z tauschen (P * M * P)
        static const int perm[4] = {0, 2, 1, 3};
        mat4 g;
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                const float v = m.m[perm[c] * 4 + perm[rr]];
                (rr == 0 ? g.c[c].x : rr == 1 ? g.c[c].y : rr == 2 ? g.c[c].z : g.c[c].w) = v;
            }
        out[k] = g;
    }
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
