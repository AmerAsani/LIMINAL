// V10: Modelle der Entities (siehe anim/creature.hpp, Quellen und Auslegung: docs/ENTITAETEN.md).
// Aus Distanzfeldern modelliert (render/sculpt) mit eigener Anatomie je Entity; die Fluegel der
// Deathmoth sind ein eigenes, duennes Netz mit gezacktem Rand (render/moth_wing.hpp), damit der
// Umriss scharf bleibt. Je Punkt kommen zwei Werte fuer den Shader dazu (uv): Verdeckung (Schmutz in
// Falten und Ritzen, aus dem Distanzfeld) und Kantenwoelbung (abgegriffene Kanten, aus dem Netz).
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <memory>

#include "anim/creature.hpp"
#include "anim/skeleton.hpp"
#include "core/jobs.hpp"
#include "render/figure_models.hpp"
#include "render/moth_wing.hpp"
#include "render/sculpt.hpp"
#include "render/world_renderer.hpp"

namespace lim::gfx {

using namespace anim;
using sculpt::Prim;
using sculpt::Shape;

namespace {

using Mask = std::uint64_t;
constexpr Mask kAll = ~Mask(1);  // alle ausser der Wurzel

struct ECtx {
    const CreatureSkeleton* sk = nullptr;
    std::vector<std::vector<int>> children;
    int lod = 0;
    JobSystem* jobs = nullptr;
    vec3 J(int j) const { return sk->bind[(size_t)j]; }
};

const vec3 kF{1, 0, 0}, kL{0, 1, 0}, kU{0, 0, 1};

float sat(float x) { return std::clamp(x, 0.0f, 1.0f); }
float smoothF(float a, float b, float x) {
    float t = sat((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
vec3 lerpV(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
vec3 nrm(vec3 v) {
    float l = length(v);
    return l > 1e-6f ? v / l : vec3{1, 0, 0};
}
Mask bits(std::initializer_list<int> l) {
    Mask k = 0;
    for (int b : l) k |= Mask(1) << b;
    return k;
}
// feste Zufallswerte 0..1 (je Entity und Bauteil reproduzierbar)
float h01(int a, int b) { return wing::hash01((std::uint32_t)a * 2654435761u + (std::uint32_t)b * 40503u + 17u); }

float segDist(vec3 p, vec3 a, vec3 b) {
    vec3 ab = b - a;
    float t = std::clamp(dot(p - a, ab) / std::max(dot(ab, ab), 1e-9f), 0.0f, 1.0f);
    return length(p - (a + ab * t));
}

// Punkt auf einer Polylinie (Anteil der Bogenlaenge 0..1)
vec3 along(const std::vector<vec3>& p, float t) {
    float total = 0.0f;
    for (size_t i = 1; i < p.size(); ++i) total += length(p[i] - p[i - 1]);
    float want = std::clamp(t, 0.0f, 1.0f) * total;
    for (size_t i = 1; i < p.size(); ++i) {
        const float l = length(p[i] - p[i - 1]);
        if (want <= l || i + 1 == p.size()) return lerpV(p[i - 1], p[i], l > 1e-6f ? std::min(1.0f, want / l) : 0.0f);
        want -= l;
    }
    return p.back();
}

// Gewichte: naechster Knochen, sein Elternteil und seine Kinder (keine Uebergriffe zwischen
// Gliedmassen, die nur nahe beieinander liegen - Arm und Rumpf, Finger und Finger)
void weighE(const ECtx& c, vec3 p, Mask allowed, float sigma, SkinVertex& v) {
    const auto& bones = c.sk->bones;
    const int nb = std::min(64, (int)bones.size());
    float nd[64];
    int best = -1;
    float bestD = 1e9f;
    for (int b = 1; b < nb; ++b) {
        nd[b] = 1e9f;
        if (!((allowed >> b) & 1u)) continue;
        const CBone& B = bones[(size_t)b];
        nd[b] = segDist(p, c.J(B.a), c.J(B.b)) / std::max(0.004f, B.radius);
        if (nd[b] < bestD) bestD = nd[b], best = b;
    }
    float w[64] = {};
    if (best < 0) {
        w[1] = 1.0f;
    } else {
        auto add = [&](int b) {
            if (b <= 0 || b >= nb || !((allowed >> b) & 1u) || w[b] > 0.0f) return;
            if (nd[b] - bestD < 5.0f * sigma) w[b] = std::exp(-(nd[b] - bestD) / sigma);
        };
        add(best);
        add(bones[(size_t)best].parent);
        for (int ch : c.children[(size_t)best]) add(ch);
    }
    int idx[4] = {0, 0, 0, 0};
    float val[4] = {0, 0, 0, 0};
    for (int b = 0; b < nb; ++b) {
        float x = w[b];
        for (int k = 0; k < 4; ++k)
            if (x > val[k]) {
                for (int m = 3; m > k; --m) val[m] = val[m - 1], idx[m] = idx[m - 1];
                val[k] = x, idx[k] = b;
                break;
            }
    }
    float tot = std::max(1e-6f, val[0] + val[1] + val[2] + val[3]);
    int q[4], qs = 0;
    for (int k = 0; k < 4; ++k) q[k] = (int)std::lround(val[k] / tot * 255.0f), qs += q[k];
    q[0] += 255 - qs;
    for (int k = 0; k < 4; ++k) v.bones[k] = (u8)idx[k], v.weights[k] = (u8)std::clamp(q[k], 0, 255);
}

// Verdeckung (aus dem Feld) und Woelbung (aus dem Netz) je Punkt
void shadingTerms(const ECtx& c, const Shape& s, const sculpt::RawMesh& m, float aoScale, std::vector<vec2>& out) {
    const size_t n = m.pos.size();
    out.assign(n, vec2{0, 0});
    std::vector<float> cav(n, 0.0f);
    auto occ = [&](int i) {
        const vec3 p = m.pos[(size_t)i], nn = m.normal[(size_t)i];
        float o = 0.0f, ws = 0.0f;
        for (int k = 1; k <= 5; ++k) {
            const float d = 0.007f * (float)k * aoScale, w = 1.0f / (float)k;
            o += w * sat((d - s.eval(p + nn * d)) / d);
            ws += w;
        }
        cav[(size_t)i] = sat((o / ws - 0.06f) * 1.7f);
    };
    if (c.jobs) c.jobs->parallelFor((int)n, occ);
    else
        for (int i = 0; i < (int)n; ++i) occ(i);
    // Woelbung: Abstand zum Mittel der Nachbarn entlang der Normalen (Kugel mit Radius r: 1/r)
    std::vector<vec3> sum(n, vec3{});
    std::vector<float> len(n, 0.0f), kap(n, 0.0f), tmp(n, 0.0f);
    std::vector<int> cnt(n, 0);
    for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
        const unsigned v[3] = {m.idx[t], m.idx[t + 1], m.idx[t + 2]};
        for (int e = 0; e < 3; ++e) {
            const unsigned a = v[e], b = v[(e + 1) % 3];
            const float l = length(m.pos[a] - m.pos[b]);
            sum[a] += m.pos[b], sum[b] += m.pos[a];
            len[a] += l, len[b] += l;
            ++cnt[a], ++cnt[b];
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (cnt[i] == 0) continue;
        const vec3 avg = sum[i] / (float)cnt[i];
        const float l = len[i] / (float)cnt[i];
        kap[i] = dot(m.pos[i] - avg, m.normal[i]) * 2.0f / std::max(l * l, 1e-8f);
    }
    for (int it = 0; it < 2; ++it) {  // glaetten
        std::fill(tmp.begin(), tmp.end(), 0.0f);
        std::fill(cnt.begin(), cnt.end(), 0);
        for (size_t t = 0; t + 2 < m.idx.size(); t += 3)
            for (int e = 0; e < 3; ++e) {
                const unsigned a = m.idx[t + (size_t)e], b = m.idx[t + (size_t)((e + 1) % 3)];
                tmp[a] += kap[b], tmp[b] += kap[a];
                ++cnt[a], ++cnt[b];
            }
        for (size_t i = 0; i < n; ++i)
            if (cnt[i]) kap[i] = 0.5f * kap[i] + 0.5f * tmp[i] / (float)cnt[i];
    }
    for (size_t i = 0; i < n; ++i) {
        const float edge = smoothF(18.0f, 70.0f, kap[i]);
        const float hollow = smoothF(12.0f, 55.0f, -kap[i]);
        out[i] = vec2{sat(cav[i] + 0.45f * hollow), edge};
    }
}

SkinPart part(const ECtx& c, const Shape& s, float h, float keep, u32 material, Mask allowed = kAll, float sigma = 0.25f, float aoScale = 1.0f) {
    sculpt::RawMesh m = sculpt::mesh(s, s.bounds(), h, c.jobs);
    const size_t tris = m.idx.size() / 3;
    const size_t target = std::max<size_t>(64, (size_t)((float)tris * keep));
    if (target < tris) {
        sculpt::simplify(m, target);
        sculpt::reproject(m, s, h, c.jobs);
    }
    std::vector<vec2> sh;
    shadingTerms(c, s, m, aoScale, sh);
    SkinPart p;
    p.material = material;
    p.v.resize(m.pos.size());
    auto one = [&](int i) {
        SkinVertex& v = p.v[(size_t)i];
        v.pos = m.pos[(size_t)i];
        v.normal = m.normal[(size_t)i];
        v.tangent = vec4(v.normal, 1.0f);
        v.uv = sh[(size_t)i];
        v.bind = v.pos;
        weighE(c, v.pos, allowed, sigma, v);
    };
    if (c.jobs) c.jobs->parallelFor((int)m.pos.size(), one);
    else
        for (int i = 0; i < (int)m.pos.size(); ++i) one(i);
    p.idx.assign(m.idx.begin(), m.idx.end());
    return p;
}

// Punkt auf der Oberflaeche: von 'from' (innen) entlang dir nach aussen
vec3 surfaceAt(const Shape& s, vec3 from, vec3 dir, float maxDist = 0.8f) {
    dir = nrm(dir);
    float t = 0.0f, prev = s.eval(from);
    if (prev > 0.0f) return from;
    for (; t < maxDist; t += 0.002f) {
        float d = s.eval(from + dir * (t + 0.002f));
        if (d > 0.0f) {
            float a = t, b = t + 0.002f;
            for (int i = 0; i < 8; ++i) {
                float mm = (a + b) * 0.5f;
                if (s.eval(from + dir * mm) > 0.0f) b = mm;
                else a = mm;
            }
            return from + dir * ((a + b) * 0.5f);
        }
    }
    return from + dir * maxDist;
}

// ein Zahn: an der Wurzel rund, zur Spitze schlank und leicht gebogen
void tooth(Shape& s, vec3 base, vec3 dir, vec3 bend, float len, float rad, float k = 0.0015f) {
    dir = nrm(dir);
    bend = nrm(bend - dir * dot(bend, dir));
    const vec3 mid = base + dir * (len * 0.5f) + bend * (len * 0.04f);
    const vec3 tip = base + dir * len + bend * (len * 0.15f);
    s.cone(base - dir * (rad * 0.8f), mid, rad, rad * 0.7f, k);
    s.cone(mid, tip, rad * 0.7f, std::max(0.0003f, rad * 0.08f), k * 0.6f);
}

// ================================================================================================
// Smiler (Entity 3): schwarze, formlose Masse; Augen und Zaehne leuchten
// ================================================================================================
// Linie des Laechelns (Ruhepose): t -1 .. 1 quer, an den Enden nach oben gezogen
vec3 grinAt(const Shape& body, float t, float dz) {
    const float y = t * 0.215f, z = 1.632f + 0.065f * t * t * (0.6f + 0.4f * std::fabs(t)) + dz;
    return surfaceAt(body, {0.0f, y, z}, kF, 0.5f);
}

Shape smilerBody(const ECtx& c, int lod) {
    using namespace sm;
    Shape s;
    auto J = [&](int j) { return c.J(j); };
    const vec3 F = kF, L = kL, U = kU;
    // obere Masse: breit wie eine Wolke - das lange Laecheln braucht Platz -, ungleiche Ballen ringsum
    s.ellipsoid({0.04f, 0.0f, 1.66f}, {0.17f, 0.27f, 0.2f});
    s.ellipsoid({-0.05f, 0.0f, 1.78f}, {0.16f, 0.22f, 0.15f}, 0.1f);
    static const float lump[6][6] = {{-0.02f, 0.22f, 1.5f, 0.14f, 0.13f, 0.17f},  {-0.03f, -0.2f, 1.44f, 0.13f, 0.12f, 0.19f},
                                     {-0.1f, 0.12f, 1.86f, 0.12f, 0.12f, 0.1f},   {-0.08f, -0.15f, 1.83f, 0.11f, 0.12f, 0.11f},
                                     {-0.12f, 0.03f, 1.55f, 0.15f, 0.17f, 0.2f},  {0.0f, 0.27f, 1.7f, 0.08f, 0.08f, 0.1f}};
    for (const auto& l : lump) s.ellipsoid({l[0], l[1], l[2]}, {l[3], l[4], l[5]}, 0.1f);
    // nach unten eine Rauchsaeule, die duenner wird und ueber dem Boden ausfranst - keine Fuesse
    s.cone(J(UPPER), J(CORE), 0.21f, 0.16f, 0.12f);
    s.cone(J(CORE), J(BASE), 0.16f, 0.07f, 0.1f);
    s.cone(J(BASE), J(BASE) - U * 0.17f, 0.07f, 0.012f, 0.06f);
    for (int k = 0; k < 3; ++k) {
        const float a = 0.6f + 2.1f * (float)k, ca = std::cos(a), sa = std::sin(a);
        s.cone({ca * 0.05f, sa * 0.05f, 0.42f}, {ca * 0.13f, sa * 0.13f, 0.2f + 0.04f * (float)k}, 0.045f, 0.008f, 0.05f);
    }
    // Zungen der Masse: weiche, ungleiche Rauchlappen (keine Spitzen) - sie wiegen sich (Knochen)
    for (int w = 0; w < kWisps - 1; ++w) {
        const float big = 0.75f + 0.5f * h01(w, 901);
        const float wr[4] = {0.075f * big, 0.06f * big, 0.045f * big, 0.03f * big};
        for (int i = 0; i < 3; ++i) s.cone(J(wisp(w, i)), J(wisp(w, i)) + (J(wisp(w, i + 1)) - J(wisp(w, i))) * 0.85f, wr[i], wr[i + 1], 0.06f);
    }
    // Mulde fuer das Laecheln, flache Augenhoehlen
    const vec3 m0 = surfaceAt(s, {0.0f, 0.0f, 1.632f}, F, 0.5f);
    s.ellipsoidAx(m0 + F * 0.014f, F, L, {0.032f, 0.235f, 0.042f}, 0.025f, Prim::Sub);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 e = surfaceAt(s, {0.0f, sg * 0.1f, 1.755f}, F, 0.5f);
        s.ellipsoidAx(e + F * 0.006f, F, L, {0.012f, 0.04f, 0.022f}, 0.012f, Prim::Sub);
    }
    if (lod < 2)
        s.displace = [](vec3 p) {
            // amorph wie Rauch: senkrecht gestreckte Wirbel, oben staerker; das Gesicht bleibt ruhig
            const float face = smoothF(0.05f, 0.15f, p.x) * (1.0f - smoothF(0.24f, 0.32f, std::fabs(p.y))) *
                               (1.0f - smoothF(0.13f, 0.22f, std::fabs(p.z - 1.69f)));
            const float amp = (0.012f + 0.014f * smoothF(1.5f, 2.1f, p.z) + 0.01f * (1.0f - smoothF(0.3f, 0.9f, p.z))) * (1.0f - 0.85f * face);
            return amp * sculpt::fbm3(vec3{p.x * 7.0f, p.y * 7.0f, p.z * 2.6f}, 3, 701) +
                   0.02f * (1.0f - face) * sculpt::fbm3(vec3{p.x * 3.0f, p.y * 3.0f, p.z * 1.4f}, 2, 707) +
                   0.0035f * (1.0f - 0.7f * face) * sculpt::noise3(p * 34.0f, 703);
        };
    return s;
}

// viele spitze Zaehne entlang des Laechelns; oben und unten versetzt, sie greifen ineinander
Shape smilerTeeth(const Shape& body, bool upper) {
    Shape s;
    const int n = upper ? 25 : 23;
    const float sg = upper ? 1.0f : -1.0f;
    for (int i = 0; i < n; ++i) {
        const float j = h01(i, upper ? 11 : 13) - 0.5f;
        const float t = -0.97f + 1.94f * ((float)i + 0.5f + 0.3f * j) / (float)n;
        const float big = 1.0f - 0.35f * std::fabs(t);
        const float len = (0.026f + 0.024f * h01(i, upper ? 17 : 19)) * big;
        const float rad = (0.0062f + 0.0026f * h01(i, upper ? 23 : 29)) * (1.0f - 0.25f * std::fabs(t));
        const vec3 base = grinAt(body, t, sg * (0.02f + 0.004f * h01(i, 31))) - kF * 0.005f;
        const vec3 dir = kU * -sg + kF * 0.22f - kL * (t * 0.12f) + kL * (j * 0.1f);
        tooth(s, base, dir, kF, len, rad, 0.002f);
    }
    s.displace = [](vec3 p) { return 0.00025f * sculpt::noise3(p * 900.0f, 711); };
    return s;
}

Shape smilerEyes(const Shape& body) {
    Shape s;
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 e = surfaceAt(body, {0.0f, sg * 0.1f, 1.755f}, kF, 0.5f);
        const vec3 ay = nrm(kL * sg + kU * 0.3f);  // der aeussere Winkel hoeher
        s.ellipsoidAx(e + kF * 0.002f, kF, ay, {0.012f, 0.036f, 0.015f});
    }
    return s;
}

void buildSmiler(ECtx& c, FigureModel& m) {
    using namespace sm;
    const float body[3] = {0.006f, 0.012f, 0.026f}, fine[3] = {0.0016f, 0.003f, 0.0055f};
    const Mask face = bits({sm::B_FACE}), jaw = bits({sm::B_JAW});
    for (int l = 0; l < kFigureLods; ++l) {
        c.lod = l;
        auto& parts = m.lods[(size_t)l];
        Shape b = smilerBody(c, l);
        parts.push_back(part(c, b, body[l], 0.3f, FM_ENT_SHADOW, kAll, 0.35f));
        parts.push_back(part(c, smilerTeeth(b, true), fine[l], 0.4f, FM_ENT_GLOW, face, 0.25f, 0.4f));
        parts.push_back(part(c, smilerTeeth(b, false), fine[l], 0.4f, FM_ENT_GLOW, jaw, 0.25f, 0.4f));
        parts.push_back(part(c, smilerEyes(b), fine[l], 0.5f, FM_ENT_GLOW, face));
    }
}

// ================================================================================================
// Hound (Entity 8): aus einem Menschen entstanden, auf allen vieren, abgemagert
// ================================================================================================
struct HoundAx {
    vec3 ha, hup, ja, jup;
};
HoundAx houndAx(const ECtx& c) {
    using namespace hd;
    HoundAx a;
    a.ha = nrm(c.J(SNOUT) - c.J(SKULL));
    a.hup = nrm(kU - a.ha * dot(kU, a.ha));
    a.ja = nrm(c.J(JAW) - c.J(JAWH));
    a.jup = nrm(kU - a.ja * dot(kU, a.ja));
    return a;
}

Shape houndBody(const ECtx& c, int lod) {
    using namespace hd;
    Shape s;
    auto J = [&](int j) { return c.J(j); };
    const vec3 F = kF, L = kL, U = kU;
    const bool mid = lod <= 1;
    // Brustkorb: flach und breit wie beim Menschen; vorn das Brustbein
    const vec3 rc{0.11f, 0.0f, 0.745f};
    s.ellipsoid(rc, {0.19f, 0.125f, 0.105f});
    s.ellipsoidAx(rc + vec3{0.06f, 0.0f, -0.07f}, F, L, {0.13f, 0.06f, 0.045f}, 0.05f);
    // Taille: eingefallen hinter den Rippen; Becken mit vorstehenden Kaemmen
    s.cone({-0.06f, 0.0f, 0.765f}, {-0.36f, 0.0f, 0.71f}, 0.075f, 0.085f, 0.06f);
    s.ellipsoid({-0.42f, 0.0f, 0.69f}, {0.11f, 0.105f, 0.085f}, 0.05f);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.ellipsoid({-0.37f, sg * 0.088f, 0.755f}, {0.05f, 0.026f, 0.03f}, 0.02f);
        s.ellipsoid({-0.51f, sg * 0.05f, 0.66f}, {0.03f, 0.025f, 0.03f}, 0.02f);
        // Schulterblaetter stehen kantig ab
        const vec3 nS = nrm(vec3{0.0f, sg * 0.5f, 0.87f});
        s.box({0.19f, sg * 0.088f, 0.862f}, F, cross(nS, F), {0.065f, 0.045f, 0.011f}, 0.009f, 0.025f);
        // Schultern, Schluesselbeine
        const vec3 sh = J(front(SHOULDER_L, sd));
        s.sphere(sh, 0.044f, 0.03f);
        if (mid) s.capsule({0.31f, sg * 0.04f, 0.735f}, sh - U * 0.01f, 0.012f, 0.015f);
    }
    // Hals: duenn, Luftroehre und Sehnen zeichnen sich ab
    s.cone(J(CHEST) + U * 0.025f, J(NECK1), 0.075f, 0.056f, 0.04f);
    s.cone(J(NECK1), J(NECK2), 0.056f, 0.05f, 0.02f);
    s.cone(J(NECK2), J(SKULL), 0.05f, 0.054f, 0.02f);
    if (mid) {
        s.capsule(J(NECK1) - U * 0.04f, J(SKULL) - U * 0.05f + F * 0.02f, 0.011f, 0.02f);
        for (int sd = 0; sd < 2; ++sd) {
            const float sg = sd == 0 ? 1.0f : -1.0f;
            s.capsule(J(CHEST) + L * (sg * 0.045f), J(SKULL) + L * (sg * 0.03f) - U * 0.03f, 0.0075f, 0.012f);
        }
    }
    // Kopf: menschlicher Hirnschaedel, lang gezogener Oberkiefer; tiefe Augenhoehlen
    const HoundAx ax = houndAx(c);
    const vec3 S = J(SKULL), ha = ax.ha, hup = ax.hup;
    // menschlicher Hirnschaedel, ausgezehrt: Schlaefen eingefallen, Jochbeine stehen vor
    s.ellipsoidAx(S + hup * 0.022f - ha * 0.015f, ha, L, {0.11f, 0.08f, 0.09f}, 0.03f);
    s.ellipsoidAx(S + ha * 0.082f + hup * 0.04f, ha, L, {0.03f, 0.07f, 0.022f}, 0.02f);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.ellipsoidAx(S + ha * 0.085f - hup * 0.018f + L * (sg * 0.06f), ha, L, {0.04f, 0.016f, 0.018f}, 0.015f);
        s.ellipsoidAx(S + ha * 0.02f + hup * 0.03f + L * (sg * 0.078f), ha, L, {0.04f, 0.012f, 0.03f}, 0.012f, Prim::Sub);  // Schlaefe
    }
    // Oberkiefer: breit und kurz - das Maul reicht von Wange zu Wange
    s.ellipsoidAx(S + ha * 0.1f - hup * 0.035f, ha, L, {0.075f, 0.068f, 0.042f}, 0.03f);
    s.cone(S + ha * 0.07f - hup * 0.025f, J(SNOUT), 0.05f, 0.036f, 0.03f);
    // Mundspalte: alles unterhalb der Lippenlinie gehoert zum Unterkiefer
    const vec3 mc = S + ha * 0.1f - hup * 0.075f;
    s.ellipsoidAx(mc - hup * 0.03f, ha, L, {0.15f, 0.085f, 0.03f}, 0.006f, Prim::Sub);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.ellipsoidAx(S + ha * 0.1f + hup * 0.012f + L * (sg * 0.036f), ha, L, {0.024f, 0.021f, 0.018f}, 0.01f, Prim::Sub);
        s.ellipsoidAx(J(SNOUT) + hup * 0.016f + L * (sg * 0.009f) - ha * 0.004f, ha, L, {0.012f, 0.006f, 0.008f}, 0.004f, Prim::Sub);
    }
    // Vorderbeine (Arme): knochig, Ellbogenspitze nach hinten, zwei Unterarmknochen, flache Hand,
    // fuenf lange Finger mit Schwimmhaeuten
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 S0 = J(front(SHOULDER_L, sd)), E = J(front(ELBOW_L, sd)), W = J(front(WRIST_L, sd)), K = J(front(KNUCK_L, sd));
        s.cone(S0, E, 0.034f, 0.026f, 0.025f);
        if (mid) s.ellipsoidAx(lerpV(S0, E, 0.4f) - F * 0.015f, nrm(E - S0), L, {0.11f, 0.02f, 0.026f}, 0.02f);
        s.sphere(E, 0.028f, 0.01f);
        s.cone(E, E - F * 0.035f + U * 0.012f, 0.022f, 0.011f, 0.01f);
        const vec3 side = nrm(cross(nrm(W - E), U));
        s.cone(E + side * 0.01f, W + side * 0.012f, 0.016f, 0.012f, 0.012f);
        s.cone(E - side * 0.01f, W - side * 0.01f, 0.015f, 0.011f, 0.012f);
        s.sphere(W, 0.022f, 0.012f);
        const vec3 dp = nrm(K - W);
        s.ellipsoidAx(W * 0.4f + K * 0.6f, dp, L, {0.062f, 0.04f, 0.012f}, 0.015f);
        vec3 prevBase{}, prevMid{};
        for (int d = 0; d < 5; ++d) {
            const vec3 tip = J(front(D0_L, sd) + d);
            const vec3 base = d == 0 ? lerpV(K, W, 0.4f) : K + (tip - K) * 0.05f;
            const vec3 mm = lerpV(base, tip, 0.5f) + U * 0.008f;
            s.cone(base, mm, 0.0105f, 0.0085f, 0.006f);
            s.cone(mm, tip, 0.0085f, 0.006f, 0.005f);
            if (lod == 0) s.sphere(mm, 0.0095f, 0.003f);
            if (d > 1 && mid) {  // Schwimmhaut zwischen den Fingern (nicht am Daumen)
                const vec3 ctr = (prevBase + base) * 0.25f + (prevMid + mm) * 0.25f - U * 0.003f;
                s.ellipsoidAx(ctr, nrm((prevMid + mm) * 0.5f - (prevBase + base) * 0.5f), L, {0.03f, 0.5f * length(mm - prevMid), 0.003f}, 0.006f);
            }
            prevBase = base, prevMid = mm;
        }
    }
    // Hinterbeine: Knie vorn, Ferse hoch erhoben, Ballen und drei Zehen
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 H = J(hind(HIP_L, sd)), Kn = J(hind(KNEE_L, sd)), Hk = J(hind(HOCK_L, sd)), Ft = J(hind(FOOT_L, sd));
        s.cone(H, Kn, 0.052f, 0.032f, 0.03f);
        if (mid) s.ellipsoidAx(lerpV(H, Kn, 0.4f) - F * 0.02f + U * 0.01f, nrm(Kn - H), L, {0.13f, 0.026f, 0.032f}, 0.02f);
        s.sphere(Kn, 0.033f, 0.012f);
        s.cone(Kn, Hk, 0.028f, 0.02f, 0.015f);
        if (mid) s.capsule(lerpV(Kn, Hk, 0.1f) + F * 0.012f, lerpV(Kn, Hk, 0.8f) + F * 0.008f, 0.0075f, 0.01f);
        s.sphere(Hk, 0.024f, 0.01f);
        s.cone(Hk, Hk - F * 0.035f + U * 0.006f, 0.019f, 0.011f, 0.01f);
        s.cone(Hk, Ft, 0.022f, 0.02f, 0.012f);
        s.ellipsoidAx(Ft - U * 0.008f + F * 0.01f, F, L, {0.04f, 0.033f, 0.017f}, 0.015f);
        for (int t = 0; t < 3; ++t) {
            const vec3 tip = J(hind(T0_L, sd) + t);
            s.cone(Ft + F * 0.01f, tip, 0.0125f, 0.0085f, 0.008f);
            if (lod == 0) s.sphere(tip - U * 0.004f, 0.008f, 0.004f);
        }
    }
    // Wirbel unter der duennen Haut
    if (mid) {
        const std::vector<vec3> spine = {J(NECK1), J(CHEST), J(SPINE2), J(SPINE1), J(PELVIS), J(PELVIS) - F * 0.1f};
        for (int k = 0; k < 24; ++k) {
            const vec3 sp = along(spine, (float)k / 23.0f) - U * 0.05f;
            const vec3 top = surfaceAt(s, sp, U, 0.3f);
            s.sphere(top - U * 0.007f, 0.0115f, 0.008f);
        }
    }
    if (lod < 2) {
        // Rippen auf den Flanken, raeudige, unebene Haut
        s.displace = [rc](vec3 p) {
            float d = 0.0008f * sculpt::noise3(p * 110.0f, 401) + 0.0004f * sculpt::noise3(p * 320.0f, 403);
            const vec3 q = p - rc;
            if (q.x > -0.17f && q.x < 0.2f && std::fabs(q.y) > 0.045f && q.z > -0.1f) {
                const float band = std::sin((q.x + 0.25f * q.z) * 6.2831853f / 0.036f);
                const float fade = smoothF(0.2f, 0.1f, std::fabs(q.x - 0.01f)) * smoothF(-0.1f, -0.03f, q.z);
                d -= 0.0042f * fade * (0.5f + 0.5f * band);
            }
            return d;
        };
    }
    return s;
}

