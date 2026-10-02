#include "audio/audio.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "core/com_ptr.hpp"
#include "core/jobs.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <objbase.h>

namespace lim::audio {

namespace {

// Einfacher WAV-Leser (PCM 16 Bit oder Float, mono/stereo) fuer eigene Klaenge
bool loadWav(const std::string& path, int rate, Sound& out) {
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
    if (!pcm || channels < 1 || srcRate <= 0) return false;
    size_t frames = pcmSize / (size_t)(channels * bits / 8);
    std::vector<float> mono(frames);
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
            }
        }
        mono[i] = s / channels;
    }
    // Abtastrate angleichen (linear)
    double ratio = (double)srcRate / rate;
    size_t n = (size_t)(frames / ratio);
    out.samples.resize(n);
    for (size_t i = 0; i < n; ++i) {
        double x = i * ratio;
        size_t a = (size_t)x;
        double f = x - a;
        float s0 = mono[std::min(a, frames - 1)], s1 = mono[std::min(a + 1, frames - 1)];
        out.samples[i] = (float)(s0 + (s1 - s0) * f);
    }
    return true;
}

}  // namespace

AudioSystem::AudioSystem() { voices_.resize(64); }

AudioSystem::~AudioSystem() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
}

bool AudioSystem::init(JobSystem& jobs, const std::string& dataDir) {
    sounds_ = synthesizeAll(jobs, sampleRate_);
    reverb_.init(sampleRate_);
    // Eigene Klaenge aus data/sounds ersetzen die erzeugten
    for (const auto& f : paths::listFiles(paths::join(dataDir, "sounds"), "", ".wav")) {
        std::string name = paths::fileName(f);
        name = name.substr(0, name.size() - 4);
        Sound s;
        if (loadWav(f, sampleRate_, s)) {
            s.loop = sounds_.count(name) ? sounds_[name].loop : false;
            sounds_[name] = std::move(s);
            log::info("Klang ersetzt: {}", name);
        }
    }
    thread_ = std::thread([this] { thread(); });
    return true;
}

void AudioSystem::initOffline(JobSystem& jobs) {
    sounds_ = synthesizeAll(jobs, sampleRate_);
    reverb_.init(sampleRate_);
    running_ = true;  // play() nimmt Klaenge an, gemischt wird nur ueber mixForTest
}

const Sound* AudioSystem::find(const std::string& name) const {
    auto it = sounds_.find(name);
    return it == sounds_.end() ? nullptr : &it->second;
}

int AudioSystem::allocVoice() {
    for (size_t i = 0; i < voices_.size(); ++i)
        if (!voices_[i].active) return (int)i;
    // aeltesten einmaligen Klang ersetzen
    for (size_t i = 0; i < voices_.size(); ++i)
        if (!voices_[i].loop) return (int)i;
    return -1;
}

void AudioSystem::play(const std::string& name, float volume, float pitch, Bus bus) {
    const Sound* s = find(name);
    if (!s || !running_) return;
    std::lock_guard lock(mutex_);
    int v = allocVoice();
    if (v < 0) return;
    Voice& vo = voices_[(size_t)v];
    vo = Voice{};
    vo.id = nextId_++;
    vo.sound = s;
    vo.volume = volume;
    vo.pitch = pitch;
    vo.bus = bus;
    vo.send = bus == Bus::Ui ? 0.0f : 0.45f;  // eigene Schritte usw. klingen im Raum nach
    vo.active = true;
}

void AudioSystem::playAt(const std::string& name, vec3 pos, float volume, float radius, float pitch, float occlusion) {
    const Sound* s = find(name);
    if (!s || !running_) return;
    std::lock_guard lock(mutex_);
    int v = allocVoice();
    if (v < 0) return;
    Voice& vo = voices_[(size_t)v];
    vo = Voice{};
    vo.id = nextId_++;
    vo.sound = s;
    vo.volume = volume;
    vo.pitch = pitch;
    vo.positional = true;
    vo.at = pos;
    vo.radius = radius;
    vo.occlusion = occlusion;
    vo.send = 0.8f;
    vo.active = true;
}

int AudioSystem::startLoop(const std::string& name, float volume, Bus bus) {
    const Sound* s = find(name);
    if (!s) return -1;
    std::lock_guard lock(mutex_);
    int v = allocVoice();
    if (v < 0) return -1;
    Voice& vo = voices_[(size_t)v];
    vo = Voice{};
    vo.id = nextId_++;
    vo.sound = s;
    vo.volume = volume;
    vo.loop = true;
    vo.bus = bus;
    vo.send = bus == Bus::Ambience ? 0.1f : (bus == Bus::Ui ? 0.0f : 0.6f);
    vo.active = true;
    // zufaelliger Startpunkt, damit gleiche Schleifen nicht phasengleich laufen
    vo.pos = (double)((u32)vo.id * 2654435761u % (u32)std::max<size_t>(1, s->samples.size()));
    return vo.id;
}

