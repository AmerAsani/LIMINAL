// V13.3 Teil 4 Level 5: Wesen des Hotels in der Sitzung (gameplay/hotel_l5.hpp) und was Samantha in der Welt tut (Fleisch
// nehmen, vor die Pfoten legen, fressen, Lesung). KREATUR_Samantha.md, KREATUR_Ants.md, KREATUR_Woodlins.md,
// KREATUR_Watchers.md, KREATUR_Gastgeber.md.
//
// Einzelspieler/Host rechnen alles fuer sich und (Host) fuer die Gaeste; der Host meldet es als Weltereignis Dropped
// "l5hotel" (Kennzahl, Ort). Abzupfen und Handschlag des Gasts kommen als Interact mit fester Kennzahl (Protokoll unveraendert).
#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "gameplay/session.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
const std::string kAntsId = "l5ants:pick", kHandId = "l5host:hand";
bool removeKind(Inventory& inv, const std::string& kind) {
    for (int i = 0; i < Inventory::kSize; ++i)
        if (inv.slots[(size_t)i] && inv.slots[(size_t)i]->kind == kind) return inv.take(i).has_value();
    return false;
}
// Himmelsrichtung von a nach b (Karte: +y ist Sueden wie auf der Karte des Spiels)
std::string compass(double dx, double dy) {
    static const char* k[8] = {"Osten", "Südosten", "Süden", "Südwesten", "Westen", "Nordwesten", "Norden", "Nordosten"};
    const double a = std::atan2(dy, dx);
    int i = (int)std::lround(a / (3.14159265 / 4.0));
    i = ((i % 8) + 8) % 8;
    return k[i];
}
std::string placeName(const std::string& target, const std::string& kind) {
    if (target == "level3") return "der Aufzug, mit dem du gekommen bist";
    if (target == "level188") return "ein Fenster voller weißem Licht";
    if (target == "level903") return "eine Bahn am Ende des heißen Gangs";
    if (target == "level5_3") return "ein langer, alter Gang, der nach Papier riecht";
    if (kind == "elevator") return "ein Aufzug";
    if (kind == "stairs") return "eine Treppe";
    return "eine Tür, die hinausführt";
}
}  // namespace

// Fleisch (Steaks auf Tabletts des Zimmerservice, Dosenfleisch im Kesselraum) - je Chunk
void GameSession::spawnHotel(const world::ChunkData& cd) {
    if (!hotel_.any()) return;
    auto inChunk = [&](vec3 p) {
        const i64 tx = ifloor(p.x), ty = ifloor(p.y);
        return tx >= cd.x0() && tx < cd.x0() + world::CS && ty >= cd.y0() && ty < cd.y0() + world::CS;
    };
    for (const HotelL5::Meat& m : hotel_.meats()) {
        if (!inChunk(m.pos) || isPicked(m.id)) continue;
        const ItemDef* def = content_.item(m.item);
        if (!def) continue;
        ecs::Entity e = prefabs_.spawn(reg_, "item_pickup", m.pos, m.yaw);
        if (e == ecs::kNull) continue;
        if (auto* p = reg_.get<Pickup>(e)) p->item = m.item, p->worldId = m.id;
        if (auto* mr = reg_.get<MeshRenderer>(e)) mr->model = def->model;
        if (auto* t = reg_.get<Transform>(e)) t->scale = (float)def->scale;
        reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
    }
}

