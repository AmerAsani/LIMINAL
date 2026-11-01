// V13.3 Teil 4: Death Rats in Level 2 an Faessern und Essensresten (KREATUR_DeathRat.md; Wikidot Level 2 Rev. 93, Entity 24
// Rev. 25). Die Level-2-Seite: fast harmlos, sie fressen die Fluessigkeiten in den Faessern und zurueckgelassenes Essen.
// Die Faesser (barrel@0..2) stehen in den Gaengen; an manchen liegen Essensreste mit einer Lache (food_scraps, level2.py).
//
//   stage 0  sucht      die naechste Futterstelle im Umkreis von 9 m (nach einer Pause; nicht, solange man nah ist)
//         1  hin        laeuft hin (Wegefeld wie beim Umhergehen, das letzte Stueck gerade)
//         2  klettert   am Fass hinauf (0,6 s im Bogen, Vorderkoerper aufgerichtet)
//         3  trinkt     auf dem Deckel, die Schnauze am Spundloch - Schlecken (8-20 s)
//         4  frisst     an den Resten oder schleckt die Lache am Fuss des Fasses (ist der Deckel besetzt) - Nagen
//         5  springt    vom Fass (0,35 s); danach flieht sie wie sonst (thinkRat)
// Kommt man nah, rennt oder leuchtet sie an, stiebt sie davon (thinkRat); wer sie bedraengt, wird gebissen (thinkRat).
// Klang: Event::Snip mit role Rat (strength 0: Schlecken, 1: Nagen) - die Ereignisart ist ohne Wirkung in der Sitzung,
// nur hoerbar (app.cpp); so bleibt das Protokoll der Koop-Ereignisse unveraendert.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"
#include "props/prop_library.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kBarrelTop = 0.885f;  // Deckel des Stahlfasses (ISO 15750, models_l2.py)

struct RatFeed {
    int stage = 0;
    int kind = 0;  // 0 Fass (Deckel), 1 Reste, 2 Lache am Fuss eines besetzten Fasses
    vec3 spot, base, top, from;
    double t = 0.0, pause = 0.0, sound = 0.0, dur = 0.0;
};
template <class A>
RatFeed& feedOf(A& a) {
    if (!a.ext) a.ext = std::make_shared<RatFeed>();
    return *static_cast<RatFeed*>(a.ext.get());
}

struct Food {
    vec3 p;
    int kind;  // 0 Fass, 1 Reste
};

int foodKind(const props::Model* m) {
    static const props::Model* barrel[3] = {props::model("barrel", 0), props::model("barrel", 1), props::model("barrel", 2)};
    static const props::Model* scraps[2] = {props::model("food_scraps", 0), props::model("food_scraps", 1)};
    if (!m) return -1;
    for (const props::Model* b : barrel)
        if (b && m == b) return 0;
    for (const props::Model* s : scraps)
        if (s && m == s) return 1;
    return -1;
}

void foods(const Presence::Context& c, vec3 near, float range, std::vector<Food>& out) {
    out.clear();
    const world::Sector* sec = c.world->sectorIfReady(near.x, near.y);
    if (!sec) return;
    for (const auto& p : sec->props) {
        const vec3 q{p.x, p.y, p.z};
        if (hdist(q, near) > range || std::fabs(q.z - near.z) > 1.2f) continue;
        const int k = foodKind(p.model);
        if (k >= 0) out.push_back({q, k});
    }
}

// Fuss eines Fasses: zur freien Seite hin (die Faesser stehen an der Wand)
bool barrelBase(const Presence::Context& c, vec3 center, vec3& base) {
    float best = -1.0f;
    for (int k = 0; k < 8; ++k) {
        const vec3 d = dirYaw((float)k * 0.7854f);
        const vec3 q = center + d * 0.42f;
        const world::Tile* t = c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y));
        if (!figureWalkable(t, 0.4f)) continue;
        // freier Raum dahinter (weg von der Wand)
        const vec3 q2 = center + d * 1.1f;
        const world::Tile* t2 = c.world->tileIfLoaded(ifloor(q2.x), ifloor(q2.y));
        const float score = (figureWalkable(t2, 0.4f) ? 1.0f : 0.0f) + 0.01f * (float)k;
        if (score > best) best = score, base = {q.x, q.y, t->floor};
    }
    return best >= 0.0f;
}
}  // namespace