void AudioSystem::setLoopVolume(int id, float volume) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id) v.volume = volume;
}

void AudioSystem::setLoopPosition(int id, vec3 pos, float radius) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id) {
            v.positional = true;
            v.at = pos;
            v.radius = radius;
        }
}

void AudioSystem::setLoopSpatial(int id, const Spatial& s) {
    std::lock_guard lock(mutex_);
    for (auto& v : voices_)
        if (v.active && v.id == id) {
            v.positional = true;
            v.at = s.pos;
            v.radius = s.radius;
            v.occlusion = std::clamp(s.occlusion, 0.0f, 1.0f);
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
    listener_ = pos;
    listenerYaw_ = yaw;
}

void AudioSystem::setVolumes(float master, float effects, float ambience) {
    std::lock_guard lock(mutex_);
    master_ = master;
    effects_ = effects;
    ambience_ = ambience;
}

void AudioSystem::setAcoustics(const Acoustics& a) {
    std::lock_guard lock(mutex_);
    reverb_.target = a;
}

void AudioSystem::setPaused(bool paused) {
    std::lock_guard lock(mutex_);
    paused_ = paused;
}

// --- V6: Nachhall ------------------------------------------------------------------------------
// Freeverb-Aufbau (Jezar): 8 parallele Kammfilter mit gedaempfter Rueckkopplung, dann 4 Allpaesse,
// rechter Kanal leicht versetzt (Stereobreite). Die Rueckkopplung jedes Kamms ergibt sich aus der
// gewuenschten Nachhallzeit (60 dB Abfall nach t60): g = 10^(-3 * Laenge / (t60 * Rate)).
void AudioSystem::Reverb::init(int rate) {
    static const int kComb[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static const int kAp[4] = {556, 441, 341, 225};
    const double k = (double)rate / 44100.0;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 8; ++i) comb[ch][i].buf.assign((size_t)((kComb[i] + ch * 23) * k), 0.0f);
        for (int i = 0; i < 4; ++i) ap[ch][i].buf.assign((size_t)((kAp[i] + ch * 23) * k), 0.0f);
    }
    pre.assign((size_t)(rate * 0.15), 0.0f);
    cur = target;
    ready = true;
}

void AudioSystem::Reverb::update(int rate, int frames) {
    // weich zum Ziel (Raumwechsel ohne Spruenge), Zeitkonstante ~0.5 s
    float k = 1.0f - std::exp(-(float)frames / ((float)rate * 0.5f));
    cur.t60 += (target.t60 - cur.t60) * k;
    cur.damping += (target.damping - cur.damping) * k;
    cur.wet += (target.wet - cur.wet) * k;
    cur.preDelay += (target.preDelay - cur.preDelay) * k;
    float t60 = std::clamp(cur.t60, 0.1f, 8.0f);
    for (int i = 0; i < 8; ++i)
        fb[i] = std::min(0.985f, std::pow(10.0f, -3.0f * (float)comb[0][i].buf.size() / (t60 * (float)rate)));
    damp = std::clamp(cur.damping, 0.0f, 0.95f);
    wet = std::max(0.0f, cur.wet);
    preDelay = std::clamp(cur.preDelay, 0.0f, 0.14f);
}

void AudioSystem::Reverb::process(const float* in, float* outLR, int frames, int rate) {
    if (!ready) return;
    const size_t pn = pre.size();
    const size_t pd = std::min(pn - 1, (size_t)(preDelay * (float)rate));
    for (int i = 0; i < frames; ++i) {
        pre[preIdx] = in[i] * 0.015f;
        float x = pre[(preIdx + pn - pd) % pn];
        preIdx = (preIdx + 1) % pn;
        for (int ch = 0; ch < 2; ++ch) {
            float acc = 0.0f;
            for (int c = 0; c < 8; ++c) {
                Comb& cb = comb[ch][c];
                float y = cb.buf[cb.idx];
                cb.store = y * (1.0f - damp) + cb.store * damp;
                cb.buf[cb.idx] = x + cb.store * fb[c];
                if (++cb.idx >= cb.buf.size()) cb.idx = 0;
                acc += y;
            }
            for (int a = 0; a < 4; ++a) {
                Allpass& al = ap[ch][a];
                float y = al.buf[al.idx];
                al.buf[al.idx] = acc + y * 0.5f;
                if (++al.idx >= al.buf.size()) al.idx = 0;
                acc = y - acc;
            }
            outLR[i * 2 + ch] += acc * wet * 3.0f;  // Freeverb: Ausgangsverstaerkung 3
        }
    }
}

