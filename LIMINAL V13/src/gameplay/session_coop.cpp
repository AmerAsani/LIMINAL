// V13.2 Koop: die Teile der Spielsitzung, die Host und Gast brauchen (MULTIPLAYER_ARCHITEKTUR.md, Abschnitte 3, 8, 9).
// Der Host fuehrt hier die Absichten der Gaeste aus - jede an seinem eigenen Zustand geprueft (Reichweite, Sicht,
// Inventar). Der Gast setzt hier um, was der Host meldet (aufgenommen, abgelegt, Stromausfall).
#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "gameplay/session.hpp"
#include "world/map_def.hpp"
#include "world/room.hpp"

namespace lim::game {

void GameSession::pawnInput(Pawn& p, const PlayerControl& c0, double dt) {
    if (!p.alive) return;
    PlayerControl c = c0;
    p.player.speedMul = p.stats.speedMul() * (p.stats.exhausted ? 0.9 : 1.0);
    if (p.id > 0 && p.id < Phenomena::kPlayers) {  // V13.3: Fieber, Ranken, ihre Einladung (session_phenomena.cpp)
        p.player.speedMul *= phenomena_.speedMul(p.id);
        if (phenomena_.frozen(p.id)) c.forward = c.side = 0.0f, c.sprint = c.jump = c.jumpHeld = false;
    }
    if (!p.stats.canSprint()) c.sprint = false;
    if (const world::Room* wr = world_->roomAt(p.player.x, p.player.y)) p.player.waterZ = wr->water;
    else p.player.waterZ = -999.0;
    p.player.update(dt, c, tileSrc());
    p.stats.updateStamina(dt, p.player.sprinting, p.player.groundSpeed > 0.3);
    if (p.player.jumped) p.stats.spendStamina(4.0);
    // Laerm wie beim eigenen Spieler (updatePresence)
    double noise = 0.0;
    const bool moving = p.player.groundSpeed > 0.4 || p.player.swimming;
    if (moving) noise = p.player.sprinting ? 1.0 : (p.player.sneaking ? 0.15 : 0.45);
    if (moving && (p.player.wading || p.player.swimming || surfaceAt(p.player.x, p.player.y) == Surface::Puddle)) noise = std::min(1.0, noise + 0.4);
    if (p.player.landed) p.landNoise = std::max(p.landNoise, 0.8 * std::clamp(p.player.fallSpeed / 5.0, 0.4, 1.0));
    p.noise = std::max(noise, p.landNoise);
    // Schritte des Mitspielers hoert der Host im Raum
    if (p.player.stepped) {
        GameEvent ev{GameEvent::OtherStep};
        ev.pos = {(float)p.player.x, (float)p.player.y, (float)p.player.z};
        ev.surface = surfaceAt(p.player.x, p.player.y);
        ev.intensity = (float)std::min(1.0, p.player.speed() / 3.0);
        events_.push_back(ev);
    }
}

void GameSession::pawnTick(Pawn& p, double dt) {
    p.landNoise = std::max(0.0, p.landNoise - dt * 2.5);
    if (!p.alive) {
        p.deadT += dt;
        return;
    }
    if (p.stats.health <= 0.0) p.stats.dead = true;  // vor der Erholung (Stats::update)
    if (!p.stats.dead) p.stats.update(dt);
    if (p.flash.update(dt)) netPersonalOut.push_back({p.id, "Die Batterie der Taschenlampe ist leer."});
    p.player.breathless = p.stats.breathless();
    if (!world_->level().flashlight) p.flash.on = false;
    // Raumtypen zehren am Verstand oder geben ihn zurueck; Hitze kostet Ausdauer (wie beim eigenen Spieler)
    const world::Room* r = world_->roomAt(p.player.x, p.player.y);
    if (r && r->type && r->type->sanityDrain != 0.0) p.stats.sanity = std::clamp(p.stats.sanity - dt * r->type->sanityDrain, 0.0, 100.0);
    if (r && r->type && r->type->staminaDrain > 0.0) p.stats.spendStamina(dt * r->type->staminaDrain);
    // Sicherungspunkte gelten fuer die Gruppe: wer vorbeigeht, setzt ihn
    const world::LevelDef& L = world_->level();
    if (L.mapped() && !presence_.threatActive())
        for (const auto& cp : L.map->checkpoints) {
            if (cp.id == checkpoint_) continue;
            const double dx = cp.x - p.player.x, dy = cp.y - p.player.y;
            if (dx * dx + dy * dy > 2.2 * 2.2) continue;
            checkpoint_ = cp.id, checkpointLabel_ = cp.label;
            GameEvent ev{GameEvent::Toast, std::format("{}: Sicherungspunkt – {}", p.name, cp.label), 2.6f};
            events_.push_back(ev);
            break;
        }
    if (p.stats.health <= 0.0) p.stats.dead = true;
    if (p.stats.dead) {
        p.alive = false;
        p.deadT = 0.0;
        p.flash.on = false;
        log::info("Koop: {} ist tot ({})", p.name, p.killRole >= 0 ? Presence::roleName((Presence::Role)p.killRole) : "Werte");
    }
    p.updateBody(dt);
}

bool GameSession::pawnInteract(Pawn& p, u32 hash, std::string& msg, int& note) {
    note = -1;
    if (p.alive && hash < 4) return pawnSpeak(p, (int)hash);  // V13.3: Ansprechen (Satz 0..3, session_lure.cpp)
    if (!p.alive || !hash) return false;
    ecs::Entity found = ecs::kNull;
    reg_.each<Pickup, Transform>([&](ecs::Entity e, Pickup& pk, Transform&) {
        if (found == ecs::kNull && worldItemHash(pk.worldId) == hash) found = e;
    });
    if (found == ecs::kNull)
        reg_.each<NoteFind, Transform>([&](ecs::Entity e, NoteFind& nf, Transform&) {
            if (found == ecs::kNull && worldItemHash(nf.id) == hash) found = e;
        });
    if (found == ecs::kNull) return false;  // gibt es nicht (mehr) - ein anderer war schneller oder die Kennzahl ist erfunden
    const Transform* t = reg_.get<Transform>(found);
    const Interactable* in = reg_.get<Interactable>(found);
    const Player& pl = p.player;
    // Reichweite wie beim eigenen Spieler, mit 0,6 m Spielraum fuer die Laufzeit im Netz; Hoehe; freie Sicht
    const double d = std::hypot(t->pos.x - pl.x, t->pos.y - pl.y);
    if (d > (in ? in->range : 2.1f) + 0.6 || t->pos.z - pl.z > 1.9 || pl.z - t->pos.z > 1.2) return false;
    if (!phys::clearLine(tileSrc(), pl.x, pl.y, pl.z, t->pos.x, t->pos.y, 0.55, 1.2)) return false;
    if (auto* nf = reg_.get<NoteFind>(found)) {
        picked.insert(nf->id);
        note = nf->index;
        netWorldOut.push_back({NetWorldEvent::Picked, hash, "", t->pos, p.id});
        reg_.destroy(found);
        return true;
    }
    auto* pk = reg_.get<Pickup>(found);
    const ItemDef* def = content_.item(pk->item);
    if (!p.inventory.add(pk->item)) {
        msg = "Inventar voll";
        return false;
    }
    msg = std::format("{} aufgenommen", def ? def->name : pk->item);
    if (pk->worldId.rfind("drop:", 0) == 0) std::erase_if(dropped_, [&](const DroppedItem& x) { return x.id == pk->worldId; });
    else picked.insert(pk->worldId);
    netWorldOut.push_back({NetWorldEvent::Picked, hash, pk->item, t->pos, p.id});
    reg_.destroy(found);
    if (target_ == found) target_ = ecs::kNull;
    return true;
}

bool GameSession::pawnUse(Pawn& p, int idx, std::string& msg, std::string& sound) {
    if (!p.alive || idx < 0 || idx >= Inventory::kSize || !p.inventory.slots[(size_t)idx]) return false;
    if (isFuse(p.inventory.slots[(size_t)idx]->kind)) {
        msg = "Eine Sicherung. Sie gehört in eine Fassung.";
        return false;
    }
    if (p.inventory.slots[(size_t)idx]->kind == "glowstick") {  // V13.3: die Geste lief beim Gast - jetzt der Wurf
        p.inventory.take(idx);
        launchGlowstick(p.player, p.eye(), p.id);
        return true;
    }
    auto kind = p.inventory.take(idx);
    const ItemDef* d = kind ? content_.item(*kind) : nullptr;
    if (!d) return false;
    sound = d->useSound;
    if (d->battery > 0.0) {
        p.flash.addCharge(d->battery);
        msg = std::format("{}: Taschenlampe {} %", d->name, (int)std::lround(p.flash.charge));
        return true;
    }
    msg = p.stats.consume(*d);
    std::vector<Phenomena::Event> pe;  // V13.3: offenes Mandelwasser steckt an, versiegeltes heilt frueh
    if (p.id > 0 && p.id < Phenomena::kPlayers)
        if (const std::string extra = phenomena_.drink(p.id, d->key, pe); !extra.empty()) msg += " · " + extra;
    return true;
}

bool GameSession::pawnDrop(Pawn& p, int idx, bool all) {
    if (!p.alive || idx < 0 || idx >= Inventory::kSize || !p.inventory.slots[(size_t)idx]) return false;
    const std::string kind = p.inventory.slots[(size_t)idx]->kind;
    const int n = all ? p.inventory.slots[(size_t)idx]->count : 1;
    for (int i = 0; i < n; ++i) p.inventory.take(idx);
    throwFrom(p.player, p.eye(), kind, n, p.id);
    return true;
}

void GameSession::pawnRespawn(Pawn& p) {
    const world::MapCheckpoint* cp = nullptr;
    if (world_->level().mapped())
        for (const auto& c : world_->level().map->checkpoints)
            if (c.id == checkpoint_) cp = &c;
    if (cp) p.player = Player(cp->x, cp->y, world_->tile(ifloor(cp->x), ifloor(cp->y)).floor, cp->yaw);
    else p.player = Player(player_.x, player_.y, player_.z, player_.angle);
    double x = p.player.x, y = p.player.y, z = p.player.z;
    if (phys::findFreeSpot(tileSrc(), x, y, z, p.player.shape)) p.player.x = x, p.player.y = y, p.player.z = z;
    p.stats.health = 100.0;
    p.stats.sanity = std::max(p.stats.sanity, 60.0);
    p.stats.zeroTime = 0.0;
    p.stats.stamina = 100.0;
    p.stats.exhausted = false;
    p.stats.dead = false;
    p.alive = true;
    p.deadT = 0.0;
    p.killRole = -1;
    p.anySeq = false;
    if (p.id > 0 && p.id < Phenomena::kPlayers) phenomena_.reset(p.id);  // V13.3
    p.body.reset();
    log::info("Koop: {} kommt am Sicherungspunkt {} zurueck", p.name, checkpoint_.empty() ? "-" : checkpoint_);
}

void GameSession::respawnCoop() {
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
    air_ = 30.0, deepT_ = 0.0, deepStage_ = 0;
    phenomena_.reset(0);  // V13.3
    events_.push_back({GameEvent::TeleportCut});
}

const world::MapTransition* GameSession::transitionAt(const Player& p, const Inventory& inv, bool& needsOk) const {
    needsOk = true;
    const world::LevelDef& L = world_->level();
    if (!L.mapped()) return nullptr;
    const world::Room* room = world_->roomAt(p.x, p.y);
    for (const world::MapTransition& t : L.map->transitions) {
        if ((t.target.empty() && t.kind != "end") || t.x1 <= t.x0) continue;
        if (p.x < (double)t.x0 || p.x >= (double)t.x1 || p.y < (double)t.y0 || p.y >= (double)t.y1) continue;
        if (t.kind == "hole" && (!room || p.z > room->floor - 2.5)) continue;
        if (t.kind == "dive" && !(p.waterZ > -900.0 && p.z < p.waterZ - 7.0)) continue;
        needsOk = t.needs.empty() || inv.count(t.needs) > 0;
        return &t;
    }
    return nullptr;
}

// werfen aus der Hand eines Spielers (dieser Rechner: throwItem; Host: auch fuer die Gaeste)
void GameSession::throwFrom(const Player& p, vec3 eye, const std::string& kind, int count, int by) {
    vec3 fwd = forwardFromAngles((float)p.angle, (float)std::clamp(p.pitch, -0.7, 0.45));
    vec3 flat = normalize(vec3{fwd.x, fwd.y, 0.0f});
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
        if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Dropped, worldItemHash(d.id), kind, d.pos, by});
    }
}

