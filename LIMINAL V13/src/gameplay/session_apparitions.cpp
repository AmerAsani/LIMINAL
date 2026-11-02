// V13.3 Teil 4: Randwesen von Level 2 in der Sitzung (gameplay/apparitions_l2.hpp; KREATUR_CannibalCuisine.md,
// KREATUR_Swindlebird.md, KREATUR_Photoshop.md, KREATUR_Macchina.md).
//
// Einzelspieler/Host rechnen Ware, Hiebe, Sichtung, Box, Tod und Gabe fuer sich und (Host) fuer die Gaeste; der Host
// meldet sie als Weltereignis Dropped "l2rand" (Kennzahl, Ort). Der Gast rechnet daraus und aus derselben Karte nur Bild
// und Klang; seinen Tod meldet der Host mit der Rolle -4 (Photoshop) bzw. -5 (Automat). Protokoll unveraendert.
#include <algorithm>
#include <cmath>
#include <format>

#include "core/log.hpp"
#include "gameplay/navfield.hpp"
#include "gameplay/session.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
int fundNote(const Content& c, const char* title) {
    const auto& n = c.notes();
    for (size_t i = 0; i < n.size(); ++i)
        if (n[i].title == title) return (int)i;
    return -1;
}
}  // namespace

// Automaten (Tastenfeld zum Benutzen, Brummen) und die Blechtafel der Macchina (Notiz) - je Chunk
void GameSession::spawnApparitions(const world::ChunkData& cd) {
    if (!appar_.any()) return;
    auto inChunk = [&](vec3 p) {
        const i64 tx = ifloor(p.x), ty = ifloor(p.y);
        return tx >= cd.x0() && tx < cd.x0() + world::CS && ty >= cd.y0() && ty < cd.y0() + world::CS;
    };
    const auto& ccs = appar_.cuisines();
    for (size_t i = 0; i < ccs.size(); ++i) {
        const Apparitions::Cuisine& c = ccs[i];
        if (!inChunk(c.pos)) continue;
        const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
        // Bedienen: vor der Vorderseite (ausserhalb des Blocks, der ihn fest stehen laesst)
        ecs::Entity e = reg_.create();
        reg_.add<Transform>(e, Transform{c.pos + f * (Apparitions::kCcD * 0.5f + 0.27f), c.yaw, 1.0f});
        reg_.add<Interactable>(e, Interactable{"Automat: eine Taste drücken", 1.9f, 50.0f});
        reg_.add<ApparitionUse>(e, ApparitionUse{0, (int)i, c.id});
        reg_.add<ChunkOwned>(e, ChunkOwned{cd.key});
        // Brummen: Kompressor und Leuchtroehre, darunter leises Murmeln (synth_l2_rand.cpp)
        ecs::Entity h = reg_.create();
        reg_.add<Transform>(h, Transform{c.pos + vec3{0, 0, 0.5f}, c.yaw, 1.0f});
        reg_.add<AudioEmitter>(h, AudioEmitter{"cc_hum", 0.5f, 11.0f, -1});
        reg_.add<ChunkOwned>(h, ChunkOwned{cd.key});
    }
    static const char* kPlate = "Blechtafel mit Zeichen";  // Notiz mit mode "fund" (nur beim Anzug)
    const int note = fundNote(content_, kPlate);
    for (const Apparitions::Relic& r : appar_.relics()) {
        if (!inChunk(r.pos)) continue;
        const vec3 f{std::cos(r.yaw), std::sin(r.yaw), 0.0f};
        ecs::Entity h = reg_.create();  // Luftzug im hohlen Anzug
        reg_.add<Transform>(h, Transform{r.pos + vec3{0, 0, 0.9f}, r.yaw, 1.0f});
        reg_.add<AudioEmitter>(h, AudioEmitter{"mc_hollow", 0.45f, 7.0f, -1});
        reg_.add<ChunkOwned>(h, ChunkOwned{cd.key});
        const std::string id = "trace:" + r.id;
        if (note < 0 || isPicked(id)) continue;
        ecs::Entity n = prefabs_.spawn(reg_, "note", r.pos + f * 0.55f + vec3{-f.y, f.x, 0.0f} * 0.25f, r.yaw + 0.4f);
        if (n == ecs::kNull) continue;
        if (auto* nf = reg_.get<NoteFind>(n)) nf->index = note, nf->id = id;
        if (auto* m = reg_.get<MeshRenderer>(n)) m->model = "prop:macchina_plate";
        if (auto* in = reg_.get<Interactable>(n)) in->prompt = "Blechtafel mit Zeichen ansehen";
        reg_.add<ChunkOwned>(n, ChunkOwned{cd.key});
    }
}