// Unterkiefer: lang und schmal - das Maul reicht bis unter die Augen
Shape houndJaw(const ECtx& c, int lod) {
    using namespace hd;
    Shape s;
    const HoundAx ax = houndAx(c);
    const vec3 A = c.J(JAWH), B = c.J(JAW), ja = ax.ja, jup = ax.jup, L = kL;
    s.ellipsoidAx(lerpV(A, B, 0.5f) - jup * 0.004f, ja, L, {0.095f, 0.06f, 0.019f});
    s.ellipsoidAx(B - ja * 0.02f - jup * 0.012f, ja, L, {0.03f, 0.03f, 0.018f}, 0.015f);  // Kinn
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.cone(A + L * (sg * 0.055f) + jup * 0.01f, lerpV(A, B, 0.55f) + L * (sg * 0.048f) - jup * 0.005f, 0.016f, 0.014f, 0.015f);
    }
    s.ellipsoidAx(lerpV(A, B, 0.55f) + jup * 0.02f, ja, L, {0.075f, 0.028f, 0.012f}, 0.006f, Prim::Sub);  // Mundboden
    if (lod < 2) s.displace = [](vec3 p) { return 0.0006f * sculpt::noise3(p * 120.0f, 411); };
    return s;
}

// Zahnbogen: u-foermig, t -1 .. 1 von links hinten ueber vorn nach rechts hinten
vec3 archPoint(vec3 center, vec3 fwd, float t, float ax, float ay) {
    const float phi = t * 1.45f;
    return center + fwd * (std::cos(phi) * ax) + kL * (std::sin(phi) * ay);
}

