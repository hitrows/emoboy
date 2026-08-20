#include "Parameters.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // ---- main panel ------------------------------------------------------
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::pitch, 1 }, "Pitch",
        NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::formant, 1 }, "Formant",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { Param::link, 1 }, "Link", false));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { Param::mode, 1 }, "Mode",
        StringArray { "Transpose", "Quantize", "Robot" }, 0));

    {
        StringArray notes;
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int octave = 2; octave <= 3; ++octave)
            for (auto* n : names)
                notes.add (String (n) + String (octave));
        // Default index 12 = C3, the knob's 12-o'clock reference note
        // (2026-08-20 rework) - a fresh instance should rest with the
        // pointer straight up, not rotated all the way to one side.
        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { Param::robotNote, 1 }, "Robot Note", notes, 12));
    }

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { Param::bypass, 1 }, "Bypass", false));

#if EMOBOY_NERD_FEATURES
    // ---- modulation sources ------------------------------------------------
    auto addLFO = [&] (const juce::String& rateId, const juce::String& phaseId, const juce::String& levelId, const juce::String& label)
    {
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { rateId, 1 }, label + " Rate",
            NormalisableRange<float> (0.06f, 30.0f, 0.0f, 0.3f), 1.0f,
            AudioParameterFloatAttributes().withLabel ("Hz")));
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { phaseId, 1 }, label + " Phase",
            NormalisableRange<float> (0.0f, 360.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("deg")));
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { levelId, 1 }, label + " Level",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("%")));
    };
    addLFO (Param::mod1Rate, Param::mod1Phase, Param::mod1Level, "Mod1");
    addLFO (Param::mod2Rate, Param::mod2Phase, Param::mod2Level, "Mod2");

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::amAttack, 1 }, "AM Attack",
        NormalisableRange<float> (1.0f, 500.0f, 0.0f, 0.4f), 10.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::amRelease, 1 }, "AM Release",
        NormalisableRange<float> (5.0f, 2000.0f, 0.0f, 0.4f), 150.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::amLevel, 1 }, "AM Level",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::ptSmooth, 1 }, "PT Smooth",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::ptOffset, 1 }, "PT Offset",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("st")));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { Param::ptLevel, 1 }, "PT Level",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // ---- routing matrix ---------------------------------------------------
    for (int s = 0; s < (int) Param::Source::Count; ++s)
    {
        for (int t = 0; t < (int) Param::Target::Count; ++t)
        {
            auto source = (Param::Source) s;
            auto target = (Param::Target) t;
            const juce::String onId = Param::routeOnId (source, target);
            const juce::String depthId = Param::routeDepthId (source, target);
            const juce::String label = juce::String (Param::sourceName (source)) + " -> " + Param::targetName (target);

            params.push_back (std::make_unique<AudioParameterBool> (
                ParameterID { onId, 1 }, label + " On", false));
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { depthId, 1 }, label + " Depth",
                NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                AudioParameterFloatAttributes().withLabel ("%")));
        }
    }
#endif // EMOBOY_NERD_FEATURES

    return { params.begin(), params.end() };
}