bool GameSession::useApparition(const ApparitionUse& au, Pawn* p, std::string& msg) {
    if (au.kind != 0) return false;
    if (!appar_.canVend(au.index, msg)) {
        std::vector<Apparitions::Event> ev;
        appar_.vend(au.index, p ? p->id : 0, msg, ev);  // leer: Fluestern
        applyApparitions(ev);
        return false;
    }
    Inventory& inv = p ? p->inventory : inventory_;
    if (!inv.add("cc_ware")) {
        msg = "Inventar voll";
        return false;
    }
    std::vector<Apparitions::Event> ev;
    appar_.vend(au.index, p ? p->id : 0, msg, ev);
    applyApparitions(ev);
    return true;
}

void GameSession::updateApparitions(double dt) {
    if (!appar_.any() && !appar_.addicted(0)) return;
    apparEvents_.clear();
    const bool auth = netRole != NetRole::Guest;
    std::array<Apparitions::PlayerIn, Apparitions::kPlayers> pl{};
    auto in = [](const Player& p, vec3 eye, bool alive) {
        Apparitions::PlayerIn r;
        r.feet = {(float)p.x, (float)p.y, (float)p.z};
        r.eye = eye;
        r.look = forwardFromAngles((float)p.angle, (float)p.pitch);
        r.alive = alive;
        return r;
    };
    pl[0] = in(player_, body_.eye(), !stats_.dead && !ride_.active);
    if (netRole == NetRole::Host)
        for (Pawn* p : pawns)
            if (p && p->id > 0 && p->id < Apparitions::kPlayers) pl[(size_t)p->id] = in(p->player, p->eye(), p->alive);
    std::vector<Viewer> viewers;
    if (!stats_.dead) {
        Viewer v;
        v.eye = body_.eye();
        v.look = pl[0].look;
        const FlashBeam fb = flashBeam();
        v.flashOn = fb.on, v.flashPos = fb.pos, v.flashDir = fb.dir;
        viewers.push_back(v);
    }
    if (netRole != NetRole::Solo)
        for (const Presence::Other& o : others_) viewers.push_back(o.view);
    appar_.update(dt, pl, viewers, auth, *world_, apparEvents_);
    // Entzug (Sucht nach der Ware): der Verstand schwindet schneller
    if (auth && !stats_.dead) stats_.sanity = std::max(0.0, stats_.sanity - dt * appar_.sanityDrain(0));
    if (netRole == NetRole::Host)
        for (Pawn* p : pawns)
            if (p && p->alive && p->id > 0 && p->id < Apparitions::kPlayers)
                p->stats.sanity = std::max(0.0, p->stats.sanity - dt * appar_.sanityDrain(p->id));
    // Photoshop: ein hingeworfener Gegenstand vor ihm - der Arm holt ihn (Host entscheidet, die Gaeste sehen es)
    vec3 c;
    float r = 0.0f;
    if (auth && appar_.wantsOffering(c, r)) {
        ecs::Entity found = ecs::kNull;
        float best = r;
        reg_.each<Pickup, Transform>([&](ecs::Entity e, Pickup& pk, Transform& t) {
            if (pk.worldId.rfind("drop:", 0) != 0) return;
            const float d = std::hypot(t.pos.x - c.x, t.pos.y - c.y);
            if (d < best && std::fabs(t.pos.z - c.z) < 1.2f) best = d, found = e;
        });
        if (found != ecs::kNull) {
            const Pickup pk = *reg_.get<Pickup>(found);
            const vec3 pos = reg_.get<Transform>(found)->pos;
            const ItemDef* def = content_.item(pk.item);
            const std::string model = def ? def->model : pk.item;
            appar_.offered(pos, model, apparEvents_);
            const u32 hash = worldItemHash(pk.worldId);
            std::erase_if(dropped_, [&](const DroppedItem& x) { return x.id == pk.worldId; });
            if (netRole == NetRole::Host) {
                netWorldOut.push_back({NetWorldEvent::Dropped, (hash & 0x00FFFFFFu) | (7u << 24), Apparitions::kNetName, pos, 0});
                netWorldOut.push_back({NetWorldEvent::Picked, hash, pk.item, pos, 0});
            }
            if (target_ == found) target_ = ecs::kNull;
            reg_.destroy(found);
            log::info("Photoshop: Gabe {} genommen", pk.item);
        }
    }
    applyApparitions(apparEvents_);
}