void GameSession::updateHotel(double dt) {
    if (!hotel_.any()) return;
    hotelEvents_.clear();
    const bool auth = netRole != NetRole::Guest;
    std::array<HotelL5::PlayerIn, HotelL5::kPlayers> pl{};
    auto corridorAt = [&](double x, double y) {
        const world::Tile* t = world_->tileIfLoaded(ifloor(x), ifloor(y));
        return t && t->room && t->room->type && t->room->type->key == "flur";
    };
    {
        HotelL5::PlayerIn& P = pl[0];
        P.id = netSelfId;
        P.feet = {(float)player_.x, (float)player_.y, (float)player_.z};
        P.eye = body_.eye();
        P.look = forwardFromAngles((float)player_.angle, (float)player_.pitch);
        P.alive = !stats_.dead && !ride_.active;
        const FlashBeam fb = flashBeam();
        P.flash = fb.on, P.flashPos = fb.pos, P.flashDir = fb.dir;
        P.health = stats_.health, P.sanity = stats_.sanity;
        P.corridor = corridorAt(player_.x, player_.y);
        P.view.eye = P.eye, P.view.look = P.look, P.view.flashOn = fb.on, P.view.flashPos = fb.pos, P.view.flashDir = fb.dir;
    }
    for (const Presence::Other& o : others_) {
        const int idx = netRole == NetRole::Guest ? (o.id == 0 ? netSelfId : o.id) : o.id;
        if (idx <= 0 || idx >= HotelL5::kPlayers || !o.player) continue;
        HotelL5::PlayerIn& P = pl[(size_t)idx];
        P.id = o.id;
        P.feet = {(float)o.player->x, (float)o.player->y, (float)o.player->z};
        P.eye = o.view.eye, P.look = o.view.look, P.view = o.view;
        P.flash = o.view.flashOn, P.flashPos = o.view.flashPos, P.flashDir = o.view.flashDir;
        P.alive = true, P.health = o.health, P.sanity = 100.0;
        P.corridor = corridorAt(o.player->x, o.player->y);
        if (netRole == NetRole::Host)
            for (Pawn* p : pawns)
                if (p && p->id == o.id) P.alive = p->alive && !p->stats.dead, P.health = p->stats.health, P.sanity = p->stats.sanity;
    }
    if (presence_.has(Presence::Role::Nest)) hotel_.setNest(presence_.posOf(Presence::Role::Nest));
    hotel_.update(dt, pl, auth, *world_, hotelEvents_);
    applyHotel(hotelEvents_);
    // furchtlos nach dem Handschlag: kein Verstand geht verloren (Wiki: nie wieder Angst - bis die Rechnung kommt)
    if (auth && hotel_.fearless(0) > 0.0 && !stats_.dead) stats_.sanity = std::max(stats_.sanity, 99.0);
    if (netRole == NetRole::Host)
        for (Pawn* p : pawns)
            if (p && p->alive && hotel_.fearless(p->id) > 0.0) p->stats.sanity = std::max(p->stats.sanity, 99.0);
    for (float& s : hotelSlow_) s = std::max(0.0f, s - (float)dt);
    // die Hand ueber dem Schreibtisch zum Benutzen - nur fuer den Gewaehlten (er erscheint nur ihm)
    vec3 hp;
    const bool want = hotel_.host().chosen == 0 && hotel_.handAt(hp);
    if (want && hotelHand_ == ecs::kNull) {
        hotelHand_ = reg_.create();
        reg_.add<Transform>(hotelHand_, Transform{hp, hotel_.officeYaw(), 1.0f});
        reg_.add<Interactable>(hotelHand_, Interactable{"Die Hand nehmen", 2.4f, 50.0f});
        reg_.add<ApparitionUse>(hotelHand_, ApparitionUse{10, 0, kHandId});
    } else if (!want && hotelHand_ != ecs::kNull) {
        if (target_ == hotelHand_) target_ = ecs::kNull;
        reg_.destroy(hotelHand_);
        hotelHand_ = ecs::kNull;
    }
}

