#include "PluginEditor.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

//==============================================================================
namespace pal
{
const juce::Colour ironDark  { 0xff1d1815 };
const juce::Colour iron      { 0xff3a322c };
const juce::Colour rustDeep  { 0xff4a1d0c };
const juce::Colour rust      { 0xff9a4518 };
const juce::Colour rustLight { 0xffc0702a };
const juce::Colour paint     { 0xffd6c7a6 };   // dirty cream stencil paint
const juce::Colour paintDim  { 0xffa8997c };
const juce::Colour signal    { 0xffd2452a };   // rusty red for active things
const juce::Colour signalHot { 0xffff6a3d };
}

namespace tex
{
inline float hash (int x, int y, int seed)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffffu) / 16777215.0f;
}
inline float smooth (float t) { return t * t * (3.0f - 2.0f * t); }
inline float smoothstep (float a, float b, float x) { return smooth (juce::jlimit (0.0f, 1.0f, (x - a) / (b - a))); }
inline float vnoise (float x, float y, int seed)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed), c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
    const float u = smooth (fx), v = smooth (fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm (float x, float y, int seed, int octaves)
{
    float s = 0, amp = 0.5f, f = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) { s += amp * vnoise (x * f, y * f, seed + i * 31); norm += amp; f *= 2.03f; amp *= 0.5f; }
    return s / norm;
}
struct RGB { float r, g, b; };
inline RGB rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }
inline RGB mix (RGB a, RGB b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
inline RGB mul (RGB a, float k) { return { a.r * k, a.g * k, a.b * k }; }

// Rusty iron colour at logical position (x, y)
inline RGB rustAt (float x, float y, float rustBias, int seed)
{
    const float n      = fbm (x / 170.0f, y / 170.0f, seed, 4);
    const float detail = fbm (x / 36.0f, y / 36.0f, seed + 7, 3);
    const float fine   = vnoise (x / 7.0f, y / 7.0f, seed + 13);
    const float mask   = smoothstep (0.45f, 0.62f, n + (detail - 0.5f) * 0.35f + rustBias);
    RGB base = mix (rgb (pal::ironDark), rgb (pal::iron), detail);
    RGB r    = mix (rgb (pal::rustDeep), rgb (pal::rust), fbm (x / 20.0f, y / 20.0f, seed + 3, 2));
    if (fine > 0.72f) r = mix (r, rgb (pal::rustLight), (fine - 0.72f) * 2.5f);
    return mix (base, r, mask);
}

// Paint-chip a rendered image: knock out chunks of alpha with noise, plus grain.
inline void distress (juce::Image& img, float scale, float wear, int seed)
{
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
        for (int x = 0; x < bd.width; ++x)
        {
            auto c = bd.getPixelColour (x, y);
            if (c.getAlpha() == 0) continue;
            const float n = fbm ((float) x / (7.0f * scale), (float) y / (7.0f * scale), seed, 3);
            const float chip = n < wear ? 0.08f : 1.0f;
            const float grain = 0.7f + 0.3f * hash (x, y, seed + 99);
            bd.setPixelColour (x, y, c.withMultipliedAlpha (chip * grain));
        }
}

inline juce::Font stencil (float size) { return juce::Font (juce::FontOptions ("Impact", size, juce::Font::plain)); }
inline juce::Font plainBold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

inline juce::Image distressedText (const juce::String& text, int w, int h, juce::Font font, juce::Colour col,
                                   juce::Justification just, float scale, float wear, int seed, float kerning = 0.0f)
{
    juce::Image img (juce::Image::ARGB, juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setColour (col);
        g.setFont (font.withExtraKerningFactor (kerning));
        g.drawText (text, 0, 0, w, h, just, false);
    }
    distress (img, scale, wear, seed);
    return img;
}
} // namespace tex

