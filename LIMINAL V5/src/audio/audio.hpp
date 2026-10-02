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

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool init(JobSystem& jobs, const std::string& dataDir);
    bool available() const { return running_; }

    // Einmalige Klaenge
    void play(const std::string& name, float volume = 1.0f, float pitch = 1.0f, Bus bus = Bus::Effects);
    void playAt(const std::string& name, vec3 pos, float volume = 1.0f, float radius = 12.0f, float pitch = 1.0f);
    // Dauerklaenge: id >= 0 bei Erfolg
    int startLoop(const std::string& name, float volume, Bus bus = Bus::Ambience);
    void setLoopVolume(int id, float volume);
    void setLoopPosition(int id, vec3 pos, float radius);
    void setLoopPitch(int id, float pitch);
    void stopLoop(int id);
    void stopAll();

    void setListener(vec3 pos, float yaw);
    void setVolumes(float master, float effects, float ambience);
    // Alle Klaenge stoppen/fortsetzen (Pause des Spiels)
    void setPaused(bool paused);

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
    };
    void thread();
    void mix(float* out, int frames);
    int allocVoice();
    const Sound* find(const std::string& name) const;

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
