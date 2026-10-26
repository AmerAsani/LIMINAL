#include "render/sculpt.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "core/jobs.hpp"

namespace lim::gfx::sculpt {

float smin(float a, float b, float k) {
    if (k <= 0.0f) return std::min(a, b);
    float h = std::max(k - std::fabs(a - b), 0.0f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}
float smax(float a, float b, float k) { return -smin(-a, -b, k); }

namespace {

inline std::uint32_t hashU(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline float lattice(int x, int y, int z, unsigned seed) {
    std::uint32_t h = hashU((std::uint32_t)x * 73856093u ^ (std::uint32_t)y * 19349663u ^ (std::uint32_t)z * 83492791u ^ seed * 2654435761u);
    return (float)(h >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

float sdEllipsoid(vec3 q, vec3 r) {
    vec3 a{q.x / r.x, q.y / r.y, q.z / r.z};
    vec3 b{q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z)};
    float k0 = length(a), k1 = length(b);
    if (k1 < 1e-9f) return -std::min(r.x, std::min(r.y, r.z));
    return k0 * (k0 - 1.0f) / k1;
}

// abgerundeter Kegel (Inigo Quilez, exakt)
float sdRoundCone(vec3 p, vec3 a, vec3 b, float r1, float r2) {
    vec3 ba = b - a;
    float l2 = dot(ba, ba);
    if (l2 < 1e-10f) return length(p - a) - std::max(r1, r2);
    float rr = r1 - r2, a2 = l2 - rr * rr, il2 = 1.0f / l2;
    vec3 pa = p - a;
    float y = dot(pa, ba), z = y - l2;
    vec3 xv = pa * l2 - ba * y;
    float x2 = dot(xv, xv), y2 = y * y * l2, z2 = z * z * l2;
    float k = (rr > 0 ? 1.0f : (rr < 0 ? -1.0f : 0.0f)) * rr * rr * x2;
    if ((z > 0 ? 1.0f : -1.0f) * a2 * z2 > k) return std::sqrt(x2 + z2) * il2 - r2;
    if ((y > 0 ? 1.0f : -1.0f) * a2 * y2 < k) return std::sqrt(x2 + y2) * il2 - r1;
    return (std::sqrt(x2 * a2 * il2) + y * rr) * il2 - r1;
}

float sdRoundBox(vec3 q, vec3 half, float rnd) {
    vec3 d{std::fabs(q.x) - half.x + rnd, std::fabs(q.y) - half.y + rnd, std::fabs(q.z) - half.z + rnd};
    vec3 m{std::max(d.x, 0.0f), std::max(d.y, 0.0f), std::max(d.z, 0.0f)};
    return length(m) + std::min(std::max(d.x, std::max(d.y, d.z)), 0.0f) - rnd;
}

float boxDistance(const Aabb& b, vec3 p) {
    vec3 d{std::max({b.lo.x - p.x, 0.0f, p.x - b.hi.x}), std::max({b.lo.y - p.y, 0.0f, p.y - b.hi.y}),
           std::max({b.lo.z - p.z, 0.0f, p.z - b.hi.z})};
    return length(d);
}

float primDist(const Prim& pr, vec3 p, const std::vector<std::function<float(vec3)>>& customs) {
    switch (pr.kind) {
        case Prim::Ellipsoid: {
            vec3 d = p - pr.a;
            return sdEllipsoid({dot(d, pr.ax[0]), dot(d, pr.ax[1]), dot(d, pr.ax[2])}, pr.r);
        }
        case Prim::RoundCone: return sdRoundCone(p, pr.a, pr.b, pr.r.x, pr.r.y);
        case Prim::RoundBox: {
            vec3 d = p - pr.a;
            return sdRoundBox({dot(d, pr.ax[0]), dot(d, pr.ax[1]), dot(d, pr.ax[2])}, pr.r, pr.round);
        }
        case Prim::Custom: return customs[(size_t)pr.custom](p);
    }
    return 1e9f;
}

Aabb orientedBounds(vec3 c, const vec3 ax[3], vec3 r, float grow) {
    vec3 e{std::fabs(ax[0].x) * r.x + std::fabs(ax[1].x) * r.y + std::fabs(ax[2].x) * r.z,
           std::fabs(ax[0].y) * r.x + std::fabs(ax[1].y) * r.y + std::fabs(ax[2].y) * r.z,
           std::fabs(ax[0].z) * r.x + std::fabs(ax[1].z) * r.y + std::fabs(ax[2].z) * r.z};
    Aabb b;
    b.add(c - e - vec3(grow));
    b.add(c + e + vec3(grow));
    return b;
}

}  // namespace

float noise3(vec3 p, unsigned seed) {
    int xi = (int)std::floor(p.x), yi = (int)std::floor(p.y), zi = (int)std::floor(p.z);
    float fx = p.x - xi, fy = p.y - yi, fz = p.z - zi;
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy), uz = fz * fz * (3 - 2 * fz);
    float r = 0;
    for (int dz = 0; dz < 2; ++dz)
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx) {
                float w = (dx ? ux : 1 - ux) * (dy ? uy : 1 - uy) * (dz ? uz : 1 - uz);
                r += w * lattice(xi + dx, yi + dy, zi + dz, seed);
            }
    return r;
}

float fbm3(vec3 p, int oct, unsigned seed) {
    float s = 0, a = 0.5f, tot = 0;
    for (int i = 0; i < oct; ++i) {
        s += a * noise3(p, seed + (unsigned)i * 101u);
        tot += a;
        p = p * 2.03f;
        a *= 0.5f;
    }
    return s / tot;
}

// --- Formen ---------------------------------------------------------------------------------
Shape& Shape::ellipsoid(vec3 c, vec3 radii, float k, Prim::Op op) {
    return ellipsoidAx(c, {1, 0, 0}, {0, 1, 0}, radii, k, op);
}

Shape& Shape::ellipsoidAx(vec3 c, vec3 ax, vec3 ay, vec3 radii, float k, Prim::Op op) {
    Prim p;
    p.kind = Prim::Ellipsoid;
    p.op = op;
    p.k = k;
    p.a = c;
    p.ax[0] = normalize(ax);
    p.ax[1] = normalize(ay - p.ax[0] * dot(ay, p.ax[0]));
    p.ax[2] = cross(p.ax[0], p.ax[1]);
    p.r = radii;
    p.box = orientedBounds(c, p.ax, radii, k);
    prims_.push_back(p);
    return *this;
}

Shape& Shape::cone(vec3 a, vec3 b, float ra, float rb, float k, Prim::Op op) {
    Prim p;
    p.kind = Prim::RoundCone;
    p.op = op;
    p.k = k;
    p.a = a, p.b = b;
    p.r = {ra, rb, 0};
    p.box.add(a - vec3(ra + k));
    p.box.add(a + vec3(ra + k));
    p.box.add(b - vec3(rb + k));
    p.box.add(b + vec3(rb + k));
    prims_.push_back(p);
    return *this;
}

Shape& Shape::custom(std::function<float(vec3)> f, Aabb box, float k, Prim::Op op) {
    Prim p;
    p.kind = Prim::Custom;
    p.op = op;
    p.k = k;
    p.custom = (int)customs_.size();
    customs_.push_back(std::move(f));
    p.box = box;
    p.box.lo = p.box.lo - vec3(k);
    p.box.hi = p.box.hi + vec3(k);
    prims_.push_back(p);
    return *this;
}

Shape& Shape::box(vec3 c, vec3 ax, vec3 ay, vec3 half, float round, float k, Prim::Op op) {
    Prim p;
    p.kind = Prim::RoundBox;
    p.op = op;
    p.k = k;
    p.a = c;
    p.ax[0] = normalize(ax);
    p.ax[1] = normalize(ay - p.ax[0] * dot(ay, p.ax[0]));
    p.ax[2] = cross(p.ax[0], p.ax[1]);
    p.r = half;
    p.round = std::min(round, std::min(half.x, std::min(half.y, half.z)));
    p.box = orientedBounds(c, p.ax, half, k);
    prims_.push_back(p);
    return *this;
}

float Shape::eval(vec3 p) const {
    float d = 1e9f;
    for (const Prim& pr : prims_) {
        float lb = boxDistance(pr.box, p);
        if (pr.op == Prim::Add) {
            if (lb >= d + pr.k) continue;
            d = smin(d, primDist(pr, p, customs_), pr.k);
        } else if (pr.op == Prim::Sub) {
            if (lb >= pr.k - d) continue;
            d = smax(d, -primDist(pr, p, customs_), pr.k);
        } else {
            d = smax(d, primDist(pr, p, customs_), pr.k);
        }
    }
    d -= offset;
    if (displace && std::fabs(d) < 0.04f) d += displace(p);
    if (clip) d = std::max(d, clip(p));
    return d;
}

Aabb Shape::bounds() const {
    Aabb b;
    for (const Prim& pr : prims_)
        if (pr.op == Prim::Add) {
            b.add(pr.box.lo);
            b.add(pr.box.hi);
        }
    if (b.valid()) {
        b.lo = b.lo - vec3(offset + 0.01f);
        b.hi = b.hi + vec3(offset + 0.01f);
    }
    return b;
}

// --- Vernetzung (Surface Nets) ------------------------------------------------------------
// Duenn besetzt: das Gitter ist in Bloecke zu 8^3 Zellen geteilt; nur Bloecke nahe der Oberflaeche
// werden abgetastet und gespeichert. So bleiben auch feine Zellen (3-4 mm) fuer 2,5 m grosse Wesen
// im Speicher klein.
RawMesh mesh(const Shape& s, Aabb box, float h, JobSystem* jobs) {
    RawMesh out;
    if (!box.valid()) return out;
    const vec3 org = box.lo - vec3(2.0f * h);
    const int nx = (int)std::ceil((box.hi.x - box.lo.x) / h) + 5;
    const int ny = (int)std::ceil((box.hi.y - box.lo.y) / h) + 5;
    const int nz = (int)std::ceil((box.hi.z - box.lo.z) / h) + 5;
    const int cx = nx - 1, cy = ny - 1, cz = nz - 1;  // Zellen
    constexpr int B = 8, S1 = B + 1, S3 = S1 * S1 * S1, C3 = B * B * B;
    const int bx = (cx + B - 1) / B, by = (cy + B - 1) / B, bz = (cz + B - 1) / B;
    const int nb = bx * by * bz;
    const float blockR = h * B * 0.8660254f;
    auto P = [&](float x, float y, float z) { return org + vec3{x * h, y * h, z * h}; };
    auto par = [&](int n, const std::function<void(int)>& f) {
        if (jobs) jobs->parallelFor(n, f);
        else
            for (int i = 0; i < n; ++i) f(i);
    };

    // 1) Bloecke einordnen: weit von der Oberflaeche -> nur der Mittelwert
    std::vector<float> center((size_t)nb);
    std::vector<char> near((size_t)nb, 0);
    par(nb, [&](int b) {
        int ix = b % bx, iy = (b / bx) % by, iz = b / (bx * by);
        int x0 = ix * B, y0 = iy * B, z0 = iz * B;
        int x1 = std::min(cx, x0 + B), y1 = std::min(cy, y0 + B), z1 = std::min(cz, z0 + B);
        float dc = s.eval(P((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f));
        center[(size_t)b] = dc;
        near[(size_t)b] = std::fabs(dc) <= blockR * 1.8f + 0.012f ? 1 : 0;
    });
    std::vector<int> slot((size_t)nb, -1), blockOf;
    for (int b = 0; b < nb; ++b)
        if (near[(size_t)b]) slot[(size_t)b] = (int)blockOf.size(), blockOf.push_back(b);
    const int nn = (int)blockOf.size();
    std::vector<float> val((size_t)nn * S3);
    std::vector<int> cellLocal((size_t)nn * C3, -1);
    std::vector<std::vector<vec3>> bverts((size_t)nn);

    // 2) nahe Bloecke abtasten (inkl. der Flaechen zum Nachbarn); der Rand ist aussen
    par(nn, [&](int k) {
        const int b = blockOf[(size_t)k];
        int ix = b % bx, iy = (b / bx) % by, iz = b / (bx * by);
        int x0 = ix * B, y0 = iy * B, z0 = iz * B;
        float* v = &val[(size_t)k * S3];
        for (int z = 0; z < S1; ++z)
            for (int y = 0; y < S1; ++y)
                for (int x = 0; x < S1; ++x) {
                    int gx = std::min(x0 + x, nx - 1), gy = std::min(y0 + y, ny - 1), gz = std::min(z0 + z, nz - 1);
                    float d = s.eval(P((float)gx, (float)gy, (float)gz));
                    if (gx == 0 || gy == 0 || gz == 0 || gx == nx - 1 || gy == ny - 1 || gz == nz - 1) d = std::max(d, h);
                    v[(z * S1 + y) * S1 + x] = d;
                }
    });

    // 3) je Zelle mit Vorzeichenwechsel ein Punkt
    static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    par(nn, [&](int k) {
        const int b = blockOf[(size_t)k];
        int ix = b % bx, iy = (b / bx) % by, iz = b / (bx * by);
        int x0 = ix * B, y0 = iy * B, z0 = iz * B;
        int x1 = std::min(cx, x0 + B), y1 = std::min(cy, y0 + B), z1 = std::min(cz, z0 + B);
        const float* vb = &val[(size_t)k * S3];
        int* cl = &cellLocal[(size_t)k * C3];
        auto& verts = bverts[(size_t)k];
        for (int z = z0; z < z1; ++z)
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    float v[8];
                    int mask = 0;
                    for (int i = 0; i < 8; ++i) {
                        int lx = x - x0 + (i & 1), ly = y - y0 + ((i >> 1) & 1), lz = z - z0 + (i >> 2);
                        v[i] = vb[(lz * S1 + ly) * S1 + lx];
                        if (v[i] < 0) mask |= 1 << i;
                    }
                    if (mask == 0 || mask == 255) continue;
                    vec3 sum{};
                    int cnt = 0;
                    for (const auto& e : edges) {
                        float a = v[e[0]], bb = v[e[1]];
                        if ((a < 0) == (bb < 0)) continue;
                        float t = a / (a - bb);
                        vec3 pa{(float)(e[0] & 1), (float)((e[0] >> 1) & 1), (float)(e[0] >> 2)};
                        vec3 pb{(float)(e[1] & 1), (float)((e[1] >> 1) & 1), (float)(e[1] >> 2)};
                        sum += pa + (pb - pa) * t;
                        ++cnt;
                    }
                    vec3 lp = sum / (float)cnt;
                    cl[((z - z0) * B + (y - y0)) * B + (x - x0)] = (int)verts.size();
                    verts.push_back(P(x + lp.x, y + lp.y, z + lp.z));
                }
    });
    std::vector<int> base((size_t)nn + 1, 0);
    for (int k = 0; k < nn; ++k) base[(size_t)k + 1] = base[(size_t)k] + (int)bverts[(size_t)k].size();
    out.pos.resize((size_t)base[(size_t)nn]);
    for (int k = 0; k < nn; ++k) std::copy(bverts[(size_t)k].begin(), bverts[(size_t)k].end(), out.pos.begin() + base[(size_t)k]);
    bverts.clear();
    auto cellAt = [&](int x, int y, int z) -> int {
        if (x < 0 || y < 0 || z < 0 || x >= cx || y >= cy || z >= cz) return -1;
        int b = (z / B * by + y / B) * bx + x / B;
        int k = slot[(size_t)b];
        if (k < 0) return -1;
        int l = cellLocal[(size_t)k * C3 + (size_t)(((z % B) * B + (y % B)) * B + (x % B))];
        return l < 0 ? -1 : base[(size_t)k] + l;
    };

    // 4) Flaechen: je Kante mit Vorzeichenwechsel ein Viereck aus den vier anliegenden Zellen
    std::vector<std::vector<unsigned>> bidx((size_t)nn);
    par(nn, [&](int k) {
        const int b = blockOf[(size_t)k];
        int ix = b % bx, iy = (b / bx) % by, iz = b / (bx * by);
        int x0 = ix * B, y0 = iy * B, z0 = iz * B;
        int x1 = std::min(cx, x0 + B), y1 = std::min(cy, y0 + B), z1 = std::min(cz, z0 + B);
        const float* vb = &val[(size_t)k * S3];
        auto at = [&](int x, int y, int z) { return vb[((z - z0) * S1 + (y - y0)) * S1 + (x - x0)]; };
        auto& idx = bidx[(size_t)k];
        auto quad = [&](int a, int bq, int c, int d, bool flip) {
            if (a < 0 || bq < 0 || c < 0 || d < 0) return;
            if (flip) std::swap(bq, d);
            float d1 = length(out.pos[(size_t)a] - out.pos[(size_t)c]), d2 = length(out.pos[(size_t)bq] - out.pos[(size_t)d]);
            if (d1 <= d2) idx.insert(idx.end(), {(unsigned)a, (unsigned)bq, (unsigned)c, (unsigned)a, (unsigned)c, (unsigned)d});
            else idx.insert(idx.end(), {(unsigned)a, (unsigned)bq, (unsigned)d, (unsigned)bq, (unsigned)c, (unsigned)d});
        };
        for (int z = z0; z < z1; ++z)
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) {
                    const float a = at(x, y, z);
                    if (y >= 1 && z >= 1) {  // Kante in x
                        float bb = at(x + 1, y, z);
                        if ((a < 0) != (bb < 0)) quad(cellAt(x, y - 1, z - 1), cellAt(x, y, z - 1), cellAt(x, y, z), cellAt(x, y - 1, z), a < 0);
                    }
                    if (x >= 1 && z >= 1) {  // Kante in y
                        float bb = at(x, y + 1, z);
                        if ((a < 0) != (bb < 0)) quad(cellAt(x - 1, y, z - 1), cellAt(x - 1, y, z), cellAt(x, y, z), cellAt(x, y, z - 1), a < 0);
                    }
                    if (x >= 1 && y >= 1) {  // Kante in z
                        float bb = at(x, y, z + 1);
                        if ((a < 0) != (bb < 0)) quad(cellAt(x - 1, y - 1, z), cellAt(x, y - 1, z), cellAt(x, y, z), cellAt(x - 1, y, z), a < 0);
                    }
                }
    });
    size_t total = 0;
    for (const auto& v : bidx) total += v.size();
    out.idx.reserve(total);
    for (const auto& v : bidx) out.idx.insert(out.idx.end(), v.begin(), v.end());
    val.clear();
    val.shrink_to_fit();

    // Punkte auf die Oberflaeche ziehen, Normalen aus dem Gradienten
    out.normal.resize(out.pos.size());
    const float eps = h * 0.3f;
    auto refine = [&](int i) {
        vec3 p = out.pos[(size_t)i];
        vec3 g{};
        for (int it = 0; it < 2; ++it) {
            float d = s.eval(p);
            g = {s.eval(p + vec3{eps, 0, 0}) - s.eval(p - vec3{eps, 0, 0}), s.eval(p + vec3{0, eps, 0}) - s.eval(p - vec3{0, eps, 0}),
                 s.eval(p + vec3{0, 0, eps}) - s.eval(p - vec3{0, 0, eps})};
            g = g / (2.0f * eps);
            float gl2 = dot(g, g);
            if (gl2 < 1e-8f) break;
            vec3 step = g * (d / gl2);
            if (length(step) > h * 0.75f) step = normalize(step) * (h * 0.75f);  // nicht aus der Zelle springen
            p -= step;
        }
        out.pos[(size_t)i] = p;
        out.normal[(size_t)i] = normalize(g);
    };
    const int nv = (int)out.pos.size();
    if (jobs) jobs->parallelFor(nv, refine);
    else
        for (int i = 0; i < nv; ++i) refine(i);
    return out;
}

}  // namespace lim::gfx::sculpt
