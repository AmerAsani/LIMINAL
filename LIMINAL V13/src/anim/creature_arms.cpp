// V13.3: Arms / Les Bras (Entite 15-FR, Wikidot FR Rev. 44) - Skelett und Bewegung (KREATUR_Arms.md).
//
// Ein einzelnes, sehr langes Glied mit drei Krallen, das in Lueftungsschaechten lebt. Wurzel: Mitte der Oeffnung in der
// Decke. Die Kette aus kSeg Gliedern liegt im Schacht (ueber der Decke, verborgen) und gleitet durch die Oeffnung heraus:
// der Arm folgt einem Weg - im Schacht waagrecht (Richtung yaw), durch die Oeffnung hinab und im Bogen zum Ziel. Er ist
// verdreht (jedes Glied ein Stueck weiter gewunden) und windet sich langsam wie eine Schlange.
//
// Eingaben: reach 0..1 wie weit er heraus ist (x kOutMax), reachAt Ziel (Brust); up Normale der Oeffnung (in den Raum,
// an der Decke (0, 0, -1)); jaw Krallen schliessen; alert Ausholen (Krallen weit auf, der Arm zieht sich zurueck wie zum
// Biss); light im Lichtkegel - er verbrennt, windet sich, Hautfetzen fallen; shiver Rauch; hang 1: Riss in der Decke
// (keine Lueftung an dieser Stelle); perceive: tastet suchend; clearance: Hoehe der Decke ueber dem Boden.
#include "anim/creature_anim.hpp"

#include <algorithm>
#include <cmath>

#include "anim/skeleton.hpp"