// Auftritt (spawnKin): eine Futterstelle 8..20 m Weg vom Spieler, ausser Sicht - dort taucht die Gruppe auf
bool Presence::ratFoodSpawn(const Context& c, vec3& out) {
    if (!c.kin || c.kin[world::KIN_RAT].mode != "bald" || !fld().valid()) return false;
    std::vector<Food> fs;
    foods(c, c.view.eye, 26.0f, fs);
    for (size_t k = fs.size(); k > 1; --k) std::swap(fs[k - 1], fs[(size_t)rng_.randint(0, (i64)k - 1)]);
    const vec3 fl = nrmF(flat(c.view.look), {1, 0, 0});
    for (const Food& f : fs) {
        vec3 base;
        if (!barrelBase(c, f.p, base)) continue;
        const int cost = fld().at(ifloor(base.x), ifloor(base.y));
        if (cost < 80 || cost > 200) continue;
        bool visible = false;
        for (const vec3& q : {base + vec3{0, 0, 0.15f}, f.p + vec3{0, 0, kBarrelTop + 0.1f}})
            if (dot(nrmF(q - c.view.eye, fl), c.view.look) > 0.3f && clearSight(*c.world, c.view.eye, q)) visible = true;
        if (visible) continue;
        out = base;
        return true;
    }
    return false;
}

