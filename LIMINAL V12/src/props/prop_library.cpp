#include "props/prop_library.hpp"

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace lim::props {

namespace {

constexpr float kPi = 3.14159265358979f;

struct Rnd {
    u32 s;
    explicit Rnd(u32 seed) : s(seed * 2654435761u + 0x9E3779B9u) {}
    float uni() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float)(s >> 8) * (1.0f / 16777216.0f);
    }
    float range(float a, float b) { return a + (b - a) * uni(); }
};

Part part(Mesh m, Look look, float r, float g, float b, float glow = 0.0f) { return {std::move(m), {look, r, g, b, glow}}; }

Model finish(std::vector<Part> parts) {
    Model md;
    md.parts = std::move(parts);
    for (const Part& p : md.parts) {
        Aabb b = p.mesh.bounds();
        if (b.valid()) md.bounds.add(b.lo), md.bounds.add(b.hi);
    }
    return md;
}

// --- Leuchten -------------------------------------------------------------------------------------
// Leuchtband (Feuchtraum-Wannenleuchte): haengt an zwei Seilen 0,35 m unter der Befestigung
Model batten(int v) {
    const float z = -0.35f;
    std::vector<Part> p;
    p.push_back(part(bevelBox({-0.76f, -0.065f, z - 0.085f}, {0.76f, 0.065f, z}, 0.02f), Look::Plastic, 0.78f, 0.78f, 0.75f));
    // Diffusor (opal); tote Roehre: grau, an den Enden geschwaerzt
    Mesh dif = bevelBox({-0.72f, -0.05f, z - 0.1f}, {0.72f, 0.05f, z - 0.08f}, 0.012f);
    if (v == 0) p.push_back(part(dif, Look::Glow, 0.95f, 1.0f, 0.93f, 1.0f));
    else p.push_back(part(dif, Look::Plastic, 0.42f, 0.42f, 0.4f));
    if (v == 1) {
        p.push_back(part(box({-0.72f, -0.051f, z - 0.101f}, {-0.6f, 0.051f, z - 0.079f}), Look::Plastic, 0.12f, 0.11f, 0.1f));
        p.push_back(part(box({0.6f, -0.051f, z - 0.101f}, {0.72f, 0.051f, z - 0.079f}), Look::Plastic, 0.12f, 0.11f, 0.1f));
    }
    Mesh wires;
    for (float x : {-0.6f, 0.6f}) {
        wires.add(cylinder({x, 0, z}, {x, 0, 0}, 0.004f, 5, false));
        wires.add(cylinder({x, 0, -0.012f}, {x, 0, 0}, 0.018f, 8, true));  // Deckenhaken
    }
    p.push_back(part(wires, Look::Metal, 0.55f, 0.55f, 0.55f));
    return finish(std::move(p));
}

// Hallenstrahler (Lagerhalle): Pendel, Gehaeuse, Reflektor, Glas
Model highbay(int) {
    std::vector<Part> p;
    p.push_back(part(cylinder({0, 0, -0.62f}, {0, 0, 0}, 0.012f, 8, false), Look::Metal, 0.4f, 0.4f, 0.4f));
    p.push_back(part(cylinder({0, 0, -0.72f}, {0, 0, -0.6f}, 0.09f, 16, true), Look::Paint, 0.18f, 0.19f, 0.2f));
    p.push_back(part(cone({0, 0, -0.98f}, {0, 0, -0.72f}, 0.26f, 0.12f, 24, false), Look::Metal, 0.7f, 0.7f, 0.7f));
    p.push_back(part(cylinder({0, 0, -0.985f}, {0, 0, -0.975f}, 0.25f, 24, true), Look::Glow, 1.0f, 0.88f, 0.7f, 1.4f));
    return finish(std::move(p));
}

// Notausgangsschild (Wandmontage, blickt entlang +x)
Model exitSign(int v) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({0.0f, -0.19f, -0.085f}, {0.05f, 0.19f, 0.085f}, 0.008f), Look::Plastic, 0.85f, 0.86f, 0.84f));
    const int cell = v == 1 ? kCellExitArrow : kCellExit;
    Mesh face = quad({0.0505f, 0.17f, 0.07f}, {0, -0.34f, 0}, {0, 0, -0.14f}, cellUv0(cell), cellUv1(cell));
    p.push_back(part(face, Look::Sign, 0.75f, 1.0f, 0.82f, 0.9f));
    return finish(std::move(p));
}

// --- Fahrbahnmarkierung ---------------------------------------------------------------------------
Model stallLine(int v) {
    const bool yellow = v == 1;
    return finish({part(box({-2.4f, -0.06f, 0.0f}, {2.4f, 0.06f, 0.004f}), Look::RoadPaint, yellow ? 0.78f : 0.82f,
                        yellow ? 0.64f : 0.82f, yellow ? 0.22f : 0.8f)});
}

Model arrow(int) {
    std::vector<vec2> o = {{-1.1f, -0.1f}, {0.55f, -0.1f}, {0.55f, -0.32f}, {1.1f, 0.0f}, {0.55f, 0.32f}, {0.55f, 0.1f}, {-1.1f, 0.1f}};
    // Faecher vom ersten Punkt waeren falsch (nicht konvex): Schaft und Spitze getrennt
    Mesh m = prism({{-1.1f, -0.1f}, {0.56f, -0.1f}, {0.56f, 0.1f}, {-1.1f, 0.1f}}, 0.0f, 0.004f);
    m.add(prism({{0.55f, -0.32f}, {1.1f, 0.0f}, {0.55f, 0.32f}}, 0.0f, 0.004f));
    (void)o;
    return finish({part(m, Look::RoadPaint, 0.82f, 0.82f, 0.8f)});
}

// Schablonenschrift auf allen vier Seiten einer 1-m-Stuetze: Buchstabe des Decks und Nummer.
// variant = Buchstabe (0 = A) * 256 + Nummer
Model pillarLabel(int v) {
    const char letter = (char)('A' + (v >> 8) % 26);
    const int num = v & 255;
    std::string text(1, letter);
    text += ' ';
    text += std::to_string(num);
    const float ch = 0.24f, cw = 0.17f, gap = 0.03f;
    const float total = (float)text.size() * cw + (float)(text.size() - 1) * gap;
    Mesh m;
    for (int side = 0; side < 4; ++side) {
        Mesh f;
        float x = -total * 0.5f;
        for (char c : text) {
            const int cell = glyphCell(c);
            if (cell >= 0) f.add(quad({0.506f, -x, ch * 0.5f}, {0, -cw, 0}, {0, 0, -ch}, cellUv0(cell), cellUv1(cell)));
            x += cw + gap;
        }
        f.transform(mat4::rotationZ((float)side * kPi * 0.5f));
        m.add(f);
    }
    return finish({part(m, Look::Stencil, 0.92f, 0.92f, 0.9f)});
}

// --- Fahrzeug ---------------------------------------------------------------------------------------
// Querschnitt einer Karosserie: unten breit, ueber der Guertellinie eingezogen
std::vector<vec2> carRing(float w, float zb, float zt, float topScale, float r) {
    std::vector<vec2> ring;
    const int pc = 3;
    const float hw = w * 0.5f, ht = hw * topScale;
    // unten rechts -> oben rechts -> oben links -> unten links (gegen den Uhrzeigersinn in yz)
    auto arc = [&](vec2 c, float a0, float a1, float rad) {
        for (int s = 0; s <= pc; ++s) {
            float a = a0 + (a1 - a0) * (float)s / (float)pc;
            ring.push_back({c.x + rad * std::cos(a), c.y + rad * std::sin(a)});
        }
    };
    const float rb = std::min(r, (zt - zb) * 0.3f);
    arc({hw - rb, zb + rb}, -kPi * 0.5f, 0.0f, rb);
    arc({ht - rb, zt - rb}, 0.0f, kPi * 0.5f, rb);
    arc({-ht + rb, zt - rb}, kPi * 0.5f, kPi, rb);
    arc({-hw + rb, zb + rb}, kPi, kPi * 1.5f, rb);
    return ring;
}

Model car(int v) {
    // Kompaktwagen ohne Marke und Kennzeichen (F: Wagen ohne Marke), etwa 4,3 x 1,75 x 1,45 m.
    // Unterer Koerper (Lack) bis zur Guertellinie, Fensterband (Glas) darueber.
    struct St {
        float x, w, zb, zt;
    };
    const St body[] = {{2.16f, 1.52f, 0.40f, 0.70f}, {2.08f, 1.68f, 0.32f, 0.84f}, {1.6f, 1.74f, 0.30f, 0.92f},
                       {0.9f, 1.75f, 0.30f, 0.97f},  {0.5f, 1.75f, 0.30f, 0.98f},  {-0.4f, 1.75f, 0.30f, 0.98f},
                       {-1.3f, 1.75f, 0.30f, 0.98f}, {-1.7f, 1.74f, 0.31f, 0.98f}, {-2.06f, 1.70f, 0.34f, 0.92f},
                       {-2.16f, 1.56f, 0.42f, 0.80f}};
    std::vector<Section> lower;
    for (const St& s : body) lower.push_back({s.x, carRing(s.w, s.zb, s.zt, 0.985f, 0.12f)});
    Mesh lo = loft(lower, true);
    // Fensterband: von der Windschutzscheibe bis zur Heckscheibe
    const St glass[] = {{0.62f, 1.66f, 0.96f, 0.98f}, {0.2f, 1.6f, 0.96f, 1.30f}, {-0.2f, 1.56f, 0.96f, 1.42f},
                        {-0.9f, 1.55f, 0.96f, 1.43f}, {-1.35f, 1.56f, 0.96f, 1.30f}, {-1.72f, 1.62f, 0.96f, 0.99f}};
    std::vector<Section> up;
    for (const St& s : glass) up.push_back({s.x, carRing(s.w, s.zb, s.zt, 0.86f, 0.08f)});
    Mesh gl = loft(up, true);
    // Dachhaut ueber dem Glas (die Saeulen bleiben dunkel)
    const St roof[] = {{0.05f, 1.42f, 1.35f, 1.37f}, {-0.25f, 1.36f, 1.42f, 1.445f}, {-0.9f, 1.35f, 1.43f, 1.455f}, {-1.25f, 1.38f, 1.33f, 1.36f}};
    std::vector<Section> rf;
    for (const St& s : roof) rf.push_back({s.x, carRing(s.w, s.zb, s.zt, 0.98f, 0.04f)});
    Mesh roofM = loft(rf, true);
    // Lack: verblasstes Rot, Taubenblau, Silber, Flaschengruen (die Lackschicht multipliziert die Albedo noch)
    static const float col[4][3] = {{0.56f, 0.13f, 0.1f}, {0.24f, 0.36f, 0.55f}, {0.86f, 0.87f, 0.85f}, {0.16f, 0.27f, 0.2f}};
    const float* c = col[v & 3];
    std::vector<Part> p;
    lo.add(roofM);
    p.push_back(part(lo, Look::CarPaint, c[0], c[1], c[2]));
    p.push_back(part(gl, Look::Glass, 0.04f, 0.045f, 0.05f));
    // Raeder, Felgen, Radhaeuser (dunkel)
    Mesh tires, rims, wells;
    for (float x : {1.32f, -1.34f})
        for (float y : {-0.74f, 0.74f}) {
            const float o = y > 0 ? 1.0f : -1.0f;
            tires.add(cylinder({x, y - 0.1f * o, 0.31f}, {x, y + 0.1f * o, 0.31f}, 0.31f, 20, true));
            rims.add(cylinder({x, y + 0.1f * o, 0.31f}, {x, y + 0.104f * o, 0.31f}, 0.19f, 16, true));
            wells.add(cylinder({x, y - 0.06f * o, 0.33f}, {x, y + 0.115f * o, 0.33f}, 0.36f, 18, false));
        }
    p.push_back(part(tires, Look::Rubber, 0.05f, 0.05f, 0.05f));
    p.push_back(part(rims, Look::Metal, 0.45f, 0.45f, 0.46f));
    p.push_back(part(wells, Look::Plastic, 0.02f, 0.02f, 0.02f));
    // Stossfaenger, Leuchten, Spiegel
    Mesh bump;
    bump.add(bevelBox({2.08f, -0.8f, 0.3f}, {2.2f, 0.8f, 0.48f}, 0.04f));
    bump.add(bevelBox({-2.22f, -0.8f, 0.32f}, {-2.1f, 0.8f, 0.5f}, 0.04f));
    p.push_back(part(bump, Look::Plastic, 0.07f, 0.07f, 0.075f));
    Mesh head, tail;
    for (float y : {-0.6f, 0.6f}) {
        head.add(bevelBox({2.05f, y - 0.2f, 0.62f}, {2.135f, y + 0.2f, 0.74f}, 0.02f));
        tail.add(bevelBox({-2.14f, y - 0.22f, 0.64f}, {-2.06f, y + 0.22f, 0.78f}, 0.02f));
    }
    p.push_back(part(head, Look::Glass, 0.55f, 0.56f, 0.55f));
    p.push_back(part(tail, Look::Plastic, 0.25f, 0.03f, 0.03f));
    Mesh mir;
    for (float y : {-0.93f, 0.93f}) mir.add(bevelBox({0.5f, y - 0.07f, 0.98f}, {0.62f, y + 0.07f, 1.08f}, 0.02f));
    p.push_back(part(mir, Look::Plastic, 0.06f, 0.06f, 0.06f));
    return finish(std::move(p));
}

