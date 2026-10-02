// V7: Texturen der neuen Objekte - Batterie, Notizzettel, Notausgang (Tuer, Zarge, Leuchtschild).
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "render/noise.hpp"
#include "render/texture_vending.hpp"

namespace lim::gfx {

using namespace noise;

namespace {
float textCenter(float x, float y, float cx, float y0, float ch, const char* s) {
    return pixelText(x, y, cx - pixelTextWidth(ch, s) * 0.5f, y0, ch, s);
}
}  // namespace

// Monozelle: schwarzer Mantel mit kupferfarbenem Band, Schrift "1,5 V", Pluspol blank
VendSample batteryTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    // v: 0 oben (Pluspol) .. 1 unten
    if (v < 0.08f) {
        s.r = s.g = s.b = 0.72f, s.metal = 1.0f, s.rough = 0.25f;
        return s;
    }
    s.r = s.g = s.b = 0.06f, s.rough = 0.35f, s.metal = 0.2f;
    if (v < 0.36f) s.r = 0.72f, s.g = 0.42f, s.b = 0.18f, s.metal = 0.6f, s.rough = 0.3f;  // Kupferband
    // Schrift laeuft um den Mantel (u ringsum); zweimal, damit sie von jeder Seite lesbar ist
    float uu = u * 2.0f - std::floor(u * 2.0f);
    if (textCenter(uu, v, 0.5f, 0.48f, 0.16f, "1,5 V") > 0.5f) s.r = s.g = s.b = 0.85f;
    if (v > 0.7f && textCenter(uu, v, 0.5f, 0.74f, 0.09f, "ALKALINE") > 0.5f) s.r = 0.75f, s.g = 0.6f, s.b = 0.3f;
    // Abrieb an den Kanten
    if ((v < 0.12f || v > 0.95f) && value(u * 60.0f, v * 60.0f, 60, 60, 701) > 0.6f) s.r = s.g = s.b = 0.5f, s.metal = 1.0f;
    return s;
}

// Notizzettel: vergilbtes Papier, liniert, eilige Handschrift (Kritzellinien), Falz, Kaffeefleck
VendSample noteTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    float fib = value(u * 300.0f, v * 300.0f, 300, 300, 711) * 0.5f + value(u * 900.0f, v * 900.0f, 900, 900, 712) * 0.5f;
    float age = fbm(u, v, 3, 3, 713) * 0.5f + 0.5f;
    s.r = 0.86f - 0.08f * age, s.g = 0.82f - 0.1f * age, s.b = 0.68f - 0.14f * age;
    s.r *= 0.96f + 0.06f * fib, s.g *= 0.96f + 0.06f * fib, s.b *= 0.96f + 0.06f * fib;
    s.rough = 0.92f;
    s.h = 0.00003f * fib;
    // Linien (v laeuft entlang der langen Seite, Text quer)
    float line = std::fmod(v * 22.0f, 1.0f);
    if (line < 0.04f && u > 0.08f && u < 0.94f) s.r *= 0.86f, s.g *= 0.9f, s.b *= 0.98f;
    // Handschrift: unregelmaessige Wortgruppen auf den Linien
    int row = (int)std::floor(v * 22.0f);
    if (row >= 2 && row <= 19 && line > 0.35f && line < 0.85f) {
        float wordNoise = value(u * 18.0f, (float)row * 3.1f, 18, 1 << 20, 714);
        float rowLen = 0.55f + 0.38f * r01(h2(row, 7, 715));
        float stroke = std::fabs(std::sin(u * 260.0f + (float)row * 1.7f + 6.0f * value(u * 40.0f, (float)row, 40, 1 << 20, 716)));
        if (u > 0.1f && u < 0.1f + rowLen && wordNoise > 0.32f && stroke > 0.55f) s.r = 0.16f, s.g = 0.18f, s.b = 0.3f, s.rough = 0.8f;
    }
    // Falz in der Mitte
    float fold = std::fabs(v - 0.5f);
    if (fold < 0.006f) s.r *= 0.85f, s.g *= 0.85f, s.b *= 0.85f, s.h -= 0.0002f;
    // Kaffeefleck mit Rand
    float cx = u - 0.72f, cy = v - 0.78f;
    float d = std::sqrt(cx * cx + cy * cy) + 0.03f * value(u * 25.0f, v * 25.0f, 25, 25, 717);
    if (d < 0.13f) {
        float rim = d > 0.115f ? 1.0f : 0.25f;
        s.r *= 1.0f - 0.25f * rim, s.g *= 1.0f - 0.32f * rim, s.b *= 1.0f - 0.45f * rim;
    }
    return s;
}

