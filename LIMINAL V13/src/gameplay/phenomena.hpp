// V13.3: Erscheinungen von Level 1 ohne eigene Figur (Wikidot level-1, Revision 83; entity-17, entity-99).
//
//   Crawler-Pilz (Entity 17, KREATUR_Crawler.md): befallene Stellen (Myzel an feuchten Waenden und Pfuetzen, verpilzte
//     Kadaver, das Lager eines Infizierten). Wer lange darin steht oder watet oder unversiegeltes Mandelwasser trinkt,
//     steckt sich an. Die Stadien folgen der Zeitleiste des Wikis (3 min Juckreiz, 1 min Flecken und Hunger, 30 s Fieber,
//     7,5 min Uebernahme), danach hat der Pilz den Koerper. Mandelwasser heilt die ersten zwei Stadien und haelt die
//     spaeteren auf (Interpretation - das Wiki kennt kein Gegenmittel).
//   Garden (KREATUR_L1_Erscheinungen.md): die Garden-Uhr zaehlt die Zeit im Sektor; Zeichen, Ranken, Verwandlung, Tod
//     (eigener Grund). Draussen geht sie langsam zurueck.
//   Gemaelde (KREATUR_GameMaster.md): Bilder liegen kurz vor den Tueren zu den Hallen am Boden und verschwinden,
//     wenn niemand hinsieht (tauchen an einer anderen Tuer wieder auf). Eines zeigt die Spielleiterin (Entity 99) mit
//     ihrem Bildschirmgesicht; wer darueber geht, wird eingeladen - und "aus dem Spiel geworfen" (Interpretation).
//   Spuren an einer Zimmertuer (KREATUR_L1_Erscheinungen.md): selten klatschende Schritte, Krallen am Metall, Atem.
//
// Ort aller Erscheinungen: Spawnpunkte der Karte mit kind "trace" (tools/levels/level1.py). Spielrelevantes (Ansteckung,
// Garden-Uhr, Tod, Hinauswurf) rechnet der Host fuer alle Spieler; der Gast rechnet fuer sich nur Bild und Klang.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/json.hpp"
#include "core/math.hpp"
#include "core/rng.hpp"
#include "gameplay/visible.hpp"

namespace lim::world {
class World;
struct LevelDef;
}  // namespace lim::world

namespace lim::game {

struct Viewer;

class Phenomena {
public:
    static constexpr int kPlayers = 4;  // 0 = dieser Rechner (Einzelspieler, Host oder Gast), 1..3 Gaeste beim Host

    struct Config {
        bool fungus = false, garden = false, paintings = false, claws = false, jester = false;
        // Pilz: Dauer der Stadien 1..4 (Wiki entity-17: 3 min, 1 min, 30 s, 7 min 30 s)
        double stage[4] = {180.0, 60.0, 30.0, 450.0};
        double touchInfect = 2.5;    // s Kontakt mit Befall bis zur Ansteckung
        double cureUntil = 2;        // Mandelwasser heilt bis zu diesem Stadium; danach haelt es nur auf
        double holdSeconds = 60.0;   // so lange haelt Mandelwasser spaeter das Fortschreiten an
        // Garden-Uhr (s): Zeichen, Ranken, Beginn der Verwandlung (Wiki: "eine Stunde", im Spiel verdichtet), Tod
        double gardenSigns = 180.0, gardenVines = 360.0, gardenBegin = 600.0, gardenDeath = 900.0;
        double gardenRecover = 0.33;  // draussen geht die Uhr mit diesem Anteil der Zeit zurueck
        // Gemaelde
        int paintingCount = 4;
        double vanishAfter = 7.0;      // s ungesehen, dann kann ein Gemaelde verschwinden
        double jesterCooldown = 240.0; // s nach einem Hinauswurf (fuer diesen Spieler ist sie dann nicht im Bild)
        // Spuren an der Tuer: mittlerer Abstand der Geraeusche, solange man in der Naehe ist (s)
        double clawEvery = 110.0;
    };

    struct Ailment {
        double touch = 0.0, touchWarn = 0.0;  // Kontakt mit Befall (s), Sperre fuer den Hinweis
        double infectT = -1.0;                // seit der Ansteckung (s); < 0 gesund
        double hold = 0.0;                    // Mandelwasser haelt das Fortschreiten an (s)
        int stage = 0;                        // 0 gesund, 1..4
        double garden = 0.0;                  // Garden-Uhr (s)
        int gstage = 0;                       // 0, 1 Zeichen, 2 Ranken, 3 Verwandlung
        bool inGarden = false;
        double jesterCool = 0.0;              // nach dem Hinauswurf (s)
        double jesterT = -1.0;                // Einladung laeuft (s seit Beginn), < 0 keine
        int jesterLine = 0;
        double twitchT = 12.0, itchT = 8.0, gardenSoundT = 10.0;
        bool dead = false;
        std::string cause;                    // "pilz" oder "garten"
    };