//==============================================================================
DreadLookAndFeel::DreadLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, pal::paintDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, pal::paintDim);
    setColour (juce::ComboBox::textColourId, pal::paint);
    setColour (juce::ComboBox::arrowColourId, pal::signal);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::PopupMenu::textColourId, pal::paint);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::rust);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, pal::paint);
    setColour (juce::TextButton::textColourOffId, pal::paint);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::AlertWindow::textColourId, pal::paint);
    setColour (juce::AlertWindow::outlineColourId, pal::rust);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f0b09));
    setColour (juce::TextEditor::textColourId, pal::paint);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a3420));
    setColour (juce::TextEditor::focusedOutlineColourId, pal::rustLight);
    setColour (juce::TextEditor::highlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, pal::signalHot);

    // small rust texture used on every knob face
    rustDisc = juce::Image (juce::Image::ARGB, 128, 128, true);
    juce::Image::BitmapData bd (rustDisc, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const float n = tex::fbm ((float) x / 22.0f, (float) y / 22.0f, 501, 4);
            const float a = tex::smoothstep (0.5f, 0.7f, n);
            const auto c = tex::rustAt ((float) x * 2.0f, (float) y * 2.0f, 0.3f, 777);
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.r, c.g, c.b, a * 0.85f));
        }
}

juce::Font DreadLookAndFeel::getComboBoxFont (juce::ComboBox&) { return tex::plainBold (13.0f); }
juce::Font DreadLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }
juce::Font DreadLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return tex::stencil (juce::jmin (17.0f, (float) h * 0.62f)).withExtraKerningFactor (0.08f); }

void DreadLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (down ? pal::rustDeep : juce::Colour (0xff3a2a20), 0, r.getY(),
                                             down ? juce::Colour (0xff2a120a) : juce::Colour (0xff17110e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hi ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
}

void DreadLookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& w, const juce::Rectangle<int>& textArea, juce::TextLayout& layout)
{
    auto r = w.getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1a1411)); g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (pal::rustDeep.withAlpha (0.5f), 0, 0, juce::Colours::transparentBlack, 0, r.getHeight() * 0.6f, false));
    g.fillRect (r);
    g.setColour (pal::rust); g.drawRect (r, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRect (r.reduced (2.0f), 1.0f);
    for (auto p : { juce::Point<float> (8, 8), { r.getRight() - 8, 8 }, { 8, r.getBottom() - 8 }, { r.getRight() - 8, r.getBottom() - 8 } })
    {
        g.setColour (juce::Colour (0xff6a5a4c)); g.fillEllipse (p.x - 3, p.y - 3, 6, 6);
        g.setColour (pal::rust.withAlpha (0.6f)); g.drawEllipse (p.x - 3, p.y - 3, 6, 6, 1.0f);
    }
    g.setColour (pal::paint);
    layout.draw (g, textArea.toFloat());
}

void DreadLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (2.5f, r * 0.13f);

    // grimy track + rusty value arc
    juce::Path track; track.addCentredArc (c.x, c.y, r - thick, r - thick, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff120e0c)); g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3a2418)); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, r - thick, r - thick, 0, startAngle, angle, true);
    const auto arcCol = s.isMouseOverOrDragging() ? pal::signalHot : pal::signal;
    g.setColour (arcCol.withAlpha (0.25f));
    g.strokePath (val, juce::PathStrokeType (thick + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // worn iron knob body
    const float br = r - thick * 2.1f;
    if (br < 4.0f) return;
    const juce::Rectangle<float> body (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (body.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - br * 0.5f, c.y - br * 0.7f,
                                             juce::Colour (0xff17120f), c.x + br * 0.6f, c.y + br, true));
    g.fillEllipse (body);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        // offset the rust texture per knob so they don't all match
        const float off = (float) (((juce::uint64) (juce::pointer_sized_int) &s >> 4) % 64);
        g.setOpacity (0.9f);
        g.drawImage (rustDisc, body.expanded (br * 0.4f).translated (-off * 0.3f, off * 0.2f), juce::RectanglePlacement::stretchToFit);
    }
    // knurled rim
    g.setColour (juce::Colour (0xff0f0b09).withAlpha (0.8f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i / 28.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * br * 0.86f, c.y + std::sin (a) * br * 0.86f,
                    c.x + std::cos (a) * br, c.y + std::sin (a) * br, 1.0f);
    }
    g.setColour (juce::Colour (0xff0b0807)); g.drawEllipse (body, 1.2f);

    // chipped paint pointer
    juce::Path p; p.addRoundedRectangle (-1.6f, -br * 0.92f, 3.2f, br * 0.6f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x + 0.8f, c.y + 1.0f));
    g.setColour (pal::paint);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void DreadLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    if (b.getButtonText().isEmpty()) // gate step: a rusty indicator lamp
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 2.5f);
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (pal::signalHot, r.getCentreX(), r.getCentreY(), pal::rustDeep, r.getX(), r.getY(), true));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour (pal::signalHot.withAlpha (0.18f)); g.drawRoundedRectangle (r.expanded (1.5f), 3.0f, 2.0f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a221d), r.getX(), r.getY(), juce::Colour (0xff15110f), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.5f);
        }
        g.setColour (hi ? pal::rustLight : juce::Colour (0xff4a2c1c)); g.drawRoundedRectangle (r, 2.5f, 1.0f);
        return;
    }
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.0f);
    g.setColour (on ? pal::signal : juce::Colour (0xff1a1411)); g.fillRoundedRectangle (box, 2.0f);
    g.setColour (juce::Colour (0xff5a3420)); g.drawRoundedRectangle (box, 2.0f, 1.0f);
    if (on) { g.setColour (pal::paint); g.drawLine (box.getX() + 3, box.getCentreY(), box.getCentreX() - 1, box.getBottom() - 3, 2.0f);
              g.drawLine (box.getCentreX() - 1, box.getBottom() - 3, box.getRight() - 3, box.getY() + 3, 2.0f); }
    g.setColour (pal::paint); g.setFont (tex::plainBold (12.0f));
    g.drawText (b.getButtonText().toUpperCase(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

void DreadLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff221b17), 0, 0, juce::Colour (0xff120e0c), 0, (float) height, false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.isMouseOver() ? pal::rustLight : juce::Colour (0xff5a3420));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (pal::signal); g.fillPath (arrow);
}

