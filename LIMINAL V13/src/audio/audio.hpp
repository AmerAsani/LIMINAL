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
//
// V13.3 (AUDIO_BUG.md): "Audio geht manchmal, manchmal nicht"
//   - gestoppte, unhoerbare Schleifen gaben ihre Stimme nie frei (Stimmen liefen voll) - behoben
//   - Geraetewechsel, entferntes Geraet, Formatwechsel, exklusiver Modus: Benachrichtigungen von Windows
//     (IMMNotificationClient, IAudioSessionEvents) und Fehlercodes oeffnen den Strom neu
//   - ohne Geraet mischt ein Ersatztakt weiter (Klaenge laufen ab statt sich zu stauen)
//   - Audiothread mit MMCSS-Prioritaet ("Pro Audio") gegen Unterlaeufe unter Last
//   - Watchdog: steht der Strom, haengt der Thread oder verstummt der Master-Ausgang bei erwartetem
//     Klang, startet die Engine neu (mit Protokolleintrag)
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

// V13.3 (AUDIO_BUG.md): Zustand des Audiosystems fuer Protokoll, Watchdog und Tests
struct AudioStats {
    bool ready = false;       // Klaenge erzeugt
    bool deviceOpen = false;  // WASAPI-Strom laeuft
    std::string device;       // Name des Geraets
    int deviceRate = 0, deviceChannels = 0, deviceBits = 0;  // Mischformat des Geraets
    int streamRate = 0, mixRate = 0, streamChannels = 0;
    unsigned bufferFrames = 0;
    float periodMs = 0.0f;
    unsigned opens = 0, openFailures = 0, losses = 0, restarts = 0;
    long lastHr = 0;  // letzter Fehlercode (HRESULT)
    std::string lastReason;  // Grund des letzten Neustarts
    int voices = 0, loops = 0, stopping = 0, stuck = 0, peakVoices = 0;
    unsigned long long plays = 0, allocFails = 0, dropped = 0, reclaimed = 0, badInput = 0;
    unsigned long long frames = 0, underruns = 0, timeouts = 0, nonFinite = 0, silenceAlarms = 0;
    float outPeak = 0.0f, outRms = 0.0f, expectedRms = 0.0f;  // Master-Ausgang, letzte Sekunde (linear)
    float outRmsFast = 0.0f;                                   // Master-Ausgang, gleitend ueber ~100 ms
    float mixMsMax = 0.0f, lockWaitMsMax = 0.0f;  // laengster Mischvorgang / laengste Wartezeit auf die Sperre
    bool threadHung = false;
};

// V13.3: gezielt ausgeloeste Fehler fuer Tests und den Skriptbefehl audiofault (AUDIO_BUG.md, "Was simuliert ist").
// Jeder Fehler laeuft durch denselben Code wie der echte: Benachrichtigungen ueber die Rueckrufe von
// IMMNotificationClient/IAudioSessionEvents, Fehlercodes an der Stelle, an der WASAPI sie liefern wuerde.
enum class Fault {
    DeviceInvalidated,  // naechster GetCurrentPadding liefert AUDCLNT_E_DEVICE_INVALIDATED
    DefaultChanged,     // OnDefaultDeviceChanged mit anderer Geraete-ID (Kopfhoerer eingesteckt)
    FormatChanged,      // OnSessionDisconnected(DisconnectReasonFormatChanged); param: neue Abtastrate des Stroms (0: 48000)
    ExclusiveOverride,  // OnSessionDisconnected(ExclusiveModeOverride); param ms lang liefert Initialize AUDCLNT_E_DEVICE_IN_USE
    DeviceRemoved,      // OnDeviceStateChanged(UNPLUGGED); param ms lang kein Geraet (E_NOTFOUND), dann OnDeviceStateChanged(ACTIVE)
    StallEvents,        // param ms lang signalisiert der Treiber kein Ereignis
    MuteOutput,         // der Master-Ausgang bleibt stumm, bis die Engine neu startet (haengender Mischerzustand)
    NanOutput,          // NaN im Zustand des Gleichspannungsfilters (vergifteter DSP-Zustand)
    InitFails,          // die naechsten param Aufrufe von Initialize liefern AUDCLNT_E_UNSUPPORTED_FORMAT
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    // Klaenge erzeugen; device: Geraetethread starten (WASAPI)
    bool init(JobSystem& jobs, const std::string& dataDir, bool device = true);
    // Tests: Klaenge erzeugen und mischen ohne Audiogeraet (mixForTest)
    void initOffline(JobSystem& jobs);
    // V13.3 (Tests): die Klangbank eines anderen Systems mitbenutzen (kein erneutes Erzeugen)
    void initShared(const AudioSystem& other);
    // V13.3: Geraetethread starten (nach initOffline/initShared; init macht das selbst)
    void startDevice();
    // Klang nach Name; fehlt er, die erste Variante (name_0)
    const Sound* sound(const std::string& name) const { return find(name); }
    bool ready() const { return ready_; }        // V13.3: Klaenge da, play() nimmt an
    bool available() const { return running_; }  // WASAPI-Strom offen
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
    // V13.3: Pause und Daempfung zuruecksetzen (neue Sitzung, Hauptmenue) - nichts bleibt haengen
    void resetMix();

