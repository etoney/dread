#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
// Factory presets: { paramID, value } pairs applied over the defaults.
namespace
{
struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> p {
        { "Unease (Default)", {} },
        { "Basement Hum", { { "dread", 0.25f }, { "droneDetune", 0.15f }, { "droneSub", 0.7f }, { "metalLevel", 0.1f },
                            { "airLevel", 0.1f }, { "cutoff", 700.0f }, { "reverbMix", 0.25f }, { "torment", 0.3f } } },
        { "Bowed Morgue", { { "dread", 0.5f }, { "droneLevel", 0.2f }, { "metalLevel", 1.0f }, { "metalResonance", 0.92f },
                            { "metalTone", 0.7f }, { "airLevel", 0.15f }, { "reverbMix", 0.55f }, { "reverbSize", 0.9f } } },
        { "Whispers Behind You", { { "dread", 0.6f }, { "droneLevel", 0.15f }, { "metalLevel", 0.1f }, { "airLevel", 0.9f },
                                   { "airMotion", 0.85f }, { "cutoff", 6000.0f }, { "wobble", 0.4f }, { "delayMix", 0.35f } } },
        { "Heartbeat Gate", { { "dread", 0.4f }, { "gateDepth", 1.0f }, { "gateRate", 0.0f }, { "attack", 0.01f },
                              { "torment", 0.55f }, { "droneSub", 0.8f }, { "cutoff", 900.0f }, { "reverbMix", 0.2f } } },
        { "Asylum Pulse", { { "dread", 0.7f }, { "gateDepth", 0.95f }, { "gateRate", 3.0f }, { "ringMix", 0.45f },
                            { "ringFreq", 41.0f }, { "metalLevel", 0.6f }, { "delayMix", 0.3f }, { "delayTime", 3.0f } } },
        { "Total Dread", { { "dread", 1.0f }, { "droneFold", 0.6f }, { "metalLevel", 0.7f }, { "airLevel", 0.5f },
                           { "torment", 0.45f }, { "reverbMix", 0.6f }, { "reverbSize", 1.0f }, { "release", 8.0f } } },
        { "Shock Stabs", { { "dread", 0.55f }, { "attack", 0.005f }, { "decay", 0.4f }, { "sustain", 0.0f }, { "release", 1.5f },
                           { "envAmount", 0.8f }, { "resonance", 0.6f }, { "torment", 0.6f }, { "stingLevel", 1.0f } } },
        { "Broken Piano", { { "droneLevel", 0.0f }, { "metalLevel", 0.0f }, { "airLevel", 0.0f }, { "pianoLevel", 1.0f },
                            { "pianoBreak", 0.65f }, { "dread", 0.25f }, { "attack", 0.002f }, { "sustain", 1.0f },
                            { "release", 1.2f }, { "cutoff", 9000.0f }, { "envAmount", 0.0f }, { "wobble", 0.3f },
                            { "reverbMix", 0.4f }, { "reverbSize", 0.6f }, { "delayMix", 0.12f } } },
        { "Demonic Whispers", { { "droneLevel", 0.0f }, { "metalLevel", 0.15f }, { "airLevel", 0.0f }, { "whisperLevel", 1.0f },
                                { "whisperDemon", 0.75f }, { "airMotion", 0.6f }, { "dread", 0.5f }, { "attack", 0.3f },
                                { "sustain", 1.0f }, { "release", 2.5f }, { "cutoff", 12000.0f }, { "envAmount", 0.0f },
                                { "reverbMix", 0.45f }, { "delayMix", 0.25f }, { "wobble", 0.2f } } },
        { "Screams", { { "droneLevel", 0.0f }, { "metalLevel", 0.0f }, { "airLevel", 0.0f }, { "screamLevel", 1.0f },
                       { "screamRasp", 0.5f }, { "dread", 0.2f }, { "torment", 0.2f }, { "attack", 0.04f }, { "sustain", 1.0f },
                       { "release", 0.6f }, { "cutoff", 14000.0f }, { "envAmount", 0.0f }, { "reverbMix", 0.5f },
                       { "reverbSize", 0.8f }, { "delayMix", 0.1f } } },
        { "Haunted Parlor", { { "droneLevel", 0.15f }, { "metalLevel", 0.0f }, { "airLevel", 0.0f }, { "pianoLevel", 0.9f },
                              { "pianoBreak", 0.5f }, { "whisperLevel", 0.5f }, { "whisperDemon", 0.5f }, { "dread", 0.4f },
                              { "attack", 0.002f }, { "sustain", 1.0f }, { "release", 2.0f }, { "cutoff", 8000.0f },
                              { "envAmount", 0.0f }, { "wobble", 0.35f }, { "reverbMix", 0.5f } } },
        { "Rusty Chains", { { "droneLevel", 0.0f }, { "metalLevel", 0.0f }, { "airLevel", 0.0f }, { "chainLevel", 1.0f },
                            { "chainRattle", 0.45f }, { "chainRust", 0.6f }, { "dread", 0.2f }, { "attack", 0.05f },
                            { "sustain", 1.0f }, { "release", 1.5f }, { "cutoff", 14000.0f }, { "envAmount", 0.0f },
                            { "reverbMix", 0.45f }, { "reverbSize", 0.65f }, { "delayMix", 0.08f } } },
        { "Dungeon", { { "droneLevel", 0.35f }, { "droneSub", 0.6f }, { "metalLevel", 0.0f }, { "airLevel", 0.0f },
                       { "chainLevel", 0.8f }, { "chainRattle", 0.25f }, { "chainRust", 0.8f }, { "whisperLevel", 0.35f },
                       { "whisperDemon", 0.7f }, { "dread", 0.55f }, { "cutoff", 5000.0f }, { "reverbMix", 0.6f },
                       { "reverbSize", 0.95f }, { "release", 4.0f } } },
    };
    return p;
}
} // namespace

