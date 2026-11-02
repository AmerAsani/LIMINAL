// V13.3 Teil 4 Level 5.3: der Hausherr (Wikidot entity-18 Rev. 54, level-5-3 Rev. 40) - Skelett und Bewegung
// (KREATUR_Gentleman.md).
//
// Die Seite der Bibliothek erzaehlt am Ende, wie der Herr des Hotels durch eine kleine Holztuer in die Bibliothek tritt, die
// Gaenge mustert, ein Notizbuch aus dem naechsten Regal vor die Fuesse bekommt, es aufhebt und durchsieht, es ohne
// hinzusehen hinter sich wirft, zur Bibliothek spricht und mit unbewegtem Gesicht geht. Die Entity-18-Seite beschreibt kein
// Aussehen - die Gestalt ist Auslegung (Briefing): ein Herr im Frack, zu gross, die Arme mit zwei Ellbogen, Haende mit zu
// langen Fingern (wie der Arm im Buero von Level 5), das Gesicht glatt wie gemalt.
// Bewegung (Kanaele wie bei allen Wesen, damit die Puppe beim Gast dasselbe tut):
//   Gehen     gemessen, aufrecht, lange Schritte, kaum Hub; die Haende auf dem Ruecken verschraenkt (alert 0) oder die
//             zu langen Arme haengen bis unter die Knie und pendeln (alert 1)
//   Umsehen   ohne Blickziel mustert er langsam die Gaenge und Galerien; shiver: ein Seufzer (Schultern heben und senken sich)
//   Wahrnehmen perceive/perceiveAt: der Kopf dreht sich langsam bis fast nach hinten (166 Grad), der Koerper bleibt;
//             light: er sieht in die Lampe, ohne zu blinzeln
//   Notizbuch fly = Phase / 6: faellt aus dem Regal rechts vor die Fuesse, er verbeugt sich steif aus der Huefte (der Kopf
//             bleibt waagrecht), der rechte Arm entfaltet sich zum Boden, hebt es auf, liest (die linke Hand blaettert),
//             wirft es ueber die Schulter hinter sich, ohne hinzusehen - es fliegt im Bogen und landet
//   Sprechen  talk: der Kiefer bewegt sich kaum (das gemalte Gesicht bleibt), der Kopf nickt leicht; plea: er spricht zur
//             Bibliothek - beide Arme halb geoeffnet, die Handflaechen nach oben
//   Riss      growl 0..1: im gemalten Gesicht oeffnet sich ein Riss, darunter rohes, nasses Fleisch (Massstab des Knochens)
//   Griff     reach/reachAt: der rechte Arm entfaltet sich Glied fuer Glied zum Gesicht des Ziels; strike: die Finger
//             wachsen und schliessen sich (Massstab der Hand)
//   Revers    wave: beide Haende ziehen die Revers glatt (danach geht er)
// Koordinaten wie im Spiel: x vorn, y links, z oben; Wurzel am Boden.
#include "anim/creature_anim.hpp"

#include <algorithm>
#include <cmath>

#include "anim/skeleton.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kTau = 6.2831853f, kPiG = 3.14159265f;
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
float hashTo01(unsigned i) {
    i ^= i >> 16, i *= 0x7feb352du, i ^= i >> 15, i *= 0x846ca68bu, i ^= i >> 16;
    return (float)(i & 0xFFFFFF) / 16777216.0f;
}
struct Fr {  // Rahmen: Ursprung, Achsen (vorn, links, oben)
    vec3 o, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    vec3 at(vec3 p) const { return o + x * p.x + y * p.y + z * p.z; }
    vec3 dir(vec3 d) const { return x * d.x + y * d.y + z * d.z; }
    void turn(vec3 axis, float a) { x = rot(x, axis, a), y = rot(y, axis, a), z = rot(z, axis, a); }
};
void yawPitchG(vec3 a, vec3 b, vec3 F, vec3 L, float& yw, float& pt) {
    const vec3 d = b - a;
    const float fx = dot(d, F), fy = dot(d, L), fz = d.z;
    yw = std::atan2(fy, fx);
    pt = std::atan2(fz, std::max(0.02f, std::hypot(fx, fy)));
}
// FABRIK fuer drei Glieder (Schulter, Ellbogen 1, Ellbogen 2, Handgelenk): p[1..3] kommen als Startform herein (die
// Haltung, aus der sich der Arm entfaltet) und behalten so ihre Beugerichtung
void fabrik3(vec3 base, vec3 T, const float* L, vec3* p) {
    const float total = L[0] + L[1] + L[2];
    const vec3 d = T - base;
    if (length(d) >= total * 0.999f) {
        const vec3 u = nrm(d, {0, 0, -1});
        p[0] = base, p[1] = base + u * L[0], p[2] = p[1] + u * L[1], p[3] = p[2] + u * L[2];
        return;
    }
    p[0] = base;
    for (int it = 0; it < 12; ++it) {
        p[3] = T;
        p[2] = p[3] + nrm(p[2] - p[3], {0, 0, 1}) * L[2];
        p[1] = p[2] + nrm(p[1] - p[2], {0, 0, 1}) * L[1];
        p[1] = base + nrm(p[1] - base, {0, 0, -1}) * L[0];
        p[2] = p[1] + nrm(p[2] - p[1], {0, 0, -1}) * L[1];
        p[3] = p[2] + nrm(p[3] - p[2], {0, 0, -1}) * L[2];
    }
}