Shape houndTeeth(const ECtx& c, const Shape& head, const Shape& jaw, bool upper, int lod) {
    using namespace hd;
    Shape s;
    const HoundAx ax = houndAx(c);
    // mehrere Zahnreihen (Fandom): aussen gross mit Fangzaehnen, innen kleiner
    const int rows = lod == 0 ? 2 : 1;
    for (int r = 0; r < rows; ++r) {
        const int n = r == 0 ? 16 : 12;
        const float sc = r == 0 ? 1.0f : 0.78f;
        for (int i = 0; i < n; ++i) {
            const float t = -1.0f + 2.0f * ((float)i + 0.5f) / (float)n + 0.03f * (h01(i + r * 40, upper ? 31 : 37) - 0.5f);
            const float fang = std::exp(-std::pow((std::fabs(t) - 0.32f) / 0.1f, 2.0f));
            const float len = (0.014f + 0.01f * h01(i + r * 40, upper ? 41 : 43) + 0.02f * fang) * sc;
            const float rad = (0.0042f + 0.0013f * fang) * sc;
            vec3 base, dir, bend;
            if (upper) {
                const vec3 ctr = c.J(SKULL) + ax.ha * 0.075f - ax.hup * 0.06f;
                const vec3 p = archPoint(ctr, ax.ha, t, 0.085f * sc, 0.055f * sc);
                base = surfaceAt(head, p + ax.hup * 0.05f, -ax.hup, 0.12f) + ax.hup * 0.003f;
                dir = -ax.hup + ax.ha * 0.1f;
                bend = -ax.ha;
            } else {
                const vec3 ctr = c.J(JAWH) + ax.ja * 0.06f + ax.jup * 0.015f;
                const vec3 p = archPoint(ctr, ax.ja, t, 0.08f * sc, 0.05f * sc);
                base = surfaceAt(jaw, p - ax.jup * 0.03f, ax.jup, 0.1f) - ax.jup * 0.003f;
                dir = ax.jup + ax.ja * 0.12f;
                bend = -ax.ja;
            }
            tooth(s, base, dir, bend, len, rad, 0.001f);
        }
    }
    s.displace = [](vec3 p) { return 0.0002f * sculpt::noise3(p * 900.0f, 421); };
    return s;
}

