#include "gameplay/session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>

#include "anim/figures.hpp"
#include "core/log.hpp"
#include "core/rng.hpp"
#include "gameplay/kin_l3.hpp"  // V13.3 Level 3
#include "props/prop_library.hpp"
#include "world/map_def.hpp"
#include "world/room.hpp"
#include "world/sector.hpp"

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
    hitScale_ = std::clamp(d.stalkerHit / 35.0, 0.3, 2.0);  // V13.2
    world_ = std::make_unique<world::World>(level, seed, d.itemsNone, d.itemsOne, jobs, loadRadius, content.supplyRules());
    tiles_ = std::make_unique<TileAdapter>(*world_);
    world_->onChunkLoaded = [this](const world::ChunkData& cd) { onChunk(cd, true); };
    world_->onChunkUnloaded = [this](const world::ChunkData& cd) { onChunk(cd, false); };
    phenomena_.setup(world_->level(), (u64)seed);  // V13.3: Erscheinungen ohne Figur (session_phenomena.cpp)
    purple_.setup(world_->level(), (u64)seed);     // V13.3: purpifizierte Wesen in der Wand (session_purple.cpp)
    // V7: Laufbuch (V8: in der Welt beginnt jeder Lauf in der ersten Ebene)
    runLog_.levels.push_back(world_->level().world() ? world_->level().regions.front().id : levelId);
}

GameSession::~GameSession() {
    // Welt zuerst: sie wartet auf ihre Jobs und ruft dabei keine Listener mehr auf
    world_->onChunkLoaded = nullptr;
    world_->onChunkUnloaded = nullptr;
    world_.reset();
}

phys::TileSource& GameSession::tileSrc() { return *tiles_; }

void GameSession::placeAtSpawn() {
    auto [x, y, z, a] = world_->findSpawn();
    player_ = Player(x, y, z, a);
    body_.reset();
    transitionsArmed_ = false;  // V11: wer im Ausloesebereich ankommt, muss ihn erst verlassen
    // V11: der Sicherungspunkt am Start gilt still als erreicht
    if (checkpoint_.empty() && world_->level().mapped()) {
        double best = 1e18;
        for (const auto& cp : world_->level().map->checkpoints) {
            const double d = (cp.x - x) * (cp.x - x) + (cp.y - y) * (cp.y - y);
            if (d < best) best = d, checkpoint_ = cp.id, checkpointLabel_ = cp.label;
        }
    }
}

bool GameSession::placeAtAnchor(const std::string& id) {
    const world::LevelDef& L = world_->level();
    if (!L.mapped()) return false;
    const world::MapTransition* t = L.map->transition(id);
    if (!t) return false;
    player_ = Player(t->x, t->y, world_->tile(ifloor(t->x), ifloor(t->y)).floor, t->yaw);
    body_.reset();
    transitionsArmed_ = false;  // wer im Ausloesebereich ankommt, muss ihn erst verlassen
    double best = 1e18;
    for (const auto& cp : L.map->checkpoints) {
        const double d = (cp.x - t->x) * (cp.x - t->x) + (cp.y - t->y) * (cp.y - t->y);
        if (d < best) best = d, checkpoint_ = cp.id, checkpointLabel_ = cp.label;
    }
    log::info("Ankunft am Anker {} ({}, {})", id, t->x, t->y);
    return true;
}

void GameSession::respawnAtCheckpoint() {
    const world::MapCheckpoint* cp = nullptr;
    if (world_->level().mapped())
        for (const auto& c : world_->level().map->checkpoints)
            if (c.id == checkpoint_) cp = &c;
    if (cp) {
        player_ = Player(cp->x, cp->y, world_->tile(ifloor(cp->x), ifloor(cp->y)).floor, cp->yaw);
        body_.reset();
        transitionsArmed_ = false;
    } else {
        placeAtSpawn();
    }
    preload();
    stats_.health = 100.0;
    stats_.sanity = std::max(stats_.sanity, 60.0);
    stats_.zeroTime = 0.0;
    stats_.stamina = 100.0;
    stats_.exhausted = false;
    stats_.dead = false;
    deathReported_ = false;
    deathCause_.clear();
    killCam_ = false, killT_ = 0.0, killRole_ = -1;
    air_ = 30.0, deepT_ = 0.0, deepStage_ = 0;  // V11
    phenomena_.reset(0);  // V13.3: neuer Anfang ohne Pilz und Ranken
    presence_.clear();
    presence_.setGrace(90.0);  // Schonzeit: kein Stromausfall, keine Entity gleich wieder
    haunt_.reset();
    events_.push_back({GameEvent::TeleportCut});
    log::info("Wiedereinstieg am Sicherungspunkt {} ({})", checkpoint_.empty() ? "-" : checkpoint_, checkpointLabel_);
}

// V11: Sicherungspunkte der Karte - wer vorbeigeht (2,2 m), speichert hier. Nicht mitten in einer
// Begegnung (sonst saesse man nach dem Tod gleich wieder in der Gefahr).
void GameSession::trackCheckpoints() {
    const world::LevelDef& L = world_->level();
    if (!L.mapped() || stats_.dead || presence_.threatActive()) return;
    for (const auto& cp : L.map->checkpoints) {
        if (cp.id == checkpoint_) continue;
        const double dx = cp.x - player_.x, dy = cp.y - player_.y;
        if (dx * dx + dy * dy > 2.2 * 2.2) continue;
        const world::Tile* t = world_->tileIfLoaded(ifloor(cp.x), ifloor(cp.y));
        if (!t || std::fabs(player_.z - t->floor) > 1.0) continue;
        checkpoint_ = cp.id, checkpointLabel_ = cp.label;
        GameEvent ev{GameEvent::Checkpoint, cp.label, 2.6f};
        ev.pos = {(float)cp.x, (float)cp.y, t->floor + 2.2f};
        events_.push_back(ev);
        log::info("Sicherungspunkt {} ({})", cp.id, cp.label);
        break;
    }
}

