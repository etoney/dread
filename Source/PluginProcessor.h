#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DreadEngine.h"
#include "RaiseHell.h"

class DreadProcessor : public juce::AudioProcessor
{
public:
    DreadProcessor();
    ~DreadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DREAD"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- user presets (saved as files in Documents/Castle Danger Music Company/DREAD/Presets)
    static juce::File getUserPresetFolder();
    int getNumFactoryPresets() const;
    juce::StringArray getUserPresetNames() const;
    void refreshUserPresets();
    bool userPresetExists (const juce::String& name) const;
    bool saveUserPreset (const juce::String& name);      // saves current settings, selects it
    bool deleteUserPreset (int programIndex);
    bool isUserPreset (int programIndex) const { return programIndex >= getNumFactoryPresets(); }

    // RAISE HELL: re-roll every effect (randomly, but within musical rules). Returns the character name.
    juce::String raiseHell();

private:
    int lastHellCharacter = -1;
public:
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void pullParams();
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    dread::Engine engine;
    std::array<std::atomic<float>*, 16> gatePtrs {};
    int currentProgram = 0;
    juce::StringArray userPresets;              // sorted names, guarded by presetLock
    mutable juce::CriticalSection presetLock;
    juce::File fileForPreset (const juce::String& name) const;
    void selectProgramByName (const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreadProcessor)
};