// Zahnfleisch, Gaumen, Zunge
Shape houndMouth(const ECtx& c, const Shape& head, const Shape& jaw, bool upper) {
    using namespace hd;
    Shape s;
    const HoundAx ax = houndAx(c);
    vec3 prev{};
    for (int i = 0; i <= 14; ++i) {
        const float t = -1.0f + 2.0f * (float)i / 14.0f;
        vec3 p;
        if (upper) {
            const vec3 ctr = c.J(SKULL) + ax.ha * 0.075f - ax.hup * 0.06f;
            p = surfaceAt(head, archPoint(ctr, ax.ha, t, 0.08f, 0.051f) + ax.hup * 0.05f, -ax.hup, 0.12f) + ax.hup * 0.004f;
        } else {
            const vec3 ctr = c.J(JAWH) + ax.ja * 0.06f + ax.jup * 0.015f;
            p = surfaceAt(jaw, archPoint(ctr, ax.ja, t, 0.075f, 0.046f) - ax.jup * 0.03f, ax.jup, 0.1f) - ax.jup * 0.004f;
        }
        if (i > 0) s.capsule(prev, p, 0.0065f, 0.004f);
        prev = p;
    }
    if (upper) {
        s.ellipsoidAx(c.J(SKULL) + ax.ha * 0.1f - ax.hup * 0.056f, ax.ha, kL, {0.07f, 0.045f, 0.008f}, 0.006f);  // Gaumen
    } else {
        s.ellipsoidAx(c.J(JAWH) + ax.ja * 0.07f + ax.jup * 0.01f, ax.ja, kL, {0.06f, 0.034f, 0.011f}, 0.006f);  // Zunge
    }
    s.displace = [](vec3 p) { return 0.0004f * sculpt::noise3(p * 300.0f, 431); };
    return s;
}