// --- Tore, Tueren, Lagerboxen -----------------------------------------------------------------------
// Rolltor aus Lamellen (Wandmontage am Ursprung, blickt entlang +x)
Model shutter(float w, float h, int v, float openFrac) {
    std::vector<Part> p;
    static const float col[4][3] = {{0.62f, 0.33f, 0.12f}, {0.25f, 0.34f, 0.45f}, {0.55f, 0.56f, 0.55f}, {0.42f, 0.18f, 0.14f}};
    const float* c = col[v & 3];
    Mesh slats;
    const float zOpen = h * openFrac;
    for (float z = zOpen; z < h - 0.01f; z += 0.1f) {
        slats.add(bevelBox({0.02f, -w * 0.5f, z}, {0.05f, w * 0.5f, std::min(h, z + 0.095f)}, 0.012f));
    }
    p.push_back(part(slats, Look::Paint, c[0], c[1], c[2]));
    Mesh frame;
    frame.add(box({0.0f, -w * 0.5f - 0.08f, 0.0f}, {0.07f, -w * 0.5f, h}));
    frame.add(box({0.0f, w * 0.5f, 0.0f}, {0.07f, w * 0.5f + 0.08f, h}));
    frame.add(bevelBox({0.0f, -w * 0.5f - 0.1f, h}, {0.32f, w * 0.5f + 0.1f, h + 0.34f}, 0.03f));  // Wickelkasten
    frame.add(box({0.04f, -w * 0.5f, zOpen}, {0.08f, w * 0.5f, zOpen + 0.05f}));                 // Endleiste
    p.push_back(part(frame, Look::Galvanized, 0.58f, 0.58f, 0.57f));
    if (openFrac > 0.02f) p.push_back(part(box({0.0f, -w * 0.5f, 0.0f}, {0.01f, w * 0.5f, zOpen}), Look::Plastic, 0.01f, 0.01f, 0.01f));
    Mesh lock = cylinder({0.05f, 0.0f, zOpen + 0.02f}, {0.09f, 0.0f, zOpen + 0.02f}, 0.025f, 10, true);
    p.push_back(part(lock, Look::Metal, 0.6f, 0.55f, 0.4f));
    return finish(std::move(p));
}

// --- Moebel und Spuren --------------------------------------------------------------------------------
Model deskCrt(int v) {
    std::vector<Part> p;
    Mesh top = bevelBox({-0.35f, -0.7f, 0.72f}, {0.35f, 0.7f, 0.75f}, 0.008f);
    p.push_back(part(top, Look::Wood, 0.46f, 0.36f, 0.26f));
    Mesh legs;
    for (float x : {-0.31f, 0.31f})
        for (float y : {-0.66f, 0.66f}) legs.add(box({x - 0.02f, y - 0.02f, 0.0f}, {x + 0.02f, y + 0.02f, 0.72f}));
    p.push_back(part(legs, Look::Metal, 0.35f, 0.35f, 0.36f));
    // Roehrenmonitor (Kunststoff vergilbt), Bildschirm dunkel
    Mesh crt = bevelBox({-0.2f, -0.19f, 0.75f}, {0.18f, 0.19f, 1.13f}, 0.03f);
    crt.add(bevelBox({0.18f, -0.17f, 0.78f}, {0.22f, 0.17f, 1.1f}, 0.015f));
    p.push_back(part(crt, Look::Plastic, 0.66f, 0.62f, 0.52f));
    p.push_back(part(quad({0.221f, 0.14f, 1.07f}, {0, -0.28f, 0}, {0, 0, -0.24f}, {0, 0}, {1, 1}), Look::Glass, 0.05f, 0.06f, 0.06f));
    p.push_back(part(bevelBox({0.25f, -0.22f, 0.75f}, {0.33f, 0.22f, 0.77f}, 0.005f), Look::Plastic, 0.6f, 0.57f, 0.5f));
    if (v != 2) {  // Stuhl (bei v 2 umgestuerzt fehlt er)
        Mesh ch;
        ch.add(bevelBox({0.45f, -0.22f, 0.44f}, {0.88f, 0.22f, 0.5f}, 0.02f));
        ch.add(bevelBox({0.84f, -0.2f, 0.5f}, {0.9f, 0.2f, 0.95f}, 0.02f));
        ch.add(cylinder({0.66f, 0.0f, 0.05f}, {0.66f, 0.0f, 0.44f}, 0.025f, 8));
        for (int k = 0; k < 5; ++k) {
            float a = (float)k * kPi * 0.4f;
            ch.add(box({0.66f + std::cos(a) * 0.02f - 0.015f, std::sin(a) * 0.02f - 0.015f, 0.02f},
                       {0.66f + std::cos(a) * 0.28f + 0.015f, std::sin(a) * 0.28f + 0.015f, 0.05f}));
        }
        p.push_back(part(ch, Look::Fabric, 0.16f, 0.17f, 0.2f));
    }
    // Papiere
    p.push_back(part(box({-0.25f, 0.3f, 0.751f}, {0.05f, 0.52f, 0.753f}), Look::Sign, 0.95f, 0.93f, 0.88f));
    return finish(std::move(p));
}

Model bed(int) {
    std::vector<Part> p;
    Mesh fr;
    for (float x : {-0.95f, 0.95f})
        for (float y : {-0.42f, 0.42f}) fr.add(cylinder({x, y, 0.0f}, {x, y, x > 0 ? 0.95f : 0.75f}, 0.02f, 8));
    fr.add(cylinder({-0.95f, -0.42f, 0.45f}, {0.95f, -0.42f, 0.45f}, 0.02f, 8));
    fr.add(cylinder({-0.95f, 0.42f, 0.45f}, {0.95f, 0.42f, 0.45f}, 0.02f, 8));
    fr.add(cylinder({0.95f, -0.42f, 0.9f}, {0.95f, 0.42f, 0.9f}, 0.02f, 8));
    p.push_back(part(fr, Look::Paint, 0.72f, 0.74f, 0.72f));
    p.push_back(part(bevelBox({-0.95f, -0.42f, 0.46f}, {0.95f, 0.42f, 0.6f}, 0.05f), Look::Fabric, 0.62f, 0.62f, 0.58f));
    p.push_back(part(bevelBox({0.6f, -0.3f, 0.6f}, {0.9f, 0.3f, 0.68f}, 0.04f), Look::Fabric, 0.72f, 0.72f, 0.68f));
    return finish(std::move(p));
}

Model painting(int v) {
    std::vector<Part> p;
    const float w = 0.92f, h = 0.66f;
    Mesh fr;
    fr.add(bevelBox({0.0f, -w * 0.5f, -h * 0.5f}, {0.04f, w * 0.5f, -h * 0.5f + 0.05f}, 0.01f));
    fr.add(bevelBox({0.0f, -w * 0.5f, h * 0.5f - 0.05f}, {0.04f, w * 0.5f, h * 0.5f}, 0.01f));
    fr.add(bevelBox({0.0f, -w * 0.5f, -h * 0.5f}, {0.04f, -w * 0.5f + 0.05f, h * 0.5f}, 0.01f));
    fr.add(bevelBox({0.0f, w * 0.5f - 0.05f, -h * 0.5f}, {0.04f, w * 0.5f, h * 0.5f}, 0.01f));
    p.push_back(part(fr, Look::Wood, 0.22f, 0.15f, 0.08f));
    const int cell = kCellPainting0 + (v & 7);
    p.push_back(part(quad({0.025f, w * 0.5f - 0.05f, h * 0.5f - 0.05f}, {0, -(w - 0.1f), 0}, {0, 0, -(h - 0.1f)}, cellUv0(cell), cellUv1(cell)),
                     Look::Sign, 1.0f, 1.0f, 1.0f));
    return finish(std::move(p));
}

Model camp(int v) {
    Rnd r(1700u + (u32)v);
    std::vector<Part> p;
    p.push_back(part(bevelBox({-1.0f, -0.36f, 0.0f}, {0.95f, 0.36f, 0.13f}, 0.05f), Look::Fabric, 0.14f, 0.2f, 0.13f));
    p.push_back(part(bevelBox({0.95f, -0.3f, 0.0f}, {1.25f, 0.3f, 0.18f}, 0.07f), Look::Fabric, 0.4f, 0.38f, 0.34f));
    Mesh bottles;
    for (int k = 0; k < 4; ++k) {
        float x = r.range(-0.9f, 0.7f), y = r.range(0.45f, 0.85f) * (k & 1 ? 1.0f : -1.0f);
        if (k == 3) bottles.add(cylinder({x, y, 0.035f}, {x + 0.24f, y + 0.05f, 0.035f}, 0.035f, 10));  // umgefallen
        else bottles.add(cylinder({x, y, 0.0f}, {x, y, 0.24f}, 0.035f, 10));
    }
    p.push_back(part(bottles, Look::Plastic, 0.55f, 0.6f, 0.58f));
    p.push_back(part(bevelBox({-1.4f, 0.4f, 0.0f}, {-1.05f, 0.7f, 0.45f}, 0.06f), Look::Fabric, 0.24f, 0.16f, 0.12f));  // Rucksack
    // Kreidestriche an der Wand waeren Text - stattdessen ein Zettel am Boden
    p.push_back(part(box({-0.3f, -0.62f, 0.001f}, {-0.09f, -0.47f, 0.003f}), Look::Sign, 0.92f, 0.9f, 0.84f));
    return finish(std::move(p));
}

Model powerBox(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({0.0f, -0.3f, -0.45f}, {0.24f, 0.3f, 0.45f}, 0.015f), Look::Paint, 0.56f, 0.57f, 0.55f));
    p.push_back(part(box({0.241f, -0.005f, -0.43f}, {0.243f, 0.005f, 0.43f}), Look::Plastic, 0.1f, 0.1f, 0.1f));
    p.push_back(part(cylinder({0.12f, 0.0f, 0.45f}, {0.12f, 0.0f, 1.6f}, 0.025f, 10), Look::Galvanized, 0.6f, 0.6f, 0.6f));
    p.push_back(part(quad({0.2435f, 0.12f, 0.35f}, {0, -0.12f, 0}, {0, 0, -0.1f}, cellUv0(kCellWarn), cellUv1(kCellWarn)), Look::Sign, 1, 1, 1));
    return finish(std::move(p));
}

Model holeGlow(int) {
    // warmes, feuerartiges Leuchten von unten (Level 19), unregelmaessiger Rand
    Rnd r(1900u);
    std::vector<vec2> o;
    for (int k = 0; k < 14; ++k) {
        float a = 2.0f * kPi * (float)k / 14.0f;
        float rr = 1.6f * r.range(0.8f, 1.1f);
        o.push_back({std::cos(a) * rr, std::sin(a) * rr});
    }
    return finish({part(prism(o, 0.0f, 0.02f), Look::Glow, 1.0f, 0.5f, 0.16f, 2.2f)});
}

Model crate(int v) {
    Rnd r(2100u + (u32)v);
    std::vector<Part> p;
    const float L = 0.8f, W = 0.55f, H = 0.5f;
    Mesh planks;
    for (int s = 0; s < 4; ++s) {
        const float z0 = (float)s * H / 4.0f;
        planks.add(bevelBox({-L * 0.5f, -W * 0.5f, z0 + 0.004f}, {L * 0.5f, -W * 0.5f + 0.02f, z0 + H / 4.0f - 0.004f}, 0.004f));
        planks.add(bevelBox({-L * 0.5f, W * 0.5f - 0.02f, z0 + 0.004f}, {L * 0.5f, W * 0.5f, z0 + H / 4.0f - 0.004f}, 0.004f));
        planks.add(bevelBox({-L * 0.5f, -W * 0.5f, z0 + 0.004f}, {-L * 0.5f + 0.02f, W * 0.5f, z0 + H / 4.0f - 0.004f}, 0.004f));
        planks.add(bevelBox({L * 0.5f - 0.02f, -W * 0.5f, z0 + 0.004f}, {L * 0.5f, W * 0.5f, z0 + H / 4.0f - 0.004f}, 0.004f));
    }
    planks.add(bevelBox({-L * 0.5f, -W * 0.5f, H}, {L * 0.5f, W * 0.5f, H + 0.02f}, 0.005f));  // Deckel
    const float tone = r.range(0.85f, 1.05f);
    p.push_back(part(planks, Look::Wood, 0.55f * tone, 0.43f * tone, 0.3f * tone));
    Mesh posts;
    for (float x : {-L * 0.5f + 0.03f, L * 0.5f - 0.03f})
        for (float y : {-W * 0.5f + 0.03f, W * 0.5f - 0.03f}) posts.add(box({x - 0.025f, y - 0.025f, 0.0f}, {x + 0.025f, y + 0.025f, H}));
    p.push_back(part(posts, Look::Wood, 0.42f, 0.33f, 0.24f));
    return finish(std::move(p));
}

