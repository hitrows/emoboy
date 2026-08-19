#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ---------------------------------------------------------------------------
// Plain JUCE UI - sliders/comboboxes/toggles grouped into sections, no
// custom look-and-feel or artwork. Explicitly out of scope for this demo
// per the brief; the point is to audition the DSP and parameter set before
// investing in real UI/branding.
// ---------------------------------------------------------------------------
class EmoBoyEditor : public juce::AudioProcessorEditor
{
public:
    explicit EmoBoyEditor (EmoBoyProcessor&);
    ~EmoBoyEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    // One labelled rotary slider bound to an APVTS parameter.
    struct KnobRow
    {
        juce::Label label;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    struct ToggleRow
    {
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    struct ComboRow
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    EmoBoyProcessor& proc;

    juce::Viewport viewport;
    juce::Component content;

    KnobRow& addKnob (const juce::String& paramId, const juce::String& labelText);
    ComboRow& addCombo (const juce::String& paramId, const juce::String& labelText);
    ToggleRow& addToggle (const juce::String& paramId, const juce::String& labelText);

    std::vector<std::unique_ptr<KnobRow>> knobs;
    std::vector<std::unique_ptr<ComboRow>> combos;
    std::vector<std::unique_ptr<ToggleRow>> toggles;
    std::vector<std::unique_ptr<juce::GroupComponent>> groups;

    void buildMainSection();

#if EMOBOY_NERD_FEATURES
    // Mod1/Mod2/AM/PT + the routing matrix - "EmoBoy Nerd" territory,
    // compiled out of the plain build. See PluginProcessor.h/.cpp and
    // Parameters.h/.cpp for the same gate; HANDOFF.md for why.
    std::vector<std::unique_ptr<juce::Label>> freeLabels;

    // Routing matrix widgets: [source][target].
    struct RouteCell
    {
        juce::ToggleButton on;
        juce::Slider depth { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<APVTS::ButtonAttachment> onAttachment;
        std::unique_ptr<APVTS::SliderAttachment> depthAttachment;
    };
    std::array<std::array<std::unique_ptr<RouteCell>, ModMatrix::numTargets>, ModMatrix::numSources> routeCells;

    void buildModSection();
    void buildRoutingSection();
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoyEditor)
};
