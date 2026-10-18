// V10: Umriss der Deathmoth-Fluegel - gemeinsam fuer das Modell (render/entity_models) und das
// Fluegelmuster (render/texture_entity). In der Ruhepose liegen die Fluegel flach ausgebreitet in der
// Ebene z = const (Modellraum, x vorn, y links). Jeder Fluegel ist von seiner Wurzel aus sternfoermig:
// zu jedem Winkel th (0 = seitlich, positiv nach vorn) gibt rmax(th) den Rand an. Die Quellen
// beschreiben zwei Fluegelpaare (Wikidot) mit unregelmaessig gezackten Raendern (Fandom); daher ist der
// Aussenrand gezackt, Vorderkante und Innenrand sind glatt.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lim::gfx::wing {

struct Def {
    float rx, ry, z;   // Wurzel (linke Seite), Ebene
    float th0, th1;    // Winkelbereich: Vorderkante .. Innenrand (th0 > th1)
    float jag0, jag1;  // gezackter Aussenrand zwischen jag1 .. jag0
    int teeth;
    float jagAmp;
};

inline const Def& def(int w) {
    static const Def d[2] = {
        {0.012f, 0.016f, 0.010f, 0.30f, -1.05f, 0.22f, -0.95f, 13, 0.0068f},  // Vorderfluegel
        {-0.012f, 0.014f, 0.006f, -0.95f, -1.55f, -0.98f, -1.50f, 7, 0.0058f},  // Hinterfluegel
    };
    return d[std::clamp(w, 0, 1)];
}

// glatter Rand (Catmull-Rom ueber Stuetzpunkte)
inline float smoothR(int w, float th) {
    static const float fa[9] = {0.30f, 0.24f, 0.12f, -0.05f, -0.30f, -0.55f, -0.78f, -0.93f, -1.05f};
    static const float fr[9] = {0.200f, 0.208f, 0.204f, 0.192f, 0.170f, 0.143f, 0.112f, 0.082f, 0.050f};
    static const float ha[7] = {-0.95f, -1.02f, -1.15f, -1.30f, -1.42f, -1.50f, -1.55f};
    static const float hr[7] = {0.100f, 0.138f, 0.152f, 0.150f, 0.135f, 0.115f, 0.090f};
    const float* A = w == 0 ? fa : ha;
    const float* R = w == 0 ? fr : hr;
    const int n = w == 0 ? 9 : 7;
    if (th >= A[0]) return R[0];
    if (th <= A[n - 1]) return R[n - 1];
    int i = 0;
    while (i + 1 < n - 1 && th < A[i + 1]) ++i;
    const float t = (th - A[i]) / (A[i + 1] - A[i]);
    const float p0 = R[std::max(0, i - 1)], p1 = R[i], p2 = R[i + 1], p3 = R[std::min(n - 1, i + 2)];
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2.0f * p1 + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

inline float hash01(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return (float)(x & 0xFFFFFF) / 16777216.0f;
}

// Zacken: unregelmaessige Saegezaehne, an den Enden des Aussenrands auslaufend
inline float jag(int w, float th) {
    const Def& d = def(w);
    if (th > d.jag0 || th < d.jag1) return 0.0f;
    const float u = (d.jag0 - th) / (d.jag0 - d.jag1);  // 0 .. 1 entlang des Aussenrands
    const float x = u * (float)d.teeth;
    const int k = (int)x;
    const float f = x - (float)k;
    const float skew = 0.35f + 0.3f * hash01((std::uint32_t)(k * 7 + w * 101 + 3));  // Spitze nicht mittig
    const float depth = 0.55f + 0.9f * hash01((std::uint32_t)(k * 13 + w * 211 + 5));
    const float tri = f < skew ? f / skew : (1.0f - f) / (1.0f - skew);  // 0 Kerbe .. 1 Spitze
    const float taper = std::min(1.0f, std::min(u, 1.0f - u) * 6.0f);
    return d.jagAmp * depth * (1.0f - tri) * taper;
}

inline float rmax(int w, float th) { return smoothR(w, th) - jag(w, th); }

// Punkt (x, |y|) in Fluegelkoordinaten: Winkel, Abstand zur Wurzel, Anteil am Rand (1 = Rand).
// false: ausserhalb des Winkelbereichs
inline bool polar(int w, float x, float y, float& th, float& r, float& rho) {
    const Def& d = def(w);
    const float dx = x - d.rx, dy = y - d.ry;
    th = std::atan2(dx, dy);
    r = std::sqrt(dx * dx + dy * dy);
    if (th > d.th0 || th < d.th1) return false;
    rho = r / std::max(1e-4f, rmax(w, th));
    return true;
}

}  // namespace lim::gfx::wing
