// V13.3 Level 3 (KREATUR_Wretch.md, KREATUR_SkinStealer.md): Zusaetze zur Bewegung der Zweibeiner, nach updateBiped.
//
//   Wretch mit Waffe (extra_[0] = 1 Rohrstueck, 2 Nagelbrett, 3 Blechklinge): die Waffe liegt starr in der rechten Hand
//     (Modell: an den Handknochen gebunden, Griffmitte 8,5 cm vor dem Handgelenk, 2 cm zur Handflaeche; die Waffe zeigt zur
//     Daumenseite = "oben" des Handknochens). Hier wird je Haltung Griff, Waffenrichtung und Handflaeche gesetzt, der Arm
//     folgt (IK), die Hand dreht sich mit:
//       Rohr    schleift beim Gehen hinter ihm her (die Spitze auf dem Boden), Handflaeche nach aussen; holt beidhaendig ueber
//               den Kopf aus und schlaegt von oben nach vorn unten durch
//       Brett   liegt auf der Schulter; holt beidhaendig hinter die rechte Schulter aus und fegt waagrecht nach links durch
//       Klinge  tief vor dem Oberschenkel, zuckend; zieht den Arm an die Huefte zurueck (geduckt) und sticht vor
//     Lockt er (plea), steckt die Waffe hinter dem Bein, die freie Hand bittet. strike: 0..0,5 Ausholen, 0,5..1 Schlag
//     (Treffpunkt bei 0,5 + 0,45 x 0,5 - der Kontakt der Pose liegt dort).
//   Skin-Stealer im Mantel (extra_[0] = 2): die Haende in den Manteltaschen (ruhig), im Lampenlicht die Hand vor die
//     Brille; shiver = Verwandlung (Zucken, der Kopf ruckt, die Arme reissen nach aussen, der Kiefer weit offen).
//   Skin-Stealer zerrissen (3): wahre Gestalt; wachsam (alert) spreizt er die Arme mit gekruemmten Fingern.
#include <algorithm>
#include <cmath>

