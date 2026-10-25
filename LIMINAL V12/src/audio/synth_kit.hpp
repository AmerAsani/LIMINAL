// V11: Baukasten der Klangerzeugung. Alle Klaenge von LIMINAL entstehen beim Start aus diesen
// Bausteinen (audio/synth_*.cpp) - deterministisch aus festen Seeds.
//
// Grundsaetze (AUDIO_DESIGN.md):
//   - Klaenge werden nach Lautheit normiert (K-gewichtet), nicht nach Spitzenwert: ein Schritt und
//     ein Rohrschlag stehen so im richtigen Verhaeltnis, egal wie spitz ihre Wellenform ist.
//   - Schleifen werden mit gleicher Leistung ueberblendet und haben keine periodischen Ereignisse.
//   - Ferne entsteht durch Faltung mit einer erzeugten Raumantwort, nicht durch Kammfilter.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "audio/audio.hpp"
#include "audio/dsp.hpp"

namespace lim::audio::kit {

using dsp::Biquad;
using dsp::kPi;
using dsp::kTwoPi;
using dsp::Rand;

// Lautheitsziele (LUFS, K-gewichtet, ohne Raum) je Klangart
namespace level {
constexpr float kStep = -26.0f;      // eigene Schritte
constexpr float kFoley = -24.0f;     // Gegenstaende, Kleidung
constexpr float kEvent = -21.0f;     // Tueren, Schlaege, Mechanik
constexpr float kCreature = -20.0f;  // Stimmen und Laute der Kreaturen
constexpr float kLoop = -28.0f;      // Punktquellen-Schleifen (Leuchten, Lueftung)
constexpr float kBed = -34.0f;       // Klangbetten der Ebenen
constexpr float kDistant = -27.0f;   // ferne Ereignisse (schon gedaempft)
constexpr float kUi = -30.0f;        // Oberflaeche
}  // namespace level

struct Buf {
    std::vector<float> x;
    int rate = 48000;
    Buf() = default;
    Buf(float seconds, int r) : x((size_t)std::max(1.0f, seconds * (float)r), 0.0f), rate(r) {}
    size_t n() const { return x.size(); }
    float t(size_t i) const { return (float)i / (float)rate; }
    size_t at(float sec) const { return (size_t)std::max(0.0f, sec * (float)rate); }
    // Klang hinzumischen (wird bei Bedarf verlaengert)
    void add(const std::vector<float>& s, float atSec, float gain = 1.0f);
    void add(const Buf& b, float atSec, float gain = 1.0f) { add(b.x, atSec, gain); }
    Sound sound(bool loop = false) const {
        Sound s;
        s.samples = x;
        s.loop = loop;
        s.rate = rate;
        return s;
    }
};

// --- Nachbearbeitung -----------------------------------------------------------------------------
// auf Ziel-Lautheit bringen, Spitzen hoechstens peak (danach leiser, nie verzerrt)
void normalize(Buf& b, float lufs, float peak = 0.9f);
// sanft ein- und ausblenden (gegen Knackser)
void fades(Buf& b, float inSec, float outSec);
// Stille am Ende kuerzen (unter -70 dB relativ zur Spitze), danach kurz ausblenden
void trim(Buf& b, float keepSec = 0.02f);
// nahtlose Schleife: das Ende wird mit gleicher Leistung in den Anfang ueberblendet (Laenge sinkt um xfade)
void loopify(Buf& b, float xfadeSec);
// Gleichanteil entfernen
void dcBlock(Buf& b);
// Faltung mit einer kuenstlichen Raumantwort; wet/dry gemischt, Laenge waechst um die Fahne
void reverb(Buf& b, const dsp::RoomSpec& room, float wet, float dry, std::uint32_t seed, float tail = -1.0f);
// "weit weg im Gebaeude": Tiefpass, Hochpass, Faltung mit grosser Raumantwort, kein Direktschall
void distantize(Buf& b, float lowpass, float t60, std::uint32_t seed);
// Filter auf den ganzen Puffer
void filter(Buf& b, Biquad::Type t, float hz, float q, float gainDb = 0.0f);
void applyEnv(Buf& b, const std::function<float(float)>& env);

// --- Erzeuger ------------------------------------------------------------------------------------
// Huellkurve: lineare Attacke, exponentieller Abfall (60 dB nach t60)
inline float envAD(float t, float attack, float t60) {
    if (t < 0) return 0.0f;
    if (t < attack) return t / attack;
    return std::exp(-6.9078f * (t - attack) / std::max(1e-4f, t60));
}
// weiche Huelle fuer Ereignisse mit Plateau (Anstieg, Halten, Abfall; Sekunden)
inline float envAHR(float t, float a, float h, float r) {
    if (t < 0) return 0.0f;
    if (t < a) return dsp::smooth01(t / a);
    if (t < a + h) return 1.0f;
    return 1.0f - dsp::smooth01((t - a - h) / std::max(1e-4f, r));
}

// Anschlag eines Koerpers: Kraftstoss (Dauer contact s) regt Moden an; dazu ein kurzer
// Rauschanteil (Kontaktgeraeusch). Rueckgabe: Signal (Laenge len s)
struct ImpactSpec {
    std::vector<dsp::Mode> modes;
    float contact = 0.003f;   // Kontaktdauer (s): kurz = hart, hell
    float noise = 0.2f;       // Rauschanteil des Kontakts
    float noiseHz = 3000.0f;  // Mitte des Kontaktrauschens
    float noiseQ = 0.7f;
    float noiseT60 = 0.02f;
};
std::vector<float> impact(const ImpactSpec& s, float strength, float len, int rate, Rand& r);
// Modensaetze typischer Koerper (Grundfrequenz f0, Streuung je Variante)
std::vector<dsp::Mode> modesMetalPipe(float f0, float t60, Rand& r);     // Rohr: inharmonisch, lang
std::vector<dsp::Mode> modesMetalPlate(float f0, float t60, Rand& r);    // Blech/Gitter: dicht
std::vector<dsp::Mode> modesWood(float f0, float t60, Rand& r);          // Holzbrett: wenige, kurz
std::vector<dsp::Mode> modesConcrete(float f0, Rand& r);                 // Beton: dicht, sehr kurz
std::vector<dsp::Mode> modesCeramic(float f0, float t60, Rand& r);       // Fliese/Porzellan: hell
std::vector<dsp::Mode> modesGlass(float f0, float t60, Rand& r);         // Glas: hell, lang
std::vector<dsp::Mode> modesBell(float f0, float t60, Rand& r);          // Glocke (Aufzug)
std::vector<dsp::Mode> modesPlastic(float f0, float t60, Rand& r);       // Kunststoffgehaeuse

// Koerner: count kurze Klaenge zu zufaelligen Zeiten in [t0, t1) (Dichte nach density(t))
void grains(Buf& b, float t0, float t1, float perSecond, const std::function<void(Buf&, float, Rand&)>& grain, Rand& r,
            const std::function<float(float)>& density = nullptr);
// Blase / Tropfen nach Minnaert: steigender Sinus (Radius in mm bestimmt die Frequenz)
void bubble(Buf& b, float atSec, float radiusMm, float amp, Rand& r);
// Reibung (Stick-Slip): Impulsfolge mit schwankender Rate durch Resonanzen - Knarren, Quietschen
struct CreakSpec {
    float rate0 = 40.0f, rate1 = 60.0f;  // Impulse je Sekunde (Anfang, Ende)
    float jitter = 0.25f;
    std::vector<float> formants{600.0f, 1400.0f, 2900.0f};
    float q = 8.0f;
};
void creak(Buf& b, float atSec, float len, const CreakSpec& c, float amp, Rand& r);

// Stimme (Quelle-Filter): Stimmlippenimpulse mit Jitter/Shimmer/Unterharmonischen durch Formanten
struct VoiceSpec {
    std::function<float(float)> f0;             // Grundfrequenz ueber der Zeit (t in s, relativ)
    std::function<float(float)> amp;            // Lautstaerke ueber der Zeit
    std::vector<float> formants{500, 1100, 2400};
    std::vector<float> bandwidths{90, 140, 200};
    float jitter = 0.02f, shimmer = 0.1f;
    float subharm = 0.0f;   // 0..1 Periodenverdopplung (Knurren)
    float breath = 0.15f;   // Atemrauschen
    float rough = 0.0f;     // Rauheit (Amplitudenmodulation 20-40 Hz)
    float roughHz = 28.0f;
    float tiltHz = 3500.0f; // spektrale Neigung: oberhalb faellt das Spektrum mit 12 dB/Oktave ab
};
void voice(Buf& b, float atSec, float len, const VoiceSpec& v, Rand& r);
// Atem: Rauschen durch Vokaltrakt-Formanten (h-Laut), Ein- oder Ausatmen
void breathe(Buf& b, float atSec, float len, bool inhale, float amp, float tension, Rand& r);

}  // namespace lim::audio::kit
