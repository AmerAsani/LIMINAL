#include "audio/audio.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>

#include "core/com_ptr.hpp"
#include "core/jobs.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"

#include <audioclient.h>
#include <audiopolicy.h>
#include <avrt.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <propidl.h>
#include <propsys.h>

namespace lim::audio {

namespace {

constexpr int kVoices = 96;
constexpr int kSynthRate = 48000;  // Abtastrate der erzeugten Klaenge (der Mischer rechnet je Stimme um)
constexpr float kSpeedOfSound = 343.0f;
constexpr float kHeadRadius = 0.0875f;
// V13.3 Stille-Erkennung: erwarteter Pegel ueber -60 dBFS, gemessener unter -100 dBFS, 1 s lang
constexpr double kExpectAudible = 0.001;   // -60 dBFS
constexpr double kMeasuredSilent = 1e-5;   // -100 dBFS
constexpr double kSilenceSeconds = 1.0;

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// V13.3: Fehlercodes lesbar ins Protokoll
const char* hrName(long hr) {
    switch ((unsigned long)hr) {
        case 0x88890004ul: return "AUDCLNT_E_DEVICE_INVALIDATED";
        case 0x8889000Aul: return "AUDCLNT_E_DEVICE_IN_USE";
        case 0x88890008ul: return "AUDCLNT_E_UNSUPPORTED_FORMAT";
        case 0x88890010ul: return "AUDCLNT_E_SERVICE_NOT_RUNNING";
        case 0x8889000Ful: return "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
        case 0x88890026ul: return "AUDCLNT_E_RESOURCES_INVALIDATED";
        case 0x8889000Eul: return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
        case 0x88890017ul: return "AUDCLNT_E_CPUUSAGE_EXCEEDED";
        case 0x88890018ul: return "AUDCLNT_E_BUFFER_ERROR";
        case 0x88890006ul: return "AUDCLNT_E_BUFFER_TOO_LARGE";
        case 0x80070490ul: return "E_NOTFOUND (kein Geraet)";
        case 0x80070005ul: return "E_ACCESSDENIED";
        case 0x80070057ul: return "E_INVALIDARG";
        case 0x8007000Eul: return "E_OUTOFMEMORY";
        case 0x800401F0ul: return "CO_E_NOTINITIALIZED";
        default: return "";
    }
}
std::string hrText(long hr) {
    const char* n = hrName(hr);
    return std::format("0x{:08X}{}{}", (unsigned long)hr, *n ? " " : "", n);
}

const char* stepName(int s) {
    static const char* names[] = {"-", "Oeffnen", "Warten auf den Treiber", "GetCurrentPadding", "GetBuffer", "Mischen", "ReleaseBuffer",
                                  "Schliessen", "Ersatztakt (ohne Geraet)"};
    return s >= 0 && s < 9 ? names[s] : "?";
}
enum Step { kIdle, kOpen, kWait, kPadding, kGetBuffer, kMix, kRelease, kClose, kNull };

const char* disconnectText(int r) {
    switch (r) {
        case DisconnectReasonDeviceRemoval: return "Geraet entfernt";
        case DisconnectReasonServerShutdown: return "Audiodienst beendet";
        case DisconnectReasonFormatChanged: return "Format/Abtastrate des Geraets geaendert";
        case DisconnectReasonSessionLogoff: return "Abmeldung";
        case DisconnectReasonSessionDisconnected: return "Remotesitzung getrennt";
        case DisconnectReasonExclusiveModeOverride: return "anderes Programm nutzt das Geraet exklusiv";
        default: return "unbekannter Grund";
    }
}

// Einfacher WAV-Leser (PCM 16/24 Bit oder Float, mono/stereo) fuer eigene Klaenge
bool loadWav(const std::string& path, Sound& out) {
    auto data = paths::readBinary(path);
    if (!data || data->size() < 44) return false;
    const unsigned char* p = data->data();
    if (std::memcmp(p, "RIFF", 4) != 0 || std::memcmp(p + 8, "WAVE", 4) != 0) return false;
    size_t off = 12;
    int channels = 0, srcRate = 0, bits = 0, format = 0;
    const unsigned char* pcm = nullptr;
    size_t pcmSize = 0;
    while (off + 8 <= data->size()) {
        u32 size;
        std::memcpy(&size, p + off + 4, 4);
        if (std::memcmp(p + off, "fmt ", 4) == 0 && size >= 16) {
            format = p[off + 8] | (p[off + 9] << 8);
            channels = p[off + 10] | (p[off + 11] << 8);
            std::memcpy(&srcRate, p + off + 12, 4);
            bits = p[off + 22] | (p[off + 23] << 8);
        } else if (std::memcmp(p + off, "data", 4) == 0) {
            pcm = p + off + 8;
            pcmSize = std::min<size_t>(size, data->size() - off - 8);
        }
        off += 8 + size + (size & 1);
    }
    if (!pcm || channels < 1 || srcRate <= 0 || (bits != 16 && bits != 24 && bits != 32)) return false;
    const size_t frames = pcmSize / (size_t)(channels * bits / 8);
    out.samples.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        float s = 0;
        for (int c = 0; c < channels; ++c) {
            const unsigned char* q = pcm + (i * channels + c) * (bits / 8);
            if (format == 3 && bits == 32) {
                float f;
                std::memcpy(&f, q, 4);
                s += f;
            } else if (bits == 16) {
                s += (float)(i16)(q[0] | (q[1] << 8)) / 32768.0f;
            } else if (bits == 24) {
                i32 v = (i32)((u32)q[0] << 8 | (u32)q[1] << 16 | (u32)q[2] << 24) >> 8;
                s += (float)v / 8388608.0f;
            }
        }
        out.samples[i] = s / (float)channels;
    }
    out.rate = srcRate;  // der Mischer rechnet die Abtastrate je Stimme um
    return true;
}

// Basisname einer Variante: "step_concrete_3" -> "step_concrete" (sonst leer)
std::string variantBase(const std::string& n) {
    const size_t u = n.find_last_of('_');
    if (u == std::string::npos || u + 1 >= n.size()) return {};
    for (size_t i = u + 1; i < n.size(); ++i)
        if (n[i] < '0' || n[i] > '9') return {};
    return n.substr(0, u);
}

bool finite3(vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

// V13.3: Name eines Endpunkts (PKEY_Device_FriendlyName)
std::string deviceName(IMMDevice* dev) {
    static const PROPERTYKEY kFriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
    std::string name = "?";
    Com<IPropertyStore> ps;
    if (dev && SUCCEEDED(dev->OpenPropertyStore(STGM_READ, ps.put()))) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(ps->GetValue(kFriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) name = paths::narrow(pv.pwszVal);
        PropVariantClear(&pv);
    }
    return name;
}

// PKEY_AudioEngine_DeviceFormat: aendert sich, wenn jemand Abtastrate oder Bittiefe des Geraets umstellt
const PROPERTYKEY kDeviceFormat = {{0xf19f064d, 0x082c, 0x4e27, {0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c}}, 0};

// --- V13.3: Benachrichtigungen von Windows (beliebiger Thread; nur Merker setzen) ----------------------------
class DeviceNotifier final : public IMMNotificationClient {
public:
    explicit DeviceNotifier(AudioSystem* s) : sys_(s) {}
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG r = --ref_;
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD state) override {
        sys_->onDeviceState(id, state == DEVICE_STATE_ACTIVE);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override {
        sys_->onDeviceState(id, true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override {
        sys_->onDeviceState(id, false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override {
        if (flow == eRender && role == eConsole) sys_->onDefaultChanged(id);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR id, const PROPERTYKEY key) override {
        if (key.fmtid == kDeviceFormat.fmtid && key.pid == kDeviceFormat.pid) sys_->onFormatProperty(id);
        return S_OK;
    }

private:
    std::atomic<ULONG> ref_{1};
    AudioSystem* sys_;
};

class SessionEvents final : public IAudioSessionEvents {
public:
    explicit SessionEvents(AudioSystem* s) : sys_(s) {}
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG r = --ref_;
        if (r == 0) delete this;
        return r;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IAudioSessionEvents)) {
            *out = static_cast<IAudioSessionEvents*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float v, WINBOOL mute, LPCGUID) override {
        sys_->onSessionVolume(v, mute != FALSE);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD, float[], DWORD, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID, LPCGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason r) override {
        sys_->onSessionDisconnected((int)r);
        return S_OK;
    }

private:
    std::atomic<ULONG> ref_{1};
    AudioSystem* sys_;
};

WAVEFORMATEXTENSIBLE stereoFloat(int rate) {
    WAVEFORMATEXTENSIBLE wf{};
    wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels = 2;
    wf.Format.nSamplesPerSec = (DWORD)rate;
    wf.Format.wBitsPerSample = 32;
    wf.Format.nBlockAlign = 8;
    wf.Format.nAvgBytesPerSec = (DWORD)rate * 8;
    wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wf.Samples.wValidBitsPerSample = 32;
    wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return wf;
}

}  // namespace

AudioSystem::AudioSystem() {
    voices_.resize(kVoices);
    sounds_ = std::make_shared<SoundMap>();
    wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    notifier_ = new DeviceNotifier(this);
}

AudioSystem::~AudioSystem() {
    stop_ = true;
    wake();
    if (thread_.joinable()) thread_.join();
    flushNotes();
    if (notifier_) static_cast<DeviceNotifier*>(notifier_)->Release();
    if (wake_) CloseHandle((HANDLE)wake_);
}

bool AudioSystem::init(JobSystem& jobs, const std::string& dataDir, bool device) {
    auto bank = std::make_shared<SoundMap>(synthesizeAll(jobs, kSynthRate));
    // Eigene Klaenge aus data/sounds ersetzen die erzeugten oder ergaenzen Varianten
    for (const auto& f : paths::listFiles(paths::join(dataDir, "sounds"), "", ".wav")) {
        std::string name = paths::fileName(f);
        name = name.substr(0, name.size() - 4);
        Sound s;
        if (loadWav(f, s)) {
            auto it = bank->find(name);
            const std::string base = variantBase(name);
            s.loop = it != bank->end() ? it->second.loop
                                       : (!base.empty() && bank->count(base + "_0") ? (*bank)[base + "_0"].loop : false);
            (*bank)[name] = std::move(s);
            log::info("Klang ersetzt/ergaenzt: {}", name);
        } else {
            log::warn("Klang nicht lesbar (Format?): {}", f);  // V13.3: fehlerhafte Datei melden statt still uebergehen
        }
    }
    {
        std::lock_guard lock(mutex_);
        sounds_ = std::move(bank);
        buildGroups();
        resetDsp();
    }
    ready_ = true;
    if (device) startDevice();
    return true;
}

void AudioSystem::initOffline(JobSystem& jobs) {
    std::lock_guard lock(mutex_);
    if (sounds_->empty()) sounds_ = std::make_shared<SoundMap>(synthesizeAll(jobs, kSynthRate));
    buildGroups();
    resetDsp();
    ready_ = true;  // play() nimmt Klaenge an; ohne Thread wird nur ueber mixForTest gemischt
}

void AudioSystem::initShared(const AudioSystem& other) {
    std::lock_guard lock(mutex_);
    sounds_ = other.sounds_;
    buildGroups();
    resetDsp();
    ready_ = true;
}

void AudioSystem::startDevice() {
    if (thread_.joinable()) return;
    stop_ = false;
    thread_ = std::thread([this] { thread(); });
}

void AudioSystem::buildGroups() {
    groups_.clear();
    std::vector<std::string> keys;
    for (auto& [k, v] : *sounds_) keys.push_back(k);
    std::sort(keys.begin(), keys.end());
    for (const auto& k : keys) {
        const std::string base = variantBase(k);
        if (!base.empty()) groups_[base].push_back(&sounds_->at(k));
    }
}

// V13.3: Hall, Fruehreflexionen, Begrenzer und Filter neu - auch nach einem vergifteten Zustand (NaN)
void AudioSystem::resetDsp() {
    const float rate = (float)sampleRate_;
    fdn_.init(rate);
    fdn_.clear();
    early_.init(rate);
    limiter_.init(rate, 1.5f, 120.0f, -1.0f);
    cur_ = target_;
    roomDirty_ = true;
    roomTimer_ = 0.0f;
    dcZ_[0] = dcZ_[1] = dcX_[0] = dcX_[1] = 0.0f;
    muffleZ_[0] = muffleZ_[1] = 0.0f;
    muffleCur_ = muffle_;
    duckEnv_ = 0.0f, duck_ = 1.0f;
    for (auto& v : voices_) {
        if (!std::isfinite(v.lp1) || !std::isfinite(v.lp2) || !std::isfinite(v.shL) || !std::isfinite(v.shR)) {
            v.lp1 = v.lp2 = v.shL = v.shR = 0.0f;
            v.ring.fill(0.0f);
        }
        if (!std::isfinite(v.curGainL) || !std::isfinite(v.curGainR)) v.curGainL = v.curGainR = 0.0f;
    }
    silentSec_ = 0.0, silentExpSq_ = 0.0, silentFrames_ = 0;
    fMute_ = false;  // die gestellten Haengezustaende (Tests) sind Teil des Mischerzustands
    fNan_ = false;
}

void AudioSystem::setMixRate(int rate) {
    if (rate <= 0 || rate == sampleRate_) return;
    sampleRate_ = rate;
    resetDsp();
}

std::vector<std::string> AudioSystem::names() const {
    std::vector<std::string> n;
    for (auto& [k, v] : *sounds_) n.push_back(k);
    std::sort(n.begin(), n.end());
    return n;
}

int AudioSystem::variants(const std::string& base) const {
    auto it = groups_.find(base);
    return it == groups_.end() ? 0 : (int)it->second.size();
}

const Sound* AudioSystem::find(const std::string& name) const {
    auto it = sounds_->find(name);
    if (it != sounds_->end()) return &it->second;
    auto g = groups_.find(name);
    if (g != groups_.end() && !g->second.empty()) return g->second.front();
    return nullptr;
}

const Sound* AudioSystem::pick(const std::string& name) {
    auto g = groups_.find(name);
    if (g != groups_.end() && !g->second.empty() && !sounds_->count(name)) {
        const int n = (int)g->second.size();
        int& last = lastVariant_[name];
        int k = rand_.irange(0, n - 1);
        if (n > 1 && k == last) k = (k + 1 + rand_.irange(0, n - 2)) % n;
        last = k;
        return g->second[(size_t)k];
    }
    const Sound* s = find(name);
    // V11: unbekannte Namen einmal melden (ein vertippter Name waere sonst einfach still)
    if (!s && !sounds_->empty() && missing_.insert(name).second) note("WKlang fehlt: " + name);
    return s;
}

int AudioSystem::allocVoice(int priority) {
    for (size_t i = 0; i < voices_.size(); ++i)
        if (!voices_[i].active) return (int)i;
    // V13.3: zuerst eine Stimme, die ohnehin gerade ausblendet (auch eine Schleife)
    int best = -1;
    float bestFade = 2.0f;
    for (size_t i = 0; i < voices_.size(); ++i)
        if (voices_[i].stopping && voices_[i].fade < bestFade) bestFade = voices_[i].fade, best = (int)i;
    if (best >= 0) return best;
    // die am wenigsten hoerbare einmalige Stimme mit gleicher oder niedrigerer Prioritaet ersetzen
    float bestA = 1e9f;
    for (size_t i = 0; i < voices_.size(); ++i) {
        const Voice& v = voices_[i];
        if (v.loop || v.priority > priority) continue;
        const float a = v.audible * (float)v.priority;
        if (a < bestA) bestA = a, best = (int)i;
    }
    if (best < 0) ++st_.allocFails;  // V13.3: alle Stimmen belegt (laufende Schleifen werden nie verdraengt)
    return best;
}

void AudioSystem::startVoice(Voice& vo, const Sound* s, float volume, float pitch, Bus bus, int priority) {
    vo = Voice{};
    vo.id = nextId_++;
    vo.sound = s;
    vo.volume = volume;
    vo.pitch = pitch;
    vo.bus = bus;
    vo.priority = priority;
    vo.active = true;
    vo.audible = volume;
    ++st_.plays;
}

void AudioSystem::play(const std::string& name, float volume, float pitch, Bus bus) {
    PlayOpts o;
    o.volume = volume, o.pitch = pitch, o.bus = bus, o.vary = false;
    play2d(name, o);
}

void AudioSystem::play2d(const std::string& name, const PlayOpts& o) {
    std::lock_guard lock(mutex_);
    // V13.3: Klaenge werden angenommen, sobald sie erzeugt sind - auch waehrend das Geraet (neu) verbunden wird
    // (vorher: verworfen, solange der Strom nicht lief; ohne Geraet mischt der Ersatztakt)
    if (!ready_) {
        ++st_.dropped;
        return;
    }
    if (!std::isfinite(o.volume) || !std::isfinite(o.pitch) || o.pitch <= 0.0f) {
        ++st_.badInput;
        return;
    }
    const Sound* s = pick(name);
    if (!s || s->samples.empty()) return;
    int v = allocVoice(o.priority);
    if (v < 0) return;
    float vol = o.volume, pitch = o.pitch;
    if (o.vary) vol *= dsp::dbToGain(rand_.range(-1.5f, 1.5f)), pitch *= rand_.range(0.97f, 1.03f);
    Voice& vo = voices_[(size_t)v];
    startVoice(vo, s, vol, pitch, o.bus, o.priority);
    vo.send = o.send >= 0.0f ? o.send : (o.bus == Bus::Ui ? 0.0f : 0.35f);  // eigene Schritte klingen im Raum nach
}

void AudioSystem::playAt(const std::string& name, vec3 pos, float volume, float radius, float pitch, float occlusion) {
    PlayOpts o;
    o.volume = volume, o.pitch = pitch, o.vary = false;
    Spatial s;
    s.pos = pos, s.radius = radius, s.occlusion = occlusion;
    play3d(name, s, o);
}

void AudioSystem::play3d(const std::string& name, const Spatial& sp, const PlayOpts& o) {
    std::lock_guard lock(mutex_);
    if (!ready_) {
        ++st_.dropped;
        return;
    }
    // V13.3: ungueltige Angaben (NaN) wuerden den ganzen Mischer vergiften - abweisen
    if (!finite3(sp.pos) || !std::isfinite(sp.radius) || !std::isfinite(sp.pathExtra) || !std::isfinite(sp.occlusion) ||
        !std::isfinite(o.volume) || !std::isfinite(o.pitch) || o.pitch <= 0.0f) {
        ++st_.badInput;
        return;
    }
    // Quellen jenseits der Reichweite gar nicht erst starten
    if (length(sp.pos - listener_) + sp.pathExtra > sp.radius) return;
    const Sound* s = pick(name);
    if (!s || s->samples.empty()) return;
    int v = allocVoice(o.priority);
    if (v < 0) return;
    float vol = o.volume, pitch = o.pitch;
    if (o.vary) vol *= dsp::dbToGain(rand_.range(-1.5f, 1.5f)), pitch *= rand_.range(0.97f, 1.03f);
    Voice& vo = voices_[(size_t)v];
    startVoice(vo, s, vol, pitch, o.bus, o.priority);
    vo.positional = true;
    vo.at = sp.pos;
    vo.radius = sp.radius;
    vo.refDist = std::max(0.2f, o.refDist != 1.0f ? o.refDist : sp.refDist);
    vo.pathExtra = std::max(0.0f, sp.pathExtra);
    vo.occlusion = dsp::clampf(sp.occlusion, 0.0f, 1.0f);
    vo.send = o.send >= 0.0f ? o.send : 0.8f;
}

int AudioSystem::startLoop(const std::string& name, float volume, Bus bus) {
    std::lock_guard lock(mutex_);
    if (!ready_ || !std::isfinite(volume)) return -1;
    const Sound* s = pick(name);
    if (!s || s->samples.empty()) return -1;
    int v = allocVoice(3);
    if (v < 0) return -1;
    Voice& vo = voices_[(size_t)v];
    startVoice(vo, s, volume, 1.0f, bus, 3);
    vo.loop = true;
    vo.send = bus == Bus::Ambience ? 0.1f : (bus == Bus::Ui ? 0.0f : 0.6f);
    // zufaelliger Startpunkt, damit gleiche Schleifen nicht phasengleich laufen
    vo.pos = (double)((u32)vo.id * 2654435761u % (u32)std::max<size_t>(1, s->samples.size()));
    return vo.id;
}

void AudioSystem::setLoopVolume(int id, float volume) {
    std::lock_guard lock(mutex_);
    if (!std::isfinite(volume)) {
        ++st_.badInput;
        return;
    }
    for (auto& v : voices_)
        if (v.active && v.id == id) v.volume = volume;
}

void AudioSystem::setLoopPosition(int id, vec3 pos, float radius) {
    std::lock_guard lock(mutex_);
    if (!finite3(pos) || !std::isfinite(radius)) {
        ++st_.badInput;
        return;
    }
    for (auto& v : voices_)
        if (v.active && v.id == id) {
            v.positional = true;
            v.at = pos;
            v.radius = radius;
        }
}

void AudioSystem::setLoopSpatial(int id, const Spatial& s) {
    std::lock_guard lock(mutex_);
    if (!finite3(s.pos) || !std::isfinite(s.radius) || !std::isfinite(s.refDist) || !std::isfinite(s.pathExtra) ||
        !std::isfinite(s.occlusion)) {
        ++st_.badInput;
        return;
    }
    for (auto& v : voices_)
        if (v.active && v.id == id) {
            v.positional = true;
            v.at = s.pos;
            v.radius = s.radius;
            v.refDist = std::max(0.2f, s.refDist);
            v.pathExtra = std::max(0.0f, s.pathExtra);
            v.occlusion = dsp::clampf(s.occlusion, 0.0f, 1.0f);
        }
}

void AudioSystem::setLoopFadeIn(int id, float seconds) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id) {
            v.fadeIn = 0.0f;
            v.fadeInStep = 1.0f / std::max(1.0f, seconds * (float)sampleRate_);
        }
}

void AudioSystem::setLoopSend(int id, float send) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id) v.send = std::max(0.0f, send);
}

