#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // Fader travel, measured off the user's own guide (pics/lines.png): a
    // red line (cap's pink stripe position at max value), a yellow line
    // (middle - matches the cap sprite's own resting position almost
    // exactly, off by under a pixel), and a green line (min value). Same
    // guide, same 3 numbers, for all 4 faders - not per-fader.
    constexpr int kFaderTravelTop = 504;    // red line - value = max
    constexpr int kFaderTravelBottom = 768; // green line - value = min

    // Cap sprite geometry, measured off pics/faders.png via its alpha
    // channel bounding box (not eyeballed) - see HANDOFF.md.
    constexpr int kCapNativeWidth = 66;
    constexpr int kCapNativeHeight = 106;
    constexpr int kFaderHalfWidth = 40; // hit-box half-width, a bit wider than the cap for an easier grab

    // Same 0.6x scale as the 0.1.1/0.1.2 skin - fits an 1920x1080 screen
    // next to a DAW window.
    constexpr float kUiScale = 0.6f;

    struct FaderSlot { int xCenter; };

    // Cap x-centres, measured off pics/faders.png (alpha bounding boxes of
    // the 4 sprite copies) - these superseded the earlier eyeballed values
    // from 0.1.1/0.1.2, which were off by up to ~30px on the last two.
    constexpr FaderSlot kPitchSlot   { 375 };
    constexpr FaderSlot kFormantSlot { 501 };
    constexpr FaderSlot kDriveSlot   { 626 };
    constexpr FaderSlot kMixSlot     { 829 };

    // The slider's hit-box is padded above/below the pure travel range by
    // half the cap's height, so the cap sprite never gets clipped against
    // the component's own bounds when it's drawn at the very top or
    // bottom of its travel (Component::paint is clipped to local bounds).
    juce::Rectangle<int> faderBounds (FaderSlot slot)
    {
        const int capHalfHeight = kCapNativeHeight / 2;
        juce::Rectangle<int> native { slot.xCenter - kFaderHalfWidth, kFaderTravelTop - capHalfHeight,
                                       kFaderHalfWidth * 2, (kFaderTravelBottom - kFaderTravelTop) + kCapNativeHeight };
        return { juce::roundToInt (native.getX() * kUiScale), juce::roundToInt (native.getY() * kUiScale),
                 juce::roundToInt (native.getWidth() * kUiScale), juce::roundToInt (native.getHeight() * kUiScale) };
    }
}

EmoBoyEditor::FaderOverlay::FaderOverlay (const juce::Image& capImageIn)
    : cap (capImageIn)
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
}

void EmoBoyEditor::FaderOverlay::paint (juce::Graphics& g)
{
    if (! cap.isValid())
        return;

    const auto bounds = getLocalBounds().toFloat();
    const double range = getMaximum() - getMinimum();
    const float proportion = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;

    const float capW = kCapNativeWidth * kUiScale;
    const float capH = kCapNativeHeight * kUiScale;
    const float travelHeight = bounds.getHeight() - capH; // pure travel, padding excluded

    // proportion = 1 (max) -> cap centre at the top of the travel (padding
    // below it); proportion = 0 (min) -> cap centre at the bottom.
    const float centreY = capH * 0.5f + (1.0f - proportion) * travelHeight;
    const float centreX = bounds.getWidth() * 0.5f;

    g.drawImage (cap, { centreX - capW * 0.5f, centreY - capH * 0.5f, capW, capH });
}

void EmoBoyEditor::FaderOverlay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick();
}

EmoBoyEditor::EmoBoyEditor (EmoBoyProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      background (juce::ImageCache::getFromMemory (BinaryData::pedalbg_png, BinaryData::pedalbg_pngSize)),
      capImage (juce::ImageCache::getFromMemory (BinaryData::fadercap_png, BinaryData::fadercap_pngSize)),
      pitchFader (capImage), formantFader (capImage), driveFader (capImage), mixFader (capImage)
{
    for (auto* fader : { &pitchFader, &formantFader, &driveFader, &mixFader })
        addAndMakeVisible (fader);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::pitch, pitchFader);
    formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::formant, formantFader);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::drive, driveFader);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::mix, mixFader);

    pitchFader.onDoubleClick = [this] { beginTextEntry (pitchFader); };
    formantFader.onDoubleClick = [this] { beginTextEntry (formantFader); };
    driveFader.onDoubleClick = [this] { beginTextEntry (driveFader); };
    mixFader.onDoubleClick = [this] { beginTextEntry (mixFader); };

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

void EmoBoyEditor::beginTextEntry (FaderOverlay& fader)
{
    // One shared editor, created on demand and torn down once committed -
    // this is the only text that ever appears on the panel, and only for
    // as long as someone is actively typing a value in.
    valueEditor = std::make_unique<juce::Label>();
    auto* editor = valueEditor.get();

    editor->setText (juce::String (fader.getValue(), 2), juce::dontSendNotification);
    editor->setJustificationType (juce::Justification::centred);
    editor->setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.85f));
    editor->setColour (juce::Label::textColourId, juce::Colour (0xffe8579f));
    editor->setColour (juce::Label::outlineColourId, juce::Colour (0xffe8579f));
    editor->setColour (juce::TextEditor::highlightColourId, juce::Colour (0xffe8579f).withAlpha (0.4f));

    constexpr int w = 72, h = 22;
    editor->setBounds (fader.getBounds().getCentreX() - w / 2, fader.getBounds().getCentreY() - h / 2, w, h);
    addAndMakeVisible (editor);

    editor->onEditorHide = [this, &fader]
    {
        if (valueEditor != nullptr)
        {
            const float typed = valueEditor->getText().retainCharacters ("0123456789.-").getFloatValue();
            fader.setValue (juce::jlimit ((float) fader.getMinimum(), (float) fader.getMaximum(), typed),
                             juce::sendNotificationSync);
            removeChildComponent (valueEditor.get());
            valueEditor.reset();
        }
    };

    editor->setEditable (true, true, false);
    editor->showEditor();
    if (auto* ed = editor->getCurrentTextEditor())
        ed->selectAll();
}

void EmoBoyEditor::resized()
{
    pitchFader.setBounds (faderBounds (kPitchSlot));
    formantFader.setBounds (faderBounds (kFormantSlot));
    driveFader.setBounds (faderBounds (kDriveSlot));
    mixFader.setBounds (faderBounds (kMixSlot));
}