enum X : int {
    X_ALERT, X_ALERTV, X_TALK, X_TALKV, X_PLEA, X_PLEAV, X_PERC, X_PERCV, X_LIGHT, X_LIGHTV, X_GROWL, X_GROWLV,
    X_REACH, X_REACHV, X_STRIKE, X_STRIKEV, X_WAVE, X_WAVEV, X_SIGH, X_SIGHV, X_TILT, X_TILTV, X_HY, X_HYV, X_HP, X_HPV,
    X_SYL, X_SYLV, X_SYLW, X_SYLT, X_TS, X_TSV, X_TB, X_TBV, X_BOW, X_BOWV, X_SIZE
};
// Wurf: Handgelenk im Rumpfrahmen je Fortschritt u (0 .. 0,18 ausholen, .. 0,32 ueber die Schulter, Loslassen bei 0,3)
vec3 throwWrist(float u) {
    const vec3 read{0.36f, -0.06f, 1.62f}, up{0.26f, -0.34f, 1.78f}, back{-0.36f, -0.34f, 1.93f}, down{-0.22f, -0.30f, 1.20f};
    if (u < 0.18f) return lerp3(read, up, sm01(u / 0.18f));
    if (u < 0.32f) return lerp3(up, back, sm01((u - 0.18f) / 0.14f));
    return lerp3(back, down, sm01((u - 0.32f) / 0.4f));
}
}  // namespace