bool AudioSystem::loopActive(int id) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id && !v.stopping) return true;
    return false;
}

void AudioSystem::setLoopPitch(int id, float pitch) {
    std::lock_guard lock(mutex_);
    if (!std::isfinite(pitch) || pitch <= 0.0f) {
        ++st_.badInput;
        return;
    }
    for (auto& v : voices_)
        if (v.active && v.id == id) v.pitch = pitch;
}

void AudioSystem::stopLoop(int id) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id) v.stopping = true;
}

void AudioSystem::stopAll() {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_) v.stopping = true;
}

void AudioSystem::setListener(vec3 pos, float yaw) {
    std::lock_guard lock(mutex_);
    if (!finite3(pos) || !std::isfinite(yaw)) {
        ++st_.badInput;
        return;
    }
    listener_ = pos;
    listenerYaw_ = yaw;
}

void AudioSystem::setVolumes(float master, float effects, float ambience) {
    std::lock_guard lock(mutex_);
    master_ = std::isfinite(master) ? std::max(0.0f, master) : master_;
    bus_[(int)Bus::Effects] = std::isfinite(effects) ? std::max(0.0f, effects) : bus_[(int)Bus::Effects];
    bus_[(int)Bus::Ambience] = std::isfinite(ambience) ? std::max(0.0f, ambience) : bus_[(int)Bus::Ambience];
}

