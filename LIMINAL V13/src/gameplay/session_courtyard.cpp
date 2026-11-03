// V13.3 Teil 4 Level 188: der Hof in der Sitzung (gameplay/courtyard_l188.hpp; KREATUR_Wurzelpistole.md, KREATUR_Horcher.md).
//
//   Wurzelpistolen  je eine Stelle zum Anfassen ("Herausziehen", Benutzen-Art 12); Griff, Zuege, Schuss, Losreissen rechnet der
//                   Host (Einzelspieler: dieser Rechner), die Gaeste bekommen sie als Weltereignis "l188".
//   Laerm           der Schuss ist ein Laermereignis fuer die Wesen (der Horcher hoert ihn in den Gaengen).
//   Probenzettel    ein Fund (Notiz mit mode "fund") an der Stelle trace/e39/note der Karte.
//   Horcher         die Orte aus der Karte (trace/hr: knock, emerge) gehen einmal an die Praesenz (presence_listener.cpp).
// Nie toedlich: die Pistole nimmt Blut, laesst aber mindestens 5 Lebenspunkte (Seite: fuer Gesunde nur leichte Muedigkeit).
#include <algorithm>
#include <cmath>

#include "core/log.hpp"
#include "gameplay/session.hpp"
#include "world/world.hpp"

namespace lim::game {

void GameSession::spawnCourtyard(const world::ChunkData& cd) {
    if (!court188_.any()) return;
    auto inChunk = [&](vec3 p) {
        const i64 tx = ifloor(p.x), ty = ifloor(p.y);
        return tx >= cd.x0() && tx < cd.x0() + world::CS && ty >= cd.y0() && ty < cd.y0() + world::CS;
    };
    const auto& guns = court188_.guns();
    for (size_t i = 0; i < guns.size(); ++i) {
        if (!inChunk(guns[i].pos)) continue;
        ecs::Entity e = reg_.create();
        reg_.add<Transform>(e, Transform{guns[i].pos + vec3{0, 0, 0.12f}, guns[i].yaw, 1.0f});
        reg_.add<Interactable>(e, Interactable{"Herausziehen", 1.8f, 50.0f});
        reg_.add<ApparitionUse>(e, ApparitionUse{12, (int)i, guns[i].id});
        reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
    }
    vec3 np;
    float ny = 0.0f;
    if (court188_.noteAt(np, ny) && inChunk(np))
        libraryNote(np, ny, Courtyard188::kNote, "trace:l188_note", "prop:e39_note", "Zettel lesen");
}

void GameSession::updateCourtyard(double dt) {
    if (!court188_.any()) return;
    if (!court188_.knockSpots().empty())  // einmal je Ebene (die Praesenz merkt sich die Orte)
        presence_.listenerSpots(court188_.knockSpots(), court188_.emergeSpots(), court188_.emergeYaws());
    courtEvents_.clear();
    const bool auth = netRole != NetRole::Guest;
    std::array<Courtyard188::PlayerIn, Courtyard188::kPlayers> pl{};
    pl[0].feet = {(float)player_.x, (float)player_.y, (float)player_.z};
    pl[0].eye = body_.eye();
    pl[0].alive = !stats_.dead && !ride_.active;
    for (const Presence::Other& o : others_) {
        const int idx = netRole == NetRole::Guest ? (o.id == 0 ? netSelfId : o.id) : o.id;
        if (idx <= 0 || idx >= Courtyard188::kPlayers || !o.player) continue;
        Courtyard188::PlayerIn& P = pl[(size_t)idx];
        P.feet = {(float)o.player->x, (float)o.player->y, (float)o.player->z};
        P.eye = o.view.eye;
        P.alive = true;
        if (netRole == NetRole::Host)
            for (Pawn* p : pawns)
                if (p && p->id == o.id) P.alive = p->alive && !p->stats.dead;
    }
    if (netRole == NetRole::Guest && netSelfId > 0 && netSelfId < Courtyard188::kPlayers) {  // beim Gast ist Index 0 der Host
        std::swap(pl[0], pl[(size_t)netSelfId]);
    }
    court188_.update(dt, pl, auth, courtEvents_);
    applyCourtyard(courtEvents_);
}

void GameSession::applyCourtyard(std::vector<Courtyard188::Event>& evs) {
    for (const Courtyard188::Event& e : evs) {
        Pawn* pawn = nullptr;
        const bool self = e.player == (netRole == NetRole::Guest ? netSelfId : 0);
        if (netRole == NetRole::Host && e.player != 0)
            for (Pawn* p : pawns)
                if (p && p->id == e.player) pawn = p;
        auto tell = [&](const std::string& msg, float secs) {
            if (self) events_.push_back({GameEvent::Toast, msg, secs});
            else if (pawn) netPersonalOut.push_back({pawn->id, msg});
        };
        switch (e.kind) {
            case Courtyard188::Event::Toast: tell(e.text, e.value); break;
            case Courtyard188::Event::SoundAt: {
                GameEvent ev{GameEvent::SoundAt};
                ev.sound = e.text, ev.pos = e.pos, ev.intensity = e.value;
                events_.push_back(ev);
                break;
            }
            case Courtyard188::Event::Hurt: {
                const double amount = (double)e.value * (e.text == "tear" ? hitScale_ : 1.0);
                auto hurt = [&](double& h) { h = std::max(std::min(h, 5.0), h - amount); };  // nie toedlich
                if (pawn) hurt(pawn->stats.health);
                else if (self && netRole != NetRole::Guest && !stats_.dead) {
                    hurt(stats_.health);
                    GameEvent ev{GameEvent::Figure};  // roter Rand wie bei den Wesen
                    ev.index = (int)Presence::Event::Hit, ev.role = (int)Presence::Role::Listener, ev.pos = body_.eye();
                    ev.intensity = e.value;
                    events_.push_back(ev);
                }
                break;
            }
            case Courtyard188::Event::Tired: break;  // court188_ merkt es sich (Tempo in session.cpp / pawnTick)
            case Courtyard188::Event::Noise:
                if (netRole != NetRole::Guest) noisePings_.push_back({e.pos, e.value});
                break;
            case Courtyard188::Event::Net:
                if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Dropped, e.code, Courtyard188::kNetName, e.pos, 0});
                break;
        }
    }
}

// Koop (Gast): Ereignis des Hosts (Weltereignis Dropped "l188")
void GameSession::netCourtyard(u32 code, vec3 pos) {
    std::vector<Courtyard188::Event> ev;
    court188_.applyNet(code, pos, ev);
    for (const Courtyard188::Event& e : ev) {
        if (e.kind != Courtyard188::Event::SoundAt) continue;
        GameEvent g{GameEvent::SoundAt};
        g.sound = e.text, g.pos = e.pos, g.intensity = e.value;
        events_.push_back(g);
    }
}

bool GameSession::useCourtyard(const ApparitionUse& au, Pawn* p, std::string& msg) {
    std::vector<Courtyard188::Event> ev;
    const int who = p ? p->id : 0;
    const vec3 eye = p ? vec3{(float)p->player.x, (float)p->player.y, (float)p->player.z + 1.6f} : body_.eye();
    const bool ok = court188_.touch(au.index, who, eye, msg, ev);
    applyCourtyard(ev);
    return ok;
}

}  // namespace lim::game
