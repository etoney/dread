# DREAD — horror texture designer

An original synth for horror and thriller sound design (VST3 / AU / Standalone, built with JUCE), inspired by the *workflow* of Heavyocity's Psychosis: layered sources, one-knob "make it worse" macros, a rhythmic gate, and a jumpscare zone. It uses no samples. Every sound is synthesized.

## Sound engine
| Layer | What it is |
|---|---|
| **Drone** | 5 detuned band-limited saws with independent random pitch drift, a wavefolder (Fold) and a sub sine |
| **Metal** | Noise-bowed comb resonator pair at an inharmonic ratio (2.756×): bowed cymbals, scraped rails |
| **Air** | Noise through two drifting formant filters: breathy, whisper-like movement |
| **Broken Piano** | Modal piano with stretched partials on detuned string pairs. *Break* knocks keys out of tune (the same key is always wrong the same way), kills some keys and partials, and adds string rattle |
| **Demon Whispers** | Two whisperers speaking random syllables with sibilants. *Demon* lowers the vocal tract and adds a period-doubled growl. Syllable speed follows Air *Motion* |
| **Screams** | Voice-like source that swoops up into each note, with growing wild vibrato, jitter, breath and open-vowel formants. *Rasp* adds roughness, subharmonics and drive |
| **Sting zone** | Notes **C6 and up** fire one-shot jumpscares (noise slam + dive-bomb sweep + inharmonic bell partials). Toggle with *Sting zone* |

**Macros**
- **DREAD**: one knob that raises pitch drift, darkens the filter, adds ring mod, sub, whisper, wobble and reverb size together.
- **TORMENT**: low-end body boost plus heavy saturation.

**FX chain:** Ring mod → Torment → 16-step tempo-synced Gate → Wobble (tape warble) → Ping-pong delay → 8-line FDN reverb (up to ~13 s tails) → Output.

12 factory presets: Unease, Basement Hum, Bowed Morgue, Whispers Behind You, Heartbeat Gate, Asylum Pulse, Total Dread, Shock Stabs, Broken Piano, Demonic Whispers, Screams, Haunted Parlor.

## Building
Requirements: CMake 3.22+, Git, and a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows). JUCE is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target DREAD_VST3 DREAD_Standalone   # add DREAD_AU on macOS
```
The output goes to `build/DREAD_artefacts/Release/`. Copy `DREAD.vst3` to:
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (and `DREAD.component` to `~/Library/Audio/Plug-Ins/Components/`)
- Windows: `C:\Program Files\Common Files\VST3\`

**No compiler?** Push this folder to a GitHub repo. The included workflow (`.github/workflows/build.yml`) builds the macOS and Windows plugins and attaches them as downloadable artifacts on the Actions tab.

On macOS, unsigned builds may need: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/DREAD.vst3`

## Offline DSP test
`Source/DreadEngine.h` doesn't depend on JUCE. Run `cmake --build build --target dread_render && ./build/dread_render demo.wav` to render a 20-second demo.