void AudioSystem::setBusVolume(Bus b, float v) {
    std::lock_guard lock(mutex_);
    if (std::isfinite(v)) bus_[(int)b] = std::max(0.0f, v);
}

void AudioSystem::setAcoustics(const Acoustics& a) {
    std::lock_guard lock(mutex_);
    target_ = a;
    roomDirty_ = true;
}

void AudioSystem::setPaused(bool paused) {
    std::lock_guard lock(mutex_);
    paused_ = paused;
}

void AudioSystem::setMuffle(float m) {
    std::lock_guard lock(mutex_);
    muffle_ = std::isfinite(m) ? dsp::clampf(m, 0.0f, 1.0f) : 0.0f;
}

void AudioSystem::resetMix() {
    std::lock_guard lock(mutex_);
    paused_ = false;
    muffle_ = 0.0f;
}

// --- Raum ----------------------------------------------------------------------------------------
// Die Zielwerte werden weich angefahren (Zeitkonstante ~0,4 s); die Fruehreflexionen folgen sofort
// der Hoererposition (sie aendern sich ohnehin nur langsam).
void AudioSystem::updateRoom() {
    const float k = 0.12f;
    auto glide = [&](float& c, float t) { c += (t - c) * k; };
    glide(cur_.t60, target_.t60);
    glide(cur_.damping, target_.damping);
    glide(cur_.wet, target_.wet);
    glide(cur_.preDelay, target_.preDelay);
    glide(cur_.size, target_.size);
    glide(cur_.diffusion, target_.diffusion);
    glide(cur_.early, target_.early);
    cur_.gate = target_.gate;
    const float tLow = target_.t60Low > 0 ? target_.t60Low : target_.t60 * (1.15f + 0.3f * (1.0f - target_.damping));
    const float tHigh = target_.t60High > 0 ? target_.t60High : target_.t60 * (1.0f - 0.75f * dsp::clampf(target_.damping, 0.0f, 1.0f));
    if (cur_.t60Low < 0) cur_.t60Low = tLow;
    if (cur_.t60High < 0) cur_.t60High = tHigh;
    glide(cur_.t60Low, tLow);
    glide(cur_.t60High, tHigh);
    dsp::RoomSpec r;
    r.t60Mid = cur_.t60, r.t60Low = cur_.t60Low, r.t60High = cur_.t60High;
    r.size = cur_.size, r.preDelay = cur_.preDelay, r.diffusion = cur_.diffusion, r.gate = cur_.gate;
    fdn_.set(r);
    if (target_.roomW > 0.0f)
        early_.set(dsp::shoeboxReflections(target_.roomW, target_.roomD, target_.roomH, target_.lx, target_.ly, target_.lz,
                                           target_.absorption, listenerYaw_));
    else
        early_.set({});
}

// --- Mischer ------------------------------------------------------------------------------------
void AudioSystem::mixForTest(float* out, int frames) {
    mix(out, frames);
    flushNotes();
}

