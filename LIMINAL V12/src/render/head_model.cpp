#include "render/head_model.hpp"

#include <algorithm>
#include <cmath>

namespace lim::gfx {

namespace {

// Catmull-Rom durch Stuetzstellen (xs aufsteigend)
float curve(const float (*pts)[2], int n, float x) {
    if (x <= pts[0][0]) return pts[0][1];
    if (x >= pts[n - 1][0]) return pts[n - 1][1];
    int i = 0;
    while (i < n - 2 && x > pts[i + 1][0]) ++i;
    float x0 = pts[i][0], x1 = pts[i + 1][0];
    float t = (x - x0) / (x1 - x0);
    float p0 = pts[std::max(0, i - 1)][1], p1 = pts[i][1], p2 = pts[i + 1][1], p3 = pts[std::min(n - 1, i + 2)][1];
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float g(float x, float c, float w) {
    float d = (x - c) / w;
    return std::exp(-d * d);
}

float sminF(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return b + (a - b) * h - k * h * (1.0f - h);
}
float smaxF(float a, float b, float k) { return -sminF(-a, -b, k); }

// Profile (Elevation in rad -> Radius in m)
const float kFront[][2] = {{-1.57f, 0.104f}, {-1.3f, 0.118f}, {-1.08f, 0.135f}, {-0.92f, 0.138f}, {-0.72f, 0.126f}, {-0.5f, 0.112f},
                           {-0.3f, 0.104f},  {-0.12f, 0.095f}, {0.0f, 0.093f},  {0.16f, 0.099f}, {0.45f, 0.106f}, {0.8f, 0.111f},
                           {1.2f, 0.114f},   {1.57f, 0.115f}};
const float kSide[][2] = {{-1.57f, 0.07f}, {-1.25f, 0.078f}, {-0.98f, 0.086f}, {-0.72f, 0.078f}, {-0.45f, 0.073f}, {-0.2f, 0.073f},
                          {0.0f, 0.075f},  {0.35f, 0.079f},  {0.8f, 0.088f},   {1.2f, 0.103f},   {1.57f, 0.115f}};
const float kBack[][2] = {{-1.57f, 0.07f}, {-1.15f, 0.074f}, {-0.75f, 0.08f}, {-0.4f, 0.09f}, {0.0f, 0.098f},
                          {0.4f, 0.104f},  {0.9f, 0.11f},    {1.3f, 0.113f}, {1.57f, 0.115f}};

}  // namespace

vec3 headOriginFromAtlas() { return {0.027f, 0.0f, 0.044f}; }
float headEyeRadius() { return 0.0122f; }
vec3 headEyeCenter(int side, const HeadParams& hp) {
    float s = side == 0 ? 1.0f : -1.0f;
    return {0.067f - 0.004f * (hp.eyeDeep - 1.0f), s * 0.0315f, -0.004f};
}

float headDistance(vec3 q, const HeadParams& hp) {
    const float r = length(q);
    if (r < 1e-5f) return -0.08f;
    const vec3 d = q / r;
    const float th = std::atan2(d.y, d.x);  // 0 vorn, + links
    const float ph = std::asin(std::clamp(d.z, -1.0f, 1.0f));
    const float ath = std::fabs(th);
    // Profile mischen: das Gesicht bleibt vorn lange flach, dann rundet es zur Seite ab
    const float u = ath / 1.5708f;
    float R;
    {
        float phF = ph;
        float rf = curve(kFront, 14, phF);
        // laengeres Untergesicht
        rf *= 1.0f + (hp.faceLong - 1.0f) * smoothstep(-0.3f, -1.0f, ph);
        float rs = curve(kSide, 11, ph);
        float rb = curve(kBack, 9, ph);
        // Kieferbreite: unteres Drittel der Seite
        float jaw = 1.0f + (hp.jawW - 1.0f) * smoothstep(-0.3f, -0.95f, ph);
        rs *= jaw;
        if (hp.female) rs *= 1.0f - 0.04f * smoothstep(-0.3f, -0.9f, ph), rf *= 1.0f - 0.025f * smoothstep(-0.6f, -1.0f, ph);
        if (u <= 1.0f) {
            float t = smoothstep(0.18f, 1.0f, u);
            t = std::pow(t, 0.8f);
            R = rf + (rs - rf) * t;
        } else {
            float t = smoothstep(1.0f, 1.95f, u);
            R = rs + (rb - rs) * t;
        }
    }
    // Merkmale (nur vorn)
    if (hp.features) {
        const float sideTh = ath;
        R += 0.0055f * hp.brow * g(th, 0.0f, 0.62f) * g(ph, 0.15f, 0.075f) * (hp.female ? 0.5f : 1.0f);
        // Augenhoehlen
        R -= 0.0165f * hp.eyeDeep * g(sideTh, 0.42f, 0.19f) * g(ph, -0.03f, 0.085f);
        // Nase: Ruecken, Spitze (breiter), Fluegel
        float wn = 0.07f + 0.055f * smoothstep(-0.02f, -0.34f, ph);
        R += hp.nose * (0.0055f * g(ph, -0.06f, 0.13f) + 0.028f * g(ph, -0.32f, 0.09f)) * g(th, 0.0f, wn) * (hp.female ? 0.85f : 1.0f);
        R += 0.009f * hp.nose * g(sideTh, 0.15f, 0.06f) * g(ph, -0.4f, 0.055f);
        R -= 0.006f * g(th, 0.0f, 0.05f) * g(ph, -0.47f, 0.025f);  // unter der Nasenspitze
        // Lippen und Kinn
        R += 0.0072f * hp.lips * g(th, 0.0f, 0.27f) * g(ph, -0.565f, 0.042f);
        R -= 0.0048f * g(th, 0.0f, 0.3f) * g(ph, -0.635f, 0.017f);  // Mundspalte
        R += 0.0068f * hp.lips * g(th, 0.0f, 0.24f) * g(ph, -0.69f, 0.038f);
        R -= 0.0042f * g(th, 0.0f, 0.26f) * g(ph, -0.78f, 0.035f);   // Kinnfurche
        R += 0.0085f * g(th, 0.0f, 0.3f) * g(ph, -0.9f, 0.07f) * (hp.female ? 0.6f : 1.0f);
        // Mundwinkel leicht eingezogen
        R -= 0.0025f * g(sideTh, 0.3f, 0.06f) * g(ph, -0.635f, 0.04f);
        // Jochbeine, Wangen, Nasolabialfalten, Schlaefen
        R += 0.0075f * g(sideTh, 0.72f, 0.2f) * g(ph, -0.16f, 0.1f);
        R += (0.006f * hp.cheeks - 0.004f - 0.01f * hp.thin) * g(sideTh, 0.6f, 0.22f) * g(ph, -0.48f, 0.13f);
        R -= 0.0022f * g(sideTh, 0.34f, 0.05f) * g(ph, -0.5f, 0.12f);
        R -= (0.003f + 0.007f * hp.thin) * g(sideTh, 1.15f, 0.25f) * g(ph, 0.35f, 0.2f);
    } else {
        // glatt wie unter Stoff: nur ein Hauch von Nase und Augenhoehlen
        R += 0.008f * g(th, 0.0f, 0.12f) * g(ph, -0.28f, 0.16f);
        R -= 0.007f * g(ath, 0.42f, 0.2f) * g(ph, -0.02f, 0.1f);
        R -= (0.003f + 0.007f * hp.thin) * g(ath, 1.15f, 0.25f) * g(ph, 0.35f, 0.2f);
    }
    float dist = (r - R) * 0.88f;
    if (hp.features) {
        // Lider: eine Haut-Schale ueber dem Augapfel, die weich in die Augenhoehle uebergeht; die
        // mandelfoermige Lidspalte schneidet sie vor dem Augapfel auf (Lidrand ~2 mm dick).
        // Der aeussere Augenwinkel liegt etwas hoeher, das Oberlid woelbt sich staerker als das Unterlid.
        const float er = headEyeRadius();
        for (int side = 0; side < 2; ++side) {
            const vec3 e = q - headEyeCenter(side, hp);
            const float s = side == 0 ? 1.0f : -1.0f;
            float lid = length(e) - (er + 0.0021f);
            dist = sminF(dist, lid, 0.0045f);
            const float a = e.y * s;           // + zur Schlaefe
            const float b = e.z - 0.07f * a;   // Neigung der Lidachse
            const float w = 0.0117f;
            const float t = std::clamp(a / w, -1.0f, 1.0f);
            const float shape = std::max(0.0f, 1.0f - t * t);
            const float up = 0.0054f * hp.eyeOpen * std::pow(shape, 0.75f) * (1.0f - 0.12f * t);
            const float lo = 0.0041f * std::pow(shape, 0.85f) * (1.0f + 0.1f * t);
            if (!hp.openEyes) continue;
            float ap = std::max(std::max(b - up, -b - lo), std::fabs(a) - w);
            ap = std::max(ap, -e.x);
            dist = smaxF(dist, -ap, 0.0012f);
        }
    }
    return dist;
}

}  // namespace lim::gfx
