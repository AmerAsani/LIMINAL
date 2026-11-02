// V13.3 Teil 4: Randwesen von Level 2 ohne eigene Rolle (Wikidot level-2, Revision 93; entity-36 Rev. 13, entity-192
// Rev. 17, entity-104 Rev. 43, entity-189 Rev. 27 - alle CC BY-SA 3.0). Modelle: tools/blender/models/models_l2_rand.py.
//
//   Cannibal Cuisine (KREATUR_CannibalCuisine.md): leuchtende Automaten an Sackgassen und kahlen Waenden. Ihr Licht bleibt
//     im Stromausfall an (Wegmarke). Drinnen fluestern die Leichen; eine Taste druecken: eine Knochenhand schiebt die Ware
//     vor, bis sie faellt - gratis. Wer sie isst, ist suechtig: anderes Essen und Mandelwasser nimmt der Koerper nicht mehr
//     an, fern der Automaten zehrt der Entzug, die Fluesterer werden zu Schluchzen in einer vertrauten Stimme. Wer den
//     Automaten angreift, den schlagen Skelettarme aus den Klappen (die unteren stemmen ihn hoch).
//   Swindlebird (KREATUR_Swindlebird.md): hoechstens eine Sichtung je Lauf, nie bestaetigt - ein blauer, papageienhafter
//     Vogel am Ende eines langen Tunnels, der auftaucht, wo niemand hinsieht, und fort ist, sobald man ihn zu genau
//     ansehen oder naeher kommen will. Keine Feder, keine Spur.
//   Photoshop (KREATUR_Photoshop.md): hoechstens einer je Lauf, neben dem Roehrenmonitor eines Bueros; der Monitor zeigt
//     sein Bild. Wer naeher als fuenf Fuss kommt, den schliesst eine durchsichtige Box mit schwarzen Kanten ein, die
//     schrumpft; wer hindurchgeht oder bleibt, wird "ausgeschnitten". Ein hingeworfener Gegenstand rettet: ein weisser,
//     blutiger Arm holt ihn in den Bildschirm, die Box faellt, das Wesen sucht seine Beute anderswo.
//   Macchina (KREATUR_Macchina.md): seit etwa 1850 verschwunden - nur Reste in der Werkstatt (Anzug, Kindergestell,
//     Wandtuch, Pike; Kartenobjekte). Hier: der hohle Anzug klingt im Luftzug, der erste Blick darauf erzaehlt den Fund,
//     eine Blechtafel mit Zeichen liegt zu seinen Fuessen (Notizbuch).
//
// Orte: Spawnpunkte der Karte mit kind "trace" (tools/levels/level2.py, set = cuisine, swindle, photoshop, macchina).
// Spielentscheidendes (Ware, Hiebe, Sichtung, Box, Tod, Gabe) rechnet der Host fuer alle; die Gaeste bekommen es als
// Weltereignis Dropped "l2rand" mit einer Kennzahl (Protokoll unveraendert) und rechnen daraus nur Bild und Klang.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/core.hpp"
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

class Apparitions {
public:
    static constexpr int kPlayers = 4;  // 0 = dieser Rechner, 1..3 Gaeste beim Host
    // Masse aus models_l2_rand.py (Gelenke gleichlautend)
    static constexpr float kCcW = 1.22f, kCcD = 0.76f, kCcH = 1.83f;
    static constexpr float kPsW = 0.9f, kPsH = 2.36f;

    struct Config {
        bool cuisine = false, swindle = false, photoshop = false, macchina = false;
        int stock = 3;                  // Ware je Automat (Wiki: Nachschub alle 12 h - laenger dauert kein Lauf)
        double strike = 30.0;           // Schaden je Hieb der Skelettarme (Prozentpunkte Gesundheit)
        double withdraw = 0.06;         // Entzug: zusaetzlicher Verstandsverlust je s fern der Automaten (Prozentpunkte)
        double photoshopChance = 0.6;   // ist Photoshop in diesem Lauf da?
        double boxRange = 1.52;         // 5 Fuss um den Klotz
        double boxTime = 5.0;           // s, bis die Box geschlossen ist
        double swindleChance = 0.5;     // gibt es in diesem Lauf eine Sichtung?
        double swindleAfter = 240.0, swindleSpan = 900.0;  // s auf der Ebene bis dahin
    };

