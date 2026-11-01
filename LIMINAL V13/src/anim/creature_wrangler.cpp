// V13.3: Wrangler (Entity 75, KREATUR_Wrangler.md) - Skelett und Bewegung.
//
// Ein Wesen von 16 bis 145 km Laenge sieht man nie ganz. Zu sehen ist ein Abschnitt des Leibs, der durch Boden, Wand oder
// Decke zieht: Ring um Ring, durchscheinend, darin Eingeweide und Verschlucktes; er dreht sich dabei langsam wie ein Bohrer
// (die Seite: so graebt er sich durch hartes Material) und zieht sich wie ein Regenwurm in Wellen zusammen. Bricht der Kopf
// heraus, folgt ihm ein Stueck Hals aus derselben Flaeche.
//
// Eingaben (Presence -> CreatureAnimator::Input; im Koop dieselben Kanaele, alle 0..1 bzw. -1..1):
//   pos, yaw    Mitte der Bahn am Boden; Richtung der Bahn (der Leib zieht von -x nach +x)
//   lean        halbe Spannweite der Bahn S = 16 m * lean (an beiden Enden liegt die Bahn 3,6 m tief)
//   tilt        Hoehe der Leibesachse ueber dem Boden H = 4 m * tilt (< 0: der Bogen haengt von der Decke herab)
//   crouch      halbe Laenge des geraden Stuecks W = 10 m * crouch (quer durch einen Gang: es reicht in beide Waende)
//   wave        halbe Gangbreite 5 m * wave: dort sitzen die Kraenze an den Waenden (0: am Boden bzw. an der Decke)
//   hang        wie weit der Leib heraus ist (0 ganz in der Flaeche .. 1 volle Hoehe)
//   fly         Tempo des Leibs 4 m/s * fly; alert haelt ihn an (gespannt), shiver: Zittern, growl: Beben
//   reach       Kopf heraus (0 in der Flaeche .. 1 heraus); strike: Zuschnappen (Phase); jaw: Maul bzw. Zangen auf
//   fwd, bait   Durchbruchstelle des Kopfs pos + fwd * 20 m * bait; up: Normale der Flaeche dort
//   perceive    der Kopf sucht (pendelt); lookAt / reachAt: wohin er sieht / zuschnappt
//   plea        > 0,5: Jungtiere am Ast (Variante "brood"): der Ast sitzt bei pos an der Wand (Normale: up), sie winden sich
#include <algorithm>
#include <array>
#include <cmath>

#include "anim/creature_anim.hpp"