void GameSession::applyApparitions(std::vector<Apparitions::Event>& evs) {
    for (const Apparitions::Event& e : evs) {
        Pawn* pawn = nullptr;
        bool self = true;
        if (netRole == NetRole::Host && e.player != 0) {
            for (Pawn* p : pawns)
                if (p && p->id == e.player) pawn = p;
            self = false;
        }
        switch (e.kind) {
            case Apparitions::Event::Toast:
                if (self && e.player == 0) events_.push_back({GameEvent::Toast, e.text, e.value});
                else if (pawn) netPersonalOut.push_back({pawn->id, e.text});
                break;
            case Apparitions::Event::SoundAt: {
                GameEvent ev{GameEvent::SoundAt};
                ev.sound = e.text, ev.pos = e.pos, ev.intensity = e.value;
                events_.push_back(ev);
                break;
            }
            case Apparitions::Event::Hurt:
            case Apparitions::Event::Died: {
                const bool died = e.kind == Apparitions::Event::Died;
                const int role = e.text == "photoshop" ? -4 : -5;
                if (pawn) {
                    if (!pawn->alive || pawn->stats.dead) break;
                    pawn->stats.health = died ? 0.0 : std::max(0.0, pawn->stats.health - (double)e.value);
                    if (pawn->stats.health <= 0.0) pawn->stats.dead = true, pawn->killRole = role;
                } else if (self && netRole != NetRole::Guest && !stats_.dead) {
                    stats_.health = died ? 0.0 : std::max(0.0, stats_.health - (double)e.value);
                    if (!died) events_.push_back({GameEvent::Toast, "Ein Knochenarm trifft dich.", 1.6f});
                    if (stats_.health <= 0.0) stats_.dead = true, deathCause_ = e.text, killRole_ = role;
                }
                break;
            }
            case Apparitions::Event::Net:
                if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Dropped, e.code, Apparitions::kNetName, e.pos, 0});
                break;
        }
    }
}

// Koop (Gast): Ereignis des Hosts (Weltereignis Dropped "l2rand")
void GameSession::netApparition(u32 code, vec3 pos) {
    std::string model;
    if (Apparitions::netKind(code) == 7) {  // Gabe an Photoshop: welcher Gegenstand lag dort?
        float best = 0.8f;
        reg_.each<Pickup, Transform>([&](ecs::Entity, Pickup& pk, Transform& t) {
            const float d = length(t.pos - pos);
            if (d < best) {
                best = d;
                const ItemDef* def = content_.item(pk.item);
                model = def ? def->model : pk.item;
            }
        });
        if (model.empty()) model = "almond";
    }
    std::vector<Apparitions::Event> ev;
    appar_.applyNet(code, pos, model, ev);
    for (Apparitions::Event& e : ev) {
        if (e.kind == Apparitions::Event::Toast) {
            if (e.player != netSelfId) continue;
            e.player = 0;
        }
        if (e.kind == Apparitions::Event::Net || e.kind == Apparitions::Event::Hurt || e.kind == Apparitions::Event::Died) continue;
        applyApparitionsLocal(e);
    }
}

