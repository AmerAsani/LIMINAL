#include "audio/dsp.hpp"

#include <algorithm>

namespace lim::audio::dsp {

float Rand::gauss() {
    float u1 = std::max(uni(), 1e-7f), u2 = uni();
    return std::sqrt(-2.0f * std::log(u1)) * std::cos(kTwoPi * u2);
}

// --- Biquad (Robert Bristow-Johnson, Audio EQ Cookbook) ----------------------------------------
void Biquad::set(Type t, float hz, float q, float rate, float gainDb) {
    const float w = kTwoPi * clampf(hz, 1.0f, rate * 0.49f) / rate;
    const float c = std::cos(w), s = std::sin(w);
    const float al = s / (2.0f * std::max(q, 0.01f));
    const float A = std::pow(10.0f, gainDb / 40.0f);
    float B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
    switch (t) {
        case LP: B0 = (1 - c) / 2, B1 = 1 - c, B2 = (1 - c) / 2, A0 = 1 + al, A1 = -2 * c, A2 = 1 - al; break;
        case HP: B0 = (1 + c) / 2, B1 = -(1 + c), B2 = (1 + c) / 2, A0 = 1 + al, A1 = -2 * c, A2 = 1 - al; break;
        case BP: B0 = al, B1 = 0, B2 = -al, A0 = 1 + al, A1 = -2 * c, A2 = 1 - al; break;
        case Notch: B0 = 1, B1 = -2 * c, B2 = 1, A0 = 1 + al, A1 = -2 * c, A2 = 1 - al; break;
        case Peak: B0 = 1 + al * A, B1 = -2 * c, B2 = 1 - al * A, A0 = 1 + al / A, A1 = -2 * c, A2 = 1 - al / A; break;
        case LowShelf: {
            const float sq = 2.0f * std::sqrt(A) * al;
            B0 = A * ((A + 1) - (A - 1) * c + sq), B1 = 2 * A * ((A - 1) - (A + 1) * c), B2 = A * ((A + 1) - (A - 1) * c - sq);
            A0 = (A + 1) + (A - 1) * c + sq, A1 = -2 * ((A - 1) + (A + 1) * c), A2 = (A + 1) + (A - 1) * c - sq;
            break;
        }
        case HighShelf: {
            const float sq = 2.0f * std::sqrt(A) * al;
            B0 = A * ((A + 1) + (A - 1) * c + sq), B1 = -2 * A * ((A - 1) + (A + 1) * c), B2 = A * ((A + 1) + (A - 1) * c - sq);
            A0 = (A + 1) - (A - 1) * c + sq, A1 = 2 * ((A - 1) - (A + 1) * c), A2 = (A + 1) - (A - 1) * c - sq;
            break;
        }
        case AllPass: B0 = 1 - al, B1 = -2 * c, B2 = 1 + al, A0 = 1 + al, A1 = -2 * c, A2 = 1 - al; break;
    }
    b0 = B0 / A0, b1 = B1 / A0, b2 = B2 / A0, a1 = A1 / A0, a2 = A2 / A0;
}

// --- Modale Resonanz ----------------------------------------------------------------------------
void ModalBank::add(float hz, float t60, float gain) { modes_.push_back({hz, t60, gain}); }

void ModalBank::prepare(float rate) {
    st_.clear();
    for (const Mode& m : modes_) {
        if (m.hz <= 10.0f || m.hz >= rate * 0.45f) continue;
        const float w = kTwoPi * m.hz / rate;
        const float r = t60Coef(m.t60, rate);
        St s;
        s.c1 = 2.0f * r * std::cos(w);
        s.c2 = r * r;
        s.g = m.gain * std::sin(w);  // Impulsantwort hat die Amplitude ~gain
        st_.push_back(s);
    }
}

float ModalBank::run(float force) {
    float out = 0.0f;
    for (St& s : st_) {
        const float y = s.c1 * s.y1 - s.c2 * s.y2 + s.g * force;
        s.y2 = s.y1;
        s.y1 = y;
        out += y;
    }
    return out;
}

// --- Verzoegerung -------------------------------------------------------------------------------
float DelayLine::read(float d) const {
    const int n = (int)buf_.size();
    float pos = (float)w_ - std::max(d, 2.0f);
    while (pos < 0.0f) pos += (float)n;
    const int i = (int)pos;
    const float f = pos - (float)i;
    auto at = [&](int k) {
        k %= n;
        if (k < 0) k += n;
        return buf_[(size_t)k];
    };
    const float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

// --- Feedback-Delay-Netzwerk ---------------------------------------------------------------------
namespace {
constexpr float kFdnMs[8] = {31.7f, 37.9f, 41.3f, 47.9f, 53.1f, 59.3f, 67.1f, 73.9f};
constexpr float kDiffMs[4] = {4.77f, 3.59f, 12.73f, 9.31f};

// orthonormale Hadamard-Matrix 8 x 8 (schnelle Walsh-Hadamard-Transformation)
inline void hadamard8(float* v) {
    for (int h = 1; h < 8; h <<= 1)
        for (int i = 0; i < 8; i += h << 1)
            for (int j = i; j < i + h; ++j) {
                const float a = v[j], b = v[j + h];
                v[j] = a + b;
                v[j + h] = a - b;
            }
    const float s = 0.35355339f;  // 1/sqrt(8)
    for (int i = 0; i < 8; ++i) v[i] *= s;
}
}  // namespace

void Fdn::init(float rate) {
    rate_ = rate;
    for (int i = 0; i < N; ++i) {
        line_[i].init((int)(rate * 0.20f) + 64);
        modPh_[i] = (float)i * 0.83f;
        modRate_[i] = 0.21f + 0.13f * (float)i;
    }
    for (int i = 0; i < 4; ++i) diff_[i].init((int)(kDiffMs[i] * 0.001f * rate), 0.62f);
    pre_.init((int)(rate * 0.2f));
    set(RoomSpec{});
}

void Fdn::set(const RoomSpec& r) {
    sizeCur_ = clampf(r.size, 0.3f, 2.6f);
    preSamples_ = clampf(r.preDelay, 0.0f, 0.18f) * rate_;
    const float tm = clampf(r.t60Mid, 0.08f, 12.0f);
    const float tl = clampf(r.t60Low, 0.08f, 14.0f);
    const float th = clampf(r.t60High, 0.05f, tm);
    for (int i = 0; i < N; ++i) {
        len_[i] = kFdnMs[i] * 0.001f * rate_ * sizeCur_;
        const float m = len_[i];
        const float gm = std::pow(10.0f, -3.0f * m / (rate_ * tm));
        const float gh = std::pow(10.0f, -3.0f * m / (rate_ * th));
        const float gl = std::pow(10.0f, -3.0f * m / (rate_ * tl));
        g_[i] = gm;
        // Hoehen: Einpol-Tiefpass mit Gleichanteil 1 und Nyquist-Verstaerkung gh/gm (Jot)
        const float ratio = clampf(gh / gm, 0.02f, 1.0f);
        lpA_[i] = (1.0f - ratio) / (1.0f + ratio);
        // Tiefen: Kuhschwanz (Einpol bei 250 Hz) auf gl/gm
        lsA_[i] = onePoleCoef(250.0f, rate_);
        lsG_[i] = clampf(gl / gm, 0.3f, 1.0f / std::max(gm, 0.01f) * 0.995f);
    }
    for (int i = 0; i < 4; ++i) diff_[i].g = 0.35f + 0.35f * clampf(r.diffusion, 0.0f, 1.0f);
    gateLen_ = r.gate > 0.0f ? r.gate * rate_ : 0.0f;
}

void Fdn::clear() {
    for (int i = 0; i < N; ++i) {
        line_[i].init(line_[i].size() - 4);
        lpZ_[i] = lsZ_[i] = 0.0f;
    }
    pre_.init(pre_.size() - 4);
    for (auto& d : diff_) std::fill(d.buf.begin(), d.buf.end(), 0.0f);
    gateEnv_ = 1.0f, gateTimer_ = 0.0f, inEnv_ = 0.0f;
}

void Fdn::process(const float* in, float* outL, float* outR, int n) {
    const float modDepth = 0.0004f * rate_;  // +-0.4 ms
    const float phStep = kTwoPi / rate_;
    const float envA = onePoleCoef(30.0f, rate_);
    static constexpr float sgnL[N] = {1, -1, 1, -1, 1, 1, -1, -1};
    static constexpr float sgnR[N] = {1, 1, -1, -1, -1, 1, 1, -1};
    static constexpr float sgnIn[N] = {1, -1, -1, 1, 1, 1, -1, -1};
    for (int s = 0; s < n; ++s) {
        // Vorverzoegerung und Diffusion
        pre_.push(in[s]);
        float x = preSamples_ >= 2.0f ? pre_.read(preSamples_) : in[s];
        for (auto& d : diff_) x = d.run(x);
        // Ausgaenge der Leitungen, Absorption
        float v[N];
        for (int i = 0; i < N; ++i) {
            float len = len_[i];
            if (i < 4) {
                modPh_[i] += modRate_[i] * phStep;
                if (modPh_[i] > kTwoPi) modPh_[i] -= kTwoPi;
                len += modDepth * std::sin(modPh_[i]);
            }
            float y = line_[i].read(len);
            // Hoehen
            lpZ_[i] = y * (1.0f - lpA_[i]) + lpZ_[i] * lpA_[i];
            y = lpZ_[i];
            // Tiefen
            lsZ_[i] = y + (lsZ_[i] - y) * lsA_[i];
            y = y + (lsG_[i] - 1.0f) * lsZ_[i];
            v[i] = y * g_[i];
        }
        float oL = 0, oR = 0;
        for (int i = 0; i < N; ++i) oL += v[i] * sgnL[i], oR += v[i] * sgnR[i];
        hadamard8(v);
        for (int i = 0; i < N; ++i) line_[i].push(v[i] + x * sgnIn[i] * 0.5f);
        // Abbruch (Level 37): Fahne endet hart, wenn der Eingang verstummt
        if (gateLen_ > 0.0f) {
            const float a = std::fabs(in[s]);
            inEnv_ = a > inEnv_ ? a : a + (inEnv_ - a) * envA;
            if (inEnv_ > 1e-4f) gateTimer_ = 0.0f;
            else gateTimer_ += 1.0f;
            const float target = gateTimer_ < gateLen_ ? 1.0f : 0.0f;
            gateEnv_ += (target - gateEnv_) * (target > gateEnv_ ? 0.02f : 0.0015f);
            oL *= gateEnv_, oR *= gateEnv_;
        }
        // Ausgangspegel: Fahne einer Halle (T60 2,5 s) liegt bei wet 0,3 etwa 20 dB unter dem Direktschall
        outL[s] += oL * 1.6f;
        outR[s] += oR * 1.6f;
    }
}

// --- Fruehreflexionen ---------------------------------------------------------------------------
std::vector<EarlyTap> shoeboxReflections(float w, float d, float h, float lx, float ly, float lz, float absorption,
                                         float yaw) {
    std::vector<EarlyTap> taps;
    w = std::max(w, 1.0f), d = std::max(d, 1.0f), h = std::max(h, 1.5f);
    lx = clampf(lx, 0.2f, w - 0.2f), ly = clampf(ly, 0.2f, d - 0.2f), lz = clampf(lz, 0.2f, h - 0.1f);
    const float refl = std::sqrt(clampf(1.0f - absorption, 0.05f, 0.98f));
    const float c = 343.0f;
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    auto add = [&](float ix, float iy, float iz, int order) {
        const float dx = ix - lx, dy = iy - ly, dz = iz - lz;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 0.3f) return;
        const float g = std::pow(refl, (float)order) / (1.0f + dist);
        // rechts = -y relativ zur Blickrichtung (wie im Mischer)
        const float side = (-dx * sy + dy * cy) / dist;  // > 0 links
        const float pan = clampf(-side, -1.0f, 1.0f);
        const float a = (pan + 1.0f) * 0.25f * kPi;
        taps.push_back({dist / c, g * std::cos(a) * 1.41f, g * std::sin(a) * 1.41f});
    };
    // erste Ordnung: sechs Flaechen
    add(-lx, ly, lz, 1);
    add(2 * w - lx, ly, lz, 1);
    add(lx, -ly, lz, 1);
    add(lx, 2 * d - ly, lz, 1);
    add(lx, ly, -lz, 1);
    add(lx, ly, 2 * h - lz, 1);
    // zweite Ordnung: Wand-Wand und Wand-Boden/Decke
    const float xs[2] = {-lx, 2 * w - lx}, ys[2] = {-ly, 2 * d - ly}, zs[2] = {-lz, 2 * h - lz};
    for (float ix : xs)
        for (float iy : ys) add(ix, iy, lz, 2);
    for (float ix : xs)
        for (float iz : zs) add(ix, ly, iz, 2);
    for (float iy : ys)
        for (float iz : zs) add(lx, iy, iz, 2);
    // Flatterecho zwischen parallelen Waenden (nur deutlich in schmalen, langen Raeumen)
    add(lx + 2 * w, ly, lz, 2);
    add(lx - 2 * w, ly, lz, 2);
    add(lx, ly + 2 * d, lz, 2);
    add(lx, ly - 2 * d, lz, 2);
    return taps;
}

void EarlyReflections::process(const float* in, float* outL, float* outR, int n) {
    for (int s = 0; s < n; ++s) {
        line_.push(in[s]);
        float l = 0, r = 0;
        for (const EarlyTap& t : taps_) {
            const float d = std::min(t.delay * rate_, (float)line_.size() - 8.0f);
            const float v = line_.read(std::max(2.0f, d));
            l += v * t.gainL;
            r += v * t.gainR;
        }
        outL[s] += l;
        outR[s] += r;
    }
}

// --- FFT und Faltung ----------------------------------------------------------------------------
void fft(std::vector<float>& re, std::vector<float>& im, bool inverse) {
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(re[i], re[j]), std::swap(im[i], im[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * 3.14159265358979323846 / (double)len * (inverse ? 1.0 : -1.0);
        const float wr = (float)std::cos(ang), wi = (float)std::sin(ang);
        for (size_t i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (size_t j = 0; j < len / 2; ++j) {
                const size_t a = i + j, b = i + j + len / 2;
                const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr, im[b] = im[a] - ti;
                re[a] += tr, im[a] += ti;
                const float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
    if (inverse) {
        const float s = 1.0f / (float)n;
        for (size_t i = 0; i < n; ++i) re[i] *= s, im[i] *= s;
    }
}

std::vector<float> convolve(const std::vector<float>& x, const std::vector<float>& h) {
    if (x.empty() || h.empty()) return x;
    const size_t outLen = x.size() + h.size() - 1;
    size_t n = 1;
    while (n < outLen) n <<= 1;
    std::vector<float> ar(n, 0.0f), ai(n, 0.0f), br(n, 0.0f), bi(n, 0.0f);
    std::copy(x.begin(), x.end(), ar.begin());
    std::copy(h.begin(), h.end(), br.begin());
    fft(ar, ai, false);
    fft(br, bi, false);
    for (size_t i = 0; i < n; ++i) {
        const float r = ar[i] * br[i] - ai[i] * bi[i];
        const float im = ar[i] * bi[i] + ai[i] * br[i];
        ar[i] = r, ai[i] = im;
    }
    fft(ar, ai, true);
    ar.resize(outLen);
    return ar;
}

std::vector<float> makeImpulse(const RoomSpec& r, float rate, std::uint32_t seed, float length) {
    const int n = std::max(16, (int)(length * rate));
    const int pre = (int)(clampf(r.preDelay, 0.0f, 0.2f) * rate);
    std::vector<float> out((size_t)(n + pre), 0.0f);
    Rand rnd(seed);
    Biquad lowLp = Biquad::make(Biquad::LP, 250.0f, 0.7f, rate);
    Biquad midBp1 = Biquad::make(Biquad::HP, 250.0f, 0.7f, rate), midBp2 = Biquad::make(Biquad::LP, 4000.0f, 0.7f, rate);
    Biquad highHp = Biquad::make(Biquad::HP, 4000.0f, 0.7f, rate);
    const float kl = t60Coef(r.t60Low, rate), km = t60Coef(r.t60Mid, rate), kh = t60Coef(r.t60High, rate);
    float el = 1, em = 1, eh = 1;
    const float buildUp = 0.012f * rate * std::max(0.5f, r.size);
    for (int i = 0; i < n; ++i) {
        // erst einzelne Reflexionen (duenn), dann dichtes Rauschen
        const float dens = std::min(1.0f, (float)i / buildUp);
        float w = rnd.gauss();
        if (rnd.uni() > 0.05f + 0.95f * dens * dens) w = 0.0f;
        else if (dens < 1.0f) w *= 1.0f / std::sqrt(0.05f + 0.95f * dens * dens);
        const float lo = lowLp.run(w) * el, mid = midBp2.run(midBp1.run(w)) * em, hi = highHp.run(w) * eh;
        el *= kl, em *= km, eh *= kh;
        out[(size_t)(i + pre)] = lo + mid + hi;
    }
    // Energie normieren
    double e = 0;
    for (float v : out) e += (double)v * v;
    const float s = e > 0 ? (float)(1.0 / std::sqrt(e)) : 1.0f;
    for (float& v : out) v *= s;
    return out;
}

// --- Begrenzer ----------------------------------------------------------------------------------
void Limiter::init(float rate, float lookaheadMs, float releaseMs, float ceilingDb) {
    la_ = std::max(8, (int)(rate * lookaheadMs * 0.001f));
    bufL_.assign((size_t)la_, 0.0f);
    bufR_.assign((size_t)la_, 0.0f);
    peak_.assign((size_t)la_, 0.0f);
    pos_ = 0;
    ceiling_ = dbToGain(ceilingDb);
    rel_ = std::exp(-1.0f / (rate * releaseMs * 0.001f));
    gainNow_ = 1.0f;
}

void Limiter::process(float* x, int frames) {
    const float att = 1.0f - std::exp(-7.0f / (float)la_);  // innerhalb der Vorausschau sicher unten
    for (int i = 0; i < frames; ++i) {
        const float l = x[i * 2], r = x[i * 2 + 1];
        peak_[pos_] = std::max(std::fabs(l), std::fabs(r));
        float pk = 0.0f;
        for (float p : peak_) pk = std::max(pk, p);
        const float target = pk > ceiling_ ? ceiling_ / pk : 1.0f;
        if (target < gainNow_) gainNow_ += (target - gainNow_) * att;
        else gainNow_ = target + (gainNow_ - target) * rel_;
        const float dl = bufL_[pos_], dr = bufR_[pos_];
        bufL_[pos_] = l, bufR_[pos_] = r;
        if (++pos_ >= (size_t)la_) pos_ = 0;
        // Sicherheitsnetz: harte Grenze knapp unter 0 dBFS (greift praktisch nie)
        x[i * 2] = clampf(dl * gainNow_, -0.999f, 0.999f);
        x[i * 2 + 1] = clampf(dr * gainNow_, -0.999f, 0.999f);
    }
}

// --- Messung ------------------------------------------------------------------------------------
float loudnessDb(const std::vector<float>& x, float rate) {
    Biquad pre = Biquad::make(Biquad::HighShelf, 1681.0f, 0.7071f, rate, 4.0f);
    Biquad rlb = Biquad::make(Biquad::HP, 38.0f, 0.5f, rate);
    const size_t block = (size_t)(rate * 0.4f), hop = block / 4;
    std::vector<float> k(x.size());
    for (size_t i = 0; i < x.size(); ++i) k[i] = rlb.run(pre.run(x[i]));
    std::vector<double> blocks;
    for (size_t s = 0; s + block <= k.size() || (s == 0 && !k.empty()); s += hop) {
        const size_t e = std::min(k.size(), s + block);
        double ms = 0;
        for (size_t i = s; i < e; ++i) ms += (double)k[i] * k[i];
        ms /= (double)std::max<size_t>(1, e - s);
        blocks.push_back(ms);
        if (e == k.size()) break;
    }
    // absolute (-70) und relative (-10 dB) Schwelle
    auto lufs = [](double ms) { return -0.691 + 10.0 * std::log10(std::max(ms, 1e-12)); };
    double sum = 0;
    int cnt = 0;
    for (double b : blocks)
        if (lufs(b) > -70.0) sum += b, ++cnt;
    if (!cnt) return -100.0f;
    const double rel = lufs(sum / cnt) - 10.0;
    double sum2 = 0;
    int cnt2 = 0;
    for (double b : blocks)
        if (lufs(b) > rel && lufs(b) > -70.0) sum2 += b, ++cnt2;
    return (float)lufs(cnt2 ? sum2 / cnt2 : sum / cnt);
}

float peakAbs(const std::vector<float>& x) {
    float m = 0;
    for (float v : x) m = std::max(m, std::fabs(v));
    return m;
}

}  // namespace lim::audio::dsp
