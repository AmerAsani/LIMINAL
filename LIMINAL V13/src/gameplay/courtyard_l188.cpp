// V13.3 Teil 4 Level 188: die Wurzelpistole (Entity 39) und die Orte des Horchers (gameplay/courtyard_l188.hpp;
// KREATUR_Wurzelpistole.md, KREATUR_Horcher.md; Wikidot level-188 Rev. 69, entity-39 Rev. 46 - CC BY-SA 3.0).
//
// Ablauf einer Wurzelpistole (Auslegung, im Briefing gekennzeichnet; die Seite: man muss sie mit Kraft herausziehen, dann
// schlingen sich die Wurzeln um Hand und Unterarm, sind einige Sekunden eiskalt und fuellen sich mit Blut; wo zu wenig Metall im
// Boden ist, bleibt die Schale schwach und kann beim Schuss versagen; Gesunde spueren leichte Muedigkeit):
//   ruht      verkuemmert im Moos bzw. im Riss; kommt jemand naeher als 2,4 m, heben sich die Wurzelspitzen, der Lauf dreht sich
//             ein wenig nach ihm, die Rinde knarrt. Ab und zu sickert ein Tropfen leerer Huelsen aus dem Lauf.
//   Griff     wer sie anfasst (herausziehen will), bekommt sie nicht los - sie sitzt fest (kein Eisen, keine Kraft zum Loesen);
//             die Wurzeln schlingen sich in 1 s um Hand und Unterarm (eiskalt), dann trinkt sie: drei Zuege (je 5 Schaden).
//   Schuss    nach 3,9 s ist die Kammer voll, die schwache Schale entlaedt sich von selbst: ein Knall, der durch Hof und Gaenge
//             hallt (Laerm - der Horcher hoert ihn), Huelsen spritzen, der Schuss faellt kurz vor ihr ins Moos und keimt (ein
//             Keimling waechst in einer Minute). Die Wurzeln lassen los; wer gespendet hat, ist 45 s muede (Tempo 86 %).
//   losreissen wer sich waehrend des Griffs weiter als 1,7 m entfernt, reisst die Wurzeln aus dem Fleisch (9 Schaden, kein Schuss).
//   welk      danach 150 s schlaff und kalt (nach dem Losreissen 90 s); wer sie dann anfasst, dem passiert nichts.
// Mehrspieler: der Host entscheidet Griff, Zuege, Schuss, Losreissen und Erholung und schickt sie als Weltereignis Dropped
// "l188" (Kennzahl: Art << 24 | Pistole << 16 | Spieler << 8 | Zusatz, Ort: Keimling); jeder Rechner rechnet daraus Bild und
// Klang. Protokoll unveraendert.
#include "gameplay/courtyard_l188.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "world/level_def.hpp"
#include "world/map_def.hpp"