    // Mischt frames Stereo-Bilder (fuer Tests ohne Audiogeraet)
    void mixForTest(float* out, int frames);
    // V11: Reduktion des Begrenzers im letzten Block (Tests: 1 = keine)
    float limiterGain() const { return limiter_.reduction(); }
    // V13.3: Zustand (Stimmen, Geraet, Pegel am Master-Ausgang) und eine Zeile fuers Protokoll
    AudioStats stats();
    std::string statsLine();
    // V13.3 Watchdog (Hauptthread, je Bild): haengt der Audiothread oder steht der Strom, Neustart anfordern
    void watchdog();
    // V13.3: Strom schliessen und neu oeffnen (Grund ins Protokoll)
    void requestRestart(const std::string& reason);
    // V13.3 (Tests): Stille-Erkennung auch ohne Geraet (mixForTest)
    void enableSilenceCheck(bool on);
    void injectFault(Fault f, int param = 0);
    int mixRate() const { return sampleRate_; }

    // V13.3: Rueckrufe der Geraete- und Sitzungsbenachrichtigungen (beliebiger Thread, nur Merker setzen)
    void onDefaultChanged(const wchar_t* id);
    void onDeviceState(const wchar_t* id, bool active);
    void onSessionDisconnected(int reason);
    void onFormatProperty(const wchar_t* id);
    void onSessionVolume(float volume, bool mute);

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
        unsigned stopFrames = 0;  // V13.3: so lange schon "stopping" (Bilder)
    };
    using SoundMap = std::unordered_map<std::string, Sound>;
    friend struct DeviceThread;
    void thread();
    void mix(float* out, int frames);
    int allocVoice(int priority);
    const Sound* find(const std::string& name) const;
    const Sound* pick(const std::string& name);  // mit Variantenwahl
    std::unordered_set<std::string> missing_;     // V11: schon gemeldete unbekannte Namen
    void startVoice(Voice& v, const Sound* s, float volume, float pitch, Bus bus, int priority);
    void updateRoom();
    void buildGroups();
    // V13.3 (unter mutex_)
    void resetDsp();
    void setMixRate(int rate);
    void measureOutput(float* out, int frames, double blockExpSq);
    void note(std::string msg);  // Protokollzeile, ausserhalb der Sperre geschrieben
    void flushNotes();
    void wake();
    bool silenceAlarmPending();

    std::shared_ptr<SoundMap> sounds_;  // V13.3: geteilt (Tests: eine Klangbank fuer viele Systeme)
    std::unordered_map<std::string, std::vector<const Sound*>> groups_;
    std::unordered_map<std::string, int> lastVariant_;
    std::vector<Voice> voices_;
    std::mutex mutex_;
    std::thread thread_;
    std::atomic<bool> ready_{false};
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

    // V13.3: Messwerte (unter mutex_), Geraeteangaben (unter statMutex_)
    AudioStats st_;
    std::mutex statMutex_;
    struct DeviceInfo {
        std::string device;
        std::wstring id;
        int deviceRate = 0, deviceChannels = 0, deviceBits = 0, streamRate = 0, streamChannels = 0;
        unsigned bufferFrames = 0;
        float periodMs = 0.0f;
        unsigned opens = 0, openFailures = 0, losses = 0, restarts = 0;
        long lastHr = 0;
        unsigned long long underruns = 0, timeouts = 0;
        float mixMsMax = 0.0f;
        std::string reopenReason, lastReason;
    } dev_;
    double winPeak_ = 0.0, winSq_ = 0.0, winExpSq_ = 0.0, fastSq_ = 0.0;
    long long winFrames_ = 0;
    std::vector<std::string> notes_;
    // Stille-Erkennung (unter mutex_)
    bool silenceCheck_ = false, silenceAlarm_ = false;
    double silentSec_ = 0.0, silentExpSq_ = 0.0;
    long long silentFrames_ = 0;
    double clock_ = 0.0, nanNoteT_ = -1e9;  // gemischte Zeit (s)
    // Geraetethread: Wecken, Neustart, Herzschlag
    void* wake_ = nullptr;           // HANDLE (Auto-Reset-Ereignis)
    void* notifier_ = nullptr;       // IMMNotificationClient (Geraetewechsel)
    void* sessionEvents_ = nullptr;  // IAudioSessionEvents des offenen Stroms (unter statMutex_)
    std::atomic<bool> reopen_{false};
    std::atomic<unsigned long long> heartbeat_{0}, mixed_{0};
    std::atomic<int> step_{0};
    std::atomic<bool> hung_{false};
    // Watchdog (nur Hauptthread)
    unsigned long long wdBeat_ = 0, wdFrames_ = 0;
    long long wdBeatT_ = 0, wdFramesT_ = 0;
    bool wdStalled_ = false;
    // Fehler (Tests)
    std::atomic<int> fInvalidate_{0}, fInitFails_{0}, fRate_{0};
    std::atomic<long long> fNoDeviceUntil_{0}, fInUseUntil_{0}, fStallUntil_{0};
    std::atomic<bool> fMute_{false}, fNan_{false};
};

// Klangerzeugung (auch fuer Tests nutzbar)
std::unordered_map<std::string, Sound> synthesizeAll(JobSystem& jobs, int rate);

}  // namespace lim::audio
