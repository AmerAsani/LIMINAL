// V13.3 Level 3 (KREATUR_RedKnight.md): der Red Knight (Wikidot entity-71, Rev. 44; level-3, Rev. 18, Vorfall GB-03) im
// Skelett des Wretch (Zweibeiner bp, Variante 4 - keine eigene Rolle). Nach updateBiped (Stil "rk": aufrecht, weit
// ausgreifend, keuchend) setzt diese Datei die Arme und das Schwert:
//
//   Schwert   ein Zweihaender, an den rechten Handknochen gebunden (Modell redknight.py): Griffmitte 8,5 cm vor dem Handgelenk,
//             2 cm zur Handflaeche, die Klinge zeigt zur Daumenseite. Spitze 1,22 m vor der Griffmitte, Knauf 0,36 m dahinter.
//   Tragen    (Grundhaltung, auch im Lauf) die Klinge liegt auf der rechten Schulter, schraeg nach hinten oben - die Seite: das
//             Schwert ist zu gross fuer die Huefte, er traegt es auf der Schulter. Die linke Hand pumpt im Lauf.
//   Hieb      (strike 0..1, wave < 0,5) beidhaendig: ueber die rechte Schulter ausholen (0..0,45), schraeg nach links unten
//             durchziehen (0,45..0,7), Nachschwung - gegen etwas, das man nicht sieht (der Verfolger, Interpretation).
//   Schnitt   (strike mit wave > 0,5) der Schnitt in die Luft, der den Durchgang oeffnet: von links unten schraeg nach rechts
//             oben (die Seite: ein wilder Hieb, der einen Mann von der Huefte bis zum Schluesselbein teilen wuerde).
//   Stuetzen  (crouch > 0,5) erschoepft auf das Schwert gestuetzt: die Spitze vor ihm am Boden, beide Haende am Griff, der Kopf
//             gesenkt (die Seite: er lehnt keuchend auf seiner langen Klinge).
//   Panik     (alert) der Atem fliegt, der Kopf zuckt zurueck; der Blick zurueck kommt aus perceive (updateBiped).
#include <algorithm>
#include <cmath>
#include <memory>