//==============================================================================
DreadEditor::DreadEditor (DreadProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

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
    sec ("RUSTY CHAINS",   { makeKnob ("chainLevel", "Level"), makeKnob ("chainRattle", "Rattle"), makeKnob ("chainRust", "Rust") });
    sec ("TWISTED SYMPHONY", { makeKnob ("symphonyLevel", "Level"), makeKnob ("symphonyTwist", "Twist"), makeKnob ("symphonySection", "Section") });
    sec ("DEMENTED PIPE ORGAN", { makeKnob ("organLevel", "Level"), makeKnob ("organDement", "Dement"), makeKnob ("organStops", "Stops") });
    sec ("GHOST PIANO", { makeKnob ("ghostLevel", "Level"), makeKnob ("ghostBreak", "Break") });
    sec ("HEARTBEAT", { makeKnob ("heartLevel", "Level"), makeKnob ("heartRate", "Rate"), makeKnob ("heartPanic", "Panic") });

    proc.refreshUserPresets();
    rebuildPresetMenu();
    presetBox.onChange = [this] { presetMenuChanged(); };
    addAndMakeVisible (presetBox);
    saveButton.setTooltip ("Save the current settings as a new preset");
    saveButton.onClick = [this] { showSaveDialog(); };
    addAndMakeVisible (saveButton);

    gateRateBox.addItemList ({ "1/8", "1/16", "1/32", "1/16T" }, 1);
    delayTimeBox.addItemList ({ "1/4", "1/8.", "1/8", "1/16" }, 1);
    distTypeBox.addItemList ({ "Tube", "Tape", "Fuzz", "Wavefold", "Bitcrush", "Blown Speaker" }, 1);
    stingTypeBox.addItemList ({ "Slam", "Shriek Stabs", "Sub Boom", "Metal Crash" }, 1);
    addAndMakeVisible (gateRateBox); addAndMakeVisible (delayTimeBox); addAndMakeVisible (distTypeBox); addAndMakeVisible (stingTypeBox);
    stingTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "stingType", stingTypeBox);
    gateRateAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "gateRate", gateRateBox);
    delayTimeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "delayTime", delayTimeBox);
    distTypeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "distType", distTypeBox);

    for (int i = 0; i < 16; ++i)
    {
        addAndMakeVisible (steps[(size_t) i]);
        stepAtts[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "gate" + juce::String (i), steps[(size_t) i]);
    }
    addAndMakeVisible (stingZone);
    stingZoneAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "stingZone", stingZone);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (24);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffc9bc9f));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff171210));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff4a3a2c));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, pal::signal.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, pal::rust.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colour (0xff221b17));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, pal::paint);
    addAndMakeVisible (keyboard);

    setSize (1120, 860);
    startTimerHz (20);
}