    struct Event {
        enum Kind { Toast, SoundAt, Hurt, Died, Net } kind = Toast;
        int player = 0;      // Toast/Hurt/Died: wen es trifft (Klaenge: nur fuer diesen Rechner)
        std::string text;    // Meldung, Klang bzw. Todesursache ("automat", "photoshop")
        vec3 pos;
        float value = 1.0f;  // Lautstaerke, Schaden, Dauer der Meldung
        u32 code = 0;        // Net: Kennzahl des Ereignisses fuer die Gaeste
    };

    struct PlayerIn {
        vec3 feet, eye, look;
        bool alive = false, flash = false;
    };

    struct Part {
        std::string model;
        int id = -2;
    };

    struct Cuisine {
        vec3 pos;
        float yaw = 0.0f;
        int variant = 0, stock = 3;
        int slotRow = 0, slotCol = 0;      // das leere Fach, durch das die Hand nachschiebt
        std::string id;
        double vendT = -1.0;               // s seit dem Tastendruck (< 0: nichts)
        double angerT = -1.0, angerLen = 0.0, strikeT = 0.0;
        int target = -1, strikeArm = 0;
        float lurch = 0.0f, lift = 0.0f;   // Ausfall nach vorn, Anheben auf den unteren Armen (m)
        float flicker = 1.0f;              // Kopfschild: Helligkeit (Licht)
        double whisperT = 6.0;
        Part body, flapTop, flapBot, hum, arm[4][3], vendHand, ware;
    };

    struct Bird {
        int state = 0;                     // 0 keine, 1 sitzt, 2 fliegt fort, 3 fort
        int spot = -1;
        double t = 0.0, seen = 0.0, unseen = 0.0, seenNeed = 1.4;
        bool wasSeen = false, told = false;
        vec3 pos, vel;
        float yaw = 0.0f;
        Part body, head, wingIn[2], wingOut[2], tail, shin[2], tars[2], foot[2];
    };

    struct Shop {
        bool present = false;
        int state = 0;                     // 0 lauert, 1 Box, 2 Gabe (Arm), 3 fort
        int spot = -1, victim = -1;
        vec3 pos, screen, boxC, item;
        float yaw = 0.0f, screenYaw = 0.0f, half = 1.0f;
        double t = 0.0, glitchT = 2.0, burst = 0.0, cool = 0.0, hintT = 0.0;
        bool hinted = false, killed = false;
        float slice[4] = {0, 0, 0, 0};
        Part block[4], screenImg, armA, armB, hand, edge, handle, offer;
    };

    struct Relic {
        vec3 pos;
        float yaw = 0.0f;
        std::string id;
        double creakT = 20.0;
    };

    struct Addict {
        bool on = false;
        int eaten = 0;
        double sobT = 9.0, withdrawT = 0.0;
        bool macchinaSeen = false;
    };

    // Karte und Einstellungen der Ebene lesen (setzt alles ausser den Sucht-Zustaenden zurueck)
    void setup(const world::LevelDef& level, u64 seed);
    const Config& config() const { return cfg_; }
    Config& config() { return cfg_; }
    bool any() const { return cfg_.cuisine || cfg_.swindle || cfg_.photoshop || cfg_.macchina; }
    bool addicted(int id) const { return add_[(size_t)id].on; }

    // ein Bild: players[0] dieser Rechner, beim Host 1..3 die Gaeste; viewers: alle Blicke (Sichtung, Swindlebird)
    void update(double dt, const std::array<PlayerIn, kPlayers>& players, const std::vector<Viewer>& viewers, bool authoritative,
                world::World& w, std::vector<Event>& out);

