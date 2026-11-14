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
    void reset() { from = to = 0; phase = 0; }
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
    // Distortion
    float distDrive = 0.0f, distTone = 0.7f, distMix = 1.0f; int distType = 0;
    // Chains + sting type
    float chainLevel = 0.0f, chainRattle = 0.4f, chainRust = 0.5f;
    int stingType = 0;
    // Twisted Strings (param ids kept from the old Twisted Symphony so saved sounds still load)
    float symphonyLevel = 0.0f, symphonyTwist = 0.5f;
    // Hell's Bells
    float bellLevel = 0.0f, bellSinister = 0.3f;
    // Demented Pipe Organ
    float organLevel = 0.0f, organDement = 0.5f, organStops = 0.5f;
    // Heartbeat
    float heartLevel = 0.0f, heartRate = 70.0f, heartPanic = 0.2f;
    // Ghost Piano (the original additive piano)
    float ghostLevel = 0.0f, ghostBreak = 0.5f;
};

// Per-key seed: the same key always makes the same "random" choices, so playback is repeatable.
inline uint32_t keySeed (int note, uint32_t salt) { return ((uint32_t) (note + 1) * 2654435761u) ^ (salt * 0x9E3779B9u) ^ 0x5bd1e995u; }

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
                // bites early: bit depth falls fast at first, sample-rate reduction grows steadily
                const int factor = 1 + (int) (std::pow (drive, 1.3f) * 40.0f);
                if (++holdCount >= factor) { holdCount = 0; hold = x; }
                const float bits = 16.0f - std::pow (drive, 0.55f) * 13.0f;
                // quantise relative to the signal's own level, so quiet sources crush hard but never vanish
                env = std::max (std::abs (x), env * 0.9997f) + 1e-5f;
                const float steps = std::pow (2.0f, bits - 1.0f) * 0.5f;
                y = std::round (hold / env * steps) / steps * env;
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
    float sr = 48000, hold = 0, env = 0.1f; int holdCount = 0, crackle = 0;
    Rng rng; OnePole toneLp;
};

//==============================================================================
// BROKEN PIANO — physical-model piano: a felt hammer strikes 2-3 waveguide strings per key
// (loss filter + stiffness allpass), unison strings beat against each other, a soundboard colours the
// tone, and each key has a key-thump. BREAK (fixed per key): keys pulled out of tune, unison strings
// spread apart (honky-tonk/beating), missing strings, dead/clunking keys, duller rusty strings,
// more string stiffness (clang) and buzzing/rattling strings.
class PianoLayer
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (auto& st : strings) st.line.init ((int) (sr / 20.0f) + 16);
    }

    void start (int note, float freq, float vel, float brk)
    {
        Rng key (keySeed (note, 11u));
        const float keyDetune = key.bi() * brk * 42.0f;
        dead = key.uni() < brk * 0.18f;
        rattle = brk * brk * (0.15f + 0.45f * key.uni());
        const float spreadC = 0.5f + brk * (6.0f + 12.0f * key.uni());
        const float f0 = freq * centsToRatio (keyDetune);
        float T = 7.5f * std::pow (261.6f / f0, 0.55f);
        T = clampf (T, 0.35f, 22.0f) * (1.0f - brk * 0.4f) * (dead ? 0.05f : 1.0f);
        static constexpr float offs[3] = { 0.0f, 1.0f, -1.0f };
        numStrings = note < 34 ? 2 : 3;
        for (int k = 0; k < numStrings; ++k)
        {
            auto& st = strings[(size_t) k];
            st.line.clear(); st.lpState = 0; st.apX = st.apY = 0;
            st.mute = (k > 0 && key.uni() < brk * 0.35f);
            const float f = f0 * centsToRatio (offs[k] * spreadC + key.bi() * brk * 3.0f);
            st.damp = clampf (0.06f + brk * 0.28f + (dead ? 0.35f : 0.0f) + 0.18f * std::max (0.0f, 1.0f - f / 700.0f), 0.0f, 0.88f);
            st.ap = -clampf (0.12f + brk * 0.45f, 0.0f, 0.8f) * clampf (f / 1200.0f, 0.2f, 1.0f);
            const float lpDelay = st.damp / (1.0f - st.damp);
            const float apDelay = (1.0f - st.ap) / (1.0f + st.ap);
            st.len = std::max (2.0f, sr / f - lpDelay - apDelay);
            st.g = std::pow (10.0f, -3.0f / (T * f));
            st.pan = 0.5f + offs[k] * 0.12f;
        }
        // hammer: short felt pulse, harder = shorter and brighter
        hammerLen = std::max (8.0f, sr * (0.0042f - 0.0026f * vel));
        hammerT = 0; hammerAmp = 0.55f * (0.25f + 0.75f * vel);
        hammerLp.reset(); hammerLp.set (450.0f + 7000.0f * vel * vel * (1.0f - brk * 0.3f), 0.7f, sr);
        thumpT = 0; thumpAmp = (dead ? 0.6f : 0.12f) * (0.4f + 0.6f * vel); thumpPh = 0;
        clickHp.reset(); clickHp.set (2500.0f, 0.7f, sr);
        board[0].reset(); board[1].reset(); board[2].reset();
        board[0].set (95.0f, 1.3f, sr); board[1].set (240.0f, 1.1f, sr); board[2].set (900.0f, 0.8f, sr);
        keyPan = 0.22f + 0.56f * clampf ((float) (note - 21) / 87.0f, 0.0f, 1.0f);
    }

    void tick (Rng& rng, float& l, float& rt)
    {
        // excitation
        float exc = 0;
        if (hammerT < hammerLen)
        {
            const float w = std::sin (kPi * hammerT / hammerLen);
            exc = (hammerLp.lp (rng.bi()) * 0.6f + w) * w * hammerAmp;
            hammerT += 1.0f;
        }
        float out = 0, sl = 0, sr_ = 0;
        for (int k = 0; k < numStrings; ++k)
        {
            auto& st = strings[(size_t) k];
            const float x = st.line.read (st.len);
            st.lpState = (1.0f - st.damp) * x + st.damp * st.lpState;
            const float y = st.ap * st.lpState + st.apX - st.ap * st.apY;
            st.apX = st.lpState; st.apY = y;
            st.line.push (y * st.g + exc / (float) numStrings);
            if (! st.mute) { out += y; sl += y * (1.0f - st.pan); sr_ += y * st.pan; }
        }
        // soundboard colouring
        const float b = board[0].bp (out) * 0.35f + board[1].bp (out) * 0.3f + board[2].bp (out) * 0.15f;
        // buzzing / rattling string
        const float nz = rng.bi();
        float sig = out * 0.8f + b;
        sig += sig * nz * rattle + std::max (0.0f, std::abs (out) - 0.08f) * nz * rattle * 2.0f;
        // key thump (louder on dead keys)
        if (thumpT < 0.06f * sr)
        {
            const float tt = thumpT / sr;
            thumpPh += (85.0f + 60.0f * std::exp (-tt * 60.0f)) / sr;
            sig += (std::sin (kTwoPi * thumpPh) * std::exp (-tt * 45.0f) + clickHp.hp (rng.bi()) * std::exp (-tt * 400.0f) * 0.5f) * thumpAmp;
            thumpT += 1.0f;
        }
        const float spreadL = (sl - sr_) * 0.15f;
        l  = (sig * (1.0f - keyPan) + spreadL) * 4.4f;
        rt = (sig * keyPan - spreadL) * 6.0f;
    }

