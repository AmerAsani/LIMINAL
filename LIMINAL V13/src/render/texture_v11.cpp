// V11: Texturen der Ausstattung.
//
//   L_SIGNS      Atlas 8 x 8 Zellen (props/prop_library.hpp):
//                  0-25 A-Z, 26-35 0-9, 36 '-'  (Schablonenschrift: Strich weiss, Grund schwarz; der
//                  Shader schneidet den Grund weg -> aufgespruehte Schrift direkt auf der Flaeche)
//                  38 Notausgang (eigene Bildzeichen: Figur laeuft zur Tuer), 39 Notausgang mit Pfeil
//                  41 Warnschild, 42 Durchfahrt verboten, 43 Feuerloescher
//                  48-55 Bilder (eigene, prozedurale Motive), 56-59 Zettel
//   L_PAINTED    lackiertes Blech: feine Orangenhaut, Abplatzer mit Rostrand, Kratzer
//   L_ROADPAINT  Fahrbahnfarbe: Abrieb in Fahrtrichtung, Reifenspuren, Beton scheint durch
//   L_RUBBER     Gummi: fein genarbt, stumpf
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "core/core.hpp"

#include "render/noise.hpp"
#include "render/texture_vending.hpp"

namespace lim::gfx {

using namespace noise;

namespace {

struct Seg {
    float x0, y0, x1, y1;
};

// Schablonenschrift auf einem 4 x 6 Raster (y nach oben); Polylinien je Zeichen
const char* const kGlyphs[37] = {
    "0,0 2,6 4,0;0.8,2.2 3.2,2.2",                                                     // A
    "0,0 0,6 3,6 4,5 3,3.2 0,3.2;3,3.2 4,2 4,1 3,0 0,0",                               // B
    "4,5.2 3,6 1,6 0,5 0,1 1,0 3,0 4,0.8",                                             // C
    "0,0 0,6 2.5,6 4,4.5 4,1.5 2.5,0 0,0",                                             // D
    "4,6 0,6 0,0 4,0;0,3 3,3",                                                         // E
    "4,6 0,6 0,0;0,3 3,3",                                                             // F
    "4,5.2 3,6 1,6 0,5 0,1 1,0 3,0 4,1 4,2.8 2.2,2.8",                                 // G
    "0,0 0,6;4,0 4,6;0,3 4,3",                                                         // H
    "2,0 2,6;1,0 3,0;1,6 3,6",                                                         // I
    "4,6 4,1 3,0 1,0 0,1",                                                             // J
    "0,0 0,6;4,6 0,2.5;1.3,3.6 4,0",                                                   // K
    "0,6 0,0 4,0",                                                                     // L
    "0,0 0,6 2,3 4,6 4,0",                                                             // M
    "0,0 0,6 4,0 4,6",                                                                 // N
    "1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,1 1,0",                                             // O
    "0,0 0,6 3,6 4,5 4,4 3,3 0,3",                                                     // P
    "1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,1 1,0;2.5,1.5 4.2,-0.2",                            // Q
    "0,0 0,6 3,6 4,5 4,4 3,3 0,3;2,3 4,0",                                             // R
    "4,5.2 3,6 1,6 0,5 0,4 1,3 3,3 4,2 4,1 3,0 1,0 0,0.8",                             // S
    "0,6 4,6;2,6 2,0",                                                                 // T
    "0,6 0,1 1,0 3,0 4,1 4,6",                                                         // U
    "0,6 2,0 4,6",                                                                     // V
    "0,6 1,0 2,3.5 3,0 4,6",                                                           // W
    "0,0 4,6;0,6 4,0",                                                                 // X
    "0,6 2,3 4,6;2,3 2,0",                                                             // Y
    "0,6 4,6 0,0 4,0",                                                                 // Z
    "1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,1 1,0;0.8,1.2 3.2,4.8",                             // 0
    "1,4.8 2.2,6 2.2,0;1,0 3.4,0",                                                     // 1
    "0,5 1,6 3,6 4,5 4,4 0,0 4,0",                                                     // 2
    "0,5.2 1,6 3,6 4,5 4,4 3,3 1.5,3;3,3 4,2 4,1 3,0 1,0 0,0.8",                       // 3
    "3,0 3,6 0,1.8 4,1.8",                                                             // 4
    "4,6 0.3,6 0,3.4 3,3.6 4,2.5 4,1 3,0 1,0 0,0.8",                                   // 5
    "3.6,6 2,6 0,4 0,1 1,0 3,0 4,1 4,2.5 3,3.4 1,3.4 0,2.5",                           // 6
    "0,6 4,6 1.5,0",                                                                   // 7
    "1,3 0,4 0,5 1,6 3,6 4,5 4,4 3,3 1,3 0,2 0,1 1,0 3,0 4,1 4,2 3,3",                 // 8
    "0.4,0 2,0 4,2 4,5 3,6 1,6 0,5 0,3.5 1,2.6 3,2.6 4,3.5",                           // 9
    "0.8,3 3.2,3",                                                                     // -
};

std::vector<Seg> parse(const char* s) {
    std::vector<Seg> out;
    float px = 0, py = 0;
    bool have = false;
    const char* p = s;
    while (*p) {
        if (*p == ';') {
            have = false;
            ++p;
            continue;
        }
        if (*p == ' ') {
            ++p;
            continue;
        }
        char* e;
        float x = std::strtof(p, &e);
        p = e + 1;  // ','
        float y = std::strtof(p, &e);
        p = e;
        if (have) out.push_back({px, py, x, y});
        px = x, py = y, have = true;
    }
    return out;
}

const std::vector<Seg>& glyph(int i) {
    // threadsicher: die Texturen entstehen parallel
    static const std::vector<std::vector<Seg>> cache = [] {
        std::vector<std::vector<Seg>> c;
        for (int k = 0; k < 37; ++k) c.push_back(parse(kGlyphs[k]));
        return c;
    }();
    return cache[(size_t)i];
}

float segDist(float x, float y, const Seg& s) {
    float dx = s.x1 - s.x0, dy = s.y1 - s.y0;
    float t = std::clamp(((x - s.x0) * dx + (y - s.y0) * dy) / std::max(1e-6f, dx * dx + dy * dy), 0.0f, 1.0f);
    float qx = s.x0 + dx * t - x, qy = s.y0 + dy * t - y;
    return std::sqrt(qx * qx + qy * qy);
}

// Deckung eines Zeichens in Zellkoordinaten (0..1), Strichbreite in Rastereinheiten
float glyphCover(int i, float cu, float cv, float px) {
    // Zeichen 4 x 6 Einheiten, mittig in der Zelle mit Rand
    const float scale = 6.0f / 0.78f;  // Einheiten je Zelle (vertikal)
    float x = (cu - 0.5f) * scale + 2.0f, y = (1.0f - cv - 0.5f) * scale + 3.0f;
    float d = 1e9f;
    for (const Seg& s : glyph(i)) d = std::min(d, segDist(x, y, s));
    const float hw = 0.52f, aa = px * scale * 8.0f * 0.75f;
    return 1.0f - smooth(hw - aa, hw + aa, d);
}

float circle(float x, float y, float cx, float cy, float r, float aa) { return 1.0f - smooth(r - aa, r + aa, std::hypot(x - cx, y - cy)); }
float capsule(float x, float y, float ax, float ay, float bx, float by, float r, float aa) {
    return 1.0f - smooth(r - aa, r + aa, segDist(x, y, {ax, ay, bx, by}));
}
float rectMask(float x, float y, float x0, float y0, float x1, float y1, float aa) {
    float dx = std::max(x0 - x, x - x1), dy = std::max(y0 - y, y - y1);
    return 1.0f - smooth(-aa, aa, std::max(dx, dy));
}

// eigenes Notausgang-Bildzeichen: Figur im Lauf zur offenen Tuer (gruen/weiss)
VendSample exitPictogram(float x, float y, float px, bool arrow) {
    VendSample s;
    const float aa = px * 8.0f * 1.2f;
    float white = 0.0f;
    // Rahmen der Tuer rechts
    float door = rectMask(x, y, 0.62f, 0.14f, 0.86f, 0.86f, aa) - rectMask(x, y, 0.66f, 0.18f, 0.82f, 0.86f, aa);
    white = std::max(white, door);
    // Figur: Kopf, Rumpf schraeg, Arme und Beine im Schritt
    white = std::max(white, circle(x, y, 0.47f, 0.76f, 0.06f, aa));
    white = std::max(white, capsule(x, y, 0.45f, 0.66f, 0.38f, 0.44f, 0.045f, aa));
    white = std::max(white, capsule(x, y, 0.44f, 0.62f, 0.56f, 0.55f, 0.03f, aa));
    white = std::max(white, capsule(x, y, 0.44f, 0.62f, 0.31f, 0.56f, 0.03f, aa));
    white = std::max(white, capsule(x, y, 0.38f, 0.44f, 0.5f, 0.3f, 0.035f, aa));
    white = std::max(white, capsule(x, y, 0.5f, 0.3f, 0.48f, 0.16f, 0.033f, aa));
    white = std::max(white, capsule(x, y, 0.38f, 0.44f, 0.27f, 0.32f, 0.035f, aa));
    white = std::max(white, capsule(x, y, 0.27f, 0.32f, 0.17f, 0.3f, 0.033f, aa));
    if (arrow) {
        white = std::max(white, capsule(x, y, 0.08f, 0.88f, 0.3f, 0.88f, 0.025f, aa));
        white = std::max(white, capsule(x, y, 0.08f, 0.88f, 0.15f, 0.93f, 0.025f, aa));
        white = std::max(white, capsule(x, y, 0.08f, 0.88f, 0.15f, 0.83f, 0.025f, aa));
    }
    const float n = value(x * 40.0f, y * 40.0f, 1 << 20, 1 << 20, 3301) * 0.06f;
    s.r = mix(0.02f, 0.95f, white) + n * 0.2f;
    s.g = mix(0.52f, 0.97f, white) + n;
    s.b = mix(0.26f, 0.94f, white) + n * 0.4f;
    s.rough = 0.35f;
    return s;
}

// Bilder: eigene, stille Motive mit Pinselstruktur (nichts Bekanntes nachgemalt)
VendSample paintingCell(int k, float x, float y, float px) {
    VendSample s;
    (void)px;
    const float brush = value(x * 30.0f + y * 3.0f, y * 90.0f, 1 << 20, 1 << 20, 3400 + (u32)k) * 0.5f +
                        value(x * 70.0f, y * 20.0f, 1 << 20, 1 << 20, 3410 + (u32)k) * 0.5f;
    float r, g, b;
    const float yy = 1.0f - y;  // oben = 1
    switch (k) {
        case 0: {  // leerer Flur in Fluchtperspektive, eine Tuer am Ende
            float d = std::fabs(x - 0.5f) * 2.0f;
            float floorL = yy < 0.4f - 0.25f * (1.0f - d) ? 1.0f : 0.0f;
            r = mix(0.62f, 0.36f, floorL), g = mix(0.56f, 0.3f, floorL), b = mix(0.32f, 0.2f, floorL);
            float door = rectMask(x, yy, 0.47f, 0.4f, 0.53f, 0.55f, 0.004f);
            r = mix(r, 0.12f, door), g = mix(g, 0.1f, door), b = mix(b, 0.08f, door);
            break;
        }
        case 1: {  // Feld unter niedrigem Himmel, ein einzelner Pfahl
            float hor = smooth(0.38f, 0.42f, yy);
            r = mix(0.52f, 0.62f, hor), g = mix(0.46f, 0.63f, hor), b = mix(0.25f, 0.6f, hor);
            float pole = rectMask(x, yy, 0.69f, 0.36f, 0.705f, 0.56f, 0.003f);
            r = mix(r, 0.1f, pole), g = mix(g, 0.1f, pole), b = mix(b, 0.09f, pole);
            break;
        }
        case 2: {  // ruhiges Meer, graues Licht
            float hor = smooth(0.48f, 0.5f, yy);
            float wave = 0.03f * std::sin(x * 90.0f + yy * 40.0f) * (1.0f - hor);
            r = mix(0.16f, 0.62f, hor) + wave, g = mix(0.22f, 0.64f, hor) + wave, b = mix(0.27f, 0.66f, hor) + wave;
            break;
        }
        case 3: {  // ein leerer Stuhl vor einer Wand
            r = 0.55f, g = 0.5f, b = 0.42f;
            float ch = rectMask(x, yy, 0.42f, 0.2f, 0.58f, 0.26f, 0.003f) + rectMask(x, yy, 0.55f, 0.26f, 0.58f, 0.48f, 0.003f) +
                       rectMask(x, yy, 0.42f, 0.05f, 0.44f, 0.2f, 0.003f) + rectMask(x, yy, 0.56f, 0.05f, 0.58f, 0.2f, 0.003f);
            ch = std::min(1.0f, ch);
            r = mix(r, 0.2f, ch), g = mix(g, 0.14f, ch), b = mix(b, 0.1f, ch);
            float floorL = smooth(0.12f, 0.1f, yy);
            r = mix(r, 0.32f, floorL), g = mix(g, 0.28f, floorL), b = mix(b, 0.22f, floorL);
            break;
        }
        case 4: {  // Fenster bei Nacht: nur ein helles Rechteck im Dunkel
            r = 0.05f, g = 0.06f, b = 0.09f;
            float win = rectMask(x, yy, 0.38f, 0.42f, 0.62f, 0.72f, 0.004f);
            r = mix(r, 0.78f, win), g = mix(g, 0.7f, win), b = mix(b, 0.48f, win);
            break;
        }
        case 5: {  // Treppenhaus von oben, Spirale aus Gelaendern
            float a = std::atan2(yy - 0.5f, x - 0.5f), rr = std::hypot(x - 0.5f, yy - 0.5f);
            float spiral = 0.5f + 0.5f * std::sin(rr * 60.0f - a * 3.0f);
            r = 0.2f + 0.3f * spiral * (1.0f - rr), g = 0.22f + 0.28f * spiral * (1.0f - rr), b = 0.2f + 0.2f * spiral * (1.0f - rr);
            break;
        }
        case 6: {  // Portraet ohne Gesicht: Kopf und Schultern, Gesicht verwischt
            r = 0.3f, g = 0.24f, b = 0.18f;
            float head = circle(x, yy, 0.5f, 0.62f, 0.14f, 0.01f);
            float body = std::min(1.0f, rectMask(x, yy, 0.25f, 0.0f, 0.75f, 0.42f, 0.02f) + circle(x, yy, 0.5f, 0.42f, 0.25f, 0.02f));
            float f = std::max(head, body);
            r = mix(r, 0.62f, head) * (1.0f - 0.6f * body * (1.0f - head)), g = mix(g, 0.52f, head) * (1.0f - 0.6f * body * (1.0f - head));
            b = mix(b, 0.42f, head) * (1.0f - 0.6f * body * (1.0f - head));
            (void)f;
            break;
        }
        default: {  // Farbfelder (gelblich, wie die Waende der ersten Ebene)
            float band = std::floor(yy * 3.0f);
            r = 0.6f - 0.08f * band, g = 0.55f - 0.07f * band, b = 0.3f - 0.03f * band;
            break;
        }
    }
    const float var = 0.85f + 0.3f * brush;
    s.r = std::clamp(r * var, 0.0f, 1.0f), s.g = std::clamp(g * var, 0.0f, 1.0f), s.b = std::clamp(b * var, 0.0f, 1.0f);
    s.h = 0.0002f * brush;
    s.rough = 0.62f;
    return s;
}

}  // namespace

VendSample signsAtlas(float u, float v, float px) {
    const int cx = std::min(7, (int)(u * 8.0f)), cy = std::min(7, (int)(v * 8.0f));
    const int cell = cy * 8 + cx;
    const float cu = u * 8.0f - (float)cx, cv = v * 8.0f - (float)cy;
    VendSample s;
    s.rough = 0.6f;
    if (cell <= 36) {
        // Schablone: Strich weiss, Grund schwarz (wird ausgeschnitten); Spruehnebel am Rand
        float c = glyphCover(cell, cu, cv, px);
        float spray = value(cu * 60.0f, cv * 60.0f, 1 << 20, 1 << 20, 3200 + (u32)cell);
        c = std::clamp(c * (0.92f + 0.16f * spray), 0.0f, 1.0f);
        s.r = s.g = s.b = c;
        s.rough = 0.75f;
        return s;
    }
    if (cell == 38 || cell == 39) return exitPictogram(cu, 1.0f - cv, px, cell == 39);
    if (cell == 41) {  // Warnschild: gelbes Dreieck, schwarzer Rand, Ausrufezeichen
        float x = cu, y = 1.0f - cv;
        float t = std::max(std::fabs(x - 0.5f) * 1.732f + (y - 0.12f) * 0.866f - 0.75f * 0.866f, 0.12f - y);
        float inner = std::max(std::fabs(x - 0.5f) * 1.732f + (y - 0.2f) * 0.866f - 0.62f * 0.866f, 0.2f - y);
        float tri = t < 0.0f ? 1.0f : 0.0f, inn = inner < 0.0f ? 1.0f : 0.0f;
        float mark = std::max(capsule(x, y, 0.5f, 0.32f, 0.5f, 0.55f, 0.035f, 0.004f), circle(x, y, 0.5f, 0.25f, 0.035f, 0.004f));
        float yel = inn * (1.0f - mark);
        s.r = tri ? mix(0.03f, 0.95f, yel) : 0.85f, s.g = tri ? mix(0.03f, 0.75f, yel) : 0.85f, s.b = tri ? mix(0.03f, 0.08f, yel) : 0.82f;
        return s;
    }
    if (cell == 42) {
        float x = cu - 0.5f, y = cv - 0.5f, r = std::hypot(x, y);
        bool red = r < 0.42f && !(std::fabs(y) < 0.07f && std::fabs(x) < 0.3f);
        s.r = r < 0.42f ? (red ? 0.75f : 0.95f) : 0.85f, s.g = r < 0.42f ? (red ? 0.08f : 0.95f) : 0.85f, s.b = r < 0.42f ? (red ? 0.06f : 0.94f) : 0.82f;
        return s;
    }
    if (cell == 43) {
        float x = cu, y = 1.0f - cv;
        float ext = std::max(capsule(x, y, 0.5f, 0.2f, 0.5f, 0.62f, 0.12f, 0.005f), capsule(x, y, 0.5f, 0.7f, 0.62f, 0.78f, 0.03f, 0.005f));
        s.r = mix(0.72f, 0.95f, ext), s.g = mix(0.07f, 0.94f, ext), s.b = mix(0.06f, 0.92f, ext);
        return s;
    }
    if (cell >= 48 && cell < 56) return paintingCell(cell - 48, cu, cv, px);
    if (cell >= 56 && cell < 60) {  // Zettel mit Handschrift (unleserlich)
        s.r = 0.9f, s.g = 0.88f, s.b = 0.8f;
        float row = cv * 12.0f;
        float lr = row - std::floor(row);
        float word = value(cu * 30.0f, std::floor(row), 1 << 20, 1 << 20, 3500 + (u32)cell);
        if (lr > 0.4f && lr < 0.55f && word > 0.4f && cu > 0.08f && cu < 0.9f && row > 1.0f && row < 11.0f) s.r = 0.18f, s.g = 0.2f, s.b = 0.32f;
        return s;
    }
    s.r = s.g = s.b = 0.0f;
    return s;
}

VendSample paintedTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    const float peel = fbm(u, v, 5, 4, 3601) * 0.5f + 0.5f;
    const float orange = value(u * 500.0f, v * 500.0f, 500, 500, 3602);
    const float scratch = std::fabs(std::sin(u * 220.0f + 6.0f * value(u * 3.0f, v * 40.0f, 3, 40, 3603)));
    s.r = s.g = s.b = 0.9f;
    s.h = 0.00004f * orange;
    s.rough = 0.42f + 0.1f * orange;
    if (peel > 0.78f) {  // Abplatzer: blankes Blech, Rostrand
        float rim = smooth(0.78f, 0.81f, peel) * (1.0f - smooth(0.81f, 0.84f, peel));
        s.r = mix(0.55f, 0.48f, rim), s.g = mix(0.55f, 0.3f, rim), s.b = mix(0.55f, 0.18f, rim);
        s.metal = 1.0f - rim;
        s.rough = 0.5f + 0.3f * rim;
        s.h = -0.0002f;
    } else if (scratch > 0.997f && value(u * 4.0f, v * 4.0f, 4, 4, 3604) > 0.55f) {
        s.r = s.g = s.b = 0.98f;
        s.rough = 0.3f;
        s.h = -0.00004f;
    }
    float dirt = smooth(0.55f, 0.9f, fbm(u, v, 2, 4, 3605) * 0.5f + 0.5f);
    s.r *= 1.0f - 0.2f * dirt, s.g *= 1.0f - 0.22f * dirt, s.b *= 1.0f - 0.25f * dirt;
    return s;
}

VendSample roadPaintTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    // abgefahren: Abrieb in Laengsrichtung (u), Beton scheint fleckig durch
    const float wear = value(u * 6.0f, v * 90.0f, 6, 90, 3701) * 0.55f + value(u * 40.0f, v * 300.0f, 40, 300, 3702) * 0.45f;
    const float holes = fbm(u, v, 12, 3, 3703) * 0.5f + 0.5f;
    float paint = smooth(0.32f, 0.4f, wear) * smooth(0.25f, 0.35f, holes);
    s.r = mix(0.38f, 0.92f, paint), s.g = mix(0.38f, 0.92f, paint), s.b = mix(0.37f, 0.9f, paint);
    s.h = 0.0003f * paint;
    s.rough = mix(0.8f, 0.6f, paint);
    return s;
}

VendSample rubberTexture(float u, float v, float px) {
    VendSample s;
    (void)px;
    const float grain = value(u * 400.0f, v * 400.0f, 400, 400, 3801);
    s.r = s.g = s.b = 0.85f + 0.08f * grain;
    s.h = 0.00008f * grain;
    s.rough = 0.88f;
    return s;
}

}  // namespace lim::gfx