    struct Event {
        enum Kind { Toast, Sound2D, SoundAt, Twitch, Died, JesterStart, JesterExpel, Infected, Cured } kind = Toast;
        int player = 0;
        std::string text;   // Meldung bzw. Klang
        vec3 pos;           // SoundAt: Ort; JesterExpel: Ziel
        float value = 1.0f; // Lautstaerke, Zucken (rad), Blick nach dem Hinauswurf
        float value2 = 0.0f;
    };

    // Eingabe je Spieler und Bild
    struct PlayerIn {
        vec3 feet;
        bool alive = true, sneaking = false, onPuddle = false, inGarden = false;
    };

    struct Painting {
        int spot = -1, variant = 0;
        bool jester = false, shown = true;
        vec3 pos;
        float yaw = 0.0f, halfW = 0.5f, halfH = 0.35f;
        double unseen = 0.0, hidden = 0.0, squeakT = 6.0;
        int expr = 0;
        std::string model;
        int meshId = -2;
    };
    struct Spot {
        vec3 pos;
        float yaw = 0.0f, radius = 1.0f;
        std::string id;
    };
    struct ClawDoor {
        Spot spot;
        double near = 0.0, cool = 25.0, seqT = -1.0;
        int seqStep = 0, variant = 0;
    };

    // Karte und Einstellungen der Ebene lesen (setzt alles zurueck, ausser den Leiden der Spieler)
    void setup(const world::LevelDef& level, u64 seed);
    const Config& config() const { return cfg_; }
    bool any() const { return cfg_.fungus || cfg_.garden || cfg_.paintings || cfg_.claws; }

    // ein Spieler: Kontakt, Stadien, Garden-Uhr, Einladung der Spielleiterin. authoritative: Einzelspieler oder Host
    // (Tod und Hinauswurf); der Gast rechnet fuer sich nur Bild, Klang und Tempo
    void updatePlayer(int id, const PlayerIn& in, double dt, bool authoritative, double blackout, std::vector<Event>& out);
    // die Gemaelde (alle Betrachter dieses Rechners) und die Geraeusche hinter der Tuer (nur fuer den eigenen Spieler)
    void updateWorld(double dt, const std::vector<Viewer>& viewers, const PlayerIn& self, bool selfInClawRoom, double blackout,
                     world::World& w, std::vector<Event>& out);
    // Trinken: "almond" heilt bzw. haelt auf, "almond_open" (unversiegelt) steckt an. Zusatzmeldung ("" keine)
    std::string drink(int id, const std::string& itemKey, std::vector<Event>& out);
    void reset(int id) { ail_[(size_t)id] = Ailment{}; }
    void clearGarden(int id) { ail_[(size_t)id].garden = 0.0, ail_[(size_t)id].gstage = 0; }

    // Wirkung
    double sanityDrain(int id) const;  // zusaetzlich je Sekunde (Prozentpunkte)
    double speedMul(int id) const;     // 1 = normal
    bool frozen(int id) const { return ail_[(size_t)id].jesterT >= 0.0; }
    float fungusFx(int id) const;      // Bild 0..1
    float gardenFx(int id) const;      // Bild 0..1
    vec4 skinFx(int id) const;         // x Pilz, y Ranken (Haut)
    float muffle(int id) const;        // Klang dumpf 0..1
    float jesterFx(int id) const;      // Bildschirmgesicht: Ausdruck * 2 + Staerke
    const Ailment& ailment(int id) const { return ail_[(size_t)id]; }
    Ailment& ailment(int id) { return ail_[(size_t)id]; }
    static int stageOf(const Config& c, double t);  // Stadium zur Zeit seit der Ansteckung (5 = vorbei)

    // Gemaelde zeichnen (Bild, nicht Spiel)
    void collect(std::vector<VisibleMesh>& out, vec3 eye);
    const std::vector<Painting>& paintings() const { return paintings_; }
    std::vector<Painting>& paintings() { return paintings_; }
    const std::vector<Spot>& paintingSpots() const { return paintSpots_; }
    const std::vector<Spot>& fungusZones() const { return fungus_; }
    const std::vector<Spot>& bottleSpots() const { return bottles_; }
    const std::vector<Spot>& noteSpots() const { return notes_; }
    const std::vector<Spot>& expelSpots() const { return expel_; }
    std::vector<ClawDoor>& clawDoors() { return claws_; }
    // in einem befallenen Bereich? (Halbmesser der Stelle)
    bool inFungus(vec3 feet) const;
    // Test/Skript: Gemaelde sofort an einen anderen freien Platz (false: keiner frei)
    bool relocate(Painting& p, const std::vector<vec3>& avoid);

    Json toJson(int id) const;
    void fromJson(int id, const Json& j);

private:
    Config cfg_;
    std::array<Ailment, kPlayers> ail_{};
    std::vector<Spot> fungus_, paintSpots_, bottles_, notes_, expel_;
    std::vector<ClawDoor> claws_;
    std::vector<Painting> paintings_;
    Rng rng_{1};
    u64 seed_ = 1;
    double checkT_ = 0.0;
    void jesterTick(int id, Ailment& a, const PlayerIn& in, double dt, bool authoritative, std::vector<Event>& out);
    bool onPainting(const Painting& p, vec3 feet, float grow) const;
    void modelOf(Painting& p) const;
};

}  // namespace lim::game
