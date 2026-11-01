// V13.3: Erscheinungen von Level 1 ohne Figur in der Sitzung (gameplay/phenomena.hpp; KREATUR_Crawler.md,
// KREATUR_GameMaster.md, KREATUR_L1_Erscheinungen.md).
//
// Einzelspieler/Host rechnen Ansteckung, Garden-Uhr, Tod und Hinauswurf fuer sich und (Host) fuer die Figuren der
// Gaeste; Verstand und Tempo der Gaeste setzt der Host an deren Werten. Der Gast rechnet fuer sich nur, was er sieht und
// hoert (Haut, Bildrand, Klang, Einblendung), aus derselben Karte und seiner eigenen Lage - spielentscheidend ist allein
// der Host: den Tod meldet er mit der Rolle -2 (Pilz) bzw. -3 (Garden), den Hinauswurf als Korrektur der Lage, die
// Wege der Gemaelde als Weltereignis Dropped "painting" (Protokoll unveraendert).
#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "gameplay/navfield.hpp"
#include "gameplay/session.hpp"
#include "world/room.hpp"

namespace lim::game {

bool GameSession::inGarden(double x, double y) const {
    const world::Room* r = world_->roomAt(x, y);
    return r && r->type && r->type->key == "garden";
}

// unversiegelte Flaschen (offenes Mandelwasser) an ihren Plaetzen - wie Fundstuecke, je Chunk
void GameSession::spawnTraces(const world::ChunkData& cd) {
    const ItemDef* def = content_.item("almond_open");
    if (!def) return;
    for (const Phenomena::Spot& s : phenomena_.bottleSpots()) {
        const i64 tx = ifloor(s.pos.x), ty = ifloor(s.pos.y);
        if (tx < cd.x0() || tx >= cd.x0() + world::CS || ty < cd.y0() || ty >= cd.y0() + world::CS) continue;
        const std::string id = "trace:" + s.id;
        if (picked.count(id)) continue;
        ecs::Entity e = prefabs_.spawn(reg_, "item_pickup", s.pos, s.yaw);
        if (e == ecs::kNull) continue;
        if (auto* p = reg_.get<Pickup>(e)) p->item = "almond_open", p->worldId = id;
        if (auto* m = reg_.get<MeshRenderer>(e)) m->model = def->model;
        reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
    }
}

void GameSession::updatePhenomena(double dt) {
    if (!phenomena_.any() && !phenomena_.afflicted()) return;
    phenEvents_.clear();
    const bool auth = netRole != NetRole::Guest;
    auto in = [&](const Player& p, bool alive) {
        Phenomena::PlayerIn r;
        r.feet = {(float)p.x, (float)p.y, (float)p.z};
        r.alive = alive && !ride_.active;
        r.sneaking = p.sneaking;
        r.onPuddle = p.wading || surfaceAt(p.x, p.y) == Surface::Puddle;
        r.inGarden = inGarden(p.x, p.y);
        return r;
    };
    const Phenomena::PlayerIn me = in(player_, !stats_.dead);
    phenomena_.updatePlayer(0, me, dt, auth, phenEvents_);
    if (auth && !stats_.dead) stats_.sanity = std::max(0.0, stats_.sanity - dt * phenomena_.sanityDrain(0));
    if (netRole == NetRole::Host)
        for (Pawn* p : pawns) {
            if (!p || p->id <= 0 || p->id >= Phenomena::kPlayers) continue;
            phenomena_.updatePlayer(p->id, in(p->player, p->alive), dt, true, phenEvents_);
            if (p->alive) p->stats.sanity = std::max(0.0, p->stats.sanity - dt * phenomena_.sanityDrain(p->id));
        }
    // Betrachter fuer die Gemaelde: der eigene Blick und die Mitspieler (updatePresence hat others_ gefuellt)
    std::vector<Viewer> viewers;
    Viewer v;
    v.eye = body_.eye();
    v.look = forwardFromAngles((float)player_.angle, (float)player_.pitch);
    if (!stats_.dead) viewers.push_back(v);
    if (netRole != NetRole::Solo)
        for (const Presence::Other& o : others_) viewers.push_back(o.view);
    phenomena_.updateWorld(dt, viewers, me, auth, *world_, phenEvents_);
    if (netRole == NetRole::Host)  // die Gaeste legen dieselben Bilder an dieselbe Tuer (Variante und Winkel in z)
        for (int i : phenomena_.takeMoves()) {
            const Phenomena::Painting& p = phenomena_.paintings()[(size_t)i];
            const float yawOff = std::remainder(p.yaw - phenomena_.paintingSpots()[(size_t)p.spot].yaw, 6.2831853f);
            const vec3 pos = p.pos + vec3{0, 0, (float)p.variant * 10.0f + std::clamp(yawOff + 0.5f, 0.0f, 1.0f)};
            netWorldOut.push_back({NetWorldEvent::Dropped, Phenomena::paintingHash(i), "painting", pos, 0});
        }
    else
        phenomena_.takeMoves();
    applyPhenomena(phenEvents_);
}

void GameSession::applyPhenomena(std::vector<Phenomena::Event>& evs) {
    for (const Phenomena::Event& e : evs) {
        Pawn* pawn = nullptr;
        if (e.player != 0) {
            for (Pawn* p : pawns)
                if (p && p->id == e.player) pawn = p;
            if (!pawn) continue;
        }
        switch (e.kind) {
            case Phenomena::Event::Toast:
                if (!pawn) events_.push_back({GameEvent::Toast, e.text, e.value});
                break;  // Gaeste rechnen ihre Meldungen selbst
            case Phenomena::Event::Sound2D:
                if (!pawn) {
                    GameEvent ev{GameEvent::SoundAt};
                    ev.sound = e.text, ev.pos = body_.eye(), ev.intensity = e.value;
                    events_.push_back(ev);
                }
                break;
            case Phenomena::Event::SoundAt: {
                GameEvent ev{GameEvent::SoundAt};
                ev.sound = e.text, ev.pos = e.pos, ev.intensity = e.value;
                events_.push_back(ev);
                break;
            }
            case Phenomena::Event::Twitch:
                if (!pawn) player_.angle += e.value, player_.pitch = std::clamp(player_.pitch + (double)e.value2, -1.45, 1.45);
                break;
            case Phenomena::Event::Died:
                if (pawn) {  // Host: der Gast stirbt (die Koop-Schicht meldet es mit der Rolle)
                    pawn->stats.health = 0.0, pawn->stats.dead = true;
                    pawn->killRole = e.text == "garten" ? -3 : -2;
                } else if (!stats_.dead) {
                    stats_.health = 0.0, stats_.dead = true;
                    deathCause_ = e.text;
                    killRole_ = e.text == "garten" ? -3 : -2;
                }
                break;
            case Phenomena::Event::JesterStart:
                if (!pawn) {
                    GameEvent ev{GameEvent::SoundAt};
                    ev.sound = "jester_squeak", ev.pos = e.pos, ev.intensity = 0.7f;
                    events_.push_back(ev);
                }
                break;
            case Phenomena::Event::JesterExpel: {
                Player& pl = pawn ? pawn->player : player_;
                double x = e.pos.x, y = e.pos.y, z = world_->tile(ifloor(e.pos.x), ifloor(e.pos.y)).floor;
                if (phys::findFreeSpot(tileSrc(), x, y, z, pl.shape)) {
                    pl.x = x, pl.y = y, pl.z = z, pl.vx = pl.vy = 0.0;
                    pl.angle = e.value, pl.pitch = 0.0;
                }
                if (!pawn) {
                    body_.reset();
                    events_.push_back({GameEvent::TeleportCut});
                    events_.push_back({GameEvent::Toast, "Rausgeworfen. Irgendwo anders in den Hallen – und das Kichern verhallt.", 4.0f});
                    GameEvent ev{GameEvent::SoundAt};
                    ev.sound = "jester_squeak", ev.pos = body_.eye(), ev.intensity = 0.5f;
                    events_.push_back(ev);
                } else {
                    pawn->body.reset();
                }
                break;
            }
            case Phenomena::Event::Infected:
            case Phenomena::Event::Cured: break;
        }
    }
}

}  // namespace lim::game
