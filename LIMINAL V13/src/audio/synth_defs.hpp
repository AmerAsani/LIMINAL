// V11: Verzeichnis der erzeugten Klaenge. Jede Gruppe (Spieler, Welt, Kreaturen, Ebenen) traegt ihre
// Klaenge mit Name und Rechenvorschrift ein; synthesizeAll() rechnet sie parallel.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "audio/audio.hpp"

namespace lim::audio {

struct SoundDef {
    std::string name;
    std::function<Sound()> make;
};

void addPlayerSounds(std::vector<SoundDef>& d, int rate);    // synth_player.cpp: Schritte, Koerper, Gegenstaende, Oberflaeche
void addWorldSounds(std::vector<SoundDef>& d, int rate);     // synth_world.cpp: Leuchten, Elektrik, Wasser, Rohre, Tueren
void addCreatureSounds(std::vector<SoundDef>& d, int rate);  // synth_creatures.cpp: Kreaturen, Halluzinationen
void addLevelSounds(std::vector<SoundDef>& d, int rate);     // synth_levels.cpp: Klangbetten, ferne Ereignisse, Musik
void addDullerSounds(std::vector<SoundDef>& d, int rate);    // V13.3 synth_duller.cpp: Duller (Wand, Gelenke)
void addArmsSounds(std::vector<SoundDef>& d, int rate);      // V13.3 synth_arms.cpp: Arms (Schacht, Schrei im Licht)
void addWranglerSounds(std::vector<SoundDef>& d, int rate);  // V13.3 synth_wrangler.cpp: Wrangler (Beben, Leib, Kopf, Jungtiere)
void addGrowlerSounds(std::vector<SoundDef>& d, int rate);   // V13.3: synth_growler.cpp
void addNguithrSounds(std::vector<SoundDef>& d, int rate);   // V13.3: synth_nguithrxurh.cpp
void addOuroborosSounds(std::vector<SoundDef>& d, int rate); // V13.3: synth_ouroboros.cpp (Ouroboros-Arbeiter)
void addGoblinSounds(std::vector<SoundDef>& d, int rate);    // V13.3: synth_goblin.cpp (Plague Goblins, Beil)
void addLureSounds(std::vector<SoundDef>& d, int rate);      // V13.3 synth_lure.cpp: Leuchtstab, Ansprechen (Spieler, Skin-Stealer)
void addPhenomenaSounds(std::vector<SoundDef>& d, int rate);  // V13.3 synth_phenomena.cpp: Pilz, Garden, Spielleiterin, Tuerspuren
void addKinL3Sounds(std::vector<SoundDef>& d, int rate);      // V13.3 synth_kin_l3.cpp: Level 3 - Wretch (Waffen, Fetzen), Skin-Stealer (Saetze)

}  // namespace lim::audio
