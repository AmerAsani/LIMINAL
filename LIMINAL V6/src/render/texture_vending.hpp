// V6: Texturen des Automaten (Gehaeuse-Atlas und Faecher hinter dem Glas).
#pragma once

namespace lim::gfx {

struct VendSample {
    float r = 0.5f, g = 0.5f, b = 0.5f;
    float h = 0.0f;      // Hoehe (m) -> Normalen; im Innenraum: > 0 = Produkt (deckend)
    float rough = 0.5f;
    float ao = 1.0f;
    float metal = 0.0f;
};

// Gehaeuse: Lack, Bedienfeld, Tasten, Anzeige, Muenzschlitz, Klappe, Leuchtschild, Gebrauchsspuren
VendSample vendingBody(float u, float v, float px);
// Faecher: Flaschen, Dosen, Snacks mit Spiralen, Preisleisten (Maske ueber die Hoehe)
VendSample vendingInterior(float u, float v, float px);
// 5x7-Pixelschrift: Deckung an (x, y) fuer Text ab (x0, y0) mit Zeichenhoehe ch (gleiche Einheit)
float pixelText(float x, float y, float x0, float y0, float ch, const char* s);
float pixelTextWidth(float ch, const char* s);

}  // namespace lim::gfx