CreatureSkeleton gm::makeSkeleton() {
    CreatureSkeleton s;
    s.kind = Entity::Gentleman;
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
    J(PELVIS, {0, 0, 1.06f});
    J(SPINE, {0.0f, 0, 1.27f});
    J(CHEST, {0.01f, 0, 1.48f});
    J(NECK, {0.0f, 0, 1.70f});   // zwischen den Schultern
    J(HEAD, {0.012f, 0, 1.86f});  // Schaedelansatz: der Hals ist eine Handbreit zu lang
    J(HEADTOP, {0.02f, 0, 2.12f});
    J(JAWP, {0.035f, 0, 1.915f});
    J(JAWT, {0.112f, 0, 1.872f});
    J(CRA, {0.092f, -0.048f, 2.035f});  // Riss: von der rechten Schlaefe ueber die Wange
    J(CRB, {0.112f, -0.030f, 1.945f});
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd == 0 ? 1.0f : -1.0f;
        const vec3 S{-0.005f, 0.215f * g, 1.655f};
        const vec3 E1 = S + vec3{0.0f, 0.03f * g, -0.298f};
        const vec3 E2 = E1 + vec3{0.02f, 0.02f * g, -0.268f};
        const vec3 W = E2 + vec3{0.025f, 0.012f * g, -0.268f};
        J(armJ(sd, 0), S);
        J(armJ(sd, 1), E1);
        J(armJ(sd, 2), E2);
        J(armJ(sd, 3), W);
        J(armJ(sd, 4), W + vec3{0.02f, -0.005f * g, -0.239f});  // Fingerspitzen: unter den Knien
        J(legJ(sd, 0), {0.0f, 0.098f * g, 1.01f});
        J(legJ(sd, 1), {0.02f, 0.098f * g, 0.545f});
        J(legJ(sd, 2), {0.0f, 0.096f * g, 0.088f});
        J(legJ(sd, 3), {0.215f, 0.10f * g, 0.02f});
    }
    J(TAILA_L, {-0.11f, 0.075f, 1.12f});
    J(TAILB_L, {-0.13f, 0.085f, 0.60f});
    J(TAILA_R, {-0.11f, -0.075f, 1.12f});
    J(TAILB_R, {-0.13f, -0.085f, 0.60f});
    J(BKA, {0.30f, -0.06f, 0.012f});  // Notizbuch flach am Boden: Ruecken -> Vorderkante (13 cm), Hoehe 19 cm quer
    J(BKB, {0.43f, -0.06f, 0.012f});

    B(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    B(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.16f, F);
    B(B_SPINE, SPINE, CHEST, B_PELVIS, 0.15f, F);
    B(B_CHEST, CHEST, NECK, B_SPINE, 0.17f, F);
    B(B_NECK, NECK, HEAD, B_CHEST, 0.06f, F);
    B(B_HEAD, HEAD, HEADTOP, B_NECK, 0.11f, F);
    B(B_JAW, JAWP, JAWT, B_HEAD, 0.04f, Z, false);
    B(B_CRACK, CRA, CRB, B_HEAD, 0.01f, F, false);
    for (int sd = 0; sd < 2; ++sd) {
        B(armB(sd, 0), CHEST, armJ(sd, 0), B_CHEST, 0.05f, Z, false);
        B(armB(sd, 1), armJ(sd, 0), armJ(sd, 1), armB(sd, 0), 0.05f, F);
        B(armB(sd, 2), armJ(sd, 1), armJ(sd, 2), armB(sd, 1), 0.045f, F);
        B(armB(sd, 3), armJ(sd, 2), armJ(sd, 3), armB(sd, 2), 0.04f, F);
        B(armB(sd, 4), armJ(sd, 3), armJ(sd, 4), armB(sd, 3), 0.035f, F, false);
        B(legB(sd, 0), legJ(sd, 0), legJ(sd, 1), B_PELVIS, 0.075f, F);
        B(legB(sd, 1), legJ(sd, 1), legJ(sd, 2), legB(sd, 0), 0.055f, F);
        B(legB(sd, 2), legJ(sd, 2), legJ(sd, 3), legB(sd, 1), 0.04f, Z);
    }
    B(B_TAIL_L, TAILA_L, TAILB_L, B_PELVIS, 0.06f, F);
    B(B_TAIL_R, TAILA_R, TAILB_R, B_PELVIS, 0.06f, F);
    B(B_BOOK, BKA, BKB, B_ROOT, 0.02f, Z, false);
    s.height = 2.12f;
    s.reach = 1.3f;
    s.center = {0.0f, 0, 1.15f};
    return s;
}

