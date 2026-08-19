#pragma once

// ---------------------------------------------------------------------------
// All parameter ID strings live here, same convention as the "Not Sure"
// project: these strings get written into every saved session/preset once
// this ships, so once real users exist they become permanent. This is a
// demo, not shipping yet, so that constraint does not bite today - but the
// IDs are written sensibly anyway so nothing has to be renamed later.
// ---------------------------------------------------------------------------

#include <juce_audio_processors/juce_audio_processors.h>

namespace Param
{
    // Main panel
    static const juce::String pitch        = "pitch";       // semitones, -12..12
    static const juce::String formant      = "formant";     // semitones, -12..12
    static const juce::String link         = "link";        // bool
    static const juce::String mode         = "mode";        // choice: Transpose/Quantize/Robot
    static const juce::String robotNote    = "robotNote";   // choice C2..B3 (24 notes)
    static const juce::String drive        = "drive";       // 0..100 %
    static const juce::String mix          = "mix";         // 0..100 %

    // Modulation sources
    static const juce::String mod1Rate     = "mod1Rate";    // Hz
    static const juce::String mod1Phase    = "mod1Phase";   // degrees
    static const juce::String mod1Level    = "mod1Level";   // 0..100 %

    static const juce::String mod2Rate     = "mod2Rate";
    static const juce::String mod2Phase    = "mod2Phase";
    static const juce::String mod2Level    = "mod2Level";

    static const juce::String amAttack     = "amAttack";    // ms
    static const juce::String amRelease    = "amRelease";   // ms
    static const juce::String amLevel      = "amLevel";     // 0..100 %

    static const juce::String ptSmooth     = "ptSmooth";    // 0..100 %
    static const juce::String ptOffset     = "ptOffset";    // semitones, reference note -24..24
    static const juce::String ptLevel      = "ptLevel";     // 0..100 %

    // Routing matrix: 4 sources x 4 targets, each an enable bool + a bipolar
    // depth. Built programmatically - see makeRoutingId() below - rather than
    // spelling out 32 names, but the resulting ID strings are still stable
    // and human-readable (e.g. "route_mod1_pitch_on", "route_am_drive_depth").
    enum class Source { Mod1, Mod2, AM, PT, Count };
    enum class Target { Pitch, Formant, Mix, Drive, Count };

    inline const char* sourceName (Source s)
    {
        switch (s)
        {
            case Source::Mod1: return "mod1";
            case Source::Mod2: return "mod2";
            case Source::AM:   return "am";
            case Source::PT:   return "pt";
            default:           return "?";
        }
    }

    inline const char* targetName (Target t)
    {
        switch (t)
        {
            case Target::Pitch:   return "pitch";
            case Target::Formant: return "formant";
            case Target::Mix:     return "mix";
            case Target::Drive:   return "drive";
            default:               return "?";
        }
    }

    inline juce::String routeOnId (Source s, Target t)
    {
        return juce::String ("route_") + sourceName (s) + "_" + targetName (t) + "_on";
    }

    inline juce::String routeDepthId (Source s, Target t)
    {
        return juce::String ("route_") + sourceName (s) + "_" + targetName (t) + "_depth";
    }

    enum class Mode { Transpose = 0, Quantize = 1, Robot = 2 };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
