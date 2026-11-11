// RaiseHell.h — "RAISE HELL": randomly, but musically, re-rolls every effect.
// Framework-free so it can be tested offline. Sound sources, envelope, stings and output are never touched.
//
// Design goals
//  * Varied: 11 characters, ~40% of rolls blend two characters, a wildcard effect pushed hard,
//    independent on/off chances per effect, mutated gate rhythms.
//  * A starting point, not an end point: rolls keep headroom (no squashing against the limiter), and
//    any effect a roll leaves OFF gets its amount knob at 0 with its other settings ready, so turning
//    that one knob up brings the effect straight in.
#pragma once
#include "DreadEngine.h"
#include <string>
#include <utility>
#include <vector>

namespace dread
{
struct HellRoll
{
    std::string character;                                   // e.g. "SEASICK / CORRODED"
    int primary = -1;                                        // index of the main character (used to avoid repeats)
    std::vector<std::pair<std::string, float>> values;       // parameter id -> real value
};

namespace hell
{
inline float pick (Rng& r, float lo, float hi, float curve = 1.0f) { return lo + (hi - lo) * std::pow (r.uni(), curve); }
inline float pickLog (Rng& r, float lo, float hi) { return lo * std::pow (hi / lo, r.uni()); }
inline int pickInt (Rng& r, int n) { return std::min (n - 1, (int) (r.uni() * (float) n)); }
inline bool chance (Rng& r, float p) { return r.uni() < p; }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }

inline const std::vector<const char*>& gatePatterns()
{
    static const std::vector<const char*> p {
        "x...x...x...x...", "..x...x...x...x.", "x..x..x.x..x..x.", "x.x.....x.x.....",
        "x.xxx.xxx.xxx.xx", "xxxx..xx..x.xx..", "x.x.x.x.x.x.x.x.", "x..x.x..x..x.x..",
        "xxx.xxx.xxx.xx.x", "x...x..x..x.x...", "xx.x.xx.x.xx.x.x", "x.....x...x.....",
        "x..xx..xx..xx..x", "xxxxxxxx........", "x.x..x.x..x.x..x", "xx..xx..x.x.x.x.",
    };
    return p;
}

struct Fx
{
    float dread = 0.3f, torment = 0, ringMix = 0, ringFreq = 40, wobble = 0;
    int distType = 0; float distDrive = 0, distTone = 0.6f, distMix = 1;
    float gateDepth = 0; int gateRate = 1; std::string pattern = "x...x...x...x...";
    float delayMix = 0, delayFb = 0.4f; int delayTime = 1;
    float reverbMix = 0.3f, reverbSize = 0.6f;
    float cutoff = 8000, reso = 0.1f, envAmt = 0.1f;
};

inline std::string randomPattern (Rng& r)
{
    if (chance (r, 0.75f))
    {
        std::string p = gatePatterns()[(size_t) pickInt (r, (int) gatePatterns().size())];
        if (chance (r, 0.5f))                                  // mutate a few steps so it isn't always a stock rhythm
            for (int k = 0, n = 1 + pickInt (r, 3); k < n; ++k) { auto& c = p[(size_t) pickInt (r, 16)]; c = (c == 'x') ? '.' : 'x'; }
        if (chance (r, 0.3f)) { const int s = pickInt (r, 16); p = p.substr ((size_t) s) + p.substr (0, (size_t) s); }   // rotate
        return p;
    }
    std::string p; const float density = pick (r, 0.3f, 0.75f);
    for (int i = 0; i < 16; ++i) p += chance (r, density) ? 'x' : '.';
    return p;
}

static const char* const kNames[] = { "CAVERNOUS", "CORRODED", "PULSING", "SEASICK", "SHREDDED", "POSSESSED HALL",
                                      "UNDERWATER", "RADIO FROM HELL", "FROZEN", "GLITCH", "DYING MACHINE" };
constexpr int kNumCharacters = 11;

// One character's roll. Every effect gets an independent on/off chance first, then the character shapes it.
inline Fx rollCharacter (Rng& r, int arch)
{
    Fx f;
    f.dread    = pick (r, 0.0f, 0.75f);
    f.torment  = chance (r, 0.45f) ? pick (r, 0.05f, 0.4f, 1.4f) : 0.0f;
    f.ringMix  = chance (r, 0.35f) ? pick (r, 0.05f, 0.4f) : 0.0f;
    f.ringFreq = chance (r, 0.55f) ? pick (r, 8.0f, 70.0f) : pickLog (r, 90.0f, 1200.0f);
    f.wobble   = chance (r, 0.5f) ? pick (r, 0.05f, 0.6f, 1.3f) : 0.0f;
    f.distType = pickInt (r, 6);
    f.distDrive = chance (r, 0.5f) ? pick (r, 0.1f, 0.65f) : 0.0f;
    f.distTone = pick (r, 0.3f, 0.9f);
    f.distMix  = pick (r, 0.4f, 1.0f);
    f.gateDepth = chance (r, 0.3f) ? pick (r, 0.4f, 1.0f) : 0.0f;
    f.gateRate = pickInt (r, 4);
    f.pattern  = randomPattern (r);
    f.delayMix = chance (r, 0.55f) ? pick (r, 0.05f, 0.4f) : 0.0f;
    f.delayFb  = pick (r, 0.15f, 0.75f);
    f.delayTime = pickInt (r, 4);
    f.reverbMix = pick (r, 0.1f, 0.65f);
    f.reverbSize = pick (r, 0.25f, 1.0f);
    f.cutoff   = pickLog (r, 1800.0f, 16000.0f);
    f.reso     = pick (r, 0.0f, 0.55f, 1.4f);
    f.envAmt   = pick (r, 0.0f, 0.7f, 1.3f);

    switch (arch)
    {
        case 0: // CAVERNOUS
            f.reverbMix = pick (r, 0.45f, 0.65f); f.reverbSize = pick (r, 0.85f, 1.0f); f.delayMix = pick (r, 0.05f, 0.3f);
            f.distDrive *= 0.4f; f.torment *= 0.5f; f.gateDepth = 0.0f; break;
        case 1: // CORRODED
        { static const int t[] = { 4, 5, 2 }; f.distType = t[pickInt (r, 3)]; f.distDrive = pick (r, 0.4f, 0.75f); f.distTone = pick (r, 0.3f, 0.6f);
          f.reverbMix = pick (r, 0.1f, 0.35f); f.cutoff = pickLog (r, 3000.0f, 9000.0f); break; }
        case 2: // PULSING
        { static const int gr[] = { 1, 3, 0, 2 }; f.gateDepth = pick (r, 0.7f, 1.0f); f.gateRate = gr[pickInt (r, 4)];
          f.delayMix = pick (r, 0.1f, 0.3f); f.delayTime = chance (r, 0.5f) ? 1 : 3; break; }
        case 3: // SEASICK
            f.wobble = pick (r, 0.45f, 0.95f); f.ringMix = pick (r, 0.15f, 0.45f); f.ringFreq = pick (r, 12.0f, 45.0f);
            f.dread = pick (r, 0.4f, 0.85f); f.distDrive *= 0.5f; break;
        case 4: // SHREDDED
        { static const int t[] = { 2, 0, 3 }; f.distType = t[pickInt (r, 3)]; f.distDrive = pick (r, 0.5f, 0.8f); f.torment = pick (r, 0.25f, 0.5f);
          f.cutoff = pickLog (r, 4000.0f, 14000.0f); f.reverbMix = pick (r, 0.1f, 0.3f); f.delayMix = 0.0f; break; }
        case 5: // POSSESSED HALL
            f.delayMix = pick (r, 0.25f, 0.45f); f.delayFb = pick (r, 0.5f, 0.8f); f.delayTime = chance (r, 0.6f) ? 1 : 0;
            f.reverbMix = pick (r, 0.35f, 0.6f); f.wobble = pick (r, 0.15f, 0.4f); f.ringMix = pick (r, 0.08f, 0.25f); f.ringFreq = pickLog (r, 150.0f, 800.0f); break;
        case 6: // UNDERWATER
            f.cutoff = pickLog (r, 500.0f, 1500.0f); f.reso = pick (r, 0.3f, 0.6f); f.envAmt = pick (r, 0.0f, 0.3f);
            f.wobble = pick (r, 0.3f, 0.65f); f.reverbMix = pick (r, 0.3f, 0.55f); f.distDrive = 0.0f; f.gateDepth *= 0.5f; break;
        case 7: // RADIO FROM HELL
        { static const int t[] = { 4, 0, 5 }; f.distType = t[pickInt (r, 3)]; f.distDrive = pick (r, 0.35f, 0.7f); f.distTone = pick (r, 0.25f, 0.5f);
          f.cutoff = pickLog (r, 2500.0f, 5500.0f); f.reso = pick (r, 0.35f, 0.6f); f.ringMix = pick (r, 0.1f, 0.3f); f.ringFreq = pickLog (r, 300.0f, 1200.0f);
          f.reverbMix = pick (r, 0.05f, 0.25f); break; }
        case 8: // FROZEN
            f.reverbMix = pick (r, 0.45f, 0.62f); f.reverbSize = 1.0f; f.delayMix = pick (r, 0.15f, 0.3f); f.delayFb = pick (r, 0.55f, 0.75f);
            f.delayTime = 0; f.wobble = 0.0f; f.gateDepth = 0.0f; f.distDrive = 0.0f; f.torment = 0.0f; f.cutoff = pickLog (r, 7000.0f, 16000.0f); f.envAmt = 0.0f; break;
        case 9: // GLITCH
            f.gateDepth = pick (r, 0.8f, 1.0f); f.gateRate = chance (r, 0.6f) ? 2 : 3; f.pattern = randomPattern (r);
            f.distType = 4; f.distDrive = pick (r, 0.3f, 0.6f); f.delayMix = pick (r, 0.1f, 0.3f); f.delayTime = 3; f.delayFb = pick (r, 0.3f, 0.6f); break;
        default: // DYING MACHINE
            f.wobble = pick (r, 0.6f, 1.0f); f.distType = 5; f.distDrive = pick (r, 0.3f, 0.6f); f.ringMix = pick (r, 0.15f, 0.35f);
            f.ringFreq = pick (r, 5.0f, 18.0f); f.torment = pick (r, 0.1f, 0.35f); break;
    }
    return f;
}

inline Fx blend (const Fx& a, const Fx& b, float t, Rng& r)
{
    Fx f;
    f.dread = lerp (a.dread, b.dread, t); f.torment = lerp (a.torment, b.torment, t);
    f.ringMix = lerp (a.ringMix, b.ringMix, t); f.ringFreq = t < 0.5f ? a.ringFreq : b.ringFreq; f.wobble = lerp (a.wobble, b.wobble, t);
    const bool pickB = r.uni() < t;
    f.distType = pickB ? b.distType : a.distType; f.distDrive = lerp (a.distDrive, b.distDrive, t);
    f.distTone = lerp (a.distTone, b.distTone, t); f.distMix = lerp (a.distMix, b.distMix, t);
    f.gateDepth = lerp (a.gateDepth, b.gateDepth, t); f.gateRate = (r.uni() < t ? b : a).gateRate; f.pattern = (r.uni() < t ? b : a).pattern;
    f.delayMix = lerp (a.delayMix, b.delayMix, t); f.delayFb = lerp (a.delayFb, b.delayFb, t); f.delayTime = (r.uni() < t ? b : a).delayTime;
    f.reverbMix = lerp (a.reverbMix, b.reverbMix, t); f.reverbSize = lerp (a.reverbSize, b.reverbSize, t);
    f.cutoff = std::exp (lerp (std::log (a.cutoff), std::log (b.cutoff), t)); f.reso = lerp (a.reso, b.reso, t); f.envAmt = lerp (a.envAmt, b.envAmt, t);
    return f;
}
} // namespace hell

