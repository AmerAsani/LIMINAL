// V8: Aufzuege der zusammenhaengenden Welt.
//
// Ein Aufzug steht als Kern an einer Wand (world/room.cpp): hinten massiv, davor eine Kabine von
// 2 x 2 m hinter einer Schiebetuer. Ruft man ihn, gleitet die Tuer auf; in der Kabine startet E die
// Fahrt. Die Tuer schliesst, die Kabine ruckt an und brummt - und waehrend der Fahrt wird man in
// eine baugleiche Kabine in der naechsten Schicht versetzt (gleiche Lage in der Kabine, gleiche
// Blickrichtung). Es gibt weder Ladebild noch Ueberblendung; man merkt es erst, wenn die Tuer
// aufgeht. Jede Fahrt wird gemerkt: Derselbe Aufzug faehrt spaeter wieder zurueck.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>

#include "core/log.hpp"
#include "gameplay/session.hpp"
#include "render/texture_vending.hpp"
#include "world/room.hpp"

namespace lim::game {

namespace {
int displayCell(int layer) { return layer <= 0 ? 0 : (layer <= 9 ? layer : 12); }
std::string displayModel(int cell) { return std::format("elev_disp_{}", cell); }
}  // namespace

int GameSession::strataLayer(double x, double y) const {
    if (!world_->level().world()) return 0;
    return (int)std::floor(world_->generator().zones().strataCoord(x, y));
}

// Liegt (x, y) in der Kabine eines Aufzugs (Tuermitte, Blick aus der Kabine)?
bool GameSession::inCabin(const vec3& door, float yaw, double x, double y, double grow) {
    const double c = std::cos(yaw), s = std::sin(yaw);
    const double dx = x - door.x, dy = y - door.y;
    const double along = dx * c + dy * s, side = -dx * s + dy * c;
    return along > -2.0 - grow && along < 0.05 + grow && std::fabs(side) < 1.0 + grow;
}

ecs::Entity GameSession::cabinAt(double x, double y) {
    ecs::Entity found = ecs::kNull;
    reg_.each<Elevator, Transform>([&](ecs::Entity e, Elevator&, Transform& t) {
        if (found == ecs::kNull && inCabin(t.pos, t.yaw, x, y)) found = e;
    });
    return found;
}

ecs::Entity GameSession::elevatorById(const std::string& id) {
    ecs::Entity found = ecs::kNull;
    reg_.each<Elevator>([&](ecs::Entity e, Elevator& el) {
        if (el.id == id) found = e;
    });
    return found;
}

void GameSession::spawnElevators(const world::ChunkData& cd) {
    if (!world_->level().world()) return;
    std::unordered_set<const world::Room*> seen;
    for (int ly = 0; ly < world::CS; ++ly)
        for (int lx = 0; lx < world::CS; ++lx) {
            const world::Room* r = cd.tile(lx, ly).room;
            if (!r || !r->elevatorWanted || !seen.insert(r).second || !r->prepared()) continue;
            auto spot = r->elevatorSpot();
            if (!spot) continue;
            // der Aufzug gehoert zum Chunk seiner Kabine
            const i64 cx = ifloor(spot->doorX - spot->dirX * 1.0), cy = ifloor(spot->doorY - spot->dirY * 1.0);
            if (cx < cd.x0() || cx >= cd.x0() + world::CS || cy < cd.y0() || cy >= cd.y0() + world::CS) continue;
            const vec3 pos{(float)spot->doorX, (float)spot->doorY, (float)r->floor};
            const float yaw = std::atan2((float)spot->dirY, (float)spot->dirX);
            ecs::Entity e = prefabs_.spawn(reg_, "elevator", pos, yaw);
            if (e == ecs::kNull) continue;
            if (auto* el = reg_.get<Elevator>(e)) {
                el->id = std::format("elev:{},{},{}", r->sx, r->sy, r->index);
                el->layer = strataLayer(spot->doorX, spot->doorY);
                el->display = displayCell(el->layer);
                el->mDisplay = displayModel(el->display);
                // Ankunft: der Zielaufzug erscheint erst mit seinem Chunk - die Tuer geht dann auf
                if (ride_.active && ride_.moved && el->id == ride_.destId) el->display = ride_.destLayer > ride_.fromLayer ? 10 : 11,
                                                                           el->mDisplay = displayModel(el->display);
            }
            reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
        }
}

// Ziel einer Fahrt: eine gemerkte Verbindung, sonst ein Aufzug in der naechsten Schicht nach aussen -
// auf dem Strahl vom Start durch diesen Aufzug, so dass die Richtung erhalten bleibt.
bool GameSession::findElevatorTarget(const std::string& fromId, vec3 door, int layer, ElevatorLink& out) {
    if (auto it = elevLinks_.find(fromId); it != elevLinks_.end()) {
        out = it->second;
        return true;
    }
    const world::LevelDef& L = world_->level();
    if (!L.world()) return false;
    const auto& zf = world_->generator().zones();
    const int target = layer + 1;
    double len = std::hypot(door.x, door.y);
    const double ux = len > 1.0 ? door.x / len : 1.0, uy = len > 1.0 ? door.y / len : 0.0;
    double rr = len;
    for (; rr < len + 3000.0; rr += 8.0)
        if (zf.strataCoord(ux * rr, uy * rr) >= (double)target + 0.45) break;
    const double px = ux * rr, py = uy * rr;
    const i64 S = L.sectorSize;
    const i64 csx = floorDiv(ifloor(px), S), csy = floorDiv(ifloor(py), S);
    double bestD = 1e18;
    for (i64 rad = 0; rad <= 6; ++rad) {
        for (i64 sy = csy - rad; sy <= csy + rad; ++sy)
            for (i64 sx = csx - rad; sx <= csx + rad; ++sx) {
                if (std::max(std::abs(sx - csx), std::abs(sy - csy)) != rad) continue;
                world::Sector& sec = world_->sector(sx, sy);
                std::vector<const world::Room*> cands;
                for (const auto& rp : sec.rooms)
                    if (rp->elevatorWanted) cands.push_back(rp.get());
                for (const world::Room* r : cands) {
                    auto [rcx, rcy] = r->center();
                    if ((int)std::floor(zf.strataCoord(rcx, rcy)) != target) continue;
                    if (!r->prepared()) world_->tile(ifloor(rcx), ifloor(rcy));  // Sektor verknuepfen -> Raum vorbereitet
                    if (!r->prepared()) continue;
                    auto spot = r->elevatorSpot();
                    if (!spot) continue;
                    const double d = std::hypot(spot->doorX - px, spot->doorY - py);
                    if (d < bestD) {
                        bestD = d;
                        out.to = std::format("elev:{},{},{}", r->sx, r->sy, r->index);
                        out.door = {(float)spot->doorX, (float)spot->doorY, (float)r->floor};
                        out.dir = {(float)spot->dirX, (float)spot->dirY};
                        out.layer = target;
                    }
                }
            }
        if (bestD < 1e17) return true;
    }
    return false;
}

bool GameSession::startRide(ecs::Entity e) {
    auto* el = reg_.get<Elevator>(e);
    auto* t = reg_.get<Transform>(e);
    if (!el || !t || ride_.active) return false;
    ride_ = {};
    ride_.active = true;
    ride_.fromId = el->id;
    ride_.fromDoor = t->pos;
    ride_.fromDir = {std::cos(t->yaw), std::sin(t->yaw)};
    ride_.fromLayer = el->layer;
    el->openTarget = 0.0f;
    el->hold = 0.0f;
    GameEvent ev{GameEvent::SoundAt};
    ev.sound = "elev_doors", ev.pos = t->pos + vec3{0, 0, 1.2f}, ev.intensity = 0.8f;
    events_.push_back(ev);
    return true;
}

void GameSession::updateElevators(double dt, double prevX, double prevY) {
    const float fdt = (float)dt;
    // Tueren: gleiten auf/zu; offene bleiben eine Weile offen (solange jemand in der Kabine steht)
    reg_.each<Elevator, Transform>([&](ecs::Entity e, Elevator& el, Transform& t) {
        const bool inside = inCabin(t.pos, t.yaw, player_.x, player_.y);
        const bool riding = ride_.active && (el.id == ride_.fromId || el.id == ride_.destId);
        if (!riding && el.openTarget > 0.5f) {
            if (!inside) el.hold -= fdt;
            if (el.hold <= 0.0f) {
                el.openTarget = 0.0f;
                GameEvent ev{GameEvent::SoundAt};
                ev.sound = "elev_doors", ev.pos = t.pos + vec3{0, 0, 1.2f}, ev.intensity = 0.6f;
                events_.push_back(ev);
            }
        }
        const float speed = 1.0f / 1.1f;
        el.open += std::clamp(el.openTarget - el.open, -speed * fdt, speed * fdt);
        // geschlossene Tuer: nicht hineinlaufen (die Kabine selbst ist begehbarer Boden)
        if (el.open < 0.85f && !riding && inCabin(t.pos, t.yaw, player_.x, player_.y, 0.3) &&
            !inCabin(t.pos, t.yaw, prevX, prevY, 0.3)) {
            player_.x = prevX;
            player_.y = prevY;
        }
        (void)e;
    });
    if (!ride_.active) return;

    // --- Fahrt ---------------------------------------------------------------------------------
    ride_.t += dt;
    ecs::Entity from = elevatorById(ride_.fromId);
    Elevator* fe = from != ecs::kNull ? reg_.get<Elevator>(from) : nullptr;
    if (ride_.travel < 0.0) {
        // Tuer schliesst - dann geht es los
        if (fe && fe->open > 0.02f && ride_.t < 4.0) return;
        if (ride_.escape) {  // V9: die letzte Fahrt fuehrt nirgendwohin, das man kennt
            ride_.travel = 0.0;
            if (fe) fe->display = 11, fe->mDisplay = displayModel(11);
            GameEvent ev{GameEvent::SoundAt};
            ev.sound = "escape_ride", ev.pos = ride_.fromDoor + vec3{0, 0, 1.4f}, ev.intensity = 1.0f;
            events_.push_back(ev);
            log::info("Die letzte Fahrt beginnt ({})", ride_.fromId);
            return;
        }
        ElevatorLink target;
        if (!findElevatorTarget(ride_.fromId, ride_.fromDoor, ride_.fromLayer, target)) {
            events_.push_back({GameEvent::Toast, "Der Aufzug rührt sich nicht.", 2.5f});
            if (fe) fe->openTarget = 1.0f, fe->hold = 5.0f;
            ride_ = {};
            return;
        }
        ride_.destId = target.to;
        ride_.destDoor = target.door;
        ride_.destDir = target.dir;
        ride_.destLayer = target.layer;
        ride_.travel = 0.0;
        if (fe) fe->display = ride_.destLayer > ride_.fromLayer ? 10 : 11, fe->mDisplay = displayModel(fe->display);
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "elev_ride", ev.pos = ride_.fromDoor + vec3{0, 0, 1.4f}, ev.intensity = 0.9f;
        events_.push_back(ev);
        log::info("Aufzug {} -> {} (Schicht {} -> {})", ride_.fromId, ride_.destId, ride_.fromLayer, ride_.destLayer);
        return;
    }
    ride_.travel += dt;
    if (ride_.escape) {
        // V9: nach elf Sekunden ist nur noch Licht. Die Sicherungen bleiben in den Fassungen.
        if (!ride_.escaped && ride_.travel >= 11.0) {
            ride_.escaped = true;
            for (const auto& k : fuseKinds())
                for (int i = 0; i < Inventory::kSize; ++i)
                    if (inventory_.slots[(size_t)i] && inventory_.slots[(size_t)i]->kind == k) {
                        inventory_.take(i);
                        break;
                    }
            runLog_.escaped = true;
            runLog_.escapeTime = stats_.playTime;
            events_.push_back({GameEvent::Escaped});
            log::info("Entkommen nach {:.0f} s Spielzeit", stats_.playTime);
        }
        return;  // die Fahrt endet erst mit finishEscape() (nach dem Abspann)
    }
    if (!ride_.moved && ride_.travel >= 1.7) {
        // In die baugleiche Kabine am Ziel: gleiche Lage in der Kabine, gleiche Blickrichtung zur Tuer
        const double fyaw = std::atan2(ride_.fromDir.y, ride_.fromDir.x), tyaw = std::atan2(ride_.destDir.y, ride_.destDir.x);
        const double dx = player_.x - ride_.fromDoor.x, dy = player_.y - ride_.fromDoor.y;
        const double along = dx * std::cos(fyaw) + dy * std::sin(fyaw), side = -dx * std::sin(fyaw) + dy * std::cos(fyaw);
        player_.x = ride_.destDoor.x + along * std::cos(tyaw) - side * std::sin(tyaw);
        player_.y = ride_.destDoor.y + along * std::sin(tyaw) + side * std::cos(tyaw);
        player_.z = ride_.destDoor.z;
        player_.angle += tyaw - fyaw;
        player_.vx = player_.vy = player_.vz = 0.0;
        world_->preload(player_.x, player_.y);  // im geschlossenen Kasten faellt ein Ruckler nicht auf
        body_.reset();
        body_.update(0.0, player_);
        // Rueckweg merken (beide Richtungen)
        elevLinks_[ride_.fromId] = {ride_.destId, ride_.destDoor, ride_.destDir, ride_.destLayer};
        elevLinks_[ride_.destId] = {ride_.fromId, ride_.fromDoor, ride_.fromDir, ride_.fromLayer};
        ride_.moved = true;
        events_.push_back({GameEvent::TeleportCut});
        // der Ausgangsaufzug kann mit seinem Chunk entladen sein - neu suchen statt alten Zeiger nutzen
        if (ecs::Entity src = elevatorById(ride_.fromId); src != ecs::kNull)
            if (auto* se = reg_.get<Elevator>(src)) se->display = displayCell(se->layer), se->mDisplay = displayModel(se->display);
        return;
    }
    if (ride_.moved && !ride_.arrived && ride_.travel >= 4.6) {
        ride_.arrived = true;
        ecs::Entity dest = elevatorById(ride_.destId);
        if (auto* de = dest != ecs::kNull ? reg_.get<Elevator>(dest) : nullptr) {
            de->openTarget = 1.0f;
            de->hold = 6.0f;
            de->display = displayCell(de->layer);
            de->mDisplay = displayModel(de->display);
        }
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = "elev_ding", ev.pos = ride_.destDoor + vec3{0, 0, 2.4f}, ev.intensity = 0.8f;
        events_.push_back(ev);
        ev.sound = "elev_doors", ev.pos = ride_.destDoor + vec3{0, 0, 1.2f}, ev.intensity = 0.8f;
        events_.push_back(ev);
    }
    if (ride_.arrived && ride_.travel >= 5.6) ride_ = {};  // Tuer ist auf - weiter zu Fuss
}

// V9: mit allen drei Sicherungen
bool GameSession::startEscape(ecs::Entity e) {
    if (fuseKinds().empty() || fusesHeld() < (int)fuseKinds().size() || !startRide(e)) return false;
    ride_.escape = true;
    events_.push_back({GameEvent::EscapeStart, "Die drei Sicherungen rasten ein. Die Kabine summt – lauter als je zuvor.", 5.0f});
    return true;
}

float GameSession::escapeGlow() const {
    if (!ride_.active || !ride_.escape || ride_.travel < 0.0) return 0.0f;
    return (float)std::clamp((ride_.travel - 2.5) / 8.5, 0.0, 1.0);
}

void GameSession::finishEscape() {
    if (!ride_.escape) return;
    if (ecs::Entity e = elevatorById(ride_.fromId); e != ecs::kNull)
        if (auto* el = reg_.get<Elevator>(e)) {
            el->openTarget = 1.0f;
            el->hold = 6.0f;
            el->display = displayCell(el->layer);
            el->mDisplay = displayModel(el->display);
        }
    ride_ = {};
    presence_.clear();
    presence_.setGrace(240.0);
    events_.push_back({GameEvent::Toast, "Die Tür geht auf. Es riecht nach feuchtem Teppich.", 5.0f});
}

void GameSession::elevatorLights(std::vector<vec3>& out, float radius) const {
    auto& reg = const_cast<ecs::Registry&>(reg_);
    reg.each<Elevator, Transform>([&](ecs::Entity, Elevator& el, Transform& t) {
        const double dx = t.pos.x - player_.x, dy = t.pos.y - player_.y;
        if (dx * dx + dy * dy > (double)radius * radius) return;
        if (el.open < 0.02f && !inCabin(t.pos, t.yaw, player_.x, player_.y)) return;
        out.push_back(t.pos + vec3{-std::cos(t.yaw) * 1.0f, -std::sin(t.yaw) * 1.0f, 2.15f});
    });
}

}  // namespace lim::game
