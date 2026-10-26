// V11: Schallausbreitung durch die Kachelwelt.
//
// Vom Ohr aus wird ein Wegefeld ueber die offenen Kacheln gerechnet (Dijkstra, acht Nachbarn, 40 m in jede
// Richtung). Eine Quelle hinter einer Wand erreicht das Ohr dann auf dem kuerzesten Weg - durch die Tuer, um
// die Ecke - statt durch die Wand:
//   - scheinbare Richtung: die letzte Stelle des Wegs, die das Ohr noch direkt "sieht" (Beugung an der Kante),
//   - Umweg: Weglaenge minus gerader Abstand (der Mischer daempft und verzoegert nach der Weglaenge),
//   - Verdeckung: gering durch eine offene Tuer, staerker je weiter der Schall um die Kante biegen muss.
// Fuehrt kein Weg in Reichweite zur Quelle, bleibt der Durchgang durch die Wand: leise und dumpf.
#pragma once

#include <unordered_map>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"

namespace lim::world {
class World;
}

namespace lim::game {

struct SoundRoute {
    vec3 pos;                 // scheinbare Position (Richtung der letzten Oeffnung, Abstand wie gerade)
    float pathExtra = 0.0f;   // Umweg (m)
    float occlusion = 0.0f;   // 0 frei .. 1 ganz dumpf
    bool direct = true;       // freie Sicht
    bool around = false;      // ueber den Weg (Tuer, Ecke)
};

class SoundField {
public:
    static constexpr int kR = 40, kN = 2 * kR + 1;
    // neu rechnen, wenn das Ohr die Kachel wechselt, sonst alle 0,5 s (Tueren, nachgeladene Chunks)
    void update(world::World& w, vec3 ear, double dt);
    void invalidate() { valid_ = false; }
    // Weg einer Quelle zum Ohr; radius: Reichweite der Quelle (laengere Wege zaehlen nicht)
    SoundRoute route(world::World& w, vec3 ear, vec3 src, float radius) const;
    // Strahl durch das Kachelraster (0 frei, 0,55 je Hindernis, ab zwei Hindernissen 1)
    static float rayOcclusion(world::World& w, vec3 a, vec3 b);
    // Weglaenge vom Ohr zur Kachel (m, < 0: kein Weg)
    float pathTo(i64 x, i64 y) const;
    int rebuilds() const { return rebuilds_; }

private:
    bool valid_ = false;
    i64 ox_ = 0, oy_ = 0;
    float earZ_ = 0.0f;
    double age_ = 0.0;
    int rebuilds_ = 0;
    std::vector<float> dist_;
    std::vector<int> parent_;
    struct Cached {
        vec3 portal;
        bool ok = false;
    };
    mutable std::unordered_map<int, Cached> portals_;  // je Quellkachel: letzte sichtbare Stelle des Wegs
    int index(i64 x, i64 y) const;
};

}  // namespace lim::game