void GameSession::applyHotel(std::vector<HotelL5::Event>& evs) {
    for (const HotelL5::Event& e : evs) {
        Pawn* pawn = nullptr;
        const bool self = e.player == 0;
        if (netRole == NetRole::Host && e.player != 0)
            for (Pawn* p : pawns)
                if (p && p->id == e.player) pawn = p;
        auto tell = [&](const std::string& msg, float secs) {
            if (self) events_.push_back({GameEvent::Toast, msg, secs});
            else if (pawn) netPersonalOut.push_back({pawn->id, msg});
        };
        switch (e.kind) {
            case HotelL5::Event::Toast: tell(e.text, e.value); break;
            case HotelL5::Event::SoundAt: {
                if (!self && e.text.rfind("e18_", 0) == 0) break;  // die Stimme hoert nur der Gewaehlte
                GameEvent ev{GameEvent::SoundAt};
                ev.sound = e.text, ev.pos = e.pos, ev.intensity = e.value;
                events_.push_back(ev);
                if (netRole == NetRole::Host && (e.text == "wl_bite" || e.text == "wa_strike")) netSoundOut.push_back({e.text, e.pos, e.value, -1});
                break;
            }
            case HotelL5::Event::Sound2d:
                if (self) {
                    GameEvent ev{GameEvent::SoundAt};
                    ev.sound = e.text, ev.pos = body_.eye(), ev.intensity = e.value;
                    events_.push_back(ev);
                }
                break;
            case HotelL5::Event::Hurt: {
                const double floor = e.text == "ants" ? hotel_.config().antFloor : hotel_.config().floor;  // nie toedlich
                auto hurt = [&](double& h) {
                    if (h > floor) h = std::max(floor, h - (double)e.value * (e.text == "ants" ? 1.0 : hitScale_));
                };
                if (pawn) hurt(pawn->stats.health);
                else if (self && netRole != NetRole::Guest && !stats_.dead) {
                    hurt(stats_.health);
                    if (e.text != "ants") {
                        GameEvent ev{GameEvent::Figure};  // Treffer: roter Rand wie bei den Wesen
                        ev.index = (int)Presence::Event::Hit, ev.role = (int)Presence::Role::Samantha, ev.pos = body_.eye();
                        ev.intensity = e.value;
                        events_.push_back(ev);
                    }
                }
                break;
            }
            case HotelL5::Event::Sanity: {
                auto drain = [&](double& s) { s = e.text == "bill" ? std::min(s, 20.0) : std::max(0.0, s - (double)e.value); };
                if (pawn) drain(pawn->stats.sanity);
                else if (self && netRole != NetRole::Guest) drain(stats_.sanity);
                break;
            }
            case HotelL5::Event::Slow:
                if (e.player >= 0 && e.player < HotelL5::kPlayers) hotelSlow_[(size_t)e.player] = e.value;
                break;
            case HotelL5::Event::Deal: {  // ein paar Dinge gegen nie wieder Angst
                Inventory* inv = self ? &inventory_ : (pawn ? &pawn->inventory : nullptr);
                std::vector<std::string> took;
                for (int n = 0; inv && n < (int)e.value; ++n) {
                    int best = -1;
                    double bv = 0.0;
                    for (int i = 0; i < Inventory::kSize; ++i) {
                        const auto& s = inv->slots[(size_t)i];
                        if (!s) continue;
                        const ItemDef* d = content_.item(s->kind);
                        if (!d || d->key.rfind("fuse", 0) == 0 || d->key == "hatchet") continue;
                        const double v = d->sanity + d->health + d->battery * 50.0 + d->stamina + 1.0;
                        if (v > bv) bv = v, best = i;
                    }
                    if (best < 0) break;
                    const ItemDef* d = content_.item(inv->slots[(size_t)best]->kind);
                    took.push_back(d ? d->name : inv->slots[(size_t)best]->kind);
                    inv->take(best);
                }
                if (self) stats_.sanity = 100.0;
                else if (pawn) pawn->stats.sanity = 100.0;
                std::string msg = "Eine kühle, trockene Hand. Die Angst ist fort – einfach fort.";
                if (!took.empty()) {
                    msg += " Er hat sich genommen: ";
                    for (size_t i = 0; i < took.size(); ++i) msg += (i ? ", " : "") + took[i];
                    msg += ".";
                }
                tell(msg, 5.0f);
                log::info("Hotel: Handschlag (Spieler {}), {} Dinge genommen", e.player, took.size());
                break;
            }
            case HotelL5::Event::Net:
                if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Dropped, e.code, HotelL5::kNetName, e.pos, 0});
                break;
        }
    }
}

// Koop (Gast): Ereignis des Hosts (Weltereignis Dropped "l5hotel")
void GameSession::netHotel(u32 code, vec3 pos) {
    std::vector<HotelL5::Event> ev;
    hotel_.applyNet(code, pos, netSelfId, ev);
    for (const HotelL5::Event& e : ev) {
        if (e.player != 0) continue;
        if (e.kind == HotelL5::Event::Toast) events_.push_back({GameEvent::Toast, e.text, e.value});
        else if (e.kind == HotelL5::Event::SoundAt) {
            GameEvent g{GameEvent::SoundAt};
            g.sound = e.text, g.pos = e.pos, g.intensity = e.value;
            events_.push_back(g);
        }
    }
}

// Abzupfen (Benutzen ohne Ziel, Tierchen am eigenen Koerper)
bool GameSession::hotelPick() {
    if (hotel_.antsOn(0) <= 0 || stats_.dead) return false;
    body_.gesture(Body::Gesture::Grab, body_.eye() + forwardFromAngles((float)player_.angle, -1.0f) * 0.4f - vec3{0, 0, 0.5f}, "", 0.0f, 1.0f);
    if (netAction) {  // Gast: der Host zupft (fuer alle sichtbar), hier gleich der Klang
        netAction(1, worldItemHash(kAntsId));
        GameEvent g{GameEvent::SoundAt};
        g.sound = "ant_pick", g.pos = body_.eye(), g.intensity = 0.8f;
        events_.push_back(g);
        return true;
    }
    std::vector<HotelL5::Event> ev;
    hotel_.pickAnts(0, ev);
    applyHotel(ev);
    return true;
}

