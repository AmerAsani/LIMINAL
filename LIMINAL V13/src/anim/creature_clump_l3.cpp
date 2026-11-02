// V13.3 Level 3 (KREATUR_Clump.md, Abschnitt "Level 3"): Bewegung des grossen, bewaffneten Buendels - Skelett des Buendels,
// Modelle ent_clump_l3 (bewaffnet) und ent_clump_l3_bare (entwaffnet), um scale_ (1,5) vergroessert.
//
// Anders als das kleine Buendel (updateClump: jede Hand greift fuer sich, der Kern zieht ruckweise nach) ist es geordnet: vier
// Laufarme (0, 2, 4, 6) setzen im Takt zwei gegen zwei, der Kern gleitet ruhig. Die drei Waffenarme (1 Knochenspiess,
// 3 Rohr / Knochenspiess, 5 Knochensichel) tragen nicht - am Boden haelt es sie erhoben vor dem Kern wie eine Gottesanbeterin,
// an der Decke haengen sie herab und pendeln nach. An der Decke liegt der Kern dicht unter ihr, die Laufarme greifen nach
// oben, die Beine baumeln; das Maul zeigt nach unten.
// Eingaben (extra_): {Variante, Decke 0..1, Hieb 0..1, Waffenarm, Riss 0..1}. Hieb: 0-0,33 ausholen (hoch und zurueck),
// 0,33-0,65 Schlag nach vorn unten (dorthin, wohin es sich gedreht hat), danach zurueck. Riss: Arm 5 packt den Unterarm von
// Arm 3, beide zittern, dann reisst Arm 3 nach aussen (das Modell wechselt, presence_clump_l3.cpp).
// Haende werden im Bildraum (vergroessert) aufgesetzt, damit sie beim Gehen nicht rutschen; gerechnet wird im Skelettmass.
#include <algorithm>
#include <cmath>
#include <memory>