    // Cannibal Cuisine: Taste druecken (Host/Einzelspieler; false: leer oder beschaeftigt - Meldung in msg)
    bool canVend(int idx, std::string& msg) const;
    bool vend(int idx, int player, std::string& msg, std::vector<Event>& out);
    int cuisineAt(vec3 eye, vec3 look, float range) const;  // Automat im Blick (-1 keiner)
    // gegessen (nach dem Verbrauch der Ware); Rueckgabe: Zusatzmeldung
    std::string eat(int id);
    // nimmt der Koerper dieses Item nicht mehr an? (suechtig: Mandelwasser, Riegel)
    bool refuses(int id, const std::string& itemKey) const;
    double sanityDrain(int id) const;  // zusaetzlich je s (Entzug)
    // Beil: trifft der Schwung einen Automaten oder Photoshop? (Host; 2 = Treffer wie goblinSwing)
    int swing(int player, vec3 eye, vec3 look, std::vector<Event>& out);

    // Photoshop: will er eine Gabe? (Host) Ort und Halbmesser, in dem liegende Gegenstaende zaehlen
    bool wantsOffering(vec3& center, float& radius) const;
    void offered(vec3 itemPos, const std::string& itemModel, std::vector<Event>& out);  // ein Gegenstand wurde genommen

    // Koop (Gast): Ereignis des Hosts anwenden; Gabe: Modell des genommenen Gegenstands (die Sitzung kennt es)
    void applyNet(u32 code, vec3 pos, const std::string& itemModel, std::vector<Event>& out);
    static constexpr const char* kNetName = "l2rand";
    static u32 netKind(u32 code) { return code >> 24; }

    // Skript/Test: Zustaende erzwingen
    void forceBird(int spot);
    void scheduleBird(double in) { sbDue_ = clock_ + in; }  // Test: die Sichtung in so vielen Sekunden ansetzen
    void forceShop(int spot);
    void setState(const std::string& field, double value);

    // Bild
    void collect(std::vector<VisibleMesh>& out, vec3 eye);
    struct Light {
        vec3 pos, color;
        float range = 6.0f;
        vec3 dir{0.0f, 0.0f, -1.0f};
    };
    void lights(std::vector<Light>& out, vec3 eye) const;

    const std::vector<Cuisine>& cuisines() const { return cc_; }
    std::vector<Cuisine>& cuisines() { return cc_; }
    const std::vector<vec3>& swindleSpots() const { return sbSpots_; }
    const std::vector<float>& swindleYaws() const { return sbYaw_; }
    const Bird& bird() const { return bird_; }
    const Shop& shop() const { return shop_; }
    int shopSpots() const { return (int)psSpots_.size(); }
    const std::vector<Relic>& relics() const { return relics_; }
    double swindleDue() const { return sbDue_; }

    Json toJson(int id) const;
    void fromJson(int id, const Json& j);
    void resetPlayer(int id) { add_[(size_t)id] = Addict{}; }
    Addict& addict(int id) { return add_[(size_t)id]; }
    const Addict& addict(int id) const { return add_[(size_t)id]; }

private:
    struct PsSpot {
        vec3 pos, screen;
        float yaw = 0.0f, screenYaw = 0.0f;
    };
    Config cfg_;
    std::vector<Cuisine> cc_;
    std::vector<vec3> sbSpots_;
    std::vector<float> sbYaw_;
    std::vector<PsSpot> psSpots_;
    std::vector<Relic> relics_;
    std::array<Addict, kPlayers> add_{};
    Bird bird_;
    Shop shop_;
    double clock_ = 0.0, sbDue_ = -1.0;
    bool hold_ = false;  // Skript (Bilder): keine Entscheidungen - Haltung halten, nichts toetet, nichts fliegt fort
    Rng rng_{1};
    void updateCuisine(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, std::vector<Event>& out);
    void updateBird(double dt, const std::array<PlayerIn, kPlayers>& pl, const std::vector<Viewer>& viewers, bool auth,
                    world::World& w, std::vector<Event>& out);
    void updateShop(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, std::vector<Event>& out);
    void updateRelics(double dt, const std::array<PlayerIn, kPlayers>& pl, std::vector<Event>& out);
    void startAnger(Cuisine& c, int target, std::vector<Event>& out);
    void startBox(int victim, vec3 at, std::vector<Event>& out);
    void birdGone(std::vector<Event>& out);
    bool birdSeen(const std::vector<Viewer>& viewers, world::World& w) const;
    void setupParts();
};

}  // namespace lim::game