private:
    struct Str { DelayLine line; float len = 100, g = 0.99f, damp = 0.2f, ap = 0, lpState = 0, apX = 0, apY = 0, pan = 0.5f; bool mute = false; };
    float sr = 48000, rattle = 0, hammerLen = 100, hammerT = 1e9f, hammerAmp = 0, thumpT = 1e9f, thumpAmp = 0, thumpPh = 0, keyPan = 0.5f;
    bool dead = false; int numStrings = 3;
    std::array<Str, 3> strings;
    SVF hammerLp, clickHp; std::array<SVF, 3> board;
};

//==============================================================================
// GHOST PIANO (the original DREAD piano) — additive/modal piano: stretched (inharmonic) partials on detuned string pairs,
// hammer thump, per-key fixed detune (same key is always wrong the same way), dead keys, string rattle.
class GhostPianoLayer
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    void start (int note, float freq, float vel, float brk)
    {
        Rng key (keySeed (note, 21u));                           // per-key, deterministic
        const float keyDetune = key.bi() * brk * 45.0f;
        dead = key.uni() < brk * 0.22f;
        rattle = brk * (0.15f + 0.35f * key.uni());
        const float f0 = freq * centsToRatio (keyDetune);
        const float B = 0.0003f * (1.0f + brk * 10.0f);
        const float pairCents = 0.6f + brk * 14.0f;
        const float baseT = 4.0f * std::pow (261.6f / f0, 0.45f) * (dead ? 0.12f : 1.0f);
        Rng r (keySeed (note, 22u));                             // same key -> same partial pattern

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
        l = sl * 0.75f; rt = sr_ * 0.75f;
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

    void start (float freq, uint32_t seed)
    {
        rng = Rng (seed); growl.phase = 0; odd = false; jit = 0;
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

//==============================================================================
// TWISTED STRINGS — a bowed string section per note. Eight players, each with their own tuning, bow
// timing (they don't all start together), Helmholtz-motion (saw-like) string tone, stick-slip bow noise,
// and vibrato that only blooms after the note has settled, as real players do. A violin/viola/cello body
// resonance bank (chosen by register) shapes the tone; the bow attack starts dark and opens up.
// SINISTER (0 = clean, warm section): players drift and slide apart, vibrato turns wide and slow,
// sul ponticello (glassy, nasal, scratchy bowing near the bridge), sour cluster notes creep in,
// nervous bow tremolo, and the whole section sags flat the longer a chord is held.
class StringsLayer
{
public:
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void start (float freq, float vel, int note, uint32_t seed)
    {
        rng = Rng (seed);
        base = freq; t = 0; velocity = vel;
        bodyScale = note < 43 ? 0.5f : (note < 55 ? 0.72f : 1.0f);          // cello / viola / violin body
        for (int i = 0; i < kPlayers; ++i)
        {
            auto& pl = players[(size_t) i];
            pl.osc.phase = rng.uni();
            pl.detune = rng.bi();
            pl.vibRate = 4.8f + rng.uni() * 1.4f; pl.vibPh = rng.uni(); pl.vibDepth = 8.0f + rng.uni() * 7.0f;
            pl.vibOnset = 0.18f + rng.uni() * 0.4f;
            pl.delay = rng.uni() * 0.03f; pl.attack = 0.07f + rng.uni() * 0.12f;
            pl.pan = 0.12f + 0.76f * ((float) (i % kMain) / (float) (kMain - 1));
            pl.drift.reset(); pl.drift.setRate (0.08f + rng.uni() * 0.25f, sr);
            pl.inc = freq / sr; pl.amp = 0;
        }
        tremPh = rng.uni();
        for (auto& f : bodyL) f.reset();
        for (auto& f : bodyR) f.reset();
        bowL.reset(); bowR.reset(); pontL.reset(); pontR.reset(); noiseBp.reset();
    }

    void control (float sinister, int blockLen)
    {
        const float dt = (float) blockLen / sr;
        const float s = sinister;
        const float sag = -s * 1.8f * std::min (1.0f, t / 7.0f);              // semitones
        for (int i = 0; i < kPlayers; ++i)
        {
            auto& pl = players[(size_t) i];
            const float rate = pl.vibRate * (1.0f - 0.45f * s);
            pl.vibPh += rate * dt; if (pl.vibPh >= 1.0f) pl.vibPh -= 1.0f;
            const float d = pl.drift.tick (rng); for (int k = 1; k < blockLen; ++k) pl.drift.tick (rng);
            const float bloom = smoothstep01 ((t - pl.vibOnset) / 0.45f);
            const float cents = pl.detune * (3.0f + s * 38.0f)
                              + d * s * 110.0f
                              + std::sin (kTwoPi * pl.vibPh) * pl.vibDepth * (1.0f + s * 2.2f) * bloom
                              + sag * 100.0f * (0.6f + 0.4f * std::abs (pl.detune));
            float mul = 1.0f;
            if (i == kMain)     mul = 1.0595f;                                  // minor 2nd
            if (i == kMain + 1) mul = 1.4142f;                                  // tritone
            pl.inc = std::min (0.45f, base * mul * centsToRatio (cents) / sr);
            const float on = smoothstep01 ((t - pl.delay) / pl.attack);
            pl.amp = i >= kMain ? on * s * s * 0.5f : on;
        }
        // bow attack: dark at first, opening as the bow grips; harder playing = brighter
        const float grip = 1.0f - std::exp (-t / 0.07f);
        const float bright = (1400.0f + 6500.0f * grip * (0.55f + 0.45f * velocity)) * (0.85f + 0.3f * s);
        bowL.set (std::min (bright, sr * 0.45f), 0.7f, sr); bowR.set (std::min (bright, sr * 0.45f), 0.7f, sr);
        static constexpr float bf[5] = { 275.0f, 460.0f, 1100.0f, 2700.0f, 3600.0f };
        static constexpr float bq[5] = { 3.0f, 4.0f, 2.0f, 1.6f, 2.5f };
        for (int k = 0; k < 5; ++k)
        {
            const float f = std::min (bf[k] * (k < 3 ? bodyScale : (0.8f + 0.2f * bodyScale)), sr * 0.45f);
            bodyL[(size_t) k].set (f, bq[k], sr); bodyR[(size_t) k].set (f * 1.03f, bq[k], sr);
        }
        pontL.set (2800.0f, 0.7f, sr); pontR.set (2800.0f, 0.7f, sr);
        noiseBp.set (std::min (base * 6.0f, 7000.0f), 1.2f, sr);
        pont = s * 0.9f; scratch = 0.02f + s * 0.07f;
        tremPh += 11.0f * dt; if (tremPh >= 1.0f) tremPh -= 1.0f;
        trem = 1.0f - std::pow (s, 1.5f) * 0.55f * (0.5f + 0.5f * std::sin (kTwoPi * tremPh));
        t += dt;
    }

    void tick (float& l, float& r)
    {
        float sl = 0, sr_ = 0, stick = 0;
        for (int i = 0; i < kPlayers; ++i)
        {
            auto& pl = players[(size_t) i];
            if (pl.amp < 0.0005f) { pl.osc.tick (pl.inc); continue; }
            const float v = pl.osc.tick (pl.inc) * pl.amp;
            if (pl.osc.phase < 0.12f) stick += pl.amp;                          // stick-slip release moment
            sl += v * (1.0f - pl.pan); sr_ += v * pl.pan;
        }
        const float bowNoise = noiseBp.bp (rng.bi()) * stick * scratch;
        sl = bowL.lp (sl + bowNoise); sr_ = bowR.lp (sr_ + bowNoise);
        auto body = [] (std::array<SVF, 5>& b, float x)
        { return x * 0.35f + b[0].bp (x) * 0.55f + b[1].bp (x) * 0.5f + b[2].bp (x) * 0.3f + b[3].bp (x) * 0.45f + b[4].bp (x) * 0.25f; };
        float yl = body (bodyL, sl), yr = body (bodyR, sr_);
        yl += pontL.hp (yl) * pont * 1.6f; yr += pontR.hp (yr) * pont * 1.6f;
        l = yl * trem * 0.21f; r = yr * trem * 0.21f;
    }

private:
    static float smoothstep01 (float x) { x = clampf (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
    static constexpr int kMain = 8, kPlayers = kMain + 2;
    struct Player { Saw osc; Drift drift; float inc = 0.01f, detune = 0, vibRate = 5, vibPh = 0, vibDepth = 10, vibOnset = 0.3f, delay = 0, attack = 0.1f, pan = 0.5f, amp = 0; };
    float sr = 48000, base = 220, t = 0, velocity = 1, bodyScale = 1, pont = 0, scratch = 0.02f, tremPh = 0, trem = 1;
    Rng rng; std::array<Player, kPlayers> players;
    std::array<SVF, 5> bodyL, bodyR; SVF bowL, bowR, pontL, pontR, noiseBp;
};

//==============================================================================
// HELL'S BELLS — modal church bell. Each partial follows a real bell's tuning (hum, prime, tierce, quint,
// nominal, deciem, undeciem, octave nominal and upper partials), split into a pair of slightly different
// modes that beat against each other (the bell's warble), struck by a clapper (harder = brighter), with the
// low partials ringing far longer than the high ones. Lower keys = bigger bells with longer tails.
// SINISTER (0 = a clean, well-tuned bell): the partials bend toward a cursed tuning (grinding minor-2nd
// tierce, a tritone where the fifth should be, flat octaves), faster ominous beating, a doom sub-octave,
// a darker, heavier strike, longer ring, and the bell's pitch slowly sinks as it rings.
class BellsLayer
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    void start (float freq, float vel, uint32_t seed, float sinister)
    {
        rng = Rng (seed);
        base = freq; t = 0; s = sinister;
        static constexpr float clean[kPartials]    = { 0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f, 6.796f, 0.25f };
        static constexpr float cursed[kPartials]   = { 0.47f, 1.0f, 1.062f, 1.414f, 1.94f, 2.38f, 2.79f, 2.88f, 4.42f, 5.2f, 7.1f, 0.25f };
        static constexpr float amp[kPartials]      = { 0.35f, 0.5f, 0.45f, 0.25f, 1.0f, 0.45f, 0.35f, 0.3f, 0.2f, 0.12f, 0.08f, 0.0f };
        static constexpr float life[kPartials]     = { 1.0f, 0.65f, 0.5f, 0.35f, 0.45f, 0.25f, 0.22f, 0.18f, 0.12f, 0.09f, 0.06f, 1.2f };
        const float humT = clampf (18.0f * std::pow (261.6f / freq, 0.8f), 2.0f, 45.0f) * (1.0f + s * 0.5f);
        for (int k = 0; k < kPartials; ++k)
        {
            const float jitter = rng.bi() * (0.003f + s * 0.02f);
            ratio[k] = (clean[k] + (cursed[k] - clean[k]) * s) * (1.0f + jitter);
            float a = amp[k];
            if (k == kPartials - 1) a = 0.45f * s;                              // doom sub-octave
            if (k >= 5 && k < kPartials - 1) a *= (1.0f - 0.5f * s) * std::pow (vel, 0.4f + 0.15f * (float) k);   // harder strike = brighter
            if (k == 0) a *= 1.0f + s * 0.6f;
            const float T = humT * life[k];
            const float beatHz = (0.25f + rng.uni() * 1.1f) * (1.0f + s * 3.5f);
            for (int d = 0; d < 2; ++d)
            {
                auto& m = modes[(size_t) (k * 2 + d)];
                m.hzOffset = d == 0 ? 0.0f : beatHz;
                m.amp = a * (d == 0 ? 0.62f : 0.38f);
                m.decay = std::exp (-6.9f / (T * sr));
                m.re = 0; m.im = 0;                                              // struck below
                m.pan = 0.5f + (d == 0 ? -0.18f : 0.18f) * (k & 1 ? 1.0f : -1.0f);
                const float ph = rng.uni() * kTwoPi;
                m.re = std::cos (ph) * m.amp; m.im = std::sin (ph) * m.amp;
            }
        }
        strikeT = 0; strikeAmp = (0.25f + 0.75f * vel) * (1.0f - 0.4f * s);
        strikeBp.reset(); strikeBp.set (std::min (freq * 4.0f, 9000.0f), 1.6f, sr);
        thunkPh = 0;
        updateFreqs();
    }

    void control (int blockLen)
    {
        t += (float) blockLen / sr;
        updateFreqs();
    }

    void tick (float& l, float& r)
    {
        float sl = 0, sr_ = 0;
        for (auto& m : modes)
        {
            const float re = m.re * m.c - m.im * m.sn, im = m.re * m.sn + m.im * m.c;
            m.re = re * m.decay; m.im = im * m.decay;
            sl += m.im * (1.0f - m.pan); sr_ += m.im * m.pan;
        }
        // clapper strike: a short metallic click and a low thunk
        if (strikeT < 0.08f * sr)
        {
            const float tt = strikeT / sr;
            thunkPh += (90.0f + 60.0f * std::exp (-tt * 50.0f)) / sr;
            const float hit = strikeBp.bp (rng.bi()) * std::exp (-tt * 160.0f) * 1.4f + std::sin (kTwoPi * thunkPh) * std::exp (-tt * 40.0f) * 0.4f;
            sl += hit * strikeAmp; sr_ += hit * strikeAmp; strikeT += 1.0f;
        }
        l = sl * 1.15f; r = sr_ * 1.15f;
    }

private:
    static constexpr int kPartials = 12;
    void updateFreqs()
    {
        const float sink = std::pow (2.0f, -s * 1.2f * std::min (1.0f, t / 6.0f) / 12.0f);   // pitch slowly sinks when sinister
        for (int k = 0; k < kPartials; ++k)
            for (int d = 0; d < 2; ++d)
            {
                auto& m = modes[(size_t) (k * 2 + d)];
                const float f = std::min (base * ratio[k] * sink + m.hzOffset, sr * 0.45f);
                const float w = kTwoPi * f / sr;
                m.c = std::cos (w); m.sn = std::sin (w);
            }
    }
    struct Mode { float re = 0, im = 0, c = 1, sn = 0, decay = 0.9999f, amp = 0, hzOffset = 0, pan = 0.5f; };
    float sr = 48000, base = 220, t = 0, s = 0, strikeT = 1e9f, strikeAmp = 0, thunkPh = 0;
    float ratio[kPartials] {};
    std::array<Mode, kPartials * 2> modes;
    Rng rng; SVF strikeBp;
};

//==============================================================================
// RUSTY CHAINS — a stream of link-on-link impacts (with little cascades, like a chain settling) into
// three banks of inharmonic metal resonators. RUST makes the links duller and shorter and adds gritty
// micro-scrapes inside each clink (no background noise). The pattern is seeded per key.
class ChainLayer
{
public:
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }

    void start (float freq, float vel, uint32_t seed)
    {
        rng = Rng (seed);
        const float size = std::pow (freq / 261.6f, 0.35f);   // lower notes = heavier chain
        static constexpr float ratios[kModes] = { 1.0f, 1.47f, 2.09f, 2.56f, 3.21f };
        static constexpr float bankBase[kBanks] = { 1650.0f, 2450.0f, 420.0f };
        for (int b = 0; b < kBanks; ++b)
            for (int m = 0; m < kModes; ++m)
            {
                auto& md = banks[b][m];
                md.f = std::min (bankBase[b] * size * ratios[m] * (0.93f + 0.14f * rng.uni()), sr * 0.45f);
                md.y1 = md.y2 = 0;
            }
        intensity = 0.5f + 0.5f * vel;
        countdown = 1 + (int) (rng.uni() * 0.01f * sr); follow = 0; grit = 0; lastRust = -1.0f; clickLeft = 0;
        clickHp.reset();
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
        clickHp.set (1500.0f - rust * 700.0f, 0.7f, sr);
    }

    void tick (float rattle, float rust, float& l, float& r)
    {
        if (--countdown <= 0)
        {
            hit (0.3f + 0.7f * rng.uni(), rng.uni() < 0.18f ? 2 : (rng.uni() < 0.5f ? 0 : 1));
            follow = (int) (rng.uni() * (1.0f + rattle * 4.0f));
            followTimer = (int) ((0.004f + rng.uni() * 0.025f) * sr);
            grit = (int) (rust * (1.0f + rng.uni() * 6.0f));
            gritTimer = (int) ((0.0012f + rng.uni() * 0.003f) * sr);
            countdown = (int) (-std::log (std::max (1e-4f, rng.uni())) / rate * sr) + 1;
        }
        if (follow > 0 && --followTimer <= 0)
        {
            hit (amp * (0.35f + 0.4f * rng.uni()), rng.uni() < 0.7f ? bank : (int) (rng.uni() * 2.99f));
            --follow; followTimer = (int) ((0.004f + rng.uni() * 0.03f) * sr);
            grit = (int) (rust * (1.0f + rng.uni() * 4.0f));
        }
        if (grit > 0 && --gritTimer <= 0)          // rusty micro-scrapes inside the clink
        {
            const float a = amp * (0.12f + 0.2f * rng.uni());
            hit (a, bank, true);
            --grit; gritTimer = (int) ((0.001f + rng.uni() * 0.004f) * sr);
        }

        float x = 0;
        if (clickLeft > 0) { x = clickHp.hp (rng.bi()) * clickAmp * ((float) clickLeft / (float) clickLen); --clickLeft; }

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
        const float metal = out[0] + out[1] + out[2] * 1.3f;
        l = metal * (1.0f - pan) * intensity * 16.0f;
        r = metal * pan * intensity * 16.0f;
    }

private:
    static constexpr int kBanks = 3, kModes = 5;
    struct Mode { float f = 1000, c = 0, r2 = 0, g = 0, y1 = 0, y2 = 0; };
    void hit (float a, int b, bool scrape = false)
    {
        clickAmp = a; bank = b;
        if (! scrape) { amp = a; pan = 0.1f + 0.8f * rng.uni(); }
        clickLen = clickLeft = (scrape ? 4 : 8) + (int) (rng.uni() * (scrape ? 12.0f : 40.0f));
    }
    float sr = 48000, rate = 10, intensity = 1, amp = 0, clickAmp = 0, pan = 0.5f, lastRust = -1;
    int countdown = 1, follow = 0, followTimer = 0, grit = 0, gritTimer = 0, bank = 0, clickLeft = 0, clickLen = 1;
    Rng rng; Mode banks[kBanks][kModes]; SVF clickHp;
};

//==============================================================================
// DEMENTED PIPE ORGAN — additive pipe ranks (16', 8' flute & principal, 4', 2 2/3', 2', mixture, reeds).
// STOPS pulls more ranks, from a soft flute up to full organ with reeds. DEMENT: every rank tuned
// differently (fixed per key), a seasick tremulant, wind starvation wobble, the pitch sagging as the
// note is held, and a "ciphering" wrong pipe a semitone up. Each note starts with pipe chiff.
class OrganLayer
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    void start (float freq, float vel, uint32_t seed)
    {
        rng = Rng (seed); base = freq; t = 0; velocity = vel;
        for (int i = 0; i < kRanks; ++i) { ranks[(size_t) i].ph = rng.uni(); ranks[(size_t) i].detune = rng.bi(); ranks[(size_t) i].saw.phase = rng.uni(); }
        cipherPh = rng.uni(); cipherUp = rng.uni() < 0.6f;
        tremPh = rng.uni(); tremRate = 5.2f + rng.uni() * 1.5f;
        starve.reset(); starve.setRate (0.35f + rng.uni() * 0.3f, sr);
        chiffT = 0; chiff.reset(); windHp.reset();
        chiff.set (std::min (base * 4.0f, 8000.0f), 2.0f, sr);
        windHp.set (2500.0f, 0.7f, sr);
    }

    void control (float dement, float stops, int blockLen)
    {
        const float dt = (float) blockLen / sr;
        tremPh += tremRate * dt; if (tremPh >= 1.0f) tremPh -= 1.0f;
        const float st = starve.tick (rng); for (int k = 1; k < blockLen; ++k) starve.tick (rng);
        const float tremC = std::sin (kTwoPi * tremPh) * (3.0f + dement * 28.0f);
        const float sagC  = -dement * 110.0f * std::min (1.0f, t / 6.0f);
        const float common = tremC + sagC + st * dement * 55.0f;
        for (int i = 0; i < kRanks; ++i)
        {
            auto& rk = ranks[(size_t) i];
            const float cents = common + rk.detune * dement * 38.0f;
            rk.inc = std::min (0.45f, base * kRatio[i] * centsToRatio (cents) / sr);
            const float x = (stops - kThreshold[i]) / 0.12f;
            rk.gain = clampf (x, 0.0f, 1.0f) * kGain[i];
        }
        cipherInc = std::min (0.45f, base * (cipherUp ? 1.0595f : 0.9439f) * centsToRatio (common) / sr);
        cipherGain = dement * dement * 0.35f;
        ampTrem = 1.0f - (0.08f + dement * 0.22f) * (0.5f + 0.5f * std::sin (kTwoPi * tremPh));
        t += dt;
    }

    void tick (float& l, float& r)
    {
        float sl = 0, sr_ = 0;
        for (int i = 0; i < kRanks; ++i)
        {
            auto& rk = ranks[(size_t) i];
            if (rk.gain < 0.001f) { rk.ph += rk.inc; if (rk.ph >= 1.0f) rk.ph -= 1.0f; continue; }
            float v;
            if (kType[i] == 2) v = rk.saw.tick (rk.inc) * 0.6f;
            else
            {
                rk.ph += rk.inc; if (rk.ph >= 1.0f) rk.ph -= 1.0f;
                const float a = kTwoPi * rk.ph;
                v = kType[i] == 0 ? std::sin (a) + 0.06f * std::sin (2.0f * a)
                                  : std::sin (a) + 0.45f * std::sin (2.0f * a) + 0.22f * std::sin (3.0f * a);
            }
            v *= rk.gain;
            if (i & 1) sr_ += v; else sl += v;
        }
        cipherPh += cipherInc; if (cipherPh >= 1.0f) cipherPh -= 1.0f;
        const float ci = std::sin (kTwoPi * cipherPh) * cipherGain;
        float ch = 0;
        if (chiffT < 0.08f * sr) { ch = chiff.bp (rng.bi()) * std::exp (-chiffT / (0.025f * sr)) * 0.5f * (0.5f + 0.5f * velocity); chiffT += 1.0f; }
        const float wind = windHp.hp (rng.bi()) * 0.012f;
        l = ((sl + sr_ * 0.35f + ci) * ampTrem + ch + wind) * 0.38f;
        r = ((sr_ + sl * 0.35f + ci * 0.6f) * ampTrem + ch + wind) * 0.38f;
    }

private:
    static constexpr int kRanks = 9;
    // 16' flute, 8' flute, 8' principal, 4' flute, 2 2/3', 2', mixture, 8' reed, 16' reed
    static constexpr float kRatio[kRanks]     = { 0.5f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 1.0f, 0.5f };
    static constexpr int   kType[kRanks]      = { 0,    0,    1,    0,    1,    1,    1,    2,    2 };
    static constexpr float kThreshold[kRanks] = { 0.42f, -0.2f, 0.22f, 0.08f, 0.52f, 0.62f, 0.75f, 0.68f, 0.86f };
    static constexpr float kGain[kRanks]      = { 0.6f, 0.8f, 0.6f, 0.45f, 0.3f, 0.25f, 0.2f, 0.5f, 0.45f };
    struct Rank { float ph = 0, inc = 0.01f, detune = 0, gain = 0; Saw saw; };
    float sr = 48000, base = 220, t = 0, velocity = 1, cipherPh = 0, cipherInc = 0, cipherGain = 0, tremPh = 0, tremRate = 6, ampTrem = 1, chiffT = 1e9f;
    bool cipherUp = true;
    Rng rng; Drift starve; std::array<Rank, kRanks> ranks; SVF chiff, windHp;
};

//==============================================================================
// HEARTBEAT — lub-dub thumps while the key is held. RATE = beats per minute. PANIC speeds the heart
// up the longer you hold, makes it irregular (with skipped / doubled beats) and pounding.
// Lower keys = deeper, bigger heart. Pattern is fixed per key.
class HeartbeatLayer
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    void start (float freq, float vel, uint32_t seed)
    {
        rng = Rng (seed); velocity = vel;
        pitchMul = clampf (std::pow (freq / 65.4f, 0.22f), 0.65f, 1.6f);
        t = 0; nextBeat = 0.02f; lubT = dubT = 1e9f; dubDelay = 0.3f; beatAmp = 1;
        thud.reset(); thud.set (140.0f * pitchMul, 0.7f, sr);
    }

    void tick (float rate, float panic, float& l, float& r)
    {
        if (t >= nextBeat)
        {
            const float bpm = rate + panic * 75.0f * std::min (1.0f, t / 10.0f);
            float interval = 60.0f / bpm * (1.0f + rng.bi() * panic * 0.22f);
            if (panic > 0.4f && rng.uni() < (panic - 0.4f) * 0.35f) interval *= (rng.uni() < 0.5f ? 0.55f : 1.6f);   // extra / skipped beat
            dubDelay = clampf (60.0f / bpm * 0.33f, 0.14f, 0.32f);
            lubT = 0; dubT = -dubDelay;
            beatAmp = (0.85f + 0.3f * rng.uni() * panic) * (1.0f + panic * 0.4f);
            nextBeat = t + interval;
        }
        float v = 0;
        if (lubT < 0.35f)
        {
            lubPh += (42.0f + 34.0f * std::exp (-lubT * 30.0f)) * pitchMul / sr; if (lubPh >= 1.0f) lubPh -= 1.0f;
            v += std::sin (kTwoPi * lubPh) * std::exp (-lubT * 13.0f) + thud.lp (rng.bi()) * std::exp (-lubT * 45.0f) * 1.2f;
        }
        if (dubT >= 0.0f && dubT < 0.3f)
        {
            dubPh += (58.0f + 36.0f * std::exp (-dubT * 35.0f)) * pitchMul / sr; if (dubPh >= 1.0f) dubPh -= 1.0f;
            v += (std::sin (kTwoPi * dubPh) * std::exp (-dubT * 18.0f) + thud.lp (rng.bi()) * std::exp (-dubT * 55.0f)) * 0.7f;
        }
        lubT += 1.0f / sr; dubT += 1.0f / sr; t += 1.0f / sr;
        const float y = std::tanh (v * beatAmp * 1.4f) * (0.5f + 0.5f * velocity) * 0.9f;
        l = r = y;
    }

private:
    float sr = 48000, velocity = 1, pitchMul = 1, t = 0, nextBeat = 0, lubT = 1e9f, dubT = 1e9f, dubDelay = 0.3f, beatAmp = 1, lubPh = 0, dubPh = 0;
    Rng rng; SVF thud;
};

