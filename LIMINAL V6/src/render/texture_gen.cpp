#include "render/texture_gen.hpp"
#include "render/texture_compress.hpp"
#include "render/texture_vending.hpp"

#include <algorithm>
#include <cmath>

#include "core/jobs.hpp"
#include "core/log.hpp"
#include "render/noise.hpp"

namespace lim::gfx {

using namespace noise;

//                                    Name         Meter  Normal Rauh  Variation
const LayerDesc kLayers[L_COUNT] = {
    {"plaster", 2.0f, 1.0f, 0.0f, 0.55f},    {"carpet", 2.0f, 1.0f, 0.0f, 0.9f},
    {"slabs", 4.0f, 1.0f, 0.0f, 1.0f},       {"bigtiles", 3.0f, 1.0f, 0.0f, 0.45f},
    {"grate", 0.5f, 1.0f, 0.0f, 0.8f},       {"ceiltile", 1.0f, 1.0f, 0.0f, 0.4f},
    {"floortile", 1.0f, 1.0f, 0.0f, 0.4f},   {"checker", 2.0f, 1.0f, 0.0f, 0.35f},
    {"wallpaper", 1.1f, 1.0f, 0.0f, 0.8f},   {"blocks", 1.6f, 1.0f, 0.0f, 0.7f},
    {"bricks", 1.0f, 1.0f, 0.0f, 0.9f},      {"panels", 1.5f, 1.0f, 0.0f, 0.5f},
    {"concrete", 2.4f, 1.0f, 0.0f, 0.9f},    {"wood", 1.0f, 1.0f, 0.0f, 0.3f},
    {"metal", 1.0f, 1.0f, 0.0f, 0.5f},       {"laminate", 1.0f, 1.0f, 0.0f, 0.25f},
    {"fabric", 0.5f, 1.0f, 0.0f, 0.4f},      {"monolith", 4.0f, 1.0f, 0.0f, 0.1f},
    {"diffuser", 0.1f, 0.6f, 0.0f, 0.0f},    {"crate", 1.0f, 1.0f, 0.0f, 0.5f},
    {"vending", 1.0f, 1.0f, 0.0f, 0.0f},     {"trim", 1.0f, 1.0f, 0.0f, 0.3f},
    {"item_energy", 1.0f, 1.0f, 0.0f, 0.0f}, {"item_almond", 1.0f, 1.0f, 0.0f, 0.0f},
    {"vend_body", 1.0f, 1.0f, 0.0f, 0.0f},
};

namespace {

struct Surf {
    float r = 0.9f, g = 0.9f, b = 0.9f;
    float h = 0.0f;       // Hoehe in Metern
    float rough = 0.8f;
    float ao = 1.0f;
    float metal = 0.0f;
    void gray(float v) { r = g = b = v; }
    void rgb(float rr, float gg, float bb) { r = rr, g = gg, b = bb; }
    void mul(float k) { r *= k, g *= k, b *= k; }
};

// Raster mit optionalem Versatz jeder zweiten Reihe (Laeuferverband).
struct GridHit {
    float lu, lv;      // Position in der Zelle [0,1)
    float edge;        // Abstand zur naechsten Fuge (Meter)
    int cu, cv;        // Zellindex
};
GridHit gridAt(float x, float y, float cellW, float cellH, bool bond, int cellsU) {
    int cv = (int)std::floor(y / cellH);
    float xo = (bond && (cv & 1)) ? x + cellW * 0.5f : x;
    int cu = (int)std::floor(xo / cellW);
    GridHit g;
    g.lu = xo / cellW - (float)cu;
    g.lv = y / cellH - (float)cv;
    g.edge = std::min(std::min(g.lu, 1.0f - g.lu) * cellW, std::min(g.lv, 1.0f - g.lv) * cellH);
    g.cu = ((cu % cellsU) + cellsU) % cellsU;
    g.cv = cv;
    return g;
}

// Fuge: 0 in der Fugenmitte, 1 auf der Flaeche (weicher Uebergang = Fase)
float jointMask(float edge, float halfWidth, float bevel) { return smooth(halfWidth, halfWidth + bevel, edge); }

// --- Ebenen ------------------------------------------------------------------------------
Surf plaster(float u, float v, float px) {
    Surf s;
    float n = fbm(u, v, 6, 5, 11);
    float stip = value(u * 380.0f, v * 380.0f, 380, 380, 12) * 0.6f + value(u * 900.0f, v * 900.0f, 900, 900, 13) * 0.4f;
    float stain = smooth(0.25f, 0.7f, fbm(u, v, 2, 4, 14) * 0.5f + 0.5f);
    s.gray(0.9f * (1.0f + 0.035f * n));
    s.r *= 1.0f - 0.05f * stain;
    s.g *= 1.0f - 0.06f * stain;
    s.b *= 1.0f - 0.1f * stain;
    s.h = 0.00035f * stip + 0.0006f * n;
    s.rough = 0.86f + 0.06f * n;
    return s;
}

Surf carpet(float u, float v, float px) {
    Surf s;
    const float m = 2.0f, tile = 0.5f;
    GridHit g = gridAt(u * m, v * m, tile, tile, false, 4);
    // Schlingenflor: feines, leicht gerichtetes Rauschen
    bool turn = ((g.cu + g.cv) & 1) != 0;
    float fu = turn ? v : u, fv = turn ? u : v;
    float fib = value(fu * 700.0f, fv * 520.0f, 700, 520, 21) * 0.55f + value(u * 1400.0f, v * 1400.0f, 1400, 1400, 22) * 0.45f;
    float rows = 0.5f + 0.5f * std::sin(fv * 520.0f * 6.2831853f);
    float speck = r01(h2((int)(u * 480.0f), (int)(v * 480.0f), 23));
    float wear = fbm(u, v, 3, 4, 24) * 0.5f + 0.5f;
    float dirt = smooth(0.55f, 0.85f, fbm(u, v, 2, 5, 25) * 0.5f + 0.5f);
    float base = 0.82f + 0.14f * (fib - 0.5f) + 0.03f * (rows - 0.5f) + (turn ? 0.007f : -0.007f);
    if (speck > 0.985f) base *= 0.7f;
    else if (speck < 0.012f) base *= 1.18f;
    base *= 1.0f - 0.12f * dirt - 0.05f * (wear - 0.5f);
    s.gray(base);
    s.b *= 0.97f;
    float seam = jointMask(g.edge, 0.0008f, 0.0025f);
    s.mul(0.75f + 0.25f * seam);
    s.h = 0.0012f * (fib - 0.5f) * seam - 0.0015f * (1.0f - seam);
    s.rough = 0.96f;
    s.ao = 0.8f + 0.2f * fib;
    return s;
}

Surf slabs(float u, float v, float px) {
    Surf s;
    const float m = 4.0f;
    GridHit g = gridAt(u * m, v * m, 2.0f, 2.0f, false, 2);
    float tint = 1.0f + 0.05f * (r01(h2(g.cu, g.cv, 31)) - 0.5f);
    float cloud = fbm(u, v, 4, 6, 32);
    Cell pores = worley(u, v, 90, 33);
    float pore = 1.0f - smooth(0.0f, 0.12f, pores.d1);
    float trowel = fbm(u, v * 0.5f, 8, 3, 34);
    float joint = jointMask(g.edge, 0.004f, 0.004f);
    float base = 0.78f * tint * (1.0f + 0.09f * cloud + 0.03f * trowel) * (1.0f - 0.35f * pore);
    s.gray(base * (0.55f + 0.45f * joint));
    s.r *= 1.01f;
    s.h = 0.0005f * cloud - 0.0006f * pore - 0.004f * (1.0f - joint);
    s.rough = std::clamp(0.78f + 0.12f * trowel - 0.1f * smooth(0.3f, 0.7f, cloud * 0.5f + 0.5f), 0.4f, 0.95f);
    s.ao = 0.75f + 0.25f * joint;
    return s;
}

Surf bigtiles(float u, float v, float px) {
    Surf s;
    const float m = 3.0f;
    GridHit g = gridAt(u * m, v * m, 1.5f, 1.5f, false, 2);
    float tint = 1.0f + 0.03f * (r01(h2(g.cu, g.cv, 41)) - 0.5f);
    // Terrazzo/Marmor: verwirbelte Adern und feine Koernung
    float w = fbm(u + 0.3f * fbm(u, v, 3, 4, 42), v, 3, 5, 43);
    float vein = 1.0f - smooth(0.0f, 0.06f, std::fabs(w));
    Cell ch = worley(u, v, 160, 44);
    float chip = 1.0f - smooth(0.05f, 0.25f, ch.d1);
    float chipTone = r01(ch.id);
    float base = 0.9f * tint * (1.0f - 0.08f * vein) * (1.0f + (chipTone - 0.5f) * 0.12f * chip);
    float joint = jointMask(g.edge, 0.002f, 0.0015f);
    s.gray(base * (0.78f + 0.22f * joint));
    s.b *= 1.01f;
    s.h = -0.0015f * (1.0f - joint);
    float smudge = fbm(u, v, 5, 4, 45) * 0.5f + 0.5f;
    s.rough = joint > 0.5f ? 0.1f + 0.14f * smudge : 0.85f;
    s.ao = 0.85f + 0.15f * joint;
    return s;
}

Surf grate(float u, float v, float px) {
    Surf s;
    const float m = 0.5f;
    float x = u * m, y = v * m;
    float bearing = lineDist(x, m / 15.0f);   // Tragstaebe alle 3.3 cm
    float cross = lineDist(y, m / 5.0f);      // Querstaebe alle 10 cm
    float barA = 1.0f - smooth(0.0022f, 0.0032f, bearing);
    float barB = 1.0f - smooth(0.0026f, 0.0036f, cross);
    float bar = std::max(barA, barB);
    float rust = smooth(0.35f, 0.75f, fbm(u, v, 4, 5, 51) * 0.5f + 0.5f);
    float scr = fbm(u * 3.0f, v * 0.3f, 16, 3, 52);
    if (bar > 0.01f) {
        s.gray(0.62f + 0.08f * scr);
        s.r = s.r * (1.0f - rust) + 0.55f * rust;
        s.g = s.g * (1.0f - rust) + 0.32f * rust;
        s.b = s.b * (1.0f - rust) + 0.2f * rust;
        s.metal = 0.85f * (1.0f - rust);
        s.rough = 0.38f + 0.45f * rust;
        s.h = barA > barB ? 0.0f : -0.004f;
        s.ao = 1.0f;
    } else {
        s.gray(0.05f);
        s.h = -0.03f;
        s.rough = 0.9f;
        s.ao = 0.25f;
    }
    float mixb = std::clamp(bar, 0.0f, 1.0f);
    if (mixb < 1.0f && mixb > 0.0f) s.h = -0.03f * (1.0f - mixb);
    return s;
}

Surf ceiltile(float u, float v, float px) {
    Surf s;
    const float m = 1.0f;
    GridHit g = gridAt(u * m, v * m, 1.0f, 1.0f, false, 1);
    // Mineralfaser: Wurmloecher und Nadelstiche
    Cell w1 = worley(u, v, 34, 61);
    float worm = 1.0f - smooth(0.0f, 0.05f, std::fabs(w1.d2 - w1.d1) - 0.02f);
    float wormMask = smooth(0.2f, 0.6f, fbm(u, v, 12, 3, 62) * 0.5f + 0.5f);
    Cell pin = worley(u, v, 120, 63);
    float pinhole = 1.0f - smooth(0.02f, 0.08f, pin.d1);
    float grain = value(u * 600.0f, v * 600.0f, 600, 600, 64);
    float body = 0.93f * (1.0f - 0.25f * worm * wormMask - 0.3f * pinhole) * (0.97f + 0.06f * grain);
    // T-Profil: 2 cm breit, leicht erhaben, glatter
    float tbar = 1.0f - smooth(0.009f, 0.011f, g.edge);
    float lip = smooth(0.011f, 0.03f, g.edge);
    if (tbar > 0.5f) {
        s.gray(0.97f);
        s.rough = 0.35f;
        s.h = 0.002f;
        s.metal = 0.0f;
    } else {
        s.gray(body);
        s.rough = 0.95f;
        s.h = -0.004f * (1.0f - lip) - 0.001f * (worm * wormMask + pinhole);
        s.ao = 0.7f + 0.3f * lip;
    }
    return s;
}

Surf floortile(float u, float v, float px) {
    Surf s;
    const float m = 1.0f;
    GridHit g = gridAt(u * m, v * m, 0.5f, 0.5f, false, 2);
    float tint = 1.0f + 0.025f * (r01(h2(g.cu, g.cv, 71)) - 0.5f);
    float joint = jointMask(g.edge, 0.0015f, 0.0012f);
    float glaze = fbm(u, v, 8, 3, 72);
    s.gray(0.95f * tint * (1.0f + 0.01f * glaze));
    if (joint < 0.5f) s.gray(0.62f);
    s.h = -0.0015f * (1.0f - joint) + 0.0002f * glaze;
    s.rough = joint > 0.5f ? 0.08f + 0.05f * (glaze * 0.5f + 0.5f) : 0.9f;
    s.ao = 0.8f + 0.2f * joint;
    return s;
}

Surf checker(float u, float v, float px) {
    Surf s;
    const float m = 2.0f;
    GridHit g = gridAt(u * m, v * m, 1.0f, 1.0f, false, 2);
    bool dark = ((g.cu + g.cv) & 1) != 0;
    float wear = fbm(u, v, 6, 5, 81) * 0.5f + 0.5f;
    float scratch = std::fabs(fbm(u * 4.0f, v * 0.2f, 32, 2, 82));
    float joint = jointMask(g.edge, 0.001f, 0.0012f);
    float base = dark ? 0.3f + 0.04f * wear : 0.96f - 0.05f * wear;
    s.gray(base * (0.8f + 0.2f * joint));
    s.h = -0.001f * (1.0f - joint);
    s.rough = 0.22f + 0.2f * wear + 0.2f * smooth(0.0f, 0.05f, 0.05f - scratch);
    return s;
}

Surf wallpaper(float u, float v, float px) {
    Surf s;
    const float m = 1.1f;
    float x = u * m, y = v * m;
    const float strip = 0.55f;
    float sx = x / strip - std::floor(x / strip);
    float seamD = std::min(sx, 1.0f - sx) * strip;
    // Muster: feine senkrechte Streifen und ein zurueckhaltendes Rautenmotiv
    float stripes = 0.5f + 0.5f * std::cos(sx * 6.2831853f * 11.0f);
    float mu = x / 0.1375f, mv = y / 0.1375f;
    float fu = mu - std::floor(mu) - 0.5f, fv = mv - std::floor(mv) - 0.5f;
    float diamond = std::fabs(fu) + std::fabs(fv);
    float motif = smooth(0.42f, 0.38f, diamond) - smooth(0.3f, 0.26f, diamond) * 0.7f;
    float grain = value(u * 800.0f, v * 800.0f, 880, 880, 91);
    float stain = smooth(0.45f, 0.85f, fbm(u, v, 2, 5, 92) * 0.5f + 0.5f);
    float drip = smooth(0.6f, 0.95f, fbm(u * 6.0f, v * 0.5f, 3, 4, 93) * 0.5f + 0.5f) * smooth(0.3f, 0.9f, v);
    float base = 0.93f * (1.0f - 0.035f * stripes - 0.05f * motif) * (0.98f + 0.04f * grain);
    s.gray(base);
    // Vergilbung und Wasserflecken
    float yellow = 0.12f * stain + 0.1f * drip;
    s.r *= 1.0f - 0.25f * yellow;
    s.g *= 1.0f - 0.4f * yellow;
    s.b *= 1.0f - 0.9f * yellow;
    float seam = jointMask(seamD, 0.0004f, 0.0012f);
    s.mul(0.86f + 0.14f * seam);
    s.h = 0.00015f * grain + 0.0002f * motif + 0.0003f * (1.0f - seam) * (sx < 0.5f ? 1.0f : -0.5f);
    s.rough = 0.72f + 0.1f * stain;
    return s;
}

Surf blocks(float u, float v, float px) {
    Surf s;
    const float m = 1.6f;
    GridHit g = gridAt(u * m, v * m, 0.4f, 0.2f, true, 4);
    float tint = 1.0f + 0.07f * (r01(h2(g.cu, g.cv, 101)) - 0.5f);
    Cell p = worley(u, v, 110, 102);
    float pore = 1.0f - smooth(0.0f, 0.18f, p.d1);
    float cloud = fbm(u, v, 5, 5, 103);
    float mortar = jointMask(g.edge, 0.005f, 0.003f);
    float base = 0.86f * tint * (1.0f + 0.08f * cloud) * (1.0f - 0.3f * pore);
    s.gray(mortar > 0.5f ? base : 0.72f + 0.05f * cloud);
    s.h = -0.004f * (1.0f - mortar) - 0.0008f * pore + 0.0006f * cloud;
    s.rough = 0.88f;
    s.ao = 0.7f + 0.3f * mortar;
    return s;
}

Surf bricks(float u, float v, float px) {
    Surf s;
    const float m = 1.0f;
    GridHit g = gridAt(u * m, v * m, 0.25f, 1.0f / 12.0f, true, 4);
    std::uint32_t id = h2(g.cu, g.cv, 111);
    float tint = 0.62f + 0.3f * r01(id);
    float burn = r01(id * 17u + 3u);
    float cloud = fbm(u, v, 8, 5, 112);
    float chipEdge = 0.004f + 0.004f * (fbm(u, v, 40, 2, 113) * 0.5f + 0.5f);
    float mortar = jointMask(g.edge, chipEdge, 0.003f);
    float grime = smooth(0.4f, 0.9f, fbm(u, v, 3, 4, 114) * 0.5f + 0.5f);
    if (mortar > 0.5f) {
        s.rgb(tint * (1.0f + 0.06f * cloud), tint * (0.93f + 0.05f * cloud), tint * (0.86f + 0.04f * cloud));
        if (burn > 0.85f) s.mul(0.72f);
    } else {
        s.gray(0.62f + 0.06f * cloud);
    }
    s.mul(1.0f - 0.25f * grime);
    s.h = -0.006f * (1.0f - mortar) + 0.001f * cloud;
    s.rough = 0.9f;
    s.ao = 0.65f + 0.35f * mortar;
    return s;
}

Surf panels(float u, float v, float px) {
    Surf s;
    const float m = 1.5f;
    float x = u * m, y = v * m;
    float seamD = lineDist(x, 1.5f);
    float groove = jointMask(seamD, 0.003f, 0.002f);
    float peel = value(u * 500.0f, v * 500.0f, 750, 750, 121);
    float dent = fbm(u, v, 3, 3, 122);
    // Schraubenkoepfe entlang der Fugen
    float sy = y / 0.3f - std::floor(y / 0.3f) - 0.5f;
    float sxd = std::fabs(seamD - 0.018f);
    float screw = 1.0f - smooth(0.003f, 0.0045f, std::sqrt(sxd * sxd + (sy * 0.3f) * (sy * 0.3f)));
    s.gray(0.9f * (1.0f + 0.02f * dent));
    s.mul(0.75f + 0.25f * groove);
    s.h = -0.003f * (1.0f - groove) + 0.00008f * peel + 0.0004f * dent + 0.0012f * screw;
    s.rough = 0.42f + 0.1f * peel;
    if (screw > 0.5f) {
        s.gray(0.7f);
        s.metal = 0.9f;
        s.rough = 0.35f;
    }
    return s;
}

Surf concrete(float u, float v, float px) {
    Surf s;
    const float m = 2.4f;
    GridHit g = gridAt(u * m, v * m, 1.2f, 2.4f, false, 2);
    float cloud = fbm(u, v, 5, 6, 131);
    Cell p = worley(u, v, 140, 132);
    float pore = 1.0f - smooth(0.0f, 0.14f, p.d1);
    float joint = jointMask(g.edge, 0.002f, 0.004f);
    float tie = 1.0f - smooth(0.012f, 0.016f, std::hypot((g.lu - 0.25f) * 1.2f, (g.lv - 0.25f) * 2.4f));
    tie = std::max(tie, 1.0f - smooth(0.012f, 0.016f, std::hypot((g.lu - 0.75f) * 1.2f, (g.lv - 0.75f) * 2.4f)));
    s.gray(0.8f * (1.0f + 0.1f * cloud) * (1.0f - 0.3f * pore) * (0.8f + 0.2f * joint) * (1.0f - 0.5f * tie));
    s.h = 0.0008f * cloud - 0.001f * pore - 0.002f * (1.0f - joint) - 0.01f * tie;
    s.rough = 0.85f;
    return s;
}

// V6: Furnier mit gestreckter Maserung (Frueh-/Spaetholz), Poren, vereinzelten Aesten und
// Paneelfugen alle 0.5 m; jedes Paneel mit eigenem Ton. Ersetzt die regelmaessigen Sinuswellen.
Surf wood(float u, float v, float px) {
    Surf s;
    const int panels = 2;
    int pi = (int)std::floor(u * panels);
    float lu = u * panels - (float)pi;
    std::uint32_t pid = h2(pi, 0, 141);
    float seam = std::min(lu, 1.0f - lu) / panels;
    float sg = smooth(0.0008f, 0.0025f, seam);
    // Maserung: Linien entlang u, durch fbm verbogen
    float warp = fbm(u, v, 3, 3, 142 + (pid & 7u)) * 0.06f + fbm(u, v, 9, 2, 143) * 0.012f;
    float ringV = (v + warp) * 70.0f + r01(pid) * 10.0f;
    float ring = ringV - std::floor(ringV);
    float late = smooth(0.55f, 0.75f, ring) * (1.0f - smooth(0.85f, 0.98f, ring));
    float pores = value(u * 900.0f, v * 160.0f, 900, 160, 144);
    // Aeste: selten, dunkle Ellipsen mit Ringen
    Cell k = worley(u, v, 4, 145);
    float knot = (r01(k.id) < 0.35f) ? 1.0f - smooth(0.04f, 0.09f, k.d1) : 0.0f;
    float tone = 0.86f + 0.16f * r01(pid * 3u + 1u);
    float base = tone * (1.0f - 0.16f * late - 0.06f * pores - 0.35f * knot);
    s.rgb(base, base * 0.95f, base * 0.88f);
    s.mul(0.55f + 0.45f * sg);
    s.h = -0.0012f * (1.0f - sg) - 0.00012f * late - 0.00008f * pores - 0.0002f * knot;
    s.rough = 0.48f + 0.12f * late + 0.08f * pores;
    s.ao = 0.75f + 0.25f * sg;
    return s;
}
Surf metal(float u, float v, float px) {
    Surf s;
    float peel = value(u * 320.0f, v * 320.0f, 320, 320, 151);
    float scratch = 0.0f;
    for (int i = 0; i < 3; ++i) {
        float a = r01(h2(i, 0, 152)) * 3.14159f;
        float d = std::fabs(std::sin(a) * u - std::cos(a) * v + r01(h2(i, 1, 152)));
        float lines = std::fabs(fbm(u * std::cos(a) + v * std::sin(a), d, 64, 2, 153 + (u32)i));
        scratch = std::max(scratch, 1.0f - smooth(0.0f, 0.02f, lines));
    }
    float wear = smooth(0.5f, 0.85f, fbm(u, v, 3, 4, 154) * 0.5f + 0.5f);
    s.gray(0.9f * (1.0f - 0.05f * wear) * (1.0f + 0.12f * scratch));
    s.h = 0.00006f * peel - 0.00005f * scratch;
    s.rough = 0.38f + 0.12f * peel + 0.15f * wear - 0.12f * scratch;
    s.metal = 0.15f * scratch;
    return s;
}

Surf laminate(float u, float v, float px) {
    Surf s;
    Cell c = worley(u, v, 90, 161);
    float speck = 1.0f - smooth(0.05f, 0.22f, c.d1);
    float tone = r01(c.id);
    float base = 0.92f + (tone - 0.5f) * 0.2f * speck;
    s.gray(base);
    s.h = 0.00005f * speck;
    s.rough = 0.32f + 0.06f * (fbm(u, v, 6, 3, 162) * 0.5f + 0.5f);
    return s;
}

Surf fabric(float u, float v, float px) {
    Surf s;
    float wu = std::sin(u * 6.2831853f * 180.0f), wv = std::sin(v * 6.2831853f * 180.0f);
    float weave = (wu > 0) == (wv > 0) ? 1.0f : 0.0f;
    float fuzz = value(u * 900.0f, v * 900.0f, 900, 900, 171);
    s.gray(0.85f + 0.06f * weave + 0.06f * (fuzz - 0.5f));
    s.h = 0.0004f * weave + 0.0002f * fuzz;
    s.rough = 1.0f;
    s.ao = 0.85f + 0.15f * weave;
    return s;
}

Surf monolith(float u, float v, float px) {
    Surf s;
    float n = fbm(u, v, 3, 6, 181);
    float vein = 1.0f - smooth(0.0f, 0.02f, std::fabs(fbm(u + n * 0.2f, v, 2, 5, 182)));
    s.gray(0.9f + 0.1f * n + 0.25f * vein);
    s.rough = 0.05f + 0.04f * (n * 0.5f + 0.5f);
    s.h = 0.0f;
    return s;
}

Surf diffuser(float u, float v, float px) {
    Surf s;
    float fu = u * 20.0f - std::floor(u * 20.0f), fv = v * 20.0f - std::floor(v * 20.0f);
    float pyr = 1.0f - std::max(std::fabs(fu - 0.5f), std::fabs(fv - 0.5f)) * 2.0f;
    s.gray(0.9f + 0.1f * pyr);
    s.h = 0.0006f * pyr;
    s.rough = 0.3f;
    return s;
}

// V6: Kistenbretter aus rauem Fichtenholz - je Brett eigener Ton, teils vergraut, gestreckte
// Maserung mit Aesten, Saegeriefen quer, Naegel an den Enden, Schmutz in den Fugen.
Surf crate(float u, float v, float px) {
    Surf s;
    const float board = 1.0f / 7.0f;
    int bi = (int)std::floor(v / board);
    float lv = v / board - (float)bi;
    float gap = std::min(lv, 1.0f - lv) * board;
    float g = jointMask(gap, 0.004f, 0.003f);
    std::uint32_t id = h2(bi, 0, 191);
    float warp = fbm(u, v, 4, 3, 192 + (u32)(id % 5u)) * 0.05f;
    float ringV = (v + warp) * 95.0f + r01(id) * 7.0f;
    float ring = ringV - std::floor(ringV);
    float late = smooth(0.6f, 0.8f, ring) * (1.0f - smooth(0.88f, 0.98f, ring));
    float saw = value(u * 45.0f, v * 900.0f, 45, 900, 193 + id);  // Saegeriefen quer zum Brett
    Cell k = worley(u, v, 6, 194);
    float knot = (r01(k.id) < 0.25f) ? 1.0f - smooth(0.05f, 0.12f, k.d1) : 0.0f;
    float tone = 0.78f + 0.24f * r01(id);
    float grey = smooth(0.6f, 0.95f, r01(id * 7u + 3u)) * 0.6f;  // verwittertes Brett
    float fiber = value(u * 300.0f, v * 30.0f, 300, 30, 195);
    float base = tone * (1.0f - 0.18f * late - 0.08f * saw - 0.4f * knot) * (0.92f + 0.12f * fiber);
    s.rgb(base * (1.0f - 0.25f * grey) + 0.25f * grey * 0.6f, base * (0.95f - 0.15f * grey) + 0.2f * grey * 0.6f,
          base * (0.86f - 0.05f * grey) + 0.22f * grey * 0.6f);
    // Naegel an den Brettenden
    float nx = std::min(std::fabs(u - 0.06f), std::fabs(u - 0.94f));
    float nail = 1.0f - smooth(0.004f, 0.006f, std::hypot(nx, (lv - 0.5f) * board));
    s.mul(0.4f + 0.6f * g);
    s.h = -0.006f * (1.0f - g) - 0.0003f * late - 0.0002f * saw - 0.0004f * knot + 0.0006f * nail;
    s.rough = 0.82f + 0.1f * saw;
    if (nail > 0.5f) {
        s.rgb(0.32f, 0.26f, 0.22f);  // angerostet
        s.metal = 0.6f;
        s.rough = 0.6f;
    }
    s.ao = 0.55f + 0.45f * g;
    return s;
}
Surf trim(float u, float v, float px) {
    Surf s;
    float grain = fbm(u * 6.0f, v * 0.25f, 16, 3, 211);
    s.gray(0.93f + 0.04f * grain);
    s.h = 0.0001f * grain;
    s.rough = 0.42f;
    return s;
}

// Energieriegel: Folie (rot) mit gelbem Band und dunklem Schriftzug, gezackte Enden.
Surf itemEnergy(float u, float v, float px) {
    Surf s;
    float band = std::fabs(v - 0.5f);
    s.rgb(0.80f, 0.16f, 0.10f);
    if (band < 0.22f) s.rgb(0.98f, 0.78f, 0.22f);
    // "Schrift": kurze dunkle Balken im Band
    float lx = u * 14.0f - std::floor(u * 14.0f);
    if (band < 0.1f && u > 0.3f && u < 0.7f && lx < 0.6f) s.rgb(0.35f, 0.12f, 0.06f);
    float crimp = std::min(u, 1.0f - u);
    float ridge = crimp < 0.08f ? 0.5f + 0.5f * std::sin(v * 90.0f) : 0.0f;
    float wrinkle = fbm(u, v, 8, 3, 221);
    s.h = 0.0004f * ridge + 0.00015f * wrinkle;
    s.rough = 0.28f + 0.1f * (wrinkle * 0.5f + 0.5f);
    s.metal = 0.55f;
    return s;
}

// Mandelwasser: weisse Flasche, beiges Etikett mit Text, blauer Deckel (v = Hoehe).
Surf itemAlmond(float u, float v, float px) {
    Surf s;
    s.rgb(0.90f, 0.92f, 0.90f);
    s.rough = 0.22f;
    if (v > 0.86f) {
        s.rgb(0.25f, 0.45f, 0.85f);
        s.rough = 0.4f;
        s.h = 0.0003f * (0.5f + 0.5f * std::sin(u * 6.2831853f * 40.0f));
    } else if (v > 0.3f && v < 0.66f) {
        s.rgb(0.86f, 0.74f, 0.52f);
        s.rough = 0.55f;
        float row = (v - 0.3f) / 0.36f * 5.0f;
        float lr = row - std::floor(row);
        float word = value(u * 60.0f, std::floor(row), 60, 8, 231);
        if (lr > 0.35f && lr < 0.65f && word > 0.45f && u > 0.1f && u < 0.45f) s.rgb(0.35f, 0.22f, 0.12f);
        if (row > 3.8f && row < 4.6f && u > 0.05f && u < 0.5f) s.rgb(0.25f, 0.45f, 0.85f);
    }
    return s;
}

Surf fromVend(const VendSample& v) {
    Surf s;
    s.rgb(v.r, v.g, v.b);
    s.h = v.h;
    s.rough = v.rough;
    s.ao = v.ao;
    s.metal = v.metal;
    return s;
}

Surf sampleLayer(int layer, float u, float v, float px) {
    switch (layer) {
        case L_PLASTER: return plaster(u, v, px);
        case L_CARPET: return carpet(u, v, px);
        case L_SLABS: return slabs(u, v, px);
        case L_BIGTILES: return bigtiles(u, v, px);
        case L_GRATE: return grate(u, v, px);
        case L_CEILTILE: return ceiltile(u, v, px);
        case L_FLOORTILE: return floortile(u, v, px);
        case L_CHECKER: return checker(u, v, px);
        case L_WALLPAPER: return wallpaper(u, v, px);
        case L_BLOCKS: return blocks(u, v, px);
        case L_BRICKS: return bricks(u, v, px);
        case L_PANELS: return panels(u, v, px);
        case L_CONCRETE: return concrete(u, v, px);
        case L_WOOD: return wood(u, v, px);
        case L_METAL: return metal(u, v, px);
        case L_LAMINATE: return laminate(u, v, px);
        case L_FABRIC: return fabric(u, v, px);
        case L_MONOLITH: return monolith(u, v, px);
        case L_DIFFUSER: return diffuser(u, v, px);
        case L_CRATE: return crate(u, v, px);
        case L_VENDING: return fromVend(vendingInterior(u, v, px));
        case L_TRIM: return trim(u, v, px);
        case L_ITEM_ENERGY: return itemEnergy(u, v, px);
        case L_ITEM_ALMOND: return itemAlmond(u, v, px);
        case L_VEND_BODY: return fromVend(vendingBody(u, v, px));
    }
    return {};
}

inline float srgbToLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
inline float linearToSrgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}
inline u8 toByte(float v) { return (u8)std::clamp((int)std::lround(v * 255.0f), 0, 255); }

}  // namespace

