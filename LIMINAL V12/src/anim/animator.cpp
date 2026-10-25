#include "anim/animator.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace lim::anim {

namespace {
constexpr float kTau = 6.2831853f;
float wrapPi(float a) { return std::remainder(a, kTau); }
float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 dirYaw(float yaw) { return {std::cos(yaw), std::sin(yaw), 0.0f}; }
float approach(float v, float target, float maxStep) { return v + std::clamp(target - v, -maxStep, maxStep); }

std::array<Style, kFigureCount> makeStyles() {
    std::array<Style, kFigureCount> a{};
    a[(int)Figure::Player] = Style{};
    return a;
}
}  // namespace

const Style& figureStyle(Figure f) {
    static const std::array<Style, kFigureCount> styles = makeStyles();
    return styles[(size_t)f];
}

void Animator::init(Figure f, vec3 pos, float yaw) {
    fig_ = f;
    b_ = figureBuild(f);
    style_ = figureStyle(f);
    bind_ = bindPose(b_);
    pose_ = bind_;
    snap(pos, yaw);
}

vec3 Animator::restFoot(int i, vec3 pos, float yaw) const {
    const float sg = i == 0 ? 1.0f : -1.0f;
    vec3 L{-std::sin(yaw), std::cos(yaw), 0.0f};
    return vec3{pos.x, pos.y, pos.z} + L * (sg * (b_.hipW * 0.5f + 0.02f * b_.height) * style_.stanceWidth);
}

void Animator::snap(vec3 pos, float yaw) {
    pos_ = pos;
    yaw_ = yaw;
    vel_ = {};
    speed_ = 0.0f;
    phase_ = 0.0f;
    for (int i = 0; i < 2; ++i) {
        Foot& f = foot_[i];
        f.plant = f.from = f.to = restFoot(i, pos, yaw);
        f.yaw = f.fromYaw = f.toYaw = yaw + (i == 0 ? 1.0f : -1.0f) * style_.footOut;
        f.swing = false;
        f.t = 0.0f;
    }
    headYaw_ = 0.0f;
    headPitch_ = 0.0f;
    init_ = true;
    Input in;
    in.pos = pos;
    in.yaw = yaw;
    update(0.0, in);
}

void Animator::startSwing(int i, vec3 target, float yaw, float dur) {
    Foot& f = foot_[i];
    f.swing = true;
    f.from = f.plant;
    f.fromYaw = f.yaw;
    f.to = target;
    f.toYaw = yaw;
    f.t = 0.0f;
    f.dur = dur;
}

vec3 Animator::eyePos() const {
    const float hs = b_.headScale * (0.6f + 0.4f * b_.height / 1.78f);
    return pose_.j[J_HEAD] + pose_.headFwd * (0.1f * hs) + pose_.headUp * (0.044f * hs);
}