//==============================================================================
// Sustained voice: Drone (detuned saws + sub + folder), Metal (bowed combs), Air (gusting wind), plus the other layers
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
        piano.prepare (sr); ghost.prepare (sr); whisper.prepare (sr, seed ^ 0x51ED27u); bells.prepare (sr);
        chains.prepare (sr, seed ^ 0xC4A1u);
        strings.prepare (sr, seed ^ 0x5F0Du);
        organ.prepare (sr); heart.prepare (sr);
    }

    void start (int n, float velocity, const Params& p)
    {
        note = n; vel = velocity; baseFreq = mtof ((float) n);
        rng = Rng (keySeed (n, 1u));                       // same key -> same "random" behaviour
        if (! amp.active())
        {
            filtL.reset(); filtR.reset(); combA.clear(); combB.clear();
            for (int k = 0; k < 5; ++k) { osc[(size_t) k].phase = rng.uni(); drift[(size_t) k].reset(); }
            pitchDrift.reset(); gust[0].reset(); gust[1].reset(); subPhase = 0;
            windL.reset(); windR.reset();
        }
        amp.set (p.attack, p.decay, p.sustain, p.release, sr);
        fenv.set (std::max (0.005f, p.attack * 0.6f), p.decay * 1.5f + 0.2f, 0.0f, p.release, sr);
        amp.on(); fenv.on();
        piano.start (n, baseFreq, velocity, p.pianoBreak);
        ghost.start (n, baseFreq, velocity, p.ghostBreak);
        whisper.start (baseFreq, keySeed (n, 2u));
        bells.start (baseFreq, velocity, keySeed (n, 3u), p.bellSinister);
        chains.start (baseFreq, velocity, keySeed (n, 4u));
        strings.start (baseFreq, velocity, n, keySeed (n, 5u));
        organ.start (baseFreq, velocity, keySeed (n, 6u));
        heart.start (baseFreq, velocity, keySeed (n, 7u));
    }
    void stop() { amp.off(); fenv.off(); }
    void kill() { amp.st = ADSR::Idle; amp.v = 0; }

    // Renders and ADDS into L/R. 'derived' values are computed once per block by the engine.
    void render (float* L, float* R, int n, const Params& p, float driftCents, float driftRate,
                 float cutoffMul, float subAmt, float airAmt)
    {
        for (int i = 0; i < 5; ++i) drift[i].setRate (driftRate * (0.6f + 0.2f * (float) i), sr);
        for (int i = 0; i < 2; ++i) gust[i].setRate ((0.08f + p.airMotion * 0.9f) * (1.0f + 0.31f * (float) i), sr);
        const float windScale = std::pow (baseFreq / 261.6f, 0.15f);
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

            // wind: band-passed noise whose centre and level follow slow gusts
            const float g0 = gust[0].from * 0.5f + 0.5f, g1 = gust[1].from * 0.5f + 0.5f;
            windL.set ((200.0f + g0 * g0 * 1300.0f) * windScale, 1.6f + g0 * 1.4f, sr);
            windR.set ((200.0f + g1 * g1 * 1300.0f) * windScale, 1.6f + g1 * 1.4f, sr);
            if (p.organLevel > 0.0001f)   organ.control (p.organDement, p.organStops, m);
            if (p.whisperLevel > 0.0001f) whisper.control (p.airMotion, p.whisperDemon);
            if (p.bellLevel > 0.0001f)    bells.control (m);
            if (p.chainLevel > 0.0001f)   chains.control (p.chainRattle, p.chainRust);
            if (p.symphonyLevel > 0.0001f) strings.control (p.symphonyTwist, m);

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

                // --- Air (gusting wind)
                const float ga = gust[0].tick (rng) * 0.5f + 0.5f, gb = gust[1].tick (rng) * 0.5f + 0.5f;
                const float airGain = p.airLevel + airAmt;
                float awL = 0, awR = 0;
                if (airGain > 0.0001f)
                {
                    awL = windL.bp (rng.bi()) * (0.15f + 0.85f * ga * ga) * 3.6f;
                    awR = windR.bp (rng.bi()) * (0.15f + 0.85f * gb * gb) * 3.6f;
                }

                float xl = dl * p.droneLevel + ml * p.metalLevel + awL * airGain;
                float xr = dr * p.droneLevel + mr * p.metalLevel + awR * airGain;

                float el, er;
                if (p.pianoLevel > 0.0001f)   { piano.tick (rng, el, er);                                 xl += el * p.pianoLevel;   xr += er * p.pianoLevel; }
                if (p.ghostLevel > 0.0001f)   { ghost.tick (rng, el, er);                                 xl += el * p.ghostLevel;   xr += er * p.ghostLevel; }
                if (p.whisperLevel > 0.0001f) { whisper.tick (p.airMotion, p.whisperDemon, el, er);       xl += el * p.whisperLevel; xr += er * p.whisperLevel; }
                if (p.bellLevel > 0.0001f)    { bells.tick (el, er);                                      xl += el * p.bellLevel;    xr += er * p.bellLevel; }
                if (p.chainLevel > 0.0001f)   { chains.tick (p.chainRattle, p.chainRust, el, er);         xl += el * p.chainLevel;   xr += er * p.chainLevel; }
                if (p.symphonyLevel > 0.0001f) { strings.tick (el, er);                                   xl += el * p.symphonyLevel; xr += er * p.symphonyLevel; }
                if (p.organLevel > 0.0001f)   { organ.tick (el, er);                                      xl += el * p.organLevel;   xr += er * p.organLevel; }
                if (p.heartLevel > 0.0001f)   { heart.tick (p.heartRate, p.heartPanic, el, er);           xl += el * p.heartLevel;   xr += er * p.heartLevel; }

                xl = filtL.lp (xl); xr = filtR.lp (xr);
                const float g = a * (0.35f + 0.65f * vel) * 0.5f;
                L[s + j] += xl * g; R[s + j] += xr * g;
            }
        }
    }

private:
    float sr = 48000, vel = 1, baseFreq = 110, subPhase = 0;
    Rng rng;
    std::array<Saw, 5> osc; std::array<Drift, 5> drift; Drift pitchDrift; std::array<Drift, 2> gust;
    DelayLine combA, combB; OnePole combDampA, combDampB;
    SVF filtL, filtR, windL, windR;
    ADSR amp, fenv;
    PianoLayer piano; WhisperLayer whisper; BellsLayer bells; ChainLayer chains; StringsLayer strings;
    OrganLayer organ; HeartbeatLayer heart; GhostPianoLayer ghost;
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
        rng = Rng (keySeed (note, 100u + (uint32_t) type));   // same key -> same hit
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
        // DREAD adds to these, but never pins them at maximum: the knob always keeps room to move
        const float ringMix    = 1.0f - (1.0f - clampf (p.ringMix, 0.0f, 1.0f)) * (1.0f - d * 0.3f);
        const float wobble     = 1.0f - (1.0f - clampf (p.wobble, 0.0f, 1.0f)) * (1.0f - d * 0.35f);
        reverb.setSize (1.0f - (1.0f - clampf (p.reverbSize, 0.0f, 1.0f)) * (1.0f - d * 0.2f));

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
