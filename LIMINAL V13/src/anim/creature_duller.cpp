// V13.3: Duller (Entity 6, Wikidot Rev. 52) - Skelett und Bewegung (KREATUR_Duller.md).
//
// Gestalt: gross (2,19 m), dunkelgrau, ausgemergelt, ohne Gesicht und Ohren; sehr lange Arme. Der Unterarm besteht aus
// sieben ineinander steckenden Gliedern (das aeussere ist der Unterarm): eingefahren haengen die Haende tief unter den
// Knien, ausgefahren reicht ein Arm gut 3 m weit - durch die Wand in den Nachbargang.
//
// Bewegung: wackliger, unnatuerlicher Gang (Becken schwankt weit, die Schritte ungleich, ab und zu knickt ein Knie ein,
// der Kopf pendelt nach); stehend wiegt er sich, der gesichtslose Kopf "lauscht" schief. Kratzen: beide Haende an der
// eigenen Wandseite, die Finger schaben. Greifen: der rechte Arm faehrt durch die Bruchstelle zum Ziel, Glied fuer Glied;
// die Hand schliesst sich (jaw). Flucht: weit vorgebeugt, lange Schritte, die Arme schleifen hinterher.
//
// Bruchstelle (gehoert zum Wesen, liegt auf der Wandseite des Opfers): Woelbung aus fuenf Brocken (bait 0..1), Risse;
// faehrt der Arm hindurch, fliegen die Brocken heraus und fallen zu Boden, der Rand mit dem Loch bleibt. shiver = 0: die
// Wand ist wieder heil (alles in der Wand verborgen).
//
// Eingaben (CreatureAnimator::Input): reach 0..1 wie weit der Arm ausfahren darf, reachAt Ziel; up Normale der Wand auf
// der Seite des Opfers (|up.z| < 0,5 = Wandbetrieb); hang = Abstand Wurzel -> Wandflaeche des Opfers / 4 m; bait
// Woelbung; shiver Bruchstelle sichtbar; jaw Griff; alert Kratzen; perceive Wahrnehmen; growl Flucht; crouch buecken.
#include "anim/creature_anim.hpp"

#include <algorithm>
#include <cmath>

#include "anim/skeleton.hpp"

