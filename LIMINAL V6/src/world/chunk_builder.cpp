// Chunk-Aufbau: Kacheln rastern, Umgebungslicht, Lichtquellen und das
// Dreiecksnetz der Weltgeometrie erzeugen.
#include <algorithm>
#include <unordered_set>

#include "world/chunk.hpp"
#include "world/materials.hpp"
#include "world/room.hpp"

namespace lim::world {
namespace {

// Abdunklung einer Ecke je Anzahl angrenzender offener Kacheln (V4: AO[4 - n])
constexpr float kAO[5] = {1.0f, 0.86f, 0.74f, 0.64f, 0.6f};
constexpr float kEps = 0.0015f;  // Ueberlappung benachbarter Flaechen gegen Pixelspalten

const Room* ownerRoom(const Tile& t) {
    if (t.room) return t.room;
    if (t.door) return t.door->localRoom();
    return nullptr;
}

vec3 tintOf(const Tile& t) {
    const Room* r = ownerRoom(t);
    if (!r) return vec3(1.0f);
    vec3 lamp{(float)r->lampColor[0], (float)r->lampColor[1], (float)r->lampColor[2]};
    vec3 wall{(float)r->wallColor[0], (float)r->wallColor[1], (float)r->wallColor[2]};
    float wl = std::max(0.05f, (wall.x + wall.y + wall.z) / 3.0f);
    // Licht der Leuchten, leicht gefaerbt vom Widerschein der Waende
    return lamp * lerp(vec3(1.0f), wall / wl, 0.35f);
}

struct Mesher {
    ChunkMesh& mesh;
    const MaterialBank& bank;
    u32 flickMode = 0, flickSeed = 0;