// Notausgang-Atlas: Tuerblatt (u 0..0.5), Leuchtschild (u 0.5..1, v 0..0.25), Metall (Rest)
VendSample exitTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    if (u < 0.5f) {
        // Tuerblatt: graugruen lackiertes Stahlblech, Sicke, Aufkleber, Abrieb an Griff und Fuss
        const float X = u / 0.5f * 0.94f, Z = v * 2.06f;  // Meter (Z von oben)
        float n = value(X * 8.0f, Z * 8.0f, 1 << 20, 1 << 20, 721);
        s.r = 0.42f + 0.04f * n, s.g = 0.48f + 0.04f * n, s.b = 0.44f + 0.03f * n;
        s.rough = 0.42f + 0.1f * n;
        // umlaufende Sicke
        float e = std::min(std::min(X - 0.08f, 0.86f - X), std::min(Z - 0.1f, 1.96f - Z));
        if (std::fabs(e) < 0.01f) s.h = -0.0015f, s.r *= 0.9f, s.g *= 0.9f, s.b *= 0.9f;
        // Aufkleber "NOTAUSGANG - TUER SCHLIESST SELBSTTAETIG"
        if (X > 0.25f && X < 0.69f && Z > 0.62f && Z < 0.78f) {
            s.r = 0.15f, s.g = 0.55f, s.b = 0.3f, s.rough = 0.5f;
            if (textCenter(X, Z, 0.47f, 0.645f, 0.04f, "NOTAUSGANG") > 0.5f) s.r = s.g = s.b = 0.92f;
            if (textCenter(X, Z, 0.47f, 0.72f, 0.022f, "TUER SCHLIESST SELBST") > 0.5f) s.r = s.g = s.b = 0.88f;
        }
        // Abrieb: Griffbereich, Trittspuren unten
        float wear = std::max(std::exp(-std::pow((Z - 1.06f) / 0.08f, 2.0f)) * (X < 0.85f ? 1.0f : 0.0f),
                              Z > 1.82f ? (Z - 1.82f) / 0.24f : 0.0f);
        // abgegriffener Lack: weich etwas heller und glatter (keine harten Kanten)
        float w = wear * (0.55f + 0.45f * value(X * 22.0f, Z * 22.0f, 1 << 20, 1 << 20, 722));
        s.r += (0.6f - s.r) * 0.35f * w, s.g += (0.62f - s.g) * 0.35f * w, s.b += (0.6f - s.b) * 0.35f * w;
        s.rough *= 1.0f - 0.3f * w;
        if (Z > 1.9f) s.r *= 0.7f, s.g *= 0.7f, s.b *= 0.7f;  // Schmutz am Fuss
        return s;
    }
    if (v < 0.25f) {
        // Leuchtschild (0,56 x 0,18 m, siehe makeExitDoor): gruener Grund, weisses Fluchtweg-
        // Piktogramm (laufende Figur, Tuer) und Schrift. Gerechnet in Metern, damit nichts verzerrt.
        const float X = (u - 0.5f) / 0.5f * 0.56f, Y = v / 0.25f * 0.18f;
        s.r = 0.04f, s.g = 0.62f, s.b = 0.28f, s.rough = 0.3f;
        bool white = false;
        auto seg = [&](float ax, float ay, float bx, float by, float w) {
            float dx = bx - ax, dy = by - ay, t = std::clamp(((X - ax) * dx + (Y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
            float ex = X - (ax + dx * t), ey = Y - (ay + dy * t);
            return ex * ex + ey * ey < w * w;
        };
        // laufende Figur: Kopf, Rumpf, Arme, Beine (nach rechts zur Tuer)
        float hx = X - 0.082f, hy = Y - 0.04f;
        if (hx * hx + hy * hy < 0.014f * 0.014f) white = true;
        if (seg(0.076f, 0.062f, 0.064f, 0.106f, 0.0095f) ||                                // Rumpf
            seg(0.064f, 0.106f, 0.04f, 0.124f, 0.0075f) || seg(0.04f, 0.124f, 0.032f, 0.152f, 0.0075f) ||  // hinteres Bein
            seg(0.064f, 0.106f, 0.088f, 0.13f, 0.0075f) || seg(0.088f, 0.13f, 0.1f, 0.155f, 0.0075f) ||    // vorderes Bein
            seg(0.074f, 0.072f, 0.1f, 0.088f, 0.0065f) || seg(0.074f, 0.072f, 0.048f, 0.082f, 0.0065f))   // Arme
            white = true;
        // Tuer: Rahmen mit dunklem Durchgang
        if (X > 0.118f && X < 0.162f && Y > 0.026f && Y < 0.158f && !(X > 0.126f && X < 0.154f && Y > 0.034f)) white = true;
        // Pfeil nach rechts unter der Schrift
        if (seg(0.215f, 0.142f, 0.27f, 0.142f, 0.0045f) || seg(0.27f, 0.142f, 0.255f, 0.13f, 0.0045f) ||
            seg(0.27f, 0.142f, 0.255f, 0.154f, 0.0045f))
            white = true;
        // Schrift
        if (textCenter(X, Y, 0.362f, 0.05f, 0.04f, "NOTAUSGANG") > 0.5f) white = true;
        // dunkler Rand der Scheibe
        if (X < 0.008f || X > 0.552f || Y < 0.008f || Y > 0.172f) s.r *= 0.5f, s.g *= 0.5f, s.b *= 0.5f;
        if (white) s.r = 0.95f, s.g = 0.98f, s.b = 0.95f;
        return s;
    }
    // gebuersteter Stahl (Zarge, Stange, Gehaeuse)
    float brush = value(u * 900.0f, v * 8.0f, 900, 8, 731);
    s.r = s.g = 0.5f + 0.06f * brush, s.b = 0.52f + 0.06f * brush;
    s.metal = 0.9f, s.rough = 0.35f + 0.1f * brush;
    return s;
}

}  // namespace lim::gfx
