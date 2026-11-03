// V13.3 Teil 4 Level 188: der Horcher - Skelett und Bewegung (KREATUR_Horcher.md; Wikidot level-188 Rev. 69).
//
// Die Seite nennt in den Gaengen um den Hof neben Facelings im rosa Kleid und Skin-Stealern "andere boesartige
// menschenaehnliche Lebensformen", ohne sie zu beschreiben. Der Horcher ist die begruendete Auslegung (Briefing): er wohnt
// hinter den von innen verschlossenen Tueren, klopft (Log 2: lautes, rhythmisches Haemmern) und tritt hinter einem aus einer
// Tuer (Log 4). Gestalt: kreideweiss wie die Waende, gebueckt; der Hals viel zu lang, der Kopf ein riesiges Ohr, dessen Muschel
// nach vorn lauscht, der Gehoergang ist der Mund; der rechte Arm zu einer Klopf-Faust ausgewachsen, auf die er sich stuetzt.
// Bewegung (Kanaele wie bei allen Wesen, damit die Puppe beim Gast dasselbe tut):
//   Gehen     auf drei Stuetzen: die Faust setzt vor, dann der linke, dann der rechte Fuss; geduckt (crouch) tiefer und weiter vorn
//   Lauschen  ohne Ziel schwenkt das Ohr langsam wie eine Schuessel; perceive/perceiveAt: das Ohr richtet sich auf das Geraeusch,
//             die duenne linke Hand legt sich hinter den Rand der Muschel und lenkt sie; shiver: der Rand zittert
//   Laut      growl: der Hals pumpt, der Gehoergang weitet sich (jaw, growl)
//   Klopfen   wave: die Faust schlaegt kurz auf, was vor ihr ist (Boden, Wand)
//   Ankuendigung reach: er richtet sich auf (bis 2,3 m), die Faust hoch ueber dem Kopf; wave dazu: drei Schlaege in die Luft
//   Schlag    strike 0..1: die Faust faehrt auf reachAt herab
//   Licht     light: das Ohr knittert (der Rand faltet sich ein), der Kopf dreht sich weg, die Hand deckt den Gehoergang
// Koordinaten wie im Spiel: x vorn, y links, z oben; Wurzel am Boden.
#include "anim/creature_anim.hpp"

#include <algorithm>
#include <cmath>

#include "anim/skeleton.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kTau = 6.2831853f, kPiL = 3.14159265f;
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
vec3 rot(vec3 v, vec3 k, float a) {  // Rodrigues: um die Einheitsachse k
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1.0f - c));
}
vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
vec3 lerp3(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
struct Fr {  // Rahmen: Ursprung, Achsen (vorn, links, oben)
    vec3 o, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    vec3 at(vec3 p) const { return o + x * p.x + y * p.y + z * p.z; }
    vec3 dir(vec3 d) const { return x * d.x + y * d.y + z * d.z; }
    void turn(vec3 axis, float a) { x = rot(x, axis, a), y = rot(y, axis, a), z = rot(z, axis, a); }
};
void yawPitchL(vec3 a, vec3 b, vec3 F, vec3 L, float& yw, float& pt) {
    const vec3 d = b - a;
    const float fx = dot(d, F), fy = dot(d, L), fz = d.z;
    yw = std::atan2(fy, fx);
    pt = std::atan2(fz, std::max(0.02f, std::hypot(fx, fy)));
}

enum X : int {
    X_ALERT, X_ALERTV, X_CROUCH, X_CROUCHV, X_PERC, X_PERCV, X_LIGHT, X_LIGHTV, X_GROWL, X_GROWLV, X_JAW, X_JAWV,
    X_REACH, X_REACHV, X_STRIKE, X_STRIKEV, X_WAVE, X_WAVEV, X_SHIV, X_SHIVV, X_TILT, X_TILTV, X_HY, X_HYV, X_HP, X_HPV,
    X_FREEZE, X_FREEZET, X_SCANY, X_SCANP, X_HAND, X_HANDV, X_FISTZ, X_FISTZV, X_SIZE
};
// Bindehaltung der Faust (Knoechel am Boden) - gleich der Ruhepose im Skelett
constexpr vec3 kFist{0.92f, -0.36f, 0.04f};
}  // namespace