Shape houndEyes(const ECtx& c) {
    using namespace hd;
    Shape s;
    const HoundAx ax = houndAx(c);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.sphere(c.J(SKULL) + ax.ha * 0.094f + ax.hup * 0.012f + kL * (sg * 0.036f), 0.0145f);
    }
    return s;
}

// Krallen an Fingern und Zehen
Shape houndClaws(const ECtx& c) {
    using namespace hd;
    Shape s;
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 K = c.J(front(KNUCK_L, sd));
        for (int d = 0; d < 5; ++d) {
            const vec3 tip = c.J(front(D0_L, sd) + d), dir = nrm(tip - K);
            s.cone(tip - dir * 0.006f, tip + dir * 0.022f - kU * 0.009f, 0.0064f, 0.0011f, 0.002f);
        }
        const vec3 Ft = c.J(hind(FOOT_L, sd));
        for (int t = 0; t < 3; ++t) {
            const vec3 tip = c.J(hind(T0_L, sd) + t), dir = nrm(tip - Ft);
            s.cone(tip - dir * 0.005f, tip + dir * 0.019f - kU * 0.008f, 0.0068f, 0.0012f, 0.002f);
        }
    }
    s.displace = [](vec3 p) { return 0.0003f * sculpt::noise3(p * 500.0f, 441); };
    return s;
}

// Haaransatz: eine verfilzte Kappe ueber dem Hirnschaedel
Shape houndScalp(const ECtx& c, int lod) {
    using namespace hd;
    Shape s;
    const HoundAx ax = houndAx(c);
    const vec3 S = c.J(SKULL), ha = ax.ha, hup = ax.hup;
    s.ellipsoidAx(S + hup * 0.026f - ha * 0.018f, ha, kL, {0.11f, 0.081f, 0.086f});
    s.clip = [S, ha, hup](vec3 p) {
        const vec3 q = p - S;
        return -(dot(q, hup) + 0.012f + 0.35f * std::max(0.0f, -dot(q, ha)) - 0.25f * std::max(0.0f, dot(q, ha) - 0.07f));
    };
    if (lod < 2) s.displace = [](vec3 p) { return 0.0035f * sculpt::fbm3(vec3{p.x * 90.0f, p.y * 90.0f, p.z * 30.0f}, 3, 451); };
    return s;
}

// Straehnen einer Haarkette (Ruhepose): verfilzte Buendel, an der Wurzel gestreut, zur Spitze hin
// auseinander und duenner. Am Ruecken zusaetzlich kurze Buendel, die flach auf der Wirbelsaeule liegen.
Shape houndHair(const ECtx& c, bool head, int h, int lod) {
    using namespace hd;
    Shape s;
    std::vector<vec3> pts;
    if (head)
        for (int i = 0; i < kHeadHairJ; ++i) pts.push_back(c.J(hairH(h, i)));
    else
        for (int i = 0; i < kBackHairJ; ++i) pts.push_back(c.J(hairB(h, i)));
    const HoundAx ax = houndAx(c);
    // dichte, verfilzte Buendel; auf groben Stufen weniger, dafuer dicker
    const int n = head ? (lod == 0 ? 28 : (lod == 1 ? 13 : 6)) : (lod == 0 ? 22 : (lod == 1 ? 10 : 5));
    const float thick = lod == 0 ? 1.0f : (lod == 1 ? 1.5f : 2.3f);
    const float sg = head ? (h == 2 ? -1.0f : 1.0f) : ((h % 2) == 0 ? 1.0f : -1.0f);
    // Flaeche, aus der die Wurzeln wachsen: zwei Richtungen auf der Haut
    vec3 A1, A3;
    float s1, s3;
    if (head && h == 0) A1 = kL, A3 = ax.ha, s1 = 0.11f, s3 = 0.07f;            // Scheitel vorn: faellt ueber das Gesicht
    else if (head) A1 = ax.ha, A3 = ax.hup, s1 = 0.12f, s3 = 0.05f;            // Seiten des Kopfes
    else A1 = kF, A3 = kL * sg, s1 = 0.17f, s3 = 0.045f;                        // Maehne entlang der Wirbelsaeule
    const vec3 T0 = nrm(pts[1] - pts[0]);
    const vec3 A2 = nrm(cross(T0, A1));
    const int segs = lod == 0 ? 9 : (lod == 1 ? 6 : 4);
    for (int i = 0; i < n; ++i) {
        const int sd = (head ? 0 : 100) + h * 40 + i;
        // Wurzeln gleichmaessig ueber die Flaeche gestreut (Gitter mit Zittern)
        const int cols = std::max(2, (int)std::lround(std::sqrt((float)n * 2.0f)));
        const float u = (((float)(i % cols) + 0.5f) / (float)cols - 0.5f) + 0.5f * (h01(sd, 1) - 0.5f) / (float)cols;
        const float v = (((float)(i / cols) + 0.5f) / std::ceil((float)n / (float)cols) - 0.5f) + 0.3f * (h01(sd, 2) - 0.5f);
        const float lenK = 0.62f + 0.38f * h01(sd, 3);
        const float ph1 = 6.28f * h01(sd, 4), ph2 = 6.28f * h01(sd, 5);
        const vec3 off = A1 * (u * s1) + A3 * (v * s3) - A2 * 0.004f;
        const float fan = 0.25f + 0.5f * h01(sd, 6);
        vec3 prev{};
        float rPrev = 0.0f;
        for (int k = 0; k <= segs; ++k) {
            const float t = (float)k / (float)segs * lenK;
            const vec3 p = along(pts, t) + off * (1.0f + fan * t) + A2 * (0.007f * std::sin(t * 7.0f + ph1) * t) + A1 * (0.006f * std::sin(t * 5.0f + ph2) * t);
            const float r = (0.0033f * thick) * (1.0f - 0.75f * t) + 0.0006f;
            if (k > 0) s.cone(prev, p, rPrev, r, 0.0012f);
            prev = p, rPrev = r;
        }
    }
    if (lod < 2) s.displace = [](vec3 p) { return 0.0004f * sculpt::noise3(p * 700.0f, 461); };
    return s;
}

