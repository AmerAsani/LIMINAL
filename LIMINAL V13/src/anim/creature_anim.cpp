#include "anim/creature_anim.hpp"

#include <algorithm>
#include <cmath>

#include "anim/skeleton.hpp"

namespace lim::anim {

namespace {
constexpr float kPi = 3.14159265f, kTau = 6.2831853f;
float wrapPi(float a) { return std::remainder(a, kTau); }
float sm01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }

// kritisch gedaempfte Feder (implizit, fuer jedes dt stabil)
void spring(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
void springV(vec3& x, vec3& v, vec3 t, float omega, float dt) {
    spring(x.x, v.x, t.x, omega, dt);
    spring(x.y, v.y, t.y, omega, dt);
    spring(x.z, v.z, t.z, omega, dt);
}

// 3x3-Drehung (Spalten)
struct M3 {
    vec3 c0{1, 0, 0}, c1{0, 1, 0}, c2{0, 0, 1};
    vec3 operator*(vec3 v) const { return c0 * v.x + c1 * v.y + c2 * v.z; }
    M3 operator*(const M3& o) const { return M3{(*this) * o.c0, (*this) * o.c1, (*this) * o.c2}; }
};
M3 axisAngle(vec3 a, float ang) {
    return M3{rotateAround({1, 0, 0}, a, ang), rotateAround({0, 1, 0}, a, ang), rotateAround({0, 0, 1}, a, ang)};
}
// Drehung, die d0 auf d1 bringt, auf v anwenden (kleinster Bogen)
vec3 rotateArc(vec3 v, vec3 d0, vec3 d1) {
    vec3 ax = cross(d0, d1);
    float sn = length(ax), cs = dot(d0, d1);
    if (sn < 1e-6f) return v;
    return rotateAround(v, ax / sn, std::atan2(sn, cs));
}
vec3 safeNorm(vec3 v, vec3 fb) {
    float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
vec3 xf(const mat4& m, vec3 p) { return (m * vec4(p, 1.0f)).xyz(); }
vec3 xfDir(const mat4& m, vec3 d) { return (m * vec4(d, 0.0f)).xyz(); }
}  // namespace

float CreatureAnimator::rnd() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return (float)(rng_ & 0xFFFFFF) / 16777216.0f;
}

void CreatureAnimator::init(Entity e, vec3 pos, float yaw, unsigned seed) {
    e_ = e;
    sk_ = &entitySkeleton(e);
    rng_ = seed * 2654435761u + 0x9E3779B9u;
    if (rng_ == 0) rng_ = 1;
    pose_ = bindPose(*sk_);
    creatureFrames(*sk_, pose_, bindFrames_);
    legs_ = e == Entity::Hound ? 4 : 0;
    int chains = 0;
    switch (e) {
        case Entity::Smiler: chains = sm::kWisps; break;
        case Entity::Hound: chains = hd::kHeadHair + hd::kBackHair; break;
        case Entity::Gestalt: chains = ge::kWisps; break;
        default: break;
    }
    chain_.assign((size_t)chains, Chain{});
    for (int s = 0; s < 2; ++s)
        for (int f = 0; f < 5; ++f) curl_[s][f] = curlT_[s][f] = 0.04f + 0.1f * rnd();
    twitchT_ = 3.0f + 6.0f * rnd();
    blinkT_ = 2.0f + 4.0f * rnd();
    listenT_ = 3.0f + 5.0f * rnd();
    antT_ = 0.5f + rnd();
    flap_ = rnd();
    last_ = Input{};
    last_.pos = pos;
    last_.yaw = yaw;
    snap(pos, yaw);
}

void CreatureAnimator::snap(vec3 pos, float yaw) {
    pos_ = pos;
    yaw_ = yaw;
    vel_ = {};
    speed_ = 0.0f;
    phase_ = 0.0f;
    run_ = false;
    const auto& B = sk_->bind;
    const vec3 F = dirYaw(yaw), L{-F.y, F.x, 0.0f};
    auto restAt = [&](int j) { return pos + F * B[(size_t)j].x + L * B[(size_t)j].y; };
    if (e_ == Entity::Hound) {
        const int ids[4] = {hd::KNUCK_L, hd::KNUCK_R, hd::FOOT_L, hd::FOOT_R};
        for (int i = 0; i < 4; ++i) {
            Leg& l = leg_[i];
            l.plant = l.from = l.to = restAt(ids[i]);
            l.plant.z = l.from.z = l.to.z = pos.z;
            l.swing = false;
            l.t = 0.0f;
        }
    }
    if (e_ == Entity::Clump) {  // V11: Haende und Fuesse der Ruhepose, Kern ueber der Wurzel
        for (int a = 0; a < cl::kArms; ++a) {
            Grip& g = grip_[a];
            g.plant = g.from = g.to = restAt(cl::armJ(a, 2));
            g.plant.z = g.from.z = g.to.z = pos.z + 0.04f;
            g.swing = false;
            g.stray = 0.25f + 0.25f * rnd();
        }
        for (int l = 0; l < cl::kLegs; ++l) foot_[l] = restAt(cl::legJ(l, 2)) + vec3{0, 0, 0.06f};
        core_ = pos + vec3{0, 0, cl::kCoreZ}, coreV_ = {};
    }
    chainInit_ = false;
    init_ = true;
    Input in = last_;
    in.pos = pos;
    in.yaw = yaw;
    update(0.0, in);
}

void CreatureAnimator::setHidden(bool hidden) {
    hang_ = bait_ = hidden ? 1.0f : 0.0f;
    hangV_ = baitV_ = 0.0f;
    if (hidden) core_ = pos_ + vec3{0, 0, std::min(last_.clearance, 6.0f) + 0.5f}, coreV_ = {};
}

float CreatureAnimator::height() const {
    if (e_ == Entity::Hound) return sk_->height - crouch_ * 0.2f;
    return sk_->height;
}

vec3 CreatureAnimator::headPos() const {
    switch (e_) {
        case Entity::Smiler: return pose_.j[sm::FACE];
        case Entity::Hound: return pose_.j[hd::SKULL];
        case Entity::Moth: return pose_.j[mo::HEAD];
        case Entity::Gestalt: return pose_.j[ge::HEAD];
        case Entity::Clump: return pose_.j[cl::CORE];
        default: return pose_.j[ns::TOP];
    }
}

void CreatureAnimator::update(double dtIn, const Input& in) {
    const float dt = (float)std::clamp(dtIn, 0.0, 0.25);
    stepped_ = false;
    time_ += dt;
    const bool air = e_ == Entity::Moth;
    vec3 d = in.pos - pos_;
    if (!air) d.z = 0.0f;
    if (length(d) > (air ? 0.8f : 1.5f) && init_ && dt > 0.0f) {  // versetzt: sofort dort
        last_ = in;
        snap(in.pos, in.yaw);
        return;
    }
    if (dt > 0.0f) {
        vec3 v = d / dt;
        vel_ = vel_ + (v - vel_) * std::min(1.0f, dt * 6.0f);
    }
    pos_ = in.pos;
    const float turn = e_ == Entity::Hound ? 6.0f : (air ? 9.0f : (e_ == Entity::Smiler ? 1.2f : 0.5f));
    const float yawBefore = yaw_;
    yaw_ += wrapPi(in.yaw - yaw_) * std::min(1.0f, dt * turn);
    speed_ = air ? length(vel_) : std::hypot(vel_.x, vel_.y);
    switch (e_) {
        case Entity::Smiler: updateSmiler(dt, in); break;
        case Entity::Hound: updateHound(dt, in); break;
        case Entity::Moth: {
            // Querneigung aus der Drehgeschwindigkeit (in die Kurve legen)
            const float yawRate = dt > 0.0f ? wrapPi(yaw_ - yawBefore) / dt : 0.0f;
            spring(bank_, bankV_, std::clamp(yawRate * speed_ * 0.35f, -0.75f, 0.75f), 5.0f, dt);
            updateMoth(dt, in);
            break;
        }
        case Entity::Gestalt: updateGestalt(dt, in); break;
        case Entity::Clump: updateClump(dt, in); break;
        default: updateNest(); break;
    }
    last_ = in;
    chainInit_ = true;
}

// --- Bausteine -------------------------------------------------------------------------------
void CreatureAnimator::stepLegs(float dt, int n, const float* offs, float duty, float S, const vec3* rest, float standDur, const float* weight) {
    const bool walking = speed_ > 0.07f;
    const float rate = walking ? speed_ / std::max(0.2f, S) : 0.0f;
    phase_ += rate * dt;
    phase_ -= std::floor(phase_);
    const float T = rate > 1e-3f ? 1.0f / rate : 1.0f;
    bool anySwing = false;
    for (int i = 0; i < n; ++i) anySwing = anySwing || leg_[i].swing;
    auto land = [&](int i) {
        Leg& f = leg_[i];
        f.swing = false;
        f.plant = f.to;
        stepped_ = true;
        stepPos_ = f.plant;
        stepWeight_ = weight[i] * std::clamp(0.5f + speed_ * 0.25f, 0.5f, 1.4f);
    };
    for (int i = 0; i < n; ++i) {
        Leg& f = leg_[i];
        if (walking) {
            float ph = phase_ + offs[i];
            ph -= std::floor(ph);
            const bool want = ph > duty;
            const float tSw = (ph - duty) / (1.0f - duty);
            const float remain = (1.0f - std::clamp(tSw, 0.0f, 1.0f)) * (1.0f - duty) * T;
            vec3 target = rest[i] + vel_ * (remain + duty * T * 0.5f);
            target.z = pos_.z;
            if (want && !f.swing) {
                f.swing = true;
                f.from = f.plant;
                f.to = target;
                f.t = 0.0f;
                f.dur = (1.0f - duty) * T;
            }
            if (f.swing) {
                if (!want) {
                    land(i);
                } else {
                    f.t = std::clamp(tSw, 0.0f, 1.0f);
                    if (f.t < 0.7f) f.to = target;
                }
            }
        } else {
            if (f.swing) {
                f.t += dt / std::max(0.15f, std::min(f.dur, standDur));
                if (f.t >= 1.0f) land(i);
            } else if (!anySwing) {
                vec3 dd = f.plant - rest[i];
                dd.z = 0.0f;
                if (length(dd) > 0.12f) {
                    f.swing = true;
                    f.from = f.plant;
                    f.to = rest[i];
                    f.to.z = pos_.z;
                    f.t = 0.0f;
                    f.dur = standDur;
                    anySwing = true;
                }
            }
        }
    }
}

vec3 CreatureAnimator::footPos(int i, const float* lift) const {
    const Leg& f = leg_[i];
    if (!f.swing) return f.plant;
    float t = sm01(f.t);
    vec3 p = f.from + (f.to - f.from) * t;
    p.z = f.from.z + (f.to.z - f.from.z) * t + lift[i] * std::sin(kPi * std::clamp(f.t, 0.0f, 1.0f));
    return p;
}

void CreatureAnimator::initChain(Chain& c, const std::vector<vec3>& pts) {
    c.p = pts;
    c.prev = pts;
    c.len.assign(pts.size(), 0.0f);
    for (size_t i = 1; i < pts.size(); ++i) c.len[i] = length(pts[i] - pts[i - 1]);
}

// Kette mit Traegheit: haengt an root, zieht zu den Zielpunkten (Steifigkeit), Laengen bleiben
void CreatureAnimator::simChain(Chain& c, vec3 root, const std::vector<vec3>& target, float stiff, float damp, float gravity, float dt) {
    if (c.p.size() != target.size()) initChain(c, target);
    const int n = (int)c.p.size();
    if (dt > 0.0f) {
        const int sub = std::max(1, (int)std::ceil(dt / (1.0f / 90.0f)));
        const float h = dt / (float)sub;
        const float keep = std::pow(damp, h * 60.0f);
        for (int s = 0; s < sub; ++s) {
            c.p[0] = c.prev[0] = root;
            for (int i = 1; i < n; ++i) {
                vec3 v = (c.p[(size_t)i] - c.prev[(size_t)i]) * keep;
                c.prev[(size_t)i] = c.p[(size_t)i];
                vec3 acc = (target[(size_t)i] - c.p[(size_t)i]) * stiff - vec3{0, 0, gravity};
                c.p[(size_t)i] += v + acc * (h * h);
            }
            for (int i = 1; i < n; ++i) {  // Laengen (von der Wurzel aus), dann aus dem Koerper heraus
                vec3 dd = c.p[(size_t)i] - c.p[(size_t)i - 1];
                float l = length(dd);
                if (l > 1e-6f) c.p[(size_t)i] = c.p[(size_t)i - 1] + dd * (c.len[(size_t)i] / l);
                for (const Cap& k : cols_) {
                    const vec3 ab = k.b - k.a;
                    const float t = std::clamp(dot(c.p[(size_t)i] - k.a, ab) / std::max(dot(ab, ab), 1e-9f), 0.0f, 1.0f);
                    const vec3 q = k.a + ab * t, dq = c.p[(size_t)i] - q;
                    const float dl = length(dq);
                    if (dl < k.r && dl > 1e-6f) c.p[(size_t)i] = q + dq * (k.r / dl);
                }
                if (e_ != Entity::Moth) c.p[(size_t)i].z = std::max(c.p[(size_t)i].z, pos_.z + 0.012f);
            }
        }
    }
    c.p[0] = root;
}

mat4 CreatureAnimator::boneNow(int b) const {
    const CBone& B = sk_->bones[(size_t)b];
    if (B.a == B.b) {
        vec3 f = pose_.up[(size_t)b];
        f.z = 0.0f;
        f = safeNorm(f, {1, 0, 0});
        return boneFrame(pose_.j[B.a], pose_.j[B.a] + f, vec3{0, 0, 1});
    }
    return boneFrame(pose_.j[B.a], pose_.j[B.b], pose_.up[(size_t)b]);
}

void CreatureAnimator::hangChain(int chain, int parent, const int* idx, int n, float stiff, float damp, float gravity, vec3 wind, float dt) {
    const auto& B = sk_->bind;
    const mat4 rel = boneNow(parent) * rigidInverse(bindFrames_[(size_t)parent]);
    std::vector<vec3> tgt((size_t)n);
    for (int k = 0; k < n; ++k) tgt[(size_t)k] = xf(rel, B[(size_t)idx[k]]) + wind * ((float)k / (float)std::max(1, n - 1));
    Chain& c = chain_[(size_t)chain];
    if (!chainInit_ || c.p.size() != (size_t)n) initChain(c, tgt);
    simChain(c, tgt[0], tgt, stiff, damp, gravity, dt);
    for (int k = 0; k < n; ++k) pose_.j[(size_t)idx[k]] = c.p[(size_t)k];
}

void CreatureAnimator::autoUps() {
    const float c = std::cos(yaw_), s = std::sin(yaw_);
    auto rotYaw = [&](vec3 v) { return vec3{v.x * c - v.y * s, v.x * s + v.y * c, v.z}; };
    const auto& B = sk_->bind;
    for (size_t i = 0; i < sk_->bones.size(); ++i) {
        const CBone& b = sk_->bones[i];
        if (b.a == b.b) {
            pose_.up[i] = dirYaw(yaw_);
            continue;
        }
        vec3 d0 = rotYaw(safeNorm(B[b.b] - B[b.a], {1, 0, 0}));
        vec3 d1 = safeNorm(pose_.j[b.b] - pose_.j[b.a], d0);
        pose_.up[i] = rotateArc(rotYaw(sk_->bindUp[i]), d0, d1);
    }
}

// V11: autoUps kennt je Knochen nur Ruhe- und jetzige Richtung - dreht sich der Kopf, verdrehen sich die
// Haarknochen dabei gegeneinander, und eine breite Straehne reisst an den Gelenken in Sprossen. Hier
// dreht die Ruhe-Oben-Richtung mit dem Elternknochen mit (Paralleltransport) und folgt dann auf
// kuerzestem Weg der jetzigen Richtung.
void CreatureAnimator::chainUps(int parent, const int* bones, int n) {
    int par = parent;
    for (int k = 0; k < n; ++k) {
        const int b = bones[k];
        const CBone& B = sk_->bones[(size_t)b];
        const mat4 rel = boneNow(par) * rigidInverse(bindFrames_[(size_t)par]);
        const vec3 d0 = safeNorm(xfDir(rel, sk_->bind[B.b] - sk_->bind[B.a]), {1, 0, 0});
        const vec3 d1 = safeNorm(pose_.j[B.b] - pose_.j[B.a], d0);
        pose_.up[(size_t)b] = rotateArc(xfDir(rel, sk_->bindUp[(size_t)b]), d0, d1);
        par = b;
    }
}

// --- Smiler ----------------------------------------------------------------------------------
void CreatureAnimator::updateSmiler(float dt, const Input& in) {
    using namespace sm;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    auto W = [&](vec3 l) { return pos_ + Mb * l; };
    spring(jaw_, jawV_, in.jaw, 0.9f, dt);  // das Laecheln oeffnet sich langsam
    spring(tilt_, tiltV_, in.tilt, 0.8f, dt);
    // schwebt; die obere Masse zieht beim Gleiten nach
    const vec3 hover = U * (0.016f * std::sin(time_ * 0.7f) + 0.007f * std::sin(time_ * 1.9f + 1.0f));
    const vec3 upperRigid = W(B[UPPER]) + hover;
    if (!chainInit_) mass_ = upperRigid, massV_ = {};
    springV(mass_, massV_, upperRigid, 2.2f, dt);
    vec3 lag = mass_ - upperRigid;
    if (length(lag) > 0.25f) lag = normalize(lag) * 0.25f;
    const float sw1 = 0.03f * std::sin(time_ * 0.53f), sw2 = 0.025f * std::sin(time_ * 0.37f + 2.0f);
    J[ROOT] = pos_;
    J[BASE] = W(B[BASE]) + lag * 0.15f + Lw * (sw2 * 0.5f);
    J[CORE] = W(B[CORE]) + lag * 0.5f + Lw * sw1 + F * (sw2 * 0.5f) + hover * 0.5f;
    J[UPPER] = upperRigid + lag + Lw * (sw1 * 0.6f) + F * (sw2 * 0.4f);
    // Gesicht: dreht sich langsam zu dir - weiter, als ein Kopf es koennte
    float wantYaw = 0.0f, wantPitch = 0.0f;
    if (in.look) {
        const vec3 dl = in.lookAt - J[UPPER];
        const float lx = dot(dl, F), ly = dot(dl, Lw);
        wantYaw = std::clamp(std::atan2(ly, lx), -1.5f, 1.5f);
        wantPitch = std::clamp(std::atan2(dl.z - 0.25f, std::hypot(lx, ly)), -0.55f, 0.45f);
    }
    spring(headYaw_, headYawV_, wantYaw, 1.0f, dt);
    spring(headPitch_, headPitchV_, wantPitch, 1.0f, dt);
    const M3 R = axisAngle({0, 0, 1}, headYaw_) * axisAngle({0, 1, 0}, -headPitch_) * axisAngle({1, 0, 0}, tilt_);
    // V11: die Zaehne zittern kaum merklich (der Unterkiefer bebt)
    const float tremble = 0.0035f * std::sin(time_ * 37.0f) + 0.0025f * std::sin(time_ * 53.0f + 1.0f);
    const M3 Rj = R * axisAngle({0, 1, 0}, jaw_ * 0.17f + tremble);
    J[FACE] = J[UPPER] + Mb * (R * (B[FACE] - B[UPPER]));
    J[FACETIP] = J[UPPER] + Mb * (R * (B[FACETIP] - B[UPPER]));
    J[JAWH] = J[UPPER] + Mb * (R * (B[JAWH] - B[UPPER]));
    J[JAWTIP] = J[JAWH] + Mb * (Rj * (B[JAWTIP] - B[JAWH]));
    for (int j : {LIDH_L, LIDT_L, LIDH_R, LIDT_R}) J[j] = J[UPPER] + Mb * (R * (B[j] - B[UPPER]));
    // V11: Lider - schliessen von oben und unten zugleich. Unregelmaessiger Lidschlag (schnell zu, kurz
    // halten, langsamer auf); je weiter das Laecheln aufgeht, desto enger werden die Augen.
    blinkT_ -= dt;
    if (blinkT_ <= 0.0f && blinkPh_ < 0.0f) blinkPh_ = 0.0f, blinkT_ = 2.5f + 4.5f * rnd();
    float blink = 0.0f;
    if (blinkPh_ >= 0.0f) {
        blinkPh_ += dt;
        const float p = blinkPh_;
        blink = p < 0.09f ? p / 0.09f : (p < 0.14f ? 1.0f : 1.0f - (p - 0.14f) / 0.2f);
        if (p >= 0.34f) blinkPh_ = -1.0f, blink = 0.0f;
    }
    lids_ = std::clamp(std::max(blink, 0.12f + 0.38f * jaw_), 0.0f, 1.0f);
    auto setUps = [&]() {
        pose_.up[sm::B_FACE] = Mb * (R * sk_->bindUp[sm::B_FACE]);
        pose_.up[sm::B_JAW] = Mb * (Rj * sk_->bindUp[sm::B_JAW]);
        // Lider: um die Achse ihres Auges (Ruhepose offen); oben nach unten, unten nach oben
        for (int sd = 0; sd < 2; ++sd) {
            const int h = sd == 0 ? LIDH_L : LIDH_R, bu = sd == 0 ? B_LIDU_L : B_LIDU_R, bd = sd == 0 ? B_LIDD_L : B_LIDD_R;
            const vec3 ax = normalize(B[h + 1] - B[h]);
            pose_.up[bu] = Mb * (R * (axisAngle(ax, lids_ * kLidOpen) * sk_->bindUp[bu]));
            pose_.up[bd] = Mb * (R * (axisAngle(ax, -lids_ * kLidOpen) * sk_->bindUp[bd]));
        }
    };
    autoUps();
    setUps();
    // Zungen: steigen wie Rauch, treiben im Luftzug, ziehen beim Gleiten nach hinten
    cols_.clear();
    for (int w = 0; w < kWisps; ++w) {
        const int idx[4] = {wisp(w, 0), wisp(w, 1), wisp(w, 2), wisp(w, 3)};
        const float fw = (float)w;
        const vec3 wind = vec3{0.06f * std::sin(time_ * 1.3f + fw * 1.7f), 0.06f * std::cos(time_ * 1.1f + fw * 2.3f), 0.03f * std::sin(time_ * 2.1f + fw)} - lag * 0.8f;
        hangChain(w, sm::B_MID, idx, 4, 7.0f, 0.88f, -1.2f, wind, dt);
    }
    autoUps();
    setUps();
}

// --- Buendel (V11) ---------------------------------------------------------------------------
// Am Boden zieht es sich Hand ueber Hand: die Arme greifen ringsum, eine Hand nach der anderen setzt neu
// auf (unregelmaessig, ruckartig), der Kern folgt in Stoessen und schaukelt, die Beine schleifen nach.
// Verborgen: der Kern liegt ueber der Decke, nur der Koederarm haengt heraus, die Finger zucken.
// Ausholen: der lange Arm hebt sich hoch hinter den Kern; Griff: er schnellt zum Ziel.
void CreatureAnimator::updateClump(float dt, const Input& in) {
    using namespace cl;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), L{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, L, U};
    spring(hang_, hangV_, in.hang, 2.4f, dt);
    spring(bait_, baitV_, in.bait, 2.0f, dt);
    spring(windup_, windupV_, in.alert, 3.2f, dt);
    spring(grab_, grabV_, in.reach, 10.0f, dt);
    spring(grope_, gropeV_, in.shiver, 2.0f, dt);
    const float floorZ = pos_.z, ceilZ = pos_.z + std::min(in.clearance, 6.0f);
    // Kern: am Boden ruckweise (Stoesse, waehrend die Haende ziehen), schaukelnd; in der Decke verborgen
    pull_ += (speed_ * 2.2f + 0.15f) * dt;
    const float pulse = std::pow(std::max(0.0f, std::sin(pull_ * 6.2832f)), 2.0f);
    const vec3 floorCore = pos_ + U * (kCoreZ + 0.02f * std::sin(time_ * 1.4f));
    const vec3 hideCore = vec3{pos_.x, pos_.y, ceilZ + 0.5f};
    const vec3 want = floorCore + (hideCore - floorCore) * sm01(hang_);
    springV(core_, coreV_, want, 2.0f + 10.0f * pulse + 6.0f * hang_, dt);
    const vec3 lag = core_ - want;
    if (length(lag) > 0.35f) core_ = want + normalize(lag) * 0.35f;
    // aufbaeumen beim Griff (das Maul nach vorn), sonst schaukeln
    const float rock = 0.14f * std::sin(pull_ * 3.1f) * (1.0f - hang_) + 0.05f * std::sin(time_ * 0.9f);
    const M3 Rc = axisAngle(L, rock - grab_ * 1.25f - windup_ * 0.25f) * axisAngle(F, 0.08f * std::sin(pull_ * 2.3f + 1.0f));
    auto body = [&](int j) { return core_ + Rc * (Mb * (B[(size_t)j] - B[CORE])); };
    J[ROOT] = pos_;
    J[CORE] = core_;
    J[TOP] = body(TOP);
    J[MOUTH] = body(MOUTH);
    int swinging = 0;
    for (const Grip& o : grip_) swinging += o.swing ? 1 : 0;
    for (int a = 0; a < kArms; ++a) {
        Grip& g = grip_[a];
        const vec3 S = body(armJ(a, 0));
        const float lu = upperLen(a), lf = foreLen(a);
        const vec3 d0 = Mb * vec3{std::cos(kArm[a].ang), std::sin(kArm[a].ang), 0.0f};
        vec3 target, pole = U + d0 * 0.4f, handDir;
        bool flatHand = true;
        if (a == 0 && (windup_ > 0.05f || grab_ > 0.05f)) {
            // ausholen: hoch hinter den Kern; greifen: zum Ziel
            const vec3 up = core_ + U * 1.45f - F * 0.55f + L * 0.25f;
            target = up + (in.reachAt - up) * std::clamp(grab_, 0.0f, 1.0f);
            target = S + (target - S) * std::min(1.0f, (lu + lf) * 0.98f / std::max(0.01f, length(target - S)));
            pole = U - F * 0.6f;
            handDir = normalize(target - S);
            flatHand = false;
        } else if (hang_ > 0.5f) {
            if (a == 0) {  // Koeder: haengt aus der Decke, schwingt leise, die Finger zucken
                const float drop = (lu + lf) * (0.15f + 0.8f * sm01(bait_));
                target = S - U * drop + F * (0.06f * std::sin(time_ * 0.7f)) + L * (0.05f * std::sin(time_ * 0.53f + 1.0f));
                pole = F;
                handDir = vec3{0, 0, -1};
            } else {  // die uebrigen Arme liegen eingerollt um den Kern
                target = core_ + (S - core_) * 1.6f + U * 0.15f;
                handDir = normalize(target - S);
            }
            flatHand = false;
            g.plant = g.from = g.to = vec3{target.x, target.y, floorZ + 0.04f};
            g.swing = false;
        } else {
            // greifen ringsum: Ruheort der Hand, beim Kriechen etwas voraus; tastend: suchend hin und her
            const float sweep = grope_ * 0.7f * std::sin(time_ * 1.6f + (float)a * 2.1f);
            const vec3 dr = Mb * vec3{std::cos(kArm[a].ang + sweep), std::sin(kArm[a].ang + sweep), 0.0f};
            const float reach = (lu + lf) * (0.55f + 0.2f * grope_ * (0.5f + 0.5f * std::sin(time_ * 1.1f + (float)a)));
            const vec3 ideal = vec3{core_.x, core_.y, floorZ + 0.04f} + dr * reach + F * std::min(0.5f, speed_ * 0.4f);
            if (!g.swing) {
                const float stray = g.stray * (1.0f - 0.5f * grope_);
                if (length(vec3{g.plant.x - ideal.x, g.plant.y - ideal.y, 0.0f}) > stray && swinging < 2) {
                    g.swing = true;
                    ++swinging;
                    g.t = 0.0f;
                    g.from = g.plant;
                    g.to = ideal + F * std::min(0.35f, speed_ * 0.3f) + vec3{0.08f * (rnd() - 0.5f), 0.08f * (rnd() - 0.5f), 0.0f};
                    g.to.z = floorZ + 0.04f;
                    g.dur = 0.2f + 0.18f * rnd();
                    g.stray = 0.22f + 0.3f * rnd();
                }
            }
            if (g.swing) {
                g.t += dt / g.dur;
                if (g.t >= 1.0f) {
                    g.swing = false;
                    --swinging;
                    g.plant = g.to;
                    stepped_ = true, stepPos_ = g.to, stepWeight_ = 0.4f + 0.4f * rnd();
                }
            }
            const float t = g.swing ? sm01(g.t) : 0.0f;
            target = g.swing ? g.from + (g.to - g.from) * t + U * (0.3f * std::sin(t * 3.1416f)) : g.plant;
            handDir = safeNorm(vec3{target.x - S.x, target.y - S.y, 0.0f}, F);
        }
        vec3 E, Wr;
        ik2(S, target, lu, lf, pole, E, Wr);
        J[armJ(a, 0)] = S;
        J[armJ(a, 1)] = E;
        J[armJ(a, 2)] = Wr;
        // Finger: flach auf dem Boden, sonst in Armrichtung; sie zucken (Koeder, Tasten)
        const float twitch = (a == 0 ? 0.6f : 0.15f + 0.5f * grope_) * std::sin(time_ * (9.0f + (float)a)) * std::sin(time_ * 2.3f + (float)a);
        vec3 tip = Wr + handDir * kHand + L * (0.03f * twitch);
        if (flatHand) tip.z = std::max(floorZ + 0.02f, Wr.z - 0.03f);
        J[armJ(a, 3)] = tip;
    }
    // Beine: schleifen hinten nach; verborgen eingezogen
    for (int l = 0; l < kLegs; ++l) {
        const float sg = l == 0 ? 1.0f : -1.0f;
        const vec3 H = body(legJ(l, 0));
        vec3 drag = vec3{core_.x, core_.y, floorZ + 0.06f} - F * 0.9f + L * (sg * 0.32f);
        if (hang_ > 0.5f) drag = H - F * 0.35f + U * 0.1f + L * (sg * 0.1f);
        foot_[l] = foot_[l] + (drag - foot_[l]) * std::min(1.0f, dt * (hang_ > 0.5f ? 6.0f : 2.2f));
        vec3 K, Fo;
        ik2(H, foot_[l], kThigh, kShin, U + L * (sg * 0.3f), K, Fo);
        J[legJ(l, 0)] = H;
        J[legJ(l, 1)] = K;
        J[legJ(l, 2)] = Fo;
    }
    autoUps();
    pose_.up[B_CORE] = Rc * (Mb * sk_->bindUp[B_CORE]);
}