CreatureSkeleton hr::makeSkeleton() {
    CreatureSkeleton s;
    s.kind = Entity::Listener;
    s.bind.assign((size_t)JCOUNT, vec3{});
    s.bones.assign((size_t)BCOUNT, CBone{});
    s.bindUp.assign((size_t)BCOUNT, vec3{1, 0, 0});
    auto J = [&](int i, vec3 p) { s.bind[(size_t)i] = p; };
    auto B = [&](int i, int a, int b, int parent, float r, vec3 up, bool shadow = true) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)b, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        s.bindUp[(size_t)i] = up;
    };
    const vec3 F{1, 0, 0}, Z{0, 0, 1};
    J(ROOT, {0, 0, 0});
    J(PELVIS, {0.0f, 0, 0.96f});
    J(SPINE, {0.17f, 0, 1.24f});
    J(CHEST, {0.38f, 0, 1.46f});
    J(NECK0, {0.50f, 0, 1.56f});   // Halsansatz vorn ueber der Brust
    J(NECK1, {0.74f, 0, 1.50f});
    J(NECK2, {0.92f, 0, 1.32f});   // Ende des Halses: steckt von hinten oben in der Muschel
    J(EAR, {0.99f, 0, 1.11f});     // Mitte des Ohrs (0,62 m hoch, 0,4 m breit)
    J(EARTIP, {0.97f, 0.06f, 1.42f});
    J(LOBE, {1.00f, -0.03f, 0.82f});  // Ohrlaeppchen: darin ein verkuemmertes Gesicht
    J(CANAL, {1.06f, 0.0f, 1.08f});   // Gehoergang (Mund)
    // linker Arm: duenn, lange Finger; rechter Arm: riesig, Faust am Boden
    J(armJ(0, 0), {0.40f, 0.19f, 1.50f});
    J(armJ(0, 1), {0.50f, 0.30f, 1.22f});
    J(armJ(0, 2), {0.66f, 0.22f, 1.04f});
    J(armJ(0, 3), {0.86f, 0.16f, 0.96f});
    J(armJ(1, 0), {0.36f, -0.25f, 1.53f});
    J(armJ(1, 1), {0.58f, -0.42f, 1.00f});
    J(armJ(1, 2), {0.80f, -0.38f, 0.30f});
    J(armJ(1, 3), kFist);
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd == 0 ? 1.0f : -1.0f;
        J(legJ(sd, 0), {-0.02f, 0.13f * g, 0.94f});
        J(legJ(sd, 1), {0.16f, 0.16f * g, 0.52f});
        J(legJ(sd, 2), {-0.03f, 0.15f * g, 0.09f});
        J(legJ(sd, 3), {0.16f, 0.16f * g, 0.015f});
    }
    B(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    B(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.15f, F);
    B(B_SPINE, SPINE, CHEST, B_PELVIS, 0.15f, F);
    B(B_CHEST, CHEST, NECK0, B_SPINE, 0.17f, F);
    B(B_NECK0, NECK0, NECK1, B_CHEST, 0.07f, Z);
    B(B_NECK1, NECK1, NECK2, B_NECK0, 0.065f, Z);
    B(B_HEAD, NECK2, EAR, B_NECK1, 0.16f, F);
    B(B_HELIX, EAR, EARTIP, B_HEAD, 0.14f, F);
    B(B_LOBE, EAR, LOBE, B_HEAD, 0.12f, F);
    B(B_CANAL, EAR, CANAL, B_HEAD, 0.05f, Z, false);
    for (int sd = 0; sd < 2; ++sd) {
        const float big = sd == 1 ? 1.0f : 0.0f;
        B(armB(sd, 0), CHEST, armJ(sd, 0), B_CHEST, 0.06f + 0.05f * big, Z, false);
        B(armB(sd, 1), armJ(sd, 0), armJ(sd, 1), armB(sd, 0), 0.04f + 0.06f * big, sd ? vec3{0, -1, 0} : vec3{0, 1, 0});
        B(armB(sd, 2), armJ(sd, 1), armJ(sd, 2), armB(sd, 1), 0.035f + 0.075f * big, F);
        B(armB(sd, 3), armJ(sd, 2), armJ(sd, 3), armB(sd, 2), 0.03f + 0.1f * big, F);
        B(legB(sd, 0), legJ(sd, 0), legJ(sd, 1), B_PELVIS, 0.07f, F);
        B(legB(sd, 1), legJ(sd, 1), legJ(sd, 2), legB(sd, 0), 0.05f, F);
        B(legB(sd, 2), legJ(sd, 2), legJ(sd, 3), legB(sd, 1), 0.04f, Z);
    }
    s.height = 1.6f;
    s.reach = 1.25f;
    s.center = {0.45f, 0, 0.95f};
    return s;
}