// --- Gast ---------------------------------------------------------------------------------------
void GameSession::netRemoveItem(u32 hash) {
    std::vector<ecs::Entity> gone;
    reg_.each<Pickup, Transform>([&](ecs::Entity e, Pickup& pk, Transform&) {
        if (pk.worldId.rfind("drop:", 0) == 0) {
            for (const DroppedItem& d : dropped_)
                if (d.id == pk.worldId && (d.netHash ? d.netHash : worldItemHash(d.id)) == hash) gone.push_back(e);
        } else if (worldItemHash(pk.worldId) == hash) {
            picked.insert(pk.worldId);
            gone.push_back(e);
        }
    });
    reg_.each<NoteFind, Transform>([&](ecs::Entity e, NoteFind& nf, Transform&) {
        if (worldItemHash(nf.id) == hash) picked.insert(nf.id), gone.push_back(e);
    });
    std::erase_if(dropped_, [&](const DroppedItem& d) { return d.netHash == hash; });
    for (ecs::Entity e : gone) {
        if (target_ == e) target_ = ecs::kNull;
        if (reg_.alive(e)) reg_.destroy(e);
    }
}

void GameSession::netAddDropped(u32 hash, const std::string& kind, vec3 pos) {
    if (kind == "glowstick_lit") {  // V13.3: geworfener, leuchtender Leuchtstab (session_lure.cpp)
        netAddLure(pos);
        return;
    }
    if (kind == "painting") {  // V13.3: ein Gemaelde liegt jetzt vor einer anderen Tuer (session_phenomena.cpp)
        phenomena_.netMove(hash, pos);
        return;
    }
    if (!content_.item(kind)) return;
    for (const DroppedItem& d : dropped_)
        if (d.netHash == hash) return;  // schon da (doppelt gemeldet)
    if (dropped_.size() >= kMaxDropped) {
        if (reg_.alive(dropped_.front().entity)) reg_.destroy(dropped_.front().entity);
        dropped_.erase(dropped_.begin());
    }
    DroppedItem d;
    d.id = std::format("drop:n{}", hash);
    d.kind = kind;
    d.pos = pos;
    d.yaw = hash01(hash) * 6.28f;
    d.netHash = hash;
    dropped_.push_back(d);
    spawnDropped(dropped_.back(), vec3{0, 0, 0}, false);  // faellt aus der Hand zu Boden
}

