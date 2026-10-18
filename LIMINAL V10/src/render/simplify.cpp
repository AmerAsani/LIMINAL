// V10: Netzvereinfachung nach Garland/Heckbert (Quadric Error Metrics).
//
// Je Punkt eine Fehlerquadrik aus den Ebenen der anliegenden Dreiecke; Kanten werden in der
// Reihenfolge des kleinsten Fehlers zusammengezogen (neuer Punkt im Minimum der Quadrik, sonst
// Endpunkt/Mitte). Zuege, die ein Dreieck umklappen oder entarten lassen, werden verworfen.
// Danach werden die Punkte auf die Oberflaeche des Feldes zurueckgezogen und die Normalen aus
// dem Gradienten neu bestimmt - die Vereinfachung behaelt so Gesicht und Finger und spart auf
// grossen, glatten Flaechen.
#include <algorithm>
#include <cmath>
#include <queue>

#include "core/jobs.hpp"
#include "render/sculpt.hpp"

namespace lim::gfx::sculpt {

namespace {

struct Quadric {
    double a[10] = {};  // symmetrische 4x4: xx xy xz xw yy yz yw zz zw ww
    void addPlane(double nx, double ny, double nz, double d, double w) {
        a[0] += w * nx * nx, a[1] += w * nx * ny, a[2] += w * nx * nz, a[3] += w * nx * d;
        a[4] += w * ny * ny, a[5] += w * ny * nz, a[6] += w * ny * d;
        a[7] += w * nz * nz, a[8] += w * nz * d;
        a[9] += w * d * d;
    }
    void add(const Quadric& o) {
        for (int i = 0; i < 10; ++i) a[i] += o.a[i];
    }
    double eval(double x, double y, double z) const {
        return a[0] * x * x + 2 * a[1] * x * y + 2 * a[2] * x * z + 2 * a[3] * x + a[4] * y * y + 2 * a[5] * y * z + 2 * a[6] * y +
               a[7] * z * z + 2 * a[8] * z + a[9];
    }
    bool optimum(double& x, double& y, double& z) const {
        double m00 = a[0], m01 = a[1], m02 = a[2], m11 = a[4], m12 = a[5], m22 = a[7];
        double det = m00 * (m11 * m22 - m12 * m12) - m01 * (m01 * m22 - m12 * m02) + m02 * (m01 * m12 - m11 * m02);
        if (std::fabs(det) < 1e-12) return false;
        double bx = -a[3], by = -a[6], bz = -a[8];
        double inv = 1.0 / det;
        x = inv * (bx * (m11 * m22 - m12 * m12) - m01 * (by * m22 - m12 * bz) + m02 * (by * m12 - m11 * bz));
        y = inv * (m00 * (by * m22 - m12 * bz) - bx * (m01 * m22 - m12 * m02) + m02 * (m01 * bz - by * m02));
        z = inv * (m00 * (m11 * bz - by * m12) - m01 * (m01 * bz - by * m02) + bx * (m01 * m12 - m11 * m02));
        return true;
    }
};

struct Cand {
    double cost;
    unsigned a, b;
    unsigned va, vb;  // Versionen der Punkte beim Einreihen
    vec3 p;
    bool operator>(const Cand& o) const { return cost > o.cost; }
};

}  // namespace

void simplify(RawMesh& m, size_t targetTris) {
    const size_t nv = m.pos.size();
    size_t nf = m.idx.size() / 3;
    if (nf <= targetTris || nv < 8) return;
    std::vector<unsigned> tri(m.idx);
    std::vector<char> faceAlive(nf, 1);
    std::vector<std::vector<unsigned>> vf(nv);  // Punkt -> Dreiecke
    for (size_t f = 0; f < nf; ++f)
        for (int k = 0; k < 3; ++k) vf[tri[f * 3 + k]].push_back((unsigned)f);
    std::vector<Quadric> q(nv);
    std::vector<vec3> pos(m.pos);
    auto faceNormal = [&](size_t f, vec3* outN, double* area) {
        vec3 a = pos[tri[f * 3]], b = pos[tri[f * 3 + 1]], c = pos[tri[f * 3 + 2]];
        vec3 n = cross(b - a, c - a);
        float l = length(n);
        if (area) *area = l * 0.5;
        *outN = l > 1e-12f ? n / l : vec3(0, 0, 0);
    };
    for (size_t f = 0; f < nf; ++f) {
        vec3 n;
        double area;
        faceNormal(f, &n, &area);
        double d = -dot(n, pos[tri[f * 3]]);
        Quadric fq;
        fq.addPlane(n.x, n.y, n.z, d, std::max(area, 1e-9));
        for (int k = 0; k < 3; ++k) q[tri[f * 3 + k]].add(fq);
    }
    std::vector<unsigned> ver(nv, 0);
    std::vector<char> alive(nv, 1);
    std::priority_queue<Cand, std::vector<Cand>, std::greater<Cand>> heap;
    auto candidate = [&](unsigned a, unsigned b) {
        Quadric s = q[a];
        s.add(q[b]);
        double x, y, z;
        vec3 p;
        vec3 mid = (pos[a] + pos[b]) * 0.5f;
        if (s.optimum(x, y, z) && length(vec3{(float)x, (float)y, (float)z} - mid) < length(pos[a] - pos[b]) * 2.0f)
            p = {(float)x, (float)y, (float)z};
        else {
            double ca = s.eval(pos[a].x, pos[a].y, pos[a].z), cb = s.eval(pos[b].x, pos[b].y, pos[b].z), cm = s.eval(mid.x, mid.y, mid.z);
            p = ca < cb ? (ca < cm ? pos[a] : mid) : (cb < cm ? pos[b] : mid);
        }
        double cost = std::max(0.0, s.eval(p.x, p.y, p.z));
        heap.push({cost, a, b, ver[a], ver[b], p});
    };
    for (size_t f = 0; f < nf; ++f)
        for (int k = 0; k < 3; ++k) {
            unsigned a = tri[f * 3 + k], b = tri[f * 3 + (k + 1) % 3];
            if (a < b) candidate(a, b);
        }
    size_t faces = nf;
    std::vector<unsigned> touched;
    while (faces > targetTris && !heap.empty()) {
        Cand c = heap.top();
        heap.pop();
        if (!alive[c.a] || !alive[c.b] || ver[c.a] != c.va || ver[c.b] != c.vb) continue;
        // Faltenpruefung: kein anliegendes Dreieck darf umklappen oder zu spitz werden
        bool ok = true;
        for (unsigned v : {c.a, c.b}) {
            for (unsigned f : vf[v]) {
                if (!faceAlive[f]) continue;
                unsigned i0 = tri[f * 3], i1 = tri[f * 3 + 1], i2 = tri[f * 3 + 2];
                bool hasA = i0 == c.a || i1 == c.a || i2 == c.a, hasB = i0 == c.b || i1 == c.b || i2 == c.b;
                if (hasA && hasB) continue;  // verschwindet
                vec3 before;
                faceNormal(f, &before, nullptr);
                vec3 p0 = pos[i0], p1 = pos[i1], p2 = pos[i2];
                if (i0 == v) p0 = c.p;
                if (i1 == v) p1 = c.p;
                if (i2 == v) p2 = c.p;
                vec3 n = cross(p1 - p0, p2 - p0);
                float l = length(n);
                if (l < 1e-12f || dot(n / l, before) < 0.25f) {
                    ok = false;
                    break;
                }
                // Seitenverhaeltnis: keine Splitter
                float e = std::max({length(p1 - p0), length(p2 - p1), length(p0 - p2)});
                if (l / (e * e) < 0.06f) {
                    ok = false;
                    break;
                }
            }
            if (!ok) break;
        }
        if (!ok) continue;
        // b in a ueberfuehren
        pos[c.a] = c.p;
        q[c.a].add(q[c.b]);
        alive[c.b] = 0;
        ++ver[c.a];
        ++ver[c.b];
        for (unsigned f : vf[c.b]) {
            if (!faceAlive[f]) continue;
            unsigned* t = &tri[f * 3];
            bool hasA = t[0] == c.a || t[1] == c.a || t[2] == c.a;
            if (hasA) {
                faceAlive[f] = 0;
                --faces;
                continue;
            }
            for (int k = 0; k < 3; ++k)
                if (t[k] == c.b) t[k] = c.a;
            vf[c.a].push_back(f);
        }
        vf[c.b].clear();
        // tote Dreiecke aus der Liste von a entfernen, Nachbarn neu bewerten
        std::erase_if(vf[c.a], [&](unsigned f) { return !faceAlive[f]; });
        touched.clear();
        for (unsigned f : vf[c.a])
            for (int k = 0; k < 3; ++k) {
                unsigned v = tri[f * 3 + k];
                if (v != c.a) touched.push_back(v);
            }
        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
        for (unsigned v : touched) candidate(std::min(c.a, v), std::max(c.a, v));
    }
    // verdichten
    std::vector<int> remap(nv, -1);
    RawMesh out;
    for (size_t f = 0; f < nf; ++f) {
        if (!faceAlive[f]) continue;
        for (int k = 0; k < 3; ++k) {
            unsigned v = tri[f * 3 + k];
            if (remap[v] < 0) {
                remap[v] = (int)out.pos.size();
                out.pos.push_back(pos[v]);
                out.normal.push_back(m.normal[v]);
            }
            out.idx.push_back((unsigned)remap[v]);
        }
    }
    m = std::move(out);
}

void reproject(RawMesh& m, const Shape& s, float h, JobSystem* jobs) {
    const float eps = std::max(h * 0.25f, 0.0008f);
    auto one = [&](int i) {
        vec3 p = m.pos[(size_t)i];
        vec3 g{};
        for (int it = 0; it < 2; ++it) {
            float d = s.eval(p);
            g = {s.eval(p + vec3{eps, 0, 0}) - s.eval(p - vec3{eps, 0, 0}), s.eval(p + vec3{0, eps, 0}) - s.eval(p - vec3{0, eps, 0}),
                 s.eval(p + vec3{0, 0, eps}) - s.eval(p - vec3{0, 0, eps})};
            g = g / (2.0f * eps);
            float gl2 = dot(g, g);
            if (gl2 < 1e-8f) break;
            vec3 step = g * (d / gl2);
            if (length(step) > h) step = normalize(step) * h;
            p -= step;
        }
        m.pos[(size_t)i] = p;
        if (dot(g, g) > 1e-10f) m.normal[(size_t)i] = normalize(g);
    };
    if (jobs) jobs->parallelFor((int)m.pos.size(), one);
    else
        for (int i = 0; i < (int)m.pos.size(); ++i) one(i);
}

}  // namespace lim::gfx::sculpt
