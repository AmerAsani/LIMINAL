// V13.3 Teil 4 Level 5.3 (KREATUR_Gentleman.md, KREATUR_Bibliothekswissen.md; Wikidot level-5-3 Rev. 40, entity-18 Rev. 54,
// entity-141 Rev. 13, entity-189 Rev. 27 - CC BY-SA 3.0): Orte und Funde der Bibliothek.
//
// Der Hausherr (Rolle Gentleman, presence_gentleman.cpp) kommt durch den alten Gang in die Halle; die Karte nennt dafuer zwei
// Orte (Spawnpunkte "trace", Satz "gm": Tag "door" - Gang hinter der alten Tuer, Tag "stand" - sein Platz in der Halle mit
// Blickrichtung, rechts von ihm ein Regal). Das Notizbuch, das er hinter sich wirft, wird dort, wo es landet, ein Fund
// (Notiz "Notizbuch mit Skizzen"); nach seinem Besuch stuerzen Buecher aus dem Regal (Zorn der Bibliothek).
// Wissen statt Wesen (Wiki: die Buecher der Bibliothek kennen u. a. die roten Lichter und das Volk in den Anzuegen - beide sind
// nicht anwesend): ein Folio auf einem Lesepult und Karteikarten auf einem Katalogschrank (Satz "lore", Tags "e141"/"e189").
// Klaenge und Saetze: Klangnummern ab kGentCueBase (Presence::Event Mimic; die Gaeste hoeren sie ueber kinNetRoute).
#pragma once

#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"

namespace lim::world {
struct LevelDef;
}

namespace lim::game {

struct KinLine;

namespace lib53 {
constexpr float kGentCueBase = 800.0f;  // kKinCue (100) + 700
enum GentCue : int {
    GM_DOOR,       // am Ende des alten Gangs geht eine Tuer (vor Sicht)
    GM_SIGH,       // ein langer Seufzer durch die Nase
    GM_MUTTER,     // er erkennt den Ort
    GM_BOOKDROP,   // das Notizbuch faellt aus dem Regal vor seine Fuesse
    GM_PAGES,      // er blaettert
    GM_THROW,      // er wirft es hinter sich
    GM_LAND,       // es schlaegt auf und rutscht
    GM_SPEAK0, GM_SPEAK1,
    GM_HUSH,       // Stille - die Bibliothek antwortet nicht
    GM_SPEAK2,
    GM_NOTICE,     // er bemerkt einen Spieler
    GM_HUNT,       // er kommt auf ihn zu
    GM_ANNOUNCE,   // der Arm entfaltet sich - gleich greift er zu
    GM_GRIP,       // der Griff (ganz nah)
    GM_COMPOSE,    // er richtet sich und geht
    GM_LIGHT,      // Licht im Gesicht
    GM_CRACK,      // der Firnis seines Gesichts knistert, ein Riss oeffnet sich
    GM_GONE,       // weit hinten schliesst sich die Tuer
    GM_WRATH,      // Zorn der Bibliothek: Holz aechzt, Buecher stuerzen
    GM_CREAK,      // die Regale knarren, als er eintritt (die Bibliothek bemerkt ihn)
    GM_DICTA,      // das Diktiergeraet am Lesepult (rote Lichter: eine bekannte Stimme, zu glatt)
    GM_COUNT
};
const KinLine* gentLine(int cue);
const KinLine* gentLineBySound(const std::string& s);
}  // namespace lib53

class Library53 {
public:
    struct Lore {
        vec3 pos;
        float yaw = 0.0f;
        std::string id, title, model, prompt;
    };
    void setup(const world::LevelDef& level);
    bool any() const { return scene_ || !lore_.empty(); }
    bool scene() const { return scene_; }
    vec3 door() const { return door_; }
    vec3 stand() const { return stand_; }
    float standYaw() const { return yaw_; }
    vec3 landing() const;  // wo das geworfene Notizbuch liegt (aus anim::gm::bookLand)
    vec3 pileAt() const;   // wo die Buecher aus dem Regal stuerzen (Zorn)
    const std::vector<Lore>& lore() const { return lore_; }
    static constexpr const char* kNotebook = "Notizbuch mit Skizzen";
    static constexpr const char* kNotebookId = "trace:l53_notebook";

    // Zustand der Sitzung (je Ebene)
    bool thrown = false, provoked = false, wrath = false, dictaPlayed = false;
    float hush = 0.0f, prevBook = 0.0f;

private:
    bool scene_ = false;
    vec3 door_, stand_;
    float yaw_ = 0.0f;
    std::vector<Lore> lore_;
};

}  // namespace lim::game