void GameSession::netPowerCut(vec3 pos, float seconds) {
    haunt_.powerCut(world_->roomAt(pos.x, pos.y), seconds, pos);
}

void GameSession::netDie(int role) {
    if (stats_.dead) return;
    stats_.health = 0.0;
    stats_.dead = true;
    deathCause_ = role >= 0 ? "stalker" : role == -2 ? "pilz" : role == -3 ? "garten" : "stats";  // V13.3: -2/-3 Erscheinungen
    killRole_ = role;
    if (role >= 0) {
        killCam_ = true, killT_ = 0.0;
        killYaw0_ = (float)player_.angle, killPitch0_ = (float)player_.pitch;
    }
}

double GameSession::netReconcile(vec3 pos, vec3 vel, const std::vector<NetReplay>& replay, bool snap) {
    Player t = player_;
    t.x = pos.x, t.y = pos.y, t.z = pos.z;
    t.vx = vel.x, t.vy = vel.y, t.vz = vel.z;
    for (const NetReplay& r : replay) {
        PlayerControl c = r.c;
        c.yawDelta = c.pitchDelta = 0.0f, c.turn = c.look = 0.0f;
        t.angle = r.yaw, t.pitch = r.pitch;
        t.update(r.dt, c, tileSrc());
    }
    const double ex = t.x - player_.x, ey = t.y - player_.y, ez = t.z - player_.z;
    const double err = std::sqrt(ex * ex + ey * ey + ez * ez);
    if (snap || err > 2.0) {  // grosser Fehler (oder Wiedereinstieg): springen
        player_.x = t.x, player_.y = t.y, player_.z = t.z;
        player_.vx = t.vx, player_.vy = t.vy, player_.vz = t.vz;
        player_.onGround = t.onGround;
    } else if (err > 0.01) {  // weich: ein Drittel je Momentaufnahme (20/s - nach etwa 0,2 s ausgeglichen)
        const double k = err > 0.5 ? 0.6 : 0.35;
        player_.x += ex * k, player_.y += ey * k, player_.z += ez * k;
        player_.vx = t.vx, player_.vy = t.vy;
    }
    return err;
}

