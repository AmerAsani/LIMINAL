// V13.3 Teil 4: Jerry (Wikidot entity-7 Rev. 14, level-2 Rev. 93) - Skelett und Bewegung (KREATUR_Jerry.md).
//
// Ein blauer Ara auf einem rostigen Rohr. Er bleibt auf dem Rohr (an seinen Raum gebunden); was sich bewegt, ist der Vogel:
//   Kopf      ruckartig wie bei Papageien - der Blick springt von Ziel zu Ziel und haelt dann still (Feder mit hoher Frequenz),
//             neigt sich schief (tilt), dreht weit ueber die Schulter. look/lookAt: Ziel des Blicks.
//   Augen     perceive 0..1: die Pupillen verengen sich ("pinning" - bei Papageien Zeichen hoher Erregung); beim Sprechen
//             pulsieren sie (echtes Verhalten sprechender Papageien).
//   Stimme    talk 0..1: Silben - Schnabel klappt wenig, die dicke schwarze Zunge rollt vor und zurueck, die Kehle hebt sich.
//             jaw 0..1: Schnabel offen (Kreischen, Drohen), der Oberschnabel hebt sich mit.
//   Gefieder  growl 0..1: Halskrause gestraeubt (Massstab des Halsknochens), Schwanz faechert; crouch 0..1: aufgeplustert,
//             zusammengesunken (Ruhe); alert 0..1: schlank und aufrecht.
//   Fluegel   fly 0..1: angelegt .. halb abgespreizt (0,5, "Mantel") .. weit offen (Drohen, Unterwerfung); wave 0..1: Schlagen.
//   Putzen    shiver 0..1: der Kopf faehrt unter den Fluegel und an die Brust, der Schnabel knabbert.
//   Sprung    reach 0..1: Rumpf kippt vor, Hals gestreckt, Schnabel auf; hang 1: in der Luft (Faenge nach vorn gespreizt).
//   Ruhe      Atmen, Schwanzwippen, die Zehen greifen nach, ab und zu ein Schnabelknirschen.
// Koordinaten wie im Spiel: x vorn, y links, z oben; Wurzel oben auf dem Rohr.
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
float wrapA(float a) { return std::remainder(a, kTau); }
void spring(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
vec3 nrm(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
// Drehung von v um die Einheitsachse k (Rodrigues); positiver Winkel: rechtsherum um k
vec3 rot(vec3 v, vec3 k, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.0f - c));
}
vec3 nlerp(vec3 a, vec3 b, float t) { return nrm(a * (1.0f - t) + b * t, b); }
vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
float hashf(unsigned i) {
    i ^= i >> 16, i *= 0x7feb352du, i ^= i >> 15, i *= 0x846ca68bu, i ^= i >> 16;
    return (float)(i & 0xFFFFFF) / 16777216.0f;
}
struct Fr {  // starrer Rahmen: Ursprung, Achsen
    vec3 o, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    vec3 at(vec3 p) const { return o + x * p.x + y * p.y + z * p.z; }
    vec3 dir(vec3 d) const { return x * d.x + y * d.y + z * d.z; }
};

// Masse der Fluegel (m): Oberarm, Unterarm, Hand, Handschwingen, Armschwingen
constexpr float kHum = 0.095f, kFore = 0.10f, kHand = 0.085f;
constexpr float kPrimL[jy::kPrim] = {0.30f, 0.29f, 0.27f, 0.24f};
constexpr float kSecL[jy::kSec] = {0.17f, 0.16f, 0.15f};
// Richtungen (linker Fluegel, Rumpfraum) angelegt und weit offen
const vec3 kFoldHum{-0.75f, 0.15f, -0.64f}, kFoldFore{0.62f, 0.05f, 0.55f}, kFoldHand{-0.80f, 0.02f, -0.60f};
const vec3 kOpenHum{-0.05f, 0.75f, 0.66f}, kOpenFore{0.20f, 0.92f, 0.33f}, kOpenHand{-0.05f, 0.97f, 0.22f};
vec3 foldPrim(int k) { return rot(nrm(vec3{-0.62f, -0.03f, -0.78f}, {0, 0, -1}), {0, 1, 0}, -0.05f * (float)k); }
vec3 foldSec(int k) { return rot(nrm(vec3{-0.55f, -0.02f, -0.83f}, {0, 0, -1}), {0, 1, 0}, 0.12f * (float)k); }
vec3 openSec(int k) {
    static const vec3 d[jy::kSec] = {{-0.80f, 0.55f, 0.05f}, {-0.90f, 0.38f, 0.0f}, {-0.95f, 0.20f, -0.05f}};
    return nrm(d[k], {-1, 0, 0});
}
const vec3 kOpenN{0.05f, -0.25f, 0.97f};  // Oberseite des offenen Fluegels (links)
vec3 mirror(vec3 v, int s) { return s ? vec3{v.x, -v.y, v.z} : v; }

