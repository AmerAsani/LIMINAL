#include "app/soundscape.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/math.hpp"
#include "render/flicker.hpp"
#include "world/materials.hpp"
#include "world/room.hpp"
#include "world/world.hpp"

namespace lim::app {

namespace {

// Klang je Leuchtenart (Index = ChunkLight::hum)
struct LampKind {
    const char* sound;
    int variants;
    float volume, radius, send;
};
constexpr LampKind kKinds[5] = {
    {"", 0, 0.0f, 0.0f, 0.0f},
    {"lamp_hum_", 4, 0.13f, 9.0f, 0.55f},  // Leuchtstoffroehre (Buero)
    {"lamp_ind_", 2, 0.16f, 15.0f, 0.7f},  // Industrieleuchte
    {"lamp_emg_", 2, 0.08f, 6.0f, 0.4f},   // Notleuchte
    {"lamp_hum_", 4, 0.06f, 6.0f, 0.5f},   // Wandleuchte (kleiner Leuchtkasten)
};
constexpr int kMaxLampVoices = 8;

u64 lampKey(vec3 p) {
    return hash64((i64)std::lround(p.x * 4.0f), (i64)std::lround(p.y * 4.0f), (i64)std::lround(p.z * 4.0f));
}

// Schallschluckgrad (mittlere Frequenzen) der Oberflaechen
float floorAbsorption(world::PlanePattern p) {
    switch (p) {
        case world::PlanePattern::Carpet: return 0.30f;
        case world::PlanePattern::Grate: return 0.15f;
        default: return 0.03f;
    }
}
float ceilAbsorption(world::PlanePattern p) {
    return p == world::PlanePattern::Tiles ? 0.55f : 0.04f;  // Akustik-Deckenplatten
}
float wallAbsorption(world::WallPattern w) {
    switch (w) {
        case world::WallPattern::Wallpaper: return 0.08f;
        case world::WallPattern::Panels: return 0.10f;
        case world::WallPattern::Plain: return 0.05f;
        default: return 0.03f;  // Ziegel, Bloecke, Beton
    }
}

}  // namespace

float Soundscape::occlusion(world::World& world, vec3 a, vec3 b) {
    // Strahl durch das Kachelraster (DDA). Jede massive Kachel bzw. jede Kachel, deren Boden/Decke
    // der Strahl schneidet, zaehlt als Hindernis; zwei Hindernisse = voll verdeckt.
    vec2 d{b.x - a.x, b.y - a.y};
    i64 cx = ifloor(a.x), cy = ifloor(a.y);
    const i64 ex = ifloor(b.x), ey = ifloor(b.y);
    const int sx = d.x >= 0 ? 1 : -1, sy = d.y >= 0 ? 1 : -1;
    const float adx = std::max(std::fabs(d.x), 1e-6f), ady = std::max(std::fabs(d.y), 1e-6f);
    float tdx = 1.0f / adx, tdy = 1.0f / ady;
    float tmx = (sx > 0 ? (float)(cx + 1) - a.x : a.x - (float)cx) * tdx;
    float tmy = (sy > 0 ? (float)(cy + 1) - a.y : a.y - (float)cy) * tdy;
    float t0 = 0.0f;
    int blocked = 0;
    for (int i = 0; i < 96; ++i) {
        float t1 = std::min(std::min(tmx, tmy), 1.0f);
        if (i > 0 && !(cx == ex && cy == ey)) {
            const world::Tile* tile = world.tileIfLoaded(cx, cy);
            if (tile) {
                float za = a.z + (b.z - a.z) * t0, zb = a.z + (b.z - a.z) * t1;
                if (tile->solid() || std::min(za, zb) < tile->floor - 0.05f || std::max(za, zb) > tile->ceil + 0.05f)
                    if (++blocked >= 2) return 1.0f;
            }
        }
        if (t1 >= 1.0f || (cx == ex && cy == ey)) break;
        if (tmx < tmy) {
            cx += sx;
            t0 = tmx;
            tmx += tdx;
        } else {
            cy += sy;
            t0 = tmy;
            tmy += tdy;
        }
    }
    return blocked * 0.55f;
}

void Soundscape::reset(audio::AudioSystem& audio) {
    for (auto& [k, l] : lamps_)
        if (l.voice >= 0) audio.stopLoop(l.voice);
    lamps_.clear();
    if (bedId_ >= 0) audio.stopLoop(bedId_);
    bedId_ = -1;
    bed_ = 0.0f;
    acousticRoom_ = nullptr;
    scanTimer_ = 0.0;
}

void Soundscape::update(double dt, double time, audio::AudioSystem& audio, world::World& world, const world::Room* room,
                        vec3 listener, double sanity, bool active) {
    updateLamps(dt, time, audio, world, listener);
    updateAcoustics(world, room, audio);
    if (active) updateDistant(dt, audio, world, room, listener, sanity);
}

void Soundscape::updateLamps(double dt, double time, audio::AudioSystem& audio, world::World& world, vec3 listener) {
    scanTimer_ -= dt;
    const bool scan = scanTimer_ <= 0.0;
    float fluorescent = 0.0f;
    if (scan) {
        scanTimer_ = 0.15;
        // Kandidaten: alle klingenden Leuchten der umliegenden Chunks, nach wahrgenommener Lautstaerke
        struct Cand {
            float gain;
            u64 key;
            const world::ChunkLight* l;
        };
        std::vector<Cand> cands;
        const i64 pcx = ifloor(listener.x) >> world::CS_SHIFT, pcy = ifloor(listener.y) >> world::CS_SHIFT;
        for (i64 dy = -2; dy <= 2; ++dy)
            for (i64 dx = -2; dx <= 2; ++dx) {
                const world::ChunkData* cd = world.chunk({pcx + dx, pcy + dy});
                if (!cd) continue;
                for (const world::ChunkLight& l : cd->lights) {
                    if (l.hum == 0 || l.hum > 4) continue;
                    const LampKind& k = kKinds[l.hum];
                    float d = length(l.pos - listener);
                    if (d >= k.radius) continue;
                    float a = 1.0f - d / k.radius;
                    cands.push_back({k.volume * a * a, lampKey(l.pos), &l});
                }
            }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.gain > b.gain; });
        for (auto& [key, lamp] : lamps_) lamp.wanted = false;
        for (size_t i = 0; i < cands.size() && i < (size_t)kMaxLampVoices; ++i) {
            const world::ChunkLight& l = *cands[i].l;
            Lamp& lamp = lamps_[cands[i].key];
            lamp.wanted = true;
            lamp.pos = l.pos;
            lamp.kind = l.hum;
            lamp.flickerMode = l.flickerMode;
            lamp.flickerSeed = l.flickerSeed;
        }
        for (auto& [key, lamp] : lamps_) {
            if (!lamp.wanted) continue;
            // Verdeckung: Ohr -> Leuchte (knapp unter der Decke)
            float occ = occlusion(world, listener, lamp.pos - vec3{0.0f, 0.0f, 0.1f});
            lamp.occTarget = occ;
            if (lamp.voice < 0) lamp.occ = occ;  // neue Quellen sofort richtig
        }
    }
    const float tt = (float)std::fmod(time, 3600.0);
    for (auto it = lamps_.begin(); it != lamps_.end();) {
        Lamp& lamp = it->second;
        const LampKind& k = kKinds[lamp.kind];
        float target = lamp.wanted ? 1.0f : 0.0f;
        lamp.level += (target - lamp.level) * (float)std::min(1.0, dt * 1.6);
        if (!lamp.wanted && lamp.level < 0.01f) {
            if (lamp.voice >= 0) audio.stopLoop(lamp.voice);
            it = lamps_.erase(it);
            continue;
        }
        if (lamp.voice < 0 && lamp.wanted) {
            u64 h = lampKey(lamp.pos);
            std::string name = std::format("{}{}", k.sound, (int)(h % (u64)k.variants));
            lamp.voice = audio.startLoop(name, 0.0f, audio::Bus::Ambience);
            if (lamp.voice >= 0) {
                audio.setLoopSend(lamp.voice, k.send);
                audio.setLoopPitch(lamp.voice, 0.995f + 0.01f * hash01((u32)h));  // Exemplarstreuung
            }
        }
        lamp.occ += (lamp.occTarget - lamp.occ) * (float)std::min(1.0, dt * 6.0);
        // Flackern: gedimmte Roehre brummt leiser; alte Roehren knacken beim Einbruch
        float f = gfx::flickerFactor(lamp.flickerMode, lamp.flickerSeed, tt);
        const float dark = (int)lamp.flickerSeed == boSeed_ ? boAmount_ : 0.0f;  // V7: Stromausfall
        lamp.tickCooldown -= (float)dt;
        if (lamp.flickerMode == 2 && lamp.lastFlicker >= 0.93f && f < 0.93f && lamp.tickCooldown <= 0.0f) {
            audio.playAt("lamp_tick", lamp.pos, 0.3f * lamp.level, k.radius, 0.92f + 0.16f * hash01((u32)lampKey(lamp.pos)),
                         lamp.occ);
            lamp.tickCooldown = 0.8f;
        }
        lamp.lastFlicker = f;
        if (lamp.voice >= 0) {
            audio.setLoopVolume(lamp.voice, k.volume * lamp.level * (0.55f + 0.45f * f) * (1.0f - dark));
            audio.setLoopSpatial(lamp.voice, {lamp.pos, k.radius, lamp.occ});
        }
        if (lamp.kind == 1) {
            float d = length(lamp.pos - listener);
            fluorescent += lamp.level * std::max(0.0f, 1.0f - d / 18.0f) * (1.0f - 0.6f * lamp.occ) * (1.0f - dark);
        }
        ++it;
    }
    // Diffuses Grundsummen vieler Roehren (grosse Raeume): leise, ohne Richtung
    if (bedId_ < 0) bedId_ = audio.startLoop("hum", 0.0f, audio::Bus::Ambience);
    float bedTarget = std::min(0.05f, fluorescent * 0.012f);
    bed_ += (bedTarget - bed_) * (float)std::min(1.0, dt * 1.0);
    if (bedId_ >= 0) audio.setLoopVolume(bedId_, bed_);
}

