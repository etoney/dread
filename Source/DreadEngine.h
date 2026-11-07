// DreadEngine.h — framework-free DSP core for the DREAD horror sound-design synth.
// No JUCE dependency, so it can be unit-tested / rendered offline.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dread
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline float mtof (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
inline float centsToRatio (float c) { return std::pow (2.0f, c / 1200.0f); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
// transparent below ~0.7, then curves smoothly to a ceiling of 0.98
inline float softLimit (float x)
{
    const float a = std::abs (x);
    if (a <= 0.7f) return x;
    const float y = 0.7f + 0.28f * std::tanh ((a - 0.7f) / 0.28f);
    return x < 0 ? -y : y;
}

//==============================================================================
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float bi()  { return uni() * 2.0f - 1.0f; }                           // [-1,1)
};

// Smooth random LFO (cosine-interpolated sample & hold), output in [-1,1]
struct Drift
{
    float from = 0, to = 0, phase = 0, inc = 0.0001f;
    void setRate (float hz, float sr) { inc = hz / sr; }
    float tick (Rng& r)
    {
        phase += inc;
        if (phase >= 1.0f) { phase -= 1.0f; from = to; to = r.bi(); }
        const float w = 0.5f - 0.5f * std::cos (kPi * phase);
        return from + (to - from) * w;
    }
};

// Band-limited saw (polyBLEP)
struct Saw
{
    float phase = 0;
    float tick (float dt)
    {
        float t = phase;
        float v = 2.0f * t - 1.0f;
        if (t < dt)             { t /= dt;          v -= t + t - t * t - 1.0f; }
        else if (t > 1.0f - dt) { t = (t - 1.0f) / dt; v -= t * t + t + t + 1.0f; }
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return v;
    }
};

// Cytomic TPT state-variable filter
struct SVF
{
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1;
    void set (float fc, float q, float sr)
    {
        fc = clampf (fc, 15.0f, sr * 0.45f);
        const float g = std::tan (kPi * fc / sr);
        k  = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    float bp (float x) { float l, b, h; tick (x, l, b, h); return b; }
    float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
    void reset() { ic1 = ic2 = 0; }
};

struct OnePole
{
    float z = 0, a = 0;
    void setCutoff (float fc, float sr) { a = std::exp (-kTwoPi * fc / sr); }
    float lp (float x) { z = x + (z - x) * a; return z; }
};

// Fractional delay line
struct DelayLine
{
    std::vector<float> buf; int w = 0, mask = 0;
    void init (int minSize)
    {
        int n = 1; while (n < minSize) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float delaySamples) const
    {
        const float d = clampf (delaySamples, 1.0f, (float) mask - 2.0f);
        float rp = (float) w - d;
        while (rp < 0) rp += (float) (mask + 1);
        const int i0 = (int) rp; const float f = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * f;
    }
};

struct ADSR
{
    enum State { Idle, Attack, Decay, Sustain, Release } st = Idle;
    float v = 0, aInc = 0, dCoef = 0, rCoef = 0, sus = 1;
    void set (float a, float d, float s, float r, float sr)
    {
        aInc  = 1.0f / std::max (1.0f, a * sr);
        dCoef = std::exp (-4.6f / std::max (1.0f, d * sr));
        rCoef = std::exp (-4.6f / std::max (1.0f, r * sr));
        sus   = s;
    }
    void on()  { st = Attack; }
    void off() { if (st != Idle) st = Release; }
    bool active() const { return st != Idle; }
    float tick()
    {
        switch (st)
        {
            case Attack:  v += aInc; if (v >= 1.0f) { v = 1.0f; st = Decay; } break;
            case Decay:   v = sus + (v - sus) * dCoef; if (std::abs (v - sus) < 1e-4f) st = Sustain; break;
            case Sustain: v = sus; break;
            case Release: v *= rCoef; if (v < 1e-5f) { v = 0; st = Idle; } break;
            default: break;
        }
        return v;
    }
};

//==============================================================================
struct Params
{
    // Layers
    float droneLevel = 0.8f, droneDetune = 0.35f, droneFold = 0.2f, droneSub = 0.3f;
    float metalLevel = 0.4f, metalResonance = 0.7f, metalTone = 0.5f;
    float airLevel = 0.25f, airMotion = 0.4f;
    // Voice
    float cutoff = 2500.0f, resonance = 0.25f, envAmount = 0.3f;
    float attack = 1.2f, decay = 1.0f, sustain = 0.8f, release = 3.0f;
    // Macros
    float dread = 0.3f, torment = 0.15f;
    // FX
    float ringMix = 0.0f, ringFreq = 120.0f, wobble = 0.15f;
    float gateDepth = 0.0f; int gateRate = 1; std::array<bool, 16> gateSteps { true,false,true,true, false,true,false,true, true,false,false,true, false,true,true,false };
    float delayMix = 0.15f, delayFeedback = 0.45f; int delayTime = 1;
    float reverbMix = 0.35f, reverbSize = 0.7f;
    float stingLevel = 0.8f; bool stingZone = true;
    float outputDb = -6.0f;
    // Extra sources
    float pianoLevel = 0.0f, pianoBreak = 0.5f;
    float whisperLevel = 0.0f, whisperDemon = 0.4f;
    float screamLevel = 0.0f, screamRasp = 0.4f;
    // Distortion
    float distDrive = 0.0f, distTone = 0.7f, distMix = 1.0f; int distType = 0;
    // Chains + sting type
    float chainLevel = 0.0f, chainRattle = 0.4f, chainRust = 0.5f;
    int stingType = 0;
    // Twisted Symphony
    float symphonyLevel = 0.0f, symphonyTwist = 0.5f, symphonySection = 0.3f;
};

//==============================================================================
// Multi-mode distortion (one instance per channel).
// Types: 0 Tube, 1 Tape, 2 Fuzz, 3 Wavefold, 4 Bitcrush, 5 Blown Speaker
class Distortion
{
public:
    static constexpr int kNumTypes = 6;
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); hold = 0; holdCount = 0; crackle = 0; }
    void setTone (float tone) { toneLp.setCutoff (700.0f * std::pow (24.0f, tone), sr); }   // 700 Hz .. ~16.8 kHz

    float process (float x, float drive, int type, float mix)
    {
        const float g = 1.0f + drive * drive * 45.0f;
        float y;
        switch (type)
        {
            case 0: // Tube: asymmetric soft clip (even harmonics)
                y = std::tanh (x * g + 0.25f) - 0.2449f;
                y *= 1.0f / (1.0f + drive * 2.2f);
                break;
            case 1: // Tape: gentle saturation that compresses
            {
                const float v = x * (1.0f + drive * drive * 18.0f);
                y = v / (1.0f + std::abs (v));
                y *= 1.0f / (1.0f + drive * 1.6f);
                break;
            }
            case 2: // Fuzz: hard clip + rectified octave-up
            {
                const float v = clampf (x * g * 2.0f, -1.0f, 1.0f);
                y = (v * 0.7f + std::abs (v) * 0.45f * drive) / (1.0f + drive * 3.0f);
                break;
            }
            case 3: // Wavefold: sine folder, gets metallic fast
                y = std::sin (x * (1.0f + drive * 14.0f) * 1.5708f) / (1.0f + drive * 2.5f);
                break;
            case 4: // Bitcrush: bit depth 16 -> 3 bits, sample-rate down to ~1/40
            {
                const int factor = 1 + (int) (drive * drive * 40.0f);
                if (++holdCount >= factor) { holdCount = 0; hold = x; }
                const float bits = 16.0f - drive * 13.0f;
                const float steps = std::pow (2.0f, bits - 1.0f);
                y = std::round (hold * steps) / steps;
                break;
            }
            default: // Blown Speaker: harsh asymmetric clip, a cone that bottoms out, crackling dropouts
            {
                float v = std::tanh (x * g * 1.5f);
                v = std::min (v, 0.55f);
                if (crackle > 0) { --crackle; v *= 0.08f; }
                else if (rng.uni() < drive * 0.004f * std::abs (v) * 4.0f) crackle = 20 + (int) (rng.uni() * 300.0f);
                y = v / (1.0f + drive * 1.8f);
                break;
            }
        }
        y = toneLp.lp (y);
        return x + (y - x) * mix;
    }

private:
    float sr = 48000, hold = 0; int holdCount = 0, crackle = 0;
    Rng rng; OnePole toneLp;
};