DreadEditor::~DreadEditor() { dialog.reset(); setLookAndFeel (nullptr); }

//==============================================================================
void DreadEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const int f = proc.getNumFactoryPresets();
    presetBox.addSectionHeading ("FACTORY PRESETS");
    for (int i = 0; i < f; ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    const auto user = proc.getUserPresetNames();
    presetBox.addSeparator();
    presetBox.addSectionHeading ("USER PRESETS");
    if (user.isEmpty()) presetBox.addItem ("(none yet: click SAVE)", 8999), presetBox.setItemEnabled (8999, false);
    for (int i = 0; i < user.size(); ++i) presetBox.addItem (user[i], f + i + 1);
    presetBox.addSeparator();
    presetBox.addItem ("Delete current user preset...", kDeleteId);
    presetBox.setItemEnabled (kDeleteId, proc.isUserPreset (proc.getCurrentProgram()));
    presetBox.addItem ("Refresh preset list", kRefreshId);
    presetBox.addItem ("Show presets folder", kShowFolderId);
    lastPresetId = proc.getCurrentProgram() + 1;
    presetBox.setSelectedId (lastPresetId, juce::dontSendNotification);
}

void DreadEditor::presetMenuChanged()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0) return;
    if (id == kShowFolderId || id == kDeleteId || id == kRefreshId)
    {
        presetBox.setSelectedId (lastPresetId, juce::dontSendNotification);   // keep showing the real preset
        if (id == kShowFolderId)
        {
            auto folder = DreadProcessor::getUserPresetFolder();
            folder.createDirectory();
            folder.startAsProcess();
        }
        else if (id == kRefreshId)
        {
            proc.refreshUserPresets(); rebuildPresetMenu();
        }
        else
        {
            const int prog = proc.getCurrentProgram();
            const auto name = proc.getProgramName (prog);
            showConfirm ("Delete Preset", "Delete the user preset \"" + name + "\"?\nThis can't be undone.", "Delete",
                         [this, prog] { proc.deleteUserPreset (prog); rebuildPresetMenu(); });
        }
        return;
    }
    lastPresetId = id;
    proc.setCurrentProgram (id - 1);
    presetBox.setItemEnabled (kDeleteId, proc.isUserPreset (id - 1));
}

void DreadEditor::closeDialog()
{
    // defer deletion: we're usually inside the dialog's own button callback
    juce::Component::SafePointer<DreadEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });
}

void DreadEditor::showSaveDialog()
{
    if (dialog != nullptr) return;
    const auto cur = proc.getProgramName (proc.getCurrentProgram());
    dialog = std::make_unique<juce::AlertWindow> ("SAVE PRESET", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addTextEditor ("name", proc.isUserPreset (proc.getCurrentProgram()) ? cur : juce::String(), "Preset name");
    if (auto* te = dialog->getTextEditor ("name")) { te->setFont (tex::plainBold (15.0f)); te->setInputRestrictions (60); }
    dialog->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    addAndMakeVisible (*dialog);
    dialog->setCentrePosition (getLocalBounds().getCentre());
    juce::Component::SafePointer<DreadEditor> safe (this);
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr) return;
        const auto name = safe->dialog->getTextEditorContents ("name").trim();
        safe->closeDialog();
        if (result != 1 || name.isEmpty()) return;

        auto doSave = [safe, name]
        {
            if (safe == nullptr) return;
            if (! safe->proc.saveUserPreset (name))
                safe->showConfirm ("SAVE FAILED", "Couldn't write the preset file to:\n" + DreadProcessor::getUserPresetFolder().getFullPathName(), "OK", {});
            safe->rebuildPresetMenu();
        };
        if (safe->proc.userPresetExists (name))
            juce::MessageManager::callAsync ([safe, name, doSave] { if (safe != nullptr)
                safe->showConfirm ("PRESET EXISTS", "A preset named \"" + name + "\" already exists.\nReplace it?", "Replace", doSave); });
        else
            juce::MessageManager::callAsync (doSave);
    }), false);
    if (auto* te = dialog->getTextEditor ("name")) { te->grabKeyboardFocus(); te->selectAll(); }
}