void Soundscape::updateAcoustics(world::World& world, const world::Room* room, audio::AudioSystem& audio) {
    if (!room || room == acousticRoom_) return;
    acousticRoom_ = room;
    const auto& bank = world.bank();
    const float W = (float)room->w(), D = (float)room->h(), H = (float)std::max(2.0, room->height);
    const float V = W * D * H;
    const float floorA = W * D, wallA = 2.0f * H * (W + D);
    float aFloor = floorAbsorption(bank[room->fmat].plane);
    float aCeil = ceilAbsorption(bank[room->cmat].plane);
    float aWall = wallAbsorption(bank[room->wmat].wall);
    // Einbauten (Regale, Kisten, Abtrennungen) streuen und schlucken etwas
    if (room->type->has(world::F_SHELVES) || room->type->has(world::F_CRATES) || room->type->has(world::F_CUBICLES))
        aWall += 0.05f;
    float S = 2.0f * floorA + wallA;
    float A = floorA * aFloor + floorA * aCeil + wallA * aWall + 0.003f * V;  // Luft daempft in grossen Raeumen
    float t60 = std::clamp(0.161f * V / std::max(A, 1.0f), 0.18f, 5.5f);
    float meanAbs = (floorA * aFloor + floorA * aCeil + wallA * aWall) / std::max(S, 1.0f);
    audio::Acoustics ac;
    ac.t60 = t60;
    ac.damping = std::clamp(0.2f + meanAbs * 1.4f, 0.2f, 0.75f);
    ac.wet = std::clamp(0.06f + 0.06f * t60, 0.07f, 0.35f);
    ac.preDelay = std::clamp(4.0f * V / std::max(S, 1.0f) / 343.0f, 0.004f, 0.09f);  // mittlere freie Weglaenge
    acoustics_ = ac;
    audio.setAcoustics(ac);
}