//==============================================================================
// BROKEN PIANO — additive/modal piano: stretched (inharmonic) partials on detuned string pairs,
// hammer thump, per-key fixed detune (same key is always wrong the same way), dead keys, string rattle.
class PianoLayer
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    void start (int note, float freq, float vel, float brk)
    {
        Rng key ((uint32_t) (note * 2654435761u) ^ 0xA5A5A5u);  // per-key, deterministic
        const float keyDetune = key.bi() * brk * 45.0f;
        dead = key.uni() < brk * 0.22f;
        rattle = brk * (0.15f + 0.35f * key.uni());
        const float f0 = freq * centsToRatio (keyDetune);
        const float B = 0.0003f * (1.0f + brk * 10.0f);
        const float pairCents = 0.6f + brk * 14.0f;
        const float baseT = 4.0f * std::pow (261.6f / f0, 0.45f) * (dead ? 0.12f : 1.0f);
        Rng r ((uint32_t) note * 7919u + (uint32_t) (vel * 1000.0f));

        num = 0;
        for (int n = 1; n <= 12; ++n)
        {
            const float fn = f0 * (float) n * std::sqrt (1.0f + B * (float) (n * n));
            if (fn > sr * 0.42f) break;
            float amp = std::pow ((float) n, -0.9f) * (0.4f + 0.6f * std::pow (vel, (float) n * 0.15f));
            if (r.uni() < brk * 0.3f) amp *= 0.08f;                    // missing partials
            const float T = baseT / (1.0f + 0.45f * (float) (n - 1));
            for (int s = 0; s < 2; ++s)
            {
                const float f  = fn * centsToRatio ((s == 0 ? -0.5f : 0.5f) * pairCents + r.bi() * brk * 3.0f);
                const float w  = kTwoPi * f / sr;
                const float rr = std::exp (-6.9f / (T * sr));
                auto& p = parts[(size_t) num++];
                p.c = 2.0f * rr * std::cos (w); p.r2 = rr * rr;
                p.s2 = 0; p.s1 = amp * std::sin (w);
                p.pan = s == 0 ? 0.35f : 0.65f;
            }
        }
        hammerT = 0; hammerAmp = 0.5f * vel * (dead ? 2.0f : 1.0f);
        hammer.set (std::min (f0 * 3.0f, 4500.0f), 1.5f, sr);
    }

    void tick (Rng& rng, float& l, float& rt)
    {
        float sl = 0, sr_ = 0;
        for (int i = 0; i < num; ++i)
        {
            auto& p = parts[(size_t) i];
            const float y = p.c * p.s1 - p.r2 * p.s2;
            p.s2 = p.s1; p.s1 = y;
            sl += y * (1.0f - p.pan); sr_ += y * p.pan;
        }
        // rattle: noise amplitude-modulated by the string signal
        const float nz = rng.bi();
        sl += sl * nz * rattle; sr_ += sr_ * nz * rattle;
        // hammer thump
        if (hammerT < 0.05f * sr)
        {
            const float h = hammer.bp (rng.bi()) * hammerAmp * std::exp (-hammerT / (0.006f * sr));
            sl += h; sr_ += h; hammerT += 1.0f;
        }
        l = sl * 1.3f; rt = sr_ * 1.3f;
    }

private:
    struct Partial { float c = 0, r2 = 0, s1 = 0, s2 = 0, pan = 0.5f; };
    float sr = 48000, rattle = 0, hammerT = 1e9f, hammerAmp = 0;
    bool dead = false; int num = 0;
    std::array<Partial, 24> parts {};
    SVF hammer;
};

//==============================================================================
// Formant tables {F1, F2, F3}
static constexpr float kVowels[7][3] = {
    { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 },
    { 300, 870, 2240 },  { 660, 1700, 2400 }, { 490, 1350, 1690 } };

// DEMON WHISPERS — two whisperers speaking random syllables (noise through gliding formants,
// sibilant consonants), plus an optional period-doubled growl through a lowered vocal tract.
class WhisperLayer
{
public:
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void start (float freq)
    {
        growlFreq = std::max (35.0f, freq * 0.5f);
        sizeScale = std::pow (freq / 130.0f, 0.12f);
        for (auto& w : wh) { w.t = 0; w.dur = 0.05f + rng.uni() * 0.2f; w.amp = 0; }
    }

    void control (float motion, float demon)
    {
        const float scale = sizeScale * (1.0f - demon * 0.42f);
        const float glide = 0.25f + motion * 0.5f;
        for (auto& w : wh)
            for (int k = 0; k < 3; ++k)
            {
                w.cur[k] += (w.tgt[k] - w.cur[k]) * glide;
                w.f[k].set (w.cur[k] * scale, 9.0f + (float) k * 3.0f, sr);
            }
        growlRough.setCutoff (60.0f, sr);
        sib.set (5500.0f, 0.8f, sr);
    }