namespace lim::game {

namespace {
constexpr double kWrapT = 1.0, kPulse0 = 1.45, kPulseDt = 0.8, kFireT = 3.9, kReleaseT = 1.4, kWiltT = 150.0, kTornT = 90.0;
constexpr int kPulses = 3, kMaxSprouts = 6;
constexpr float kPulseHurt = 5.0f, kTearHurt = 9.0f, kTearDist = 1.7f, kSenseR = 2.4f, kTired = 45.0f;
constexpr float kPiC = 3.14159265f;
enum NetKind : u32 { NK_GRIP = 1, NK_PULSE = 2, NK_FIRE = 3, NK_TEAR = 4, NK_RECOVER = 5 };

u32 code(u32 kind, u32 a, u32 b, u32 c) { return (kind << 24) | ((a & 0xFF) << 16) | ((b & 0xFF) << 8) | (c & 0xFF); }
vec3 dirOf(float yaw) { return {std::cos(yaw), std::sin(yaw), 0.0f}; }
float hd2(vec3 a, vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }
vec3 flat(vec3 v) { return {v.x, v.y, 0.0f}; }
float sm(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
void spring(float& x, float& v, float target, float omega, double dtd) {
    const float dt = (float)dtd;
    if (dt <= 0.0f) return;
    const float f = 1.0f + 2.0f * dt * omega, oo = omega * omega, hoo = dt * oo, hhoo = dt * hoo;
    const float inv = 1.0f / (f + hhoo);
    const float nx = (f * x + dt * v + hhoo * target) * inv;
    const float nv = (v + hoo * (target - x)) * inv;
    x = nx, v = nv;
}
vec3 nrmC(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-6f ? v / l : fb;
}
mat4 rotX(float a) {
    mat4 m = mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.c[1] = {0, c, s, 0};
    m.c[2] = {0, -s, c, 0};
    return m;
}
mat4 rotY(float a) {
    mat4 m = mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.c[0] = {c, 0, -s, 0};
    m.c[2] = {s, 0, c, 0};
    return m;
}
vec3 applyM(const mat4& m, vec3 p) {
    const vec4 r = m * vec4{p.x, p.y, p.z, 1.0f};
    return {r.x, r.y, r.z};
}
// Glied von p0 nach p1 (Modell entlang +x, Laenge kSegLen), Querschnitt rs
mat4 segMatrix(vec3 p0, vec3 p1, float rs, float segLen) {
    const vec3 d = p1 - p0;
    const float len = std::max(1e-4f, length(d));
    const vec3 X = d / len;
    const vec3 h = std::fabs(X.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0};
    const vec3 Y = nrmC(cross(h, X), {0, 1, 0});
    const vec3 Z = cross(X, Y);
    mat4 m = mat4::identity();
    const float sx = len / segLen;
    m.c[0] = {X.x * sx, X.y * sx, X.z * sx, 0};
    m.c[1] = {Y.x * rs, Y.y * rs, Y.z * rs, 0};
    m.c[2] = {Z.x * rs, Z.y * rs, Z.z * rs, 0};
    m.c[3] = {p0.x, p0.y, p0.z, 1};
    return m;
}
void push(std::vector<VisibleMesh>& out, Courtyard188::Part& p, const mat4& w, bool dynamic, float cullR) {
    VisibleMesh v;
    v.model = &p.model;
    v.meshId = &p.id;
    v.world = w;
    v.dynamic = dynamic;
    if (cullR > 0.0f) v.cullCenter = applyM(w, vec3{0, 0, 0}), v.cullRadius = cullR;
    out.push_back(v);
}
}  // namespace

void Courtyard188::setup(const world::LevelDef& level, u64 seed) {
    *this = Courtyard188{};
    rng_ = Rng(seed ^ 0x188E39ull);
    if (!level.mapped()) return;
    const auto& sp = level.map->spawns;
    for (const world::MapSpawn& s : sp) {
        if (s.kind != "trace") continue;
        const vec3 p{(float)s.x, (float)s.y, (float)s.z};
        if (s.set == "hr") {
            if (s.hasTag("knock")) knock_.push_back(p);
            if (s.hasTag("emerge")) emerge_.push_back(p), emergeYaw_.push_back((float)s.yaw);
        } else if (s.set == "e39") {
            if (s.hasTag("moss") || s.hasTag("floor")) {
                Gun g;
                g.pos = p, g.yaw = (float)s.yaw, g.kind = s.hasTag("floor") ? 1 : 0;
                g.lean = g.kind ? 0.32f : 0.22f;
                g.id = std::format("trace:l188_e39_{}", guns_.size());
                g.dripT = rng_.uniform(3.0, 9.0);
                guns_.push_back(std::move(g));
            } else if (s.hasTag("note")) {
                notePos_ = p, noteYaw_ = (float)s.yaw, hasNote_ = true;
            } else if (s.hasTag("seed")) {
                mossSpots_.push_back(p);
            }
        }
    }
    setupParts();
    if (any()) log::info("Hof: {} Wurzelpistolen, {} Keimorte, {} Klopftueren, {} Zellentueren", guns_.size(), mossSpots_.size(), knock_.size(), emerge_.size());
}

void Courtyard188::setupParts() {
    for (Gun& g : guns_) {
        g.body.model = "prop:e39_gun";
        g.bed.model = g.kind ? "prop:e39_bed_crack" : "prop:e39_bed_moss";
        g.husk.model = "prop:e39_husk";
        for (auto& row : g.seg)
            for (Part& p : row) p.model = "prop:e39_root";
    }
}

bool Courtyard188::noteAt(vec3& pos, float& yaw) const {
    if (!hasNote_) return false;
    pos = notePos_, yaw = noteYaw_;
    return true;
}

// Lage des Koerpers: Griff im Boden, etwas schief (verkuemmert), welk sackt der Lauf ab; wittert sie jemanden, dreht sie sich
// ein wenig nach ihm
static mat4 gunMatrix(const Courtyard188::Gun& g) {
    float yaw = g.yaw;
    if (g.sense > 0.01f) {
        const float want = std::atan2(g.senseAt.y - g.pos.y, g.senseAt.x - g.pos.x);
        yaw += std::clamp(std::remainder(want - g.yaw, 2.0f * kPiC), -0.45f, 0.45f) * std::clamp(g.sense, 0.0f, 1.0f);
    }
    const float roll = g.lean * (0.6f + 0.9f * g.droop);
    const float pitch = 0.08f + 0.4f * g.droop;  // Lauf nach unten
    return mat4::translation(g.pos - vec3{0, 0, 0.012f}) * mat4::rotationZ(yaw) * rotY(pitch) * rotX(roll);
}

vec3 Courtyard188::muzzle(const Gun& g) const { return applyM(gunMatrix(g), vec3{kMuzzleX + 0.006f, 0.0f, kMuzzleZ}); }

void Courtyard188::startGrip(Gun& g, int player, std::vector<Event>& out) {
    const int gi = (int)(&g - guns_.data());
    g.state = 1, g.t = 0.0, g.drank = 0, g.victim = player;
    const vec3 grip = applyM(gunMatrix(g), vec3{0.0f, 0.0f, kGripTop * 0.7f});
    out.push_back({Event::SoundAt, 0, "e39_wrap", grip, 0.85f});
    out.push_back({Event::Toast, player,
                   g.kind ? "Du willst sie aus dem Riss ziehen – sie sitzt fest. Wurzeln schlingen sich um Hand und Unterarm."
                          : "Du willst sie aus dem Moos ziehen – sie sitzt fest. Wurzeln schlingen sich um Hand und Unterarm.",
                   grip, 3.4f});
    out.push_back({Event::Net, 0, "", grip, 1.0f, code(NK_GRIP, (u32)gi, (u32)player, 0)});
    log::info("Hof: Wurzelpistole {} - Griff (Spieler {})", gi, player);
}

void Courtyard188::fire(Gun& g, int gi, bool authoritative, std::vector<Event>& out) {
    const vec3 mz = muzzle(g);
    g.state = 2, g.t = 0.0, g.flashT = 0.0, g.huskT = 0.0;
    ++g.shots;
    out.push_back({Event::SoundAt, 0, "e39_bang", mz, 1.0f});
    if (!authoritative) return;
    out.push_back({Event::Noise, 0, "", mz, 1.0f});
    // der Schuss faellt kurz vor ihr in den Boden und keimt: der naechste Keimort vor dem Lauf (1,5 .. 6 m)
    const vec3 fw = nrmC(flat(mz - g.pos), dirOf(g.yaw));
    int best = -1;
    float bs = 1e9f;
    for (size_t i = 0; i < mossSpots_.size(); ++i) {
        const vec3 d = mossSpots_[i] - g.pos;
        const float dist = std::hypot(d.x, d.y);
        if (dist < 1.2f || dist > 7.0f || std::fabs(d.z) > 1.0f) continue;
        const float s = -dot(nrmC(flat(d), fw), fw) * 3.0f + std::fabs(dist - 3.0f) * 0.4f + (float)rng_.uniform(0.0, 1.2);
        bool used = false;
        for (const Sprout& sp : sprouts_) used = used || hd2(sp.pos, mossSpots_[i]) < 0.3f;
        if (!used && s < bs) bs = s, best = (int)i;
    }
    vec3 at = best >= 0 ? mossSpots_[(size_t)best] : g.pos + fw * 2.0f;
    if (best >= 0) {
        if ((int)sprouts_.size() >= kMaxSprouts) sprouts_.erase(sprouts_.begin());
        Sprout s;
        s.pos = at, s.yaw = (float)rng_.uniform(0.0, 6.283), s.age = 0.0;
        s.part.model = "prop:e39_sprout";
        sprouts_.push_back(std::move(s));
    }
    if (g.victim >= 0 && g.victim < kPlayers) {
        tired_[(size_t)g.victim] = kTired;
        out.push_back({Event::Tired, g.victim, "", mz, kTired});
        out.push_back({Event::Toast, g.victim,
                       "Ein Knall – die Kammer hat sich von selbst entladen. Die Wurzeln lassen los. Du bist müde, als hättest du Blut gespendet.",
                       mz, 4.5f});
    }
    out.push_back({Event::Net, 0, "", at, 1.0f, code(NK_FIRE, (u32)gi, (u32)std::max(0, g.victim), best >= 0 ? 1u : 0u)});
    log::info("Hof: Wurzelpistole {} feuert (Schuss {}), Keim {}", gi, g.shots, best >= 0 ? std::format("bei ({:.1f}, {:.1f})", at.x, at.y) : "keiner");
}

void Courtyard188::update(double dt, const std::array<PlayerIn, kPlayers>& players, bool authoritative, std::vector<Event>& out) {
    for (float& t : tired_) t = std::max(0.0f, t - (float)dt);
    if (hold_) dt = 0.0;
    for (size_t gi = 0; gi < guns_.size(); ++gi) {
        Gun& g = guns_[gi];
        g.t += dt;
        // wittern: der naechste Lebende in 2,4 m (nur in Ruhe)
        float best = 1e9f;
        vec3 at = g.senseAt;
        for (const PlayerIn& p : players)
            if (p.alive && std::fabs(p.feet.z - g.pos.z) < 2.0f)
                if (const float d = hd2(p.feet, g.pos); d < best) best = d, at = p.eye;
        const float want = g.state == 0 && best < kSenseR ? 1.0f - 0.6f * best / kSenseR : 0.0f;
        if (want > 0.0f) g.senseAt = at;
        spring(g.sense, g.senseV, want, 2.6f, dt);
        if (g.state == 0 && want > 0.3f) {
            g.creakT -= dt;
            if (g.creakT <= 0.0) g.creakT = rng_.uniform(2.4, 4.8), out.push_back({Event::SoundAt, 0, "e39_creak", g.pos + vec3{0, 0, 0.08f}, 0.55f});
        }
        if (g.state != 3 && g.state != 4) {
            g.dripT -= dt;
            if (g.dripT <= 0.0 && g.huskT < 0.0) g.dripT = rng_.uniform(7.0, 15.0), g.huskT = 0.0, out.push_back({Event::SoundAt, 0, "e39_drip", muzzle(g), 0.35f});
        }
        if (g.huskT >= 0.0 && (g.huskT += dt) > 2.4) g.huskT = -1.0;
        if (g.flashT >= 0.0 && (g.flashT += dt) > 0.14) g.flashT = -1.0;
        spring(g.droop, g.droopV, g.state >= 3 ? 1.0f : (g.state == 2 ? 0.55f : 0.12f), g.state >= 2 ? 1.6f : 0.9f, dt);
        spring(g.wrap, g.wrapV, g.state == 1 ? 1.0f : 0.0f, g.state == 1 ? 3.0f : 2.2f, dt);
        if (g.victim >= 0 && g.victim < kPlayers && players[(size_t)g.victim].alive) g.victimEye = players[(size_t)g.victim].eye;
        switch (g.state) {
            case 1: {
                if (!authoritative) break;
                const PlayerIn* v = g.victim >= 0 && g.victim < kPlayers ? &players[(size_t)g.victim] : nullptr;
                if (!v || !v->alive) {  // tot oder fort: sie laesst los
                    g.state = 3, g.t = 0.0, g.cool = kTornT;
                    out.push_back({Event::Net, 0, "", g.pos, 1.0f, code(NK_TEAR, (u32)gi, 0, 0)});
                    break;
                }
                if (hd2(v->feet, g.pos) > kTearDist) {  // losgerissen
                    g.state = 4, g.t = 0.0, g.cool = kTornT;
                    ++g.tears;
                    out.push_back({Event::Hurt, g.victim, "tear", v->eye, kTearHurt});
                    out.push_back({Event::SoundAt, 0, "e39_tear", g.pos + vec3{0, 0, 0.1f}, 0.9f});
                    out.push_back({Event::Toast, g.victim, "Du reißt dich los – die Wurzeln reißen aus dem Fleisch. Es blutet.", v->eye, 3.0f});
                    out.push_back({Event::Net, 0, "", g.pos, 1.0f, code(NK_TEAR, (u32)gi, (u32)g.victim, 1)});
                    log::info("Hof: Wurzelpistole {} - losgerissen (Spieler {})", gi, g.victim);
                    break;
                }
                const int n = g.t < kPulse0 ? 0 : std::min(kPulses, (int)((g.t - kPulse0) / kPulseDt) + 1);
                while (g.drank < n) {
                    ++g.drank;
                    const vec3 grip = applyM(gunMatrix(g), vec3{0.0f, 0.0f, kGripTop * 0.7f});
                    out.push_back({Event::Hurt, g.victim, "drink", v->eye, kPulseHurt});
                    out.push_back({Event::SoundAt, 0, "e39_suck", grip, 0.8f});
                    if (g.drank == 1) out.push_back({Event::Toast, g.victim, "Eiskalt. Dann zieht es im Arm – sie trinkt.", v->eye, 2.6f});
                    out.push_back({Event::Net, 0, "", grip, 1.0f, code(NK_PULSE, (u32)gi, (u32)g.victim, (u32)g.drank)});
                }
                if (g.t >= kFireT) fire(g, (int)gi, true, out);
                break;
            }
            case 2:
                if (g.t > kReleaseT) {
                    g.state = 3, g.t = 0.0, g.cool = kWiltT, g.victim = -1;
                    out.push_back({Event::SoundAt, 0, "e39_wilt", g.pos + vec3{0, 0, 0.1f}, 0.5f});
                }
                break;
            case 3:
            case 4:
                if (g.wrap < 0.05f) g.victim = -1;
                g.cool -= dt;
                if (authoritative && g.cool <= 0.0) {
                    g.state = 0, g.t = 0.0, g.drank = 0, g.victim = -1;
                    out.push_back({Event::Net, 0, "", g.pos, 1.0f, code(NK_RECOVER, (u32)gi, 0, 0)});
                }
                break;
            default: break;
        }
    }
    for (Sprout& s : sprouts_) s.age += dt;
}

bool Courtyard188::touch(int idx, int player, vec3 eye, std::string& msg, std::vector<Event>& out) {
    if (idx < 0 || idx >= (int)guns_.size()) return false;
    Gun& g = guns_[(size_t)idx];
    if (g.state == 3 || g.state == 4) {
        msg = "Welk. Die Wurzeln hängen schlaff und kalt – sie rührt sich nicht.";
        return false;
    }
    if (g.state != 0) {
        msg = "Ihre Wurzeln sind schon um einen Arm geschlungen.";
        return false;
    }
    g.victimEye = eye;
    startGrip(g, player, out);
    return true;
}

void Courtyard188::applyNet(u32 c, vec3 pos, std::vector<Event>& out) {
    const u32 kind = netKind(c);
    const int gi = (int)((c >> 16) & 0xFF), player = (int)((c >> 8) & 0xFF), extra = (int)(c & 0xFF);
    if (gi < 0 || gi >= (int)guns_.size()) return;
    Gun& g = guns_[(size_t)gi];
    switch (kind) {
        case NK_GRIP:
            g.state = 1, g.t = 0.0, g.drank = 0, g.victim = player;
            out.push_back({Event::SoundAt, 0, "e39_wrap", pos, 0.85f});
            break;
        case NK_PULSE:
            g.drank = extra, g.state = 1;
            out.push_back({Event::SoundAt, 0, "e39_suck", pos, 0.8f});
            break;
        case NK_FIRE: {
            g.victim = player;
            fire(g, gi, false, out);
            if (extra) {
                if ((int)sprouts_.size() >= kMaxSprouts) sprouts_.erase(sprouts_.begin());
                Sprout s;
                s.pos = pos, s.yaw = (float)(pos.x * 1.7f + pos.y * 0.9f), s.age = 0.0;
                s.part.model = "prop:e39_sprout";
                sprouts_.push_back(std::move(s));
            }
            break;
        }
        case NK_TEAR:
            g.state = extra ? 4 : 3, g.t = 0.0, g.cool = kTornT;
            if (extra) out.push_back({Event::SoundAt, 0, "e39_tear", g.pos + vec3{0, 0, 0.1f}, 0.9f});
            break;
        case NK_RECOVER:
            g.state = 0, g.t = 0.0, g.drank = 0, g.victim = -1;
            break;
        default: break;
    }
}

bool Courtyard188::setState(const std::string& key, double v) {
    const int i = (int)v;
    auto gun = [&]() -> Gun* { return i >= 0 && i < (int)guns_.size() ? &guns_[(size_t)i] : nullptr; };
    std::vector<Event> sink;
    if (key == "hold") return hold_ = v > 0.5, true;
    if (key == "grip") {  // Bilder: Spieler 0 greift Pistole i
        Gun* g = gun();
        if (!g) return false;
        g->state = 0;
        startGrip(*g, 0, sink);
        return true;
    }
    if (key == "wrap") {  // Bilder: Griff fest (Wurzeln ganz um den Arm), Zeit eingefroren
        Gun* g = gun();
        if (!g) return false;
        g->state = 1, g->wrap = 1.0f, g->wrapV = 0.0f, g->victim = 0, g->t = 2.0;
        return true;
    }
    if (key == "fire") {
        Gun* g = gun();
        if (!g) return false;
        fire(*g, i, true, sink);
        return true;
    }
    if (key == "wilt") {
        Gun* g = gun();
        if (!g) return false;
        g->state = 3, g->t = 0.0, g->cool = kWiltT, g->droop = 1.0f;
        return true;
    }
    if (key == "recover") {
        Gun* g = gun();
        if (!g) return false;
        g->state = 0, g->t = 0.0, g->cool = 0.0, g->drank = 0, g->victim = -1, g->droop = 0.12f, g->wrap = 0.0f;
        return true;
    }
    if (key == "drip") {
        Gun* g = gun();
        if (!g) return false;
        g->huskT = 0.0;
        return true;
    }
    if (key == "sprouts") {  // Bilder: Keimlinge an den ersten n Keimorten, ausgewachsen
        sprouts_.clear();
        for (int k = 0; k < std::min(i, (int)mossSpots_.size()); ++k) {
            Sprout s;
            s.pos = mossSpots_[(size_t)k], s.yaw = 0.7f * (float)k, s.age = 120.0;
            s.part.model = "prop:e39_sprout";
            sprouts_.push_back(std::move(s));
        }
        return true;
    }
    return false;
}

void Courtyard188::collect(std::vector<VisibleMesh>& out, vec3 eye) {
    for (size_t gi = 0; gi < guns_.size(); ++gi) {
        Gun& g = guns_[gi];
        const float de = length(g.pos - eye);
        if (de > 45.0f) continue;
        push(out, g.bed, mat4::translation(g.pos) * mat4::rotationZ(g.yaw), false, 0.4f);
        const mat4 B = gunMatrix(g);
        push(out, g.body, B, true, 0.3f);
        // Huelsen: ein Tropfen faellt aus dem Lauf und bleibt kurz liegen
        if (g.huskT >= 0.0) {
            const vec3 mz = muzzle(g);
            const float t = (float)std::max(0.0, g.huskT - 0.6);  // erst quillt er
            const float s = std::min(1.0f, (float)g.huskT / 0.6f);
            vec3 p = mz - vec3{0, 0, 0.004f + 4.9f * t * t};
            p.z = std::max(p.z, g.pos.z + 0.004f);
            push(out, g.husk, mat4::translation(p) * mat4::scale(vec3{s, s, s * (p.z <= g.pos.z + 0.0045f ? 0.35f : 1.0f)}), true, 0.05f);
        }
        if (de > 22.0f) continue;  // Wurzeln nur nah
        const vec3 base = g.pos + vec3{0, 0, 0.004f};
        const vec3 hand = applyM(B, vec3{-0.004f, 0.0f, kGripTop * 0.62f});
        const vec3 arm = nrmC(g.victimEye - vec3{0, 0, 0.32f} - hand, {0, 0, 1});  // zur Schulter
        const vec3 au = nrmC(cross(arm, std::fabs(arm.z) < 0.9f ? vec3{0, 0, 1} : vec3{1, 0, 0}), {0, 1, 0});
        const vec3 av = cross(arm, au);
        const bool drinking = g.state == 1 && g.t > kWrapT;
        for (int i = 0; i < kTendrils; ++i) {
            const float ang = g.yaw + (float)i * (2.0f * kPiC / kTendrils) + 0.35f * std::sin((float)i * 2.1f + (float)gi);
            const vec3 dr = dirOf(ang), side{-dr.y, dr.x, 0.0f};
            vec3 pts[kSegs + 1];
            for (int j = 0; j <= kSegs; ++j) {
                const float s = (float)j / kSegs;
                // Ruhe: ein kurzes Stueck ueber dem Boden, dann in die Erde (Runde 1: lange, duenne Wurzeln lagen frei wie
                // Spinnenbeine); im Riss des Gangs bleiben sie flach im Riss
                const float r = 0.02f + s * 0.42f;
                vec3 rest = base + dr * r + side * (0.025f * std::sin(s * 5.0f + (float)i));
                rest.z += g.kind == 0 ? 0.02f * (1.0f - s) - 0.11f * s * s : 0.004f - 0.03f * s * s;
                // wittern: die Wurzeln zucken und kriechen ein wenig zum Naechsten (kaum Hub - es tastet, es steht nicht auf)
                if (g.sense > 0.01f) {
                    const float k = g.sense * std::sin(kPiC * std::min(1.0f, s * 1.6f));
                    rest += nrmC(flat(g.senseAt - rest), dr) * (0.035f * k) + side * (0.01f * std::sin((float)g.t * 7.0f + (float)(i * 5 + j)) * k);
                    rest.z += (0.012f + 0.006f * std::sin((float)g.t * 9.0f + (float)(i * 7 + j))) * k;
                }
                vec3 wp;
                if (s < 0.35f) {  // vom Griffende hinauf zur Hand
                    const float u = s / 0.35f;
                    wp = base + (hand - base) * u + dr * (0.07f * std::sin(kPiC * u));
                } else {          // um Hand und Unterarm, spiralig bis zum Ellbogen (der Arm zeigt zur Schulter des Gehaltenen)
                    const float u = (s - 0.35f) / 0.65f;
                    const float phi = ang + u * 5.0f * kPiC;
                    const float rad = 0.045f + 0.012f * u + (drinking ? 0.005f * std::sin((float)g.t * 8.0f - u * 6.0f) : 0.0f);
                    wp = hand + arm * (0.03f + u * 0.5f) + (au * std::cos(phi) + av * std::sin(phi)) * rad;
                }
                const float w = sm(g.wrap * 1.5f - s * 0.5f);
                pts[j] = rest + (wp - rest) * w;
            }
            for (int j = 0; j < kSegs; ++j) {
                const float s = (float)j / kSegs;
                float rs = 1.55f - 1.1f * s;  // Runde 1: am Griff zu duenn
                if (drinking) rs *= 1.0f + 0.14f * std::max(0.0f, std::sin((float)g.t * 8.0f - s * 6.0f));
                push(out, g.seg[i][j], segMatrix(pts[j], pts[j + 1], rs, kSegLen), g.state != 0 || g.sense > 0.01f, 0.0f);
            }
        }
    }
    for (Sprout& s : sprouts_) {
        if (length(s.pos - eye) > 30.0f) continue;
        const float k = std::clamp(0.2f + (float)s.age / 60.0f, 0.2f, 1.0f);
        push(out, s.part, mat4::translation(s.pos) * mat4::rotationZ(s.yaw) * mat4::scale(vec3{k, k, k}), s.age < 60.0, 0.1f);
    }
}

void Courtyard188::lights(std::vector<Light>& out, vec3 eye) const {
    for (const Gun& g : guns_) {
        if (g.flashT < 0.0 || length(g.pos - eye) > 40.0f) continue;
        const float k = 1.0f - (float)g.flashT / 0.14f;
        out.push_back({muzzle(g), vec3{1.0f, 0.72f, 0.42f} * (6.0f * k), 7.0f, {0.0f, 0.0f, -1.0f}});
    }
}

}  // namespace lim::game