size_t TextureSet::bcOffset(int layer, int mip, int blockBytes) const {
    auto mipBytes = [&](int m) {
        size_t b = (size_t)std::max(1, (size >> m) / 4);
        return b * b * (size_t)blockBytes;
    };
    size_t perLayer = 0;
    for (int m = 0; m < mips; ++m) perLayer += mipBytes(m);
    size_t off = perLayer * (size_t)layer;
    for (int m = 0; m < mip; ++m) off += mipBytes(m);
    return off;
}

void compressTextureSet(TextureSet& ts, JobSystem& jobs) {
    ts.albedoBC.resize(ts.bcOffset(L_COUNT, 0, 8));
    ts.normalBC.resize(ts.bcOffset(L_COUNT, 0, 16));
    jobs.parallelFor(L_COUNT, [&](int layer) {
        for (int m = 0; m < ts.mips; ++m) {
            int s = std::max(4, ts.size >> m);
            compressBC1(ts.albedo.data() + ts.mipOffset(layer, m, 4), s, s, ts.albedoBC.data() + ts.bcOffset(layer, m, 8));
            compressBC5(ts.normal.data() + ts.mipOffset(layer, m, 2), s, s, ts.normalBC.data() + ts.bcOffset(layer, m, 16));
        }
    });
    ts.compressed = true;
    std::vector<u8>().swap(ts.albedo);
    std::vector<u8>().swap(ts.normal);
}