Model pallet(int v) {
    Rnd r(2200u + (u32)v);
    std::vector<Part> p;
    Mesh m;
    for (int k = 0; k < 5; ++k) {
        float y = -0.4f + 0.03f + (float)k * (0.74f / 4.0f);
        m.add(bevelBox({-0.6f, y - 0.05f, 0.122f}, {0.6f, y + 0.05f, 0.144f}, 0.004f));
    }
    for (float y : {-0.37f, 0.0f, 0.37f}) m.add(bevelBox({-0.6f, y - 0.05f, 0.0f}, {0.6f, y + 0.05f, 0.122f}, 0.006f));
    p.push_back(part(m, Look::Wood, 0.6f, 0.5f, 0.36f));
    if (v & 1) {  // mit Kartons
        Mesh boxes;
        for (int k = 0; k < 3; ++k) {
            float x0 = -0.55f + (float)k * 0.38f;
            float h = r.range(0.28f, 0.5f);
            boxes.add(bevelBox({x0, -0.36f, 0.145f}, {x0 + 0.34f, 0.36f, 0.145f + h}, 0.01f));
        }
        p.push_back(part(boxes, Look::Cardboard, 0.55f, 0.42f, 0.28f));
    }
    return finish(std::move(p));
}

// Hochregal: ein Feld (2,7 m entlang x) einer Doppelreihe (Ruecken an Ruecken, 2 x 1,0 m tief, 0,2 m Spalt),
// 5,2 m hoch, Fachboeden auf 1,7 / 3,2 / 4,7 m. Rahmen blau, Traversen orange.
// variant & 3 = Beladung, variant & 4 = Endrahmen auch bei +x (letztes Feld der Reihe)
Model rack(int v) {
    Rnd r(2400u + (u32)v);
    constexpr float L = 2.7f, H = 5.2f, D = 1.0f, Gap = 0.2f;
    const float levels[] = {0.0f, 1.7f, 3.2f, 4.7f};
    std::vector<Part> p;
    Mesh frame;
    const int ends = (v & 4) ? 2 : 1;
    for (int e = 0; e < ends; ++e) {
        const float x = e == 0 ? -L * 0.5f : L * 0.5f;
        for (int row = 0; row < 2; ++row) {
            const float y0 = row == 0 ? -Gap * 0.5f - D : Gap * 0.5f, y1 = y0 + D;
            frame.add(box({x - 0.045f, y0, 0.0f}, {x + 0.045f, y0 + 0.07f, H}));
            frame.add(box({x - 0.045f, y1 - 0.07f, 0.0f}, {x + 0.045f, y1, H}));
            // Diagonalen zwischen den Pfosten (Zickzack)
            for (float z = 0.15f; z < H - 0.6f; z += 0.9f) {
                const bool up = (int)(z / 0.9f) % 2 == 0;
                frame.add(cylinder({x, up ? y0 + 0.07f : y1 - 0.07f, z}, {x, up ? y1 - 0.07f : y0 + 0.07f, z + 0.9f}, 0.016f, 6, false));
            }
            frame.add(box({x - 0.07f, y0 - 0.03f, 0.0f}, {x + 0.07f, y0 + 0.1f, 0.01f}));  // Fussplatten
            frame.add(box({x - 0.07f, y1 - 0.1f, 0.0f}, {x + 0.07f, y1 + 0.03f, 0.01f}));
        }
        frame.add(box({x - 0.03f, -Gap * 0.5f - 0.02f, H - 0.4f}, {x + 0.03f, Gap * 0.5f + 0.02f, H - 0.34f}));  // Abstandhalter
    }
    p.push_back(part(frame, Look::Paint, 0.09f, 0.2f, 0.46f));
    Mesh beams;
    for (int k = 1; k < 4; ++k) {
        const float z = levels[k];
        for (int row = 0; row < 2; ++row) {
            const float y0 = row == 0 ? -Gap * 0.5f - D : Gap * 0.5f, y1 = y0 + D;
            beams.add(box({-L * 0.5f + 0.045f, y0, z - 0.12f}, {L * 0.5f - 0.045f, y0 + 0.05f, z}));
            beams.add(box({-L * 0.5f + 0.045f, y1 - 0.05f, z - 0.12f}, {L * 0.5f - 0.045f, y1, z}));
        }
    }
    p.push_back(part(beams, Look::Paint, 0.86f, 0.36f, 0.05f));
    // Paletten (1,2 tief x 0,8 breit, ragen 0,1 ueber die Traversen), Kartons, gewickelte Ladung, Luecken
    Mesh pal, card, wrap;
    for (int k = 0; k < 4; ++k) {
        const float z = levels[k];
        for (int row = 0; row < 2; ++row) {
            const float yc = (row == 0 ? -1.0f : 1.0f) * (Gap * 0.5f + D * 0.5f);
            for (int s = 0; s < 3; ++s) {
                const float u = r.uni();
                if (u < 0.12f + 0.08f * (float)(v & 3)) continue;  // leeres Fach
                const float xc = -0.88f + (float)s * 0.88f;
                pal.add(box({xc - 0.4f, yc - 0.6f, z}, {xc + 0.4f, yc + 0.6f, z + 0.144f}));
                const float top = std::min(z + 0.144f + r.range(0.7f, 1.25f), (k < 3 ? levels[k + 1] - 0.18f : H + 0.2f));
                if (u < 0.55f) {
                    // Kartonstapel, leicht versetzt
                    float zz = z + 0.144f;
                    while (zz < top - 0.2f) {
                        const float hh = std::min(r.range(0.25f, 0.42f), top - zz);
                        const float ox = r.range(-0.03f, 0.03f), oy = r.range(-0.04f, 0.04f);
                        card.add(bevelBox({xc - 0.37f + ox, yc - 0.56f + oy, zz}, {xc + 0.37f + ox, yc + 0.56f + oy, zz + hh}, 0.008f));
                        zz += hh;
                    }
                } else {
                    wrap.add(bevelBox({xc - 0.39f, yc - 0.59f, z + 0.144f}, {xc + 0.39f, yc + 0.59f, top}, 0.05f));
                }
            }
        }
    }
    p.push_back(part(pal, Look::Wood, 0.55f, 0.46f, 0.33f));
    p.push_back(part(card, Look::Cardboard, 0.56f, 0.43f, 0.29f));
    p.push_back(part(wrap, Look::Plastic, 0.62f, 0.64f, 0.63f));
    return finish(std::move(p));
}

// Sicherheitsleuchte (V11): markiert einen Sicherungspunkt. Eigene Batterie, brennt auch im Stromausfall.
// Haengt an der Decke (Ursprung); gruene Glasflaeche nach unten und seitlich
Model safetyLight(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({-0.19f, -0.075f, -0.12f}, {0.19f, 0.075f, 0.0f}, 0.012f), Look::Plastic, 0.86f, 0.87f, 0.85f));
    Mesh glow;
    glow.add(bevelBox({-0.16f, -0.06f, -0.135f}, {0.16f, 0.06f, -0.115f}, 0.006f));
    glow.add(box({-0.16f, -0.079f, -0.1f}, {0.16f, -0.074f, -0.03f}));
    glow.add(box({-0.16f, 0.074f, -0.1f}, {0.16f, 0.079f, -0.03f}));
    p.push_back(part(glow, Look::Glow, 0.3f, 1.0f, 0.5f, 1.4f));
    return finish(std::move(p));
}

mat4 rotXY(float rx, float ry);

// Steckdose (Wandmontage, blickt entlang +x): Platte mit zwei Loechern
Model outlet(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({0.0f, -0.045f, -0.045f}, {0.012f, 0.045f, 0.045f}, 0.004f), Look::Plastic, 0.86f, 0.84f, 0.76f));
    Mesh holes;
    for (float y : {-0.013f, 0.013f}) holes.add(cylinder({0.0121f, y, 0.0f}, {0.0125f, y, 0.0f}, 0.0055f, 8, true));
    p.push_back(part(holes, Look::Plastic, 0.05f, 0.05f, 0.05f));
    return finish(std::move(p));
}

// Lueftungsgitter (Wandmontage nahe der Decke, blickt entlang +x): Rahmen und schraege Lamellen,
// dahinter Dunkel
Model ventGrille(int) {
    std::vector<Part> p;
    const float w = 0.5f, h = 0.28f;
    p.push_back(part(box({0.0f, -w * 0.5f, -h * 0.5f}, {0.004f, w * 0.5f, h * 0.5f}), Look::Plastic, 0.02f, 0.02f, 0.02f));
    Mesh frame, slats;
    frame.add(box({0.0f, -w * 0.5f - 0.02f, h * 0.5f}, {0.02f, w * 0.5f + 0.02f, h * 0.5f + 0.02f}));
    frame.add(box({0.0f, -w * 0.5f - 0.02f, -h * 0.5f - 0.02f}, {0.02f, w * 0.5f + 0.02f, -h * 0.5f}));
    frame.add(box({0.0f, -w * 0.5f - 0.02f, -h * 0.5f}, {0.02f, -w * 0.5f, h * 0.5f}));
    frame.add(box({0.0f, w * 0.5f, -h * 0.5f}, {0.02f, w * 0.5f + 0.02f, h * 0.5f}));
    for (float z = -h * 0.5f + 0.03f; z < h * 0.5f - 0.01f; z += 0.04f) {
        Mesh s = box({-0.004f, -w * 0.5f, -0.003f}, {0.004f, w * 0.5f, 0.003f});
        s.transform(mat4::translation({0.012f, 0.0f, z}) * rotXY(0.0f, 0.7f));
        slats.add(s);
    }
    p.push_back(part(frame, Look::Paint, 0.78f, 0.76f, 0.68f));
    p.push_back(part(slats, Look::Paint, 0.74f, 0.72f, 0.64f));
    return finish(std::move(p));
}

// V11: klassische Saeule (Andachtsraeume in Level 3: "wie eine Kathedrale, klassische Saeulen"). Variante =
// Hoehe in Dezimetern (Boden bis Decke). Plinthe, Wulst, kannelierter Schaft (leicht verjuengt), Kapitell mit
// Abakus. Stein, hell, mit Patina.
Model column(int v) {
    const float H = std::max(2.4f, (float)v * 0.1f), r0 = 0.27f, r1 = 0.23f;
    Mesh stone;
    stone.add(bevelBox({-0.38f, -0.38f, 0.0f}, {0.38f, 0.38f, 0.16f}, 0.02f));        // Plinthe
    stone.add(cylinder({0, 0, 0.16f}, {0, 0, 0.24f}, r0 + 0.06f, 28, true));          // Wulst
    stone.add(cylinder({0, 0, 0.24f}, {0, 0, 0.28f}, r0 + 0.02f, 28, true));
    // Schaft: Loft mit 20 Kanneluren, nach oben schlanker
    std::vector<Section> sec;
    const int flutes = 20, per = 3;
    for (int k = 0; k <= 8; ++k) {
        const float t = (float)k / 8.0f, z = 0.28f + (H - 0.28f - 0.5f) * t;
        const float r = r0 + (r1 - r0) * t;
        Section s{z, {}};
        for (int i = 0; i < flutes * per; ++i) {
            const float a = 6.2832f * (float)i / (float)(flutes * per);
            const float groove = (i % per == 1) ? 0.012f : 0.0f;
            s.ring.push_back({std::cos(a) * (r - groove), std::sin(a) * (r - groove)});
        }
        sec.push_back(s);
    }
    Mesh shaft = loft(sec, false);
    // loft laeuft entlang x: auf z drehen (x -> z, y -> x, z -> y)
    for (Vtx& p : shaft.v) {
        p.p = vec3{p.p.y, p.p.z, p.p.x};
        p.n = vec3{p.n.y, p.n.z, p.n.x};
    }
    stone.add(shaft);
    const float cz = H - 0.5f;
    stone.add(cylinder({0, 0, cz}, {0, 0, cz + 0.06f}, r1 + 0.03f, 28, true));         // Halsring
    stone.add(cone({0, 0, cz + 0.06f}, {0, 0, cz + 0.32f}, r1 + 0.02f, 0.36f, 28, true));  // Echinus
    stone.add(bevelBox({-0.4f, -0.4f, cz + 0.32f}, {0.4f, 0.4f, H}, 0.02f));            // Abakus bis zur Decke
    smoothNormals(stone, 30.0f);
    return finish({part(stone, Look::Concrete, 0.74f, 0.71f, 0.64f)});
}