void DreadEditor::showConfirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk)
{
    if (dialog != nullptr) return;
    dialog = std::make_unique<juce::AlertWindow> (title, message, juce::MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&lnf);
    dialog->addButton (okText.toUpperCase(), 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (onOk) dialog->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    addAndMakeVisible (*dialog);
    dialog->setCentrePosition (getLocalBounds().getCentre());
    juce::Component::SafePointer<DreadEditor> safe (this);
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe, onOk] (int result)
    {
        if (safe == nullptr) return;
        safe->closeDialog();
        if (result == 1 && onOk) juce::MessageManager::callAsync (onOk);   // runs after the dialog is gone
    }), false);
}

DreadEditor::Knob* DreadEditor::makeKnob (const juce::String& id, const juce::String& text, bool big)
{
    auto k = std::make_unique<Knob>();
    k->big = big;
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    k->slider.setColour (juce::Slider::textBoxTextColourId, pal::paintDim);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxHighlightColourId, pal::rust.withAlpha (0.5f));
    k->label.setText (text.toUpperCase(), juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (big ? tex::stencil (19.0f).withExtraKerningFactor (0.08f) : tex::plainBold (11.0f).withExtraKerningFactor (0.06f));
    k->label.setColour (juce::Label::textColourId, big ? pal::paint : pal::paintDim);
    addAndMakeVisible (k->slider); addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void DreadEditor::layoutSection (Section& s, juce::Rectangle<int> area)
{
    s.bounds = area;
    auto inner = area.reduced (10).withTrimmedTop (18);
    if (s.knobs.empty()) return;
    const int w = inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (k->big ? 22 : 15));
        k->slider.setBounds (cell);
    }
}

void DreadEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (60);
    logoArea  = header.withWidth (300);
    auto presetArea = header.removeFromRight (250);
    saveButton.setBounds (presetArea.removeFromRight (64).withSizeKeepingCentre (64, 28));
    presetArea.removeFromRight (6);
    presetBox.setBounds (presetArea.withSizeKeepingCentre (presetArea.getWidth(), 28));
    badgeArea = juce::Rectangle<int> (0, 0, 420, 52).withCentre ({ getWidth() / 2 - 10, header.getCentreY() });

    keyboard.setBounds (r.removeFromBottom (60));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);
    r.removeFromBottom (10);

    const int gap = 9;
    const int gateH = 120;
    auto rowGate = r.removeFromBottom (gateH); r.removeFromBottom (gap);
    const int rowH = (r.getHeight() - gap * 3) / 4;
    auto rowSrcA = r.removeFromTop (rowH); r.removeFromTop (gap);
    auto rowSrcB = r.removeFromTop (rowH); r.removeFromTop (gap);
    auto rowShape = r.removeFromTop (rowH); r.removeFromTop (gap);
    auto rowFx = r;

    auto splitRow = [gap] (juce::Rectangle<int> row, std::vector<float> weights)
    {
        std::vector<juce::Rectangle<int>> out; float total = 0; for (auto w : weights) total += w;
        const int avail = row.getWidth() - gap * (int) (weights.size() - 1);
        for (size_t i = 0; i < weights.size(); ++i)
        {
            out.push_back (i + 1 == weights.size() ? row : row.removeFromLeft ((int) ((float) avail * weights[i] / total)));
            row.removeFromLeft (gap);
        }
        return out;
    };
    // section indices: 0 drone, 1 metal, 2 air, 3 macros, 4 filter, 5 fx, 6 gate, 7 sting,
    //                  8 piano, 9 whispers, 10 screams, 11 distortion, 12 chains, 13 symphony, 14 organ, 15 ghost piano, 16 heartbeat

    // SOUND SOURCES
    // row A: drone, broken piano, twisted symphony, demented pipe organ
    auto a = splitRow (rowSrcA, { 4, 2.2f, 2.2f, 3, 3.3f });
    layoutSection (sections[0], a[0]); layoutSection (sections[8], a[1]); layoutSection (sections[15], a[2]);
    layoutSection (sections[13], a[3]); layoutSection (sections[14], a[4]);
    // row B: metal, air, demon whispers, screams, rusty chains, heartbeat
    auto b = splitRow (rowSrcB, { 2.9f, 2, 2.6f, 2, 2.9f, 2.9f });
    layoutSection (sections[1], b[0]); layoutSection (sections[2], b[1]); layoutSection (sections[9], b[2]);
    layoutSection (sections[10], b[3]); layoutSection (sections[12], b[4]); layoutSection (sections[16], b[5]);

    // SHAPING: filter/amp, distortion, macros
    auto c = splitRow (rowShape, { 7, 3.4f, 2.8f });
    layoutSection (sections[4], c[0]); layoutSection (sections[11], c[1]); layoutSection (sections[3], c[2]);

    // EFFECTS + sting/output
    auto d = splitRow (rowFx, { 7, 2.6f });
    layoutSection (sections[5], d[0]);
    auto st = d[1];
    sections[7].bounds = st;
    auto sInner = st.reduced (10).withTrimmedTop (18);
    stingZone.setBounds (sInner.removeFromBottom (20));
    const int kw = sInner.getWidth() / 2;
    for (auto* k : sections[7].knobs)
    {
        auto cell = sInner.removeFromLeft (kw);
        k->label.setBounds (cell.removeFromTop (15));
        k->slider.setBounds (cell);
    }

    // GATE: full width
    sections[6].bounds = rowGate;
    auto gInner = rowGate.reduced (10).withTrimmedTop (18);
    auto depthCell = gInner.removeFromLeft (80);
    sections[6].knobs[0]->label.setBounds (depthCell.removeFromTop (14));
    sections[6].knobs[0]->slider.setBounds (depthCell);
    gateRateBox.setBounds (gInner.removeFromLeft (96).withSizeKeepingCentre (80, 24));
    gInner.removeFromLeft (8);
    gateArea = gInner.withSizeKeepingCentre (gInner.getWidth(), std::min (52, gInner.getHeight()));
    const int sw = gateArea.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
        steps[(size_t) i].setBounds (gateArea.getX() + i * sw + ((i % 4 == 0) ? 4 : 0), gateArea.getY(), sw - 3, gateArea.getHeight());

    // header-row menus
    auto fx = sections[5].bounds;
    delayTimeBox.setBounds (fx.getRight() - 86, fx.getY() + 6, 74, 20);
    auto ds = sections[11].bounds;
    distTypeBox.setBounds (ds.getRight() - 120, ds.getY() + 6, 108, 20);
    stingTypeBox.setBounds (st.getRight() - 122, st.getY() + 6, 110, 20);

    buildBackdrop();
}