void buildHound(ECtx& c, FigureModel& m) {
    using namespace hd;
    struct Q {
        float body, fine, hair, keep;
    };
    static const Q L[kFigureLods] = {{0.0032f, 0.0012f, 0.0017f, 0.3f}, {0.007f, 0.0028f, 0.0035f, 0.36f}, {0.016f, 0.007f, 0.008f, 0.5f}};
    const float hairKeep[kFigureLods] = {0.16f, 0.22f, 0.4f};
    Mask hair = 0;
    for (int b = hd::B_HAIRH0; b < BCOUNT; ++b) hair |= Mask(1) << b;
    const Mask jaw = bits({hd::B_JAW}), head = bits({hd::B_HEAD});
    const Mask bodyM = kAll & ~jaw & ~hair;
    for (int l = 0; l < kFigureLods; ++l) {
        c.lod = l;
        const Q& q = L[l];
        auto& parts = m.lods[(size_t)l];
        const Shape body = houndBody(c, l);
        const Shape jawS = houndJaw(c, l);
        parts.push_back(part(c, body, q.body, q.keep, FM_ENT_SKIN, bodyM));
        parts.push_back(part(c, jawS, l == 0 ? 0.0022f : q.body, q.keep, FM_ENT_SKIN, jaw));
        if (l < 2) {
            parts.push_back(part(c, houndMouth(c, body, jawS, true), q.fine * 1.4f, q.keep, FM_ENT_MOUTH, head));
            parts.push_back(part(c, houndMouth(c, body, jawS, false), q.fine * 1.4f, q.keep, FM_ENT_MOUTH, jaw));
            parts.push_back(part(c, houndTeeth(c, body, jawS, true, l), q.fine * 0.9f, q.keep, FM_ENT_TOOTH, head, 0.25f, 0.4f));
            parts.push_back(part(c, houndTeeth(c, body, jawS, false, l), q.fine * 0.9f, q.keep, FM_ENT_TOOTH, jaw, 0.25f, 0.4f));
            parts.push_back(part(c, houndClaws(c), q.fine, q.keep, FM_ENT_HORN, bodyM, 0.25f, 0.5f));
        }
        parts.push_back(part(c, houndEyes(c), q.fine, 0.5f, FM_ENT_EYESHINE, head));
        parts.push_back(part(c, houndScalp(c, l), q.hair * 1.6f, q.keep, FM_ENT_HAIR, head));
        for (int h = 0; h < kHeadHair; ++h)
            parts.push_back(part(c, houndHair(c, true, h, l), q.hair, hairKeep[l], FM_ENT_HAIR, bits({hd::B_HEAD, hairHB(h, 0), hairHB(h, 1), hairHB(h, 2)}), 0.3f, 0.6f));
        static const int backParent[3] = {hd::B_NECK1, hd::B_SPINE2, hd::B_SPINE1};
        for (int h = 0; h < kBackHair; ++h)
            parts.push_back(part(c, houndHair(c, false, h, l), q.hair, hairKeep[l], FM_ENT_HAIR, bits({backParent[h / 2], hairBB(h, 0), hairBB(h, 1)}), 0.3f, 0.6f));
    }
}

// ================================================================================================
// Deathmoth (Entity 4)
// ================================================================================================
// Brust mit Panzer, Kopf, Saugruessel
Shape mothBody(const ECtx& c, int lod) {
    using namespace mo;
    Shape s;
    auto J = [&](int j) { return c.J(j); };
    const vec3 F = kF, L = kL, U = kU;
    s.ellipsoid({0.0f, 0.0f, 0.002f}, {0.03f, 0.023f, 0.022f});
    s.ellipsoid({0.026f, 0.0f, 0.006f}, {0.012f, 0.017f, 0.016f}, 0.006f);   // Halsschild
    s.ellipsoid({-0.018f, 0.0f, 0.016f}, {0.01f, 0.01f, 0.006f}, 0.005f);   // Schildchen
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.ellipsoidAx({0.008f, sg * 0.016f, 0.012f}, F, L, {0.016f, 0.007f, 0.005f}, 0.004f);  // Schulterplatten
    }
    s.sphere(J(HEAD), 0.011f, 0.006f);
    s.cone(J(HEAD) + F * 0.008f - U * 0.007f, J(PROB), 0.0021f, 0.0004f, 0.002f);
    if (lod < 2) s.displace = [](vec3 p) { return 0.00012f * sculpt::noise3(p * 900.0f, 501); };
    return s;
}

Shape mothEyes(const ECtx& c) {
    using namespace mo;
    Shape s;
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.sphere(c.J(HEAD) + kF * 0.004f + kL * (sg * 0.0095f) + kU * 0.002f, 0.0078f);
    }
    return s;
}

// Hinterleib: rund, gegliedert, fein behaart
Shape mothAbdomen(const ECtx& c, int lod) {
    using namespace mo;
    Shape s;
    const std::vector<vec3> line = {c.J(ABD0), c.J(ABD1), c.J(ABD2), c.J(ABDTIP)};
    s.cone(c.J(ABD0) + kF * 0.008f, c.J(ABDTIP), 0.012f, 0.005f, 0.006f);
    for (int k = 0; k < 6; ++k) {
        const float t = ((float)k + 0.5f) / 6.0f;
        const vec3 p = along(line, t), d = nrm(along(line, std::min(1.0f, t + 0.05f)) - along(line, std::max(0.0f, t - 0.05f)));
        const float r = 0.022f * (1.0f - 0.55f * std::pow(t, 1.3f));
        s.ellipsoidAx(p, d, kL, {0.0115f, r, r * 0.92f}, 0.004f);
    }
    if (lod < 2) s.displace = [](vec3 p) { return 0.0007f * sculpt::noise3(p * 900.0f, 511) + 0.0005f * sculpt::fbm3(p * 280.0f, 2, 513); };
    return s;
}

// Beine mit Dornen an den Gelenken, Schwanzborsten
Shape mothLegs(const ECtx& c, int lod) {
    using namespace mo;
    Shape s;
    for (int l = 0; l < 6; ++l) {
        const vec3 j0 = c.J(legJ(l, 0)), j1 = c.J(legJ(l, 1)), j2 = c.J(legJ(l, 2)), j3 = c.J(legJ(l, 3));
        s.cone(j0, j1, 0.0026f, 0.0022f, 0.001f);
        s.cone(j1, j2, 0.0021f, 0.0016f, 0.001f);
        s.cone(j2, j3, 0.0014f, 0.0006f, 0.0008f);
        if (lod == 0) {
            const float sg = (l % 2) == 0 ? 1.0f : -1.0f;
            for (const vec3* jj : {&j1, &j2}) {
                for (int k = 0; k < 2; ++k) {
                    const vec3 out = nrm(kL * sg + kU * (k == 0 ? 0.6f : -0.3f) - kF * 0.4f);
                    s.cone(*jj, *jj + out * 0.004f, 0.0009f, 0.0002f, 0.0004f);
                }
            }
            for (int k = 1; k <= 3; ++k) s.sphere(lerpV(j2, j3, (float)k / 4.0f), 0.0011f - 0.0001f * (float)k, 0.0004f);  // Fussglieder
        }
    }
    for (int b : {BRISTLE_L, BRISTLE_R}) s.cone(c.J(ABDTIP), c.J(b), 0.0011f, 0.0002f, 0.0006f);
    return s;
}

// gefiederte Fuehler (Wikidot): Schaft mit Kammzaehnen zu beiden Seiten
Shape mothAntennae(const ECtx& c, int lod) {
    using namespace mo;
    Shape s;
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 b = c.J(sd == 0 ? ANTB_L : ANTB_R), t = c.J(sd == 0 ? ANTT_L : ANTT_R);
        const vec3 d = nrm(t - b);
        const vec3 side = nrm(cross(d, kU)), upS = nrm(cross(side, d));
        s.cone(b, t, 0.0011f, 0.0006f, 0.0005f);
        if (lod == 0) {
            for (int k = 0; k < 16; ++k) {
                const float u = 0.1f + 0.85f * (float)k / 15.0f;
                const vec3 p = lerpV(b, t, u);
                const float len = 0.0075f * (1.0f - 0.6f * u) + 0.0018f;
                for (float sg : {1.0f, -1.0f}) s.cone(p, p + nrm(d * 0.55f + side * sg + upS * 0.15f) * len, 0.00048f, 0.00025f, 0.0002f);
            }
        } else {
            s.ellipsoidAx(lerpV(b, t, 0.5f), d, side, {0.5f * length(t - b), 0.0055f, 0.0009f}, 0.001f);
        }
    }
    return s;
}

