// V13.3: Growler (Entity 130, Wikidot trimmed:entity-130; KREATUR_Growler.md) - Skelett und Bewegung.
//
// Eine grob menschenaehnliche Masse aus bunten Pfeifenreinigern, staendig vornuebergebeugt: kurze Beine, lange Arme, alle
// Glieder enden in einem Gewirr aus Draht, das hinter ihm ueber den Boden schleift. Kein Kopf - nur der Knoten auf der
// Brust, aus dem die Kabel auseinanderlaufen (Level-1-Seite). Blind: es "sieht" mit Klicks (Echoortung).
//
//   Ruhe       wiegt sich, die Fasern zucken, eine Ranke tastet; der Knoten zittert
//   Gang       torkelnd: schwere, kurze Schritte, der Rumpf rollt von Seite zu Seite, die Arme pendeln, das Gewirr schleift
//   Wahrnehmen haelt inne, der Knoten dreht sich zum Geraeusch und flackert (klickt), der Rumpf neigt sich hin
//   Jagen      weit vorgebeugt, die Arme vorgestreckt, schneller Torkelgang
//   Ausholen   richtet sich auf, breitet die Arme, die Drahtrippen straeuben sich, die Ranken steigen (Ankuendigung)
//   Angriff    stoesst vor, die Arme umschlingen, die Rippen fahren ganz aus, die Ranken peitschen (strike)
//   Aufnehmen  die Brustklappen oeffnen sich (jaw) - dahinter dunkles Gewirr und das einzige Organ, ein blasser Nervenstrang
//   Verwirrt   (schwerer Gegenstand am Kopf) taumelt, schlaegt mit den Armen um sich, der Knoten dreht durch (shiver)
//   Tod        sackt in sich zusammen, die Glieder fallen auseinander (hang)
// Groesse: Massstab um die Wurzel (Junge in Level 2 ~0,5, ausgewachsen 1, verschmolzen bis 1,6).
#include <algorithm>
#include <cmath>

#include "anim/creature_anim.hpp"

namespace lim::anim {

// anim/skeleton.hpp (nicht eingebunden: dort heissen die Knochen des Menschen gleich)
void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);
mat4 rigidInverse(const mat4& m);

