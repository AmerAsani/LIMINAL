// V13.3: Ouroboros-Arbeiter (Wikidot level-1, Sektor Ouroboros; backrooms-remodeling-co; KREATUR_OuroborosArbeiter.md) -
// Skelett und Bewegung.
//
// Schattenhafte Arbeiter im Overall, die seit Jahrzehnten an derselben Baustelle arbeiten, ohne dass etwas fertig wird. Eigene
// Anatomie: 2,05 m, schmaler Rumpf, der Kopf haengt an einem langen Hals vor der Brust, die Arme reichen bis unter das Knie und
// haben zwischen Ellbogen und Handgelenk ein zweites Gelenk, das gegen den Ellbogen knickt (Zickzack). Der Brecher (Variante 1)
// ist schwer und gebueckt und geht auf den Knoecheln. Der Blick gilt immer der Arbeit - nie dem Spieler (die Eingabe lookAt
// wird nicht benutzt).
//
//   Ruhe/Gehen  langsam, schwer; die langen Arme haengen und pendeln kaum, der Kopf bleibt gesenkt; Rauch steigt von Schultern,
//               Scheitel und Ruecken auf und zieht beim Gehen nach
//   Tragen      ein Stein vor dem Bauch, beide Haende an den Seiten
//   Setzen      Stein von der Palette heben (alert 0) bzw. in die Lage setzen und mit der Kelle nachklopfen (alert 1); strike: Phase
//   Haemmern    der Hammer holt weit aus, das zweite Gelenk klappt nach hinten, schlaegt auf die Mauerkrone
//   Schweissen  kniet, der Brenner an der Naht des Geruests; Funken (shiver: Lichtbogen an) fliegen, springen am Boden
//   Presslufthammer (Brecher) ueber das Geraet gebeugt, alles bebt (shiver)
//   Einreissen  (Brecher) die Brechstange unter der Lage, zieht sie nach unten (strike) - die Lage kippt (Mauer, jaw)
//   Begutachten alle stehen still vor der fertigen Mauer, kein Wanken
//   Aufrichten  die Haende am umgestuerzten Geruest, es hebt sich
//   Vergehen    (fly) knickt ein, sinkt zusammen, zerfaellt von den Gliedern her in Rauch; Erscheinen umgekehrt
//   Licht       keine Reaktion im Verhalten - im Lampenkegel zerfasert der Rauch staerker und der Kopf ruckt kaum merklich
// Mauer (Variante 2): Eingaben reach = gesetzte Steine / 42, growl = abgetragene Lagen / 7, jaw = Kippen der obersten Lage,
// alert = Brocken (0 keine, bis 0,5 im Flug, danach liegen sie), shiver = der Presslufthammer laeuft.
// Geruest (Variante 3): tilt = Kippwinkel / 90 Grad (bis 1,06: federt nach), shiver = es knirscht und wankt.
// Weitere Eingaben (setExtra): Variante, Aufgabe (ob::Task).
#include <algorithm>
#include <cmath>

#include "anim/creature_anim.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);  // anim/skeleton.cpp

