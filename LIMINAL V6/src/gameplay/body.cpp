#include "gameplay/body.hpp"

#include <algorithm>
#include <cmath>

#include "gameplay/visible.hpp"

namespace lim::game {

namespace {

constexpr double kPiD = 3.141592653589793;

// Proportionen des Strichmaennchens (Meter). Augenhoehe = Player::kEye.
constexpr float kLine = 0.02f;        // Strichstaerke (Radius)
constexpr float kHip = 0.94f;         // Beckenhoehe im Stand
constexpr float kSpine = 0.55f;       // Becken -> Halsansatz
constexpr float kNeck = 0.06f;
constexpr float kHeadR = 0.11f;       // runder Kopf
constexpr float kUpperArm = 0.30f, kForearm = 0.28f;
constexpr float kThigh = 0.47f, kShin = 0.45f;
constexpr float kHipW = 0.075f, kShoulderW = 0.11f, kFootW = 0.11f;
constexpr float kAnkle = 0.055f;

float smoothT(float t) {
    t = saturate(t);
    return t * t * (3.0f - 2.0f * t);
}

// Zwei-Knochen-IK: A Wurzel, T Ziel, pole: Richtung, in die das mittlere Gelenk zeigt
void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end) {
    vec3 d = T - A;
    float dist = length(d);
    vec3 dir = dist > 1e-5f ? d / dist : vec3(0, 0, -1);
    dist = std::clamp(dist, std::fabs(l1 - l2) + 1e-3f, l1 + l2 - 1e-4f);
    float a = (l1 * l1 - l2 * l2 + dist * dist) / (2.0f * dist);
    float h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
    vec3 pp = pole - dir * dot(pole, dir);
    float pl = length(pp);
    if (pl < 1e-4f) {
        pp = cross(dir, vec3(0, 0, 1));
        pl = length(pp);
        if (pl < 1e-4f) pp = vec3(1, 0, 0), pl = 1.0f;
    }
    pp = pp / pl;
    mid = A + dir * a + pp * h;
    end = A + dir * dist;
}

mat4 columns(vec3 x, vec3 y, vec3 z, vec3 pos) {
    mat4 m;
    m.c[0] = vec4(x, 0);
    m.c[1] = vec4(y, 0);
    m.c[2] = vec4(z, 0);
    m.c[3] = vec4(pos, 1);
    return m;
}

// Einheitszylinder (x: 0..1, Radius 1) als Strich von a nach b
mat4 segment(vec3 a, vec3 b, float r) {
    vec3 d = b - a;
    float len = length(d);
    vec3 x = len > 1e-6f ? d / len : vec3(0, 0, 1);
    vec3 helper = std::fabs(x.z) < 0.9f ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 y = normalize(cross(helper, x));
    vec3 z = cross(x, y);
    return columns(x * std::max(len, 1e-4f), y * r, z * r, a);
}

mat4 sphere(vec3 c, float r) { return mat4::translation(c) * mat4::scale(vec3(r)); }

void follow(double& v, double target, double rate, double dt) { v += (target - v) * std::min(1.0, dt * rate); }

}  // namespace

void Body::gesture(Gesture g, vec3 target, const std::string& itemModel, float holdOffset, float scale) {
    gesture_ = g;
    gestureT_ = 0.0;
    gestureTarget_ = target;
    if (heldModel_ != itemModel) idHeld_ = -2;
    heldModel_ = itemModel;
    heldOffset_ = holdOffset;
    heldScale_ = scale;
}

mat4 Body::rootMatrix() const { return mat4::translation(root_) * mat4::rotationZ((float)yaw_); }

void Body::solveLeg(int i, vec3 target, const vec3& kneeDir) {
    ik2(pose_.hip[i], target, kThigh, kShin, kneeDir, pose_.knee[i], pose_.ankle[i]);
}

void Body::solveArm(int i, vec3 target, const vec3& pole) {
    ik2(pose_.shoulder[i], target, kUpperArm, kForearm, pole, pose_.elbow[i], pose_.hand[i]);
}

