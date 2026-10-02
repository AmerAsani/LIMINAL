// V6: prozedurale Texturen des Getraenke- und Snackautomaten.
//
// Gehaeuse (Atlas, siehe vending_layout.hpp): dunkelroter Einbrennlack mit Orangenhaut-Struktur,
// Chromrahmen um das Fenster, hellgraues Bedienfeld mit Anzeige, Tastenfeld, Muenzschlitz,
// Rueckgabe, Geldscheinpruefer und Beschriftung, Ausgabeklappe, Leuchtschild. Gebrauchsspuren:
// Kratzer, abgeplatzter Lack an Kanten, Schmutz zum Boden hin, Fingerabdruecke am Bedienfeld,
// Aufkleberreste, abgegriffene Tasten.
// Innenraum: fuenf Faecher - Flaschen, Dosen, Snacks hinter Spiralen, Preisleisten. Einzelne
// Faecher sind leer (ausverkauft).
#include "render/texture_vending.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "render/noise.hpp"
#include "render/vending_layout.hpp"

namespace lim::gfx {

using namespace noise;

namespace {

// --- 5x7-Schrift ---------------------------------------------------------------------------------
// Zeilen von oben nach unten, Bit 4 = linke Spalte. Sonderzeichen im Text: [ = Ae, \ = Oe, ] = Ue, $ = Euro
struct Glyph {
    char c;
    std::uint8_t rows[7];
};
constexpr Glyph kFont[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}}, {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}}, {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}}, {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}}, {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}}, {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}}, {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}}, {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}}, {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04}}, {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}}, {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}}, {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}}, {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}}, {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}}, {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'[', {0x0A, 0x00, 0x0E, 0x11, 0x1F, 0x11, 0x11}}, {'\\', {0x0A, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E}},
    {']', {0x0A, 0x00, 0x11, 0x11, 0x11, 0x11, 0x0E}}, {',', {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}}, {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}}, {'$', {0x07, 0x08, 0x1E, 0x08, 0x1E, 0x08, 0x07}},
    {'/', {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}}, {'>', {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}},
};

const Glyph* glyph(char c) {
    for (const Glyph& g : kFont)
        if (g.c == c) return &g;
    return nullptr;
}

float textCentered(float x, float y, float cx, float y0, float ch, const char* s) {
    return pixelText(x, y, cx - pixelTextWidth(ch, s) * 0.5f, y0, ch, s);
}

bool inRect(float x, float z, const vend::Rect& r) { return x >= r.x0 && x < r.x1 && z >= r.z0 && z < r.z1; }
float rectEdge(float x, float z, const vend::Rect& r) {
    return std::min(std::min(x - r.x0, r.x1 - x), std::min(z - r.z0, r.z1 - z));
}

// Duenne Kratzer: kurze, gerade Linien in Zellen (Abstand in m zur naechsten Linie)
float scratches(float x, float z, float cell, std::uint32_t seed, float density) {
    float best = 1.0f;
    int cx = (int)std::floor(x / cell), cz = (int)std::floor(z / cell);
    for (int oz = -1; oz <= 1; ++oz)
        for (int ox = -1; ox <= 1; ++ox) {
            std::uint32_t h = h2(cx + ox, cz + oz, seed);
            if (r01(h) > density) continue;
            float px = ((float)(cx + ox) + r01(h * 3u + 1u)) * cell, pz = ((float)(cz + oz) + r01(h * 5u + 2u)) * cell;
            float a = r01(h * 7u + 3u) * 3.14159f, len = cell * (0.3f + 0.9f * r01(h * 11u + 4u));
            float dx = std::cos(a), dz = std::sin(a);
            float t = std::clamp((x - px) * dx + (z - pz) * dz, -len * 0.5f, len * 0.5f);
            float ex = x - (px + dx * t), ez = z - (pz + dz * t);
            best = std::min(best, std::sqrt(ex * ex + ez * ez));
        }
    return best;
}

