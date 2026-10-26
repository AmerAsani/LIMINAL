#include "anim/creature.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace lim::anim {

// anim/skeleton.hpp (nicht eingebunden: dort heissen die Knochen des Menschen gleich)
void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {

// Baukasten: Gelenke und Knochen in der Reihenfolge der Aufzaehlungen
struct Builder {
    CreatureSkeleton s;
    explicit Builder(Entity e, int joints, int bones) {
        s.kind = e;
        s.bind.assign((size_t)joints, vec3{});
        s.bones.assign((size_t)bones, CBone{});
        s.bindUp.assign((size_t)bones, vec3{0, 0, 1});
    }
    void j(int i, vec3 p) { s.bind[(size_t)i] = p; }
    // Knochen i von Gelenk a nach b; up: Hinweis in der Ruhepose
    void b(int i, int a, int bb, int parent, float r, vec3 up, bool shadow = true) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)bb, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        s.bindUp[(size_t)i] = up;
    }
    vec3 dir(int a, int b) const {
        vec3 d = s.bind[(size_t)b] - s.bind[(size_t)a];
        float l = length(d);
        return l > 1e-6f ? d / l : vec3{1, 0, 0};
    }
};

// senkrecht zur Knochenrichtung in der Ebene von d und z ("vorn-oben" fuer Hals und Kopf)
vec3 sagittalUp(vec3 d) {
    vec3 u{-d.z, 0.0f, d.x};
    float l = length(u);
    return l > 1e-4f ? u / l : vec3{1, 0, 0};
}
// fuer (fast) senkrechte Knochen: vorn als Hinweis, sonst oben
vec3 upFor(vec3 d) { return std::fabs(d.z) > 0.8f ? vec3{1, 0, 0} : vec3{0, 0, 1}; }

// --- Smiler ------------------------------------------------------------------------------------
}  // namespace

namespace sm {
Eye smilerEye(int side) {
    // leicht ungleich: links etwas groesser, rechts steiler; bei beiden der aeussere Winkel hoeher
    const float y = side == 0 ? 0.118f : -0.112f, z = 1.915f;
    const float roll = side == 0 ? 0.12f : -0.17f;
    const float dy = y / kHeadR[1], dz = (z - kHeadC[2]) / kHeadR[2];
    const float x = kHeadC[0] + kHeadR[0] * std::sqrt(std::max(0.05f, 1.0f - dy * dy - dz * dz));
    Eye e;
    e.surf = {x, y, z};
    e.n = normalize(vec3{(x - kHeadC[0]) / (kHeadR[0] * kHeadR[0]), y / (kHeadR[1] * kHeadR[1]), (z - kHeadC[2]) / (kHeadR[2] * kHeadR[2])});
    const vec3 t = normalize(cross(vec3{0, 0, 1}, e.n)), u = cross(e.n, t);
    e.t = t * std::cos(roll) + u * std::sin(roll);
    e.u = u * std::cos(roll) - t * std::sin(roll);
    e.hinge = e.surf - e.n * kLidHinge;
    e.len = side == 0 ? 0.052f : 0.047f;
    e.r = side == 0 ? 0.0088f : 0.0078f;
    return e;
}
}  // namespace sm