// --- Hound -----------------------------------------------------------------------------------
void CreatureAnimator::updateHound(float dt, const Input& in) {
    using namespace hd;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    auto W = [&](vec3 l) { return pos_ + Mb * l; };
    spring(lean_, leanV_, in.lean, 3.0f, dt);
    spring(jaw_, jawV_, in.jaw, 7.0f, dt);
    spring(reach_, reachV_, in.reach, 6.0f, dt);
    spring(tilt_, tiltV_, in.tilt, 3.0f, dt);
    spring(crouch_, crouchV_, in.crouch, 2.2f, dt);
    spring(growl_, growlV_, in.growl, 3.0f, dt);
    // V13.2: Licht (geblendet: zurueckweichen, Kopf weg und tief), Wahrnehmen (wittern), Ruhe
    spring(light_, lightV_, in.light, 4.0f, dt);
    spring(perc_, percV_, in.perceive, 3.0f, dt);
    spring(idleW_, idleWV_, speed_ > 0.07f ? 0.0f : 1.0f, 2.5f, dt);
    const float crouchE = std::clamp(crouch_ + 0.35f * light_, 0.0f, 1.0f);
    // Gangart: Kreuzgang - ab ~2,2 m/s Galopp mit Flugphase
    if (!run_ && speed_ > 2.2f) run_ = true;
    else if (run_ && speed_ < 1.6f) run_ = false;
    static const float walkOff[4] = {0.25f, 0.75f, 0.0f, 0.5f}, runOff[4] = {0.0f, 0.1f, 0.5f, 0.62f};
    const float* offs = run_ ? runOff : walkOff;
    const float duty = run_ ? 0.38f : 0.66f + 0.06f * crouchE;
    // V13.2: kuerzerer Galoppsprung (vorher 1,5 + 0,3 v: die Beine ueberstreckten sich, im Sammeln fiel der Rumpf zu Boden)
    const float S = run_ ? 1.05f + 0.17f * speed_ : 0.9f + 0.3f * speed_ - 0.25f * crouchE;
    vec3 rest[4];
    for (int s = 0; s < 2; ++s) {
        rest[s] = W({B[front(KNUCK_L, s)].x, B[front(KNUCK_L, s)].y, 0.0f});
        rest[2 + s] = W({B[hind(FOOT_L, s)].x, B[hind(FOOT_L, s)].y, 0.0f});
    }
    const float wgt[4] = {0.5f, 0.5f, 0.7f, 0.7f};
    stepLegs(dt, 4, offs, duty, S, rest, 0.42f, wgt);
    // V13.2: im Stehen ab und zu eine Pfote neu setzen (Gewicht verlagern) - nicht beim Wittern oder im Licht
    shuffleT_ -= dt;
    if (shuffleT_ <= 0.0f) {
        shuffleT_ = 4.0f + 5.0f * rnd();
        bool anySw = false;
        for (int i = 0; i < 4; ++i) anySw = anySw || leg_[i].swing;
        if (speed_ < 0.07f && !anySw && perc_ < 0.3f && light_ < 0.3f) {
            const int i = std::min(3, (int)(rnd() * 4.0f));
            Leg& f = leg_[i];
            f.swing = true, f.from = f.plant, f.t = 0.0f, f.dur = 0.45f;
            f.to = rest[i] + F * ((rnd() - 0.5f) * 0.08f) + Lw * ((rnd() - 0.5f) * 0.06f);
            f.to.z = pos_.z;
        }
    }
    const float lift[4] = {run_ ? 0.2f : 0.1f, run_ ? 0.2f : 0.1f, run_ ? 0.16f : 0.07f, run_ ? 0.16f : 0.07f};
    vec3 g[4];
    for (int i = 0; i < 4; ++i) g[i] = footPos(i, lift);

    // Rumpf: Brust und Becken; im Galopp wippt und beugt sich die Wirbelsaeule
    float bobC = 0.0f, bobP = 0.0f, flex = 0.0f;
    const bool moving = speed_ > 0.07f;
    if (run_) {
        bobC = 0.03f * std::sin(kTau * phase_);
        bobP = 0.035f * std::sin(kTau * phase_ + 2.2f);
        flex = 0.06f * std::sin(kTau * phase_ - 0.6f);
    } else if (moving) {
        bobC = 0.012f * std::cos(4.0f * kPi * phase_);
        bobP = 0.012f * std::cos(4.0f * kPi * phase_ + 1.0f);
    } else {
        bobC = 0.006f * std::sin(time_ * 2.4f);  // flacher, schneller Atem
    }
    // V13.2: Atem sichtbar (Brustkorb hebt sich, ~3 s), in Ruhe Gewicht von Seite zu Seite; im Licht zittert er
    const float breath = std::sin(time_ * 1.9f) * (0.6f + 0.4f * idleW_);
    swayPh_ += dt * (0.5f + 0.15f * std::sin(time_ * 0.21f));
    const float sway = std::sin(swayPh_) * idleW_ * (1.0f - perc_) * (1.0f - light_);
    const vec3 swayC = Lw * (0.022f * sway) + F * (0.012f * std::sin(swayPh_ * 0.5f) * idleW_);
    const vec3 swayP = Lw * (-0.012f * sway);
    const vec3 trem = Lw * (0.004f * light_ * std::sin(time_ * 37.0f)) + U * (0.003f * light_ * std::sin(time_ * 29.0f));
    // Angriff: der ganze Koerper springt nach vorn (nicht nur der Kopf); Licht: er weicht zurueck
    const vec3 lunge = F * (0.27f * reach_ - 0.07f * light_) - U * (0.05f * reach_);
    // hinkt: steht das linke Vorderbein, sackt die Brust ein
    float limp = 0.0f;
    if (moving) {
        float ph0 = phase_ + offs[0];
        ph0 -= std::floor(ph0);
        if (ph0 < duty) limp = 0.04f * std::sin(kPi * ph0 / duty);
    }
    vec3 C = W({B[CHEST].x + flex * 0.5f, 0.0f, 0.0f});
    C.z = pos_.z + B[CHEST].z - crouchE * 0.2f - (run_ ? 0.03f : 0.0f) + bobC - limp + 0.012f * breath + 0.05f * perc_;
    vec3 P = W({B[PELVIS].x - flex * 0.5f, 0.0f, 0.0f});
    P.z = pos_.z + B[PELVIS].z - crouchE * 0.1f + bobP - 0.03f * reach_ - 0.06f * light_;
    C = C + swayC + trem + lunge;
    P = P + swayP + lunge * 0.4f;
    // ausgemergelt: der Ruecken buckelt
    const vec3 ctrl = (P + C) * 0.5f + U * (0.08f - (run_ ? flex * 0.8f : 0.0f) + crouchE * 0.05f + 0.006f * breath + 0.04f * light_);
    auto bez = [&](float t) {
        float a = 1.0f - t;
        return P * (a * a) + ctrl * (2.0f * a * t) + C * (t * t);
    };
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[SPINE1] = bez(0.33f);
    J[SPINE2] = bez(0.67f);
    J[CHEST] = C;
    const vec3 chestFwd = safeNorm(C - J[SPINE2], F);

    // Finger zucken einzeln
    twitchT_ -= dt;
    if (twitchT_ <= 0.0f) {
        twitchT_ = 2.0f + rnd() * 6.0f;
        const int s = rnd() < 0.5f ? 0 : 1, f = std::min(4, (int)(rnd() * 5.0f));
        curlT_[s][f] = rnd() < 0.5f ? 0.45f + 0.3f * rnd() : 0.02f + 0.1f * rnd();
    }
    // Vorderbeine (Arme): Ellbogen nach hinten, die Hand flach, fuenf Finger
    for (int s = 0; s < 2; ++s) {
        const int sh = front(SHOULDER_L, s), el = front(ELBOW_L, s), wr = front(WRIST_L, s), kn = front(KNUCK_L, s);
        const float lu = length(B[el] - B[sh]), lf = length(B[wr] - B[el]);
        const vec3 S0 = C + Mb * (B[sh] - B[CHEST]);
        vec3 k = g[s];
        k.z += 0.025f;
        // V13.2: wittert er im Stehen, haelt er die linke Vorderpfote angehoben (lauernd)
        const float point = s == 0 ? perc_ * idleW_ : 0.0f;
        k = k + U * (0.09f * point) - F * (0.04f * point);
        // V13.2: Sprung - beide Vorderpfoten weit vor und angehoben
        k = k + F * (0.28f * reach_) + U * (0.12f * reach_);
        const float sw = std::max(leg_[s].swing ? std::sin(kPi * leg_[s].t) : 0.0f, 0.7f * point);
        vec3 wristT = k - F * (0.10f * (1.0f - sw * 0.6f)) + U * (0.095f + sw * 0.06f);
        // V13.2: geblendet hebt er die Pfote auf der Seite des Lichts vor das Gesicht
        const vec3 toLs = safeNorm(vec3{in.lightFrom.x - C.x, in.lightFrom.y - C.y, 0.0f}, F);
        const float shield = light_ * idleW_ * ((s == 0) == (dot(toLs, Lw) > 0.0f) ? 1.0f : 0.0f);
        if (shield > 0.001f) {
            const vec3 face = headPos_ + headDir_ * 0.1f + U * 0.02f;  // V13.2: vor die Augen
            wristT = wristT + (face - wristT) * (0.85f * shield);
            k = k + (face + F * 0.05f + U * 0.1f - k) * (0.85f * shield);
        }
        vec3 e, w;
        ik2(S0, wristT, lu, lf, U * 0.25f - F, e, w);
        J[sh] = S0;
        J[el] = e;
        J[wr] = w;
        J[kn] = k;
        for (int dg = 0; dg < 5; ++dg) {
            spring(curl_[s][dg], curlV_[s][dg], curlT_[s][dg], 4.0f, dt);
            if (std::fabs(curl_[s][dg] - curlT_[s][dg]) < 0.01f && curlT_[s][dg] > 0.3f) curlT_[s][dg] = 0.03f + 0.08f * rnd();
            const vec3 bd = B[front(D0_L, s) + dg] - B[kn];
            const float dl = length(bd);
            const vec3 dw = safeNorm(Mb * vec3{bd.x, bd.y, 0.0f}, F);
            const float c = sw * 1.1f + curl_[s][dg];
            J[front(D0_L, s) + dg] = k + safeNorm(dw * std::cos(c) - U * (0.12f + std::sin(c)), dw) * dl;
        }
    }
    // Hinterbeine: Ferse erhoben (Sprunggelenk nach hinten), Ballen und drei Zehen
    for (int s = 0; s < 2; ++s) {
        const int hp = hind(HIP_L, s), kn = hind(KNEE_L, s), hk = hind(HOCK_L, s), ft = hind(FOOT_L, s);
        const float lt = length(B[kn] - B[hp]), ls = length(B[hk] - B[kn]);
        const vec3 H = P + Mb * (B[hp] - B[PELVIS]);
        vec3 f = g[2 + s];
        f.z += 0.03f;
        const float sw = leg_[2 + s].swing ? std::sin(kPi * leg_[2 + s].t) : 0.0f;
        const vec3 hockT = f + Mb * vec3{-0.09f - sw * 0.05f, 0.0f, 0.14f + sw * 0.04f - crouch_ * 0.03f};
        vec3 knee, hock;
        ik2(H, hockT, lt, ls, F + U * 0.2f, knee, hock);
        J[hp] = H;
        J[kn] = knee;
        J[hk] = hock;
        J[ft] = f + (hock - hockT);
        for (int t = 0; t < 3; ++t) {
            const vec3 bd = B[hind(T0_L, s) + t] - B[ft];
            const vec3 dw = safeNorm(Mb * vec3{bd.x, bd.y, 0.0f}, F);
            J[hind(T0_L, s) + t] = J[ft] + safeNorm(dw - U * (0.25f + sw), dw) * length(bd);
        }
    }

    // Kopf: haengt vorgestreckt unter den Schultern; geduckt noch tiefer; beim Zuschnappen stoesst er vor
    vec3 hl = run_ ? vec3{0.76f, 0.0f, 0.68f} : vec3{0.70f, 0.0f, 0.64f};
    hl = hl + (vec3{0.74f, 0.0f, 0.5f} - hl) * crouch_;
    // V13.2: wittern - der Kopf hebt sich und streckt sich vor; Licht - er zieht ihn ein und senkt ihn
    hl = hl + vec3{0.06f, 0.0f, 0.16f} * perc_ + vec3{-0.12f, 0.0f, -0.1f} * light_;
    // Schnueffeln in Stoessen (0,75 s an der Luft, 0,75 s Pause)
    spring(sniff_, sniffV_, perc_ > 0.3f && std::fmod(time_, 1.5f) < 0.75f ? 1.0f : 0.0f, 10.0f, dt);
    vec3 headT = W(hl) + Lw * (lean_ * 0.5f);
    headT.z = pos_.z + hl.z;
    headT = headT + swayC + lunge + trem;  // der Kopf folgt dem Rumpf
    headT.z += 0.011f * perc_ * sniff_ * std::sin(time_ * 42.0f);
    if (reach_ > 0.01f) headT = headT + (in.reachAt - headT) * (reach_ * 0.7f);
    {
        vec3 dc = headT - C;
        float dl = length(dc);
        if (dl > 1e-4f) headT = C + dc * (std::clamp(dl, 0.25f, 0.5f + 0.04f * reach_) / dl);  // V13.2: Hals beim Sprung kaum laenger
    }
    const vec3 restDir = safeNorm(Mb * (B[SNOUT] - B[SKULL]), F);
    if (!chainInit_) {
        headPos_ = headT;
        headPosV_ = {};
        headDir_ = restDir;
        headDirV_ = {};
    }
    springV(headPos_, headPosV_, headT, run_ ? 8.0f : (reach_ > 0.2f ? 14.0f : 5.5f), dt);
    vec3 want = in.look ? safeNorm(in.lookAt - headPos_, restDir) : restDir;
    // V13.2: in Ruhe sieht er sich langsam um; witternd zum Reiz; im Licht vom Licht weg (nach unten-seitlich)
    if (!in.look && idleW_ > 0.01f) {
        const float sy = 0.35f * std::sin(time_ * 0.37f) * idleW_, sp = 0.12f * std::sin(time_ * 0.23f + 1.0f) * idleW_;
        want = safeNorm(Mb * vec3{std::cos(sy), std::sin(sy), sp - 0.25f}, restDir);
    }
    if (perc_ > 0.01f) want = safeNorm(want + (safeNorm(in.perceiveAt - headPos_, want) - want) * perc_, restDir);
    if (light_ > 0.01f) {
        const vec3 toL = safeNorm(vec3{in.lightFrom.x - headPos_.x, in.lightFrom.y - headPos_.y, 0.0f}, F);
        const vec3 away = safeNorm(Lw * (dot(toL, Lw) > 0.0f ? -1.0f : 1.0f) + F * 0.5f - U * 0.7f, restDir);
        want = safeNorm(want + (away - want) * light_, restDir);
    }
    {
        const float yw = std::clamp(std::atan2(dot(want, Lw), dot(want, F)), -1.2f, 1.2f);
        const float pt = std::clamp(std::asin(std::clamp(want.z, -1.0f, 1.0f)), -1.1f, 0.45f);
        want = Mb * vec3{std::cos(pt) * std::cos(yw), std::cos(pt) * std::sin(yw), std::sin(pt)};
    }
    springV(headDir_, headDirV_, want, in.alert > 0.5f ? 9.0f : 5.0f, dt);
    headDir_ = safeNorm(headDir_, restDir);
    // lauschen: still stehend den Kopf schief legen (er hoert sehr gut)
    listenT_ -= dt;
    listenOn_ -= dt;
    if (listenT_ <= 0.0f) {
        listenT_ = 4.0f + 6.0f * rnd();
        if (in.alert > 0.3f && speed_ < 0.4f) listenOn_ = 1.0f + 0.8f * rnd(), listenSide_ = rnd() < 0.5f ? 1.0f : -1.0f;
    }
    spring(listen_, listenV_, listenOn_ > 0.0f ? 0.38f * listenSide_ : 0.0f, 5.0f, dt);
    vec3 hu = safeNorm(orthoTo(U, headDir_), F);
    hu = rotateAround(hu, headDir_, tilt_ + listen_);
    const mat4 Hb = bindFrames_[hd::B_HEAD];
    const mat4 Hc = boneFrame(headPos_, headPos_ + headDir_, hu);
    const mat4 relH = Hc * rigidInverse(Hb);
    J[SKULL] = headPos_;
    J[SNOUT] = xf(relH, B[SNOUT]);
    J[JAWH] = xf(relH, B[JAWH]);
    // Kiefer: beim Knurren zittert er halb offen, beim Zuschnappen weit auf
    const vec3 headY = safeNorm(cross(hu, headDir_), Lw);
    const float open = jaw_ * 0.8f + growl_ * (0.14f + 0.035f * std::sin(time_ * 47.0f) + 0.02f * std::sin(time_ * 31.0f)) +
                       0.06f * perc_ + 0.25f * reach_;
    J[JAW] = J[JAWH] + rotateAround(xfDir(relH, B[JAW] - B[JAWH]), headY, open);
    // Hals: Bogen von der Brust zum Kopf
    {
        const vec3 t0 = safeNorm(chestFwd + U * 0.15f, F), t3 = headDir_;
        const float nl = length(headPos_ - C);
        const vec3 p1 = C + t0 * (nl * 0.38f), p2 = headPos_ - t3 * (nl * 0.32f);
        auto cub = [&](float t) {
            float a = 1.0f - t;
            return C * (a * a * a) + p1 * (3.0f * a * a * t) + p2 * (3.0f * a * t * t) + headPos_ * (t * t * t);
        };
        J[NECK1] = cub(0.34f);
        J[NECK2] = cub(0.68f);
    }
    autoUps();
    pose_.up[hd::B_HEAD] = hu;
    pose_.up[hd::B_JAW] = rotateAround(xfDir(relH, sk_->bindUp[hd::B_JAW]), headY, open);
    // Haar: haengt mit Traegheit, nicht durch Kopf und Rumpf
    cols_.clear();
    cols_.push_back({headPos_, J[SNOUT] - headDir_ * 0.03f, 0.075f});
    cols_.push_back({P + U * 0.0f, C, 0.105f});
    cols_.push_back({C, J[NECK2], 0.07f});
    for (int h = 0; h < kHeadHair; ++h) {
        const int idx[4] = {hairH(h, 0), hairH(h, 1), hairH(h, 2), hairH(h, 3)};
        hangChain(h, hd::B_HEAD, idx, 4, 11.0f, 0.9f, 3.5f, {}, dt);
    }
    const int backParent[3] = {hd::B_NECK1, hd::B_SPINE2, hd::B_SPINE1};
    for (int h = 0; h < kBackHair; ++h) {
        const int idx[3] = {hairB(h, 0), hairB(h, 1), hairB(h, 2)};
        hangChain(kHeadHair + h, backParent[h / 2], idx, 3, 16.0f, 0.9f, 2.5f, {}, dt);
    }
    cols_.clear();
    autoUps();
    pose_.up[hd::B_HEAD] = hu;
    pose_.up[hd::B_JAW] = rotateAround(xfDir(relH, sk_->bindUp[hd::B_JAW]), headY, open);
    for (int h = 0; h < kHeadHair; ++h) {
        const int bs[3] = {hairHB(h, 0), hairHB(h, 1), hairHB(h, 2)};
        chainUps(hd::B_HEAD, bs, 3);
    }
    for (int h = 0; h < kBackHair; ++h) {
        const int bs[2] = {hairBB(h, 0), hairBB(h, 1)};
        chainUps(backParent[h / 2], bs, 2);
    }
}