void AudioSystem::mix(float* out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    const auto tw = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex_);
    // V13.3: wie lange der Mischer auf die Sperre wartete (haelt der Spielthread sie zu lange?)
    st_.lockWaitMsMax = std::max(st_.lockWaitMsMax, std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - tw).count());
    const size_t nf = (size_t)frames;
    send_.assign(nf, 0.0f);
    wetL_.assign(nf, 0.0f);
    wetR_.assign(nf, 0.0f);
    erL_.assign(nf, 0.0f);
    erR_.assign(nf, 0.0f);
    for (int b = 0; b < kBusCount; ++b) busL_[b].assign(nf, 0.0f), busR_[b].assign(nf, 0.0f);
    const float rate = (float)sampleRate_;
    const vec3 right{-std::sin(listenerYaw_), std::cos(listenerYaw_), 0.0f};
    const vec3 fwd{std::cos(listenerYaw_), std::sin(listenerYaw_), 0.0f};
    const float shadowA = dsp::onePoleCoef(1700.0f, rate);
    double blockExpSq = 0.0;  // V13.3: erwartete Energie am Ausgang (Stille-Erkennung)
    for (auto& v : voices_) {
        if (!v.active || !v.sound || v.sound->samples.empty()) continue;
        if (v.stopping) {
            v.stopFrames += (unsigned)frames;
            // V13.3 Watchdog: das Ausblenden dauert 50 ms. Haengt eine Stimme laenger darin, wird sie freigegeben
            // (nach der Behebung von B-017 sollte das nie greifen - jeder Fall steht im Protokoll)
            if (v.stopFrames > (unsigned)(sampleRate_ / 2)) {
                v.active = false;
                if (st_.reclaimed++ < 20) note(std::format("WAudio-Watchdog: Stimme {} hing {} ms im Ausblenden - freigegeben", v.id,
                                                           (unsigned long long)v.stopFrames * 1000ull / (unsigned)sampleRate_));
                continue;
            }
        }
        const bool world = v.bus != Bus::Ui;
        const float pauseMul = (paused_ && world) ? 0.3f : 1.0f;
        float gain = v.volume * pauseMul;
        float cutoff = 20000.0f;
        float itdL = 0, itdR = 0, hfL = 1, hfR = 1, pan = 0;
        float sendScale = 1.0f;
        if (v.positional) {
            const vec3 d = v.at - listener_;
            const float dist = length(d);
            const float de = dist + v.pathExtra;
            // Abstand: 1/d ab der Referenzdistanz, weich auf null bis zur Reichweite
            float g = v.refDist / std::max(v.refDist, de);
            g *= 1.0f - dsp::smooth01((de - v.radius * 0.7f) / std::max(0.1f, v.radius * 0.3f));
            g *= 1.0f - 0.6f * v.occlusion;
            gain *= g;
            // Hall: faellt langsamer ab als der Direktschall (diffuses Feld), Verdeckung trennt Raeume
            sendScale = std::sqrt(std::max(g, 0.0f)) * (1.0f - 0.5f * v.occlusion);
            if (dist > 0.05f) {
                const vec3 n = d / dist;
                const float lat = dsp::clampf(dot(n, right), -1.0f, 1.0f);  // > 0 rechts
                const float front = dot(n, fwd);
                const float th = std::asin(std::fabs(lat));
                const float itd = kHeadRadius / kSpeedOfSound * (th + std::sin(th)) * rate;  // Woodworth
                if (lat > 0) itdL = itd;
                else itdR = itd;
                // Kopfschatten: das abgewandte Ohr verliert Hoehen, das zugewandte gewinnt etwas
                const float farEar = dsp::dbToGain(-11.0f * std::fabs(lat)), nearEar = dsp::dbToGain(2.0f * std::fabs(lat));
                hfL = lat > 0 ? farEar : nearEar;
                hfR = lat > 0 ? nearEar : farEar;
                // von hinten etwas dumpfer (Ohrmuschel)
                const float back = std::max(0.0f, -front);
                hfL *= 1.0f - 0.3f * back;
                hfR *= 1.0f - 0.3f * back;
                pan = lat * 0.55f;
            }
            cutoff = std::min(cutoff, std::max(1800.0f, 20000.0f * std::exp(-de / 55.0f)));          // Luft
            cutoff = std::min(cutoff, 20000.0f * std::pow(0.03f, v.occlusion));                         // Waende
        }
        if (!std::isfinite(gain)) gain = 0.0f;  // V13.3: nie NaN in den Mischer
        if (gain < 1e-5f && v.curGainL < 1e-5f && v.curGainR < 1e-5f && v.loop) {
            // V13.3 (B-017): eine gestoppte Schleife, die man nicht hoert, ist sofort frei. Vorher lief hier das
            // Ausblenden nie (es geschieht nur im hoerbaren Zweig unten) - die Stimme blieb fuer immer belegt.
            if (v.stopping) {
                v.active = false;
                continue;
            }
            // unhoerbare Schleife: nur weiterlaufen lassen
            v.pos += (double)v.pitch * (double)v.sound->rate / rate * frames;
            const double len = (double)v.sound->samples.size();
            if (v.pos >= len) v.pos = std::fmod(v.pos, len);
            v.audible = 0.0f;
            continue;
        }
        v.lpCoef = cutoff >= 19999.0f ? 0.0f : dsp::onePoleCoef(std::max(cutoff, 120.0f), rate);
        v.itdL = itdL, v.itdR = itdR, v.hfL = hfL, v.hfR = hfR;
        const float a = (pan + 1.0f) * 0.25f * dsp::kPi;
        const float gl = gain * std::cos(a) * 1.41421356f, gr = gain * std::sin(a) * 1.41421356f;
        const float sendTarget = v.send * sendScale * v.volume * pauseMul;
        const std::vector<float>& s = v.sound->samples;
        const size_t n = s.size();
        const double len = (double)n;
        const double step = (double)v.pitch * (double)v.sound->rate / (double)rate;
        auto at = [&](long long i) -> float {
            if (v.loop) {
                i %= (long long)n;
                if (i < 0) i += (long long)n;
                return s[(size_t)i];
            }
            if (i < 0 || i >= (long long)n) return 0.0f;
            return s[(size_t)i];
        };
        float* oL = busL_[(int)v.bus].data();
        float* oR = busR_[(int)v.bus].data();
        const float inv = 1.0f / (float)frames;
        double vsq = 0.0;  // V13.3: Energie der Stimme (erwarteter Pegel am Ausgang)
        for (int i = 0; i < frames; ++i) {
            const float t = (float)i * inv;
            if (v.stopping) {
                v.fade -= 1.0f / (0.05f * rate);  // 50 ms ausblenden
                if (v.fade <= 0.0f) {
                    v.active = false;
                    break;
                }
            }
            if (v.fadeIn < 1.0f) v.fadeIn = std::min(1.0f, v.fadeIn + v.fadeInStep);
            // kubisch interpoliert lesen
            const long long i0 = (long long)std::floor(v.pos);
            const float f = (float)(v.pos - (double)i0);
            const float y0 = at(i0 - 1), y1 = at(i0), y2 = at(i0 + 1), y3 = at(i0 + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            float x = (((c3 * f + c2) * f + c1) * f + y1) * v.fade * v.fadeIn;
            // Luft und Verdeckung (12 dB/Oktave), Koeffizient gleitet
            v.lpCoefCur += (v.lpCoef - v.lpCoefCur) * 0.002f;
            if (v.lpCoefCur > 1e-4f) {
                v.lp1 = x + (v.lp1 - x) * v.lpCoefCur;
                v.lp2 = v.lp1 + (v.lp2 - v.lp1) * v.lpCoefCur;
                x = v.lp2;
            }
            vsq += (double)x * x;
            // Hall: mono
            v.sendCur += (sendTarget - v.sendCur) * 0.002f;
            send_[(size_t)i] += x * v.sendCur;
            // Ohren
            float l, r;
            if (v.positional) {
                v.ring[(size_t)v.ringPos] = x;
                const float itl = v.curItdL + (v.itdL - v.curItdL) * t, itr = v.curItdR + (v.itdR - v.curItdR) * t;
                auto delayed = [&](float d) {
                    float p = (float)v.ringPos - d;
                    if (p < 0) p += 64.0f;
                    const int k0 = (int)p;
                    const float fr = p - (float)k0;
                    const float a0 = v.ring[(size_t)(k0 & 63)], a1 = v.ring[(size_t)((k0 + 1) & 63)];
                    return a0 + (a1 - a0) * fr;
                };
                l = itl > 0.01f ? delayed(itl) : x;
                r = itr > 0.01f ? delayed(itr) : x;
                v.ringPos = (v.ringPos + 1) & 63;
                // Kopfschatten: Einpol-Teilung, Hoehenanteil je Ohr
                const float hl = v.curHfL + (v.hfL - v.curHfL) * t, hr = v.curHfR + (v.hfR - v.curHfR) * t;
                v.shL = l + (v.shL - l) * shadowA;
                v.shR = r + (v.shR - r) * shadowA;
                l = v.shL + hl * (l - v.shL);
                r = v.shR + hr * (r - v.shR);
            } else {
                l = r = x;
            }
            const float gL = v.curGainL + (gl - v.curGainL) * t, gR = v.curGainR + (gr - v.curGainR) * t;
            oL[i] += l * gL;
            oR[i] += r * gR;
            v.pos += step;
            if (v.pos >= len) {
                if (v.loop) v.pos -= len;
                else {
                    v.active = false;
                    break;
                }
            }
        }
        {
            const double gm = (double)std::max(gl, gr) * bus_[(int)v.bus] * master_;
            blockExpSq += vsq * gm * gm;
        }
        v.curGainL = gl, v.curGainR = gr;
        v.curItdL = v.itdL, v.curItdR = v.itdR, v.curHfL = v.hfL, v.curHfR = v.hfR;
        v.audible = std::max(gl, gr);
    }

    // Raum: Fruehreflexionen und Fahne aus dem gemeinsamen Hallweg
    roomTimer_ += (float)frames / rate;
    if (roomDirty_ || roomTimer_ > 0.05f) {
        updateRoom();
        roomTimer_ = 0.0f;
        roomDirty_ = std::fabs(cur_.t60 - target_.t60) > 0.01f || std::fabs(cur_.size - target_.size) > 0.01f ||
                     std::fabs(cur_.wet - target_.wet) > 0.002f;
    }
    const float wet = std::max(0.0f, cur_.wet) * (paused_ ? 0.3f : 1.0f);
    // Fruehreflexionen haben ihren eigenen Pegel (sie sind in kleinen Raeumen kaum leiser als der
    // Direktschall), die Fahne folgt wet
    early_.process(send_.data(), erL_.data(), erR_.data(), frames);
    fdn_.process(send_.data(), wetL_.data(), wetR_.data(), frames);
    const float erGain = std::max(0.0f, cur_.early) * (paused_ ? 0.3f : 1.0f);

    // Ducking: laute Ereignisse (Effekte, Kreaturen) druecken die Atmosphaere um bis zu 5 dB
    double e = 0;
    for (int b : {(int)Bus::Effects, (int)Bus::Creatures})
        for (size_t i = 0; i < nf; ++i) e += (double)busL_[b][i] * busL_[b][i] + (double)busR_[b][i] * busR_[b][i];
    const float rmsNow = (float)std::sqrt(e / (2.0 * (double)nf));
    const float tc = (float)frames / rate;
    duckEnv_ += (rmsNow - duckEnv_) * (rmsNow > duckEnv_ ? std::min(1.0f, tc / 0.03f) : std::min(1.0f, tc / 0.6f));
    const float duckTarget = dsp::dbToGain(-5.0f * dsp::smooth01((duckEnv_ - 0.02f) / 0.15f));
    const float duckFrom = duck_;
    duck_ = duckTarget;

    // Summe der Busse, Gleichspannungsfilter, Daempfung (Unterwasser), Begrenzer
    muffleCur_ += (muffle_ - muffleCur_) * std::min(1.0f, tc / 0.15f);
    const float mufA = dsp::onePoleCoef(20000.0f * std::pow(0.025f, muffleCur_), rate);
    const float dcA = dsp::onePoleCoef(18.0f, rate);
    if (fNan_) dcZ_[0] = std::nanf("");  // V13.3 (Test): vergifteter Filterzustand
    for (size_t i = 0; i < nf; ++i) {
        const float t = (float)i / (float)nf;
        const float duck = duckFrom + (duck_ - duckFrom) * t;
        float L = 0, R = 0;
        for (int b = 0; b < kBusCount; ++b) {
            float g = bus_[b];
            if (b == (int)Bus::Ambience) g *= duck;
            L += busL_[b][i] * g;
            R += busR_[b][i] * g;
        }
        L += (wetL_[i] * wet + erL_[i] * erGain) * bus_[(int)Bus::Effects];
        R += (wetR_[i] * wet + erR_[i] * erGain) * bus_[(int)Bus::Effects];
        if (muffleCur_ > 0.002f) {
            muffleZ_[0] = L + (muffleZ_[0] - L) * mufA;
            muffleZ_[1] = R + (muffleZ_[1] - R) * mufA;
            L = muffleZ_[0], R = muffleZ_[1];
        }
        L *= master_, R *= master_;
        // Gleichspannung und Tiefstbass unter ~18 Hz entfernen
        dcZ_[0] = L + (dcZ_[0] - L) * dcA;
        dcZ_[1] = R + (dcZ_[1] - R) * dcA;
        out[i * 2] = L - dcZ_[0];
        out[i * 2 + 1] = R - dcZ_[1];
    }
    limiter_.process(out, frames);
    if (fMute_) std::memset(out, 0, sizeof(float) * 2 * nf);  // V13.3 (Test): haengender stummer Zustand
    measureOutput(out, frames, blockExpSq);
}