bool GameSession::rescueAfterFailedTransition() {
    const bool fell = lastTransitionKind_ == "hole";
    lastTransitionKind_.clear();
    if (!fell) return false;
    transitionInside_.clear();
    // V11: wer in ein Loch ohne Ziel faellt (Lochfelder in Level 0), steht wieder am letzten sicheren Ort
    const world::MapCheckpoint* cp = nullptr;
    if (world_->level().mapped())
        for (const auto& c : world_->level().map->checkpoints)
            if (c.id == checkpoint_) cp = &c;
    if (cp) {
        player_ = Player(cp->x, cp->y, world_->tile(ifloor(cp->x), ifloor(cp->y)).floor, cp->yaw);
        body_.reset();
        transitionsArmed_ = false;
    } else {
        placeAtSpawn();
    }
    preload();
    return true;
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
            if (isPicked(it->id)) continue;
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
        spawnFinds(cd);  // V7: Notizen, Batterien, Notausgaenge
        spawnTraces(cd);  // V13.3: offenes Mandelwasser (session_phenomena.cpp)
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

// V7: Fundstuecke der Raeume dieses Chunks (jeder Fund erscheint mit dem Chunk seiner Kachel)
void GameSession::spawnFinds(const world::ChunkData& cd) {
    spawnElevators(cd);  // V8
    std::unordered_set<const world::Room*> seen;
    std::vector<FindSpot> spots;
    for (int ly = 0; ly < world::CS; ++ly)
        for (int lx = 0; lx < world::CS; ++lx) {
            const world::Room* r = cd.tile(lx, ly).room;
            if (!r || !seen.insert(r).second || !r->prepared()) continue;
            spots.clear();
            double strata = -1.0;  // V9: Lage in der Schicht (Sicherungskaesten nur tief drinnen)
            if (world_->level().world()) {
                auto [rcx, rcy] = r->center();
                strata = world_->generator().zones().strataCoord(rcx, rcy);
            }
            roomFinds(*r, world_->level(), spots, strata);
            for (const FindSpot& s : spots) {
                if (s.tx < cd.x0() || s.tx >= cd.x0() + world::CS || s.ty < cd.y0() || s.ty >= cd.y0() + world::CS) continue;
                ecs::Entity e = ecs::kNull;
                if (s.kind == FindSpot::Note) {
                    if (isPicked(s.id)) continue;
                    // bevorzugt eine Notiz, die noch nicht im Notizbuch steht (V9: Hinweise zuerst)
                    int idx = content_.priorityNote(journal_, world_->level().world());
                    for (u64 k = 0; k < 8 && idx < 0; ++k) {
                        idx = content_.noteFor(world_->level().regionId(r->zone), s.hash + k * 0x9E3779B97F4A7C15ull,
                                               world_->level().world());
                        if (idx < 0 || std::find(journal_.begin(), journal_.end(), idx) == journal_.end()) break;
                        if (k < 7) idx = -1;
                    }
                    if (idx < 0) continue;
                    e = prefabs_.spawn(reg_, "note", s.pos, s.yaw);
                    if (auto* n = reg_.get<NoteFind>(e)) n->index = idx, n->id = s.id;
                } else if (s.kind == FindSpot::Battery) {
                    if (isPicked(s.id) || !content_.item("battery")) continue;
                    e = prefabs_.spawn(reg_, "item_pickup", s.pos, s.yaw);
                    if (auto* p = reg_.get<Pickup>(e)) p->item = "battery", p->worldId = s.id;
                    if (auto* m = reg_.get<MeshRenderer>(e)) m->model = "battery";
                } else if (s.kind == FindSpot::Glowstick) {  // V13.3
                    // isPicked: auch vom Host gemeldet, bevor der Chunk hier geladen war (Koop, ferner Fund / spaeter Beitritt)
                    if (isPicked(s.id) || !content_.item("glowstick")) continue;
                    e = prefabs_.spawn(reg_, "item_pickup", s.pos, s.yaw);
                    if (auto* p = reg_.get<Pickup>(e)) p->item = "glowstick", p->worldId = s.id;
                    if (auto* m = reg_.get<MeshRenderer>(e)) m->model = "glowstick";
                } else if (s.kind == FindSpot::FuseBox) {
                    // V9: Sicherungskasten - knapp vor der Wand; schon geleert: offen und still
                    if (!content_.item(s.item)) continue;
                    const vec3 into{std::cos(s.yaw) * 0.04f, std::sin(s.yaw) * 0.04f, 0.0f};
                    e = prefabs_.spawn(reg_, "fusebox", s.pos + into, s.yaw);
                    if (auto* fb = reg_.get<FuseBox>(e)) {
                        fb->id = s.id, fb->item = s.item;
                        if (isPicked(s.id)) {
                            fb->taken = true;
                            if (auto* m = reg_.get<MeshRenderer>(e)) m->model = "fusebox_empty";
                            reg_.remove<Interactable>(e);
                            reg_.remove<AudioEmitter>(e);
                        }
                    }
                } else {
                    e = prefabs_.spawn(reg_, "exit_door", s.pos, s.yaw);
                    if (auto* x = reg_.get<ExitDoor>(e)) x->id = s.id;
                }
                if (e != ecs::kNull) reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
            }
        }
    // V11: feste Karte - Sicherungskaesten an ihren Spawnpunkten
    if (world_->level().mapped())
        if (const world::Sector* sec = world_->sectorIfReady(cd.x0() + 0.5, cd.y0() + 0.5))
            for (const world::Sector::NoteSpot& fs : sec->fuseboxes) {
                if (fs.x < (float)cd.x0() || fs.x >= (float)(cd.x0() + world::CS) || fs.y < (float)cd.y0() || fs.y >= (float)(cd.y0() + world::CS))
                    continue;
                if (!content_.item(fs.item)) continue;
                const vec3 into{std::cos(fs.yaw) * 0.04f, std::sin(fs.yaw) * 0.04f, 0.0f};
                ecs::Entity e = prefabs_.spawn(reg_, "fusebox", vec3{fs.x, fs.y, fs.z} + into, fs.yaw);
                if (e == ecs::kNull) continue;
                if (auto* fb = reg_.get<FuseBox>(e)) {
                    fb->id = fs.id, fb->item = fs.item;
                    if (isPicked(fs.id)) {
                        fb->taken = true;
                        if (auto* m = reg_.get<MeshRenderer>(e)) m->model = "fusebox_empty";
                        reg_.remove<Interactable>(e);
                        reg_.remove<AudioEmitter>(e);
                    }
                }
                reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
            }
    // V11: feste Karte - Notizen an ihren Spawnpunkten (der Lauf waehlt die Punkte, die Sitzung den Text)
    if (world_->level().mapped())
        if (const world::Sector* sec = world_->sectorIfReady(cd.x0() + 0.5, cd.y0() + 0.5))
            for (const world::Sector::NoteSpot& ns : sec->notes) {
                if (ns.x < (float)cd.x0() || ns.x >= (float)(cd.x0() + world::CS) || ns.y < (float)cd.y0() || ns.y >= (float)(cd.y0() + world::CS))
                    continue;
                if (isPicked(ns.id)) continue;
                int idx = -1;
                if (auto it = mapNotes_.find(ns.id); it != mapNotes_.end()) {
                    idx = it->second;
                } else {
                    std::vector<int> taken = journal_;  // gelesene und anderswo schon ausgelegte Notizen
                    for (const auto& [k, v] : mapNotes_) taken.push_back(v);
                    idx = content_.noteForLevel(levelId, hash64(world_->seed(), ns.id.c_str()), taken);
                    if (idx >= 0) {
                        mapNotes_[ns.id] = idx;
                        log::info("Notiz \"{}\" bei ({:.0f}, {:.0f})", content_.notes()[(size_t)idx].title, ns.x, ns.y);
                    }
                }
                if (idx < 0) continue;
                ecs::Entity e = prefabs_.spawn(reg_, "note", {ns.x, ns.y, ns.z}, ns.yaw);
                if (e == ecs::kNull) continue;
                if (auto* n = reg_.get<NoteFind>(e)) n->index = idx, n->id = ns.id;
                reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
            }
}

// V7: Notausgaenge, die man gesehen hat, merkt sich die Karte
void GameSession::trackExits(double dt) {
    exitScan_ -= dt;
    if (exitScan_ > 0.0) return;
    exitScan_ = 0.4;
    reg_.each<ExitDoor, Transform>([&](ecs::Entity, ExitDoor& x, Transform& t) {
        for (const auto& k : knownExits_)
            if (k.id == x.id) return;
        double dx = t.pos.x - player_.x, dy = t.pos.y - player_.y;
        if (dx * dx + dy * dy > 28.0 * 28.0) return;
        // Sichtlinie zum Schild (knapp vor der Wand)
        double sx = t.pos.x + std::cos(t.yaw) * 0.4, sy = t.pos.y + std::sin(t.yaw) * 0.4;
        if (!phys::clearLine(*tiles_, player_.x, player_.y, player_.z, sx, sy, 0.6, 1.4)) return;
        knownExits_.push_back({x.id, t.pos, t.yaw});
        events_.push_back({GameEvent::Toast, "Ein grünes Schild: NOTAUSGANG", 3.0f});
    });
}

// V11: Uebergaenge der festen Karte (Bodenloch, Naht, Treppe ...). Wer den Ausloesebereich betritt -
// beim Loch: wer hineinfaellt -, wechselt die Ebene. Erneut erst nach dem Verlassen des Bereichs.
void GameSession::trackTransitions() {
    const world::LevelDef& L = world_->level();
    if (!L.mapped()) return;
    const world::MapTransition* hit = nullptr;
    for (const world::MapTransition& t : L.map->transitions) {
        if ((t.target.empty() && t.kind != "end") || t.x1 <= t.x0) continue;
        if (player_.x < (double)t.x0 || player_.x >= (double)t.x1 || player_.y < (double)t.y0 || player_.y >= (double)t.y1) continue;
        if (t.kind == "hole" && (!room_ || player_.z > room_->floor - 2.5)) continue;
        if (t.kind == "dive" && !(player_.waterZ > -900.0 && player_.z < player_.waterZ - 7.0)) continue;  // V11: tief tauchen
        hit = &t;
        break;
    }
    if (!transitionsArmed_) {
        transitionsArmed_ = true;
        transitionInside_ = hit ? hit->id : "";
        return;
    }
    if (!hit) {
        transitionInside_.clear();
        return;
    }
    if (transitionInside_ == hit->id) return;
    transitionInside_ = hit->id;
    if (hit->kind == "end") {  // V11: das Ende der Reise (Level 37)
        runLog_.escaped = true;
        runLog_.escapeTime = stats_.playTime;
        events_.push_back({GameEvent::Escaped});
        log::info("Das Ende der Reise erreicht ({})", hit->id);
        return;
    }
    if (!hit->needs.empty() && inventory_.count(hit->needs) <= 0) {
        // V11: der Aufzug faehrt erst mit der Sicherung (Level 3)
        const ItemDef* d = content_.item(hit->needs);
        events_.push_back({GameEvent::Toast, std::format("Kein Strom. Es fehlt: {}.", d ? d->name : hit->needs), 4.0f});
        return;
    }
    if (!hit->needs.empty()) {  // die Sicherung sitzt jetzt im Aufzug
        for (int i = 0; i < Inventory::kSize; ++i)
            if (inventory_.slots[(size_t)i] && inventory_.slots[(size_t)i]->kind == hit->needs) {
                inventory_.take(i);
                break;
            }
    }
    lastTransitionKind_ = hit->kind;
    GameEvent ev{GameEvent::ExitOpened};
    ev.text = hit->target;
    ev.sound = hit->kind == "hole" ? "debris" : hit->kind == "elevator" ? "elev_doors" : hit->kind == "stairs" ? "door_steel" : "";
    ev.anchor = hit->id;
    ev.pos = {(float)player_.x, (float)player_.y, (float)player_.z};
    events_.push_back(ev);
    log::info("Uebergang {} ({}) -> {}", hit->id, hit->kind, hit->target);
}

// V11: Wasser - Luft unter Wasser (danach Ertrinken), kaltes Wasser, Schwimmzuege; die Tiefe in Level 7:
// wer lange im tiefen Wasser bleibt, ruft etwas herauf. Es kuendigt sich an (Grollen, Wellen); Licht nach
// unten haelt es kurz fern; wer bleibt, wird geholt.
void GameSession::updateWater(double dt) {
    const bool inWater = player_.wading || player_.swimming;
    if (player_.stroke) {
        static u32 n = 0;
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = player_.underwater ? "underwater" : "swim";
        ev.pos = {(float)player_.x, (float)player_.y, (float)player_.z + 1.2f};
        ev.intensity = player_.underwater ? 0.0f : 0.5f;
        if (!player_.underwater) events_.push_back(ev);
        ++n;
    }
    // Luft
    if (player_.underwater) {
        if (air_ > 10.0 && air_ - dt <= 10.0) events_.push_back({GameEvent::Toast, "Die Luft wird knapp.", 2.5f});
        air_ -= dt;
        if (air_ < 0.0) {
            stats_.health = std::max(0.0, stats_.health - dt * 12.0);
            if (stats_.health <= 0.0 && !stats_.dead) stats_.dead = true, deathCause_ = "wasser";
        }
    } else {
        air_ = std::min(30.0, air_ + dt * 6.0);
    }
    if (inWater && room_ && room_->type && room_->type->waterDamage > 0.0) {
        stats_.health = std::max(0.0, stats_.health - dt * room_->type->waterDamage);
        if (stats_.health <= 0.0 && !stats_.dead) stats_.dead = true, deathCause_ = "wasser";
    }
    // die Tiefe
    const bool deep = player_.swimming && room_ && room_->type && room_->type->deep && player_.z < player_.waterZ - 2.0;
    const FlashBeam fb = flashBeam();
    if (deep) deepT_ += dt * (fb.on && fb.dir.z < -0.4f ? -1.5 : 1.0);
    else deepT_ = std::max(0.0, deepT_ - dt * 0.5);
    deepT_ = std::max(0.0, deepT_);
    const vec3 below{(float)player_.x, (float)player_.y, (float)player_.z - 6.0f};
    if (deepStage_ == 0 && deepT_ > 40.0) {
        deepStage_ = 1;
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "deep_rumble", ev.pos = below, ev.intensity = 1.0f;
        events_.push_back(ev);
        events_.push_back({GameEvent::Toast, "Unter dir bewegt sich etwas sehr Grosses.", 3.5f});
    } else if (deepStage_ == 1 && deepT_ > 58.0) {
        deepStage_ = 2;
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "deep_surge", ev.pos = below, ev.intensity = 1.0f;
        events_.push_back(ev);
    } else if (deepStage_ == 2 && deepT_ > 70.0 && !stats_.dead) {
        deepStage_ = 3;
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "deep_strike", ev.pos = below, ev.intensity = 1.0f;
        events_.push_back(ev);
        stats_.health = 0.0, stats_.dead = true, deathCause_ = "tiefe";
    } else if (deepStage_ > 0 && deepStage_ < 3 && deepT_ < 20.0) {
        deepStage_ = 0;
    }
}

void GameSession::exitSigns(std::vector<std::pair<vec3, vec3>>& out, float radius) const {
    auto& reg = const_cast<ecs::Registry&>(reg_);
    reg.each<ExitDoor, Transform>([&](ecs::Entity, ExitDoor&, Transform& t) {
        double dx = t.pos.x - player_.x, dy = t.pos.y - player_.y;
        if (dx * dx + dy * dy > (double)radius * radius) return;
        vec3 dir{std::cos(t.yaw), std::sin(t.yaw), 0.0f};
        out.push_back({t.pos + dir * 0.14f + vec3{0, 0, 2.36f}, dir});
    });
}

// V8: In welcher Ebene ist der Spieler? Ein Raum zaehlt erst, wenn seine Ebene dort klar
// ueberwiegt (an Nahtstellen mischen sich beide), und die Ebene erst, wenn man eine Weile in
// ihr unterwegs ist - so merkt man oft gar nicht sofort, dass man woanders ist. Beim ersten
// Schritt in eine unbekannte Ebene faellt weit hinten eine Tuer ins Schloss; der Name der Ebene
// erscheint erst spaeter.
void GameSession::trackRegion(double dt) {
    const world::LevelDef& L = world_->level();
    if (!L.world() || !room_) return;
    const int r = L.regionOfZone(room_->zone);
    double share = 0.0;
    for (int z = 0; z < L.zoneCount(); ++z)
        if (L.zones[(size_t)z].region == r) share += room_->zoneW[(size_t)z];
    const std::string& id = L.regions[(size_t)r].id;
    const bool known = std::find(runLog_.levels.begin(), runLog_.levels.end(), id) != runLog_.levels.end();
    if (region_ < 0) {  // Spielbeginn oder geladener Stand: still uebernehmen
        region_ = regionCand_ = r;
        if (!known) runLog_.levels.push_back(id);
        return;
    }
    if (r != regionCand_) {
        regionCand_ = r;
        regionTime_ = 0.0;
        slamPending_ = r != region_ && !known;
    }
    if (r == region_ || share < 0.7) return;
    if (slamPending_) {
        slamPending_ = false;
        GameEvent ev{GameEvent::DistantSlam};
        const double a = player_.angle + 3.14159265358979;  // hinter dem Spieler
        ev.pos = vec3{(float)(player_.x + std::cos(a) * 30.0), (float)(player_.y + std::sin(a) * 30.0), (float)player_.z + 1.5f};
        events_.push_back(ev);
    }
    regionTime_ += dt;
    if (regionTime_ < 14.0) return;
    region_ = r;
    ++runLog_.exits;  // in der Welt: Uebergaenge zwischen Ebenen
    if (!known) {
        runLog_.levels.push_back(id);
        GameEvent ev{GameEvent::RegionFound, regionName(), 7.0f};
        ev.index = r;
        events_.push_back(ev);
    }
}

bool GameSession::zoneKnown(int zone) const {
    const world::LevelDef& L = world_->level();
    if (!L.world()) return true;
    const std::string& id = L.regions[(size_t)L.regionOfZone(zone)].id;
    return std::find(runLog_.levels.begin(), runLog_.levels.end(), id) != runLog_.levels.end();
}

std::string GameSession::regionName() const {
    const world::LevelDef& L = world_->level();
    if (!L.world()) return L.title.empty() ? L.name : std::format("{} · {}", L.name, L.title);
    const world::RegionDef& R = L.regions[(size_t)currentRegion()];
    return R.title.empty() ? R.name : std::format("{} · {}", R.name, R.title);
}

// V7: Halluzinationen -> Ereignisse fuer Klang und Bild
void GameSession::updateHaunt(double dt) {
    hauntEvents_.clear();
    haunt_.update(dt, player_, *world_, room_, stats_.sanity, player_.stepped, hallucinations && !stats_.dead, hauntEvents_);
    for (const Haunt::Event& h : hauntEvents_) {
        GameEvent ev;
        ev.pos = h.pos;
        switch (h.type) {
            case Haunt::Event::Step:
                ev.type = GameEvent::PhantomStep;
                ev.surface = surfaceUnder();
                break;
            case Haunt::Event::PowerOff:
                ev.type = GameEvent::Blackout, ev.index = 1;
                if (!h.real) ++runLog_.visions;
                break;
            case Haunt::Event::PowerOn: ev.type = GameEvent::Blackout, ev.index = 0; break;
            case Haunt::Event::GhostGone: ev.type = GameEvent::Blackout, ev.index = 2; break;
            case Haunt::Event::Breaker: ev.type = GameEvent::Blackout, ev.index = 3; break;  // V11
            case Haunt::Event::GhostSeen:
                ev.type = GameEvent::GhostSeen;
                ++runLog_.visions;
                break;
        }
        events_.push_back(ev);
    }
}

void GameSession::carryOver(const GameSession& from) {
    stats_ = from.stats_;
    stats_.dead = false;
    inventory_ = from.inventory_;
    flash_ = from.flash_;
    journal_ = from.journal_;
    player_.distance = from.player_.distance;
    runLog_ = from.runLog_;
    runLog_.nextLevel(levelId);
    // V11: Gedaechtnis der Ebenen - die verlassene merken, die betretene wiederherstellen
    runSeed = from.runSeedOr();
    levelMemory = from.levelMemory;
    levelMemory[from.levelId] = {from.picked, from.mapNotes_};
    if (auto it = levelMemory.find(levelId); it != levelMemory.end()) picked = it->second.picked, mapNotes_ = it->second.notes;
    phenomena_.ailment(0) = from.phenomena_.ailment(0);  // V13.3: der Pilz reist mit (die Garden-Uhr geht draussen zurueck)
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
    // aus der Hand vor dem Koerper, leicht nach oben und in Blickrichtung (mit Schwung des Laufens)
    throwFrom(player_, body_.eye(), kind, count, netSelfId);  // V13.2: auch fuer die Gaeste (session_coop.cpp)
    const ItemDef* def = content_.item(kind);
    GameEvent ev{GameEvent::Drop,
                 count > 1 ? std::format("{} × {} abgelegt", count, def ? def->name : kind)
                           : std::format("{} abgelegt", def ? def->name : kind),
                 1.6f};
    events_.push_back(ev);
    body_.gesture(Body::Gesture::Toss, body_.eye() - vec3{0, 0, 0.32f}, "");
}

bool GameSession::dropSlot(int idx, bool all) {
    if (netAction) {  // V13.2 Koop (Gast): der Host legt ab und meldet es allen
        netAction(all ? 4 : 3, (u32)std::clamp(idx, 0, Inventory::kSize - 1));
        if (idx >= 0 && idx < Inventory::kSize && inventory_.slots[(size_t)idx]) body_.gesture(Body::Gesture::Toss, body_.eye() - vec3{0, 0, 0.32f}, "");
        return true;
    }
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
                    noisePings_.push_back({np, 0.6f * std::clamp(impact / 4.0f, 0.5f, 1.0f)});  // V11: das Buendel hoert es
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
        if (netRole != NetRole::Guest) stats_.update(dt);  // V13.2 Koop: beim Gast rechnet der Host die Werte
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
    player_.speedMul *= 1.0 - 0.5 * presence_.trance();  // V13.3: der Anblick des Wranglers laehmt
    player_.speedMul *= 1.0 - 0.55 * presence_.jerryHold(netSelfId);  // V13.3 Teil 4: Jerrys Bann laehmt die Schritte
    // V13.3: betaeubt (Nguithr'xurh) langsamer und ohne Sprint; durch das Gewirr einer Growler-Leiche waten
    player_.speedMul *= presence_.slowFor(netSelfId, {(float)player_.x, (float)player_.y, (float)player_.z});
    player_.speedMul *= phenomena_.speedMul(0);            // V13.3: Fieber, Ranken (session_phenomena.cpp)
    player_.speedMul *= presence_.packSlow(netSelfId);     // V13.3 Level 3: ein Hound hat ins Bein gebissen (hinkt)
    PlayerControl c = paused ? PlayerControl{} : control;
    if (!stats_.canSprint() || presence_.stupor(netSelfId) > 0.5f || presence_.packSlow(netSelfId) < 0.8f) c.sprint = false;
    if (ride_.active) c.forward = c.side = 0.0f, c.sprint = c.jump = false;  // V8: im Aufzug nur umsehen
    if (stats_.dead) c.forward = c.side = 0.0f, c.sprint = c.jump = c.jumpHeld = false;  // V13.2: tot (Koop: zusehen)
    if (phenomena_.frozen(0)) c.forward = c.side = 0.0f, c.sprint = c.jump = c.jumpHeld = false;  // V13.3: ihre Einladung
    const double prevX = player_.x, prevY = player_.y;
    // V11: Wasserspiegel am Ort (Raum unter dem Spieler)
    if (const world::Room* wr = world_->roomAt(player_.x, player_.y)) player_.waterZ = wr->water;
    else player_.waterZ = -999.0;
    if (!world_->level().flashlight && flash_.on) flash_.on = false;  // V11: Level 6
    player_.update(dt, c, *tiles_);
    // V13.3: Ouroboros-Arbeiter weichen nicht aus - wer im Weg steht, wird beiseitegeschoben (Wand bleibt Wand)
    if (!paused && !stats_.dead && !ride_.active) {
        const vec3 sh = presence_.shoveFor(netSelfId, {(float)player_.x, (float)player_.y, (float)player_.z});
        if (sh.x != 0.0f || sh.y != 0.0f) phys::moveAndSlide(*tiles_, player_.x, player_.y, player_.z, sh.x * dt, sh.y * dt, player_.shape);
    }
    // V13.3: der Leib des Wranglers zieht den Blick an (KREATUR_Wrangler.md) - gegensteuern, wegsehen
    if (vec3 at; !paused && !stats_.dead && presence_.trance(&at) > 0.02f) {
        const float tr = presence_.trance();
        const vec3 d = at - body_.eye();
        const double k = std::min(1.0, dt * 1.1 * tr * tr);
        player_.angle += std::remainder(std::atan2(d.y, d.x) - player_.angle, 2.0 * 3.14159265) * k;
        player_.pitch += (std::atan2(d.z, std::max(0.3f, std::hypot(d.x, d.y))) - player_.pitch) * k * 0.7;
        if (netRole != NetRole::Guest) stats_.sanity = std::max(0.0, stats_.sanity - dt * 0.45 * tr);
    }
    // V13.3 Teil 4: Jerrys Bann (KREATUR_Jerry.md) - ab 0,45 zieht er den Blick zu seinem Gesicht; wegsehen kostet Kraft
    if (vec3 at; !paused && !stats_.dead && presence_.jerrySpell(netSelfId, &at) > 0.45f) {
        const float pull = (presence_.jerrySpell(netSelfId) - 0.45f) / 0.55f;
        const vec3 d = at - body_.eye();
        const double k = std::min(1.0, dt * 1.6 * pull * pull);
        player_.angle += std::remainder(std::atan2(d.y, d.x) - player_.angle, 2.0 * 3.14159265) * k;
        player_.pitch += (std::atan2(d.z, std::max(0.3f, std::hypot(d.x, d.y))) - player_.pitch) * k * 0.7;
        if (netRole != NetRole::Guest) stats_.sanity = std::max(0.0, stats_.sanity - dt * 0.6 * pull);
    }
    if (!paused) updateElevators(dt, prevX, prevY);  // V8
    if (!paused) {
        bool wasExhausted = stats_.exhausted;
        stats_.updateStamina(dt, player_.sprinting, player_.groundSpeed > 0.3);
        if (player_.jumped) stats_.spendStamina(4.0);
        if (stats_.exhausted && !wasExhausted) events_.push_back({GameEvent::Exhausted, "Außer Atem – kurz verschnaufen …", 2.5f});
        updateDropped(dt);
        updateLures(dt);  // V13.3: Leuchtstaebe (session_lure.cpp)
        updateWater(dt);  // V11
        // V7: Taschenlampe verbraucht Batterie
        if (flash_.update(dt)) events_.push_back({GameEvent::FlashEmpty, "Die Batterie der Taschenlampe ist leer.", 3.0f});
    }
    player_.breathless = stats_.breathless();
    applyBodyStyle();  // V10
    if (netRole == NetRole::Host) {  // V13.2 Koop: auch um die Figuren der Gaeste laden
        std::vector<std::pair<double, double>> centers;
        for (const Pawn* p : pawns)
            if (p) centers.push_back({p->player.x, p->player.y});
        world_->setExtraCenters(std::move(centers));
    }
    world_->update(player_.x, player_.y);
    // V13: Schaltfolge der Lampe (siehe toggleFlashlight). Ist sie an, ist sie gehoben (auch wenn ein Spielstand oder
    // ein Test sie direkt einschaltet); ging sie ohne Daumen aus (Batterie leer), sinkt sie kurz danach
    if (flash_.on && flashPending_ == 0) flashRaise_ = true, flashLowerT_ = -1.0;
    if (!flash_.on && flashPending_ == 0 && flashRaise_ && flashLowerT_ < 0.0) flashLowerT_ = 0.6;
    if (flashPending_ == 1 && !body_.pressing() && (body_.flashWeight() > 0.9f || !body_.viewmodelHand())) {
        body_.pressSwitch();
        flashPending_ = 2;
        flashClickT_ = 0.07;
    }
    if (flashClickT_ >= 0.0 && (flashClickT_ -= dt) < 0.0) flashClick();
    if (flashLowerT_ >= 0.0 && (flashLowerT_ -= dt) < 0.0 && !flash_.on && flashPending_ == 0) flashRaise_ = false;
    body_.setFlashlight(flashRaise_, flash_.on && flash_.output(stats_.playTime) > 0.05f);
    {
        // V11: freier Raum vor der Lampe - vom Auge und von der Hoehe der Lampe aus in Blickrichtung abgetastet
        const float ca = (float)std::cos(player_.angle), sa = (float)std::sin(player_.angle);
        const float cp = (float)std::cos(player_.pitch), spc = (float)std::sin(player_.pitch);
        const vec3 F{ca, sa, 0.0f}, R{-sa, ca, 0.0f}, U{0.0f, 0.0f, 1.0f};
        const vec3 hf = F * cp + U * spc, hu = U * cp - F * spc;
        const vec3 eye = body_.eye();
        float clear = 0.9f;
        for (const vec3& o : {eye, eye - R * 0.15f - hu * 0.22f})
            for (float t = 0.05f; t < clear; t += 0.04f) {
                const vec3 q = o + hf * t;
                const world::Tile* tl = world_->tileIfLoaded(ifloor(q.x), ifloor(q.y));
                if (!tl || tl->solid() || q.z < tl->floor + 0.02f || q.z > tl->ceil - 0.02f) {
                    clear = t;
                    break;
                }
            }
        body_.setFlashClearance(clear);
        // V11: freier Raum fuer das Auge in Richtung seines Versatzes (Koerpermitte -> Auge, waagrecht)
        vec3 off{eye.x - (float)player_.x, eye.y - (float)player_.y, 0.0f};
        vec3 dir = length(off) > 0.02f ? normalize(off) : F;
        float room = 0.6f;
        for (float t = 0.02f; t < 0.6f; t += 0.02f) {
            const vec3 q{(float)player_.x + dir.x * t, (float)player_.y + dir.y * t, eye.z};
            const world::Tile* tl = world_->tileIfLoaded(ifloor(q.x), ifloor(q.y));
            if (!tl || tl->solid() || q.z > tl->ceil - 0.02f) {
                room = t;
                break;
            }
        }
        body_.setEyeRoom(std::max(0.06f, room - 0.13f));
    }
    body_.update(paused ? 0.0 : dt, player_);
    if (!paused) explore_.reveal(*world_, player_.x, player_.y, player_.z, dt);

    room_ = world_->roomAt(player_.x, player_.y);
    if (room_ && !paused) runLog_.enter(room_->sx, room_->sy, room_->index);  // V7: Laufbuch
    if (!paused) trackRegion(dt);                                               // V8
    updateFog(dt);
    // Zonenwechsel dezent einblenden (V4: hud.track_zone)
    if (room_) {
        int z = room_->zone;
        if (zone_ < 0) zone_ = z;
        else if (z != zone_ && room_->zoneW[(size_t)z] > 0.6) {
            zone_ = z;
            // V8: Zonen einer noch unbekannten Ebene verraten sich nicht durch ihren Namen
            if (zoneKnown(z)) events_.push_back({GameEvent::ZoneTitle, world_->level().zones[(size_t)z].name, 3.5f});
        }
    }
    if (!paused) {
        if (player_.stepped) events_.push_back({GameEvent::Footstep, "", 0, surfaceUnder(), "", (float)std::min(1.0, player_.speed() / 3.0)});
        if (player_.landed) events_.push_back({GameEvent::Land, "", 0, surfaceUnder(), "", (float)std::min(1.0, player_.fallSpeed / 8.0)});
        if (player_.jumped) events_.push_back({GameEvent::Jump, "", 0, surfaceUnder(), "", 1.0f});
        supplyHint(dt);
        updateWanderers(dt);
        updateHaunt(dt);  // V7
        if (netRole == NetRole::Host)  // V13.2 Koop: Werte, Lampe, Koerper und Tod der Gaeste (vor den Wesen)
            for (Pawn* p : pawns)
                if (p) pawnTick(*p, dt);
        updatePresence(dt);  // V10
        updatePhenomena(dt);  // V13.3: Pilz, Garden, Gemaelde, Tuerspuren (nach den Wesen: others_ ist gefuellt)
        updatePurple(dt);     // V13.3: purpifizierte Wesen in der Wand (session_purple.cpp)
    }
    if (killCam_ && !paused) killT_ += dt;  // V11
    target_ = ecs::kNull;
    if (!paused) {
        findTarget();
        trackExits(dt);  // V7
        if (netRole == NetRole::Solo) trackTransitions();  // V11 (V13.2 Koop: die Gruppe wechselt, CoopHost)
        if (netRole != NetRole::Guest) trackCheckpoints();  // V11 (V13.2 Koop: entscheidet der Host)
        trackFuses(dt);  // V9
        cabinHints();
    }
    // seltene Items: sanftes, langsames Schimmern (verraet den Fund, ohne zu blenden)
    const float glint = 0.06f + 0.06f * (float)std::sin(stats_.playTime * 2.3);
    reg_.each<MeshRenderer>([&](ecs::Entity e, MeshRenderer& m) { m.highlight = m.glint ? glint : 0.0f; });
    if (target_ != ecs::kNull)
        if (auto* m = reg_.get<MeshRenderer>(target_)) m->highlight = 0.35f + 0.15f * (float)std::sin(stats_.playTime * 5.0);
}

// --- Kamera (V6) ---------------------------------------------------------------------------
ViewCamera GameSession::camera(bool thirdPerson, double dt) {
    ViewCamera c;
    // V11: Schreck - der Blick reisst zum Toetenden herum (0,1 s), das Bild zittert und zoomt heran
    vec3 kh;
    if (killCam_ && presence_.killerHead(kh)) {
        const vec3 eye = body_.eye();
        const vec3 d = kh - eye;
        const float ty = std::atan2(d.y, d.x), tp = std::atan2(d.z, std::max(0.05f, std::hypot(d.x, d.y)));
        const float t = (float)killT_, k = (float)std::min(1.0, killT_ / 0.1);
        const float s = k * k * (3.0f - 2.0f * k);
        c.yaw = killYaw0_ + std::remainder(ty - killYaw0_, 2.0f * 3.14159265f) * s;
        c.pitch = killPitch0_ + (tp - killPitch0_) * s;
        const float shake = 0.045f * std::exp(-t * 2.2f) + 0.008f;
        c.pos = eye + vec3{std::sin(t * 53.0f), std::sin(t * 47.0f + 1.3f), std::sin(t * 61.0f + 2.1f)} * shake * 0.4f;
        c.yaw += std::sin(t * 39.0f) * shake;
        c.pitch += std::sin(t * 43.0f + 0.7f) * shake;
        c.roll = std::sin(t * 29.0f) * shake * 1.6f;
        c.fovMul = 1.0f - 0.22f * (float)std::min(1.0, killT_ / 0.09);
        if (killRole_ == (int)Presence::Role::Jerry) c.fovMul *= 0.4f + 0.6f * (float)std::exp(-killT_ * 1.5);  // V13.3 Teil 4: der Blick versinkt in seinen Augen
        return c;
    }
    c.yaw = (float)(player_.angle + player_.sway());
    c.pitch = (float)player_.pitch;
    c.roll = body_.roll();
    c.fovMul = 1.0f + body_.fovKick();
    if (!thirdPerson) {
        c.pos = body_.eye();
        if (const float q = presence_.quake(); q > 0.01f) {  // V13.3: das Beben (Wrangler) ruettelt die Kamera
            const float t = (float)stats_.playTime;
            c.pos += vec3{std::sin(t * 23.1f), std::sin(t * 19.7f + 1.0f), std::sin(t * 29.3f + 2.0f)} * (0.011f * q);
            c.roll += std::sin(t * 17.3f) * 0.007f * q;
            c.pitch += std::sin(t * 21.7f + 0.4f) * 0.005f * q;
        }
        if (ride_.active && ride_.travel >= 0.0) {  // V8: die Kabine ruckelt beim Anfahren und Halten
            const double t = ride_.travel;
            const double env = std::exp(-t * 2.5) + std::exp(-std::max(0.0, 4.6 - t) * 3.0) * (t > 3.0 ? 1.0 : 0.0) + 0.25;
            c.pos.z += (float)(std::sin(t * 31.0) * 0.004 * env + std::sin(t * 9.0) * 0.002);
            c.roll += (float)(std::sin(t * 13.0) * 0.0025 * env);
            if (ride_.escape) {  // V9: die letzte Fahrt - immer schneller, immer heller
                const double k = std::min(1.0, t / 10.0);
                c.pos.z += (float)(std::sin(t * 41.0) * 0.007 * k + std::sin(t * 17.0) * 0.004 * k);
                c.roll += (float)(std::sin(t * 7.3) * 0.006 * k * k);
                c.fovMul *= 1.0f + 0.07f * (float)(k * k);
            }
        }
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
    if (player_.underwater) {  // V11: unter Wasser - dichter, blau-gruener Nebel, sofort
        fog_ = {0.02f, 0.07f, 0.075f};
        fogDensity_ = 0.35;
        return;
    }
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

Surface GameSession::surfaceUnder() const { return surfaceAt(player_.x, player_.y); }

Surface GameSession::surfaceAt(double x, double y) const {
    const world::Tile* t = world_->tileIfLoaded(ifloor(x), ifloor(y));
    if (!t || t->solid()) return Surface::Concrete;
    // V11: Pfuetzen der Ausstattung (etwa 1 m Halbmesser)
    if (const world::Sector* sec = world_->sectorIfReady(x, y))
        for (const world::Sector::Spot& sp : sec->spots)
            if (sp.kind == "puddle" && (sp.x - x) * (sp.x - x) + (sp.y - y) * (sp.y - y) < 1.0 && std::fabs(sp.z - t->floor) < 0.3)
                return Surface::Puddle;
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
        case world::PlanePattern::Planks: return Surface::Wood;  // V11
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
    if (ecs::Entity cab = cabinAt(p.x, p.y); cab != ecs::kNull) {  // V8: in der Kabine: das Tastenfeld
        target_ = cab;
        return;
    }
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
    if (target_ == ecs::kNull) return speakTarget() ? "Ansprechen" : "";  // V13.3 (KREATUR_SkinStealer.md)
    auto& reg = const_cast<ecs::Registry&>(reg_);
    auto* pk = reg.get<Pickup>(target_);
    auto* in = reg.get<Interactable>(target_);
    // V6: ohne Tastenangabe - die Oberflaeche setzt die tatsaechliche Belegung davor
    if (pk) {
        const ItemDef* d = content_.item(pk->item);
        return std::format("{} {}", d ? d->name : pk->item, in ? in->prompt : "aufnehmen");
    }
    if (auto* el = reg.get<Elevator>(target_)) {  // V8
        if (ride_.active) return "";
        auto* t = reg.get<Transform>(target_);
        if (t && inCabin(t->pos, t->yaw, player_.x, player_.y)) {
            if (!fuseKinds().empty() && fusesHeld() >= (int)fuseKinds().size()) return "Sicherungen einsetzen";  // V9
            return elevLinks_.count(el->id) ? "Knopf drücken  (zurück)" : "Knopf drücken";
        }
        return el->open > 0.5f ? "" : "Aufzug rufen";
    }
    return in ? in->prompt : "";
}

void GameSession::interact() {
    if (target_ == ecs::kNull) {
        if (speakTarget()) speak();  // V13.3: niemand zum Aufnehmen im Blick, aber eine Gestalt - ansprechen
        return;
    }
    if (netAction) {  // V13.2 Koop (Gast): Aufnehmen und Lesen entscheidet der Host
        if (const u32 h = targetHash()) {
            netAction(1, h);
            if (auto* t = reg_.get<Transform>(target_)) body_.gesture(Body::Gesture::Grab, t->pos, "", 0.0f, 1.0f);
        }
        return;
    }
    // V8: Aufzug rufen bzw. in der Kabine losfahren
    if (auto* el = reg_.get<Elevator>(target_)) {
        auto* t = reg_.get<Transform>(target_);
        if (!t || ride_.active) return;
        if (inCabin(t->pos, t->yaw, player_.x, player_.y)) {
            // V9: mit allen drei Sicherungen geht es hinaus
            if (fuseKinds().empty() || fusesHeld() < (int)fuseKinds().size() || !startEscape(target_)) startRide(target_);
        } else if (el->open < 0.5f) {
            el->openTarget = 1.0f;
            el->hold = 8.0f;
            GameEvent ev{GameEvent::SoundAt};
            ev.sound = "elev_ding", ev.pos = t->pos + vec3{0, 0, 2.4f}, ev.intensity = 0.7f;
            events_.push_back(ev);
            ev.sound = "elev_doors", ev.pos = t->pos + vec3{0, 0, 1.2f}, ev.intensity = 0.8f;
            events_.push_back(ev);
        }
        return;
    }
    // V9: Sicherung herausnehmen
    if (auto* fb = reg_.get<FuseBox>(target_)) {
        auto* t = reg_.get<Transform>(target_);
        if (fb->taken || !t) return;
        const ItemDef* d = content_.item(fb->item);
        if (hasFuse(fb->item)) {
            events_.push_back({GameEvent::Toast, "Du hast schon eine Sicherung aus dieser Ebene.", 2.6f});
            return;
        }
        if (!inventory_.add(fb->item)) {
            events_.push_back({GameEvent::InventoryFull, "Inventar voll", 2.2f});
            return;
        }
        picked.insert(fb->id);
        fb->taken = true;
        ++runLog_.fuses;
        if (auto* m = reg_.get<MeshRenderer>(target_)) m->model = "fusebox_empty", m->meshId = -2;
        reg_.remove<Interactable>(target_);
        reg_.remove<AudioEmitter>(target_);
        const int n = fusesHeld(), all = (int)fuseKinds().size();
        GameEvent ev{GameEvent::FuseTaken,
                     n >= all ? std::format("{} – alle {} Sicherungen. Jetzt einen Aufzug finden.", d ? d->name : fb->item, all)
                              : std::format("{} – {} von {}", d ? d->name : fb->item, n, all),
                     4.5f};
        ev.pos = t->pos + vec3{std::cos(t->yaw) * 0.12f, std::sin(t->yaw) * 0.12f, 1.42f};
        ev.index = n;
        events_.push_back(ev);
        body_.gesture(Body::Gesture::Grab, ev.pos, "fuse", d ? (float)d->holdOffset : 0.0f, 1.0f);
        log::info("Sicherung {} genommen ({} von {})", fb->item, n, all);
        target_ = ecs::kNull;
        return;
    }
    // V7: Notiz lesen - sie wandert ins Notizbuch
    if (auto* nf = reg_.get<NoteFind>(target_)) {
        picked.insert(nf->id);
        if (std::find(journal_.begin(), journal_.end(), nf->index) == journal_.end()) journal_.push_back(nf->index);
        GameEvent ev{GameEvent::NoteRead};
        ev.index = nf->index;
        events_.push_back(ev);
        if (auto* t = reg_.get<Transform>(target_)) body_.gesture(Body::Gesture::Grab, t->pos, "note", 0.0f, 1.0f);
        reg_.destroy(target_);
        target_ = ecs::kNull;
        return;
    }
    // V7: Notausgang - die Anwendung wechselt die Ebene
    if (reg_.get<ExitDoor>(target_)) {
        GameEvent ev{GameEvent::ExitOpened};
        ev.text = world_->level().nextLevel;
        if (auto* t = reg_.get<Transform>(target_)) ev.pos = t->pos;
        events_.push_back(ev);
        return;
    }
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
    // V13.3: das Beil wird nicht verbraucht - es schwingt (Plague Goblins: verscheuchen, ein Treffer toetet)
    const bool hatchet = idx >= 0 && idx < Inventory::kSize && inventory_.slots[(size_t)idx] && inventory_.slots[(size_t)idx]->kind == "hatchet";
    if (hatchet && !body_.gestureActive()) {
        const ItemDef* hd = content_.item("hatchet");
        body_.gesture(Body::Gesture::Swing, {}, hd ? hd->model : "hatchet", hd ? (float)hd->holdOffset : 0.0f, hd ? (float)hd->scale : 1.0f);
        GameEvent ev{GameEvent::Consume, "", 0.0f};
        ev.sound = "hatchet_swing";
        events_.push_back(ev);
        if (netAction) netAction(2, (u32)idx);  // Koop (Gast): der Host rechnet den Schwung
        else if (hatchetSwing(player_, body_.eye(), netSelfId) == 2) {
            GameEvent hit{GameEvent::Consume, "", 0.0f};
            hit.sound = "hatchet_hit";
            events_.push_back(hit);
        }
        return;
    }
    if (hatchet) return;
    if (startGlowThrow(idx)) return;  // V13.3: Leuchtstab knicken und werfen (session_lure.cpp; Gast: Eingabe beim Loslassen)
    if (netAction) {  // V13.2 Koop (Gast)
        if (idx >= 0 && idx < Inventory::kSize && inventory_.slots[(size_t)idx]) {
            std::vector<Phenomena::Event> pe;  // V13.3: eigenes Bild und Klang (die Werte rechnet der Host)
            const std::string extra = phenomena_.drink(0, inventory_.slots[(size_t)idx]->kind, pe);
            if (!extra.empty()) events_.push_back({GameEvent::Toast, extra, 3.0f});
            netAction(2, (u32)idx);
        }
        return;
    }
    if (idx >= 0 && idx < Inventory::kSize && inventory_.slots[(size_t)idx] && isFuse(inventory_.slots[(size_t)idx]->kind)) {
        events_.push_back({GameEvent::Toast, "Eine Sicherung. Sie gehört in eine Fassung – unter dem Tastenfeld eines Aufzugs.", 3.2f});
        return;
    }
    auto kind = inventory_.take(idx);
    if (!kind) {
        events_.push_back({GameEvent::Toast, "Kein Item auf diesem Platz", 1.5f});
        return;
    }
    const ItemDef* d = content_.item(*kind);
    if (!d) return;
    if (d->battery > 0.0) {
        // V7: Batterie wechseln - laedt die Taschenlampe
        flash_.addCharge(d->battery);
        GameEvent ev{GameEvent::Consume, std::format("{}: Taschenlampe {} %", d->name, (int)std::lround(flash_.charge)), 2.6f};
        ev.sound = d->useSound;
        events_.push_back(ev);
        return;
    }
    GameEvent ev{GameEvent::Consume, stats_.consume(*d), 3.0f};
    std::vector<Phenomena::Event> pe;  // V13.3: offenes Mandelwasser steckt an, versiegeltes heilt frueh (KREATUR_Crawler.md)
    if (const std::string extra = phenomena_.drink(0, d->key, pe); !extra.empty()) ev.text += " · " + extra, ev.duration = 4.0f;
    ev.sound = d->useSound;
    events_.push_back(ev);
    // die Hand fuehrt das Item zum Mund
    body_.gesture(Body::Gesture::Use, {}, d->model, (float)d->holdOffset, (float)d->scale);
}

// V13: Ein- und Ausschalten mit der Hand: zum Einschalten hebt der Arm die Lampe, der Daumen drueckt den Schalter,
// erst dann geht das Licht an (Klick und Licht zugleich). Zum Ausschalten drueckt der Daumen zuerst, das Licht geht
// aus, danach sinkt der Arm. Nochmal druecken waehrend des Hebens bricht ab.
void GameSession::toggleFlashlight() {
    if (flashPending_ == -1 || flashPending_ == 2) return;  // der Daumen drueckt gerade
    if (flashPending_ == 1) {
        flashPending_ = 0, flashRaise_ = false;
        return;
    }
    if (flash_.on) {
        flashPending_ = -1;
        body_.pressSwitch();
        flashClickT_ = 0.07;
        return;
    }
    flashRaise_ = true, flashPending_ = 1, flashLowerT_ = -1.0;
}

void GameSession::flashClick() {
    const int what = flashPending_;
    flashPending_ = 0;
    if (what == -1) {
        flash_.on = false;
        if (netAction) netAction(5, 0);  // V13.2 Koop (Gast): der Host schaltet mit
        GameEvent ev{GameEvent::FlashToggle};
        ev.intensity = 0.0f;
        events_.push_back(ev);
        flashLowerT_ = 0.12;
        return;
    }
    if (what != 2) return;
    if (!world_->level().flashlight) {  // V11: Level 6 - hier gibt die Lampe kein Licht
        events_.push_back({GameEvent::Toast, "Die Lampe klickt. Kein Licht.", 2.4f});
        events_.push_back({GameEvent::FlashEmpty, "", 0.0f});
        flashLowerT_ = 0.5;
        return;
    }
    if (!flash_.toggle()) {
        if (netAction) netAction(5, 0);
        events_.push_back({GameEvent::Toast, "Die Batterie ist leer – vielleicht liegt irgendwo eine.", 2.8f});
        events_.push_back({GameEvent::FlashEmpty, "", 0.0f});
        flashLowerT_ = 0.5;
        return;
    }
    if (netAction) netAction(5, flash_.on ? 1u : 0u);
    GameEvent ev{GameEvent::FlashToggle};
    ev.intensity = flash_.on ? 1.0f : 0.0f;
    events_.push_back(ev);
}

FlashBeam GameSession::flashBeam() const {
    FlashBeam b;
    b.power = flash_.output(stats_.playTime);
    b.on = flash_.on && b.power > 0.01f && body_.flashlightHeld();
    b.pos = body_.flashlightPos();
    b.dir = body_.flashlightDir();
    return b;
}

void GameSession::selectSlot(int idx) {
    if (netAction && idx >= 0 && idx < Inventory::kSize) netAction(6, (u32)idx);  // V13.2 Koop: der Host merkt sich die Wahl
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
    palette_.clear();  // V10: Knochen dieses Bildes
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
    lures_.collect(out, eye);  // V13.3: Leuchtstaebe
    phenomena_.collect(out, eye);  // V13.3: Gemaelde am Boden (KREATUR_GameMaster.md)
    haunt_.collect(out);  // V7: "Der Andere"
    presence_.collect(out, palette_, eye, figureDetail);  // V10: die Figuren
    for (Pawn* p : pawns)  // V13.2 Koop: die Mitspieler (Spielermodell, Lampe von aussen)
        if (p) p->collect(out, palette_, eye, figureDetail);
    // V8: Aufzuege - Rahmen und Kabine, zwei Schiebefluegel, Etagenanzeige
    reg_.each<Elevator, Transform>([&](ecs::Entity, Elevator& el, Transform& t) {
        vec3 d = t.pos - eye;
        if (dot(d, d) > 70.0f * 70.0f) return;
        const mat4 m = mat4::translation(t.pos) * mat4::rotationZ(t.yaw);
        const float slide = 0.98f * el.open;
        auto add = [&](std::string* model, int* id, const mat4& w) {
            VisibleMesh v;
            v.model = model;
            v.meshId = id;
            v.world = w;
            out.push_back(v);
        };
        add(&el.mFrame, &el.idFrame, m);
        add(&el.mDoor, &el.idDoorA, m * mat4::translation(vec3{0.0f, -0.5f - slide, 0.0f}));
        add(&el.mDoor, &el.idDoorB, m * mat4::translation(vec3{0.0f, 0.5f + slide, 0.0f}));
        add(&el.mDisplay, &el.idDisplay, m);
    });
}

void GameSession::collectSounds(std::vector<SoundSource>& out) {
    reg_.each<AudioEmitter, Transform>([&](ecs::Entity e, AudioEmitter& a, Transform& t) {
        out.push_back({e, &a.sound, t.pos, a.volume, a.radius, &a.voice});
    });
    collectPurpleSounds(out);  // V13.3: Brummen der purpifizierten Wesen (session_purple.cpp)
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
    j.set("phenomena", phenomena_.toJson(0));  // V13.3
    j.set("inventory", inventory_.toJson());
    j.set("selected", inventory_.selected);
    std::vector<std::string> ids(picked.begin(), picked.end());
    std::sort(ids.begin(), ids.end());
    Json pk = Json::array();
    for (auto& s : ids) pk.push(s);
    j.set("picked", std::move(pk));
    // V11: Lauf-Seed und Gedaechtnis der anderen Ebenen
    j.set("run_seed", (long long)runSeedOr());
    Json lm = Json::object();
    for (const auto& [lid, mem] : levelMemory) {
        if (lid == levelId) continue;
        Json e = Json::object(), pa = Json::array(), no = Json::object();
        for (const auto& id : mem.picked) pa.push(id);
        for (const auto& [nid, idx] : mem.notes) no.set(nid, idx);
        e.set("picked", std::move(pa));
        e.set("notes", std::move(no));
        lm.set(lid, std::move(e));
    }
    j.set("level_memory", std::move(lm));
    Json mn = Json::object();
    for (const auto& [nid, idx] : mapNotes_) mn.set(nid, idx);
    j.set("map_notes", std::move(mn));
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
    j.set("flashlight", flash_.toJson());  // V7
    Json jn = Json::array();
    for (int i : journal_) jn.push(i);
    j.set("journal", std::move(jn));
    Json ke = Json::array();
    for (const auto& k : knownExits_) {
        Json e = Json::object();
        e.set("id", k.id);
        e.set("x", (double)k.pos.x);
        e.set("y", (double)k.pos.y);
        e.set("z", (double)k.pos.z);
        e.set("yaw", (double)k.yaw);
        ke.push(std::move(e));
    }
    j.set("exits_seen", std::move(ke));
    j.set("runlog", runLog_.toJson());
    // V8: gefahrene Aufzugsverbindungen (damit jeder Aufzug wieder zurueckfaehrt)
    Json el = Json::array();
    for (const auto& [from, l] : elevLinks_) {
        Json e = Json::object();
        e.set("from", from);
        e.set("to", l.to);
        e.set("x", (double)l.door.x);
        e.set("y", (double)l.door.y);
        e.set("z", (double)l.door.z);
        e.set("dx", (double)l.dir.x);
        e.set("dy", (double)l.dir.y);
        e.set("layer", l.layer);
        el.push(std::move(e));
    }
    j.set("elevators", std::move(el));
    // V9: gesehene Sicherungskaesten, Hinweis in der Kabine
    Json kf = Json::array();
    for (const auto& k : knownFuses_) {
        Json e = Json::object();
        e.set("id", k.id);
        e.set("item", k.item);
        e.set("x", (double)k.pos.x);
        e.set("y", (double)k.pos.y);
        e.set("z", (double)k.pos.z);
        e.set("yaw", (double)k.yaw);
        kf.push(std::move(e));
    }
    j.set("fuses_seen", std::move(kf));
    j.set("cabin_hint", cabinHinted_);
    j.set("checkpoint", checkpoint_);  // V11
    j.set("checkpoint_label", checkpointLabel_);
    return j;
}

void GameSession::fromJson(const Json& j) {
    checkpoint_ = j["checkpoint"].asString(checkpoint_);  // V11
    checkpointLabel_ = j["checkpoint_label"].asString(checkpointLabel_);
    const Json& p = j["player"];
    player_.x = p["x"].asNumber(player_.x);
    player_.y = p["y"].asNumber(player_.y);
    player_.z = p["z"].asNumber(player_.z);
    player_.angle = p["angle"].asNumber(0.0);
    // V4 speicherte den Blickwinkel begrenzt auf +-0.9 rad - passt in den V5-Bereich
    player_.pitch = std::clamp(p["pitch"].asNumber(0.0), -Player::kPitchMax, Player::kPitchMax);
    player_.distance = p["distance"].asNumber(0.0);
    stats_.fromJson(j["stats"]);
    phenomena_.fromJson(0, j["phenomena"]);  // V13.3
    inventory_.fromJson(j["inventory"]);
    inventory_.selected = (int)(j["selected"].asInt(0) % Inventory::kHotbar);
    picked.clear();
    for (const auto& id : j["picked"].items()) picked.insert(id.asString());
    runSeed = (i64)j["run_seed"].asInt(0);  // V11
    if (const Json* lm = j.find("level_memory"); lm && lm->isObject())
        for (const auto& [lid, e] : lm->members()) {
            LevelMemory mem;
            for (const auto& id : e["picked"].items()) mem.picked.insert(id.asString());
            if (const Json* no = e.find("notes"); no && no->isObject())
                for (const auto& [nid, idx] : no->members()) mem.notes[nid] = (int)idx.asInt(-1);
            levelMemory[lid] = std::move(mem);
        }
    if (const Json* mn = j.find("map_notes"); mn && mn->isObject())
        for (const auto& [nid, idx] : mn->members()) mapNotes_[nid] = (int)idx.asInt(-1);
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
    flash_.fromJson(j["flashlight"]);  // V7 (aeltere Staende: Lampe mit Startladung)
    flashRaise_ = flash_.on, flashPending_ = 0, flashClickT_ = flashLowerT_ = -1.0;  // V13
    journal_.clear();
    for (const auto& n : j["journal"].items())
        if (n.asInt(-1) >= 0 && n.asInt(-1) < (long long)content_.notes().size()) journal_.push_back((int)n.asInt());
    knownExits_.clear();
    for (const auto& e : j["exits_seen"].items())
        knownExits_.push_back({e["id"].asString(""), {e["x"].asFloat(), e["y"].asFloat(), e["z"].asFloat()}, e["yaw"].asFloat()});
    // V7: Laufbuch (aeltere Staende: beginnt hier mit der aktuellen Ebene)
    runLog_.fromJson(j["runlog"]);
    elevLinks_.clear();  // V8
    for (const auto& e : j["elevators"].items())
        elevLinks_[e["from"].asString("")] = {e["to"].asString(""), {e["x"].asFloat(), e["y"].asFloat(), e["z"].asFloat()},
                                              {e["dx"].asFloat(), e["dy"].asFloat()}, (int)e["layer"].asInt(0)};
    ride_ = {};
    if (runLog_.levels.empty()) runLog_.levels.push_back(levelId);
    // V9
    knownFuses_.clear();
    for (const auto& e : j["fuses_seen"].items())
        knownFuses_.push_back({e["id"].asString(""), e["item"].asString(""), {e["x"].asFloat(), e["y"].asFloat(), e["z"].asFloat()},
                               e["yaw"].asFloat()});
    cabinHinted_ = j["cabin_hint"].asBool(false);
    cabinFuses_ = -1;
    presence_.clear();
    presence_.setGrace(90.0);  // nach dem Laden: erst einmal Ruhe
    deathCause_.clear();
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
    deathCause_.clear();
}

// --- V10: Entities (Praesenz) --------------------------------------------------------------------
void GameSession::updatePresence(double dt) {
    presenceEvents_.clear();
    Presence::Context c;
    c.player = &player_;
    c.view.eye = body_.eye();
    c.view.look = forwardFromAngles((float)player_.angle, (float)player_.pitch);
    const FlashBeam fb = flashBeam();
    c.view.flashOn = fb.on && fb.power > 0.15f;
    c.view.flashPos = fb.pos;
    c.view.flashDir = fb.dir;
    c.view.blackoutSeed = haunt_.blackout().seed;
    c.view.blackoutAmount = haunt_.blackout().amount;
    c.view.blackoutOrigin = haunt_.blackout().origin;  // V11
    c.view.blackoutFront = haunt_.blackout().front;
    c.view.fog = (float)fogDensity_;
    c.world = world_.get();
    c.sanity = stats_.sanity;
    // V11: feste Karten (Level 1 ...) sagen selbst, welche Regie gilt ("presence_region")
    const world::LevelDef& L = world_->level();
    c.region = L.world() ? currentRegion() : L.presenceRegion;
    c.mode = L.world() || L.mapped() ? figureMode : 0;
    c.allowHound = L.allowHound, c.allowSmiler = L.allowSmiler, c.allowGestalt = L.allowGestalt, c.allowBlackout = L.allowBlackout;
    // V11: Buendel - Lager aus der Karte (einmal je Ebene), Laerm des Spielers (ENTITY_BEHAVIOR 1)
    c.allowClump = L.allowClump, c.clumpRate = L.clumpRate;
    c.kin = L.kin.data();  // V13.2: neue Wesen der Ebene
    if (const auto& sl = inventory_.slots[(size_t)inventory_.selected]; sl && sl->kind == "almond") c.almond = true;  // V13.3: Duller
    if (L.allowClump && !lairsRead_ && L.mapped())
        if (const world::Sector* sec = world_->sectorIfReady(player_.x, player_.y)) {
            lairsRead_ = true;
            clumpLairs_.clear();
            for (const auto& e : sec->entities)
                if (e.item == "clump") clumpLairs_.push_back({e.x, e.y, e.z});
        }
    c.lairs = &clumpLairs_;
    double noise = 0.0;
    const bool moving = player_.groundSpeed > 0.4 || player_.swimming;
    if (moving) noise = player_.sprinting ? 1.0 : (player_.sneaking ? 0.15 : 0.45);
    if (moving && (player_.wading || player_.swimming || surfaceUnder() == Surface::Puddle)) noise = std::min(1.0, noise + 0.4);
    if (player_.landed) landNoise_ = std::max(landNoise_, 0.8 * std::clamp(player_.fallSpeed / 5.0, 0.4, 1.0));
    landNoise_ = std::max(0.0, landNoise_ - dt * 2.5);
    c.noise = std::max(noise, landNoise_);
    c.pings = &noisePings_;
    c.lures = &presenceLures_, c.smilerBait = L.smilerBait;  // V13.3: Leuchtkoeder (KREATUR_Smiler.md)
    c.houndPack = L.houndPack.on ? &L.houndPack : nullptr;  // V13.3 Level 2/3: Hound-Rudel, grosses Buendel
    c.clumpBig = L.clumpBig.on ? &L.clumpBig : nullptr;
    if (const Json& bo = L.audio["blackout"]; bo.isObject()) {
        c.blackoutEvery0 = bo["every"][0].asNumber(200.0), c.blackoutEvery1 = bo["every"][1].asNumber(340.0);
        c.blackoutLen0 = bo["duration"][0].asNumber(20.0), c.blackoutLen1 = bo["duration"][1].asNumber(32.0);
    }
    c.hallucinations = hallucinations;
    c.suspended = ride_.active || stats_.dead;
    c.fusesHeld = fusesHeld();
    c.room = room_;
    c.canBlackout = room_ && !room_->fixtures().empty() && room_->avgLight > 0.2;
    const DifficultyDef& d = content_.difficulty(difficulty);
    c.stalkerSpeed = d.stalkerSpeed;
    c.stalkerPause = d.stalkerPause;
    // V7: die Gestalt im (halluzinierten) Stromausfall zeigt jetzt das Praesenzsystem
    if (haunt_.takeGhostRequest() && hallucinations) presence_.force(Presence::Role::Gestalt);
    if (netRole != NetRole::Solo) updatePawnsPresence(c);  // V13.2 Koop
    goblinContext(c);                                      // V13.3: Plague Goblins (Vorraete, Beil, Liegendes, Gesundheit)
    if (netRole == NetRole::Guest && netCreaturesFresh) {
        presence_.netApply(netCreatures, presence_.netTime());
        netCreaturesFresh = false;
    }
    presence_.update(dt, c, presenceEvents_);
    // V13.3: Growler senden WLAN und laden Geraete auf (die Seite) - nah an einem laedt sich die Lampe (das Rauschen zehrt dafuer)
    if (presence_.staticLevel() > 0.25f && !stats_.dead) flash_.addCharge(dt * 3.0 * presence_.staticLevel());
    noisePings_.clear();
    if (netRole == NetRole::Guest) {  // Werte rechnet der Host; hier nur Klang und Bild der Puppen
        for (const Presence::Event& e : presenceEvents_) {
            GameEvent ev;
            ev.type = GameEvent::Figure;
            ev.index = (int)e.type;
            ev.role = (int)e.role;
            ev.pos = e.pos;
            ev.intensity = e.strength;
            if (e.type == Presence::Event::Step) ev.surface = surfaceAt(e.pos.x, e.pos.y);
            events_.push_back(ev);
        }
        return;
    }
    // V13.2 Koop (Host): was die Wesen den Gaesten antun, und ihre Dauerwirkung auf deren Verstand
    for (Pawn* p : pawns) {
        if (!p || !p->alive) continue;
        if (presence_.seenRoleBy(Presence::Role::Smiler, p->id)) p->stats.sanity -= dt * 0.5;
        if (presence_.seenRoleBy(Presence::Role::Gestalt, p->id)) p->stats.sanity -= dt * 0.2;
        p->stats.sanity = std::max(0.0, p->stats.sanity);
    }
    // in das Laecheln zu starren, die Naehe eines jagenden Hounds, die Gestalt - das zehrt am Verstand
    if (presence_.seenRoleBy(Presence::Role::Smiler, netSelfId)) stats_.sanity -= dt * 0.5;
    // V11: Raumtypen zehren am Verstand (rote Raeume) oder geben ihn zurueck (Glut im Dachboden); Hitze kostet Ausdauer
    if (room_ && room_->type && room_->type->sanityDrain != 0.0)
        stats_.sanity = std::clamp(stats_.sanity - dt * room_->type->sanityDrain, 0.0, 100.0);
    if (room_ && room_->type && room_->type->staminaDrain > 0.0) stats_.spendStamina(dt * room_->type->staminaDrain);
    if (presence_.seenRoleBy(Presence::Role::Gestalt, netSelfId)) stats_.sanity -= dt * 0.2;
    if (presence_.hunting()) stats_.sanity -= dt * 0.3 * presence_.threatProximity();
    stats_.sanity = std::max(0.0, stats_.sanity);
    using PE = Presence::Event;
    for (const PE& e : presenceEvents_) {
        goblinWorld(e);  // V13.3: Plague Goblins - Diebstahl, Aufheben, Verlorenes, Geschenk
        if (netRole == NetRole::Host && e.target != netSelfId) {  // V13.2 Koop: betrifft einen Gast
            netFigureOut.push_back(e);
            Pawn* p = nullptr;
            for (Pawn* q : pawns) p = q && q->id == e.target ? q : p;
            if (p && p->alive) {
                switch (e.type) {
                    case PE::FirstSight: p->stats.sanity = std::max(0.0, p->stats.sanity - (e.role == Presence::Role::Smiler || e.role == Presence::Role::Hound ? 4.0 : 1.0)); break;
                    case PE::Stare:
                    case PE::Vision: p->stats.sanity = std::max(0.0, p->stats.sanity - 3.0); break;
                    case PE::Attack:  // toedlich wie beim eigenen Spieler (ENTITY_BEHAVIOR.md F4)
                        p->stats.health = 0.0, p->stats.dead = true, p->killRole = (int)e.role;
                        ++runLog_.caught;
                        break;
                    case PE::Sting:
                        p->stats.health = std::max(0.0, p->stats.health - 5.0);
                        p->stats.sanity = std::max(0.0, p->stats.sanity - 1.0);
                        if (p->stats.health <= 0.0) p->stats.dead = true, p->killRole = (int)e.role;
                        break;
                    case PE::Hit:  // V13.2: Schlag, Hieb, Biss
                        p->stats.health = std::max(0.0, p->stats.health - (double)e.strength * hitScale_);
                        p->stats.sanity = std::max(0.0, p->stats.sanity - 2.0);
                        if (p->stats.health <= 0.0) p->stats.dead = true, p->killRole = (int)e.role;
                        break;
                    case PE::Whisper: p->stats.sanity = std::max(0.0, p->stats.sanity - 0.4 * e.strength); break;
                    case PE::Spell: p->stats.sanity = std::max(0.0, p->stats.sanity - 2.0); break;  // V13.3 Teil 4: Jerrys Bann
                    case PE::Static:  // V13.3: das Rauschen eines Growlers aus der Naehe
                        p->stats.health = std::max(0.0, p->stats.health - (double)e.strength * hitScale_);
                        p->stats.sanity = std::max(0.0, p->stats.sanity - 0.5);
                        if (p->stats.health <= 0.0) p->stats.dead = true, p->killRole = (int)e.role;
                        break;
                    default: break;
                }
            }
            if (e.type == PE::Blackout) {  // der Strom faellt im Raum des Gasts aus - fuer alle
                haunt_.powerCut(world_->roomAt(e.pos.x, e.pos.y), e.strength, e.pos);
                netBlackoutOut.push_back({e.pos, e.strength});
                continue;
            }
            // was man im Raum hoert, hoert auch der Host (Schritte, Knurren, Flattern ...)
            const bool audible = e.type == PE::Step || e.type == PE::Growl || e.type == PE::Bark || e.type == PE::Whine ||
                                 e.type == PE::TakeOff || e.type == PE::Knock || e.type == PE::Drop || e.type == PE::Drag ||
                                 e.type == PE::Flicker || e.type == PE::Breath || e.type == PE::Mimic || e.type == PE::Plead ||
                                 e.type == PE::Gurgle || e.type == PE::Hum || e.type == PE::Squeak || e.type == PE::Scream ||
                                 e.type == PE::Swing || e.type == PE::Pull || e.type == PE::Whisper ||  // V13.2
                                 e.type == PE::Scrape || e.type == PE::Bulge || e.type == PE::Burst || e.type == PE::Burn || e.type == PE::Retract ||
                                 (e.type == PE::Windup && e.role == Presence::Role::Arms) ||  // V13.3
                                 e.type == PE::Click || e.type == PE::Lurch || e.type == PE::Merge || e.type == PE::Collapse ||
                                 e.type == PE::Snip || e.type == PE::SacBurst || e.type == PE::Skitter || e.type == PE::Stridulate ||  // V13.3
                                 e.type == PE::Croak || e.type == PE::Steal || e.type == PE::Take || e.type == PE::Hiss ||
                                 e.type == PE::Screech || e.type == PE::Portal || e.type == PE::Perish || e.type == PE::Mimic ||  // V13.3 Plague Goblins
                                 e.type == PE::Reply || e.type == PE::Shriek ||                // V13.3: Skin-Stealer (Ansprechen)
                                 (e.role == Presence::Role::Jerry && (e.type == PE::Speak || e.type == PE::Flap || e.type == PE::Preen ||
                                                                      e.type == PE::Announce));  // V13.3 Teil 4: Jerry
            if (!audible) continue;
            if (e.type == PE::Flicker)
                if (const world::Tile* t = world_->tileIfLoaded(ifloor(e.pos.x), ifloor(e.pos.y)); t && t->room) haunt_.flicker(t->room);
            GameEvent ev;
            ev.type = GameEvent::Figure;
            ev.index = (int)e.type;
            ev.role = (int)e.role;
            ev.pos = e.pos;
            ev.intensity = e.strength;
            if (e.type == PE::Step) ev.surface = surfaceAt(e.pos.x, e.pos.y);
            events_.push_back(ev);
            continue;
        }
        if (netRole == NetRole::Host && (e.type == PE::Step || e.type == PE::Growl || e.type == PE::Bark || e.type == PE::TakeOff ||
                                         e.type == PE::Knock || e.type == PE::Drop || e.type == PE::Drag || e.type == PE::Flicker ||
                                         e.type == PE::Mimic || e.type == PE::Plead || e.type == PE::Gurgle || e.type == PE::Hum ||
                                         e.type == PE::Squeak || e.type == PE::Scream || e.type == PE::Swing || e.type == PE::Pull ||
                                         e.type == PE::Scrape || e.type == PE::Bulge || e.type == PE::Burst || e.type == PE::Burn ||
                                         e.type == PE::Retract || (e.type == PE::Windup && e.role == Presence::Role::Arms) ||  // V13.3
                                         e.type == PE::Click || e.type == PE::Lurch || e.type == PE::Merge || e.type == PE::Collapse ||  // V13.3
                                         e.type == PE::Snip || e.type == PE::SacBurst || e.type == PE::Skitter || e.type == PE::Stridulate ||
                                         e.type == PE::Croak || e.type == PE::Take || e.type == PE::Hiss || e.type == PE::Screech ||
                                         e.type == PE::Portal || e.type == PE::Perish ||  // V13.3 Plague Goblins
                                         e.type == PE::Reply || e.type == PE::Shriek ||
                                         (e.role == Presence::Role::Jerry && (e.type == PE::Speak || e.type == PE::Flap || e.type == PE::Preen ||
                                                                              e.type == PE::Announce))))  // V13.3 Teil 4: Jerry
            netFigureOut.push_back(e);  // auch die Gaeste hoeren es
        GameEvent ev;
        ev.type = GameEvent::Figure;
        ev.index = (int)e.type;
        ev.role = (int)e.role;
        ev.pos = e.pos;
        ev.intensity = e.strength;
        const bool threat = e.role == Presence::Role::Smiler || e.role == Presence::Role::Hound;
        switch (e.type) {
            case PE::Step: ev.surface = surfaceAt(e.pos.x, e.pos.y); break;
            case PE::FirstSight:
                if (threat) ++runLog_.encounters, ev.type = GameEvent::StalkerSeen;
                stats_.sanity = std::max(0.0, stats_.sanity - (threat ? 4.0 : 1.0));
                break;
            case PE::Stare: stats_.sanity = std::max(0.0, stats_.sanity - 3.0); break;
            case PE::Attack:
                // V11 (ENTITY_BEHAVIOR.md F4, Entscheidung 2026-10-25): der Zugriff ist toedlich. Ausgleich: die
                // Ankuendigung (F2), erfahrbare Regeln (F5) und der Wiedereinstieg am letzten Sicherungspunkt
                ev.type = GameEvent::StalkerHit;
                ++runLog_.caught;
                (void)d;
                stats_.health = 0.0;
                stats_.dead = true;
                deathCause_ = "stalker";
                killRole_ = (int)e.role;
                killCam_ = true, killT_ = 0.0;
                killYaw0_ = (float)player_.angle, killPitch0_ = (float)player_.pitch;
                ev.index = runLog_.caught;
                break;
            case PE::Sting:
                stats_.health = std::max(0.0, stats_.health - 5.0);
                stats_.sanity = std::max(0.0, stats_.sanity - 1.0);
                if (stats_.health <= 0.0) {
                    stats_.dead = true;
                    deathCause_ = "stalker";
                    killRole_ = (int)e.role;
                }
                break;
            case PE::Blackout:
                haunt_.powerCut(room_, e.strength, {(float)player_.x, (float)player_.y, (float)player_.z});
                if (netRole == NetRole::Host) netBlackoutOut.push_back({{(float)player_.x, (float)player_.y, (float)player_.z}, e.strength});
                break;
            case PE::Flicker:
                if (const world::Tile* t = world_->tileIfLoaded(ifloor(e.pos.x), ifloor(e.pos.y)); t && t->room) haunt_.flicker(t->room);
                break;
            case PE::Vision:
                ++runLog_.visions;
                stats_.sanity = std::max(0.0, stats_.sanity - 3.0);
                break;
            case PE::Hit:  // V13.2: Schlag, Hieb, Biss - Schaden nach Schwierigkeit
                stats_.health = std::max(0.0, stats_.health - (double)e.strength * hitScale_);
                stats_.sanity = std::max(0.0, stats_.sanity - 2.0);
                ev.intensity = e.strength;
                if (stats_.health <= 0.0) {
                    stats_.dead = true;
                    deathCause_ = "stalker";
                    killRole_ = (int)e.role;
                }
                break;
            case PE::Whisper: stats_.sanity = std::max(0.0, stats_.sanity - 0.4 * e.strength); break;  // V13.2: es zehrt
            case PE::Spell: stats_.sanity = std::max(0.0, stats_.sanity - 2.0); break;  // V13.3 Teil 4: Jerrys Bann
            case PE::Static:  // V13.3: das Rauschen eines Growlers aus der Naehe - zehrt am Verstand, grosse schaden dem Kopf
                stats_.health = std::max(0.0, stats_.health - (double)e.strength * hitScale_);
                stats_.sanity = std::max(0.0, stats_.sanity - 0.5);
                if (stats_.health <= 0.0) {
                    stats_.dead = true;
                    deathCause_ = "stalker";
                    killRole_ = (int)e.role;
                }
                break;
            case PE::Scream:  // V13.2: Ratten - das Licht im Raum flackert (weiter ueber Flicker)
                if (const world::Tile* t = world_->tileIfLoaded(ifloor(e.pos.x), ifloor(e.pos.y)); t && t->room) haunt_.flicker(t->room);
                break;
            default: break;
        }
        events_.push_back(ev);
    }
    if (netRole == NetRole::Host) kinNetRoute(netFigureOut, netSoundOut);  // V13.3 Level 3: genaue Saetze und Waffenklaenge
}

// V10: Koerperart anwenden - realistisch: Masse aus dem Koerperbau der Spielerfigur
void GameSession::applyBodyStyle() {
    const int want = realisticBody ? 1 : 0;
    if (want == bodyStyle_) return;
    bodyStyle_ = want;
    BodyProportions pr;
    if (want) {
        const anim::Build& b = anim::figureBuild(anim::Figure::Player);
        const float hs = b.headScale * (0.6f + 0.4f * b.height / 1.78f);
        pr.thigh = b.thigh, pr.shin = b.shin, pr.ankle = b.ankle;
        pr.hip = b.ankle + (b.thigh + b.shin) * 0.975f;
        pr.eyeUp = 0.154f * hs, pr.eyeFwd = 0.1f * hs;
        pr.spine = (float)Player::kEye - pr.hip - pr.eyeUp;
        pr.neck = 0.11f * hs, pr.headR = 0.1f * hs;
        pr.upperArm = b.upperArm, pr.forearm = b.forearm;
        pr.hipW = b.hipW * 0.5f, pr.shoulderW = b.shoulderW * 0.5f;
        pr.footW = b.hipW * 0.5f + 0.02f * b.height;
        // V11: Griffmitte - knapp hinter den Knoecheln (die Finger liegen vorn am Schaft an) und um Schaftradius
        // plus halbe Handdicke vor der Handflaeche
        const float hand = b.hand > 0.0f ? b.hand : 0.104f * b.height;
        pr.gripFwd = hand * 0.45f - 0.027f;
        pr.gripOut = 0.0165f + 0.0135f;
        playerRig_.init(anim::Figure::Player);
        // V13: Lage der Hand am Schaft aus ihrem Skelett
        if (const props::Model* hm = vmHandModel() ? props::model("vm_hand_l") : nullptr; hm && hm->skin) {
            vec3 wr, fa, hd, pa;
            if (vmGeometryFromSkin(*hm->skin, wr, fa, hd, pa)) body_.setViewmodelGeometry(wr, fa, hd, pa);
        }
    }
    body_.setProportions(pr);
    body_.reset();
    body_.update(0.0, player_);
}

// V12: gibt es die Hand aus Blender (data/models/vm_hand_l.glb)?
bool GameSession::vmHandModel() {
    static const bool has = props::model("vm_hand_l") != nullptr;
    return has;
}

// V13: Geometrie der Hand aus ihrem Skelett (Lampenrahmen): Handgelenk, Ellbogenseite, Knoechel, Handflaeche.
// false, wenn das Modell kein Skelett hat (dann bleiben die Werte von V12)
bool vmGeometryFromSkin(const props::Skin& sk, vec3& wrist, vec3& forearm, vec3& handDir, vec3& palm) {
    vec3 elbow, mcp, index, little;
    if (!props::skinJointRest(sk, "hand", wrist) || !props::skinJointRest(sk, "elbow", elbow) ||
        !props::skinJointRest(sk, "f1_1", mcp) || !props::skinJointRest(sk, "f0_1", index) ||
        !props::skinJointRest(sk, "f3_1", little))
        return false;
    forearm = normalize(wrist - elbow);
    handDir = normalize(mcp - wrist);
    // Handflaeche: senkrecht zur Ebene Handgelenk - Zeige- - Kleinfinger-Grundgelenk, zur Beugeseite. Das Spiel ist
    // gespiegelt (linkshaendig): fuer die linke Hand zeigt cross(Zeige, Klein) hier zur Beugeseite
    palm = normalize(cross(index - wrist, little - wrist));
    return true;
}

// V10: realistischer Koerper - die Gelenke des Koerpers (Body) auf das Skelett der Spielerfigur
void GameSession::collectBody(std::vector<VisibleMesh>& out, bool firstPerson) {
    if (!realisticBody || bodyStyle_ != 1) {
        body_.collect(out, firstPerson);
        return;
    }
    playerRig_.setPose(playerJointPose(body_, vec3{(float)player_.x, (float)player_.y, (float)player_.z}));
    // V12: haelt die Hand die Lampe, kommt die Hand aus Blender (Body::collectProps); die des Skeletts (Seite 1 =
    // linke Hand des Koerpers) schrumpft ins Handgelenk. V13: die Lampe bleibt immer in der Hand - mit Skelett zeichnet
    // die Sitzung die Hand (Griff, Daumendruck als Mischung der Posen aus Blender)
    const props::Model* hm = vmHandModel() ? props::model("vm_hand_l") : nullptr;
    const props::Skin* sk = hm ? hm->skin.get() : nullptr;
    body_.setViewmodelHand(hm != nullptr, sk != nullptr);
    body_.setCarryLamp(sk != nullptr);
    playerRig_.hideHand(body_.viewmodelHand() && body_.lampVisible() ? 1 : -1);
    const size_t figFirst = out.size();
    if (!(debugDraw & 2)) playerRig_.collect(out, palette_, body_.eye(), figureDetail, false, true, firstPerson);
    if (sk && body_.viewmodelHand() && body_.lampVisible()) {
        // V13: was vom Aermel in die Hand aus Blender ragt, wegschneiden (Kugel 2 cm vor dem Handgelenk, Radius 2,5 cm)
        const vec3 wr = body_.pose().hand[0], hd = body_.flashHandDir();
        for (size_t i = figFirst; i < out.size(); ++i) out[i].clip = vec4(wr + hd * 0.02f, 0.025f);
    }
    if (!(debugDraw & 4)) body_.collectProps(out);
    const vec4 skin = phenomena_.skinFx(0);  // V13.3: Pilz und Ranken auf der eigenen Haut (nur Hautmaterialien)
    for (size_t i = figFirst; i < out.size(); ++i) out[i].fx = skin;
    if (sk && body_.viewmodelHand() && body_.lampVisible() && !(debugDraw & 1)) {
        static const int grip = sk->pose("grip"), press = sk->pose("press");
        std::vector<float> w(sk->poses.size(), 0.0f);
        const float p = body_.thumbPress();
        if (grip >= 0) w[(size_t)grip] = 1.0f - p;
        if (press >= 0) w[(size_t)press] = p;
        props::skinMatrices(*sk, w, vmHandBones_);
        const mat4 L = body_.lampFrame();
        const vec3 o = body_.flashGrip();
        const size_t off = palette_.size();
        for (const mat4& s : vmHandBones_) {
            mat4 m = L * s;
            m.c[3] = vec4(m.c[3].xyz() - o, 1.0f);
            palette_.push_back(m);
        }
        VisibleMesh v;
        v.model = &vmHandMesh_;
        v.meshId = &vmHandId_;
        v.world = mat4::translation(o);
        v.bones = (int)off;
        v.attached = true;
        v.cullCenter = o;
        v.cullRadius = 0.0f;
        v.fx = skin;
        out.push_back(v);
    }
}

void GameSession::collectShadows(std::vector<ShadowCaster>& out) {
    presence_.shadows(out);
    for (const Pawn* p : pawns)  // V13.2 Koop
        if (p) p->shadow(out);
    if (realisticBody && playerRig_.posed()) {
        ShadowCaster s;
        playerRig_.shadow(s);
        s.player = true;
        out.push_back(std::move(s));
    }
}

// --- V9: Sicherungen ----------------------------------------------------------------------------
const std::vector<std::string>& GameSession::fuseKinds() const { return world_->level().fuses.items; }

int GameSession::fusesHeld() const {
    int n = 0;
    for (const auto& k : fuseKinds()) n += hasFuse(k) ? 1 : 0;
    return n;
}

std::string GameSession::fuseLabel(const std::string& kind) const {
    const ItemDef* d = content_.item(kind);
    std::string name = d ? d->name : kind;
    const size_t a = name.find('('), b = name.rfind(')');
    return a != std::string::npos && b != std::string::npos && b > a ? name.substr(a + 1, b - a - 1) : name;
}

void GameSession::fuseLights(std::vector<FuseLight>& out, float radius) const {
    auto& reg = const_cast<ecs::Registry&>(reg_);
    reg.each<FuseBox, Transform>([&](ecs::Entity, FuseBox& fb, Transform& t) {
        if (fb.taken) return;
        const double dx = t.pos.x - player_.x, dy = t.pos.y - player_.y;
        if (dx * dx + dy * dy > (double)radius * radius) return;
        const vec3 dir{std::cos(t.yaw), std::sin(t.yaw), 0.0f};
        out.push_back({t.pos + dir * 0.2f + vec3{0, 0, 1.45f}, dir, (float)(std::hash<std::string>()(fb.id) % 997) * 0.37f});
    });
}

// Sicherungskaesten in der Naehe: erst das Brummen als Hinweis, dann - in Sicht - auf die Karte
void GameSession::trackFuses(double dt) {
    fuseScan_ -= dt;
    if (fuseScan_ > 0.0) return;
    fuseScan_ = 0.4;
    reg_.each<FuseBox, Transform>([&](ecs::Entity, FuseBox& fb, Transform& t) {
        const double dx = t.pos.x - player_.x, dy = t.pos.y - player_.y, d2 = dx * dx + dy * dy;
        if (!fb.taken && !hasFuse(fb.item) && d2 < 30.0 * 30.0 && hinted_.insert(fb.id).second)
            events_.push_back({GameEvent::Toast, "Irgendwo ganz in der Nähe brummt Starkstrom …", 3.5f});
        for (const auto& k : knownFuses_)
            if (k.id == fb.id) return;
        if (d2 > 22.0 * 22.0) return;
        const double sx = t.pos.x + std::cos(t.yaw) * 0.4, sy = t.pos.y + std::sin(t.yaw) * 0.4;
        if (!phys::clearLine(*tiles_, player_.x, player_.y, player_.z, sx, sy, 0.6, 1.4)) return;
        knownFuses_.push_back({fb.id, fb.item, t.pos, t.yaw});
        if (!fb.taken) events_.push_back({GameEvent::Toast, "Ein Sicherungskasten. Er brummt und flackert orange.", 3.2f});
    });
}

// In einer Aufzugskabine: die drei leeren Fassungen
void GameSession::cabinHints() {
    if (!world_->level().world() || ride_.active || fuseKinds().empty() || cabinAt(player_.x, player_.y) == ecs::kNull) return;
    const int n = fusesHeld(), all = (int)fuseKinds().size();
    if (!cabinHinted_) {
        cabinHinted_ = true;
        cabinFuses_ = n;
        events_.push_back({GameEvent::Toast, "Unter dem Tastenfeld: drei leere Fassungen für Sicherungen. Daneben, eingeritzt: »RAUS«.", 6.5f});
        return;
    }
    if (n == cabinFuses_) return;
    cabinFuses_ = n;
    if (n >= all) {
        events_.push_back({GameEvent::Toast, "Alle drei Sicherungen passen in die Fassungen. [E]", 4.0f});
    } else if (n > 0) {
        std::string missing;
        for (const auto& k : fuseKinds())
            if (!hasFuse(k)) missing += (missing.empty() ? "" : ", ") + fuseLabel(k);
        events_.push_back({GameEvent::Toast, std::format("Sicherungen: {} von {}. Es fehlen: {}", n, all, missing), 4.5f});
    }
}

}  // namespace lim::game
