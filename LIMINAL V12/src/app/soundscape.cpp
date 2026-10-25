#include "app/soundscape.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/math.hpp"
#include "render/flicker.hpp"
#include "world/level_def.hpp"
#include "world/materials.hpp"
#include "world/room.hpp"
#include "world/sector.hpp"
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
    {"lamp_hum_", 4, 0.13f, 9.0f, 0.55f},    // Leuchtstoffroehre (Buero)
    {"lamp_ind_", 3, 0.16f, 15.0f, 0.7f},    // Industrieleuchte, Leuchtband im Parkdeck
    {"lamp_emg_", 2, 0.08f, 6.0f, 0.4f},     // Notleuchte
    {"lamp_sconce_", 2, 0.06f, 6.0f, 0.5f},  // Wandleuchte (kleiner Leuchtkasten)
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
        case world::PlanePattern::Planks: return 0.1f;  // V11
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

void range2(const Json& a, float& lo, float& hi, float d0, float d1) {
    lo = (float)a[0].asNumber(d0);
    hi = (float)a[1].asNumber(a[0].asNumber(d1));
    if (hi < lo) std::swap(lo, hi);
}

std::vector<std::string> strings(const Json& a) {
    std::vector<std::string> out;
    for (const auto& s : a.items())
        if (s.isString()) out.push_back(s.asString());
    return out;
}

bool contains(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

}  // namespace

float Soundscape::occlusion(world::World& world, vec3 a, vec3 b) { return game::SoundField::rayOcclusion(world, a, b); }

audio::Spatial Soundscape::spatial(world::World& world, vec3 ear, vec3 src, float radius, float refDist) const {
    const game::SoundRoute r = field_.route(world, ear, src, radius);
    audio::Spatial sp;
    sp.pos = r.pos, sp.radius = radius, sp.occlusion = r.occlusion, sp.refDist = refDist, sp.pathExtra = r.pathExtra;
    return sp;
}

void Soundscape::reset(audio::AudioSystem& audio) {
    for (auto& [k, l] : lamps_)
        if (l.voice >= 0) audio.stopLoop(l.voice);
    lamps_.clear();
    for (Bed& b : beds_)
        if (b.voice >= 0) audio.stopLoop(b.voice);
    beds_.clear();
    for (auto& [k, s] : spotLoops_)
        if (s.voice >= 0) audio.stopLoop(s.voice);
    spotLoops_.clear();
    events_.clear();
    spotKinds_.clear();
    configured_ = nullptr;
    levelAudio_ = false;
    acousticRoom_ = nullptr;
    scanTimer_ = 0.0;
    bedDuck_ = bedDuckNow_ = 0.0f;
}

int Soundscape::activeBeds() const {
    int n = 0;
    for (const Bed& b : beds_) n += b.voice >= 0 ? 1 : 0;
    return n;
}

void Soundscape::configure(const world::LevelDef& L, audio::AudioSystem& audio) {
    reset(audio);
    configured_ = &L;
    const Json& a = L.audio;
    levelAudio_ = a.isObject() && (a.has("beds") || a.has("events"));
    if (!levelAudio_) return;
    for (const auto& b : a["beds"].items()) {
        Bed bed;
        bed.sound = b["sound"].asString("");
        bed.gain = (float)b["gain"].asNumber(1.0);
        bed.only = strings(b["only"]);
        bed.notIn = strings(b["not"]);
        bed.voice = audio.startLoop(bed.sound, 0.0f, audio::Bus::Ambience);
        if (bed.voice >= 0) audio.setLoopSend(bed.voice, 0.0f);  // Betten tragen ihren Raum schon in sich
        beds_.push_back(std::move(bed));
    }
    for (const auto& e : a["events"].items()) {
        Event ev;
        ev.sound = e["sound"].asString("");
        ev.where = e["where"].asString("");
        range2(e["every"], ev.every0, ev.every1, 30.0f, 60.0f);
        range2(e["dist"], ev.dist0, ev.dist1, 10.0f, 30.0f);
        ev.height = e.has("height");
        if (ev.height) range2(e["height"], ev.h0, ev.h1, 0.0f, 0.0f);
        ev.gain = (float)e["gain"].asNumber(0.7);
        // erste Wiederholung irgendwo im Intervall, damit nicht alles zugleich einsetzt
        ev.next = rng_.uniform(0.3, 1.0) * rng_.uniform(ev.every0, ev.every1);
        events_.push_back(std::move(ev));
    }
    if (const Json* sp = a.find("spots"); sp && sp->isObject())
        for (const auto& [k, v] : sp->members()) {
            SpotKind s;
            s.sound = v["sound"].asString("");
            s.radius = (float)v["radius"].asNumber(8.0);
            s.gain = (float)v["gain"].asNumber(0.5);
            s.voices = (int)v["voices"].asInt(2);
            spotKinds_[k] = s;
        }
}