// Belegung von xs_ (Federn und Zeitgeber)
enum X : int {
    X_HY, X_HYV, X_HP, X_HPV, X_TILT, X_TILTV, X_SACC, X_TY, X_TP,  // Kopf: Gier, Nicken (rel. zum Rumpf), Neigung; naechster Ruck
    X_ALERT, X_ALERTV, X_FLUFF, X_FLUFFV, X_RUFF, X_RUFFV, X_JAW, X_JAWV, X_TALK, X_TALKV, X_PIN, X_PINV,
    X_FLY, X_FLYV, X_FLAP, X_FLAPV, X_PREEN, X_PREENV, X_REACH, X_REACHV, X_AIR, X_AIRV, X_LEAN, X_LEANV,
    X_TAIL, X_TAILV, X_GRIND, X_GRIP, X_SYL, X_BLINK, X_PSIDE, X_PT,
    X_SIZE
};
}  // namespace

CreatureSkeleton jy::makeSkeleton() {
    CreatureSkeleton s;
    s.kind = Entity::Jerry;
    s.bind.assign((size_t)JCOUNT, vec3{});
    s.bones.assign((size_t)BCOUNT, CBone{});
    s.bindUp.assign((size_t)BCOUNT, vec3{1, 0, 0});
    auto J = [&](int i, vec3 p) { s.bind[(size_t)i] = p; };
    auto B = [&](int i, int a, int b, int parent, float r, vec3 up, bool shadow = true) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)b, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        s.bindUp[(size_t)i] = up;
    };
    const vec3 F{1, 0, 0}, Y{0, 1, 0}, Z{0, 0, 1};
    J(ROOT, {0, 0, 0});
    J(PELVIS, {-0.035f, 0, 0.115f});
    J(SPINE, {-0.02f, 0, 0.215f});
    J(CHEST, {0.005f, 0, 0.30f});
    J(NECK0, {0.02f, 0, 0.345f});
    J(NECK1, {0.04f, 0, 0.385f});
    J(HEAD, {0.055f, 0, 0.425f});
    J(CROWN, {0.045f, 0, 0.475f});
    J(BEAKP, {0.098f, 0, 0.44f});
    J(BEAKT, {0.135f, 0, 0.36f});
    J(JAWP, {0.088f, 0, 0.415f});
    J(JAWT, {0.13f, 0, 0.385f});
    J(TONGA, {0.095f, 0, 0.41f});
    J(TONGB, {0.118f, 0, 0.398f});
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd ? -1.0f : 1.0f;
        // Auge: Gelenk auf der Hornhaut (Mitte der Pupille) - der Pupillenknochen verengt sich um diesen Punkt (Massstab)
        const vec3 e{0.068f, 0.03f * g, 0.448f}, ed = nrm(vec3{0.45f, 0.89f * g, 0.05f}, Y);
        J(sd ? EYE_R : EYE_L, e + ed * 0.0098f);
        J(sd ? PUP_R : PUP_L, e + ed * 0.0138f);
    }
    J(RUFF, {-0.01f, 0, 0.395f});
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 sh = mirror({0.005f, 0.048f, 0.305f}, sd);
        const vec3 el = sh + mirror(nrm(kFoldHum, F), sd) * kHum;
        const vec3 wr = el + mirror(nrm(kFoldFore, F), sd) * kFore;
        const vec3 ht = wr + mirror(nrm(kFoldHand, F), sd) * kHand;
        J(wingJ(sd, 0), sh), J(wingJ(sd, 1), el), J(wingJ(sd, 2), wr), J(wingJ(sd, 3), ht);
        for (int k = 0; k < kPrim; ++k) J(wingJ(sd, 4 + k), ht + mirror(foldPrim(k), sd) * kPrimL[k]);
        for (int k = 0; k < kSec; ++k) J(wingJ(sd, 4 + kPrim + k), el + mirror(foldSec(k), sd) * kSecL[k]);
        // Beine: Huefte im Bauch, Knie verborgen im Gefieder, Ferse sichtbar, kurzer Lauf, Zehen um das Rohr
        J(legJ(sd, 0), mirror({-0.02f, 0.032f, 0.115f}, sd));
        J(legJ(sd, 1), mirror({0.025f, 0.04f, 0.085f}, sd));
        J(legJ(sd, 2), mirror({-0.005f, 0.042f, 0.05f}, sd));
        J(legJ(sd, 3), mirror({0.002f, 0.042f, 0.012f}, sd));
        static const vec3 toe[kToes][2] = {{{0.035f, 0.055f, 0.0f}, {0.05f, 0.058f, -0.035f}},
                                           {{0.035f, 0.032f, -0.002f}, {0.048f, 0.03f, -0.038f}},
                                           {{-0.03f, 0.035f, -0.003f}, {-0.042f, 0.033f, -0.035f}},
                                           {{-0.028f, 0.055f, 0.0f}, {-0.04f, 0.06f, -0.03f}}};
        for (int t = 0; t < kToes; ++t) J(legJ(sd, 4 + t * 2), mirror(toe[t][0], sd)), J(legJ(sd, 5 + t * 2), mirror(toe[t][1], sd));
    }
    J(TAIL0, {-0.07f, 0, 0.12f});
    J(TAIL1, {-0.15f, 0, -0.12f});
    J(TAIL2, {-0.19f, 0, -0.42f});

    B(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    B(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.07f, F);
    B(B_SPINE, SPINE, CHEST, B_PELVIS, 0.07f, F);
    B(B_CHEST, CHEST, NECK0, B_SPINE, 0.055f, F);
    B(B_NECK0, NECK0, NECK1, B_CHEST, 0.04f, F);
    B(B_NECK1, NECK1, HEAD, B_NECK0, 0.04f, F);
    B(B_HEAD, HEAD, CROWN, B_NECK1, 0.045f, F);
    B(B_BEAK, BEAKP, BEAKT, B_HEAD, 0.025f, Z);
    B(B_JAW, JAWP, JAWT, B_HEAD, 0.02f, Z);
    B(B_TONGUE, TONGA, TONGB, B_JAW, 0.008f, Z, false);
    B(B_PUP_L, EYE_L, PUP_L, B_HEAD, 0.006f, Z, false);
    B(B_PUP_R, EYE_R, PUP_R, B_HEAD, 0.006f, Z, false);
    B(B_RUFF, NECK0, RUFF, B_CHEST, 0.03f, Z, false);
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 n = sd ? -Y : Y;
        B(wingB(sd, 0), wingJ(sd, 0), wingJ(sd, 1), B_CHEST, 0.025f, n);
        B(wingB(sd, 1), wingJ(sd, 1), wingJ(sd, 2), wingB(sd, 0), 0.022f, n);
        B(wingB(sd, 2), wingJ(sd, 2), wingJ(sd, 3), wingB(sd, 1), 0.02f, n);
        for (int k = 0; k < kPrim; ++k) B(wingB(sd, 3 + k), wingJ(sd, 3), wingJ(sd, 4 + k), wingB(sd, 2), 0.02f, n);
        for (int k = 0; k < kSec; ++k) B(wingB(sd, 3 + kPrim + k), wingJ(sd, 1), wingJ(sd, 4 + kPrim + k), wingB(sd, 1), 0.025f, n);
        B(legB(sd, 0), legJ(sd, 0), legJ(sd, 1), B_PELVIS, 0.025f, Y, false);
        B(legB(sd, 1), legJ(sd, 1), legJ(sd, 2), legB(sd, 0), 0.015f, Y, false);
        B(legB(sd, 2), legJ(sd, 2), legJ(sd, 3), legB(sd, 1), 0.01f, Y, false);
        for (int t = 0; t < kToes; ++t) {
            B(legB(sd, 3 + t * 2), legJ(sd, 3), legJ(sd, 4 + t * 2), legB(sd, 2), 0.007f, Y, false);
            B(legB(sd, 4 + t * 2), legJ(sd, 4 + t * 2), legJ(sd, 5 + t * 2), legB(sd, 3 + t * 2), 0.006f, Y, false);
        }
    }
    B(B_TAIL0, TAIL0, TAIL1, B_PELVIS, 0.035f, Y);
    B(B_TAIL0 + 1, TAIL1, TAIL2, B_TAIL0, 0.03f, Y);
    s.height = 0.5f;
    s.reach = 0.75f;
    s.center = {-0.04f, 0.0f, 0.12f};
    return s;
}