// V11: Moebel, auf denen Funde liegen koennen (Ablagen statt Boden). Wandmoebel: Ursprung hinten unten in der
// Mitte, Front nach +x (an die Wandflaeche stellen). Hoehen der Ablagen stehen in tools/levels/mapkit.py
// (SURFACE), damit Funde genau darauf liegen.

// Buecherregal (Holz, 0,9 x 0,34 x 2,0 m): fuenf Boeden, Buecher in gedeckten Farben, Luecken, einige liegen
Model bookcase(int v) {
    std::vector<Part> p;
    Rnd r(4100u + (unsigned)v * 37u);
    const float D = 0.34f, W = 0.45f, H = 2.0f, t = 0.022f;
    Mesh wood;
    wood.add(bevelBox({0.0f, -W, 0.0f}, {D, -W + t, H}, 0.004f));
    wood.add(bevelBox({0.0f, W - t, 0.0f}, {D, W, H}, 0.004f));
    wood.add(bevelBox({0.0f, -W, H - t}, {D + 0.01f, W, H}, 0.004f));
    wood.add(box({0.0f, -W, 0.0f}, {0.008f, W, H}));  // Rueckwand
    wood.add(bevelBox({0.02f, -W + t, 0.0f}, {D, W - t, 0.07f}, 0.003f));  // Sockel
    static const float boards[] = {0.07f, 0.45f, 0.84f, 1.23f, 1.62f};
    for (float z : boards) wood.add(box({0.01f, -W + t, z}, {D - 0.01f, W - t, z + 0.02f}));
    p.push_back(part(wood, Look::Wood, 0.30f, 0.19f, 0.11f));
    // Buecher in vier gedeckten Farben (je ein Teil)
    static const float cols[4][3] = {{0.34f, 0.10f, 0.08f}, {0.14f, 0.20f, 0.14f}, {0.10f, 0.13f, 0.24f}, {0.52f, 0.44f, 0.30f}};
    Mesh books[4];
    for (int b = 1; b < 5; ++b) {
        const float z0 = boards[b] + 0.02f, room = (b < 4 ? boards[b + 1] : H - t) - z0;
        float y = -W + t + 0.01f;
        while (y < W - t - 0.04f) {
            if (r.range(0.0f, 1.0f) < 0.12f) {  // Luecke
                y += r.range(0.08f, 0.22f);
                continue;
            }
            const float bw = r.range(0.022f, 0.05f), bh = std::min(room - 0.02f, r.range(0.2f, 0.32f)), bd = r.range(0.17f, 0.24f);
            Mesh bk = box({0.0f, 0.0f, 0.0f}, {bd, bw, bh});
            const bool leaning = r.range(0.0f, 1.0f) < 0.06f;
            mat4 tr = mat4::translation({D - bd - 0.02f, y, z0});
            if (leaning) tr = tr * rotXY(-0.35f, 0.0f);
            bk.transform(tr);
            books[(int)r.range(0.0f, 3.99f)].add(bk);
            y += bw + 0.002f;
        }
    }
    for (int k = 0; k < 4; ++k)
        if (!books[k].v.empty()) p.push_back(part(books[k], Look::Fabric, cols[k][0], cols[k][1], cols[k][2]));
    return finish(std::move(p));
}

// niedriger Couchtisch (0,9 x 0,5 m, 0,45 m hoch), Ursprung Mitte
Model coffeeTable(int) {
    Mesh t;
    t.add(bevelBox({-0.45f, -0.25f, 0.41f}, {0.45f, 0.25f, 0.45f}, 0.008f));
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            t.add(bevelBox({sx * 0.4f - 0.022f, sy * 0.2f - 0.022f, 0.0f}, {sx * 0.4f + 0.022f, sy * 0.2f + 0.022f, 0.41f}, 0.004f));
    t.add(box({-0.4f, -0.2f, 0.12f}, {0.4f, 0.2f, 0.135f}));  // Ablage unten
    return finish({part(t, Look::Wood, 0.26f, 0.16f, 0.09f)});
}

// Stahlregal (1,2 x 0,45 x 1,9 m): vier Pfosten, vier Boeden, darauf etwas Ware (Variante 0 leer, 1 Kartons)
Model shelfUnit(int v) {
    std::vector<Part> p;
    Rnd r(4300u + (unsigned)v * 11u);
    const float D = 0.45f, W = 0.6f;
    Mesh steel, boards;
    for (float x : {0.02f, D - 0.02f})
        for (float y : {-W + 0.02f, W - 0.02f}) steel.add(box({x - 0.018f, y - 0.018f, 0.0f}, {x + 0.018f, y + 0.018f, 1.9f}));
    static const float lv[] = {0.12f, 0.6f, 1.08f, 1.56f};
    for (float z : lv) {
        boards.add(box({0.0f, -W, z}, {D, W, z + 0.025f}));
        steel.add(box({0.0f, -W, z - 0.03f}, {0.01f, W, z}));
        steel.add(box({D - 0.01f, -W, z - 0.03f}, {D, W, z}));
    }
    p.push_back(part(steel, Look::Paint, 0.28f, 0.33f, 0.36f));
    p.push_back(part(boards, Look::Galvanized, 0.55f, 0.56f, 0.55f));
    if (v >= 1) {
        Mesh boxes;
        for (int k = 1; k < 4; ++k)
            for (int n = 0; n < 2; ++n) {
                if (r.range(0.0f, 1.0f) < 0.45f) continue;
                const float bw = r.range(0.25f, 0.4f), bh = r.range(0.18f, 0.32f), y = r.range(-W + 0.2f, W - 0.2f);
                boxes.add(box({0.04f, y - bw * 0.5f, lv[k - 1] + 0.025f}, {D - 0.05f, y + bw * 0.5f, lv[k - 1] + 0.025f + bh}));
            }
        if (!boxes.v.empty()) p.push_back(part(boxes, Look::Cardboard, 0.58f, 0.45f, 0.30f));
    }
    return finish(std::move(p));
}

// Deckenlampe im Hotel: Messingrosette, kurzer Stab, Glaskugel (leuchtet). Ursprung an der Decke.
Model ceilingGlobe(int) {
    std::vector<Part> p;
    p.push_back(part(cylinder({0, 0, -0.03f}, {0, 0, 0.0f}, 0.11f, 20, true), Look::Metal, 0.62f, 0.46f, 0.22f));
    p.push_back(part(cylinder({0, 0, -0.32f}, {0, 0, -0.03f}, 0.012f, 8, false), Look::Metal, 0.62f, 0.46f, 0.22f));
    p.push_back(part(cylinder({0, 0, -0.36f}, {0, 0, -0.32f}, 0.06f, 16, true), Look::Metal, 0.62f, 0.46f, 0.22f));
    p.push_back(part(sphere({0, 0, -0.5f}, {0.15f, 0.15f, 0.15f}, 20, 12), Look::Glow, 1.0f, 0.86f, 0.62f, 1.0f));
    return finish(std::move(p));
}

// V11: Bogen in einem Durchgang mit Sturz (Bogenraeume in Level 0). Die Kachelwelt kennt nur rechteckige
// Oeffnungen: das Modell fuellt die oberen Ecken bis zum Sturz und laesst einen flachen Korbbogen frei.
// Ursprung: Mitte des Durchgangs auf der Schwelle; x quer durch die Wand (1 m), y laengs der Wand.
// Variante: lichte Breite in m (2 .. 6). Kaempfer 0,5 m unter dem Sturz (2,35 m), Laibung glatt gerundet.
Model arch(int v) {
    const float w = (float)std::clamp(v, 2, 6), top = 2.35f, rise = std::min(0.55f, w * 0.18f), spring = top - rise;
    const float half = w * 0.5f;
    Mesh fill;
    const int n = 24;
    for (int k = 0; k < n; ++k) {
        const float y0 = -half + w * (float)k / (float)n, y1 = -half + w * (float)(k + 1) / (float)n;
        auto zc = [&](float y) {
            const float t = std::clamp(y / half, -1.0f, 1.0f);
            return spring + rise * std::sqrt(std::max(0.0f, 1.0f - t * t));
        };
        // Querschnitt (y, z) gegen den Uhrzeigersinn, quer durch die Wand extrudiert
        std::vector<vec2> o = {{y0, zc(y0)}, {y1, zc(y1)}, {y1, top + 0.02f}, {y0, top + 0.02f}};
        Mesh s = prism(o, -0.5f, 0.5f);
        // prism: Umriss in xy, Dicke in z -> drehen: (y, z, x)
        for (Vtx& p : s.v) {
            p.p = vec3{p.p.z, p.p.x, p.p.y};
            p.n = vec3{p.n.z, p.n.x, p.n.y};
        }
        fill.add(s);
    }
    smoothNormals(fill, 35.0f);
    return finish({part(fill, Look::Paint, 0.80f, 0.74f, 0.57f)});
}

// V11: Deckenschacht (Lager des Buendels): Ursprung an der Deckenflaeche, nach unten. Eine dunkle Oeffnung
// 0,6 x 0,6 m im Rahmen; das Lamellengitter ist an einer Kante aufgeklappt und haengt schraeg herab.
Model ceilingVent(int v) {
    std::vector<Part> p;
    const float w = 0.3f;
    p.push_back(part(quad({-w, -w, -0.004f}, {2.0f * w, 0.0f, 0.0f}, {0.0f, 2.0f * w, 0.0f}, {0, 0}, {1, 1}, true), Look::Plastic, 0.008f, 0.008f, 0.008f));
    Mesh frame, grille;
    frame.add(box({-w - 0.03f, -w - 0.03f, -0.02f}, {w + 0.03f, -w, -0.002f}));
    frame.add(box({-w - 0.03f, w, -0.02f}, {w + 0.03f, w + 0.03f, -0.002f}));
    frame.add(box({-w - 0.03f, -w, -0.02f}, {-w, w, -0.002f}));
    frame.add(box({w, -w, -0.02f}, {w + 0.03f, w, -0.002f}));
    // Gitter: Rahmen und Lamellen in der Ebene (y 0 .. 2w), um die Kante y = -w nach unten geklappt
    Mesh panel;
    panel.add(box({-w, 0.0f, -0.012f}, {w, 0.02f, 0.0f}));
    panel.add(box({-w, 2.0f * w - 0.02f, -0.012f}, {w, 2.0f * w, 0.0f}));
    panel.add(box({-w, 0.0f, -0.012f}, {-w + 0.02f, 2.0f * w, 0.0f}));
    panel.add(box({w - 0.02f, 0.0f, -0.012f}, {w, 2.0f * w, 0.0f}));
    for (float y = 0.05f; y < 2.0f * w - 0.03f; y += 0.045f) panel.add(box({-w + 0.02f, y, -0.01f}, {w - 0.02f, y + 0.012f, -0.002f}));
    const float open = v == 0 ? 1.15f : 0.85f;  // wie weit aufgeklappt (rad)
    panel.transform(mat4::translation({0.0f, -w, -0.01f}) * rotXY(-open, 0.0f));
    grille.add(panel);
    p.push_back(part(frame, Look::Paint, 0.62f, 0.62f, 0.6f));
    p.push_back(part(grille, Look::Paint, 0.55f, 0.55f, 0.53f));
    return finish(std::move(p));
}

// V11: Wasseroberflaeche: 1 x 1 m (mit scale auf den Raum gezogen), beidseitig (auch von unten zu sehen)
Model waterSurface(int) {
    return finish({part(quad({-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0, 0}, {1, 1}, true), Look::Water, 0.1f, 0.22f, 0.24f)});
}

// Drehung um x (rx), danach um y (ry)
mat4 rotXY(float rx, float ry) {
    const float cx = std::cos(rx), sx = std::sin(rx), cy = std::cos(ry), sy = std::sin(ry);
    return mat4::fromRows({cy, sy * sx, sy * cx, 0}, {0, cx, -sx, 0}, {-sy, cy * sx, cy * cx, 0}, {0, 0, 0, 1});
}

// Absperrschranke (Baustelle): zwei Pfosten mit Fussplatten, rot-weisse Latte; v 1 = umgekippt
Model barrier(int v) {
    Mesh feet, red, white;
    for (float x : {-0.75f, 0.75f}) {
        feet.add(box({x - 0.02f, -0.02f, 0.0f}, {x + 0.02f, 0.02f, 1.02f}));
        feet.add(bevelBox({x - 0.035f, -0.28f, 0.0f}, {x + 0.035f, 0.28f, 0.035f}, 0.008f));
    }
    for (int k = 0; k < 6; ++k) {
        const float x0 = -0.8f + (float)k * (1.6f / 6.0f);
        (k & 1 ? white : red).add(box({x0, 0.021f, 0.86f}, {x0 + 1.6f / 6.0f, 0.042f, 1.0f}));
    }
    std::vector<Part> p = {part(feet, Look::Galvanized, 0.55f, 0.55f, 0.54f), part(red, Look::Paint, 0.72f, 0.07f, 0.05f),
                           part(white, Look::Paint, 0.86f, 0.86f, 0.83f)};
    if (v == 1)
        for (Part& q : p) q.mesh.transform(mat4::translation({0, 0, 0.28f}) * rotXY(-1.5f, 0.0f));
    return finish(std::move(p));
}

