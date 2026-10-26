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
#include <mmdeviceapi.h>
#include <objbase.h>
#include <propidl.h>
#include <propsys.h>

namespace lim::audio {

namespace {

constexpr int kVoices = 96;
constexpr float kSpeedOfSound = 343.0f;
constexpr float kHeadRadius = 0.0875f;

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

}  // namespace

AudioSystem::AudioSystem() { voices_.resize(kVoices); }

AudioSystem::~AudioSystem() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
}

bool AudioSystem::init(JobSystem& jobs, const std::string& dataDir) {
    sounds_ = synthesizeAll(jobs, sampleRate_);
    // Eigene Klaenge aus data/sounds ersetzen die erzeugten oder ergaenzen Varianten
    for (const auto& f : paths::listFiles(paths::join(dataDir, "sounds"), "", ".wav")) {
        std::string name = paths::fileName(f);
        name = name.substr(0, name.size() - 4);
        Sound s;
        if (loadWav(f, s)) {
            auto it = sounds_.find(name);
            const std::string base = variantBase(name);
            s.loop = it != sounds_.end() ? it->second.loop
                                         : (!base.empty() && sounds_.count(base + "_0") ? sounds_[base + "_0"].loop : false);
            sounds_[name] = std::move(s);
            log::info("Klang ersetzt/ergaenzt: {}", name);
        }
    }
    initOffline(jobs);
    running_ = false;
    thread_ = std::thread([this] { thread(); });
    return true;
}

void AudioSystem::initOffline(JobSystem& jobs) {
    if (sounds_.empty()) sounds_ = synthesizeAll(jobs, sampleRate_);
    groups_.clear();
    std::vector<std::string> keys;
    for (auto& [k, v] : sounds_) keys.push_back(k);
    std::sort(keys.begin(), keys.end());
    for (const auto& k : keys) {
        const std::string base = variantBase(k);
        if (!base.empty()) groups_[base].push_back(&sounds_[k]);
    }
    fdn_.init((float)sampleRate_);
    early_.init((float)sampleRate_);
    limiter_.init((float)sampleRate_, 1.5f, 120.0f, -1.0f);
    cur_ = target_;
    roomDirty_ = true;
    running_ = true;  // play() nimmt Klaenge an; ohne Thread wird nur ueber mixForTest gemischt
}

std::vector<std::string> AudioSystem::names() const {
    std::vector<std::string> n;
    for (auto& [k, v] : sounds_) n.push_back(k);
    std::sort(n.begin(), n.end());
    return n;
}

int AudioSystem::variants(const std::string& base) const {
    auto it = groups_.find(base);
    return it == groups_.end() ? 0 : (int)it->second.size();
}

const Sound* AudioSystem::find(const std::string& name) const {
    auto it = sounds_.find(name);
    if (it != sounds_.end()) return &it->second;
    auto g = groups_.find(name);
    if (g != groups_.end() && !g->second.empty()) return g->second.front();
    return nullptr;
}

const Sound* AudioSystem::pick(const std::string& name) {
    auto g = groups_.find(name);
    if (g != groups_.end() && !g->second.empty() && !sounds_.count(name)) {
        const int n = (int)g->second.size();
        int& last = lastVariant_[name];
        int k = rand_.irange(0, n - 1);
        if (n > 1 && k == last) k = (k + 1 + rand_.irange(0, n - 2)) % n;
        last = k;
        return g->second[(size_t)k];
    }
    const Sound* s = find(name);
    // V11: unbekannte Namen einmal melden (ein vertippter Name waere sonst einfach still)
    if (!s && !sounds_.empty() && missing_.insert(name).second) log::warn("Klang fehlt: {}", name);
    return s;
}

int AudioSystem::allocVoice(int priority) {
    for (size_t i = 0; i < voices_.size(); ++i)
        if (!voices_[i].active) return (int)i;
    // die am wenigsten hoerbare einmalige Stimme mit gleicher oder niedrigerer Prioritaet ersetzen
    int best = -1;
    float bestA = 1e9f;
    for (size_t i = 0; i < voices_.size(); ++i) {
        const Voice& v = voices_[i];
        if (v.loop || v.priority > priority) continue;
        const float a = v.audible * (float)v.priority;
        if (a < bestA) bestA = a, best = (int)i;
    }
    if (best < 0) ++st_.allocFails;  // V13.3: alle Stimmen belegt (Schleifen werden nie verdraengt)
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
    if (!running_) {
        std::lock_guard lock(mutex_);
        ++st_.dropped;
        return;
    }
    std::lock_guard lock(mutex_);
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
    if (!running_) {
        std::lock_guard lock(mutex_);
        ++st_.dropped;
        return;
    }
    std::lock_guard lock(mutex_);
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
    bus_[(int)Bus::Effects] = effects;
    bus_[(int)Bus::Ambience] = ambience;
}