inline HellRoll rollHell (uint32_t seed, int avoidCharacter = -1)
{
    using namespace hell;
    Rng r (seed * 2654435761u + 0x5eed);
    for (int i = 0; i < 3; ++i) r.next();

    int a = pickInt (r, kNumCharacters);
    if (a == avoidCharacter) a = (a + 1 + pickInt (r, kNumCharacters - 1)) % kNumCharacters;   // never the same character twice in a row
    Fx f = rollCharacter (r, a);
    std::string name = kNames[a];
    if (chance (r, 0.4f))                                       // blend in a second character
    {
        int b = pickInt (r, kNumCharacters); if (b == a) b = (b + 1 + pickInt (r, kNumCharacters - 1)) % kNumCharacters;
        const float t = pick (r, 0.3f, 0.6f);
        f = blend (f, rollCharacter (r, b), t, r);
        name = std::string (kNames[a]) + " / " + kNames[b];
    }
    if (chance (r, 0.25f))                                      // wildcard: push one effect hard
    {
        switch (pickInt (r, 6))
        {
            case 0: f.wobble = pick (r, 0.7f, 1.0f); break;
            case 1: f.ringMix = pick (r, 0.35f, 0.55f); break;
            case 2: f.delayMix = pick (r, 0.35f, 0.5f); f.delayFb = pick (r, 0.6f, 0.8f); break;
            case 3: f.gateDepth = 1.0f; break;
            case 4: f.reso = pick (r, 0.55f, 0.75f); f.envAmt = pick (r, 0.5f, 0.9f); break;
            default: f.distDrive = pick (r, 0.6f, 0.8f); break;
        }
    }

    // ---- keep it musical, and keep headroom so later knob changes are clearly audible
    f.torment   = std::min (f.torment, 0.5f);
    f.dread     = std::min (f.dread, 0.55f);     // DREAD stays moderate so the effect knobs (not the macro) carry the character
    f.distDrive = std::min (f.distDrive, 0.8f);
    if (f.distDrive > 0.5f) f.distTone = std::min (f.distTone, 0.65f);
    if (f.torment > 0.3f && f.distDrive > 0.45f) { f.distDrive *= 0.8f; f.torment *= 0.75f; }   // don't stack saturation
    if (f.reverbMix > 0.55f) f.delayFb = std::min (f.delayFb, 0.6f);
    if (f.ringMix > 0.3f && f.ringFreq > 200.0f) f.ringMix *= 0.6f;
    if (f.reso > 0.35f) f.cutoff = std::max (f.cutoff, std::min (3000.0f, f.cutoff * 2.0f));
    if (f.distDrive > 0.5f) f.cutoff = std::max (f.cutoff, 3500.0f);
    if (f.gateDepth > 0.0f) f.wobble = std::min (f.wobble, 0.6f);
    {
        const float wet = f.reverbMix * (0.6f + 0.6f * f.reverbSize) + f.delayMix * (1.0f + f.delayFb) + f.dread * 0.7f;
        if (wet > 0.95f) { const float k = 0.95f / wet; f.reverbMix *= k; f.delayMix *= k; f.dread *= k; }
    }

    // ---- effects that end up OFF: amount knob at 0, everything else ready to go when you turn it up
    if (f.distDrive < 0.05f) { f.distDrive = 0.0f; f.distMix = 1.0f; }          // DRIVE is the distortion's on-knob
    if (f.distType == 4) { f.distMix = std::max (f.distMix, 0.75f); f.distTone = std::max (f.distTone, 0.62f); }   // bitcrush grit needs mix and top end
    if (f.ringMix < 0.03f)   { f.ringMix = 0.0f; }                               // RING (amount) brings it in
    if (f.delayMix < 0.03f)  { f.delayMix = 0.0f; f.delayFb = std::max (f.delayFb, 0.35f); }
    if (f.gateDepth < 0.05f) { f.gateDepth = 0.0f; }                             // DEPTH brings in the rolled rhythm
    if (f.wobble < 0.03f)    f.wobble = 0.0f;

    HellRoll roll;
    roll.character = name;
    roll.primary = a;
    auto& v = roll.values;
    v = { { "dread", f.dread }, { "torment", f.torment }, { "ringMix", f.ringMix }, { "ringFreq", clampf (f.ringFreq, 5.0f, 2000.0f) },
          { "wobble", f.wobble }, { "distType", (float) f.distType }, { "distDrive", f.distDrive }, { "distTone", f.distTone },
          { "distMix", f.distMix }, { "gateDepth", f.gateDepth }, { "gateRate", (float) f.gateRate },
          { "delayMix", f.delayMix }, { "delayFeedback", std::min (f.delayFb, 0.9f) }, { "delayTime", (float) f.delayTime },
          { "reverbMix", f.reverbMix }, { "reverbSize", f.reverbSize },
          { "cutoff", clampf (f.cutoff, 40.0f, 16000.0f) }, { "resonance", f.reso }, { "envAmount", f.envAmt } };
    // gate rhythm must have both open and closed steps, otherwise DEPTH would do nothing
    {
        int on = 0; for (char c : f.pattern) on += (c == 'x');
        for (int i = 0; on < 3 && i < 16; ++i) if (f.pattern[(size_t) (i * 5 % 16)] != 'x') { f.pattern[(size_t) (i * 5 % 16)] = 'x'; ++on; }
        for (int i = 0; on > 12 && i < 16; ++i) if (f.pattern[(size_t) (i * 7 % 16 + 0)] == 'x' && i * 7 % 16 != 0) { f.pattern[(size_t) (i * 7 % 16)] = '.'; --on; }
    }
    for (int i = 0; i < 16; ++i) v.push_back ({ "gate" + std::to_string (i), f.pattern[(size_t) i] == 'x' ? 1.0f : 0.0f });
    return roll;
}

