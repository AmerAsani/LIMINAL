// V10: Texturen der Figuren. Alle nahtlos kachelbar und in realer Groesse (siehe kLayers):
// Haut (Poren, Flecken, feine Linien), Wolle (Koeper, Flusen), Baumwolle (Leinwandbindung),
// Anzugstoff (feiner Koeper, leichter Glanz), Denim (blauer Koeper mit hellem Schuss), Leder
// (Narbung, Knicke), Haar (Straehnen) und ein Atlas fuer Ausweis und Augen.
// Die Grundfarbe ist hell und neutral - das Material toent (Hautton, Stofffarbe).
#include <algorithm>
#include <cmath>

#include "render/noise.hpp"
#include "render/texture_vending.hpp"

namespace lim::gfx {

using namespace noise;

namespace {
float sat(float x) { return std::clamp(x, 0.0f, 1.0f); }
float sm(float a, float b, float x) {
    float t = sat((x - a) / (b - a));
    return t * t * (3 - 2 * t);
}
float textAt(float x, float y, float x0, float y0, float ch, const char* s) { return pixelText(x, y, x0, y0, ch, s); }
}  // namespace

// Haut: 10 cm je Kachel
VendSample skinTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    float mott = fbm(u, v, 3, 4, 1001) * 0.5f + 0.5f;           // grossere Flecken
    float red = sm(0.55f, 0.85f, fbm(u, v, 5, 3, 1002) * 0.5f + 0.5f);
    float pore = worley(u, v, 150, 1003).d1;                      // Poren (<1 mm)
    float pores = 1.0f - sm(0.0f, 0.22f, pore);
    float lines = std::fabs(fbm(u * 1.0f, v, 14, 3, 1004));       // feine Hautlinien
    float line = 1.0f - sm(0.0f, 0.05f, lines);
    float fine = value(u * 400.0f, v * 400.0f, 400, 400, 1005);
    // Grundton (sRGB) hell und neutral-warm; Toenung kommt vom Material
    s.r = 0.86f + 0.05f * (mott - 0.5f) + 0.04f * red;
    s.g = 0.74f + 0.05f * (mott - 0.5f) - 0.03f * red;
    s.b = 0.68f + 0.04f * (mott - 0.5f) - 0.02f * red;
    float dark = 1.0f - 0.025f * pores - 0.03f * line + 0.015f * (fine - 0.5f);
    s.r *= dark, s.g *= dark, s.b *= dark;
    s.h = -0.000018f * pores - 0.00002f * line + 0.00001f * fine;
    s.rough = 0.55f + 0.18f * pores + 0.1f * (mott - 0.5f);
    s.ao = 1.0f - 0.15f * pores;
    return s;
}

// Wolle (Mantel, Kapuzenjacke): Koeperbindung diagonal, Flusen, 18 cm je Kachel
VendSample woolTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    const float n = 220.0f;
    float diag = std::sin((u + v) * n * 3.14159f);              // Koepergrat
    float twill = 0.5f + 0.5f * diag;
    float yarn = value(u * 700.0f, v * 700.0f, 700, 700, 1101);
    float fuzz = fbm(u, v, 40, 3, 1102) * 0.5f + 0.5f;
    float pill = 1.0f - sm(0.0f, 0.08f, worley(u, v, 60, 1103).d1);
    // Koepergrat nur schwach: auf Abstand flimmert ein feines Muster sonst wie ein Kettenhemd
    float g = 0.76f + 0.035f * twill + 0.07f * (yarn - 0.5f) + 0.07f * (fuzz - 0.5f) + 0.05f * pill;
    s.r = s.g = s.b = g;
    s.h = 0.00012f * twill + 0.00012f * yarn + 0.0002f * pill;
    s.rough = 0.95f;
    s.ao = 0.82f + 0.18f * twill;
    return s;
}

