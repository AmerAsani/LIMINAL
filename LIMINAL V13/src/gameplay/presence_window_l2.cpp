// V13.3 Teil 4: Fensterwesen in Level 2 (KREATUR_Fensterwesen.md; Wikidot Entity 2 Rev. 36, Level 2 Rev. 93).
//
// Level 2: selten, meist am Ende von Gaengen; man soll nicht hingehen. Entity 2: ein Fenster bildet sich auf fast jeder
// Flaeche, lieber an Waenden, im Stil der Ebene; es bleibt, wo es entstanden ist. Die Rohrgaenge haben keine Fenster -
// also bildet es sich selbst eines: ein altes Stahlfenster in der Stirnwand einer Sackgasse (Modell ent_window_l2;
// Spawnpunkte "entity"/"window" der Karte, tools/levels/level2.py). Dahinter die verschwommene, vertraute Landschaft und
// der dunkle Umriss eines Menschen.
//
//   Entstehen   nur ungesehen: die Stirnwand liegt ausser Sicht jedes Spielers (Blickfeld und freie Sicht), 12-45 m Weg;
//               Abstand der Auftritte presence.window.every, hoechstens presence.window.count zugleich. Auf der Flucht
//               oder mit zerruettetem Verstand oefter (Entity 2: neue Fenster entstehen aus dem Wunsch, zu entkommen).
//   Ankuendigung  beim Entstehen ein Knacken von Glas und Rahmen und ein Zug von Aussenluft (Event::Burst, role Window,
//               strength 1); danach in der Naehe immer wieder ferner Wind und Vogelstimmen hinter Glas (strength 0,3) -
//               man hoert es, bevor man es sieht. Dann wie in Level 188: Fluestern, Winken, Greifen (thinkWindow).
//   Vergehen    weit weg (70 m) und laenger ungesehen: fort (es "stirbt" nie vor den Augen).
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kWinZ = 1.85f;  // Mitte der Oeffnung ueber dem Boden (Unterkante 1,0 m, Oberkante 2,37 m; Tunnel 2,6 m)
}

bool Presence::windowL2(const Context& c) const {
    return c.kin && c.kin[world::KIN_WINDOW].on && c.kin[world::KIN_WINDOW].mode == "end";
}

bool Presence::spawnWindowL2(const Context& c, std::vector<Event>& out, bool forced) {
    if (!windowL2(c) || !fld().valid()) return false;
    const world::Sector* sec = c.world->sectorIfReady(c.player->x, c.player->y);
    if (!sec) return false;
    const vec3 eye = c.view.eye;
    std::vector<const world::Sector::NoteSpot*> cand;
    for (const auto& e : sec->entities)
        if (e.item == "window") cand.push_back(&e);
    for (size_t k = cand.size(); k > 1; --k) std::swap(cand[k - 1], cand[(size_t)rng_.randint(0, (i64)k - 1)]);
    if (forced)  // erzwungen (Skripte, Tests): das naechste Gangende
        std::sort(cand.begin(), cand.end(), [&](const auto* a, const auto* b) {
            const int ca = fld().at(ifloor(a->x), ifloor(a->y)), cb = fld().at(ifloor(b->x), ifloor(b->y));
            return (ca < 0 ? 1 << 30 : ca) < (cb < 0 ? 1 << 30 : cb);
        });
    for (const auto* sp : cand) {
        const vec3 n = dirYaw(sp->yaw);
        const world::Tile* t = c.world->tileIfLoaded(ifloor(sp->x), ifloor(sp->y));
        if (!t || !t->open()) continue;
        const vec3 wall{sp->x - n.x * 0.3f, sp->y - n.y * 0.3f, t->floor};
        bool taken = false;
        for (const auto& a : agents_) taken = taken || (a->role == Role::Window && hdist(a->pos, wall) < 1.0f);
        if (taken) continue;
        const int cost = fld().at(ifloor(sp->x), ifloor(sp->y));
        if (cost < 0 || (!forced && (cost < 120 || cost > 450)) || (forced && cost > 600)) continue;
        if (hdist(wall, eye) < 6.0f) continue;
        // ungesehen: kein Punkt der Fensterflaeche im Blickfeld mit freier Sicht
        const vec3 l{-n.y, n.x, 0.0f};
        bool visible = false;
        for (float dz : {1.2f, kWinZ, 2.3f})
            for (float dl : {-0.55f, 0.0f, 0.55f}) {
                const vec3 q = wall + n * 0.12f + l * dl + vec3{0, 0, dz};
                if (dot(nrmF(q - eye, {1, 0, 0}), c.view.look) > 0.25f && clearSight(*c.world, eye, q)) visible = true;
            }
        if (visible && !forced) continue;
        auto a = std::make_unique<Agent>();
        a->role = Role::Window;
        a->ent = Entity::Window;
        a->variant = 1;  // ent_window_l2: Stahlfenster der Ebene
        a->mode = 1;
        a->glow = 1.0f;
        a->noShadow = true;
        a->initBody(wall + vec3{0, 0, kWinZ}, sp->yaw, (unsigned)rng_.randint(1, 1 << 30));
        a->rig.setGlow(1.0f);
        a->timer = rng_.uniform(3.0, 6.0);
        a->timer2 = rng_.uniform(5.0, 9.0);
        out.push_back({Event::Burst, Role::Window, wall + n * 0.3f + vec3{0, 0, kWinZ}, 1.0f});
        log::info("Praesenz: Fensterwesen bildet sich am Gangende bei ({:.1f}, {:.1f}), {:.0f} m Weg", wall.x, wall.y, cost / 10.0);
        agents_.push_back(std::move(a));
        return true;
    }
    return false;
}

void Presence::regieWindowL2(double dt, const Context& c, std::vector<Event>& out) {
    if (!windowL2(c)) return;
    const world::KinDef& K = c.kin[world::KIN_WINDOW];
    const vec3 eye = c.view.eye;
    int alive = 0;
    for (auto& ap : agents_) {
        Agent& a = *ap;
        if (a.role != Role::Window || a.variant != 1 || a.showcase || a.puppet) continue;
        ++alive;
        const float d = hdist(a.pos, eye);
        // Aussenluft hinter Glas: ferner Wind, Vogelstimmen - Geraeusch vor Sicht
        a.timer2 -= dt;
        if (a.timer2 <= 0.0 && d < 26.0f && a.state == 0) {
            a.timer2 = rng_.uniform(6.0, 11.0);
            out.push_back({Event::Burst, Role::Window, a.pos + dirYaw(a.yaw) * 0.3f, 0.3f});
        }
        if (d > 70.0f && a.unseenT > 12.0) a.state = 9;  // fort - nie vor den Augen
    }
    if (alive >= std::max(1, K.count)) return;
    // auf der Flucht (Jaeger in der Naehe) oder mit zerruettetem Verstand entstehen sie oefter (Entity 2)
    const double urge = (hunting() ? 2.5 : 1.0) * (c.sanity < 45.0 ? 1.8 : 1.0);
    nextWindowL2_ -= dt * urge;
    if (nextWindowL2_ > 0.0) return;
    if (rng_.uniform(0.0, 1.0) > std::max(0.05, K.chance > 0.0 ? K.chance : 1.0)) {  // nicht jede Gelegenheit
        nextWindowL2_ = rng_.uniform(K.every * 0.4, K.every * 0.8);
        return;
    }
    nextWindowL2_ = spawnWindowL2(c, out, false) ? rng_.uniform(K.every * 0.7, K.every * 1.3) * c.stalkerPause : 10.0;
}

}  // namespace lim::game
