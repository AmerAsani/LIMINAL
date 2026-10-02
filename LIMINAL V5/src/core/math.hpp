// Vektoren, Matrizen und Rechenhilfen.
//
// Koordinaten: Die Welt liegt in der x/y-Ebene (x = Osten, y = Sueden wie auf
// der Karte), z zeigt nach oben. Ein Blickwinkel von 0 schaut entlang +x,
// positive Winkel drehen nach rechts (wie in V4).
//
// Matrizen sind spaltenweise gespeichert (vec4 c[4]) und werden als M * v
// angewendet. Das entspricht dem Standard-Packing von HLSL-Konstantenpuffern,
// die Shader rechnen mit mul(M, v).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace lim {

constexpr float kPi = std::numbers::pi_v<float>;

// --- Ganzzahl-Arithmetik wie in Python (Abrunden statt Abschneiden) ---------------
constexpr std::int64_t floorDiv(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    std::int64_t r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
}
constexpr std::int64_t floorMod(std::int64_t a, std::int64_t b) {
    std::int64_t r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
}
inline std::int64_t ifloor(double v) { return (std::int64_t)std::floor(v); }
// Python round(x): kaufmaennisch auf die naechste gerade Zahl (Banker's Rounding).
inline double pyRound(double v) { return std::nearbyint(v); }
// Python round(x, 2) fuer kleine Werte (Weltgenerierung: Vielfache von 0.6).
inline double pyRound2(double v) { return std::nearbyint(v * 100.0) / 100.0; }

