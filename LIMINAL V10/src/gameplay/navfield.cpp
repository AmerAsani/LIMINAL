#include "gameplay/navfield.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

#include "world/room.hpp"
#include "world/world.hpp"

namespace lim::game {

bool figureWalkable(const world::Tile* t, float clearance) {
    return t && t->open() && !(t->flags & world::TF_MACHINE) && t->clearance() >= clearance;
}

bool clearSight(world::World& w, vec3 a, vec3 b) {
    vec3 d = b - a;
    float len = std::sqrt(dot(d, d));
    if (len < 0.5f) return true;
    for (float t = 0.25f; t < len - 0.3f; t += 0.2f) {
        vec3 q = a + d * (t / len);
        const world::Tile* tl = w.tileIfLoaded(ifloor(q.x), ifloor(q.y));
        if (!tl || tl->solid() || q.z < tl->floor + 0.05f || q.z > tl->ceil - 0.02f) return false;
    }
    return true;
}

void NavField::build(world::World& w, i64 cx, i64 cy, float clearance) {
    cx_ = cx, cy_ = cy;
    f_.assign((size_t)kN * kN, -1);
    if (!figureWalkable(w.tileIfLoaded(cx, cy), clearance)) {
        // Ursprung auf einer Kante: naechste begehbare Nachbarkachel
        bool found = false;
        for (int r = 1; r <= 2 && !found; ++r)
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx)
                    if (figureWalkable(w.tileIfLoaded(cx + dx, cy + dy), clearance)) cx_ = cx + dx, cy_ = cy + dy, found = true;
        if (!found) return;
    }
    using Node = std::pair<int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> q;
    f_[(size_t)(kR * kN + kR)] = 0;
    q.push({0, kR * kN + kR});
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!q.empty()) {
        auto [c, idx] = q.top();
        q.pop();
        if (c != f_[(size_t)idx]) continue;
        const int lx = idx % kN, ly = idx / kN;
        const world::Tile* t = w.tileIfLoaded(cx_ - kR + lx, cy_ - kR + ly);
        if (!t) continue;
        for (int k = 0; k < 8; ++k) {
            const int nx = lx + dx[k], ny = ly + dy[k];
            if (nx < 0 || ny < 0 || nx >= kN || ny >= kN) continue;
            const world::Tile* n = w.tileIfLoaded(cx_ - kR + nx, cy_ - kR + ny);
            if (!figureWalkable(n, clearance) || std::fabs(n->floor - t->floor) > 0.65f) continue;
            if (k >= 4 && (!figureWalkable(w.tileIfLoaded(cx_ - kR + nx, cy_ - kR + ly), clearance) ||
                           !figureWalkable(w.tileIfLoaded(cx_ - kR + lx, cy_ - kR + ny), clearance)))
                continue;
            const int nc = c + (k < 4 ? 10 : 14);
            int& f = f_[(size_t)(ny * kN + nx)];
            if (f < 0 || nc < f) {
                f = nc;
                q.push({nc, ny * kN + nx});
            }
        }
    }
}

int NavField::at(i64 x, i64 y) const {
    const i64 lx = x - cx_ + kR, ly = y - cy_ + kR;
    if (f_.empty() || lx < 0 || ly < 0 || lx >= kN || ly >= kN) return -1;
    return f_[(size_t)(ly * kN + lx)];
}

bool NavField::next(double x, double y, double& tx, double& ty) const {
    const i64 cx = ifloor(x), cy = ifloor(y);
    const int f = at(cx, cy);
    if (f < 0) return false;
    if (f == 0) {
        tx = (double)cx_ + 0.5, ty = (double)cy_ + 0.5;
        return true;
    }
    int best = f;
    i64 bx = cx, by = cy;
    for (int ddy = -1; ddy <= 1; ++ddy)
        for (int ddx = -1; ddx <= 1; ++ddx) {
            if (!ddx && !ddy) continue;
            const int g = at(cx + ddx, cy + ddy);
            if (g < 0 || g >= best) continue;
            if (ddx && ddy && (at(cx + ddx, cy) < 0 || at(cx, cy + ddy) < 0)) continue;
            best = g, bx = cx + ddx, by = cy + ddy;
        }
    if (best == f) return false;
    tx = (double)bx + 0.5, ty = (double)by + 0.5;
    return true;
}

void NavField::ring(int lo, int hi, std::vector<std::pair<i64, i64>>& out) const {
    for (int ly = 0; ly < kN; ++ly)
        for (int lx = 0; lx < kN; ++lx) {
            int f = f_.empty() ? -1 : f_[(size_t)(ly * kN + lx)];
            if (f >= lo && f <= hi) out.push_back({cx_ - kR + lx, cy_ - kR + ly});
        }
}

bool litPoint(const Viewer& v, world::World& w, vec3 p) {
    if (v.flashOn) {
        vec3 d = p - v.flashPos;
        float len = std::sqrt(dot(d, d));
        if (len < 22.0f && len > 0.01f && dot(d / len, v.flashDir) > 0.895f) return true;
    }
    const world::Tile* t = w.tileIfLoaded(ifloor(p.x), ifloor(p.y));
    if (!t || t->light < 0.18f) return false;
    if (v.blackoutSeed >= 0 && v.blackoutAmount > 0.5f && t->room && (int)(t->room->seed & 31u) == v.blackoutSeed) return false;
    return true;
}

Sight perceive(const Viewer& v, world::World& w, vec3 feet, float height) {
    Sight s;
    vec3 c = feet + vec3{0, 0, height * 0.6f};
    s.dist = length(c - v.eye);
    // Nebel: ab einer gewissen Entfernung verschwimmt alles
    float fogVis = std::exp(-s.dist * v.fog * 0.6f);
    for (float hf : {0.93f, 0.62f, 0.28f}) {
        vec3 target = feet + vec3{0, 0, height * hf};
        vec3 d = target - v.eye;
        float len = std::sqrt(dot(d, d));
        if (len < 0.01f) continue;
        float cosA = dot(d / len, v.look);
        float ang = std::acos(std::clamp(cosA, -1.0f, 1.0f));
        if (ang > 0.9f) continue;  // ~52 Grad: ausserhalb des Blickfelds
        if (!clearSight(w, v.eye, target)) continue;
        s.inView = true;
        s.angle = std::min(s.angle, ang);
        bool lit = litPoint(v, w, target);
        s.lit = s.lit || lit;
        if (!lit) continue;
        // am Rand des Blickfelds weniger deutlich
        float edge = 1.0f - smoothstep(0.62f, 0.9f, ang);
        s.level = std::max(s.level, edge * std::clamp(fogVis * 1.6f, 0.0f, 1.0f));
    }
    s.direct = s.level > 0.3f && s.angle < 0.23f;
    return s;
}

}  // namespace lim::game
