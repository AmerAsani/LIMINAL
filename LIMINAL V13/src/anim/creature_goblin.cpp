// V13.3: Plague Goblins (Entity 25, Wikidot entity-25 Rev. 28; KREATUR_PlagueGoblins.md) - Skelett und Bewegung.
//
// Katzengrosse Mischwesen aus Vogel und Katze (die Seite: "feline-avian", halb zweibeinig, 3,6-7,7 kg) in Umhang und Maske.
// Innen wie ein Vogel gebaut (Luftsaecke, hohle Knochen) - darum der Vogel in der Bewegung:
//   Gehen      kurze, schnelle Schritte auf den Zehen (die Ferse steht hoch), der Kopf bleibt im Raum stehen und ruckt dann
//              nach vorn (wie bei Tauben und Kraehen); der Umhang wippt nach
//   Huschen    auf allen vieren (die langen Arme werden Vorderbeine), Galopp mit Flugphase; Umhang und Schwanz fliegen
//              nach hinten. Die Torwaechterin huscht nie - sie laeuft aufrecht, den Stock in der Hand
//   Ruhe       Kopfrucke in alle Richtungen (Sakkaden), Ohren zucken einzeln, die Krallen reiben aneinander
//   Wahrnehmen erstarrt, Ohren steil, der Kopf schnellt herum und legt sich schief (Eule)
//   Licht      Arm vor die Augen, duckt sich, der Kopf weicht aus
//   Wuehlen    haengt am Ruecken des Opfers (Wurzel dort), die Krallen graben im Takt
//   Beute      haelt sie hoch, dann verschwindet sie im Umhang (Massstab 0) - die Seite: verschwindet spurlos
//   Drohen     die Maske klappt hoch (Scharnier an der Stirn): darunter Kraehenschnabel mit Saegezaehnen, weit offen;
//              die Arme spreizen den Umhang (doppelte Silhouette), Ohren flach, der Kiefer zittert (Fauchen)
//   Angriff    duckt sich, springt in einem Bogen auf das Ziel (strike 0..1), Krallen voran, faellt zurueck
//   Tod        kippt auf die Seite, Glieder zucken, die Maske rutscht ab und bleibt daneben liegen
//   Portal     springt hinein (schrumpft zur Mitte) bzw. heraus; das Portal selbst (Variante 3): Oval mit Sandstroemen
// extra_ = {Variante, Zustand (gb::State), Zeit im Zustand (s)}; in.fly: auf allen vieren (Portal: Oeffnung 0..1);
// in.growl: Drohen; in.bait: Beute in der Klaue; in.strike: Sprungphase; in.wave: Stock hoch (Torwaechterin);
// in.plea: bietet an (Handel); in.shiver: Zucken (tot); in.lean: um die Ecke spaehen.
#include <algorithm>
#include <cmath>

#include "anim/creature_anim.hpp"

namespace lim::anim {

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end);

