// V13.3: Leuchtkoeder (KREATUR_Smiler.md, Abschnitt "Leuchtstab als Koeder").
//
// Das Wiki (Level 1, Entity 3) raet beim Smiler: Licht aus - oder die Lichtquelle als Koeder wegwerfen. Ein geknickter
// Leuchtstab fliegt hier wie ein echter: Wurfparabel mit Ueberschlag, prallt von Waenden und Decke ab, springt auf dem
// Boden kurz auf, rutscht, rollt quer zu seiner Achse aus (oder schwimmt). Er leuchtet eine begrenzte Zeit gruen (eigenes
// Licht mit Schatten, eigenes Modell in drei Stufen: hell, matt, erloschen) und liegt danach als leere Huelle da.
// Nur Bild und Bewegung - wer ihn zuerst bemerkt, entscheidet die Praesenz (Presence::Context::lures).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/math.hpp"
#include "gameplay/visible.hpp"

namespace lim::world {
class World;
}

namespace lim::game {

class LureSystem {
public:
    static constexpr double kGlowSeconds = 60.0;  // so lange leuchtet ein Stab (danach nur noch Huelle)
    static constexpr double kSpentKeep = 240.0;   // so lange liegt die Huelle noch (dann raeumt das Spiel sie fort)
    static constexpr size_t kMax = 16;            // hoechstens so viele zugleich (die aeltesten verschwinden zuerst)
    static constexpr float kRadius = 0.0085f;     // halbe Dicke (Auflage am Boden)
    static constexpr float kHalfLen = 0.085f;     // halbe Laenge (Huelle mit Kappe und Oese)

    struct Lure {
        int id = 0, by = 0;
        vec3 pos, vel;               // Mitte des Stabs
        float yaw = 0.0f;            // Richtung der Achse in der Ebene
        float pitch = 0.0f;          // Neigung der Achse (Ueberschlag im Flug)
        float roll = 0.0f;           // Drehung um die eigene Achse (Rollen am Boden)
        float spin = 0.0f;           // Ueberschlag (rad/s)
        float rollV = 0.0f;          // Rollen (rad/s)
        double age = 0.0;
        bool ground = false, resting = false, floating = false;
        float landCool = 0.0f;
    };
    // ein Aufprall (fuer Klang und Laerm): Ort, Staerke 0..1, Art (0 Boden, 1 Wand, 2 Decke, 3 Wasser)
    struct Impact {
        vec3 pos;
        float strength = 0.0f;
        int kind = 0;
    };

    // wirft einen frisch geknickten Stab (Hand, Geschwindigkeit, Blickrichtung des Werfers); Rueckgabe: Kennung
    int launch(vec3 start, vec3 vel, float yaw, int by, unsigned seed);
    // bewegt alle Staebe (feste Teilschritte); Aufpraelle dieses Bildes in hits
    void update(double dt, world::World& w, std::vector<Impact>& hits);
    // Leuchtkraft 0..1 (Aufleuchten beim Knicken, dann langsam schwaecher, zuletzt rasch dunkel)
    static float glow(double age);
    float glowOf(const Lure& l) const { return glow(l.age); }
    void collect(std::vector<VisibleMesh>& out, vec3 eye);
    const std::vector<Lure>& lures() const { return lures_; }
    std::vector<Lure>& lures() { return lures_; }
    void clear() { lures_.clear(); }

private:
    std::vector<Lure> lures_;
    int next_ = 1;
    // Modelle (stabile Adressen fuer VisibleMesh)
    std::string mLit_ = "glowstick_lit", mDim_ = "glowstick_dim", mOff_ = "glowstick";
    int idLit_ = -2, idDim_ = -2, idOff_ = -2;
};

}  // namespace lim::game
