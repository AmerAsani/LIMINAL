// V13.3 Level 3 (KREATUR_DeathRat.md, Abschnitt Level 3): Bewegung der grossen Death Rat und ihrer Stromfalle. Beide nutzen
// das Rattenskelett (rt), die Modelle ent_rat_l3 und ent_rat_trap (tools/blender/creatures/rat_l3.py, rat_trap.py).
//
// Grosse Ratte (extra_[0] = 2): wie ein Wasserschwein - der Leib um kG vergroessert und um kLift gehoben, die Beine dazu um
//   kLegK gestreckt (Massstab je Knochen; das Modell hat duenne, kurze Beine). Schwerfaelliger Passgang, im Lauf ein
//   rollender Galopp; der Leib schaukelt. Eingaben:
//     crouch   lauert: Leib tief, Kopf vor und unten, die Ohren angelegt
//     plea     sitzt auf den Keulen (Vorderbeine gestreckt, wie ein Wasserschwein) - wartet, spricht, wetzt
//     jaw      wetzt die Zaehne (schnelles Mahlen) - bei Fallenbau und am Gitter
//     strike   droht vor dem Biss: richtet die Brust auf, die Finger der Vorderpfoten gespreizt vor, Kopf hoch, Zaehne frei
//     reach    beisst: stoesst den Kopf zu reachAt, das Maul weit auf
//     talk     das Sprachgeraet spricht (der Mund bleibt zu - nur ein Zucken im Hals); perceive, light wie sonst
// Falle (extra_[0] = 3; [1..3] linker, [4..6] rechter Isolator in Weltkoordinaten, [7] gerissen 0/1, [8] Funke 0..1,
//   [9] Brummen 0..1): Brett und Quelle an der Wurzel (Gangquerung = Blickrichtung), die Isolatoren starr an den Waenden
//   (rechts um 180 Grad gedreht), der Draht durch die Schwanzgelenke mit Durchhang (schwingt beim Brummen); gerissen haengen
//   beide Haelften zu Boden. Der Funke (Knochen 5) wird mit 'Funke' eingeblendet.
#include <algorithm>
#include <cmath>

