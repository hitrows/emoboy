#include "PluginEditor.h"

namespace
{
    constexpr int kKnobSize = 72;
    constexpr int kRowHeight = 96;
    constexpr int kMargin = 10;
}

EmoBoyEditor::KnobRow& EmoBoyEditor::addKnob (const juce::String& paramId, const juce::String& labelText)
{
    auto row = std::make_unique<KnobRow>();
    row->label.setText (labelText, juce::dontSendNotification);
    row->label.setJustificationType (juce::Justification::centred);
    row->slider.setTextValueSuffix ("");
    content.addAndMakeVisible (row->label);
    content.addAndMakeVisible (row->slider);
    row->attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, row->slider);
    knobs.push_back (std::move (row));
    return *knobs.back();
}

EmoBoyEditor::ComboRow& EmoBoyEditor::addCombo (const juce::String& paramId, const juce::String& labelText)
{
    auto row = std::make_unique<ComboRow>();
    row->label.setText (labelText, juce::dontSendNotification);
    row->label.setJustificationType (juce::Justification::centred);
    content.addAndMakeVisible (row->label);
    content.addAndMakeVisible (row->box);

    // ComboBoxAttachment only keeps selection in sync - it does not
    // populate the dropdown itself, so without this the box shows
    // "(no choices)" forever even though the parameter has them.
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        row->box.addItemList (choiceParam->choices, 1);

    row->attachment = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, paramId, row->box);
    combos.push_back (std::move (row));
    return *combos.back();
}

EmoBoyEditor::ToggleRow& EmoBoyEditor::addToggle (const juce::String& paramId, const juce::String& labelText)
{
    auto row = std::make_unique<ToggleRow>();
    row->button.setButtonText (labelText);
    content.addAndMakeVisible (row->button);
    row->attachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, paramId, row->button);
    toggles.push_back (std::move (row));
    return *toggles.back();
}

EmoBoyEditor::EmoBoyEditor (EmoBoyProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);

    buildMainSection();

#if EMOBOY_NERD_FEATURES
    // Mod1/Mod2/AM/PT + the routing matrix - see PluginProcessor.h/.cpp and
    // Parameters.h/.cpp for the same EMOBOY_NERD_FEATURES gate, and
    // HANDOFF.md for why: parked for a later "EmoBoy Nerd" variant,
    // revisit at the 1.0 release. Nothing deleted - flip the CMake option
    // and these two calls come back.
    buildModSection();
    buildRoutingSection();
#endif

    content.setSize (1, 1);
    for (auto& g : groups)
        content.setSize (juce::jmax (content.getWidth(), g->getRight() + kMargin),
                          juce::jmax (content.getHeight(), g->getBottom() + kMargin));

    setResizable (true, true);
    setSize (content.getWidth(), content.getHeight());
}

void EmoBoyEditor::buildMainSection()
{
    auto group = std::make_unique<juce::GroupComponent> ("main", "EmoBoy (working name - not final)");
    content.addAndMakeVisible (*group);
    groups.push_back (std::move (group));

    int x = kMargin * 2;
    const int y = 40;

    auto placeKnob = [&] (const juce::String& id, const juce::String& label)
    {
        auto& k = addKnob (id, label);
        k.label.setBounds (x, y, kKnobSize, 18);
        k.slider.setBounds (x, y + 18, kKnobSize, kKnobSize);
        x += kKnobSize + kMargin;
    };

    placeKnob (Param::pitch, "Pitch");
    placeKnob (Param::formant, "Formant");

    auto& link = addToggle (Param::link, "Link");
    link.button.setBounds (x, y + 30, 70, 24);
    x += 80;

    auto& mode = addCombo (Param::mode, "Mode");
    mode.label.setBounds (x, y, 110, 18);
    mode.box.setBounds (x, y + 20, 110, 24);
    x += 120;

    auto& robotNote = addCombo (Param::robotNote, "Robot Note");
    robotNote.label.setBounds (x, y, 100, 18);
    robotNote.box.setBounds (x, y + 20, 100, 24);
    x += 110;

    placeKnob (Param::drive, "Drive");
    placeKnob (Param::mix, "Mix");

    groups.back()->setBounds (kMargin, 10, x + kMargin, kRowHeight + 20);
}