void Body::update(double dt, const Player& p) {
    dt = std::clamp(dt, 0.0, 0.1);
    time_ += dt;
    if (!initialized_) {
        yaw_ = p.angle;
        squash_ = squashV_ = 0.0;
        lean_ = side_ = sprint_ = air_ = rise_ = fall_ = flail_ = rollS_ = 0.0;
        spd_ = 0.0;
        gesture_ = Gesture::None;
        initialized_ = true;
    }
    // Der Koerper folgt dem Blick kurz verzoegert (hoechstens 0,6 rad verdreht)
    double diff = std::remainder(p.angle - yaw_, 2.0 * kPiD);
    if (std::fabs(diff) > 0.6) {
        yaw_ += diff - std::copysign(0.6, diff);
        diff = std::copysign(0.6, diff);
    }
    yaw_ += diff * std::min(1.0, dt * 14.0);
    const float cy = (float)std::cos(yaw_), sy = (float)std::sin(yaw_);
    const vec3 F{cy, sy, 0}, R{-sy, cy, 0}, U{0, 0, 1};
    root_ = {(float)p.x, (float)p.y, (float)(p.z + p.camStep)};

    // Bewegung im Koerperraum
    double vf = p.vx * F.x + p.vy * F.y, vr = p.vx * R.x + p.vy * R.y;
    double hs = std::hypot(vf, vr);
    if (hs > 0.15) dirF_ = vf / hs, dirR_ = vr / hs;
    follow(spd_, p.onGround ? p.groundSpeed : hs, 10.0, dt);

    // Zustaende weich nachfuehren
    follow(sprint_, p.sprinting ? 1.0 : 0.0, 6.0, dt);
    follow(air_, p.onGround ? 0.0 : 1.0, p.onGround ? 16.0 : 9.0, dt);
    follow(rise_, (!p.onGround && p.vz > 0.4) ? 1.0 : 0.0, 10.0, dt);
    follow(fall_, (!p.onGround && p.vz < -0.5) ? std::min(1.0, p.airTime / 0.6) : 0.0, 5.0, dt);
    // Atemnot: vorgebeugt, im Stehen deutlicher als im Lauf
    follow(lean_, 0.045 * p.gait + 0.15 * sprint_ - 0.06 * rise_ + p.breathless * 0.16 * (1.0 - 0.6 * p.gait), 5.0, dt);
    breathPhase_ += dt * (1.7 + 7.5 * p.breathless);
    follow(side_, std::clamp(vr / 3.0, -1.0, 1.0) * 0.06 * (1.0 - air_), 6.0, dt);

    // Federn in den Knien: Landung staucht, Absprung streckt (gedaempfte Feder, Teilschritte)
    if (p.landed) squashV_ -= 0.35 + 2.6 * p.landImpact;
    if (p.jumped) squashV_ += 0.5;
    for (double rest = dt; rest > 1e-6;) {
        double h = std::min(rest, 0.008);
        const double k = 170.0, c = 2.0 * std::sqrt(k) * 0.55;
        squashV_ += (-k * squash_ - c * squashV_) * h;
        squash_ += squashV_ * h;
        rest -= h;
    }
    squash_ = std::clamp(squash_, -0.38, 0.12);

    // --- Becken und Wirbelsaeule ---------------------------------------------------------
    const double phase = p.bobPhase;
    const float gait = (float)std::clamp(p.gait, 0.0, 1.0);
    const float sprint = (float)sprint_, air = (float)air_, rise = (float)rise_, fall = (float)fall_;
    float bob = -(float)std::cos(2.0 * phase) * (0.03f + 0.025f * sprint) * gait * (1.0f - air);
    // Atmen: ruhig im Stehen, nach dem Sprinten schnelles, tiefes Keuchen
    float breathe = (float)std::sin(breathPhase_) * (0.006f * (1.0f - gait) + 0.016f * (float)p.breathless);
    float pelvisH = kHip + bob + breathe + (float)squash_ - 0.05f * sprint + 0.04f * air * rise;
    vec3& pelvis = pose_.pelvis;
    pelvis = root_ + U * pelvisH;
    vec3 spineDir = normalize(U + F * (float)std::tan(lean_) + R * (float)side_);
    pose_.neck = pelvis + spineDir * kSpine;
    pose_.chest = pose_.neck - spineDir * 0.05f;
    pose_.fwd = F, pose_.right = R, pose_.up = U;

    // Kopf: neigt sich mit dem Blick (gedaempft, damit er nicht abknickt)
    float hp = (float)std::clamp(p.pitch * 0.7, -0.85, 0.85);
    pose_.headFwd = F * std::cos(hp) + U * std::sin(hp);
    pose_.headUp = U * std::cos(hp) - F * std::sin(hp);
    pose_.head = pose_.neck + spineDir * kNeck + pose_.headUp * kHeadR;

    // Kamera im Kopf: Nackengelenk + Augenversatz, voll mit dem Blick geneigt. Beim Blick nach
    // unten wandert das Auge etwas nach vorn und tiefer: Schultern und Oberarme bleiben hinter der
    // Kamera, im Bild liegen Haende, Becken, Knie und Fuesse.
    {
        float cp = (float)std::cos(p.pitch), spc = (float)std::sin(p.pitch);
        vec3 hf = F * cp + U * spc, hu = U * cp - F * spc;
        vec3 neckCam = pose_.neck;
        if (!p.bobEnabled) neckCam -= U * (bob + breathe + (float)squash_ * 0.5f);  // ohne Wippen, Landung halb
        eye_ = neckCam + hu * 0.13f + hf * 0.07f;
        follow(rollS_, p.bobEnabled ? -side_ * 0.2 - p.turnV * 0.004 : 0.0, 8.0, dt);
        roll_ = (float)rollS_;
        fovKick_ = p.bobEnabled ? sprint * 0.06f : 0.0f;
    }

    // --- Beine ---------------------------------------------------------------------------
    // Gangzyklus aus der Schrittphase: Fuss i setzt bei phase = k*pi auf (Schrittgeraeusch)
    const double omega = 5.5 + 1.3 * spd_;
    const float stride = (float)std::clamp(spd_ / omega, 0.0, 0.5);
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? -1.0f : 1.0f;
        pose_.hip[i] = pelvis + R * (side * kHipW);
        double psi = phase + kPiD * 0.5 + i * kPiD;
        float sw = (float)std::sin(psi), cw = (float)std::cos(psi);
        float lift = (0.09f + 0.08f * sprint) * gait * std::pow(std::max(0.0f, cw), 1.3f);
        float off = stride * sw;
        vec3 foot = root_ + F * (0.03f + off * (float)dirF_) + R * (side * kFootW + off * (float)dirR_) +
                    U * (kAnkle + lift);
        // in der Luft: beim Steigen Knie anziehen, beim Fallen strampeln
        if (air > 0.001f) {
            float kick = (float)std::sin(time_ * 13.0 + i * kPiD) * 0.2f * fall;
            vec3 tuck = pelvis + R * (side * 0.13f) + F * 0.12f - U * 0.48f;
            vec3 dangle = pelvis + R * (side * 0.16f) + F * kick - U * (0.80f - 0.12f * std::fabs(kick));
            vec3 airFoot = lerp(dangle, tuck, rise);
            foot = lerp(foot, airFoot, air);
        }
        // Landung: Fuesse bleiben am Boden, die Knie federn
        solveLeg(i, foot, F + R * (side * 0.15f));
        vec3 toeDir = normalize(F * (1.0f - 0.5f * air) - U * (0.25f * air + 0.4f * lift));
        pose_.toe[i] = pose_.ankle[i] + toeDir * 0.13f;
    }

    // --- Arme ----------------------------------------------------------------------------
    double flailBase = time_ * (8.0 + 6.0 * fall);
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? -1.0f : 1.0f;
        pose_.shoulder[i] = pose_.chest + R * (side * kShoulderW);
        // Arm i schwingt mit dem gegenueberliegenden Bein; beim Sprinten angewinkelt auf Brusthoehe
        double legPsi = phase + kPiD * 0.5 + (1 - i) * kPiD;
        double theta = (0.32 + 0.3 * sprint) * gait * std::sin(legPsi) + 0.06 + 0.06 * sprint;
        double bend = 0.2 + 0.12 * gait + 1.0 * sprint;
        // erschoepft im Stehen: Arme haengen schlaff nach vorn (Richtung Knie)
        theta += 0.35 * p.breathless * (1.0 - gait);
        bend += 0.25 * p.breathless * (1.0 - gait);
        double abd = 0.13 + 0.04 * gait + 0.16 * sprint + (double)std::sin(time_ * 0.9 + i) * 0.015 * (1.0 - gait);
        if (air > 0.001f) {
            // Steigen: Arme schwungvoll nach vorn oben ("Juhu!"). Fallen: Rudern (Windmuehle)
            double fa = flailBase + i * kPiD;
            double thetaAir = (1.0 - fall) * 2.0 + fall * (1.5 + 1.0 * std::sin(fa));
            double abdAir = (1.0 - fall) * 0.55 + fall * (0.95 + 0.25 * std::cos(fa));
            theta += (thetaAir - theta) * air;
            bend += (0.3 - bend) * air;
            abd += (abdAir - abd) * air;
        }
        // Landung: Arme kurz nach vorn zum Abfangen
        theta += -squash_ * 1.8;
        bend += -squash_ * 1.4;
        auto dirOf = [&](double ang) {
            float ca = (float)std::cos(abd), sa = (float)std::sin(abd);
            return normalize((F * (float)std::sin(ang) - U * (float)std::cos(ang)) * ca + R * (side * sa));
        };
        pose_.elbow[i] = pose_.shoulder[i] + dirOf(theta) * kUpperArm;
        pose_.hand[i] = pose_.elbow[i] + dirOf(theta + bend) * kForearm;
    }

    // --- Gesten (rechte Hand) ------------------------------------------------------------
    showHeld_ = false;
    if (gesture_ != Gesture::None) {
        gestureT_ += dt;
        float t = (float)gestureT_, w = 0.0f;
        vec3 target = gestureTarget_;
        float cp = (float)std::cos(p.pitch), spc = (float)std::sin(p.pitch);
        vec3 hf = F * cp + U * spc, hu = U * cp - F * spc;
        if (gesture_ == Gesture::Grab) {
            const float dur = 0.55f;
            w = t < 0.2f ? smoothT(t / 0.2f) : smoothT((dur - t) / (dur - 0.2f));
            // auf dem Rueckweg haelt die Hand das Item
            if (t > 0.2f) target = eye_ + hf * 0.42f - hu * 0.24f + R * 0.17f;
            showHeld_ = t > 0.18f && t < dur - 0.06f;
            if (t >= dur) gesture_ = Gesture::None;
        } else if (gesture_ == Gesture::Toss) {
            // Ablegen: kurzer Wurf aus dem Handgelenk - Hand schnellt nach vorn und sinkt zurueck
            const float dur = 0.45f;
            w = t < 0.1f ? smoothT(t / 0.1f) : smoothT((dur - t) / (dur - 0.1f));
            target = eye_ + hf * (0.28f + 0.22f * smoothT(t / 0.16f)) - hu * (0.24f - 0.08f * smoothT(t / 0.16f)) + R * 0.15f;
            if (t >= dur) gesture_ = Gesture::None;
        } else {
            const float dur = 1.05f;
            w = t < 0.25f ? smoothT(t / 0.25f) : (t > 0.8f ? smoothT((dur - t) / (dur - 0.8f)) : 1.0f);
            // vor dem Gesicht, leicht rechts unten; "Mampfen" bewegt die Hand Richtung Mund
            float munch = t > 0.25f && t < 0.8f ? std::max(0.0f, (float)std::sin((t - 0.25f) * 19.0f)) * 0.05f : 0.0f;
            target = eye_ + hf * (0.44f - munch) - hu * (0.19f - munch * 0.8f) + R * 0.09f;
            showHeld_ = t < 0.82f;
            if (t >= dur) gesture_ = Gesture::None;
        }
        if (gesture_ != Gesture::None || w > 0.0f) {
            vec3 goal = lerp(pose_.hand[1], target, w);
            vec3 pole = normalize(pose_.elbow[1] - pose_.shoulder[1]) * 0.3f - U + R * 0.6f;
            solveArm(1, goal, pole);
        }
        if (showHeld_) {
            vec3 x = R, z = normalize(hu * 0.85f + hf * 0.15f), y = cross(z, x);
            if (gesture_ == Gesture::Use && t > 0.25f) {  // zum Mund kippen
                float tilt = 0.55f * smoothT((t - 0.25f) / 0.2f);
                z = normalize(z * std::cos(tilt) - y * std::sin(tilt));
                y = cross(z, x);
            }
            heldRot_ = columns(x * heldScale_, y * heldScale_, z * heldScale_, vec3(0, 0, 0));
            heldPos_ = pose_.hand[1] + hf * 0.02f - z * (heldOffset_ * heldScale_);
        }
    }
}