namespace {

CreatureSkeleton makeSmiler() {
    using namespace sm;
    Builder k(Entity::Smiler, JCOUNT, BCOUNT);
    // V11 (ART_DIRECTION 5.2): eine Saeule tieferer Dunkelheit, das Gesicht in 1,7-2,0 m Hoehe
    k.j(ROOT, {0, 0, 0});
    k.j(BASE, {0.0f, 0, 0.34f});
    k.j(CORE, {0.0f, 0, 0.98f});
    k.j(UPPER, {0.02f, 0, 1.52f});
    k.j(FACE, {0.12f, 0, 1.82f});
    k.j(FACETIP, {0.24f, 0, 1.82f});
    k.j(JAWH, {-0.06f, 0, 1.80f});  // weit hinten: der Unterkiefer senkt sich fast senkrecht
    k.j(JAWTIP, {0.20f, 0, 1.72f});
    // die Lider schwingen um eine Achse hinter jedem Auge; der Knochen liegt laengs dieser Achse
    for (int sd = 0; sd < 2; ++sd) {
        const Eye e = smilerEye(sd);
        k.j(sd == 0 ? LIDH_L : LIDH_R, e.hinge);
        k.j(sd == 0 ? LIDT_L : LIDT_R, e.hinge + e.t * 0.05f);
    }
    // Zungen der Masse: steigen hinten von Schultern und Hinterkopf auf und ziehen nach hinten weg -
    // von vorn bleibt der Umriss eine Saeule (keine Hoerner, keine Ohren)
    static const float ang[kWisps] = {2.2f, 2.75f, 3.14f, -2.75f, -2.2f, 3.14f};
    static const float h0[kWisps] = {1.58f, 1.78f, 1.64f, 1.74f, 1.55f, 1.98f};
    for (int w = 0; w < kWisps; ++w) {
        const float c = std::cos(ang[w]), s = std::sin(ang[w]);
        const float r = w == 5 ? 0.1f : 0.2f;
        vec3 p{0.0f + c * r, s * r * 1.3f, h0[w]};
        k.j(wisp(w, 0), p);
        const float seg[3] = {0.15f, 0.13f, 0.11f};
        for (int i = 1; i < kWispJ; ++i) {
            p = p + normalize(vec3{c * 0.8f - 0.2f, s * 0.45f, 0.75f}) * seg[i - 1];
            k.j(wisp(w, i), p);
        }
    }
    const vec3 F{1, 0, 0};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_LOW, BASE, CORE, B_ROOT, 0.2f, F);
    k.b(B_MID, CORE, UPPER, B_LOW, 0.22f, F);
    k.b(B_HEAD, UPPER, FACE, B_MID, 0.2f, sagittalUp(k.dir(UPPER, FACE)));
    k.b(B_FACE, FACE, FACETIP, B_HEAD, 0.12f, {0, 0, 1}, false);
    k.b(B_JAW, JAWH, JAWTIP, B_HEAD, 0.08f, sagittalUp(k.dir(JAWH, JAWTIP)), false);
    for (int sd = 0; sd < 2; ++sd) {
        const Eye e = smilerEye(sd);
        const int h = sd == 0 ? LIDH_L : LIDH_R;
        k.b(sd == 0 ? B_LIDU_L : B_LIDU_R, h, h + 1, B_HEAD, 0.01f, e.u, false);
        k.b(sd == 0 ? B_LIDD_L : B_LIDD_R, h, h + 1, B_HEAD, 0.01f, e.u, false);
    }
    for (int w = 0; w < kWisps; ++w)
        for (int i = 0; i < 3; ++i)
            k.b(wispB(w, i), wisp(w, i), wisp(w, i + 1), i == 0 ? B_HEAD : wispB(w, i - 1), 0.05f - 0.012f * (float)i,
                upFor(k.dir(wisp(w, i), wisp(w, i + 1))), false);
    k.s.height = 2.15f;
    k.s.reach = 1.2f;
    k.s.center = {0.02f, 0, 1.15f};
    return k.s;
}

