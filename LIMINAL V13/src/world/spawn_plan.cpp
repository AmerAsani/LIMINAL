#include "world/spawn_plan.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>

#include "core/rng.hpp"

namespace lim::world {

SpawnPlan::SpawnPlan(const MapDef& map, const Json& rules, u64 seed) : map_(&map) {
    const size_t n = map.spawns.size();
    active_.assign(n, false);
    item_.assign(n, std::string());
    variant_.assign(n, 0);

    // Wege ueber den Raumgraphen: Dijkstra von der Startraum-Mitte, Kanten ueber Tuermitten
    const size_t R = map.rooms.size();
    roomDist_.assign(R, 1e18);
    std::vector<std::vector<std::pair<size_t, double>>> adj(R);
    auto ctr = [&](size_t i) {
        const MapRoom& r = map.rooms[i];
        return std::pair<double, double>{(r.x0 + r.x1) * 0.5, (r.y0 + r.y1) * 0.5};
    };
    for (const MapDoor& d : map.doors) {
        const double dx = (d.x0 + d.x1) * 0.5, dy = (d.y0 + d.y1) * 0.5;
        auto [ax, ay] = ctr((size_t)d.a);
        auto [bx, by] = ctr((size_t)d.b);
        const double w = std::hypot(ax - dx, ay - dy) + std::hypot(bx - dx, by - dy);
        adj[(size_t)d.a].push_back({(size_t)d.b, w});
        adj[(size_t)d.b].push_back({(size_t)d.a, w});
    }
    const int start = map.roomAt(map.startX, map.startY);
    if (start >= 0) {
        using Q = std::pair<double, size_t>;
        std::priority_queue<Q, std::vector<Q>, std::greater<Q>> q;
        auto [sx, sy] = ctr((size_t)start);
        roomDist_[(size_t)start] = std::hypot(sx - map.startX, sy - map.startY);
        q.push({roomDist_[(size_t)start], (size_t)start});
        while (!q.empty()) {
            auto [d, u] = q.top();
            q.pop();
            if (d > roomDist_[u]) continue;
            for (auto [v, w] : adj[u])
                if (d + w < roomDist_[v]) roomDist_[v] = d + w, q.push({d + w, v});
        }
    }

    Rng rng(hash64((i64)seed, "spawnplan", map.id.c_str()));
    // nach Art gruppieren
    std::map<std::string, std::vector<size_t>> byKind;
    for (size_t i = 0; i < n; ++i) byKind[map.spawns[i].kind].push_back(i);

    for (auto& [kind, idx] : byKind) {
        const Json& rule = rules[kind];
        if (kind == "deco") {
            const double chance = rule["chance"].asNumber(0.5);
            for (size_t i : idx) {
                const MapSpawn& s = map.spawns[i];
                active_[i] = rng.random() < chance * s.weight;
                variant_[i] = (int)rng.randint(0, 1023);
            }
            continue;
        }
        if (kind == "entity") {  // Kandidaten fuer die Regie, immer verfuegbar
            for (size_t i : idx) active_[i] = true;
            continue;
        }
        const i64 lo = rule["count"][0].asInt((i64)idx.size()), hi = rule["count"][1].asInt(lo);
        const i64 want = std::min<i64>((i64)idx.size(), rng.randint(std::max<i64>(0, lo), std::max(lo, hi)));
        const double spacing = rule["spacing"].asNumber(0.0);
        const double firstWithin = rule["first_within"].asNumber(-1.0);
        // Reihenfolge: zufaellig, gewichtet; zuerst (falls verlangt) ein Punkt nahe am Start
        std::vector<size_t> order = idx;
        for (size_t k = order.size(); k > 1; --k) std::swap(order[k - 1], order[(size_t)rng.randint(0, (i64)k - 1)]);
        if (firstWithin > 0.0) {
            auto near = std::find_if(order.begin(), order.end(), [&](size_t i) {
                return pathFromStart(map.spawns[i].x, map.spawns[i].y) <= firstWithin;
            });
            if (near != order.end()) std::rotate(order.begin(), near, near + 1);
        }
        std::vector<size_t> chosen;
        std::vector<int> groups;
        for (size_t i : order) {
            if ((i64)chosen.size() >= want) break;
            const MapSpawn& s = map.spawns[i];
            if (s.group >= 0 && std::find(groups.begin(), groups.end(), s.group) != groups.end()) continue;
            bool ok = true;
            for (size_t c : chosen)
                if (std::hypot(map.spawns[c].x - s.x, map.spawns[c].y - s.y) < spacing) ok = false;
            if (!ok) continue;
            if (rng.random() > std::min(1.0, s.weight)) continue;
            chosen.push_back(i);
            if (s.group >= 0) groups.push_back(s.group);
        }
        for (size_t i : chosen) {
            active_[i] = true;
            variant_[i] = (int)rng.randint(0, 1023);
        }
        if (kind == "item") {
            // Gegenstand je Punkt aus der Tabelle
            std::vector<std::string> keys;
            std::vector<double> w;
            for (const auto& [k, v] : rule["table"].members()) keys.push_back(k), w.push_back(v.asNumber(0.0));
            if (keys.empty()) keys = {"almond"}, w = {1.0};
            for (size_t i : chosen) item_[i] = keys[rng.weighted(w)];
        }
    }
}

double SpawnPlan::pathFromStart(double x, double y) const {
    if (!map_) return 0.0;
    const int r = map_->roomAt(x, y);
    if (r < 0) return 1e9;
    const MapRoom& R = map_->rooms[(size_t)r];
    return roomDist_[(size_t)r] + std::hypot((R.x0 + R.x1) * 0.5 - x, (R.y0 + R.y1) * 0.5 - y);
}

int SpawnPlan::countActive(const std::string& kind) const {
    int c = 0;
    if (!map_) return 0;
    for (size_t i = 0; i < active_.size(); ++i)
        if (active_[i] && map_->spawns[i].kind == kind) ++c;
    return c;
}

}  // namespace lim::world