#include "anim/creature_anim.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kPiR = 3.14159265f;
vec3 dirR(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
float sR(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
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
vec3 rotR(vec3 v, vec3 axis, float ang) {
    const float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
vec3 arcR(vec3 v, vec3 d0, vec3 d1) {
    const vec3 ax = cross(d0, d1);
    const float s = length(ax), c = dot(d0, d1);
    if (s < 1e-6f) return c > 0.0f ? v : v * -1.0f;
    return rotR(v, ax / s, std::atan2(s, c));
}
struct KGrip {
    vec3 g, d, p;  // Griffmitte (rechte Hand), Richtung der Klinge, Handflaeche
};
KGrip mixG(const KGrip& a, const KGrip& b, float t) {
    return {a.g + (b.g - a.g) * t, nR(a.d + (b.d - a.d) * t, b.d), nR(a.p + (b.p - a.p) * t, b.p)};
}
struct KnightAnim {
    float strikeW = 0, strikeWV = 0, restW = 0, restWV = 0, runW = 0, runWV = 0;
    vec3 jit, jitV;
};
}  // namespace

void CreatureAnimator::bipedKnight(float dt, const Input& in) {
    using namespace bp;
    if (!ext_) ext_ = std::make_shared<KnightAnim>();
    KnightAnim& k = *static_cast<KnightAnim*>(ext_.get());
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirR(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    auto loc = [&](vec3 v) { return F * v.x + Lw * v.y + U * v.z; };
    const float c = std::cos(yaw_), s = std::sin(yaw_);
    auto rotYaw = [&](vec3 v) { return vec3{v.x * c - v.y * s, v.x * s + v.y * c, v.z}; };
    auto boneUp = [&](int b) {
        const CBone& bn = sk_->bones[(size_t)b];
        const vec3 d0 = rotYaw(nR(B[bn.b] - B[bn.a], {1, 0, 0}));
        const vec3 d1 = nR(J[bn.b] - J[bn.a], d0);
        pose_.up[(size_t)b] = arcR(rotYaw(sk_->bindUp[(size_t)b]), d0, d1);
    };
    auto setArm = [&](int si, vec3 wt, vec3 hd, vec3 up, vec3 pole, float w) {
        if (w <= 0.001f) return;
        const int sh = arm(SHOULDER_L, si), el = arm(ELBOW_L, si), wr = arm(WRIST_L, si), hn = arm(HAND_L, si);
        const float l1 = length(B[el] - B[sh]), l2 = length(B[wr] - B[el]), lh = length(B[hn] - B[wr]);
        const vec3 wcur = J[wr], hcur = nR(J[hn] - J[wr], hd);
        const vec3 tgt = wcur + (wt - wcur) * w;
        const vec3 hdir = nR(hcur + (hd - hcur) * w, hd);
        vec3 e, ww;
        ik2(J[sh], tgt, l1, l2, pole, e, ww);
        J[el] = e, J[wr] = ww, J[hn] = ww + hdir * lh;
        boneUp(armB(B_UPPER_L, si));
        boneUp(armB(B_FORE_L, si));
        const int hb = armB(B_HAND_L, si);
        boneUp(hb);
        pose_.up[(size_t)hb] = w > 0.999f ? up : nR(pose_.up[(size_t)hb] + (up - pose_.up[(size_t)hb]) * w, up);
    };
    const vec3 S1 = J[SHOULDER_R];
    const float strike = std::clamp(in.strike, 0.0f, 1.0f);
    const bool rift = in.wave > 0.5f;
    spR(k.strikeW, k.strikeWV, strike > 0.0f ? 1.0f : 0.0f, strike > 0.0f ? 18.0f : 6.0f, dt);
    spR(k.restW, k.restWV, in.crouch > 0.5f && strike <= 0.0f ? 1.0f : 0.0f, 3.0f, dt);
    spR(k.runW, k.runWV, run_ ? 1.0f : 0.0f, 5.0f, dt);
    const float gait = std::sin(6.2831853f * phase_);
    auto G = [&](vec3 off, vec3 d, vec3 p) { return KGrip{S1 + loc(off), nR(loc(d), F), nR(loc(p), Lw)}; };

    // Tragen: die Klinge auf der Schulter (im Lauf federt sie mit dem Schritt)
    KGrip carry = G({0.2f, 0.03f, -0.1f}, {-0.78f, -0.12f, 0.6f}, {0.1f, 1.0f, 0.0f});  // Runde 2: flacher, auf der Schulter
    KGrip runG = G({0.21f, 0.04f, -0.09f + 0.025f * gait}, {-0.82f, -0.14f, 0.55f}, {0.1f, 1.0f, 0.0f});
    KGrip base = mixG(carry, runG, k.runW);
    // Stuetzen: Spitze 0,5 m vor ihm am Boden, Griff vor dem Bauch
    {
        const vec3 tip = pos_ + F * 0.52f + U * 0.02f;
        const vec3 g = pos_ + F * 0.36f - Lw * 0.02f + U * 1.2f;
        const KGrip rest{g, nR(tip - g, -U), nR(-F + Lw * 0.3f, -F)};
        base = mixG(base, rest, sR(k.restW));
    }
    KGrip pose = base;
    float twoHand = sR(k.restW);
    if (k.strikeW > 0.001f) {
        KGrip wind, hit, follow;
        if (!rift) {  // Hieb: ueber die rechte Schulter, schraeg nach links unten
            wind = G({-0.02f, 0.06f, 0.24f}, {-0.35f, -0.42f, 0.84f}, {0.3f, 1.0f, 0.2f});
            hit = G({0.47f, 0.30f, -0.22f}, {0.78f, 0.52f, -0.34f}, {-0.2f, 0.4f, 0.9f});
            follow = G({0.26f, 0.46f, -0.5f}, {0.12f, 0.75f, -0.65f}, {-0.6f, 0.2f, 0.75f});
        } else {  // Schnitt in die Luft: von links unten nach rechts oben (nach einer Drehung, die die Rolle steuert)
            wind = G({0.18f, 0.42f, -0.52f}, {0.32f, 0.55f, -0.77f}, {-0.5f, 0.3f, 0.8f});
            hit = G({0.48f, 0.04f, 0.04f}, {0.72f, -0.34f, 0.6f}, {0.3f, 0.4f, -0.86f});
            follow = G({0.14f, -0.16f, 0.3f}, {-0.1f, -0.5f, 0.86f}, {0.6f, 0.6f, -0.5f});
        }
        KGrip sp;
        if (strike <= 0.45f) {
            const float u = strike / 0.45f, e = 1.0f - (1.0f - u) * (1.0f - u);
            sp = mixG(base, wind, e);
        } else if (strike <= 0.7f) {
            const float v = (strike - 0.45f) / 0.25f, e = v * v;
            sp = mixG(wind, hit, e);
            sp.g = sp.g + loc({0.14f, 0.0f, 0.04f}) * std::sin(kPiR * e);  // Bogen ueber vorn
        } else {
            sp = mixG(hit, follow, sR((strike - 0.7f) / 0.3f));
        }
        pose = mixG(base, sp, k.strikeW);
        twoHand = std::max(twoHand, k.strikeW * (strike <= 0.45f ? sR(strike / 0.2f) : 1.0f));
    }
    // die Spitze bleibt ueber dem Boden (Stuetzen: genau darauf)
    {
        const float floorZ = pos_.z + 0.02f, tipLen = 1.22f;
        const float tipZ = pose.g.z + pose.d.z * tipLen;
        if (tipZ < floorZ) {
            const float dz = std::clamp((floorZ - pose.g.z) / tipLen, -0.98f, 0.98f);
            vec3 hz = nR(vec3{pose.d.x, pose.d.y, 0.0f}, F);
            pose.d = hz * std::sqrt(1.0f - dz * dz) + U * dz;
        }
    }
    const vec3 D = nR(pose.d, F);
    const vec3 P = nR(pose.p - D * dot(pose.p, D), Lw);
    const vec3 h = cross(P, D);  // Handknochen: Handgelenk -> Knoechel
    const vec3 wrist = pose.g - h * 0.085f - P * 0.02f;
    const float up01 = sR((wrist.z - S1.z + 0.1f) / 0.4f);
    setArm(1, wrist, h, D, nR(loc({-0.6f + 0.8f * up01, -0.8f, -0.4f + 0.5f * up01}), -F), 1.0f);
    // beidhaendig: die linke Hand unter der rechten am langen Griff (zum Knauf hin)
    if (twoHand > 0.001f) {
        const vec3 gl = pose.g - D * 0.13f;
        const vec3 wl = gl - h * 0.085f + P * 0.02f;
        setArm(0, wl, h, D, nR(loc({-0.4f + 0.6f * up01, 0.8f, -0.4f + 0.4f * up01}), -F), twoHand);
    }
    // Panik: der Kopf zuckt (kleine Stoesse in einer Feder), die Schultern heben sich mit dem fliegenden Atem
    if (in.alert > 0.05f) {
        if (rnd() < dt * 2.2f * in.alert) k.jitV = k.jitV + vec3{rnd() - 0.5f, rnd() - 0.5f, (rnd() - 0.5f) * 0.4f} * 0.9f;
        k.jitV = k.jitV - k.jit * (90.0f * dt) - k.jitV * (std::min(1.0f, 14.0f * dt));
        k.jit = k.jit + k.jitV * dt;
        const vec3 off = loc(k.jit) * 0.06f * in.alert;
        J[HEAD] = J[HEAD] + off, J[HEADTOP] = J[HEADTOP] + off * 1.3f, J[JAW] = J[JAW] + off;
        boneUp(B_NECK);
        boneUp(B_HEAD);
    }
}

}  // namespace lim::anim
