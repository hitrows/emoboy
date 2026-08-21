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

    // HITROWS wordmark glow sprite bounds. Originally cropped from the
    // shared "light transp.png" sheet (0.1.8); the user split it into its
    // own dedicated pics/hitrows.png afterwards and reverted the shared
    // sheet, so this crop comes from that separate file now - same
    // measurement approach (alpha bounding box, cross-checked visually as
    // a composite over bg-clean.png before use), different source file.
    constexpr int kHitrowsGlowX = 130, kHitrowsGlowY = 130, kHitrowsGlowW = 310, kHitrowsGlowH = 110;
    juce::Rectangle<int> hitrowsGlowBounds() { return scaledNativeRect ({ kHitrowsGlowX, kHitrowsGlowY, kHitrowsGlowW, kHitrowsGlowH }); }

    // Mode footswitches (bottom row, "1"/"2"/"3" = Transpose/Quantize/
    // Robot), bottom-edge-only glow crops - bounds measured off each
    // sprite's alpha channel, cross-checked visually as a composite over
    // bg-clean.png (shown to the user for approval) before use.
    constexpr int kModeGlowY = 343, kModeGlowH = 69;
    constexpr int kModeGlowX[3] = { 300, 425, 555 };
    constexpr int kModeGlowW[3] = { 140, 145, 140 };
    juce::Rectangle<int> modeGlowBounds (int i) { return scaledNativeRect ({ kModeGlowX[i], kModeGlowY, kModeGlowW[i], kModeGlowH }); }

    // PEAK lamp glow bounds, from the user's dedicated pics/lamp.png -
    // alpha bounding box (98,199)-(139,240), cropped with a little padding
    // (85,186)-(152,253) and cross-checked visually before use, same as
    // every other glow sprite.
    constexpr int kLampGlowX = 85, kLampGlowY = 186, kLampGlowW = 67, kLampGlowH = 67;
    juce::Rectangle<int> lampGlowBounds() { return scaledNativeRect ({ kLampGlowX, kLampGlowY, kLampGlowW, kLampGlowH }); }

    // Preset footswitches (top row, "1"/"2"/"3"/"4" - full box-outline
    // glow, unlike the bottom row's under-glow-only style). Went through
    // two corrections: 0.1.13 used the glow sprite's own alpha bounding
    // box (over-captured blur bleeding into the gaps - way oversized for
    // 2/3/4). 0.1.14's first fix measured the actual button frames but
    // used symmetric ~10px padding on both sides, which was enough to eat
    // the (only ~10-15px wide) gaps between buttons and made 1/2/3 bleed
    // into their right-hand neighbour - confirmed by the user testing live
    // in Logic, not something visible in an isolated per-button composite.
    // This pass pulls back specifically the *right* edge of 1/2/3 (button
    // 4 has nothing to its right, and was already correct); button 1
    // reverted to its original 0.1.13 bounds exactly, which were never
    // the problem. Checked as one combined composite of the whole row
    // (all four "lit" at once) rather than four isolated crops, so any
    // overlap between neighbours is actually visible before use.
    constexpr int kPresetGlowY = 200, kPresetGlowH = 86;
    constexpr int kPresetGlowX[4] = { 299, 438, 563, 688 };
    constexpr int kPresetGlowW[4] = { 140, 122, 120, 135 }; // button 2 widened slightly (+7px) after the row-composite fix undershot it a touch
    juce::Rectangle<int> presetGlowBounds (int i) { return scaledNativeRect ({ kPresetGlowX[i], kPresetGlowY, kPresetGlowW[i], kPresetGlowH }); }

    const juce::Colour kKnobTickColour { 190, 120, 150 }; // matches the fader caps' own inlaid stripe tone - user's pick over the brighter branding pink
}