namespace {
constexpr float kPiG = 3.14159265f, kTauG = 6.2831853f;
float smG(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float c01(float t) { return std::clamp(t, 0.0f, 1.0f); }
float mixG(float a, float b, float t) { return a + (b - a) * t; }
vec3 mixV(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
vec3 rotG(vec3 v, vec3 axis, float ang) {
    const float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1.0f - c));
}
vec3 nrmG(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
void sprG(float& x, float& v, float target, float omega, float dt) {
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
void sprGV(vec3& x, vec3& v, vec3 t, float omega, float dt) {
    sprG(x.x, v.x, t.x, omega, dt);
    sprG(x.y, v.y, t.y, omega, dt);
    sprG(x.z, v.z, t.z, omega, dt);
}
float wrapG(float a) { return std::remainder(a, kTauG); }
float hashG(unsigned& s) {
    s = s * 1664525u + 1013904223u;
    return (float)(s >> 8) / 16777216.0f;
}
float fracG(float x) { return x - std::floor(x); }
vec3 yawDir(float y) { return {std::cos(y), std::sin(y), 0.0f}; }

struct GBld {
    CreatureSkeleton s;
    GBld(Entity e, int joints, int bones) {
        s.kind = e;
        s.bind.assign((size_t)joints, vec3{});
        s.bones.assign((size_t)bones, CBone{});
        s.bindUp.assign((size_t)bones, vec3{0, 0, 1});
    }
    void j(int i, vec3 p) { s.bind[(size_t)i] = p; }
    void b(int i, int a, int bb, int parent, float r, bool shadow = true) {
        CBone& x = s.bones[(size_t)i];
        x.a = (std::uint8_t)a, x.b = (std::uint8_t)bb, x.parent = (std::int8_t)parent, x.radius = r, x.shadow = shadow;
        const vec3 d = nrmG(s.bind[(size_t)bb] - s.bind[(size_t)a], {1, 0, 0});
        s.bindUp[(size_t)i] = a == bb ? vec3{1, 0, 0} : (std::fabs(d.z) > 0.8f ? vec3{1, 0, 0} : vec3{0, 0, 1});
    }
};

// Ruhepose (Henchman, Massstab 1) - Wesen und Portal
constexpr float kThigh = 0.082f, kShin = 0.109f, kTarsus = 0.067f, kToe = 0.06f;
constexpr float kUpper = 0.106f, kFore = 0.090f, kHand = 0.054f;
constexpr float kSeg[4] = {0.097f, 0.095f, 0.068f, 0.061f};  // Becken-Ruecken, Ruecken-Brust, Brust-Hals, Hals-Kopf
vec3 strandDir(int k) { return {std::cos(gb::kStrandAng[k]), std::sin(gb::kStrandAng[k]), 0.0f}; }
vec3 portalCenter() { return {0.0f, 0.0f, 0.05f + gb::kPortalH * 0.5f}; }
vec3 ringPt(float a, float grow) {  // Punkt auf dem Rand des Portals (lokal: y quer, z hoch)
    return portalCenter() + vec3{0.0f, std::cos(a) * gb::kPortalW * 0.5f * grow, std::sin(a) * gb::kPortalH * 0.5f * grow};
}

struct Strand {
    vec3 p[3]{}, q[3]{};
    float len[3]{};
};
struct GState {
    bool init = false;
    unsigned rng = 1;
    float yaw = 0.0f;
    float quad = 0.0f, quadV = 0.0f, crouch = 0.0f, crouchV = 0.0f, alert = 0.0f, alertV = 0.0f, mask = 0.0f, maskV = 0.0f;
    float jaw = 0.0f, jawV = 0.0f, flare = 0.0f, flareV = 0.0f, reach = 0.0f, reachV = 0.0f, light = 0.0f, lightV = 0.0f;
    float perc = 0.0f, percV = 0.0f, plea = 0.0f, pleaV = 0.0f, wave = 0.0f, waveV = 0.0f, lean = 0.0f, leanV = 0.0f;
    float tilt = 0.0f, tiltV = 0.0f, loot = 0.0f, lootV = 0.0f, fear = 0.0f, fearV = 0.0f;
    float ph = 0.0f, prevPh = 0.0f;
    float earT[2]{1.0f, 2.0f}, earK[2]{}, earKV[2]{}, earKT[2]{};
    vec3 earTip[2]{}, earTipV[2]{};
    bool earInit = false;
    vec3 hdir{1, 0, 0}, hdirV{}, hdirTgt{1, 0, 0};
    float sacT = 0.4f, tiltR = 0.0f, tiltRT = 2.0f;
    vec3 headHold{}, headHoldV{};
    Strand cl[gb::kStrands];
    vec3 lastPos{};
    vec3 mfP{}, mfT{}, mfV{};  // abgefallene Maske
    bool maskFree = false;
    float twitchT = 0.0f;
    vec3 twitch[4]{};
    float fidget = 0.0f;
};
}  // namespace

CreatureSkeleton gb::makeSkeleton() {
    using namespace gb;
    GBld k(Entity::Goblin, JCOUNT, BCOUNT);
    k.j(ROOT, {0, 0, 0});
    k.j(PELVIS, {-0.02f, 0, 0.215f});
    k.j(SPINE, {0.0f, 0, 0.31f});
    k.j(CHEST, {0.03f, 0, 0.40f});
    k.j(NECK, {0.06f, 0, 0.46f});
    k.j(HEAD, {0.085f, 0, 0.515f});
    k.j(SNOUT, {0.145f, 0, 0.505f});
    k.j(CROWN, {0.075f, 0, 0.575f});
    k.j(JAWP, {0.115f, 0, 0.49f});
    k.j(JAWT, {0.235f, 0, 0.455f});
    k.j(MASKP, {0.115f, 0, 0.565f});
    k.j(MASKT, {0.315f, 0, 0.455f});
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        k.j(earJ(s, 0), {0.065f, sg * 0.035f, 0.565f});
        k.j(earJ(s, 1), {0.045f, sg * 0.062f, 0.655f});
        k.j(earJ(s, 2), {0.03f, sg * 0.078f, 0.745f});
        k.j(armJ(s, 0), {0.035f, sg * 0.065f, 0.415f});
        k.j(armJ(s, 1), {0.015f, sg * 0.095f, 0.315f});
        k.j(armJ(s, 2), {0.07f, sg * 0.085f, 0.245f});
        k.j(armJ(s, 3), {0.115f, sg * 0.08f, 0.215f});
        k.j(legJ(s, 0), {-0.02f, sg * 0.042f, 0.215f});
        k.j(legJ(s, 1), {0.035f, sg * 0.052f, 0.155f});
        k.j(legJ(s, 2), {-0.035f, sg * 0.05f, 0.072f});
        k.j(legJ(s, 3), {-0.005f, sg * 0.05f, 0.012f});
        k.j(legJ(s, 4), {0.055f, sg * 0.05f, 0.0f});
    }
    k.j(TAIL0 + 0, {-0.055f, 0, 0.205f});
    k.j(TAIL0 + 1, {-0.13f, 0, 0.16f});
    k.j(TAIL0 + 2, {-0.205f, 0, 0.12f});
    k.j(TAIL0 + 3, {-0.275f, 0, 0.105f});
    for (int q = 0; q < kStrands; ++q) {
        const vec3 d = strandDir(q);
        k.j(cloakJ(q, 0), vec3{0.03f, 0, 0.44f} + d * 0.062f);
        k.j(cloakJ(q, 1), vec3{0.0f, 0, 0.27f} + d * 0.115f);
        k.j(cloakJ(q, 2), vec3{-0.01f, 0, 0.075f} + d * 0.15f);
    }
    k.j(CANE_A, {0.13f, -0.105f, 0.0f});
    k.j(CANE_B, {0.10f, -0.095f, 0.36f});
    k.j(ITEM_A, {0.115f, 0.08f, 0.215f});
    k.j(ITEM_B, {0.115f, 0.08f, 0.325f});
    k.j(PORT_A, portalCenter());
    k.j(PORT_B, portalCenter() + vec3{0, 0, kPortalH * 0.5f});
    for (int q = 0; q < kSwirls; ++q) {
        const float a = (float)q * kTauG / (float)kSwirls;
        k.j(swirlJ(q, 0), ringPt(a, 1.04f));
        k.j(swirlJ(q, 1), ringPt(a + 0.55f, 1.04f));
    }
    k.b(B_ROOT, ROOT, ROOT, -1, 0.0f, false);
    k.b(B_PELVIS, PELVIS, SPINE, B_ROOT, 0.07f);
    k.b(B_SPINE, SPINE, CHEST, B_PELVIS, 0.07f);
    k.b(B_CHEST, CHEST, NECK, B_SPINE, 0.06f);
    k.b(B_NECK, NECK, HEAD, B_CHEST, 0.035f);
    k.b(B_HEAD, HEAD, SNOUT, B_NECK, 0.05f);
    k.b(B_JAW, JAWP, JAWT, B_HEAD, 0.012f, false);
    k.b(B_MASK, MASKP, MASKT, B_HEAD, 0.04f);
    for (int s = 0; s < 2; ++s) {
        k.b(earB(s, 0), earJ(s, 0), earJ(s, 1), B_HEAD, 0.012f, false);
        k.b(earB(s, 1), earJ(s, 1), earJ(s, 2), earB(s, 0), 0.01f, false);
        k.b(armB(s, 0), armJ(s, 0), armJ(s, 1), B_CHEST, 0.02f, false);
        k.b(armB(s, 1), armJ(s, 1), armJ(s, 2), armB(s, 0), 0.016f, false);
        k.b(armB(s, 2), armJ(s, 2), armJ(s, 3), armB(s, 1), 0.012f, false);
        k.b(legB(s, 0), legJ(s, 0), legJ(s, 1), B_PELVIS, 0.025f);
        k.b(legB(s, 1), legJ(s, 1), legJ(s, 2), legB(s, 0), 0.016f);
        k.b(legB(s, 2), legJ(s, 2), legJ(s, 3), legB(s, 1), 0.011f, false);
        k.b(legB(s, 3), legJ(s, 3), legJ(s, 4), legB(s, 2), 0.01f, false);
    }
    for (int i = 0; i < 3; ++i) k.b(B_TAIL0 + i, TAIL0 + i, TAIL0 + i + 1, i == 0 ? B_PELVIS : B_TAIL0 + i - 1, 0.02f - 0.004f * (float)i, false);
    for (int q = 0; q < kStrands; ++q) {
        k.b(cloakB(q, 0), cloakJ(q, 0), cloakJ(q, 1), B_CHEST, 0.03f);
        k.b(cloakB(q, 1), cloakJ(q, 1), cloakJ(q, 2), cloakB(q, 0), 0.03f);
    }
    k.b(B_CANE, CANE_A, CANE_B, armB(1, 2), 0.012f, false);
    k.b(B_ITEM, ITEM_A, ITEM_B, armB(0, 2), 0.025f, false);
    k.b(B_PORTAL, PORT_A, PORT_B, B_ROOT, 0.3f, false);
    for (int q = 0; q < kSwirls; ++q) {
        k.b(B_SWIRL0 + q, swirlJ(q, 0), swirlJ(q, 1), B_PORTAL, 0.03f, false);
        k.s.bindUp[(size_t)(B_SWIRL0 + q)] = vec3{1, 0, 0};  // Normale der Portalflaeche
    }
    k.s.bindUp[(size_t)B_PORTAL] = vec3{1, 0, 0};
    k.s.height = 0.62f;
    k.s.reach = 1.4f;
    k.s.center = {0.0f, 0, 0.32f};
    return k.s;
}

float CreatureAnimator::goblinHeight() const {
    const int v = extra_.empty() ? 0 : (int)extra_[0];
    if (v == 3) return gb::kPortalH;
    return std::max(0.18f, pose_.j[gb::CROWN].z - pos_.z + 0.02f * scale_);
}

vec3 CreatureAnimator::goblinHead() const {
    const int v = extra_.empty() ? 0 : (int)extra_[0];
    if (v == 3) return pos_ + portalCenter();
    return pose_.j[gb::HEAD];
}

void CreatureAnimator::updateGoblin(float dt, const Input& in) {
    using namespace gb;
    if (!ext_) {
        ext_ = std::make_shared<GState>();
        static_cast<GState*>(ext_.get())->rng = rng_ | 1u;
    }
    GState& g = *static_cast<GState*>(ext_.get());
    auto& J = pose_.j;
    const size_t nb = sk_->bones.size();
    pose_.s.assign(nb, 1.0f);
    const int variant = extra_.size() > 0 ? (int)extra_[0] : 0;
    const int st = extra_.size() > 1 ? (int)extra_[1] : 0;
    const float stT = extra_.size() > 2 ? extra_[2] : 0.0f;
    const float S = scale_;
    auto hide = [&](int b) { pose_.s[(size_t)b] = 0.001f; };

    // ================================================ Portal =================================================
    if (variant == 3) {
        const float open = c01(in.fly);
        const vec3 F = yawDir(yaw_), L{-F.y, F.x, 0.0f}, U{0, 0, 1};
        auto W = [&](vec3 l) { return pos_ + F * l.x + L * l.y + U * l.z; };
        const vec3 C = W(portalCenter());
        for (int j = 0; j < PORT_A; ++j) J[(size_t)j] = C;
        J[ROOT] = pos_;
        for (int b = 0; b < B_PORTAL; ++b) hide(b);
        J[PORT_A] = C;
        J[PORT_B] = C + U * (kPortalH * 0.5f);
        // der Rand atmet und zittert; offen schwillt er leicht nach
        const float breathe = 1.0f + 0.035f * std::sin(time_ * 5.3f) + 0.02f * std::sin(time_ * 13.7f);
        pose_.s[B_PORTAL] = std::max(0.001f, smG(open) * breathe);
        for (int q = 0; q < kSwirls; ++q) {
            // Sand kreist am Rand, gegenlaeufig je zweiter Strom, und zieht beim Oeffnen nach innen
            const float dir = (q % 2) == 0 ? 1.0f : -1.0f;
            const float a = (float)q * kTauG / (float)kSwirls + dir * time_ * (0.8f + 0.11f * (float)q);
            const float grow = (0.55f + 0.5f * smG(open)) * (1.0f + 0.07f * std::sin(time_ * 3.1f + (float)q * 1.9f));
            J[(size_t)swirlJ(q, 0)] = W(ringPt(a, grow));
            J[(size_t)swirlJ(q, 1)] = W(ringPt(a + 0.55f * dir, grow));
            pose_.s[(size_t)(B_SWIRL0 + q)] = std::max(0.001f, smG(open * 1.4f - 0.2f));
        }
        autoUps();
        for (int q = 0; q < kSwirls; ++q) pose_.up[(size_t)(B_SWIRL0 + q)] = F;
        pose_.up[B_PORTAL] = F;
        return;
    }

    // ================================================ Wesen ==================================================
    const bool gate = variant == 2, scout = variant == 1;
    const bool dead = st == S_DEAD, cling = st == S_RUMMAGE;
    // --- Kanaele glaetten ------------------------------------------------------------------------------------
    float quadT = std::max(c01(in.fly), smG((speed_ - 2.0f) / 1.5f));
    if (gate) quadT = std::min(quadT, 0.3f);
    if (cling || dead) quadT = 0.0f;
    sprG(g.quad, g.quadV, quadT, 6.5f, dt);
    const float lightT = c01(in.light);
    sprG(g.light, g.lightV, lightT, 9.0f, dt);
    sprG(g.crouch, g.crouchV, c01(in.crouch + 0.55f * g.light + 0.35f * in.growl + (scout ? 0.15f : 0.0f)), 7.0f, dt);
    sprG(g.alert, g.alertV, c01(std::max(in.alert, in.perceive)), 10.0f, dt);
    // die Torwaechterin hebt die Maske nie (der Hut; sie befiehlt, sie beisst nicht)
    const float maskT = dead || gate ? 0.0f : c01(std::max(in.growl, in.strike > 0.0f ? 1.0f : 0.0f));
    sprG(g.mask, g.maskV, maskT, 7.5f, dt);
    const float chatter = in.talk > 0.01f ? in.talk * (0.5f + 0.5f * std::sin(time_ * 47.0f)) * 0.25f : 0.0f;
    const float hiss = in.growl > 0.3f ? 0.06f * std::sin(time_ * 61.0f) : 0.0f;
    sprG(g.jaw, g.jawV, c01(std::max(in.jaw, in.growl * 0.75f) + chatter + hiss), 18.0f, dt);
    sprG(g.flare, g.flareV, c01(in.growl), 6.0f, dt);
    sprG(g.reach, g.reachV, c01(in.reach), 9.0f, dt);
    sprG(g.perc, g.percV, c01(in.perceive), 8.0f, dt);
    sprG(g.plea, g.pleaV, c01(in.plea), 5.0f, dt);
    sprG(g.wave, g.waveV, c01(in.wave), 5.0f, dt);
    sprG(g.lean, g.leanV, std::clamp(in.lean, -0.6f, 0.6f), 6.0f, dt);
    sprG(g.fear, g.fearV, c01(std::max(st == S_LOOT || st == S_SCURRY ? 1.0f : 0.0f, g.light)), 6.0f, dt);
    // Beute: sichtbar, solange sie in der Klaue liegt; nach dem Griff verschwindet sie im Umhang (0,45-0,85 s)
    float lootS = c01(in.bait);
    if (st == S_LOOT) lootS *= 1.0f - smG((stT - 0.45f) / 0.4f);
    sprG(g.loot, g.lootV, lootS, 14.0f, dt);
    // Kopfneigung: vorgegeben, beim Wahrnehmen ab und zu schief (Eule)
    g.tiltRT -= dt;
    if (g.tiltRT <= 0.0f) {
        g.tiltR = g.perc > 0.4f && hashG(g.rng) < 0.6f ? (hashG(g.rng) - 0.5f) * 1.1f : 0.0f;
        g.tiltRT = 0.6f + 1.8f * hashG(g.rng);
    }
    sprG(g.tilt, g.tiltV, in.tilt + g.tiltR, 12.0f, dt);

    // --- Koerperdrehung: ruckartig, grosse Wendungen schnell -----------------------------------------------------
    if (!g.init) g.yaw = yaw_;
    {
        const float dy = wrapG(yaw_ - g.yaw);
        const float om = std::fabs(dy) > 0.45f ? 15.0f : (dead ? 0.0f : 6.0f);
        g.yaw += dy * std::min(1.0f, dt * om);
    }
    const vec3 F = yawDir(g.yaw), L{-F.y, F.x, 0.0f}, U{0, 0, 1};

    // --- Sprung (Angriff): Versatz der Wurzel im Bogen zum Ziel -----------------------------------------------
    vec3 R = pos_;
    float leapAir = 0.0f, coil = 0.0f;
    vec3 toT = F;
    if (in.strike > 0.0f && !dead) {
        const float s = c01(in.strike);
        vec3 d = in.reachAt - pos_;
        d.z = 0.0f;
        const float dl = length(d);
        toT = dl > 1e-3f ? d / dl : F;
        const float dist = std::clamp(dl - 0.25f, 0.0f, 1.1f);
        if (s < 0.3f) coil = smG(s / 0.3f);
        else if (s < 0.65f) {
            const float u = (s - 0.3f) / 0.35f;
            R = pos_ + toT * (dist * smG(u)) + U * (0.42f * std::sin(kPiG * u));
            leapAir = std::sin(kPiG * u);
            coil = 1.0f - smG(u * 3.0f);
        } else {
            const float u = (s - 0.65f) / 0.35f;
            R = pos_ + toT * (dist * (1.0f - smG(u)));
            coil = 0.4f * (1.0f - u);
        }
    }
    auto W = [&](vec3 l) { return R + (F * l.x + L * l.y + U * l.z) * S; };

    // --- Gang -----------------------------------------------------------------------------------------------
    const float spd = dead ? 0.0f : speed_;
    const float moving = cling ? 0.0f : smG((spd - 0.05f) / 0.25f);
    const float q = g.quad;
    const float freqU = (gate ? 2.0f : 2.5f) + 2.2f * c01(spd / 1.6f), freqQ = 3.4f + 0.22f * spd;
    const float freq = mixG(freqU, freqQ, q);
    g.prevPh = g.ph;
    if (moving > 0.01f) g.ph += dt * freq;
    const float stride = std::min(spd / std::max(freq, 0.5f), mixG(0.2f, 0.62f, q) * S);
    // Wippen: Vogelschritt (bei jedem Aufsetzen tiefer), Galopp (Flugphase)
    const float bobU = -0.01f * moving * (0.5f + 0.5f * std::cos(4.0f * kPiG * g.ph));
    const float bobQ = 0.03f * moving * std::sin(kTauG * g.ph + 0.6f);
    const float bob = mixG(bobU, bobQ, q) * S;
    const float breath = std::sin(time_ * (2.1f + 1.4f * g.alert)) * 0.02f;

    // --- Rumpf: vier Glieder von der Huefte, Neigung aufrecht .. waagrecht ---------------------------------------
    const float c = c01(g.crouch + 0.6f * coil);
    const float er = gate ? -0.12f : (scout ? 0.06f : 0.0f);
    float aU[4] = {0.21f + 0.25f * c + er, 0.32f + 0.3f * c + er + breath, 0.45f + 0.25f * c + er, 0.6f + 0.2f * c};
    const float aQ[4] = {1.45f, 1.52f, 1.12f, 0.78f};
    aU[1] -= 0.25f * g.light, aU[2] -= 0.2f * g.light;          // Licht: weicht zurueck
    aU[1] += 0.25f * g.flare, aU[2] += 0.2f * g.flare;          // Drohen: vorgebeugt, gespreizt
    if (cling) aU[0] = 0.35f, aU[1] = 0.45f, aU[2] = 0.55f, aU[3] = 0.45f;  // am Ruecken: gegen die Flaeche gedrueckt
    const float qq = std::max(q, 0.75f * leapAir);
    vec3 pelL = mixV(vec3{-0.02f, 0, 0.215f - 0.07f * c}, vec3{-0.07f, 0, 0.165f - 0.03f * c}, qq);
    if (cling) pelL = {-0.02f, 0, 0.2f};
    J[ROOT] = pos_;
    J[PELVIS] = W(pelL) + U * bob;
    const int segJ[5] = {PELVIS, SPINE, CHEST, NECK, HEAD};
    for (int i = 0; i < 4; ++i) {
        const float a = mixG(aU[i], aQ[i], qq);
        vec3 d = F * std::sin(a) + U * std::cos(a);
        d = rotG(d, F, -g.lean * (0.6f + 0.25f * (float)i));
        J[(size_t)segJ[i + 1]] = J[(size_t)segJ[i]] + d * (kSeg[i] * S);
    }

    // --- Kopf: Richtung mit Sakkaden, Vogel-Halten beim Gehen -----------------------------------------------------
    const float pitch0 = mixG(0.15f, -0.12f, q) + 0.25f * coil;
    const vec3 baseH = nrmG(F * std::cos(pitch0) - U * std::sin(pitch0), F);
    vec3 want = baseH;
    const bool looking = (in.look || g.perc > 0.2f) && !dead;
    if (looking) {
        const vec3 at = g.perc > 0.2f ? in.perceiveAt : in.lookAt;
        const vec3 to = nrmG(at - J[HEAD], baseH);
        // Kopf bis 2,1 rad zur Seite (Eule), 1 rad hoch/runter
        float ya = wrapG(std::atan2(to.y, to.x) - g.yaw);
        ya = std::clamp(ya, -2.1f, 2.1f);
        float pa = std::clamp(std::asin(std::clamp(to.z, -1.0f, 1.0f)), -1.0f, 1.0f);
        want = vec3{std::cos(g.yaw + ya) * std::cos(pa), std::sin(g.yaw + ya) * std::cos(pa), std::sin(pa)};
    }
    if (st == S_ATTACK || in.strike > 0.0f) want = nrmG(toT + U * 0.15f, baseH);
    g.sacT -= dt;
    if (g.sacT <= 0.0f || !g.init) {
        vec3 t = want;
        if (!looking && !dead) {  // Ruhe: der Blick springt umher
            const float ya = (hashG(g.rng) - 0.5f) * (moving > 0.5f ? 0.7f : 1.9f);
            const float pa = (hashG(g.rng) - 0.5f) * 0.6f;
            t = rotG(rotG(want, U, ya), nrmG(cross(U, want), L), -pa);
        } else if (looking) {
            t = rotG(want, U, (hashG(g.rng) - 0.5f) * 0.12f);
        }
        g.hdirTgt = t;
        g.sacT = looking ? 0.12f + 0.3f * hashG(g.rng) : 0.3f + 1.3f * hashG(g.rng);
    }
    if (looking) g.hdirTgt = nrmG(g.hdirTgt + (want - g.hdirTgt) * std::min(1.0f, dt * 4.0f), want);
    vec3 hT = nrmG(g.hdirTgt, F);
    if (g.light > 0.05f) hT = nrmG(rotG(hT, U, 0.6f * g.light) - U * (0.3f * g.light), F);  // vom Licht weg
    if (!g.init) g.hdir = hT;
    sprGV(g.hdir, g.hdirV, hT, dead ? 2.0f : 26.0f, dt);  // sehr schnelle Feder: Rucke
    const vec3 hd = nrmG(g.hdir, F);
    vec3 hl = nrmG(cross(U, hd), L);
    vec3 hu = cross(hd, hl);
    hl = rotG(hl, hd, g.tilt), hu = rotG(hu, hd, g.tilt);
    // Vogel-Halten: beim aufrechten Gehen bleibt der Kopf im Raum stehen, bis er 4,5 cm zurueckliegt - dann ruckt er vor
    const vec3 headIdeal = J[HEAD];
    if (g.init && q < 0.4f && moving > 0.3f && !cling) {
        if (length(headIdeal - g.headHold) > 0.045f * S) g.headHold = headIdeal + (headIdeal - g.headHold) * 0.4f;
        vec3 hp = g.headHold;
        const vec3 dd = hp - headIdeal;
        if (length(dd) > 0.05f * S) hp = headIdeal + dd * (0.05f * S / length(dd));
        J[HEAD] = hp;
    } else {
        g.headHold = headIdeal;
    }
    auto H = [&](float x, float y, float z) { return J[HEAD] + (hd * x + hl * y + hu * z) * S; };
    J[SNOUT] = H(0.06f, 0, -0.01f);
    J[CROWN] = H(-0.01f, 0, 0.06f);
    J[JAWP] = H(0.03f, 0, -0.025f);
    const float jawA = 0.04f + 0.9f * g.jaw;
    J[JAWT] = J[JAWP] + rotG((hd * 0.12f - hu * 0.035f) * S, hl, jawA);
    J[MASKP] = H(0.03f, 0, 0.05f);
    J[MASKT] = J[MASKP] + rotG((hd * 0.2f - hu * 0.11f) * S, hl, -1.95f * g.mask);

    // --- Ohren --------------------------------------------------------------------------------------------------
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        g.earT[s] -= dt;
        if (g.earT[s] <= 0.0f) {  // einzelnes Zucken
            g.earKT[s] = (hashG(g.rng) - 0.3f) * 0.9f;
            g.earT[s] = 0.5f + 3.5f * hashG(g.rng);
        }
        g.earKT[s] *= std::exp(-dt * 6.0f);
        sprG(g.earK[s], g.earKV[s], g.earKT[s], 30.0f, dt);
        const vec3 rest{-0.2f, sg * 0.3f, 0.93f}, up{0.08f, sg * 0.22f, 0.97f}, flat{-0.92f, sg * 0.28f, 0.22f};
        const float fl = c01(std::max(g.mask, std::max(0.75f * q, g.fear)) + (dead ? 1.0f : 0.0f));
        vec3 dl = mixV(mixV(rest, up, g.alert), flat, fl);
        dl = nrmG(dl, rest);
        vec3 d = nrmG(hd * dl.x + hl * dl.y + hu * dl.z, U);
        d = rotG(d, hl, g.earK[s]);
        const vec3 E0 = H(-0.02f, sg * 0.035f, 0.05f);
        const vec3 E1 = E0 + d * (0.092f * S);
        const vec3 tipIdeal = E1 + nrmG(d - hd * 0.18f, d) * (0.092f * S);
        if (!g.earInit) g.earTip[s] = tipIdeal;
        sprGV(g.earTip[s], g.earTipV[s], tipIdeal, 15.0f, dt);
        J[(size_t)earJ(s, 0)] = E0;
        J[(size_t)earJ(s, 1)] = E1;
        J[(size_t)earJ(s, 2)] = E1 + nrmG(g.earTip[s] - E1, d) * (0.092f * S);
    }
    g.earInit = true;