#if EMOBOY_NERD_FEATURES
void EmoBoyEditor::buildModSection()
{
    const int top = kRowHeight + 40;

    auto buildSourceGroup = [&] (const juce::String& title, int columnIndex,
                                  const juce::String& id1, const juce::String& l1,
                                  const juce::String& id2, const juce::String& l2,
                                  const juce::String& id3, const juce::String& l3)
    {
        const int groupWidth = (kKnobSize + kMargin) * 3 + kMargin * 2;
        const int gx = kMargin + columnIndex * (groupWidth + kMargin);

        auto group = std::make_unique<juce::GroupComponent> (title, title);
        group->setBounds (gx, top, groupWidth, kRowHeight + 20);
        content.addAndMakeVisible (*group);
        groups.push_back (std::move (group));

        int x = gx + kMargin;
        const int y = top + 30;
        auto placeKnob = [&] (const juce::String& id, const juce::String& label)
        {
            auto& k = addKnob (id, label);
            k.label.setBounds (x, y, kKnobSize, 18);
            k.slider.setBounds (x, y + 18, kKnobSize, kKnobSize);
            x += kKnobSize + kMargin;
        };
        placeKnob (id1, l1);
        placeKnob (id2, l2);
        placeKnob (id3, l3);
    };

    buildSourceGroup ("Mod 1 (LFO)", 0, Param::mod1Rate, "Rate", Param::mod1Phase, "Phase", Param::mod1Level, "Level");
    buildSourceGroup ("Mod 2 (LFO)", 1, Param::mod2Rate, "Rate", Param::mod2Phase, "Phase", Param::mod2Level, "Level");

    const int top2 = top + kRowHeight + 40;
    auto buildSourceGroup2Impl = [&] (const juce::String& title, int columnIndex,
                                       const juce::String& id1, const juce::String& l1,
                                       const juce::String& id2, const juce::String& l2,
                                       const juce::String& id3, const juce::String& l3)
    {
        const int groupWidth = (kKnobSize + kMargin) * 3 + kMargin * 2;
        const int gx = kMargin + columnIndex * (groupWidth + kMargin);

        auto group = std::make_unique<juce::GroupComponent> (title, title);
        group->setBounds (gx, top2, groupWidth, kRowHeight + 20);
        content.addAndMakeVisible (*group);
        groups.push_back (std::move (group));

        int x = gx + kMargin;
        const int y = top2 + 30;
        auto placeKnob = [&] (const juce::String& id, const juce::String& label)
        {
            auto& k = addKnob (id, label);
            k.label.setBounds (x, y, kKnobSize, 18);
            k.slider.setBounds (x, y + 18, kKnobSize, kKnobSize);
            x += kKnobSize + kMargin;
        };
        placeKnob (id1, l1);
        placeKnob (id2, l2);
        placeKnob (id3, l3);
    };

    buildSourceGroup2Impl ("AM (Envelope)", 0, Param::amAttack, "Attack", Param::amRelease, "Release", Param::amLevel, "Level");
    buildSourceGroup2Impl ("PT (Pitch Tracker)", 1, Param::ptSmooth, "Smooth", Param::ptOffset, "Offset", Param::ptLevel, "Level");
}

void EmoBoyEditor::buildRoutingSection()
{
    const int top = kRowHeight + 40 + (kRowHeight + 40) * 2 + 10;

    auto group = std::make_unique<juce::GroupComponent> ("routing", "Modulation Routing (source -> target, depth %)");
    const int rowH = 30;
    const int labelColW = 90;
    const int cellW = 150;
    const int groupWidth = labelColW + cellW * ModMatrix::numTargets + kMargin;
    const int groupHeight = rowH * (ModMatrix::numSources + 1) + 20;
    group->setBounds (kMargin, top, groupWidth, groupHeight);
    content.addAndMakeVisible (*group);
    groups.push_back (std::move (group));

    static const char* sourceLabels[ModMatrix::numSources] = { "Mod 1", "Mod 2", "AM", "PT" };
    static const char* targetLabels[ModMatrix::numTargets] = { "Pitch", "Formant", "Mix", "Drive" };

    const int headerY = top + 24;
    for (int t = 0; t < ModMatrix::numTargets; ++t)
    {
        auto lbl = std::make_unique<juce::Label> (juce::String(), juce::String (targetLabels[t]));
        lbl->setJustificationType (juce::Justification::centred);
        lbl->setBounds (kMargin + labelColW + t * cellW, headerY, cellW, 18);
        content.addAndMakeVisible (*lbl);
        freeLabels.push_back (std::move (lbl));
    }

    for (int s = 0; s < ModMatrix::numSources; ++s)
    {
        const int rowY = headerY + 22 + s * rowH;

        auto lbl = std::make_unique<juce::Label> (juce::String(), juce::String (sourceLabels[s]));
        lbl->setJustificationType (juce::Justification::centredLeft);
        lbl->setBounds (kMargin, rowY + 4, labelColW, rowH - 4);
        content.addAndMakeVisible (*lbl);
        freeLabels.push_back (std::move (lbl));

        for (int t = 0; t < ModMatrix::numTargets; ++t)
        {
            auto cell = std::make_unique<RouteCell>();
            const int cx = kMargin + labelColW + t * cellW;

            cell->on.setBounds (cx, rowY + 2, 26, rowH - 4);
            cell->depth.setBounds (cx + 30, rowY + 2, cellW - 34, rowH - 4);
            cell->depth.setRange (-100.0, 100.0, 0.1);

            content.addAndMakeVisible (cell->on);
            content.addAndMakeVisible (cell->depth);

            auto source = (Param::Source) s;
            auto target = (Param::Target) t;
            cell->onAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, Param::routeOnId (source, target), cell->on);
            cell->depthAttachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, Param::routeDepthId (source, target), cell->depth);

            routeCells[(size_t) s][(size_t) t] = std::move (cell);
        }
    }

    content.setSize (groupWidth + kMargin * 2, top + groupHeight + kMargin);
}
#endif // EMOBOY_NERD_FEATURES

void EmoBoyEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void EmoBoyEditor::resized()
{
    viewport.setBounds (getLocalBounds());
    if (content.getWidth() < viewport.getWidth())
        content.setSize (viewport.getWidth(), content.getHeight());
}
