#include "render/figure_models.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>

#include "core/jobs.hpp"
#include "render/head_model.hpp"
#include "render/sculpt.hpp"
#include "render/world_renderer.hpp"

namespace lim::gfx {

using namespace anim;
using sculpt::Prim;
using sculpt::Shape;

namespace {

constexpr u32 bit(Bone b) { return 1u << b; }
constexpr u32 kTorsoBones = (1u << B_PELVIS) | (1u << B_SPINE) | (1u << B_CHEST) | (1u << B_NECK);
constexpr u32 kArmBones = (1u << B_CLAV_L) | (1u << B_UARM_L) | (1u << B_FARM_L) | (1u << B_CLAV_R) | (1u << B_UARM_R) | (1u << B_FARM_R);
constexpr u32 kHandBones = (1u << B_HAND_L) | (1u << B_FING_L) | (1u << B_THUMB_L) | (1u << B_HAND_R) | (1u << B_FING_R) | (1u << B_THUMB_R);
constexpr u32 kLegBones = (1u << B_THIGH_L) | (1u << B_SHIN_L) | (1u << B_THIGH_R) | (1u << B_SHIN_R);
constexpr u32 kFootBones = (1u << B_FOOT_L) | (1u << B_TOE_L) | (1u << B_FOOT_R) | (1u << B_TOE_R);
constexpr u32 kHeadBones = (1u << B_HEAD) | (1u << B_JAW);

// Zellgroessen je Detailstufe (m): Koerper/Kleidung, Kopf/Haende, Kleinteile
constexpr float kBodyH[kFigureLods] = {0.0105f, 0.022f, 0.05f};
constexpr float kFineH[kFigureLods] = {0.0042f, 0.0085f, 0.05f};
constexpr float kSmallH[kFigureLods] = {0.0032f, 0.0065f, 0.0f};
// Anteil der Dreiecke, der nach der Vereinfachung bleibt
constexpr float kKeep[kFigureLods] = {0.27f, 0.3f, 0.6f};

struct Ctx {
    Build b;
    JointPose bp;
    Frames bind{};
    std::array<float, kBoneCount> rad{};
    float H = 1.78f, hs = 1.0f;
    int lod = 0;
    const vec3 F{1, 0, 0}, L{0, 1, 0}, U{0, 0, 1};
    JobSystem* jobs = nullptr;

