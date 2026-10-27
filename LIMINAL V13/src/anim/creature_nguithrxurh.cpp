// V13.3: Nguithr'xurh (Entity 16, Wikidot entity-16; KREATUR_Nguithrxurh.md) - Skelett und Bewegung.
//
// Eine Walzenspinne (Solifugae) mit 16 Gliedmassen: zwoelf Laufbeine, zwei Pedipalpen (tastend vorgestreckt), zwei
// riesige Cheliceren (Scheren, die abwechselnd arbeiten). Im Mittel 18 cm lang (die Seite), je Ebene groesser/kleiner.
// Im selben Skelett: die Netzkugeln einer Kolonie (Variante "web") - sie haengen an Faeden von der Decke, gefuellt mit
// einem Betaeubungsstoff; abgeschnitten fallen sie, zerplatzen am Boden und hinterlassen eine milchige Pfuetze.
//
// Spinne (extra_[0] == 0): sitzt an Decke oder Wand (in.up = Normale der Flaeche, in.fwd = vorn), die Beine greifen;
//   Ruhe       reglos, nur der Hinterleib pumpt, ab und zu tippt ein Pedipalpus, eine Chelicere putzt
//   Wahrnehmen Pedipalpen und vordere Beine heben sich (Drohhaltung der Walzenspinnen)
//   Licht      erstarrt, drueckt sich flach an die Flaeche
//   Huschen    sehr schneller Wechselgang (zwei Gruppen zu sechs Beinen)
//   Abseilen   (hang) haengt am Faden aus den Spinnwarzen, die Beine angezogen, dreht sich langsam
//   Fressen    (jaw) die Cheliceren saegen abwechselnd vor und zurueck; Biss (reach): Stoss nach vorn, Scheren weit auf
// Netz (extra_[0] == 1): extra_ = {1, Anzahl, je Kugel: Anker x y z, Haengelaenge, Boden z, Fallzeit (s, < 0 haengt)}
#include <algorithm>
#include <cmath>

#include "anim/creature_anim.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);
mat4 rigidInverse(const mat4& m);