// Fluegel: duennes Netz ueber dem Polarraster (th, rho) des Umrisses; Oberseite, Unterseite, Rand
SkinPart mothWing(int w, int side, int lod) {
    using namespace mo;
    const wing::Def& d = wing::def(w);
    const int NT = lod == 0 ? (w == 0 ? 104 : 60) : (lod == 1 ? (w == 0 ? 52 : 30) : (w == 0 ? 26 : 16));
    const int NR = lod == 0 ? 11 : (lod == 1 ? 7 : 4);
    const float sg = side == 0 ? 1.0f : -1.0f;
    const int bone = w == 0 ? (side == 0 ? mo::B_WF_L : mo::B_WF_R) : (side == 0 ? mo::B_WH_L : mo::B_WH_R);
    const float rho0 = 0.05f;
    auto rhoAt = [&](int k) { return rho0 + (1.0f - rho0) * std::pow((float)k / (float)(NR - 1), 0.85f); };
    auto thAt = [&](int i) { return d.th0 + (d.th1 - d.th0) * (float)i / (float)NT; };
    // Mittelflaeche: leicht gewoelbt (Raender sinken etwas), feine Rippen entlang der Adern
    auto mid = [&](float th, float rho) {
        const float r = rho * wing::rmax(w, th);
        const float x = d.rx + r * std::sin(th), y = d.ry + r * std::cos(th);
        const float z = d.z - 0.004f * rho * rho + 0.0004f * rho * std::sin(th * 46.0f);
        return vec3{x, y * sg, z};
    };
    auto thick = [&](float rho) { return 0.0011f * (1.0f - rho) + 0.0005f; };
    SkinPart p;
    p.material = FM_ENT_WING;
    std::vector<vec3> pos;
    std::vector<float> rhoV;
    std::vector<int> faceSide;  // +1 oben, -1 unten
    const int perSide = (NT + 1) * NR + 1;
    for (int s = 0; s < 2; ++s) {
        const float zs = s == 0 ? 0.5f : -0.5f;
        for (int i = 0; i <= NT; ++i)
            for (int k = 0; k < NR; ++k) {
                const float rho = rhoAt(k);
                pos.push_back(mid(thAt(i), rho) + kU * (zs * thick(rho)));
                rhoV.push_back(rho);
                faceSide.push_back(s == 0 ? 1 : -1);
            }
        pos.push_back(vec3{d.rx, d.ry * sg, d.z + zs * thick(0.0f)});
        rhoV.push_back(0.0f);
        faceSide.push_back(s == 0 ? 1 : -1);
    }
    auto id = [&](int s, int i, int k) { return (u32)(s * perSide + i * NR + k); };
    auto rootId = [&](int s) { return (u32)(s * perSide + (NT + 1) * NR); };
    std::vector<u32> idx;
    vec3 centroid{};
    for (const vec3& q : pos) centroid += q;
    centroid = centroid / (float)pos.size();
    // Dreieck mit gewuenschter Ausrichtung (want: Normale zeigt in diese Richtung)
    auto tri = [&](u32 a, u32 b, u32 c2, vec3 want) {
        const vec3 n = cross(pos[b] - pos[a], pos[c2] - pos[a]);
        if (dot(n, want) < 0.0f) std::swap(b, c2);
        idx.push_back(a), idx.push_back(b), idx.push_back(c2);
    };
    for (int s = 0; s < 2; ++s) {
        const vec3 want = s == 0 ? kU : -kU;
        for (int i = 0; i < NT; ++i) {
            for (int k = 0; k + 1 < NR; ++k) {
                tri(id(s, i, k), id(s, i + 1, k), id(s, i + 1, k + 1), want);
                tri(id(s, i, k), id(s, i + 1, k + 1), id(s, i, k + 1), want);
            }
            tri(rootId(s), id(s, i, 0), id(s, i + 1, 0), want);
        }
    }
    // Rand: Vorderkante (i = 0), Aussenrand (k = NR-1), Innenrand (i = NT), Wurzel
    std::vector<std::pair<int, int>> rim;  // (i, k); k = -1: Wurzel
    for (int k = -1; k < NR; ++k) rim.push_back({0, k});
    for (int i = 1; i <= NT; ++i) rim.push_back({i, NR - 1});
    for (int k = NR - 2; k >= -1; --k) rim.push_back({NT, k});
    auto rimId = [&](int s, std::pair<int, int> q) { return q.second < 0 ? rootId(s) : id(s, q.first, q.second); };
    for (size_t e = 0; e + 1 < rim.size(); ++e) {
        if (rim[e].second < 0 && rim[e + 1].second < 0) continue;
        const u32 a0 = rimId(0, rim[e]), b0 = rimId(0, rim[e + 1]), a1 = rimId(1, rim[e]), b1 = rimId(1, rim[e + 1]);
        vec3 out = (pos[a0] + pos[b0]) * 0.5f - centroid;
        out.z = 0.0f;
        tri(a0, b0, b1, out);
        tri(a0, b1, a1, out);
    }
    // Normalen aus den Flaechen
    std::vector<vec3> nrmV(pos.size(), vec3{});
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const vec3 n = cross(pos[idx[t + 1]] - pos[idx[t]], pos[idx[t + 2]] - pos[idx[t]]);
        for (int k = 0; k < 3; ++k) nrmV[idx[t + (size_t)k]] += n;
    }
    p.v.resize(pos.size());
    for (size_t i = 0; i < pos.size(); ++i) {
        SkinVertex& v = p.v[i];
        v.pos = pos[i];
        v.normal = length(nrmV[i]) > 1e-12f ? normalize(nrmV[i]) : kU * (float)faceSide[i];
        // fuer das Muster zaehlt die Flaechenseite (oben/unten), nicht die gemittelte Randnormale
        v.tangent = vec4(kU * (float)faceSide[i], 1.0f);
        const float rho = rhoV[i];
        v.uv = vec2{0.35f * (1.0f - smoothF(0.0f, 0.3f, rho)), 0.6f * smoothF(0.9f, 1.0f, rho)};
        v.bind = v.pos;
        v.bones[0] = (u8)bone, v.bones[1] = v.bones[2] = v.bones[3] = 0;
        v.weights[0] = 255, v.weights[1] = v.weights[2] = v.weights[3] = 0;
    }
    p.idx = std::move(idx);
    return p;
}

void buildMoth(ECtx& c, FigureModel& m) {
    struct Q {
        float body, legs, ant, keep;
    };
    static const Q L[kFigureLods] = {{0.001f, 0.0006f, 0.00045f, 0.28f}, {0.0018f, 0.0011f, 0.0009f, 0.35f}, {0.0035f, 0.0f, 0.0f, 0.5f}};
    for (int l = 0; l < kFigureLods; ++l) {
        c.lod = l;
        const Q& q = L[l];
        auto& parts = m.lods[(size_t)l];
        parts.push_back(part(c, mothBody(c, l), q.body, q.keep, FM_ENT_CHITIN, kAll, 0.25f, 0.3f));
        parts.push_back(part(c, mothAbdomen(c, l), q.body, q.keep, FM_ENT_FUR, kAll, 0.25f, 0.3f));
        parts.push_back(part(c, mothEyes(c), l == 2 ? q.body : q.body * 0.8f, 0.5f, FM_ENT_MOTHEYE, bits({mo::B_HEAD, mo::B_THORAX})));
        if (l < 2) {
            parts.push_back(part(c, mothLegs(c, l), q.legs, q.keep, FM_ENT_CHITIN, kAll, 0.25f, 0.2f));
            parts.push_back(part(c, mothAntennae(c, l), q.ant, q.keep, FM_ENT_CHITIN, bits({mo::B_ANT_L, mo::B_ANT_R}), 0.25f, 0.15f));
        }
        for (int w = 0; w < 2; ++w)
            for (int sd = 0; sd < 2; ++sd) parts.push_back(mothWing(w, sd, l));
    }
}

// ================================================================================================
// Gestalt (Level 0): dunkler, formloser Umriss - Kopf und Schultern, unten zerfasert
// ================================================================================================
Shape gestaltBody(const ECtx& c, int lod) {
    using namespace ge;
    Shape s;
    auto J = [&](int j) { return c.J(j); };
    const vec3 U = kU;
    s.ellipsoid(J(HEAD) + U * 0.04f, {0.092f, 0.082f, 0.112f});
    s.cone(J(NECK) - U * 0.04f, J(HEAD), 0.06f, 0.055f, 0.04f);
    s.ellipsoid({0.0f, 0.0f, 1.47f}, {0.1f, 0.22f, 0.085f}, 0.07f);
    s.cone(J(CHEST), J(MID), 0.19f, 0.15f, 0.08f);
    s.cone(J(MID), J(LOW), 0.15f, 0.1f, 0.06f);
    s.cone(J(LOW), J(LOW) - U * 0.16f, 0.1f, 0.03f, 0.07f);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        s.cone({0.0f, sg * 0.215f, 1.45f}, {0.03f, sg * 0.2f, 0.95f}, 0.052f, 0.03f, 0.05f);  // Arme, im Umriss verschmolzen
        s.cone({0.03f, sg * 0.2f, 0.95f}, {0.04f, sg * 0.18f, 0.72f}, 0.03f, 0.004f, 0.03f);
    }
    // unten: weiche, kurze Fasern, die ueber dem Boden enden
    for (int w = 0; w < kWisps; ++w) {
        const vec3 a = J(wisp(w, 0)), b = lerpV(J(wisp(w, 0)), J(wisp(w, 1)), 0.8f);
        s.cone(a, b, 0.05f, 0.025f, 0.06f);
    }
    if (lod < 2)
        s.displace = [](vec3 p) {
            // rauchiger, unruhiger Rand - nach unten staerker, am Kopf ruhiger
            const float low = 1.0f - smoothF(0.3f, 1.0f, p.z);
            return (0.009f + 0.012f * low) * sculpt::fbm3(vec3{p.x * 6.0f, p.y * 6.0f, p.z * 2.0f}, 3, 801) +
                   0.004f * sculpt::fbm3(p * 22.0f, 2, 803);
        };
    return s;
}