void Animator::update(double dtIn, const Input& in) {
    const float dt = (float)std::clamp(dtIn, 0.0, 0.1);
    const Style& st = style_;
    const Build& b = b_;
    stepped_ = false;
    time_ += dt;
    breathT_ += dt * st.breathRate * std::max(0.0f, in.breathMul);

    // --- Wurzel ------------------------------------------------------------------------------
    vec3 d = in.pos - pos_;
    d.z = 0.0f;
    if (length(d) > 1.5f) {  // versetzt: sofort dort stehen
        snap(in.pos, in.yaw);
        return;
    }
    if (dt > 0.0f) {
        vec3 v = d / dt;
        vel_ = vel_ + (v - vel_) * std::min(1.0f, dt * 8.0f);
    }
    pos_ = in.pos;
    yaw_ += wrapPi(in.yaw - yaw_) * std::min(1.0f, dt * 5.0f);
    speed_ = length(vel_);
    const vec3 F = dirYaw(yaw_), L{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const float leg = b.thigh + b.shin;
    const bool walking = speed_ > 0.12f;

    // --- Gangzyklus und Fuesse ---------------------------------------------------------------
    const float S = std::max(0.3f, (0.72f * leg + 0.3f * speed_) * st.stride);  // Strecke je Doppelschritt
    // Standanteil: Gehen ~62 %, schneller wird daraus Laufen mit Flugphase (sonst muesste das
    // Becken tief einknicken, um die lange Standstrecke zu ueberbruecken)
    const float duty = 0.62f - 0.26f * std::clamp((speed_ - 1.8f) / 2.2f, 0.0f, 1.0f);
    float rate = walking ? speed_ / S : 0.0f;
    if (st.limp > 0.0f && phase_ >= 0.5f) rate *= 1.0f + 0.6f * st.limp;  // auf dem schwachen Bein schneller weiter
    phase_ = phase_ + rate * dt;
    phase_ -= std::floor(phase_);
    const float T = rate > 1e-3f ? 1.0f / rate : 1.0f;
    for (int i = 0; i < 2; ++i) {
        Foot& f = foot_[i];
        const float sg = i == 0 ? 1.0f : -1.0f;
        const float footYaw = yaw_ + sg * st.footOut;
        if (walking) {
            float ph = phase_ + 0.5f * (float)i;
            ph -= std::floor(ph);
            bool wantSwing = ph > duty;
            vec3 side = L * (sg * (b.hipW * 0.5f + 0.02f * b.height) * st.stanceWidth);
            float tSw = (ph - duty) / (1.0f - duty);
            float remain = (1.0f - std::clamp(tSw, 0.0f, 1.0f)) * (1.0f - duty) * T;
            vec3 target = pos_ + vel_ * (remain + duty * T * 0.5f) + side;
            if (wantSwing && !f.swing) startSwing(i, target, footYaw, (1.0f - duty) * T);
            if (f.swing) {
                if (!wantSwing) {  // aufgesetzt - am zuletzt verfolgten Ziel (nicht neu zielen: die Phase
                                   // steht schon am Anfang des naechsten Zyklus)
                    f.swing = false;
                    f.plant = f.to;
                    f.yaw = f.toYaw;
                    stepped_ = true;
                    stepFoot_ = i;
                } else {
                    f.t = std::clamp(tSw, 0.0f, 1.0f);
                    if (f.t < 0.65f) f.to = target, f.toYaw = footYaw;  // Ziel nachfuehren
                }
            }
        } else {
            // Stehen: angefangene Schritte zu Ende, schiefe Fuesse mit kleinen Schritten nachsetzen
            if (f.swing) {
                f.t += dt / std::max(0.15f, f.dur);
                if (f.t >= 1.0f) {
                    f.swing = false;
                    f.plant = f.to;
                    f.yaw = f.toYaw;
                    stepped_ = true;
                    stepFoot_ = i;
                }
            } else if (!foot_[1 - i].swing) {
                vec3 rest = restFoot(i, pos_, yaw_);
                float off = length(vec3{f.plant.x - rest.x, f.plant.y - rest.y, 0.0f});
                float yawOff = std::fabs(wrapPi(f.yaw - footYaw));
                // der weiter entfernte Fuss zuerst
                vec3 restO = restFoot(1 - i, pos_, yaw_);
                float offO = length(vec3{foot_[1 - i].plant.x - restO.x, foot_[1 - i].plant.y - restO.y, 0.0f});
                if ((off > 0.11f || yawOff > 0.55f) && (off >= offO || offO < 0.11f)) startSwing(i, rest, footYaw, 0.42f / std::max(0.6f, st.stride));
            }
        }
    }

    // Fusspositionen dieses Bildes (Knoechel) und Neigung
    vec3 ankle[2], footFwd[2];
    float lift[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        Foot& f = foot_[i];
        vec3 p = f.plant;
        float fy = f.yaw, pitch = 0.0f;
        if (f.swing) {
            float t = smooth01(f.t);
            p = lerp(f.from, f.to, t);
            fy = f.fromYaw + wrapPi(f.toYaw - f.fromYaw) * t;
            float h = st.liftHeight * (0.05f + 0.035f * std::min(speed_, 2.0f)) * std::sin(f.t * kPi);
            p.z = lerp(f.from.z, f.to.z, t) + h;
            lift[i] = h;
            pitch = -0.4f * std::sin(f.t * kPi) * (1.0f - f.t) + 0.18f * smooth01((f.t - 0.6f) / 0.4f);
        } else if (walking) {
            // Ferse hebt sich am Ende des Stands
            float ph = phase_ + 0.5f * (float)i;
            ph -= std::floor(ph);
            pitch = -0.45f * smooth01((ph - (duty - 0.2f)) / 0.2f);
        }
        footFwd[i] = dirYaw(fy);
        vec3 side = normalize(cross(U, footFwd[i]));
        pose_.footUp[i] = rotateAround(U, side, -pitch);
        // Knoechel ueber dem Fuss; bei gehobener Ferse dreht der Fuss um den Ballen
        vec3 ballOff = footFwd[i] * (b.foot * 0.72f) - U * (b.ankle * 0.82f);
        vec3 a0 = p + U * b.ankle;
        if (pitch < 0.0f && !f.swing) {
            vec3 ballW = a0 + ballOff;
            vec3 rel = a0 - ballW;
            a0 = ballW + rotateAround(rel, side, -pitch);
        } else if (f.swing) {
            a0 = p + U * b.ankle;
        }
        ankle[i] = a0;
    }

    // --- Becken -------------------------------------------------------------------------------
    const JointPose& bind = bind_;
    const float standZ = bind.j[J_PELVIS].z;
    float sp = std::min(speed_, 2.2f);
    float bob = walking ? st.bob * (0.011f + 0.012f * sp) * (0.5f + 0.5f * std::cos(4.0f * kPi * phase_)) : 0.0f;
    float idle = (walking ? 0.3f : 1.0f) * st.idleSway * (0.013f * std::sin(time_ * kTau / 7.3f) + 0.005f * std::sin(time_ * kTau / 2.9f + 1.0f));
    float swayW = walking ? st.hipSway * 0.022f * std::sin(kTau * (phase_ - 0.06f)) : 0.0f;
    float limpDrop = (walking && phase_ >= 0.5f) ? st.limp * 0.022f * std::sin((phase_ - 0.5f) * 2.0f * kPi) : 0.0f;
    float pelvisZ = pos_.z + standZ - st.kneeBend - in.crouch * 0.32f * leg - bob - limpDrop;
    vec3 pelvis = vec3{pos_.x, pos_.y, pelvisZ} + L * (swayW + idle) + F * (0.02f * sp - 0.01f);
    // Beine nicht ueberstrecken: Becken notfalls absenken
    {
        float excess = 0.0f;
        for (int i = 0; i < 2; ++i) {
            const float sg = i == 0 ? 1.0f : -1.0f;
            vec3 hip = pelvis + L * (sg * b.hipW * 0.5f) - U * (0.018f * b.height);
            excess = std::max(excess, length(ankle[i] - hip) - leg * 0.985f);
        }
        if (excess > 0.0f) pelvis.z -= excess;
    }
    float pelvisYaw = walking ? 0.07f * std::min(sp, 1.6f) * std::sin(kTau * phase_) : 0.0f;
    const vec3 PF = rotateAround(F, U, pelvisYaw), PL = rotateAround(L, U, pelvisYaw);
    float pelvisRoll = (swayW * 2.2f + limpDrop * 4.0f);
    const vec3 PU = rotateAround(U, PF, -pelvisRoll);

    // --- Beine ----------------------------------------------------------------------------------
    for (int i = 0; i < 2; ++i) {
        const float sg = i == 0 ? 1.0f : -1.0f;
        vec3 hip = pelvis + PL * (sg * b.hipW * 0.5f) - PU * (0.018f * b.height);
        vec3 knee, an;
        vec3 pole = normalize(footFwd[i] + PL * (sg * 0.12f) + U * 0.05f);
        ik2(hip, ankle[i], b.thigh, b.shin, pole, knee, an);
        pose_.j[legJoint(J_HIP_L, i)] = hip;
        pose_.j[legJoint(J_KNEE_L, i)] = knee;
        pose_.j[legJoint(J_ANKLE_L, i)] = an;
        pose_.legPole[i] = pole;
        placeFoot(pose_, b, i, footFwd[i]);
    }
    (void)lift;

    // --- Rumpf ------------------------------------------------------------------------------------
    float lean = st.lean + 0.045f * sp;
    float chestYaw = -0.6f * pelvisYaw;
    float breath = std::sin(breathT_ * kTau) * st.breath * std::max(0.0f, in.breathMul);
    vec3 CF = rotateAround(F, U, chestYaw), CL = rotateAround(L, U, chestYaw);
    vec3 spineUp = normalize(U + F * std::tan(lean * 0.6f) + L * (in.leanSide * 0.6f - swayW * 1.2f));
    pose_.j[J_ROOT] = pos_;
    pose_.j[J_PELVIS] = pelvis;
    pose_.j[J_SPINE] = pelvis + spineUp * b.spineLow - F * 0.012f * b.height;
    vec3 chestUp = normalize(U + F * std::tan(lean) + L * (in.leanSide - swayW * 0.6f));
    pose_.j[J_CHEST] = pose_.j[J_SPINE] + chestUp * b.spineUp + CF * (0.004f * b.height) + chestUp * (0.003f * breath);
    vec3 neckDir = normalize(chestUp + CF * (st.slump * 6.0f + lean * 0.4f));
    pose_.j[J_NECK] = pose_.j[J_CHEST] + neckDir * b.neck - CF * (0.012f * b.height) + chestUp * (0.002f * breath);
    pose_.fwd = PF;
    pose_.up = PU;
    pose_.chestFwd = normalize(orthoTo(CF, chestUp));

    // --- Kopf -------------------------------------------------------------------------------------
    {
        float wantYaw = 0.0f, wantPitch = -st.headDrop;
        if (in.look) {
            vec3 toT = in.lookAt - (pose_.j[J_NECK] + U * 0.15f);
            float hor = std::hypot(toT.x, toT.y);
            wantYaw = wrapPi(std::atan2(toT.y, toT.x) - (yaw_ + chestYaw));
            wantPitch = std::atan2(toT.z, std::max(hor, 0.1f)) - st.headDrop * 0.4f;
        }
        wantYaw = std::clamp(wantYaw, -st.headMax, st.headMax);
        wantPitch = std::clamp(wantPitch, -0.9f, 0.6f);
        // gleichmaessig, mit weichem Anfahren und Abbremsen
        float maxStep = st.headSpeed * dt * std::clamp(std::fabs(wantYaw - headYaw_) * 2.0f + 0.15f, 0.15f, 1.0f);
        headYaw_ = approach(headYaw_, wantYaw, maxStep);
        headPitch_ = approach(headPitch_, wantPitch, st.headSpeed * 0.7f * dt * std::clamp(std::fabs(wantPitch - headPitch_) * 3.0f + 0.2f, 0.2f, 1.0f));
        vec3 hf = rotateAround(CF, U, headYaw_);
        vec3 hl = rotateAround(CL, U, headYaw_);
        hf = rotateAround(hf, hl, -headPitch_);
        vec3 hu = normalize(cross(hf, hl));
        hu = rotateAround(hu, hf, -(st.headTilt + in.headTiltAdd));
        hl = normalize(cross(hu, hf));
        pose_.headFwd = hf;
        pose_.headUp = hu;
        // Kopfpunkte relativ zum Kopf wie in der Ruhepose
        const vec3 bh = bind.j[J_HEAD];
        auto rel = [&](Joint j) {
            vec3 o = bind.j[j] - bh;
            return hf * o.x + hl * o.y + hu * o.z;
        };
        vec3 neckToHead = bind.j[J_HEAD] - bind.j[J_NECK];
        float nl = length(neckToHead);
        vec3 nd = normalize(neckDir * 0.55f + hu * 0.45f + hf * 0.18f);
        pose_.j[J_HEAD] = pose_.j[J_NECK] + nd * nl;
        pose_.j[J_HEADTOP] = pose_.j[J_HEAD] + rel(J_HEADTOP);
        pose_.j[J_JAW] = pose_.j[J_HEAD] + rel(J_JAW);
        pose_.j[J_CHIN] = pose_.j[J_HEAD] + rel(J_CHIN);
    }

    // --- Arme -----------------------------------------------------------------------------------
    {
        const vec3 bc = bind.j[J_NECK];
        const vec3 cu = chestUp;
        const vec3 cl = normalize(cross(cu, pose_.chestFwd));
        const vec3 cf = pose_.chestFwd;
        auto relChest = [&](Joint j) {
            vec3 o = bind.j[j] - bc;
            return pose_.j[J_NECK] + cf * o.x + cl * o.y + cu * o.z;
        };
        float swingA = st.armSwing * (0.08f + 0.22f * std::min(sp, 1.6f));
        for (int i = 0; i < 2; ++i) {
            const float sg = i == 0 ? 1.0f : -1.0f;
            vec3 sh = relChest(sideJoint(J_SHOULDER_L, i)) - cu * (st.slump + 0.003f * breath * -1.0f);
            pose_.j[sideJoint(J_STERN_L, i)] = relChest(sideJoint(J_STERN_L, i));
            pose_.j[sideJoint(J_SHOULDER_L, i)] = sh;
            float theta = st.armHang + (walking ? swingA * (i == 0 ? -1.0f : 1.0f) * std::cos(kTau * phase_) : 0.0f);
            theta += 0.02f * std::sin(time_ * 0.8f + (float)i * 2.0f) * st.idleSway;
            float abd = 0.12f + st.armOut + 0.03f * (float)walking;
            vec3 dir = normalize(cu * -std::cos(theta) + cf * std::sin(theta));
            dir = normalize(dir * std::cos(abd) + cl * (sg * std::sin(abd)));
            vec3 elbow = sh + dir * b.upperArm;
            float bend = st.elbowBend + (theta > 0.0f ? theta * 0.8f : 0.0f);
            vec3 perp = orthoTo(cf, dir);
            perp = length(perp) > 1e-4f ? normalize(perp) : cf;
            vec3 fdir = normalize(dir * std::cos(bend) + perp * std::sin(bend));
            vec3 wrist = elbow + fdir * b.forearm;
            vec3 pole = normalize(cf * -1.0f + cl * (sg * 0.25f));
            if (in.reach > 0.0f) {
                vec3 e2, w2;
                ik2(sh, lerp(wrist, in.reachAt + cl * (sg * 0.1f), smooth01(in.reach)), b.upperArm, b.forearm, normalize(pole - cu * 0.6f), e2, w2);
                elbow = e2, wrist = w2;
                fdir = normalize(wrist - elbow);
            }
            pose_.j[sideJoint(J_ELBOW_L, i)] = elbow;
            pose_.j[sideJoint(J_WRIST_L, i)] = wrist;
            pose_.armPole[i] = pole;
            vec3 out = normalize(orthoTo(cl * sg, fdir));
            pose_.palm[i] = normalize(out * -1.0f - cf * 0.25f);
            pose_.fingerCurl[i] = st.fingerCurl + 0.05f * std::sin(time_ * 0.5f + (float)i);
            pose_.thumbCurl[i] = 0.25f + st.fingerCurl * 0.4f;
            placeHand(pose_, b, i, normalize(fdir + pose_.palm[i] * 0.08f));
        }
    }
}

}  // namespace lim::anim