#include "anim/creature_anim.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kPiK = 3.14159265f, kTauK = 6.2831853f;
vec3 dirK(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
float sK(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 nK(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
void spK(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
vec3 rotK(vec3 v, vec3 axis, float ang) {
    const float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
vec3 arcK(vec3 v, vec3 d0, vec3 d1) {  // kuerzeste Drehung d0 -> d1 auf v anwenden
    const vec3 ax = cross(d0, d1);
    const float s = length(ax), c = dot(d0, d1);
    if (s < 1e-6f) return c > 0.0f ? v : v * -1.0f;
    return rotK(v, ax / s, std::atan2(s, c));
}
// Haltung der Waffenhand: Griffmitte, Richtung der Waffe, Handflaeche (wird zur Waffe senkrecht gestellt)
struct WGrip {
    vec3 g, d, p;
};
WGrip mixGrip(const WGrip& a, const WGrip& b, float t) {
    return {a.g + (b.g - a.g) * t, nK(a.d + (b.d - a.d) * t, b.d), nK(a.p + (b.p - a.p) * t, b.p)};
}

struct KinAnim {
    float strikeW = 0.0f, strikeWV = 0.0f, runW = 0.0f, runWV = 0.0f, lureW = 0.0f, lureWV = 0.0f;
    float pocket = 1.0f, pocketV = 0.0f, shield = 0.0f, shieldV = 0.0f, spread = 0.0f, spreadV = 0.0f, conv = 0.0f, convV = 0.0f;
    float twT = 0.5f;
    vec3 jit, jitV, armJ[2], armJV[2];
    float lastStrike = 0.0f;
};
}  // namespace

void CreatureAnimator::bipedKinL3(float dt, const Input& in) {
    using namespace bp;
    const int variant = extra_.empty() ? 0 : (int)(extra_[0] + 0.5f);
    const bool armed = e_ == Entity::Wretch && variant >= 1 && variant <= 3;
    const bool coat = e_ == Entity::SkinStealer && variant >= 2;
    if (!armed && !coat) return;
    if (!ext_) ext_ = std::make_shared<KinAnim>();
    KinAnim& k = *static_cast<KinAnim*>(ext_.get());
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirK(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    auto loc = [&](vec3 v) { return F * v.x + Lw * v.y + U * v.z; };  // Koerperrahmen -> Welt
    const float c = std::cos(yaw_), s = std::sin(yaw_);
    auto rotYaw = [&](vec3 v) { return vec3{v.x * c - v.y * s, v.x * s + v.y * c, v.z}; };
    auto boneUp = [&](int b) {  // wie autoUps: Ruhe-Oben auf kuerzestem Weg zur jetzigen Richtung
        const CBone& bn = sk_->bones[(size_t)b];
        const vec3 d0 = rotYaw(nK(B[bn.b] - B[bn.a], {1, 0, 0}));
        const vec3 d1 = nK(J[bn.b] - J[bn.a], d0);
        pose_.up[(size_t)b] = arcK(rotYaw(sk_->bindUp[(size_t)b]), d0, d1);
    };
    // einen Arm (Seite si) so stellen, dass das Handgelenk bei wt liegt und die Hand nach hd zeigt; up: Oben des Handknochens
    auto setArm = [&](int si, vec3 wt, vec3 hd, vec3 up, vec3 pole, float w) {
        if (w <= 0.001f) return;
        const int sh = arm(SHOULDER_L, si), el = arm(ELBOW_L, si), wr = arm(WRIST_L, si), hn = arm(HAND_L, si);
        const float l1 = length(B[el] - B[sh]), l2 = length(B[wr] - B[el]), lh = length(B[hn] - B[wr]);
        const vec3 S0 = J[sh];
        const vec3 wcur = J[wr], hcur = nK(J[hn] - J[wr], hd);
        const vec3 tgt = wcur + (wt - wcur) * w;
        const vec3 hdir = nK(hcur + (hd - hcur) * w, hd);
        vec3 e, ww;
        ik2(S0, tgt, l1, l2, pole, e, ww);
        J[el] = e, J[wr] = ww, J[hn] = ww + hdir * lh;
        boneUp(armB(B_UPPER_L, si));
        boneUp(armB(B_FORE_L, si));
        const int hb = armB(B_HAND_L, si);
        boneUp(hb);
        if (w > 0.999f) pose_.up[(size_t)hb] = up;
        else pose_.up[(size_t)hb] = nK(pose_.up[(size_t)hb] + (up - pose_.up[(size_t)hb]) * w, up);
    };
    const vec3 S1 = J[SHOULDER_R], S0L = J[SHOULDER_L];
    const float strike = std::clamp(in.strike, 0.0f, 1.0f);

    if (armed) {
        // --- Wretch: Waffe in der rechten Hand ---------------------------------------------------------------------------
        const int wv = variant;
        const float tipLen = wv == 1 ? 0.92f : (wv == 2 ? 0.80f : 0.46f);  // Spitze ab Griffmitte (Modell)
        spK(k.strikeW, k.strikeWV, strike > 0.0f ? 1.0f : 0.0f, strike > 0.0f ? 22.0f : 7.0f, dt);
        spK(k.runW, k.runWV, run_ ? 1.0f : 0.0f, 5.0f, dt);
        spK(k.lureW, k.lureWV, in.plea > 0.3f ? 1.0f : 0.0f, 4.0f, dt);
        const float gait = std::sin(kTauK * phase_), mv = sK(speed_ / 0.8f);
        const vec3 tgt = in.reachAt + U * 0.25f;  // Kopfhoehe des Ziels (reachAt liegt auf Brusthoehe)
        auto G = [&](vec3 off, vec3 d, vec3 p) { return WGrip{S1 + loc(off), nK(loc(d), F), nK(loc(p), Lw)}; };
        WGrip carry, runG, lure, wind, hit, follow;
        if (wv == 1) {  // Rohrstueck
            carry = G({0.06f + 0.05f * gait * mv, -0.07f, -0.56f}, {-0.97f, -0.2f, -0.6f}, {0, -1, 0});
            runG = G({0.0f + 0.12f * gait, -0.13f, -0.5f}, {-0.75f, -0.25f, -0.5f}, {0, -1, 0});
            lure = G({-0.12f, -0.02f, -0.56f}, {-0.3f, 0.05f, -0.95f}, {0, -1, 0});
            wind = G({-0.05f, 0.10f, 0.34f}, {-0.62f, 0.05f, 0.78f}, {0, 1, 0});
            hit = G({0.50f, 0.13f, -0.18f}, {0.95f, 0.0f, -0.12f}, {0, 1, 0});
            follow = G({0.40f, 0.17f, -0.44f}, {0.5f, 0.05f, -0.86f}, {0, 1, 0});
        } else if (wv == 2) {  // Nagelbrett
            carry = G({0.17f, 0.04f, -0.27f + 0.012f * gait * mv}, {-0.32f, -0.08f, 0.94f}, {0, 1, 0});
            runG = G({0.18f, 0.06f, -0.24f + 0.03f * gait}, {-0.36f, -0.05f, 0.93f}, {0, 1, 0});
            lure = G({-0.12f, -0.03f, -0.55f}, {-0.2f, 0.0f, -0.98f}, {0, -1, 0});
            wind = G({0.0f, 0.12f, 0.10f}, {-0.45f, -0.5f, 0.74f}, {0.3f, 1.0f, 0.2f});
            hit = G({0.42f, 0.30f, -0.05f}, {0.75f, 0.62f, 0.22f}, {-0.3f, 0.2f, 0.93f});
            follow = G({0.22f, 0.44f, -0.12f}, {0.0f, 1.0f, -0.05f}, {-0.6f, 0.0f, 0.8f});
        } else {  // Blechklinge
            const float tw = 0.02f * std::sin(time_ * 23.0f) * (std::fmod(time_, 3.1f) < 0.5f ? 1.0f : 0.0f);  // zuckt
            carry = G({0.16f, 0.0f, -0.5f + tw}, {0.85f, 0.12f, -0.5f}, {0, 1, 0});
            runG = G({0.2f + 0.06f * gait, 0.02f, -0.38f}, {0.9f, 0.1f, -0.2f}, {0, 1, 0});
            lure = G({-0.12f, 0.0f, -0.52f}, {-0.3f, 0.1f, -0.95f}, {0, -1, 0});
            wind = G({-0.14f, -0.05f, 0.0f}, {0.95f, 0.0f, 0.1f}, {0, 1, 0});  // Runde 4: auf Schulterhoehe zurueckgezogen (sichtbar)
            hit = G({0.6f, 0.04f, -0.12f}, {1.0f, 0.0f, 0.05f}, {0, 1, 0});
            follow = G({0.62f, 0.04f, -0.14f}, {1.0f, 0.03f, 0.0f}, {0.2f, 1, 0});
        }
        // Ziel: Schlag und Stich zielen zum Kopf des Ziels
        {
            const vec3 dt0 = nK(tgt - hit.g, F);
            if (wv == 1) hit.d = nK(hit.d + dt0 * 0.6f, hit.d);
            if (wv == 3) {
                const vec3 dir = nK(tgt - S1, F);
                wind.d = nK(dir + U * 0.05f, F);
                hit.g = S1 + dir * 0.6f, hit.d = dir;
                follow.g = S1 + dir * 0.63f, follow.d = dir;
            }
        }
        WGrip base = mixGrip(carry, runG, k.runW);
        base = mixGrip(base, lure, k.lureW);
        WGrip pose = base;
        float twoHand = 0.0f;
        if (k.strikeW > 0.001f) {
            WGrip sp;
            if (strike <= 0.5f) {  // ausholen: schnell hoch, dann zitternd halten
                const float u = strike / 0.5f, e = 1.0f - (1.0f - u) * (1.0f - u);
                sp = mixGrip(base, wind, e);
                const float tr = 0.012f * sK((u - 0.6f) / 0.4f);
                sp.g = sp.g + loc({tr * std::sin(time_ * 37.0f), tr * std::sin(time_ * 29.0f), tr * std::sin(time_ * 41.0f)});
            } else {  // Schlag: beschleunigt bis zum Treffpunkt, dann Nachschwung
                const float u = (strike - 0.5f) / 0.5f;
                if (u < 0.45f) {
                    const float v = u / 0.45f, e = v * v;
                    if (wv == 2) {  // waagrecht fegen: Richtung ueber den Gierwinkel (sonst kippt sie ueber den Kopf)
                        auto ang = [&](vec3 d, float& a, float& b) {
                            a = std::atan2(dot(d, Lw), dot(d, F));
                            b = std::asin(std::clamp(d.z, -1.0f, 1.0f));
                        };
                        float a0, b0, a1, b1;
                        ang(wind.d, a0, b0);
                        ang(hit.d, a1, b1);
                        const float a = a0 + (a1 - a0) * e, b = b0 + (b1 - b0) * e;
                        sp = mixGrip(wind, hit, e);
                        sp.d = F * (std::cos(a) * std::cos(b)) + Lw * (std::sin(a) * std::cos(b)) + U * std::sin(b);
                        sp.g = sp.g + loc({0.12f, -0.04f, 0.0f}) * std::sin(kPiK * e);
                    } else {
                        sp = mixGrip(wind, hit, e);
                        if (wv == 1) sp.g = sp.g + loc({0.08f, 0.0f, 0.12f}) * std::sin(kPiK * e);  // Bogen ueber vorn
                    }
                } else {
                    sp = mixGrip(hit, follow, sK((u - 0.45f) / 0.55f));
                }
            }
            pose = mixGrip(base, sp, k.strikeW);
            twoHand = wv == 3 ? 0.0f : k.strikeW * (strike <= 0.5f ? sK(strike / 0.25f) : 1.0f);
        }
        // die Spitze bleibt ueber dem Boden (das Rohr schleift darauf)
        {
            const float floorZ = pos_.z + (wv == 1 ? 0.025f : 0.04f);
            const float tipZ = pose.g.z + pose.d.z * tipLen;
            if (tipZ < floorZ) {
                const float dz = std::clamp((floorZ - pose.g.z) / tipLen, -0.98f, 0.98f);
                vec3 hz{pose.d.x, pose.d.y, 0.0f};
                hz = nK(hz, -F);
                pose.d = hz * std::sqrt(1.0f - dz * dz) + U * dz;
            }
        }
        const vec3 D = nK(pose.d, F);
        const vec3 P = nK(pose.p - D * dot(pose.p, D), Lw);
        const vec3 h = cross(P, D);  // Handknochen: Handgelenk -> Knoechel
        const vec3 wrist = pose.g - h * 0.085f - P * 0.02f;
        const float up01 = sK((wrist.z - S1.z + 0.1f) / 0.4f);  // Arm erhoben: Ellbogen nach aussen und vorn
        const vec3 pole = nK(loc({-0.6f + 0.8f * up01, -0.75f, -0.4f + 0.5f * up01}), -F);
        setArm(1, wrist, h, D, pole, 1.0f);
        // beidhaendig (Rohr, Brett): die linke Hand unter der rechten am Griff, Handflaeche gegenueber
        if (twoHand > 0.001f) {
            const vec3 gl = pose.g - D * 0.105f;
            const vec3 wl = gl - h * 0.085f + P * 0.02f;
            const vec3 poleL = nK(loc({-0.4f + 0.6f * up01, 0.75f, -0.4f + 0.4f * up01}), -F);
            setArm(0, wl, h, D, poleL, twoHand);
        } else if (wv == 3 && k.strikeW > 0.001f) {  // Klinge: die freie Hand greift vor (zum Ziel)
            const vec3 dir = nK(tgt - U * 0.3f - S0L, F);
            const float lw = k.strikeW * (strike <= 0.5f ? sK(strike / 0.3f) : 1.0f);
            setArm(0, S0L + dir * 0.5f, dir, nK(loc({0.2f, 0.0f, 1.0f}), U), nK(loc({-0.5f, 0.8f, -0.4f}), -F), lw * 0.85f);
        }
        k.lastStrike = strike;
        return;
    }

    // --- Skin-Stealer im Mantel / zerrissen ----------------------------------------------------------------------------
    const bool calm = in.alert < 0.3f && in.reach < 0.05f && in.shiver < 0.05f && in.talk < 0.5f;
    spK(k.pocket, k.pocketV, variant == 2 && calm ? 1.0f : 0.0f, 3.0f, dt);
    spK(k.shield, k.shieldV, variant == 2 && in.light > 0.3f && in.shiver < 0.05f ? 1.0f : 0.0f, 6.0f, dt);
    spK(k.spread, k.spreadV, variant == 3 && in.alert > 0.3f ? std::clamp(1.0f - in.reach * 1.5f, 0.0f, 1.0f) : 0.0f, 4.0f, dt);
    spK(k.conv, k.convV, std::clamp(in.shiver, 0.0f, 1.0f), 10.0f, dt);
    // Verwandlung: Stoesse in Kopf und Armen
    k.twT -= dt;
    if (k.conv > 0.05f && k.twT <= 0.0f) {
        k.twT = 0.07f + 0.18f * rnd();
        k.jitV = k.jitV + vec3{rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f} * (26.0f * k.conv);
        for (int i = 0; i < 2; ++i) k.armJV[i] = k.armJV[i] + vec3{rnd() - 0.5f, rnd() - 0.5f, rnd() - 0.5f} * (9.0f * k.conv);
    }
    for (int i = 0; i < 3; ++i) {
        float* x = i == 0 ? &k.jit.x : (i == 1 ? &k.jit.y : &k.jit.z);
        float* v = i == 0 ? &k.jitV.x : (i == 1 ? &k.jitV.y : &k.jitV.z);
        spK(*x, *v, 0.0f, 18.0f, dt);
        for (int a = 0; a < 2; ++a) {
            float* ax = i == 0 ? &k.armJ[a].x : (i == 1 ? &k.armJ[a].y : &k.armJ[a].z);
            float* av = i == 0 ? &k.armJV[a].x : (i == 1 ? &k.armJV[a].y : &k.armJV[a].z);
            spK(*ax, *av, 0.0f, 14.0f, dt);
        }
    }
    for (int si = 0; si < 2; ++si) {
        const float sg = si == 0 ? 1.0f : -1.0f;
        const vec3 S = si == 0 ? S0L : S1;
        const vec3 polePk = nK(loc({-0.9f, sg * 0.4f, -0.1f}), -F);
        // Haende in den Manteltaschen (Huefte, leicht vorn), die Hand zeigt hinein
        if (k.pocket > 0.001f) {
            // Taschenoeffnung an der Huefte (Klappe im Modell): Handgelenk an der Oeffnung, die Hand nach innen unten hinein
            const vec3 wt = S + loc({0.075f, -sg * 0.025f, -0.52f});
            setArm(si, wt, nK(loc({-0.35f, -sg * 0.45f, -1.0f}), -U), nK(loc({1.0f, 0.0f, 0.1f}), F), polePk, k.pocket);
        }
        // Verwandlung, noch im Mantel: die Haende krallen sich ins eigene Gesicht (dort reisst die Haut), Ellbogen weit
        // aussen, zuckend; danach (zerrissen) entfalten sich die Arme weit nach aussen und oben
        if (k.conv > 0.001f && variant == 2) {
            const vec3 wt = headPos_ + headDir_ * 0.1f - U * 0.1f - Lw * (sg * -0.075f) + k.armJ[si] * 0.035f;
            const vec3 hd = nK(U * 0.8f + headDir_ * 0.2f - Lw * (sg * 0.25f) + k.armJ[si] * 0.3f, U);
            setArm(si, wt, hd, nK(headDir_ * -1.0f, -F), nK(loc({-0.2f, sg * 0.9f, -0.3f}), Lw * sg), k.conv);
        }
        if (k.conv > 0.001f && variant == 3) {  // entfaltet: die Arme ueber den Kopf, Ellbogen weit, Krallen nach vorn
            const vec3 wt = S + loc({0.14f, sg * 0.3f, 0.36f}) + k.armJ[si] * 0.05f;
            const vec3 hd = nK(loc({0.6f, sg * 0.25f, 0.5f}) + k.armJ[si] * 0.3f, U);
            setArm(si, wt, hd, nK(loc({0.0f, 0.0f, 1.0f}), U), nK(loc({-0.3f, sg * 0.9f, -0.2f}), Lw * sg), k.conv);
        }
        // wahre Gestalt, wachsam: geduckt, beide Haende vor der Brust nach vorn gestreckt, die langen Finger zum Ziel
        if (k.spread > 0.001f) {
            const vec3 wt = S + loc({0.32f, sg * 0.04f, -0.42f});  // unter der Maske auf der Brust (sie bleibt sichtbar)
            setArm(si, wt, nK(loc({0.85f, -sg * 0.1f, -0.25f}), F), nK(loc({0.0f, 0.0f, 1.0f}), U), nK(loc({-0.3f, sg * 0.8f, -0.5f}), -U), k.spread);
        }
    }
    // Lampe im Gesicht: die rechte Hand vor die Brille, der Handruecken zum Licht
    if (k.shield > 0.001f) {
        const vec3 wt = headPos_ + headDir_ * 0.2f - U * 0.1f - Lw * 0.04f;
        setArm(1, wt, nK(U + headDir_ * 0.3f, U), nK(headDir_ * -1.0f, -F), nK(loc({-0.3f, -0.8f, -0.5f}), -U), k.shield);
    }
    // Kopf: ruckt in der Verwandlung (um das Kopfgelenk), der Kiefer reisst weit auf
    if (k.conv > 0.001f || in.jaw > 0.6f) {
        const vec3 Hc = J[HEAD];
        const float ry = k.jit.x * 0.6f, rp = k.jit.y * 0.5f - 0.25f * k.conv, rr = k.jit.z * 0.7f;
        auto rot = [&](vec3 v) { return rotK(rotK(rotK(v, U, ry), Lw, rp), F, rr); };
        J[HEADTOP] = Hc + rot(J[HEADTOP] - Hc);
        J[JAW] = Hc + rot(J[JAW] - Hc);
        pose_.up[B_HEAD] = rot(pose_.up[B_HEAD]);
        pose_.up[B_JAW] = rot(pose_.up[B_JAW]);
        const float extra = 0.55f * sK((in.jaw - 0.6f) / 0.4f) * (variant == 2 ? 1.0f : 0.6f);
        if (extra > 0.001f) {
            const vec3 ax = nK(cross(nK(J[HEADTOP] - Hc, U), headDir_), Lw);
            J[JAW] = Hc + rotK(J[JAW] - Hc, ax, -extra);
            pose_.up[B_JAW] = rotK(pose_.up[B_JAW], ax, -extra);
        }
        headPos_ = (J[HEAD] + J[HEADTOP]) * 0.5f;
    }
}

}  // namespace lim::anim