void Soundscape::updateDistant(double dt, audio::AudioSystem& audio, world::World& world, const world::Room* room,
                               vec3 listener, double sanity) {
    nextDistant_ -= dt;
    if (nextDistant_ > 0.0 || !room) return;
    // je weniger Verstand, desto oefter
    double s = std::clamp(sanity / 100.0, 0.0, 1.0);
    nextDistant_ = rng_.uniform(45.0, 120.0) * (0.45 + 0.55 * s);
    const std::string& zone = world.level().zones[(size_t)room->zone].key;
    static const char* office[] = {"dist_door", "dist_knock", "dist_creak", "dist_rumble"};
    static const char* industrial[] = {"dist_clank", "dist_rumble", "dist_door", "dist_steam"};
    static const char* tunnel[] = {"dist_rumble", "dist_clank", "dist_creak", "dist_knock"};
    static const char* halls[] = {"dist_door", "dist_rumble", "dist_clank", "dist_creak"};
    static const char* maint[] = {"dist_steam", "dist_clank", "dist_rumble", "dist_door"};
    const char* const* list = zone == "industrial" ? industrial
                              : zone == "tunnel"   ? tunnel
                              : zone == "halls"    ? halls
                              : zone == "maint"    ? maint
                                                   : office;
    const char* name = list[rng_.randint(0, 3)];
    double a = rng_.uniform(0.0, 2.0 * kPi), dist = rng_.uniform(22.0, 38.0);
    vec3 pos{listener.x + (float)(std::cos(a) * dist), listener.y + (float)(std::sin(a) * dist),
             listener.z + (float)rng_.uniform(-1.5, 2.0)};
    audio.playAt(name, pos, 0.85f, (float)dist * 1.7f, (float)rng_.uniform(0.9, 1.08), 0.75f);
}

}  // namespace lim::app
