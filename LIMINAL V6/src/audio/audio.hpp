// Audio: Ausgabe ueber WASAPI (Teil von Windows), eigener Mischer-Thread,
// raeumliche Klangquellen und prozedural erzeugte Klaenge.
//
// Alle Klaenge entstehen beim Start per Synthese (Brummen der Leuchtstoff-
// roehren, Schritte je Untergrund, Automaten, Atmosphaere der Zonen ...).
// Liegt unter data/sounds/<name>.wav eine Datei, ersetzt sie den Klang.
// Ohne Audiogeraet laeuft das Spiel stumm weiter.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"

namespace lim {
class JobSystem;
}

namespace lim::audio {

enum class Bus { Effects, Ambience, Ui };

struct Sound {
    std::vector<float> samples;  // mono, 48 kHz
    bool loop = false;
};

// V6: Raumakustik des Raums, in dem der Hoerer steht (Nachhall ueber einen Hall-Bus).
struct Acoustics {
    float t60 = 0.6f;       // Nachhallzeit (s)
    float damping = 0.35f;  // Hoehendaempfung im Nachhall (0 hart .. 1 sehr weich)
    float wet = 0.18f;      // Pegel des Nachhalls
    float preDelay = 0.012f;  // Abstand bis zu den ersten Reflexionen (s)
};

// V6: raeumliche Eigenschaften einer Quelle
struct Spatial {
    vec3 pos;
    float radius = 10.0f;
    float occlusion = 0.0f;  // 0 freie Sicht .. 1 hinter Waenden (leiser, dumpfer)
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool init(JobSystem& jobs, const std::string& dataDir);
    // Tests: Klaenge erzeugen und mischen ohne Audiogeraet (mixForTest)
    void initOffline(JobSystem& jobs);
    const Sound* sound(const std::string& name) const { return find(name); }
    bool available() const { return running_; }

    // Einmalige Klaenge
    void play(const std::string& name, float volume = 1.0f, float pitch = 1.0f, Bus bus = Bus::Effects);
    void playAt(const std::string& name, vec3 pos, float volume = 1.0f, float radius = 12.0f, float pitch = 1.0f,
                float occlusion = 0.0f);
    // Dauerklaenge: id >= 0 bei Erfolg
    int startLoop(const std::string& name, float volume, Bus bus = Bus::Ambience);
    void setLoopVolume(int id, float volume);
    void setLoopPosition(int id, vec3 pos, float radius);
    void setLoopSpatial(int id, const Spatial& s);  // V6: Position, Reichweite, Verdeckung
    void setLoopPitch(int id, float pitch);
    void setLoopFadeIn(int id, float seconds);       // V6: weich einblenden statt einsetzen
    void setLoopSend(int id, float send);            // V6: Anteil im Nachhall des Raums
    void stopLoop(int id);
    void stopAll();
    bool loopActive(int id);

    void setListener(vec3 pos, float yaw);
    void setVolumes(float master, float effects, float ambience);
    void setAcoustics(const Acoustics& a);  // V6: Nachhall des aktuellen Raums (wird weich ueberblendet)
    // Alle Klaenge stoppen/fortsetzen (Pause des Spiels)
    void setPaused(bool paused);

    // Mischt frames Stereo-Bilder (fuer Tests ohne Audiogeraet)
    void mixForTest(float* out, int frames) { mix(out, frames); }

private:
    struct Voice {
        int id = -1;
        const Sound* sound = nullptr;
        double pos = 0.0;
        float pitch = 1.0f;
        float volume = 1.0f, curGainL = 0, curGainR = 0;
        bool loop = false, positional = false, active = false;
        vec3 at;
        float radius = 10.0f;
        Bus bus = Bus::Effects;
        float fade = 1.0f;  // Ausblenden beim Stoppen
        bool stopping = false;
        // V6: Raumklang
        float occlusion = 0.0f;
        float lpState = 0.0f, lpStateR = 0.0f, lpState2 = 0.0f, lpStateR2 = 0.0f, lpCoef = 0.0f;  // Tiefpass (Verdeckung, Luftdaempfung)
        float itd = 0.0f, curItd = 0.0f;      // Laufzeitunterschied zwischen den Ohren (Samples, >0: rechts spaeter)
        float send = 0.0f;                    // Anteil in den Hall
        float fadeIn = 1.0f, fadeInStep = 0.0f;
    };
    // Nachhall nach Schroeder/Moorer (Freeverb-Aufbau): je Kanal 8 Kammfilter + 4 Allpaesse
    struct Reverb {
        struct Comb {
            std::vector<float> buf;
            size_t idx = 0;
            float store = 0.0f;
        };
        struct Allpass {
            std::vector<float> buf;
            size_t idx = 0;
        };
        Comb comb[2][8];
        Allpass ap[2][4];
        std::vector<float> pre;
        size_t preIdx = 0;
        float fb[8] = {}, damp = 0.3f, wet = 0.0f, preDelay = 0.01f;
        Acoustics target, cur;
        bool ready = false;
        void init(int rate);
        void update(int rate, int frames);
        void process(const float* in, float* outLR, int frames, int rate);
    };
    void thread();
    void mix(float* out, int frames);
    int allocVoice();
    const Sound* find(const std::string& name) const;
    Reverb reverb_;
    std::vector<float> send_;

    std::unordered_map<std::string, Sound> sounds_;
    std::vector<Voice> voices_;
    std::mutex mutex_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    int nextId_ = 1;
    vec3 listener_;
    float listenerYaw_ = 0.0f;
    float master_ = 0.8f, effects_ = 1.0f, ambience_ = 0.8f;
    bool paused_ = false;
    int sampleRate_ = 48000;
};

// Klangerzeugung (auch fuer Tests nutzbar)
std::unordered_map<std::string, Sound> synthesizeAll(JobSystem& jobs, int rate);

}  // namespace lim::audio
