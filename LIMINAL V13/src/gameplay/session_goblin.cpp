// V13.3: Plague Goblins (KREATUR_PlagueGoblins.md) - was die Sitzung fuer die Bande tut. Die Praesenz entscheidet (beim Host),
// hier geschieht es in der Welt: was im Inventar zu holen ist (je Spieler), was in der Naehe liegt, der Diebstahl aus dem
// Inventar, das Aufheben liegender Funde, was ein toter Goblin verliert, das Geschenk beim Handel - und das Beil.
#include <format>

#include "core/log.hpp"
#include "gameplay/components.hpp"
#include "gameplay/pawn.hpp"
#include "gameplay/session.hpp"

namespace lim::game {

namespace {
// was die Seite sie sammeln laesst: Essen, Mandelwasser, Vorraete - nicht das Beil (Klinge), nicht die Sicherungen
bool stealable(const ItemDef* d) {
    return d && d->key != "hatchet" && (d->sanity > 0.0 || d->health > 0.0 || d->battery > 0.0 || d->stamina > 0.0);
}
void lootOf(const Inventory& inv, const Content& content, std::vector<std::string>& out) {
    out.clear();
    for (const auto& s : inv.slots)
        if (s && stealable(content.item(s->kind)))
            for (int i = 0; i < s->count; ++i) out.push_back(s->kind);
}
bool removeOne(Inventory& inv, const std::string& kind) {
    for (int i = 0; i < Inventory::kSize; ++i)
        if (inv.slots[(size_t)i] && inv.slots[(size_t)i]->kind == kind) return inv.take(i).has_value();
    return false;
}
}  // namespace

void GameSession::goblinContext(Presence::Context& c) {
    lootOf(inventory_, content_, goblinLoot_);
    c.loot = &goblinLoot_;
    c.health = stats_.health;
    if (const auto& sl = inventory_.slots[(size_t)inventory_.selected]; sl && sl->kind == "hatchet") c.hatchet = true;
    loose_.clear();
    const vec3 me{(float)player_.x, (float)player_.y, (float)player_.z};
    reg_.each<Pickup, Transform>([&](ecs::Entity, Pickup& pk, Transform& t) {
        if (std::hypot(t.pos.x - me.x, t.pos.y - me.y) > 32.0f) return;
        loose_.push_back({t.pos, pk.item, pk.worldId.rfind("drop:", 0) == 0});
    });
    c.loose = &loose_;
    for (Presence::Other& o : others_)
        for (Pawn* p : pawns)
            if (p && p->id == o.id) {
                auto& l = pawnLoot_[(size_t)(o.id & 3)];
                lootOf(p->inventory, content_, l);
                o.loot = &l;
                o.health = p->stats.health;
                const auto& sl = p->inventory.slots[(size_t)std::clamp(p->inventory.selected, 0, Inventory::kSize - 1)];
                o.hatchet = sl && sl->kind == "hatchet";
            }
}

void GameSession::spawnLooseAt(const std::string& kind, vec3 pos) {
    if (!content_.item(kind)) return;
    if (dropped_.size() >= kMaxDropped) {
        if (reg_.alive(dropped_.front().entity)) reg_.destroy(dropped_.front().entity);
        dropped_.erase(dropped_.begin());
    }
    const u32 h = hash32((u32)nextDrop_ * 2246822519u + 777u);
    DroppedItem d;
    d.id = std::format("drop:{}", nextDrop_++);
    d.kind = kind;
    d.pos = pos;
    d.yaw = hash01(h) * 6.2831853f;
    dropped_.push_back(d);
    const vec3 vel{(hash01(h ^ 0x9E37u) - 0.5f) * 0.8f, (hash01(h ^ 0x51EDu) - 0.5f) * 0.8f, 1.2f};
    spawnDropped(dropped_.back(), vel, false);
    if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Dropped, worldItemHash(d.id), kind, d.pos, -1});
}

void GameSession::goblinWorld(const Presence::Event& e) {
    if (e.role != Presence::Role::Goblin) return;
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
        case PE::Steal: {
            if (e.strength < 0.25f) {  // Ankuendigung: es zerrt am Rucksack
                tell("Etwas zerrt an deinem Rucksack!", 1.6f);
                if (self) stats_.sanity = std::max(0.0, stats_.sanity - 1.0);
                else if (pw) pw->stats.sanity = std::max(0.0, pw->stats.sanity - 1.0);
                break;
            }
            if (e.strength < 0.75f) {
                tell("Krallen kramen in deinem Rucksack - und finden nichts.", 2.4f);
                break;
            }
            Inventory* inv = self ? &inventory_ : (pw ? &pw->inventory : nullptr);
            if (inv && removeOne(*inv, e.item)) {
                const ItemDef* d = content_.item(e.item);
                tell(std::format("{} ist weg - verschwunden unter einem Umhang.", d ? d->name : e.item), 3.0f);
                log::info("Plague Goblins: {} gestohlen (Spieler {})", e.item, e.target);
            }
            break;
        }
        case PE::Take: {  // den naechsten passenden Fund bei pos entfernen
            ecs::Entity best = ecs::kNull;
            float bd = 0.7f;
            reg_.each<Pickup, Transform>([&](ecs::Entity en, Pickup& pk, Transform& t) {
                if (!e.item.empty() && pk.item != e.item) return;
                const float dd = length(t.pos - e.pos);
                if (dd < bd) bd = dd, best = en;
            });
            if (best == ecs::kNull) break;
            auto* pk = reg_.get<Pickup>(best);
            const Transform* t = reg_.get<Transform>(best);
            u32 hash = worldItemHash(pk->worldId);
            if (pk->worldId.rfind("drop:", 0) == 0) {
                for (const DroppedItem& x : dropped_)
                    if (x.id == pk->worldId && x.netHash) hash = x.netHash;
                std::erase_if(dropped_, [&](const DroppedItem& x) { return x.id == pk->worldId; });
            } else {
                picked.insert(pk->worldId);
            }
            if (netRole == NetRole::Host) netWorldOut.push_back({NetWorldEvent::Picked, hash, pk->item, t ? t->pos : e.pos, -1});
            if (target_ == best) target_ = ecs::kNull;
            reg_.destroy(best);
            break;
        }
        case PE::Spill: spawnLooseAt(e.item, e.pos); break;
        case PE::Gift: {
            spawnLooseAt(e.item, e.pos);
            const ItemDef* d = content_.item(e.item);
            tell(std::format("Die Torwaechterin legt dir etwas hin: {}.", d ? d->name : e.item), 3.0f);
            break;
        }
        default: break;
    }
}

int GameSession::hatchetSwing(const Player& p, vec3 eye, int who) {
    const vec3 look = forwardFromAngles((float)p.angle, (float)p.pitch);
    return presence_.goblinSwing(who, eye, look);
}

}  // namespace lim::game