// V13.3: Pegel am Master-Ausgang (Spitze und RMS je Sekunde), ungueltige Werte, Stille bei erwartetem Klang
void AudioSystem::measureOutput(float* out, int frames, double blockExpSq) {
    int active = 0;
    for (const auto& v : voices_) active += v.active ? 1 : 0;
    st_.peakVoices = std::max(st_.peakVoices, active);
    st_.frames += (unsigned long long)frames;
    const double rate = (double)sampleRate_;
    clock_ += (double)frames / rate;
    double sq = 0.0, pk = 0.0;
    bool bad = false;
    for (int i = 0; i < frames * 2; ++i) {
        const double x = out[i];
        if (!std::isfinite(x)) {
            bad = true;
            break;
        }
        pk = std::max(pk, std::fabs(x));
        sq += x * x;
    }
    if (bad) {
        // Watchdog: NaN/Inf im Ausgang - Block stumm, Hall/Filter/Begrenzer neu (sonst bliebe es fuer immer still)
        ++st_.nonFinite;
        std::memset(out, 0, sizeof(float) * 2 * (size_t)frames);
        resetDsp();
        sq = pk = 0.0;
        if (clock_ - nanNoteT_ > 5.0) {
            nanNoteT_ = clock_;
            note("WAudio-Watchdog: ungueltige Werte (NaN/Inf) am Master-Ausgang - Hall, Filter und Begrenzer zurueckgesetzt");
        }
    }
    if (silenceCheck_) {
        // Stille-Erkennung: 1 s lang nichts am Master-Ausgang, obwohl die Stimmen im Mittel deutlich Klang liefern.
        // Der erwartete Pegel wird ueber die stille Strecke gemittelt (leise Stellen eines Klangs loesen nichts aus).
        const double outRms = std::sqrt(sq / (2.0 * std::max(1, frames)));
        if (outRms >= kMeasuredSilent) {
            silentSec_ = 0.0, silentExpSq_ = 0.0, silentFrames_ = 0;
        } else {
            silentSec_ += (double)frames / rate;
            silentExpSq_ += blockExpSq;
            silentFrames_ += frames;
        }
        if (silentSec_ >= kSilenceSeconds) {
            const double expRms = std::sqrt(silentExpSq_ / (double)std::max<long long>(1, silentFrames_));
            if (expRms > kExpectAudible) {
                ++st_.silenceAlarms;
                silenceAlarm_ = true;
                note(std::format("WAudio-Watchdog: Master-Ausgang stumm (unter {:.0f} dBFS) bei erwartetem Klang ({:.1f} dBFS) seit {:.1f} s - Engine startet neu",
                                 20.0 * std::log10(kMeasuredSilent), 20.0 * std::log10(expRms), silentSec_));
                resetDsp();
            }
            silentSec_ = 0.0, silentExpSq_ = 0.0, silentFrames_ = 0;  // naechste Sekunde neu bewerten
        }
    }
    fastSq_ += (sq / (2.0 * std::max(1, frames)) - fastSq_) * std::min(1.0, (double)frames / (0.1 * rate));
    st_.outRmsFast = (float)std::sqrt(std::max(0.0, fastSq_));
    winPeak_ = std::max(winPeak_, pk);
    winSq_ += sq;
    winExpSq_ += blockExpSq;
    winFrames_ += frames;
    if (winFrames_ >= sampleRate_) {
        st_.outPeak = (float)winPeak_;
        st_.outRms = (float)std::sqrt(winSq_ / (2.0 * (double)winFrames_));
        st_.expectedRms = (float)std::sqrt(winExpSq_ / (double)winFrames_);
        winPeak_ = winSq_ = winExpSq_ = 0.0;
        winFrames_ = 0;
    }
}

void AudioSystem::note(std::string msg) { notes_.push_back(std::move(msg)); }

void AudioSystem::flushNotes() {
    std::vector<std::string> n;
    {
        std::lock_guard lock(mutex_);
        n.swap(notes_);
    }
    for (const auto& m : n) {
        if (m.empty()) continue;
        if (m[0] == 'W') log::warn("{}", m.substr(1));
        else if (m[0] == 'E') log::error("{}", m.substr(1));
        else log::info("{}", m.substr(m[0] == 'I' ? 1 : 0));
    }
}

bool AudioSystem::silenceAlarmPending() {
    std::lock_guard lock(mutex_);
    const bool a = silenceAlarm_;
    silenceAlarm_ = false;
    return a;
}

void AudioSystem::enableSilenceCheck(bool on) {
    std::lock_guard lock(mutex_);
    silenceCheck_ = on;
    silentSec_ = 0.0, silentExpSq_ = 0.0, silentFrames_ = 0;
}

