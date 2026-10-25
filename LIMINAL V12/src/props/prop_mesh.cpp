#include "props/prop_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace lim::props {

namespace {
constexpr float kPi = 3.14159265358979f;

vec3 xform(const mat4& m, vec3 p, float w) {
    vec4 r = m * vec4{p.x, p.y, p.z, w};
    return {r.x, r.y, r.z};
}

void face(Mesh& m, vec3 a, vec3 b, vec3 c, vec3 d, vec3 n) {
    const u32 base = (u32)m.v.size();
    m.v.push_back({a, n, {0, 0}});
    m.v.push_back({b, n, {1, 0}});
    m.v.push_back({c, n, {1, 1}});
    m.v.push_back({d, n, {0, 1}});
    m.i.insert(m.i.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}
}  // namespace

void Mesh::add(const Mesh& o) {
    const u32 base = (u32)v.size();
    v.insert(v.end(), o.v.begin(), o.v.end());
    for (u32 k : o.i) i.push_back(k + base);
}

void Mesh::transform(const mat4& m) {
    for (Vtx& x : v) {
        x.p = xform(m, x.p, 1.0f);
        x.n = normalize(xform(m, x.n, 0.0f));
    }
}

void Mesh::translate(vec3 d) {
    for (Vtx& x : v) x.p += d;
}

Aabb Mesh::bounds() const {
    Aabb b;
    for (const Vtx& x : v) b.add(x.p);
    return b;
}

Mesh box(vec3 lo, vec3 hi) {
    Mesh m;
    const vec3 a = lo, b = hi;
    face(m, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {b.x, a.y, b.z}, {1, 0, 0});
    face(m, {a.x, b.y, a.z}, {a.x, a.y, a.z}, {a.x, a.y, b.z}, {a.x, b.y, b.z}, {-1, 0, 0});
    face(m, {b.x, b.y, a.z}, {a.x, b.y, a.z}, {a.x, b.y, b.z}, {b.x, b.y, b.z}, {0, 1, 0});
    face(m, {a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}, {0, -1, 0});
    face(m, {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}, {0, 0, 1});
    face(m, {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, a.y, a.z}, {a.x, a.y, a.z}, {0, 0, -1});
    return m;
}

Mesh bevelBox(vec3 lo, vec3 hi, float bv) {
    const vec3 ext = hi - lo;
    bv = std::min(bv, 0.45f * std::min(ext.x, std::min(ext.y, ext.z)));
    if (bv <= 1e-4f) return box(lo, hi);
    // 8 Ecken je 3 Punkte; Flaechen: 6 Hauptflaechen, 12 Fasen, 8 Eckdreiecke
    Mesh m;
    auto P = [&](int sx, int sy, int sz, int axis) {  // Eckpunkt, um bv entlang der beiden anderen Achsen nach innen
        vec3 c{sx ? hi.x : lo.x, sy ? hi.y : lo.y, sz ? hi.z : lo.z};
        vec3 in{sx ? -bv : bv, sy ? -bv : bv, sz ? -bv : bv};
        if (axis != 0) c.x += in.x;
        if (axis != 1) c.y += in.y;
        if (axis != 2) c.z += in.z;
        return c;
    };
    // Hauptflaechen (um bv verkleinert)
    face(m, P(1, 0, 0, 0), P(1, 1, 0, 0), P(1, 1, 1, 0), P(1, 0, 1, 0), {1, 0, 0});
    face(m, P(0, 1, 0, 0), P(0, 0, 0, 0), P(0, 0, 1, 0), P(0, 1, 1, 0), {-1, 0, 0});
    face(m, P(1, 1, 0, 1), P(0, 1, 0, 1), P(0, 1, 1, 1), P(1, 1, 1, 1), {0, 1, 0});
    face(m, P(0, 0, 0, 1), P(1, 0, 0, 1), P(1, 0, 1, 1), P(0, 0, 1, 1), {0, -1, 0});
    face(m, P(0, 0, 1, 2), P(1, 0, 1, 2), P(1, 1, 1, 2), P(0, 1, 1, 2), {0, 0, 1});
    face(m, P(0, 1, 0, 2), P(1, 1, 0, 2), P(1, 0, 0, 2), P(0, 0, 0, 2), {0, 0, -1});
    // Fasen entlang x (zwischen y- und z-Flaechen), entlang y, entlang z
    const float s = 0.70710678f;
    for (int sy = 0; sy < 2; ++sy)
        for (int sz = 0; sz < 2; ++sz) {
            vec3 n{0, sy ? s : -s, sz ? s : -s};
            vec3 a = P(0, sy, sz, 1), b = P(1, sy, sz, 1), c = P(1, sy, sz, 2), d = P(0, sy, sz, 2);
            if ((sy ^ sz) == 0) face(m, a, b, c, d, n);
            else face(m, b, a, d, c, n);
        }
    for (int sx = 0; sx < 2; ++sx)
        for (int sz = 0; sz < 2; ++sz) {
            vec3 n{sx ? s : -s, 0, sz ? s : -s};
            vec3 a = P(sx, 0, sz, 0), b = P(sx, 1, sz, 0), c = P(sx, 1, sz, 2), d = P(sx, 0, sz, 2);
            if ((sx ^ sz) == 1) face(m, a, b, c, d, n);
            else face(m, b, a, d, c, n);
        }
    for (int sx = 0; sx < 2; ++sx)
        for (int sy = 0; sy < 2; ++sy) {
            vec3 n{sx ? s : -s, sy ? s : -s, 0};
            vec3 a = P(sx, sy, 0, 0), b = P(sx, sy, 0, 1), c = P(sx, sy, 1, 1), d = P(sx, sy, 1, 0);
            if ((sx ^ sy) == 0) face(m, a, b, c, d, n);
            else face(m, b, a, d, c, n);
        }
    // Ecken
    const float k = 0.57735027f;
    for (int sx = 0; sx < 2; ++sx)
        for (int sy = 0; sy < 2; ++sy)
            for (int sz = 0; sz < 2; ++sz) {
                vec3 n{sx ? k : -k, sy ? k : -k, sz ? k : -k};
                vec3 a = P(sx, sy, sz, 0), b = P(sx, sy, sz, 1), c = P(sx, sy, sz, 2);
                const u32 base = (u32)m.v.size();
                m.v.push_back({a, n, {0, 0}});
                m.v.push_back({b, n, {1, 0}});
                m.v.push_back({c, n, {0, 1}});
                if (dot(cross(b - a, c - a), n) > 0) m.i.insert(m.i.end(), {base, base + 1, base + 2});
                else m.i.insert(m.i.end(), {base, base + 2, base + 1});
            }
    // Ausrichtung aller Flaechen pruefen (Normale nach aussen)
    for (size_t t = 0; t + 2 < m.i.size(); t += 3) {
        Vtx &A = m.v[m.i[t]], &B = m.v[m.i[t + 1]], &C = m.v[m.i[t + 2]];
        if (dot(cross(B.p - A.p, C.p - A.p), A.n) < 0) std::swap(m.i[t + 1], m.i[t + 2]);
    }
    return m;
}

Mesh cone(vec3 a, vec3 b, float ra, float rb, int seg, bool caps) {
    Mesh m;
    vec3 ax = normalize(b - a);
    vec3 t = std::fabs(ax.z) < 0.9f ? normalize(cross(ax, vec3{0, 0, 1})) : normalize(cross(ax, vec3{1, 0, 0}));
    vec3 u = cross(t, ax);
    const float len = length(b - a);
    const float slope = (ra - rb) / std::max(len, 1e-4f);
    for (int s = 0; s <= seg; ++s) {
        float ang = 2.0f * kPi * (float)s / (float)seg;
        vec3 d = t * std::cos(ang) + u * std::sin(ang);
        vec3 n = normalize(d + ax * slope);
        m.v.push_back({a + d * ra, n, {(float)s / seg, 0}});
        m.v.push_back({b + d * rb, n, {(float)s / seg, 1}});
    }
    for (int s = 0; s < seg; ++s) {
        u32 i0 = (u32)(s * 2), i1 = i0 + 1, i2 = i0 + 2, i3 = i0 + 3;
        m.i.insert(m.i.end(), {i0, i2, i3, i0, i3, i1});
    }
    if (caps) {
        for (int e = 0; e < 2; ++e) {
            const vec3 c = e ? b : a;
            const float r = e ? rb : ra;
            if (r <= 1e-5f) continue;
            const vec3 n = e ? ax : -ax;
            const u32 base = (u32)m.v.size();
            m.v.push_back({c, n, {0.5f, 0.5f}});
            for (int s = 0; s <= seg; ++s) {
                float ang = 2.0f * kPi * (float)s / (float)seg;
                vec3 d = t * std::cos(ang) + u * std::sin(ang);
                m.v.push_back({c + d * r, n, {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
            }
            for (int s = 0; s < seg; ++s) {
                u32 p = base + 1 + (u32)s, q = p + 1;
                if (e) m.i.insert(m.i.end(), {base, p, q});
                else m.i.insert(m.i.end(), {base, q, p});
            }
        }
    }
    return m;
}

Mesh cylinder(vec3 a, vec3 b, float r, int seg, bool caps) { return cone(a, b, r, r, seg, caps); }

Mesh sphere(vec3 c, vec3 r, int seg, int rings) {
    Mesh m;
    for (int j = 0; j <= rings; ++j) {
        float th = kPi * (float)j / (float)rings;
        for (int s = 0; s <= seg; ++s) {
            float ph = 2.0f * kPi * (float)s / (float)seg;
            vec3 d{std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)};
            vec3 n = normalize(vec3{d.x / r.x, d.y / r.y, d.z / r.z});
            m.v.push_back({c + d * r, n, {(float)s / seg, (float)j / rings}});
        }
    }
    for (int j = 0; j < rings; ++j)
        for (int s = 0; s < seg; ++s) {
            u32 i0 = (u32)(j * (seg + 1) + s), i1 = i0 + 1, i2 = i0 + (u32)(seg + 1), i3 = i2 + 1;
            m.i.insert(m.i.end(), {i0, i2, i1, i1, i2, i3});
        }
    return m;
}

Mesh quad(vec3 o, vec3 eu, vec3 ev, vec2 uv0, vec2 uv1, bool doubleSided) {
    Mesh m;
    vec3 n = normalize(cross(ev, eu));
    const u32 b = 0;
    m.v.push_back({o, n, {uv0.x, uv0.y}});
    m.v.push_back({o + eu, n, {uv1.x, uv0.y}});
    m.v.push_back({o + eu + ev, n, {uv1.x, uv1.y}});
    m.v.push_back({o + ev, n, {uv0.x, uv1.y}});
    m.i = {b, b + 2, b + 1, b, b + 3, b + 2};
    // Wicklung an der Normale ausrichten (Vorderseite = n)
    if (dot(cross(m.v[2].p - m.v[0].p, m.v[1].p - m.v[0].p), n) < 0) m.i = {b, b + 1, b + 2, b, b + 2, b + 3};
    if (doubleSided) {
        Mesh back = m;
        for (Vtx& x : back.v) x.n = -x.n;
        for (size_t t = 0; t < back.i.size(); t += 3) std::swap(back.i[t + 1], back.i[t + 2]);
        m.add(back);
    }
    return m;
}

Mesh prism(const std::vector<vec2>& o, float z0, float z1) {
    Mesh m;
    const size_t n = o.size();
    if (n < 3) return m;
    // Deckel (Faecher - fuer konvexe oder sternfoermige Umrisse)
    for (int e = 0; e < 2; ++e) {
        const float z = e ? z1 : z0;
        const vec3 nn{0, 0, e ? 1.0f : -1.0f};
        const u32 base = (u32)m.v.size();
        for (const vec2& p : o) m.v.push_back({{p.x, p.y, z}, nn, {p.x, p.y}});
        for (size_t k = 1; k + 1 < n; ++k) {
            if (e) m.i.insert(m.i.end(), {base, base + (u32)k, base + (u32)k + 1});
            else m.i.insert(m.i.end(), {base, base + (u32)k + 1, base + (u32)k});
        }
    }
    for (size_t k = 0; k < n; ++k) {
        const vec2 a = o[k], b = o[(k + 1) % n];
        vec3 nn = normalize(vec3{b.y - a.y, -(b.x - a.x), 0});
        face(m, {a.x, a.y, z0}, {b.x, b.y, z0}, {b.x, b.y, z1}, {a.x, a.y, z1}, nn);
    }
    return m;
}

Mesh pipePath(const std::vector<vec3>& pts, float r, int seg, float bendR) {
    Mesh m;
    if (pts.size() < 2) return m;
    // Ecken mit kurzen Boegen: jeder innere Punkt wird durch zwei Punkte im Abstand bendR ersetzt
    // und mit 4 Zwischenstuecken verbunden
    std::vector<vec3> path{pts.front()};
    for (size_t k = 1; k + 1 < pts.size(); ++k) {
        vec3 a = pts[k - 1], c = pts[k], b = pts[k + 1];
        vec3 da = normalize(a - c), db = normalize(b - c);
        float br = std::min(bendR, 0.45f * std::min(length(a - c), length(b - c)));
        vec3 p0 = c + da * br, p1 = c + db * br;
        for (int s = 0; s <= 4; ++s) {
            float t = (float)s / 4.0f;
            vec3 q = p0 * ((1 - t) * (1 - t)) + c * (2 * (1 - t) * t) + p1 * (t * t);  // quadratische Bezier
            path.push_back(q);
        }
    }
    path.push_back(pts.back());
    for (size_t k = 0; k + 1 < path.size(); ++k) {
        if (length(path[k + 1] - path[k]) < 1e-4f) continue;
        m.add(cylinder(path[k], path[k + 1], r, seg, k == 0 || k + 2 == path.size()));
    }
    return m;
}

std::vector<vec2> roundRect(float w, float z0, float z1, float r, int pc) {
    std::vector<vec2> ring;
    const float hw = w * 0.5f;
    r = std::min(r, std::min(hw, (z1 - z0) * 0.5f));
    const vec2 c[4] = {{hw - r, z0 + r}, {hw - r, z1 - r}, {-hw + r, z1 - r}, {-hw + r, z0 + r}};
    const float a0[4] = {-kPi * 0.5f, 0.0f, kPi * 0.5f, kPi};
    for (int k = 0; k < 4; ++k)
        for (int s = 0; s <= pc; ++s) {
            float a = a0[k] + kPi * 0.5f * (float)s / (float)pc;
            ring.push_back({c[k].x + r * std::cos(a), c[k].y + r * std::sin(a)});
        }
    return ring;
}

Mesh loft(const std::vector<Section>& s, bool capEnds) {
    Mesh m;
    if (s.size() < 2) return m;
    const size_t n = s[0].ring.size();
    for (size_t k = 0; k < s.size(); ++k)
        for (size_t j = 0; j < n; ++j) m.v.push_back({{s[k].x, s[k].ring[j].x, s[k].ring[j].y}, {0, 0, 1}, {(float)j / n, (float)k / s.size()}});
    for (size_t k = 0; k + 1 < s.size(); ++k)
        for (size_t j = 0; j < n; ++j) {
            u32 a = (u32)(k * n + j), b = (u32)(k * n + (j + 1) % n), c = (u32)((k + 1) * n + (j + 1) % n), d = (u32)((k + 1) * n + j);
            m.i.insert(m.i.end(), {a, c, b, a, d, c});
        }
    // Normalen aus den Flaechen (glatt)
    std::vector<vec3> acc(m.v.size(), vec3{0, 0, 0});
    for (size_t t = 0; t < m.i.size(); t += 3) {
        vec3 A = m.v[m.i[t]].p, B = m.v[m.i[t + 1]].p, C = m.v[m.i[t + 2]].p;
        vec3 fn = cross(B - A, C - A);
        for (int q = 0; q < 3; ++q) acc[m.i[t + (size_t)q]] += fn;
    }
    for (size_t k = 0; k < m.v.size(); ++k) m.v[k].n = normalize(acc[k]);
    // Ausrichtung: Normalen sollen vom Schwerpunkt jedes Querschnitts weg zeigen (sonst alles umdrehen)
    float side = 0.0f;
    for (size_t k = 0; k < s.size(); ++k) {
        vec2 c{0, 0};
        for (const vec2& p : s[k].ring) c += p;
        c = c / (float)n;
        for (size_t j = 0; j < n; ++j) {
            const Vtx& x = m.v[k * n + j];
            side += x.n.y * (x.p.y - c.x) + x.n.z * (x.p.z - c.y);
        }
    }
    if (side < 0.0f) {
        for (Vtx& x : m.v) x.n = -x.n;
        for (size_t t = 0; t + 2 < m.i.size(); t += 3) std::swap(m.i[t + 1], m.i[t + 2]);
    }
    if (capEnds) {
        for (int e = 0; e < 2; ++e) {
            const Section& sec = e ? s.back() : s.front();
            vec3 nn{e ? 1.0f : -1.0f, 0, 0};
            vec2 ctr{0, 0};
            for (const vec2& p : sec.ring) ctr += p;
            ctr = ctr / (float)n;
            const u32 base = (u32)m.v.size();
            m.v.push_back({{sec.x, ctr.x, ctr.y}, nn, {0.5f, 0.5f}});
            for (const vec2& p : sec.ring) m.v.push_back({{sec.x, p.x, p.y}, nn, {0, 0}});
            for (size_t j = 0; j < n; ++j) {
                u32 p = base + 1 + (u32)j, q = base + 1 + (u32)((j + 1) % n);
                if (e) m.i.insert(m.i.end(), {base, p, q});
                else m.i.insert(m.i.end(), {base, q, p});
            }
        }
    }
    // Wicklung pruefen: Dreiecke sollen zu ihrer Normale passen
    for (size_t t = 0; t + 2 < m.i.size(); t += 3) {
        Vtx &A = m.v[m.i[t]], &B = m.v[m.i[t + 1]], &C = m.v[m.i[t + 2]];
        vec3 fn = cross(B.p - A.p, C.p - A.p);
        if (dot(fn, A.n + B.n + C.n) < 0) std::swap(m.i[t + 1], m.i[t + 2]);
    }
    return m;
}

void smoothNormals(Mesh& m, float creaseDeg) {
    const float cosC = std::cos(creaseDeg * kPi / 180.0f);
    std::vector<vec3> fn(m.i.size() / 3);
    for (size_t t = 0; t < fn.size(); ++t) {
        vec3 A = m.v[m.i[t * 3]].p, B = m.v[m.i[t * 3 + 1]].p, C = m.v[m.i[t * 3 + 2]].p;
        fn[t] = normalize(cross(B - A, C - A));
    }
    auto key = [](vec3 p) {
        auto q = [](float f) { return (u64)(i64)std::lround(f * 2000.0f); };
        u64 h = q(p.x) * 0x9E3779B97F4A7C15ull;
        h ^= q(p.y) + 0x632BE59BD9B4E019ull + (h << 6) + (h >> 2);
        h ^= q(p.z) + 0x85EBCA77C2B2AE63ull + (h << 6) + (h >> 2);
        return h;
    };
    std::unordered_map<u64, std::vector<size_t>> at;
    for (size_t t = 0; t < fn.size(); ++t)
        for (int k = 0; k < 3; ++k) at[key(m.v[m.i[t * 3 + (size_t)k]].p)].push_back(t);
    std::vector<vec3> out(m.v.size());
    for (size_t t = 0; t < fn.size(); ++t)
        for (int k = 0; k < 3; ++k) {
            const u32 vi = m.i[t * 3 + (size_t)k];
            vec3 s{0, 0, 0};
            for (size_t o : at[key(m.v[vi].p)])
                if (dot(fn[o], fn[t]) >= cosC) s += fn[o];
            out[vi] = normalize(s);
        }
    for (size_t k = 0; k < m.v.size(); ++k) m.v[k].n = out[k];
}

}  // namespace lim::props
