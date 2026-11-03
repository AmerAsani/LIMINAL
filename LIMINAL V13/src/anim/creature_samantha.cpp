// V13.3 Teil 4 Level 5: Samantha (Wikidot entity-26 Rev. 22, level-5 Rev. 184) - Skelett und Bewegung (KREATUR_Samantha.md).
//
// Eine kleine Hauskatze (fast ein Kaetzchen), schwarz-weiss, gruene Augen - aber niemand haelt sie fuer eine gewoehnliche
// Katze. Bewegung wie eine Katze, mit Bruechen, die das Wiki traegt (sie spricht, ihr Kiefer zerreisst Fleisch muehelos):
//   Sitzen    crouch 0..1: auf den Keulen, Vorderbeine gerade, Schwanz um die Pfoten gelegt (Ruhe, Ansprechen)
//   Gehen     Passgang der Katze (diagonal versetzt), Schwanz als Fragezeichen erhoben (alert), Kopf ruhig
//   Blick     look/lookAt; tilt: legt den Kopf schief - weiter als eine Katze (Auslegung: zu menschlich)
//   Sprechen  talk 0..1: der Kiefer formt Silben wie ein Mund, die Zunge arbeitet sichtbar mit
//   Kiefer    jaw 0..1: oeffnet bis gut 80 Grad (Gelenk weit hinten), die Saegezaehne liegen frei
//   Warnung   growl 0..1: Buckel, steife Beine, Ohren flach, Schwanz steil; shiver 0..1: das Fell straeubt sich (Massstab)
//   Sprung    reach 0..1: Rumpf steil zum Ziel (reachAt), Vorderpfoten mit Krallen voraus; hang 0..1: haengt am Gesicht
//             (Hinterbeine treten), strike 0..1: Phase der Hiebe der Vorderpfoten
//   Fressen   bait 0..1: Kopf am Boden (perceiveAt), reisst mit Kopfrucken, kaut
//   Licht     light 0..1: kneift, wendet das Gesicht ab; perceive 0..1: Ohren vor, Kopf zu perceiveAt
//   Schwanz   wave 0..1: peitscht (gereizt)
// Koordinaten wie im Spiel: x vorn, y links, z oben; Wurzel am Boden unter der Rumpfmitte.
#include "anim/creature_anim.hpp"

#include <algorithm>
#include <cmath>

#include "anim/skeleton.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kTau = 6.2831853f, kPiS = 3.14159265f;
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
// Gier/Nicken von a nach b im Rahmen (vorn F, links L)
void yawPitchS(vec3 a, vec3 b, vec3 F, vec3 L, float& yw, float& pt) {
    const vec3 d = b - a;
    const float fx = dot(d, F), fy = dot(d, L), fz = d.z;
    yw = std::atan2(fy, fx);
    pt = std::atan2(fz, std::max(0.02f, std::hypot(fx, fy)));
}

// Haltungen (Rumpf, Wurzelraum): stehend = Bindung; sitzend
const vec3 kSitPelvis{-0.085f, 0.0f, 0.065f}, kSitSpine{-0.05f, 0.0f, 0.128f}, kSitChest{-0.005f, 0.0f, 0.188f};
const vec3 kSitNeck{0.025f, 0.0f, 0.236f};

enum X : int {
    X_SIT, X_SITV, X_ALERT, X_ALERTV, X_TALK, X_TALKV, X_JAW, X_JAWV, X_ARCH, X_ARCHV, X_PUFF, X_PUFFV, X_REACH, X_REACHV,
    X_HANG, X_HANGV, X_EAT, X_EATV, X_LIGHT, X_LIGHTV, X_PERC, X_PERCV, X_LASH, X_LASHV, X_TILT, X_TILTV,
    X_HY, X_HYV, X_HP, X_HPV, X_EARF, X_EARFV, X_SYL, X_SYLV, X_SYLT, X_BLINK, X_FLICK, X_FLICKT, X_KNEAD,
    X_SIZE
};
}  // namespace