// Betonbrocken und Bewehrungseisen (Bruchkante, Boden des Lochs)
Model rubble(int v) {
    Rnd r(2500u + (u32)v);
    Mesh chunks, rebar;
    const int n = 5 + (int)(r.uni() * 4.0f);
    for (int k = 0; k < n; ++k) {
        const float s = r.range(0.08f, 0.32f);
        Mesh c = bevelBox({-s, -s * r.range(0.6f, 1.0f), 0.0f}, {s, s * r.range(0.6f, 1.0f), s * r.range(0.5f, 1.1f)}, s * 0.05f);
        c.transform(mat4::translation({r.range(-0.7f, 0.7f), r.range(-0.6f, 0.6f), -s * 0.15f}) * mat4::rotationZ(r.range(0, 6.28f)) *
                    rotXY(r.range(-0.4f, 0.4f), r.range(-0.4f, 0.4f)));
        chunks.add(c);
    }
    for (int k = 0; k < 2; ++k) {
        const vec3 a{r.range(-0.5f, 0.5f), r.range(-0.4f, 0.4f), 0.02f};
        const vec3 b = a + vec3{r.range(0.3f, 0.7f), r.range(-0.3f, 0.3f), r.range(0.05f, 0.35f)};
        const vec3 c = b + vec3{r.range(0.1f, 0.3f), r.range(-0.2f, 0.2f), r.range(-0.1f, 0.15f)};
        rebar.add(cylinder(a, b, 0.008f, 6, false));
        rebar.add(cylinder(b, c, 0.008f, 6, false));
    }
    return finish({part(chunks, Look::Concrete, 0.36f, 0.355f, 0.34f), part(rebar, Look::Metal, 0.32f, 0.2f, 0.13f)});
}

// Bruchkante einer Deckenplatte (1 m entlang y): ragt 0,1 - 0,4 m ueber das Loch (+x), 0,28 m dick,
// Bewehrung steht heraus und haengt nach unten. Ursprung auf der Kante, Oberseite buendig mit dem Boden.
Model slabEdge(int v) {
    Rnd r(2600u + (u32)v);
    std::vector<vec2> o = {{-0.15f, -0.5f}};
    const int n = 6;
    for (int k = 0; k <= n; ++k) o.push_back({r.range(0.08f, 0.38f), -0.5f + (float)k / (float)n});
    o.push_back({-0.15f, 0.5f});
    // Faecher aus dem Mittelpunkt: die gezackte Seite ist nicht konvex, ein Streifen je Zacke schon
    Mesh slab;
    for (size_t k = 1; k + 2 < o.size(); ++k)
        slab.add(prism({{-0.15f, o[k].y}, o[k], o[k + 1], {-0.15f, o[k + 1].y}}, -0.28f, -0.003f));
    Mesh rebar;
    for (int k = 0; k < 3; ++k) {
        const float y = r.range(-0.4f, 0.4f), zz = r.range(-0.22f, -0.06f);
        const vec3 a{0.0f, y, zz}, b{r.range(0.35f, 0.6f), y + r.range(-0.08f, 0.08f), zz + r.range(-0.05f, 0.03f)};
        const vec3 c = b + vec3{r.range(0.05f, 0.2f), r.range(-0.1f, 0.1f), -r.range(0.2f, 0.6f)};
        rebar.add(cylinder(a, b, 0.008f, 6, false));
        rebar.add(cylinder(b, c, 0.008f, 6, false));
    }
    return finish({part(slab, Look::Concrete, 0.46f, 0.45f, 0.43f), part(rebar, Look::Metal, 0.3f, 0.19f, 0.12f)});
}

// --- Level 2: Rohrgaenge ---------------------------------------------------------------------------------
// Handrad eines Ventils (Achse entlang +x, Radebene yz), Speichen, Flansch
Model valveWheel(int v) {
    std::vector<Part> p;
    const float R = v == 1 ? 0.22f : 0.15f;
    Mesh rim, spokes;
    const int n = 20;
    for (int k = 0; k < n; ++k) {
        const float a0 = 2.0f * kPi * (float)k / (float)n, a1 = 2.0f * kPi * (float)(k + 1) / (float)n;
        rim.add(cylinder({0.12f, std::cos(a0) * R, std::sin(a0) * R}, {0.12f, std::cos(a1) * R, std::sin(a1) * R}, 0.016f, 6, false));
    }
    for (int k = 0; k < 4; ++k) {
        const float a = kPi * 0.5f * (float)k + 0.4f;
        spokes.add(cylinder({0.12f, 0, 0}, {0.12f, std::cos(a) * R, std::sin(a) * R}, 0.01f, 6, false));
    }
    spokes.add(cylinder({0.0f, 0, 0}, {0.13f, 0, 0}, 0.025f, 10, true));
    p.push_back(part(rim, Look::Paint, 0.62f, 0.1f, 0.07f));
    p.push_back(part(spokes, Look::Metal, 0.36f, 0.34f, 0.32f));
    return finish(std::move(p));
}

// Manometer: Dose mit Glas, Ziffernblatt hell, Zeiger
Model gauge(int v) {
    std::vector<Part> p;
    p.push_back(part(cylinder({0.0f, 0, 0}, {0.06f, 0, 0}, 0.075f, 18, true), Look::Metal, 0.5f, 0.48f, 0.42f));
    p.push_back(part(cylinder({0.06f, 0, 0}, {0.062f, 0, 0}, 0.066f, 18, true), Look::Plastic, 0.9f, 0.88f, 0.82f));
    const float a = 0.6f + 0.5f * (float)(v & 3);
    p.push_back(part(box({0.062f, -0.002f, 0.0f}, {0.066f, 0.002f, 0.055f}), Look::Plastic, 0.1f, 0.05f, 0.05f));
    p.back().mesh.transform(mat4::translation({0, 0, 0}) * rotXY(a, 0.0f));
    p.push_back(part(cylinder({-0.12f, 0, -0.02f}, {0.0f, 0, -0.02f}, 0.012f, 8, false), Look::Metal, 0.4f, 0.38f, 0.34f));
    return finish(std::move(p));
}

// Stahltuer in der Wand (verschlossen), blickt entlang +x: Zarge, Blatt mit Nieten und Sicke, Drucker
Model steelDoor(int v) {
    std::vector<Part> p;
    const float w = 0.95f, h = 2.05f;
    Mesh frame;
    frame.add(box({0.0f, -w * 0.5f - 0.06f, 0.0f}, {0.05f, -w * 0.5f, h + 0.06f}));
    frame.add(box({0.0f, w * 0.5f, 0.0f}, {0.05f, w * 0.5f + 0.06f, h + 0.06f}));
    frame.add(box({0.0f, -w * 0.5f, h}, {0.05f, w * 0.5f, h + 0.06f}));
    p.push_back(part(frame, Look::Paint, 0.28f, 0.29f, 0.28f));
    static const float col[3][3] = {{0.36f, 0.4f, 0.38f}, {0.42f, 0.24f, 0.16f}, {0.3f, 0.33f, 0.38f}};
    const float* c = col[v % 3];
    Mesh leaf = bevelBox({0.005f, -w * 0.5f, 0.01f}, {0.04f, w * 0.5f, h}, 0.006f);
    leaf.add(bevelBox({0.04f, -w * 0.5f + 0.1f, 0.25f}, {0.046f, w * 0.5f - 0.1f, 0.95f}, 0.004f));
    leaf.add(bevelBox({0.04f, -w * 0.5f + 0.1f, 1.15f}, {0.046f, w * 0.5f - 0.1f, h - 0.15f}, 0.004f));
    p.push_back(part(leaf, Look::Paint, c[0], c[1], c[2]));
    Mesh hw;
    hw.add(cylinder({0.04f, w * 0.5f - 0.1f, 1.05f}, {0.09f, w * 0.5f - 0.1f, 1.05f}, 0.012f, 8, true));
    hw.add(cylinder({0.09f, w * 0.5f - 0.1f, 1.05f}, {0.09f, w * 0.5f - 0.24f, 1.05f}, 0.012f, 8, true));
    for (float z : {0.15f, 1.0f, h - 0.1f})
        for (float y : {-w * 0.5f + 0.04f, w * 0.5f - 0.04f}) hw.add(cylinder({0.04f, y, z}, {0.047f, y, z}, 0.01f, 6, true));
    p.push_back(part(hw, Look::Metal, 0.45f, 0.43f, 0.4f));
    return finish(std::move(p));
}

// Stahlfass: Mantel mit zwei Rollreifen, Deckel mit Spund; Lack (blau, rostrot, schwarz)
Model barrel(int v) {
    std::vector<Part> p;
    const float R = 0.29f, H = 0.88f;
    static const float col[3][3] = {{0.12f, 0.24f, 0.46f}, {0.42f, 0.16f, 0.08f}, {0.08f, 0.08f, 0.08f}};
    const float* c = col[v % 3];
    Mesh body = cylinder({0, 0, 0.0f}, {0, 0, H}, R, 24, true);
    for (float z : {0.3f, 0.58f}) body.add(cylinder({0, 0, z - 0.012f}, {0, 0, z + 0.012f}, R + 0.012f, 24, false));
    body.add(cylinder({0, 0, H - 0.01f}, {0, 0, H + 0.012f}, R + 0.006f, 24, false));
    p.push_back(part(body, Look::Paint, c[0], c[1], c[2]));
    p.push_back(part(cylinder({0.12f, 0.05f, H}, {0.12f, 0.05f, H + 0.015f}, 0.03f, 10, true), Look::Metal, 0.45f, 0.44f, 0.42f));
    return finish(std::move(p));
}

// Rollwagen: Plattform, vier Lenkrollen, Schiebebuegel
Model cart(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({-0.5f, -0.3f, 0.16f}, {0.5f, 0.3f, 0.2f}, 0.01f), Look::Galvanized, 0.55f, 0.55f, 0.53f));
    Mesh wheels;
    for (float x : {-0.42f, 0.42f})
        for (float y : {-0.23f, 0.23f}) wheels.add(cylinder({x, y - 0.02f, 0.06f}, {x, y + 0.02f, 0.06f}, 0.06f, 12, true));
    p.push_back(part(wheels, Look::Rubber, 0.06f, 0.06f, 0.06f));
    p.push_back(part(pipePath({{-0.48f, -0.26f, 0.2f}, {-0.6f, -0.26f, 0.95f}, {-0.6f, 0.26f, 0.95f}, {-0.48f, 0.26f, 0.2f}}, 0.015f, 8, 0.06f),
                     Look::Metal, 0.42f, 0.42f, 0.42f));
    return finish(std::move(p));
}

// Pumpe: Grundrahmen, Motor (Rippen), Pumpengehaeuse, Flansche
Model pump(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({-0.7f, -0.35f, 0.0f}, {0.7f, 0.35f, 0.12f}, 0.01f), Look::Paint, 0.2f, 0.22f, 0.24f));
    Mesh motor = cylinder({-0.6f, 0, 0.42f}, {0.1f, 0, 0.42f}, 0.26f, 20, true);
    for (float x = -0.55f; x < 0.05f; x += 0.06f) motor.add(cylinder({x, 0, 0.42f}, {x + 0.015f, 0, 0.42f}, 0.28f, 20, false));
    p.push_back(part(motor, Look::Paint, 0.16f, 0.32f, 0.44f));
    Mesh housing = sphere({0.38f, 0, 0.42f}, {0.26f, 0.24f, 0.26f}, 16, 10);
    housing.add(cylinder({0.38f, 0, 0.66f}, {0.38f, 0, 1.0f}, 0.09f, 14, true));
    housing.add(cylinder({0.62f, 0, 0.42f}, {0.95f, 0, 0.42f}, 0.09f, 14, true));
    p.push_back(part(housing, Look::Paint, 0.5f, 0.12f, 0.08f));
    Mesh fl;
    fl.add(cylinder({0.38f, 0, 0.98f}, {0.38f, 0, 1.02f}, 0.14f, 14, true));
    fl.add(cylinder({0.93f, 0, 0.42f}, {0.97f, 0, 0.42f}, 0.14f, 14, true));
    p.push_back(part(fl, Look::Metal, 0.42f, 0.4f, 0.37f));
    return finish(std::move(p));
}