    void tick (float motion, float demon, float& l, float& r)
    {
        // growl source (shared): saw with jitter + period doubling
        jit = jit * 0.999f + rng.bi() * 0.001f;
        const float gf = growlFreq * (1.0f + jit * 8.0f + growlRough.lp (rng.bi()) * 0.6f);
        const float before = growl.phase;
        float g = growl.tick (gf / sr);
        if (growl.phase < before) odd = ! odd;
        g *= odd ? 1.0f : 0.45f;

        l = r = 0;
        for (auto& w : wh)
        {
            w.t += 1.0f / sr;
            if (w.t >= w.dur) newSyllable (w, motion);
            const float x = w.t / w.dur;
            const float env = w.amp * std::pow (std::max (0.0f, std::sin (kPi * x)), 0.8f);
            const float src = rng.bi() * (1.0f - demon * 0.4f) + g * demon * 1.6f * (0.35f + 0.65f * env / std::max (0.05f, w.amp));
            float y = w.f[0].bp (src) * 1.0f + w.f[1].bp (src) * 0.7f + w.f[2].bp (src) * 0.45f;
            y *= env;
            w.cons *= 0.9985f;
            y += sib.hp (rng.bi()) * w.cons * 0.8f;
            l += y * (1.0f - w.pan); r += y * w.pan;
        }
        l *= 0.95f; r *= 0.95f;
    }

private:
    struct Whisperer { SVF f[3]; float cur[3] { 500, 1500, 2500 }, tgt[3] { 500, 1500, 2500 };
                       float t = 0, dur = 0.1f, amp = 0, pan = 0.5f, cons = 0; };
    void newSyllable (Whisperer& w, float motion)
    {
        w.t = 0;
        w.dur = (0.07f + rng.uni() * 0.3f) * (1.4f - motion * 0.8f);
        const bool gap = rng.uni() < 0.22f;
        w.amp = gap ? 0.0f : 0.35f + rng.uni() * 0.65f;
        const int v = (int) (rng.uni() * 7.0f) % 7;
        for (int k = 0; k < 3; ++k) w.tgt[k] = kVowels[v][k] * (0.9f + rng.uni() * 0.2f);
        w.pan = 0.1f + rng.uni() * 0.8f;
        if (! gap && rng.uni() < 0.45f) w.cons = 0.6f + rng.uni() * 0.4f;
    }
    float sr = 48000, growlFreq = 55, sizeScale = 1, jit = 0; bool odd = false;
    Rng rng; Saw growl; OnePole growlRough; SVF sib;
    std::array<Whisperer, 2> wh;
};

// SCREAMS — glottal saw with a rising pitch contour, growing wild vibrato and jitter,
// pitch-tracking open-vowel formants, breath, rasp (rough AM + subharmonics) and drive.
class ScreamLayer
{
public:
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void start (float freq)
    {
        base = freq; t = 0; vibPhase = rng.uni();
        vibRate = 5.5f + rng.uni() * 3.0f;
        pan = 0.2f + rng.uni() * 0.6f;
        vowelMix = rng.uni();
        jitter.setRate (14.0f, sr);
        rough.setCutoff (180.0f, sr);
        for (auto& f : form) f.reset();
    }

    void control (float rasp)
    {
        const float f = curFreq;
        const float a = vowelMix + 0.15f * std::sin (t * 1.7f);
        const float f1 = std::max (750.0f, f * 1.08f);
        const float f2 = 1250.0f + a * 700.0f;
        const float f3 = 2850.0f + a * 300.0f;
        form[0].set (f1, 4.0f, sr); form[1].set (f2, 7.0f, sr); form[2].set (f3, 9.0f, sr);
        breathHp.set (1800.0f, 0.7f, sr);
        drive = 1.0f + rasp * 5.0f;
    }

    void tick (float rasp, float& l, float& r)
    {
        const float tt = t; t += 1.0f / sr;
        const float slide = -7.0f * std::exp (-tt * 9.0f);                       // swoop up into the scream
        const float vibDepth = 0.15f + 0.6f * std::min (1.0f, tt / 0.8f);
        vibPhase += vibRate / sr; if (vibPhase >= 1.0f) vibPhase -= 1.0f;
        const float semis = slide + std::sin (kTwoPi * vibPhase) * vibDepth + jitter.tick (rng) * (0.25f + rasp * 0.7f);
        curFreq = base * std::pow (2.0f, semis / 12.0f);

        const float before = glottis.phase;
        float src = glottis.tick (std::min (curFreq, sr * 0.3f) / sr);
        if (glottis.phase < before) odd = ! odd;
        if (odd) src *= 1.0f - rasp * 0.55f;                                       // subharmonic
        src *= 1.0f + rough.lp (rng.bi()) * rasp * 9.0f;                           // roughness
        src += breathHp.hp (rng.bi()) * 0.35f;

        float y = form[0].bp (src) * 1.0f + form[1].bp (src) * 0.8f + form[2].bp (src) * 0.5f;
        y = std::tanh (y * 0.25f * drive) / std::sqrt (drive);
        l = y * (1.0f - pan) * 2.6f; r = y * pan * 2.6f;
    }

private:
    float sr = 48000, base = 440, t = 0, vibPhase = 0, vibRate = 6, pan = 0.5f, vowelMix = 0.5f, curFreq = 440, drive = 1;
    bool odd = false;
    Rng rng; Saw glottis; Drift jitter; OnePole rough; SVF breathHp;
    std::array<SVF, 3> form;
};

//==============================================================================
// TWISTED SYMPHONY — a small orchestra section per note: 6 players on the note, 2 an octave below,
// each with their own vibrato and slowly wandering intonation. TWIST spreads the tuning, makes the
// players slide away from each other, adds a minor-2nd and tritone cluster, and lets the whole section
// sag flat the longer a note is held. SECTION blends bowed strings (body resonances + rosin) to brass.
class SymphonyLayer
{
public:
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void start (float freq, float vel)
    {
        base = freq; t = 0; brassEnv = 0; velocity = vel;
        for (int i = 0; i < kPlayers; ++i)
        {
            auto& pl = players[(size_t) i];
            pl.osc.phase = rng.uni();
            pl.vibRate = 4.6f + rng.uni() * 1.8f; pl.vibPh = rng.uni();
            pl.detune = rng.bi(); pl.pan = 0.15f + 0.7f * ((float) (i % 6) / 5.0f);
            pl.drift.setRate (0.07f + rng.uni() * 0.25f, sr);
            pl.inc = freq / sr;
        }
        for (auto& f : body) f.reset();
        brassL.reset(); brassR.reset();
    }

