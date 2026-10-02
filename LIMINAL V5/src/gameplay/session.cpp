#include "gameplay/session.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "core/rng.hpp"
#include "world/room.hpp"

namespace lim::game {

class GameSession::TileAdapter : public phys::TileSource {
public:
    explicit TileAdapter(world::World& w) : w_(w) {}
    const world::Tile& at(i64 x, i64 y) override { return w_.tile(x, y); }

private:
    world::World& w_;
};

GameSession::GameSession(const Content& content, const PrefabLibrary& prefabs,
                         std::shared_ptr<const world::LevelDef> level, i64 seed_, const std::string& diff,
                         const std::string& nm, JobSystem& jobs, int loadRadius)
    : name(nm),
      difficulty(diff),
      levelId(level->id),
      seed(seed_),
      content_(content),
      prefabs_(prefabs),
      stats_(content.difficulty(diff)),
      inventory_(&content) {
    const DifficultyDef& d = content.difficulty(diff);
    world_ = std::make_unique<world::World>(level, seed, d.itemsNone, d.itemsOne, jobs, loadRadius);
    tiles_ = std::make_unique<TileAdapter>(*world_);
    world_->onChunkLoaded = [this](const world::ChunkData& cd) { onChunk(cd, true); };
    world_->onChunkUnloaded = [this](const world::ChunkData& cd) { onChunk(cd, false); };
}

GameSession::~GameSession() {
    // Welt zuerst: sie wartet auf ihre Jobs und ruft dabei keine Listener mehr auf
    world_->onChunkLoaded = nullptr;
    world_->onChunkUnloaded = nullptr;
    world_.reset();
}

void GameSession::placeAtSpawn() {
    auto [x, y, z, a] = world_->findSpawn();
    player_ = Player(x, y, z, a);
}

void GameSession::preload() {
    world_->preload(player_.x, player_.y);
    double x = player_.x, y = player_.y, z = player_.z;
    if (phys::findFreeSpot(*tiles_, x, y, z, player_.shape)) {
        player_.x = x, player_.y = y, player_.z = z;
    }
}

// --- Entities der Chunks -------------------------------------------------------------------
void GameSession::onChunk(const world::ChunkData& cd, bool loaded) {
    if (loaded) {
        // Items auf den Theken
        for (const world::ItemSpawn* it : cd.items) {
            if (picked.count(it->id)) continue;
            const ItemDef* def = content_.item(it->kind);
            if (!def) continue;
            // Ausrichtung: Riegel laengs der Theke, leicht verdreht (stabil je Item)
            float base = std::atan2((float)it->dirY, (float)it->dirX) + kPi * 0.5f;
            float jitter = (hash01((u32)std::hash<std::string>()(it->id)) - 0.5f) * 0.7f;
            ecs::Entity e = prefabs_.spawn(reg_, "item_pickup", {(float)it->x, (float)it->y, (float)it->z}, base + jitter);
            if (e == ecs::kNull) continue;
            if (auto* p = reg_.get<Pickup>(e)) {
                p->item = it->kind;
                p->worldId = it->id;
            }
            if (auto* m = reg_.get<MeshRenderer>(e)) m->model = def->model;
            if (auto* t = reg_.get<Transform>(e)) t->scale = (float)def->scale;
            reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
        }
        // Automaten
        for (int ly = 0; ly < world::CS; ++ly)
            for (int lx = 0; lx < world::CS; ++lx) {
                const world::Tile& t = cd.tile(lx, ly);
                if (!(t.flags & world::TF_MACHINE) || !t.room) continue;
                vec3 pos{(float)(cd.x0() + lx) + 0.5f, (float)(cd.y0() + ly) + 0.5f, (float)t.room->floor + 1.0f};
                ecs::Entity e = prefabs_.spawn(reg_, "vending_machine", pos, 0.0f);
                if (e != ecs::kNull) reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
            }
        spawnLevelEntities(cd);
    } else {
        std::vector<ecs::Entity> gone;
        reg_.each<ChunkOwned>([&](ecs::Entity e, ChunkOwned& o) {
            if (o.chunk == cd.key) gone.push_back(e);
        });
        for (auto e : gone) reg_.destroy(e);
    }
    if (chunkListener) chunkListener(cd, loaded);
}

// Entities aus "entities" der Level-Datei: je Raum hoechstens einmal, im Chunk mit der Raummitte
void GameSession::spawnLevelEntities(const world::ChunkData& cd) {
    const auto& defs = world_->level().entities;
    if (defs.empty()) return;
    std::unordered_set<const world::Room*> seen;
    for (int ly = 0; ly < world::CS; ++ly)
        for (int lx = 0; lx < world::CS; ++lx) {
            const world::Room* r = cd.tile(lx, ly).room;
            if (!r || !seen.insert(r).second) continue;
            i64 cx = r->x0 + floorDiv(r->x1 - r->x0, 2), cy = r->y0 + floorDiv(r->y1 - r->y0, 2);
            if (cx < cd.x0() || cx >= cd.x0() + world::CS || cy < cd.y0() || cy >= cd.y0() + world::CS) continue;
            const world::Tile& t = cd.tile((int)(cx - cd.x0()), (int)(cy - cd.y0()));
            if (t.room != r || (t.flags & (world::TF_SOLID | world::TF_MACHINE))) continue;
            for (size_t k = 0; k < defs.size(); ++k) {
                const world::EntitySpawnDef& d = defs[k];
                if (!d.roomTypes.empty() &&
                    std::find(d.roomTypes.begin(), d.roomTypes.end(), r->type->index) == d.roomTypes.end())
                    continue;
                if (hfloat(r->seed, 0x5EA7, (i64)k) >= d.chance) continue;
                vec3 pos{(float)cx + 0.5f, (float)cy + 0.5f, t.floor + (float)d.height};
                float yaw = (float)hfloat(r->seed, 0x5EA8, (i64)k) * 2.0f * kPi;
                ecs::Entity e = prefabs_.spawn(reg_, d.prefab, pos, yaw);
                if (e == ecs::kNull) continue;
                reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
                if (auto* w = reg_.get<Wanderer>(e)) w->seed = (u32)hash64(r->seed, 0x5EA9, (i64)k) | 1u;
            }
        }
}

// Wanderer: gehen zu zufaelligen Zielen in der Naehe, bleiben auf begehbaren Kacheln
// (keine Waende, keine Stufen ueber 0,35 m) und im bereits geladenen Bereich.
void GameSession::updateWanderers(double dt) {
    float fdt = (float)dt;
    reg_.each<Wanderer, Transform>([&](ecs::Entity, Wanderer& w, Transform& t) {
        vec2 p{t.pos.x, t.pos.y};
        vec2 d = w.target - p;
        float len = std::hypot(d.x, d.y);
        w.timer -= fdt;
        if (w.timer <= 0.0f || len < 0.1f) {
            w.seed = hash32(w.seed + 0x9E3779B9u);
            float a = hash01(w.seed) * 2.0f * kPi;
            float r = 1.5f + hash01(w.seed ^ 0x68E31DA4u) * 4.5f;
            w.target = p + vec2{std::cos(a), std::sin(a)} * r;
            w.timer = r / std::max(0.1f, w.speed) + 1.0f + hash01(w.seed ^ 0xB5297A4Du) * 2.0f;
            return;
        }
        vec2 np = p + d * (std::min(len, w.speed * fdt) / len);
        const world::Tile* old = world_->tileIfLoaded(ifloor(p.x), ifloor(p.y));
        const world::Tile* nt = world_->tileIfLoaded(ifloor(np.x), ifloor(np.y));
        if (!old || !nt || (nt->flags & (world::TF_SOLID | world::TF_MACHINE)) ||
            std::fabs(nt->floor - old->floor) > 0.35f || nt->ceil - nt->floor < 1.9f) {
            w.timer = 0.0f;  // blockiert oder nicht geladen: neues Ziel
            return;
        }
        t.pos = {np.x, np.y, t.pos.z + (nt->floor - old->floor)};
        float want = std::atan2(d.y, d.x);
        float diff = std::remainder(want - t.yaw, 2.0f * kPi);
        t.yaw += diff * std::min(1.0f, fdt * 6.0f);
    });
}

// --- Aktualisierung ------------------------------------------------------------------------
void GameSession::update(double dt, const PlayerControl& control, bool paused) {
    if (!paused) {
        stats_.update(dt);
        autosaveTimer += dt;
        if (autosaveTimer >= kAutosave) {
            autosaveTimer = 0.0;
            events_.push_back({GameEvent::AutosaveDue});
        }
        if (stats_.dead && !deathReported_) {
            deathReported_ = true;
            events_.push_back({GameEvent::Died});
        }
    }
    player_.speedMul = stats_.speedMul();
    PlayerControl c = paused ? PlayerControl{} : control;
    player_.update(dt, c, *tiles_);
    world_->update(player_.x, player_.y);

    room_ = world_->roomAt(player_.x, player_.y);
    updateFog(dt);
    // Zonenwechsel dezent einblenden (V4: hud.track_zone)
    if (room_) {
        int z = room_->zone;
        if (zone_ < 0) zone_ = z;
        else if (z != zone_ && room_->zoneW[(size_t)z] > 0.6) {
            zone_ = z;
            events_.push_back({GameEvent::ZoneTitle, world_->level().zones[(size_t)z].name, 3.5f});
        }
    }
    if (!paused) {
        if (player_.stepped) events_.push_back({GameEvent::Footstep, "", 0, surfaceUnder(), "", (float)std::min(1.0, player_.speed() / 3.0)});
        if (player_.landed) events_.push_back({GameEvent::Land, "", 0, surfaceUnder(), "", (float)std::min(1.0, player_.fallSpeed / 8.0)});
        supplyHint(dt);
        updateWanderers(dt);
    }
    target_ = ecs::kNull;
    if (!paused) findTarget();
    reg_.each<MeshRenderer>([&](ecs::Entity e, MeshRenderer& m) { m.highlight = 0.0f; });
    if (target_ != ecs::kNull)
        if (auto* m = reg_.get<MeshRenderer>(target_)) m->highlight = 0.35f + 0.15f * (float)std::sin(stats_.playTime * 5.0);
}

void GameSession::updateFog(double dt) {
    if (!room_) return;
    vec3 tgt{(float)room_->fog[0], (float)room_->fog[1], (float)room_->fog[2]};
    if (!fogInit_) {
        fog_ = tgt;
        fogDensity_ = room_->fogDensity;
        fogInit_ = true;
        return;
    }
    float k = (float)std::min(1.0, dt * 1.2);
    fog_ = lerp(fog_, tgt, k);
    fogDensity_ += (room_->fogDensity - fogDensity_) * k;
}

Surface GameSession::surfaceUnder() const {
    const world::Tile* t = world_->tileIfLoaded(ifloor(player_.x), ifloor(player_.y));
    if (!t || t->solid()) return Surface::Concrete;
    const world::Material& m = world_->bank()[t->fmat];
    switch (m.surface) {
        case world::Surface::Crate:
        case world::Surface::Counter:
        case world::Surface::Trim: return Surface::Wood;
        case world::Surface::Metal:
        case world::Surface::Shelf:
        case world::Surface::Machine: return Surface::Metal;
        default: break;
    }
    switch (m.plane) {
        case world::PlanePattern::Carpet: return Surface::Carpet;
        case world::PlanePattern::Grate: return Surface::Metal;
        case world::PlanePattern::Tiles:
        case world::PlanePattern::BigTiles:
        case world::PlanePattern::Checker: return Surface::Tile;
        default: return Surface::Concrete;
    }
}

// Item, das mit E aufgenommen werden kann: nah, vor dem Spieler, frei sichtbar (V4 find_target)
void GameSession::findTarget() {
    const Player& p = player_;
    double best = 1e9;
    double ca = std::cos(p.angle), sa = std::sin(p.angle);
    reg_.each<Interactable, Transform>([&](ecs::Entity e, Interactable& in, Transform& t) {
        double dx = t.pos.x - p.x, dy = t.pos.y - p.y;
        double d = std::hypot(dx, dy);
        if (d > in.range || d < 1e-6) return;
        if ((dx * ca + dy * sa) / d < std::cos(in.coneDeg * kPi / 180.0)) return;
        if (t.pos.z - p.z > 1.9 || p.z - t.pos.z > 1.2) return;
        if (!phys::clearLine(*tiles_, p.x, p.y, p.z, t.pos.x, t.pos.y, 0.55, 1.2)) return;
        if (d < best) {
            best = d;
            target_ = e;
        }
    });
}

std::string GameSession::targetPrompt() const {
    if (target_ == ecs::kNull) return "";
    auto& reg = const_cast<ecs::Registry&>(reg_);
    auto* pk = reg.get<Pickup>(target_);
    auto* in = reg.get<Interactable>(target_);
    if (pk) {
        const ItemDef* d = content_.item(pk->item);
        return std::format("[E]  {} {}", d ? d->name : pk->item, in ? in->prompt : "aufnehmen");
    }
    return in ? "[E]  " + in->prompt : "";
}

void GameSession::interact() {
    if (target_ == ecs::kNull) return;
    auto* pk = reg_.get<Pickup>(target_);
    if (!pk) return;
    const ItemDef* d = content_.item(pk->item);
    if (inventory_.add(pk->item)) {
        picked.insert(pk->worldId);
        events_.push_back({GameEvent::Pickup, std::format("{} aufgenommen", d ? d->name : pk->item), 2.2f});
        reg_.destroy(target_);
        target_ = ecs::kNull;
    } else {
        events_.push_back({GameEvent::InventoryFull, "Inventar voll", 2.2f});
    }
}

void GameSession::useSlot(int idx) {
    auto kind = inventory_.take(idx);
    if (!kind) {
        events_.push_back({GameEvent::Toast, "Kein Item auf diesem Platz", 1.5f});
        return;
    }
    const ItemDef* d = content_.item(*kind);
    if (!d) return;
    GameEvent ev{GameEvent::Consume, stats_.consume(*d), 3.0f};
    ev.sound = d->useSound;
    events_.push_back(ev);
}

void GameSession::selectSlot(int idx) {
    inventory_.selected = std::clamp(idx, 0, Inventory::kHotbar - 1);
    if (const auto& s = inventory_.slots[(size_t)inventory_.selected])
        if (const ItemDef* d = content_.item(s->kind)) events_.push_back({GameEvent::Toast, d->name, 1.2f});
}

// Dezenter Hinweis, wenn ein Versorgungsraum mit Items in der Naehe ist (V4 _machine_hint)
void GameSession::supplyHint(double dt) {
    hintTimer_ -= dt;
    if (hintTimer_ > 0.0) return;
    hintTimer_ = 0.5;
    bool done = false;
    world_->forEachSupplyRoom([&](const world::Room& r) {
        if (done) return;
        std::string rid = std::format("{},{},{}", r.sx, r.sy, r.index);
        if (hinted_.count(rid)) return;
        bool any = false;
        for (const auto& it : r.items)
            if (!picked.count(it.id)) any = true;
        if (!any) return;
        auto [cx, cy] = r.center();
        if ((cx - player_.x) * (cx - player_.x) + (cy - player_.y) * (cy - player_.y) < 22.0 * 22.0) {
            hinted_.insert(rid);
            events_.push_back({GameEvent::Toast, "Ganz in der Nähe summt ein Automat …", 3.5f});
            done = true;
        }
    });
}

// --- Darstellung ----------------------------------------------------------------------------
void GameSession::collectMeshes(std::vector<VisibleMesh>& out) {
    vec3 eye = player_.eyePos();
    reg_.each<MeshRenderer, Transform>([&](ecs::Entity e, MeshRenderer& m, Transform& t) {
        vec3 d = t.pos - eye;
        if (dot(d, d) > m.drawDistance * m.drawDistance) return;
        VisibleMesh v;
        v.model = &m.model;
        v.meshId = &m.meshId;
        v.world = mat4::translation(t.pos) * mat4::rotationZ(t.yaw) * mat4::scale(vec3(t.scale));
        v.highlight = m.highlight;
        out.push_back(v);
    });
}

void GameSession::collectSounds(std::vector<SoundSource>& out) {
    reg_.each<AudioEmitter, Transform>([&](ecs::Entity e, AudioEmitter& a, Transform& t) {
        out.push_back({e, &a.sound, t.pos, a.volume, a.radius, &a.voice});
    });
}

// --- Spielstand -----------------------------------------------------------------------------
Json GameSession::toJson() const {
    Json j = Json::object();
    Json p = Json::object();
    p.set("x", player_.x);
    p.set("y", player_.y);
    p.set("z", player_.z);
    p.set("angle", player_.angle);
    p.set("pitch", player_.pitch);
    p.set("distance", player_.distance);
    j.set("player", std::move(p));
    j.set("stats", stats_.toJson());
    j.set("inventory", inventory_.toJson());
    j.set("selected", inventory_.selected);
    std::vector<std::string> ids(picked.begin(), picked.end());
    std::sort(ids.begin(), ids.end());
    Json pk = Json::array();
    for (auto& s : ids) pk.push(s);
    j.set("picked", std::move(pk));
    return j;
}

void GameSession::fromJson(const Json& j) {
    const Json& p = j["player"];
    player_.x = p["x"].asNumber(player_.x);
    player_.y = p["y"].asNumber(player_.y);
    player_.z = p["z"].asNumber(player_.z);
    player_.angle = p["angle"].asNumber(0.0);
    // V4 speicherte den Blickwinkel begrenzt auf +-0.9 rad - passt in den V5-Bereich
    player_.pitch = std::clamp(p["pitch"].asNumber(0.0), -Player::kPitchMax, Player::kPitchMax);
    player_.distance = p["distance"].asNumber(0.0);
    stats_.fromJson(j["stats"]);
    inventory_.fromJson(j["inventory"]);
    inventory_.selected = (int)(j["selected"].asInt(0) % Inventory::kHotbar);
    picked.clear();
    for (const auto& id : j["picked"].items()) picked.insert(id.asString());
    deathReported_ = false;
}

void GameSession::reviveAfterDeath() {
    // Nach dem Tod: mit voller Health und etwas Sanity weiterspielen (wie V4)
    stats_.health = 100.0;
    stats_.sanity = std::max(30.0, stats_.sanity);
    stats_.zeroTime = 0.0;
    stats_.dead = false;
    deathReported_ = false;
}

}  // namespace lim::game
