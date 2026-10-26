#include "gameplay/sound_field.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

#include "world/world.hpp"

namespace lim::game {

namespace {

// Kachel, durch die Schall geht: nicht massiv, Luft zwischen Boden und Decke
bool airy(const world::Tile* t) { return t && t->open() && t->clearance() > 0.4f; }

}  // namespace

float SoundField::rayOcclusion(world::World& world, vec3 a, vec3 b) {
    // Strahl durch das Kachelraster (DDA). Jede massive Kachel bzw. jede Kachel, deren Boden/Decke
    // der Strahl schneidet, zaehlt als Hindernis; zwei Hindernisse = voll verdeckt.
    vec2 d{b.x - a.x, b.y - a.y};
    i64 cx = ifloor(a.x), cy = ifloor(a.y);
    const i64 ex = ifloor(b.x), ey = ifloor(b.y);
    const int sx = d.x >= 0 ? 1 : -1, sy = d.y >= 0 ? 1 : -1;
    const float adx = std::max(std::fabs(d.x), 1e-6f), ady = std::max(std::fabs(d.y), 1e-6f);
    float tdx = 1.0f / adx, tdy = 1.0f / ady;
    float tmx = (sx > 0 ? (float)(cx + 1) - a.x : a.x - (float)cx) * tdx;
    float tmy = (sy > 0 ? (float)(cy + 1) - a.y : a.y - (float)cy) * tdy;
    float t0 = 0.0f;
    int blocked = 0;
    for (int i = 0; i < 96; ++i) {
        float t1 = std::min(std::min(tmx, tmy), 1.0f);
        if (i > 0 && !(cx == ex && cy == ey)) {
            const world::Tile* tile = world.tileIfLoaded(cx, cy);
            if (tile) {
                float za = a.z + (b.z - a.z) * t0, zb = a.z + (b.z - a.z) * t1;
                if (tile->solid() || std::min(za, zb) < tile->floor - 0.05f || std::max(za, zb) > tile->ceil + 0.05f)
                    if (++blocked >= 2) return 1.0f;
            }
        }
        if (t1 >= 1.0f || (cx == ex && cy == ey)) break;
        if (tmx < tmy) {
            cx += sx;
            t0 = tmx;
            tmx += tdx;
        } else {
            cy += sy;
            t0 = tmy;
            tmy += tdy;
        }
    }
    return blocked * 0.55f;
}

int SoundField::index(i64 x, i64 y) const {
    const i64 lx = x - ox_ + kR, ly = y - oy_ + kR;
    if (lx < 0 || ly < 0 || lx >= kN || ly >= kN) return -1;
    return (int)(ly * kN + lx);
}

float SoundField::pathTo(i64 x, i64 y) const {
    if (!valid_) return -1.0f;
    const int i = index(x, y);
    if (i < 0) return -1.0f;
    return dist_[(size_t)i] < 1e8f ? dist_[(size_t)i] : -1.0f;
}

void SoundField::update(world::World& w, vec3 ear, double dt) {
    age_ += dt;
    const i64 tx = ifloor(ear.x), ty = ifloor(ear.y);
    if (valid_ && tx == ox_ && ty == oy_ && age_ < 0.5) return;
    ox_ = tx, oy_ = ty, earZ_ = ear.z, age_ = 0.0;
    valid_ = true;
    ++rebuilds_;
    portals_.clear();
    dist_.assign((size_t)kN * kN, 1e9f);
    parent_.assign((size_t)kN * kN, -1);
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> q;
    const int start = index(tx, ty);
    dist_[(size_t)start] = 0.0f;
    q.push({0.0f, start});
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static const float cost[8] = {1, 1, 1, 1, 1.4142f, 1.4142f, 1.4142f, 1.4142f};
    while (!q.empty()) {
        auto [d, i] = q.top();
        q.pop();
        if (d > dist_[(size_t)i]) continue;
        const i64 x = ox_ - kR + i % kN, y = oy_ - kR + i / kN;
        const world::Tile* here = w.tileIfLoaded(x, y);
        for (int k = 0; k < 8; ++k) {
            const i64 nx = x + dx[k], ny = y + dy[k];
            const int j = index(nx, ny);
            if (j < 0) continue;
            const world::Tile* t = w.tileIfLoaded(nx, ny);
            if (!airy(t)) continue;
            // diagonal nur, wenn beide Nachbarn offen sind (nicht durch die Ecke einer Wand)
            if (k >= 4 && (!airy(w.tileIfLoaded(x + dx[k], y)) || !airy(w.tileIfLoaded(x, y + dy[k])))) continue;
            // Stufen: grosse Hoehenspruenge (Bodenloch, Schacht) leiten weniger - etwas teurer
            const float step = here ? std::fabs(t->floor - here->floor) : 0.0f;
            const float nd = d + cost[k] + (step > 1.2f ? step * 0.5f : 0.0f);
            if (nd < dist_[(size_t)j]) {
                dist_[(size_t)j] = nd;
                parent_[(size_t)j] = i;
                q.push({nd, j});
            }
        }
    }
}

SoundRoute SoundField::route(world::World& w, vec3 ear, vec3 src, float radius) const {
    SoundRoute r;
    r.pos = src;
    const float straight = length(src - ear);
    const float ray = rayOcclusion(w, ear, src);
    if (ray <= 0.0f) return r;  // freie Sicht
    r.direct = false;
    // durch die Wand: eine Wand daempft viel staerker, als der gerade Strahl (0,55 je Kachel) es abbildet -
    // nur tiefe Anteile kommen durch
    r.occlusion = std::max(ray, 0.85f);
    const int si = valid_ ? index(ifloor(src.x), ifloor(src.y)) : -1;
    if (si < 0 || dist_[(size_t)si] > 1e8f) return r;
    // Weglaenge: Kachelmitten, dazu das Stueck von der Kachelmitte zur Quelle; mindestens der gerade Abstand
    const float path = std::max(straight, dist_[(size_t)si] + 0.5f + std::fabs(src.z - earZ_) * 0.5f);
    if (path > radius) return r;
    // letzte Stelle auf dem Weg, die das Ohr noch direkt erreicht (je Quellkachel zwischengespeichert)
    auto it = portals_.find(si);
    if (it == portals_.end()) {
        Cached c;
        int i = si;
        for (int guard = 0; guard < kN * 2 && i >= 0; ++guard) {
            const vec3 p{(float)(ox_ - kR + i % kN) + 0.5f, (float)(oy_ - kR + i / kN) + 0.5f, ear.z};
            if (rayOcclusion(w, ear, p) <= 0.0f) {
                c.portal = p, c.ok = true;
                break;
            }
            i = parent_[(size_t)i];
        }
        it = portals_.emplace(si, c).first;
    }
    if (!it->second.ok) return r;
    const vec3 portal = it->second.portal;
    vec3 toPortal = portal - ear;
    const float lp = length(toPortal);
    vec3 dir = lp > 0.05f ? toPortal / lp : normalize(src - ear);
    // Beugung: je staerker der Schall an der Oeffnung abknicken muss, desto dumpfer (Hoehen gehen verloren)
    vec3 onward = src - portal;
    const float lo = length(onward);
    const float bend = lo > 0.05f ? std::acos(std::clamp(dot(dir, onward / lo), -1.0f, 1.0f)) : 0.0f;
    const float occAround = std::clamp(0.12f + 0.42f * bend / 3.14159f + 0.01f * (path - straight), 0.1f, 0.75f);
    // gibt es einen offenen Weg in Reichweite, ist er lauter als der Durchgang durch die Wand (Beugung an einer
    // Oeffnung kostet wenige dB, eine Wand 25 dB und mehr)
    r.around = true;
    r.pos = ear + vec3{dir.x, dir.y, (src.z - ear.z) / std::max(straight, 0.01f)} * straight;
    r.pathExtra = path - straight;
    r.occlusion = occAround;
    return r;
}

}  // namespace lim::game