CreatureSkeleton sa::makeSkeleton() {
    CreatureSkeleton s;
    s.kind = Entity::Samantha;
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
    J(PELVIS, {-0.10f, 0, 0.165f});
    J(SPINE, {-0.01f, 0, 0.18f});
    J(CHEST, {0.075f, 0, 0.165f});
    J(NECK, {0.115f, 0, 0.205f});
    J(HEAD, {0.145f, 0, 0.235f});
    J(NOSE, {0.205f, 0, 0.228f});
    J(JAWP, {0.132f, 0, 0.214f});  // Gelenk weit hinten: das Maul reisst bis hinter die Augen auf
    J(JAWT, {0.196f, 0, 0.206f});
    J(TONGA, {0.148f, 0, 0.214f});
    J(TONGB, {0.184f, 0, 0.211f});
    J(EARB_L, {0.138f, 0.024f, 0.262f});
    J(EAR_L, {0.132f, 0.032f, 0.302f});
    J(EARB_R, {0.138f, -0.024f, 0.262f});
    J(EAR_R, {0.132f, -0.032f, 0.302f});
    for (int l = 0; l < 4; ++l) {
        const float g = (l % 2 == 0) ? 1.0f : -1.0f;
        if (l < 2) {
            J(legJ(l, 0), {0.07f, 0.03f * g, 0.15f});
            J(legJ(l, 1), {0.05f, 0.032f * g, 0.09f});
            J(legJ(l, 2), {0.07f, 0.03f * g, 0.024f});
            J(legJ(l, 3), {0.094f, 0.03f * g, 0.006f});
        } else {
            J(legJ(l, 0), {-0.10f, 0.035f * g, 0.145f});
            J(legJ(l, 1), {-0.064f, 0.04f * g, 0.095f});
            J(legJ(l, 2), {-0.124f, 0.034f * g, 0.044f});
            J(legJ(l, 3), {-0.096f, 0.034f * g, 0.006f});
        }
    }
    const vec3 tail[kTail] = {{-0.135f, 0, 0.175f}, {-0.178f, 0, 0.148f}, {-0.215f, 0, 0.114f},
                              {-0.245f, 0, 0.078f}, {-0.266f, 0, 0.045f}, {-0.28f, 0, 0.016f}};
    for (int i = 0; i < kTail; ++i) J(TAIL0 + i, tail[i]);

    B(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    B(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.05f, Z);
    B(B_SPINE, SPINE, CHEST, B_PELVIS, 0.05f, Z);
    B(B_CHEST, CHEST, NECK, B_SPINE, 0.045f, Z);
    B(B_NECK, NECK, HEAD, B_CHEST, 0.035f, Z);
    B(B_HEAD, HEAD, NOSE, B_NECK, 0.04f, Z);
    B(B_JAW, JAWP, JAWT, B_HEAD, 0.02f, Z);
    B(B_TONGUE, TONGA, TONGB, B_JAW, 0.008f, Z, false);
    B(B_EAR_L, EARB_L, EAR_L, B_HEAD, 0.012f, F, false);
    B(B_EAR_R, EARB_R, EAR_R, B_HEAD, 0.012f, F, false);
    for (int l = 0; l < 4; ++l) {
        B(legB(l, 0), legJ(l, 0), legJ(l, 1), l < 2 ? B_CHEST : B_PELVIS, 0.02f, F);
        B(legB(l, 1), legJ(l, 1), legJ(l, 2), legB(l, 0), 0.014f, F);
        B(legB(l, 2), legJ(l, 2), legJ(l, 3), legB(l, 1), 0.012f, Z, false);
    }
    for (int i = 0; i < kTail - 1; ++i) B(B_TAIL0 + i, TAIL0 + i, TAIL0 + i + 1, i == 0 ? B_PELVIS : B_TAIL0 + i - 1, 0.014f - 0.0015f * (float)i, Y, i < 2);
    s.height = 0.27f;
    s.reach = 0.45f;
    s.center = {-0.02f, 0.0f, 0.14f};
    return s;
}

void CreatureAnimator::updateSamantha(float dt, const Input& in) {
    using namespace sa;
    auto& P = pose_.j;
    const auto& Bd = sk_->bind;
    if ((int)xs_.size() != X_SIZE) {
        xs_.assign((size_t)X_SIZE, 0.0f);
        xs_[X_BLINK] = 2.5f, xs_[X_FLICKT] = 3.0f, xs_[X_SYLT] = 0.1f;
        xs_[X_SIT] = in.crouch;
    }
    float* X = xs_.data();
    if ((int)pose_.s.size() != BCOUNT) pose_.s.assign((size_t)BCOUNT, 1.0f);
    const float t = time_;
    // --- Federn der Eingaben -------------------------------------------------------------------------------------------
    const bool moving = speed_ > 0.06f;
    spring(X[X_SIT], X[X_SITV], moving ? 0.0f : in.crouch, 4.0f, dt);
    spring(X[X_ALERT], X[X_ALERTV], in.alert, 5.0f, dt);
    spring(X[X_TALK], X[X_TALKV], in.talk, 10.0f, dt);
    spring(X[X_JAW], X[X_JAWV], in.jaw, in.jaw > X[X_JAW] ? 14.0f : 6.0f, dt);
    spring(X[X_ARCH], X[X_ARCHV], in.growl, 6.0f, dt);
    spring(X[X_PUFF], X[X_PUFFV], std::max(in.shiver, 0.7f * in.growl), 9.0f, dt);
    spring(X[X_REACH], X[X_REACHV], in.reach, 12.0f, dt);
    spring(X[X_HANG], X[X_HANGV], in.hang, 10.0f, dt);
    spring(X[X_EAT], X[X_EATV], in.bait, 4.0f, dt);
    spring(X[X_LIGHT], X[X_LIGHTV], in.light, 5.0f, dt);
    spring(X[X_PERC], X[X_PERCV], in.perceive, 6.0f, dt);
    spring(X[X_LASH], X[X_LASHV], in.wave, 3.0f, dt);
    spring(X[X_TILT], X[X_TILTV], in.tilt, 2.5f, dt);
    const float sit = std::clamp(X[X_SIT], 0.0f, 1.0f), alert = std::clamp(X[X_ALERT], 0.0f, 1.0f);
    const float talk = std::clamp(X[X_TALK], 0.0f, 1.0f), arch = std::clamp(X[X_ARCH], 0.0f, 1.0f);
    const float puff = std::clamp(X[X_PUFF], 0.0f, 1.0f), reach = std::clamp(X[X_REACH], 0.0f, 1.0f);
    const float hang = std::clamp(X[X_HANG], 0.0f, 1.0f), eat = std::clamp(X[X_EAT], 0.0f, 1.0f);
    const float lightK = std::clamp(X[X_LIGHT], 0.0f, 1.0f), perc = std::clamp(X[X_PERC], 0.0f, 1.0f);
    const float lash = std::clamp(X[X_LASH], 0.0f, 1.0f);
    const float air = std::max(reach, hang);  // im Sprung / am Gesicht: keine Beine am Boden

    // --- Rahmen: Wurzel am Boden; im Sprung kippt der Rumpf steil zum Ziel -------------------------------------------------
    const vec3 F = dirYaw(yaw_), Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    Fr root;
    root.o = pos_, root.x = F, root.y = Lw, root.z = U;
    Fr body = root;
    float pitchUp = 0.0f;
    if (air > 0.001f) {
        float yw, pt;
        yawPitchS(pos_ + U * 0.15f, in.reachAt, F, Lw, yw, pt);
        pitchUp = air * std::clamp(pt + 0.35f, 0.2f, 1.25f);
        body.turn(Lw, -pitchUp);
        body.o = pos_ + U * (0.06f * air);
    }
    // Rumpf: stehend (Bindung) .. sitzend; Buckel (arch): Ruecken hoch, Brust und Becken tiefer auf steifen Beinen
    const float breath = std::sin(t * (alert > 0.5f ? 5.2f : 3.1f)) * (1.0f - air);
    auto torso = [&](int j, vec3 sitP) {
        vec3 p = lerp3(Bd[(size_t)j], sitP, sit);
        if (j == SPINE) p.z += 0.038f * arch * (1.0f - sit);
        if (j == PELVIS || j == CHEST) p.z += 0.012f * arch * (1.0f - sit);  // steife Beine: alles hoeher
        if (j == CHEST) p.z += 0.0025f * breath;
        if (eat > 0.0f && j == CHEST) p.z -= 0.035f * eat * (1.0f - sit), p.x += 0.01f * eat;
        return body.at(p);
    };
    P[ROOT] = pos_;
    P[PELVIS] = torso(PELVIS, kSitPelvis);
    P[SPINE] = torso(SPINE, kSitSpine);
    P[CHEST] = torso(CHEST, kSitChest);
    vec3 neckL = lerp3(Bd[NECK], kSitNeck, sit);
    neckL.z += 0.01f * alert - 0.03f * eat - 0.012f * arch;
    neckL.x += 0.02f * eat;
    P[NECK] = body.at(neckL);
    // Massstab: Atem, gestraeubtes Fell (Rumpf, Hals, Schwanz)
    const float fp = 1.0f + 0.12f * puff;
    pose_.s[sa::B_PELVIS] = fp, pose_.s[sa::B_SPINE] = fp * (1.0f + 0.01f * breath), pose_.s[sa::B_CHEST] = fp, pose_.s[sa::B_NECK] = 1.0f + 0.08f * puff;

    // --- Kopf: Blick, Wahrnehmen, Licht, Fressen; schief gelegt --------------------------------------------------------------
    const vec3 bF = body.x, bL = body.y;
    float wy = 0.0f, wp = 0.0f;
    if (in.look) yawPitchS(P[NECK], in.lookAt, bF, bL, wy, wp);
    else wy = 0.35f * std::sin(t * 0.37f) * (1.0f - alert), wp = -0.05f + 0.06f * std::sin(t * 0.29f);
    if (perc > 0.01f) {
        float y2, p2;
        yawPitchS(P[NECK], in.perceiveAt, bF, bL, y2, p2);
        wy += wrapA(y2 - wy) * perc, wp += (p2 - wp) * perc;
    }
    if (lightK > 0.01f) {  // aus dem Licht drehen
        float ly, lp;
        yawPitchS(P[NECK], in.lightFrom, bF, bL, ly, lp);
        wy += ((ly > 0.0f ? ly - 1.2f : ly + 1.2f) - wy) * lightK * 0.6f;
        wp += (-0.25f - wp) * lightK * 0.4f;
    }
    if (eat > 0.01f) wp += (-1.15f - wp) * eat, wy *= 1.0f - eat;
    if (air > 0.01f) wy *= 1.0f - air, wp = wp * (1.0f - air) + 0.15f * air;  // im Sprung: Kopf in Flugrichtung
    wy = std::clamp(wrapA(wy), -1.45f, 1.45f);
    wp = std::clamp(wp, -1.25f, 0.95f);
    // Fressen: ruckartiges Reissen (Kopf seitlich und zurueck)
    const float tear = eat * std::max(0.0f, std::sin(t * 4.3f)) * std::max(0.0f, std::sin(t * 1.7f + 0.4f));
    wy += 0.35f * tear * std::sin(t * 11.0f);
    wp += 0.25f * tear;
    if (!chainInit_) X[X_HY] = wy, X[X_HP] = wp, X[X_HYV] = X[X_HPV] = 0.0f, chainInit_ = true;
    spring(X[X_HY], X[X_HYV], wy, alert > 0.5f ? 14.0f : 8.0f, dt);
    spring(X[X_HP], X[X_HPV], wp, alert > 0.5f ? 14.0f : 8.0f, dt);
    const float hy = X[X_HY], hp = X[X_HP];
    // schief: bis 0,9 rad - weiter als eine Katze es tut
    const float tl = std::clamp(X[X_TILT], -0.9f, 0.9f) + 0.06f * std::sin(t * 0.8f) * alert;
    Fr hf = body;  // Kopfrahmen
    hf.turn(body.z, hy);
    hf.turn(hf.y, -hp);
    hf.turn(hf.x, tl);
    Fr nf = body;  // Hals: halbe Drehung
    nf.turn(body.z, hy * 0.5f);
    nf.turn(nf.y, -hp * 0.5f);
    P[HEAD] = P[NECK] + nf.dir(Bd[HEAD] - Bd[NECK]) * (1.0f + 0.15f * alert + 0.25f * eat);
    auto headAt = [&](int j) { return P[HEAD] + hf.dir(Bd[(size_t)j] - Bd[HEAD]); };
    P[NOSE] = headAt(NOSE);
    P[JAWP] = headAt(JAWP);
    // Kiefer: Silben (wie ein Mund - Auslegung), weit offen bis gut 80 Grad (jaw), kauen beim Fressen
    X[X_SYLT] -= dt;
    if (X[X_SYLT] <= 0.0f) X[X_SYLT] = 0.09f + 0.09f * hashTo01((unsigned)(t * 977.0f)), X[X_SYL] = talk * (0.25f + 0.45f * hashTo01((unsigned)(t * 613.0f) + 7u));
    spring(X[X_SYLV], X[X_FLICK], X[X_SYL], 30.0f, dt);  // X_SYLV: Silbe (gefedert), X_FLICK als ihre Geschwindigkeit
    const float syl = std::clamp(X[X_SYLV], 0.0f, 1.0f);
    const float chew = eat * (0.5f + 0.5f * std::sin(t * 9.0f));
    const float open = std::clamp(X[X_JAW], 0.0f, 1.0f) * 1.42f + syl * 0.42f + chew * 0.45f + 0.03f * lightK;
    Fr jf = hf;
    jf.turn(hf.y, open);  // Kinn nach unten
    P[JAWT] = P[JAWP] + jf.dir(Bd[JAWT] - Bd[JAWP]);
    // Zunge: liegt im Unterkiefer, beim Sprechen vor und zurueck, hebt sich
    const float tw = talk * std::sin(t * 23.0f + 1.3f);
    P[TONGA] = P[JAWP] + jf.dir(Bd[TONGA] - Bd[JAWP]);
    P[TONGB] = P[JAWP] + jf.dir(Bd[TONGB] - Bd[JAWP] + vec3{0.006f * tw, 0.0f, 0.004f * talk + 0.003f * std::fabs(tw)});
    // Ohren: vor (aufmerksam), flach zurueck (Warnung, Licht), einzeln zuckend
    X[X_FLICKT] -= dt;
    if (X[X_FLICKT] <= 0.0f) X[X_FLICKT] = 1.5f + 4.0f * hashTo01((unsigned)(t * 131.0f)), X[X_KNEAD] = 0.25f;
    X[X_KNEAD] = std::max(0.0f, X[X_KNEAD] - dt);
    const float flat = std::clamp(std::max(arch, lightK * 0.7f) + 0.6f * hang, 0.0f, 1.0f);
    for (int sd = 0; sd < 2; ++sd) {
        const int base = sd ? EARB_R : EARB_L, tip = sd ? EAR_R : EAR_L;
        const float g = sd ? -1.0f : 1.0f;
        vec3 d = Bd[(size_t)tip] - Bd[(size_t)base];
        d = rot(d, {1, 0, 0}, g * (0.95f * flat));     // flach zur Seite
        d = rot(d, {0, 1, 0}, -0.9f * flat + 0.25f * (alert + perc) * (1.0f - flat));  // zurueck / vor
        if (X[X_KNEAD] > 0.0f && sd == ((int)(t * 3.0f) & 1)) d = rot(d, {1, 0, 0}, g * 0.4f * std::sin(X[X_KNEAD] * 40.0f));
        P[(size_t)base] = headAt(base);
        P[(size_t)tip] = P[(size_t)base] + hf.dir(d);
    }
    headPos_ = (P[HEAD] + P[NOSE]) * 0.5f + hf.z * 0.012f;
    headDir_ = hf.x;

    // --- Beine -------------------------------------------------------------------------------------------------------------
    if (!run_ && speed_ > 1.1f) run_ = true;
    else if (run_ && speed_ < 0.8f) run_ = false;
    static const float walkOff[4] = {0.5f, 0.0f, 0.75f, 0.25f}, runOff[4] = {0.0f, 0.1f, 0.5f, 0.6f};
    const float* offs = run_ ? runOff : walkOff;
    vec3 rest[4];
    for (int l = 0; l < 4; ++l) rest[l] = root.at({Bd[(size_t)legJ(l, 2)].x, Bd[(size_t)legJ(l, 2)].y, 0.0f});
    const float wgt[4] = {0.03f, 0.03f, 0.04f, 0.04f};
    stepLegs(dt, 4, offs, run_ ? 0.4f : 0.6f, run_ ? 0.28f + 0.05f * speed_ : 0.08f + 0.12f * speed_, rest, 0.15f, wgt);
    const float lift[4] = {0.03f, 0.03f, 0.028f, 0.028f};
    for (int l = 0; l < 4; ++l) {
        const bool front = l < 2;
        const float g = (l % 2 == 0) ? 1.0f : -1.0f;
        const vec3 base = front ? P[CHEST] : P[PELVIS];
        const vec3 T = base + body.dir(Bd[(size_t)legJ(l, 0)] - Bd[front ? CHEST : PELVIS]);
        const float l1 = length(Bd[(size_t)legJ(l, 1)] - Bd[(size_t)legJ(l, 0)]);
        const float l2 = length(Bd[(size_t)legJ(l, 2)] - Bd[(size_t)legJ(l, 1)]);
        const vec3 pawV = Bd[(size_t)legJ(l, 3)] - Bd[(size_t)legJ(l, 2)];
        // stehend/gehend: Fuss aus dem Gangzyklus (Handgelenk ueber dem Boden)
        vec3 wrist = footPos(l, lift) + U * Bd[(size_t)legJ(l, 2)].z;
        vec3 tip = wrist + root.dir(pawV);
        vec3 knee;
        // sitzend: Vorderbeine gerade unter der Brust, Hinterpfoten flach nach vorn unter dem Leib
        if (sit > 0.001f) {
            const vec3 sw = front ? root.at({0.045f, g * 0.026f, 0.022f}) : root.at({-0.115f, g * 0.042f, 0.012f});
            const vec3 st = front ? root.at({0.068f, g * 0.025f, 0.005f}) : root.at({-0.035f, g * 0.036f, 0.005f});
            wrist = lerp3(wrist, sw, sit), tip = lerp3(tip, st, sit);
        }
        if (eat > 0.001f && front) {  // eine Pfote haelt das Fleisch am Boden
            const vec3 hold = lerp3(root.at({0.15f, g * 0.03f, 0.02f}), in.perceiveAt + U * 0.02f + Lw * (g * 0.025f), 0.6f);
            wrist = lerp3(wrist, hold, eat * (g > 0.0f ? 1.0f : 0.25f));
            tip = wrist + root.dir(pawV) * 1.1f;
        }
        if (air > 0.001f) {
            if (front) {  // Vorderpfoten voraus, Krallen, im Takt der Hiebe (strike)
                const float ph = in.strike > 0.0f ? in.strike * kTau * 3.0f + (g > 0.0f ? 0.0f : kPiS) : t * 2.0f;
                const vec3 aim = nrm(in.reachAt - T, body.x);
                const vec3 sw = T + aim * (0.15f + 0.03f * std::sin(ph)) + body.y * (g * (0.04f + 0.03f * std::cos(ph))) +
                                body.z * (0.035f * std::sin(ph + 0.6f)) * hang;
                wrist = lerp3(wrist, sw, air);
                tip = lerp3(tip, wrist + aim * 0.03f + body.z * 0.012f, air);
            } else {  // Hinterbeine: haengen, treten (wie Katzen im Kampf)
                const float kick = hang * std::sin(t * 17.0f + (g > 0.0f ? 0.0f : 1.7f));
                const vec3 sw = T - body.x * (0.07f - 0.05f * kick) - body.z * 0.06f + body.y * (g * 0.03f);
                wrist = lerp3(wrist, sw, air);
                tip = lerp3(tip, wrist - body.x * 0.025f, air);
            }
        }
        // Buckel: steife Beine (die Pfoten bleiben, die Gelenke strecken sich)
        const vec3 pole = front ? nrm(-body.x * (1.0f - 0.6f * arch) - U * 0.2f, -body.x) : nrm(body.x + U * 0.1f, body.x);
        vec3 w2;
        ik2(T, wrist, l1, l2, front ? pole : pole, knee, w2);
        if (!front && sit > 0.001f) knee = lerp3(knee, root.at({-0.03f, g * 0.062f, 0.06f}), sit);  // Keule seitlich
        P[(size_t)legJ(l, 0)] = T;
        P[(size_t)legJ(l, 1)] = knee;
        P[(size_t)legJ(l, 2)] = w2;
        P[(size_t)legJ(l, 3)] = w2 + (tip - wrist);
    }

    // --- Schwanz: Fragezeichen (gehend, aufmerksam), um die Pfoten (sitzend), steil und gestraeubt (Warnung), peitscht ------------
    {
        const vec3 b0 = torso(TAIL0, vec3{-0.105f, 0.0f, 0.055f});
        P[TAIL0] = b0;
        const float up = std::clamp(0.25f + 0.7f * alert * (1.0f - sit) + 1.0f * arch, -0.6f, 1.35f);
        vec3 d = nrm(-body.x * std::cos(up) + body.z * std::sin(up), -body.x);
        if (sit > 0.5f) d = nrm(lerp3(d, nrm(-root.x * 0.6f - U * 0.8f, -root.x), (sit - 0.5f) * 2.0f), d);
        const float lashA = (0.55f * lash + 0.12f) * std::sin(t * (lash > 0.3f ? 6.5f : 1.8f));
        float segL = 0.0f;
        for (int i = 1; i < kTail; ++i) {
            segL = length(Bd[(size_t)(TAIL0 + i)] - Bd[(size_t)(TAIL0 + i - 1)]);
            const float k = (float)i / (float)(kTail - 1);
            // Bogen: aufrecht biegt sich die Spitze nach vorn (Fragezeichen), haengend nach unten; sitzend um die Pfoten
            float bend = (alert * (1.0f - sit) * (1.0f - arch)) * (k > 0.6f ? 0.55f : 0.05f) - (1.0f - alert) * (1.0f - sit) * 0.12f;
            vec3 side = nrm(cross(d, U), body.y);
            d = nrm(rot(d, side, -bend), d);
            d = nrm(rot(d, U, lashA * (0.4f + k) * 0.35f + sit * 0.55f), d);
            if (sit > 0.5f) d.z = std::max(d.z, -0.05f), d = nrm(d, -root.x);
            vec3 p = P[(size_t)(TAIL0 + i - 1)] + d * segL;
            if (p.z < pos_.z + 0.012f) p.z = pos_.z + 0.012f;
            P[(size_t)(TAIL0 + i)] = p;
        }
        for (int i = 0; i < kTail - 1; ++i) pose_.s[(size_t)(sa::B_TAIL0 + i)] = 1.0f + 0.45f * puff;
    }

    autoUps();
    pose_.up[sa::B_HEAD] = hf.z;
    pose_.up[sa::B_JAW] = jf.z;
    pose_.up[sa::B_TONGUE] = jf.z;
    pose_.up[sa::B_NECK] = nf.z;
    pose_.up[sa::B_PELVIS] = body.z, pose_.up[sa::B_SPINE] = body.z, pose_.up[sa::B_CHEST] = body.z;
    pose_.up[sa::B_EAR_L] = hf.x, pose_.up[sa::B_EAR_R] = hf.x;
}

}  // namespace lim::anim