size_t TextureSet::mipOffset(int layer, int mip, int bpp) const {
    size_t perLayer = 0;
    for (int m = 0; m < mips; ++m) {
        size_t s = (size_t)std::max(1, size >> m);
        perLayer += s * s * (size_t)bpp;
    }
    size_t off = perLayer * (size_t)layer;
    for (int m = 0; m < mip; ++m) {
        size_t s = (size_t)std::max(1, size >> m);
        off += s * s * (size_t)bpp;
    }
    return off;
}

TextureSet generateTextures(int size, JobSystem& jobs) {
    TextureSet ts;
    ts.size = size;
    ts.mips = 1;
    while ((size >> ts.mips) >= 4) ++ts.mips;
    ts.albedo.resize(ts.mipOffset(L_COUNT, 0, 4));
    ts.normal.resize(ts.mipOffset(L_COUNT, 0, 2));
    ts.orm.resize(ts.mipOffset(L_COUNT, 0, 4));

    // Stufe 1: Oberflaechen abtasten (je Ebene in Zeilenbloecken parallel). Albedo und
    // Rauheit/AO/Metall landen direkt als Bytes in Mip 0, die Hoehe bleibt fuer die Normalen.
    const int blocksPerLayer = 8;
    std::vector<std::vector<float>> height(L_COUNT, std::vector<float>((size_t)size * size));
    jobs.parallelFor(L_COUNT * blocksPerLayer, [&](int job) {
        int layer = job / blocksPerLayer, blk = job % blocksPerLayer;
        int y0 = size * blk / blocksPerLayer, y1 = size * (blk + 1) / blocksPerLayer;
        float px = 1.0f / (float)size;
        u8* a = ts.albedo.data() + ts.mipOffset(layer, 0, 4);
        u8* o = ts.orm.data() + ts.mipOffset(layer, 0, 4);
        auto& hh = height[(size_t)layer];
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < size; ++x) {
                size_t i = (size_t)y * size + x;
                Surf s = sampleLayer(layer, ((float)x + 0.5f) * px, ((float)y + 0.5f) * px, px);
                a[i * 4] = toByte(std::clamp(s.r, 0.0f, 1.0f));
                a[i * 4 + 1] = toByte(std::clamp(s.g, 0.0f, 1.0f));
                a[i * 4 + 2] = toByte(std::clamp(s.b, 0.0f, 1.0f));
                a[i * 4 + 3] = 255;
                o[i * 4] = toByte(s.ao);
                o[i * 4 + 1] = toByte(s.rough);
                o[i * 4 + 2] = toByte(s.metal);
                o[i * 4 + 3] = toByte(std::clamp(0.5f + s.h * 25.0f, 0.0f, 1.0f));
                hh[i] = s.h;
            }
    });

    // Stufe 2: Normalen aus der Hoehe, dann Mip-Ketten (je Ebene parallel)
    jobs.parallelFor(L_COUNT, [&](int layer) {
        const auto& hh = height[(size_t)layer];
        const float texel = kLayers[layer].meters / (float)size;
        std::vector<float> A((size_t)size * size * 4), N((size_t)size * size * 3), O((size_t)size * size * 4);
        const u8* a0 = ts.albedo.data() + ts.mipOffset(layer, 0, 4);
        const u8* o0 = ts.orm.data() + ts.mipOffset(layer, 0, 4);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                size_t i = (size_t)y * size + x;
                auto hAt = [&](int xx, int yy) { return hh[(size_t)((yy + size) % size) * size + (size_t)((xx + size) % size)]; };
                float dx = (hAt(x + 1, y) - hAt(x - 1, y)) / (2.0f * texel);
                float dy = (hAt(x, y + 1) - hAt(x, y - 1)) / (2.0f * texel);
                float nx = -dx, ny = -dy, nz = 1.0f;
                float l = std::sqrt(nx * nx + ny * ny + nz * nz);
                N[i * 3] = nx / l, N[i * 3 + 1] = ny / l, N[i * 3 + 2] = nz / l;
                for (int c = 0; c < 3; ++c) A[i * 4 + c] = srgbToLinear(a0[i * 4 + c] / 255.0f);
                A[i * 4 + 3] = 1.0f;
                for (int c = 0; c < 4; ++c) O[i * 4 + c] = o0[i * 4 + c] / 255.0f;
            }
        int sz = size;
        for (int m = 0; m < ts.mips; ++m) {
            u8* a = ts.albedo.data() + ts.mipOffset(layer, m, 4);
            u8* n = ts.normal.data() + ts.mipOffset(layer, m, 2);
            u8* o = ts.orm.data() + ts.mipOffset(layer, m, 4);
            for (size_t i = 0; i < (size_t)sz * sz; ++i) {
                a[i * 4] = toByte(linearToSrgb(A[i * 4]));
                a[i * 4 + 1] = toByte(linearToSrgb(A[i * 4 + 1]));
                a[i * 4 + 2] = toByte(linearToSrgb(A[i * 4 + 2]));
                a[i * 4 + 3] = 255;
                float nx = N[i * 3], ny = N[i * 3 + 1], nz = N[i * 3 + 2];
                float l = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (l > 1e-6f) nx /= l, ny /= l;
                n[i * 2] = toByte(nx * 0.5f + 0.5f);
                n[i * 2 + 1] = toByte(ny * 0.5f + 0.5f);
                for (int c = 0; c < 4; ++c) o[i * 4 + c] = toByte(O[i * 4 + c]);
            }
            if (m + 1 >= ts.mips) break;
            // 2x2-Mittelung (Albedo linear, Normalen ungenormt -> Rauheit waechst mit der Streuung)
            int ns = std::max(1, sz / 2);
            std::vector<float> A2((size_t)ns * ns * 4), N2((size_t)ns * ns * 3), O2((size_t)ns * ns * 4);
            for (int y = 0; y < ns; ++y)
                for (int x = 0; x < ns; ++x) {
                    size_t d = (size_t)y * ns + x;
                    size_t s00 = (size_t)(2 * y) * sz + 2 * x, s10 = s00 + 1, s01 = s00 + sz, s11 = s01 + 1;
                    for (int c = 0; c < 4; ++c) {
                        A2[d * 4 + c] = 0.25f * (A[s00 * 4 + c] + A[s10 * 4 + c] + A[s01 * 4 + c] + A[s11 * 4 + c]);
                        O2[d * 4 + c] = 0.25f * (O[s00 * 4 + c] + O[s10 * 4 + c] + O[s01 * 4 + c] + O[s11 * 4 + c]);
                    }
                    float len = 0.0f;
                    for (int c = 0; c < 3; ++c) {
                        N2[d * 3 + c] = 0.25f * (N[s00 * 3 + c] + N[s10 * 3 + c] + N[s01 * 3 + c] + N[s11 * 3 + c]);
                        len += N2[d * 3 + c] * N2[d * 3 + c];
                    }
                    // Toksvig: kuerzere gemittelte Normale -> mehr Rauheit (weniger Glitzern in der Ferne)
                    len = std::sqrt(len);
                    float r = O2[d * 4 + 1];
                    float a2 = r * r * r * r;
                    float ft = len / std::max(1e-4f, len + a2 * (1.0f - len));
                    float rNew = std::sqrt(std::sqrt(std::max(a2, (a2 / std::max(ft, 1e-4f)))));
                    O2[d * 4 + 1] = std::clamp(std::max(r, rNew), 0.0f, 1.0f);
                }
            A.swap(A2);
            N.swap(N2);
            O.swap(O2);
            sz = ns;
        }
    });
    return ts;
}

