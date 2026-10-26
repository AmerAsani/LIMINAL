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
//
// V11: Hat das Level einen Abschnitt "audio" (AUDIO_DESIGN.md, Abschnitt 4), bestimmt er das Klangbild:
//   beds        Klangbetten, je nach Raumtyp lauter oder leiser ("only"/"not": Raumtypen)
//   events      Einzelklaenge in zufaelligen Abstaenden, in Entfernung und Hoehe gestreut oder an
//               Stellen der Ausstattung ("where": puddle, car ...); V12: "only" beschraenkt auf Raumtypen
//   spots       Dauerklaenge an Stellen der Ausstattung (Rohre, Schaltkasten, Glut im Bodenloch)
//   acoustics   Nachhall je Raumtyp; die Fruehreflexionen folgen den Raummassen und dem Hoerer
//   hum_surge   V12: das Brummen aller Leuchten schwillt in zufaelligen Abstaenden an und klingt wieder ab
//               ({"every": [s, s], "gain": Faktor, "len": s})
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "audio/audio.hpp"
#include "core/rng.hpp"
#include "gameplay/sound_field.hpp"

namespace lim::world {
class World;
class Room;
struct LevelDef;
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
    // V11: raeumliche Angaben ueber den Weg des Schalls (durch Tueren, um Ecken) statt geradlinig durch die Wand
    audio::Spatial spatial(world::World& world, vec3 ear, vec3 src, float radius, float refDist = 1.0f) const;
    const game::SoundField& field() const { return field_; }
    int activeLamps() const { return (int)lamps_.size(); }
    // V7: Stromausfall - Leuchten dieses Flacker-Seeds verstummen (amount 0..1)
    void setBlackout(int seed, float amount, vec3 origin = {}, float front = -1.0f) {
        boSeed_ = seed, boAmount_ = amount, boOrigin_ = origin, boFront_ = front;
    }
    const audio::Acoustics& acoustics() const { return acoustics_; }
    // V11: Klangbetten gemeinsam leiser (Begegnung, Stromausfall: die Lueftung steht still), 0..1
    void setBedDuck(float d) { bedDuck_ = d; }
    // V11: Stille - ferne Einzelklaenge pausieren (waehrend einer Begegnung und kurz danach)
    void setQuiet(bool q) { quiet_ = q; }
    int activeBeds() const;
    int activeSpots() const { return (int)spotLoops_.size(); }
    // V12: aktueller Faktor des Anschwellens (1 = ruhig)
    float humSurge() const { return surgeNow_; }

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
    void updateAcoustics(double dt, world::World& world, const world::Room* room, vec3 listener, audio::AudioSystem& audio);
    void updateDistant(double dt, audio::AudioSystem& audio, world::World& world, const world::Room* room,
                       vec3 listener, double sanity);

    // V11: Klangbild aus dem Level
    struct Bed {
        std::string sound;
        float gain = 1.0f, level = 0.0f;
        std::vector<std::string> only, notIn;
        int voice = -1;
    };
    struct Event {
        std::string sound, where;
        std::vector<std::string> only;  // V12: nur in diesen Raumtypen (leer: ueberall)
        float every0 = 30, every1 = 60, dist0 = 10, dist1 = 30, h0 = 0, h1 = 0, gain = 0.7f;
        bool height = false;
        double next = 0.0;
    };
    struct SpotKind {
        std::string sound;
        float radius = 8.0f, gain = 0.5f;
        int voices = 2;
    };
    struct SpotLoop {
        int voice = -1;
        std::string kind;
        vec3 pos;
        float level = 0.0f, occ = 0.0f;
        bool wanted = false;
        audio::Spatial sp;  // V11: Weg zum Ohr (beim Abtasten bestimmt)
    };
    void configure(const world::LevelDef& L, audio::AudioSystem& audio);
    void updateBeds(double dt, audio::AudioSystem& audio, const world::Room* room);
    void updateEvents(double dt, audio::AudioSystem& audio, world::World& world, const world::Room* room, vec3 listener);
    void updateSpots(double dt, audio::AudioSystem& audio, world::World& world, vec3 listener);

    std::unordered_map<u64, Lamp> lamps_;
    double scanTimer_ = 0.0;
    audio::Acoustics acoustics_;
    const world::Room* acousticRoom_ = nullptr;
    double acousticTimer_ = 0.0;
    vec3 acousticEar_{};
    double nextDistant_ = 40.0;
    Rng rng_{0x5EED5u};
    int boSeed_ = -1;
    float boAmount_ = 0.0f;
    vec3 boOrigin_{};
    float boFront_ = -1.0f;  // V11: Welle des Stromausfalls

    const world::LevelDef* configured_ = nullptr;
    bool levelAudio_ = false;
    std::vector<Bed> beds_;
    std::vector<Event> events_;
    std::unordered_map<std::string, SpotKind> spotKinds_;
    std::unordered_map<u64, SpotLoop> spotLoops_;
    double spotScan_ = 0.0;
    float bedDuck_ = 0.0f, bedDuckNow_ = 0.0f;
    game::SoundField field_;  // V11: Schallausbreitung vom Ohr aus
    bool quiet_ = false;
    // V12: Anschwellen des Brummens
    bool surge_ = false;
    float surgeEvery0_ = 60.0f, surgeEvery1_ = 160.0f, surgeGain_ = 2.0f, surgeLen_ = 5.0f;
    double surgeNext_ = 0.0, surgeT_ = -1.0;
    float surgeNow_ = 1.0f;
};

}  // namespace lim::app