//==============================================================================
APVTS::ParameterLayout DreadProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto add = [&] (const char* id, const char* name, float lo, float hi, float def, float skewCentre = -1.0f, const char* suffix = "")
    {
        NormalisableRange<float> r (lo, hi);
        if (skewCentre > 0) r.setSkewForCentre (skewCentre);
        const String sfx (suffix);
        auto toText = [sfx] (float v, int) -> String
        {
            if (sfx == "Hz") return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (juce::roundToInt (v)) + " Hz";
            if (sfx == "s")  return v < 1.0f ? String (juce::roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s";
            if (sfx == "dB") return String (v, 1) + " dB";
            return String (juce::roundToInt (v * 100.0f)) + "%";
        };
        auto fromText = [sfx] (const String& t) -> float
        {
            const float v = t.retainCharacters ("0123456789.-").getFloatValue();
            if (sfx == "Hz") return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
            if (sfx == "s")  return t.containsIgnoreCase ("ms") ? v / 1000.0f : v;
            if (sfx == "dB") return v;
            return v / 100.0f;
        };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                             AudioParameterFloatAttributes().withLabel (suffix)
                                                                 .withStringFromValueFunction (toText)
                                                                 .withValueFromStringFunction (fromText)));
    };

    add ("droneLevel", "Drone Level", 0, 1, 0.8f);
    add ("droneDetune", "Drone Detune", 0, 1, 0.35f);
    add ("droneFold", "Drone Fold", 0, 1, 0.2f);
    add ("droneSub", "Drone Sub", 0, 1, 0.3f);
    add ("metalLevel", "Metal Level", 0, 1, 0.4f);
    add ("metalResonance", "Metal Resonance", 0, 1, 0.7f);
    add ("metalTone", "Metal Tone", 0, 1, 0.5f);
    add ("airLevel", "Air Level", 0, 1, 0.25f);
    add ("airMotion", "Air Motion", 0, 1, 0.4f);

    add ("pianoLevel", "Piano Level", 0, 1, 0);
    add ("pianoBreak", "Piano Break", 0, 1, 0.5f);
    add ("whisperLevel", "Whisper Level", 0, 1, 0);
    add ("whisperDemon", "Whisper Demon", 0, 1, 0.4f);
    add ("screamLevel", "Scream Level", 0, 1, 0);
    add ("screamRasp", "Scream Rasp", 0, 1, 0.4f);

    add ("chainLevel", "Chains Level", 0, 1, 0);
    add ("chainRattle", "Chains Rattle", 0, 1, 0.4f);
    add ("chainRust", "Chains Rust", 0, 1, 0.5f);

    add ("cutoff", "Cutoff", 40, 16000, 2500, 1200, "Hz");
    add ("resonance", "Resonance", 0, 1, 0.25f);
    add ("envAmount", "Filter Env", 0, 1, 0.3f);
    add ("attack", "Attack", 0.001f, 10, 1.2f, 0.8f, "s");
    add ("decay", "Decay", 0.01f, 10, 1.0f, 1.0f, "s");
    add ("sustain", "Sustain", 0, 1, 0.8f);
    add ("release", "Release", 0.01f, 15, 3.0f, 2.0f, "s");

    add ("dread", "DREAD", 0, 1, 0.3f);
    add ("torment", "TORMENT", 0, 1, 0.15f);

    add ("ringMix", "Ring Mix", 0, 1, 0);
    add ("ringFreq", "Ring Freq", 5, 2000, 120, 150, "Hz");
    add ("wobble", "Wobble", 0, 1, 0.15f);
    add ("gateDepth", "Gate Depth", 0, 1, 0);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "gateRate", 1 }, "Gate Rate",
                                                         StringArray { "1/8", "1/16", "1/32", "1/16T" }, 1));
    const bool defSteps[16] = { 1,0,1,1, 0,1,0,1, 1,0,0,1, 0,1,1,0 };
    for (int i = 0; i < 16; ++i)
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "gate" + String (i), 1 },
                                                           "Gate Step " + String (i + 1), defSteps[i]));
    add ("distDrive", "Distortion Drive", 0, 1, 0);
    add ("distTone", "Distortion Tone", 0, 1, 0.7f);
    add ("distMix", "Distortion Mix", 0, 1, 1.0f);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "distType", 1 }, "Distortion Type",
                                                         StringArray { "Tube", "Tape", "Fuzz", "Wavefold", "Bitcrush", "Blown Speaker" }, 0));
    add ("delayMix", "Delay Mix", 0, 1, 0.15f);
    add ("delayFeedback", "Delay Feedback", 0, 0.95f, 0.45f);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "delayTime", 1 }, "Delay Time",
                                                         StringArray { "1/4", "1/8.", "1/8", "1/16" }, 1));
    add ("reverbMix", "Reverb Mix", 0, 1, 0.35f);
    add ("reverbSize", "Reverb Size", 0, 1, 0.7f);
    add ("stingLevel", "Sting Level", 0, 1, 0.8f);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "stingType", 1 }, "Sting Type",
                                                         StringArray { "Slam", "Shriek Stabs", "Sub Boom", "Metal Crash" }, 0));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "stingZone", 1 }, "Sting Zone (C6+)", true));
    add ("outputDb", "Output", -36, 6, -6, -1, "dB");

    return { ps.begin(), ps.end() };
}