AudioStats AudioSystem::stats() {
    AudioStats s;
    {
        std::lock_guard lock(mutex_);
        s = st_;
        s.voices = s.loops = s.stopping = s.stuck = 0;
        for (const auto& v : voices_) {
            if (!v.active) continue;
            ++s.voices;
            s.loops += v.loop ? 1 : 0;
            s.stopping += v.stopping ? 1 : 0;
            // das Ausblenden dauert 50 ms; wer nach 250 ms noch "stopping" ist, haengt
            s.stuck += v.stopping && v.stopFrames > (unsigned)(sampleRate_ / 4) ? 1 : 0;
        }
        s.ready = ready_;
        s.mixRate = sampleRate_;
    }
    std::lock_guard lock(statMutex_);
    s.deviceOpen = running_;
    s.device = dev_.device;
    s.deviceRate = dev_.deviceRate, s.deviceChannels = dev_.deviceChannels, s.deviceBits = dev_.deviceBits;
    s.streamRate = dev_.streamRate, s.streamChannels = dev_.streamChannels, s.bufferFrames = dev_.bufferFrames, s.periodMs = dev_.periodMs;
    s.opens = dev_.opens, s.openFailures = dev_.openFailures, s.losses = dev_.losses, s.restarts = dev_.restarts;
    s.lastHr = dev_.lastHr;
    s.lastReason = dev_.lastReason;
    s.underruns = dev_.underruns, s.timeouts = dev_.timeouts, s.mixMsMax = dev_.mixMsMax;
    s.threadHung = hung_;
    return s;
}

static float toDb(float x) { return x > 1e-9f ? 20.0f * std::log10(x) : -180.0f; }

std::string AudioSystem::statsLine() {
    const AudioStats s = stats();
    return std::format(
        "Geraet {} '{}' ({} Hz/{} Kan./{} Bit, Strom {} Hz {} Kan., Mischer {} Hz, Puffer {} Bilder, Periode {:.1f} ms) | geoeffnet {} "
        "fehlgeschl. {} verloren {} Neustarts {} letzter Fehler {} letzter Grund '{}' | Stimmen {}/{} (Schleifen {}, stopping {}, haengend {}, "
        "Spitze {}) | gestartet {} abgewiesen {} (belegt) verworfen {} zurueckgeholt {} ungueltig {} | Ausgang Spitze {:.1f} dBFS RMS {:.1f} dBFS "
        "erwartet {:.1f} dBFS | Bilder {} Unterlaeufe {} Zeitueberschr. {} NaN {} Stille-Alarme {} Mischen max {:.2f} ms (Sperre {:.2f} ms){}",
        s.deviceOpen ? "offen" : "ZU", s.device, s.deviceRate, s.deviceChannels, s.deviceBits, s.streamRate, s.streamChannels, s.mixRate,
        s.bufferFrames, s.periodMs, s.opens, s.openFailures, s.losses, s.restarts, hrText(s.lastHr), s.lastReason, s.voices, kVoices, s.loops,
        s.stopping, s.stuck, s.peakVoices, s.plays, s.allocFails, s.dropped, s.reclaimed, s.badInput, toDb(s.outPeak), toDb(s.outRms),
        toDb(s.expectedRms), s.frames, s.underruns, s.timeouts, s.nonFinite, s.silenceAlarms, s.mixMsMax, s.lockWaitMsMax, s.threadHung ? " | THREAD HAENGT" : "");
}

// --- V13.3: Neustart, Benachrichtigungen, Watchdog, Fehler fuer Tests ----------------------------------------
void AudioSystem::wake() {
    if (wake_) SetEvent((HANDLE)wake_);
}

void AudioSystem::requestRestart(const std::string& reason) {
    {
        std::lock_guard lock(statMutex_);
        dev_.reopenReason = reason;
    }
    reopen_ = true;
    wake();
}

void AudioSystem::onDefaultChanged(const wchar_t* id) {
    std::wstring cur;
    {
        std::lock_guard lock(statMutex_);
        cur = dev_.id;
    }
    if (!running_) {
        wake();  // ohne Geraet: sofort neu versuchen
        return;
    }
    if (!id) requestRestart("kein Standardgeraet mehr");
    else if (cur != id) requestRestart("Standardgeraet gewechselt (z. B. Kopfhoerer eingesteckt)");
}

void AudioSystem::onDeviceState(const wchar_t* id, bool active) {
    if (active) {
        if (!running_) wake();  // ein Geraet ist (wieder) da
        return;
    }
    bool ours = false;
    {
        std::lock_guard lock(statMutex_);
        ours = id && dev_.id == id;
    }
    if (ours) requestRestart("Geraet entfernt oder deaktiviert");
}

void AudioSystem::onSessionDisconnected(int reason) { requestRestart(std::string("Sitzung getrennt: ") + disconnectText(reason)); }

void AudioSystem::onFormatProperty(const wchar_t* id) {
    bool ours = false;
    {
        std::lock_guard lock(statMutex_);
        ours = id && dev_.id == id;
    }
    if (ours) requestRestart("Geraeteformat (Abtastrate/Bittiefe) geaendert");
}

void AudioSystem::onSessionVolume(float volume, bool mute) {
    std::lock_guard lock(mutex_);
    note(std::format("IAudio: Windows-Lautstaerke dieser Sitzung {:.0f} %{}", volume * 100.0f, mute ? ", STUMM geschaltet" : ""));
}

void AudioSystem::watchdog() {
    flushNotes();
    if (!thread_.joinable()) return;
    const long long now = nowMs();
    const unsigned long long hb = heartbeat_;
    if (hb != wdBeat_ || wdBeatT_ == 0) {
        wdBeat_ = hb, wdBeatT_ = now;
        if (hung_) {
            hung_ = false;
            log::info("Audio-Watchdog: Audiothread reagiert wieder");
        }
    } else if (now - wdBeatT_ > 2000 && !hung_) {
        hung_ = true;
        log::error("Audio-Watchdog: Audiothread reagiert seit {} ms nicht (Schritt: {}) - Neustart angefordert", now - wdBeatT_, stepName(step_));
        requestRestart("Audiothread reagierte nicht (Watchdog)");
    }
    // Strom offen, aber nichts gemischt: der Treiber holt keine Daten mehr
    const unsigned long long f = mixed_;
    if (!running_ || f != wdFrames_ || wdFramesT_ == 0) {
        wdFrames_ = f, wdFramesT_ = now, wdStalled_ = false;
    } else if (now - wdFramesT_ > 1500 && !wdStalled_) {
        wdStalled_ = true;
        log::warn("Audio-Watchdog: Strom offen, aber seit {} ms nichts gemischt - Neustart angefordert", now - wdFramesT_);
        requestRestart("Strom stand (Watchdog)");
    }
}

void AudioSystem::injectFault(Fault f, int param) {
    auto* dn = static_cast<DeviceNotifier*>(notifier_);
    auto session = [&](AudioSessionDisconnectReason r) {
        SessionEvents* se = nullptr;
        {
            std::lock_guard lock(statMutex_);
            se = static_cast<SessionEvents*>(sessionEvents_);
            if (se) se->AddRef();
        }
        if (se) {
            se->OnSessionDisconnected(r);  // derselbe Rueckruf, den Windows aufruft
            se->Release();
        } else {
            onSessionDisconnected((int)r);
        }
    };
    std::wstring cur;
    {
        std::lock_guard lock(statMutex_);
        cur = dev_.id;
    }
    log::info("Audio-Test: Fehler {} ({}) ausgeloest", (int)f, param);
    switch (f) {
        case Fault::DeviceInvalidated: fInvalidate_ = 1; break;
        case Fault::DefaultChanged: dn->OnDefaultDeviceChanged(eRender, eConsole, L"{0.0.0.00000000}.{simuliert-kopfhoerer}"); break;
        case Fault::FormatChanged:
            fRate_ = param > 0 ? param : kSynthRate;
            session(DisconnectReasonFormatChanged);
            break;
        case Fault::ExclusiveOverride:
            fInUseUntil_ = nowMs() + std::max(0, param);
            session(DisconnectReasonExclusiveModeOverride);
            break;
        case Fault::DeviceRemoved:
            fNoDeviceUntil_ = nowMs() + std::max(0, param);
            dn->OnDeviceStateChanged(cur.c_str(), DEVICE_STATE_UNPLUGGED);
            break;
        case Fault::StallEvents: fStallUntil_ = nowMs() + std::max(0, param); break;
        case Fault::MuteOutput: fMute_ = true; break;
        case Fault::NanOutput: fNan_ = true; break;
        case Fault::InitFails: fInitFails_ = std::max(0, param); break;
    }
}

// --- Geraetethread -----------------------------------------------------------------------------------------
struct DeviceThread {
    AudioSystem& a;
    Com<IMMDeviceEnumerator> en;
    Com<IMMDevice> dev;
    Com<IAudioClient> client;
    Com<IAudioRenderClient> render;
    Com<IAudioSessionControl> session;
    SessionEvents* events = nullptr;
    HANDLE ev = nullptr;
    UINT32 buf = 0;
    int channels = 2, bits = 32;
    bool isFloat = true;
    std::vector<float> tmp;
    long hr = 0;
    std::string step;
    bool simRemoved = false;
    std::wstring removedId;

