#include "PluginEditor.h"

namespace colours
{
const juce::Colour bg       { 0xff0b0909 };
const juce::Colour panel    { 0xff161111 };
const juce::Colour edge     { 0xff3a1d1d };
const juce::Colour blood    { 0xffb3141c };
const juce::Colour bloodHot { 0xffff3b30 };
const juce::Colour bone     { 0xffd8cfc4 };
const juce::Colour ash      { 0xff7d6f6a };
}

//==============================================================================
DreadLookAndFeel::DreadLookAndFeel()
{
    using namespace colours;
    setColour (juce::Slider::textBoxTextColourId, bone);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, ash);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::textColourId, bone);
    setColour (juce::ComboBox::arrowColourId, blood);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, bone);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, blood);
    setColour (juce::ToggleButton::textColourId, bone);
}

void DreadLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = r * 0.14f;

    // track + value arc
    juce::Path track; track.addCentredArc (c.x, c.y, r - thick, r - thick, 0, startAngle, endAngle, true);
    g.setColour (edge); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, r - thick, r - thick, 0, startAngle, angle, true);
    g.setColour (s.isMouseOverOrDragging() ? bloodHot : blood);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    const float br = r - thick * 2.2f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2222), c.x, c.y - br, juce::Colour (0xff0e0b0b), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (edge); g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

    // pointer (a scratch mark)
    juce::Path p; p.addRoundedRectangle (-1.5f, -br * 0.95f, 3.0f, br * 0.55f, 1.5f);
    g.setColour (bone); g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void DreadLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    if (b.getButtonText().isEmpty()) // gate step
    {
        g.setColour (b.getToggleState() ? blood : panel.brighter (0.05f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (hi ? bloodHot : edge); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        return;
    }
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.0f);
    g.setColour (b.getToggleState() ? blood : panel); g.fillRoundedRectangle (box, 3.0f);
    g.setColour (edge); g.drawRoundedRectangle (box, 3.0f, 1.0f);
    g.setColour (bone); g.setFont (13.0f);
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

//==============================================================================
DreadEditor::DreadEditor (DreadProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    auto sec = [&] (const juce::String& title, std::vector<Knob*> ks) { sections.push_back ({ title, std::move (ks), {} }); };
    sec ("DRONE",  { makeKnob ("droneLevel", "Level"), makeKnob ("droneDetune", "Detune"), makeKnob ("droneFold", "Fold"), makeKnob ("droneSub", "Sub") });
    sec ("METAL",  { makeKnob ("metalLevel", "Level"), makeKnob ("metalResonance", "Bow"), makeKnob ("metalTone", "Tone") });
    sec ("AIR",    { makeKnob ("airLevel", "Level"), makeKnob ("airMotion", "Motion") });
    sec ("MACROS", { makeKnob ("dread", "DREAD", true), makeKnob ("torment", "TORMENT", true) });
    sec ("FILTER / AMP", { makeKnob ("cutoff", "Cutoff"), makeKnob ("resonance", "Reso"), makeKnob ("envAmount", "Env"),
                           makeKnob ("attack", "Attack"), makeKnob ("decay", "Decay"), makeKnob ("sustain", "Sustain"), makeKnob ("release", "Release") });
    sec ("FX", { makeKnob ("ringMix", "Ring"), makeKnob ("ringFreq", "Ring Hz"), makeKnob ("wobble", "Wobble"),
                 makeKnob ("delayMix", "Delay"), makeKnob ("delayFeedback", "Feedback"), makeKnob ("reverbMix", "Reverb"), makeKnob ("reverbSize", "Size") });
    sec ("GATE",   { makeKnob ("gateDepth", "Depth") });
    sec ("STING",  { makeKnob ("stingLevel", "Level"), makeKnob ("outputDb", "Output") });
    sec ("BROKEN PIANO",   { makeKnob ("pianoLevel", "Level"), makeKnob ("pianoBreak", "Break") });
    sec ("DEMON WHISPERS", { makeKnob ("whisperLevel", "Level"), makeKnob ("whisperDemon", "Demon") });
    sec ("SCREAMS",        { makeKnob ("screamLevel", "Level"), makeKnob ("screamRasp", "Rasp") });
    sec ("DISTORTION",     { makeKnob ("distDrive", "Drive"), makeKnob ("distTone", "Tone"), makeKnob ("distMix", "Mix") });

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    gateRateBox.addItemList ({ "1/8", "1/16", "1/32", "1/16T" }, 1);
    delayTimeBox.addItemList ({ "1/4", "1/8.", "1/8", "1/16" }, 1);
    distTypeBox.addItemList ({ "Tube", "Tape", "Fuzz", "Wavefold", "Bitcrush", "Blown Speaker" }, 1);
    addAndMakeVisible (gateRateBox); addAndMakeVisible (delayTimeBox); addAndMakeVisible (distTypeBox);
    distTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "distType", distTypeBox);
    gateRateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "gateRate", gateRateBox);
    delayTimeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "delayTime", delayTimeBox);

    for (int i = 0; i < 16; ++i)
    {
        addAndMakeVisible (steps[(size_t) i]);
        stepAtts[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "gate" + juce::String (i), steps[(size_t) i]);
    }
    addAndMakeVisible (stingZone);
    stingZoneAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "stingZone", stingZone);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (24);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::blood);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::blood.withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    setSize (1040, 800);
    startTimerHz (20);
}

DreadEditor::~DreadEditor() { setLookAndFeel (nullptr); }