namespace {
constexpr float kPiO = 3.14159265f, kTauO = 6.2831853f;
vec3 rotO(vec3 v, vec3 axis, float ang) {
    const float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
float smO(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float wrapO(float a) { return std::remainder(a, kTauO); }
vec3 yawO(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
vec3 nrmO(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
void sprO(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
void sprV(vec3& x, vec3& v, vec3 target, float omega, float dt) {
    sprO(x.x, v.x, target.x, omega, dt);
    sprO(x.y, v.y, target.y, omega, dt);
    sprO(x.z, v.z, target.z, omega, dt);
}
float hashO(unsigned a) {
    a ^= a >> 16, a *= 0x7feb352du, a ^= a >> 15, a *= 0x846ca68bu, a ^= a >> 16;
    return (float)(a & 0xFFFFFF) / 16777216.0f;
}
// Drehung im Koerperraum (Spalten = gedrehte Achsen)
struct R3 {
    vec3 c0{1, 0, 0}, c1{0, 1, 0}, c2{0, 0, 1};
    vec3 operator*(vec3 v) const { return c0 * v.x + c1 * v.y + c2 * v.z; }
    R3 operator*(const R3& o) const { return R3{(*this) * o.c0, (*this) * o.c1, (*this) * o.c2}; }
};
R3 rotAxO(vec3 a, float ang) { return R3{rotO({1, 0, 0}, a, ang), rotO({0, 1, 0}, a, ang), rotO({0, 0, 1}, a, ang)}; }

struct OBuild {
    CreatureSkeleton s;
    OBuild(int joints, int bones) {
        s.kind = Entity::Ouroboros;
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
    vec3 dir(int a, int b) const { return nrmO(s.bind[(size_t)b] - s.bind[(size_t)a], {1, 0, 0}); }
};
vec3 upFor(vec3 d) { return std::fabs(d.z) > 0.8f ? vec3{1, 0, 0} : vec3{0, 0, 1}; }

// Lage der Werkzeuge in der Ruhepose: vor dem Koerper nebeneinander (Griff/Spitze, Ende) - das Modell entsteht dort
const vec3 kToolA[ob::kTools] = {{0.45f, -0.45f, 0.70f}, {0.45f, -0.20f, 0.70f}, {0.45f, 0.05f, 0.70f},
                                 {0.45f, 0.35f, 0.70f},  {0.80f, 0.00f, 0.00f},  {0.80f, 0.50f, 0.00f}};
const vec3 kToolB[ob::kTools] = {{0.45f, -0.45f, 1.02f}, {0.45f, -0.20f, 0.83f}, {0.45f, 0.05f, 0.95f},
                                 {0.45f, 0.35f, 0.98f},  {0.80f, 0.00f, 0.85f},  {0.80f, 0.50f, 1.30f}};
// Rauchfahnen: Ansatz (linke, rechte Schulter, Scheitel, Ruecken) und Richtung in Ruhe
const vec3 kWispA[ob::kWisps] = {{-0.02f, 0.17f, 1.63f}, {-0.02f, -0.17f, 1.63f}, {0.13f, 0.0f, 2.0f}, {-0.15f, 0.0f, 1.42f}};
const vec3 kWispD[ob::kWisps] = {{-0.05f, 0.04f, 0.17f}, {-0.05f, -0.04f, 0.17f}, {-0.03f, 0.0f, 0.18f}, {-0.09f, 0.0f, 0.16f}};

// Zustand der Bewegung (CreatureAnimator::ext_)
struct OState {
    int task = -1, prevTask = -1;
    float blend = 1.0f;
    float ph = 0.0f, act = 0.0f, actV = 0.0f;  // Phase zyklischer Aufgaben; Fortschritt einmaliger Handlungen (geglaettet)
    float crouch = 0.0f, crouchV = 0.0f, bow = 0.0f, bowV = 0.0f, twist = 0.0f, twistV = 0.0f, sway = 0.0f, swayV = 0.0f;
    float hy = 0.0f, hyV = 0.0f, hp = 0.4f, hpV = 0.0f, light = 0.0f, lightV = 0.0f, dis = 0.0f, disV = 0.0f;
    float carry = 0.0f, carryV = 0.0f, walkW = 0.0f, walkWV = 0.0f, vib = 0.0f, vibV = 0.0f;
    vec3 hand[2]{}, handV[2]{};
    bool handInit = false;
    float judT = 0.0f;
    vec3 jud{};
    float tilt = 0.0f, tiltV = 0.0f;  // Geruest
    float roll = 0.0f;                // Kopf schief (je Arbeiter)
};
}  // namespace

namespace ob {
CreatureSkeleton makeSkeleton() {
    OBuild k(JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(PELVIS, {0.0f, 0, 1.00f});
    k.j(BELLY, {0.01f, 0, 1.24f});
    k.j(CHEST, {0.02f, 0, 1.48f});
    k.j(NECK, {0.04f, 0, 1.62f});
    k.j(HEAD, {0.11f, 0, 1.79f});
    k.j(HEADTOP, {0.17f, 0, 1.99f});
    for (int s = 0; s < 2; ++s) {
        const float g = s == 0 ? 1.0f : -1.0f;
        const vec3 arm[5] = {{0.0f, g * 0.20f, 1.58f}, {-0.01f, g * 0.23f, 1.28f}, {0.01f, g * 0.245f, 1.00f},
                             {0.03f, g * 0.245f, 0.75f}, {0.05f, g * 0.24f, 0.53f}};
        for (int i = 0; i < 5; ++i) k.j(armJ(s, i), arm[i]);
        const vec3 leg[4] = {{0.0f, g * 0.10f, 0.97f}, {0.03f, g * 0.11f, 0.52f}, {0.0f, g * 0.11f, 0.09f}, {0.21f, g * 0.11f, 0.03f}};
        for (int i = 0; i < 4; ++i) k.j(legJ(s, i), leg[i]);
    }
    for (int t = 0; t < kTools; ++t) k.j(toolJ(t, 0), kToolA[t]), k.j(toolJ(t, 1), kToolB[t]);
    for (int w = 0; w < kWisps; ++w) {
        k.j(wispJ(w, 0), kWispA[w]);
        k.j(wispJ(w, 1), kWispA[w] + kWispD[w]);
        k.j(wispJ(w, 2), kWispA[w] + kWispD[w] * 2.0f + vec3{-0.02f, 0, 0.0f});
    }
    for (int q = 0; q < kSparks; ++q) {
        const vec3 a{0.62f, 0.30f + 0.035f * (float)q, 1.12f};
        k.j(sparkJ(q, 0), a);
        k.j(sparkJ(q, 1), a + vec3{0.05f, 0, 0});
    }
    // Mauer: entlang y, Mitte am Boden; Lage c von linkem zu rechtem Ende (unten, Mitte der Dicke); die losen Steine und die
    // Brocken liegen in der Ruhepose ueber der Mauer (sonst wuechsen sie im Modell mit den Lagen zusammen)
    for (int c = 0; c < kCourses; ++c) {
        k.j(courseJ(c, 0), {0.0f, -kWallL * 0.5f, (float)c * kCourseH});
        k.j(courseJ(c, 1), {0.0f, kWallL * 0.5f, (float)c * kCourseH});
    }
    for (int q = 0; q < kLay; ++q) {
        const float y = -kWallL * 0.5f + ((float)q + 0.5f) * kBlockL;
        k.j(layJ(q, 0), {0.0f, y, 2.05f});
        k.j(layJ(q, 1), {0.0f, y, 2.30f});
    }
    for (int q = 0; q < kDebris; ++q) {
        const vec3 a{0.0f, -0.75f + 0.5f * (float)q, 2.55f};
        k.j(debJ(q, 0), a);
        k.j(debJ(q, 1), a + vec3{0, 0, 0.12f});
    }
    k.j(TOWER_A, {kTowerD * 0.5f, 0, 0});
    k.j(TOWER_B, {kTowerD * 0.5f, 0, kTowerH});
    const vec3 F{1, 0, 0}, Z{0, 0, 1}, Y{0, 1, 0};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_PELVIS, PELVIS, BELLY, B_ROOT, 0.16f, F);
    k.b(B_BELLY, BELLY, CHEST, B_PELVIS, 0.15f, F);
    k.b(B_CHEST, CHEST, NECK, B_BELLY, 0.17f, F);
    k.b(B_NECK, NECK, HEAD, B_CHEST, 0.05f, F);
    k.b(B_HEAD, HEAD, HEADTOP, B_NECK, 0.09f, F);
    for (int s = 0; s < 2; ++s) {
        for (int i = 0; i < 4; ++i) {
            const int a = armJ(s, i), b = armJ(s, i + 1);
            k.b(armB(s, i), a, b, i == 0 ? B_CHEST : armB(s, i - 1), i == 3 ? 0.03f : 0.045f - 0.004f * (float)i, upFor(k.dir(a, b)), i < 3);
        }
        k.b(legB(s, 0), legJ(s, 0), legJ(s, 1), B_PELVIS, 0.075f, F);
        k.b(legB(s, 1), legJ(s, 1), legJ(s, 2), legB(s, 0), 0.06f, F);
        k.b(legB(s, 2), legJ(s, 2), legJ(s, 3), legB(s, 1), 0.05f, Z);
    }
    for (int t = 0; t < kTools; ++t) k.b(B_TOOL0 + t, toolJ(t, 0), toolJ(t, 1), armB(1, 3), t == T_JACK ? 0.07f : 0.025f, F, t == T_JACK || t == T_BAR);
    for (int w = 0; w < kWisps; ++w)
        for (int i = 0; i < 2; ++i) k.b(wispB(w, i), wispJ(w, i), wispJ(w, i + 1), i == 0 ? B_CHEST : wispB(w, 0), 0.03f, F, false);
    for (int q = 0; q < kSparks; ++q) k.b(B_SPARK0 + q, sparkJ(q, 0), sparkJ(q, 1), B_ROOT, 0.005f, Z, false);
    for (int c = 0; c < kCourses; ++c) k.b(B_COURSE0 + c, courseJ(c, 0), courseJ(c, 1), B_ROOT, 0.12f, Z);
    for (int q = 0; q < kLay; ++q) k.b(B_LAY0 + q, layJ(q, 0), layJ(q, 1), B_ROOT, 0.1f, F);
    for (int q = 0; q < kDebris; ++q) k.b(B_DEB0 + q, debJ(q, 0), debJ(q, 1), B_ROOT, 0.05f, F, false);
    k.b(B_TOWER, TOWER_A, TOWER_B, B_ROOT, 0.45f, F);
    (void)Y;
    k.s.height = 2.05f;
    k.s.reach = 3.9f;  // das Geruest liegt umgekippt 3,4 m weit
    k.s.center = {0.0f, 0, 1.0f};
    return k.s;
}
}  // namespace ob

float CreatureAnimator::ouroHeight() const {
    const int v = extra_.empty() ? 0 : (int)extra_[0];
    if (v == 2) return ob::kCourses * ob::kCourseH;
    if (v == 3) return ob::kTowerH;
    return std::max(0.5f, pose_.j[ob::HEADTOP].z - pos_.z);
}

vec3 CreatureAnimator::ouroHead() const {
    const int v = extra_.empty() ? 0 : (int)extra_[0];
    if (v == 2) return pos_ + vec3{0, 0, 1.0f};
    if (v == 3) return pos_ + vec3{0, 0, 2.0f};
    return (pose_.j[ob::HEAD] + pose_.j[ob::HEADTOP]) * 0.5f;
}

void CreatureAnimator::updateOuroboros(float dt, const Input& in) {
    using namespace ob;
    if (!ext_) {
        ext_ = std::make_shared<OState>();
        static_cast<OState*>(ext_.get())->roll = (hashO(rng_) - 0.5f) * 0.32f;
    }
    OState& g = *static_cast<OState*>(ext_.get());
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const size_t nb = sk_->bones.size();
    const int variant = extra_.size() > 0 ? (int)extra_[0] : 0;
    const int task = extra_.size() > 1 ? (int)extra_[1] : K_IDLE;
    const vec3 F = yawO(yaw_), L{-F.y, F.x, 0.0f}, U{0, 0, 1};
    auto W = [&](vec3 l) { return pos_ + F * l.x + L * l.y + U * l.z; };
    pose_.s.assign(nb, 1.0f);
    auto hide = [&](int b) { pose_.s[(size_t)b] = 0.001f; };
    auto park = [&](int j0, int j1, vec3 at) {  // unbenutzte Gelenke an einen Punkt (Mitte fuer Sichtbarkeit)
        for (int j = j0; j < j1; ++j) J[(size_t)j] = at;
    };

    // ============================================== Mauer ======================================================
    if (variant == 2) {
        const int laid = std::clamp((int)std::lround(in.reach * 42.0f), 0, 42);
        const int broken = std::clamp((int)std::lround(in.growl * 7.0f), 0, kCourses);
        const int full = laid / kLay, part = laid % kLay, top = kCourses - broken;
        J[ROOT] = pos_;
        park(PELVIS, COURSE0, W({0, 0, 0.9f}));
        park(TOWER_A, JCOUNT, W({0, 0, 0.9f}));
        for (int b = B_PELVIS; b < B_COURSE0; ++b) hide(b);
        hide(B_TOWER);
        autoUps();
        const vec3 vib = (F * std::sin(time_ * 61.0f) + L * std::sin(time_ * 47.0f + 1.0f)) * (0.004f * in.shiver);
        for (int c = 0; c < kCourses; ++c) {
            vec3 a = W({0, -kWallL * 0.5f, (float)c * kCourseH}), b = W({0, kWallL * 0.5f, (float)c * kCourseH});
            vec3 up = U;
            bool vis = c < full && c < top;
            if (c == top - 1 && vis) a = a + vib, b = b + vib;
            if (c == top - 1 && vis && in.jaw > 0.001f) {  // eingerissen: kippt um die Unterkante auf der Rueckseite, faellt
                const float th = 0.5f * kPiO * smO(in.jaw / 0.55f);
                const float fall = std::clamp((in.jaw - 0.42f) / 0.3f, 0.0f, 1.0f);
                const vec3 piv = W({-kWallT * 0.5f, 0, (float)c * kCourseH});
                const vec3 drop = -U * ((float)c * kCourseH * fall * fall);
                a = piv + rotO(a - piv, L, -th) + drop;
                b = piv + rotO(b - piv, L, -th) + drop;
                up = rotO(U, L, -th);
                if (in.jaw >= 0.995f) vis = false;
            }
            J[(size_t)courseJ(c, 0)] = a, J[(size_t)courseJ(c, 1)] = b;
            pose_.up[(size_t)(B_COURSE0 + c)] = up;
            if (!vis) hide(B_COURSE0 + c);
        }
        for (int q = 0; q < kLay; ++q) {
            const int c = std::min(full, kCourses - 1);
            const float y = -kWallL * 0.5f + ((float)q + 0.5f) * kBlockL + ((c & 1) ? kBlockL * 0.5f : 0.0f);
            const vec3 a = W({0, y, (float)c * kCourseH});
            J[(size_t)layJ(q, 0)] = a, J[(size_t)layJ(q, 1)] = a + U * kCourseH;
            pose_.up[(size_t)(B_LAY0 + q)] = F;
            if (!(q < part && full < top && full < kCourses)) hide(B_LAY0 + q);
        }
        // Brocken der zuletzt zerschlagenen Lage (Index top): fliegen bis alert 0,5, dann liegen sie im abgesperrten Feld
        for (int q = 0; q < kDebris; ++q) {
            const float h1 = hashO((unsigned)(q * 977 + top * 131 + 7)), h2 = hashO((unsigned)(q * 613 + top * 59 + 3));
            const vec3 a0 = W({0.0f, -0.8f + 0.53f * (float)q, (float)top * kCourseH + 0.12f});
            const vec3 a1 = W({(h1 < 0.5f ? -1.0f : 1.0f) * (0.35f + 0.6f * h2), -0.9f + 0.6f * (float)q + 0.25f * (h1 - 0.5f), 0.0f});
            const float t = std::clamp(in.alert / 0.5f, 0.0f, 1.0f);
            const vec3 p = a0 + (a1 - a0) * t + U * (std::sin(kPiO * t) * (0.25f + 0.2f * h2));
            const float spin = t * (5.0f + 4.0f * h1);
            const vec3 d = nrmO(rotO(U, nrmO(L + F * (h2 - 0.5f), L), spin * (t < 1.0f ? 1.0f : 0.0f) + (t >= 1.0f ? 1.45f + h1 : 0.0f)), U);
            J[(size_t)debJ(q, 0)] = p + U * 0.02f;
            J[(size_t)debJ(q, 1)] = p + U * 0.02f + d * 0.12f;
            pose_.up[(size_t)(B_DEB0 + q)] = nrmO(cross(d, L), F);
            if (in.alert < 0.001f || top >= kCourses) hide(B_DEB0 + q);
        }
        pose_.up[B_ROOT] = F;
        headPos_ = pos_ + U * 1.0f;
        return;
    }

    // ============================================== Geruest ====================================================
    if (variant == 3) {
        sprO(g.tilt, g.tiltV, in.tilt, 30.0f, dt);  // weich, aber ohne Verzug (der Host rechnet den Fall)
        J[ROOT] = pos_;
        park(PELVIS, TOWER_A, W({0, 0, 1.0f}));
        for (int b = B_PELVIS; b < B_TOWER; ++b) hide(b);
        autoUps();
        const float sway = in.shiver * (0.03f * std::sin(time_ * 5.3f) + 0.012f * std::sin(time_ * 13.1f + 0.7f));
        const float roll = in.shiver * 0.018f * std::sin(time_ * 3.7f + 1.1f);
        const float th = g.tilt * 0.5f * kPiO + sway;
        vec3 dir = rotO(U, L, th);
        dir = rotO(dir, F, roll);
        const vec3 piv = W({kTowerD * 0.5f, 0, 0});
        J[TOWER_A] = piv;
        J[TOWER_B] = piv + dir * kTowerH;
        pose_.up[B_TOWER] = rotO(rotO(F, L, th), F, roll);
        pose_.up[B_ROOT] = F;
        // fuer die Sichtbarkeit: Mitte des Geruests
        park(PELVIS, TOWER_A, piv + dir * (kTowerH * 0.5f) - F * (kTowerD * 0.5f));
        headPos_ = piv + dir * (kTowerH * 0.6f);
        return;
    }

    // ============================================== Arbeiter ===================================================
    const bool heavy = variant == 1;
    const float dis = std::clamp(in.fly, 0.0f, 1.0f);
    if (task != g.task) g.prevTask = g.task, g.task = task, g.blend = 0.0f;
    g.blend = std::min(1.0f, g.blend + dt * 2.2f);
    sprO(g.light, g.lightV, in.light, 5.0f, dt);
    sprO(g.dis, g.disV, dis, 6.0f, dt);
    sprO(g.walkW, g.walkWV, speed_ > 0.12f ? 1.0f : 0.0f, 4.0f, dt);
    const bool carryTask = task == K_CARRY || (task == K_LAY && in.alert < 0.5f && in.strike > 0.45f);
    sprO(g.carry, g.carryV, carryTask ? 1.0f : 0.0f, 6.0f, dt);
    sprO(g.vib, g.vibV, task == K_JACK ? in.shiver : 0.0f, 9.0f, dt);
    // einmalige Handlungen (Setzen, Einreissen): Fortschritt aus der Eingabe, geglaettet; Zyklen (Haemmern): eigene Uhr, sanft
    // auf die Phase des Hosts gezogen (Puppen bekommen sie nur wenige Male je Sekunde)
    if (task == K_LAY || task == K_PRY) {
        if (g.act > in.strike + 0.5f) g.act = in.strike, g.actV = 0.0f;  // neue Handlung
        sprO(g.act, g.actV, in.strike, 14.0f, dt);
    }
    if (task == K_HAMMER) {
        g.ph += dt / 0.85f;
        g.ph += std::remainder(in.strike - g.ph, 1.0f) * std::min(1.0f, dt * 2.5f);
        g.ph -= std::floor(g.ph);
    }
    const float u = std::clamp(g.act, 0.0f, 1.0f);
    // Ziel der Arbeit (Welt): vor dem Arbeiter im Abstand je Aufgabe, seitlich lean, Hoehe tilt x 2,5 m
    float dTask = 1.0f;
    switch (task) {
        case K_LAY: dTask = 0.55f; break;
        case K_HAMMER: dTask = 0.58f; break;
        case K_WELD: dTask = 0.52f; break;
        case K_JACK: dTask = 0.42f; break;
        case K_PRY: dTask = 0.62f; break;
        case K_LIFT: dTask = 0.38f; break;
        case K_INSPECT: dTask = 1.3f; break;
        default: break;
    }
    const vec3 T = pos_ + F * dTask + L * in.lean + U * (in.tilt * 2.5f);
    const bool working = task == K_LAY || task == K_HAMMER || task == K_WELD || task == K_JACK || task == K_PRY || task == K_LIFT ||
                         task == K_INSPECT;

    // --- Beine ----------------------------------------------------------------------------------------------
    const int nLeg = heavy ? 4 : 2;
    const float footY = heavy ? 0.16f : 0.11f;
    vec3 rest[4];
    rest[0] = W({0.0f, footY, 0.0f}), rest[1] = W({0.0f, -footY, 0.0f});
    rest[2] = W({0.62f, 0.32f, 0.0f}), rest[3] = W({0.62f, -0.32f, 0.0f});
    if (!chainInit_ || !g.handInit) {
        for (int i = 0; i < 4; ++i) {
            Leg& l = leg_[i];
            l.plant = l.from = l.to = rest[i];
            l.swing = false, l.t = 0.0f;
        }
    }
    const float offs[4] = {0.0f, 0.5f, 0.25f, 0.75f}, wgt[4] = {1.0f, 1.0f, 0.6f, 0.6f};
    const float stride = heavy ? 0.42f + 0.2f * std::min(speed_, 2.0f) : 0.5f + 0.22f * std::min(speed_, 2.0f);
    stepLegs(dt, nLeg, offs, heavy ? 0.66f : 0.62f, stride, rest, 0.32f, wgt);
    const float lift[4] = {0.09f, 0.09f, 0.07f, 0.07f};
    vec3 foot[4];
    for (int i = 0; i < nLeg; ++i) foot[i] = footPos(i, lift);
    if (!heavy) foot[2] = rest[2], foot[3] = rest[3];
    const bool moving = speed_ > 0.07f;
    const float ph = kTauO * phase_;

    // --- Haltung -------------------------------------------------------------------------------------------
    float wantCrouch = task == K_WELD ? 0.85f : 0.0f;
    float relH = T.z - pos_.z;
    float wantBow = heavy ? 0.95f : 0.14f;
    if (moving) wantBow += heavy ? 0.1f : 0.08f;
    if (g.carry > 0.5f) wantBow = heavy ? 0.7f : 0.04f;
    switch (task) {
        case K_LAY:
            if (in.alert < 0.5f) wantBow = (heavy ? 1.0f : 0.25f) + 1.0f * std::sin(kPiO * std::min(1.0f, u / 0.9f)) * std::clamp((1.25f - relH) / 0.9f, 0.2f, 1.0f);
            else wantBow = 0.12f + 0.95f * std::clamp((1.35f - relH) / 1.3f, 0.0f, 1.0f) * (u < 0.7f ? 1.0f : 0.8f);
            break;
        case K_HAMMER: wantBow = (heavy ? 0.8f : 0.15f) + 0.9f * std::clamp((1.3f - relH) / 1.3f, 0.0f, 1.0f); break;
        case K_WELD: wantBow = 0.38f; break;
        case K_JACK: wantBow = 0.62f + 0.25f * std::clamp((0.75f - relH) / 0.5f, 0.0f, 1.0f); break;
        case K_PRY: wantBow = 0.55f + 0.25f * std::sin(kPiO * u) - 0.2f * u; break;
        case K_INSPECT: wantBow = heavy ? 0.85f : 0.2f; break;
        case K_LIFT: wantBow = 0.1f; break;
        default: break;
    }
    // vergehen: sackt zusammen, faltet sich nach vorn
    wantBow += 0.9f * smO(dis * 1.6f);
    wantCrouch = std::max(wantCrouch, smO(dis * 1.4f));
    sprO(g.crouch, g.crouchV, wantCrouch, 4.0f, dt);
    sprO(g.bow, g.bowV, wantBow, task == K_LAY ? 7.0f : 4.0f, dt);
    float wantTw = 0.0f;
    if (working) {
        const vec3 d = T - pos_;
        wantTw = std::clamp(wrapO(std::atan2(d.y, d.x) - yaw_), -0.6f, 0.6f);
    }
    sprO(g.twist, g.twistV, wantTw, 4.0f, dt);
    const float idleSway = task == K_INSPECT ? 0.0f : 0.012f * std::sin(time_ * 0.55f + g.roll * 9.0f) + 0.006f * std::sin(time_ * 1.7f);
    sprO(g.sway, g.swayV, (moving ? std::sin(ph) * (heavy ? 0.07f : 0.045f) : 0.0f) + idleSway, 6.0f, dt);
    // Erschuetterung durch den Presslufthammer
    const vec3 vib = (F * std::sin(time_ * 71.0f) + L * std::sin(time_ * 53.0f + 0.4f) + U * std::sin(time_ * 89.0f + 1.3f)) * (0.006f * g.vib);

    // Becken
    const float standH = heavy ? 0.84f : 0.97f;
    float pelvisZ = standH - g.crouch * (heavy ? 0.3f : 0.44f) + (moving ? 0.022f * std::cos(2.0f * ph) : 0.0f);
    pelvisZ -= 0.35f * smO((dis - 0.3f) * 1.6f);
    const float hipBack = 0.1f * std::clamp(g.bow - 0.3f, 0.0f, 1.0f) * (heavy ? 0.5f : 1.0f);
    const vec3 P = pos_ + F * (B[PELVIS].x - hipBack) + L * g.sway * 0.6f + U * pelvisZ + vib * 0.5f;
    const R3 Mb{F, L, U};
    const vec3 Ly{0, 1, 0}, Fx{1, 0, 0}, Uz{0, 0, 1};
    auto Rpart = [&](float k) { return rotAxO(Uz, g.twist * k) * rotAxO(Ly, g.bow * k) * rotAxO(Fx, -g.sway * 0.4f); };
    const R3 Rlow = Rpart(0.35f), Rmid = Rpart(0.75f), Rt = Rpart(1.0f);
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[BELLY] = P + Mb * (Rlow * (B[BELLY] - B[PELVIS]));
    J[CHEST] = J[BELLY] + Mb * (Rmid * (B[CHEST] - B[BELLY])) + vib;
    J[NECK] = J[CHEST] + Mb * (Rt * (B[NECK] - B[CHEST]));
    const vec3 TF = Mb * (Rt * Fx), TL = Mb * (Rt * Ly), TU = Mb * (Rt * Uz);  // Brustkorb in Weltachsen

    // Kopf: zur Arbeit (oder gesenkt vor die Fuesse) - nie zum Spieler
    vec3 lookDir = nrmO(F * 0.8f - U * 0.75f, F);
    if (working) lookDir = nrmO(T - J[NECK], lookDir);
    if (g.carry > 0.5f && !working) lookDir = nrmO(F * 0.7f - U * 0.85f, F);
    // relativ zum Brustkorb
    const vec3 lb{dot(lookDir, TF), dot(lookDir, TL), dot(lookDir, TU)};
    float hyW = std::clamp(std::atan2(lb.y, std::max(0.05f, lb.x)), -0.9f, 0.9f);
    float hpW = std::clamp(std::asin(std::clamp(-lb.z, -1.0f, 1.0f)), -0.5f, 1.25f);
    hpW += 0.6f * smO(dis * 1.5f);
    sprO(g.hy, g.hyV, hyW, 4.5f, dt);
    sprO(g.hp, g.hpV, hpW, 4.5f, dt);
    // im Lampenlicht: kaum merkliches Rucken (wie ein Bild, dem Bilder fehlen)
    g.judT -= dt;
    if (g.judT <= 0.0f) {
        g.judT = 0.06f + 0.1f * hashO(rng_ + (unsigned)(time_ * 100.0f));
        const float a = 0.05f * g.light;
        g.jud = vec3{(hashO((unsigned)(time_ * 977.0f)) - 0.5f) * a, (hashO((unsigned)(time_ * 613.0f) + 9u) - 0.5f) * a, 0.0f};
    }
    const R3 Rh = Rt * rotAxO(Uz, g.hy + g.jud.x) * rotAxO(Ly, g.hp - 0.35f + g.jud.y) * rotAxO(Fx, g.roll + (task == K_INSPECT ? g.roll * 1.5f : 0.0f));
    J[HEAD] = J[NECK] + Mb * (Rh * (B[HEAD] - B[NECK])) * 1.0f;
    J[HEADTOP] = J[HEAD] + Mb * (Rh * (B[HEADTOP] - B[HEAD]));

    // --- Arme ----------------------------------------------------------------------------------------------
    const float lUp = length(B[armJ(0, 1)] - B[armJ(0, 0)]), lF1 = length(B[armJ(0, 2)] - B[armJ(0, 1)]);
    const float lF2 = length(B[armJ(0, 3)] - B[armJ(0, 2)]), lHand = length(B[armJ(0, 4)] - B[armJ(0, 3)]);
    vec3 S[2];
    for (int s = 0; s < 2; ++s) S[s] = J[CHEST] + Mb * (Rt * (B[armJ(s, 0)] - B[CHEST]));
    vec3 hand[2], hdir[2];
    float bend2[2] = {0.45f, 0.45f};  // Knick im zweiten Gelenk (rad)
    int tool = -1;
    vec3 toolA, toolB, toolUp = U;
    // haengend: bis unter das Knie, pendeln wenig
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const float swing = moving ? std::sin(ph + (s == 0 ? 0.0f : kPiO)) * (heavy ? 0.0f : 0.1f) : 0.0f;
        hand[s] = S[s] - U * ((lUp + lF1 + lF2) * 0.9f) + F * (0.05f + swing) + L * (sg * 0.03f) + TF * 0.08f;
        hdir[s] = nrmO(-U + F * 0.15f, -U);
        if (heavy && moving) {  // auf den Knoecheln
            hand[s] = foot[2 + s] + U * (lHand * 0.55f);
            hdir[s] = nrmO(-U + F * 0.4f, -U);
            bend2[s] = 0.3f;
        }
    }
    // Stein vor dem Bauch (Tragen)
    const vec3 carryC = J[CHEST] + TF * 0.36f - U * 0.42f;
    if (g.carry > 0.01f && task != K_LAY) {
        for (int s = 0; s < 2; ++s) {
            const float sg = s == 0 ? 1.0f : -1.0f;
            hand[s] = hand[s] + ((carryC + TL * (sg * 0.2f)) - hand[s]) * g.carry;
            hdir[s] = nrmO(hdir[s] * (1.0f - g.carry) + (-TL * sg - U * 0.3f) * g.carry, -U);
        }
        if (g.carry > 0.5f) tool = T_BRICK, toolA = carryC - U * (kCourseH * 0.5f), toolB = carryC + U * (kCourseH * 0.5f), toolUp = TF;
    }
    switch (task) {
        case K_LAY: {
            if (in.alert < 0.5f) {  // von der Palette heben: hinab zum Stein (T), greifen, aufrichten
                const float down = smO(u / 0.45f), up = smO((u - 0.45f) / 0.55f);
                const vec3 grab = T + U * (kCourseH * 0.5f);
                const vec3 c = grab + (carryC - grab) * up;
                for (int s = 0; s < 2; ++s) {
                    const float sg = s == 0 ? 1.0f : -1.0f;
                    const vec3 tgt = (u < 0.45f ? grab : c) + TL * (sg * 0.2f);
                    hand[s] = hand[s] + (tgt - hand[s]) * (u < 0.45f ? down : 1.0f);
                    hdir[s] = nrmO(-TL * sg - U * 0.4f, -U);
                    bend2[s] = 0.7f;
                }
                if (u >= 0.45f) tool = T_BRICK, toolA = c - U * (kCourseH * 0.5f), toolB = c + U * (kCourseH * 0.5f), toolUp = TF;
            } else {  // setzen: den Stein auf die Lage (T = Mitte unten), dann mit der Kelle nachklopfen
                const float mv = smO(u / 0.55f);
                const vec3 c0 = carryC, c1 = T + U * (kCourseH * 0.5f);
                const vec3 c = c0 + (c1 - c0) * mv;
                if (u < 0.56f) {
                    tool = T_BRICK, toolA = c - U * (kCourseH * 0.5f), toolB = c + U * (kCourseH * 0.5f), toolUp = TF;
                    for (int s = 0; s < 2; ++s) {
                        const float sg = s == 0 ? 1.0f : -1.0f;
                        hand[s] = c + TL * (sg * 0.2f) + U * 0.05f;
                        hdir[s] = nrmO(-TL * sg - U * 0.5f, -U);
                        bend2[s] = 0.8f;
                    }
                } else {
                    // Kelle (rechte Hand): zwei Klopfer auf den Stein; links stuetzt auf der Krone
                    const float k = (u - 0.56f) / 0.44f;
                    const float tap = std::fabs(std::sin(k * 2.0f * kPiO));
                    const vec3 topC = T + U * kCourseH;
                    hand[1] = topC + U * (0.06f + 0.16f * tap) - TL * 0.05f + F * -0.08f;
                    hdir[1] = nrmO(F * 0.6f + U * 0.2f, F);
                    hand[0] = topC + TL * 0.35f + U * 0.04f;
                    hdir[0] = nrmO(F - U * 0.6f, F);
                    bend2[1] = 1.0f, bend2[0] = 0.6f;
                    tool = T_TROWEL, toolA = hand[1] - hdir[1] * 0.02f, toolB = hand[1] + hdir[1] * 0.11f, toolUp = TL;
                }
            }
            break;
        }
        case K_HAMMER: {
            // Ausholen (0..0,45), Schlag (0,45..0,55), zurueckfedern; der Hammerkopf trifft T (Mauerkrone)
            const float p = g.ph;
            float a;  // Winkel des Stiels: + nach vorn unten, - nach oben hinten
            if (p < 0.45f) a = 0.5f - 2.0f * smO(p / 0.45f);
            else if (p < 0.55f) a = -1.5f + 2.0f * ((p - 0.45f) / 0.1f) * ((p - 0.45f) / 0.1f);
            else a = 0.5f - 0.15f * std::sin((p - 0.55f) / 0.45f * kPiO);
            const float raise = std::clamp((0.5f - a) / 2.0f, 0.0f, 1.0f);
            const vec3 dirH = nrmO(rotO(TF, TL, a), TF);
            const vec3 grip = T - nrmO(rotO(TF, TL, 0.5f), TF) * 0.3f + U * (0.28f * raise) - TF * (0.12f * raise) - TL * 0.04f;
            hand[1] = grip;
            hdir[1] = nrmO(dirH + U * 0.1f, dirH);
            bend2[1] = 0.5f + 1.1f * raise;
            hand[0] = T + TL * 0.32f + U * 0.03f;
            hdir[0] = nrmO(F - U * 0.8f, -U);
            bend2[0] = 0.6f;
            tool = T_HAMMER, toolA = grip - dirH * 0.04f, toolB = grip + dirH * 0.3f, toolUp = TL;
            break;
        }
        case K_WELD: {
            vec3 tp = T + TL * (0.05f * std::sin(time_ * 0.6f)) + U * (0.02f * std::sin(time_ * 0.9f + 1.0f));
            const vec3 n = nrmO(tp - (J[CHEST] - U * 0.25f), F);
            const vec3 grip = tp - n * 0.27f;
            hand[1] = grip;
            hdir[1] = n;
            bend2[1] = 1.1f;
            hand[0] = grip + TL * 0.16f - n * 0.06f + U * 0.05f;  // fuehrt den Schlauch
            hdir[0] = nrmO(n - TL * 0.5f, n);
            bend2[0] = 0.9f;
            tool = T_TORCH, toolA = grip - n * 0.03f, toolB = tp, toolUp = U;
            break;
        }
        case K_JACK: {
            const float k = std::clamp((relH - 0.25f) / 0.5f, 0.0f, 1.0f) * 0.55f;
            const vec3 ax = nrmO(U * (1.0f - k) - F * (0.15f + k), U);
            const vec3 tip = T + vib * 2.0f;
            const vec3 top = tip + ax * 0.85f;
            for (int s = 0; s < 2; ++s) {
                const float sg = s == 0 ? 1.0f : -1.0f;
                hand[s] = top + TL * (sg * 0.2f) + vib * 1.5f;
                hdir[s] = nrmO(F * 0.5f - U * 0.6f + TL * sg * 0.2f, -U);
                bend2[s] = 0.9f;
            }
            tool = T_JACK, toolA = tip, toolB = top, toolUp = F;
            break;
        }
        case K_PRY: {
            // die Spitze unter der Lage (T), der Stiel zum Arbeiter; ziehen = Stiel hinab
            const float a = 0.35f + 0.75f * smO(u / 0.7f);
            const vec3 dirB = nrmO(-F * std::cos(a) - U * std::sin(a), -F);
            const vec3 tip = T;
            hand[1] = tip + dirB * 0.95f;
            hand[0] = tip + dirB * 1.12f + TL * 0.04f;
            hdir[0] = hdir[1] = nrmO(-U + TL * 0.3f, -U);
            bend2[0] = bend2[1] = 0.7f;
            tool = T_BAR, toolA = tip, toolB = tip + dirB * 1.2f, toolUp = L;
            break;
        }
        case K_LIFT: {
            for (int s = 0; s < 2; ++s) {
                const float sg = s == 0 ? 1.0f : -1.0f;
                hand[s] = T + TL * (sg * 0.28f);
                hdir[s] = nrmO(F + U * 0.6f, F);
                bend2[s] = 0.5f;
            }
            break;
        }
        default: break;
    }
    // Haende glaetten (Aufgabenwechsel, Puppen)
    for (int s = 0; s < 2; ++s) {
        if (!g.handInit) g.hand[s] = hand[s], g.handV[s] = {};
        const float om = task == K_HAMMER || task == K_JACK ? 60.0f : (task == K_LAY ? 18.0f : 9.0f);
        sprV(g.hand[s], g.handV[s], hand[s], om, dt);
        if (task == K_HAMMER && s == 1) g.hand[s] = hand[s];  // der Schlag ist hart
    }
    g.handInit = true;
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const vec3 W2 = g.hand[s];
        // zwei Stufen: Schulter -> Handgelenk ueber den Ellbogen mit der wirksamen Laenge des geknickten Unterarms, dann das
        // zweite Gelenk gegen den Ellbogen (Zickzack)
        const float b2 = std::clamp(bend2[s], 0.15f, 1.6f);
        const float l2 = std::sqrt(lF1 * lF1 + lF2 * lF2 + 2.0f * lF1 * lF2 * std::cos(b2));
        const vec3 pole = nrmO(-TF * 0.6f + TL * (sg * 0.9f) - U * 0.2f, TL * sg);
        vec3 e1, wr;
        ik2(S[s], W2, lUp, l2, pole, e1, wr);
        vec3 e2, wr2;
        const vec3 pole2 = nrmO(TF * 0.8f - TL * (sg * 0.3f) + U * 0.2f, TF);
        ik2(e1, wr, lF1, lF2, pole2, e2, wr2);
        J[(size_t)armJ(s, 0)] = S[s];
        J[(size_t)armJ(s, 1)] = e1;
        J[(size_t)armJ(s, 2)] = e2;
        J[(size_t)armJ(s, 3)] = wr2;
        J[(size_t)armJ(s, 4)] = wr2 + nrmO(hdir[s], -U) * lHand;
    }
    // --- Beine (IK) ----------------------------------------------------------------------------------------
    const float lTh = length(B[legJ(0, 1)] - B[legJ(0, 0)]), lSh = length(B[legJ(0, 2)] - B[legJ(0, 1)]);
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const vec3 hip = P + Mb * (Rlow * (B[legJ(s, 0)] - B[PELVIS]));
        vec3 ank = foot[s] + U * B[legJ(s, 2)].z;
        if (g.crouch > 0.01f) {  // knien: rechts das Knie am Boden (Fuss nach hinten), links der Fuss vor
            const vec3 kneel = s == 1 ? foot[s] - F * 0.42f + U * 0.05f : foot[s] + F * 0.22f + U * B[legJ(s, 2)].z;
            ank = ank + (kneel - ank) * g.crouch;
        }
        vec3 knee, an2;
        ik2(hip, ank, lTh, lSh, nrmO(F + L * (sg * 0.08f), F), knee, an2);
        J[(size_t)legJ(s, 0)] = hip;
        J[(size_t)legJ(s, 1)] = knee;
        J[(size_t)legJ(s, 2)] = an2;
        const vec3 toe = an2 + nrmO(F + L * (sg * 0.05f), F) * 0.215f;
        J[(size_t)legJ(s, 3)] = vec3{toe.x, toe.y, std::max(toe.z - 0.06f, pos_.z + 0.02f)};
    }
    // --- Werkzeuge -----------------------------------------------------------------------------------------
    for (int t = 0; t < kTools; ++t) {
        if (t == tool) {
            J[(size_t)toolJ(t, 0)] = toolA, J[(size_t)toolJ(t, 1)] = toolB;
        } else {
            J[(size_t)toolJ(t, 0)] = J[(size_t)armJ(1, 3)], J[(size_t)toolJ(t, 1)] = J[(size_t)armJ(1, 3)] + U * 0.05f;
            hide(B_TOOL0 + t);
        }
    }
    // --- Rauch: steigt von Schultern, Scheitel, Ruecken auf, zieht beim Gehen nach; im Licht und beim Vergehen mehr ---------
    const vec3 wbase[kWisps] = {S[0] + U * 0.04f - TF * 0.03f, S[1] + U * 0.04f - TF * 0.03f, J[NECK] - TF * 0.05f + U * 0.03f, J[CHEST] - TF * 0.15f + U * 0.02f};
    for (int w = 0; w < kWisps; ++w) {
        const float fw = (float)w;
        const float len = (0.15f + 0.04f * std::sin(time_ * 0.9f + fw * 1.7f)) * (1.0f + 1.2f * g.light + 3.0f * g.dis);
        const float amp = 0.05f * (1.0f + 1.6f * g.light + 1.5f * g.dis);
        const float sp = 1.0f + 1.4f * g.light;
        const vec3 trail = -vel_ * 0.07f;
        vec3 p = wbase[w];
        J[(size_t)wispJ(w, 0)] = p;
        for (int i = 1; i < 3; ++i) {
            const float fi = (float)i;
            const vec3 drift = (F * std::sin(time_ * 1.3f * sp + fw * 2.1f + fi) + L * std::sin(time_ * 1.1f * sp + fw * 1.3f + fi * 1.9f)) * amp * fi;
            p = p + U * (len * 0.5f) + drift * 0.5f + trail * (0.5f * fi);
            J[(size_t)wispJ(w, i)] = p;
        }
    }
    // --- Funken (Schweissen, Lichtbogen an) ------------------------------------------------------------------
    const bool arc = task == K_WELD && in.shiver > 0.1f && dis < 0.05f;
    for (int q = 0; q < kSparks; ++q) {
        const float period = 0.42f + 0.07f * (float)q;
        const float tt = time_ / period + (float)q * 0.37f;
        const unsigned cyc = (unsigned)std::floor(tt);
        const float t = (tt - std::floor(tt)) * period;
        const float h1 = hashO(cyc * 7919u + (unsigned)q * 104729u), h2 = hashO(cyc * 3571u + (unsigned)q * 7907u + 11u);
        const vec3 tip = J[(size_t)toolJ(T_TORCH, 1)];
        const vec3 away = nrmO(tip - J[CHEST], F);
        vec3 v = rotO(nrmO(-away * 0.2f + F * 0.1f + U * 0.35f, U), U, (h1 - 0.5f) * 5.5f) * (1.4f + 1.6f * h2);
        v = v + vec3{(h2 - 0.5f) * 0.6f, (h1 - 0.5f) * 0.6f, 0.0f};
        vec3 p = tip + v * t + U * (-4.9f * t * t);
        vec3 vel = v + U * (-9.8f * t);
        if (p.z < pos_.z + 0.01f) {  // am Boden: springt flach weiter
            p.z = pos_.z + 0.01f;
            vel.z = 0.0f;
        }
        const vec3 dv = nrmO(vel, F);
        J[(size_t)sparkJ(q, 0)] = p - dv * 0.05f;
        J[(size_t)sparkJ(q, 1)] = p;
        if (!arc || t > period * 0.85f) hide(B_SPARK0 + q);
    }
    // --- Mauer und Geruest gehoeren nicht zu diesem Koerper -----------------------------------------------------
    park(COURSE0, JCOUNT, J[PELVIS]);
    for (int b = B_COURSE0; b <= B_TOWER; ++b) hide(b);
    autoUps();
    for (int t = 0; t < kTools; ++t)
        if (t == tool) pose_.up[(size_t)(B_TOOL0 + t)] = toolUp;
    // Kopf: Rolle mitnehmen
    pose_.up[B_HEAD] = Mb * (Rh * sk_->bindUp[B_HEAD]);
    // Rauchfaeden: nur im Lampenlicht (Lichtreaktion) und beim Vergehen
    for (int w = 0; w < kWisps; ++w)
        for (int i = 0; i < 2; ++i) pose_.s[(size_t)wispB(w, i)] = g.light > 0.03f ? std::max(0.001f, 0.35f + 0.75f * g.light) : 0.001f;
    // --- vergehen: von den Gliedern her in Rauch; der Rauch selbst waechst noch einmal ---------------------------
    if (g.dis > 0.001f) {
        auto fade = [&](int b, float delay) { pose_.s[(size_t)b] = std::max(0.001f, 1.0f - smO((g.dis - delay) / 0.45f)); };
        for (int s = 0; s < 2; ++s) {
            for (int i = 0; i < 4; ++i) fade(armB(s, i), 0.25f - 0.06f * (float)i);
            for (int i = 0; i < 3; ++i) fade(legB(s, i), 0.35f);
        }
        fade(B_HEAD, 0.4f), fade(B_NECK, 0.45f), fade(B_CHEST, 0.5f), fade(B_BELLY, 0.52f), fade(B_PELVIS, 0.55f);
        for (int t = 0; t < kTools; ++t) fade(B_TOOL0 + t, 0.1f);
        for (int w = 0; w < kWisps; ++w)
            for (int i = 0; i < 2; ++i) pose_.s[(size_t)wispB(w, i)] = std::max(0.001f, (1.0f + 1.5f * smO(g.dis * 2.0f)) * (1.0f - smO((g.dis - 0.75f) / 0.25f)));
    }
    headPos_ = (J[HEAD] + J[HEADTOP]) * 0.5f;
    headDir_ = Mb * (Rh * Fx);
}

}  // namespace lim::anim