void Soundscape::update(double dt, double time, audio::AudioSystem& audio, world::World& world, const world::Room* room,
                        vec3 listener, double sanity, bool active) {
    if (configured_ != &world.level()) configure(world.level(), audio), field_.invalidate();
    field_.update(world, listener, dt);  // V11: Wegefeld fuer Tueren und Ecken
    updateLamps(dt, time, audio, world, listener);
    updateAcoustics(dt, world, room, listener, audio);
    if (levelAudio_) {
        updateBeds(dt, audio, room);
        updateSpots(dt, audio, world, listener);
        if (active) updateEvents(dt, audio, world, room, listener);
    } else if (active && !quiet_) {
        updateDistant(dt, audio, world, room, listener, sanity);
    }
}

void Soundscape::updateLamps(double dt, double time, audio::AudioSystem& audio, world::World& world, vec3 listener) {
    scanTimer_ -= dt;
    const bool scan = scanTimer_ <= 0.0;
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
        // V7: Stromausfall. V11: als Welle - nur Leuchten hinter der Front verstummen
        const bool hit = boFront_ < 0.0f || std::hypot(lamp.pos.x - boOrigin_.x, lamp.pos.y - boOrigin_.y) < boFront_;
        const float dark = (int)lamp.flickerSeed == boSeed_ && hit ? boAmount_ : 0.0f;
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
        ++it;
    }
}

