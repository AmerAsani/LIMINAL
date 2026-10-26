// V9: Atlas des Sicherungskastens und der Sicherung (Ebene L_FUSE).
//
//   u 0..0.5,  v 0..0.5   Gehaeuse: hellgrau lackiertes Stahlblech, Kratzer, Rost an den Kanten
//   u 0.5..1,  v 0..0.5   Warnschild: gelbes Dreieck mit Blitz, "VORSICHT STARKSTROM"
//   u 0..0.5,  v 0.5..1   Montageplatte innen: verzinktes Blech, Beschriftung; Ecke unten rechts:
//                         schwarzes Bakelit (Halter)
//   u 0.5..0.75, v 0.5..1 Porzellankoerper der Sicherung (weiss, Aufdruck)
//   u 0.75..1, v 0.5..1   Messingkappen
#include <algorithm>
#include <cmath>

#include "render/noise.hpp"
#include "render/texture_vending.hpp"

namespace lim::gfx {

using namespace noise;

namespace {
float textAt(float x, float y, float cx, float y0, float ch, const char* s) {
    return pixelText(x, y, cx - pixelTextWidth(ch, s) * 0.5f, y0, ch, s);
}
}  // namespace

VendSample fuseTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    if (v < 0.5f && u < 0.5f) {
        // Gehaeuse (1 Einheit = 1 m: der Bereich deckt 1 x 1 m)
        const float X = u * 2.0f, Y = v * 2.0f;
        float n = value(X * 9.0f, Y * 9.0f, 1 << 20, 1 << 20, 901) * 0.6f + value(X * 40.0f, Y * 40.0f, 1 << 20, 1 << 20, 902) * 0.4f;
        s.r = 0.62f + 0.05f * n, s.g = 0.62f + 0.05f * n, s.b = 0.58f + 0.04f * n;
        s.rough = 0.48f + 0.12f * n;
        s.h = 0.00003f * n;
        // feine Kratzer
        float sc = std::fabs(std::sin(X * 140.0f + 9.0f * value(X * 5.0f, Y * 60.0f, 1 << 20, 1 << 20, 903)));
        if (sc > 0.995f && value(X * 3.0f, Y * 3.0f, 1 << 20, 1 << 20, 904) > 0.55f) s.r = s.g = 0.78f, s.b = 0.74f, s.rough = 0.35f;
        // Rost- und Schmutzflecken
        float rust = value(X * 7.0f, Y * 7.0f, 1 << 20, 1 << 20, 905) * 0.7f + value(X * 30.0f, Y * 30.0f, 1 << 20, 1 << 20, 906) * 0.3f;
        if (rust > 0.74f) {
            float k = std::min(1.0f, (rust - 0.74f) / 0.12f);
            s.r += (0.42f - s.r) * k, s.g += (0.24f - s.g) * k, s.b += (0.12f - s.b) * k;
            s.rough = 0.85f, s.h -= 0.00008f * k;
        }
        float grime = value(X * 3.0f, Y * 3.0f, 1 << 20, 1 << 20, 907);
        s.r *= 0.9f + 0.1f * grime, s.g *= 0.9f + 0.1f * grime, s.b *= 0.88f + 0.1f * grime;
        return s;
    }
    if (v < 0.5f) {
        // Warnschild (0,5 x 0,5 m Bereich): gelbes Schild, oben Dreieck mit Blitz, unten Schrift
        const float X = (u - 0.5f) * 2.0f, Y = v * 2.0f;  // 0..1
        s.r = 0.95f, s.g = 0.78f, s.b = 0.08f, s.rough = 0.4f;
        // Dreieck: Spitze oben (0.5, 0.06), Basis bei 0.62
        const float ty = (Y - 0.06f) / 0.56f, half = 0.36f * ty;
        const bool inTri = ty >= 0.0f && ty <= 1.0f && std::fabs(X - 0.5f) <= half;
        const bool inInner = Y >= 0.15f && Y <= 0.58f && std::fabs(X - 0.5f) <= 0.643f * (Y - 0.15f);  // gleiche Steigung
        if (inTri && !inInner) s.r = s.g = s.b = 0.03f;
        // Blitz: drei Segmente
        auto seg = [&](float ax, float ay, float bx, float by, float w) {
            float dx = bx - ax, dy = by - ay, t = std::clamp(((X - ax) * dx + (Y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
            float ex = X - (ax + dx * t), ey = Y - (ay + dy * t);
            return ex * ex + ey * ey < w * w;
        };
        if (seg(0.55f, 0.24f, 0.45f, 0.40f, 0.03f) || seg(0.45f, 0.40f, 0.56f, 0.40f, 0.03f) || seg(0.56f, 0.40f, 0.46f, 0.55f, 0.03f))
            s.r = s.g = s.b = 0.03f;
        if (textAt(X, Y, 0.5f, 0.68f, 0.09f, "VORSICHT") > 0.5f) s.r = s.g = s.b = 0.03f;
        if (textAt(X, Y, 0.5f, 0.82f, 0.09f, "STARKSTROM") > 0.5f) s.r = s.g = s.b = 0.03f;
        if (X < 0.025f || X > 0.975f || Y < 0.025f || Y > 0.975f) s.r = s.g = s.b = 0.05f;  // Rand
        // vergilbt, leicht verschmutzt
        float age = value(X * 6.0f, Y * 6.0f, 1 << 20, 1 << 20, 911);
        s.r *= 0.9f + 0.1f * age, s.g *= 0.86f + 0.1f * age, s.b *= 0.8f + 0.2f * age;
        return s;
    }
    if (u < 0.5f) {
        const float X = u * 2.0f, Y = (v - 0.5f) * 2.0f;  // 0..1
        if (X > 0.9f && Y > 0.9f) {  // Bakelit
            s.r = s.g = s.b = 0.035f, s.rough = 0.35f;
            return s;
        }
        // verzinktes Blech mit Zinkblumen
        float sp = value(X * 60.0f, Y * 60.0f, 1 << 20, 1 << 20, 921);
        float cell = value(X * 14.0f, Y * 14.0f, 1 << 20, 1 << 20, 922);
        s.r = s.g = 0.52f + 0.1f * cell + 0.04f * sp, s.b = 0.54f + 0.1f * cell + 0.04f * sp;
        s.metal = 0.85f, s.rough = 0.4f + 0.15f * sp;
        // Beschriftung und Hutschiene
        if (textAt(X, Y, 0.5f, 0.08f, 0.07f, "HAUPTSICHERUNG") > 0.5f) s.r = s.g = s.b = 0.08f, s.metal = 0.0f;
        if (textAt(X, Y, 0.47f, 0.8f, 0.06f, "NICHT ENTFERNEN") > 0.5f) s.r = 0.6f, s.g = 0.08f, s.b = 0.06f, s.metal = 0.0f;
        if (Y > 0.2f && Y < 0.24f) s.r = s.g = s.b = 0.72f, s.metal = 1.0f, s.rough = 0.25f;
        return s;
    }
    if (u < 0.75f) {
        // Porzellan (u ringsum, v entlang der Sicherung)
        const float X = (u - 0.5f) * 4.0f, Y = (v - 0.5f) * 2.0f;
        float n = value(X * 30.0f, Y * 30.0f, 1 << 20, 1 << 20, 931);
        s.r = 0.9f + 0.03f * n, s.g = 0.88f + 0.03f * n, s.b = 0.82f + 0.03f * n;
        s.rough = 0.22f;
        float xx = X * 2.0f - std::floor(X * 2.0f);  // Aufdruck zweimal ringsum
        if (textAt(xx, Y, 0.5f, 0.3f, 0.16f, "63A") > 0.5f) s.r = 0.1f, s.g = 0.12f, s.b = 0.3f, s.rough = 0.5f;
        if (textAt(xx, Y, 0.5f, 0.56f, 0.09f, "500V") > 0.5f) s.r = 0.1f, s.g = 0.12f, s.b = 0.3f, s.rough = 0.5f;
        if (Y > 0.78f && Y < 0.86f && xx > 0.42f && xx < 0.58f) s.r = 0.75f, s.g = 0.1f, s.b = 0.08f;  // Kennmelder
        return s;
    }
    // Messing
    const float Y = (v - 0.5f) * 2.0f;
    float br = value(u * 400.0f, Y * 10.0f, 1 << 20, 1 << 20, 941);
    s.r = 0.78f + 0.06f * br, s.g = 0.6f + 0.05f * br, s.b = 0.3f + 0.04f * br;
    s.metal = 1.0f, s.rough = 0.28f + 0.12f * br;
    if (value(u * 50.0f, Y * 50.0f, 1 << 20, 1 << 20, 942) > 0.72f) s.r *= 0.7f, s.g *= 0.72f, s.b *= 0.6f, s.rough = 0.6f;  // angelaufen
    return s;
}

}  // namespace lim::gfx