// Four presets for the top-row footswitches (2026-08-20, user's ask: "4
// killer presets"). Named after the panel's own graffiti art (VOID/CRY/
// LOST are printed on the pedal; ROBOT exercises the mode the pink
// footswitch/knob are for) even though no name is ever shown in this
// no-labels build - just for orientation here and in conversation.
// Not tuned against real vocal material by ear (can't) - a reasonable
// first pass per character, easy to retune once actually heard.
const std::array<EmoBoyEditor::Preset, 4> EmoBoyEditor::presets { {
    // pitch, formant, drive,  mix,   mode,                    robotNoteIndex
    { -3.0f,  -3.0f,   15.0f, 100.0f, Param::Mode::Transpose,  12 }, // "Cry"   - subtle sad pitch-down double
    { -12.0f, -9.0f,   65.0f, 100.0f, Param::Mode::Transpose,  12 }, // "Void"  - full octave down, heavy drive, monster voice
    {  5.0f,   6.0f,   10.0f,  80.0f, Param::Mode::Transpose,  12 }, // "Lost"  - airy pitched-up, mostly wet
    {  0.0f,   2.0f,   35.0f, 100.0f, Param::Mode::Robot,      12 }, // "Robot" - flattened to middle C, moderate drive
} };

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

    // Status indicator only now (2026-08-20) - lit whenever Mode==Robot,
    // same as before, but no click handler: mode button "3" (bottom row)
    // is the actual control now, and having two controls toggle the same
    // parameter was flagged as a duplicate-functionality glitch by the
    // user. setInterceptsMouseClicks(false, false) so it doesn't even
    // show a pressable hover/down state for a click that would do nothing.
    robotButton.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::robotglow_png, BinaryData::robotglow_pngSize));
    robotButton.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (robotButton);

    bypassButton.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::bypassglow_png, BinaryData::bypassglow_pngSize));
    addAndMakeVisible (bypassButton);
    bypassButton.onClick = [this]
    {
        auto* bypassParam = proc.apvts.getParameter (Param::bypass);
        bypassParam->setValueNotifyingHost (bypassParam->getValue() > 0.5f ? 0.0f : 1.0f);
    };

    hitrowsGlow.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::hitrowsglow_png, BinaryData::hitrowsglow_pngSize));
    hitrowsGlow.setInterceptsMouseClicks (false, false); // status indicator only, not a control
    addAndMakeVisible (hitrowsGlow);

    {
        static const struct { const char* data; int size; } modeGlowData[3] = {
            { BinaryData::mode1glow_png, BinaryData::mode1glow_pngSize },
            { BinaryData::mode2glow_png, BinaryData::mode2glow_pngSize },
            { BinaryData::mode3glow_png, BinaryData::mode3glow_pngSize },
        };
        static const Param::Mode modeForButton[3] = { Param::Mode::Transpose, Param::Mode::Quantize, Param::Mode::Robot };

        for (int i = 0; i < 3; ++i)
        {
            auto& button = modeButtons[(size_t) i];
            button.setGlowImage (juce::ImageCache::getFromMemory (modeGlowData[i].data, modeGlowData[i].size));
            addAndMakeVisible (button);
            const auto targetMode = modeForButton[i];
            button.onClick = [this, targetMode]
            {
                auto* modeParam = proc.apvts.getParameter (Param::mode);
                modeParam->setValueNotifyingHost (modeParam->convertTo0to1 ((float) (int) targetMode));
            };
        }
    }

    peakLamp.setGlowImage (juce::ImageCache::getFromMemory (BinaryData::lampglow_png, BinaryData::lampglow_pngSize));
    peakLamp.setInterceptsMouseClicks (false, false); // status indicator only, not a control
    addAndMakeVisible (peakLamp);

    {
        static const struct { const char* data; int size; } presetGlowData[4] = {
            { BinaryData::preset1glow_png, BinaryData::preset1glow_pngSize },
            { BinaryData::preset2glow_png, BinaryData::preset2glow_pngSize },
            { BinaryData::preset3glow_png, BinaryData::preset3glow_pngSize },
            { BinaryData::preset4glow_png, BinaryData::preset4glow_pngSize },
        };
        for (int i = 0; i < 4; ++i)
        {
            auto& button = presetButtons[(size_t) i];
            button.setGlowImage (juce::ImageCache::getFromMemory (presetGlowData[i].data, presetGlowData[i].size));
            addAndMakeVisible (button);
            button.onClick = [this, i] { applyPreset (i); };
        }
    }

    startTimerHz (30);

    const int nativeW = background.getWidth() > 0 ? background.getWidth() : 1074;
    const int nativeH = background.getHeight() > 0 ? background.getHeight() : 976;
    setSize (juce::roundToInt (nativeW * kUiScale), juce::roundToInt (nativeH * kUiScale));
}

void EmoBoyEditor::timerCallback()
{
    const int currentMode = (int) proc.apvts.getRawParameterValue (Param::mode)->load();
    const bool isRobot = currentMode == (int) Param::Mode::Robot;
    robotButton.setLit (isRobot);

    static const Param::Mode modeForButton[3] = { Param::Mode::Transpose, Param::Mode::Quantize, Param::Mode::Robot };
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setLit (currentMode == (int) modeForButton[i]);

    const bool isBypassed = proc.apvts.getRawParameterValue (Param::bypass)->load() > 0.5f;
    bypassButton.setLit (isBypassed);
    hitrowsGlow.setLit (! isBypassed);

    peakLamp.setLit (proc.isPeakLedOn());
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

void EmoBoyEditor::applyPreset (int index)
{
    const auto& p = presets[(size_t) index];

    auto setNorm = [this] (const juce::String& id, float actualValue)
    {
        if (auto* param = proc.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (actualValue));
    };

    setNorm (Param::pitch, p.pitch);
    setNorm (Param::formant, p.formant);
    setNorm (Param::drive, p.drive);
    setNorm (Param::mix, p.mix);
    setNorm (Param::mode, (float) (int) p.mode);
    setNorm (Param::robotNote, (float) p.robotNoteIndex);

    // "Lit" here just means "most recently clicked" - presets aren't a
    // stored parameter, so there's nothing to poll in timerCallback the
    // way Mode/Bypass are synced. Set directly, once, here.
    for (int i = 0; i < 4; ++i)
        presetButtons[(size_t) i].setLit (i == index);
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
    hitrowsGlow.setBounds (hitrowsGlowBounds());

    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setBounds (modeGlowBounds (i));

    peakLamp.setBounds (lampGlowBounds());

    for (int i = 0; i < 4; ++i)
        presetButtons[(size_t) i].setBounds (presetGlowBounds (i));
}