// Applies a roll to engine Params directly (used by the offline tests).
inline void applyRoll (const HellRoll& roll, Params& p)
{
    for (auto& [id, x] : roll.values)
    {
        if (id == "dread") p.dread = x; else if (id == "torment") p.torment = x; else if (id == "ringMix") p.ringMix = x;
        else if (id == "ringFreq") p.ringFreq = x; else if (id == "wobble") p.wobble = x; else if (id == "distType") p.distType = (int) x;
        else if (id == "distDrive") p.distDrive = x; else if (id == "distTone") p.distTone = x; else if (id == "distMix") p.distMix = x;
        else if (id == "gateDepth") p.gateDepth = x; else if (id == "gateRate") p.gateRate = (int) x; else if (id == "delayMix") p.delayMix = x;
        else if (id == "delayFeedback") p.delayFeedback = x; else if (id == "delayTime") p.delayTime = (int) x;
        else if (id == "reverbMix") p.reverbMix = x; else if (id == "reverbSize") p.reverbSize = x; else if (id == "cutoff") p.cutoff = x;
        else if (id == "resonance") p.resonance = x; else if (id == "envAmount") p.envAmount = x;
        else if (id.rfind ("gate", 0) == 0) p.gateSteps[(size_t) std::stoi (id.substr (4))] = x > 0.5f;
    }
}
} // namespace dread