namespace lim::anim {

namespace {
constexpr float kPi = 3.14159265f, kTau = 6.2831853f;
float sm01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
float wrapPi(float a) { return std::remainder(a, kTau); }
void spring(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
struct M3 {
    vec3 c0{1, 0, 0}, c1{0, 1, 0}, c2{0, 0, 1};
    vec3 operator*(vec3 v) const { return c0 * v.x + c1 * v.y + c2 * v.z; }
    M3 operator*(const M3& o) const { return M3{(*this) * o.c0, (*this) * o.c1, (*this) * o.c2}; }
};
M3 axisAngle(vec3 a, float ang) { return M3{rotateAround({1, 0, 0}, a, ang), rotateAround({0, 1, 0}, a, ang), rotateAround({0, 0, 1}, a, ang)}; }
vec3 safeNorm(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
float hash1(unsigned i) {
    i ^= i >> 16, i *= 0x7feb352du, i ^= i >> 15, i *= 0x846ca68bu, i ^= i >> 16;
    return (float)(i & 0xFFFFFF) / 16777216.0f;
}
// Kette von Punkten nach Bogenlaenge (der Arm folgt ihr)
struct Path {
    vec3 p[48];
    float s[48];
    int n = 0;
    void add(vec3 q) {
        if (n >= 48) return;
        p[n] = q;
        s[n] = n ? s[n - 1] + length(q - p[n - 1]) : 0.0f;
        ++n;
    }
    float len() const { return n ? s[n - 1] : 0.0f; }
    vec3 at(float t) const {  // ueber das Ende hinaus geradeaus weiter
        if (n < 2) return n ? p[0] : vec3{};
        if (t <= 0.0f) return p[0] + safeNorm(p[1] - p[0], {1, 0, 0}) * t;
        for (int i = 1; i < n; ++i)
            if (t <= s[i]) {
                const float u = (t - s[i - 1]) / std::max(1e-6f, s[i] - s[i - 1]);
                return p[i - 1] + (p[i] - p[i - 1]) * u;
            }
        return p[n - 1] + safeNorm(p[n - 1] - p[n - 2], {1, 0, 0}) * (t - s[n - 1]);
    }
};
// Belegung von xs_
constexpr int X_INIT = 0, X_EXT = 1, X_EXTV = 2, X_GRIP = 3, X_GRIPV = 4, X_BULGE = 5, X_BULGEV = 6, X_STUMBLE = 7, X_STUMBLEV = 8,
              X_STUMBLET = 9, X_SCRAPE = 10, X_SCRAPEV = 11, X_FLEE = 12, X_FLEEV = 13, X_BURST = 14, X_RIM = 15, X_SWAY = 16,
              X_SWAYV = 17, X_ARMPH = 18, X_ARMPHV = 19, X_CHUNK = 24, X_CHUNKN = 16, X_SIZE = X_CHUNK + du::kChunks * X_CHUNKN;
// Brocken: Lage (0), Geschwindigkeit (3), Achse x (6), Achse z (9), Drehung (12), Zustand (15: 0 in der Wand, 1 fliegt, 2 liegt)
}  // namespace

// --- Skelett ------------------------------------------------------------------------------------------------------
float du::extendedLen() {
    float L = 0.0f;
    for (int k = 0; k < kSeg; ++k) L += segLen(k);
    return L - kSegOverlap * (float)(kSeg - 1);
}

CreatureSkeleton du::skeleton() {
    CreatureSkeleton s;
    s.kind = Entity::Duller;
    s.bind.assign((size_t)JCOUNT, vec3{});
    s.bones.assign((size_t)BCOUNT, CBone{});
    s.bindUp.assign((size_t)BCOUNT, vec3{0, 0, 1});
    auto J = [&](int i, vec3 p) { s.bind[(size_t)i] = p; };
    auto B = [&](int i, int a, int b, int parent, float r, vec3 up, bool shadow) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)b, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        s.bindUp[(size_t)i] = up;
    };
    const vec3 F{1, 0, 0}, Z{0, 0, 1};
    J(ROOT, {0, 0, 0});
    J(PELVIS, {0, 0, kHip});
    J(SPINE, {0.01f, 0, kWaist});
    J(CHEST, {0.02f, 0, kChest});
    J(NECK, {0.0f, 0, kShoulder + 0.03f});
    J(HEAD, {0.02f, 0, kNeckTop});
    J(HEADTOP, {0.03f, 0, kHeadTop});
    // Fingerlaengen (Grundglied, Endglied): Daumen, Zeige-, Mittel-, Ring-, kleiner Finger - lang und duenn
    static const float fl[kFing][2] = {{0.055f, 0.05f}, {0.088f, 0.078f}, {0.098f, 0.088f}, {0.088f, 0.078f}, {0.068f, 0.058f}};
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 S{0.0f, sg * kShoulderW, kShoulder};
        const vec3 d{0.0f, sg * std::sin(kArmOut), -std::cos(kArmOut)};
        const vec3 E = S + d * kUpper;
        J(sh(sd), S);
        J(el(sd), E);
        float st = 0.0f;
        for (int k = 0; k < kSeg; ++k) {
            J(segS(sd, k), E + d * st);
            J(segE(sd, k), E + d * (st + segLen(k)));
            st += segLen(k) - kSegOverlap;
        }
        const vec3 W = s.bind[(size_t)segE(sd, kSeg - 1)];
        J(kn(sd), W + d * kHand);
        // Finger in der Ruhepose gestreckt, quer zur Hand (x) gefaechert; der Daumen setzt am Handgelenk an
        for (int f = 0; f < kFing; ++f) {
            vec3 base, dir = d;
            if (f == 0) {
                base = W + d * 0.035f + F * 0.032f;
                dir = normalize(d + F * 0.75f);
            } else {
                base = s.bind[(size_t)kn(sd)] + F * (0.033f - 0.022f * (float)(f - 1));
            }
            J(fin(sd, f, 0), base);
            J(fin(sd, f, 1), base + dir * fl[f][0]);
            J(fin(sd, f, 2), base + dir * (fl[f][0] + fl[f][1]));
        }
        const vec3 H{0.0f, sg * kHipW, kHip - 0.05f};
        J(leg(sd, 0), H);
        J(leg(sd, 1), {0.015f, sg * kHipW, H.z - kThigh});
        J(leg(sd, 2), {0.0f, sg * kHipW, kAnkle});
        J(leg(sd, 3), {kFoot - 0.05f, sg * (kHipW + 0.01f), 0.02f});
    }
    // Bruchstelle: vor dem Duller in der Ebene x = kBreachX, Normale +x
    const vec3 Bc{kBreachX, 0.0f, kBreachZ};
    J(CRACK_A, Bc), J(CRACK_B, Bc + F * 0.05f), J(RIM_A, Bc), J(RIM_B, Bc + F * 0.05f);
    for (int k = 0; k < kChunks; ++k) {
        const float th = ((float)k + 0.5f) * kTau / (float)kChunks;
        const vec3 c = Bc + vec3{0.0f, 0.1f * std::cos(th), 0.1f * std::sin(th)};
        J(CHUNK0 + k * 2, c), J(CHUNK0 + k * 2 + 1, c + F * 0.03f);
    }
    B(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    B(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.12f, F, true);
    B(B_SPINE, SPINE, CHEST, B_PELVIS, 0.11f, F, true);
    B(B_CHEST, CHEST, NECK, B_SPINE, 0.13f, F, true);
    B(B_NECK, NECK, HEAD, B_CHEST, 0.04f, F, true);
    B(B_HEAD, HEAD, HEADTOP, B_NECK, 0.085f, F, true);
    for (int sd = 0; sd < 2; ++sd) {
        B(bClav(sd), CHEST, sh(sd), B_CHEST, 0.04f, Z, false);
        B(bUpper(sd), sh(sd), el(sd), bClav(sd), 0.035f, F, true);
        for (int k = 0; k < kSeg; ++k) B(bSeg(sd, k), segS(sd, k), segE(sd, k), k == 0 ? bUpper(sd) : bSeg(sd, k - 1), 0.03f - 0.002f * (float)k, F, k == 0);
        B(bHand(sd), segE(sd, kSeg - 1), kn(sd), bSeg(sd, kSeg - 1), 0.03f, F, false);
        for (int f = 0; f < kFing; ++f) {
            B(bFin(sd, f, 0), fin(sd, f, 0), fin(sd, f, 1), bHand(sd), 0.009f, F, false);
            B(bFin(sd, f, 1), fin(sd, f, 1), fin(sd, f, 2), bFin(sd, f, 0), 0.007f, F, false);
        }
        B(bLeg(sd, 0), leg(sd, 0), leg(sd, 1), B_PELVIS, 0.055f, F, true);
        B(bLeg(sd, 1), leg(sd, 1), leg(sd, 2), bLeg(sd, 0), 0.045f, F, true);
        B(bLeg(sd, 2), leg(sd, 2), leg(sd, 3), bLeg(sd, 1), 0.035f, Z, false);
    }
    B(B_CRACK, CRACK_A, CRACK_B, B_ROOT, 0.0f, Z, false);
    B(B_RIM, RIM_A, RIM_B, B_ROOT, 0.0f, Z, false);
    for (int k = 0; k < kChunks; ++k) B(B_CHUNK0 + k, CHUNK0 + k * 2, CHUNK0 + k * 2 + 1, B_ROOT, 0.0f, Z, false);
    s.height = kHeadTop;
    s.reach = 3.0f;
    s.center = {0.0f, 0.0f, 1.1f};
    return s;
}

// --- Bewegung -----------------------------------------------------------------------------------------------------
void CreatureAnimator::updateDuller(float dt, const Input& in) {
    using namespace du;
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    if ((int)xs_.size() != X_SIZE) xs_.assign((size_t)X_SIZE, 0.0f);
    float* X = xs_.data();
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const M3 Mb{F, Lw, U};
    if (!chainInit_) {  // eingesetzt oder versetzt: Fuesse an ihre Plaetze, Brocken in die Wand
        for (int i = 0; i < 2; ++i) {
            Leg& l = leg_[i];
            l.plant = l.from = l.to = pos_ + F * B[(size_t)leg(i, 2)].x + Lw * B[(size_t)leg(i, 2)].y;
            l.swing = false, l.t = 0.0f;
        }
        for (int k = 0; k < kChunks; ++k) X[X_CHUNK + k * X_CHUNKN + 15] = 0.0f;
        X[X_BURST] = 0.0f;
    }
    // Wandbetrieb: Bruchstelle (Mitte auf der Wandflaeche des Opfers), Normale zum Opfer
    const bool wall = in.hang > 0.02f && std::fabs(in.up.z) < 0.5f;
    const vec3 n = wall ? safeNorm(vec3{in.up.x, in.up.y, 0.0f}, F) : F;
    const vec3 t1 = safeNorm(cross(U, n), Lw);
    const float D = in.hang * 4.0f;
    const vec3 breach = pos_ + n * D + U * kBreachH;
    const vec3 nearFace = pos_ + n * 0.42f;  // die eigene Wandseite (die Praesenz stellt ihn 0,42 m davor)

    spring(X[X_EXT], X[X_EXTV], std::clamp(in.reach, 0.0f, 1.0f), in.reach > X[X_EXT] ? 7.0f : 4.0f, dt);  // schiesst heraus
    X[X_EXT] = std::clamp(X[X_EXT], 0.0f, 1.0f);
    spring(X[X_GRIP], X[X_GRIPV], in.jaw, 12.0f, dt);
    spring(X[X_BULGE], X[X_BULGEV], in.bait, 3.0f, dt);
    spring(X[X_SCRAPE], X[X_SCRAPEV], in.alert, 4.0f, dt);
    spring(X[X_FLEE], X[X_FLEEV], in.growl, 3.0f, dt);
    spring(perc_, percV_, in.perceive, 7.0f, dt);  // ruckartig: der Kopf schnellt herum
    spring(tilt_, tiltV_, in.tilt, 3.0f, dt);
    spring(crouch_, crouchV_, std::max(in.crouch, sm01((kHeadTop + 0.08f - in.clearance) / 0.35f)), 3.0f, dt);  // niedrige Tuer: buecken
    spring(idleW_, idleWV_, speed_ > 0.07f ? 0.0f : 1.0f, 2.5f, dt);
    const float ext = X[X_EXT], grip = X[X_GRIP], scrape = X[X_SCRAPE], flee = X[X_FLEE];

    // --- Gang: ungleiche Schritte, wankend; ab 2,4 m/s Rennen (Flucht) --------------------------------------------------
    if (!run_ && speed_ > 2.4f) run_ = true;
    else if (run_ && speed_ < 1.8f) run_ = false;
    static const float offs[2] = {0.0f, 0.5f};
    const float duty = run_ ? 0.34f : 0.6f;
    const float legL = kThigh + kShin;
    // Schrittlaenge schwankt von Schritt zu Schritt (wackliger, unnatuerlicher Gang)
    const float irregular = 0.82f + 0.36f * hash1((unsigned)std::floor(time_ * 1.7f) * 7919u + 13u);
    const float S = legL * irregular * (run_ ? 1.65f + 0.3f * speed_ : 0.8f + 0.42f * speed_);
    vec3 rest[2];
    for (int sd = 0; sd < 2; ++sd) {
        // die Fuesse kreuzen beim Gehen fast ueber (schmale, unsichere Spur), im Stand weit
        const float wide = B[(size_t)leg(sd, 2)].y * (speed_ > 0.07f ? 0.45f : 1.25f);
        rest[sd] = pos_ + F * B[(size_t)leg(sd, 2)].x + Lw * wide;
    }
    const float wgt[2] = {0.5f, 0.5f};
    stepLegs(dt, 2, offs, duty, S, rest, 0.45f, wgt);
    const float lift[2] = {run_ ? 0.16f : 0.09f, run_ ? 0.16f : 0.07f};
    vec3 g[2];
    for (int i = 0; i < 2; ++i) g[i] = footPos(i, lift);
    const bool moving = speed_ > 0.07f;
    const float mv = sm01(speed_ / 0.6f);
    const float c1 = std::cos(kTau * phase_);
    // Einknicken: beim Gehen ab und zu ein Knie, das nachgibt (Becken sackt, Rumpf ruckt zur Seite)
    X[X_STUMBLET] -= dt;
    if (X[X_STUMBLET] <= 0.0f) {
        X[X_STUMBLET] = 1.4f + 2.8f * rnd();
        if (moving && !run_) X[X_STUMBLEV] -= 0.55f + 0.5f * rnd();
    }
    spring(X[X_STUMBLE], X[X_STUMBLEV], 0.0f, 6.5f, dt);
    const float stumble = std::clamp(X[X_STUMBLE], -0.4f, 0.4f);
    // Wiegen im Stand (langsam, nicht gleichmaessig) und Schwanken im Gehen
    swayPh_ += dt * (0.37f + 0.12f * std::sin(time_ * 0.21f));
    const float sway = std::sin(swayPh_) + 0.4f * std::sin(swayPh_ * 2.3f + 1.0f);

    // --- Rumpf --------------------------------------------------------------------------------------------------------
    float bend = 0.3f + 0.25f * mv + 0.45f * flee + 0.35f * crouch_ + 0.18f * scrape - 0.12f * perc_;
    bend += 0.15f * ext;  // greift: er lehnt sich in die Wand
    const float roll = -0.07f * c1 * mv * (run_ ? 0.6f : 1.0f) + 0.035f * sway * idleW_ + 0.5f * stumble * (moving ? 1.0f : 0.0f);
    vec3 P = pos_ + Lw * (0.06f * mv * std::cos(kTau * (phase_ - 0.25f)) * (run_ ? 0.3f : 1.0f) + 0.025f * sway * idleW_);
    P += F * (0.12f * scrape + 0.1f * ext);  // zur Wand
    float bob = run_ ? -0.04f * std::cos(2.0f * kTau * (phase_ - 0.15f)) : -0.025f * mv * std::cos(2.0f * kTau * phase_);
    P.z = pos_.z + B[PELVIS].z - 0.05f + bob + 0.09f * stumble - 0.14f * crouch_ - 0.06f * flee - 0.03f * scrape;
    const float py = -0.12f * c1 * mv;
    const M3 Rpel = axisAngle(U, py) * axisAngle({0, 1, 0}, bend * 0.25f) * axisAngle({1, 0, 0}, roll);
    // Fuesse: im Stand flach, im Schwung abrollend
    vec3 ankT[2], footV[2];
    for (int sd = 0; sd < 2; ++sd) {
        const Leg& L = leg_[sd];
        const vec3 fd0 = Mb * (B[(size_t)leg(sd, 3)] - B[(size_t)leg(sd, 2)]);
        float pitch = 0.0f;
        if (L.swing) pitch = 0.5f * std::sin(kPi * L.t) * (1.0f - L.t) - 0.25f * sm01((L.t - 0.6f) / 0.4f);
        else if (moving) pitch = std::clamp((-dot(g[sd] - P, F) - 0.12f) * 1.6f, 0.0f, 0.8f);
        footV[sd] = rotateAround(fd0, Lw, pitch);
        ankT[sd] = g[sd] + U * kAnkle + (L.swing ? vec3{} : fd0 - footV[sd]);
        const float toeZ = ankT[sd].z + footV[sd].z;
        if (toeZ < pos_.z + 0.01f) ankT[sd].z += pos_.z + 0.01f - toeZ;
    }
    {  // Beine muessen den Boden erreichen
        float drop = 0.0f;
        for (int sd = 0; sd < 2; ++sd) {
            const vec3 H = P + Mb * (Rpel * (B[(size_t)leg(sd, 0)] - B[PELVIS]));
            const float h = std::hypot(ankT[sd].x - H.x, ankT[sd].y - H.y), lmax = legL * 0.985f;
            drop = std::max(drop, (H.z - ankT[sd].z) - std::sqrt(std::max(lmax * lmax - h * h, 0.01f)));
        }
        P.z -= std::min(drop, 0.12f + 0.15f * crouch_);
    }
    const float tw = 0.1f * c1 * mv;
    const M3 Rsp = Rpel * axisAngle(U, -py * 0.6f + tw * 0.3f) * axisAngle({0, 1, 0}, bend * 0.35f) * axisAngle({1, 0, 0}, roll * 0.4f);
    const float breath = std::sin(time_ * kTau * 0.21f) + 0.3f * std::sin(time_ * kTau * 0.53f);
    const M3 Rch = Rsp * axisAngle(U, -py * 0.5f + tw * 0.7f) * axisAngle({0, 1, 0}, bend * 0.4f - 0.01f * breath) * axisAngle({1, 0, 0}, roll * 0.3f);
    J[ROOT] = pos_;
    J[PELVIS] = P;
    J[SPINE] = P + Mb * (Rpel * (B[SPINE] - B[PELVIS]));
    J[CHEST] = J[SPINE] + Mb * (Rsp * (B[CHEST] - B[SPINE]));
    J[NECK] = J[CHEST] + Mb * (Rch * (B[NECK] - B[CHEST])) + U * (0.004f * breath);
    // Hals: der Kopf haengt vor (gebeugte, lauernde Haltung)
    const M3 Rnk = Rch * axisAngle({0, 1, 0}, 0.42f + 0.25f * flee - 0.2f * perc_);
    J[HEAD] = J[NECK] + Mb * (Rnk * (B[HEAD] - B[NECK]));
    // Kopf: ohne Gesicht - er "sieht" trotzdem hin; lauschend legt er ihn schief an die Wand
    float wy = 0.0f, wp = -0.25f;
    auto yawPitch = [&](vec3 a, vec3 b, float& y, float& p) {
        const vec3 d = b - a;
        y = std::atan2(dot(d, Lw), dot(d, F));
        p = std::atan2(d.z, std::hypot(dot(d, F), dot(d, Lw)));
    };
    if (in.look) yawPitch(J[HEAD], in.lookAt, wy, wp);
    else if (idleW_ > 0.01f) wy = 0.5f * std::sin(time_ * 0.27f) * idleW_, wp = -0.25f + 0.1f * std::sin(time_ * 0.17f + 1.0f);
    if (perc_ > 0.01f) {
        float y2, p2;
        yawPitch(J[HEAD], in.perceiveAt, y2, p2);
        wy += wrapPi(y2 - wy) * perc_;
        wp += (p2 - wp) * perc_;
    }
    if (scrape > 0.01f) wy += (0.0f - wy) * scrape, wp += (-0.1f - wp) * scrape;
    wy = std::clamp(wrapPi(wy), -1.4f, 1.4f);
    wp = std::clamp(wp, -1.0f, 0.7f);
    if (!chainInit_) headYaw_ = wy, headPitch_ = wp, headYawV_ = headPitchV_ = 0.0f;
    const float hw = perc_ > 0.3f ? 16.0f : (flee > 0.3f ? 6.0f : 2.6f);
    spring(headYaw_, headYawV_, wy, hw, dt);
    spring(headPitch_, headPitchV_, wp, hw, dt);
    // nachpendeln beim Gehen (der Kopf sitzt locker), ruckartiges Zucken
    twitchT_ -= dt;
    if (twitchT_ <= 0.0f) {
        twitchT_ = 1.2f + 3.5f * rnd();
        headYawV_ += (rnd() - 0.5f) * 9.0f, headPitchV_ += (rnd() - 0.5f) * 4.0f;
    }
    const float loll = 0.16f * c1 * mv + 0.25f * stumble;
    const float tiltAll = tilt_ + 0.3f * scrape + loll + 0.12f * std::sin(time_ * 0.31f) * idleW_;
    const M3 Rh = axisAngle(U, headYaw_) * axisAngle({0, 1, 0}, -headPitch_) * axisAngle({1, 0, 0}, tiltAll);
    J[HEADTOP] = J[HEAD] + Mb * (Rh * (B[HEADTOP] - B[HEAD]));
    headPos_ = (J[HEAD] + J[HEADTOP]) * 0.5f;
    headDir_ = Mb * (Rh * vec3{1, 0, 0});

    // --- Arme ---------------------------------------------------------------------------------------------------------
    static const float fl[kFing][2] = {{0.055f, 0.05f}, {0.088f, 0.078f}, {0.098f, 0.088f}, {0.088f, 0.078f}, {0.068f, 0.058f}};
    const float L0 = segLen(0), Lmax = extendedLen();
    spring(X[X_ARMPH], X[X_ARMPHV], (float)(moving ? 1 : 0), 2.0f, dt);
    vec3 handC = headPos_;
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 S0 = J[CHEST] + Mb * (Rch * (B[(size_t)sh(sd)] - B[CHEST])) + U * (0.006f * breath);
        // 1) gewoehnliche Haltung: die langen Arme haengen locker, pendeln gegengleich, im Lauf schleifen sie hinterher
        float phi = -sg * 0.32f * c1 * mv * (run_ ? 0.4f : 1.0f) + 0.05f * std::sin(time_ * 0.9f + (float)sd * 2.0f) * idleW_;
        float out = 0.12f + 0.04f * sway * sg;
        float elb = 0.25f + 0.2f * mv;
        if (run_ || flee > 0.2f) {  // Flucht: die Arme schleifen nach hinten, fast gestreckt
            const float k = std::max(run_ ? 1.0f : 0.0f, flee);
            phi += (-0.95f + 0.15f * std::sin(time_ * 8.0f + (float)sd * 2.4f) - phi) * k;
            out += (0.25f - out) * k;
            elb += (0.12f - elb) * k;
        }
        const vec3 du0 = Mb * normalize(vec3{std::sin(phi) * std::cos(out), sg * std::sin(out), -std::cos(phi) * std::cos(out)});
        const vec3 bax = safeNorm(cross(du0, F), -Lw);
        vec3 E = S0 + du0 * kUpper;
        vec3 fdir = rotateAround(du0, bax, elb);
        vec3 pole = safeNorm(-F + Lw * (sg * 0.3f), -F);
        // Kratzen: beide Haende an der eigenen Wandseite, die Finger schaben (rechts auf Hoehe der Bruchstelle)
        if (scrape > 0.01f && wall) {
            const vec3 hand = nearFace + t1 * (sg * 0.24f) + U * (sd == 1 ? kBreachH - 0.05f : kBreachH + 0.3f) +
                              U * (0.03f * std::sin(time_ * 5.0f + (float)sd)) + t1 * (0.02f * std::sin(time_ * 3.3f + 2.0f * (float)sd));
            const vec3 wristT = hand - n * 0.02f - U * 0.05f;
            vec3 e, w;
            ik2(S0, wristT, kUpper, L0, safeNorm(-U - n * 0.3f + t1 * (sg * 0.5f), -U), e, w);
            const vec3 fd2 = safeNorm(w - e, fdir);
            E = E + (e - E) * scrape;
            fdir = safeNorm(fdir + (fd2 - fdir) * scrape, fd2);
        }
        // 2) ausgefahren (rechter Arm): Oberarm zur Bruchstelle, die Glieder gleiten heraus und folgen dem Weg zum Ziel
        const bool reaching = sd == 1 && ext > 0.002f;
        const float w = reaching ? sm01(ext * 5.0f) : 0.0f;
        Path path;
        float Lf = L0;
        if (reaching) {
            const vec3 goal = wall ? breach : in.reachAt;
            const vec3 ud = safeNorm(goal - S0, F);
            const vec3 E2 = S0 + ud * kUpper;
            E = E + (E2 - E) * w;
            path.add(E);
            vec3 T = in.reachAt;
            if (wall) {
                path.add(breach - n * 0.05f);
                path.add(breach + n * 0.02f);
                if (dot(T - breach, n) < 0.25f) T += n * (0.25f - dot(T - breach, n));  // nie hinter die Wand
                const vec3 c = breach + n * 0.45f;
                const vec3 prev = breach + n * 0.02f;
                for (int i = 1; i <= 14; ++i) {  // Bogen von der Wand zum Ziel
                    const float u = (float)i / 14.0f;
                    path.add(prev * ((1 - u) * (1 - u)) + c * (2 * u * (1 - u)) + T * (u * u));
                }
            } else {
                path.add(T);
            }
            // so lang, dass das Handgelenk etwas vor dem Ziel liegt; kann es das Ziel nicht erreichen, tastet es
            const float need = std::max(L0, path.len() - 0.16f);
            const float allow = L0 + (Lmax - L0) * ext;
            Lf = std::min(need, allow);
            if (allow < need - 0.05f) {  // zu kurz: die Hand tastet suchend hin und her
                const float sw = std::sin(time_ * 3.1f) * 0.18f + std::sin(time_ * 7.3f) * 0.05f;
                for (int i = 2; i < path.n; ++i) path.p[i] += t1 * (sw * ((float)i / (float)path.n)) + U * (0.08f * std::sin(time_ * 2.3f));
            }
        } else {
            path.add(E);
            path.add(E + fdir * 3.5f);
        }
        const float f = std::clamp((Lf - L0) / std::max(1e-3f, Lmax - L0), 0.0f, 1.0f);
        const vec3 baseUp = safeNorm(cross(bax, fdir), F);
        float st = 0.0f;
        for (int k = 0; k < kSeg; ++k) {
            const float sRet = L0 - segLen(k), sExt = st;
            const float s0 = sRet + (sExt - sRet) * f;
            st += segLen(k) - kSegOverlap;
            J[(size_t)segS(sd, k)] = path.at(s0);
            J[(size_t)segE(sd, k)] = path.at(s0 + segLen(k));
        }
        J[(size_t)sh(sd)] = S0;
        J[(size_t)el(sd)] = E;
        // Hand und Finger: haengend (Finger leicht gekruemmt), kratzend (Finger schaben), greifend (offen -> geschlossen)
        const vec3 W = J[(size_t)segE(sd, kSeg - 1)];
        const vec3 tan = safeNorm(W - J[(size_t)segS(sd, kSeg - 1)], fdir);
        vec3 hdir = tan;
        if (scrape > 0.01f && wall && !reaching) hdir = safeNorm(hdir + (U * 0.7f + n - hdir) * scrape, hdir);
        J[(size_t)kn(sd)] = W + hdir * kHand;
        const vec3 across = safeNorm(cross(hdir, sd == 1 && reaching ? U : bax), F);
        const vec3 palm = safeNorm(cross(across, hdir), U);  // Handflaeche (Kruemmung der Finger)
        float curl = 0.35f + 0.1f * std::sin(time_ * 0.8f + (float)sd);
        if (sd == 1 && reaching) curl = 0.05f + 1.25f * grip;                       // offen, dann zu
        if (scrape > 0.01f && !reaching) curl += (0.6f + 0.45f * std::sin(time_ * 13.0f + (float)sd * 1.7f) - curl) * scrape;  // schaben
        for (int fi = 0; fi < kFing; ++fi) {
            const vec3 base = fi == 0 ? W + hdir * 0.035f + across * 0.032f : J[(size_t)kn(sd)] + across * (0.033f - 0.022f * (float)(fi - 1));
            vec3 d0 = fi == 0 ? safeNorm(hdir + across * 0.75f, hdir) : safeNorm(hdir + across * (0.06f * (2.0f - (float)fi)), hdir);
            if (sd == 1 && reaching) d0 = safeNorm(d0 + across * (0.25f * (2.0f - (float)fi)) * (1.0f - grip), d0);  // gespreizt
            const float c = curl * (fi == 0 ? 0.6f : 1.0f) * (0.85f + 0.3f * hash1((unsigned)(fi * 3 + sd)));
            const vec3 ax = safeNorm(cross(d0, palm), across);
            const vec3 d1 = rotateAround(d0, ax, -c * 0.8f);
            const vec3 d2 = rotateAround(d1, ax, -c * 1.0f);
            J[(size_t)fin(sd, fi, 0)] = base;
            J[(size_t)fin(sd, fi, 1)] = base + d1 * fl[fi][0];
            J[(size_t)fin(sd, fi, 2)] = base + d1 * fl[fi][0] + d2 * fl[fi][1];
        }
        if (sd == 1 && ext > 0.3f) handC = J[(size_t)kn(sd)] + hdir * 0.06f;
        (void)baseUp;
    }
    // --- Beine --------------------------------------------------------------------------------------------------------
    for (int sd = 0; sd < 2; ++sd) {
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 H = P + Mb * (Rpel * (B[(size_t)leg(sd, 0)] - B[PELVIS]));
        vec3 knee, ank;
        // Knie leicht nach aussen (o-beinig, unsicher)
        ik2(H, ankT[sd], kThigh, kShin, safeNorm(F + Lw * (sg * 0.25f), F), knee, ank);
        J[(size_t)leg(sd, 0)] = H;
        J[(size_t)leg(sd, 1)] = knee;
        J[(size_t)leg(sd, 2)] = ank;
        J[(size_t)leg(sd, 3)] = ank + footV[sd];
    }
    // --- Bruchstelle ----------------------------------------------------------------------------------------------------
    const bool shown = in.shiver > 0.5f && wall;
    const float bulge = std::clamp(X[X_BULGE], 0.0f, 1.0f);
    const vec3 Bb{kBreachX, 0.0f, kBreachZ};  // Mitte in der Ruhepose
    auto toWall = [&](vec3 bindP, float depth) {  // Ruhepose (Ebene x = kBreachX) -> Wandflaeche
        const vec3 r = bindP - Bb;
        return breach + n * (r.x + depth) + t1 * r.y + U * r.z;
    };
    const float crackD = shown ? -0.04f + 0.0415f * sm01((bulge - 0.2f) / 0.25f) : -0.3f;
    J[CRACK_A] = toWall(B[CRACK_A], crackD), J[CRACK_B] = toWall(B[CRACK_B], crackD);
    // Brocken: in der Wand (Woelbung), fliegend, liegend
    bool burst = X[X_BURST] > 0.5f;
    if (!shown) {
        burst = false, X[X_BURST] = 0.0f;
        for (int k = 0; k < kChunks; ++k) X[X_CHUNK + k * X_CHUNKN + 15] = 0.0f;
    } else if (!burst && ext > 0.03f && bulge > 0.6f) {  // der Arm bricht durch
        burst = true, X[X_BURST] = 1.0f;
        for (int k = 0; k < kChunks; ++k) {
            float* c = X + X_CHUNK + k * X_CHUNKN;
            const vec3 p = toWall(B[(size_t)(CHUNK0 + k * 2)], 0.0f);
            const vec3 rad = safeNorm(p - breach, U);
            const vec3 v = n * (1.6f + 1.3f * hash1(k * 11u + 1u)) + rad * (0.5f + 0.6f * hash1(k * 11u + 2u)) + U * (0.2f + 0.6f * hash1(k * 11u + 3u));
            const vec3 wv = vec3{hash1(k * 11u + 4u) - 0.5f, hash1(k * 11u + 5u) - 0.5f, hash1(k * 11u + 6u) - 0.5f} * 16.0f;
            c[0] = p.x, c[1] = p.y, c[2] = p.z, c[3] = v.x, c[4] = v.y, c[5] = v.z;
            c[6] = n.x, c[7] = n.y, c[8] = n.z, c[9] = 0.0f, c[10] = 0.0f, c[11] = 1.0f;
            c[12] = wv.x, c[13] = wv.y, c[14] = wv.z, c[15] = 1.0f;
        }
    }
    const float rimD = shown && burst ? 0.0f : -0.3f;
    J[RIM_A] = toWall(B[RIM_A], rimD), J[RIM_B] = toWall(B[RIM_B], rimD);
    pose_.up[B_CRACK] = U, pose_.up[B_RIM] = U;
    const float floorZ = pos_.z;
    for (int k = 0; k < kChunks; ++k) {
        float* c = X + X_CHUNK + k * X_CHUNKN;
        const int ja = CHUNK0 + k * 2, jb = ja + 1;
        if (c[15] < 0.5f) {  // in der Wand: die Woelbung waechst, zittert beim Kratzen
            const float jit = scrape * 0.004f * std::sin(time_ * 31.0f + (float)k * 2.1f);
            const float dep = -0.14f + 0.14f * sm01(bulge) + (bulge > 0.05f ? jit : 0.0f);
            J[(size_t)ja] = toWall(B[(size_t)ja], shown ? dep : -0.3f);
            J[(size_t)jb] = toWall(B[(size_t)jb], shown ? dep : -0.3f);
            pose_.up[(size_t)(B_CHUNK0 + k)] = U;
            continue;
        }
        vec3 p{c[0], c[1], c[2]}, v{c[3], c[4], c[5]}, ax{c[6], c[7], c[8]}, az{c[9], c[10], c[11]}, wv{c[12], c[13], c[14]};
        if (c[15] < 1.5f) {  // fliegt
            v.z -= 9.8f * dt;
            p += v * dt;
            const float wl = length(wv);
            if (wl > 1e-4f) ax = rotateAround(ax, wv / wl, wl * dt), az = rotateAround(az, wv / wl, wl * dt);
            if (dot(p - breach, n) > 1.7f) v -= n * std::max(0.0f, dot(v, n)) * 1.6f;  // die Wand gegenueber
            if (p.z < floorZ + 0.035f) {
                p.z = floorZ + 0.035f;
                v.z = -0.22f * v.z, v.x *= 0.45f, v.y *= 0.45f, wv = wv * 0.4f;
                if (length(v) < 0.35f) c[15] = 2.0f;
            }
        } else {  // liegt: dreht sich flach auf den Boden
            const vec3 want = ax.z >= 0.0f ? U : -U;
            ax = safeNorm(ax + (want - ax) * std::min(1.0f, dt * 6.0f), want);
            az = safeNorm(az - ax * dot(az, ax), F);
        }
        ax = safeNorm(ax, n);
        az = safeNorm(az - ax * dot(az, ax), U);
        c[0] = p.x, c[1] = p.y, c[2] = p.z, c[3] = v.x, c[4] = v.y, c[5] = v.z;
        c[6] = ax.x, c[7] = ax.y, c[8] = ax.z, c[9] = az.x, c[10] = az.y, c[11] = az.z, c[12] = wv.x, c[13] = wv.y, c[14] = wv.z;
        J[(size_t)ja] = p;
        J[(size_t)jb] = p + ax * 0.03f;
        pose_.up[(size_t)(B_CHUNK0 + k)] = az;
    }
    // Kopf fuer den Schreck: mit ausgestrecktem Arm die Hand
    if (ext > 0.3f) headPos_ = handC;
    // Oben-Richtungen: Rumpf, Kopf aus den Drehungen; Glieder: autoUps (Brocken und Bruchstelle oben gesetzt)
    std::vector<vec3> keep(pose_.up.begin() + B_CRACK, pose_.up.end());
    autoUps();
    std::copy(keep.begin(), keep.end(), pose_.up.begin() + B_CRACK);
    pose_.up[du::B_PELVIS] = Mb * (Rpel * sk_->bindUp[du::B_PELVIS]);
    pose_.up[du::B_SPINE] = Mb * (Rsp * sk_->bindUp[du::B_SPINE]);
    pose_.up[du::B_CHEST] = Mb * (Rch * sk_->bindUp[du::B_CHEST]);
    pose_.up[du::B_NECK] = Mb * (Rnk * sk_->bindUp[du::B_NECK]);
    pose_.up[du::B_HEAD] = Mb * (Rh * sk_->bindUp[du::B_HEAD]);
    for (int sd = 0; sd < 2; ++sd) pose_.up[(size_t)bClav(sd)] = Mb * (Rch * sk_->bindUp[(size_t)bClav(sd)]);
    (void)lift;
}

}  // namespace lim::anim
