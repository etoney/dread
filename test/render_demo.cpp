// Offline render test: g++ -O2 -std=c++17 -I../Source render_demo.cpp -o render && ./render demo.wav
#include "DreadEngine.h"
#include <cstdio>
#include <cstdlib>

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes;
    const uint16_t fmt = 1, ch = 2, bits = 16, align = 4; const uint32_t rate = (uint32_t) sr, bps = rate * 4, fmtLen = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f); std::fwrite (&bps, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t s[2] = { (int16_t) (dread::clampf (L[i], -1, 1) * 32767), (int16_t) (dread::clampf (R[i], -1, 1) * 32767) };
        std::fwrite (s, 2, 2, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const int sr = 48000, block = 256;
    dread::Engine e; e.prepare (sr, block);
    std::vector<float> L, R; std::vector<float> bl (block), br (block);
    double ppq = 0; const double bpm = 90;
    float peak = 0; bool bad = false;

    auto run = [&] (float seconds) {
        int blocks = (int) (seconds * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            e.process (bl.data(), br.data(), block, bpm, ppq);
            ppq += block * bpm / 60.0 / sr;
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (bl[i]) || ! std::isfinite (br[i])) bad = true;
                peak = std::max (peak, std::max (std::abs (bl[i]), std::abs (br[i])));
                L.push_back (bl[i]); R.push_back (br[i]);
            }
        }
    };

    // 1) Low dread drone cluster
    e.params.dread = 0.35f; e.noteOn (38, 0.8f); e.noteOn (45, 0.7f); e.noteOn (51, 0.6f);
    run (5.0f);
    // 2) crank dread + metal, add gate
    e.params.dread = 0.85f; e.params.metalLevel = 0.8f; e.params.gateDepth = 0.9f; e.params.gateRate = 1;
    run (4.0f);
    // 3) jumpscare sting
    e.noteOn (96, 1.0f); run (0.5f);
    e.allNotesOff(); e.params.gateDepth = 0.0f; run (3.5f);
    // 4) Torment + ring pulse
    e.params.dread = 0.5f; e.params.torment = 0.7f; e.params.ringMix = 0.5f; e.params.ringFreq = 37;
    e.params.gateDepth = 1.0f; e.params.gateRate = 3; e.params.attack = 0.01f; e.params.release = 0.8f;
    e.noteOn (33, 1.0f); e.noteOn (40, 1.0f); run (4.0f);
    e.allNotesOff(); run (3.0f);

    std::printf ("samples=%zu peak=%.3f finite=%s\n", L.size(), peak, bad ? "NO" : "yes");
    writeWav (argc > 1 ? argv[1] : "demo.wav", L, R, sr);
    return bad ? 1 : 0;
}