// Rohrbuendel: 1 m langes Stueck (entlang x) unter der Decke an der Wand, 2-4 Rohre, Konsolen.
// variant: Zahl der Rohre (2 + v % 3), v & 4: mit Isolierung (hell)
Model pipeBundle(int v) {
    std::vector<Part> p;
    const int n = 2 + v % 3;
    Mesh pipes, ins, brackets;
    for (int k = 0; k < n; ++k) {
        const float r = k == 0 ? 0.09f : (k == 1 ? 0.06f : 0.04f);
        const float y = 0.12f + (float)k * 0.2f, z = -0.15f - (float)(k % 2) * 0.12f;
        (v & 4 && k == 0 ? ins : pipes).add(cylinder({-0.5f, y, z}, {0.5f, y, z}, r + (v & 4 && k == 0 ? 0.03f : 0.0f), 14, false));
    }
    brackets.add(box({-0.04f, 0.0f, -0.36f}, {0.04f, 0.05f, 0.0f}));
    brackets.add(box({-0.04f, 0.0f, -0.36f}, {0.04f, 0.2f * (float)n + 0.1f, -0.33f}));
    static const float col[3][3] = {{0.52f, 0.48f, 0.42f}, {0.3f, 0.42f, 0.32f}, {0.5f, 0.24f, 0.16f}};  // grau, gruen, rostrot
    const float* c = col[(v >> 3) % 3 == 0 ? (v % 3) : 0];
    p.push_back(part(pipes, Look::Paint, c[0], c[1], c[2]));
    if (!ins.v.empty()) p.push_back(part(ins, Look::Fabric, 0.8f, 0.78f, 0.72f));
    p.push_back(part(brackets, Look::Galvanized, 0.5f, 0.5f, 0.48f));
    return finish(std::move(p));
}

// --- Level 19: Dachboden --------------------------------------------------------------------------------------
// Dachsparren/Balken: 1 m entlang x (mit scale auf Raumbreite gezogen), Querschnitt 0,12 x 0,18
Model rafter(int) {
    return finish({part(bevelBox({-0.5f, -0.06f, -0.18f}, {0.5f, 0.06f, 0.0f}, 0.01f), Look::Wood, 0.42f, 0.32f, 0.22f)});
}

// Daemmwolle (rosa) zwischen den Sparren: 1 x 0,5 m, buckelig
Model insulation(int v) {
    Rnd r(3100u + (u32)v);
    Mesh m;
    for (int k = 0; k < 5; ++k) {
        const float x0 = -0.5f + (float)k * 0.2f;
        m.add(bevelBox({x0, -0.24f + r.range(-0.02f, 0.0f), -0.13f - r.range(0.0f, 0.04f)}, {x0 + 0.2f, 0.24f + r.range(0.0f, 0.02f), 0.0f}, 0.05f));
    }
    return finish({part(m, Look::Fabric, 0.86f, 0.55f, 0.58f)});
}

// Glut zwischen den Dielen: ein 1 m langer, leuchtender Spalt (mit scale verlaengert)
Model floorGlow(int v) {
    const float w = v == 1 ? 0.03f : 0.015f;
    return finish({part(box({-0.5f, -w * 0.5f, 0.0f}, {0.5f, w * 0.5f, 0.003f}), Look::Glow, 1.0f, 0.45f, 0.12f, 1.8f)});
}

// Alter Kleiderschrank (blickt entlang +x)
Model wardrobe(int v) {
    std::vector<Part> p;
    const float tone = v == 1 ? 0.8f : 1.0f;
    Mesh body = bevelBox({-0.3f, -0.55f, 0.0f}, {0.3f, 0.55f, 1.95f}, 0.015f);
    body.add(bevelBox({-0.32f, -0.58f, 1.95f}, {0.33f, 0.58f, 2.03f}, 0.015f));
    p.push_back(part(body, Look::Wood, 0.36f * tone, 0.24f * tone, 0.15f * tone));
    Mesh doors;
    doors.add(bevelBox({0.3f, -0.53f, 0.08f}, {0.32f, -0.01f, 1.9f}, 0.006f));
    doors.add(bevelBox({0.3f, 0.01f, 0.08f}, {0.32f, 0.53f, 1.9f}, 0.006f));
    p.push_back(part(doors, Look::Wood, 0.4f * tone, 0.27f * tone, 0.17f * tone));
    p.push_back(part(cylinder({0.32f, -0.05f, 1.0f}, {0.35f, -0.05f, 1.0f}, 0.012f, 8, true), Look::Metal, 0.6f, 0.5f, 0.3f));
    return finish(std::move(p));
}

// Holzstuhl
Model chair(int v) {
    std::vector<Part> p;
    Mesh m;
    for (float x : {-0.2f, 0.2f})
        for (float y : {-0.2f, 0.2f}) m.add(box({x - 0.02f, y - 0.02f, 0.0f}, {x + 0.02f, y + 0.02f, x < 0.0f ? 0.95f : 0.45f}));
    m.add(bevelBox({-0.23f, -0.23f, 0.43f}, {0.23f, 0.23f, 0.47f}, 0.01f));
    m.add(box({-0.22f, -0.2f, 0.7f}, {-0.18f, 0.2f, 0.9f}));
    if (v == 1) m.transform(mat4::translation({0.0f, 0.0f, 0.23f}) * rotXY(1.57f, 0.0f));  // umgekippt
    p.push_back(part(m, Look::Wood, 0.44f, 0.32f, 0.2f));
    return finish(std::move(p));
}

// Truhe mit Beschlaegen
Model trunk(int) {
    std::vector<Part> p;
    Mesh b = bevelBox({-0.45f, -0.27f, 0.0f}, {0.45f, 0.27f, 0.42f}, 0.02f);
    b.add(cylinder({-0.45f, 0.0f, 0.42f}, {0.45f, 0.0f, 0.42f}, 0.27f, 16, true));
    p.push_back(part(b, Look::Wood, 0.34f, 0.22f, 0.13f));
    Mesh bands;
    for (float x : {-0.3f, 0.3f}) bands.add(cylinder({x - 0.015f, 0, 0.42f}, {x + 0.015f, 0, 0.42f}, 0.28f, 16, false));
    p.push_back(part(bands, Look::Metal, 0.36f, 0.32f, 0.26f));
    return finish(std::move(p));
}

// Beschaedigte Tuer (Ausgang nach Level 5): Blatt schief in der Zarge, unten gesplittert, im Spalt Dunkel
Model damagedDoor(int) {
    std::vector<Part> p;
    const float w = 0.9f, h = 2.0f;
    Mesh frame;
    frame.add(box({0.0f, -w * 0.5f - 0.07f, 0.0f}, {0.06f, -w * 0.5f, h + 0.07f}));
    frame.add(box({0.0f, w * 0.5f, 0.0f}, {0.06f, w * 0.5f + 0.07f, h + 0.07f}));
    frame.add(box({0.0f, -w * 0.5f, h}, {0.06f, w * 0.5f, h + 0.07f}));
    p.push_back(part(frame, Look::Wood, 0.3f, 0.21f, 0.13f));
    p.push_back(part(box({0.0f, -w * 0.5f, 0.0f}, {0.01f, w * 0.5f, h}), Look::Plastic, 0.0f, 0.0f, 0.0f));
    Mesh leaf = bevelBox({0.02f, -w * 0.5f + 0.02f, 0.25f}, {0.06f, w * 0.5f - 0.03f, h - 0.02f}, 0.008f);
    leaf.transform(mat4::translation({0.0f, -w * 0.5f, 0.0f}) * mat4::rotationZ(0.35f) * mat4::translation({0.0f, w * 0.5f, 0.0f}));
    p.push_back(part(leaf, Look::Wood, 0.42f, 0.3f, 0.18f));
    Rnd r(3300u);
    Mesh splinters;
    for (int k = 0; k < 6; ++k) {
        const float y = r.range(-0.35f, 0.35f);
        splinters.add(cylinder({0.04f, y, 0.0f}, {0.04f + r.range(-0.05f, 0.08f), y + r.range(-0.05f, 0.05f), r.range(0.1f, 0.3f)}, 0.012f, 4, false));
    }
    p.push_back(part(splinters, Look::Wood, 0.48f, 0.36f, 0.22f));
    return finish(std::move(p));
}

// --- Level 3: Umspannwerk ---------------------------------------------------------------------------------
// Transformator: Kessel mit Kuehlrippen, drei Durchfuehrungen mit Isolatoren oben, Sockel
Model transformer(int v) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({-1.3f, -0.9f, 0.0f}, {1.3f, 0.9f, 0.25f}, 0.02f), Look::Concrete, 0.5f, 0.5f, 0.48f));
    Mesh tank = bevelBox({-1.0f, -0.6f, 0.25f}, {1.0f, 0.6f, 2.3f}, 0.04f);
    for (float x = -0.9f; x <= 0.91f; x += 0.15f) {
        tank.add(box({x - 0.015f, -0.85f, 0.4f}, {x + 0.015f, -0.6f, 2.1f}));
        tank.add(box({x - 0.015f, 0.6f, 0.4f}, {x + 0.015f, 0.85f, 2.1f}));
    }
    static const float col[2][3] = {{0.32f, 0.36f, 0.3f}, {0.42f, 0.42f, 0.4f}};
    const float* c = col[v & 1];
    p.push_back(part(tank, Look::Paint, c[0], c[1], c[2]));
    Mesh ins;
    for (int k = 0; k < 3; ++k) {
        const float x = -0.6f + 0.6f * (float)k;
        for (int d = 0; d < 6; ++d) ins.add(cylinder({x, 0, 2.3f + 0.12f * (float)d}, {x, 0, 2.36f + 0.12f * (float)d}, 0.09f, 12, true));
        ins.add(cylinder({x, 0, 2.3f}, {x, 0, 3.1f}, 0.05f, 10, true));
    }
    p.push_back(part(ins, Look::Ceramic, 0.62f, 0.4f, 0.26f));
    p.push_back(part(quad({1.001f, 0.25f, 1.6f}, {0, -0.5f, 0}, {0, 0, -0.5f}, cellUv0(kCellWarn), cellUv1(kCellWarn)), Look::Sign, 1, 1, 1));
    return finish(std::move(p));
}

// Gitterzaun: 1 m breites Feld (entlang x), 3 m hoch, Pfosten, Maschendraht (Gitterstaebe)
Model fencePanel(int) {
    std::vector<Part> p;
    Mesh posts, mesh;
    posts.add(box({-0.5f, -0.03f, 0.0f}, {-0.44f, 0.03f, 3.0f}));
    posts.add(box({-0.5f, -0.02f, 2.95f}, {0.5f, 0.02f, 3.0f}));
    posts.add(box({-0.5f, -0.02f, 0.0f}, {0.5f, 0.02f, 0.05f}));
    for (float x = -0.44f; x < 0.5f; x += 0.08f) mesh.add(box({x - 0.004f, -0.004f, 0.05f}, {x + 0.004f, 0.004f, 2.95f}));
    for (float z = 0.12f; z < 2.95f; z += 0.08f) mesh.add(box({-0.44f, -0.004f, z - 0.004f}, {0.5f, 0.004f, z + 0.004f}));
    p.push_back(part(posts, Look::Galvanized, 0.5f, 0.5f, 0.48f));
    p.push_back(part(mesh, Look::Galvanized, 0.46f, 0.46f, 0.45f));
    return finish(std::move(p));
}

// Schaltschrank (blickt entlang +x): 2 m hoch, zwei Tueren, Lueftungsschlitze, Warnschild
Model switchCabinet(int v) {
    std::vector<Part> p;
    static const float col[3][3] = {{0.56f, 0.57f, 0.55f}, {0.62f, 0.5f, 0.2f}, {0.3f, 0.34f, 0.4f}};
    const float* c = col[v % 3];
    Mesh body = bevelBox({0.0f, -0.6f, 0.0f}, {0.5f, 0.6f, 2.0f}, 0.01f);
    body.add(bevelBox({0.5f, -0.58f, 0.05f}, {0.52f, -0.01f, 1.95f}, 0.005f));
    body.add(bevelBox({0.5f, 0.01f, 0.05f}, {0.52f, 0.58f, 1.95f}, 0.005f));
    p.push_back(part(body, Look::Paint, c[0], c[1], c[2]));
    Mesh slots;
    for (float z = 1.6f; z < 1.85f; z += 0.04f)
        for (float y : {-0.4f, 0.2f}) slots.add(box({0.52f, y, z}, {0.523f, y + 0.2f, z + 0.015f}));
    slots.add(cylinder({0.52f, -0.08f, 1.0f}, {0.56f, -0.08f, 1.0f}, 0.02f, 8, true));
    slots.add(cylinder({0.52f, 0.08f, 1.0f}, {0.56f, 0.08f, 1.0f}, 0.02f, 8, true));
    p.push_back(part(slots, Look::Plastic, 0.05f, 0.05f, 0.05f));
    p.push_back(part(quad({0.524f, -0.2f, 1.4f}, {0, -0.16f, 0}, {0, 0, -0.14f}, cellUv0(kCellWarn), cellUv1(kCellWarn)), Look::Sign, 1, 1, 1));
    return finish(std::move(p));
}