void CreatureAnimator::updateJerry(float dt, const Input& in) {
    using namespace jy;
    auto& P = pose_.j;
    const auto& Bd = sk_->bind;
    if ((int)xs_.size() != X_SIZE) {
        xs_.assign((size_t)X_SIZE, 0.0f);
        xs_[X_SACC] = 0.3f, xs_[X_GRIND] = 6.0f, xs_[X_BLINK] = 3.0f;
    }
    float* X = xs_.data();
    if ((int)pose_.s.size() != BCOUNT) pose_.s.assign((size_t)BCOUNT, 1.0f);
    const float t = time_;
    // --- Federn der Eingaben -----------------------------------------------------------------------------------------
    spring(X[X_ALERT], X[X_ALERTV], in.alert, 5.0f, dt);
    spring(X[X_FLUFF], X[X_FLUFFV], in.crouch, 1.6f, dt);
    spring(X[X_RUFF], X[X_RUFFV], std::max(in.growl, 0.6f * in.reach), 7.0f, dt);
    spring(X[X_TALK], X[X_TALKV], in.talk, 9.0f, dt);
    spring(X[X_PIN], X[X_PINV], in.perceive, 10.0f, dt);
    spring(X[X_FLY], X[X_FLYV], std::max(in.fly, 0.85f * in.reach), in.fly > X[X_FLY] ? 7.5f : 3.5f, dt);
    spring(X[X_FLAP], X[X_FLAPV], in.wave, 6.0f, dt);
    spring(X[X_PREEN], X[X_PREENV], in.shiver, 3.0f, dt);
    spring(X[X_REACH], X[X_REACHV], in.reach, 9.0f, dt);
    spring(X[X_AIR], X[X_AIRV], in.hang, 6.0f, dt);
    spring(X[X_LEAN], X[X_LEANV], in.lean, 3.0f, dt);
    const float alert = std::clamp(X[X_ALERT], 0.0f, 1.0f), fluff = std::clamp(X[X_FLUFF], 0.0f, 1.0f) * (1.0f - alert);
    const float ruff = std::clamp(X[X_RUFF], 0.0f, 1.0f), talk = std::clamp(X[X_TALK], 0.0f, 1.0f), pin = std::clamp(X[X_PIN], 0.0f, 1.0f);
    const float fly = std::clamp(X[X_FLY], 0.0f, 1.0f), flap = std::clamp(X[X_FLAP], 0.0f, 1.0f);
    const float preen = std::clamp(X[X_PREEN], 0.0f, 1.0f) * (1.0f - alert), reach = std::clamp(X[X_REACH], 0.0f, 1.0f);
    const float air = std::clamp(X[X_AIR], 0.0f, 1.0f);

    // --- Rumpf: Atmen, Wiegen, Vorkippen (Sprung), aufrecht (wach), zusammengesunken (Ruhe) --------------------------------
    const float breath = std::sin(t * 5.6f) * (0.5f + 0.5f * fluff);
    const float pitch = -0.06f * alert + 0.06f * fluff + 0.55f * reach + 0.08f * X[X_LEAN] + 0.012f * std::sin(t * 0.7f) + 0.03f * talk * std::sin(t * 3.1f);
    const float roll = 0.02f * std::sin(t * 0.43f + 1.0f) * (1.0f - alert);
    Fr root;
    root.o = pos_;
    root.x = dirYaw(yaw_);
    root.y = {-root.x.y, root.x.x, 0.0f};
    root.z = {0, 0, 1};
    Fr body = root;  // um die Wurzel gekippt (vor: +pitch)
    body.x = rot(root.x, root.y, pitch);
    body.z = rot(root.z, root.y, pitch);
    body.y = rot(root.y, body.x, roll), body.z = rot(body.z, body.x, roll);
    body.o = pos_ - vec3{0, 0, 0.018f * fluff} + root.x * (0.05f * reach);
    for (int j : {(int)PELVIS, (int)SPINE, (int)CHEST, (int)NECK0}) P[(size_t)j] = body.at(Bd[(size_t)j]);
    P[CHEST] += body.x * (0.0035f * breath);
    // Massstab: aufgeplustert (Ruhe), Atmen in der Brust, Halskrause gestraeubt, Pupillen
    const float puff = 1.0f + 0.07f * fluff - 0.025f * alert;
    pose_.s[jy::B_PELVIS] = puff;
    pose_.s[jy::B_SPINE] = puff * (1.0f + 0.012f * breath);
    pose_.s[jy::B_CHEST] = puff * (1.0f + 0.01f * breath);
    pose_.s[jy::B_RUFF] = 1.0f + 0.32f * ruff + 0.06f * fluff;
    // Augen: verengt (Erregung), beim Sprechen pulsierend ("pinning"); im Dunkeln weit
    X[X_PT] += dt * (4.0f + 7.0f * talk);
    const float pulse = talk > 0.05f ? 0.5f + 0.5f * std::sin(X[X_PT]) : 0.0f;
    const float pupil = std::clamp(1.0f - 0.55f * pin - 0.25f * talk * pulse, 0.3f, 1.15f);
    pose_.s[jy::B_PUP_L] = pose_.s[jy::B_PUP_R] = pupil;

    // --- Kopf: ruckartige Blicke; Ziel aus lookAt, sonst wandernd; Putzen fuehrt ihn an Fluegel und Brust ----------------
    float wantY = X[X_TY], wantP = X[X_TP];
    X[X_SACC] -= dt;
    if (in.look) {
        const vec3 d = in.lookAt - body.at(Bd[HEAD]);
        const float ly = std::atan2(dot(d, root.y), dot(d, root.x));
        const float lp = std::atan2(d.z, std::max(0.05f, std::hypot(dot(d, root.x), dot(d, root.y))));
        // Papageien sehen mit einem Auge: der Kopf dreht sich etwas zur Seite (ein Auge zum Ziel), dann wieder frontal
        const float eye = (alert > 0.5f && std::fmod(t, 3.7f) < 1.6f) ? 0.45f * (std::sin(t * 0.9f) > 0 ? 1.0f : -1.0f) : 0.0f;
        if (X[X_SACC] <= 0.0f || std::fabs(wrapA(ly + eye - X[X_TY])) > 0.35f) {
            wantY = ly + eye, wantP = lp;
            X[X_SACC] = 0.25f + 0.35f * hashf(rng_ + (unsigned)(t * 13.0f));
        }
    } else if (X[X_SACC] <= 0.0f) {  // umsehen: Spruenge, dazwischen Stillhalten
        const float r1 = hashf(rng_ * 7u + (unsigned)(t * 31.0f)), r2 = hashf(rng_ * 13u + (unsigned)(t * 17.0f));
        wantY = (r1 - 0.5f) * 2.6f * (1.0f - 0.6f * fluff);
        wantP = (r2 - 0.4f) * 0.6f - 0.25f * fluff;
        X[X_SACC] = 0.5f + 1.8f * hashf(rng_ * 3u + (unsigned)(t * 5.0f));
    }
    if (preen > 0.05f) {  // unter den Fluegel, an die Brust; die Seite wechselt
        X[X_PSIDE] -= dt;
        if (X[X_PSIDE] <= -2.4f) X[X_PSIDE] = 2.4f;
        const float side = X[X_PSIDE] > 0.0f ? 1.0f : -1.0f;
        wantY = wantY * (1.0f - preen) + side * 2.2f * preen;
        wantP = wantP * (1.0f - preen) + (-0.75f) * preen;
    }
    if (reach > 0.05f) wantP = wantP * (1.0f - reach) + std::min(wantP, 0.05f) * reach;
    wantY = std::clamp(wrapA(wantY), -2.7f, 2.7f), wantP = std::clamp(wantP, -1.1f, 0.9f);
    X[X_TY] = wantY, X[X_TP] = wantP;
    spring(X[X_HY], X[X_HYV], wantY, 16.0f, dt);
    spring(X[X_HP], X[X_HPV], wantP, 14.0f, dt);
    spring(X[X_TILT], X[X_TILTV], in.tilt + 0.35f * std::sin(t * 0.37f) * (1.0f - alert) * (1.0f - preen) + 0.4f * preen, 6.0f, dt);
    const float hy = X[X_HY], hp = X[X_HP];
    vec3 hx = rot(root.x, {0, 0, 1}, hy);
    hx = nrm(hx * std::cos(hp) + vec3{0, 0, std::sin(hp)}, root.x);
    vec3 hz = nrm(vec3{0, 0, 1} - hx * hx.z, {0, 0, 1});
    hz = rot(hz, hx, X[X_TILT]);
    const vec3 hyv = cross(hz, hx);
    // Hals: dreht ein Stueck mit, streckt sich beim Sprung; der Kopf sitzt etwas zur Blickrichtung versetzt
    const vec3 neckB = Bd[HEAD] - Bd[NECK0];
    vec3 nv = rot(body.dir(neckB), body.z, 0.35f * hy) * (1.0f + 0.6f * reach + 0.08f * alert - 0.12f * fluff);
    nv += hx * (0.012f + 0.03f * reach) - body.z * (0.02f * preen);
    P[HEAD] = P[NECK0] + nv;
    P[NECK1] = (P[NECK0] + P[HEAD]) * 0.5f - body.x * 0.008f + hx * 0.004f + body.z * (0.004f * talk * std::sin(t * 9.0f));
    // Kopfgelenke aus der Ruhepose (Kopfraum der Ruhe: x vorn, y links, z oben)
    auto headAt = [&](int j) {
        const vec3 o = Bd[(size_t)j] - Bd[HEAD];
        return P[HEAD] + hx * o.x + hyv * o.y + hz * o.z;
    };
    for (int j : {(int)CROWN, (int)BEAKP, (int)BEAKT, (int)JAWP, (int)JAWT, (int)TONGA, (int)TONGB, (int)EYE_L, (int)PUP_L, (int)EYE_R, (int)PUP_R})
        P[(size_t)j] = headAt(j);
    // Schnabel: Silben (Sprechen), Knirschen (Ruhe), Knabbern (Putzen), weit offen (Kreischen, Sprung)
    X[X_GRIND] -= dt;
    float grind = 0.0f;
    if (X[X_GRIND] < 0.0f) {
        grind = 0.04f * std::sin(t * 38.0f);
        if (X[X_GRIND] < -1.4f) X[X_GRIND] = 5.0f + 9.0f * hashf(rng_ + (unsigned)(t * 3.0f));
    }
    X[X_SYL] += dt * (5.5f + 2.0f * std::sin(t * 0.8f));
    const float syl = talk * (0.5f + 0.5f * std::sin(X[X_SYL])) * (0.6f + 0.4f * std::sin(X[X_SYL] * 0.37f + 1.0f));
    const float nib = preen * 0.12f * (0.5f + 0.5f * std::sin(t * 17.0f));
    spring(X[X_JAW], X[X_JAWV], std::max({in.jaw, 0.9f * reach, syl * 0.32f, nib}) + grind, 22.0f, dt);
    const float jaw = std::clamp(X[X_JAW], -0.05f, 1.0f);
    P[BEAKT] = P[BEAKP] + rot(P[BEAKT] - P[BEAKP], hyv, -0.14f * jaw);
    P[JAWT] = P[JAWP] + rot(P[JAWT] - P[JAWP], hyv, 0.62f * jaw);
    // Zunge: liegt im Unterschnabel, rollt beim Sprechen vor und hebt sich
    const float tg = talk * std::sin(X[X_SYL] * 1.3f + 0.6f);
    P[TONGA] = P[JAWP] + rot(P[TONGA] - P[JAWP], hyv, 0.62f * jaw * 0.8f);
    P[TONGB] = P[TONGA] + rot(P[TONGB] - P[TONGA], hyv, 0.62f * jaw * 0.6f - 0.35f * tg) * (1.0f + 0.25f * talk * (0.5f + 0.5f * tg));
    headPos_ = (P[EYE_L] + P[EYE_R]) * 0.5f + hx * 0.02f;
    headDir_ = hx;
    P[RUFF] = body.at(Bd[RUFF]);

    // --- Fluegel: angelegt .. offen, Schlagen ------------------------------------------------------------------------
    const float u = sm01(fly);
    const float flapA = flap * 0.62f * std::sin(t * kTau * 3.6f);
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd ? -1.0f : 1.0f;
        // Schlag: um die Laengsachse des Rumpfs, links und rechts gegengleich (beide heben sich zugleich)
        const float el = flapA + (fly > 0.6f ? 0.08f * std::sin(t * 2.1f + (float)sd) * (1.0f - flap) : 0.0f);
        auto openD = [&](vec3 d) { return rot(mirror(nrm(d, {0, 1, 0}), sd), {1, 0, 0}, el * g); };
        const vec3 fn = mirror(vec3{0, 1, 0}, sd), on = openD(kOpenN);
        const vec3 n = body.dir(nlerp(fn, on, u));
        const vec3 dH = body.dir(nlerp(mirror(nrm(kFoldHum, {1, 0, 0}), sd), openD(kOpenHum), u));
        const vec3 dF = body.dir(nlerp(mirror(nrm(kFoldFore, {1, 0, 0}), sd), openD(kOpenFore), u));
        const vec3 dA = body.dir(nlerp(mirror(nrm(kFoldHand, {1, 0, 0}), sd), openD(kOpenHand), u));
        const vec3 sh = body.at(Bd[(size_t)wingJ(sd, 0)]) + body.y * (g * 0.006f * fluff);
        P[(size_t)wingJ(sd, 0)] = sh;
        P[(size_t)wingJ(sd, 1)] = sh + dH * kHum;
        P[(size_t)wingJ(sd, 2)] = P[(size_t)wingJ(sd, 1)] + dF * kFore;
        P[(size_t)wingJ(sd, 3)] = P[(size_t)wingJ(sd, 2)] + dA * kHand;
        // angelegt etwas abgehoben, wenn aufgeplustert (die Fluegel liegen dann lose ueber dem Gefieder)
        for (int k = 0; k < kPrim; ++k) {
            const vec3 open = rot(openD(kOpenHand), on, g * (0.15f + 0.25f * (float)k));  // Faecher nach hinten
            const vec3 d = body.dir(nlerp(mirror(foldPrim(k), sd), open, u));
            P[(size_t)wingJ(sd, 4 + k)] = P[(size_t)wingJ(sd, 3)] + d * kPrimL[k];
            pose_.up[(size_t)wingB(sd, 3 + k)] = nrm(n - d * dot(n, d), n);
        }
        for (int k = 0; k < kSec; ++k) {
            const vec3 d = body.dir(nlerp(mirror(foldSec(k), sd), rot(mirror(openSec(k), sd), {1, 0, 0}, el * g), u));
            P[(size_t)wingJ(sd, 4 + kPrim + k)] = P[(size_t)wingJ(sd, 1)] + d * kSecL[k];
            pose_.up[(size_t)wingB(sd, 3 + kPrim + k)] = nrm(n - d * dot(n, d), n);
        }
        pose_.up[(size_t)wingB(sd, 0)] = nrm(n - dH * dot(n, dH), n);
        pose_.up[(size_t)wingB(sd, 1)] = nrm(n - dF * dot(n, dF), n);
        pose_.up[(size_t)wingB(sd, 2)] = nrm(n - dA * dot(n, dA), n);
    }

    // --- Beine: Fuesse greifen das Rohr (Rahmen ohne Kippen); in der Luft nach vorn gestreckt, Zehen gespreizt ------------
    Fr feet = root;
    X[X_GRIP] += dt;
    const float regrip = std::fmod(X[X_GRIP], 9.0f) < 0.35f ? 1.0f : 0.0f;  // ab und zu loest ein Fuss und greift nach
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd ? -1.0f : 1.0f;
        const vec3 hip = body.at(Bd[(size_t)legJ(sd, 0)]);
        vec3 foot = feet.at(Bd[(size_t)legJ(sd, 3)]);
        vec3 ankle = feet.at(Bd[(size_t)legJ(sd, 2)]) + vec3{0, 0, -0.008f * fluff};
        const float lift = (sd == 0 ? regrip : 0.0f) * 0.012f;
        foot += vec3{0, 0, lift}, ankle += vec3{0, 0, lift};
        if (air > 0.01f) {  // Faenge nach vorn, wie zum Zupacken
            const vec3 fa = hip + body.x * 0.09f - body.z * 0.06f + body.y * (g * 0.035f);
            foot = foot * (1.0f - air) + fa * air;
            ankle = ankle * (1.0f - air) + (hip + (fa - hip) * 0.55f + body.x * 0.01f) * air;
        }
        P[(size_t)legJ(sd, 0)] = hip;
        P[(size_t)legJ(sd, 2)] = ankle;
        P[(size_t)legJ(sd, 3)] = foot;
        // Knie: zwischen Huefte und Ferse, nach vorn gebeugt (Laengen der Ruhepose)
        const float l1 = length(Bd[(size_t)legJ(sd, 1)] - Bd[(size_t)legJ(sd, 0)]), l2 = length(Bd[(size_t)legJ(sd, 2)] - Bd[(size_t)legJ(sd, 1)]);
        const vec3 d = ankle - hip;
        const float dl = std::clamp(length(d), 0.01f, l1 + l2 - 1e-4f);
        const vec3 dn = nrm(d, {0, 0, -1});
        const float a = (l1 * l1 - l2 * l2 + dl * dl) / (2.0f * dl);
        const float h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
        const vec3 bend = nrm(body.x - dn * dot(body.x, dn), body.x);
        P[(size_t)legJ(sd, 1)] = hip + dn * a + bend * h;
        const float curl = air > 0.01f ? -0.6f * air : 0.06f * alert + 0.04f * std::sin(t * 1.3f + (float)sd) * (1.0f - alert);
        for (int tt = 0; tt < kToes; ++tt) {
            const vec3 m0 = Bd[(size_t)legJ(sd, 4 + tt * 2)] - Bd[(size_t)legJ(sd, 3)];
            const vec3 m1 = Bd[(size_t)legJ(sd, 5 + tt * 2)] - Bd[(size_t)legJ(sd, 4 + tt * 2)];
            const vec3 ax = tt < 2 ? feet.y : -feet.y;  // vordere Zehen krallen nach hinten-unten, hintere nach vorn-unten
            const vec3 mid = foot + rot(feet.dir(m0), ax, curl * 0.5f);
            P[(size_t)legJ(sd, 4 + tt * 2)] = mid;
            P[(size_t)legJ(sd, 5 + tt * 2)] = mid + rot(feet.dir(m1), ax, curl);
            pose_.up[(size_t)legB(sd, 3 + tt * 2)] = feet.y;
            pose_.up[(size_t)legB(sd, 4 + tt * 2)] = feet.y;
        }
        pose_.up[(size_t)legB(sd, 0)] = body.y;
        pose_.up[(size_t)legB(sd, 1)] = body.y;
        pose_.up[(size_t)legB(sd, 2)] = feet.y;
    }

    // --- Schwanz: haengt hinter dem Rohr (halb der Schwerkraft folgend), wippt; gestraeubt/aufgeregt hebt er sich ----------
    spring(X[X_TAIL], X[X_TAILV], 0.25f * talk * std::sin(t * 6.0f) + 0.18f * ruff + (std::fmod(t, 6.3f) < 0.25f ? 0.25f : 0.0f) - 0.3f * reach, 10.0f, dt);
    P[TAIL0] = body.at(Bd[TAIL0]);
    const vec3 tl1 = nrm(Bd[TAIL1] - Bd[TAIL0], {0, 0, -1}), tl2 = nrm(Bd[TAIL2] - Bd[TAIL1], {0, 0, -1});
    const float tw = X[X_TAIL], sway = 0.05f * std::sin(t * 0.9f) * (1.0f - alert);
    const vec3 d1 = rot(rot(nlerp(body.dir(tl1), root.dir(tl1), 0.5f), root.y, tw), root.z, sway);
    const vec3 d2 = rot(rot(nlerp(body.dir(tl2), root.dir(tl2), 0.7f), root.y, 0.6f * tw + 0.04f * std::sin(t * 1.7f)), root.z, 1.6f * sway);
    P[TAIL1] = P[TAIL0] + d1 * length(Bd[TAIL1] - Bd[TAIL0]);
    P[TAIL2] = P[TAIL1] + d2 * length(Bd[TAIL2] - Bd[TAIL1]);
    pose_.up[jy::B_TAIL0] = body.y, pose_.up[jy::B_TAIL0 + 1] = body.y;

    // --- Oben-Richtungen der uebrigen Knochen ------------------------------------------------------------------------
    P[ROOT] = pos_;
    pose_.up[jy::B_ROOT] = root.x;
    for (int b : {(int)jy::B_PELVIS, (int)jy::B_SPINE, (int)jy::B_CHEST, (int)jy::B_NECK0}) pose_.up[(size_t)b] = body.x;
    pose_.up[jy::B_NECK1] = nrm(body.x + hx, body.x);
    pose_.up[jy::B_HEAD] = hx;
    pose_.up[jy::B_BEAK] = hz, pose_.up[jy::B_JAW] = hz, pose_.up[jy::B_TONGUE] = hz;
    pose_.up[jy::B_PUP_L] = hz, pose_.up[jy::B_PUP_R] = hz;
    pose_.up[jy::B_RUFF] = body.z;
}

}  // namespace lim::anim