//==============================================================================
DreadProcessor::DreadProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DREAD", createLayout())
{
    for (int i = 0; i < 16; ++i) gatePtrs[(size_t) i] = apvts.getRawParameterValue ("gate" + juce::String (i));
    refreshUserPresets();
}

bool DreadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DreadProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void DreadProcessor::pullParams()
{
    auto& p = engine.params;
    p.droneLevel = get ("droneLevel"); p.droneDetune = get ("droneDetune"); p.droneFold = get ("droneFold"); p.droneSub = get ("droneSub");
    p.metalLevel = get ("metalLevel"); p.metalResonance = get ("metalResonance"); p.metalTone = get ("metalTone");
    p.airLevel = get ("airLevel"); p.airMotion = get ("airMotion");
    p.pianoLevel = get ("pianoLevel"); p.pianoBreak = get ("pianoBreak");
    p.whisperLevel = get ("whisperLevel"); p.whisperDemon = get ("whisperDemon");
    p.screamLevel = get ("screamLevel"); p.screamRasp = get ("screamRasp");
    p.cutoff = get ("cutoff"); p.resonance = get ("resonance"); p.envAmount = get ("envAmount");
    p.attack = get ("attack"); p.decay = get ("decay"); p.sustain = get ("sustain"); p.release = get ("release");
    p.dread = get ("dread"); p.torment = get ("torment");
    p.ringMix = get ("ringMix"); p.ringFreq = get ("ringFreq"); p.wobble = get ("wobble");
    p.gateDepth = get ("gateDepth"); p.gateRate = (int) get ("gateRate");
    for (int i = 0; i < 16; ++i) p.gateSteps[(size_t) i] = gatePtrs[(size_t) i]->load() > 0.5f;
    p.distDrive = get ("distDrive"); p.distTone = get ("distTone"); p.distMix = get ("distMix"); p.distType = (int) get ("distType");
    p.delayMix = get ("delayMix"); p.delayFeedback = get ("delayFeedback"); p.delayTime = (int) get ("delayTime");
    p.reverbMix = get ("reverbMix"); p.reverbSize = get ("reverbSize");
    p.chainLevel = get ("chainLevel"); p.chainRattle = get ("chainRattle"); p.chainRust = get ("chainRust");
    p.stingType = (int) get ("stingType");
    p.stingLevel = get ("stingLevel"); p.stingZone = get ("stingZone") > 0.5f;
    p.outputDb = get ("outputDb");
}

void DreadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    pullParams();

    // Merge on-screen keyboard
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    double bpm = 120.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }

    const int numSamples = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    juce::HeapBlock<float> monoR;
    if (R == nullptr) { monoR.allocate ((size_t) numSamples, true); R = monoR.get(); }

    const double samplesToBeats = bpm / 60.0 / getSampleRate();
    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end > pos)
        {
            const double q = ppq >= 0.0 ? ppq + pos * samplesToBeats : -1.0;
            engine.process (L + pos, R + pos, end - pos, bpm, q);
            pos = end;
        }
    };

    for (const auto meta : midi)
    {
        renderTo (std::min (meta.samplePosition, numSamples));
        const auto m = meta.getMessage();
        if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    renderTo (numSamples);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);
}

//==============================================================================
//==============================================================================
// Programs = factory presets followed by the user's saved presets.
int DreadProcessor::getNumFactoryPresets() const { return (int) presets().size(); }

juce::File DreadProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Castle Danger Music Company").getChildFile ("DREAD").getChildFile ("Presets");
}

juce::File DreadProcessor::fileForPreset (const juce::String& name) const
{
    return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".dreadpreset");
}

void DreadProcessor::refreshUserPresets()
{
    juce::StringArray names;
    for (const auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.dreadpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    const juce::ScopedLock sl (presetLock);
    userPresets = names;
}

juce::StringArray DreadProcessor::getUserPresetNames() const { const juce::ScopedLock sl (presetLock); return userPresets; }

bool DreadProcessor::userPresetExists (const juce::String& name) const { return fileForPreset (name).existsAsFile(); }

int DreadProcessor::getNumPrograms()
{
    const juce::ScopedLock sl (presetLock);
    return getNumFactoryPresets() + userPresets.size();
}

const juce::String DreadProcessor::getProgramName (int index)
{
    const int f = getNumFactoryPresets();
    if (juce::isPositiveAndBelow (index, f)) return presets()[(size_t) index].name;
    const juce::ScopedLock sl (presetLock);
    return userPresets[index - f];
}

void DreadProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    const int f = getNumFactoryPresets();

    if (index < f)
    {
        currentProgram = index;
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! rp->getParameterID().startsWith ("gate") || rp->getParameterID() == "gateDepth" || rp->getParameterID() == "gateRate")
                    rp->setValueNotifyingHost (rp->getDefaultValue());
        for (auto& [id, v] : presets()[(size_t) index].values)
            if (auto* rp = apvts.getParameter (id))
                rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        return;
    }

    // user preset: read every PARAM from the file and push it to the host-visible parameters
    const auto file = fileForPreset (getProgramName (index));
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return;
    currentProgram = index;
    const auto tree = juce::ValueTree::fromXml (*xml);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto child = tree.getChildWithProperty ("id", rp->getParameterID());
            const float v = child.isValid() ? (float) child.getProperty ("value") : rp->convertFrom0to1 (rp->getDefaultValue());
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        }
}

bool DreadProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return false;
    const auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;
    auto state = apvts.copyState();
    state.setProperty ("presetName", name, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (fileForPreset (name))) return false;
    refreshUserPresets();
    selectProgramByName (fileForPreset (name).getFileNameWithoutExtension());
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    return true;
}

bool DreadProcessor::deleteUserPreset (int index)
{
    if (! isUserPreset (index)) return false;
    const bool ok = fileForPreset (getProgramName (index)).deleteFile();
    refreshUserPresets();
    if (currentProgram >= getNumPrograms()) currentProgram = 0;
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withProgramChanged (true));
    return ok;
}

void DreadProcessor::selectProgramByName (const juce::String& name)
{
    for (int i = getNumPrograms(); --i >= 0;)
        if (getProgramName (i) == name) { currentProgram = i; return; }
}

void DreadProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("programName", getProgramName (currentProgram), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void DreadProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            const juce::String name = vt.getProperty ("programName", "");
            if (name.isNotEmpty()) selectProgramByName (name);   // user presets may have moved in the list
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* DreadProcessor::createEditor() { return new DreadEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DreadProcessor(); }
