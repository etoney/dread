# DREAD — horror texture designer

An original synth for horror and thriller sound design (VST3 / AU / Standalone, built with JUCE), inspired by the *workflow* of Heavyocity's Psychosis: layered sources, one-knob "make it worse" macros, a rhythmic gate, and a jumpscare zone. It uses no samples. Every sound is synthesized.

## Sound engine
| Layer | What it is |
|---|---|
| **Drone** | 5 detuned band-limited saws with independent random pitch drift, a wavefolder (Fold) and a sub sine |
| **Metal** | Noise-bowed comb resonator pair at an inharmonic ratio (2.756×): bowed cymbals, scraped rails |
| **Air** | Noise through two drifting formant filters: breathy, whisper-like movement |
| **Broken Piano** | Physical-model piano with stretched partials on detuned string pairs. *Break* knocks keys out of tune (the same key is always wrong the same way), kills some keys and partials, and adds string rattle |
| **Demon Whispers** | Two whisperers speaking random syllables with sibilants. *Demon* lowers the vocal tract and adds a period-doubled growl. Syllable speed follows Air *Motion* |
| **Twisted Strings** | Bowed string section per note: 8 players with their own tuning, bow timing and late-blooming vibrato, stick-slip bow noise and a violin/viola/cello body resonance. *Sinister* = players drift and slide apart, wide slow vibrato, sul ponticello, cluster notes, bow tremolo, sagging pitch |
| **Hell's Bells** | Modal church bell with real bell partial tuning (hum, prime, tierce, quint, nominal...), beating mode pairs and a clapper strike. *Sinister* = cursed tuning (minor 2nd, tritone, flat octaves), faster beating, doom sub-octave, darker strike, sinking pitch |
| **Ghost Piano** | The original additive DREAD piano: glassy, bell-like stretched partials on detuned string pairs. *Break* = out-of-tune keys, missing overtones, rattle, dead keys |
| **Demented Pipe Organ** | Additive pipe ranks (16' to mixtures and reeds). *Stops* = soft flute → full organ; *Dement* = ranks out of tune with each other, tremulant, wind-starved wobble, sagging pitch, a ciphering wrong pipe |
| **Heartbeat** | Lub-dub heartbeat while a key is held. *Rate* in BPM; *Panic* speeds it up over time, makes it irregular and pounding |
| **Rusty Chains** | Link-on-link impacts in little cascades through three banks of metal resonators, plus rusty grinding and creaks. *Rattle* = slow clinks → frantic shaking; *Rust* = duller links + more grinding |
| **Sting zone** | Notes **C6 and up** fire one-shot jumpscares. *Sting Type*: Slam, Shriek Stabs, Sub Boom, Metal Crash. Stings skip Torment/Distortion/Gate so they hit at full force |

**Macros**
- **DREAD**: one knob that raises pitch drift, darkens the filter, adds ring mod, sub, whisper, wobble and reverb size together.
- **TORMENT**: low-end body boost plus heavy saturation.

**RAISE HELL** button: re-rolls every effect within one of 11 characters (often blended with a second, sometimes with a wildcard effect pushed hard), never repeating the last character, with rules that keep loudness, wet level and distortion musical and leave every knob live as a starting point. Sources, envelope, stings and output are untouched.

**FX chain:** Ring mod → Torment → Distortion (Tube, Tape, Fuzz, Wavefold, Bitcrush, Blown Speaker, with Drive / Tone / Mix) → 16-step tempo-synced Gate → Wobble (tape warble) → Ping-pong delay → 8-line FDN reverb (up to ~13 s tails) → Output.

23 factory presets: Init (Clean), Unease, Basement Hum, Bowed Morgue, Whispers Behind You, Heartbeat Gate, Asylum Pulse, Total Dread, Shock Stabs, Broken Piano, Demonic Whispers, Haunted Parlor, Rusty Chains, Dungeon, Twisted Strings, Funeral Bells, Hell's Bells, Church Bells (Clean), Satanic Strings, Ghost Piano, Demented Organ, Heartbeat, Panic Attack.

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