bool Presence::ratFeed(Agent& a, double dt, const Context& c, std::vector<Event>& out, bool scare) {
    if (!c.kin || c.kin[world::KIN_RAT].mode != "bald" || a.puppet || a.showcase) return false;
    RatFeed& F = feedOf(a);
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, feet);
    F.t += dt;
    F.pause -= dt;
    auto leave = [&](double pause) {
        F.stage = 0, F.pause = pause, F.t = 0.0;
        a.hasGoal = false;
    };
    // vom Fass herunter: im Bogen auf den Boden vor dem Fass
    if (F.stage == 5) {
        const float s = (float)std::clamp(F.t / 0.35, 0.0, 1.0);
        a.pos = F.from + (F.base - F.from) * s + vec3{0, 0, 0.12f * std::sin(3.14159f * s)};
        a.perceive = 1.0f, a.perceiveAt = c.view.eye;
        if (s >= 1.0f) {
            a.pos = F.base;
            leave(rng_.uniform(9.0, 16.0));
            return false;  // jetzt stiebt sie davon (thinkRat)
        }
        return true;
    }
    const bool high = F.stage == 2 || F.stage == 3;
    if (scare && high) {  // aufgeschreckt auf dem Fass: herunter
        out.push_back({Event::Squeak, Role::Rat, a.pos, 0.7f});
        F.stage = 5, F.t = 0.0, F.from = a.pos;
        const vec3 away = nrmF(flat(F.base - F.spot), {1, 0, 0});
        F.base = F.base + away * 0.25f;
        return true;
    }
    if (scare || (a.provoke > 2.5 && d < 1.6f)) {  // am Boden: wie sonst (davonstieben, zubeissen)
        if (F.stage) leave(rng_.uniform(9.0, 16.0));
        return false;
    }
    switch (F.stage) {
        case 0: {  // eine Futterstelle suchen
            if (F.pause > 0.0 || d < 5.0f) return false;
            std::vector<Food> fs;
            foods(c, a.pos, 9.0f, fs);
            float best = 1e9f;
            const Food* pick = nullptr;
            for (const Food& f : fs) {
                const float dd = hdist(f.p, a.pos) + (float)rng_.uniform(0.0, 1.5);
                if (dd < best) best = dd, pick = &f;
            }
            if (!pick) {
                F.pause = rng_.uniform(6.0, 12.0);
                return false;
            }
            vec3 base;
            if (!barrelBase(c, pick->p, base)) {
                F.pause = 8.0;
                return false;
            }
            F.spot = pick->p, F.base = base, F.kind = pick->kind;
            // Fass: ist der Deckel schon besetzt, schleckt sie die Lache am Fuss
            if (F.kind == 0)
                for (const auto& o : agents_)
                    if (o.get() != &a && o->role == Role::Rat && o->ext) {
                        const RatFeed& G = *static_cast<const RatFeed*>(o->ext.get());
                        if ((G.stage == 2 || G.stage == 3 || (G.stage == 1 && G.kind == 0)) && hdist(G.spot, F.spot) < 0.1f) F.kind = 2;
                    }
            if (F.kind == 1) F.base = pick->p + nrmF(flat(base - pick->p), {1, 0, 0}) * 0.22f, F.base.z = base.z;
            if (F.kind == 2) {  // seitlich am Fuss
                const vec3 side = rotateYaw(nrmF(flat(base - pick->p), {1, 0, 0}), (float)rng_.uniform(-1.0, 1.0));
                const vec3 q = pick->p + side * 0.40f;
                if (figureWalkable(c.world->tileIfLoaded(ifloor(q.x), ifloor(q.y)), 0.4f)) F.base = {q.x, q.y, base.z};
            }
            const vec3 dir = nrmF(flat(F.spot - F.base), {1, 0, 0});
            F.top = F.spot - dir * 0.05f + vec3{0, 0, kBarrelTop};
            a.field.build(*c.world, ifloor(F.base.x), ifloor(F.base.y), 0.4f);
            if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) {
                F.pause = 8.0;
                return false;
            }
            F.stage = 1, F.t = 0.0;
            return true;
        }
        case 1: {  // hin
            const float rest = hdist(a.pos, F.base);
            if (rest > 0.6f && F.t < 25.0) {
                if (!walkTo(a, a.field, 0.5, dt, c)) {  // im Zielfeld: gerade weiter
                    const vec3 to = nrmF(flat(F.base - a.pos), {1, 0, 0});
                    a.pos += to * (float)(0.5 * dt);
                    a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 6.0);
                }
                return true;
            }
            if (rest > 0.08f && F.t < 26.0) {
                const vec3 to = nrmF(flat(F.base - a.pos), {1, 0, 0});
                a.pos += to * (float)std::min((double)rest, 0.45 * dt);
                a.yaw += wrapA(std::atan2(to.y, to.x) - a.yaw) * (float)std::min(1.0, dt * 6.0);
                return true;
            }
            if (F.t >= 26.0) {
                leave(10.0);
                return false;
            }
            const vec3 dir = nrmF(flat(F.spot - F.base), {1, 0, 0});
            a.yaw = std::atan2(dir.y, dir.x);
            F.from = a.pos;
            F.t = 0.0;
            F.dur = rng_.uniform(8.0, 20.0);
            F.stage = F.kind == 0 ? 2 : 4;
            return true;
        }
        case 2: {  // klettert hinauf
            const float s = (float)std::clamp(F.t / 0.6, 0.0, 1.0);
            const float e = s * s * (3.0f - 2.0f * s);
            a.pos = F.from + (F.top - F.from) * e + vec3{0, 0, 0.08f * std::sin(3.14159f * s)};
            a.perceive = 1.0f, a.perceiveAt = F.top + vec3{0, 0, 0.3f};  // aufgerichtet, die Nase voran
            if (s >= 1.0f) a.pos = F.top, F.stage = 3, F.t = 0.0;
            return true;
        }
        case 3:
        case 4: {  // trinkt am Spundloch / frisst die Reste, schleckt die Lache
            if (F.stage == 3) a.pos = F.top;
            // Kopf zum Futter, kauen; ab und zu hebt sie den Kopf und wittert
            const bool sniff = std::fmod(F.t, 6.5) > 5.3;
            a.jaw = sniff ? 0.0f : 1.0f;
            a.perceive = sniff ? 1.0f : 0.0f;
            a.perceiveAt = c.view.eye;
            F.sound -= dt;
            if (!sniff && F.sound <= 0.0 && d < 14.0f) {
                F.sound = rng_.uniform(0.9, 1.7);
                out.push_back({Event::Snip, Role::Rat, a.pos + vec3{0, 0, 0.05f}, F.kind == 1 ? 1.0f : 0.0f});
            }
            if (F.t > F.dur) {
                if (F.stage == 3) {
                    F.stage = 5, F.t = 0.0, F.from = a.pos;
                    return true;
                }
                leave(rng_.uniform(10.0, 25.0));
                return false;
            }
            return true;
        }
        default: break;
    }
    return false;
}

}  // namespace lim::game