// Kabelpritsche unter der Decke: 1 m (entlang x), Blechwanne mit Kabelbuendel, Haenger
Model cableTray(int v) {
    std::vector<Part> p;
    Mesh tray;
    tray.add(box({-0.5f, -0.2f, -0.32f}, {0.5f, 0.2f, -0.3f}));
    tray.add(box({-0.5f, -0.2f, -0.32f}, {0.5f, -0.19f, -0.24f}));
    tray.add(box({-0.5f, 0.19f, -0.32f}, {0.5f, 0.2f, -0.24f}));
    tray.add(box({-0.01f, -0.21f, -0.32f}, {0.01f, -0.2f, 0.0f}));
    tray.add(box({-0.01f, 0.2f, -0.32f}, {0.01f, 0.21f, 0.0f}));
    p.push_back(part(tray, Look::Galvanized, 0.52f, 0.52f, 0.5f));
    Mesh cables;
    Rnd r(3400u + (u32)v);
    for (int k = 0; k < 7; ++k) {
        const float y = -0.16f + 0.05f * (float)k, rr = r.range(0.012f, 0.025f);
        cables.add(cylinder({-0.5f, y, -0.3f + rr}, {0.5f, y, -0.3f + rr}, rr, 6, false));
    }
    p.push_back(part(cables, Look::Rubber, 0.05f, 0.05f, 0.05f));
    return finish(std::move(p));
}

// Statue (Ruheraum): eine verhuellte, stehende Gestalt aus Stein auf einem Sockel - ohne Gesicht
Model statue(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({-0.4f, -0.4f, 0.0f}, {0.4f, 0.4f, 0.6f}, 0.03f), Look::Concrete, 0.62f, 0.6f, 0.56f));
    std::vector<Section> secs;
    const float zs[] = {0.6f, 0.9f, 1.4f, 1.9f, 2.1f, 2.25f, 2.42f, 2.55f};
    const float rs[] = {0.32f, 0.3f, 0.24f, 0.26f, 0.2f, 0.12f, 0.14f, 0.02f};
    Mesh body;
    for (int k = 0; k + 1 < 8; ++k) body.add(cone({0, 0, zs[k]}, {0, 0, zs[k + 1]}, rs[k], rs[k + 1], 18, k == 0));
    p.push_back(part(body, Look::Concrete, 0.74f, 0.72f, 0.68f));
    return finish(std::move(p));
}

// --- Level 4: stilles Stockwerk -----------------------------------------------------------------------------
// Wasserspender: Unterschrank, Flasche (blaeulich, halb voll), zwei Haehne
Model waterCooler(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({-0.16f, -0.16f, 0.0f}, {0.16f, 0.16f, 0.95f}, 0.02f), Look::Plastic, 0.86f, 0.86f, 0.84f));
    p.push_back(part(cylinder({0, 0, 0.95f}, {0, 0, 1.4f}, 0.14f, 20, true), Look::Glass, 0.45f, 0.6f, 0.75f));
    p.push_back(part(cylinder({0, 0, 0.95f}, {0, 0, 1.18f}, 0.135f, 20, true), Look::Water, 0.2f, 0.35f, 0.5f));
    Mesh taps;
    taps.add(box({0.16f, -0.08f, 0.72f}, {0.2f, -0.04f, 0.76f}));
    taps.add(box({0.16f, 0.04f, 0.72f}, {0.2f, 0.08f, 0.76f}));
    p.push_back(part(taps, Look::Plastic, 0.25f, 0.3f, 0.7f));
    return finish(std::move(p));
}

// Fenster (Wandmontage, blickt entlang +x): Rahmen, Scheibe. v 0: geschwaerzt; v 1: Regen und Nebel
// (schwaches, kaltes Licht von draussen)
Model window(int v) {
    std::vector<Part> p;
    const float w = 1.6f, h = 1.3f, z0 = 0.9f;
    Mesh fr;
    fr.add(box({0.0f, -w * 0.5f - 0.06f, z0 - 0.06f}, {0.08f, w * 0.5f + 0.06f, z0}));
    fr.add(box({0.0f, -w * 0.5f - 0.06f, z0 + h}, {0.08f, w * 0.5f + 0.06f, z0 + h + 0.06f}));
    fr.add(box({0.0f, -w * 0.5f - 0.06f, z0}, {0.08f, -w * 0.5f, z0 + h}));
    fr.add(box({0.0f, w * 0.5f, z0}, {0.08f, w * 0.5f + 0.06f, z0 + h}));
    fr.add(box({0.0f, -0.02f, z0}, {0.08f, 0.02f, z0 + h}));
    fr.add(box({0.08f, -w * 0.5f - 0.08f, z0 - 0.08f}, {0.26f, w * 0.5f + 0.08f, z0 - 0.04f}));  // Fensterbank
    p.push_back(part(fr, Look::Paint, 0.82f, 0.82f, 0.8f));
    if (v == 0) p.push_back(part(box({0.02f, -w * 0.5f, z0}, {0.03f, w * 0.5f, z0 + h}), Look::Glass, 0.01f, 0.01f, 0.012f));
    else p.push_back(part(box({0.02f, -w * 0.5f, z0}, {0.03f, w * 0.5f, z0 + h}), Look::Glow, 0.42f, 0.48f, 0.55f, 0.35f));
    return finish(std::move(p));
}

// Abdruck von Moebeln im Teppich: dunklere Flaechen (v 0 Schreibtisch mit Stuhlkreis, v 1 Schrank)
Model carpetImprint(int v) {
    std::vector<Part> p;
    Mesh m;
    if (v == 0) {
        for (float x : {-0.75f, 0.75f})
            for (float y : {-0.35f, 0.35f}) m.add(box({x - 0.04f, y - 0.04f, 0.0f}, {x + 0.04f, y + 0.04f, 0.002f}));
        m.add(cylinder({0.0f, 0.75f, 0.0f}, {0.0f, 0.75f, 0.002f}, 0.32f, 20, true));
    } else {
        m.add(box({-0.6f, -0.3f, 0.0f}, {0.6f, 0.3f, 0.002f}));
    }
    p.push_back(part(m, Look::Fabric, 0.14f, 0.15f, 0.17f));
    return finish(std::move(p));
}

// --- Level 5: Hotel -------------------------------------------------------------------------------------------
// Kronleuchter (haengt an der Decke, Ursprung): Kette, Messingring, Kerzenarme mit leuchtenden Kolben,
// Glasbehang
Model chandelier(int v) {
    std::vector<Part> p;
    const float R = v == 1 ? 0.9f : 0.6f;
    p.push_back(part(cylinder({0, 0, -0.6f}, {0, 0, 0.0f}, 0.012f, 6, false), Look::Metal, 0.55f, 0.45f, 0.25f));
    Mesh brass;
    brass.add(sphere({0, 0, -0.7f}, {0.12f, 0.12f, 0.16f}, 12, 8));
    const int arms = v == 1 ? 12 : 8;
    Mesh bulbs, drops;
    for (int k = 0; k < arms; ++k) {
        const float a = 2.0f * kPi * (float)k / (float)arms;
        const vec3 tip{std::cos(a) * R, std::sin(a) * R, -0.82f};
        brass.add(pipePath({{0, 0, -0.75f}, {std::cos(a) * R * 0.5f, std::sin(a) * R * 0.5f, -0.95f}, tip}, 0.012f, 6, 0.1f));
        brass.add(cylinder(tip, tip + vec3{0, 0, 0.06f}, 0.03f, 10, true));
        bulbs.add(sphere(tip + vec3{0, 0, 0.11f}, {0.025f, 0.025f, 0.05f}, 8, 6));
        drops.add(sphere(tip + vec3{0, 0, -0.1f}, {0.02f, 0.02f, 0.05f}, 6, 4));
    }
    p.push_back(part(brass, Look::Metal, 0.66f, 0.5f, 0.24f));
    p.push_back(part(bulbs, Look::Glow, 1.0f, 0.86f, 0.62f, 1.6f));
    p.push_back(part(drops, Look::Glass, 0.8f, 0.82f, 0.84f));
    return finish(std::move(p));
}

// Wandleuchte (blickt entlang +x): Messingarm, Schirm, warmes Leuchten
Model wallSconce(int) {
    std::vector<Part> p;
    p.push_back(part(cylinder({0.0f, 0, 0}, {0.012f, 0, 0}, 0.06f, 12, true), Look::Metal, 0.62f, 0.48f, 0.24f));
    p.push_back(part(pipePath({{0.0f, 0, 0}, {0.12f, 0, 0.02f}, {0.15f, 0, 0.12f}}, 0.01f, 6, 0.05f), Look::Metal, 0.62f, 0.48f, 0.24f));
    p.push_back(part(cone({0.15f, 0, 0.1f}, {0.15f, 0, 0.26f}, 0.09f, 0.05f, 14, false), Look::Fabric, 0.9f, 0.78f, 0.56f));
    p.push_back(part(sphere({0.15f, 0, 0.16f}, {0.03f, 0.03f, 0.04f}, 8, 6), Look::Glow, 1.0f, 0.82f, 0.56f, 1.2f));
    return finish(std::move(p));
}

// Zimmertuer (verschlossen, blickt entlang +x): Rahmen, Fuellungstuer, Messingschild mit Nummer.
// variant = Zimmernummer (101 ... 999)
Model hotelDoor(int v) {
    std::vector<Part> p;
    const float w = 0.95f, h = 2.15f;
    Mesh frame;
    frame.add(bevelBox({0.0f, -w * 0.5f - 0.09f, 0.0f}, {0.05f, -w * 0.5f, h + 0.1f}, 0.01f));
    frame.add(bevelBox({0.0f, w * 0.5f, 0.0f}, {0.05f, w * 0.5f + 0.09f, h + 0.1f}, 0.01f));
    frame.add(bevelBox({0.0f, -w * 0.5f - 0.09f, h}, {0.05f, w * 0.5f + 0.09f, h + 0.12f}, 0.01f));
    p.push_back(part(frame, Look::Wood, 0.3f, 0.16f, 0.1f));
    Mesh leaf = bevelBox({0.005f, -w * 0.5f, 0.0f}, {0.035f, w * 0.5f, h}, 0.006f);
    for (float z0 : {0.2f, 1.2f}) leaf.add(bevelBox({0.035f, -w * 0.5f + 0.12f, z0}, {0.045f, w * 0.5f - 0.12f, z0 + 0.75f}, 0.012f));
    p.push_back(part(leaf, Look::Wood, 0.38f, 0.2f, 0.12f));
    Mesh brass;
    brass.add(cylinder({0.035f, w * 0.5f - 0.1f, 1.02f}, {0.09f, w * 0.5f - 0.1f, 1.02f}, 0.025f, 12, true));
    brass.add(bevelBox({0.045f, -0.13f, 1.55f}, {0.052f, 0.13f, 1.7f}, 0.004f));
    p.push_back(part(brass, Look::Metal, 0.7f, 0.54f, 0.26f));
    const std::string num = std::to_string(std::max(1, v % 1000));
    const float ch = 0.09f, cw = 0.06f;
    float y = (float)num.size() * cw * 0.5f;
    Mesh digits;
    for (char c : num) {
        const int cell = glyphCell(c);
        if (cell >= 0) digits.add(quad({0.0525f, y, 1.67f}, {0, -cw, 0}, {0, 0, -ch}, cellUv0(cell), cellUv1(cell)));
        y -= cw;
    }
    p.push_back(part(digits, Look::Stencil, 0.18f, 0.12f, 0.05f));
    return finish(std::move(p));
}

// Sofa (blickt entlang +x), Samt in Weinrot oder Gold
Model sofa(int v) {
    std::vector<Part> p;
    static const float col[2][3] = {{0.42f, 0.08f, 0.1f}, {0.6f, 0.46f, 0.18f}};
    const float* c = col[v & 1];
    Mesh m;
    m.add(bevelBox({-0.4f, -0.95f, 0.12f}, {0.45f, 0.95f, 0.45f}, 0.06f));
    m.add(bevelBox({-0.45f, -0.95f, 0.12f}, {-0.2f, 0.95f, 0.9f}, 0.08f));
    m.add(bevelBox({-0.45f, -1.08f, 0.12f}, {0.45f, -0.88f, 0.65f}, 0.08f));
    m.add(bevelBox({-0.45f, 0.88f, 0.12f}, {0.45f, 1.08f, 0.65f}, 0.08f));
    p.push_back(part(m, Look::Fabric, c[0], c[1], c[2]));
    Mesh legs;
    for (float x : {-0.38f, 0.38f})
        for (float y : {-1.0f, 1.0f}) legs.add(cone({x, y, 0.0f}, {x, y, 0.12f}, 0.02f, 0.03f, 8, true));
    p.push_back(part(legs, Look::Wood, 0.25f, 0.13f, 0.07f));
    return finish(std::move(p));
}

