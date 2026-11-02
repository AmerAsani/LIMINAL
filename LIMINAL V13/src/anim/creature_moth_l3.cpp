// V13.3 Level 3 (KREATUR_Deathmoth.md, Abschnitt Level 3): Bewegung des Praetorianers (Mottenskelett mit acht Beinen,
// Modell ent_moth_praetorian, tools/blender/creatures/moth_praetorian.py). extra_[0] = 5.
//
// Er fliegt nicht (die Fluegel sind verkuemmert, sie faecheln nur Lockstoffe). Der Leib ist um kG vergroessert, die acht
// Beine dazu um kLegK gestreckt (Massstab je Knochen): ein schwerer Panzerleib hoch auf duennen Beinen, die Knie ueber dem
// Ruecken wie bei einem Weberknecht, die Brust aufgerichtet (der Kopf auf gut 0,8 m, mit den Fuehlern knapp 1 m).
// Gang: zwei Viererguppen im Wechsel (L1 R2 L3 R4 / R1 L2 R3 L4), schnell und ruckartig; die Spitzen tasten. Eingaben:
//   crouch   pirscht: Leib tief, Beine weit gespreizt, Kopf vor
//   alert    Fuehler hoch und gespreizt; perceive: Kopf und Fuehler zu perceiveAt; light: zuckt zurueck, Vorderbeine schuetzend
//   shiver   faechelt: die Fluegelstummel schwirren, die Duftpinsel am Hinterleib spreizen sich (Lockstoff - ruft die anderen)
//   strike   Phase 0..1 eines Stichs: bis 0,5 baeumt er sich auf (die Vorderbeine hoch ueber dem Kopf, auf reachAt gerichtet),
//            danach stossen die Stachelhaken zu (Treffer bei 0,6), der Hinterleib krummt sich unter den Leib
//   jaw      spuckt: der Kopf schnellt vor und hoch (aetzende Fluessigkeit aus der Druese)
#include <algorithm>
#include <cmath>
#include <memory>