// Skript "rand" (Bilder, Tests): vor das Randwesen stellen und es ansehen; Swindlebird und Photoshop werden erzwungen
bool GameSession::scriptApparition(const std::string& name, double dist, double side, int nr) {
    vec3 at;
    float yaw = 0.0f, aim = 1.0f;
    auto pick = [&](size_t n) { return nr >= 0 && (size_t)nr < n ? (size_t)nr : n; };
    if (name == "cuisine") {
        const size_t n = appar_.cuisines().size();
        size_t i = pick(n);
        if (i == n) {  // der naechste
            float bd = 1e9f;
            for (size_t k = 0; k < n; ++k)
                if (const float d = std::hypot(appar_.cuisines()[k].pos.x - (float)player_.x, appar_.cuisines()[k].pos.y - (float)player_.y); d < bd)
                    bd = d, i = k;
        }
        if (i >= n) return false;
        at = appar_.cuisines()[i].pos, yaw = appar_.cuisines()[i].yaw, aim = 1.05f;
    } else if (name == "swindle") {
        const size_t n = appar_.swindleSpots().size();
        const size_t i = pick(n) == n ? 0 : pick(n);
        if (i >= n) return false;
        appar_.forceBird((int)i);
        at = appar_.swindleSpots()[i], yaw = appar_.swindleYaws()[i], aim = 0.25f;
    } else if (name == "photoshop") {
        const int n = appar_.shopSpots();
        const int i = nr >= 0 && nr < n ? nr : (appar_.shop().present ? appar_.shop().spot : 0);
        if (i >= n) return false;
        appar_.forceShop(i);
        at = appar_.shop().pos, yaw = appar_.shop().yaw, aim = 1.45f;
    } else if (name == "macchina") {
        const size_t n = appar_.relics().size();
        const size_t i = pick(n) == n ? 0 : pick(n);
        if (i >= n) return false;
        at = appar_.relics()[i].pos, yaw = appar_.relics()[i].yaw, aim = 0.8f;
    } else {
        return false;
    }
    for (int k = 0; k < 13; ++k) {
        const float y0 = yaw + (float)side + (k == 0 ? 0.0f : (float)((k + 1) / 2) * 0.45f * (k % 2 ? 1.0f : -1.0f));
        const double minD = name == "photoshop" ? appar_.config().boxRange + 0.75 : 0.6;  // nicht in seine Box stellen
        for (double dd = dist; dd > minD; dd -= 0.25) {
            const double x = at.x + std::cos(y0) * dd, y = at.y + std::sin(y0) * dd;
            const world::Tile& t = world_->tile(ifloor(x), ifloor(y));
            if (!t.open() || std::fabs(t.floor - at.z) > 0.6f) continue;
            const vec3 eye{(float)x, (float)y, (float)t.floor + (float)Player::kEye};
            // freie Sicht bis vor das Wesen (der Automat steht selbst auf einem Block)
            const vec3 sightTo = at + vec3{std::cos(yaw), std::sin(yaw), 0.0f} * (name == "cuisine" ? 0.75f : 0.0f) + vec3{0, 0, aim};
            if (!clearSight(*world_, eye, sightTo)) continue;
            player_.x = x, player_.y = y, player_.z = t.floor, player_.vx = player_.vy = 0.0;
            player_.angle = std::atan2(at.y - y, at.x - x);
            player_.pitch = std::atan2(at.z + aim - eye.z, (float)dd) * 0.85;
            body_.reset();
            if (name == "swindle") appar_.setState("sb_t", 0.0);
            log::info("Skript: rand {} bei ({:.1f}, {:.1f}), Abstand {:.2f}", name, x, y, dd);
            return true;
        }
    }
    return false;
}

void GameSession::applyApparitionsLocal(const Apparitions::Event& e) {
    if (e.kind == Apparitions::Event::Toast) events_.push_back({GameEvent::Toast, e.text, e.value});
    else if (e.kind == Apparitions::Event::SoundAt) {
        GameEvent ev{GameEvent::SoundAt};
        ev.sound = e.text, ev.pos = e.pos, ev.intensity = e.value;
        events_.push_back(ev);
    }
}

}  // namespace lim::game