namespace lim::anim {

namespace wg {

CreatureSkeleton makeWrangler() {
    CreatureSkeleton s;
    s.kind = Entity::Wrangler;
    s.bind.assign(JCOUNT, vec3{});
    s.bones.assign(BCOUNT, CBone{});
    s.bindUp.assign(BCOUNT, vec3{0, 0, 1});
    auto bone = [&](int i, int a, int b, int parent, float r, vec3 up, bool shadow) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)b, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        s.bindUp[(size_t)i] = up;
    };
    const vec3 X{1, 0, 0}, Z{0, 0, 1};
    s.bind[ROOT] = {0, 0, 0};
    bone(B_ROOT, ROOT, ROOT, -1, 0.0f, X, false);
    // Ringe: jeder ein starrer Abschnitt von 0,55 m (Ruhepose), die Achse in 1 m Hoehe
    const float x0 = -0.5f * (float)kRings * kRingLen;
    for (int r = 0; r < kRings; ++r) {
        s.bind[(size_t)ringA(r)] = {x0 + (float)r * kRingLen, 0.0f, kAxisZ};
        s.bind[(size_t)ringA(r) + 1] = {x0 + (float)(r + 1) * kRingLen, 0.0f, kAxisZ};
        bone(B_RING0 + r, ringA(r), ringA(r) + 1, B_ROOT, 0.42f, Z, true);
    }
    // Verschlucktes: im Leib, etwas unter der Achse (Ruhepose bei x = -6, -2, 2, 6)
    for (int k = 0; k < kItems; ++k) {
        const float x = -6.0f + 4.0f * (float)k;
        s.bind[(size_t)itemA(k)] = {x, 0.0f, kAxisZ - 0.08f};
        s.bind[(size_t)itemA(k) + 1] = {x + 0.3f, 0.0f, kAxisZ - 0.08f};
        bone(B_ITEM0 + k, itemA(k), itemA(k) + 1, B_ROOT, 0.1f, Z, false);
    }
    // Hals (6 x 0,5 m) und Kopf (1,05 m), Unterkiefer (Maennchen), zwei Zangen (Weibchen)
    for (int i = 0; i <= kNeck; ++i) s.bind[(size_t)NECK0 + i] = {9.5f + (float)i * kNeckLen, 0.0f, kAxisZ};
    for (int i = 0; i < kNeck; ++i) bone(B_NECK0 + i, NECK0 + i, NECK0 + i + 1, i == 0 ? B_ROOT : B_NECK0 + i - 1, 0.4f, Z, true);
    const vec3 hb = s.bind[(size_t)NECK0 + kNeck];
    s.bind[SNOUT] = hb + vec3{1.05f, 0, 0};
    bone(B_SKULL, NECK0 + kNeck, SNOUT, B_NECK0 + kNeck - 1, 0.45f, Z, true);
    s.bind[JAWH] = hb + vec3{0.12f, 0, -0.16f};
    s.bind[JAWTIP] = hb + vec3{0.98f, 0, -0.36f};
    bone(B_JAW, JAWH, JAWTIP, B_SKULL, 0.2f, Z, false);
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd == 0 ? 1.0f : -1.0f;
        const int a = sd == 0 ? PINC_L0 : PINC_R0;
        s.bind[(size_t)a] = hb + vec3{0.82f, g * 0.30f, -0.16f};
        s.bind[(size_t)a + 1] = hb + vec3{1.72f, g * 0.17f, -0.22f};
        bone(sd == 0 ? B_PINC_L : B_PINC_R, a, a + 1, B_SKULL, 0.06f, Z, false);
    }
    // Kraenze: flach am Boden (Ruhepose bei x = -12, -15, -18), der Knochen zeigt entlang der Normalen
    for (int c = 0; c < kCollars; ++c) {
        const vec3 p{-12.0f - 3.0f * (float)c, 0.0f, 0.0f};
        s.bind[(size_t)colA(c)] = p;
        s.bind[(size_t)colA(c) + 1] = p + Z * 0.3f;
        bone(B_COL0 + c, colA(c), colA(c) + 1, B_ROOT, 0.0f, X, false);
    }
    s.height = 2.2f;
    s.reach = 13.0f;  // die Bahn ist bis 16 m lang, Kopf und Kraenze liegen weit von der Mitte
    s.center = {0, 0, kAxisZ};
    return s;
}

}  // namespace wg