namespace lim::anim {

namespace {
constexpr float kTau = 6.2831853f;
float sm01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
void spring(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
vec3 safeNorm(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
float hash1(unsigned i) {
    i ^= i >> 16, i *= 0x7feb352du, i ^= i >> 15, i *= 0x846ca68bu, i ^= i >> 16;
    return (float)(i & 0xFFFFFF) / 16777216.0f;
}
constexpr float kClawL[3] = {0.13f, 0.12f, 0.24f};  // Fingerglied, zweites Glied, Kralle
// Belegung von xs_
constexpr int X_OUT = 0, X_OUTV = 1, X_CLOSE = 2, X_CLOSEV = 3, X_WIND = 4, X_WINDV = 5, X_BURN = 6, X_BURNV = 7, X_SMOKE = 8,
              X_SMOKEV = 9, X_CRACK = 10, X_CRACKV = 11, X_SHRED = 16, X_SHREDN = 16, X_WISP = X_SHRED + ar::kShreds * X_SHREDN,
              X_SIZE = X_WISP + ar::kSmoke * 2;
// Fetzen: Lage (0), Geschwindigkeit (3), Achse x (6), Achse z (9), Drehung (12), Zustand (15: 0 am Arm, 1 faellt, 2 liegt)
// Rauch je Fahne: Phase (0..1), Quelle (Fetzen)

// Weg des Arms nach Bogenlaenge t: t < 0 im Schacht (von der Oeffnung zurueck), t >= 0 heraus (Punkte q)
struct Way {
    vec3 O, n, duct;
    vec3 q[40];
    float s[40];
    int m = 0;
    void add(vec3 p) {
        if (m >= 40) return;
        q[m] = p;
        s[m] = m ? s[m - 1] + length(p - q[m - 1]) : 0.0f;
        ++m;
    }
    vec3 at(float t) const {
        if (t <= 0.0f) {  // im Schacht: 0,3 m die Oeffnung hinauf, dann waagrecht den Schacht entlang
            const float u = -t;
            return u < 0.3f ? O - n * u : O - n * 0.3f + duct * (u - 0.3f);
        }
        for (int i = 1; i < m; ++i)
            if (t <= s[i]) return q[i - 1] + (q[i] - q[i - 1]) * ((t - s[i - 1]) / std::max(1e-6f, s[i] - s[i - 1]));
        return m >= 2 ? q[m - 1] + safeNorm(q[m - 1] - q[m - 2], n) * (t - s[m - 1]) : O + n * t;
    }
};
}  // namespace

CreatureSkeleton ar::skeleton() {
    CreatureSkeleton s;
    s.kind = Entity::Arms;
    s.bind.assign((size_t)JCOUNT, vec3{});
    s.bones.assign((size_t)BCOUNT, CBone{});
    s.bindUp.assign((size_t)BCOUNT, vec3{1, 0, 0});
    auto J = [&](int i, vec3 p) { s.bind[(size_t)i] = p; };
    auto B = [&](int i, int a, int b, int parent, float r, vec3 up) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)b, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = false;
        s.bindUp[(size_t)i] = up;
    };
    const vec3 F{1, 0, 0}, Z{0, 0, 1};
    J(ROOT, {0, 0, 0});
    for (int i = 0; i <= kSeg; ++i) J(C0 + i, {0, 0, kTop - (float)i * kSegLen});
    J(KN, s.bind[(size_t)WRIST] - Z * kHand);
    for (int c = 0; c < kClaws; ++c) {  // Ruhepose: maessig gespreizt, je Gelenk etwas einwaerts
        const float th = (float)c * kTau / (float)kClaws;
        const vec3 rad{std::cos(th), std::sin(th), 0.0f};
        vec3 p = s.bind[(size_t)KN] + rad * 0.035f;
        J(claw(c, 0), p);
        for (int i = 0; i < 3; ++i) {
            const float a = 0.62f - 0.27f * (float)i;
            p += (rad * std::sin(a) - Z * std::cos(a)) * kClawL[i];
            J(claw(c, i + 1), p);
        }
    }
    J(CRACK_A, {0, 0, 0}), J(CRACK_B, {0, 0, -0.05f});
    for (int k = 0; k < kShreds; ++k) {
        const vec3 a{0.5f, ((float)k - 1.0f) * 0.12f, -1.0f};
        J(SHRED0 + k * 2, a), J(SHRED0 + k * 2 + 1, a + F * 0.02f);
    }
    for (int k = 0; k < kSmoke; ++k) {
        const vec3 a{-0.5f, ((float)k - 1.5f) * 0.15f, -1.2f};
        J(SMOKE0 + k * 2, a), J(SMOKE0 + k * 2 + 1, a + Z * 0.12f);
    }
    B(B_ROOT, ROOT, ROOT, -1, 0.0f, F);
    for (int i = 0; i < kSeg; ++i) B(B_SEG0 + i, C0 + i, C0 + i + 1, i == 0 ? B_ROOT : B_SEG0 + i - 1, 0.045f, F);
    B(B_HAND, WRIST, KN, B_SEG0 + kSeg - 1, 0.04f, F);
    for (int c = 0; c < kClaws; ++c) {
        const float th = (float)c * kTau / (float)kClaws;
        const vec3 tng{-std::sin(th), std::cos(th), 0.0f};  // senkrecht zur Ebene der Kralle
        for (int i = 0; i < 3; ++i) B(bClaw(c, i), claw(c, i), claw(c, i + 1), i == 0 ? B_HAND : bClaw(c, i - 1), 0.015f, tng);
    }
    B(B_CRACK, CRACK_A, CRACK_B, B_ROOT, 0.0f, F);
    for (int k = 0; k < kShreds; ++k) B(B_SHRED0 + k, SHRED0 + k * 2, SHRED0 + k * 2 + 1, B_ROOT, 0.0f, Z);
    for (int k = 0; k < kSmoke; ++k) B(B_SMOKE0 + k, SMOKE0 + k * 2, SMOKE0 + k * 2 + 1, B_ROOT, 0.0f, F);
    s.height = 0.3f;
    s.reach = 3.0f;
    s.center = {0.0f, 0.0f, -1.2f};
    return s;
}

void CreatureAnimator::updateArms(float dt, const Input& in) {
    using namespace ar;
    auto& J = pose_.j;
    if ((int)xs_.size() != X_SIZE) xs_.assign((size_t)X_SIZE, 0.0f);
    float* X = xs_.data();
    const vec3 U{0, 0, 1};
    // Oeffnung: Normale in den Raum (an der Decke nach unten); Vorfuehrung ohne Angabe: Decke
    const vec3 n = in.up.z > 0.5f ? vec3{0, 0, -1} : safeNorm(in.up, {0, 0, -1});
    const vec3 duct = dirYaw(yaw_);
    const vec3 O = pos_;
    const float floorZ = O.z - std::clamp(in.clearance, 1.2f, 6.0f);
    if (!chainInit_) {
        for (int k = 0; k < kShreds; ++k) X[X_SHRED + k * X_SHREDN + 15] = 0.0f;
        X[X_OUT] = std::clamp(in.reach, 0.0f, 1.0f) * kOutMax, X[X_OUTV] = 0.0f;
    }
    // Federn: heraus langsam gleitend, beim Zugriff blitzschnell, verbrannt noch schneller zurueck
    const float want = std::clamp(in.reach, 0.0f, 1.0f) * kOutMax;
    const float om = in.light > 0.4f ? 11.0f : (want > X[X_OUT] + 0.4f && in.alert < 0.5f ? 9.0f : 2.6f);
    spring(X[X_OUT], X[X_OUTV], want, om, dt);
    X[X_OUT] = std::clamp(X[X_OUT], 0.0f, kOutMax);
    spring(X[X_CLOSE], X[X_CLOSEV], in.jaw, 14.0f, dt);
    spring(X[X_WIND], X[X_WINDV], in.alert, 6.0f, dt);
    spring(X[X_BURN], X[X_BURNV], in.light, 8.0f, dt);
    spring(X[X_SMOKE], X[X_SMOKEV], in.shiver, 1.5f, dt);
    spring(X[X_CRACK], X[X_CRACKV], in.hang, 20.0f, dt);
    spring(perc_, percV_, in.perceive, 2.0f, dt);
    const float out = X[X_OUT], close = std::clamp(X[X_CLOSE], 0.0f, 1.0f), wind = std::clamp(X[X_WIND], 0.0f, 1.0f);
    const float burn = std::clamp(X[X_BURN], 0.0f, 1.0f);

    // --- Weg heraus: Bogen von der Oeffnung (entlang n) zum Ziel; Ausholen: S-Kurve zurueck; dazu eine Welle, die den Arm
    // entlanglaeuft (er windet sich), im Licht ein wildes Zucken
    Way w;
    w.O = O, w.n = n, w.duct = duct;
    vec3 T = in.reachAt;
    if (dot(T - O, n) < 0.3f) T += n * (0.3f - dot(T - O, n));
    const vec3 toT = T - O;
    const vec3 side = safeNorm(cross(n, safeNorm(toT, duct)), safeNorm(cross(n, duct), {0, 1, 0}));
    const float dist = length(toT);
    vec3 P1 = O + n * std::max(0.35f, 0.55f * dist);
    vec3 P2 = T;
    if (wind > 0.01f) {  // zusammengezogen, die Hand ueber dem Ziel, bereit zuzuschnappen
        const vec3 back = safeNorm(vec3{O.x - T.x, O.y - T.y, 0.0f}, duct);
        P1 += back * (0.35f * wind) - n * (0.1f * wind);
        P2 = T + (O + n * (0.45f * dist) + back * 0.25f - T) * (0.55f * wind);
    }
    const float waveA = (0.06f + 0.05f * perc_) * (1.0f - 0.7f * close) + 0.16f * burn;
    const float waveF = 1.6f + 6.0f * burn;
    for (int i = 0; i <= 36; ++i) {
        const float u = (float)i / 36.0f;
        vec3 p = O * ((1 - u) * (1 - u)) + P1 * (2 * u * (1 - u)) + P2 * (u * u);
        const float env = sm01(u * 3.0f);  // an der Oeffnung keine Welle
        p += side * (waveA * env * std::sin(u * 7.0f - time_ * waveF)) + cross(side, safeNorm(P2 - O, n)) * (waveA * 0.6f * env * std::sin(u * 5.0f - time_ * waveF * 0.8f + 1.0f));
        w.add(p);
    }
    // --- Kette: das Handgelenk liegt bei t = out, die Glieder dahinter (im Schacht, solange t < 0)
    const float wristT = std::min(out, std::max(0.0f, w.s[w.m - 1] - 0.1f));  // nicht ueber das Ziel hinaus
    for (int i = 0; i <= kSeg; ++i) J[(size_t)(C0 + i)] = w.at(wristT - (float)(kSeg - i) * kSegLen);
    J[ROOT] = O;
    const vec3 Wr = J[(size_t)WRIST];
    const vec3 h = safeNorm(Wr - J[(size_t)(WRIST - 1)], n);
    J[(size_t)KN] = Wr + h * kHand;
    // Verdrehung: jedes Glied etwas weiter gewunden, langsam windend; Bezug quer zum Schacht
    vec3 ref = safeNorm(cross(duct, U), {0, 1, 0});
    for (int i = 0; i < kSeg; ++i) {
        const vec3 c = safeNorm(J[(size_t)(C0 + i + 1)] - J[(size_t)(C0 + i)], n);
        vec3 r = ref - c * dot(ref, c);
        if (length(r) < 0.1f) r = safeNorm(cross(c, duct), {1, 0, 0});
        r = safeNorm(r, {1, 0, 0});
        const float tw = 0.5f * (float)i + 0.3f * std::sin(time_ * 0.7f + (float)i * 0.6f) + 1.2f * burn * std::sin(time_ * 13.0f + (float)i);
        pose_.up[(size_t)(B_SEG0 + i)] = rotateAround(r, c, tw);
        ref = r;
    }
    const float twH = 0.5f * (float)kSeg + 0.3f * std::sin(time_ * 0.7f + (float)kSeg * 0.6f);
    vec3 r0 = ref - h * dot(ref, h);
    r0 = safeNorm(length(r0) < 0.1f ? cross(h, duct) : r0, {1, 0, 0});
    const vec3 hup = rotateAround(r0, h, twH);
    pose_.up[B_HAND] = hup;
    // --- Krallen: drei um die Handachse; offen gespreizt, ausholend weit zurueckgebogen, geschlossen um die Brust gelegt
    const float twitch = (1.0f - close) * (0.08f * std::sin(time_ * 9.0f) + 0.05f * std::sin(time_ * 23.0f)) + burn * 0.5f * std::sin(time_ * 19.0f);
    float alpha = 1.25f + 0.45f * wind - 0.75f * close, beta = 0.32f - 0.2f * wind + 0.62f * close;
    for (int c = 0; c < kClaws; ++c) {
        const vec3 rad = rotateAround(hup, h, (float)c * kTau / (float)kClaws);
        const vec3 tng = safeNorm(cross(h, rad), {0, 0, 1});
        vec3 p = J[(size_t)KN] + rad * 0.035f;
        J[(size_t)claw(c, 0)] = p;
        for (int i = 0; i < 3; ++i) {
            const float a = alpha - beta * (float)i + twitch * (0.6f + 0.4f * hash1((unsigned)(c * 5 + i)));
            const vec3 d = h * std::cos(a) + rad * std::sin(a);
            p += d * kClawL[i];
            J[(size_t)claw(c, i + 1)] = p;
            pose_.up[(size_t)bClaw(c, i)] = tng;
        }
    }
    headPos_ = J[(size_t)KN] + h * 0.18f;  // Mitte zwischen den Krallen (Schreck)
    headDir_ = h;
    // --- Riss in der Decke (nur wo keine Lueftung ist): an der Oeffnung, sonst im Schacht verborgen
    const float cd = X[X_CRACK] > 0.5f ? 0.0f : 0.45f;
    J[CRACK_A] = O - n * cd, J[CRACK_B] = O - n * cd + n * 0.05f;
    pose_.up[B_CRACK] = duct;
    // --- Hautfetzen: verbrennt der Arm, loesen sie sich vom Unterarm und fallen unter die Oeffnung; sonst im Schacht
    const vec3 hidden = O - n * 0.8f;
    for (int k = 0; k < kShreds; ++k) {
        float* c = X + X_SHRED + k * X_SHREDN;
        if (c[15] < 0.5f) {
            if (burn > 0.5f && out > 0.4f) {  // loest sich
                const vec3 p = w.at(wristT - 0.25f - 0.35f * (float)k) + side * (0.04f * (hash1(k * 7u + 1u) - 0.5f));
                const vec3 v = vec3{hash1(k * 7u + 2u) - 0.5f, hash1(k * 7u + 3u) - 0.5f, 0.0f} * 0.8f;
                c[0] = p.x, c[1] = p.y, c[2] = p.z, c[3] = v.x, c[4] = v.y, c[5] = 0.0f;
                c[6] = 1.0f, c[7] = 0.0f, c[8] = 0.0f, c[9] = 0.0f, c[10] = 0.0f, c[11] = 1.0f;
                c[12] = (hash1(k * 7u + 4u) - 0.5f) * 9.0f, c[13] = (hash1(k * 7u + 5u) - 0.5f) * 9.0f, c[14] = (hash1(k * 7u + 6u) - 0.5f) * 9.0f;
                c[15] = 1.0f;
            } else {
                J[(size_t)(SHRED0 + k * 2)] = hidden, J[(size_t)(SHRED0 + k * 2 + 1)] = hidden + vec3{0.02f, 0, 0};
                pose_.up[(size_t)(B_SHRED0 + k)] = U;
                continue;
            }
        }
        vec3 p{c[0], c[1], c[2]}, v{c[3], c[4], c[5]}, ax{c[6], c[7], c[8]}, az{c[9], c[10], c[11]}, wv{c[12], c[13], c[14]};
        if (c[15] < 1.5f) {  // faellt flatternd (Luftwiderstand)
            v.z -= 9.8f * dt;
            v = v * std::pow(0.12f, dt);  // Fetzen bremsen stark
            p += v * dt;
            const float wl = length(wv);
            if (wl > 1e-4f) ax = rotateAround(ax, wv / wl, wl * dt), az = rotateAround(az, wv / wl, wl * dt);
            if (p.z < floorZ + 0.006f) p.z = floorZ + 0.006f, c[15] = 2.0f;
        } else {  // liegt flach
            az = safeNorm(az + (U - az) * std::min(1.0f, dt * 8.0f), U);
            ax = safeNorm(ax - az * dot(ax, az), {1, 0, 0});
        }
        ax = safeNorm(ax - az * dot(ax, az), {1, 0, 0});
        c[0] = p.x, c[1] = p.y, c[2] = p.z, c[3] = v.x, c[4] = v.y, c[5] = v.z;
        c[6] = ax.x, c[7] = ax.y, c[8] = ax.z, c[9] = az.x, c[10] = az.y, c[11] = az.z, c[12] = wv.x, c[13] = wv.y, c[14] = wv.z;
        J[(size_t)(SHRED0 + k * 2)] = p;
        J[(size_t)(SHRED0 + k * 2 + 1)] = p + ax * 0.02f;
        pose_.up[(size_t)(B_SHRED0 + k)] = az;
    }
    // --- Rauch: Fahnen steigen von den Fetzen (oder vom Arm) auf und verschwinden in der Decke
    const float smoke = std::clamp(X[X_SMOKE], 0.0f, 1.0f);
    for (int k = 0; k < kSmoke; ++k) {
        float& ph = X[X_WISP + k * 2];
        const float* sc = X + X_SHRED + (k % kShreds) * X_SHREDN;
        vec3 src = sc[15] > 0.5f ? vec3{sc[0], sc[1], sc[2]} : w.at(wristT - 0.3f * (float)k);
        ph += dt * (0.35f + 0.12f * hash1(k * 3u + 9u));
        if (ph >= 1.0f) ph -= 1.0f;
        const float rise = ph * std::max(0.6f, O.z - src.z - 0.1f);
        const vec3 drift = side * (0.12f * std::sin(time_ * 0.9f + (float)k * 1.7f) * ph) + duct * (0.05f * ph);
        vec3 a = src + U * (0.04f + rise) + drift;
        if (smoke < 0.08f || a.z > O.z - 0.05f) a = hidden;  // aus oder oben angekommen: verborgen
        J[(size_t)(SMOKE0 + k * 2)] = a;
        J[(size_t)(SMOKE0 + k * 2 + 1)] = a + U * 0.12f;
        pose_.up[(size_t)(B_SMOKE0 + k)] = duct;
    }
}

}  // namespace lim::anim