// --- Hound -------------------------------------------------------------------------------------
CreatureSkeleton makeHound() {
    using namespace hd;
    Builder k(Entity::Hound, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(PELVIS, {-0.40f, 0, 0.70f});
    k.j(SPINE1, {-0.18f, 0, 0.78f});
    k.j(SPINE2, {0.04f, 0, 0.83f});
    k.j(CHEST, {0.24f, 0, 0.82f});
    k.j(NECK1, {0.36f, 0, 0.785f});
    k.j(NECK2, {0.45f, 0, 0.72f});
    k.j(SKULL, {0.53f, 0, 0.64f});   // Mitte des Hirnschaedels - der Kopf haengt unter den Schultern
    k.j(SNOUT, {0.69f, 0, 0.59f});   // Gesicht / Oberkiefer vorn (menschlich flach)
    k.j(JAWH, {0.54f, 0, 0.56f});    // Kiefergelenk
    k.j(JAW, {0.68f, 0, 0.515f});    // Kinn
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        // Arme als Vorderbeine: Ellbogen nach hinten, die flache Hand nach vorn
        const vec3 sh{0.22f, sg * 0.16f, 0.80f}, el{0.10f, sg * 0.19f, 0.47f}, wr{0.28f, sg * 0.16f, 0.12f}, kn{0.38f, sg * 0.15f, 0.025f};
        k.j(front(SHOULDER_L, s), sh);
        k.j(front(ELBOW_L, s), el);
        k.j(front(WRIST_L, s), wr);
        k.j(front(KNUCK_L, s), kn);
        // fuenf lange Finger; der Daumen innen (zur Koerpermitte)
        const vec3 tips[5] = {{0.06f, -sg * 0.05f, -0.015f}, {0.13f, -sg * 0.024f, -0.018f}, {0.145f, 0.0f, -0.018f}, {0.135f, sg * 0.022f, -0.018f},
                              {0.11f, sg * 0.042f, -0.016f}};
        for (int f = 0; f < 5; ++f) k.j(front(D0_L, s) + f, kn + tips[f]);
        // Hinterbeine: Knie vorn, Ferse hoch erhoben, Ballen und drei Zehen
        const vec3 hp{-0.42f, sg * 0.11f, 0.68f}, kne{-0.12f, sg * 0.13f, 0.40f}, hk{-0.44f, sg * 0.12f, 0.17f}, ft{-0.34f, sg * 0.12f, 0.03f};
        k.j(hind(HIP_L, s), hp);
        k.j(hind(KNEE_L, s), kne);
        k.j(hind(HOCK_L, s), hk);
        k.j(hind(FOOT_L, s), ft);
        const vec3 toes[3] = {{0.08f, sg * 0.03f, -0.025f}, {0.095f, 0.0f, -0.025f}, {0.08f, -sg * 0.03f, -0.025f}};
        for (int t = 0; t < 3; ++t) k.j(hind(T0_L, s) + t, ft + toes[t]);
    }
    // Haar am Kopf: eine Straehne faellt vorn ueber das Gesicht (verdeckt das Maul), zwei an den Seiten
    {
        const vec3 S = k.s.bind[SKULL];
        const vec3 roots[3] = {S + vec3{0.06f, 0.0f, 0.075f}, S + vec3{0.0f, 0.06f, 0.06f}, S + vec3{0.0f, -0.06f, 0.06f}};
        // V11: die Stirnstraehne faellt in der Ruhepose schon fast so, wie sie haengt (weniger Biegung im Skinning)
        const vec3 steps[3][3] = {{{0.08f, 0.0f, -0.05f}, {0.05f, 0.0f, -0.13f}, {0.0f, 0.0f, -0.16f}},
                                  {{0.02f, 0.06f, -0.06f}, {0.03f, 0.04f, -0.14f}, {0.02f, 0.02f, -0.16f}},
                                  {{0.02f, -0.06f, -0.06f}, {0.03f, -0.04f, -0.14f}, {0.02f, -0.02f, -0.16f}}};
        for (int h = 0; h < kHeadHair; ++h) {
            vec3 p = roots[h];
            k.j(hairH(h, 0), p);
            for (int i = 0; i < 3; ++i) k.j(hairH(h, i + 1), p = p + steps[h][i]);
        }
    }
    // Maehne: Nacken, Widerrist, Rueckenmitte - je links und rechts, faellt seitlich herab
    {
        const float xs[3] = {0.36f, 0.16f, -0.06f}, zs[3] = {0.845f, 0.925f, 0.895f};
        for (int r = 0; r < 3; ++r)
            for (int s = 0; s < 2; ++s) {
                const float sg = s == 0 ? 1.0f : -1.0f;
                const int h = r * 2 + s;
                vec3 p{xs[r], sg * 0.03f, zs[r] - 0.02f};  // V11: Wurzeln in der Haut, nicht darueber
                k.j(hairB(h, 0), p);
                // V11: weicher Fall ueber die Flanke (kein Haken), etwas laenger
                k.j(hairB(h, 1), p = p + vec3{-0.05f, sg * 0.075f, -0.055f});
                k.j(hairB(h, 2), p + vec3{-0.04f, sg * 0.05f, -0.2f});
            }
    }
    const vec3 Z{0, 0, 1}, Y{0, 1, 0}, F{1, 0, 0};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_PELVIS, PELVIS, SPINE1, B_ROOT, 0.1f, Z);
    k.b(B_SPINE1, SPINE1, SPINE2, B_PELVIS, 0.1f, Z);
    k.b(B_SPINE2, SPINE2, CHEST, B_SPINE1, 0.13f, Z);
    k.b(B_NECK1, CHEST, NECK1, B_SPINE2, 0.075f, Z);
    k.b(B_NECK2, NECK1, NECK2, B_NECK1, 0.055f, Z);
    k.b(B_NECK3, NECK2, SKULL, B_NECK2, 0.05f, Z);
    k.b(B_HEAD, SKULL, SNOUT, B_NECK3, 0.08f, Z);
    k.b(B_JAW, JAWH, JAW, B_HEAD, 0.04f, sagittalUp(k.dir(JAWH, JAW)), false);
    for (int s = 0; s < 2; ++s) {
        k.b(frontB(B_SCAP_L, s), CHEST, front(SHOULDER_L, s), B_SPINE2, 0.07f, Z);
        k.b(frontB(B_UPPER_L, s), front(SHOULDER_L, s), front(ELBOW_L, s), frontB(B_SCAP_L, s), 0.042f, F);
        k.b(frontB(B_FORE_L, s), front(ELBOW_L, s), front(WRIST_L, s), frontB(B_UPPER_L, s), 0.032f, F);
        k.b(frontB(B_PALM_L, s), front(WRIST_L, s), front(KNUCK_L, s), frontB(B_FORE_L, s), 0.03f, Z);
        for (int d = 0; d < 5; ++d)
            k.b(frontB(B_D0_L, s) + d, front(KNUCK_L, s), front(D0_L, s) + d, frontB(B_PALM_L, s), 0.011f, Z, false);
        k.b(hindB(B_THIGH_L, s), hind(HIP_L, s), hind(KNEE_L, s), B_PELVIS, 0.06f, Y);
        k.b(hindB(B_SHIN_L, s), hind(KNEE_L, s), hind(HOCK_L, s), hindB(B_THIGH_L, s), 0.038f, Y);
        k.b(hindB(B_META_L, s), hind(HOCK_L, s), hind(FOOT_L, s), hindB(B_SHIN_L, s), 0.03f, Y);
        for (int t = 0; t < 3; ++t) k.b(hindB(B_T0_L, s) + t, hind(FOOT_L, s), hind(T0_L, s) + t, hindB(B_META_L, s), 0.012f, Z, false);
    }
    for (int h = 0; h < kHeadHair; ++h)
        for (int i = 0; i < 3; ++i)
            k.b(hairHB(h, i), hairH(h, i), hairH(h, i + 1), i == 0 ? B_HEAD : hairHB(h, i - 1), 0.03f, upFor(k.dir(hairH(h, i), hairH(h, i + 1))), i == 0);
    const int backParent[3] = {B_NECK1, B_SPINE2, B_SPINE1};
    for (int h = 0; h < kBackHair; ++h)
        for (int i = 0; i < 2; ++i)
            k.b(hairBB(h, i), hairB(h, i), hairB(h, i + 1), i == 0 ? backParent[h / 2] : hairBB(h, i - 1), 0.03f,
                upFor(k.dir(hairB(h, i), hairB(h, i + 1))), false);
    k.s.height = 0.95f;
    k.s.reach = 1.3f;
    k.s.center = {0.0f, 0, 0.55f};
    return k.s;
}

