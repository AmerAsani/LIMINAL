#include "gameplay/session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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
    world_ = std::make_unique<world::World>(level, seed, d.itemsNone, d.itemsOne, jobs, loadRadius, content.supplyRules());
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
    body_.reset();
}

void GameSession::preload() {
    world_->preload(player_.x, player_.y);
    double x = player_.x, y = player_.y, z = player_.z;
    if (phys::findFreeSpot(*tiles_, x, y, z, player_.shape)) {
        player_.x = x, player_.y = y, player_.z = z;
    }
    body_.reset();
    body_.update(0.0, player_);
    explore_.reveal(*world_, player_.x, player_.y, player_.z, 0.0, true);
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
            if (auto* m = reg_.get<MeshRenderer>(e)) {
                m->model = def->model;
                m->glint = def->rare;  // V6: seltene Funde schimmern leicht auf der Theke
            }
            if (auto* t = reg_.get<Transform>(e)) t->scale = (float)def->scale;
            reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
        }
        explore_.refreshChunk(cd, world_->bank());
        // Automaten
        for (int ly = 0; ly < world::CS; ++ly)
            for (int lx = 0; lx < world::CS; ++lx) {
                const world::Tile& t = cd.tile(lx, ly);
                if (!(t.flags & world::TF_MACHINE) || !t.room) continue;
                // V6: Modell steht auf dem Boden, Front zeigt in den Raum (Brummen kommt vom Kompressor unten)
                vec3 pos{(float)(cd.x0() + lx) + 0.5f, (float)(cd.y0() + ly) + 0.5f, (float)t.room->floor};
                float yaw = std::atan2((float)t.room->machineDirY, (float)t.room->machineDirX);
                ecs::Entity e = prefabs_.spawn(reg_, "vending_machine", pos, yaw);
                if (e != ecs::kNull) reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
            }
        spawnLevelEntities(cd);
        // V6: abgelegte Items dieses Chunks wieder hinlegen
        for (auto& d : dropped_)
            if (!reg_.alive(d.entity) && world::chunkOf(ifloor(d.pos.x), ifloor(d.pos.y)) == cd.key) spawnDropped(d, {}, true);
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

// --- Abgelegte Items (V6) --------------------------------------------------------------------
ecs::Entity GameSession::spawnDropped(DroppedItem& d, vec3 vel, bool resting) {
    const ItemDef* def = content_.item(d.kind);
    if (!def) return ecs::kNull;
    ecs::Entity e = prefabs_.spawn(reg_, "item_pickup", d.pos, d.yaw);
    if (e == ecs::kNull) return e;
    if (auto* p = reg_.get<Pickup>(e)) {
        p->item = d.kind;
        p->worldId = d.id;
    }
    if (auto* m = reg_.get<MeshRenderer>(e)) {
        m->model = def->model;
        m->glint = def->rare;
    }
    if (auto* t = reg_.get<Transform>(e)) t->scale = (float)def->scale;
    Dropped dr;
    dr.vel = vel;
    dr.resting = resting;
    dr.id = d.id;
    dr.spin = resting ? 0.0f : (hash01((u32)nextDrop_ * 2654435761u) - 0.5f) * 9.0f;
    reg_.add<Dropped>(e, dr);
    reg_.add<ChunkOwned>(e, ChunkOwned{world::chunkOf(ifloor(d.pos.x), ifloor(d.pos.y))});
    d.entity = e;
    return e;
}

