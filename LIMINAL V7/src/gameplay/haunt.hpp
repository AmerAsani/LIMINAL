// V7: Halluzinationen bei schwindendem Verstand.
//
//   Phantomschritte  (Sanity < 55) Schritte hinter dir, ein Echo im Takt deiner eigenen. Dreht
//                    man sich um, verstummen sie - und da ist nichts.
//   Stromausfall     (Sanity < 35) Die Leuchten des Raums flackern, gehen fuer ein paar Sekunden
//                    aus (auch ihr Summen verstummt) und flackern zurueck.
//   Der Andere       (Sanity < 25) Nach einem Stromausfall steht am Ende des Gangs eine Gestalt -
//                    ein Strichmaennchen wie du, nur groesser. Sie sieht dich an. Kommt man naeher
//                    oder sieht zu lange hin, flackert das Licht und sie ist fort.
//
// Alles ist rein subjektiv: keine Wirkung auf Werte, nichts davon wird gespeichert, die Welt
// bleibt unveraendert. Die Ereignisse vergibt eine eigene Zufallsquelle (nicht die der Welt).
#pragma once

#include <vector>

#include "core/math.hpp"
#include "core/rng.hpp"
#include "gameplay/body.hpp"
#include "gameplay/player.hpp"

namespace lim::world {
class World;
class Room;
}  // namespace lim::world

namespace lim::game {

struct VisibleMesh;

class Haunt {
public:
    struct Event {
        enum Type { Step, PowerOff, PowerOn, GhostSeen, GhostGone } type;
        vec3 pos;
    };
    // Raum-Leuchten aus: Flacker-Seed des Raums (-1 keiner) und Dunkelheit 0..1
    struct Blackout {
        int seed = -1;
        float amount = 0.0f;
    };

    // stepped: der Spieler hat in diesem Bild einen Schritt gesetzt
    void update(double dt, const Player& p, world::World& world, const world::Room* room, double sanity, bool stepped,
                bool enabled, std::vector<Event>& out);
    void reset();
    Blackout blackout() const { return {boSeed_, boAmount_}; }
    bool ghostActive() const { return ghost_; }
    vec3 ghostPos() const { return ghostPos_; }
    void collect(std::vector<VisibleMesh>& out);
    // Test/Skript: 1 Schritte, 2 Stromausfall, 3 Stromausfall mit Gestalt
    void force(int what) { forced_ = what; }

private:
    bool startBlackout(const world::Room* room, double len);
    bool spawnGhost(const Player& p, world::World& world, const world::Room* room);
    float blackoutCurve(double t, double len) const;

    Rng rng_{0x4A0B7u};
    int forced_ = 0;
    // Phantomschritte
    double nextSteps_ = 40.0;
    int stepsLeft_ = 0;
    vec3 stepDir_{-1, 0, 0};      // woher die Schritte kommen
    std::vector<double> echoes_;  // ausstehende Echos (Restzeit)
    // Stromausfall
    double nextBlackout_ = 70.0;
    int boSeed_ = -1;
    float boAmount_ = 0.0f;
    double boT_ = 0.0, boLen_ = 0.0;
    bool boGhost_ = false;  // in diesem Ausfall eine Gestalt setzen
    bool boLoud_ = true;    // Klang beim Aus-/Einschalten (kurzes Flackern ohne)
    // Der Andere
    bool ghost_ = false, ghostSeen_ = false;
    vec3 ghostPos_;
    double ghostT_ = 0.0, ghostLook_ = 0.0, ghostAway_ = 0.0, ghostCooldown_ = 0.0;
    int ghostSeed_ = -1;
    Body ghostBody_;
    bool ghostInit_ = false;
    Player ghostPlayer_{0, 0, 0, 0};
};

}  // namespace lim::game