std::vector<u8> generateMacroNoise(int size) {
    std::vector<u8> out((size_t)size * size * 4);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float u = ((float)x + 0.5f) / (float)size, v = ((float)y + 0.5f) / (float)size;
            size_t i = ((size_t)y * size + x) * 4;
            out[i] = toByte(fbm(u, v, 4, 5, 301) * 0.5f + 0.5f);
            out[i + 1] = toByte(fbm(u, v, 8, 4, 302) * 0.5f + 0.5f);
            out[i + 2] = toByte(fbm(u, v, 16, 3, 303) * 0.5f + 0.5f);
            out[i + 3] = 255;
        }
    return out;
}

// Symbole: einfache Vektorformen mit Kantenglaettung (4x4 Ueberabtastung)
std::vector<u8> generateItemIcon(const std::string& kind, int size) {
    std::vector<u8> out((size_t)size * size * 4, 0);
    auto shade = [&](float u, float v, float& r, float& g, float& b, float& a) {
        a = 0;
        if (kind == "energy") {
            // schraeg liegender Riegel
            float c = 0.7071f, s = 0.7071f;
            float x = (u - 0.5f) * c + (v - 0.5f) * s, y = -(u - 0.5f) * s + (v - 0.5f) * c;
            if (std::fabs(x) < 0.42f && std::fabs(y) < 0.14f) {
                a = 1;
                r = 0.80f, g = 0.16f, b = 0.10f;
                if (std::fabs(y) < 0.065f) r = 0.98f, g = 0.78f, b = 0.22f;
                if (std::fabs(y) < 0.03f && std::fabs(x) < 0.18f && std::fmod(x + 1.0f, 0.06f) < 0.035f)
                    r = 0.35f, g = 0.12f, b = 0.06f;
                if (std::fabs(x) > 0.36f) r *= 0.8f, g *= 0.8f, b *= 0.8f;
                float hl = std::exp(-std::pow((y + 0.08f) * 18.0f, 2.0f)) * 0.25f;
                r += hl, g += hl, b += hl;
            }
        } else {
            // Flasche
            float x = std::fabs(u - 0.5f);
            bool cap = v > 0.08f && v < 0.2f && x < 0.08f;
            bool neck = v >= 0.2f && v < 0.3f && x < 0.08f + (v - 0.2f) * 1.2f;
            bool body = v >= 0.3f && v < 0.92f && x < 0.2f;
            if (cap) a = 1, r = 0.25f, g = 0.45f, b = 0.85f;
            else if (neck || body) {
                a = 1;
                r = 0.90f, g = 0.92f, b = 0.90f;
                if (body && v > 0.48f && v < 0.76f) {
                    r = 0.86f, g = 0.74f, b = 0.52f;
                    if (v > 0.57f && v < 0.63f && x < 0.12f) r = 0.35f, g = 0.22f, b = 0.12f;
                }
                float hl = std::exp(-std::pow((u - 0.42f) * 22.0f, 2.0f)) * 0.3f;
                r += hl, g += hl, b += hl;
            }
        }
    };
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            float R = 0, G = 0, B = 0, A = 0;
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx) {
                    float u = ((float)x + (sx + 0.5f) / 4.0f) / (float)size;
                    float v = ((float)y + (sy + 0.5f) / 4.0f) / (float)size;
                    float r = 0, g = 0, b = 0, a = 0;
                    shade(u, v, r, g, b, a);
                    R += srgbToLinear(std::min(r, 1.0f)) * a, G += srgbToLinear(std::min(g, 1.0f)) * a;
                    B += srgbToLinear(std::min(b, 1.0f)) * a, A += a;
                }
            size_t i = ((size_t)y * size + x) * 4;
            // gerade (nicht vormultiplizierte) Farbe, sRGB-kodiert; der UI-Shader multipliziert selbst
            float inv = A > 0.0f ? 1.0f / A : 0.0f;
            out[i] = toByte(linearToSrgb(R * inv));
            out[i + 1] = toByte(linearToSrgb(G * inv));
            out[i + 2] = toByte(linearToSrgb(B * inv));
            out[i + 3] = toByte(A / 16.0f);
        }
    return out;
}

}  // namespace lim::gfx
