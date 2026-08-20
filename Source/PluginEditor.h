#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// 0.1.3 pedal skin: user-supplied clean background (no text labels, no cap
// graphics - pics/bg-clean.png) plus a real fader-cap sprite (pics/faders.png,
// one cap cropped out and reused for all 4) that actually slides along the
// fader's travel, instead of the 0.1.1/0.1.2 thin-line placeholder. Only the
// 4 continuous parameters implemented so far (Pitch/Formant/Drive/Mix) are
// exposed - no labels anywhere, by explicit request. Link/Mode/Robot Note
// still exist and work at their defaults, just not shown here.
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
    // paint() is fully overridden, so only the cap sprite this class draws
    // itself ever appears. Mouse/keyboard interaction is unchanged,
    // inherited from Slider as normal.
    class FaderOverlay : public juce::Slider
    {
    public:
        explicit FaderOverlay (const juce::Image& capImage);
        void paint (juce::Graphics&) override;

    private:
        const juce::Image& cap;
    };

    EmoBoyProcessor& proc;
    juce::Image background;
    juce::Image capImage;

    FaderOverlay pitchFader, formantFader, driveFader, mixFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment, formantAttachment, driveAttachment, mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoyEditor)
};
