// V13.3: Level 3 - "purpifizierte" tote Wesen in den Ziegelwaenden (KREATUR_L3_Purpur.md; Wikidot level-3, Revision 18,
// Abschnitt Purpification; eigene Worte).
//
// Die Seite: Selten findet man tote Wesen, die zum Teil in die Wand genoclippt sind; sie und die Wand um sie herum sind
// ganz violett, violette Energie stroemt aus ihnen ueber die Ziegel. Ziegel und Kadaver enthalten viel Gammastrahlung und
// eine unbekannte, starke Energie, die violett leuchtet. Neben einem Hound stand eine gemalte Botschaft.
//
// Die Koerper sind Ausstattung der Karte (Modelle purple_stealer, purple_hound; tools/levels/level3.py); jede Stelle hat
// einen Spawnpunkt kind "trace", set "purple" (Tag stealer/hound) 0,3 m vor der Wand. Hier steht nur, was die Stelle tut:
//   Licht    violettes Zusatzlicht vor dem Koerper, atmet langsam; seltener Schwall          (Seite: leuchtet violett)
//   Klang    Brummen (Schleife), Knistern in Schueben, selten Knacken im Mauerwerk           (Interpretation)
//   Strahlung in der Naehe zehrt sie am Verstand, Bildrauschen, ein Hinweis (Metallgeschmack) (Seite: Gamma; Wirkung Interpretation)
//   Lampe    nah an der Stelle stockt die Taschenlampe; wer sie auf den Koerper richtet,
//            laesst die Glut anschwellen - die Energie zieht am Licht                        (Interpretation)
//   Schrift  wer vor der Botschaft neben dem Hound steht und hinsieht, liest sie (Untertitel)
// Es sind Tote: sie bewegen sich nicht und greifen nicht an (Seite: kein Verhalten beschrieben).
// Mehrspieler: den Verstand der Gaeste rechnet der Host (Lage der Figuren); Licht, Klang, Lampe, Bild und Hinweise
// rechnet jeder Rechner fuer sich aus derselben Karte. Kein Spielzustand im Netz, Protokoll unveraendert.
#pragma once

#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"
#include "core/rng.hpp"

namespace lim::world {
struct LevelDef;
}

namespace lim::game {

class Purpification {
public:
    struct Config {
        double hearRadius = 15.0;   // m: Knistern und Brummen
        double radRadius = 3.6;     // m: Strahlung spuerbar (Verstand, Bildrauschen)
        double drain = 0.32;        // Prozentpunkte Verstand je Sekunde dicht davor (faellt bis radRadius auf 0)
        double crackleEvery = 2.6;  // s im Mittel zwischen zwei Schueben Knistern
        double settleEvery = 48.0;  // s im Mittel zwischen zwei Knacken im Mauerwerk
        double surgeEvery = 34.0;   // s im Mittel zwischen zwei Schwallen
        double lampRadius = 4.2;    // m: die Taschenlampe stockt
        double readRadius = 3.4;    // m: die Botschaft lesen
    };
    struct Site {
        std::string kind;           // "stealer", "hound"
        vec3 base;                  // Ursprung des Modells (Wandflaeche, Boden)
        float yaw = 0.0f;           // in den Gang
        vec3 glow;                  // Mitte des Leuchtens (vor dem Koerper)
        vec3 text;                  // Mitte der Botschaft (nur hound), sonst wie base
        std::string loop = "purple_hum";
        int voice = -1;             // Klangschleife (App)
        double crackleT = 1.0, settleT = 20.0, surgeT = 15.0, readCool = 0.0;
        float surge = 0.0f;         // 0..1 Schwall (klingt ab)
        float lit = 0.0f;           // 0..1 die eigene Lampe liegt darauf
        float flash = 0.0f;         // Knistern gerade eben (Bild)
        float phase = 0.0f;
        bool seen = false;
    };
    struct Event {
        enum Kind { Toast, SoundAt } kind = Toast;
        std::string text;   // Meldung bzw. Klang
        vec3 pos;
        float value = 1.0f; // Dauer der Meldung bzw. Lautstaerke
    };
    struct Light {
        vec3 pos, color;
        float range = 4.0f;
    };

    void setup(const world::LevelDef& level, u64 seed);
    bool any() const { return !sites_.empty(); }
    const Config& config() const { return cfg_; }
    const std::vector<Site>& sites() const { return sites_; }
    std::vector<Site>& sites() { return sites_; }

    // Strahlung am Ort (0..1): 1 dicht vor dem Koerper, 0 ab radRadius; hinter der Wand nichts
    float exposure(vec3 pos) const;
    double sanityDrain(vec3 pos) const { return cfg_.drain * (double)exposure(pos); }
    // eigener Spieler (Klang, Hinweise, Lesen, Schwall). lampOn/lampPos/lampDir: die eigene Taschenlampe
    void update(double dt, vec3 eye, vec3 look, bool alive, bool lampOn, vec3 lampPos, vec3 lampDir, std::vector<Event>& out);
    // Darstellung: Zusatzlichter (naechste zuerst), Faktor auf die Taschenlampe, Bildrauschen
    void lights(std::vector<Light>& out, vec3 eye, float radius, double time) const;
    float lampFactor(vec3 eye, double time) const;
    float grain(vec3 eye) const;
    float glowLevel(const Site& s, double time) const;  // 0.5..2 (Atmen, Schwall, Lampe)

    static const char* readingText();  // die Botschaft in eigenen Worten (Untertitel)

private:
    Config cfg_;
    std::vector<Site> sites_;
    Rng rng_{1};
    bool hintRad_ = false, hintSeen_ = false;
};

}  // namespace lim::game