    // called every 16 samples
    void control (float twist, float section, int blockLen)
    {
        const float dt = (float) blockLen / sr;
        const float sag = -twist * 2.2f * std::min (1.0f, t / 7.0f);                 // semitones
        for (int i = 0; i < kPlayers; ++i)
        {
            auto& pl = players[(size_t) i];
            pl.vibPh += pl.vibRate * dt; if (pl.vibPh >= 1.0f) pl.vibPh -= 1.0f;
            const float d = pl.drift.tick (rng);
            for (int k = 1; k < blockLen; ++k) pl.drift.tick (rng);                  // advance drift at audio rate
            const float cents = pl.detune * (4.0f + twist * 50.0f)
                              + d * twist * 130.0f
                              + std::sin (kTwoPi * pl.vibPh) * (11.0f + twist * 28.0f)
                              + sag * 100.0f * (0.6f + 0.4f * std::abs (pl.detune));
            float mul = 1.0f;
            if (i >= 6 && i < 8) mul = 0.5f;                                      // low section
            if (i == 8) mul = 1.0595f;                                           // minor 2nd
            if (i == 9) mul = 1.4142f;                                           // tritone
            pl.inc = std::min (0.45f, base * mul * centsToRatio (cents) / sr);
        }
        clusterGain = twist * twist * 0.55f;
        // strings body resonances
        body[0].set (290.0f, 2.0f, sr); body[1].set (720.0f, 2.2f, sr);
        body[2].set (2600.0f, 1.6f, sr); body[3].set (3500.0f, 2.5f, sr);
        // brass: brightness swells in with the note
        brassEnv += (1.0f - brassEnv) * std::min (1.0f, dt * 7.0f);
        const float bc = std::min (sr * 0.4f, base * (1.5f + 9.0f * brassEnv * (0.5f + 0.5f * velocity)));
        brassL.set (bc, 0.9f, sr); brassR.set (bc, 0.9f, sr);
        rosin.set (4500.0f, 0.7f, sr);
        mixSection = section;
        t += dt;
    }

    void tick (float& l, float& r)
    {
        float sl = 0, sr_ = 0;
        for (int i = 0; i < kPlayers; ++i)
        {
            auto& pl = players[(size_t) i];
            float v = pl.osc.tick (pl.inc);
            if (i >= 8) v *= clusterGain;
            else if (i >= 6) v *= 0.8f;
            sl += v * (1.0f - pl.pan); sr_ += v * pl.pan;
        }
        // strings: body resonances + a little rosin noise
        const float nz = rosin.hp (rng.bi()) * 0.08f;
        const float mono = (sl + sr_) * 0.5f;
        const float bodyRes = body[0].bp (mono) * 0.5f + body[1].bp (mono) * 0.45f + body[2].bp (mono) * 0.3f + body[3].bp (mono) * 0.2f;
        const float strL = sl * 0.35f + bodyRes + nz, strR = sr_ * 0.35f + bodyRes + nz;
        // brass: closed-then-opening lowpass with a bit of blat
        const float brL = std::tanh (brassL.lp (sl) * 1.6f), brR = std::tanh (brassR.lp (sr_) * 1.6f);
        l = (strL * (1.0f - mixSection) + brL * mixSection * 0.9f) * 0.32f;
        r = (strR * (1.0f - mixSection) + brR * mixSection * 0.9f) * 0.32f;
    }

private:
    static constexpr int kPlayers = 10;
    struct Player { Saw osc; Drift drift; float inc = 0.01f, vibRate = 5, vibPh = 0, detune = 0, pan = 0.5f; };
    float sr = 48000, base = 220, t = 0, brassEnv = 0, velocity = 1, clusterGain = 0, mixSection = 0;
    Rng rng; std::array<Player, kPlayers> players; std::array<SVF, 4> body; SVF brassL, brassR, rosin;
};

//==============================================================================
// RUSTY CHAINS — a stochastic stream of link-on-link impacts (with little cascades, like a chain
// settling) into three banks of inharmonic metal resonators, plus rusty stick-slip grinding and creaks.
class ChainLayer
{
public:
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void start (float freq, float vel)
    {
        const float size = std::pow (freq / 261.6f, 0.35f);   // lower notes = heavier chain
        static constexpr float ratios[kModes] = { 1.0f, 1.47f, 2.09f, 2.56f, 3.21f };
        static constexpr float bankBase[kBanks] = { 1650.0f, 2450.0f, 420.0f };
        for (int b = 0; b < kBanks; ++b)
            for (int m = 0; m < kModes; ++m)
            {
                auto& md = banks[b][m];
                md.f = bankBase[b] * size * ratios[m] * (0.93f + 0.14f * rng.uni());
                md.f = std::min (md.f, sr * 0.45f);
                md.y1 = md.y2 = 0;
            }
        intensity = 0.5f + 0.5f * vel;
        countdown = 1; follow = 0; lastRust = -1.0f;
        groan.setRate (0.35f, sr); creakDrift.setRate (0.6f, sr);
    }

    void control (float rattle, float rust)
    {
        rate = 5.0f + std::pow (rattle, 1.4f) * 70.0f;
        if (std::abs (rust - lastRust) > 0.002f)
        {
            lastRust = rust;
            for (int b = 0; b < kBanks; ++b)
                for (int m = 0; m < kModes; ++m)
                {
                    auto& md = banks[b][m];
                    const float T = (b == 2 ? 0.45f : 0.22f) / (1.0f + 0.5f * (float) m) * (1.0f - rust * 0.65f);
                    const float rr = std::exp (-6.9f / (std::max (0.01f, T) * sr));
                    md.c = 2.0f * rr * std::cos (kTwoPi * md.f / sr); md.r2 = rr * rr; md.g = (1.0f - rr) * 6.0f;
                }
        }
        clickHp.set (1500.0f, 0.7f, sr);
        grind.set (1900.0f + rust * 900.0f, 1.6f, sr);
        creak.set (650.0f + creakDrift.from * 250.0f, 22.0f, sr);
    }

    void tick (float rattle, float rust, float& l, float& r)
    {
        // schedule impacts (Poisson) and short follow-up cascades
        if (--countdown <= 0)
        {
            hit (0.3f + 0.7f * rng.uni(), rng.uni() < 0.18f ? 2 : (rng.uni() < 0.5f ? 0 : 1));
            follow = (int) (rng.uni() * (1.0f + rattle * 4.0f));
            followTimer = (int) ((0.004f + rng.uni() * 0.025f) * sr);
            countdown = (int) (-std::log (std::max (1e-4f, rng.uni())) / rate * sr) + 1;
        }
        if (follow > 0 && --followTimer <= 0)
        {
            hit (amp * (0.35f + 0.4f * rng.uni()), rng.uni() < 0.7f ? bank : (int) (rng.uni() * 2.99f));
            --follow; followTimer = (int) ((0.004f + rng.uni() * 0.03f) * sr);
        }

        // excitation click
        float x = 0;
        if (clickLeft > 0) { x = clickHp.hp (rng.bi()) * amp * ((float) clickLeft / (float) clickLen); --clickLeft; }

        float out[kBanks] {};
        for (int b = 0; b < kBanks; ++b)
        {
            const float in = (b == bank) ? x : 0.0f;
            for (int m = 0; m < kModes; ++m)
            {
                auto& md = banks[b][m];
                const float y = md.c * md.y1 - md.r2 * md.y2 + in * md.g;
                md.y2 = md.y1; md.y1 = y; out[b] += y;
            }
        }

        // rust: stick-slip grinding crackle + occasional creak
        const float slow = 0.5f + 0.5f * groan.tick (rng);
        float crackle = 0;
        if (rng.uni() < (150.0f + rattle * 1500.0f) * slow / sr) crackle = rng.bi() * (0.5f + rng.uni());
        float rustOut = grind.bp (crackle) * 1.2f;
        const float cd = creakDrift.tick (rng);
        rustOut += creak.bp (rng.bi()) * std::max (0.0f, cd - 0.35f) * 2.5f;
        rustOut *= rust * slow;

        const float metal = out[0] + out[1] + out[2] * 1.3f;
        const float lw = 1.0f - pan, rw = pan;
        l = (metal * lw + rustOut * 0.8f) * intensity * 16.0f;
        r = (metal * rw + rustOut * 0.8f) * intensity * 16.0f;
    }

private:
    static constexpr int kBanks = 3, kModes = 5;
    struct Mode { float f = 1000, c = 0, r2 = 0, g = 0, y1 = 0, y2 = 0; };
    void hit (float a, int b)
    {
        amp = a; bank = b; pan = 0.1f + 0.8f * rng.uni();
        clickLen = clickLeft = 8 + (int) (rng.uni() * 40.0f);
    }
    float sr = 48000, rate = 10, intensity = 1, amp = 0, pan = 0.5f, lastRust = -1;
    int countdown = 1, follow = 0, followTimer = 0, bank = 0, clickLeft = 0, clickLen = 1;
    Rng rng; Mode banks[kBanks][kModes]; SVF clickHp, grind, creak; Drift groan, creakDrift;
};