// Lack: Orangenhaut, leicht fleckig, Kratzer, abgeplatzte Stellen (zeigen Grundierung/Metall)
void paint(VendSample& s, float x, float z, float worn, std::uint32_t seed) {
    float peel = value(x * 900.0f, z * 900.0f, 1 << 20, 1 << 20, seed) * 0.5f + value(x * 2100.0f, z * 2100.0f, 1 << 20, 1 << 20, seed + 1) * 0.5f;
    float cloud = value(x * 6.0f, z * 6.0f, 1 << 20, 1 << 20, seed + 2);
    s.r = 0.50f * (0.92f + 0.12f * cloud), s.g = 0.075f, s.b = 0.065f;
    s.rough = 0.32f + 0.08f * peel + 0.15f * worn;
    s.h = 0.00004f * peel;
    float sc = scratches(x, z, 0.09f, seed + 7, 0.55f);
    if (sc < 0.00045f) {  // Kratzer: heller, matter, leicht vertieft
        s.r = 0.72f, s.g = 0.55f, s.b = 0.5f;
        s.rough = 0.6f;
        s.h -= 0.00006f;
    }
    float chip = value(x * 55.0f, z * 55.0f, 1 << 20, 1 << 20, seed + 3) * 0.7f + value(x * 160.0f, z * 160.0f, 1 << 20, 1 << 20, seed + 4) * 0.3f;
    if (chip > 0.9f - 0.12f * worn) {  // abgeplatzter Lack: graue Grundierung bzw. blankes Metall
        bool metal = chip > 0.94f - 0.1f * worn;
        s.r = metal ? 0.55f : 0.42f, s.g = metal ? 0.55f : 0.42f, s.b = metal ? 0.56f : 0.4f;
        s.metal = metal ? 0.9f : 0.0f;
        s.rough = metal ? 0.45f : 0.75f;
        s.h -= 0.00012f;
    }
}

// Schmutz: zum Boden hin dunkler und matter, fleckig
void grime(VendSample& s, float x, float zFromFloor, std::uint32_t seed) {
    float n = value(x * 14.0f, zFromFloor * 14.0f, 1 << 20, 1 << 20, seed) * 0.6f + value(x * 50.0f, zFromFloor * 50.0f, 1 << 20, 1 << 20, seed + 1) * 0.4f;
    float g = smooth(0.45f, 0.0f, zFromFloor) * (0.5f + 0.5f * n);
    s.r *= 1.0f - 0.45f * g, s.g *= 1.0f - 0.42f * g, s.b *= 1.0f - 0.38f * g;
    s.rough = std::min(1.0f, s.rough + 0.35f * g);
    s.ao *= 1.0f - 0.25f * g;
}

}  // namespace

float pixelTextWidth(float ch, const char* s) {
    float unit = ch / 7.0f;
    return (float)std::strlen(s) * 6.0f * unit - unit;
}

float pixelText(float x, float y, float x0, float y0, float ch, const char* s) {
    float unit = ch / 7.0f;
    if (y < y0 || y >= y0 + ch || x < x0) return 0.0f;
    int col = (int)std::floor((x - x0) / unit);
    int row = (int)std::floor((y - y0) / unit);
    int idx = col / 6, cx = col % 6;
    if (idx >= (int)std::strlen(s) || cx >= 5 || row < 0 || row >= 7) return 0.0f;
    const Glyph* g = glyph(s[idx]);
    if (!g) return 0.0f;
    return (g->rows[row] >> (4 - cx)) & 1 ? 1.0f : 0.0f;
}