// --- Mischer -------------------------------------------------------------------------------------
// V6: raeumlicher Klang je Quelle:
//   - Abstand: weicher Abfall bis zur Reichweite, dazu Luftdaempfung (ferne Quellen dumpfer)
//   - Richtung: Panorama mit gleicher Leistung plus Laufzeitunterschied zwischen den Ohren
//     (bis ~0.6 ms) und leichter Kopfschatten-Daempfung fuer Quellen hinter dem Hoerer
//   - Verdeckung: hinter Waenden leiser und deutlich dumpfer (Tiefpass)
//   - Nachhall: jede Quelle sendet anteilig in den Hall des aktuellen Raums
void AudioSystem::mix(float* out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    std::lock_guard lock(mutex_);
    send_.assign((size_t)frames, 0.0f);
    const vec3 right{-std::sin(listenerYaw_), std::cos(listenerYaw_), 0.0f};
    const vec3 fwd{std::cos(listenerYaw_), std::sin(listenerYaw_), 0.0f};
    const float rate = (float)sampleRate_;
    const float maxItd = 0.00062f * rate;
    for (auto& v : voices_) {
        if (!v.active || !v.sound || v.sound->samples.empty()) continue;
        // Spiel pausiert: Welt-Klaenge stumm, Oberflaeche weiter
        float busVol = v.bus == Bus::Ambience ? ambience_ : (v.bus == Bus::Ui ? 1.0f : effects_);
        float pauseMul = (paused_ && v.bus != Bus::Ui) ? 0.25f : 1.0f;
        float gain = v.volume * busVol * master_ * pauseMul;
        float pan = 0.0f, cutoff = 20000.0f;
        v.itd = 0.0f;
        if (v.positional) {
            vec3 d = v.at - listener_;
            float dist = length(d);
            float att = std::max(0.0f, 1.0f - dist / std::max(v.radius, 0.1f));
            gain *= att * att * (1.0f - 0.6f * v.occlusion);
            if (dist > 0.05f) {
                vec3 n = d / dist;
                pan = std::clamp(dot(n, right), -1.0f, 1.0f) * 0.85f;
                v.itd = std::clamp(dot(n, right), -1.0f, 1.0f) * maxItd;
                if (dot(n, fwd) < 0.0f) cutoff *= 1.0f - 0.45f * std::min(1.0f, -dot(n, fwd) * 1.5f);
            }
            cutoff *= std::exp(-dist / 45.0f);                  // Luftdaempfung
            cutoff = std::min(cutoff, 20000.0f * std::pow(0.03f, v.occlusion));  // Verdeckung bis ~600 Hz
        }
        v.lpCoef = cutoff >= 19999.0f ? 0.0f : std::exp(-2.0f * kPi * std::max(cutoff, 80.0f) / rate);
        // gleichmaessige Leistung ueber das Panorama
        float a = (pan + 1.0f) * 0.25f * kPi;
        float gl = gain * std::cos(a) * 1.41421356f, gr = gain * std::sin(a) * 1.41421356f;
        const std::vector<float>& s = v.sound->samples;
        const double len = (double)s.size();
        const size_t n = s.size();
        auto sampleAt = [&](double p) -> float {
            if (p < 0.0) {
                if (!v.loop) return 0.0f;
                p += len;
            }
            size_t i0 = (size_t)p;
            if (i0 >= n) i0 = n - 1;
            double f = p - (double)i0;
            size_t i1 = i0 + 1 < n ? i0 + 1 : (v.loop ? 0 : i0);
            return (float)(s[i0] + (s[i1] - s[i0]) * f);
        };
        float stateL = v.lpState, stateR = v.lpStateR, stateL2 = v.lpState2, stateR2 = v.lpStateR2;
        for (int i = 0; i < frames; ++i) {
            float t = (float)i / (float)frames;
            float l = v.curGainL + (gl - v.curGainL) * t, r = v.curGainR + (gr - v.curGainR) * t;
            float itd = v.curItd + (v.itd - v.curItd) * t;
            if (v.stopping) {
                v.fade -= 1.0f / 2400.0f;  // 50 ms ausblenden
                if (v.fade <= 0.0f) {
                    v.active = false;
                    break;
                }
            }
            if (v.fadeIn < 1.0f) v.fadeIn = std::min(1.0f, v.fadeIn + v.fadeInStep);
            float env = v.fade * v.fadeIn;
            // Laufzeitunterschied: das abgewandte Ohr hoert einen etwas aelteren Teil des Klangs
            float sl = sampleAt(v.pos - (double)std::max(0.0f, itd)) * env;
            float sr = sampleAt(v.pos - (double)std::max(0.0f, -itd)) * env;
            if (v.lpCoef > 0.0f) {  // zwei Stufen: 12 dB je Oktave, Waende schlucken Hoehen stark
                stateL = sl + (stateL - sl) * v.lpCoef;
                stateR = sr + (stateR - sr) * v.lpCoef;
                stateL2 = stateL + (stateL2 - stateL) * v.lpCoef;
                stateR2 = stateR + (stateR2 - stateR) * v.lpCoef;
                sl = stateL2, sr = stateR2;
            }
            out[i * 2] += sl * l;
            out[i * 2 + 1] += sr * r;
            send_[(size_t)i] += (sl * l + sr * r) * 0.5f * v.send;
            v.pos += v.pitch;
            if (v.pos >= len) {
                if (v.loop) v.pos -= len;
                else {
                    v.active = false;
                    break;
                }
            }
        }
        v.lpState = stateL;
        v.lpStateR = stateR;
        v.lpState2 = stateL2;
        v.lpStateR2 = stateR2;
        v.curGainL = gl;
        v.curGainR = gr;
        v.curItd = v.itd;
    }
    // Nachhall des aktuellen Raums
    reverb_.update(sampleRate_, frames);
    float pauseWet = paused_ ? 0.25f : 1.0f;
    for (float& x : send_) x *= pauseWet;
    reverb_.process(send_.data(), out, frames, sampleRate_);
    // weicher Begrenzer gegen Uebersteuern
    for (int i = 0; i < frames * 2; ++i) {
        float x = out[i];
        out[i] = x / (1.0f + std::fabs(x) * 0.35f);
    }
}
void AudioSystem::thread() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (!stop_) {
        Com<IMMDeviceEnumerator> en;
        Com<IMMDevice> dev;
        Com<IAudioClient> client;
        Com<IAudioRenderClient> render;
        HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        bool ok = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                             __uuidof(IMMDeviceEnumerator), (void**)en.put())) &&
                  SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, dev.put())) &&
                  SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)client.put()));
        UINT32 bufFrames = 0;
        if (ok) {
            WAVEFORMATEXTENSIBLE wf{};
            wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            wf.Format.nChannels = 2;
            wf.Format.nSamplesPerSec = (DWORD)sampleRate_;
            wf.Format.wBitsPerSample = 32;
            wf.Format.nBlockAlign = 8;
            wf.Format.nAvgBytesPerSec = (DWORD)sampleRate_ * 8;
            wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            wf.Samples.wValidBitsPerSample = 32;
            wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            // Windows rechnet in das Geraeteformat um (Win7+)
            DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                          AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            ok = SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 400000, 0, &wf.Format, nullptr)) &&
                 SUCCEEDED(client->SetEventHandle(ev)) && SUCCEEDED(client->GetBufferSize(&bufFrames)) &&
                 SUCCEEDED(client->GetService(__uuidof(IAudioRenderClient), (void**)render.put())) &&
                 SUCCEEDED(client->Start());
        }
        if (!ok) {
            running_ = false;
            CloseHandle(ev);
            // spaeter erneut versuchen (Geraet eingesteckt ...)
            for (int i = 0; i < 20 && !stop_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (!running_) log::info("Audio: WASAPI, {} Hz Stereo, Puffer {} Bilder", sampleRate_, bufFrames);
        running_ = true;
        std::vector<float> tmp;
        while (!stop_) {
            if (WaitForSingleObject(ev, 200) != WAIT_OBJECT_0) continue;
            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding))) break;  // Geraet weg -> neu verbinden
            UINT32 frames = bufFrames - padding;
            if (!frames) continue;
            BYTE* data = nullptr;
            if (FAILED(render->GetBuffer(frames, &data))) break;
            tmp.resize((size_t)frames * 2);
            mix(tmp.data(), (int)frames);
            std::memcpy(data, tmp.data(), tmp.size() * sizeof(float));
            render->ReleaseBuffer(frames, 0);
        }
        client->Stop();
        CloseHandle(ev);
    }
    running_ = false;
    CoUninitialize();
}

}  // namespace lim::audio