void GameSession::throwItem(const std::string& kind, int count) {
    const Player& p = player_;
    // aus der Hand vor dem Koerper, leicht nach oben und in Blickrichtung (mit Schwung des Laufens)
    vec3 fwd = forwardFromAngles((float)p.angle, (float)std::clamp(p.pitch, -0.7, 0.45));
    vec3 flat = normalize(vec3{fwd.x, fwd.y, 0.0f});
    vec3 eye = body_.eye();
    vec3 start = eye + flat * 0.3f - vec3{0, 0, 0.32f};
    auto freeAt = [&](vec3 q) {
        const world::Tile* t = world_->tileIfLoaded(ifloor(q.x), ifloor(q.y));
        return t && t->open() && q.z > t->floor + 0.05f && q.z < t->ceil - 0.08f;
    };
    if (!freeAt(start)) start = {(float)p.x, (float)p.y, (float)p.z + 1.0f};
    for (int i = 0; i < count; ++i) {
        if (dropped_.size() >= kMaxDropped) {  // aelteste zuerst aufgeben (Speicher bleibt begrenzt)
            if (reg_.alive(dropped_.front().entity)) reg_.destroy(dropped_.front().entity);
            dropped_.erase(dropped_.begin());
        }
        u32 h = hash32((u32)nextDrop_ * 747796405u + 12345u);
        vec3 spread{(hash01(h) - 0.5f) * 0.5f, (hash01(h ^ 0x9E37u) - 0.5f) * 0.5f, hash01(h ^ 0x51EDu) * 0.3f};
        vec3 vel = fwd * 1.8f + vec3{0, 0, 1.1f} + vec3{(float)p.vx, (float)p.vy, 0.0f} * 0.5f + spread * (count > 1 ? 1.0f : 0.3f);
        DroppedItem d;
        d.id = std::format("drop:{}", nextDrop_++);
        d.kind = kind;
        d.pos = start + vec3{(hash01(h ^ 0xA5u) - 0.5f) * 0.06f, (hash01(h ^ 0x5Au) - 0.5f) * 0.06f, 0.0f};
        d.yaw = (float)p.angle + (hash01(h ^ 0x77u) - 0.5f) * 1.2f;
        dropped_.push_back(d);
        spawnDropped(dropped_.back(), vel, false);
    }
    const ItemDef* def = content_.item(kind);
    GameEvent ev{GameEvent::Drop,
                 count > 1 ? std::format("{} × {} abgelegt", count, def ? def->name : kind)
                           : std::format("{} abgelegt", def ? def->name : kind),
                 1.6f};
    events_.push_back(ev);
    body_.gesture(Body::Gesture::Toss, start, "");
}

bool GameSession::dropSlot(int idx, bool all) {
    if (idx < 0 || idx >= Inventory::kSize || !inventory_.slots[(size_t)idx]) {
        events_.push_back({GameEvent::Toast, "Kein Item auf diesem Platz", 1.5f});
        return false;
    }
    std::string kind = inventory_.slots[(size_t)idx]->kind;
    int n = all ? inventory_.slots[(size_t)idx]->count : 1;
    for (int i = 0; i < n; ++i) inventory_.take(idx);
    throwItem(kind, n);
    return true;
}

void GameSession::dropStack(const std::string& kind, int count) {
    if (count > 0 && content_.item(kind)) throwItem(kind, count);
}

