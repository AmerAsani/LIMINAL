#include "gameplay/figure.hpp"
#include "gameplay/body.hpp"

#include <set>

#include "core/log.hpp"

#include <algorithm>
#include <cmath>

namespace lim::game {

using namespace anim;

// V10/V13.2: die Gelenke des Koerpers (Body) auf das Skelett der Spielerfigur. Frueher in GameSession::collectBody;
// eigene Funktion fuer Tests (B-001) und die Koerper der Mitspieler
anim::JointPose playerJointPose(const Body& body, vec3 root) {
    const BodyPose& bp = body.pose();
    const Build& b = figureBuild(Figure::Player);
    const JointPose bind = bindPose(b);
    JointPose jp = bind;
    // Skelettseite 0 liegt bei +L (= Body::right); im Koerper (Body) ist das Index 1
    const vec3 U{0, 0, 1}, F = bp.fwd, L = bp.right;
    const vec3 sd = normalize(bp.neck - bp.pelvis);
    const vec3 pelvis = bp.pelvis + sd * (0.018f * b.height);
    const float len = length(bp.neck - pelvis), total = b.spineLow + b.spineUp + b.neck;
    jp.fwd = F, jp.up = normalize(U * 0.7f + sd * 0.3f);
    jp.j[J_ROOT] = root;
    jp.j[J_PELVIS] = pelvis;
    jp.j[J_SPINE] = pelvis + sd * (len * b.spineLow / total);
    jp.j[J_CHEST] = jp.j[J_SPINE] + sd * (len * b.spineUp / total) + F * 0.007f;
    jp.j[J_NECK] = bp.neck;
    jp.chestFwd = normalize(orthoTo(F, sd));
    // Kopf
    const vec3 hf = bp.headFwd, hu = bp.headUp, hl = normalize(cross(hu, hf));
    jp.headFwd = hf, jp.headUp = hu;
    jp.j[J_HEAD] = bp.neck + normalize(sd * 0.55f + hu * 0.45f + hf * 0.18f) * length(bind.j[J_HEAD] - bind.j[J_NECK]);
    auto relHead = [&](Joint j) {
        vec3 o = bind.j[j] - bind.j[J_HEAD];
        return jp.j[J_HEAD] + hf * o.x + hl * o.y + hu * o.z;
    };
    jp.j[J_HEADTOP] = relHead(J_HEADTOP), jp.j[J_JAW] = relHead(J_JAW), jp.j[J_CHIN] = relHead(J_CHIN);
    const vec3 cu = sd, cf = jp.chestFwd, cl = normalize(cross(cu, cf));
    auto relChest = [&](Joint j) {
        vec3 o = bind.j[j] - bind.j[J_NECK];
        return bp.neck + cf * o.x + cl * o.y + cu * o.z;
    };
    for (int i = 0; i < 2; ++i) {
        const float sg = i == 0 ? 1.0f : -1.0f;
        const int bi = 1 - i;  // Index im Koerper (Body)
        jp.j[sideJoint(J_STERN_L, i)] = relChest(sideJoint(J_STERN_L, i));
        jp.j[sideJoint(J_SHOULDER_L, i)] = bp.shoulder[bi];
        jp.j[sideJoint(J_ELBOW_L, i)] = bp.elbow[bi];
        jp.j[sideJoint(J_WRIST_L, i)] = bp.hand[bi];
        vec3 mid = (bp.shoulder[bi] + bp.hand[bi]) * 0.5f, pole = bp.elbow[bi] - mid;
        jp.armPole[i] = length(pole) > 0.01f ? normalize(pole) : F * -1.0f;
        vec3 d = normalize(bp.hand[bi] - bp.elbow[bi]);
        vec3 palm = orthoTo(L * -sg, d);
        jp.palm[i] = length(palm) > 1e-3f ? normalize(palm) : normalize(orthoTo(F * -1.0f, d));
        const bool grip = (bi == 0 && body.flashlightHeld()) || (bi == 1 && body.holdsItem());
        jp.fingerCurl[i] = grip ? 0.85f : 0.42f;  // entspannt leicht gekruemmt
        jp.thumbCurl[i] = grip ? 0.75f : 0.3f;
        if (bi == 0 && body.flashWeight() > 0.001f && body.proportions().gripOut > 0.0f) {
            // V11: Faustgriff um die Taschenlampe - Handrichtung und Handflaeche kommen aus dem Koerper (dort sitzt
            // auch die Lampe), die Finger schliessen sich mit dem Heben
            const float w = body.flashWeight();
            d = body.flashHandDir();
            jp.palm[i] = body.flashPalm();
            jp.fingerCurl[i] = 0.42f + (0.84f - 0.42f) * w;  // ~90 Grad: Finger liegen vorn am Schaft
            jp.thumbCurl[i] = 0.3f + (0.55f - 0.3f) * w;
        }
        placeHand(jp, b, i, d);
        jp.j[legJoint(J_HIP_L, i)] = bp.hip[bi];
        jp.j[legJoint(J_KNEE_L, i)] = bp.knee[bi];
        jp.j[legJoint(J_ANKLE_L, i)] = bp.ankle[bi];
        vec3 lm = (bp.hip[bi] + bp.ankle[bi]) * 0.5f, lp = bp.knee[bi] - lm;
        jp.legPole[i] = length(lp) > 0.01f ? normalize(lp) : F;
        vec3 toe = bp.toe[bi] - bp.ankle[bi];
        vec3 ff = orthoTo(toe, U);
        ff = length(ff) > 1e-3f ? normalize(ff) : F;
        jp.footUp[i] = normalize(orthoTo(U, normalize(toe)) + U * 0.5f);
        placeFoot(jp, b, i, ff);
    }
    return jp;
}

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
    for (int l = 0; l < kFigureLods; ++l) modelNh_[l] = model_[l] + "_nh1", meshIdNh_[l] = -2;  // V13
    fpModelNh_ = fpModel_ + "_nh1", fpIdNh_ = -2;
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
    if (hiddenHand_ >= 0 && !creature_) {
        // V12: Hand, Finger und Daumen folgen starr dem Unterarm (sonst schwenkt das hintere Ende der Handgelenk-
        // Kapsel bei abgeknickter Hand durch den Aermel) und schrumpfen auf 2 % ums Handgelenk (nicht 0 - die
        // Normalen bleiben gueltig)
        // V13: der Punkt liegt 2 cm im Aermel (vom Handgelenk zum Ellbogen); die Haut der Hand fehlt im Netz ohnehin
        // (Variante _nh1), nur die LOD-2-Haut haengt noch an diesen Knochen
        const vec3 wj = pose_.j[anim::sideJoint(anim::J_WRIST_L, hiddenHand_)], ej = pose_.j[anim::sideJoint(anim::J_ELBOW_L, hiddenHand_)];
        const vec3 w = wj - normalize(wj - ej) * 0.02f - origin_;
        const mat4 fore = palette[off + (size_t)anim::sideBone(anim::B_FARM_L, hiddenHand_)];
        for (anim::Bone bn : {anim::B_HAND_L, anim::B_FING_L, anim::B_THUMB_L}) {
            mat4& m = palette[off + (size_t)anim::sideBone(bn, hiddenHand_)];
            m = fore;
            for (int c = 0; c < 3; ++c) m.c[c] = m.c[c] * 0.02f;
            m.c[3] = vec4(m.c[3].xyz() * 0.02f + w * 0.98f, 1.0f);
        }
    }
    VisibleMesh v;
    const bool nh = hiddenHand_ == 1 && !creature_;  // V13: Netz ohne diese Hand
    if (firstPerson && !creature_) {
        v.model = nh ? &fpModelNh_ : &fpModel_;
        v.meshId = nh ? &fpIdNh_ : &fpId_;
    } else {
        v.model = nh ? &modelNh_[lod] : &model_[lod];
        v.meshId = nh ? &meshIdNh_[lod] : &meshId_[lod];
    }
    v.world = mat4::translation(origin_);
    v.bones = (int)off;  // Index der ersten Matrix
    v.dynamic = dynamic;
    v.attached = attached;
    v.dualQuat = true;  // V13.2 (B-001): Dual Quaternions - die Rahmen sind starr (Menschen und Wesen), Gelenke klappen nicht ein
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