namespace {
vec3 rotV(vec3 v, vec3 axis, float ang) {  // wie rotateAround (anim/skeleton.hpp)
    const float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
constexpr float kPiN = 3.14159265f, kTauN = 6.2831853f;
float smN(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
void sprN(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
void sprNV(vec3& x, vec3& v, vec3 t, float omega, float dt) {
    sprN(x.x, v.x, t.x, omega, dt);
    sprN(x.y, v.y, t.y, omega, dt);
    sprN(x.z, v.z, t.z, omega, dt);
}
vec3 nrmN(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
struct NB {
    CreatureSkeleton s;
    NB(Entity e, int joints, int bones) {
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
    vec3 dir(int a, int b) const { return nrmN(s.bind[(size_t)b] - s.bind[(size_t)a], {1, 0, 0}); }
};
vec3 upN(vec3 d) { return std::fabs(d.z) > 0.8f ? vec3{1, 0, 0} : vec3{0, 0, 1}; }

// Beine: Paar p (0 vorn .. 5 hinten), Winkel zur Vorderrichtung und Laenge (m, Mittelgroesse)
constexpr float kLegAng[6] = {0.66f, 1.05f, 1.43f, 1.83f, 2.22f, 2.6f};
constexpr float kLegLen[6] = {0.10f, 0.115f, 0.13f, 0.14f, 0.15f, 0.165f};
vec3 coxa(int l) {
    const int p = l / 2;
    const float sg = (l % 2) == 0 ? 1.0f : -1.0f;
    return {0.035f - 0.011f * (float)p, sg * 0.018f, 0.026f};
}
vec3 legDir(int l) {
    const int p = l / 2;
    const float sg = (l % 2) == 0 ? 1.0f : -1.0f;
    return {std::cos(kLegAng[p]), sg * std::sin(kLegAng[p]), 0.0f};
}

// Zustand der Bewegung
struct NState {
    vec3 up{0, 0, 1}, upV{}, fwd{1, 0, 0}, fwdV{}, lastPos{};
    bool init = false;
    vec3 plant[ng::kLegs]{}, from[ng::kLegs]{}, to[ng::kLegs]{};
    float swingT[ng::kLegs]{};
    bool swing[ng::kLegs]{};
    float ph = 0.0f, freeze = 0.0f, freezeV = 0.0f, flat = 0.0f, flatV = 0.0f, rise = 0.0f, riseV = 0.0f, hang = 0.0f, hangV = 0.0f;
    float bite = 0.0f, biteV = 0.0f, chew = 0.0f, chewV = 0.0f, spin = 0.0f, tapT = 1.0f, tapSide = 0.0f, tapPh = -1.0f;
    vec3 vel{};
    float sacPh[ng::kSacs]{};
};
}  // namespace

CreatureSkeleton makeNguithrSkeleton() {
    using namespace ng;
    NB k(Entity::Nguithr, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(PRO, {0.015f, 0, 0.032f});
    k.j(EYES, {0.05f, 0, 0.040f});
    k.j(OPI0, {-0.02f, 0, 0.032f});
    k.j(OPI1, {-0.065f, 0, 0.036f});
    k.j(OPI2, {-0.105f, 0, 0.030f});
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        k.j(s == 0 ? CHB_L : CHB_R, {0.055f, sg * 0.011f, 0.036f});
        k.j(s == 0 ? CHT_L : CHT_R, {0.105f, sg * 0.013f, 0.026f});
        k.j(s == 0 ? PPB_L : PPB_R, {0.045f, sg * 0.02f, 0.026f});
        k.j(s == 0 ? PPK_L : PPK_R, {0.085f, sg * 0.055f, 0.055f});
        k.j(s == 0 ? PPT_L : PPT_R, {0.135f, sg * 0.07f, 0.012f});
    }
    for (int l = 0; l < kLegs; ++l) {
        const int p = l / 2;
        const vec3 C = coxa(l), d = legDir(l);
        const float Lp = kLegLen[p];
        k.j(legJ(l, 0), C);
        k.j(legJ(l, 1), C + d * (0.32f * Lp) + vec3{0, 0, 0.055f + 0.005f * (float)p});
        k.j(legJ(l, 2), C + d * (0.78f * Lp) + vec3{0, 0, 0.025f});
        k.j(legJ(l, 3), vec3{C.x + d.x * Lp, C.y + d.y * Lp, 0.0f});
    }
    k.j(DRAG_A, k.s.bind[OPI2]);
    k.j(DRAG_B, k.s.bind[OPI2] + vec3{0, 0, 0.25f});
    for (int q = 0; q < kSacs; ++q) {
        const Sac& S = kSac[q];
        k.j(sacJ(q, 0), {S.x, S.y, 0.0f});
        k.j(sacJ(q, 1), {S.x, S.y, -S.len});
        k.j(sacJ(q, 2), {S.x, S.y, -kHang});
        k.j(sacJ(q, 3), {S.x + 0.1f, S.y, -kHang});
    }
    const vec3 F{1, 0, 0}, Z{0, 0, 1};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_PRO, PRO, EYES, B_ROOT, 0.03f, Z);
    k.b(B_OPI0, OPI0, OPI1, B_PRO, 0.035f, Z);
    k.b(B_OPI1, OPI1, OPI2, B_OPI0, 0.03f, Z);
    k.b(B_CH_L, CHB_L, CHT_L, B_PRO, 0.012f, Z, false);
    k.b(B_CH_R, CHB_R, CHT_R, B_PRO, 0.012f, Z, false);
    for (int s = 0; s < 2; ++s) {
        const int pb = s == 0 ? PPB_L : PPB_R, b0 = s == 0 ? B_PP0_L : B_PP0_R;
        k.b(b0, pb, pb + 1, B_PRO, 0.006f, upN(k.dir(pb, pb + 1)), false);
        k.b(b0 + 1, pb + 1, pb + 2, b0, 0.005f, upN(k.dir(pb + 1, pb + 2)), false);
    }
    for (int l = 0; l < kLegs; ++l)
        for (int i = 0; i < 3; ++i)
            k.b(legB(l, i), legJ(l, i), legJ(l, i + 1), i == 0 ? B_PRO : legB(l, i - 1), 0.004f, upN(k.dir(legJ(l, i), legJ(l, i + 1))), false);
    k.b(B_DRAG, DRAG_A, DRAG_B, B_OPI1, 0.002f, F, false);
    for (int q = 0; q < kSacs; ++q) {
        k.b(sacB(q), sacJ(q, 0), sacJ(q, 1), B_ROOT, kSac[q].r, F, true);
        k.b(splatB(q), sacJ(q, 2), sacJ(q, 3), B_ROOT, 0.01f, Z, false);
    }
    k.s.height = 0.08f;
    k.s.reach = 1.8f;
    k.s.center = {0.0f, 0, 0.03f};
    return k.s;
}

void CreatureAnimator::updateNguithr(float dt, const Input& in) {
    using namespace ng;
    if (!ext_) ext_ = std::make_shared<NState>();
    NState& g = *static_cast<NState*>(ext_.get());
    const auto& B = sk_->bind;
    auto& J = pose_.j;
    const size_t nb = sk_->bones.size();
    pose_.s.assign(nb, 1.0f);
    const bool web = !extra_.empty() && extra_[0] > 0.5f;
    if (web) {
        // --- Netzkugeln --------------------------------------------------------------------------------------
        for (int j = 0; j < SAC0; ++j) J[(size_t)j] = pos_;  // Spinnenteile verborgen in der Mitte
        for (int b = 0; b < B_SAC0; ++b) pose_.s[(size_t)b] = 0.001f;
        const int n = extra_.size() > 1 ? std::min((int)extra_[1], kSacs) : 0;
        sprN(light_, lightV_, in.shiver, 3.0f, dt);  // Zittern (eine Spinne laeuft ueber das Netz)
        for (int q = 0; q < kSacs; ++q) {
            const size_t o = 2 + (size_t)q * 6;
            if (q >= n || o + 5 >= extra_.size() || extra_[o + 3] < 0.0f) {  // keine Kugel an dieser Stelle
                for (int i = 0; i < 4; ++i) J[(size_t)sacJ(q, i)] = pos_;
                pose_.s[(size_t)sacB(q)] = pose_.s[(size_t)splatB(q)] = 0.001f;
                continue;
            }
            const vec3 A{extra_[o], extra_[o + 1], extra_[o + 2]};
            const float hang = extra_[o + 3], floorZ = extra_[o + 4], fall = extra_[o + 5];
            const float r = kSac[q].r, len0 = kSac[q].len;
            g.sacPh[q] += dt * (0.9f + 0.13f * (float)q);
            vec3 c = A - vec3{0, 0, hang};
            if (fall < 0.0f) {  // haengt: pendelt kaum merklich, zittert, wenn die Spinne darueber laeuft
                const float sw = 0.012f + 0.03f * light_;
                const float sx = std::sin(g.sacPh[q] + (float)q * 1.7f) * sw * hang * (1.0f + 4.0f * light_ * std::sin(time_ * 23.0f + (float)q));
                const float sy = std::cos(g.sacPh[q] * 0.77f + (float)q) * sw * hang;
                c = c + vec3{sx, sy, 0.0f};
            } else {  // faellt (mit Faden), zerplatzt am Boden
                const float zc = c.z - 0.5f * 9.8f * fall * fall;
                c.z = std::max(zc, floorZ + r * 0.7f);
            }
            const bool burst = fall >= 0.0f && c.z <= floorZ + r * 0.7f + 1e-4f;
            // Knochen: vom (gedachten) Anker der Ruhepose - der Faden reicht im Netz weit nach oben (durch die Decke)
            J[(size_t)sacJ(q, 0)] = c + nrmN(A - c, {0, 0, 1}) * len0;
            J[(size_t)sacJ(q, 1)] = c;
            if (burst) {
                const float tb = fall - std::sqrt(std::max(0.0f, 2.0f * (A.z - hang - floorZ - r * 0.7f) / 9.8f));
                pose_.s[(size_t)sacB(q)] = std::max(0.001f, 1.0f - tb * 9.0f);  // platzt in 0,1 s
                const vec3 p{c.x, c.y, floorZ + 0.004f};
                J[(size_t)sacJ(q, 2)] = p;
                J[(size_t)sacJ(q, 3)] = p + vec3{std::cos((float)q * 2.1f), std::sin((float)q * 2.1f), 0.0f} * 0.1f;
                pose_.s[(size_t)splatB(q)] = std::clamp(0.2f + tb * 6.0f, 0.001f, 1.0f);
            } else {
                J[(size_t)sacJ(q, 2)] = c;
                J[(size_t)sacJ(q, 3)] = c + vec3{0.1f, 0, 0};
                pose_.s[(size_t)splatB(q)] = 0.001f;
            }
        }
        J[(size_t)DRAG_A] = J[(size_t)DRAG_B] = pos_;
        autoUps();
        for (int q = 0; q < kSacs; ++q) pose_.up[(size_t)sacB(q)] = vec3{1, 0, 0}, pose_.up[(size_t)splatB(q)] = vec3{0, 0, 1};
        headPos_ = pos_;
        return;
    }
    // --- Spinne ---------------------------------------------------------------------------------------------------
    const float s = std::max(0.05f, scale_);
    for (int q = 0; q < kSacs; ++q) {
        for (int i = 0; i < 4; ++i) J[(size_t)sacJ(q, i)] = pos_;
        pose_.s[(size_t)sacB(q)] = pose_.s[(size_t)splatB(q)] = 0.001f;
    }
    // Lage: Normale der Flaeche und vorn (gefedert); haengend am Faden zeigt der Kopf nach unten
    vec3 wantUp = nrmN(in.up, {0, 0, 1}), wantFwd = length(in.fwd) > 0.1f ? nrmN(in.fwd, {1, 0, 0}) : vec3{std::cos(yaw_), std::sin(yaw_), 0.0f};
    if (!g.init || !chainInit_) {
        g.up = wantUp, g.fwd = wantFwd, g.upV = g.fwdV = {}, g.lastPos = pos_, g.init = true;
        g.hang = in.hang;
    }
    sprNV(g.up, g.upV, wantUp, 9.0f, dt);
    sprNV(g.fwd, g.fwdV, wantFwd, 9.0f, dt);
    const vec3 U = nrmN(g.up, {0, 0, 1});
    const vec3 F = nrmN(g.fwd - U * dot(g.fwd, U), std::fabs(U.z) > 0.9f ? vec3{1, 0, 0} : vec3{0, 0, 1});
    const vec3 L = cross(U, F);
    const vec3 v = dt > 0.0f ? (pos_ - g.lastPos) / dt : vec3{};
    g.lastPos = pos_;
    g.vel = g.vel + (v - g.vel) * std::min(1.0f, dt * 10.0f);
    const float spd = length(g.vel - U * dot(g.vel, U));
    sprN(g.freeze, g.freezeV, in.light, 5.0f, dt);
    sprN(g.flat, g.flatV, std::max(in.light, in.crouch), 4.0f, dt);
    sprN(g.rise, g.riseV, in.perceive, 4.0f, dt);
    sprN(g.hang, g.hangV, in.hang, 3.0f, dt);
    sprN(g.bite, g.biteV, in.reach, 14.0f, dt);
    sprN(g.chew, g.chewV, in.jaw, 6.0f, dt);
    const float live = 1.0f - 0.85f * g.freeze;  // im Licht erstarrt alles
    const bool free = g.hang > 0.5f || in.fly > 0.5f;
    auto Wl = [&](vec3 l) { return pos_ + (F * l.x + L * l.y + U * l.z) * s; };
    // Koerper: Hinterleib pumpt (Atem), Wippen im Lauf, flach an die Flaeche gedrueckt im Licht, aufgerichtet beim Drohen
    const float breath = 0.0025f * std::sin(time_ * 3.3f) * live;
    const float bob = spd > 0.05f ? 0.004f * std::sin(g.ph * kTauN * 2.0f) : 0.0f;
    const float lift = (bob - 0.012f * g.flat + 0.008f * g.rise) * 1.0f;
    const float pitchUp = 0.45f * g.rise - 0.15f * g.bite;  // vorn hoch
    auto body = [&](vec3 l) {  // Rumpfpunkt: um das Prosoma geneigt
        const vec3 c = B[PRO];
        vec3 d = l - c;
        d = rotV(d, {0, 1, 0}, -pitchUp * (d.x > 0.0f ? 1.0f : 0.35f));
        return Wl(c + d + vec3{0.03f * g.bite, 0, lift});
    };
    J[ROOT] = pos_;
    J[PRO] = body(B[PRO]);
    J[EYES] = body(B[EYES]);
    J[OPI0] = body(B[OPI0]);
    J[OPI1] = body(B[OPI1] + vec3{0, 0, breath});
    J[OPI2] = body(B[OPI2] + vec3{-breath * 0.5f, 0, breath * 1.6f});
    // Cheliceren: abwechselnd vor und zurueck (fressen), weit auf beim Biss, putzen ab und zu
    for (int sd = 0; sd < 2; ++sd) {
        const int b0 = sd == 0 ? CHB_L : CHB_R;
        const float saw = g.chew * 0.012f * std::sin(time_ * 11.0f + (float)sd * kPiN) + 0.002f * std::sin(time_ * 1.7f + (float)sd) * live;
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const vec3 open{0.0f, sg * 0.012f * g.bite, 0.01f * g.bite};
        J[(size_t)b0] = body(B[(size_t)b0] + vec3{saw, 0, 0});
        J[(size_t)b0 + 1] = body(B[(size_t)b0 + 1] + vec3{saw, 0, 0} + open);
    }
    // Pedipalpen: tasten vorn, tippen ab und zu auf die Flaeche; drohend hoch gehalten
    g.tapT -= dt * live;
    if (g.tapT <= 0.0f && g.tapPh < 0.0f) g.tapPh = 0.0f, g.tapSide = rnd() < 0.5f ? 0.0f : 1.0f, g.tapT = 0.8f + 2.5f * rnd();
    float tap = 0.0f;
    if (g.tapPh >= 0.0f) {
        g.tapPh += dt * 4.0f;
        tap = std::sin(kPiN * std::min(g.tapPh, 1.0f));
        if (g.tapPh >= 1.0f) g.tapPh = -1.0f;
    }
    for (int sd = 0; sd < 2; ++sd) {
        const int pb = sd == 0 ? PPB_L : PPB_R;
        const float sg = sd == 0 ? 1.0f : -1.0f;
        const float t = ((int)g.tapSide == sd ? tap : 0.0f) * live;
        const vec3 knee = B[(size_t)pb + 1] + vec3{0.01f * g.rise, sg * 0.01f * g.rise, 0.05f * g.rise + 0.012f * t};
        vec3 tip = B[(size_t)pb + 2] + vec3{0.01f * t, 0.0f, 0.09f * g.rise + 0.02f * t - 0.012f * t * t};
        if (g.bite > 0.05f) tip = tip + vec3{0.03f * g.bite, -sg * 0.03f * g.bite, 0.02f * g.bite};  // greifen
        if (free) tip = B[(size_t)pb + 1] + vec3{0.03f, sg * 0.01f, -0.03f};
        J[(size_t)pb] = body(B[(size_t)pb]);
        J[(size_t)pb + 1] = body(knee);
        J[(size_t)pb + 2] = body(tip);
    }
    // --- Beine: Wechselgang zweier Gruppen; frei haengend angezogen -------------------------------------------
    const float S = 0.06f * s + 0.02f * spd;
    const bool walking = spd > 0.03f;
    if (walking) g.ph += spd / std::max(0.02f, S) * dt;
    g.ph -= std::floor(g.ph);
    const float duty = 0.55f;
    for (int l = 0; l < kLegs; ++l) {
        const int p = l / 2, sd = l % 2;
        const vec3 C = body(coxa(l));
        const float Lp = kLegLen[p] * s;
        const vec3 restL = coxa(l) + legDir(l) * kLegLen[p];
        vec3 rest = Wl(vec3{restL.x, restL.y, 0.0f});
        // vordere Beine heben sich beim Drohen
        const bool raise = p == 0 && g.rise > 0.05f;
        const int grp = (p + sd) % 2;
        vec3 footW;
        if (free) {  // am Faden: angezogen, die Spitzen zucken
            const vec3 d = legDir(l);
            footW = Wl(coxa(l) + d * (kLegLen[p] * 0.55f) + vec3{0, 0, 0.05f + 0.01f * std::sin(time_ * 6.0f + (float)l)});
            g.plant[l] = g.from[l] = g.to[l] = rest, g.swing[l] = false;
        } else {
            if (!g.init || length(g.plant[l]) < 1e-6f || length(g.plant[l] - rest) > 3.0f * Lp) g.plant[l] = g.from[l] = g.to[l] = rest, g.swing[l] = false;
            float phl = g.ph + (grp ? 0.5f : 0.0f) + 0.04f * (float)p;
            phl -= std::floor(phl);
            const bool want = walking && phl > duty;
            if (want && !g.swing[l]) {
                g.swing[l] = true, g.from[l] = g.plant[l], g.swingT[l] = 0.0f;
                g.to[l] = rest + (g.vel - U * dot(g.vel, U)) * ((1.0f - duty) * S / std::max(spd, 0.05f) * 1.1f);
            }
            if (!walking && !g.swing[l] && length(g.plant[l] - rest) > 0.35f * Lp && rnd() < dt * 8.0f * live) {  // nachsetzen
                g.swing[l] = true, g.from[l] = g.plant[l], g.to[l] = rest, g.swingT[l] = 0.0f;
            }
            if (g.swing[l]) {
                const float durS = walking ? (1.0f - duty) * S / std::max(spd, 0.05f) : 0.09f;
                g.swingT[l] += dt / std::max(0.03f, durS);
                if (walking) g.to[l] = rest + (g.vel - U * dot(g.vel, U)) * ((1.0f - duty) * S / std::max(spd, 0.05f) * 1.1f);
                if (g.swingT[l] >= 1.0f || (walking && !want)) {
                    g.swing[l] = false, g.plant[l] = g.to[l];
                    if (p == 3) stepped_ = true, stepPos_ = g.plant[l], stepWeight_ = 0.04f;
                }
            }
            const float t = g.swing[l] ? smN(g.swingT[l]) : 0.0f;
            footW = g.swing[l] ? g.from[l] + (g.to[l] - g.from[l]) * t + U * (0.025f * s * std::sin(kPiN * t)) : g.plant[l];
            if (raise) footW = footW + (C + (F * 0.07f + U * (0.09f * g.rise)) * s - footW) * g.rise;
        }
        // Ferse ueber der Klaue, Knie hoch ueber dem Koerper (ik2 von der Huefte zur Ferse)
        const vec3 toFoot = footW - C;
        const vec3 hz = nrmN(toFoot - U * dot(toFoot, U), F);
        const vec3 A = footW + U * (0.025f * s) - hz * (0.22f * Lp);
        const float l1 = length(B[(size_t)legJ(l, 1)] - B[(size_t)legJ(l, 0)]) * s, l2 = length(B[(size_t)legJ(l, 2)] - B[(size_t)legJ(l, 1)]) * s;
        vec3 K, Ae;
        ik2(C, A, l1, l2, nrmN(U + hz * 0.15f, U), K, Ae);
        J[(size_t)legJ(l, 0)] = C;
        J[(size_t)legJ(l, 1)] = K;
        J[(size_t)legJ(l, 2)] = Ae;
        J[(size_t)legJ(l, 3)] = Ae + nrmN(footW - Ae, -U) * (length(B[(size_t)legJ(l, 3)] - B[(size_t)legJ(l, 2)]) * s);
    }
    // Faden: haengt sie, geht er senkrecht hinauf (das Netz ist lang genug, um durch die Decke zu reichen)
    J[(size_t)DRAG_A] = J[OPI2];
    J[(size_t)DRAG_B] = J[OPI2] + vec3{0, 0, 0.25f};
    autoUps();
    // Oben-Richtungen aus der Lage des Koerpers (F, L, U) statt nur aus der Blickrichtung - an der Decke steht die Spinne
    // kopf; dann auf kuerzestem Weg zur jetzigen Richtung jedes Knochens
    for (int b = 1; b < B_DRAG; ++b) {
        const CBone& cb = sk_->bones[(size_t)b];
        auto R = [&](vec3 m) { return F * m.x + L * m.y + U * m.z; };
        const vec3 d0 = nrmN(R(B[cb.b] - B[cb.a]), F), d1 = nrmN(J[cb.b] - J[cb.a], d0);
        vec3 up = R(sk_->bindUp[(size_t)b]);
        const vec3 ax = cross(d0, d1);
        const float sn = length(ax), cs = dot(d0, d1);
        if (sn > 1e-6f) up = rotV(up, ax / sn, std::atan2(sn, cs));
        pose_.up[(size_t)b] = up;
    }
    pose_.up[0] = F;
    for (int b = 0; b < B_SAC0; ++b) pose_.s[(size_t)b] = s;
    pose_.s[(size_t)B_DRAG] = g.hang > 0.3f ? 1.0f : 0.001f;
    pose_.up[(size_t)B_DRAG] = vec3{1, 0, 0};
    headPos_ = J[EYES];
    headDir_ = F;
}

}  // namespace lim::anim
