// Audio: Ausgabe ueber WASAPI (Teil von Windows), eigener Mischer-Thread,
// raeumliche Klangquellen und prozedural erzeugte Klaenge.
//
// Alle Klaenge entstehen beim Start per Synthese (audio/synth*.cpp). Liegt unter
// data/sounds/<name>.wav eine Datei, ersetzt sie den Klang; <name>_<n>.wav ergaenzt Varianten.
// Ohne Audiogeraet laeuft das Spiel stumm weiter.
//
// V11: neuer Mischer (siehe AUDIO_DESIGN.md)
//   - Abstand nach 1/d ab einer Referenzdistanz, weich ausgeblendet bis zur Reichweite;
//     Luftdaempfung (ferne Quellen verlieren Hoehen)
//   - Richtung: Laufzeitdifferenz nach Woodworth, frequenzabhaengiger Kopfschatten je Ohr,
//     leiser in den Hoehen von hinten
//   - Verdeckung und Umweg (Schallweg durch Tueren, app/sound_propagation)
//   - Raumhall: Fruehreflexionen aus den Raummassen + Feedback-Delay-Netzwerk mit drei
//     Nachhallzeiten; je weiter eine Quelle, desto groesser ihr Hallanteil
//   - Busse (Effekte, Atmosphaere, Kreaturen, Oberflaeche), Ducking der Atmosphaere,
//     Begrenzer mit Vorausschau statt Dauersaettigung
//   - Varianten: "step_concrete" waehlt zufaellig unter step_concrete_0..n (nie zweimal dieselbe),
//     dazu leichte Streuung von Tonhoehe und Pegel
#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "audio/dsp.hpp"
#include "core/core.hpp"
#include "core/math.hpp"

namespace lim {
class JobSystem;
}

namespace lim::audio {

enum class Bus { Effects, Ambience, Ui, Creatures };
constexpr int kBusCount = 4;

struct Sound {
    std::vector<float> samples;  // mono
    bool loop = false;
    int rate = 48000;  // V11: eigene Abtastrate (z. B. 24 kHz fuer tiefe Klangbetten)
};

// Raumakustik des Raums, in dem der Hoerer steht (wird weich ueberblendet).
struct Acoustics {
    float t60 = 0.6f;         // Nachhallzeit Mitte (s)
    float damping = 0.35f;    // Hoehendaempfung (0 hart .. 1 sehr weich); ergibt t60High, falls nicht gesetzt
    float wet = 0.18f;        // Pegel des Nachhalls
    float preDelay = 0.012f;  // bis zum Einsatz der Fahne (s)
    // V11
    float t60Low = -1.0f, t60High = -1.0f;  // < 0: aus t60 und damping
    float size = 1.0f;                       // Leitungslaengen (0.4 kleiner Raum .. 2.5 Halle)
    float diffusion = 0.7f;
    float gate = 0.0f;                       // > 0: Fahne bricht nach so vielen s ab (Level 37)
    float early = 0.6f;                      // Pegel der Fruehreflexionen (relativ zu wet)
    float roomW = 0, roomD = 0, roomH = 0;   // Raummasse (m) fuer die Fruehreflexionen (0: keine)
    float lx = 0, ly = 0, lz = 0;            // Hoerer im Raum (m ab der Ecke x0/y0, Boden)
    float absorption = 0.2f;                 // mittlerer Schluckgrad der Flaechen
};

// raeumliche Eigenschaften einer Quelle
struct Spatial {
    vec3 pos;
    float radius = 10.0f;     // Reichweite (dahinter stumm)
    float occlusion = 0.0f;   // 0 freie Sicht .. 1 hinter Waenden (leiser, dumpfer)
    float refDist = 1.0f;     // V11: bis hierhin volle Lautstaerke, danach 1/d
    float pathExtra = 0.0f;   // V11: Umweg des Schalls (m) gegenueber der geraden Linie
};

struct PlayOpts {
    float volume = 1.0f, pitch = 1.0f;
    Bus bus = Bus::Effects;
    bool vary = true;      // Streuung von Tonhoehe (+-3 %) und Pegel (+-1,5 dB)
    float send = -1.0f;    // Anteil im Hall (< 0: nach Bus)
    float refDist = 1.0f;
    int priority = 1;      // hoeher = wird spaeter verdraengt
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool init(JobSystem& jobs, const std::string& dataDir);
    // Tests: Klaenge erzeugen und mischen ohne Audiogeraet (mixForTest)
    void initOffline(JobSystem& jobs);
    // Klang nach Name; fehlt er, die erste Variante (name_0)
    const Sound* sound(const std::string& name) const { return find(name); }
    bool available() const { return running_; }
    // Namen aller Klaenge (Export, Tests)
    std::vector<std::string> names() const;
    int variants(const std::string& base) const;
    // Variante waehlen wie beim Abspielen (Tests)
    const Sound* pickVariant(const std::string& name) {
        std::lock_guard lock(mutex_);
        return pick(name);
    }