// Sessel
Model armchair(int v) {
    std::vector<Part> p;
    static const float col[2][3] = {{0.42f, 0.08f, 0.1f}, {0.6f, 0.46f, 0.18f}};
    const float* c = col[v & 1];
    Mesh m;
    m.add(bevelBox({-0.35f, -0.4f, 0.12f}, {0.4f, 0.4f, 0.45f}, 0.06f));
    m.add(bevelBox({-0.4f, -0.4f, 0.12f}, {-0.18f, 0.4f, 1.0f}, 0.08f));
    m.add(bevelBox({-0.4f, -0.5f, 0.12f}, {0.4f, -0.34f, 0.68f}, 0.07f));
    m.add(bevelBox({-0.4f, 0.34f, 0.12f}, {0.4f, 0.5f, 0.68f}, 0.07f));
    p.push_back(part(m, Look::Fabric, c[0], c[1], c[2]));
    return finish(std::move(p));
}

// Beistelltisch (rund) mit Lampe oder leer
Model sideTable(int v) {
    std::vector<Part> p;
    Mesh t = cylinder({0, 0, 0.62f}, {0, 0, 0.66f}, 0.3f, 20, true);
    t.add(cylinder({0, 0, 0.0f}, {0, 0, 0.62f}, 0.03f, 10, false));
    t.add(cylinder({0, 0, 0.0f}, {0, 0, 0.03f}, 0.2f, 16, true));
    p.push_back(part(t, Look::Wood, 0.3f, 0.16f, 0.09f));
    if (v == 1) {
        p.push_back(part(cylinder({0, 0, 0.66f}, {0, 0, 0.98f}, 0.02f, 8, false), Look::Metal, 0.66f, 0.5f, 0.25f));
        p.push_back(part(cone({0, 0, 0.92f}, {0, 0, 1.12f}, 0.16f, 0.09f, 16, false), Look::Fabric, 0.9f, 0.8f, 0.6f));
    }
    return finish(std::move(p));
}

// Spieltisch im Ballsaal: gruenes Tuch, verteilte Karten, Chips - eine Partie, die nie beendet wurde
Model gameTable(int) {
    std::vector<Part> p;
    Rnd r(3600u);
    p.push_back(part(cylinder({0, 0, 0.72f}, {0, 0, 0.78f}, 1.1f, 32, true), Look::Wood, 0.3f, 0.15f, 0.08f));
    p.push_back(part(cylinder({0, 0, 0.78f}, {0, 0, 0.785f}, 1.0f, 32, true), Look::Fabric, 0.1f, 0.3f, 0.16f));
    p.push_back(part(cylinder({0, 0, 0.0f}, {0, 0, 0.72f}, 0.12f, 12, false), Look::Wood, 0.28f, 0.14f, 0.08f));
    Mesh cards, chips;
    for (int k = 0; k < 14; ++k) {
        const float a = r.range(0.0f, 6.28f), d = r.range(0.2f, 0.85f);
        Mesh c = box({-0.04f, -0.06f, 0.0f}, {0.04f, 0.06f, 0.002f});
        c.transform(mat4::translation({std::cos(a) * d, std::sin(a) * d, 0.786f + 0.002f * (float)(k % 3)}) * mat4::rotationZ(r.range(0.0f, 6.28f)));
        cards.add(c);
    }
    for (int k = 0; k < 9; ++k) {
        const float a = r.range(0.0f, 6.28f), d = r.range(0.3f, 0.8f);
        const int st = 1 + (int)r.range(0.0f, 4.0f);
        chips.add(cylinder({std::cos(a) * d, std::sin(a) * d, 0.786f}, {std::cos(a) * d, std::sin(a) * d, 0.786f + 0.008f * (float)st}, 0.02f, 10, true));
    }
    p.push_back(part(cards, Look::Plastic, 0.92f, 0.9f, 0.86f));
    p.push_back(part(chips, Look::Plastic, 0.6f, 0.08f, 0.08f));
    Mesh chairs;
    for (int k = 0; k < 6; ++k) {
        const float a = 2.0f * kPi * (float)k / 6.0f;
        Mesh c = bevelBox({-0.22f, -0.22f, 0.42f}, {0.22f, 0.22f, 0.48f}, 0.02f);
        c.add(bevelBox({0.18f, -0.2f, 0.48f}, {0.24f, 0.2f, 1.0f}, 0.02f));
        for (float x : {-0.18f, 0.18f})
            for (float y : {-0.18f, 0.18f}) c.add(box({x - 0.02f, y - 0.02f, 0.0f}, {x + 0.02f, y + 0.02f, 0.42f}));
        c.transform(mat4::translation({std::cos(a) * 1.45f, std::sin(a) * 1.45f, 0.0f}) * mat4::rotationZ(a));
        chairs.add(c);
    }
    p.push_back(part(chairs, Look::Wood, 0.34f, 0.18f, 0.1f));
    return finish(std::move(p));
}

// Kessel (Kesselraum): liegender Zylinder auf Fuessen, Mannloch, Rohre, Manometer
Model boiler(int) {
    std::vector<Part> p;
    Mesh body = cylinder({-1.6f, 0, 1.1f}, {1.6f, 0, 1.1f}, 0.9f, 28, true);
    for (float x : {-1.2f, 0.0f, 1.2f}) body.add(cylinder({x - 0.03f, 0, 1.1f}, {x + 0.03f, 0, 1.1f}, 0.93f, 28, false));
    p.push_back(part(body, Look::Paint, 0.2f, 0.18f, 0.16f));
    Mesh feet;
    for (float x : {-1.2f, 1.2f}) feet.add(bevelBox({x - 0.2f, -0.6f, 0.0f}, {x + 0.2f, 0.6f, 0.4f}, 0.02f));
    p.push_back(part(feet, Look::Concrete, 0.45f, 0.44f, 0.42f));
    Mesh fit;
    fit.add(cylinder({1.6f, 0, 1.1f}, {1.7f, 0, 1.1f}, 0.35f, 20, true));
    fit.add(cylinder({0.0f, 0, 2.0f}, {0.0f, 0, 3.2f}, 0.16f, 14, false));
    fit.add(cylinder({-0.9f, 0, 2.0f}, {-0.9f, 0, 2.5f}, 0.08f, 10, false));
    p.push_back(part(fit, Look::Metal, 0.42f, 0.36f, 0.3f));
    return finish(std::move(p));
}

// Alter Lautsprecher (Trichter an der Wand, blickt entlang +x)
Model speaker(int) {
    std::vector<Part> p;
    p.push_back(part(bevelBox({0.0f, -0.12f, -0.12f}, {0.1f, 0.12f, 0.12f}, 0.01f), Look::Wood, 0.32f, 0.2f, 0.12f));
    p.push_back(part(cone({0.1f, 0, 0}, {0.4f, 0, 0}, 0.05f, 0.2f, 18, false), Look::Metal, 0.62f, 0.5f, 0.28f));
    return finish(std::move(p));
}

Model puddle(int v) {
    Rnd r(2300u + (u32)v);
    const float size = 0.8f + 0.9f * r.uni();
    std::vector<vec2> o;
    const int n = 18;
    for (int k = 0; k < n; ++k) {
        float a = 2.0f * kPi * (float)k / (float)n;
        float rr = size * (0.75f + 0.25f * std::sin(a * 3.0f + r.range(0, 6)) + 0.15f * r.range(-1, 1));
        o.push_back({std::cos(a) * rr * 1.3f, std::sin(a) * rr});
    }
    return finish({part(prism(o, 0.0f, 0.003f), Look::Water, 0.04f, 0.045f, 0.045f)});
}

Model pipeRun(int v) {
    const float r = v == 1 ? 0.07f : 0.045f;
    std::vector<Part> p;
    p.push_back(part(cylinder({-0.5f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f}, r, 14, false), v == 1 ? Look::Paint : Look::Galvanized,
                     v == 1 ? 0.36f : 0.6f, v == 1 ? 0.3f : 0.6f, v == 1 ? 0.22f : 0.6f));
    Mesh br;
    br.add(box({-0.03f, -r - 0.012f, -r - 0.015f}, {0.03f, r + 0.012f, -r}));
    br.add(box({-0.03f, -0.015f, -r - 0.015f}, {0.03f, 0.015f, 0.3f}));
    p.push_back(part(br, Look::Metal, 0.4f, 0.4f, 0.4f));
    return finish(std::move(p));
}

// --- Verzeichnis -------------------------------------------------------------------------------------
struct Entry {
    int variants;
    std::function<Model(int)> make;
};

const std::map<std::string, Entry>& registry() {
    static const std::map<std::string, Entry> r = {
        {"batten", {2, batten}},
        {"highbay", {1, highbay}},
        {"exit_sign", {2, exitSign}},
        {"stall_line", {2, stallLine}},
        {"arrow", {1, arrow}},
        {"pillar_label", {26 * 256, pillarLabel}},
        {"car", {4, car}},
        {"locker_shutter", {4, [](int v) { return shutter(1.8f, 2.2f, v, v == 3 ? 0.55f : 0.0f); }}},
        {"rollgate", {4, [](int v) { return shutter(3.0f, 3.6f, v, 0.0f); }}},
        {"desk_crt", {3, deskCrt}},
        {"bed", {1, bed}},
        {"painting", {8, painting}},
        {"camp", {4, camp}},
        {"power_box", {1, powerBox}},
        {"hole_glow", {1, holeGlow}},
        {"crate", {4, crate}},
        {"pallet", {4, pallet}},
        {"rack", {8, rack}},
        {"safety_light", {1, safetyLight}},
        {"outlet", {1, outlet}},
        {"vent_grille", {1, ventGrille}},
        {"ceiling_vent", {2, ceilingVent}},
        {"arch", {7, arch}},
        {"bookcase", {4, bookcase}},
        {"coffee_table", {1, coffeeTable}},
        {"shelf_unit", {2, shelfUnit}},
        {"ceiling_globe", {1, ceilingGlobe}},
        {"column", {80, column}},
        {"valve_wheel", {2, valveWheel}},
        {"gauge", {4, gauge}},
        {"steel_door", {3, steelDoor}},
        {"barrel", {3, barrel}},
        {"cart", {1, cart}},
        {"pump", {1, pump}},
        {"pipe_bundle", {8, pipeBundle}},
        {"rafter", {1, rafter}},
        {"insulation", {4, insulation}},
        {"floor_glow", {2, floorGlow}},
        {"wardrobe", {2, wardrobe}},
        {"chair", {2, chair}},
        {"trunk", {1, trunk}},
        {"damaged_door", {1, damagedDoor}},
        {"transformer", {2, transformer}},
        {"fence_panel", {1, fencePanel}},
        {"switch_cabinet", {3, switchCabinet}},
        {"cable_tray", {4, cableTray}},
        {"statue", {1, statue}},
        {"water_cooler", {1, waterCooler}},
        {"window", {2, window}},
        {"carpet_imprint", {2, carpetImprint}},
        {"chandelier", {2, chandelier}},
        {"wall_sconce", {1, wallSconce}},
        {"hotel_door", {1000, hotelDoor}},
        {"sofa", {2, sofa}},
        {"armchair", {2, armchair}},
        {"side_table", {2, sideTable}},
        {"game_table", {1, gameTable}},
        {"boiler", {1, boiler}},
        {"speaker", {1, speaker}},
        {"water_surface", {1, waterSurface}},
        {"barrier", {2, barrier}},
        {"rubble", {4, rubble}},
        {"slab_edge", {4, slabEdge}},
        {"puddle", {6, puddle}},
        {"pipe_run", {2, pipeRun}},
    };
    return r;
}

}  // namespace

// V12: Modelle aus Dateien (eine Variante je Datei)
std::mutex& importedMutex() {
    static std::mutex m;
    return m;
}
std::map<std::string, std::unique_ptr<Model>>& imported() {
    static std::map<std::string, std::unique_ptr<Model>> m;
    return m;
}

void registerModel(const std::string& name, Model m) {
    std::lock_guard lock(importedMutex());
    imported()[name] = std::make_unique<Model>(std::move(m));
}

const Model* model(const std::string& name, int variant) {
    {
        std::lock_guard lock(importedMutex());
        auto im = imported().find(name);
        if (im != imported().end()) return im->second.get();
    }
    static std::mutex mutex;
    static std::map<std::pair<std::string, int>, std::unique_ptr<Model>> cache;
    const auto& reg = registry();
    auto it = reg.find(name);
    if (it == reg.end()) return nullptr;
    const int v = ((variant % it->second.variants) + it->second.variants) % it->second.variants;
    std::lock_guard lock(mutex);
    auto& slot = cache[{name, v}];
    if (!slot) slot = std::make_unique<Model>(it->second.make(v));
    return slot.get();
}

int variants(const std::string& name) {
    {
        std::lock_guard lock(importedMutex());
        if (imported().count(name)) return 1;
    }
    auto it = registry().find(name);
    return it == registry().end() ? 0 : it->second.variants;
}

std::vector<std::string> modelNames() {
    std::vector<std::string> n;
    for (const auto& [k, e] : registry()) n.push_back(k);
    std::lock_guard lock(importedMutex());
    for (const auto& [k, e] : imported())
        if (!registry().count(k)) n.push_back(k);
    return n;
}

int glyphCell(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == '-') return 36;
    return -1;  // Leerzeichen und Unbekanntes: kein Zeichen
}

}  // namespace lim::props
