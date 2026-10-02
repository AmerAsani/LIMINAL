// V6: Klanglandschaft - alles, was die Umgebung von sich aus hoeren laesst.
//
//   Leuchten:   Jede Leuchte der Umgebung ist eine eigene raeumliche Quelle (Leuchtstoffroehren
//               summen, Industrieleuchten brummen tiefer, Notleuchten schnarren). Die naechsten,
//               lautesten werden mit Stimmen belegt, weich ein- und ausgeblendet; flackernde
//               Roehren knacken bei Einbruechen. Hinter Waenden klingen sie dumpf.
//   Raumklang:  Nachhallzeit nach Sabine aus Volumen, Flaeche und Materialien des Raums
//               (Teppich und Akustikdecke schlucken, Beton und Fliesen hallen).
//   Fernes:     sehr selten ein Klang irgendwo weit weg im Gebaeude, passend zur Zone -
//               haeufiger, je mehr der Verstand schwindet.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "audio/audio.hpp"
#include "core/rng.hpp"

namespace lim::world {
class World;
class Room;
}  // namespace lim::world

namespace lim::app {

class Soundscape {
public:
    // listener: Ohrposition; room: aktueller Raum (darf nullptr sein); sanity 0..100
    void update(double dt, double time, audio::AudioSystem& audio, world::World& world, const world::Room* room,
                vec3 listener, double sanity, bool active);
    void reset(audio::AudioSystem& audio);  // neue Sitzung: alle Stimmen beenden
    // Verdeckung zwischen zwei Punkten durch die Kachelwelt (0 frei .. 1 verdeckt), nur geladene Chunks
    static float occlusion(world::World& world, vec3 from, vec3 to);
    int activeLamps() const { return (int)lamps_.size(); }
    // V7: Stromausfall - Leuchten dieses Flacker-Seeds verstummen (amount 0..1)
    void setBlackout(int seed, float amount) { boSeed_ = seed, boAmount_ = amount; }
    const audio::Acoustics& acoustics() const { return acoustics_; }

private:
    struct Lamp {
        int voice = -1;
        vec3 pos;
        int kind = 0;
        u8 flickerMode = 0, flickerSeed = 0;
        float level = 0.0f, occ = 0.0f, occTarget = 0.0f, lastFlicker = 1.0f, tickCooldown = 0.0f;
        bool wanted = false;
    };
    void updateLamps(double dt, double time, audio::AudioSystem& audio, world::World& world, vec3 listener);
    void updateAcoustics(world::World& world, const world::Room* room, audio::AudioSystem& audio);
    void updateDistant(double dt, audio::AudioSystem& audio, world::World& world, const world::Room* room,
                       vec3 listener, double sanity);

    std::unordered_map<u64, Lamp> lamps_;
    double scanTimer_ = 0.0;
    int bedId_ = -1;
    float bed_ = 0.0f;
    audio::Acoustics acoustics_;
    const world::Room* acousticRoom_ = nullptr;
    double nextDistant_ = 40.0;
    Rng rng_{0x5EED5u};
    int boSeed_ = -1;
    float boAmount_ = 0.0f;
};

}  // namespace lim::app