    vec3 J(int j) const { return bp.j[(size_t)j]; }
    vec3 SJ(Joint left, int s) const { return bp.j[sideJoint(left, s)]; }
    vec3 LJ(Joint left, int s) const { return bp.j[legJoint(left, s)]; }
    float sg(int s) const { return s == 0 ? 1.0f : -1.0f; }
    // Punkt im Kopf (Norm-Masse eines 1,78-m-Kopfes, mit hs skaliert)
    vec3 hd(float x, float y, float z) const { return J(J_HEAD) + F * (x * hs) + L * (y * hs) + U * (z * hs); }
};

float segDist(vec3 p, vec3 a, vec3 b) {
    vec3 ab = b - a;
    float t = std::clamp(dot(p - a, ab) / std::max(dot(ab, ab), 1e-9f), 0.0f, 1.0f);
    return length(p - (a + ab * t));
}

using Policy = std::function<void(vec3, float*)>;

void weigh(const Ctx& c, vec3 p, u32 allowed, float sigma, const Policy& policy, SkinVertex& v) {
    float nd[kBoneCount], w[kBoneCount] = {};
    float best = 1e9f;
    for (int b = 1; b < kBoneCount; ++b) {
        nd[b] = 1e9f;
        if (!(allowed & (1u << b))) continue;
        const BoneSeg& sg = kBoneSeg[(size_t)b];
        nd[b] = segDist(p, c.J(sg.a), c.J(sg.b)) / c.rad[(size_t)b];
        best = std::min(best, nd[b]);
    }
    float sum = 0.0f;
    for (int b = 1; b < kBoneCount; ++b)
        if (nd[b] - best < 5.0f * sigma) w[b] = std::exp(-(nd[b] - best) / sigma), sum += w[b];
    if (sum <= 0.0f) w[B_PELVIS] = 1.0f, sum = 1.0f;
    for (float& x : w) x /= sum;
    if (policy) policy(p, w);
    // die vier staerksten, auf 255 normiert
    int idx[4] = {0, 0, 0, 0};
    float val[4] = {0, 0, 0, 0};
    for (int b = 0; b < kBoneCount; ++b) {
        float x = w[b];
        for (int k = 0; k < 4; ++k)
            if (x > val[k]) {
                for (int m = 3; m > k; --m) val[m] = val[m - 1], idx[m] = idx[m - 1];
                val[k] = x, idx[k] = b;
                break;
            }
    }
    float tot = val[0] + val[1] + val[2] + val[3];
    int q[4], qs = 0;
    for (int k = 0; k < 4; ++k) q[k] = (int)std::lround(val[k] / tot * 255.0f), qs += q[k];
    q[0] += 255 - qs;
    for (int k = 0; k < 4; ++k) v.bones[k] = (u8)idx[k], v.weights[k] = (u8)std::clamp(q[k], 0, 255);
}

// Netz eines Feldes -> Teil mit Gewichten (triplanar)
using Tint = std::function<vec2(vec3)>;  // je Punkt: x Roete, y Dunkelheit (Lippen, Augenhoehlen, Brauen, Bartschatten)

SkinPart toPart(const Ctx& c, const sculpt::RawMesh& m, u32 material, u32 allowed, float sigma, const Policy& policy, const Tint& tint = nullptr) {
    SkinPart part;
    part.material = material;
    part.v.resize(m.pos.size());
    auto one = [&](int i) {
        SkinVertex& v = part.v[(size_t)i];
        v.pos = m.pos[(size_t)i];
        v.normal = m.normal[(size_t)i];
        v.tangent = vec4(v.normal, 1.0f);
        v.uv = tint ? tint(v.pos) : vec2{0, 0};
        v.bind = v.pos;
        weigh(c, v.pos, allowed, sigma, policy, v);
    };
    if (c.jobs) c.jobs->parallelFor((int)m.pos.size(), one);
    else
        for (int i = 0; i < (int)m.pos.size(); ++i) one(i);
    part.idx.assign(m.idx.begin(), m.idx.end());
    return part;
}

SkinPart meshPart(const Ctx& c, const Shape& s, float h, u32 material, u32 allowed, float sigma = 0.35f, const Policy& policy = nullptr,
                  Aabb box = Aabb{}, const Tint& tint = nullptr) {
    if (!box.valid()) box = s.bounds();
    sculpt::RawMesh m = sculpt::mesh(s, box, h, c.jobs);
    const size_t tris = m.idx.size() / 3;
    const size_t target = std::max<size_t>(64, (size_t)((float)tris * kKeep[c.lod]));
    if (target < tris) {
        sculpt::simplify(m, target);
        sculpt::reproject(m, s, h, c.jobs);
    }
    return toPart(c, m, material, allowed, sigma, policy, tint);
}

// --- Koerperformen ---------------------------------------------------------------------------
// g: Aufmass (Kleidung ueber dem Koerper); detail: Muskel- und Knochenrelief
void torso(Shape& s, const Ctx& c, float g, bool detail) {
    const Build& b = c.b;
    const vec3 F = c.F, L = c.L, U = c.U;
    const float hk = c.H / 1.78f;
    s.ellipsoid(c.J(J_PELVIS) + U * (0.018f * hk) - F * 0.006f, {b.hipsD + g, b.hipsW + g, 0.11f * hk + g});
    s.ellipsoid(c.J(J_SPINE) - U * 0.012f + F * (b.bellyFwd * 0.4f), {b.waistD + b.bellyFwd * 0.55f + g, b.waistW + g, 0.135f * hk + g}, 0.07f);
    if (b.bellyFwd > 0.0f)
        s.ellipsoid(c.J(J_SPINE) + F * (b.waistD * 0.5f + b.bellyFwd * 0.35f) - U * (0.055f * hk),
                    {0.055f + b.bellyFwd * 0.75f + g, b.waistW * 0.82f + g, 0.1f * hk + g}, 0.07f);
    s.ellipsoid(c.J(J_CHEST) - U * (0.012f * hk), {b.chestD + g, b.chestW + g, 0.155f * hk + g}, 0.075f);
    // Schulterguertel und Trapezmuskel
    s.ellipsoid(c.J(J_NECK) - U * (0.085f * hk) - F * 0.006f, {b.chestD * 0.72f + g, b.shoulderW * 0.5f - 0.03f + g, 0.05f * hk + g}, 0.05f);
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = c.sg(sd);
        vec3 sh = c.SJ(J_SHOULDER_L, sd);
        s.cone(c.J(J_NECK) + U * (0.005f * hk) - F * 0.015f, sh - U * 0.004f - F * 0.008f, b.neckR * 0.8f + g, 0.026f + g, 0.045f);
        // Deltamuskel
        vec3 armDir = normalize(c.SJ(J_ELBOW_L, sd) - sh);
        s.ellipsoidAx(sh + armDir * (0.04f * hk) + L * (sg * 0.002f), armDir, F,
                      {0.065f * hk + g, b.upperArmR * (0.98f + 0.15f * b.muscle) + g, b.upperArmR * (1.02f + 0.15f * b.muscle) + g}, 0.03f);
        // Gesaess
        s.ellipsoid(c.J(J_PELVIS) - F * (b.hipsD * 0.5f) + L * (sg * 0.064f) - U * (0.055f * hk),
                    {0.072f + g, 0.074f + g, 0.088f * hk + g}, 0.05f);
        if (b.bust > 0.0f)
            s.ellipsoid(c.J(J_CHEST) + F * (b.chestD * 0.7f) + L * (sg * 0.068f) - U * (0.035f * hk),
                        {b.bust * 0.9f + g * 0.6f, b.bust * 1.05f + g, b.bust + g}, 0.045f);
        if (detail) {
            if (b.muscle > 0.05f)
                s.ellipsoid(c.J(J_CHEST) + F * (b.chestD * 0.62f) + L * (sg * 0.06f) + U * (0.035f * hk),
                            {0.02f + 0.02f * b.muscle, 0.065f, 0.048f}, 0.04f);
            // Schulterblatt, Schluesselbein
            s.ellipsoid(c.J(J_CHEST) + U * (0.055f * hk) - F * (b.chestD * 0.78f) + L * (sg * 0.075f), {0.018f + 0.01f * b.thin, 0.055f, 0.068f},
                        0.04f);
            s.capsule(c.SJ(J_STERN_L, sd) + U * 0.004f, sh + U * 0.012f + F * 0.012f, 0.0105f + 0.002f * b.thin, 0.022f);
        }
    }
    if (detail) {
        // Rinne der Wirbelsaeule, Bauchnabel
        s.capsule(c.J(J_SPINE) - F * (b.waistD + 0.004f), c.J(J_NECK) - F * (b.chestD * 0.85f) - U * 0.04f, 0.0075f, 0.02f, Prim::Sub);
        if (b.thin < 0.6f) s.sphere(c.J(J_SPINE) + F * (b.waistD + b.bellyFwd * 0.6f + 0.004f) - U * 0.03f, 0.0065f, 0.008f, Prim::Sub);
    }
}

void neckShape(Shape& s, const Ctx& c, float g, float fromT) {
    vec3 a = lerp(c.J(J_NECK), c.J(J_HEAD), fromT) - c.U * (fromT == 0.0f ? 0.02f : 0.0f);
    s.cone(a, c.J(J_HEAD) + c.U * 0.01f, c.b.neckR * (1.1f - 0.15f * fromT) + g, c.b.neckR * 0.9f + g, 0.035f);
}

void arms(Shape& s, const Ctx& c, float g, bool forearms, bool detail) {
    const Build& b = c.b;
    for (int sd = 0; sd < 2; ++sd) {
        vec3 S = c.SJ(J_SHOULDER_L, sd), E = c.SJ(J_ELBOW_L, sd), W = c.SJ(J_WRIST_L, sd);
        vec3 ad = normalize(E - S), fd = normalize(W - E);
        float ua = length(E - S), fa = length(W - E);
        s.cone(S, E, b.upperArmR + g, b.upperArmR * 0.76f + g, 0.03f);
        if (detail) {
            // Bizeps vorn, Trizeps hinten
            vec3 front = normalize(orthoTo(c.F, ad));
            s.ellipsoidAx(S + ad * (ua * 0.52f) + front * (b.upperArmR * 0.28f), ad, front,
                          {ua * 0.3f, b.upperArmR * (0.62f + 0.25f * b.muscle), b.upperArmR * 0.7f}, 0.03f);
            s.ellipsoidAx(S + ad * (ua * 0.4f) - front * (b.upperArmR * 0.3f), ad, front, {ua * 0.32f, b.upperArmR * 0.7f, b.upperArmR * 0.72f},
                          0.03f);
        }
        if (!forearms) continue;
        s.cone(E, W, b.forearmR + g, b.wristR + g, 0.022f);
        if (detail) {
            vec3 front = normalize(orthoTo(c.F, fd));
            s.ellipsoidAx(E + fd * (fa * 0.28f) + front * (b.forearmR * 0.15f), fd, front,
                          {fa * 0.3f, b.forearmR * (1.02f + 0.1f * b.muscle), b.forearmR * 0.95f}, 0.03f);
            // Ellbogenspitze (Olekranon)
            s.sphere(E - front * (b.forearmR * 0.55f), 0.017f + 0.004f * b.thin, 0.018f);
        }
    }
}

void hands(Shape& s, const Ctx& c, int lod, int onlySide = -1) {
    const Build& b = c.b;
    for (int sd = 0; sd < 2; ++sd) {
        if (onlySide >= 0 && sd != onlySide) continue;
        vec3 W = c.SJ(J_WRIST_L, sd), K = c.SJ(J_KNUCKLE_L, sd), T = c.SJ(J_FINGERTIP_L, sd);
        vec3 d = normalize(K - W);
        vec3 palm = normalize(orthoTo(c.bp.palm[sd], d));
        vec3 side = normalize(cross(palm, d));
        if (sd == 1) side = side * -1.0f;  // Richtung Daumen
        const float pl = length(K - W), hw = b.handW, ws = hw / 0.042f;
        const float thin = b.thin;
        // Handgelenk und Mittelhand
        s.capsule(W - d * 0.03f, W + d * 0.012f, b.wristR, 0.02f);
        s.box(W + d * (pl * 0.55f), d, side, {pl * 0.5f, hw * 0.98f, 0.0125f * ws}, 0.009f, 0.018f);
        vec3 fd = normalize(T - K);
        float fl = length(T - K);
        if (lod >= 2) {
            s.box(K + fd * (fl * 0.45f), fd, side, {fl * 0.45f, hw * 0.9f, 0.0095f * ws}, 0.0085f, 0.02f);
        } else {
            const float off[4] = {0.74f, 0.25f, -0.24f, -0.72f};
            const float len[4] = {0.9f, 1.0f, 0.95f, 0.76f};
            for (int f = 0; f < 4; ++f) {
                vec3 base = K + side * (off[f] * hw * 0.98f) - d * (f == 3 ? 0.008f : 0.0f);
                vec3 dir = normalize(fd + side * (off[f] * 0.05f));
                float r0 = (0.0088f - 0.0006f * (float)f) * ws * (1.0f - 0.12f * thin), r1 = r0 * 0.78f;
                vec3 mid = base + dir * (fl * len[f] * 0.55f);
                s.cone(base, mid, r0, r0 * 0.9f, 0.006f);
                s.cone(mid, base + dir * (fl * len[f] - r1), r0 * 0.9f, r1, 0.004f);
                // Knoechel (mager: deutlicher)
                s.sphere(base + palm * (-0.003f), r0 * (1.02f + 0.15f * thin), 0.006f);
                s.sphere(mid, r0 * (0.95f + 0.12f * thin), 0.004f);
            }
        }
        vec3 TB = c.SJ(J_THUMBBASE_L, sd), TT = c.SJ(J_THUMBTIP_L, sd);
        s.cone(TB - d * 0.01f, lerp(TB, TT, 0.5f), 0.0135f * ws, 0.0105f * ws, 0.012f);
        s.cone(lerp(TB, TT, 0.5f), TT, 0.0105f * ws, 0.0085f * ws, 0.005f);
        // Daumenballen
        s.ellipsoidAx(W + d * (pl * 0.38f) + side * (hw * 0.55f) + palm * 0.008f, d, side, {pl * 0.32f, hw * 0.42f, 0.014f}, 0.015f);
    }
}

void legs(Shape& s, const Ctx& c, float g, bool shins, bool detail) {
    const Build& b = c.b;
    for (int sd = 0; sd < 2; ++sd) {
        vec3 Hp = c.LJ(J_HIP_L, sd), K = c.LJ(J_KNEE_L, sd), A = c.LJ(J_ANKLE_L, sd);
        vec3 td = normalize(K - Hp), sdn = normalize(A - K);
        float tl = length(K - Hp), sl = length(A - K);
        vec3 front = normalize(orthoTo(c.F, td));
        s.cone(Hp - td * 0.04f, K, b.thighR + g, b.kneeR * 1.05f + g, 0.045f);
        if (detail) {
            s.ellipsoidAx(Hp + td * (tl * 0.45f) + front * (b.thighR * 0.3f), td, front, {tl * 0.36f, b.thighR * 0.82f, b.thighR * 0.8f}, 0.04f);
            s.ellipsoidAx(Hp + td * (tl * 0.4f) - front * (b.thighR * 0.28f), td, front, {tl * 0.36f, b.thighR * 0.78f, b.thighR * 0.8f}, 0.04f);
        }
        s.sphere(K + front * (b.kneeR * 0.08f), b.kneeR + g, 0.03f);
        if (detail) s.ellipsoidAx(K + front * (b.kneeR * 0.72f) + c.U * 0.006f, c.U, c.L, {0.026f, 0.022f, 0.014f}, 0.015f);
        if (!shins) continue;
        vec3 sfront = normalize(orthoTo(c.F, sdn));
        s.cone(K, A, b.kneeR * 0.92f + g, b.ankleR + g, 0.025f);
        s.ellipsoidAx(K + sdn * (sl * 0.3f) - sfront * (b.calfR * 0.32f), sdn, sfront,
                      {sl * 0.3f, b.calfR * 0.88f + g, b.calfR * 0.86f + g}, 0.045f);
        s.sphere(A, b.ankleR * 1.12f + g, 0.02f);
    }
}

void bareFeet(Shape& s, const Ctx& c, bool detail) {
    const Build& b = c.b;
    for (int sd = 0; sd < 2; ++sd) {
        vec3 A = c.LJ(J_ANKLE_L, sd), Bl = c.LJ(J_BALL_L, sd), T = c.LJ(J_TOETIP_L, sd);
        vec3 fd = normalize(orthoTo(Bl - A, c.U));
        vec3 side = normalize(cross(c.U, fd));
        vec3 heel = vec3{A.x, A.y, 0.0f} - fd * 0.04f + c.U * 0.03f;
        s.cone(heel, vec3{Bl.x, Bl.y, 0.0f} + c.U * 0.022f, 0.03f, 0.024f, 0.03f);
        s.cone(A - c.U * 0.01f, vec3{Bl.x, Bl.y, 0.0f} + c.U * 0.03f, b.ankleR * 0.95f, 0.022f, 0.03f);
        vec3 toeBase = vec3{Bl.x, Bl.y, 0.0f} + c.U * 0.016f;
        float tl = length(orthoTo(T - Bl, c.U));
        if (detail) {
            const float off[5] = {0.026f, 0.01f, -0.004f, -0.017f, -0.029f};
            const float len[5] = {1.0f, 0.92f, 0.82f, 0.72f, 0.6f};
            const float rr[5] = {0.0125f, 0.0085f, 0.008f, 0.0075f, 0.007f};
            float sgn = sd == 0 ? -1.0f : 1.0f;  // grosser Zeh innen
            for (int t = 0; t < 5; ++t) {
                vec3 p0 = toeBase + side * (sgn * -off[t] * 1.0f);
                s.cone(p0, p0 + fd * (tl * len[t]) - c.U * 0.004f, rr[t], rr[t] * 0.85f, 0.006f);
            }
        } else {
            s.box(toeBase + fd * (tl * 0.45f), fd, side, {tl * 0.5f, 0.036f, 0.011f}, 0.01f, 0.015f);
        }
        // Fussgewoelbe innen
        s.ellipsoid(vec3{A.x, A.y, 0.0f} + fd * 0.05f + side * ((sd == 0 ? -1.0f : 1.0f) * 0.03f), {0.04f, 0.012f, 0.01f}, 0.015f, Prim::Sub);
    }
}

// --- Kopf ------------------------------------------------------------------------------------
struct HeadStyle {
    bool face = true;        // false: kein Gesicht (Der Andere)
    float eyeDeep = 1.0f;    // Tiefe der Augenhoehlen
    float faceLong = 1.0f;   // Gesichtslaenge
    float cheeks = 0.0f;     // Wangenfuelle (-1 eingefallen .. 1 voll)
    bool female = false;
    float nose = 1.0f, lips = 1.0f, brow = 1.0f;
    float stubble = 0.0f;    // Bartschatten (Faerbung)
    float eyeOpen = 1.0f;    // Lidspalte
    bool openEyes = true;    // grobe Stufen: geschlossen, nur Schatten
};

HeadParams headParams(const Ctx& c, const HeadStyle& st) {
    HeadParams hp;
    hp.features = st.face;
    hp.female = st.female;
    hp.eyeDeep = st.eyeDeep;
    hp.faceLong = st.faceLong;
    hp.cheeks = st.cheeks;
    hp.thin = c.b.thin;
    hp.jawW = c.b.jawW;
    hp.nose = st.nose;
    hp.lips = st.lips;
    hp.brow = st.brow;
    hp.eyeOpen = st.eyeOpen;
    hp.openEyes = st.openEyes;
    return hp;
}

// Kopfmittelpunkt (Augenhoehe) in der Ruhepose
vec3 headOrigin(const Ctx& c) {
    vec3 o = headOriginFromAtlas();
    return c.J(J_HEAD) + c.F * (o.x * c.hs) + c.U * (o.z * c.hs);
}
vec3 headPoint(const Ctx& c, vec3 q) { return headOrigin(c) + (c.F * q.x + c.L * q.y + c.U * q.z) * c.hs; }

void head(Shape& s, const Ctx& c, const HeadStyle& st) {
    const HeadParams hp = headParams(c, st);
    const vec3 o = headOrigin(c), F = c.F, L = c.L, U = c.U;
    const float hs = c.hs;
    Aabb box;
    box.add(headPoint(c, {-0.14f, -0.12f, -0.18f}));
    box.add(headPoint(c, {0.18f, 0.12f, 0.14f}));
    s.custom([=](vec3 p) {
        vec3 d = p - o;
        vec3 q{dot(d, F) / hs, dot(d, L) / hs, dot(d, U) / hs};
        return headDistance(q, hp) * hs;
    }, box);
    // Ohren: hochkant, oben leicht nach hinten geneigt, mit Muschel (Der Andere hat keine)
    if (!st.face) return;
    for (float sg : {1.0f, -1.0f}) {
        vec3 ec = headPoint(c, {-0.014f, sg * 0.071f, -0.006f});
        vec3 ax = normalize(F * 0.95f - U * 0.3f), up = normalize(U * 0.95f + F * 0.3f);
        s.ellipsoidAx(ec, ax, L, vec3{0.017f, 0.0075f, 0.031f} * hs * 1.0f, 0.007f * hs);
        (void)up;
        if (st.face) s.ellipsoidAx(ec + L * (sg * 0.006f * hs) + F * (0.002f * hs), ax, L, vec3{0.009f, 0.004f, 0.018f} * hs, 0.003f * hs, Prim::Sub);
    }
}

// Hals vom Rumpf bis unter den Kopf; inset: duenner (liegt unter dem Hals eines Koerperteils)
void headNeck(Shape& s, const Ctx& c, float inset, bool female) {
    vec3 top = headPoint(c, {-0.028f, 0.0f, -0.065f});
    s.cone(c.J(J_NECK) - c.U * 0.06f + c.F * 0.005f, top, c.b.neckR * 1.12f - inset, c.b.neckR * 0.92f - inset, 0.03f * c.hs);
    if (!female && inset == 0.0f)  // Adamsapfel
        s.ellipsoid(lerp(c.J(J_NECK), top, 0.45f) + c.F * (c.b.neckR * 0.88f), vec3{0.011f, 0.01f, 0.016f} * c.hs, 0.014f * c.hs);
}

// --- explizite Kleinteile ---------------------------------------------------------------------
void addVertex(SkinPart& p, const Ctx& c, vec3 pos, vec3 n, vec4 t, vec2 uv, u32 allowed) {
    SkinVertex v{};
    v.pos = pos, v.normal = n, v.tangent = t, v.uv = uv, v.bind = pos;
    weigh(c, pos, allowed, 0.3f, nullptr, v);
    p.v.push_back(v);
}

// Augapfel mit dem Pol in Blickrichtung (Iris im Atlas L_FIGDETAIL)
void eyeball(SkinPart& p, const Ctx& c, vec3 center, float r, int seg) {
    const vec3 f = c.F, l = c.L, u = c.U;
    const u32 base = (u32)p.v.size();
    const int rings = seg / 2;
    for (int i = 0; i <= rings; ++i) {
        float th = (float)i / rings * kPi;  // 0 = Pupille
        for (int j = 0; j <= seg; ++j) {
            float ph = (float)j / seg * 2.0f * kPi;
            vec3 n = f * std::cos(th) + (l * std::cos(ph) + u * std::sin(ph)) * std::sin(th);
            // Atlas: Iris-Feld unten links (u 0..0.25, v 0.75..1), radial
            float rr = std::min(th / 0.67f, 1.0f);  // Iris bis ~30 Grad, Pupille ~13 Grad
            vec2 uv{0.125f + 0.12f * rr * std::cos(ph), 0.875f + 0.12f * rr * std::sin(ph)};
            vec3 t = normalize(cross(u, n));
            addVertex(p, c, center + n * r, n, vec4(t, 1.0f), uv, bit(B_HEAD));
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < seg; ++j) {
            u32 a = base + (u32)(i * (seg + 1) + j), b = a + (u32)seg + 1;
            p.idx.insert(p.idx.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
}

Policy headPolicy(const Ctx& c) {
    const float mouthZ = c.hd(0, 0, -0.03f).z;
    return [mouthZ](vec3 p, float* w) {
        if (p.z > mouthZ) {
            w[B_HEAD] += w[B_JAW];
            w[B_JAW] = 0.0f;
        }
    };
}

// Stoffknitter: Falten laengs zur Schwerkraft (Mantel) oder unregelmaessig (Hemd)
std::function<float(vec3)> folds(float amp, vec3 freq, unsigned seed) {
    return [=](vec3 p) {
        vec3 q{p.x * freq.x, p.y * freq.y, p.z * freq.z};
        return amp * sculpt::fbm3(q, 3, seed);
    };
}

// ---------------------------------------------------------------------------------------------
struct Assembly {
    Ctx c;
    FigureModel m;
};

// Kopf mit Hals (der Hals reicht bis unter Kragen bzw. in den Rumpf) und Augen.
// bodyNeck: die Figur hat einen eigenen Hals im Koerperteil - dann liegt dieser hier knapp darunter
void addHeadParts(Assembly& as, int lod, u32 skinMat, const HeadStyle& hstIn, bool eyes, bool bodyNeck = false) {
    Ctx& c = as.c;
    auto& parts = as.m.lods[(size_t)lod];
    HeadStyle hst = hstIn;
    hst.openEyes = lod == 0;  // erst in der Naehe: offene Lider und Augaepfel
    eyes = eyes && lod == 0;
    Shape s;
    head(s, c, hst);
    headNeck(s, c, bodyNeck ? 0.0025f : 0.0f, hst.female);
    const float cutZ = c.J(J_NECK).z - 0.05f;
    s.clip = [cutZ](vec3 p) { return cutZ - p.z; };
    if (lod < 2) {
        // Farbe im Gesicht: Lippen roetlich, Augenhoehlen und Brauen dunkler, Wangen leicht gerötet,
        // bei Maennern ein Bartschatten
        const vec3 o = headOrigin(c), F = c.F, L = c.L, U = c.U;
        const float hs = c.hs;
        const bool face = hst.face, female = hst.female;
        const float stubble = hst.stubble;
        const bool open = hst.openEyes;
        Tint tint = [=](vec3 p) {
            vec3 d = p - o;
            vec3 q{dot(d, F) / hs, dot(d, L) / hs, dot(d, U) / hs};
            float r = length(q);
            if (r < 1e-4f || !face) return vec2{0.0f, 0.0f};
            float th = std::atan2(q.y, q.x), ph = std::asin(std::clamp(q.z / r, -1.0f, 1.0f)), at = std::fabs(th);
            auto G = [](float x, float c0, float w) { float e = (x - c0) / w; return std::exp(-e * e); };
            float red = 0.75f * G(th, 0.0f, 0.24f) * (G(ph, -0.57f, 0.035f) + G(ph, -0.69f, 0.035f)) + 0.28f * G(at, 0.62f, 0.2f) * G(ph, -0.28f, 0.12f) +
                        0.15f * G(th, 0.0f, 0.1f) * G(ph, -0.33f, 0.08f);
            float dark = 0.4f * G(at, 0.42f, 0.2f) * G(ph, -0.02f, 0.1f) + 0.18f * G(at, 0.42f, 0.14f) * G(ph, -0.12f, 0.05f);
            if (!open) dark += 0.55f * G(at, 0.42f, 0.09f) * G(ph, -0.03f, 0.045f);  // geschlossene Stufe: Auge als Schatten
            if (!female) dark += 0.55f * G(at, 0.38f, 0.17f) * G(ph, 0.165f, 0.022f);  // Brauen
            else dark += 0.35f * G(at, 0.4f, 0.15f) * G(ph, 0.17f, 0.016f);
            if (stubble > 0.0f && ph < -0.38f && at < 1.25f) dark += stubble * smoothstep(-0.38f, -0.6f, ph) * (1.0f - 0.7f * G(th, 0.0f, 0.2f) * G(ph, -0.63f, 0.06f));
            return vec2{std::clamp(red, 0.0f, 1.0f), std::clamp(dark, 0.0f, 1.0f)};
        };
        parts.push_back(meshPart(c, s, kFineH[lod], skinMat, kHeadBones | bit(B_NECK) | bit(B_CHEST), 0.3f, headPolicy(c), Aabb{}, tint));
        parts.back().head = true;
        if (eyes) {
            SkinPart e;
            e.material = FM_EYE;
            const HeadParams hp = headParams(c, hst);
            for (int sd = 0; sd < 2; ++sd) eyeball(e, c, headPoint(c, headEyeCenter(sd, hp)), headEyeRadius() * c.hs, lod == 0 ? 18 : 10);
            e.head = true;
            parts.push_back(std::move(e));
        }
    }
}

void addHandParts(Assembly& as, int lod, u32 skinMat) {
    Ctx& c = as.c;
    if (lod >= 2) return;  // in der groebsten Stufe Teil des Koerpers
    Shape s;
    hands(s, c, lod);
    as.m.lods[(size_t)lod].push_back(meshPart(c, s, kFineH[lod], skinMat, kHandBones | bit(B_FARM_L) | bit(B_FARM_R), 0.3f));
}

// kurzes Haar: Schale ueber dem Schaedel oberhalb des Haaransatzes
void shortHair(Assembly& as, int lod, unsigned seed) {
    Ctx& c = as.c;
    Shape s;
    HeadStyle plain;
    plain.face = false;
    head(s, c, plain);
    s.offset = 0.0065f;
    const vec3 f = c.F, u = c.U, l = c.L, origin = headOrigin(c);
    const float hs = c.hs;
    s.clip = [f, u, l, origin, hs](vec3 p) {
        vec3 d = p - origin;
        float x = dot(d, f) / hs, y = std::fabs(dot(d, l)) / hs, z = dot(d, u) / hs;
        // Haaransatz: Stirn hoch (Geheimratsecken), ueber den Ohren, im Nacken tief
        float front = std::clamp(x / 0.09f, 0.0f, 1.0f);
        float line = 0.025f + 0.045f * front + 0.012f * front * std::clamp((y - 0.02f) / 0.04f, 0.0f, 1.0f);
        line -= 0.075f * std::clamp(-x / 0.07f, 0.0f, 1.0f);
        if (y > 0.06f && x > -0.04f && x < 0.03f) line += 0.03f;  // ueber den Ohren frei
        return (line - z) * hs;
    };
    s.displace = [seed](vec3 p) { return 0.0016f * sculpt::fbm3(p * 160.0f, 2, seed); };
    float h = lod == 0 ? kFineH[0] : (lod == 1 ? kFineH[1] : 0.02f);
    as.m.lods[(size_t)lod].push_back(meshPart(c, s, h, FM_HAIR_SHORT, kHeadBones | bit(B_NECK), 0.3f));
    as.m.lods[(size_t)lod].back().head = true;
}

void shoes(Assembly& as, int lod, u32 mat, float bulk, bool sneaker) {
    Ctx& c = as.c;
    Shape s;
    for (int sd = 0; sd < 2; ++sd) {
        vec3 A = c.LJ(J_ANKLE_L, sd), Bl = c.LJ(J_BALL_L, sd), T = c.LJ(J_TOETIP_L, sd);
        vec3 fd = normalize(orthoTo(T - A, c.U));
        vec3 side = normalize(cross(c.U, fd));
        vec3 heel = vec3{A.x, A.y, 0.0f} - fd * 0.05f;
        vec3 toe = vec3{T.x, T.y, 0.0f} + fd * 0.012f;
        vec3 mid = (heel + toe) * 0.5f;
        float len = length(toe - heel) * 0.5f;
        s.box(mid + c.U * 0.045f, fd, side, {len, 0.047f + bulk, 0.04f}, 0.032f, 0.0f);
        s.cone(vec3{A.x, A.y, 0.06f}, vec3{A.x, A.y, 0.11f + (sneaker ? 0.01f : 0.0f)} - fd * 0.01f, 0.05f + bulk, 0.044f + bulk, 0.03f);
        // Spann
        s.ellipsoidAx(vec3{Bl.x, Bl.y, 0.0f} - fd * 0.04f + c.U * 0.055f, fd, side, {0.07f, 0.045f + bulk, 0.035f}, 0.03f);
        (void)Bl;
    }
    s.displace = [](vec3 p) { return 0.0008f * sculpt::fbm3(p * 90.0f, 2, 7); };
    float h = lod == 0 ? 0.0055f : (lod == 1 ? 0.011f : 0.03f);
    as.m.lods[(size_t)lod].push_back(meshPart(c, s, h, mat, kFootBones | kLegBones, 0.3f));
    if (sneaker && lod < 2) {  // helle Gummisohle
        Shape so;
        for (int sd = 0; sd < 2; ++sd) {
            vec3 A = c.LJ(J_ANKLE_L, sd), T = c.LJ(J_TOETIP_L, sd);
            vec3 fd = normalize(orthoTo(T - A, c.U));
            vec3 side = normalize(cross(c.U, fd));
            vec3 heel = vec3{A.x, A.y, 0.0f} - fd * 0.055f, toe = vec3{T.x, T.y, 0.0f} + fd * 0.016f;
            so.box((heel + toe) * 0.5f + c.U * 0.016f, fd, side, {length(toe - heel) * 0.5f, 0.051f + bulk, 0.016f}, 0.012f);
        }
        as.m.lods[(size_t)lod].push_back(meshPart(c, so, lod == 0 ? 0.004f : 0.008f, FM_SOLE, kFootBones, 0.3f));
    }
}

void trousers(Assembly& as, int lod, u32 mat, float g, float hemZ, float waistUp, bool ragged, unsigned seed) {
    Ctx& c = as.c;
    Shape s;
    // Becken und Beine
    const float hk = c.H / 1.78f;
    s.ellipsoid(c.J(J_PELVIS) + c.U * (0.018f * hk) - c.F * 0.006f, {c.b.hipsD + g, c.b.hipsW + g, 0.11f * hk + g});
    const float gl = 1.0f - 0.3f * c.b.thin;  // mager: flaches Gesaess
    for (int sd = 0; sd < 2; ++sd)
        s.ellipsoid(c.J(J_PELVIS) - c.F * (c.b.hipsD * 0.5f) + c.L * (c.sg(sd) * 0.064f * gl) - c.U * (0.055f * hk),
                    {0.072f * gl + g, 0.074f * gl + g, 0.088f * hk * gl + g}, 0.05f);
    s.ellipsoid(c.J(J_SPINE) - c.U * 0.04f + c.F * (c.b.bellyFwd * 0.4f), {c.b.waistD + c.b.bellyFwd * 0.5f + g, c.b.waistW + g, 0.09f + g}, 0.06f);
    legs(s, c, g, true, false);
    const float waistZ = c.J(J_SPINE).z - 0.03f + waistUp;
    s.clip = [waistZ, hemZ, ragged, seed](vec3 p) {
        float hem = hemZ;
        if (ragged) hem += 0.03f * sculpt::fbm3(vec3{p.x * 40.0f, p.y * 40.0f, 0.0f}, 2, seed) + 0.015f;
        return std::max(p.z - waistZ, hem - p.z);
    };
    s.displace = [seed](vec3 p) {
        return 0.0035f * sculpt::fbm3(vec3{p.x * 30.0f, p.y * 30.0f, p.z * 9.0f}, 3, seed) +
               0.0015f * sculpt::fbm3(p * 70.0f, 2, seed + 5);
    };
    as.m.lods[(size_t)lod].push_back(
        meshPart(c, s, kBodyH[lod], mat, kLegBones | bit(B_PELVIS) | bit(B_SPINE) | kFootBones, 0.4f));
}

// Oberteil: Rumpf + Aermel bis sleeveT (0 Schulter .. 1 Ellbogen .. 2 Handgelenk)
Shape topShape(const Ctx& c, float g, float sleeveT, float hemZ, float collarUp) {
    Shape s;
    torso(s, c, g, false);
    neckShape(s, c, g * 0.7f, 0.0f);
    for (int sd = 0; sd < 2; ++sd) {
        vec3 S = c.SJ(J_SHOULDER_L, sd), E = c.SJ(J_ELBOW_L, sd), W = c.SJ(J_WRIST_L, sd);
        vec3 ad = normalize(E - S);
        s.cone(S + ad * 0.025f, E, c.b.upperArmR * 0.95f + g * 0.75f, c.b.upperArmR * 0.82f + g, 0.03f);
        if (sleeveT > 1.0f) s.cone(E, W + normalize(W - E) * 0.01f, c.b.forearmR * 1.05f + g, c.b.wristR * 1.2f + g, 0.025f);
    }
    const vec3 neck = c.J(J_NECK), U = c.U;
    const float collarZ = neck.z + collarUp;
    // Aermelenden als Ebenen senkrecht zum Arm
    struct Cut {
        vec3 o, n;
    };
    std::vector<Cut> cuts;
    for (int sd = 0; sd < 2; ++sd) {
        vec3 S = c.SJ(J_SHOULDER_L, sd), E = c.SJ(J_ELBOW_L, sd), W = c.SJ(J_WRIST_L, sd);
        vec3 o, n;
        if (sleeveT <= 1.0f) o = lerp(S, E, sleeveT), n = normalize(E - S);
        else o = lerp(E, W, sleeveT - 1.0f), n = normalize(W - E);
        cuts.push_back({o, n});
    }
    s.clip = [cuts, hemZ, collarZ, neck](vec3 p) {
        float d = std::max(hemZ - p.z, p.z - collarZ);
        for (const Cut& k : cuts) {
            // nur auf der Seite des Arms schneiden
            vec3 r = p - k.o;
            if (dot(r, k.n) > 0.0f && length(orthoTo(r, k.n)) < 0.12f) d = std::max(d, dot(r, k.n));
        }
        (void)neck;
        return d;
    };
    return s;
}

void buildPlayer(Assembly& as) {
    Ctx& c = as.c;
    for (int lod = 0; lod < kFigureLods; ++lod) {
        c.lod = lod;
        auto& parts = as.m.lods[(size_t)lod];
        addHandParts(as, lod, FM_SKIN_PLAYER);
        HeadStyle hst;
        if (lod < 2) addHeadParts(as, lod, FM_SKIN_PLAYER, hst, true);
        else {
            Shape h;
            head(h, c, hst);
            neckShape(h, c, 0, 0.3f);
            hands(h, c, 2);
            parts.push_back(meshPart(c, h, 0.02f, FM_SKIN_PLAYER, kHeadBones | bit(B_NECK) | kHandBones | kArmBones, 0.3f));
        }
        shortHair(as, lod, 13);
        // Kapuzenjacke: lange Aermel mit Buendchen, Kapuze im Nacken
        Shape hoodie = topShape(c, 0.022f, 1.96f, c.J(J_PELVIS).z - 0.02f, 0.02f);
        hoodie.ellipsoid(c.J(J_NECK) - c.F * (c.b.neckR + 0.035f) + c.U * 0.0f, {0.05f, 0.11f, 0.075f}, 0.05f);
        hoodie.displace = [](vec3 p) {
            return 0.0042f * sculpt::fbm3(vec3{p.x * 20.0f, p.y * 20.0f, p.z * 12.0f}, 3, 101) + 0.001f * sculpt::fbm3(p * 70.0f, 2, 103);
        };
        parts.push_back(meshPart(c, hoodie, kBodyH[lod], FM_HOODIE, kTorsoBones | kArmBones, 0.4f));
        trousers(as, lod, FM_JEANS, 0.014f, c.LJ(J_ANKLE_L, 0).z + 0.045f, 0.0f, false, 107);
        shoes(as, lod, FM_SNEAKER, 0.004f, true);
    }
}

}  // namespace

FigureModel buildFigure(Figure f, JobSystem* jobs) {
    Assembly as;
    Ctx& c = as.c;
    c.b = figureBuild(f);
    c.H = c.b.height;
    c.hs = c.b.headScale * (0.6f + 0.4f * c.H / 1.78f);
    c.bp = bindPose(c.b);
    c.jobs = jobs;
    computeFrames(c.bp, c.bind);
    as.m.bindInv.resize(kBoneCount);
    for (int b = 0; b < kBoneCount; ++b) as.m.bindInv[(size_t)b] = rigidInverse(c.bind[(size_t)b]);
    const Build& b = c.b;
    auto& r = c.rad;
    r.fill(0.05f);
    r[B_PELVIS] = b.hipsW, r[B_SPINE] = b.waistW, r[B_CHEST] = b.chestW, r[B_NECK] = b.neckR * 1.3f;
    r[B_HEAD] = 0.085f * c.hs, r[B_JAW] = 0.045f * c.hs;
    for (int s = 0; s < 2; ++s) {
        r[sideBone(B_CLAV_L, s)] = 0.05f, r[sideBone(B_UARM_L, s)] = b.upperArmR, r[sideBone(B_FARM_L, s)] = b.forearmR;
        r[sideBone(B_HAND_L, s)] = 0.026f, r[sideBone(B_FING_L, s)] = 0.012f, r[sideBone(B_THUMB_L, s)] = 0.012f;
        r[legBone(B_THIGH_L, s)] = b.thighR, r[legBone(B_SHIN_L, s)] = b.calfR, r[legBone(B_FOOT_L, s)] = 0.04f, r[legBone(B_TOE_L, s)] = 0.03f;
    }
    (void)f;
    buildPlayer(as);  // V10: Menschen gibt es nur noch als Spieler (und dessen Trugbild)
    as.m.radius = std::max(0.9f, c.H * 0.62f);
    return std::move(as.m);
}

std::string figureStats(const FigureModel& m) {
    std::string s;
    for (int l = 0; l < kFigureLods; ++l) {
        size_t tris = 0, verts = 0;
        for (const auto& p : m.lods[(size_t)l]) tris += p.idx.size() / 3, verts += p.v.size();
        s += std::format("{}L{}: {} Teile, {} Dreiecke, {} Punkte", l ? "  " : "", l, m.lods[(size_t)l].size(), tris, verts);
    }
    return s;
}

}  // namespace lim::gfx