VendSample vendingBody(float u, float v, float px) {
    VendSample s;
    const float Z = v * vend::kHeight;  // von oben
    const float zFloor = vend::kHeight - Z;
    if (u < vend::kFrontU) {
        // ------------------------------------------------------------------ Front
        const float X = u / vend::kFrontU * 2.0f * vend::kHalfW;
        paint(s, X, Z, 0.0f, 401);
        // Leuchtschild: weisse Streuscheibe mit rotem Schriftzug und feinem Rand
        if (inRect(X, Z, vend::kHeader)) {
            float e = rectEdge(X, Z, vend::kHeader);
            s.r = 0.93f, s.g = 0.91f, s.b = 0.86f;
            s.rough = 0.25f, s.metal = 0.0f, s.h = 0.0f;
            float cx = (vend::kHeader.x0 + vend::kHeader.x1) * 0.5f;
            if (textCentered(X, Z, cx, 0.065f, 0.07f, "ERFRISCHUNG") > 0.5f) s.r = 0.72f, s.g = 0.07f, s.b = 0.06f;
            if (textCentered(X, Z, cx, 0.155f, 0.022f, "KALTE GETR[NKE - SNACKS") > 0.5f) s.r = 0.25f, s.g = 0.22f, s.b = 0.2f;
            if (e < 0.008f) s.r = 0.6f, s.g = 0.6f, s.b = 0.62f, s.metal = 1.0f, s.rough = 0.3f;
            float dust = value(X * 30.0f, Z * 30.0f, 1 << 20, 1 << 20, 450);
            s.r *= 0.94f + 0.06f * dust, s.g *= 0.94f + 0.06f * dust, s.b *= 0.92f + 0.06f * dust;
            return s;
        }
        // Chromrahmen um das Fenster
        vend::Rect wf{vend::kWindow.x0 - 0.016f, vend::kWindow.z0 - 0.016f, vend::kWindow.x1 + 0.016f, vend::kWindow.z1 + 0.016f};
        if (inRect(X, Z, wf) && !inRect(X, Z, vend::kWindow)) {
            float n = value(X * 300.0f, Z * 4.0f, 1 << 20, 1 << 20, 460);
            s.r = s.g = 0.66f + 0.08f * n, s.b = 0.68f + 0.08f * n;
            s.metal = 1.0f, s.rough = 0.18f + 0.1f * n;
            s.h = 0.0012f * smooth(0.0f, 0.006f, rectEdge(X, Z, wf));
            if (scratches(X, Z, 0.05f, 461, 0.7f) < 0.0003f) s.rough = 0.45f;
            return s;
        }
        if (inRect(X, Z, vend::kWindow)) {  // hinter dem Glas (wird vom Glas verdeckt)
            s.r = s.g = s.b = 0.05f;
            return s;
        }
        // Bedienfeld
        if (inRect(X, Z, vend::kPanel)) {
            float e = rectEdge(X, Z, vend::kPanel);
            float brush = value(X * 4.0f, Z * 1200.0f, 1 << 20, 1 << 20, 470);
            s.r = 0.6f + 0.04f * brush, s.g = 0.61f + 0.04f * brush, s.b = 0.63f + 0.04f * brush;
            s.metal = 0.85f, s.rough = 0.38f + 0.06f * brush;
            s.h = 0.001f * smooth(0.0f, 0.004f, e);
            // Fingerabdruecke/Abrieb um Tasten und Muenzschlitz
            float smudge = value(X * 70.0f, Z * 70.0f, 1 << 20, 1 << 20, 471);
            if (Z > 0.42f && Z < 1.0f) s.rough -= 0.12f * smooth(0.55f, 0.8f, smudge);
            const float pcx = (vend::kPanel.x0 + vend::kPanel.x1) * 0.5f;
            // Anzeige (Rahmen; die leuchtende Scheibe ist ein eigenes Teil mit derselben Grafik)
            vend::Rect dr{vend::kDisplay.x0 - 0.006f, vend::kDisplay.z0 - 0.006f, vend::kDisplay.x1 + 0.006f, vend::kDisplay.z1 + 0.006f};
            if (inRect(X, Z, dr)) {
                s.r = s.g = s.b = 0.03f, s.metal = 0.0f, s.rough = 0.2f, s.h = -0.0008f;
                if (inRect(X, Z, vend::kDisplay)) {
                    // LED-Anzeige: Punktraster, Text leuchtet rot-orange
                    float dot = (std::fmod(X, 0.0035f) < 0.0026f && std::fmod(Z, 0.0035f) < 0.0026f) ? 1.0f : 0.25f;
                    float t1 = textCentered(X, Z, pcx, vend::kDisplay.z0 + 0.012f, 0.024f, "W[HLEN");
                    float t2 = textCentered(X, Z, pcx, vend::kDisplay.z0 + 0.046f, 0.02f, "0,00 $");
                    float lit = std::max(t1, t2);
                    s.r = 0.05f + 0.95f * lit * dot, s.g = 0.02f + 0.3f * lit * dot, s.b = 0.02f + 0.05f * lit * dot;
                }
                return s;
            }
            // Tastenfeld 3 x 4
            static const char* keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", "OK"};
            const float kx0 = 0.70f, kz0 = 0.43f, kw = 0.05f, kh = 0.05f, ks = 0.038f;
            int kc = (int)std::floor((X - kx0) / kw), kr = (int)std::floor((Z - kz0) / kh);
            if (kc >= 0 && kc < 3 && kr >= 0 && kr < 4) {
                float lx = X - (kx0 + kc * kw + (kw - ks) * 0.5f), lz = Z - (kz0 + kr * kh + (kh - ks) * 0.5f);
                if (lx >= 0 && lx < ks && lz >= 0 && lz < ks) {
                    float edge = std::min(std::min(lx, ks - lx), std::min(lz, ks - lz));
                    s.r = 0.86f, s.g = 0.85f, s.b = 0.82f, s.metal = 0.0f;
                    // abgegriffen: Mitte glaenzender und etwas dunkler
                    float cxk = lx / ks - 0.5f, czk = lz / ks - 0.5f;
                    float wear = std::max(0.0f, 1.0f - (cxk * cxk + czk * czk) * 6.0f) * (0.4f + 0.6f * r01(h2(kc, kr, 472)));
                    s.rough = 0.45f - 0.2f * wear;
                    s.r -= 0.08f * wear, s.g -= 0.08f * wear, s.b -= 0.07f * wear;
                    s.h = 0.0018f * smooth(0.0f, 0.004f, edge);
                    const char* lab = keys[kr * 3 + kc];
                    if (textCentered(lx, lz, ks * 0.5f, ks * 0.5f - 0.007f, 0.014f, lab) > 0.5f) s.r = s.g = s.b = 0.12f;
                    s.ao = 0.85f + 0.15f * smooth(0.0f, 0.004f, edge);
                    return s;
                }
                s.ao = 0.8f;  // Spalt zwischen den Tasten
            }
            // Muenzschlitz mit Chromblende und Beschriftung
            if (textCentered(X, Z, pcx, 0.655f, 0.012f, "M]NZEN") > 0.5f) s.r = s.g = s.b = 0.1f;
            vend::Rect coin{pcx - 0.03f, 0.675f, pcx + 0.03f, 0.715f};
            if (inRect(X, Z, coin)) {
                float e = rectEdge(X, Z, coin);
                s.r = s.g = s.b = 0.75f, s.metal = 1.0f, s.rough = 0.22f;
                s.h = 0.0015f * smooth(0.0f, 0.006f, e);
                if (std::fabs(X - pcx) < 0.0025f && std::fabs(Z - 0.695f) < 0.013f) s.r = s.g = s.b = 0.02f, s.h = -0.004f, s.metal = 0.0f;
                return s;
            }
            // Rueckgabeknopf (rund, verchromt)
            float bx = X - (pcx + 0.055f), bz = Z - 0.695f;
            float br = std::sqrt(bx * bx + bz * bz);
            if (br < 0.014f) {
                s.r = s.g = s.b = 0.7f, s.metal = 1.0f, s.rough = 0.25f;
                s.h = 0.0025f * std::sqrt(std::max(0.0f, 1.0f - (br / 0.014f) * (br / 0.014f)));
                return s;
            }
            if (textCentered(X, Z, pcx + 0.055f, 0.715f, 0.008f, "R]CKGABE") > 0.5f) s.r = s.g = s.b = 0.1f;
            // Geldscheinpruefer: dunkler Schlitz, gruener Pfeil
            vend::Rect bill{pcx - 0.06f, 0.77f, pcx + 0.06f, 0.83f};
            if (inRect(X, Z, bill)) {
                float e = rectEdge(X, Z, bill);
                s.r = s.g = s.b = 0.06f, s.metal = 0.0f, s.rough = 0.3f;
                s.h = 0.002f * smooth(0.0f, 0.006f, e);
                if (std::fabs(Z - 0.80f) < 0.003f && std::fabs(X - pcx) < 0.045f) s.r = s.g = s.b = 0.0f, s.h = -0.004f;
                float ax = X - (pcx - 0.05f), az = std::fabs(Z - 0.785f);
                if (ax > 0 && ax < 0.012f && az < 0.006f - ax * 0.5f) s.r = 0.1f, s.g = 0.65f, s.b = 0.2f;
                return s;
            }
            if (textCentered(X, Z, pcx, 0.845f, 0.009f, "NUR M]NZEN") > 0.5f) s.r = 0.6f, s.g = 0.08f, s.b = 0.06f;
            // Rueckgabeschale
            vend::Rect cup{pcx - 0.04f, 0.95f, pcx + 0.04f, 1.01f};
            if (inRect(X, Z, cup)) {
                float e = rectEdge(X, Z, cup);
                s.r = s.g = s.b = 0.08f, s.metal = 0.3f, s.rough = 0.5f;
                s.h = -0.006f * smooth(0.0f, 0.008f, e);
                s.ao = 0.5f + 0.5f * smooth(0.0f, 0.01f, e);
                return s;
            }
            // Aufkleberrest (vergilbt, halb abgekratzt)
            vend::Rect stk{pcx - 0.07f, 1.07f, pcx + 0.05f, 1.15f};
            if (inRect(X, Z, stk) && value(X * 120.0f, Z * 120.0f, 1 << 20, 1 << 20, 473) > 0.35f) {
                s.r = 0.8f, s.g = 0.76f, s.b = 0.6f, s.metal = 0.0f, s.rough = 0.8f, s.h += 0.0002f;
                if (textCentered(X, Z, pcx - 0.01f, 1.10f, 0.012f, "DEFEKT?") > 0.5f) s.r = 0.3f, s.g = 0.25f, s.b = 0.2f;
            }
            return s;
        }
        // Ausgabeklappe: dunkler Kunststoff mit Scharnierlinie und Beschriftung
        if (inRect(X, Z, vend::kFlap)) {
            float e = rectEdge(X, Z, vend::kFlap);
            s.r = s.g = s.b = 0.07f, s.metal = 0.0f;
            s.rough = 0.55f + 0.1f * value(X * 200.0f, Z * 200.0f, 1 << 20, 1 << 20, 480);
            s.h = -0.004f * smooth(0.0f, 0.01f, e);
            if (std::fabs(Z - (vend::kFlap.z0 + 0.03f)) < 0.002f) s.h -= 0.002f, s.r = s.g = s.b = 0.03f;
            float cx = (vend::kFlap.x0 + vend::kFlap.x1) * 0.5f;
            if (textCentered(X, Z, cx, vend::kFlap.z0 + 0.1f, 0.022f, "HIER ENTNEHMEN") > 0.5f) s.r = s.g = s.b = 0.55f;
            if (textCentered(X, Z, cx, vend::kFlap.z0 + 0.15f, 0.014f, ">  DR]CKEN") > 0.5f) s.r = s.g = s.b = 0.45f;
            // Kratzer vom Hineingreifen
            if (scratches(X, Z, 0.05f, 481, 0.8f) < 0.0004f) s.r = s.g = s.b = 0.22f;
            return s;
        }
        // Lueftungsgitter unten
        if (Z > 1.68f && Z < 1.74f && X > 0.12f && X < 0.8f) {
            float slot = std::fmod(X - 0.12f, 0.024f);
            if (slot < 0.014f) {
                s.r = s.g = s.b = 0.02f;
                s.h = -0.004f;
                s.ao = 0.5f;
            }
        }
        // Fase der Frontkanten: Lack dort oefter abgestossen
        float side = std::min(X, 2.0f * vend::kHalfW - X);
        if (side < 0.03f) paint(s, X, Z, 0.9f, 402);
        if (Z > vend::kHeight - vend::kBase) {  // Sockel: schwarz, verschrammt
            s.r = s.g = s.b = 0.04f, s.rough = 0.7f, s.metal = 0.0f;
            if (scratches(X, Z, 0.04f, 490, 0.9f) < 0.0005f) s.r = s.g = s.b = 0.18f;
        }
        grime(s, X, zFloor, 403);
        return s;
    }
    // ---------------------------------------------------------------------- Seite / Oberseite
    const float D = (u - vend::kFrontU) / (1.0f - vend::kFrontU) * vend::kDepth;  // 0 vorn .. Tiefe hinten
    paint(s, D, Z, 0.05f, 410);
    // verblasste Werbegrafik: breites, helleres Band schraeg ueber die Seite
    float band = (D * 0.8f + (vend::kHeight - Z) * 0.35f) - 0.75f;
    if (std::fabs(band) < 0.12f) {
        float k = smooth(0.12f, 0.08f, std::fabs(band)) * 0.55f;
        s.r += (0.75f - s.r) * k, s.g += (0.6f - s.g) * k, s.b += (0.45f - s.b) * k;
    }
    // Lueftungsschlitze hinten unten
    if (D > 0.55f && D < 0.74f && Z > 1.5f && Z < 1.72f && std::fmod(Z, 0.03f) < 0.012f) s.r = s.g = s.b = 0.02f, s.h = -0.004f, s.ao = 0.5f;
    // Rost an der Unterkante
    float rust = value(D * 25.0f, Z * 25.0f, 1 << 20, 1 << 20, 411);
    if (Z > vend::kHeight - 0.25f && rust > 0.6f + (vend::kHeight - Z) * 1.2f) {
        s.r = 0.38f, s.g = 0.2f, s.b = 0.1f, s.rough = 0.9f, s.metal = 0.0f, s.h += 0.0002f;
    }
    if (Z > vend::kHeight - vend::kBase) s.r = s.g = s.b = 0.04f, s.rough = 0.7f, s.metal = 0.0f;
    grime(s, D, zFloor, 412);
    // Oberseite (oberer Teil der Region): Staubschicht
    if (Z < 0.02f) s.r = s.g = s.b = 0.3f, s.rough = 0.95f;
    (void)px;
    return s;
}

