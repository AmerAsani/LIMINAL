// V13.3: purpifizierte tote Wesen in den Ziegelwaenden von Level 3 - siehe purpification.hpp, KREATUR_L3_Purpur.md.
// Alle Texte in eigenen Worten (Wikidot level-3, Rev. 18, CC BY-SA 3.0).
#include "gameplay/purpification.hpp"

#include <algorithm>
#include <cmath>

#include "core/json.hpp"
#include "world/level_def.hpp"
#include "world/map_def.hpp"

namespace lim::game {

namespace {
constexpr float kPi2 = 6.2831853f;
// Lage im Modell (tools/blender/models/models_l3_purple.py): x in den Gang, y nach rechts (Blick auf die Wand), z oben
struct Shape {
    const char* kind;
    float gx, gy, gz;  // Mitte des Leuchtens
    float tx, ty, tz;  // Mitte der Botschaft (ty = 0: keine)
};
constexpr Shape kShapes[] = {
    {"stealer", 0.32f, 0.12f, 1.40f, 0.0f, 0.0f, 0.0f},
    {"hound", 0.42f, 0.02f, 0.48f, 0.0f, 1.85f, 1.50f},
};
vec3 local(vec3 base, float yaw, float x, float y, float z) {
    const vec3 n{std::cos(yaw), std::sin(yaw), 0.0f}, r{std::sin(yaw), -std::cos(yaw), 0.0f};
    return base + n * x + r * y + vec3{0.0f, 0.0f, z};
}
float pulse(u32 seed, double time) {  // Stocken der Lampe: kurze Einbrueche, unregelmaessig
    const u32 k = (u32)(time * 13.0);
    const float a = hash01(k * 2654435761u + seed), b = hash01((k + 1) * 2654435761u + seed);
    const float f = (float)(time * 13.0 - std::floor(time * 13.0));
    return a + (b - a) * f;
}
}  // namespace

const char* Purpification::readingText() {
    return "An die Wand gemalt, hastig, die Farbe ist verlaufen: „Mein Liebes – dafür warst du nie gemacht. Eines Tages sehe "
           "ich dich wieder, mit echten Augen, mit echten Händen. Und dann heile ich dich, Fleisch und Geist.“";
}

void Purpification::setup(const world::LevelDef& level, u64 seed) {
    sites_.clear();
    cfg_ = Config{};
    hintRad_ = hintSeen_ = false;
    rng_ = Rng(hash64((i64)seed, "purple", level.id.c_str()));
    const Json& j = level.phenomena;
    if (!j.isObject() || !level.mapped() || !j["purple"].asBool(false)) return;
    cfg_.drain = j["purple_drain"].asNumber(cfg_.drain);
    cfg_.radRadius = j["purple_radius"].asNumber(cfg_.radRadius);
    for (const world::MapSpawn& s : level.map->spawns) {
        if (s.kind != "trace" || s.set != "purple") continue;
        for (const Shape& sh : kShapes) {
            if (!s.hasTag(sh.kind)) continue;
            Site st;
            st.kind = sh.kind;
            st.yaw = (float)s.yaw;
            st.base = vec3{(float)s.x, (float)s.y, (float)s.z} - vec3{std::cos(st.yaw), std::sin(st.yaw), 0.0f} * 0.3f;
            st.glow = local(st.base, st.yaw, sh.gx, sh.gy, sh.gz);
            st.text = sh.ty > 0.0f ? local(st.base, st.yaw, sh.tx, sh.ty, sh.tz) : st.base;
            st.phase = (float)rng_.uniform(0.0, kPi2);
            st.crackleT = rng_.uniform(0.5, cfg_.crackleEvery);
            st.settleT = rng_.uniform(6.0, cfg_.settleEvery);
            st.surgeT = rng_.uniform(8.0, cfg_.surgeEvery);
            sites_.push_back(st);
        }
    }
}

float Purpification::exposure(vec3 p) const {
    float best = 0.0f;
    for (const Site& s : sites_) {
        const vec3 n{std::cos(s.yaw), std::sin(s.yaw), 0.0f};
        const vec3 d = p - s.glow;
        if (dot(p - s.base, n) < -0.05f) continue;  // hinter der Wand
        const float dh = std::sqrt(d.x * d.x + d.y * d.y), dz = std::max(0.0f, std::fabs(d.z) - 0.8f);
        const float dist = std::sqrt(dh * dh + dz * dz);
        const float e = std::clamp(1.0f - (dist - 0.4f) / ((float)cfg_.radRadius - 0.4f), 0.0f, 1.0f);
        best = std::max(best, e * e);
    }
    return best;
}

void Purpification::update(double dt, vec3 eye, vec3 look, bool alive, bool lampOn, vec3 lampPos, vec3 lampDir,
                           std::vector<Event>& out) {
    for (Site& s : sites_) {
        const float d = length(eye - s.glow);
        s.flash = std::max(0.0f, s.flash - (float)dt * 9.0f);
        s.surge = std::max(0.0f, s.surge - (float)dt * 0.35f);
        s.readCool = std::max(0.0, s.readCool - dt);
        // die eigene Lampe auf dem Koerper: die Glut schwillt an (Interpretation: die Energie zieht am Licht)
        bool onIt = false;
        if (lampOn) {
            const vec3 to = s.glow - lampPos;
            const float dl = length(to);
            onIt = dl < 11.0f && dl > 0.05f && dot(to * (1.0f / dl), lampDir) > 0.93f;
        }
        s.lit = std::clamp(s.lit + (float)dt * (onIt ? 0.7f : -0.4f), 0.0f, 1.0f);
        if (d > (float)cfg_.hearRadius + 4.0f) continue;
        // Knistern in Schueben (Gamma? Entladung in den Ziegeln - Interpretation); naeher und angeleuchtet oefter
        s.crackleT -= dt * (1.0 + 1.5 * s.lit + 1.2 * s.surge);
        if (s.crackleT <= 0.0) {
            s.crackleT = cfg_.crackleEvery * rng_.uniform(0.35, 1.65);
            out.push_back({Event::SoundAt, "purple_crackle", s.glow + vec3{0.0f, 0.0f, (float)rng_.uniform(-0.5, 0.6)},
                           (float)rng_.uniform(0.55, 1.0)});
            s.flash = 1.0f;
        }
        s.settleT -= dt;
        if (s.settleT <= 0.0) {  // Mauerwerk und Kadaver knacken (Spannung im Ziegel)
            s.settleT = cfg_.settleEvery * rng_.uniform(0.5, 1.5);
            out.push_back({Event::SoundAt, "purple_settle", s.base + vec3{0.0f, 0.0f, 1.0f}, 0.9f});
        }
        s.surgeT -= dt * (1.0 + 2.0 * s.lit);
        if (s.surgeT <= 0.0) {  // Schwall: das Leuchten schwillt an und laeuft ueber die Bahnen
            s.surgeT = cfg_.surgeEvery * rng_.uniform(0.6, 1.4);
            s.surge = 1.0f;
            out.push_back({Event::SoundAt, "purple_surge", s.glow, 1.0f});
        }
        if (!alive) continue;
        // Hinweise (einmal je Lauf): der Anblick, die Strahlung
        const vec3 to = s.glow - eye;
        const float facing = d > 0.01f ? dot(to * (1.0f / d), look) : 1.0f;
        if (!hintSeen_ && d < 5.5f && facing > 0.8f && dot(eye - s.base, vec3{std::cos(s.yaw), std::sin(s.yaw), 0.0f}) > 0.2f) {
            hintSeen_ = true;
            out.push_back({Event::Toast, "Violett. Ein toter Körper steckt halb in der Wand – und die Ziegel um ihn herum leuchten mit.", eye, 5.0f});
        }
        if (!hintRad_ && exposure(eye) > 0.45f) {
            hintRad_ = true;
            out.push_back({Event::Toast, "Ein Kribbeln auf der Haut, Metall auf der Zunge. Was immer das ist – es strahlt.", eye, 5.0f});
        }
        // die Botschaft neben dem Hound
        if (s.kind == "hound" && s.readCool <= 0.0) {
            const vec3 tt = s.text - eye;
            const float dt2 = length(tt);
            if (dt2 < (float)cfg_.readRadius && dot(tt * (1.0f / std::max(dt2, 0.01f)), look) > 0.88f &&
                dot(eye - s.base, vec3{std::cos(s.yaw), std::sin(s.yaw), 0.0f}) > 0.3f) {
                s.readCool = 75.0;
                out.push_back({Event::Toast, readingText(), eye, 9.0f});
            }
        }
    }
}

float Purpification::glowLevel(const Site& s, double time) const {
    const float breathe = 0.78f + 0.22f * std::sin((float)time * (kPi2 / 6.5f) + s.phase);
    const float swell = s.surge * s.surge * (0.9f + 0.1f * std::sin((float)time * 23.0f));
    return breathe * (1.0f + 0.9f * swell + 0.7f * s.lit) + 0.25f * s.flash;
}

void Purpification::lights(std::vector<Light>& out, vec3 eye, float radius, double time) const {
    std::vector<std::pair<float, const Site*>> near;
    for (const Site& s : sites_) {
        const float d = length(s.glow - eye);
        if (d < radius) near.push_back({d, &s});
    }
    std::sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [d, s] : near) {
        const float g = glowLevel(*s, time);
        Light L;
        L.pos = s->glow;
        L.color = vec3{0.55f, 0.20f, 1.0f} * (0.4f * g);
        L.range = 3.0f + 1.3f * std::min(g, 1.6f);
        out.push_back(L);
    }
}

float Purpification::lampFactor(vec3 eye, double time) const {
    float f = 1.0f;
    for (size_t i = 0; i < sites_.size(); ++i) {
        const Site& s = sites_[i];
        const float d = length(s.glow - eye);
        if (d >= (float)cfg_.lampRadius) continue;
        const float k = std::clamp(((float)cfg_.lampRadius - d) / ((float)cfg_.lampRadius - 0.8f), 0.0f, 1.0f);
        const float p = pulse((u32)i * 7919u + 17u, time);
        // je naeher, desto oefter und tiefer: Einbrueche, wenn das Rauschen unter die Schwelle faellt
        const float dip = std::clamp((0.25f + 0.45f * k - p) / 0.12f, 0.0f, 1.0f);
        f = std::min(f, 1.0f - (0.25f + 0.55f * k) * dip - 0.12f * k);
    }
    return std::max(f, 0.08f);
}

float Purpification::grain(vec3 eye) const {
    float g = 0.0f;
    const float e = exposure(eye);
    for (const Site& s : sites_)
        if (length(s.glow - eye) < (float)cfg_.radRadius + 1.0f) g = std::max(g, s.flash);
    return e * (0.9f + 2.2f * g);
}

}  // namespace lim::game
