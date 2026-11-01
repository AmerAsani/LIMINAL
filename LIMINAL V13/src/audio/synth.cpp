// Prozedurale Klangerzeugung: sammelt die Klaenge aller Gruppen (synth_player, synth_world,
// synth_creatures, synth_levels) und rechnet sie parallel. Deterministisch (feste Seeds).
//
// V11: Die Klaenge aus V5-V10 (Sinus plus Rauschen mit Exponentialhuellkurven) sind vollstaendig
// ersetzt. Warum, steht in AUDIO_DESIGN.md.
#include <mutex>

#include "audio/synth_defs.hpp"
#include "core/jobs.hpp"

namespace lim::audio {

std::unordered_map<std::string, Sound> synthesizeAll(JobSystem& jobs, int rate) {
    std::vector<SoundDef> defs;
    addPlayerSounds(defs, rate);
    addWorldSounds(defs, rate);
    addCreatureSounds(defs, rate);
    addLevelSounds(defs, rate);
    addDullerSounds(defs, rate);  // V13.3
    addArmsSounds(defs, rate);
    addWranglerSounds(defs, rate);  // V13.3
    addLureSounds(defs, rate);      // V13.3: Leuchtstab, Ansprechen
    std::vector<Sound> out(defs.size());
    jobs.parallelFor((int)defs.size(), [&](int i) { out[(size_t)i] = defs[(size_t)i].make(); });
    std::unordered_map<std::string, Sound> map;
    for (size_t i = 0; i < defs.size(); ++i) map[defs[i].name] = std::move(out[i]);
    return map;
}

}  // namespace lim::audio