#include "anim/creature_anim.hpp"
#include "anim/skeleton.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kPiR = 3.14159265f, kTauR = 6.2831853f;
// grosse Ratte: Leib x kG (gut 1,1 m lang), gehoben um kLift (Bindungsmeter vor kG), Beine zusaetzlich x kLegK
constexpr float kG = 3.5f, kLegK = 1.75f, kLift = 0.043f;
vec3 dirR(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
vec3 nR(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
void spR(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
struct R3 {
    vec3 c0{1, 0, 0}, c1{0, 1, 0}, c2{0, 0, 1};
    vec3 operator*(vec3 v) const { return c0 * v.x + c1 * v.y + c2 * v.z; }
    R3 operator*(const R3& o) const { return R3{(*this) * o.c0, (*this) * o.c1, (*this) * o.c2}; }
};
R3 rotR(vec3 a, float ang) { return R3{rotateAround({1, 0, 0}, a, ang), rotateAround({0, 1, 0}, a, ang), rotateAround({0, 0, 1}, a, ang)}; }
float wrapR(float a) { return std::remainder(a, kTauR); }

// Zustand der grossen Ratte (ext_)
struct RatL3Anim {
    float sit = 0, sitV = 0, low = 0, lowV = 0, rear = 0, rearV = 0, grind = 0, grindV = 0, roll = 0, rollV = 0, talkTw = 0;
};
}  // namespace

float CreatureAnimator::ratL3Height() const {
    if (extra_.empty()) return sk_->height;
    if (extra_[0] > 2.5f) return 0.25f;  // Falle: Quelle und Isolatoren
    return std::max(0.35f, pose_.j[rt::HEAD].z - pos_.z + 0.12f);
}

void CreatureAnimator::updateRatL3(float dt, const Input& in) {
    using namespace rt;
    if (!ext_) ext_ = std::make_shared<RatL3Anim>();
    RatL3Anim& S = *static_cast<RatL3Anim*>(ext_.get());
    const auto& B0 = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirR(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const R3 Mb{F, Lw, U};
    // Ruhepose in dieser Groesse: Leib x kG; Bein l: Oberkante x kG, der Rest um sie x kG x kLegK
    vec3 B[JCOUNT];
    for (int j = 0; j < JCOUNT; ++j) B[j] = B0[(size_t)j] * kG;
    for (int l = 0; l < 4; ++l)
        for (int i = 1; i < 4; ++i) B[legJ(l, i)] = B[legJ(l, 0)] + (B0[(size_t)legJ(l, i)] - B0[(size_t)legJ(l, 0)]) * (kG * kLegK);
    const float lift = kLift * kG;
    auto W = [&](vec3 l) { return pos_ + Mb * l; };
    spR(jaw_, jawV_, in.jaw, 10.0f, dt);
    spR(perc_, percV_, in.perceive, 4.0f, dt);
    spR(reach_, reachV_, in.reach, 8.0f, dt);
    spR(light_, lightV_, in.light, 4.0f, dt);
    spR(idleW_, idleWV_, speed_ > 0.06f ? 0.0f : 1.0f, 2.5f, dt);
    spR(S.sit, S.sitV, in.plea * (speed_ > 0.15f ? 0.0f : 1.0f), 2.2f, dt);
    spR(S.low, S.lowV, in.crouch, 2.5f, dt);
    spR(S.rear, S.rearV, in.strike, 5.0f, dt);
    spR(talk_, talkV_, in.talk, 8.0f, dt);
    if (!run_ && speed_ > 1.5f) run_ = true;
    else if (run_ && speed_ < 1.1f) run_ = false;
    // Gang: Passgang (Wasserschwein), im Lauf ein rollender Galopp
    static const float walkOff[4] = {0.0f, 0.5f, 0.0f, 0.5f}, runOff[4] = {0.55f, 0.65f, 0.0f, 0.1f};
    const float* offs = run_ ? runOff : walkOff;
    const float duty = run_ ? 0.4f : 0.66f;
    const float S0 = run_ ? 0.55f + 0.12f * speed_ : 0.22f + 0.2f * speed_;
    vec3 rest[4];
    for (int l = 0; l < 4; ++l) rest[l] = W({B[legJ(l, 2)].x, B[legJ(l, 2)].y * 1.1f, 0.0f});
    const float wgt[4] = {0.5f, 0.5f, 0.6f, 0.6f};
    stepLegs(dt, 4, offs, duty, S0, rest, 0.3f, wgt);
    const float lift4[4] = {run_ ? 0.09f : 0.05f, run_ ? 0.09f : 0.05f, run_ ? 0.1f : 0.045f, run_ ? 0.1f : 0.045f};
    vec3 g[4];
    for (int i = 0; i < 4; ++i) g[i] = footPos(i, lift4);
    const bool moving = speed_ > 0.06f;
    // Leib: schaukelt im Passgang seitlich (Rollen), im Galopp Nicken
    float bobC = 0, bobP = 0, flex = 0, rollT = 0;
    if (run_) {
        bobC = 0.035f * std::sin(kTauR * phase_);
        bobP = 0.04f * std::sin(kTauR * phase_ + 2.2f);
        flex = std::sin(kTauR * phase_ - 0.4f);
    } else if (moving) {
        bobC = 0.012f * std::cos(4.0f * kPiR * phase_);
        bobP = 0.012f * std::cos(4.0f * kPiR * phase_ + 0.8f);
        rollT = 0.07f * std::sin(kTauR * phase_);
    } else {
        bobC = 0.006f * std::sin(time_ * 2.1f);  // schwerer Atem
        bobP = 0.003f * std::sin(time_ * 2.1f - 0.6f);
    }
    spR(S.roll, S.rollV, rollT, 6.0f, dt);
    const float sit = S.sit * (1.0f - S.rear), low = S.low * (1.0f - S.sit), rear = S.rear;
    const vec3 lunge = F * (0.22f * reach_);
    // Brust: gesessen hoch, lauernd tief, drohend aufgerichtet
    vec3 C = W({B[CHEST].x + flex * 0.05f - rear * 0.12f - sit * 0.1f, 0.0f, 0.0f}) + lunge;
    C.z = pos_.z + B[CHEST].z + lift + bobC + sit * 0.17f - low * 0.07f + rear * 0.26f - 0.03f * light_;
    vec3 P = W({B[PELVIS].x - flex * 0.05f + sit * 0.08f + rear * 0.04f, 0.0f, 0.0f}) + lunge * 0.3f;
    P.z = pos_.z + B[PELVIS].z + lift + bobP - sit * (B[PELVIS].z + lift - 0.17f) - low * 0.04f - rear * 0.05f;
    const vec3 ctrl = (P + C) * 0.5f + U * ((0.04f - 0.03f * flex) * (1.0f - sit));
    const vec3 Lr = rotateAround(Lw, F, S.roll);
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[SPINE] = P * 0.25f + ctrl * 0.5f + C * 0.25f;
    J[CHEST] = C;
    // Kopf: Blick, Wittern, Licht (wendet den Kopf ab, kneift), lauern (vor und unten), drohen (hoch), beissen (zum Ziel)
    float wy = 0.0f, wp = -0.1f;
    if (in.look) {
        const vec3 d = in.lookAt - C;
        wy = std::atan2(dot(d, Lw), dot(d, F)), wp = std::atan2(d.z, std::hypot(dot(d, F), dot(d, Lw)));
    } else if (idleW_ > 0.01f) {
        wy = 0.35f * std::sin(time_ * 0.5f) * idleW_, wp = -0.15f + 0.1f * std::sin(time_ * 0.37f);
    }
    if (perc_ > 0.01f) {
        const vec3 d = in.perceiveAt - C;
        const float y2 = std::atan2(dot(d, Lw), dot(d, F)), p2 = std::atan2(d.z, std::hypot(dot(d, F), dot(d, Lw)));
        wy += wrapR(y2 - wy) * perc_;
        wp += (p2 - wp) * perc_;
    }
    wp += (-0.5f - wp) * low * 0.6f + (0.35f - wp) * rear * 0.7f;
    if (light_ > 0.01f) {
        const vec3 d = in.lightFrom - C;
        const float ly = std::atan2(dot(d, Lw), dot(d, F));
        wy += wrapR((ly > 0.0f ? ly - 1.2f : ly + 1.2f) - wy) * light_ * 0.6f;
        wp += (-0.3f - wp) * light_ * 0.4f;
    }
    if (reach_ > 0.01f) {
        const vec3 d = in.reachAt - C;
        const float ry = std::atan2(dot(d, Lw), dot(d, F)), rp = std::atan2(d.z, std::hypot(dot(d, F), dot(d, Lw)));
        wy += wrapR(ry - wy) * reach_;
        wp += (rp - wp) * reach_;
    }
    wy = std::clamp(wrapR(wy), -1.1f, 1.1f);
    wp = std::clamp(wp, -0.9f, 0.9f);
    if (!chainInit_) headYaw_ = wy, headPitch_ = wp, headYawV_ = headPitchV_ = 0.0f;
    spR(headYaw_, headYawV_, wy, 6.0f, dt);  // schwerer Kopf: langsamer als die kleine Ratte
    spR(headPitch_, headPitchV_, wp, 6.0f, dt);
    const R3 Rn = rotR({0, 0, 1}, headYaw_ * 0.45f) * rotR({0, 1, 0}, -headPitch_ * 0.45f);
    const R3 Rh = rotR({0, 0, 1}, headYaw_) * rotR({0, 1, 0}, -headPitch_);
    // Wetzen: schnelles Mahlen; Sprechen: ein Zucken im Hals (das Geraet spricht, nicht der Mund)
    spR(S.grind, S.grindV, jaw_ > 0.3f ? 1.0f : 0.0f, 12.0f, dt);
    S.talkTw = talk_ * 0.012f * std::sin(time_ * 37.0f) * (0.5f + 0.5f * std::sin(time_ * 5.3f));
    J[HEAD] = C + Mb * (Rn * (B[HEAD] - B[CHEST])) + U * S.talkTw;
    J[NECK] = (C + J[HEAD]) * 0.5f + U * 0.012f;
    J[SNOUT] = J[HEAD] + Mb * (Rh * (B[SNOUT] - B[HEAD]));
    const float open = 0.06f * S.grind * (0.5f + 0.5f * std::sin(time_ * 26.0f)) + 0.62f * reach_ + 0.32f * rear;
    const R3 Rj = Rh * rotR({0, 1, 0}, std::max(0.0f, open));
    J[JAW] = J[HEAD] + Mb * (Rj * (B[JAW] - B[HEAD]));
    headPos_ = J[HEAD];
    headDir_ = Mb * (Rh * vec3{1, 0, 0});
    // Beine: IK von der Oberkante (an Brust/Becken) zur Pfote. Sitzend: die Keulen unter dem Becken, die Pfoten vorn
    for (int l = 0; l < 4; ++l) {
        const bool front = l < 2;
        const float sg = (l % 2 == 0) ? 1.0f : -1.0f;
        const vec3 base = front ? C : P;
        const vec3 T = base + Mb * (B[legJ(l, 0)] - B[front ? CHEST : PELVIS]) + Lr * (0.06f * S.roll * sg);
        const float l1 = length(B[legJ(l, 1)] - B[legJ(l, 0)]), l2 = length(B[legJ(l, 2)] - B[legJ(l, 1)]);
        const float pawH = B[legJ(l, 2)].z * 0.9f;
        vec3 paw = g[l] + U * pawH;
        if (!front && sit > 0.001f) {  // sitzt: die Hinterpfoten flach neben dem Becken, die Ferse am Boden
            const vec3 sp = W({B[PELVIS].x + 0.18f, sg * (B[legJ(l, 0)].y + 0.03f), 0.0f}) + U * pawH;
            paw = paw + (sp - paw) * sit;
        }
        if (front && rear > 0.001f)  // droht: die Vorderpfoten hoch vor der Brust, Finger vor
            paw = paw + (C + Mb * vec3{0.26f, sg * 0.16f, -0.05f} - paw) * rear;
        if (front && reach_ > 0.001f) paw = paw + F * (0.12f * reach_);
        vec3 knee, pw;
        const vec3 pole = front ? nR(-F * 0.6f - U * 0.3f + Lw * (sg * 0.2f), -F) : nR(F + U * 0.15f + Lw * (sg * 0.15f), F);
        ik2(T, paw, l1, l2, pole, knee, pw);
        J[legJ(l, 0)] = T;
        J[legJ(l, 1)] = knee;
        J[legJ(l, 2)] = pw;
        const float sw = leg_[l].swing ? std::sin(kPiR * leg_[l].t) : 0.0f;
        const float curl = front ? 0.9f * rear + 0.5f * sw : 0.6f * sw - 0.4f * sit;  // Finger heben sich im Schritt
        J[legJ(l, 3)] = pw + rotateAround(Mb * (B[legJ(l, 3)] - B[legJ(l, 2)]), Lw, curl);
    }
    // Schwanz: kein Netz (Wasserschwein) - starr am Becken
    for (int i = 0; i < kTail; ++i) J[TAIL0 + i] = P + Mb * (B[TAIL0 + i] - B[PELVIS]);
    autoUps();
    pose_.up[rt::B_HEAD] = Mb * (Rh * sk_->bindUp[rt::B_HEAD]);
    pose_.up[rt::B_JAW] = Mb * (Rj * sk_->bindUp[rt::B_JAW]);
    // Rollen im Passgang: Becken und Ruecken drehen sich um ihre Laengsachse (das Fett schwingt)
    for (int b : {(int)rt::B_PELVIS, (int)rt::B_SPINE}) {
        const vec3 ax = nR(J[sk_->bones[(size_t)b].b] - J[sk_->bones[(size_t)b].a], F);
        pose_.up[(size_t)b] = rotateAround(pose_.up[(size_t)b], ax, S.roll * (b == rt::B_PELVIS ? 1.0f : 0.6f));
    }
    // Massstab je Knochen: Leib kG, Beine kG x kLegK
    pose_.s.assign(sk_->bones.size(), kG);
    for (int l = 0; l < 4; ++l)
        for (int i = 0; i < 3; ++i) pose_.s[(size_t)legB(l, i)] = kG * kLegK;
}

void CreatureAnimator::updateRatTrap(float dt, const Input& in) {
    using namespace rt;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const auto x = [&](size_t i) { return i < extra_.size() ? extra_[i] : 0.0f; };
    const vec3 F = dirR(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const R3 Mb{F, Lw, U}, Mr{-F, -Lw, U};  // rechts: um 180 Grad gedreht (der Nagel zeigt in die andere Wand)
    const vec3 aL{x(1), x(2), x(3)}, aR{x(4), x(5), x(6)};
    const bool snapped = x(7) > 0.5f;
    const float spark = x(8), hum = x(9);
    for (vec3& j : J) j = pos_;
    J[ROOT] = pos_;
    // Isolatoren: die Spule an der Wandflaeche
    J[legJ(0, 0)] = aL;
    J[legJ(0, 1)] = aL + Mb * (B[legJ(0, 1)] - B[legJ(0, 0)]);
    J[legJ(1, 0)] = aR;
    J[legJ(1, 1)] = aR + Mr * (B[legJ(1, 1)] - B[legJ(1, 0)]);
    for (int i = 2; i < 4; ++i) J[legJ(0, i)] = J[legJ(0, 1)], J[legJ(1, i)] = J[legJ(1, 1)];
    // Draht: von Schlinge zu Schlinge (1,5 cm vor der Wand), Durchhang 1,5 cm; das Brummen laesst ihn zittern
    const vec3 across = nR(aR - aL, F);
    const vec3 w0 = aL + across * 0.015f, w1 = aR - across * 0.015f;
    const float span = length(w1 - w0);
    for (int i = 0; i < kTail; ++i) {
        const float t = (float)i / (float)(kTail - 1);
        vec3 p = w0 + (w1 - w0) * t;
        const float sag = 4.0f * t * (1.0f - t);
        p.z -= (0.008f + 0.004f * span) * sag;
        p += (U * std::sin(time_ * 120.0f + 2.3f * (float)i) + Lw * std::sin(time_ * 97.0f + 1.7f * (float)i)) * (0.0012f * hum * sag);
        J[TAIL0 + i] = p;
    }
    if (snapped) {  // gerissen: die Haelften haengen von den Isolatoren zu Boden (das mittlere Stueck verschwindet)
        const float floorZ = pos_.z + 0.004f;
        const vec3 dl = across * 0.12f, dr = -across * 0.12f;
        J[TAIL0 + 1] = w0 + dl * 0.6f + U * ((floorZ - w0.z) * 0.6f);
        J[TAIL0 + 2] = vec3{w0.x + dl.x * 1.6f, w0.y + dl.y * 1.6f, floorZ};
        J[TAIL0 + 3] = vec3{w1.x + dr.x * 1.6f, w1.y + dr.y * 1.6f, floorZ};
        J[TAIL0 + 4] = w1;
    }
    // Funke am Unterbrecher (Wurzelrahmen: 0,17 m vor der Wand, 3,5 cm hoch)
    const vec3 sp = pos_ + Mb * vec3{0.17f, 0.0f, 0.036f};
    J[HEAD] = sp;
    J[JAW] = sp + Mb * (B[JAW] - B[HEAD]);
    // Oben-Richtungen und Massstab
    for (size_t b = 0; b < sk_->bones.size(); ++b) pose_.up[b] = Mb * sk_->bindUp[b];
    pose_.up[rt::B_ROOT] = F;
    pose_.up[legB(1, 0)] = Mr * sk_->bindUp[(size_t)legB(1, 0)];
    pose_.s.assign(sk_->bones.size(), 1.0f);
    for (int i = 0; i < kTail - 1; ++i) {
        const float bl = length(B[TAIL0 + i + 1] - B[TAIL0 + i]);
        const vec3 d = J[TAIL0 + i + 1] - J[TAIL0 + i];
        const float len = length(d);
        pose_.s[(size_t)(rt::B_TAIL0 + i)] = std::max(0.001f, len / std::max(1e-4f, bl));
        pose_.up[(size_t)(rt::B_TAIL0 + i)] = nR(U - d * (dot(U, d) / std::max(1e-6f, len * len)), Lw);
    }
    if (snapped) pose_.s[(size_t)(rt::B_TAIL0 + 2)] = 0.001f;
    // der Funke: blitzt, zuckt in der Groesse; sonst unsichtbar
    const float flick = 0.7f + 0.3f * std::sin(time_ * 91.0f) * std::sin(time_ * 57.0f);
    pose_.s[(size_t)rt::B_JAW] = spark > 0.02f ? std::max(0.001f, spark * flick * 1.3f) : 0.001f;
    (void)dt;
    (void)in;
}

}  // namespace lim::anim