//==============================================================================
// Sustained voice: Drone (detuned saws + sub + folder), Metal (bowed combs), Air (drifting whisper formants)
class Voice
{
public:
    bool active() const { return amp.active(); }
    int note = -1; uint64_t age = 0;

    void prepare (float sampleRate, uint32_t seed)
    {
        sr = sampleRate; rng = Rng (seed);
        combA.init ((int) (sr / 15.0f) + 8); combB.init ((int) (sr / 15.0f) + 8);
        for (auto& o : osc) o.phase = rng.uni();
        piano.prepare (sr); whisper.prepare (sr, seed ^ 0x51ED27u); scream.prepare (sr, seed ^ 0xBADC0DEu);
        chains.prepare (sr, seed ^ 0xC4A1u);
        symphony.prepare (sr, seed ^ 0x5F0Du);
    }

    void start (int n, float velocity, const Params& p)
    {
        note = n; vel = velocity; baseFreq = mtof ((float) n);
        if (! amp.active()) { filtL.reset(); filtR.reset(); combA.clear(); combB.clear(); }
        amp.set (p.attack, p.decay, p.sustain, p.release, sr);
        fenv.set (std::max (0.005f, p.attack * 0.6f), p.decay * 1.5f + 0.2f, 0.0f, p.release, sr);
        amp.on(); fenv.on();
        piano.start (n, baseFreq, velocity, p.pianoBreak);
        whisper.start (baseFreq);
        scream.start (baseFreq);
        chains.start (baseFreq, velocity);
        symphony.start (baseFreq, velocity);
    }
    void stop() { amp.off(); fenv.off(); }
    void kill() { amp.st = ADSR::Idle; amp.v = 0; }

    // Renders and ADDS into L/R. 'derived' values are computed once per block by the engine.
    void render (float* L, float* R, int n, const Params& p, float driftCents, float driftRate,
                 float cutoffMul, float subAmt, float airAmt)
    {
        for (int i = 0; i < 5; ++i) drift[i].setRate (driftRate * (0.6f + 0.2f * (float) i), sr);
        for (int i = 0; i < 2; ++i) formDrift[i].setRate (0.15f + p.airMotion * 2.5f * (1.0f + 0.37f * (float) i), sr);
        pitchDrift.setRate (driftRate * 0.3f, sr);

        static constexpr float spread[5] = { 0.0f, -1.0f, 1.0f, -0.45f, 0.55f };
        static constexpr float pans[5]   = { 0.5f, 0.15f, 0.85f, 0.35f, 0.65f };
        const float detuneC = p.droneDetune * 45.0f;
        const float foldAmt = 1.0f + p.droneFold * 5.0f;
        const float combFb  = clampf (0.86f + p.metalResonance * 0.135f, 0.0f, 0.997f);
        combDampA.setCutoff (800.0f + p.metalTone * 9000.0f, sr);
        combDampB.setCutoff (600.0f + p.metalTone * 7000.0f, sr);

        for (int s = 0; s < n; s += 16)
        {
            const int m = std::min (16, n - s);
            // control-rate updates
            const float fe   = fenv.v;
            const float kt   = std::pow (baseFreq / 261.6f, 0.5f);
            const float fc   = p.cutoff * cutoffMul * kt * std::pow (2.0f, p.envAmount * fe * 5.0f);
            const float q    = 0.6f + p.resonance * 9.0f;
            filtL.set (fc, q, sr); filtR.set (fc, q, sr);

            const float f1 = 350.0f + (formDrift[0].from * 0.5f + 0.5f) * 500.0f;
            const float f2 = 1000.0f + (formDrift[1].from * 0.5f + 0.5f) * 1500.0f;
            formA.set (f1, 7.0f, sr); formB.set (f2, 9.0f, sr);
            if (p.whisperLevel > 0.0001f) whisper.control (p.airMotion, p.whisperDemon);
            if (p.screamLevel > 0.0001f)  scream.control (p.screamRasp);
            if (p.chainLevel > 0.0001f)   chains.control (p.chainRattle, p.chainRust);
            if (p.symphonyLevel > 0.0001f) symphony.control (p.symphonyTwist, p.symphonySection, m);

            for (int j = 0; j < m; ++j)
            {
                const float a  = amp.tick(); fenv.tick();
                const float pd = pitchDrift.tick (rng) * driftCents * 0.5f;

                // --- Drone
                float dl = 0, dr = 0;
                for (int k = 0; k < 5; ++k)
                {
                    const float c  = spread[k] * detuneC + drift[k].tick (rng) * driftCents + pd;
                    const float f  = baseFreq * centsToRatio (c);
                    float v = osc[k].tick (f / sr);
                    v = std::sin (v * foldAmt * 0.5f * kPi);   // wavefolder
                    dl += v * (1.0f - pans[k]); dr += v * pans[k];
                }
                subPhase += baseFreq * 0.5f * centsToRatio (pd) / sr; if (subPhase >= 1.0f) subPhase -= 1.0f;
                const float sub = std::sin (kTwoPi * subPhase) * (p.droneSub + subAmt);
                dl = dl * 0.3f + sub; dr = dr * 0.3f + sub;

                // --- Metal (noise-bowed comb pair, inharmonic ratio)
                const float bowNoise = rng.bi() * (0.6f + 0.4f * std::abs (drift[0].from));
                const float exc = bowNoise * 0.03f;
                const float freqA = baseFreq * centsToRatio (pd);
                const float freqB = freqA * 2.756f;
                float ya = combA.read (sr / freqA);
                float yb = combB.read (sr / std::min (freqB, sr * 0.2f));
                ya = combDampA.lp (ya); yb = combDampB.lp (yb);
                combA.push (std::tanh (exc + ya * combFb));
                combB.push (std::tanh (exc + yb * combFb * 0.985f));
                const float ml = (ya * 1.6f + yb * 0.7f) * 3.0f, mr = (ya * 0.9f + yb * 1.4f) * 3.0f;

                // --- Air (whisper formants)
                formDrift[0].tick (rng); formDrift[1].tick (rng);
                const float nz = rng.bi();
                const float air = (formA.bp (nz) + formB.bp (nz) * 0.7f) * 0.9f;

                const float airL = (p.airLevel + airAmt);
                float xl = dl * p.droneLevel + ml * p.metalLevel + air * airL;
                float xr = dr * p.droneLevel + mr * p.metalLevel + air * airL;

                float el, er;
                if (p.pianoLevel > 0.0001f)   { piano.tick (rng, el, er);                                 xl += el * p.pianoLevel;   xr += er * p.pianoLevel; }
                if (p.whisperLevel > 0.0001f) { whisper.tick (p.airMotion, p.whisperDemon, el, er);       xl += el * p.whisperLevel; xr += er * p.whisperLevel; }
                if (p.screamLevel > 0.0001f)  { scream.tick (p.screamRasp, el, er);                       xl += el * p.screamLevel;  xr += er * p.screamLevel; }
                if (p.chainLevel > 0.0001f)   { chains.tick (p.chainRattle, p.chainRust, el, er);         xl += el * p.chainLevel;   xr += er * p.chainLevel; }
                if (p.symphonyLevel > 0.0001f) { symphony.tick (el, er);                                  xl += el * p.symphonyLevel; xr += er * p.symphonyLevel; }

                xl = filtL.lp (xl); xr = filtR.lp (xr);
                const float g = a * (0.35f + 0.65f * vel) * 0.5f;
                L[s + j] += xl * g; R[s + j] += xr * g;
            }
        }
    }

private:
    float sr = 48000, vel = 1, baseFreq = 110, subPhase = 0;
    Rng rng;
    std::array<Saw, 5> osc; std::array<Drift, 5> drift; Drift pitchDrift; std::array<Drift, 2> formDrift;
    DelayLine combA, combB; OnePole combDampA, combDampB;
    SVF filtL, filtR, formA, formB;
    ADSR amp, fenv;
    PianoLayer piano; WhisperLayer whisper; ScreamLayer scream; ChainLayer chains; SymphonyLayer symphony;
};