// --- Deathmoth -------------------------------------------------------------------------------
void CreatureAnimator::updateMoth(float dt, const Input& in) {
    using namespace mo;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 U{0, 0, 1};
    if (!chainInit_) fly_ = in.fly, flyV_ = 0.0f;
    spring(fly_, flyV_, in.fly, 4.0f, dt);
    fly_ = std::clamp(fly_, 0.0f, 1.0f);
    // Ausrichtung: im Flug Nase in Flugrichtung (steigt sie, hebt sich die Nase), Querneigung in
    // Kurven; sitzend auf der Flaeche (in.up)
    vec3 fwd = length(in.fwd) > 0.1f ? normalize(in.fwd) : dirYaw(yaw_);
    if (fly_ > 0.01f) fwd = safeNorm(fwd + U * (std::clamp(vel_.z * 0.5f, -0.5f, 0.5f) * fly_), fwd);
    const vec3 upFly = rotateAround(U, safeNorm(vec3{fwd.x, fwd.y, 0.0f}, {1, 0, 0}), bank_);
    const vec3 upWant = safeNorm(lerp(safeNorm(in.up, U), upFly, fly_), U);
    if (!chainInit_) bodyUp_ = upWant, bodyUpV_ = {};
    springV(bodyUp_, bodyUpV_, upWant, 8.0f, dt);
    bodyUp_ = safeNorm(bodyUp_, U);
    const vec3 Fw = safeNorm(orthoTo(fwd, bodyUp_), safeNorm(orthoTo(vec3{1, 0, 0}, bodyUp_), {0, 1, 0}));
    const vec3 Lw = cross(bodyUp_, Fw);
    bodyFwd_ = Fw;
    const M3 Mb{Fw, Lw, bodyUp_};
    // Fluegelschlag: ~9 Hz; das hintere Paar spaeter und schwaecher; sitzend flach an der Flaeche
    flap_ += dt * (8.5f + 1.5f * fly_);
    flap_ -= std::floor(flap_);
    flapAmp_ = fly_;
    shiverPh_ += dt * 27.0f;
    shiverPh_ -= std::floor(shiverPh_);
    const float ph = kTau * flap_;
    const float restA = -0.2f;
    const float shiver = std::clamp(in.shiver, 0.0f, 1.0f) * 0.13f * std::sin(kTau * shiverPh_);
    const float aF = restA + (0.32f + 0.85f * std::sin(ph) - restA) * fly_ + shiver;
    const float aH = restA + (0.26f + 0.72f * std::sin(ph - 0.65f) - restA) * fly_ + shiver * 0.8f;
    // sitzend schieben sich die Vorderfluegel nach hinten ueber die Hinterfluegel
    const float sweepF = fly_ * 0.22f * std::cos(ph) + (1.0f - fly_) * 0.38f, sweepH = fly_ * 0.16f * std::cos(ph - 0.65f) + (1.0f - fly_) * 0.1f;
    const float bob = -0.007f * std::sin(ph) * fly_;
    auto W = [&](vec3 l) { return pos_ + Mb * l + bodyUp_ * bob; };
    J[ROOT] = W(B[ROOT]);
    J[HEAD] = W(B[HEAD]);
    J[PROB] = W(B[PROB]);
    // Hinterleib: im Flug pendelt er mit dem Schlag, sitzend atmet er
    const float curlA = fly_ * (0.1f * std::sin(ph + 1.0f) + 0.08f) + (1.0f - fly_) * 0.03f * std::sin(time_ * 1.7f);
    J[ABD0] = W(B[ABD0]);
    const M3 r1 = axisAngle({0, 1, 0}, -curlA), r2 = r1 * r1, r3 = r2 * r1;
    J[ABD1] = J[ABD0] + Mb * (r1 * (B[ABD1] - B[ABD0]));
    J[ABD2] = J[ABD1] + Mb * (r2 * (B[ABD2] - B[ABD1]));
    J[ABDTIP] = J[ABD2] + Mb * (r3 * (B[ABDTIP] - B[ABD2]));
    J[BRISTLE_L] = J[ABDTIP] + Mb * (r3 * (B[BRISTLE_L] - B[ABDTIP]));
    J[BRISTLE_R] = J[ABDTIP] + Mb * (r3 * (B[BRISTLE_R] - B[ABDTIP]));
    // Fuehler: tasten, im Flug nach hinten gelegt
    antT_ -= dt;
    if (antT_ <= 0.0f) {
        antT_ = 0.4f + 1.6f * rnd();
        const int s = rnd() < 0.5f ? 0 : 1;
        antTg_[s] = (rnd() - 0.4f) * 0.5f;
    }
    for (int s = 0; s < 2; ++s) {
        spring(ant_[s], antV_[s], antTg_[s], 9.0f, dt);
        const int b = s == 0 ? ANTB_L : ANTB_R, t = s == 0 ? ANTT_L : ANTT_R;
        J[b] = W(B[b]);
        const vec3 v = rotateAround(B[t] - B[b], {0, s == 0 ? 1.0f : -1.0f, 0}, ant_[s] + fly_ * 0.45f);
        J[t] = J[b] + Mb * v;
    }
    // Fluegel: Platten, die um die Laengsachse schlagen und dabei vor und zurueck schwingen
    M3 wingRot[4];
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const int wf = s == 0 ? WF_L : WF_R, wft = s == 0 ? WFT_L : WFT_R, wh = s == 0 ? WH_L : WH_R, wht = s == 0 ? WHT_L : WHT_R;
        const M3 rf = axisAngle({1, 0, 0}, sg * aF) * axisAngle({0, 0, 1}, sg * sweepF);
        const M3 rh = axisAngle({1, 0, 0}, sg * aH) * axisAngle({0, 0, 1}, sg * sweepH);
        J[wf] = W(B[wf]);
        J[wft] = J[wf] + Mb * (rf * (B[wft] - B[wf]));
        J[wh] = W(B[wh]);
        J[wht] = J[wh] + Mb * (rh * (B[wht] - B[wh]));
        wingRot[s * 2] = rf, wingRot[s * 2 + 1] = rh;
    }
    // Beine: sitzend greifen sie die Flaeche, im Flug sind sie unter den Leib gezogen
    for (int l = 0; l < 6; ++l) {
        const float sg = (l % 2) == 0 ? 1.0f : -1.0f;
        const vec3 base = B[legJ(l, 0)];
        const float fx = B[legJ(l, 2)].x - base.x;
        const vec3 tuck[3] = {base + vec3{fx * 0.5f, sg * 0.016f, -0.004f}, base + vec3{fx * 0.25f - 0.012f, sg * 0.011f, -0.017f},
                              base + vec3{-0.016f, sg * 0.005f, -0.021f}};
        J[legJ(l, 0)] = W(base);
        for (int i = 1; i < 4; ++i) J[legJ(l, i)] = W(lerp(B[legJ(l, i)], tuck[i - 1], fly_));
    }
    // Knochen-oben: alles im Koerperrahmen (sie sitzt auch an Waenden und Decken)
    for (size_t b = 0; b < sk_->bones.size(); ++b) pose_.up[b] = Mb * sk_->bindUp[b];
    pose_.up[mo::B_ROOT] = Fw;
    pose_.up[mo::B_ABD1] = Mb * (r1 * sk_->bindUp[mo::B_ABD1]);
    pose_.up[mo::B_ABD2] = Mb * (r2 * sk_->bindUp[mo::B_ABD2]);
    pose_.up[mo::B_ABD3] = Mb * (r3 * sk_->bindUp[mo::B_ABD3]);
    pose_.up[mo::B_WF_L] = Mb * (wingRot[0] * sk_->bindUp[mo::B_WF_L]);
    pose_.up[mo::B_WH_L] = Mb * (wingRot[1] * sk_->bindUp[mo::B_WH_L]);
    pose_.up[mo::B_WF_R] = Mb * (wingRot[2] * sk_->bindUp[mo::B_WF_R]);
    pose_.up[mo::B_WH_R] = Mb * (wingRot[3] * sk_->bindUp[mo::B_WH_R]);
}

