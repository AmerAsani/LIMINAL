// V9: "Der Andere" ist echt.
//
// In V7 war die grosse, gesichtslose Gestalt am Ende des Gangs eine Halluzination. Jetzt gibt es
// ihn wirklich - und er folgt dir. Er hat nur eine Regel:
//
//   Er bewegt sich nur, wenn du ihn nicht siehst.
//
// "Sehen" heisst: Er ist in deinem Blickfeld, nichts steht dazwischen, und er steht im Licht -
// eine Leuchte ueber ihm oder der Kegel deiner Taschenlampe. Im Dunkeln siehst du ihn nicht, also
// kommt er. Dreht man ihm den Ruecken zu, holt er auf (etwas schneller, als man geht). Schaut man
// ihn an, erstarrt er mitten im Schritt; doch wer ihn lange anstarrt, verliert schneller den
// Verstand. Erreicht er dich ungesehen, packt er zu (Health und Sanity sinken stark) und ist fort.
// Er faehrt nicht mit dem Aufzug. Nach einer Weile verliert er die Spur.
//
// Der Weg zum Spieler ist ein Distanzfeld (Dijkstra) ueber die geladenen Kacheln rund um den
// Spieler; der Andere folgt seinem Gefaelle. Er erscheint nur ungesehen und nie in der Naehe.
// Nichts davon wird gespeichert (nach dem Laden beginnt eine neue Schonfrist).
#pragma once

#include <vector>

#include "core/math.hpp"
#include "core/rng.hpp"
#include "gameplay/body.hpp"
#include "gameplay/player.hpp"

namespace lim::world {
class World;
}

namespace lim::game {

struct VisibleMesh;

class Stalker {
public:
    struct Event {
        enum Type { Appear, Step, FirstSight, Attack, Lost } type;
        vec3 pos;
    };
    // Was der Spieler gerade sieht (Kamera, Taschenlampe, Stromausfall)
    struct View {
        vec3 eye, look;  // look: Blickrichtung (normiert)
        bool flashOn = false;
        vec3 flashPos, flashDir;
        int blackoutSeed = -1;
        float blackoutAmount = 0.0f;
    };
    struct Params {
        double speed = 4.0;  // m/s, solange ungesehen
        double pause = 1.0;  // Faktor fuer die Pausen zwischen zwei Jagden
    };

    // enabled: Einstellung "Verfolger"; suspended: gerade nicht moeglich (Aufzugfahrt, Tod ...)
    void update(double dt, const Player& p, const View& v, world::World& w, double sanity, bool enabled, bool suspended,
                const Params& prm, std::vector<Event>& out);
    // verschwinden; fruehestens nach `delay` Sekunden wieder
    void vanish(double delay);
    void setGrace(double s) { next_ = s; }
    bool active() const { return state_ == Hunt; }
    bool seen() const { return active() && seen_; }
    vec3 pos() const { return {(float)x_, (float)y_, (float)z_}; }
    // 0 (weit/fort) .. 1 (unmittelbar neben dir)
    float proximity() const;
    double distance() const { return dist_; }
    double nextIn() const { return next_; }
    void collect(std::vector<VisibleMesh>& out);
    // Test/Skript: sofort erscheinen (dist > 0: in etwa dieser Wegstrecke)
    void force(double dist = 0.0) { forced_ = true, forcedDist_ = dist; }

private:
    enum State { Idle, Hunt };
    static constexpr int kR = 40, kN = 2 * kR + 1;  // Distanzfeld: 81 x 81 Kacheln um den Spieler
    void buildField(world::World& w, i64 cx, i64 cy);
    int fieldAt(i64 x, i64 y) const;
    bool spawn(const Player& p, const View& v, world::World& w, int lo, int hi);
    bool seenBy(const View& v, world::World& w) const;
    bool litAt(const View& v, world::World& w, vec3 target) const;

    State state_ = Idle;
    Rng rng_{0x57A1C3u};
    double next_ = 180.0;  // bis zum naechsten Auftritt (s)
    bool forced_ = false;
    double forcedDist_ = 0.0;
    double x_ = 0, y_ = 0, z_ = 0, angle_ = 0;
    double huntT_ = 0.0, lostT_ = 0.0, dist_ = 1e9, seenT_ = 0.0;
    bool seen_ = false, sighted_ = false;
    // Distanzfeld (Kosten 10 je gerader, 14 je schraeger Kachel; -1 unerreichbar)
    std::vector<int> field_;
    i64 fx_ = 0, fy_ = 0;  // Mitte des Feldes (Spielerkachel)
    double fieldT_ = 0.0;
    // Koerper: dieselbe Figur wie die Halluzination, angetrieben von einem Schein-Spieler
    Body body_;
    Player puppet_{0, 0, 0, 0};
    bool bodyInit_ = false;
};

}  // namespace lim::game
