// V13.3: Dauerklaenge des Wranglers (KREATUR_Wrangler.md): das Rumpeln in der Tiefe (am Ort des Wesens, so laut wie das
// Beben), das Gleiten des Leibs (am naechsten Ring) und der Sog (ohne Ort, so stark wie der Bann des Spielers).
#include "app/app.hpp"
#include "app/soundscape.hpp"
#include "audio/audio.hpp"
#include "gameplay/session.hpp"

namespace lim::app {

void wranglerLoops(audio::AudioSystem& au, Soundscape& ss, game::GameSession& s, vec3 ear, int gen, bool dead) {
    static int rumbleId = -1, bodyId = -1, droneId = -1, boundGen = -1;
    if (boundGen != gen) boundGen = gen, rumbleId = bodyId = droneId = -1;  // neue Sitzung: alles schon gestoppt
    vec3 ra, ba;
    float rv = 0.0f, bv = 0.0f;
    const bool on = !dead && s.presence().wranglerSound(ear, ra, rv, ba, bv);
    auto loop = [&](int& id, const char* name, bool want, float fadeIn) {
        if (want && id < 0) {
            id = au.startLoop(name, 0.0f, audio::Bus::Creatures);
            au.setLoopFadeIn(id, fadeIn);
        }
        if (!want && id >= 0) au.stopLoop(id), id = -1;
        return id >= 0;
    };
    if (loop(rumbleId, "wrangler_rumble", on && rv > 0.02f, 1.5f)) {
        au.setLoopSpatial(rumbleId, ss.spatial(s.world(), ear, ra, 70.0f, 4.0f));
        au.setLoopVolume(rumbleId, std::min(1.0f, 0.25f + 0.9f * rv));
        au.setLoopPitch(rumbleId, 0.9f + 0.15f * rv);
    }
    if (loop(bodyId, "wrangler_slide", on && bv > 0.02f, 1.0f)) {
        au.setLoopSpatial(bodyId, ss.spatial(s.world(), ear, ba, 30.0f, 2.0f));
        au.setLoopVolume(bodyId, 0.85f * bv);
    }
    const float tr = dead ? 0.0f : s.presence().trance();
    if (loop(droneId, "wrangler_drone", tr > 0.03f, 2.0f)) au.setLoopVolume(droneId, 0.7f * tr);
}

}  // namespace lim::app
