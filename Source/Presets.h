#pragma once

#include "Parameters.h"
#include <array>

// ---------------------------------------------------------------------------
// Single source of truth for EmoBoy's factory presets (2026-08-21) - shared
// between EmoBoyProcessor (exposes all 9 as standard host-recognised
// programs via getNumPrograms()/getProgramName()/setCurrentProgram(), so
// they show up in Logic's own preset selector) and EmoBoyEditor (the first
// 4 also each get their own dedicated top-row footswitch - there's only
// room for 4 buttons on the panel, so the other 5 are host-menu only).
//
// Index order here IS the host program index order - the top-row buttons
// assume button i == program i for i in 0..3, so don't reorder without
// checking PluginEditor.cpp's applyPreset().
// ---------------------------------------------------------------------------
struct EmoBoyPreset
{
    const char* name;
    float pitch, formant, drive, mix;
    Param::Mode mode;
    int robotNoteIndex;
};

namespace Presets
{
    // 0-3 (Cry/Void/Lost/Robot): the original 4, named after the graffiti
    // stars already painted on the panel art (VOID/CRY/LOST) plus the
    // robot icon. Unchanged from the earlier top-row-only version.
    //
    // 4-8 (Titan/Wraith/Warp/Drone/Shatter): brainstormed for non-vocal
    // uses of the plugin - a first pass by ear-adjacent reasoning about
    // what each mechanism does, not yet validated against a real source
    // by listening (see HANDOFF.md - "measure by listening" applies here
    // same as everywhere else; these are a starting point, not final).
    //   Titan   - bass: formant pulled down at unchanged pitch, for a
    //             bigger low end without pitching the note itself down.
    //   Wraith  - foley/creature texture: pitch and formant both down,
    //             more drive, less than fully wet so it blends with a
    //             source rather than replacing it outright.
    //   Warp    - sample/loop pitch-up without the "chipmunk" - formant
    //             pulled down to compensate. Transpose mode (fixed
    //             shift, no pitch tracking) since loops aren't monophonic
    //             vocal material.
    //   Drone   - Robot mode as a plain pitch-lock utility for a
    //             melodic instrument, not a vocal "robot" character -
    //             kept closer to neutral than the Robot preset above.
    //   Shatter - Quantize mode pushed onto percussive/non-vocal
    //             material on purpose, for the glitchy mis-tracking
    //             artefacts rather than correct pitch-snapping.
    static const std::array<EmoBoyPreset, 9> table { {
        { "Cry",     -3.0f,  -3.0f, 15.0f, 100.0f, Param::Mode::Transpose, 12 },
        { "Void",   -12.0f,  -9.0f, 65.0f, 100.0f, Param::Mode::Transpose, 12 },
        { "Lost",     5.0f,   6.0f, 10.0f,  80.0f, Param::Mode::Transpose, 12 },
        { "Robot",    0.0f,   2.0f, 35.0f, 100.0f, Param::Mode::Robot,     12 },
        { "Titan",    0.0f, -10.0f, 20.0f, 100.0f, Param::Mode::Transpose, 12 },
        { "Wraith",  -7.0f,  -7.0f, 40.0f,  70.0f, Param::Mode::Transpose, 12 },
        { "Warp",     7.0f,  -7.0f, 10.0f, 100.0f, Param::Mode::Transpose, 12 },
        { "Drone",    0.0f,   0.0f, 25.0f, 100.0f, Param::Mode::Robot,     12 },
        { "Shatter",  0.0f,   0.0f, 50.0f, 100.0f, Param::Mode::Quantize,  12 },
    } };
}