void CreatureAnimator::updateGentleman(float dt, const Input& in) {
    using namespace gm;
    auto& P = pose_.j;
    const auto& Bd = sk_->bind;
    if ((int)xs_.size() != X_SIZE) {
        xs_.assign((size_t)X_SIZE, 0.0f);
        xs_[X_ALERT] = in.alert, xs_[X_SYLT] = 0.1f;
    }
    float* X = xs_.data();
    if ((int)pose_.s.size() != BCOUNT) pose_.s.assign((size_t)BCOUNT, 1.0f);
    const float t = time_;
    const float ph = std::clamp(in.fly, 0.0f, 1.0f) * kBookPhases;  // Notizbuch
    // --- Federn der Eingaben (gemessen: langsamer als bei den Tieren) -----------------------------------------------------
    spring(X[X_ALERT], X[X_ALERTV], in.alert, 3.0f, dt);
    spring(X[X_TALK], X[X_TALKV], in.talk, 6.0f, dt);
    spring(X[X_PLEA], X[X_PLEAV], in.plea, 2.2f, dt);
    spring(X[X_PERC], X[X_PERCV], in.perceive, 1.8f, dt);
    spring(X[X_LIGHT], X[X_LIGHTV], in.light, 1.6f, dt);
    spring(X[X_GROWL], X[X_GROWLV], in.growl, in.growl > X[X_GROWL] ? 9.0f : 2.0f, dt);
    spring(X[X_REACH], X[X_REACHV], in.reach, in.reach > X[X_REACH] + 0.3f ? 11.0f : 3.2f, dt);
    spring(X[X_STRIKE], X[X_STRIKEV], in.strike, 10.0f, dt);
    spring(X[X_WAVE], X[X_WAVEV], in.wave, 4.0f, dt);
    spring(X[X_SIGH], X[X_SIGHV], in.shiver, 1.6f, dt);
    spring(X[X_TILT], X[X_TILTV], in.tilt, 2.0f, dt);
    const float alert = std::clamp(X[X_ALERT], 0.0f, 1.0f), talk = std::clamp(X[X_TALK], 0.0f, 1.0f);
    const float plea = std::clamp(X[X_PLEA], 0.0f, 1.0f), perc = std::clamp(X[X_PERC], 0.0f, 1.0f);
    const float lightK = std::clamp(X[X_LIGHT], 0.0f, 1.0f), growl = std::clamp(X[X_GROWL], 0.0f, 1.0f);
    const float reach = std::clamp(X[X_REACH], 0.0f, 1.0f), strike = std::clamp(X[X_STRIKE], 0.0f, 1.0f);
    const float wave = std::clamp(X[X_WAVE], 0.0f, 1.0f), sigh = std::clamp(X[X_SIGH], 0.0f, 1.2f);

    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    Fr root;
    root.o = pos_, root.x = F, root.y = Lw, root.z = U;

    // --- Notizbuch: Phase -> Verbeugung (Aufheben), Lage -------------------------------------------------------------
    const bool pick = ph > 2.0f && ph <= 3.0f;
    const float pu = pick ? ph - 2.0f : 0.0f;
    // steif aus der Huefte (bis 80 Grad), der Ruecken bleibt gerade; beim Wurf ein kleines Nachgeben nach vorn
    float bowT = pick ? 1.38f * sm01(pu / 0.32f) * (1.0f - sm01((pu - 0.5f) / 0.32f)) : 0.0f;
    if (ph > 4.0f && ph < 5.0f) bowT = 0.12f * std::sin(kPiG * std::clamp((ph - 4.15f) / 0.3f, 0.0f, 1.0f));
    spring(X[X_BOW], X[X_BOWV], bowT, 6.0f, dt);
    const float bow = std::clamp(X[X_BOW], -0.2f, 1.45f);

    // --- Beine: gemessene, lange Schritte ---------------------------------------------------------------------------------
    run_ = false;
    static const float offs[2] = {0.0f, 0.5f};
    const float duty = 0.6f;
    const float legL = length(Bd[legJ(0, 1)] - Bd[legJ(0, 0)]) + length(Bd[legJ(0, 2)] - Bd[legJ(0, 1)]);
    const float S = legL * (0.95f + 0.42f * speed_);
    vec3 rest[2];
    for (int sd = 0; sd < 2; ++sd) rest[sd] = root.at({Bd[legJ(sd, 2)].x, Bd[legJ(sd, 2)].y * 0.92f, 0.0f});
    {
        const bool walkNow = speed_ > 0.07f;
        if (walkNow && !leg_[0].swing && !leg_[1].swing) {  // Anlauf: der hintere Fuss tritt zuerst
            int lag = walk_ ? -1 : 0;
            float worst = 0.0f;
            for (int i = 0; i < 2; ++i) {
                vec3 d = leg_[i].plant - rest[i];
                d.z = 0.0f;
                if (-dot(d, F) > worst) worst = -dot(d, F), lag = i;
            }
            if (lag >= 0 && (!walk_ || worst > 0.45f * legL)) {
                const float q = duty + 0.01f - offs[lag];
                phase_ = q - std::floor(q);
            }
        }
        walk_ = walkNow;
    }
    const float wgt[2] = {0.55f, 0.55f};
    stepLegs(dt, 2, offs, duty, S, rest, 0.45f, wgt);
    const float lift[2] = {0.055f, 0.055f};
    vec3 g[2];
    for (int i = 0; i < 2; ++i) g[i] = footPos(i, lift);
    const bool moving = speed_ > 0.07f;
    const float mv = sm01(speed_ / 0.6f);
    const float c1 = std::cos(kTau * phase_);
    const float bob = moving ? -0.011f * mv * std::cos(2.0f * kTau * (phase_ - 0.05f)) : 0.0f;
    const float breath = std::sin(t * kTau * 0.16f);

    // Becken: kaum Seitschwung; Rumpf aufrecht, eine Spur nach hinten (stolz); Verbeugung um die Huefte
    vec3 Pp = pos_ + U * (Bd[PELVIS].z + bob - 0.035f * bow) + Lw * (0.012f * mv * std::cos(kTau * (phase_ - 0.3f))) - F * (0.06f * std::sin(bow));
    Fr pf = root;  // Becken
    const float py = -0.05f * c1 * mv;
    pf.turn(U, py);
    Fr tf = pf;  // Rumpf
    tf.turn(U, -py * 0.6f);
    tf.turn(tf.y, bow - 0.035f);
    P[ROOT] = pos_;
    P[PELVIS] = Pp;
    P[SPINE] = Pp + tf.dir(Bd[SPINE] - Bd[PELVIS]);
    P[CHEST] = P[SPINE] + tf.dir(Bd[CHEST] - Bd[SPINE]) + U * (0.004f * breath + 0.012f * sigh);
    P[NECK] = P[CHEST] + tf.dir(Bd[NECK] - Bd[CHEST]) + U * (0.02f * sigh);

    // Fuesse: im Schwung rollt der Fuss ab; Knie nach vorn
    const float ankH = Bd[legJ(0, 2)].z;
    for (int sd = 0; sd < 2; ++sd) {
        const Leg& Lg = leg_[sd];
        const vec3 fd0 = root.dir(Bd[legJ(sd, 3)] - Bd[legJ(sd, 2)]);
        float pitch = 0.0f;
        if (Lg.swing) pitch = 0.38f * std::sin(kPiG * Lg.t) * (1.0f - Lg.t) - 0.18f * sm01((Lg.t - 0.65f) / 0.35f);
        else if (moving) pitch = std::clamp((-dot(g[sd] - Pp, F) - 0.12f) * 1.6f, 0.0f, 0.6f);
        const vec3 fv = rot(fd0, Lw, pitch);
        vec3 ank = g[sd] + U * ankH + (Lg.swing ? vec3{} : fd0 - fv);
        if (ank.z + fv.z < pos_.z + 0.01f) ank.z += pos_.z + 0.01f - (ank.z + fv.z);
        const vec3 H = Pp + pf.dir(Bd[legJ(sd, 0)] - Bd[PELVIS]);
        const float l1 = length(Bd[legJ(sd, 1)] - Bd[legJ(sd, 0)]), l2 = length(Bd[legJ(sd, 2)] - Bd[legJ(sd, 1)]);
        vec3 knee, a2;
        ik2(H, ank, l1, l2, F, knee, a2);
        P[legJ(sd, 0)] = H;
        P[legJ(sd, 1)] = knee;
        P[legJ(sd, 2)] = a2;
        P[legJ(sd, 3)] = a2 + fv;
    }

    // --- Notizbuch: wo es ist (Mitte, Achse Ruecken->Vorderkante, Deckel-Normale) ------------------------------------------
    vec3 bookC = root.at(bookFeet()), bookX = rot(F, U, 0.5f), bookUp = U;
    bool bookShown = ph > 0.001f, bookInHand = false, bookOnFace = false;
    float throwU = -1.0f;
    if (ph > 0.001f && ph <= 1.0f) {  // faellt aus dem Regal, ueberschlaegt sich
        const float u = ph;
        const vec3 a = root.at(bookShelf()), b = root.at(bookFeet());
        bookC = lerp3(a, b, u) + U * (0.12f * std::sin(kPiG * u) * (1.0f - u));
        bookC.z = a.z + (b.z - a.z) * (u * u);
        const vec3 ax = nrm(cross(nrm(b - a, F), U), Lw);
        bookX = rot(rot(F, U, 0.5f), ax, 2.6f * u);
        bookUp = rot(U, ax, 2.6f * u);
    } else if (ph > 3.0f && ph <= 4.0f) {
        bookInHand = bookOnFace = true;
    } else if (pick && pu >= 0.45f) {
        bookInHand = true;
    } else if (ph > 4.0f && ph <= 5.0f) {
        throwU = ph - 4.0f;
        if (throwU < 0.30f) bookInHand = true;
    } else if (ph > 5.0f) {
        bookC = root.at(bookLand()), bookX = rot(F, U, 2.2f), bookUp = U;
    }

    // --- Kopf: waagrecht (auch in der Verbeugung), Blick, Wahrnehmen bis fast nach hinten, in die Lampe -----------------------
    float wy = 0.0f, wp = 0.0f;
    if (in.look) yawPitchG(P[NECK], in.lookAt, F, Lw, wy, wp);
    else {  // mustert die Gaenge und die Galerien
        const float idle = 1.0f - alert * 0.5f;
        wy = (0.75f * std::sin(t * 0.21f) + 0.25f * std::sin(t * 0.53f + 1.0f)) * idle * (1.0f - mv * 0.6f);
        wp = 0.10f + 0.12f * std::sin(t * 0.17f + 0.6f) * idle;
    }
    if (bookShown && ph < 4.05f) {  // er sieht das Buch an (faellt, liegt, in der Hand)
        float by, bp;
        const vec3 at = bookInHand ? root.at({0.38f, -0.04f, 1.66f}) : bookC;
        yawPitchG(P[NECK], at, F, Lw, by, bp);
        const float k = ph < 1.0f ? sm01(ph * 3.0f) : 1.0f;
        wy += (by - wy) * 0.85f * k, wp += (bp - wp) * 0.85f * k;
    }
    if (throwU >= 0.0f) wy *= 0.3f, wp = 0.05f;  // wirft, ohne hinzusehen
    if (perc > 0.01f) {
        float y2, p2;
        yawPitchG(P[NECK], in.perceiveAt, F, Lw, y2, p2);
        y2 = std::clamp(y2, -2.9f, 2.9f);
        wy += (y2 - wy) * perc, wp += (p2 - wp) * perc;
    }
    if (lightK > 0.01f) {  // er sieht in das Licht, ohne zu blinzeln
        float ly, lp;
        yawPitchG(P[NECK], in.lightFrom, F, Lw, ly, lp);
        wy += (std::clamp(ly, -2.4f, 2.4f) - wy) * lightK * 0.8f, wp += (lp - wp) * lightK * 0.8f;
    }
    wy = std::clamp(wrapA(wy), perc > 0.3f ? -2.9f : -1.5f, perc > 0.3f ? 2.9f : 1.5f);
    wp = std::clamp(wp, -0.9f, 0.7f);
    if (!chainInit_) X[X_HY] = wy, X[X_HP] = wp, X[X_HYV] = X[X_HPV] = 0.0f;
    const float hOm = perc > 0.3f ? 2.2f : (throwU >= 0.0f ? 6.0f : 3.4f);
    spring(X[X_HY], X[X_HYV], wy, hOm, dt);
    spring(X[X_HP], X[X_HPV], wp, hOm, dt);
    const float hy = X[X_HY];
    // Sprechen: leichtes Nicken; Silben kaum sichtbar
    X[X_SYLT] -= dt;
    if (X[X_SYLT] <= 0.0f) X[X_SYLT] = 0.11f + 0.1f * hashTo01((unsigned)(t * 977.0f)), X[X_SYL] = talk * (0.3f + 0.7f * hashTo01((unsigned)(t * 613.0f) + 3u));
    spring(X[X_SYLV], X[X_SYLW], X[X_SYL], 24.0f, dt);
    const float syl = std::clamp(X[X_SYLV], 0.0f, 1.0f);
    const float hp = X[X_HP] + 0.03f * talk * std::sin(t * 3.1f);
    const float tl = std::clamp(X[X_TILT], -0.6f, 0.6f) + 0.05f * talk * std::sin(t * 1.3f);
    // Hals: ein Drittel der Drehung; jenseits von 90 Grad streckt er sich (zu weit)
    const float over = std::clamp((std::fabs(hy) - 1.4f) / 1.4f, 0.0f, 1.0f);
    Fr nf = root;
    nf.turn(U, hy * 0.35f);
    nf.turn(nf.y, -hp * 0.3f);
    P[HEAD] = P[NECK] + nf.dir(Bd[HEAD] - Bd[NECK]) * (1.0f + 0.22f * over);
    Fr hf = root;
    hf.turn(U, hy);
    hf.turn(hf.y, -hp);
    hf.turn(hf.x, tl);
    auto headAt = [&](int j) { return P[HEAD] + hf.dir(Bd[(size_t)j] - Bd[HEAD]); };
    P[HEADTOP] = headAt(HEADTOP);
    P[JAWP] = headAt(JAWP);
    Fr jf = hf;
    jf.turn(hf.y, syl * 0.07f + 0.015f * growl);
    P[JAWT] = P[JAWP] + jf.dir(Bd[JAWT] - Bd[JAWP]);
    P[CRA] = headAt(CRA);
    P[CRB] = headAt(CRB);
    headPos_ = P[HEAD] + hf.dir(vec3{0.10f, 0.0f, 1.995f} - Bd[HEAD]);
    headDir_ = hf.x;
    pose_.s[gm::B_CRACK] = std::max(0.002f, growl);

    // --- Arme: drei Glieder je Arm --------------------------------------------------------------------------------------------
    for (int sd = 0; sd < 2; ++sd) {
        const float gg = sd == 0 ? 1.0f : -1.0f;
        const vec3 S = P[CHEST] + tf.dir(Bd[armJ(sd, 0)] - Bd[CHEST]) + U * (0.022f * sigh);
        const float L[3] = {length(Bd[armJ(sd, 1)] - Bd[armJ(sd, 0)]), length(Bd[armJ(sd, 2)] - Bd[armJ(sd, 1)]),
                            length(Bd[armJ(sd, 3)] - Bd[armJ(sd, 2)])};
        const float LH = length(Bd[armJ(sd, 4)] - Bd[armJ(sd, 3)]);
        // Grundhaltung: auf dem Ruecken verschraenkt (alert 0) .. haengend und pendelnd (alert 1)
        const vec3 bA{-0.42f, 0.05f * gg, -0.9f}, bB{-0.25f, -0.2f * gg, -0.95f}, bC{0.05f, -0.92f * gg, -0.38f}, bH{0.0f, -0.55f * gg, -0.83f};
        const vec3 hA{0.0f, 0.08f * gg, -1.0f}, hB{0.05f, 0.02f * gg, -1.0f}, hC{0.07f, 0.0f, -1.0f}, hH{0.05f, -0.02f * gg, -1.0f};
        const float swing = 0.2f * std::sin(kTau * phase_ + (sd ? kPiG : 0.0f)) * mv * alert;
        auto dirOf = [&](vec3 b, vec3 h, float lag) { return rot(tf.dir(nrm(lerp3(b, h, alert), h)), tf.y, -swing * lag); };
        vec3 p[4];
        p[0] = S;
        p[1] = p[0] + dirOf(bA, hA, 1.0f) * L[0];
        p[2] = p[1] + dirOf(bB, hB, 1.25f) * L[1];
        p[3] = p[2] + dirOf(bC, hC, 1.5f) * L[2];
        vec3 hd = dirOf(bH, hH, 1.7f);
        // Gesten: Handgelenk-Ziel (Rumpfrahmen bzw. Welt) und Handrichtung, gewichtet
        vec3 tgt = p[3], hdT = hd;
        float wgt2 = 0.0f;
        auto blend = [&](vec3 at, vec3 hdir, float w) {
            if (w <= 0.001f) return;
            const float k = std::clamp(w, 0.0f, 1.0f);
            tgt = lerp3(tgt, at, k), hdT = nrm(lerp3(hdT, hdir, k), hdir), wgt2 = std::max(wgt2, k);
        };
        blend(Pp + tf.dir(vec3{0.36f, 0.44f * gg, 1.24f} - Bd[PELVIS]), tf.dir(nrm({0.55f, 0.55f * gg, 0.15f}, F)), plea);
        blend(P[CHEST] + tf.dir({0.15f, 0.11f * gg, -0.10f + 0.025f * std::sin(t * 6.0f) * wave}), tf.dir(nrm({0.15f, -0.25f * gg, 0.95f}, U)), wave);
        if (sd == 1) {  // rechter Arm: Notizbuch, Griff
            if (pick) {
                const vec3 at = root.at(bookFeet()) + U * 0.045f - F * 0.02f;
                const vec3 hold = Pp + tf.dir(vec3{0.36f, -0.06f, 1.62f} - Bd[PELVIS]);
                const float down = sm01(pu / 0.4f), upk = sm01((pu - 0.5f) / 0.45f);
                blend(lerp3(at, hold, upk), nrm(lerp3(nrm(at - p[0], -U), tf.dir({0.2f, 0.25f, 0.95f}), upk), -U), down);
            } else if (ph > 3.0f && ph <= 4.0f) {  // Runde 2: vor der Brust war das schwarze Buch vor dem Frack nicht zu sehen
                blend(Pp + tf.dir(vec3{0.36f, -0.06f, 1.62f} - Bd[PELVIS]), tf.dir(nrm({0.2f, 0.25f, 0.95f}, U)), 1.0f);
            } else if (throwU >= 0.0f) {
                const float back = 1.0f - sm01((throwU - 0.6f) / 0.4f);
                blend(Pp + tf.dir(throwWrist(throwU) - Bd[PELVIS]), tf.dir(nrm({throwU < 0.3f ? 0.3f : -0.6f, -0.2f, 0.75f}, U)), back);
            }
            if (reach > 0.001f) {  // Griff: zum Gesicht des Ziels
                const vec3 aim = nrm(in.reachAt - S, F);
                const float maxL = (L[0] + L[1] + L[2]) * 0.98f;
                const vec3 face = in.reachAt - aim * 0.17f;
                const vec3 at = length(face - S) > maxL ? S + aim * maxL : face;
                // erst das obere Glied, dann das mittlere (entfaltet sich sichtbar Glied fuer Glied)
                blend(lerp3(S + tf.dir({0.25f, -0.08f, -0.45f}), at, sm01((reach - 0.2f) / 0.8f)), aim, sm01(reach / 0.3f));
            }
        } else if (ph > 3.0f && ph <= 4.0f) {  // linke Hand: blaettert
            const float leaf = std::sin(t * 2.3f);
            blend(Pp + tf.dir(vec3{0.37f, 0.07f, 1.6f + 0.015f * leaf} - Bd[PELVIS]), tf.dir(nrm({0.5f, -0.6f, 0.3f + 0.3f * leaf}, F)), sm01((ph - 3.0f) / 0.25f));
        }
        if (wgt2 > 0.001f) fabrik3(S, tgt, L, p);
        hd = hdT;
        P[armJ(sd, 0)] = p[0];
        P[armJ(sd, 1)] = p[1];
        P[armJ(sd, 2)] = p[2];
        P[armJ(sd, 3)] = p[3];
        const float handS = sd == 1 ? 1.0f + 0.24f * strike : 1.0f;  // die Finger wachsen beim Griff
        P[armJ(sd, 4)] = p[3] + hd * LH;
        pose_.s[(size_t)armB(sd, 4)] = handS;
        if (sd == 1 && bookInHand) {
            const vec3 side = nrm(cross(hd, tf.z), tf.y);
            bookC = p[3] + hd * 0.10f + side * 0.015f;
            bookX = nrm(cross(side, hd) * (bookOnFace ? 1.0f : 0.4f) + side * 0.6f, side);
            bookUp = nrm(cross(bookX, hd), tf.x);
        }
    }
    if (throwU >= 0.30f) {  // fliegt im Bogen hinter ihn und schlaegt auf
        const float k = std::clamp((throwU - 0.30f) / 0.70f, 0.0f, 1.0f);
        const vec3 a = Pp + tf.dir(throwWrist(0.30f) - Bd[PELVIS]), b = root.at(bookLand());
        bookC = lerp3(a, b, k) + U * (1.0f * 4.0f * k * (1.0f - k));
        if (k >= 1.0f) bookC = b;
        const vec3 ax = nrm(vec3{0.3f, 0.7f, 0.2f}, Lw);
        const float spin = (1.0f - k) * 9.0f;
        bookX = k >= 1.0f ? rot(F, U, 2.2f) : rot(F, ax, spin);
        bookUp = k >= 1.0f ? U : rot(U, ax, spin);
    }
    {
        const vec3 bx = nrm(bookX, F);
        P[BKA] = bookC - bx * 0.065f;
        P[BKB] = bookC + bx * 0.065f;
        pose_.s[gm::B_BOOK] = bookShown ? 1.0f : 0.001f;
    }

    // --- Rockschoesse: haengen nach, schwingen im Schritt --------------------------------------------------------------------
    for (int sd = 0; sd < 2; ++sd) {
        const int a = sd ? TAILA_R : TAILA_L, b = sd ? TAILB_R : TAILB_L;
        const float trail = std::clamp(dot(vel_, F) * 0.09f, -0.2f, 0.35f);
        spring(X[sd ? X_TB : X_TS], X[sd ? X_TBV : X_TSV], trail + 0.06f * std::sin(kTau * phase_ + (sd ? kPiG : 0.0f)) * mv, 5.0f, dt);
        const float sw = X[sd ? X_TB : X_TS];
        P[a] = Pp + pf.dir(Bd[a] - Bd[PELVIS]);
        vec3 d = pf.dir(Bd[b] - Bd[a]);
        d = rot(d, pf.y, -sw - 0.9f * bow);
        P[b] = P[a] + d;
    }

    autoUps();
    pose_.up[gm::B_HEAD] = hf.x;
    pose_.up[gm::B_JAW] = jf.z;
    pose_.up[gm::B_CRACK] = hf.x;
    pose_.up[gm::B_NECK] = nf.x;
    pose_.up[gm::B_PELVIS] = tf.x, pose_.up[gm::B_SPINE] = tf.x, pose_.up[gm::B_CHEST] = tf.x;
    pose_.up[gm::B_BOOK] = nrm(bookUp, U);
}

}  // namespace lim::anim
