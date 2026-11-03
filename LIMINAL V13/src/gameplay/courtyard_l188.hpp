// V13.3 Teil 4 Level 188 (KREATUR_Wurzelpistole.md, KREATUR_Horcher.md; Wikidot level-188 Rev. 69, entity-39 Rev. 46 -
// CC BY-SA 3.0): Orte der Wesen im Hof der Fenster und die Wurzelpistole (Entity 39) als Weltobjekt.
//
// Wiki: Das Grün zwischen den Wegen ist lebendes Moos; Besucher gaben Proben ab, nach der Untersuchung gingen sie an einen Kult
// (Amor Incrementum), der sich mit Gegengaben bedankte - das Wort "Gaben" verweist auf Entity 39: pflanzliche Wesen in Gestalt
// von Schusswaffen, eine Schale aus Mineralien und Metall, Wurzeln am Griff, die sich beim Herausziehen um Hand und Unterarm
// schlingen und Blut ziehen (daraus die Munition; Feuersalz statt Pulver); ein Schuss kann als Same keimen und durch fast jeden
// Boden wurzeln; unberuehrt verwelken sie nach etwa zwei Wochen; ohne Eisen im Boden bleibt die Schale schwach und versagt.
// Auslegung (Briefing): Eine der Gaben kam mit Besuchern in den Hof; ein Schuss keimte im Moos, einer in einem Riss des
// Gangbodens (Blut genug). Ohne Eisen im Boden sind beide verkuemmert und lassen sich nicht herausziehen. Wer sie anfasst,
// dem schlingen sich die Wurzeln um den Arm, es trinkt (Blut, Muedigkeit); die volle Kammer loest sich von selbst - ein Knall,
// der durch Hof und Gaenge hallt (Laerm: der Horcher hoert ihn), ein neuer Keim im Moos. Danach welkt sie eine Weile.
// Spuren der Gaben: drei ausgestochene Moosproben mit Staeben und ein Probenzettel (Fund, eigene Worte).
//
// Orte: Spawnpunkte der Karte mit kind "trace" (tools/levels/level188.py): set "e39" (Tag moss/floor: eine Wurzelpistole;
// Tag note: der Probenzettel), set "hr" (Tag knock: vor einer verschlossenen Tuer; Tag emerge: hinter einer Zellentuer).
// Spielentscheidendes (Griff, Blut, Schuss, Keim) rechnet der Host; die Gaeste bekommen es als Weltereignis Dropped "l188"
// mit einer Kennzahl (Protokoll unveraendert) und rechnen daraus Bild und Klang.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"
#include "core/rng.hpp"
#include "gameplay/visible.hpp"

namespace lim::world {
struct LevelDef;
}

namespace lim::game {

class Courtyard188 {
public:
    static constexpr int kPlayers = 4;
    static constexpr int kTendrils = 6, kSegs = 7;
    static constexpr float kSegLen = 0.075f;  // Glied einer Wurzel (models_l188_gifts.py: e39_root)
    // Masse der Wurzelpistole (models_l188_gifts.py, Ursprung unten am Griff, Lauf entlang +x)
    static constexpr float kGripTop = 0.105f;
    static constexpr float kMuzzleX = 0.158f, kMuzzleZ = 0.142f;
    static constexpr const char* kNetName = "l188";
    static constexpr const char* kNote = "Probenzettel aus dem Hof";

    struct Part {
        std::string model;
        int id = -2;
    };
    struct Gun {
        vec3 pos;
        float yaw = 0.0f, lean = 0.22f;
        int kind = 0;               // 0 im Moos (Hof), 1 im Riss des Gangbodens
        std::string id;             // stabile Kennung (Bedienfeld)
        int state = 0;              // 0 ruht, 1 Griff (trinkt), 2 Schuss, 3 welk
        double t = 0.0, cool = 0.0, dripT = 4.0, creakT = 0.0, flashT = -1.0;
        int victim = -1, drank = 0;
        float sense = 0.0f, senseV = 0.0f, droop = 0.0f, droopV = 0.0f, wrap = 0.0f, wrapV = 0.0f;
        vec3 senseAt, hand;
        double huskT = -1.0;        // ein Tropfen faellt (s seit dem Loesen)
        Part body, bed, seg[kTendrils][kSegs], husk;
    };
    struct Sprout {
        vec3 pos;
        float yaw = 0.0f;
        double age = 0.0;
        Part part;
    };
    struct Event {
        enum Kind { Toast, SoundAt, Hurt, Tired, Noise, Net } kind = Toast;
        int player = 0;      // Toast/Hurt/Tired: wen es trifft
        std::string text;    // Meldung bzw. Klang
        vec3 pos;
        float value = 1.0f;  // Lautstaerke, Schaden, Dauer (s), Laerm
        u32 code = 0;
    };
    struct PlayerIn {
        vec3 feet;
        bool alive = false;
    };
    struct Light {
        vec3 pos, color;
        float range = 6.0f;
        vec3 dir{0.0f, 0.0f, -1.0f};
    };

    void setup(const world::LevelDef& level, u64 seed);
    bool any() const { return !guns_.empty() || !knock_.empty(); }
    // ein Bild: players[0] dieser Rechner, beim Host 1..3 die Gaeste; authoritative: Einzelspieler/Host
    void update(double dt, const std::array<PlayerIn, kPlayers>& players, bool authoritative, std::vector<Event>& out);
    // anfassen (Host/Einzelspieler): false - welk oder schon im Griff (msg sagt warum)
    bool touch(int idx, int player, vec3 eye, std::string& msg, std::vector<Event>& out);
    // Koop (Gast): Ereignis des Hosts anwenden
    void applyNet(u32 code, vec3 pos, std::vector<Event>& out);
    static u32 netKind(u32 code) { return code >> 24; }
    // Skript/Test
    bool setState(const std::string& key, double v);

    void collect(std::vector<VisibleMesh>& out, vec3 eye);
    void lights(std::vector<Light>& out, vec3 eye) const;
    vec3 muzzle(const Gun& g) const;

    std::vector<Gun>& guns() { return guns_; }
    const std::vector<Gun>& guns() const { return guns_; }
    const std::vector<Sprout>& sprouts() const { return sprouts_; }
    // der Probenzettel (Fund) und die Orte des Horchers
    bool noteAt(vec3& pos, float& yaw) const;
    const std::vector<vec3>& knockSpots() const { return knock_; }
    const std::vector<vec3>& emergeSpots() const { return emerge_; }
    const std::vector<float>& emergeYaws() const { return emergeYaw_; }
    float tired(int id) const { return id >= 0 && id < kPlayers && tired_[(size_t)id] > 0.0f ? 0.86f : 1.0f; }  // Tempo-Faktor
    float& tiredLeft(int id) { return tired_[(size_t)std::clamp(id, 0, kPlayers - 1)]; }

private:
    std::vector<Gun> guns_;
    std::vector<Sprout> sprouts_;
    std::vector<vec3> knock_, emerge_, mossSpots_;
    std::vector<float> emergeYaw_;
    vec3 notePos_;
    float noteYaw_ = 0.0f;
    bool hasNote_ = false;
    std::array<float, kPlayers> tired_{};
    Rng rng_{1};
    bool hold_ = false;
    void fire(Gun& g, int gi, bool authoritative, std::vector<Event>& out);
    void startGrip(Gun& g, int player, std::vector<Event>& out);
    void setupParts();
};

}  // namespace lim::game