bool GameSession::hotelInteractPawn(Pawn& p, u32 hash, std::string& msg) {
    if (hash == worldItemHash(kAntsId)) {
        std::vector<HotelL5::Event> ev;
        const bool ok = hotel_.pickAnts(p.id, ev);
        applyHotel(ev);
        return ok;
    }
    if (hash == worldItemHash(kHandId)) {
        if (std::hypot(hotel_.officeDesk().x - (float)p.player.x, hotel_.officeDesk().y - (float)p.player.y) > 3.5f) return false;
        std::vector<HotelL5::Event> ev;
        const bool ok = hotel_.shake(p.id, ev);
        applyHotel(ev);
        if (!ok) msg.clear();
        return ok;
    }
    return false;
}

bool GameSession::useHotel(const ApparitionUse& au, Pawn* p, std::string& msg) {
    if (au.kind != 10) return false;
    if (p) return hotelInteractPawn(*p, worldItemHash(kHandId), msg);
    std::vector<HotelL5::Event> ev;
    const bool ok = hotel_.shake(0, ev);
    applyHotel(ev);
    return ok;
}

bool GameSession::hotelForce(const std::string& what, int nr) {
    std::vector<HotelL5::Event> ev;
    const bool ok = hotel_.force(what, nr, 0, ev);
    applyHotel(ev);
    return ok;
}

float GameSession::hotelSlow(int id) const { return id >= 0 && id < HotelL5::kPlayers && hotelSlow_[(size_t)id] > 0.0f ? 0.55f : 1.0f; }

// --- Samantha in der Welt (Host/Einzelspieler; KREATUR_Samantha.md) ------------------------------------------------------
void GameSession::samanthaWorld(const Presence::Event& e) {
    if (e.role != Presence::Role::Samantha) return;
    using PE = Presence::Event;
    const bool self = e.target == netSelfId;
    Pawn* pw = nullptr;
    if (!self)
        for (Pawn* q : pawns) pw = q && q->id == e.target ? q : pw;
    auto tell = [&](const std::string& msg, float secs) {
        if (self) events_.push_back({GameEvent::Toast, msg, secs});
        else if (pw) netPersonalOut.push_back({pw->id, msg});
    };
    switch (e.type) {
        case PE::Steal: {  // sie nimmt das Stueck (aus der Hand, aus dem Rucksack - sie weiss, wo es ist)
            Inventory* inv = self ? &inventory_ : (pw ? &pw->inventory : nullptr);
            if (inv && removeKind(*inv, e.item)) {
                const ItemDef* d = content_.item(e.item);
                tell(std::format("Die Katze zieht dir {} aus dem Rucksack und legt es sich vor die Pfoten.", d ? d->name : e.item), 3.2f);
            }
            break;
        }
        case PE::Spill: spawnLooseAt(e.item, e.pos); break;
        case PE::Take: {  // gefressen: das Stueck vor ihr verschwindet
            ecs::Entity best = ecs::kNull;
            float bd = 0.8f;
            reg_.each<Pickup, Transform>([&](ecs::Entity en, Pickup& pk, Transform& t) {
                if (pk.item != e.item) return;
                const float dd = length(t.pos - e.pos);
                if (dd < bd) bd = dd, best = en;
            });
            if (best == ecs::kNull) break;
            auto* pk = reg_.get<Pickup>(best);
            const Transform* t = reg_.get<Transform>(best);
            u32 hash = worldItemHash(pk->worldId);
            for (const DroppedItem& x : dropped_)
                if (x.id == pk->worldId && x.netHash) hash = x.netHash;
            std::erase_if(dropped_, [&](const DroppedItem& x) { return x.id == pk->worldId; });
            if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Picked, hash, pk->item, t ? t->pos : e.pos, -1});
            if (target_ == best) target_ = ecs::kNull;
            reg_.destroy(best);
            break;
        }
        case PE::Gift: {  // die Lesung: Guete nach dem Fleisch (Wiki) - Wahres ueber den Weg hinaus und die Gefahr
            const Player& P = self ? player_ : (pw ? pw->player : player_);
            const world::LevelDef& L = world_->level();
            const world::MapTransition* best = nullptr;
            double bd = 1e9;
            if (L.mapped())
                for (const auto& tr : L.map->transitions) {
                    if (tr.target.empty()) continue;
                    const double d = std::hypot(tr.x - P.x, tr.y - P.y);
                    if (d < bd) bd = d, best = &tr;
                }
            std::string msg;
            if (e.strength >= 0.7f) {
                msg = "Sie spricht leise, ohne zu blinzeln: ";
                if (best) {
                    std::string place = placeName(best->target, best->kind);
                    if (!place.empty()) place[0] = (char)std::toupper((unsigned char)place[0]);
                    msg += std::format("„{} – im {}, gut {} Schritte von hier.", place,
                                       compass(best->x - P.x, best->y - P.y), (int)std::lround(bd * 1.3 / 10.0) * 10);
                    if (presence_.has(Presence::Role::Nest)) {
                        const vec3 n = presence_.posOf(Presence::Role::Nest);
                        msg += std::format(" Meide die Tür im {} – dort brüten sie.", compass(n.x - P.x, n.y - P.y));
                    }
                    msg += " Und du hast mehr Angst, als du zugibst.“";
                    if (self) explore_.reveal(*world_, best->x, best->y, P.z, 0.0, true);  // man sieht es nun auf der Karte
                } else {
                    msg += "„Hier hinaus führt kein Weg, den du gehen willst. Bleib in den Gängen.“";
                }
            } else {
                msg = "Sie verzieht das Maul: ";
                msg += best ? std::format("„Billiges Zeug. Irgendwo im {}. Mehr ist das nicht wert.“", compass(best->x - P.x, best->y - P.y))
                            : "„Billiges Zeug. Mehr ist das nicht wert.“";
            }
            tell(msg, e.strength >= 0.7f ? 7.5f : 4.5f);
            log::info("Samantha: Lesung (Guete {:.1f}) fuer Spieler {}", e.strength, e.target);
            break;
        }
        default: break;
    }
}