template <class T>
constexpr T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
constexpr float saturate(float v) { return clamp(v, 0.0f, 1.0f); }
constexpr float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float a, float b, float x) {
    float t = saturate((x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

// --- Vektoren -----------------------------------------------------------------------
struct vec2 {
    float x = 0, y = 0;
    constexpr vec2() = default;
    constexpr vec2(float x_, float y_) : x(x_), y(y_) {}
    constexpr vec2 operator+(vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr vec2 operator-(vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr vec2 operator/(float s) const { return {x / s, y / s}; }
    vec2& operator+=(vec2 o) { x += o.x; y += o.y; return *this; }
};

struct vec3 {
    float x = 0, y = 0, z = 0;
    constexpr vec3() = default;
    constexpr vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr explicit vec3(float s) : x(s), y(s), z(s) {}
    constexpr vec3 operator+(vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr vec3 operator-(vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr vec3 operator*(vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    constexpr vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr vec3 operator-() const { return {-x, -y, -z}; }
    vec3& operator+=(vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    vec3& operator-=(vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
constexpr vec3 operator*(float s, vec3 v) { return v * s; }
constexpr float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr vec3 cross(vec3 a, vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(vec3 v) { return std::sqrt(dot(v, v)); }
inline vec3 normalize(vec3 v) {
    float l = length(v);
    return l > 1e-12f ? v / l : vec3(0, 0, 1);
}
inline vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
inline vec3 vmin(vec3 a, vec3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline vec3 vmax(vec3 a, vec3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }

struct vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    constexpr vec4() = default;
    constexpr vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr vec4(vec3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    constexpr vec4 operator+(vec4 o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr vec3 xyz() const { return {x, y, z}; }
};
constexpr float dot(vec4 a, vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

// Farben aus 0..255
constexpr vec4 rgba8(int r, int g, int b, int a = 255) { return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f}; }

// --- Matrix -------------------------------------------------------------------------
struct mat4 {
    vec4 c[4];  // Spalten

    static constexpr mat4 identity() {
        mat4 m;
        m.c[0] = {1, 0, 0, 0};
        m.c[1] = {0, 1, 0, 0};
        m.c[2] = {0, 0, 1, 0};
        m.c[3] = {0, 0, 0, 1};
        return m;
    }
    constexpr float at(int row, int col) const {
        const vec4& v = c[col];
        return row == 0 ? v.x : (row == 1 ? v.y : (row == 2 ? v.z : v.w));
    }
    constexpr vec4 row(int r) const { return {at(r, 0), at(r, 1), at(r, 2), at(r, 3)}; }
    constexpr vec4 operator*(vec4 v) const { return c[0] * v.x + c[1] * v.y + c[2] * v.z + c[3] * v.w; }
    constexpr mat4 operator*(const mat4& o) const {
        mat4 r;
        for (int i = 0; i < 4; ++i) r.c[i] = (*this) * o.c[i];
        return r;
    }
    static mat4 fromRows(vec4 r0, vec4 r1, vec4 r2, vec4 r3) {
        mat4 m;
        m.c[0] = {r0.x, r1.x, r2.x, r3.x};
        m.c[1] = {r0.y, r1.y, r2.y, r3.y};
        m.c[2] = {r0.z, r1.z, r2.z, r3.z};
        m.c[3] = {r0.w, r1.w, r2.w, r3.w};
        return m;
    }
    static mat4 translation(vec3 t) {
        mat4 m = identity();
        m.c[3] = {t.x, t.y, t.z, 1};
        return m;
    }
    static mat4 scale(vec3 s) {
        mat4 m = identity();
        m.c[0].x = s.x;
        m.c[1].y = s.y;
        m.c[2].z = s.z;
        return m;
    }
    static mat4 rotationZ(float a) {
        float cs = std::cos(a), sn = std::sin(a);
        mat4 m = identity();
        m.c[0] = {cs, sn, 0, 0};
        m.c[1] = {-sn, cs, 0, 0};
        return m;
    }
};

mat4 inverse(const mat4& m);

// Blickrichtung aus Drehwinkel (yaw) und Neigung (pitch), siehe Kopfkommentar.
inline vec3 forwardFromAngles(float yaw, float pitch) {
    return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}

// Kamera-Sicht: x rechts, y oben, z vorwaerts (linkshaendig wie Direct3D).
inline mat4 viewMatrix(vec3 eye, float yaw, float pitch) {
    vec3 f = forwardFromAngles(yaw, pitch);
    vec3 r{-std::sin(yaw), std::cos(yaw), 0.0f};
    vec3 u{-std::sin(pitch) * std::cos(yaw), -std::sin(pitch) * std::sin(yaw), std::cos(pitch)};
    return mat4::fromRows({r, -dot(r, eye)}, {u, -dot(u, eye)}, {f, -dot(f, eye)}, {0, 0, 0, 1});
}

// Unendliche Perspektive mit umgekehrtem Tiefenpuffer (nah = 1, fern = 0):
// hohe Genauigkeit ueber die gesamte Sichtweite mit einem 32-Bit-Float-Puffer.
inline mat4 perspectiveReversedInfinite(float fovY, float aspect, float zNear, float jitterX = 0, float jitterY = 0) {
    float sy = 1.0f / std::tan(fovY * 0.5f);
    float sx = sy / aspect;
    return mat4::fromRows({sx, 0, jitterX, 0}, {0, sy, jitterY, 0}, {0, 0, 0, zNear}, {0, 0, 1, 0});
}

// Achsenparallele Box
struct Aabb {
    vec3 lo{1e30f, 1e30f, 1e30f};
    vec3 hi{-1e30f, -1e30f, -1e30f};
    void add(vec3 p) {
        lo = vmin(lo, p);
        hi = vmax(hi, p);
    }
    bool valid() const { return lo.x <= hi.x; }
};

// Sichtpyramide aus einer View-Projection-Matrix (Ebenen nach innen gerichtet).
struct Frustum {
    vec4 planes[5];  // links, rechts, unten, oben, nah (fern ist unendlich)
    static Frustum fromViewProj(const mat4& vp);
    bool intersects(const Aabb& box) const;
    bool intersectsSphere(vec3 c, float r) const;
};

}  // namespace lim
