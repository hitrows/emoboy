#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// 0.1.1 placeholder skin: the user's hardware-pedal mockup photo as a static
// background, with only the 4 continuous parameters actually implemented so
// far (Pitch/Formant/Drive/Mix) made draggable over their fader positions in
// the photo. No thumb/track artwork is drawn - just a thin pink line at the
// current value, per the user's explicit ask ("полоска на месте бегунка").
// Link/Mode/Robot Note are not exposed here at all; this build is only for
// evaluating the 4 fader-mapped parameters. Real layered design (separate
// cap/track art, the button grid, etc.) comes later - see HANDOFF.md.
// ---------------------------------------------------------------------------
class EmoBoyEditor : public juce::AudioProcessorEditor
{
public:
    explicit EmoBoyEditor (EmoBoyProcessor&);
    ~EmoBoyEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A juce::Slider with all default LookAndFeel drawing suppressed -
    // paint() is fully overridden, so only the thin pink indicator line
    // this class draws itself ever appears. Mouse/keyboard interaction is
    // unchanged, inherited from Slider as normal.
    class FaderOverlay : public juce::Slider
    {
    public:
        FaderOverlay();
        void paint (juce::Graphics&) override;
    };

    EmoBoyProcessor& proc;
    juce::Image background;

    FaderOverlay pitchFader, formantFader, driveFader, mixFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment, formantAttachment, driveAttachment, mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoyEditor)
};
