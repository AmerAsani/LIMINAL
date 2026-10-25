// V10: Materialtexturen der Entities (siehe texture_entity.hpp). Alle Muster sind kachelbar
// (periodisches Rauschen mit ganzzahligen Frequenzen je Kachel); v laeuft am Koerper nach oben.
// Ausnahme: das Fluegelmuster ist ein Atlas der Ruhepose.
#include "render/texture_entity.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>

#include "core/jobs.hpp"
#include "render/moth_wing.hpp"
#include "render/noise.hpp"
#include "render/texture_compress.hpp"

namespace lim::gfx {

const EntLayerDesc kEntLayers[EL_COUNT] = {
    {"haut", 0.7f, 1.0f},
    {"haar", 0.4f, 0.9f},
    {"horn", 0.25f, 0.8f},
    {"zahn", 0.12f, 0.6f},
    {"fluegel", 0.5f, 0.7f},  // Atlas: 0,5 m = Kachel um den Ursprung (render/moth_wing.hpp)
    {"panzer", 0.15f, 0.8f},
    {"pelz", 0.12f, 0.8f},
    {"nest", 0.8f, 1.0f},
};
const EntLayerDesc kMicroLayers[ML_COUNT] = {
    {"poren", 0.08f, 0.45f},
    {"fasern", 0.07f, 0.5f},
    {"haarrisse", 0.1f, 0.35f},
    {"schuppen", 0.02f, 0.4f},
};

namespace {

using noise::Cell;
using noise::r01;
using noise::worley;

struct Tex {
    float r = 0.5f, g = 0.5f, b = 0.5f;
    float h = 0.0f;  // Hoehe (m)
    float rough = 0.5f, ao = 1.0f;
};

float sat(float x) { return std::clamp(x, 0.0f, 1.0f); }
float sm(float a, float b, float x) {
    float t = sat((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
struct C3 {
    float r, g, b;
};
C3 mixc(C3 a, C3 b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
C3 mulc(C3 a, float k) { return {a.r * k, a.g * k, a.b * k}; }
Tex make(C3 c, float h, float rough, float ao) {
    Tex t;
    t.r = sat(c.r), t.g = sat(c.g), t.b = sat(c.b), t.h = h, t.rough = sat(rough), t.ao = sat(ao);
    return t;
}

// schnelles periodisches Gradientenrauschen (16 feste Richtungen statt Winkelfunktionen)
struct Grad {
    float v[16][2];
    Grad() {
        for (int i = 0; i < 16; ++i) v[i][0] = std::cos(i * 0.39269908f), v[i][1] = std::sin(i * 0.39269908f);
    }
};
const Grad kGrad;
inline int wrapi(int v, int p) {
    int m = v % p;
    return m < 0 ? m + p : m;
}
inline float gn(float x, float y, int px, int py, std::uint32_t seed) {
    const int ix = (int)std::floor(x), iy = (int)std::floor(y);
    const float fx = x - (float)ix, fy = y - (float)iy;
    const int x0 = wrapi(ix, px), x1 = wrapi(ix + 1, px), y0 = wrapi(iy, py), y1 = wrapi(iy + 1, py);
    auto g = [&](int gx, int gy, float dx, float dy) {
        const float* d = kGrad.v[noise::h2(gx, gy, seed) & 15u];
        return d[0] * dx + d[1] * dy;
    };
    const float n00 = g(x0, y0, fx, fy), n10 = g(x1, y0, fx - 1, fy), n01 = g(x0, y1, fx, fy - 1), n11 = g(x1, y1, fx - 1, fy - 1);
    const float u = noise::fade(fx), v = noise::fade(fy);
    const float a = n00 + (n10 - n00) * u, b = n01 + (n11 - n01) * u;
    return (a + (b - a) * v) * 1.41421356f;
}
// Rauschen in einer Kachel: fu, fv Zellen je Kachel (ganzzahlig, duerfen verschieden sein)
inline float N(float u, float v, int fu, int fv, std::uint32_t seed) { return gn(u * (float)fu, v * (float)fv, fu, fv, seed); }
inline float fbm(float u, float v, int fu, int fv, int oct, std::uint32_t seed, float gain = 0.5f) {
    float s = 0.0f, a = 1.0f, n = 0.0f;
    for (int o = 0; o < oct; ++o) {
        s += N(u, v, fu, fv, seed + (std::uint32_t)o * 131u) * a;
        n += a;
        a *= gain;
        fu *= 2, fv *= 2;
    }
    return s / n;
}
inline float ridge(float u, float v, int f, std::uint32_t seed, float sharp) { return std::pow(1.0f - std::fabs(N(u, v, f, f, seed)), sharp); }

// --- Ebenen -----------------------------------------------------------------------------------
float sq(float x) { return x * x; }

// Hound: fahle, kraenkliche Haut; Raeude mit kahlen, rohen Flecken und Schorf (Fandom), Kratzspuren,
// Haarstoppeln, Adern unter der duennen Haut, Schmutz
Tex skin(float u, float v) {
    const float macro = fbm(u, v, 3, 3, 4, 31);
    const float mott = fbm(u, v, 14, 14, 3, 44);  // fleckig: gelblich-grau, an duennen Stellen violett-grau
    C3 col = mixc({0.40f, 0.37f, 0.31f}, {0.33f, 0.33f, 0.33f}, sm(-0.3f, 0.4f, macro));
    col = mixc(col, {0.36f, 0.31f, 0.34f}, sm(0.1f, 0.6f, mott) * 0.5f);
    col = mixc(col, {0.45f, 0.42f, 0.30f}, sm(-0.6f, -0.2f, mott) * 0.4f);
    const float mg = fbm(u, v, 6, 6, 4, 32) + 0.35f * N(u, v, 30, 30, 33);
    const float mange = sm(0.18f, 0.42f, mg);
    const Cell cr = worley(u, v, 90, 34);
    const float crust = mange * (1.0f - sm(0.15f, 0.45f, cr.d1)) * (r01(cr.id) > 0.35f ? 1.0f : 0.0f);
    const float wu = u + 0.03f * N(u, v, 8, 8, 35), wv = v + 0.03f * N(u, v, 8, 8, 36);
    const float vein = std::min(1.0f, std::pow(1.0f - std::fabs(N(wu, wv, 12, 12, 37)), 14.0f) +
                                          0.5f * std::pow(1.0f - std::fabs(N(wu, wv, 30, 30, 38)), 18.0f));
    // Kratzspuren: duenne Streifen in zwei Richtungen, nur fleckweise
    const float scrMask = sm(0.25f, 0.6f, fbm(u, v, 5, 5, 3, 39) * 0.5f + 0.5f);
    const float scr = std::max(std::pow(1.0f - std::fabs(N(u, v, 14, 160, 40)), 22.0f), std::pow(1.0f - std::fabs(N(u, v, 160, 14, 41)), 22.0f)) * scrMask;
    const Cell st = worley(u, v, 380, 42);
    const float stub = (1.0f - mange) * (1.0f - sm(0.04f, 0.16f, st.d1));
    const float grime = sm(0.2f, 0.8f, fbm(u, v, 5, 5, 3, 43) * 0.5f + 0.5f);
    col = mixc(col, {0.40f, 0.42f, 0.47f}, vein * 0.35f);
    col = mixc(col, {0.45f, 0.35f, 0.31f}, mange * 0.6f);   // rohe, entzuendete Stellen
    col = mixc(col, {0.21f, 0.16f, 0.12f}, crust * 0.75f);  // Schorf
    col = mixc(col, {0.36f, 0.25f, 0.22f}, scr * 0.6f);     // Kratzer
    col = mulc(col, 1.0f - 0.35f * stub);
    col = mixc(col, {0.17f, 0.14f, 0.11f}, grime * 0.4f);
    const float h = 0.0003f * macro + 0.00025f * crust - 0.00022f * scr + 0.00005f * stub + 0.0001f * vein - 0.0001f * mange;
    const float rough = 0.62f + 0.15f * crust - 0.25f * mange * (1.0f - crust) + 0.1f * grime;
    return make(col, h, rough, 1.0f - 0.3f * crust - 0.2f * scr);
}

// Hound: schwarzes, straehniges Haar - Straehnen entlang v, Buendel, einzelne graue Haare, fettig
Tex hair(float u, float v) {
    const float wu = u + 0.02f * N(u, v, 6, 3, 51);
    const float s1 = N(wu, v, 260, 6, 52), s2 = N(wu, v, 620, 14, 53), s3 = N(wu, v, 1400, 30, 54);
    const float strand = 0.5f * s1 + 0.3f * s2 + 0.2f * s3;
    const float clump = N(wu, v, 40, 3, 55);
    const float oily = sm(0.2f, 0.7f, fbm(u, v, 5, 3, 3, 56) * 0.5f + 0.5f);
    const float grey = sm(0.82f, 0.9f, N(wu, v, 900, 8, 57) * 0.5f + 0.5f);
    C3 col = mulc({0.035f, 0.03f, 0.028f}, 0.75f + 0.5f * (strand * 0.5f + 0.5f));
    col = mixc(col, {0.09f, 0.075f, 0.06f}, sm(0.3f, 0.9f, s2) * 0.4f);
    col = mixc(col, {0.22f, 0.21f, 0.2f}, grey * 0.5f);
    const float h = 0.00035f * strand + 0.0004f * clump;
    return make(col, h, 0.55f - 0.25f * oily + 0.1f * (s3 * 0.5f + 0.5f), 0.75f + 0.25f * (strand * 0.5f + 0.5f));
}

// Horn: Wachstumsrillen, Bernsteinschlieren, Absplitterungen (Krallen)
Tex horn(float u, float v) {
    const float g = 0.6f * N(u, v, 6, 90, 21) + 0.4f * N(u, v, 14, 240, 22);
    const float mot = fbm(u, v, 4, 4, 3, 23);
    const Cell c = worley(u, v, 20, 24);
    const float chip = (r01(c.id) > 0.85f ? 1.0f : 0.0f) * (1.0f - sm(0.05f, 0.18f, c.d1));
    C3 col = mixc({0.075f, 0.065f, 0.055f}, {0.2f, 0.15f, 0.09f}, sm(0.2f, 0.7f, g) * 0.6f);
    col = mixc(col, {0.24f, 0.21f, 0.17f}, chip);
    return make(col, 0.00012f * g - 0.0004f * chip, 0.32f + 0.1f * mot + 0.35f * chip, 1.0f - 0.3f * chip);
}

// Zahnschmelz: vergilbt, braune Flecken, Wachstumslinien, feine Risse
Tex tooth(float u, float v) {
    const float yel = sm(-0.2f, 0.6f, fbm(u, v, 4, 4, 4, 61));
    const float stain = sm(0.35f, 0.75f, fbm(u, v, 10, 10, 3, 62) * 0.5f + 0.5f);
    const Cell c = worley(u, v, 30, 63);
    const float crack = 1.0f - sm(0.0f, 0.015f, c.d2 - c.d1);
    const float band = 0.5f + 0.5f * N(u, v, 3, 60, 64);
    C3 col = mixc({0.86f, 0.83f, 0.74f}, {0.74f, 0.66f, 0.48f}, yel * 0.6f);
    col = mixc(col, {0.45f, 0.36f, 0.24f}, stain * 0.45f);
    col = mixc(col, {0.3f, 0.24f, 0.17f}, crack * 0.5f);
    col = mulc(col, 0.93f + 0.07f * band);
    return make(col, -0.00005f * crack + 0.00002f * band, 0.22f + 0.2f * stain + 0.15f * crack, 1.0f - 0.3f * crack);
}

// Deathmoth-Fluegel: Atlas der Ruhepose. Ein Texel entspricht einem Punkt (x, |y|) in der Ebene der
// ausgebreiteten Fluegel; Muster in Fluegelkoordinaten (Winkel von der Wurzel, Anteil am Rand), damit
// Adern strahlenfoermig laufen und Binden dem Aussenrand folgen.
Tex wingTex(float u, float v) {
    const float m = 0.5f;  // = kEntLayers[EL_WING].meters
    const float x = (u - std::floor(u + 0.5f)) * m, y = std::fabs((v - std::floor(v + 0.5f)) * m);
    float th = 0.0f, r = 0.0f, rho = 2.0f;
    int w = -1;
    for (int k = 0; k < 2 && w < 0; ++k)
        if (wing::polar(k, x, y, th, r, rho) && rho <= 1.05f) w = k;
    if (w < 0) return make({0.2f, 0.18f, 0.16f}, 0.0f, 0.8f, 1.0f);
    const wing::Def& d = wing::def(w);
    const float a = (th - d.th1) / (d.th0 - d.th1);  // 0 Innenrand .. 1 Vorderkante
    rho = std::min(rho, 1.0f);
    const float rEdge = r / std::max(rho, 1e-3f);
    // Grundton: grau mit braunen Wolken
    const float mot = fbm(u, v, 24, 24, 4, 71);
    const float cloud = fbm(u, v, 10, 10, 3, 72);
    C3 col = mixc({0.34f, 0.32f, 0.29f}, {0.27f, 0.22f, 0.17f}, sm(-0.2f, 0.5f, cloud));
    col = mulc(col, 0.85f + 0.3f * (mot * 0.5f + 0.5f));
    // Adern: strahlenfoermig von der Wurzel, leicht gebogen; eine Querader schliesst die Mittelzelle
    const int nv = w == 0 ? 9 : 7;
    float vein = 0.0f;
    for (int k = 0; k < nv; ++k) {
        const float ak = ((float)k + 0.5f) / (float)nv;
        const float tk = d.th1 + (d.th0 - d.th1) * ak + 0.06f * rho * rho * (ak - 0.5f);
        const float dist = std::fabs(th - tk) * r;
        const float wd = 0.00045f + 0.0005f * (1.0f - rho);
        if (rho > 0.12f) vein = std::max(vein, std::exp(-dist * dist / (wd * wd)));
    }
    vein = std::max(vein, std::exp(-sq((rho - 0.42f) * rEdge / 0.0006f)) * sm(0.2f, 0.35f, a) * (1.0f - sm(0.7f, 0.85f, a)));
    // Querbinden: wellig, parallel zum Aussenrand; Mittelfeld dunkler, dunkles Saumfeld, heller Saum
    const float wav = 0.025f * std::sin(a * 38.0f) + 0.012f * std::sin(a * 91.0f + 1.3f);
    const float am = std::exp(-sq((rho - 0.34f - wav) / 0.022f));
    const float pm = std::exp(-sq((rho - 0.7f + wav * 1.4f) / 0.02f));
    const float medial = sm(0.36f, 0.42f, rho) * (1.0f - sm(0.64f, 0.7f, rho));
    const float sub = sm(0.8f, 0.86f, rho) * (1.0f - sm(0.95f, 0.98f, rho));
    const float fringe = sm(0.975f, 1.0f, rho);
    const float hairy = 1.0f - sm(0.12f, 0.32f, rho);
    col = mixc(col, mulc(col, 0.62f), medial * 0.6f);
    col = mixc(col, {0.1f, 0.085f, 0.075f}, std::max(am, pm) * 0.85f);
    col = mixc(col, {0.14f, 0.12f, 0.1f}, sub * 0.55f);
    col = mixc(col, {0.48f, 0.45f, 0.4f}, fringe * 0.7f);
    col = mixc(col, {0.12f, 0.1f, 0.085f}, vein * 0.5f);
    col = mixc(col, {0.45f, 0.43f, 0.4f}, hairy * 0.55f * (0.6f + 0.4f * N(u, v, 300, 300, 73)));
    // Mittelfleck des Vorderfluegels: dunkler Ring mit hellerem Kern
    if (w == 0) {
        const float tc = d.th1 + (d.th0 - d.th1) * 0.55f, rc = 0.5f * wing::rmax(0, tc);
        const float cx = d.rx + rc * std::sin(tc), cy = d.ry + rc * std::cos(tc);
        const float dd = std::hypot(x - cx, (y - cy) * 0.8f);
        col = mixc(col, {0.07f, 0.06f, 0.05f}, std::exp(-sq((dd - 0.0065f) / 0.0016f)) * 0.9f);
        col = mixc(col, {0.42f, 0.38f, 0.32f}, (1.0f - sm(0.0035f, 0.005f, dd)) * 0.6f);
    }
    // abgeriebene Schuppen: dunklere, glattere, durchscheinende Stellen
    const float worn = sm(0.55f, 0.75f, fbm(u, v, 14, 14, 3, 74) * 0.5f + 0.5f) * sm(0.5f, 0.9f, rho);
    col = mixc(col, {0.22f, 0.2f, 0.17f}, worn * 0.4f);
    const float hairS = N(u, v, 700, 700, 75);
    const float h = 0.00012f * vein + 0.00004f * hairS * (0.4f + 0.6f * hairy) + 0.00003f * mot;
    return make(col, h, 0.82f + 0.1f * hairy - 0.12f * worn, 1.0f - 0.12f * vein - 0.12f * std::max(am, pm));
}

// Deathmoth-Brust: dunkelbrauner, gefleckter Panzer mit unregelmaessiger Oberflaeche (Fandom)
Tex chitin(float u, float v) {
    const float macro = fbm(u, v, 4, 4, 4, 81);
    const Cell sp = worley(u, v, 26, 82);
    const float spot = (r01(sp.id) > 0.5f ? 1.0f : 0.0f) * (1.0f - sm(0.2f, 0.45f, sp.d1));
    const Cell pt = worley(u, v, 140, 83);
    const float pits = 1.0f - sm(0.0f, 0.25f, pt.d1);
    const float rug = std::pow(1.0f - std::fabs(N(u, v, 40, 40, 84)), 5.0f);
    C3 col = mixc({0.16f, 0.105f, 0.065f}, {0.24f, 0.165f, 0.1f}, sm(-0.3f, 0.5f, macro));
    col = mixc(col, {0.06f, 0.04f, 0.03f}, spot * 0.8f);
    col = mulc(col, 1.0f - 0.25f * pits);
    const float h = 0.00008f * macro - 0.00005f * pits + 0.00004f * rug;
    return make(col, h, 0.32f + 0.12f * pits + 0.08f * rug, 1.0f - 0.3f * pits);
}

// Deathmoth-Hinterleib: feines, graues Haar (Fandom)
Tex fur(float u, float v) {
    const float s1 = N(u, v, 300, 40, 91), s2 = N(u, v, 900, 90, 92);
    const float tuft = fbm(u, v, 20, 20, 3, 93);
    C3 col = mixc({0.36f, 0.34f, 0.31f}, {0.27f, 0.24f, 0.2f}, sm(-0.3f, 0.5f, tuft));
    col = mulc(col, 0.82f + 0.18f * (0.6f * s1 + 0.4f * s2) + 0.1f);
    const float h = 0.0001f * s1 + 0.00006f * s2 + 0.00012f * tuft;
    return make(col, h, 0.88f, 0.8f + 0.2f * (s1 * 0.5f + 0.5f));
}

// Nest: fleischig, geadert, glaenzendes Harz, Einschluesse aus verdautem Material (Wikidot)
Tex nest(float u, float v) {
    const float macro = fbm(u, v, 3, 3, 4, 101);
    const float flesh = fbm(u, v, 8, 8, 4, 102);
    const float wu = u + 0.03f * N(u, v, 6, 6, 103), wv = v + 0.03f * N(u, v, 6, 6, 104);
    const float vein = std::min(1.0f, std::pow(1.0f - std::fabs(N(wu, wv, 12, 12, 105)), 10.0f) +
                                          0.6f * std::pow(1.0f - std::fabs(N(wu, wv, 30, 30, 106)), 14.0f));
    const float resin = sm(-0.05f, 0.3f, fbm(u, v, 5, 5, 4, 107) + 0.2f * N(u, v, 24, 24, 108));
    const Cell fb = worley(u, v, 60, 109);
    const float bits = (r01(fb.id) > 0.72f ? 1.0f : 0.0f) * (1.0f - sm(0.08f, 0.3f, fb.d1)) * (0.6f + 0.4f * N(u, v, 400, 400, 110));
    // Fleisch: blass graurosa bis dunkel blutrot, darin violette Adern; Harz in Lachen (dunkles Bernstein)
    C3 col = mixc({0.46f, 0.31f, 0.31f}, {0.27f, 0.13f, 0.14f}, sm(-0.4f, 0.5f, flesh));
    col = mixc(col, {0.40f, 0.36f, 0.35f}, sm(0.35f, 0.7f, macro * 0.5f + 0.5f) * 0.35f);
    col = mixc(col, {0.2f, 0.08f, 0.12f}, vein * 0.45f);
    col = mixc(col, {0.33f, 0.19f, 0.06f}, resin * 0.55f);  // Harz: dunkles Bernstein
    col = mixc(col, {0.45f, 0.42f, 0.36f}, bits * 0.5f);
    col = mulc(col, 0.85f + 0.25f * (macro * 0.5f + 0.5f));
    const float h = 0.0006f * flesh + 0.0002f * vein - 0.0002f * resin + 0.0002f * bits;
    return make(col, h, 0.6f - 0.5f * resin + 0.15f * bits, 1.0f - 0.25f * vein);
}

Tex layerSample(int layer, float u, float v) {
    switch (layer) {
        case EL_SKIN: return skin(u, v);
        case EL_HAIR: return hair(u, v);
        case EL_HORN: return horn(u, v);
        case EL_TOOTH: return tooth(u, v);
        case EL_WING: return wingTex(u, v);
        case EL_CHITIN: return chitin(u, v);
        case EL_FUR: return fur(u, v);
        default: return nest(u, v);
    }
}

// Mikroebenen: nur Hoehe (-> Normalen)
float microHeight(int layer, float u, float v) {
    switch (layer) {
        case ML_PORES: {
            const Cell c = worley(u, v, 70, 121);
            return -0.00003f * (1.0f - sm(0.0f, 0.3f, c.d1)) + 0.00001f * N(u, v, 256, 256, 122);
        }
        case ML_FIBER: return 0.00002f * (0.6f * N(u, v, 500, 30, 123) + 0.4f * N(u, v, 30, 500, 124));
        case ML_CRAZE: {
            const Cell c = worley(u, v, 24, 125);
            return -0.00002f * (1.0f - sm(0.0f, 0.02f, c.d2 - c.d1)) + 0.000006f * N(u, v, 300, 300, 126);
        }
        default: {
            // Schuppen der Fluegel: versetzte Reihen, jede Schuppe ein flacher Bogen
            const float rows = 64.0f, cols = 64.0f;
            const float rv = v * rows, row = std::floor(rv), g = rv - row;
            const float cu = u * cols + 0.5f * (float)((int)row & 1), f = cu - std::floor(cu);
            const float sc = std::sin(3.14159265f * f) * std::sqrt(std::max(0.0f, 1.0f - g));
            return 0.000012f * sc + 0.000003f * N(u, v, 256, 256, 127);
        }
    }
}

// --- Aufbereitung -----------------------------------------------------------------------------
float toLin(u8 v) { return std::pow((float)v / 255.0f, 2.2f); }
u8 toSrgb(float l) { return (u8)std::lround(std::pow(sat(l), 1.0f / 2.2f) * 255.0f); }

void normalsFromHeight(const std::vector<float>& H, int size, float texel, float strength, std::vector<u8>& out, JobSystem& jobs) {
    out.resize((size_t)size * size * 2);
    jobs.parallelFor(size, [&](int y) {
        for (int x = 0; x < size; ++x) {
            auto h = [&](int xx, int yy) { return H[(size_t)((yy + size) % size) * size + (size_t)((xx + size) % size)]; };
            float dx = (h(x + 1, y) - h(x - 1, y)) / (2.0f * texel) * strength;
            float dy = (h(x, y + 1) - h(x, y - 1)) / (2.0f * texel) * strength;
            float nx = -dx, ny = -dy, nz = 1.0f;
            float l = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= l, ny /= l;
            size_t i = ((size_t)y * size + x) * 2;
            out[i] = (u8)std::lround(sat(nx * 0.5f + 0.5f) * 255.0f);
            out[i + 1] = (u8)std::lround(sat(ny * 0.5f + 0.5f) * 255.0f);
        }
    });
}

// halbe Groesse: Farbe linear gemittelt, Normalen neu normiert, sonst einfach gemittelt
void downRGBA(const std::vector<u8>& src, int s, std::vector<u8>& dst, JobSystem& jobs) {
    const int d = s / 2;
    dst.resize((size_t)d * d * 4);
    jobs.parallelFor(d, [&](int y) {
        for (int x = 0; x < d; ++x) {
            float acc[4] = {0, 0, 0, 0};
            for (int k = 0; k < 4; ++k) {
                const u8* p = &src[(((size_t)(2 * y + (k >> 1))) * s + (size_t)(2 * x + (k & 1))) * 4];
                for (int c = 0; c < 3; ++c) acc[c] += toLin(p[c]);
                acc[3] += p[3];
            }
            u8* o = &dst[((size_t)y * d + x) * 4];
            for (int c = 0; c < 3; ++c) o[c] = toSrgb(acc[c] * 0.25f);
            o[3] = (u8)std::lround(acc[3] * 0.25f);
        }
    });
}
void downRG(const std::vector<u8>& src, int s, std::vector<u8>& dst, bool normal, JobSystem& jobs) {
    const int d = s / 2;
    dst.resize((size_t)d * d * 2);
    jobs.parallelFor(d, [&](int y) {
        for (int x = 0; x < d; ++x) {
            float a = 0, b = 0, z = 0;
            for (int k = 0; k < 4; ++k) {
                const u8* p = &src[(((size_t)(2 * y + (k >> 1))) * s + (size_t)(2 * x + (k & 1))) * 2];
                float nx = p[0] / 255.0f, ny = p[1] / 255.0f;
                if (normal) {
                    nx = nx * 2 - 1, ny = ny * 2 - 1;
                    z += std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                }
                a += nx, b += ny;
            }
            a *= 0.25f, b *= 0.25f, z *= 0.25f;
            if (normal) {
                float l = std::sqrt(a * a + b * b + z * z);
                if (l > 1e-5f) a /= l, b /= l;
                a = a * 0.5f + 0.5f, b = b * 0.5f + 0.5f;
            }
            u8* o = &dst[((size_t)y * d + x) * 2];
            o[0] = (u8)std::lround(sat(a) * 255.0f), o[1] = (u8)std::lround(sat(b) * 255.0f);
        }
    });
}

// blockkomprimieren, parallel in Streifen zu 4 Zeilen
void bc1(const std::vector<u8>& rgba, int s, u8* out, JobSystem& jobs) {
    const int strips = std::max(1, s / 4);
    jobs.parallelFor(strips, [&](int k) { compressBC1(rgba.data() + (size_t)k * 4 * s * 4, s, 4, out + (size_t)k * (s / 4) * 8); });
}
void bc5(const std::vector<u8>& rg, int s, u8* out, JobSystem& jobs) {
    const int strips = std::max(1, s / 4);
    jobs.parallelFor(strips, [&](int k) { compressBC5(rg.data() + (size_t)k * 4 * s * 2, s, 4, out + (size_t)k * (s / 4) * 16); });
}

int mipCount(int size) {
    int m = 0;
    while ((size >> m) >= 4) ++m;
    return m;
}

}  // namespace

size_t EntityTextures::offset(int sz, int mps, int layer, int mip, int blockBytes) const {
    size_t perLayer = 0;
    for (int m = 0; m < mps; ++m) {
        size_t b = (size_t)std::max(1, (sz >> m) / 4);
        perLayer += b * b * (size_t)blockBytes;
    }
    size_t off = perLayer * (size_t)layer;
    for (int m = 0; m < mip; ++m) {
        size_t b = (size_t)std::max(1, (sz >> m) / 4);
        off += b * b * (size_t)blockBytes;
    }
    return off;
}

EntityTextures generateEntityTextures(int size, JobSystem& jobs) {
    const auto t0 = std::chrono::steady_clock::now();
    EntityTextures t;
    t.size = size;
    t.mips = mipCount(size);
    t.albedo.resize(t.offset(size, t.mips, EL_COUNT, 0, 8));
    t.normal.resize(t.offset(size, t.mips, EL_COUNT, 0, 16));
    t.orm.resize(t.offset(size, t.mips, EL_COUNT, 0, 16));
    const float px = 1.0f / (float)size;
    for (int layer = 0; layer < EL_COUNT; ++layer) {
        std::vector<float> H((size_t)size * size);
        std::vector<u8> A((size_t)size * size * 4), O((size_t)size * size * 2), Nn;
        jobs.parallelFor(size, [&](int y) {
            for (int x = 0; x < size; ++x) {
                const Tex s = layerSample(layer, ((float)x + 0.5f) * px, ((float)y + 0.5f) * px);
                const size_t i = (size_t)y * size + x;
                H[i] = s.h;
                A[i * 4 + 0] = (u8)std::lround(s.r * 255.0f);
                A[i * 4 + 1] = (u8)std::lround(s.g * 255.0f);
                A[i * 4 + 2] = (u8)std::lround(s.b * 255.0f);
                A[i * 4 + 3] = 255;
                O[i * 2 + 0] = (u8)std::lround(s.ao * 255.0f);
                O[i * 2 + 1] = (u8)std::lround(s.rough * 255.0f);
            }
        });
        normalsFromHeight(H, size, kEntLayers[layer].meters * px, 1.0f, Nn, jobs);
        H.clear();
        H.shrink_to_fit();
        int s = size;
        for (int m = 0; m < t.mips; ++m) {
            bc1(A, s, t.albedo.data() + t.offset(size, t.mips, layer, m, 8), jobs);
            bc5(Nn, s, t.normal.data() + t.offset(size, t.mips, layer, m, 16), jobs);
            bc5(O, s, t.orm.data() + t.offset(size, t.mips, layer, m, 16), jobs);
            if (m + 1 < t.mips) {
                std::vector<u8> a2, n2, o2;
                downRGBA(A, s, a2, jobs);
                downRG(Nn, s, n2, true, jobs);
                downRG(O, s, o2, false, jobs);
                A.swap(a2), Nn.swap(n2), O.swap(o2);
                s /= 2;
            }
        }
    }
    // Mikroebenen (feste Groesse)
    const int ms = 1024;
    t.microSize = ms;
    t.microMips = mipCount(ms);
    t.micro.resize(t.offset(ms, t.microMips, ML_COUNT, 0, 16));
    const float mpx = 1.0f / (float)ms;
    for (int layer = 0; layer < ML_COUNT; ++layer) {
        std::vector<float> H((size_t)ms * ms);
        jobs.parallelFor(ms, [&](int y) {
            for (int x = 0; x < ms; ++x) H[(size_t)y * ms + x] = microHeight(layer, ((float)x + 0.5f) * mpx, ((float)y + 0.5f) * mpx);
        });
        std::vector<u8> Nn;
        normalsFromHeight(H, ms, kMicroLayers[layer].meters * mpx, 1.0f, Nn, jobs);
        int s = ms;
        for (int m = 0; m < t.microMips; ++m) {
            bc5(Nn, s, t.micro.data() + t.offset(ms, t.microMips, layer, m, 16), jobs);
            if (m + 1 < t.microMips) {
                std::vector<u8> n2;
                downRG(Nn, s, n2, true, jobs);
                Nn.swap(n2);
                s /= 2;
            }
        }
    }
    t.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return t;
}

}  // namespace lim::gfx