    // --- Beine (Fussballen) und Vorderbeine im Galopp -------------------------------------------------------------
    const vec3 tUp = nrmG(J[NECK] - J[PELVIS], U);
    const vec3 tFwd = nrmG(cross(L, tUp), F);
    vec3 footT[2], foreT[2];
    float footLift[2]{}, foreLift[2]{};
    bool stepNow = false;
    vec3 stepAt{};
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        // aufrecht: Wechselschritt, kurze Schwungphase
        const float uU = fracG(g.ph + 0.5f * (float)s), dutyU = 0.6f;
        float xU, zU;
        if (uU < dutyU) xU = stride * (0.5f - uU / dutyU), zU = 0.0f;
        else {
            const float p = (uU - dutyU) / (1.0f - dutyU);
            xU = stride * (-0.5f + smG(p)), zU = 0.05f * S * std::sin(kPiG * p) * moving;
        }
        // Galopp (Hinterbeine fast gleichzeitig, Vorderbeine eine halbe Phase spaeter)
        const float uQ = fracG(g.ph + 0.08f * (float)s), dutyQ = 0.38f;
        float xQ, zQ;
        if (uQ < dutyQ) xQ = stride * (0.5f - uQ / dutyQ), zQ = 0.0f;
        else {
            const float p = (uQ - dutyQ) / (1.0f - dutyQ);
            xQ = stride * (-0.5f + smG(p)), zQ = 0.07f * S * std::sin(kPiG * p) * moving;
        }
        const vec3 nU = W({pelL.x + 0.02f, sg * 0.05f, 0.0f}), nQ = W({-0.1f, sg * 0.052f, 0.0f});
        footT[s] = mixV(nU + F * xU, nQ + F * xQ, q);
        footT[s].z = pos_.z + mixG(zU, zQ, q);
        footLift[s] = mixG(zU, zQ, q);
        // Vorderbeine (Arme) im Galopp
        const float uF = fracG(g.ph + 0.5f + 0.09f * (float)s);
        float xF, zF;
        if (uF < dutyQ) xF = stride * (0.5f - uF / dutyQ), zF = 0.0f;
        else {
            const float p = (uF - dutyQ) / (1.0f - dutyQ);
            xF = stride * (-0.5f + smG(p)), zF = 0.08f * S * std::sin(kPiG * p) * moving;
        }
        foreT[s] = W({0.17f, sg * 0.05f, 0.0f}) + F * xF;
        foreT[s].z = pos_.z + zF;
        foreLift[s] = zF;
        // Klang: Aufsetzen (Beginn der Standphase)
        if (moving > 0.3f && g.init) {
            const float u0 = fracG(g.prevPh + (q > 0.5f ? 0.08f : 0.5f) * (float)s), u1 = q > 0.5f ? uQ : uU;
            if (u1 < u0) stepNow = true, stepAt = footT[s];
            if (q > 0.5f) {
                const float f0 = fracG(g.prevPh + 0.5f + 0.09f * (float)s);
                if (uF < f0) stepNow = true, stepAt = foreT[s];
            }
        }
    }
    if (stepNow) stepped_ = true, stepPos_ = stepAt, stepWeight_ = 0.18f + 0.25f * q;
    run_ = q > 0.5f;
    if (cling) {  // am Ruecken: die Fuesse krallen sich vorn in den Stoff
        for (int s = 0; s < 2; ++s) {
            const float sg = s == 0 ? 1.0f : -1.0f;
            footT[s] = W({0.11f, sg * 0.055f, 0.03f + 0.012f * std::sin(time_ * 9.0f + (float)s * 2.0f)});
        }
    }
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const vec3 hip = J[PELVIS] + L * (sg * 0.042f * S);
        const vec3 ball = footT[s];
        const float lift = footLift[s];
        // Ferse hoch und zurueck; geduckt flacher
        const vec3 heelDir = nrmG(F * (-0.42f - 0.5f * c) + U * (1.0f - 0.35f * c), U);
        vec3 heel = ball + heelDir * (kTarsus * S);
        vec3 knee, heelOut;
        ik2(hip, heel, kThigh * S, kShin * S, nrmG(F + U * 0.2f, F), knee, heelOut);
        J[(size_t)legJ(s, 0)] = hip;
        J[(size_t)legJ(s, 1)] = knee;
        J[(size_t)legJ(s, 2)] = heelOut;
        J[(size_t)legJ(s, 3)] = ball;
        const vec3 toeDir = nrmG(F - U * (lift > 0.002f ? 0.6f : 0.0f), F);
        vec3 toe = ball + toeDir * (kToe * S);
        toe.z = std::max(toe.z, pos_.z);
        J[(size_t)legJ(s, 4)] = toe;
    }

    // --- Arme -----------------------------------------------------------------------------------------------------
    vec3 reachAt = in.reachAt;
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;
        const vec3 sh = J[CHEST] + L * (sg * 0.065f * S) + tUp * (0.015f * S);
        g.fidget += dt;
        // Ruhe: Krallen vor der Brust gefaltet, reiben aneinander
        vec3 T = J[CHEST] + tFwd * (0.085f * S) - tUp * (0.16f * S) + L * (sg * 0.042f * S) +
                 tFwd * (0.008f * S * std::sin(g.fidget * 5.0f + (float)s * 2.4f));
        T = T + F * (0.03f * S * std::sin(kTauG * g.ph + (float)s * kPiG) * moving * (1.0f - q));
        // Galopp: Vorderbeine
        T = mixV(T, foreT[s], q);
        // Beute (links): hochgehalten, dann in den Umhang
        if (s == 0 && g.loot > 0.01f) {
            const vec3 hold = J[CHEST] + tFwd * (0.1f * S) - tUp * (0.06f * S) + L * (0.02f * S);
            const vec3 stash = J[CHEST] + tFwd * (0.03f * S) - tUp * (0.12f * S);
            const float u = st == S_LOOT ? smG((stT - 0.4f) / 0.45f) : 0.0f;
            T = mixV(T, mixV(hold, stash, u), c01(g.loot * 3.0f) * (1.0f - q));
        }
        // Torwaechterin: die rechte Hand am Stock
        vec3 caneTip{}, caneTop{};
        if (gate && s == 1) {
            const float tap = std::sin(kTauG * g.ph) * moving;
            caneTip = W({0.13f + 0.05f * tap, -0.10f, 0.0f});
            caneTip.z = pos_.z + 0.025f * S * std::max(0.0f, tap) * moving;
            const vec3 cd = nrmG(vec3{0, 0, 1} + F * (-0.08f) + L * 0.03f, U);
            caneTop = caneTip + cd * (0.36f * S);
            T = mixV(T, caneTip + cd * (0.27f * S), 1.0f - q);
        }
        // greifen (Ware aufheben, nach dem Opfer langen)
        if (g.reach > 0.01f) {
            vec3 d = reachAt - sh;
            const float dl = length(d);
            if (dl > 0.23f * S) d = d * (0.23f * S / dl);
            const vec3 pick = vec3{0, 0, 0.012f * S * std::sin(time_ * 21.0f + (float)s * 1.7f)};
            T = mixV(T, sh + d + pick, g.reach);
        }
        // wuehlen (am Ruecken): beide Krallen graben im Wechsel
        if (cling) T = J[CHEST] + F * (0.15f * S) + U * ((0.03f + 0.045f * std::sin(time_ * 17.0f + (float)s * kPiG)) * S) + L * (sg * 0.04f * S);
        // drohen: den Umhang aufspannen
        if (g.flare > 0.01f) T = mixV(T, sh + (L * (sg * 0.2f) + U * 0.05f + F * 0.06f) * S, g.flare);
        // Angriff: Krallen voran
        if (in.strike > 0.25f && !dead) T = mixV(T, sh + nrmG(toT * 0.9f + U * 0.1f, F) * (0.23f * S), smG((in.strike - 0.25f) / 0.15f) * (in.strike < 0.85f ? 1.0f : 0.0f));
        // Licht: der Arm auf der Lichtseite schirmt die Augen ab
        if (g.light > 0.02f) {
            const vec3 lf = in.lightFrom - J[HEAD];
            const bool side = dot(lf, L) * sg > 0.0f;
            if (side) T = mixV(T, H(0.07f, sg * 0.03f, 0.015f), g.light);
        }
        // anbieten (Handel): die rechte Klaue vorgestreckt, Handflaeche oben
        if (s == 1 && g.plea > 0.01f) T = mixV(T, sh + (F * 0.21f - U * 0.03f) * S, g.plea);
        // Stock hoch (Torwaechterin oeffnet/schliesst das Portal)
        if (s == 1 && g.wave > 0.01f) T = mixV(T, sh + (U * 0.22f + F * 0.07f) * S, g.wave);
        vec3 elbow, wrist;
        const vec3 pole = nrmG(F * (-0.6f) + L * (sg * 0.6f) - U * 0.3f, -F);
        ik2(sh, T, kUpper * S, kFore * S, pole, elbow, wrist);
        vec3 hand = nrmG(wrist - elbow, F);
        if (q > 0.3f) hand = nrmG(mixV(hand, -U + F * 0.4f, q), -U);
        if (cling) hand = nrmG(F - U * 0.4f, F);
        if (s == 1 && g.plea > 0.3f) hand = nrmG(F + U * 0.15f, F);
        J[(size_t)armJ(s, 0)] = sh;
        J[(size_t)armJ(s, 1)] = elbow;
        J[(size_t)armJ(s, 2)] = wrist;
        vec3 claw = wrist + hand * (kHand * S);
        claw.z = std::max(claw.z, pos_.z + 0.004f);
        J[(size_t)armJ(s, 3)] = claw;
        if (gate && s == 1) {
            if (g.wave > 0.01f) {  // hoch erhoben: Stock senkrecht ueber der Hand
                const vec3 top = wrist + U * (0.12f * S);
                caneTop = mixV(caneTop, top, g.wave);
                caneTip = mixV(caneTip, top - U * (0.36f * S), g.wave);
            }
            if (q > 0.05f) {  // im Lauf quer in der Hand
                caneTip = mixV(caneTip, wrist - F * (0.22f * S), q);
                caneTop = mixV(caneTop, wrist + F * (0.14f * S), q);
            }
            J[CANE_A] = caneTip;
            J[CANE_B] = caneTop;
        }
    }
    if (!gate) {
        J[CANE_A] = J[(size_t)armJ(1, 2)];
        J[CANE_B] = J[CANE_A] + U * (0.05f * S);
        hide(B_CANE);
    }
    // Beute in der linken Klaue
    {
        const vec3 a = J[(size_t)armJ(0, 3)] - U * (0.02f * S);
        J[ITEM_A] = a;
        J[ITEM_B] = a + nrmG(U + F * 0.25f, U) * (0.11f * S);
        pose_.s[B_ITEM] = std::max(0.001f, g.loot);
    }

    // --- Schwanz --------------------------------------------------------------------------------------------------
    {
        const float sway = 0.25f * std::sin(time_ * 1.7f) + 0.35f * moving * std::sin(kTauG * g.ph);
        const float pU[3] = {0.6f, 0.45f, 0.2f}, pQ[3] = {0.08f, -0.04f, -0.1f}, pT[3] = {-0.55f, -0.75f, -0.9f};
        vec3 p = J[PELVIS] + (F * (-0.035f) - U * 0.01f) * S;
        J[TAIL0] = p;
        for (int i = 0; i < 3; ++i) {
            const float pi = mixG(mixG(pU[i], pQ[i], q), pT[i], g.flare * 0.8f);
            vec3 d = nrmG(-F * std::cos(pi) - U * std::sin(pi), -F);
            d = rotG(d, U, sway * (float)(i + 1) / 3.0f);
            p = p + d * (0.078f * S);
            p.z = std::max(p.z, pos_.z + 0.012f);
            J[(size_t)(TAIL0 + i + 1)] = p;
        }
    }

    // --- Umhang: sechs Straenge mit Traegheit (Verlet), am Brustring festgemacht ------------------------------------
    {
        const vec3 ringC = J[CHEST] + tUp * (0.035f * S);
        const vec3 capA = J[PELVIS] - tUp * (0.02f * S), capB = J[NECK];
        const float capR = (0.078f + 0.02f * q) * S;
        const bool reset = !g.init || length(pos_ - g.lastPos) > 0.8f;
        const vec3 drag = vel_ * -0.045f;
        for (int k = 0; k < kStrands; ++k) {
            Strand& sd = g.cl[k];
            const vec3 dk = strandDir(k);
            const vec3 radial = nrmG(tFwd * dk.x + L * dk.y, F);
            const vec3 anchor = ringC + radial * (0.062f * S);
            const vec3 midRest = J[PELVIS] + tUp * (0.06f * S) + radial * (0.115f * S);
            vec3 hemRest = midRest + radial * (0.04f * S) - U * (0.19f * S);
            // gespreizt: die vorderen und seitlichen Saeume folgen den Haenden nach aussen
            if (g.flare > 0.01f && (k <= 1 || k >= 4)) {
                const int s = k <= 1 ? 0 : 1;
                const vec3 hnd = J[(size_t)armJ(s, 3)];
                hemRest = mixV(hemRest, hnd + radial * (0.03f * S) - U * (0.03f * S), g.flare * (k == 0 || k == 5 ? 1.0f : 0.6f));
            }
            if (reset) {
                const vec3 bindA = sk_->bind[(size_t)cloakJ(k, 0)], bindM = sk_->bind[(size_t)cloakJ(k, 1)], bindH = sk_->bind[(size_t)cloakJ(k, 2)];
                sd.len[1] = length(bindM - bindA) * S;
                sd.len[2] = length(bindH - bindM) * S;
                sd.p[0] = sd.q[0] = anchor;
                sd.p[1] = sd.q[1] = midRest;
                sd.p[2] = sd.q[2] = hemRest;
            }
            sd.p[0] = sd.q[0] = anchor;
            const vec3 rest[3] = {anchor, midRest, hemRest};
            const int sub = std::max(1, (int)std::ceil(dt / (1.0f / 90.0f)));
            const float h = dt / (float)sub;
            for (int it = 0; it < sub && dt > 0.0f; ++it) {
                for (int i = 1; i < 3; ++i) {
                    const vec3 v = (sd.p[i] - sd.q[i]) * 0.9f;
                    sd.q[i] = sd.p[i];
                    const vec3 acc = (rest[i] - sd.p[i]) * (g.flare > 0.01f && i == 2 ? 260.0f : 70.0f) - U * 6.0f + drag * 60.0f;
                    sd.p[i] = sd.p[i] + v + acc * (h * h);
                }
                for (int i = 1; i < 3; ++i) {
                    const vec3 dd = sd.p[i] - sd.p[i - 1];
                    const float l = length(dd);
                    if (l > 1e-6f) sd.p[i] = sd.p[i - 1] + dd * (sd.len[i] / l);
                    // nicht durch den Rumpf
                    const vec3 ab = capB - capA;
                    const float t = std::clamp(dot(sd.p[i] - capA, ab) / std::max(dot(ab, ab), 1e-9f), 0.0f, 1.0f);
                    const vec3 qp = capA + ab * t, dq = sd.p[i] - qp;
                    const float dql = length(dq);
                    const float rr = capR * (i == 1 ? 1.0f : 1.15f);
                    if (dql < rr && dql > 1e-6f) sd.p[i] = qp + dq * (rr / dql);
                    sd.p[i].z = std::max(sd.p[i].z, pos_.z + 0.012f * S);
                }
            }
            for (int i = 0; i < 3; ++i) J[(size_t)cloakJ(k, i)] = sd.p[i];
        }
    }

    // --- Portalteile verborgen --------------------------------------------------------------------------------------
    for (int j = PORT_A; j < JCOUNT; ++j) J[(size_t)j] = J[CHEST];
    hide(B_PORTAL);
    for (int k = 0; k < kSwirls; ++k) hide(B_SWIRL0 + k);

    // --- Tod: kippt auf die Seite, zuckt; die Maske rutscht ab -------------------------------------------------------
    if (dead) {
        const float fall = smG(stT / 0.45f);
        const vec3 pivot = pos_ + U * (0.02f * S);
        const float roll = -1.45f * fall;
        // zucken: einzelne Glieder, abklingend
        g.twitchT -= dt;
        if (g.twitchT <= 0.0f) {
            for (auto& t : g.twitch) t = vec3{hashG(g.rng) - 0.5f, hashG(g.rng) - 0.5f, hashG(g.rng) - 0.5f} * (0.05f * S * c01(in.shiver));
            g.twitchT = 0.05f + 0.25f * hashG(g.rng);
        }
        for (int j = 1; j < CLOAK0; ++j) {
            vec3 p = pivot + rotG(J[(size_t)j] - pivot, F, roll);
            p.z = std::max(pos_.z + 0.01f, p.z - 0.04f * S * fall);
            J[(size_t)j] = p;
        }
        for (int s = 0; s < 2; ++s) {
            J[(size_t)legJ(s, 3)] += g.twitch[s];
            J[(size_t)legJ(s, 4)] += g.twitch[s] * 1.4f;
            J[(size_t)armJ(s, 3)] += g.twitch[2 + s];
        }
        // Maske: ab 0,25 s frei, faellt neben den Kopf
        if (stT > 0.25f) {
            const vec3 head = J[HEAD];
            const vec3 restP = vec3{head.x, head.y, pos_.z} + F * (0.12f * S) + L * (0.03f * S) + U * (0.035f * S);
            if (!g.maskFree) g.mfP = J[MASKP], g.mfT = J[MASKT], g.mfV = {}, g.maskFree = true;
            g.mfV.z -= 9.8f * dt;
            g.mfP += g.mfV * dt;
            const vec3 tgt = restP;
            g.mfP.x += (tgt.x - g.mfP.x) * std::min(1.0f, dt * 6.0f);
            g.mfP.y += (tgt.y - g.mfP.y) * std::min(1.0f, dt * 6.0f);
            if (g.mfP.z < tgt.z) g.mfP.z = tgt.z, g.mfV = {};
            const vec3 tipRest = g.mfP + nrmG(F * 0.9f - L * 0.4f, F) * (0.228f * S) - U * (0.03f * S);
            g.mfT = mixV(g.mfT, tipRest, std::min(1.0f, dt * 5.0f));
            J[MASKP] = g.mfP;
            J[MASKT] = g.mfT;
        }
    } else {
        g.maskFree = false;
    }

    // --- Portal: hinein (schrumpft zur Mitte) und heraus ------------------------------------------------------------
    float shrink = 1.0f;
    if (st == S_ENTER) shrink = 1.0f - smG((stT - 0.12f) / 0.38f);
    if (st == S_EMERGE) shrink = smG(stT / 0.42f);
    if (st == S_INSIDE) shrink = 0.0f;
    if (shrink < 0.999f) {
        const vec3 cc = J[SPINE];
        for (int j = 1; j < PORT_A; ++j) J[(size_t)j] = cc + (J[(size_t)j] - cc) * std::max(0.02f, shrink);
        for (size_t b = 0; b < nb; ++b) pose_.s[b] = std::max(0.001f, pose_.s[b] * shrink);
    }

    autoUps();
    // Kopf, Kiefer und Maske tragen die Neigung mit (autoUps kennt keine Drehung um die Laengsachse)
    pose_.up[B_HEAD] = hu;
    pose_.up[B_JAW] = rotG(hu, hl, jawA);
    if (!dead || stT <= 0.25f) pose_.up[B_MASK] = rotG(hu, hl, -1.95f * g.mask);
    g.lastPos = pos_;
    g.init = true;
}

}  // namespace lim::anim
