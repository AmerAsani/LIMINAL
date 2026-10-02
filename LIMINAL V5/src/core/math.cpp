#include "core/math.hpp"

namespace lim {

mat4 inverse(const mat4& mat) {
    // Allgemeine 4x4-Inversion (Kofaktoren), Zeilenform fuer Lesbarkeit.
    float m[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) m[r * 4 + c] = mat.at(r, c);
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    float id = det != 0.0f ? 1.0f / det : 0.0f;
    return mat4::fromRows({inv[0] * id, inv[1] * id, inv[2] * id, inv[3] * id},
                          {inv[4] * id, inv[5] * id, inv[6] * id, inv[7] * id},
                          {inv[8] * id, inv[9] * id, inv[10] * id, inv[11] * id},
                          {inv[12] * id, inv[13] * id, inv[14] * id, inv[15] * id});
}

Frustum Frustum::fromViewProj(const mat4& vp) {
    // Gribb/Hartmann: Ebenen aus Zeilen der Matrix (Clip-Raum von D3D, umgekehrtes z).
    vec4 r0 = vp.row(0), r1 = vp.row(1), r2 = vp.row(2), r3 = vp.row(3);
    Frustum f;
    f.planes[0] = r3 + r0;
    f.planes[1] = r3 + r0 * -1.0f;
    f.planes[2] = r3 + r1;
    f.planes[3] = r3 + r1 * -1.0f;
    f.planes[4] = r3 + r2 * -1.0f;  // umgekehrtes z: z_clip <= w
    for (auto& p : f.planes) {
        float l = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        if (l > 0) p = p * (1.0f / l);
    }
    return f;
}

bool Frustum::intersects(const Aabb& b) const {
    for (const auto& p : planes) {
        vec3 v{p.x >= 0 ? b.hi.x : b.lo.x, p.y >= 0 ? b.hi.y : b.lo.y, p.z >= 0 ? b.hi.z : b.lo.z};
        if (p.x * v.x + p.y * v.y + p.z * v.z + p.w < 0) return false;
    }
    return true;
}

bool Frustum::intersectsSphere(vec3 c, float r) const {
    for (const auto& p : planes)
        if (p.x * c.x + p.y * c.y + p.z * c.z + p.w < -r) return false;
    return true;
}

}  // namespace lim