    void quad(vec3 a, vec3 b, vec3 c, vec3 d, u32 axis, u16 mat, u32 extra = 255) {
        static const vec3 kN[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        vec3 n = kN[axis];
        // Wicklung so, dass cross(b - a, c - a) in Richtung der Normalen zeigt
        if (dot(cross(b - a, c - a), n) < 0.0f) std::swap(b, d);
        u32 base = (u32)mesh.vertices.size();
        u32 data = packVertexData(axis, mat, flickMode, flickSeed, extra);
        for (vec3 p : {a, b, c, d}) {
            mesh.vertices.push_back({p.x, p.y, p.z, data});
            mesh.bounds.add(p);
        }
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }

    // Waagrechte Flaeche [x0,x1] x [y0,y1] auf Hoehe z.
    void horizontal(float x0, float y0, float x1, float y1, float z, bool up, u16 mat, u32 extra = 255) {
        quad({x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}, up ? NZ_POS : NZ_NEG, mat, extra);
    }

    // Senkrechte Flaeche auf der Ebene x = c (axisX) bzw. y = c, Spanne [s0, s1], Hoehe [z0, z1].
    void vertical(bool planeX, float c, float s0, float s1, float z0, float z1, u32 axis, u16 mat, u32 extra = 255) {
        if (z1 - z0 < 1e-4f) return;
        if (planeX) quad({c, s0, z0}, {c, s1, z0}, {c, s1, z1}, {c, s0, z1}, axis, mat, extra);
        else quad({s0, c, z0}, {s1, c, z0}, {s1, c, z1}, {s0, c, z1}, axis, mat, extra);
    }

    // Achsenparallele Box (alle 6 Seiten, ausser die Unterseite wenn noBottom).
    void box(vec3 lo, vec3 hi, u16 mat, bool noBottom = false) {
        horizontal(lo.x, lo.y, hi.x, hi.y, hi.z, true, mat);
        if (!noBottom) horizontal(lo.x, lo.y, hi.x, hi.y, lo.z, false, mat);
        vertical(true, lo.x, lo.y, hi.y, lo.z, hi.z, NX_NEG, mat);
        vertical(true, hi.x, lo.y, hi.y, lo.z, hi.z, NX_POS, mat);
        vertical(false, lo.y, lo.x, hi.x, lo.z, hi.z, NY_NEG, mat);
        vertical(false, hi.y, lo.x, hi.x, lo.z, hi.z, NY_POS, mat);
    }
};

// Abmessung der Deckenleuchte in Welt-x/y. Lange Leuchten liegen entlang der Raumachse.
vec2 lampExtent(const Room* r) {
    vec2 s{0.62f, 0.62f};  // (quer, laengs)
    const std::string& style = r->lightStyle;
    if (style == "panels_long") s = {0.28f, 0.96f};
    else if (style == "pools" || style == "void") s = {0.5f, 0.5f};
    else if (style == "emergency") s = {0.22f, 0.42f};
    else if (style == "dark") s = {0.3f, 0.3f};
    return r->axis() == 0 ? vec2{s.y, s.x} : s;
}

void emitCeilingLamp(Mesher& m, const Tile& t, float X, float Y) {
    const Room* r = ownerRoom(t);
    vec2 ext = lampExtent(r);
    float hx = ext.x * 0.5f, hy = ext.y * 0.5f;
    float cx = X + 0.5f, cy = Y + 0.5f;
    float z = t.ceil;
    const float drop = 0.035f, rim = 0.035f;
    // Gehaeuse (Rahmen) und leuchtende Scheibe
    vec3 lo{cx - hx - rim, cy - hy - rim, z - drop};
    vec3 hi{cx + hx + rim, cy + hy + rim, z};
    m.vertical(true, lo.x, lo.y, hi.y, lo.z, hi.z, NX_NEG, r->mFixture);
    m.vertical(true, hi.x, lo.y, hi.y, lo.z, hi.z, NX_POS, r->mFixture);
    m.vertical(false, lo.y, lo.x, hi.x, lo.z, hi.z, NY_NEG, r->mFixture);
    m.vertical(false, hi.y, lo.x, hi.x, lo.z, hi.z, NY_POS, r->mFixture);
    // Rahmenring (4 Streifen) auf der Unterseite
    m.horizontal(lo.x, lo.y, hi.x, cy - hy, lo.z, false, r->mFixture);
    m.horizontal(lo.x, cy + hy, hi.x, hi.y, lo.z, false, r->mFixture);
    m.horizontal(lo.x, cy - hy, cx - hx, cy + hy, lo.z, false, r->mFixture);
    m.horizontal(cx + hx, cy - hy, hi.x, cy + hy, lo.z, false, r->mFixture);
    m.horizontal(cx - hx, cy - hy, cx + hx, cy + hy, lo.z + 0.004f, false, r->lampmat);
}

// Wandleuchte (Leuchtkasten) an der Wand hinter der Kachel.
void emitWallLamp(Mesher& m, const Room* r, float X, float Y, float floorZ, int dirX, int dirY) {
    const float halfW = 0.26f, halfH = 0.13f, depth = 0.08f;
    float zc = floorZ + 1.9f;
    float cx = X + 0.5f, cy = Y + 0.5f;
    vec3 lo, hi;
    u32 frontAxis;
    if (dirX != 0) {
        float wallX = dirX > 0 ? X : X + 1.0f;  // Wand liegt entgegen der Richtung
        float fx = wallX + dirX * depth;
        lo = {std::min(wallX, fx), cy - halfW, zc - halfH};
        hi = {std::max(wallX, fx), cy + halfW, zc + halfH};
        frontAxis = dirX > 0 ? NX_POS : NX_NEG;
        m.vertical(true, dirX > 0 ? hi.x : lo.x, lo.y, hi.y, lo.z, hi.z, frontAxis, r->lampmat);
    } else {
        float wallY = dirY > 0 ? Y : Y + 1.0f;
        float fy = wallY + dirY * depth;
        lo = {cx - halfW, std::min(wallY, fy), zc - halfH};
        hi = {cx + halfW, std::max(wallY, fy), zc + halfH};
        frontAxis = dirY > 0 ? NY_POS : NY_NEG;
        m.vertical(false, dirY > 0 ? hi.y : lo.y, lo.x, hi.x, lo.z, hi.z, frontAxis, r->lampmat);
    }
    // Seiten, Ober- und Unterseite
    m.horizontal(lo.x, lo.y, hi.x, hi.y, hi.z, true, r->mFixture);
    m.horizontal(lo.x, lo.y, hi.x, hi.y, lo.z, false, r->mFixture);
    if (dirX != 0) {
        m.vertical(false, lo.y, lo.x, hi.x, lo.z, hi.z, NY_NEG, r->mFixture);
        m.vertical(false, hi.y, lo.x, hi.x, lo.z, hi.z, NY_POS, r->mFixture);
    } else {
        m.vertical(true, lo.x, lo.y, hi.y, lo.z, hi.z, NX_NEG, r->mFixture);
        m.vertical(true, hi.x, lo.y, hi.y, lo.z, hi.z, NX_POS, r->mFixture);
    }
}

// Volle Wand zwischen offener Kachel und massivem Nachbarn, inkl. Sockelleiste.
// planeX/c/s0/s1 beschreiben die Wandebene, axis die Normale (zur Kachel hin).
void emitWall(Mesher& m, const Tile& t, bool planeX, float c, float s0, float s1, u32 axis) {
    const Material& mat = m.bank[t.roomWall];
    float z0 = t.floor, z1 = t.ceil;
    const Room* r = t.room;
    float rf = r ? (float)r->floor : t.floor;
    // Baender (Sockelleiste); leuchtende Baender werden als Wandleuchte gebaut
    struct Seg {
        float a, b;
        u16 mat;
        bool trim;
    };
    std::vector<Seg> bands;
    for (const Band& b : mat.bands) {
        if (m.bank[b.mat].emissive) continue;
        float a = std::max(z0, rf + (float)b.z0), e = std::min(z1, rf + (float)b.z1);
        if (e > a + 1e-4f) bands.push_back({a, e, b.mat, true});
    }
    std::sort(bands.begin(), bands.end(), [](const Seg& x, const Seg& y) { return x.a < y.a; });
    float cur = z0;
    for (const Seg& b : bands) {
        if (b.a > cur) m.vertical(planeX, c, s0 - kEps, s1 + kEps, cur - kEps, b.a + kEps, axis, t.roomWall);
        cur = std::max(cur, b.b);
    }
    if (z1 > cur) m.vertical(planeX, c, s0 - kEps, s1 + kEps, cur - kEps, z1 + kEps, axis, t.roomWall);
    // Sockelleiste: leicht vorstehend, mit Oberkante und Stirnseiten
    const float out = 0.014f;
    float sign = (axis == NX_POS || axis == NY_POS) ? 1.0f : -1.0f;
    float cf = c + sign * out;
    for (const Seg& b : bands) {
        m.vertical(planeX, cf, s0, s1, b.a, b.b, axis, b.mat);
        float lo = std::min(c, cf), hi = std::max(c, cf);
        if (planeX) {
            m.horizontal(lo, s0, hi, s1, b.b, true, b.mat);
            m.vertical(false, s0, lo, hi, b.a, b.b, NY_NEG, b.mat);
            m.vertical(false, s1, lo, hi, b.a, b.b, NY_POS, b.mat);
        } else {
            m.horizontal(s0, lo, s1, hi, b.b, true, b.mat);
            m.vertical(true, s0, lo, hi, b.a, b.b, NX_NEG, b.mat);
            m.vertical(true, s1, lo, hi, b.a, b.b, NX_POS, b.mat);
        }
    }
}

// V6: Der Automat ist ein eigenes Modell. Seine Kachel bleibt fuer Kollision und Schatten ein Block,
// wird aber wie freier Boden gezeichnet (Boden, Wand dahinter, Decke) - ohne Stufenflaechen.
Tile visualTile(const Tile& t) {
    if (!(t.flags & TF_MACHINE) || !t.room) return t;
    Tile v = t;
    v.floor = (float)t.room->floor;
    v.fmat = t.room->fmat;
    v.wmat = t.room->wmat;
    v.flags = (u8)(t.flags & ~(TF_MACHINE | TF_FEATURE));
    return v;
}

void meshTile(Mesher& m, const ChunkData& cd, int lx, int ly) {
    const Tile t = visualTile(cd.tile(lx, ly));
    if (t.solid()) return;
    const Room* room = ownerRoom(t);
    m.flickMode = room ? (u32)room->flicker : 0u;
    m.flickSeed = room ? (u32)(room->seed & 31u) : 0u;
    const float X = (float)(cd.x0() + lx), Y = (float)(cd.y0() + ly);

    // Boden und Decke
    m.horizontal(X - kEps, Y - kEps, X + 1 + kEps, Y + 1 + kEps, t.floor, true, t.fmat);
    if (t.flags & TF_LAMP) {
        m.horizontal(X - kEps, Y - kEps, X + 1 + kEps, Y + 1 + kEps, t.ceil, false, room->cmat);
        emitCeilingLamp(m, t, X, Y);
    } else {
        m.horizontal(X - kEps, Y - kEps, X + 1 + kEps, Y + 1 + kEps, t.ceil, false, t.cmat);
    }

    // Vier Seiten: Nachbar (dx, dy), Wandebene und Normale zur Kachel hin
    struct Side {
        int dx, dy;
        bool planeX;
        float c, s0, s1;
        u32 axis;
    };
    const Side sides[4] = {
        {1, 0, true, X + 1, Y, Y + 1, NX_NEG},
        {-1, 0, true, X, Y, Y + 1, NX_POS},
        {0, 1, false, Y + 1, X, X + 1, NY_NEG},
        {0, -1, false, Y, X, X + 1, NY_POS},
    };
    for (const Side& s : sides) {
        const Tile n = visualTile(cd.tile(lx + s.dx, ly + s.dy));
        if (n.solid() || n.floor >= t.ceil || n.ceil <= t.floor) {
            if (!n.solid() && n.floor >= t.ceil) {
                // Nachbar ist ein Block, der hoeher als unsere Decke reicht
                m.vertical(s.planeX, s.c, s.s0 - kEps, s.s1 + kEps, t.floor - kEps, t.ceil + kEps, s.axis, n.wmat);
            } else {
                emitWall(m, t, s.planeX, s.c, s.s0, s.s1, s.axis);
            }
            continue;
        }
        if (n.floor > t.floor) {
            // Stufe, Kiste, Podest, Theke, Automat
            float top = std::min(n.floor, t.ceil);
            m.vertical(s.planeX, s.c, s.s0 - kEps, s.s1 + kEps, t.floor - kEps, top + kEps, s.axis, n.wmat);
        }
        if (n.ceil < t.ceil) {
            // Tuersturz, Traeger, niedrigere Decke dahinter
            float bot = std::max(n.ceil, t.floor);
            m.vertical(s.planeX, s.c, s.s0 - kEps, s.s1 + kEps, bot - kEps, t.ceil + kEps, s.axis, t.roomWall);
        }
    }

    // Wandleuchte an der Raumwand hinter dieser Kachel
    if ((t.flags & TF_WALLLAMP) && room) {
        for (const Fixture& f : room->fixtures()) {
            if (f.kind != Fixture::Wall) continue;
            if ((i64)f.x != (i64)X || (i64)f.y != (i64)Y) continue;
            const Tile& behind = cd.tile(lx - f.dirX, ly - f.dirY);
            if (behind.solid()) emitWallLamp(m, room, X, Y, (float)room->floor, f.dirX, f.dirY);
        }
    }
}

}  // namespace

void buildChunk(ChunkData& out, TileFetch fetch, void* ctx, const MaterialBank& bank) {
    const i64 X0 = out.x0(), Y0 = out.y0();
    for (int ly = -1; ly <= CS; ++ly)
        for (int lx = -1; lx <= CS; ++lx) out.tiles[(size_t)((ly + 1) * CB + (lx + 1))] = fetch(ctx, X0 + lx, Y0 + ly);

    // Eckhelligkeiten (V4) und getoentes Umgebungslicht
    for (int j = 0; j <= CS; ++j)
        for (int i = 0; i <= CS; ++i) {
            const Tile* cells[4] = {&out.tiles[(size_t)(j * CB + i)], &out.tiles[(size_t)(j * CB + i + 1)],
                                    &out.tiles[(size_t)((j + 1) * CB + i)], &out.tiles[(size_t)((j + 1) * CB + i + 1)]};
            double s = 0.0;
            vec3 rgb(0.0f);
            int n = 0;
            for (const Tile* c : cells)
                if (c->open()) {
                    s += c->light;
                    rgb += tintOf(*c) * c->light;
                    ++n;
                }
            out.corners[(size_t)(j * (CS + 1) + i)] = n ? (float)((s / n) * kAO[4 - n]) : 0.0f;
            if (i < CS && j < CS) out.ambient[(size_t)(j * CS + i)] = n ? rgb * (kAO[4 - n] / (float)n) : vec3(0.0f);
        }

    // Lichtquellen und Items der Raeume in diesem Chunk
    std::unordered_set<const Room*> rooms;
    for (int ly = 0; ly < CS; ++ly)
        for (int lx = 0; lx < CS; ++lx) {
            const Tile& t = out.tile(lx, ly);
            if (t.room) rooms.insert(t.room);
        }
    for (const Room* r : rooms) {
        vec3 lamp{(float)r->lampColor[0], (float)r->lampColor[1], (float)r->lampColor[2]};
        const std::string& ls = r->lightStyle;
        const u8 hum = (ls == "panels" || ls == "panels_wide" || ls == "panels_long" || ls == "bright") ? 1
                       : ls == "pools"                                                                 ? 2
                       : ls == "emergency"                                                             ? 3
                                                                                                         : 0;
        for (const Fixture& f : r->fixtures()) {
            i64 fx = ifloor(f.x), fy = ifloor(f.y);
            if (fx < X0 || fx >= X0 + CS || fy < Y0 || fy >= Y0 + CS) continue;
            const Tile& t = out.tile((int)(fx - X0), (int)(fy - Y0));
            ChunkLight L{};
            L.intensity = (float)f.intensity;
            L.radius = (float)f.radius;
            L.flickerMode = (u8)r->flicker;
            L.flickerSeed = (u8)(r->seed & 31u);
            L.dirX = (i8)f.dirX;
            L.dirY = (i8)f.dirY;
            switch (f.kind) {
                case Fixture::Ceiling: {
                    L.kind = ChunkLight::Ceiling;
                    L.pos = {(float)f.x, (float)f.y, t.ceil - 0.05f};
                    L.color = lamp;
                    L.area = lampExtent(r);
                    L.drop = t.ceil - t.floor;
                    L.hum = hum;
                    break;
                }
                case Fixture::Wall:
                    L.kind = ChunkLight::Wall;
                    L.pos = {(float)f.x - f.dirX * 0.38f, (float)f.y - f.dirY * 0.38f, (float)r->floor + 1.9f};
                    L.color = lamp;
                    L.area = {0.5f, 0.26f};
                    L.hum = 4;
                    break;
                case Fixture::Machine:
                    L.kind = ChunkLight::Machine;
                    L.pos = {(float)f.x + f.dirX * 0.62f, (float)f.y + f.dirY * 0.62f, (float)r->floor + 1.05f};
                    L.color = {1.0f, 0.6f, 0.5f};  // V6: helles Fach hinter Glas, rotes Leuchtschild
                    L.area = {0.7f, 1.3f};
                    break;
            }
            out.lights.push_back(L);
        }
        for (const ItemSpawn& it : r->items) {
            i64 ix = ifloor(it.x), iy = ifloor(it.y);
            if (ix >= X0 && ix < X0 + CS && iy >= Y0 && iy < Y0 + CS) out.items.push_back(&it);
        }
    }

    // Netz
    Mesher m{out.mesh, bank};
    out.mesh.vertices.reserve(4096);
    out.mesh.indices.reserve(6144);
    for (int ly = 0; ly < CS; ++ly)
        for (int lx = 0; lx < CS; ++lx) meshTile(m, out, lx, ly);
}

}  // namespace lim::world