// --- Deathmoth ---------------------------------------------------------------------------------
CreatureSkeleton makeMoth() {
    using namespace mo;
    Builder k(Entity::Moth, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(HEAD, {0.042f, 0, 0.004f});
    k.j(PROB, {0.078f, 0, -0.022f});
    k.j(ABD0, {-0.03f, 0, -0.002f});
    k.j(ABD1, {-0.065f, 0, -0.006f});
    k.j(ABD2, {-0.10f, 0, -0.01f});
    k.j(ABDTIP, {-0.128f, 0, -0.012f});
    k.j(BRISTLE_L, {-0.162f, 0.011f, -0.012f});
    k.j(BRISTLE_R, {-0.162f, -0.011f, -0.012f});
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        k.j(s == 0 ? ANTB_L : ANTB_R, {0.056f, sg * 0.008f, 0.012f});
        k.j(s == 0 ? ANTT_L : ANTT_R, {0.098f, sg * 0.046f, 0.03f});
        // Ruhepose: die Fluegel flach ausgebreitet (so sitzen Motten an der Wand)
        k.j(s == 0 ? WF_L : WF_R, {0.012f, sg * 0.016f, 0.010f});
        k.j(s == 0 ? WFT_L : WFT_R, {0.045f, sg * 0.20f, 0.010f});
        k.j(s == 0 ? WH_L : WH_R, {-0.012f, sg * 0.014f, 0.006f});
        k.j(s == 0 ? WHT_L : WHT_R, {-0.135f, sg * 0.075f, 0.006f});
    }
    // Beine: Huefte an der Brust, Knie aussen hoch, Fuss auf der Flaeche 4 cm unter der Brust
    const float bx[3] = {0.018f, 0.0f, -0.016f}, by[3] = {0.010f, 0.013f, 0.011f}, fx[3] = {0.032f, 0.004f, -0.032f}, tx[3] = {0.05f, 0.008f, -0.052f};
    for (int l = 0; l < 6; ++l) {
        const int pair = l / 2;
        const float sg = (l % 2) == 0 ? 1.0f : -1.0f;
        const vec3 base{bx[pair], sg * by[pair], -0.016f};
        k.j(legJ(l, 0), base);
        k.j(legJ(l, 1), base + vec3{fx[pair] * 0.6f, sg * 0.03f, 0.012f});
        k.j(legJ(l, 2), base + vec3{fx[pair], sg * 0.055f, -0.016f});
        k.j(legJ(l, 3), base + vec3{tx[pair], sg * 0.066f, -0.024f});
    }
    const vec3 Z{0, 0, 1}, F{1, 0, 0};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_THORAX, ROOT, HEAD, B_ROOT, 0.026f, Z);
    k.b(B_HEAD, HEAD, PROB, B_THORAX, 0.016f, sagittalUp(k.dir(HEAD, PROB)), false);
    k.b(B_ABD1, ABD0, ABD1, B_THORAX, 0.022f, Z);
    k.b(B_ABD2, ABD1, ABD2, B_ABD1, 0.02f, Z);
    k.b(B_ABD3, ABD2, ABDTIP, B_ABD2, 0.014f, Z, false);
    k.b(B_BRISTLE_L, ABDTIP, BRISTLE_L, B_ABD3, 0.004f, Z, false);
    k.b(B_BRISTLE_R, ABDTIP, BRISTLE_R, B_ABD3, 0.004f, Z, false);
    k.b(B_ANT_L, ANTB_L, ANTT_L, B_THORAX, 0.006f, Z, false);
    k.b(B_ANT_R, ANTB_R, ANTT_R, B_THORAX, 0.006f, Z, false);
    k.b(B_WF_L, WF_L, WFT_L, B_THORAX, 0.03f, Z);
    k.b(B_WH_L, WH_L, WHT_L, B_THORAX, 0.03f, Z);
    k.b(B_WF_R, WF_R, WFT_R, B_THORAX, 0.03f, Z);
    k.b(B_WH_R, WH_R, WHT_R, B_THORAX, 0.03f, Z);
    for (int l = 0; l < 6; ++l)
        for (int i = 0; i < 3; ++i) k.b(legB(l, i), legJ(l, i), legJ(l, i + 1), i == 0 ? B_THORAX : legB(l, i - 1), 0.004f, Z, false);
    k.s.height = 0.12f;
    k.s.reach = 0.26f;
    k.s.center = {-0.02f, 0, 0};
    return k.s;
}