void GameSession::netFlicker(vec3 pos) {
    if (const world::Tile* t = world_->tileIfLoaded(ifloor(pos.x), ifloor(pos.y)); t && t->room) haunt_.flicker(t->room);
}

u32 GameSession::targetHash() const {
    if (target_ == ecs::kNull) return 0;
    auto& reg = const_cast<ecs::Registry&>(reg_);
    if (auto* pk = reg.get<Pickup>(target_)) {
        if (pk->worldId.rfind("drop:", 0) == 0)
            for (const DroppedItem& d : dropped_)
                if (d.id == pk->worldId) return d.netHash ? d.netHash : worldItemHash(d.id);
        return worldItemHash(pk->worldId);
    }
    if (auto* nf = reg.get<NoteFind>(target_)) return worldItemHash(nf->id);
    return 0;
}

u32 GameSession::sharedHash() const {
    // reihenfolgeunabhaengig: Summe gemischter Kennzahlen, dazu die Anzahlen
    auto mix = [](u32 h) {
        h ^= h >> 16, h *= 0x7feb352du, h ^= h >> 15, h *= 0x846ca68bu, h ^= h >> 16;
        return h;
    };
    u32 a = 0, b = 0;
    for (const std::string& id : picked) a += mix(worldItemHash(id));
    for (const DroppedItem& d : dropped_) b += mix((d.netHash ? d.netHash : worldItemHash(d.id)) ^ 0x9E3779B9u);
    return mix(a ^ mix(b + 0x632BE5ABu) ^ ((u32)picked.size() << 8) ^ (u32)dropped_.size());
}