void buildGestalt(ECtx& c, FigureModel& m) {
    const float h[3] = {0.008f, 0.016f, 0.03f};
    for (int l = 0; l < kFigureLods; ++l) {
        c.lod = l;
        m.lods[(size_t)l].push_back(part(c, gestaltBody(c, l), h[l], 0.4f, FM_ENT_SHADOW, kAll, 0.35f));
    }
}

// ================================================================================================
// Nest (Entity 4): fleischige, harzige Wucherung um einen Tuerrahmen
// ================================================================================================
Shape nestBody(int lod) {
    Shape s;
    const float H = 2.2f, hw = 0.5f * ns::kOpenW;
    std::uint32_t q = 1;
    auto R = [&]() { return wing::hash01(q++ * 2654435761u + 977u); };
    // an beiden Pfosten - nach oben dichter und dicker -, vom Durchgang bis auf die Wand
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        // ein durchgehender Wulst am Pfosten ...
        s.cone({0.02f, sg * (hw + 0.03f), 0.0f}, {0.03f, sg * (hw + 0.01f), H}, 0.05f, 0.07f, 0.05f);
        // ... und darauf viele kleinere Ballen, nach oben dichter und dicker
        for (int k = 0; k < 44; ++k) {
            const float z = H * std::pow(R(), 0.65f), t = z / H;
            const float r = (0.03f + 0.09f * t * (0.3f + 0.7f * R())) * (R() < 0.2f ? 1.6f : 1.0f);  // einzelne grosse Beulen
            const vec3 c{-0.22f + 0.32f * R(), sg * (hw + 0.01f + 0.1f * (R() - 0.4f)), z};
            s.ellipsoid(c, {r * (0.7f + 0.5f * R()), r * (0.6f + 0.4f * R()), r * (0.9f + 0.6f * R())}, 0.07f);
        }
    }
    // am Sturz die dickste Masse
    s.cone({0.04f, -0.62f, H + 0.02f}, {0.04f, 0.62f, H + 0.02f}, 0.08f, 0.1f, 0.05f);
    for (int k = 0; k < 30; ++k) {
        const float r = 0.06f + 0.1f * R();
        s.ellipsoid({-0.15f + 0.3f * R(), -0.62f + 1.24f * R(), H + 0.12f * (R() - 0.4f)}, {r * 0.8f, r * (0.8f + 0.6f * R()), r * 0.7f}, 0.06f);
    }
    // auf die Wand gekrochen: flache Fladen
    for (int k = 0; k < 16; ++k) {
        const float r = 0.06f + 0.08f * R();
        const bool above = R() < 0.4f;
        const float y = above ? -0.7f + 1.4f * R() : (R() < 0.5f ? 1.0f : -1.0f) * (hw + 0.08f + 0.2f * R());
        const float z = above ? H + 0.05f + 0.3f * R() : 0.1f + 2.2f * std::pow(R(), 0.6f);
        s.ellipsoid({0.01f + 0.02f * R(), y, z}, {0.025f + 0.015f * R(), r, r * (0.7f + 0.5f * R())}, 0.05f);
    }
    // Adern kriechen vom Rahmen ueber die Wand und verzweigen sich
    for (int k = 0; k < 14; ++k) {
        const float sg = (k % 2) == 0 ? 1.0f : -1.0f;
        const bool top = k >= 10;
        vec3 p0 = top ? vec3{0.015f, -0.5f + R(), H + 0.1f} : vec3{0.015f, sg * (hw + 0.06f), 0.3f + 1.9f * R()};
        float r = 0.016f + 0.012f * R();
        for (int seg = 0; seg < 5; ++seg) {
            const vec3 dir = top ? vec3{0.0f, (R() - 0.5f) * 0.6f, 0.5f + 0.5f * R()} : vec3{0.0f, sg * (0.6f + 0.4f * R()), (R() - 0.5f) * 1.2f};
            const vec3 p1 = p0 + nrm(dir) * (0.08f + 0.08f * R());
            s.cone(p0, {0.008f, p1.y, p1.z}, r, r * 0.75f, 0.01f);
            p0 = {0.008f, p1.y, p1.z};
            r *= 0.75f;
        }
    }
    // am Boden ausgelaufen
    for (int k = 0; k < 8; ++k)
        s.ellipsoid({0.25f * R() - 0.05f, (k % 2 == 0 ? 1.0f : -1.0f) * (0.3f + 0.3f * R()), 0.0f}, {0.12f, 0.1f, 0.03f + 0.02f * R()}, 0.05f);
    // Harzfaeden vom Sturz
    for (int k = 0; k < 9; ++k) {
        const vec3 top{0.04f * R(), -0.42f + 0.84f * ((float)k + R()) / 9.0f, H - 0.06f};
        const float len = 0.25f + 0.45f * R();
        const vec3 bottom = top + vec3{0.03f * (R() - 0.5f), 0.03f * (R() - 0.5f), -len};
        const vec3 mid = lerpV(top, bottom, 0.45f) + vec3{0.01f * (R() - 0.5f), 0.0f, 0.0f};
        s.cone(top, mid, 0.014f, 0.008f, 0.01f);
        s.cone(mid, bottom, 0.008f, 0.0025f, 0.004f);
        s.sphere(bottom, 0.006f, 0.003f);
    }
    // zerrissene Haut ueber der oberen Oeffnung
    s.box({0.03f, 0.0f, 1.98f}, kF, kL, {0.007f, 0.47f, 0.2f}, 0.004f, 0.04f);
    for (int k = 0; k < 7; ++k) s.sphere({0.03f, -0.38f + 0.76f * R(), 1.86f + 0.22f * R()}, 0.04f + 0.07f * R(), 0.02f, Prim::Sub);
    for (int k = 0; k < 10; ++k)
        s.ellipsoid({0.03f, -0.45f + 0.9f * ((float)k + 0.5f) / 10.0f, 1.78f + 0.05f * R()}, {0.03f, 0.05f + 0.03f * R(), 0.05f + 0.06f * R()}, 0.02f, Prim::Sub);
    s.clip = [](vec3 p) { return std::max({-p.z - 0.005f, p.x - 0.35f, -p.x - 0.42f}); };
    if (lod < 2) s.displace = [](vec3 p) { return 0.006f * sculpt::fbm3(p * 14.0f, 3, 901) + 0.0015f * sculpt::noise3(p * 60.0f, 903); };
    return s;
}

void buildNest(ECtx& c, FigureModel& m) {
    const float h[3] = {0.0065f, 0.013f, 0.026f}, keep[3] = {0.25f, 0.3f, 0.45f};
    for (int l = 0; l < kFigureLods; ++l) {
        c.lod = l;
        m.lods[(size_t)l].push_back(part(c, nestBody(l), h[l], keep[l], FM_ENT_NEST, bits({ns::B_BODY}), 0.25f, 3.0f));
    }
}

}  // namespace

FigureModel buildEntity(Entity e, JobSystem* jobs) {
    FigureModel m;
    ECtx c;
    c.sk = &entitySkeleton(e);
    c.jobs = jobs;
    c.children.assign(c.sk->bones.size(), {});
    for (size_t b = 0; b < c.sk->bones.size(); ++b)
        if (c.sk->bones[b].parent >= 0) c.children[(size_t)c.sk->bones[b].parent].push_back((int)b);
    std::vector<mat4> frames;
    creatureFrames(*c.sk, bindPose(*c.sk), frames);
    m.bindInv.resize(frames.size());
    for (size_t b = 0; b < frames.size(); ++b) m.bindInv[b] = rigidInverse(frames[b]);
    switch (e) {
        case Entity::Smiler: buildSmiler(c, m); break;
        case Entity::Hound: buildHound(c, m); break;
        case Entity::Moth: buildMoth(c, m); break;
        case Entity::Gestalt: buildGestalt(c, m); break;
        default: buildNest(c, m); break;
    }
    m.radius = c.sk->reach;
    return m;
}

}  // namespace lim::gfx
