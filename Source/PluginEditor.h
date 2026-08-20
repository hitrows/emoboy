#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// 0.1.3 pedal skin: user-supplied clean background (no text labels, no cap
// graphics - pics/bg-clean.png) plus a real fader-cap sprite (pics/faders.png,
// one cap cropped out and reused for all 4) that actually slides along the
// fader's travel, instead of the 0.1.1/0.1.2 thin-line placeholder. Only the
// 4 continuous parameters implemented so far (Pitch/Formant/Drive/Mix) are
// exposed - no labels anywhere, by explicit request. Link/Quantize aren't
// reachable from this UI at all yet.
//
// Double-clicking any fader opens a small numeric entry box (added 0.1.4) -
// the only "text on the panel" that exists, and only while actively typing.
//
// 0.1.5 adds the Robot Note knob and the Robot toggle button/backlight -
// Mode is now reachable from the UI (toggles Robot <-> Transpose), still
// with no Quantize access.
// ---------------------------------------------------------------------------
class EmoBoyEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit EmoBoyEditor (EmoBoyProcessor&);
    ~EmoBoyEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A juce::Slider with all default LookAndFeel drawing suppressed -
    // paint() is fully overridden, so only the cap sprite this class draws
    // itself ever appears. Mouse/keyboard interaction (drag, scroll,
    // keyboard nudge) is unchanged, inherited from Slider as normal;
    // double-click is hooked via onDoubleClick so the owning editor can
    // pop up a shared text-entry box sized wider than this narrow strip.
    class FaderOverlay : public juce::Slider
    {
    public:
        explicit FaderOverlay (const juce::Image& capImage);
        void paint (juce::Graphics&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        std::function<void()> onDoubleClick;

    private:
        const juce::Image& cap;
    };

    // The Robot Note pot - a rotary Slider with default LookAndFeel drawing
    // suppressed the same way; paint() draws only a thin tick at the value's
    // angle, in the muted tone matching the fader caps' own inlaid stripe
    // (chosen over the brighter branding pink - user's call, 2026-08-20).
    class RobotKnob : public juce::Slider
    {
    public:
        RobotKnob();
        void paint (juce::Graphics&) override;
    };

    // The ROBOT footswitch. Its unlit look is baked into the background
    // photo; this just overlays the user-supplied pre-blurred/expanded
    // glow sprite (pics/"light transp.png", cropped to just this button)
    // when Mode == Robot, and toggles Mode <-> Transpose on click.
    class RobotButton : public juce::Button
    {
    public:
        RobotButton();
        void setGlowImage (const juce::Image& glowImage);
        void setLit (bool shouldBeLit);

    private:
        void paintButton (juce::Graphics&, bool, bool) override;
        juce::Image glow;
        bool lit = false;
    };

    void timerCallback() override; // polls Mode to keep the backlight in sync (incl. host automation/state loads, not just clicks here)

    EmoBoyProcessor& proc;
    juce::Image background;
    juce::Image capImage;

    FaderOverlay pitchFader, formantFader, driveFader, mixFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment, formantAttachment, driveAttachment, mixAttachment;

    RobotKnob robotKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> robotNoteAttachment;

    RobotButton robotButton;

    // Created on demand by beginTextEntry(), destroyed once the value is
    // committed - not a permanent fixture, so it never counts as a label
    // sitting on the panel.
    std::unique_ptr<juce::Label> valueEditor;
    void beginTextEntry (FaderOverlay& fader);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoyEditor)
};
