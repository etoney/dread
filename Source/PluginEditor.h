#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class DreadLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DreadLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class DreadEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DreadEditor (DreadProcessor&);
    ~DreadEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        bool big = false;
    };
    struct Section { juce::String title; std::vector<Knob*> knobs; juce::Rectangle<int> bounds; };

    Knob* makeKnob (const juce::String& id, const juce::String& text, bool big = false);
    void layoutSection (Section&, juce::Rectangle<int>);
    void timerCallback() override;

    DreadProcessor& proc;
    DreadLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::ComboBox presetBox, gateRateBox, delayTimeBox, distTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gateRateAtt, delayTimeAtt, distTypeAtt;
    std::array<juce::ToggleButton, 16> steps;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 16> stepAtts;
    juce::ToggleButton stingZone { "Sting zone C6+" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stingZoneAtt;
    juce::Rectangle<int> gateArea;
    juce::MidiKeyboardComponent keyboard;
    float flicker = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreadEditor)
};
