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

    // Robot Note angle mapping, reworked 2026-08-20: 12 o'clock (angle 0)
    // is the reference note (index 12 in the 24-entry list - "C2" in
    // Logic Pro's own octave numbering, see Parameters.cpp), not one end
    // of the sweep. Clockwise raises the note a semitone at a time up to
    // the top of the list at 5 o'clock (+150deg, 11 steps); counter-
    // clockwise lowers it down to the bottom at 7 o'clock (-150deg, 12
    // steps) - the long way through 12, same ~300 degree sweep as before.
    // The two halves get very slightly different angular steps (150/11 vs
    // 150/12, ~9% apart) because the reference note isn't exactly centred
    // in the 24-note range - a consequence of the user's explicit
    // endpoints, not a bug.
    constexpr int kRobotNoteCentreIndex = 12; // "C2" (Logic numbering)
    constexpr int kRobotNoteMaxIndex = 23;    // "B2" (Logic numbering)
    constexpr float kKnobSweepHalf = 2.6180f; // 150 degrees, one side

    float angleForRobotNoteIndex (int index)
    {
        const int offset = index - kRobotNoteCentreIndex;
        if (offset >= 0)
            return kKnobSweepHalf * ((float) offset / (float) (kRobotNoteMaxIndex - kRobotNoteCentreIndex));
        return -kKnobSweepHalf * ((float) -offset / (float) kRobotNoteCentreIndex);
    }

    // Glow sprite bounds within pics/"light transp.png" (cropped into
    // Resources/robotglow.png with a padding margin so its soft falloff
    // isn't clipped) - measured off the sprite's own alpha channel, not
    // eyeballed. Doubles as the button's clickable area: a bit larger
    // than the plain button rect underneath, which only makes it easier
    // to hit, and doesn't overlap the knob above it.
    constexpr int kRobotGlowX = 120, kRobotGlowY = 575, kRobotGlowW = 180, kRobotGlowH = 120;
    juce::Rectangle<int> robotGlowBounds() { return scaledNativeRect ({ kRobotGlowX, kRobotGlowY, kRobotGlowW, kRobotGlowH }); }

    // BYPASS footswitch glow sprite bounds, same measurement approach as
    // the Robot glow above (alpha bounding box of the cropped sprite,
    // padded so the falloff isn't clipped).
    constexpr int kBypassGlowX = 815, kBypassGlowY = 315, kBypassGlowW = 180, kBypassGlowH = 110;
    juce::Rectangle<int> bypassGlowBounds() { return scaledNativeRect ({ kBypassGlowX, kBypassGlowY, kBypassGlowW, kBypassGlowH }); }

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
    const float angle = angleForRobotNoteIndex ((int) std::lround (getValue()));

    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f;
    const float rIn = radius * 0.30f;
    const float rOut = radius * 0.85f;

    juce::Point<float> p1 (centre.x + rIn * std::sin (angle), centre.y - rIn * std::cos (angle));
    juce::Point<float> p2 (centre.x + rOut * std::sin (angle), centre.y - rOut * std::cos (angle));

    g.setColour (kKnobTickColour);
    g.drawLine ({ p1, p2 }, juce::jmax (2.0f, radius * 0.09f));
}

void EmoBoyEditor::RobotKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick();
}

EmoBoyEditor::GlowToggleButton::GlowToggleButton() : juce::Button ({}) {}

void EmoBoyEditor::GlowToggleButton::setGlowImage (const juce::Image& glowImage) { glow = glowImage; }

void EmoBoyEditor::GlowToggleButton::setLit (bool shouldBeLit)
{
    if (lit != shouldBeLit)
    {
        lit = shouldBeLit;
        repaint();
    }
}

void EmoBoyEditor::GlowToggleButton::paintButton (juce::Graphics& g, bool, bool)
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
    robotKnob.onDoubleClick = [this] { showNotePicker(); };

    robotButton.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::robotglow_png, BinaryData::robotglow_pngSize));
    addAndMakeVisible (robotButton);
    robotButton.onClick = [this]
    {
        auto* modeParam = proc.apvts.getParameter (Param::mode);
        const bool isRobot = (int) std::round (modeParam->convertFrom0to1 (modeParam->getValue())) == (int) Param::Mode::Robot;
        const auto newMode = isRobot ? Param::Mode::Transpose : Param::Mode::Robot;
        modeParam->setValueNotifyingHost (modeParam->convertTo0to1 ((float) (int) newMode));
    };

    bypassButton.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::bypassglow_png, BinaryData::bypassglow_pngSize));
    addAndMakeVisible (bypassButton);
    bypassButton.onClick = [this]
    {
        auto* bypassParam = proc.apvts.getParameter (Param::bypass);
        bypassParam->setValueNotifyingHost (bypassParam->getValue() > 0.5f ? 0.0f : 1.0f);
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

    const bool isBypassed = proc.apvts.getRawParameterValue (Param::bypass)->load() > 0.5f;
    bypassButton.setLit (isBypassed);
}

void EmoBoyEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
}

void EmoBoyEditor::beginTextEntry (FaderOverlay& fader)
{
    auto* aw = new juce::AlertWindow ("Enter value", {}, juce::AlertWindow::NoIcon);
    aw->addTextEditor ("value", juce::String (fader.getValue(), 2));
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (auto* ed = aw->getTextEditor ("value"))
        ed->selectAll();

    juce::Component::SafePointer<EmoBoyEditor> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis, &fader, aw] (int result)
        {
            if (safeThis == nullptr)
                return;
            if (result == 1)
            {
                const float typed = aw->getTextEditorContents ("value").getFloatValue();
                fader.setValue (juce::jlimit ((float) fader.getMinimum(), (float) fader.getMaximum(), typed),
                                 juce::sendNotificationSync);
            }
        }), true); // deleteWhenDismissed
}

void EmoBoyEditor::showNotePicker()
{
    auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (Param::robotNote));
    if (choiceParam == nullptr)
        return;

    const int currentIndex = choiceParam->getIndex();

    juce::PopupMenu menu; // deliberately the plain, unstyled default look - user's ask
    for (int i = 0; i < choiceParam->choices.size(); ++i)
        menu.addItem (i + 1, choiceParam->choices[i], true, i == currentIndex);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (robotKnob),
        [this, choiceParam] (int result)
        {
            if (result > 0)
                choiceParam->setValueNotifyingHost (choiceParam->convertTo0to1 ((float) (result - 1)));
        });
}

void EmoBoyEditor::resized()
{
    pitchFader.setBounds (faderBounds (kPitchSlot));
    formantFader.setBounds (faderBounds (kFormantSlot));
    driveFader.setBounds (faderBounds (kDriveSlot));
    mixFader.setBounds (faderBounds (kMixSlot));

    robotKnob.setBounds (knobBounds());
    robotButton.setBounds (robotGlowBounds());
    bypassButton.setBounds (bypassGlowBounds());
}
