#include "gameplay/pawn.hpp"

#include <algorithm>
#include <cmath>

#include "anim/figures.hpp"
#include "gameplay/content.hpp"

namespace lim::game {

Pawn::Pawn(int id_, std::string name_, const DifficultyDef& diff, const Content* content)
    : id(id_), name(std::move(name_)), stats(diff), inventory(content) {}

// wie GameSession::applyBodyStyle (realistischer Koerper), ohne die Hand aus Blender: von aussen haelt die Hand des
// Skeletts die Lampe
void Pawn::setupBody() {
    if (bodyReady_) return;
    bodyReady_ = true;
    BodyProportions pr;
    const anim::Build& b = anim::figureBuild(anim::Figure::Player);
    const float hs = b.headScale * (0.6f + 0.4f * b.height / 1.78f);
    pr.thigh = b.thigh, pr.shin = b.shin, pr.ankle = b.ankle;
    pr.hip = b.ankle + (b.thigh + b.shin) * 0.975f;
    pr.eyeUp = 0.154f * hs, pr.eyeFwd = 0.1f * hs;
    pr.spine = (float)Player::kEye - pr.hip - pr.eyeUp;
    pr.neck = 0.11f * hs, pr.headR = 0.1f * hs;
    pr.upperArm = b.upperArm, pr.forearm = b.forearm;
    pr.hipW = b.hipW * 0.5f, pr.shoulderW = b.shoulderW * 0.5f;
    pr.footW = b.hipW * 0.5f + 0.02f * b.height;
    const float hand = b.hand > 0.0f ? b.hand : 0.104f * b.height;
    pr.gripFwd = hand * 0.45f - 0.027f;
    pr.gripOut = 0.0165f + 0.0135f;
    rig.init(anim::Figure::Player);
    body.setProportions(pr);
    body.setOutsideView(true);
    body.setViewmodelHand(false);
    body.reset();
    body.update(0.0, player);
}

void Pawn::updateBody(double dt) {
    setupBody();
    const bool lit = lampLit || (flash.on && flash.charge > 0.0);
    body.setFlashlight(lit, lit);
    body.update(dt, player);
}

void Pawn::addSample(const Sample& s) {
    if (nSamples == (int)samples.size()) {
        for (size_t i = 1; i < samples.size(); ++i) samples[i - 1] = samples[i];
        --nSamples;
    }
    samples[(size_t)nSamples++] = s;
}

void Pawn::interpolate(double t, double dt) {
    if (nSamples > 0) {
        const Sample* s0 = &samples[0];
        const Sample* s1 = s0;
        for (int i = 0; i < nSamples; ++i) {
            if (samples[(size_t)i].t <= t) s0 = &samples[(size_t)i];
            s1 = &samples[(size_t)i];
            if (samples[(size_t)i].t >= t) break;
        }
        const double span = s1->t - s0->t;
        const float k = span > 1e-4 ? (float)std::clamp((t - s0->t) / span, 0.0, 1.0) : 1.0f;
        const vec3 p = s0->p + (s1->p - s0->p) * k;
        const double vx = span > 1e-4 ? (s1->p.x - s0->p.x) / span : 0.0, vy = span > 1e-4 ? (s1->p.y - s0->p.y) / span : 0.0;
        player.x = p.x, player.y = p.y, player.z = p.z;
        player.vx = vx, player.vy = vy;
        player.angle = s0->yaw + std::remainder((double)s1->yaw - s0->yaw, 6.283185307179586) * k;
        player.pitch = s0->pitch + (s1->pitch - s0->pitch) * k;
        const float spd = s0->speed + (s1->speed - s0->speed) * k;
        const u8 f = s1->flags;
        player.groundSpeed = spd;
        player.sprinting = (f & 4) != 0;
        player.sneaking = (f & 8) != 0;
        player.onGround = (f & 16) != 0;
        player.swimming = (f & 32) != 0;
        player.gait = std::clamp(spd / 3.0, 0.0, 1.0);
        lampLit = (f & 1) != 0;
        alive = (f & 2) == 0;
        ready = (f & 64) != 0;
    }
    updateBody(dt);
}

void Pawn::collect(std::vector<VisibleMesh>& out, std::vector<mat4>& palette, vec3 eye, int detail) {
    if (!alive) return;
    setupBody();
    rig.setPose(playerJointPose(body, vec3{(float)player.x, (float)player.y, (float)player.z}));
    rig.hideHand(-1);
    rig.collect(out, palette, eye, detail, true, false, false);
    body.collectProps(out);
}

void Pawn::shadow(std::vector<ShadowCaster>& out) const {
    if (!alive || !rig.posed()) return;
    ShadowCaster s;
    rig.shadow(s);
    out.push_back(std::move(s));
}

bool Pawn::beam(vec3& pos, vec3& dir, float& power, double time) const {
    if (!alive) return false;
    const bool on = lampLit || (flash.on && flash.charge > 0.0);
    if (!on || !body.flashlightHeld()) return false;
    pos = body.flashlightPos();
    dir = body.flashlightDir();
    power = lampLit ? 1.0f : flash.output(time);
    return power > 0.01f;
}

}  // namespace lim::game