void AudioSystem::setBusVolume(Bus b, float v) {
    std::lock_guard lock(mutex_);
    bus_[(int)b] = std::max(0.0f, v);
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
    muffle_ = dsp::clampf(m, 0.0f, 1.0f);
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
void AudioSystem::mix(float* out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    std::lock_guard lock(mutex_);
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
    for (auto& v : voices_) {
        if (!v.active || !v.sound || v.sound->samples.empty()) continue;
        if (v.stopping) v.stopFrames += (unsigned)frames;  // V13.3: Diagnose (haengt eine Stimme im Ausblenden?)
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
        if (gain < 1e-5f && v.curGainL < 1e-5f && v.curGainR < 1e-5f && v.loop) {
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
            winExpSq_ += vsq * gm * gm;
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
    measureOutput(out, frames);
}

// V13.3: Pegel am Master-Ausgang (Spitze und RMS je Sekunde) und Stimmenzahl
void AudioSystem::measureOutput(const float* out, int frames) {
    int active = 0;
    for (const auto& v : voices_) active += v.active ? 1 : 0;
    st_.peakVoices = std::max(st_.peakVoices, active);
    st_.frames += (unsigned long long)frames;
    for (int i = 0; i < frames * 2; ++i) {
        const double x = out[i];
        if (!std::isfinite(x)) {
            ++st_.nonFinite;
            continue;
        }
        winPeak_ = std::max(winPeak_, std::fabs(x));
        winSq_ += x * x;
    }
    winFrames_ += frames;
    if (winFrames_ >= sampleRate_) {
        st_.outPeak = (float)winPeak_;
        st_.outRms = (float)std::sqrt(winSq_ / (2.0 * (double)winFrames_));
        st_.expectedRms = (float)std::sqrt(winExpSq_ / (double)winFrames_);
        winPeak_ = winSq_ = winExpSq_ = 0.0;
        winFrames_ = 0;
    }
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
        s.ready = !sounds_.empty();
    }
    std::lock_guard lock(statMutex_);
    s.deviceOpen = running_;
    s.device = dev_.device;
    s.deviceRate = dev_.deviceRate, s.deviceChannels = dev_.deviceChannels, s.deviceBits = dev_.deviceBits;
    s.streamRate = dev_.streamRate, s.bufferFrames = dev_.bufferFrames, s.periodMs = dev_.periodMs;
    s.opens = dev_.opens, s.openFailures = dev_.openFailures, s.losses = dev_.losses, s.restarts = dev_.restarts;
    s.lastHr = dev_.lastHr;
    s.underruns = dev_.underruns, s.timeouts = dev_.timeouts, s.mixMsMax = dev_.mixMsMax;
    return s;
}

static float toDb(float x) { return x > 1e-9f ? 20.0f * std::log10(x) : -180.0f; }

std::string AudioSystem::statsLine() {
    const AudioStats s = stats();
    return std::format(
        "Geraet {} '{}' ({} Hz/{} Kan./{} Bit, Strom {} Hz, Puffer {} Bilder, Periode {:.1f} ms) | geoeffnet {} fehlgeschl. {} verloren {} "
        "Neustarts {} letzter Fehler 0x{:08X} | Stimmen {}/{} (Schleifen {}, stopping {}, haengend {}, Spitze {}) | gestartet {} "
        "abgewiesen {} (belegt) verworfen {} (ohne Geraet) zurueckgeholt {} | Ausgang Spitze {:.1f} dBFS RMS {:.1f} dBFS erwartet {:.1f} dBFS | "
        "Bilder {} Unterlaeufe {} Zeitueberschr. {} ungueltig {} Stille-Alarme {} Mischen max {:.2f} ms",
        s.deviceOpen ? "offen" : "ZU", s.device, s.deviceRate, s.deviceChannels, s.deviceBits, s.streamRate, s.bufferFrames, s.periodMs,
        s.opens, s.openFailures, s.losses, s.restarts, (unsigned long)s.lastHr, s.voices, (int)voices_.size(), s.loops, s.stopping, s.stuck,
        s.peakVoices, s.plays, s.allocFails, s.dropped, s.reclaimed, toDb(s.outPeak), toDb(s.outRms), toDb(s.expectedRms), s.frames,
        s.underruns, s.timeouts, s.nonFinite, s.silenceAlarms, s.mixMsMax);
}

// V13.3: Name eines Endpunkts (PKEY_Device_FriendlyName)
static std::string deviceName(IMMDevice* dev) {
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

void AudioSystem::thread() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    long lastLoggedHr = 0;
    while (!stop_) {
        Com<IMMDeviceEnumerator> en;
        Com<IMMDevice> dev;
        Com<IAudioClient> client;
        Com<IAudioRenderClient> render;
        HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        // V13.3: jeden Schritt einzeln pruefen und den Fehlercode protokollieren
        HRESULT hr = S_OK;
        const char* step = "";
        auto call = [&](const char* what, HRESULT h) {
            if (FAILED(hr)) return false;
            hr = h, step = what;
            return SUCCEEDED(h);
        };
        bool ok = call("CoCreateInstance", CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                                            (void**)en.put())) &&
                  call("GetDefaultAudioEndpoint", en->GetDefaultAudioEndpoint(eRender, eConsole, dev.put())) &&
                  call("Activate", dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)client.put()));
        UINT32 bufFrames = 0;
        int mixRate = 0, mixCh = 0, mixBits = 0;
        REFERENCE_TIME period = 0;
        if (ok) {
            WAVEFORMATEX* mf = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&mf)) && mf) {
                mixRate = (int)mf->nSamplesPerSec, mixCh = mf->nChannels, mixBits = mf->wBitsPerSample;
                CoTaskMemFree(mf);
            }
            client->GetDevicePeriod(&period, nullptr);
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
            ok = call("Initialize", client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 400000, 0, &wf.Format, nullptr)) &&
                 call("SetEventHandle", client->SetEventHandle(ev)) && call("GetBufferSize", client->GetBufferSize(&bufFrames)) &&
                 call("GetService", client->GetService(__uuidof(IAudioRenderClient), (void**)render.put())) &&
                 call("Start", client->Start());
        }
        if (!ok) {
            running_ = false;
            CloseHandle(ev);
            {
                std::lock_guard lock(statMutex_);
                ++dev_.openFailures;
                dev_.lastHr = (long)hr;
            }
            if ((long)hr != lastLoggedHr) log::warn("Audio: Geraet nicht verfuegbar ({}: 0x{:08X}) - neuer Versuch alle 2 s", step, (unsigned long)hr);
            lastLoggedHr = (long)hr;
            // spaeter erneut versuchen (Geraet eingesteckt ...)
            for (int i = 0; i < 20 && !stop_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        lastLoggedHr = 0;
        const std::string name = deviceName(dev.get());
        {
            std::lock_guard lock(statMutex_);
            ++dev_.opens;
            dev_.device = name;
            dev_.deviceRate = mixRate, dev_.deviceChannels = mixCh, dev_.deviceBits = mixBits;
            dev_.streamRate = sampleRate_, dev_.bufferFrames = bufFrames, dev_.periodMs = (float)period / 10000.0f;
        }
        log::info("Audio: WASAPI '{}' (Geraet {} Hz, {} Kanaele, {} Bit; Periode {:.1f} ms), Strom {} Hz Stereo float, Puffer {} Bilder ({:.1f} ms)",
                  name, mixRate, mixCh, mixBits, (double)period / 10000.0, sampleRate_, bufFrames, 1000.0 * bufFrames / sampleRate_);
        running_ = true;
        std::vector<float> tmp;
        bool first = true;
        while (!stop_) {
            if (WaitForSingleObject(ev, 200) != WAIT_OBJECT_0) {
                std::lock_guard lock(statMutex_);
                ++dev_.timeouts;
                continue;
            }
            UINT32 padding = 0;
            if (FAILED(hr = client->GetCurrentPadding(&padding))) break;  // Geraet weg -> neu verbinden
            if (padding == 0 && !first) {
                std::lock_guard lock(statMutex_);
                ++dev_.underruns;  // Puffer leergelaufen
            }
            first = false;
            UINT32 frames = bufFrames - padding;
            if (!frames) continue;
            BYTE* data = nullptr;
            if (FAILED(hr = render->GetBuffer(frames, &data))) break;
            tmp.resize((size_t)frames * 2);
            const auto t0 = std::chrono::steady_clock::now();
            mix(tmp.data(), (int)frames);
            const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::memcpy(data, tmp.data(), tmp.size() * sizeof(float));
            render->ReleaseBuffer(frames, 0);
            std::lock_guard lock(statMutex_);
            dev_.mixMsMax = std::max(dev_.mixMsMax, ms);
        }
        if (!stop_) {
            std::lock_guard lock(statMutex_);
            ++dev_.losses;
            dev_.lastHr = (long)hr;
            log::warn("Audio: Strom unterbrochen (0x{:08X}) - verbinde neu", (unsigned long)hr);
        }
        client->Stop();
        CloseHandle(ev);
    }
    running_ = false;
    CoUninitialize();
}

}  // namespace lim::audio