//==============================================================================
// One-shot sting / jumpscare. Types: 0 Slam, 1 Shriek Stabs, 2 Sub Boom, 3 Metal Crash
class Sting
{
public:
    bool active = false;
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void trigger (int note, float velocity, int stingType)
    {
        active = true; t = 0; vel = velocity; type = std::clamp (stingType, 0, 3);
        pitch = mtof ((float) note - 24.0f);          // C6 -> C4 region
        subPhase = 0;
        for (auto& o : saws) o.phase = rng.uni();
        for (auto& ph : partPhase) ph = rng.uni();
        for (int k = 0; k < kParts; ++k)
        {
            partRatio[k] = 1.0f + (float) k * (0.9f + rng.uni() * 1.3f) + rng.uni() * 0.4f;
            partDecay[k] = 0.25f + rng.uni() * (k < 4 ? 2.8f : 1.2f);
            partPan[k]   = rng.uni();
        }
        for (auto& f : filt) f.reset();
        hp.reset(); lp.reset(); rumble.reset();
        hp.set (300.0f, 0.7f, sr); rumble.set (90.0f, 0.8f, sr);
    }

    void render (float* L, float* R, int n, float level)
    {
        const float g = level * (0.45f + 0.55f * vel);
        for (int i = 0; i < n && active; ++i)
        {
            const float tt = t / sr;
            float l = 0, r = 0;
            switch (type)
            {
                case 0: renderSlam (tt, l, r); break;
                case 1: renderShriek (tt, l, r); break;
                case 2: renderBoom (tt, l, r); break;
                default: renderCrash (tt, l, r); break;
            }
            L[i] += std::tanh (l * 2.2f) * g * 1.05f;
            R[i] += std::tanh (r * 2.2f) * g * 1.05f;
            t += 1.0f;
            if (tt > 4.5f) active = false;
        }
    }

private:
    static constexpr int kParts = 10;

    float sine (float& ph, float f) { ph += f / sr; if (ph >= 1.0f) ph -= 1.0f; return std::sin (kTwoPi * ph); }

    // big orchestral-style hit: click + sub boom + dissonant brass/string cluster + metal ring
    void renderSlam (float tt, float& l, float& r)
    {
        const float click = hp.hp (rng.bi()) * std::exp (-tt * 70.0f) * 1.4f;
        const float boom = std::tanh (sine (subPhase, 32.0f + 120.0f * std::exp (-tt * 7.0f)) * 2.5f) * std::exp (-tt * 2.4f) * 1.1f;
        static constexpr float cl[5] = { 1.0f, 1.0595f, 1.4142f, 1.4983f, 2.1189f };
        const float sag = std::pow (2.0f, -0.8f * std::min (1.0f, tt) / 12.0f);
        float cs = 0, cr = 0;
        for (int k = 0; k < 5; ++k)
        {
            const float v = saws[(size_t) k].tick (pitch * cl[k] * sag / sr);
            if (k & 1) cr += v; else cs += v;
        }
        const float bright = 600.0f + 7000.0f * std::exp (-tt * 3.5f);
        if (((int) t & 15) == 0) { filt[0].set (bright, 1.2f, sr); filt[1].set (bright, 1.2f, sr); }
        const float clEnv = (1.0f - std::exp (-tt * 300.0f)) * std::exp (-tt * 1.8f) * 0.55f;
        cs = filt[0].lp (cs) * clEnv; cr = filt[1].lp (cr) * clEnv;
        float pl, pr; partials (tt, 0.35f, pl, pr);
        l = click + boom + cs + cr * 0.4f + pl;
        r = click + boom + cr + cs * 0.4f + pr;
    }

