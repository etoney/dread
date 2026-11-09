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
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;

private:
    juce::Image rustDisc;
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
    void buildBackdrop();
    void rebuildPresetMenu();
    void presetMenuChanged();
    void showSaveDialog();
    void showConfirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk);
    void closeDialog();
    void timerCallback() override;

    DreadProcessor& proc;
    DreadLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::TextButton saveButton { "SAVE" };
    juce::TextButton hellButton { "RAISE HELL" };
    juce::Label hellLabel;
    float hellFade = 0.0f;
    std::unique_ptr<juce::AlertWindow> dialog;
    int lastPresetId = 1;
    static constexpr int kShowFolderId = 9001, kDeleteId = 9002, kRefreshId = 9003;
    juce::ComboBox presetBox, gateRateBox, delayTimeBox, distTypeBox, stingTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gateRateAtt, delayTimeAtt, distTypeAtt, stingTypeAtt;
    std::array<juce::ToggleButton, 16> steps;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 16> stepAtts;
    juce::ToggleButton stingZone { "Sting zone C6+" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stingZoneAtt;
    juce::Rectangle<int> gateArea, logoArea, badgeArea;
    juce::MidiKeyboardComponent keyboard;
    juce::Image backdrop, logoImage;
    float flicker = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreadEditor)
};
