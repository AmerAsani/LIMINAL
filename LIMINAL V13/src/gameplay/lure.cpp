#include "gameplay/lure.hpp"

#include <algorithm>
#include <cmath>

#include "core/rng.hpp"
#include "world/room.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
constexpr float kPiL = 3.14159265f;
constexpr float kGravity = 9.81f;

mat4 orient(vec3 pos, float yaw, float pitch, float roll) {
    const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const vec3 a{cp * cy, cp * sy, sp};
    const vec3 s0{-sy, cy, 0.0f};
    const vec3 u0 = cross(a, s0);
    const vec3 s = s0 * std::cos(roll) + u0 * std::sin(roll);
    const vec3 u = cross(a, s);
    return mat4::fromRows({a.x, s.x, u.x, pos.x}, {a.y, s.y, u.y, pos.y}, {a.z, s.z, u.z, pos.z}, {0, 0, 0, 1});
}

float sgnF(float v) { return v < 0.0f ? -1.0f : 1.0f; }
}  // namespace

int LureSystem::launch(vec3 start, vec3 vel, float yaw, int by, unsigned seed) {
    if (lures_.size() >= kMax) lures_.erase(lures_.begin());
    Lure l;
    l.id = next_++;
    l.by = by;
    l.pos = start;
    l.vel = vel;
    // ueber die Hand geworfen: der Stab liegt etwas schraeg in der Hand und ueberschlaegt sich nach vorn
    l.yaw = yaw + (hash01(seed) - 0.5f) * 0.7f;
    l.pitch = 0.35f + hash01(seed ^ 0x2C1Bu) * 0.4f;
    l.spin = -(8.5f + hash01(seed ^ 0x51EDu) * 6.0f);
    l.roll = hash01(seed ^ 0x77A1u) * 6.2831853f;
    lures_.push_back(l);
    return l.id;
}

float LureSystem::glow(double age) {
    if (age <= 0.0 || age >= kGlowSeconds) return 0.0f;
    // beim Knicken mischen sich die Fluessigkeiten: in einer halben Sekunde ganz hell
    const double up = std::min(1.0, age / 0.5);
    const double t = age / kGlowSeconds;
    // dann langsam schwaecher (die Reaktion ermuedet), zuletzt rasch dunkel
    const double fade = std::pow(1.0 - t, 0.55) * (1.0 - 0.25 * t);
    return (float)(up * up * (3.0 - 2.0 * up) * fade);
}