void CreatureAnimator::updateListener(float dt, const Input& in) {
    using namespace hr;
    auto& P = pose_.j;
    const auto& Bd = sk_->bind;
    if ((int)xs_.size() != X_SIZE) {
        xs_.assign((size_t)X_SIZE, 0.0f);
        xs_[X_ALERT] = in.alert, xs_[X_CROUCH] = in.crouch, xs_[X_FREEZET] = 2.0f;
    }
    float* X = xs_.data();
    if ((int)pose_.s.size() != BCOUNT) pose_.s.assign((size_t)BCOUNT, 1.0f);
    const float t = time_;
    // --- Federn der Eingaben ------------------------------------------------------------------------------------------------
    spring(X[X_ALERT], X[X_ALERTV], in.alert, 4.0f, dt);
    spring(X[X_CROUCH], X[X_CROUCHV], in.crouch, 3.5f, dt);
    spring(X[X_PERC], X[X_PERCV], in.perceive, in.perceive > X[X_PERC] ? 5.0f : 2.2f, dt);
    spring(X[X_LIGHT], X[X_LIGHTV], in.light, in.light > X[X_LIGHT] ? 14.0f : 2.4f, dt);
    spring(X[X_GROWL], X[X_GROWLV], in.growl, in.growl > X[X_GROWL] ? 8.0f : 2.0f, dt);
    spring(X[X_JAW], X[X_JAWV], std::max(in.jaw, 0.6f * in.growl), 9.0f, dt);
    spring(X[X_REACH], X[X_REACHV], in.reach, in.reach > X[X_REACH] ? 4.2f : 3.0f, dt);
    spring(X[X_STRIKE], X[X_STRIKEV], in.strike, 26.0f, dt);
    spring(X[X_WAVE], X[X_WAVEV], in.wave, 6.0f, dt);
    spring(X[X_SHIV], X[X_SHIVV], in.shiver, 5.0f, dt);
    spring(X[X_TILT], X[X_TILTV], in.tilt, 3.0f, dt);
    const float alert = std::clamp(X[X_ALERT], 0.0f, 1.0f), crouch = std::clamp(X[X_CROUCH], 0.0f, 1.2f);
    const float perc = std::clamp(X[X_PERC], 0.0f, 1.0f), lightK = std::clamp(X[X_LIGHT], 0.0f, 1.0f);
    const float growl = std::clamp(X[X_GROWL], 0.0f, 1.0f), jaw = std::clamp(X[X_JAW], 0.0f, 1.0f);
    const float reach = std::clamp(X[X_REACH], 0.0f, 1.0f), strike = std::clamp(in.strike, 0.0f, 1.0f);
    const float wave = std::clamp(X[X_WAVE], 0.0f, 1.0f), shiv = std::clamp(X[X_SHIV], 0.0f, 1.0f);

    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    Fr root;
    root.o = pos_, root.x = F, root.y = Lw, root.z = U;

    // --- Gang auf drei Stuetzen (Faust, linker Fuss, rechter Fuss) ---------------------------------------------------------
    run_ = speed_ > 2.4f;
    const float mv = sm01(speed_ / 0.8f), gal = sm01((speed_ - 1.8f) / 2.0f);
    static const float offs[3] = {0.33f, 0.66f, 0.0f};  // Faust (Index 2) setzt zuerst
    const float duty = 0.62f - 0.14f * gal;
    const float S = 0.9f + 0.36f * speed_;
    const float reachK = sm01(reach / 0.35f);  // die Faust ist oben: sie stuetzt nicht
    vec3 rest[3];
    for (int sd = 0; sd < 2; ++sd) rest[sd] = root.at({Bd[legJ(sd, 2)].x - 0.08f * crouch, Bd[legJ(sd, 2)].y, 0.0f});
    rest[2] = root.at({kFist.x + 0.12f * crouch + 0.18f * gal, kFist.y, 0.0f});
    const float wgt[3] = {0.45f, 0.45f, 0.9f};
    stepLegs(dt, 3, offs, duty, S, rest, 0.4f, wgt);
    const float lift[3] = {0.07f + 0.05f * gal, 0.07f + 0.05f * gal, 0.16f + 0.14f * gal};
    vec3 g[3];
    for (int i = 0; i < 3; ++i) g[i] = footPos(i, lift);
    const float c1 = std::cos(kTau * phase_);
    const float breath = std::sin(t * kTau * 0.21f);
    // Aufbaeumen (Ankuendigung): der Rumpf richtet sich auf; der Schlag wirft ihn wieder nach vorn
    const float su = strike;
    const float slam = su > 0.0f ? sm01((su - 0.18f) / 0.22f) * (1.0f - sm01((su - 0.7f) / 0.3f)) : 0.0f;
    const float rear = reachK * (1.0f - slam);
    // Becken: hebt und senkt sich mit jedem Faustschritt (die Last liegt vorn), geduckt tiefer
    const float bob = -(0.016f * mv + 0.035f * gal) * std::cos(3.0f * kTau * (phase_ - 0.08f));  // drei Stuetzen je Takt
    vec3 Pp = pos_ + U * (Bd[PELVIS].z - 0.24f * crouch + bob + 0.07f * rear - 0.12f * slam) - F * (0.04f * rear) + Lw * (0.02f * mv * std::cos(kTau * (phase_ - 0.2f)));
    Fr pf = root;
    pf.turn(U, -0.08f * c1 * mv);
    Fr tf = pf;  // Rumpf: nach vorn geneigt; geduckt mehr, aufgebaeumt fast senkrecht, im Schlag weit nach vorn
    const float pitch = 0.22f * crouch + 0.08f * alert - 0.85f * rear + 0.55f * slam + 0.06f * gal + 0.02f * breath;
    tf.turn(tf.y, pitch);
    tf.turn(tf.x, 0.05f * std::sin(t * 0.37f) + 0.07f * mv * std::sin(kTau * phase_));  // schief, rollt im Gang
    P[ROOT] = pos_;
    P[PELVIS] = Pp;
    P[SPINE] = Pp + tf.dir(Bd[SPINE] - Bd[PELVIS]);
    // Rueckgrat: der Brustkorb haengt nach vorn (die Faust traegt ihn), Atem hebt ihn kaum
    P[CHEST] = P[SPINE] + rot(tf.dir(Bd[CHEST] - Bd[SPINE]), tf.y, 0.1f * crouch) + U * (0.006f * breath);
    P[NECK0] = P[CHEST] + rot(tf.dir(Bd[NECK0] - Bd[CHEST]), tf.y, 0.1f * crouch);

    // Beine
    const float ankH = Bd[legJ(0, 2)].z;
    for (int sd = 0; sd < 2; ++sd) {
        const Leg& Lg = leg_[sd];
        const vec3 fd0 = root.dir(Bd[legJ(sd, 3)] - Bd[legJ(sd, 2)]);
        float fp = 0.0f;
        if (Lg.swing) fp = 0.45f * std::sin(kPiL * Lg.t) * (1.0f - Lg.t);
        const vec3 fv = rot(fd0, Lw, fp);
        vec3 ank = g[sd] + U * ankH;
        const vec3 H = Pp + pf.dir(Bd[legJ(sd, 0)] - Bd[PELVIS]);
        const float l1 = length(Bd[legJ(sd, 1)] - Bd[legJ(sd, 0)]), l2 = length(Bd[legJ(sd, 2)] - Bd[legJ(sd, 1)]);
        vec3 knee, a2;
        ik2(H, ank, l1, l2, F + Lw * (sd == 0 ? 0.25f : -0.25f), knee, a2);
        P[legJ(sd, 0)] = H;
        P[legJ(sd, 1)] = knee;
        P[legJ(sd, 2)] = a2;
        P[legJ(sd, 3)] = a2 + fv;
    }

    // --- Hals und Ohr: lauschen, ausrichten, zittern, wegdrehen ------------------------------------------------------------
    // ohne Ziel: das Ohr schwenkt langsam wie eine Schuessel, haelt ab und zu still (lauscht in eine Richtung)
    X[X_FREEZET] -= dt;
    if (X[X_FREEZET] <= 0.0f) {
        X[X_FREEZET] = 1.4f + 2.6f * rnd();
        X[X_FREEZE] = rnd() < 0.45f ? 1.0f : 0.0f;
        X[X_SCANY] = (rnd() * 2.0f - 1.0f) * 0.9f, X[X_SCANP] = -0.25f + 0.45f * rnd();
    }
    float wy = X[X_FREEZE] > 0.5f ? X[X_SCANY] : 0.55f * std::sin(t * 0.43f) + 0.3f * std::sin(t * 0.97f + 1.3f);
    float wp = X[X_FREEZE] > 0.5f ? X[X_SCANP] : -0.05f + 0.15f * std::sin(t * 0.31f + 0.4f);
    wy *= 1.0f - 0.4f * mv, wp *= 1.0f - 0.4f * mv;
    if (in.look) {
        float ly, lp;
        yawPitchL(P[NECK0], in.lookAt, F, Lw, ly, lp);
        wy += (ly - wy) * 0.85f, wp += (lp - wp) * 0.85f;
    }
    if (perc > 0.01f) {
        float y2, p2;
        yawPitchL(P[NECK0], in.perceiveAt, F, Lw, y2, p2);
        wy += (std::clamp(y2, -2.2f, 2.2f) - wy) * perc, wp += (p2 - wp) * perc;
    }
    if (lightK > 0.01f) {  // vom Licht weg: Kopf zur Seite und nach unten
        float ly, lp;
        yawPitchL(P[NECK0], in.lightFrom, F, Lw, ly, lp);
        const float away = ly >= 0.0f ? ly - 1.6f : ly + 1.6f;
        wy += (std::clamp(away, -1.6f, 1.6f) - wy) * lightK, wp += (-0.6f - wp) * lightK;
    }
    if (rear > 0.01f) wp += (0.15f - wp) * rear * 0.6f;
    wy = std::clamp(wrapA(wy), -1.9f, 1.9f), wp = std::clamp(wp, -1.1f, 0.8f);
    if (!chainInit_) X[X_HY] = wy, X[X_HP] = wp, X[X_HYV] = X[X_HPV] = 0.0f;
    const float hOm = lightK > 0.3f ? 9.0f : (perc > 0.4f ? 5.5f : 2.4f);
    spring(X[X_HY], X[X_HYV], wy, hOm, dt);
    spring(X[X_HP], X[X_HPV], wp, hOm, dt);
    const float hy = X[X_HY], hp = X[X_HP];
    const float tl = std::clamp(X[X_TILT], -0.7f, 0.7f) + 0.12f * std::sin(t * 0.53f) * (1.0f - perc);
    // Hals: drei Glieder teilen sich die Drehung; im Aufbaeumen steigt er nach oben, der Kehlsack pumpt beim Laut
    const float pump = growl * 0.02f * std::sin(t * 23.0f);
    Fr nf = tf;
    nf.turn(U, hy * 0.3f);
    nf.turn(nf.y, -hp * 0.25f - 0.55f * rear);
    P[NECK1] = P[NECK0] + nf.dir(Bd[NECK1] - Bd[NECK0]) * (1.0f + pump);
    Fr nf2 = nf;
    nf2.turn(U, hy * 0.35f);
    nf2.turn(nf2.y, -hp * 0.35f - 0.35f * rear);
    P[NECK2] = P[NECK1] + nf2.dir(Bd[NECK2] - Bd[NECK1]);
    // Ohr: Rahmen aus Gier und Neigung um den Hals; Zittern des Randes (shiver) und im Licht ein harter Schauder
    Fr hf = root;
    hf.turn(U, hy);
    hf.turn(hf.y, -hp - 0.3f * rear + 0.35f * slam);
    hf.turn(hf.x, tl);
    const float trem = (0.025f * shiv + 0.05f * lightK) * std::sin(t * 41.0f) + 0.012f * growl * std::sin(t * 17.0f);
    hf.turn(hf.z, trem);
    auto headAt = [&](int j) { return P[NECK2] + hf.dir(Bd[(size_t)j] - Bd[NECK2]); };
    P[EAR] = headAt(EAR);
    P[EARTIP] = headAt(EARTIP);
    P[LOBE] = headAt(LOBE);
    P[CANAL] = headAt(CANAL);
    headPos_ = P[CANAL];
    headDir_ = hf.x;
    // Massstab: der Rand knittert im Licht (zieht sich zusammen), der Gehoergang weitet sich beim Laut und im Schlag
    pose_.s[B_HELIX] = 1.0f - 0.28f * lightK + 0.04f * shiv * std::sin(t * 37.0f);
    pose_.s[B_LOBE] = 1.0f - 0.12f * lightK;
    pose_.s[B_CANAL] = 1.0f + 0.55f * jaw + 0.35f * slam;

    // --- Arme -------------------------------------------------------------------------------------------------------------
    // rechts: die Klopf-Faust. Am Boden (Stuetze), beim Klopfen kurze Schlaege, in der Ankuendigung hoch ueber dem Kopf, im
    // Schlag herab auf reachAt
    {
        const int sd = 1;
        const vec3 S = P[CHEST] + tf.dir(Bd[armJ(sd, 0)] - Bd[CHEST]) + U * (0.05f * rear);
        const float l1 = length(Bd[armJ(sd, 1)] - Bd[armJ(sd, 0)]), l2 = length(Bd[armJ(sd, 2)] - Bd[armJ(sd, 1)]);
        const float lf = length(Bd[armJ(sd, 3)] - Bd[armJ(sd, 2)]);
        // Stuetze: Knoechel am Boden, das Handgelenk darueber (leicht hinten)
        vec3 knuck = g[2];
        // Klopfen: die Faust hebt kurz und schlaegt auf (vier Schlaege je Sekunde)
        const float rap = wave * std::max(0.0f, std::sin(t * kTau * 2.0f));
        knuck += U * (0.22f * rap) - F * (0.05f * rap);
        vec3 wristT = knuck + U * (lf * 0.92f) - F * (lf * 0.35f);
        vec3 fistDir = nrm(knuck - wristT, -U);
        // Ankuendigung: hoch ueber dem Kopf, leicht nach hinten, die Faust zittert; wave: drei Schlaege in die Luft
        if (reachK > 0.001f) {
            const vec3 up = P[NECK0] + U * 0.95f - F * 0.05f - Lw * 0.25f + U * (0.12f * rap) + F * (0.02f * std::sin(t * 31.0f));
            const vec3 upDir = nrm(F * 0.5f + U * 0.6f - Lw * 0.1f, U);
            wristT = lerp3(wristT, up, reachK);
            fistDir = nrm(lerp3(fistDir, upDir, reachK), U);
        }
        // Schlag: herab auf das Ziel (die Faust trifft dort, das Handgelenk dahinter)
        if (slam > 0.001f) {
            const vec3 aim = in.reachAt;
            const vec3 dirA = nrm(aim - S, F);
            const float maxL = (l1 + l2) * 0.97f;
            vec3 w = aim - dirA * lf * 0.8f + U * 0.05f;
            if (length(w - S) > maxL) w = S + nrm(w - S, F) * maxL;
            wristT = lerp3(wristT, w, slam);
            fistDir = nrm(lerp3(fistDir, nrm(aim - w, dirA), slam), dirA);
        }
        // Aushol-Phase des Schlags (0..0,18): noch weiter zurueck
        if (su > 0.0f && su < 0.2f) wristT += -F * (0.25f * sm01(su / 0.18f)) + U * (0.1f * sm01(su / 0.18f));
        vec3 el, wr;
        ik2(S, wristT, l1, l2, -Lw * 0.8f - F * 0.4f + U * (0.3f * reachK), el, wr);
        P[armJ(sd, 0)] = S;
        P[armJ(sd, 1)] = el;
        P[armJ(sd, 2)] = wr;
        P[armJ(sd, 3)] = wr + fistDir * lf;
        if (P[armJ(sd, 3)].z < pos_.z + 0.02f) P[armJ(sd, 3)].z = pos_.z + 0.02f;
    }
    // links: duenn; tastet in Ruhe vor der Brust; lauscht es angestrengt, legt sich die Hand hinter den Rand der Muschel; im
    // Licht deckt sie den Gehoergang
    {
        const int sd = 0;
        const vec3 S = P[CHEST] + tf.dir(Bd[armJ(sd, 0)] - Bd[CHEST]);
        const float l1 = length(Bd[armJ(sd, 1)] - Bd[armJ(sd, 0)]), l2 = length(Bd[armJ(sd, 2)] - Bd[armJ(sd, 1)]);
        const float lh = length(Bd[armJ(sd, 3)] - Bd[armJ(sd, 2)]);
        const float feel = std::sin(t * 1.7f) * 0.03f + std::sin(t * 4.1f + 0.7f) * 0.012f;
        vec3 wristT = Pp + tf.dir(Bd[armJ(sd, 2)] - Bd[PELVIS]) + Lw * feel + U * (0.02f * std::sin(t * 2.3f));
        vec3 hd = nrm(tf.dir(Bd[armJ(sd, 3)] - Bd[armJ(sd, 2)]), F);
        const float cup = std::max(perc * 0.9f, shiv * 0.6f) * (1.0f - lightK);
        spring(X[X_HAND], X[X_HANDV], cup, 4.0f, dt);
        const float cupK = std::clamp(X[X_HAND], 0.0f, 1.0f);
        if (cupK > 0.001f) {  // hinter den Rand der Muschel (linke Seite), die Finger um die Kante
            const vec3 rim = P[EAR] + hf.dir(vec3{-0.08f, 0.22f, 0.04f}) ;
            wristT = lerp3(wristT, rim - hf.dir(vec3{0.02f, 0.12f, 0.0f}), cupK);
            hd = nrm(lerp3(hd, hf.dir(nrm(vec3{0.55f, -0.15f, 0.2f}, F)), cupK), hf.x);
        }
        if (lightK > 0.001f) {  // ueber den Gehoergang
            wristT = lerp3(wristT, P[CANAL] + hf.dir(vec3{0.14f, 0.12f, -0.05f}), lightK);
            hd = nrm(lerp3(hd, hf.dir(vec3{-0.1f, -0.9f, 0.1f}), lightK), -hf.y);
        }
        if (rear > 0.01f) wristT = lerp3(wristT, P[CHEST] + tf.dir({0.25f, 0.2f, 0.05f}), rear * 0.7f);
        vec3 el, wr;
        ik2(S, wristT, l1, l2, Lw * 0.7f - F * 0.5f - U * 0.2f, el, wr);
        P[armJ(sd, 0)] = S;
        P[armJ(sd, 1)] = el;
        P[armJ(sd, 2)] = wr;
        P[armJ(sd, 3)] = wr + hd * lh;
    }

    autoUps();
    pose_.up[hr::B_PELVIS] = tf.x, pose_.up[hr::B_SPINE] = tf.x, pose_.up[hr::B_CHEST] = tf.x;  // hr:: - anim::B_* (Mensch) gleichnamig
    pose_.up[hr::B_NECK0] = nf.z, pose_.up[hr::B_NECK1] = nf2.z;
    pose_.up[hr::B_HEAD] = hf.x, pose_.up[hr::B_HELIX] = hf.x, pose_.up[hr::B_LOBE] = hf.x, pose_.up[hr::B_CANAL] = hf.z;
}

}  // namespace lim::anim
