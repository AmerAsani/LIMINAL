// V13.3: Leuchtstab (KREATUR_Smiler.md, "Leuchtstab als Koeder"). Ein handelsueblicher Knicklicht-Stab: 15 cm Huelle
// aus durchscheinendem Kunststoff (1,5 cm dick, hinten gerundet), vorn eine Kappe mit Oese zum Aufhaengen. Liegend:
// Achse entlang +x, Mitte im Ursprung (x), Unterkante auf z = 0 (Achse auf z = 0,0085 m).
#include <cmath>

#include "render/meshes.hpp"

namespace lim::gfx {

namespace {
constexpr float kAxisZ = 0.0085f;

// Drehkoerper um die z-Achse (Profil r, z von hinten nach vorn) - danach auf die x-Achse gelegt
void lathe(MeshData& d, const float (*prof)[2], int np, int seg, float v0, float v1) {
    const u16 base = (u16)d.vertices.size();
    const float z0 = prof[0][1], z1 = prof[np - 1][1];
    for (int i = 0; i < np; ++i) {
        const int i0 = i > 0 ? i - 1 : 0, i1 = i + 1 < np ? i + 1 : np - 1;
        const float dr = prof[i1][0] - prof[i0][0], dz = prof[i1][1] - prof[i0][1];
        float nr = dz, nz = -dr;
        const float nl = std::sqrt(nr * nr + nz * nz) + 1e-9f;
        nr /= nl, nz /= nl;
        for (int j = 0; j <= seg; ++j) {
            const float u = (float)j / (float)seg, a = u * 6.2831853f, c = std::cos(a), s = std::sin(a);
            const float v = v0 + (v1 - v0) * (prof[i][1] - z0) / std::max(1e-6f, z1 - z0);
            d.vertices.push_back({{c * prof[i][0], s * prof[i][0], prof[i][1]}, normalize(vec3{c * nr, s * nr, nz}), {-s, c, 0, 1}, {u, v}});
        }
    }
    for (int i = 0; i + 1 < np; ++i)
        for (int j = 0; j < seg; ++j) {
            const u16 a = (u16)(base + i * (seg + 1) + j), b = (u16)(a + seg + 1);
            d.indices.insert(d.indices.end(), {a, b, (u16)(a + 1), (u16)(a + 1), b, (u16)(b + 1)});
        }
}

// Ring (Torus) in der Ebene x-z um (0, 0, cz): Oese der Kappe
void ring(MeshData& d, float cz, float R, float r, int seg, int side) {
    const u16 base = (u16)d.vertices.size();
    for (int i = 0; i <= seg; ++i) {
        const float a = (float)i / (float)seg * 6.2831853f;
        const vec3 c{R * std::cos(a), 0.0f, cz + R * std::sin(a)};
        const vec3 out{std::cos(a), 0.0f, std::sin(a)}, ax{0.0f, 1.0f, 0.0f};
        for (int j = 0; j <= side; ++j) {
            const float b = (float)j / (float)side * 6.2831853f;
            const vec3 n = out * std::cos(b) + ax * std::sin(b);
            const vec3 t{-std::sin(a), 0.0f, std::cos(a)};
            d.vertices.push_back({c + n * r, n, {t.x, t.y, t.z, 1}, {(float)i / seg, (float)j / side}});
        }
    }
    for (int i = 0; i < seg; ++i)
        for (int j = 0; j < side; ++j) {
            const u16 a = (u16)(base + i * (side + 1) + j), b = (u16)(a + side + 1);
            d.indices.insert(d.indices.end(), {a, b, (u16)(a + 1), (u16)(a + 1), b, (u16)(b + 1)});  // Umlaufsinn wie lathe
        }
}

// z-Achse -> x-Achse (Drehung um y um +90 Grad: (x, y, z) -> (z, y, -x)), Achse auf kAxisZ
void layDown(MeshData& d) {
    for (MeshVertex& v : d.vertices) {
        v.pos = vec3{v.pos.z, v.pos.y, -v.pos.x + kAxisZ};
        v.normal = vec3{v.normal.z, v.normal.y, -v.normal.x};
        v.tangent = vec4{v.tangent.z, v.tangent.y, -v.tangent.x, v.tangent.w};
    }
}
}  // namespace

GlowstickParts makeGlowstick() {
    GlowstickParts g;
    // Huelle: hinten halbrund geschlossen, gerade bis unter die Kappe
    const float tube[][2] = {{0.0f, -0.0755f},  {0.0030f, -0.0751f}, {0.0053f, -0.0738f}, {0.0068f, -0.0718f}, {0.0075f, -0.0690f},
                             {0.0075f, -0.0500f}, {0.0075f, 0.0f},     {0.0075f, 0.0500f},  {0.0075f, 0.0640f},  {0.0068f, 0.0650f},
                             {0.0f, 0.0650f}};
    lathe(g.tube, tube, (int)(sizeof(tube) / sizeof(tube[0])), 20, 0.0f, 1.0f);
    // Kappe mit Rille, vorn ein kurzer Steg zur Oese
    const float cap[][2] = {{0.0f, 0.0598f},    {0.0079f, 0.0600f}, {0.0088f, 0.0608f}, {0.0088f, 0.0668f}, {0.0083f, 0.0672f},
                            {0.0083f, 0.0684f}, {0.0088f, 0.0688f}, {0.0088f, 0.0760f}, {0.0081f, 0.0782f}, {0.0040f, 0.0790f},
                            {0.0024f, 0.0792f}, {0.0024f, 0.0812f}, {0.0f, 0.0814f}};
    lathe(g.cap, cap, (int)(sizeof(cap) / sizeof(cap[0])), 20, 0.0f, 1.0f);
    ring(g.cap, 0.0858f, 0.0046f, 0.0012f, 18, 8);
    layDown(g.tube);
    layDown(g.cap);
    return g;
}

}  // namespace lim::gfx
