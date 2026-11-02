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
bool isBiped(Entity e) { return e == Entity::Faceling || e == Entity::SkinStealer || e == Entity::Wretch; }
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
    legs_ = e == Entity::Hound || e == Entity::Rat ? 4 : (isBiped(e) || e == Entity::Duller ? 2 : 0);
    int chains = 0;
    switch (e) {
        case Entity::Smiler: chains = sm::kWisps; break;
        case Entity::Hound: chains = hd::kHeadHair + hd::kBackHair; break;
        case Entity::Gestalt: chains = ge::kWisps; break;
        case Entity::Rat: chains = 1; break;  // V13.2: Schwanz
        case Entity::Growler: chains = 7; break;  // V13.3: Gewirr an Armen und Beinen, Ranken
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
    if (isBiped(e_) || e_ == Entity::Rat) {  // V13.2: Fuesse (Knoechel) bzw. Pfoten der Ruhepose
        for (int i = 0; i < legs_; ++i) {
            Leg& l = leg_[i];
            l.plant = l.from = l.to = restAt(e_ == Entity::Rat ? rt::legJ(i, 2) : bp::leg(bp::ANKLE_L, i));
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
    if (isBiped(e_)) return std::max(0.5f, pose_.j[bp::HEADTOP].z - pos_.z);  // V13.2: gebeugt (Wretch), geduckt
    if (e_ == Entity::Duller) return std::max(0.5f, pose_.j[du::HEADTOP].z - pos_.z);  // V13.3: gebueckt
    if (e_ == Entity::Growler) return std::max(0.3f, pose_.j[gw::HUMP].z - pos_.z + 0.12f * scale_);  // V13.3: aufgerichtet, zusammengesunken
    if (e_ == Entity::Nguithr) return sk_->height * scale_;
    if (e_ == Entity::Ouroboros) return ouroHeight();  // V13.3: Arbeiter gebueckt, Mauer, Geruest
    if (e_ == Entity::Goblin) return goblinHeight();   // V13.3: aufrecht, auf allen vieren, Portal
    return sk_->height;
}

vec3 CreatureAnimator::headPos() const {
    switch (e_) {
        case Entity::Smiler: return pose_.j[sm::FACE];
        case Entity::Hound: return pose_.j[hd::SKULL];
        case Entity::Moth: return pose_.j[mo::HEAD];
        case Entity::Gestalt: return pose_.j[ge::HEAD];
        case Entity::Clump: return pose_.j[cl::CORE];
        case Entity::Faceling:
        case Entity::SkinStealer:
        case Entity::Wretch: return (pose_.j[bp::HEAD] + pose_.j[bp::HEADTOP]) * 0.5f;
        case Entity::Rat: return pose_.j[rt::HEAD];
        case Entity::Window: return (pose_.j[wn::FIG_NECK] + pose_.j[wn::FIG_TOP]) * 0.5f;
        case Entity::Duller: return headPos_;  // V13.3: Kopf - mit ausgestrecktem Arm die greifende Hand (Schreck)
        case Entity::Arms: return headPos_;    // V13.3: Mitte der Krallen
        case Entity::Wrangler: return (pose_.j[wg::NECK0 + wg::kNeck] + pose_.j[wg::SNOUT]) * 0.5f;  // V13.3
        case Entity::Growler: return pose_.j[gw::KNOT];  // V13.3: der Knoten auf der Brust
        case Entity::Nguithr: return pose_.j[ng::EYES];
        case Entity::Ouroboros: return ouroHead();  // V13.3
        case Entity::Goblin: return goblinHead();   // V13.3
        case Entity::Jerry: return headPos_;        // V13.3 Teil 4: zwischen den Augen (Blickkontakt, Schreck)
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
    float turn = e_ == Entity::Hound ? 6.0f : (air ? 9.0f : (e_ == Entity::Smiler ? 1.2f : 0.5f));
    if (isBiped(e_)) turn = e_ == Entity::SkinStealer ? 3.0f : 4.5f;  // V13.2
    if (e_ == Entity::Rat) turn = 12.0f;
    if (e_ == Entity::Window) turn = 1000.0f;  // haengt fest in der Wand
    if (e_ == Entity::Duller) turn = 7.0f;     // V13.3: ruckartig
    if (e_ == Entity::Arms) turn = 1000.0f;    // V13.3: Richtung des Schachts
    if (e_ == Entity::Wrangler) turn = 1000.0f;  // V13.3: die Bahn liegt fest
    if (e_ == Entity::Growler) turn = 2.6f;      // V13.3: schwerfaellig
    if (e_ == Entity::Nguithr) turn = 1000.0f;   // V13.3: die Lage kommt aus up/fwd (eigene Feder)
    if (e_ == Entity::Ouroboros) turn = 1000.0f;  // V13.3: eigene Feder je Variante (Mauer und Geruest stehen fest)
    if (e_ == Entity::Goblin) turn = 1000.0f;     // V13.3: eigene Feder (ruckartige Vogelwendungen)
    if (e_ == Entity::Jerry) turn = 2.2f;         // V13.3 Teil 4: tritt auf dem Rohr langsam herum
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
            if (scale_ != 1.0f) {  // V13.3 Teil 4: kleinere Motten (Level 2) - Gelenke und Knochen um den Ankerpunkt
                for (vec3& j : pose_.j) j = pos_ + (j - pos_) * scale_;
                pose_.s.assign(sk_->bones.size(), scale_);
            }
            break;
        }
        case Entity::Gestalt: updateGestalt(dt, in); break;
        case Entity::Clump: updateClump(dt, in); break;
        case Entity::Faceling:
        case Entity::SkinStealer:
        case Entity::Wretch: updateBiped(dt, in); break;
        case Entity::Rat: updateRat(dt, in); break;
        case Entity::Window: updateWindow(dt, in); break;
        case Entity::Duller: updateDuller(dt, in); break;  // V13.3
        case Entity::Arms: updateArms(dt, in); break;
        case Entity::Wrangler: updateWrangler(dt, in); break;  // V13.3
        case Entity::Growler: updateGrowler(dt, in); break;  // V13.3
        case Entity::Nguithr: updateNguithr(dt, in); break;
        case Entity::Ouroboros: updateOuroboros(dt, in); break;  // V13.3
        case Entity::Goblin: updateGoblin(dt, in); break;  // V13.3
        case Entity::Jerry: updateJerry(dt, in); break;    // V13.3 Teil 4
        default: updateNest(dt, in); break;
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
    // V13.2: wahrnehmen - der Kopf legt sich kurz schief (neugierig); Licht zieht ihn an (Entity-3-Seite)
    spring(perc_, percV_, in.perceive, 2.5f, dt);
    spring(light_, lightV_, in.light, 3.0f, dt);
    spring(tilt_, tiltV_, in.tilt + 0.28f * perc_ * std::sin(time_ * 0.9f), 0.8f + 1.5f * perc_, dt);
    // schwebt; die obere Masse zieht beim Gleiten nach
    // V13.2: die Masse hebt und senkt sich langsam wie Atem (in Ruhe deutlicher)
    const vec3 hover = U * (0.016f * std::sin(time_ * 0.7f) + 0.007f * std::sin(time_ * 1.9f + 1.0f) + 0.018f * std::sin(time_ * 1.2f) * (1.0f - perc_));
    const vec3 upperRigid = W(B[UPPER]) + hover;
    if (!chainInit_) mass_ = upperRigid, massV_ = {};
    springV(mass_, massV_, upperRigid, 2.2f, dt);
    vec3 lag = mass_ - upperRigid;
    if (length(lag) > 0.25f) lag = normalize(lag) * 0.25f;
    const float sw1 = 0.03f * std::sin(time_ * 0.53f), sw2 = 0.025f * std::sin(time_ * 0.37f + 2.0f);
    // V13.2: die Masse neigt sich zum Reiz und zum Licht; vor dem Zugriff richtet sie sich auf
    vec3 toward{};
    auto flatDir = [&](vec3 at) { return safeNorm(vec3{at.x - upperRigid.x, at.y - upperRigid.y, 0.0f}, F); };
    if (perc_ > 0.01f) toward = toward + flatDir(in.perceiveAt) * perc_;
    if (light_ > 0.01f) toward = toward + flatDir(in.lightFrom) * light_;
    // V13.3 (Kanal bait, KREATUR_Smiler.md): am Leuchtkoeder senkt sich die obere Masse ueber das Licht, das Gesicht
    // folgt tiefer hinab als sonst
    spring(bait_, baitV_, in.bait, 1.1f, dt);
    const vec3 leanOff = toward * (0.13f + 0.27f * bait_) + U * (0.09f * jaw_ - 0.36f * bait_);
    J[ROOT] = pos_;
    J[BASE] = W(B[BASE]) + lag * 0.15f + Lw * (sw2 * 0.5f);
    J[CORE] = W(B[CORE]) + lag * 0.5f + Lw * sw1 + F * (sw2 * 0.5f) + hover * 0.5f + leanOff * 0.45f;
    J[UPPER] = upperRigid + lag + Lw * (sw1 * 0.6f) + F * (sw2 * 0.4f) + leanOff;
    // Gesicht: dreht sich langsam zu dir - weiter, als ein Kopf es koennte
    float wantYaw = 0.0f, wantPitch = 0.0f;
    auto aimAt = [&](vec3 at, float& yw, float& pt) {
        const vec3 dl = at - J[UPPER];
        const float lx = dot(dl, F), ly = dot(dl, Lw);
        yw = std::clamp(std::atan2(ly, lx), -1.5f, 1.5f);
        pt = std::clamp(std::atan2(dl.z - 0.25f, std::hypot(lx, ly)), -0.55f - 0.45f * bait_, 0.45f);
    };
    if (in.look) aimAt(in.lookAt, wantYaw, wantPitch);
    // V13.2: in Ruhe treibt das Gesicht kaum merklich (lebt, auch wenn es dich ansieht)
    wantYaw += 0.07f * std::sin(time_ * 0.31f + 0.5f), wantPitch += 0.04f * std::sin(time_ * 0.47f);
    // V13.2: zum Reiz, zum Licht - schneller als sonst
    const float pull = std::max(perc_, light_);
    if (pull > 0.01f) {
        float y2 = 0.0f, p2 = 0.0f;
        aimAt(light_ > perc_ ? in.lightFrom : in.perceiveAt, y2, p2);
        wantYaw += (y2 - wantYaw) * pull, wantPitch += (p2 - wantPitch) * pull;
    }
    spring(headYaw_, headYawV_, wantYaw, 1.0f + 2.5f * pull, dt);
    spring(headPitch_, headPitchV_, wantPitch, 1.0f + 2.5f * pull, dt);
    const M3 R = axisAngle({0, 0, 1}, headYaw_) * axisAngle({0, 1, 0}, -headPitch_) * axisAngle({1, 0, 0}, tilt_);
    // V11: die Zaehne zittern kaum merklich (der Unterkiefer bebt)
    const float tremble = 0.0035f * std::sin(time_ * 37.0f) + 0.0025f * std::sin(time_ * 53.0f + 1.0f);
    const M3 Rj = R * axisAngle({0, 1, 0}, jaw_ * 0.26f + tremble);  // V13.2: weiter auf (vorher 0,17)
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
    // V13.2: wahrnehmend weit offen, im Licht zusammengekniffen
    lids_ = std::clamp(std::max(blink, (0.12f + 0.38f * jaw_) * (1.0f - perc_) + 0.55f * light_), 0.0f, 1.0f);
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
        // V13.2: im Licht und vor dem Zugriff werden die Zungen unruhig und steigen hoeher
        const float agit = 1.0f + 1.5f * light_ + 1.0f * jaw_;
        const vec3 wind = vec3{0.06f * std::sin(time_ * 1.3f * agit + fw * 1.7f), 0.06f * std::cos(time_ * 1.1f * agit + fw * 2.3f),
                               0.03f * std::sin(time_ * 2.1f + fw)} * agit - lag * 0.8f;
        hangChain(w, sm::B_MID, idx, 4, 7.0f, 0.88f, -1.2f - 1.0f * jaw_, wind, dt);
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
    // V13.2: lauschen (es ist blind, Level-1-Seite: jagt nach Gehoer) - erstarrt, hebt den Kern, legt die Haende flach
    // und tastet mit den Fingern nach Erschuetterungen. Licht: keine Reaktion (blind).
    spring(perc_, percV_, in.perceive, 2.5f, dt);
    spring(idleW_, idleWV_, speed_ > 0.07f ? 0.0f : 1.0f, 2.5f, dt);
    const float floorZ = pos_.z, ceilZ = pos_.z + std::min(in.clearance, 6.0f);
    // Kern: am Boden ruckweise (Stoesse, waehrend die Haende ziehen), schaukelnd; in der Decke verborgen
    pull_ += (speed_ * 2.2f + 0.15f) * dt;
    const float pulse = std::pow(std::max(0.0f, std::sin(pull_ * 6.2832f)), 2.0f);
    // V13.2: Atem - der Kern hebt und senkt sich (in Ruhe deutlicher); lauschend steht er hoeher und still
    const float breathC = (0.02f + 0.02f * idleW_ * (1.0f - perc_)) * std::sin(time_ * 1.4f);
    const vec3 floorCore = pos_ + U * (kCoreZ + breathC + 0.22f * perc_);
    const vec3 hideCore = vec3{pos_.x, pos_.y, ceilZ + 0.5f};
    const vec3 want = floorCore + (hideCore - floorCore) * sm01(hang_);
    springV(core_, coreV_, want, 2.0f + 10.0f * pulse + 6.0f * hang_, dt);
    const vec3 lag = core_ - want;
    if (length(lag) > 0.35f) core_ = want + normalize(lag) * 0.35f;
    // aufbaeumen beim Griff (das Maul nach vorn), sonst schaukeln
    const float rock = (0.14f * std::sin(pull_ * 3.1f) * (1.0f - hang_) + 0.05f * std::sin(time_ * 0.9f)) * (1.0f - perc_);
    // lauschend neigt sich der Kern (mit Ohren und Maul) zur Richtung des Geraeuschs
    float listenTilt = 0.0f, listenRoll = 0.0f;
    if (perc_ > 0.01f) {
        const vec3 dl = in.perceiveAt - core_;
        listenTilt = -0.35f * perc_ * std::clamp(dot(safeNorm(dl, F), F), -1.0f, 1.0f);
        listenRoll = 0.35f * perc_ * std::clamp(dot(safeNorm(dl, F), L), -1.0f, 1.0f);
    }
    const M3 Rc = axisAngle(L, rock - grab_ * 1.25f - windup_ * 0.25f + listenTilt) *
                  axisAngle(F, 0.08f * std::sin(pull_ * 2.3f + 1.0f) * (1.0f - perc_) - listenRoll);
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
        float lie = 0.0f;  // V13.2: liegender langer Arm
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
            // V13.2: der laengste Arm liegt in Ruhe lang ausgestreckt (vorher stand er wie eine Stelze); lauschend
            // greifen alle Haende weit und flach aus
            lie = a == 0 ? 0.32f * idleW_ * (1.0f - grope_) : 0.0f;
            const float reach = (lu + lf) * (0.55f + lie + 0.15f * perc_ + 0.2f * grope_ * (0.5f + 0.5f * std::sin(time_ * 1.1f + (float)a)));
            const vec3 ideal = vec3{core_.x, core_.y, floorZ + 0.04f} + dr * reach + F * std::min(0.5f, speed_ * 0.4f);
            // V13.2: in Ruhe unruhig - ab und zu greift eine Hand ein Stueck neu (nie beim Lauschen)
            if (!g.swing && idleW_ > 0.5f && perc_ < 0.2f && rnd() < dt * 0.12f) g.stray = 0.0f;
            if (!g.swing && perc_ < 0.5f) {
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
        // V13.2: liegender langer Arm - Ellbogen flach nach aussen, nicht nach oben
        if (a == 0 && lie > 0.01f && flatHand) pole = safeNorm(d0 * 0.3f - U * idleW_ + U * (1.0f - idleW_), U);  // Ellbogen zum Boden
        if (flatHand && perc_ > 0.01f) pole = safeNorm(pole + (d0 * 0.4f - U * 0.6f - pole) * perc_, U);  // lauschend: alle Ellbogen flach
        vec3 E, Wr;
        ik2(S, target, lu, lf, pole, E, Wr);
        J[armJ(a, 0)] = S;
        J[armJ(a, 1)] = E;
        J[armJ(a, 2)] = Wr;
        // Finger: flach auf dem Boden, sonst in Armrichtung; sie zucken (Koeder, Tasten)
        // lauschend tippen die Finger in schnellen Stoessen auf den Boden
        const float tap = perc_ * std::max(0.0f, std::sin(time_ * (13.0f + 1.7f * (float)a) + (float)a * 1.3f));
        const float twitch = (a == 0 ? 0.6f : 0.15f + 0.5f * grope_) * std::sin(time_ * (9.0f + (float)a)) * std::sin(time_ * 2.3f + (float)a) + 0.8f * tap;
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
    // V13.2: wahrnehmen (Fuehler vor, Fluegel heben sich), Licht (zieht an: Nase zur Lampe, schnellerer Schlag),
    // Stich (Sturz mit der Nase voran, Beine vor, Hinterleib unter den Leib gekrummt)
    spring(perc_, percV_, in.perceive, 3.0f, dt);
    spring(light_, lightV_, in.light, 3.0f, dt);
    spring(reach_, reachV_, in.reach, 6.0f, dt);
    // Ausrichtung: im Flug Nase in Flugrichtung (steigt sie, hebt sich die Nase), Querneigung in
    // Kurven; sitzend auf der Flaeche (in.up)
    vec3 fwd = length(in.fwd) > 0.1f ? normalize(in.fwd) : dirYaw(yaw_);
    if (fly_ > 0.01f) fwd = safeNorm(fwd + U * (std::clamp(vel_.z * 0.5f, -0.5f, 0.5f) * fly_), fwd);
    if (light_ > 0.01f && fly_ > 0.3f) fwd = safeNorm(fwd + (safeNorm(in.lightFrom - pos_, fwd) - fwd) * (0.8f * light_), fwd);
    if (reach_ > 0.01f) fwd = safeNorm(fwd - U * (0.9f * reach_), fwd);
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
    flap_ += dt * (8.5f + 1.5f * fly_ + 4.0f * light_ + 3.0f * reach_);
    flap_ -= std::floor(flap_);
    flapAmp_ = fly_;
    shiverPh_ += dt * 27.0f;
    shiverPh_ -= std::floor(shiverPh_);
    const float ph = kTau * flap_;
    const float restA = -0.2f;
    // Aufwaermen: deutliches Zittern mit halb gehobenen Fluegeln (vorher 0,13 rad, im Bild kaum zu sehen)
    const float shivW = std::clamp(in.shiver, 0.0f, 1.0f);
    const float shiver = shivW * (0.24f * std::sin(kTau * shiverPh_) + 0.22f);
    // sitzend oeffnen sich die Fluegel ab und zu langsam (wie ruhende Motten), wahrnehmend heben sie sich
    const float restOpen = (1.0f - fly_) * (0.3f * std::pow(std::max(0.0f, std::sin(time_ * 0.7f + 1.0f)), 6.0f) + 0.25f * perc_);
    // im Sturz (Stich) schlagen die Fluegel flacher und legen sich nach hinten
    const float dive = 1.0f - 0.55f * reach_;
    const float aF = restA + (0.32f + 0.85f * std::sin(ph) * dive - restA) * fly_ + shiver + restOpen;
    const float aH = restA + (0.26f + 0.72f * std::sin(ph - 0.65f) * dive - restA) * fly_ + shiver * 0.8f + restOpen * 0.8f;
    // sitzend schieben sich die Vorderfluegel nach hinten ueber die Hinterfluegel
    const float sweepF = fly_ * 0.22f * std::cos(ph) + (1.0f - fly_) * 0.38f + 0.35f * reach_,
                sweepH = fly_ * 0.16f * std::cos(ph - 0.65f) + (1.0f - fly_) * 0.1f + 0.2f * reach_;
    const float bob = -0.007f * std::sin(ph) * fly_;
    // V13.2: am Licht zuckt sie unruhig hin und her (Motten taumeln um Lampen)
    const vec3 jit = (Lw * std::sin(time_ * 13.0f) + bodyUp_ * std::sin(time_ * 17.0f + 1.0f)) * (0.018f * light_ * fly_);
    auto W = [&](vec3 l) { return pos_ + Mb * l + bodyUp_ * bob + jit; };
    J[ROOT] = W(B[ROOT]);
    J[HEAD] = W(B[HEAD]);
    J[PROB] = W(B[PROB]);
    // Hinterleib: im Flug pendelt er mit dem Schlag, sitzend atmet er
    const float curlA = fly_ * (0.1f * std::sin(ph + 1.0f) + 0.08f) + (1.0f - fly_) * 0.03f * std::sin(time_ * 1.7f) + 0.5f * reach_;
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
        // wahrnehmend und im Licht zeigen die Fuehler nach vorn (nicht im Flug nach hinten gelegt)
        const float fwdAnt = std::max(perc_, light_);
        const vec3 v = rotateAround(B[t] - B[b], {0, s == 0 ? 1.0f : -1.0f, 0}, (ant_[s] + fly_ * 0.45f) * (1.0f - fwdAnt) - 0.35f * fwdAnt);
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
        // Stich: die Beine strecken sich vor und unter den Kopf (greifen)
        const vec3 strike[3] = {base + vec3{0.012f, sg * 0.012f, -0.012f}, base + vec3{0.026f, sg * 0.01f, -0.026f},
                                base + vec3{0.04f, sg * 0.006f, -0.034f}};
        for (int i = 1; i < 4; ++i) J[legJ(l, i)] = W(lerp(lerp(B[legJ(l, i)], tuck[i - 1], fly_), strike[i - 1], reach_));
    }
    for (int l = 6; l < 8; ++l)  // V13.3 Level 3: viertes Beinpaar (nur der Praetorianer hat dort ein Netz) - starr am Leib
        for (int i = 0; i < 4; ++i) J[legJ(l, i)] = W(B[legJ(l, i)]);
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
    // V13.2: wahrnehmen - der Kopf wendet sich ganz zum Beobachter, die Gestalt beugt sich ihm entgegen, die Schultern
    // heben sich (lauernd). Es geschieht nur ungesehen (Presence friert sie im Blick ein): beim naechsten Blick steht sie so da.
    spring(perc_, percV_, in.perceive, 1.2f, dt);
    // Gewicht verlagern (Huefte zur Seite, Gegenneigung) und Atem in Brust und Schultern
    const float shift = std::sin(time_ * 0.33f + 0.7f);
    const float s1 = 0.008f * std::sin(time_ * 0.41f) + 0.07f * perc_, s2 = 0.006f * std::sin(time_ * 0.29f + 1.3f) - 0.025f * shift;
    const M3 Rs = axisAngle({0, 1, 0}, s1) * axisAngle({1, 0, 0}, s2);
    spring(tilt_, tiltV_, in.tilt + 0.32f * perc_, 0.7f, dt);  // wahrnehmend legt sie den Kopf schief
    float wantYaw = 0.0f;
    if (in.look) {
        const vec3 dl = in.lookAt - J[NECK];
        const float lim = 0.4f + 0.7f * perc_;
        wantYaw = std::clamp(std::atan2(dot(dl, Lw), dot(dl, F)), -lim, lim);
    }
    spring(headYaw_, headYawV_, wantYaw, 0.6f + 0.6f * perc_, dt);
    const int body[5] = {ROOT, LOW, MID, CHEST, NECK};
    for (int k : body) J[k] = pos_ + Mb * (Rs * B[k]);
    const float breath = std::sin(time_ * 1.25f);
    J[LOW] = J[LOW] + Lw * (0.035f * shift);
    J[MID] = J[MID] + Lw * (0.022f * shift);
    // vorgebeugt durch Verschieben statt Drehen: die Arme haben keine eigenen Knochen und haengen so weiter senkrecht
    J[CHEST] = J[CHEST] + U * (0.007f * breath + 0.04f * perc_) + Lw * (0.008f * shift) + F * (0.06f * perc_);
    J[NECK] = J[NECK] + U * (0.005f * breath + 0.05f * perc_) + F * (0.12f * perc_);  // Schultern hoch, Kopf vorgeschoben
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
// V13.2: das Nest lebt - die Saecke heben und senken sich wie Atem (je eigene Phase), die Haut ueber der Oeffnung
// schwingt an ihrer Angel und woelbt sich mit dem Atem, die Wucherung im Flur pulsiert, das Gelege zuckt. Im Licht
// und wenn der Schwarm unruhig ist (alert) atmet es schneller und tiefer.
void CreatureAnimator::updateNest(float dt, const Input& in) {
    using namespace ns;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    spring(light_, lightV_, in.light, 1.5f, dt);
    spring(perc_, percV_, in.alert, 1.0f, dt);
    const float agit = std::clamp(std::max(light_, perc_), 0.0f, 1.0f);
    swayPh_ += dt * (1.3f + 1.6f * agit);  // Atemphase (~4,8 s, unruhig ~2,1 s)
    const float amp = 0.012f + 0.016f * agit;
    for (size_t k = 0; k < B.size(); ++k) J[k] = pos_ + Mb * B[k];
    // Saecke: entlang ihrer Achse nach aussen und zurueck
    for (int s = 0; s < 4; ++s) {
        const int a = SAC0_A + 2 * s, b = a + 1;
        const vec3 ax = normalize(B[b] - B[a]);
        const float br = std::sin(swayPh_ + 1.7f * (float)s) * (s == 3 ? 0.7f : 1.0f);
        const vec3 off = Mb * (ax * (amp * br));
        J[a] = J[a] + off, J[b] = J[b] + off;
    }
    // Haut: schwingt an der Angel (um die Querachse), woelbt sich mit dem Atem zum Flur
    {
        const float swing = 0.035f * std::sin(time_ * 0.8f) + 0.02f * std::sin(time_ * 1.9f + 1.0f) + 0.05f * agit * std::sin(time_ * 3.1f);
        const vec3 d = B[MEMB_B] - B[MEMB_T];
        const vec3 r{d.x * std::cos(swing) - d.z * std::sin(swing), d.y, d.x * std::sin(swing) + d.z * std::cos(swing)};
        J[MEMB_B] = J[MEMB_T] + Mb * (r + vec3{-amp * 0.8f * std::sin(swayPh_ + 0.6f), 0, 0});
    }
    // Wucherung im Flur: pulsiert leicht
    {
        const vec3 off = Mb * vec3{-0.006f * (1.0f + agit) * std::sin(swayPh_ * 1.3f + 2.0f), 0, 0};
        J[LIP_A] = J[LIP_A] + off, J[LIP_B] = J[LIP_B] + off;
    }
    // Gelege: zuckt ab und zu (Larven regen sich)
    {
        twitchT_ -= dt;
        if (twitchT_ <= 0.0f) twitchT_ = 1.5f + 3.0f * rnd(), sniffV_ = 1.0f;
        sniffV_ = std::max(0.0f, sniffV_ - dt * 3.0f);
        const vec3 off = Mb * vec3{0.0f, 0.004f * sniffV_ * std::sin(time_ * 40.0f), 0.003f * sniffV_ * std::sin(time_ * 33.0f)};
        J[EGG_A] = J[EGG_A] + off, J[EGG_B] = J[EGG_B] + off;
    }
    for (size_t b = 0; b < sk_->bones.size(); ++b) pose_.up[b] = Mb * sk_->bindUp[b];
    pose_.up[ns::B_ROOT] = F;
}

// --- V13.2: Zweibeiner (Faceling, Skin-Stealer, Wretch) -----------------------------------------------------------
// Gang auf zwei Beinen (stepLegs): Becken mit Hub, Seitschwung und Drehung, Rumpf mit Gegendrehung, die Arme haengen
// senkrecht und schwingen gegengleich. Der Kopf hat eine eigene Blickrichtung (Blick, Wahrnehmen, Licht, Umsehen, Rucke).
// Haltung je Wesen (BipedStyle, ANIMATIONEN.md):
//   Faceling     menschlich, etwas zaghaft; winkt (Level 903: gruesst Bekannte); vor der Lampe hebt er die Hand vor das
//                Gesicht und wendet sich ab; der entwickelte schlaegt mit dem Steinwerkzeug zu (strike).
//   Skin-Stealer lange Glieder, zu wenig Armschwung (er spielt einen Menschen), langsam drehender Kopf, ab und zu ein
//                Ruck; spricht er (talk), bewegt sich der Kiefer in Silben; jagend weit vorgebeugt, greift mit beiden Haenden.
//   Wretch       gebeugt, der Kopf haengt vor; schlurft (das rechte Bein zieht nach), der Kopf zuckt; sprintet mit
//                schlackernden Armen; fleht (plea) halb aufgerichtet mit vorgestreckten Haenden; gurgelt (growl).
namespace {
struct BipedStyle {
    float bend, bendRun;     // Vorbeugen des Rumpfs (rad), gehend und rennend
    float neck;              // Kopf vorgeschoben (rad im Hals)
    float lookDown;          // Grundneigung des Blicks (rad, + nach unten)
    float armOut, armSwing;  // Haengen der Arme seitlich (rad), Armschwung (rad)
    float elbow, elbowRun;   // Beugung der Ellbogen (rad)
    float runAt;             // ab hier rennt er (m/s)
    float stride;            // Schrittlaenge (Anteil)
    float lift, liftRun;     // Fusshub (m)
    float drag;              // 0..1: das rechte Bein schleift nach (Wretch)
    float jerk;              // ruckartige Kopfbewegungen je Sekunde
    float breathHz, breath;  // Atem: Frequenz, Hub der Schultern (m)
    float knees;             // Grundbeugung der Knie (Absenkung des Beckens, m)
};
const BipedStyle& bipedStyle(Entity e) {
    static const BipedStyle fc{0.03f, 0.22f, 0.05f, 0.05f, 0.11f, 0.30f, 0.22f, 1.25f, 2.4f, 1.0f, 0.07f, 0.16f, 0.0f, 0.0f, 0.25f, 0.005f, 0.01f};
    static const BipedStyle ss{0.08f, 0.42f, 0.12f, 0.12f, 0.08f, 0.09f, 0.10f, 1.0f, 2.6f, 1.08f, 0.06f, 0.2f, 0.0f, 0.08f, 0.14f, 0.009f, 0.015f};
    static const BipedStyle wr{0.40f, 0.62f, 0.55f, 0.1f, 0.16f, 0.14f, 0.45f, 1.0f, 2.2f, 0.82f, 0.035f, 0.15f, 0.7f, 0.35f, 0.45f, 0.007f, 0.06f};
    return e == Entity::SkinStealer ? ss : (e == Entity::Wretch ? wr : fc);
}
// Richtung (Gier, Neigung) von a nach b im Rahmen F/L (Neigung + nach oben)
void yawPitch(vec3 a, vec3 b, vec3 F, vec3 L, float& yw, float& pt) {
    const vec3 d = b - a;
    yw = std::atan2(dot(d, L), dot(d, F));
    pt = std::atan2(d.z, std::hypot(dot(d, F), dot(d, L)));
}
}  // namespace

void CreatureAnimator::updateBiped(float dt, const Input& in) {
    using namespace bp;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const BipedStyle& st = bipedStyle(e_);
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    auto W = [&](vec3 l) { return pos_ + Mb * l; };
    spring(lean_, leanV_, in.lean, 3.0f, dt);
    spring(jaw_, jawV_, in.jaw, 8.0f, dt);
    spring(reach_, reachV_, in.reach, 6.0f, dt);
    spring(tilt_, tiltV_, in.tilt, 3.0f, dt);
    spring(crouch_, crouchV_, in.crouch, 2.5f, dt);
    spring(growl_, growlV_, in.growl, 4.0f, dt);
    spring(light_, lightV_, in.light, 3.5f, dt);
    spring(perc_, percV_, in.perceive, 2.5f, dt);
    spring(wave_, waveV_, in.wave, 3.0f, dt);
    spring(talk_, talkV_, in.talk, 6.0f, dt);
    spring(plea_, pleaV_, in.plea, 2.0f, dt);
    spring(idleW_, idleWV_, speed_ > 0.07f ? 0.0f : 1.0f, 2.5f, dt);
    const bool wretch = e_ == Entity::Wretch;

    // Gang: zwei Beine im Wechsel, ab runAt rennen (mit Flugphase)
    if (!run_ && speed_ > st.runAt) run_ = true;
    else if (run_ && speed_ < st.runAt - 0.6f) run_ = false;
    static const float offs[2] = {0.0f, 0.5f};
    const float duty = run_ ? 0.36f : 0.58f + 0.05f * st.drag;
    const float legL = length(B[KNEE_L] - B[HIP_L]) + length(B[ANKLE_L] - B[KNEE_L]);
    const float S = st.stride * legL * (run_ ? 1.5f + 0.42f * speed_ : 0.85f + 0.38f * speed_);
    vec3 rest[2];
    for (int s = 0; s < 2; ++s) rest[s] = W({B[leg(ANKLE_L, s)].x, B[leg(ANKLE_L, s)].y * 0.85f, 0.0f});
    // Anlauf und Ausgleich: der Fuss, der am weitesten hinten steht, tritt sofort (sonst bleibt er im Stand stehen,
    // bis die Phase ihn ruft, und der Koerper zieht in einen Spagat)
    {
        const bool walkNow = speed_ > 0.07f;
        if (walkNow && (run_ || (!leg_[0].swing && !leg_[1].swing))) {  // im Rennen auch waehrend des anderen Schwungs
            int lag = walk_ ? -1 : 0;
            float worst = 0.0f;
            for (int i = 0; i < 2; ++i) {
                if (leg_[i].swing) continue;
                vec3 d = leg_[i].plant - rest[i];
                d.z = 0.0f;
                if (-dot(d, F) > worst) worst = -dot(d, F), lag = i;
            }
            if (lag >= 0 && (!walk_ || worst > 0.42f * legL)) {
                const float ph = duty + 0.01f - offs[lag];
                phase_ = ph - std::floor(ph);
            }
        }
        walk_ = walkNow;
    }
    const float heavy = e_ == Entity::SkinStealer ? 0.9f : 0.55f;
    const float wgt[2] = {heavy, st.drag > 0.0f ? 0.35f : heavy};
    stepLegs(dt, 2, offs, duty, S, rest, 0.4f, wgt);
    // im Stehen ab und zu einen Fuss neu setzen (Gewicht verlagern) - nicht beim Lauschen, Greifen oder Flehen
    shuffleT_ -= dt;
    if (shuffleT_ <= 0.0f) {
        shuffleT_ = 5.0f + 6.0f * rnd();
        if (speed_ < 0.07f && !leg_[0].swing && !leg_[1].swing && perc_ < 0.3f && reach_ < 0.1f && plea_ < 0.3f) {
            const int i = rnd() < 0.5f ? 0 : 1;
            Leg& f = leg_[i];
            f.swing = true, f.from = f.plant, f.t = 0.0f, f.dur = 0.5f;
            f.to = rest[i] + F * ((rnd() - 0.5f) * 0.1f) + Lw * ((rnd() - 0.3f) * 0.05f * (i == 0 ? 1.0f : -1.0f));
            f.to.z = pos_.z;
        }
    }
    const float lf = run_ ? st.liftRun : st.lift;
    const float lift[2] = {lf, std::max(0.012f, lf * (1.0f - st.drag))};
    vec3 g[2];
    for (int i = 0; i < 2; ++i) g[i] = footPos(i, lift);
    const bool moving = speed_ > 0.07f;
    const float mv = sm01(speed_ / 0.6f);
    const float c1 = std::cos(kTau * phase_);
    // Hub: tief im Doppelstand, hoch ueber dem Standbein; im Rennen tief in der Mitte des Stands
    float bob = 0.0f;
    if (run_) bob = -0.03f * std::cos(2.0f * kTau * (phase_ - 0.18f));
    else if (moving) bob = -0.016f * mv * std::cos(2.0f * kTau * (phase_ - 0.05f));
    // Wretch: auf dem schleifenden Bein sackt er ein
    float limp = 0.0f;
    if (moving && st.drag > 0.0f) {
        float ph1 = phase_ + offs[1];
        ph1 -= std::floor(ph1);
        if (ph1 < duty) limp = 0.045f * st.drag * std::sin(kPi * ph1 / duty);
    }
    // Atem (Wretch: rasselnd, unregelmaessig) und in Ruhe das Gewicht von Seite zu Seite
    const float bph = time_ * kTau * st.breathHz;
    const float breath = std::sin(bph) + (wretch ? 0.35f * std::sin(bph * 2.7f + 1.0f) : 0.0f);
    swayPh_ += dt * (0.42f + 0.1f * std::sin(time_ * 0.17f));
    const float sway = std::sin(swayPh_) * idleW_ * (1.0f - perc_ * 0.7f);
    // Gesten
    const float grab = reach_;
    const float stk = std::clamp(in.strike, 0.0f, 1.0f);
    const float stkWind = stk > 0.0f ? sm01(stk / 0.45f) * (1.0f - sm01((stk - 0.55f) / 0.12f)) : 0.0f;
    const float stkHit = stk > 0.0f ? sm01((stk - 0.55f) / 0.12f) * (1.0f - sm01((stk - 0.85f) / 0.15f)) : 0.0f;

    // Rumpf: Vorbeugen je Gangart und Geste
    float bend = st.bend + (st.bendRun - st.bend) * sm01((speed_ - 0.8f) / std::max(0.5f, st.runAt + 0.2f));
    bend += 0.3f * crouch_ + 0.22f * grab + 0.15f * stkHit - 0.05f * stkWind - st.bend * 0.6f * plea_;
    if (e_ == Entity::Faceling) bend -= 0.07f * light_;     // weicht vor der Lampe zurueck
    if (e_ == Entity::SkinStealer) bend += 0.06f * perc_;   // beugt sich dem Reiz entgegen
    const float py = -0.09f * c1 * mv * (run_ ? 1.3f : 1.0f);
    vec3 P = pos_ + Lw * ((run_ ? 0.01f : 0.024f) * mv * std::cos(kTau * (phase_ - 0.3f)) + 0.022f * sway) + F * (0.16f * grab * (1.0f - mv));
    P.z = pos_.z + B[PELVIS].z - st.knees - (run_ ? 0.05f : 0.0f) + bob - limp - 0.22f * crouch_ - 0.06f * grab;
    const M3 Rpel = axisAngle({0, 0, 1}, py) * axisAngle({0, 1, 0}, bend * 0.3f) * axisAngle({1, 0, 0}, lean_ * 0.3f);
    // Fuesse: im Schwung rollt der Fuss ab (Zehen erst unten, beim Aufsetzen oben); im Stand hebt sich die Ferse, sobald
    // der Fuss hinter der Huefte steht (er dreht um die Zehen, der Knoechel steigt)
    const float ankH = B[ANKLE_L].z;
    vec3 ankT[2], footV[2];
    for (int s = 0; s < 2; ++s) {
        const Leg& L = leg_[s];
        const vec3 fd0 = Mb * (B[leg(TOE_L, s)] - B[leg(ANKLE_L, s)]);
        float pitch = 0.0f;
        if (L.swing) {
            const float t = L.t;
            pitch = 0.45f * std::sin(kPi * t) * (1.0f - t) - 0.2f * sm01((t - 0.65f) / 0.35f);
            if (s == 1 && st.drag > 0.0f) pitch = pitch * (1.0f - st.drag) + 0.42f * st.drag;  // die Zehen schleifen
        } else if (moving) {
            const float rel = dot(g[s] - P, F);
            pitch = std::clamp((-rel - 0.1f) * 1.8f, 0.0f, 0.75f) * (1.0f - 0.6f * st.drag);
        }
        footV[s] = rotateAround(fd0, Lw, pitch);
        ankT[s] = g[s] + U * ankH + (L.swing ? vec3{} : fd0 - footV[s]);
        const float toeZ = ankT[s].z + footV[s].z;
        if (toeZ < pos_.z + 0.01f) ankT[s].z += pos_.z + 0.01f - toeZ;
    }
    // die Beine muessen den Boden erreichen: Becken notfalls absenken
    {
        float drop = 0.0f;
        for (int s = 0; s < 2; ++s) {
            const vec3 H = P + Mb * (Rpel * (B[leg(HIP_L, s)] - B[PELVIS]));
            const vec3 A = ankT[s];
            const float h = std::hypot(A.x - H.x, A.y - H.y), lmax = legL * 0.985f;
            const float allowed = std::sqrt(std::max(lmax * lmax - h * h, 0.01f));
            drop = std::max(drop, (H.z - A.z) - allowed);
        }
        P.z -= std::min(drop, 0.07f + 0.15f * crouch_);  // nie in die Hocke fallen: lieber streckt sich das Bein (Fuss hebt ab)
    }
    // Schultern drehen gegen das Becken; beim Ausholen zurueck, beim Schlag vor
    const float tw = 0.11f * c1 * mv * (run_ ? 1.5f : 1.0f) * std::min(1.0f, st.armSwing / 0.2f) - 0.35f * stkWind + 0.25f * stkHit;
    const M3 Rsp = Rpel * axisAngle({0, 0, 1}, -py * 0.7f + tw * 0.3f) * axisAngle({0, 1, 0}, bend * 0.35f) * axisAngle({1, 0, 0}, lean_ * 0.35f);
    const M3 Rch = Rsp * axisAngle({0, 0, 1}, -py * 0.5f + tw * 0.7f) * axisAngle({0, 1, 0}, bend * 0.35f - 0.012f * breath) *
                   axisAngle({1, 0, 0}, lean_ * 0.35f);
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[SPINE] = P + Mb * (Rpel * (B[SPINE] - B[PELVIS]));
    J[CHEST] = J[SPINE] + Mb * (Rsp * (B[CHEST] - B[SPINE]));
    J[NECK] = J[CHEST] + Mb * (Rch * (B[NECK] - B[CHEST])) + U * (st.breath * breath * 0.6f);
    const float neckP = st.neck * (1.0f - 0.5f * plea_) + 0.08f * perc_ * (e_ == Entity::SkinStealer ? 1.0f : 0.3f) +
                        0.25f * grab * (wretch ? 1.0f : 0.3f);
    const M3 Rnk = Rch * axisAngle({0, 1, 0}, neckP);
    J[HEAD] = J[NECK] + Mb * (Rnk * (B[HEAD] - B[NECK]));

    // Kopf: Blickrichtung im Koerperrahmen (bleibt aufrecht, auch wenn der Hals vorhaengt)
    float wy = 0.0f, wp = -st.lookDown;
    if (in.look) {
        yawPitch(J[HEAD], in.lookAt, F, Lw, wy, wp);
    } else if (idleW_ > 0.01f) {
        wy = 0.45f * std::sin(time_ * 0.31f) * idleW_;
        wp = -st.lookDown + 0.08f * std::sin(time_ * 0.19f + 1.0f) * idleW_;
    }
    if (perc_ > 0.01f) {
        float y2, p2;
        yawPitch(J[HEAD], in.perceiveAt, F, Lw, y2, p2);
        wy += wrapPi(y2 - wy) * perc_;
        wp += (p2 - wp) * perc_;
    }
    if (light_ > 0.01f) {  // vom Licht weg, Kopf gesenkt (der Faceling deutlich, der Skin-Stealer kaum)
        float ly, lp;
        yawPitch(J[HEAD], in.lightFrom, F, Lw, ly, lp);
        const float away = ly > 0.0f ? ly - 1.3f : ly + 1.3f;
        const float k = e_ == Entity::Faceling ? 0.8f : (wretch ? 0.5f : 0.3f);
        wy += wrapPi(away - wy) * light_ * k;
        wp += (-0.45f - wp) * light_ * k * 0.7f;
    }
    if (st.jerk > 0.0f) {  // Rucke: ein Stoss in die Feder des Kopfes
        twitchT_ -= dt;
        if (twitchT_ <= 0.0f) {
            twitchT_ = (0.4f + 1.2f * rnd()) / st.jerk;
            headYawV_ += (rnd() - 0.5f) * 14.0f;
            headPitchV_ += (rnd() - 0.5f) * 6.0f;
        }
    }
    wy = std::clamp(wrapPi(wy), -1.25f, 1.25f);
    wp = std::clamp(wp, -0.9f, 0.6f);
    if (!chainInit_) headYaw_ = wy, headPitch_ = wp, headYawV_ = headPitchV_ = 0.0f;
    const float hw = in.alert > 0.5f || reach_ > 0.2f ? 9.0f : (e_ == Entity::SkinStealer ? 2.2f : 4.0f);
    spring(headYaw_, headYawV_, wy, hw, dt);
    spring(headPitch_, headPitchV_, wp, hw, dt);
    // lauschen: still stehend den Kopf schief legen
    listenT_ -= dt;
    listenOn_ -= dt;
    if (listenT_ <= 0.0f) {
        listenT_ = 4.0f + 6.0f * rnd();
        if (in.alert > 0.3f && speed_ < 0.4f) listenOn_ = 1.0f + 0.8f * rnd(), listenSide_ = rnd() < 0.5f ? 1.0f : -1.0f;
    }
    spring(listen_, listenV_, listenOn_ > 0.0f ? 0.3f * listenSide_ : 0.0f, 5.0f, dt);
    const float tiltAll = tilt_ + listen_ + 0.25f * plea_ + (e_ == Entity::SkinStealer ? 0.2f * perc_ : 0.0f);
    const M3 Rh = axisAngle({0, 0, 1}, headYaw_) * axisAngle({0, 1, 0}, -headPitch_) * axisAngle({1, 0, 0}, tiltAll);
    J[HEADTOP] = J[HEAD] + Mb * (Rh * (B[HEADTOP] - B[HEAD]));
    // Kiefer: offen (jaw), zugreifend, sprechend in Silben, Wretch haengend und gurgelnd, flehend
    float open = 0.3f * jaw_ + (e_ == Entity::Faceling ? 0.0f : 0.25f * grab);
    if (talk_ > 0.01f) {
        const float syl = std::max(0.0f, std::sin(time_ * 28.0f)) * (0.55f + 0.45f * std::sin(time_ * 3.7f));
        open += talk_ * 0.16f * syl * (std::fmod(time_, 2.6f) < 1.8f ? 1.0f : 0.0f);
    }
    if (wretch) open += 0.08f + growl_ * (0.12f + 0.04f * std::sin(time_ * 41.0f)) + 0.1f * plea_ * (0.5f + 0.5f * std::sin(time_ * 9.0f));
    else open += 0.1f * growl_;
    const M3 Rj = Rh * axisAngle({0, 1, 0}, open);
    J[JAW] = J[HEAD] + Mb * (Rj * (B[JAW] - B[HEAD]));
    headPos_ = (J[HEAD] + J[HEADTOP]) * 0.5f;
    headDir_ = Mb * (Rh * vec3{1, 0, 0});

    // Arme
    const float shrug = st.breath * breath * 0.6f + (e_ == Entity::Faceling ? 0.025f * light_ : 0.0f) + 0.02f * plea_;
    const bool lightLeft = dot(in.lightFrom - J[CHEST], Lw) > 0.0f;
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const int sh = arm(SHOULDER_L, s), el = arm(ELBOW_L, s), wr = arm(WRIST_L, s), hn = arm(HAND_L, s);
        const float l1 = length(B[el] - B[sh]), l2 = length(B[wr] - B[el]), lh = length(B[hn] - B[wr]);
        const vec3 S0 = J[CHEST] + Mb * (Rch * (B[sh] - B[CHEST])) + U * shrug;
        // Grundhaltung: haengt senkrecht (auch bei gebeugtem Rumpf), schwingt gegengleich zum Bein derselben Seite
        float phi = (s == 0 ? -1.0f : 1.0f) * st.armSwing * c1 * mv * (run_ ? 1.8f : 1.0f);
        float out = st.armOut + 0.015f * sway * sg;
        float elb = st.elbow + (run_ ? st.elbowRun : 0.0f) * mv + std::max(0.0f, phi) * 0.5f;
        if (run_ && wretch) {  // die Arme schlackern beim Sprint nach hinten aussen
            phi = -0.5f + 0.35f * std::sin(time_ * 9.5f + 2.0f * (float)s);
            out += 0.35f + 0.15f * std::sin(time_ * 7.3f + (float)s);
            elb = 0.3f + 0.25f * std::sin(time_ * 8.1f + 1.3f * (float)s);
        }
        const vec3 du = Mb * normalize(vec3{std::sin(phi) * std::cos(out), sg * std::sin(out), -std::cos(phi) * std::cos(out)});
        const vec3 bax = safeNorm(cross(du, F), -Lw);
        const vec3 df = rotateAround(du, bax, elb);
        vec3 Wt = S0 + du * l1 + df * l2;
        vec3 hdir = rotateAround(df, bax, 0.12f);
        vec3 pole = safeNorm(-F + Lw * (sg * 0.35f) - U * 0.25f, -F);
        auto blend = [&](vec3 w, vec3 h, float a) {
            if (a <= 0.001f) return;
            Wt = Wt + (w - Wt) * a;
            hdir = safeNorm(hdir + (h - hdir) * a, h);
        };
        // winken (rechte Hand): Oberarm seitlich hoch, Unterarm aufrecht, pendelt
        if (s == 1 && wave_ > 0.001f) {
            const vec3 od = Mb * normalize(vec3{0.35f, sg, -0.1f});
            const float wv = 0.38f * std::sin(time_ * kTau * 1.7f);
            const vec3 fa = safeNorm(U * std::cos(wv) + od * std::sin(wv), U);
            blend(S0 + od * l1 + fa * (l2 * 0.95f), fa, wave_);
            pole = safeNorm(pole + (od - U - pole) * wave_, pole);
        }
        // Lampe im Gesicht (Faceling): die Hand auf der Seite des Lichts vor das Gesicht
        // (nur ruhig: wer aufgebracht ist oder zuschlaegt, hebt die Hand nicht vor das Gesicht)
        if (e_ == Entity::Faceling && (s == 0) == lightLeft)
            blend(headPos_ + headDir_ * 0.16f - U * 0.03f - Lw * (sg * 0.02f), safeNorm(U + headDir_ * 0.2f, U),
                  light_ * 0.9f * (1.0f - std::clamp(in.alert, 0.0f, 1.0f)) * (stk > 0.0f ? 0.0f : 1.0f));
        // flehen: beide Haende vor, Handflaechen nach oben, zitternd
        blend(S0 + Mb * vec3{0.34f, -sg * 0.07f, -0.16f} + U * (0.006f * std::sin(time_ * 13.0f + (float)s)),
              Mb * normalize(vec3{1.0f, -sg * 0.15f, 0.25f}), plea_);
        // zugreifen: beide Arme zum Ziel
        {
            const vec3 to = in.reachAt - S0;
            const float dl = length(to);
            const vec3 dirT = dl > 1e-4f ? to / dl : F;
            blend(S0 + dirT * std::min(dl, (l1 + l2) * 0.97f), dirT, grab);
        }
        // zuschlagen (rechte Hand mit dem Steinwerkzeug): ueber den Kopf ausholen, nach vorn unten schlagen
        if (s == 1 && stk > 0.0f) {
            blend(S0 + Mb * vec3{-0.12f, -0.02f, 0.0f} + U * ((l1 + l2) * 0.8f), Mb * normalize(vec3{-0.4f, 0.0f, 1.0f}), stkWind);
            const vec3 dirT = safeNorm(in.reachAt - S0, F);
            blend(S0 + safeNorm(dirT - U * 0.35f, F) * ((l1 + l2) * 0.95f), safeNorm(dirT - U * 0.6f, F), stkHit);
        }
        vec3 e, w;
        ik2(S0, Wt, l1, l2, pole, e, w);
        J[sh] = S0;
        J[el] = e;
        J[wr] = w;
        J[hn] = w + safeNorm(hdir, U) * lh;
    }
    // Beine: Huefte - Knie (vorn) - Knoechel - Zehen
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const int hp = leg(HIP_L, s), kn = leg(KNEE_L, s), an = leg(ANKLE_L, s), to = leg(TOE_L, s);
        const float l1 = length(B[kn] - B[hp]), l2 = length(B[an] - B[kn]);
        const vec3 H = P + Mb * (Rpel * (B[hp] - B[PELVIS]));
        vec3 knee, ank;
        ik2(H, ankT[s], l1, l2, safeNorm(F + Lw * (sg * 0.08f), F), knee, ank);
        J[hp] = H;
        J[kn] = knee;
        J[an] = ank;
        J[to] = ank + footV[s];
    }
    autoUps();
    pose_.up[bp::B_PELVIS] = Mb * (Rpel * sk_->bindUp[bp::B_PELVIS]);
    pose_.up[bp::B_SPINE] = Mb * (Rsp * sk_->bindUp[bp::B_SPINE]);
    pose_.up[bp::B_CHEST] = Mb * (Rch * sk_->bindUp[bp::B_CHEST]);
    pose_.up[bp::B_NECK] = Mb * (Rnk * sk_->bindUp[bp::B_NECK]);
    pose_.up[bp::B_HEAD] = Mb * (Rh * sk_->bindUp[bp::B_HEAD]);
    pose_.up[bp::B_JAW] = Mb * (Rj * sk_->bindUp[bp::B_JAW]);
    for (int s = 0; s < 2; ++s) pose_.up[(size_t)armB(bp::B_CLAV_L, s)] = Mb * (Rch * sk_->bindUp[(size_t)armB(bp::B_CLAV_L, s)]);
    if (e_ == Entity::Wretch || e_ == Entity::SkinStealer) bipedKinL3(dt, in);  // V13.3 Level 3
}

// --- V13.2: Death Rat ------------------------------------------------------------------------------------------------
// Trab (diagonale Paare); schnell ein Galopp in Spruengen (vorn und hinten je fast gleichzeitig, der Ruecken beugt und
// streckt sich). Im Stand schnell atmend, die Nase schnueffelt in Stoessen; witternd (perceive) richtet sie sich auf den
// Hinterbeinen auf, die Vorderpfoten vor der Brust. Fressen (jaw): Kopf am Boden, kauend. Beissen (reach): Stoss nach
// vorn, das Maul weit auf. Im Licht duckt sie sich und dreht den Kopf weg. Der Schwanz schleift mit Traegheit nach.
void CreatureAnimator::updateRat(float dt, const Input& in) {
    using namespace rt;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    auto W = [&](vec3 l) { return pos_ + Mb * l; };
    const float sc = sk_->height / 0.2f;  // Massstab des Skeletts (kRatScale)
    spring(jaw_, jawV_, in.jaw, 10.0f, dt);
    spring(perc_, percV_, in.perceive, 4.0f, dt);
    spring(reach_, reachV_, in.reach, 9.0f, dt);
    spring(light_, lightV_, in.light, 4.0f, dt);
    spring(idleW_, idleWV_, speed_ > 0.05f ? 0.0f : 1.0f, 3.0f, dt);
    if (!run_ && speed_ > 0.9f) run_ = true;
    else if (run_ && speed_ < 0.6f) run_ = false;
    static const float walkOff[4] = {0.0f, 0.5f, 0.5f, 0.0f}, runOff[4] = {0.5f, 0.58f, 0.0f, 0.08f};
    const float* offs = run_ ? runOff : walkOff;
    const float duty = run_ ? 0.38f : 0.62f;
    const float S = (run_ ? 0.24f + 0.07f * speed_ : 0.09f + 0.14f * speed_) * sc;
    vec3 rest[4];
    for (int l = 0; l < 4; ++l) rest[l] = W({B[legJ(l, 2)].x, B[legJ(l, 2)].y, 0.0f});
    const float wgt[4] = {0.05f, 0.05f, 0.07f, 0.07f};
    stepLegs(dt, 4, offs, duty, S, rest, 0.12f, wgt);
    const float lift[4] = {(run_ ? 0.03f : 0.016f) * sc, (run_ ? 0.03f : 0.016f) * sc, (run_ ? 0.035f : 0.013f) * sc, (run_ ? 0.035f : 0.013f) * sc};
    vec3 g[4];
    for (int i = 0; i < 4; ++i) g[i] = footPos(i, lift);
    const bool moving = speed_ > 0.05f;
    const float rear = perc_ * idleW_ * (1.0f - jaw_);
    float bobC = 0.0f, bobP = 0.0f, flex = 0.0f;
    if (run_) {
        bobC = 0.012f * sc * std::sin(kTau * phase_);
        bobP = 0.014f * sc * std::sin(kTau * phase_ + 2.4f);
        flex = std::sin(kTau * phase_ - 0.5f);
    } else if (moving) {
        bobC = 0.004f * sc * std::cos(4.0f * kPi * phase_);
        bobP = 0.004f * sc * std::cos(4.0f * kPi * phase_ + 1.0f);
    } else {
        bobC = 0.0025f * sc * std::sin(time_ * 9.0f);  // schneller Atem
    }
    const vec3 lunge = F * (0.06f * sc * reach_);
    vec3 C = W({B[CHEST].x + flex * 0.03f * sc - rear * 0.05f * sc, 0.0f, 0.0f}) + lunge;
    C.z = pos_.z + B[CHEST].z + bobC + rear * 0.11f * sc - 0.015f * sc * jaw_ - 0.02f * sc * light_;
    vec3 P = W({B[PELVIS].x - flex * 0.03f * sc, 0.0f, 0.0f}) + lunge * 0.3f;
    P.z = pos_.z + B[PELVIS].z + bobP - rear * 0.02f * sc - 0.015f * sc * light_;
    const vec3 ctrl = (P + C) * 0.5f + U * ((0.025f - 0.02f * flex) * sc);
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[SPINE] = P * 0.25f + ctrl * 0.5f + C * 0.25f;
    J[CHEST] = C;
    // Kopf: Blick, Wittern (Nase hoch), Fressen (zum Boden), Licht (weg)
    float wy = 0.0f, wp = -0.15f;
    if (in.look) yawPitch(C, in.lookAt, F, Lw, wy, wp);
    else if (idleW_ > 0.01f) wy = 0.5f * std::sin(time_ * 0.9f) * idleW_, wp = -0.2f + 0.15f * std::sin(time_ * 0.7f);
    if (perc_ > 0.01f) {
        float y2, p2;
        yawPitch(C, in.perceiveAt, F, Lw, y2, p2);
        wy += wrapPi(y2 - wy) * perc_;
        wp += (std::max(p2, 0.0f) + 0.5f * rear - wp) * perc_;
    }
    wp += (-0.95f - wp) * jaw_;
    if (light_ > 0.01f) {
        float ly, lp;
        yawPitch(C, in.lightFrom, F, Lw, ly, lp);
        wy += wrapPi((ly > 0.0f ? ly - 1.5f : ly + 1.5f) - wy) * light_ * 0.7f;
    }
    if (reach_ > 0.01f) {
        float ry, rp;
        yawPitch(C, in.reachAt, F, Lw, ry, rp);
        wy += wrapPi(ry - wy) * reach_;
        wp += (rp - wp) * reach_;
    }
    wy = std::clamp(wrapPi(wy), -1.0f, 1.0f);
    wp = std::clamp(wp, -1.1f, 0.9f);
    if (!chainInit_) headYaw_ = wy, headPitch_ = wp, headYawV_ = headPitchV_ = 0.0f;
    spring(headYaw_, headYawV_, wy, 10.0f, dt);
    spring(headPitch_, headPitchV_, wp, 10.0f, dt);
    const M3 Rn = axisAngle({0, 0, 1}, headYaw_ * 0.5f) * axisAngle({0, 1, 0}, -headPitch_ * 0.5f);
    const M3 Rh = axisAngle({0, 0, 1}, headYaw_) * axisAngle({0, 1, 0}, -headPitch_);
    // Schnueffeln in Stoessen (Nase zuckt)
    spring(sniff_, sniffV_, (idleW_ > 0.5f || perc_ > 0.3f) && std::fmod(time_, 1.1f) < 0.6f ? 1.0f : 0.0f, 12.0f, dt);
    J[HEAD] = C + Mb * (Rn * (B[HEAD] - B[CHEST]));
    J[NECK] = (C + J[HEAD]) * 0.5f + U * (0.004f * sc);
    J[SNOUT] = J[HEAD] + Mb * (Rh * (B[SNOUT] - B[HEAD])) + U * (0.0025f * sc * sniff_ * std::sin(time_ * 48.0f));
    const float open = jaw_ * (0.12f + 0.12f * std::sin(time_ * 30.0f)) + 0.55f * reach_ + 0.04f * sniff_ * std::sin(time_ * 24.0f);
    const M3 Rj = Rh * axisAngle({0, 1, 0}, std::max(0.0f, open));
    J[JAW] = J[HEAD] + Mb * (Rj * (B[JAW] - B[HEAD]));
    headPos_ = J[HEAD];
    headDir_ = Mb * (Rh * vec3{1, 0, 0});
    // Beine: vorn am Brustkorb (Ellbogen nach hinten), hinten am Becken (Knie nach vorn, Sprunggelenk hinten)
    for (int l = 0; l < 4; ++l) {
        const bool front = l < 2;
        const float sg = (l % 2 == 0) ? 1.0f : -1.0f;
        const vec3 base = front ? C : P;
        const vec3 T = base + Mb * (B[legJ(l, 0)] - B[front ? CHEST : PELVIS]);
        const float l1 = length(B[legJ(l, 1)] - B[legJ(l, 0)]), l2 = length(B[legJ(l, 2)] - B[legJ(l, 1)]);
        vec3 paw = g[l] + U * B[legJ(l, 2)].z;
        if (front && rear > 0.001f)  // aufgerichtet: Vorderpfoten vor der Brust
            paw = paw + (C + Mb * vec3{0.04f * sc, sg * 0.025f * sc, -0.045f * sc} - paw) * rear;
        if (front && reach_ > 0.001f) paw = paw + F * (0.04f * sc * reach_);
        vec3 knee, pw;
        ik2(T, paw, l1, l2, front ? safeNorm(-F - U * 0.2f, -F) : safeNorm(F + U * 0.1f, F), knee, pw);
        J[legJ(l, 0)] = T;
        J[legJ(l, 1)] = knee;
        J[legJ(l, 2)] = pw;
        const float sw = leg_[l].swing ? std::sin(kPi * leg_[l].t) : 0.0f;
        J[legJ(l, 3)] = pw + rotateAround(Mb * (B[legJ(l, 3)] - B[legJ(l, 2)]), Lw, 0.6f * sw + (front ? 0.9f * rear : 0.0f));
    }
    autoUps();
    pose_.up[rt::B_HEAD] = Mb * (Rh * sk_->bindUp[rt::B_HEAD]);
    pose_.up[rt::B_JAW] = Mb * (Rj * sk_->bindUp[rt::B_JAW]);
    // Schwanz: haengt mit Traegheit am Becken, schleift am Boden, pendelt im Lauf
    cols_.clear();
    {
        const int idx[kTail] = {TAIL0, TAIL0 + 1, TAIL0 + 2, TAIL0 + 3, TAIL0 + 4};
        const vec3 wind = Lw * ((moving ? 0.03f * std::sin(kTau * phase_) : 0.02f * std::sin(time_ * 1.3f)) * sc);
        hangChain(0, rt::B_PELVIS, idx, kTail, 15.0f, 0.84f, 1.8f, wind, dt);  // steif genug, dass er im Galopp nicht hochschlaegt
    }
    autoUps();
    pose_.up[rt::B_HEAD] = Mb * (Rh * sk_->bindUp[rt::B_HEAD]);
    pose_.up[rt::B_JAW] = Mb * (Rj * sk_->bindUp[rt::B_JAW]);
}

// --- V13.2: Fensterwesen -------------------------------------------------------------------------------------------
// Der Umriss wiegt sich kaum merklich und atmet; er wendet den Kopf dem Betrachter zu (look) und legt ihn schief. Er
// lockt mit der linken Hand (wave): Unterarm aufrecht, die Hand winkt zu sich heran. Greift er (reach), lehnt er sich
// aus dem Fenster und streckt den rechten Arm zum Ziel - laenger als ein Menschenarm (bis gut 2 m von der Wand).
void CreatureAnimator::updateWindow(float dt, const Input& in) {
    using namespace wn;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    spring(reach_, reachV_, in.reach, 7.0f, dt);  // der Arm schiesst heraus
    spring(wave_, waveV_, in.wave, 2.0f, dt);
    spring(perc_, percV_, in.perceive, 1.5f, dt);
    spring(tilt_, tiltV_, in.tilt, 1.5f, dt);
    for (size_t k = 0; k < B.size(); ++k) J[k] = pos_ + Mb * B[k];
    const float out = reach_;
    const float sw = (0.018f * std::sin(time_ * 0.45f) + 0.008f * std::sin(time_ * 1.13f + 1.0f)) * (1.0f - out);
    const float br = 0.004f * std::sin(time_ * 1.2f);
    // heraus lehnen: um die Huefte (Unterkante der Oeffnung) nach vorn, die Huefte ueber die Bank
    const M3 Rl = axisAngle({0, 1, 0}, 0.45f * out + 0.05f * perc_) * axisAngle({1, 0, 0}, sw * 1.5f);
    const vec3 base = J[FIG_BASE] + F * (0.3f * out);
    J[FIG_BASE] = base;
    J[FIG_CHEST] = base + Mb * (Rl * (B[FIG_CHEST] - B[FIG_BASE])) + U * br;
    J[FIG_NECK] = base + Mb * (Rl * (B[FIG_NECK] - B[FIG_BASE])) + U * br;
    float wy = 0.0f, wp = 0.0f;
    if (in.look) yawPitch(J[FIG_NECK], in.lookAt, F, Lw, wy, wp);
    if (!chainInit_) headYaw_ = 0.0f, headYawV_ = 0.0f;
    spring(headYaw_, headYawV_, std::clamp(wy, -0.5f, 0.5f) * (0.4f + 0.6f * perc_), 1.2f, dt);
    const M3 Rh = Rl * axisAngle({0, 0, 1}, headYaw_) * axisAngle({1, 0, 0}, tilt_ + 0.12f * std::sin(time_ * 0.3f) + 0.2f * perc_);
    J[FIG_TOP] = J[FIG_NECK] + Mb * (Rh * (B[FIG_TOP] - B[FIG_NECK]));
    for (int s = 0; s < 2; ++s) {
        const int sh = s == 0 ? SH_L : SH_R;
        const float l1 = length(B[sh + 1] - B[sh]), l2 = length(B[sh + 2] - B[sh + 1]), lh = length(B[sh + 3] - B[sh + 2]);
        const vec3 S0 = J[FIG_CHEST] + Mb * (Rl * (B[sh] - B[FIG_CHEST]));
        // Ruhe: haengt dicht am Koerper (die Ruhepose spreizt die Arme nur fuer das Vernetzen)
        const float sg = s == 0 ? 1.0f : -1.0f;
        vec3 Wt = S0 + Mb * (Rl * (normalize(vec3{0.02f, sg * 0.2f, -1.0f}) * ((l1 + l2) * 0.97f)));
        vec3 hdir = Mb * (Rl * normalize(vec3{0.0f, sg * 0.15f, -1.0f}));
        float k = 1.0f;  // Streckung des Arms
        vec3 pole = Mb * (Rl * vec3{-0.3f, s == 0 ? 1.0f : -1.0f, -0.4f});
        if (s == 0 && wave_ > 0.001f) {  // locken: Oberarm seitlich, Unterarm aufrecht, die Hand winkt heran
            const vec3 od = Mb * normalize(vec3{0.25f, 1.0f, -0.35f});
            const float curl = 0.9f * (0.5f + 0.5f * std::sin(time_ * kTau * 0.9f));
            const vec3 E = S0 + od * l1;
            const vec3 Wg = E + safeNorm(U + F * 0.15f, U) * l2;
            Wt = Wt + (Wg - Wt) * wave_;
            hdir = safeNorm(hdir + (rotateAround(U, Lw, -curl) - hdir) * wave_, U);
            pole = safeNorm(pole + (od - U * 0.5f - pole) * wave_, pole);
        }
        if (s == 1 && out > 0.001f) {  // greifen: zum Ziel, der Arm streckt sich
            const vec3 to = in.reachAt - S0;
            const float dl = length(to);
            k = 1.0f + (std::clamp(dl / ((l1 + l2) * 0.97f), 1.0f, 1.9f) - 1.0f) * out;
            const vec3 dirT = dl > 1e-4f ? to / dl : F;
            Wt = Wt + (S0 + dirT * std::min(dl, (l1 + l2) * k * 0.97f) - Wt) * out;
            hdir = safeNorm(hdir + (dirT - hdir) * out, dirT);
            pole = safeNorm(pole + (-U - pole) * out, pole);
        }
        vec3 e, w;
        ik2(S0, Wt, l1 * k, l2 * k, pole, e, w);
        J[sh] = S0;
        J[sh + 1] = e;
        J[sh + 2] = w;
        J[sh + 3] = w + hdir * lh;
    }
    autoUps();
    for (int b = wn::B_BODY; b <= wn::B_CHEST; ++b) pose_.up[(size_t)b] = Mb * (Rl * sk_->bindUp[(size_t)b]);
    pose_.up[wn::B_HEAD] = Mb * (Rh * sk_->bindUp[wn::B_HEAD]);
    pose_.up[wn::B_BACK] = Mb * sk_->bindUp[wn::B_BACK];
}

}  // namespace lim::anim