int GameSession::itemCount() const {
    int n = 0;
    const_cast<ecs::Registry&>(reg_).each<Pickup>([&](ecs::Entity, Pickup&) { ++n; });
    return n;
}

// Praesenz im Koop: Host - die Gaeste sind weitere Ziele; Gast - Mitspieler fuer Blick und Licht der Puppen
void GameSession::updatePawnsPresence(Presence::Context& c) {
    others_.clear();
    for (Pawn* p : pawns) {
        if (!p || !p->alive) continue;
        Presence::Other o;
        o.id = p->id;
        o.player = &p->player;
        o.view.eye = p->eye();
        o.view.look = forwardFromAngles((float)p->player.angle, (float)p->player.pitch);
        vec3 fp, fd;
        float pw = 0.0f;
        o.view.flashOn = p->beam(fp, fd, pw, stats_.playTime) && pw > 0.15f;
        o.view.flashPos = fp, o.view.flashDir = fd;
        o.view.blackoutSeed = c.view.blackoutSeed, o.view.blackoutAmount = c.view.blackoutAmount;
        o.view.blackoutOrigin = c.view.blackoutOrigin, o.view.blackoutFront = c.view.blackoutFront, o.view.fog = c.view.fog;
        o.noise = p->noise;
        o.room = world_->roomAt(p->player.x, p->player.y);
        o.canBlackout = o.room && !o.room->fixtures().empty() && o.room->avgLight > 0.2;
        others_.push_back(o);
    }
    c.others = &others_;
    c.selfId = netSelfId;
    c.selfAlive = !stats_.dead;
    if (netRole == NetRole::Guest) {
        c.puppet = true;
        return;
    }
    // Host: tot - die Wesen machen weiter, solange jemand lebt
    bool anyAlive = !stats_.dead;
    for (Pawn* p : pawns) anyAlive = anyAlive || (p && p->alive);
    c.suspended = ride_.active || !anyAlive;
}

}  // namespace lim::game