// --- Gestalt -----------------------------------------------------------------------------------
CreatureSkeleton makeGestalt() {
    using namespace ge;
    Builder k(Entity::Gestalt, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(LOW, {0, 0, 0.30f});
    k.j(MID, {0, 0, 0.85f});
    k.j(CHEST, {0.01f, 0, 1.36f});
    k.j(NECK, {0.02f, 0, 1.60f});
    k.j(HEAD, {0.03f, 0, 1.74f});
    k.j(TOP, {0.03f, 0, 1.92f});
    for (int w = 0; w < kWisps; ++w) {
        const float a = 0.785f + 1.5708f * (float)w, c = std::cos(a), s = std::sin(a);
        k.j(wisp(w, 0), {c * 0.11f, s * 0.11f, 0.40f});
        k.j(wisp(w, 1), {c * 0.16f, s * 0.16f, 0.21f});
        k.j(wisp(w, 2), {c * 0.21f, s * 0.21f, 0.06f});
    }
    const vec3 F{1, 0, 0};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_LOW, LOW, MID, B_ROOT, 0.16f, F);
    k.b(B_MID, MID, CHEST, B_LOW, 0.18f, F);
    k.b(B_CHEST, CHEST, NECK, B_MID, 0.17f, F);
    k.b(B_NECK, NECK, HEAD, B_CHEST, 0.06f, F);
    k.b(B_HEAD, HEAD, TOP, B_NECK, 0.1f, F);
    for (int w = 0; w < kWisps; ++w)
        for (int i = 0; i < 2; ++i) k.b(wispB(w, i), wisp(w, i), wisp(w, i + 1), i == 0 ? B_LOW : wispB(w, i - 1), 0.04f, F, false);
    k.s.height = 1.92f;
    k.s.reach = 1.0f;
    k.s.center = {0.02f, 0, 1.0f};
    return k.s;
}

