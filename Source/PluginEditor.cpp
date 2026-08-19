#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // Pixel coordinates measured directly off Resources/pedalbg.png
    // (1074x976 native) - see HANDOFF.md, "0.1.1 pedal skin" for how these
    // were found (a grid overlay crop, not eyeballed blind) and the
    // caveat that they are a first pass, not verified against a live
    // screenshot (screen recording permission wasn't available in this
    // session - verified instead via an offline snapshot tool, see
    // tools/preview.cpp).
    constexpr int kFaderTravelTop = 500;    // y at max value
    constexpr int kFaderTravelBottom = 785; // y at min value
    constexpr int kFaderHalfWidth = 45;

    // The photo is 1074x976 native - too big to fit an 1920x1080 screen
    // comfortably alongside a DAW window. Everything (window size, fader
    // hit-boxes) is scaled down by this factor; the background image is
    // drawn scaled to match, so it stays one consistent picture rather
    // than a native-res crop.
    constexpr float kUiScale = 0.6f;

    struct FaderSlot { int xCenter; };

    // Order matches the photo left-to-right: Pitch, Formant, then the two
    // faders the user mapped to Drive and Mix (printed on the photo itself
    // as "Mix Balance" and "Reverb" - a mismatch with the mockup's own
    // placeholder text, not with the user's instructions, which are what
    // this follows).
    constexpr FaderSlot kPitchSlot   { 380 };
    constexpr FaderSlot kFormantSlot { 500 };
    constexpr FaderSlot kDriveSlot   { 650 };
    constexpr FaderSlot kMixSlot     { 858 };

    // Coordinates above are in native photo-pixel space; this returns the
    // scaled-down rect the slider actually gets, in editor-window space.
    juce::Rectangle<int> faderBounds (FaderSlot slot)
    {
        juce::Rectangle<int> native { slot.xCenter - kFaderHalfWidth, kFaderTravelTop,
                                       kFaderHalfWidth * 2, kFaderTravelBottom - kFaderTravelTop };
        return { juce::roundToInt (native.getX() * kUiScale), juce::roundToInt (native.getY() * kUiScale),
                 juce::roundToInt (native.getWidth() * kUiScale), juce::roundToInt (native.getHeight() * kUiScale) };
    }

    const juce::Colour kIndicatorPink { 0xffE8579F };
}

EmoBoyEditor::FaderOverlay::FaderOverlay()
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
}

void EmoBoyEditor::FaderOverlay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const double range = getMaximum() - getMinimum();
    const float proportion = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
    const float y = bounds.getBottom() - proportion * bounds.getHeight();

    g.setColour (kIndicatorPink);
    g.fillRect (bounds.getX(), y - 1.5f, bounds.getWidth(), 3.0f);
}

EmoBoyEditor::EmoBoyEditor (EmoBoyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::pedalbg_png, BinaryData::pedalbg_pngSize);

    for (auto* fader : { &pitchFader, &formantFader, &driveFader, &mixFader })
        addAndMakeVisible (fader);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::pitch, pitchFader);
    formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::formant, formantFader);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::drive, driveFader);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::mix, mixFader);

    const int nativeW = background.getWidth() > 0 ? background.getWidth() : 1074;
    const int nativeH = background.getHeight() > 0 ? background.getHeight() : 976;
    setSize (juce::roundToInt (nativeW * kUiScale), juce::roundToInt (nativeH * kUiScale));
}

void EmoBoyEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
}

void EmoBoyEditor::resized()
{
    pitchFader.setBounds (faderBounds (kPitchSlot));
    formantFader.setBounds (faderBounds (kFormantSlot));
    driveFader.setBounds (faderBounds (kDriveSlot));
    mixFader.setBounds (faderBounds (kMixSlot));
}