// Baumwolle (Hemd, Nachthemd, Lumpen): Leinwandbindung, Noppen, 6 cm je Kachel
VendSample cottonTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    const float n = 140.0f;
    float wu = std::sin(u * n * 6.2831853f), wv = std::sin(v * n * 6.2831853f);
    float weave = 0.5f + 0.25f * wu * (wv > 0 ? 1.0f : -1.0f) + 0.25f * wv * (wu > 0 ? -1.0f : 1.0f);
    float slub = fbm(u * 1.0f, v, 30, 2, 1201) * 0.5f + 0.5f;
    float fib = value(u * 600.0f, v * 600.0f, 600, 600, 1202);
    float g = 0.86f + 0.06f * weave + 0.05f * (slub - 0.5f) + 0.03f * (fib - 0.5f);
    s.r = s.g = s.b = g;
    s.h = 0.00012f * weave + 0.00008f * slub;
    s.rough = 0.88f;
    s.ao = 0.9f + 0.1f * weave;
    return s;
}

// Anzugstoff (Hose, Krawatte): feiner Koeper, 8 cm je Kachel, leichter Glanz
VendSample worstedTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    float diag = std::sin((u * 2.0f + v) * 260.0f * 3.14159f);
    float fib = value(u * 800.0f, v * 800.0f, 800, 800, 1301);
    float mel = fbm(u, v, 20, 2, 1302) * 0.5f + 0.5f;
    float g = 0.8f + 0.05f * diag + 0.05f * (fib - 0.5f) + 0.06f * (mel - 0.5f);
    s.r = s.g = s.b = g;
    s.h = 0.00008f * diag;
    s.rough = 0.66f + 0.08f * fib;
    return s;
}

// Denim: Koeper mit blauer Kette und hellem Schuss, Waschung; Toenung blau durch das Material
VendSample denimTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    float diag = std::sin((u * 3.0f - v) * 240.0f * 3.14159f);
    float weft = value(u * 900.0f, v * 300.0f, 900, 300, 1401);
    float wash = fbm(u, v, 4, 4, 1402) * 0.5f + 0.5f;
    float slub = value(u * 6.0f, v * 160.0f, 6, 160, 1403);
    float g = 0.58f + 0.12f * (0.5f + 0.5f * diag) + 0.25f * sm(0.65f, 0.95f, weft) + 0.15f * wash + 0.06f * slub;
    s.r = s.g = s.b = sat(g);
    s.h = 0.00015f * diag;
    s.rough = 0.85f;
    return s;
}

// Leder (Schuhe, Guertel, Knoepfe): Narbung, Knicke, abgestossene Stellen
VendSample leatherTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    Cell c = worley(u, v, 70, 1501);
    float grain = sm(0.0f, 0.4f, c.d2 - c.d1);
    float crease = 1.0f - sm(0.0f, 0.03f, std::fabs(fbm(u, v, 6, 3, 1502)));
    float scuff = sm(0.6f, 0.9f, fbm(u, v, 9, 3, 1503) * 0.5f + 0.5f);
    float g = 0.82f + 0.08f * grain - 0.12f * crease + 0.12f * scuff;
    s.r = s.g = s.b = g;
    s.h = 0.0001f * grain - 0.00025f * crease;
    s.rough = 0.38f + 0.18f * (1.0f - grain) + 0.25f * scuff + 0.15f * crease;
    return s;
}

// Haar: Straehnen laengs (v), 5 cm je Kachel
VendSample hairTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    float strand = value(u * 260.0f, v * 6.0f, 260, 6, 1601);
    float clump = value(u * 30.0f, v * 2.0f, 30, 2, 1602);
    float fine = value(u * 900.0f, v * 20.0f, 900, 20, 1603);
    float g = 0.55f + 0.25f * strand + 0.12f * clump + 0.08f * fine;
    s.r = s.g = s.b = g;
    s.h = 0.0003f * strand + 0.0002f * clump;
    s.rough = 0.5f + 0.15f * (1.0f - strand);
    s.ao = 0.75f + 0.25f * clump;
    return s;
}