// --- Nest --------------------------------------------------------------------------------------
CreatureSkeleton makeNest() {
    using namespace ns;
    Builder k(Entity::Nest, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(TOP, {0, 0, 2.2f});
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, {1, 0, 0}, false);
    k.b(B_BODY, ROOT, TOP, B_ROOT, 0.6f, {1, 0, 0}, false);
    k.s.height = 2.2f;
    k.s.reach = 1.5f;
    k.s.center = {0.05f, 0, 1.1f};
    return k.s;
}

}  // namespace

namespace cl {
vec3 shoulder(int a) {
    const vec3 d{std::cos(kArm[a].ang), std::sin(kArm[a].ang), 0.0f};
    return vec3{0, 0, kCoreZ + kArm[a].h} + d * (kCoreR * 0.82f);
}
}  // namespace cl

namespace {

// --- Buendel (V11) -----------------------------------------------------------------------------
// Ruhepose: der Kern liegt am Boden, die Arme stuetzen sich ringsum auf (Ellbogen hoch wie bei einer
// Spinne), Arm 0 liegt lang nach vorn; die Beine schleifen hinten nach
CreatureSkeleton makeClump() {
    using namespace cl;
    Builder k(Entity::Clump, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(CORE, {0, 0, kCoreZ});
    k.j(TOP, {-0.04f, 0, kCoreZ + kCoreR});
    k.j(MOUTH, {0.06f, 0, 0.1f});
    for (int a = 0; a < kArms; ++a) {
        const vec3 d{std::cos(kArm[a].ang), std::sin(kArm[a].ang), 0.0f};
        const vec3 S = shoulder(a);
        const float lu = upperLen(a), lf = foreLen(a);
        const vec3 T = vec3{S.x, S.y, 0.05f} + d * ((lu + lf) * 0.62f);
        vec3 E, W;
        ik2(S, T, lu, lf, vec3{0, 0, 1} + d * 0.3f, E, W);
        k.j(armJ(a, 0), S);
        k.j(armJ(a, 1), E);
        k.j(armJ(a, 2), W);
        k.j(armJ(a, 3), W + normalize(vec3{W.x - S.x, W.y - S.y, 0.0f}) * kHand - vec3{0, 0, 0.02f});
    }
    for (int l = 0; l < kLegs; ++l) {
        const float sg = l == 0 ? 1.0f : -1.0f;
        const vec3 H{-0.24f, sg * 0.14f, kCoreZ - 0.18f}, T{-0.95f, sg * 0.32f, 0.06f};
        vec3 K, Fo;
        ik2(H, T, kThigh, kShin, vec3{0, sg * 0.3f, 1.0f}, K, Fo);
        k.j(legJ(l, 0), H);
        k.j(legJ(l, 1), K);
        k.j(legJ(l, 2), Fo);
    }
    const vec3 F{1, 0, 0}, Z{0, 0, 1};
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, F, false);
    k.b(B_CORE, CORE, TOP, B_ROOT, 0.4f, F);
    for (int a = 0; a < kArms; ++a)
        for (int i = 0; i < 3; ++i) {
            const vec3 d = k.dir(armJ(a, i), armJ(a, i + 1));
            k.b(armB(a, i), armJ(a, i), armJ(a, i + 1), i == 0 ? B_CORE : armB(a, i - 1), i == 0 ? 0.06f : (i == 1 ? 0.045f : 0.035f),
                std::fabs(d.z) > 0.8f ? F : Z, i < 2);
        }
    for (int l = 0; l < kLegs; ++l)
        for (int i = 0; i < 2; ++i)
            k.b(legB(l, i), legJ(l, i), legJ(l, i + 1), i == 0 ? B_CORE : legB(l, 0), i == 0 ? 0.07f : 0.05f, Z, true);
    k.s.height = 0.85f;
    k.s.reach = 1.6f;
    k.s.center = {0.1f, 0, 0.45f};
    return k.s;
}

}  // namespace

