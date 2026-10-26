#include "gameplay/figure.hpp"

#include <algorithm>
#include <cmath>

namespace lim::game {

using namespace anim;

void FigureRig::init(Figure f) {
    creature_ = false;
    fig_ = f;
    b_ = figureBuild(f);
    height_ = b_.height;
    reach_ = b_.height * 0.75f;
    JointPose bind = bindPose(b_);
    Frames bf;
    computeFrames(bind, bf);
    bindInv_.resize(kBoneCount);
    for (int i = 0; i < kBoneCount; ++i) bindInv_[(size_t)i] = rigidInverse(bf[(size_t)i]);
    for (int l = 0; l < kFigureLods; ++l) {
        model_[l] = std::string(figureModel(f)) + "_l" + std::to_string(l);
        meshId_[l] = -2;
    }
    fpModel_ = std::string(figureModel(f)) + "_fp";
    fpId_ = -2;
    pose_ = bind;
    posed_ = false;
}

void FigureRig::initEntity(Entity e) {
    creature_ = true;
    ent_ = e;
    const CreatureSkeleton& sk = entitySkeleton(e);
    height_ = sk.height;
    reach_ = sk.reach;
    std::vector<mat4> bf;
    creatureFrames(sk, anim::bindPose(sk), bf);
    bindInv_.resize(bf.size());
    for (size_t i = 0; i < bf.size(); ++i) bindInv_[i] = rigidInverse(bf[i]);
    for (int l = 0; l < kFigureLods; ++l) {
        model_[l] = std::string(entityModel(e)) + "_l" + std::to_string(l);
        meshId_[l] = -2;
    }
    fpModel_ = model_[0];
    fpId_ = -2;
    cpose_ = anim::bindPose(sk);
    posed_ = false;
}

void FigureRig::setPose(const JointPose& p) {
    pose_ = p;
    Frames f;
    computeFrames(p, f);
    frames_.assign(f.begin(), f.end());
    origin_ = p.j[J_ROOT];
    posed_ = true;
}

void FigureRig::setCreaturePose(const CreaturePose& p) {
    cpose_ = p;
    creatureFrames(entitySkeleton(ent_), p, frames_);
    origin_ = p.j[0];
    posed_ = true;
}

vec3 FigureRig::center() const {
    if (!creature_) return pose_.j[J_PELVIS] + vec3{0, 0, 0.15f};
    vec3 s{};
    for (const vec3& j : cpose_.j) s += j;
    return cpose_.j.empty() ? origin_ : s / (float)cpose_.j.size();
}

void FigureRig::collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail, bool dynamic, bool attached,
                        bool firstPerson, int material) {
    if (!posed_) return;
    const vec3 c = center();
    const float d = length(c - eye);
    if (d > 70.0f && !attached) return;
    // Detailstufe nach Abstand und Einstellung (0 niedrig .. 3 ultra)
    static const float near0[4] = {0.0f, 2.5f, 6.0f, 13.0f};
    static const float near1[4] = {6.0f, 15.0f, 26.0f, 38.0f};
    // kleine Wesen (die Deathmoth) wechseln frueher auf grobe Stufen
    const float sz = creature_ ? std::clamp(reach_ / 1.1f, 0.3f, 1.0f) : 1.0f;
    int lod = d < near0[std::clamp(detail, 0, 3)] * sz ? 0 : (d < near1[std::clamp(detail, 0, 3)] * sz ? 1 : 2);
    if (attached) lod = detail >= 1 ? 0 : 1;
    const size_t off = palette.size();
    for (size_t b = 0; b < frames_.size() && b < bindInv_.size(); ++b) {
        mat4 m = frames_[b] * bindInv_[b];
        m.c[3] = vec4(m.c[3].xyz() - origin_, 1.0f);
        palette.push_back(m);
    }
    VisibleMesh v;
    if (firstPerson && !creature_) {
        v.model = &fpModel_;
        v.meshId = &fpId_;
    } else {
        v.model = &model_[lod];
        v.meshId = &meshId_[lod];
    }
    v.world = mat4::translation(origin_);
    v.bones = (int)off;  // Index der ersten Matrix
    v.dynamic = dynamic;
    v.attached = attached;
    v.cullCenter = c;
    v.cullRadius = attached ? 0.0f : reach_;
    v.material = material;
    v.highlight = glow_;
    out.push_back(v);
}

