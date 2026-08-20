#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <array>

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
// with no Quantize access. Also adds the BYPASS footswitch (hard bypass,
// wired to getBypassParameter()).
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
    // Double-click opens a note-picker popup (onDoubleClick), same pattern
    // as FaderOverlay's text entry.
    class RobotKnob : public juce::Slider
    {
    public:
        RobotKnob();
        void paint (juce::Graphics&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;

        std::function<void()> onDoubleClick;
    };

    // A footswitch-style button whose unlit look is baked into the
    // background photo; this just overlays a pre-blurred/expanded glow
    // sprite (cropped from pics/"light transp.png") when told to, and
    // reports clicks via juce::Button::onClick as normal. Reused for both
    // the ROBOT and BYPASS footswitches - only the glow image, position,
    // and click handler differ between them.
    class GlowToggleButton : public juce::Button
    {
    public:
        GlowToggleButton();
        void setGlowImage (const juce::Image& glowImage);
        void setLit (bool shouldBeLit);

    private:
        void paintButton (juce::Graphics&, bool, bool) override;
        juce::Image glow;
        bool lit = false;
    };

    void timerCallback() override; // polls Mode/Bypass to keep both backlights in sync (incl. host automation/state loads, not just clicks here)

    EmoBoyProcessor& proc;
    juce::Image background;
    juce::Image capImage;

    FaderOverlay pitchFader, formantFader, driveFader, mixFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment, formantAttachment, driveAttachment, mixAttachment;

    RobotKnob robotKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> robotNoteAttachment;

    GlowToggleButton robotButton;
    GlowToggleButton bypassButton;

    // Mode select footswitches, bottom row: "1"=Transpose, "2"=Quantize,
    // "3"=Robot (2026-08-20). Unlike ROBOT/BYPASS these only glow along
    // their bottom edge in the source art, not a full box outline -
    // doesn't change the code, just which crop gets used.
    std::array<GlowToggleButton, 3> modeButtons;

    // The HITROWS wordmark's lit look - purely a status indicator (no
    // click handler, clicks pass through), lit whenever the plugin is
    // *not* bypassed. Reuses GlowToggleButton for the image-overlay
    // machinery even though nothing here is actually a button.
    GlowToggleButton hitrowsGlow;

    // Standard (unstyled) juce::AlertWindow text-entry dialog - user's ask
    // (2026-08-20): a native-feeling system dialog, not a custom popup
    // drawn on the panel.
    void beginTextEntry (FaderOverlay& fader);

    // Standard (unstyled) juce::PopupMenu note picker for the Robot Note
    // knob - user's ask, deliberately not themed to match the panel.
    void showNotePicker();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoyEditor)
};