const CreatureSkeleton& entitySkeleton(Entity e) {
    static const std::array<CreatureSkeleton, kEntityCount> all = {makeSmiler(), makeHound(), makeMoth(), makeGestalt(), makeNest(),
                                                                   makeClump()};
    return all[(size_t)std::min((int)e, kEntityCount - 1)];
}

const char* entityModel(Entity e) {
    switch (e) {
        case Entity::Smiler: return "ent_smiler";
        case Entity::Hound: return "ent_hound";
        case Entity::Moth: return "ent_moth";
        case Entity::Gestalt: return "ent_gestalt";
        case Entity::Nest: return "ent_nest";
        case Entity::Clump: return "ent_clump";
        default: return "ent_smiler";
    }
}

const char* entityName(Entity e) {
    switch (e) {
        case Entity::Smiler: return "Smiler";
        case Entity::Hound: return "Hound";
        case Entity::Moth: return "Deathmoth";
        case Entity::Gestalt: return "Gestalt";
        case Entity::Nest: return "Nest";
        case Entity::Clump: return "Buendel";
        default: return "?";
    }
}

mat4 boneFrame(vec3 a, vec3 b, vec3 up) {
    vec3 x = b - a;
    float l = length(x);
    x = l > 1e-6f ? x / l : vec3{1, 0, 0};
    vec3 z = up - x * dot(up, x);
    float zl = length(z);
    if (zl < 1e-5f) {
        z = std::fabs(x.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0};
        z = normalize(z - x * dot(z, x));
    } else {
        z = z / zl;
    }
    vec3 y = cross(z, x);
    mat4 m;
    m.c[0] = vec4(x, 0.0f);
    m.c[1] = vec4(y, 0.0f);
    m.c[2] = vec4(z, 0.0f);
    m.c[3] = vec4(a, 1.0f);
    return m;
}

void creatureFrames(const CreatureSkeleton& s, const CreaturePose& p, std::vector<mat4>& out) {
    out.resize(s.bones.size());
    for (size_t i = 0; i < s.bones.size(); ++i) {
        const CBone& b = s.bones[i];
        if (b.a == b.b) {
            vec3 f = p.up[i];
            f.z = 0.0f;
            f = length(f) > 1e-5f ? normalize(f) : vec3{1, 0, 0};
            out[i] = boneFrame(p.j[b.a], p.j[b.a] + f, vec3{0, 0, 1});
        } else {
            out[i] = boneFrame(p.j[b.a], p.j[b.b], p.up[i]);
        }
    }
}

CreaturePose bindPose(const CreatureSkeleton& s) {
    CreaturePose p;
    p.j = s.bind;
    p.up = s.bindUp;
    return p;
}

}  // namespace lim::anim
