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
    juce::Rectangle<int> scaledNativeRect (juce::Rectangle<int> native)
    {
        return { juce::roundToInt (native.getX() * kUiScale), juce::roundToInt (native.getY() * kUiScale),
                 juce::roundToInt (native.getWidth() * kUiScale), juce::roundToInt (native.getHeight() * kUiScale) };
    }

    juce::Rectangle<int> faderBounds (FaderSlot slot)
    {
        const int capHalfHeight = kCapNativeHeight / 2;
        return scaledNativeRect ({ slot.xCenter - kFaderHalfWidth, kFaderTravelTop - capHalfHeight,
                                    kFaderHalfWidth * 2, (kFaderTravelBottom - kFaderTravelTop) + kCapNativeHeight });
    }

    // Robot Note knob - centre/radius measured off pics/knob.png's yellow
    // guide circle (alpha bounding box), cross-checked visually against
    // where the knob's face actually sits in pics/bg-clean.png.
    constexpr int kKnobCentreX = 194;
    constexpr int kKnobCentreY = 506;
    constexpr int kKnobRadius = 28;
    juce::Rectangle<int> knobBounds() { return scaledNativeRect ({ kKnobCentreX - kKnobRadius, kKnobCentreY - kKnobRadius, kKnobRadius * 2, kKnobRadius * 2 }); }

    // Standard ~300 degree pot sweep, measured in the clockwise-from-12
    // convention: 7 o'clock (-150deg) to 5 o'clock (+150deg), through 12.
    // 2026-08-20, confirmed with the user: clockwise = higher note, so
    // 7 o'clock/minimum-rotation = C (index 0), 5 o'clock/maximum = B
    // (index 23) - i.e. no inversion needed, this is JUCE's own default
    // rotary-slider angle convention.
    constexpr float kKnobStartAngle = -2.6180f;
    constexpr float kKnobEndAngle = 2.6180f;

    // Glow sprite bounds within pics/"light transp.png" (cropped into
    // Resources/robotglow.png with a padding margin so its soft falloff
    // isn't clipped) - measured off the sprite's own alpha channel, not
    // eyeballed. Doubles as the button's clickable area: a bit larger
    // than the plain button rect underneath, which only makes it easier
    // to hit, and doesn't overlap the knob above it.
    constexpr int kRobotGlowX = 120, kRobotGlowY = 575, kRobotGlowW = 180, kRobotGlowH = 120;
    juce::Rectangle<int> robotGlowBounds() { return scaledNativeRect ({ kRobotGlowX, kRobotGlowY, kRobotGlowW, kRobotGlowH }); }

    const juce::Colour kKnobTickColour { 190, 120, 150 }; // matches the fader caps' own inlaid stripe tone - user's pick over the brighter branding pink
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

EmoBoyEditor::RobotKnob::RobotKnob()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
}

void EmoBoyEditor::RobotKnob::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const double range = getMaximum() - getMinimum();
    const float proportion = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;
    const float angle = kKnobStartAngle + proportion * (kKnobEndAngle - kKnobStartAngle);

    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f;
    const float rIn = radius * 0.30f;
    const float rOut = radius * 0.85f;

    juce::Point<float> p1 (centre.x + rIn * std::sin (angle), centre.y - rIn * std::cos (angle));
    juce::Point<float> p2 (centre.x + rOut * std::sin (angle), centre.y - rOut * std::cos (angle));

    g.setColour (kKnobTickColour);
    g.drawLine ({ p1, p2 }, juce::jmax (2.0f, radius * 0.09f));
}

EmoBoyEditor::RobotButton::RobotButton() : juce::Button ({}) {}

void EmoBoyEditor::RobotButton::setGlowImage (const juce::Image& glowImage) { glow = glowImage; }

void EmoBoyEditor::RobotButton::setLit (bool shouldBeLit)
{
    if (lit != shouldBeLit)
    {
        lit = shouldBeLit;
        repaint();
    }
}

void EmoBoyEditor::RobotButton::paintButton (juce::Graphics& g, bool, bool)
{
    if (lit && glow.isValid())
        g.drawImage (glow, getLocalBounds().toFloat());
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

    addAndMakeVisible (robotKnob);
    robotNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Param::robotNote, robotKnob);

    robotButton.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::robotglow_png, BinaryData::robotglow_pngSize));
    addAndMakeVisible (robotButton);
    robotButton.onClick = [this]
    {
        auto* modeParam = proc.apvts.getParameter (Param::mode);
        const bool isRobot = (int) std::round (modeParam->convertFrom0to1 (modeParam->getValue())) == (int) Param::Mode::Robot;
        const auto newMode = isRobot ? Param::Mode::Transpose : Param::Mode::Robot;
        modeParam->setValueNotifyingHost (modeParam->convertTo0to1 ((float) (int) newMode));
    };

    startTimerHz (30);

    const int nativeW = background.getWidth() > 0 ? background.getWidth() : 1074;
    const int nativeH = background.getHeight() > 0 ? background.getHeight() : 976;
    setSize (juce::roundToInt (nativeW * kUiScale), juce::roundToInt (nativeH * kUiScale));
}

void EmoBoyEditor::timerCallback()
{
    const bool isRobot = (int) proc.apvts.getRawParameterValue (Param::mode)->load() == (int) Param::Mode::Robot;
    robotButton.setLit (isRobot);
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

    robotKnob.setBounds (knobBounds());
    robotButton.setBounds (robotGlowBounds());
}