namespace {
vec3 rotV(vec3 v, vec3 axis, float ang) {  // wie rotateAround (anim/skeleton.hpp)
    const float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
constexpr float kPiG = 3.14159265f, kTauG = 6.2831853f;
float wrapG(float a) { return std::remainder(a, kTauG); }
float smG(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 yawDir(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
void sprG(float& x, float& v, float target, float omega, float dt) {
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
R3 rotAx(vec3 a, float ang) { return R3{rotV({1, 0, 0}, a, ang), rotV({0, 1, 0}, a, ang), rotV({0, 0, 1}, a, ang)}; }
vec3 nrmG(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
float hashG(unsigned a) {
    a ^= a >> 16, a *= 0x7feb352du, a ^= a >> 15, a *= 0x846ca68bu, a ^= a >> 16;
    return (float)(a & 0xFFFFFF) / 16777216.0f;
}

// Baukasten wie in creature.cpp
struct GB {
    CreatureSkeleton s;
    GB(Entity e, int joints, int bones) {
        s.kind = e;
        s.bind.assign((size_t)joints, vec3{});
        s.bones.assign((size_t)bones, CBone{});
        s.bindUp.assign((size_t)bones, vec3{0, 0, 1});
    }
    void j(int i, vec3 p) { s.bind[(size_t)i] = p; }
    void b(int i, int a, int bb, int parent, float r, vec3 up, bool shadow = true) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)bb, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        s.bindUp[(size_t)i] = up;
    }
    vec3 dir(int a, int b) const { return nrmG(s.bind[(size_t)b] - s.bind[(size_t)a], {1, 0, 0}); }
};
vec3 upOf(vec3 d) { return std::fabs(d.z) > 0.8f ? vec3{1, 0, 0} : vec3{0, 0, 1}; }

// Spitzenbuendel: Ansatz auf der Rumpfoberflaeche, Richtung nach aussen
struct Spike {
    vec3 at, dir;
};
const Spike kSpk[gw::kSpikes] = {{{0.50f, 0.26f, 1.48f}, {0.7f, 0.7f, 0.12f}}, {{0.50f, -0.26f, 1.48f}, {0.7f, -0.7f, 0.12f}},
                                 {{0.36f, 0.34f, 1.12f}, {0.5f, 0.86f, 0.0f}},   {{0.36f, -0.34f, 1.12f}, {0.5f, -0.86f, 0.0f}},
                                 {{0.32f, 0.22f, 0.92f}, {0.62f, 0.6f, -0.3f}},  {{0.32f, -0.22f, 0.92f}, {0.62f, -0.6f, -0.3f}}};
constexpr float kSpkIn = 0.08f, kSpkOut = 0.10f;  // Knochen: so tief im Rumpf .. so weit hinaus (Ruhe)

// Zustand der Bewegung (CreatureAnimator::ext_)
struct GState {
    float chainScale = -1.0f;
    float sway = 0.0f, swayV = 0.0f, pitch = 0.0f, pitchV = 0.0f, rear = 0.0f, rearV = 0.0f;
    float spike = 0.0f, spikeV = 0.0f, door = 0.0f, doorV = 0.0f, lunge = 0.0f, lungeV = 0.0f, confuse = 0.0f, confuseV = 0.0f;
    float slump = 0.0f, slumpV = 0.0f, hunt = 0.0f, huntV = 0.0f, stat = 0.0f, statV = 0.0f;
    float twistY = 0.0f, twistYV = 0.0f, knotSpin = 0.0f;
    float twitchT = 2.0f, twitchK = -1.0f, twitchPh = 0.0f;
    vec3 flail[2]{};
    float flailT = 0.0f;
};
}  // namespace

CreatureSkeleton makeGrowlerSkeleton() {
    using namespace gw;
    GB k(Entity::Growler, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(PELVIS, {-0.06f, 0, 0.80f});
    k.j(SPINE, {0.06f, 0, 1.28f});
    k.j(CHEST, {0.26f, 0, 1.62f});
    k.j(HUMP, {0.10f, 0, 2.02f});
    k.j(KNOT, {0.58f, 0, 1.55f});
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        k.j(s == 0 ? DOORH_L : DOORH_R, {0.36f, sg * 0.24f, 1.18f});
        k.j(s == 0 ? DOORT_L : DOORT_R, {0.50f, sg * 0.03f, 1.18f});
        // Arme: lang, haengen bis zum Boden, das Gewirr schleift hinterher
        const vec3 arm[kArmJ] = {{0.20f, sg * 0.46f, 1.74f}, {0.42f, sg * 0.66f, 1.20f}, {0.52f, sg * 0.64f, 0.64f}, {0.50f, sg * 0.62f, 0.30f},
                                 {0.40f, sg * 0.60f, 0.06f}, {0.12f, sg * 0.58f, 0.02f}, {-0.18f, sg * 0.56f, 0.02f}};
        for (int i = 0; i < kArmJ; ++i) k.j(armJ(s, i), arm[i]);
        // kurze Beine; vom Knoechel schleift ein Gewirr nach hinten
        const vec3 leg[kLegJ] = {{-0.04f, sg * 0.22f, 0.80f}, {0.14f, sg * 0.27f, 0.46f}, {-0.02f, sg * 0.28f, 0.10f},
                                 {0.16f, sg * 0.30f, 0.02f},  {-0.22f, sg * 0.32f, 0.02f}, {-0.46f, sg * 0.34f, 0.02f}};
        for (int i = 0; i < kLegJ; ++i) k.j(legJ(s, i), leg[i]);
    }
    for (int q = 0; q < kSpikes; ++q) {
        const vec3 d = nrmG(kSpk[q].dir, {1, 0, 0});
        k.j(SPK0 + q * 2, kSpk[q].at - d * kSpkIn);
        k.j(SPK0 + q * 2 + 1, kSpk[q].at + d * kSpkOut);
    }
    // Ranken: vom Buckel nach oben und hinten, dann hinab
    const vec3 base[kTend] = {{-0.05f, 0.25f, 1.95f}, {-0.12f, 0.0f, 2.03f}, {-0.05f, -0.25f, 1.95f}};
    for (int t = 0; t < kTend; ++t) {
        const float sy = t == 0 ? 1.0f : (t == 2 ? -1.0f : 0.0f);
        vec3 p = base[t];
        const vec3 st[4] = {{-0.18f, sy * 0.05f, 0.22f}, {-0.28f, sy * 0.06f, 0.06f}, {-0.26f, sy * 0.04f, -0.18f}, {-0.16f, sy * 0.02f, -0.28f}};
        k.j(tendJ(t, 0), p);
        for (int i = 0; i < 4; ++i) k.j(tendJ(t, i + 1), p = p + st[i]);
    }
    const vec3 F{1, 0, 0}, Z{0, 0, 1};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.30f, F);
    k.b(B_SPINE, SPINE, CHEST, B_PELVIS, 0.34f, F);
    k.b(B_HUMP, CHEST, HUMP, B_SPINE, 0.36f, F);
    k.b(B_KNOT, CHEST, KNOT, B_SPINE, 0.10f, Z, false);
    k.b(B_DOOR_L, DOORH_L, DOORT_L, B_SPINE, 0.06f, Z, false);
    k.b(B_DOOR_R, DOORH_R, DOORT_R, B_SPINE, 0.06f, Z, false);
    for (int q = 0; q < kSpikes; ++q) k.b(B_SPK0 + q, SPK0 + q * 2, SPK0 + q * 2 + 1, q < 2 ? B_SPINE : B_PELVIS, 0.03f, upOf(k.dir(SPK0 + q * 2, SPK0 + q * 2 + 1)), false);
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < 6; ++i) {
            const int a = armJ(s, i), b = armJ(s, i + 1);
            k.b(armB(s, i), a, b, i == 0 ? B_HUMP : armB(s, i - 1), i < 2 ? 0.10f - 0.02f * (float)i : 0.05f, upOf(k.dir(a, b)), i < 3);
        }
        k.b(legB(s, 0), legJ(s, 0), legJ(s, 1), B_PELVIS, 0.12f, F);
        k.b(legB(s, 1), legJ(s, 1), legJ(s, 2), legB(s, 0), 0.09f, F);
        k.b(legB(s, 2), legJ(s, 2), legJ(s, 3), legB(s, 1), 0.06f, Z, false);
        k.b(legB(s, 3), legJ(s, 2), legJ(s, 4), legB(s, 1), 0.04f, Z, false);
        k.b(legB(s, 4), legJ(s, 4), legJ(s, 5), legB(s, 3), 0.03f, Z, false);
    }
    for (int t = 0; t < kTend; ++t)
        for (int i = 0; i < 4; ++i)
            k.b(tendB(t, i), tendJ(t, i), tendJ(t, i + 1), i == 0 ? B_HUMP : tendB(t, i - 1), 0.04f - 0.007f * (float)i,
                upOf(k.dir(tendJ(t, i), tendJ(t, i + 1))), i < 2);
    k.s.height = 2.1f;
    k.s.reach = 1.7f;
    k.s.center = {0.1f, 0, 1.1f};
    return k.s;
}