#include "anim/creature_anim.hpp"
#include "anim/skeleton.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kPiC = 3.14159265f;
vec3 nC(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
void spC(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
void spCV(vec3& x, vec3& v, vec3 t, float omega, float dt) {
    spC(x.x, v.x, t.x, omega, dt);
    spC(x.y, v.y, t.y, omega, dt);
    spC(x.z, v.z, t.z, omega, dt);
}
float smC(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 lerpC(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

struct BigAnim {
    bool init = false;
    float ceil = 1.0f, ceilV = 0.0f, tear = 0.0f, tearV = 0.0f, perc = 0.0f, percV = 0.0f, alert = 0.0f, alertV = 0.0f;
    float idle = 1.0f, idleV = 0.0f, shiver = 0.0f, shiverV = 0.0f, gait = 0.0f, strike = 0.0f;
    vec3 core, coreV;
    struct Grip {
        vec3 plant, from, to;  // Bildraum (vergroessert)
        bool swing = false;
        float t = 0.0f, dur = 0.3f;
    } g[cl::kArms];
    vec3 hold[cl::kArms], holdV[cl::kArms];  // Waffenarme: Ziel der Hand (Skelettmass) mit Nachschwingen
    vec3 foot[cl::kLegs], footV[cl::kLegs];
};
}  // namespace

void CreatureAnimator::updateClumpL3(float dt, const Input& in) {
    using namespace cl;
    if (!ext_) ext_ = std::make_shared<BigAnim>();
    BigAnim& S = *static_cast<BigAnim*>(ext_.get());
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const float s = std::max(0.3f, scale_);
    const float ceilIn = extra_.size() > 1 ? extra_[1] : 0.0f, strikeIn = extra_.size() > 2 ? extra_[2] : 0.0f;
    const float tearIn = extra_.size() > 4 ? extra_[4] : 0.0f;
    const int sArm = extra_.size() > 3 ? std::clamp((int)std::lround(extra_[3]), 1, 5) | 1 : 3;
    const vec3 F{std::cos(yaw_), std::sin(yaw_), 0.0f}, L{-F.y, F.x, 0.0f}, U{0, 0, 1};
    auto Mb = [&](vec3 v) { return F * v.x + L * v.y + U * v.z; };
    auto W = [&](vec3 v) { return pos_ + Mb(v); };
    auto vis = [&](vec3 p) { return pos_ + (p - pos_) * s; };    // Skelettmass -> Bild
    auto unvis = [&](vec3 p) { return pos_ + (p - pos_) / s; };  // Bild -> Skelettmass
    const float ceilL = std::min(in.clearance, 6.0f) / s;          // Decke ueber dem Boden (Skelettmass)
    spC(S.ceil, S.ceilV, ceilIn, ceilIn > S.ceil ? 2.2f : 5.0f, dt);
    spC(S.tear, S.tearV, tearIn, 6.0f, dt);
    spC(S.perc, S.percV, in.perceive, 2.5f, dt);
    spC(S.alert, S.alertV, in.alert, 3.0f, dt);
    spC(S.shiver, S.shiverV, in.shiver, 6.0f, dt);
    spC(S.idle, S.idleV, speed_ > 0.07f ? 0.0f : 1.0f, 2.5f, dt);
    S.strike = strikeIn;
    const float cs = smC(S.ceil);
    // Gang: zwei gegen zwei im Takt (0/4, dann 2/6); Schrittlaenge im Skelettmass
    const float vLocal = speed_ / s;
    const bool moving = speed_ > 0.07f;
    const float stride = 0.55f;
    if (moving) S.gait += dt * vLocal / stride * 0.5f;
    S.gait -= std::floor(S.gait);
    // Kern: ruhig gleitend; Atem; lauschend hoeher; beim Ausholen zurueckgelehnt
    const float breath = (0.018f + 0.02f * S.idle) * std::sin(time_ * 1.2f);
    const vec3 floorCore = W({0.03f - 0.08f * S.alert, 0.0f, kCoreZ * 1.12f + breath + 0.2f * S.perc});
    const vec3 ceilCore = W({0.0f, 0.0f, ceilL - 0.40f + 0.5f * breath});
    const vec3 want = lerpC(floorCore, ceilCore, cs);
    if (!S.init) {
        // Runde 1: der Anfang muss der Eingabe folgen (vorher stand er an der Decke - die Haende griffen am Boden nach oben)
        S.ceil = ceilIn, S.ceilV = 0.0f;
        S.core = want, S.coreV = {};
        for (int a = 0; a < kArms; ++a) {
            const vec3 d0 = Mb({std::cos(kArm[a].ang), std::sin(kArm[a].ang), 0.0f});
            const vec3 p = vec3{want.x, want.y, pos_.z + (cs > 0.5f ? ceilL : 0.0f)} + d0 * ((upperLen(a) + foreLen(a)) * 0.6f);
            S.g[a].plant = S.g[a].from = S.g[a].to = vis(p);
            S.hold[a] = S.holdV[a] = {};
        }
        for (int l = 0; l < kLegs; ++l) S.foot[l] = want - U * 0.3f, S.footV[l] = {};
        S.init = true;
    }
    spCV(S.core, S.coreV, want, 7.0f, dt);
    const float rock = 0.05f * std::sin(S.gait * 4.0f * kPiC) * (1.0f - S.idle) + 0.03f * std::sin(time_ * 0.8f);
    // Neigung: beim Ausholen zurueck, beim Schlag nach vorn; an der Decke liegt der Kern flach
    const float hit = S.strike > 0.33f && S.strike < 0.75f ? std::sin((S.strike - 0.33f) / 0.42f * kPiC) : 0.0f;
    const float pitch = (rock - 0.22f * S.alert + 0.35f * hit) * (1.0f - cs);
    auto Rc = [&](vec3 v) {  // um L kippen, dann wenig um F rollen
        const float c = std::cos(pitch), sn = std::sin(pitch);
        vec3 r{v.x * c - v.z * sn, v.y, v.x * sn + v.z * c};
        const float roll = 0.04f * std::sin(time_ * 0.7f + 1.0f) + 0.06f * S.shiver * std::sin(time_ * 31.0f);
        const float cr = std::cos(roll), sr = std::sin(roll);
        return vec3{r.x, r.y * cr - r.z * sr, r.y * sr + r.z * cr};
    };
    auto body = [&](int j) { return S.core + Mb(Rc(B[(size_t)j] - B[CORE])); };
    J[ROOT] = pos_;
    J[CORE] = S.core;
    J[TOP] = body(TOP);
    J[MOUTH] = body(MOUTH);
    const float surfZ = pos_.z + ceilL * cs;  // wo die Laufhaende greifen (Boden oder Decke)
    // --- Laufarme ------------------------------------------------------------------------------------------------------------
    static const int kWalk[4] = {0, 4, 2, 6};  // Gruppe A: 0, 4; Gruppe B: 2, 6
    int swinging = 0;
    for (int k = 0; k < 4; ++k) swinging += S.g[kWalk[k]].swing ? 1 : 0;
    for (int k = 0; k < 4; ++k) {
        const int a = kWalk[k];
        auto& g = S.g[a];
        const vec3 Sh = body(armJ(a, 0));
        const float lu = upperLen(a), lf = foreLen(a);
        const vec3 d0 = Mb({std::cos(kArm[a].ang), std::sin(kArm[a].ang), 0.0f});
        // Runde 2: der lange Arm 0 liegt am Boden fast gestreckt nach vorn (gefaltet stand er wie ein Spinnenbein 2 m hoch)
        const float reach = (lu + lf) * (a == 0 ? 0.88f - 0.3f * cs : 0.64f * (1.0f - 0.25f * cs)) + 0.12f * S.perc;
        const vec3 ideal = vec3{S.core.x, S.core.y, surfZ} + d0 * reach + F * std::min(0.3f, vLocal * 0.35f);
        const bool groupA = k < 2, myHalf = groupA ? S.gait < 0.5f : S.gait >= 0.5f;
        const vec3 plantL = unvis(g.plant);
        const float off = length(vec3{plantL.x - ideal.x, plantL.y - ideal.y, (plantL.z - ideal.z) * 0.5f});
        const bool wantStep = moving ? (myHalf && off > 0.1f) : (off > 0.32f && swinging < (off > 0.8f ? 2 : 1));
        if (!g.swing && wantStep) {
            g.swing = true;
            ++swinging;
            g.t = 0.0f;
            g.from = g.plant;
            g.to = vis(ideal + F * std::min(0.28f, vLocal * 0.3f));
            g.dur = moving ? std::clamp(0.5f * stride / std::max(0.2f, vLocal), 0.16f, 0.42f) : 0.35f;
        }
        if (g.swing) {
            g.t += dt / g.dur;
            if (g.t >= 1.0f) {
                g.swing = false;
                g.plant = g.to;
                stepped_ = true, stepPos_ = g.to, stepWeight_ = 0.7f + 0.3f * rnd();
            }
        }
        const float t = g.swing ? smC(g.t) : 0.0f;
        const vec3 lift = U * ((cs > 0.5f ? -0.18f : 0.22f) * std::sin(t * kPiC));
        vec3 target = g.swing ? lerpC(unvis(g.from), unvis(g.to), t) + lift : unvis(g.plant);
        // lauschend tippen die Finger
        const float tap = S.perc * std::max(0.0f, std::sin(time_ * (12.0f + 1.5f * (float)a) + (float)a));
        // Runde 2: Ellbogen nach aussen und nur wenig hoch (geordnet, nicht spinnenhaft); an der Decke nach aussen unten
        const vec3 pole = cs > 0.5f ? nC(-U * 0.6f + d0 * 0.8f, -U) : nC(d0 * 0.85f + U * (a == 0 ? 0.1f : 0.35f), U);
        vec3 E, Wr;
        ik2(Sh, target, lu, lf, pole, E, Wr);
        J[armJ(a, 0)] = Sh;
        J[armJ(a, 1)] = E;
        J[armJ(a, 2)] = Wr;
        vec3 hd = nC(vec3{Wr.x - Sh.x, Wr.y - Sh.y, 0.0f}, F);
        vec3 tip = Wr + hd * kHand;
        tip.z = cs > 0.5f ? std::min(surfZ - 0.02f, Wr.z + 0.03f) - 0.03f * tap : std::max(pos_.z + 0.02f, Wr.z - 0.03f) + 0.03f * tap;
        J[armJ(a, 3)] = tip;
    }
    // --- Waffenarme: erhoben (Boden) oder haengend (Decke); Hieb; Riss -------------------------------------------------------
    vec3 fore3a, fore3b;
    for (int a : {1, 3, 5}) {
        const vec3 Sh = body(armJ(a, 0));
        const float lu = upperLen(a), lf = foreLen(a);
        const float side = std::sin(kArm[a].ang) >= 0.0f ? 1.0f : -1.0f;
        const float sway = 0.04f * std::sin(time_ * (0.9f + 0.13f * (float)a) + (float)a);
        const vec3 raised = S.core + Mb({0.42f + 0.06f * (float)(a == 3), side * (0.28f + 0.05f * (float)(a == 5)), 0.42f + sway});
        const vec3 hang = Sh + Mb({0.08f + sway, side * 0.1f, -(lu + lf) * 0.8f});
        vec3 goal = lerpC(raised, hang, cs);
        vec3 pole = lerpC(nC(U * 0.8f - F * 0.4f + L * side * 0.4f, U), nC(F + L * side * 0.3f, F), cs);
        if (a == sArm && S.strike > 0.001f && cs < 0.5f) {  // Hieb
            const float p = S.strike;
            const vec3 high = S.core + Mb({-0.28f, side * 0.38f, 1.15f});
            const vec3 at = W({2.2f / s, side * 0.12f, 0.25f / s});
            if (p < 0.33f) goal = lerpC(raised, high, smC(p / 0.33f));
            else if (p < 0.65f) {
                const float k = (p - 0.33f) / 0.32f;
                goal = lerpC(high, at, k * k);
            } else {
                goal = lerpC(at, raised, smC((p - 0.65f) / 0.35f));
            }
            pole = nC(U - F * 0.5f + L * side * 0.5f, U);
        }
        if (S.tear > 0.01f && cs < 0.5f) {  // Riss: Arm 3 nach vorn gehalten, Arm 5 packt seinen Unterarm
            if (a == 3) {
                goal = lerpC(goal, S.core + Mb({0.55f, 0.02f, 0.4f}), smC(S.tear * 3.0f));
                if (S.tear > 0.62f) goal = goal + Mb({0.25f, -0.45f, 0.35f}) * smC((S.tear - 0.62f) / 0.38f);
                goal = goal + Mb({0.0f, 0.02f, 0.02f}) * (S.shiver * std::sin(time_ * 37.0f));
            }
            if (a == 5 && length(fore3b - fore3a) > 0.01f) {
                goal = lerpC(goal, lerpC(fore3a, fore3b, 0.55f), smC(S.tear * 2.5f) * (S.tear > 0.62f ? 1.0f - smC((S.tear - 0.62f) / 0.25f) : 1.0f));
                goal = goal + Mb({0.03f, 0.0f, 0.0f}) * (S.shiver * std::sin(time_ * 41.0f));
            }
        }
        // die Hand folgt mit Traegheit (an der Decke pendeln die Waffen nach)
        if (length(S.hold[a]) < 1e-4f) S.hold[a] = goal;
        spCV(S.hold[a], S.holdV[a], goal, a == sArm && S.strike > 0.33f ? 26.0f : (cs > 0.5f ? 3.0f : 7.0f), dt);
        vec3 E, Wr;
        ik2(Sh, S.hold[a], lu, lf, pole, E, Wr);
        J[armJ(a, 0)] = Sh;
        J[armJ(a, 1)] = E;
        J[armJ(a, 2)] = Wr;
        J[armJ(a, 3)] = Wr + nC(Wr - E, F) * kHand;
        if (a == 3) fore3a = E, fore3b = Wr;
    }
    // --- Beine: am Boden schleifen sie nach, an der Decke baumeln sie --------------------------------------------------------
    for (int l = 0; l < kLegs; ++l) {
        const float sg = l == 0 ? 1.0f : -1.0f;
        const vec3 H = body(legJ(l, 0));
        const vec3 drag = vec3{S.core.x, S.core.y, pos_.z + 0.06f} - F * 0.85f + L * (sg * 0.3f);
        const vec3 dang = H - U * (kThigh + kShin) * 0.85f - F * 0.15f + L * (sg * (0.1f + 0.05f * std::sin(time_ * 0.8f + (float)l)));
        spCV(S.foot[l], S.footV[l], lerpC(drag, dang, cs), cs > 0.5f ? 3.0f : 5.0f, dt);
        vec3 K, Fo;
        ik2(H, S.foot[l], kThigh, kShin, nC(U * (1.0f - 2.0f * cs) + L * (sg * 0.3f) + F * cs, U), K, Fo);
        J[legJ(l, 0)] = H;
        J[legJ(l, 1)] = K;
        J[legJ(l, 2)] = Fo;
    }
    autoUps();
    pose_.up[B_CORE] = Mb(Rc(sk_->bindUp[B_CORE]));
    // vergroessern (um die Wurzel am Boden)
    for (vec3& j : J) j = vis(j);
    pose_.s.assign(sk_->bones.size(), s);
}

}  // namespace lim::anim