    explicit DeviceThread(AudioSystem& s) : a(s) {}

    bool fail(const char* where, HRESULT h) {
        hr = (long)h;
        step = where;
        return false;
    }

    bool open() {
        a.step_ = kOpen;
        hr = 0;
        step.clear();
        const long long now = nowMs();
        long long lapT = now;
        std::string laps;  // V13.3: langsame Schritte beim Oeffnen (ueber 20 ms) ins Protokoll
        auto lap = [&](const char* what) {
            const long long t = nowMs();
            if (t - lapT > 20) laps += std::format("{}{} {} ms", laps.empty() ? "" : ", ", what, t - lapT);
            lapT = t;
        };
        HRESULT h;
        if (!en) {
            h = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)en.put());
            if (FAILED(h)) return fail("CoCreateInstance(MMDeviceEnumerator)", h);
            // Geraetewechsel (Kopfhoerer, USB, Bluetooth), Entfernen, Formatwechsel melden lassen
            h = en->RegisterEndpointNotificationCallback(static_cast<DeviceNotifier*>(a.notifier_));
            if (FAILED(h)) log::warn("Audio: Geraetebenachrichtigung nicht verfuegbar ({})", hrText((long)h));
        }
        if (now < a.fNoDeviceUntil_) {
            if (!simRemoved) {
                simRemoved = true;
                std::lock_guard lock(a.statMutex_);
                removedId = a.dev_.id;
            }
            return fail("GetDefaultAudioEndpoint (Test: Geraet abgezogen)", (HRESULT)0x80070490);
        }
        h = en->GetDefaultAudioEndpoint(eRender, eConsole, dev.put());
        lap("GetDefaultAudioEndpoint");
        if (FAILED(h)) return fail("GetDefaultAudioEndpoint", h);
        std::wstring id;
        {
            LPWSTR p = nullptr;
            if (SUCCEEDED(dev->GetId(&p)) && p) id = p;
            if (p) CoTaskMemFree(p);
        }
        const std::string name = deviceName(dev.get());
        h = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)client.put());
        lap("Activate");
        if (FAILED(h)) return fail("Activate(IAudioClient)", h);
        WAVEFORMATEX* mf = nullptr;
        h = client->GetMixFormat(&mf);
        if (FAILED(h) || !mf) return fail("GetMixFormat", FAILED(h) ? h : E_FAIL);
        std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mfHold(mf, &CoTaskMemFree);
        const int mixRate = (int)mf->nSamplesPerSec, mixCh = mf->nChannels, mixBits = mf->wBitsPerSample;
        REFERENCE_TIME period = 0;
        client->GetDevicePeriod(&period, nullptr);
        // 1. Stereo float in der Rate des Mischers (Windows rechnet in das Geraeteformat um)
        const int want = a.fRate_ > 0 ? a.fRate_.load() : kSynthRate;
        WAVEFORMATEXTENSIBLE wf = stereoFloat(want);
        const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        bool simulated = false;
        if (now < a.fInUseUntil_) {
            h = (HRESULT)0x8889000A;  // Test: AUDCLNT_E_DEVICE_IN_USE (ein anderes Programm haelt das Geraet exklusiv)
            simulated = true;
        } else if (a.fInitFails_ > 0) {
            --a.fInitFails_;
            h = (HRESULT)0x88890008;  // Test: AUDCLNT_E_UNSUPPORTED_FORMAT
            simulated = true;
        } else {
            h = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 400000, 0, &wf.Format, nullptr);
        }
        lap("Initialize");
        int rate = want;
        channels = 2, bits = 32, isFloat = true;
        if (h == (HRESULT)0x88890008 || h == E_INVALIDARG) {
            // 2. genau das Mischformat des Geraets (aeltere Treiber ohne Umrechnung); der Mischer laeuft dann in dessen Rate
            const bool fl = mf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                            (mf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                             IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mf)->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
            const bool pcm = mf->wFormatTag == WAVE_FORMAT_PCM ||
                             (mf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                              IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mf)->SubFormat, KSDATAFORMAT_SUBTYPE_PCM));
            log::warn("Audio: Stereo-float {} Hz abgelehnt ({}{}) - versuche das Mischformat des Geraets ({} Hz, {} Kanaele, {} Bit {})", want,
                      hrText((long)h), simulated ? ", Test" : "", mixRate, mixCh, mixBits, fl ? "float" : pcm ? "PCM" : "unbekannt");
            if (!(fl && mixBits == 32) && !(pcm && (mixBits == 16 || mixBits == 24 || mixBits == 32)))
                return fail("Mischformat nicht unterstuetzt", (HRESULT)0x88890008);
            client.reset();
            h = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)client.put());
            if (FAILED(h)) return fail("Activate(IAudioClient) fuer Mischformat", h);
            h = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 400000, 0, mf, nullptr);
            rate = mixRate, channels = mixCh, bits = mixBits, isFloat = fl;
        }
        if (FAILED(h)) return fail(simulated ? "Initialize (Test)" : "Initialize", h);
        ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!ev) return fail("CreateEvent", HRESULT_FROM_WIN32(GetLastError()));
        if (FAILED(h = client->SetEventHandle(ev))) return fail("SetEventHandle", h);
        if (FAILED(h = client->GetBufferSize(&buf))) return fail("GetBufferSize", h);
        if (FAILED(h = client->GetService(__uuidof(IAudioRenderClient), (void**)render.put()))) return fail("GetService(IAudioRenderClient)", h);
        // Sitzung: Trennung (Formatwechsel, exklusiver Modus, Geraet entfernt, Dienst beendet) melden lassen
        if (SUCCEEDED(client->GetService(__uuidof(IAudioSessionControl), (void**)session.put()))) {
            events = new SessionEvents(&a);
            if (FAILED(session->RegisterAudioSessionNotification(events))) log::warn("Audio: Sitzungsbenachrichtigung nicht verfuegbar");
            std::lock_guard lock(a.statMutex_);
            a.sessionEvents_ = events;
        }
        // Windows-Lautstaerke: stumm geschaltete Sitzung oder Geraet ist keine Stoerung des Spiels, erklaert aber Stille
        {
            Com<ISimpleAudioVolume> sv;
            float v = 1.0f;
            BOOL m = FALSE;
            if (SUCCEEDED(client->GetService(__uuidof(ISimpleAudioVolume), (void**)sv.put())) && SUCCEEDED(sv->GetMasterVolume(&v)) &&
                SUCCEEDED(sv->GetMute(&m)) && (m || v < 0.05f))
                log::warn("Audio: Windows-Lautstaerke dieses Spiels {:.0f} %{}", v * 100.0f, m ? ", STUMM geschaltet" : "");
            Com<IAudioEndpointVolume> epv;
            if (SUCCEEDED(dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)epv.put())) &&
                SUCCEEDED(epv->GetMute(&m)) && SUCCEEDED(epv->GetMasterVolumeLevelScalar(&v)) && (m || v < 0.05f))
                log::warn("Audio: Geraet '{}' ist {} (Lautstaerke {:.0f} %)", name, m ? "STUMM geschaltet" : "fast auf null", v * 100.0f);
        }
        lap("Dienste/Lautstaerke");
        {
            std::lock_guard lock(a.mutex_);
            a.setMixRate(rate);
        }
        // mit Stille vorfuellen, dann starten
        BYTE* d = nullptr;
        if (SUCCEEDED(render->GetBuffer(buf, &d))) render->ReleaseBuffer(buf, AUDCLNT_BUFFERFLAGS_SILENT);
        if (FAILED(h = client->Start())) return fail("Start", h);
        lap("Start");
        {
            std::lock_guard lock(a.statMutex_);
            ++a.dev_.opens;
            a.dev_.device = name;
            a.dev_.id = id;
            a.dev_.deviceRate = mixRate, a.dev_.deviceChannels = mixCh, a.dev_.deviceBits = mixBits;
            a.dev_.streamRate = rate, a.dev_.streamChannels = channels, a.dev_.bufferFrames = buf, a.dev_.periodMs = (float)period / 10000.0f;
        }
        log::info("Audio: WASAPI '{}' geoeffnet (Geraet {} Hz, {} Kanaele, {} Bit; Periode {:.1f} ms) - Strom {} Hz, {} Kanaele {} {} Bit, Puffer {} "
                  "Bilder ({:.1f} ms); Oeffnen {} ms{}{}",
                  name, mixRate, mixCh, mixBits, (double)period / 10000.0, rate, channels, isFloat ? "float" : "PCM", bits, buf,
                  1000.0 * buf / rate, nowMs() - now, laps.empty() ? "" : " - davon ", laps);
        return true;
    }

    HRESULT padding(UINT32& p) {
        if (a.fInvalidate_.exchange(0)) return (HRESULT)0x88890004;  // Test: AUDCLNT_E_DEVICE_INVALIDATED
        return client->GetCurrentPadding(&p);
    }

    std::string lost(const char* where, HRESULT h) {
        {
            std::lock_guard lock(a.statMutex_);
            ++a.dev_.losses;
            a.dev_.lastHr = (long)h;
        }
        return std::format("Geraet verloren ({}: {})", where, hrText((long)h));
    }

    // In das Format des Geraets schreiben (meist Stereo float: direkt)
    void write(BYTE* data, UINT32 frames) {
        if (channels == 2 && isFloat && bits == 32) {
            std::memcpy(data, tmp.data(), (size_t)frames * 2 * sizeof(float));
            return;
        }
        const int bytes = bits / 8;
        for (UINT32 i = 0; i < frames; ++i) {
            const float L = tmp[i * 2], R = tmp[i * 2 + 1];
            for (int c = 0; c < channels; ++c) {
                const float v = std::clamp(channels == 1 ? 0.5f * (L + R) : c == 0 ? L : c == 1 ? R : 0.0f, -1.0f, 1.0f);
                BYTE* q = data + ((size_t)i * channels + c) * bytes;
                if (isFloat) {
                    std::memcpy(q, &v, 4);
                } else if (bits == 16) {
                    const i16 s = (i16)std::lround(v * 32767.0f);
                    std::memcpy(q, &s, 2);
                } else if (bits == 24) {
                    const i32 s = (i32)std::lround(v * 8388607.0f);
                    q[0] = (BYTE)(s & 0xFF), q[1] = (BYTE)((s >> 8) & 0xFF), q[2] = (BYTE)((s >> 16) & 0xFF);
                } else {
                    const i32 s = (i32)std::lround((double)v * 2147483647.0);
                    std::memcpy(q, &s, 4);
                }
            }
        }
    }

    // Laufender Strom; Rueckgabe: Grund fuer das Ende ("" beim Beenden des Spiels)
    std::string run() {
        HANDLE hs[2] = {ev, (HANDLE)a.wake_};
        {
            std::lock_guard lock(a.mutex_);
            a.silenceCheck_ = true;
            a.silentSec_ = 0.0, a.silentExpSq_ = 0.0, a.silentFrames_ = 0;
            a.silenceAlarm_ = false;
        }
        a.running_ = true;
        long long lastEvent = nowMs();
        bool first = true;
        while (!a.stop_) {
            ++a.heartbeat_;
            a.step_ = kWait;
            const DWORD w = WaitForMultipleObjects(2, hs, FALSE, 100);
            if (a.stop_) break;
            if (a.reopen_.exchange(false)) {
                std::lock_guard lock(a.statMutex_);
                return a.dev_.reopenReason.empty() ? std::string("Neustart angefordert") : a.dev_.reopenReason;
            }
            const long long now = nowMs();
            const bool event = w == WAIT_OBJECT_0 && now >= a.fStallUntil_;  // Test: Treiber meldet sich nicht
            if (w == WAIT_TIMEOUT) {
                std::lock_guard lock(a.statMutex_);
                ++a.dev_.timeouts;
            }
            if (!event) {
                if (w == WAIT_OBJECT_0 + 1) continue;  // geweckt ohne Auftrag
                // keine Meldung des Treibers: lebt das Geraet noch?
                a.step_ = kPadding;
                UINT32 p = 0;
                const HRESULT h = padding(p);
                if (FAILED(h)) return lost("GetCurrentPadding", h);
                if (now - lastEvent >= 500) return std::format("Strom haengt: seit {} ms keine Meldung des Treibers", now - lastEvent);
                continue;
            }
            lastEvent = now;
            a.step_ = kPadding;
            UINT32 pad = 0;
            HRESULT h = padding(pad);
            if (FAILED(h)) return lost("GetCurrentPadding", h);
            if (pad == 0 && !first) {
                std::lock_guard lock(a.statMutex_);
                ++a.dev_.underruns;  // Puffer leergelaufen: der Mischer kam zu spaet
            }
            first = false;
            const UINT32 frames = buf - std::min(buf, pad);
            if (!frames) continue;
            a.step_ = kGetBuffer;
            BYTE* data = nullptr;
            if (FAILED(h = render->GetBuffer(frames, &data))) return lost("GetBuffer", h);
            a.step_ = kMix;
            tmp.resize((size_t)frames * 2);
            const auto t0 = std::chrono::steady_clock::now();
            a.mix(tmp.data(), (int)frames);
            const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
            write(data, frames);
            a.step_ = kRelease;
            if (FAILED(h = render->ReleaseBuffer(frames, 0))) return lost("ReleaseBuffer", h);
            a.mixed_ += frames;
            {
                std::lock_guard lock(a.statMutex_);
                a.dev_.mixMsMax = std::max(a.dev_.mixMsMax, ms);
            }
            a.flushNotes();
            if (a.silenceAlarmPending()) return "Stille am Master-Ausgang bei erwartetem Klang";
        }
        return "";
    }

    void close() {
        a.step_ = kClose;
        a.running_ = false;
        {
            std::lock_guard lock(a.mutex_);
            a.silenceCheck_ = false;
        }
        {
            std::lock_guard lock(a.statMutex_);
            a.sessionEvents_ = nullptr;
        }
        if (session && events) session->UnregisterAudioSessionNotification(events);
        if (events) events->Release(), events = nullptr;
        if (client) client->Stop();
        render.reset();
        session.reset();
        client.reset();
        dev.reset();
        if (ev) CloseHandle(ev), ev = nullptr;
        buf = 0;
    }

    // Ohne Geraet: die vergangene Zeit weitermischen und verwerfen (Klaenge laufen ab, Schleifen weiter, nichts staut
    // sich); geweckt wird bei Geraetemeldungen, dann sofort neuer Versuch
    void idle(int ms) {
        a.step_ = kNull;
        const long long until = nowMs() + ms;
        long long last = nowMs();
        std::vector<float> scratch;
        while (!a.stop_) {
            ++a.heartbeat_;
            const DWORD w = WaitForSingleObject((HANDLE)a.wake_, 10);
            const long long now = nowMs();
            const int frames = (int)std::min<long long>((now - last) * a.sampleRate_ / 1000, a.sampleRate_ / 2);
            if (frames > 0) {
                scratch.resize((size_t)frames * 2);
                a.mix(scratch.data(), frames);
                last += (long long)frames * 1000 / a.sampleRate_;
                a.flushNotes();
            }
            // Test: das abgezogene Geraet ist wieder da - Windows meldet es (OnDeviceStateChanged ACTIVE)
            if (simRemoved && now >= a.fNoDeviceUntil_) {
                simRemoved = false;
                static_cast<DeviceNotifier*>(a.notifier_)->OnDeviceStateChanged(removedId.c_str(), DEVICE_STATE_ACTIVE);
            }
            if (w == WAIT_OBJECT_0) {
                a.reopen_ = false;  // ohnehin gleich neuer Versuch
                break;
            }
            if (now >= until) break;
        }
    }
};