VendSample vendingInterior(float u, float v, float px) {
    VendSample s;
    const float W = vend::kWindow.x1 - vend::kWindow.x0, H = vend::kWindow.z1 - vend::kWindow.z0;
    const float x = u * W, y = v * H;
    const float rh = H / (float)vend::kRows;
    const int row = std::min(vend::kRows - 1, (int)std::floor(y / rh));
    const float ry = y - (float)row * rh;  // im Fach von oben
    s.r = s.g = s.b = 0.0f;
    s.h = -0.01f;  // durchsichtig (dahinter liegt das Fach)
    s.rough = 0.4f;
    // Preisleiste an der Fachkante (unten): dunkel, kleine weisse Schilder mit Preisen
    const float strip = 0.016f;
    const int slots = row < 2 ? 5 : (row == 2 ? 6 : 4);
    const float sw = W / (float)slots;
    const int slot = std::min(slots - 1, (int)std::floor(x / sw));
    const float sx = x - ((float)slot + 0.5f) * sw;  // relativ zur Fachmitte
    std::uint32_t id = h2(row, slot, 501);
    if (ry > rh - strip) {
        s.r = s.g = s.b = 0.08f;
        s.h = 0.002f;
        s.rough = 0.5f;
        float ly = ry - (rh - strip);
        if (std::fabs(sx) < 0.022f && ly > 0.002f && ly < strip - 0.002f) {
            s.r = s.g = s.b = 0.85f;
            static const char* prices[4] = {"1,20", "1,50", "0,90", "2,00"};
            const char* p = prices[(id >> 4) & 3u];
            if (textCentered(sx, ly, 0.0f, 0.004f, 0.008f, p) > 0.5f) s.r = s.g = s.b = 0.08f;
            // Fachnummer
        }
        return s;
    }
    if (r01(id * 13u + 5u) < 0.16f) return s;  // ausverkauft: leeres Fach
    // Grundfarben, gedaempft und etwas verblichen
    static const float pal[8][3] = {{0.62f, 0.12f, 0.1f}, {0.15f, 0.3f, 0.55f}, {0.2f, 0.45f, 0.2f},  {0.78f, 0.6f, 0.15f},
                                    {0.85f, 0.42f, 0.12f}, {0.45f, 0.18f, 0.4f}, {0.82f, 0.8f, 0.75f}, {0.12f, 0.12f, 0.13f}};
    const float* c = pal[(id >> 8) % 8u];
    const float* c2 = pal[(id >> 12) % 8u];
    const float bottom = rh - strip;  // Fachboden (von oben)
    if (row < 2) {
        // Flaschen: Koerper, Schulter, Hals, Deckel
        const float bw = 0.034f, bh = 0.165f;
        float top = bottom - bh;
        float yy = ry - top;  // 0 Deckel .. bh Boden
        if (yy < 0 || ry > bottom) return s;
        float halfW = yy < 0.02f ? 0.012f : (yy < 0.05f ? 0.012f + (yy - 0.02f) / 0.03f * (bw - 0.012f) : bw);
        if (std::fabs(sx) > halfW) return s;
        float cyl = std::cos(sx / halfW * 1.35f);
        bool clear = (id & 3u) == 0;  // Wasser in klarer Flasche
        if (yy < 0.016f) {  // Deckel
            s.r = c2[0] * 0.9f, s.g = c2[1] * 0.9f, s.b = c2[2] * 0.9f, s.rough = 0.5f;
        } else if (yy > 0.07f && yy < 0.125f) {  // Etikett
            s.r = c[0], s.g = c[1], s.b = c[2], s.rough = 0.6f;
            if (yy > 0.09f && yy < 0.1f) s.r = s.g = s.b = 0.88f;
        } else {
            s.r = clear ? 0.55f : c[0] * 0.6f, s.g = clear ? 0.62f : c[1] * 0.6f, s.b = clear ? 0.7f : c[2] * 0.6f;
            s.rough = 0.1f;
        }
        float k = 0.45f + 0.55f * cyl;
        s.r *= k, s.g *= k, s.b *= k;
        if (sx > halfW * 0.25f && sx < halfW * 0.45f && yy > 0.03f) s.r += 0.35f, s.g += 0.35f, s.b += 0.35f;  // Glanzlicht
        s.h = 0.004f;
        return s;
    }
    if (row == 2) {
        // Dosen
        const float cw = 0.033f, ch = 0.122f;
        float top = bottom - ch;
        float yy = ry - top;
        if (yy < 0 || ry > bottom || std::fabs(sx) > cw) return s;
        float cyl = std::cos(sx / cw * 1.35f);
        if (yy < 0.008f || yy > ch - 0.006f) {
            s.r = s.g = s.b = 0.7f, s.metal = 1.0f, s.rough = 0.3f;  // Rand
        } else {
            s.r = c[0], s.g = c[1], s.b = c[2], s.rough = 0.25f, s.metal = 0.3f;
            float band = std::fabs(yy - ch * 0.55f);
            if (band < 0.012f) s.r = c2[0], s.g = c2[1], s.b = c2[2];
        }
        float k = 0.4f + 0.6f * cyl;
        s.r *= k, s.g *= k, s.b *= k;
        if (sx > cw * 0.3f && sx < cw * 0.5f) s.r += 0.3f, s.g += 0.3f, s.b += 0.3f;
        s.h = 0.004f;
        return s;
    }
    // Snacks hinter Spiralen
    const float bwid = sw * 0.36f, bht = 0.15f;
    float top = bottom - bht;
    float yy = ry - top;
    // Spirale: Drahtwindungen vor dem Snack (deckend, metallisch)
    float coilPhase = (sx + sw * 0.5f) / (sw / 3.5f);
    float coilY = bottom - 0.012f - 0.075f * (0.5f + 0.5f * std::sin(coilPhase * 6.2831853f));
    if (std::fabs(ry - coilY) < 0.0022f && std::fabs(sx) < sw * 0.45f) {
        s.r = s.g = s.b = 0.62f, s.metal = 1.0f, s.rough = 0.3f, s.h = 0.004f;
        return s;
    }
    if (yy < 0 || ry > bottom) return s;
    float ex = sx / bwid, ey = (yy / bht) * 2.0f - 1.0f;
    float shape = std::pow(std::fabs(ex), 4.0f) + std::pow(std::fabs(ey), 6.0f);
    if (shape > 1.0f) return s;
    // Tuete: Folie mit Knitter, Sichtfenster, Logo-Band
    float crumple = value(sx * 300.0f, yy * 300.0f, 1 << 20, 1 << 20, id) - 0.5f;
    s.r = c[0], s.g = c[1], s.b = c[2], s.rough = 0.35f, s.metal = 0.4f;
    if (std::fabs(ey + 0.25f) < 0.18f) s.r = 0.9f, s.g = 0.85f, s.b = 0.7f;
    float k = 0.55f + 0.45f * (1.0f - shape) + 0.25f * crumple;
    s.r *= k, s.g *= k, s.b *= k;
    s.h = 0.004f;
    (void)px;
    return s;
}

}  // namespace lim::gfx