DreadEditor::Knob* DreadEditor::makeKnob (const juce::String& id, const juce::String& text, bool big)
{
    auto k = std::make_unique<Knob>();
    k->big = big;
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->slider.setPopupDisplayEnabled (false, false, this);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::Font (big ? 15.0f : 12.0f, big ? juce::Font::bold : juce::Font::plain));
    if (big) k->label.setColour (juce::Label::textColourId, colours::bone);
    addAndMakeVisible (k->slider); addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void DreadEditor::layoutSection (Section& s, juce::Rectangle<int> area)
{
    s.bounds = area;
    auto inner = area.reduced (8).withTrimmedTop (18);
    if (s.knobs.empty()) return;
    const int w = inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

void DreadEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto header = r.removeFromTop (48);
    presetBox.setBounds (header.removeFromRight (260).withSizeKeepingCentre (260, 28));

    keyboard.setBounds (r.removeFromBottom (70));
    r.removeFromBottom (8);

    const int rowH = (r.getHeight() - 24) / 4;
    auto row1 = r.removeFromTop (rowH); r.removeFromTop (8);
    auto rowSrc = r.removeFromTop (rowH); r.removeFromTop (8);
    auto row2 = r.removeFromTop (rowH); r.removeFromTop (8);
    auto row3 = r;
    const int gap = 8;

    auto splitRow = [gap] (juce::Rectangle<int> row, std::vector<float> weights)
    {
        std::vector<juce::Rectangle<int>> out; float total = 0; for (auto w : weights) total += w;
        const int avail = row.getWidth() - gap * (int) (weights.size() - 1);
        for (size_t i = 0; i < weights.size(); ++i)
        {
            out.push_back (row.removeFromLeft ((int) (avail * weights[i] / total)));
            row.removeFromLeft (gap);
        }
        return out;
    };

    auto a = splitRow (row1, { 4, 3, 2, 3 });
    for (int i = 0; i < 4; ++i) layoutSection (sections[(size_t) i], a[(size_t) i]);
    auto src = splitRow (rowSrc, { 2, 2, 2, 3 });
    for (int i = 0; i < 4; ++i) layoutSection (sections[(size_t) (8 + i)], src[(size_t) i]);
    auto ds = sections[11].bounds;
    distTypeBox.setBounds (ds.getRight() - 128, ds.getY() + 4, 120, 20);
    auto b = splitRow (row2, { 1, 1 });
    layoutSection (sections[4], b[0]); layoutSection (sections[5], b[1]);
    auto c = splitRow (row3, { 7, 2 });

    // Gate: depth knob + rate + 16 steps
    auto gate = c[0];
    sections[6].bounds = gate;
    auto gInner = gate.reduced (8).withTrimmedTop (18);
    auto depthCell = gInner.removeFromLeft (80);
    sections[6].knobs[0]->label.setBounds (depthCell.removeFromTop (16));
    sections[6].knobs[0]->slider.setBounds (depthCell);
    auto rateCol = gInner.removeFromLeft (90).withSizeKeepingCentre (80, 26);
    gateRateBox.setBounds (rateCol);
    gInner.removeFromLeft (8);
    gateArea = gInner.withSizeKeepingCentre (gInner.getWidth(), std::min (60, gInner.getHeight()));
    const int sw = gateArea.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i].setBounds (gateArea.getX() + i * sw + ((i % 4 == 0) ? 3 : 0), gateArea.getY(), sw - 2, gateArea.getHeight());

    // Sting / output
    auto st = c[1];
    sections[7].bounds = st;
    auto sInner = st.reduced (8).withTrimmedTop (18);
    stingZone.setBounds (sInner.removeFromBottom (24));
    const int kw = sInner.getWidth() / 2;
    for (auto* k : sections[7].knobs)
    {
        auto cell = sInner.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }

    // delay time combo sits in the FX header
    auto fx = sections[5].bounds;
    delayTimeBox.setBounds (fx.getRight() - 84, fx.getY() + 4, 76, 20);
}

void DreadEditor::paint (juce::Graphics& g)
{
    using namespace colours;
    g.fillAll (bg);

    // faint vignette
    auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, full.getCentreX(), full.getCentreY(),
                                             juce::Colour (0xaa000000), 0, 0, true));
    g.fillRect (full);

    // Title — flickers with the DREAD amount
    const float dreadAmt = proc.apvts.getRawParameterValue ("dread")->load();
    const float alpha = juce::jlimit (0.35f, 1.0f, 1.0f - flicker * dreadAmt * 0.6f);
    g.setColour (blood.withAlpha (alpha));
    g.setFont (juce::Font (34.0f, juce::Font::bold));
    g.drawText ("D R E A D", 16, 10, 300, 40, juce::Justification::centredLeft);
    g.setColour (ash); g.setFont (12.0f);
    g.drawText ("horror texture designer  v" JucePlugin_VersionString, 210, 22, 260, 20, juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        auto rb = s.bounds.toFloat();
        g.setColour (panel); g.fillRoundedRectangle (rb, 6.0f);
        g.setColour (s.title == "MACROS" ? blood.withAlpha (0.7f) : edge); g.drawRoundedRectangle (rb, 6.0f, 1.0f);
        g.setColour (s.title == "MACROS" ? bloodHot : ash);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.reduced (10, 4).removeFromTop (16), juce::Justification::centredLeft);
    }
}

void DreadEditor::timerCallback()
{
    // random flicker for the logo, more likely as DREAD rises
    const float d = proc.apvts.getRawParameterValue ("dread")->load();
    auto& rnd = juce::Random::getSystemRandom();
    flicker = (rnd.nextFloat() < 0.05f + d * 0.25f) ? rnd.nextFloat() : flicker * 0.5f;
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    repaint (0, 0, 480, 60);
}