void AudioSystem::thread() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // V13.3: Echtzeit-Prioritaet ueber MMCSS - sonst verdraengen die Arbeitsthreads (Welt, Texturen) den Mischer und
    // der Puffer laeuft leer (Unterlaeufe, Knacken, Aussetzer)
    DWORD task = 0;
    HANDLE mm = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
    if (!mm) SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    log::info("Audio: Thread-Prioritaet {}", mm ? "MMCSS 'Pro Audio'" : "TIME_CRITICAL (MMCSS nicht verfuegbar)");
    DeviceThread d(*this);
    int fails = 0;
    long lastHr = 1;
    long long downSince = 0;
    while (!stop_) {
        ++heartbeat_;
        if (!d.open()) {
            d.close();
            ++fails;
            {
                std::lock_guard lock(statMutex_);
                ++dev_.openFailures;
                dev_.lastHr = d.hr;
            }
            const int wait = fails <= 1 ? 100 : fails == 2 ? 250 : fails == 3 ? 500 : fails == 4 ? 1000 : 2000;
            if (d.hr != lastHr || fails == 1)
                log::warn("Audio: Geraet nicht verfuegbar ({}: {}) - Versuch {}, naechster in {} ms (ohne Geraet laeuft der Mischer weiter)", d.step,
                          hrText(d.hr), fails, wait);
            lastHr = d.hr;
            if (!downSince) downSince = nowMs();
            d.idle(wait);
            continue;
        }
        if (downSince) log::info("Audio: wieder verfuegbar nach {} Fehlversuchen ({:.2f} s ohne Ausgabe)", fails, (double)(nowMs() - downSince) / 1000.0);
        fails = 0, lastHr = 1, downSince = 0;
        const std::string why = d.run();
        d.close();
        if (stop_) break;
        downSince = nowMs();
        {
            std::lock_guard lock(statMutex_);
            ++dev_.restarts;
            dev_.lastReason = why;
        }
        log::warn("Audio-Watchdog: {} - Engine startet neu", why);
    }
    d.close();
    if (d.en) d.en->UnregisterEndpointNotificationCallback(static_cast<DeviceNotifier*>(notifier_));
    d.en.reset();
    if (mm) AvRevertMmThreadCharacteristics(mm);
    CoUninitialize();
}

}  // namespace lim::audio
