// V7: Fundstuecke und Notausgaenge.
//
// Notizzettel anderer Wanderer, Batterien und - selten, weit vom Start - Notausgaenge zur
// naechsten Ebene. Alles entsteht allein aus den Daten eines fertigen Raums (Lage, Seed,
// Kacheln, Tueren) ueber Hashwerte: Es verbraucht keine Zufallszahlen des Generators, die Welt
// bleibt bitgenau dieselbe, und jeder Fund liegt nach dem Neuladen wieder am selben Platz.
#pragma once

#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"
#include "world/level_def.hpp"

namespace lim::world {
class Room;
}

namespace lim::game {

struct FindSpot {
    enum Kind { Note, Battery, Exit };
    Kind kind = Note;
    std::string id;      // stabil: "note:sx,sy,raum" usw. (Spielstand: aufgenommen/gelesen)
    vec3 pos;            // Boden (Notiz, Batterie) bzw. Wandebene (Notausgang)
    float yaw = 0.0f;    // Notausgang: zeigt in den Raum
    i64 tx = 0, ty = 0;  // Kachel (entscheidet, mit welchem Chunk der Fund erscheint)
    u64 hash = 0;        // fuer die Auswahl des Notiztextes
};

// Fundstuecke eines (vorbereiteten) Raums; haengt nur von Raum und Level ab
void roomFinds(const world::Room& r, const world::LevelDef& level, std::vector<FindSpot>& out);

}  // namespace lim::game