    // "shower scene" style: repeated, dissonant high violin screeches
    void renderShriek (float tt, float& l, float& r)
    {
        static constexpr float cl[4] = { 2.0f, 2.119f, 2.828f, 2.997f };
        const float stabLen = 0.14f;
        const int stab = (int) (tt / stabLen);
        const float ts = tt - (float) stab * stabLen;
        const float env = stab < 7 ? (1.0f - std::exp (-ts * 600.0f)) * std::exp (-ts * 10.0f) * (1.0f - (float) stab * 0.07f) : 0.0f;
        const float tail = std::exp (-std::max (0.0f, tt - 0.98f) * 3.0f) * (tt > 0.98f ? 0.25f : 0.0f);
        const float vib = 1.0f + 0.012f * std::sin (kTwoPi * 7.0f * tt) + 0.004f * rng.bi();
        float a = 0, b = 0;
        for (int k = 0; k < 4; ++k)
        {
            const float v = saws[(size_t) k].tick (pitch * cl[k] * vib / sr);
            if (k & 1) b += v; else a += v;
        }
        if (((int) t & 15) == 0) { filt[0].set (3200.0f, 1.8f, sr); filt[1].set (2600.0f, 1.8f, sr); }
        const float bow = hp.hp (rng.bi()) * 0.25f;
        const float e = env + tail;
        l = (filt[0].bp (a + bow) * 1.4f + a * 0.25f) * e;
        r = (filt[1].bp (b + bow) * 1.4f + b * 0.25f) * e;
    }

    // trailer-style sub drop with air sweep and rumble tail
    void renderBoom (float tt, float& l, float& r)
    {
        const float f = 28.0f + 140.0f * std::exp (-tt * 4.0f);
        const float boom = std::tanh (sine (subPhase, f) * 3.0f) * std::exp (-tt * 1.3f) * 1.3f;
        const float click = hp.hp (rng.bi()) * std::exp (-tt * 90.0f) * 1.2f;
        if (((int) t & 15) == 0) lp.set (150.0f + 7000.0f * std::exp (-tt * 5.0f), 0.9f, sr);
        const float air = lp.lp (rng.bi()) * std::exp (-tt * 1.6f) * 0.7f;
        const float rum = rumble.lp (rng.bi()) * std::exp (-tt * 0.8f) * 1.8f;
        l = boom + click + air + rum;
        r = boom + click + air * 0.9f + rum;
    }

    // big metallic crash: dense inharmonic partials + noise burst
    void renderCrash (float tt, float& l, float& r)
    {
        const float burst = hp.hp (rng.bi()) * (std::exp (-tt * 25.0f) * 1.2f + std::exp (-tt * 2.0f) * 0.25f);
        float pl, pr; partials (tt, 1.0f, pl, pr);
        const float thud = sine (subPhase, 55.0f + 60.0f * std::exp (-tt * 12.0f)) * std::exp (-tt * 6.0f) * 0.7f;
        l = burst + pl * 1.5f + thud;
        r = burst * 0.95f + pr * 1.5f + thud;
    }

    void partials (float tt, float amt, float& l, float& r)
    {
        l = r = 0;
        for (int k = 0; k < kParts; ++k)
        {
            const float f = pitch * 0.5f * partRatio[k];
            if (f > sr * 0.45f) continue;
            const float v = sine (partPhase[k], f) * std::exp (-tt / partDecay[k] * 2.0f) / (1.0f + (float) k * 0.35f) * amt * 0.5f;
            l += v * (1.0f - partPan[k]); r += v * partPan[k];
        }
    }

    float sr = 48000, t = 0, vel = 1, pitch = 220, subPhase = 0;
    int type = 0;
    std::array<Saw, 5> saws;
    std::array<float, kParts> partPhase {}, partRatio {}, partDecay {}, partPan {};
    Rng rng; SVF hp, lp, rumble; std::array<SVF, 2> filt;
};

//==============================================================================
// 8-line feedback delay network reverb
class FDNReverb
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& d : lines) d.init ((int) (sr * 0.25f));
        for (auto& lp : damp) lp.setCutoff (5500.0f, sr);
        for (auto& p : predelay) p.init ((int) (sr * 0.1f));
    }
    void reset() { for (auto& d : lines) d.clear(); for (auto& p : predelay) p.clear(); }
    void setSize (float size)
    {
        static constexpr float base[8] = { 1013, 1187, 1361, 1559, 1721, 1931, 2113, 2381 }; // samples @48k
        const float t60 = 1.5f + size * 12.0f;
        const float scale = (0.55f + size * 1.1f) * sr / 48000.0f;
        for (int i = 0; i < 8; ++i)
        {
            len[i]  = base[i] * scale;
            gain[i] = std::pow (10.0f, -3.0f * len[i] / (sr * t60));
        }
    }
    void process (float inL, float inR, float& outL, float& outR)
    {
        predelay[0].push (inL); predelay[1].push (inR);
        const float pl = predelay[0].read (sr * 0.025f), pr = predelay[1].read (sr * 0.025f);
        float x[8];
        for (int i = 0; i < 8; ++i) x[i] = damp[i].lp (lines[i].read (len[i])) * gain[i];
        // Hadamard 8
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h * 2)
                for (int j = i; j < i + h; ++j) { const float a = x[j], b = x[j + h]; x[j] = a + b; x[j + h] = a - b; }
        const float norm = 0.35355339f;
        outL = outR = 0;
        for (int i = 0; i < 8; ++i)
        {
            const float v = x[i] * norm;
            lines[i].push (v + ((i & 1) ? pr : pl) * 0.5f);
            if (i & 1) outR += v; else outL += v;
        }
        outL *= 0.5f; outR *= 0.5f;
    }
private:
    float sr = 48000;
    std::array<DelayLine, 8> lines; std::array<OnePole, 8> damp; std::array<DelayLine, 2> predelay;
    std::array<float, 8> len {}, gain {};
};