void Soundscape::updateAcoustics(double dt, world::World& world, const world::Room* room, vec3 ear, audio::AudioSystem& audio) {
    if (!room) return;
    acousticTimer_ -= dt;
    const vec3 de = ear - acousticEar_;
    // V11: die Fruehreflexionen haengen vom Standort ab - bei Bewegung etwa viermal je Sekunde nachfuehren
    const bool moved = levelAudio_ && acousticTimer_ <= 0.0 && dot(de, de) > 1.5f * 1.5f;
    if (room == acousticRoom_ && !moved) return;
    acousticRoom_ = room;
    acousticTimer_ = 0.25;
    acousticEar_ = ear;
    const float W = (float)room->w(), D = (float)room->h(), H = (float)std::max(2.0, room->height);
    const float V = W * D * H;
    const float floorA = W * D, wallA = 2.0f * H * (W + D);
    float S = 2.0f * floorA + wallA;
    audio::Acoustics ac;
    const Json* cfg = nullptr;
    if (levelAudio_) {
        const Json& all = configured_->audio["acoustics"];
        cfg = room->type ? all.find(room->type->key) : nullptr;
        if (!cfg) cfg = all.find("default");
    }
    if (cfg) {
        // V11: Nachhall je Raumtyp aus dem Level, Fruehreflexionen aus Raummassen und Hoererort
        ac.t60 = (float)(*cfg)["t60"].asNumber(0.8);
        ac.t60Low = (float)(*cfg)["t60_low"].asNumber(-1.0);
        ac.t60High = (float)(*cfg)["t60_high"].asNumber(-1.0);
        ac.size = (float)(*cfg)["size"].asNumber(1.0);
        ac.wet = (float)(*cfg)["wet"].asNumber(0.18);
        ac.early = (float)(*cfg)["early"].asNumber(0.6);
        ac.absorption = (float)(*cfg)["absorption"].asNumber(0.2);
        ac.gate = (float)(*cfg)["gate"].asNumber(0.0);
        ac.damping = std::clamp(0.2f + ac.absorption * 1.4f, 0.2f, 0.75f);
        ac.roomW = W, ac.roomD = D, ac.roomH = H;
        ac.lx = std::clamp(ear.x - (float)room->x0, 0.2f, W - 0.2f);
        ac.ly = std::clamp(ear.y - (float)room->y0, 0.2f, D - 0.2f);
        ac.lz = std::clamp(ear.z - (float)room->floor, 0.2f, H - 0.2f);
    } else {
        const auto& bank = world.bank();
        float aFloor = floorAbsorption(bank[room->fmat].plane);
        float aCeil = ceilAbsorption(bank[room->cmat].plane);
        float aWall = wallAbsorption(bank[room->wmat].wall);
        // Einbauten (Regale, Kisten, Abtrennungen) streuen und schlucken etwas
        if (room->type->has(world::F_SHELVES) || room->type->has(world::F_CRATES) || room->type->has(world::F_CUBICLES))
            aWall += 0.05f;
        float A = floorA * aFloor + floorA * aCeil + wallA * aWall + 0.003f * V;  // Luft daempft in grossen Raeumen
        float t60 = std::clamp(0.161f * V / std::max(A, 1.0f), 0.18f, 5.5f);
        float meanAbs = (floorA * aFloor + floorA * aCeil + wallA * aWall) / std::max(S, 1.0f);
        ac.t60 = t60;
        ac.damping = std::clamp(0.2f + meanAbs * 1.4f, 0.2f, 0.75f);
        ac.wet = std::clamp(0.06f + 0.06f * t60, 0.07f, 0.35f);
    }
    ac.preDelay = std::clamp(4.0f * V / std::max(S, 1.0f) / 343.0f, 0.004f, 0.09f);  // mittlere freie Weglaenge
    acoustics_ = ac;
    audio.setAcoustics(ac);
}

void Soundscape::updateBeds(double dt, audio::AudioSystem& audio, const world::Room* room) {
    bedDuckNow_ += (bedDuck_ - bedDuckNow_) * (float)std::min(1.0, dt * 0.8);
    const std::string type = room && room->type ? room->type->key : std::string();
    for (Bed& b : beds_) {
        if (b.voice < 0) continue;
        bool here = room && (b.only.empty() || contains(b.only, type)) && !contains(b.notIn, type);
        const float target = b.gain * (here ? 1.0f : 0.3f) * (1.0f - 0.85f * bedDuckNow_);
        b.level += (target - b.level) * (float)std::min(1.0, dt * 0.5);
        audio.setLoopVolume(b.voice, b.level);
    }
}