namespace {
constexpr float kTau = 6.2831853f;
float sat(float x) { return std::clamp(x, 0.0f, 1.0f); }
float smoothstepF(float a, float b, float x) {
    const float t = sat((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
vec3 nrm(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
// gleitende Feder (wie creature_anim.cpp)
void springW(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
float hashF(int i) {
    unsigned h = (unsigned)i * 2654435761u + 0x9E3779B9u;
    h ^= h >> 15, h *= 2246822519u, h ^= h >> 13;
    return (float)(h & 0xFFFFFF) / 16777216.0f;
}

// Bahn als Folge von Punkten gleicher Parameterschritte mit Bogenlaenge; ausserhalb geradeaus weiter
struct Track {
    static constexpr int N = 129;
    std::array<vec3, N> p;
    std::array<float, N> s;
    float len = 0.0f;
    void finish() {
        s[0] = 0.0f;
        for (int i = 1; i < N; ++i) s[(size_t)i] = s[(size_t)i - 1] + length(p[(size_t)i] - p[(size_t)i - 1]);
        len = s[N - 1];
    }
    vec3 tanAt(int i) const {
        const int a = std::max(0, i - 1), b = std::min(N - 1, i + 1);
        return nrm(p[(size_t)b] - p[(size_t)a], {1, 0, 0});
    }
    vec3 at(float u, vec3& tan) const {
        if (u <= 0.0f) {
            tan = tanAt(0);
            return p[0] + tan * u;
        }
        if (u >= len) {
            tan = tanAt(N - 1);
            return p[N - 1] + tan * (u - len);
        }
        int lo = 0, hi = N - 1;
        while (hi - lo > 1) {
            const int m = (lo + hi) / 2;
            (s[(size_t)m] <= u ? lo : hi) = m;
        }
        const float seg = std::max(1e-5f, s[(size_t)hi] - s[(size_t)lo]), k = (u - s[(size_t)lo]) / seg;
        tan = nrm(tanAt(lo) * (1.0f - k) + tanAt(hi) * k, {1, 0, 0});
        return p[(size_t)lo] + (p[(size_t)hi] - p[(size_t)lo]) * k;
    }
};

// Drehung um eine Einheitsachse (Rodrigues)
vec3 rotAxis(vec3 v, vec3 axis, float a) {
    const float c = std::cos(a), s = std::sin(a);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
}  // namespace

void CreatureAnimator::updateWrangler(float dt, const Input& in) {
    using namespace wg;
    auto& J = pose_.j;
    auto& Up = pose_.up;
    const vec3 F{std::cos(yaw_), std::sin(yaw_), 0.0f}, Lw{-F.y, F.x, 0.0f}, U{0, 0, 1};
    const vec3 deep = pos_ - U * 9.0f;  // verborgen: tief unter allem
    for (vec3& j : J) j = deep;
    for (size_t b = 0; b < Up.size(); ++b) Up[b] = sk_->bindUp[b];
    J[ROOT] = pos_;
    Up[B_ROOT] = F;

    // --- Jungtiere am Ast (Variante brood): der Ast an der Wand bei pos, Normale up; die Jungen haengen und winden sich ---
    if (in.plea > 0.5f) {
        const vec3 n = nrm(in.up, F);
        const vec3 side = nrm(cross(U, n), Lw);
        springW(light_, lightV_, in.light, 2.0f, dt);
        springW(perc_, percV_, std::max(in.alert, in.perceive), 2.5f, dt);
        const float agit = sat(std::max(light_, perc_));
        // Ast: Kranz 0 an der Wand (flach auf der Wandflaeche)
        J[colA(0)] = pos_ + n * 0.004f;
        J[colA(0) + 1] = J[colA(0)] + n * 0.3f;
        Up[B_COL0] = U;
        // sechs Junge zu je zwei Ringknochen: haengen vom Ast herab, winden sich (unruhig im Licht oder wenn sich etwas naehert)
        for (int y = 0; y < 6; ++y) {
            const float h = hashF(y * 7 + 3), g = hashF(y * 13 + 5);
            const vec3 root = pos_ + n * (0.12f + 0.1f * g) + side * ((float)y - 2.5f) * 0.16f + U * (0.25f * h - 0.05f);
            const float ph = time_ * (1.6f + 2.6f * agit + 0.4f * h) + (float)y * 1.7f;
            const float sw = (0.35f + 0.5f * agit) * std::sin(ph), sw2 = (0.4f + 0.6f * agit) * std::sin(ph * 1.3f + 1.1f);
            const vec3 d1 = nrm(-U * 1.0f + n * (0.55f + 0.2f * std::sin(ph * 0.7f)) + side * sw * 0.6f, -U);
            const vec3 m = root + d1 * 0.36f;
            const vec3 d2 = nrm(d1 + n * (0.55f * sw2) + side * (0.8f * sw2) - U * 0.25f, -U);  // der schwere Kopf pendelt und kruemmt sich
            // Ruhepose (wrangler_brood.py): das Junge liegt von x0 + 0,19 bis x0 + 0,91 m - Knochen 2y traegt die ersten 0,36 m
            J[(size_t)ringA(2 * y)] = root - d1 * 0.19f, J[(size_t)ringA(2 * y) + 1] = m;
            J[(size_t)ringA(2 * y + 1)] = m, J[(size_t)ringA(2 * y + 1) + 1] = m + d2 * 0.36f;
            Up[(size_t)B_RING0 + 2 * y] = nrm(cross(d1, side), n);
            Up[(size_t)B_RING0 + 2 * y + 1] = nrm(cross(d2, side), n);
        }
        return;
    }

    // --- Leib ---------------------------------------------------------------------------------------------------
    const float S = std::max(2.5f, 16.0f * in.lean), H = 4.0f * in.tilt, W = std::clamp(10.0f * in.crouch, 0.0f, S - 1.6f);
    const float hw = 5.0f * in.wave;
    const float sgn = H < 0.0f ? -1.0f : 1.0f;
    const float base = H < 0.0f ? std::min(in.clearance, 12.0f) : 0.0f;  // Boden bzw. Decke (relativ zu pos)
    const float D = 3.6f;
    springW(jaw_, jawV_, in.hang, 2.6f, dt);  // jaw_ dient hier als Hoehe des Leibs (bricht in gut einer Sekunde heraus)
    const float pres = sat(jaw_);
    const float Hc = -1.8f + (std::fabs(H) + 1.8f) * pres;
    springW(crouch_, crouchV_, in.alert, 3.0f, dt);  // angehalten (gespannt)
    springW(growl_, growlV_, in.growl, 4.0f, dt);
    const float halt = sat(crouch_);
    flow_ += dt * 4.0f * in.fly * (1.0f - 0.97f * halt);
    // seitlicher Schwung der Bahn (der Leib sucht sich seinen Weg), langsam veraenderlich
    const float swayT = 0.18f * std::sin(time_ * 0.21f) + 0.1f * std::sin(time_ * 0.13f + 2.0f);
    Track tr;
    for (int i = 0; i < Track::N; ++i) {
        const float x = -S + 2.0f * S * (float)i / (float)(Track::N - 1);
        const float q = smoothstepF(W, S, std::fabs(x));
        const float zz = Hc * (1.0f - q) - D * q;
        const float y = (0.32f * std::sin(x * 0.45f + 0.7f) + swayT) * (1.0f - q);
        tr.p[(size_t)i] = pos_ + F * x + Lw * y + U * (base + sgn * zz);
    }
    tr.finish();
    // Ringe: Abstand 0,5 m (die Ringe greifen ineinander), die Kette mittig auf der Bahn; jeder Ring springt am Ende der
    // Kette (tief im Boden) an ihren Anfang zurueck
    const float d = 0.5f, chain = (float)kRings * d, c0 = (tr.len - chain) * 0.5f;
    const float quake = sat(growl_), shiv = sat(in.shiver) + 0.6f * halt;
    std::array<vec3, kRings> rc{};
    for (int r = 0; r < kRings; ++r) {
        float u = std::fmod((float)r * d + flow_, chain);
        if (u < 0.0f) u += chain;
        u += c0;
        // Peristaltik: Wellen von Stauchen und Strecken laufen nach vorn
        u += 0.07f * std::sin(kTau * u / 3.4f - time_ * 2.1f) * (1.0f - halt);
        vec3 T;
        vec3 P = tr.at(u, T);
        const vec3 side = nrm(Lw - T * dot(Lw, T), Lw), up0 = cross(T, side);
        // Drehung wie ein Bohrer: jeder Ring dreht sich mit seinem Weg weiter (dazu ein fester Versatz je Ring)
        const float roll = u * 0.55f + (float)r * 2.399f;
        const vec3 up = up0 * std::cos(roll) + side * std::sin(roll);
        // Beben und Zittern (angehalten bebt der Leib vor Spannung)
        const float jt = time_ * 37.0f + (float)r * 1.3f;
        P += (up0 * std::sin(jt) + side * std::cos(jt * 1.21f)) * (0.012f * quake + 0.016f * shiv);
        rc[(size_t)r] = P;
        J[(size_t)ringA(r)] = P - T * (kRingLen * 0.5f);
        J[(size_t)ringA(r) + 1] = P + T * (kRingLen * 0.5f);
        Up[(size_t)B_RING0 + r] = up;
    }
    // Verschlucktes: zieht einmal vorbei (kein Ruecksprung); danach tief geparkt
    for (int k = 0; k < kItems; ++k) {
        const float u = c0 + flow_ - 4.0f - 6.5f * (float)k;
        if (u > tr.len + 1.5f || u < -6.0f) continue;
        vec3 T;
        const vec3 P = tr.at(u, T);
        const vec3 side = nrm(Lw - T * dot(Lw, T), Lw), up0 = cross(T, side);
        const float tumble = u * 0.31f + (float)k * 1.7f;
        const vec3 dir = nrm(T * std::cos(0.5f * std::sin(tumble)) + side * std::sin(0.5f * std::sin(tumble)), T);
        const vec3 a = P - up0 * 0.1f - dir * 0.15f;
        J[(size_t)itemA(k)] = a;
        J[(size_t)itemA(k) + 1] = a + dir * 0.3f;
        Up[(size_t)B_ITEM0 + k] = up0 * std::cos(tumble) + side * std::sin(tumble);
    }
    // Kraenze, wo der Leib die Flaeche durchstoesst: an den Waenden (Gang) oder am Boden / an der Decke
    {
        const float rise = smoothstepF(0.05f, 0.35f, pres);
        for (int c = 0; c < 2; ++c) {
            const float side = c == 0 ? -1.0f : 1.0f;
            vec3 at, n;
            bool ok = false;
            if (hw > 0.05f) {
                // Waende bei x = -hw und +hw; die Normale zeigt in den Gang
                const int i = std::clamp((int)std::lround((side * hw + S) / (2.0f * S) * (float)(Track::N - 1)), 0, Track::N - 1);
                at = pos_ + F * (side * hw) + U * (tr.p[(size_t)i].z - pos_.z);
                n = F * -side;
                ok = tr.p[(size_t)i].z - pos_.z > 0.15f;
            } else {
                // die Bahn kreuzt die Flaeche (Boden oder Decke): erste Stelle von der Mitte aus, wo die Achse hindurchgeht
                const int mid = Track::N / 2;
                for (int i = mid; i >= 0 && i < Track::N && !ok; i += (int)side) {
                    const float z = (tr.p[(size_t)i].z - pos_.z - base) * sgn;
                    if (z <= 0.0f) {
                        at = vec3{tr.p[(size_t)i].x, tr.p[(size_t)i].y, pos_.z + base};
                        ok = i != mid;
                    }
                }
                n = U * sgn;
            }
            if (!ok) continue;
            const vec3 a = at + n * (0.006f - 0.3f * (1.0f - rise));
            J[(size_t)colA(c)] = a;
            J[(size_t)colA(c) + 1] = a + n * 0.3f;
            Up[(size_t)B_COL0 + c] = nrm(F - n * dot(F, n), Lw);
        }
    }

    // --- Hals und Kopf ------------------------------------------------------------------------------------------
    springW(reach_, reachV_, in.reach, 9.0f, dt);
    springW(lean_, leanV_, in.strike, 16.0f, dt);  // lean_ dient hier als Zuschnappen
    springW(perc_, percV_, in.perceive, 2.0f, dt);
    springW(talk_, talkV_, in.jaw, 7.0f, dt);  // talk_ dient hier als Maul / Zangen
    const float out = sat(reach_), lunge = sat(lean_), mouth = sat(talk_);
    if (out < 0.004f) return;
    const vec3 Hp = pos_ + nrm(in.fwd, F) * (20.0f * in.bait);
    const vec3 n = nrm(in.up, U);
    // der Kranz an der Durchbruchstelle zeigt sich, sobald es dort arbeitet (Risse vor dem Ausbruch)
    {
        const float k = smoothstepF(0.0f, 0.12f, out);
        const vec3 a = Hp + n * (0.008f - 0.32f * (1.0f - k));
        J[(size_t)colA(2)] = a;
        J[(size_t)colA(2) + 1] = a + n * 0.3f;
        Up[(size_t)B_COL0 + 2] = nrm(std::fabs(n.z) > 0.8f ? F : U - n * dot(U, n), F);
    }
    vec3 Hb = Hp + n * (-1.95f + 3.1f * out);  // Schaedelbasis
    const vec3 tgt = in.reachAt;
    vec3 dT = nrm(tgt - Hb, n);
    if (lunge > 0.001f) {
        const vec3 hl = tgt - dT * 1.25f;  // die Schnauze haelt knapp vor dem Ziel
        Hb = Hb + (hl - Hb) * lunge;
        const vec3 off = Hb - Hp;
        const float l = length(off);
        if (l > 3.7f) Hb = Hp + off * (3.7f / l);  // der Hals reicht weit (6 x 0,5 m aus der Flaeche)
        dT = nrm(tgt - Hb, dT);
    }
    // Blick: aus der Flaeche heraus, dann zum Ziel; sucht er, pendelt der Kopf
    const float look = sat(out * 1.6f - 0.3f);
    vec3 fH = nrm(n * (1.0f - look) + dT * look, n);
    if (perc_ > 0.01f) fH = nrm(rotAxis(fH, U, perc_ * 0.45f * std::sin(time_ * 1.3f)) + U * (0.08f * perc_ * std::sin(time_ * 0.9f)), fH);
    const vec3 uH = nrm(U - fH * dot(U, fH), std::fabs(fH.z) > 0.9f ? nrm(-n + F * 0.01f, F) : U);
    const vec3 sH = cross(uH, fH);
    // Hals: von tief in der Flaeche (unter einer Wand taucht er in den Boden) heraus bis zur Schaedelbasis
    {
        const vec3 q0 = Hp - n * 1.1f - U * (std::fabs(n.z) > 0.8f ? 1.4f : 2.6f), q1 = Hp - n * 0.35f, q2 = Hb - fH * 1.0f, q3 = Hb;
        constexpr int M = 48;
        std::array<vec3, M + 1> bz;
        std::array<float, M + 1> bs;
        for (int i = 0; i <= M; ++i) {
            const float t = (float)i / (float)M, u = 1.0f - t;
            bz[(size_t)i] = q0 * (u * u * u) + q1 * (3.0f * u * u * t) + q2 * (3.0f * u * t * t) + q3 * (t * t * t);
            bs[(size_t)i] = i ? bs[(size_t)i - 1] + length(bz[(size_t)i] - bz[(size_t)i - 1]) : 0.0f;
        }
        const float L = bs[M];
        for (int i = 0; i <= kNeck; ++i) {
            const float want = L - (float)(kNeck - i) * kNeckLen;
            vec3 p;
            if (want <= 0.0f) {
                p = bz[0] + nrm(bz[0] - bz[1], -n) * (-want);
            } else {
                int k = 1;
                while (k < M && bs[(size_t)k] < want) ++k;
                const float t = (want - bs[(size_t)k - 1]) / std::max(1e-5f, bs[(size_t)k] - bs[(size_t)k - 1]);
                p = bz[(size_t)k - 1] + (bz[(size_t)k] - bz[(size_t)k - 1]) * t;
            }
            J[(size_t)NECK0 + i] = p;
        }
        for (int i = 0; i < kNeck; ++i) {
            const vec3 T = nrm(J[(size_t)NECK0 + i + 1] - J[(size_t)NECK0 + i], fH);
            const float w = (float)(i + 1) / (float)kNeck;
            Up[(size_t)B_NECK0 + i] = nrm(uH * w + n * (1.0f - w) - T * dot(uH * w + n * (1.0f - w), T), uH);
        }
    }
    J[(size_t)NECK0 + kNeck] = Hb;
    J[SNOUT] = Hb + fH * 1.05f;
    Up[B_SKULL] = uH;
    // Unterkiefer (Maennchen): klappt um das Gelenk hinten unter dem Laecheln weit herab
    {
        const vec3 jh = Hb + fH * 0.12f - uH * 0.16f;
        const vec3 rest = nrm(fH * 0.86f - uH * 0.2f, fH);
        const float a = mouth * 0.95f + 0.05f * std::sin(time_ * 2.0f) * perc_;
        const vec3 dj = rotAxis(rest, sH, a);
        J[JAWH] = jh;
        J[JAWTIP] = jh + dj * 0.88f;
        Up[B_JAW] = rotAxis(uH, sH, a);
    }
    // Zangen (Weibchen): spreizen sich mit dem Maul, schlagen beim Zuschnappen zusammen
    for (int sd = 0; sd < 2; ++sd) {
        const float g = sd == 0 ? 1.0f : -1.0f;
        const int a = sd == 0 ? PINC_L0 : PINC_R0;
        const vec3 p0 = Hb + fH * 0.82f + sH * (g * 0.30f) - uH * 0.16f;
        const float spread = 0.12f + 0.75f * mouth - 0.85f * lunge * mouth;
        const vec3 rest = nrm(fH * 0.9f - sH * (g * 0.13f) - uH * 0.06f, fH);
        const vec3 dir = rotAxis(rest, uH, g * spread);
        J[(size_t)a] = p0;
        J[(size_t)a + 1] = p0 + dir * 0.9f;
        Up[sd == 0 ? B_PINC_L : B_PINC_R] = uH;
    }
}

}  // namespace lim::anim