// Atlas: Ausweis (u 0.5..1, v 0..0.5), Auge (u 0..0.25, v 0.75..1), sonst heller Kunststoff
VendSample figDetailTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    s.r = s.g = s.b = 0.86f, s.rough = 0.4f;
    if (u >= 0.5f && v < 0.5f) {
        // Ausweiskarte, hochkant: X 0..1 quer, Y 0..1 laengs (oben das Band-Loch)
        const float X = (u - 0.5f) * 2.0f, Y = v * 2.0f;
        s.r = 0.93f, s.g = 0.93f, s.b = 0.9f, s.rough = 0.3f;
        if (Y > 0.1f && Y < 0.26f) s.r = 0.16f, s.g = 0.3f, s.b = 0.52f;  // Kopfband
        if (Y > 0.06f && Y < 0.09f && std::fabs(X - 0.5f) < 0.12f) s.r = s.g = s.b = 0.2f;  // Schlitz
        if (textAt(X, Y, 0.14f, 0.15f, 0.07f, "MITARBEITER") > 0.5f) s.r = s.g = s.b = 0.95f;
        // Foto: grauer Kasten mit verwaschener Kopfform
        if (X > 0.22f && X < 0.78f && Y > 0.32f && Y < 0.7f) {
            float hx = (X - 0.5f) / 0.13f, hy = (Y - 0.46f) / 0.11f;
            float headM = hx * hx + hy * hy < 1.0f ? 1.0f : 0.0f;
            float sh = (Y > 0.58f && std::fabs(X - 0.5f) < 0.24f - (0.7f - Y) * 0.4f) ? 1.0f : 0.0f;
            float g = 0.62f - 0.3f * std::max(headM, sh) + 0.05f * value(X * 40.0f, Y * 40.0f, 1 << 20, 1 << 20, 1701);
            s.r = s.g = s.b = g;
            s.rough = 0.25f;
        }
        if (textAt(X, Y, 0.1f, 0.75f, 0.06f, "NR 0000-") > 0.5f) s.r = s.g = s.b = 0.15f;
        if (textAt(X, Y, 0.1f, 0.85f, 0.05f, "EBENE 0") > 0.5f) s.r = s.g = s.b = 0.3f;
        // Abnutzung an den Kanten
        float e = std::min(std::min(X, 1.0f - X), std::min(Y, 1.0f - Y));
        if (e < 0.03f) s.r *= 0.8f, s.g *= 0.8f, s.b *= 0.78f;
        return s;
    }
    if (u < 0.25f && v >= 0.75f) {
        // Auge: Pupille, Iris (dunkel, radial gemasert), Limbus, Lederhaut mit Aederchen
        const float dx = (u - 0.125f) / 0.12f, dy = (v - 0.875f) / 0.12f;
        const float r = std::sqrt(dx * dx + dy * dy), a = std::atan2(dy, dx);
        if (r < 0.32f) {
            s.r = s.g = s.b = 0.015f;
        } else if (r < 0.7f) {
            float rad = value(a * 30.0f, r * 4.0f, 1 << 20, 1 << 20, 1711);
            float t = (r - 0.32f) / 0.38f;
            s.r = 0.2f + 0.1f * rad - 0.06f * t, s.g = 0.17f + 0.08f * rad - 0.05f * t, s.b = 0.13f + 0.06f * rad;
        } else if (r < 0.78f) {
            s.r = 0.08f, s.g = 0.07f, s.b = 0.06f;
        } else {
            float vein = 1.0f - sm(0.0f, 0.04f, std::fabs(fbm(u * 3.0f, v * 3.0f, 8, 3, 1712)));
            s.r = 0.84f, s.g = 0.79f - 0.15f * vein, s.b = 0.74f - 0.15f * vein;
        }
        s.rough = 0.08f;
        return s;
    }
    return s;
}

}  // namespace lim::gfx