void FigureRig::shadow(ShadowCaster& s) const {
    s.center = center();
    s.radius = height_ * 0.7f;
    s.caps.clear();
    auto cap = [&](vec3 a, vec3 c, float r) {
        s.caps.push_back(vec4(a, r));
        s.caps.push_back(vec4(c, 0.0f));
    };
    if (creature_) {
        // je Knochen eine Kapsel (duenne Finger und Zehen nicht)
        const CreatureSkeleton& sk = entitySkeleton(ent_);
        for (const CBone& b : sk.bones)
            if (b.shadow && b.a != b.b && b.radius >= 0.015f) cap(cpose_.j[b.a], cpose_.j[b.b], std::min(b.radius, 0.45f));
        return;
    }
    const auto& j = pose_.j;
    const Build& b = b_;
    const float hs = b.headScale * (0.6f + 0.4f * b.height / 1.78f);
    cap(j[J_PELVIS], j[J_CHEST], std::max(b.waistW, b.hipsW) * 0.85f);
    cap(j[J_CHEST], j[J_NECK] - (j[J_NECK] - j[J_CHEST]) * 0.2f, b.chestW * 0.82f);
    cap(j[J_NECK], j[J_HEAD], b.neckR);
    cap(j[J_HEAD] + pose_.headFwd * (0.03f * hs), j[J_HEADTOP] - pose_.headUp * (0.07f * hs), 0.085f * hs);
    for (int i = 0; i < 2; ++i) {
        cap(j[sideJoint(J_SHOULDER_L, i)], j[sideJoint(J_ELBOW_L, i)], b.upperArmR * 1.1f);
        cap(j[sideJoint(J_ELBOW_L, i)], j[sideJoint(J_WRIST_L, i)], b.forearmR);
        cap(j[sideJoint(J_WRIST_L, i)], j[sideJoint(J_FINGERTIP_L, i)], 0.028f);
        cap(j[legJoint(J_HIP_L, i)], j[legJoint(J_KNEE_L, i)], b.thighR);
        cap(j[legJoint(J_KNEE_L, i)], j[legJoint(J_ANKLE_L, i)], b.calfR * 0.95f);
        cap(j[legJoint(J_ANKLE_L, i)], j[legJoint(J_TOETIP_L, i)], 0.04f);
    }
}

void packShadows(const std::vector<ShadowCaster>& casters, std::vector<vec4>& rows, int& figures) {
    rows.clear();
    figures = (int)std::min<size_t>(casters.size(), 16);
    size_t capRow = (size_t)figures * 2;
    for (int f = 0; f < figures; ++f) {
        const ShadowCaster& c = casters[(size_t)f];
        // Huelle: eine senkrechte Kapsel A..B mit Radius R um alle Kapseln (viel enger als eine
        // Kugel - so pruefen weniger Pixel die einzelnen Glieder)
        float zmin = 1e9f, zmax = -1e9f, R = 0.05f;
        for (size_t k = 0; k + 1 < c.caps.size(); k += 2) {
            const float r = c.caps[k].w;
            for (const vec4& e : {c.caps[k], c.caps[k + 1]}) {
                zmin = std::min(zmin, e.z - r), zmax = std::max(zmax, e.z + r);
                R = std::max(R, std::hypot(e.x - c.center.x, e.y - c.center.y) + r);
            }
        }
        if (c.caps.empty()) zmin = c.center.z, zmax = c.center.z;
        float az = zmin + R, bz = zmax - R;
        if (bz < az) az = bz = (zmin + zmax) * 0.5f, R = std::max(R, (zmax - zmin) * 0.5f);
        rows.push_back(vec4(c.center.x, c.center.y, az, R + 0.02f));
        rows.push_back({(float)(c.caps.size() / 2), c.player ? 1.0f : 0.0f, (float)capRow, bz});
        capRow += c.caps.size();
    }
    for (int f = 0; f < figures; ++f) rows.insert(rows.end(), casters[(size_t)f].caps.begin(), casters[(size_t)f].caps.end());
}

}  // namespace lim::game
