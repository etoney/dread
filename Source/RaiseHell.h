// RaiseHell.h — "RAISE HELL": randomly, but musically, re-rolls every effect.
// Framework-free so it can be tested offline. Sound sources, envelope, stings and output are never touched.
#pragma once
#include "DreadEngine.h"
#include <string>
#include <utility>
#include <vector>

namespace dread
{
struct HellRoll
{
    std::string character;                                   // e.g. "CORRODED"
    std::vector<std::pair<std::string, float>> values;       // parameter id -> real value
};

namespace hell
{
inline float pick (Rng& r, float lo, float hi, float curve = 1.0f) { return lo + (hi - lo) * std::pow (r.uni(), curve); }
inline float pickLog (Rng& r, float lo, float hi) { return lo * std::pow (hi / lo, r.uni()); }
inline int pickInt (Rng& r, int n) { return std::min (n - 1, (int) (r.uni() * (float) n)); }
inline bool chance (Rng& r, float p) { return r.uni() < p; }

// Musical gate rhythms (16 steps)
inline const std::vector<const char*>& gatePatterns()
{
    static const std::vector<const char*> p {
        "x...x...x...x...",   // four on the floor
        "..x...x...x...x.",   // offbeats
        "x..x..x.x..x..x.",   // tresillo 3-3-2
        "x.x.....x.x.....",   // heartbeat
        "x.xxx.xxx.xxx.xx",   // gallop
        "xxxx..xx..x.xx..",   // stutter
        "x.x.x.x.x.x.x.x.",   // eighths
        "x..x.x..x..x.x..",   // limp
        "xxx.xxx.xxx.xx.x",   // chug
    };
    return p;
}
} // namespace hell

inline HellRoll rollHell (uint32_t seed)
{
    using namespace hell;
    Rng r (seed * 2654435761u + 0x5eed);
    for (int i = 0; i < 3; ++i) r.next();

    static const char* names[] = { "CAVERNOUS", "CORRODED", "PULSING", "SEASICK", "SHREDDED", "POSSESSED HALL" };
    const int arch = pickInt (r, 6);

    // ---- baseline roll (moderate everything)
    float dread    = pick (r, 0.15f, 0.8f);
    float torment  = pick (r, 0.0f, 0.45f, 1.6f);
    float ringMix  = chance (r, 0.35f) ? pick (r, 0.05f, 0.35f) : 0.0f;
    float ringFreq = chance (r, 0.6f) ? pick (r, 18.0f, 70.0f) : pickLog (r, 90.0f, 900.0f);
    float wobble   = pick (r, 0.0f, 0.45f, 1.5f);
    int   distType = pickInt (r, 6);
    float distDrive = chance (r, 0.55f) ? pick (r, 0.1f, 0.6f) : 0.0f;
    float distTone = pick (r, 0.35f, 0.85f);
    float distMix  = pick (r, 0.5f, 1.0f);
    float gateDepth = chance (r, 0.3f) ? pick (r, 0.5f, 1.0f) : 0.0f;
    int   gateRate = pickInt (r, 4);
    float delayMix = chance (r, 0.6f) ? pick (r, 0.05f, 0.35f) : 0.0f;
    float delayFb  = pick (r, 0.2f, 0.7f);
    int   delayTime = pickInt (r, 4);
    float reverbMix = pick (r, 0.2f, 0.65f);
    float reverbSize = pick (r, 0.4f, 1.0f);
    float cutoff   = pickLog (r, 2500.0f, 16000.0f);
    float reso     = pick (r, 0.0f, 0.45f, 1.5f);
    float envAmt   = pick (r, 0.0f, 0.5f, 1.4f);

    // ---- character
    switch (arch)
    {
        case 0: // CAVERNOUS: huge space, little grit
            reverbMix = pick (r, 0.45f, 0.62f); reverbSize = pick (r, 0.85f, 1.0f);
            delayMix = pick (r, 0.1f, 0.3f); distDrive *= 0.4f; torment *= 0.5f; gateDepth = 0.0f;
            break;
        case 1: // CORRODED: broken electronics
        {
            static const int t[] = { 4, 5, 2 };   // Bitcrush, Blown Speaker, Fuzz
            distType = t[pickInt (r, 3)]; distDrive = pick (r, 0.45f, 0.85f); distTone = pick (r, 0.3f, 0.6f);
            torment = pick (r, 0.2f, 0.5f); reverbMix = pick (r, 0.15f, 0.35f); cutoff = pickLog (r, 3500.0f, 9000.0f);
            break;
        }
        case 2: // PULSING: rhythmic chop
            gateDepth = pick (r, 0.75f, 1.0f); { static const int gr[] = { 1, 3, 0 }; gateRate = gr[pickInt (r, 3)]; }
            torment = pick (r, 0.2f, 0.5f); delayMix = pick (r, 0.1f, 0.3f); delayTime = chance (r, 0.5f) ? 1 : 3;
            break;
        case 3: // SEASICK: warped, throbbing, wrong
            wobble = pick (r, 0.45f, 0.9f); ringMix = pick (r, 0.15f, 0.45f); ringFreq = pick (r, 18.0f, 45.0f);
            dread = pick (r, 0.5f, 0.9f); distDrive *= 0.5f;
            break;
        case 4: // SHREDDED: aggressive
        {
            static const int t[] = { 2, 0, 3 };   // Fuzz, Tube, Wavefold
            distType = t[pickInt (r, 3)]; distDrive = pick (r, 0.55f, 0.9f); torment = pick (r, 0.35f, 0.65f);
            cutoff = pickLog (r, 4000.0f, 12000.0f); reverbMix = pick (r, 0.15f, 0.3f); delayMix = 0.0f;
            break;
        }
        default: // POSSESSED HALL: echoes that answer back
            delayMix = pick (r, 0.25f, 0.45f); delayFb = pick (r, 0.55f, 0.8f); delayTime = 1;
            reverbMix = pick (r, 0.4f, 0.6f); wobble = pick (r, 0.2f, 0.4f);
            ringMix = pick (r, 0.1f, 0.25f); ringFreq = pickLog (r, 150.0f, 700.0f);
            break;
    }

    // ---- "intelligence": keep combinations musical and safe
    if (distDrive > 0.5f) distTone = std::min (distTone, 0.65f);                 // tame fizz
    if (torment > 0.4f && distDrive > 0.5f) distDrive *= 0.75f;                   // avoid mush
    if (distDrive < 0.001f) distMix = 1.0f;
    if (reverbMix + delayMix > 1.0f) { const float k = 1.0f / (reverbMix + delayMix); reverbMix *= k; delayMix *= k; }
    if (reverbMix > 0.55f) delayFb = std::min (delayFb, 0.6f);                   // no runaway wash
    if (ringMix > 0.3f && ringFreq > 200.0f) ringMix *= 0.6f;                     // metallic ring gets harsh fast
    if (reso > 0.35f) cutoff = std::max (cutoff, 3000.0f);                        // no whistling mud
    if (gateDepth > 0.0f) wobble = std::min (wobble, 0.5f);                       // keep rhythm readable
    // loudness budget: big rooms + long echoes + DREAD (which adds sub, air and room) stack up fast
    {
        const float wet = reverbMix * (0.6f + 0.6f * reverbSize) + delayMix * (1.0f + delayFb) + dread * 0.7f;
        if (wet > 0.95f) { const float k = 0.95f / wet; reverbMix *= k; delayMix *= k; dread *= k; }
    }
    // the quietest results were heavy crush/fuzz through a closed filter: open it up a bit
    if (distDrive > 0.5f) cutoff = std::max (cutoff, 4000.0f);

    // ---- gate pattern: mostly musical templates, sometimes a fresh random one
    std::string pat;
    if (chance (r, 0.7f)) pat = gatePatterns()[(size_t) pickInt (r, (int) gatePatterns().size())];
    else
    {
        const float density = pick (r, 0.4f, 0.7f);
        for (int i = 0; i < 16; ++i) pat += (i == 0 || chance (r, density)) ? 'x' : '.';
    }

    HellRoll roll;
    roll.character = names[arch];
    auto& v = roll.values;
    v = { { "dread", dread }, { "torment", torment }, { "ringMix", ringMix }, { "ringFreq", ringFreq },
          { "wobble", wobble }, { "distType", (float) distType }, { "distDrive", distDrive }, { "distTone", distTone },
          { "distMix", distMix }, { "gateDepth", gateDepth }, { "gateRate", (float) gateRate },
          { "delayMix", delayMix }, { "delayFeedback", std::min (delayFb, 0.9f) }, { "delayTime", (float) delayTime },
          { "reverbMix", reverbMix }, { "reverbSize", reverbSize },
          { "cutoff", cutoff }, { "resonance", reso }, { "envAmount", envAmt } };
    for (int i = 0; i < 16; ++i) v.push_back ({ "gate" + std::to_string (i), pat[(size_t) i] == 'x' ? 1.0f : 0.0f });
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