//==============================================================================
// Bakes the whole rusty backdrop (iron, rust, drips, riveted plates, stencils, badge) into one image.
void DreadEditor::buildBackdrop()
{
    const float scale = 2.0f;
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    const int PW = (int) ((float) W * scale), PH = (int) ((float) H * scale);
    juce::Image base (juce::Image::RGB, W, H, false);   // noise texture at 1x (it's grungy anyway)

    std::vector<juce::Rectangle<float>> plates;
    for (auto& s : sections) plates.push_back (s.bounds.toFloat());
    const auto badge = badgeArea.toFloat();
    const auto kb = keyboard.getBounds().toFloat().expanded (6.0f);

    {
        juce::Image::BitmapData bd (base, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < H; ++py)
        {
            const float y = (float) py;
            for (int px = 0; px < W; ++px)
            {
                const float x = (float) px;
                tex::RGB col;
                int inside = -1; float edge = 1e9f; bool shadow = false;
                for (int i = 0; i < (int) plates.size(); ++i)
                {
                    const auto& p = plates[(size_t) i];
                    if (p.contains (x, y))
                    {
                        inside = i;
                        edge = std::min ({ x - p.getX(), p.getRight() - x, y - p.getY(), p.getBottom() - y });
                        break;
                    }
                    if (p.translated (3.0f, 4.0f).contains (x, y)) shadow = true;
                }
                const bool inBadge = badge.contains (x, y);
                if (inBadge) edge = std::min ({ x - badge.getX(), badge.getRight() - x, y - badge.getY(), badge.getBottom() - y });

                if (inside >= 0 || inBadge)
                {
                    // dark steel plate, rust creeping in from the edges
                    const float d = tex::fbm (x / 30.0f, y / 30.0f, 211, 3);
                    tex::RGB steel = tex::mix (tex::rgb (juce::Colour (0xff1e1a17)), tex::rgb (juce::Colour (0xff332c27)), d);
                    const float edgeRust = (1.0f - juce::jlimit (0.0f, 1.0f, edge / 14.0f)) * tex::smoothstep (0.35f, 0.6f, tex::vnoise (x / 6.0f, y / 6.0f, 17));
                    const float bloom = tex::smoothstep (0.6f, 0.75f, tex::fbm (x / 55.0f, y / 55.0f, 404, 3)) * 0.7f;
                    col = tex::mix (steel, tex::rustAt (x, y, 0.15f, 1337), juce::jlimit (0.0f, 1.0f, edgeRust + bloom));
                    col = tex::mul (col, 0.9f);
                }
                else
                {
                    col = tex::rustAt (x, y, 0.08f, 42);
                    if (shadow) col = tex::mul (col, 0.45f);
                    if (kb.contains (x, y)) col = tex::mul (col, 0.35f);
                }

                // grime drips running down
                const float streak = tex::fbm (x / 9.0f, y / 240.0f, 909, 3);
                const float drip = tex::smoothstep (0.58f, 0.78f, streak) * (0.25f + 0.5f * y / (float) H);
                col = tex::mul (col, 1.0f - drip * 0.6f);
                // rust run-off below rivet lines / plate bottoms
                const float runoff = tex::smoothstep (0.66f, 0.82f, tex::fbm (x / 5.0f, y / 120.0f, 31337, 2));
                col = tex::mix (col, tex::rgb (pal::rustDeep), runoff * 0.35f);
                // pitting
                if (tex::hash (px, py, 5) > 0.988f) col = tex::mul (col, 0.45f);
                // overall grime + vignette
                const float grime = 0.72f + 0.28f * tex::fbm (x / 260.0f, y / 260.0f, 77, 2);
                const float dx = (x / (float) W - 0.5f) * 2.0f, dy = (y / (float) H - 0.5f) * 2.0f;
                const float vig = 1.0f - 0.45f * std::min (1.0f, (dx * dx + dy * dy) * 0.6f);
                col = tex::mul (col, grime * vig);
                bd.setPixelColour (px, py, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, col.r), juce::jlimit (0.0f, 1.0f, col.g),
                                                                        juce::jlimit (0.0f, 1.0f, col.b), 1.0f));
            }
        }
    }

    backdrop = juce::Image (juce::Image::RGB, PW, PH, false);
    juce::Graphics g (backdrop);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (base, 0, 0, PW, PH, 0, 0, W, H);
    g.addTransform (juce::AffineTransform::scale (scale));
    juce::Random rnd (1991);

    auto rivet = [&] (float cx, float cy, float rad)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - rad + 1, cy - rad + 1.5f, rad * 2, rad * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a68), cx - rad * 0.4f, cy - rad * 0.5f, juce::Colour (0xff2a201a), cx + rad, cy + rad, true));
        g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (cx - rad, cy - rad, rad * 2, rad * 2, 1.2f);
        g.setColour (pal::rustDeep.withAlpha (0.35f)); g.fillRect (cx - 0.8f, cy + rad, 1.6f, 3.0f + rnd.nextFloat() * 9.0f); // rust tear
    };

    auto plateFrame = [&] (juce::Rectangle<float> p, bool hot)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawLine (p.getX() + 2, p.getY() + 1, p.getRight() - 2, p.getY() + 1, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (p, 1.0f);
        if (hot) { g.setColour (pal::signal.withAlpha (0.5f)); g.drawRect (p.reduced (2.0f), 1.5f); }
        rivet (p.getX() + 6, p.getY() + 6, 3.0f); rivet (p.getRight() - 6, p.getY() + 6, 3.0f);
        rivet (p.getX() + 6, p.getBottom() - 6, 3.0f); rivet (p.getRight() - 6, p.getBottom() - 6, 3.0f);
    };

    // scratches over everything
    for (int i = 0; i < 260; ++i)
    {
        const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
        const float len = 6.0f + rnd.nextFloat() * 70.0f, ang = rnd.nextFloat() * juce::MathConstants<float>::pi;
        g.setColour ((rnd.nextBool() ? juce::Colour (0xffd0c0a8) : juce::Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.1f));
        g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.5f + rnd.nextFloat() * 0.6f);
    }

    int seed = 10;
    for (auto& s : sections)
    {
        const auto p = s.bounds.toFloat();
        const bool hot = s.title == "MACROS";
        plateFrame (p, hot);
        auto title = tex::distressedText (s.title, s.bounds.getWidth() - 40, 18, tex::stencil (15.0f), hot ? pal::signalHot : pal::paint,
                                          juce::Justification::centredLeft, scale, 0.2f, seed++, 0.1f);
        g.drawImage (title, juce::Rectangle<float> (p.getX() + 14, p.getY() + 3, (float) s.bounds.getWidth() - 40, 18.0f));
    }

    // keyboard bay frame
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawRect (kb, 2.0f);
    rivet (kb.getX() - 4, kb.getCentreY(), 3.5f); rivet (kb.getRight() + 4, kb.getCentreY(), 3.5f);

    // CASTLE DANGER MC badge
    plateFrame (badge, false);
    g.setColour (pal::paint.withAlpha (0.25f)); g.drawRect (badge.reduced (5.0f), 1.0f);
    auto brand = tex::distressedText ("CASTLE DANGER MUSIC COMPANY", badgeArea.getWidth() - 30, badgeArea.getHeight() - 8, tex::stencil (25.0f),
                                      juce::Colour (0xffeadfc4), juce::Justification::centred, scale, 0.24f, 4242, 0.14f);
    g.setColour (juce::Colours::black);
    g.setOpacity (0.7f); g.drawImage (brand, badge.reduced (15.0f, 4.0f).translated (1.5f, 2.0f));
    g.setOpacity (1.0f); g.drawImage (brand, badge.reduced (15.0f, 4.0f));

    // stamped version / subtitle under the logo
    auto sub = tex::distressedText ("HORROR TEXTURE DESIGNER  //  V" + juce::String (JucePlugin_VersionString),
                                    300, 14, tex::plainBold (10.0f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 99, 0.1f);
    g.drawImage (sub, juce::Rectangle<float> ((float) logoArea.getX() + 4, (float) logoArea.getBottom() - 14, 300.0f, 14.0f));

    // logo is drawn live (it flickers) from its own distressed image
    logoImage = tex::distressedText ("DREAD", 260, 50, tex::stencil (52.0f), pal::signal, juce::Justification::centredLeft,
                                     scale, 0.3f, 666, 0.22f);
}