// Skript "hotel" (Bilder, Tests): vor ein Wesen des Hotels stellen und es ansehen
bool GameSession::scriptHotel(const std::string& name, double dist, double side, int nr) {
    vec3 at;
    float yaw = 0.0f, aim = 0.3f;
    bool wall = false;
    if (name == "colony") {
        const auto& c = hotel_.colonies();
        if (c.empty()) return false;
        const size_t i = nr >= 0 && (size_t)nr < c.size() ? (size_t)nr : 0;
        at = c[i].pos, yaw = c[i].yaw, aim = 0.08f;
    } else if (name == "watcher") {
        const auto& w = hotel_.watchers();
        if (w.empty()) return false;
        const size_t i = nr >= 0 && (size_t)nr < w.size() ? (size_t)nr : 0;
        const auto& S = hotel_.spots("watcher")[(size_t)w[i].spot];
        at = S.pos + vec3{-std::sin(S.yaw), std::cos(S.yaw), 0.0f} * (0.5f * (float)w[i].side), yaw = S.yaw, aim = 1.8f;
    } else if (name == "woodlin") {
        const auto& s = hotel_.spots("woodlin");
        if (s.empty()) return false;
        const size_t i = nr >= 0 && (size_t)nr < s.size() ? (size_t)nr : 0;
        at = s[i].pos, yaw = s[i].yaw, aim = 0.25f;
    } else if (name == "portrait") {
        float y = 0.0f;
        if (!hotel_.portraitNear(vec3{(float)player_.x, (float)player_.y, 0.0f}, at, y)) return false;
        yaw = y, aim = 0.12f, wall = true;  // at.z: Hoehe des Bildes (1,75 m ueber dem Boden der Gaenge)
    } else if (name == "office") {
        if (!hotel_.hasOffice()) return false;
        at = hotel_.officeDesk(), yaw = hotel_.officeYaw(), aim = 0.95f;
    } else {
        return false;
    }
    for (int k = 0; k < 13; ++k) {
        const float y0 = yaw + (float)side + (k == 0 ? 0.0f : (float)((k + 1) / 2) * 0.4f * (k % 2 ? 1.0f : -1.0f));
        for (double dd = dist; dd > 0.5; dd -= 0.25) {
            const double x = at.x + std::cos(y0) * dd, y = at.y + std::sin(y0) * dd;
            const world::Tile& t = world_->tile(ifloor(x), ifloor(y));
            if (!t.open() || std::fabs(t.floor - (wall ? at.z - 1.75f : at.z)) > 1.2f) continue;
            const vec3 eye{(float)x, (float)y, (float)t.floor + (float)Player::kEye};
            const vec3 to = at + vec3{0, 0, aim};
            if (!clearSight(*world_, eye, to)) continue;
            player_.x = x, player_.y = y, player_.z = t.floor, player_.vx = player_.vy = 0.0;
            player_.angle = std::atan2(at.y - y, at.x - x);
            player_.pitch = std::atan2(to.z - eye.z, (float)dd);
            body_.reset();
            log::info("Skript: hotel {} bei ({:.1f}, {:.1f}), Abstand {:.2f}", name, x, y, dd);
            return true;
        }
    }
    return false;
}

}  // namespace lim::game