void Soundscape::updateSpots(double dt, audio::AudioSystem& audio, world::World& world, vec3 listener) {
    spotScan_ -= dt;
    if (spotScan_ <= 0.0 && !spotKinds_.empty()) {
        spotScan_ = 0.3;
        for (auto& [k, s] : spotLoops_) s.wanted = false;
        if (const world::Sector* sec = world.sectorIfReady(listener.x, listener.y)) {
            struct Cand {
                float d2;
                const world::Sector::Spot* s;
            };
            std::vector<Cand> cands;
            for (const auto& sp : sec->spots) {
                auto it = spotKinds_.find(sp.kind);
                if (it == spotKinds_.end()) continue;
                const float r = it->second.radius * 1.15f;
                const vec3 d = vec3{sp.x, sp.y, sp.z} - listener;
                const float d2 = dot(d, d);
                if (d2 < r * r) cands.push_back({d2, &sp});
            }
            std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
            // je Art die naechsten Stellen; dichte Reihen (ein Rohrstueck je Meter) zaehlen als eine Quelle
            std::unordered_map<std::string, std::vector<vec3>> taken;
            for (const Cand& c : cands) {
                const SpotKind& k = spotKinds_[c.s->kind];
                auto& list = taken[c.s->kind];
                if ((int)list.size() >= k.voices) continue;
                const vec3 p{c.s->x, c.s->y, c.s->z};
                bool near = false;
                for (const vec3& q : list) near = near || dot(p - q, p - q) < 3.0f * 3.0f;
                if (near) continue;
                list.push_back(p);
                const u64 key = hash64((i64)std::lround(p.x * 4.0f), (i64)std::lround(p.y * 4.0f), (i64)std::lround(p.z * 4.0f));
                SpotLoop& s = spotLoops_[key];
                s.wanted = true;
                s.kind = c.s->kind;
                s.pos = p + vec3{0.0f, 0.0f, 0.4f};
                s.occ = occlusion(world, listener, s.pos);
                s.sp = spatial(world, listener, s.pos, k.radius);
            }
        }
    }
    for (auto it = spotLoops_.begin(); it != spotLoops_.end();) {
        SpotLoop& s = it->second;
        const SpotKind& k = spotKinds_[s.kind];
        s.level += ((s.wanted ? 1.0f : 0.0f) - s.level) * (float)std::min(1.0, dt * 1.2);
        if (!s.wanted && s.level < 0.01f) {
            if (s.voice >= 0) audio.stopLoop(s.voice);
            it = spotLoops_.erase(it);
            continue;
        }
        if (s.voice < 0 && s.wanted) {
            s.voice = audio.startLoop(k.sound, 0.0f, audio::Bus::Ambience);
            if (s.voice >= 0) audio.setLoopSend(s.voice, 0.5f);
        }
        if (s.voice >= 0) {
            audio.setLoopVolume(s.voice, k.gain * s.level);
            audio.setLoopSpatial(s.voice, s.sp.radius > 0.0f ? s.sp : audio::Spatial{s.pos, k.radius, s.occ});
        }
        ++it;
    }
}

void Soundscape::updateEvents(double dt, audio::AudioSystem& audio, world::World& world, const world::Room* room, vec3 listener) {
    if (quiet_) return;  // V11: Stille - die Abstaende laufen erst danach weiter
    for (Event& ev : events_) {
        ev.next -= dt;
        if (ev.next > 0.0) continue;
        ev.next = rng_.uniform(ev.every0, ev.every1);
        vec3 pos;
        if (!ev.where.empty()) {
            // an einer Stelle der Ausstattung (Tropfen in die Pfuetze, Ticken am Wagen)
            const world::Sector* sec = world.sectorIfReady(listener.x, listener.y);
            if (!sec) continue;
            const float r = ev.dist1 > 0.0f ? ev.dist1 : 25.0f;
            std::vector<const world::Sector::Spot*> c;
            for (const auto& sp : sec->spots) {
                if (sp.kind != ev.where) continue;
                const float d = length(vec3{sp.x, sp.y, sp.z} - listener);
                if (d >= ev.dist0 && d < r) c.push_back(&sp);
            }
            if (c.empty()) {
                ev.next = rng_.uniform(4.0, 9.0);  // gerade keine solche Stelle in der Naehe
                continue;
            }
            const world::Sector::Spot& sp = *c[(size_t)rng_.randint(0, (int)c.size() - 1)];
            pos = {sp.x + (float)rng_.uniform(-0.3, 0.3), sp.y + (float)rng_.uniform(-0.3, 0.3), sp.z + (ev.where == "car" ? 0.7f : 0.02f)};
        } else {
            const double a = rng_.uniform(0.0, 2.0 * kPi), dist = rng_.uniform(ev.dist0, ev.dist1);
            const float base = room ? (float)room->floor : listener.z - 1.6f;
            const float z = ev.height ? base + (float)rng_.uniform(ev.h0, ev.h1) : listener.z + (float)rng_.uniform(-1.5, 2.0);
            pos = {listener.x + (float)(std::cos(a) * dist), listener.y + (float)(std::sin(a) * dist), z};
        }
        const float d = length(pos - listener);
        audio::PlayOpts o;
        o.volume = ev.gain;
        o.bus = audio::Bus::Ambience;
        o.send = 0.7f;
        audio.play3d(ev.sound, spatial(world, listener, pos, std::max(14.0f, d * 1.8f)), o);
    }
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