// --- Gestalt ---------------------------------------------------------------------------------
void CreatureAnimator::updateGestalt(float dt, const Input& in) {
    using namespace ge;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    const float s1 = 0.008f * std::sin(time_ * 0.41f), s2 = 0.006f * std::sin(time_ * 0.29f + 1.3f);
    const M3 Rs = axisAngle({0, 1, 0}, s1) * axisAngle({1, 0, 0}, s2);
    spring(tilt_, tiltV_, in.tilt, 0.7f, dt);
    float wantYaw = 0.0f;
    if (in.look) {
        const vec3 dl = in.lookAt - J[NECK];
        wantYaw = std::clamp(std::atan2(dot(dl, Lw), dot(dl, F)), -0.4f, 0.4f);
    }
    spring(headYaw_, headYawV_, wantYaw, 0.6f, dt);
    const int body[5] = {ROOT, LOW, MID, CHEST, NECK};
    for (int k : body) J[k] = pos_ + Mb * (Rs * B[k]);
    const M3 Rh = Rs * axisAngle({0, 0, 1}, headYaw_) * axisAngle({1, 0, 0}, tilt_);
    J[HEAD] = J[NECK] + Mb * (Rh * (B[HEAD] - B[NECK]));
    J[TOP] = J[NECK] + Mb * (Rh * (B[TOP] - B[NECK]));
    autoUps();
    for (int b = ge::B_LOW; b <= ge::B_CHEST; ++b) pose_.up[(size_t)b] = Mb * (Rs * sk_->bindUp[(size_t)b]);
    pose_.up[ge::B_NECK] = Mb * (Rh * sk_->bindUp[ge::B_NECK]);
    pose_.up[ge::B_HEAD] = Mb * (Rh * sk_->bindUp[ge::B_HEAD]);
    cols_.clear();
    for (int w = 0; w < kWisps; ++w) {
        const int idx[3] = {wisp(w, 0), wisp(w, 1), wisp(w, 2)};
        const float fw = (float)w;
        const vec3 wind{0.03f * std::sin(time_ * 0.6f + fw * 1.9f), 0.03f * std::cos(time_ * 0.5f + fw * 2.7f), 0.0f};
        hangChain(w, ge::B_LOW, idx, 3, 5.0f, 0.9f, -0.3f, wind, dt);
    }
    autoUps();
    for (int b = ge::B_LOW; b <= ge::B_CHEST; ++b) pose_.up[(size_t)b] = Mb * (Rs * sk_->bindUp[(size_t)b]);
    pose_.up[ge::B_NECK] = Mb * (Rh * sk_->bindUp[ge::B_NECK]);
    pose_.up[ge::B_HEAD] = Mb * (Rh * sk_->bindUp[ge::B_HEAD]);
}

// --- Nest ------------------------------------------------------------------------------------
void CreatureAnimator::updateNest() {
    const auto& B = sk_->bind;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    for (size_t k = 0; k < B.size(); ++k) pose_.j[k] = pos_ + Mb * B[k];
    for (size_t b = 0; b < sk_->bones.size(); ++b) pose_.up[b] = Mb * sk_->bindUp[b];
    pose_.up[ns::B_ROOT] = F;
}

}  // namespace lim::anim