//==============================================================================
class Engine
{
public:
    static constexpr int kVoices = 8, kStings = 4, kStingNote = 84; // C6 and up = stings

    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        for (int i = 0; i < kVoices; ++i) voices[(size_t) i].prepare (sr, 1234u + (uint32_t) i * 7919u);
        for (int i = 0; i < kStings; ++i) stings[(size_t) i].prepare (sr, 777u + (uint32_t) i * 104729u);
        tmpL.assign ((size_t) maxBlock, 0.0f); tmpR.assign ((size_t) maxBlock, 0.0f);
        reverb.prepare (sr);
        delayL.init ((int) (sr * 4.0f)); delayR.init ((int) (sr * 4.0f));
        wobL.init ((int) (sr * 0.05f)); wobR.init ((int) (sr * 0.05f));
        delayDamp[0].setCutoff (3500.0f, sr); delayDamp[1].setCutoff (3500.0f, sr);
        bodyL.setCutoff (120.0f, sr); bodyR.setCutoff (120.0f, sr);
        distL.prepare (sr, 91u); distR.prepare (sr, 913u);
        gateSmooth.setCutoff (60.0f, sr);
        dcL.setCutoff (12.0f, sr); dcR.setCutoff (12.0f, sr);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v.kill();
        for (auto& s : stings) s.active = false;
        reverb.reset(); delayL.clear(); delayR.clear(); wobL.clear(); wobR.clear();
        freeBeat = 0;
    }

    void noteOn (int note, float velocity)
    {
        if (params.stingZone && note >= kStingNote)
        {
            auto& s = stings[(size_t) (nextSting++ % kStings)];
            s.trigger (note, velocity, params.stingType);
            return;
        }
        Voice* target = nullptr;
        for (auto& v : voices) if (v.active() && v.note == note) { target = &v; break; }
        if (! target) for (auto& v : voices) if (! v.active()) { target = &v; break; }
        if (! target)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age < target->age) target = &v;
        }
        target->age = ++ageCounter;
        target->start (note, velocity, params);
    }

    void noteOff (int note)
    {
        for (auto& v : voices) if (v.active() && v.note == note) v.stop();
    }

    void allNotesOff() { for (auto& v : voices) v.stop(); }

    // ppq < 0 => free-run gate from internal clock
    void process (float* L, float* R, int n, double bpm, double ppq)
    {
        const Params& p = params;
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);

        // --- Dread macro: one knob pushes many things toward "wrong"
        const float d          = p.dread;
        const float driftCents = 3.0f + d * 45.0f;
        const float driftRate  = 0.12f + d * 1.6f;
        const float cutoffMul  = 1.0f - d * 0.45f;
        const float subAmt     = d * 0.25f;
        const float airAmt     = d * 0.3f;
        const float ringMix    = clampf (p.ringMix + d * 0.3f, 0.0f, 1.0f);
        const float wobble     = clampf (p.wobble + d * 0.35f, 0.0f, 1.0f);
        reverb.setSize (clampf (p.reverbSize + d * 0.2f, 0.0f, 1.0f));

        for (auto& v : voices) if (v.active()) v.render (L, R, n, p, driftCents, driftRate, cutoffMul, subAmt, airAmt);
        // stings render to their own bus: they skip ring/torment/distortion/gate so the hit stays huge
        if ((int) tmpL.size() < n) { tmpL.resize ((size_t) n); tmpR.resize ((size_t) n); }
        std::fill (tmpL.begin(), tmpL.begin() + n, 0.0f); std::fill (tmpR.begin(), tmpR.begin() + n, 0.0f);
        for (auto& s : stings) if (s.active) s.render (tmpL.data(), tmpR.data(), n, p.stingLevel);

        // timing
        bpm = (bpm > 20.0 && bpm < 400.0) ? bpm : 120.0;
        const double beatsPerSample = bpm / 60.0 / sr;
        static constexpr double gateLen[4]  = { 0.5, 0.25, 0.125, 1.0 / 6.0 };   // 1/8, 1/16, 1/32, 1/16T
        static constexpr double delayLen[4] = { 1.0, 0.75, 0.5, 0.25 };          // 1/4, 1/8., 1/8, 1/16
        const double gl = gateLen[std::clamp (p.gateRate, 0, 3)];
        const float  dSamples = (float) (delayLen[std::clamp (p.delayTime, 0, 3)] * 60.0 / bpm * sr);
        double beat = (ppq >= 0.0) ? ppq : freeBeat;

        const float tormentPre = 1.0f + p.torment * 22.0f;
        const float tormentMk  = 1.0f / std::pow (tormentPre, 0.62f);
        const float ringInc    = p.ringFreq / sr;
        const float outGain    = dbToGain (p.outputDb);
        distL.setTone (p.distTone); distR.setTone (p.distTone);
        wobDrift[0].setRate (0.6f + wobble * 2.0f, sr); wobDrift[1].setRate (0.7f + wobble * 2.3f, sr);

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            // Ring mod
            ringPhase += ringInc; if (ringPhase >= 1.0f) ringPhase -= 1.0f;
            const float car = std::sin (kTwoPi * ringPhase);
            l = l * (1.0f - ringMix) + l * car * ringMix * 1.4f;
            r = r * (1.0f - ringMix) + r * car * ringMix * 1.4f;

            // Torment: body boost + saturation
            if (p.torment > 0.001f)
            {
                l += bodyL.lp (l) * p.torment * 1.5f; r += bodyR.lp (r) * p.torment * 1.5f;
                l = std::tanh (l * tormentPre) * tormentMk; r = std::tanh (r * tormentPre) * tormentMk;
            }

            // Distortion
            if (p.distDrive > 0.001f)
            {
                l = distL.process (l, p.distDrive, p.distType, p.distMix);
                r = distR.process (r, p.distDrive, p.distType, p.distMix);
            }

            // Step gate
            const int step = (int) std::floor (beat / gl) & 15;
            const float gTarget = p.gateSteps[(size_t) step] ? 1.0f : 1.0f - p.gateDepth;
            const float gg = gateSmooth.lp (gTarget);
            l *= gg; r *= gg;
            beat += beatsPerSample;

            // Wobble (tape warble / seasick pitch)
            wobL.push (l); wobR.push (r);
            if (wobble > 0.001f)
            {
                const float dl = sr * (0.009f + wobDrift[0].tick (rng) * 0.0045f * wobble);
                const float dr = sr * (0.009f + wobDrift[1].tick (rng) * 0.0045f * wobble);
                const float w  = std::min (1.0f, wobble * 2.5f);
                l = l * (1.0f - w) + wobL.read (dl) * w;
                r = r * (1.0f - w) + wobR.read (dr) * w;
            }

            // stings join here (post gate/wobble) so they still get delay + reverb
            l += tmpL[(size_t) i]; r += tmpR[(size_t) i];

            // Ping-pong delay
            const float tl = delayL.read (dSamples), tr = delayR.read (dSamples);
            delayL.push (std::tanh (delayDamp[0].lp ((l + r) * 0.5f + tr * p.delayFeedback)));
            delayR.push (std::tanh (delayDamp[1].lp (tl * p.delayFeedback)));
            l += tl * p.delayMix; r += tr * p.delayMix;

            // Reverb
            float vl, vr; reverb.process (l, r, vl, vr);
            l = l * (1.0f - p.reverbMix * 0.5f) + vl * p.reverbMix * 1.2f;
            r = r * (1.0f - p.reverbMix * 0.5f) + vr * p.reverbMix * 1.2f;

            // DC block + output + safety clip
            l -= dcL.lp (l); r -= dcR.lp (r);
            l *= outGain; r *= outGain;
            L[i] = softLimit (l); R[i] = softLimit (r);
        }
        freeBeat = beat;
    }

    Params params;

private:
    float sr = 48000;
    std::array<Voice, kVoices> voices; std::array<Sting, kStings> stings;
    uint64_t ageCounter = 0; int nextSting = 0;
    std::vector<float> tmpL, tmpR;
    FDNReverb reverb;
    DelayLine delayL, delayR, wobL, wobR;
    OnePole delayDamp[2], bodyL, bodyR, gateSmooth, dcL, dcR;
    Distortion distL, distR;
    std::array<Drift, 2> wobDrift; Rng rng { 4242u };
    float ringPhase = 0; double freeBeat = 0;
};

} // namespace dread