    // Einmalige Klaenge
    void play(const std::string& name, float volume = 1.0f, float pitch = 1.0f, Bus bus = Bus::Effects);
    void playAt(const std::string& name, vec3 pos, float volume = 1.0f, float radius = 12.0f, float pitch = 1.0f,
                float occlusion = 0.0f);
    // V11: mit allen Angaben (Varianten, Bus, Umweg ...)
    void play2d(const std::string& name, const PlayOpts& o);
    void play3d(const std::string& name, const Spatial& s, const PlayOpts& o);
    // Dauerklaenge: id >= 0 bei Erfolg
    int startLoop(const std::string& name, float volume, Bus bus = Bus::Ambience);
    void setLoopVolume(int id, float volume);
    void setLoopPosition(int id, vec3 pos, float radius);
    void setLoopSpatial(int id, const Spatial& s);
    void setLoopPitch(int id, float pitch);
    void setLoopFadeIn(int id, float seconds);
    void setLoopSend(int id, float send);
    void stopLoop(int id);
    void stopAll();
    bool loopActive(int id);

    void setListener(vec3 pos, float yaw);
    void setVolumes(float master, float effects, float ambience);
    void setBusVolume(Bus b, float v);  // V11
    void setAcoustics(const Acoustics& a);
    void setPaused(bool paused);
    // V11: Unterwasser / Ohnmacht - alles dumpf (0 aus .. 1 ganz)
    void setMuffle(float m);

    // Mischt frames Stereo-Bilder (fuer Tests ohne Audiogeraet)
    void mixForTest(float* out, int frames) { mix(out, frames); }
    // V11: Reduktion des Begrenzers im letzten Block (Tests: 1 = keine)
    float limiterGain() const { return limiter_.reduction(); }

private:
    struct Voice {
        int id = -1;
        const Sound* sound = nullptr;
        double pos = 0.0;
        float pitch = 1.0f;
        float volume = 1.0f;
        float curGainL = 0, curGainR = 0;
        bool loop = false, positional = false, active = false;
        vec3 at;
        float radius = 10.0f, refDist = 1.0f, pathExtra = 0.0f;
        Bus bus = Bus::Effects;
        int priority = 1;
        float fade = 1.0f;
        bool stopping = false;
        float occlusion = 0.0f;
        // mono: zwei Einpol-Tiefpaesse (Luft, Verdeckung)
        float lp1 = 0, lp2 = 0, lpCoef = 0, lpCoefCur = 0;
        // Ohren: Laufzeit (Samples) und Kopfschatten (Hoehenanteil je Ohr)
        float itdL = 0, itdR = 0, curItdL = 0, curItdR = 0;
        float hfL = 1, hfR = 1, curHfL = 1, curHfR = 1, shL = 0, shR = 0;
        std::array<float, 64> ring{};
        int ringPos = 0;
        float send = 0.0f, sendCur = 0.0f;
        float fadeIn = 1.0f, fadeInStep = 0.0f;
        float audible = 0.0f;  // fuer das Verdraengen
    };
    void thread();
    void mix(float* out, int frames);
    int allocVoice(int priority);
    const Sound* find(const std::string& name) const;
    const Sound* pick(const std::string& name);  // mit Variantenwahl
    std::unordered_set<std::string> missing_;     // V11: schon gemeldete unbekannte Namen
    void startVoice(Voice& v, const Sound* s, float volume, float pitch, Bus bus, int priority);
    void updateRoom();

    std::unordered_map<std::string, Sound> sounds_;
    std::unordered_map<std::string, std::vector<const Sound*>> groups_;
    std::unordered_map<std::string, int> lastVariant_;
    std::vector<Voice> voices_;
    std::mutex mutex_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    int nextId_ = 1;
    vec3 listener_;
    float listenerYaw_ = 0.0f;
    float master_ = 0.8f;
    float bus_[kBusCount] = {1.0f, 0.8f, 1.0f, 1.0f};
    bool paused_ = false;
    float muffle_ = 0.0f, muffleCur_ = 0.0f, muffleZ_[2] = {0, 0};
    int sampleRate_ = 48000;
    dsp::Rand rand_{0xA0D10u};

    // Raum
    dsp::Fdn fdn_;
    dsp::EarlyReflections early_;
    Acoustics target_, cur_;
    bool roomDirty_ = true;
    float roomTimer_ = 0.0f;
    std::vector<float> send_, wetL_, wetR_, erL_, erR_, busL_[kBusCount], busR_[kBusCount];
    float duckEnv_ = 0.0f, duck_ = 1.0f;
    float dcZ_[2] = {0, 0}, dcX_[2] = {0, 0};
    dsp::Limiter limiter_;
};

// Klangerzeugung (auch fuer Tests nutzbar)
std::unordered_map<std::string, Sound> synthesizeAll(JobSystem& jobs, int rate);

}  // namespace lim::audio