// Bewegung. Alle Masse der Ruhepose gehen mit dem Massstab s (scale_) ein; die Knochen bekommen denselben Massstab
// (pose_.s), so waechst das Netz mit.
void CreatureAnimator::updateGrowler(float dt, const Input& in) {
    using namespace gw;
    if (!ext_) ext_ = std::make_shared<GState>();
    GState& g = *static_cast<GState*>(ext_.get());
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const float s = std::max(0.05f, scale_);
    const vec3 F = yawDir(yaw_), L{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const R3 Mb{F, L, U};
    auto W = [&](vec3 l) { return pos_ + Mb * (l * s); };
    // Eingaben glaetten
    sprG(g.hunt, g.huntV, std::max(in.alert, in.reach), 2.2f, dt);
    sprG(perc_, percV_, in.perceive, 2.0f, dt);
    sprG(g.rear, g.rearV, in.crouch, 3.0f, dt);
    sprG(g.lunge, g.lungeV, in.reach, 7.0f, dt);
    sprG(g.spike, g.spikeV, std::max(in.crouch * 0.55f, in.reach), 9.0f, dt);  // straeuben sich schon beim Ausholen
    sprG(g.door, g.doorV, in.jaw, 1.6f, dt);
    sprG(g.confuse, g.confuseV, in.shiver, 3.0f, dt);
    sprG(g.slump, g.slumpV, in.hang, 1.1f, dt);
    sprG(g.stat, g.statV, in.growl, 4.0f, dt);
    sprG(idleW_, idleWV_, speed_ > 0.07f ? 0.0f : 1.0f, 2.0f, dt);
    const float dead = smG(g.slump);
    if (!chainInit_) {
        for (int i = 0; i < 2; ++i) {
            Leg& l = leg_[i];
            l.plant = l.from = l.to = W({B[legJ(i, 2)].x, B[legJ(i, 2)].y, 0.0f});
            l.swing = false, l.t = 0.0f;
        }
        g.chainScale = -1.0f;
    }
    // --- Beine: kurz und schwer; torkelnd (die Schrittweite schwankt) -------------------------------------------
    run_ = speed_ > 2.2f * s;
    vec3 rest[2];
    for (int i = 0; i < 2; ++i) rest[i] = W({B[legJ(i, 2)].x, B[legJ(i, 2)].y, 0.0f});
    const float offs[2] = {0.0f, 0.5f}, wgt[2] = {1.0f, 1.0f};
    const float S = (0.5f + 0.18f * std::min(speed_ / s, 3.0f)) * s * (dead > 0.5f ? 0.2f : 1.0f);
    stepLegs(dt, 2, offs, run_ ? 0.48f : 0.6f, S, rest, 0.28f, wgt);
    const float lift[2] = {0.16f * s, 0.16f * s};
    vec3 foot[2];
    for (int i = 0; i < 2; ++i) foot[i] = footPos(i, lift);
    const bool moving = speed_ > 0.07f;
    // --- Rumpf: Rollen (Torkeln), Beugung (Jagd), Aufrichten (Ausholen), Zusammensacken (Tod) --------------------
    const float ph = kTauG * phase_;
    const float lurch = moving ? std::sin(ph) : 0.0f;
    // im Stand wiegt es sich langsam, unregelmaessig
    const float idleSway = 0.05f * std::sin(time_ * 0.7f) + 0.03f * std::sin(time_ * 1.9f + 1.3f);
    sprG(g.sway, g.swayV, lurch * (0.16f + 0.08f * g.hunt) + idleSway * idleW_, 6.0f, dt);
    const float wantPitch = 0.10f + 0.32f * g.hunt - 0.62f * g.rear + 0.12f * perc_ + 0.35f * g.lunge;
    sprG(g.pitch, g.pitchV, wantPitch, 3.0f, dt);
    // Kopfloses "Hinwenden": der ganze Oberkoerper dreht sich zum Reiz (Wahrnehmen, Angriff)
    float wantTw = 0.0f;
    const vec3 aim = in.reach > 0.05f ? in.reachAt : in.perceiveAt;
    if (std::max(perc_, g.lunge) > 0.02f) {
        const vec3 d = aim - pos_;
        wantTw = std::clamp(wrapG(std::atan2(d.y, d.x) - yaw_), -0.7f, 0.7f) * std::max(perc_, g.lunge);
    }
    // verwirrt: der Rumpf ruckt hin und her
    wantTw += g.confuse * 0.55f * std::sin(time_ * 3.1f) * std::sin(time_ * 1.3f + 0.4f);
    sprG(g.twistY, g.twistYV, wantTw, 3.5f, dt);
    // Zittern: senden (Rauschen) und verwirrt
    const float trem = (0.006f + 0.01f * g.stat + 0.012f * g.confuse) * s;
    const vec3 shake = (L * std::sin(time_ * 31.0f) + U * std::sin(time_ * 23.0f + 1.0f) + F * std::sin(time_ * 27.0f + 2.0f)) * trem;
    float pelvisZ = (B[PELVIS].z + 0.04f * std::cos(2.0f * ph) * (moving ? 1.0f : 0.0f) + 0.2f * g.rear) * s;
    pelvisZ = pelvisZ * (1.0f - 0.8f * dead);  // Runde 5: die Leiche stand noch halb - jetzt liegt sie
    const vec3 lungeOff = F * (0.55f * s * g.lunge);
    const vec3 P = pos_ + F * (B[PELVIS].x * s) + L * (g.sway * 0.12f * s) + U * pelvisZ + lungeOff * 0.5f + shake * 0.4f;
    // Drehung des Oberkoerpers: Beugung um L (vor), Rollen um F, Hinwenden um U; tot: vornueber auf den Boden
    const R3 Rt = rotAx(U, g.twistY) * rotAx(L, g.pitch + 1.42f * dead) * rotAx(F, -g.sway * 0.55f - 0.3f * dead);
    const R3 Rlow = rotAx(U, g.twistY * 0.4f) * rotAx(L, g.pitch * 0.45f + 0.95f * dead) * rotAx(F, -g.sway * 0.3f);
    const vec3 Pb = B[PELVIS];
    auto T = [&](int j) { return P + Mb * (Rt * ((B[(size_t)j] - Pb) * s)) + lungeOff * 0.5f + shake; };
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[SPINE] = P + Mb * (Rlow * ((B[SPINE] - Pb) * s)) + lungeOff * 0.25f + shake * 0.6f;
    J[CHEST] = T(CHEST);
    J[HUMP] = T(HUMP);
    J[KNOT] = T(KNOT);
    const R3 RtW = R3{Mb * (Rt * vec3{1, 0, 0}), Mb * (Rt * vec3{0, 1, 0}), Mb * (Rt * vec3{0, 0, 1})};  // Rumpf in Weltachsen
    const vec3 TF = RtW.c0, TL = RtW.c1, TU = RtW.c2;
    // Brustklappen: drehen um die senkrechte Achse des Rumpfs nach aussen
    for (int sd = 0; sd < 2; ++sd) {
        const int h = sd == 0 ? DOORH_L : DOORH_R, t = h + 1;
        J[h] = T(h);
        const vec3 off = B[t] - B[h];
        const float ang = (sd == 0 ? 1.0f : -1.0f) * 1.75f * smG(g.door);
        J[t] = J[h] + Mb * (Rt * (rotV(off, {0, 0, 1}, ang) * s));
    }
    // Drahtrippen: in Ruhe kurze Stummel; beim Ausholen straeuben sie sich, beim Angriff fahren sie ganz aus (zitternd)
    for (int q = 0; q < kSpikes; ++q) {
        const vec3 d = Mb * (Rt * nrmG(kSpk[q].dir, {1, 0, 0}));
        const float jit = g.spike > 0.05f ? 0.012f * std::sin(time_ * (40.0f + 3.0f * (float)q)) : 0.0f;
        const float ext = (0.28f * g.spike + jit - 0.04f * dead) * s;
        J[SPK0 + q * 2] = T(SPK0 + q * 2) + d * ext;
        J[SPK0 + q * 2 + 1] = T(SPK0 + q * 2 + 1) + d * ext;
    }
    // --- Arme --------------------------------------------------------------------------------------------------
    const float lu = length(B[armJ(0, 1)] - B[armJ(0, 0)]) * s, lf = length(B[armJ(0, 2)] - B[armJ(0, 1)]) * s;
    g.flailT -= dt;
    if (g.flailT <= 0.0f) {  // verwirrt: neue, zufaellige Ziele fuer die Haende
        g.flailT = 0.25f + 0.3f * rnd();
        for (int sd = 0; sd < 2; ++sd) g.flail[sd] = vec3{rnd() * 0.9f - 0.2f, (sd == 0 ? 1.0f : -1.0f) * (0.4f + 0.8f * rnd()), rnd() * 1.4f - 0.3f};
    }
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 S0 = T(armJ(sd, 0));
        // Ruhe: haengt fast senkrecht, leicht vor dem Koerper; Gang: pendelt gegen das Bein derselben Seite
        const float swing = moving ? 0.35f * std::sin(ph + (sd == 0 ? kPiG : 0.0f)) : 0.06f * std::sin(time_ * 0.8f + (float)sd * 2.0f);
        vec3 hang = S0 + (F * (0.22f + swing) + L * (sg * 0.14f) - U * 1.0f) * ((lu + lf) * 0.86f);
        // Jagd: vorgestreckt, die Haende tief vor dem Rumpf
        const vec3 hunt = S0 + nrmG(F * 0.85f + L * (sg * 0.25f) - U * 0.45f, F) * ((lu + lf) * 0.92f);
        // Ausholen: weit ausgebreitet und hoch
        const vec3 spread = S0 + nrmG(L * (sg * 1.0f) + U * 0.55f - F * 0.15f, L) * ((lu + lf) * 0.95f);
        // Angriff: umschlingen - die Haende treffen sich am Ziel
        const vec3 grab = S0 + nrmG(in.reachAt + L * (sg * 0.18f * s) - S0, F) * std::min((lu + lf) * 0.97f, length(in.reachAt - S0));
        // tot: flach ausgebreitet am Boden
        const vec3 flat = vec3{S0.x, S0.y, pos_.z + 0.1f * s} + (F * 0.5f + L * (sg * 0.9f)) * ((lu + lf) * 0.7f);
        vec3 tgt = hang + (hunt - hang) * g.hunt;
        tgt = tgt + (spread - tgt) * smG(g.rear);
        tgt = tgt + (grab - tgt) * smG(g.lunge);
        tgt = tgt + (S0 + Mb * (g.flail[sd] * s) - tgt) * g.confuse * 0.8f;
        tgt = tgt + (flat - tgt) * dead;
        vec3 E, Wr;
        ik2(S0, tgt, lu, lf, nrmG(L * (sg * 1.0f) - F * 0.6f + U * 0.2f, L), E, Wr);
        J[armJ(sd, 0)] = S0;
        J[armJ(sd, 1)] = E;
        J[armJ(sd, 2)] = Wr;
    }
    // --- Beine -------------------------------------------------------------------------------------------------
    const float lt = length(B[legJ(0, 1)] - B[legJ(0, 0)]) * s, ls = length(B[legJ(0, 2)] - B[legJ(0, 1)]) * s;
    for (int sd = 0; sd < 2; ++sd) {
        const vec3 H = P + Mb * (Rlow * ((B[legJ(sd, 0)] - Pb) * s));
        vec3 A = foot[sd] + U * (B[legJ(sd, 2)].z * s);
        if (dead > 0.01f) A = A + (vec3{H.x, H.y, pos_.z + 0.12f * s} + F * (0.3f * s) - A) * dead;  // eingeknickt
        vec3 K, Ae;
        ik2(H, A, lt, ls, nrmG(F + U * 0.1f, F), K, Ae);
        J[legJ(sd, 0)] = H;
        J[legJ(sd, 1)] = K;
        J[legJ(sd, 2)] = Ae;
        J[legJ(sd, 3)] = Ae + nrmG(vec3{F.x, F.y, 0.0f} * 0.9f - U * 0.4f, F) * (length(B[legJ(sd, 3)] - B[legJ(sd, 2)]) * s);
    }
    // --- Gewirr und Ranken (Ketten mit Traegheit) --------------------------------------------------------------
    for (int t = 0; t < kTend; ++t) J[tendJ(t, 0)] = T(tendJ(t, 0));
    autoUps();
    if (std::fabs(g.chainScale - s) > 0.02f) chainInit_ = false, g.chainScale = s;  // neue Groesse: Ketten neu
    cols_.clear();
    cols_.push_back({J[PELVIS], J[CHEST], 0.36f * s});
    cols_.push_back({J[CHEST], J[HUMP], 0.34f * s});
    // Kette ab Gelenk idx[0]: Ruheform starr am Knochen 'parent' (mit Massstab), Wind/Zug obendrauf
    auto chainOf = [&](int ch, int parent, const int* idx, int n, float stiff, float damp, float grav, const vec3* over, float overW) {
        const mat4 rel = boneNow(parent) * rigidInverse(bindFrames_[(size_t)parent]);
        const vec3 pa = J[sk_->bones[(size_t)parent].a];
        const vec3 pa0 = sk_->bind[sk_->bones[(size_t)parent].a];
        std::vector<vec3> tgt((size_t)n);
        for (int q = 0; q < n; ++q) {
            const vec3 r = (rel * vec4(B[(size_t)idx[q]], 1.0f)).xyz();
            const vec3 r0 = (rel * vec4(pa0, 1.0f)).xyz();
            tgt[(size_t)q] = pa + (r - r0) * s;
            if (over && q > 0) tgt[(size_t)q] = tgt[(size_t)q] + (over[q] - tgt[(size_t)q]) * overW;
        }
        tgt[0] = J[(size_t)idx[0]];
        Chain& c = chain_[(size_t)ch];
        if (!chainInit_ || c.p.size() != (size_t)n) initChain(c, tgt);
        simChain(c, tgt[0], tgt, stiff, damp, grav, dt);
        for (int q = 1; q < n; ++q) J[(size_t)idx[q]] = c.p[(size_t)q];
    };
    if (chain_.size() < 7) chain_.assign(7, Chain{});
    // Gewirr an den Haenden: schleift am Boden nach (schwer, kaum Steifigkeit); verwirrt schlackert es
    for (int sd = 0; sd < 2; ++sd) {
        const int idx[5] = {armJ(sd, 2), armJ(sd, 3), armJ(sd, 4), armJ(sd, 5), armJ(sd, 6)};
        chainOf(sd, armB(sd, 1), idx, 5, 1.2f + 6.0f * g.lunge, 0.86f, 7.0f * (1.0f - 0.6f * g.lunge), nullptr, 0.0f);
    }
    // Gewirr an den Fuessen
    for (int sd = 0; sd < 2; ++sd) {
        const int idx[3] = {legJ(sd, 2), legJ(sd, 4), legJ(sd, 5)};
        chainOf(2 + sd, legB(sd, 1), idx, 3, 0.8f, 0.85f, 8.0f, nullptr, 0.0f);
    }
    // Ranken: in Ruhe ueber dem Ruecken, tastend; Ausholen: steil auf; Peitsche (strike): ueber den Ruecken nach vorn
    g.twitchT -= dt;
    if (g.twitchT <= 0.0f && g.twitchK < 0.0f) g.twitchK = (float)(int)(rnd() * 2.99f), g.twitchPh = 0.0f, g.twitchT = 1.5f + 3.0f * rnd();
    if (g.twitchK >= 0.0f) {
        g.twitchPh += dt * 2.2f;
        if (g.twitchPh > 1.0f) g.twitchK = -1.0f;
    }
    const float whip = in.strike > 0.0f ? in.strike : 0.0f;
    for (int t = 0; t < kTend; ++t) {
        const int idx[5] = {tendJ(t, 0), tendJ(t, 1), tendJ(t, 2), tendJ(t, 3), tendJ(t, 4)};
        vec3 over[5];
        const vec3 b0 = J[(size_t)idx[0]];
        const float sy = t == 0 ? 1.0f : (t == 2 ? -1.0f : 0.0f);
        float w = 0.0f;
        // aufgerichtet (Ausholen) und Peitschenschlag: Zielkurve in Rumpfachsen
        // Runde 1: in Ruhe standen die Ranken wie Hoerner - jetzt haengen sie ueber den Ruecken und heben sich erst, wenn er
        // lauscht oder jagt (sichtbares Zeichen: er hat einen bemerkt)
        const float alertUp = 0.8f * std::max(g.hunt, perc_);
        if (g.rear > 0.05f || whip > 0.0f || alertUp > 0.05f) {
            // Phase: 0..0.45 hinter dem Koerper hoch, 0.45..0.7 nach vorn hinab, dann zurueck
            const float p = whip > 0.0f ? whip : 0.0f;
            const float up = whip > 0.0f ? (p < 0.45f ? smG(p / 0.45f) : 1.0f - smG((p - 0.45f) / 0.25f)) : 1.0f;
            const float fwd = whip > 0.0f ? (p < 0.45f ? 0.0f : (p < 0.7f ? smG((p - 0.45f) / 0.25f) : 1.0f - smG((p - 0.7f) / 0.3f))) : 0.0f;
            for (int q = 0; q < 5; ++q) {
                const float u = (float)q / 4.0f;
                const vec3 raised = b0 + (TU * (1.0f * u) - TF * (0.35f * u * u) + TL * (sy * 0.25f * u)) * (1.2f * s);
                const vec3 lash = b0 + (TU * (0.55f * u - 0.9f * u * u) + TF * (1.2f * u) + TL * (sy * 0.3f * u)) * (1.2f * s);
                over[q] = raised + (lash - raised) * fwd;
            }
            w = std::max({smG(g.rear), whip > 0.0f ? up : 0.0f, alertUp});
        }
        // Zucken: eine Ranke hebt sich kurz und tastet
        if (g.twitchK >= 0.0f && (int)g.twitchK == t && w < 0.1f) {
            const float a = std::sin(kPiG * g.twitchPh);
            for (int q = 0; q < 5; ++q) {
                const float u = (float)q / 4.0f;
                over[q] = b0 + (TU * (0.7f * u) - TF * (0.3f * u) + TL * (sy * 0.4f * u + 0.2f * std::sin(time_ * 5.0f) * u)) * (1.1f * s);
            }
            w = 0.6f * a;
        }
        const float stiff = (w > 0.05f ? 14.0f : 2.2f) * (1.0f - dead);
        chainOf(4 + t, B_HUMP, idx, 5, stiff, whip > 0.0f ? 0.93f : 0.87f, (7.0f + 3.0f * dead) * (1.0f - 0.8f * w), w > 0.0f ? over : nullptr, w);
    }
    // Zehen nach dem Gewirr (bleiben am Fuss)
    autoUps();
    // Rumpfknochen: Oben-Richtung aus der Drehung des Rumpfs (auch Verdrehen um die eigene Achse), dann zur Richtung
    auto arcUp = [&](int b, const R3& R) {
        const CBone& cb = sk_->bones[(size_t)b];
        const vec3 d0 = nrmG(Mb * (R * (B[cb.b] - B[cb.a])), F), d1 = nrmG(J[cb.b] - J[cb.a], d0);
        vec3 up = Mb * (R * sk_->bindUp[(size_t)b]);
        const vec3 ax = cross(d0, d1);
        const float sn = length(ax), cs = dot(d0, d1);
        if (sn > 1e-6f) up = rotV(up, ax / sn, std::atan2(sn, cs));
        pose_.up[(size_t)b] = up;
    };
    arcUp(gw::B_PELVIS, Rlow);
    arcUp(gw::B_SPINE, Rt);
    arcUp(B_HUMP, Rt);
    for (int q = 0; q < kSpikes; ++q) arcUp(B_SPK0 + q, Rt);
    for (int sd = 0; sd < 2; ++sd) arcUp(sd == 0 ? B_DOOR_L : B_DOOR_R, Rt * rotAx({0, 0, 1}, (sd == 0 ? 1.0f : -1.0f) * 1.75f * smG(g.door)));
    // Knoten: dreht und zittert beim Lauschen (Klicks) und beim Senden; verwirrt dreht er durch
    g.knotSpin += dt * (0.3f + 6.0f * perc_ * (0.5f + 0.5f * std::sin(time_ * 9.0f)) + 4.0f * g.stat + 14.0f * g.confuse);
    pose_.up[B_KNOT] = rotV(Mb * (Rt * sk_->bindUp[B_KNOT]), nrmG(J[KNOT] - J[CHEST], TF), g.knotSpin);
    // Massstab je Knochen: alles gleich
    pose_.s.assign(sk_->bones.size(), s);
    headPos_ = J[KNOT];
    headDir_ = TF;
    (void)hashG;
}

}  // namespace lim::anim