#include "anim/creature_anim.hpp"
#include "anim/skeleton.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kPiP = 3.14159265f, kTauP = 6.2831853f;
constexpr float kG = 3.6f, kLegK = 2.3f;  // Leib x 3,6, Beine x 8,3 (70-85 cm)
vec3 dirP(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
vec3 nP(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
void spP(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
struct P3 {
    vec3 c0{1, 0, 0}, c1{0, 1, 0}, c2{0, 0, 1};
    vec3 operator*(vec3 v) const { return c0 * v.x + c1 * v.y + c2 * v.z; }
    P3 operator*(const P3& o) const { return P3{(*this) * o.c0, (*this) * o.c1, (*this) * o.c2}; }
};
P3 rotP(vec3 a, float ang) { return P3{rotateAround({1, 0, 0}, a, ang), rotateAround({0, 1, 0}, a, ang), rotateAround({0, 0, 1}, a, ang)}; }
vec3 arcP(vec3 v, vec3 d0, vec3 d1) {
    const vec3 ax = cross(d0, d1);
    const float sn = length(ax), cs = dot(d0, d1);
    if (sn < 1e-6f) return cs < 0.0f ? -v : v;
    return rotateAround(v, ax / sn, std::atan2(sn, cs));
}
float smP(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Ruheplatz der Fussspitzen (Rahmen der Gierrichtung, m): vorn, Mitte, hinten (Brust), am Hinterleib
constexpr float kRestX[4] = {0.46f, 0.14f, -0.18f, -0.48f}, kRestY[4] = {0.40f, 0.52f, 0.50f, 0.38f};
// Viererguppen: L1 R2 L3 R4 / R1 L2 R3 L4
constexpr float kOff[8] = {0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f};

struct PraetorianAnim {
    vec3 plant[8], from[8], to[8];
    bool swing[8]{}, init = false;
    float t[8]{}, phase = 0.0f;
    float low = 0, lowV = 0, rear = 0, rearV = 0, fan = 0, fanV = 0, spit = 0, spitV = 0, flinch = 0, flinchV = 0, alert = 0, alertV = 0;
    float bob = 0, ant[2]{}, antV[2]{}, antT = 0.5f, antTg[2]{};
};
}  // namespace

bool CreatureAnimator::praetorian() const { return e_ == Entity::Moth && !extra_.empty() && extra_[0] > 4.5f; }

void CreatureAnimator::updatePraetorian(float dt, const Input& in) {
    using namespace mo;
    if (!ext_) ext_ = std::make_shared<PraetorianAnim>();
    PraetorianAnim& S = *static_cast<PraetorianAnim*>(ext_.get());
    const auto& B0 = sk_->bind;
    auto& J = pose_.j;
    const vec3 F = dirP(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    spP(S.low, S.lowV, in.crouch, 3.0f, dt);
    spP(S.alert, S.alertV, std::max(in.alert, in.perceive), 4.0f, dt);
    spP(S.fan, S.fanV, in.shiver, 6.0f, dt);
    spP(S.spit, S.spitV, in.jaw, 14.0f, dt);
    spP(S.flinch, S.flinchV, in.light, 5.0f, dt);
    // Stich: aufbaeumen bis 0,5, dann zustossen
    const float st = in.strike;
    const float rearT = st <= 0.0f ? 0.0f : (st < 0.5f ? smP(st / 0.5f) : 1.0f - 0.35f * smP((st - 0.5f) / 0.5f));
    spP(S.rear, S.rearV, std::max(rearT, 0.35f * S.flinch), st > 0.0f ? 14.0f : 5.0f, dt);
    const float stab = st > 0.5f ? std::sin(kPiP * std::clamp((st - 0.5f) / 0.45f, 0.0f, 1.0f)) : 0.0f;
    // Leib: Hoehe, Neigung (die Brust aufgerichtet), Gangwippen
    const bool walking = speed_ > 0.08f;
    const float stride = 0.42f + 0.12f * std::min(speed_, 3.0f);
    const float rate = walking ? speed_ / stride : 0.0f;
    S.phase += rate * dt;
    S.phase -= std::floor(S.phase);
    const float duty = speed_ > 1.6f ? 0.5f : 0.62f;
    S.bob = walking ? 0.018f * std::sin(kTauP * 2.0f * S.phase) : 0.006f * std::sin(time_ * 3.1f);
    const float bodyH = 0.6f - 0.22f * S.low + 0.16f * S.rear + S.bob;
    const float pitch = 0.38f - 0.25f * S.low + 0.6f * S.rear;  // Brust hoch, Hinterleib tief
    const P3 Myaw{F, Lw, U};
    const P3 Mb = Myaw * rotP({0, 1, 0}, -pitch);  // um die Querachse: Nase hoch
    const vec3 T0 = pos_ + U * bodyH;
    auto W = [&](vec3 bindP) { return T0 + Mb * (bindP * kG); };
    // --- Leib ---------------------------------------------------------------------------------------------------------
    J[ROOT] = T0;
    // Kopf: zum Wahrgenommenen / zum Ziel; spucken: schnellt vor und hoch
    vec3 aim = in.look ? in.lookAt : T0 + F * 2.0f;
    if (in.perceive > 0.01f) aim = in.perceiveAt;
    if (st > 0.0f || in.reach > 0.01f) aim = in.reachAt;
    const vec3 hb = W(B0[HEAD]);
    const vec3 dHead = nP(aim - hb, Mb * vec3{1, 0, 0});
    const vec3 headF = nP(Mb * vec3{1, 0, 0} * 0.45f + dHead * 0.55f + U * (0.25f * S.spit), F);
    J[HEAD] = hb + headF * (0.012f * S.spit);
    J[PROB] = J[HEAD] + headF * (length(B0[PROB] - B0[HEAD]) * kG);
    // Hinterleib: haengt, schwingt im Gang; beim Stich unter den Leib gekruemmt; faechelt er, hebt er die Spitze
    const float curl = 0.28f + 0.9f * S.rear - 0.25f * S.fan + 0.05f * std::sin(time_ * 1.7f);
    const float sway = walking ? 0.12f * std::sin(kTauP * S.phase) : 0.04f * std::sin(time_ * 0.9f);
    vec3 prev = W(B0[ABD0]);
    J[ABD0] = prev;
    vec3 dir = Mb * vec3{-1, 0, 0};
    const int abd[4] = {ABD0, ABD1, ABD2, ABDTIP};
    for (int i = 1; i < 4; ++i) {
        dir = nP(rotateAround(rotateAround(dir, Mb * vec3{0, 1, 0}, -curl * 0.45f), U, sway * 0.5f), dir);
        const float len = length(B0[abd[i]] - B0[abd[i - 1]]) * kG;
        prev = prev + dir * len;
        J[abd[i]] = prev;
    }
    // Duftpinsel: anliegend, beim Faecheln gespreizt
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const vec3 side = nP(cross(dir, U), Lw) * sg;
        const float len = length(B0[BRISTLE_L] - B0[ABDTIP]) * kG;
        J[s == 0 ? BRISTLE_L : BRISTLE_R] = J[ABDTIP] + nP(dir + side * (0.25f + 1.3f * S.fan) + U * (0.6f * S.fan), dir) * len;
    }
    // Fuehler: gefiedert, tasten; wachsam hoch und gespreizt, zum Wahrgenommenen
    S.antT -= dt;
    if (S.antT <= 0.0f) {
        S.antT = 0.25f + 0.8f * rnd();
        S.antTg[0] = rnd() - 0.5f, S.antTg[1] = rnd() - 0.5f;
    }
    for (int s = 0; s < 2; ++s) {
        spP(S.ant[s], S.antV[s], S.antTg[s], 9.0f, dt);
        const float sg = s == 0 ? 1.0f : -1.0f;
        const int jb = s == 0 ? ANTB_L : ANTB_R, jt = s == 0 ? ANTT_L : ANTT_R;
        const vec3 b = J[HEAD] + Mb * ((B0[jb] - B0[HEAD]) * kG);
        const vec3 rel = (B0[jt] - B0[jb]) * kG;
        vec3 d = Mb * rel;
        d = rotateAround(d, U, sg * (0.25f * S.alert) + 0.35f * S.ant[s]);
        d = rotateAround(d, nP(cross(d, U), Lw), -0.5f * S.alert + 0.25f * S.ant[s]);
        if (in.perceive > 0.01f) d = nP(d + (aim - b) * 0.25f * in.perceive, d) * length(rel);
        J[jb] = b;
        J[jt] = b + d;
    }
    // Fluegelstummel: angelegt; faechelt er, schwirren sie
    const float beat = S.fan * 0.55f * std::sin(time_ * kTauP * 16.0f);
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const int wf = s == 0 ? WF_L : WF_R, wft = s == 0 ? WFT_L : WFT_R, wh = s == 0 ? WH_L : WH_R, wht = s == 0 ? WHT_L : WHT_R;
        const vec3 axis = Mb * vec3{1, 0, 0};
        const P3 rf = rotP(axis, sg * (0.35f + beat)), rh = rotP(axis, sg * (0.25f + 0.7f * beat));
        J[wf] = W(B0[wf]);
        J[wft] = J[wf] + rf * (Mb * ((B0[wft] - B0[wf]) * kG));
        J[wh] = W(B0[wh]);
        J[wht] = J[wh] + rh * (Mb * ((B0[wht] - B0[wh]) * kG));
    }
    // --- Beine ---------------------------------------------------------------------------------------------------------
    if (S.init && std::hypot(S.plant[0].x - pos_.x, S.plant[0].y - pos_.y) > 2.0f) S.init = false;  // versetzt
    for (int l = 0; l < 8; ++l) {
        const int pair = l / 2;
        const float sg = (l % 2) == 0 ? 1.0f : -1.0f;
        const float spread = 1.0f + 0.22f * S.low;
        vec3 rest = pos_ + Myaw * vec3{kRestX[pair] * (1.0f + 0.1f * S.low), sg * kRestY[pair] * spread, 0.0f};
        if (!S.init) S.plant[l] = rest, S.to[l] = rest;
        // Gang: Viererguppen im Wechsel
        if (walking) {
            float ph = S.phase + kOff[l];
            ph -= std::floor(ph);
            const bool want = ph > duty;
            const float tSw = std::clamp((ph - duty) / (1.0f - duty), 0.0f, 1.0f);
            const float T = rate > 1e-3f ? 1.0f / rate : 1.0f;
            vec3 target = rest + vel_ * ((1.0f - tSw) * (1.0f - duty) * T + duty * T * 0.5f);
            target.z = pos_.z;
            if (want && !S.swing[l]) S.swing[l] = true, S.from[l] = S.plant[l], S.to[l] = target, S.t[l] = 0.0f;
            if (S.swing[l]) {
                if (!want) {
                    S.swing[l] = false, S.plant[l] = S.to[l];
                    stepped_ = true, stepPos_ = S.plant[l], stepWeight_ = 0.35f;
                } else {
                    S.t[l] = tSw;
                    if (tSw < 0.7f) S.to[l] = target;
                }
            }
        } else if (S.swing[l]) {
            S.t[l] += dt / 0.18f;
            if (S.t[l] >= 1.0f) S.swing[l] = false, S.plant[l] = S.to[l], stepped_ = true, stepPos_ = S.plant[l], stepWeight_ = 0.3f;
        } else {
            vec3 dd = S.plant[l] - rest;
            dd.z = 0.0f;
            bool other = false;
            for (int k = 0; k < 8; ++k) other = other || S.swing[k];
            if (length(dd) > 0.14f && !other) S.swing[l] = true, S.from[l] = S.plant[l], S.to[l] = rest, S.to[l].z = pos_.z, S.t[l] = 0.0f;
        }
        vec3 tip = S.plant[l];
        if (S.swing[l]) {
            const float u = smP(S.t[l]);
            tip = S.from[l] + (S.to[l] - S.from[l]) * u;
            tip.z = S.from[l].z + (S.to[l].z - S.from[l].z) * u + 0.14f * std::sin(kPiP * std::clamp(S.t[l], 0.0f, 1.0f));
        }
        const vec3 hip = W(B0[legJ(l, 0)]);
        const vec3 out = nP(vec3{tip.x - hip.x, tip.y - hip.y, 0.0f}, Lw * sg);
        // aufgebaeumt: die vorderen zwei Paare hoch, die Spitzen auf das Ziel; beim Stoss die ersten zum Ziel
        if (pair <= 1 && S.rear > 0.01f) {
            const vec3 aimP = in.reachAt + Lw * (sg * (pair == 0 ? 0.12f : 0.3f)) - F * (pair == 0 ? 0.0f : 0.25f);
            const vec3 raised = hip + nP(aimP - hip, F) * (pair == 0 ? 0.42f : 0.36f) + U * (pair == 0 ? 0.38f : 0.22f);
            const vec3 stabbed = hip + nP(aimP - hip, F) * 0.66f;
            const vec3 up = pair == 0 ? raised + (stabbed - raised) * stab : raised;
            tip = tip + (up - tip) * S.rear;
        }
        if (pair == 0 && S.flinch > 0.01f && st <= 0.0f)  // Licht: die Vorderbeine schuetzend vor den Kopf
            tip = tip + (hip + F * 0.25f + U * 0.2f + Lw * (sg * 0.1f) - tip) * (S.flinch * 0.7f);
        const float l1 = length(B0[legJ(l, 1)] - B0[legJ(l, 0)]) * kG * kLegK, l2 = length(B0[legJ(l, 2)] - B0[legJ(l, 1)]) * kG * kLegK;
        const float l3 = length(B0[legJ(l, 3)] - B0[legJ(l, 2)]) * kG * kLegK;
        // die Fussglieder haengen von der Schiene nach unten aussen zur Spitze (am Ziel: in Stossrichtung)
        const bool raisedLeg = pair <= 1 && S.rear > 0.3f;
        const vec3 tarsDir = raisedLeg ? nP(in.reachAt - hip, F) : nP(-U * 0.88f + out * 0.42f, -U);
        const vec3 foot = tip - tarsDir * l3;
        vec3 knee, fj;
        ik2(hip, foot, l1, l2, nP(U * 1.0f + out * 0.55f, U), knee, fj);
        J[legJ(l, 0)] = hip;
        J[legJ(l, 1)] = knee;
        J[legJ(l, 2)] = fj;
        J[legJ(l, 3)] = fj + tarsDir * l3;
    }
    S.init = true;
    // --- Oben-Richtungen, Massstab --------------------------------------------------------------------------------------
    for (size_t b = 0; b < sk_->bones.size(); ++b) {
        const CBone& c = sk_->bones[b];
        if (c.a == c.b) {
            pose_.up[b] = F;
            continue;
        }
        const vec3 d0 = Mb * nP(B0[c.b] - B0[c.a], {1, 0, 0}), d1 = nP(J[c.b] - J[c.a], d0);
        pose_.up[b] = arcP(Mb * sk_->bindUp[b], d0, d1);
    }
    pose_.up[mo::B_HEAD] = arcP(Mb * sk_->bindUp[mo::B_HEAD], Mb * nP(B0[PROB] - B0[HEAD], {1, 0, 0}), nP(J[PROB] - J[HEAD], F));
    pose_.s.assign(sk_->bones.size(), kG);
    for (int l = 0; l < 8; ++l)
        for (int i = 0; i < 3; ++i) pose_.s[(size_t)legB(l, i)] = kG * kLegK;
    headPos_ = J[HEAD];
    headDir_ = headF;
}

}  // namespace lim::anim
