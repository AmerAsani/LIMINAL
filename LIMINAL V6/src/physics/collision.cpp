#include "physics/collision.hpp"

#include <cmath>
#include <cstdlib>

namespace lim::phys {

bool blocked(TileSource& w, double x, double y, double z, const Shape& s) {
    const double r = s.radius;
    for (i64 ty = ifloor(y - r); ty <= ifloor(y + r); ++ty)
        for (i64 tx = ifloor(x - r); tx <= ifloor(x + r); ++tx) {
            const world::Tile& c = w.at(tx, ty);
            if (c.solid()) return true;
            if ((double)c.floor - z > s.maxStep) return true;
            if ((double)c.ceil - std::max((double)c.floor, z) < s.body) return true;
        }
    return false;
}

double ground(TileSource& w, double x, double y, double z, const Shape& s) {
    const double r = s.radius * 0.7;
    double g = -1e9;
    for (i64 ty = ifloor(y - r); ty <= ifloor(y + r); ++ty)
        for (i64 tx = ifloor(x - r); tx <= ifloor(x + r); ++tx) {
            const world::Tile& c = w.at(tx, ty);
            if (c.open() && (double)c.floor > g && (double)c.floor - z <= s.maxStep + 0.01) g = c.floor;
        }
    return g > -1e8 ? g : z;
}

double headroom(TileSource& w, double x, double y, const Shape& s) {
    const double r = s.radius;
    double cap = 1e9;
    for (i64 ty = ifloor(y - r); ty <= ifloor(y + r); ++ty)
        for (i64 tx = ifloor(x - r); tx <= ifloor(x + r); ++tx) {
            const world::Tile& c = w.at(tx, ty);
            if (c.open()) cap = std::min(cap, (double)c.ceil - s.body);
        }
    return cap;
}

MoveResult moveAndSlide(TileSource& w, double& x, double& y, double z, double dx, double dy, const Shape& s) {
    MoveResult r;
    int steps = std::max(1, (int)(std::max(std::fabs(dx), std::fabs(dy)) / 0.2) + 1);
    double sx = dx / steps, sy = dy / steps;
    double ox = x, oy = y;
    for (int i = 0; i < steps; ++i) {
        if (sx != 0.0) {
            if (!blocked(w, x + sx, y, z, s)) x += sx;
            else {
                r.hitX = true;
                sx = 0.0;
            }
        }
        if (sy != 0.0) {
            if (!blocked(w, x, y + sy, z, s)) y += sy;
            else {
                r.hitY = true;
                sy = 0.0;
            }
        }
    }
    r.moved = std::hypot(x - ox, y - oy);
    return r;
}

bool clearLine(TileSource& w, double x0, double y0, double z, double x1, double y1, double stopBefore, double headroom) {
    double dx = x1 - x0, dy = y1 - y0;
    double d = std::hypot(dx, dy);
    if (d < 1e-6) return true;
    int steps = (int)(std::max(0.0, d - stopBefore) / 0.2);
    for (int k = 1; k <= steps; ++k) {
        double t = k * 0.2 / d;
        const world::Tile& c = w.at(ifloor(x0 + dx * t), ifloor(y0 + dy * t));
        if (c.solid() || (double)c.floor > z + headroom) return false;
    }
    return true;
}

bool findFreeSpot(TileSource& w, double& x, double& y, double& z, const Shape& s) {
    if (!blocked(w, x, y, z, s)) return true;
    i64 bx = ifloor(x), by = ifloor(y);
    for (i64 rad = 1; rad < 10; ++rad)
        for (i64 dy = -rad; dy <= rad; ++dy)
            for (i64 dx = -rad; dx <= rad; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != rad) continue;
                const world::Tile& c = w.at(bx + dx, by + dy);
                if (c.solid() || c.ceil - c.floor < 1.9f) continue;
                double nx = (double)(bx + dx) + 0.5, ny = (double)(by + dy) + 0.5;
                if (!blocked(w, nx, ny, c.floor, s)) {
                    x = nx, y = ny, z = c.floor;
                    return true;
                }
            }
    return false;
}

}  // namespace lim::phys
