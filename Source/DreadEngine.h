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
    }

    void start (int n, float velocity, const Params& p)
    {
        note = n; vel = velocity; baseFreq = mtof ((float) n);
        if (! amp.active()) { filtL.reset(); filtR.reset(); combA.clear(); combB.clear(); }
        amp.set (p.attack, p.decay, p.sustain, p.release, sr);
        fenv.set (std::max (0.005f, p.attack * 0.6f), p.decay * 1.5f + 0.2f, 0.0f, p.release, sr);
        amp.on(); fenv.on();
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
                const float ml = ya * 1.6f + yb * 0.7f, mr = ya * 0.9f + yb * 1.4f;

                // --- Air (whisper formants)
                formDrift[0].tick (rng); formDrift[1].tick (rng);
                const float nz = rng.bi();
                const float air = (formA.bp (nz) + formB.bp (nz) * 0.7f) * 0.9f;

                const float airL = (p.airLevel + airAmt);
                float xl = dl * p.droneLevel + ml * p.metalLevel + air * airL;
                float xr = dr * p.droneLevel + mr * p.metalLevel + air * airL;

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
};

//==============================================================================
// One-shot sting / jumpscare: noise slam + dive-bomb sweep + inharmonic metal partials
class Sting
{
public:
    bool active = false;
    void prepare (float sampleRate, uint32_t seed) { sr = sampleRate; rng = Rng (seed); }
    void trigger (int note, float velocity)
    {
        active = true; t = 0; vel = velocity;
        base = mtof ((float) note - 36.0f);
        sweepPhase = 0; for (auto& ph : partPhase) ph = rng.uni();
        hp.reset(); bpf.reset();
        hp.set (900.0f, 0.7f, sr); bpf.set (base * 4.0f, 3.0f, sr);
    }
    void render (float* L, float* R, int n, float level)
    {
        static constexpr float ratios[6] = { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.4f };
        static constexpr float decays[6] = { 2.2f, 1.6f, 1.1f, 0.8f, 0.55f, 0.4f };
        for (int i = 0; i < n && active; ++i)
        {
            const float tt = t / sr;
            // noise slam
            const float nEnv = std::exp (-tt * 14.0f);
            const float nz = hp.hp (rng.bi()) * nEnv * 0.9f + bpf.bp (rng.bi()) * std::exp (-tt * 5.0f) * 1.5f;
            // dive-bomb sweep 2500Hz -> 35Hz
            const float sf = 35.0f + 2465.0f * std::exp (-tt * 9.0f);
            sweepPhase += sf / sr; if (sweepPhase >= 1.0f) sweepPhase -= 1.0f;
            const float sw = std::tanh (std::sin (kTwoPi * sweepPhase) * 3.0f) * std::exp (-tt * 2.5f) * 0.7f;
            // metal partials
            float pl = 0, pr = 0;
            for (int k = 0; k < 6; ++k)
            {
                partPhase[k] += base * ratios[k] / sr; if (partPhase[k] >= 1.0f) partPhase[k] -= 1.0f;
                const float v = std::sin (kTwoPi * partPhase[k]) * std::exp (-tt / decays[k] * 2.0f) / (1.0f + (float) k * 0.6f);
                if (k & 1) pr += v; else pl += v;
            }
            const float g = level * (0.4f + 0.6f * vel) * 1.0f;
            L[i] += (nz + sw + pl * 0.8f + pr * 0.3f) * g;
            R[i] += (nz * 0.95f + sw + pr * 0.8f + pl * 0.3f) * g;
            t += 1.0f;
            if (tt > 4.0f) active = false;
        }
    }
private:
    float sr = 48000, t = 0, vel = 1, base = 220, sweepPhase = 0;
    std::array<float, 6> partPhase {};
    Rng rng; SVF hp, bpf;
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
            s.trigger (note, velocity);
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
        for (auto& s : stings) if (s.active) s.render (L, R, n, p.stingLevel);

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
            L[i] = clampf (l, -1.5f, 1.5f); R[i] = clampf (r, -1.5f, 1.5f);
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
    std::array<Drift, 2> wobDrift; Rng rng { 4242u };
    float ringPhase = 0; double freeBeat = 0;
};

} // namespace dread