void GameSession::updateDropped(double dt) {
    reg_.each<Dropped, Transform>([&](ecs::Entity e, Dropped& d, Transform& t) {
        if (d.resting) return;
        const float g = 14.0f;
        float rest = (float)dt;
        while (rest > 1e-5f && !d.resting) {
            float h = std::min(rest, 1.0f / 120.0f);
            rest -= h;
            d.vel.z -= g * h;
            vec3 np = t.pos + d.vel * h;
            const world::Tile* cur = world_->tileIfLoaded(ifloor(t.pos.x), ifloor(t.pos.y));
            const world::Tile* nt = world_->tileIfLoaded(ifloor(np.x), ifloor(np.y));
            if (!nt || nt->solid() || nt->floor > np.z + 0.02f || nt->ceil < np.z + 0.05f) {
                // Wand, Kante oder Einbau: waagrecht abprallen
                d.vel.x *= -0.3f, d.vel.y *= -0.3f;
                np.x = t.pos.x, np.y = t.pos.y;
                nt = cur;
                if (!nt || nt->solid()) {
                    d.resting = true;
                    break;
                }
            }
            if (np.z > nt->ceil - 0.05f) {
                np.z = nt->ceil - 0.05f;
                d.vel.z = std::min(d.vel.z, 0.0f);
            }
            if (np.z <= nt->floor) {
                np.z = nt->floor;
                float impact = -d.vel.z;
                if (impact > 1.6f) {  // kurz aufspringen
                    d.vel.z = impact * 0.3f;
                    d.vel.x *= 0.55f, d.vel.y *= 0.55f;
                    d.spin *= 0.5f;
                } else {
                    d.vel = {};
                    d.resting = true;
                }
                if (impact > 0.6f) {
                    GameEvent ev{GameEvent::ItemLand};
                    ev.pos = np;
                    ev.intensity = std::min(1.0f, impact / 6.0f);
                    events_.push_back(ev);
                }
            }
            t.pos = np;
            t.yaw += d.spin * h;
        }
        for (auto& r : dropped_)
            if (r.id == d.id) {
                r.pos = t.pos;
                r.yaw = t.yaw;
                break;
            }
        if (d.resting)
            if (auto* o = reg_.get<ChunkOwned>(e)) o->chunk = world::chunkOf(ifloor(t.pos.x), ifloor(t.pos.y));
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
    // V6: erschoepft etwas langsamer, Sprinten nur mit Ausdauer
    player_.speedMul = stats_.speedMul() * (stats_.exhausted ? 0.9 : 1.0);
    PlayerControl c = paused ? PlayerControl{} : control;
    if (!stats_.canSprint()) c.sprint = false;
    player_.update(dt, c, *tiles_);
    if (!paused) {
        bool wasExhausted = stats_.exhausted;
        stats_.updateStamina(dt, player_.sprinting, player_.groundSpeed > 0.3);
        if (player_.jumped) stats_.spendStamina(4.0);
        if (stats_.exhausted && !wasExhausted) events_.push_back({GameEvent::Exhausted, "Außer Atem – kurz verschnaufen …", 2.5f});
        updateDropped(dt);
    }
    player_.breathless = stats_.breathless();
    world_->update(player_.x, player_.y);
    body_.update(paused ? 0.0 : dt, player_);
    if (!paused) explore_.reveal(*world_, player_.x, player_.y, player_.z, dt);

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
        if (player_.jumped) events_.push_back({GameEvent::Jump, "", 0, surfaceUnder(), "", 1.0f});
        supplyHint(dt);
        updateWanderers(dt);
    }
    target_ = ecs::kNull;
    if (!paused) findTarget();
    // seltene Items: sanftes, langsames Schimmern (verraet den Fund, ohne zu blenden)
    const float glint = 0.06f + 0.06f * (float)std::sin(stats_.playTime * 2.3);
    reg_.each<MeshRenderer>([&](ecs::Entity e, MeshRenderer& m) { m.highlight = m.glint ? glint : 0.0f; });
    if (target_ != ecs::kNull)
        if (auto* m = reg_.get<MeshRenderer>(target_)) m->highlight = 0.35f + 0.15f * (float)std::sin(stats_.playTime * 5.0);
}

// --- Kamera (V6) ---------------------------------------------------------------------------
ViewCamera GameSession::camera(bool thirdPerson, double dt) {
    ViewCamera c;
    c.yaw = (float)(player_.angle + player_.sway());
    c.pitch = (float)player_.pitch;
    c.roll = body_.roll();
    c.fovMul = 1.0f + body_.fovKick();
    if (!thirdPerson) {
        c.pos = body_.eye();
        tpDist_ = 0.3;  // beim Umschalten faehrt die Schulterkamera weich heraus
        tpInit_ = false;
        return c;
    }
    // Schulterkamera: hinter dem Kopf, leicht rechts versetzt. Stoesst sie an Waende, Boden
    // oder Decke, rueckt sie sofort heran und gleitet danach langsam wieder zurueck.
    const BodyPose& b = body_.pose();
    // Drehpunkt auf Kopfhoehe, folgt Spruengen leicht verzoegert (die Kamera "atmet" mit,
    // statt hart nach oben zu schnellen und in niedrigen Gaengen an die Decke zu stossen)
    float pz = b.neck.z + 0.16f;
    if (!tpInit_ || std::fabs(pz - tpPivotZ_) > 1.5f) tpPivotZ_ = pz, tpInit_ = true;
    tpPivotZ_ += (pz - tpPivotZ_) * (float)std::min(1.0, dt * 7.0);
    vec3 pivot{b.neck.x, b.neck.y, tpPivotZ_};
    if (const world::Tile* t = world_->tileIfLoaded(ifloor(pivot.x), ifloor(pivot.y)))
        if (t->open()) pivot.z = std::clamp(pivot.z, t->floor + 0.3f, t->ceil - 0.16f);
    vec3 fwd = forwardFromAngles(c.yaw, c.pitch);
    vec3 right{-std::sin(c.yaw), std::cos(c.yaw), 0.0f};
    vec3 want = pivot - fwd * 2.3f + right * 0.42f + vec3(0, 0, 0.18f);
    vec3 d = want - pivot;
    float len = length(d);
    vec3 dir = d / std::max(len, 1e-4f);
    auto freeAt = [&](vec3 q) {
        const float r = 0.16f;
        const float offs[5][2] = {{0, 0}, {r, 0}, {-r, 0}, {0, r}, {0, -r}};
        for (const auto& o : offs) {
            const world::Tile* t = world_->tileIfLoaded(ifloor(q.x + o[0]), ifloor(q.y + o[1]));
            if (!t || t->solid() || q.z < t->floor + 0.14f || q.z > t->ceil - 0.14f) return false;
        }
        return true;
    };
    float freeLen = len;
    for (float t = 0.05f; t <= len; t += 0.05f)
        if (!freeAt(pivot + dir * t)) {
            freeLen = t - 0.1f;
            break;
        }
    freeLen = std::max(0.2f, freeLen);
    if (freeLen < tpDist_) tpDist_ = freeLen;
    else tpDist_ += (freeLen - tpDist_) * std::min(1.0, dt * 3.0);
    c.pos = pivot + dir * (float)tpDist_;
    c.fovMul = 1.0f + body_.fovKick() * 0.5f;
    return c;
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
    // V6: ohne Tastenangabe - die Oberflaeche setzt die tatsaechliche Belegung davor
    if (pk) {
        const ItemDef* d = content_.item(pk->item);
        return std::format("{} {}", d ? d->name : pk->item, in ? in->prompt : "aufnehmen");
    }
    return in ? in->prompt : "";
}

void GameSession::interact() {
    if (target_ == ecs::kNull) return;
    auto* pk = reg_.get<Pickup>(target_);
    if (!pk) return;
    const ItemDef* d = content_.item(pk->item);
    if (inventory_.add(pk->item)) {
        // abgelegte Items verschwinden aus ihrer Liste, Theken-Items gelten dauerhaft als aufgenommen
        if (pk->worldId.rfind("drop:", 0) == 0)
            std::erase_if(dropped_, [&](const DroppedItem& d) { return d.id == pk->worldId; });
        else picked.insert(pk->worldId);
        bool rare = d && d->rare;
        GameEvent ev{GameEvent::Pickup, std::format("{}{} aufgenommen", rare ? "Seltener Fund!  " : "", d ? d->name : pk->item),
                     rare ? 3.2f : 2.2f};
        ev.rare = rare;
        events_.push_back(ev);
        // die Hand greift zum Item
        if (auto* t = reg_.get<Transform>(target_))
            body_.gesture(Body::Gesture::Grab, t->pos, d ? d->model : pk->item, d ? (float)d->holdOffset : 0.0f,
                          d ? (float)d->scale : 1.0f);
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
    // die Hand fuehrt das Item zum Mund
    body_.gesture(Body::Gesture::Use, {}, d->model, (float)d->holdOffset, (float)d->scale);
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
    j.set("explored", explore_.toJson());  // V6: aufgedeckte Minimap
    // V6: abgelegte Items (Lage wie zuletzt gesehen)
    Json dr = Json::array();
    for (const auto& d : dropped_) {
        Json e = Json::object();
        e.set("id", d.id);
        e.set("kind", d.kind);
        e.set("x", (double)d.pos.x);
        e.set("y", (double)d.pos.y);
        e.set("z", (double)d.pos.z);
        e.set("yaw", (double)d.yaw);
        dr.push(std::move(e));
    }
    j.set("dropped", std::move(dr));
    j.set("drop_next", nextDrop_);
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
    // V6: Erkundung (fehlt bei V3-V5: Karte beginnt leer und fuellt sich ab hier)
    explore_.fromJson(j["explored"]);
    // V6: abgelegte Items (erscheinen, sobald ihr Chunk geladen ist)
    for (auto& d : dropped_)
        if (reg_.alive(d.entity)) reg_.destroy(d.entity);
    dropped_.clear();
    for (const Json& e : j["dropped"].items()) {
        DroppedItem d;
        d.id = e["id"].asString("");
        d.kind = e["kind"].asString("");
        d.pos = {e["x"].asFloat(), e["y"].asFloat(), e["z"].asFloat()};
        d.yaw = e["yaw"].asFloat();
        if (d.id.rfind("drop:", 0) == 0 && content_.item(d.kind) && dropped_.size() < kMaxDropped) dropped_.push_back(d);
    }
    nextDrop_ = std::max<long long>(1, j["drop_next"].asInt(1));
    for (const auto& d : dropped_) nextDrop_ = std::max(nextDrop_, std::atoll(d.id.c_str() + 5) + 1);
    body_.reset();
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