void LureSystem::update(double dt, world::World& w, std::vector<Impact>& hits) {
    std::erase_if(lures_, [](const Lure& l) { return l.age > kGlowSeconds + kSpentKeep; });
    for (Lure& l : lures_) {
        l.age += dt;
        l.landCool = std::max(0.0f, l.landCool - (float)dt);
        if (l.resting) continue;
        // Wasser: der Stab schwimmt (Wasserspiegel des Raums, einmal je Bild)
        float waterZ = -1e9f;
        if (const world::Room* r = w.roomAt(l.pos.x, l.pos.y); r && r->hasWater()) waterZ = (float)r->water;
        auto hit = [&](vec3 p, float s, int kind) {
            if (s < 0.04f || l.landCool > 0.0f) return;
            hits.push_back({p, std::min(1.0f, s), kind});
            l.landCool = 0.05f;
        };
        auto blocked = [&](float x, float y, float z) {
            const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
            return !t || t->solid() || t->floor > z - kRadius + 0.03f || t->ceil < z + kRadius;
        };
        float rest = (float)dt;
        while (rest > 1e-6f && !l.resting) {
            const float h = std::min(rest, 1.0f / 240.0f);
            rest -= h;
            vec3 v = l.vel;
            if (!l.ground) v.z -= kGravity * h;
            v = v - v * (0.05f * h);  // wenig Luftwiderstand (leicht, schlank)
            vec3 np = l.pos + v * h;
            // Waende: je Achse pruefen, nur die gekreuzte Komponente umkehren (Ecken: beide)
            if (ifloor(np.x) != ifloor(l.pos.x) && blocked(np.x, l.pos.y, np.z)) {
                hit(vec3{np.x, l.pos.y, np.z}, std::fabs(v.x) / 6.0f, 1);
                v.x = -v.x * 0.32f, v.y *= 0.8f, v.z *= 0.85f;
                np.x = l.pos.x;
                l.spin = -l.spin * 0.55f;
                l.yaw += (hash01((u32)(l.id * 7919 + (int)(l.age * 1000.0))) - 0.5f) * 0.9f;
            }
            if (ifloor(np.y) != ifloor(l.pos.y) && blocked(np.x, np.y, np.z)) {
                hit(vec3{np.x, np.y, np.z}, std::fabs(v.y) / 6.0f, 1);
                v.y = -v.y * 0.32f, v.x *= 0.8f, v.z *= 0.85f;
                np.y = l.pos.y;
                l.spin = -l.spin * 0.55f;
                l.yaw += (hash01((u32)(l.id * 104729 + (int)(l.age * 1000.0))) - 0.5f) * 0.9f;
            }
            const world::Tile* t = w.tileIfLoaded(ifloor(np.x), ifloor(np.y));
            if (!t || t->solid()) {  // nicht geladen oder in der Wand: liegen lassen, wo er ist
                l.vel = {};
                l.resting = true;
                break;
            }
            const bool wet = waterZ > t->floor;
            const float fl = wet ? waterZ : t->floor;
            if (np.z + kRadius > t->ceil) {  // Decke
                np.z = t->ceil - kRadius;
                if (v.z > 0.0f) {
                    hit(np, v.z / 6.0f, 2);
                    v.z = -v.z * 0.3f;
                }
            }
            const float bottom = np.z - kRadius;
            if (bottom <= fl) {
                np.z = fl + kRadius;
                const float imp = -v.z;
                if (!l.ground) {
                    if (imp > 0.9f && !wet) {  // kurz aufspringen (harter Kunststoff)
                        hit(np, imp / 5.5f, 0);
                        v.z = imp * 0.28f;
                        v.x *= 0.7f, v.y *= 0.7f;
                        l.spin *= 0.45f;
                    } else {
                        if (imp > 0.25f) hit(np, imp / 5.5f, wet ? 3 : 0);
                        v.z = 0.0f;
                        l.ground = true;
                        l.floating = wet;
                        if (wet) v.x *= 0.4f, v.y *= 0.4f;
                    }
                } else {
                    v.z = 0.0f;
                }
            } else if (l.ground && bottom > fl + 0.02f) {
                l.ground = false;  // ueber eine Kante: faellt weiter
            }
            if (!l.ground) {
                l.pitch += l.spin * h;
            } else {
                // liegt flach: Achse waagrecht (die naechste Lage), gleitet laengs und rollt quer aus
                const float flatP = std::round(l.pitch / kPiL) * kPiL;
                l.pitch += (flatP - l.pitch) * std::min(1.0f, h * 30.0f);
                const vec3 ax{std::cos(l.yaw), std::sin(l.yaw), 0.0f}, sd{-std::sin(l.yaw), std::cos(l.yaw), 0.0f};
                float va = dot(v, ax), vs = dot(v, sd);
                const float muA = l.floating ? 1.0f : 4.2f, muS = l.floating ? 1.0f : 1.9f;  // Gleiten bremst stark, Rollen weniger
                va = sgnF(va) * std::max(0.0f, std::fabs(va) - muA * h);
                vs = sgnF(vs) * std::max(0.0f, std::fabs(vs) - muS * h);
                l.roll += vs / kRadius * h;
                v = ax * va + sd * vs;
                v.z = 0.0f;
                if (std::fabs(va) < 0.015f && std::fabs(vs) < 0.015f && std::fabs(flatP - l.pitch) < 0.01f) {
                    v = {};
                    l.pitch = flatP;
                    l.resting = true;
                }
            }
            l.pos = np;
            l.vel = v;
        }
    }
}

void LureSystem::collect(std::vector<VisibleMesh>& out, vec3 eye) {
    for (const Lure& l : lures_) {
        const vec3 d = l.pos - eye;
        if (dot(d, d) > 45.0f * 45.0f) continue;
        const float g = glow(l.age);
        VisibleMesh v;
        if (g > 0.38f) v.model = &mLit_, v.meshId = &idLit_;
        else if (g > 0.015f) v.model = &mDim_, v.meshId = &idDim_;
        else v.model = &mOff_, v.meshId = &idOff_;
        const float bob = l.floating ? 0.004f * std::sin((float)l.age * 2.1f) : 0.0f;
        v.world = orient(l.pos + vec3{0, 0, bob}, l.yaw, l.pitch, l.roll) * mat4::translation(vec3{0, 0, -kRadius});
        v.dynamic = !l.resting;
        v.cullCenter = l.pos;
        v.cullRadius = 0.14f;
        out.push_back(v);
    }
}

}  // namespace lim::game