void DreadEditor::paint (juce::Graphics& g)
{
    if (backdrop.isValid()) g.drawImage (backdrop, getLocalBounds().toFloat());
    else g.fillAll (pal::ironDark);

    const float dreadAmt = proc.apvts.getRawParameterValue ("dread")->load();
    const float alpha = juce::jlimit (0.3f, 1.0f, 1.0f - flicker * dreadAmt * 0.7f);
    if (logoImage.isValid())
    {
        const auto la = juce::Rectangle<float> ((float) logoArea.getX() + 4, (float) logoArea.getY() - 2, 260.0f, 50.0f);
        g.setColour (juce::Colours::black);
        g.setOpacity (0.6f);  g.drawImage (logoImage, la.translated (2.0f, 3.0f));
        g.setOpacity (alpha); g.drawImage (logoImage, la);
        g.setOpacity (1.0f);
    }
}

void DreadEditor::timerCallback()
{
    const float d = proc.apvts.getRawParameterValue ("dread")->load();
    auto& rnd = juce::Random::getSystemRandom();
    const float before = flicker;
    flicker = (rnd.nextFloat() < 0.05f + d * 0.25f) ? rnd.nextFloat() : flicker * 0.5f;
    if (dialog == nullptr && ! presetBox.isPopupActive() && presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
    {
        if (presetBox.indexOfItemId (proc.getCurrentProgram() + 1) < 0) rebuildPresetMenu();
        lastPresetId = proc.getCurrentProgram() + 1;
        presetBox.setSelectedId (lastPresetId, juce::dontSendNotification);
        presetBox.setItemEnabled (kDeleteId, proc.isUserPreset (proc.getCurrentProgram()));
    }
    if (std::abs (flicker - before) > 0.01f) repaint (logoArea.withHeight (54).withWidth (270));
}