void Body::collect(std::vector<VisibleMesh>& out, bool firstPerson) {
    auto add = [&](std::string* model, int* id, const mat4& m) {
        VisibleMesh v;
        v.model = model;
        v.meshId = id;
        v.world = m;
        v.attached = true;
        out.push_back(v);
    };
    const BodyPose& b = pose_;
    auto line = [&](vec3 a, vec3 c, float r = kLine) { add(&mLimb_, &idLimb_, segment(a, c, r)); };
    auto joint = [&](vec3 c, float r = kLine) { add(&mJoint_, &idJoint_, sphere(c, r)); };

    // Rumpf: Wirbelsaeule mit kleinem Schulter- und Hueftbalken
    line(b.pelvis, b.neck);
    line(b.shoulder[0], b.shoulder[1]);
    line(b.hip[0], b.hip[1]);
    joint(b.neck);
    for (int i = 0; i < 2; ++i) {
        // Beine
        line(b.hip[i], b.knee[i]);
        line(b.knee[i], b.ankle[i]);
        joint(b.hip[i]);
        joint(b.knee[i]);
        // Fuss: kurzer Strich in gleicher Staerke, rund abgeschlossen
        line(b.ankle[i], b.toe[i]);
        joint(b.ankle[i]);
        joint(b.toe[i]);
        // Arme: enden einfach als Strich (rundes Ende in Strichstaerke, keine Hand-Knolle)
        line(b.shoulder[i], b.elbow[i]);
        line(b.elbow[i], b.hand[i]);
        joint(b.shoulder[i]);
        joint(b.elbow[i]);
        joint(b.hand[i]);
    }
    if (!firstPerson) {
        // Kopf mit hellen Punktaugen und Grinsen (in der Ego-Perspektive sitzt die Kamera darin)
        line(b.neck, b.head - b.headUp * (kHeadR * 0.9f));
        add(&mHead_, &idHead_, sphere(b.head, kHeadR));
        vec3 f = b.headFwd, u = b.headUp, r = b.right;
        for (float s : {-1.0f, 1.0f})
            add(&mEye_, &idEye_, sphere(b.head + f * (kHeadR * 0.86f) + r * (s * kHeadR * 0.36f) + u * (kHeadR * 0.25f),
                                        kHeadR * 0.13f));
        const float k = kHeadR / 0.14f;  // Grinsen ist fuer einen 0,14-m-Kopf gebaut
        add(&mSmile_, &idSmile_, columns(r * k, cross(u, r) * k, u * k, b.head + f * (kHeadR * 0.84f) - u * (kHeadR * 0.22f)));
    }
    if (showHeld_ && !heldModel_.empty()) {
        mat4 m = heldRot_;
        m.c[3] = vec4(heldPos_, 1.0f);
        add(&heldModel_, &idHeld_, m);
    }
}

}  // namespace lim::game
